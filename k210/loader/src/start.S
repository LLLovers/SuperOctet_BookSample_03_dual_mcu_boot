.section .text.start,"ax",@progbits
.global _start
_start:
    csrw mie, zero
    csrci mstatus, 8
    csrr a0, mhartid
    bnez a0, .Lsecondary
    # Only hart 0 uses C code and a stack while receiving the image.
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, __stack_top
    call loader_main
1:
    wfi
    j 1b

.Lsecondary:
    # Hart 0 publishes loader_go only after CRC validation and UART result drain.
    la t0, loader_go
2:
    lw t1, 0(t0)
    beqz t1, 2b
    fence rw,rw
    fence.i
    li t0, 0x80000000
    jr t0
