/*
 * asm31_80E0F02.c: readable functions of bn6f asm31.s, 0x080E0F02-0x080EBE00, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── spawnChargeShotChargeObject_80E0F02 ── */
/* spawnChargeShotChargeObject_80E0F02(r5 = owner object, r7 = result slot):
 * Spawns the charge-up effect object that appears while a charge shot is
 * building (object_spawnType4, 8-byte object). The spawner hands the new
 * object back in r5. If it spawned, the effect is linked to its owner
 * (RelatedObject1Ptr), stored into *r7, and remembers r7 in ExtraVars so it
 * can clear that slot when it dies; its header flags also get bit 4 set.
 * The caller's r5 is preserved (the original push/pop {r5} around the call
 * exists because the spawner clobbers it). NZ reflect the new object pointer,
 * from the "mov r0, r5" null check that gates the linking; v and c are left
 * untouched, as in the original (movs r0, r5 = lsls #0 does not set them). */


Next spawnChargeShotChargeObject_80E0F02(void) {
    u32 owner = R5;       /* object the charge effect belongs to */
    u32 resultSlot = R7;  /* where the caller keeps track of the effect */

    R0 = 8;
    GUEST_CALL(object_spawnType4);

    u32 newObject = R5;      /* spawner returns the new object in r5 */
    SET_FLAGS_NZ(newObject); /* mov r0, r5: null check for the branch below */
    R5 = owner;              /* restore caller's r5 (original pop {r5}) */

    if (newObject != 0) {
        ST32(newObject + oBattleObject_RelatedObject1Ptr, owner);
        ST32(resultSlot, newObject);
        ST32(newObject + oBattleObject_ExtraVars, resultSlot);
        ST8(newObject + oObjectHeader_Flags,
            LD8(newObject + oObjectHeader_Flags) | 4);
    }
    RETURN_TO_CALLER();
}

/* ── battle_setExtraAnimState_80E0F2E ── */
/* battle_setExtraAnimState_80E0F2E(r0 = selector, r5 = battle object):
 * Reset an object's extra-var timer to 0x15 (r0 == 1) or 8 (otherwise).
 * If the timer actually changed, reload the object's sprite animation
 * (sprite 0x80, animation 0x14), drop any queued animation, and clear
 * the "shadowless" flag (bit 3) in the object header.
 * Leaves r3 = value sprite_noShadow leaves behind, or the old timer
 * value when the early-exit path is taken; r4/r5 preserved. */
Next battle_setExtraAnimState_80E0F2E(void) {
    u32 animId = (R0 == 1) ? 0x15 : 8;
    u32 oldTimer = LD32(R5 + oBattleObject_ExtraVars + 0xC);

    ST32(R5 + oBattleObject_ExtraVars + 0xC, animId);

    if (oldTimer != animId) {
        R0 = 0x80;
        R1 = 0x14;
        R2 = animId;
        GUEST_CALL(sprite_load);
        GUEST_CALL(sprite_loadAnimationData);
        GUEST_CALL(sprite_noShadow);

        ST8(R5 + oBattleObject_CurAnimCopy, 0xFF);

        /* oObjectHeader_Flags is at offset 0 of the object header */
        ST8(R5, LD8(R5) & ~8u);
    } else {
        /* early exit: original leaves the old timer value in r3 */
        R3 = oldTimer;
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectSetTimerFromParam1Table_80E19DC ── */
/* battle_objectSetTimerFromParam1Table_80E19DC(r5 = battle object):
 * Load the object's next Timer value from a lookup table indexed by its
 * Param1 byte, store it into the Timer field, then advance the object's
 * state to CUR_STATE_UPDATE (4) so the state machine continues next frame.
 * Outputs: r0 = CUR_STATE_UPDATE (nz set by the mov), r1 = timer value,
 * r7 = timer table address. */
#define oBattleObject_Param1 4
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized 8
#define CUR_STATE_UPDATE 4

Next battle_objectSetTimerFromParam1Table_80E19DC(void) {
    /* Lookup table of timer values indexed by Param1 (literal pool pointer). */
    R7 = A_dword_80E19F0;

    u32 timer = LD8(R7 + LD8(R5 + oBattleObject_Param1));
    ST16(R5 + oBattleObject_Timer, timer);
    R1 = timer;

    R0 = CUR_STATE_UPDATE;
    SET_FLAGS_NZ(R0); /* flags left by the original's mov r0, #CUR_STATE_UPDATE */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, R0);

    RETURN_TO_CALLER();
}

#undef oBattleObject_Param1
#undef oBattleObject_CurStateActionPhaseAndPhaseInitialized
#undef CUR_STATE_UPDATE

/* ── battle_object_freeWithPhaseReset_80E1A5E ── */
/* battle_object_freeWithPhaseReset_80E1A5E(r5 = battle object):
 * Mark the object's phase as uninitialized and free its memory.
 * Results and flags are those left by object_freeMemory. */
Next battle_object_freeWithPhaseReset_80E1A5E(void) {
    ST8(R5 + oBattleObject_PhaseInitialized, 1);

    R0 = 1;
    GUEST_CALL(object_freeMemory);

    RETURN_TO_CALLER();
}

/* ── battle_spawnLinkedObject_80E1A6A ── */
/* battle_spawnLinkedObject_80E1A6A(r5 = battle object, r7 = extra vars holder):
 * Spawns a type-0x11 battle object, then, if the object pointer in r5 is
 * non-NULL, wires the object and the holder together:
 *   object->relatedObject1 = object, *holder = object, object->extraVars = holder.
 * Leaves C/V cleared and Z from the object pointer (the flag-setting
 * "mov r0, r5" is really adds r0, r5, #0). */
Next battle_spawnLinkedObject_80E1A6A(void) {
    u32 savedObject = R5;

    R0 = 0x11;
    GUEST_CALL(object_spawnType4);

    /* r0 = r5; the spawn normally leaves r5 untouched, but the original
     * saves/restores it around the call, so keep the caller's value. */
    R0 = R5;
    SET_FLAGS_ADD(R5, 0);
    R5 = savedObject;

    if (R0 != 0) {
        ST32(R0 + oBattleObject_RelatedObject1Ptr, R5);
        if (R7 != 0)
            ST32(R7, R0);
        ST32(R0 + oBattleObject_ExtraVars, R7);
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectFlyAnimUpdateAndDestroy_80E1D54 ── */
/* battle_objectFlyAnimUpdateAndDestroy_80E1D54(r5 = battle object):
 * Advance the object's sprite animation, integrate its Z velocity into its Z
 * position, and switch the object to the "destroy" state action once the
 * sprite's current frame parameters report the end marker (bit 0x80).
 * Leaves r0 = frame parameters (or CUR_STATE_DESTROY_sub_80E1D54 when destroyed), r1 = 0x80,
 * and flags as the last flag-setting instruction of the taken path leaves them. */

enum { CUR_STATE_DESTROY_sub_80E1D54 = 8u };

Next battle_objectFlyAnimUpdateAndDestroy_80E1D54(void) {
    u32 obj = R5;

    GUEST_CALL(sprite_update);

    /* Z += ZVelocity */
    u32 z      = LD32(obj + oBattleObject_Z);
    u32 zSpeed = LD32(obj + oBattleObject_ZVelocity);
    ST32(obj + oBattleObject_Z, z + zSpeed);

    GUEST_CALL(sprite_getFrameParameters);

    R1 = 0x80;
    u32 frameParams = R0;
    SET_FLAGS_NZ(frameParams & R1);
    if (frameParams & 0x80) {
        /* Animation finished: mark the object for destruction. */
        R0 = CUR_STATE_DESTROY_sub_80E1D54;
        SET_FLAGS_NZ(R0);
        ST32(obj + oBattleObject_CurPhaseAndPhaseInitialized, R0);
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectFreeWrapper_80E1D72 ── */
/* battle_objectFreeWrapper_80E1D72(r0 = object): destructor for a battle object — hand its
 * memory slot back to the object pool via object_freeMemory.
 * All arguments (r0) and carried state (r5) are passed through untouched;
 * r0-r3 and the flags come back from object_freeMemory. */
Next battle_objectFreeWrapper_80E1D72(void) {
    GUEST_CALL(object_freeMemory);
    RETURN_TO_CALLER();
}

/* ── battle_objectUpdatePosAndRunAction_80E3BEA ── */
/* battle_objectUpdatePosAndRunAction_80E3BEA (r5 = battle object):
 * Update this object's position via sub_80E3D1A — first from its current
 * (X, Y), then again from the position advanced by its X velocity — and
 * then hand control to the handler for its current action, selected from
 * the two-entry jump table off_80E3C0C. The handler is entered with
 * r0 = CurAction (offset 9 in the battle object), r1 = its table entry
 * (thumb address), r2 = whatever sub_80E3D1A left there; everything else
 * passes through untouched. When the handler returns, return to
 * battle_objectUpdatePosAndRunAction_80E3BEA's caller. */
Next battle_objectUpdatePosAndRunAction_80E3BEA(void) {
    R0 = LD32(R5 + oBattleObject_X);
    R1 = LD32(R5 + oBattleObject_Y);
    GUEST_CALL(sub_80E3D1A);

    R0 += LD32(R5 + oBattleObject_XVelocity);
    GUEST_CALL(sub_80E3D1A);

    u32 action = LD8(R5 + 9);           /* oBattleObject_CurAction */
    R0 = action;
    R1 = LD32(A_off_80E3C0C + action);  /* handler address, index 0 or 1 */

    if (action == 0)
        GUEST_CALL(sub_80E3C14);
    else
        GUEST_CALL(sub_80E3CC4);

    RETURN_TO_CALLER();
}

/* ── nullsub_12 ── */
/* nullsub_12: empty placeholder function; does nothing. */
Next nullsub_12(void) {
    RETURN_TO_CALLER();
}

/* ── playerObject_main_80EA460 ── */
/* Dispatch the player battle-object state machine.
 * Reads CurState byte, calls the matching handler, then the shared
 * post-state hook (sub_801BCF4).
 *
 * Jump table (off_80EA478, word-indexed by state byte):
 *   state 0 → playerObject_init_80172F0
 *   state 4 → playerObject_update_80EA484
 *   state 8 → sub_8016C4E
 */

/* register form: calls playerObject_init_80172F0 outside this file, which has no typed C interface yet */
static void call_player_init(u32 state) {
    R0 = state;
    GUEST_CALL(playerObject_init_80172F0);
}

/* register form: calls playerObject_update_80EA484 outside this file, which has no typed C interface yet */
static void call_player_update(u32 state) {
    R0 = state;
    GUEST_CALL(playerObject_update_80EA484);
}

/* register form: calls sub_8016C4E outside this file, which has no typed C interface yet */
static void call_sub_8016C4E(u32 state) {
    R0 = state;
    GUEST_CALL(sub_8016C4E);
}

/* register form: calls sub_801BCF4 outside this file, which has no typed C interface yet */
static void call_post_state_update(void) {
    GUEST_CALL(sub_801BCF4);
}

static void player_state_dispatch(u32 battle_obj) {
    u32 state = LD8(battle_obj + oBattleObject_CurState);

    switch (state) {
        case 0: call_player_init(state);   break;
        case 4: call_player_update(state); break;
        case 8: call_sub_8016C4E(state);   break;
    }

    call_post_state_update();
}

/* playerObject_main_80EA460(r5 = player battle-object pointer) */
Next playerObject_main_80EA460(void) {
    player_state_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── nullsub_105 ── */
/* nullsub_105: an empty stub (part of the AI attack jumptable region; the real
 * handlers are the routines whose addresses follow it in the table).
 * It does nothing and returns immediately. */

/* No-op stub — nothing to do in the typed function. */
static void nullsub_105_body(void) {
}

Next nullsub_105(void) {
    nullsub_105_body();
    RETURN_TO_CALLER();
}

/* ── aiAttack_stepStateMachine_80EB04C ── */
/* aiAttack_stepStateMachine_80EB04C: run one step of the AI attack-state machine for the object in R7.
 *
 * The state byte oAIAttackVars_Unk_00 is a byte offset into the handler table
 * at off_80EB074 (entries are 4 bytes apart, so it holds 0, 4, 8, 0xC or 0x10).
 * The selected handler is entered with r0 = state byte and r1 = its own table
 * entry (the original's `mov lr, pc / bx r1` dispatch), and may clobber the
 * flags and LR, which pass straight through to this function's caller.
 *
 * After the handler returns: if it latched "busy" into oAIAttackVars_Unk_30,
 * we're done. Otherwise we poll the D-pad; if the currently held direction
 * differs from the one remembered in oAIAttackVars_Unk_0c, store 1 into
 * oAIAttackVars_Unk_30.
 */
Next aiAttack_stepStateMachine_80EB04C(void) {
    u32 state = LD8(R7); /* oAIAttackVars_Unk_00 is at offset 0 */

    R0 = state;                          /* handler input: current state */
    R1 = LD32(A_off_80EB074 + state);    /* handler address from the table */
    switch (state) {
    case 0x00: GUEST_CALL(sub_80EB088); break;
    case 0x04: GUEST_CALL(sub_80EB128); break;
    case 0x08: GUEST_CALL(aiAttack_TimerTickEnd_80EB194); break;
    case 0x0C: GUEST_CALL(sub_80EB1C4); break;
    case 0x10: GUEST_CALL(sub_80EB1F8); break;
    default: break; /* unreachable: the state byte is always a table offset */
    }

    /* tst busy, busy: skip the direction poll if the handler flagged busy */
    u32 busy = LD32(R7 + oAIAttackVars_Unk_30);
    R0 = busy;
    SET_FLAGS_NZ(busy);
    if (busy != 0)
        RETURN_TO_CALLER();

    /* busy is 0 here, so r0 = 0 going into the button poll */
    GUEST_CALL(playerObject_checkDirectionButtons_800FA54);

    /* r1 ends up holding the remembered direction either way */
    R1 = LD8(R7 + oAIAttackVars_Unk_0c);
    SET_FLAGS_CMP(R0, R1);
    if (R0 != R1) {
        R0 = 1;                              /* mov r0, #1 also sets nz */
        ST32(R7 + oAIAttackVars_Unk_30, R0);
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── aiAttack_TimerTickEnd_80EB194 ── */
/* Offsets/bit positions referenced by the assembly that aren't in game.h. */
#define OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT 19   /* lsl r0, r0, #19 */
#define oAIAttackVars_Unk_00 0                   /* strh r0, [r7,#0] */

/* aiAttack_TimerTickEnd_80EB194: per-frame tick of an AI attack action. R7 points at the object's
 * AI attack variables.
 *
 * Decrements the action timer (Unk_10) and leaves that value in R0. While it
 * is still counting down there is nothing else to do. Once it reaches zero the
 * object's "busy" flag (0x40) is cleared and the CURRENTLY_MOVING flag is set,
 * the timer is reloaded from Unk_18 (the reloaded value stays in R0 when the
 * AI-layer check sub_80F02A2 is called, as in the original), the animation is
 * reset if the check returns non-zero, and finally the action state (Unk_00)
 * is set to 0xC.
 *
 * Flags left behind: on the early-return path those of the decrement; on the
 * reset path nz from the final MOV (Thumb MOVS) and c/v from the last helper
 * call, matching the original. */
Next aiAttack_TimerTickEnd_80EB194(void) {
    /* timer--; done for this frame unless it just hit zero. R0 keeps the
     * decremented value, as the original leaves it there for callers. */
    R0 = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(R0, 1);               /* SUBS r0, #1 */
    R0 -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, R0);
    if (R0 != 0)
        RETURN_TO_CALLER();

    /* Timer expired: no longer blocked, and the object starts moving. */
    R0 = 0x40;
    GUEST_CALL(object_clearFlag);

    R0 = 1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
    GUEST_CALL(object_setFlag1);

    /* Reload the timer for the next attack and ask the AI layer about it.
     * R0 still holds the reloaded value when sub_80F02A2 is entered. */
    R0 = LD16(R7 + oAIAttackVars_Unk_18);
    ST16(R7 + oAIAttackVars_Unk_10, R0);
    GUEST_CALL(sub_80F02A2);

    if (R0 != 0) {
        R0 = 0;
        GUEST_CALL(object_setAnimation);
    }

    /* Back to the idle/step state. */
    R0 = 0xC;
    SET_FLAGS_NZ(R0);
    ST16(R7 + oAIAttackVars_Unk_00, R0);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackDispatch_80EB436 ── */
/* battle_aiAttackDispatch_80EB436: AI attack dispatcher.
 *
 * The byte at the head of the AI attack-vars block (R7) selects one of the
 * attack routines in the jump table at jt_80EB448. The original tail-calls
 * the selected handler (LR holds the address of the following `pop {pc}`,
 * so the handler's return goes straight back to our caller), which the
 * GUEST_CALL/RETURN_TO_CALLER pair reproduces: the handler's registers and
 * condition flags pass through to the caller untouched.
 *
 * On entry to the handler: R0 = selector byte, R1 = the raw table entry
 * (handler address | Thumb bit), exactly as the original leaves them. */
Next battle_aiAttackDispatch_80EB436(void) {
    u32 slot = LD8(R7);                     /* selector at offset 0 of the block */

    R0 = slot;                              /* handler receives the selector */
    R1 = LD32(A_jt_80EB448 + 4 * slot);     /* table entry (Thumb | 1) */

    if (slot == 0)
        GUEST_CALL(sub_80EB450);
    else
        GUEST_CALL(sub_80EB502);

    RETURN_TO_CALLER();
}

/* ── aiAttackTimerTick_80EB758 ── */
/* The disassembly refers to the attack-phase byte at offset 1 as
 * oAIAttackVars_Unk_01; that name is not in the shared header, so provide it. */

/* aiAttackTimerTick_80EB758: AI attack-state timer tick (r7 = attack vars object).
 * On the first invocation (phase byte still 0) the countdown is initialised
 * to 5 frames and the phase byte is marked started (4).  Every call then
 * decrements the timer; when it goes negative the object leaves its attack
 * state.  Flags left behind are those of the timer decrement (or, when the
 * exit-state routine runs, whatever it leaves). */
Next aiAttackTimerTick_80EB758(void) {
    /* Initialise the attack countdown on the first invocation. */
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        ST16(R7 + oAIAttackVars_Unk_10, 5);
        ST8(R7 + oAIAttackVars_Unk_01, 4);
    }

    /* Decrement the frame counter and store it back. */
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    u32 remaining = timer - 1;
    ST16(R7 + oAIAttackVars_Unk_10, remaining);
    SET_FLAGS_CMP(timer, 1); /* flags as after SUBS timer, #1 */
    R0 = remaining;

    /* Timer expired: leave the attack state. */
    if ((s32)remaining < 0) {
        GUEST_CALL(object_exitAttackState);
    }

    RETURN_TO_CALLER();
}

/* ── aiAttack_BeginAttackSequence_80EB84C ── */
/* aiAttack_BeginAttackSequence_80EB84C(r7 = AI attack vars):
 * Begin a new attack sequence: pick the attack command (2 when the byte at
 * offset 0x03 is 11, otherwise 1) and store it to Unk_12, clear the state
 * field Unk_30, and reload the attack timer Unk_00 with 8.
 * Leaves r0 = 8, r1 = the Unk_03 byte that was read; flags come from the
 * final MOV r0, #8 (N/Z cleared, C/V untouched). */
Next aiAttack_BeginAttackSequence_80EB84C(void) {
    u32 unk03 = LD8(R7 + 3); /* oAIAttackVars_Unk_03 */

    /* cmp r1, #0xB -- branch is taken when the byte differs from 11 */
    SET_FLAGS_CMP(unk03, 0xB);
    u16 command = (unk03 == 0xB) ? 2 : 1;

    ST16(R7 + oAIAttackVars_Unk_12, command);
    ST32(R7 + oAIAttackVars_Unk_30, 0);

    R0 = 8;
    R1 = unk03; /* r1 still holds the byte loaded at the top */
    ST16(R7 + 0, 8); /* oAIAttackVars_Unk_00 */
    SET_FLAGS_NZ(R0); /* mov r0, #8 -- final N/Z update */
    RETURN_TO_CALLER();
}

/* ── battle_getSpecialActorSlotIfAiMatches_80EBB78 ── */
/* battle_getSpecialActorSlotIfAiMatches_80EBB78(r5 = battle object):
 * Reads the object's AI data and checks for a specific actor: actor type 1
 * with AI index 0x12 and version 4. R0 = that actor's slot (0xC), or 0.
 * R2 ends holding the AI data's version byte; R7 is untouched (the original
 * only uses it as scratch and restores it on return). */
Next battle_getSpecialActorSlotIfAiMatches_80EBB78(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData);                    /* oAIData_ActorType (offset 0) */
    u32 aiIndex = LD8(aiData + 1);                  /* oAIData_AIIndex   (offset 1) */
    u32 version = LD8(aiData + oAIData_Version_16);

    R2 = version;

    u32 slot = 0;
    if (actorType == 1 && aiIndex == 0x12 && version == 4)
        slot = 0xC;

    R0 = slot;
    RETURN_TO_CALLER();
}

/* ── aiAttack_dispatchStateHandler_80EBC0E ── */
/* aiAttack_dispatchStateHandler_80EBC0E: dispatch an AI attack sub-state handler.
 * R7 points at the AI attack vars; the state byte in its first field
 * selects the handler from a two-entry table in ROM:
 *   0 -> sub_80EBC28, 1 -> aiAttack_windupTick_80EBD3C.
 * The state byte is passed to the handler in R0 and the handler pointer
 * in R1 (as the original's `ldr r1, [table, state]` leaves it); the
 * `mov lr, pc / bx r1` sequence makes this an effective tail call, so the
 * handler's results (registers and flags) go straight back to our caller. */
Next aiAttack_dispatchStateHandler_80EBC0E(void) {
    u32 attackState = LD8(R7); /* oAIAttackVars state byte (offset 0) */

    R0 = attackState;
    R1 = LD32(A_off_80EBC20 + attackState); /* dispatch table entry */

    if (attackState == 0)
        GUEST_CALL(sub_80EBC28);
    else
        GUEST_CALL(aiAttack_windupTick_80EBD3C); /* state >= 2 reads past the table in the original */

    RETURN_TO_CALLER();
}

/* ── aiAttack_windupTick_80EBD3C ── */
/* aiAttack_windupTick_80EBD3C(r5 = battle object, r7 = AI attack vars):
 * Per-tick update for an AI-controlled attack. On the first tick (state byte
 * at [r7,#1] still 0) the object detaches its linked object, clears AI field
 * 0x68, plays the wind-up animation (7), and arms a short timer (3) before
 * switching to the "attacking" state (4). Every tick then counts the timer
 * down in r0; once it goes negative the object leaves the attack state.
 *
 * Flags out: from the timer subtraction, or from object_exitAttackState when
 * that call is made. r0 ends as timer-1 (or whatever the exit call leaves). */
Next aiAttack_windupTick_80EBD3C(void) {
    if (LD8(R7 + 1) == 0) {         /* oAIAttackVars_Unk_01: state byte */
        /* First tick: tear down the previous attack's leftovers and start
         * the wind-up animation. */
        ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);

        u32 ai = LD32(R5 + oBattleObject_AIDataPtr);
        ST32(ai + oAIData_Unk_68, 0);

        R0 = 7;                 /* animation id */
        R1 = ai;                /* AI data the animation applies to */
        GUEST_CALL(object_setAnimation);

        ST16(R7 + oAIAttackVars_Unk_10, 3);   /* attack timer */
        ST8(R7 + 1, 4);                       /* state: attack in progress */
    }

    /* Count the attack timer down in r0, as the original does; on underflow
     * the attack is over. */
    R0 = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(R0, 1);       /* flags of the original's SUBS r0, #1 */
    R0 -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, R0);

    if ((s32)R0 < 0) {          /* timer was 0: leave the attack state */
        GUEST_CALL(object_exitAttackState);
    }

    RETURN_TO_CALLER();
}

/* ── battle_getAiIndexFlag_80EBD68 ── */
/* battle_getAiIndexFlag_80EBD68(r5 = battle object):
 * For an AI-controlled actor (actor type == 2), return the per-AI-index byte
 * from the table at off_80EBD80 (custom AI slot flags); anything else returns 0.
 * Flags are left as the original leaves them: from CMP actorType,#2, with the
 * "mov r0,#0" path additionally forcing N=0/Z=1 (Thumb MOV-immediate sets flags). */
Next battle_getAiIndexFlag_80EBD68(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData + 0);        /* oAIData_ActorType */

    /* CMP r1, #2 */
    SET_FLAGS_CMP(actorType, 2);

    if (actorType == 2) {
        u32 aiIndex = LD8(aiData + 1);      /* oAIData_AIIndex */
        R0 = LD8(A_off_80EBD80 + aiIndex);
        /* only ldrb's since the CMP: flags stay as CMP left them */
    } else {
        R0 = 0;
        /* mov r0, #0 in Thumb sets N and Z; C and V keep the CMP's values */
        SET_FLAGS_NZ(0);
    }
    RETURN_TO_CALLER();
}
