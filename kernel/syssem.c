#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sem.h"

#define NSEM 16

static struct semaphore sems[NSEM];

static int
valid_id(int id)
{
  return id >= 0 && id < NSEM;
}

// Initialize semaphore table once during kernel boot.
void
seminit(void)
{
  for (int i = 0; i < NSEM; i++)
    sem_init(&sems[i], 0);
}

uint64
sys_sem_init(void)
{
  int id, count;
  argint(0, &id);
  argint(1, &count);
  if (!valid_id(id) || count < 0) {
    return -1;
  }
  acquire(&sems[id].lock);
  sems[id].count = count;
  release(&sems[id].lock);

  return 0;
}

uint64
sys_sem_wait(void)
{
  int id;
  argint(0, &id);

  if (!valid_id(id))
    return -1;

  sem_wait(&sems[id]);
  return 0;
}

uint64
sys_sem_post(void)
{
  int id;
  argint(0, &id);

  if (!valid_id(id))
    return -1;

  sem_post(&sems[id]);
  return 0;
}