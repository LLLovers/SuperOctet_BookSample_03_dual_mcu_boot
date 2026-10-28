OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY { loader (rwx) : ORIGIN = 0x805FC000, LENGTH = 0x4000 }
PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text .text.*) *(.rodata .rodata.*) } > loader :text
    .data : ALIGN(8) {
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.data .data.* .sdata .sdata.*)
    } > loader :data
    __loader_binary_end = .;
    .bss (NOLOAD) : ALIGN(8) { *(.bss .bss.* .sbss .sbss.* COMMON) } > loader :data
    .stack (NOLOAD) : ALIGN(16) {
        . += 1024;
        __stack_top = .;
    } > loader :data
    __loader_end = .;
    /DISCARD/ : { *(.eh_frame .eh_frame.* .comment) }
}
ASSERT(__loader_binary_end <= 0x805FC800, "K210 loader binary exceeds 2048 bytes")
ASSERT(__loader_end <= 0x80600000, "K210 loader exceeds its 16 KiB reserved region")
