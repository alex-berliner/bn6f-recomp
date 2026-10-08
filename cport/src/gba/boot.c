/*
 * boot.c — C version of bn6f's GameEntryPoint (asm/start.s).
 *
 * The original sets the banked stack pointers, clears RAM, copies the IWRAM
 * routines and calls three init functions before jumping to main_. Here the
 * banked stacks belong to the guest register file, the IWRAM routines are
 * lifted C living in ROM (their data tables are read via IWRAM_ROM()), and
 * the IRQ vector points at the native irq_entry().
 */
#include "gba.h"
#include "lifted.h"
#include "runtime.h"

extern char __bss_start[], __bss_end[];
extern char __iwram_start[], __iwram_end[], __iwram_lma[];

static void clear32(u32 addr, u32 size) {
    for (u32 i = 0; i < size; i += 4)
        *(volatile u32 *)(addr + i) = 0;
}

/* Call a lifted function the way start.s does (`mov lr, pc; bx r0`). */
static void call_init(GuestFn fn) {
    LR = 0xFFFFFFF0u;
    run(fn);
}

void gba_boot(void) {
    REG_IME = 0;
    REG_WAITCNT = 0x45B4;

    for (char *p = __iwram_start, *q = __iwram_lma; p < __iwram_end; p++, q++)
        *p = *q;
    for (char *p = __bss_start; p < __bss_end; p++)
        *p = 0;
#if defined(LIFT_TRACE)
    /* Stack high-water-mark pattern (everything below this frame). */
    {
        u32 here = (u32)__builtin_frame_address(0);
        for (u32 a = ((u32)__bss_end + 3) & ~3u; a < here - 64; a += 4)
            *(volatile u32 *)a = 0xA5A5A5A5u;
        for (u32 a = 0x03006A00; a < 0x03007900; a += 4)
            *(volatile u32 *)a = 0x5A5A5A5Au;
    }
#endif

    /* Guest-visible RAM, as start_clearMemory does. IWRAM 0x3005B00..0x3007900
     * holds this runtime's state and stacks and is left alone. */
    clear32(0x03000000, 0x5B00);
    clear32(0x03007900, 0x500);
    clear32(0x02000000, 0x40000);
    clear32(0x06000000, 0x18000);
    clear32(0x07000000, 0x400);
    clear32(0x05000000, 0x400);

    /* Guest CPU state after GameEntryPoint's mode switches. */
    cpu.cpsr_ctl = 0x1F;
    cpu.bank_sp[BANK_IRQ] = 0x03007F60;
    cpu.bank_sp[BANK_SVC] = 0x03007FE0;
    SP = 0x03007E00;   /* iStack */

    BIOS_IRQ_VECTOR = (u32)irq_entry;

    call_init(SetPrimaryToolkitPointers);
    call_init(RandomizeExtraToolkitPointers);
    call_init(start_800023C);

    ST8(0x020081B0u, 0);      /* byte_20081B0 */
    ST32(0x02009930u, 1);     /* dword_2009930 */
    ST32(0x0200A870u, 0);     /* dword_200A870 */
    REG_DISPSTAT = 8;
    REG_KEYCNT = 0x83FF;

    run(main_);
    rt_fatal("main_ returned", 0);
}
