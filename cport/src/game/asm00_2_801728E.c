/*
 * asm00_2_801728E.c: readable functions of bn6f asm00_2.s, 0x0801728E-0x0801B9E6, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_applyDimmerShaderTo_801728E ── */
/* battle_applyDimmerShaderTo_801728E: Apply a color shading effect to the sprite of the current acting
 * battle object, if the relevant "hit flash" conditions are met. */
Next battle_applyDimmerShaderTo_801728E(void) {
    /* Bail out unless the "0x40" battle flag is set (e.g. enemy being hit). */
    GUEST_CALL(battle_testFlag0x40_800A8F8);
    if (R0 == 0) goto done;

    /* Need an active action on this battle object. */
    if (LD8(R5 + oBattleObject_CurAction) == 0) goto done;

    /* Look up the opposing entry and read its 0x50 status byte. */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);
    if (LD8(R0 + 0x50) == 0) goto done;

    /* Compute the shader palette from the battle state "unk40" timer.
     * The lookup table maps a 0..0x1F tick value to a brightness index. */
    GUEST_CALL(battle_getStateUnk40_800A704);
    u32 tick = R0 & 0x1F;
    u8  shadeIdx = LD8(A_byte_80172C8 + tick);

    /* Convert shade index (0..0x1E) into a packed u32: shadeIdx * 0x21,
     * which fits the (R, G, B=0) shader format sprite_setColorShader expects. */
    R0 = (u32)shadeIdx * 0x21;
    GUEST_CALL(sprite_setColorShader);

done:
    RETURN_TO_CALLER();
}

/* ── sub_80173F4 ── */
/* Phase handler addresses as stored in the original's jump table at off_801740C.
 * The table is indexed by the raw CurPhase byte, so the phases count 0, 4, 8, 0xC;
 * the stored values carry the Thumb bit. */
#define PHASE_HANDLER_00 0x0801741Du /* battle_destroyBattleObject_801741C + 1 */
#define PHASE_HANDLER_04 0x0801746Fu /* sub_801746E + 1 */
#define PHASE_HANDLER_08 0x080174ABu /* battle_objectTickTimerAndSetPhaseC_80174AA + 1 */
#define PHASE_HANDLER_0C 0x080174BFu /* battle_tickIntroState_80174BE + 1 */

enum {
    PHASE_00 = 0x0,
    PHASE_04 = 0x4,
    PHASE_08 = 0x8,
    PHASE_0C = 0xC,
};

/* register form: calls sprite_forceWhitePalette outside this file, which has no typed C interface yet */
static void force_white_palette(void) {
    GUEST_CALL(sprite_forceWhitePalette);
}

/* register form: calls battle_destroyBattleObject_801741C outside this file, which has no typed C interface yet.
 * The original loads the chosen handler's address into r1 before jumping to it, and a
 * handler may leave r1 untouched, so r1 must carry that address into the call. */
static void run_phase_00(u32 phase) {
    R0 = phase;
    R1 = PHASE_HANDLER_00;
    GUEST_CALL(battle_destroyBattleObject_801741C);
}

/* register form: calls sub_801746E outside this file, which has no typed C interface yet.
 * The original loads the chosen handler's address into r1 before jumping to it, and a
 * handler may leave r1 untouched, so r1 must carry that address into the call. */
static void run_phase_04(u32 phase) {
    R0 = phase;
    R1 = PHASE_HANDLER_04;
    GUEST_CALL(sub_801746E);
}

/* register form: calls battle_objectTickTimerAndSetPhaseC_80174AA outside this file, which has no typed C interface yet.
 * The original loads the chosen handler's address into r1 before jumping to it, and a
 * handler may leave r1 untouched, so r1 must carry that address into the call. */
static void run_phase_08(u32 phase) {
    R0 = phase;
    R1 = PHASE_HANDLER_08;
    GUEST_CALL(battle_objectTickTimerAndSetPhaseC_80174AA);
}

/* register form: calls battle_tickIntroState_80174BE outside this file, which has no typed C interface yet.
 * The original loads the chosen handler's address into r1 before jumping to it, and a
 * handler may leave r1 untouched, so r1 must carry that address into the call. */
static void run_phase_0C(u32 phase) {
    R0 = phase;
    R1 = PHASE_HANDLER_0C;
    GUEST_CALL(battle_tickIntroState_80174BE);
}

/* Flash every sprite white, then run this battle object's current phase
 * handler (jump table off_801740C, indexed by the raw CurPhase byte).
 * Whatever registers the handler leaves behind (r1, r5, r8-r12) reach our
 * caller unchanged, exactly as in the original's jump-table call; the object
 * pointer in r5 and r10 reach the handler untouched. */
static void flash_white_then_run_phase(u32 battleObject) {
    force_white_palette();

    u8 phase = LD8(battleObject + oBattleObject_CurPhase);
    switch (phase) {
    case PHASE_00: run_phase_00(phase); break;
    case PHASE_04: run_phase_04(phase); break;
    case PHASE_08: run_phase_08(phase); break;
    case PHASE_0C: run_phase_0C(phase); break;
    }
}

/* sub_80173F4(r5 = battle object) -> r1, r5, r8-r12 as left by the phase handler */
Next sub_80173F4(void) {
    flash_white_then_run_phase(R5);
    RETURN_TO_CALLER();
}

/* ── sub_801746E ── */
/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet */
static bool battle_time_stopped(void) {
    GUEST_CALL(battle_isTimeStop);
    return !ZERO_FLAG();          /* the original skips the whole sequence when the result is nonzero */
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet.
 * The AI data pointer is still in r1 at this call in the original, so it is passed along. */
static void play_sound_effect_sub_801746E(u32 id, u32 ai) {
    R0 = id;
    R1 = ai;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls spawn_t1_0x0_EffectObject outside this file, which has no typed C interface yet.
 * The original calls it twice back to back; the second call reuses whatever register state the
 * first one left behind, so both calls sit side by side here. */
static void spawn_effect_object_pair(u32 source, u32 x, u32 y, u32 z) {
    R0 = source;
    R1 = x;
    R2 = y;
    R3 = z;
    R4 = 3;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
    GUEST_CALL(spawn_t1_0x0_EffectObject);
}

/* Start the object's destruction sequence: switch it to animation 2 and let the animator run
 * again, unlink its paired object, clear the AI scratch fields, play sound 0x6C, spawn the
 * effect-object pair at its position and arm the 21-frame countdown into phase 8.
 * Nothing happens while time stop is active. */
static void begin_destruction_sequence(u32 obj) {
    if (battle_time_stopped())
        return;

    ST8(obj + oBattleObject_CurAnim, 2);
    ST32(obj + oBattleObject_RelatedObject1Ptr, 0);
    ST8(obj + oBattleObject_PreventAnim, 0);

    u32 ai = LD32(obj + oBattleObject_AIDataPtr);
    ST32(ai + oAIData_AIScriptOffset, 0);
    ST32(ai + oAIData_TempAttackObjectPtr, 0);

    play_sound_effect_sub_801746E(0x6C, ai);

    /* the spawner takes the object's position words at +0x34 plus a pointer just past them */
    u32 x = LD32(obj + 0x34);
    u32 y = LD32(obj + 0x38);
    u32 z = LD32(obj + 0x3C);
    spawn_effect_object_pair(obj + 0x40, x, y, z);

    ST16(obj + oBattleObject_Timer, 0x15);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 8);
}

/* sub_801746E(r5 = battle object) -> r1 (left as the last callee returned it), r5/r10 unchanged */
Next sub_801746E(void) {
    begin_destruction_sequence(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectTickTimerAndSetPhaseC_80174AA ── */
/* battle_objectTickTimerAndSetPhaseC_80174AA (r5 = battle object): tick the object's action timer down by
 * one. When it reaches zero, reset the timer and advance the object to
 * phase 12 (0xC). r0 and the flags reflect the decrement, as in the original
 * (flags come from the SUBS; the expired path then re-runs them through the
 * phase write, which only refreshes nz). */
Next battle_objectTickTimerAndSetPhaseC_80174AA(void) {
    u32 timer = LD16(R5 + oBattleObject_Timer);
    u32 remaining = timer - 1;

    ST16(R5 + oBattleObject_Timer, remaining);
    R0 = remaining;
    SET_FLAGS_CMP(timer, 1); /* flags of "subs r0, #1" */

    if (remaining == 0) {
        /* Timer expired: restart it and switch to phase 12. */
        ST16(R5 + oBattleObject_Timer, 0);
        R0 = 0xC;
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── battle_tickIntroState_80174BE ── */
/* register form: calls sprite_setMosaicSize outside this file, which has no typed C interface yet
 * (original sets both r0 and r1 to the halved timer before the call) */
static void call_sprite_setMosaicSize(u32 half) {
    R0 = half;
    R1 = half;
    GUEST_CALL(sprite_setMosaicSize);
}

/* register form: calls sprite_setAlpha_8002c7a outside this file, which has no typed C interface yet */
static u32 call_sprite_setAlpha(u32 alpha) {
    R0 = alpha;
    GUEST_CALL(sprite_setAlpha_8002c7a);
    return R0;
}

/* register form: calls sprite_clearMosaic outside this file, which has no typed C interface yet */
static void call_sprite_clearMosaic(void) {
    GUEST_CALL(sprite_clearMosaic);
}

/* register form: calls sprite_disableAlpha outside this file, which has no typed C interface yet */
static void call_sprite_disableAlpha(void) {
    GUEST_CALL(sprite_disableAlpha);
}

/* register form: calls battle_ClearAllianceTargetSlotIfSelf_802CDD0 outside this file, which has no typed C interface yet */
static void call_battle_ClearAllianceTargetSlotIfSelf(void) {
    GUEST_CALL(battle_ClearAllianceTargetSlotIfSelf_802CDD0);
}

/* register form: calls battle_getObjectNameID_8011020 outside this file, which has no typed C interface yet */
static void call_battle_getObjectNameID(void) {
    GUEST_CALL(battle_getObjectNameID_8011020);
}

/* Per-branch output: the value to leave in R0 plus the CMP operands whose subtraction
 * reproduces the original's outgoing flags, so the adapter can apply a single SET_FLAGS. */
typedef struct {
    u32 r0;
    u32 cmp_a;
    u32 cmp_b;
} IntroResult;

/* Tick a battle object's appearance timer. While it ramps from 0 to 0x20, shrink the sprite
 * mosaic and fade its alpha. At 0x20 the intro finishes: clear the effects, drop the alive
 * flag, refresh the object's name, and mark the state action phase as initialized. */
static IntroResult tick_intro(u32 object_ptr) {
    u32 timer = (u32)LD16(object_ptr + oBattleObject_Timer) + 1;
    ST16(object_ptr + oBattleObject_Timer, (u16)timer);

    if (timer == 0x20) {
        call_sprite_clearMosaic();
        call_sprite_disableAlpha();
        call_battle_ClearAllianceTargetSlotIfSelf();

        u8 flags = LD8(object_ptr + oObjectHeader_Flags);
        ST8(object_ptr + oObjectHeader_Flags, flags & (u8)~2);

        call_battle_getObjectNameID();
        ST32(object_ptr + oBattleObject_CurStateActionPhaseAndPhaseInitialized, 8);

        /* Flags inherited from CMP r0,#0x20 (equal: Z=1, C=1). The trailing `mov r0,#8`
         * does not update flags, so the outgoing state matches CMP(0x20, 0x20). */
        IntroResult r = { 8, 0x20, 0x20 };
        return r;
    }

    /* Mid-fade: mosaic halves each tick, alpha ramps from 0x10 toward 0. */
    u32 half = timer >> 1;
    call_sprite_setMosaicSize(half);
    u32 r0 = call_sprite_setAlpha(0x10 - half);

    /* Flags come from `neg r0,r0` (RSBS r0,r0,#0) where r0 had just been (half - 0x10);
     * that's equivalent to CMP #0, (half - 0x10). R0 is the clobbered return from setAlpha. */
    IntroResult res = { r0, 0, half - 0x10 };
    return res;
}

Next battle_tickIntroState_80174BE(void) {
    IntroResult res = tick_intro(R5);
    R0 = res.r0;
    SET_FLAGS_CMP(res.cmp_a, res.cmp_b);
    RETURN_TO_CALLER();
}

/* ── battle_endTurnMarkActed_8017888 ── */
/* battle_endTurnMarkActed_8017888(r5 = battle object):
 * Turn-end handling for a battle object. If the battle flag 0x40 is set, the
 * object's actor type (byte 1 of the GetVerActorTyAndAIIdx record for its
 * NameID) is 2, and the first word of its AI data block (aidata+0x40) is
 * still zero, run battle_spawnAndLinkTypeF_80E1620 (r1 = actor type, r7 = AI data block pointer).
 * In every case the object is marked as having acted: CurAction (offset 9)
 * = 8 and the current phase is cleared. r0 = 0 on return.
 * Flags: N/Z from the final "mov r0,#0"; C/V from the last compare or callee. */
Next battle_endTurnMarkActed_8017888(void) {
    u32 saved_r7 = R7;
    u32 obj = R5;

    GUEST_CALL(battle_testFlag0x40_800A8F8); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = LD16(obj + oBattleObject_NameID);
        GUEST_CALL(GetVerActorTyAndAIIdx_80182B4); /* -> {version, actor_type, ai_index} */
        R1 = LD8(R0 + 1); /* actor type */
        if (R1 != 2) {
            SET_FLAGS_CMP(R1, 2);
        } else {
            /* first word of this enemy's AI data block */
            R7 = LD32(obj + oBattleObject_AIDataPtr) + 0x40;
            R0 = LD32(R7);
            if (R0 != 0) {
                SET_FLAGS_CMP(R0, 0);
            } else {
                GUEST_CALL(battle_spawnAndLinkTypeF_80E1620);
            }
        }
    }

    R7 = saved_r7; /* r7 is only scratch here; restored on exit */
    ST8(obj + 9, 8); /* CurAction = 8 */
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_80178B6 ── */
/* sub_80178B6(r5 = battle object):
 * Trampoline/dispatcher: reads the object's handler selector (oBattleObject_MoveSubState,
 * which stores a byte offset into the jump table) and tail-calls the matching
 * handler battleObject_initMoveStep_80178D4 / battleObject_applyVerticalMovement_8017992 / sub_8017A38. The original pushes lr, sets
 * lr = pc and branches through the table, so the callee returns straight back
 * to our caller: this is a tail call and the callee's results are ours.
 * On entry to the handler, r0 holds the selector byte and r1 holds the table
 * entry (the Thumb handler pointer), exactly as the bx r1 sequence leaves them. */
Next sub_80178B6(void) {
    u32 kind = LD8(R5 + oBattleObject_MoveSubState);

    /* r1 at handler entry is the raw jump-table entry, as loaded by `ldr r1, [r1,r0]`. */
    R0 = kind;
    R1 = LD32(A_off_80178C8 + kind);

    switch (kind) {
    case 0: GUEST_CALL(battleObject_initMoveStep_80178D4); break;
    case 4: GUEST_CALL(battleObject_applyVerticalMovement_8017992); break;
    case 8: GUEST_CALL(sub_8017A38); break;
    /* Any other selector would send the original to a garbage address; the game
     * never stores one, so there is nothing to do here. */
    }

    RETURN_TO_CALLER();
}

/* ── battleObject_initMoveStep_80178D4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r8, r9, r10, r11, r12; } call_battle_dispatchActorTurnByTypeAndAIIdx_800F404_result;
/* register form: generated wrapper: battle_dispatchActorTurnByTypeAndAIIdx_800F404 has no typed C interface yet */
static inline call_battle_dispatchActorTurnByTypeAndAIIdx_800F404_result call_battle_dispatchActorTurnByTypeAndAIIdx_800F404(u32 r4, u32 r5, u32 r8) {
    R4 = r4;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(battle_dispatchActorTurnByTypeAndAIIdx_800F404);
    return (call_battle_dispatchActorTurnByTypeAndAIIdx_800F404_result){ R4, R5, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r2; } call_object_setFlag1_result_battleObject_initMoveStep_80178D4;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_battleObject_initMoveStep_80178D4 call_object_setFlag1_battleObject_initMoveStep_80178D4(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_battleObject_initMoveStep_80178D4){ R2 };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag_battleObject_initMoveStep_80178D4(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

typedef struct { u32 r2, r5, r6, r7, r8, r9, r10, r11, r12; } call_sub_8011450_result;
/* register form: generated wrapper: sub_8011450 has no typed C interface yet */
static inline call_sub_8011450_result call_sub_8011450(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_8011450);
    return (call_sub_8011450_result){ R2, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r3; } call_battleObject_clearPwrAtkState_8012EA8_result;
/* register form: generated wrapper: battleObject_clearPwrAtkState_8012EA8 has no typed C interface yet */
static inline call_battleObject_clearPwrAtkState_8012EA8_result call_battleObject_clearPwrAtkState_8012EA8(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_clearPwrAtkState_8012EA8);
    return (call_battleObject_clearPwrAtkState_8012EA8_result){ R3 };
}

typedef struct { u32 r0; } call_object_getFlip_result_battleObject_initMoveStep_80178D4;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battleObject_initMoveStep_80178D4 call_object_getFlip_battleObject_initMoveStep_80178D4(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battleObject_initMoveStep_80178D4){ R0 };
}

typedef struct { u32 r3; } call_sprite_setFlip_result_battleObject_initMoveStep_80178D4;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_battleObject_initMoveStep_80178D4 call_sprite_setFlip_battleObject_initMoveStep_80178D4(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_battleObject_initMoveStep_80178D4){ R3 };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battleObject_initMoveStep_80178D4(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r3, r4, r6; bool c, v; } call_object_updateCollisionPanels_result_battleObject_initMoveStep_80178D4;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_battleObject_initMoveStep_80178D4 call_object_updateCollisionPanels_battleObject_initMoveStep_80178D4(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_battleObject_initMoveStep_80178D4){ R3, R4, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_clearFlag has no typed C interface yet */
static inline void call_object_clearFlag_battleObject_initMoveStep_80178D4(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_clearFlag);
}

/* register form: generated wrapper: battle_resetCollisionState_80101C4 has no typed C interface yet */
static inline void call_battle_resetCollisionState_80101C4(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_resetCollisionState_80101C4);
}

/* register form: generated wrapper: object_removePanelReserve has no typed C interface yet */
static inline void call_object_removePanelReserve_battleObject_initMoveStep_80178D4(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_removePanelReserve);
}

typedef struct { u32 r3; bool v; } call_tableAddSaturatedByte_800AB46_result;
/* register form: generated wrapper: tableAddSaturatedByte_800AB46 has no typed C interface yet */
static inline call_tableAddSaturatedByte_800AB46_result call_tableAddSaturatedByte_800AB46(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(tableAddSaturatedByte_800AB46);
    return (call_tableAddSaturatedByte_800AB46_result){ R3, OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r4, r5, r8, r9, r10, r11, r12; } call_sub_800E45E_result;
/* register form: generated wrapper: sub_800E45E has no typed C interface yet */
static inline call_sub_800E45E_result call_sub_800E45E(u32 r2, u32 r5) {
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sub_800E45E);
    return (call_sub_800E45E_result){ R0, R1, R2, R4, R5, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r1, r3, r5, r6, r7; bool z, n, v; } call_battle_objectCheckPanelByAlliance_800E5AC_result_battleObject_initMoveStep_80178D4;
/* register form: generated wrapper: battle_objectCheckPanelByAlliance_800E5AC has no typed C interface yet */
static inline call_battle_objectCheckPanelByAlliance_800E5AC_result_battleObject_initMoveStep_80178D4 call_battle_objectCheckPanelByAlliance_800E5AC_battleObject_initMoveStep_80178D4(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(battle_objectCheckPanelByAlliance_800E5AC);
    return (call_battle_objectCheckPanelByAlliance_800E5AC_result_battleObject_initMoveStep_80178D4){ R1, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3; bool c, v; } call_object_reservePanel_result_battleObject_initMoveStep_80178D4;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_battleObject_initMoveStep_80178D4 call_object_reservePanel_battleObject_initMoveStep_80178D4(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_battleObject_initMoveStep_80178D4){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battleObject_initMoveStep_80178D4: end-of-turn setup for a battle object.
 *
 * Dispatches the actor's AI/turn logic, resets per-turn state (flags,
 * animation, sprite flip, panel coordinates, collision), then either
 * commits a one-panel step (setting velocity and reserving the target
 * panel) or falls through to a short idle wait. */

#define FLAG_ACTED       0x500000u
#define FLAG_ANIM_MASK   0x800u       /* 1 << 11 */
#define FLAG_ANIM_IDLE   0x20000u
#define FLAG_CLEAR_MOVE  0x1441u
#define VEL_X_SCALE      0x000A0000u
#define VEL_Y_SCALE      0x00060000u

static void init_move_step(u32 r4_in, u32 r5_in, u32 r8_in, u32 r10_in) {
    u32 r5 = r5_in;

    /* Run the actor's turn/AI dispatch. */
    {
        call_battle_dispatchActorTurnByTypeAndAIIdx_800F404_result d =
            call_battle_dispatchActorTurnByTypeAndAIIdx_800F404(r4_in, r5, r8_in);
        r5 = d.r5;
        /* r4, r8, r9, r10, r11, r12 are left in guest registers by the wrapper */
    }

    /* Set the "acted this turn" flag and clear scratch pointers. */
    call_object_setFlag1_battleObject_initMoveStep_80178D4(FLAG_ACTED, r5);

    ST32(r5 + oBattleObject_RelatedObject1Ptr, 0);
    {
        u32 ai = LD32(r5 + oBattleObject_AIDataPtr);
        ST32(ai + oAIData_TempAttackObjectPtr, 0);
    }

    /* Pick an idle animation index based on two flag bits:
     *   bit 0x800 set   -> 2, bit clear -> 1 unless bit 0x20000 set -> 0 */
    {
        u32 flags = call_object_getFlag_battleObject_initMoveStep_80178D4(r5);
        s32 anim = 2;
        if (!(flags & FLAG_ANIM_MASK)) {
            anim = 1;
            if (flags & FLAG_ANIM_IDLE)
                anim = 0;
        }
        ST8(r5 + oBattleObject_CurAnim, (u8)anim);
    }
    ST8(r5 + oBattleObject_CurAnimCopy, 0xFF);

    /* Update secondary state (sub_8011450 may change r5 and r8-r12). */
    {
        call_sub_8011450_result s = call_sub_8011450(r5);
        r5 = s.r5;
    }

    call_battleObject_clearPwrAtkState_8012EA8(r5);

    /* Mirror the sprite to match the object's facing. */
    {
        call_object_getFlip_result_battleObject_initMoveStep_80178D4 gf = call_object_getFlip_battleObject_initMoveStep_80178D4(r5);
        call_sprite_setFlip_battleObject_initMoveStep_80178D4(gf.r0, r5);
    }

    /* Commit queued panel position and recompute world coordinates. */
    {
        u16 xy = LD16(r5 + oBattleObject_FuturePanelXY);
        ST16(r5 + oBattleObject_PanelXY, xy);
    }
    call_object_setCoordinatesFromPanels_battleObject_initMoveStep_80178D4(r5);
    call_object_updateCollisionPanels_battleObject_initMoveStep_80178D4(r5);

    /* Reset z and clear move-related flags. */
    ST16(r5 + oBattleObject_Z_3c, 0);
    call_object_clearFlag_battleObject_initMoveStep_80178D4(FLAG_CLEAR_MOVE, r5);
    call_battle_resetCollisionState_80101C4(r5);

    /* Release the reservation on the previously-queued future panel. */
    {
        u32 fx = LD8(r5 + oBattleObject_FuturePanelX);
        u32 fy = LD8(r5 + oBattleObject_FuturePanelY);
        call_object_removePanelReserve_battleObject_initMoveStep_80178D4(fx, fy, r5);
    }

    /* Increment alliance turn counter (saturated to 3). */
    {
        u32 alliance = LD8(r5 + oBattleObject_Alliance);
        call_tableAddSaturatedByte_800AB46(alliance, 3, 1);
    }

    /* Ask the AI helper for a contact offset (dx, dy) and a step timer. */
    call_sub_800E45E_result ai = call_sub_800E45E(1, r5);
    u32 dx = ai.r0;
    u32 dy = ai.r1;
    u32 timer2 = ai.r2;
    r5 = ai.r5;

    ST8(r5 + oBattleObject_ContactPanelOffsetX, dx);
    ST8(r5 + oBattleObject_ContactOffsetY, dy);
    ST16(r5 + oBattleObject_Timer2, timer2);

    if (timer2 != 0) {
        /* Step toward target panel: check alliance occupancy first. */
        u32 target_x = (u8)(dx + (u8)LD8(r5 + oBattleObject_PanelX));
        u32 target_y = (u8)(dy + (u8)LD8(r5 + oBattleObject_PanelY));

        call_battle_objectCheckPanelByAlliance_800E5AC_result_battleObject_initMoveStep_80178D4 chk =
            call_battle_objectCheckPanelByAlliance_800E5AC_battleObject_initMoveStep_80178D4(target_x, target_y, r5);
        r5 = chk.r5;

        if (!chk.z) {
            /* Path is clear: apply velocity from signed direction bytes,
             * queue the target panel, reserve it, enter moving state. */
            s32 vel_x = (s32)VEL_X_SCALE * (s32)(s8)LD8(r5 + oBattleObject_ContactPanelOffsetX);
            s32 vel_y = (s32)VEL_Y_SCALE * (s32)(s8)LD8(r5 + oBattleObject_ContactOffsetY);
            ST32(r5 + oBattleObject_XVelocity, vel_x);
            ST32(r5 + oBattleObject_YVelocity, vel_y);

            ST8(r5 + oBattleObject_FuturePanelX, target_x);
            ST8(r5 + oBattleObject_FuturePanelY, target_y);
            call_object_reservePanel_battleObject_initMoveStep_80178D4(target_x, target_y, r5);
            ST8(r5 + oBattleObject_MoveSubState, 4);
            return;
        }
    }

    /* Idle fallback: wait 0x18 ticks in sub-state 8. */
    ST16(r5 + oBattleObject_Timer, 0x18);
    ST8(r5 + oBattleObject_MoveSubState, 8);
}

Next battleObject_initMoveStep_80178D4(void) {
    init_move_step(R4, R5, R8, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8017A38 ── */
/* sub_8017A38: Battle object tick.
 * Decrement the object's Timer field; when it hits zero, branch on a flag
 * to either clear a "stuck" bitmask and reset CurAction=4, or else run the
 * full "knock down / recover" cleanup (clear several AI/object flags,
 * reset animation fields, call sub_8011450, then set CurAction=8).
 *   inputs:  r5 = object pointer, lr
 *   outputs: r1 r5 r8 r9 r10 r11 r12
 *   clobbers: r0 r2 r3 r4 r6 r7 lr, nz c v
 */
Next sub_8017A38(void) {
    u32 timer = LD16(R5 + oBattleObject_Timer);
    timer -= 1;
    ST16(R5 + oBattleObject_Timer, timer);

    /* BGE locret — timer still positive, nothing else to do. */
    if ((s32)timer >= 0)
        RETURN_TO_CALLER();

    /* Timer expired: peek at object flag 0x800 (bit 11). */
    GUEST_CALL(object_getFlag);
    u32 flags = R0;
    if ((flags & 0x800) != 0) {
        /* Branch A — just clear the high bit and snap to action 4. */
        R0 = 0x100000;
        GUEST_CALL(object_clearFlag);
        ST8(R5 + oBattleObject_CurAction, 4);
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
        RETURN_TO_CALLER();
    }

    /* Branch B — full recovery sequence. */
    R0 = 0x501800;
    GUEST_CALL(object_clearFlag);

    R0 = 0x1000043F;
    GUEST_CALL(ClearAIData_Unk_44_Flag);

    R0 = 0x200000;
    GUEST_CALL(ClearAIDataUnk0x48Flag);

    R0 = 0x10;
    GUEST_CALL(object_clearFlag2);

    ST8(R5 + oBattleObject_AIType, 0);
    ST8(R5 + oBattleObject_CurAnim, 0);
    ST8(R5 + oBattleObject_CurAnimCopy, 0xFF);

    GUEST_CALL(sub_8011450);

    ST8(R5 + oBattleObject_CurAction, 8);
    ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    RETURN_TO_CALLER();
}

/* ── GetVerActorTyAndAIIdx_80182B4 ── */
/* GetVerActorTyAndAIIdx_80182B4:
 *   input:  r0 = enemy index (u16)
 *   output: r0 = pointer into the per-index (version, actor_type, ai_index) table
 *           v   = overflow flag from the final index arithmetic
 *   clobbers: nz c r1
 *
 * Returns a pointer to a 3-byte record { version, actor_type, ai_index }
 * for the given enemy index. The table is a flat array of 3-byte entries
 * addressed by index*3. */
Next GetVerActorTyAndAIIdx_80182B4(void) {
    u32 enemy_idx = R0;
    u32 offset = enemy_idx * 3;
    u32 result = (u32)A_EnemyVerActorTyAndAIIdxTable + offset;

    /* Match the original's flag-setting: ADD computes flags on the
     * (base << 0) + (offset << 0) operation. */
    SET_FLAGS_ADD(result - offset, offset);

    R1 = 3;
    R0 = result;
    RETURN_TO_CALLER();
}

/* ── getBattleArmPositionMaybe_8018810 ── */
/* getBattleArmPositionMaybe_8018810(r0 = arm id, r1 = arm index, r2/r3 = battle object):
 * Computes one coordinate pair of an arm/part attachment point on a battle
 * object, mirrored to match the object's facing direction.
 *   - Ids 0xCD..0xFF are not a real arm slot: returns the fixed pair (0, 7).
 *   - Otherwise: enemy_getArmPosition_800F26C fetches the lookup base for the object pair,
 *     getIndexedSignedBytePair_8018842 reads the signed (x, y) byte offsets
 *     for the given index, and object_getFlipDirection gives the mirror factor.
 * Returns r0 = x * flip, r1 = y, r2 = flip direction. r4-r6 preserved. */
Next getBattleArmPositionMaybe_8018810(void) {
    u32 id = R0, index = R1, objA = R2, objB = R3;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    if ((s32)id >= (s32)0xCD && (s32)id <= (s32)0xFF) {
        /* Invalid slot id: fixed default position */
        R0 = 0;
        R1 = 7;
    } else {
        /* Callees receive the object args again in r4-r6 */
        R4 = index;
        R5 = objA;
        R6 = objB;

        GUEST_CALL(enemy_getArmPosition_800F26C);
        u32 base0 = R0, base1 = R1;

        /* r2 = index (r4 still holds it; enemy_getArmPosition_800F26C preserves r4) */
        R2 = R4;
        GUEST_CALL(getIndexedSignedBytePair_8018842);
        u32 offsetX = R0, offsetY = R1;

        R0 = objA;
        R1 = objB;
        GUEST_CALL(object_getFlipDirection);
        u32 flip = R0;

        R0 = offsetX * flip;
        R1 = offsetY;
        R2 = flip;

        R4 = savedR4;
        R5 = savedR5;
        R6 = savedR6;
    }
    RETURN_TO_CALLER();
}

/* ── getIndexedSignedBytePair_8018842 ── */
/* getIndexedSignedBytePair_8018842(r0, r1 = arguments for the lookup, r2 = entry index, r4 = unused):
 * Looks up a two-byte table entry: the entry base comes from getAnmSeqRowIndexTableEntry_8018886
 * (called with r0/r1 as passed in), offset by twice the index in r2.
 * Outputs: r0 = first byte (signed), r1 = second byte (signed) of the entry.
 * r2/r3/r4 are left untouched. */
Next getIndexedSignedBytePair_8018842(void) {
    u32 addr = R2 + R2;

    GUEST_CALL(getAnmSeqRowIndexTableEntry_8018886);
    addr += R0;

    R0 = (s8)LD8(addr);
    R1 = (s8)LD8(addr + 1);
    RETURN_TO_CALLER();
}

/* ── getAnmSeqRowIndexTableEntry_8018886 ── */
/* getAnmSeqRowIndexTableEntry_8018886(r0 = table_index, r1 = sub_index):
 * Look up an entry in one of three sprite/animation tables.
 *
 * When r0 == 0, r1 is used as an index into a 16-byte mapping table (A_byte_80188B0) to
 * remap it into a different table index (one of 0, 1, or 2). Otherwise r1 is kept as-is.
 *
 * R0 is then used to pick one of three table bases (A_off_80188A0 -> {A_byte_80188C0,
 * A_byte_801898C, A_byte_80191C8}) and the result pointer is the base plus r1 * 0x44
 * (each record is 0x44 bytes long).
 *
 *   inputs:  r0 r1 lr
 *   outputs: r0 r3
 *   clobbers: nz c v r1 r2
 *   preserved: -
 */
Next getAnmSeqRowIndexTableEntry_8018886(void) {
    /* If table_index == 0, remap sub_index through the lookup table. */
    if (R0 == 0) {
        R1 = LD8(A_byte_80188B0 + R1);
    }

    /* Base pointer for the chosen table, then offset into record r1. */
    R0 = LD32(A_off_80188A0 + R0);
    R3 = 0x44;
    R0 = R0 + R1 * R3;

    RETURN_TO_CALLER();
}

/* ── collision_resetAllHitData_801986C ── */
/* collision_resetAllHitData_801986C: reset the collision system's per-hit data.
 * Clears the "active collision" bitfield, then reinitializes all 32 collision
 * data slots: each slot is zero-filled and its CollisionIndexBit is set to a
 * unique one-hot bit (0x80000000 for slot 0, 0x40000000 for slot 1, ...).
 * Finally clears the collision scratch buffer.
 *
 * Reads the toolkit pointer from r10; preserves r2-r5 and r10. */
Next collision_resetAllHitData_801986C(void) {
    ST32(A_eActiveCollisionDataBitfield, 0);

    u32 indexBit = 1u << 31;
    u32 entry = LD32(R10 + oToolkit_CollisionDataPtr);
    while (indexBit != 0) {
        R0 = entry;
        R1 = oCollisionData_Size;
        GUEST_CALL(ZeroFillByWord);
        ST32(entry + oCollisionData_CollisionIndexBit, indexBit);
        entry += oCollisionData_Size;
        indexBit >>= 1;
    }

    GUEST_CALL(clearScratchBuffer_2034F60_8019FA4);
    RETURN_TO_CALLER();
}

/* ── object_createCollisionData ── */
/* object_createCollisionData: 08019892-080198CE THUMB
 *
 * Allocates a collision-data slot for the battle object at r5. Scans
 * eActiveCollisionDataBitfield from bit 31 down for the first free slot,
 * claims it, zeroes the slot, marks it active and stores its address in
 * the object.
 *
 * inputs:  r5 = battle object, r10 = toolkit (r4 preserved, unused)
 * outputs: r0 = slot address (0 if all 32 slots are taken), r1, r2, r3,
 *          c, v; r4/r5/r10 preserved.
 */
Next object_createCollisionData(void) {
    u32 slotBase = LD32(R10 + oToolkit_CollisionDataPtr);
    u32 bitfieldAddr = A_eActiveCollisionDataBitfield;
    u32 used = LD32(bitfieldAddr);

    /* find the first free slot; each slot is 0xA8 bytes */
    u32 mask = 1u << 31;
    while (used & mask) {
        slotBase += 0xA8;
        mask >>= 1;
        if (mask == 0) {
            /* no free slot: return 0. Flags are those left by the last
             * `lsr r1, r1, #1` (result 0, carry out of the final bit):
             * Z=1, C=1, N=0, V=0. */
            R0 = 0;
            R1 = 0;
            R2 = used;
            R3 = bitfieldAddr;
            SET_FLAGS_CMP(0, 0);
            RETURN_TO_CALLER();
        }
    }

    /* claim the slot */
    used |= mask;
    ST32(bitfieldAddr, used);
    R2 = used;
    R3 = bitfieldAddr;

    /* clear the fixed part and the per-hit part of the slot */
    R0 = slotBase;
    R1 = 0x44;
    GUEST_CALL(ZeroFillByWord);

    R0 = slotBase + 0x48;
    R1 = 0x60;
    GUEST_CALL(ZeroFillByWord);

    /* mark the slot active and link it to the object */
    ST8(slotBase, 1);
    ST32(R5 + oBattleObject_CollisionDataPtr, slotBase);

    R0 = slotBase;
    R1 = 1;
    /* the original ends with `adds r0, r4, #0` before the two stores */
    SET_FLAGS_ADD(slotBase, 0);
    RETURN_TO_CALLER();
}

/* ── object_freeCollisionData ── */
/* object_freeCollisionData(r0 = collision data pointer):
 * Mark a piece of collision data as disabled and clear its bit in the
 * engine-wide active-collision bitfield. Safe to call with a NULL pointer. */
Next object_freeCollisionData(void) {
    u32 collisionData = R0;
    if (collisionData == 0)
        goto done;

    /* Disable this collision data slot. */
    ST8(collisionData + oCollisionData_Enabled, 0);

    /* Look up the index bit belonging to this slot and clear it. */
    u32 indexBit = LD32(collisionData + oCollisionData_CollisionIndexBit);
    u32 bitfield = LD32(A_eActiveCollisionDataBitfield);
    ST32(A_eActiveCollisionDataBitfield, bitfield & ~indexBit);

done:
    RETURN_TO_CALLER();
}

/* ── battle_SetObjectHitModifier_8019F86 ── */
/* battle_SetObjectHitModifier_8019F86(r0 = hit modifier value, r5 = battle object):
 * store the object's hit modifier into its collision data.
 * Returns (r1) the pointer to the collision data that was written. */
Next battle_SetObjectHitModifier_8019F86(void) {
    u32 *collisionData = (u32 *)LD32(R5 + oBattleObject_CollisionDataPtr);
    ST8((u32)collisionData + oCollisionData_HitModifierBase, R0);
    R1 = (u32)collisionData;
    RETURN_TO_CALLER();
}

/* ── battleObject_SetSecondaryElementWeakness_8019F9E ── */
/* battleObject_SetSecondaryElementWeakness_8019F9E(r0 = element, r5 = battle object):
 * set the object's secondary element weakness in its collision data. */
Next battleObject_SetSecondaryElementWeakness_8019F9E(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST8(collisionData + oCollisionData_SecondaryElementWeakness, R0);
    RETURN_TO_CALLER();
}

/* ── clearScratchBuffer_2034F60_8019FA4 ── */
/* clearScratchBuffer_2034F60_8019FA4: wipe the 0xA0-byte scratch buffer at unk_2034F60.
 * Inputs: r2, r3 (passed through untouched). */
Next clearScratchBuffer_2034F60_8019FA4(void) {
    R0 = A_unk_2034F60;
    R1 = 0xA0;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── object_setupCollisionData ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r3; } call_object_getTableEntry_801A0BA_result;
/* register form: generated wrapper: object_getTableEntry_801A0BA has no typed C interface yet */
static inline call_object_getTableEntry_801A0BA_result call_object_getTableEntry_801A0BA(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_getTableEntry_801A0BA);
    return (call_object_getTableEntry_801A0BA_result){ R0, R1, R3 };
}

typedef struct { u32 r0; } call_battle_isTimeStop_result;
/* register form: generated wrapper: battle_isTimeStop has no typed C interface yet */
static inline call_battle_isTimeStop_result call_battle_isTimeStop(void) {
    GUEST_CALL(battle_isTimeStop);
    return (call_battle_isTimeStop_result){ R0 };
}

typedef struct { u32 r0, r1, r3, r6; bool z, n, c, v; } call_sub_8019F44_result;
/* register form: generated wrapper: object_applySelfCollision_8019F44 has no typed C interface yet */
static inline call_sub_8019F44_result call_sub_8019F44(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_applySelfCollision_8019F44);
    return (call_sub_8019F44_result){ R0, R1, R3, R6, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Fill the collision-data struct for a battle object, resolve self/target collision
 * type flags from the table, and finalize via object_applySelfCollision_8019F44. */
typedef struct {
    u32 r0;  /* object_applySelfCollision_8019F44 exit r0 */
    u32 r1;  /* object_applySelfCollision_8019F44 exit r1 */
    u64 r6;  /* object_applySelfCollision_8019F44 exit r6 */
} CollisionSetupResult;

static CollisionSetupResult setup_collision_data(u32 out_ptr, u32 parent_ptr,
                                                 u32 table_self, u32 table_target,
                                                 u32 hit_modifier) {
    /* Copy parent object pointer */
    ST32(out_ptr + oCollisionData_ParentObjectPtr, parent_ptr);

    /* Store hit modifier base */
    ST8(out_ptr + oCollisionData_HitModifierBase, (u8)hit_modifier);

    /* Split element byte into primary (low nibble) and secondary (high nibble) */
    u32 element = LD8(parent_ptr + oBattleObject_Element);
    ST8(out_ptr + oCollisionData_PrimaryElement,   (u8)(element & 0x0F));
    ST8(out_ptr + oCollisionData_SecondaryElement, (u8)(element & 0xF0));

    /* Copy alliance/direction-flip field */
    ST16(out_ptr + oCollisionData_AllianceAndDirectionFlip,
         LD16(parent_ptr + oBattleObject_AllianceAndDirectionFlip));

    /* Copy panel X/Y */
    ST16(out_ptr + oCollisionData_PanelXY,
         LD16(parent_ptr + oBattleObject_PanelXY));

    /* Region is always 1 */
    ST8(out_ptr + oCollisionData_Region, 1);

    /* Stamina damage counter / disabler (low byte of 16-bit field) */
    ST8(out_ptr + oCollisionData_StaminaDamageCounterDisabler,
        (u8)LD16(parent_ptr + oBattleObject_StaminaDamageCounterDisabler));

    /* Self damage */
    ST16(out_ptr + oCollisionData_SelfDamage,
         LD16(parent_ptr + oBattleObject_Damage));

    /* Self collision-type flags: lookup by alliance + table_self */
    u8 alliance = (u8)LD8(parent_ptr + oBattleObject_Alliance);
    call_object_getTableEntry_801A0BA_result self_entry =
        call_object_getTableEntry_801A0BA(alliance, table_self);
    ST32(out_ptr + oCollisionData_SelfCollisionTypeFlags, self_entry.r0);

    /* If time stop is active, OR in the frozen bit (bit 16) */
    call_battle_isTimeStop_result ts = call_battle_isTimeStop();
    if (ts.r0 != 0) {
        u32 flags = LD32(out_ptr + oCollisionData_SelfCollisionTypeFlags);
        ST32(out_ptr + oCollisionData_SelfCollisionTypeFlags, flags | (1u << 16));
    }

    /* Target collision-type flags: lookup by alliance + table_target */
    call_object_getTableEntry_801A0BA_result target_entry =
        call_object_getTableEntry_801A0BA(alliance, table_target);
    ST32(out_ptr + oCollisionData_TargetCollisionTypeFlags, target_entry.r0);

    /* Finalize: object_applySelfCollision_8019F44(collision_ptr, r1 from target lookup, parent_ptr) */
    call_sub_8019F44_result fin =
        call_sub_8019F44(out_ptr, target_entry.r1, parent_ptr);

    CollisionSetupResult result;
    result.r0 = fin.r0;
    result.r1 = fin.r1;
    result.r6 = fin.r6;
    return result;
}

Next object_setupCollisionData(void) {
    CollisionSetupResult res = setup_collision_data(
        R0,    /* out_ptr: collision data struct to fill */
        R5,    /* parent_ptr: source battle object */
        R1,    /* table_self: self collision-type table index */
        R2,    /* table_target: target collision-type table index */
        R3);   /* hit_modifier: stored as 8-bit */

    R0 = res.r0;
    R1 = res.r1;
    R6 = (u32)res.r6;
    RETURN_TO_CALLER();
}

/* ── object_removeCollisionData ── */
/* object_removeCollisionData (0x0801A00E):
 * THUMB trampoline into the ARM helper _object_removeCollisionData.
 * The helper is invoked with the caller's r4-r7 (and r10) still in place as
 * its arguments; the surrounding push/pop in the original exists only to
 * preserve r4-r7 and the original return address across the call, since the
 * ARM helper does not follow the standard callee-saved register discipline.
 * Results come back in r1-r3 and the condition flags; r4-r7 are unchanged. */
Next object_removeCollisionData(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    GUEST_CALL(_object_removeCollisionData);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── object_presentCollisionData ── */
/* object_presentCollisionData (r5 = battle object, r1 = new collision flags, r3 = context):
 * Called when an object is present in a collision. Refreshes the object's
 * collision data block: records the new collision flags, clears one-shot hit
 * state unless time is currently stopped, wipes the per-object interaction
 * block, and finally runs the shared collision handler (object_updateCollisionPanels_300777C) on the
 * collision data.
 * Preserves r4-r7; returns with r0-r3 as left by object_updateCollisionPanels_300777C. */
Next object_presentCollisionData(void) {
    u32 savedR4 = R4;
    u32 object = R5;
    u32 newFlags = R1;

    u32 coll = LD32(object + oBattleObject_CollisionDataPtr);
    R4 = coll; /* callees see r4 = collision data, as in the original */

    /* Merge the flags still pending in the collision data with the ones the
     * current collision produced, then store the new flags passed in r1. */
    u32 flagsFromCollision = LD32(coll + oCollisionData_FlagsFromCollision);
    u32 mergedFlags = LD32(coll + oCollisionData_CollisionFlagsAccumulator) | flagsFromCollision;
    ST32(coll + oCollisionData_CollisionFlagsAccumulator, newFlags);

    R0 = mergedFlags;
    R2 = flagsFromCollision;
    GUEST_CALL(battle_isTimeStop);

    if (R0 == 0) {
        /* Time is not stopped: drop the hit modifier, its follow-up byte and
         * the pending collision flags. */
        ST8(coll + oCollisionData_HitModifierFinal, 0);
        ST8(coll + 3, 0);
        ST32(coll + oCollisionData_CollisionFlagsAccumulator, 0);
    }

    /* Always clear the resolved status effect and zero the object's
     * interaction block (0x40 halfwords = 128 bytes at coll+0x68). */
    ST8(coll + oCollisionData_StatusEffectFinal, 0);
    R0 = coll + 0x68;
    R1 = 0x40;
    GUEST_CALL(ZeroFillByHalfword);

    /* Invoke the collision handler on the (reloaded, possibly updated)
     * collision data pointer. */
    R0 = LD32(object + oBattleObject_CollisionDataPtr);
    GUEST_CALL(object_updateCollisionPanels_300777C);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── object_updateCollisionPanels ── */
/* object_updateCollisionPanels(r4/r6 = scratch (restored), r5 = battle object):
 * Copies the object's current panel coordinates into its collision data record,
 * remembers the previous coordinates, then asks sub_800E994 which way the
 * object should face for the move from the old panel to the new one and stores
 * that as the collision data's direction.
 * Returns r0 = direction, r1/r2/r3 = new Y / old X / old Y (as passed to the
 * helper), with r4 and r6 preserved and the helper's flags left intact. */
Next object_updateCollisionPanels(void) {
    u32 object = R5;
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    u32 collision = LD32(object + oBattleObject_CollisionDataPtr);

    /* Previous panel position stored in the collision data. */
    u32 prevPanelX = LD8(collision + oCollisionData_PanelX);
    u32 prevPanelY = LD8(collision + oCollisionData_PanelY);

    /* Overwrite it with the object's current panel position. */
    u32 panelX = LD8(object + oBattleObject_PanelX);
    ST8(collision + oCollisionData_PanelX, panelX);
    u32 panelY = LD8(object + oBattleObject_PanelY);
    ST8(collision + oCollisionData_PanelY, panelY);

    /* direction = facing for the step (prevX, prevY) -> (panelX, panelY) */
    R0 = panelX;
    R1 = panelY;
    R2 = prevPanelX;
    R3 = prevPanelY;
    R4 = LD8(object + oBattleObject_Alliance);
    R6 = collision;
    GUEST_CALL(sub_800E994);
    u32 direction = R0;

    /* Restore the caller's r4/r6, as the original's push/pop does. */
    R4 = savedR4;
    R6 = savedR6;

    ST8(collision + oCollisionData_Direction, direction);
    RETURN_TO_CALLER();
}

/* ── object_setCollisionPanelsToCurrent ── */
/* object_setCollisionPanelsToCurrent(r4 = ?, r5 = battle object, lr = return):
 * Copy the object's current panel coordinates into its collision data record.
 * Leaves r0 = panel X, r1 = panel Y, r2 = collision data pointer; preserves r4. */
Next object_setCollisionPanelsToCurrent(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);

    u32 panelX = LD8(R5 + oBattleObject_PanelX);
    ST8(collision + oCollisionData_PanelX, panelX);
    R0 = panelX;

    u32 panelY = LD8(R5 + oBattleObject_PanelY);
    ST8(collision + oCollisionData_PanelY, panelY);
    R1 = panelY;

    R2 = collision;
    R4 = R4; /* r4 is preserved */
    RETURN_TO_CALLER();
}

/* ── object_clearCollisionRegion ── */
/* object_clearCollisionRegion(r5 = battle object):
 * Zero the object's collision region byte (offset 1 in its collision data),
 * leaving the collision data pointer in r0 and 0 in r1
 * (flags set as by "mov r1, #0"). */
Next object_clearCollisionRegion(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    R0 = collisionData;
    R1 = 0;
    ST8(collisionData + 1, R1); /* oCollisionData_Region == 1 */

    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── object_setCollisionRegion ── */
/* object_setCollisionRegion(r0 = region index, r5 = battle object):
 * Write the region index into the object's collision data
 * (offset oCollisionData_Region == 1, no macro is defined for it).
 * Returns r1 = the object's collision data pointer. */
Next object_setCollisionRegion(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST8(collisionData + 1 /* oCollisionData_Region */, R0);

    R1 = collisionData;
    RETURN_TO_CALLER();
}

/* ── object_getTableEntry_801A0BA ── */
/* object_getTableEntry_801A0BA(r0 = column index, r1 = row index):
 * fetch one 32-bit entry from the two-dimensional table at byte_8019C7C
 * (8 bytes per row, 4 bytes per entry). Leaves r0 = table entry,
 * r1 = byte offset of the entry within the table, r3 = table base.
 * The C/V flags come from the offset addition, as in the original. */
Next object_getTableEntry_801A0BA(void) {
    u32 column = R0;
    u32 row = R1;

    R0 = column << 2;            /* byte offset of the entry within its row */
    R3 = A_byte_8019C7C;         /* base of the lookup table */
    R1 = row << 3;               /* byte offset of the row */

    /* add r1, r1, r0 — flags come from the original operand values */
    SET_FLAGS_ADD(R1, R0);
    R1 = R1 + R0;                /* absolute offset of the entry in the table */

    R0 = LD32(R3 + R1);          /* the table entry itself */

    RETURN_TO_CALLER();
}

/* ── object_setCollisionHitEffect ── */
/* object_setCollisionHitEffect(r0 = hit effect, r5 = battle object):
 * store the hit-effect type into the object's collision data.
 * r3 = collision data pointer. */
Next object_setCollisionHitEffect(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    R3 = collisionData;
    ST8(collisionData + 9, R0); /* offset 9 = oCollisionData_HitEffect */

    RETURN_TO_CALLER();
}

/* ── object_setFlag1 ── */
/* object_setFlag1(r0 = flag bits, r5 = battle object):
 * OR the given bits into the object's CollisionData flags1 word.
 * Returns: r1 = collision data pointer, r2 = updated flags word, nz = flags word. */
Next object_setFlag1(void) {
    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags1) | R0;
    ST32(R1 + oCollisionData_ObjectFlags1, R2);
    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── object_clearFlag ── */
/* object_clearFlag(r0 = flag mask, r5 = battle object):
 * clear the given bits of the object's ObjectFlags1 in its collision data.
 * Returns r1 = collision data pointer, r2 = updated flags; NZ set on the flags value. */
Next object_clearFlag(void) {
    u32 flagsMask = R0;

    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags1);
    R2 &= ~flagsMask;
    SET_FLAGS_NZ(R2);
    ST32(R1 + oCollisionData_ObjectFlags1, R2);

    RETURN_TO_CALLER();
}

/* ── object_getFlag ── */
/* object_getFlag(r5 = battle object):
 * returns the object's first collision flag word from its collision data. */
Next object_getFlag(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    R0 = LD32(collisionData + oCollisionData_ObjectFlags1);
    RETURN_TO_CALLER();
}

/* ── object_setFlag2 ── */
/* object_setFlag2(r0 = flag bits, r5 = battle object):
 * set the given bits in the object's "flags 2" field of its collision data.
 * Returns r1 = collision data pointer, r2 = updated flags, and NZ from the ORR. */
Next object_setFlag2(void) {
    u32 flags = R0;

    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags2) | flags;
    ST32(R1 + oCollisionData_ObjectFlags2, R2);

    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── object_clearFlag2 ── */
/* object_clearFlag2(r0 = flag bits to clear, r5 = battle object):
 * Clears the given bits in the object's ObjectFlags2 field.
 * Leaves r1 = collision data pointer, r2 = new flags value, NZ from that value. */
Next object_clearFlag2(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 flags = LD32(collision + oCollisionData_ObjectFlags2) & ~R0;

    ST32(collision + oCollisionData_ObjectFlags2, flags);

    R1 = collision;
    R2 = flags;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── object_getFlag2 ── */
/* object_getFlag2: read the ObjectFlags2 field of the object's collision data.
 * Inputs:  r5 = battle object
 * Outputs: r0 = ObjectFlags2 */
Next object_getFlag2(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R0 = LD32(collision + oCollisionData_ObjectFlags2);
    RETURN_TO_CALLER();
}

/* ── object_setCollisionStatusEffect1 ── */
/* object_setCollisionStatusEffect1: 0801A258-0801A25E THUMB
 * Sets the status effect byte on the collision data attached to a battle object.
 *   inputs:  r0 = status effect base value, r5 = battle object pointer
 *   outputs: r3 = pointer to the object's collision data
 *   clobbers/preserved: - */
Next object_setCollisionStatusEffect1(void) {
    R3 = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST8(R3 + oCollisionData_StatusEffectBase, R0);
    RETURN_TO_CALLER();
}

/* ── battle_clearObjectStatusFlagsAndTimers_801A264 ── */
/* battle_clearObjectStatusFlagsAndTimers_801A264(r5 = battle object): clear the object's active status flags
 * (both flag words) and reset all of its collision timers (blind, etc.).
 * Leaves r0 = 0, r3 = pointer to the collision data, and Z set (from r0 = 0). */
Next battle_clearObjectStatusFlagsAndTimers_801A264(void) {
    /* Clear status bits in the object's two flag words. */
    R0 = 0x8001E800;
    GUEST_CALL(object_clearFlag);

    R0 = 0x000300E8;
    GUEST_CALL(object_clearFlag2);

    R0 = 0;
    SET_FLAGS_NZ(R0);

    /* Zero out every timer field of the collision data. */
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    ST16(collision + oCollisionData_StatusEffectTimer1, 0);
    ST16(collision + oCollisionData_StatusEffectTimer, 0);
    ST16(collision + oCollisionData_BlindTimer, 0);
    ST16(collision + oCollisionData_StatusEffectTimer8, 0);
    ST16(collision + oCollisionData_HitInvulTimer, 0);
    ST16(collision + oCollisionData_HitStunTimer, 0);

    RETURN_TO_CALLER();
}

/* ── battle_object_clearFlagsAndCollisionCounter_801A284 ── */
/* battle_object_clearFlagsAndCollisionCounter_801A284(r5 = battle object):
 * Clear the object's 0x800 status flag and its secondary flag 8, then zero
 * the halfword at oCollisionData_StatusEffectTimer1 of its collision data.
 * Returns r3 = pointer to the object's collision data. */
Next battle_object_clearFlagsAndCollisionCounter_801A284(void) {
    R0 = 0x800;
    GUEST_CALL(object_clearFlag);

    R0 = 8;
    GUEST_CALL(object_clearFlag2);

    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collision + oCollisionData_StatusEffectTimer1, 0);
    R3 = collision;

    RETURN_TO_CALLER();
}

/* ── battleObject_clearHitFlags_801A29A ── */
/* battleObject_clearHitFlags_801A29A(r5 = battle object): clear the object's flag bits 0x10000 in
 * both flag words and reset the halfword at oCollisionData_HitInvulTimer of its
 * collision data. Leaves r0 = 0 (flags updated accordingly). */
Next battleObject_clearHitFlags_801A29A(void) {
    R0 = 0x10000;
    GUEST_CALL(object_clearFlag);

    R0 = 0x10000;
    GUEST_CALL(object_clearFlag2);

    R0 = 0;
    R3 = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(R3 + oCollisionData_HitInvulTimer, R0);

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battleObject_resetCollisionState_801A2B0 ── */
/* battleObject_resetCollisionState_801A2B0(r5 = battle object):
 * Reset an object's collision state: clear its status flag bits, zero the
 * collision timer, and restore its Z coordinate from its AI data block.
 * Leaves r0 = restored Z value, r3 = collision data pointer. */
Next battleObject_resetCollisionState_801A2B0(void) {
    /* Clear the object's main and secondary status flags. */
    R0 = 0x80000000;
    GUEST_CALL(object_clearFlag);
    R0 = 0x20000;
    GUEST_CALL(object_clearFlag2);

    /* Zero the collision data timer at offset 0x2c. */
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collisionData + oCollisionData_HitStunTimer, 0);

    /* Restore the object's Z position from the AI data block. */
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 z = LD16(aiData + oAIData_SavedZ16);
    ST16(R5 + oBattleObject_Z16, z);

    R3 = collisionData;
    R0 = z;
    RETURN_TO_CALLER();
}

/* ── battle_toggleAllianceAndAccumulateCollisionToAiData_801A308 ── */
/* battle_toggleAllianceAndAccumulateCollisionToAiData_801A308 (r5 = battle object):
 * Flip the object's alliance (player <-> enemy), fetch the AI data block for
 * the new side via battle_getPlayerAIData_8010022, then add the collision halfword at
 * CollisionData+0x92 into the AI's byte at oAIData_CounterHealDamage.
 * Preserves r4/r5/r6; returns nothing. */
Next battle_toggleAllianceAndAccumulateCollisionToAiData_801A308(void) {
    u32 savedR4 = R4;
    u32 object = R5;

    /* Toggle the alliance field and get the matching AI data pointer. */
    R0 = LD8(object + oBattleObject_Alliance) ^ 1;
    GUEST_CALL(battle_getPlayerAIData_8010022);
    u32 aiData = R0;

    /* Accumulate the collision value into the AI data byte. */
    u32 collisionData = LD32(object + oBattleObject_CollisionDataPtr);
    u32 collisionValue = LD16(collisionData + 0x92);
    ST8(aiData + oAIData_CounterHealDamage, LD8(aiData + oAIData_CounterHealDamage) + collisionValue);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_spawnHitEffectAtArm_801A42E ── */
/* battle_spawnHitEffectAtArm_801A42E(r5 = battle object):
 * While the object is in a hit/collision state (collision timer byte at
 * +0x74 and hit-flash counter at +0x80 of its collision data are both
 * non-zero), compute the object's arm position and call battle_spawnObject6B_linkRelated_80E8124 to
 * spawn/draw the associated effect there.
 * Preserves r4-r7; r3 on return is whatever battle_spawnObject6B_linkRelated_80E8124 left (or the
 * incoming r3 when it returns early). */
Next battle_spawnHitEffectAtArm_801A42E(void) {
    u32 savedR4 = R4;

    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    if (LD8(collisionData + 0x74) != 0 && LD16(collisionData + 0x80) != 0) {
        R0 = LD16(R5 + oBattleObject_NameID);
        R1 = 5;
        R2 = LD8(R5 + oBattleObject_Alliance);
        R3 = LD8(R5 + oBattleObject_DirectionFlip);
        GUEST_CALL(getBattleArmPositionMaybe_8018810);

        u32 posX = R0;
        u32 posY = R1;

        R0 = posX;
        R1 = posX << 16;
        R2 = 0;
        R3 = posY << 16;
        R4 = 0;
        GUEST_CALL(battle_spawnObject6B_linkRelated_80E8124);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_handleCollisionCrouchSlideReset_801A45C ── */
/* ---------------------------------------------------------------------------
 * battle_handleCollisionCrouchSlideReset_801A45C: counter-hit resolution for one battle object
 * ------------------------------------------------------------------------- */

/* Collision flag 0x40: this object was struck inside its counter window. */
#define COLLISION_FLAG_COUNTER_HIT 0x40

/* Popup id handed to battle_addCappedCounter for the counter-hit callout. */
#define POPUP_COUNTER_HIT 0x1500

/* Per-side stat-table entry that tallies counter hits. */
#define TALLY_COUNTER_HITS 8

/* r4/r6 belong to the caller's frame (the original saves them with push {r4,r6});
 * the typed function hands the entry values back so the adapter can restore them. */
typedef struct {
    u32 saved_r4;
    u32 saved_r6;
} counter_hit_result_t;

/* register form: the original's plain `mov r2, #0x40` on the early-out path is a
 * register output (r2) that callers observe; no game function is involved and a C
 * local cannot become guest r2, so write the output register directly */
static void set_output_r2(u32 value) {
    R2 = value;
}

/* register form: calls battle_testFlag0x40_800A8F8 outside this file, which has no
 * typed C interface yet. It takes the flag mask in r2 and also reads the caller's
 * live context (object r5, collision data r4, collision flags r6). */
static bool battle_flag_0x40_set(u32 obj, u32 collision, u32 flags) {
    R2 = 0x40;
    R4 = collision;
    R5 = obj;
    R6 = flags;
    GUEST_CALL(battle_testFlag0x40_800A8F8);
    return R0 != 0;
}

/* register form: calls battle_addCappedCounter outside this file, which has no typed C interface
 * yet. Besides (side, popup id) in r0/r1 it consumes the context registers r4-r6. */
static void show_counter_hit_popup(u32 side, u32 obj, u32 collision, u32 flags) {
    R0 = side;
    R1 = POPUP_COUNTER_HIT;
    R4 = collision;
    R5 = obj;
    R6 = flags;
    GUEST_CALL(battle_addCappedCounter);
}

/* register form: calls tableAddSaturatedByte_800AB46 outside this file, which has no
 * typed C interface yet. (side, entry, delta) go in r0-r2 and the context in r4-r6. */
static void bump_side_tally(u32 side, u32 entry, u32 delta, u32 obj, u32 collision, u32 flags) {
    R0 = side;
    R1 = entry;
    R2 = delta;
    R4 = collision;
    R5 = obj;
    R6 = flags;
    GUEST_CALL(tableAddSaturatedByte_800AB46);
}

/* register form: calls battle_isBattleOver outside this file, which has no typed C
 * interface yet (it reads no argument registers) */
static bool battle_is_over(void) {
    GUEST_CALL(battle_isBattleOver);
    return R0 != 0;
}

/* register form: calls sub_801E270 outside this file, which has no typed C interface
 * yet; it takes the counter-hit event entirely from the context registers r4-r6
 * (object, collision data, collision flags) instead of r0-r3 */
static void run_counter_hit_event(u32 obj, u32 collision, u32 flags) {
    R4 = collision;
    R5 = obj;
    R6 = flags;
    GUEST_CALL(sub_801E270);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C
 * interface yet */
static void play_sound(u32 sound) {
    R0 = sound;
    GUEST_CALL(PlaySoundEffect);
}

/* Resolve a counter hit on `obj`: bit 0x40 appeared in its collision flags, i.e. it
 * was struck while attacking. While global battle flag 0x40 is set, show the
 * counter-hit popup on the opposing side; either way credit that side's counter
 * tally and restart this object's counter timer. If the battle is still running,
 * fire the counter-hit event and play the select sound.
 * saved_r4/saved_r6 are the caller's register values that must survive (push {r4,r6}). */
static counter_hit_result_t resolve_counter_hit(u32 obj, u32 saved_r4, u32 saved_r6) {
    counter_hit_result_t result = { saved_r4, saved_r6 };

    u32 collision = LD32(obj + oBattleObject_CollisionDataPtr);
    u32 flags = LD32(collision + oCollisionData_FlagsFromCollision);

    if (!(flags & COLLISION_FLAG_COUNTER_HIT)) {
        set_output_r2(0x40);        /* the original leaves r2 = 0x40 when bailing out here */
        return result;
    }

    if (battle_flag_0x40_set(obj, collision, flags))
        show_counter_hit_popup(LD8(obj + oBattleObject_Alliance) ^ 1, obj, collision, flags);

    /* Re-read the alliance: the popup spawn ran in between, exactly as the original does. */
    bump_side_tally(LD8(obj + oBattleObject_Alliance) ^ 1, TALLY_COUNTER_HITS, 1,
                    obj, collision, flags);

    ST8(collision + oCollisionData_CounterTimer, 0);

    if (battle_is_over())
        return result;

    /* The original keeps sub_801E270's r0 alive across the sound call (push/pop {r0}),
     * but r0 is a clobbered register, so it is not reproduced here. */
    run_counter_hit_event(obj, collision, flags);
    play_sound(SOUND_SELECT_86);
    return result;
}

/* battle_handleCollisionCrouchSlideReset_801A45C(r5 = battle object) -> r2 (0x40 on early out, else callee leftover),
 * r4/r6 restored, r5/r7-r12 untouched here and left as the callees dropped them. */
Next battle_handleCollisionCrouchSlideReset_801A45C(void) {
    counter_hit_result_t result = resolve_counter_hit(R5, R4, R6);
    R4 = result.saved_r4;
    R6 = result.saved_r6;
    RETURN_TO_CALLER();
}

/* ── battle_spawnArmHitEffect_801A4A6 ── */
/* battle_spawnArmHitEffect_801A4A6(r5 = battle object):
 * Checks the object's collision data byte at offset 0xA4; when it reports
 * hit type 0xF4 or 0xF7, looks up where the arm attack connects for this
 * object and hands the result to the battle-effect helper battle_spawnObject6B_linkRelated_80E8124.
 * Returns with r3 and the c/v flags as left by battle_spawnObject6B_linkRelated_80E8124; r4-r7 preserved. */
Next battle_spawnArmHitEffect_801A4A6(void) {
    u32 savedR4 = R4;

    u32 hitType = LD8(LD32(R5 + oBattleObject_CollisionDataPtr) + 0xA4);
    if (hitType != 0xF4 && hitType != 0xF7) {
        /* No arm contact: leave with the flags of the original's last CMP. */
        SET_FLAGS_CMP(hitType, 0xF7);
        RETURN_TO_CALLER();
    }

    /* Ask where the arm position lands for this object's name/alliance/facing. */
    R0 = LD16(R5 + oBattleObject_NameID);
    R1 = 5;
    R2 = LD8(R5 + oBattleObject_Alliance);
    R3 = LD8(R5 + oBattleObject_DirectionFlip);
    GUEST_CALL(getBattleArmPositionMaybe_8018810);

    /* Pass the position on: r0 stays as returned, r1 = position << 16,
     * r3 = other result << 16, r2 = 0, r4 = 3. */
    {
        u32 position = R0;
        u32 other = R1;
        R1 = position << 16;
        R2 = 0;
        R3 = other << 16;
        R4 = 3;
        GUEST_CALL(battle_spawnObject6B_linkRelated_80E8124); /* updates r3 and the c/v flags */
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_ai_markWindBoxReady_801A506 ── */
/* register form: calls SetAIData_Unk_44_Flag outside this file, which has no typed C interface yet */
static void set_ai_data_unk44_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(SetAIData_Unk_44_Flag);
}

/* If the battle object's collision data has both a non-zero byte at 0x75 and a non-zero
 * halfword at 0x80, set the AI data's unk_44 flag to 0x80000000. Returns the collision
 * data pointer (R3) — the original always loads it before any branch, so callers see the
 * same value either way. */
static u32 maybe_set_ai_unk44_flag(u32 battle_object_ptr) {
    u32 collision_data = LD32(battle_object_ptr + oBattleObject_CollisionDataPtr);
    u8  byte_75 = LD8(collision_data + 0x75);
    if (byte_75 == 0) return collision_data;
    u16 half_80 = LD16(collision_data + 0x80);
    if (half_80 == 0) return collision_data;
    set_ai_data_unk44_flag(0x80000000);
    return collision_data;
}

Next battle_ai_markWindBoxReady_801A506(void) {
    R3 = maybe_set_ai_unk44_flag(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_applyStatusEffect_801A554 ── */
/* battleObject_applyStatusEffect_801A554: apply the battle object's current status effect (r5 = object).
 *
 * The status effect byte in the object's collision data selects an entry from
 * the status effect table (off_80209EC): the high nibble picks a sub-table,
 * the low nibble picks an 8-byte entry within it. Each entry describes one
 * field patch plus one object flag:
 *   +0: word  - flag value to set on the object (object_setFlag2)
 *   +4: halfword - value to write into the collision data
 *   +6: byte  - offset of the field in the collision data to patch
 *
 * Status effects 0x50-0x55 additionally clear flag value 6 afterwards.
 * Outputs: r2 as left by the last flag helper called (or untouched if the
 * object has no status effect). */
Next battleObject_applyStatusEffect_801A554(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 status = LD8(collisionData + oCollisionData_StatusEffectFinal);

    if (status != 0) {
        u32 entry = LD32(A_off_80209EC + ((status >> 4) - 1) * 4)
                  + (status & 0xF) * 8;
        u16 patchValue = LD16(entry + 4);
        u8 patchOffset = LD8(entry + 6);
        u32 flagValue = LD32(entry);

        ST16(collisionData + patchOffset, patchValue);

        /* Register state at the call mirrors the original: r1 still holds the
         * patched value, r2 the table entry, r3 the collision data. */
        R1 = patchValue;
        R2 = entry;
        R3 = collisionData;
        R0 = flagValue;
        GUEST_CALL(object_setFlag2);

        if ((s32)status >= 0x50 && (s32)status <= 0x55) {
            R0 = 6;
            GUEST_CALL(object_clearFlag2);
        }
    }
    RETURN_TO_CALLER();
}

/* ── battle_ClearObjectCollisionResult_801A5E2 ── */
/* battle_ClearObjectCollisionResult_801A5E2(r5 = battle object):
 * Reset the object's collision result: clear the collision data fields at
 * offsets 0x48 and 0x4C (the field at 0x4C directly follows the one at 0x48). */
Next battle_ClearObjectCollisionResult_801A5E2(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST32(collisionData + oCollisionData_LastHitObjectPtr, 0);
    ST32(collisionData + oCollisionData_LastHitObjectPtr + 4, 0); /* offset 0x4C */

    RETURN_TO_CALLER();
}

/* ── battle_clearInvisFlashTimer_801A648 ── */
/* battle_clearInvisFlashTimer_801A648(r5 = battle object, r10 = battle state used by battle_isPaused):
 * Tick-down helper for the "flashing invisibility" a fighter gets after
 * being hit: while the battle is unpaused, if the object's invisibility
 * flash timer is running, the hit flag (0x4) is set and the persistent
 * invisibility flag (0x1000) is not, clear the timer so the flashing stops.
 * Returns r3 = the object's collision data pointer (or the incoming r3 if
 * the battle was paused and nothing was touched). */
Next battle_clearInvisFlashTimer_801A648(void) {
    GUEST_CALL(battle_isPaused); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG())
        RETURN_TO_CALLER();

    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    u32 flashTimer = LD16(collision + oCollisionData_FlashingInvisTimer);
    u32 hitFlags = LD32(collision + oCollisionData_FlagsFromCollision);

    if (flashTimer != 0 && (hitFlags & 0x4) && !(hitFlags & 0x1000))
        ST16(collision + oCollisionData_FlashingInvisTimer, 0);

    RETURN_TO_CALLER();
}

/* ── battle_resetFlashingInvisTimer_801A67E ── */
/* battle_resetFlashingInvisTimer_801A67E(r5 = battle object):
 * Reset the object's "flashing while invisible" timer to zero.
 * Returns the collision data pointer in r3. */
Next battle_resetFlashingInvisTimer_801A67E(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collision + oCollisionData_FlashingInvisTimer, 0);
    R3 = collision;
    RETURN_TO_CALLER();
}

/* ── battle_incrementAllianceStatOnCollisionHit_801A6B4 ── */
/* battle_incrementAllianceStatOnCollisionHit_801A6B4(r5 = battle object):
 * Check the object's collision data (battle_checkHPDigit4State_801A6E8); if it reports a hit,
 * increment the navi stat byte at offset 0x18 of the object's alliance
 * stats, capped at 7. R4 (the collision data pointer) is restored on return. */
Next battle_incrementAllianceStatOnCollisionHit_801A6B4(void) {
    u32 savedR4 = R4;

    R4 = LD32(R5 + oBattleObject_CollisionDataPtr);
    GUEST_CALL(battle_checkHPDigit4State_801A6E8);

    if (R0 != 0) {
        R1 = 0x18;
        GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

        u32 level = R0 + 1;
        if ((s32)level > 7)
            level = 7;

        R1 = 0x18;
        R2 = level;
        GUEST_CALL(SetBattleNaviStatsByte_AllianceFromBattleObject);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_objectDieOnCollisionHit_801A6D6 ── */
/* battle_objectDieOnCollisionHit_801A6D6(r5 = battle object):
 * Check the object's collision data (passed to battle_checkHPDigit4State_801A6E8 in r4); if it
 * reports a hit, the object's HP is zeroed. r4 is saved and restored around
 * the call, r7 passes through untouched. */
Next battle_objectDieOnCollisionHit_801A6D6(void) {
    u32 savedR4 = R4;

    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    R4 = collisionData;

    GUEST_CALL(battle_checkHPDigit4State_801A6E8);

    if (R0 != 0)
        ST16(R5 + oBattleObject_HP, 0);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_checkHPDigit4State_801A6E8 ── */
/* battle_checkHPDigit4State_801A6E8: check the custom-panel state byte at +0xA4 of the object in R4.
 * - 0xF4 (panel flagged): return 1.
 * - 0xF7 (panel awaiting check): convert R5's HP to BCD and look for a "4"
 *   digit (a bug/frag digit appearing in the HP readout). If one is found,
 *   return 1; otherwise clear the state byte and return 0.
 * - anything else: return 0.
 * R0 = result; R2/R3 and R4/R5/R7 are left as the original leaves them. */
Next battle_checkHPDigit4State_801A6E8(void) {
    enum { STATE_BYTE = 0xA4, PANEL_FLAGGED = 0xF4, PANEL_PENDING = 0xF7 };

    u32 state = LD8(R4 + STATE_BYTE);
    u32 result = 0;

    if (state == PANEL_PENDING) {
        R0 = LD16(R5 + oBattleObject_HP);
        GUEST_CALL(memory_bcdConvert);
        u32 bcd = R0;

        u32 hasFour = 0;
        while (bcd != 0) {
            if ((bcd & 0xF) == 4)
                hasFour = 1;
            bcd >>= 4;
        }

        result = hasFour;
        if (hasFour == 0)
            ST8(R4 + STATE_BYTE, 0); /* no 4-digit: reset the panel state */
    } else if (state == PANEL_FLAGGED) {
        result = 1;
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── battle_buffTwoStatsPlus2OnEventF6_801A720 ── */
/* battle_buffTwoStatsPlus2OnEventF6_801A720(r5 = battle object):
 * If the object's collision slot 0xA4 holds event 0xF6, raise the navi stats
 * at offsets 0x18 and 0x19 by 2 (capped at 7) and then run battle_applyPanelEventF6Status.
 * Flags c/v are left over from the collision comparison on the early-exit
 * path, and from battle_applyPanelEventF6Status on the normal path. */
static u32 stat_plus2_capped(u32 value)
{
    u32 raised = value + 2;
    if ((s32)raised > 7) /* original compares signed and keeps the sum if <= 7 */
        raised = 7;
    return raised;
}

Next battle_buffTwoStatsPlus2OnEventF6_801A720(void) {
    u32 savedR4 = R4; /* original pushes/pops r4, so it must come out unchanged */
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 eventType = LD8(collisionData + 0xA4);

    if (eventType != 0xF6) {
        SET_FLAGS_CMP(eventType, 0xF6);
        RETURN_TO_CALLER();
    }

    /* bump stat 0x18 by 2, capped at 7 */
    R1 = 0x18;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    R2 = stat_plus2_capped(R0);
    R1 = 0x18;
    GUEST_CALL(SetBattleNaviStatsByte_AllianceFromBattleObject);

    /* bump stat 0x19 by 2, capped at 7 */
    R1 = 0x19;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    R2 = stat_plus2_capped(R0);
    R1 = 0x19;
    GUEST_CALL(SetBattleNaviStatsByte_AllianceFromBattleObject);

    /* battle_applyPanelEventF6Status may clobber r2/r4; r2 is part of our output, r4 is restored */
    GUEST_CALL(battle_applyPanelEventF6Status);
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A ── */
/* battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A(r2 = column index, r4 = caller value (preserved), r5 = battle object):
 * When the object's collision data says it is standing on a specific kind of panel
 * (byte at +0xA4 equal to 0xF6), aim the AI's Unk_12 marker two columns ahead,
 * saturating at 7, and run battle_applyPanelEventF6Status with the AI data / collision data / object
 * pointers. Otherwise just fall through, leaving the compare flags behind for the
 * caller. r2 (and c/v when the callee runs) come back from battle_applyPanelEventF6Status. */
Next battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A(void) {
    u32 saved_r4 = R4;
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 collisionByte = LD8(collisionData + 0xA4);

    if (collisionByte == 0xF6) {
        u32 target;
        R3 = LD32(R5 + oBattleObject_AIDataPtr);
        /* The original reads the old Unk_12 value, but it is dead: r0 is
         * immediately overwritten by the add below. */
        target = R2 + 2;
        if ((s32)target > 7)
            target = 7;
        ST8(R3 + oAIData_CounterHitChain, target);
        R4 = collisionData;      /* battle_applyPanelEventF6Status expects the collision data pointer in r4 */
        GUEST_CALL(battle_applyPanelEventF6Status);
        R4 = saved_r4;           /* restore the caller's r4, as the original's pop does */
    } else {
        SET_FLAGS_CMP(collisionByte, 0xF6);
    }
    RETURN_TO_CALLER();
}

/* ── battle_clearAIDataAndBarrier_801A7F4 ── */
/* battle_clearAIDataAndBarrier_801A7F4(r5 = battle object):
 * Clear the object's AI state field at oAIData_SubordinateObjectPtr and drop its
 * barrier flag (offset 6 in its collision data). */
Next battle_clearAIDataAndBarrier_801A7F4(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST32(aiData + oAIData_SubordinateObjectPtr, 0);
    ST8(collision + 6, 0);

    RETURN_TO_CALLER();
}

/* ── battleObject_processCollisionResultsForPlayer_801AC6C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool c, v; } call_sprite_clearFinalPalette_result;
/* register form: generated wrapper: sprite_clearFinalPalette has no typed C interface yet */
static inline call_sprite_clearFinalPalette_result call_sprite_clearFinalPalette(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_clearFinalPalette);
    return (call_sprite_clearFinalPalette_result){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: battle_getFlags has no typed C interface yet */
static inline u32 call_battle_getFlags(void) {
    GUEST_CALL(battle_getFlags);
    return R0;
}

typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_removeCollisionData_result;
/* register form: generated wrapper: object_removeCollisionData has no typed C interface yet */
static inline call_object_removeCollisionData_result call_object_removeCollisionData(u32 r4, u32 r5, u32 r7) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(object_removeCollisionData);
    return (call_object_removeCollisionData_result){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r3; } call_battle_isBattleOver_result;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result call_battle_isBattleOver(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result){ R0, R3 };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

typedef struct { u32 r2, r4, r6; } call_sub_801A802_result;
/* register form: generated wrapper: battleObject_updateBarrierStatus_801A802 has no typed C interface yet */
static inline call_sub_801A802_result call_sub_801A802(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_updateBarrierStatus_801A802);
    return (call_sub_801A802_result){ R2, R4, R6 };
}

typedef struct { u32 r2, r7; } call_sub_801A186_result;
/* register form: generated wrapper: object_updatePoisonPanelStatus_801A186 has no typed C interface yet */
static inline call_sub_801A186_result call_sub_801A186(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updatePoisonPanelStatus_801A186);
    return (call_sub_801A186_result){ R2, R7 };
}

typedef struct { u32 r2, r4, r7; } call_sub_801A36A_result;
/* register form: generated wrapper: object_tickIcePanelFreeze_801A36A has no typed C interface yet */
static inline call_sub_801A36A_result call_sub_801A36A(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_tickIcePanelFreeze_801A36A);
    return (call_sub_801A36A_result){ R2, R4, R7 };
}

typedef struct { u32 r6; } call_battle_tickAiSelfDrain_8010230_result;
/* register form: generated wrapper: battle_tickAiSelfDrain_8010230 has no typed C interface yet */
static inline call_battle_tickAiSelfDrain_8010230_result call_battle_tickAiSelfDrain_8010230(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_tickAiSelfDrain_8010230);
    return (call_battle_tickAiSelfDrain_8010230_result){ R6 };
}

typedef struct { u32 r5, r6, r7, r10; } call_battleObject_handleImpact_802CFF8_result;
/* register form: generated wrapper: battleObject_handleImpact_802CFF8 has no typed C interface yet */
static inline call_battleObject_handleImpact_802CFF8_result call_battleObject_handleImpact_802CFF8(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_handleImpact_802CFF8);
    return (call_battleObject_handleImpact_802CFF8_result){ R5, R6, R7, R10 };
}

typedef struct { u32 r5, r6, r7, r10; } call_hiveBlockHappensHere_802CEF4_result;
/* register form: generated wrapper: hiveBlockHappensHere_802CEF4 has no typed C interface yet */
static inline call_hiveBlockHappensHere_802CEF4_result call_hiveBlockHappensHere_802CEF4(u32 r5) {
    R5 = r5;
    GUEST_CALL(hiveBlockHappensHere_802CEF4);
    return (call_hiveBlockHappensHere_802CEF4_result){ R5, R6, R7, R10 };
}

typedef struct { u32 r6, r7; } call_battle_incrementAllianceStatOnCollisionHit_801A6B4_result;
/* register form: generated wrapper: battle_incrementAllianceStatOnCollisionHit_801A6B4 has no typed C interface yet */
static inline call_battle_incrementAllianceStatOnCollisionHit_801A6B4_result call_battle_incrementAllianceStatOnCollisionHit_801A6B4(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_incrementAllianceStatOnCollisionHit_801A6B4);
    return (call_battle_incrementAllianceStatOnCollisionHit_801A6B4_result){ R6, R7 };
}

typedef struct { u32 r6, r7; } call_battle_buffTwoStatsPlus2OnEventF6_801A720_result;
/* register form: generated wrapper: battle_buffTwoStatsPlus2OnEventF6_801A720 has no typed C interface yet */
static inline call_battle_buffTwoStatsPlus2OnEventF6_801A720_result call_battle_buffTwoStatsPlus2OnEventF6_801A720(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_buffTwoStatsPlus2OnEventF6_801A720);
    return (call_battle_buffTwoStatsPlus2OnEventF6_801A720_result){ R6, R7 };
}

typedef struct { u32 r5, r7, r8, r9, r10, r11, r12; } call_sub_80139F6_result;
/* register form: generated wrapper: battleObject_applyStatusEffectOnTurnEnd_80139F6 has no typed C interface yet */
static inline call_sub_80139F6_result call_sub_80139F6(u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r11, u32 r12) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R8 = r8;
    R9 = r9;
    R11 = r11;
    R12 = r12;
    GUEST_CALL(battleObject_applyStatusEffectOnTurnEnd_80139F6);
    return (call_sub_80139F6_result){ R5, R7, R8, R9, R10, R11, R12 };
}

/* register form: generated wrapper: battleObject_applyHitModifierFlags_801AEB0 has no typed C interface yet */
static inline void call_sub_801AEB0(u32 r5, u32 r7) {
    R5 = r5;
    R7 = r7;
    GUEST_CALL(battleObject_applyHitModifierFlags_801AEB0);
}

/* register form: generated wrapper: battle_applyFlattenHitStatus_800EB26 has no typed C interface yet */
static inline void call_battle_applyFlattenHitStatus_800EB26(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_applyFlattenHitStatus_800EB26);
}

typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_handlePanelDamage_8013F1E_result;
/* register form: generated wrapper: handlePanelDamage_8013F1E has no typed C interface yet */
static inline call_handlePanelDamage_8013F1E_result call_handlePanelDamage_8013F1E(u32 r5) {
    R5 = r5;
    GUEST_CALL(handlePanelDamage_8013F1E);
    return (call_handlePanelDamage_8013F1E_result){ R5, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r0, r6; } call_GetBattleNaviStatsByte_AllianceFromBattleObject_result;
/* register form: generated wrapper: GetBattleNaviStatsByte_AllianceFromBattleObject has no typed C interface yet */
static inline call_GetBattleNaviStatsByte_AllianceFromBattleObject_result call_GetBattleNaviStatsByte_AllianceFromBattleObject(u32 r1, u32 r5) {
    R1 = r1;
    R5 = r5;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    return (call_GetBattleNaviStatsByte_AllianceFromBattleObject_result){ R0, R6 };
}

/* register form: generated wrapper: battleObject_applyStatusEffect_801A554 has no typed C interface yet */
static inline void call_battleObject_applyStatusEffect_801A554(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_applyStatusEffect_801A554);
}

/* register form: generated wrapper: battleObject_clearChipSelectionOnAirShoe_801A2CC has no typed C interface yet */
static inline void call_sub_801A2CC(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_clearChipSelectionOnAirShoe_801A2CC);
}

typedef struct { u32 r5, r10; } call_sub_801A324_result;
/* register form: generated wrapper: battle_applyCounterHealAndSpawnEffect_801A324 has no typed C interface yet */
static inline call_sub_801A324_result call_sub_801A324(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_applyCounterHealAndSpawnEffect_801A324);
    return (call_sub_801A324_result){ R5, R10 };
}

typedef struct { u32 r2, r6; bool c, v; } call_object_calculateFinalDamage1_result;
/* register form: generated wrapper: object_calculateFinalDamage1 has no typed C interface yet */
static inline call_object_calculateFinalDamage1_result call_object_calculateFinalDamage1(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_calculateFinalDamage1);
    return (call_object_calculateFinalDamage1_result){ R2, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { bool c, v; } call_sub_801A420_result;
/* register form: generated wrapper: battleObject_tickCounterTimer_801A420 has no typed C interface yet */
static inline call_sub_801A420_result call_sub_801A420(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_tickCounterTimer_801A420);
    return (call_sub_801A420_result){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: battle_runObjectAI_80143FC has no typed C interface yet */
static inline void call_battle_runObjectAI_80143FC(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_runObjectAI_80143FC);
}

typedef struct { u32 r2; } call_sub_80142DC_result;
/* register form: generated wrapper: battleObject_checkAngerTrigger_80142DC has no typed C interface yet */
static inline call_sub_80142DC_result call_sub_80142DC(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_checkAngerTrigger_80142DC);
    return (call_sub_80142DC_result){ R2 };
}

typedef struct { u32 r3; } call_battle_objectClearPendingHitIfCollisionProcessed_8010198_result;
/* register form: generated wrapper: battle_objectClearPendingHitIfCollisionProcessed_8010198 has no typed C interface yet */
static inline call_battle_objectClearPendingHitIfCollisionProcessed_8010198_result call_battle_objectClearPendingHitIfCollisionProcessed_8010198(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_objectClearPendingHitIfCollisionProcessed_8010198);
    return (call_battle_objectClearPendingHitIfCollisionProcessed_8010198_result){ R3 };
}

typedef struct { u32 r3; } call_battle_clearInvisFlashTimer_801A648_result;
/* register form: generated wrapper: battle_clearInvisFlashTimer_801A648 has no typed C interface yet */
static inline call_battle_clearInvisFlashTimer_801A648_result call_battle_clearInvisFlashTimer_801A648(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_clearInvisFlashTimer_801A648);
    return (call_battle_clearInvisFlashTimer_801A648_result){ R3 };
}

typedef struct { u32 r2, r3, r5, r6, r10; bool v; } call_object_spawnHiteffect_result;
/* register form: generated wrapper: object_spawnHiteffect has no typed C interface yet */
static inline call_object_spawnHiteffect_result call_object_spawnHiteffect(u32 r2, u32 r5) {
    R2 = r2;
    R5 = r5;
    GUEST_CALL(object_spawnHiteffect);
    return (call_object_spawnHiteffect_result){ R2, R3, R5, R6, R10, OVERFLOW_FLAG() };
}

/* battleObject_processCollisionResultsForPlayer_801AC6C: per-frame collision resolution for the active battle object.
 *
 * Runs the full hit-response pipeline: clears the sprite palette, loads the
 * collision-data pointer, gates on battle flags / battle-over / object-ignore-
 * collision flag, then applies impact handlers, stat buffs, panel damage, a
 * status-effect guard, final damage, AI tick, and hit-effect spawn.
 *
 * r4 and r7 are internal scratch (pushed/popped in the original) – callers
 * see them unchanged, so the adapter saves and restores them.
 */

typedef struct {
    u32 r5, r8, r9, r10, r11, r12;
} CollisionOutputs;

static CollisionOutputs handle_collision(u32 r4, u32 r5, u32 r6,
                                         u32 r8, u32 r9, u32 r10,
                                         u32 r11, u32 r12) {
    CollisionOutputs out = { r5, r8, r9, r10, r11, r12 };

    /* Refresh the sprite's palette cache. */
    call_sprite_clearFinalPalette(r5);

    /* Grab the collision-data pointer (local scratch in r7). */
    u32 r7 = LD32(r5 + oBattleObject_CollisionDataPtr);

    /* Gate 1: bit 0 of the global battle flags must be set. */
    u32 flags = call_battle_getFlags();
    if (!(flags & 1u))
        return out;

    /* Remove the collision record (updates r5; r6/r7 are scratch). */
    call_object_removeCollisionData_result rm =
        call_object_removeCollisionData(r4, r5, r7);
    out.r5 = rm.r5;
    r6 = rm.r6;
    r7 = rm.r7;

    /* Gate 2: bail if the battle just ended. */
    call_battle_isBattleOver_result over = call_battle_isBattleOver();
    if (over.r0 != 0)
        return out;

    /* Gate 3: skip if the object's ignore-collision flag (bit 8) is set. */
    u32 obj_flag = call_object_getFlag(out.r5);
    if (obj_flag & (1u << 8))
        return out;

    /* --- Impact / buff / stat pipeline (r7 is local scratch throughout) --- */

    call_sub_801A802_result h1 = call_sub_801A802(out.r5);
    r6 = h1.r6;

    call_sub_801A186_result h2 = call_sub_801A186(out.r5);
    r7 = h2.r7;

    call_sub_801A36A_result h3 = call_sub_801A36A(out.r5);
    r7 = h3.r7;

    call_battle_tickAiSelfDrain_8010230_result h4 =
        call_battle_tickAiSelfDrain_8010230(out.r5);
    r6 = h4.r6;

    /* Impact and hive-block handlers (r5, r10 change). */
    call_battleObject_handleImpact_802CFF8_result h5 =
        call_battleObject_handleImpact_802CFF8(out.r5);
    out.r5 = h5.r5; r6 = h5.r6; r7 = h5.r7; out.r10 = h5.r10;

    call_hiveBlockHappensHere_802CEF4_result h6 =
        call_hiveBlockHappensHere_802CEF4(out.r5);
    out.r5 = h6.r5; r6 = h6.r6; r7 = h6.r7; out.r10 = h6.r10;

    /* Alliance-stat increment and +2 buff on event F6. */
    call_battle_incrementAllianceStatOnCollisionHit_801A6B4_result h7 =
        call_battle_incrementAllianceStatOnCollisionHit_801A6B4(out.r5);
    r6 = h7.r6; r7 = h7.r7;

    call_battle_buffTwoStatsPlus2OnEventF6_801A720_result h8 =
        call_battle_buffTwoStatsPlus2OnEventF6_801A720(out.r5);
    r6 = h8.r6; r7 = h8.r7;

    /* Sub-screen collision / stat-update. */
    call_sub_80139F6_result h9 = call_sub_80139F6(
        out.r5, r6, r7, out.r8, out.r9, out.r11, out.r12);
    out.r5 = h9.r5; r7 = h9.r7;
    out.r8 = h9.r8; out.r9 = h9.r9; out.r10 = h9.r10;
    out.r11 = h9.r11; out.r12 = h9.r12;

    call_sub_801AEB0(out.r5, r7);
    call_battle_applyFlattenHitStatus_800EB26(out.r5);

    /* Panel-damage handler (r7 becomes dead after this point). */
    call_handlePanelDamage_8013F1E_result h10 =
        call_handlePanelDamage_8013F1E(out.r5);
    out.r5 = h10.r5; out.r8 = h10.r8; out.r9 = h10.r9;
    out.r10 = h10.r10; out.r11 = h10.r11; out.r12 = h10.r12;

    /* --- Status-effect guard: check three Navi-stat bytes to decide
     *     whether the object is immune / not status-vulnerable. --- */
    bool apply_status = true;

    call_GetBattleNaviStatsByte_AllianceFromBattleObject_result s1 =
        call_GetBattleNaviStatsByte_AllianceFromBattleObject(0x29u, out.r5);
    if (s1.r0 == 7u) {
        apply_status = false;
    } else {
        call_GetBattleNaviStatsByte_AllianceFromBattleObject_result s2 =
            call_GetBattleNaviStatsByte_AllianceFromBattleObject(0x2Cu, out.r5);
        if (s2.r0 == 7u || s2.r0 == 0x13u) {
            apply_status = false;
        } else {
            call_GetBattleNaviStatsByte_AllianceFromBattleObject_result s3 =
                call_GetBattleNaviStatsByte_AllianceFromBattleObject(0x52u, out.r5);
            if (s3.r0 != 0u)
                apply_status = false;
        }
    }
    if (apply_status)
        call_battleObject_applyStatusEffect_801A554(out.r5);

    /* --- Final damage / AI / hit-effect tail --- */
    call_sub_801A2CC(out.r5);

    call_sub_801A324_result h11 = call_sub_801A324(out.r5);
    out.r5 = h11.r5; out.r10 = h11.r10;

    call_object_calculateFinalDamage1_result dmg =
        call_object_calculateFinalDamage1(out.r5);
    u32 r2 = dmg.r2;

    call_sub_801A420(out.r5);
    call_battle_runObjectAI_80143FC(out.r5);

    call_sub_80142DC_result h12 = call_sub_80142DC(out.r5);
    r2 = h12.r2;

    call_battle_objectClearPendingHitIfCollisionProcessed_8010198(out.r5);
    call_battle_clearInvisFlashTimer_801A648(out.r5);

    /* Spawn the hit-effect sprite; r5 and r10 are our outputs. */
    call_object_spawnHiteffect_result hit =
        call_object_spawnHiteffect(r2, out.r5);
    out.r5 = hit.r5; out.r10 = hit.r10;

    return out;
}

Next battleObject_processCollisionResultsForPlayer_801AC6C(void) {
    u32 saved_r4 = R4, saved_r7 = R7;
    CollisionOutputs res = handle_collision(
        saved_r4, R5, R6, R8, R9, R10, R11, R12);
    R5 = res.r5, R8 = res.r8, R9 = res.r9;
    R10 = res.r10, R11 = res.r11, R12 = res.r12;
    R4 = saved_r4, R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── battleObject_applyHitModifierFlags_801AEB0 ── */
/* ------------------------------------------------------------------ */
/* Object status flags used by object_getFlag / object_setFlag2 etc.   */
#define OBJ_FLAG2_STATUS_2     0x000002u
#define OBJ_FLAG2_FLINCH       0x000004u  /* hit-stun flag */
#define OBJ_FLAG2_STATUS_0x10  0x000010u
#define OBJ_FLAG2_STATUS_0x100 0x000100u

/* Target states that ignore parts of a hit. */
#define OBJ_FLAGS_SUPERARMOR_OR_ANGER 0x220000u /* per the original's comment */
#define OBJ_FLAGS_0x100040            0x100040u

/* Bits of CollisionData.hitModifierFinal: what each set bit does to the hit object. */
#define HITMOD_FLINCH       0x01u /* object flinches unless it has superarmor/anger */
#define HITMOD_STATUS_2     0x02u /* sets object status flag 2 */
#define HITMOD_REACTION     0x3Cu /* any of these bits starts a status reaction */
#define HITMOD_ALT_REACTION 0x40u /* picks the flag-0x100 reaction instead of 0x10 */

/* register form: calls object_getFlag outside this file, which has no typed C interface yet */
static u32 object_flags(void) {
    GUEST_CALL(object_getFlag);
    return R0;
}

/* register form: calls object_setFlag2 outside this file, which has no typed C interface yet */
static void object_set_flag2(u32 flag) {
    R0 = flag;
    GUEST_CALL(object_setFlag2);
}

/* register form: calls object_clearFlag2 outside this file, which has no typed C interface yet */
static void object_clear_flag2(u32 flag) {
    R0 = flag;
    GUEST_CALL(object_clearFlag2);
}

/* Apply a collision's final hit modifier to the object that was hit.
 *
 * Each set bit of the hit modifier puts the target into the matching state via
 * its status flags; the "reaction" bits additionally mark the object at
 * oBattleObject_ContactState so the rest of the battle code knows the reaction fired.
 */
static void apply_hit_modifiers(u32 object, u32 collision) {
    u32 hitModifier = LD8(collision + oCollisionData_HitModifierFinal);
    u32 flags = object_flags();

    /* Flinching is ignored while the target has superarmor or anger. */
    if (!(flags & OBJ_FLAGS_SUPERARMOR_OR_ANGER) && (hitModifier & HITMOD_FLINCH))
        object_set_flag2(OBJ_FLAG2_FLINCH);

    if (hitModifier & HITMOD_STATUS_2)
        object_set_flag2(OBJ_FLAG2_STATUS_2);

    /* No reaction bits set: nothing further to do. */
    if (!(hitModifier & HITMOD_REACTION))
        return;

    if (hitModifier & HITMOD_ALT_REACTION) {
        /* Alternate reaction: take flag 0x100 and drop the flinch flag again. */
        object_set_flag2(OBJ_FLAG2_STATUS_0x100);
        object_clear_flag2(OBJ_FLAG2_FLINCH);
        ST8(object + oBattleObject_ContactState, 1); /* original stores this twice */
    } else if (!(object_flags() & OBJ_FLAGS_0x100040)) {
        /* Weaker reaction, skipped if the target is already in a 0x100040 state. */
        object_set_flag2(OBJ_FLAG2_STATUS_0x10);
        ST8(object + oBattleObject_ContactState, 1);
    }
}

/* battleObject_applyHitModifierFlags_801AEB0(r5 = hit battle object, r7 = collision data) -> no outputs */
Next battleObject_applyHitModifierFlags_801AEB0(void) {
    apply_hit_modifiers(R5, R7);
    RETURN_TO_CALLER();
}
