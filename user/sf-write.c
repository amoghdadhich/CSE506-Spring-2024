#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/debug.h"
#include "kernel/fcntl.h"

// Demonstrate read-write for small files and normal files
int main(int argc, char** argv) {

  int file_type = 0;

  /* Determine if the test is for small files or normal files */
  if (strcmp(argv[1], "-s") == 0) {
    file_type = O_SMALLFILE;
    printf("Small file test\n");
  }
  else if (strcmp(argv[1], "-n") == 0) {
    file_type = O_RDWR;
    printf("Normal file test\n");
  }
  else {
    printf("Usage: smalltest [-s] || [-n]\n");
    exit(1);
  }

  for (int i = 0; i < 15; i++) {
    char name[3];
    name[0] = 't';
    name[1] = '0' + i;
    name[2] = '\0';

    int fd;

    if (file_type == O_SMALLFILE) {
      fd = open(name, O_CREATE | O_RDWR | O_SMALLFILE);
    }
    else {
      fd = open(name, O_CREATE | O_RDWR);
    }

    if (fd < 0) {
      printf("Failed to create file\n");
      exit(1);
    }

    write(fd, "Prof. Dongyoon Lee is very cool.", 33);

    close(fd);
  }

  exit(0);
}
