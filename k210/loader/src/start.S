.section .text.start,"ax",@progbits
.global _start
_start:
    csrw mie, zero
    csrci mstatus, 8
    # ROM MEMORY_BOOT enters on hart 0 only. Hart 1 waits in ROM until
    # the application SDK sends its IPI from register_core1().
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, __stack_top
    call loader_main
1:
    wfi
    j 1b
