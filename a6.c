
#include "types.h"
#include "user.h"

int number_of_processes = 8;

int main(int argc, char *argv[])
{
#ifndef MLFQ
    int j;
    for (j = 0; j < number_of_processes; j++)
    {
        int pid = fork();
        if (pid < 0)
        {
            printf(1, "Fork failed\n");
            continue;
        }
        if (pid == 0)
        {
            volatile int i;
            for (volatile int k = 0; k < number_of_processes; k++)
            {

                // sleep(200); //io time
                for (i = 0; i < 100000000; i++)
                {
                    ; //cpu time
                }
            }
            //   printf(1, "Process: %d Finished\n", j);
            // ps();
            exit();
        }
        else
        {
#ifdef PBS
            ;
//if j odd, then more is subtracted -> lesser priority number -> ODD are destined to be scheduled first
#endif
            set_priority(100 - (20 + (j % 2) * 5), pid);
        }
    }
    for (j = 0; j < number_of_processes + 5; j++)
    {
        wait();
    }
    exit();
#endif

#ifdef MLFQ
    int j;
    for (j = 0; j < number_of_processes; j++)
    {
        int pid = fork();
        if (pid < 0)
        {
            printf(1, "Fork failed\n");
            continue;
        }
        if (pid == 0)
        {
            volatile int i;
            for (volatile int k = 0; k < number_of_processes; k++)
            {
                if (j % 2 == 0)
                {
                    sleep(200); //io time
                    for (i = 0; i < 8000000; i++)
                    {
                        ; //cpu time
                    }
                }
                else
                {
                    for (i = 0; i < 800000000; i++)
                    {
                        ; //cpu time
                    }
                }
            }
            //   printf(1, "Process: %d Finished\n", j);
            ps();
            exit();
        }
        else
        {
            ;
            // set_priority(100 - (20 + j), pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
        }
    }
    for (j = 0; j < number_of_processes + 5; j++)
    {
        wait();
    }
    exit();
#endif
}