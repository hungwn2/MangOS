
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "defs.h"
#include "mmc.h"
#define TIMEOUT_LOOPS 10000000

// Additional controller bits.
#define SMHC_AHB_ACCESS  (1U << 31)
#define SMHC_FIFO_RESET  (1U << 1)
#define SMHC_ERRORS_MASK 0xBBC2U

#ifndef MMC_FS_SECTOR
#define MMC_FS_SECTOR 2048
#endif

static uint32 rca;
static int initialized;
static struct sleeplock mmc_lock;

static int
smhc_wait(uint32 mask)
{
  for (int i = 0; i < TIMEOUT_LOOPS; i++) {
    uint32 status = ReadReg(SMHC_RINTSTS);

    if (status & SMHC_ERRORS_MASK) {
      printk("smhc error: %x\n", status);
      return -1;
    }

    if (status & mask)
      return 0;
  }

  printk("smhc timeout: %x\n", ReadReg(SMHC_RINTSTS));
  return -1;
}

static int
smhc_send_cmd(uint32 cmd, uint32 arg, uint32 *resp)
{
  WriteReg(SMHC_RINTSTS, 0xFFFFFFFF);
  WriteReg(SMHC_CMDARG, arg);
  WriteReg(SMHC_CMD, CMD_START | cmd);

  if (smhc_wait(CMD_DONE) < 0) {
    printk("CMD%d failed: RINT=%x STATUS=%x CLKCR=%x\n", cmd & 0x3F,
           ReadReg(SMHC_RINTSTS), ReadReg(SMHC_STATUS), ReadReg(SMHC_CLKDIV));
    return -1;
  }

  if (resp)
    *resp = ReadReg(SMHC_RESP0);

  printk("CMD%d OK resp=%x\n", cmd & 0x3F, resp ? *resp : 0);

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
  // HOSC = 24 MHz
  // M = 15, P = 1, fixed post-divider = 2
  // SMHC module clock = 24 MHz / 15 / 2 = 800 kHz
  // Internal card divider = 2 -> approximately 400 kHz

  WriteCCU(SMHC0_CLK_REG, (1U << 31) | (1U << 8) | 14U);
}

static int
smhc_update_clock(void)
{
  WriteReg(SMHC_CMD, CMD_START | CMD_UPCLK | CMD_WAIT);

  for (int i = 0; i < TIMEOUT_LOOPS; i++) {
    if (!(ReadReg(SMHC_CMD) & CMD_START))
      return 0;
  }

  printk("SMHC clock update timeout\n");
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
  v &= ~(0xFU << 24);
  WriteGPIO(PF_CFG0, v);

  // Pull-ups for CMD and DAT pins, not CLK.
  v = ReadGPIO(PF_PULL0);

  for (int i = 0; i < 6; i++) {
    v &= ~(3U << (i * 2));

    if (i != 2)
      v |= (1U << (i * 2));
  }

  WriteGPIO(PF_PULL0, v);
  printk("PF_CFG0=%x\n", ReadGPIO(PF_CFG0));
  printk("PF_PULL0=%x\n", ReadGPIO(PF_PULL0));
  printk("PF_DAT=%x\n", ReadGPIO(PF_DAT));

  printk("Card detected=%d\n", !!(ReadGPIO(PF_DAT) & (1U << 6)));
}

static int
smhc_reset_fifo(void)
{
  WriteReg(SMHC_CTRL, ReadReg(SMHC_CTRL) | SMHC_FIFO_RESET);

  for (int i = 0; i < TIMEOUT_LOOPS; i++) {
    if (!(ReadReg(SMHC_CTRL) & SMHC_FIFO_RESET))
      return 0;
  }

  printk("SMHC FIFO reset timeout\n");
  return -1;
}

int
smhc_init(void)
{
  uint32 resp;
  int ready = 0;
  int i;

  initialized = 0;
  rca = 0;

  printk("SMHC: configuring GPIO\n");
  smhc_gpio_init();

  printk("SMHC: enabling bus clock\n");
  smhc0_bus_enable();

  printk("SMHC: enabling module clock\n");
  smhc0_clock_enable();

  printk("SMHC: CCU clock=%x\n", ReadCCU(SMHC0_CLK_REG));

  // Reset controller, FIFO and DMA.
  WriteReg(SMHC_CTRL, 7);

  for (i = 0; i < TIMEOUT_LOOPS; i++) {
    if ((ReadReg(SMHC_CTRL) & 7) == 0)
      break;
  }

  if (i == TIMEOUT_LOOPS) {
    printk("SMHC controller reset timeout\n");
    return -1;
  }
  // Enable new timing mode (required for D1).
  WriteReg(SMHC_NTSR, ReadReg(SMHC_NTSR) | (1U << 31));

  // Enable software-controlled sampling delay.
  WriteReg(SMHC_SAMP_DL, 1U << 7);

  printk("SMHC NTSR=%x\n", ReadReg(SMHC_NTSR));

  // Enable CPU access to FIFO (disable DMA mode).
  WriteReg(SMHC_CTRL, ReadReg(SMHC_CTRL) | SMHC_AHB_ACCESS);

  WriteReg(SMHC_INTMASK, 0);
  WriteReg(SMHC_IDIE, 0);
  WriteReg(SMHC_DMAC, 0);
  WriteReg(SMHC_RINTSTS, 0xFFFFFFFF);
  WriteReg(SMHC_TMOUT, 0xFFFFFFFF);

  // 1-bit SD mode.
  WriteReg(SMHC_CTYPE, 0);
  WriteReg(SMHC_BLKSIZ, 512);

  // Set internal clock divider to 2.
  // CLKCR divider field = divider - 1.
  WriteReg(SMHC_CLKDIV, 1);

  if (smhc_update_clock() < 0)
    return -1;

  // Enable card output clock.
  WriteReg(SMHC_CLKDIV, ReadReg(SMHC_CLKDIV) | (1U << 16));

  if (smhc_update_clock() < 0)
    return -1;

  printk("SMHC: clock configured\n");

  // CMD0: reset card to idle.
  printk("SMHC: CMD0\n");

  if (smhc_send_cmd(CMD_INIT | 0, 0, 0) < 0)
    return -1;

  // CMD8: SD v2 voltage/interface check.
  printk("SMHC: CMD8\n");

  if (smhc_send_cmd(CMD_RESP | CMD_CRC | 8, 0x1AA, &resp) < 0)
    return -1;

  if ((resp & 0xFFF) != 0x1AA) {
    printk("SMHC: CMD8 invalid response=%x\n", resp);
    return -1;
  }

  // CMD55 + ACMD41: initialize SD card.
  printk("SMHC: ACMD41\n");

  for (i = 0; i < 10000; i++) {
    if (smhc_send_cmd(CMD_RESP | CMD_CRC | 55, 0, 0) < 0)
      return -1;

    // HCS=1, request 2.7-3.6 V operating range.
    if (smhc_send_cmd(CMD_RESP | 41, 0x40FF8000, &resp) < 0)
      return -1;

    if (resp & (1U << 31)) {
      ready = 1;
      break;
    }
  }

  if (!ready) {
    printk("SMHC: card initialization timeout\n");
    return -1;
  }

  // Require SDHC/SDXC sector addressing.
  if (!(resp & (1U << 30))) {
    printk("SMHC: unsupported SDSC card\n");
    return -1;
  }

  printk("SMHC: card ready, OCR=%x\n", resp);

  // CMD2: obtain CID (136-bit response).
  printk("SMHC: CMD2\n");

  if (smhc_send_cmd(CMD_RESP | CMD_LONG | CMD_CRC | 2, 0, 0) < 0)
    return -1;

  // CMD3: request relative card address.
  printk("SMHC: CMD3\n");

  if (smhc_send_cmd(CMD_RESP | CMD_CRC | 3, 0, &resp) < 0)
    return -1;

  rca = resp & 0xFFFF0000;

  printk("SMHC: RCA=%x\n", rca);

  // CMD7: select the SD card.
  printk("SMHC: CMD7\n");

  if (smhc_send_cmd(CMD_RESP | CMD_CRC | 7, rca, 0) < 0)
    return -1;

  // Wait until card is no longer busy.
  for (i = 0; i < TIMEOUT_LOOPS; i++) {
    if (!(ReadReg(SMHC_STATUS) & CARD_BUSY))
      break;
  }

  if (i == TIMEOUT_LOOPS) {
    printk("SMHC: card busy timeout\n");
    return -1;
  }

  initialized = 1;

  printk("SMHC: SD card initialized successfully\n");
  return 0;
}

int
smhc_read(uint32 sector, void *buf)
{
  uint32 *data = (uint32 *)buf;
  int t;

  if (!initialized || ((uint64)buf & 3))
    return -1;

  if (smhc_reset_fifo() < 0)
    return -1;

  // Ensure FIFO is accessible by the CPU.
  WriteReg(SMHC_CTRL, ReadReg(SMHC_CTRL) | SMHC_AHB_ACCESS);

  WriteReg(SMHC_BLKSIZ, 512);
  WriteReg(SMHC_BYTCNT, 512);

  // CMD17: read a single 512-byte sector.
  WriteReg(SMHC_RINTSTS, 0xFFFFFFFF);
  WriteReg(SMHC_CMDARG, sector);
  WriteReg(SMHC_CMD, CMD_START | CMD_WAIT | CMD_DATA | CMD_CRC | CMD_RESP | 17);

  // Receive 128 32-bit words from FIFO.
  for (int i = 0; i < 128; i++) {
    for (t = 0; t < TIMEOUT_LOOPS; t++) {
      if (ReadReg(SMHC_RINTSTS) & SMHC_ERRORS_MASK)
        goto fail;

      if (!(ReadReg(SMHC_STATUS) & FIFO_EMPTY))
        break;
    }

    if (t == TIMEOUT_LOOPS) {
      printk("SMHC: read FIFO timeout at word %d\n", i);
      goto fail;
    }

    data[i] = ReadReg(SMHC_FIFO);
  }

  // Verify command completion.
  if (smhc_wait(CMD_DONE) < 0)
    goto fail;

  // Verify data transfer completion.
  if (smhc_wait(DATA_OVER) < 0)
    goto fail;

  return 0;

fail:
  printk("SMHC read failed: sector=%u status=%x\n", sector,
         ReadReg(SMHC_RINTSTS));
  return -1;
}

int
smhc_write(uint32 sector, const void *buf)
{
  const uint32 *data = (const uint32 *)buf;
  int t;

  if (!initialized || ((uint64)buf & 3))
    return -1;

  if (smhc_reset_fifo() < 0)
    return -1;

  WriteReg(SMHC_CTRL, ReadReg(SMHC_CTRL) | SMHC_AHB_ACCESS);

  WriteReg(SMHC_BLKSIZ, 512);
  WriteReg(SMHC_BYTCNT, 512);

  // CMD24: write a single 512-byte sector.
  WriteReg(SMHC_RINTSTS, 0xFFFFFFFF);
  WriteReg(SMHC_CMDARG, sector);
  WriteReg(SMHC_CMD, CMD_START | CMD_WAIT | CMD_WRITE | CMD_DATA | CMD_CRC |
                       CMD_RESP | 24);

  // Transmit 128 32-bit words through FIFO.
  for (int i = 0; i < 128; i++) {
    for (t = 0; t < TIMEOUT_LOOPS; t++) {
      if (ReadReg(SMHC_RINTSTS) & SMHC_ERRORS_MASK)
        goto fail;

      if (!(ReadReg(SMHC_STATUS) & FIFO_FULL))
        break;
    }

    if (t == TIMEOUT_LOOPS) {
      printk("SMHC: write FIFO timeout at word %d\n", i);
      goto fail;
    }

    WriteReg(SMHC_FIFO, data[i]);
  }

  // Verify command completion.
  if (smhc_wait(CMD_DONE) < 0)
    goto fail;

  // Verify data transfer completion.
  if (smhc_wait(DATA_OVER) < 0)
    goto fail;

  // Wait for card programming to finish.
  for (t = 0; t < TIMEOUT_LOOPS; t++) {
    if (!(ReadReg(SMHC_STATUS) & CARD_BUSY))
      return 0;
  }

  printk("SMHC: card programming timeout\n");

fail:
  printk("SMHC write failed: sector=%u status=%x\n", sector,
         ReadReg(SMHC_RINTSTS));
  return -1;
}

void
mmc_disk_init(void)
{
  initsleeplock(&mmc_lock, "mmc");
  if (smhc_init() < 0)
    panic("mmc_disk_init");
  printk("mmc: xv6 fs at sector %d\n", MMC_FS_SECTOR);
}

void
mmc_disk_rw(struct buf *b, int write)
{
  uint32 sector = MMC_FS_SECTOR + b->blockno * (BSIZE / 512);
  acquiresleep(&mmc_lock);

  for (int i = 0; i < BSIZE / 512; i++) {
    void *p = b->data + i * 512;
    int r;
    push_off();
    if (write) {
      r = smhc_write(sector + i, p);
    } else {
      r = smhc_read(sector + i, p);
    }
    pop_off();
    if (r < 0) {
      panic("mmc_disk_rw");
    }
  }
  releasesleep(&mmc_lock);
  // DEBUG: is the read path sane, and where is the superblock?
  static uint32 probe[128];

  // Sector 0 should hold an MBR: last word = 0xAA55xxxx
  if (smhc_read(0, probe) < 0)
    printk("mmc: sector 0 read failed\n");
  else
    printk("mmc: sector 0 word0=%x word127=%x\n", probe[0], probe[127]);

  // Common places fs.img ends up (superblock = image offset + 2)
  uint32 where[] = {2, 2050, 4098, 8194, 10242, 32770, 34818};
  for (int i = 0; i < NELEM(where); i++) {
    if (smhc_read(where[i], probe) < 0)
      printk("mmc: sector %d read failed\n", where[i]);
    else
      printk("mmc: sector %d word0=%x\n", where[i], probe[0]);
  }
}