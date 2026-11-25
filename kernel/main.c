
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

// static void
// mark(const char *s)
// {
//   d1_uart_puts(s);
//   d1_uart_puts("\r\n");
// }

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
  consoleinit();
  printkinit();
  printk("\nxv6 kernel is booting on MangoPi D1\n\n");

  kinit();
  kvminit();
  kvminithart();
  printk("paging enabled\n");

  procinit();
  trapinit();
  trapinithart();
  plicinithart();
  mmc_disk_init();
  binit();
  iinit();
  fileinit();
  userinit();
  printk("starting scheduler\n");
  scheduler();
#endif
}
