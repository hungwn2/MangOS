#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void d1_uart_puts(const char *);

static void
mark(const char *s)
{
  d1_uart_puts(s);
  d1_uart_puts("\r\n");
}

void
main(void)
{
  mark("main entered");

  mark("kinit...");
  kinit();

  mark("kvminit...");
  kvminit();

  mark("kvminithart...");
  kvminithart();

  mark("paging enabled");

  mark("procinit...");
  procinit();

  mark("trapinit...");
  trapinit();

  mark("trapinithart...");
  trapinithart();

  mark("basic kernel initialization OK");

  intr_on();

  mark("interrupts enabled");

  uint last = ticks;

  for (;;) {
    if (ticks != last) {
      last = ticks;

      if ((ticks % 10) == 0)
        mark("10 ticks passed");
    }

    asm volatile("wfi");
  }
}