/*
 * asm32.c: readable functions of bn6f asm32.s, up to 0x08119438, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_810D970 ── */
/* sub_810D970: Battle object per-phase hook.
 * On phase 0, clear the +0xC extra-var slot; if the +8 extra-var pointer is
 * non-null, reset the object's frame timer. Then dispatch the standard
 * per-phase update. */
Next sub_810D970(void) {
    if ((u8)LD8(R5 + oBattleObject_CurPhase) == 0) {
        ST32(R5 + oBattleObject_ExtraVars + 0xC, 0);
        R0 = LD32(R5 + oBattleObject_ExtraVars + 8);
        SET_FLAGS_NZ(R0);
        if (!ZERO_FLAG()) {
            GUEST_CALL(battle_ResetObjectTimerTo8_80C4072);
        }
    }
    GUEST_CALL(battle_objectUpdateDispatchPhase_80165C2);
    RETURN_TO_CALLER();
}

/* ── sub_810D998 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_sub_810DAF0_result;
/* register form: generated wrapper: sub_810DAF0 has no typed C interface yet */
static inline call_sub_810DAF0_result call_sub_810DAF0(u32 r4, u32 r5, u32 r7, u32 r8, u32 r9, u32 r11, u32 r12) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    R8 = r8;
    R9 = r9;
    R11 = r11;
    R12 = r12;
    GUEST_CALL(sub_810DAF0);
    return (call_sub_810DAF0_result){ R0, R1, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* sub_810D998: advance one step of the enemy AI state machine.
 * Calls a pre-check (sub_810DAF0); if it returns nonzero the AI is paused
 * and no handler runs. Otherwise the state byte stored in the AI block
 * (at the modified r6 from sub_810DAF0) indexes a four-entry jump table
 * whose handler runs in-place and sets the output registers.
 * r4 and r6 are preserved by push/pop: they always equal the entry values. */

struct ai_step_result {
    u32 r1, r5, r8, r9, r10, r11, r12;
};

/* register form: jump-table dispatch via bx; after the handler returns,
 * pop {r4,r6,pc} restores the entry values of the preserved registers. */
static struct ai_step_result call_ai_handler(u32 handler_addr, u32 state,
                                              u32 r4, u32 r5, u32 r6, u32 r7,
                                              u32 r8, u32 r9, u32 r10, u32 r11, u32 r12,
                                              u32 entry_r4, u32 entry_r6) {
    R0 = state;
    R1 = handler_addr;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R8 = r8;
    R9 = r9;
    R10 = r10;
    R11 = r11;
    R12 = r12;
    GUEST_CALL_ADDR(handler_addr);
    /* restore preserved registers: pop {r4,r6,pc} */
    R4 = entry_r4;
    R6 = entry_r6;
    struct ai_step_result out = { R1, R5, R8, R9, R10, R11, R12 };
    return out;
}

static struct ai_step_result ai_step(u32 r4, u32 r5, u32 r6, u32 r7,
                                      u32 r8, u32 r9, u32 r10, u32 r11, u32 r12) {
    call_sub_810DAF0_result pre = call_sub_810DAF0(r4, r5, r7, r8, r9, r11, r12);

    if (pre.r0 != 0) {
        /* register form: pop {r4,r6,pc} restores entry values after the pre-check
         * modified them; wrapper left R4/R6 changed but original pops them. */
        R4 = r4;
        R6 = r6;
        return (struct ai_step_result){ pre.r1, pre.r5, pre.r8, pre.r9,
                                        pre.r10, pre.r11, pre.r12 };
    }

    u32 state = LD8(pre.r6 + oAIState_Unk_00);
    u32 handler = LD32(A_off_810D9B4 + state);

    return call_ai_handler(handler, state,
                           pre.r4, pre.r5, pre.r6, pre.r7,
                           pre.r8, pre.r9, pre.r10, pre.r11, pre.r12,
                           r4, r6);
}

/* sub_810D998(r4..r12, lr) -> r1 r5 r8 r9 r10 r11 r12; preserves r4 r6 */
Next sub_810D998(void) {
    struct ai_step_result out = ai_step(R4, R5, R6, R7, R8, R9, R10, R11, R12);
    R1 = out.r1, R5 = out.r5, R8 = out.r8, R9 = out.r9,
    R10 = out.r10, R11 = out.r11, R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── sub_810DA00 ── */
/* sub_810DA00: AI helper that runs a forward probe from an object.
 * Inputs:  r5 = battle object pointer, r6 = AI state pointer
 * Outputs: r1, r5
 * If the probe succeeds (sub_8109708 returns nonzero), the AI sets
 * state 0->0xC. Otherwise it writes 0 to state+2 and 8 to state+0. */
Next sub_810DA00(void) {
    /* SP/PC bookkeeping left to the caller (the original here pushes {lr}
     * and pops into PC; the readable harness returns via RETURN_TO_CALLER()). */

    /* object_getFrontDirection() -> forward panel delta */
    R0 = 0;
    GUEST_CALL(object_getFrontDirection);
    u32 forward = R0;

    /* panel X/Y of the object, plus the ExtraVars+4 word */
    u32 panelX  = LD8(R5 + oBattleObject_PanelX);
    u32 panelY  = LD8(R5 + oBattleObject_PanelY);
    u32 extra4  = LD32(R5 + oBattleObject_ExtraVars + 4);

    /* probe (panelX + forward, panelY, extra4) */
    R0 = panelX + forward;
    R1 = panelY;
    R2 = extra4;
    GUEST_CALL(sub_8109708);

    if (R0 != 0) {
        /* probe hit: move to AI state 0x0C */
        ST8(R6 + oAIState_Unk_00, 0x0C);
    } else {
        /* probe missed: clear state+2, transition to state 0x08 */
        ST8(R6 + oAIState_Unk_02, (u8)R0);
        ST8(R6 + oAIState_Unk_00, 0x08);
    }

    RETURN_TO_CALLER();
}

/* ── sub_810DA24 ── */
/* sub_810DA24: AI state dispatcher.
 * R6 points to the AI state struct; the byte at offset oAIState_Unk_02
 * selects the per-state handler via the jump table at off_810DA44
 * (0 -> sub_810DA4C, 1 -> sub_810DAAA). The handler is called and returns
 * back here; whatever registers it leaves behind (r1, r5, r8-r12) are
 * handed straight on to our caller. */
Next sub_810DA24(void) {
    u32 state = LD8(R6 + oAIState_Unk_02);

    if (state == 0)
        GUEST_CALL(sub_810DA4C);
    else
        GUEST_CALL(sub_810DAAA);

    RETURN_TO_CALLER();
}

/* ── sub_810DA4C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFrontDirection_result;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result call_object_getFrontDirection(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result){ R0 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; } call_sub_8109708_result;
/* register form: generated wrapper: sub_8109708 has no typed C interface yet */
static inline call_sub_8109708_result call_sub_8109708(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sub_8109708);
    return (call_sub_8109708_result){ R0, R1, R2, R3, R4, R5, R6, R7 };
}

typedef struct { u32 r0, r2, r3, r5, r6, r7; } call_sub_80103F8_result;
/* register form: generated wrapper: sub_80103F8 has no typed C interface yet */
static inline call_sub_80103F8_result call_sub_80103F8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_80103F8);
    return (call_sub_80103F8_result){ R0, R2, R3, R5, R6, R7 };
}

/* AI step: find a movement target tile for the battle object.
 * Reads the front direction, checks the panel ahead via sub_8109708,
 * then (if walkable) locates the opposing alliance's object and picks
 * an X-coordinate relative to its panel range.  Writes four fields of
 * the AI state (r6) and leaves r1 = the range lower bound. */
static u32 ai_select_movement_target(u32 battle, u32 ai_state) {
    /* Probe the tile one step ahead of us. */
    call_object_getFrontDirection_result dir1 = call_object_getFrontDirection(battle);
    u32 front_x = dir1.r0 + LD8(battle + oBattleObject_PanelX);
    u32 front_y = LD8(battle + oBattleObject_PanelY);
    u32 extra_4 = LD32(battle + oBattleObject_ExtraVars + 4);

    call_sub_8109708_result walkable = call_sub_8109708(front_x, front_y, extra_4, battle);
    if (walkable.r0 != 0) {
        /* Panel is blocked – give up, set state byte to 0xC. */
        ST8(ai_state + oAIState_Unk_00, 0xC);
        return walkable.r1;
    }

    /* Find the opposing alliance's battle object. */
    u32 opposing = LD8(battle + oBattleObject_Alliance) ^ 1;
    call_sub_80103F8_result enemy_res = call_sub_80103F8(opposing, battle);
    u32 enemy = enemy_res.r0;

    ST8(ai_state + oAIState_Unk_05, LD8(enemy + 0x13));

    /* Build the signed panel-X range [lo, hi] around our column. */
    call_object_getFrontDirection_result dir2 = call_object_getFrontDirection(battle);
    u32 d = dir2.r0;
    s32 px = (s32)LD8(battle + oBattleObject_PanelX);
    s32 lo = px + (s32)d;
    u32 extra_0 = LD32(battle + oBattleObject_ExtraVars);
    u32 offset = d * extra_0;
    s32 hi = px + (s32)offset;

    if (lo >= hi) {
        s32 tmp = lo;
        lo = hi;
        hi = tmp;
    }

    /* Decide target X: chase the enemy directly if in range, else step. */
    s32 enemy_x = (s32)LD8(enemy + 0x12);
    s32 target_x;
    if (enemy_x >= lo && enemy_x <= hi) {
        target_x = px;
    } else {
        target_x = enemy_x - (s32)offset;
    }

    ST8(ai_state + oAIState_Unk_04, (u8)target_x);
    ST8(ai_state + oAIState_Unk_02, 4);

    return (u32)lo;
}

/* sub_810DA4C(r5 = battle object, r6 = AI state, r10, lr) -> r1, r5 */
Next sub_810DA4C(void) {
    R1 = ai_select_movement_target(R5, R6);
    RETURN_TO_CALLER();
}

/* ── sub_810DB20 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3; } call_GetAllianceDependentPanelParamArgs_result;
/* register form: generated wrapper: GetAllianceDependentPanelParamArgs has no typed C interface yet */
static inline call_GetAllianceDependentPanelParamArgs_result call_GetAllianceDependentPanelParamArgs(u32 r3, u32 r5) {
    R3 = r3;
    R5 = r5;
    GUEST_CALL(GetAllianceDependentPanelParamArgs);
    return (call_GetAllianceDependentPanelParamArgs_result){ R2, R3 };
}

typedef struct { u32 r0, r2, r3, r4, r5; } call_object_checkPanelParameters_result;
/* register form: generated wrapper: object_checkPanelParameters has no typed C interface yet */
static inline call_object_checkPanelParameters_result call_object_checkPanelParameters(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_checkPanelParameters);
    return (call_object_checkPanelParameters_result){ R0, R2, R3, R4, R5 };
}

/* sub_810DB20: AI panel-step helper.
 *
 * Attempts to move a battle object one panel closer to its AI target (r6).
 * Tries the Y axis first; if that yields a valid panel (checkPanelParameters
 * returns non-zero), returns the candidate (x, y).  Otherwise falls through
 * to try the X axis.  If neither axis produces a valid step, returns (0, y).
 *
 * Inputs:  r5 = battle object pointer, r6 = AI state pointer.
 * Outputs: r0 = candidate X (or 0 if no valid move), r1 = candidate Y.
 */

typedef struct { u32 x; u32 y; } PanelStepResult;

static PanelStepResult ai_step_panel_toward_target(u32 battle_obj, u32 ai_state) {
    /* --- Y axis attempt --- */
    u32 target_y = LD8(ai_state + oAIState_Unk_05);
    u32 panel_y  = LD8(battle_obj + oBattleObject_PanelY);

    if (panel_y != target_y) {
        /* Nudge Y one step toward the target. */
        u32 cand_y = (panel_y < target_y) ? (panel_y + 1) : (panel_y - 1);
        u32 cand_x = LD8(battle_obj + oBattleObject_PanelX);

        call_GetAllianceDependentPanelParamArgs_result gp =
            call_GetAllianceDependentPanelParamArgs(A_byte_810DBF0, battle_obj);

        call_object_checkPanelParameters_result chk =
            call_object_checkPanelParameters(cand_x, cand_y, gp.r2, gp.r3);

        if (chk.r0 != 0) {
            PanelStepResult out = { cand_x, cand_y };
            return out;
        }
        /* Y step not valid; fall through to try X axis. */
    }

    /* --- X axis attempt --- */
    u32 panel_x  = LD8(battle_obj + oBattleObject_PanelX);
    u32 target_x = LD8(ai_state + oAIState_Unk_04);

    if (panel_x == target_x) {
        /* Already aligned on X: no move possible. */
        PanelStepResult out = { 0, target_x };
        return out;
    }

    /* Nudge X one step toward the target. */
    u32 cand_x = (panel_x < target_x) ? (panel_x + 1) : (panel_x - 1);
    u32 cand_y = LD8(battle_obj + oBattleObject_PanelY);

    call_GetAllianceDependentPanelParamArgs_result gp =
        call_GetAllianceDependentPanelParamArgs(A_byte_810DBF0, battle_obj);

    call_object_checkPanelParameters_result chk =
        call_object_checkPanelParameters(cand_x, cand_y, gp.r2, gp.r3);

    if (chk.r0 != 0) {
        PanelStepResult out = { cand_x, cand_y };
        return out;
    }

    /* No valid step found on either axis. */
    PanelStepResult out = { 0, cand_y };
    return out;
}

Next sub_810DB20(void) {
    PanelStepResult res = ai_step_panel_toward_target(R5, R6);
    R0 = res.x;
    R1 = res.y;
    RETURN_TO_CALLER();
}

/* ── sub_810DCCE ── */
/* sub_810DCCE(r5 = battle object):
 * Lazily creates the object's auxiliary entity (type 4, parameter table 0x10B04)
 * on first call and stores its handle in ExtraVars+8; does nothing if it already
 * exists. r4 is saved/restored around the call, so it always passes through
 * unchanged. On the "already exists" path the flags are those of the handle
 * test; on the creation path they are whatever the creator call left. */
Next sub_810DCCE(void) {
    u32 savedR4 = R4;
    u32 handle = LD32(R5 + oBattleObject_ExtraVars + 8);

    if (handle == 0) {
        R0 = 0;
        R1 = 0;
        R2 = 4;
        R3 = 0;
        R4 = 0x10B04; /* creation parameter table */
        GUEST_CALL(sub_80C4038);
        ST32(R5 + oBattleObject_ExtraVars + 8, R0);
    } else {
        /* Already created: expose the handle and leave its comparison flags. */
        R0 = handle;
        SET_FLAGS_CMP(handle, 0);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── ai_DealQueuedAttackDamage_8112F34 ── */
/* ai_DealQueuedAttackDamage_8112F34(r7 = AI attack vars):
 * Fire one of the AI opponent's queued attacks: hand the attack's target
 * panel coordinates (Unk_16 = x, Unk_17 = y), an elevation of 0xA00 plus
 * Unk_0f, and the attack's damage (field at offset 8) to the damage
 * routine battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84, with r2/r3 cleared.
 * r4/r6/r7 are only scratch for setting up the call (the original saves
 * and restores them around the callee), so they end unchanged; r1-r3 are
 * whatever the callee leaves behind. */
Next ai_DealQueuedAttackDamage_8112F34(void) {
    u32 vars = R7;
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    R0 = LD8(vars + oAIAttackVars_Unk_16);
    R1 = LD8(vars + oAIAttackVars_Unk_17);
    R4 = 0xA00 + LD8(vars + oAIAttackVars_Unk_0f);
    R2 = 0;
    R3 = 0;
    R6 = LD32(vars + 8);

    GUEST_CALL(battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84);

    /* The original pushed r4/r6/r7 and pops them after the call, so the
     * caller's values survive the callee. */
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;

    RETURN_TO_CALLER();
}

/* ── ai_attackState_SetupSpecial_8112FBA ── */
/* register form: calls sub_80C4038 outside this file, which has no typed C interface yet */
static u32 call_sub_80C4038(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4) {
    R0 = a0; R1 = a1; R2 = a2; R3 = a3; R4 = a4;
    GUEST_CALL(sub_80C4038);
    return R0;
}

/* register form: calls battle_SetObjectExtraFlag14_80C4086 outside this file, which has no typed C interface yet */
static u32 battle_SetObjectExtraFlag14_helper(void) {
    GUEST_CALL(battle_SetObjectExtraFlag14_80C4086);
    return R0;
}

/* register form: calls object_getFrontDirection outside this file, which has no typed C interface yet */
static s32 object_getFrontDirection_helper(void) {
    GUEST_CALL(object_getFrontDirection);
    return (s32)R0;
}

/* register form: calls object_setCounterTime outside this file, which has no typed C interface yet */
static u32 object_setCounterTime_helper(u32 frames) {
    R0 = frames;
    GUEST_CALL(object_setCounterTime);
    return R0;
}

/* "Special" attack setup on an AI attacker.
 * Phase depends on attacker.oBattleObject_RelatedObject1Ptr:
 *   1 -> set attacker anim=2, clear AI Unk_28, look up the victim object
 *        (sub_80C4038 with payload 0x12810) and store as AI Unk_30, turn the
 *        victim's extra flag 14 on, set its timer to AI Unk_0e * 10, save the
 *        front direction as AI Unk_34, set the counter time to 30 frames,
 *        and write AI Unk_00 = 8.
 *   0 -> write AI Unk_00 = 0xC (a different phase).
 *   anything else -> leave AI Unk_00 untouched.
 * Returns the value the original leaves in r0. */
static u32 do_ai_special_attack(u32 attacker, u32 ai_vars) {
    u32 related = LD32(attacker + oBattleObject_RelatedObject1Ptr);
    u32 final_r0 = related;

    if (related == 1) {
        ST8(attacker + oBattleObject_CurAnim, 2);
        ST32(ai_vars + oAIAttackVars_Unk_28, 0);

        u32 victim = call_sub_80C4038(0, 0, 0, 0, 0x12810);
        ST32(ai_vars + oAIAttackVars_Unk_30, victim);

        u32 flag_target = battle_SetObjectExtraFlag14_helper();
        u32 scaled = (u32)LD8(ai_vars + oAIAttackVars_Unk_0e) * 0xA;
        ST16(flag_target + oBattleObject_Timer, scaled);

        ST32(ai_vars + oAIAttackVars_Unk_34, (u32)object_getFrontDirection_helper());

        object_setCounterTime_helper(0x1E);
        ST16(ai_vars + oAIAttackVars_Unk_00, 8);
        final_r0 = 8;
    } else if (related == 0) {
        ST16(ai_vars + oAIAttackVars_Unk_00, 0xC);
        final_r0 = 0xC;
    }

    return final_r0;
}

Next ai_attackState_SetupSpecial_8112FBA(void) {
    u32 result = do_ai_special_attack(R5, R7);
    R0 = result;
    SET_FLAGS_CMP(result, 0);
    RETURN_TO_CALLER();
}

/* ── ai_8113038 ── */
#define ANIM_END_MARKER 0x80 /* frame-parameter bit marking the end of an animation */

/* register form: calls sprite_getFrameParameters outside this file, which has no typed C interface yet */
static u32 sprite_frame_params(u32 anim) {
    R0 = anim;
    GUEST_CALL(sprite_getFrameParameters);
    return R0;
}

/* register form: calls object_exitAttackState outside this file, which has no typed C interface yet */
static u32 exit_attack_state(u32 frames_left, u32 r1_in) {
    R0 = frames_left; /* the expired countdown, as the original passes it */
    R1 = r1_in;       /* the original's end-marker mask is still in r1 here */
    GUEST_CALL(object_exitAttackState);
    return R1;        /* this call decides the r1 ai_8113038's caller sees */
}

/* Advance a battle object's attack state by one frame (its AIAttackVars at `attack`).
 * The first frame marks the attack started, switches the object to animation 3
 * and loads the duration from Unk_18 into the countdown at Unk_10.  Each frame,
 * a running animation is stopped once its frame parameters report the end
 * marker, and when the countdown expires the object leaves the attack state.
 * Returns the value to leave in r1 (an output of the original): the end-marker
 * mask once the animation check has run, otherwise the incoming r1 or whatever
 * the exit call left in it. */
static u32 step_attack_state(u32 object, u32 attack, u32 r1) {
    if (LD8(attack + oAIAttackVars_Unk_01) == 0) {
        /* first frame of the attack */
        ST8(attack + oAIAttackVars_Unk_01, 1);
        ST8(object + oBattleObject_CurAnim, 3);
        ST16(attack + oAIAttackVars_Unk_10, LD16(attack + oAIAttackVars_Unk_18));
    }

    u32 anim = LD8(object + oBattleObject_CurAnim);
    if (anim != 0) {
        r1 = ANIM_END_MARKER; /* the original builds its test mask in r1, which callers read */
        if (sprite_frame_params(anim) & ANIM_END_MARKER)
            ST8(object + oBattleObject_CurAnim, 0); /* animation finished: stop it */
    }

    u32 frames_left = LD16(attack + oAIAttackVars_Unk_10) - 1;
    ST16(attack + oAIAttackVars_Unk_10, (u16)frames_left);
    if ((s32)frames_left <= 0)
        r1 = exit_attack_state(frames_left, r1); /* countdown expired: leave the attack state */
    return r1;
}

/* ai_8113038(r5 = battle object, r7 = its AI attack vars) -> r1 */
Next ai_8113038(void) {
    R1 = step_attack_state(R5, R7, R1);
    RETURN_TO_CALLER();
}

/* ── sub_81130E4 ── */
/* sub_81130E4:
 *   inputs:  r4 r5 r10 lr
 *   outputs: r1 r5 r8 r9 r10 r11 r12
 *   clobbers (unused by callers): nz c v r0 r2 r3 r4 r6 r7 lr
 *   preserved: -
 *
 * Trivial two-call tail sequence: call sub_8113114, then call sub_8016B02,
 * and return to the caller. The original uses a `pop {pc}` to do the
 * return, but since sub_8016B02 does not consume LR from the stack, the
 * net effect on guest-visible state is identical to a normal call-return
 * followed by RETURN_TO_CALLER().
 */
Next sub_81130E4(void) {
    GUEST_CALL(sub_8113114);
    GUEST_CALL(sub_8016B02);
    RETURN_TO_CALLER();
}

/* ── sub_81130F0 ── */
Next sub_81130F0(void) {
    /* Wrapper that updates internal state then dispatches the battle
     * object's state machine for this frame. */
    GUEST_CALL(sub_8113114);
    GUEST_CALL(battleObjectStateDispatcher_8016CE8);
    RETURN_TO_CALLER();
}

/* ── sub_8113114 ── */
/* sub_8113114: clears BattleObject.RelatedObject1Ptr to 0.
 * inputs: r4 = AIData pointer, r5 = BattleObject pointer
 * clobbers: r0, r1, nz */
Next sub_8113114(void) {
    u32 r0 = LD8(R4 + oAIData_Version_16);
    r0 = r0 & 1;
    (void)r0; /* bit 0 of AI version, result unused beyond flag setting */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    ST32(R5 + oBattleObject_RelatedObject1Ptr, R0);
    RETURN_TO_CALLER();
}

/* ── battle_packObjStateBitToAttrCode_8113154 ── */
/* battle_packObjStateBitToAttrCode_8113154(r4 = battle object, r6 = destination byte):
 * Packs the object's state bit at offset 0x16 (bit 0 only) into a small
 * attribute code: 0 -> 4, 1 -> 8, and stores it to [r6]. Sets r1 = 1 and
 * leaves the flags from the final add. */
Next battle_packObjStateBitToAttrCode_8113154(void) {
    u32 code = (LD8(R4 + 0x16) & 1) << 2;

    R1 = 1;
    SET_FLAGS_ADD(code, 4);
    code = code + 4;

    ST8(R6, code);
    R0 = code;
    RETURN_TO_CALLER();
}

/* ── battle_initAttackFromPreset_8113162 ── */
/* battle_initAttackFromPreset_8113162: initialize an attack on a battle object.
 * r4 = object supplying the attack index (its +0x16 byte selects one preset),
 * r5 = context object whose +0x13 state byte must pass battle_pickRandomRowObjectFields_810971A's check,
 * r7 = object whose attack fields are (re)initialized from the preset tables.
 * Returns early without touching r7 if the check fails; flags on that path come
 * from the "state == 0" comparison, otherwise from object_setAttack0. */
Next battle_initAttackFromPreset_8113162(void) {
    R0 = LD8(R5 + 0x13);
    GUEST_CALL(battle_pickRandomRowObjectFields_810971A);

    /* cmp r0, #0 from the original: caller may branch on these flags. */
    SET_FLAGS_CMP(R0, 0);
    if (R0 == 0)
        RETURN_TO_CALLER();

    /* Attack preset selected by the attacker's +0x16 index. */
    u32 preset = LD8(R4 + 0x16);

    /* Per-preset stat tables: power, element/type, hit behavior, etc. */
    ST8 (R7 + 0xF,  LD8 (A_byte_81130DE + preset));
    ST8 (R7 + 0xE,  LD8 (A_byte_81130D8 + preset));
    ST8 (R7 + 0xC,  LD8 (A_byte_81130C6 + preset));
    ST8 (R7 + 0x18, LD8 (A_byte_81130D2 + preset));
    ST16(R7 + 0x8,  LD8 (A_byte_81130C0 + preset));

    /* Fixed width of 10 for this attack. */
    ST16(R7 + 0xA, 10);

    /* Per-preset word-sized field (e.g. sprite/animation pointer). */
    R1 = preset << 2;
    ST32(R7 + 0x30, LD32(A_byte_81130A8 + R1));

    R0 = 10;
    GUEST_CALL(object_setAttack0);
    RETURN_TO_CALLER();
}

/* ── sub_8116924 ── */
/* Pet-menu object dispatcher (sub_8116924).
 *
 * A trampoline: it reads the pet-menu object's state byte at [r5+8], picks
 * a handler address from the three-entry table at off_8116938, and calls
 * it with LR pointing back here, so the handler returns straight through
 * to sub_8116924's caller. The handler is entered with r0 = its own
 * address (r0 still holds the table entry when `bx r0` executes) and
 * r1 = the state byte; its register results (r2, r3, r5, r10, r11) flow
 * unchanged to the caller, and the flags are whatever the handler left. */

/* register form: dispatch through a code address taken from a jump table;
 * the three handlers have no typed wrappers, and their r2/r3/r5/r10/r11
 * results must reach sub_8116924's caller unfiltered, which rules out a
 * typed wrapper for them. */
static u32 call_pet_menu_handler(u32 handler, u32 state) {
    R0 = handler;   /* handler entry state: r0 = handler address ... */
    R1 = state;     /* ... r1 = the object's state byte */
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Run one pet-menu object step: dispatch on the state byte at [object+8]
 * (an index into the handler table at off_8116938). */
static void run_pet_menu_object(u32 object) {
    u32 state = LD8(object + 8);
    u32 handler = LD32(A_off_8116938 + state);
    call_pet_menu_handler(handler, state);
}

/* sub_8116924(r5 = pet-menu object) -> r2 r3 r5 r10 r11 left by the handler */
Next sub_8116924(void) {
    run_pet_menu_object(R5);
    RETURN_TO_CALLER();
}

/* ── petMenu_initSprite_8116944 ── */
/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void init_gui_sprite_sub_8116944(int type) {
    R0 = 1;
    R1 = type;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_set_anim_sub_8116944(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_load_anim_data_sub_8116944(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_no_shadow_sub_8116944(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void sprite_set_sub_entry_palette_bank(u32 bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void sprite_set_palette_sub_8116944(int pal) {
    R0 = pal;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void sprite_set_priority_sub_8116944(void) {
    R0 = 0;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void slot_list_store_halfword_and_clear(u32 v) {
    R0 = v;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void sprite_set_sub_record_field_a_sub_8116944(void) {
    R0 = 0;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void sprite_set_xy(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

/* register form: calls petMenu_updateObject_81169B0 outside this file, which has no typed C interface yet.
 * The original passes R0 (=6) as an argument; the function clobbers R0 on return. */
static void pet_menu_update_object(u32 arg) {
    R0 = arg;
    GUEST_CALL(petMenu_updateObject_81169B0);
}

/* Initialize a pet-menu sprite record. `obj` points to the record (R5 in the original).
 * Leaves R0 and the flags as petMenu_updateObject_81169B0 does, matching the original. */
static void pet_menu_init_sprite(u32 obj, int gui_type) {
    init_gui_sprite_sub_8116944(gui_type);
    sprite_set_anim_sub_8116944(LD8(obj + 4));
    sprite_load_anim_data_sub_8116944();
    sprite_no_shadow_sub_8116944();
    sprite_set_sub_entry_palette_bank(0xC);
    sprite_set_palette_sub_8116944(0);
    sprite_set_priority_sub_8116944();
    slot_list_store_halfword_and_clear(0x2D4);
    sprite_set_sub_record_field_a_sub_8116944();

    ST8(obj + 8, 4);

    u8 a = 0, b = 1;
    if (LD8(obj + 5) != 0) {
        a = 8;
        b = 0x23;
    }
    ST8(obj + 9, a);
    ST8(obj, b);

    sprite_set_xy(0xB0, 0x40);
    ST8(obj + 4, 0xFF);
    ST8(obj + 0xC, 6);
    pet_menu_update_object(6);
}

/* petMenu_initSprite_8116944(r1 = gui type, r5 = pet-menu object) -> r0 = petMenu_updateObject's return; flags from it */
Next petMenu_initSprite_8116944(void) {
    u32 obj = R5;
    int gui_type = (int)R1;
    pet_menu_init_sprite(obj, gui_type);
    RETURN_TO_CALLER();
}

/* ── petMenu_updateObject_81169B0 ── */
/* petMenu_updateObject_81169B0: per-frame update of one PET menu object (object pointer in r5).
 *
 * The object's mode byte at (r5 + 9) selects an entry of the three-entry
 * handler table off_81169CC by byte offset (0, 4 or 8). The chosen handler
 * runs with r7 = ePETMenuData and r1 = the mode byte, and returns to
 * 0x81169BE (the "mov lr, pc; bx r0" idiom), i.e. straight into the
 * sprite update below. Afterwards the object's sprite is refreshed with
 * sprite_update, then r7 is restored and the function returns to its own
 * caller via pop {r7,pc} -- which resumes at the lr saved on entry, NOT at
 * the link register left behind by the internal calls. */
Next petMenu_updateObject_81169B0(void) {
    u32 returnAddr = LR;    /* push {r7,lr}: pop {pc} returns here */
    u32 savedR7 = R7;

    R7 = A_ePETMenuData;

    /* ldrb r1,[r5,#9]; ldr r0,[table,r1]; mov lr,pc; bx r0 */
    u32 mode = LD8(R5 + 9);
    R1 = mode;
    LR = 0x81169BE;                      /* handlers return into the bl below */
    R0 = LD32(A_off_81169CC + mode);     /* table entry: handler address | 1 */

    switch (mode) {
    case 0: GUEST_CALL(npc_tickDisappearTimer_81169D8); break;
    case 4: GUEST_CALL(battle_reloadObjectSprite_8116A04); break;
    case 8: GUEST_CALL(sub_8116A28); break;
    default: break;  /* unreachable: the mode byte is always 0, 4 or 8 */
    }

    /* bl sprite_update */
    LR = 0x81169C2 | 1u;
    GUEST_CALL(sprite_update);

    /* pop {r7,pc}: restore the caller's r7 and return to the address that
     * was saved on entry, not to sprite_update's return link. */
    R7 = savedR7;
    LR = returnAddr;
    RETURN_TO_CALLER();
}

/* ── npc_tickDisappearTimer_81169D8 ── */
/* npc_tickDisappearTimer_81169D8: run one step of an object's exit/disappear sequence.
 * While the signed 16-bit flag at r7+0x10 is set, do nothing. Otherwise
 * count the byte timer at r5+0xC down; when it reaches zero, put the
 * object into state 4 with animation id 0x23 and reload its sprite
 * (animation 1, no shadow).
 *
 * r4/r5 are preserved; r2/r3 are never touched. r0/r1 keep the values
 * the original leaves behind on the early-exit paths. */
Next npc_tickDisappearTimer_81169D8(void) {
    /* Nothing to do while the +0x10 flag is set. (r0 holds the 0x10
     * offset the original computes before the load.) */
    R0 = 0x10;
    s32 flag = LDS16(R7 + 0x10);
    R1 = (u32)flag;
    SET_FLAGS_NZ(flag);
    if (flag != 0)
        RETURN_TO_CALLER();

    /* Tick the timer down; still busy until it hits zero. */
    u32 timer = LD8(R5 + 0xC);
    R0 = timer - 1;
    ST8(R5 + 0xC, timer - 1);
    SET_FLAGS_CMP(timer, 1);
    if (timer != 1)
        RETURN_TO_CALLER();

    /* Timer expired: switch to the exit state and reset the sprite. */
    ST8(R5 + 9, 4);
    ST8(R5 + 0, 0x23);
    R0 = 1;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    RETURN_TO_CALLER();
}

/* ── battle_reloadObjectSprite_8116A04 ── */
/* battle_reloadObjectSprite_8116A04 (r5 = battle object):
 * Refresh the object's sprite from its current frame parameters.
 * If the frame parameters report that the animation must be (re)loaded
 * (bits 0xC0 set), restart it: select animation 0, load its data, remove
 * the shadow, run the follow-up setup in sub_8116A44, and mark the object
 * state byte at +9 as 8 (also leaving that value in r0). Flags are left
 * from the last flag-setting instruction (TST r0, 0xC0 on the skip path,
 * MOV r0, #8 otherwise). */
Next battle_reloadObjectSprite_8116A04(void) {
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;

    /* mov r1, #0xC0 happens before the branch, so R1 is 0xC0 on exit
     * even when the reload is skipped. */
    R1 = 0xC0;

    if ((frameParams & 0xC0) == 0) {
        /* tst r0, r1 -> Z set, nothing to do */
        SET_FLAGS_NZ(frameParams & 0xC0);
        RETURN_TO_CALLER();
    }

    R0 = 0;
    GUEST_CALL(sprite_setAnimation);        /* sprite_setAnimation(0) */
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    GUEST_CALL(sub_8116A44);

    /* obj->state = 8 (MOV #8 is flag-setting in Thumb); r0 ends as 8 too,
     * regardless of what sub_8116A44 left in it. */
    R0 = 8;
    SET_FLAGS_NZ(8);
    ST8(R5 + 9, 8);
    RETURN_TO_CALLER();
}

/* ── sub_8116A28 ── */
/* sub_8116A28: conditionally fix up a state byte, then continue into sub_8116A44.
 * If the counter byte pointed to by r7 has reached 8, the byte at r5+8 is also
 * set to 8 (both are kept in sync). Flags/results come from sub_8116A44. */
Next sub_8116A28(void) {
    if (LD8(R7) == 8)
        ST8(R5 + 8, 8);

    GUEST_CALL(sub_8116A44);
    RETURN_TO_CALLER();
}

/* ── sub_8116A44 ── */
/* register form: calls sub_802FE28 outside this file, which has no typed C interface yet */
static void call_sub_802FE28(u32 a1, u32 a2, u32 notUsed, u32 a4) {
    R0 = a1;
    R1 = a2;
    R2 = notUsed;
    R3 = a4;
    GUEST_CALL(sub_802FE28);
}

/* Drive sub_802FE28 over a packed table of (a1, a2) pairs. Each entry is 8 bytes:
 * a1 at offset 0, a2 at offset 4. notUsed is fixed at 0 and a4 at 1. */
static void run_table(u32 base, u32 count) {
    for (u32 i = 0; i < count; i++) {
        u32 entry = base + i * 8;
        call_sub_802FE28(LD32(entry), LD32(entry + 4), 0, 1);
    }
}

/* Initialize three small table-driven groups of sub_802FE28 calls (3 + 3 + 2 entries). */
static void init_three_tables(void) {
    run_table(A_dword_8116A94, 3);
    run_table(A_dword_8116AB0, 3);
    run_table(A_dword_8116ACC, 2);
}

Next sub_8116A44(void) {
    init_three_tables();
    SET_FLAGS_CMP(2, 2);
    RETURN_TO_CALLER();
}

/* ── sub_8116ADC ── */
/* register form: jump-table dispatch; the original branches through a loaded
 * pointer (mov lr, pc; bx r0) so we call the computed address directly.
 * Before the call: R0 = target, R1 = variant, R7 = ePETMenuData (menu data).
 * R5 (battle object) is already set by the caller and read by the target. */
static void epet_menu_dispatch(u32 battle_obj) {
    u32 variant = (u32)LD8(battle_obj + 8);
    u32 funcptr = LD32(A_off_8116AF0 + variant);
    u32 saved_r7 = R7;
    R0 = funcptr;
    R1 = variant;
    R7 = A_ePETMenuData;
    GUEST_CALL_ADDR(funcptr);
    R7 = saved_r7;
}

/* sub_8116ADC: ePET menu variant dispatch (r5 = battle object pointer) */
Next sub_8116ADC(void) {
    epet_menu_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8116B00 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_GetCurPETNavi_result;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result call_GetCurPETNavi(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result){ R0 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_sub_8116B00(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_sub_8116B00;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_sub_8116B00 call_sprite_loadAnimationData_sub_8116B00(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_sub_8116B00){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_sub_8116B00(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r2; } call_sprite_setSubEntryPaletteBank_800304A_result_sub_8116B00;
/* register form: generated wrapper: sprite_setSubEntryPaletteBank_800304A has no typed C interface yet */
static inline call_sprite_setSubEntryPaletteBank_800304A_result_sub_8116B00 call_sprite_setSubEntryPaletteBank_800304A_sub_8116B00(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
    return (call_sprite_setSubEntryPaletteBank_800304A_result_sub_8116B00){ R2 };
}

/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline void call_sprite_setPalette_sub_8116B00(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
}

typedef struct { u32 r1, r2; } call_sprite_setPriority_8002E14_result_sub_8116B00;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_sub_8116B00 call_sprite_setPriority_8002E14_sub_8116B00(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_sub_8116B00){ R1, R2 };
}

/* register form: generated wrapper: slotList_storeHalfwordAndClearField_800306C has no typed C interface yet */
static inline void call_slotList_storeHalfwordAndClearField_800306C_sub_8116B00(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: generated wrapper: sprite_setSubRecordFieldA_8003060 has no typed C interface yet */
static inline void call_sprite_setSubRecordFieldA_8003060_sub_8116B00(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

typedef struct { u32 r3; bool c, v; } call_sprite_setCoordinates_result_sub_8116B00;
/* register form: generated wrapper: sprite_setCoordinates has no typed C interface yet */
static inline call_sprite_setCoordinates_result_sub_8116B00 call_sprite_setCoordinates_sub_8116B00(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(sprite_setCoordinates);
    return (call_sprite_setCoordinates_result_sub_8116B00){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r5, r10, r11; } call_sprite_dispatchVariantUpdate_8116B9C_result;
/* register form: generated wrapper: sprite_dispatchVariantUpdate_8116B9C has no typed C interface yet */
static inline call_sprite_dispatchVariantUpdate_8116B9C_result call_sprite_dispatchVariantUpdate_8116B9C(u32 r5, u32 r7) {
    R5 = r5;
    R7 = r7;
    GUEST_CALL(sprite_dispatchVariantUpdate_8116B9C);
    return (call_sprite_dispatchVariantUpdate_8116B9C_result){ R2, R3, R5, R10, R11 };
}

/* Direct call to sub-record initializer at loc_8002774 (no generated wrapper). */
/* register form: known code address, no typed wrapper provided */
static void call_loc_8002774(u32 a1, u32 a2) {
    R0 = a1;
    R1 = a2;
    GUEST_CALL(loc_8002774);
}

/* Team byte values stored at sprite offset 0. */
#define TEAM_A  0x23
#define TEAM_B  0x43

/* Animation index overrides. */
#define ANIM_DEFAULT_KIND  3
#define ANIM_PET_NAVI      8

/* Initialize a battle sprite for display: pack team byte, set animation,
 * configure palette/priority/coordinates, then dispatch its variant update. */
static call_sprite_dispatchVariantUpdate_8116B9C_result
init_sprite_variant(u32 spr, u32 player_slot) {
    /* Store team byte: 0x43 if kind is set, else 0x23. */
    u8 kind = LD8(spr + 4);
    ST8(spr, kind ? TEAM_B : TEAM_A);

    /* Initialize sub-record fields. */
    call_loc_8002774(0, kind);

    /* Determine animation index; override for current PET navi. */
    u8 anim = LD8(spr + 4);
    if (anim == ANIM_DEFAULT_KIND && call_GetCurPETNavi().r0 != 0)
        anim = ANIM_PET_NAVI;

    /* Configure sprite appearance. */
    call_sprite_setAnimation_sub_8116B00(anim, spr);

    /* Load animation data; the sprite slot may be relocated. */
    call_sprite_loadAnimationData_result_sub_8116B00 anim_res = call_sprite_loadAnimationData_sub_8116B00(spr);
    spr = anim_res.r5;

    call_sprite_noShadow_sub_8116B00(spr);
    call_sprite_setSubEntryPaletteBank_800304A_sub_8116B00(0xD, spr);
    call_sprite_setPalette_sub_8116B00(1, spr);
    call_sprite_setPriority_8002E14_sub_8116B00(0, spr);
    call_slotList_storeHalfwordAndClearField_800306C_sub_8116B00(LD32(A_off_8116B90), spr);
    call_sprite_setSubRecordFieldA_8003060_sub_8116B00(0, spr);

    /* Set state: frame 4 active, sub-frame 0. */
    ST8(spr + 8, 4);
    ST8(spr + 9, 0);

    /* Compute display coordinates based on team kind and slot alignment. */
    u32 x, y;
    if (LD8(spr + 5) != 0) {
        u8 k = LD8(spr + 4);
        y = LD8(A_byte_8116BEC + (u32)k * 2);
        x = (LD8(player_slot + 4) == k) ? 0x1E : 0x16;
    } else {
        x = 0x16;
        y = 0x20;
    }
    call_sprite_setCoordinates_sub_8116B00(x, y, spr);

    /* Mark sprite as visible and active. */
    ST8(spr + 0xD, 1);
    ST8(spr + 0xC, 0);

    /* Final variant dispatch; produces the output registers. */
    return call_sprite_dispatchVariantUpdate_8116B9C(spr, player_slot);
}

/* sub_8116B00(r5 = sprite, r7 = player slot, r10, lr) -> r2 r3 r5 r10 r11 */
Next sub_8116B00(void) {
    call_sprite_dispatchVariantUpdate_8116B9C_result res = init_sprite_variant(R5, R7);
    R2 = res.r2; R3 = res.r3; R5 = res.r5; R10 = res.r10; R11 = res.r11;
    RETURN_TO_CALLER();
}

/* ── sprite_dispatchVariantUpdate_8116B9C ── */
/* sprite_dispatchVariantUpdate_8116B9C(r5 = sprite/object pointer):
 * Run the per-variant update routine selected by the byte at [r5+9], then
 * advance the sprite animation. The selector byte indexes a 3-entry word
 * table, so it only ever holds 0, 4 or 8 (byte offsets into the table).
 * All registers except r0/r1 (clobbered by the call) are passed through. */
Next sprite_dispatchVariantUpdate_8116B9C(void) {
    switch (LD8(R5 + 9)) {
    case 0: GUEST_CALL(sprite_fallingObjectStepDown_8116BC0); break; /* off_8116BB4[0] */
    case 4: GUEST_CALL(sub_8116BFC); break; /* off_8116BB4[1] */
    case 8: GUEST_CALL(sprite_scrollStepAndCullOffscreen_8116C14); break; /* off_8116BB4[2] */
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── sprite_fallingObjectStepDown_8116BC0 ── */
/* sprite_fallingObjectStepDown_8116BC0(r5 = falling object state):
 * Move a falling object 8 pixels further down, then check it against the
 * stop depth selected by the state byte at [r5+4] (an index into the
 * halfword table at byte_8116BEC: 0x20, 0x30, ... 0x90). While the object
 * is still above its stop depth, nothing else happens; once it has reached
 * or passed it, snap its coordinate to the stop depth and store phase 4
 * into the state byte at [r5+9].
 *
 * Register results: r0/r1 = the coordinates read back (r0 becomes 4 when
 * the object settles), r2 = state byte * 2, r3 = the table entry
 * (or whatever sprite_setCoordinates leaves in r3 when it is called). */
Next sprite_fallingObjectStepDown_8116BC0(void) {
    R0 = 0;
    R1 = 8;
    GUEST_CALL(sprite_addCoordinates);   /* move down by (0, 8) */
    GUEST_CALL(sprite_getCoordinates);   /* r0 = x, r1 = y */

    R2 = LD8(R5 + 4) << 1;               /* state byte -> halfword offset */
    R3 = LD16(A_byte_8116BEC + R2);      /* stop depth for this state */

    SET_FLAGS_CMP(R3, R1);
    if ((s32)R3 <= (s32)R1) {            /* depth reached: settle here */
        R1 = R3;
        GUEST_CALL(sprite_setCoordinates);
        R0 = 4;
        ST8(R5 + 9, R0);                 /* advance to phase 4 */
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8116BFC ── */
/* sub_8116BFC(r4, r5 = state object, r6, r7 = state byte ptr):
 * Runs the two phase-update helpers (record_adjustNthSubRecordFieldC_8116C40, sub_8116C78), then checks
 * the byte at [r7]; if it has reached 8, store 8 into [r5+9] as well.
 * Leaves r0 = the loaded byte (or 8 on the store path) and the flags from
 * the final CMP/MOV for the caller to branch on. */
Next sub_8116BFC(void) {
    GUEST_CALL(record_adjustNthSubRecordFieldC_8116C40);
    GUEST_CALL(sub_8116C78);

    u32 counter = LD8(R7);
    R0 = counter;
    SET_FLAGS_CMP(counter, 8);
    if (counter == 8) {
        R0 = 8;             /* mov r0,#8 sets NZ only; C/V stay from the CMP */
        SET_FLAGS_NZ(R0);
        ST8(R5 + 9, 8);
    }
    RETURN_TO_CALLER();
}

/* ── sprite_scrollStepAndCullOffscreen_8116C14 ── */
/* sprite_scrollStepAndCullOffscreen_8116C14(r7 = sprite):
 * Nudge the sprite along one axis by 8x the signed value stored at
 * sprite+0x10, then look at the resulting coordinate. If it has moved
 * past zero into negative territory, clear the sprite's scale counter
 * and mark it unscalable (it has scrolled off / shrunk away).
 *
 * Outputs: r0/r1 hold the coordinates returned by sprite_getCoordinates;
 * flags come from the CMP against that coordinate, or (when the reset
 * call is made) from whatever that callee leaves behind. */
Next sprite_scrollStepAndCullOffscreen_8116C14(void) {
    s32 offset = LDS16(R7 + 0x10);

    R0 = (u32)offset << 3;
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    GUEST_CALL(sprite_getCoordinates);
    SET_FLAGS_CMP(R0, 0);

    if ((s32)R0 < 0)
        GUEST_CALL(sprite_clearScaleCounterAndMakeUnscalable_804668A);

    RETURN_TO_CALLER();
}

/* ── record_adjustNthSubRecordFieldC_8116C40 ── */
/* record_adjustNthSubRecordFieldC_8116C40: adjust the 16-bit value at +0xC of an Nth sub-record by +/-2,
 * clamped to 0x16..0x1E.
 *
 * The sub-record pointer comes from record_getNthSubRecordPtr_8116CD4 (called
 * with the caller's registers untouched; it picks the record itself).
 * Direction depends on the bytes at [r7+4] and [r5+4]:
 *   equal     -> increase by 2, clamped so the value never exceeds 0x1E
 *   different -> decrease by 2, clamped so the value never goes below 0x16
 *
 * inputs:  r4 r5 r6 r7 lr
 * outputs: r4 r5 r6 (unchanged)
 * clobbers: nz c v r0 r1 r2 r3 lr
 */
Next record_adjustNthSubRecordFieldC_8116C40(void) {
    GUEST_CALL(record_getNthSubRecordPtr_8116CD4);
    u32 record = R0;

    s32 limit = 0x16;
    s32 step = -2;
    u32 raise = 0;

    u32 lhs = LD8(R7 + 4);
    u32 rhs = LD8(R5 + 4);
    if (lhs == rhs) {
        limit = 0x1E;
        step = 2;
        raise = 1;
    }

    s32 value = (s32)LD16(record + 0xC) + step;

    if (raise) {
        if (value >= limit)
            value = limit;
    } else {
        if (value <= limit)
            value = limit;
    }

    ST16(record + 0xC, (u32)value);
    RETURN_TO_CALLER();
}

/* ── sub_8116C78 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_sprite_setSubEntryPaletteBank_800304A_result_sub_8116C78;
/* register form: generated wrapper: sprite_setSubEntryPaletteBank_800304A has no typed C interface yet */
static inline call_sprite_setSubEntryPaletteBank_800304A_result_sub_8116C78 call_sprite_setSubEntryPaletteBank_800304A_sub_8116C78(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
    return (call_sprite_setSubEntryPaletteBank_800304A_result_sub_8116C78){ R2 };
}

typedef struct { u32 r3; } call_sprite_setPalette_result;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result call_sprite_setPalette_sub_8116C78(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result){ R3 };
}

/* Advance a battle sprite's sub-palette animation state and apply the result.
 *
 * sprite_ptr (r5): the sprite's state block.  Byte 0 bit 6 is a "match"
 * flag, byte 4 is the alliance ID, byte 0xC is the current frame index,
 * byte 0xD is a per-frame countdown timer.
 * related_ptr (r7): a second object whose byte 4 is compared against
 * the sprite's alliance to decide whether the sprite participates in the
 * match animation.
 *
 * If the alliances match we switch to an animated palette-bank pair and
 * tick the timer from a small two-entry table (byte_8116CCC).  If the
 * timer reaches zero we load the next entry's frame/timer pair.  Either
 * way the (possibly default) bank and palette are handed to the sprite
 * helpers, and bit 6 of byte 0 is cleared. */
static void sub_8116C78_impl(u32 sprite_ptr, u32 related_ptr) {
    u32 bank = 0xD;
    u32 palette = 1;
    bool animate = false;

    if (LD8(related_ptr + 4) == LD8(sprite_ptr + 4)) {
        bank = 0xC;
        palette = 0;
        animate = true;
    }

    /* Clear the match flag (bit 6) in the sprite's control byte. */
    ST8(sprite_ptr, LD8(sprite_ptr) & ~0x40);

    if (animate) {
        u32 tbl = A_byte_8116CCC;
        u32 off = (u32)LD8(sprite_ptr + 0xC) << 2;
        bank = LD8(tbl + off);
        palette = LD8(tbl + off + 1);

        /* Decrement the frame timer (32-bit wrap mirrors the original SUBS). */
        u32 timer = LD8(sprite_ptr + 0xD);
        u32 new_timer = timer - 1;
        ST8(sprite_ptr + 0xD, (u8)new_timer);

        /* On underflow (timer was 0) load the next frame/timer pair. */
        if ((s32)new_timer < 0) {
            ST8(sprite_ptr + 0xC, LD8(tbl + off + 2));
            ST8(sprite_ptr + 0xD, LD8(tbl + off + 3));
        }
    }

    call_sprite_setSubEntryPaletteBank_800304A_sub_8116C78(bank, sprite_ptr);
    call_sprite_setPalette_sub_8116C78(palette, sprite_ptr);
}

Next sub_8116C78(void) {
    sub_8116C78_impl(R5, R7);
    RETURN_TO_CALLER();
}

/* ── record_getNthSubRecordPtr_8116CD4 ── */
/* record_getNthSubRecordPtr_8116CD4(r5 = record pointer): returns r5 + 16 * (high nibble of byte at r5+2),
 * i.e. selects one of sixteen 16-byte sub-records within the record. */
Next record_getNthSubRecordPtr_8116CD4(void) {
    u32 kind = (LD8(R5 + 2) >> 4) << 4;
    R0 = R5 + kind;
    RETURN_TO_CALLER();
}

/* ── sub_8116CE0 ── */
#define oBattleObject_BehaviorStep 8

/* register form: dispatch the battle object's behavior handler via the 3-entry jump table at
 * off_8116CF4. The table entries already have the Thumb bit set (+1). R1 carries the step
 * byte through to the handler unchanged; R0 is the loaded handler address; all other
 * registers (R2..R12, SP, LR apart from the call return) pass through. */
static void dispatch_behavior_step(u32 object_ptr) {
    u32 step_byte = LD8(object_ptr + oBattleObject_BehaviorStep);
    R1 = step_byte;
    R0 = LD32(A_off_8116CF4 + step_byte);
    GUEST_CALL_ADDR(R0);
}

/* sub_8116CE0(r5 = battle object pointer; r2 r3 r5 r10 r11 output, rest pass through) */
Next sub_8116CE0(void) {
    dispatch_behavior_step(R5);
    RETURN_TO_CALLER();
}

/* ── battle_initSpriteType23_8116D00 ── */
/* Object offsets for the battle sprite record pointed to by R5 */
#define oBattleSprite_Type       0x00   /* class/type ID byte */
#define oBattleSprite_Animation  0x04   /* animation index */
#define oBattleSprite_Variant    0x05   /* if non-zero, use alternate coordinates */
#define oBattleSprite_State      0x08   /* current behavior step */
#define oBattleSprite_SubState   0x09   /* y-offset sub-state */
#define oBattleSprite_AnIM1      0x0C   /* animation ID primary slot */
#define oBattleSprite_AnIM2      0x0D   /* animation ID secondary slot */

/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void call_initGuiSprite_sub_8116D00(u32 sprite_id, u32 gui_arg) {
    R0 = sprite_id;
    R1 = gui_arg;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation_sub_8116D00(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_sprite_loadAnimationData_sub_8116D00(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_sprite_noShadow_sub_8116D00(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void call_sprite_setSubEntryPaletteBank_sub_8116D00(u32 bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_sprite_setPalette_sub_8116D00(u32 pal) {
    R0 = pal;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_sprite_setPriority_sub_8116D00(u32 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void call_slotList_storeHalfwordAndClearField(u32 slot) {
    R0 = slot;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void call_sprite_setSubRecordFieldA_sub_8116D00(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void call_sprite_setCoordinates_sub_8116D00(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

/* register form: calls battle_objectRunBehaviorStep_8116D6C outside this file, which has no typed C interface yet */
static void call_battle_objectRunBehaviorStep(u32 sub_offset) {
    R2 = sub_offset;
    GUEST_CALL(battle_objectRunBehaviorStep_8116D6C);
}

/* Initialize a battle sprite object (type 0x23) with its animation, palette, and
 * slot settings, then immediately advance to the next behavior step.
 * gui_arg: second argument to initGuiSprite (passed through from R1).
 * obj: guest pointer to the battle sprite record (R5). */
static void init_battle_sprite_and_run_step(u32 gui_arg, u32 obj) {
    /* Write the sprite class/type ID */
    ST8(obj + oBattleSprite_Type, 0x23);

    /* Initialize GUI sprite slot 2, passing the caller's R1 context */
    call_initGuiSprite_sub_8116D00(2, gui_arg);

    /* Copy animation ID from offset 4 into the two animation slots at +0xC and +0xD */
    u8 anim_id = (u8)LD8(obj + oBattleSprite_Animation);
    ST8(obj + oBattleSprite_AnIM1, anim_id);
    ST8(obj + oBattleSprite_AnIM2, anim_id);

    /* Configure sprite rendering: animation, data load, no shadow */
    call_sprite_setAnimation_sub_8116D00(anim_id);
    call_sprite_loadAnimationData_sub_8116D00();
    call_sprite_noShadow_sub_8116D00();

    /* Palette and priority setup */
    call_sprite_setSubEntryPaletteBank_sub_8116D00(0xC);
    call_sprite_setPalette_sub_8116D00(0);
    call_sprite_setPriority_sub_8116D00(0);

    /* Reserve an OAM/sprite slot and initialize sub-record field */
    call_slotList_storeHalfwordAndClearField(0x38B);
    call_sprite_setSubRecordFieldA_sub_8116D00(0);

    /* Set behavior state to step 4 (active) */
    ST8(obj + oBattleSprite_State, 4);

    /* Choose coordinates based on variant flag at offset 5 */
    u32 x_coord;
    u32 y_offset;
    if ((u8)LD8(obj + oBattleSprite_Variant) != 0) {
        x_coord = 0xAA;
        y_offset = 4;
    } else {
        x_coord = 0x11A;
        y_offset = 0;
    }
    ST8(obj + oBattleSprite_SubState, y_offset);

    /* Position the sprite on screen */
    call_sprite_setCoordinates_sub_8116D00(x_coord, 0x97);

    /* Tail-advance: run the next behavior step (sets r0-r3, flags, possibly r5) */
    call_battle_objectRunBehaviorStep(y_offset);
}

/* battle_initSpriteType23_8116D00(r5 = battle sprite ptr, r1 = gui arg) */
Next battle_initSpriteType23_8116D00(void) {
    init_battle_sprite_and_run_step(R1, R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectRunBehaviorStep_8116D6C ── */
/* battle_objectRunBehaviorStep_8116D6C: run one step of the battle object at R5, then refresh its sprite.
 *
 * [R5+9] holds a byte offset into a small table of behavior handlers
 * (off_8116D84: 0 -> battle_moveSpriteLeftUntilX170_8116D90, 4 -> petMenu_updateSpriteVisibility_8116DB4, 8 -> battle_stepForwardAnimWait_8116DEC).
 * The original performs an indirect `mov lr, pc; bx r0` call, so each
 * handler returns straight back here; afterwards the object's sprite
 * animation is advanced by sprite_update before returning to the caller.
 * All other registers (R2..R12) are simply passed through to the handler. */
Next battle_objectRunBehaviorStep_8116D6C(void) {
    switch (LD8(R5 + 9)) {
    case 0: GUEST_CALL(battle_moveSpriteLeftUntilX170_8116D90); break;
    case 4: GUEST_CALL(petMenu_updateSpriteVisibility_8116DB4); break;
    case 8: GUEST_CALL(battle_stepForwardAnimWait_8116DEC); break;
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── battle_moveSpriteLeftUntilX170_8116D90 ── */
/* battle_moveSpriteLeftUntilX170_8116D90(r5 = battle object):
 * Drift the object's sprite 16 px to the left; once its x coordinate has
 * reached 0xAA or below, clamp it to exactly 0xAA and advance the object's
 * state byte to 4. r2 is left holding the 0xAA boundary constant on both
 * paths, matching the original. Flags reflect the last flag-setting
 * instruction (CMP on the early-out path, MOVS #4 plus callee flags
 * otherwise). */
Next battle_moveSpriteLeftUntilX170_8116D90(void) {
    R0 = -(u32)0x10;                    /* add (-16, 0) to the sprite position */
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    GUEST_CALL(sprite_getCoordinates);  /* r0 = x, r1 = y */

    R2 = 0xAA;                          /* boundary constant, kept in r2 */
    if ((s32)R0 > (s32)R2) {
        SET_FLAGS_CMP(R0, R2);
        RETURN_TO_CALLER();
    }

    R0 = R2;                            /* snap x back to the boundary... */
    GUEST_CALL(sprite_setCoordinates);

    R0 = 4;                             /* ...and mark the object as finished */
    SET_FLAGS_NZ(R0);
    ST8(R5 + 9, R0);
    RETURN_TO_CALLER();
}

/* ── petMenu_updateSpriteVisibility_8116DB4 ── */
/* petMenu_updateSpriteVisibility_8116DB4(r5 = sprite object):
 * Keeps a PET-menu-linked sprite (e.g. the MegaMan sprite shown on the PET)
 * in sync with the PET menu's state:
 *   - ePETMenuData+4 (menu substate) being 8 means the sprite should be shown;
 *     the show/hide flag is written to object+0xC, and whenever it changes the
 *     sprite's animation is (re)started with no shadow.
 *   - sprite_update is then run for the object.
 *   - When the menu's main state (ePETMenuData+0) has reached 8, object+0x9 is
 *     set to 8 to mark the sprite as done; r0..r3 and the nz/c/v flags are left
 *     as sprite_update / the final state comparison left them.
 * Preserves r4, r5 and the incoming r7. */
Next petMenu_updateSpriteVisibility_8116DB4(void) {
    u32 petMenuSubstate = LD8(A_ePETMenuData + 4);
    u32 showSprite = (petMenuSubstate == 8) ? 1u : 0u;

    /* Update the object's visible flag; restart the animation only on change. */
    ST8(R5 + 0xC, showSprite);
    if ((u8)LD8(R5 + 0xD) != showSprite) {
        ST8(R5 + 0xD, showSprite);
        R0 = showSprite;
        GUEST_CALL(sprite_setAnimation);      /* (u8 a1) -> void */
        GUEST_CALL(sprite_loadAnimationData); /* () -> void */
        GUEST_CALL(sprite_noShadow);          /* () -> void */
    }

    GUEST_CALL(sprite_update);                /* leaves results in r1..r3 */

    u32 petMenuState = LD8(A_ePETMenuData);
    if (petMenuState == 8) {
        /* Menu finished: flag the object with state 8. */
        R0 = 8;
        ST8(R5 + 9, 8);
        SET_FLAGS_NZ(8);
    } else {
        /* Keep the state byte in r0 and the comparison flags for the caller. */
        R0 = petMenuState;
        SET_FLAGS_CMP(petMenuState, 8);
    }
    RETURN_TO_CALLER();
}

/* ── battle_stepForwardAnimWait_8116DEC ── */
/* battle_stepForwardAnimWait_8116DEC(r5 = object state):
 * Runs sprite_addCoordinates for the "step forward" animation (pose 0x16,
 * parameter 0). The call returns the number of frames the animation has
 * run; once that reaches 0x11A the object's state byte at [r5+8] is set
 * to 8 to signal that the movement finished.
 * Flags: as the final CMP (elapsed vs 0x11A), except on the fall-through
 * path where the trailing "mov r0,#8" overwrites only n/z. */
Next battle_stepForwardAnimWait_8116DEC(void) {
    R0 = 0x16;
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    u32 elapsed = R0;
    u32 limit = 0x11A;
    R2 = limit;
    SET_FLAGS_CMP(elapsed, limit);
    if ((s32)elapsed >= (s32)limit) {
        R0 = 8;
        SET_FLAGS_NZ(8);
        ST8(R5 + 8, 8);
    }
    RETURN_TO_CALLER();
}

/* ── stateDispatcher_3states_8116E10 ── */
/* stateDispatcher_3states_8116E10: state dispatcher for the object in R5.
 *
 * Reads the word offset of the current state from [R5+8] and tail-calls the
 * matching handler out of the table at off_8116E24:
 *   offset 0 -> battle_objectInit_8116E30, 4 -> battle_objectUpdateByKind_8116F34, 8 -> sub_8116FD4.
 *
 * The original does this with `mov lr, pc; bx r0` followed by `pop {pc}`,
 * so the handler returns straight to this function's caller: it is a tail
 * call. The handler is entered with R0 = its own table entry (as the
 * original leaves the loaded target address in R0) and R1 = the table
 * offset; every other register passes through untouched, and the handler's
 * results (registers and flags) become this function's results.
 */
Next stateDispatcher_3states_8116E10(void) {
    u32 sel = LD8(R5 + 8);                      /* word offset into the handler table */
    u32 handler = LD32(A_off_8116E24 + sel);    /* Thumb address of the state handler */

    R1 = sel;
    R0 = handler;

    switch (sel) {
    case 0:
        GUEST_CALL(battle_objectInit_8116E30);
        break;
    case 4:
        GUEST_CALL(battle_objectUpdateByKind_8116F34);
        break;
    case 8:
        GUEST_CALL(sub_8116FD4);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectInit_8116E30 ── */
/* Struct/object layout offsets for battle objects (from disassembly) */
#define oBattleObject_kind       0   /* kind byte stored here */
#define oBattleObject_flag       4   /* flag byte at offset 4 */
#define oBattleObject_team       5   /* team index at offset 5 */
#define oBattleObject_state      8   /* state byte at offset 8 */

/* Sprite kind values chosen by [obj+4] */
#define SPRITE_KIND_BASE         0x23
#define SPRITE_KIND_ALT          0xa3

/* initGuiSprite_8002770 sub-kind */
#define INITGUI_SUBKIND          6

/* Ready state stored at [obj+8] before per-kind update */
#define BATTLE_OBJ_STATE_READY   4

/* Guest addresses of the data tables referenced by the original code.
 * The original loads these addresses from off_* tables in guest memory, then
 * dereferences them; we mirror that exactly via LD8/LD32. */
#define A_off_8116F0C            0x08116F0Cu
#define A_off_8116EB8            0x08116EB8u
#define A_off_8116EC8            0x08116EC8u
#define A_off_8116EEC            0x08116EECu
#define A_off_8116EFC            0x08116EFCu
#define A_byte_8116F18           0x08116F18u
#define A_byte_8116F1D           0x08116F1Du
#define A_byte_8116F23           0x08116F23u
#define A_off_8116ED4            0x08116ED4u
#define A_byte_8116F28           0x08116F28u
#define A_byte_8116F2D           0x08116F2Du

/* ---- Register-form helpers (call game functions outside this file) ---- */

/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void call_initGuiSprite(u32 kind, u32 workspace) {
    R0 = kind;
    R1 = workspace;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_sprite_loadAnimationData(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_sprite_noShadow(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_sprite_setPalette(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_sprite_setPriority(u32 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void call_slotList_storeHalfwordAndClearField_sub_8116E30(u32 value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void call_sprite_setSubEntryPaletteBank(u32 bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void call_sprite_setSubRecordFieldA(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls battle_objectUpdateByKind_8116F34 outside this file, which has no typed C interface yet */
static void call_battle_objectUpdateByKind(void) {
    GUEST_CALL(battle_objectUpdateByKind_8116F34);
}

/* ---- Typed implementation ---- */

/* Load a u32 from the team-indexed pointer table at `table_addr` in guest memory. */
static u32 load_team_ptr(u32 table_addr, u32 team) {
    return LD32(table_addr + (team << 2));
}

/* Initialize a battle object: pick sprite kind from [obj+4], set up the GUI sprite using the
 * team-indexed workspace at [obj+5], apply per-team/per-object sprite parameters, mark the
 * object ready (state = 4), then run the per-kind update. */
static void battle_object_setup_and_update(u32 obj) {
    u8 flag   = LD8(obj + oBattleObject_flag);
    u8 team   = LD8(obj + oBattleObject_team);

    /* Choose sprite kind: 0x23 normally, 0xa3 when the flag byte is non-zero. */
    u8 kind = (flag != 0) ? SPRITE_KIND_ALT : SPRITE_KIND_BASE;
    ST8(obj + oBattleObject_kind, kind);

    /* Initialize the GUI sprite in the workspace selected by the team. */
    u32 workspace = load_team_ptr(A_off_8116F0C, team);
    call_initGuiSprite(INITGUI_SUBKIND, workspace);

    /* Animation, animation data load, no shadow, palette - all indexed by flag. */
    call_sprite_setAnimation(LD8(A_byte_8116F18 + flag));
    call_sprite_loadAnimationData();
    call_sprite_noShadow();
    call_sprite_setPalette(LD8(A_byte_8116F1D + flag));

    /* Sprite priority: priority table chosen by team, byte by flag. */
    {
        u32 prio_table = load_team_ptr(A_off_8116EB8, team);
        call_sprite_setPriority(LD8(prio_table + flag));
    }

    /* Slot-list store: u32 table chosen by team, entry by flag (each entry is a u32). */
    {
        u32 slot_table = load_team_ptr(A_off_8116EC8, team);
        call_slotList_storeHalfwordAndClearField_sub_8116E30(LD32(slot_table + (flag << 2)));
    }

    /* Sub-entry palette bank: byte table chosen by team, entry by flag. */
    {
        u32 pal_table = load_team_ptr(A_off_8116EEC, team);
        call_sprite_setSubEntryPaletteBank(LD8(pal_table + flag));
    }

    /* Sub-record field A: byte table chosen by team, entry by flag. */
    {
        u32 subf_table = load_team_ptr(A_off_8116EFC, team);
        call_sprite_setSubRecordFieldA(LD8(subf_table + flag));
    }

    /* Mark the object ready and dispatch to its per-kind update. */
    ST8(obj + oBattleObject_state, BATTLE_OBJ_STATE_READY);
    call_battle_objectUpdateByKind();
}

/* ---- Thin adapter ---- */

/* battle_objectInit_8116E30(r5 = battle object) -> r0..r12, lr clobbered by callees (per disasm contract) */
Next battle_objectInit_8116E30(void) {
    battle_object_setup_and_update(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectUpdateByKind_8116F34 ── */
/* Jump table jt_8116F4C: Thumb entry points of the per-object update handlers,
 * exactly as they appear in the table the original indexes through. */
enum {
    ADDR_sub_8116F60                       = 0x08116F60 + 1,
    ADDR_battle_objectUpdateWithFieldC_8116F8C = 0x08116F8C + 1,
    ADDR_sub_8116F9C                       = 0x08116F9C + 1,
};

/* battle_objectUpdateByKind_8116F34: run one battle object's type-specific update, then its sprite update.
 * R5 points at the battle object; the byte at obj+4 selects which handler runs:
 *   0/3 -> sub_8116F60, 1/4 -> battle_objectUpdateWithFieldC_8116F8C, 2 -> sub_8116F9C.
 * For fidelity with the original `mov lr, pc; bx r0`, each handler is entered with
 * R0 = its own jump-table entry and R1 = type*4 (result of the `lsl r1, r1, #2`).
 * Tail behaviour: after the handler returns, sprite_update is called on the object. */
Next battle_objectUpdateByKind_8116F34(void) {
    u32 kind = LD8(R5 + 4);

    switch (kind) {
    case 0:
    case 3:
        R0 = ADDR_sub_8116F60;
        R1 = kind << 2;
        GUEST_CALL(sub_8116F60);
        break;

    case 1:
    case 4:
        R0 = ADDR_battle_objectUpdateWithFieldC_8116F8C;
        R1 = kind << 2;
        GUEST_CALL(battle_objectUpdateWithFieldC_8116F8C);
        break;

    case 2:
        R0 = ADDR_sub_8116F9C;
        R1 = kind << 2;
        GUEST_CALL(sub_8116F9C);
        break;

    default:
        /* Unreachable: the object's update kind is always 0..4. (The original
         * would index past jt_8116F4C and jump to garbage.) */
        break;
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── battle_objectUpdateWithFieldC_8116F8C ── */
/* battle_objectUpdateWithFieldC_8116F8C: update hook for the object in r5.
 * Runs the "before" update for the object, then passes the object's
 * halfword at +0xC to the "after" update routine. All register results
 * (r0-r3, flags) are simply whatever sub_81170C0 leaves behind. */
Next battle_objectUpdateWithFieldC_8116F8C(void) {
    GUEST_CALL(sub_81170AC);

    R0 = LD16(R5 + 0xC);
    GUEST_CALL(sub_81170C0);

    RETURN_TO_CALLER();
}

/* ── sprite_positionSubmenuEntries_8116FDC ── */
/* register form: calls getStructFrom2008450 outside this file, which has no typed C interface yet */
static u32 lookupPoolEntry(u32 index) {
    R0 = index;
    GUEST_CALL(getStructFrom2008450);
    if (ZERO_FLAG())
        return 0;               /* Z set: no object registered at this index */
    return R1;                  /* address of the object's struct */
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static u32 setSpriteCoordinates(u32 x, u32 y, u32 entry) {
    R0 = x;
    R1 = y;
    R5 = entry;                 /* the callee keeps working on the entry through r5 */
    GUEST_CALL(sprite_setCoordinates);
    return R5;                  /* entry pointer as the callee left it */
}

/* Move the three sprites belonging to the submenu entries starting at pool
 * index firstIndex, and set or clear each entry's "off screen" flag (bit 1
 * of its status byte).
 *
 * The selected entry (firstIndex itself) is drawn at the anchor (baseX,
 * baseY).  Its two neighbours sit 13 pixels left and right of the anchor
 * (byte_8117060 holds one {dx, dy} word pair per slot), but while their
 * halfword at +0xE is zero they crowd towards the anchor instead: 7 pixels
 * over while their counter at +0xC is still below 10, 3 pixels over after
 * that.  Bit 1 stays set unless the anchor is on screen (baseX < 0x38) and
 * the submenu id in sSubmenu is 0x18 or 8.
 *
 * r5 is clobbered by the sprite call but the original restores it on exit,
 * so it is passed in and handed back unchanged. */
static u32 moveSubmenuEntrySprites(u32 baseX, u32 baseY, u32 firstIndex, u32 r5) {
    u32 endIndex = firstIndex + 3;
    u32 slot = 0;

    for (u32 index = firstIndex; (s32)index < (s32)endIndex; index++, slot++) {
        u32 entry = lookupPoolEntry(index);
        if (entry == 0)
            continue;           /* nothing registered at this index */

        u32 xOffset;
        if (index != firstIndex && LD16(entry + 0xE) == 0) {
            /* Side entries huddle towards the selected one while it counts up. */
            xOffset = ((s32)LD16(entry + 0xC) < 10) ? 0xFFFFFFF9u : 0xFFFFFFFDu;
        } else {
            xOffset = LD32(A_byte_8117060 + slot * 8);
        }
        u32 yOffset = LD32(A_byte_8117060 + slot * 8 + 4);

        entry = setSpriteCoordinates(baseX + xOffset, baseY + yOffset, entry);

        u32 submenuId = LD8(A_sSubmenu);
        u32 status = LD8(entry);
        if ((s32)baseX >= 0x38 || (submenuId != 0x18 && submenuId != 8))
            ST8(entry, status | 2);     /* anchor off screen / wrong menu: hide */
        else
            ST8(entry, status & ~2u);   /* anchor on screen: show */
    }
    return r5;
}

/* sprite_positionSubmenuEntries_8116FDC(r0 = anchor x, r1 = anchor y, r2 = first pool index) -> r3 left
 * by the sprite callee, r5 restored, r4/r6/r7 untouched. */
Next sprite_positionSubmenuEntries_8116FDC(void) {
    R5 = moveSubmenuEntrySprites(R0, R1, R2, R5);
    RETURN_TO_CALLER();
}

/* ── sprite_setStructHalfwordPairOnThree_8117084 ── */
/* sprite_setStructHalfwordPairOnThree_8117084(r0 = halfword A, r1 = halfword B, r2 = first struct index):
 * Walks the three struct indices r2..r2+2. For each index, looks up the
 * corresponding struct from the pool at 0x2008450; while a struct is found,
 * its halfwords at +0xC and +0xE are filled with the two values passed in.
 * Stops at the first index that has no struct (lookup returns r0 == 0).
 * On a normal run-through the flags are those of the final index comparison;
 * on an early abort they are whatever the lookup left behind.
 * r4-r7 are untouched, and r1/r2 end up holding the values the last lookup
 * returned, exactly as in the original. */
Next sprite_setStructHalfwordPairOnThree_8117084(void) {
    u32 valueA = R0;
    u32 valueB = R1;
    u32 index = R2;
    u32 endIndex = index + 3;

    for (;;) {
        R0 = index;
        GUEST_CALL(getStructFrom2008450);
        if (R0 == 0)
            break; /* no struct for this index: stop, keeping the callee's flags/registers */

        u32 entry = R1; /* struct pointer returned by the lookup */
        ST16(entry + 0xC, valueA);
        ST16(entry + 0xE, valueB);
        R0 = valueB;

        index++;
        if ((s32)index >= (s32)endIndex) {
            SET_FLAGS_CMP(index, endIndex);
            break;
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_81170C0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n, c, v; } call_sprite_setUnk0x2c_result;
/* register form: generated wrapper: sprite_setUnk0x2c has no typed C interface yet */
static inline call_sprite_setUnk0x2c_result call_sprite_setUnk0x2c(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setUnk0x2c);
    return (call_sprite_setUnk0x2c_result){ R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_81170C0:
 *   inputs:  r0 r5 lr
 *   outputs: r5
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *
 * Draws two values from the BIOS PRNG (svc 6) and uses them to punch holes
 * out of a default bitmask (0x7FFFFE00), then hands the resulting mask to
 * sprite_setUnk0x2c (with the sprite object pointer in r5).
 *
 * The two PRNG draws are chained: each svc 6 call takes the incoming r0/r1
 * as (seed, limit) and returns a random value in r0 and the updated state in
 * r1. The original uses the first call's r1 as the second call's r0, sets
 * r1 to 0x0A, then uses r0 and r1 after the second call. Because svc 6
 * reads and writes guest registers directly, the draw is a small
 * register-form helper. */

#define DEFAULT_SPRITE_MASK   0x7FFFFE00u
#define MASK_BIT_FIRST_DRAW   0x80000000u
#define MASK_BIT_SECOND_DRAW  0x10000000u
#define MASK_BIT_SECOND_STATE 0x00040000u

/* One BIOS svc 6 (Random) draw: inputs r0/r1, outputs r0/r1. */
static u32 bios_random(u32 r0_in, u32 r1_in, u32 *r1_out) {
    /* register form: svc 6 is a BIOS service that consumes and returns r0/r1 */
    R0 = r0_in;
    R1 = r1_in;
    swi_call(6);
    u32 result = R0;
    *r1_out = R1;
    return result;
}

/* ARM register-shift LSR (no flags): shift amount is the low byte; a count
 * of 32 or more yields zero. */
static u32 lsr_byte(u32 value, u32 amount) {
    u32 s = amount & 0xFFu;
    return (s >= 32u) ? 0u : (value >> s);
}

/* Punch the mask holes out of DEFAULT_SPRITE_MASK.  The 0x10000000 bit is
 * cleared only if either PRNG draw is non-zero (the branch on the second
 * draw's value, falling through to the first draw's test); when the second
 * draw is zero the shift count is zero, i.e. the whole bit is cleared. */
static u32 build_sprite_mask(u32 first_draw, u32 second_draw, u32 second_state) {
    u32 mask = DEFAULT_SPRITE_MASK;
    mask &= ~lsr_byte(MASK_BIT_FIRST_DRAW, first_draw);
    if (first_draw != 0 || second_draw != 0)
        mask &= ~lsr_byte(MASK_BIT_SECOND_DRAW, second_draw);
    mask &= ~lsr_byte(MASK_BIT_SECOND_STATE, second_state);
    return mask;
}

static void draw_sprite_random_mask(u32 first_seed, u32 object) {
    u32 first_state, second_draw, second_state;
    u32 first_draw = bios_random(first_seed, 0x64, &first_state);
    second_draw = bios_random(first_state, 0x0A, &second_state);
    call_sprite_setUnk0x2c(build_sprite_mask(first_draw, second_draw, second_state), object);
}

Next sub_81170C0(void) {
    draw_sprite_random_mask(R0, R5);
    RETURN_TO_CALLER();
}

/* ── sub_8117690 ── */
/* Jump-table dispatch over a battle-context byte (offset 8). The three table
 * entries are THUMB targets (low bit set). Original behaviour:
 *   push {lr}
 *   r0 = &table; r1 = ctx[8]; r0 = table[r1]
 *   lr = retaddr (0x811769C, the pop); bx r0
 *   pop {pc}      ; returns to caller via the saved lr
 * The target is a THUMB function (bx with bit0 set); the callee runs with our
 * lr, so on return the original pops our saved pc and resumes here. We model
 * it directly so the guest stack matches the original. */

static u32 dispatch_via_table(u32 context, u32 caller_lr) {
    u32 idx    = LD8(context + 8);
    u32 target = LD32(A_off_81176A4 + idx);

    /* register form: callee (sub_81176B0/C0/C8) reads guest stack relative to SP,
     * so we must reproduce the original's push {lr}/pop {pc} to match layout. */
    SP -= 4;
    ST32(SP, caller_lr);
    R0 = target;
    R1 = idx;
    GUEST_CALL_ADDR(target);
    u32 result = R0;
    SP += 4;
    return result;
}

/* sub_8117690(r5 = battle context) -> r0 (handler result) */
Next sub_8117690(void) {
    R0 = dispatch_via_table(R5, LR);
    RETURN_TO_CALLER();
}

/* ── sub_81176B0 ── */
/* sub_81176B0(r5 = state struct):
 * Mark the entity as active (flag 1) and set its mode/counter field at +8 to 4,
 * then run the shared update at sub_81176C0. Flags and r0-r3 on return come
 * from that call. */
Next sub_81176B0(void) {
    ST8(R5, 1);
    ST8(R5 + 8, 4);

    GUEST_CALL(sub_81176C0);

    RETURN_TO_CALLER();
}

/* ── sub_81176C0 ── */
/* sub_81176C0: tail-call wrapper for battle_playObjectTripleSound_81176D0. */
Next sub_81176C0(void) {
    GUEST_CALL(battle_playObjectTripleSound_81176D0);
    RETURN_TO_CALLER();
}

/* ── battle_playObjectTripleSound_81176D0 ── */
/* register form: calls one of three known object getters selected by loop index;
   the callees read R5 (object pointer) and may clobber R4, so save/restore R4-R7 */
static u32 call_object_getter(u32 obj_ptr, u32 index) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R5 = obj_ptr;
    R6 = index;
    switch (index) {
    case 0: GUEST_CALL(battle_getObjectPairOddByteClamped8_811774C); break;
    case 1: GUEST_CALL(sub_8117764); break;
    default: GUEST_CALL(sub_8117768); break;
    }
    u32 result = R0;
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return result;
}

/* register form: calls sub_802FE28 outside this file, which has no typed C interface yet;
   preserves R4-R7 per the caller's contract */
static void play_sound_event(u32 sound_id, u16 packed_attr) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = sound_id;
    R1 = packed_attr;
    R2 = 0;
    R3 = 3;
    GUEST_CALL(sub_802FE28);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* Read the 32-bit base sound-ID pair from the type-indexed ROM table.
 * Table layout at byte_8117724: two u16 entries (high, low) per 4-byte slot;
 * slot selected by the object's type byte at offset 4. */
static u32 read_sound_base(u32 obj_ptr) {
    u32 off = (u32)LD8(obj_ptr + 4) * 4;
    u32 hi = LD16(A_byte_8117724 + off);
    u32 lo = LD16(A_byte_8117724 + off + 2);
    return (hi << 16) | lo;
}

/* Play three sound events for a battle object, advancing the upper half of the
 * base sound-ID by 0x80000 each iteration.  Each call packs the getter result
 * into a 16-bit attribute alongside fixed flags 0xA000 and 0x800.
 * Returns 0x80000 (the value left in R0 by the final MOV/LSL before pop). */
static u32 battle_play_object_sounds(u32 obj_ptr) {
    u32 sound_base = read_sound_base(obj_ptr);
    for (int i = 0; i < 3; i++) {
        u32 mod = call_object_getter(obj_ptr, (u32)i);
        u16 packed = (u16)(0xA800 | mod);
        play_sound_event(sound_base, packed);
        sound_base += 0x00080000;
    }
    return 0x00080000;
}

/* battle_playObjectTripleSound_81176D0(r5 = battle-object pointer) -> r0 = 0x80000;
 * flags from CMP r6, 3 with r6 = 3 (Z=1, N=0, C=1, V=0).
 * r4-r7 preserved (push/pop in original). */
Next battle_playObjectTripleSound_81176D0(void) {
    R0 = battle_play_object_sounds(R5);
    SET_FLAGS_CMP(3, 3);
    RETURN_TO_CALLER();
}

/* ── battle_getObjectPairOddByteClamped8_811774C ── */
/* battle_getObjectPairOddByteClamped8_811774C(r5 = object/state pointer):
 * Look up a byte in the table at unk_201C400. The index is taken from the
 * byte at [r5+4], doubled and incremented (i.e. the odd entry of a byte
 * pair), and the result is clamped to at most 8.
 * Returns: r0 = clamped table value, r1 = table index.
 * Flags: as if `cmp value, #8` ran; on the clamp path n/z are then
 * re-set by `mov r0, #8` (c/v keep the comparison's values). */
Next battle_getObjectPairOddByteClamped8_811774C(void) {
    u32 index = (LD8(R5 + 4) << 1) + 1;
    u32 value = LD8(A_unk_201C400 + index);

    SET_FLAGS_CMP(value, 8);
    if ((s32)value > 8) {
        value = 8;
        SET_FLAGS_NZ(value);
    }

    R0 = value;
    R1 = index;
    RETURN_TO_CALLER();
}

/* ── sub_8117764 ── */
/* sub_8117764: return 9, setting Z/N from that value. */
Next sub_8117764(void) {
    R0 = 9;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_dispatchModeHandler_81178D8 ── */
/* object_dispatchModeHandler_81178D8: behaviour dispatcher for the object whose pointer is in R5.
 *
 * The byte at [R5+8] is the object's mode, stored already scaled as a byte
 * offset into the handler pointer table at off_81178EC (the game only uses
 * 0, 4 or 8, which select sprite_slotInitKind23_81178FC / sub_811794C / sub_8117990). The word
 * read from the table is the handler's Thumb entry address (odd). That word
 * is the call target and is also passed to the handler itself in R0, along
 * with the mode byte in R1 and the shared work-area address in R7; R2-R6 and
 * R8-R12 pass through untouched. Whatever the handler leaves in R0/R1 and
 * the flags is what the caller of this dispatcher sees.
 *
 * Note the mode byte is added to the table base *directly* (ldr r0, [r0,r1]),
 * so it must not be re-scaled here -- the loaded word, not mode/4, decides
 * which handler runs.
 */

#define HANDLER_SUB_81178FC 0x081178FDu /* = sprite_slotInitKind23_81178FC + 1 (Thumb) */
#define HANDLER_SUB_811794C 0x0811794Du /* = sub_811794C + 1 (Thumb) */
#define HANDLER_SUB_8117990 0x08117991u /* = sub_8117990 + 1 (Thumb) */

Next object_dispatchModeHandler_81178D8(void) {
    u32 mode = (u8)LD8(R5 + 8);

    R7 = A_byte_2011C10;
    R1 = mode;

    /* Fetch the handler pointer exactly as the original does: the mode byte
     * is added straight to the table base, and the loaded word is both the
     * branch target and the value handed to the callee in R0. */
    u32 handler = LD32(A_off_81178EC + mode);
    R0 = handler;

    if (handler == HANDLER_SUB_81178FC) {
        GUEST_CALL(sprite_slotInitKind23_81178FC);
    } else if (handler == HANDLER_SUB_811794C) {
        GUEST_CALL(sub_811794C);
    } else if (handler == HANDLER_SUB_8117990) {
        GUEST_CALL(sub_8117990);
    }
    /* Any other word can only result from reading past the three-entry table
     * (mode >= 12, which real game states never produce). In that case the
     * loaded pointer simply remains in R0 and nothing else is disturbed. */

    RETURN_TO_CALLER();
}

/* ── sprite_slotInitKind23_81178FC ── */
/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void set_animation_sub_81178FC(u8 anim)               { R0 = anim;             GUEST_CALL(sprite_setAnimation); }
/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void load_animation_data_sub_81178FC(void)            {                         GUEST_CALL(sprite_loadAnimationData); }
/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_no_shadow_helper(void)        {                         GUEST_CALL(sprite_noShadow); }
/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void set_palette_bank(u8 bank)            { R0 = bank;              GUEST_CALL(sprite_setSubEntryPaletteBank_800304A); }
/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void store_slot_halfword_sub_81178FC(u32 val)         { R0 = val;               GUEST_CALL(slotList_storeHalfwordAndClearField_800306C); }
/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void set_sub_record_field_a_sub_81178FC(u32 val)      { R0 = val;               GUEST_CALL(sprite_setSubRecordFieldA_8003060); }
/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void set_sprite_priority(u32 prio)        { R0 = prio;              GUEST_CALL(sprite_setPriority_8002E14); }
/* register form: calls sub_811794C outside this file, which has no typed C interface yet */
/* leftover unused: sub_811794C's r0 (sprite_slotInitKind23_81178FC's callers read neither r0 nor the flags) */
static u32 call_sub_811794C(void)                {                         GUEST_CALL(sub_811794C); return R0; }
/* register form: calls sub_8117998 outside this file, which has no typed C interface yet */
static u8   call_sub_8117998(void)               {                         GUEST_CALL(sub_8117998); return (u8)R0; }
/* register form: calls loc_8002774 outside this file, which has no typed C interface yet */
static void run_byte_script(u32 a1, u32 a2)      { R0 = a1; R1 = a2;       GUEST_CALL(loc_8002774); }

/* Initialize the sprite state slot pointed to by slot_ptr.
 * Reads sub_kind from [slot_ptr+4], stamps kind 0x23, runs the byte script,
 * picks and loads an animation, configures palette/slot/priority,
 * sets [slot_ptr+8] = 4, and calls sub_811794C. Returns the r0 it left behind. */
static u32 init_sprite_slot(u32 slot_ptr) {
    u8 sub_kind = LD8(slot_ptr + 4);
    u8 anim;

    ST8(slot_ptr, 0x23);
    run_byte_script(0x10, A_byte_2030204);

    anim = call_sub_8117998();
    ST8(slot_ptr + 5, anim);
    set_animation_sub_81178FC(anim);
    load_animation_data_sub_81178FC();
    sprite_no_shadow_helper();

    set_palette_bank((u8)(9 + sub_kind));

    store_slot_halfword_sub_81178FC(0x180);
    set_sub_record_field_a_sub_81178FC(1);
    set_sprite_priority(0);

    ST8(slot_ptr + 8, 4);
    return call_sub_811794C();
}

/* sprite_slotInitKind23_81178FC(r5 = slot ptr) -> flags from sub_811794C */
Next sprite_slotInitKind23_81178FC(void) {
    R0 = init_sprite_slot(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811794C ── */
Next sub_811794C(void) {
    /* Frame-update step for the in-battle HP/shield bar UI tile. Picks the
     * tile variant (r4 = 0x21 = max, 0x23 = normal) based on the current
     * battle-chip art, places the sprite, advances its animation, then
     * writes back the new frame. */

    u32 variant = 0x23;
    GUEST_CALL(sub_8047D84);
    if ((s32)R0 != 0) {
        u8 first = LD8(R0);
        if (first == 1 && LD16(R0 + 2) == 0x70) {
            variant = 0x21;
        }
    }
    ST8(R5, (u8)variant);

    /* Place sprite: x = 0x24 + ((0x1E - (sprite_palette_slot - 0xC)) << 3), y = 0x38 */
    u32 palette_idx = LD8(R7 + 8);
    R0 = 0x24 + ((0x1E - (palette_idx - 0xC)) << 3);
    R1 = 0x38;
    GUEST_CALL(sprite_setCoordinates);

    /* Advance the tile animation by one step. */
    GUEST_CALL(sub_8117998);

    /* If the frame index changed, push it to the sprite engine. */
    u32 frame = R0;
    if (frame != (u32)LD8(R5 + 5)) {
        ST8(R5 + 5, (u8)frame);
        GUEST_CALL(sprite_setAnimation);
        GUEST_CALL(sprite_loadAnimationData);
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_8117998 ── */
/* sub_8117998: fetches a lookup-table pointer from sub_8047D84, then computes
 * (byte[table] - 1) * 2 + byte[r5 + 4]; r0 = the resulting offset.
 * The entry index is 1-based, hence the -1. r4-r7 and lr are only shuffled
 * through the stack in the original; the "mov r6, #0" there is dead because
 * the pop overwrites r6, so this wrapper simply preserves r4-r7. */
Next sub_8117998(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(sub_8047D84);
    u32 table = R0;

    R0 = ((LD8(table) - 1) << 1) + LD8(savedR5 + 4);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── dispatchStateHandler_81179AE ── */
/* dispatchStateHandler_81179AE: per-frame state dispatcher.
 *
 * Fetches the active work area through sub_8047D84 (which locates it via
 * the singleton pointer at byte_2011C10), then invokes one of four state
 * handlers depending on the state byte at the start of that work area.
 * The handlers run with r1 = state * 4 and r4 = work area; r2/r3 and
 * r5-r12 pass straight through to them. r4-r7 are preserved for the
 * caller, exactly as the original's push/pop does. */
Next dispatchStateHandler_81179AE(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R7 = A_byte_2011C10;
    GUEST_CALL(sub_8047D84);

    u32 work = R0;
    u32 state = LD8(work);

    R1 = state << 2;
    R4 = work;
    switch (state) {
    case 0: GUEST_CALL(sub_81179E0); break;
    case 1: GUEST_CALL(keyItemMenu_renderDescription_81179E4); break;
    case 2: GUEST_CALL(folder_DrawSelectedChipCodeCounts_8117A14); break;
    default: GUEST_CALL(sub_8117AB4); break; /* state 3; larger values are unreachable */
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── keyItemMenu_renderDescription_81179E4 ── */
/* keyItemMenu_renderDescription_81179E4: render the description of the selected key item.
 * r4 = key item menu state (halfword at +2 is the item id),
 * r7 = menu context ([r7+0x24] selects which gfx block to render),
 * r10 = toolkit. The result of the key item lookup is stored into the
 * toolkit's unk_200A220 work struct. */
Next keyItemMenu_renderDescription_81179E4(void) {
    /* original push {r4-r7,lr}: r4-r7 are read here and restored on exit */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* look up the key item (id in the halfword at menu_state+2) */
    R0 = LD16(R4 + 2);
    GUEST_CALL(CheckKeyItem);

    /* remember the lookup result in the unk_200A220 work struct */
    u32 work = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(work + 8, R0);

    /* render the item's text graphic:
     * r0 = source selector from the menu context, r1 = 0x12,
     * r2 = text buffer, r3 = VRAM destination,
     * r4/r5 = tile layout, r6 = source data, r7 = 0 */
    R0 = LD32(R7 + 0x24);
    R1 = 0x12;
    R2 = A_unk_2016200;
    R3 = 0x6013520;
    R4 = 2;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* pop {r4-r7} */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── folder_DrawSelectedChipCodeCounts_8117A14 ── */
/* Folder screen, selected-entry redraw: render the four code letters of the
 * chip shown at *entry (chip id halfword at +2) into the 0x200-byte tile
 * buffer at unk_2016300, then draw the per-code "copies in folder" tallies
 * by scanning the folder, and queue the finished tiles to VRAM at 0x6013B40.
 *
 * The drawing callees keep the tile-buffer write cursor in guest r6: each one
 * advances it and hands it back, so the cursor is threaded from call to call.
 * They also read guest r4 (the chip id whose own folder entry is skipped when
 * counting) and r5 (the chip's database entry), so those registers are loaded
 * around the calls exactly as the original does. r4-r7 themselves are
 * preserved outputs of this function: the original saves/restores them, so
 * the entry values are passed in and handed back untouched. */

#define CODE_TILE_BUFFER      A_unk_2016300
#define CODE_TILE_BUFFER_SIZE 0x200u      /* 0x20 << 4 in the original */
#define CODE_TILE_VRAM_DEST   0x6013B40u

/* r0-r3 exactly as the final callee left them; callers of the original see
 * these verbatim (r4-r7 are preserved and returned separately). */
struct GfxCalleeTail {
    u32 r0, r1, r2, r3;
};

/* Everything the caller observes after the call: r0-r3 from the last callee,
 * r4-r7 unchanged (the original push/pop {r4-r7}). */
struct FolderCodeResult {
    u32 r0, r1, r2, r3;
    u32 r4, r5, r6, r7;
};

/* register form: calls ZeroFillByEightWords outside this file, which has no typed C interface yet */
static void clear_code_tile_buffer(u32 dest, u32 size) {
    R0 = dest;
    R1 = size;
    GUEST_CALL(ZeroFillByEightWords);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_8117A14(u32 which_chip) {
    R0 = which_chip;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: sub_8117A84 draws one code letter at the r6 tile cursor and
 * hands back the advanced cursor in r6; it has no typed C interface yet */
static u32 draw_code_letter(u32 code, u32 chip_data, u32 cursor) {
    R0 = code;
    R5 = chip_data;     /* callee sees the chip's database entry in r5 */
    R6 = cursor;
    GUEST_CALL(sub_8117A84);
    return R6;
}

/* register form: calls folder_CountChipsOfCodeOrSkipEntry_8117AA0 outside this file,
 * which has no typed C interface yet; it counts folder entries matching the
 * code (skipping the r4 chip id's own entry) and draws the tally at the r6
 * cursor, handing back the advanced cursor */
static u32 count_folder_chips(u32 chip_id, u32 code, u32 chip_data, u32 cursor) {
    R0 = chip_id;
    R1 = code;
    R4 = chip_id;       /* entry to skip when counting */
    R5 = chip_data;
    R6 = cursor;
    R7 = code;
    GUEST_CALL(folder_CountChipsOfCodeOrSkipEntry_8117AA0);
    return R6;
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has
 * no typed C interface yet; r0-r3 as the callee leaves them are returned */
static struct GfxCalleeTail queue_code_tile_upload(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    struct GfxCalleeTail tail = { R0, R1, R2, R3 };
    return tail;
}

static struct FolderCodeResult build_code_counter_tiles(u32 entry, u32 r5_in, u32 r6_in, u32 r7_in) {
    struct FolderCodeResult out;

    /* Clear the tile buffer the codes and tallies are rendered into. */
    clear_code_tile_buffer(CODE_TILE_BUFFER, CODE_TILE_BUFFER_SIZE);

    u32 chip_id = LD16(entry + 2);
    u32 chip = get_chip_data_sub_8117A14(chip_id);

    /* Bytes 0-3 of the chip's database entry are its four code letters. */
    u32 codes[4];
    codes[0] = LD8(chip);
    codes[1] = LD8(chip + 1);
    codes[2] = LD8(chip + 2);
    codes[3] = LD8(chip + 3);

    /* Draw the four code letters first, cursor advancing through the buffer. */
    u32 cursor = CODE_TILE_BUFFER;
    for (u32 i = 0; i < 4; i++)
        cursor = draw_code_letter(codes[i], chip, cursor);

    /* Then the per-code folder tallies; the original re-reads the id here. */
    chip_id = LD16(entry + 2);
    for (u32 i = 0; i < 4; i++)
        cursor = count_folder_chips(chip_id, codes[i], chip, cursor);

    /* Hand the finished tiles to the DMA queue; guest r4-r7 hold the values
     * the drawing callees left (chip id / chip entry / cursor / last code),
     * exactly as in the original's call. */
    struct GfxCalleeTail tail = queue_code_tile_upload(CODE_TILE_BUFFER, CODE_TILE_VRAM_DEST, CODE_TILE_BUFFER_SIZE);

    out.r0 = tail.r0;
    out.r1 = tail.r1;
    out.r2 = tail.r2;
    out.r3 = tail.r3;
    out.r4 = entry;     /* preserved: the original pops r4-r7 */
    out.r5 = r5_in;
    out.r6 = r6_in;
    out.r7 = r7_in;
    return out;
}

/* folder_DrawSelectedChipCodeCounts_8117A14(r4 -> folder cursor entry, chip id at +2; r5-r7 untouched inputs)
 * -> r0-r3 as left by QueueEightWordAlignedGFXTransfer, nz/c/v likewise,
 * r4-r7 preserved. */
Next folder_DrawSelectedChipCodeCounts_8117A14(void) {
    struct FolderCodeResult out = build_code_counter_tiles(R4, R5, R6, R7);
    R0 = out.r0, R1 = out.r1, R2 = out.r2, R3 = out.r3, R4 = out.r4, R5 = out.r5, R6 = out.r6, R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── sub_8117A84 ── */
/* sub_8117A84(r0 = record index, r6 = destination buffer):
 * Copies the 0x40-byte record with index r0 from the table at unk_2031344
 * into the buffer pointed to by r6 (via CopyByEightWords), then advances r6
 * past the record. If the index is out of range (> 0x1A), only the advance
 * happens. r3 is preserved. */
Next sub_8117A84(void) {
    if ((s32)R0 <= 0x1A) {
        R0 = (u32)R0 * 0x40 + A_unk_2031344; /* src = table + index * 0x40 */
        R1 = R6;                             /* dest = caller's buffer */
        R2 = 0x40;                           /* size = one record */
        GUEST_CALL(CopyByEightWords);
    }
    R6 += 0x40;
    RETURN_TO_CALLER();
}

/* ── folder_CountChipsOfCodeOrSkipEntry_8117AA0 ── */
/* folder_CountChipsOfCodeOrSkipEntry_8117AA0(r0 = chip id, r1 = letter code index, r6 = folder entry pointer):
 * If r1 is a regular letter code (0..0x1A), count how many chips in the folder
 * match that id and code (GetChipCountOfCode) and hand the result to
 * copyTwoDigitNumberTiles_8117B94. Otherwise the entry has no letter code to match: skip past it
 * by advancing the folder pointer one entry (0x40 bytes).
 * Results are left in r2..r7 by the callee chain / the pointer bump. */
Next folder_CountChipsOfCodeOrSkipEntry_8117AA0(void) {
    if ((s32)R1 <= 0x1A) {
        /* r0/r1 already hold the chip id and code for GetChipCountOfCode. */
        GUEST_CALL(GetChipCountOfCode);
        /* copyTwoDigitNumberTiles_8117B94 picks up the count (and any other live registers). */
        GUEST_CALL(copyTwoDigitNumberTiles_8117B94);
    } else {
        R6 += 0x40;
    }
    RETURN_TO_CALLER();
}

/* ── copyTwoDigitNumberTiles_8117B94 ── */
/* copyTwoDigitNumberTiles_8117B94(r0 = entry index, r6 = destination buffer):
 * Copies two 32-byte entries from the table at unk_2031204 into the buffer
 * pointed to by r6. The index is split by dividing r0 by 10 (BIOS Div):
 * the quotient selects the first entry and the remainder the second one.
 * On return r6 points past both copied entries; r7 is left untouched. */
Next copyTwoDigitNumberTiles_8117B94(void) {
    u32 dest = R6;

    /* svc 6 = Div: r0 = r0 / r1 (quotient), r1 = r0 % r1 (remainder). */
    R1 = 10;
    swi_call(6);
    u32 quotient = R0;
    u32 remainder = R1;

    /* First entry, skipped when the whole index fits in one table row. */
    if (quotient != 0) {
        R0 = A_unk_2031204 + (quotient << 5);
        R1 = dest;
        R2 = 0x20;
        GUEST_CALL(CopyByEightWords);
    }

    /* Second entry directly below the first one. */
    dest += 0x20;
    R0 = A_unk_2031204 + (remainder << 5);
    R1 = dest;
    R2 = 0x20;
    GUEST_CALL(CopyByEightWords);

    R6 = dest + 0x20;
    RETURN_TO_CALLER();
}

/* ── sub_8117BC8 ── */
/* register form: dispatches via table at off_8117BDC indexed by a byte from the object at r5+8;
   the targets (sprite_initGuiSlotA_8117BE8, sub_8117C4C, sub_8117CCC) have no typed C interface yet */
static void dispatch_handler(void) {
    u32 table = A_off_8117BDC;
    u32 idx = LD8(R5 + 8);
    R0 = LD32(table + idx);
    GUEST_CALL_ADDR(R0);
}

/* sub_8117BC8(r5 = handler object) -> r2 r3 r5 r10 r11 */
Next sub_8117BC8(void) {
    dispatch_handler();
    RETURN_TO_CALLER();
}

/* ── sprite_initGuiSlotA_8117BE8 ── */
/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void call_initGuiSprite_sub_8117BE8(int a1, int a2) {
    R0 = a1; R1 = a2;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation_sub_8117BE8(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_sprite_loadAnimationData_sub_8117BE8(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_sprite_noShadow_sub_8117BE8(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void call_sprite_setSubEntryPaletteBank_sub_8117BE8(int bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_sprite_setPalette_sub_8117BE8(int pal) {
    R0 = pal;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_sprite_setPriority_sub_8117BE8(int prio) {
    R0 = prio;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void call_slotList_storeHalfword_sub_8117BE8(int value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void call_sprite_setSubRecordFieldA_sub_8117BE8(int value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void call_sprite_setCoordinates_sub_8117BE8(int x, int y) {
    R0 = x; R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

/* register form: calls sub_8117C4C outside this file, which has no typed C interface yet */
/* leftover unused: sub_8117C4C's r0 (sprite_initGuiSlotA_8117BE8's callers read neither r0 nor the flags) */
static u32 call_postInit(void) {
    R0 = 0;
    GUEST_CALL(sub_8117C4C);
    return R0;
}

/* Initialize a GUI sprite record at recordPtr:
 * - write the sprite ID (0x23) and reset its frame counter
 * - assign the sprite to GUI slot 0xa with its shared data table
 * - configure animation, palette, priority, OAM attribute, position
 * - set the state/sub-state bytes, then run the post-init routine
 * Returns r0 from the post-init call (the original's only visible result). */
static u32 initGuiSpriteRecord(u32 recordPtr) {
    ST8(recordPtr, 0x23);                              /* sprite ID */
    call_initGuiSprite_sub_8117BE8(0xa, A_unk_2034B6C);            /* bind to GUI slot + data */
    ST8(recordPtr + 0xC, 0);                           /* reset animation tick */
    call_sprite_setAnimation_sub_8117BE8(0);                       /* start at animation 0 */
    call_sprite_loadAnimationData_sub_8117BE8();                   /* load frame set */
    call_sprite_noShadow_sub_8117BE8();                            /* disable drop shadow */
    call_sprite_setSubEntryPaletteBank_sub_8117BE8(9);             /* palette bank 9 */
    call_sprite_setPalette_sub_8117BE8(0);                         /* palette index 0 */
    call_sprite_setPriority_sub_8117BE8(2);                        /* mid-layer priority */
    call_slotList_storeHalfword_sub_8117BE8(0x170);                /* OAM attr2 (tile+flip) */
    call_sprite_setSubRecordFieldA_sub_8117BE8(0);                 /* clear sub-record field A */
    call_sprite_setCoordinates_sub_8117BE8(0xD4, 0xC);             /* screen position (0xD4,0x0C) */
    ST8(recordPtr + 8, 4);                             /* state byte */
    ST8(recordPtr + 9, 0);                             /* sub-state byte */
    return call_postInit();                            /* post-init → r0, flags */
}

/* sprite_initGuiSlotA_8117BE8(r5 = sprite record pointer) -> r0 */
Next sprite_initGuiSlotA_8117BE8(void) {
    R0 = initGuiSpriteRecord(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8117C4C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_sprite_update_result_sub_8117C4C;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_sub_8117C4C call_sprite_update_sub_8117C4C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_sub_8117C4C){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_8117C4C: run the sprite's current sub-state handler, then the shared
 * sprite_update. The sub-state byte at [r5+9] selects a Thumb handler from
 * the table at 08117C64; the handler is entered with r0 = handler address,
 * r1 = sub-state index and r5 = sprite. It may change r5, r10 and r11, which
 * the following sprite_update call (r5) and this function's caller (r10/r11)
 * still see. */

/* Jump table of Thumb handler addresses (off_8117C60 holds this address). */
#define SUB_STATE_TABLE A_off_8117C64

/* State the sub-state handler leaves behind that later code still needs:
 * the sprite pointer for sprite_update, and the callee-saved r10/r11 that
 * sub_8117C4C passes through to its caller untouched. */
struct handler_exit_state {
    u32 sprite;   /* r5 after the handler */
    u32 r10;
    u32 r11;
};

/* Register form: the handler is reached through a code address (mov lr, pc;
 * bx r0 in the original), which no generated wrapper covers. It runs with
 * exactly the entry state the original builds, and the registers it leaves
 * behind are read back out afterwards for the sprite_update call and the
 * caller of sub_8117C4C. */
static struct handler_exit_state run_sub_state_handler(u32 sprite) {
    u32 index = LD8(sprite + 9);
    u32 handler = LD32(SUB_STATE_TABLE + index);
    R0 = handler;
    R1 = index;
    R5 = sprite;
    GUEST_CALL_ADDR(handler);
    struct handler_exit_state state = { R5, R10, R11 };
    return state;
}

/* Everything sub_8117C4C leaves in its output registers. */
struct sub_state_result {
    u32 r2;
    u32 r3;
    u32 r5;
    u32 r10;
    u32 r11;
};

/* Dispatch the sub-state handler, then run the shared sprite_update on the
 * sprite pointer the handler left in r5; sprite_update supplies the r2/r3/r5
 * the caller sees, while r10/r11 come straight from the handler. */
static struct sub_state_result sprite_state_tick(u32 sprite) {
    struct handler_exit_state after_handler = run_sub_state_handler(sprite);
    call_sprite_update_result_sub_8117C4C updated = call_sprite_update_sub_8117C4C(after_handler.sprite);
    struct sub_state_result result = {
        updated.r2, updated.r3, updated.r5, after_handler.r10, after_handler.r11
    };
    return result;
}

/* sub_8117C4C(r5 = sprite) -> r2 r3 r5 r10 r11 (flags are clobbered) */
Next sub_8117C4C(void) {
    struct sub_state_result result = sprite_state_tick(R5);
    R2 = result.r2;
    R3 = result.r3;
    R5 = result.r5;
    R10 = result.r10;
    R11 = result.r11;
    RETURN_TO_CALLER();
}

/* ── sub_8117CD4 ── */
/* sub_8117CD4: draw-related helper for the menu system. It picks the menu
 * state block to read from (the shared submenu if one is open, otherwise
 * whatever menu object sub_81312F4 hands back), builds three arguments from
 * its fields and passes them to sub_812068C:
 *   r0 = byte    at menu+0x17 (some selection/counter field)
 *   r1 = 0x5140 when that byte equals halfword menu+0x1E, else 0x6140
 *   r2 = 0xC08003 - (s16 at menu+0x18 << 19), a screen/scroll-derived value
 *   r3 = halfword at menu+0x1E
 * r0-r3 and the flags on return are whatever sub_812068C leaves; r7 is
 * preserved (the original saves and restores it around the calls). */
Next sub_8117CD4(void) {
    u32 menu = A_sSubmenu;

    GUEST_CALL(submenu_isOpen_81355D8);
    if (!ZERO_FLAG()) {
        /* No submenu open: use the menu object the game gives us instead. */
        GUEST_CALL(sub_81312F4);
        menu = R0;
    }

    u32 selection = LD8(menu + 0x17);
    u32 entry     = LD16(menu + 0x1E);
    u32 scroll    = (u32)LDS16(menu + 0x18) << 19;

    R0 = selection;
    R1 = (selection == entry) ? 0x5140 : 0x6140;
    R2 = 0xC08003 - scroll;
    R3 = entry;
    GUEST_CALL(sub_812068C);

    RETURN_TO_CALLER();
}

/* ── entity_setLookupEntryStateFromFlag_8117D10 ── */
/* entity_setLookupEntryStateFromFlag_8117D10(r0 = flag, r1 = entity):
 * Look up the entity in the table at 0x2008450; if an entry exists for it,
 * store 4 at entry+9 when flag is nonzero, 0 otherwise. If there is no
 * entry, do nothing. */
Next entity_setLookupEntryStateFromFlag_8117D10(void) {
    u32 flag = R0;

    R0 = R1;
    GUEST_CALL(getStructFrom2008450); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
        u32 entry = R1;
        ST8(entry + 9, flag != 0 ? 4 : 0);
    }
    RETURN_TO_CALLER();
}

/* ── objectStateDispatcher_8117D2C ── */
/* objectStateDispatcher_8117D2C: state-machine dispatcher for the object in r5.
 * The byte at [r5,#8] is the object's current state, stored as a *byte
 * offset* into the jump table at off_8117D40 (0, 4 or 8) - the original's
 * `ldr r0, [r0, r1]` uses an unscaled register offset, so the value is not
 * a word index. The selected handler is then called with
 * r0 = its own table entry (thumb address, so the handler can find the
 * data/entries that follow it in the table) and r1 = the state byte.
 * All other registers and the condition flags pass straight through to the
 * handler, and whatever the handler leaves becomes this function's result. */



Next objectStateDispatcher_8117D2C(void) {
    u32 state = LD8(R5 + 8); /* byte offset into the jump table: 0, 4 or 8 */
    u32 handler = LD32(A_off_8117D40 + state);

    R0 = handler; /* callee receives its own table entry, not the caller's r0 */
    R1 = state;

    switch (state) {
    case 0:
        GUEST_CALL(sub_8117D4C);
        break;
    case 4:
        GUEST_CALL(sub_8117DC8);
        break;
    case 8:
        GUEST_CALL(sub_8117EC0);
        break;
    default:
        /* Unreachable in practice: the state byte is always 0, 4 or 8. The
           original would jump through whatever word sits at table+state. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_8117D4C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6; } call_initGuiSprite_8002770_result;
/* register form: generated wrapper: initGuiSprite_8002770 has no typed C interface yet */
static inline call_initGuiSprite_8002770_result call_initGuiSprite_8002770(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(initGuiSprite_8002770);
    return (call_initGuiSprite_8002770_result){ R4, R5, R6 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_sub_8117D4C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_sub_8117D4C;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_sub_8117D4C call_sprite_loadAnimationData_sub_8117D4C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_sub_8117D4C){ R2, R4, R5 };
}

typedef struct { u32 r1; } call_sprite_noShadow_result;
/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline call_sprite_noShadow_result call_sprite_noShadow_sub_8117D4C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
    return (call_sprite_noShadow_result){ R1 };
}

/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline void call_sprite_setPalette_sub_8117D4C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
}

typedef struct { u32 r2; } call_sprite_setPriority_8002E14_result_sub_8117D4C;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_sub_8117D4C call_sprite_setPriority_8002E14_sub_8117D4C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_sub_8117D4C){ R2 };
}

/* register form: generated wrapper: slotList_storeHalfwordAndClearField_800306C has no typed C interface yet */
static inline void call_slotList_storeHalfwordAndClearField_800306C_sub_8117D4C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

typedef struct { u32 r2; } call_sprite_setSubEntryPaletteBank_800304A_result_sub_8117D4C;
/* register form: generated wrapper: sprite_setSubEntryPaletteBank_800304A has no typed C interface yet */
static inline call_sprite_setSubEntryPaletteBank_800304A_result_sub_8117D4C call_sprite_setSubEntryPaletteBank_800304A_sub_8117D4C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
    return (call_sprite_setSubEntryPaletteBank_800304A_result_sub_8117D4C){ R2 };
}

/* register form: generated wrapper: sprite_setSubRecordFieldA_8003060 has no typed C interface yet */
static inline void call_sprite_setSubRecordFieldA_8003060_sub_8117D4C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

typedef struct { u32 r3; bool v; } call_sprite_setCoordinates_result_sub_8117D4C;
/* register form: generated wrapper: sprite_setCoordinates has no typed C interface yet */
static inline call_sprite_setCoordinates_result_sub_8117D4C call_sprite_setCoordinates_sub_8117D4C(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(sprite_setCoordinates);
    return (call_sprite_setCoordinates_result_sub_8117D4C){ R3, OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r5, r10, r11; } call_sub_8117DC8_result;
/* register form: generated wrapper: sub_8117DC8 has no typed C interface yet */
static inline call_sub_8117DC8_result call_sub_8117DC8(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_8117DC8);
    return (call_sub_8117DC8_result){ R2, R3, R5, R10, R11 };
}

/* off_8117DB4: two u32 palette-VRAM base pointers (0x90, 0xC0) indexed by
 * the player-slot byte at obj+3.  byte_8117DC4: four palette-bank numbers
 * (2, 3, 4, 0) likewise indexed by slot. */

/* Initialize and position one link/select portrait sprite.
 * obj (r5 on entry) is the sprite control block:
 *   +0  graphics ID byte
 *   +3  player slot (0–3)
 *   +4  current animation frame
 *   +8  animation state
 *   +0xC derived sub-record field (frame + 5)
 * Returns the registers produced by the final sub_8117DC8 call. */
static call_sub_8117DC8_result init_link_portrait(u32 obj) {
    ST8(obj, 0x23);

    call_initGuiSprite_8002770(0x61, A_unk_20251B0, obj);

    u32 frameVal = LD8(obj + 4) + 5;
    ST8(obj + 0xC, frameVal);

    call_sprite_setAnimation_sub_8117D4C(frameVal, obj);
    call_sprite_loadAnimationData_sub_8117D4C(obj);
    call_sprite_noShadow_sub_8117D4C(obj);
    call_sprite_setPalette_sub_8117D4C(0, obj);
    call_sprite_setPriority_8002E14_sub_8117D4C(2, obj);

    u8 slot = LD8(obj + 3);

    /* Select palette base by slot from the word table at off_8117DB4. */
    u32 palBase = LD32(A_off_8117DB4 + slot * 4);
    call_slotList_storeHalfwordAndClearField_800306C_sub_8117D4C(palBase, obj);

    /* Palette bank for this slot from the byte table at byte_8117DC4. */
    u32 palBank = LD8(A_byte_8117DC4 + slot);
    call_sprite_setSubEntryPaletteBank_800304A_sub_8117D4C(palBank, obj);

    call_sprite_setSubRecordFieldA_8003060_sub_8117D4C(3, obj);

    /* X = 0x78 fixed; Y = 0x28 + slot * 0x20 (four rows, 32 px apart). */
    call_sprite_setCoordinates_sub_8117D4C(0x78, 0x28 + (u32)slot * 0x20, obj);

    ST8(obj + 8, 4);

    return call_sub_8117DC8(obj);
}

/* sub_8117D4C(r5 = sprite control block) -> r2 r3 r5 r10 r11 */
Next sub_8117D4C(void) {
    call_sub_8117DC8_result res = init_link_portrait(R5);
    R2 = res.r2;
    R3 = res.r3;
    R5 = res.r5;
    R10 = res.r10;
    R11 = res.r11;
    RETURN_TO_CALLER();
}

/* ── battle_refreshObjectSprite_8117E20 ── */
/* battle_refreshObjectSprite_8117E20: per-frame sprite refresh for a battle object.
 * r5 = object struct, r7+0xD = the object's current state byte.
 * The animation to show is the one stored at r5+4, or that plus 5 when the
 * state byte differs from the copy cached at r5+3 (i.e. the object changed
 * state since the animation was last picked). If the resulting animation
 * differs from the one actually loaded (cached at r5+0xC), load it and drop
 * the shadow; finally advance the sprite one frame. */
Next battle_refreshObjectSprite_8117E20(void) {
    u32 state       = LD8(R7 + 0xD);
    u32 baseAnim    = LD8(R5 + 4);
    u32 wantedAnim  = (state == LD8(R5 + 3)) ? baseAnim : baseAnim + 5;
    u32 currentAnim = LD8(R5 + 0xC);

    if (wantedAnim != currentAnim) {
        ST8(R5 + 0xC, wantedAnim);
        R0 = wantedAnim;
        R1 = currentAnim;
        GUEST_CALL(sprite_setAnimation);
        GUEST_CALL(sprite_loadAnimationData);
        GUEST_CALL(sprite_noShadow);
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_8117E60 ── */
/* sub_8117E60: update a battle sprite's animation/animation data.
 * Inputs:  r5 = battle sprite struct ptr, r7 = object/base state ptr
 * Outputs: r2 r3 r5 (r2/r3 from the tail call; r5 untouched)
 * Clobbers: r0 r1
 */
Next sub_8117E60(void) {
    u8 sprite_dir  = LD8(R5 + 3);                 /* [r5+3] = facing direction */
    u8 sprite_base = LD8(R5 + 4);                 /* [r5+4] = base anim index  */
    u8 obj_dir_raw = LD8(R7 + 0xD);               /* [r7+0xD] = object facing  */
    u8 target_anim;

    if ((obj_dir_raw >> 1) == sprite_dir) {
        /* Same lane as the sprite: maybe bump from base anim. */
        target_anim = sprite_base;
        if (obj_dir_raw != 2 && obj_dir_raw != 0)
            target_anim = sprite_base + 1;
    } else {
        /* Opposite lane: use ranged anim, +5 base, +1 if 2-lanes-or-same-row. */
        target_anim = sprite_base + 5;
        if (obj_dir_raw >= 2 && LD8(R7 + 0x21) != 0)
            target_anim = sprite_base + 6;
    }

    if (target_anim != LD8(R5 + 0xC)) {
        ST8(R5 + 0xC, target_anim);
        R0 = target_anim;
        GUEST_CALL(sprite_setAnimation);
        GUEST_CALL(sprite_loadAnimationData);
        GUEST_CALL(sprite_noShadow);
    }

    GUEST_CALL(sprite_update);   /* R2/R3 outputs come from this leaf */
    RETURN_TO_CALLER();
}

/* ── sub_8118F24 ── */
/* sub_8118F24(r5 = battle object pointer): sprite-update trampoline.
 * Dispatches to one of three Thumb handlers via a ROM jump table indexed
 * by the state byte at [r5+8], preserving LR across the call. All output
 * registers (r2/r3/r5/r10/r11) come from the dispatched handler. */
Next sub_8118F24(void) {
    u32 obj = R5;
    u32 lr_save = LR;

    u32 state = LD8(obj + 8);
    u32 target = LD32(A_off_8118F38 + state);

    /* register form: indirect call through a ROM jump table; the target is
     * a Thumb address (low bit set) and LR must be preserved across it so
     * the callee returns to us. */
    R1 = state;
    R0 = target;
    GUEST_CALL_ADDR(target);

    LR = lr_save;
    RETURN_TO_CALLER();
}

/* ── sprite_initBattle_8118F44 ── */
/* Coordinate table for battle sprites (x,y pairs, indexed by sprite field +5 * 4) */
#define SPRITE_TYPE_INIT        0x23
#define COORD_TABLE_ADDR        A_byte_8118FA4

/* register form: calls loc_8002774 outside this file, which has no typed C interface yet */
static void call_configureSpriteFlag(u32 a1, u32 a2) {
    R0 = a1;
    R1 = a2;
    GUEST_CALL(loc_8002774);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_setAnimation_sub_8118F44(u32 animId) {
    R0 = animId;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_loadAnimationData_sub_8118F44(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_noShadow_sub_8118F44(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_setPalette_sub_8118F44(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_setPriority_sub_8118F44(u32 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void call_storeHalfwordAndClear(u32 value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sub_81190E0 outside this file, which has no typed C interface yet */
static void call_initSubSlot(void) {
    GUEST_CALL(sub_81190E0);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void call_setSubRecordFieldA_sub_8118F44(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void call_setCoordinates(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

/* register form: calls sprite_dispatchUpdate_8118FC0 outside this file, which has no typed C interface yet */
static void call_dispatchUpdate(u32 tablePtr) {
    R2 = tablePtr;
    GUEST_CALL(sprite_dispatchUpdate_8118FC0);
}

/* Initialize a battle sprite object: set type, animation, visual properties,
 * coordinates, and dispatch its first update.  sprite is the GBA address
 * of the sprite record (caller's R5). */
static void init_battle_sprite(u32 sprite) {
    /* Mark the sprite as type 0x23 (battle entity). */
    ST8(sprite, SPRITE_TYPE_INIT);

    /* Enable per-sprite configuration slot. */
    call_configureSpriteFlag(8, A_unk_201E284);

    /* Copy animation ID (field +4) into the sub-animation slot (field +0xC). */
    u32 animId = LD8(sprite + 4);
    ST8(sprite + 0xC, animId);

    /* Load and apply the animation, disable shadow. */
    call_setAnimation_sub_8118F44(animId);
    call_loadAnimationData_sub_8118F44();
    call_noShadow_sub_8118F44();

    /* Palette 0, priority 2. */
    call_setPalette_sub_8118F44(0);
    call_setPriority_sub_8118F44(2);

    /* Clear sub-slot halfword and reset state. */
    call_storeHalfwordAndClear(0);
    call_initSubSlot();
    call_setSubRecordFieldA_sub_8118F44(4);

    /* Look up (x, y) from the coordinate table using sprite field +5. */
    u32 offset = (LD8(sprite + 5)) << 2;
    u32 x = LD16(COORD_TABLE_ADDR + offset);
    u32 y = LD16(COORD_TABLE_ADDR + offset + 2);
    call_setCoordinates(x, y);

    /* Set active state (field +8 = 4) and trigger the first update tick. */
    ST8(sprite + 8, 4);
    call_dispatchUpdate(COORD_TABLE_ADDR);
}

/* sprite_initBattle_8118F44(r5 = sprite record pointer) -> void (registers preserved per callee chain) */
Next sprite_initBattle_8118F44(void) {
    init_battle_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_dispatchUpdate_8118FC0 ── */
/* sprite_dispatchUpdate_8118FC0: run the per-object update handler selected by byte 6 of the
 * object whose pointer is in R5, then advance the object's sprite animation.
 *
 * The handler table off_8118FD8 is indexed by that byte:
 *   0 -> nullsub_8118FE0  (a plain "bx lr" stub: no per-object update)
 *   1 -> sub_8118FE4
 * All registers are passed through to the handler untouched, and whatever
 * R0/R1 it leaves behind are forwarded to sprite_update (the original
 * dispatches with "mov lr, pc / bx r0" and then falls into "bl sprite_update"). */
Next sprite_dispatchUpdate_8118FC0(void) {
    u32 handlerIndex = LD8(R5 + 6);

    /* Index 0 selects the nullsub stub; since that stub only does "bx lr",
     * branching to it has no observable effect, so only the real handler
     * needs to run here. */
    if (handlerIndex != 0)
        GUEST_CALL(sub_8118FE4);

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── nullsub_8118FE0 ── */
/* nullsub_8118FE0: empty stub — pushes and immediately pops LR, doing nothing.
 * Present in the original ROM only to occupy a slot in a function table. */
Next nullsub_8118FE0(void) {
    RETURN_TO_CALLER();
}

/* ── battle_dispatchEventScript_8119018 ── */
/* register form: calls sub_8137884 outside this file, which has no typed C interface yet */
static u32 lookup_script_entry(u32 index, u32 obj_ptr, u32 script_data) {
    R0 = index;
    R1 = index;
    R2 = obj_ptr;
    R3 = script_data;
    GUEST_CALL(sub_8137884);
    return R0;
}

/* register form: calls sub_804A24C outside this file, which has no typed C interface yet */
static u32 advance_event_script(u32 value) {
    R0 = value;
    GUEST_CALL(sub_804A24C);
    return R0;
}

/* register form: calls sub_3006B94 outside this file, which has no typed C interface yet */
static void run_text_script(u32 r0val, u32 obj_ptr, u32 script_data) {
    R0 = r0val;
    R1 = A_sub_3006B94 | 1;
    R2 = obj_ptr;
    R3 = script_data;
    R4 = 8;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 0;
    GUEST_CALL(sub_3006B94);
}

/* Dispatch a battle event script for the given object.
 * Writes active-state 4 to the object, resolves the script type from its
 * action byte (offset 3) via two parallel lookup tables, validates the entry,
 * then tail-calls the text-script interpreter. */
static void do_battle_event_script(u32 object) {
    ST8(object + 9, 4);

    u32 index = LD8(object + 3) - 7;
    u32 offset = index << 2;
    u32 obj_table_val = LD32(A_off_811905C + offset);
    u32 script_table_val = LD32(A_byte_811906C + offset);

    u32 validated = lookup_script_entry(index, obj_table_val, script_table_val);
    validated >>= 4;
    validated = advance_event_script(validated);

    run_text_script(validated, obj_table_val, script_table_val);
}

/* battle_dispatchEventScript_8119018(r5 = battle object ptr) -> r0-r3, flags via tail call to sub_3006B94 */
Next battle_dispatchEventScript_8119018(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7, saved_lr = LR;
    do_battle_event_script(saved_r5);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7; LR = saved_lr;
    RETURN_TO_CALLER();
}

/* ── sub_81190D8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_sprite_clearScaleCounterAndMakeUnscalable_804668A_result;
/* register form: generated wrapper: sprite_clearScaleCounterAndMakeUnscalable_804668A has no typed C interface yet */
static inline call_sprite_clearScaleCounterAndMakeUnscalable_804668A_result call_sprite_clearScaleCounterAndMakeUnscalable_804668A(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_clearScaleCounterAndMakeUnscalable_804668A);
    return (call_sprite_clearScaleCounterAndMakeUnscalable_804668A_result){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_81190D8: clear a sprite's scale counter and mark it unscalable.
 *
 * The original is a bare tail-call stub: it forwards the sprite object
 * pointer in r5 to sprite_clearScaleCounterAndMakeUnscalable_804668A and
 * returns immediately. Everything the caller sees afterwards (r2, r3, the
 * flags, and the untouched r4/r5/r6/r7) is simply whatever that callee
 * leaves behind, so the wrapper's execution of the callee already puts the
 * guest registers in their final state and the adapter has nothing to copy. */

/* Forward the object pointer to the scale-reset routine. The generated
 * wrapper runs the callee in place, leaving its register results (r2, r3,
 * ...) and flags exactly as the original tail call does. */
static void reset_sprite_scale(u32 sprite) {
    call_sprite_clearScaleCounterAndMakeUnscalable_804668A(sprite);
}

/* sub_81190D8(r5 = sprite object pointer) */
Next sub_81190D8(void) {
    reset_sprite_scale(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_testFrameParamsMaskC0_81190F8 ── */
/* sprite_testFrameParamsMaskC0_81190F8(r0 = sprite handle):
 * Looks up the sprite struct registered for the handle in r0 (via the list at
 * 0x2008450) and reports whether the sprite's current frame carries the status
 * bits 0xC0 in its frame parameters.
 * Returns r0 = 1 if the bits are set, 0 if not (or if no struct was found),
 * with N/Z reflecting r0 and C/V cleared (the original's "mov r0, r4" is an
 * adds r0, r4, #0, which zeroes the carry/overflow flags, and the trailing
 * tst r0, r0 leaves them cleared). r1 ends as 0xC0 on the found path;
 * r4/r5 are preserved. */
Next sprite_testFrameParamsMaskC0_81190F8(void) {
    u32 savedR4 = R4, savedR5 = R5;
    u32 flagged = 0;

    /* The callees see r4 = 0 here, matching the original's working register. */
    R4 = 0;

    /* Look up the entry for the handle in r0; Z set means it is unused. */
    GUEST_CALL(getStructFrom2008450);        /* r1 = the entry; Z set when its first byte is 0 */
    if (!ZERO_FLAG()) {
        /* The frame-parameter query takes the accompanying value in r5. */
        R5 = R1;
        GUEST_CALL(sprite_getFrameParameters);   /* -> r0 = frame parameter bits */

        /* Test the frame parameter bits against the 0xC0 mask (also left in r1). */
        R1 = 0xC0;
        if (R0 & R1)
            flagged = 1;
    }

    /* Restore the caller's r4/r5 (the original pops them back off the stack). */
    R4 = savedR4;
    R5 = savedR5;

    R0 = flagged;
    /* mov r0, r4 == adds r0, r4, #0: no carry-out and no signed overflow are
     * possible, so this clears C and V (N/Z from r4 as well). The original's
     * tst r0, r0 then re-derives the same N/Z while leaving C/V cleared. */
    SET_FLAGS_ADD(R0, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8119118 ── */
/* sub_8119118(r0):
 * If the global struct lookup succeeds, set its field at offset 8 to 8. */
Next sub_8119118(void) {
    /* Call lookup helper; on success r1 points at the struct. */
    GUEST_CALL(getStructFrom2008450);
    if (!ZERO_FLAG())
        ST8(R1 + 8, 8);
    RETURN_TO_CALLER();
}

/* ── npc_StateModeDispatcher_8119364 ── */
/* npc_StateModeDispatcher_8119364: dispatcher for the state machine object in r5. Reads the mode
 * byte at [r5+8] and calls one of three handlers via the table at
 * off_8119378 (battle_spriteInit_8119384 / sub_81196A0 / sub_8119804).
 *
 * Note the table is indexed with the raw mode byte (`ldr r0, [r0, r1]`), so
 * the byte itself holds the byte offset: 0, 4 or 8, one entry per 4-byte
 * word. The handler is entered with r0 = its own table entry (thumb address)
 * and r1 = the mode byte; r2..r12 and the flags pass through untouched.
 *
 * The original uses the `mov lr, pc; bx r0` trick with LR pointing at the
 * trailing `pop {pc}`, so when the handler returns, that pop returns straight
 * to this function's caller. Net effect: a plain call, with the handler's
 * registers and flags becoming this function's outputs. */
Next npc_StateModeDispatcher_8119364(void) {
    u32 mode = LD8(R5 + 8);

    R1 = mode;
    R0 = LD32(A_off_8119378 + mode); /* handler address, as the callee sees it */

    switch (mode) {
    case 0:
        GUEST_CALL(battle_spriteInit_8119384);
        break;
    case 4:
        GUEST_CALL(sub_81196A0);
        break;
    case 8:
        GUEST_CALL(sub_8119804);
        break;
    default:
        /* Not reachable in practice: the original would jump through a
         * garbage pointer here. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_spriteInit_8119384 ── */
/*
 * battle_spriteInit_8119384: Initialize a battle sprite object from its character/team indices.
 *
 * Reads char_idx [obj+4] and team_idx [obj+5].  Clears bit 0x10 ("forced")
 * from char_idx if present and marks [obj+0xA].  Writes a sprite-type byte
 * (0x23 for Net.EXE, 0x83 otherwise) to [obj].  If char_idx==0, invokes a
 * special handler.  Then populates the GUI sprite with base graphic, pointer,
 * animation, palette, priority, slot halfword, sub-entry palette bank, and
 * sub-record field A looked up from per-team/per-char tables.  Finally sets
 * object state fields and calls the sprite-finalize tail.
 */

/* register form: calls sub_81195E4 outside this file, which has no typed C interface yet */
static void zero_char_handler(u32 sprite_type) {
    R0 = sprite_type;
    GUEST_CALL(sub_81195E4);
}

/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void init_gui_sprite(u8 base_graphic, u32 sprite_ptr, u8 char_idx) {
    R0 = base_graphic;
    R1 = sprite_ptr;
    R4 = char_idx;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void set_animation(u8 anim_id) {
    R0 = anim_id;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void load_animation_data(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void disable_shadow(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void set_palette(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void set_priority(u8 value) {
    R0 = value;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void store_slot_halfword(u32 value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void set_sub_palette_bank(u8 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void set_sub_record_field_a(u8 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls object_writeSubstructField30_8002FEC outside this file, which has no typed C interface yet */
static void write_substruct_field30(u32 ptr) {
    R0 = ptr;
    GUEST_CALL(object_writeSubstructField30_8002FEC);
}

/* register form: calls sub_81196A0 outside this file, which has no typed C interface yet */
static void finalize_sprite(void) {
    R0 = 0;
    GUEST_CALL(sub_81196A0);
}

/* --- typed implementation --- */

static void init_battle_sprite_sub_8119384(u32 obj) {
    /* Read character index; clear the "forced" bit (0x10) if set */
    u8 char_idx = LD8(obj + 4);
    if (char_idx & 0x10) {
        char_idx &= (u8)~0x10;
        ST8(obj + 4, char_idx);
        ST8(obj + 0xA, 1);
    }

    /* Sprite-type byte: 0x23 for Net.EXE (char 1), 0x83 otherwise */
    u8 sprite_type = (char_idx == 1) ? 0x23 : 0x83;
    ST8(obj, sprite_type);

    /* Character 0 triggers a special initial handler */
    if (char_idx == 0)
        zero_char_handler(sprite_type);

    /* Team index used for all per-team table lookups */
    u8 team_idx = LD8(obj + 5);

    /* Look up sprite pointer: off_81191B4[team] -> row, row[char] -> ptr */
    u32 team_row   = LD32(A_off_81191B4 + (team_idx << 2));
    u32 sprite_ptr = LD32(team_row + (char_idx << 2));

    /* Initialize GUI sprite with base graphic and pointer */
    u8 base_graphic = LD8(A_dword_8119624 + char_idx);
    init_gui_sprite(base_graphic, sprite_ptr, char_idx);

    /* Set animation, load data, disable shadow, zero palette */
    u8 anim_id = LD8(A_dword_811963C + char_idx);
    set_animation(anim_id);
    load_animation_data();
    disable_shadow();
    set_palette(0);

    /* Display priority from team/char table */
    u32 prio_table = LD32(A_off_8119208 + (team_idx << 2));
    u8 priority    = LD8(prio_table + char_idx);
    set_priority(priority);

    /* Slot-list halfword from team/char table (stride 4) */
    u32 slot_table = LD32(A_off_8119128 + (team_idx << 2));
    u32 slot_value = LD32(slot_table + (char_idx << 2));
    store_slot_halfword(slot_value);

    /* Sub-entry palette bank from team/char table */
    u32 pal_table = LD32(A_off_8119170 + (team_idx << 2));
    u8 pal_bank   = LD8(pal_table + char_idx);
    set_sub_palette_bank(pal_bank);

    /* Sub-record field A from team/char table */
    u32 fld_table = LD32(A_off_8119228 + (team_idx << 2));
    u8 field_a    = LD8(fld_table + char_idx);
    set_sub_record_field_a(field_a);

    /* Character 0 also writes to the battle-object substruct */
    if (char_idx == 0) {
        u32 sub_ptr = LD32(A_off_811924C + (team_idx << 2));
        write_substruct_field30(sub_ptr);
    }

    /* Set final object state fields.
     * The original does mov r0,#0xFF then strh r0,[r5,#0x10]: THUMB strh
     * stores only the low 16 bits (0x00FF), not 0xFFFF. */
    ST8(obj + 0x08, 4);
    ST16(obj + 0x10, 0x00FF);
    ST8(obj + 0x0D, 0xFF);
    ST16(obj + 0x1A, 0x00FF);
    ST8(obj + 0x13, 0);

    /* Tail: finalize */
    finalize_sprite();
}

/* battle_spriteInit_8119384(r5 = battle sprite object pointer) -> r0-r5, lr, nz/c/v via callee */
Next battle_spriteInit_8119384(void) {
    init_battle_sprite_sub_8119384(R5);
    RETURN_TO_CALLER();
}
