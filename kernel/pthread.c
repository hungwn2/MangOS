#include "pthread.h"

int
thread_create(void (*fcn)(void *), void *arg, void *stack)
{
  int i, pid;
  struct proc *thread;
  struct proc *p = myproc();
  struct proc *np;

  if (fcn == 0 || stack == 0)
    return -1;
  if ((np = allocproc()) == 0) {
    return -1;
  }
  *np->trapframe = *p->trapframe;
  np->trapframe->epc = (uint64)fcn;
  np->trapframe->a0 = (uint64)arg;
  np->trapframe->sp = (uint64)stack + PGSIZE;
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  safestrcpy(np->name, p->name, sizeof(np->name));
  np->cwd = idup(p->cwd);
}
int thread_join(void);
int thread_exit(void);