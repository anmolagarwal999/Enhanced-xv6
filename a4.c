
#include "types.h"
#include "user.h"

int number_of_processes = 6;

int main(int argc, char *argv[])
{
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
                    //io
                    sleep(200 + 10 * j); //io time
                }
                else
                {
                    //cpu
                    if (j % 3 != 0)
                    {
                        for (i = 0; i < 100000000; i++)
                        {
                            ; //cpu time
                        }
                    }
                    else
                    {
                        for (i = 0; i < 30000000; i++)
                        {
                            ; //cpu time
                            sleep(5);
                        }
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
#ifdef PBS
            set_priority(100 - (20 + j), pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
#endif
        }
    }
    for (j = 0; j < number_of_processes + 5; j++)
    {
        wait();
    }
    exit();
}