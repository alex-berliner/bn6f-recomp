/*
 * asm23.c: readable functions of bn6f asm23.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battleObject_scenarioEffectInputHandler_8089704 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5, r6; } call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInputHandler_8089704;
/* register form: generated wrapper: battle_getScenarioEffectStateField_81421D0 has no typed C interface yet */
static inline call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInputHandler_8089704 call_battle_getScenarioEffectStateField_81421D0_battleObject_scenarioEffectInputHandler_8089704(void) {
    GUEST_CALL(battle_getScenarioEffectStateField_81421D0);
    return (call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInputHandler_8089704){ R0, R4, R5, R6 };
}

typedef struct { u32 r0, r4, r5, r6, r7; bool z, n; } call_battle_checkScenarioEffectActive_8142190_result;
/* register form: generated wrapper: battle_checkScenarioEffectActive_8142190 has no typed C interface yet */
static inline call_battle_checkScenarioEffectActive_8142190_result call_battle_checkScenarioEffectActive_8142190(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_checkScenarioEffectActive_8142190);
    return (call_battle_checkScenarioEffectActive_8142190_result){ R0, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3, r4; bool z, n; } call_sub_8142A84_result;
/* register form: generated wrapper: sub_8142A84 has no typed C interface yet */
static inline call_sub_8142A84_result call_sub_8142A84(u32 r0) {
    R0 = r0;
    GUEST_CALL(sub_8142A84);
    return (call_sub_8142A84_result){ R2, R3, R4, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_battleObject_scenarioEffectInputHandler_8089704;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battleObject_scenarioEffectInputHandler_8089704 call_PlaySoundEffect_battleObject_scenarioEffectInputHandler_8089704(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battleObject_scenarioEffectInputHandler_8089704){ R1, R2, R3, R4, R5, R6, R7, R10 };
}

/* register form: generated wrapper: e_setScenarioEffectStateB has no typed C interface yet */
static inline void call_e_setScenarioEffectStateB(u32 r0) {
    R0 = r0;
    GUEST_CALL(e_setScenarioEffectStateB);
}

/* Handle one frame of the scenario-effect selection input during battle.
 *
 * Reads the joypad held-button word (offset +2) and shoulder word (offset +4) from
 * the toolkit's joypad pointer.  Dispatches:
 *   A (0x1):  attempt to advance the effect sub-state; on failure play CANT_JACK_IN,
 *             on success play SELECT; sub_8142A84 gates whether the advance is allowed.
 *   B (0x2):  play UNSELECT.
 *   L (0x40): play 0x66, cycle the effect state backward (0→3 wrap).
 *   R (0x80): play UNK_66, cycle the effect state forward (3→0 wrap).
 *
 * Returns an action code: 0 = nothing / blocked, 1 = shoulder cycled,
 * 2 = A advanced, 3 = B unselected.  The caller tests Z on the return. */
static u32 handle_scenario_effect_input(u32 toolkit, u32 r2) {
    u32 joypad = LD32(toolkit + oToolkit_JoypadPtr);
    u16 held   = LD16(joypad + 2);

    /* A button */
    if (held & 1) {
        u32 state = call_battle_getScenarioEffectStateField_81421D0_battleObject_scenarioEffectInputHandler_8089704().r0;
        call_battle_checkScenarioEffectActive_8142190_result act =
            call_battle_checkScenarioEffectActive_8142190(state);

        if (act.z) {
            /* No active effect to jack into */
            call_PlaySoundEffect_battleObject_scenarioEffectInputHandler_8089704(SOUND_CANT_JACK_IN, r2);
            return 0;
        }

        call_sub_8142A84_result gate = call_sub_8142A84(act.r0);
        if (gate.z) {
            /* Advance not allowed this frame */
            return 0;
        }

        call_PlaySoundEffect_battleObject_scenarioEffectInputHandler_8089704(SOUND_SELECT_67, gate.r2);
        return 2;
    }

    /* B button */
    if (held & 2) {
        call_PlaySoundEffect_battleObject_scenarioEffectInputHandler_8089704(SOUND_UNSELECT_68, r2);
        return 3;
    }

    /* Shoulder buttons (separate joypad word at offset +4) */
    u16 shoulder = LD16(joypad + 4);

    if (shoulder & 0x40) {
        /* L: cycle backward with wrap 0→3 */
        call_PlaySoundEffect_battleObject_scenarioEffectInputHandler_8089704(0x66, r2);
        u32 cur = call_battle_getScenarioEffectStateField_81421D0_battleObject_scenarioEffectInputHandler_8089704().r0;
        s32 dec = (s32)cur - 1;
        if (dec < 0)
            dec = 3;
        call_e_setScenarioEffectStateB((u32)dec);
        return 1;
    }

    if (shoulder & 0x80) {
        /* R: cycle forward with wrap 3→0 */
        call_PlaySoundEffect_battleObject_scenarioEffectInputHandler_8089704(SOUND_UNK_66, r2);
        u32 cur = call_battle_getScenarioEffectStateField_81421D0_battleObject_scenarioEffectInputHandler_8089704().r0;
        u32 inc = cur + 1;
        if ((s32)inc >= 4)
            inc = 0;
        call_e_setScenarioEffectStateB(inc);
        return 1;
    }

    return 0;
}

/* battleObject_scenarioEffectInputHandler_8089704
 * (r2 = sound param, r10 = toolkit ptr, lr = return addr) -> r0, nz
 *  r2 r3 r5 r7 r10 outputs are left by callee wrappers. */
Next battleObject_scenarioEffectInputHandler_8089704(void) {
    R0 = handle_scenario_effect_input(R10, R2);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

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

/* ── beastOverworldAi_decideTransformOrIdle_8089C16 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r3; } call_GetCurPETNavi_result_beastOverworldAi_decideTransformOrIdle_8089C16;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result_beastOverworldAi_decideTransformOrIdle_8089C16 call_GetCurPETNavi_beastOverworldAi_decideTransformOrIdle_8089C16(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result_beastOverworldAi_decideTransformOrIdle_8089C16){ R0, R3 };
}

typedef struct { u32 r0, r1, r3, r4, r5, r6, r7; bool c, v; } call_GetMaxAndCurHPForCurPETNavi_80010D4_result;
/* register form: generated wrapper: GetMaxAndCurHPForCurPETNavi_80010D4 has no typed C interface yet */
static inline call_GetMaxAndCurHPForCurPETNavi_80010D4_result call_GetMaxAndCurHPForCurPETNavi_80010D4(u32 r0) {
    R0 = r0;
    GUEST_CALL(GetMaxAndCurHPForCurPETNavi_80010D4);
    return (call_GetMaxAndCurHPForCurPETNavi_80010D4_result){ R0, R1, R3, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r3; bool c, v; } call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_beastOverworldAi_decideTransformOrIdle_8089C16;
/* register form: generated wrapper: owPlayer_setS200ace0_fixedAnimationSelect_809e13c has no typed C interface yet */
static inline call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_beastOverworldAi_decideTransformOrIdle_8089C16 call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_beastOverworldAi_decideTransformOrIdle_8089C16(u32 r0) {
    R0 = r0;
    GUEST_CALL(owPlayer_setS200ace0_fixedAnimationSelect_809e13c);
    return (call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_beastOverworldAi_decideTransformOrIdle_8089C16){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: ai_setAIDataUnk18_8089CB0 has no typed C interface yet */
static inline void call_ai_setAIDataUnk18_8089CB0(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(ai_setAIDataUnk18_8089CB0);
}

/* register form: generated wrapper: GetOWPlayerFacingDirection has no typed C interface yet */
static inline u32 call_GetOWPlayerFacingDirection(void) {
    GUEST_CALL(GetOWPlayerFacingDirection);
    return R0;
}

typedef struct { u32 r4, r5, r7; bool z, n; } call_battleScenarioEffect_isActive_814216E_result;
/* register form: generated wrapper: battleScenarioEffect_isActive_814216E has no typed C interface yet */
static inline call_battleScenarioEffect_isActive_814216E_result call_battleScenarioEffect_isActive_814216E(void) {
    GUEST_CALL(battleScenarioEffect_isActive_814216E);
    return (call_battleScenarioEffect_isActive_814216E_result){ R4, R5, R7, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r5, r7; bool z, n; } call_e_scenarioEffectStateCountersAnySet_result;
/* register form: generated wrapper: e_scenarioEffectStateCountersAnySet has no typed C interface yet */
static inline call_e_scenarioEffectStateCountersAnySet_result call_e_scenarioEffectStateCountersAnySet(void) {
    GUEST_CALL(e_scenarioEffectStateCountersAnySet);
    return (call_e_scenarioEffectStateCountersAnySet_result){ R5, R7, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* beastOverworldAi_decideTransformOrIdle_8089C16
 *
 * Gate byte at +0x13: if already set, skip entirely.
 * Otherwise marks +0x08, checks the current PET-Navi's HP:
 *   HP == 0 → KO: clears the mark, plays KO animation for navi 0, sets state 0x84.
 *   HP != 0 → hurt: plays facing-direction animation, then checks battle scenario
 *              effects and counters to decide between transform (0x7C) and idle (0x1C).
 * Always returns 1 (caller uses this as "reaction handled" flag).
 */
static void beastOverworldAi_decideTransformOrIdle_impl(u32 player_obj) {
    /* If the overworld gate at +0x13 is already set, nothing to do */
    if (LD8(player_obj + 0x13) != 0)
        return;

    /* Mark that a reaction is being handled */
    ST8(player_obj + 8, 1);

    u32 navi = call_GetCurPETNavi_beastOverworldAi_decideTransformOrIdle_8089C16().r0;
    u32 hp = call_GetMaxAndCurHPForCurPETNavi_80010D4(navi).r0;

    if (hp == 0) {
        /* PET-Navi is KO'd */
        ST8(player_obj + 8, 0);  /* r0 == 0 here, matching original strb */

        navi = call_GetCurPETNavi_beastOverworldAi_decideTransformOrIdle_8089C16().r0;
        if (navi == 0)
            call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_beastOverworldAi_decideTransformOrIdle_8089C16(0x3A);

        call_ai_setAIDataUnk18_8089CB0(0x1E, player_obj);
        ST8(player_obj, 0x84);   /* KO state */
    } else {
        /* PET-Navi is alive / hurt */
        u32 facing = call_GetOWPlayerFacingDirection();
        call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_beastOverworldAi_decideTransformOrIdle_8089C16(facing);
        call_ai_setAIDataUnk18_8089CB0(0x1E, player_obj);

        call_battleScenarioEffect_isActive_814216E_result eff =
            call_battleScenarioEffect_isActive_814216E();

        if (eff.z) {
            /* Scenario effect is not active → go idle */
            call_ai_setAIDataUnk18_8089CB0(0, player_obj);
            ST8(player_obj, 0x1C);
        } else {
            call_e_scenarioEffectStateCountersAnySet_result cnt =
                call_e_scenarioEffectStateCountersAnySet();

            if (!cnt.z) {
                /* Counters are set → go idle */
                call_ai_setAIDataUnk18_8089CB0(0, player_obj);
                ST8(player_obj, 0x1C);
            } else {
                /* All clear → Beast-overworld transform */
                ST8(player_obj, 0x7C);
            }
        }
    }
}

/* beastOverworldAi_decideTransformOrIdle_8089C16(r5 = player_obj, r10) -> r0 */
Next beastOverworldAi_decideTransformOrIdle_8089C16(void) {
    beastOverworldAi_decideTransformOrIdle_impl(R5);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
