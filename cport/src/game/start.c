/*
 * start.c: readable functions of bn6f start.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── entryRomInit_800023C ── */
/* register form: calls sub_3005DA0 outside this file, which has no typed C interface yet.
 * The original pops the saved caller-LR into r0 and bx's to it, so the return value
 * that matters is the caller's LR, not whatever sub_3005DA0 left in r0. */
static u32 call_rom_init(u32 caller_lr) {
    GUEST_CALL(sub_3005DA0);
    return caller_lr;  /* original: pop {r0} gets the saved LR, then bx r0 returns to caller */
}

/* GBA ROM entry stub: invoke the main initialization routine sub_3005DA0,
 * then return to the BIOS/caller. r0 on exit carries the caller's LR. */
Next entryRomInit_800023C(void) {
    R0 = call_rom_init(LR);
    RETURN_TO_CALLER();
}

/* ── SetInterruptCallback ── */
/* SetInterruptCallback(r0 = interrupt index, r1 = handler):
 * Installs `handler` as the callback for interrupt number `interrupt index`.
 * All the real work is done by the crt0 helper _SetInterruptCallback, which
 * reads the index from R0 and the handler pointer from R1 (both already in
 * place from our caller) and stores the pointer into the interrupt dispatch
 * table. The original's `mov lr, pc / bx r2` round trip is just the call
 * linkage to that helper, and its `push {lr} / pop {r0} / bx r0` is the
 * return to the caller, both of which the guest call/return machinery
 * handles here.
 *
 * On exit R2 and R3 hold whatever the helper left in them, R0 is scratched,
 * and R4 passes through untouched. */
Next SetInterruptCallback(void) {
    GUEST_CALL(_SetInterruptCallback);

    RETURN_TO_CALLER();
}

/* ── crt0_start_800027C ── */
/* crt0_start_800027C: start.s trampoline (0800027C).
 *
 * The original code is the standard ARMv4 long-call idiom:
 *   push {lr}              -- remember our caller's return address
 *   ldr r1, =sub_3005E18+1 -- Thumb entry point of the routine
 *   mov lr, pc             -- return address = the `pop {r0}` below
 *   bx r1                  -- call it (no BLX on ARM7TDMI)
 *   pop {r0}               -- execution comes back here...
 *   bx r0                  -- ...and returns to our caller
 *
 * In other words: run sub_3005E18 as a plain subroutine, then return
 * normally. No arguments are passed and no result is produced; R4 and
 * every other register the callee doesn't own must survive untouched.
 */
Next crt0_start_800027C(void) {
    GUEST_CALL(sub_3005E18);
    RETURN_TO_CALLER();
}

/* ── crt0_entry_trampoline_8000288 ── */
/* crt0_entry_trampoline_8000288: trampoline that invokes sub_3005E2C as a leaf call and
 * returns straight back to our own caller. sub_3005E2C's results in
 * r2/r3 are passed through unchanged; r4 is untouched. */
Next crt0_entry_trampoline_8000288(void) {
    GUEST_CALL(sub_3005E2C);
    RETURN_TO_CALLER();
}
