#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/debug.h"
#include "kernel/fcntl.h"

int main(void)
{
  debug("This is the %dst debug statement!\n", 1);
  int fd = open("test.txt", O_CREATE | O_RDWR | O_SMALLFILE);

  if (fd < 0) {
    printf("Failed to create file\n");
    exit(1);
  }

  write(fd, "aaa\n", 4);
  write(fd, "bbb", 3);
  write(fd, "bbb", 3);
  write(fd, "bbb", 3);
  write(fd, "bbb", 3);
  write(fd, "bbb", 3);
  write(fd, "bbb", 3);
  write(fd, "bbb", 3);
  write(fd, "bbb", 3);
  write(fd, "bbb", 3);
  write(fd, "bbb\n", 4);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);
  write(fd, "loda bsdk\n", 10);

  close(fd);

  printf("Test program\n");
  exit(0);
}
