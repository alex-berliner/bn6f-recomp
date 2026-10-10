ENTRY(_start)
SECTIONS {
    .text 0x08000000 : { KEEP(*(.text.start)) *(.text .text.*) *(.rodata .rodata.*) }
    /DISCARD/ : { *(.comment) *(.ARM.*) }
}
