#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define NUM_CALLS 1

/* Demonstrate the logging behavior of FS syscalls */

int main(int argc, char ** argv)
{
        
    int fd = open("/testfile.out", O_CREATE | O_RDWR);

    if (fd == -1) {
        printf("Failed to open file!");
        exit(-1);
    }

    int t1 = uptime();

    for (int i = 0; i < NUM_CALLS; i++)
    {
        write(fd, "This is a line\n", 16);
    }

    int t2 = uptime();

    printf("Time to write %d bytes = %d\n", NUM_CALLS * 16, t2 - t1);

    close(fd);
    exit(0);
}