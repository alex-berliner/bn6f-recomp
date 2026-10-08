/* Linker script for the lifted BN6 GBA ROM.
 *
 * 0x08000000  original bn6f.gba image (data at its original addresses; the
 *             original code bytes are no longer executed)
 * 0x08800000  lifted C code, runtime, dispatch table
 * IWRAM 0x03005700-0x030079D0: port IWRAM code, runtime state and the host
 *             stack, in the block the original copied SoundMainRAM and its
 *             IWRAM routines into (they run from C instead; see m4a_init.c).
 */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03005700, LENGTH = 0x22D0
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

    /* Host stack, shared by the main loop and interrupt handlers (irq.c),
     * from the end of .bss up to just below the guest's own stack. The
     * original's IWRAM routines end at 0x030079D4 (0x03005B00 + 0x1ED4) and
     * its stack (iStack, 0x03007E00 down) never grows into them, so the
     * guest never uses anything below that. Above the host stack: the BIOS's
     * IRQ entry frame (24 bytes, IRQ mode). tools/stackuse.sh measures the
     * peak (1372 bytes in the battle trace); the minimum below is 1.3x that. */
    __irq_stack_top = 0x030079D0;
    __sys_stack_top = 0x030079B0;
    __sys_stack_limit = __sys_stack_top - 0x700;
    ASSERT(__bss_end <= __sys_stack_limit, "IWRAM code + runtime .bss leave the host stack too small")
    ASSERT(SIZEOF(.data) == 0, "initialized .data is not supported; use const or .bss")

    /DISCARD/ : { *(.comment) *(.note*) }
}
