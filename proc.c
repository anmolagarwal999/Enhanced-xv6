#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

//Regular bold text
// #define BBLK "\e[1;30m"
// #define BRED "\e[1;31m"
// #define BGRN "\e[1;32m"
// #define BYEL "\e[1;33m"
// #define BBLU "\e[1;34m"
// #define BMAG "\e[1;35m"
// #define BCYN "\e[1;36m"
// #define BWHT "\e[1;37m"
// #define ANSI_RESET "\e[0m"
// #define part cprintf(BRED "---------------------------------\n" ANSI_RESET);

// //Regular bold text
#define BBLK ""
#define BRED ""
#define BGRN ""
#define BYEL ""
#define BBLU ""
#define BMAG ""
#define BCYN ""
#define BWHT ""
#define ANSI_RESET ""
#define part cprintf(BRED "---------------------------------\n" ANSI_RESET);

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct proc *queue_ptr[5][65];
int q_latest_idx[5] = {-1, -1, -1, -1, -1};
int patience_threshold[5] = {60, 20, 25, 30, 35};

//*************************************************************************************************************

void setup_demotion(struct proc *p)
{
  acquire(&ptable.lock);
  p->should_demote = 1;
  release(&ptable.lock);
}

//*************************************************************************************************************
static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}
//-------------------------------------------------------------------------------------------------

char *states_str[] = {
    "unused   \0",
    "embryo   \0",
    "sleep    \0",
    "runable  \0",
    "running  \0",
    "zombie   \0"};

int get_ps_data()
{
  struct proc *p;
  acquire(&ptable.lock);
  // for (int j = 0; j < 5; j++)
  // {
  //   cprintf(BRED "Size of queue id %d is %d\n" ANSI_RESET, j, q_latest_idx[j] + 1);
  // }
  cprintf("pid\tPriority\tState\t  r_time\tw_time\tn_run\tcur_q\tq0\tq1\tq2\tq3\tq4\n");
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state != UNUSED)
    {
      cprintf("%d\t%d\t\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", p->pid, p->curr_priority, states_str[p->state], p->tot_run_time, p->curr_wait_time,
              p->num_times_scheduled,
              p->curr_queue_id,
              p->tot_ticks_got[0],
              p->tot_ticks_got[1],
              p->tot_ticks_got[2],
              p->tot_ticks_got[3],
              p->tot_ticks_got[4]);
    }
  }

  release(&ptable.lock);
  return 0;
}

//-----------------------------------------------------------------------------------------------------
// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
/*
The function mycpu (2437) returns the current
processor’s struct cpu. mycpu does this by reading the processor identifier from the
local APIC hardware and looking through the array of struct cpu for an entry with
that identifier. 
*/
struct cpu *mycpu(void)
{
  int apicid, i;
  /*docs:
  The return value of mycpu is fragile: if the timer were to interrupt and
  cause the thread to be moved to a different processor, the return value would no
  longer be correct. To avoid this problem, xv6 requires that callers of mycpu disable interrupts, and only enable them after they finish using the returned struct cpu.
  */
  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

//---------------------------------------------------------------------------------------------------------------------

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
/*
docs:
The function myproc (2457) returns the struct proc pointer for the process that
is running on the current processor. myproc disables interrupts, invokes mycpu, fetches
the current process pointer (c->proc) out of the struct cpu, and then enables interrupts. If there is no process running, because the the caller is executing in scheduler,
myproc returns zero.
*/
struct proc *myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

int get_maxima(int a, int b)
{
  if (a > b)
  {
    return a;
  }
  else
  {
    return b;
  }
}

int reset_pbs_chances(int new_priority)
{
  //lock already held
  //reset pbs_chances in curr_priority category
  struct proc *p;

  int max_chances_yet = 0;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->curr_priority == new_priority)
    {
      max_chances_yet = get_maxima(max_chances_yet, p->pbs_chances);
    }
  }

  //reset to binary ,, also verify RR followed till now: invariant: max difference between chances yet can be 1
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->curr_priority == new_priority)
    {
      p->pbs_chances -= max_chances_yet;
      if (p->pbs_chances != 0 && p->pbs_chances != 1)
      {
        //cprintf(BGRN "RR invariant violated in PBS scheduling\n" ANSI_RESET);
        return -1;
      }
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------------------------------------------------

/* MEANING OF 'static in c
https://stackoverflow.com/a/558201/6427607
In C, a static function is not visible outside of its translation unit, which is the object file it is compiled into.
*/

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.

/* docs pg 23
The job of allocproc (2473) is to allocate a slot (a struct proc) in the process table and to
initialize the parts of the process’s state required for its kernel thread to execute.

Allocproc is called for each new process

The allocproc function creates a new process data structure, and allocates a new kernel stack.
On the kernel stack of a new process, it pushes a trap frame and a context structure. The fork
function copies the parent’s trap frame onto the child’s trap frame (line 2572). As a result, both
child and the parent return to the same point in the program, right after the fork system call.
*/

void initialize_new_fields(struct proc *p)
{
  // if(p==0)
  // {
  //   panic("Pointer to NULL process is trying to get initialized\n");
  // }

  ///////////////////////////////////
  p->creation_time = ticks;
  p->tot_sleep_time = 0;
  p->tot_run_time = 0;
  p->num_times_scheduled = 0;

  p->end_time = 0;
  p->curr_wait_time = 0;

  //also set in allocproc itself while lock is held to prevent race condition with set_priority
  p->curr_priority = 60;
  p->pbs_chances = 0;
/////////////////////////////////////////
#ifndef PBS
  p->curr_priority = -1;
#endif

#ifdef MLFQ
  p->curr_ticks_got = 0;
  p->curr_queue_id = 0;
  p->enter = ticks;
  for (int i = 0; i < 5; i++)
  {
    p->tot_ticks_got[i] = 0;
  }
#endif
}

static struct proc *allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  /* docs 
  Allocproc scans the proc table for a slot with state UNUSED (2480-2482). When
  it finds an unused slot, allocproc sets the state to EMBRYO to mark it as used and
  gives the process a unique pid (2469-2489). 
*/
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
    {
      goto found;
    }
  }

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->curr_priority = 60;
  // cprintf(BYEL "Inside allocproc() allotted pid %d\n" ANSI_RESET, p->pid);

  release(&ptable.lock);

  // docs pg 24: Allocproc tries to Allocate kernel stack for process's kernel thread
  if ((p->kstack = kalloc()) == 0)
  {
    /* docs pg 24
    If the memory allocation fails, allocproc changes the
  state back to UNUSED and returns zero to signal failure.
  */
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  /*docs:
  push address of trapret on the stack
  */
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  initialize_new_fields(p);

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
/* docs pg 23
Userinit’s first action is to call allocproc.
It is called only for the first process
*/

void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
  {
    panic("userinit: out of memory?");
  }
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
#ifdef MLFQ
  push_back_process(p, 0);

#endif
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
    {
      return -1;
    }
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

//#################################################################################################################################
// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.

/* docs :job of fork() is to fill elements of proc struct


*/
int fork(void)
{
  int i, pid;
  struct proc *np; // docs: pointer to new process that is being created
  struct proc *curproc = myproc();

  // Allocate process.
  /* 
  https://youtu.be/JOaBNuXgqqw?t=210
  docs : allocproc will pick an unused proc entry,
  assign new pid,
  set state to EMBRYO,
  allocate kernel stack
  */

  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  /* 
  docs:
  copyuvm will copy page directory
  from parent process(curproc->pgdir) to child process (np->pgdir).

  Parent process is process invoking fork system call,
  its pcb is pointed by curproc
  */
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    /* docs:
    This 'if' is executed only if the copyuvm fails.
    On failure,
    the kernel stack space must be freed,
    it must repoint to NULL,
    and state of ptr wil change from EMBRYO to UNUSED
    */
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }

  ////////////////////////////////////////////////////////////////////////////////////
  /* docs:
  here we copy some parameters of parent to the child
  */
  np->sz = curproc->sz; //initially , size of process memory of parent and child will be same
  np->parent = curproc;
  *np->tf = *curproc->tf;

  /* docs:
  in child process, set eax to zero, 
  this ensures that in child process, ret value of fork is zero*/
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  //------------------------
  /* docs
copies following stuff from parent to child
file pointers,
current working directory (cwd),
executable name
*/
  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  //--------------------------
  pid = np->pid;
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  acquire(&ptable.lock);
  // docs: make child eligible to be selected by SCHEDULER
  np->state = RUNNABLE;

//------------------------ONLY NEEDED FOR PBS------------------------------------------
//lock already held, time to reset chances of old processes
#ifdef PBS
  reset_pbs_chances(np->curr_priority);
#endif
  //------------------------------------------------------------------------------------
#ifdef MLFQ
  if (logs == 1)
  {
    cprintf("[%d] %d %d I\n", ticks, np->pid, np->curr_queue_id);
  }
  if (debug_now == 1)
  {
    cprintf("TICKS: [%d]| pid: %d  queue_num: %d Initial insertion into queue\n", ticks, np->pid, np->curr_queue_id);
  }
  push_back_process(np, 0);
#endif
  release(&ptable.lock);

  //docs: parent process is returned pid of child
  return pid;
}

//#################################################################################################################################
/* docs:
https://youtu.be/JOaBNuXgqqw?t=580
*/

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
  {
    panic("init exiting");
  }

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;
  /* docs:
  When a child calls exit (line 2604), it acquires ptable.lock, and wakes up its
parent. Note that the exit function does not actually free up the memory of the process. Instead,
the process simply marks itself as a zombie, and relinquishes the CPU by calling sched. 
  */
  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  /* Before exit yields the processor, it reparents all of the exiting
process’s children, passing them to the initproc (2653-2660).*/
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  /*docs:
  When a process terminates itself using exit, it calls sched
one last time to give up the CPU (line 2641). 
  */

  curproc->end_time = ticks;
  // cprintf(BYEL "[%d] Inside exit() for pid %d with overall runtime %d and overall wait time %d\n" ANSI_RESET, ticks, curproc->pid,
  //         curproc->tot_run_time,
  //         (curproc->end_time - curproc->creation_time) - (curproc->tot_run_time + curproc->tot_sleep_time));
  // cprintf(BYEL "[%d] Inside exit() for pid %d with overall runtime %d || and overall wait time %d || turnaround-time: %d\n" ANSI_RESET, ticks, curproc->pid,
  //         curproc->tot_run_time,
  //         (curproc->end_time - curproc->creation_time) - (curproc->tot_run_time + curproc->tot_sleep_time),
  //         (curproc->end_time - curproc->creation_time));
  cprintf(BYEL "[%d] Inside exit() for name %s pid %d with overall runtime %d || and overall wait time %d || turnaround-time: %d\n" ANSI_RESET, ticks, curproc->name, curproc->pid,
          curproc->tot_run_time,
          (curproc->end_time - curproc->creation_time) - (curproc->tot_run_time + curproc->tot_sleep_time),
          (curproc->end_time - curproc->creation_time));
  sched();
  panic("zombie exit");
}

//##########################################################
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  //cprintf(BYEL "Inside wait()for pid %d\n" ANSI_RESET, curproc->pid);

  /*docs:
hen a parent calls wait, the wait function (line 2653) acquires ptable.lock, and looks
over all processes to find any of its zombie children. If none is found, it calls sleep. 
*/
  acquire(&ptable.lock);
  // docs: infinite loop
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
      {
        // docs: check if proc which called 'wait' is parent of the ith entry in ptable
        continue;
      }

      // docs: There is atleast one non-zombie child of calling process
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found child whose ptr is 'p' and state=ZOMBIE e child has exited
        pid = p->pid;

        // docs: free kernel stack and set ptr to NULL
        kfree(p->kstack);
        p->kstack = 0;

        //docs: deallocate pg directory
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    //docs: this instruction is reached only if there exists atleast one child of parent but none of the childfen have exited
    /*docs:
    Note that the lock provided to sleep in this case is also ptable.lock, so sleep must not attempt to re-lock it again. 
    */
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}
//#################################################################################################################################
int waitx(int *wtime, int *rtime)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  cprintf(BYEL "Inside waitX XX ()for pid %d\n" ANSI_RESET, curproc->pid);

  /*docs:
hen a parent calls wait, the wait function (line 2653) acquires ptable.lock, and looks
over all processes to find any of its zombie children. If none is found, it calls sleep. 
*/
  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
      {
        // docs: check if proc which called 'wait' is parent of the ith entry in ptable
        continue;
      }

      // docs: There is atleast one non-zombie child of calling process
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found child whose ptr is 'p' and state=ZOMBIE e child has exited
        pid = p->pid;

        // docs: free kernel stack and set ptr to NULL
        kfree(p->kstack);
        p->kstack = 0;

        *rtime = p->tot_run_time;
        *wtime = (p->end_time - p->creation_time) - (p->tot_run_time + p->tot_sleep_time);

        //docs: deallocate pg directory
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    //docs: this instruction is reached only if there exists atleast one child of parent but none of the childfen have exited
    /*docs:
    Note that the lock provided to sleep in this case is also ptable.lock, so sleep must not attempt to re-lock it again. 
    */
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}
//#################################################################################################################################
int set_priority(int new_priority, int proc_pid)
{
  struct proc *p;
  struct proc *chosen_proc = 0;
  // if (debug_now)
  // {
  //   //cprintf(BRED "Args received by system call in proc.c are new_priority : %d  and pid as %d\n" ANSI_RESET, new_priority, proc_pid);
  // }

  if (new_priority < 0 || new_priority > 100)
  {
    //as priority has to be in [0,100]
    return -4;
  }

  int found_proc = 0;
  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == proc_pid)
    {
      chosen_proc = p;
      found_proc = 1;
      break;
    }
  }

  if (found_proc == 1)
  {
    if (chosen_proc == 0)
    {
      if (debug_now == 1)
      {
        cprintf(BRED "++++++++++++++\nWeird stuff happening while setting priority\n++++++++++\n" ANSI_RESET);
      }
      release(&ptable.lock);
      return -1;
    }
    else
    {
      if (chosen_proc->state == UNUSED || chosen_proc->state == ZOMBIE)
      {
        //can't have race condition with allocproc as lock has to be held in both
        //release lock before returning
        release(&ptable.lock);
        //-2 -> proc_pid is currently unused or zombie
        return -2;
      }
      else
      {

        //lock ptable already held
        reset_pbs_chances(new_priority);

        int old_priority = chosen_proc->curr_priority;
        chosen_proc->curr_priority = new_priority;
       // cprintf(BRED "New priority of pid %d is %d\n" ANSI_RESET, chosen_proc->pid, chosen_proc->curr_priority);
        //release lock before returning
        chosen_proc->pbs_chances = 0;
        release(&ptable.lock);
        return old_priority;
      }
    }
  }
  else
  {
    //release lock before returning
    release(&ptable.lock);
    //-3 -> proc_pid not found
    return -3;
  }
  //I think this release is redundant-> figure it out
  release(&ptable.lock);
}
//#################################################################################################################################

void update_process_stats()
{
  struct proc *p;
  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p == 0 || p->killed || p->pid == 0)
    {
      continue;
    }
    if (p->state == RUNNING)
    {
      p->tot_run_time++;
#ifdef MLFQ

      p->curr_ticks_got++;
      p->tot_ticks_got[p->curr_queue_id]++;
#endif
    }
    else if (p->state == SLEEPING)
    {
      p->tot_sleep_time++;
    }
    else if (p->state == RUNNABLE)
    {
      //needed for ps display
      p->curr_wait_time++;
    }
  }

  release(&ptable.lock);
}

//#################################################################################################################################

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

/*docs:
  This scheduler thread loops in this function forever, finding active processes to run and context
  switching to them. The job of the scheduler function is to look through the list of processes, find
  a runnable process, set its state to RUNNING, and switch to the process. 
  
  Every CPU has a scheduler thread that calls the scheduler function (line 2708) at the start,
and loops in it forever. The job of the scheduler function is to look through the list of processes,
find a runnable process, set its state to RUNNING, and switch to the process
*/
#ifdef RR
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  //cprintf(BYEL "Inside scheduler() for cpu id %d\n" ANSI_RESET, c->apicid);
  int i = 0;
  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    i = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
      {
        i++;
        continue;
      }

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      /*swutch to process's page table*/
      switchuvm(p);

      p->state = RUNNING;
      p->curr_wait_time = 0;
      p->num_times_scheduled++;

      /*
docs:
The swtch function (line 2950) does the job of switching between two contexts, and old one
and a new one. The step of pushing registers into the old stack is exactly similar to the step
of restoring registers from the new stack, because the new stack was also created by swtch
at an earlier time.
*/
      //cprintf(BCYN "Inside SCHEDULER choosing process with pid %d and 'i': %d\n" ANSI_RESET, p->pid, i);
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      i++;
    }
    release(&ptable.lock);
  }
}
#endif

#ifdef FCFS
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  int found_proc = 0;
  struct proc *oldest_proc = 0;

  //cprintf(BYEL "Inside scheduler() for cpu id %d\n" ANSI_RESET, c->apicid);
  int i = 0;
  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    found_proc = 0;
    i = 0;
    oldest_proc = 0; //pointing to null
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
      {
        i++;
        continue;
      }
      else
      {
        //make comparisons
        //also take care of no process found
        //cprintf(BRED"i is %d\n"ANSI_RESET, i);

        if (found_proc == 0)
        {
          oldest_proc = p;
          found_proc = 1;
        }
        else
        {
          if (p->creation_time < oldest_proc->creation_time)
          {
            oldest_proc = p;
          }
        }

        i++;
      }
    }
    if (found_proc == 1)
    {
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      //----------------------------------------------
      p = oldest_proc;
      //--------------------------------------------------
      c->proc = p;
      /*swutch to process's page table*/
      switchuvm(p);

      p->state = RUNNING;
      p->curr_wait_time = 0;
      p->num_times_scheduled++;

      //cprintf(BCYN "Inside SCHEDULER choosing process with pid %d with creation time %d\n" ANSI_RESET, p->pid, p->creation_time);
      // if (debug_now == 1)
      // {
      //cprintf(BCYN "Inside SCHEDULER choosing process with pid %d with creation time %d\n" ANSI_RESET, p->pid, p->creation_time);
      // }

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      //part;
    }

    release(&ptable.lock);
  }
}
#endif
//------------------------------------------------------------------------------------------------------------------

#ifdef PBS
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  //find a runnable process with MIN priority AND MIN PBS CHANCES
  int min_prioirity_existing = -1;
  int min_pbs_chances = -1;
  int start_idx = 0;

  //cprintf(BYEL "Inside scheduler() for cpu id %d\n" ANSI_RESET, c->apicid);
  int i = 0;
  start_idx = 0;
  const int max_chances_poss = 147483647;
  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    min_prioirity_existing = 1001;
    min_pbs_chances = max_chances_poss;
    i = 0;
    //finding the process with min priority and min chances gotten till now
    for (i = 0; i < NPROC; i++)
    {
      p = &ptable.proc[i];
      if (p->state != RUNNABLE)
      {
        continue;
      }
      else
      {
        if (p->curr_priority < min_prioirity_existing)
        {
          min_prioirity_existing = p->curr_priority;
          min_pbs_chances = p->pbs_chances;
        }
        else if (p->curr_priority == min_prioirity_existing)
        {
          if (min_pbs_chances < p->pbs_chances)
          {
            min_pbs_chances = p->pbs_chances;
          }
        }
      }
    }

    if (min_prioirity_existing == 101)
    {
      goto lock_release_ceremony;
    }

    int num_checked_yet = 0;

    for (i = start_idx; num_checked_yet < NPROC; i = (i + 1) % NPROC, num_checked_yet++)
    {
      p = &ptable.proc[i];
      if (p->state != RUNNABLE)
      {
        continue;
      }

      if (p->curr_priority != min_prioirity_existing || p->pbs_chances != min_pbs_chances)
      {
        continue;
      }

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      /*swutch to process's page table*/
      switchuvm(p);

      p->state = RUNNING;
      p->curr_wait_time = 0;

      /*
      docs:
        The swtch function (line 2950) does the job of switching between two contexts, and old one
    of restoring registers from the new stack, because the new stack was also created by swtch
    and a new one. The step of pushing registers into the old stack is exactly similar to the step
    at an earlier time.
*/
      //cprintf(BCYN "Inside PBS SCHEDULER choosing process with pid %d and 'i': %d\n" ANSI_RESET, p->pid, i);
      swtch(&(c->scheduler), p->context);
      switchkvm();
      start_idx = (i + 1) % NPROC;

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;

      //breaking as a rechecking of prioirities is needed
      break;
    }

  lock_release_ceremony:
    release(&ptable.lock);
  }
}
#endif

#ifdef MLFQ
void scheduler(void)
{
  struct proc *p = 0;

  struct cpu *c = mycpu();
  c->proc = 0;

  while (1)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    //0 queue processes aren't allowed
    //START FROM QUEUE 1
    for (int i = 1; i < 5; i++)
    {
      for (int j = 0; j <= q_latest_idx[i]; j++)
      {
        struct proc *curr_p = queue_ptr[i][j];
        if (curr_p->curr_wait_time > patience_threshold[i])
        {
          delete_process(curr_p, i);
          if (debug_now)
          {
            cprintf(BGRN "Process %d PROMOTED queue %d due to age timeas already waited for %d\n" ANSI_RESET, curr_p->pid, i - 1, curr_p->curr_wait_time);
          }
          if (logs == 1)
          {
            cprintf(BMAG "[%d] %d %d P\n" ANSI_RESET, ticks, curr_p->pid, i - 1);
          }
          //resetting waiting time as queues have been switched
          curr_p->curr_wait_time = 0;
          push_back_process(curr_p, i - 1);
        }
      }
    }

    for (int i = 0; i < 5; i++)
    {
      if (q_latest_idx[i] >= 0)
      {
        p = queue_ptr[i][0];
        delete_process(p, i);
        goto found_one;
      }
    }
    goto lock_release_ceremony;
  found_one:
    if (p != 0 && p->state == RUNNABLE)
    {
      p->curr_ticks_got = 0;
      p->num_times_scheduled++;
      if (debug_now == 1)
      {
        cprintf("[%d] Scheduling pid %d from Queue %d\n", ticks, p->pid, p->curr_queue_id);
      }
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      //wait time is resetted
      p->curr_wait_time = 0;
      swtch(&c->scheduler, p->context);
      switchkvm();
      c->proc = 0;

      if (p == 0)
      {
        //weird stuff
        goto lock_release_ceremony;
      }

      //state can either be sleeping or runnable
      //sleeping -> deal in wakeup1 regarding reputting in queue
      if (p->state == RUNNABLE)
      {
        if (p->should_demote == 1)
        {
          p->should_demote = 0;
          p->curr_ticks_got = 0;
          p->curr_wait_time = 0;
          // int old = p->curr_queue_id;
          if (p->curr_queue_id < 4)
          {
            p->curr_queue_id++;
            if (debug_now == 1)
            {
              cprintf(BMAG "DEMOTING Process %d to Queue %d\n" ANSI_RESET, p->pid, p->curr_queue_id);
            }
            if (logs == 1)
            {
              cprintf(BMAG "[%d] %d %d D1\n" ANSI_RESET, ticks, p->pid, p->curr_queue_id);
            }
          }
          else
          {
            if (debug_now == 1)
            {
              cprintf(BBLU "RETAINING Process %d at Queue %d\n" ANSI_RESET, p->pid, p->curr_queue_id);
            }
          }
        }
        push_back_process(p, p->curr_queue_id);
      }
    }
  lock_release_ceremony:
    release(&ptable.lock);
  }
}
#endif

//-----------------------------------------------------------------------------------------------------

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
  {
    panic("sched ptable.lock");
  }
  if (mycpu()->ncli != 1)
  {
    panic("sched locks");
  }
  if (p->state == RUNNING)
  {
    panic("sched running");
  }
  if (readeflags() & FL_IF)
  {
    panic("sched interruptible");
  }
  intena = mycpu()->intena;
  /*
  DOCS:
  A running process always gives up the CPU at this call to swtch in line 2766, and always
resumes execution at this point at a later time. The only exception is a process running for the
first time, that never would have called swtch at line 2766, and hence never resumes from
there.

 xv6 holds ptable.lock across calls to swtch: the caller of
swtch must already hold the lock, and control of the lock passes to the switched-to
code. 
  */
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); //DOC: yieldlock
                         // cprintf(BCYN "Inside yield() for process with pid %d\n" ANSI_RESET, myproc()->pid);

  myproc()->state = RUNNABLE;
  sched();

  /*docs:
    Any function that calls sched must do so with the ptable.lock held. This lock is held all
during the context switch. During a context switch from P1 to P2, P1 locks ptable.lock,
calls sched, which switches to the scheduler thread, which again switches to process P2. When
process P2 returns from sched, it releases the lock. For example, you can see the lock and
release calls before and after the call to sched in yield, sleep, and exit. Note that the
function forkret also releases the lock for a process that is executed for the first time, since a new
process does not return in sched. Typically, a process that locks also does the corresponding
unlock, except during a context switch when a lock is acquired by one process and released by
the other.
    */

  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

//------------------------------------------------------------------------------------------------------
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  /*Sanity check: a process must exist*/
  if (p == 0)
    panic("sleep");

  /*Sanity check: a process must be holding a lock*/
  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {
    acquire(&ptable.lock);
    /*docs:   Now that sleep holds ptable.lock, it is safe to release lk: some other process may start a
call to wakeup(chan), but wakeup will not run until it can acquire ptable.lock, so it
must wait until sleep has finished putting the process to sleep, keeping the wakeup
from missing the sleep.
   */

    release(lk);
  }
  // Go to sleep.
  p->chan = chan;

  //cprintf(BRED "Inside SLEEP() for pid %d\n" ANSI_RESET, p->pid);
  p->state = SLEEPING;

  /*docs:
When a process has to block for an event
and sleep, it calls `sched` to give up the CPU (line 2825). The function sched simply checks
various conditions, and calls swtch to switch to the scheduler thread.
*/
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
#ifdef MLFQ
      //NOW, on being scheduled, process will resume in sleep and then below syscall() in trap
      //Already ignored this is lower half of scheduler.c, so queue insertion must be done here
      p->curr_ticks_got = 0;
      push_back_process(p, p->curr_queue_id);
      if (debug_now == 1)
      {
        cprintf(BBLU "RETAINING Process %d at Queue %d\n" ANSI_RESET, p->pid, p->curr_queue_id);
      }
      // if (logs == 1)
      // {
      //   cprintf("[%d] %d %d R\n", ticks, p->pid, p->curr_queue_id);
      // }
#endif
    }
  }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
      {
        /* PUSH BACK NEEDED HERE AS pg 71 of MIT documentation
        kill does very little: it just sets the victim’s p->killed and, if it is sleeping,
        wakes it up. Eventually the victim will enter or leave the kernel, at which point code
      in trap will call exit if p->killed is set. If the victim is running in user space, it
        will soon enter the kernel by making a system call or because the timer (or some other
        device) interrupts
        */
#ifdef MLFQ
        push_back_process(p, p->curr_queue_id);
#endif
        p->state = RUNNABLE;
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

#ifdef MLFQ
int push_back_process(struct proc *p, int q_id)
{

  for (int i = 0; i < q_latest_idx[q_id]; i++)
  {
    if (p->pid == queue_ptr[q_id][i]->pid)
      return -1;
  }
  q_latest_idx[q_id]++;
  queue_ptr[q_id][q_latest_idx[q_id]] = p;
  //p->curr_wait_time = 0;
  p->curr_queue_id = q_id;

  return 1;
}

int delete_process(struct proc *p, int q_id)
{
  int shift_begin_pos = -1;
  for (int i = 0; i <= q_latest_idx[q_id]; i++)
  {
    if (queue_ptr[q_id][i]->pid == p->pid)
    {
      shift_begin_pos = i;
      goto found_pos;
    }
  }
  return 0;
found_pos:
  for (int i = shift_begin_pos; i < q_latest_idx[q_id]; i++)
  {
    queue_ptr[q_id][i] = queue_ptr[q_id][i + 1];
  }
  queue_ptr[q_id][q_latest_idx[q_id]] = 0;
  q_latest_idx[q_id] -= 1;
  return 1;
}
#endif