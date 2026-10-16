.section .text
.global _entry
_entry:

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

spin:
        j spin