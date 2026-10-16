
#include "proc.h"
struct thread {
  struct proc;
  int is_thread;
};

int thread_create(void (*fcn)(void *), void *arg,
                  void *stack) int thread_join(void);
int thread_exit(void);