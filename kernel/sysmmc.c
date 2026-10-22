
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "mmc.h"

uint64
sys_mmcread(void)
{
  int sector;
  uint64 addr;
  uint32 buf[128];

  argint(0, &sector);
  argaddr(1, &addr);

  if (sector < 0)
    return -1;

  if (smhc_read((uint32)sector, buf) < 0)
    return -1;

  if (copyout(myproc()->pagetable, myproc()->sz, addr, (char *)buf, 512) < 0)
    return -1;

  return 0;
}

uint64
sys_mmcwrite(void)
{
  int sector;
  uint64 addr;
  uint32 buf[128];

  argint(0, &sector);
  argaddr(1, &addr);

  if (sector < 0)
    return -1;

  if (copyin(myproc()->pagetable, myproc()->sz, (char *)buf, addr, 512) < 0)
    return -1;

  return smhc_write((uint32)sector, buf);
}
