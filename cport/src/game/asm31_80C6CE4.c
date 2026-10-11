/*
 * asm31_80C6CE4.c: readable functions of bn6f asm31.s, 0x080C6CE4-0x080E0F02, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_spawnBasicObjectKind0x16_80C6CE4 ── */
/* battle_spawnBasicObjectKind0x16_80C6CE4(r0..r2 = init args, r5 = context):
 * Spawns a type-3 battle object of kind 0x16. The spawn routine returns the
 * new object pointer in r5. If one was created, it is initialized as a basic
 * battle object with (newObject, arg0, arg1, 0).
 *
 * The original saves r0-r2/r5 on the stack and pops them back shifted by one
 * slot (r0->r1, r1->r2, r2->r3), while r5 is restored to its incoming value. */
Next battle_spawnBasicObjectKind0x16_80C6CE4(void) {
    u32 arg0 = R0;
    u32 arg1 = R1;
    u32 arg2 = R2;
    u32 savedR5 = R5;

    /* Spawn the object; the new object pointer comes back in r5. */
    R0 = 0x16;
    GUEST_CALL(object_spawnType3);
    u32 newObject = R5;

    /* mov r0, r5: keep the new object pointer in r0. */
    R0 = newObject;

    /* pop {r1-r3,r5}: saved args shift into r1-r3, r5 is restored. */
    R1 = arg0;
    R2 = arg1;
    R3 = arg2;
    R5 = savedR5;

    /* Only initialize if an object was actually spawned. */
    if (newObject != 0) {
        R3 = 0;
        GUEST_CALL(BasicBattleObjectInitialize);
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectDispatchPanelChange_80C6CFC ── */
/* battle_objectDispatchPanelChange_80C6CFC(r0, r1 = panel coords, r2 = panel change action):
 * dispatch a panel-state change requested by a battle object. The action code in
 * r2 selects what happens to the panel:
 *   0xFF -> nothing (leave the panel untouched),
 *   3    -> crack the panel,
 *   1    -> break the panel,
 *   else -> set the panel type directly from r0/r1.
 * All arguments (r0-r2 and the callee-saved r4-r7, r10) are passed through to the
 * chosen handler unchanged; its register results are the caller's results. */
Next battle_objectDispatchPanelChange_80C6CFC(void) {
    switch ((u8)R2) {
    case 0xFF:
        /* No-op action: return without touching the panel. */
        break;
    case 3:
        GUEST_CALL(object_crackPanel);
        break;
    case 1:
        GUEST_CALL(object_breakPanel_dup1);
        break;
    default:
        GUEST_CALL(object_setPanelType);
        break;
    }
    RETURN_TO_CALLER();
}

/* ── sub_80C80B8 ── */
/* sub_80C80B8: Initialize a battle object on entry to its action phase.
 * Sets Z to 0, builds collision data, plays spawn VFX/sfx, and marks
 * the state action phase as CUR_STATE_UPDATE. inputs: r5 r8 r10 lr
 * outputs: r5 r8 r10  clobbers: nz c v r0 r1 r2 r3 lr  preserved: r4 */
Next sub_80C80B8(void) {
    u32 saved_r4 = R4;

    /* Sync panel coordinates into the object's pos from its grid panel. */
    GUEST_CALL(object_setCoordinatesFromPanels);

    /* Reset Z so the object is drawn at panel height. */
    ST32(R5 + oBattleObject_Z, 0);

    /* Build the per-object collision data; bail if allocation failed. */
    GUEST_CALL(object_createCollisionData);
    if (R0 == 0) {
        GUEST_CALL(object_freeMemory);
        R4 = saved_r4;
        RETURN_TO_CALLER();
    }

    /* Configure collision shape (4 wide, 5 tall, panel-step 3) and flash. */
    R1 = 4;
    R2 = 5;
    R3 = 3;
    GUEST_CALL(object_setupCollisionData);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    GUEST_CALL(object_setCollisionHitEffect);

    /* Make the collision data visible this frame. */
    GUEST_CALL(object_presentCollisionData);

    /* Spawn the landing/entry effect at the object's current position,
     * shifted upward by 0x20 pixels. */
    R1 = LD32(R5 + oBattleObject_X);
    R2 = LD32(R5 + oBattleObject_Y);
    R3 = 0x20u << 16;
    R4 = 0;
    GUEST_CALL(spawn_t1_0x0_EffectObject);

    /* Play sound effect index 0x106 (0x07 + 0xFF). */
    R0 = 0x07 + 0xFF;
    GUEST_CALL(PlaySoundEffect);

    /* Transition this object's action-phase state into the Update step. */
    u32 next_state = CUR_STATE_UPDATE;
    SET_FLAGS_NZ(next_state);
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, next_state);

    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_80C81F0 ── */
/* sub_80C81F0: initialize the battle object's sprite from panel coordinates,
 * set its Z, load sprites/animations, configure flip and apply update phase.
 * Inputs:  r5 = battle object pointer, lr
 * Outputs: r5 (preserved)
 * Clobbers: r0, r1, r2, r3, lr, flags */
Next sub_80C81F0(void) {
    /* Position the object based on its panel grid coordinates. */
    GUEST_CALL(object_setCoordinatesFromPanels);

    /* Z16 (depth) is layered; 0 keeps the sprite on its normal plane. */
    ST16(R5 + oBattleObject_Z16, 0);

    /* sprite_load(tileCount=8, paletteCount=0xC, tileBase=0x80) */
    R0 = 0x80;
    R1 = 8;
    R2 = 0xC;
    GUEST_CALL(sprite_load);

    /* This object shouldn't cast a shadow. */
    GUEST_CALL(sprite_noShadow);

    /* Set flag bit 1 in the object header flags. */
    u8 flags = LD8(R5 + oObjectHeader_Flags);
    flags |= 0x02;
    ST8(R5 + oObjectHeader_Flags, flags);

    /* Animation index 0xD, stored in both the current and pending anim slots. */
    u8 anim = 0x0D;
    ST8(R5 + oBattleObject_CurAnim, anim);
    ST8(R5 + oBattleObject_CurAnimCopy, anim);

    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);

    /* Apply palette 0. */
    R0 = 0;
    GUEST_CALL(sprite_setPalette);

    /* Mirror the sprite to match the object's facing. */
    GUEST_CALL(object_getFlip);
    GUEST_CALL(sprite_setFlip);

    /* Advance into the Update sub-phase. */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    RETURN_TO_CALLER();
}

/* ── sub_80C8268 ── */
/* sub_80C8268: Battle object phase-4 state — seed timer once from Param2,
 * tick it each frame, dispatch Param3 while it lasts, then destroy. */
Next sub_80C8268(void) {
    if (LD8(R5 + oBattleObject_CurPhase) == 0) {
        /* First entry: set phase, init Param3, seed timers, run setup. */
        ST8(R5 + oBattleObject_CurPhase, 4);

        R0 = LD8(R5 + oBattleObject_Param3);
        GUEST_CALL(sub_80C832E);

        u32 param2 = LD8(R5 + oBattleObject_Param2);
        ST16(R5 + oBattleObject_Timer, param2);
        ST16(R5 + oBattleObject_Timer2, 1);

        GUEST_CALL(sub_80C82D0);
    }

    /* Tick the timer down by 1 and branch on the original's signed BLE. */
    u32 timer = LD16(R5 + oBattleObject_Timer);
    u32 new_timer = timer - 1;
    SET_FLAGS_CMP(timer, 1);
    ST16(R5 + oBattleObject_Timer, (u16)new_timer);

    if ((s32)new_timer > 0) {
        /* Timer still positive: dispatch the Param3 sub-handler. */
        R0 = LD8(R5 + oBattleObject_Param3);
        GUEST_CALL(sub_80C8310);
    } else {
        /* Timer expired: move to the destroy state. */
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80C8310 ── */
/* sub_80C8310(r0 = panel_count, r5 = battle_object):
 *   Highlight the front-facing panels of a battle object, one per step.
 *   Starting from the panel directly in front of the object, highlight
 *   `panel_count` consecutive panels in that direction.
 */
Next sub_80C8310(void) {
    u32 panel_count = R0;
    u32 obj = R5;
    u32 step;

    for (step = 1; (s32)step <= (s32)panel_count; step++) {
        /* object_getFrontDirection() returns the unit step along the axis the
         * object is facing. Result comes back in R0. */
        GUEST_CALL(object_getFrontDirection);
        s32 x = (s32)LD8(obj + oBattleObject_PanelX) + (s32)R0 * (s32)step;
        u32 y = LD8(obj + oBattleObject_PanelY);

        R0 = (u32)x;
        R1 = y;
        GUEST_CALL(object_highlightPanel);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80C8368 ── */
/* sub_80C8368(r5 = array of object pointers at base+0x60 stride 4, count 5):
 * For each of the 5 slots, if the pointer is non-NULL, dispatch sub_80E1B3C on it.
 * Used to update/draw the 5 battle-object slots attached to r5. */
Next sub_80C8368(void) {
    for (u32 slot = 0; slot < 5; slot++) {
        u32 ptr = LD32(R5 + (slot * 4 + 0x60));
        if (ptr != 0) {
            R0 = ptr;
            GUEST_CALL(sub_80E1B3C);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_80C913C ── */
Next sub_80C913C(void) {
    u32 obj = R5;
    u32 panel_x = LD8(obj + oBattleObject_PanelX);
    u32 panel_y = LD8(obj + oBattleObject_PanelY);

    /* Save panel_x/panel_y on the stack as the original does */
    u32 saved = SP -= 8;
    ST32(saved, panel_x);
    ST32(saved + 4, panel_y);

    R0 = panel_x;
    R1 = panel_y;
    GUEST_CALL(object_getPanelParameters);
    u32 params = R0;
    /* Restore panel_x/panel_y to R0/R1 like the original's pop {r0,r1} */
    R0 = panel_x;
    R1 = panel_y;
    SP += 8;

    /* tst r3, #0x600000 */
    SET_FLAGS_NZ(params & 0x600000);
    if ((params & 0x600000) == 0) goto fail;

    /* sub_800AE54(panel_x, panel_y, 1 - alliance) */
    u32 alliance = LD8(obj + oBattleObject_Alliance);
    R0 = panel_x;
    R1 = panel_y;
    R2 = (u32)(0 - alliance) + 1;
    GUEST_CALL(sub_800AE54);
    u32 found = R0;

    SET_FLAGS_NZ(found);
    if (found == 0) goto fail;

    ST32(obj + oBattleObject_RelatedObject1Ptr, found);

    /* Pick a slot: dword_203CFB0 (Alliance 0) or dword_203CFB0+0xC (Alliance != 0) */
    u32 slot = (u32)A_dword_203CFB0;
    alliance = LD8(obj + oBattleObject_Alliance);
    SET_FLAGS_NZ(alliance);
    if (alliance == 0) {
        slot += 0xC;
    }

    ST32(slot, 0);
    ST32(slot + 4, obj);
    ST32(slot + 8, found);

    R0 = found;
    RETURN_TO_CALLER();

fail:
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── battle_BDT_update2_80c950c ── */
/* battle_BDT_update2_80c950c:
 *   inputs:  r5 lr
 *   outputs: r5
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r5 r6 r7
 *
 * Per-frame update for a battle object (BDT entry): apply velocity to
 * position, refresh panel/collision state, tick down a timer, and when the
 * timer expires decrement a sub-counter. When that counter reaches zero,
 * advance the animation, clear the collision region and mark the object for
 * destruction; otherwise hand off to battle_BDT_80c956c for the next phase.
 */
Next battle_BDT_update2_80c950c(void) {
    /* Integrate X velocity into position. */
    s32 x      = (s32)LD32(R5 + oBattleObject_X);
    s32 x_vel  = (s32)LD32(R5 + oBattleObject_XVelocity);
    ST32(R5 + oBattleObject_X, x + x_vel);

    /* Integrate Y velocity into position. */
    s32 y      = (s32)LD32(R5 + oBattleObject_Y);
    s32 y_vel  = (s32)LD32(R5 + oBattleObject_YVelocity);
    ST32(R5 + oBattleObject_Y, y + y_vel);

    /* Re-derive the panels the object occupies and update its collision map. */
    GUEST_CALL(object_setPanelsFromCoordinates);
    GUEST_CALL(object_updateCollisionPanels);

    /* Tick the per-frame timer; bail out if it hasn't elapsed yet. */
    u32 timer = LD16(R5 + oBattleObject_Timer) - 1;
    ST16(R5 + oBattleObject_Timer, (u16)timer);
    SET_FLAGS_NZ(timer);
    if (!ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    /* Timer elapsed: decrement the remaining-hit counter. */
    u32 remaining = LD8(R5 + oBattleObject_Param2) - 1;
    ST8(R5 + oBattleObject_Param2, (u8)remaining);
    SET_FLAGS_NZ(remaining);
    if (!ZERO_FLAG()) {
        /* More hits left: kick off the next phase. */
        GUEST_CALL(battle_BDT_80c956c);
        RETURN_TO_CALLER();
    }

    /* Last hit consumed: bump the animation, drop the collision region,
     * and queue the object for destruction at the next state transition. */
    u32 anim = LD8(R5 + oBattleObject_CurAnim) + 1;
    ST8(R5 + oBattleObject_CurAnim, (u8)anim);

    GUEST_CALL(object_clearCollisionRegion);

    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
         CUR_STATE_DESTROY);

    RETURN_TO_CALLER();
}

/* ── CheckIfThunderObjectFindsOpponentByGoingRight ── */
/* CheckIfThunderObjectFindsOpponentByGoingRight(r4 = opponent mask, r5 = battleObject):
 * scans rightward from the object's current panelX, checking Y from 3 down to 0;
 * returns the panelX where the opponent mask matches (or 0 if none found in x=panelX..6).
 *   inputs:  r4 r5 lr
 *   outputs: r4 r6 r7
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 */
Next CheckIfThunderObjectFindsOpponentByGoingRight(void) {
    u32 x = LD8(R5 + oBattleObject_PanelX);
    u32 y;

    for (;;) {
        for (y = 3;; y--) {
            R0 = x;
            R1 = y;
            GUEST_CALL(object_getPanelParameters);
            if (R0 & R4) goto found;
            if (y == 0) break;
        }
        x++;
        if (x == 7) { x = 0; break; }
    }
found:
    R6 = x;
    R7 = y;
    RETURN_TO_CALLER();
}

/* ── sub_80C97E0 ── */
/* register form: calls object_spawnType3 outside this file, which has no typed C interface yet */
static u32 call_object_spawnType3(u32 type, u32 arg1, u32 arg2, u32 arg3) {
    R0 = type;
    R1 = arg1;
    R2 = arg2;
    R3 = arg3;
    GUEST_CALL(object_spawnType3);
    return R5;    /* object_spawnType3 answers in r5: the new object, or 0 if it failed */
}

/* Spawn a battle object of type 0x2B via object_spawnType3 and cross-link it with
 * the object that requested the spawn. r1-r3 are spawn parameters that the caller
 * of this function passes straight through to object_spawnType3 untouched.
 * Returns the new object, or 0 if no object could be spawned.
 * Flag note: the caller-visible c/v flags come from the `adds r0, r5, #0` null
 * check below, i.e. c = v = 0 and nz = (spawned != 0). */
static u32 spawn_linked_battle_object(u32 arg1, u32 arg2, u32 arg3, u32 owner, u32 extraVars) {
    u32 spawned = call_object_spawnType3(0x2B, arg1, arg2, arg3);

    if (spawned == 0)
        return spawned;    /* spawn failed: skip all linking */

    /* Two-way link: new object points back at its owner, and the owner's
     * extra-vars slot points at the new object. */
    ST32(spawned + oBattleObject_RelatedObject1Ptr, owner);
    ST32(spawned + oBattleObject_ExtraVars, extraVars);
    ST32(extraVars, spawned);

    /* The new object inherits the owner's alliance/direction-flip setting. */
    ST16(spawned + oBattleObject_AllianceAndDirectionFlip,
         LD16(owner + oBattleObject_AllianceAndDirectionFlip));

    /* Mark the new object with flag bits 0x04 and 0x10 (flags byte is at offset 0). */
    ST8(spawned, LD8(spawned) | 0x14);

    return spawned;
}

/* sub_80C97E0: spawn object type 0x2B and link it to the requesting object.
 * r1-r3: pass-through arguments for object_spawnType3
 * r5: the requesting (owner) object; r7: owner's link-slot pointer
 * Outputs r3 (as left by object_spawnType3) and c/v from the null check;
 * r4-r7 are preserved (r5 restored exactly as the original's pop does). */
Next sub_80C97E0(void) {
    u32 owner = R5;
    u32 spawned = spawn_linked_battle_object(R1, R2, R3, owner, R7);
    R5 = owner;          /* the original pop {r5} undoes the spawn call's r5 */
    R0 = spawned;        /* the original leaves the new object in r0 (clobbered) */
    SET_FLAGS_ADD(spawned, 0);   /* `mov r0, r5` == adds r0, r5, #0 */
    RETURN_TO_CALLER();
}

/* ── battle_objectCountdownFireAttackTick_80C9CA2 ── */
/* battle_objectCountdownFireAttackTick_80C9CA2: per-tick countdown for a battle object (r5 = object).
 *
 * Decrements the object's timer; while it is still nonzero there is nothing
 * more to do this frame. Once it reaches zero the object "fires": the game
 * asks for the direction towards the player, sub_80C9CDA places the attack
 * on the panel in front of the object, and the object switches to action 4.
 *
 * The original pushes/pops r4 and r6 around the calls: sub_80C9CDA clobbers
 * r6, but callers expect this routine to preserve it (along with r4/r5/r7),
 * so both are saved and restored on the firing path as well. */

/* The CurAction field has no named macro in the headers; the disassembly
 * stores it at struct offset 9 (strb r0, [r5,#9]). */
#define oBattleObject_CurAction_sub_80C9CA2 9

Next battle_objectCountdownFireAttackTick_80C9CA2(void) {
    u32 obj = R5;
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    /* Tick the countdown (halfword field); flags come from the subtraction. */
    u32 timer = LD16(obj + oBattleObject_Timer);
    R0 = timer - 1;
    ST16(obj + oBattleObject_Timer, R0);
    SET_FLAGS_CMP(timer, 1);

    if (R0 != 0) { /* still counting down */
        RETURN_TO_CALLER();
    }

    /* Timer expired: aim at the panel between the object and the player. */
    GUEST_CALL(object_getFrontDirection); /* r0 = direction to the player */

    R0 += LD8(obj + oBattleObject_PanelX);
    R1 = LD8(obj + oBattleObject_PanelY);
    R3 = LD32(obj + oBattleObject_Z);
    R6 = LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler);

    GUEST_CALL(sub_80C9CDA); /* places the attack; r1/r3/c/v are its outputs */

    /* Attack launched: move the object into its post-attack action state. */
    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(obj + oBattleObject_CurAction_sub_80C9CA2, R0);

    /* Callees preserve r4 but not r6; restore both as the original's pop does. */
    R4 = savedR4;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── battle_objectDestroyWhenAnimFrameDone_80C9CC4 ── */
/* State constant used by the battle-object state machine: the "destroyed"
 * state this helper moves an object into once its death animation ends. */
#define CUR_STATE_DESTROY 8

/* battle_objectDestroyWhenAnimFrameDone_80C9CC4 (r5 = battle object):
 * Query the object's current animation frame parameters; while the frame
 * still carries the 0x80 bit the death animation is running, so do nothing.
 * Once the bit clears, drop the object's collision region and switch it to
 * the destroyed state.
 * Leaves r1 = 0x80 (from the TST operand) as the original does. Flags: on the
 * early-out path nz comes from the TST; on the destroy path nz comes from
 * MOVS r0, #CUR_STATE_DESTROY. c/v are whatever the last callee left. */
Next battle_objectDestroyWhenAnimFrameDone_80C9CC4(void) {
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;

    R1 = 0x80; /* operand of the TST below, live in r1 on both paths */

    if (frameParams & 0x80) {
        GUEST_CALL(object_clearCollisionRegion);

        R0 = CUR_STATE_DESTROY;
        ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
        SET_FLAGS_NZ(R0); /* MOVS r0, #CUR_STATE_DESTROY */
    } else {
        SET_FLAGS_NZ(frameParams & 0x80); /* TST r0, r1 */
    }

    RETURN_TO_CALLER();
}

/* ── t3_0x33_80CA544 ── */
/* Dispatch a battle object's current state through a three-entry jump table,
 * then update its sprite. The state byte is a direct offset into the table
 * (0, 4, or 8). Each handler and object_updateSprite may modify any guest
 * register; the register state after the final call is the complete result.
 *
 * register form: indirect dispatch through a jump table whose entries have no
 * typed C interface; all guest registers pass through to callees that modify
 * them in ways the type system cannot express. */
static void dispatch_battle_state(u32 r5) {
    u8 state = (u8)LD8(r5 + oBattleObject_CurState);
    R0 = state;
    R1 = LD32(A_off_80CA55C + state);

    switch (state) {
        case 0: GUEST_CALL(sub_80CA568);           break;
        case 4: GUEST_CALL(sub_80CA61A);           break;
        case 8: GUEST_CALL(object_genericDestroy);  break;
    }

    GUEST_CALL(object_updateSprite);
}

/* t3_0x33_80CA544(r5 = battle object ptr) -> all registers */
Next t3_0x33_80CA544(void) {
    dispatch_battle_state(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnAndInitObject0x33_80CA686 ── */
/* battle_spawnAndInitObject0x33_80CA686: spawn a battle object of type 0x33 and hand it the caller's
 * arguments. The new object pointer comes back from object_spawnType3 in r5;
 * if one was spawned, it is initialized via BasicBattleObjectInitialize with
 * the caller's r0-r2 shifted down into r1-r3 (the push/pop pair in the
 * original performs this shuffle). Returns the object pointer in r0
 * (0 if nothing was spawned); r1-r3 are clobbered. */
Next battle_spawnAndInitObject0x33_80CA686(void) {
    u32 arg0 = R0, arg1 = R1, arg2 = R2, arg5 = R5;

    R0 = 0x33;                     /* battle object type to spawn */
    GUEST_CALL(object_spawnType3); /* new object returned in R5 */

    u32 object = R5;
    R0 = object;
    if (object != 0) {
        R1 = arg0;
        R2 = arg1;
        R3 = arg2;
        R5 = arg5;
        GUEST_CALL(BasicBattleObjectInitialize);
    }

    RETURN_TO_CALLER();
}

/* ── battle_object_SetStateUpdatePhase_80CAB88 ── */
/* battle_object_SetStateUpdatePhase_80CAB88(r5 = battle object):
 * Advance the object's state machine: mark the current state as needing its
 * update step to run. Leaves r0 = 4 (flags set by the mov). */
Next battle_object_SetStateUpdatePhase_80CAB88(void) {
    R0 = 4; /* CUR_STATE_UPDATE */
    ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_objectDestroyWithRelated2Unlink_80CAC44 ── */
/* battle_objectDestroyWithRelated2Unlink_80CAC44: destroy a battle object (r5 = object to destroy).
 * Before destroying it, break the back-link from its related object: if that
 * object's header field (at offset 0) still points at this object, clear it so
 * it doesn't reference a soon-to-be-dead object. Flags and r0-r3 afterwards
 * are whatever object_genericDestroy leaves behind. */
Next battle_objectDestroyWithRelated2Unlink_80CAC44(void) {
    u32 related = LD32(R5 + oBattleObject_RelatedObject2Ptr);

    if (LD32(related) == R5)
        ST32(related, 0);

    GUEST_CALL(object_genericDestroy);
    RETURN_TO_CALLER();
}

/* ── battle_applyElementAttackModIfEnabled_80CAD14 ── */
/* battle_applyElementAttackModIfEnabled_80CAD14(r0 = flag, r5 = battle object):
 * If r0 is nonzero, load the object's element (r2), attack parameter (r4)
 * and damage-counter disable word (r6), then hand off to sub_80CAEC8 to
 * apply the elemental attack modifier. r4/r6 are callee-saved: the original
 * pushes them (with lr) and pops them after the call, so both branches leave
 * them exactly as the caller had them. Either way the Z flag from testing r0
 * is left for the caller. */
Next battle_applyElementAttackModIfEnabled_80CAD14(void) {
    if (R0 != 0) {
        /* push {r4,r6,lr}: reserve the frame so the callee sees the same
         * stack, keeping r4/r6 intact across the call. */
        u32 frame = SP - 12;
        SP = frame;
        ST32(frame, R4);
        ST32(frame + 4, R6);
        ST32(frame + 8, LR);

        R2 = LD8(R5 + oBattleObject_Element);
        R4 = LD8(R5 + 4);
        R6 = LD32(R5 + oBattleObject_DamageAndStaminaDamageCounterDisabler);
        GUEST_CALL(sub_80CAEC8);

        /* pop {r4,r6,pc}: restore the caller's r4/r6 (lr is handled by
         * RETURN_TO_CALLER). Flags are whatever the callee left. */
        R4 = LD32(frame);
        R6 = LD32(frame + 4);
        SP = frame + 12;
    } else {
        /* tst r0, r0: only nz is affected, c/v stay as the caller set them */
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80CAF14 ── */
/* Tables indexed by (param1 << 2) for ship extra-variable offsets. */
#define SHIP_EXTRA_TABLE_1  A_byte_80CAF78
#define SHIP_EXTRA_TABLE_2  A_byte_80CAF8C

#define FRONT_DIR_SCALE  0x140000
#define SHIP_Z_DEFAULT   0x280000
#define SHIP_SPRITE_ID   0x80
#define SHIP_SPRITE_LAYER 8
#define SHIP_SPRITE_PRIO  0xD
#define SHIP_ANIM_ID      0x16
#define OBJ_FLAG_ACTIVE   (1u << 1)
#define CUR_STATE_UPDATE_sub_80CAF14  4

/* register form: calls object_setCoordinatesFromPanels outside this file, which has no typed C interface yet */
static void call_set_coordinates(void) {
    GUEST_CALL(object_setCoordinatesFromPanels);
}

/* register form: calls object_getFrontDirection outside this file, which has no typed C interface yet */
static s32 call_get_front_direction(void) {
    GUEST_CALL(object_getFrontDirection);
    return (s32)R0;
}

/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void call_sprite_load(u32 id, u32 layer, u32 priority) {
    R0 = id; R1 = layer; R2 = priority;
    GUEST_CALL(sprite_load);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_no_shadow(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_set_animation(u32 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_load_anim_data(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_set_palette(u32 pal) {
    R0 = pal;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls object_getFlip outside this file, which has no typed C interface yet */
static u32 call_get_flip(void) {
    GUEST_CALL(object_getFlip);
    return R0;
}

/* register form: calls sprite_setFlip outside this file, which has no typed C interface yet */
static void call_set_flip(u32 flip) {
    R0 = flip;
    GUEST_CALL(sprite_setFlip);
}

/* Result of ship initialization: only the values the original explicitly leaves in r0/r1.
 * r2 is left behind by sprite_setFlip's GUEST_CALL and must not be overridden. */
typedef struct { u32 phase; u32 r1; } ShipInitResult;

/* Initialize a battle-ship sprite: front-row X offset, default Z, sprite load,
 * animation/palette setup, flip propagation, param1-indexed extra vars, and
 * transition to the update phase. */
static ShipInitResult init_ship(u32 obj) {
    /* Place on the front row: X += direction × scale, Z = fixed altitude. */
    call_set_coordinates();
    s32 dir = call_get_front_direction();
    ST32(obj + oBattleObject_X, (s32)LD32(obj + oBattleObject_X) + dir * (s32)FRONT_DIR_SCALE);
    ST32(obj + oBattleObject_Z, SHIP_Z_DEFAULT);

    /* Load ship sprite and disable its shadow. */
    call_sprite_load(SHIP_SPRITE_ID, SHIP_SPRITE_LAYER, SHIP_SPRITE_PRIO);
    call_no_shadow();

    /* Mark object as active/visible in the header flags byte. */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | OBJ_FLAG_ACTIVE);

    /* Assign and load the idle animation. */
    ST8(obj + oBattleObject_CurAnim, SHIP_ANIM_ID);
    ST8(obj + oBattleObject_CurAnimCopy, SHIP_ANIM_ID);
    call_set_animation(SHIP_ANIM_ID);
    call_load_anim_data();

    /* Default palette 0, then propagate the object's facing to the sprite. */
    call_set_palette(0);
    u32 flip = call_get_flip();
    call_set_flip(flip);

    /* Copy param1-indexed extra-variable offsets from the two ship tables. */
    u32 idx = (u32)LD8(obj + oBattleObject_Param1) << 2;
    u32 extra1 = LD32(SHIP_EXTRA_TABLE_1 + idx);
    u32 extra2 = LD32(SHIP_EXTRA_TABLE_2 + idx);
    ST32(obj + oBattleObject_ExtraVars, extra1);
    ST32(obj + oBattleObject_ExtraVars + 4, extra2);

    /* Transition to the update state. */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE_sub_80CAF14);

    /* The original's last write to r1 is the table-2 load; r2 is left by sprite_setFlip. */
    ShipInitResult r = { CUR_STATE_UPDATE_sub_80CAF14, extra2 };
    return r;
}

/* sub_80CAF14(r5 = battle object) -> r0=CUR_STATE_UPDATE_sub_80CAF14, r1=extra2, nz from r0 */
Next sub_80CAF14(void) {
    ShipInitResult res = init_ship(R5);
    R0 = res.phase;
    R1 = res.r1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_destroyWrapper_80CB078 ── */
/* object_destroyWrapper_80CB078: thin wrapper that destroys the object pointed to by R0
 * via the generic object destructor; all results and flags come straight
 * from object_genericDestroy. */
Next object_destroyWrapper_80CB078(void) {
    GUEST_CALL(object_genericDestroy);
    RETURN_TO_CALLER();
}

/* ── sub_80CB080 ── */
/* register form: calls object_spawnType3 outside this file, which has no typed C interface yet */
static u32 spawn_object_type3(u32 type) {
    u32 saved_r5 = R5;
    R0 = type;
    GUEST_CALL(object_spawnType3);
    u32 result = R5;
    R5 = saved_r5;
    return result;
}

struct SpawnConfigResult {
    u32 r1;
    u32 r2;
    u32 r3;
};

/* Spawn a type-0x38 battle object and configure it: set future panel coords,
 * copy panel position / alliance / element / damage-disabler from the source,
 * and activate the object (flags |= 0x10).
 * Returns the final values of r1, r2, r3 that the original leaves behind. */
static struct SpawnConfigResult configure_spawned_object(
    u32 future_x, u32 future_y, u32 element,
    u32 source_obj, u32 damage_disabler)
{
    struct SpawnConfigResult res;
    u32 new_obj = spawn_object_type3(0x38);

    if (new_obj == 0) {
        /* Spawn failed: original pop shifts r0→r1, r1→r2, r2→r3 */
        res.r1 = future_x;
        res.r2 = future_y;
        res.r3 = element;
        return res;
    }

    ST8(new_obj + oBattleObject_FuturePanelX, future_x);
    ST8(new_obj + oBattleObject_FuturePanelY, future_y);
    ST8(new_obj + oBattleObject_PanelX,
        LD8(source_obj + oBattleObject_PanelX));
    ST8(new_obj + oBattleObject_PanelY,
        LD8(source_obj + oBattleObject_PanelY));
    ST8(new_obj + oBattleObject_Element, element);
    ST32(new_obj + oBattleObject_DamageAndStaminaDamageCounterDisabler,
         damage_disabler);
    u16 alliance = LD16(source_obj + oBattleObject_AllianceAndDirectionFlip);
    ST16(new_obj + oBattleObject_AllianceAndDirectionFlip, alliance);

    u8 flags = LD8(new_obj + oObjectHeader_Flags) | 0x10;
    ST8(new_obj + oObjectHeader_Flags, flags);

    /* Final register state left by the original's last writes */
    res.r1 = 0x10;
    res.r2 = flags;
    res.r3 = alliance;
    return res;
}

/* sub_80CB080(r0=futureX, r1=futureY, r2=element, r5=sourceObj, r6=damageDisabler)
 * Outputs r1, r2, r3; r4–r7 preserved. */
Next sub_80CB080(void) {
    struct SpawnConfigResult res = configure_spawned_object(
        R0, R1, R2, R5, R6);
    R1 = res.r1;
    R2 = res.r2;
    R3 = res.r3;
    RETURN_TO_CALLER();
}

/* ── sub_80CB0AE ── */
#define COLLISION_REGION_TYPE    0x4050101u
#define COLLISION_SPAWN_MODE     3u
#define COLLISION_REGION_TIMER   5u

/* register form: calls object_spawnCollisionRegion outside this file, which has no typed
   C interface yet.  Saves/restores r4/r6/r7/lr across the guest call to replicate the
   original's push {r4,r6,r7,lr} / pop {r4,r6,r7,pc} preservation, and writes the timer
   (r1 = 5; strh [r0 + timer]) on success because those side-effects depend on the
   callee's register result and must happen before the saved registers are restored. */
static u32 spawn_collision_region_call(u8 panel_x, u8 panel_y, u8 element,
                                       u32 obj, u32 dmg_disable) {
    u32 save_r4 = R4, save_r6 = R6, save_r7 = R7, save_lr = LR;

    R0 = panel_x;
    R1 = panel_y;
    R2 = element;
    R3 = 0;
    R4 = COLLISION_REGION_TYPE;
    R5 = obj;
    R6 = dmg_disable;
    R7 = COLLISION_SPAWN_MODE;

    GUEST_CALL(object_spawnCollisionRegion);

    u32 region = R0;
    if (region != 0) {
        R1 = COLLISION_REGION_TIMER;
        ST16(region + oBattleObject_Timer, COLLISION_REGION_TIMER);
    }

    R4 = save_r4;
    R6 = save_r6;
    R7 = save_r7;
    LR = save_lr;

    return region;
}

/* Read a battle object's panel coordinates, element, and damage-counter-disabler flag,
   spawn its collision region, and set the region's timer to 5 on success.
   Returns the new region's object pointer, or 0 if the spawn failed. */
static u32 spawn_collision_for_object(u32 obj) {
    u8 panel_x = LD8(obj + oBattleObject_PanelX);
    u8 panel_y = LD8(obj + oBattleObject_PanelY);
    u8 element = LD8(obj + oBattleObject_Element);
    u32 dmg_disable = LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler);

    return spawn_collision_region_call(panel_x, panel_y, element, obj, dmg_disable);
}

/* sub_80CB0AE(r5 = battle object) -> c, v (region spawn + timer init) */
Next sub_80CB0AE(void) {
    u32 region = spawn_collision_for_object(R5);
    SET_FLAGS_NZ(region);
    RETURN_TO_CALLER();
}

/* ── sub_80CE138 ── */
/* sub_80CE138: Initialize a battle object sprite, set its flags, load
 * its graphics and animation, place it on the panel grid, and queue
 * the "update" state for the next frame. */
Next sub_80CE138(void) {
    /* Mark the object as active (flag bit 1). */
    u8 flags = LD8(R5);
    ST8(R5, flags | 0x02);

    /* Load sprite graphics (sprite id 0x80, tiles 0x0C, palette 0x50). */
    R0 = 0x80;
    R1 = 0x0C;
    R2 = 0x50;
    GUEST_CALL(sprite_load);

    /* Start the entry animation. */
    R0 = 4;
    ST16(R5 + oBattleObject_CurAnimAndCurAnimCopy, R0);
    GUEST_CALL(sprite_setAnimation);

    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_update);
    GUEST_CALL(sprite_noShadow);
    GUEST_CALL(object_setCoordinatesFromPanels);

    /* Z = 0xA00000 (in front of the field), reset the per-state timer,
     * and enter the update phase. */
    ST32(R5 + oBattleObject_Z, 0x00A00000);
    ST16(R5 + oBattleObject_Timer, 0);
    ST32(R5 + 8, CUR_STATE_UPDATE);

    GUEST_CALL(sub_80CE178);

    RETURN_TO_CALLER();
}

/* ── sub_80D84DE ── */
/* sub_80D84DE: respawn / reinitialize a battle object.
 * inputs:  r2 r4 r5 r8 r9 r10 r11 r12 lr
 * outputs: r5 r8 r9 r10 r11 r12
 * clobbers: nz c v r0 r1 r2 r3 r4 lr
 * preserved: r6 r7
 *
 * r5 is the battle object pointer. Marks it as hidden/being-respawned
 * (sets bit 1 of its header flags), clears its final palette, runs the
 * generic post-death hook, sets HP to 1, installs the standard list of
 * per-state handler functions, refreshes collision data and the timestop
 * sprite bookkeeping. */
Next sub_80D84DE(void) {
    SP -= 4;
    ST32(SP, LR);

    /* Set oObjectHeader_Flags bit 1 (object-hidden / respawn flag). */
    u8 flags = LD8(R5 + oObjectHeader_Flags) | 2;
    SET_FLAGS_NZ(flags);
    ST8(R5 + oObjectHeader_Flags, flags);

    GUEST_CALL(sprite_clearFinalPalette);
    GUEST_CALL(sub_801AD9E);

    ST16(R5 + oBattleObject_HP, 1);

    R0 = A_off_80D8508;
    GUEST_CALL(sub_801B394);

    GUEST_CALL(object_presentCollisionData);
    GUEST_CALL(object_updateSpriteTimestop);

    LR = LD32(SP);
    SP += 4;
    RETURN_TO_CALLER();
}

/* ── sub_80DA834 ── */
struct U32Pair { u32 first, second; };

/* register form: calls object_setCoordinatesFromPanels outside this file, which has no typed C interface yet */
static void call_set_coords_from_panels(void) {
    GUEST_CALL(object_setCoordinatesFromPanels);
}

/* register form: calls sub_8011504 outside this file, which has no typed C interface yet */
static u32 call_sub_8011504(u32 collision_a, u32 collision_b) {
    R0 = collision_a;
    R1 = collision_b;
    GUEST_CALL(sub_8011504);
    return R2;
}

/* register form: calls object_setCollisionHitEffect outside this file, which has no typed C interface yet */
static void call_set_hit_effect(u32 col_desc) {
    R0 = 0xFF;
    R1 = 2;
    R2 = col_desc;
    GUEST_CALL(object_setCollisionHitEffect);
}

/* register form: calls object_presentCollisionData outside this file, which has no typed C interface yet */
static void call_present_collision(void) {
    /* R1 = 2, R2 = col_desc already in registers; callee updates them */
    GUEST_CALL(object_presentCollisionData);
}

/* register form: calls sprite_setUnk0x2c outside this file, which has no typed C interface yet */
static void call_sprite_palette(u32 color) {
    R0 = color;
    GUEST_CALL(sprite_setUnk0x2c);
}

/* register form: calls battle_getFlags outside this file, which has no typed C interface yet */
static u32 call_battle_getFlags(void) {
    /* R2 is preserved by the callee (already set from presentCollisionData) */
    GUEST_CALL(battle_getFlags);
    return R0;
}

/* register form: calls sub_80DA886 outside this file, which has no typed C interface yet */
static void call_sub_80DA886(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(sub_80DA886);
    /* R0-R2 and nz/c/v now hold this function's outputs */
}

/* Initialise a battle object from its spawn data.
 * Sets position, Z=0, collision descriptor, active flag, HP=1, hit effect,
 * collision geometry, sprite palette, state=UPDATE, then picks an initial
 * action (0 or 4) based on battle flag bit 0. */
static void init_battle_object(u32 obj) {
    call_set_coords_from_panels();

    /* zero the Z coordinate */
    ST32(obj + oBattleObject_Z, 0);

    /* build collision descriptor from fixed data */
    u32 col_desc = call_sub_8011504(0x100100C, 0xE0F0300);

    /* mark object active (set flag bit 2) */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* HP and MaxHP = 1 */
    ST16(obj + oBattleObject_HP, 1);
    ST16(obj + oBattleObject_MaxHP, 1);

    /* hit-effect id 0xFF on collision */
    call_set_hit_effect(col_desc);

    /* present collision geometry (updates R1/R2 in-place) */
    call_present_collision();

    /* palette tint */
    call_sprite_palette(0xFF80000);

    /* enter UPDATE phase */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    /* choose initial action from battle flags */
    u32 battle_flags = call_battle_getFlags();
    u32 action;
    u32 arg_r0, arg_r1;

    if (battle_flags & 1) {
        /* bit 0 set: clear object flag bit 4, action = 4 */
        u8 new_flags = (u8)(LD8(obj + oObjectHeader_Flags) & ~4);
        ST8(obj + oObjectHeader_Flags, new_flags);
        action  = 4;
        arg_r0  = new_flags;
        arg_r1  = 4;
    } else {
        /* bit 0 clear: keep flags, action = 0 */
        action = 0;
        arg_r0 = battle_flags;
        arg_r1 = 1;
    }

    ST8(obj + oBattleObject_CurAction, action);

    /* finalise the action state machine; outputs R0-R2 and flags */
    call_sub_80DA886(arg_r0, arg_r1, action);
}

/* sub_80DA834(r5 = battle object) -> void (outputs R0-R2, flags from sub_80DA886) */
Next sub_80DA834(void) {
    init_battle_object(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectLingerThenDestroyPhase_80DA974 ── */
/* battle_objectLingerThenDestroyPhase_80DA974 (r5 = battle object, r10 = toolkit):
 * Death phase of an object that lingers briefly before despawning.
 * On the first entry it copies the object's halfword parameter at +4 into
 * the battle state, switches to animation 1, loads a 90-frame timer,
 * clears its collision region and marks the phase initialized (4).
 * Every entry then counts the timer down; once it expires the object is
 * switched to the DESTROY state (offset +8 = 8).
 * Outputs: R0 = decremented timer (or 8 on the destroy path), R1 = the
 * parameter value on the setup path, flags from the decrement (or from
 * `mov r0, #8` on the destroy path; c/v untouched there). */
#define CUR_STATE_DESTROY_sub_80DA974 8

Next battle_objectLingerThenDestroyPhase_80DA974(void) {
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        /* one-time setup for this phase */
        u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
        R1 = LD16(R5 + 4); /* oBattleObject_Param1 */
        ST16(battle + oBattleState_Unk_36, R1);

        ST8(R5 + oBattleObject_CurAnim, 1);
        ST16(R5 + oBattleObject_Timer, 0x5A);

        GUEST_CALL(object_clearCollisionRegion);

        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    }

    /* count the remaining time down; keep going while it is still positive */
    u32 timer = LD16(R5 + oBattleObject_Timer);
    SET_FLAGS_CMP(timer, 1); /* subs r0, #1 */
    timer -= 1;
    ST16(R5 + oBattleObject_Timer, timer);
    R0 = timer;

    if ((s32)timer > 0)
        RETURN_TO_CALLER();

    /* timer expired: switch the object to the destroy state */
    R0 = CUR_STATE_DESTROY_sub_80DA974;
    SET_FLAGS_NZ(R0); /* mov r0, #CUR_STATE_DESTROY_sub_80DA974 only updates n/z */
    ST32(R5 + 8 /* oBattleObject_CurStateActionPhaseAndPhaseInitialized */, R0);
    RETURN_TO_CALLER();
}

/* ── battle_spawnTypeA9ObjectAtPanel_80DA9DC ── */
/* battle_spawnTypeA9ObjectAtPanel_80DA9DC(r0 = panel X, r1 = panel Y, r2 = element):
 * Spawns the battle object with type ID 0xA9 via object_spawnType3 (which
 * also sees the untouched r1/r2 and r3 = 0, and returns the new object
 * pointer in r5, 0 = none).  The original spills r0-r2/r5 before the call
 * and pops them shifted by one slot, so on return:
 *   r0 = object pointer, r1 = caller's r0, r2 = caller's r1, r3 = caller's r2,
 *   r5 preserved.
 * If an object was created, it is placed on the given panel, given the
 * element, and flag bits 0x14 are set in its header flags (offset 0); r1/r2
 * then hold the updated flags and the OR mask, as in the original.
 * Flags: the "mov r0, r5" is really adds r0, r5, #0, which sets nz and
 * clears c/v; the orr afterwards updates nz only, leaving c = v = 0. */
Next battle_spawnTypeA9ObjectAtPanel_80DA9DC(void) {
    u32 panelX = R0;
    u32 panelY = R1;
    u32 element = R2;
    u32 callerR5 = R5;

    R0 = 0xA9;      /* object type to spawn */
    R3 = 0;
    GUEST_CALL(object_spawnType3);

    u32 object = R5;   /* spawn result: new object pointer, or 0 if none */
    R5 = callerR5;
    R1 = panelX;
    R2 = panelY;
    R3 = element;
    R0 = object;

    /* adds r0, r5, #0 — the flag-setting "mov" */
    SET_FLAGS_ADD(object, 0);

    if (object != 0) {
        ST8(object + oBattleObject_PanelX, panelX);
        ST8(object + oBattleObject_PanelY, panelY);
        ST8(object + oBattleObject_Element, element);

        u32 flags = LD8(object) | 0x14;   /* oObjectHeader_Flags is at offset 0 */
        SET_FLAGS_NZ(flags);              /* orr sets nz only; c/v stay cleared */
        ST8(object, flags);

        R1 = flags;
        R2 = 0x14;
    }
    RETURN_TO_CALLER();
}

/* ── rng_pickRandomHalfwordFromTableEntry_80DA9FE ── */
/* rng_pickRandomHalfwordFromTableEntry_80DA9FE(r0 = entry index):
 * Pick one of the eight halfwords inside the 16-byte table entry at
 * byte_80211A0 + index*16, using a positive RNG value masked to 0..7.
 * r0 = the selected halfword; r4 and r7 are untouched. */
Next rng_pickRandomHalfwordFromTableEntry_80DA9FE(void) {
    u32 entry = A_byte_80211A0 + ((u32)R0 << 4);

    GUEST_CALL(GetPositiveSignedRNG);

    R0 = LD16(entry + (R0 & 0xE));
    RETURN_TO_CALLER();
}

/* ── t3_0xd3_80DFE40 ── */
/* t3_0xd3_80DFE40(r5 = battle object):
 * Runs the object's current state handler, then refreshes its sprite.
 * The state byte doubles as a byte offset into the handler table (states are
 * stored pre-multiplied by 4: 0/4/8 select the three entries), so no shift
 * is applied when indexing. r0/r1 hold the state and the handler's own table
 * entry when the handler is entered; r0 afterwards is whatever the handler
 * and object_updateSprite leave behind. */
Next t3_0xd3_80DFE40(void) {
    u32 state = LD8(R5 + 8);               /* oBattleObject_CurState */

    R0 = state;                            /* handlers receive the state index */
    R1 = LD32(A_off_80DFE58 + state);      /* ...and their own table entry */

    switch (state) {
    case 0 * 4: GUEST_CALL(sub_80DFE64);          break; /* alive/normal */
    case 1 * 4: GUEST_CALL(sub_80DFEA8);          break; /* second phase */
    case 2 * 4: GUEST_CALL(object_genericDestroy); break; /* destroyed */
    }

    GUEST_CALL(object_updateSprite);

    RETURN_TO_CALLER();
}

/* ── battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84 ── */
/* battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84(r0 = panel X, r1 = panel Y, r2 = element, r5 = object whose
 *             alliance/direction-flip halfword is copied, r6 = damage word):
 * Spawns a battle object of type 0xD3 via object_spawnType3 (the new object
 * comes back in r5, NULL if none could be created) and, on success, fills in
 * its panel position, element, damage/stamina word and the alliance/
 * direction-flip halfword taken from the object originally in r5.
 * On return: r0 = the new object pointer (r5 as returned by the spawner),
 * r1 = old r0, r2 = old r1, r3 = copied halfword (old r2 if spawn failed),
 * r5 = old r5; r4/r6/r7 untouched. */
Next battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84(void) {
    u32 oldR0 = R0;
    u32 oldR1 = R1;
    u32 oldR2 = R2;
    u32 oldR5 = R5;
    u32 oldR6 = R6;

    R0 = 0xD3;                          /* battle object type to spawn */
    GUEST_CALL(object_spawnType3);      /* spawned object returned in r5 */

    u32 obj = R5;
    R0 = obj;                           /* mov r0, r5 (unconditional) */

    /* pop {r1-r3,r5}: saved r0-r2 shift back one register */
    R1 = oldR0;
    R2 = oldR1;
    R3 = oldR2;
    R5 = oldR5;

    if (obj != 0) {
        ST8(obj + oBattleObject_PanelX, oldR0);
        ST8(obj + oBattleObject_PanelY, oldR1);
        ST8(obj + oBattleObject_Element, oldR2);
        ST32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler, oldR6);
        u32 alliance = LD16(oldR5 + oBattleObject_AllianceAndDirectionFlip);
        ST16(obj + oBattleObject_AllianceAndDirectionFlip, alliance);
        R3 = alliance;                  /* ldrh overwrites r3 on this path */
    }

    RETURN_TO_CALLER();
}

/* ── t4_0x0_80E0548 ── */
/* t4_0x0_80E0548: battle-object state dispatcher (ROM jump table at off_80E055C) */

/* register form: calls sub_80E0568 outside this file, which has no typed C interface yet */
static u32 call_sub_80E0568(u32 state, u32 handler_addr) {
    R0 = state;
    R1 = handler_addr;
    GUEST_CALL(sub_80E0568);
    return R0;
}

/* register form: calls sub_80E05C4 outside this file, which has no typed C interface yet */
static u32 call_sub_80E05C4(u32 state, u32 handler_addr) {
    R0 = state;
    R1 = handler_addr;
    GUEST_CALL(sub_80E05C4);
    return R0;
}

/* register form: calls object_freeMemory outside this file, which has no typed C interface yet */
static u32 call_object_freeMemory(u32 state, u32 handler_addr) {
    R0 = state;
    R1 = handler_addr;
    GUEST_CALL(object_freeMemory);
    return R0;
}

/* Dispatch a battle object to its per-state handler via the ROM jump table.
 * The state byte is used directly as a byte offset into the table (0, 4, or 8). */
static u32 dispatch_battle_object_state(u32 battle_obj) {
    u32 state = LD8(battle_obj + oBattleObject_CurState);
    u32 handler_addr = LD32(A_off_80E055C + state);
    switch (state) {
    case 0: return call_sub_80E0568(state, handler_addr);
    case 4: return call_sub_80E05C4(state, handler_addr);
    case 8: return call_object_freeMemory(state, handler_addr);
    }
    return 0;
}

Next t4_0x0_80E0548(void) {
    R0 = dispatch_battle_object_state(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80E0568 ── */
/* Register-form helpers — used by the typed function below. Declared up-front so the
 * C compiler sees them before they're called. Each one matches one external game
 * function that has no typed C interface yet. */
static void rf_sprite_load(int a1, int a2, int a3);
static void rf_sprite_loadAnimationData(void);
static void rf_sprite_noShadow(void);
static void rf_sprite_setAnimation(u8 a1);
static void rf_sprite_update(void);
static void rf_sprite_setPalette(int palette);
static void rf_sprite_setFlip(u8 a1);
static void rf_sprite_setPriority_8002E14(int priority);
static void rf_battle_MirrorRelatedObjectFlag1_80E0616(void);

/* Battle object "init" state action.
 *
 * Called from a battle object's state machine once when it first spawns. Looks up
 * a sprite descriptor by the per-object "Param1" index, loads its tile/anim/palette
 * data, applies the object's own Param2/Param3/Param4 tweaks, marks the object's
 * header flags, and finally advances the object's state phase to CUR_STATE_UPDATE.
 *
 * `obj` is the battle object's base address (the caller's r5). Returns the value
 * placed in r0 (CUR_STATE_UPDATE), which the caller also stores to memory.
 */
static u32 battle_object_init_action(u32 obj) {
    u32 desc_index = LD8(obj + oBattleObject_Param1);
    u32 desc = A_byte_80E0398 + (desc_index << 2);

    /* sprite_load(tile_data=0x80, sprite_id=desc[0], palette_id=desc[1]) */
    rf_sprite_load(0x80, LD8(desc), LD8(desc + 1));
    rf_sprite_loadAnimationData();
    rf_sprite_noShadow();

    /* Kick off the sprite animation described by desc[2]. */
    u8 anim = LD8(desc + 2);
    ST8(obj + oBattleObject_CurAnim, anim);
    ST8(obj + oBattleObject_CurAnimCopy, anim);
    rf_sprite_setAnimation(anim);
    rf_sprite_loadAnimationData();
    rf_sprite_update();

    /* desc[3] is a base palette index; Param3 (a u8) is added on top. */
    rf_sprite_setPalette(LD8(desc + 3) + LD8(obj + oBattleObject_Param3));

    rf_sprite_setFlip(LD8(obj + oBattleObject_Param2));

    /* Set the "active" header flag (bit 1). */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* If Param4 is non-zero, force the sprite to BG priority 0. */
    if (LD8(obj + oBattleObject_Param4) != 0)
        rf_sprite_setPriority_8002E14(0);

    rf_battle_MirrorRelatedObjectFlag1_80E0616();

    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
    return CUR_STATE_UPDATE;
}

/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void rf_sprite_load(int a1, int a2, int a3) {
    R0 = a1;
    R1 = a2;
    R2 = a3;
    GUEST_CALL(sprite_load);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void rf_sprite_loadAnimationData(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void rf_sprite_noShadow(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void rf_sprite_setAnimation(u8 a1) {
    R0 = a1;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static void rf_sprite_update(void) {
    GUEST_CALL(sprite_update);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void rf_sprite_setPalette(int palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setFlip outside this file, which has no typed C interface yet */
static void rf_sprite_setFlip(u8 a1) {
    R0 = a1;
    GUEST_CALL(sprite_setFlip);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void rf_sprite_setPriority_8002E14(int priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls battle_MirrorRelatedObjectFlag1_80E0616 outside this file, which has no typed C interface yet */
static void rf_battle_MirrorRelatedObjectFlag1_80E0616(void) {
    GUEST_CALL(battle_MirrorRelatedObjectFlag1_80E0616);
}

Next sub_80E0568(void) {
    R0 = battle_object_init_action(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80E05C4 ── */
/* sub_80E05C4: Battle-object timer tick – decrement timer, possibly destroy, update sprite. */

#define CUR_STATE_DESTROY_sub_80E05C4 8
#define oBattleObject_FrameData 0x20  /* signed per-frame data, offset from battle-object base */

/* register form: calls battle_MirrorRelatedObjectFlag1_80E0616 outside this file, which has no typed C interface yet */
static void check_mirror_related_flag(u32 object) {
    R5 = object;
    GUEST_CALL(battle_MirrorRelatedObjectFlag1_80E0616);
}

/* register form: calls sprite_getFrameParameters outside this file, which has no typed C interface yet */
static u32 get_frame_parameters(u32 object) {
    R5 = object;
    GUEST_CALL(sprite_getFrameParameters);
    return R0;
}

/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static void call_sprite_update(u32 object) {
    R5 = object;
    GUEST_CALL(sprite_update);
}

/* Tick the object's countdown timer.  If it expires, or if the current frame
 * carries bit 7 while the frame-data word is non-positive, destroy the object.
 * Always finish with sprite_update (whose outputs become ours). */
static void battle_object_timer_update(u32 object) {
    u16 new_timer = (u16)(LD16(object + oBattleObject_Timer) - 1);
    ST16(object + oBattleObject_Timer, new_timer);

    bool destroy = (new_timer == 0);

    if (!destroy) {
        check_mirror_related_flag(object);
        u32 frame_params = get_frame_parameters(object);
        if (frame_params & 0x80) {
            s16 frame_data = LDS16(object + oBattleObject_FrameData);
            if (frame_data <= 0)
                destroy = true;
        }
    }

    if (destroy) {
        u8 flags = LD8(object + oObjectHeader_Flags) & ~2u;
        ST8(object + oObjectHeader_Flags, flags);
        ST32(object + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY_sub_80E05C4);
    }

    call_sprite_update(object);
}

Next sub_80E05C4(void) {
    battle_object_timer_update(R5);
    /* sprite_update's GUEST_CALL already left r0–r3, nz, c, v in place */
    RETURN_TO_CALLER();
}

/* ── spawn_t1_0x0_EffectObject ── */
/* spawn_t1_0x0_EffectObject: spawn a type-4 battle object with effect type 0.
 * The remaining arguments (r1-r3) and the r5 context pointer are passed
 * straight through to object_spawnType4. Returns the object the callee
 * leaves in r5 (flags reflect that value), while restoring the caller's r5. */
Next spawn_t1_0x0_EffectObject(void) {
    u32 callerR5 = R5;

    R0 = 0;                            /* effect type 0 */
    GUEST_CALL(object_spawnType4);

    /* mov r0, r5 (encoded as ADDS r0, r5, #0, so it sets the flags) */
    R0 = R5;
    SET_FLAGS_ADD(R5, 0);

    R5 = callerR5;
    RETURN_TO_CALLER();
}

/* ── battle_MirrorRelatedObjectFlag1_80E0616 ── */
/* battle_MirrorRelatedObjectFlag1_80E0616(r5 = battle object): copy the "related object 1" bit of the
 * flag byte. Clears our flag bit 1, then sets it again iff the related
 * object has it set; does nothing if the object has no extra vars.
 * Flags (c/v) reflect `extra == 0`, as the original's CMP leaves them. */
Next battle_MirrorRelatedObjectFlag1_80E0616(void) {
    u32 extra = LD32(R5 + oBattleObject_ExtraVars);

    SET_FLAGS_CMP(extra, 0);
    if (extra != 0) {
        u32 related = LD32(R5 + oBattleObject_RelatedObject1Ptr);

        /* The object header flag byte sits at offset 0 of the object. */
        u32 flags = LD8(R5) & ~2u;

        R2 = 2;
        R1 = LD8(related) & R2;
        ST8(R5, flags | R1);
    }

    RETURN_TO_CALLER();
}

/* ── battle_object_teardownAndFree_80E06E0 ── */
/* battle_object_teardownAndFree_80E06E0: teardown routine for a battle object.
 * Asks sub_800AA92 (with argument 0) whether cleanup should be skipped;
 * if it returns nonzero we bail out early. Otherwise it runs
 * battle_orStateFlags5c_8001382(2) and then releases the object whose pointer that call
 * leaves in r0 via object_freeMemory.
 * Flags on the early-exit path reflect the TST of sub_800AA92's result. */
Next battle_object_teardownAndFree_80E06E0(void) {
    R0 = 0;
    GUEST_CALL(sub_800AA92);

    if (R0 != 0) {
        /* tst r0, r0 */
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    R0 = 2;
    GUEST_CALL(battle_orStateFlags5c_8001382);
    /* r0 still holds battle_orStateFlags5c_8001382's result: the object to free. */
    GUEST_CALL(object_freeMemory);

    RETURN_TO_CALLER();
}

/* ── battle_spawnType4ObjectWithActiveFlag_80E06F8 ── */
/* battle_spawnType4ObjectWithActiveFlag_80E06F8(r1-r3 = arguments forwarded to the spawner, r5 = object header):
 * Spawns a type-4 object through object_spawnType4 (r0 = 2 selects the
 * variant), then sets flag bit 0x04 in the oObjectHeader_Flags byte of the
 * object the spawner leaves in r5, and hands that object back in r0.
 * The caller's r4/r5 are preserved (push/pop in the original). */
Next battle_spawnType4ObjectWithActiveFlag_80E06F8(void) {
    u32 callerR4 = R4;                 /* push {r4,r5,lr} */
    u32 callerR5 = R5;

    R0 = 2;                            /* variant id for object_spawnType4 */
    GUEST_CALL(object_spawnType4);

    u32 object = R5;                   /* object to mark, as left by the spawner */
    ST8(object, LD8(object) | 4);      /* set flag 0x04 in its header */

    R0 = object;                       /* mov r0, r5 */
    R4 = callerR4;                     /* pop {r4,r5,pc} */
    R5 = callerR5;
    RETURN_TO_CALLER();
}

/* ── t4_0x4_80E0844 ── */
/* t4_0x4_80E0844: per-tick dispatcher for a battle object (r5 = object).
 *
 * Reads the object's state byte (offset oBattleObject_CurState = 8) and calls
 * the handler selected from the jump table at off_80E0858. The state byte is
 * used directly as a byte offset into the table, so the values stored in the
 * field are 0, 4 or 8:
 *   0 -> sub_80E0864        (init/idle step)
 *   4 -> sprite_updateDestroyWhenAnimEnds_80E08A6        (main step)
 *   8 -> object_freeMemory  (teardown: release the object)
 *
 * The handler is entered with R0 = state byte and R1 = the table entry it was
 * fetched from (the original's `ldr r1,[r1,r0]; bx r1` leaves the handler's
 * Thumb address in R1). `mov lr, pc; bx r1` followed by `pop {pc}` makes this
 * a pass-through call: the handler's register results and flags flow straight
 * through to our caller, and LR/SP end up unchanged. States outside 0/4/8
 * never occur in the game; the original would index past the table. */
Next t4_0x4_80E0844(void) {
    u32 state = LD8(R5 + 8); /* oBattleObject_CurState */

    R0 = state;
    R1 = LD32(A_off_80E0858 + state);

    switch (state) {
    case 0:
        GUEST_CALL(sub_80E0864);
        break;
    case 4:
        GUEST_CALL(sprite_updateDestroyWhenAnimEnds_80E08A6);
        break;
    default:
        /* state == 8 (and, impossible in practice, anything larger): the
         * table's last entry is the object destructor. */
        GUEST_CALL(object_freeMemory);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sprite_updateDestroyWhenAnimEnds_80E08A6 ── */
/* sprite_updateDestroyWhenAnimEnds_80E08A6 (r5 = object header):
 * Sprite update callback for an object whose animation has finished.
 * Asks the sprite for its current frame parameters; if they signal
 * "sequence finished" (bit 7 set), clear the object's flag bit 1 (bit 0x2
 * of the header flags, at offset 0) and move the object into the DESTROY
 * state (8), so the shared object state machine will take it down.
 * Finally ticks the sprite to its next frame.  Returns with whatever
 * r0-r3 and the condition flags sprite_update left behind. */
Next sprite_updateDestroyWhenAnimEnds_80E08A6(void) {
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;

    if (frameParams & 0x80) {
        /* oObjectHeader_Flags == 0 */
        u32 flags = LD8(R5);
        ST8(R5, flags & ~0x2u);
        ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 8 /* CUR_STATE_DESTROY */);
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── chargeShotChargeObject_80E0DF0 ── */
/*
 * chargeShotChargeObject_80E0DF0 -- state dispatcher for the charge-shot
 * charge-up battle object.
 *
 * r5 holds the object. Its current-state byte is used directly as a byte
 * offset into the three-entry handler table at off_80E0E04, i.e. the object
 * stores its state pre-multiplied by four:
 *   state 0 -> chargeShotChargeObject_init   (first tick after spawn)
 *   state 4 -> chargeShotChargeObject_update (per-frame behaviour)
 *   state 8 -> object_freeMemory             (object is being removed)
 *
 * The handler is invoked with the state byte in r0 and its own table entry
 * (its own address) in r1; every other register passes through untouched.
 * The original performs the call with "mov lr, pc; bx r1" and follows it
 * with a pop {pc}, so after the handler returns this function simply
 * returns to its own caller.
 */
Next chargeShotChargeObject_80E0DF0(void) {
    /* ldrb r0, [r5, #oBattleObject_CurState] (offset 8 in the object) */
    u32 state = LD8(R5 + 8);

    R0 = state;
    R1 = LD32(A_off_80E0E04 + state); /* handler address from the method table */

    switch (state) {
    case 0: /* table slot 0: spawn/initialisation */
        GUEST_CALL(chargeShotChargeObject_init_80E0E10);
        break;
    case 4: /* table slot 1: per-frame update */
        GUEST_CALL(chargeShotChargeObject_update_80E0E20);
        break;
    default: /* table slot 2 (state 8): release the object's memory */
        GUEST_CALL(object_freeMemory);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── chargeShotChargeObject_init_80E0E10 ── */
/* chargeShotChargeObject_init_80E0E10(r5 = charge shot battle object):
 * State initializer for the charge-shot charge object: set its extra var to
 * active, advance it to the UPDATE phase, then immediately run its update step
 * (the update's final flag-setting instruction provides our nz/c/v outputs). */


Next chargeShotChargeObject_init_80E0E10(void) {
    ST32(R5 + oBattleObject_ExtraVars + 4, 1);

    R0 = CUR_STATE_UPDATE;
    ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);

    GUEST_CALL(chargeShotChargeObject_update_80E0E20);

    RETURN_TO_CALLER();
}
