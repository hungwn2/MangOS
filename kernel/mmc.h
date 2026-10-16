
#ifndef _MMC_H_
#define _MMC_H_

#include "types.h"

#define CCU_BASE   0x02001000UL
#define GPIO_BASE  0x02000000UL
#define SMHC0_BASE 0x04020000UL

/* CCU registers */
#define SMHC0_CLK_REG 0x0830
#define SMHC_BGR_REG  0x084C

#define SMHC0_RST    16
#define SMHC0_GATING 0

/* GPIO registers */
#define PF_CFG0  0x0B4
#define PF_PULL0 0x0D0

/* SMHC registers */
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

/* SMHC command flags */
#define CMD_START (1U << 31)
#define CMD_UPCLK (1U << 21)
#define CMD_INIT  (1U << 15)
#define CMD_WAIT  (1U << 13)
#define CMD_WRITE (1U << 10)
#define CMD_DATA  (1U << 9)
#define CMD_CRC   (1U << 8)
#define CMD_LONG  (1U << 7)
#define CMD_RESP  (1U << 6)

/* SMHC status flags */
#define CMD_DONE    (1U << 2)
#define DATA_OVER   (1U << 3)
#define FIFO_EMPTY  (1U << 2)
#define FIFO_FULL   (1U << 3)
#define CARD_BUSY   (1U << 9)
#define SMHC_ERRORS 0xBFC2U

/* MMIO access */
#define Reg(offset) ((volatile uint32 *)(SMHC0_BASE + (offset)))

#define ReadReg(offset)       (*Reg(offset))
#define WriteReg(offset, val) (*Reg(offset) = (val))

#define CCUReg(offset) ((volatile uint32 *)(CCU_BASE + (offset)))

#define ReadCCU(offset)       (*CCUReg(offset))
#define WriteCCU(offset, val) (*CCUReg(offset) = (val))

#define GPIOReg(offset) ((volatile uint32 *)(GPIO_BASE + (offset)))

#define ReadGPIO(offset)       (*GPIOReg(offset))
#define WriteGPIO(offset, val) (*GPIOReg(offset) = (val))

/* Public driver API */
int smhc_init(void);
int smhc_read(uint32 sector, void *buf);
int smhc_write(uint32 sector, const void *buf);

#endif
