#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define NUM_CALLS 1000

/* Demonstrate the logging behavior of FS syscalls */

int main(int argc, char ** argv)
{
    uint8 sequentialTest;

    /* Determine if the calls are to be made sequentially or concurrently */
    if (argc > 1 && strcmp(argv[1], "-s") == 0)
        sequentialTest = 1;
    else 
        sequentialTest = 0;
        
    int fd = open("/testfile.out", O_CREATE | O_RDWR);
    int pid;

    if (fd == -1) {
        printf("Failed to open file!");
        exit(-1);
    }

    int t1 = uptime();
    /* Concurrent writes */

    if (!sequentialTest){
        for (int i = 0; i < NUM_CALLS; i++){
            if ((pid = fork()) == 0){
                // Child is here
                printf("Child %d writing...\n", pid);
                write(fd, "Written by Child!\n", 19);
                close(fd);
                exit(0);
            }

            else if (pid == -1) {
                printf("Fork failed!\n");
                exit(-1);
            }
        }

        for (int i = 0; i < NUM_CALLS; i++){
            pid = wait(0);
            printf("Child %d exited!\n", pid);
        }
    }

    /* Sequential writes */

    else {
        for (int i = 0; i < NUM_CALLS; i++){
            write(fd, "This is a line\n", 16);
        }   
    }

    int t2 = uptime();

    printf("Time to perform writes = %d\n", t2 - t1);

    close(fd);
    exit(0);
}