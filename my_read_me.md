# ASSIGNMENT 5
- Anmol Agarwal, 201910168

## Answer to question in task 3
MLFQ policy of demoting a process only if it tries to use more than allotted time slice is unfair as a process can **game the scheduler** by making sure that if time slice is x, it always relinquishes CPU at x-1 and thus, keeps it's position in the same queue.

## Implementation of FCFS
I am just finding the oldest created process (which has least value of creation time) which is still runnable and just scheduling it.
```c=
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

      //cprintf(BCYN "Inside SCHEDULER choosing process with pid %d with creation time %d\n" ANSI_RESET, p->pid, p->creation_time);
      if (debug_now == 1)
      {
        cprintf(BCYN "Inside SCHEDULER choosing process with pid %d with creation time %d\n" ANSI_RESET, p->pid, p->creation_time);
      }

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      //part;
    }

    release(&ptable.lock);
  }
```
## Implementation of PBS
note:In case a higher process arrives, I am allowing the current pcorcc to complete its time slice.
Invariant: Number of chances between processes in same queue can be atmost 1
Here, whenever a new process enters a prioirty class, I reset pbs_chances of all processes in that class to either 0 or 1 on a relative basis to ensure fairness and sequentiallity.



#IMPLEMENTATION OF MLFQ
The code is highly commented for readibility and be found in proc.c under scheduler() defined for MLFQ.
The key implementation points are:
* Invariant: queues only contain RUNNABLE processes
* Initial process 1 is pushed in userinit() whereas other first times are pushed into queue 0 in fork()
* Before making a decision based on priority, CPU checks whether a process has aged or not (curr_wait_time variable keeps a record of this). Then it picks the frontmost process from the first non-empty queue.
* My queues are more STL vector like than similar to queues themselves.
* WHen a timer interrupt is encountered, it is checked whether process currently occupying CPU has overstayed. If yes, then process is flagged to be demoted and is made to relinquish the CPU.
* For processes which go into sleep(), they are reinserted into the queue again in wakeup() function.

#FINDINGS:
#SEE ANALYSIS.pdf
