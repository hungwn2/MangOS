#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sem.h"

void
sem_init(struct semaphore *s, int count)
{
  if (s->count < 0) {
    panic("invalid count");
  }
  s->count = 0;
  initlock(&s->lock, "semaphore");
}

void
sem_wait(struct semaphore *s)
{
  acquire(&s->lock);

  while (s->count == 0) {
    sleep_prepare(s);
    release(&s->lock);
    sleep();
    acquire(&s->lock);
  }
  s->count--;
  release(&s->lock);
}

void
sem_post(struct semaphore *s)
{
  acquire(&s->lock);
  s->count++;
  wakeup(s);
  release(&s->lock);
}