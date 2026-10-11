/*
 * asm01.c: readable functions of bn6f asm01.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── chatbox_resetPrinterState_801FE00 ── */
/* chatbox_resetPrinterState_801FE00: reset/clear the text-printer state used by message boxes.
 * Zeroes the printer's word-sized state, its halfword flags, marks its
 * current character slot as unused (0xFF), and clears its two buffers.
 * Leaves r2 untouched; r3 ends pointing at the printer state struct. */
Next chatbox_resetPrinterState_801FE00(void) {
    /* clear the pending-word/state field */
    ST32(A_eStruct203F7D8, 0);

    /* clear the printer flags and mark the current char slot empty */
    ST16(A_eStruct2036780 + 2, 0);   /* eFlags2036782 */
    ST8(A_eStruct2036780 + 4, 0xFF); /* byte_2036784  */

    /* keep the struct pointer live in r3, as the original does */
    R3 = A_eStruct2036780;

    /* clear the 0x200-byte text buffer ... */
    R0 = A_dword_203F4A0;
    R1 = 0x200;
    GUEST_CALL(ZeroFillByEightWords);

    /* ... and the 0x100-byte secondary buffer */
    R0 = A_dword_203CBE0;
    R1 = 0x100;
    GUEST_CALL(ZeroFillByEightWords);

    RETURN_TO_CALLER();
}

/* ── sub_801FE5E ── */
/* sub_801FE5E: read the byte at eStruct203F7D8.
 * Returns r0 = that byte, r3 = the struct's address. */
Next sub_801FE5E(void) {
    R3 = A_eStruct203F7D8;
    R0 = LD8(R3);
    RETURN_TO_CALLER();
}

/* ── eStruct203F7D8_getUnk01 ── */
/* eStruct203F7D8_getUnk01: return the byte at eStruct203F7D8+1. */
Next eStruct203F7D8_getUnk01(void) {
    R0 = LD8(A_eStruct203F7D8 + 1);
    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_dispatchStateHandler_801FEEE ── */
/* eStruct200BC30_dispatchStateHandler_801FEEE(r0 = handler argument):
 * State-machine dispatcher for eStruct200BC30. Asks the struct which jump-table
 * slot is active (eStruct200BC30_getJumpOffset00), fetches the handler address
 * from off_801FF08, and calls it with the original argument restored in r0
 * (and the table base in r1 / the table entry in r2, as the original does).
 * The table holds only two distinct handlers, alternating A,B,B,A, so the
 * dispatch reduces to a two-way call. r4-r7 are pushed/popped around the call
 * in the original, so they are preserved here even though the callees may
 * clobber them; r8-r12 and lr flow through to the handler untouched.
 * Flags (c, v) are whatever the handler leaves behind. */
Next eStruct200BC30_dispatchStateHandler_801FEEE(void) {
    u32 arg = R0;
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    /* mov r7, r0: stash the argument across the offset query */
    R7 = arg;

    GUEST_CALL(eStruct200BC30_getJumpOffset00);

    /* ldr r1, =off_801FF08; ldr r2, [r1, r0]: pick the handler entry */
    R1 = A_off_801FF08;
    R2 = LD32(R1 + R0);
    R0 = R7; /* mov r0, r7: hand the argument to the handler */

    /* bx r2: the entry is either sub_801FF18 (slots 0 and 3)
     * or sub_801FFD6 (slots 1 and 2) */
    if (R2 == LD32(A_off_801FF08 + 0) || R2 == LD32(A_off_801FF08 + 12))
        GUEST_CALL(sub_801FF18);
    else
        GUEST_CALL(sub_801FFD6);

    /* pop {r4-r7,pc}: restore the saved registers for the caller */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── eStruct2038160_clearStruct ── */
/* eStruct2038160_clearStruct: zero out the 4-byte battle object struct
 * at eStruct2038160 (r2, r3 are preserved across the call). */
Next eStruct2038160_clearStruct(void) {
    R0 = A_eT4BattleObjectsEnd; /* =eStruct2038160 */
    R1 = sizeof(u32);
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}
