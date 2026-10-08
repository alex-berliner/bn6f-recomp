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
extern char __sys_stack_top[], __irq_stack_top[];

static void clear32(u32 addr, u32 size) {
    for (u32 i = 0; i < size; i += 4)
        *(volatile u32 *)(addr + i) = 0;
}

/* Call a lifted function the way start.s does (`mov lr, pc; bx r0`). */
static void call_init(GuestFn fn) {
    LR = 0xFFFFFFF0u;
    run(fn);
}

/* GameEntryPoint (start.s, ARM) sets the BIOS IRQ vector and WAITCNT with
 *   080000F4 ldr r0, =0x03007FFC; ldr r1, =sub_3005B00; 080000FC str r1, [r0]
 *   08000100 ldr r0, =WAITCNT;    ldr r1, =0x45B4;      08000108 str r1, [r0]
 * Flashcart firmware patches these in the ROM image, and this port must
 * follow: SuperFW's database replaces the WAITCNT store with a no-op (the
 * Supercard's SDRAM cannot run at 3/1 waitstates) and points the IRQ vector
 * constant at its own slot (0x03007FF4, for its in-game menu); its engine for
 * unknown ROMs patches the constants instead. So each store is done as the
 * (possibly patched) original would: through its constants, if the store
 * instruction is still there. Unpatched, these are the plain registers. */
#define ORIG_STR_R1_R0 0xE5801000u
static void orig_store(u32 str_insn, u32 addr_literal, u32 value) {
    if (*(volatile u32 *)str_insn == ORIG_STR_R1_R0)
        *(volatile u32 *)(uintptr_t)*(volatile u32 *)addr_literal = value;
}

__attribute__((used)) void gba_boot(void) {   /* entered from crt0.c (asm) */
    REG_IME = 0;
    orig_store(0x08000108u, 0x08000200u, *(volatile u32 *)0x08000204u);    /* WAITCNT = 0x45B4 */

    /* Guest-visible RAM, as start_clearMemory does. IWRAM 0x3005700..0x30079D0
     * holds this port's IWRAM code, state and stack and is left alone. */
    clear32(0x03000000, 0x5700);
    clear32((u32)__irq_stack_top, 0x03007E00 - (u32)__irq_stack_top);
    clear32(0x02000000, 0x40000);
    clear32(0x06000000, 0x18000);
    clear32(0x07000000, 0x400);
    clear32(0x05000000, 0x400);

    for (char *p = __iwram_start, *q = __iwram_lma; p < __iwram_end; p++, q++)
        *p = *q;
    for (char *p = __bss_start; p < __bss_end; p++)
        *p = 0;
    /* Stack high-water-mark pattern (everything below this frame); read
     * back by tools/stackuse.sh. */
    {
        u32 here = (u32)__builtin_frame_address(0);
        for (u32 a = ((u32)__bss_end + 3) & ~3u; a < here - 64; a += 4)
            *(volatile u32 *)a = 0xA5A5A5A5u;
        for (u32 a = (u32)__sys_stack_top; a < (u32)__irq_stack_top; a += 4)
            *(volatile u32 *)a = 0x5A5A5A5Au;
    }


    /* Guest CPU state after GameEntryPoint's mode switches. */
    cpu.cpsr_ctl = 0x1F;
    cpu.bank_sp[BANK_IRQ] = 0x03007F60;
    cpu.bank_sp[BANK_SVC] = 0x03007FE0;
    SP = 0x03007E00;   /* iStack */

    orig_store(0x080000FCu, 0x080001F8u, (u32)irq_entry);                  /* BIOS IRQ vector */

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
