
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

// #ifdef PLATFORM_QEMU
// static void
// debug(char c)
// {
//   *(volatile uint8 *)UART0 = c;
// }
// #endif

#ifdef PLATFORM_QEMU
static volatile int started = 0;
#endif

#ifdef PLATFORM_MANGOPI

void d1_uart_puts(const char *);

static void
mark(const char *s)
{
  d1_uart_puts(s);
  d1_uart_puts("\r\n");
}

static uint32 sector_data[128];

#endif

void
main(void)
{

#ifdef PLATFORM_QEMU

  if (cpuid() == 0) {
    consoleinit();
    printkinit();

    printk("\nxv6 kernel is booting\n\n");

    kinit();
    kvminit();
    kvminithart();

    procinit();

    trapinit();
    trapinithart();

    plicinit();
    plicinithart();

    binit();
    iinit();
    fileinit();

    virtio_disk_init();

    userinit();

    __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
  } else {
    while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0)
      ;

    kvminithart();
    trapinithart();
    plicinithart();
  }

  scheduler();

#elif defined(PLATFORM_MANGOPI)

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
#endif
}
