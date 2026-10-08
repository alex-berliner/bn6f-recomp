/* Linker script for the lifted BN6 GBA ROM.
 *
 * 0x08000000  original bn6f.gba image (data at its original addresses; the
 *             original code bytes are no longer executed)
 * 0x08800000  lifted C code, runtime, dispatch table
 * IWRAM 0x03005700-0x03007900: port IWRAM code, runtime state and host
 *             stacks (the SoundMainRAM copy and IWRAM routines the original
 *             placed here run from C instead; see m4a_init.c). The block the
 *             original copied its IWRAM routines into)
 */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03005700, LENGTH = 0x2200
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

    /* Hot hand-written code runs from IWRAM (copied at boot), like the
     * routines the original game copied there. */
    .iwram : {
        __iwram_start = .;
        *(.iwram .iwram.*)
        . = ALIGN(4);
        __iwram_end = .;
    } > iwram AT > rom
    __iwram_lma = LOADADDR(.iwram);

    .data : { *(.data .data.*) } > iwram AT > rom
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > iwram

    /* Host stacks: main (system mode) below the IRQ stack; measured peaks
     * with tools/stackuse.sh (wander trace): main 456, IRQ 828 bytes. */
    __irq_stack_top = 0x03007900;
    __sys_stack_top = 0x03007300;          /* IRQ stack: 0x600 bytes */
    __sys_stack_limit = 0x03006E00;        /* main stack: 0x500 bytes */
    ASSERT(__bss_end <= __sys_stack_limit, "IWRAM code + runtime .bss too large for the IWRAM block")
    ASSERT(SIZEOF(.data) == 0, "initialized .data is not supported; use const or .bss")

    /DISCARD/ : { *(.comment) *(.note*) }
}
