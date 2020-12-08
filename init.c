// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

//Regular bold text
#define BBLK "\e[1;30m"
#define BRED "\e[1;31m"
#define BGRN "\e[1;32m"
#define BYEL "\e[1;33m"
#define BBLU "\e[1;34m"
#define BMAG "\e[1;35m"
#define BCYN "\e[1;36m"
#define BWHT "\e[1;37m"
#define ANSI_RESET "\e[0m"

/*
docs : this init.c is initiated by the kernel during BOOT
*/
char *argv[] = {"sh", 0};

int main(void)
{
  int pid, wpid;
 // printf(1, BRED "Entered main() of init.c state 1\n" ANSI_RESET);

  if (open("console", O_RDWR) < 0)
  {
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0); // stdout
  dup(0); // stderr
  //printf(1, BGRN "Entered main() of init.c state 2\n" ANSI_RESET);

  /* docs: https://youtu.be/a2LQbb82EZM?t=1176
  Infinite 'for' loop,its only job is to fork the SHELL process
  and it waits till that spawn is complete .
  */
 /*docs: vukturu
 Next, it forks a 
child, and exec’s the shell executable in the child. While the child runs the shell (and forks
various other processes), the parent waits and reaps zombies
 */
  for (;;)
  {
  //  printf(1, BGRN "Entered main() of init.c state 3\n" ANSI_RESET);
    printf(1, BYEL "init: starting sh\n" ANSI_RESET);

    pid = fork();
    if (pid < 0)
    {
      printf(1, "init: fork failed\n");
      exit();
    }
    if (pid == 0)
    {
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }

    //printf(1, BMAG "Before waiting\n" ANSI_RESET);

    /* docs: keep waiting as long as the forked child does not complete*/
    /* docs: wait() in while loop condition is itself kind of time consuming,
    Also 'zombie' is printed only when the process whose wait() was over wasn't the child for whom we were waiting
    */
    while ((wpid = wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");

    //docs
    printf(1, BGRN "end of 'for; loop iteration reached\n" ANSI_RESET);
  }
}
