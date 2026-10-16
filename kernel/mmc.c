
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"

#define CCU_BASE   0x02001000UL
#define GPIO_BASE  0x02000000UL
#define SMHC0_BASE 0x04020000UL

// CCU
#define SMHC0_CLK_REG 0x0830
#define SMHC_BGR_REG  0x084C

#define SMHC0_RST    16
#define SMHC0_GATING 0

// GPIO Port F
#define PF_CFG0  0xB4
#define PF_DRV0  0xC8
#define PF_PULL0 0xD0

//SMHC
#define SMHC_CTRL    0x000
#define SMHC_CLKDIV  0x004
#define SMHC_TMOUT   0x008
#define SMHC_CTYPE   0x00C
#define SMHC_BLKSIZ  0x010
#define SMHC_BYTCNT  0x014
#define SMHC_CMD     0x018
#define SMHC_CMDARG  0x01C
#define SMHC_RESP0   0x020
#define SMHC_INTMASK 0x030
#define SMHC_RINTSTS 0x038
#define SMHC_STATUS  0x03C
#define SMHC_FIFOTH  0x040

#define SMHC_DMAC 0x080
#define SMHC_DLBA 0x084
#define SMHC_IDST 0x088
#define SMHC_IDIE 0x08C
#define SMHC_FIFO 0x200

#define Reg(offset)           ((volatile uint32 *)(SMHC0_BASE + (offset)))
#define ReadReg(offset)       (*Reg(offset))
#define WriteReg(offset, val) (*Reg(offset) = (val))

#define CCUReg(offset)        ((volatile uint32 *)(CCU_BASE + (offset)))
#define ReadCCU(offset)       (*CCUReg(offset))
#define WriteCCU(offset, val) (*CCUReg(offset) = (val))

#define GPIOReg(offset)        ((volatile uint32 *)(GPIO_BASE + (offset)))
#define ReadGPIO(offset)       (*GPIOReg(offset))
#define WriteGPIO(offset, val) (*GPIOReg(offset) = (val))

/* Command bits */
#define CMD_START (1U << 31)
#define CMD_UPCLK (1U << 21)
#define CMD_INIT  (1U << 15)
#define CMD_WAIT  (1U << 13)
#define CMD_WRITE (1U << 10)
#define CMD_DATA  (1U << 9)
#define CMD_CRC   (1U << 8)
#define CMD_LONG  (1U << 7)
#define CMD_RESP  (1U << 6)

/* Interrupt/status bits */
#define CMD_DONE    (1U << 2)
#define DATA_OVER   (1U << 3)
#define FIFO_EMPTY  (1U << 2)
#define FIFO_FULL   (1U << 3)
#define CARD_BUSY   (1U << 9)
#define SMHC_ERRORS 0xBFC2U

#define TIMEOUT_LOOPS 10000000

static uint32 rca;
static int initialized;
static struct sleeplock mmclock;

static int
smhc_wait(uint32 mask)
{
  for (int i = 0; i < TIMEOUT_LOOPS; i++) {
    uint32 status = ReadReg(SMHC_RINTSTS);

    if (status & SMHC_ERRORS) {
      printf("smhc error: %x\n", status);
      return -1;
    }

    if (status & mask)
      return 0;
  }

  printf("smhc timeout: %x\n", ReadReg(SMHC_RINTSTS));
  return -1;
}

static int
smhc_send_cmd(uint32 cmd, uint32 arg, uint32 *resp)
{
  WriteReg(SMHC_RINTSTS, 0xFFFFFFFF);
  WriteReg(SMHC_CMDARG, arg);
  WriteReg(SMHC_CMD, CMD_START | cmd);

  if (smhc_wait(CMD_DONE) < 0)
    return -1;

  if (resp)
    *resp = ReadReg(SMHC_RESP0);

  return 0;
}

static void
smhc0_bus_enable(void)
{
  uint32 v = ReadCCU(SMHC_BGR_REG);

  v |= (1U << SMHC0_RST);
  v |= (1U << SMHC0_GATING);

  WriteCCU(SMHC_BGR_REG, v);
}

static void
smhc0_clock_enable(void)
{
  // HOSC 24 MHz, M=1, N=1
  WriteCCU(SMHC0_CLK_REG, 1U << 31);
}

static int
smhc_update_clock(void)
{
  WriteReg(SMHC_CMD, CMD_START | CMD_UPCLK | CMD_WAIT);

  for (int i = 0; i < TIMEOUT_LOOPS; i++)
    if (!(ReadReg(SMHC_CMD) & CMD_START))
      return 0;

  return -1;
}

static void
smhc_gpio_init(void)
{
  uint32 v;

  // PF0-PF5 -> SMHC0 function 2
  v = ReadGPIO(PF_CFG0);

  for (int i = 0; i < 6; i++) {
    v &= ~(0xFU << (i * 4));
    v |= (2U << (i * 4));
  }

  WriteGPIO(PF_CFG0, v);

  // Configure pull-ups for data and command pins.
  // PF2 (CLK) has no pull-up.
  v = ReadGPIO(PF_PULL0);

  for (int i = 0; i < 6; i++) {
    v &= ~(3U << (i * 2));
    if (i != 2)
      v |= (1U << (i * 2));
  }

  WriteGPIO(PF_PULL0, v);
}

int
smhc_init(void)
{
  uint32 resp;
  int ready = 0;

  initialized = 0;
  initsleeplock(&mmclock, "mmc");

  smhc_gpio_init();
  smhc0_bus_enable();
  smhc0_clock_enable();

  // Reset controller/FIFO/DMA
  WriteReg(SMHC_CTRL, 7);

  int i;
  for (i = 0; i < TIMEOUT_LOOPS; i++)
    if ((ReadReg(SMHC_CTRL) & 7) == 0)
      break;

  if (i == TIMEOUT_LOOPS)
    return -1;

  WriteReg(SMHC_INTMASK, 0);
  WriteReg(SMHC_IDIE, 0);
  WriteReg(SMHC_DMAC, 0);
  WriteReg(SMHC_RINTSTS, 0xFFFFFFFF);
  WriteReg(SMHC_TMOUT, 0xFFFFFFFF);

  // 1-bit mode, 512-byte sectors
  WriteReg(SMHC_CTYPE, 0);
  WriteReg(SMHC_BLKSIZ, 512);

  // 24 MHz / (2 * 30) = 400 kHz
  WriteReg(SMHC_CLKDIV, 30);

  if (smhc_update_clock() < 0)
    return -1;

  // Enable output clock (bit 16).
  WriteReg(SMHC_CLKDIV, ReadReg(SMHC_CLKDIV) | (1U << 16));

  if (smhc_update_clock() < 0)
    return -1;

  // CMD0: reset card
  if (smhc_send_cmd(CMD_INIT | 0, 0, 0) < 0)
    return -1;

  // CMD8: SD version 2+, 2.7-3.6 V
  if (smhc_send_cmd(CMD_RESP | CMD_CRC | 8, 0x1AA, &resp) < 0)
    return -1;

  if ((resp & 0xFFF) != 0x1AA)
    return -1;

  // ACMD41: initialize SDHC/SDXC card
  for (i = 0; i < 10000; i++) {
    if (smhc_send_cmd(CMD_RESP | CMD_CRC | 55, 0, 0) < 0)
      return -1;

    if (smhc_send_cmd(CMD_RESP | 41, 0x40FF8000, &resp) < 0)
      return -1;

    if (resp & (1U << 31)) {
      ready = 1;
      break;
    }
  }

  if (!ready)
    return -1;

  // Require SDHC/SDXC block addressing
  if (!(resp & (1U << 30)))
    return -1;

  // CMD2: read CID
  if (smhc_send_cmd(CMD_RESP | CMD_LONG | CMD_CRC | 2, 0, 0) < 0)
    return -1;

  // CMD3: obtain RCA
  if (smhc_send_cmd(CMD_RESP | CMD_CRC | 3, 0, &resp) < 0)
    return -1;

  rca = resp & 0xFFFF0000;

  // CMD7: select card
  if (smhc_send_cmd(CMD_RESP | CMD_CRC | 7, rca, 0) < 0)
    return -1;

  // Keep the slow clock for initial testing.
  initialized = 1;
  printf("smhc: SD card initialized, RCA=%x\n", rca);
  return 0;
}

int
smhc_read(uint32 sector, void *buf)
{
  uint32 *data = (uint32 *)buf;

  if (!initialized || ((uint64)buf & 3))
    return -1;

  acquiresleep(&mmclock);

  // Reset FIFO
  WriteReg(SMHC_CTRL, ReadReg(SMHC_CTRL) | (1U << 1));

  int t;
  for (t = 0; t < TIMEOUT_LOOPS; t++)
    if (!(ReadReg(SMHC_CTRL) & (1U << 1)))
      break;

  if (t == TIMEOUT_LOOPS)
    goto fail;

  WriteReg(SMHC_BLKSIZ, 512);
  WriteReg(SMHC_BYTCNT, 512);

  // CMD17: read single block
  // Clear status, issue command, stream FIFO data.
  WriteReg(SMHC_RINTSTS, 0xFFFFFFFF);
  WriteReg(SMHC_CMDARG, sector);
  WriteReg(SMHC_CMD, CMD_START | CMD_WAIT | CMD_DATA | CMD_CRC | CMD_RESP | 17);

  for (int i = 0; i < 128; i++) {
    for (t = 0; t < TIMEOUT_LOOPS; t++) {
      if (ReadReg(SMHC_RINTSTS) & SMHC_ERRORS)
        goto fail;

      if (!(ReadReg(SMHC_STATUS) & FIFO_EMPTY))
        break;
    }

    if (t == TIMEOUT_LOOPS)
      goto fail;

    data[i] = ReadReg(SMHC_FIFO);
  }

  if (smhc_wait(DATA_OVER) < 0)
    goto fail;

  releasesleep(&mmclock);
  return 0;

fail:
  printf("smhc_read failed: sector=%d status=%x\n", sector,
         ReadReg(SMHC_RINTSTS));
  releasesleep(&mmclock);
  return -1;
}

int
smhc_write(uint32 sector, const void *buf)
{
  const uint32 *data = (const uint32 *)buf;

  if (!initialized || ((uint64)buf & 3))
    return -1;

  acquiresleep(&mmclock);

  WriteReg(SMHC_CTRL, ReadReg(SMHC_CTRL) | (1U << 1));

  int t;
  for (t = 0; t < TIMEOUT_LOOPS; t++)
    if (!(ReadReg(SMHC_CTRL) & (1U << 1)))
      break;

  if (t == TIMEOUT_LOOPS)
    goto fail;

  WriteReg(SMHC_BLKSIZ, 512);
  WriteReg(SMHC_BYTCNT, 512);

  // CMD24: write single block
  WriteReg(SMHC_RINTSTS, 0xFFFFFFFF);
  WriteReg(SMHC_CMDARG, sector);
  WriteReg(SMHC_CMD, CMD_START | CMD_WAIT | CMD_WRITE | CMD_DATA | CMD_CRC |
                       CMD_RESP | 24);

  for (int i = 0; i < 128; i++) {
    for (t = 0; t < TIMEOUT_LOOPS; t++) {
      if (ReadReg(SMHC_RINTSTS) & SMHC_ERRORS)
        goto fail;

      if (!(ReadReg(SMHC_STATUS) & FIFO_FULL))
        break;
    }

    if (t == TIMEOUT_LOOPS)
      goto fail;

    WriteReg(SMHC_FIFO, data[i]);
  }

  if (smhc_wait(DATA_OVER) < 0)
    goto fail;

  // Wait until card completes internal programming
  for (t = 0; t < TIMEOUT_LOOPS; t++)
    if (!(ReadReg(SMHC_STATUS) & CARD_BUSY))
      break;

  if (t == TIMEOUT_LOOPS)
    goto fail;

  releasesleep(&mmclock);
  return 0;

fail:
  printf("smhc_write failed: sector=%d status=%x\n", sector,
         ReadReg(SMHC_RINTSTS));
  releasesleep(&mmclock);
  return -1;
}
