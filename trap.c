#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

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

int runtime_limit[5] = {1, 2, 4, 8, 16};

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

//#######################################################################
/* docs:
Tvinit (3367), called from main, sets up the 256 entries in the table idt.
*/
void tvinit(void)
{
  int i;

  for (i = 0; i < 256; i++)
  {
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  }
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}
//#######################################################################

void idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41

void trap(struct trapframe *tf)
{

  /* docs:
  Trap (3401) looks at the hardware trap number tf->trapno to
  decide why it has been called and what needs to be done
  */
  //--------------------------------------------------------------
  if (tf->trapno == T_SYSCALL)
  {
    if (myproc()->killed)
    {
      exit();
    }
    myproc()->tf = tf;
    /*For system calls, trap invokes syscall() function (3701). */
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }
  //------------------------
  // this part is entered only if trap was not due to a sys call

  switch (tf->trapno)
  {
  case T_IRQ0 + IRQ_TIMER:
    /*docs:
  Trap for a timer interrupt does just two things: increment the ticks variable
(3417), and call wakeup. 
  */
    // cprintf(BRED "Timer interrupt %d %d\n" ANSI_RESET, cpuid(), ticks);
    if (cpuid() == 0)
    {
      acquire(&tickslock);
      ticks++;
      update_process_stats();
      /* docs:
      xv6 provides sleep (line 2803) and wakeup (line 2864) functions, that are equivalent to the
wait and signal functions of a conditional variable.
      */
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;

  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;

  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;

  case T_IRQ0 + 7:

  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if (myproc() == 0 || (tf->cs & 3) == 0)
    {
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n", tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno, tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
  {
    exit();
  }
#ifdef RR
  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER)
  {
    //cprintf(BMAG"RR : Yielding process with pid %d\n"ANSI_RESET, myproc()->pid);
    //MY RR timer is one unit only
    yield();
  }
#endif

#ifdef FCFS
//NO PREEMPTION in case of FCFS
// // Force process to give up CPU on clock tick.
// // If interrupts were on while locks held, would need to check nlock.
// if (myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER)
// {
//   /*docs
//    It is important to understand one particular aspect of the main trap handling function. In particular, pay attention to what happens if the interrupt was a timer interrupt. In this case, the trap
//   function tries to yield the CPU to another process (line 3424).
//   */
//   //cprintf(BMAG"Yielding process with pid %d\n"ANSI_RESET, myproc()->pid);
//   yield();
// }
#endif

#ifdef PBS
  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  //WOULD GET SCHEDULED AGAIN
  if (myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER)
  {
    //cprintf(BMAG"RR : Yielding process with pid %d\n"ANSI_RESET, myproc()->pid);
    yield();
  }
#endif

#ifdef MLFQ
  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0 + IRQ_TIMER)
  {
    if (myproc()->curr_ticks_got >= runtime_limit[myproc()->curr_queue_id] + 1)
    {
      setup_demotion(myproc());
      if (debug_now == 1)
      {
        cprintf("pid %d at queue %d being forced to relinquish as already spent:  %d\n", myproc()->pid, myproc()->curr_queue_id, myproc()->curr_ticks_got);
      }
      yield();
    }
  }
#endif

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
  {
    exit();
  }
}
