from pathlib import Path
import sys

ROOT = Path.cwd()

def replace_once(path, old, new):
    p = ROOT / path
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"[FAIL] expected text not found in {path}")
    if text.count(old) != 1:
        raise SystemExit(f"[FAIL] expected exactly one match in {path}, found {text.count(old)}")
    p.write_text(text.replace(old, new, 1))
    print(f"[OK] {path}")

replace_once(
    "kernel/param.h",
    "#define NCPU        8                 // maximum number of CPUs",
    "#define NCPU        1                 // Allwinner D1 has one C906 application CPU",
)

replace_once(
    "kernel/kernel.ld",
    "   * ensure that entry.S / _entry is at 0x80000000,\n"
    "   * where qemu's -kernel jumps.\n"
    "   */\n"
    "  . = 0x80000000;",
    "   * MangoPi MQ Pro / Allwinner D1:\n"
    "   * xfel loads kernel.bin at 0x40000000 and jumps there.\n"
    "   */\n"
    "  . = 0x40000000;",
)

p = ROOT / "kernel/memlayout.h"
text = p.read_text()
for old, new in [
    ("#define UART0     0x10000000L", "#define UART0     0x02500000L"),
    ("#define UART0_IRQ 10", "#define UART0_IRQ 18"),
    ("#define KERNBASE 0x80000000L", "#define KERNBASE 0x40000000L"),
]:
    if old not in text:
        raise SystemExit(f"[FAIL] expected text not found in kernel/memlayout.h: {old}")
    text = text.replace(old, new, 1)
p.write_text(text)
print("[OK] kernel/memlayout.h")

replace_once(
    "Makefile",
    "  $K/uart.o \\\n",
    "  $K/uart.o \\\n"
    "  $K/d1_uart.o \\\n",
)

replace_once(
    "Makefile",
    "\t$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym\n",
    "\t$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym\n"
    "\t$(OBJCOPY) -S -O binary $K/kernel $K/kernel.bin\n",
)

replace_once(
    "Makefile",
    "\t$K/kernel fs.img \\\n",
    "\t$K/kernel $K/kernel.bin fs.img \\\n",
)

start_c = r'''#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main(void);
void d1_uart_hw_init(void);

// entry.S needs one stack per CPU.
__attribute__((aligned(16))) char stack0[4096 * NCPU];

// entry.S jumps here in machine mode on stack0.
void
start(void)
{
  d1_uart_hw_init();

  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  w_mepc((uint64)main);
  w_satp(0);

  w_medeleg(0xffff);
  w_mideleg(0xffff);

  w_pmpaddr0(0xffffffffULL);
  w_pmpcfg0(0x0f);

  w_tp(0);

  // Do not use MENVCFG_ADUE, MENVCFG_STCE, or stimecmp yet.
  // The D1 timer path is added after basic UART boot works.

  asm volatile("mret");
}
'''
(ROOT / "kernel/start.c").write_text(start_c)
print("[OK] kernel/start.c")

main_c = r'''#include "types.h"

void d1_uart_puts(const char *s);

void
main(void)
{
  d1_uart_puts("\r\n");
  d1_uart_puts("xv6 on MangoPi MQ Pro / Allwinner D1\r\n");
  d1_uart_puts("stage 1 boot OK\r\n");

  for (;;)
    asm volatile("wfi");
}
'''
(ROOT / "kernel/main.c").write_text(main_c)
print("[OK] kernel/main.c")

d1_uart_c = r'''#include "types.h"
#include "memlayout.h"

#define GPIO_PB_CFG1 0x02000034L
#define UART_BGR     0x0200190cL

#define UART_RBR 0x00
#define UART_THR 0x00
#define UART_DLL 0x00
#define UART_DLH 0x04
#define UART_IER 0x04
#define UART_FCR 0x08
#define UART_LCR 0x0c
#define UART_MCR 0x10
#define UART_LSR 0x14

#define UART_LSR_DR   (1U << 0)
#define UART_LSR_THRE (1U << 5)

static inline uint32
read32(uint64 addr)
{
  return *(volatile uint32 *)addr;
}

static inline void
write32(uint64 addr, uint32 value)
{
  *(volatile uint32 *)addr = value;
}

void
d1_uart_hw_init(void)
{
  uint32 v;

  // PB8 = UART0_TX, PB9 = UART0_RX, function 6.
  v = read32(GPIO_PB_CFG1);
  v &= ~0xffU;
  v |= 0x66U;
  write32(GPIO_PB_CFG1, v);

  // UART0 clock gate + reset deassert.
  v = read32(UART_BGR);
  v |= (1U << 0);
  v |= (1U << 16);
  write32(UART_BGR, v);

  // Polling only for stage 1.
  write32(UART0 + UART_IER, 0);
  write32(UART0 + UART_FCR, 0xf7);
  write32(UART0 + UART_MCR, 0);

  // Enable divisor latch.
  v = read32(UART0 + UART_LCR);
  write32(UART0 + UART_LCR, v | 0x80);

  // xv6-d1 uses divisor 13 for 115200 baud.
  write32(UART0 + UART_DLL, 13);
  write32(UART0 + UART_DLH, 0);

  // 8 data bits, 1 stop bit, no parity.
  write32(UART0 + UART_LCR, 0x03);
}

void
d1_uart_putc(char c)
{
  while ((read32(UART0 + UART_LSR) & UART_LSR_THRE) == 0)
    ;
  write32(UART0 + UART_THR, (uint32)(uint8)c);
}

void
d1_uart_puts(const char *s)
{
  while (*s)
    d1_uart_putc(*s++);
}

int
d1_uart_getc(void)
{
  if ((read32(UART0 + UART_LSR) & UART_LSR_DR) == 0)
    return -1;
  return (int)(read32(UART0 + UART_RBR) & 0xff);
}
'''
(ROOT / "kernel/d1_uart.c").write_text(d1_uart_c)
print("[OK] kernel/d1_uart.c")

print()
print("Stage-1 MangoPi changes applied.")
print("Inspect with: git diff")
print("Build with: make clean && make kernel/kernel")
