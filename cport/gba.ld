/* Linker script for the lifted BN6 GBA ROM.
 *
 * 0x08000000  original bn6f.gba image (data at its original addresses; the
 *             original code bytes are no longer executed)
 * 0x08800000  lifted C code, runtime, dispatch table
 * IWRAM 0x03005B00-0x03007900: runtime state + host stacks (the block the
 *             original copied its IWRAM routines into)
 */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03005B00, LENGTH = 0x1E00
}

SECTIONS {
    .base 0x08000000 : { KEEP(*(.base)) } > rom
    .text 0x08800000 : {
        KEEP(*(.text.crt0))
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
        . = ALIGN(4);
    } > rom
    .ARM.exidx : { *(.ARM.exidx*) } > rom
    __rom_end = .;

    .data : { *(.data .data.*) } > iwram AT > rom
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > iwram

    __irq_stack_top = 0x03007900;
    __sys_stack_top = 0x03006500;
    ASSERT(__bss_end <= 0x03005E00, "runtime .bss too large for the IWRAM block")
    ASSERT(SIZEOF(.data) == 0, "initialized .data is not supported; use const or .bss")

    /DISCARD/ : { *(.comment) *(.note*) }
}
