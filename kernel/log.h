#ifndef LOG_H
#define LOG_H

struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  struct spinlock commitLock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // In commit, don't allow blocks to be copied to disk
  int copying;     // Don't allow syscalls to execute when the log is being copied to disk
  int copyAttempted;    // Dont try to initiate a copy again when a previous thread has already attempted to copy 
  int dev;
  struct logheader lh;
};

void copy_and_initiate_commit();
void commit_loop();

#endif