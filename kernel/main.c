
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

static uint32 sector_data[128];

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

  mark("printk init...");
  printkinit();
  printk("printk initialized\n");

  mark("trapinit...");
  trapinit();

  mark("trapinithart...");
  trapinithart();

  mark("basic kernel initialization OK");

  mark("smhc_init...");

  if (smhc_init() < 0) {
    mark("SMHC initialization FAILED");
  } else {
    mark("SMHC initialization OK");

    mark("Reading sector 0...");

    if (smhc_read(0, sector_data) < 0) {
      mark("MMC read FAILED");
    } else {
      mark("MMC read OK");

      // Print first 16 words of sector 0.
      for (int i = 0; i < 16; i++)
        printk("%x ", sector_data[i]);

      printk("\n");
    }
  }

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
