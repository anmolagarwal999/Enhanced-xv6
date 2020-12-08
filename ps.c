#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        printf(2, "Error in usage of ps: No arguments are accepted by this command\n");
        exit();
    }
    ps();
    exit();
}
