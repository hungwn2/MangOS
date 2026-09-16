#include "types.h"
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
