#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/debug.h"
#include "kernel/fcntl.h"

// Demonstrate read-write for small files and normal files
int main() {

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


    char buf[34];
    read(fd, buf, 34);

    struct stat st;
    fstat(fd, &st);
    if (st.type == T_SMALLFILE)
      printf("type: smallfile | ");
    else
      printf("type: normalfile | ");
    printf("%s: %s\n", name, buf);

    close(fd);
  }

  exit(0);
}
