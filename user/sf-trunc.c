#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/debug.h"
#include "kernel/fcntl.h"

void print_details(int fd, char* name) {
  char buf[34];
  read(fd, buf, 34);

  struct stat st;
  fstat(fd, &st);
  if (st.type == T_SMALLFILE)
    printf("type: smallfile | size: %d | ", st.size);
  else
    printf("type: normalfile | size: %d | ", st.size);
  printf("%s: %s\n", name, buf);
}

// Demonstrate read-write for small files and normal files
int main(int argc, char** argv) {
  int sz = 0;
  if (strcmp(argv[1], "-i") == 0) {
    sz = 100;
  }
  else if (strcmp(argv[1], "-d") == 0) {
    sz = 5;
  }
  else {
    printf("Usage: smalltest [-i] || [-d]\n");
    exit(1);
  }

  for (int i = 0; i < 15; i++) {
    char name[3];
    name[0] = 't';
    name[1] = '0' + i;
    name[2] = '\0';

    int fd = open(name, O_RDWR);


    if (fd < 0) {
      printf("Failed to open file\n");
      exit(1);
    }

    print_details(fd, name);
    ftruncate(fd, sz);
    print_details(fd, name);

    close(fd);
  }

  exit(0);
}
