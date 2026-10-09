/*
 * leaves_farm_18.c: small functions from the model farm, set 18.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

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

/* ── startScreen_printTextIfFlag3_802FBFC ── */
/* startScreen_printTextIfFlag3_802FBFC(r5 = start screen): if the start screen's state field has
 * bit 3 set, hand the text descriptor table at byte_802FC14 to the
 * text printer helper sub_8001158. r2/r3, the c and v flags (and r5-r7)
 * are whatever sub_8001158 leaves behind when it runs. */
Next startScreen_printTextIfFlag3_802FBFC(void) {
    if (LD16(R5 + 0xA) & 8) {
        R0 = A_byte_802FC14;
        GUEST_CALL(sub_8001158); /* () -> void */
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_playFlaggedSfx_802FC70 ── */
/* startScreen_playFlaggedSfx_802FC70: start-screen per-frame helper (r5 = StartScreen state block).
 * If bit 0x100 of the halfword at +0x0A is set, trigger the effect whose
 * parameter table is byte_802FC88 (passed to sub_8001158 in r0).
 *
 * Flag behaviour of the original: the "lsl r1, r1, #4" used to build the
 * test mask (0x10 -> 0x100) is a Thumb shifted-move, which clears C (bit 28
 * of 0x10 shifts out) and leaves V alone; the subsequent TST only updates
 * N/Z. So on the early-out path C is always 0, V is untouched, and N/Z come
 * from (state & 0x100). On the call path the flags are whatever
 * sub_8001158 leaves behind. */
Next startScreen_playFlaggedSfx_802FC70(void) {
    u32 state = LD16(R5 + 0xA);

    /* Emulate the flag effect of the mask setup: LSL r1, r1, #4 cleared C. */
    cpu.c = 0;

    if (state & 0x100) {
        /* r0 = sound/effect parameter table, run it. */
        R0 = A_byte_802FC88;
        GUEST_CALL(sub_8001158);
    } else {
        /* TST r0, r1 result (0 or 0x100, so N clear; Z per the result). */
        SET_FLAGS_NZ(state & 0x100);
    }

    RETURN_TO_CALLER();
}
