#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/debug.h"

int main(void)
{
    debug("This is the %dst debug statement!\n", 1);
    printf("Test program\n");
    exit(0);
}