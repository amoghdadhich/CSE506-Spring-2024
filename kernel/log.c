#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

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

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // In commit, don't allow blocks to be copied to disk
  int copying;     // Don't allow syscalls to execute when the log is being copied to disk
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static int copy_and_initiate_commit();

void
initlog(int dev, struct superblock* sb)
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
    struct buf* lbuf = bread(log.dev, log.start + tail + 1); // read log block
    struct buf* dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    if (recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf* buf = bread(log.dev, log.start);
  struct logheader* lh = (struct logheader*)(buf->data);
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
  struct buf* buf = bread(log.dev, log.start);
  struct logheader* hb = (struct logheader*)(buf->data);
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
  while (1) {
    if (log.copying) {
      sleep(&log, &log.lock);
    }
    else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGSIZE) {
      // this op might exhaust log space; wait for blocks to be copied to disk log
      sleep(&log, &log.lock);
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
  int do_copy = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if (log.copying)
    panic("log.copying");
  if (log.lh.n > LOGSIZE - MAXOPBLOCKS) {
    // Initiate a copy operation if the next transaction could fill up the log
    debug("[END OP] Number of blocks in log = %d. Initiating copy!\n");
    do_copy = 1;
    log.copying = 1;
  }
  else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    debug("[END OP] : %d blocks in the log. Ending transaction...\n", log.lh.n);
    wakeup(&log);
  }
  release(&log.lock);

  if (do_copy) {
    acquire(&log.lock);

    while (1) {
      if (log.committing) {
        // Check if a commit is in progress
        debug("[END OP] : Attempting copy while commit in progress. Sleeping ...\n");
        sleep(&log, &log.lock);
      }

      else {
        release(&log.lock);

        // Copy without holding lock as IO might cause process to sleep
        if (copy_and_initiate_commit() < 0)
          panic("Commit failed!\n");

        else
          break;
      }
    }
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf* to = bread(log.dev, log.start + tail + 1); // log block
    struct buf* from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    brelse(from);
    brelse(to);
  }
}

// static void
// commit()
// {
//   if (log.lh.n > 0) {
//     write_log();     // Write modified blocks from cache to log
//     write_head();    // Write header to disk -- the real commit
//     install_trans(0); // Now install writes to home locations
//     log.lh.n = 0;
//     write_head();    // Erase the transaction from the log
//   }
// }

static int
copy_and_initiate_commit()
{
  if (log.committing == 0 && log.lh.n > 0) {
    // Copy
    write_log();
    write_head();

    // Reset outstanding transactions to 0 and initiate the commit
    acquire(&log.lock);
    log.lh.n = 0;
    log.committing = 1;

    // Since copying is completed any sleeping syscalls can be woken up
    debug("[COPY] Copy complete!\n");
    log.copying = 0;
    wakeup(&log);
    release(&log.lock);

    // Fork a process which installs the transactions to disk
    int pid;
    if ((pid = fork()) == 0) {
      // Child process performs commit actions
      debug("[COPY] Child process commmitting log...\n");
      install_trans(0);
      // TO DO: Clear the header value on disk 
      // TO DO: Write a policy to commit intermittentlys

      // Commit completed
      acquire(&log.lock);
      log.committing = 0;
      wakeup(&log);
      debug("[COPY] Log committed! Child exiting...\n");
      release(&log.lock);

      exit(0);
    }

    else if (pid == -1) {
      printf("Fork failed in log commit!\n");
      return -1;
    }

    else {
      // Parent process returns
      printf("Parent exiting after initiating commit\n");
      return 0;
    }
  }

  else
    printf("Log committing while copying!");

  return -1;
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
log_write(struct buf* b)
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

