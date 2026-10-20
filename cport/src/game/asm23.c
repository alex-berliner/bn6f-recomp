/*
 * asm23.c: readable functions of bn6f asm23.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── ai_startChatScriptAtScenarioState2_8089792 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5, r6, r7; } call_sub_81421D0_result;
/* register form: generated wrapper: battle_getScenarioEffectStateField_81421D0 has no typed C interface yet */
static inline call_sub_81421D0_result call_sub_81421D0(void) {
    GUEST_CALL(battle_getScenarioEffectStateField_81421D0);
    return (call_sub_81421D0_result){ R0, R4, R5, R6, R7 };
}

typedef struct { u32 r1, r2, r3, r5; bool c, v; } call_sub_8089CA4_result_sub_8089792;
/* register form: generated wrapper: ai_runChatScript has no typed C interface yet */
static inline call_sub_8089CA4_result_sub_8089792 call_sub_8089CA4_sub_8089792(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(ai_runChatScript);
    return (call_sub_8089CA4_result_sub_8089792){ R1, R2, R3, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* ai_startChatScriptAtScenarioState2_8089792 -- runs battle_getScenarioEffectStateField_81421D0, passes its result (+2) and its r5 to
 * ai_runChatScript, then stores the marker byte 0x14 at the address ai_runChatScript
 * leaves in r5. Returns 1 in r0; r2/r3 are simply whatever ai_runChatScript left
 * behind. r4-r7 must be preserved: battle_getScenarioEffectStateField_81421D0 clobbers them, and the
 * original pushes/pops them around the calls, so the entry values are handed
 * through and written back by the adapter. */
typedef struct {
    u32 r0;
    u32 r4, r5, r6, r7;
} sub_8089792_out;

static sub_8089792_out sub_8089792_body(u32 r4, u32 r5, u32 r6, u32 r7)
{
    /* First helper produces a value in r0 and the r5 the second one wants. */
    call_sub_81421D0_result setup = call_sub_81421D0();

    /* Second helper resolves the target (r0 = setup value + 2, r5 = setup r5;
     * r10 passes through untouched inside the wrapper). */
    call_sub_8089CA4_result_sub_8089792 target = call_sub_8089CA4_sub_8089792(setup.r0 + 2, setup.r5);

    /* Store the marker byte 0x14 at the address the helper returned in r5. */
    ST8(target.r5, 0x14);

    sub_8089792_out out;
    out.r0 = 1;                 /* the function's return value */
    out.r4 = r4;                /* preserved saved registers */
    out.r5 = r5;
    out.r6 = r6;
    out.r7 = r7;
    return out;
}

/* inputs: r5 r10 lr; outputs: r0 r2 r3; preserved: r4-r7.
 * r2/r3 are left in place by the call_sub_8089CA4_sub_8089792 wrapper, so the adapter
 * does not touch them; it only writes r0 and restores r4-r7. */
Next ai_startChatScriptAtScenarioState2_8089792(void) {
    sub_8089792_out out = sub_8089792_body(R4, R5, R6, R7);
    R0 = out.r0;
    R4 = out.r4;
    R5 = out.r5;
    R6 = out.r6;
    R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── ai_chatScriptTickStoreState38_808989C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n, c, v; } call_sub_8089CB4_result;
/* register form: generated wrapper: battle_tickChipLockout_8089CB4 has no typed C interface yet */
static inline call_sub_8089CB4_result call_sub_8089CB4(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_tickChipLockout_8089CB4);
    return (call_sub_8089CB4_result){ ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3, r5; bool c, v; } call_sub_8089CA4_result_sub_808989C;
/* register form: generated wrapper: ai_runChatScript has no typed C interface yet */
static inline call_sub_8089CA4_result_sub_808989C call_sub_8089CA4_sub_808989C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(ai_runChatScript);
    return (call_sub_8089CA4_result_sub_808989C){ R1, R2, R3, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * ai_chatScriptTickStoreState38_808989C — conditional update of the object pointed to by r5.
 *
 * First battle_tickChipLockout_8089CB4 tests the object (r10 carries the surrounding context).
 * If it reports "nothing to do" (Z set) the routine stops there. Otherwise
 * it runs ai_runChatScript with kind 7, which leaves its results in r1/r2/r3
 * and may adjust the object pointer in r5, then stores 0x38 at that
 * (possibly updated) pointer. Both paths return r0 = 1; r5 itself is
 * preserved to the caller (the original saves and restores it).
 */

/* Body: run the kind-7 update and the 0x38 store unless the check says skip. */
static void sub_808989C_body(u32 r5) {
    if (call_sub_8089CB4(r5).z)
        return;                                 /* check failed: leave the object alone */

    /* ai_runChatScript leaves r1/r2/r3 (and its updated r5) behind for the caller;
     * the guest registers already hold them after the wrapper call. */
    call_sub_8089CA4_result_sub_808989C res = call_sub_8089CA4_sub_808989C(7, r5);
    ST8(res.r5, 0x38);
}

/* ai_chatScriptTickStoreState38_808989C(r5 = object ptr, r10 = context) -> r0 = 1, r2/r3 from ai_runChatScript when run */
Next ai_chatScriptTickStoreState38_808989C(void) {
    u32 r5 = R5;
    sub_808989C_body(r5);
    R5 = r5;        /* the original's pop restores r5, which ai_runChatScript may have changed */
    R0 = 1;
    RETURN_TO_CALLER();
}
