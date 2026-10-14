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
 * chosen handler unchanged; its register results are the caller's results.
 * The original compares the whole of r2 (`cmp r2, #0xff`), so 0x103 sets the type. */
Next battle_objectDispatchPanelChange_80C6CFC(void) {
    switch (R2) {
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

/* ── t3_0x1f_80C8098 ── */
/* t3_0x1f_80C8098 -- battle object task dispatcher.
 *
 * Reads the object's current phase (oBattleObject_CurState) and calls the
 * matching handler from the jump table at off_80C80AC:
 *   0 -> battleObject_initActionPhase_80C80B8  (begin the object's action phase)
 *   1 -> sub_80C8100
 *   2 -> object_genericDestroy                 (remove the object)
 * The handler is entered with r0 = current state and r1 = its own table
 * entry (the original loads both right before `mov lr, pc; bx r1`); every
 * other register, including the object pointer in r5, passes through
 * untouched.  The original tail-calls the handler, so whatever the handler
 * leaves in r5/r8-r12 is this function's result; here the handler returns
 * into our helper and the adapter returns straight to the caller, which
 * touches nothing in between and leaves those registers exactly as the
 * handler set them. */

/* register form: the handler address is read at runtime from the object's
 * state table, so the call has to go through a guest code address with
 * r0/r1 set by hand -- C cannot express either. */
static void call_object_state_handler(u32 handler, u32 state) {
    R0 = state;      /* handlers take the current state in r0 */
    R1 = handler;    /* the original happens to leave the table entry in r1 */
    GUEST_CALL_ADDR(handler);
}

/* Run the handler selected by the object's current state.  No result value:
 * the handler's effects are the guest register/memory state it leaves
 * behind (r5, r8-r12 and its memory writes), which the guest call has
 * already put in place. */
static void dispatch_object_state_t3_0x1f_80C8098(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80C80AC + state);
    call_object_state_handler(handler, state);
}

Next t3_0x1f_80C8098(void) {
    dispatch_object_state_t3_0x1f_80C8098(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_initActionPhase_80C80B8 ── */
/* battleObject_initActionPhase_80C80B8: Initialize a battle object on entry to its action phase.
 * Sets Z to 0, builds collision data, plays spawn VFX/sfx, and marks
 * the state action phase as CUR_STATE_UPDATE. inputs: r5 r8 r10 lr
 * outputs: r5 r8 r10  clobbers: nz c v r0 r1 r2 r3 lr  preserved: r4 */
Next battleObject_initActionPhase_80C80B8(void) {
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

/* ── sub_80C8100 ── */
/* sub_80C8100(r5 = battle object): per-frame collision update for a battle
 * object. Refreshes the object's collision data, then either tears the object
 * down (battle over, or the collision data reports a hit) or runs its current
 * action handler and publishes the resulting collision data.
 *
 * Outputs: r5 r8 r9 r10 r11 r12 pass through unchanged. */
Next sub_80C8100(void) {
    u32 self = R5;

    /* Drop last frame's collision data, then spawn the impact effect. */
    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);

    GUEST_CALL(battle_isBattleOver);
    u32 battleOver = R0;

    /* A pending hit flag in the collision data also tears the object down.
     * (Only inspected while the battle is still running.) */
    u32 collisionFlags = 0;
    if (battleOver == 0) {
        R1 = LD32(self + oBattleObject_CollisionDataPtr);
        R0 = LD32(R1 + oCollisionData_FlagsFromCollision);
        collisionFlags = R0;
    }

    if (battleOver != 0 || collisionFlags != 0) {
        /* No usable collision state left: clear the collision region and
         * advance the object to the destroyed state. */
        GUEST_CALL(object_clearCollisionRegion);
        ST32(self + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
             CUR_STATE_DESTROY);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isTimeStop);
    if (!ZERO_FLAG()) /* time is stopped: leave the collision data untouched */
        RETURN_TO_CALLER();

    /* Run the action handler for the object's current action. The handler
     * table off_80C8140 is indexed by oBattleObject_CurAction and holds the
     * single entry sub_80C8144; the handler returns straight into the
     * presentCollisionData tail below. */
    u32 action = LD8(self + oBattleObject_CurAction);
    u32 handler = LD32(A_off_80C8140 + action);
    R0 = action;
    R1 = handler;
    GUEST_CALL(sub_80C8144);

    /* Publish the collision data the handler produced. */
    GUEST_CALL(object_presentCollisionData);

    RETURN_TO_CALLER();
}

/* ── sub_80C8144 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFrontDirection_result_sub_80C8144;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_sub_80C8144 call_object_getFrontDirection_sub_80C8144(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_sub_80C8144){ R0 };
}

typedef struct { u32 r0; bool c, v; } call_object_isValidPanel_result;
/* register form: generated wrapper: object_isValidPanel has no typed C interface yet */
static inline call_object_isValidPanel_result call_object_isValidPanel(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_isValidPanel);
    return (call_object_isValidPanel_result){ R0, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r6; } call_object_updateCollisionPanels_result;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result call_object_updateCollisionPanels(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result){ R6 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7; } call_spawn_t1_0x0_EffectObject_result;
/* register form: generated wrapper: spawn_t1_0x0_EffectObject has no typed C interface yet */
static inline call_spawn_t1_0x0_EffectObject_result call_spawn_t1_0x0_EffectObject(u32 r1, u32 r2, u32 r3, u32 r4) {
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
    return (call_spawn_t1_0x0_EffectObject_result){ R1, R2, R3, R4, R5, R6, R7 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result call_PlaySoundEffect(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result){ R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_object_highlightPanel_result;
/* register form: generated wrapper: object_highlightPanel has no typed C interface yet */
static inline call_object_highlightPanel_result call_object_highlightPanel(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_highlightPanel);
    return (call_object_highlightPanel_result){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result call_object_clearCollisionRegion(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result){ R1 };
}

/* Battle object "advance one panel" handler.
 *
 * On the first call of a phase (CurPhase == 0) it latches CurPhase = 4 and
 * seeds the countdown timer from Param2. Every call then decrements the timer:
 *  - while it still counts down, the object's current panel stays highlighted;
 *  - when it reaches zero the object steps one panel in its facing direction.
 *    If the destination panel is off the field, the collision region is
 *    cleared and the object switches to the destroy state. Otherwise it
 *    commits its world position and collision panels, restarts the countdown,
 *    spawns a step dust effect, plays the step sound and highlights the panel
 *    it landed on.
 *
 * Register contract: inputs r5 (object) lr; outputs r1 r5 r10; preserved
 * r4 r5 r6 r7 r10.
 */

struct StepResult {
    u32 r1;   /* value the last panel call leaves behind in r1 */
    u32 r10;  /* r10 pass-through (as PlaySoundEffect leaves it) */
};

static struct StepResult step_object_forward(u32 obj, u32 r10_in) {
    struct StepResult res;
    res.r10 = r10_in;

    /* register form: this handler must preserve the incoming r4 (the original
     * push/pop {r4,lr} restores it on return), but the callees on the step
     * path clobber the guest r4 register (spawn_t1_0x0_EffectObject leaves
     * its scratch value there). The incoming r4 is not one of this function's
     * inputs and exists only in the register file, so typed C cannot preserve
     * it across the wrapper calls without touching the register directly. */
    u32 saved_r4 = R4;

    /* First tick of the phase: latch phase 4 and seed the countdown. */
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        ST8(obj + oBattleObject_CurPhase, 4);
        ST16(obj + oBattleObject_Timer, LD8(obj + oBattleObject_Param2));
    }

    /* Count down one tick; the decremented value is what the next call sees. */
    u32 timer = LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, timer);

    if ((s32)timer > 0) {
        /* Still counting down: just keep the current panel highlighted. */
        res.r1 = call_object_highlightPanel(LD8(obj + oBattleObject_PanelX),
                                            LD8(obj + oBattleObject_PanelY)).r1;
    } else {
        /* Timer expired: move one panel in the object's facing direction. */
        u32 step_x = call_object_getFrontDirection_sub_80C8144(obj).r0;
        u32 new_x = step_x + LD8(obj + oBattleObject_PanelX);
        ST8(obj + oBattleObject_PanelX, new_x);

        if (call_object_isValidPanel(new_x, LD8(obj + oBattleObject_PanelY)).r0 == 0) {
            /* Walked off the field: drop the collision region, self-destruct. */
            res.r1 = call_object_clearCollisionRegion(obj).r1;
            ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
                 CUR_STATE_DESTROY);
        } else {
            /* Landed on the field: commit world position and collision
             * panels, restart the countdown, spawn the step effect and play
             * its sound. */
            call_object_setCoordinatesFromPanels(obj);
            call_object_updateCollisionPanels(obj);
            ST16(obj + oBattleObject_Timer, LD8(obj + oBattleObject_Param2));

            call_spawn_t1_0x0_EffectObject_result fx = call_spawn_t1_0x0_EffectObject(
                LD32(obj + oBattleObject_X),
                LD32(obj + oBattleObject_Y),
                0x20u << 16,   /* effect layer/priority word (0x00200000) */
                0);

            call_PlaySoundEffect_result sfx =
                call_PlaySoundEffect(0x106, fx.r2);   /* 7 + 0xFF: step sound */
            res.r10 = sfx.r10;

            /* Highlight the panel the object just landed on. */
            res.r1 = call_object_highlightPanel(LD8(obj + oBattleObject_PanelX),
                                                LD8(obj + oBattleObject_PanelY)).r1;
        }
    }

    /* Restore the caller's r4, as the original's pop {r4,pc} does. */
    R4 = saved_r4;
    return res;
}

Next sub_80C8144(void) {
    struct StepResult res = step_object_forward(R5, R10);
    R1 = res.r1;
    R10 = res.r10;
    RETURN_TO_CALLER();
}

/* ── sub_80C81A6 ── */
/* sub_80C81A6(r0 = panel X, r1 = panel Y, r2 = element, r5 = source object,
 *             r6 = damage / stamina-damage counter value):
 *
 * Spawns a battle object of spawn type 0x1F via object_spawnType3, then
 * initializes it from the caller's context: the panel position, element and
 * damage counter passed in as arguments, plus the alliance/direction-flip
 * halfword copied from the source object in r5. Finally sets header flag
 * 0x10 on the new object (marking it as set up / active).
 *
 * If no object could be spawned, nothing is written. Either way the caller's
 * r5 (and r7) pass through unchanged, and r1 is 0x10 on success or the
 * original r0 argument on failure. */
Next sub_80C81A6(void) {
    u32 panelX = R0;
    u32 panelY = R1;
    u32 element = R2;
    u32 source = R5;

    R0 = 0x1F;
    GUEST_CALL(object_spawnType3);
    u32 spawned = R5;

    if (spawned != 0) {
        ST8(spawned + oBattleObject_PanelX, panelX);
        ST8(spawned + oBattleObject_PanelY, panelY);
        ST8(spawned + oBattleObject_Element, element);
        ST32(spawned + oBattleObject_DamageAndStaminaDamageCounterDisabler, R6);

        u32 alliance = LD16(source + oBattleObject_AllianceAndDirectionFlip);
        ST16(spawned + oBattleObject_AllianceAndDirectionFlip, alliance);

        ST8(spawned + oObjectHeader_Flags,
            LD8(spawned + oObjectHeader_Flags) | 0x10);
        R1 = 0x10;
    } else {
        R1 = panelX;
    }

    R5 = source;
    RETURN_TO_CALLER();
}

/* ── t3_0x20_80C81CC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result call_object_updateSpritePaused(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t3_0x20_80C81CC: per-frame update for this battle object. The object's
 * current state byte selects a handler through the jump table at
 * off_80C81E4 (the original indexes it with the raw state byte), the
 * handler runs with r5 = object pointer and r0 = state, and afterwards the
 * object's sprite is refreshed with object_updateSpritePaused. */

/* The state handler is reached through the object's jump table, so it has
 * to be called through its code address instead of a named wrapper. It is
 * entered exactly as the original enters it: r0 = state byte, r1 = its own
 * table entry, r5 = object pointer; r10 and the other argument registers
 * are already in place from the caller. */
static u32 call_cur_state_handler(u32 handler, u32 state, u32 object) { /* register form: calls through a guest code address (noted by the tool) */
    R0 = state;
    R1 = handler;
    R5 = object;
    GUEST_CALL_ADDR(handler);
    return R0;
}

static void t3_update_object(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80C81E4 + state);

    call_cur_state_handler(handler, state, object);

    /* register form: the handler returns with the (possibly updated) object
     * pointer in r5, and object_updateSpritePaused must be called with that
     * value; after the raw call it exists only in the guest register. */
    call_object_updateSpritePaused(R5);
}

/* r5/r8-r12 outputs are whatever the state handler and the sprite update
 * leave behind, so the adapter only passes the object pointer in. */
Next t3_0x20_80C81CC(void) {
    t3_update_object(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_initSpriteAndEnterUpdate_80C81F0 ── */
/* battleObject_initSpriteAndEnterUpdate_80C81F0: initialize the battle object's sprite from panel coordinates,
 * set its Z, load sprites/animations, configure flip and apply update phase.
 * Inputs:  r5 = battle object pointer, lr
 * Outputs: r5 (preserved)
 * Clobbers: r0, r1, r2, r3, lr, flags */
Next battleObject_initSpriteAndEnterUpdate_80C81F0(void) {
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

/* ── sub_80C8232 ── */
/* sub_80C8232 (r5 = battle object, r10 = context):
 * Per-frame update for a battle object that follows another object's lead.
 * If the battle is over, or the related object has moved on from action 0xB,
 * the object is torn down (state = CUR_STATE_DESTROY). While time is stopped
 * nothing happens; otherwise the handler for the object's own current action
 * is run from its dispatch table. */
static void battle_object_begin_destroy(u32 obj) {
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
}

Next sub_80C8232(void) {
    GUEST_CALL(battle_isBattleOver);
    if (R0 != 0) {
        battle_object_begin_destroy(R5);
        RETURN_TO_CALLER();
    }

    u32 related = LD32(R5 + oBattleObject_RelatedObject1Ptr);
    if (LD8(related + oBattleObject_CurAction) != 0xB) {
        battle_object_begin_destroy(R5);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isTimeStop);
    if (!ZERO_FLAG()) /* time stop active: idle this frame */
        RETURN_TO_CALLER();

    /* Dispatch the current action's handler through the table (its single
     * entry is battleObject_runPhase4AndDestroy_80C8268); r1 still holds the table entry, as in the original. */
    R0 = LD8(R5 + oBattleObject_CurAction);
    R1 = LD32(A_off_80C8264 + R0);
    GUEST_CALL(battleObject_runPhase4AndDestroy_80C8268);
    RETURN_TO_CALLER();
}

/* ── battleObject_runPhase4AndDestroy_80C8268 ── */
/* battleObject_runPhase4AndDestroy_80C8268: Battle object phase-4 state — seed timer once from Param2,
 * tick it each frame, dispatch Param3 while it lasts, then destroy. */
Next battleObject_runPhase4AndDestroy_80C8268(void) {
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
        GUEST_CALL(battleObject_highlightFrontPanels_80C8310);
    } else {
        /* Timer expired: move to the destroy state. */
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80C829C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battle_runObjects_80C8368 has no typed C interface yet */
static inline void call_battle_runObjects_80C8368(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_runObjects_80C8368);
}

typedef struct { u32 r0, r1, r2, r3, r5, r6, r7; bool z, n, c, v; } call_object_genericDestroy_result;
/* register form: generated wrapper: object_genericDestroy has no typed C interface yet */
static inline call_object_genericDestroy_result call_object_genericDestroy(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_genericDestroy);
    return (call_object_genericDestroy_result){ R0, R1, R2, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_80C829C: run one frame of battle object updates for the object in r5,
 * then destroy it. r5 goes out as object_genericDestroy leaves it. */

static u32 sub_80C829C_body(u32 object) {
    call_battle_runObjects_80C8368(object);
    return call_object_genericDestroy(object).r5;
}

Next sub_80C829C(void) {
    R5 = sub_80C829C_body(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C82A8 ── */
/* sub_80C82A8(r0 = panel X, r1 = panel Y, r2 = element, r5 = related object,
 *             r6 = damage/stamina counter disabler value):
 * Spawns a battle object of spawn type 0x20. If one was allocated, the new
 * object (returned by object_spawnType3 in r5, moved to r0) is initialised
 * from the caller's arguments: panel position and element, the related
 * object pointer and counter-disabler value, the alliance/direction-flip
 * halfword copied from the related object, and header flag 0x10 ("has
 * related object"). The spawn result is left in r0 and the flags reflect it
 * (the original's "mov r0, r5" is an LSLS, which sets N/Z); r5 is restored
 * to the caller's value. The original's push/pop shuffle means that on the
 * failure path r1/r2/r3 end as the caller's r0/r1/r2, and on the success
 * path r2 ends as the new object's header flags with bit 4 set. */
Next sub_80C82A8(void) {
    u32 panelX = R0;
    u32 panelY = R1;
    u32 element = R2;
    u32 relatedObject = R5;
    u32 counterDisabler = R6;

    R0 = 0x20;
    GUEST_CALL(object_spawnType3);
    u32 object = R5;

    /* pop {r1-r3,r5}: the caller's r0/r1/r2 land in r1/r2/r3 */
    R0 = object;
    R1 = panelX;
    R2 = panelY;
    R3 = element;
    R5 = relatedObject;
    SET_FLAGS_NZ(object);   /* "mov r0, r5" (lsls r0, r5, #0): N/Z from result */

    if (object != 0) {
        ST8(object + oBattleObject_PanelX, panelX);
        ST8(object + oBattleObject_PanelY, panelY);
        ST8(object + oBattleObject_Element, element);
        ST32(object + oBattleObject_RelatedObject1Ptr, relatedObject);
        ST32(object + oBattleObject_DamageAndStaminaDamageCounterDisabler, counterDisabler);
        u32 alliance = LD16(relatedObject + oBattleObject_AllianceAndDirectionFlip);
        ST16(object + oBattleObject_AllianceAndDirectionFlip, alliance);
        R3 = alliance;
        u32 flags = LD8(object + oObjectHeader_Flags) | 0x10;
        ST8(object + oObjectHeader_Flags, flags);
        R2 = flags;
        R1 = 0x10;
    }

    RETURN_TO_CALLER();
}

/* ── sub_80C82D0 ── */
/* sub_80C82D0(r5 = battle object):
 * Steps outward from the object's own panel, one panel at a time in the
 * direction the object is facing, and invokes sub_80CC9FC once per panel
 * for the first Param3 panels ahead of it. r5 (the object pointer) comes
 * back unchanged; r4/r6/r7 are preserved across the whole helper. */
Next sub_80C82D0(void) {
    u32 object = R5;

    /* The original saves r4/r6/r7 on entry and restores them on exit,
     * so the caller's values survive even though the loop body clobbers
     * them while setting up each sub_80CC9FC call. */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 step = 1;
    do {
        /* Ask the object which way "ahead" is (r4 tells it how many
         * panels out we currently are). */
        R4 = step;
        R5 = object;
        GUEST_CALL(object_getFrontDirection);

        /* Target panel = own panel + front direction * step. */
        R0 = R0 * step + LD8(object + oBattleObject_PanelX);
        R1 = LD8(object + oBattleObject_PanelY);
        R2 = LD8(object + oBattleObject_Element);
        R6 = LD32(object + oBattleObject_DamageAndStaminaDamageCounterDisabler);
        /* Packed effect parameters: kind 3, Param2, animation 0xB. */
        R7 = 3 | (LD8(object + oBattleObject_Param2) << 16) | (0xBu << 24);
        R4 = 0x4050101;
        R5 = LD32(object + oBattleObject_RelatedObject1Ptr);

        GUEST_CALL(sub_80CC9FC);

        step++;
    } while ((s32)step <= (s32)LD8(object + oBattleObject_Param3));

    R4 = savedR4;
    R5 = object;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battleObject_highlightFrontPanels_80C8310 ── */
/* battleObject_highlightFrontPanels_80C8310(r0 = panel_count, r5 = battle_object):
 *   Highlight the front-facing panels of a battle object, one per step.
 *   Starting from the panel directly in front of the object, highlight
 *   `panel_count` consecutive panels in that direction.
 */
Next battleObject_highlightFrontPanels_80C8310(void) {
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

/* ── sub_80C832E ── */
/* Target list inside a battle object: one 4-byte entry per panel distance
 * away from the object, filled in by sub_80E1B1E. */
#define oBattleObject_TargetList 0x60

/* sub_80C832E(r0 = number of panels to scan, r3 = ?, r5 = battle object):
 * Starting one panel away from the object's own panel, walk up to `count`
 * panels in the direction object_getFrontDirection reports. Every panel that
 * is still on the field is handed to sub_80E1B1E, which records it in the
 * object's target list at obj+0x60+(distance-1)*4. The walk stops at the
 * first panel off the field. r3 and r5 come out unchanged; r0-r2, lr and the
 * flags are scratch. */
Next sub_80C832E(void) {
    u32 count = R0;
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;

    u32 distance = 1;
    do {
        /* Which way the object is facing; used as the per-step displacement. */
        GUEST_CALL(object_getFrontDirection);

        u32 x = R0 * distance + (u32)LD8(R5 + oBattleObject_PanelX);
        u32 y = (u32)LD8(R5 + oBattleObject_PanelY);

        R0 = x;
        R1 = y;
        GUEST_CALL(object_isValidPanel);
        if (R0 == 0)
            break; /* walked off the field: stop scanning */

        /* Record this panel as target entry (distance-1). r4/r6 are still
         * live registers at this call site, so hand them over as they are. */
        u32 slot = (distance - 1) * 4;
        R0 = x;
        R1 = y;
        R2 = slot;
        R4 = distance;
        R6 = count;
        R7 = R5 + oBattleObject_TargetList + slot;
        GUEST_CALL(sub_80E1B1E);
        R7 = savedR7; /* restore r7 across the call, as the original does */

        distance++;
    } while ((s32)distance <= (s32)count);

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_runObjects_80C8368 ── */
/* battle_runObjects_80C8368(r5 = array of object pointers at base+0x60 stride 4, count 5):
 * For each of the 5 slots, if the pointer is non-NULL, dispatch sub_80E1B3C on it.
 * Used to update/draw the 5 battle-object slots attached to r5. */
Next battle_runObjects_80C8368(void) {
    for (u32 slot = 0; slot < 5; slot++) {
        u32 ptr = LD32(R5 + (slot * 4 + 0x60));
        if (ptr != 0) {
            R0 = ptr;
            GUEST_CALL(sub_80E1B3C);
        }
    }
    RETURN_TO_CALLER();
}

/* ── t3_0x28_80C8FFC ── */
/* t3_0x28_80C8FFC: update step for the type-0x28 battle object.
 *
 * Reads the object's current state and runs the matching state handler.
 * The state byte is stored pre-multiplied by 4, so it indexes the handler
 * table at off_80C9014 (one word per state) directly:
 *   state 0  -> sub_80C9024
 *   state 4  -> sub_80C9074
 *   state 8  -> sub_80C90DC
 *   state 12 -> object_genericDestroy
 * Afterwards it refreshes the object's sprite, exactly as the original's
 * "mov lr, pc / bx r1" tail into `bl object_updateSprite` does.
 */
Next t3_0x28_80C8FFC(void) {
    u32 state = LD8(R5 + oBattleObject_CurState);

    R0 = state;                 /* handlers receive the state in r0 */
    switch (state) {
    case 0:
        GUEST_CALL(sub_80C9024);
        break;
    case 4:
        GUEST_CALL(sub_80C9074);
        break;
    case 8:
        GUEST_CALL(sub_80C90DC);
        break;
    case 12:
        GUEST_CALL(object_genericDestroy);
        break;
    }

    /* the handler's r0 (and the rest of the register file) flows straight
     * into object_updateSprite, as in the original fall-through */
    GUEST_CALL(object_updateSprite);

    RETURN_TO_CALLER();
}

/* ── sub_80C9024 ── */
/* sub_80C9024: initialize a battle object sprite, collision data and entry state.
 * Inputs:  r5 = battle object ptr
 * Outputs: r5, r8 unchanged
 */
Next sub_80C9024(void) {
    /* Load sprite 0x80 (slot 0x14), 5 frames of animation. */
    R0 = 0x80;
    R1 = 0x14;
    R2 = 5;
    GUEST_CALL(sprite_load);

    /* Set current animation to 0x0B and apply it. */
    R0 = 0xB;
    ST8(R5 + oBattleObject_CurAnim, R0);
    GUEST_CALL(sprite_setAnimation);

    /* Load the animation frame data, drop any shadow, then update the sprite. */
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    GUEST_CALL(sprite_update);

    /* Snap the sprite to its panel coordinates and store its facing as X velocity. */
    GUEST_CALL(object_setCoordinatesFromPanels);
    GUEST_CALL(object_getFrontDirection);
    ST32(R5 + oBattleObject_XVelocity, R0);

    /* Element 5 (wind); allocate collision data for the object. */
    R0 = 5;
    ST8(R5 + oBattleObject_Element, R0);
    GUEST_CALL(object_createCollisionData);

    /* If allocation failed, tear the object down and bail out. */
    SET_FLAGS_NZ(R0);
    if (R0 == 0) {
        GUEST_CALL(object_freeMemory);
        RETURN_TO_CALLER();
    }

    /* Configure collision (shape 4, size 5, no extra param) and present it. */
    R1 = 4;
    R2 = 5;
    R3 = 0;
    GUEST_CALL(object_setupCollisionData);
    GUEST_CALL(object_presentCollisionData);

    /* Mark the object as ready for its update phase. */
    R0 = CUR_STATE_UPDATE;
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, R0);
    RETURN_TO_CALLER();
}

/* ── sub_80C90DC ── */
/* sub_80C90DC(r5 = self):
 * Mirror the related object's motion: copy bit-1 of oObjectHeader_Flags from it,
 * then if checkBattleOrderMatch_802CE30 permits, look up an indexed (dx,dy) by name, flip the sign
 * to our facing direction, add it into our 0x34..0x40 slot (loaded from the
 * related object's same slot) and tick the timer. When the timer reaches zero
 * (or the gate is closed) clear bit-1, set state=0xC, and let battleObject_DetachFromAllianceSlot_802CDE6
 * clean up. */
Next sub_80C90DC(void) {
    u32 other = LD32(R5 + oBattleObject_RelatedObject1Ptr);

    /* Copy bit 1 of the related object's flags into our own flags. */
    u32 other_flags = LD8(other + oObjectHeader_Flags);
    u32 self_flags  = LD8(R5 + oObjectHeader_Flags);
    self_flags = (self_flags & ~(u32)2) | (other_flags & (u32)2);
    ST8(R5 + oObjectHeader_Flags, self_flags);

    GUEST_CALL(checkBattleOrderMatch_802CE30);
    if (ZERO_FLAG()) goto expire;

    /* Look up the related object's name index, then fetch a (dx,dy) pair from
     * the motion table at index 0x1B. */
    R0 = LD16(other + oBattleObject_NameID);
    GUEST_CALL(enemy_getArmPosition_800F26C);
    R2 = 0x1B;
    GUEST_CALL(getIndexedSignedBytePair_8018842);

    /* Sign-extend the two bytes to 32 bits and shift to Q16 so we can scale by
     * the facing direction without losing precision. */
    s32 sx = (s32)(s8)R0 << 16;
    s32 sy = (s32)(s8)R1 << 16;

    /* Multiply the x component by -dir (so the related object's velocity is
     * mirrored onto us). y is added directly without scaling. */
    GUEST_CALL(object_getFrontDirection);
    s32 dir = -((s32)R0);
    sx = sx * dir;

    /* Load (x, mid, z) from the related object's 0x34 slot, add the scaled
     * offsets to x and z, then write all three words into our 0x34 slot. */
    u32 src = other + 0x34;
    u32 x = LD32(src);
    u32 m = LD32(src + 4);
    u32 z = LD32(src + 8);
    x = x + sx;
    z = z + sy;

    u32 dst = R5 + 0x34;
    ST32(dst,     x);
    ST32(dst + 4, m);
    ST32(dst + 8, z);

    /* Tick the per-frame timer; bail out (preserving the flags from SUBS) if
     * it has not reached zero. */
    {
        u32 t = LD16(R5 + oBattleObject_Timer);
        t -= 1;
        ST16(R5 + oBattleObject_Timer, t);
        SET_FLAGS_NZ(t);
    }
    if (!ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

expire:
    GUEST_CALL(battleObject_DetachFromAllianceSlot_802CDE6);

    {
        u32 f = LD8(R5 + oObjectHeader_Flags);
        f = f & ~(u32)2;
        ST8(R5 + oObjectHeader_Flags, f);
    }
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, 0xC);

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

/* ── sub_80C9184 ── */
/* sub_80C9184(r0 = panel X, r1 = panel Y, r5 = source object, r6 = damage/
 * stamina-damage counter disabler): allocate a battle object of size 0x28 via
 * sub_8003374 (which delivers the new object, or 0, in r5) and, if the
 * allocation succeeded, initialise it with the given panel position and
 * damage value, copying the alliance/direction-flip halfword from the source
 * object. The object pointer and the source pointer are preserved across the
 * allocator call on the stack.
 * Outputs: r0 = new object pointer, r5 = source object (unchanged),
 * r1 = the copied alliance/flip halfword (or the incoming r0 if the pool was
 * full and nothing was copied). Flags reflect the new object pointer. */
Next sub_80C9184(void) {
    u32 panelX = R0;
    u32 panelY = R1;
    u32 source = R5;
    u32 damage = R6;

    R0 = 0x28;                      /* object size for the allocator */
    GUEST_CALL(sub_8003374);        /* allocated object (or 0) comes back in r5 */
    u32 newObj = R5;
    R0 = newObj;

    if (newObj != 0) {
        ST8(newObj + oBattleObject_PanelX, panelX);
        ST8(newObj + oBattleObject_PanelY, panelY);
        ST32(newObj + oBattleObject_DamageAndStaminaDamageCounterDisabler, damage);
        u32 alliance = LD16(source + oBattleObject_AllianceAndDirectionFlip);
        ST16(newObj + oBattleObject_AllianceAndDirectionFlip, alliance);
        R1 = alliance;              /* the load into r1 is the original's last write to it */
    } else {
        R1 = panelX;                /* pop {r1,r2,r5} restores r1 = incoming r0 here */
    }

    R5 = source;
    SET_FLAGS_NZ(newObj);           /* from "mov r0, r5" (adds), tested by the branch */
    RETURN_TO_CALLER();
}

/* ── battle_BDT_main_80c93cc ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result call_object_updateSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_BDT_main_80c93cc — one step of the "battle BDT" object's state machine.
 *
 * The original reads oBattleObject_CurState from the battle object in r5, picks
 * a handler (init / update / destroy) from the table right after the function
 * body, calls it through the table with `mov lr, pc; bx r1` (r0 = state byte,
 * r1 = handler address), and then calls object_updateSprite with the object
 * still in r5 to refresh its sprites. */

/* register form: the state handler is reached through the state table with
 * `mov lr, pc; bx r1`, which no generated wrapper covers. The helper performs
 * that one indirect call, entering it exactly as the original does:
 * r0 = state byte, r1 = handler address from the table. */
static void call_battle_BDT_state_handler(u32 state, u32 handler) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

/* Typed body: r5 holds the battle object pointer on entry and is handed to
 * object_updateSprite afterwards (the state handler leaves it intact).
 * r5 as the original leaves it is the output of object_updateSprite, which
 * the generated wrapper already leaves in place. */
static void battle_BDT_main_step(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80C93E4 + state);

    call_battle_BDT_state_handler(state, handler);
    call_object_updateSprite(object);
}

/* battle_BDT_main_80c93cc(r5 = battle object) -> r5 r8 r9 r10 r11 r12 */
Next battle_BDT_main_80c93cc(void) {
    battle_BDT_main_step(R5);
    RETURN_TO_CALLER();
}

/* ── battle_BDT_destroy_80c93f0 ── */
/* battle_BDT_destroy_80c93f0 (r5 = battle object): destroy a battle BDT object.
 * If its sprite frame parameters have the destroy bit (0x80) set, clear the
 * related object's header (if any) and free the object; otherwise do nothing.
 * Leaves r5 untouched for the caller. */
Next battle_BDT_destroy_80c93f0(void) {
    GUEST_CALL(sprite_getFrameParameters);

    if (R0 & 0x80) {
        u32 related = LD32(R5 + oBattleObject_RelatedObject1Ptr);
        if (related != 0)
            ST32(related + oBattleObject_ObjectHeader, 0);

        /* object_genericDestroy is called with r0 = related-object pointer
         * (or 0x80 if there was none) and r1 = 0 (or 0x80), exactly as the
         * original leaves them before the call. */
        R0 = related;
        R1 = (related != 0) ? 0 : 0x80;
        GUEST_CALL(object_genericDestroy);
    }
    RETURN_TO_CALLER();
}

/* ── battle_BDT_update_80c94bc ── */
/* battle_BDT_update_80c94bc(r5 = battle object):
 * Per-frame update for a battle object's collision state. Refreshes the
 * object's collision data, spawns the matching collision effect, then runs
 * the current action's handler (jump table at off_80C9504, indexed by
 * CurAction; the handler receives the collision data pointer in r7).
 * If the battle is over, the collision flags report a hit, or the panel
 * under the object is no longer valid, the collision region is cleared and
 * the object moves to the destroy state; otherwise its collision panels are
 * presented and highlighted.
 * Note: r4 and r7 are saved/restored around the whole routine, so both come
 * out unchanged even though r7 is used to pass the collision data pointer. */
Next battle_BDT_update_80c94bc(void) {
    u32 self = R5;
    u32 savedR4 = R4;
    u32 savedR7 = R7;

    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);

    GUEST_CALL(battle_isBattleOver);
    if (R0 != 0) /* battle is over */
        goto teardown;

    {
        u32 collision = LD32(self + oBattleObject_CollisionDataPtr);
        u32 hitFlags = LD32(collision + oCollisionData_FlagsFromCollision);

        /* 0xFF800000: flag bits that mean "still colliding" and thus
         * skip straight to teardown. */
        if ((hitFlags & 0xFF800000u) != 0)
            goto teardown;

        {
            /* Dispatch the handler for the current action. The handler is
             * looked up by CurAction and gets the collision data in r7. */
            u32 action = LD8(self + oBattleObject_CurAction);

            R7 = collision;
            R0 = action;
            R1 = LD32(A_off_80C9504 + action);
            GUEST_CALL(battle_BDT_update2_80c950c);

            GUEST_CALL(object_isCurrentPanelValid);
            if (R0 == 0) /* panel under the object is no longer valid */
                goto teardown;

            GUEST_CALL(object_presentCollisionData);
            GUEST_CALL(object_highlightCurrentCollisionPanels);

            R4 = savedR4;
            R7 = savedR7;
            RETURN_TO_CALLER();
        }
    }

teardown:
    GUEST_CALL(object_clearCollisionRegion);
    ST32(self + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
         CUR_STATE_DESTROY);

    R4 = savedR4;
    R7 = savedR7;
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

/* ── spawnThunderObjectMaybe_80c954c ── */
/* spawnThunderObjectMaybe_80c954c(r0 = panel X, r1 = panel Y, r2 = element,
 *                                 r5 = object to copy alliance/direction from,
 *                                 r6 = damage value, r7 = related object pointer):
 * Spawns a type-0x2A battle object and initialises its panel position,
 * element, damage, alliance/direction (copied from the object in r5) and
 * related-object pointer.
 *
 * On return: r0 = new object pointer (0 if the spawn failed), r1/r2 = the
 * caller's original r0/r1, r3 = the copied alliance/direction halfword (or
 * the incoming element argument when the spawn failed), r5 = the original
 * source object, n/z/c/v reflect "adds r0, r5, #0" on the spawned pointer. */
Next spawnThunderObjectMaybe_80c954c(void) {
    u32 panelX        = R0;
    u32 panelY        = R1;
    u32 element       = R2;
    u32 sourceObject  = R5;
    u32 damage        = R6;
    u32 relatedObject = R7;

    /* Spawn the battle object; the new object pointer comes back in r5. */
    R0 = 0x2A;
    GUEST_CALL(object_spawnType3);
    u32 spawned = R5;

    /* mov r0, r5 (modeled as adds r0, r5, #0): r0 = new object, flags follow. */
    R0 = spawned;
    SET_FLAGS_ADD(spawned, 0);

    /* pop {r1-r3,r5}: the caller's original r0/r1/r2 come back in r1/r2/r3. */
    R1 = panelX;
    R2 = panelY;
    R3 = element;
    R5 = sourceObject;

    if (spawned != 0) {
        ST8(spawned + oBattleObject_PanelX, panelX);
        ST8(spawned + oBattleObject_PanelY, panelY);
        ST8(spawned + oBattleObject_Element, element);
        ST32(spawned + oBattleObject_DamageAndStaminaDamageCounterDisabler, damage);
        u32 alliance = LD16(sourceObject + oBattleObject_AllianceAndDirectionFlip);
        ST16(spawned + oBattleObject_AllianceAndDirectionFlip, alliance);
        ST32(spawned + oBattleObject_RelatedObject1Ptr, relatedObject);
        /* the alliance load above leaves its value behind in r3 */
        R3 = alliance;
    }

    RETURN_TO_CALLER();
}

/* ── battle_BDT_80c956c ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r4, r7; } call_FindOpponentPanelXYForThunderObject_result;
/* register form: generated wrapper: FindOpponentPanelXYForThunderObject has no typed C interface yet */
static inline call_FindOpponentPanelXYForThunderObject_result call_FindOpponentPanelXYForThunderObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(FindOpponentPanelXYForThunderObject);
    return (call_FindOpponentPanelXYForThunderObject_result){ R0, R1, R4, R7 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result call_object_getFrontDirection(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result){ R0 };
}

/* Constants from the literal pool at 0x080C95DC–0x080C95E8 */
#define THUNDER_X_BASE_NORMAL   0xAAAB
#define THUNDER_X_BASE_POWER    0x10000
#define THUNDER_Y_BASE_NORMAL   0x6666
#define THUNDER_Y_BASE_POWER    0x999A
#define THUNDER_TIMER_NORMAL    0x3C
#define THUNDER_TIMER_POWER     0x28
#define THUNDER_POWER_LEVEL     2

/* Decide velocity direction and axis, write velocities + timer to the
 * thunder BattleObject at `obj`.  Returns Param1 (the value the original
 * leaves in r2 on exit).  R4 and R7 are written by the
 * FindOpponentPanelXYForThunderObject wrapper. */
static u32 thunder_update(u32 obj) {
    call_FindOpponentPanelXYForThunderObject_result opp =
        call_FindOpponentPanelXYForThunderObject(obj);

    s32 direction;
    bool vertical;

    if (opp.r0 == 0) {
        /* No opponent found – fire in the object's facing direction. */
        call_object_getFrontDirection_result front =
            call_object_getFrontDirection(obj);
        direction = (s32)front.r0;
        vertical = false;
    } else {
        u32 my_y = LD8(obj + oBattleObject_PanelY);
        u32 my_x = LD8(obj + oBattleObject_PanelX);

        if (opp.r1 == my_y) {
            /* Same row: horizontal, toward opponent's column. */
            direction = ((s32)opp.r0 < (s32)my_x) ? -1 : 1;
            vertical = false;
        } else if (opp.r0 == my_x) {
            /* Same column: vertical, toward opponent's row. */
            direction = ((s32)opp.r1 < (s32)my_y) ? -1 : 1;
            vertical = true;
        } else {
            /* Neither matches: horizontal toward opponent's column. */
            direction = ((s32)opp.r0 < (s32)my_x) ? -1 : 1;
            vertical = false;
        }
    }

    /* Speed bump when Param1 == 2 (charged / enhanced thunder). */
    u32 param1 = LD8(obj + oBattleObject_Param1);

    if (vertical) {
        u32 base = (param1 == THUNDER_POWER_LEVEL)
                       ? THUNDER_Y_BASE_POWER : THUNDER_Y_BASE_NORMAL;
        ST32(obj + oBattleObject_YVelocity, direction * (s32)base);
        ST32(obj + oBattleObject_XVelocity, 0);
    } else {
        u32 base = (param1 == THUNDER_POWER_LEVEL)
                       ? THUNDER_X_BASE_POWER : THUNDER_X_BASE_NORMAL;
        ST32(obj + oBattleObject_XVelocity, direction * (s32)base);
        ST32(obj + oBattleObject_YVelocity, 0);
    }

    /* Active-frame countdown before the object expires. */
    u32 timer = (param1 == THUNDER_POWER_LEVEL)
                    ? THUNDER_TIMER_POWER : THUNDER_TIMER_NORMAL;
    ST16(obj + oBattleObject_Timer, timer);

    /* r2 at exit is always Param1 (re-read at the timer comparison). */
    return LD8(obj + oBattleObject_Param1);
}

/* battle_BDT_80c956c(r5 = thunder object, lr) -> r2 = Param1, r4, r7 */
Next battle_BDT_80c956c(void) {
    R2 = thunder_update(R5);
    RETURN_TO_CALLER();
}

/* ── FindOpponentPanelXYForThunderObject ── */
/* FindOpponentPanelXYForThunderObject(r5 = thunder object):
 * Search for a panel occupied by an opponent that the object's thunder
 * attack can reach, scanning horizontally away from the object. Which
 * direction is swept first depends on the object's alliance (player/enemy
 * side) combined with its DirectionFlip flag; if the first sweep finds
 * nothing, the opposite direction is tried as a fallback.
 *
 * The check helpers receive the object in r5 plus a side marker in r4
 * (0x4000000 for alliance 0, 0x8000000 otherwise) and report the panel they
 * found in r6 (x) / r7 (y), with r6 == 0 meaning "nothing found".
 *
 * Returns r0 = panel x, r1 = panel y (both 0 if no opponent was found).
 * r4/r6/r7 are scratch for the helpers and are restored before returning. */
Next FindOpponentPanelXYForThunderObject(void) {
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;

    u32 alliance = LD8(R5 + oBattleObject_Alliance);
    u32 sideFlag = (alliance == 0) ? A_LCDControl : A__text;
    u32 flip = LD8(R5 + oBattleObject_DirectionFlip);

    R4 = sideFlag;
    if ((alliance ^ flip) == 0) {
        /* Facing "forward" for this side: sweep right first, then left. */
        GUEST_CALL(CheckIfThunderObjectFindsOpponentByGoingRight);
        if (R6 == 0)
            GUEST_CALL(CheckIfThunderObjectFindsOpponentByGoingLeft);
    } else {
        /* Sweep left first, then right. */
        GUEST_CALL(CheckIfThunderObjectFindsOpponentByGoingLeft);
        if (R6 == 0)
            GUEST_CALL(CheckIfThunderObjectFindsOpponentByGoingRight);
    }

    R0 = R6;
    R1 = R7;
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
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

/* ── sub_80C96C8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_sub_80C96C8;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_sub_80C96C8 call_sprite_load_sub_80C96C8(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_sub_80C96C8){ R4, R5 };
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_sub_80C96C8;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_sub_80C96C8 call_sprite_loadAnimationData_sub_80C96C8(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_sub_80C96C8){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_sub_80C96C8(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r3; } call_sprite_setPalette_result;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result call_sprite_setPalette_sub_80C96C8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result){ R3 };
}

typedef struct { u32 r2, r3, r5, r6; bool c, v; } call_object_setPanelsFromCoordinates_result;
/* register form: generated wrapper: object_setPanelsFromCoordinates has no typed C interface yet */
static inline call_object_setPanelsFromCoordinates_result call_object_setPanelsFromCoordinates(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setPanelsFromCoordinates);
    return (call_object_setPanelsFromCoordinates_result){ R2, R3, R5, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_sub_80C96C8;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_sub_80C96C8 call_object_getFlip_sub_80C96C8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_sub_80C96C8){ R0 };
}

typedef struct { u32 r1, r2, r3; } call_sprite_setFlip_result_sub_80C96C8;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_sub_80C96C8 call_sprite_setFlip_sub_80C96C8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_sub_80C96C8){ R1, R2, R3 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_80C96C8;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_80C96C8 call_PlaySoundEffect_sub_80C96C8(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_80C96C8){ R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_sub_80C9726_result;
/* register form: generated wrapper: sub_80C9726 has no typed C interface yet */
static inline call_sub_80C9726_result call_sub_80C9726(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80C9726);
    return (call_sub_80C9726_result){ R5, R8, R9, R10, R11, R12 };
}

/* Spinning shuriken spawner: selects a 6-byte template from byte_80C9664
 * indexed by Param2, installs sprite/animation/palette/flip, plays the
 * spawn sound, pins velocity to the spawn position, then enters the
 * per-frame sub-state machine (sub_80C9726).  r5 is the BattleObject. */

typedef struct {
    u32 r5, r8, r9, r10, r11, r12;
} sub_80C96C8_result;

static sub_80C96C8_result spawn_shuriken(u32 battle_obj) {
    sub_80C96C8_result out;
    u32 template;
    u8 flags;
    u32 x, y, z;
    call_sprite_setFlip_result_sub_80C96C8 flip_res;
    call_sub_80C9726_result sub_res;

    /* Pick the template entry (6 bytes each) from byte_80C9664 via Param2,
     * stash its base address in ExtraVars slot 1. */
    template = A_byte_80C9664 + (u32)LD8(battle_obj + oBattleObject_Param2) * 6u;
    ST32(battle_obj + (oBattleObject_ExtraVars + 4), template);

    /* Load tile data: VRAM block 0x80, char base + screen base from entry. */
    call_sprite_load_sub_80C96C8(0x80, LD8(template), LD8(template + 1), battle_obj);

    /* Install animation frames and remove the shadow. */
    call_sprite_loadAnimationData_sub_80C96C8(battle_obj);
    call_sprite_noShadow_sub_80C96C8(battle_obj);

    /* Force the "drawn" flag on the object header. */
    flags = LD8(battle_obj);
    ST8(battle_obj, flags | 2);

    /* Start animation from template byte 2; invalidate the copy (0xFF). */
    ST8(battle_obj + oBattleObject_CurAnim, LD8(template + 2));
    ST8(battle_obj + oBattleObject_CurAnimCopy, 0xFF);

    /* Apply the palette index from template byte 5. */
    call_sprite_setPalette_sub_80C96C8(LD8(template + 5), battle_obj);

    /* Derive panel state from coordinates, then mirror object flip onto sprite. */
    call_object_setPanelsFromCoordinates(battle_obj);
    flip_res = call_sprite_setFlip_sub_80C96C8(call_object_getFlip_sub_80C96C8(battle_obj).r0, battle_obj);

    /* Spawn sound (effect 0xA0 on the channel left by sprite_setFlip). */
    call_PlaySoundEffect_sub_80C96C8(0xA0, flip_res.r2);

    /* Freeze velocity to the spawn position so the object doesn't drift. */
    x = LD32(battle_obj + oBattleObject_X);
    y = LD32(battle_obj + oBattleObject_Y);
    z = LD32(battle_obj + oBattleObject_Z);
    ST32(battle_obj + oBattleObject_XVelocity, x);
    ST32(battle_obj + oBattleObject_YVelocity, y);
    ST32(battle_obj + oBattleObject_ZVelocity, z);

    /* Enter the update phase of the current state action. */
    ST32(battle_obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    /* Hand off to the per-frame sub-state machine; it sets r5,r8,r9,r10,r11,r12. */
    sub_res = call_sub_80C9726(battle_obj);
    out.r5  = sub_res.r5;
    out.r8  = sub_res.r8;
    out.r9  = sub_res.r9;
    out.r10 = sub_res.r10;
    out.r11 = sub_res.r11;
    out.r12 = sub_res.r12;

    return out;
}

/* sub_80C96C8(r5 = battle object, r10/lr = standard) -> r5 r8 r9 r10 r11 r12 */
Next sub_80C96C8(void) {
    sub_80C96C8_result out = spawn_shuriken(R5);
    R5  = out.r5;
    R8  = out.r8;
    R9  = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── sub_80C9726 ── */
/* sub_80C9726 (r5 = battle object):
 * Runs the handler for the object's current action, then manages the object's
 * "busy" flag bit (0x02): the bit is set as soon as the handler returns, and
 * cleared again unless the alliance check (battle_findPlayerByAlliance_800EB6C) returns nonzero, or
 * when the related object's AI data reports flag 0x100000. r5 is preserved. */
Next sub_80C9726(void) {
    u32 obj = R5;

    /* off_80C976C: table of per-action handlers, indexed by CurAction:
     * 0 -> sub_80C9774, 1 -> sub_80C97B4. The handler is entered with
     * r0 = action and r1 = its own table entry. */
    u32 action = LD8(obj + oBattleObject_CurAction);
    u32 handler = LD32(A_off_80C976C + action);
    R0 = action;
    R1 = handler;
    if (action == 0)
        GUEST_CALL(sub_80C9774);
    else
        GUEST_CALL(sub_80C97B4);

    /* Mark the object as busy while its action runs. */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Unless the alliance check passes, the action is done already:
     * clear the busy flag again. */
    R0 = LD8(obj + oBattleObject_Alliance);
    GUEST_CALL(battle_findPlayerByAlliance_800EB6C);
    if (R0 == 0)
        ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) & ~2u);

    /* If the related object's AI data has flag 0x100000 set, the action
     * ends here as well: clear the busy flag. */
    R5 = LD32(obj + oBattleObject_RelatedObject1Ptr);
    GUEST_CALL(GetAIDataUnk0x48Flag);
    if ((R0 & 0x100000) != 0)
        ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) & ~2u);

    R5 = obj;
    RETURN_TO_CALLER();
}

/* ── sub_80C9774 ── */
/* Apply the related object's position plus this object's velocity components
 * to this object's X/Y/Z, copy bit 1 of the related object's header flags
 * into this one's header flags (clearing bit 1 here first), and end the
 * current action if the first dword of ExtraVars is zero.
 *
 * Returns the values the original leaves in r0..r3 at the pop:
 *   r0 = extra_first (branch taken) or 0 (branch not taken, after STRH)
 *   r1 = related_flags & 2          (from `and r1, #2` before the TST)
 *   r2 = 2
 *   r3 = related object ptr
 *
 * The original's last flag-setting instruction is a logical op on each path
 * (`tst r0, r0` when extra_first != 0, `mov r0, #0` otherwise), so the
 * adapter sets N/Z from r0 via SET_FLAGS_NZ; C/V were last cleared by the
 * logical AND r1, r2 and stay cleared. */
typedef struct {
    u32 r0, r1, r2, r3;
} AdvanceResult;

static AdvanceResult do_advance(u32 self) {
    u32 related = LD32(self + oBattleObject_RelatedObject1Ptr);

    /* Position += velocity. */
    ST32(self + oBattleObject_X,
         LD32(related + oBattleObject_X) + LD32(self + oBattleObject_XVelocity));
    ST32(self + oBattleObject_Y,
         LD32(related + oBattleObject_Y) + LD32(self + oBattleObject_YVelocity));
    ST32(self + oBattleObject_Z,
         LD32(related + oBattleObject_Z) + LD32(self + oBattleObject_ZVelocity));

    /* Sync bit 1 of header flags from the related object: clear self bit 1,
     * take bit 1 from related, OR them together and store back. */
    u32 self_after   = LD8(self    + oObjectHeader_Flags) & ~(u32)2;
    u32 other_masked = LD8(related + oObjectHeader_Flags) & (u32)2;
    ST8(self + oObjectHeader_Flags, self_after | other_masked);

    /* If the first dword of ExtraVars is zero, end the action now. */
    u32 extra_first = LD32(LD32(self + oBattleObject_ExtraVars));

    AdvanceResult a;
    a.r1 = other_masked;
    a.r2 = 2;
    a.r3 = related;

    if (extra_first != 0) {
        /* Branch taken (bne): pop with r0 = extra_first (TST didn't modify it). */
        a.r0 = extra_first;
    } else {
        /* Branch not taken: set CurAction = 4 and clear the phase word. */
        ST8 (self + oBattleObject_CurAction, 4);
        ST16(self + oBattleObject_CurPhaseAndPhaseInitialized, 0);
        a.r0 = 0;
    }
    return a;
}

/* sub_80C9774(r5 = self) -> r0..r3, nz/c/v (nz/c/v from a logical op on r0) */
Next sub_80C9774(void) {
    AdvanceResult a = do_advance(R5);
    R0 = a.r0;
    R1 = a.r1;
    R2 = a.r2;
    R3 = a.r3;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80C97B4 ── */
/* sub_80C97B4(r5 = battle object, lr):
 * On first call (PhaseInitialized == 0): set bit 1 of object flags, copy the
 * animation byte and timer value from the object's extra-vars init blob, and
 * mark the phase initialized. Then tick the timer down; when it expires
 * schedule the object for destruction. */
Next sub_80C97B4(void) {
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        ST8(R5 + oObjectHeader_Flags, LD8(R5 + oObjectHeader_Flags) | 2);
        u32 init = LD32(R5 + oBattleObject_ExtraVars + 4);
        ST8(R5 + oBattleObject_CurAnim, LD8(init + 3));
        ST16(R5 + oBattleObject_Timer, LD8(init + 4));
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    }

    u32 timer = LD16(R5 + oBattleObject_Timer) - 1;
    ST16(R5 + oBattleObject_Timer, timer);
    if ((s32)timer < 0) {
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    }
    RETURN_TO_CALLER();
}

/* ── battle_spawnLinkedObjectType2B_80C97E0 ── */
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

/* battle_spawnLinkedObjectType2B_80C97E0: spawn object type 0x2B and link it to the requesting object.
 * r1-r3: pass-through arguments for object_spawnType3
 * r5: the requesting (owner) object; r7: owner's link-slot pointer
 * Outputs r3 (as left by object_spawnType3) and c/v from the null check;
 * r4-r7 are preserved (r5 restored exactly as the original's pop does). */
Next battle_spawnLinkedObjectType2B_80C97E0(void) {
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

/* ── sub_80C9CDA ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; } call_object_spawnType3_result;
/* register form: generated wrapper: object_spawnType3 has no typed C interface yet */
static inline call_object_spawnType3_result call_object_spawnType3_sub_80C9CDA(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType3);
    return (call_object_spawnType3_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r1; } call_BasicBattleObjectInitialize_result;
/* register form: generated wrapper: BasicBattleObjectInitialize has no typed C interface yet */
static inline call_BasicBattleObjectInitialize_result call_BasicBattleObjectInitialize(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(BasicBattleObjectInitialize);
    return (call_BasicBattleObjectInitialize_result){ R1 };
}

/* Battle object spawn helper (sub_80C9CDA).
 *
 * Spawns a battle object of spawn type 0x2F via object_spawnType3, passing the
 * caller's r1-r4 through as spawn parameters (r5-r7 are still the caller's
 * values at that point, exactly as in the original, which only changes r0).
 *
 * If the spawn reports success (non-zero object handle in its r5), the object
 * is finished with BasicBattleObjectInitialize:
 *   r0 = spawned object, r1/r2 = the caller's original r0/r1,
 *   r3 = 0, r5 = the caller's original r5, r6 = whatever the spawn left in r6.
 *
 * The original saves r0-r2/r5 on the stack and pops them back one register
 * over, so on exit r1 = old r0, r2 = old r1, while r5 comes back unchanged.
 * r4/r6/r7 stay whatever object_spawnType3 (and, on the init path,
 * BasicBattleObjectInitialize) left behind, as in the original.
 */
typedef struct {
    u32 r1;
    u32 r2;
    u32 r5;
} SpawnInitResult;

static SpawnInitResult spawn_and_initialize(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5) {
    SpawnInitResult out;

    call_object_spawnType3_result spawned =
        call_object_spawnType3_sub_80C9CDA(0x2F, a1, a2, a3, a4);

    if (spawned.r5 == 0) {
        /* Spawn produced nothing: skip initialization; r1 keeps old r0. */
        out.r1 = a0;
    } else {
        call_BasicBattleObjectInitialize_result init =
            call_BasicBattleObjectInitialize(spawned.r5, a0, a1, 0, a5, spawned.r6);
        out.r1 = init.r1;
    }

    out.r2 = a1;
    out.r5 = a5;
    return out;
}

/* sub_80C9CDA(r0..r4, r6 = spawn/init parameters) -> r1 r2 r5 */
Next sub_80C9CDA(void) {
    SpawnInitResult res = spawn_and_initialize(R0, R1, R2, R3, R4, R5);
    R1 = res.r1;
    R2 = res.r2;
    R5 = res.r5;
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
        case 0: GUEST_CALL(battleObject_initPanelBreakBounce_80CA568);           break;
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

/* ── sub_80CA61A ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_removeCollisionData_result;
/* register form: generated wrapper: object_removeCollisionData has no typed C interface yet */
static inline call_object_removeCollisionData_result call_object_removeCollisionData(u32 r4, u32 r5, u32 r7) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(object_removeCollisionData);
    return (call_object_removeCollisionData_result){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7; bool c, v; } call_object_spawnCollisionEffect_result;
/* register form: generated wrapper: object_spawnCollisionEffect has no typed C interface yet */
static inline call_object_spawnCollisionEffect_result call_object_spawnCollisionEffect(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_spawnCollisionEffect);
    return (call_object_spawnCollisionEffect_result){ R2, R3, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getCoordinatesForPanels_result;
/* register form: generated wrapper: object_getCoordinatesForPanels has no typed C interface yet */
static inline call_object_getCoordinatesForPanels_result call_object_getCoordinatesForPanels(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_getCoordinatesForPanels);
    return (call_object_getCoordinatesForPanels_result){ R0 };
}

typedef struct { u32 r0, r3; } call_IsR2BetweenR0AndR1_result;
/* register form: generated wrapper: IsR2BetweenR0AndR1 has no typed C interface yet */
static inline call_IsR2BetweenR0AndR1_result call_IsR2BetweenR0AndR1(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(IsR2BetweenR0AndR1);
    return (call_IsR2BetweenR0AndR1_result){ R0, R3 };
}

typedef struct { u32 r5, r6; } call_object_setPanelsFromCoordinates_result_sub_80CA61A;
/* register form: generated wrapper: object_setPanelsFromCoordinates has no typed C interface yet */
static inline call_object_setPanelsFromCoordinates_result_sub_80CA61A call_object_setPanelsFromCoordinates_sub_80CA61A(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setPanelsFromCoordinates);
    return (call_object_setPanelsFromCoordinates_result_sub_80CA61A){ R5, R6 };
}

typedef struct { u32 r2, r3, r4, r6; } call_object_updateCollisionPanels_result_sub_80CA61A;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_sub_80CA61A call_object_updateCollisionPanels_sub_80CA61A(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_sub_80CA61A){ R2, R3, R4, R6 };
}

typedef struct { u32 r0, r1; bool c, v; } call_object_isCurrentPanelValid_result_sub_80CA61A;
/* register form: generated wrapper: object_isCurrentPanelValid has no typed C interface yet */
static inline call_object_isCurrentPanelValid_result_sub_80CA61A call_object_isCurrentPanelValid_sub_80CA61A(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_isCurrentPanelValid);
    return (call_object_isCurrentPanelValid_result_sub_80CA61A){ R0, R1, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_sub_80CA61A;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_sub_80CA61A call_object_clearCollisionRegion_sub_80CA61A(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_sub_80CA61A){ R1 };
}

typedef struct { u32 r0, r1, r2, r3, r5, r7, r8; bool z, n, c, v; } call_object_presentCollisionData_result;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result call_object_presentCollisionData(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result){ R0, R1, R2, R3, R5, R7, R8, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Per-tick collision update for a battle object: bounce or land,
 * destroy when contact is lost, then present the collision data. */

/* Refresh panel coords, update, and check validity.
 * Returns true if the current panel is still valid; sets *out_r1 to the r1
 * value left by object_isCurrentPanelValid for the presentCollisionData call. */
static bool revalidate_panel(u32 r5, u32 *out_r1) {
    call_object_setPanelsFromCoordinates_sub_80CA61A(r5);
    call_object_updateCollisionPanels_sub_80CA61A(r5);
    call_object_isCurrentPanelValid_result_sub_80CA61A valid = call_object_isCurrentPanelValid_sub_80CA61A(r5);
    *out_r1 = valid.r1;
    return valid.r0 != 0;
}

/* sub_80CA61A: collision tick for the battle object at r5.
 * Returns the r5 (unchanged, preserved) and r8 (updated by presentCollisionData). */
typedef struct { u32 r5; u32 r8; } sub_80CA61A_result;

static sub_80CA61A_result sub_80CA61A_impl(u32 r4, u32 r5, u32 r7, u32 r8) {
    call_object_removeCollisionData(r4, r5, r7);
    call_object_spawnCollisionEffect(r5);

    /* Is the object still in contact? */
    u32 col_data = LD32(r5 + oBattleObject_CollisionDataPtr);
    u32 col_flags = LD32(col_data + oCollisionData_FlagsFromCollision);

    u32 r1_for_present = 0;
    bool do_destroy = false;

    if (col_flags != 0) {
        do_destroy = true;
    } else {
        u32 z = LD32(r5 + oBattleObject_Z);

        if ((s32)(z >> 16) < (s32)0x14) {
            /* Still rising below the height threshold: integrate vertical velocity. */
            u32 z_vel = LD32(r5 + oBattleObject_ZVelocity);
            ST32(r5 + oBattleObject_Z, z_vel + z);
            r1_for_present = z;
        } else {
            /* At or above threshold: resolve the landing position. */
            u32 px = LD8(r5 + oBattleObject_PanelX);
            u32 py = LD8(r5 + oBattleObject_PanelY);
            u32 coord = call_object_getCoordinatesForPanels(px, py).r0;

            u32 old_x = LD32(r5 + oBattleObject_X);
            u32 x_vel = LD32(r5 + oBattleObject_XVelocity);
            u32 new_x = x_vel + old_x;
            ST32(r5 + oBattleObject_X, new_x);

            if (call_IsR2BetweenR0AndR1(new_x, old_x, coord).r0 == 0) {
                /* Not yet reached target: refresh panels and test validity. */
                do_destroy = !revalidate_panel(r5, &r1_for_present);
            } else {
                /* Landed: consume one bounce counter (32-bit signed check for bgt). */
                s32 param1 = (s32)LD8(r5 + oBattleObject_Param1);
                param1 -= 1;
                ST8(r5 + oBattleObject_Param1, (u8)param1);
                if (param1 > 0) {
                    /* More bounces remain: refresh panels. */
                    do_destroy = !revalidate_panel(r5, &r1_for_present);
                } else {
                    /* Bounce counter exhausted. */
                    do_destroy = true;
                }
            }
        }
    }

    if (do_destroy) {
        /* Clear the collision-active flag and switch to the destroy phase. */
        u8 hdr = LD8(r5 + oObjectHeader_Flags);
        ST8(r5 + oObjectHeader_Flags, hdr & ~2);
        r1_for_present = call_object_clearCollisionRegion_sub_80CA61A(r5).r1;
        ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    }

    call_object_presentCollisionData_result pres =
        call_object_presentCollisionData(r1_for_present, r5, r8);

    sub_80CA61A_result result;
    result.r5 = r5;
    result.r8 = pres.r8;
    return result;
}

/* Adapter: inputs r4 r5 r7 r8; outputs r5 r8. */
Next sub_80CA61A(void) {
    sub_80CA61A_result res = sub_80CA61A_impl(R4, R5, R7, R8);
    R5 = res.r5;
    R8 = res.r8;
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

/* ── t3_0x36_80CAB68 ── */
/* Thumb dispatch table for battle-object state phases at 0x080CAB7C.
 * Entries are (function+1) indexed by the CurState byte (0, 4, 8). */
#define BSTATE_TABLE_BASE  0x80CAB7C

/* register form: original ldr r1,[tbl,r0]; bx r1 — load handler address (with
 * Thumb LSB set) into R1, then dispatch via address (no typed wrapper exists
 * for these three callees). */
static void call_state_handler(u32 dispatch_addr) {
    R1 = dispatch_addr;
    GUEST_CALL_ADDR(dispatch_addr);
}

/* Dispatch the per-frame update for a battle object based on its CurState byte.
 * Reads state from (object_addr + oBattleObject_CurState), looks up the
 * (Thumb) handler address in the dispatch table, and tail-calls it. */
static void battle_object_update_state(u32 object_addr) {
    u32 state = LD8(object_addr + oBattleObject_CurState);
    u32 dispatch_addr = LD32(BSTATE_TABLE_BASE + state);

    call_state_handler(dispatch_addr);
}

/* t3_0x36_80CAB68(r5 = battle object) — state-phase dispatcher. */
Next t3_0x36_80CAB68(void) {
    battle_object_update_state(R5);
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

/* ── sub_80CAB90 ── */
/* sub_80CAB90(r5 = battle object):
 * Per-frame state machine step for a battle object.
 *
 * - If the battle is already over, the object is torn down immediately by
 *   setting its state phase to CUR_STATE_DESTROY.
 * - Otherwise, the current action of the object's related object is checked:
 *   actions 0xA and 0xB are terminal and need no per-frame processing.
 * - If the battle clock is stopped, the update is skipped as well.
 * - Otherwise the object's own CurAction indexes the handler table
 *   off_80CABC8 (0 -> sub_80CABD0, 1 -> sub_80CAC00) and that handler is
 *   invoked with r0 = action id (and r1 = table entry, as in the original).
 *   Its results are left in r5/r8-r12 for our caller. */
Next sub_80CAB90(void) {
    GUEST_CALL(battle_isBattleOver);
    if (R0 != 0) {
        /* Battle over: put the object into its destroy state. */
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
        RETURN_TO_CALLER();
    }

    /* Terminal actions of the related object need no update here. */
    u32 related = LD32(R5 + oBattleObject_RelatedObject1Ptr);
    u32 relatedAction = LD8(related + oBattleObject_CurAction);
    if (relatedAction == 0xB || relatedAction == 0xA)
        RETURN_TO_CALLER();

    /* Frozen clock: skip the update. */
    GUEST_CALL(battle_isTimeStop);
    if (!ZERO_FLAG())
        RETURN_TO_CALLER();

    /* Dispatch on this object's own action through the handler table. */
    u32 action = LD8(R5 + oBattleObject_CurAction);
    u32 handler = LD32(A_off_80CABC8 + action);
    R0 = action;
    R1 = handler;
    if (handler == 0x080CABD1)
        GUEST_CALL(sub_80CABD0);
    else if (handler == 0x080CAC01)
        GUEST_CALL(sub_80CAC00);
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

/* ── sub_80CAC58 ── */
/* sub_80CAC58(r0, r1, r2, r3, r4, r5, r6, r7):
 * Spawn a type-0x36 battle object and initialize its fields from caller
 * registers. Note: push {r0-r2,r5} / pop {r1-r3,r5} are asymmetric, so
 * after the pop r1 holds the caller's r0, r2 holds the caller's r1, etc.
 * r5 (source object) supplies the alliance/direction-flip value.
 * r7 is the secondary related-object; a back-pointer is written to [r7].
 * If the spawn returns NULL in r5, initialization is skipped. */
Next sub_80CAC58(void) {
    /* Save caller registers that the asymmetric push/pop shifts into others */
    u32 caller_r0 = R0;
    u32 caller_r1 = R1;
    u32 caller_r2 = R2;
    u32 source_obj = R5;

    /* Spawn the new battle object (type 0x36); pointer returned in r5 */
    R0 = 0x36;
    GUEST_CALL(object_spawnType3);
    u32 new_obj = R5;

    /* Simulate pop {r1-r3, r5}: values shift from saved r0,r1,r2,r5 */
    R2 = caller_r1;
    R3 = caller_r2;
    R5 = source_obj;

    if (new_obj != 0) {
        /* r1 after pop = caller_r0; r3 after pop = caller_r2 */
        ST32(new_obj + oBattleObject_ExtraVars, caller_r0);
        ST8(new_obj + oBattleObject_Element, caller_r2);
        ST32(new_obj + oBattleObject_RelatedObject1Ptr, source_obj);
        ST32(new_obj + oBattleObject_DamageAndStaminaDamageCounterDisabler, R6);

        /* Inherit alliance and direction flip from source object */
        u16 alliance_dir = LD16(source_obj + oBattleObject_AllianceAndDirectionFlip);
        ST16(new_obj + oBattleObject_AllianceAndDirectionFlip, alliance_dir);
        R3 = alliance_dir;

        /* Link secondary related object with a back-pointer at [r7] */
        ST32(new_obj + oBattleObject_RelatedObject2Ptr, R7);
        ST32(R7, new_obj);

        /* Mark the new object with active-battle flag (bit 4) */
        u32 flags = LD8(new_obj + oObjectHeader_Flags) | 0x10;
        ST8(new_obj + oObjectHeader_Flags, flags);
        R2 = flags;
    }

    /* Last flag-setting op is ADDS new_obj, #0 → V = 0 (no overflow) */
    SET_FLAGS_ADD(new_obj, 0);

    RETURN_TO_CALLER();
}

/* ── battle_aiPickMovePanels_80CAC82 ── */
typedef struct { u32 column; u32 row; } PanelPos;
typedef struct { u32 r0, r1, r2, r3, r7; } Out5;

/* register form: stack buffer (28 bytes: extra-byte spill at sp+0, panel buffer at sp+4)
 * plus calls GetPositiveSignedRNG, svc 6, and object_getPanelsInRowFiltered outside this file.
 * Picks one random panel from the row-filtered candidate set; returns (0,0) if empty. */
static PanelPos pick_row_filtered_panel(u8 extra, u32 f0, u32 f1) {
    PanelPos out = {0, 0};
    u32 frame = SP - 28;
    SP = frame;
    u32 buf_addr = frame + 4;
    ST32(frame, (u32)extra);
    R0 = (u32)extra; R2 = f0; R3 = f1; R7 = buf_addr;
    GUEST_CALL(object_getPanelsInRowFiltered);
    s32 count = (s32)R0;
    if (count != 0) {
        R0 = (u32)count;
        GUEST_CALL(GetPositiveSignedRNG);
        R1 = (u32)count;
        swi_call(6);
        u32 packed = LD8(buf_addr + R1);
        out.column = ((u32)packed << 29) >> 29;
        out.row = (u32)packed >> 4;
    }
    SP += 28;
    return out;
}

/* register form: stack buffer + calls GetPositiveSignedRNG, svc 6, and
 * object_getPanelsIgnoreColumnFiltered outside this file. */
static PanelPos pick_ignore_col_panel(u8 extra, u32 f0, u32 f1) {
    PanelPos out = {0, 0};
    u32 frame = SP - 28;
    SP = frame;
    u32 buf_addr = frame + 4;
    ST32(frame, (u32)extra);
    R0 = (u32)extra; R2 = f0; R3 = f1; R7 = buf_addr;
    GUEST_CALL(object_getPanelsIgnoreColumnFiltered);
    s32 count = (s32)R0;
    if (count != 0) {
        R0 = (u32)count;
        GUEST_CALL(GetPositiveSignedRNG);
        R1 = (u32)count;
        swi_call(6);
        u32 packed = LD8(buf_addr + R1);
        out.column = ((u32)packed << 29) >> 29;
        out.row = (u32)packed >> 4;
    }
    SP += 28;
    return out;
}

/* Look up the (f0, f1) filter pair for an alliance in the table at A_byte_80CAD04. */
static void get_filter(u32 alliance, u32 *f0, u32 *f1) {
    u32 addr = A_byte_80CAD04 + (alliance << 3);
    *f0 = LD32(addr);
    *f1 = LD32(addr + 4);
}

/* Pick two destination panels for an AI object's movement:
 *   r0/r1 = (column, row) from the row-filtered set (0,0 if none),
 *   r2/r3 = (column, row) from the row+column filtered set (0,0 if none).
 * r4-r7 are preserved. */
static Out5 sub_80CAC82_impl(u32 r4, u32 r5, u32 r6, u32 r7) {
    (void)r4; (void)r6;
    Out5 res;
    u32 alliance = LD8(r5 + oBattleObject_Alliance);
    u32 extra = LD8(LD32(r5 + oBattleObject_ExtraVars) + 0x13);
    u32 f0, f1;

    get_filter(alliance, &f0, &f1);
    PanelPos p1 = pick_row_filtered_panel((u8)extra, f0, f1);
    res.r0 = p1.column;
    res.r1 = p1.row;

    get_filter(alliance, &f0, &f1);
    PanelPos p2 = pick_ignore_col_panel((u8)extra, f0, f1);
    res.r2 = p2.column;
    res.r3 = p2.row;
    res.r7 = r7;
    return res;
}

Next battle_aiPickMovePanels_80CAC82(void) {
    Out5 o = sub_80CAC82_impl(R4, R5, R6, R7);
    R0 = o.r0; R1 = o.r1; R2 = o.r2; R3 = o.r3; R7 = o.r7;
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

/* ── sub_80CADE0 ── */
/* sub_80CADE0:
 * Collision-update step for a battle object (object in r5).
 *
 * Removes the object's old collision data and spawns the associated effect,
 * then decides what to do next:
 *   - battle over, or the object's collision flags are set: clear the
 *     collision region and move the object to the destroy state;
 *   - time stop active: just keep the collision data presented;
 *   - otherwise: run the current action's movement handler via the
 *     two-entry jump table at off_80CAE20, then present the collision data.
 *
 * outputs: r5 r8 r9 r10 r11 r12 (everything else is scratch) */
Next sub_80CADE0(void) {
    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);

    GUEST_CALL(battle_isBattleOver);
    u32 battleOver = R0;

    /* A colliding object also takes the destroy path. */
    u32 collisionFlags = 0;
    if (battleOver == 0) {
        u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
        collisionFlags = LD32(collisionData + oCollisionData_FlagsFromCollision);
    }

    if (battleOver != 0 || collisionFlags != 0) {
        GUEST_CALL(object_clearCollisionRegion);
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isTimeStop);
    if (!ZERO_FLAG()) {
        /* Time stop: the object keeps its collision data visible this frame. */
        GUEST_CALL(object_presentCollisionData);
        RETURN_TO_CALLER();
    }

    /* Dispatch through the action jump table (off_80CAE20):
     * action 0 -> sub_80CAE28, action 1 -> battle_applyXMoveAndCheckDestroy.
     * Both handlers return into the presentCollisionData call below
     * (the original does this by setting lr = 0x80CAE0A before the bx). */
    u32 action = LD8(R5 + oBattleObject_CurAction);
    switch (action) {
    case 0:
        GUEST_CALL(sub_80CAE28);
        break;
    default:
        GUEST_CALL(battle_applyXMoveAndCheckDestroy_80CAE94);
        break;
    }

    GUEST_CALL(object_presentCollisionData);
    RETURN_TO_CALLER();
}

/* ── sub_80CAEC8 ── */
/* sub_80CAEC8(r0 = panel x, r1 = panel y, r2 = element,
 *              r5 = source object, r6 = damage value):
 * Spawn a type-3 battle object (kind 0x37) on the given panel, then copy the
 * source object's alliance/direction-flip halfword into it, store the damage
 * value, and set its header flag 0x10 (spawned-this-frame marker).
 * Returns the spawned object pointer in r5; leaves r5 unchanged (the source
 * pointer) when spawning failed. */
Next sub_80CAEC8(void) {
    u32 panelX = R0;
    u32 panelY = R1;
    u32 element = R2;
    u32 source = R5;
    u32 damage = R6;

    R0 = 0x37;
    GUEST_CALL(object_spawnType3);
    u32 obj = R5;

    if (obj != 0) {
        ST8(obj + oBattleObject_PanelX, panelX);
        ST8(obj + oBattleObject_PanelY, panelY);
        ST8(obj + oBattleObject_Element, element);
        ST32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler, damage);
        ST16(obj + oBattleObject_AllianceAndDirectionFlip,
             LD16(source + oBattleObject_AllianceAndDirectionFlip));
        ST8(obj + oObjectHeader_Flags,
            LD8(obj + oObjectHeader_Flags) | 0x10);
    }

    R5 = source; /* the pop restores the caller's r5, which is the output */
    RETURN_TO_CALLER();
}

/* ── t3_0x38_80CAEF0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x38_80CAEF0;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x38_80CAEF0 call_object_updateSpritePaused_t3_0x38_80CAEF0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x38_80CAEF0){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t3_0x38_80CAEF0 - per-frame dispatcher for a battle object.
 *
 * Reads the object's CurState byte and calls the matching handler from the
 * three-entry table at off_80CAF08:
 *   0: battle_initShipSprite_80CAF14   (ship sprite init)
 *   1: sub_80CAF9C                     (paused/idle state)
 *   2: object_destroyWrapper_80CB078   (destruction)
 * using the "mov lr,pc; bx r1" idiom, then runs object_updateSpritePaused on
 * the same object before returning.
 *
 * The values callers read afterwards (r5 from the update call, r8-r12 as the
 * state handler left them, r10 likewise) are left in the guest registers by
 * the callees themselves, so nothing has to be carried through return values.
 * object_updateSpritePaused preserves r8-r12, which is why the handler's
 * copies of them survive to the callers. */

/* register form: the state handler is entered through a code address
 * ("mov lr,pc; bx r1" onto a table entry), so it must be invoked with
 * GUEST_CALL_ADDR. It receives the state byte in r0 and its own table entry
 * in r1; the helper reports the object pointer it leaves in r5, which the
 * following object_updateSpritePaused call needs. */
static u32 run_state_handler_t3_0x38_80CAEF0(u32 handler, u32 state) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    return R5;
}

/* Dispatch on the object's CurState, then run the shared sprite update. */
static void battle_object_state_update(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80CAF08 + state);

    u32 r5 = run_state_handler_t3_0x38_80CAEF0(handler, state);
    call_object_updateSpritePaused_t3_0x38_80CAEF0(r5);
}

/* r5 = battle object, r10 = passes through to the state handler untouched.
 * All outputs (r5 r8 r9 r10 r11 r12) are whatever the callees left behind. */
Next t3_0x38_80CAEF0(void) {
    battle_object_state_update(R5);
    RETURN_TO_CALLER();
}

/* ── battle_initShipSprite_80CAF14 ── */
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

/* battle_initShipSprite_80CAF14(r5 = battle object) -> r0=CUR_STATE_UPDATE_sub_80CAF14, r1=extra2, nz from r0 */
Next battle_initShipSprite_80CAF14(void) {
    ShipInitResult res = init_ship(R5);
    R0 = res.phase;
    R1 = res.r1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80CAF9C ── */
/* sub_80CAF9C: Per-object battle update tick.
 * r5 = battle object pointer.
 * If the battle has ended → mark the object for destruction.
 * If time is stopped → skip this tick.
 * Otherwise → dispatch to the object's current action handler (jump table). */
Next sub_80CAF9C(void) {
    GUEST_CALL(battle_isBattleOver);
    if (R0 != 0) {
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isTimeStop);
    if (!ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    /* Action jump table (off_80CAFC4) has a single entry: sub_80CAFC8 for action 0. */
    GUEST_CALL(sub_80CAFC8);

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

/* ── battle_spawnAndConfigureNextObject_80CB080 ── */
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

/* battle_spawnAndConfigureNextObject_80CB080(r0=futureX, r1=futureY, r2=element, r5=sourceObj, r6=damageDisabler)
 * Outputs r1, r2, r3; r4–r7 preserved. */
Next battle_spawnAndConfigureNextObject_80CB080(void) {
    struct SpawnConfigResult res = configure_spawned_object(
        R0, R1, R2, R5, R6);
    R1 = res.r1;
    R2 = res.r2;
    R3 = res.r3;
    RETURN_TO_CALLER();
}

/* ── battle_spawnCollisionForObject_80CB0AE ── */
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

/* battle_spawnCollisionForObject_80CB0AE(r5 = battle object) -> c, v (region spawn + timer init) */
Next battle_spawnCollisionForObject_80CB0AE(void) {
    u32 region = spawn_collision_for_object(R5);
    SET_FLAGS_NZ(region);
    RETURN_TO_CALLER();
}

/* ── t3_0x44_80CC944 ── */
/* Battle object state dispatcher (task type 3 handler).
 *
 * r5 points to a battle object. Its current-state byte indexes the handler
 * table at off_80CC958:
 *   0 -> sub_80CC964, 1 -> sub_80CC9AA, 2 -> object_genericDestroy
 * (a state past the end reads whatever follows the table, as in the original).
 *
 * The chosen handler is tail-called with r0 = state byte and r1 = its own
 * table entry, and it returns straight to this function's caller (the
 * original's `mov lr, pc; bx r1` lands on the `pop {pc}`). The handler's
 * register state therefore IS this function's result: r5, r8-r12 are left
 * in the guest register file by the callee and must not be re-written here. */

/* register form: the handler address comes from the guest jump table at
 * off_80CC958, so the call must go through that address (the original's
 * mov lr, pc; bx r1). The helper places the state byte in r0 and the table
 * entry in r1, exactly as the dispatcher leaves them for the handler. */
static void call_state_handler_t3_0x44_80CC944(u32 handler, u32 state) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

/* Read the object's state byte, look up its handler in the table and run it. */
static void dispatch_battle_object_state_t3_0x44_80CC944(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80CC958 + state);
    call_state_handler_t3_0x44_80CC944(handler, state);
}

Next t3_0x44_80CC944(void) {
    dispatch_battle_object_state_t3_0x44_80CC944(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80CC964 ── */
/* sub_80CC964: Initialize a battle object's collision data and transition to update state.
 * r5 = BattleObject pointer. */
Next sub_80CC964(void) {
    u32 saved_r4 = R4;

    /* Place the object on the grid using its panel coordinates */
    GUEST_CALL(object_setCoordinatesFromPanels);

    /* Reset the object's vertical (Z) offset */
    ST32(R5 + oBattleObject_Z, 0);

    /* Allocate a collision data buffer for this object */
    GUEST_CALL(object_createCollisionData);
    u32 collisionHandle = R0;

    if (collisionHandle == 0) {
        /* Allocation failed – release memory and bail */
        GUEST_CALL(object_freeMemory);
        R4 = saved_r4;
        RETURN_TO_CALLER();
    }

    /* Configure collision geometry from the object's parameter bytes */
    R0 = collisionHandle;
    R1 = LD8(R5 + oBattleObject_Param4);
    R2 = LD8(R5 + oBattleObject_Param3);
    R3 = LD32(R5 + oBattleObject_ExtraVars);
    R4 = collisionHandle;
    GUEST_CALL(object_setupCollisionData);

    /* Assign the hit effect ID to this collision */
    R0 = LD8(R5 + oBattleObject_Param2);
    R4 = collisionHandle;
    GUEST_CALL(object_setCollisionHitEffect);

    /* Store the damage amount into the collision record (offset 1) */
    ST8(collisionHandle + 1, LD8(R5 + oBattleObject_Param1));

    /* Optionally attach a status effect if ExtraVars[4] is set */
    R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
    if (R0 != 0) {
        R4 = collisionHandle;
        GUEST_CALL(object_setCollisionStatusEffect1);
    }

    /* Finalize the collision so it becomes active in the battle */
    R4 = collisionHandle;
    GUEST_CALL(object_presentCollisionData);

    /* Load the update timer from ExtraVars[8] into the object's timer */
    ST16(R5 + oBattleObject_Timer, LD32(R5 + oBattleObject_ExtraVars + 8));

    /* Transition the object to its update state */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_80CC9AA ── */
/* sub_80CC9AA: collision aftermath step for a battle object (r5).
 *
 * Removes the object's live collision data and spawns the impact effect,
 * then decides how the collision sequence ends:
 *   - battle over, a collision flag matching the object's alliance is set,
 *     or the related object is no longer performing the recorded action
 *     -> clear the collision region and switch to the destroy state;
 *   - otherwise decrement the collision timer; while it is still positive
 *     keep the collision data presented, otherwise fall into the destroy
 *     path as well.
 *
 * inputs:  r4 r5 r7 r8 r10 lr (r5 = battle object)
 * outputs: r5 r8 (passed through untouched)
 * clobbers: nz c v r0 r1 r2 r3 lr
 * preserved: r4 r5 r6 r7
 */
Next sub_80CC9AA(void) {
    int destroy;

    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);

    GUEST_CALL(battle_isBattleOver);
    destroy = (R0 != 0);

    if (!destroy) {
        /* Does the collision flag table list a hit against our alliance? */
        u32 collisionFlags = LD32(LD32(R5 + oBattleObject_CollisionDataPtr)
                                  + oCollisionData_FlagsFromCollision);
        u32 allianceMask = LD32(A_byte_80CC9F4
                                + ((u32)LD8(R5 + oBattleObject_Alliance) << 2));
        destroy = (allianceMask & collisionFlags) != 0;
    }

    if (!destroy) {
        /* The related object must still be performing the action we recorded. */
        u32 relatedAction = LD8(LD32(R5 + oBattleObject_RelatedObject1Ptr)
                                + oBattleObject_CurAction);
        u32 expectedAction = LD32(R5 + oBattleObject_ExtraVars + 0xC);
        destroy = relatedAction != expectedAction;

        if (!destroy) {
            /* Count down the collision timer; keep presenting the collision
             * data while it is still positive. */
            u32 timer = (u32)LD16(R5 + oBattleObject_Timer) - 1;
            ST16(R5 + oBattleObject_Timer, (u16)timer);
            if ((s32)timer > 0) {
                R1 = expectedAction; /* read by object_presentCollisionData */
                GUEST_CALL(object_presentCollisionData);
                RETURN_TO_CALLER();
            }
            destroy = 1;
        }
    }

    GUEST_CALL(object_clearCollisionRegion);
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    RETURN_TO_CALLER();
}

/* ── sub_80CC9FC ── */
/* sub_80CC9FC(r0..r3,r6,r7 = fields, r5 = parent battle object):
 *   spawn a battle sub-object of type 0x44 (linked to parent in r5) and
 *   copy panel/element/damage/extra-vars/flags over from the parent's setup.
 *   Returns r3 = parent object's damage/finished flag (unchanged on the early-return path). */
Next sub_80CC9FC(void) {
    /* Reserve a 16-byte scratch window matching the original's {r0-r2,r5} push.
     * The caller-provided r0..r2 are stashed there so object_spawnType3 can
     * clobber its scratch registers without losing the panel/element seed. */
    u32 save = SP -= 16;
    ST32(save,      R0);
    ST32(save + 4,  R1);
    ST32(save + 8,  R2);
    ST32(save + 12, R5);

    R0 = 0x44;
    GUEST_CALL(object_spawnType3);

    /* The new sub-object pointer lands in R0. Pull the saved seeds back into
     * r1..r3 (and r5 = parent) the same way the asm pops them. */
    R0 = R5;                       /* spawn failed -> bail out, r0 = 0 */
    SET_FLAGS_NZ(R0);
    R1 = LD32(save);
    R2 = LD32(save + 4);
    R3 = LD32(save + 8);
    R5 = LD32(save + 12);
    SP = save + 16;

    if (ZERO_FLAG()) RETURN_TO_CALLER();

    /* Copy panel coords + element + parent's damage/state word into the sub-object. */
    ST8(R0 + oBattleObject_PanelX,                                   (u8)R1);
    ST8(R0 + oBattleObject_PanelY,                                   (u8)R2);
    ST8(R0 + oBattleObject_Element,                                  (u8)R3);
    ST32(R0 + oBattleObject_DamageAndStaminaDamageCounterDisabler, R6);

    /* Unpack the 4 packed bytes in r7 into oBattleObject_ExtraVars[0..3]. */
    u32 mask = 0xFF;
    u32 packed = R7;
    ST32(R0 + oBattleObject_ExtraVars + 0x0, packed & mask);
    ST32(R0 + oBattleObject_ExtraVars + 0x4, (packed >> 8)  & mask);
    ST32(R0 + oBattleObject_ExtraVars + 0x8, (packed >> 16) & mask);
    ST32(R0 + oBattleObject_ExtraVars + 0xC, (packed >> 24) & mask);

    /* Inherit alliance/direction-flip from the parent and link the parent back. */
    R3 = LD16(R5 + oBattleObject_AllianceAndDirectionFlip);
    ST16(R0 + oBattleObject_AllianceAndDirectionFlip, (u16)R3);
    ST32(R0 + oBattleObject_RelatedObject1Ptr, R5);

    /* Set the "has parent / linked" flag (0x10) in the object header. */
    u32 flags = LD8(R0 + oObjectHeader_Flags);
    ST8(R0 + oObjectHeader_Flags, (u8)(flags | 0x10));

    RETURN_TO_CALLER();
}

/* ── t3_0x4e_80CE118 ── */
/* Battle-object state dispatcher (t3 = task 0x4e).
 *
 * The battle object at r5 carries a current-state byte; this dispatcher
 * uses it to index the handler table at off_80CE12C and calls the
 * selected handler, then returns to its own caller. The handler runs
 * with:
 *   r0  = the state byte that selected it
 *   r1  = the jump-table base (off_80CE12C)
 *   r2/r5/r10 passed through unchanged
 * and finds the dispatcher's return address pushed at [SP] (the
 * original's `push {lr}`), which some handlers inspect. */

/* Call a state handler through its table address.
 * Register form: the handler receives its arguments in the guest
 * registers and its caller's return address in the word pushed at [SP];
 * a plain C call cannot express that calling convention. The handler's
 * register results (r5, r8-r12) stay in the guest register state. */
static void call_state_handler_t3_0x4e_80CE118(u32 handler, u32 state, u32 r2v, u32 r5v, u32 r10v, u32 callerLr) {
    /* register form: callee is entered through GUEST_CALL_ADDR with its
       arguments in guest registers and the dispatcher's lr at [SP]. */
    R0 = state;
    R1 = A_off_80CE12C;
    R2 = r2v;
    R5 = r5v;
    R10 = r10v;

    /* push {lr}: expose the caller's return address to the handler */
    SP -= 4;
    ST32(SP, callerLr);

    /* mov lr, pc; bx r1: call the handler, coming back here after it */
    GUEST_CALL_ADDR(handler);

    /* pop {lr}: restore the stack for the dispatcher's own caller */
    SP += 4;
}

/* Look up the handler for the object's current state and run it. */
static void dispatch_object_state_t3_0x4e_80CE118(u32 objPtr, u32 r2v, u32 r10v, u32 callerLr) {
    u32 state = LD8(objPtr + oBattleObject_CurState);
    u32 handler = LD32(A_off_80CE12C + state);
    call_state_handler_t3_0x4e_80CE118(handler, state, r2v, objPtr, r10v, callerLr);
}

/* t3_0x4e_80CE118: dispatch one battle object's state handler.
 * Inputs r2/r5/r10; the handler's results are left in r5/r8-r12. */
Next t3_0x4e_80CE118(void) {
    dispatch_object_state_t3_0x4e_80CE118(R5, R2, R10, LR);
    RETURN_TO_CALLER();
}

/* ── battleObject_spawnEntry_80CE138 ── */
/* battleObject_spawnEntry_80CE138: Initialize a battle object sprite, set its flags, load
 * its graphics and animation, place it on the panel grid, and queue
 * the "update" state for the next frame. */
Next battleObject_spawnEntry_80CE138(void) {
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

/* ── sub_80CE178 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_sub_80CE178;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_sub_80CE178 call_object_updateSprite_sub_80CE178(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_sub_80CE178){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_80CE178 - per-frame action step for one battle object (r5).
 *
 * Dispatches the handler selected by the object's current action
 * (CurAction indexes the two-entry table off_80CE190) with the object in
 * r5 and the caller's r2 passed straight through, then refreshes the
 * object's sprite animation. r5 and r8-r12 end up as the handler and
 * object_updateSprite leave them, so nothing is returned explicitly. */

/* register form: the action handler is only reachable through the jump
 * table off_80CE190, so it must be invoked by its runtime address; r0-r2
 * carry the action, the table entry and the caller's r2 into the handler,
 * and the (possibly updated) object pointer is read back from r5. */
static u32 dispatch_action_handler(u32 action, u32 handler, u32 context) {
    R0 = action;
    R1 = handler;
    R2 = context;
    GUEST_CALL_ADDR(handler);
    return R5;
}

static void run_action_then_update_sprite(u32 object, u32 context) {
    u32 action = LD8(object + oBattleObject_CurAction);

    /* Thumb LDR with a register offset does not scale it: CurAction is
     * used directly as the byte offset into the table. */
    u32 handler = LD32(A_off_80CE190 + action);

    object = dispatch_action_handler(action, handler, context);

    call_object_updateSprite_sub_80CE178(object);
}

Next sub_80CE178(void) {
    run_action_then_update_sprite(R5, R2);
    RETURN_TO_CALLER();
}

/* ── sub_80CE198 ── */
/* sub_80CE198: per-tick update of a battle object that is dropping toward the
 * ground. Every 10th tick (timer modulo 10 == 0) it kicks up an effect at its
 * position with a bit of random jitter on X/Z. Its height (Z16) falls by 3
 * each tick; once it would drop below 3 it has landed: clear the airborne
 * flag, switch to action 4 and reset the current phase. */
Next sub_80CE198(void) {
    u32 timer = LD16(R5 + oBattleObject_Timer) + 1;
    ST16(R5 + oBattleObject_Timer, timer);

    /* BIOS Div (swi 6): r0 / r1 -> quotient in r0, remainder in r1.
     * Remainder 0 means this is every 10th tick of the timer. */
    R0 = timer;
    R1 = 10;
    swi_call(6);
    if (R1 == 0) {
        R0 = 0x10;                                 /* effect id */
        R1 = LD32(R5 + oBattleObject_X);
        R2 = LD32(R5 + oBattleObject_Y);
        R3 = LD32(R5 + oBattleObject_Z);
        /* jitters the X/Z coords in r1/r3, leaves r0/r2 intact */
        GUEST_CALL(AddRandomVarianceToTwoCoords);
        R4 = 0x26;
        GUEST_CALL(spawn_t1_0x0_EffectObject);
    }

    u32 z = LD16(R5 + oBattleObject_Z16);
    if ((s32)z >= 3) {
        ST16(R5 + oBattleObject_Z16, z - 3);       /* still falling */
    } else {
        /* landed: clear the airborne flag (bit 1) and hand control to the
         * next action with a fresh phase */
        ST8(R5, LD8(R5) & ~0x02);
        ST8(R5 + oBattleObject_CurAction, 4);
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80CE1D6 ── */
Next sub_80CE1D6(void) {
    u8 phase = LD8(R5 + oBattleObject_CurPhase);

    if (phase == 0) {
        u32 self = R5;
        u32 target = LD32(self + oBattleObject_RelatedObject1Ptr);
        u32 amount = LD16(self + oBattleObject_Damage);
        if (amount == 0)
            amount = LD16(target + oBattleObject_MaxHP);

        /* object_addHP expects r5 = target object. Save/restore self around it. */
        R0 = amount;
        R5 = target;
        GUEST_CALL(object_addHP);
        R5 = self;

        R0 = SOUND_UNK_8A;
        GUEST_CALL(PlaySoundEffect);

        /* Pull 3 effect parameters from target + 0x34, advance R0 by 12 (ldmia r0!). */
        u32 params_src = LD32(self + oBattleObject_RelatedObject1Ptr) + 0x34;
        R1 = LD32(params_src);
        R2 = LD32(params_src + 4);
        R3 = LD32(params_src + 8);
        R0 = params_src + 12;
        R4 = 6;
        GUEST_CALL(spawn_t1_0x0_EffectObject);

        ST16(R5 + oBattleObject_Timer, 0x3C);
        ST8(R5 + oBattleObject_CurPhase, 4);
    } else {
        u16 timer = LD16(R5 + oBattleObject_Timer) - 1;
        ST16(R5 + oBattleObject_Timer, timer);
        if (timer != 0)
            RETURN_TO_CALLER();
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
             CUR_STATE_DESTROY);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80CE21C ── */
/* sub_80CE21C(r5 = object, lr = return address):
 *   Free the object's allocated memory and return to caller. */
Next sub_80CE21C(void) {
    GUEST_CALL(object_freeMemory);
    RETURN_TO_CALLER();
}

/* ── sub_80CE224 ── */
/* sub_80CE224(r0 = panel X, r1 = panel Y, r5 = owner object, r6 = counter value):
 * Spawn battle object type 0x4E at the given panel and link it to the owner
 * object in r5 (same alliance, back-pointer, and a 0x10 header flag).
 * object_spawnType3 hands back the new object through r5, but this function
 * must preserve the caller's r5, so restore it before returning. */
Next sub_80CE224(void) {
    u32 panelX = R0;
    u32 panelY = R1;
    u32 owner = R5;
    u32 counter = R6;

    R0 = 0x4E;
    GUEST_CALL(object_spawnType3);
    u32 spawned = R5;

    if (spawned != 0) {
        ST8(spawned + oBattleObject_PanelX, panelX);
        ST8(spawned + oBattleObject_PanelY, panelY);
        ST32(spawned + oBattleObject_DamageAndStaminaDamageCounterDisabler, counter);
        ST8(spawned + oBattleObject_Alliance, LD8(owner + oBattleObject_Alliance));
        ST32(spawned + oBattleObject_RelatedObject1Ptr, owner);
        ST8(spawned + oObjectHeader_Flags, LD8(spawned + oObjectHeader_Flags) | 0x10);
    }

    R5 = owner;
    RETURN_TO_CALLER();
}

/* ── t3_0x98_80D8444 ── */
/* Tail-call dispatch on BattleObject->CurState: look up the handler in the
 * jump table at off_80D8458 using CurState as a direct byte offset, then
 * transfer control. The handler's return goes directly to this function's
 * caller (classic Thumb "mov lr, pc; bx r1" tail-call idiom). */
/* register form: GUEST_CALL_ADDR for jump-table tail call */
static void battle_object_state_dispatch(u32 battle_obj) {
    u32 state = LD8(battle_obj + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D8458 + state);
    GUEST_CALL_ADDR(handler);
}

/* t3_0x98_80D8444: dispatch on BattleObject state.
 *   inputs:  r5 (BattleObject ptr)
 *   outputs: r5 r8 r9 r10 r11 r12 (modified by the selected handler)
 *   clobbers: nz c v r0 r1 r2 r3 r4 r6 r7 lr */
Next t3_0x98_80D8444(void) {
    battle_object_state_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80D8464 ── */
/* sub_80D8464: Initialize a battle object (FlagMan-type object) by snapping it to its
 * panel, loading its sprite/animation, setting its stat sheet, building its hitbox and
 * stamping the current state-action phase. */
Next sub_80D8464(void) {
    /* Snap to the object's current panel coordinates. */
    GUEST_CALL(object_setCoordinatesFromPanels);

    /* Clear Z and copy current panel XY into the "future" panel slot. */
    ST16(R5 + oBattleObject_Z16, 0);
    ST16(R5 + oBattleObject_FuturePanelXY, LD16(R5 + oBattleObject_PanelXY));

    /* Load the sprite (slot 0x80, sub 0x10, variant 0) and disable its drop shadow. */
    R0 = 0x80; R1 = 0x10; R2 = 0;
    GUEST_CALL(sprite_load);
    GUEST_CALL(sprite_noShadow);

    /* Mark the object as belonging to the player (flag bit 2). */
    u8 objFlags = LD8(R5 + oObjectHeader_Flags);
    ST8(R5 + oObjectHeader_Flags, objFlags | 2);

    /* Kick off animation index 3 on both the live and mirrored slots. */
    const u8 anim = 3;
    ST8(R5 + oBattleObject_CurAnim, anim);
    ST8(R5 + oBattleObject_CurAnimCopy, anim);
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_update);

    /* Apply palette 0 and bump Y/Z up by one pixel so the sprite renders on top. */
    R0 = 0;
    GUEST_CALL(sprite_setPalette);
    ST16(R5 + oBattleObject_Z16, (u16)(LD16(R5 + oBattleObject_Z16) - 1));
    ST16(R5 + oBattleObject_Y16, (u16)(LD16(R5 + oBattleObject_Y16) - 1));

    /* Stat sheet: name 0xD2, HP 1/1. */
    ST16(R5 + oBattleObject_NameID, 0xD2);
    const u16 oneHp = 1;
    ST16(R5 + oBattleObject_HP,    oneHp);
    ST16(R5 + oBattleObject_MaxHP, oneHp);

    /* Build the object's collision data; bail out (free it) if allocation failed. */
    GUEST_CALL(object_createCollisionData);
    if (R0 == 0) {
        GUEST_CALL(object_freeMemory);
        RETURN_TO_CALLER();
    }

    /* 4-panel-wide (0xE / 0xF), 3-frame hitbox, present it and grant invulnerability. */
    R1 = 0xE; R2 = 0xF; R3 = 3;
    GUEST_CALL(object_setupCollisionData);
    GUEST_CALL(object_presentCollisionData);
    R0 = OBJECT_FLAGS_INVULNERABLE;
    GUEST_CALL(object_setFlag1);

    /* Phase = state-update. */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    RETURN_TO_CALLER();
}

/* ── battleObject_respawn_80D84DE ── */
/* battleObject_respawn_80D84DE: respawn / reinitialize a battle object.
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
Next battleObject_respawn_80D84DE(void) {
    SP -= 4;
    ST32(SP, LR);

    /* Set oObjectHeader_Flags bit 1 (object-hidden / respawn flag). */
    u8 flags = LD8(R5 + oObjectHeader_Flags) | 2;
    SET_FLAGS_NZ(flags);
    ST8(R5 + oObjectHeader_Flags, flags);

    GUEST_CALL(sprite_clearFinalPalette);
    GUEST_CALL(object_applyCollisionHit_801AD9E);

    ST16(R5 + oBattleObject_HP, 1);

    R0 = A_off_80D8508;
    GUEST_CALL(battleObject_dispatchActionHandler_801B394);

    GUEST_CALL(object_presentCollisionData);
    GUEST_CALL(object_updateSpriteTimestop);

    LR = LD32(SP);
    SP += 4;
    RETURN_TO_CALLER();
}

/* ── sub_80D852C ── */
/* sub_80D852C (r5 = battle object): called when a custom-open object should
 * start acting on its own.
 * - If the battle is in a state where objects may act (battle_getFlags bit 0),
 *   clear the "wait" flag (bit 2), set the "active" flag (bit 1), and restart
 *   the object at action 8 / phase 0.
 * - Otherwise just call the empty stub.
 * Leaves r1 = 2 on the acting path, r1 = 1 on the stub path. */
Next sub_80D852C(void) {
    GUEST_CALL(battle_getFlags);
    if (R0 & 1) {
        u32 flags = LD8(R5) & ~4u;      /* oObjectHeader_Flags: clear bit 2 */
        ST8(R5, flags);
        flags = LD8(R5) | 2u;           /* set bit 1 */
        ST8(R5, flags);
        R1 = 2;

        ST8(R5 + oBattleObject_CurAction, 8);
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    } else {
        R1 = 1;
        GUEST_CALL(nullsub_10);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80D8558 ── */
Next sub_80D8558(void) {
    /* Tail call: return to the address in LR without doing any work. */
    RETURN_TO_CALLER();
}

/* ── sub_80D85F0 ── */
/* sub_80D85F0(r0 = panel X, r1 = panel Y, r2 = element, r4 = alliance,
 *             r6 = damage/stamina counter, r7 = extra vars):
 * Spawn a battle object of type 0x98 via object_spawnType3 (which returns the
 * new object in r5) and fill in its position, element, alliance and misc
 * fields. Marks the object header as "in use" (flags |= 0x14).
 * Returns r0 = the spawned object, or 0 if none was spawned.
 * r5 (the object register the callee reports through) is handed back to the
 * caller with its original value restored; r4/r6/r7 pass through untouched. */
Next sub_80D85F0(void) {
    u32 panelX    = R0;
    u32 panelY    = R1;
    u32 element   = R2;
    u32 alliance  = R4;
    u32 damageCtr = R6;
    u32 extraVars = R7;
    u32 callerR5  = R5;

    R0 = 0x98;
    GUEST_CALL(object_spawnType3);
    u32 object = R5;
    R5 = callerR5;

    if (object != 0) {
        ST8(object + oBattleObject_PanelX, panelX);
        ST8(object + oBattleObject_PanelY, panelY);
        ST8(object + oBattleObject_Element, element);
        ST8(object + oBattleObject_Alliance, alliance);
        ST32(object + oBattleObject_DamageAndStaminaDamageCounterDisabler, damageCtr);
        ST32(object + oBattleObject_ExtraVars, extraVars);
        ST8(object + oObjectHeader_Flags, LD8(object + oObjectHeader_Flags) | 0x14);
    }

    R0 = object;
    RETURN_TO_CALLER();
}

/* ── battle_initObject_80DA834 ── */
struct U32Pair { u32 first, second; };

/* register form: calls object_setCoordinatesFromPanels outside this file, which has no typed C interface yet */
static void call_set_coords_from_panels(void) {
    GUEST_CALL(object_setCoordinatesFromPanels);
}

/* register form: calls battleObject_initSpriteAndCollision_8011504 outside this file, which has no typed C interface yet */
static u32 call_sub_8011504(u32 collision_a, u32 collision_b) {
    R0 = collision_a;
    R1 = collision_b;
    GUEST_CALL(battleObject_initSpriteAndCollision_8011504);
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

/* register form: calls battle_objectDestructOnHitPhase_80DA886 outside this file, which has no typed C interface yet */
static void call_sub_80DA886(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(battle_objectDestructOnHitPhase_80DA886);
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

/* battle_initObject_80DA834(r5 = battle object) -> void (outputs R0-R2, flags from battle_objectDestructOnHitPhase_80DA886) */
Next battle_initObject_80DA834(void) {
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
    case 1 * 4: GUEST_CALL(battleObject_destroyOnCollision_80DFEA8);          break; /* second phase */
    case 2 * 4: GUEST_CALL(object_genericDestroy); break; /* destroyed */
    }

    GUEST_CALL(object_updateSprite);

    RETURN_TO_CALLER();
}

/* ── sub_80DFE64 ── */
/* sub_80DFE64 (r5 = battle object):
 * Finish setting up a battle object: snap it to its panel coordinates, copy
 * Param2 into its timer, then build and publish its collision data. If the
 * collision data cannot be created the object is freed instead. On success
 * the object's state advances to CUR_STATE_UPDATE.
 * Preserves r4-r7 (r7 is only used as scratch by the original) and leaves
 * r5/r8 as they came in. */
Next sub_80DFE64(void) {
    u32 obj = R5;

    /* Place the object on its panels and (re)start its timer from Param2. */
    GUEST_CALL(object_setCoordinatesFromPanels);
    ST16(obj + oBattleObject_Timer, (u16)LD8(obj + oBattleObject_Param2));

    /* Build the collision data; a null result means the object is invalid
     * and must be released. */
    GUEST_CALL(object_createCollisionData);
    if (R0 == 0) {
        GUEST_CALL(object_freeMemory);
        RETURN_TO_CALLER();
    }

    /* Map Param3 to the collision shape id: 0 -> 3, 1 -> 1, else 0. */
    u32 shape;
    switch (LD8(obj + oBattleObject_Param3)) {
    case 0:  shape = 3; break;
    case 1:  shape = 1; break;
    default: shape = 0; break;
    }

    /* Set up the collision box (10 wide, 5 tall) with no hit effect. */
    R1 = 0xA;
    R2 = 5;
    R3 = shape;
    GUEST_CALL(object_setupCollisionData);

    R0 = 0;
    GUEST_CALL(object_setCollisionHitEffect);
    GUEST_CALL(object_clearCollisionRegion);
    GUEST_CALL(object_presentCollisionData);

    /* Hand the object over to its regular update handler. */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
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

/* register form: calls battle_objectInitAction_80E0568 outside this file, which has no typed C interface yet */
/* leftover unused: battle_objectInitAction_80E0568's r0 (t4_0x0_80E0548's callers do not read r0) */
static u32 call_sub_80E0568(u32 state, u32 handler_addr) {
    R0 = state;
    R1 = handler_addr;
    GUEST_CALL(battle_objectInitAction_80E0568);
    return R0;
}

/* register form: calls battle_objectTickTimer_80E05C4 outside this file, which has no typed C interface yet */
/* leftover unused: battle_objectTickTimer_80E05C4's r0 (t4_0x0_80E0548's callers do not read r0) */
static u32 call_sub_80E05C4(u32 state, u32 handler_addr) {
    R0 = state;
    R1 = handler_addr;
    GUEST_CALL(battle_objectTickTimer_80E05C4);
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

/* ── battle_objectInitAction_80E0568 ── */
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

Next battle_objectInitAction_80E0568(void) {
    R0 = battle_object_init_action(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_objectTickTimer_80E05C4 ── */
/* battle_objectTickTimer_80E05C4: Battle-object timer tick – decrement timer, possibly destroy, update sprite. */

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

Next battle_objectTickTimer_80E05C4(void) {
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

/* ── t4_0x2_80E0638 ── */
/* Battle-object state dispatcher (t4_0x2_80E0638).
 *
 * Tail-calls the handler for the battle object's current state through the
 * jump table at off_80E064C.  The table is indexed by the raw CurState byte
 * used as a byte offset, so state 0 selects sub_80E0654 and state 4 selects
 * sub_80E0668; any other byte loads whatever word lies at that offset and
 * branches there, exactly as the original does.
 *
 * The selected handler is entered with r0 = state byte and r1 = its own table
 * entry (Thumb address, as loaded), with r5 still pointing at the battle
 * object, and it returns straight back to this routine's caller (the original
 * reaches it via `mov lr, pc; bx r1`, skipping the `pop {pc}` on the normal
 * return path).  Whatever the handler leaves in r5/r8-r12 is this routine's
 * result, so nothing on the way may touch those registers. */

/* register form: the branch target is read from the state table at run time,
 * so no single wrappered callee covers this call; GUEST_CALL_ADDR performs the
 * indirect call the way the original's `mov lr, pc; bx r1` does. */
static void call_state_handler_t4_0x2_80E0638(u32 state, u32 handler_addr) {
    R0 = state;
    R1 = handler_addr;
    GUEST_CALL_ADDR(handler_addr);
}

static void dispatch_battle_state_t4_0x2_80E0638(u32 battle_obj) {
    u32 state = LD8(battle_obj + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E064C + state);
    call_state_handler_t4_0x2_80E0638(state, handler);
}

/* t4_0x2_80E0638(r5 = battle object): dispatch on CurState; all outputs come
 * from the state handler that runs. */
Next t4_0x2_80E0638(void) {
    dispatch_battle_state_t4_0x2_80E0638(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80E0668 ── */
/* sub_80E0668: dispatch to the handler for a battle object's current phase.
 * The two-entry table at off_80E067C maps oBattleObject_CurPhase (0 or 1) to
 * the phase handler's thumb address:
 *   phase 0 -> sub_80E0684
 *   phase 1 -> battle_object_teardownAndFree_80E06E0
 * The handler is entered with r0 = phase byte and r1 = its own table entry
 * (thumb address, bit 0 set); r5 and r10 pass through to it untouched, and
 * its results in r5/r8/r9/r10/r11/r12 are the results of this function. */

/* register form: the phase handlers are reached through a code address taken
 * from the dispatch table, so no generated call_NAME wrapper covers them.
 * The helper enters the callee exactly as the original does: r0 = phase,
 * r1 = the handler's thumb address, lr = return into this function. */
static void call_phase_handler_sub_80E0668(u32 phase, u32 handler) {
    R0 = phase;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

/* Typed body: pick the handler for the object's current phase and call it.
 * r10 passes through to the callee via the guest register file; the callee's
 * register outputs (r5, r8-r12) are left in place by the call itself. */
static void dispatch_phase(u32 object, u32 r10_pass_through) {
    u32 phase = LD8(object + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80E067C + phase);
    call_phase_handler_sub_80E0668(phase, handler);
}

/* sub_80E0668 (r5 = battle object, r10 = pass-through) -> r5 r8 r9 r10 r11 r12 */
Next sub_80E0668(void) {
    dispatch_phase(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_80E0684 ── */
/* register form: relies on battle_orStateFlags5c_8001382 leaving no value the caller uses */
static void battle_or_state_flags_5c(u32 flags) {
    R0 = flags;
    GUEST_CALL(battle_orStateFlags5c_8001382);
}

/* register form: relies on GetBattleEffects returning its result in r0 (wrapper reads R0) */
static u32 get_battle_effects_sub_80E0684(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: relies on SetScreenFade leaving r2 and r5 (wrapper returns them) */
static void set_screen_fade(u32 fade_kind, u32 color) {
    R0 = fade_kind;
    R1 = color;
    GUEST_CALL(SetScreenFade);
}

/* register form: relies on IsScreenFadeActive returning Z flag based on r0 (caller branches on r0==0) */
static bool is_screen_fade_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    return R0 != 0;
}

/* Decide which screen fade to start based on the current battle state, then poll
 * for fade completion and bump the BattleObject's phase to 4 when it finishes.
 * Original uses the toolkit pointer at r10 for the battle state. */
static void run_phase_init(u32 bo_addr, u32 toolkit_addr) {
    if (LD8(bo_addr + oBattleObject_PhaseInitialized) != 0)
        goto poll;

    /* Phase init block: kick the battle into the screen-fade transition. */
    battle_or_state_flags_5c(0x10);
    u32 effects = get_battle_effects_sub_80E0684();
    u32 state_addr = LD32(toolkit_addr + oToolkit_BattleStatePtr);

    u32 fade_kind;
    if ((effects & 0x400) != 0) {
        u8 unk_1a = LD8(state_addr + oBattleState_Unk_1a);
        fade_kind = (unk_1a > 1) ? 8 : 0;
    } else {
        u8 mode = LD8(LD32(state_addr + oBattleState_BattleSettings) + 5);
        fade_kind = (mode >= 2) ? 8 : 0;
    }
    set_screen_fade(fade_kind, 0x10);

    ST8(A_byte_2036740, 1);
    ST8(bo_addr + oBattleObject_PhaseInitialized, 4);

poll:
    if (is_screen_fade_active())
        return;

    battle_or_state_flags_5c(1);
    ST16(bo_addr + oBattleObject_CurPhaseAndPhaseInitialized, 4);
}

Next sub_80E0684(void) {
    run_phase_init(R5, R10);
    RETURN_TO_CALLER();
}

/* ── battle_object_teardownAndFree_80E06E0 ── */
/* battle_object_teardownAndFree_80E06E0: teardown routine for a battle object.
 * Asks battle_countAliveNextEntries_800AA92 (with argument 0) whether cleanup should be skipped;
 * if it returns nonzero we bail out early. Otherwise it runs
 * battle_orStateFlags5c_8001382(2) and then releases the object whose pointer that call
 * leaves in r0 via object_freeMemory.
 * Flags on the early-exit path reflect the TST of battle_countAliveNextEntries_800AA92's result. */
Next battle_object_teardownAndFree_80E06E0(void) {
    R0 = 0;
    GUEST_CALL(battle_countAliveNextEntries_800AA92);

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

/* ── t4_0x3_80E0710 ── */
/* State dispatch via the jump table at off_80E0724. r5 holds the timefreeze
 * battle object; the handler is selected by CurState (Begin / sub_80E0730 /
 * End). The three handlers share this function's contract, so the table
 * dispatch can be a helper that takes/returns guest state: the trampoline is
 * too small to express otherwise (the handler pointer is read from a table
 * indexed by a guest byte, then tail-called through lr/bx). */
static void timefreeze_dispatch(u32 r3, u32 r4, u32 r5, u32 r6, u32 r7,
                                u32 r8, u32 r9, u32 r10, u32 r11, u32 r12,
                                u32 lr) {
    /* register form: jump table indexed by a guest byte; the only sane way
     * to read the handler address and tail-call it is through a guest-side
     * dispatch that preserves every register across the call. */
    GUEST_CALL_ADDR(LD32(A_off_80E0724 + LD8(r5 + oBattleObject_CurState)));
}

/* t4_0x3_80E0710 (r5 = battle object): timefreeze state trampoline */
Next t4_0x3_80E0710(void) {
    timefreeze_dispatch(R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, LR);
    RETURN_TO_CALLER();
}

/* ── sub_80E07E0 ── */
Next sub_80E07E0(void) {
    /* Save caller-supplied r0..r2 and r5 across object_spawnType4. */
    u32 saved_r0 = R0;
    u32 saved_r1 = R1;
    u32 saved_r2 = R2;
    u32 saved_r5 = R5;

    R0 = 3;
    GUEST_CALL(object_spawnType4);

    /* Original does "mov r0, r5" right after the call (before the pop),
     * so r0 holds the spawn's return value. */
    u32 obj = R5;
    /* Restore r1, r2, r3, r5 from the stack. Note r3 here is loaded from
     * the slot that held saved_r2 at push time (the original's pop {r1-r3,r5}
     * followed push {r0-r2,r5}). */
    R1 = saved_r0;
    R2 = saved_r1;
    R3 = saved_r2;
    R5 = saved_r5;

    if (obj != 0) {
        ST8(obj + oBattleObject_PanelX, (u8)R1);
        ST8(obj + oBattleObject_PanelY, (u8)R2);
        ST8(obj + oBattleObject_Element, (u8)R3);
        ST32(obj + oBattleObject_RelatedObject1Ptr, saved_r5);
        u32 alliance = LD16(saved_r5 + oBattleObject_AllianceAndDirectionFlip);
        ST16(obj + oBattleObject_AllianceAndDirectionFlip, (u16)alliance);
        ST32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler, R6);
        ST32(obj + oBattleObject_Unk_30, R7);
    }
    R0 = obj;
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

/* ── sub_80E0864 ── */
/* sub_80E0864: initialize a battle object's sprite/animation state from a
 * sprite descriptor table indexed by Param1. Sets flag 0x2 on the object
 * header and arms CUR_STATE_UPDATE. */
Next sub_80E0864(void) {
    /* Pick the sprite descriptor: off_80E0804 + Param1 * 4. */
    u32 desc = A_off_80E0804 + ((u32)LD8(R5 + oBattleObject_Param1) << 2);

    /* Load graphics, then drop the default shadow. */
    R0 = 0x80;
    R1 = LD8(desc + 0);
    R2 = LD8(desc + 1);
    GUEST_CALL(sprite_load);

    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    /* Install the first frame and its palette, then commit. */
    u32 anim = LD8(desc + 2);
    ST8(R5 + oBattleObject_CurAnim, anim);
    ST8(R5 + oBattleObject_CurAnimCopy, anim);

    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);

    R0 = LD8(desc + 3);
    GUEST_CALL(sprite_setPalette);
    GUEST_CALL(sprite_update);

    /* Mark the object as initialized (flag 0x2) and arm the update state. */
    ST8(R5 + oObjectHeader_Flags, LD8(R5 + oObjectHeader_Flags) | 0x2);
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

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
