
#ifndef SEM_H
#define SEM_H

#include "spinlock.h"

struct semaphore {
  struct spinlock lock;
  int count;
};
void sem_init(struct semaphore *s, int count);
void sem_wait(struct semaphore *s);
void sem_post(struct semaphore *s);
#endif