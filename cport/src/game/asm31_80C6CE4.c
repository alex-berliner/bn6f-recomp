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

/* ── battle_initShadowlessObject ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battle_initShadowlessObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r4, r5; } call_sprite_load_result_battle_initShadowlessObject;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battle_initShadowlessObject call_sprite_load_battle_initShadowlessObject(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battle_initShadowlessObject){ R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battle_initShadowlessObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battle_initShadowlessObject(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r1, r2, r4, r5; } call_sprite_loadAnimationData_result_battle_initShadowlessObject;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battle_initShadowlessObject call_sprite_loadAnimationData_battle_initShadowlessObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battle_initShadowlessObject){ R1, R2, R4, R5 };
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battle_initShadowlessObject;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battle_initShadowlessObject call_sprite_setPalette_battle_initShadowlessObject(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battle_initShadowlessObject){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_battle_initShadowlessObject;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battle_initShadowlessObject call_object_getFlip_battle_initShadowlessObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battle_initShadowlessObject){ R0 };
}

typedef struct { u32 r2; bool c, v; } call_sprite_setFlip_result_battle_initShadowlessObject;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_battle_initShadowlessObject call_sprite_setFlip_battle_initShadowlessObject(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_battle_initShadowlessObject){ R2, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_sub_80C6E50_result;
/* register form: generated wrapper: sub_80C6E50 has no typed C interface yet */
static inline call_sub_80C6E50_result call_sub_80C6E50(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80C6E50);
    return (call_sub_80C6E50_result){ R5, R8, R9, R10, R11, R12 };
}

/* Initialize a shadowless battle object: set coordinates, load sprite (no shadow),
 * bump Z/Y by 1, flag bit 1, reset animation to frame 0 with palette 0, apply
 * current flip, then enter CUR_STATE_UPDATE and hand off to sub_80C6E50. */
static call_sub_80C6E50_result battle_initShadowlessObject_impl(u32 obj) {
    /* Position the object from its current panel location. */
    call_object_setCoordinatesFromPanels_battle_initShadowlessObject(obj);

    /* Z16 = 0 then +1  =>  Z16 = 1;  Y16 += 1 (16-bit wrap). */
    ST16(obj + oBattleObject_Z16, 1);
    ST16(obj + oBattleObject_Y16, (u16)(LD16(obj + oBattleObject_Y16) + 1));

    /* Load the sprite (a1=0x80, a2=0x10, a3=0x20) and disable its shadow. */
    (void)call_sprite_load_battle_initShadowlessObject(0x80, 0x10, 0x20, obj);
    call_sprite_noShadow_battle_initShadowlessObject(obj);

    /* Set flag bit 1 (0x2) on the object header. */
    ST8(obj + oObjectHeader_Flags, (u8)(LD8(obj + oObjectHeader_Flags) | 2));

    /* Reset animation to frame 0, then load animation data and palette 0. */
    ST8(obj + oBattleObject_CurAnim, 0);
    ST8(obj + oBattleObject_CurAnimCopy, 0);
    call_sprite_setAnimation_battle_initShadowlessObject(0, obj);
    (void)call_sprite_loadAnimationData_battle_initShadowlessObject(obj);
    (void)call_sprite_setPalette_battle_initShadowlessObject(0, obj);

    /* Mirror the object's current flip onto its sprite. */
    {
        call_object_getFlip_result_battle_initShadowlessObject flip = call_object_getFlip_battle_initShadowlessObject(obj);
        (void)call_sprite_setFlip_battle_initShadowlessObject(flip.r0, obj);
    }

    /* Advance the state machine to the update phase. */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    /* Hand off; sub_80C6E50 produces the final r5/r8/r9/r10/r11/r12 outputs. */
    return call_sub_80C6E50(obj);
}

/* battle_initShadowlessObject: 0x080C6DFC
 * inputs:  r5 (object), r10, lr
 * outputs: r5, r8, r9, r10, r11, r12  (from sub_80C6E50) */
Next battle_initShadowlessObject(void) {
    call_sub_80C6E50_result res = battle_initShadowlessObject_impl(R5);
    R5 = res.r5;
    R8 = res.r8;
    R9 = res.r9;
    R10 = res.r10;
    R11 = res.r11;
    R12 = res.r12;
    RETURN_TO_CALLER();
}

/* ── battle_spawnDimmingCollisionPath ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_isValidPanel_result_battle_spawnDimmingCollisionPath;
/* register form: generated wrapper: object_isValidPanel has no typed C interface yet */
static inline call_object_isValidPanel_result_battle_spawnDimmingCollisionPath call_object_isValidPanel_battle_spawnDimmingCollisionPath(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_isValidPanel);
    return (call_object_isValidPanel_result_battle_spawnDimmingCollisionPath){ R0 };
}

typedef struct { u32 r0, r2, r3, r4, r5, r7; } call_object_spawnDimmingCollisionRegion_result;
/* register form: generated wrapper: object_spawnDimmingCollisionRegion has no typed C interface yet */
static inline call_object_spawnDimmingCollisionRegion_result call_object_spawnDimmingCollisionRegion(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_spawnDimmingCollisionRegion);
    return (call_object_spawnDimmingCollisionRegion_result){ R0, R2, R3, R4, R5, R7 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_battle_spawnDimmingCollisionPath;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battle_spawnDimmingCollisionPath call_object_getFrontDirection_battle_spawnDimmingCollisionPath(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battle_spawnDimmingCollisionPath){ R0 };
}

/* Spawn dimming collision regions around a battle object's path.
 *
 * Loop 1: checks the three rows at the source column (offsets -1, 0, +1 from panel_y)
 * and spawns a collision region at each valid panel.
 * Loop 2: sweeps forward in the object's facing direction (steps 1..4), spawning a
 * region at each step until the scan reaches back to the source column (then stops).
 * Every spawned region has its Timer field set to the given value.
 *
 * r0 = timer value, r1 = source panel column, r5 = battle object pointer.
 */
static void spawn_dimming_collision_path(u32 timer, u32 source_col, u32 battle_obj) {
    /* Loop 1: three vertical neighbours at the source column. */
    for (s32 row = 0; row <= 2; row++) {
        u32 panel_y = LD8(battle_obj + oBattleObject_PanelY);
        u32 target_y = (u32)((s32)panel_y + row - 1);

        if (call_object_isValidPanel_battle_spawnDimmingCollisionPath(source_col, target_y).r0 == 0)
            continue;

        u32 element = LD8(battle_obj + oBattleObject_Element);
        u32 dmg = LD32(battle_obj + oBattleObject_DamageAndStaminaDamageCounterDisabler);

        call_object_spawnDimmingCollisionRegion_result spawned =
            call_object_spawnDimmingCollisionRegion(
                source_col, target_y, element, 0,
                0x4050201, battle_obj, dmg, 3);

        ST16(spawned.r0 + oBattleObject_Timer, (u16)timer);
    }

    /* Loop 2: lateral sweep in the facing direction, up to 4 steps. */
    for (s32 step = 1; step <= 4; step++) {
        u32 dir = call_object_getFrontDirection_battle_spawnDimmingCollisionPath(battle_obj).r0;
        u32 panel_x = LD8(battle_obj + oBattleObject_PanelX);
        u32 panel_y = LD8(battle_obj + oBattleObject_PanelY);
        u32 scan_x = dir * (u32)step + panel_x;

        if (scan_x == source_col)
            break;

        u32 element = LD8(battle_obj + oBattleObject_Element);
        u32 dmg = LD32(battle_obj + oBattleObject_DamageAndStaminaDamageCounterDisabler);

        call_object_spawnDimmingCollisionRegion_result spawned =
            call_object_spawnDimmingCollisionRegion(
                scan_x, panel_y, element, 0,
                0x4050201, battle_obj, dmg, 3);

        ST16(spawned.r0 + oBattleObject_Timer, (u16)timer);
    }
}

/* battle_spawnDimmingCollisionPath(r0 = timer, r1 = source_col, r5 = battle_obj)
 * Preserves r4, r6, r7; outputs r5 (= battle_obj, unchanged). */
Next battle_spawnDimmingCollisionPath(void) {
    /* register form: r4, r6, r7 are preserved by this function's contract (the
     * original used push/pop); the wrapper calls inside clobber them. */
    u32 saved_r4 = R4, saved_r6 = R6, saved_r7 = R7;
    spawn_dimming_collision_path(R0, R1, R5);
    R4 = saved_r4; R6 = saved_r6; R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── battle_spawnRowPanelEffect ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6; } call_sub_80E3A92_result;
/* register form: generated wrapper: sub_80E3A92 has no typed C interface yet */
static inline call_sub_80E3A92_result call_sub_80E3A92(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(sub_80E3A92);
    return (call_sub_80E3A92_result){ R5, R6 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_battle_spawnRowPanelEffect;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battle_spawnRowPanelEffect call_object_getFrontDirection_battle_spawnRowPanelEffect(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battle_spawnRowPanelEffect){ R0 };
}

/*
 * Spawns a "row panel" visual effect along the facing direction of a battle object.
 *
 * Draws the effect at the object's starting panel, then walks forward up to
 * 4 tiles, re-drawing at each step until the computed X equals the target X.
 *
 * The packed word handed to sub_80E3A92 encodes:
 *   bits  8-15: effect / arg id
 *   bits 16-23: per-object flag byte (from a small table indexed by Param1)
 *   bit     0 : 1 for the first draw, 0 for subsequent draws
 */
static void spawn_row_panel_effect(u32 arg_id, u32 target_x, u32 arg_r2, u32 battle_obj) {
    /* Per-object flag: byte_80C7028 table indexed by Param1 (only value 4 → 1, else 0). */
    u32 flag = LD8(A_byte_80C7028 + LD8(battle_obj + oBattleObject_Param1));

    u32 word_first = (arg_id << 8) | (flag << 16) | 1;
    u32 word_next  = (arg_id << 8) | (flag << 16);

    /* Draw at the object's current panel (first-draw flag = 1). */
    u32 panel_y = LD8(battle_obj + oBattleObject_PanelY);
    call_sub_80E3A92(target_x, panel_y, arg_r2, 1, word_first,
                     battle_obj, LD32(battle_obj + oBattleObject_RelatedObject1Ptr));

    /* Walk forward up to 4 tiles; stop early when current X reaches the target. */
    for (u32 step = 1; step <= 4; step++) {
        u32 dir   = call_object_getFrontDirection_battle_spawnRowPanelEffect(battle_obj).r0;
        u32 cur_x = LD8(battle_obj + oBattleObject_PanelX) + dir * step;

        if (cur_x == target_x)
            break;

        u32 cur_y = LD8(battle_obj + oBattleObject_PanelY);
        call_sub_80E3A92(cur_x, cur_y, target_x, 0, word_next,
                         battle_obj, LD32(battle_obj + oBattleObject_RelatedObject1Ptr));
    }
}

/* battle_spawnRowPanelEffect(r0 = arg_id, r1 = target_x, r2, r5 = BattleObject*) -> r5 */
Next battle_spawnRowPanelEffect(void) {
    /* register form: original saves r4, r6, r7 on the stack (push/pop);
     * call_sub_80E3A92 writes r4, r7 as arguments and returns r6. */
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;
    spawn_row_panel_effect(R0, R1, R2, R5);
    R4 = saved_r4;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── t3_0x1c_80C7C18 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x1c_80C7C18;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x1c_80C7C18 call_object_updateSpritePaused_t3_0x1c_80C7C18(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x1c_80C7C18){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * t3_0x1c_80C7C18 — one step of a battle object's state machine.
 *
 * The object's CurState byte indexes a small jump table (note: the original
 * uses the raw byte as a *byte* offset into the table, not as a word index),
 * runs the handler for that state with the object in r5, and then always
 * refreshes the object's sprite (object_updateSpritePaused skips animation
 * while the battle is paused). The state handler may replace the object
 * pointer in r5 (e.g. the destroy path), and the sprite update must see the
 * handler's r5, so the indirect call reports it back.
 */

/* register form: indirect call through the state-handler jump table
 * (mov lr, pc; bx handler). Sets up the handler's arguments in the guest
 * register file, runs it, and reports the object pointer it leaves in r5. */
static u32 call_state_handler_t3_0x1c_80C7C18(u32 handler, u32 state, u32 object) {
    R0 = state;
    R1 = handler;
    R5 = object;
    GUEST_CALL_ADDR(handler);
    return R5;
}

/* r5 = battle object. Leaves the resulting r5/r8-r12 behind in the guest
 * register file (the last callee, object_updateSpritePaused, leaves r5, and
 * it preserves r8-r12), so there is nothing to return here. */
static void battle_object_state_step(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80C7C30 + state);

    u32 updated_object = call_state_handler_t3_0x1c_80C7C18(handler, state, object);
    call_object_updateSpritePaused_t3_0x1c_80C7C18(updated_object);
}

Next t3_0x1c_80C7C18(void) {
    u32 return_address = LR;    /* the original saves lr with push and restores it with pop {pc} */
    battle_object_state_step(R5);
    LR = return_address;
    RETURN_TO_CALLER();
}

/* ── battleObject_panelShaker_init_80C7C3C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battleObject_panelShaker_init_80C7C3C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r4, r5; } call_sprite_load_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battleObject_panelShaker_init_80C7C3C call_sprite_load_battleObject_panelShaker_init_80C7C3C(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battleObject_panelShaker_init_80C7C3C){ R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battleObject_panelShaker_init_80C7C3C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_panelShaker_init_80C7C3C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r1, r2, r4, r5; } call_sprite_loadAnimationData_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_panelShaker_init_80C7C3C call_sprite_loadAnimationData_battleObject_panelShaker_init_80C7C3C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_panelShaker_init_80C7C3C){ R1, R2, R4, R5 };
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battleObject_panelShaker_init_80C7C3C call_sprite_setPalette_battleObject_panelShaker_init_80C7C3C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battleObject_panelShaker_init_80C7C3C){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battleObject_panelShaker_init_80C7C3C call_object_getFlip_battleObject_panelShaker_init_80C7C3C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battleObject_panelShaker_init_80C7C3C){ R0 };
}

/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline void call_sprite_setFlip_battleObject_panelShaker_init_80C7C3C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
}

typedef struct { u32 r0, r4; } call_object_createCollisionData_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: object_createCollisionData has no typed C interface yet */
static inline call_object_createCollisionData_result_battleObject_panelShaker_init_80C7C3C call_object_createCollisionData_battleObject_panelShaker_init_80C7C3C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_createCollisionData);
    return (call_object_createCollisionData_result_battleObject_panelShaker_init_80C7C3C){ R0, R4 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result_battleObject_panelShaker_init_80C7C3C call_object_freeMemory_battleObject_panelShaker_init_80C7C3C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result_battleObject_panelShaker_init_80C7C3C){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r4, r6, r7; } call_object_setupCollisionData_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: object_setupCollisionData has no typed C interface yet */
static inline call_object_setupCollisionData_result_battleObject_panelShaker_init_80C7C3C call_object_setupCollisionData_battleObject_panelShaker_init_80C7C3C(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    GUEST_CALL(object_setupCollisionData);
    return (call_object_setupCollisionData_result_battleObject_panelShaker_init_80C7C3C){ R1, R2, R4, R6, R7 };
}

/* register form: generated wrapper: object_setCollisionStatusEffect1 has no typed C interface yet */
static inline void call_object_setCollisionStatusEffect1(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionStatusEffect1);
}

typedef struct { u32 r3; } call_object_setCollisionHitEffect_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: object_setCollisionHitEffect has no typed C interface yet */
static inline call_object_setCollisionHitEffect_result_battleObject_panelShaker_init_80C7C3C call_object_setCollisionHitEffect_battleObject_panelShaker_init_80C7C3C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionHitEffect);
    return (call_object_setCollisionHitEffect_result_battleObject_panelShaker_init_80C7C3C){ R3 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8; } call_object_presentCollisionData_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battleObject_panelShaker_init_80C7C3C call_object_presentCollisionData_battleObject_panelShaker_init_80C7C3C(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battleObject_panelShaker_init_80C7C3C){ R1, R2, R3, R4, R5, R6, R7, R8 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_battleObject_panelShaker_init_80C7C3C;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battleObject_panelShaker_init_80C7C3C call_PlaySoundEffect_battleObject_panelShaker_init_80C7C3C(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battleObject_panelShaker_init_80C7C3C){ R1, R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Initialize a panel-shaker battle object: position, sprite, flags, animation,
 * collision box, and state. Returns nothing; the object pointer and r8/r10
 * are preserved through all callee interactions. */
static void panelShaker_init(u32 object, u32 r8_val, u32 r10_val) {
    /* r10 is implicitly available to callees via the preserved register. */
    (void)r10_val;

    call_object_setCoordinatesFromPanels_battleObject_panelShaker_init_80C7C3C(object);

    /* Zero depth. */
    ST16(object + oBattleObject_Z16, 0);

    /* Load 128×20×20 sprite and disable shadow. */
    call_sprite_load_battleObject_panelShaker_init_80C7C3C(0x80, 0x14, 0x14, object);
    call_sprite_noShadow_battleObject_panelShaker_init_80C7C3C(object);

    /* Set flag bit 2 on the object header. */
    ST8(object + oObjectHeader_Flags, LD8(object + oObjectHeader_Flags) | 2);

    /* Reset animation index, load first animation, palette 0. */
    ST8(object + oBattleObject_CurAnim, 0);
    ST8(object + oBattleObject_CurAnimCopy, 0);
    call_sprite_setAnimation_battleObject_panelShaker_init_80C7C3C(0, object);
    call_sprite_loadAnimationData_battleObject_panelShaker_init_80C7C3C(object);
    call_sprite_setPalette_battleObject_panelShaker_init_80C7C3C(0, object);

    /* Mirror the object's flip state onto the sprite. */
    call_sprite_setFlip_battleObject_panelShaker_init_80C7C3C(call_object_getFlip_battleObject_panelShaker_init_80C7C3C(object).r0, object);

    /* Allocate collision rectangle; abort if allocation fails. */
    call_object_createCollisionData_result_battleObject_panelShaker_init_80C7C3C coll = call_object_createCollisionData_battleObject_panelShaker_init_80C7C3C(object);
    if (coll.r0 == 0) {
        call_object_freeMemory_battleObject_panelShaker_init_80C7C3C(object);
        return;
    }

    /* Set collision box: wider (0xa) when Param1 == 1, narrower (4) otherwise. */
    u8 param1 = LD8(object + oBattleObject_Param1);
    u32 box_r1;
    if (param1 == 1) {
        box_r1 = call_object_setupCollisionData_battleObject_panelShaker_init_80C7C3C(coll.r0, 0xa, 5, 3, object).r1;
        call_object_setCollisionStatusEffect1(0x10, object);
    } else {
        box_r1 = call_object_setupCollisionData_battleObject_panelShaker_init_80C7C3C(coll.r0, 4, 5, 3, object).r1;
    }

    call_object_setCollisionHitEffect_battleObject_panelShaker_init_80C7C3C(3, object);

    /* Register collision box with the battle field. */
    call_object_presentCollisionData_result_battleObject_panelShaker_init_80C7C3C pres =
        call_object_presentCollisionData_battleObject_panelShaker_init_80C7C3C(box_r1, object, r8_val);

    /* Play sound effect 0x12E (0x2F + 0xFF). */
    call_PlaySoundEffect_battleObject_panelShaker_init_80C7C3C(0x2f + 0xff, pres.r2);

    /* Advance the object to its update phase. */
    ST32(object + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
}

/* battleObject_panelShaker_init_80C7C3C(r5 = object, r8, r10) -> r5 r8 r10 (preserved) */
Next battleObject_panelShaker_init_80C7C3C(void) {
    panelShaker_init(R5, R8, R10);
    RETURN_TO_CALLER();
}

/* ── battle_tickCollisionHighlight_80C7D08 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; } call_object_clearCollisionRegion_result_sub_80C7D08;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_sub_80C7D08 call_object_clearCollisionRegion_sub_80C7D08(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_sub_80C7D08){ R1 };
}

typedef struct { u32 r1, r2, r3; } call_object_highlightPanel_result_sub_80C7D08;
/* register form: generated wrapper: object_highlightPanel has no typed C interface yet */
static inline call_object_highlightPanel_result_sub_80C7D08 call_object_highlightPanel_sub_80C7D08(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_highlightPanel);
    return (call_object_highlightPanel_result_sub_80C7D08){ R1, R2, R3 };
}

/* Advance the battle object's collision-highlight state machine.
 * On first tick (CurPhase==0) it initialises the phase/timers and returns 4.
 * Otherwise it maintains the highlight, decrements both timers, clears the
 * collision region when either expires, and destroys the object when the
 * main timer hits zero.  Returns the current value of r1. */
static u32 object_tick_collision_highlight(u32 obj) {
    /* First entry: set phase to 4, load both timers. */
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        ST8(obj + oBattleObject_CurPhase, 4);
        ST16(obj + oBattleObject_Timer, 0xF);
        ST16(obj + oBattleObject_Timer2, 0xA);
        return 4;
    }

    /* If the object was hit, clear its collision region. */
    u32 collision_data = LD32(obj + oBattleObject_CollisionDataPtr);
    u32 r1;
    if (LD32(collision_data + oCollisionData_FlagsFromCollision) != 0)
        r1 = call_object_clearCollisionRegion_sub_80C7D08(obj).r1;
    else
        r1 = collision_data;  /* overwritten below by highlightPanel */

    /* Re-highlight the current panel. */
    r1 = call_object_highlightPanel_sub_80C7D08(
             LD8(obj + oBattleObject_PanelX),
             LD8(obj + oBattleObject_PanelY)).r1;

    /* Decrement the short timer; clear region when it expires. */
    u32 timer2 = (s32)(u32)LD16(obj + oBattleObject_Timer2) - 1;
    ST16(obj + oBattleObject_Timer2, timer2);
    if ((s32)timer2 <= 0)
        r1 = call_object_clearCollisionRegion_sub_80C7D08(obj).r1;

    /* Decrement the main timer; clear region and destroy when it expires. */
    u32 timer = (s32)(u32)LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, timer);
    if ((s32)timer <= 0) {
        r1 = call_object_clearCollisionRegion_sub_80C7D08(obj).r1;
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    }

    return r1;
}

/* battle_tickCollisionHighlight_80C7D08(r5 = battle object pointer) -> r1 */
Next battle_tickCollisionHighlight_80C7D08(void) {
    R1 = object_tick_collision_highlight(R5);
    RETURN_TO_CALLER();
}

/* ── spawnThunderboltObject_80C7D50 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; } call_object_spawnType3_result_spawnThunderboltObject_80C7D50;
/* register form: generated wrapper: object_spawnType3 has no typed C interface yet */
static inline call_object_spawnType3_result_spawnThunderboltObject_80C7D50 call_object_spawnType3_spawnThunderboltObject_80C7D50(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType3);
    return (call_object_spawnType3_result_spawnThunderboltObject_80C7D50){ R4, R5, R6, R7 };
}

/* Thunderbolt spawner (080C7D50).
 *
 * Entry registers: r0/r1 = panel x/y, r2 = element, r3/r4 = forwarded to the
 * spawner, r5 = object whose alliance the new object inherits, r6 = damage.
 * object_spawnType3 spawns object type 0x1C and returns the new object in r5
 * (0 if the pool was full). When it succeeded, the caller's position/element/
 * damage are written into the new object, its alliance halfword is copied from
 * the source object and header flag 0x10 is set.
 *
 * Exit registers: r0 = new object pointer, r1 = entry r0, r2 = entry r1,
 * r3 = entry r2 (the saved r0-r2 come back shifted one slot through
 * push {r0-r2,r5} / pop {r1-r3,r5}; the stores then reuse r1-r3 as scratch,
 * so on the success path they end as 0x10, the ORR result and the alliance
 * value). r5 is restored to the entry value. Flags: "mov r0, r5" is
 * Thumb ADDS r0, r5, #0, so N/Z come from the pointer and C/V are cleared;
 * on success the final ORRS only rewrites N/Z, leaving C/V at 0.
 * r4/r6/r7 pass unchanged through the callee, which the wrapper call already
 * reproduces, so the adapter leaves them alone. */
#define THUNDERBOLT_TYPE   0x1C   /* object type handed to object_spawnType3 */
#define OBJHDR_FLAG_0x10   0x10   /* header flag set on the spawned object */

typedef struct {
    u32 object;     /* r0: new object pointer, 0 if the spawn failed */
    u32 exit_r1;
    u32 exit_r2;
    u32 exit_r3;
    u32 exit_r5;    /* the caller's r5, restored by the pop */
    u32 flags_val;  /* value the exit N/Z describe (C/V are always clear) */
} thunderbolt_spawn_result;

static thunderbolt_spawn_result spawn_thunderbolt_object(u32 panel_x, u32 panel_y,
        u32 element, u32 spawn_arg3, u32 spawn_arg4,
        u32 source_object, u32 damage)
{
    call_object_spawnType3_result_spawnThunderboltObject_80C7D50 spawned = call_object_spawnType3_spawnThunderboltObject_80C7D50(
        THUNDERBOLT_TYPE, panel_y, element, spawn_arg3, spawn_arg4);
    u32 object = spawned.r5;

    thunderbolt_spawn_result out;
    out.object = object;
    out.exit_r1 = panel_x;      /* pop {r1-r3,r5}: saved caller r0-r2 come */
    out.exit_r2 = panel_y;      /* back shifted into r1-r3 */
    out.exit_r3 = element;
    out.exit_r5 = source_object;
    out.flags_val = object;     /* ADDS object, #0: N/Z from it, C/V cleared */

    if (object == 0)
        return out;             /* spawn failed: register shuffling only */

    ST8(object + oBattleObject_PanelX, panel_x);
    ST8(object + oBattleObject_PanelY, panel_y);
    ST8(object + oBattleObject_Element, element);
    ST32(object + oBattleObject_DamageAndStaminaDamageCounterDisabler, damage);

    u32 alliance = LD16(source_object + oBattleObject_AllianceAndDirectionFlip);
    ST16(object + oBattleObject_AllianceAndDirectionFlip, alliance);

    u32 flags = LD8(object + oObjectHeader_Flags) | OBJHDR_FLAG_0x10;
    ST8(object + oObjectHeader_Flags, flags);

    out.exit_r1 = OBJHDR_FLAG_0x10;  /* mov r1, #0x10 */
    out.exit_r2 = flags;             /* orr r2, r1 result */
    out.exit_r3 = alliance;          /* ldrh scratch */
    out.flags_val = flags;           /* ORRS re-set N/Z; C/V stay clear */
    return out;
}

Next spawnThunderboltObject_80C7D50(void) {
    thunderbolt_spawn_result res =
        spawn_thunderbolt_object(R0, R1, R2, R3, R4, R5, R6);
    R0 = res.object;
    R1 = res.exit_r1;
    R2 = res.exit_r2;
    R3 = res.exit_r3;
    R5 = res.exit_r5;
    SET_FLAGS_ADD(res.flags_val, 0);  /* ADDS val, #0: N/Z from val, C/V = 0 */
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

/* ── battle_flyToPanelPhase4_80C85F4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFrontDirection_result_sub_80C85F4;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_sub_80C85F4 call_object_getFrontDirection_sub_80C85F4(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_sub_80C85F4){ R0 };
}

typedef struct { u32 r1, r2, r3, r5, r6; } call_object_setPanelsFromCoordinates_result_sub_80C85F4;
/* register form: generated wrapper: object_setPanelsFromCoordinates has no typed C interface yet */
static inline call_object_setPanelsFromCoordinates_result_sub_80C85F4 call_object_setPanelsFromCoordinates_sub_80C85F4(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setPanelsFromCoordinates);
    return (call_object_setPanelsFromCoordinates_result_sub_80C85F4){ R1, R2, R3, R5, R6 };
}

typedef struct { u32 r0, r2, r3; bool c, v; } call_object_getPanelParameters_result_sub_80C85F4;
/* register form: generated wrapper: object_getPanelParameters has no typed C interface yet */
static inline call_object_getPanelParameters_result_sub_80C85F4 call_object_getPanelParameters_sub_80C85F4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_getPanelParameters);
    return (call_object_getPanelParameters_result_sub_80C85F4){ R0, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* BIOS Div (svc 6): signed 32-bit quotient of numerator / denominator. */
/* register form: BIOS call reads R0/R1 as inputs and returns quotient in R0 */
static u32 bios_div(u32 numerator, u32 denominator) {
    R0 = numerator;
    R1 = denominator;
    swi_call(6);
    return R0;
}

/* Battle-object fly-to-panel update: initializes velocity on first tick,
 * then integrates position and checks the target panel on later ticks. */
static void sub_80C85F4_logic(u32 obj) {
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        /* First tick: enter flight phase 4, compute X velocity from facing,
         * derive a flight timer, and back-calculate the Z velocity. */
        ST8(obj + oBattleObject_CurPhase, 4);

        u32 direction = call_object_getFrontDirection_sub_80C85F4(obj).r0;
        ST32(obj + oBattleObject_XVelocity, direction * (4u << 16));

        /* Timer = (0x28<<17 - 0x18<<16) / (4<<16)  ≈  14 ticks */
        u32 timer = bios_div((0x28u << 17) - (0x18u << 16), 4u << 16);
        ST16(obj + oBattleObject_Timer, (u16)timer);

        /* ZVelocity = (0x10<<16) / timer  (fixed-point per-tick rise) */
        ST32(obj + oBattleObject_ZVelocity, bios_div(0x10u << 16, timer));
        return;
    }

    /* Later ticks: advance X by velocity, advance Z down by velocity,
     * recompute the panel beneath the object, then tick the timer. */
    s32 x = (s32)LD32(obj + oBattleObject_X) + (s32)LD32(obj + oBattleObject_XVelocity);
    ST32(obj + oBattleObject_X, (u32)x);
    s32 z = (s32)LD32(obj + oBattleObject_Z) - (s32)LD32(obj + oBattleObject_ZVelocity);
    ST32(obj + oBattleObject_Z, (u32)z);

    call_object_setPanelsFromCoordinates_sub_80C85F4(obj);

    s32 remaining = (s32)LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, (u16)remaining);
    if (remaining > 0)
        return;

    /* Timer expired: check whether the panel below is breakable (flag 0x10). */
    u32 params = call_object_getPanelParameters_sub_80C85F4(
        LD8(obj + oBattleObject_PanelX),
        LD8(obj + oBattleObject_PanelY)).r0;

    if (params & 0x10) {
        /* Cracked panel: switch to action 4, reset phase. */
        ST8(obj + oBattleObject_CurAction, 4);
        ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    } else {
        /* Not breakable: destroy the object. */
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    }
}

/* battle_flyToPanelPhase4_80C85F4(r5 = battle-object pointer) -> r5 (preserved) */
Next battle_flyToPanelPhase4_80C85F4(void) {
    sub_80C85F4_logic(R5);
    RETURN_TO_CALLER();
}

/* ── battle_hitSelfAndFrontPanel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r7, r10; } call_PlaySoundEffect_result_battle_hitSelfAndFrontPanel;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battle_hitSelfAndFrontPanel call_PlaySoundEffect_battle_hitSelfAndFrontPanel(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battle_hitSelfAndFrontPanel){ R5, R7, R10 };
}

typedef struct { u32 r5; } call_sub_80C8806_result;
/* register form: generated wrapper: sub_80C8806 has no typed C interface yet */
static inline call_sub_80C8806_result call_sub_80C8806(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(sub_80C8806);
    return (call_sub_80C8806_result){ R5 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_battle_hitSelfAndFrontPanel;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battle_hitSelfAndFrontPanel call_object_getFrontDirection_battle_hitSelfAndFrontPanel(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battle_hitSelfAndFrontPanel){ R0 };
}

typedef struct { u32 r0, r2, r3; bool c, v; } call_object_getPanelParameters_result_battle_hitSelfAndFrontPanel;
/* register form: generated wrapper: object_getPanelParameters has no typed C interface yet */
static inline call_object_getPanelParameters_result_battle_hitSelfAndFrontPanel call_object_getPanelParameters_battle_hitSelfAndFrontPanel(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_getPanelParameters);
    return (call_object_getPanelParameters_result_battle_hitSelfAndFrontPanel){ R0, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Play the destroy sound, spawn an outgoing element effect on the object's own
 * panel, check the panel one step ahead; if it is "solid" (bit 4), spawn an
 * incoming effect there too.  Finally mark the object DESTROY. */
static void hit_self_and_front_panel(u32 obj_ptr, u32 sfx_extra) {
    /* Sound-effect index: 0x1E + 0xFF = 0x11D. */
    call_PlaySoundEffect_battle_hitSelfAndFrontPanel(0x11D, sfx_extra);

    /* Outgoing effect from the object's own panel (kind = 0x100, i.e. "front"). */
    u32 panel_x  = LD8(obj_ptr + oBattleObject_PanelX);
    u32 panel_y  = LD8(obj_ptr + oBattleObject_PanelY);
    u32 element  = LD8(obj_ptr + oBattleObject_Element);
    u32 param1   = LD8(obj_ptr + oBattleObject_Param1);
    u32 disabler = LD32(obj_ptr + oBattleObject_DamageAndStaminaDamageCounterDisabler);

    call_sub_80C8806(panel_x, panel_y, element,
                     0x100, param1 | 0x100, obj_ptr, disabler);

    /* Determine the panel one step in the facing direction. */
    u32 dir      = call_object_getFrontDirection_battle_hitSelfAndFrontPanel(obj_ptr).r0;
    u32 target_x = dir + LD8(obj_ptr + oBattleObject_PanelX);
    u32 target_y = LD8(obj_ptr + oBattleObject_PanelY);

    /* Query that panel; bit 4 (0x10) means "solid" – spawn an incoming
     * effect (kind = 0, no 0x100 flag) only when the panel is solid. */
    if (call_object_getPanelParameters_battle_hitSelfAndFrontPanel(target_x, target_y).r0 & 0x10) {
        element  = LD8(obj_ptr + oBattleObject_Element);
        param1   = LD8(obj_ptr + oBattleObject_Param1);
        disabler = LD32(obj_ptr + oBattleObject_DamageAndStaminaDamageCounterDisabler);

        call_sub_80C8806(target_x, target_y, element,
                         0, param1, obj_ptr, disabler);
    }

    /* Transition to DESTROY state. */
    ST32(obj_ptr + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
         CUR_STATE_DESTROY);
}

/* battle_hitSelfAndFrontPanel(r2 = sfx extra, r5 = object ptr)
 * -> preserves r4, r5, r6, r7, r10 */
Next battle_hitSelfAndFrontPanel(void) {
    /* register form: call_sub_80C8806 uses r4/r6 as input parameters and the
     * callee clobbers them; the original preserves them via push/pop at entry/exit. */
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    hit_self_and_front_panel(R5, R2);
    R4 = saved_r4;
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── t3_0x28_80C8FFC ── */
/* t3_0x28_80C8FFC: update step for the type-0x28 battle object.
 *
 * Reads the object's current state and runs the matching state handler.
 * The state byte is stored pre-multiplied by 4, so it indexes the handler
 * table at off_80C9014 (one word per state) directly:
 *   state 0  -> battleObject_initSnakeArmMover_80C9024
 *   state 4  -> battleObject_movingObjectHitStep_80C9074
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
        GUEST_CALL(battleObject_initSnakeArmMover_80C9024);
        break;
    case 4:
        GUEST_CALL(battleObject_movingObjectHitStep_80C9074);
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

/* ── battleObject_initSnakeArmMover_80C9024 ── */
/* battleObject_initSnakeArmMover_80C9024: initialize a battle object sprite, collision data and entry state.
 * Inputs:  r5 = battle object ptr
 * Outputs: r5, r8 unchanged
 */
Next battleObject_initSnakeArmMover_80C9024(void) {
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

/* ── t3_0x29_80C91A0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x29_80C91A0;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x29_80C91A0 call_object_updateSpritePaused_t3_0x29_80C91A0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x29_80C91A0){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * Battle object per-state update (0x080C91A0).
 *
 * Reads the battle object's CurState byte and calls the matching state handler
 * through the jump table at off_80C91B8 (handlers: sub_80C91C4, battleObject_dispatchActionByParam1_80C91FE,
 * object_genericDestroy), passing the object pointer in r5. When the handler
 * returns, object_updateSpritePaused advances the object's sprite animation
 * (it does nothing while the battle is paused).
 */

/* register form: the state handler is reached through a jump table
 * (mov lr, pc; bx rN) and runs as guest code; it may change r5, which the
 * follow-up updateSpritePaused call must see, so read r5 back after the call. */
static u32 call_object_state_handler_t3_0x29_80C91A0(u32 handler, u32 curState, u32 argR2, u32 argR4,
                                     u32 object, u32 argR6, u32 argR7) {
    R0 = curState;  /* state byte the dispatcher read from the object */
    R1 = handler;   /* the original jumps with r1 still holding the table entry */
    R2 = argR2;
    R4 = argR4;
    R5 = object;
    R6 = argR6;
    R7 = argR7;
    GUEST_CALL_ADDR(handler);
    return R5;      /* object pointer as the handler left it */
}

/* Runs the state handler for the object in `object`, then its sprite update.
 * Returns the object pointer as the original leaves it in r5. */
static u32 update_battle_object_state(u32 argR2, u32 argR4, u32 object, u32 argR6, u32 argR7) {
    u32 curState = LD8(object + oBattleObject_CurState);

    /* The table is indexed by the raw state byte (ldr r1, [r1, r0]). */
    u32 handler = LD32(A_off_80C91B8 + curState);

    u32 handledObject = call_object_state_handler_t3_0x29_80C91A0(handler, curState, argR2, argR4,
                                                  object, argR6, argR7);

    call_object_updateSpritePaused_result_t3_0x29_80C91A0 sprite =
        call_object_updateSpritePaused_t3_0x29_80C91A0(handledObject);
    return sprite.r5;
}

/* t3_0x29_80C91A0(inputs: r2 r4 r5 r6 r7 r10 lr; outputs: r5 r8-r12) */
Next t3_0x29_80C91A0(void) {
    R5 = update_battle_object_state(R2, R4, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── battle_spawnAdjacentPanelHits_80C92F4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_isValidPanel_result_sub_80C92F4;
/* register form: generated wrapper: object_isValidPanel has no typed C interface yet */
static inline call_object_isValidPanel_result_sub_80C92F4 call_object_isValidPanel_sub_80C92F4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_isValidPanel);
    return (call_object_isValidPanel_result_sub_80C92F4){ R0 };
}

/* register form: generated wrapper: object_highlightPanel has no typed C interface yet */
static inline void call_object_highlightPanel_sub_80C92F4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_highlightPanel);
}

typedef struct { u32 r0, r2, r3, r5, r7; } call_object_spawnCollisionRegion_result_sub_80C92F4;
/* register form: generated wrapper: object_spawnCollisionRegion has no typed C interface yet */
static inline call_object_spawnCollisionRegion_result_sub_80C92F4 call_object_spawnCollisionRegion_sub_80C92F4(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_spawnCollisionRegion);
    return (call_object_spawnCollisionRegion_result_sub_80C92F4){ R0, R2, R3, R5, R7 };
}

/* Subroutine at 0x080C92F4.
 *
 * Uses the caller's object id (r0) to pick one entry out of a per-alliance
 * table; each entry holds two signed (dx,dy) panel offsets.  For each of the
 * two offsets:
 *   - compute the world panel next to the battle object (r5),
 *   - if it is a valid panel, highlight it and spawn a collision region
 *     carrying the object's element, damage data and the timer from r1,
 *     then store the timer into the spawned object.
 *
 * Register quirks of the original:
 *   - it saves r4/r6/r7 on entry, but the "save the timer" slot at
 *     [sp,#4] lands exactly on the saved r4, so r4 comes back as the
 *     timer argument (r1), NOT as the r4 the caller passed in;
 *   - r6 and r7 are restored normally (r6 additionally survives the
 *     spawn call via its own push/pop pair);
 *   - r2 ends up as the second spawn's r2, or the last checked panel x
 *     when that panel was invalid;
 *   - r5 is left untouched.
 */
typedef struct {
    u32 r2;
    u32 r4;
    u32 r6;
    u32 r7;
} panel_attack_result;

static panel_attack_result panel_attack(u32 object_id, u32 timer,
                                        u32 battle_obj,
                                        u32 r6_in, u32 r7_in) {
    /* byte_80C93C4[alliance*4 + object_id] selects a 4-byte (dx0,dy0,dx1,dy1)
     * entry in the signed offset table at byte_80C93B0. */
    u32 alliance  = LD8(battle_obj + oBattleObject_Alliance);
    u32 pair_slot = LD8(A_byte_80C93C4 + alliance * 4 + object_id);
    u32 pair_base = A_byte_80C93B0 + pair_slot * 4;

    u32 r2_out = 0;

    for (s32 i = 0; i <= 1; i++) {
        s32 dx = LDS8(pair_base + i * 2);
        s32 dy = LDS8(pair_base + i * 2 + 1);

        u32 panel_x = LD8(battle_obj + oBattleObject_PanelX);
        u32 panel_y = LD8(battle_obj + oBattleObject_PanelY);
        u32 world_x = (u32)(dx + (s32)panel_x);
        u32 world_y = (u32)(dy + (s32)panel_y);

        r2_out = panel_x;   /* r2 still holds the panel x if the check fails */

        if (call_object_isValidPanel_sub_80C92F4(world_x, world_y).r0 == 0)
            continue;

        call_object_highlightPanel_sub_80C92F4(world_x, world_y);

        u32 element = LD8(battle_obj + oBattleObject_Element);
        u32 damage  = LD32(battle_obj + oBattleObject_DamageAndStaminaDamageCounterDisabler);

        call_object_spawnCollisionRegion_result_sub_80C92F4 region =
            call_object_spawnCollisionRegion_sub_80C92F4(world_x, world_y, element, 0,
                                             0x4050201, battle_obj, damage, 3);
        ST16(region.r0 + oBattleObject_Timer, timer);
        r2_out = region.r2;
    }

    /* The pop restores r4 from the slot the timer was stored over. */
    panel_attack_result out = { r2_out, timer, r6_in, r7_in };
    return out;
}

/* battle_spawnAdjacentPanelHits_80C92F4(r0 = object id, r1 = timer, r5 = battle object)
 * -> r2, r4 (= timer), r5, r6, r7 (r6/r7 unchanged). */
Next battle_spawnAdjacentPanelHits_80C92F4(void) {
    panel_attack_result res = panel_attack(R0, R1, R5, R6, R7);
    R2 = res.r2;
    R4 = res.r4;
    R5 = R5;            /* untouched by the original */
    R6 = res.r6;
    R7 = res.r7;
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

/* ── t3_0x2b_80C96A0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1; bool z, n; } call_battle_isTimeStop_result;
/* register form: generated wrapper: battle_isTimeStop has no typed C interface yet */
static inline call_battle_isTimeStop_result call_battle_isTimeStop(void) {
    GUEST_CALL(battle_isTimeStop);
    return (call_battle_isTimeStop_result){ R0, R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_t3_0x2b_80C96A0;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_t3_0x2b_80C96A0 call_object_updateSprite_t3_0x2b_80C96A0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_t3_0x2b_80C96A0){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Shield battle object, run once per frame (task handler at 080C96A0).
 *
 * The object's CurState indexes a three-entry jump table of per-state
 * routines: 0 -> battleObject_initElement_80C96C8, 1 -> sub_80C9726, 2 -> object_freeMemory.
 * After the selected routine runs, the sprite is refreshed unless time
 * stop is currently active. */

/* Invoke the state handler selected by CurState (r5 = object).
 * register form: the handler leaves its results in r8-r12 and an updated
 * r5, which typed C cannot capture; GUEST_CALL_ADDR runs it through the
 * code address the original reaches with `mov lr, pc; bx r1`, passing
 * r0 = state and r1 = handler just like the assembly does. */
static u32 shield_run_state_handler(u32 object, u32 state)
{
    u32 handler = LD32(A_off_80C96BC + state);

    R0 = state;
    R1 = handler;
    R5 = object;
    GUEST_CALL_ADDR(handler);
    return R5;                      /* object pointer as the handler left it */
}

/* Typed body: object pointer in, object pointer out (as the callees leave
 * it).  r8-r12 are likewise left behind by the state handler itself. */
static u32 shield_tick(u32 object)
{
    u32 state = LD8(object + oBattleObject_CurState);

    u32 obj = shield_run_state_handler(object, state);

    /* battle_isTimeStop sets Z when time stop is not active; the original
     * branches past the sprite update (bne) when Z is clear. */
    if (call_battle_isTimeStop().z)
        call_object_updateSprite_t3_0x2b_80C96A0(obj);

    return obj;
}

/* t3_0x2b_80C96A0(r5 = shield object, r10 = battle context) -> r5 (r8-r12 as left by the callees) */
Next t3_0x2b_80C96A0(void)
{
    R5 = shield_tick(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_initElement_80C96C8 ── */
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

/* battleObject_initElement_80C96C8(r5 = battle object, r10/lr = standard) -> r5 r8 r9 r10 r11 r12 */
Next battleObject_initElement_80C96C8(void) {
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

/* ── t3_0x32_80CA2A8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_t3_0x32_80CA2A8;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_t3_0x32_80CA2A8 call_object_updateSprite_t3_0x32_80CA2A8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_t3_0x32_80CA2A8){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * Battle-object update, object type 3 (t3_0x32).
 *
 * The object's CurState byte selects one of three per-state routines through
 * the jump table at off_80CA2C0; afterwards the object's sprite is refreshed
 * with object_updateSprite.  The state routines leave their sprite setup in
 * r5/r8/r9/r10/r11/r12, which becomes this function's output.
 */

/* Results of a per-state handler: the sprite registers it leaves behind. */
typedef struct {
    u32 r5, r8, r9, r10, r11, r12;
} StateHandlerRegs_t3_0x32_80CA2A8;

/* register form: the handler is dispatched through a code address (the
 * original's mov lr, pc / bx r1 sequence), so it must be reached with
 * GUEST_CALL_ADDR, and its results live in r5/r8/r9/r10/r11/r12, which a
 * typed call cannot carry.  r2/r3 pass through untouched, as in the original. */
static StateHandlerRegs_t3_0x32_80CA2A8 run_state_handler_t3_0x32_80CA2A8(u32 handler, u32 state, u32 object,
                                          u32 r4, u32 r7, u32 r8, u32 r10)
{
    StateHandlerRegs_t3_0x32_80CA2A8 out;

    R0 = state;
    R1 = handler;
    R4 = r4;
    R5 = object;
    R7 = r7;
    R8 = r8;
    R10 = r10;
    GUEST_CALL_ADDR(handler);

    out.r5 = R5;
    out.r8 = R8;
    out.r9 = R9;
    out.r10 = R10;
    out.r11 = R11;
    out.r12 = R12;
    return out;
}

/* Final register state of the whole update (the function's outputs). */
typedef struct {
    u32 r5, r8, r9, r10, r11, r12;
} Type3UpdateRegs;

/* Dispatch on the object's current state, then refresh its sprite. */
static Type3UpdateRegs update_type3_object(u32 object, u32 r4, u32 r7,
                                           u32 r8, u32 r10)
{
    Type3UpdateRegs out;

    u32 state = LD8(object + oBattleObject_CurState);

    /* The state byte indexes the word table directly, so the states used by
     * this object type are 0, 4 and 8. */
    u32 handler = LD32(A_off_80CA2C0 + state);

    StateHandlerRegs_t3_0x32_80CA2A8 st = run_state_handler_t3_0x32_80CA2A8(handler, state, object,
                                            r4, r7, r8, r10);

    /* object_updateSprite keeps r10 as it is and only rewrites r0-r5. */
    call_object_updateSprite_result_t3_0x32_80CA2A8 sp = call_object_updateSprite_t3_0x32_80CA2A8(st.r5);

    out.r5 = sp.r5;
    out.r8 = st.r8;
    out.r9 = st.r9;
    out.r10 = st.r10;
    out.r11 = st.r11;
    out.r12 = st.r12;
    return out;
}

/* t3_0x32_80CA2A8(r4, r5 = object, r7, r8, r10) -> r5 r8 r9 r10 r11 r12 */
Next t3_0x32_80CA2A8(void) {
    Type3UpdateRegs out = update_type3_object(R5, R4, R7, R8, R10);
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
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
        case 4: GUEST_CALL(battleObject_updatePanelBreakBounce_80CA61A);           break;
        case 8: GUEST_CALL(object_genericDestroy);  break;
    }

    GUEST_CALL(object_updateSprite);
}

/* t3_0x33_80CA544(r5 = battle object ptr) -> all registers */
Next t3_0x33_80CA544(void) {
    dispatch_battle_state(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_updatePanelBreakBounce_80CA61A ── */
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

/* battleObject_updatePanelBreakBounce_80CA61A: collision tick for the battle object at r5.
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
Next battleObject_updatePanelBreakBounce_80CA61A(void) {
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
 *   off_80CABC8 (0 -> object_stepWaitTimer, 1 -> battle_aiRandomMoveStep) and that handler is
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
        GUEST_CALL(object_stepWaitTimer);
    else if (handler == 0x080CAC01)
        GUEST_CALL(battle_aiRandomMoveStep);
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

/* ── battleObject_updateCollisionAndState_80CADE0 ── */
/* battleObject_updateCollisionAndState_80CADE0:
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
Next battleObject_updateCollisionAndState_80CADE0(void) {
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

    /* Action jump table (off_80CAFC4) has a single entry: battleObject_throwProjectile_80CAFC8 for action 0. */
    GUEST_CALL(battleObject_throwProjectile_80CAFC8);

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

/* ── t3_0x3d_80CB900 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x3d_80CB900;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x3d_80CB900 call_object_updateSpritePaused_t3_0x3d_80CB900(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x3d_80CB900){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t3_0x3d_80CB900 — battle-object state handler, template 3 state 0x3D.
 *
 * The object's CurState byte selects one of three handlers through a jump
 * table; the handler runs with r0 = CurState and the object pointer still in
 * r5 (the handlers keep it there, which is why r5 is an output of this
 * function).  Control then falls into the shared tail: object_updateSpritePaused
 * is called on the same object before returning to the caller.
 */

/* Table at off_80CB918, indexed by CurState (byte offset, unscaled):
 *   state 0 -> battleObject_beastSpOut_init_80CB964, state 1 -> battleObject_handleCollisionFrame_80CB9E2, state 2 -> battle_effectPlaySoundThenDestroy. */
#define STATE_HANDLER_TABLE A_off_80CB918

/* register form: the per-state handlers are only reachable through the jump
 * table, so this call must go through a code address.  Guest register side
 * effects of the handler (including anything it leaves in r5/r9/r11/r12)
 * persist, exactly as in the original's mov lr, pc / bx r1 dispatch. */
static u32 call_state_handler_t3_0x3d_80CB900(u32 state) {
    u32 handler = LD32(STATE_HANDLER_TABLE + state);
    R0 = state;                       /* handlers receive CurState in r0 */
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Run the state-specific handler, then the shared paused-sprite update. */
static void run_state_handler_and_update_sprite(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);

    call_state_handler_t3_0x3d_80CB900(state);
    call_object_updateSpritePaused_t3_0x3d_80CB900(object);
}

Next t3_0x3d_80CB900(void) {
    run_state_handler_and_update_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── battle_clearObjectTimer24_80CBAF2 ── */
/* Zero the halfword counter at offset 0x24 of the object pointed to by r0.
 * The function exists only for this store; r5 is pushed/popped and so comes
 * out unchanged, which the callers rely on (it is listed as an output that
 * is passed straight through). */
static u32 clear_object_counter_24(u32 object, u32 r5_in) {
    ST16(object + 0x24, 0);
    return r5_in;
}

/* battle_clearObjectTimer24_80CBAF2(r0 = object address) -> r5 (unchanged incoming value) */
Next battle_clearObjectTimer24_80CBAF2(void) {
    R5 = clear_object_counter_24(R0, R5);
    RETURN_TO_CALLER();
}

/* ── t3_0x40_80CC0E8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool c, v; } call_handleRenderBlindForSpecificObjectsMaybe_80169BE_result_t3_0x40_80CC0E8;
/* register form: generated wrapper: handleRenderBlindForSpecificObjectsMaybe_80169BE has no typed C interface yet */
static inline call_handleRenderBlindForSpecificObjectsMaybe_80169BE_result_t3_0x40_80CC0E8 call_handleRenderBlindForSpecificObjectsMaybe_80169BE_t3_0x40_80CC0E8(u32 r5) {
    R5 = r5;
    GUEST_CALL(handleRenderBlindForSpecificObjectsMaybe_80169BE);
    return (call_handleRenderBlindForSpecificObjectsMaybe_80169BE_result_t3_0x40_80CC0E8){ R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x40_80CC0E8;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x40_80CC0E8 call_object_updateSpritePaused_t3_0x40_80CC0E8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x40_80CC0E8){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t3_0x40_80CC0E8: per-frame update for this battle object type.
 * Dispatch on the object's CurState through the table at off_80CC104
 * (initLaserShot / dispatchCollisionPhase / clearExtraBackrefAndDestroy),
 * then run the blind-render hook and the (pause-aware) sprite update. */

/* register form: the state handler is reached with mov lr, pc; bx r1, so it
 * must be invoked through its address. It sees the object pointer in r5 and
 * the caller's working registers (r4, r7, r8, r10) exactly as this function
 * received them; the object pointer it leaves in r5 feeds the two calls that
 * follow, so return it. */
static u32 call_state_handler_t3_0x40_80CC0E8(u32 handler, u32 state, u32 r4, u32 obj, u32 r7, u32 r8, u32 r10) {
    R0 = state;
    R1 = handler;
    R4 = r4;
    R5 = obj;
    R7 = r7;
    R8 = r8;
    R10 = r10;
    GUEST_CALL_ADDR(handler);
    return R5;
}

static void t3_update_object_t3_0x40_80CC0E8(u32 r4, u32 obj, u32 r7, u32 r8, u32 r10) {
    /* Jump table indexed by the raw CurState byte, as in the original. */
    u32 state = LD8(obj + oBattleObject_CurState);
    u32 handler = LD32(A_off_80CC104 + state);

    u32 obj_after_state = call_state_handler_t3_0x40_80CC0E8(handler, state, r4, obj, r7, r8, r10);

    call_handleRenderBlindForSpecificObjectsMaybe_80169BE_result_t3_0x40_80CC0E8 blind =
        call_handleRenderBlindForSpecificObjectsMaybe_80169BE_t3_0x40_80CC0E8(obj_after_state);
    call_object_updateSpritePaused_t3_0x40_80CC0E8(blind.r5);
}

/* t3_0x40_80CC0E8(r4, r5 = object, r7, r8, r10) -> r5 r8 r9 r10 r11 r12
 * The result registers are exactly what the callees leave behind, so the
 * adapter passes nothing back itself. */
Next t3_0x40_80CC0E8(void) {
    t3_update_object_t3_0x40_80CC0E8(R4, R5, R7, R8, R10);
    RETURN_TO_CALLER();
}

/* ── battle_objectWaitAndAdvancePhase_80CC208 ── */
/* Tick down a battle object's phase timer and advance its action when it hits
 * zero. If Param4 is 2 the timer is bypassed: the object immediately goes to
 * action 8. Otherwise the first tick arms the timer (PhaseInitialized = 4,
 * Timer = 120 frames, r1 = 4); once the count runs out the object switches to
 * action 4 with a fresh phase.
 * Returns the value the original leaves in r1 (it is only rewritten when the
 * timer is armed; on every other path the caller's r1 passes through). */
static u32 battle_object_phase_tick(u32 obj, u32 r1) {
    if (LD8(obj + oBattleObject_Param4) == 2) {
        ST8(obj + oBattleObject_Param4, 0);
        ST8(obj + oBattleObject_CurAction, 8);
        ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
        return r1;
    }

    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        r1 = 4;
        ST8(obj + oBattleObject_PhaseInitialized, 4);
        ST16(obj + oBattleObject_Timer, 0x78);   /* 120-frame wait */
    }

    /* Decrement and store the new count; the compare is on the full
     * register value, so a wrap from 0 lands below zero and fires. */
    s32 timer = (s32)LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, (u32)timer);
    if (timer > 0)
        return r1;

    ST8(obj + oBattleObject_CurAction, 4);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    return r1;
}

/* battle_objectWaitAndAdvancePhase_80CC208(r5 = battle object, lr) -> r1; flags are clobbered freely. */
Next battle_objectWaitAndAdvancePhase_80CC208(void) {
    R1 = battle_object_phase_tick(R5, R1);
    RETURN_TO_CALLER();
}

/* ── battleObject_rollingCannonPhaseDispatch ── */
/* Rolling-cannon battle object: phase dispatcher (0x080CC23E).
 *
 * Reads the battle object's current phase (r5 = object pointer), looks up
 * that phase's handler in the table at off_80CC250, and calls it with
 * r0 = phase, r1 = handler address, and the dispatcher's r2/r5/r10 passed
 * through unchanged. The `mov lr, pc / bx r1 / pop {pc}` sequence makes the
 * handler return straight back to this dispatcher's caller, so whatever the
 * handler leaves in r1/r5/r8-r12 *is* this function's result and never has
 * to be moved. */

/* register form: dispatches through a jump-table code address (the
 * `mov lr, pc; bx r1` pattern). The handler's register state — including the
 * dispatcher's output registers r1/r5/r8-r12 — must stay in the guest
 * register file, so the call is made directly on the guest registers and
 * nothing is returned. */
static void guest_call_phase_handler(u32 phase, u32 handler, u32 arg2, u32 obj, u32 arg10) {
    R0 = phase;      /* current phase, as the original passes it */
    R1 = handler;    /* handler address is still live in r1 at entry */
    R2 = arg2;
    R5 = obj;
    R10 = arg10;
    GUEST_CALL_ADDR(handler);
}

/* Call the handler for the battle object's current phase. */
static void rolling_cannon_phase_dispatch(u32 obj, u32 arg2, u32 arg10) {
    u32 phase   = LD8(obj + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80CC250 + phase);
    guest_call_phase_handler(phase, handler, arg2, obj, arg10);
}

/* battleObject_rollingCannonPhaseDispatch(r2, r5 = obj, r10) -> r1 r5 r8-r12 */
Next battleObject_rollingCannonPhaseDispatch(void) {
    rolling_cannon_phase_dispatch(R5, R2, R10);
    RETURN_TO_CALLER();
}

/* ── battle_phasePanelHighlight_80CC260 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4; } call_object_highlightPanelRegion_result;
/* register form: generated wrapper: object_highlightPanelRegion has no typed C interface yet */
static inline call_object_highlightPanelRegion_result call_object_highlightPanelRegion(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_highlightPanelRegion);
    return (call_object_highlightPanelRegion_result){ R1, R2, R3, R4 };
}

/* Per-frame update for an object sitting in its "highlight the panel" phase.
 *
 * First entry after the phase starts (PhaseInitialized == 0): set up the
 * animation and timers, and leave 4 in r1 as the result. Afterwards Timer2
 * counts up every frame and every 8th tick the object's panel region is
 * highlighted via object_highlightPanelRegion; Timer counts down from 0x14
 * and once it drops below zero the object moves to phase 4 (which also
 * clears PhaseInitialized, so a later activation re-initialises).
 *
 * Returns r1 exactly as the original leaves it: 4 after initialising,
 * whatever object_highlightPanelRegion left in r1 after the highlight call,
 * and the incoming value unchanged on frames where the call is skipped. */
static u32 panel_highlight_update(u32 obj, u32 r1_in) {
    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        /* First frame of the phase: initialise state. */
        ST8(obj + oBattleObject_PhaseInitialized, 4);
        ST8(obj + oBattleObject_CurAnim, 1);
        ST16(obj + oBattleObject_Timer, 0x14);
        ST16(obj + oBattleObject_Timer2, 0);
        return 4;
    }

    u32 timer2 = LD16(obj + oBattleObject_Timer2) + 1;
    ST16(obj + oBattleObject_Timer2, timer2);

    u32 r1_out = r1_in;
    if ((timer2 & 4) == 0) {
        /* Carry of Timer2>>3 clear: this is the 8th tick, so highlight
         * the panel region this object stands on. */
        call_object_highlightPanelRegion_result highlight =
            call_object_highlightPanelRegion(
                LD8(obj + oBattleObject_PanelX),
                LD8(obj + oBattleObject_PanelY),
                8,
                LD16(obj + oBattleObject_AllianceAndDirectionFlip));
        r1_out = highlight.r1;
    }

    /* Count the phase timer down; phase ends once it goes negative. */
    u32 timer = LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, timer);
    if ((s32)timer < 0)
        ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 4);

    return r1_out;
}

/* battle_phasePanelHighlight_80CC260(r5 = object, r1 passes through) -> r1 */
Next battle_phasePanelHighlight_80CC260(void) {
    R1 = panel_highlight_update(R5, R1);
    RETURN_TO_CALLER();
}

/* ── battle_phaseAppear ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battle_phaseAppear(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r2, r4, r6; } call_object_updateCollisionPanels_result_battle_phaseAppear;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_battle_phaseAppear call_object_updateCollisionPanels_battle_phaseAppear(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_battle_phaseAppear){ R2, R4, R6 };
}

typedef struct { u32 r1, r3; bool c, v; } call_sprite_noShadow_result_battle_phaseAppear;
/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline call_sprite_noShadow_result_battle_phaseAppear call_sprite_noShadow_battle_phaseAppear(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
    return (call_sprite_noShadow_result_battle_phaseAppear){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_phaseAppear: 080CC36C
 * First call on a battle object sets up its appear phase: copies future panel
 * coordinates, fixes Z-order, refreshes collision, starts the appear animation
 * and clears its shadow. Subsequent calls count down the appear timer; when it
 * hits 25 the animation resets, and when it underflows (was 0) the object's
 * action and phase are cleared.
 * Output r1 = sprite_noShadow's r1 on first call, otherwise pass-through. */

#define PHASE_APPEAR_INIT  4
#define APPEAR_Z_ORDER     0x28
#define APPEAR_ANIM        3
#define APPEAR_TIMER       0x1E
#define APPEAR_TIMER_ANIM  0x19

static u32 battle_phaseAppear_impl(u32 obj, u32 in_r1) {
    if (LD8(obj + oBattleObject_PhaseInitialized) != 0) {
        /* Already initialized: count down the appear timer. */
        s32 timer = (s32)LD16(obj + oBattleObject_Timer) - 1;
        ST16(obj + oBattleObject_Timer, (u16)timer);

        if (timer == APPEAR_TIMER_ANIM) {
            ST8(obj + oBattleObject_CurAnim, 0);
        } else if (timer < 0) {
            /* Timer underflowed past zero: abort the phase. */
            ST8(obj + oBattleObject_CurAction, 0);
            ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
        }
        return in_r1;
    }

    /* First appearance: mark initialized, latch future panel coords. */
    ST8(obj + oBattleObject_PhaseInitialized, PHASE_APPEAR_INIT);
    ST8(obj + oBattleObject_PanelX, LD8(obj + oBattleObject_FuturePanelX));
    ST8(obj + oBattleObject_PanelY, LD8(obj + oBattleObject_FuturePanelY));

    call_object_setCoordinatesFromPanels_battle_phaseAppear(obj);

    ST16(obj + oBattleObject_Z16, APPEAR_Z_ORDER);
    call_object_updateCollisionPanels_battle_phaseAppear(obj);

    ST8(obj + oBattleObject_CurAnim, APPEAR_ANIM);
    call_sprite_noShadow_result_battle_phaseAppear shadow = call_sprite_noShadow_battle_phaseAppear(obj);

    ST16(obj + oBattleObject_Timer, APPEAR_TIMER);

    return shadow.r1;
}

Next battle_phaseAppear(void) {
    R1 = battle_phaseAppear_impl(R5, R1);
    RETURN_TO_CALLER();
}

/* ── object_clearExtraBackrefAndDestroy ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5, r6, r7; bool z, n, c, v; } call_object_genericDestroy_result_object_clearExtraBackrefAndDestroy;
/* register form: generated wrapper: object_genericDestroy has no typed C interface yet */
static inline call_object_genericDestroy_result_object_clearExtraBackrefAndDestroy call_object_genericDestroy_object_clearExtraBackrefAndDestroy(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_genericDestroy);
    return (call_object_genericDestroy_result_object_clearExtraBackrefAndDestroy){ R0, R1, R2, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Destroy a battle object after removing a stale back-reference:
 * if the object's extra-var slot points back at the object itself, clear it,
 * then hand the object to the generic destroyer (which produces the r5 output). */
static u32 clear_backref_and_destroy(u32 object) {
    u32 extra = LD32(object + oBattleObject_ExtraVars);

    if (LD32(extra) == object)
        ST32(extra, 0);            /* drop the self back-reference */

    return call_object_genericDestroy_object_clearExtraBackrefAndDestroy(object).r5;
}

/* object_clearExtraBackrefAndDestroy(r5 = object) -> r5 */
Next object_clearExtraBackrefAndDestroy(void) {
    R5 = clear_backref_and_destroy(R5);
    RETURN_TO_CALLER();
}

/* ── t3_0x41_80CC4C4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool c, v; } call_handleRenderBlindForSpecificObjectsMaybe_80169BE_result_t3_0x41_80CC4C4;
/* register form: generated wrapper: handleRenderBlindForSpecificObjectsMaybe_80169BE has no typed C interface yet */
static inline call_handleRenderBlindForSpecificObjectsMaybe_80169BE_result_t3_0x41_80CC4C4 call_handleRenderBlindForSpecificObjectsMaybe_80169BE_t3_0x41_80CC4C4(u32 r5) {
    R5 = r5;
    GUEST_CALL(handleRenderBlindForSpecificObjectsMaybe_80169BE);
    return (call_handleRenderBlindForSpecificObjectsMaybe_80169BE_result_t3_0x41_80CC4C4){ R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_t3_0x41_80CC4C4;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_t3_0x41_80CC4C4 call_object_updateSprite_t3_0x41_80CC4C4(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_t3_0x41_80CC4C4){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Per-state step handlers for a battle object. oBattleObject_CurState stores
 * the byte offset of the handler's entry within this table (0, 4, 8, ...),
 * which is why the original can index it directly with the raw byte. */

/* register form: dispatch through the state handler table (mov lr,pc; bx r1),
 * which no generated wrapper covers. The callee must be entered exactly as the
 * original leaves the registers: r0 = state byte, r1 = handler address, and
 * r2-r5/r8/r10 still holding what this function was entered with. r4-r11 are
 * callee-saved under the game's calling convention, so the object pointer in
 * r5 survives the dispatch and is passed on to the two update calls below. */
static void dispatch_state_handler_t3_0x41_80CC4C4(u32 state, u32 handler) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

/* Run one step of a battle object (r5 = object): call the handler for its
 * current state, then the shared blind-render and sprite-update passes.
 * The register outputs (r5 r8 r9 r10 r11 r12) are whatever the last callee
 * leaves behind; the wrapper calls leave them in the guest registers. */
static void battle_object_step_t3_0x41_80CC4C4(u32 obj) {
    u32 state = LD8(obj + oBattleObject_CurState);

    u32 handler = LD32(A_off_80CC4E0 + state);
    dispatch_state_handler_t3_0x41_80CC4C4(state, handler);

    call_handleRenderBlindForSpecificObjectsMaybe_80169BE_t3_0x41_80CC4C4(obj);
    call_object_updateSprite_t3_0x41_80CC4C4(obj);
}

/* t3_0x41_80CC4C4(r5 = battle object) -> r5 r8 r9 r10 r11 r12 from callees */
Next t3_0x41_80CC4C4(void) {
    battle_object_step_t3_0x41_80CC4C4(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_initFromPanelSpriteAndCollision_80CC4EC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r4, r5; } call_sprite_load_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC call_sprite_load_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC){ R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r4, r5; } call_sprite_loadAnimationData_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC call_sprite_loadAnimationData_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC){ R4, R5 };
}

typedef struct { u32 r1, r2, r4, r5; } call_sprite_update_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC call_sprite_update_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC){ R1, R2, R4, R5 };
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC call_sprite_setPalette_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC call_object_getFlip_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC){ R0 };
}

/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline void call_sprite_setFlip_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
}

typedef struct { u32 r0, r4; } call_object_createCollisionData_result;
/* register form: generated wrapper: object_createCollisionData has no typed C interface yet */
static inline call_object_createCollisionData_result call_object_createCollisionData(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_createCollisionData);
    return (call_object_createCollisionData_result){ R0, R4 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result call_object_freeMemory_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r4, r6, r7; } call_object_setupCollisionData_result;
/* register form: generated wrapper: object_setupCollisionData has no typed C interface yet */
static inline call_object_setupCollisionData_result call_object_setupCollisionData(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    GUEST_CALL(object_setupCollisionData);
    return (call_object_setupCollisionData_result){ R1, R2, R4, R6, R7 };
}

typedef struct { u32 r3; } call_object_setCollisionHitEffect_result;
/* register form: generated wrapper: object_setCollisionHitEffect has no typed C interface yet */
static inline call_object_setCollisionHitEffect_result call_object_setCollisionHitEffect(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionHitEffect);
    return (call_object_setCollisionHitEffect_result){ R3 };
}

typedef struct { u32 r0, r2, r3, r4, r5, r6, r7, r8; bool c, v; } call_object_presentCollisionData_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC call_object_presentCollisionData_battleObject_initFromPanelSpriteAndCollision_80CC4EC(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC){ R0, R2, R3, R4, R5, R6, R7, R8, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Stand a freshly created battle object up on its panel: position it, load
 * the shared panel sprite, restart its animation and attach a collision box.
 * If the collision box cannot be allocated, the object is released again.
 *
 * Register notes: callers observe r0-r3/r5/r8 and the condition flags after
 * this function.  On the normal path those are exactly what the last callee
 * (object_presentCollisionData) left behind, plus r1 = 4 from the final
 * "mov"; on the out-of-memory path they are what object_freeMemory left.
 * So the typed function only reports r1 and the operand the adapter needs
 * for SET_FLAGS_NZ; C/V and every other register stay as the wrappers left
 * them. */
struct battle_object_init_result {
    u32 r1Value;     /* value the original leaves in r1 */
    u32 flagOperand; /* value the adapter feeds to SET_FLAGS_NZ */
};

static struct battle_object_init_result init_battle_object_from_panel_sprite(u32 object, u32 arg8) {
    struct battle_object_init_result out;

    /* Put the object on its panel: clear its Z height, then step one panel
     * forward in both Z and Y. */
    call_object_setCoordinatesFromPanels_battleObject_initFromPanelSpriteAndCollision_80CC4EC(object);
    ST32(object + oBattleObject_Z, 0);
    ST32(object + oBattleObject_Z, LD32(object + oBattleObject_Z) + 1);
    ST32(object + oBattleObject_Y, LD32(object + oBattleObject_Y) + 1);

    /* Load the standing sprite (bank 0x80, index 0x10, size 0x29) and hide
     * its automatically drawn shadow. */
    call_sprite_load_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC load = call_sprite_load_battleObject_initFromPanelSpriteAndCollision_80CC4EC(0x80, 0x10, 0x29, object);
    object = load.r5;
    call_sprite_noShadow_battleObject_initFromPanelSpriteAndCollision_80CC4EC(object);

    /* Mark the object header as owning a loaded sprite (bit 1) and restart
     * its animation from frame 0. */
    ST8(object + oObjectHeader_Flags, LD8(object + oObjectHeader_Flags) | 2);
    ST8(object + oBattleObject_CurAnim, 0);
    ST8(object + oBattleObject_CurAnimCopy, 0);
    call_sprite_setAnimation_battleObject_initFromPanelSpriteAndCollision_80CC4EC(0, object);

    /* Pull in the animation frames, run one sprite update, select palette 0
     * and copy the panel's horizontal flip onto the sprite. */
    call_sprite_loadAnimationData_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC anim = call_sprite_loadAnimationData_battleObject_initFromPanelSpriteAndCollision_80CC4EC(object);
    object = anim.r5;
    call_sprite_update_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC update = call_sprite_update_battleObject_initFromPanelSpriteAndCollision_80CC4EC(object);
    object = update.r5;
    call_sprite_setPalette_battleObject_initFromPanelSpriteAndCollision_80CC4EC(0, object);
    call_sprite_setFlip_battleObject_initFromPanelSpriteAndCollision_80CC4EC(call_object_getFlip_battleObject_initFromPanelSpriteAndCollision_80CC4EC(object).r0, object);

    /* Allocate the collision data; a null pointer means allocation failed. */
    u32 collision = call_object_createCollisionData(object).r0;
    if (collision == 0) {
        /* Out of memory: nothing else to do, release the object again.
         * r0-r3/r5 and all four flags are exactly what object_freeMemory
         * leaves behind. */
        call_object_freeMemory_result freed = call_object_freeMemory_battleObject_initFromPanelSpriteAndCollision_80CC4EC(object);
        out.r1Value = freed.r1;
        /* Pack freeMemory's N and Z into an operand for SET_FLAGS_NZ
         * (C and V are still the ones freeMemory left in place). */
        out.flagOperand = (freed.n ? 0x80000000u : 0u) | (freed.z ? 0u : 1u);
        return out;
    }

    /* Fill in the collision box (extent 0x29, shape 0, type 0), give it the
     * standard white-flash hit effect and hand it to the battle engine. */
    call_object_setupCollisionData_result setup =
        call_object_setupCollisionData(collision, 0x29, 0, 0, object);
    call_object_setCollisionHitEffect(0xFF, object);
    call_object_presentCollisionData_result_battleObject_initFromPanelSpriteAndCollision_80CC4EC present =
        call_object_presentCollisionData_battleObject_initFromPanelSpriteAndCollision_80CC4EC(setup.r1, object, arg8);

    /* The object is fully live now: record state phase 4.  The original's
     * final "mov r1, #4" also clears N and Z. */
    ST32(present.r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, 4);
    out.r1Value = 4;
    out.flagOperand = 4;
    return out;
}

Next battleObject_initFromPanelSpriteAndCollision_80CC4EC(void) {
    struct battle_object_init_result out = init_battle_object_from_panel_sprite(R5, R8);
    R1 = out.r1Value;
    SET_FLAGS_NZ(out.flagOperand);
    RETURN_TO_CALLER();
}

/* ── object_dieOrUpdateCollision_80CC562 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_removeCollisionData_result_object_dieOrUpdateCollision_80CC562;
/* register form: generated wrapper: object_removeCollisionData has no typed C interface yet */
static inline call_object_removeCollisionData_result_object_dieOrUpdateCollision_80CC562 call_object_removeCollisionData_object_dieOrUpdateCollision_80CC562(u32 r4, u32 r5, u32 r7) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(object_removeCollisionData);
    return (call_object_removeCollisionData_result_object_dieOrUpdateCollision_80CC562){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_spawnCollisionEffect_result_object_dieOrUpdateCollision_80CC562;
/* register form: generated wrapper: object_spawnCollisionEffect has no typed C interface yet */
static inline call_object_spawnCollisionEffect_result_object_dieOrUpdateCollision_80CC562 call_object_spawnCollisionEffect_object_dieOrUpdateCollision_80CC562(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_spawnCollisionEffect);
    return (call_object_spawnCollisionEffect_result_object_dieOrUpdateCollision_80CC562){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r3; } call_battle_isBattleOver_result_object_dieOrUpdateCollision_80CC562;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result_object_dieOrUpdateCollision_80CC562 call_battle_isBattleOver_object_dieOrUpdateCollision_80CC562(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result_object_dieOrUpdateCollision_80CC562){ R0, R1, R3 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8; bool z, n, c, v; } call_object_presentCollisionData_result_object_dieOrUpdateCollision_80CC562;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_object_dieOrUpdateCollision_80CC562 call_object_presentCollisionData_object_dieOrUpdateCollision_80CC562(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_object_dieOrUpdateCollision_80CC562){ R0, R1, R2, R3, R4, R5, R6, R7, R8, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_object_dieOrUpdateCollision_80CC562;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_object_dieOrUpdateCollision_80CC562 call_object_clearCollisionRegion_object_dieOrUpdateCollision_80CC562(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_object_dieOrUpdateCollision_80CC562){ R1 };
}

/* Object death / collision-update handler for one battle object.
 *
 * r4/r5/r7 describe the object (r5 is the object pointer, reused as the
 * argument of every callee); r8 is the collision scratch value the callees
 * keep up to date. r10 (the battle toolkit pointer) is not named here: it
 * arrives already set and every callee preserves it, so the generated
 * wrappers see it unchanged.
 *
 * Flow: drop the object's collision data, spawn its death effect, then
 * - battle still running: rebuild the object's collision data, or
 * - battle over: clear the collision region and mark the object destroyed.
 *
 * r5 and r8 are outputs only through the callees (removeCollisionData /
 * spawnCollisionEffect refresh r5, presentCollisionData refreshes r8), so
 * the wrapper calls leave them in place and nothing else writes them.
 */
static void die_or_update_collision(u32 r4, u32 r5, u32 r7, u32 r8) {
    /* Remove collision data; it may retarget the object pointer in r5. */
    r5 = call_object_removeCollisionData_object_dieOrUpdateCollision_80CC562(r4, r5, r7).r5;

    /* Spawn the death effect; it may retarget r5 as well. */
    r5 = call_object_spawnCollisionEffect_object_dieOrUpdateCollision_80CC562(r5).r5;

    call_battle_isBattleOver_result_object_dieOrUpdateCollision_80CC562 over = call_battle_isBattleOver_object_dieOrUpdateCollision_80CC562();
    if (over.r0 == 0) {
        /* Battle still running: refresh this object's collision data.
         * r1 comes back from isBattleOver, r8 passes through unchanged. */
        call_object_presentCollisionData_object_dieOrUpdateCollision_80CC562(over.r1, r5, r8);
    } else {
        /* Battle finished: drop the collision region and mark destroyed. */
        call_object_clearCollisionRegion_object_dieOrUpdateCollision_80CC562(r5);
        ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
             CUR_STATE_DESTROY);
    }
}

Next object_dieOrUpdateCollision_80CC562(void) {
    die_or_update_collision(R4, R5, R7, R8);
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

/* ── t3_0x49_80CD2EC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_t3_0x49_80CD2EC;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_t3_0x49_80CD2EC call_object_updateSprite_t3_0x49_80CD2EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_t3_0x49_80CD2EC){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Task for battle object type 0x49: run the handler selected by the object's
 * CurState (0 = battleObject_initSnakeArmHead, 1 = snakearm_headStateHandler state step,
 * 2 = object_genericDestroy), then update the object's sprite.
 *
 * register form: the state handlers are reached through the CurState jump
 * table and keep the object's live state in the high registers, returning
 * updated r5/r8-r12; no typed wrapper covers them, so the dispatch and the
 * sprite update must run on the real guest registers. */

/* Call the CurState handler through its jump-table entry.
 * The original enters it with r0 = state byte and r1 still holding the table
 * entry (the handler address); r5 and the other state registers pass through. */
/* register form: call through a code address with the object's register-resident
 * state (r5/r8-r12) as the result; no typed wrapper exists for these handlers. */
static void call_curstate_handler_t3_0x49_80CD2EC(u32 handler, u32 cur_state) {
    R0 = cur_state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

/* register form: the state handlers above return the object's updated state in
 * r5/r8-r12, which cannot be captured by a typed function's parameters. */
static void t3_0x49_step(void) {
    u32 object = R5;

    u32 cur_state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80CD304 + cur_state);

    call_curstate_handler_t3_0x49_80CD2EC(handler, cur_state);

    /* Common tail: refresh the sprite from whatever state the handler left. */
    call_object_updateSprite_result_t3_0x49_80CD2EC spr = call_object_updateSprite_t3_0x49_80CD2EC(R5);
    R5 = spr.r5;
}

/* inputs: r4 r5 r7 r8 r10 lr -> outputs: r5 r8 r9 r10 r11 r12 */
Next t3_0x49_80CD2EC(void) {
    t3_0x49_step();
    RETURN_TO_CALLER();
}

/* ── snakearm_headStateHandler ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r4, r5, r6, r7; bool c, v; } call_object_removeCollisionData_result_snakearm_headStateHandler;
/* register form: generated wrapper: object_removeCollisionData has no typed C interface yet */
static inline call_object_removeCollisionData_result_snakearm_headStateHandler call_object_removeCollisionData_snakearm_headStateHandler(u32 r4, u32 r5, u32 r7) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(object_removeCollisionData);
    return (call_object_removeCollisionData_result_snakearm_headStateHandler){ R2, R3, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r3; } call_battle_isBattleOver_result_snakearm_headStateHandler;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result_snakearm_headStateHandler call_battle_isBattleOver_snakearm_headStateHandler(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result_snakearm_headStateHandler){ R0, R3 };
}

/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline void call_object_clearCollisionRegion_snakearm_headStateHandler(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8; bool z, n, c, v; } call_object_presentCollisionData_result_snakearm_headStateHandler;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_snakearm_headStateHandler call_object_presentCollisionData_snakearm_headStateHandler(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_snakearm_headStateHandler){ R0, R1, R2, R3, R4, R5, R6, R7, R8, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Dispatch through the head-action jump table.
 * register form: the call target is only known at runtime (loaded from off_80CD410);
 * the callee (object_stepXAndDestroyIfBlocked) modifies r5 and r8, which the caller
 * needs for the subsequent presentCollisionData call. */
typedef struct { u32 r5; u32 r8; } head_dispatch_result;
static head_dispatch_result head_dispatch_action(u32 target) { /* register form: calls through a guest code address (noted by the tool) */
    GUEST_CALL_ADDR(target);
    return (head_dispatch_result){ .r5 = R5, .r8 = R8 };
}

/* Snake Arm head state handler.
 * Decides whether to destroy the head (collision hit, dead flag, or battle over)
 * or to let it step forward and re-present its collision hitbox. */
static void snakearm_headStateHandler_impl(u32 r4, u32 r5, u32 r7, u32 r8) {
    /* Remove the head's current collision; the callee may reposition r5. */
    call_object_removeCollisionData_result_snakearm_headStateHandler rm = call_object_removeCollisionData_snakearm_headStateHandler(r4, r5, r7);
    r5 = rm.r5;

    bool destroy = false;

    /* If the collision-data block still carries a "hit" flag → kill. */
    u32 extra_vars = LD32(r5 + oBattleObject_ExtraVars);
    if (extra_vars != 0) {
        u32 coll_ptr = LD32(r5 + oBattleObject_CollisionDataPtr);
        if (LD32(coll_ptr + oCollisionData_FlagsFromCollision) != 0)
            destroy = true;
    }

    /* If the extra-data pointer's first byte (health) is 0 → kill. */
    if (!destroy) {
        u32 extra4 = LD32(r5 + (oBattleObject_ExtraVars + 4));
        if (extra4 != 0 && LD8(extra4) == 0)
            destroy = true;
    }

    /* If neither condition met, check whether the battle has ended. */
    if (!destroy) {
        call_battle_isBattleOver_result_snakearm_headStateHandler over = call_battle_isBattleOver_snakearm_headStateHandler();
        if (over.r0 != 0) {
            /* Battle over: zero the head's health byte, then destroy. */
            u32 extra4 = LD32(r5 + (oBattleObject_ExtraVars + 4));
            if (extra4 != 0)
                ST8(extra4, 0);
            destroy = true;
        }
    }

    if (destroy) {
        /* Clear collision region, mark object inactive, transition to Destroy. */
        call_object_clearCollisionRegion_snakearm_headStateHandler(r5);
        u8 flags = LD8(r5 + oObjectHeader_Flags);
        ST8(r5 + oObjectHeader_Flags, flags & ~2u);
        ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
        return;
    }

    /* Alive and battle ongoing: run the head's step-forward action. */
    u8 action = LD8(r5 + oBattleObject_CurAction);
    u32 target = LD32(A_off_80CD410 + action);
    head_dispatch_result disp = head_dispatch_action(target);
    r5 = disp.r5;
    r8 = disp.r8;

    /* Re-present collision; param1 shifted into r1 selects the slot. */
    u32 param1 = LD8(r5 + oBattleObject_Param1);
    call_object_presentCollisionData_snakearm_headStateHandler(param1 << 25, r5, r8);
}

/* 0x080CD3B0(r4, r5, r7, r8, r10) -> r5 r8 r9 r10 r11 r12 */
Next snakearm_headStateHandler(void) {
    snakearm_headStateHandler_impl(R4, R5, R7, R8);
    RETURN_TO_CALLER();
}

/* ── object_stepXAndDestroyIfBlocked ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5; } call_convertBattleObjectXYToPanelXY_800E24C_result_object_stepXAndDestroyIfBlocked;
/* register form: generated wrapper: convertBattleObjectXYToPanelXY_800E24C has no typed C interface yet */
static inline call_convertBattleObjectXYToPanelXY_800E24C_result_object_stepXAndDestroyIfBlocked call_convertBattleObjectXYToPanelXY_800E24C_object_stepXAndDestroyIfBlocked(u32 r5) {
    R5 = r5;
    GUEST_CALL(convertBattleObjectXYToPanelXY_800E24C);
    return (call_convertBattleObjectXYToPanelXY_800E24C_result_object_stepXAndDestroyIfBlocked){ R0, R1, R2, R3, R5 };
}

typedef struct { u32 r0; bool c, v; } call_object_isValidPanel_result_object_stepXAndDestroyIfBlocked;
/* register form: generated wrapper: object_isValidPanel has no typed C interface yet */
static inline call_object_isValidPanel_result_object_stepXAndDestroyIfBlocked call_object_isValidPanel_object_stepXAndDestroyIfBlocked(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_isValidPanel);
    return (call_object_isValidPanel_result_object_stepXAndDestroyIfBlocked){ R0, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool c, v; } call_object_checkPanelParameters_result_object_stepXAndDestroyIfBlocked;
/* register form: generated wrapper: object_checkPanelParameters has no typed C interface yet */
static inline call_object_checkPanelParameters_result_object_stepXAndDestroyIfBlocked call_object_checkPanelParameters_object_stepXAndDestroyIfBlocked(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_checkPanelParameters);
    return (call_object_checkPanelParameters_result_object_stepXAndDestroyIfBlocked){ R0, R1, R2, R3, R4, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline void call_object_clearCollisionRegion_object_stepXAndDestroyIfBlocked(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
}

/* Clear the object's collision region, zero its extra-var pointer, and mark it destroyed. */
static void destroy_object_object_stepXAndDestroyIfBlocked(u32 obj) {
    call_object_clearCollisionRegion_object_stepXAndDestroyIfBlocked(obj);
    u32 ptr = LD32(obj + oBattleObject_ExtraVars + 4);
    if (ptr)
        ST8(ptr, 0);
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
}

/* Advance a battle object along the X axis.  If the target panel is invalid
 * or blocked by alliance parameters, destroy the object. */
static void object_stepXAndDestroyIfBlocked_impl(u32 obj) {
    /* If the collision flag bit 0x800000 is already set, skip the move. */
    u32 collData = LD32(obj + oBattleObject_CollisionDataPtr);
    if (LD32(collData + oCollisionData_FlagsFromCollision) & 0x800000) {
        destroy_object_object_stepXAndDestroyIfBlocked(obj);
        return;
    }

    /* Move X by XVelocity. */
    u32 newX = LD32(obj + oBattleObject_X) + LD32(obj + oBattleObject_XVelocity);
    ST32(obj + oBattleObject_X, newX);

    /* Recompute the panel coordinates from the new world position. */
    call_convertBattleObjectXYToPanelXY_800E24C_result_object_stepXAndDestroyIfBlocked panel =
        call_convertBattleObjectXYToPanelXY_800E24C_object_stepXAndDestroyIfBlocked(obj);
    u32 panelX = panel.r0;
    u32 panelY = panel.r1;

    ST8(obj + oBattleObject_PanelX, panelX);
    ST8(obj + oBattleObject_PanelY, panelY);
    collData = LD32(obj + oBattleObject_CollisionDataPtr);
    ST8(collData + oCollisionData_PanelX, panelX);
    ST8(collData + oCollisionData_PanelY, panelY);

    /* Reject the move if the panel is out of bounds. */
    call_object_isValidPanel_result_object_stepXAndDestroyIfBlocked valid = call_object_isValidPanel_object_stepXAndDestroyIfBlocked(panelX, panelY);
    if (!valid.r0) {
        destroy_object_object_stepXAndDestroyIfBlocked(obj);
        return;
    }

    /* If Param1 bit 7 is set the object moves freely – no further panel checks. */
    if (LD8(obj + oBattleObject_Param1) & 0x80)
        return;

    /* Look up alliance-specific panel parameters from the two-entry table. */
    u32 alliance = LD8(obj + oBattleObject_Alliance);
    u32 tableBase = A_byte_80CD478 + (alliance << 3);
    u32 param2 = LD32(tableBase);
    u32 param3 = LD32(tableBase + 4);

    call_object_checkPanelParameters_result_object_stepXAndDestroyIfBlocked check =
        call_object_checkPanelParameters_object_stepXAndDestroyIfBlocked(panelX, panelY, param2, param3);
    if (!check.r0)
        destroy_object_object_stepXAndDestroyIfBlocked(obj);
    /* Otherwise the move is allowed – fall through to normal return. */
}

/* object_stepXAndDestroyIfBlocked(r5 = battle object) */
Next object_stepXAndDestroyIfBlocked(void) {
    object_stepXAndDestroyIfBlocked_impl(R5);
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

/* ── t3_0x50_80CE530 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_t3_0x50_80CE530;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_t3_0x50_80CE530 call_object_updateSprite_t3_0x50_80CE530(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_t3_0x50_80CE530){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Type-3 battle object, state 0x50 update routine.
 *
 * Dispatches on the object's CurState through the table at off_80CE548
 * (0 -> battleObject_initFastLunge_80CE554, 1 -> battleObject_finishAndDespawn_80CE5F0, 2 -> object_genericDestroy), calling
 * the handler with the state byte in r0 and the table entry still in r1, then
 * falls through into object_updateSprite for the object.
 *
 * The dispatched handler is an opaque guest call: whatever it leaves in
 * r5 and r8-r12 stays live in the register file and is exactly what this
 * state's callers read afterwards, just as in the original where the handler
 * returns straight into the updateSprite call. */

/* register form: the handler address comes from the object's per-state jump
 * table, so the call must go through GUEST_CALL_ADDR rather than a typed
 * wrapper. The original calls it with the state byte in r0 and the table
 * entry in r1, and then uses r5 as the handler left it for the following
 * object_updateSprite call, so the (possibly advanced) object pointer has to
 * be read back out of the register file after the call. */
static u32 call_t3_state_handler(u32 handler, u32 curState) {
    R0 = curState;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    return R5;
}

/* Body: run the state's behaviour handler, then refresh the object's sprite. */
static void t3_0x50_80CE530_run(u32 obj) {
    u32 curState = LD8(obj + oBattleObject_CurState);
    u32 handler  = LD32(A_off_80CE548 + curState);
    u32 liveObj  = call_t3_state_handler(handler, curState);

    call_object_updateSprite_t3_0x50_80CE530(liveObj);
}

/* r5 = battle object. All outputs (r5, r8-r12) are left in place by the
 * dispatched handler and object_updateSprite, so the adapter only dispatches. */
Next t3_0x50_80CE530(void) {
    t3_0x50_80CE530_run(R5);
    RETURN_TO_CALLER();
}

/* ── battle_crackPanelBlinkOutHandler ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; } call_object_clearCollisionRegion_result_battle_crackPanelBlinkOutHandler;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_battle_crackPanelBlinkOutHandler call_object_clearCollisionRegion_battle_crackPanelBlinkOutHandler(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_battle_crackPanelBlinkOutHandler){ R1 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_battle_crackPanelBlinkOutHandler;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battle_crackPanelBlinkOutHandler call_PlaySoundEffect_battle_crackPanelBlinkOutHandler(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battle_crackPanelBlinkOutHandler){ R1, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r1, r2, r5, r10; } call_sub_8109794_result;
/* register form: generated wrapper: sub_8109794 has no typed C interface yet */
static inline call_sub_8109794_result call_sub_8109794(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(sub_8109794);
    return (call_sub_8109794_result){ R1, R2, R5, R10 };
}

/* Battle handler for a panel-crack object blinking out (r5 = battle object).
 *
 * First tick (CurPhase == 0): enter the blink-out phase -- clear the object's
 * collision region, start a 12-tick countdown, play the hit sound (0xD1 + 0xFF
 * = 0x1D0) and, when Param2 is set, fire the panel-coordinate effect
 * sub_8109794 with the object's panel X/Y.
 *
 * While the countdown reads 8 or less, header flag bit 1 mirrors timer bit 1
 * (the blink).  Every tick decrements the timer; the timer is compared as an
 * unsigned 16-bit counter, and once it counts down from 0 the object is moved
 * to CUR_STATE_DESTROY.
 *
 * r1 leaves exactly as the original leaves it: 2 once the blink block ran,
 * otherwise the last callee's r1 (first-tick path) or the value it had on
 * entry.  r5 and r10 pass through unchanged. */

static u32 crack_panel_blink_out(u32 obj, u32 sound_arg, u32 r1) {
    /* First tick: set up the blink-out phase. */
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        ST8(obj + oBattleObject_CurPhase, 1);
        r1 = call_object_clearCollisionRegion_battle_crackPanelBlinkOutHandler(obj).r1;
        ST16(obj + oBattleObject_Timer, 12);
        r1 = call_PlaySoundEffect_battle_crackPanelBlinkOutHandler(0xD1 + 0xFF, sound_arg).r1;
        if (LD8(obj + oBattleObject_Param2) != 0)
            r1 = call_sub_8109794(LD8(obj + oBattleObject_PanelX),
                                  LD8(obj + oBattleObject_PanelY)).r1;
    }

    u32 timer = LD16(obj + oBattleObject_Timer);

    /* Blink: below the threshold, flag bit 1 follows timer bit 1. */
    if (timer <= 8) {
        ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) & ~2u);
        r1 = 2;
        if ((timer & 2) != 0)
            ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2u);
    }

    /* Count down; the object is destroyed once the timer expires.  The timer
     * is a plain 16-bit counter: it expires only when it was exactly 0. */
    if (timer == 0) {
        ST16(obj + oBattleObject_Timer, 0xFFFF);
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
             CUR_STATE_DESTROY);
    } else {
        ST16(obj + oBattleObject_Timer, timer - 1);
    }

    return r1;
}

/* battle_crackPanelBlinkOutHandler(r2 = sound argument, r5 = battle object)
 * -> r1 (r5, r10 unchanged). */
Next battle_crackPanelBlinkOutHandler(void) {
    R1 = crack_panel_blink_out(R5, R2, R1);
    RETURN_TO_CALLER();
}

/* ── t3_0x51_80CE70C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x51_80CE70C;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x51_80CE70C call_object_updateSpritePaused_t3_0x51_80CE70C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x51_80CE70C){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t3_0x51_80CE70C — per-frame tick for battle object type 0x51.
 *
 * The object's CurState byte selects a handler from the jump table at
 * off_80CE724 (the original indexes the word table with the unscaled state
 * byte, ldr r1, [table, state]).  The handler runs with the state byte in
 * r0, its own address in r1 and the object pointer still in r5, then the
 * standard paused-aware sprite update runs for the object.  r10 (the battle
 * context) is read directly by object_updateSpritePaused and stays as it
 * was on entry, so there is nothing to hand down for it. */

/* The state handlers are reached through a code address taken from the
 * table, so they need the raw guest-call helper instead of a typed wrapper. */
static u32 call_state_handler_t3_0x51_80CE70C(u32 handler, u32 state) { /* register form: calls through a guest code address (noted by the tool) */
    R0 = state;
    R1 = handler;               /* the original leaves the handler's own address in r1 */
    GUEST_CALL_ADDR(handler);
    return R0;
}

static void t3_0x51_tick(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    call_state_handler_t3_0x51_80CE70C(LD32(A_off_80CE724 + state), state);
    call_object_updateSpritePaused_t3_0x51_80CE70C(object);
}

Next t3_0x51_80CE70C(void) {
    t3_0x51_tick(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_initSnakeArm_80CE730 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battleObject_initSnakeArm_80CE730(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r0, r2, r4, r5, r6, r7; bool c, v; } call_battleObject_initSpriteAndCollision_8011504_result_battleObject_initSnakeArm_80CE730;
/* register form: generated wrapper: battleObject_initSpriteAndCollision_8011504 has no typed C interface yet */
static inline call_battleObject_initSpriteAndCollision_8011504_result_battleObject_initSnakeArm_80CE730 call_battleObject_initSpriteAndCollision_8011504_battleObject_initSnakeArm_80CE730(u32 r0, u32 r1, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battleObject_initSpriteAndCollision_8011504);
    return (call_battleObject_initSpriteAndCollision_8011504_result_battleObject_initSnakeArm_80CE730){ R0, R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r4, r5, r6, r7; } call_spawn_t1_0x0_EffectObject_result_battleObject_initSnakeArm_80CE730;
/* register form: generated wrapper: spawn_t1_0x0_EffectObject has no typed C interface yet */
static inline call_spawn_t1_0x0_EffectObject_result_battleObject_initSnakeArm_80CE730 call_spawn_t1_0x0_EffectObject_battleObject_initSnakeArm_80CE730(u32 r1, u32 r2, u32 r3, u32 r4) {
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
    return (call_spawn_t1_0x0_EffectObject_result_battleObject_initSnakeArm_80CE730){ R4, R5, R6, R7 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result_battleObject_initSnakeArm_80CE730;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result_battleObject_initSnakeArm_80CE730 call_object_freeMemory_battleObject_initSnakeArm_80CE730(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result_battleObject_initSnakeArm_80CE730){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_setCollisionHitEffect has no typed C interface yet */
static inline void call_object_setCollisionHitEffect_battleObject_initSnakeArm_80CE730(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionHitEffect);
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battleObject_initSnakeArm_80CE730;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battleObject_initSnakeArm_80CE730 call_sprite_setPalette_battleObject_initSnakeArm_80CE730(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battleObject_initSnakeArm_80CE730){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r8; } call_object_presentCollisionData_result_battleObject_initSnakeArm_80CE730;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battleObject_initSnakeArm_80CE730 call_object_presentCollisionData_battleObject_initSnakeArm_80CE730(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battleObject_initSnakeArm_80CE730){ R2, R3, R4, R5, R6, R7, R8 };
}

typedef struct { u32 r0; bool c, v; } call_object_getFrontDirection_result_battleObject_initSnakeArm_80CE730;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battleObject_initSnakeArm_80CE730 call_object_getFrontDirection_battleObject_initSnakeArm_80CE730(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battleObject_initSnakeArm_80CE730){ R0, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Initialize the Snake Arm battle object:
 * 1. set coordinates from panels
 * 2. run sprite/collision init
 * 3a. if sprite init returns 0, spawn the hit-effect, free the object, and exit
 * 3b. otherwise, mark the header flags, set the hit-effect and palette,
 *     present collision data, compute a directional X-velocity, and
 *     switch to CUR_STATE_UPDATE
 * Outputs: r5 (possibly updated through the call chain), r8 (only updated
 *          in path 3b, via presentCollisionData). */

typedef struct { u32 r5; u32 r8; } initSnakeArm_result;

static initSnakeArm_result initSnakeArm(u32 r5, u32 r6, u32 r7, u32 r8) {
    initSnakeArm_result ret;

    /* Position the object from its panel data */
    call_object_setCoordinatesFromPanels_battleObject_initSnakeArm_80CE730(r5);

    /* Sprite/collision init (returns a status in r0; 0 means failure) */
    call_battleObject_initSpriteAndCollision_8011504_result_battleObject_initSnakeArm_80CE730 initRes =
        call_battleObject_initSpriteAndCollision_8011504_battleObject_initSnakeArm_80CE730(0x70402, 0x4050300, r5, r6, r7);
    r5 = initRes.r5;

    if (initRes.r0 == 0) {
        /* Failure path: spawn the hit-effect from the parameter block at r5+0x34,
         * then release the object's memory. */
        u32 base  = r5 + 0x34;
        u32 effX  = LD32(base);
        u32 effY  = LD32(base + 4);
        u32 effZ  = LD32(base + 8) + 0x100000;  /* Z-offset bump */

        call_spawn_t1_0x0_EffectObject_result_battleObject_initSnakeArm_80CE730 spawnRes =
            call_spawn_t1_0x0_EffectObject_battleObject_initSnakeArm_80CE730(effX, effY, effZ, 0x14);
        r5 = spawnRes.r5;

        call_object_freeMemory_result_battleObject_initSnakeArm_80CE730 freeRes = call_object_freeMemory_battleObject_initSnakeArm_80CE730(r5);
        r5 = freeRes.r5;

        ret.r5 = r5;
        ret.r8 = r8;
        return ret;
    }

    /* Normal hit path: mark the header as having collided */
    {
        u8 flags = (u8)LD8(r5 + oObjectHeader_Flags);
        ST8(r5 + oObjectHeader_Flags, (u8)(flags | 2));
    }

    call_object_setCollisionHitEffect_battleObject_initSnakeArm_80CE730(2, r5);
    call_sprite_setPalette_battleObject_initSnakeArm_80CE730(0, r5);

    /* Present collision (r1 = 2 carried through from the flag OR above) */
    call_object_presentCollisionData_result_battleObject_initSnakeArm_80CE730 collRes =
        call_object_presentCollisionData_battleObject_initSnakeArm_80CE730(2, r5, r8);
    r5 = collRes.r5;
    r8 = collRes.r8;

    /* Directional X-velocity: front direction * ExtraVars scalar */
    call_object_getFrontDirection_result_battleObject_initSnakeArm_80CE730 dirRes = call_object_getFrontDirection_battleObject_initSnakeArm_80CE730(r5);
    {
        u32 extra = LD32(r5 + oBattleObject_ExtraVars);
        ST32(r5 + oBattleObject_XVelocity, dirRes.r0 * extra);
    }

    /* Switch the object to its per-frame update state */
    ST8(r5 + oBattleObject_CurState, CUR_STATE_UPDATE);

    ret.r5 = r5;
    ret.r8 = r8;
    return ret;
}

Next battleObject_initSnakeArm_80CE730(void) {
    initSnakeArm_result res = initSnakeArm(R5, R6, R7, R8);
    R5 = res.r5;
    R8 = res.r8;
    RETURN_TO_CALLER();
}

/* ── t3_0x5d_80D00A0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x5d_80D00A0;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x5d_80D00A0 call_object_updateSpritePaused_t3_0x5d_80D00A0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x5d_80D00A0){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * Battle object "t3" step routine (0x080D00A0).
 *
 * Dispatches to the per-state handler selected by the battle object's
 * CurState byte -- the state value is used directly as a byte offset into
 * the handler table at off_80D00B8 (object_initLoadSprite /
 * object_stateStep / battle_destroyObject_80D01BC) -- and then always runs
 * object_updateSpritePaused on the object before returning.
 *
 * r2 and r10 pass through to the handlers untouched. r5 (the object
 * pointer) may be updated by the state handler; that updated pointer is
 * what the sprite update sees and what our caller receives. r8-r12 are
 * likewise whatever the callees leave behind.
 */

/*
 * Call the object state handler through its jump-table address.
 *
 * register form: the call target comes from a guest jump table, and the
 * values the handler leaves in r5 and r8-r12 must reach this routine's
 * caller exactly as the original leaves them, so this call cannot be
 * expressed as a typed wrapper call.
 */
static u32 call_state_handler_t3_0x5d_80D00A0(u32 handler, u32 state, u32 object) {
    R0 = state;    /* the handlers receive the current state in r0 */
    R1 = handler;  /* the original jumps with the table entry still in r1 */
    R5 = object;
    GUEST_CALL_ADDR(handler);
    return R5;     /* the handler's r5 is the object pointer from here on */
}

/* Typed body: run the object's state handler, then update its sprite. */
static u32 t3_state_step_and_sprite_update(u32 object) {
    u32 state   = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D00B8 + state);

    object = call_state_handler_t3_0x5d_80D00A0(handler, state, object);

    /* the original falls straight through into this call */
    call_object_updateSpritePaused_result_t3_0x5d_80D00A0 result =
        call_object_updateSpritePaused_t3_0x5d_80D00A0(object);
    return result.r5;
}

/* t3_0x5d_80D00A0(r5 = battle object; r2, r10 pass through) -> r5, r8-r12 from the callees */
Next t3_0x5d_80D00A0(void) {
    R5 = t3_state_step_and_sprite_update(R5);
    RETURN_TO_CALLER();
}

/* ── object_initLoadSprite ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_object_initLoadSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r4, r5; } call_sprite_load_result_object_initLoadSprite;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_object_initLoadSprite call_sprite_load_object_initLoadSprite(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_object_initLoadSprite){ R4, R5 };
}

/* register form: generated wrapper: sprite_hasShadow has no typed C interface yet */
static inline void call_sprite_hasShadow(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_hasShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_object_initLoadSprite(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r1, r2, r4, r5; } call_sprite_loadAnimationData_result_object_initLoadSprite;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_object_initLoadSprite call_sprite_loadAnimationData_object_initLoadSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_object_initLoadSprite){ R1, R2, R4, R5 };
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_object_initLoadSprite;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_object_initLoadSprite call_sprite_setPalette_object_initLoadSprite(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_object_initLoadSprite){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_object_initLoadSprite;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_object_initLoadSprite call_object_getFlip_object_initLoadSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_object_initLoadSprite){ R0 };
}

typedef struct { u32 r1, r2, r3; bool c, v; } call_sprite_setFlip_result_object_initLoadSprite;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_object_initLoadSprite call_sprite_setFlip_object_initLoadSprite(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_object_initLoadSprite){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Initialize a battle object's sprite: position, graphics, animation, palette, flip, and state. */
static void object_initLoadSprite_body(u32 obj)
{
    /* Compute screen coordinates from the object's tile panels */
    call_object_setCoordinatesFromPanels_object_initLoadSprite(obj);

    /* Clear the 16-bit sub-pixel Z offset */
    ST16(obj + oBattleObject_Z16, 0);

    /* Load the sprite graphics: type 0x80, width 8 tiles, height 0xE tiles */
    call_sprite_load_object_initLoadSprite(0x80, 8, 0xE, obj);

    /* Enable shadow rendering for this sprite */
    call_sprite_hasShadow(obj);

    /* Set flag bit 2 (shadow enabled marker) */
    u32 flags = LD8(obj + oObjectHeader_Flags);
    flags |= 2;
    ST8(obj + oObjectHeader_Flags, flags);

    /* Look up the default animation ID from a per-Param2 table */
    u32 param2 = LD8(obj + oBattleObject_Param2);
    u32 animId = LD8(A_word_80D0130 + param2);
    ST8(obj + oBattleObject_CurAnim, animId);
    ST8(obj + oBattleObject_CurAnimCopy, animId);

    /* Apply the animation to the sprite */
    call_sprite_setAnimation_object_initLoadSprite(animId, obj);

    /* Load animation frame data for the current sprite */
    call_sprite_loadAnimationData_object_initLoadSprite(obj);

    /* Set palette index 0 */
    call_sprite_setPalette_object_initLoadSprite(0, obj);

    /* Read the object's flip state and apply it to the sprite */
    u32 flip = call_object_getFlip_object_initLoadSprite(obj).r0;
    call_sprite_setFlip_object_initLoadSprite(flip, obj);

    /* Clear Param3 (unused / reset to 0) */
    ST8(obj + oBattleObject_Param3, 0);

    /* If animation index is 0 (idle), shift the sprite up by one pixel */
    if (LD8(obj + oBattleObject_Param2) == 0) {
        s32 y = (s32)LD32(obj + oBattleObject_Y);
        s32 z = (s32)LD32(obj + oBattleObject_Z);
        ST32(obj + oBattleObject_Y, (u32)(y - 1));
        ST32(obj + oBattleObject_Z, (u32)(z - 1));
    }

    /* Transition the object to the update state */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
}

/* object_initLoadSprite(r5 = battle object) -> r5 (unchanged) */
Next object_initLoadSprite(void) {
    object_initLoadSprite_body(R5);
    RETURN_TO_CALLER();
}

/* ── object_stateStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r3; } call_battle_isBattleOver_result_object_stateStep;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result_object_stateStep call_battle_isBattleOver_object_stateStep(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result_object_stateStep){ R0, R1, R3 };
}

typedef struct { u32 r0, r1; bool z, n; } call_battle_isTimeStop_result_object_stateStep;
/* register form: generated wrapper: battle_isTimeStop has no typed C interface yet */
static inline call_battle_isTimeStop_result_object_stateStep call_battle_isTimeStop_object_stateStep(void) {
    GUEST_CALL(battle_isTimeStop);
    return (call_battle_isTimeStop_result_object_stateStep){ R0, R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* object_stateStep — advance one battle object's state machine by one step.
 *
 * Inputs: r2 = caller argument forwarded to the action handler,
 *         r5 = object, r10 = battle context (read by the battle_* helpers
 *         from guest r10, which stays as on entry throughout).
 *
 * If the battle is over, or the object's related partner is not performing
 * action 0xA, the object is put into the destroy state. Otherwise, unless
 * time is currently stopped, the object's own current action selects a
 * handler from the table at off_80D0164 and that handler is called through
 * its address with r0 = action, r1 = handler address, r2 = caller argument,
 * r5 = object. The handler's results (r0/r1/r2/r3/r6/r7 are clobbers, and
 * r5/r8-r12 as it leaves them) are already in the guest register file after
 * the raw call, so nothing needs to be passed back explicitly. */

enum { PARTNER_ACTION_GATE = 0xA };   /* partner action that gates the dispatch */

/* Move the object to the destroy state (state/action/phase word). */
static void destroy_object(u32 object) {
    ST32(object + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
         CUR_STATE_DESTROY);
}

/* register form: the action handler is reached through a jump-table address
 * (mov lr,pc / bx r1), which no generated wrapper covers. */
static void dispatch_action_handler_object_stateStep(u32 handler, u32 action, u32 arg2,
                                    u32 object, u32 battleContext) {
    R0 = action;
    R1 = handler;
    R2 = arg2;
    R5 = object;
    R10 = battleContext;
    GUEST_CALL_ADDR(handler);
}

static void object_stateStep_run(u32 arg2, u32 object, u32 battleContext) {
    /* Battle finished: tear the object down. */
    if (call_battle_isBattleOver_object_stateStep().r0 != 0) {
        destroy_object(object);
        return;
    }

    /* Partner not in the gating action: tear the object down. */
    u32 partner = LD32(object + oBattleObject_RelatedObject1Ptr);
    u32 partnerAction = LD8(partner + oBattleObject_CurAction);
    if (partnerAction != PARTNER_ACTION_GATE) {
        destroy_object(object);
        return;
    }

    /* battle_isTimeStop leaves Z clear when time is stopped: hold this
     * object's step (no destroy, no handler call). */
    if (!call_battle_isTimeStop_object_stateStep().z)
        return;

    /* Run this object's own action handler, chosen from the jump table. */
    u32 action = LD8(object + oBattleObject_CurAction);
    u32 handler = LD32(A_off_80D0164 + action);
    dispatch_action_handler_object_stateStep(handler, action, arg2, object, battleContext);
}

Next object_stateStep(void) {
    object_stateStep_run(R2, R5, R10);
    RETURN_TO_CALLER();
}

/* ── battle_timedHitCheckUpdate ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5; } call_battle_spawnSelfPanelCollisionRegion_result;
/* register form: generated wrapper: battle_spawnSelfPanelCollisionRegion has no typed C interface yet */
static inline call_battle_spawnSelfPanelCollisionRegion_result call_battle_spawnSelfPanelCollisionRegion(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_spawnSelfPanelCollisionRegion);
    return (call_battle_spawnSelfPanelCollisionRegion_result){ R5 };
}

typedef struct { u32 r1, r2, r3, r5, r6, r7, r10; bool z, n, c, v; } call_PlaySoundEffect_result_battle_timedHitCheckUpdate;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battle_timedHitCheckUpdate call_PlaySoundEffect_battle_timedHitCheckUpdate(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battle_timedHitCheckUpdate){ R1, R2, R3, R5, R6, R7, R10, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Timed hit: drives a battle object through a countdown sequence,
 * spawning a panel collision check at 15 ticks and playing a sound at 10,
 * then self-destructing at 0. */

struct TimedHitResult {
    u32 r5;
    u32 r10;
};

static struct TimedHitResult timed_hit_update(u32 obj, u32 r2, u32 r10_in) {
    struct TimedHitResult out = { .r5 = obj, .r10 = r10_in };

    /* First tick: initialize phase to 4, set countdown to 20 (0x14) */
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        ST8(obj + oBattleObject_CurPhase, 4);
        ST16(obj + oBattleObject_Timer, 0x14);
        return out;
    }

    /* Decrement timer */
    u32 timer = LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, timer);

    if (timer == 0xF) {
        /* At 15: spawn a collision region and check panel match */
        if (LD8(obj + oBattleObject_Param2) != 0)
            return out;

        call_battle_spawnSelfPanelCollisionRegion_result spawn_res =
            call_battle_spawnSelfPanelCollisionRegion(obj);
        obj = spawn_res.r5;

        u32 extra = LD32(obj + (oBattleObject_ExtraVars + 4));
        u32 panel_x = LD8(obj + oBattleObject_PanelX);
        if (LD8(extra + 0x12) == panel_x)
            ST8(obj + oBattleObject_Param3, 1);

        out.r5 = obj;
        return out;

    } else if (timer == 0xA) {
        /* At 10: play the hit sound effect (ID 0x180) */
        if (LD8(obj + oBattleObject_Param2) != 0)
            return out;

        call_PlaySoundEffect_result_battle_timedHitCheckUpdate snd = call_PlaySoundEffect_battle_timedHitCheckUpdate(0x180, r2);
        out.r5  = snd.r5;
        out.r10 = snd.r10;
        return out;

    } else if ((s32)timer <= 0) {
        /* Timer expired (0 or wrapped negative): destroy self */
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
             CUR_STATE_DESTROY);
        return out;
    }

    /* Timer still counting down: nothing else */
    return out;
}

/* battle_timedHitCheckUpdate(r5 = object, r2 = passed to PlaySoundEffect)
 * -> r5, r10 */
Next battle_timedHitCheckUpdate(void) {
    struct TimedHitResult res = timed_hit_update(R5, R2, R10);
    R5  = res.r5;
    R10 = res.r10;
    RETURN_TO_CALLER();
}

/* ── battle_destroyObject_80D01BC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battle_clearExtraVarsOwnerRef has no typed C interface yet */
static inline void call_battle_clearExtraVarsOwnerRef(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_clearExtraVarsOwnerRef);
}

typedef struct { u32 r0, r1, r2, r3, r5, r6, r7; bool z, n, c, v; } call_object_genericDestroy_result_sub_80D01BC;
/* register form: generated wrapper: object_genericDestroy has no typed C interface yet */
static inline call_object_genericDestroy_result_sub_80D01BC call_object_genericDestroy_sub_80D01BC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_genericDestroy);
    return (call_object_genericDestroy_result_sub_80D01BC){ R0, R1, R2, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Tear down a battle object: clear its owner back-reference in the extra-vars
 * list, then release the object itself. Returns the object pointer (r5),
 * which both callees leave unchanged. */
static u32 destroy_battle_object(u32 object) {
    call_battle_clearExtraVarsOwnerRef(object);

    call_object_genericDestroy_result_sub_80D01BC result = call_object_genericDestroy_sub_80D01BC(object);
    return result.r5;
}

/* battle_destroyObject_80D01BC(r5 = object) -> r5 */
Next battle_destroyObject_80D01BC(void) {
    R5 = destroy_battle_object(R5);
    RETURN_TO_CALLER();
}

/* ── battle_clearExtraVarsOwnerRef ── */
/* Clear the object's extra-vars back-pointer.
 * ExtraVars points at a slot whose first word is the owner: if it still
 * points back to this object, blank the slot so it is no longer owned. */
static void battle_clearExtraVarsOwnerRef_typed(u32 object) {
    u32 slot = LD32(object + oBattleObject_ExtraVars);
    if (LD32(slot) == object)
        ST32(slot, 0);
}

/* battle_clearExtraVarsOwnerRef(r5 = object) */
Next battle_clearExtraVarsOwnerRef(void) {
    battle_clearExtraVarsOwnerRef_typed(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnSelfPanelCollisionRegion ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r5; } call_object_spawnCollisionRegion_result;
/* register form: generated wrapper: object_spawnCollisionRegion has no typed C interface yet */
static inline call_object_spawnCollisionRegion_result call_object_spawnCollisionRegion(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_spawnCollisionRegion);
    return (call_object_spawnCollisionRegion_result){ R2, R3, R5 };
}

/* Spawns this battle object's collision region over its own panel.
 * The original builds the call arguments from the object (r5), calls
 * object_spawnCollisionRegion, then pops r4/r6/r7 back, so those registers
 * must be handed through unchanged even though the callee's wrappers leave
 * their argument values behind in them. r5's outgoing value is whatever the
 * callee left there (its own r5 argument side effect), which the wrapper
 * reports in its result. */

/* Result of the typed helper: the callee's leftover r5 plus the registers the
 * original restores from its stack frame. */
typedef struct {
    u32 r4;
    u32 r5;
    u32 r6;
    u32 r7;
} self_region_result;

static self_region_result spawn_self_panel_region(u32 obj, u32 saved_r4, u32 saved_r6, u32 saved_r7) {
    u32 panelX  = LD8(obj + oBattleObject_PanelX);
    u32 panelY  = LD8(obj + oBattleObject_PanelY);
    u32 element = LD8(obj + oBattleObject_Element);
    u32 damage  = LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler);

    /* r3 = 0x10 << 16, r4 = region descriptor 0x4050604, r7 = size 3. */
    call_object_spawnCollisionRegion_result res =
        call_object_spawnCollisionRegion(panelX, panelY, element,
                                         0x10u << 16, 0x4050604, obj,
                                         damage, 3);

    self_region_result out;
    out.r4 = saved_r4;      /* the original's pop {r4,r6,r7,pc} restores these */
    out.r5 = res.r5;        /* r5 as the callee left it */
    out.r6 = saved_r6;
    out.r7 = saved_r7;
    return out;
}

/* inputs: r5 (battle object), lr; outputs: r5; preserved: r4 r6 r7 */
Next battle_spawnSelfPanelCollisionRegion(void) {
    self_region_result res = spawn_self_panel_region(R5, R4, R6, R7);
    R4 = res.r4;
    R5 = res.r5;
    R6 = res.r6;
    R7 = res.r7;
    RETURN_TO_CALLER();
}

/* ── t3_0x5e_80D0268 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x5e_80D0268;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x5e_80D0268 call_object_updateSpritePaused_t3_0x5e_80D0268(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x5e_80D0268){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Jump-table dispatch of the per-state virtual handler for a battle object,
 * followed by a sprite update. The handler (indexed by oBattleObject_CurState)
 * may modify R5 and the R8–R12 group; the modified R5 feeds into
 * object_updateSpritePaused, which further updates R5. */

struct StateUpdateResult {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
};

static struct StateUpdateResult state_update_and_sprite(u32 battleObj, u32 r2_in, u32 r10_in) {
    /* Virtual handler address = table[curState]; each entry is a thumb fn ptr (+1). */
    u32 curState = LD8(battleObj + oBattleObject_CurState);
    u32 funcPtr  = LD32(A_off_80D0280 + curState);

    /* register form: jump-table callee requires R0=state byte, R5=object,
     * R2/R10 passed through from caller; after return R5 and R8-R12 are
     * modified by the handler and are outputs of this function. */
    R0  = curState;
    R5  = battleObj;
    R2  = r2_in;
    R10 = r10_in;
    GUEST_CALL_ADDR(funcPtr);

    struct StateUpdateResult out;
    out.r8  = R8;
    out.r9  = R9;
    out.r10 = r10_in;  /* preserved: not in clobbers of either call */
    out.r11 = R11;
    out.r12 = R12;

    /* Sprite update runs on the handler-modified R5; R10 is unchanged.
     * object_updateSpritePaused outputs R0-R5; we keep only R5. */
    call_object_updateSpritePaused_result_t3_0x5e_80D0268 spriteRes = call_object_updateSpritePaused_t3_0x5e_80D0268(R5);
    out.r5 = spriteRes.r5;

    return out;
}

/* t3_0x5e_80D0268: r2, r5, r10, lr -> r5, r8, r9, r10, r11, r12 */
Next t3_0x5e_80D0268(void) {
    struct StateUpdateResult out = state_update_and_sprite(R5, R2, R10);
    R5  = out.r5;
    R8  = out.r8;
    R9  = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── battleObject_initializeFromStateUpdate_80D028C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battleObject_initializeFromStateUpdate_80D028C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r4, r5; } call_sprite_load_result_battleObject_initializeFromStateUpdate_80D028C;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battleObject_initializeFromStateUpdate_80D028C call_sprite_load_battleObject_initializeFromStateUpdate_80D028C(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battleObject_initializeFromStateUpdate_80D028C){ R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battleObject_initializeFromStateUpdate_80D028C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_initializeFromStateUpdate_80D028C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r1, r2, r4, r5; } call_sprite_loadAnimationData_result_battleObject_initializeFromStateUpdate_80D028C;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_initializeFromStateUpdate_80D028C call_sprite_loadAnimationData_battleObject_initializeFromStateUpdate_80D028C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_initializeFromStateUpdate_80D028C){ R1, R2, R4, R5 };
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battleObject_initializeFromStateUpdate_80D028C;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battleObject_initializeFromStateUpdate_80D028C call_sprite_setPalette_battleObject_initializeFromStateUpdate_80D028C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battleObject_initializeFromStateUpdate_80D028C){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_battleObject_initializeFromStateUpdate_80D028C;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battleObject_initializeFromStateUpdate_80D028C call_object_getFlip_battleObject_initializeFromStateUpdate_80D028C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battleObject_initializeFromStateUpdate_80D028C){ R0 };
}

/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline void call_sprite_setFlip_battleObject_initializeFromStateUpdate_80D028C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
}

typedef struct { u32 r1, r2, r3; bool c, v; } call_object_reservePanel_result_battleObject_initializeFromStateUpdate_80D028C;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_battleObject_initializeFromStateUpdate_80D028C call_object_reservePanel_battleObject_initializeFromStateUpdate_80D028C(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_battleObject_initializeFromStateUpdate_80D028C){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Bring a battle object up from its stored state: place it on the ground,
 * load its sprite resources, set its current animation, apply palette and
 * flip from its facing, re-reserve the panel it stands on, and switch it to
 * the "update" state. Returns the object pointer unchanged (the original
 * leaves r5 alone). */
static u32 battleObject_initializeFromStateUpdate(u32 obj) {
    call_object_setCoordinatesFromPanels_battleObject_initializeFromStateUpdate_80D028C(obj);

    ST32(obj + oBattleObject_Z, 0);

    call_sprite_load_battleObject_initializeFromStateUpdate_80D028C(0x80, 8, 0xE, obj);
    call_sprite_noShadow_battleObject_initializeFromStateUpdate_80D028C(obj);

    /* Set the header flag bit 1 (mark the sprite as loaded, in the game's terms) */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Select animation 0x3E and load its frame data */
    ST8(obj + oBattleObject_CurAnim, 0x3E);
    ST8(obj + oBattleObject_CurAnimCopy, 0x3E);
    call_sprite_setAnimation_battleObject_initializeFromStateUpdate_80D028C(0x3E, obj);
    call_sprite_loadAnimationData_battleObject_initializeFromStateUpdate_80D028C(obj);

    call_sprite_setPalette_battleObject_initializeFromStateUpdate_80D028C(0, obj);
    call_sprite_setFlip_battleObject_initializeFromStateUpdate_80D028C(call_object_getFlip_battleObject_initializeFromStateUpdate_80D028C(obj).r0, obj);

    call_object_reservePanel_battleObject_initializeFromStateUpdate_80D028C(LD8(obj + oBattleObject_PanelX),
                             LD8(obj + oBattleObject_PanelY), obj);

    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
    return obj;
}

/* battleObject_initializeFromStateUpdate_80D028C(r5 = object) -> r5 */
Next battleObject_initializeFromStateUpdate_80D028C(void) {
    R5 = battleObject_initializeFromStateUpdate(R5);
    RETURN_TO_CALLER();
}

/* ── object_stepDependentOnParent ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r3; } call_battle_isBattleOver_result_object_stepDependentOnParent;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result_object_stepDependentOnParent call_battle_isBattleOver_object_stepDependentOnParent(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result_object_stepDependentOnParent){ R0, R1, R3 };
}

typedef struct { u32 r0, r1; bool z, n; } call_battle_isTimeStop_result_object_stepDependentOnParent;
/* register form: generated wrapper: battle_isTimeStop has no typed C interface yet */
static inline call_battle_isTimeStop_result_object_stepDependentOnParent call_battle_isTimeStop_object_stepDependentOnParent(void) {
    GUEST_CALL(battle_isTimeStop);
    return (call_battle_isTimeStop_result_object_stepDependentOnParent){ R0, R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* object_stepDependentOnParent — step for a battle object that follows a related
 * object ("parent"):
 *   - If the battle is over, or the parent has moved on to action 0xB, this object
 *     has nothing left to wait for: switch it to the destroy state.
 *   - Otherwise, while time is stopped, do nothing and keep waiting.
 *   - Otherwise run the object's own step routine, chosen from a two-entry jump
 *     table indexed by its current action:
 *       0 -> battle_objWaitThenSetAction4
 *       1 -> battle_objectDestroyAfterTimer
 * The dispatched routine's register results (r5, r8-r12) are this function's
 * outputs; they pass straight through the guest register file.
 */

/* Switch the object into the destroy state. */
static void dep_obj_start_destroy(u32 objPtr) {
    ST32(objPtr + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
         CUR_STATE_DESTROY);
}

/* register form: dispatches to the object's step routine through the jump table;
 * the callee leaves the outputs in r5/r8-r12, which cannot cross a C return
 * value, so they must stay in the guest registers. */
static void dispatch_parent_action(u32 curAction) {
    u32 target = LD32(A_off_80D0308 + curAction);
    R0 = curAction;                 /* register state as the original leaves it before bx */
    R1 = target;
    GUEST_CALL_ADDR(target);
}

static void object_stepDependentOnParent_step(u32 objPtr) {
    if (call_battle_isBattleOver_object_stepDependentOnParent().r0 != 0) {
        dep_obj_start_destroy(objPtr);
        return;
    }

    /* The parent's current action says it is done (0xB): nothing to wait for. */
    u32 related = LD32(objPtr + oBattleObject_RelatedObject1Ptr);
    if (LD8(related + oBattleObject_CurAction) != 0xB) {
        dep_obj_start_destroy(objPtr);
        return;
    }

    /* battle_isTimeStop: Z set when time is stopped -> keep waiting this step. */
    if (!call_battle_isTimeStop_object_stepDependentOnParent().z)
        return;

    dispatch_parent_action(LD8(objPtr + oBattleObject_CurAction));
}

/* inputs: r2 r5 r10 lr / outputs: r5 r8 r9 r10 r11 r12 */
Next object_stepDependentOnParent(void) {
    object_stepDependentOnParent_step(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objWaitThenSetAction4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_battle_objWaitThenSetAction4;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battle_objWaitThenSetAction4 call_PlaySoundEffect_battle_objWaitThenSetAction4(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battle_objWaitThenSetAction4){ R1, R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Battle object helper: the object waits a short time, then performs action 4.
 *
 * First call (CurPhase == 0): mark the wait as started by storing phase 4,
 * play sound effect 0x146 (the original builds it as 0x47 + 0xFF), and set
 * the wait timer to 11 frames.
 * Later calls: decrement the timer; while it is still positive (as a signed
 * 32-bit value, matching the original's BGT) keep waiting. Once it drops to
 * zero or below, store CurAction = 4 and clear CurPhaseAndPhaseInitialized.
 *
 * PlaySoundEffect leaves r5 and r10 as they were on entry, so the object
 * pointer stays valid across the call and the caller-visible r5/r10 are
 * simply whatever that call leaves behind. */
static void obj_wait_then_set_action(u32 obj, u32 arg2)
{
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        /* First run: begin the wait. */
        ST8(obj + oBattleObject_CurPhase, 4);
        call_PlaySoundEffect_battle_objWaitThenSetAction4(0x146, arg2);
        ST16(obj + oBattleObject_Timer, 0xB);
    } else {
        /* Waiting: count the timer down and act when it expires. */
        u32 timer = LD16(obj + oBattleObject_Timer) - 1;
        ST16(obj + oBattleObject_Timer, timer);
        if ((s32)timer <= 0) {
            ST8(obj + oBattleObject_CurAction, 4);
            ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
        }
    }
}

/* battle_objWaitThenSetAction4(r2 = pass-through arg, r5 = battle object) -> r5 r10
 * Flag outputs are clobbers here; callers never branch on them. */
Next battle_objWaitThenSetAction4(void) {
    obj_wait_then_set_action(R5, R2);
    RETURN_TO_CALLER();
}

/* ── battle_objectDestroyAfterTimer ── */
/* Battle object "destroy after timer" update (r5 = object pointer).
 *
 * On the first run (CurPhase == 0) the object enters phase 4 and arms its
 * timer with Param1 minus a short lead-in. Every run then decrements the
 * timer; when it reaches zero (or was already zero, wrapping to -1) the
 * object's state is switched to CUR_STATE_DESTROY. */
static void battle_objectDestroyAfterTimer_impl(u32 obj) {
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        /* First entry: mark the phase as started and arm the timer. */
        ST8(obj + oBattleObject_CurPhase, 4);
        ST16(obj + oBattleObject_Timer,
             (u16)(LD8(obj + oBattleObject_Param1) - 0xA));
    }

    s32 timer = (s32)LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, (u16)timer);

    if (timer <= 0)
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
             CUR_STATE_DESTROY);
}

Next battle_objectDestroyAfterTimer(void) {
    battle_objectDestroyAfterTimer_impl(R5);
    RETURN_TO_CALLER();
}

/* ── battle_destroyObjectReleasePanel_80D035C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3; } call_object_removePanelReserve_result;
/* register form: generated wrapper: object_removePanelReserve has no typed C interface yet */
static inline call_object_removePanelReserve_result call_object_removePanelReserve(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_removePanelReserve);
    return (call_object_removePanelReserve_result){ R2, R3 };
}

typedef struct { u32 r0, r1, r2, r3, r5, r6, r7; bool z, n, c, v; } call_object_genericDestroy_result_sub_80D035C;
/* register form: generated wrapper: object_genericDestroy has no typed C interface yet */
static inline call_object_genericDestroy_result_sub_80D035C call_object_genericDestroy_sub_80D035C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_genericDestroy);
    return (call_object_genericDestroy_result_sub_80D035C){ R0, R1, R2, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Destroy a battle object, first giving back the panel area it reserved.
 * r5 holds the object pointer; it is passed through to both callees and is
 * this function's only output (genericDestroy keeps it intact). */
static u32 destroy_object_with_panel_release(u32 object) {
    u32 panelX = LD8(object + oBattleObject_PanelX);
    u32 panelY = LD8(object + oBattleObject_PanelY);

    call_object_removePanelReserve(panelX, panelY, object);
    return call_object_genericDestroy_sub_80D035C(object).r5;
}

/* battle_destroyObjectReleasePanel_80D035C(r5 = object) -> r5 */
Next battle_destroyObjectReleasePanel_80D035C(void) {
    R5 = destroy_object_with_panel_release(R5);
    RETURN_TO_CALLER();
}

/* ── t3_0x5f_80D0394 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x5f_80D0394;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x5f_80D0394 call_object_updateSpritePaused_t3_0x5f_80D0394(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x5f_80D0394){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Battle-object t3 per-frame state update.
 * Reads the object's current state, dispatches the matching handler through
 * a jump table (which updates r5 and r8–r12), then calls object_updateSpritePaused. */
/* register form: state handler invoked via jump table modifies guest registers r5, r8-r12;
 * no typed wrapper exists for the three possible targets. */
static void t3_state_machine_update(void) {
    u32 state = LD8(R5 + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D03AC + state);

    /* Jump-table call: handler reads r0=state, r5=object, and modifies r5, r8-r12 */
    R0 = state;
    GUEST_CALL_ADDR(handler);

    /* Sprite physics update; wrapper passes r10 through unchanged from handler */
    call_object_updateSpritePaused_t3_0x5f_80D0394(R5);
}

Next t3_0x5f_80D0394(void) {
    t3_state_machine_update();
    RETURN_TO_CALLER();
}

/* ── battleObject_initSpawnedObject_80D03B8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_object_setCoordinatesFromPanels_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline call_object_setCoordinatesFromPanels_result_battleObject_initSpawnedObject_80D03B8 call_object_setCoordinatesFromPanels_battleObject_initSpawnedObject_80D03B8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
    return (call_object_setCoordinatesFromPanels_result_battleObject_initSpawnedObject_80D03B8){ R2 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battleObject_initSpawnedObject_80D03B8 call_object_getFrontDirection_battleObject_initSpawnedObject_80D03B8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battleObject_initSpawnedObject_80D03B8){ R0 };
}

typedef struct { u32 r4, r5; } call_sprite_load_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battleObject_initSpawnedObject_80D03B8 call_sprite_load_battleObject_initSpawnedObject_80D03B8(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battleObject_initSpawnedObject_80D03B8){ R4, R5 };
}

/* register form: generated wrapper: sprite_hasShadow has no typed C interface yet */
static inline void call_sprite_hasShadow_battleObject_initSpawnedObject_80D03B8(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_hasShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_initSpawnedObject_80D03B8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r1, r2, r4, r5; } call_sprite_loadAnimationData_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_initSpawnedObject_80D03B8 call_sprite_loadAnimationData_battleObject_initSpawnedObject_80D03B8(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_initSpawnedObject_80D03B8){ R1, R2, R4, R5 };
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battleObject_initSpawnedObject_80D03B8 call_sprite_setPalette_battleObject_initSpawnedObject_80D03B8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battleObject_initSpawnedObject_80D03B8){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battleObject_initSpawnedObject_80D03B8 call_object_getFlip_battleObject_initSpawnedObject_80D03B8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battleObject_initSpawnedObject_80D03B8){ R0 };
}

/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline void call_sprite_setFlip_battleObject_initSpawnedObject_80D03B8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
}

typedef struct { u32 r0, r4; } call_object_createCollisionData_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: object_createCollisionData has no typed C interface yet */
static inline call_object_createCollisionData_result_battleObject_initSpawnedObject_80D03B8 call_object_createCollisionData_battleObject_initSpawnedObject_80D03B8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_createCollisionData);
    return (call_object_createCollisionData_result_battleObject_initSpawnedObject_80D03B8){ R0, R4 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result_battleObject_initSpawnedObject_80D03B8 call_object_freeMemory_battleObject_initSpawnedObject_80D03B8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result_battleObject_initSpawnedObject_80D03B8){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r4, r6, r7; } call_object_setupCollisionData_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: object_setupCollisionData has no typed C interface yet */
static inline call_object_setupCollisionData_result_battleObject_initSpawnedObject_80D03B8 call_object_setupCollisionData_battleObject_initSpawnedObject_80D03B8(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    GUEST_CALL(object_setupCollisionData);
    return (call_object_setupCollisionData_result_battleObject_initSpawnedObject_80D03B8){ R1, R2, R4, R6, R7 };
}

typedef struct { u32 r3; } call_object_setCollisionHitEffect_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: object_setCollisionHitEffect has no typed C interface yet */
static inline call_object_setCollisionHitEffect_result_battleObject_initSpawnedObject_80D03B8 call_object_setCollisionHitEffect_battleObject_initSpawnedObject_80D03B8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionHitEffect);
    return (call_object_setCollisionHitEffect_result_battleObject_initSpawnedObject_80D03B8){ R3 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8; } call_object_presentCollisionData_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battleObject_initSpawnedObject_80D03B8 call_object_presentCollisionData_battleObject_initSpawnedObject_80D03B8(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battleObject_initSpawnedObject_80D03B8){ R1, R2, R3, R4, R5, R6, R7, R8 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_battleObject_initSpawnedObject_80D03B8;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battleObject_initSpawnedObject_80D03B8 call_PlaySoundEffect_battleObject_initSpawnedObject_80D03B8(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battleObject_initSpawnedObject_80D03B8){ R1, R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Per-object spawn speed table indexed by Param1 (entries are 32-bit words). */
/* byte_80D0444: {0x00060000, 0x00068000, 0x00070000, 0x00078000} */

/* Initialize a newly spawned battle object: position, sprite, animation,
 * collision hitbox, and spawn sound.  Frees the object if collision
 * allocation fails; otherwise transitions it to CUR_STATE_UPDATE. */
static void init_spawned_object(u32 obj, u32 r8_val) {
    /* Derive world coordinates from the panel grid */
    call_object_setCoordinatesFromPanels_battleObject_initSpawnedObject_80D03B8(obj);

    /* Drop to ground level, then half-tile offset in Y16 and Z16 */
    ST32(obj + oBattleObject_Z, 0);
    ST16(obj + oBattleObject_Y16, LD16(obj + oBattleObject_Y16) + 1);
    ST16(obj + oBattleObject_Z16, LD16(obj + oBattleObject_Z16) + 1);

    /* X velocity = facing direction * table[Param1] */
    u32 dir = call_object_getFrontDirection_battleObject_initSpawnedObject_80D03B8(obj).r0;
    u32 param = LD8(obj + oBattleObject_Param1);
    u32 speed = LD32(A_byte_80D0444 + (param << 2));
    ST32(obj + oBattleObject_XVelocity, dir * speed);

    /* Load sprite (ID 0x80, cell-size 8, shadow flag 0xE) */
    call_sprite_load_battleObject_initSpawnedObject_80D03B8(0x80, 8, 0xE, obj);
    call_sprite_hasShadow_battleObject_initSpawnedObject_80D03B8(obj);

    /* Mark shadow bit (1 << 1) in the header flags */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Set animation slot 0x3F and load its data / palette 0 */
    ST8(obj + oBattleObject_CurAnim, 0x3F);
    ST8(obj + oBattleObject_CurAnimCopy, 0x3F);
    call_sprite_setAnimation_battleObject_initSpawnedObject_80D03B8(0x3F, obj);
    call_sprite_loadAnimationData_battleObject_initSpawnedObject_80D03B8(obj);
    call_sprite_setPalette_battleObject_initSpawnedObject_80D03B8(0, obj);

    /* Mirror sprite to match object's facing */
    call_sprite_setFlip_battleObject_initSpawnedObject_80D03B8(call_object_getFlip_battleObject_initSpawnedObject_80D03B8(obj).r0, obj);

    /* Allocate collision data; bail out if the pool is exhausted */
    u32 col = call_object_createCollisionData_battleObject_initSpawnedObject_80D03B8(obj).r0;
    if (!col) {
        call_object_freeMemory_battleObject_initSpawnedObject_80D03B8(obj);
        return;
    }

    /* Configure hitbox axes (4 / 5 / 3) and hit-effect 6, then present it */
    call_object_setupCollisionData_result_battleObject_initSpawnedObject_80D03B8 setup =
        call_object_setupCollisionData_battleObject_initSpawnedObject_80D03B8(col, 4, 5, 3, obj);
    call_object_setCollisionHitEffect_battleObject_initSpawnedObject_80D03B8(6, obj);
    call_object_presentCollisionData_result_battleObject_initSpawnedObject_80D03B8 present =
        call_object_presentCollisionData_battleObject_initSpawnedObject_80D03B8(setup.r1, obj, r8_val);

    /* Spawn SFX and enter the per-frame update state */
    call_PlaySoundEffect_battleObject_initSpawnedObject_80D03B8(0xB3, present.r2);
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
}

/* battleObject_initSpawnedObject_80D03B8(r5 = obj, r8, r10) -> (r5, r8, r10 unchanged) */
Next battleObject_initSpawnedObject_80D03B8(void) {
    init_spawned_object(R5, R8);
    RETURN_TO_CALLER();
}

/* ── battle_advanceObjectXAndDestroyIfInvalid_80D0498 ── */
/* Battle object upkeep: advance the object along X by its velocity, refresh
 * its panel data, and destroy it if it has collided or ended up somewhere
 * it cannot stay.
 *
 * The object_* callees here have no generated wrappers, so each gets a small
 * register-form helper that sets up exactly the registers the original call
 * site had in place. r1 is never written by this code itself: callers see
 * whatever r1 the last executed callee left behind, just as in the original. */

/* Recomputes the object's panel coordinates from its X/Y position.
 * Reached with r0 = new X and r1 = previous X still live. */
static void call_object_setPanelsFromCoordinates_sub_80D0498(u32 object, u32 newX, u32 oldX) { /* register form: calls object_setPanelsFromCoordinates, which has no typed C interface yet (noted by the tool) */
    R0 = newX;
    R1 = oldX;
    R5 = object;
    GUEST_CALL(object_setPanelsFromCoordinates);
}

/* Refreshes the collision panels after the object moved. */
static void call_object_updateCollisionPanels_sub_80D0498(u32 object) { /* register form: calls object_updateCollisionPanels, which has no typed C interface yet (noted by the tool) */
    R5 = object;
    GUEST_CALL(object_updateCollisionPanels);
}

/* Panel properties for a panel coordinate; bit 0x10 marks a panel the
 * object cannot simply sit on. */
static u32 call_object_getPanelParameters_sub_80D0498(u32 panelX, u32 panelY, u32 object) { /* register form: calls object_getPanelParameters, which has no typed C interface yet (noted by the tool) */
    R0 = panelX;
    R1 = panelY;
    R5 = object;
    GUEST_CALL(object_getPanelParameters);
    return R0;
}

/* Nonzero when the panel the object currently occupies may be kept. */
static u32 call_object_isCurrentPanelValid(u32 object) { /* register form: calls object_isCurrentPanelValid, which has no typed C interface yet (noted by the tool) */
    R5 = object;
    GUEST_CALL(object_isCurrentPanelValid);
    return R0;
}

/* Drops the object's active collision region. */
static void call_object_clearCollisionRegion_sub_80D0498(u32 object) { /* register form: calls object_clearCollisionRegion, which has no typed C interface yet (noted by the tool) */
    R5 = object;
    GUEST_CALL(object_clearCollisionRegion);
}

/* Move the object by its X velocity and destroy it if it may not stay.
 * Returns the (unchanged) object pointer. */
static u32 advance_object_x_and_maybe_destroy(u32 object) {
    u32 oldX = LD32(object + oBattleObject_X);
    u32 newX = oldX + LD32(object + oBattleObject_XVelocity);
    u32 collisionFlags;

    ST32(object + oBattleObject_X, newX);
    call_object_setPanelsFromCoordinates_sub_80D0498(object, newX, oldX);
    call_object_updateCollisionPanels_sub_80D0498(object);

    /* Any collision flagged by the last collision check destroys at once. */
    collisionFlags = LD32(LD32(object + oBattleObject_CollisionDataPtr)
                          + oCollisionData_FlagsFromCollision);
    if (collisionFlags == 0) {
        u32 panelParams = call_object_getPanelParameters_sub_80D0498(
            LD8(object + oBattleObject_PanelX),
            LD8(object + oBattleObject_PanelY),
            object);

        /* On a special panel the object only survives while the panel it
         * is standing on right now is still valid. */
        if ((panelParams & 0x10) != 0 && call_object_isCurrentPanelValid(object) != 0)
            return object;
    }

    call_object_clearCollisionRegion_sub_80D0498(object);
    ST32(object + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
         CUR_STATE_DESTROY);
    return object;
}

/* battle_advanceObjectXAndDestroyIfInvalid_80D0498(r5 = battle object) -> r1 (leftover from last callee), r5 */
Next battle_advanceObjectXAndDestroyIfInvalid_80D0498(void) {
    R5 = advance_object_x_and_maybe_destroy(R5);
    RETURN_TO_CALLER();
}

/* ── object_destroyWrapper_80D04D2 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5, r6, r7; bool z, n, c, v; } call_object_genericDestroy_result_sub_80D04D2;
/* register form: generated wrapper: object_genericDestroy has no typed C interface yet */
static inline call_object_genericDestroy_result_sub_80D04D2 call_object_genericDestroy_sub_80D04D2(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_genericDestroy);
    return (call_object_genericDestroy_result_sub_80D04D2){ R0, R1, R2, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Destroy an object and hand the (unchanged) object pointer back to the caller.
 * The real work is done by object_genericDestroy, which takes the object in r5
 * and returns it there as well. */
static u32 destroy_object_sub_80D04D2(u32 object) {
    return call_object_genericDestroy_sub_80D04D2(object).r5;
}

/* object_destroyWrapper_80D04D2: r5 = object -> r5 */
Next object_destroyWrapper_80D04D2(void) {
    R5 = destroy_object_sub_80D04D2(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnObject5FAtPanel_80D04DA ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6, r7; } call_object_spawnType3_result_sub_80D04DA;
/* register form: generated wrapper: object_spawnType3 has no typed C interface yet */
static inline call_object_spawnType3_result_sub_80D04DA call_object_spawnType3_sub_80D04DA(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType3);
    return (call_object_spawnType3_result_sub_80D04DA){ R5, R6, R7 };
}

/* Object kind the spawner is called with (the original loads #0x5F into r0). */
#define SPAWN_KIND 0x5F
/* Header flag bit ORed into the freshly spawned object's flags byte. */
#define OBJECT_FLAG_INIT 0x10

/* What the caller sees in r1/r5 afterwards: r5 is the parent object pointer,
 * unchanged; r1 is the 0x10 left behind by the flag OR when the spawn
 * succeeded, or the original r0 argument (stacked around the call and popped
 * into r1) when it failed. */
typedef struct {
    u32 r1_out;
    u32 r5_out;
} sub_80D04DA_result;

/* Spawn a type-3 object of kind 0x5F and initialise its battle fields.
 *
 * The original stacks r0/r1/r2/r5 around the spawner call, so the spawner is
 * invoked as (0x5F, r1, r2, r3, r4) while the new object receives
 * PanelX = arg r0, PanelY = arg r1, Element = arg r2. The spawner's r5/r6
 * results are the new object pointer and the damage word. The parent's
 * (arg r5) alliance/direction-flip halfword is copied to the new object and
 * bit 0x10 is set in its header flags. If the spawner returns a null object
 * pointer, nothing is written and r1 keeps the original r0 argument. */
static sub_80D04DA_result spawn_and_init_object(u32 panel_x, u32 panel_y, u32 element,
                                                u32 spawn_r3, u32 spawn_r4, u32 parent)
{
    sub_80D04DA_result out;
    out.r1_out = panel_x;          /* r1 after the pop, if the spawn fails */
    out.r5_out = parent;

    call_object_spawnType3_result_sub_80D04DA sp =
        call_object_spawnType3_sub_80D04DA(SPAWN_KIND, panel_y, element, spawn_r3, spawn_r4);
    u32 object = sp.r5;

    if (object != 0) {
        ST8(object + oBattleObject_PanelX, panel_x);
        ST8(object + oBattleObject_PanelY, panel_y);
        ST8(object + oBattleObject_Element, element);
        ST32(object + oBattleObject_DamageAndStaminaDamageCounterDisabler, sp.r6);
        ST16(object + oBattleObject_AllianceAndDirectionFlip,
             LD16(parent + oBattleObject_AllianceAndDirectionFlip));
        ST8(object + oObjectHeader_Flags,
            LD8(object + oObjectHeader_Flags) | OBJECT_FLAG_INIT);
        out.r1_out = OBJECT_FLAG_INIT;   /* mov r1, #0x10 is still live on this path */
    }
    return out;
}

/* battle_spawnObject5FAtPanel_80D04DA(r0 = panel X, r1 = panel Y, r2 = element, r3/r4 = spawn args,
 *              r5 = parent object)
 * -> r1 = 0x10 (or r0 on spawn failure), r5 = parent object */
Next battle_spawnObject5FAtPanel_80D04DA(void) {
    sub_80D04DA_result out = spawn_and_init_object(R0, R1, R2, R3, R4, R5);
    R1 = out.r1_out;
    R5 = out.r5_out;
    RETURN_TO_CALLER();
}

/* ── t3_0x66_80D1218 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x66_80D1218;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x66_80D1218 call_object_updateSpritePaused_t3_0x66_80D1218(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x66_80D1218){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t3_0x66_80D1218: run one battle object's per-state update.
 * r5 = battle object. Reads the object's current state, calls the matching handler
 * from the table at off_80D1230 (battle_initSpawnState / object_bombUpdate_80D12CA /
 * battle_destroyWithRelatedUnlink), then always calls object_updateSpritePaused. */

/* register form: the state handler is reached through a jump table (mov lr, pc; bx r1),
 * so no typed wrapper exists for it. The handlers keep the object pointer in r5, and
 * that (possibly updated) r5 is what the following updateSpritePaused call receives. */
static u32 dispatch_state_handler_t3_0x66_80D1218(u32 handler, u32 state, u32 object) {
    R0 = state;              /* handlers receive the current state in r0 ... */
    R1 = handler;            /* ... with the table word still sitting in r1 at entry */
    R5 = object;
    GUEST_CALL_ADDR(handler);
    return R5;               /* the object pointer as the handler left it */
}

/* r5 = battle object; returns the object pointer as the original leaves it in r5 */
static u32 battle_object_update_by_state(u32 object) {
    u32 state   = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D1230 + state);   /* table is indexed by the raw state byte */

    dispatch_state_handler_t3_0x66_80D1218(handler, state, object);
    return call_object_updateSpritePaused_t3_0x66_80D1218(object).r5;
}

/* inputs: r4 r5 r6 r7 r8 r10 lr — only r5 (the object) is used.
 * outputs: r5 (updated); r8 r9 r10 r11 r12 are left untouched, as in the original. */
Next t3_0x66_80D1218(void) {
    R5 = battle_object_update_by_state(R5);
    RETURN_TO_CALLER();
}

/* ── battle_destroyWithRelatedUnlink ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5, r6, r7; bool z, n, c, v; } call_object_genericDestroy_result_battle_destroyWithRelatedUnlink;
/* register form: generated wrapper: object_genericDestroy has no typed C interface yet */
static inline call_object_genericDestroy_result_battle_destroyWithRelatedUnlink call_object_genericDestroy_battle_destroyWithRelatedUnlink(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_genericDestroy);
    return (call_object_genericDestroy_result_battle_destroyWithRelatedUnlink){ R0, R1, R2, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Clear the related object's back-pointer, then destroy this battle object.
 * Enters with r5 = battle object; returns the (updated) object pointer in r5. */

static u32 destroy_object_and_unlink_related(u32 object) {
    /* If the object references a related object, drop that object's owner header
     * so it no longer points back at the object being destroyed. */
    u32 related = LD32(object + oBattleObject_RelatedObject1Ptr);
    if (related != 0)
        ST32(related + oBattleObject_ObjectHeader, 0);

    return call_object_genericDestroy_battle_destroyWithRelatedUnlink(object).r5;
}

Next battle_destroyWithRelatedUnlink(void) {
    R5 = destroy_object_and_unlink_related(R5);
    RETURN_TO_CALLER();
}

/* ── battle_initSpawnState ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battle_initSpawnState(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r0, r2, r4, r5, r6, r7; } call_battleObject_initSpriteAndCollision_8011504_result;
/* register form: generated wrapper: battleObject_initSpriteAndCollision_8011504 has no typed C interface yet */
static inline call_battleObject_initSpriteAndCollision_8011504_result call_battleObject_initSpriteAndCollision_8011504(u32 r0, u32 r1, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battleObject_initSpriteAndCollision_8011504);
    return (call_battleObject_initSpriteAndCollision_8011504_result){ R0, R2, R4, R5, R6, R7 };
}

typedef struct { u32 r4, r5, r6, r7; } call_spawn_t1_0x0_EffectObject_result_battle_initSpawnState;
/* register form: generated wrapper: spawn_t1_0x0_EffectObject has no typed C interface yet */
static inline call_spawn_t1_0x0_EffectObject_result_battle_initSpawnState call_spawn_t1_0x0_EffectObject_battle_initSpawnState(u32 r1, u32 r2, u32 r3, u32 r4) {
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
    return (call_spawn_t1_0x0_EffectObject_result_battle_initSpawnState){ R4, R5, R6, R7 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result_battle_initSpawnState;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result_battle_initSpawnState call_object_freeMemory_battle_initSpawnState(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result_battle_initSpawnState){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_setCollisionHitEffect has no typed C interface yet */
static inline void call_object_setCollisionHitEffect_battle_initSpawnState(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionHitEffect);
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battle_initSpawnState;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battle_initSpawnState call_sprite_setPalette_battle_initSpawnState(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battle_initSpawnState){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_battle_initSpawnState;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_battle_initSpawnState call_object_clearCollisionRegion_battle_initSpawnState(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_battle_initSpawnState){ R1 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r8; } call_object_presentCollisionData_result_battle_initSpawnState;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battle_initSpawnState call_object_presentCollisionData_battle_initSpawnState(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battle_initSpawnState){ R2, R3, R4, R5, R6, R7, R8 };
}

typedef struct { u32 r5; } call_battle_initSpawnMotion_result;
/* register form: generated wrapper: battle_initSpawnMotion has no typed C interface yet */
static inline call_battle_initSpawnMotion_result call_battle_initSpawnMotion(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_initSpawnMotion);
    return (call_battle_initSpawnMotion_result){ R5 };
}

/* Battle object spawn-state initialization.
 *
 * Inputs come in r5 (battle object), r6/r7 (sprite/collision init data), r8 (collision
 * data pointer). The object either fails sprite setup (its memory is released again and
 * a vanish effect is spawned) or is fully configured: flags, animation, hit effect, HP,
 * palette, collision data and spawn motion, then switched to its update state.
 * Outputs: r5 (object pointer, possibly updated by callees) and r8. */
struct BattleInitSpawnStateResult {
    u32 r5;
    u32 r8;
};

static struct BattleInitSpawnStateResult battle_initSpawnState_impl(u32 obj, u32 r6, u32 r7, u32 r8) {
    /* Place the object on its panel coordinates. */
    call_object_setCoordinatesFromPanels_battle_initSpawnState(obj);

    /* Try to set up sprite and collision with the fixed tile attributes. */
    call_battleObject_initSpriteAndCollision_8011504_result init =
        call_battleObject_initSpriteAndCollision_8011504(0x1034, 0xC0D0100, obj, r6, r7);
    obj = init.r5;

    if (init.r0 == 0) {
        /* Setup failed: detach the related object, spawn the spawn-in effect,
         * then release the object's memory again. r8 is untouched on this path. */
        u32 related = LD32(obj + oBattleObject_RelatedObject1Ptr);
        if (related != 0)
            ST32(related + oBattleObject_ObjectHeader, 0);

        /* Effect arguments are read from obj+0x34; the third gets 0x100000 added. */
        u32 args = obj + 0x34;
        u32 p1 = LD32(args);
        u32 p2 = LD32(args + 4);
        u32 p3 = LD32(args + 8) + (0x10 << 16);

        call_spawn_t1_0x0_EffectObject_result_battle_initSpawnState effect =
            call_spawn_t1_0x0_EffectObject_battle_initSpawnState(p1, p2, p3, 0x14);
        obj = effect.r5;

        call_object_freeMemory_result_battle_initSpawnState freed = call_object_freeMemory_battle_initSpawnState(obj);
        obj = freed.r5;

        struct BattleInitSpawnStateResult out = { obj, r8 };
        return out;
    }

    /* Setup succeeded: mark the object as "has spawned". */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Param1 becomes 1 if it was non-zero, otherwise stays 0; animation index is 2*Param1+1. */
    u32 param1 = LD8(obj + oBattleObject_Param1);
    if (param1 != 0)
        param1 = 1;
    ST8(obj + oBattleObject_Param1, param1);
    ST8(obj + oBattleObject_CurAnim, (param1 << 1) + 1);

    /* No collision hit effect. */
    call_object_setCollisionHitEffect_battle_initSpawnState(0xFF, obj);

    /* HP (and max HP, and the extra var slot) come from Param3. */
    u32 hp = LD8(obj + oBattleObject_Param3);
    ST16(obj + oBattleObject_HP, hp);
    ST16(obj + oBattleObject_MaxHP, hp);
    ST32(obj + oBattleObject_ExtraVars + 8, hp);

    /* Reset palette, collision region, then publish collision data and start spawn motion. */
    call_sprite_setPalette_battle_initSpawnState(0, obj);

    call_object_clearCollisionRegion_result_battle_initSpawnState cleared = call_object_clearCollisionRegion_battle_initSpawnState(obj);

    call_object_presentCollisionData_result_battle_initSpawnState present =
        call_object_presentCollisionData_battle_initSpawnState(cleared.r1, obj, r8);
    obj = present.r5;
    r8 = present.r8;

    call_battle_initSpawnMotion_result motion = call_battle_initSpawnMotion(obj);
    obj = motion.r5;

    /* Tell the related object (if any) that we are active. */
    u32 related = LD32(obj + oBattleObject_RelatedObject1Ptr);
    if (related != 0)
        ST32(related + oBattleObject_ObjectHeader, 2);

    /* Switch to the per-frame update state. */
    ST8(obj + oBattleObject_CurState, CUR_STATE_UPDATE);

    struct BattleInitSpawnStateResult out = { obj, r8 };
    return out;
}

/* battle_initSpawnState(r5 = object, r6, r7 = init data, r8 = collision data) -> r5, r8 */
Next battle_initSpawnState(void) {
    struct BattleInitSpawnStateResult res = battle_initSpawnState_impl(R5, R6, R7, R8);
    R5 = res.r5;
    R8 = res.r8;
    RETURN_TO_CALLER();
}

/* ── object_leapAirborneStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_object_leapAirborneStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r2, r3, r4, r6; } call_object_updateCollisionPanels_result_object_leapAirborneStep;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_object_leapAirborneStep call_object_updateCollisionPanels_object_leapAirborneStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_object_leapAirborneStep){ R2, R3, R4, R6 };
}

typedef struct { u32 r1; } call_object_setCollisionRegion_result_object_leapAirborneStep;
/* register form: generated wrapper: object_setCollisionRegion has no typed C interface yet */
static inline call_object_setCollisionRegion_result_object_leapAirborneStep call_object_setCollisionRegion_object_leapAirborneStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionRegion);
    return (call_object_setCollisionRegion_result_object_leapAirborneStep){ R1 };
}

typedef struct { u32 r0; bool c, v; } call_object_getFrontDirection_result_object_leapAirborneStep;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_object_leapAirborneStep call_object_getFrontDirection_object_leapAirborneStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_object_leapAirborneStep){ R0, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Advance a leaping/airborne battle object one frame: integrate velocity,
 * apply gravity on Z, and if the airborne timer expires, land the object
 * onto the panel grid and reset its action state. */
static u32 leap_airborne_step(u32 obj) {
    /* Integrate each axis: pos += vel */
    u32 x = LD32(obj + oBattleObject_X);
    u32 xvel = LD32(obj + oBattleObject_XVelocity);
    ST32(obj + oBattleObject_X, x + xvel);

    u32 y = LD32(obj + oBattleObject_Y);
    u32 yvel = LD32(obj + oBattleObject_YVelocity);
    ST32(obj + oBattleObject_Y, y + yvel);

    u32 z = LD32(obj + oBattleObject_Z);
    u32 zvel = LD32(obj + oBattleObject_ZVelocity);
    ST32(obj + oBattleObject_Z, z + zvel);

    /* If the leap parameter is set, decelerate Z velocity (gravity) */
    if (LD8(obj + oBattleObject_Param4) != 0) {
        ST32(obj + oBattleObject_ZVelocity, zvel + 0xFFFF8000);
    }

    /* Decrement the airborne timer; if still positive, we remain in the air */
    u16 timer = LD16(obj + oBattleObject_Timer2);
    u16 new_timer = (u16)((s16)timer - 1);
    ST16(obj + oBattleObject_Timer2, new_timer);

    if (new_timer != 0) {
        /* r1 still holds Z velocity from the integration above */
        return zvel;
    }

    /* Timer reached zero: land the object */
    call_object_setCoordinatesFromPanels_object_leapAirborneStep(obj);
    call_object_updateCollisionPanels_object_leapAirborneStep(obj);

    /* Snap Z to ground level */
    ST32(obj + oBattleObject_Z, 0);

    /* Choose collision region size: 1 by default, 24 (0x18) for wide objects */
    u32 region_size = (LD8(obj + oBattleObject_Param1) == 0) ? 1 : 0x18;
    u32 landed_r1 = call_object_setCollisionRegion_object_leapAirborneStep(region_size, obj).r1;

    /* Transition to idle action, reset phase */
    ST8(obj + oBattleObject_CurAction, 4);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);

    /* If leap parameter set, push the object forward at a fixed speed */
    if (LD8(obj + oBattleObject_Param4) != 0) {
        u32 dir = call_object_getFrontDirection_object_leapAirborneStep(obj).r0;
        ST32(obj + oBattleObject_XVelocity, dir * 0x30000);
        /* r1 = 0x30000 (the multiplier loaded just before the mul) */
        return 0x30000;
    }

    return landed_r1;
}

/* object_leapAirborneStep(r5 = battle object ptr) -> r1 */
Next object_leapAirborneStep(void) {
    R1 = leap_airborne_step(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnObject66Linked ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; } call_object_spawnType3_result_battle_spawnObject66Linked;
/* register form: generated wrapper: object_spawnType3 has no typed C interface yet */
static inline call_object_spawnType3_result_battle_spawnObject66Linked call_object_spawnType3_battle_spawnObject66Linked(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType3);
    return (call_object_spawnType3_result_battle_spawnObject66Linked){ R4, R5, R6, R7 };
}

/* Spawn battle object 0x66 and link it to its owner object.
 *
 * object_spawnType3 does the allocation: it is called with r0 = 0x66 and the
 * caller's r1-r4, and hands back the new object pointer in r5 (null when the
 * spawn failed) together with r6/r7 passed through from this function's
 * caller (damage data and related-object pointer). On success this helper
 * fills in the new object from the caller's arguments and from the owner
 * object, then sets header flag 0x10 ("linked") on it.
 *
 * Register quirk of the original: it saves r0-r2 and r5 on the stack, and
 * after the spawn call pops them back shifted by one slot into r1-r3
 * (pop {r1-r3,r5} reloads r1 <- old r0, r2 <- old r1, r3 <- old r2). So the
 * fields come from: panelX <- entry r0, panelY <- entry r1,
 * element <- entry r2, while entry r3 is only forwarded to the spawner.
 * Those popped values are also what the caller sees in r1-r3 afterwards;
 * on success r1 ends as the updated flag byte and r2 as the 0x10 mask.
 */

#define OBJECT_66   0x66   /* battle object id spawned by this helper */
#define OBJFLAG_10  0x10   /* oObjectHeader_Flags bit set on the new object */

typedef struct {
    u32 r1;   /* flag byte | 0x10 on success, entry r0 on failure */
    u32 r2;   /* 0x10 on success, entry r1 on failure */
    u32 r3;   /* entry r2, unchanged */
    u32 r5;   /* entry r5 (owner object), unchanged */
} spawn_object66_result;

static spawn_object66_result spawn_object66_linked(u32 panelX, u32 panelY, u32 element,
                                                   u32 spawnArg3, u32 spawnArg4,
                                                   u32 ownerObj)
{
    call_object_spawnType3_result_battle_spawnObject66Linked spawn =
        call_object_spawnType3_battle_spawnObject66Linked(OBJECT_66, panelY, element, spawnArg3, spawnArg4);
    u32 newObj = spawn.r5;

    if (newObj == 0)
        return (spawn_object66_result){ panelX, panelY, element, ownerObj };

    ST8 (newObj + oBattleObject_Element, element);      /* old r2 */
    ST8 (newObj + oBattleObject_PanelX, panelX);        /* old r0 */
    ST8 (newObj + oBattleObject_PanelY, panelY);        /* old r1 */
    ST32(newObj + oBattleObject_DamageAndStaminaDamageCounterDisabler, spawn.r6);
    ST16(newObj + oBattleObject_AllianceAndDirectionFlip,
         LD16(ownerObj + oBattleObject_AllianceAndDirectionFlip));
    ST32(newObj + oBattleObject_RelatedObject1Ptr, spawn.r7);

    u32 flags = LD8(newObj + oObjectHeader_Flags) | OBJFLAG_10;
    ST8(newObj + oObjectHeader_Flags, flags);

    /* The original leaves r1 = new flag byte and r2 = 0x10 behind. */
    return (spawn_object66_result){ flags, OBJFLAG_10, element, ownerObj };
}

/* battle_spawnObject66Linked
 * inputs:  r0 = panelX, r1 = panelY, r2 = element, r3/r4 = forwarded to
 *          object_spawnType3, r5 = owner object (r6/r7 forwarded through it)
 * outputs: r1 r2 r3 r5   clobbers: r0, flags */
Next battle_spawnObject66Linked(void) {
    spawn_object66_result res = spawn_object66_linked(R0, R1, R2, R3, R4, R5);
    R1 = res.r1;
    R2 = res.r2;
    R3 = res.r3;
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── battle_initSpawnMotion ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFrontDirection_result_battle_initSpawnMotion;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battle_initSpawnMotion call_object_getFrontDirection_battle_initSpawnMotion(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battle_initSpawnMotion){ R0 };
}

typedef struct { u32 r0, r1; } call_object_getCoordinatesForPanels_result_battle_initSpawnMotion;
/* register form: generated wrapper: object_getCoordinatesForPanels has no typed C interface yet */
static inline call_object_getCoordinatesForPanels_result_battle_initSpawnMotion call_object_getCoordinatesForPanels_battle_initSpawnMotion(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_getCoordinatesForPanels);
    return (call_object_getCoordinatesForPanels_result_battle_initSpawnMotion){ R0, R1 };
}

typedef struct { u32 r0, r1, r2, r3, r5; } call_math_getThrowSpeeds_result;
/* register form: generated wrapper: math_getThrowSpeeds has no typed C interface yet */
static inline call_math_getThrowSpeeds_result call_math_getThrowSpeeds(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R6 = r6;
    GUEST_CALL(math_getThrowSpeeds);
    return (call_math_getThrowSpeeds_result){ R0, R1, R2, R3, R5 };
}

/* QuickDivide BIOS helper (svc 6): quotient = dividend / divisor. */
/* register form: GBA BIOS QuickDivide takes dividend in r0 and divisor in r1 */
static u32 quick_divide(u32 dividend, u32 divisor) {
    R0 = dividend;
    R1 = divisor;
    swi_call(6);
    return R0;
}

#define SPAWN_STRUCT_OFFSET   0x34
#define THROW_TIMER_VALUE     0xD
#define THROW_HEIGHT          0xFFFF8000u
#define DEFAULT_X_SPEED       0x1C000u
#define DEFAULT_Z_SPEED       0xFFFE0000u
#define Z_GRAVITY_STEP        0x00020000u  /* -DEFAULT_Z_SPEED as unsigned */

static void battle_initSpawnMotion_impl(u32 obj) {
    u32 param4 = LD8(obj + oBattleObject_Param4);

    if (param4 != 0) {
        /* Thrown spawn: advance one panel forward, compute parabolic arc speeds. */
        u32 front_dir = call_object_getFrontDirection_battle_initSpawnMotion(obj).r0;
        u32 new_panel_x = front_dir + LD8(obj + oBattleObject_PanelX);
        ST8(obj + oBattleObject_PanelX, new_panel_x);

        u32 panel_y = LD8(obj + oBattleObject_PanelY);
        call_object_getCoordinatesForPanels_result_battle_initSpawnMotion coords =
            call_object_getCoordinatesForPanels_battle_initSpawnMotion(new_panel_x, panel_y);

        ST16(obj + oBattleObject_Timer2, THROW_TIMER_VALUE);

        /* register form: call_math_getThrowSpeeds writes R4, R5, R6;
         * these must be preserved across this function per the contract. */
        u32 saved_r4 = R4;
        u32 saved_r5 = R5;
        u32 saved_r6 = R6;
        call_math_getThrowSpeeds_result speeds =
            call_math_getThrowSpeeds(obj + SPAWN_STRUCT_OFFSET,
                                     coords.r0,
                                     coords.r1,
                                     0,
                                     THROW_TIMER_VALUE,
                                     THROW_HEIGHT);
        R4 = saved_r4;
        R5 = saved_r5;
        R6 = saved_r6;

        ST32(obj + oBattleObject_XVelocity, speeds.r0);
        ST32(obj + oBattleObject_YVelocity, speeds.r1);
        ST32(obj + oBattleObject_ZVelocity, speeds.r2);
    } else {
        /* In-place spawn with a fixed vertical fall; adjust X to land on panel. */
        ST32(obj + oBattleObject_YVelocity, 0);

        u32 front_dir = call_object_getFrontDirection_battle_initSpawnMotion(obj).r0;
        ST32(obj + oBattleObject_XVelocity, front_dir * DEFAULT_X_SPEED);
        ST32(obj + oBattleObject_ZVelocity, DEFAULT_Z_SPEED);

        /* fall_time = Z / gravity_step; shift X back by fall_time * x_vel. */
        u32 z = LD32(obj + oBattleObject_Z);
        u32 fall_time = quick_divide(z, Z_GRAVITY_STEP);
        ST16(obj + oBattleObject_Timer2, fall_time);

        u32 x_vel = LD32(obj + oBattleObject_XVelocity);
        u32 x = LD32(obj + oBattleObject_X);
        ST32(obj + oBattleObject_X, x - (fall_time * x_vel));
    }
}

/* battle_initSpawnMotion(r5 = battle object) -> r5 */
Next battle_initSpawnMotion(void) {
    battle_initSpawnMotion_impl(R5);
    RETURN_TO_CALLER();
}

/* ── t3_0x6c_80D1C20 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x6c_80D1C20;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x6c_80D1C20 call_object_updateSpritePaused_t3_0x6c_80D1C20(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x6c_80D1C20){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* State tick for a battle object (r5): run the handler for the object's
 * current state, then refresh its paused-sprite bookkeeping.
 *
 * The state table at off_80D1C38 is indexed by the raw CurState byte
 * (the object's states are stored already scaled to the 4-byte stride).
 * The handler is entered with r0 = CurState, r1 = its own table entry,
 * r5 = object, and the tick's other registers live; the handlers keep
 * their locals in the high registers, so their r5/r8-r12 results are
 * what the tick's callers observe afterwards. */
typedef struct {
    u32 r5, r8, r9, r10, r11, r12;
} StateHandlerRegs_t3_0x6c_80D1C20;

/* Branch to the selected handler with `mov lr, pc; bx r1` semantics: the
 * handler returns back here, and every register it leaves behind that our
 * callers still see has to survive the call.
 * register form: jump-table dispatch through a code address; the callee's
 * r5/r8-r12 results must be carried across the call. */
static StateHandlerRegs_t3_0x6c_80D1C20 dispatch_state_handler_t3_0x6c_80D1C20(u32 handler, u32 cur_state,
                                               u32 r4, u32 r5, u32 r6, u32 r7,
                                               u32 r8, u32 r10)
{
    R0 = cur_state;
    R1 = handler;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R8 = r8;
    R10 = r10;
    GUEST_CALL_ADDR(handler);

    StateHandlerRegs_t3_0x6c_80D1C20 out;
    out.r5 = R5;
    out.r8 = R8;
    out.r9 = R9;
    out.r10 = R10;
    out.r11 = R11;
    out.r12 = R12;
    return out;
}

/* Run the object's current state handler, then update the sprite pause
 * state with whatever object pointer the handler left in r5. */
static StateHandlerRegs_t3_0x6c_80D1C20 state_tick(u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r10)
{
    u32 cur_state = LD8(r5 + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D1C38 + cur_state);

    StateHandlerRegs_t3_0x6c_80D1C20 st = dispatch_state_handler_t3_0x6c_80D1C20(handler, cur_state,
                                                 r4, r5, r6, r7, r8, r10);

    call_object_updateSpritePaused_result_t3_0x6c_80D1C20 upd = call_object_updateSpritePaused_t3_0x6c_80D1C20(st.r5);
    st.r5 = upd.r5;
    return st;
}

Next t3_0x6c_80D1C20(void) {
    StateHandlerRegs_t3_0x6c_80D1C20 st = state_tick(R4, R5, R6, R7, R8, R10);
    R5 = st.r5;
    R8 = st.r8;
    R9 = st.r9;
    R10 = st.r10;
    R11 = st.r11;
    R12 = st.r12;
    RETURN_TO_CALLER();
}

/* ── object_releaseRelatedAndDestroy_80D1C44 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5, r6, r7; bool z, n, c, v; } call_object_genericDestroy_result_sub_80D1C44;
/* register form: generated wrapper: object_genericDestroy has no typed C interface yet */
static inline call_object_genericDestroy_result_sub_80D1C44 call_object_genericDestroy_sub_80D1C44(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_genericDestroy);
    return (call_object_genericDestroy_result_sub_80D1C44){ R0, R1, R2, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Release this object's related object (drop its refcount), clear the extra
 * variable slot, and run the generic object destructor on this object.
 * r5 (the object pointer) passes through unchanged: object_genericDestroy
 * preserves it, so the caller still sees the same object in r5 afterwards. */
static u32 release_object_and_related(u32 self) {
    /* Drop one reference on the related object's shared header */
    u32 related = LD32(self + oBattleObject_RelatedObject1Ptr);
    ST32(related + oBattleObject_ObjectHeader,
         LD32(related + oBattleObject_ObjectHeader) - 1);

    /* Clear the object's extra variable slot */
    ST32(LD32(self + oBattleObject_ExtraVars + 8), 0);

    call_object_genericDestroy_sub_80D1C44(self);
    return self;
}

/* object_releaseRelatedAndDestroy_80D1C44(r5 = object) -> r5 */
Next object_releaseRelatedAndDestroy_80D1C44(void) {
    R5 = release_object_and_related(R5);
    RETURN_TO_CALLER();
}

/* ── object_runCollisionState_80D1D14 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_removeCollisionData_result_object_runCollisionState_80D1D14;
/* register form: generated wrapper: object_removeCollisionData has no typed C interface yet */
static inline call_object_removeCollisionData_result_object_runCollisionState_80D1D14 call_object_removeCollisionData_object_runCollisionState_80D1D14(u32 r4, u32 r5, u32 r7) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(object_removeCollisionData);
    return (call_object_removeCollisionData_result_object_runCollisionState_80D1D14){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_spawnCollisionEffect_result_object_runCollisionState_80D1D14;
/* register form: generated wrapper: object_spawnCollisionEffect has no typed C interface yet */
static inline call_object_spawnCollisionEffect_result_object_runCollisionState_80D1D14 call_object_spawnCollisionEffect_object_runCollisionState_80D1D14(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_spawnCollisionEffect);
    return (call_object_spawnCollisionEffect_result_object_runCollisionState_80D1D14){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r3; } call_battle_isBattleOver_result;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result call_battle_isBattleOver(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result){ R0, R3 };
}

typedef struct { u32 r0; bool c, v; } call_object_isCurrentPanelValid_result_object_runCollisionState_80D1D14;
/* register form: generated wrapper: object_isCurrentPanelValid has no typed C interface yet */
static inline call_object_isCurrentPanelValid_result_object_runCollisionState_80D1D14 call_object_isCurrentPanelValid_object_runCollisionState_80D1D14(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_isCurrentPanelValid);
    return (call_object_isCurrentPanelValid_result_object_runCollisionState_80D1D14){ R0, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_object_runCollisionState_80D1D14;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_object_runCollisionState_80D1D14 call_object_clearCollisionRegion_object_runCollisionState_80D1D14(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_object_runCollisionState_80D1D14){ R1 };
}

typedef struct { u32 r1; bool z, n; } call_battle_isTimeStop_result_object_runCollisionState_80D1D14;
/* register form: generated wrapper: battle_isTimeStop has no typed C interface yet */
static inline call_battle_isTimeStop_result_object_runCollisionState_80D1D14 call_battle_isTimeStop_object_runCollisionState_80D1D14(void) {
    GUEST_CALL(battle_isTimeStop);
    return (call_battle_isTimeStop_result_object_runCollisionState_80D1D14){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r4, r5, r6, r7, r8; } call_object_presentCollisionData_result_object_runCollisionState_80D1D14;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_object_runCollisionState_80D1D14 call_object_presentCollisionData_object_runCollisionState_80D1D14(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_object_runCollisionState_80D1D14){ R4, R5, R6, R7, R8 };
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_object_highlightCurrentCollisionPanels_result;
/* register form: generated wrapper: object_highlightCurrentCollisionPanels has no typed C interface yet */
static inline call_object_highlightCurrentCollisionPanels_result call_object_highlightCurrentCollisionPanels(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_highlightCurrentCollisionPanels);
    return (call_object_highlightCurrentCollisionPanels_result){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* object_runCollisionState_80D1D14 – advance one collision-state tick for a battle object.
 *
 * Removes old collision data, spawns a hit effect, checks whether the object should
 * be destroyed (battle over / off-panel / collision flags), and if not, dispatches
 * the current collision action (or skips it during time-stop) then re-presents
 * collision geometry and highlights affected panels.
 *
 * Inputs:  r4 r5(object ptr) r7 r8 r10 lr
 * Outputs: r5 r8 (modified by presentCollisionData), r9 r10 r11 r12 (pass-through)
 * Preserved: r4 r7 (saved by original's push/pop; framework handles)
 */

/* register form: the jump-table dispatch (bx) modifies R1, R5, R8 in place;
   the subsequent presentCollisionData and highlight wrappers must read those
   post-dispatch register values, which plain C cannot capture. */
static void collision_tail(u32 ts_r1, bool z_set) {
    if (!z_set) {
        /* Time is stopped: skip the action dispatch, present directly. */
        call_object_presentCollisionData_object_runCollisionState_80D1D14(ts_r1, R5, R8);
    } else {
        /* Normal time: dispatch the collision action through the jump table. */
        u32 action = LD8(R5 + oBattleObject_CurAction);
        u32 target = LD32(A_off_80D1D84 + action);
        GUEST_CALL_ADDR(target);
        /* R1, R5, R8 now hold whatever the dispatched function left. */
        call_object_presentCollisionData_object_runCollisionState_80D1D14(R1, R5, R8);
    }
    call_object_highlightCurrentCollisionPanels(R5);
}

static void collision_state_logic(u32 r4, u32 r5, u32 r7, u32 r8) {
    /* Tear down previous collision geometry and spawn a hit effect. */
    call_object_removeCollisionData_result_object_runCollisionState_80D1D14 rem = call_object_removeCollisionData_object_runCollisionState_80D1D14(r4, r5, r7);
    r5 = rem.r5;
    r7 = rem.r7;

    call_object_spawnCollisionEffect_result_object_runCollisionState_80D1D14 spn = call_object_spawnCollisionEffect_object_runCollisionState_80D1D14(r5);
    r5 = spn.r5;
    r7 = spn.r7;

    /* If the battle has ended, destroy this object. */
    call_battle_isBattleOver_result over = call_battle_isBattleOver();
    if (over.r0 != 0)
        goto destroy;

    /* If the object is no longer on a valid panel, destroy it. */
    call_object_isCurrentPanelValid_result_object_runCollisionState_80D1D14 panel = call_object_isCurrentPanelValid_object_runCollisionState_80D1D14(r5);
    if (panel.r0 == 0)
        goto destroy;

    /* Inspect collision-data flags to decide whether to destroy or advance action. */
    r7 = LD32(r5 + oBattleObject_CollisionDataPtr);
    u32 flags = LD32(r7 + oCollisionData_FlagsFromCollision);

    if (flags != 0) {
        /* Non-zero pending-hit counter at offset 0x84 means resolution still in progress. */
        u16 pending = LD16(r7 + 0x84);
        if (pending != 0)
            goto destroy;

        /* Bits 0x3800000: fatal collision (wall/shield break) – destroy. */
        if (flags & 0x3800000)
            goto destroy;

        /* Bits 0xC000000: enemy body hit – switch to attack action (index 4). */
        if (flags & 0xC000000) {
            call_object_clearCollisionRegion_object_runCollisionState_80D1D14(r5);

            if (LD8(r5 + oBattleObject_CurAction) != 4) {
                ST8(r5 + oBattleObject_CurAction, 4);
                ST16(r5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
            }
        }
    }

    /* Advance or skip the per-action collision update, then re-present and highlight. */
    call_battle_isTimeStop_result_object_runCollisionState_80D1D14 ts = call_battle_isTimeStop_object_runCollisionState_80D1D14();
    collision_tail(ts.r1, ts.z);
    return;

destroy:
    call_object_clearCollisionRegion_object_runCollisionState_80D1D14(r5);
    ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
}

/* object_runCollisionState_80D1D14(r4 r5 r7 r8 r10) -> r5 r8 r9 r10 r11 r12 */
Next object_runCollisionState_80D1D14(void) {
    collision_state_logic(R4, R5, R7, R8);
    RETURN_TO_CALLER();
}

/* ── object_updateMoveStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; bool c, v; } call_object_isCurrentPanelValid_result_object_updateMoveStep;
/* register form: generated wrapper: object_isCurrentPanelValid has no typed C interface yet */
static inline call_object_isCurrentPanelValid_result_object_updateMoveStep call_object_isCurrentPanelValid_object_updateMoveStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_isCurrentPanelValid);
    return (call_object_isCurrentPanelValid_result_object_updateMoveStep){ R0, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_object_updateMoveStep;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_object_updateMoveStep call_object_clearCollisionRegion_object_updateMoveStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_object_updateMoveStep){ R1 };
}

typedef struct { u32 r5; } call_object_stepUpdateByVelocity_result;
/* register form: generated wrapper: object_stepUpdateByVelocity has no typed C interface yet */
static inline call_object_stepUpdateByVelocity_result call_object_stepUpdateByVelocity(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_stepUpdateByVelocity);
    return (call_object_stepUpdateByVelocity_result){ R5 };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_object_updateMoveStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r5, r6; } call_object_setPanelsFromCoordinates_result_object_updateMoveStep;
/* register form: generated wrapper: object_setPanelsFromCoordinates has no typed C interface yet */
static inline call_object_setPanelsFromCoordinates_result_object_updateMoveStep call_object_setPanelsFromCoordinates_object_updateMoveStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setPanelsFromCoordinates);
    return (call_object_setPanelsFromCoordinates_result_object_updateMoveStep){ R5, R6 };
}

typedef struct { u32 r1, r2, r3, r4, r6; } call_object_updateCollisionPanels_result_object_updateMoveStep;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_object_updateMoveStep call_object_updateCollisionPanels_object_updateMoveStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_object_updateMoveStep){ R1, R2, R3, R4, R6 };
}

struct move_step_result {
    u32 r1;
    u32 r5;
};

/* Advance one step of a moving object: validate its panel, integrate velocity,
 * recalculate occupied panels and collision.  If the panel check fails the
 * object is destroyed instead. */
static struct move_step_result update_move_step(u32 obj) {
    /* Is the object still on a walkable panel? */
    call_object_isCurrentPanelValid_result_object_updateMoveStep valid = call_object_isCurrentPanelValid_object_updateMoveStep(obj);
    if (valid.r0 == 0) {
        /* Invalid: tear down collision and switch to the destroyed state. */
        call_object_clearCollisionRegion_result_object_updateMoveStep cleared = call_object_clearCollisionRegion_object_updateMoveStep(obj);
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
        ST8(obj + oBattleObject_CurAction, 2);
        struct move_step_result res = { cleared.r1, obj };
        return res;
    }

    /* On the first frame of a move, record the starting panel and initialise
     * the velocity step. */
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        ST8(obj + oBattleObject_CurPhase, 1);
        u32 start_panel = (u32)LD8(obj + oBattleObject_PanelX)
                        | ((u32)LD8(obj + oBattleObject_PanelY) << 4);
        ST32(obj + oBattleObject_ExtraVars + 0x18, start_panel);
        obj = call_object_stepUpdateByVelocity(obj).r5;
        ST16(obj + oBattleObject_Timer, LD16(obj + oBattleObject_Timer2));
        call_object_setCoordinatesFromPanels_object_updateMoveStep(obj);
    }

    /* Integrate velocity into the 16.16 sub-pixel position. */
    ST32(obj + oBattleObject_X,
         LD32(obj + oBattleObject_X) + LD32(obj + oBattleObject_XVelocity));
    ST32(obj + oBattleObject_Y,
         LD32(obj + oBattleObject_Y) + LD32(obj + oBattleObject_YVelocity));

    /* Re-derive the grid panels from the new coordinates and refresh collision. */
    obj = call_object_setPanelsFromCoordinates_object_updateMoveStep(obj).r5;
    call_object_updateCollisionPanels_result_object_updateMoveStep coll = call_object_updateCollisionPanels_object_updateMoveStep(obj);

    /* Tick down the move timer; when it expires, reset the phase so the
     * object can begin its next move. */
    s32 timer_after = (s32)LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, (u16)timer_after);
    if (timer_after <= 0) {
        ST8(obj + oBattleObject_CurPhase, 0);
    }

    struct move_step_result res = { coll.r1, obj };
    return res;
}

/* object_updateMoveStep(r5 = object, r10) -> r1, r5 */
Next object_updateMoveStep(void) {
    struct move_step_result res = update_move_step(R5);
    R1 = res.r1;
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── battle_dispatchObjectPhaseHandler_80D1DEE ── */
/* Battle object phase dispatcher: reads the object's CurPhase byte and calls
 * the matching handler from a two-entry table
 *   0 -> battle_draggedObjectCheckTargetPanel (normal phase)
 *   1 -> battle_objectDraggedPhaseHandler
 * The handler is entered with r0 = phase byte, r5 = object pointer; r10 (the
 * battle context) passes through untouched. Everything the handler leaves in
 * r1/r5/r8-r12 is this function's result, so nothing is copied back here. */

/* register form: indirect call through the phase handler table; the callee
 * reads r0 (phase), r5 (object) and r10 (context) on entry */
static u32 call_phase_handler(u32 handler, u32 phase, u32 obj, u32 context) {
    R0 = phase;
    R5 = obj;
    R10 = context;
    GUEST_CALL_ADDR(handler);
    return R0;
}

static void sub_80D1DEE_dispatch(u32 obj, u32 context) {
    u32 phase = LD8(obj + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80D1E00 + phase);
    call_phase_handler(handler, phase, obj, context);
}

/* battle_dispatchObjectPhaseHandler_80D1DEE(r5 = battle object, r10 = battle context) */
Next battle_dispatchObjectPhaseHandler_80D1DEE(void) {
    sub_80D1DEE_dispatch(R5, R10);
    RETURN_TO_CALLER();
}

/* ── battle_objectDraggedPhaseHandler ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battle_objectDraggedPhaseHandler(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r1, r2, r3, r4, r6; } call_object_updateCollisionPanels_result_battle_objectDraggedPhaseHandler;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_battle_objectDraggedPhaseHandler call_object_updateCollisionPanels_battle_objectDraggedPhaseHandler(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_battle_objectDraggedPhaseHandler){ R1, R2, R3, R4, R6 };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_battle_objectDraggedPhaseHandler;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_battle_objectDraggedPhaseHandler call_object_clearCollisionRegion_battle_objectDraggedPhaseHandler(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_battle_objectDraggedPhaseHandler){ R1 };
}

typedef struct { u32 r1; } call_object_setCollisionRegion_result_battle_objectDraggedPhaseHandler;
/* register form: generated wrapper: object_setCollisionRegion has no typed C interface yet */
static inline call_object_setCollisionRegion_result_battle_objectDraggedPhaseHandler call_object_setCollisionRegion_battle_objectDraggedPhaseHandler(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionRegion);
    return (call_object_setCollisionRegion_result_battle_objectDraggedPhaseHandler){ R1 };
}

/* Handle one phase tick of a dragged battle object:
 * - Initialize phase state on first tick (5-frame countdown).
 * - If a drag-target is attached (ExtraVars+0x14), copy its panel coords
 *   and refresh collision panels.
 * - Run two nested countdown timers: main (Timer) and secondary (Timer2).
 * - On timer expiry either re-arm the collision region or destroy the object.
 * Returns r1 (collision result / placeholder). */
static u32 battle_objectDraggedPhaseHandler_impl(u32 obj) {
    /* First tick: mark initialized, load 5-frame primary timer. */
    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        ST8(obj + oBattleObject_PhaseInitialized, 1);
        ST16(obj + oBattleObject_Timer, 5);
    }

    /* Drag target pointer stored in the extra-variables slot at +0x14. */
    u32 dragTarget = LD32(obj + (oBattleObject_ExtraVars + 0x14));
    u32 r1 = dragTarget;  /* 0 when no target (original ldr leaves 0) */

    if (dragTarget != 0) {
        u8 panelX = LD8(dragTarget + 0x12);
        u8 panelY = LD8(dragTarget + 0x13);
        ST8(obj + oBattleObject_PanelX, panelX);
        ST8(obj + oBattleObject_PanelY, panelY);
        call_object_setCoordinatesFromPanels_battle_objectDraggedPhaseHandler(obj);
        r1 = call_object_updateCollisionPanels_battle_objectDraggedPhaseHandler(obj).r1;
    }

    /* Decrement primary 16-bit timer (32-bit sub, 16-bit store). */
    s32 timer = (s32)LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, timer);
    if (timer > 0)
        return r1;

    /* Primary expired – decrement secondary timer. */
    s32 timer2 = (s32)LD16(obj + oBattleObject_Timer2) - 1;
    ST16(obj + oBattleObject_Timer2, timer2);

    if (timer2 > 0) {
        /* Re-enable collision region and reset phase for next cycle. */
        r1 = call_object_setCollisionRegion_battle_objectDraggedPhaseHandler(1, obj).r1;
        ST8(obj + oBattleObject_PhaseInitialized, 0);
    } else {
        /* Both timers expired: tear down collision, mark destroyed. */
        r1 = call_object_clearCollisionRegion_battle_objectDraggedPhaseHandler(obj).r1;
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
             CUR_STATE_DESTROY);
    }

    return r1;
}

/* battle_objectDraggedPhaseHandler(r5 = battle object ptr) -> r1 */
Next battle_objectDraggedPhaseHandler(void) {
    R1 = battle_objectDraggedPhaseHandler_impl(R5);
    RETURN_TO_CALLER();
}

/* ── object_blinkOutUpdate ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline void call_object_clearCollisionRegion_object_blinkOutUpdate(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
}

/* Blink-out update for a battle object (runs while the object is disappearing).
 *
 * Entry state: oBattleObject_CurPhase == 0 means this is the first tick, so the
 * object stops colliding and gets a 10-frame blink timer.  Every tick it sets
 * the "blinking" bit in the object flags, clears it again on frames where the
 * timer's bit 1 is clear (the visible frames of the blink), counts the timer
 * down, and switches the object to the destroy state once it reaches zero.
 *
 * Register contract: in r5 = object, out r1 = 2 (the blink flag bit, the last
 * value the original loads into r1); nz/c/v come from the `sub r0,#1` on the
 * timer, i.e. CMP(old_timer, 1). */
typedef struct {
    u32 old_timer; /* timer value before the decrement; adapter sets flags from old_timer - 1 */
    u32 r1;        /* value the original leaves in r1 on exit */
} BlinkOutResult;

static BlinkOutResult blink_out_update(u32 obj) {
    BlinkOutResult out;

    /* First tick: start the blink, drop the collision region, arm the timer. */
    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        ST8(obj + oBattleObject_CurPhase, 1);
        call_object_clearCollisionRegion_object_blinkOutUpdate(obj);
        ST16(obj + oBattleObject_Timer, 0xA);
    }

    /* Mark the object as blinking out for the object layer. */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2u);

    /* Hide the sprite only on frames where timer bit 1 is set:
     * on the visible frames clear the bit again. */
    if ((LD16(obj + oBattleObject_Timer) & 2u) == 0)
        ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) & ~2u);

    /* Count the blink timer down; at zero the object destroys itself. */
    out.old_timer = LD16(obj + oBattleObject_Timer);
    u32 timer = out.old_timer - 1;
    ST16(obj + oBattleObject_Timer, timer);
    if (timer == 0)
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);

    out.r1 = 2;
    return out;
}

/* object_blinkOutUpdate(r5 = object) -> r1, flags from timer-1 */
Next object_blinkOutUpdate(void) {
    BlinkOutResult res = blink_out_update(R5);
    R1 = res.r1;
    SET_FLAGS_CMP(res.old_timer, 1);
    RETURN_TO_CALLER();
}

/* ── battle_spawnInitializedObject6c_80D1EB8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6, r7; } call_object_spawnType3_result_sub_80D1EB8;
/* register form: generated wrapper: object_spawnType3 has no typed C interface yet */
static inline call_object_spawnType3_result_sub_80D1EB8 call_object_spawnType3_sub_80D1EB8(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType3);
    return (call_object_spawnType3_result_sub_80D1EB8){ R5, R6, R7 };
}

/* Spawn a 0x6C-byte battle object and initialize it from the caller's context.
 *
 * The original shuffles its arguments through the stack (push {r0-r3,r5} /
 * pop {r1-r5}) around the spawn call, so which value a caller sees in r1/r2
 * depends on the path taken:
 *   - spawn failed (object pointer 0):  r1 = entry r0, r2 = entry r1
 *   - spawn succeeded:                  r1 = header flags | 0x10 (the value
 *     just stored), r2 = 0x10 (the ORR mask left behind by the tail code)
 * r5 always keeps its entry value (the source object pointer), which is also
 * where the alliance/direction halfword is copied from.
 *
 * The spawn call itself receives (size 0x6C, entry r1, entry r2, 0, entry r4)
 * and yields the new object pointer in r5 plus the damage-counter value (r6)
 * and related-object pointer (r7) to store into it. */
typedef struct { u32 out_r1, out_r2, out_r5; } SpawnBattleObjectResult;

static SpawnBattleObjectResult spawn_battle_object(u32 arg_r0, u32 arg_r1, u32 arg_r2,
                                                   u32 arg_r3, u32 arg_r4, u32 sourceObj) {
    SpawnBattleObjectResult out = { arg_r0, arg_r1, sourceObj };

    call_object_spawnType3_result_sub_80D1EB8 spawn =
        call_object_spawnType3_sub_80D1EB8(0x6C, arg_r1, arg_r2, 0, arg_r4);
    u32 obj = spawn.r5;
    if (obj == 0)
        return out;                    /* spawn failed: nothing to initialize */

    /* Fields come from the saved arguments, shifted down one register. */
    ST8(obj + oBattleObject_PanelX, arg_r0);
    ST8(obj + oBattleObject_PanelY, arg_r1);
    ST32(obj + oBattleObject_ExtraVars + 8, arg_r2);
    ST32(obj + oBattleObject_ExtraVars + 0xC, arg_r3);
    ST32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler, spawn.r6);
    ST8(obj + oBattleObject_Element, 4);
    ST16(obj + oBattleObject_AllianceAndDirectionFlip,
         LD16(sourceObj + oBattleObject_AllianceAndDirectionFlip));
    ST32(obj + oBattleObject_RelatedObject1Ptr, spawn.r7);

    /* Mark the object header; the stored value is also what ends up in r1,
     * and the ORR mask 0x10 is what ends up in r2. */
    u32 flags = LD8(obj + oObjectHeader_Flags) | 0x10;
    ST8(obj + oObjectHeader_Flags, flags);
    out.out_r1 = flags;
    out.out_r2 = 0x10;
    return out;
}

/* battle_spawnInitializedObject6c_80D1EB8: spawn an initialized battle object.
 * inputs r0-r7; outputs r1, r2 (path dependent, see above) and r5 = entry r5. */
Next battle_spawnInitializedObject6c_80D1EB8(void) {
    SpawnBattleObjectResult out = spawn_battle_object(R0, R1, R2, R3, R4, R5);
    R1 = out.out_r1;
    R2 = out.out_r2;
    R5 = out.out_r5;
    RETURN_TO_CALLER();
}

/* ── sub_80D1EE8 ── */
/* Reset an object's HP gauge state: kind = 8, value = 0.
 * Writes byte 8 at offset 9 and clears the halfword at offset 10 of the struct in r0. */
static void reset_hp_gauge_state(u32 obj)
{
    ST8(obj + 9, 8);
    ST16(obj + 0xA, 0);
}

/* sub_80D1EE8(r0 = struct address) -> nothing */
Next sub_80D1EE8(void) {
    reset_hp_gauge_state(R0);
    RETURN_TO_CALLER();
}

/* ── object_stepUpdateByVelocity ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r5; } call_sub_80D1F0E_result;
/* register form: generated wrapper: battleObject_advanceEnemyTargetList_80D1F0E has no typed C interface yet */
static inline call_sub_80D1F0E_result call_sub_80D1F0E(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_advanceEnemyTargetList_80D1F0E);
    return (call_sub_80D1F0E_result){ R0, R1, R5 };
}

/* register form: generated wrapper: object_bounceAtTargetColumn has no typed C interface yet */
static inline void call_object_bounceAtTargetColumn(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_bounceAtTargetColumn);
}

/* register form: generated wrapper: object_stepRedirectTowardTargetPanel has no typed C interface yet */
static inline void call_object_stepRedirectTowardTargetPanel(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_stepRedirectTowardTargetPanel);
}

typedef struct { u32 r5; } call_sub_80D1FD8_result;
/* register form: generated wrapper: battleObject_updateDirectionFlip has no typed C interface yet */
static inline call_sub_80D1FD8_result call_sub_80D1FD8(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_updateDirectionFlip);
    return (call_sub_80D1FD8_result){ R5 };
}

/* Advance one battle object by its velocity.
 *
 * r5 is the object. battleObject_advanceEnemyTargetList_80D1F0E does the common pre-step work and may return
 * an updated object pointer in r5 alongside two values in r0/r1 that the
 * movement helpers below consume. If the object has no X velocity it bounces
 * at its target column, otherwise it steers toward the target panel. Finally
 * battleObject_updateDirectionFlip finishes the step and returns the (possibly changed) object.
 *
 * inputs:  r5 (object), r10 (context, passed through untouched)
 * outputs: r5 (object)
 * clobbers: nz c v r0-r3 lr (all handled inside the wrappers) */
static u32 step_object_by_velocity(u32 object) {
    call_sub_80D1F0E_result pre = call_sub_80D1F0E(object);
    object = pre.r5;

    u32 xVelocity = LD32(object + oBattleObject_XVelocity);
    if (xVelocity == 0)
        call_object_bounceAtTargetColumn(pre.r0, pre.r1, object);
    else
        call_object_stepRedirectTowardTargetPanel(pre.r0, pre.r1, object);

    return call_sub_80D1FD8(object).r5;
}

Next object_stepUpdateByVelocity(void) {
    R5 = step_object_by_velocity(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_advanceEnemyTargetList_80D1F0E ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5, r6, r7; } call_battleObject_findClosestEnemyBattleObject_80103F8_result_sub_80D1F0E;
/* register form: generated wrapper: battleObject_findClosestEnemyBattleObject_80103F8 has no typed C interface yet */
static inline call_battleObject_findClosestEnemyBattleObject_80103F8_result_sub_80D1F0E call_battleObject_findClosestEnemyBattleObject_80103F8_sub_80D1F0E(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battleObject_findClosestEnemyBattleObject_80103F8);
    return (call_battleObject_findClosestEnemyBattleObject_80103F8_result_sub_80D1F0E){ R0, R4, R5, R6, R7 };
}

/* battleObject_advanceEnemyTargetList_80D1F0E: Advance the extra-var enemy list for a battle object, or
 * find the closest enemy when the list is exhausted. Returns two values
 * (r0, r1) read from the target object. */

struct EnemyInfo {
    u32 r0; /* byte at +0x12, or lower nibble of word at +0x78 */
    u32 r1; /* byte at +0x13, or word >> 4 at +0x78 */
};

static struct EnemyInfo get_enemy_info(u32 battle_obj) {
    u32 param2 = LD8(battle_obj + oBattleObject_Param2);
    u32 target_obj;
    u32 r2_at_fork;
    bool have_entry = false;

    if (param2 != 0) {
        u32 extra_ptr = LD32(battle_obj + (oBattleObject_ExtraVars + 0xC));
        if (extra_ptr != 0) {
            u32 entry = LD32(extra_ptr);
            if (entry != 0) {
                target_obj = entry;
                r2_at_fork = param2;
                have_entry = true;
            } else if (param2 != 1) {
                u32 counter = LD32(battle_obj + (oBattleObject_ExtraVars + 0x10));
                if (counter == 0) {
                    /* Advance to the next entry in the list */
                    u32 next_ptr = extra_ptr + 4;
                    ST32(battle_obj + (oBattleObject_ExtraVars + 0xC), next_ptr);
                    u32 new_entry = LD32(next_ptr);
                    if (new_entry != 0) {
                        ST32(battle_obj + (oBattleObject_ExtraVars + 0x10), 1);
                        target_obj = new_entry;
                        r2_at_fork = param2;
                        have_entry = true;
                    } else {
                        ST32(battle_obj + (oBattleObject_ExtraVars + 0xC), 0);
                    }
                } else {
                    ST32(battle_obj + (oBattleObject_ExtraVars + 0xC), 0);
                }
            } else {
                ST32(battle_obj + (oBattleObject_ExtraVars + 0xC), 0);
            }
        }
    }

    if (!have_entry) {
        /* No valid list entry: find the closest enemy battle object */
        u32 enemy_alliance = LD8(battle_obj + oBattleObject_Alliance) ^ 1;
        target_obj = call_battleObject_findClosestEnemyBattleObject_80103F8_sub_80D1F0E(
            enemy_alliance, battle_obj).r0;
        r2_at_fork = 0;
    }

    if (r2_at_fork == 2 &&
        LD32(battle_obj + (oBattleObject_ExtraVars + 0x10)) == 0) {
        /* Param2 is 2 with no counter set: extract nibbles from word at +0x78 */
        u32 word = LD32(target_obj + 0x78);
        return (struct EnemyInfo){ .r0 = word & 0xF, .r1 = word >> 4 };
    }

    /* Default: return bytes at +0x12 and +0x13 */
    return (struct EnemyInfo){ .r0 = LD8(target_obj + 0x12),
                               .r1 = LD8(target_obj + 0x13) };
}

/* battleObject_advanceEnemyTargetList_80D1F0E(r5 = battle object) -> r0, r1 (r5 preserved) */
Next battleObject_advanceEnemyTargetList_80D1F0E(void) {
    struct EnemyInfo info = get_enemy_info(R5);
    R0 = info.r0;
    R1 = info.r1;
    RETURN_TO_CALLER();
}

/* ── object_stepRedirectTowardTargetPanel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; bool z, n, v; } call_sub_80D200C_result;
/* register form: generated wrapper: sub_80D200C has no typed C interface yet */
static inline call_sub_80D200C_result call_sub_80D200C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80D200C);
    return (call_sub_80D200C_result){ R1, ZERO_FLAG(), NEGATIVE_FLAG(), OVERFLOW_FLAG() };
}

/* Redirect an object's step toward the panel it is heading for.
 *
 * step (r0)        movement the object still has to make this frame
 * callerY (r1)     row handed down by the caller; unused here, the row that
 *                  is actually aimed at comes back from sub_80D200C
 * obj (r5)         battle object whose velocity fields are updated
 *
 * Behaviour:
 *  - While the object moves left (XVelocity < 0) both the step and the
 *    panel X offset are mirrored so the distance check works either way.
 *  - If the step still reaches past the target column, keep walking
 *    unchanged.
 *  - Otherwise ask sub_80D200C for the row to aim at; if its signed
 *    comparison reports "greater or equal" (N == V), stop here.
 *  - Already on the target row: simply reverse the X velocity.
 *  - Otherwise: throw the movement into Y toward the target row (sign
 *    from the row comparison, magnitude from ExtraVars+4) and halt the
 *    X movement.
 */
static void step_redirect_toward_target_panel(u32 step, u32 callerY, u32 obj) {
    (void)callerY;

    s32 panel_x = (s32)LD8(obj + oBattleObject_PanelX);
    s32 xvel    = (s32)LD32(obj + oBattleObject_XVelocity);

    if (xvel < 0) {
        step    = 0u - step;
        panel_x = -panel_x;
    }

    /* Still short of the target column: keep the current velocity. */
    if ((s32)step > panel_x)
        return;

    call_sub_80D200C_result aim = call_sub_80D200C(obj);
    if (aim.n == aim.v)             /* bge: sub_80D200C's comparison says >= */
        return;

    s32 target_y = (s32)aim.r1;
    s32 panel_y  = (s32)LD8(obj + oBattleObject_PanelY);

    if (target_y == panel_y) {
        /* Already on the target row: turn around in place. */
        ST32(obj + oBattleObject_XVelocity,
             0u - LD32(obj + oBattleObject_XVelocity));
        return;
    }

    s32 y_dir;
    if (target_y > panel_y)
        y_dir = 1;
    else
        y_dir = -1;

    ST32(obj + oBattleObject_YVelocity,
         LD32(obj + oBattleObject_ExtraVars + 4) * (u32)y_dir);
    ST32(obj + oBattleObject_XVelocity, 0);
}

/* inputs: r0 r1 r5 lr; outputs: none; r1 preserved (never written here) */
Next object_stepRedirectTowardTargetPanel(void) {
    step_redirect_toward_target_panel(R0, R1, R5);
    RETURN_TO_CALLER();
}

/* ── t3_0x6e_80D2290 ── */
/* t3_0x6e_80D2290 -- per-state dispatcher for a battle object.
 *
 * Reads the object's CurState byte and jumps to the matching handler:
 *   0 -> battleObject_initObstacle_80D22B0
 *   1 -> battleObject_runUpdateActionsAndCollisions_80D2320
 *   2 -> object_genericDestroy
 * The handler is entered with r0 = CurState and every other argument
 * register (r2, r4, r5 = object, r8-r12) passed through untouched; it
 * runs as a plain call (lr = return address here), so any values it
 * leaves in r5/r8-r12 become this function's outputs.
 */

/* register form: indirect call through a jump-table entry; the callee is
 * entered mid-function with the guest's live r2/r4/r5/r8-r12 argument
 * registers, which no typed signature can describe. */
static void run_object_state_handler_t3_0x6e_80D2290(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D22A4 + state);
    R0 = state;      /* handlers expect the current state in r0 */
    R1 = handler;    /* the original leaves the table entry in r1 */
    GUEST_CALL_ADDR(handler);
}

Next t3_0x6e_80D2290(void) {
    run_object_state_handler_t3_0x6e_80D2290(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_initObstacle_80D22B0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_battleObject_initObstacle_80D22B0;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battleObject_initObstacle_80D22B0 call_sprite_load_battleObject_initObstacle_80D22B0(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battleObject_initObstacle_80D22B0){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_initObstacle_80D22B0(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r4, r5; } call_sprite_loadAnimationData_result_battleObject_initObstacle_80D22B0;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_initObstacle_80D22B0 call_sprite_loadAnimationData_battleObject_initObstacle_80D22B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_initObstacle_80D22B0){ R4, R5 };
}

typedef struct { u32 r2, r4, r5; } call_sprite_update_result_battleObject_initObstacle_80D22B0;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_battleObject_initObstacle_80D22B0 call_sprite_update_battleObject_initObstacle_80D22B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_battleObject_initObstacle_80D22B0){ R2, R4, R5 };
}

typedef struct { u32 r3; } call_sprite_hasShadow_result_battleObject_initObstacle_80D22B0;
/* register form: generated wrapper: sprite_hasShadow has no typed C interface yet */
static inline call_sprite_hasShadow_result_battleObject_initObstacle_80D22B0 call_sprite_hasShadow_battleObject_initObstacle_80D22B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_hasShadow);
    return (call_sprite_hasShadow_result_battleObject_initObstacle_80D22B0){ R3 };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battleObject_initObstacle_80D22B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r0, r4; } call_object_createCollisionData_result_battleObject_initObstacle_80D22B0;
/* register form: generated wrapper: object_createCollisionData has no typed C interface yet */
static inline call_object_createCollisionData_result_battleObject_initObstacle_80D22B0 call_object_createCollisionData_battleObject_initObstacle_80D22B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_createCollisionData);
    return (call_object_createCollisionData_result_battleObject_initObstacle_80D22B0){ R0, R4 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result_battleObject_initObstacle_80D22B0;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result_battleObject_initObstacle_80D22B0 call_object_freeMemory_battleObject_initObstacle_80D22B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result_battleObject_initObstacle_80D22B0){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r4, r6, r7; } call_object_setupCollisionData_result_battleObject_initObstacle_80D22B0;
/* register form: generated wrapper: object_setupCollisionData has no typed C interface yet */
static inline call_object_setupCollisionData_result_battleObject_initObstacle_80D22B0 call_object_setupCollisionData_battleObject_initObstacle_80D22B0(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    GUEST_CALL(object_setupCollisionData);
    return (call_object_setupCollisionData_result_battleObject_initObstacle_80D22B0){ R1, R4, R6, R7 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8; bool c, v; } call_object_presentCollisionData_result_battleObject_initObstacle_80D22B0;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battleObject_initObstacle_80D22B0 call_object_presentCollisionData_battleObject_initObstacle_80D22B0(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battleObject_initObstacle_80D22B0){ R1, R2, R3, R4, R5, R6, R7, R8, CARRY_FLAG(), OVERFLOW_FLAG() };
}

struct ObstacleInitResult {
    u32 r5;
    u32 r8;
};

/* Initialize a map obstacle: load its sprite, set stats (500 HP, name 0xCE),
 * create and present collision data.  Returns updated r5 (object) and r8 (sprite list). */
static struct ObstacleInitResult battleObject_initObstacle(u32 battleObj, u32 spriteList) {
    /* Load obstacle sprite tilemap */
    call_sprite_load_battleObject_initObstacle_80D22B0(0x80, 0x10, 8, battleObj);

    /* Reset animation to frame 0 and initialize display */
    ST16(battleObj + oBattleObject_CurAnimAndCurAnimCopy, 0);
    call_sprite_setAnimation_battleObject_initObstacle_80D22B0(0, battleObj);
    call_sprite_loadAnimationData_battleObject_initObstacle_80D22B0(battleObj);
    call_sprite_update_battleObject_initObstacle_80D22B0(battleObj);
    call_sprite_hasShadow_battleObject_initObstacle_80D22B0(battleObj);
    call_object_setCoordinatesFromPanels_battleObject_initObstacle_80D22B0(battleObj);

    /* Center on depth axis and lock to current panel */
    ST32(battleObj + oBattleObject_Z, 0);
    ST16(battleObj + oBattleObject_FuturePanelXY,
         LD16(battleObj + oBattleObject_PanelXY));

    /* Nudge rendering origin one pixel on Z and Y (tile-edge alignment) */
    ST16(battleObj + oBattleObject_Z16, LD16(battleObj + oBattleObject_Z16) - 1);
    ST16(battleObj + oBattleObject_Y16, LD16(battleObj + oBattleObject_Y16) - 1);

    /* Obstacle identity: name 206, HP 500, idle timer 6000 frames */
    ST16(battleObj + oBattleObject_NameID, 0xCE);
    ST16(battleObj + oBattleObject_HP, 500);
    ST16(battleObj + oBattleObject_MaxHP, 500);
    ST16(battleObj + oBattleObject_Timer, 0x1770);

    /* Allocate collision hitbox */
    call_object_createCollisionData_result_battleObject_initObstacle_80D22B0 cd = call_object_createCollisionData_battleObject_initObstacle_80D22B0(battleObj);
    if (cd.r0 == 0) {
        /* Allocation failed – release the object */
        call_object_freeMemory_result_battleObject_initObstacle_80D22B0 free = call_object_freeMemory_battleObject_initObstacle_80D22B0(battleObj);
        return (struct ObstacleInitResult){ free.r5, spriteList };
    }

    /* Set hitbox geometry: 14 × 15 pixels, solid type */
    call_object_setupCollisionData_result_battleObject_initObstacle_80D22B0 setup =
        call_object_setupCollisionData_battleObject_initObstacle_80D22B0(cd.r0, 0xE, 0xF, 3, battleObj);

    /* Register the obstacle's collision with the scene */
    call_object_presentCollisionData_result_battleObject_initObstacle_80D22B0 present =
        call_object_presentCollisionData_battleObject_initObstacle_80D22B0(setup.r1, battleObj, spriteList);

    /* Obstacle transitions to its update state */
    ST8(present.r5 + oBattleObject_CurState, CUR_STATE_UPDATE);

    return (struct ObstacleInitResult){ present.r5, present.r8 };
}

/* battleObject_initObstacle_80D22B0(r5 = battleObj, r8 = spriteList) -> r5, r8 */
Next battleObject_initObstacle_80D22B0(void) {
    struct ObstacleInitResult res = battleObject_initObstacle(R5, R8);
    R5 = res.r5;
    R8 = res.r8;
    RETURN_TO_CALLER();
}

/* ── battleObject_runUpdateActionsAndCollisions_80D2320 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sprite_clearFinalPalette has no typed C interface yet */
static inline void call_sprite_clearFinalPalette(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_clearFinalPalette);
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; bool v; } call_object_applyCollisionHit_801AD9E_result;
/* register form: generated wrapper: object_applyCollisionHit_801AD9E has no typed C interface yet */
static inline call_object_applyCollisionHit_801AD9E_result call_object_applyCollisionHit_801AD9E(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(object_applyCollisionHit_801AD9E);
    return (call_object_applyCollisionHit_801AD9E_result){ R2, R3, R4, R5, R6, R7, R10, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_battleObject_dispatchActionHandler_801B394_result;
/* register form: generated wrapper: battleObject_dispatchActionHandler_801B394 has no typed C interface yet */
static inline call_battleObject_dispatchActionHandler_801B394_result call_battleObject_dispatchActionHandler_801B394(u32 r0, u32 r2, u32 r3, u32 r4, u32 r5, u32 r8, u32 r9, u32 r11, u32 r12) {
    R0 = r0;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R8 = r8;
    R9 = r9;
    R11 = r11;
    R12 = r12;
    GUEST_CALL(battleObject_dispatchActionHandler_801B394);
    return (call_battleObject_dispatchActionHandler_801B394_result){ R1, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r8; bool c, v; } call_object_presentCollisionData_result_battleObject_runUpdateActionsAndCollisions_80D2320;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battleObject_runUpdateActionsAndCollisions_80D2320 call_object_presentCollisionData_battleObject_runUpdateActionsAndCollisions_80D2320(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battleObject_runUpdateActionsAndCollisions_80D2320){ R2, R3, R4, R5, R6, R7, R8, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpriteTimestop_result_battleObject_runUpdateActionsAndCollisions_80D2320;
/* register form: generated wrapper: object_updateSpriteTimestop has no typed C interface yet */
static inline call_object_updateSpriteTimestop_result_battleObject_runUpdateActionsAndCollisions_80D2320 call_object_updateSpriteTimestop_battleObject_runUpdateActionsAndCollisions_80D2320(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpriteTimestop);
    return (call_object_updateSpriteTimestop_result_battleObject_runUpdateActionsAndCollisions_80D2320){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* One battle object's per-frame update: mark the object header as updated,
 * release its final palette entry, resolve hits against it, run its current
 * action state handler, publish its collision box, then animate its sprite
 * under the time-stop rules.
 *
 * Register notes (from the lifter's analysis of the original):
 *  - r2 is dead on entry: object_applyCollisionHit overwrites it before the
 *    action dispatcher (the only reader of r2 here) is called.
 *  - r10 is an input to the callees but every one of them leaves it unchanged,
 *    and the generated wrappers keep it as on entry automatically.
 *  - r5/r8/r9/r11/r12 on exit are whatever battleObject_dispatchActionHandler
 *    left behind; the wrapper calls leave those guest registers in place, so
 *    nothing needs to be passed back explicitly.
 *  - r6/r7 are preserved: no callee here touches them. */
static void runUpdateActionsAndCollisions(u32 r4, u32 r5, u32 r8, u32 r9,
                                          u32 r11, u32 r12) {
    /* Set bit 1 of the object header flags: this object has been updated. */
    ST8(r5 + oObjectHeader_Flags, LD8(r5 + oObjectHeader_Flags) | 2);

    /* Give back the object's final palette slot for this frame. */
    call_sprite_clearFinalPalette(r5);

    /* Resolve incoming hits/collisions against this object. */
    call_object_applyCollisionHit_801AD9E_result hit =
        call_object_applyCollisionHit_801AD9E(r4, r5);

    /* Run the handler for the object's current action state, selected through
     * the state handler jump table at jt_80D2348. */
    call_battleObject_dispatchActionHandler_801B394_result action =
        call_battleObject_dispatchActionHandler_801B394(
            A_jt_80D2348, hit.r2, hit.r3, hit.r4, hit.r5, r8, r9, r11, r12);

    /* Publish this object's collision data for the frame... */
    call_object_presentCollisionData_battleObject_runUpdateActionsAndCollisions_80D2320(action.r1, action.r5, action.r8);

    /* ...and update its sprite, frozen while time stop is active. */
    call_object_updateSpriteTimestop_battleObject_runUpdateActionsAndCollisions_80D2320(action.r5);
}

/* battleObject_runUpdateActionsAndCollisions_80D2320
 *   (r2 r4 r5 r8 r9 r10 r11 r12 lr -> r5 r8 r9 r10 r11 r12) */
Next battleObject_runUpdateActionsAndCollisions_80D2320(void) {
    runUpdateActionsAndCollisions(R4, R5, R8, R9, R11, R12);
    RETURN_TO_CALLER();
}

/* ── battleObject_clearActiveOnDeletionState_80D236C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battle_getFlags has no typed C interface yet */
static inline u32 call_battle_getFlags_battleObject_clearActiveOnDeletionState_80D236C(void) {
    GUEST_CALL(battle_getFlags);
    return R0;
}

/* register form: generated wrapper: nullsub_10 has no typed C interface yet */
static inline void call_nullsub_10(void) {
    GUEST_CALL(nullsub_10);
}

/* Battle object deletion: if the battle is active (flag bit 0 set), clear the object's
 * "active" bit, set the "inactive" bit, force CurAction = 8, and reset the current phase.
 * If the battle is not active, run the null notifier and leave everything alone.
 * Returns the value the original leaves in r1: 2 on the clearing path, 1 otherwise. */
static u32 clear_active_on_deletion(u32 object) {
    u32 flags = call_battle_getFlags_battleObject_clearActiveOnDeletionState_80D236C();

    if (!(flags & 1)) {
        call_nullsub_10();
        return 1;
    }

    u8 headerFlags = LD8(object + oObjectHeader_Flags);
    ST8(object + oObjectHeader_Flags, headerFlags & ~4u);

    headerFlags = LD8(object + oObjectHeader_Flags);
    ST8(object + oObjectHeader_Flags, headerFlags | 2u);

    ST8(object + oBattleObject_CurAction, 8);
    ST16(object + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    return 2;
}

/* battleObject_clearActiveOnDeletionState_80D236C: r5=object, r10=battle ctx → r1 */
Next battleObject_clearActiveOnDeletionState_80D236C(void) {
    R1 = clear_active_on_deletion(R5);
    RETURN_TO_CALLER();
}

/* ── stub_noOp_80D2398 ── */
/* stub_noOp_80D2398 is an empty stub: its whole body is "push {lr}; pop {pc}",
 * which simply returns to the caller without touching any state. */

static void sub_80D2398_body(u32 return_address) {
    /* Nothing to do; lr flows through unchanged to the return. */
    (void)return_address;
}

Next stub_noOp_80D2398(void) {
    sub_80D2398_body(LR);
    RETURN_TO_CALLER();
}

/* ── t3_0x6f_80D2460 ── */
/* t3_0x6f_80D2460 -- battle-object state dispatcher.
 *
 * Reads the object's current state byte (object pointer arrives in r5) and
 * calls the matching handler through a small jump table:
 *   state 0 -> battleObject_initAppearOrEntryFromSide_80D2480
 *   state 4 -> battle_runObjectAction_80D24F0
 *   state 8 -> object_genericDestroy
 * The table is indexed with the raw byte value (the original's
 * `ldr r1, [r1,r0]` uses it as an unscaled byte offset), so valid states are
 * spaced 4 apart. The handler receives the object in r5 (and the state byte
 * in r0) and leaves its results for the caller in r5 and r8-r12.
 */

/* register form: dispatch through the state-handler jump table. The callee
 * must find the object pointer in r5 and leaves its results directly in
 * r5/r8-r12 for the caller, which no typed signature can express. */
static u32 call_state_handler_t3_0x6f_80D2460(u32 handler, u32 state, u32 obj) {
    R0 = state;             /* state byte, as the original passes it */
    R1 = handler;           /* table entry still live in r1 at the branch */
    R5 = obj;               /* object pointer the handlers operate on */
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Look up the handler for the object's current state and run it. */
static void dispatch_object_state(u32 obj) {
    u32 state = LD8(obj + oBattleObject_CurState);
    u32 handler = LD32(A_jt_80D2474 + state);
    call_state_handler_t3_0x6f_80D2460(handler, state, obj);
}

/* t3_0x6f_80D2460(r5 = object, lr = return address)
 * -> r5 r8 r9 r10 r11 r12 left by the chosen handler */
Next t3_0x6f_80D2460(void) {
    dispatch_object_state(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_initAppearOrEntryFromSide_80D2480 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battleObject_initAppearOrEntryFromSide_80D2480 call_sprite_load_battleObject_initAppearOrEntryFromSide_80D2480(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battleObject_initAppearOrEntryFromSide_80D2480){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_initAppearOrEntryFromSide_80D2480(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_initAppearOrEntryFromSide_80D2480 call_sprite_loadAnimationData_battleObject_initAppearOrEntryFromSide_80D2480(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_initAppearOrEntryFromSide_80D2480){ R2, R4, R5 };
}

typedef struct { u32 r3; bool c, v; } call_sprite_hasShadow_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: sprite_hasShadow has no typed C interface yet */
static inline call_sprite_hasShadow_result_battleObject_initAppearOrEntryFromSide_80D2480 call_sprite_hasShadow_battleObject_initAppearOrEntryFromSide_80D2480(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_hasShadow);
    return (call_sprite_hasShadow_result_battleObject_initAppearOrEntryFromSide_80D2480){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battleObject_initAppearOrEntryFromSide_80D2480 call_object_getFlip_battleObject_initAppearOrEntryFromSide_80D2480(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battleObject_initAppearOrEntryFromSide_80D2480){ R0 };
}

typedef struct { u32 r3; } call_sprite_setFlip_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_battleObject_initAppearOrEntryFromSide_80D2480 call_sprite_setFlip_battleObject_initAppearOrEntryFromSide_80D2480(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_battleObject_initAppearOrEntryFromSide_80D2480){ R3 };
}

typedef struct { u32 r2; } call_object_setCoordinatesFromPanels_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline call_object_setCoordinatesFromPanels_result_battleObject_initAppearOrEntryFromSide_80D2480 call_object_setCoordinatesFromPanels_battleObject_initAppearOrEntryFromSide_80D2480(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
    return (call_object_setCoordinatesFromPanels_result_battleObject_initAppearOrEntryFromSide_80D2480){ R2 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battleObject_initAppearOrEntryFromSide_80D2480 call_object_getFrontDirection_battleObject_initAppearOrEntryFromSide_80D2480(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battleObject_initAppearOrEntryFromSide_80D2480){ R0 };
}

typedef struct { u32 r1, r2, r5, r6; } call_object_setPanelsFromCoordinates_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: object_setPanelsFromCoordinates has no typed C interface yet */
static inline call_object_setPanelsFromCoordinates_result_battleObject_initAppearOrEntryFromSide_80D2480 call_object_setPanelsFromCoordinates_battleObject_initAppearOrEntryFromSide_80D2480(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setPanelsFromCoordinates);
    return (call_object_setPanelsFromCoordinates_result_battleObject_initAppearOrEntryFromSide_80D2480){ R1, R2, R5, R6 };
}

typedef struct { u32 r3; } call_sprite_setPalette_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battleObject_initAppearOrEntryFromSide_80D2480 call_sprite_setPalette_battleObject_initAppearOrEntryFromSide_80D2480(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battleObject_initAppearOrEntryFromSide_80D2480){ R3 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_battleObject_initAppearOrEntryFromSide_80D2480;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battleObject_initAppearOrEntryFromSide_80D2480 call_PlaySoundEffect_battleObject_initAppearOrEntryFromSide_80D2480(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battleObject_initAppearOrEntryFromSide_80D2480){ R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_sub_80D24F0_result;
/* register form: generated wrapper: battle_runObjectAction_80D24F0 has no typed C interface yet */
static inline call_sub_80D24F0_result call_sub_80D24F0(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_runObjectAction_80D24F0);
    return (call_sub_80D24F0_result){ R5, R8, R9, R10, R11, R12 };
}

static call_sub_80D24F0_result init_appear_or_entry_from_side(u32 r5) {
    /* Mark object as appearing / entering from side */
    ST8(r5 + oObjectHeader_Flags, LD8(r5 + oObjectHeader_Flags) | 2);

    /* Load sprite appearance sheet (width 0x80, height 0xC, palette bank 0x44) */
    r5 = call_sprite_load_battleObject_initAppearOrEntryFromSide_80D2480(0x80, 0xC, 0x44, r5).r5;

    /* Start at animation frame 0 */
    ST16(r5 + oBattleObject_CurAnimAndCurAnimCopy, 0);
    call_sprite_setAnimation_battleObject_initAppearOrEntryFromSide_80D2480(0, r5);

    /* Upload animation data */
    r5 = call_sprite_loadAnimationData_battleObject_initAppearOrEntryFromSide_80D2480(r5).r5;

    /* Query shadow attribute (result unused) */
    call_sprite_hasShadow_battleObject_initAppearOrEntryFromSide_80D2480(r5);

    /* Mirror the object horizontally for the side-entry pose */
    u32 flip = call_object_getFlip_battleObject_initAppearOrEntryFromSide_80D2480(r5).r0;
    call_sprite_setFlip_battleObject_initAppearOrEntryFromSide_80D2480(flip ^ 1, r5);

    /* Convert grid panel positions to pixel coordinates */
    call_object_setCoordinatesFromPanels_battleObject_initAppearOrEntryFromSide_80D2480(r5);

    /* Shift X by 64 pixels per front-direction step (entry offset) */
    s32 dir = (s32)call_object_getFrontDirection_battleObject_initAppearOrEntryFromSide_80D2480(r5).r0;
    ST16(r5 + oBattleObject_X16,
         (u16)(LD16(r5 + oBattleObject_X16) + (u32)(0x40 * dir)));

    /* Initial horizontal velocity: opposite of facing, in 16.16 sub-pixel units */
    ST32(r5 + oBattleObject_XVelocity, (s32)(-dir) * 0x80000);

    /* Recompute panel grid from the adjusted coordinates */
    call_object_setPanelsFromCoordinates_result_battleObject_initAppearOrEntryFromSide_80D2480 panels = call_object_setPanelsFromCoordinates_battleObject_initAppearOrEntryFromSide_80D2480(r5);
    r5 = panels.r5;

    /* Appearance animation lasts 8 frames */
    ST16(r5 + oBattleObject_Timer, 8);

    /* Palette slot 1 when Param1 is non-zero (alternate palette), else slot 0 */
    u32 palette = LD8(r5 + oBattleObject_Param1) ? 1 : 0;
    call_sprite_setPalette_battleObject_initAppearOrEntryFromSide_80D2480(palette, r5);

    /* Play the appearance sound effect */
    r5 = call_PlaySoundEffect_battleObject_initAppearOrEntryFromSide_80D2480(0xB0, panels.r2).r5;

    /* Transition to the update phase */
    ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    /* Hand off to the shared state-transition routine */
    return call_sub_80D24F0(r5);
}

/* battleObject_initAppearOrEntryFromSide_80D2480(r5) -> r5 r8 r9 r10 r11 r12 */
Next battleObject_initAppearOrEntryFromSide_80D2480(void) {
    call_sub_80D24F0_result out = init_appear_or_entry_from_side(R5);
    R5  = out.r5;
    R8  = out.r8;
    R9  = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── battle_runObjectAction_80D24F0 ── */
/* battle_runObjectAction_80D24F0 — mark an object as mid-action and dispatch to its handler.
 *
 * The jump table at off_80D250C is indexed by the raw CurAction byte,
 * exactly as the original's `ldr r1, [r1, r0]` does: the entries are 4
 * bytes apart and the action values step by 4, so action 0 selects
 * battleObj_stepAdvance_80D2514 and action 4 selects object_updateTimerState.
 *
 * Handlers are entered through the table with the original's
 * `mov lr, pc; bx rN` sequence, so they are reached with GUEST_CALL_ADDR.
 */
#define ACTION_HANDLER_TABLE A_off_80D250C

/* Header flag bit 1: set while the object's action handler is running. */
#define OBJECT_FLAG_ACTION_BUSY 0x02

/* Call the CurAction handler.
 * Register form: the handler receives CurAction in r0, its own address in
 * r1 and the object pointer in r5, and produces its results in r5 and
 * r8-r12 — a live-register contract that typed parameters cannot express,
 * so the call runs on the guest register file and its register effects
 * persist for the caller. */
static void call_action_handler(u32 handler, u32 cur_action, u32 obj) {
    R0 = cur_action;
    R1 = handler;
    R5 = obj;
    GUEST_CALL_ADDR(handler);
}

/* Set the "action busy" flag in the object header, then run the handler
 * selected by the object's current action. The handler does the rest of
 * the work for this step and leaves the object pointer and its results in
 * r5/r8-r12, exactly as the original's tail-call does. */
static void run_current_action(u32 obj) {
    u32 flags = LD8(obj + oObjectHeader_Flags);
    ST8(obj + oObjectHeader_Flags, flags | OBJECT_FLAG_ACTION_BUSY);

    u32 cur_action = LD8(obj + oBattleObject_CurAction);
    /* Byte offset, not word index: the original indexes the table with
     * the CurAction byte itself. */
    u32 handler = LD32(ACTION_HANDLER_TABLE + cur_action);
    call_action_handler(handler, cur_action, obj);
}

/* battle_runObjectAction_80D24F0(r5 = object) -> r5 r8-r12 (set by the action handler) */
Next battle_runObjectAction_80D24F0(void) {
    run_current_action(R5);
    RETURN_TO_CALLER();
}

/* ── battleObj_stepAdvance_80D2514 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5, r6; } call_convertXYToPanelXY_800E258_result;
/* register form: generated wrapper: convertXYToPanelXY_800E258 has no typed C interface yet */
static inline call_convertXYToPanelXY_800E258_result call_convertXYToPanelXY_800E258(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(convertXYToPanelXY_800E258);
    return (call_convertXYToPanelXY_800E258_result){ R0, R1, R2, R3, R5, R6 };
}

typedef struct { u32 r1, r2, r3, r5; } call_object_spawnCollisionRegion_result_sub_80D2514;
/* register form: generated wrapper: object_spawnCollisionRegion has no typed C interface yet */
static inline call_object_spawnCollisionRegion_result_sub_80D2514 call_object_spawnCollisionRegion_sub_80D2514(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_spawnCollisionRegion);
    return (call_object_spawnCollisionRegion_result_sub_80D2514){ R1, R2, R3, R5 };
}

/* Advance a battle object one tick: apply velocity, convert to panel coords,
 * optionally spawn a collision region when the timer reaches 2, then decrement
 * the timer and restart the action cycle when it expires. */
static void battle_object_advance(u32 obj) {
    /* Move X by its velocity */
    u32 new_x = LD32(obj + oBattleObject_X) + LD32(obj + oBattleObject_XVelocity);
    ST32(obj + oBattleObject_X, new_x);

    /* Convert world position to panel-grid coordinates */
    u32 world_y = LD32(obj + oBattleObject_Y);
    call_convertXYToPanelXY_800E258_result panel =
        call_convertXYToPanelXY_800E258(new_x, world_y);
    ST8(obj + oBattleObject_PanelX, panel.r0);
    ST8(obj + oBattleObject_PanelY, panel.r1);

    /* Spawn a collision region on the tick just before the action resets */
    if (LD16(obj + oBattleObject_Timer) == 2) {
        /* register form: the wrapper sets R4/R6/R7 to pass arguments and does
         * not restore them; the original saves them with push {r4,r6,r7}. */
        u32 saved_r4 = R4;
        u32 saved_r6 = R6;
        u32 saved_r7 = R7;

        call_object_spawnCollisionRegion_sub_80D2514(
            LD8(obj + oBattleObject_PanelX),   /* r0: panel X */
            LD8(obj + oBattleObject_PanelY),   /* r1: panel Y */
            LD8(obj + oBattleObject_Element),  /* r2: element type */
            LD32(obj + oBattleObject_Z),       /* r3: Z depth */
            0x4040401,                         /* r4: hit mask parameters */
            obj,                               /* r5: battle object */
            LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler), /* r6 */
            0x51                               /* r7: action/event ID */
        );

        R4 = saved_r4;
        R6 = saved_r6;
        R7 = saved_r7;
    }

    /* Decrement the action timer */
    u16 timer = (u16)(LD16(obj + oBattleObject_Timer) - 1);
    ST16(obj + oBattleObject_Timer, timer);

    /* Timer expired – restart the action cycle */
    if (timer == 0) {
        ST16(obj + oBattleObject_Timer, 0x14);
        ST8(obj + oBattleObject_CurAction, 4);
        ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    }
}

/* battleObj_stepAdvance_80D2514(r5 = battle object pointer) -> r5 (unchanged) */
Next battleObj_stepAdvance_80D2514(void) {
    battle_object_advance(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnType6FAndInitObject_80D2580 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; } call_object_spawnType3_result_sub_80D2580;
/* register form: generated wrapper: object_spawnType3 has no typed C interface yet */
static inline call_object_spawnType3_result_sub_80D2580 call_object_spawnType3_sub_80D2580(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType3);
    return (call_object_spawnType3_result_sub_80D2580){ R4, R5, R6, R7 };
}

typedef struct { u32 r1; } call_BasicBattleObjectInitialize_result_sub_80D2580;
/* register form: generated wrapper: BasicBattleObjectInitialize has no typed C interface yet */
static inline call_BasicBattleObjectInitialize_result_sub_80D2580 call_BasicBattleObjectInitialize_sub_80D2580(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(BasicBattleObjectInitialize);
    return (call_BasicBattleObjectInitialize_result_sub_80D2580){ R1 };
}

/* Spawn object type 0x6F (the caller's r1-r4 are passed through as its
 * arguments), then — unless the spawn failed (null object pointer in r5) —
 * run BasicBattleObjectInitialize on the new object, with the original
 * r0-r2/r5/r6 as its arguments.
 *
 * Registers the original leaves behind:
 *   r1: BasicBattleObjectInitialize's result, or the original r0 if the
 *       spawn failed (the original pops its saved r0 into r1 on that path),
 *   r4, r6: passed back from object_spawnType3,
 *   r5: the original r5 (the pop restores it before the branch).
 * r7 ends as it entered; the caller's r6 itself is never read. */
typedef struct {
    u32 r1;
    u32 r4;
    u32 r5;
    u32 r6;
} SpawnType3InitResult;

static SpawnType3InitResult spawn_type3_and_initialize(u32 a0, u32 a1, u32 a2, u32 a3,
                                                       u32 a4, u32 a5, u32 a6)
{
    call_object_spawnType3_result_sub_80D2580 spawn = call_object_spawnType3_sub_80D2580(0x6F, a1, a2, a3, a4);

    u32 out_r1;
    if (spawn.r5 == 0) {
        /* Spawn failed: skip initialization; r1 ends up holding the saved r0. */
        out_r1 = a0;
    } else {
        out_r1 = call_BasicBattleObjectInitialize_sub_80D2580(spawn.r5, a0, a1, a2, a5, spawn.r6).r1;
    }

    SpawnType3InitResult out;
    out.r1 = out_r1;
    out.r4 = spawn.r4;
    out.r5 = a5;
    out.r6 = spawn.r6;
    return out;
}

/* battle_spawnType6FAndInitObject_80D2580(r0-r6 = arguments) -> r1 r4 r5 r6 */
Next battle_spawnType6FAndInitObject_80D2580(void) {
    SpawnType3InitResult out = spawn_type3_and_initialize(R0, R1, R2, R3, R4, R5, R6);
    R1 = out.r1;
    R4 = out.r4;
    R5 = out.r5;
    R6 = out.r6;
    RETURN_TO_CALLER();
}

/* ── battle_spawnMirrorTriplet_80D2596 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r4, r5, r6; } call_sub_80D2580_result;
/* register form: generated wrapper: battle_spawnType6FAndInitObject_80D2580 has no typed C interface yet */
static inline call_sub_80D2580_result call_sub_80D2580(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_spawnType6FAndInitObject_80D2580);
    return (call_sub_80D2580_result){ R1, R4, R5, R6 };
}

/* Compute the slot base from a battle object's alliance XOR direction-flip,
 * negated, scaled by 5, offset by 6 (matching the original's arithmetic). */
static u32 mirror_slot_base(u32 obj) {
    u32 alliance = LD8(obj + oBattleObject_Alliance);
    u32 flip     = LD8(obj + oBattleObject_DirectionFlip);
    return (0u - (alliance ^ flip)) * 5 + 6;
}

typedef struct {
    u32 r1;
    u32 r5;
    u32 r6;
} sub_80D2596_result;

/* Run the shared slot helper (battle_spawnType6FAndInitObject_80D2580) for slots 1, 2, 3.
 * Each step chains r4/r5/r6 forward while r0 (base), r2, r3 stay constant.
 * The last step's r1/r5/r6 are this function's outputs. */
static sub_80D2596_result process_battle_object_slots(u32 obj, u32 r2, u32 r3,
                                                      u32 r4, u32 r5, u32 r6) {
    u32 base = mirror_slot_base(obj);

    call_sub_80D2580_result s1 = call_sub_80D2580(base, 1, r2, r3, r4, r5, r6);
    call_sub_80D2580_result s2 = call_sub_80D2580(base, 2, r2, r3, s1.r4, s1.r5, s1.r6);
    call_sub_80D2580_result s3 = call_sub_80D2580(base, 3, r2, r3, s2.r4, s2.r5, s2.r6);

    return (sub_80D2596_result){ .r1 = s3.r1, .r5 = s3.r5, .r6 = s3.r6 };
}

/* battle_spawnMirrorTriplet_80D2596(r2, r3, r4, r5=battle_obj, r6) -> r1, r5, r6; preserves r2, r3, r4 */
Next battle_spawnMirrorTriplet_80D2596(void) {
    /* register form: the three call_sub_80D2580 wrappers write R2/R3 as arguments
     * and battle_spawnType6FAndInitObject_80D2580 may clobber them; the original uses push/pop {r0,r2,r3}
     * around each call to guarantee R2/R3 survive the sequence. */
    u32 save_r2 = R2, save_r3 = R3;
    sub_80D2596_result res = process_battle_object_slots(R5, save_r2, save_r3, R4, R5, R6);
    R1 = res.r1;
    R5 = res.r5;
    R6 = res.r6;
    R2 = save_r2;
    R3 = save_r3;
    RETURN_TO_CALLER();
}

/* ── t3_0x71_80D2A94 ── */
/* Type-3 battle object state dispatcher.
 *
 * r5 points to the battle object; its CurState byte indexes a small table of
 * per-state update routines at off_80D2AA8 (init / pulsing-attacker update /
 * generic destroy). The chosen handler is tail-called with r0 = CurState and
 * r1 = its own address, so it behaves exactly as if the caller had invoked it
 * directly: r4/r7/r8/r10 reach it untouched, and whatever it leaves in
 * r5/r8-r12 is what this function leaves behind. */

/* register form: the handler address is picked from the object's state table,
 * and the handler's results in r5/r8-r12 must survive in the guest registers,
 * so this tail dispatch cannot be expressed as an ordinary typed C call. */
static void call_state_handler_t3_0x71_80D2A94(u32 handler, u32 state) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

static void run_state_handler_t3_0x71_80D2A94(u32 object) {
    u32 state   = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D2AA8 + state);
    call_state_handler_t3_0x71_80D2A94(handler, state);
}

Next t3_0x71_80D2A94(void) {
    run_state_handler_t3_0x71_80D2A94(R5);
    RETURN_TO_CALLER();
}

/* ── battle_initSpawnObjectCollision ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5, r6, r7; } call_sub_80D2B8E_result;
/* register form: generated wrapper: sub_80D2B8E has no typed C interface yet */
static inline call_sub_80D2B8E_result call_sub_80D2B8E(u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(sub_80D2B8E);
    return (call_sub_80D2B8E_result){ R0, R4, R5, R6, R7 };
}

typedef struct { u32 r0, r4; } call_object_createCollisionData_result_battle_initSpawnObjectCollision;
/* register form: generated wrapper: object_createCollisionData has no typed C interface yet */
static inline call_object_createCollisionData_result_battle_initSpawnObjectCollision call_object_createCollisionData_battle_initSpawnObjectCollision(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_createCollisionData);
    return (call_object_createCollisionData_result_battle_initSpawnObjectCollision){ R0, R4 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result_battle_initSpawnObjectCollision;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result_battle_initSpawnObjectCollision call_object_freeMemory_battle_initSpawnObjectCollision(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result_battle_initSpawnObjectCollision){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r4, r6, r7; } call_object_setupCollisionData_result_battle_initSpawnObjectCollision;
/* register form: generated wrapper: object_setupCollisionData has no typed C interface yet */
static inline call_object_setupCollisionData_result_battle_initSpawnObjectCollision call_object_setupCollisionData_battle_initSpawnObjectCollision(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    GUEST_CALL(object_setupCollisionData);
    return (call_object_setupCollisionData_result_battle_initSpawnObjectCollision){ R2, R4, R6, R7 };
}

typedef struct { u32 r4; } call_battle_syncToPanelInFrontOfRelated_result;
/* register form: generated wrapper: battle_syncToPanelInFrontOfRelated has no typed C interface yet */
static inline call_battle_syncToPanelInFrontOfRelated_result call_battle_syncToPanelInFrontOfRelated(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_syncToPanelInFrontOfRelated);
    return (call_battle_syncToPanelInFrontOfRelated_result){ R4 };
}

typedef struct { u32 r1; } call_object_setCollisionRegion_result_battle_initSpawnObjectCollision;
/* register form: generated wrapper: object_setCollisionRegion has no typed C interface yet */
static inline call_object_setCollisionRegion_result_battle_initSpawnObjectCollision call_object_setCollisionRegion_battle_initSpawnObjectCollision(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionRegion);
    return (call_object_setCollisionRegion_result_battle_initSpawnObjectCollision){ R1 };
}

typedef struct { u32 r3; } call_object_setCollisionHitEffect_result_battle_initSpawnObjectCollision;
/* register form: generated wrapper: object_setCollisionHitEffect has no typed C interface yet */
static inline call_object_setCollisionHitEffect_result_battle_initSpawnObjectCollision call_object_setCollisionHitEffect_battle_initSpawnObjectCollision(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionHitEffect);
    return (call_object_setCollisionHitEffect_result_battle_initSpawnObjectCollision){ R3 };
}

typedef struct { u32 r5, r8; } call_battle_objectUpdatePulsingAttacker_result;
/* register form: generated wrapper: battle_objectUpdatePulsingAttacker has no typed C interface yet */
static inline call_battle_objectUpdatePulsingAttacker_result call_battle_objectUpdatePulsingAttacker(u32 r4, u32 r5, u32 r7, u32 r8) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    R8 = r8;
    GUEST_CALL(battle_objectUpdatePulsingAttacker);
    return (call_battle_objectUpdatePulsingAttacker_result){ R5, R8 };
}

/* battle_initSpawnObjectCollision — set up the collision half of a battle object
 * that has just been spawned (r5 = battle object).
 *
 * r1 is passed through untouched to the companion-object spawner; r8 carries
 * the pulsing-attacker context.  The original pushes/pops r4, r6 and r7, so
 * those must end exactly as they started; r5 and r8 are the outputs and come
 * straight out of the final battle_objectUpdatePulsingAttacker call. */
static void battle_initSpawnObjectCollision_impl(u32 r1, u32 r5, u32 r8)
{
    if (LD8(r5 + oBattleObject_Param1) == 0) {
        /* No companion object yet: spawn one from our parameters... */
        u32 element        = LD8(r5 + oBattleObject_Element);
        u32 z              = LD32(r5 + oBattleObject_Z);
        u32 paramCount     = LD32(r5 + oBattleObject_Params) + 1;
        u32 counterDisable = LD32(r5 + oBattleObject_DamageAndStaminaDamageCounterDisabler);

        call_sub_80D2B8E_result companion = call_sub_80D2B8E(
            r1, element, z, paramCount, r5, counterDisable, r5 + 0x68);

        /* ...and hand our 0x10 header flag bit over to it. */
        u32 flags = (LD8(companion.r0 + oObjectHeader_Flags) & ~0x10u)
                  | (LD8(r5 + oObjectHeader_Flags) & 0x10u);
        ST8(companion.r0 + oObjectHeader_Flags, flags);
    }

    call_object_createCollisionData_result_battle_initSpawnObjectCollision created = call_object_createCollisionData_battle_initSpawnObjectCollision(r5);
    if (created.r0 == 0) {
        /* No collision memory left: clear the extra state and free the object. */
        ST32(r5 + (oBattleObject_ExtraVars + 8), 0);
        call_object_freeMemory_battle_initSpawnObjectCollision(r5);
        return; /* r5/r8 stay as they came in */
    }

    /* Collision shape 0x4A when Param1 is set, else 6; hit behaviour 3 when
     * Param3 == 1, else 0x61; r2 = 5 (element/size selector). */
    u32 shape     = (LD8(r5 + oBattleObject_Param1) != 0) ? 0x4Au : 6u;
    u32 behaviour = (LD8(r5 + oBattleObject_Param3) == 1) ? 3u   : 0x61u;

    call_object_setupCollisionData_result_battle_initSpawnObjectCollision setup =
        call_object_setupCollisionData_battle_initSpawnObjectCollision(created.r0, shape, 5, behaviour, r5);

    /* Timers: Param2 frames until activation, then a 12-frame pulse period. */
    ST16(r5 + oBattleObject_Timer2, LD8(r5 + oBattleObject_Param2));
    ST16(r5 + oBattleObject_Timer, 0xC);

    /* Move onto the panel in front of the related object, then arm the
     * collision region (1) and hit effect (0xA). */
    call_battle_syncToPanelInFrontOfRelated_result panel =
        call_battle_syncToPanelInFrontOfRelated(r5);

    call_object_setCollisionRegion_battle_initSpawnObjectCollision(1, r5);
    call_object_setCollisionHitEffect_battle_initSpawnObjectCollision(0xA, r5);

    ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    /* Switch to the pulsing-attacker state; this call leaves the new r5/r8
     * (and the r4/r7 it was given are the ones the earlier calls left). */
    call_battle_objectUpdatePulsingAttacker(panel.r4, r5, setup.r7, r8);
}

/* battle_initSpawnObjectCollision(r1 = spawner argument, r5 = battle object,
 *                                 r8 = pulsing context) -> r5 r8            */
Next battle_initSpawnObjectCollision(void) {
    /* The original saves/restores r4, r6 and r7 around the body. */
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;
    battle_initSpawnObjectCollision_impl(R1, R5, R8);
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_objectUpdatePulsingAttacker ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_removeCollisionData_result_battle_objectUpdatePulsingAttacker;
/* register form: generated wrapper: object_removeCollisionData has no typed C interface yet */
static inline call_object_removeCollisionData_result_battle_objectUpdatePulsingAttacker call_object_removeCollisionData_battle_objectUpdatePulsingAttacker(u32 r4, u32 r5, u32 r7) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(object_removeCollisionData);
    return (call_object_removeCollisionData_result_battle_objectUpdatePulsingAttacker){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7; bool c, v; } call_object_spawnCollisionEffect_result_battle_objectUpdatePulsingAttacker;
/* register form: generated wrapper: object_spawnCollisionEffect has no typed C interface yet */
static inline call_object_spawnCollisionEffect_result_battle_objectUpdatePulsingAttacker call_object_spawnCollisionEffect_battle_objectUpdatePulsingAttacker(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_spawnCollisionEffect);
    return (call_object_spawnCollisionEffect_result_battle_objectUpdatePulsingAttacker){ R2, R3, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline void call_object_clearCollisionRegion_battle_objectUpdatePulsingAttacker(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
}

typedef struct { u32 r2; } call_camera_initShakeEffect_80302a8_result;
/* register form: generated wrapper: camera_initShakeEffect_80302a8 has no typed C interface yet */
static inline call_camera_initShakeEffect_80302a8_result call_camera_initShakeEffect_80302a8(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(camera_initShakeEffect_80302a8);
    return (call_camera_initShakeEffect_80302a8_result){ R2 };
}

/* register form: generated wrapper: object_setCollisionRegion has no typed C interface yet */
static inline void call_object_setCollisionRegion_battle_objectUpdatePulsingAttacker(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionRegion);
}

typedef struct { u32 r1, r4; } call_battle_syncToPanelInFrontOfRelated_result_battle_objectUpdatePulsingAttacker;
/* register form: generated wrapper: battle_syncToPanelInFrontOfRelated has no typed C interface yet */
static inline call_battle_syncToPanelInFrontOfRelated_result_battle_objectUpdatePulsingAttacker call_battle_syncToPanelInFrontOfRelated_battle_objectUpdatePulsingAttacker(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_syncToPanelInFrontOfRelated);
    return (call_battle_syncToPanelInFrontOfRelated_result_battle_objectUpdatePulsingAttacker){ R1, R4 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8; bool z, n, c, v; } call_object_presentCollisionData_result_battle_objectUpdatePulsingAttacker;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battle_objectUpdatePulsingAttacker call_object_presentCollisionData_battle_objectUpdatePulsingAttacker(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battle_objectUpdatePulsingAttacker){ R0, R1, R2, R3, R4, R5, R6, R7, R8, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_objectUpdatePulsingAttacker: 080D2B2C-080D2B8E THUMB */
/* Pulse the attacker's collision region and timers.  Destroys the object if the
 * attacker link is invalid or Timer2 underflows.  Always syncs to the front panel
 * and presents collision data at the end. */

static void update_pulsing_attacker(u32 r4, u32 r5, u32 r7, u32 r8) {
    /* Remove current collision data, then spawn the pulse visual effect.
     * Both modify r5 among other registers. */
    call_object_removeCollisionData_result_battle_objectUpdatePulsingAttacker rm = call_object_removeCollisionData_battle_objectUpdatePulsingAttacker(r4, r5, r7);
    r5 = rm.r5;

    call_object_spawnCollisionEffect_result_battle_objectUpdatePulsingAttacker se = call_object_spawnCollisionEffect_battle_objectUpdatePulsingAttacker(r5);
    r5 = se.r5;

    bool destroy = false;

    /* If ExtraVars points to another object that doesn't back-reference us,
     * the attacker is gone — mark for destruction. */
    u32 extra_vars = LD32(r5 + oBattleObject_ExtraVars);
    if (extra_vars != 0 && LD32(extra_vars) != r5)
        destroy = true;

    if (!destroy) {
        /* If collision-flagged data exists, clear the region and shake the screen. */
        u32 col_data = LD32(r5 + oBattleObject_CollisionDataPtr);
        u32 col_flags = LD32(col_data + oCollisionData_FlagsFromCollision);
        if (col_flags != 0) {
            call_object_clearCollisionRegion_battle_objectUpdatePulsingAttacker(r5);
            call_camera_initShakeEffect_80302a8(0, 0xC);
        }

        /* Pulse-interval timer: underflow means the attack phase ended. */
        u32 t2 = (u32)LD16(r5 + oBattleObject_Timer2) - 1;
        ST16(r5 + oBattleObject_Timer2, t2);
        if ((s32)t2 < 0) {
            destroy = true;
        } else {
            /* Main timer: decrement; on expiry reset to 12 and (unless Param3==1)
             * force the collision region active. */
            u32 t1 = (u32)LD16(r5 + oBattleObject_Timer) - 1;
            ST16(r5 + oBattleObject_Timer, t1);
            if ((s32)t1 <= 0) {
                ST16(r5 + oBattleObject_Timer, 0xC);
                if (LD8(r5 + oBattleObject_Param3) != 1)
                    call_object_setCollisionRegion_battle_objectUpdatePulsingAttacker(1, r5);
            }
        }
    }

    if (destroy) {
        call_object_clearCollisionRegion_battle_objectUpdatePulsingAttacker(r5);
        ST32(r5 + oBattleObject_ExtraVars + 8, 0);
        ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    }

    /* Common tail: align with the panel in front, then present collision data.
     * presentCollisionData writes R0–R8 and flags; those are the function's outputs. */
    call_battle_syncToPanelInFrontOfRelated_result_battle_objectUpdatePulsingAttacker sync = call_battle_syncToPanelInFrontOfRelated_battle_objectUpdatePulsingAttacker(r5);
    call_object_presentCollisionData_battle_objectUpdatePulsingAttacker(sync.r1, r5, r8);
}

/* battle_objectUpdatePulsingAttacker(r4 r5 r7 r8 r10) -> r5 r8 */
Next battle_objectUpdatePulsingAttacker(void) {
    update_pulsing_attacker(R4, R5, R7, R8);
    RETURN_TO_CALLER();
}

/* ── battle_syncToPanelInFrontOfRelated ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFrontDirection_result_battle_syncToPanelInFrontOfRelated;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battle_syncToPanelInFrontOfRelated call_object_getFrontDirection_battle_syncToPanelInFrontOfRelated(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battle_syncToPanelInFrontOfRelated){ R0 };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battle_syncToPanelInFrontOfRelated(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r1, r2, r3, r4, r6; bool c, v; } call_object_updateCollisionPanels_result_battle_syncToPanelInFrontOfRelated;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_battle_syncToPanelInFrontOfRelated call_object_updateCollisionPanels_battle_syncToPanelInFrontOfRelated(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_battle_syncToPanelInFrontOfRelated){ R1, R2, R3, R4, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Move this battle object to the panel in front of its related object (the tile
 * it is facing, using that object's front direction), then refresh the derived
 * coordinates and collision footprint.
 *
 * Leaves r1/r4 (and the N/Z/C/V flags) exactly as object_updateCollisionPanels
 * left them, since callers only care about that callee's results. */
static void battle_syncToPanelInFrontOfRelated_impl(u32 object) {
    /* Direction the related object faces: 0 = same column, +/-1 = one over. */
    u32 front_dir = call_object_getFrontDirection_battle_syncToPanelInFrontOfRelated(object).r0;

    u32 related = LD32(object + oBattleObject_RelatedObject1Ptr);

    /* Copy the related object's panel position, offset by its front direction. */
    ST8(object + oBattleObject_PanelX, LD8(related + oBattleObject_PanelX) + front_dir);
    ST8(object + oBattleObject_PanelY, LD8(related + oBattleObject_PanelY));

    call_object_setCoordinatesFromPanels_battle_syncToPanelInFrontOfRelated(object);
    call_object_updateCollisionPanels_battle_syncToPanelInFrontOfRelated(object);
}

Next battle_syncToPanelInFrontOfRelated(void) {
    battle_syncToPanelInFrontOfRelated_impl(R5);
    RETURN_TO_CALLER();
}

/* ── t3_0x79_80D4440 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t3_0x79_80D4440;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t3_0x79_80D4440 call_object_updateSpritePaused_t3_0x79_80D4440(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t3_0x79_80D4440){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: jump-table handler for battle-object state 79;
 * the handler receives r4–r10 through the guest register file and
 * writes back r5, r8–r12 (no generated wrapper covers this computed-address call) */
static u32 state79_dispatch(u32 target, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r10) {
    R4  = r4;
    R5  = r5;
    R6  = r6;
    R7  = r7;
    R8  = r8;
    R10 = r10;
    GUEST_CALL_ADDR(target);
    return R5;  /* handler's updated object pointer */
}

/* State-79 updater: dispatches to a per-state handler via a jump table,
 * then updates sprite-pause bookkeeping. */
static call_object_updateSpritePaused_result_t3_0x79_80D4440 t3_0x79_impl(u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r10) {
    /* Table entries are at pre-scaled byte offsets (0, 4, 8) */
    u32 state  = LD8(r5 + oBattleObject_CurState);
    u32 target = LD32(A_off_80D4458 + state);

    /* Run the state handler; r8–r12 and r10 remain in the guest file */
    u32 handler_r5 = state79_dispatch(target, r4, r5, r6, r7, r8, r10);

    /* object_updateSpritePaused reads r5 and r10 (r10 preserved by convention);
     * its result updates r5, while r8–r12 stay as the handler left them */
    return call_object_updateSpritePaused_t3_0x79_80D4440(handler_r5);
}

/* t3_0x79_80D4440(r4 r5 r6 r7 r8 r10 lr) -> r5 r8 r9 r10 r11 r12 */
Next t3_0x79_80D4440(void) {
    call_object_updateSpritePaused_result_t3_0x79_80D4440 res = t3_0x79_impl(R4, R5, R6, R7, R8, R10);
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── battle_destroyObjectClearExtraVars_80D4464 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5, r6, r7; bool z, n, c, v; } call_object_genericDestroy_result_sub_80D4464;
/* register form: generated wrapper: object_genericDestroy has no typed C interface yet */
static inline call_object_genericDestroy_result_sub_80D4464 call_object_genericDestroy_sub_80D4464(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_genericDestroy);
    return (call_object_genericDestroy_result_sub_80D4464){ R0, R1, R2, R3, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Tear down a battle object: the object pointer stays in r5 throughout.
 * Zeroes the head of its extra-variable list, then hands the object to the
 * generic destroyer. r5 is returned unchanged, as the original leaves it. */
static u32 destroy_battle_object_sub_80D4464(u32 object) {
    u32 extraVars = LD32(object + oBattleObject_ExtraVars);
    ST32(extraVars, 0);
    call_object_genericDestroy_sub_80D4464(object);
    return object;
}

/* battle_destroyObjectClearExtraVars_80D4464(r5 = object, lr = return address) -> r5 (unchanged) */
Next battle_destroyObjectClearExtraVars_80D4464(void) {
    R5 = destroy_battle_object_sub_80D4464(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_initActorStateT4_80D4472 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battleObject_initActorStateT4_80D4472(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r0, r1, r2, r4, r5, r6, r7; bool c, v; } call_battleObject_initSpriteAndCollision_8011504_result_battleObject_initActorStateT4_80D4472;
/* register form: generated wrapper: battleObject_initSpriteAndCollision_8011504 has no typed C interface yet */
static inline call_battleObject_initSpriteAndCollision_8011504_result_battleObject_initActorStateT4_80D4472 call_battleObject_initSpriteAndCollision_8011504_battleObject_initActorStateT4_80D4472(u32 r0, u32 r1, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battleObject_initSpriteAndCollision_8011504);
    return (call_battleObject_initSpriteAndCollision_8011504_result_battleObject_initActorStateT4_80D4472){ R0, R1, R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r4, r5, r6, r7; } call_spawn_t1_0x0_EffectObject_result_battleObject_initActorStateT4_80D4472;
/* register form: generated wrapper: spawn_t1_0x0_EffectObject has no typed C interface yet */
static inline call_spawn_t1_0x0_EffectObject_result_battleObject_initActorStateT4_80D4472 call_spawn_t1_0x0_EffectObject_battleObject_initActorStateT4_80D4472(u32 r1, u32 r2, u32 r3, u32 r4) {
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
    return (call_spawn_t1_0x0_EffectObject_result_battleObject_initActorStateT4_80D4472){ R4, R5, R6, R7 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result_battleObject_initActorStateT4_80D4472;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result_battleObject_initActorStateT4_80D4472 call_object_freeMemory_battleObject_initActorStateT4_80D4472(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result_battleObject_initActorStateT4_80D4472){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_setCollisionHitEffect has no typed C interface yet */
static inline void call_object_setCollisionHitEffect_battleObject_initActorStateT4_80D4472(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionHitEffect);
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battleObject_initActorStateT4_80D4472;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battleObject_initActorStateT4_80D4472 call_sprite_setPalette_battleObject_initActorStateT4_80D4472(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battleObject_initActorStateT4_80D4472){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r4, r5, r6, r7, r8; } call_object_presentCollisionData_result_battleObject_initActorStateT4_80D4472;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_battleObject_initActorStateT4_80D4472 call_object_presentCollisionData_battleObject_initActorStateT4_80D4472(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_battleObject_initActorStateT4_80D4472){ R4, R5, R6, R7, R8 };
}

typedef struct { u32 r1, r2, r3; bool c, v; } call_object_reservePanel_result_battleObject_initActorStateT4_80D4472;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_battleObject_initActorStateT4_80D4472 call_object_reservePanel_battleObject_initActorStateT4_80D4472(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_battleObject_initActorStateT4_80D4472){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Type-4 actor initialization: create sprite/collision, configure HP, palette,
 * collision data, panel reservation, and transition to the update state.
 * On failure the object is freed and an effect is spawned instead. */

static void initActorStateT4(u32 obj, u32 panelX, u32 panelY, u32 r8) {
    call_object_setCoordinatesFromPanels_battleObject_initActorStateT4_80D4472(obj);

    call_battleObject_initSpriteAndCollision_8011504_result_battleObject_initActorStateT4_80D4472 sprite =
        call_battleObject_initSpriteAndCollision_8011504_battleObject_initActorStateT4_80D4472(0x41F, 0xC0D0300,
                                                         obj, panelX, panelY);

    /* Creation failed (null returned): spawn effect then free the object. */
    if (sprite.r0 == 0) {
        u32 params = obj + 0x34;
        u32 fxArg1 = LD32(params);
        u32 fxArg2 = LD32(params + 4);
        u32 fxArg3 = LD32(params + 8) + 0x00100000;   /* shift 0x10 by 16 */

        call_spawn_t1_0x0_EffectObject_result_battleObject_initActorStateT4_80D4472 fx =
            call_spawn_t1_0x0_EffectObject_battleObject_initActorStateT4_80D4472(fxArg1, fxArg2, fxArg3, 0x14);

        call_object_freeMemory_battleObject_initActorStateT4_80D4472(fx.r5);
        return;
    }

    obj = sprite.r5;

    /* Successful creation: configure hit effect, HP, animation, palette. */
    call_object_setCollisionHitEffect_battleObject_initActorStateT4_80D4472(0xA, obj);

    ST16(obj + oBattleObject_HP, 1);
    ST16(obj + oBattleObject_MaxHP, 1);

    u32 param1 = LD8(obj + oBattleObject_Param1);
    u32 palette;
    u32 collisionParam;
    if (param1 == 4) {
        ST8(obj + oBattleObject_CurAnim, 4);
        palette = 0;
        collisionParam = 4;
    } else {
        palette = param1;
        collisionParam = sprite.r1;
    }

    call_sprite_setPalette_battleObject_initActorStateT4_80D4472(palette, obj);

    call_object_presentCollisionData_result_battleObject_initActorStateT4_80D4472 coll =
        call_object_presentCollisionData_battleObject_initActorStateT4_80D4472(collisionParam, obj, r8);
    obj = coll.r5;

    /* ExtraVars[0] self-references the object. */
    ST32(LD32(obj + oBattleObject_ExtraVars), obj);

    /* Reserve the panel this actor occupies. */
    u32 px = LD8(obj + oBattleObject_PanelX);
    u32 py = LD8(obj + oBattleObject_PanelY);
    call_object_reservePanel_battleObject_initActorStateT4_80D4472(px, py, obj);

    /* Transition to the update state with a 30-frame timer. */
    ST16(obj + oBattleObject_Timer, 0x1E);
    ST8(obj + oBattleObject_CurState, CUR_STATE_UPDATE);
}

/* battleObject_initActorStateT4_80D4472(r5=obj, r6=panelX, r7=panelY, r8, r10) */
Next battleObject_initActorStateT4_80D4472(void) {
    initActorStateT4(R5, R6, R7, R8);
    RETURN_TO_CALLER();
}

/* ── battleObject_updateMovementHighlight_80D4544 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; } call_object_highlightPanel_result_battleObject_updateMovementHighlight_80D4544;
/* register form: generated wrapper: object_highlightPanel has no typed C interface yet */
static inline call_object_highlightPanel_result_battleObject_updateMovementHighlight_80D4544 call_object_highlightPanel_battleObject_updateMovementHighlight_80D4544(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_highlightPanel);
    return (call_object_highlightPanel_result_battleObject_updateMovementHighlight_80D4544){ R1, R2, R3 };
}

/* Countdown a battle object's movement-highlight timer.
 *
 * While the timer runs and bit 2 of the timer is clear, highlight the panel
 * the object stands on (that call leaves its own r1 behind, which the caller
 * reads as this function's output). Once the timer counts down to zero or
 * below, set the "ready to move" flag, switch the current action to 4 and
 * clear the phase state; on that path the `mov r1, #2` for the flag leaves
 * r1 = 2, which is also what the caller sees.
 *
 * Returns the final r1: 4 when the highlight call was skipped, otherwise
 * whatever object_highlightPanel left there, or 2 if the timer expired.
 * (obj = r5) */
static u32 update_movement_highlight(u32 obj) {
    u32 timer = LD16(obj + oBattleObject_Timer);
    u32 r1_out;

    if (timer & 4) {
        /* Bit already set: skip the panel highlight, r1 keeps the mov value. */
        r1_out = 4;
    } else {
        r1_out = call_object_highlightPanel_battleObject_updateMovementHighlight_80D4544(
                     LD8(obj + oBattleObject_PanelX),
                     LD8(obj + oBattleObject_PanelY)).r1;
    }

    /* Decrement the timer. The original reloads it here because the highlight
     * call may have changed it. Keep counting while it is still above zero. */
    timer = LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, timer);
    if ((s32)timer > 0)
        return r1_out;

    /* Timer expired: mark the object ready to move. The `orr` with r1 = 2
     * is the last thing to write r1 before returning, so r1 ends up as 2. */
    r1_out = 2;
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | r1_out);
    ST8(obj + oBattleObject_CurAction, 4);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    return r1_out;
}

/* battleObject_updateMovementHighlight_80D4544(r5 = battle object) -> r1 */
Next battleObject_updateMovementHighlight_80D4544(void) {
    R1 = update_movement_highlight(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_dispatchCurPhaseByJumpTable_80D4570 ── */
/* Battle object phase dispatcher.
 *
 * Each battle object carries a "current phase" counter. This trampoline reads
 * it from the object at r5 and jumps through a small table next to the code
 * (off_80D4584) to that phase's tick handler:
 *   phase entry 0 -> battleObject_phaseTickToNext_80D458C
 *   phase entry 4 -> battle_tickWhiteFlashBlink_80D45AE
 * The table is indexed by the raw phase byte (the original uses it directly
 * as a byte offset, so phases step in units of 4), and the handler is entered
 * with r0 = phase and r1 = the table entry it was dispatched through, exactly
 * as the original leaves the registers before its `bx r1`.
 *
 * Everything the handler leaves behind (r1, r5, r8-r12, flags) is this
 * function's output state: the guest call below produces it directly.
 */

/* register form: the dispatch target comes from the object's phase table at
 * run time, so the call must go through the guest code address, and the
 * handler's register side effects (r1, r5, r8-r12) are this function's
 * outputs and must stay in the guest register state. */
static void dispatch_cur_phase_by_table(u32 objectPtr) {
    u32 phase  = LD8(objectPtr + oBattleObject_CurPhase);
    u32 target = LD32(A_off_80D4584 + phase);   /* handler address for this phase */

    R0 = phase;                                 /* handler sees the phase... */
    R1 = target;                                /* ...and its table entry, as original */
    GUEST_CALL_ADDR(target);
}

/* battleObject_dispatchCurPhaseByJumpTable_80D4570(r5 = battle object) */
Next battleObject_dispatchCurPhaseByJumpTable_80D4570(void) {
    dispatch_cur_phase_by_table(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_phaseTickToNext_80D458C ── */
/* Advance one object's phase timer by a tick.
 *
 * The first time this runs for an object (PhaseInitialized clear) the timer is
 * seeded from the object's Param2. Every call then decrements the timer; while
 * it stays positive nothing else happens. When it hits zero (or below), the
 * timer restarts at 8 and the object moves to phase 4, which also clears the
 * initialized byte again so the next cycle re-seeds from Param2. */
static void phase_tick_to_next(u32 self) {
    if (LD8(self + oBattleObject_PhaseInitialized) == 0) {
        ST8(self + oBattleObject_PhaseInitialized, 1);
        ST16(self + oBattleObject_Timer, LD8(self + oBattleObject_Param2));
    }

    s32 timer = (s32)LD16(self + oBattleObject_Timer) - 1;
    ST16(self + oBattleObject_Timer, timer);
    if (timer > 0)
        return;

    ST16(self + oBattleObject_Timer, 8);
    ST16(self + oBattleObject_CurPhaseAndPhaseInitialized, 4);
}

/* r5 = object, no register outputs (flags and r0 are clobbered only) */
Next battleObject_phaseTickToNext_80D458C(void) {
    phase_tick_to_next(R5);
    RETURN_TO_CALLER();
}

/* ── battle_tickWhiteFlashBlink_80D45AE ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; } call_sprite_forceWhitePalette_result;
/* register form: generated wrapper: sprite_forceWhitePalette has no typed C interface yet */
static inline call_sprite_forceWhitePalette_result call_sprite_forceWhitePalette(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_forceWhitePalette);
    return (call_sprite_forceWhitePalette_result){ R1, R2, R3 };
}

/* Advance one frame of the white-flash effect on a battle object.
 *
 * While the object's timer has bit 1 set, the sprite is forced to the white
 * palette.  The 16-bit timer counts down every frame; when it reaches zero the
 * flash counter (Param3) is bumped:
 *   - counter < 2: blink again (next animation frame, phase cleared),
 *   - counter == 2: last blink done, Param2 gains 0x3C (60 frames) and the
 *     next animation frame is queued,
 *   - counter > 2: the effect is finished, move on to CurAction 0x0C.
 *
 * Returns the value the original leaves in r1: 2 unless sprite_forceWhitePalette
 * ran, in which case that call's r1 result.
 */
static u32 white_flash_tick(u32 obj) {
    u32 r1_out = 2;                       /* mov r1, #2 before the test */

    if (LD16(obj + oBattleObject_Timer) & 2)
        r1_out = call_sprite_forceWhitePalette(obj).r1;

    /* Countdown; note the original reloads the timer after the palette call. */
    u32 timer = LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, timer);
    if ((s32)timer > 0)
        return r1_out;

    /* Timer expired: advance the flash counter. */
    u32 flashCount = LD8(obj + oBattleObject_Param3) + 1;
    ST8(obj + oBattleObject_Param3, flashCount);

    if (flashCount > 2) {
        /* All flashes done: hand control back to the action state machine. */
        ST8(obj + oBattleObject_CurAction, 0xC);
        ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
        return r1_out;
    }

    if (flashCount == 2) {
        /* Final blink: schedule the next one 60 frames out. */
        ST8(obj + oBattleObject_Param2, LD8(obj + oBattleObject_Param2) + 0x3C);
    }

    /* Blink again: next animation frame, phase reset. */
    ST8(obj + oBattleObject_CurAnim, LD8(obj + oBattleObject_CurAnim) + 1);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    return r1_out;
}

/* battle_tickWhiteFlashBlink_80D45AE(r5 = battle object) -> r1 */
Next battle_tickWhiteFlashBlink_80D45AE(void) {
    R1 = white_flash_tick(R5);
    RETURN_TO_CALLER();
}

/* ── battle_killObjectByHpDrain_80D45EC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5; } call_sub_80D4674_result;
/* register form: generated wrapper: battle_initObject3Passes_80D4674 has no typed C interface yet */
static inline call_sub_80D4674_result call_sub_80D4674(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_initObject3Passes_80D4674);
    return (call_sub_80D4674_result){ R5 };
}

typedef struct { u32 r1; } call_battle_decrementHp_result;
/* register form: generated wrapper: battle_decrementHp has no typed C interface yet */
static inline call_battle_decrementHp_result call_battle_decrementHp(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_decrementHp);
    return (call_battle_decrementHp_result){ R1 };
}

/* Destroy a battle object (battle_killObjectByHpDrain_80D45EC).
 *
 * The object pointer lives in r5 on entry and on exit. If the object's Param1
 * is 4, run the extra cleanup in battle_initObject3Passes_80D4674 first (which may replace the
 * object pointer). Then deduct HP from the object's related object and mark
 * this object as destroyed.
 *
 * r1 on exit is the value battle_decrementHp leaves behind; the wrapper for
 * that call already leaves it in place. */
static u32 destroy_battle_object_sub_80D45EC(u32 obj) {
    if (LD8(obj + oBattleObject_Param1) == 4)
        obj = call_sub_80D4674(obj).r5;

    call_battle_decrementHp(LD32(obj + oBattleObject_RelatedObject1Ptr));

    ST8(obj + oBattleObject_CurState, CUR_STATE_DESTROY);
    return obj;
}

/* battle_killObjectByHpDrain_80D45EC(r5 = object) -> r5 (object), r1 (from battle_decrementHp) */
Next battle_killObjectByHpDrain_80D45EC(void) {
    R5 = destroy_battle_object_sub_80D45EC(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_runThunderboltPhase_80D4604 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline void call_object_clearCollisionRegion_sub_80D4604(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
}

typedef struct { u32 r0, r5, r7; } call_battleObject_findClosestEnemyBattleObject_80103F8_result_sub_80D4604;
/* register form: generated wrapper: battleObject_findClosestEnemyBattleObject_80103F8 has no typed C interface yet */
static inline call_battleObject_findClosestEnemyBattleObject_80103F8_result_sub_80D4604 call_battleObject_findClosestEnemyBattleObject_80103F8_sub_80D4604(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battleObject_findClosestEnemyBattleObject_80103F8);
    return (call_battleObject_findClosestEnemyBattleObject_80103F8_result_sub_80D4604){ R0, R5, R7 };
}

typedef struct { u32 r1, r2, r3, r5, r7; bool c, v; } call_spawnThunderboltObject_80C7D50_result;
/* register form: generated wrapper: spawnThunderboltObject_80C7D50 has no typed C interface yet */
static inline call_spawnThunderboltObject_80C7D50_result call_spawnThunderboltObject_80C7D50(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(spawnThunderboltObject_80C7D50);
    return (call_spawnThunderboltObject_80C7D50_result){ R1, R2, R3, R5, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Thunderbolt phase handler.
 * First tick: initializes animation (3) and countdown timer (10), or destroys
 * immediately if Param1 == 4. Subsequent ticks decrement the timer; on expiry
 * the object clears its collision region, finds the closest enemy, spawns a
 * lightning bolt at that enemy's tile position, and marks itself DESTROY.
 *
 * inputs:  r5 (battle object pointer), r10 (passed through to findClosestEnemy)
 * outputs: r1 (thunderbolt result or pass-through), r5 (object or thunderbolt result)
 * clobbers: nz c v r0 r2 r3 lr
 * preserved: r4 r5 r6 r7
 */

typedef struct { u32 r1; u32 r5; } sub_80D4604_result;

static sub_80D4604_result process_thunderbolt_object(u32 object, u32 in_r1) {
    /* --- First-run initialization --- */
    if (LD8(object + oBattleObject_PhaseInitialized) == 0) {
        ST8(object + oBattleObject_PhaseInitialized, 1);

        /* Variant with Param1 == 4 never fires; destroy immediately. */
        if (LD8(object + oBattleObject_Param1) == 4) {
            ST8(object + oBattleObject_CurState, CUR_STATE_DESTROY);
            return (sub_80D4604_result){ in_r1, object };
        }

        ST8(object + oBattleObject_CurAnim, 3);
        ST16(object + oBattleObject_Timer, 0xA);
    }

    /* --- Decrement countdown; if still positive, return for next tick --- */
    u32 timer = LD16(object + oBattleObject_Timer);
    u32 remaining = timer - 1;
    ST16(object + oBattleObject_Timer, remaining);

    if ((s32)remaining > 0)
        return (sub_80D4604_result){ in_r1, object };

    /* --- Timer expired: fire the thunderbolt --- */
    call_object_clearCollisionRegion_sub_80D4604(object);

    /* Flip alliance to find the opposing side. */
    u32 target_alliance = LD8(object + oBattleObject_Alliance) ^ 1;

    call_battleObject_findClosestEnemyBattleObject_80103F8_result_sub_80D4604 find_res =
        call_battleObject_findClosestEnemyBattleObject_80103F8_sub_80D4604(target_alliance, object);

    /* Enemy tile coordinates (y at +0x13, x at +0x12). */
    u32 enemy_y = LD8(find_res.r0 + 0x13);
    u32 enemy_x = LD8(find_res.r0 + 0x12);

    /* Damage / counter-disabler word from the found enemy. */
    u32 dmg_data = LD32(find_res.r5 + oBattleObject_DamageAndStaminaDamageCounterDisabler);

    call_spawnThunderboltObject_80C7D50_result spawn_res =
        call_spawnThunderboltObject_80C7D50(enemy_x, enemy_y, 3, 0, 0, find_res.r5, dmg_data);

    /* Mark target (r5 after spawnThunderbolt) for removal. */
    ST8(spawn_res.r5 + oBattleObject_CurState, CUR_STATE_DESTROY);

    return (sub_80D4604_result){ spawn_res.r1, spawn_res.r5 };
}

Next battleObject_runThunderboltPhase_80D4604(void) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    sub_80D4604_result res = process_thunderbolt_object(R5, R1);
    R1 = res.r1;
    R5 = res.r5;
    R4 = saved_r4;
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── object_spawnObj79FromSource ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6, r7; } call_object_spawnType3_result_object_spawnObj79FromSource;
/* register form: generated wrapper: object_spawnType3 has no typed C interface yet */
static inline call_object_spawnType3_result_object_spawnObj79FromSource call_object_spawnType3_object_spawnObj79FromSource(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType3);
    return (call_object_spawnType3_result_object_spawnObj79FromSource){ R5, R6, R7 };
}

/* Spawn a type-0x79 battle object and transfer position/damage data to it.
 * The push {r0-r2,r5} / pop {r1-r3,r5} asymmetry shifts the values by one
 * register, so the original r0 lands in PanelX, r1 in PanelY, r2 in Element.
 * r5 (source object) is restored by the pop and used for the related-object
 * pointer and alliance/direction copy.  Returns r5 unchanged. */
static u32 spawnObj79FromSource(u32 type, u32 panelX, u32 panelY, u32 element,
                                 u32 arg4, u32 sourceObj) {
    call_object_spawnType3_result_object_spawnObj79FromSource spawn =
        call_object_spawnType3_object_spawnObj79FromSource(0x79, panelX, panelY, element, arg4);
    u32 newObj = spawn.r5;

    if (newObj == 0)
        return sourceObj;

    /* Due to the push/pop shuffle: saved r0→r1, r1→r2, r2→r3 on the pop.
     * The field names below reflect what the assembly actually writes. */
    ST8(newObj + oBattleObject_PanelX, type);
    ST8(newObj + oBattleObject_PanelY, panelX);
    ST8(newObj + oBattleObject_Element, panelY);

    /* Damage/extra-var fields take spawnType3's r6/r7 outputs */
    ST32(newObj + oBattleObject_DamageAndStaminaDamageCounterDisabler, spawn.r6);
    ST32(newObj + oBattleObject_ExtraVars, spawn.r7);

    /* Link the new object back to the source (original r5, restored by pop) */
    ST32(newObj + oBattleObject_RelatedObject1Ptr, sourceObj);

    /* Inherit alliance and direction-flip from the source object */
    ST16(newObj + oBattleObject_AllianceAndDirectionFlip,
         LD16(sourceObj + oBattleObject_AllianceAndDirectionFlip));

    /* Set header flag bit 0x10 on the new object */
    ST8(newObj + oObjectHeader_Flags,
        LD8(newObj + oObjectHeader_Flags) | 0x10);

    return sourceObj;
}

/* object_spawnObj79FromSource(r0=type, r1=panelX, r2=panelY, r3=element,
 *                             r4, r5=sourceObj, r6, r7, lr) -> r5 (unchanged) */
Next object_spawnObj79FromSource(void) {
    R5 = spawnObj79FromSource(R0, R1, R2, R3, R4, R5);
    RETURN_TO_CALLER();
}

/* ── battle_initObject3Passes_80D4674 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5; } call_sub_80D4B44_result;
/* register form: generated wrapper: battle_spawnObject7BAndInit_80D4B44 has no typed C interface yet */
static inline call_sub_80D4B44_result call_sub_80D4B44(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_spawnObject7BAndInit_80D4B44);
    return (call_sub_80D4B44_result){ R5 };
}

/* battle_initObject3Passes_80D4674 — initialise the Alliance field of a battle object.
 *
 * r5 points to the battle object. The routine clears its Alliance byte,
 * invokes battle_spawnObject7BAndInit_80D4B44 three times (with the loop counter 3, 2, 1 in r1),
 * each time passing a halfword from the table at byte_80D46A4 as the r4
 * parameter, then sets the Alliance byte to 1.
 *
 * r5 is preserved: it is handed to battle_spawnObject7BAndInit_80D4B44 as its r5 argument and
 * comes back there, so it is still in place when we return. The original
 * pushes/pops r4 and r6 around the calls because battle_spawnObject7BAndInit_80D4B44 clobbers
 * both, so their entry values are restored before returning. */

#define ALLIANCE_TABLE  A_byte_80D46A4   /* halfword parameters, indexed by the loop counter */
#define ALLIANCE_PARAM  0xAu             /* fixed r6 parameter for battle_spawnObject7BAndInit_80D4B44 */
#define ALLIANCE_SIZE   0x00100000u      /* r3 parameter: 0x10 << 16 */

/* Entry values of the registers battle_spawnObject7BAndInit_80D4B44 clobbers but this routine
 * must preserve for its callers. */
typedef struct {
    u32 r4;
    u32 r6;
} PreservedRegs;

/* Run the three battle_spawnObject7BAndInit_80D4B44 passes for one battle object. Returns the
 * entry values of the registers the callee clobbers. */
static PreservedRegs setup_alliance(u32 obj, u32 entry_r4, u32 entry_r6) {
    PreservedRegs keep = { entry_r4, entry_r6 };

    ST8(obj + oBattleObject_Alliance, 0);

    for (u32 counter = 3; counter != 0; counter--) {
        u32 param = LD16(ALLIANCE_TABLE + counter * 2);
        call_sub_80D4B44(0, counter, 0, ALLIANCE_SIZE, param, obj, ALLIANCE_PARAM);
    }

    ST8(obj + oBattleObject_Alliance, 1);
    return keep;
}

/* battle_initObject3Passes_80D4674: r5 (object) -> r5; r4 and r6 preserved */
Next battle_initObject3Passes_80D4674(void) {
    PreservedRegs keep = setup_alliance(R5, R4, R6);
    R4 = keep.r4;
    R6 = keep.r6;
    RETURN_TO_CALLER();
}

/* ── t3_0x7b_80D4A28 ── */
/* Phase dispatcher for a battle object (t3 handler).
 * Reads the object's current state and calls through the state table:
 *   state 0 -> battleObject_initializePhase (freshly spawned object)
 *   state 1 -> object_destroyPhase          (marked for deletion)
 *   state 2 -> object_genericDestroy        (already destroyed)
 * The handler is entered with r0 = state and r1 = its own address, exactly
 * as the original's `mov lr, pc; bx r1` leaves them. Whatever r5 and r8-r12
 * the handler leaves behind are this handler's outputs, so the typed
 * function computes nothing extra and the adapter writes no registers. */

/* register form: indirect call through the state table; the callee expects
 * r0 = state and r1 = handler address, so both are set before the call. */
static void call_state_handler_t3_0x7b_80D4A28(u32 state, u32 handler) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

/* Look up the phase handler for the object's current state and run it. */
static void run_state_phase(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D4A3C + state);
    call_state_handler_t3_0x7b_80D4A28(state, handler);
}

Next t3_0x7b_80D4A28(void) {
    run_state_phase(R5);
    RETURN_TO_CALLER();
}

/* ── object_destroyPhase_80D4A98 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_removeCollisionData_result_object_destroyPhase_80D4A98;
/* register form: generated wrapper: object_removeCollisionData has no typed C interface yet */
static inline call_object_removeCollisionData_result_object_destroyPhase_80D4A98 call_object_removeCollisionData_object_destroyPhase_80D4A98(u32 r4, u32 r5, u32 r7) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(object_removeCollisionData);
    return (call_object_removeCollisionData_result_object_destroyPhase_80D4A98){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6; bool c, v; } call_object_spawnCollisionEffect_result_object_destroyPhase_80D4A98;
/* register form: generated wrapper: object_spawnCollisionEffect has no typed C interface yet */
static inline call_object_spawnCollisionEffect_result_object_destroyPhase_80D4A98 call_object_spawnCollisionEffect_object_destroyPhase_80D4A98(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_spawnCollisionEffect);
    return (call_object_spawnCollisionEffect_result_object_destroyPhase_80D4A98){ R2, R3, R4, R5, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { bool z, n; } call_object_clearCollisionRegion_result_object_destroyPhase_80D4A98;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_object_destroyPhase_80D4A98 call_object_clearCollisionRegion_object_destroyPhase_80D4A98(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_object_destroyPhase_80D4A98){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: object_setCollisionRegion has no typed C interface yet */
static inline void call_object_setCollisionRegion_object_destroyPhase_80D4A98(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionRegion);
}

typedef struct { u32 r1, r3, r4, r5; bool c, v; } call_object_updateSprite_result_object_destroyPhase_80D4A98;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_object_destroyPhase_80D4A98 call_object_updateSprite_object_destroyPhase_80D4A98(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_object_destroyPhase_80D4A98){ R1, R3, R4, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r5, r6, r8; bool z, n, c, v; } call_object_presentCollisionData_result_object_destroyPhase_80D4A98;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_object_destroyPhase_80D4A98 call_object_presentCollisionData_object_destroyPhase_80D4A98(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_object_destroyPhase_80D4A98){ R0, R1, R2, R3, R5, R6, R8, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Dispatch a projectile advance tick via the single-entry jump table.
 * register form: the callee is reached through `mov lr, pc; bx r1`; it
 * reads r0 (action index), r5 (battle object), r7 (collision data ptr),
 * r10 (state).  GUEST_CALL_ADDR requires the target registers on the CPU. */
static u32 dispatch_action_tick(u32 target, u32 action, u32 obj, u32 col_data, u32 r10) {
    R0 = action;
    R5 = obj;
    R7 = col_data;
    R10 = r10;
    GUEST_CALL_ADDR(target);
    return R5;
}

/* Destroy-phase body for a battle object (projectile tick path).
 * Sequence: strip collision, spawn spark FX, conditionally stash panel &
 * clear region, jump-table action dispatch, conditionally assign region,
 * advance sprite, present hitbox.
 * register form: the generated wrappers for removeCollisionData,
 * spawnCollisionEffect and updateSprite write R4 as a return-value side
 * effect, and removeCollisionData also writes R7.  Both R4 and R7 are
 * preserved by this function (original push {r4,r7,lr} / pop).  There is
 * no C-level mechanism to suppress a wrapper's register write, so we
 * restore them from the saved parameters at the end. */
static void object_destroy_phase(u32 r4, u32 r5, u32 r7, u32 r8, u32 r10) {
    /* Remove collision data and spawn the destruction spark effect. */
    call_object_removeCollisionData_result_object_destroyPhase_80D4A98 rm = call_object_removeCollisionData_object_destroyPhase_80D4A98(r4, r5, r7);
    u32 cur_obj = rm.r5;

    call_object_spawnCollisionEffect_result_object_destroyPhase_80D4A98 sp = call_object_spawnCollisionEffect_object_destroyPhase_80D4A98(cur_obj);
    cur_obj = sp.r5;

    /* If the object carried collision flags, save its panel position and
     * clear the region tile. */
    u32 col_data = LD32(cur_obj + oBattleObject_CollisionDataPtr);
    if (LD32(col_data + oCollisionData_FlagsFromCollision) != 0) {
        u16 panel_xy = LD16(cur_obj + oBattleObject_PanelXY);
        ST32(cur_obj + oBattleObject_ExtraVars, panel_xy);
        call_object_clearCollisionRegion_object_destroyPhase_80D4A98(cur_obj);
    }

    /* Jump-table dispatch: currently only one entry (projectile advance). */
    u32 action = LD8(cur_obj + oBattleObject_CurAction);
    u32 target = LD32(A_off_80D4AFC + action);
    cur_obj = dispatch_action_tick(target, action, cur_obj, col_data, r10);

    /* After the tick: if the object has no collision region assigned and
     * its panel moved from the saved value, claim region 1. */
    col_data = LD32(cur_obj + oBattleObject_CollisionDataPtr);
    if (LD8(col_data + oCollisionData_Region) == 0) {
        u16 panel_xy = LD16(cur_obj + oBattleObject_PanelXY);
        u32 saved = LD32(cur_obj + oBattleObject_ExtraVars);
        if (panel_xy != saved) {
            call_object_setCollisionRegion_object_destroyPhase_80D4A98(1, cur_obj);
        }
    }

    /* Advance the sprite sheet, then present the collision hitbox. */
    call_object_updateSprite_result_object_destroyPhase_80D4A98 upd = call_object_updateSprite_object_destroyPhase_80D4A98(cur_obj);
    call_object_presentCollisionData_object_destroyPhase_80D4A98(upd.r1, upd.r5, r8);

    /* Restore preserved registers clobbered by wrapper side effects. */
    R4 = r4;
    R7 = r7;
}

/* object_destroyPhase_80D4A98: 080D4A98 THUMB
 * inputs: r4 r5 r7 r8 r10 lr; outputs: r5 r8 r9 r10 r11 r12;
 * preserved: r4 r7; clobbers: nz c v r0 r1 r2 r3 r6 lr */
Next object_destroyPhase_80D4A98(void) {
    object_destroy_phase(R4, R5, R7, R8, R10);
    RETURN_TO_CALLER();
}

/* ── object_projectileAdvanceXTick ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6; } call_object_setPanelsFromCoordinates_result_object_projectileAdvanceXTick;
/* register form: generated wrapper: object_setPanelsFromCoordinates has no typed C interface yet */
static inline call_object_setPanelsFromCoordinates_result_object_projectileAdvanceXTick call_object_setPanelsFromCoordinates_object_projectileAdvanceXTick(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setPanelsFromCoordinates);
    return (call_object_setPanelsFromCoordinates_result_object_projectileAdvanceXTick){ R5, R6 };
}

typedef struct { u32 r1, r2, r3, r4, r6; } call_object_updateCollisionPanels_result_object_projectileAdvanceXTick;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_object_projectileAdvanceXTick call_object_updateCollisionPanels_object_projectileAdvanceXTick(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_object_projectileAdvanceXTick){ R1, R2, R3, R4, R6 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_object_projectileAdvanceXTick;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_object_projectileAdvanceXTick call_PlaySoundEffect_object_projectileAdvanceXTick(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_object_projectileAdvanceXTick){ R1, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_object_projectileAdvanceXTick;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_object_projectileAdvanceXTick call_object_clearCollisionRegion_object_projectileAdvanceXTick(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_object_projectileAdvanceXTick){ R1 };
}

/* Projectile per-tick X advance (r5 = battle object).
 *
 * While Param1 (a pre-movement delay) is non-zero it is simply counted down.
 * Once it hits zero the projectile moves by XVelocity, its occupied panels and
 * collision data are refreshed, and two edge conditions are handled:
 *   - panel column 2 with Param2 still set: clear Param2 and play sound 0xFF,
 *   - panel column 7: end of flight - release the collision region and switch
 *     the object to CUR_STATE_DESTROY.
 *
 * object_setPanelsFromCoordinates and PlaySoundEffect may retarget the object
 * pointer (r5) and PlaySoundEffect also produces the r10 the caller expects,
 * so both are threaded through and reported back. */
typedef struct {
    u32 obj;   /* r5 as the callees left it */
    u32 r10;   /* r10 as the callees left it */
} ProjectileTickResult;

static ProjectileTickResult projectile_advance_x_tick(u32 obj, u32 r10) {
    /* Still waiting out the movement delay: tick it down and do nothing else. */
    u32 moveDelay = LD8(obj + oBattleObject_Param1);
    if (moveDelay != 0) {
        ST8(obj + oBattleObject_Param1, moveDelay - 1);
        ProjectileTickResult unchanged = { obj, r10 };
        return unchanged;
    }

    /* Advance one step along X and refresh the panels the projectile covers. */
    ST32(obj + oBattleObject_X,
         LD32(obj + oBattleObject_X) + LD32(obj + oBattleObject_XVelocity));
    call_object_setPanelsFromCoordinates_result_object_projectileAdvanceXTick panels =
        call_object_setPanelsFromCoordinates_object_projectileAdvanceXTick(obj);
    obj = panels.r5;   /* setPanelsFromCoordinates may leave a new object pointer */

    call_object_updateCollisionPanels_result_object_projectileAdvanceXTick collision =
        call_object_updateCollisionPanels_object_projectileAdvanceXTick(obj);

    /* Reached column 2 with Param2 set: drop Param2 and play the impact sound.
     * The sound effect gets the r2 that updateCollisionPanels happened to leave. */
    if (LD8(obj + oBattleObject_PanelX) == 2 && LD8(obj + oBattleObject_Param2) == 1) {
        ST8(obj + oBattleObject_Param2, 0);
        call_PlaySoundEffect_result_object_projectileAdvanceXTick sound = call_PlaySoundEffect_object_projectileAdvanceXTick(0xFF, collision.r2);
        obj = sound.r5;
        r10 = sound.r10;
    }

    /* Reached column 7: free the collision region and mark the object destroyed. */
    if (LD8(obj + oBattleObject_PanelX) == 7) {
        call_object_clearCollisionRegion_object_projectileAdvanceXTick(obj);
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
             CUR_STATE_DESTROY);
    }

    ProjectileTickResult done = { obj, r10 };
    return done;
}

Next object_projectileAdvanceXTick(void) {
    ProjectileTickResult result = projectile_advance_x_tick(R5, R10);
    R5 = result.obj;
    R10 = result.r10;
    RETURN_TO_CALLER();
}

/* ── battle_spawnObject7BAndInit_80D4B44 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; } call_object_spawnType3_result_sub_80D4B44;
/* register form: generated wrapper: object_spawnType3 has no typed C interface yet */
static inline call_object_spawnType3_result_sub_80D4B44 call_object_spawnType3_sub_80D4B44(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType3);
    return (call_object_spawnType3_result_sub_80D4B44){ R4, R5, R6, R7 };
}

/* register form: generated wrapper: BasicBattleObjectInitialize has no typed C interface yet */
static inline void call_BasicBattleObjectInitialize_sub_80D4B44(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(BasicBattleObjectInitialize);
}

/* Battle helper: spawn object 0x7B through object_spawnType3 (the caller's
 * r1-r4 are the spawn arguments), then, if the spawn produced a non-null
 * object handle, run BasicBattleObjectInitialize on it.
 *
 * Register shuffle done by the original's push/pop pair: the spawn's r5
 * result becomes the init call's r0, while the init call's r1-r3 are the
 * caller's original r0-r2; r5 and r6 pass through unchanged. */
static void spawn_object_7B_and_initialize(u32 callerR0, u32 callerR1, u32 callerR2,
                                           u32 callerR3, u32 callerR4,
                                           u32 callerR5, u32 callerR6) {
    u32 object = call_object_spawnType3_sub_80D4B44(0x7B, callerR1, callerR2, callerR3, callerR4).r5;

    if (object != 0)
        call_BasicBattleObjectInitialize_sub_80D4B44(object, callerR0, callerR1, callerR2,
                                         callerR5, callerR6);
}

/* battle_spawnObject7BAndInit_80D4B44: spawn object 0x7B from the caller's r1-r4 and initialize it.
 * Inputs: r0-r6. Outputs: r5 (left unchanged, as the original's pop restores
 * it). r0-r3 and the flags are clobbered. */
Next battle_spawnObject7BAndInit_80D4B44(void) {
    spawn_object_7B_and_initialize(R0, R1, R2, R3, R4, R5, R6);
    RETURN_TO_CALLER();
}

/* ── t3_0x8c_80D6A20 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_t3_0x8c_80D6A20;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_t3_0x8c_80D6A20 call_object_updateSprite_t3_0x8c_80D6A20(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_t3_0x8c_80D6A20){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Snake-arm battle object tick (t3_0x8c_80D6A20).
 *
 * The object's CurState byte indexes a jump table at off_80D6A38 whose entries
 * are the per-state update routines (state 0: battleObject_snakeArmSpawn,
 * state 4: snakearm_hitUpdate, state 8: object_genericDestroy — the table is
 * indexed with the raw byte, so valid states are spaced 4 apart). The handler
 * is entered with r0 = state index and r1 = its own address, and control falls
 * through to the common object_updateSprite afterwards, which receives
 * whatever r5 the handler left behind.
 *
 * register form: the handlers are reached through `mov lr, pc; bx r1`, so the
 * tool generates no wrappers for them. The helper below performs that one
 * dispatch and reports the handler's r5, which the original passes on to
 * object_updateSprite (the state handlers may retarget r5).
 */
static u32 call_snake_arm_state_handler(u32 state, u32 handler) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    return R5;
}

/* Run the state handler for this object, then its sprite update. */
static void snake_arm_update(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80D6A38 + state);

    u32 object_after_handler = call_snake_arm_state_handler(state, handler);
    call_object_updateSprite_t3_0x8c_80D6A20(object_after_handler);
    /* r5/r8..r12 outputs are whatever the two callees left behind. */
}

/* t3_0x8c_80D6A20(r4 r5 r7 r8 r10 lr) -> r5 r8 r9 r10 r11 r12 */
Next t3_0x8c_80D6A20(void) {
    snake_arm_update(R5);
    RETURN_TO_CALLER();
}

/* ── snakearm_hitUpdate ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_removeCollisionData_result_snakearm_hitUpdate;
/* register form: generated wrapper: object_removeCollisionData has no typed C interface yet */
static inline call_object_removeCollisionData_result_snakearm_hitUpdate call_object_removeCollisionData_snakearm_hitUpdate(u32 r4, u32 r5, u32 r7) {
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(object_removeCollisionData);
    return (call_object_removeCollisionData_result_snakearm_hitUpdate){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r4, r5, r6, r7; bool c, v; } call_object_spawnCollisionEffect_result_snakearm_hitUpdate;
/* register form: generated wrapper: object_spawnCollisionEffect has no typed C interface yet */
static inline call_object_spawnCollisionEffect_result_snakearm_hitUpdate call_object_spawnCollisionEffect_snakearm_hitUpdate(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_spawnCollisionEffect);
    return (call_object_spawnCollisionEffect_result_snakearm_hitUpdate){ R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r3; } call_battle_isBattleOver_result_snakearm_hitUpdate;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result_snakearm_hitUpdate call_battle_isBattleOver_snakearm_hitUpdate(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result_snakearm_hitUpdate){ R0, R1, R3 };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_snakearm_hitUpdate;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_snakearm_hitUpdate call_object_clearCollisionRegion_snakearm_hitUpdate(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_snakearm_hitUpdate){ R1 };
}

typedef struct { u32 r4, r5, r6, r7, r8; } call_object_presentCollisionData_result_snakearm_hitUpdate;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result_snakearm_hitUpdate call_object_presentCollisionData_snakearm_hitUpdate(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result_snakearm_hitUpdate){ R4, R5, R6, R7, R8 };
}

typedef struct { u32 r0, r1, r2, r3, r4; bool z, n, c, v; } call_object_highlightCurrentCollisionPanels_result_snakearm_hitUpdate;
/* register form: generated wrapper: object_highlightCurrentCollisionPanels has no typed C interface yet */
static inline call_object_highlightCurrentCollisionPanels_result_snakearm_hitUpdate call_object_highlightCurrentCollisionPanels_snakearm_hitUpdate(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_highlightCurrentCollisionPanels);
    return (call_object_highlightCurrentCollisionPanels_result_snakearm_hitUpdate){ R0, R1, R2, R3, R4, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * Update hit animation for a Snake-arm object: refresh collision data each
 * frame until the timer expires, then destroy the object and zero out the
 * three panel references at r5+0x64.
 *
 * Inputs  r4, r5 (object ptr), r7, r8.
 * Outputs r5, r8 — both already placed in guest registers by the callee wrappers,
 *         so the adapter writes nothing extra.
 */
static void snakearm_hitUpdate_logic(u32 r4, u32 r5, u32 r7, u32 r8) {
    /* Remove stale collision panels */
    call_object_removeCollisionData_result_snakearm_hitUpdate rm = call_object_removeCollisionData_snakearm_hitUpdate(r4, r5, r7);
    r5 = rm.r5;

    /* Spawn visual hit effect on the remaining collision footprint */
    call_object_spawnCollisionEffect_result_snakearm_hitUpdate sp = call_object_spawnCollisionEffect_snakearm_hitUpdate(r5);
    r5 = sp.r5;

    /* If the battle already ended, skip straight to destruction */
    call_battle_isBattleOver_result_snakearm_hitUpdate ov = call_battle_isBattleOver_snakearm_hitUpdate();
    u32 r1 = ov.r1;                     /* r1 feeds presentCollisionData below */

    if (ov.r0 == 0) {                   /* battle still active */
        r7 = LD32(r5 + oBattleObject_CollisionDataPtr);
        u32 flags = LD32(r7 + oCollisionData_FlagsFromCollision);

        /* Clear the region only when a collision is currently registered */
        if (flags != 0)
            r1 = call_object_clearCollisionRegion_snakearm_hitUpdate(r5).r1;

        /* Advance the one-frame hit timer */
        u32 timer = LD16(r5 + oBattleObject_Timer);
        u32 new_timer = timer - 1;      /* 32-bit SUB; wraps to 0xFFFF if timer was 0 */
        ST16(r5 + oBattleObject_Timer, new_timer);

        if ((s32)new_timer > 0) {
            /* Timer not yet expired: present fresh panels and highlight them */
            call_object_presentCollisionData_result_snakearm_hitUpdate pr =
                call_object_presentCollisionData_snakearm_hitUpdate(r1, r5, r8);
            r5 = pr.r5;
            r8 = pr.r8;

            call_object_highlightCurrentCollisionPanels_snakearm_hitUpdate(r5);
            return;
        }
        /* Timer reached zero or negative — fall through to destroy */
    }

    /* Destroy path: clear any lingering region, set DESTROY phase,
     * and write 1 (validity flag) to offset 0x20 of each of the three
     * panel-pointer slots at r5 + 0x64 .. r5 + 0x6C. */
    call_object_clearCollisionRegion_snakearm_hitUpdate(r5);
    ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);

    u32 ptr = r5 + 0x64;
    for (int i = 0; i < 3; i++) {
        u32 panel = LD32(ptr);
        if (panel != 0)
            ST16(panel + 0x20, 1);
        ptr += 4;
    }
}

/* snakearm_hitUpdate(r4, r5, r7, r8) -> r5, r8 */
Next snakearm_hitUpdate(void) {
    snakearm_hitUpdate_logic(R4, R5, R7, R8);
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

/* ── battleObject_initializeInvulnerable_80D8464 ── */
/* battleObject_initializeInvulnerable_80D8464: Initialize a battle object (FlagMan-type object) by snapping it to its
 * panel, loading its sprite/animation, setting its stat sheet, building its hitbox and
 * stamping the current state-action phase. */
Next battleObject_initializeInvulnerable_80D8464(void) {
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

/* ── airspinObject_80D88E0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1; bool z, n; } call_battle_isTimeStop_result_airspinObject_80D88E0;
/* register form: generated wrapper: battle_isTimeStop has no typed C interface yet */
static inline call_battle_isTimeStop_result_airspinObject_80D88E0 call_battle_isTimeStop_airspinObject_80D88E0(void) {
    GUEST_CALL(battle_isTimeStop);
    return (call_battle_isTimeStop_result_airspinObject_80D88E0){ R0, R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_airspinObject_80D88E0;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_airspinObject_80D88E0 call_object_updateSprite_airspinObject_80D88E0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_airspinObject_80D88E0){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

struct airspin_out {
    u32 r5, r8, r9, r10, r11, r12;
};

/* Helper: jump-table dispatch (mov lr, pc; bx rN).  The tool marks this
 * register-form helper automatically because of GUEST_CALL_ADDR. */
static void airspin_state_dispatch(u32 addr, u32 r4, u32 r5, u32 r6,
                                   u32 r7, u32 r8, u32 r10,
                                   struct airspin_out *out) { /* register form: calls through a guest code address (noted by the tool) */
    R4 = r4; R5 = r5; R6 = r6; R7 = r7; R8 = r8; R10 = r10;
    GUEST_CALL_ADDR(addr);
    out->r5  = R5;
    out->r8  = R8;
    out->r9  = R9;
    out->r10 = R10;
    out->r11 = R11;
    out->r12 = R12;
}

/* AirSpin object tick: invoke the state handler (init / update / destroy)
 * via the jump table, then refresh the sprite if time-stop is inactive. */
static struct airspin_out airspin_update(u32 r4, u32 r5, u32 r6,
                                         u32 r7, u32 r8, u32 r10) {
    struct airspin_out out;

    /* Look up handler from jump table indexed by the object's current state */
    u32 state = LD8(r5 + oBattleObject_CurState);
    u32 handler_addr = LD32(A_off_80D88FC + state);

    /* Call state handler through the jump table */
    airspin_state_dispatch(handler_addr, r4, r5, r6, r7, r8, r10, &out);

    /* If time is not stopped, update the object's sprite (uses current r5) */
    call_battle_isTimeStop_result_airspinObject_80D88E0 ts = call_battle_isTimeStop_airspinObject_80D88E0();
    if (ts.z) {
        call_object_updateSprite_result_airspinObject_80D88E0 sprite = call_object_updateSprite_airspinObject_80D88E0(out.r5);
        out.r5 = sprite.r5;
    }

    return out;
}

/* airspinObject_80D88E0(r4, r5, r6, r7, r8, r10) -> r5, r8, r9, r10, r11, r12 */
Next airspinObject_80D88E0(void) {
    struct airspin_out out = airspin_update(R4, R5, R6, R7, R8, R10);
    R5  = out.r5;
    R8  = out.r8;
    R9  = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── object_updateSlideMovement ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_canSlideOntoPanel has no typed C interface yet */
static inline u32 call_object_canSlideOntoPanel(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_canSlideOntoPanel);
    return R0;
}

typedef struct { u32 r0, r3; } call_IsR2BetweenR0AndR1_result_object_updateSlideMovement;
/* register form: generated wrapper: IsR2BetweenR0AndR1 has no typed C interface yet */
static inline call_IsR2BetweenR0AndR1_result_object_updateSlideMovement call_IsR2BetweenR0AndR1_object_updateSlideMovement(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(IsR2BetweenR0AndR1);
    return (call_IsR2BetweenR0AndR1_result_object_updateSlideMovement){ R0, R3 };
}

/* register form: generated wrapper: battle_stepToFrontPanel_80D8BBC has no typed C interface yet */
static inline void call_sub_80D8BBC(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_stepToFrontPanel_80D8BBC);
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_object_updateSlideMovement(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_object_updateCollisionPanels_result_object_updateSlideMovement;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_object_updateSlideMovement call_object_updateCollisionPanels_object_updateSlideMovement(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_object_updateSlideMovement){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r5; } call_convertBattleObjectXYToPanelXY_800E24C_result_object_updateSlideMovement;
/* register form: generated wrapper: convertBattleObjectXYToPanelXY_800E24C has no typed C interface yet */
static inline call_convertBattleObjectXYToPanelXY_800E24C_result_object_updateSlideMovement call_convertBattleObjectXYToPanelXY_800E24C_object_updateSlideMovement(u32 r5) {
    R5 = r5;
    GUEST_CALL(convertBattleObjectXYToPanelXY_800E24C);
    return (call_convertBattleObjectXYToPanelXY_800E24C_result_object_updateSlideMovement){ R0, R1, R5 };
}

typedef struct { u32 r5, r6; } call_object_setPanelsFromCoordinates_result_object_updateSlideMovement;
/* register form: generated wrapper: object_setPanelsFromCoordinates has no typed C interface yet */
static inline call_object_setPanelsFromCoordinates_result_object_updateSlideMovement call_object_setPanelsFromCoordinates_object_updateSlideMovement(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setPanelsFromCoordinates);
    return (call_object_setPanelsFromCoordinates_result_object_updateSlideMovement){ R5, R6 };
}

/* Helper: finish the slide — snap to panel coords, update collision, reset phase.
 * Returns the collision panel r1 left by object_updateCollisionPanels. */
static u32 finish_slide(u32 obj) {
    call_object_setCoordinatesFromPanels_object_updateSlideMovement(obj);
    call_object_updateCollisionPanels_result_object_updateSlideMovement coll = call_object_updateCollisionPanels_object_updateSlideMovement(obj);
    ST8(obj + oBattleObject_CurAction, 4);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    return coll.r1;
}

/* Helper: commit the new X, recompute panels, refresh collision.
 * Returns the collision panel r1 left by object_updateCollisionPanels. */
static u32 commit_slide(u32 obj, u32 targetX) {
    ST32(obj + oBattleObject_X, targetX);
    call_object_setPanelsFromCoordinates_object_updateSlideMovement(obj);
    call_object_updateCollisionPanels_result_object_updateSlideMovement coll = call_object_updateCollisionPanels_object_updateSlideMovement(obj);
    return coll.r1;
}

/* Advance a sliding battle object by one frame.
 * obj: battle-object pointer (r5).  Returns r1 (collision panel from the last
 * updateCollisionPanels call). */
static u32 update_slide_movement(u32 obj) {
    /* Lazily mark the slide phase as initialized. */
    if (!LD8(obj + oBattleObject_PhaseInitialized)) {
        u32 futX = LD8(obj + oBattleObject_FuturePanelX);
        u32 futY = LD8(obj + oBattleObject_FuturePanelY);
        if (!call_object_canSlideOntoPanel(futX, futY))
            return finish_slide(obj);
        ST8(obj + oBattleObject_PhaseInitialized, 4);
    }

    /* Predict the next X position. */
    u32 targetX = LD32(obj + oBattleObject_XVelocity) + LD32(obj + oBattleObject_X);

    /* If the object is sliding across a panel boundary… */
    if (call_IsR2BetweenR0AndR1_object_updateSlideMovement(targetX, LD32(obj + oBattleObject_X),
                                LD32(obj + oBattleObject_ExtraVars)).r0) {
        call_sub_80D8BBC(obj);

        u32 futX = LD8(obj + oBattleObject_FuturePanelX);
        u32 futY = LD8(obj + oBattleObject_FuturePanelY);
        if (!call_object_canSlideOntoPanel(futX, futY))
            return finish_slide(obj);

        /* Decrement the slide timer; stop when it expires. */
        s32 timer = (s32)LD16(obj + oBattleObject_Timer2) - 1;
        ST16(obj + oBattleObject_Timer2, (u32)timer);
        if (timer <= 0)
            return finish_slide(obj);
    }

    /* Map the predicted position to panel coordinates and decide whether the
     * object has reached a new panel. */
    call_convertBattleObjectXYToPanelXY_800E24C_result_object_updateSlideMovement conv =
        call_convertBattleObjectXYToPanelXY_800E24C_object_updateSlideMovement(obj);
    u32 panelX = conv.r0;

    if (call_object_canSlideOntoPanel(conv.r0, conv.r1) &&
        LD8(obj + oBattleObject_PanelX) != panelX)
        return finish_slide(obj);

    return commit_slide(obj, targetX);
}

/* object_updateSlideMovement (0x080d8a3c)
 * r5 = battle-object pointer; returns r1 (collision panel), r5 preserved. */
Next object_updateSlideMovement(void) {
    R1 = update_slide_movement(R5);
    RETURN_TO_CALLER();
}

/* ── battle_repeatActionPhase ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r5; } call_battle_spawnPanelElementEffect_result;
/* register form: generated wrapper: battle_spawnPanelElementEffect has no typed C interface yet */
static inline call_battle_spawnPanelElementEffect_result call_battle_spawnPanelElementEffect(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_spawnPanelElementEffect);
    return (call_battle_spawnPanelElementEffect_result){ R1, R2, R5 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_battle_repeatActionPhase;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battle_repeatActionPhase call_PlaySoundEffect_battle_repeatActionPhase(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battle_repeatActionPhase){ R1, R2, R3, R4, R5, R6, R7, R10 };
}

/* battle_repeatActionPhase — run one "repeat" phase of a battle object's action.
 *
 * On the first tick the phase is initialized (animation 1, timer 0). The phase
 * timer then counts down every tick; when it reaches zero it is reloaded from
 * the object's extra vars, the panel effect and the sound (0xC7) play again,
 * and the repeat counter in Param2 is decremented. Once the counter is used
 * up, the object advances to action 0x0C with phase 0.
 *
 * r1/r5/r10 as seen by callers come straight from the called helpers (r1 and
 * r10 from PlaySoundEffect, r5 unchanged), so the adapter leaves them alone;
 * on the early-return paths they are untouched, matching the original.
 */
static void repeat_action_phase(u32 obj) {
    /* First entry into this phase: set up animation and start the timer. */
    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        ST8(obj + oBattleObject_CurAnim, 1);
        ST16(obj + oBattleObject_Timer, 0);
        ST8(obj + oBattleObject_PhaseInitialized, 4);
    }

    /* Tick the phase timer; while time remains, done for this frame. */
    u32 timer = LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, timer);
    if ((s32)timer > 0)
        return;

    /* Timer expired: reload it and replay the panel effect and sound. */
    ST16(obj + oBattleObject_Timer, LD32(obj + oBattleObject_ExtraVars + 4));
    call_battle_spawnPanelElementEffect_result fx =
        call_battle_spawnPanelElementEffect(obj);
    call_PlaySoundEffect_battle_repeatActionPhase(0xC7, fx.r2);

    /* Count down the number of repeats; keep repeating while any remain. */
    u32 repeats = LD8(obj + oBattleObject_Param2) - 1;
    ST8(obj + oBattleObject_Param2, repeats);
    if ((s32)repeats > 0)
        return;

    /* Repeats exhausted: move on to the next action with phase 0. */
    ST8(obj + oBattleObject_CurAction, 0x0C);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
}

/* battle_repeatActionPhase(r5 = battle object) */
Next battle_repeatActionPhase(void) {
    repeat_action_phase(R5);
    RETURN_TO_CALLER();
}

/* ── battle_timedSelfDestructState ── */
/*
 * battle_timedSelfDestructState (0x080D8B18) — per-tick state handler for a
 * battle object that blows itself up after a short fuse (r5 = battle object).
 *
 * On the first tick (PhaseInitialized == 0) the object's current animation is
 * blanked, the phase-init effect is spawned from the parameter block that
 * starts at obj+0x34 (effect kind 0x14 travels in r4), and a 4-tick fuse is
 * lit. Every tick then decrements the fuse; while it is still positive the
 * tick ends there. When it reaches zero the object is torn down: its destroy
 * events are dispatched, its entry is dropped from the battle-state object
 * list, its collision region is cleared, and it enters the CUR_STATE_DESTROY
 * state.
 *
 * Register notes: r4 is pushed/popped by the original, so it must come out
 * exactly as it came in even though the effect kind is passed in it; the
 * spawn helper below saves and restores it around the call. r5 is untouched.
 * r1 is left behind by the last callee, which the guest call chain below
 * reproduces naturally — the adapter writes nothing itself.
 */

/* spawn_t1_0x0_EffectObject(r0 = param block address, r1-r3 = its three
 * words, r4 = effect kind). r4 is a saved register for this function, so the
 * helper stashes the caller's value and puts it back after the call — the
 * original's push {r4}/pop {r4} does the same. */
/* register form: the effect kind argument travels in r4, a register the
 * function must preserve, so plain C parameters cannot carry it. */
static void call_spawn_t1_0x0_EffectObject_battle_timedSelfDestructState(u32 block, u32 w0, u32 w1, u32 w2, u32 kind) {
    u32 saved_r4 = R4;
    R0 = block;
    R1 = w0;
    R2 = w1;
    R3 = w2;
    R4 = kind;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
    R4 = saved_r4;
}

/* battle_dispatchDestroyEvents_802EF5C(r0 = the value the fuse counted down to) */
static void call_battle_dispatchDestroyEvents_802EF5C(u32 r0) { /* register form: calls battle_dispatchDestroyEvents_802EF5C, which has no typed C interface yet (noted by the tool) */
    R0 = r0;
    GUEST_CALL(battle_dispatchDestroyEvents_802EF5C);
}

/* The remaining teardown calls get no arguments of their own: the original
 * invokes each with whatever registers the previous callee left behind, so
 * these helpers only make the call and let the registers flow through. */
static void call_clearEntryOfBattleStateBattleObjectList_800F656(void) { /* register form: calls clearEntryOfBattleStateBattleObjectList_800F656, which has no typed C interface yet (noted by the tool) */
    GUEST_CALL(clearEntryOfBattleStateBattleObjectList_800F656);
}

static void call_object_clearCollisionRegion_battle_timedSelfDestructState(void) { /* register form: calls object_clearCollisionRegion, which has no typed C interface yet (noted by the tool) */
    GUEST_CALL(object_clearCollisionRegion);
}

static void timed_self_destruct_tick(u32 obj) {
    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        /* First tick of the state: switch animation and light the fuse. */
        ST8(obj + oBattleObject_CurAnim, 0);

        /* Effect parameter block at obj+0x34: its three words go in r1-r3 and
         * the block pointer passed on is past them (ldmia writeback). */
        u32 block = obj + 0x34;
        call_spawn_t1_0x0_EffectObject_battle_timedSelfDestructState(block + 12, LD32(block), LD32(block + 4),
                                       LD32(block + 8), 0x14);

        ST16(obj + oBattleObject_Timer, 4);
        ST8(obj + oBattleObject_PhaseInitialized, 4);
    }

    /* Count the fuse down; a still-burning fuse ends the tick here. */
    s32 fuse = (s32)LD16(obj + oBattleObject_Timer) - 1;
    ST16(obj + oBattleObject_Timer, (u32)fuse);
    if (fuse > 0)
        return;

    /* Fuse burnt out: tear the object down and mark it destroyed. */
    call_battle_dispatchDestroyEvents_802EF5C((u32)fuse);
    call_clearEntryOfBattleStateBattleObjectList_800F656();
    call_object_clearCollisionRegion_battle_timedSelfDestructState();
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
}

Next battle_timedSelfDestructState(void) {
    timed_self_destruct_tick(R5);
    RETURN_TO_CALLER();
}

/* ── battle_stepToFrontPanel_80D8BBC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFrontDirection_result_sub_80D8BBC;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_sub_80D8BBC call_object_getFrontDirection_sub_80D8BBC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_sub_80D8BBC){ R0 };
}

typedef struct { u32 r0, r1, r2; } call_object_getCoordinatesForPanels_result_sub_80D8BBC;
/* register form: generated wrapper: object_getCoordinatesForPanels has no typed C interface yet */
static inline call_object_getCoordinatesForPanels_result_sub_80D8BBC call_object_getCoordinatesForPanels_sub_80D8BBC(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_getCoordinatesForPanels);
    return (call_object_getCoordinatesForPanels_result_sub_80D8BBC){ R0, R1, R2 };
}

/* Advance the object one panel in its facing direction: compute the next panel
 * X/Y, store them as the future coordinates, look up the pixel coordinates
 * for that panel, and stash the X pixel value in ExtraVars.
 * r2 is left by the getCoordinatesForPanels callee (not explicitly set here). */
static void step_to_front_panel(u32 object) {
    u32 facing = call_object_getFrontDirection_sub_80D8BBC(object).r0;

    u32 futureX = facing + LD8(object + oBattleObject_PanelX);
    u32 futureY = LD8(object + oBattleObject_PanelY);

    ST8(object + oBattleObject_FuturePanelX, futureX);
    ST8(object + oBattleObject_FuturePanelY, futureY);

    call_object_getCoordinatesForPanels_result_sub_80D8BBC coords =
        call_object_getCoordinatesForPanels_sub_80D8BBC(futureX, futureY);
    ST32(object + oBattleObject_ExtraVars, coords.r0);
}

/* battle_stepToFrontPanel_80D8BBC(r5 = object) -> r2 (pixel X from getCoordinatesForPanels) */
Next battle_stepToFrontPanel_80D8BBC(void) {
    step_to_front_panel(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnPanelElementEffect ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6, r7; } call_object_spawnCollisionRegion_result_battle_spawnPanelElementEffect;
/* register form: generated wrapper: object_spawnCollisionRegion has no typed C interface yet */
static inline call_object_spawnCollisionRegion_result_battle_spawnPanelElementEffect call_object_spawnCollisionRegion_battle_spawnPanelElementEffect(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_spawnCollisionRegion);
    return (call_object_spawnCollisionRegion_result_battle_spawnPanelElementEffect){ R5, R6, R7 };
}

typedef struct { u32 r0, r1, r2, r3, r5; } call_spawn_t1_0x0_EffectObject_result_battle_spawnPanelElementEffect;
/* register form: generated wrapper: spawn_t1_0x0_EffectObject has no typed C interface yet */
static inline call_spawn_t1_0x0_EffectObject_result_battle_spawnPanelElementEffect call_spawn_t1_0x0_EffectObject_battle_spawnPanelElementEffect(u32 r1, u32 r2, u32 r3, u32 r4) {
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
    return (call_spawn_t1_0x0_EffectObject_result_battle_spawnPanelElementEffect){ R0, R1, R2, R3, R5 };
}

/* Spawn an element-effect object on the battle panel occupied by the given object,
 * then bump the new effect's Param3 by 2 if the spawn succeeded.
 * Outputs r1 (Param3+2 or callee's r1) and r2 (callee's r2); r5 unchanged. */

struct SpawnPanelEffectResult {
    u32 r1;
    u32 r2;
};

static struct SpawnPanelEffectResult spawn_panel_element_effect(u32 object_ptr) {
    /* register form: callee wrappers clobber R4, R6, R7; original preserves them
       via push/pop {r4,r6,r7,lr}. We save before the calls and restore at the end. */
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    u8 panel_x = LD8(object_ptr + oBattleObject_PanelX);
    u8 panel_y = LD8(object_ptr + oBattleObject_PanelY);
    u8 element = LD8(object_ptr + oBattleObject_Element);
    u32 disabler = LD32(object_ptr + oBattleObject_DamageAndStaminaDamageCounterDisabler);

    /* Register collision region for this panel's element effect. */
    call_object_spawnCollisionRegion_battle_spawnPanelElementEffect(panel_x, panel_y, element, 0,
                                     0x1205FF10, object_ptr, disabler, 1);

    /* Effect header: palette 0x43 in byte 0, alliance in byte 1. */
    u32 header = 0x43 | (LD8(object_ptr + oBattleObject_Alliance) << 8);

    /* Read three parameter words from the object's 0x34 field. */
    u32 param1 = LD32(object_ptr + 0x34);
    u32 param2 = LD32(object_ptr + 0x38);
    u32 param3 = LD32(object_ptr + 0x3C);

    call_spawn_t1_0x0_EffectObject_result_battle_spawnPanelElementEffect effect =
        call_spawn_t1_0x0_EffectObject_battle_spawnPanelElementEffect(param1, param2, param3, header);

    struct SpawnPanelEffectResult out;
    out.r1 = effect.r1;
    out.r2 = effect.r2;

    if (effect.r0) {
        /* Bump the newly spawned effect object's Param3 by 2. */
        out.r1 = LD8(effect.r0 + oBattleObject_Param3) + 2;
        ST8(effect.r0 + oBattleObject_Param3, out.r1);
    }

    /* register form: restore preserved registers clobbered by the wrapper calls */
    R4 = saved_r4;
    R6 = saved_r6;
    R7 = saved_r7;

    return out;
}

/* battle_spawnPanelElementEffect(r5 = BattleObject*, lr) -> r1, r2 */
Next battle_spawnPanelElementEffect(void) {
    struct SpawnPanelEffectResult result = spawn_panel_element_effect(R5);
    R1 = result.r1;
    R2 = result.r2;
    RETURN_TO_CALLER();
}

/* ── battle_setObjectExtraVar1To10_80D8C06 ── */
/* Store the "extra vars" word of the battle object in r5, setting its second
 * halfword slot to 10. */
static void set_object_extra_count(u32 object) {
    ST32(object + (oBattleObject_ExtraVars + 4), 0xA);
}

/* battle_setObjectExtraVar1To10_80D8C06(r5 = battle object) -> r5 field updated */
Next battle_setObjectExtraVar1To10_80D8C06(void) {
    set_object_extra_count(R5);
    RETURN_TO_CALLER();
}

/* ── object_canSlideOntoPanel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3; } call_object_getPanelParameters_result;
/* register form: generated wrapper: object_getPanelParameters has no typed C interface yet */
static inline call_object_getPanelParameters_result call_object_getPanelParameters(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_getPanelParameters);
    return (call_object_getPanelParameters_result){ R0, R2, R3 };
}

/* Check whether the given object may slide onto a panel.
 * Fetches the panel parameter flags via object_getPanelParameters and tests:
 *   - no parameters at all            -> no
 *   - any of the 0x0F800000 bits set  -> no
 *   - the 0x10 flag missing           -> no
 *   otherwise yes.
 * Returns 1 if it may slide, 0 otherwise. */
static u32 object_canSlideOntoPanel_typed(u32 object, u32 panelType) {
    call_object_getPanelParameters_result params =
        call_object_getPanelParameters(object, panelType);
    u32 flags = params.r0;

    if (flags == 0)
        return 0;
    if (flags & 0x0F800000)
        return 0;
    if (!(flags & 0x10))
        return 0;
    return 1;
}

Next object_canSlideOntoPanel(void) {
    R0 = object_canSlideOntoPanel_typed(R0, R1);
    RETURN_TO_CALLER();
}

/* ── battle_releaseObject_80D9532 ── */
/* Reset a battle object that was handed to us in r0:
 *   - clear the pointer the object keeps at offset 0x60 (its attached record),
 *   - store state 0xC at offset 9 (marks the object as finished/released).
 * If r0 is NULL there is nothing to do. Either way r5 comes back as the
 * object pointer (the original copies r0 into r5 before using it), and the
 * flags left behind are those of `cmp r0, #0`: carry set, overflow clear. */
static u32 release_object(u32 saved_r5, u32 object) {
    if (object == 0)
        return saved_r5;

    ST32(LD32(object + 0x60), 0);   /* *object->field60 = NULL */
    ST8(object + 9, 0xC);           /* object->state = 12 */
    return object;
}

/* battle_releaseObject_80D9532(r0 = object) -> r5 = object (or incoming r5 when r0 is NULL),
 * flags C/V from the initial comparison */
Next battle_releaseObject_80D9532(void) {
    R5 = release_object(R5, R0);
    SET_FLAGS_CMP(R0, 0);
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
        ST16(battle + oBattleState_Reward2Bonus, R1);

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
    case 0 * 4: GUEST_CALL(object_battleInitPhase_80DFE64);          break; /* alive/normal */
    case 1 * 4: GUEST_CALL(battleObject_destroyOnCollision_80DFEA8);          break; /* second phase */
    case 2 * 4: GUEST_CALL(object_genericDestroy); break; /* destroyed */
    }

    GUEST_CALL(object_updateSprite);

    RETURN_TO_CALLER();
}

/* ── object_battleInitPhase_80DFE64 ── */
/* object_battleInitPhase_80DFE64 (r5 = battle object):
 * Finish setting up a battle object: snap it to its panel coordinates, copy
 * Param2 into its timer, then build and publish its collision data. If the
 * collision data cannot be created the object is freed instead. On success
 * the object's state advances to CUR_STATE_UPDATE.
 * Preserves r4-r7 (r7 is only used as scratch by the original) and leaves
 * r5/r8 as they came in. */
Next object_battleInitPhase_80DFE64(void) {
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
        u8 unk_1a = LD8(state_addr + oBattleState_State4EventCount);
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
 * battle object; the handler is selected by CurState (Begin / battle_runCustomScreenPhase /
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
        ST32(obj + oBattleObject_ChipID, R7);
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
