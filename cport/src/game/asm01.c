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

/* ── sub_801FE24 ── */
/* sub_801FE24: (re)start the standard battle-effect overlay (object id 0x10).
 * If battle effects are already active the overlay is restarted with
 * parameter 0; otherwise the link state slot 0 is reset for a new use and
 * the overlay is started with parameter 5. When the overlay has been
 * started, the "overlay active" byte at eStruct203F7D8 is set to 1. */
Next sub_801FE24(void) {
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 8) {
        /* Battle effects already in use: restart the overlay with mode 0. */
        R0 = 0x10;
        R1 = 0;
        R2 = 2;
        R3 = 0;
        GUEST_CALL(sub_803DCE8);
    } else {
        GUEST_CALL(notZero_eByte200AD04);
        if (ZERO_FLAG())
            RETURN_TO_CALLER();

        /* Overlay is free: reset link state slot 0, then start it with mode 5. */
        R0 = 0;
        GUEST_CALL(eLinkState_ResetForNewUse_803DE88);

        R0 = 0x10;
        R1 = 0;
        R2 = 2;
        R3 = 5;
        GUEST_CALL(sub_803DCE8);
    }

    ST8(A_eStruct203F7D8, 1);
    RETURN_TO_CALLER();
}

/* ── battle_getSubstateFinishedFlag_801FE5E ── */
/* battle_getSubstateFinishedFlag_801FE5E: read the byte at eStruct203F7D8.
 * Returns r0 = that byte, r3 = the struct's address. */
Next battle_getSubstateFinishedFlag_801FE5E(void) {
    R3 = A_eStruct203F7D8;
    R0 = LD8(R3);
    RETURN_TO_CALLER();
}

/* ── battle_updateCustomGauge_801FE6C ── */
/* Register-form helpers: each calls a game function outside this file which has no typed C
 * interface yet. They put arguments in R0.., call the function, and return the result. */

static bool test_event_flag(u16 flag) {
    /* Register form: calls TestEventFlagFromImmediate outside this file, which has no typed C
     * interface yet. The original loads r0 = 0x17 (section) then r1 = flag as the flag id. */
    R0 = 0x17;
    R1 = flag;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* Z set => flag is clear */
}

static bool battle_effects_has_8(void) {
    /* Register form: calls GetBattleEffects outside this file, which has no typed C interface
     * yet. Mirrors `GetBattleEffects; mov r1, #8; tst r0, r1`. */
    GUEST_CALL(GetBattleEffects);
    R1 = 8;
    return ((R0 & R1) != 0);
}

static u32 sub_8144D04_call(u32 arg) {
    /* Register form: calls battle_getTimerEnableFlag outside this file, which has no typed C interface yet. */
    R0 = arg;
    GUEST_CALL(battle_getTimerEnableFlag);
    return R0;
}

static u8 sub_803DEB4_call(void) {
    /* Register form: calls battle_updateLinkState_803DEB4 outside this file, which has no typed C interface yet.
     * Returns the chosen step byte (the original stores r0 as a byte). */
    GUEST_CALL(battle_updateLinkState_803DEB4);
    return (u8)R0;
}

static void copy_words(u32 src, u32 dst, u32 size) {
    /* Register form: calls CopyWords outside this file, which has no typed C interface yet. */
    R0 = src;
    R1 = dst;
    R2 = size;
    GUEST_CALL(CopyWords);
}

/* Reads/writes the small struct at eStruct203F7D8 used by this routine. */
static u8  e_state_byte0(void)      { return LD8(A_eStruct203F7D8); }
static u8  e_state_byte1(void)      { return LD8(A_eStruct203F7D8 + 1); }
static void e_state_set_byte1(u8 v) { ST8(A_eStruct203F7D8 + 1, v); }

enum {
    E_STEP_DEFAULT = 2,            /* not busy; nothing decided yet */
    E_STEP_BUSY    = 4,            /* a transition step is in flight */
};

/* One step of the bus/deck transition driver. Saves the previous step byte, force-sets the
 * state to E_STEP_DEFAULT, then either kicks off a transition or copies fresh defaults
 * depending on the current day-1 flag and the current battle-effects mask. */
static void do_step(u8 prev_step) {
    e_state_set_byte1(E_STEP_DEFAULT);

    if (e_state_byte0() == 0) {
        /* First-time init: copy the two default 0x10-byte blobs into the per-step buffers. */
        copy_words(A_eStruct2036780, A_unk_20399F0, 0x10);
        copy_words(A_eStruct2036780, A_unk_2039A00, 0x10);
        return;
    }

    if (!test_event_flag(0x172D)) {
        /* Event 0x172D not set: pick a new step based on whether bit 0x8 of the current
         * battle-effects word is clear. When clear, run battle_updateLinkState_803DEB4 to choose the step;
         * when set, restore whatever was saved (the original redundantly reloads the byte). */
        if (battle_effects_has_8()) {
            e_state_set_byte1(prev_step);
            (void)e_state_byte1();                   /* mirrored load after the store */
        } else {
            e_state_set_byte1(sub_803DEB4_call());
        }
        return;
    }

    /* Event 0x172D set path. Restore the previous step first, then check the new value. */
    e_state_set_byte1(prev_step);
    if (e_state_byte1() != E_STEP_DEFAULT)
        return;

    /* Bit 0x8 of the current battle-effects word must be set to enter the questioning
     * sequence below. */
    if (!battle_effects_has_8())
        return;

    /* Two-step handshake with battle_getTimerEnableFlag: ask with arg 0, then with arg 3. If both
     * answer "1" we leave the step alone; otherwise commit step 4 (E_STEP_BUSY). */
    if (sub_8144D04_call(0) == 1 && sub_8144D04_call(3) == 1)
        return;
    e_state_set_byte1(E_STEP_BUSY);
}

Next battle_updateCustomGauge_801FE6C(void) {
    u8 prev_step = e_state_byte1();
    do_step(prev_step);
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

/* ── battle_requestSetBattleMode_80200A4 ── */
/* Set the "value" byte of eStruct203F7D8 and OR the "flags" byte with 1.
 * Returns 1 in r1, leaves r3 pointing at eStruct203F7D8. */
static void set_struct_byte_and_flag(u8 value) {
    u32 base = A_eStruct203F7D8;
    ST8(base + 3, value);
    ST8(base + 2, LD8(base + 2) | 1);
}

Next battle_requestSetBattleMode_80200A4(void) {
    set_struct_byte_and_flag((u8)R0);
    R1 = 1;
    R3 = A_eStruct203F7D8;
    RETURN_TO_CALLER();
}

/* ── eStruct2038160_clearStruct ── */
/* eStruct2038160_clearStruct: zero out the 4-byte battle object struct
 * at eStruct2038160 (r2, r3 are preserved across the call). */
Next eStruct2038160_clearStruct(void) {
    R0 = A_eStruct2038160; /* =eStruct2038160 */
    R1 = sizeof(u32);
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── eStruct2038160_getBattleTerminate01 ── */
/* eStruct2038160_getBattleTerminate01: read the BattleTerminate01 flag (byte offset 1)
 * from the global eStruct2038160 struct and return it in r0.
 *   inputs:  lr
 *   outputs: r0
 */
Next eStruct2038160_getBattleTerminate01(void) {
    R0 = LD8(A_eStruct2038160 + (A_eStruct2038160_BattleTerminate01 - 0x2038160));
    RETURN_TO_CALLER();
}
