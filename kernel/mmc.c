#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"

#define CCU_BASE   0x02001000UL
#define SMHC0_BASE 0x04020000UL

/* CCU */
#define SMHC0_CLK_REG 0x0830
#define SMHC_BGR_REG  0x084C

#define SMHC0_RST    16
#define SMHC0_GATING 0

/* SMHC */
#define SMHC_CTRL   0x000
#define SMHC_CLKDIV 0x004
#define SMHC_CTYPE  0x00C
#define SMHC_BLKSIZ 0x010
#define SMHC_BYTCNT 0x014
#define SMHC_CMD    0x018
#define SMHC_CMDARG 0x01C

#define SMHC_RESP0 0x020

#define SMHC_INTMASK 0x030
#define SMHC_RINTSTS 0x038
#define SMHC_STATUS  0x03C

#define SMHC_FIFOTH 0x040

#define SMHC_DMAC 0x080
#define SMHC_DLBA 0x084
#define SMHC_IDST 0x088
#define SMHC_IDIE 0x08C

#define Reg(offset)           ((volatile uint32 *)(SMHC0_BASE + (offset)))
#define ReadReg(offset)       (*Reg(offset))
#define WriteReg(offset, val) (*Reg(offset) = (val))

#define CCUReg(offset)        ((volatile uint32 *)(CCU_BASE + (offset)))
#define ReadCCU(offset)       (*CCUReg(offset))
#define WriteCCU(offset, val) (*CCUReg(offset) = (val))

static int
smhc_send_cmd(uint32 cmd, uint32 arg)
{
  WriteReg(SMHC_CMDARG, arg);
  WriteReg(SMHC_CMD, cmd);

  while ((ReadReg(SMHC_RINTSTS) & (1U << 2)) == 0)
    ;
  WriteReg(SMHC_RINTSTS, (1U << 2));

  return 0;
}

static void
smhc0_bus_enable(void)
{
  uint32 v;

  v = ReadCCU(SMHC_BGR_REG);

  v |= (1U << SMHC0_RST);
  v |= (1U << SMHC0_GATING);

  WriteCCU(SMHC_BGR_REG, v);
}
static void
smhc0_clock_enable(void)
{
  uint32 v;

  v = ReadCCU(SMHC0_CLK_REG);
  v |= (1U << 31);

  WriteCCU(SMHC0_CLK_REG, v);
}

static void
smhc_update_clock(void)
{
  WriteReg(SMHC_CLKDIV, 0);
  WriteReg(SMHC_CMD, 0x80200000);

  while (ReadReg(SMHC_CMD) & (1U << 31))
    ;
}

static void
smhc_init(void)
{
  uint32 v;

  v = ReadCCU(SMHC_BGR_REG);

  v |= (1U << SMHC0_RST) | (1U << SMHC0_GATING);

  WriteCCU(SMHC_BGR_REG, v);

  v = ReadCCU(SMHC0_CLK_REG);

  v |= (1U << 31);

  WriteCCU(SMHC0_CLK_REG, v);

  WriteReg(SMHC_CTRL, (1U << 0) | (1U << 1) | (1U << 2));

  while (ReadReg(SMHC_CTRL) & 0x7)
    ;

  WriteReg(SMHC_INTMASK, 0);

  WriteReg(SMHC_CLKDIV, 0);

  WriteReg(SMHC_CMD, 0x80200000);

  while (ReadReg(SMHC_CMD) & (1U << 31))
    ;
}