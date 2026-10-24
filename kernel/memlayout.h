// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0
// 10001000 -- virtio disk
// 80000000 -- qemu's boot ROM loads the kernel here,
//             then jumps here.
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel
#ifdef PLATFORM_MANGOPI

#define UART0     0x02500000L
#define UART0_IRQ 18

#define KERNBASE   0x40000000L
#define CLINT_BASE 0x14000000L
#define PLIC       0x10000000L

#else // QEMU

#define UART0     0x10000000L
#define UART0_IRQ 10

#define KERNBASE   0x80000000L
#define CLINT_BASE 0x02000000L
#define PLIC       0x0c000000L

#endif

#define PHYSTOP (KERNBASE + 128 * 1024 * 1024)

// GPIO base
#define D1_GPIO 0x02000000UL
// CCU base
#define D1_CCU 0x02001000UL
// SMHC base
#define D1_SMHC 0x04020000UL

// virtio mmio interface
#define VIRTIO0     0x10001000
#define VIRTIO0_IRQ 1

// core-local interrupt controller (CLINT)
#define CLINT_MTIMECMP(hart) (CLINT_BASE + 0x4000 + 8 * (hart))

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC_PRIORITY        (PLIC + 0x0)
#define PLIC_PENDING         (PLIC + 0x1000)
#define PLIC_SENABLE(hart)   (PLIC + 0x2080 + (hart) * 0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart) * 0x2000)
#define PLIC_SCLAIM(hart)    (PLIC + 0x201004 + (hart) * 0x2000)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
