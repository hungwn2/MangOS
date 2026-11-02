.section .text
.global _entry
_entry:
#ifdef PLATFORM_QEMU
        # one 4096-byte stack per hart: sp = stack0 + (hartid + 1) * 4096
        la sp, stack0
        li a0, 4096
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        call start
#else
        # Raw kernel.bin does not zero .bss for us.
        la t0, bss_start
        la t1, bss_end

clear_bss:
        bgeu t0, t1, bss_done
        sd zero, 0(t0)
        addi t0, t0, 8
        j clear_bss

bss_done:

        # D1 is single-core.
        la sp, stack0
        li a0, 4096
        add sp, sp, a0

        li tp, 0

        call start
#endif

spin:
        j spin