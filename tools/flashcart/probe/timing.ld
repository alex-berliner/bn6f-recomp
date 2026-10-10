ENTRY(_start)
MEMORY {
    rom (rx) : ORIGIN = 0x08D80000, LENGTH = 0x8000
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 0x1000
}
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text .text.*) *(.rodata .rodata.*) . = ALIGN(4); } > rom
    .iwram : { __iwram_start = .; *(.iwram .iwram.*) . = ALIGN(4); __iwram_end = .; } > iwram AT > rom
    __iwram_lma = LOADADDR(.iwram);
    /DISCARD/ : { *(.comment) *(.ARM.*) }
}
