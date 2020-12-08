#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf(2, "Error in usage of time: Input atleast some command for child to execute\n");
        exit();
    }

    int pid = fork();

    if (pid < 0)
    {
        printf(2, "Error: fork() failed\n");
        exit();
    }
    else if (pid == 0)
    {
        exec(argv[1], argv + 1);
        printf(2, "ERROR: exec() failed\n");
        exit();
    }
    else
    {
        int *wtime = (int *)malloc(sizeof(int));
        int *rtime = (int *)malloc(sizeof(int));
        waitx(wtime, rtime);

        printf(1, "rtime = %d\ntotal_non_sleeping_wtime = %d\n", *rtime, *wtime);
    }

    exit();
}
