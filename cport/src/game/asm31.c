/*
 * asm31.c: readable functions of bn6f asm31.s, up to 0x080C6CE4, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_ResetObjectTimerTo8_80C4072 ── */
/* battle_ResetObjectTimerTo8_80C4072(r0 = object pointer):
 * If the object exists, reset the field at offset +8 to 8 (likely a wait/counter).
 * Leaves r1 untouched when the pointer is null. */
Next battle_ResetObjectTimerTo8_80C4072(void) {
    if (R0 != 0) {
        R1 = 8;
        ST32(R0 + 8, R1);
    }
    RETURN_TO_CALLER();
}

/* ── battle_SetObjectExtraFlag14_80C4086 ── */
/* battle_SetObjectExtraFlag14_80C4086(r0 = battle object):
 * Mark the object's "extra vars" flag at ExtraVars+0x14 (e.g. an on-field
 * status trigger); does nothing when passed a null object pointer. */
Next battle_SetObjectExtraFlag14_80C4086(void) {
    if (R0 != 0)
        ST32(R0 + oBattleObject_ExtraVars + 0x14, 1);
    RETURN_TO_CALLER();
}

/* ── battle_spawnObject56LinkedToOwner_80C44A8 ── */
/* battle_spawnObject56LinkedToOwner_80C44A8(r1..r3 = spawn x/y/z, r5 = owner BattleObject):
 * Spawns battle object type 0x56 via object_spawnType1. The original pushes
 * r5 (the owner pointer) before the call, so the callee sees it as its fifth
 * (`params`) stack argument at [SP]. On success the new object is linked
 * back to the owner, the owner's AllianceAndDirectionFlip halfword is copied
 * over, and header flag bits 0x04|0x10 are set. r0 = new object pointer, or
 * NULL if the spawn failed.
 * Flags: `mov r0, r5` is really ADDS r0, r5, #0, so it sets N/Z and clears
 * C/V; on the success path the final ORRS only updates N/Z, leaving C/V at
 * those cleared values. */

/* The flags byte sits at offset 0 of the object header; provide the offset
 * locally in case the shared header does not define the symbolic name. */

Next battle_spawnObject56LinkedToOwner_80C44A8(void) {
    u32 owner = R5;

    /* Reserve the two pushed words ({lr}, {r5}); the callee reads the saved
     * owner pointer at [SP] as its params argument. */
    SP -= 8;
    ST32(SP, owner);
    ST32(SP + 4, LR);

    R0 = 0x56;
    GUEST_CALL(object_spawnType1);

    SP += 8;

    u32 spawned = R5;          /* object_spawnType1 returns its pointer in r5 */
    R0 = spawned;
    SET_FLAGS_ADD(spawned, 0); /* mov r0, r5 == ADDS r0, r5, #0: N/Z set, C=0, V=0 */
    R5 = owner;                /* pop {r5} restores the owner pointer */

    if (spawned == 0)
        RETURN_TO_CALLER();

    ST32(spawned + oBattleObject_RelatedObject1Ptr, owner);
    ST16(spawned + oBattleObject_AllianceAndDirectionFlip,
         LD16(owner + oBattleObject_AllianceAndDirectionFlip));

    /* Set header flag bits 0x04|0x10 in the new object. */
    u32 flags = LD8(spawned + oObjectHeader_Flags) | 0x14;
    R1 = flags;
    R2 = 0x14;
    ST8(spawned + oObjectHeader_Flags, flags);
    SET_FLAGS_NZ(flags);       /* ORRS updates N/Z only; C/V stay 0/0 from the MOV */
    RETURN_TO_CALLER();
}

/* ── battle_EndObjectCurrentAnim_80C44D2 ── */
/* battle_EndObjectCurrentAnim_80C44D2(r0 = battle object):
 * End the object's current animation: write 0xFF (finished) into its
 * current-animation slot and call sub_801BCD0 to refresh its animation
 * state. A NULL object pointer is a no-op. r4/r5 are preserved.
 * Flags: with a NULL object they come from the tst; otherwise the last
 * flag-setting instruction before the call is the Thumb "mov r5, r0",
 * which is really "adds r5, r0, #0" and clears C/V. */
Next battle_EndObjectCurrentAnim_80C44D2(void) {
    u32 savedR5 = R5;

    SET_FLAGS_NZ(R0);              /* tst r0, r0 */
    if (R0 == 0)
        RETURN_TO_CALLER();

    ST8(R0 + oBattleObject_CurAnimCopy, 0xFF);
    R5 = R0;                       /* helper takes the object in r5 */
    SET_FLAGS_ADD(R0, 0);          /* "mov r5, r0" == adds r5, r0, #0 */
    GUEST_CALL(sub_801BCD0);
    R5 = savedR5;                  /* pop {r5} restores the caller's r5 */
    RETURN_TO_CALLER();
}

/* ── t1_0x5e_pinkRingOnCounter_80C4B18 ── */
/* t1_0x5e_pinkRingOnCounter_80C4B18: tick function of the tutorial battle
 * object that draws the pink "counter hit" ring over MegaMan.
 *
 * Each tick it runs the handler for the object's current state (the handler
 * table holds one routine per state: init, update, free), and then animates
 * the object's sprite unless the battle is in a time-stop or is paused.
 * Omitting those two guards is what keeps the pink ring from showing up when
 * MegaMan lands a counter hit. */
enum {
    PINKRING_STATE_INIT = 0,
    PINKRING_STATE_RUN  = 4,
    PINKRING_STATE_FREE = 8,
};

/* Offset of the current-state byte in a battle object (oBattleObject_CurState). */
#define OBJ_CUR_STATE_OFFSET 8

Next t1_0x5e_pinkRingOnCounter_80C4B18(void) {
    u32 state = LD8(R5 + OBJ_CUR_STATE_OFFSET);

    /* Dispatch to the state handler; the handler sees R0 = state and
     * R1 = handler address, exactly as the original leaves them. */
    R0 = state;
    R1 = LD32(A_off_80C4B3C + state);
    switch (state) {
    case PINKRING_STATE_INIT: GUEST_CALL(battle_stateEnter_loadSprite0x80_80C4B48);       break;
    case PINKRING_STATE_RUN:  GUEST_CALL(sub_80C4B84);       break;
    case PINKRING_STATE_FREE: GUEST_CALL(object_freeMemory); break;
    }

    /* Freeze the sprite while time-stopped (e.g. during a counter hit). */
    GUEST_CALL(battle_isTimeStop);
    if (R0 != 0)
        RETURN_TO_CALLER();

    /* ...and while the battle is paused. */
    GUEST_CALL(battle_isPaused);
    if (R0 != 0)
        RETURN_TO_CALLER();

    GUEST_CALL(UpdateBattleObjectSprite);

    RETURN_TO_CALLER();
}

/* ── battle_stateEnter_loadSprite0x80_80C4B48 ── */
/* battle_stateEnter_loadSprite0x80_80C4B48: state-entry handler for a battle object (r5 = object).
 *
 * Loads the object's sprite resources, selects and starts the animation for
 * this state, switches the object into the "update" phase, and then runs the
 * per-frame update hook (sub_80C4B84).
 *
 * The original pushes/pops r4 around the calls, so R4 must be preserved even
 * though callees clobber it. r5, r6 and r10 are left untouched. */

/* Phase value stored into oBattleObject_CurPhaseAndPhaseInitialized. */
#define CUR_STATE_UPDATE 4

Next battle_stateEnter_loadSprite0x80_80C4B48(void) {
    u32 saved_r4 = R4;

    /* Load sprite resources: sprite 0x80, animation 0x14, palette 0x16. */
    R0 = 0x80;
    R1 = 0x14;
    R2 = 0x16;
    GUEST_CALL(sprite_load);

    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    /* Set flag bit 1 in the object header flags (at offset 0). */
    ST8(R5, LD8(R5) | 2);

    /* Ask which animation this state starts with (result in r1) and apply it. */
    GUEST_CALL(sub_80C4C52);
    u32 anim = R1;
    ST8(R5 + oBattleObject_CurAnim, anim);
    ST8(R5 + oBattleObject_CurAnimCopy, anim);
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);

    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_update);

    /* Advance to the "update" phase of the current state action. */
    ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    /* Run the per-frame update for this state; its results in r0-r3 are ours. */
    GUEST_CALL(sub_80C4B84);

    /* pop {r4, pc}: restore the caller's r4. */
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── SpawnCounterPinkRingMaybe_80C4C12 ── */
/* OBJECT_FLAG_PAUSE_UPDATE: bit in oObjectHeader_Flags that suspends an
 * object's normal update until something clears it. */

/* SpawnCounterPinkRingMaybe_80C4C12
 *   r1 = x, r2 = y, r3 = z, r4 = spawn params,
 *   r5 = owner BattleObject (the one that scored the counter hit),
 *   r7 = pointer that receives the new object.
 *
 * Spawns battle object type 0x5E -- the pink ring that hovers over MegaMan
 * after a counter hit -- copies the owner's alliance/direction-flip state
 * into it, links the two objects together, reports the new ring through the
 * out pointer, and pauses its updates until its script takes over. */
Next SpawnCounterPinkRingMaybe_80C4C12(void) {
    u32 owner = R5;

    /* object_spawnType1(r0 = object type, r1..r3 = x/y/z, r4 = params);
     * the new object comes back in r5. */
    R0 = 0x5E;
    GUEST_CALL(object_spawnType1);
    u32 ring = R5;
    R5 = owner; /* the callee returns in r5, but r5 is preserved for the caller */

    if (ring != 0) {
        /* inherit the owner's alliance / direction-flip halfword */
        ST16(ring + oBattleObject_AllianceAndDirectionFlip,
             LD16(owner + oBattleObject_AllianceAndDirectionFlip));

        /* link ring -> owner, and hand the ring back through the out pointer */
        ST32(ring + oBattleObject_RelatedObject1Ptr, owner);
        ST32(ring + oBattleObject_ExtraVars + 4, R7);
        ST32(R7, ring);

        ST8(ring + oBattleObject_Unk_0c, 1);

        /* set the pause-update flag on the ring's object header;
         * the original leaves the flag value in r2 */
        R2 = OBJECT_FLAG_PAUSE_UPDATE;
        ST8(ring, LD8(ring) | R2);
    }

    RETURN_TO_CALLER();
}

/* ── battle_SpawnAndInitObjectType3_80C4FFE ── */
/* battle_SpawnAndInitObjectType3_80C4FFE(r0-r2 = init arguments, r1-r3 also passed to the spawner):
 * Spawns a type-3 battle object via object_spawnType3 (called with r0 = 0;
 * the other arguments pass straight through). The new object comes back in
 * r5; if it is non-NULL, BasicBattleObjectInitialize is run on it with this
 * thunk's original r0-r2 shifted down into its r1-r3. The stack shuffling in
 * the original (push {r0-r2,r5,r6} / pop {r1-r3,r5,r6}) just preserves the
 * incoming arguments across the calls and slides r0-r2 into r1-r3.
 * Flags on the "nothing spawned" path: the "mov r0, r5" is really
 * "adds r0, r5, #0", so N/Z come from the object pointer and C/V are 0
 * (adding zero never carries or overflows). */
Next battle_SpawnAndInitObjectType3_80C4FFE(void) {
    u32 arg0 = R0, arg1 = R1, arg2 = R2;
    u32 savedR5 = R5, savedR6 = R6;

    /* Spawn with type = 0; r1-r3 are still the caller's values here. */
    R0 = 0;
    GUEST_CALL(object_spawnType3);

    u32 object = R5;

    /* adds r0, object, #0: N/Z from the pointer, C = V = 0. */
    SET_FLAGS_ADD(object, 0);

    /* Restore the incoming arguments for the initializer:
     * init(object, arg0, arg1, arg2). */
    R0 = object;
    R1 = arg0;
    R2 = arg1;
    R3 = arg2;
    R5 = savedR5;
    R6 = savedR6;

    if (object != 0)
        GUEST_CALL(BasicBattleObjectInitialize);

    RETURN_TO_CALLER();
}

/* ── battle_objectStepUpOntoElevatedPanel_80C5090 ── */
/* battle_objectStepUpOntoElevatedPanel_80C5090(r5 = battle object):
 * Once this object's Param1 (offset 4) reaches 0x1D, shift it one tile
 * upward — both its Y and Z fixed-point coordinates move by 0x10000.
 * Used when an object steps up onto an elevated panel. */
Next battle_objectStepUpOntoElevatedPanel_80C5090(void) {
    if (LD8(R5 + 4) == 0x1D) {
        ST32(R5 + oBattleObject_Y, LD32(R5 + oBattleObject_Y) + 0x10000);
        ST32(R5 + oBattleObject_Z, LD32(R5 + oBattleObject_Z) + 0x10000);
    }
    RETURN_TO_CALLER();
}

/* ── battle_object_stepLifetimeDestroyOnHit_80C532E ── */
/* battle_object_stepLifetimeDestroyOnHit_80C532E(r5 = battle object):
 * Tear-down/step handler for a battle object that owns collision data.
 * Removes the object's collision data, spawns its "hit" effect, copies the
 * collision result flags into the object's extra var (if it keeps one), then
 * either lets the object live another tick (no collision flags and its
 * lifetime timer is still positive) or destroys it: clears its collision
 * region, frees the collision data block and frees the object itself.
 *
 * Exit state: r5/r4/r6/r8/r10 preserved, r7 = the object's collision data
 * pointer; r0-r3 and the flags are whatever the last helper call left. */
Next battle_object_stepLifetimeDestroyOnHit_80C532E(void) {
    u32 obj = R5;

    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);

    u32 colData = LD32(obj + oBattleObject_CollisionDataPtr);
    u32 hitFlags = LD32(colData + oCollisionData_FlagsFromCollision);
    u32 flagsOut = LD32(obj + oBattleObject_ExtraVars + 0x10);
    R7 = colData;

    /* If the object records collision results, store just the flag bits it
     * cares about (bits 26-27 of the collision flags). */
    if (flagsOut != 0) {
        u32 masked = hitFlags & 0xC000000;
        ST32(flagsOut, masked);
        R2 = masked;
    }

    if (hitFlags != 0) {
        /* Collided with something: destroy the object this frame. */
        R0 = hitFlags;
        R1 = flagsOut;
    } else {
        /* Count the lifetime timer down; destroy it once it expires. */
        u32 timer = LD16(obj + oBattleObject_Timer) - 1;
        ST16(obj + oBattleObject_Timer, (u16)timer);
        R0 = timer;
        R1 = flagsOut;
        if ((s32)timer > 0) {
            /* Still alive: refresh the (now empty) collision data and return. */
            GUEST_CALL(object_presentCollisionData);
            RETURN_TO_CALLER();
        }
    }

    GUEST_CALL(object_clearCollisionRegion);
    R0 = LD32(obj + oBattleObject_CollisionDataPtr);
    GUEST_CALL(object_freeCollisionData);
    GUEST_CALL(object_freeMemory);
    RETURN_TO_CALLER();
}

/* ── battleObject_PlayHurtAnimAndSetAction8_80C6524 ── */
/* battleObject_PlayHurtAnimAndSetAction8_80C6524(r5 = battle object):
 * Switch the object to animation 1, play sound effect 0xA2, and set its
 * current action to 8 (field at offset 9, same as the assembly's literal).
 * Exits with r0 = 8 and flags set from that value. */
Next battleObject_PlayHurtAnimAndSetAction8_80C6524(void) {
    ST8(R5 + oBattleObject_CurAnim, 1);

    R0 = 0xA2;
    GUEST_CALL(PlaySoundEffect);

    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 9, R0);

    RETURN_TO_CALLER();
}

/* ── battle_destroyObjectOnFrameFlag_80C6536 ── */
/* battle_destroyObjectOnFrameFlag_80C6536(r5 = battle object):
 * Query the object's sprite for its current frame parameters; if the frame
 * carries the 0x80 "self-destruct" flag, move the object into the destroyed
 * state. Flag outputs come from the AND (or the MOV on the destroy path),
 * with c/v left as sprite_getFrameParameters left them. */
Next battle_destroyObjectOnFrameFlag_80C6536(void) {
    GUEST_CALL(sprite_getFrameParameters);

    R1 = 0x80;
    R0 &= R1;
    SET_FLAGS_NZ(R0);

    if (R0 != 0) {
        R0 = 8; /* CUR_STATE_DESTROY */
        SET_FLAGS_NZ(R0);
        ST8(R5 + 8 /* oBattleObject_CurState */, R0);
    }

    RETURN_TO_CALLER();
}

/* ── battle_spawnTypeFObjectInitState_80C6548 ── */
/* battle_spawnTypeFObjectInitState_80C6548: spawn a battle object and fill in its starting state.
 *
 * The caller passes the new object's state in the registers:
 *   r0 = panel X, r1 = panel Y, r2 = element, r3 = extra spawn argument,
 *   r5 = object whose alliance byte the new object inherits,
 *   r6 = value for the damage/stamina counter disabler field.
 *
 * The registers are stashed, object_spawnType3 is called with r0 = 0xF
 * (r1-r3 still holding the caller's values), and it leaves the new object
 * pointer in r5 (NULL if the spawn failed). That pointer becomes the store
 * target for the field initialisation and is also what ends up in r0.
 * The stashed r0-r2 are restored shifted into r1-r3, r5 gets its entry
 * value back, and on the success path r1 is finally reloaded with the
 * source object's alliance byte on the way out. */
Next battle_spawnTypeFObjectInitState_80C6548(void) {
    u32 panelX    = R0;
    u32 panelY    = R1;
    u32 element   = R2;
    u32 srcObject = R5; /* alliance source; also what r5 is restored to */
    u32 damageSet = R6;

    R0 = 0xF;
    GUEST_CALL(object_spawnType3);

    u32 newObject = R5; /* spawn result as left by object_spawnType3 */

    R1 = panelX;
    R2 = panelY;
    R3 = element;
    R5 = srcObject;
    R0 = newObject;

    if (newObject != 0) {
        ST8(newObject + oBattleObject_PanelX, panelX);
        ST8(newObject + oBattleObject_PanelY, panelY);
        ST8(newObject + oBattleObject_Element, element);

        u32 alliance = LD8(srcObject + oBattleObject_Alliance);
        R1 = alliance;
        ST8(newObject + oBattleObject_Alliance, alliance);

        ST32(newObject + oBattleObject_DamageAndStaminaDamageCounterDisabler, damageSet);
    }

    RETURN_TO_CALLER();
}

/* ── battle_stepObjectXByVelocityAndDestroyIfInvalidPanel_80C6A08 ── */
/* battle_stepObjectXByVelocityAndDestroyIfInvalidPanel_80C6A08: step an object sideways by its X velocity and destroy it if the
 * panel it ends up on is not a valid standing spot.  r5 = battle object.
 * Leaves nz/c/v reflecting the panel-validity test (or the destroy state value). */
Next battle_stepObjectXByVelocityAndDestroyIfInvalidPanel_80C6A08(void) {
    /* PanelX is a byte, so the velocity add wraps to one tile dimension. */
    u32 newX = LD8(R5 + oBattleObject_PanelX) + LD32(R5 + oBattleObject_XVelocity);
    ST8(R5 + oBattleObject_PanelX, newX);

    GUEST_CALL(object_setCoordinatesFromPanels);
    GUEST_CALL(object_updateCollisionPanels);
    GUEST_CALL(object_isCurrentPanelValid);

    /* tst r0, r0: is the panel we now occupy walkable? */
    SET_FLAGS_NZ(R0);
    if (R0 == 0) {
        /* Invalid panel: drop the collision region and mark the object for
         * destruction, clearing its "active" flag bit (bit 1, at offset 0). */
        GUEST_CALL(object_clearCollisionRegion);

        R1 = 2;
        ST8(R5, LD8(R5) & ~R1);

        R0 = 8; /* CUR_STATE_DESTROY */
        SET_FLAGS_NZ(R0);
        ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
    }
    RETURN_TO_CALLER();
}

/* ── battle_object_destroy_80C6AAC ── */
/* battle_object_destroy_80C6AAC(r5 = battle object):
 * Destroy this battle object: clear its collision region, then switch it to
 * the "destroyed" state (CUR_STATE_DESTROY, stored into the packed
 * state/action/phase field at offset 8). Leaves NZ set as the MOV of the
 * state constant does, since callers branch on the state value. */

Next battle_object_destroy_80C6AAC(void) {
    GUEST_CALL(object_clearCollisionRegion);

    R0 = CUR_STATE_DESTROY;
    SET_FLAGS_NZ(R0);
    /* str r0, [r5,#oBattleObject_CurStateActionPhaseAndPhaseInitialized] */
    ST32(R5 + 8, R0);

    RETURN_TO_CALLER();
}

/* ── battle_refreshObjectCollisionFromExtraVars_80C6AB8 ── */
/* battle_refreshObjectCollisionFromExtraVars_80C6AB8(r5 = battle object):
 * Refresh the object's collision box from its extra-vars data, reapply its
 * panel element effect, and mark a collision status effect if requested.
 * r2 comes back from the element update; r4/r6/r7 pass through untouched. */
Next battle_refreshObjectCollisionFromExtraVars_80C6AB8(void) {
    u32 object = R5;

    /* Update the hitbox: extra-vars byte 1 selects the box, 4x5 its size.
     * (R0 is left as-is; the callee does not read it.) */
    u32 extraVars = LD32(object + oBattleObject_ExtraVars);
    u32 shapeId = LD8(extraVars + 1);
    R1 = 4;
    R2 = 5;
    R3 = shapeId;
    GUEST_CALL(object_updateCollisionData);

    /* Reapply the effect of the panel element this object stands on. */
    R0 = LD8(object + oBattleObject_Element);
    GUEST_CALL(sub_8019F8C);

    /* Extra-vars byte 3 non-zero: flag the collision status effect. */
    extraVars = LD32(object + oBattleObject_ExtraVars);
    u32 statusEffect = LD8(extraVars + 3);
    if (statusEffect != 0) {
        R0 = statusEffect;
        GUEST_CALL(object_setCollisionStatusEffect1);
    }
    RETURN_TO_CALLER();
}

/* ── t3_0x16_80C6B40 ── */
/* t3_0x16_80C6B40 — per-frame update for battle object type 3, id 0x16.
 *
 * Runs the handler selected by the object's current state (r5 = battle
 * object), then refreshes the object's sprite. The dispatch table at
 * off_80C6B58 is indexed by the raw state byte; because the table holds
 * 32-bit entries and there is no shift before the load, the state byte
 * stores word offsets:
 *   0 -> sub_80C6B64            (state handler 0)
 *   4 -> sub_80C6C14            (state handler 1)
 *   8 -> object_genericDestroy  (final state: remove the object)
 *
 * The handler is entered with r0 = state byte and r1 = its own table
 * entry; r2-r12 pass through untouched. The original calls it with
 * mov lr, pc / bx r1, so control comes back right after the bx — the
 * state handler returns here rather than tail-calling. Afterwards
 * object_updateSprite finishes the frame using whatever the handler
 * left in the argument registers.
 */
Next t3_0x16_80C6B40(void) {
    u32 state = LD8(R5 + 8 /* oBattleObject_CurState */);

    R0 = state;
    R1 = LD32(A_off_80C6B58 + state); /* handler address from the table */

    switch (state) {
    case 0:
        GUEST_CALL(sub_80C6B64);
        break;
    case 4:
        GUEST_CALL(sub_80C6C14);
        break;
    case 8:
        GUEST_CALL(object_genericDestroy);
        break;
    default:
        /* Unreachable in practice: the original would jump through a
         * garbage table entry here and crash. */
        break;
    }

    /* The state handler has returned; refresh the sprite. */
    GUEST_CALL(object_updateSprite);

    RETURN_TO_CALLER();
}

/* ── battleObject_dyingTickDespawn_80C6CBA ── */
/* Field offsets/state constants used here that game.h spells differently
 * (the assembler listing calls them oBattleObject_Param1 /
 * oBattleObject_CurStateActionPhaseAndPhaseInitialized / CUR_STATE_DESTROY):
 *   +0x04: Param1            (u8, per-object behaviour parameter)
 *   +0x08: current state word (CUR_STATE_DESTROY = 8 marks the object dead) */
#define oBattleObject_Param1 4
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized 8
#define CUR_STATE_DESTROY 8

/* battleObject_dyingTickDespawn_80C6CBA(r5 = battle object):
 * Death tick. Objects with Param1 < 12 vanish as soon as their sprite
 * animation reports the end-of-animation bit (0x80); objects with
 * Param1 >= 12 linger for Timer2 more ticks first. Once the trigger fires,
 * the object is parked offscreen (X = 0xF0), its collision region is
 * cleared, and it is marked destroyed. Flags: nz from the last test/mov,
 * c/v carried over from the getFrameParameters call or the timer subtract. */
Next battleObject_dyingTickDespawn_80C6CBA(void) {
    if (LD8(R5 + oBattleObject_Param1) < 0xC) {
        /* Ask the current sprite whether its animation has finished. */
        GUEST_CALL(sprite_getFrameParameters);
        R1 = 0x80;
        SET_FLAGS_NZ(R0 & R1);
        if ((R0 & 0x80) == 0)
            RETURN_TO_CALLER();          /* still animating, keep the object */
    } else {
        /* Linger phase: count Timer2 down to zero before vanishing. */
        u32 timer = LD16(R5 + oBattleObject_Timer2);
        SET_FLAGS_CMP(timer, 1);
        R0 = timer - 1;
        ST16(R5 + oBattleObject_Timer2, R0);
        if (R0 != 0)
            RETURN_TO_CALLER();          /* time not up yet */
    }

    /* Vanish: park offscreen, drop the hitbox, mark the object destroyed. */
    ST16(R5 + oBattleObject_X16, 0xF0);
    GUEST_CALL(object_clearCollisionRegion);
    R0 = CUR_STATE_DESTROY;
    SET_FLAGS_NZ(R0);
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, R0);
    RETURN_TO_CALLER();
}
