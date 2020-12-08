#include "types.h"
#include "stat.h"
#include "user.h"

int str_to_int(char *s1)
{
    //strlen there in string.c of xv6
    int len = strlen(s1);
    int now = 0;
    for (int i = 0; i < len; i++)
    {
        now *= 10;
        int add=s1[i]-'0';
        if(add<0 || add>9)
        {
            printf(1,"Error: Args are only allowed to have numeric characters\n");
            exit();
        }
        now += (add);
        
    }
    return now;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf(2, "Error in usage of setPriority: Input exactly 2 arguments\n");
        exit();
    }

    int proc_id, new_priority;
    new_priority=str_to_int(argv[1]);
    proc_id=str_to_int(argv[2]);

    printf(1,"Args being sent are new_priority : %d  and pid as %d\n",new_priority,proc_id);
    int ret_val=set_priority(new_priority,proc_id);
    if(ret_val==-1)
    {
        printf(2,"ERROR: Arguments could not be parsed\n");
    }
    else if(ret_val==-2)
    {
        printf(2,"ERROR: Process either does not exist or has already exited\n");
        
    }
    else if(ret_val==-3)
    {
        printf(2,"ERROR: Proc pid not found\n");
    }
    else if(ret_val==-4)
    {
        printf(2,"ERROR: new_priority should be between 0 and 100\n");
    }
    else
    {
        printf(1,"New value set. Old priority : %d\n",ret_val);
    }   

    exit();
}
