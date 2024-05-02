#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "param.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "log.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

struct log log;

static void recover_from_log(void);
static void clear_disk_log_header();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    if(recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){

    if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // this op might exhaust log space; wait for blocks to be copied to disk log

      release(&log.lock);
      acquire(&log.commitLock);

      if (!log.copying){
        // Attempty a copy and a commit
        log.copying = 1;
      
        while (1){
          if (log.committing){
            // Check if a commit is in progress
            debug("[BEGIN OP] : Attempting copy while commit in progress. Sleeping ...\n");
            sleep(&log, &log.commitLock);
          }

          else {
            // Call this function while holding the lock, it returns after lock is released
            copy_and_initiate_commit();
            debug("[BEGIN OP] : Returning after initiating commit. Retrying transaction...\n");
            acquire(&log.lock);
            break;
          }
        }
      }

      else{
        // Don't allow another thread to initiate a copy if one thread has tried already
        debug("[BEGIN OP] : Another thread has attempted to copy. Attempting to restart transaction...\n");
        release(&log.commitLock);
        acquire(&log.lock);
        sleep(&log, &log.lock);
        continue;
      }
    } 
    
    else {
      debug("[BEGIN OP] : %d blocks in the log. Starting transaction...\n", log.lh.n);
      log.outstanding += 1;
      release(&log.lock);
      break;
    }

  }
}

// called at the end of each FS system call.
// copies the blocks to the on disk log if the log is about to fill up
void
end_op(void)
{
  acquire(&log.lock);
    log.outstanding -= 1;
  release(&log.lock);  
  
  debug("[END OP] Ending transaction...\n");
  return;
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    brelse(from);
    brelse(to);
  }
}

/* Call this function while holding commitLock. It releases the lock before returning */
void
copy_and_initiate_commit()
{
  if (log.committing == 0 && log.lh.n > 0){
    
    // Release the lock while performing I/O
    release(&log.commitLock);

    // Copy
    write_log();
    write_head();

    // Reset outstanding transactions to 0 and initiate the commit
    acquire(&log.lock);
      log.lh.n = 0; 
    release(&log.lock);

    acquire(&log.commitLock);
      log.committing = 1;
      log.copyAttempted = 0;
      log.copying = 0;
    release(&log.commitLock);

      // Some process might also be sleeping in end_op waiting for copy to complete
      wakeup(&log);       
      debug("[COPY] Copy complete! Waking up commit worker process...\n");
    return;
  }

  // No blocks to be copied. Reverse copying states
  else if (log.lh.n == 0){
      debug("Copy initiated but no blocks in log!\n");
      log.copying = 0;
      release(&log.commitLock);
      return;
  }

  else{
    debug("WARNING: copy attempted when commit in progress\n");
    release(&log.commitLock);
    return;
  }
    
  return;

}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorption
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {  // Add new block to log?
    bpin(b);
    log.lh.n++;
  }
  release(&log.lock);
}

void
commit_loop()
{
  debug("Acquiring commit lock\n");
  acquire(&log.commitLock);
  while (1)
  {
    if (log.copying){
      debug("Attempting to commit while copying. Sleeping ...\n");
      debug("Sleeping on commit lock\n");
      sleep(&log, &log.commitLock);
    }

    else if (log.committing){
      release(&log.commitLock);
      install_trans(0);
      clear_disk_log_header();
      log.committing = 0;
      acquire(&log.commitLock);
      wakeup(&log);
      debug("Finished commit!\n");
    }

    else {
      debug("Commit worker has nothing to do. Sleeping...\n");
      debug("Sleeping on commit lock\n");
      sleep(&log, &log.commitLock);
    }
  }
}

void 
clear_disk_log_header()
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  
  // Mark the number of outstanding blocks to be 0
  // This erases the transaction from the disk log
  hb->n = 0;

  bwrite(buf);
  brelse(buf);

}