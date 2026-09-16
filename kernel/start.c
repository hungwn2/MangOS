#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main(void);
void timerinit(void);
void d1_uart_hw_init(void);

extern void timervec(void);

// one stack per CPU
__attribute__((aligned(16))) char stack0[4096 * NCPU];

// scratch area used by timervec
uint64 timer_scratch[NCPU][5];

void
start(void)
{
  d1_uart_hw_init();

  // mret should enter supervisor mode.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // enter main() after mret.
  w_mepc((uint64)main);

  // paging disabled during early bring-up.
  w_satp(0);

  // delegate normal traps to S-mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);

  // allow supervisor software interrupts.
  w_sie(r_sie() | SIE_SSIE);

  // give S-mode access to physical memory.
  w_pmpaddr0(0xffffffffULL);
  w_pmpcfg0(0x0f);

  timerinit();

  w_tp(0);

  asm volatile("mret");
}

void
timerinit(void)
{
  int id = 0;

  // D1 timer frequency is much higher than we need.
  // Start with 1,000,000 ticks and adjust later.
  uint64 interval = 1000000;

  uint64 next = r_time() + interval;

  volatile uint32 *cmp = (volatile uint32 *)CLINT_MTIMECMP(id);

  // program 64-bit compare using two 32-bit MMIO writes
  cmp[1] = 0xffffffff;
  cmp[0] = (uint32)next;
  cmp[1] = (uint32)(next >> 32);

  uint64 *scratch = &timer_scratch[id][0];

  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;

  w_mscratch((uint64)scratch);

  // machine-mode timer handler
  w_mtvec((uint64)timervec);

  // enable machine interrupts
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine timer interrupt
  w_mie(r_mie() | MIE_MTIE);

  // permit S-mode to read time
  w_mcounteren(r_mcounteren() | 2);
}