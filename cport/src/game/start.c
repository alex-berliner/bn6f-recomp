/*
 * start.c: readable functions of bn6f start.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── start_clearMemory ── */
/* Zero-fill memory in 4-byte words, walking backwards from the end of the
 * range: for each step, decrement the byte count by 4 and clear the word at
 * dst + count. The count must be a multiple of 4. r1 and r2 end clobbered
 * (the count is consumed; r2 is the zero constant), flags end Z-set. */

static void clear_memory(u32 dst, u32 byte_count) {
    while (byte_count != 0) {
        byte_count -= 4;
        ST32(dst + byte_count, 0);
    }
}

/* start_clearMemory(r0 = dst, r1 = byte count) */
Next start_clearMemory(void) {
    clear_memory(R0, R1);
    RETURN_TO_CALLER();
}

/* ── start_copyMemory ── */
/* start_copyMemory copies 'size' bytes (r2) from src (r0) to dst (r1),
 * one word at a time, walking backwards from the end of the buffers.
 * Each iteration pre-decrements r2 by 4, copies the word at that offset,
 * and loops until the subtraction yields zero. The Z/C/V flags the
 * callers see are those of that final SUBS (r2 was 4, so Z set, C set,
 * V clear); r2 ends at 0 and r3 holds the last word copied. */

/* Result of the copy: the value r2 held before the final subtraction
 * (so the adapter can set the flags exactly as that SUBS did), the
 * final r2, and the last word read into r3. */
struct start_copyMemory_result {
    u32 size_before_last_sub;
    u32 final_r2;
    u32 last_word;
};

static struct start_copyMemory_result copy_memory_backwards(u32 src, u32 dst, u32 size) {
    u32 size_before_last_sub = size;
    u32 last_word = 0;

    do {
        size_before_last_sub = size;
        size -= 4;
        last_word = LD32(src + size);
        ST32(dst + size, last_word);
    } while (size != 0);

    struct start_copyMemory_result result;
    result.size_before_last_sub = size_before_last_sub;
    result.final_r2 = size;
    result.last_word = last_word;
    return result;
}

/* inputs: r0 = src, r1 = dst, r2 = size (bytes), lr = return address
 * outputs: nz c v from the final SUBS, r2 = 0, r3 = last word copied */
Next start_copyMemory(void) {
    struct start_copyMemory_result result = copy_memory_backwards(R0, R1, R2);
    R2 = result.final_r2;
    R3 = result.last_word;
    SET_FLAGS_CMP(result.size_before_last_sub, 4);
    RETURN_TO_CALLER();
}

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
/* crt0_entry_trampoline_8000288: trampoline that invokes init_installCommIntrHandlers as a leaf call and
 * returns straight back to our own caller. init_installCommIntrHandlers's results in
 * r2/r3 are passed through unchanged; r4 is untouched. */
Next crt0_entry_trampoline_8000288(void) {
    GUEST_CALL(init_installCommIntrHandlers);
    RETURN_TO_CALLER();
}
