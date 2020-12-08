// Per-CPU state
struct cpu
{
  uchar apicid;              // Local APIC ID
  struct context *scheduler; // swtch() here to enter scheduler
  struct taskstate ts;       // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS]; // x86 global descriptor table
  volatile uint started;     // Has the CPU started?
  int ncli;                  // Depth of pushcli nesting.
  int intena;                // Were interrupts enabled before pushcli?
  struct proc *proc;         // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context
{
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate
{
  UNUSED,
  EMBRYO,   // docs : a new process is currently being created
  SLEEPING, // docs : blocked for io -> unavoidable -> cannot proceed wihout io
  RUNNABLE, //docs : ready to run -> enters this state when it is preemepted for another process to run on the cpu
  RUNNING,  // docs : currently executing
  ZOMBIE
};

// docs READ pg 21 of rev_11.pdf
// Per-process state
struct proc
{
  uint sz; // Size of process memory (bytes)

  //////////////////////////////////////////////
  /*pg 22
  p->pgdir holds the process’s page table,
  in the format that the x86 hardware expects.
  xv6 causes the paging hardware to use a process’s p->pgdir   when executing
  that process. A process’s page table also serves as the record of the addresses of the
  physical pages allocated to store the process’s memory*/

  /* docs : NOT sure but -> HERE, pgdir seems to be the page directory pointer for that process
  which maps pages to frames and is present in kernel memory (not kernel stack I think).
  */
  pde_t *pgdir; // Page table
  ///////////////////////////////////////

  //docs: one kernel stack is present for each process where system calls etc store their registers etc
  char *kstack; // Bottom of kernel stack for this process

  /*pg 22-> p->state indicates whether the process is allocated,
   ready to run, running, waiting for I/O, or exiting*/
  enum procstate state; // Process state

  int pid;                 // Process ID
  struct proc *parent;     // Parent process
  struct trapframe *tf;    // Trap frame for current syscall
  struct context *context; // swtch() here to run process
  void *chan;              // If non-zero, sleeping on chan
  int killed;              // If non-zero, have been killed

  //docs NOFILE = max possible number of open files per process
  struct file *ofile[NOFILE]; // Open files

  //docs cwd -> points to current working directory of the process
  struct inode *cwd; // Current directory
  char name[16];     // Process name (debugging)

  int creation_time;
  int end_time;
  int tot_run_time;
  int curr_wait_time;
  int tot_sleep_time;
  int curr_priority;
  int pbs_chances;

  int should_demote; //queue needs to be changed
  int enter;

  int curr_queue_id; //current queue number
  // int idx_in_curr_queue;
  int num_times_scheduled; //number of times scheduler has chosen this process
  int tot_ticks_got[6];    //ticks received in each queue over entire lifetime
  int curr_ticks_got;      //number of ticks acquired in current cpu slice
  // int idx_in_ptable;
};

//----------------------------------------------------------
//Addition of extra functions to made visible outside
#define debug_now 0
#define logs 1
void update_process_stats();
int push_back_process(struct proc *p, int q_id);
int delete_process(struct proc *p, int q_id);
void setup_demotion(struct proc *p);
//-------------------------------------------------------

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
