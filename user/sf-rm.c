#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/debug.h"
#include "kernel/fcntl.h"

// Demonstrate read-write for small files and normal files
int main(int argc, char** argv) {

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
