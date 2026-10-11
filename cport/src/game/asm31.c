/*
 * asm31.c: readable functions of bn6f asm31.s, up to 0x080C6CE4, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── t1_0x5_tempAttackObject_80B8CD8 ── */
/* register form: tail-dispatch to the temp-attack object's state handler.
 * sub_80B8CF8 / sub_80B8DA6 / object_freeMemory have no typed C interface yet. */
static void call_temp_attack_handler(u32 state, u32 handler_addr, u32 obj_ptr) {
    R0 = state;
    R1 = handler_addr;
    R5 = obj_ptr;
    switch (state) {
    case 0: GUEST_CALL(sub_80B8CF8);      break;
    case 4: GUEST_CALL(sub_80B8DA6);      break;
    case 8: GUEST_CALL(object_freeMemory); break;
    }
}

/* Jump-table dispatch: look up the handler for the object's current state
 * and tail-call it, passing state in r0 and handler address in r1. */
static void temp_attack_dispatch(u32 obj_ptr) {
    u32 state = (u32)LD8(obj_ptr + oBattleObject_CurState);
    u32 handler_addr = LD32(A_off_80B8CEC + state);
    call_temp_attack_handler(state, handler_addr, obj_ptr);
}

Next t1_0x5_tempAttackObject_80B8CD8(void) {
    temp_attack_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80B8CF8 ── */
/* Navi/battle-sprite spawn table: each entry is 5 bytes.
 * [0] sprite tile ID, [1] animation data, [2] palette base,
 * [3] transform sprite ID, [4] arm-position index. */
#define NAVI_SPRITE_TABLE       A_byte_80B8BD4
#define SPRITE_POOL_DEFAULT     0x80
#define PARAM_GLIDE             0xF
#define NAMEID_ROLL_FIRST       0x1AC
#define NAMEID_ROLL_LAST        0x1B5
#define TRANSFORM_SPRITE_ID     0xF8
#define NAMEID_GLIDE_NO_ARM     0x1A0
#define NAMEID_GLIDE_ARM_A      0x1B6

/* ------------------------------------------------------------------ */

/* register form: sprite_load is called with R4 = table entry in the original;
 * the callee may read R4 internally, and the caller's R4 is protected by push/pop. */
static void call_sprite_load(u32 pool, u32 tileID, u32 animData, u32 tableEntry) {
    R0 = pool;
    R1 = tileID;
    R2 = animData;
    R4 = tableEntry;
    GUEST_CALL(sprite_load);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_sprite_loadAnimationData(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_sprite_noShadow(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls GetBattleNaviStatsByte outside this file, which has no typed C interface yet */
static u8 call_GetBattleNaviStatsByte(u8 alliance, u32 statOffset) {
    R0 = alliance;
    R1 = statOffset;
    GUEST_CALL(GetBattleNaviStatsByte);
    return (u8)R0;
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation(u32 animID) {
    R0 = animID;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_sprite_setPalette(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: original passes arm-index from [r4,#4] and table ptr in R4
 * to getBattleArmPositionMaybe_8018810, which returns X in R0, Y in R1 */
static void call_get_arm_position(u32 tableEntry, u32 nameID, u32 armIdx,
                                  u8 alliance, u8 dirFlip,
                                  u32 *outX, u32 *outY) {
    R0 = nameID;
    R1 = armIdx;
    R2 = alliance;
    R3 = dirFlip;
    R4 = tableEntry;
    GUEST_CALL(getBattleArmPositionMaybe_8018810);
    *outX = R0;
    *outY = R1;
}

/* register form: sub_80B8DA6 reads R4 (table entry) internally; original keeps R4
 * = table entry until after the pop restores the caller's value */
static void call_post_init_sub_80B8DA6(u32 tableEntry) {
    R4 = tableEntry;
    GUEST_CALL(sub_80B8DA6);
}

/* ------------------------------------------------------------------ */

/* Spawn and initialise a battle-navi sprite from the per-Param1 entry in the
 * navi-sprite table, set animation, palette, arm/velocity offset, and state. */
static void init_battle_navi_sprite(u32 obj) {
    u32 param1 = LD8(obj + oBattleObject_Param1);
    u32 entry  = NAVI_SPRITE_TABLE + param1 * 5;

    /* Load sprite, animation data, disable shadow. */
    call_sprite_load(SPRITE_POOL_DEFAULT, LD8(entry), LD8(entry + 1), entry);
    call_sprite_loadAnimationData();
    call_sprite_noShadow();

    /* Determine sprite ID: roll-family navis that are currently transformed
     * get a fixed override (0xF8); otherwise use the table's byte [3]. */
    u32 related = LD32(obj + oBattleObject_RelatedObject1Ptr);
    u32 nameID  = LD16(related + oBattleObject_NameID);

    u8 sprId = LD8(entry + 3);
    if (nameID >= NAMEID_ROLL_FIRST && nameID <= NAMEID_ROLL_LAST) {
        u8 alliance  = LD8(obj + oBattleObject_Alliance);
        u8 transform = call_GetBattleNaviStatsByte(alliance, oNaviStats_Transformation);
        if (transform)
            sprId = TRANSFORM_SPRITE_ID;
    }
    ST8(obj + oBattleObject_Unk_0c, sprId);

    /* Set initial animation from Param2. */
    u8 anim = LD8(obj + oBattleObject_Param2);
    ST8(obj + oBattleObject_CurAnim, anim);
    ST8(obj + oBattleObject_CurAnimCopy, anim);
    call_sprite_setAnimation(anim);
    call_sprite_loadAnimationData();

    /* Mark object as visible (flag bit 1). */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Palette: table base + per-object Param4 offset. */
    call_sprite_setPalette(LD8(entry + 2) + LD8(obj + oBattleObject_Param4));

    /* Reset velocities, then compute arm/velocity offset (16.16 fixed-point). */
    ST32(obj + oBattleObject_XVelocity, 0);
    ST32(obj + oBattleObject_ZVelocity, 0);

    if (param1 == PARAM_GLIDE) {
        /* Glide-type spawn: hard-coded offsets per related navi nameID. */
        if (nameID != NAMEID_GLIDE_NO_ARM) {
            u32 ax, az;
            if (nameID == NAMEID_GLIDE_ARM_A) {
                ax = 0xB;
                az = (u32)(s32)(-7);
            } else {
                ax = 0;
                az = 0xD;
            }
            ST32(obj + oBattleObject_XVelocity, ax << 16);
            ST32(obj + oBattleObject_ZVelocity, az << 16);
        }
    } else {
        /* Normal spawn: arm index from table byte [4] drives position lookup. */
        u8 armIdx = LD8(entry + 4);
        if (armIdx != 0) {
            u32 ax, az;
            call_get_arm_position(entry, nameID, armIdx,
                                  LD8(obj + oBattleObject_Alliance),
                                  LD8(obj + oBattleObject_DirectionFlip),
                                  &ax, &az);
            ST32(obj + oBattleObject_XVelocity, ax << 16);
            ST32(obj + oBattleObject_ZVelocity, az << 16);
        }
    }

    /* Transition into the update state and run post-init. */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
    call_post_init_sub_80B8DA6(entry);
}

/* ------------------------------------------------------------------ */

/* sub_80B8CF8(obj = R5) -> r0-r3 (from sub_80B8DA6), nzcv; preserves caller's r4. */
Next sub_80B8CF8(void) {
    u32 saved_r4 = R4;
    init_battle_navi_sprite(R5);
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── spawn_t1_0x5_tempAttackObject_80B8E30 ── */
/* register form: calls object_spawnType1 outside this file, which has no typed C interface yet */
static u32 call_spawn_type1(u32 x, u32 y, u32 z, u32 params, u32 *leftover_r2) {
    u32 saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    R0 = 5;
    R1 = x;
    R2 = y;
    R3 = z;
    R4 = params;
    GUEST_CALL(object_spawnType1);
    u32 result = R5;
    *leftover_r2 = R2;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    return result;
}

typedef struct {
    u32 object;    /* spawned pointer (0 on failure) */
    u32 r1;        /* final r1: 0x14 on success, caller's original r0 on failure */
    u32 r2;        /* final r2: header flags on success, callee leftover on failure */
    u32 nz_value;  /* value for the last flag-setting instruction */
    u32 c_flag;    /* C flag: 0 on both paths (original uses ADDS/MOVS that clears C) */
} SpawnResult;

/* Spawn a temporary attack object (type 1, index 5), link it to a related
 * object, inherit panel position and alliance, and set header flag bits 0x14.
 * Writes the result pointer to *result_addr (caller's r7). */
static SpawnResult spawn_temp_attack(u32 x, u32 y, u32 z, u32 params,
                                     u32 related_obj, u32 result_addr,
                                     u32 orig_r0) {
    u32 leftover_r2;
    u32 obj = call_spawn_type1(x, y, z, params, &leftover_r2);

    SpawnResult res;
    res.object = obj;

    if (obj != 0) {
        /* Link the new object back to its related object */
        ST32(obj + oBattleObject_RelatedObject1Ptr, related_obj);
        /* Store the result pointer as ExtraVars */
        ST32(obj + oBattleObject_ExtraVars, result_addr);

        /* Inherit panel position and alliance/direction from the related object */
        u16 panel = LD16(related_obj + oBattleObject_PanelXY);
        ST16(obj + oBattleObject_PanelXY, panel);
        u16 alliance = LD16(related_obj + oBattleObject_AllianceAndDirectionFlip);
        ST16(obj + oBattleObject_AllianceAndDirectionFlip, alliance);

        /* Set flag bits 0x14 on the object header */
        u8 flags = LD8(obj + oObjectHeader_Flags) | 0x14;
        ST8(obj + oObjectHeader_Flags, flags);

        res.r1 = 0x14;
        res.r2 = (u32)flags;
        res.nz_value = (u32)flags;
        res.c_flag = 0;  /* C cleared by the original's flag-setting sequence */
    } else {
        res.r1 = orig_r0;
        res.r2 = leftover_r2;
        res.nz_value = 0;
        res.c_flag = 0;  /* C cleared by mov r0, r5 (ADDS encoding) */
    }

    /* Publish the spawned pointer (or 0) to the caller's result slot */
    ST32(result_addr, obj);

    return res;
}

/* spawn_t1_0x5_tempAttackObject_80B8E30
 * (r1=x, r2=y, r3=z, r4=params, r5=related_obj, r7=result_addr,
 *  r0=orig_r0 [restored into r1 on failure], r6=saved) */
Next spawn_t1_0x5_tempAttackObject_80B8E30(void) {
    SpawnResult res = spawn_temp_attack(R1, R2, R3, R4, R5, R7, R0);
    R0 = res.object;
    R1 = res.r1;
    R2 = res.r2;
    SET_FLAGS_NZ(res.nz_value);
    /* The original's "mov r0, r5" uses ADDS encoding (clears C and V);
       the final ORRS only sets N/Z, so C stays 0 from that ADDS. */
    cpu.c = res.c_flag;
    cpu.v = 0;
    RETURN_TO_CALLER();
}

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

/* ── sub_80C4B84 ── */
/* sub_80C4B84: refresh a battle object – update its animation, alliance gate,
 * mirror related-object state (extra vars, direction/sprite flip), and
 * handle emotion data before freeing memory. */

#define ANIM_ACTIVE     0x02u
#define ANIM_CLEAR_MASK 0xFDu  /* ~ANIM_ACTIVE */
#define REL_FLAG_4      0x04u
#define EXTRA_VARS_OFF  0x34u

/* register form: calls sub_80C4C52 outside this file, which has no typed C interface yet */
static u32 get_new_anim(u32 obj) {
    R5 = obj;
    GUEST_CALL(sub_80C4C52);
    return R1;
}

/* register form: calls sub_800EB6C outside this file, which has no typed C interface yet */
static u32 check_alliance_valid(u32 alliance) {
    R0 = alliance;
    GUEST_CALL(sub_800EB6C);
    return R0;
}

/* register form: calls nullsub_10 outside this file, which has no typed C interface yet */
static void call_nullsub_10(void) {
    GUEST_CALL(nullsub_10);
}

/* register form: calls object_getFlag outside this file, which has no typed C interface yet */
static u32 object_flag(u32 related) {
    R5 = related;
    GUEST_CALL(object_getFlag);
    return R0;
}

/* register form: calls object_getFlip outside this file, which has no typed C interface yet */
static u32 object_flip(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_getFlip);
    return R0;
}

/* register form: calls sprite_setFlip outside this file, which has no typed C interface yet */
static void call_sprite_set_flip(u32 obj, u32 flip) {
    R5 = obj;
    R0 = flip;
    GUEST_CALL(sprite_setFlip);
}

/* register form: calls callPossiblyGetBattleEmotion_8015B54 outside this file, which has no typed C interface yet */
static u32 get_battle_emotion(u32 obj, u32 alliance, u32 related) {
    R0 = alliance;
    R4 = related;
    R5 = obj;
    GUEST_CALL(callPossiblyGetBattleEmotion_8015B54);
    return R0;
}

/* register form: calls object_freeMemory outside this file, which has no typed C interface yet */
static void call_object_free_memory(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_freeMemory);
}

/* register form: replicates the original's final "cmp r0, #2" flag state on the emotion==2 early-return path */
static void set_flags_emotion_eq2(void) {
    SET_FLAGS_CMP(2, 2);
}

/* Mark this object as active, then gate on alliance validity. */
static void update_active_flag(u32 obj) {
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | ANIM_ACTIVE);

    u32 valid = check_alliance_valid(LD8(obj + oBattleObject_Alliance));

    if (valid == 0) {
        ST8(obj + oObjectHeader_Flags,
            LD8(obj + oObjectHeader_Flags) & ANIM_CLEAR_MASK);
    } else {
        ST8(obj + oObjectHeader_Flags,
            LD8(obj + oObjectHeader_Flags) | ANIM_ACTIVE);

        if (LD8(obj + oBattleObject_Unk_0c) == 0) {
            ST8(obj + oObjectHeader_Flags,
                LD8(obj + oObjectHeader_Flags) & ANIM_CLEAR_MASK);
        }
    }
}

/* Mirror related-object properties into this object. Returns the related pointer (R4). */
static u32 mirror_related(u32 obj) {
    u32 related = LD32(obj + oBattleObject_RelatedObject1Ptr);

    /* Clear active flag if the related object carries the hidden bit. */
    if (object_flag(related) & REL_FLAG_4) {
        ST8(obj + oObjectHeader_Flags,
            LD8(obj + oObjectHeader_Flags) & ANIM_CLEAR_MASK);
    }

    related = LD32(obj + oBattleObject_RelatedObject1Ptr);

    /* Copy three extra-variable words from related → obj. */
    ST32(obj + EXTRA_VARS_OFF,       LD32(related + EXTRA_VARS_OFF));
    ST32(obj + EXTRA_VARS_OFF + 4,   LD32(related + EXTRA_VARS_OFF + 4));
    ST32(obj + EXTRA_VARS_OFF + 8,   LD32(related + EXTRA_VARS_OFF + 8));

    /* Sync direction flip byte. */
    ST8(obj + oBattleObject_DirectionFlip,
        LD8(related + oBattleObject_DirectionFlip));

    /* Sync sprite horizontal flip. */
    call_sprite_set_flip(obj, object_flip(obj));

    return related;
}

/* If the emotion pointer is set, query battle emotion; otherwise fall through to free.
 * Returns 0 on all paths (the only difference is whether freeMemory was called).
 * Flags and R0–R3 are left in the global register file by the helpers. */
static void process_emotion(u32 obj, u32 related) {
    u32 emotion_data = LD32(LD32(obj + oBattleObject_ExtraVars + 4));

    if (emotion_data != 0) {
        u32 alliance = LD8(obj + oBattleObject_Alliance);
        u32 emotion = get_battle_emotion(obj, alliance, related);

        if (emotion == 2) {
            set_flags_emotion_eq2();
            return;
        }

        /* Clear the emotion pointer. */
        ST32(LD32(obj + oBattleObject_ExtraVars + 4), 0);
    }

    call_object_free_memory(obj);
}

/* Main body: returns the related-object pointer (becomes R4 for the caller). */
static u32 sub_80C4B84_body(u32 obj) {
    /* Update current animation slot. */
    u32 anim = get_new_anim(obj);
    ST8(obj + oBattleObject_CurAnim, anim);

    /* Alliance-gated active flag. */
    update_active_flag(obj);

    call_nullsub_10();

    /* Mirror related-object state. */
    u32 related = mirror_related(obj);

    /* Emotion / free-memory handling. */
    process_emotion(obj, related);

    return related;
}

/* sub_80C4B84(r5 = battle object) -> r4 = related object; r0–r3/flags from last callee */
Next sub_80C4B84(void) {
    R4 = sub_80C4B84_body(R5);
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

/* ── t3_0x0_80C4E58 ── */
/* register form: calls sub_80C4E7C outside this file, which has no typed C interface yet */
static void call_idle_handler(u32 state, u32 object_ptr) {
    R0 = state;
    R5 = object_ptr;
    GUEST_CALL(sub_80C4E7C);
}

/* register form: calls sub_80C4F02 outside this file, which has no typed C interface yet */
static void call_active_handler(u32 state, u32 object_ptr) {
    R0 = state;
    R5 = object_ptr;
    GUEST_CALL(sub_80C4F02);
}

/* register form: calls object_genericDestroy outside this file, which has no typed C interface yet */
static void call_destroy_handler(u32 state, u32 object_ptr) {
    R0 = state;
    R5 = object_ptr;
    GUEST_CALL(object_genericDestroy);
}

/* register form: calls object_updateSprite outside this file, which has no typed C interface yet */
static void call_update_sprite_t3_0x0_80C4E58(u32 object_ptr) {
    R5 = object_ptr;
    GUEST_CALL(object_updateSprite);
}

/* Dispatch the battle object's per-state handler (jump table keyed by CurState byte),
 * then unconditionally refresh its sprite. */
static void battle_object_state_update(u32 object_ptr) {
    u32 state = LD8(object_ptr + oBattleObject_CurState);
    switch (state) {
    case 0:
        call_idle_handler(state, object_ptr);
        break;
    case 4:
        call_active_handler(state, object_ptr);
        break;
    case 8:
        call_destroy_handler(state, object_ptr);
        break;
    }
    call_update_sprite_t3_0x0_80C4E58(object_ptr);
}

/* t3_0x0_80C4E58(r5 = battle object pointer) */
Next t3_0x0_80C4E58(void) {
    battle_object_state_update(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C4F02 ── */
/* Collision phase handler for a battle object.
 * Processes the object's collision data, advances its panel-movement timer,
 * reacts to solid/breakable/crackable panel hits, and either re-presents
 * collision data or destroys the object. */

#define CUR_STATE_DESTROY_sub_80C4F02  8

/* register form: calls object_removeCollisionData outside this file, which has no typed C interface yet */
static void remove_collision_data_sub_80C4F02(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_removeCollisionData);
}

/* register form: calls object_spawnCollisionEffect outside this file, which has no typed C interface yet */
static void spawn_collision_effect_sub_80C4F02(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_spawnCollisionEffect);
}

/* register form: calls battle_isBattleOver outside this file, which has no typed C interface yet */
static bool is_battle_over(u32 obj) {
    R5 = obj;
    GUEST_CALL(battle_isBattleOver);
    return R0 != 0;
}

/* register form: calls object_getFrontDirection outside this file, which has no typed C interface yet */
static s32 get_front_direction(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_getFrontDirection);
    return (s32)R0;
}

/* register form: calls object_isValidPanel outside this file, which has no typed C interface yet */
static bool is_valid_panel(u32 x, u32 y) {
    R0 = x; R1 = y;
    GUEST_CALL(object_isValidPanel);
    return R0 != 0;
}

/* register form: calls sub_80C5014 outside this file, which has no typed C interface yet */
static void sub_80c5014(u32 obj) {
    R5 = obj;
    GUEST_CALL(sub_80C5014);
}

/* register form: calls object_setCoordinatesFromPanels outside this file, which has no typed C interface yet */
static void set_coords_from_panels(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

/* register form: calls battle_objectStepUpOntoElevatedPanel_80C5090 outside this file, which has no typed C interface yet */
static void step_up_elevated_panel(u32 obj) {
    R5 = obj;
    GUEST_CALL(battle_objectStepUpOntoElevatedPanel_80C5090);
}

/* register form: calls object_updateCollisionPanels outside this file, which has no typed C interface yet */
static void update_collision_panels(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_updateCollisionPanels);
}

/* register form: calls sub_80C5050 outside this file, which has no typed C interface yet */
static void sub_80c5050(u32 obj) {
    R5 = obj;
    GUEST_CALL(sub_80C5050);
}

/* register form: calls object_crackPanel outside this file, which has no typed C interface yet */
static void crack_panel(u32 x, u32 y) {
    R0 = x; R1 = y;
    GUEST_CALL(object_crackPanel);
}

/* register form: calls object_breakPanel_dup2 outside this file, which has no typed C interface yet */
static void break_panel(u32 x, u32 y) {
    R0 = x; R1 = y;
    GUEST_CALL(object_breakPanel_dup2);
}

/* register form: calls object_isPanelSolid outside this file, which has no typed C interface yet */
static bool is_panel_solid(u32 x, u32 y) {
    R0 = x; R1 = y;
    GUEST_CALL(object_isPanelSolid);
    return !ZERO_FLAG();
}

/* register form: calls object_setPanelType outside this file, which has no typed C interface yet */
static void set_panel_type(u32 x, u32 y, u32 type) {
    R0 = x; R1 = y; R2 = type;
    GUEST_CALL(object_setPanelType);
}

/* register form: calls object_clearCollisionRegion outside this file, which has no typed C interface yet */
static void clear_collision_region_sub_80C4F02(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_clearCollisionRegion);
}

/* register form: calls object_presentCollisionData outside this file, which has no typed C interface yet */
static u32 present_collision_data_sub_80C4F02(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_presentCollisionData);
    return R0;
}

/* Mark the object as destroyed: clear its collision region and set the destroy state. */
static void destroy_object(u32 obj) {
    clear_collision_region_sub_80C4F02(obj);
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY_sub_80C4F02);
}

/* Handle the case where the object has NOT been hit (no collision flags).
 * Advances the panel-movement timer and shifts the object one panel per tick. */
static u32 handle_no_collision(u32 obj) {
    /* Initialize the phase timer on first entry */
    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        ST16(obj + oBattleObject_Timer, LD16(obj + oBattleObject_Timer2));
        ST8(obj + oBattleObject_PhaseInitialized, 4);
    }

    /* Decrement timer; if still counting, re-present collision data and return */
    s32 timer = (s32)(LD16(obj + oBattleObject_Timer) - 1);
    ST16(obj + oBattleObject_Timer, (u32)timer);
    if (timer >= 0)
        return present_collision_data_sub_80C4F02(obj);

    /* Timer expired: move the object forward one panel */
    s32 dir = get_front_direction(obj);
    u32 new_x = (u32)dir + LD8(obj + oBattleObject_PanelX);
    ST8(obj + oBattleObject_PanelX, new_x);

    s8 y_shift = (s8)LD8(obj + oBattleObject_Param2);
    u32 new_y = LD8(obj + oBattleObject_PanelY) + y_shift;
    ST8(obj + oBattleObject_PanelY, new_y);
    ST8(obj + oBattleObject_Param2, 0);

    if (is_valid_panel(new_x, new_y)) {
        /* Successfully stepped onto a valid panel */
        set_coords_from_panels(obj);
        step_up_elevated_panel(obj);
        update_collision_panels(obj);
        ST8(obj + oBattleObject_PhaseInitialized, 0);
        return present_collision_data_sub_80C4F02(obj);
    }

    /* Invalid panel: special handling for Param1 == 0xC */
    if (LD8(obj + oBattleObject_Param1) == 0xC)
        sub_80c5014(obj);

    destroy_object(obj);
    return present_collision_data_sub_80C4F02(obj);
}

/* Handle the case where the object HAS been hit (collision flags set).
 * Reacts based on Param1 (the panel/object type that was contacted). */
static u32 handle_collision(u32 obj) {
    u32 param1 = LD8(obj + oBattleObject_Param1);

    if (param1 == 0xC)
        sub_80c5050(obj);

    u32 x = LD8(obj + oBattleObject_PanelX);
    u32 y = LD8(obj + oBattleObject_PanelY);

    if (param1 == 7 || param1 == 0x15) {
        /* Crackable panel types */
        crack_panel(x, y);
    } else if (param1 == 0x16) {
        /* Breakable panel */
        break_panel(x, y);
    } else if (param1 == 0x22) {
        /* Solid panel: set to type 0xC (NPC) or keep related object's state (Player) */
        if (is_panel_solid(x, y)) {
            u32 related = LD32(obj + oBattleObject_RelatedObject1Ptr);
            u32 type = LD8(related + oBattleObject_CurState);
            if (LD8(obj + oBattleObject_Alliance) != 0)
                type = 0xC;
            set_panel_type(x, y, type);
        }
    } else if (param1 == 0x24) {
        /* Solid panel: set to type 0xB (NPC) or keep related object's state (Player) */
        if (is_panel_solid(x, y)) {
            u32 related = LD32(obj + oBattleObject_RelatedObject1Ptr);
            u32 type = LD8(related + oBattleObject_CurState);
            if (LD8(obj + oBattleObject_Alliance) != 0)
                type = 0xB;
            set_panel_type(x, y, type);
        }
    } else {
        /* Default solid panel: use related object's state unless it is 0xFF */
        if (is_panel_solid(x, y)) {
            u32 related = LD32(obj + oBattleObject_RelatedObject1Ptr);
            u32 type = LD8(related + oBattleObject_CurState);
            if (type != 0xFF)
                set_panel_type(x, y, type);
        }
    }

    destroy_object(obj);
    return present_collision_data_sub_80C4F02(obj);
}

/* Main collision-phase routine for a battle object (object pointer in obj). */
static u32 process_collision_phase(u32 obj) {
    remove_collision_data_sub_80C4F02(obj);
    spawn_collision_effect_sub_80C4F02(obj);

    if (is_battle_over(obj)) {
        destroy_object(obj);
        return present_collision_data_sub_80C4F02(obj);
    }

    u32 coll_data = LD32(obj + oBattleObject_CollisionDataPtr);
    if (LD32(coll_data + oCollisionData_FlagsFromCollision) != 0)
        return handle_collision(obj);

    return handle_no_collision(obj);
}

/* sub_80C4F02(r5 = obj) -> r0, r1, r2, r3, nz, c, v */
Next sub_80C4F02(void) {
    R0 = process_collision_phase(R5);
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

/* ── t3_0x8_80C5BB0 ── */
/*
 * t3_0x8_80C5BB0 — battle-object state-machine dispatcher.
 *
 * The jump table off_80C5BC8 is indexed by CurState (byte offsets 0, 4, 8):
 *   0 → sub_80C5BD4       (spawn / initialize)
 *   4 → sub_80C5C40       (active / per-frame logic)
 *   8 → object_genericDestroy (death cleanup)
 * After the handler returns, object_updateSprite is called every frame.
 */

/* register form: dispatches via the state jump table and then refreshes the sprite */
static void dispatch_state_and_update_sprite(u32 object) {
    R5 = object;
    u32 state = LD8(R5 + oBattleObject_CurState);
    R0 = state;
    switch (state) {
        case 0: GUEST_CALL(sub_80C5BD4); break;
        case 4: GUEST_CALL(sub_80C5C40); break;
        case 8: GUEST_CALL(object_genericDestroy); break;
    }
    GUEST_CALL(object_updateSprite);
}

/* t3_0x8_80C5BB0(r5 = battle object) → dispatch on CurState, then update sprite */
Next t3_0x8_80C5BB0(void) {
    dispatch_state_and_update_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C5C40 ── */
/* Jump table address for collision action dispatch (entries at +0 and +4). */
#define COLLISION_ACTION_TABLE  0x080C5C94u

/* Handler addresses stored in the table (with Thumb bit). */
#define COLLISION_HANDLER_0  (0x080C5C9C + 1)
#define COLLISION_HANDLER_1  (0x080C5D84 + 1)

/* Bit 1 in ObjectHeader_Flags marks the object as actively colliding. */
#define OBJ_FLAG_COLLIDING  2

/* Collision spawn parameters begin at object offset 0x34 (three words). */
#define oBattleObject_CollisionSpawnParams  0x34

/* Effect type passed to spawn_t1_0x0_EffectObject. */
#define COLLISION_SPAWN_EFFECT_TYPE  0x14

/* State value for the destroy path. */
#define CUR_STATE_DESTROY_sub_80C5C40  8

/* --- register-form helpers --- */

/* register form: calls object_removeCollisionData outside this file */
static void remove_collision_data(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_removeCollisionData);
}

/* register form: calls object_spawnCollisionEffect outside this file */
static void spawn_collision_effect(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_spawnCollisionEffect);
}

/* register form: calls battle_isBattleOver outside this file, returns r0 != 0 */
static bool battle_is_over(void) {
    GUEST_CALL(battle_isBattleOver);
    return R0 != 0;
}

/* register form: calls battle_isTimeStop outside this file, returns true if Z clear */
static bool battle_time_stopped(void) {
    GUEST_CALL(battle_isTimeStop);
    return !ZERO_FLAG();
}

/* register form: calls object_presentCollisionData outside this file */
static void present_collision_data(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_presentCollisionData);
}

/* register form: calls object_clearCollisionRegion then sets R0=CUR_STATE_DESTROY_sub_80C5C40
   (the original does MOV R0,#8 after the BL, which the adapter cannot branch on) */
static void clear_collision_region(u32 obj) {
    R5 = obj;
    GUEST_CALL(object_clearCollisionRegion);
    R0 = CUR_STATE_DESTROY_sub_80C5C40;
}

/* register form: calls spawn_t1_0x0_EffectObject outside this file */
static void spawn_collision_destroy_effect(u32 obj, u32 p0, u32 p1, u32 p2) {
    R1 = p0;
    R2 = p1;
    R3 = p2;
    R4 = COLLISION_SPAWN_EFFECT_TYPE;
    R5 = obj;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
}

/* register form: calls collision action handler (sub_80C5C9C) via bx r1 dispatch */
static void call_collision_action_0(u32 obj, u32 action) {
    R0 = action;
    R5 = obj;
    GUEST_CALL(sub_80C5C9C);
}

/* register form: calls collision action handler (sub_80C5D84) via bx r1 dispatch */
static void call_collision_action_1(u32 obj, u32 action) {
    R0 = action;
    R5 = obj;
    GUEST_CALL(sub_80C5D84);
}

/* --- typed logic --- */

/* Destroy the collision object: clear its colliding flag, clear its region, set state to DESTROY. */
static void destroy_collision_object(u32 obj) {
    u8 flags = LD8(obj + oObjectHeader_Flags);
    ST8(obj + oObjectHeader_Flags, flags & ~OBJ_FLAG_COLLIDING);
    clear_collision_region(obj);
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY_sub_80C5C40);
}

/* Process a collision event on a battle object (r5 = object header pointer).
 * Removes collision data, spawns the hit effect, and depending on game state either
 * presents the data (return), dispatches an action callback, or destroys the object. */
static void handle_collision_event(u32 obj) {
    remove_collision_data(obj);
    spawn_collision_effect(obj);

    /* If the battle has ended, spawn a special visual effect then destroy. */
    if (battle_is_over()) {
        u32 p0 = LD32(obj + oBattleObject_CollisionSpawnParams);
        u32 p1 = LD32(obj + oBattleObject_CollisionSpawnParams + 4);
        u32 p2 = LD32(obj + oBattleObject_CollisionSpawnParams + 8);
        spawn_collision_destroy_effect(obj, p0, p1, p2);
        destroy_collision_object(obj);
        return;
    }

    /* During time-stop, just re-present the collision and bail. */
    if (battle_time_stopped()) {
        present_collision_data(obj);
        return;
    }

    /* Check the collision-data flags: if set, the collision was already resolved. */
    u32 coll_data = LD32(obj + oBattleObject_CollisionDataPtr);
    if (LD32(coll_data + oCollisionData_FlagsFromCollision) != 0) {
        destroy_collision_object(obj);
        return;
    }

    /* Dispatch the per-action collision handler from the jump table. */
    u32 action = LD8(obj + oBattleObject_CurAction);
    u32 handler = LD32(COLLISION_ACTION_TABLE + action);
    if (handler == COLLISION_HANDLER_0)
        call_collision_action_0(obj, action);
    else
        call_collision_action_1(obj, action);

    present_collision_data(obj);
}

/* sub_80C5C40(r5 = battle object pointer) -> side effects; all registers are outputs via callees */
Next sub_80C5C40(void) {
    handle_collision_event(R5);
    RETURN_TO_CALLER();
}

/* ── t3_0xf_80C6414 ── */
/* t3_0xf_80C6414 (0x080C6414): Battle-object state-machine dispatch.
 * Reads CurState from the object, looks up the handler in a ROM jump table,
 * calls it, then refreshes the object's sprite for timestop before returning. */

/* register form: calls sub_80C6438 outside this file, which has no typed C interface yet */
static void state_init(u32 state, u32 handlerAddr, u32 battleObj) {
    R0 = state;
    R1 = handlerAddr;
    R5 = battleObj;
    GUEST_CALL(sub_80C6438);
}

/* register form: calls sub_80C647C outside this file, which has no typed C interface yet */
static void state_active(u32 state, u32 handlerAddr, u32 battleObj) {
    R0 = state;
    R1 = handlerAddr;
    R5 = battleObj;
    GUEST_CALL(sub_80C647C);
}

/* register form: calls object_genericDestroy outside this file, which has no typed C interface yet */
static void state_destroy(u32 state, u32 handlerAddr, u32 battleObj) {
    R0 = state;
    R1 = handlerAddr;
    R5 = battleObj;
    GUEST_CALL(object_genericDestroy);
}

/* register form: calls object_updateSpriteTimestop outside this file, which has no typed C interface yet */
static void update_sprite_timestop(void) {
    GUEST_CALL(object_updateSpriteTimestop);
}

static void dispatch_state(u32 battleObj) {
    u32 state       = LD8(battleObj + oBattleObject_CurState);
    u32 handlerAddr = LD32(A_off_80C642C + state);

    /* Jump-table entries are at byte offsets 0, 4, 8 (word-sized) */
    if (state == 0)
        state_init(state, handlerAddr, battleObj);
    else if (state == 4)
        state_active(state, handlerAddr, battleObj);
    else
        state_destroy(state, handlerAddr, battleObj);

    /* After the handler returns, update the sprite for timestop visibility */
    update_sprite_timestop();
}

/* t3_0xf_80C6414(r5 = BattleObject*) */
Next t3_0xf_80C6414(void) {
    dispatch_state(R5);
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

/* ── sub_80C6A34 ── */
/* register form: calls the phase handler by name; the target is a function pointer
 * read from a table indexed by CurPhase, so no single typed C interface covers it. */
static void dispatch_phase_handler(u32 phase, u32 handler_addr) {
    R0 = phase;
    R1 = handler_addr;
    if (phase == 0)
        GUEST_CALL(sub_80C6A50);
    else
        GUEST_CALL(battle_object_destroy_80C6AAC);
}

/* Dispatch a battle object's per-frame update through its current-phase handler table. */
static void battle_object_phase_update(u32 battle_obj) {
    u32 phase  = LD8(battle_obj + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80C6A48 + phase);
    dispatch_phase_handler(phase, handler);
}

/* sub_80C6A34(r5 = BattleObject*) -> r0, r1, nzcv
 * Looks up CurPhase in the two-entry handler table and tail-calls the result. */
Next sub_80C6A34(void) {
    battle_object_phase_update(R5);
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

/* ── sub_80C6B64 ── */
#define TABLE_ENTRY_SIZE 4
#define PANEL_TYPE_FLAG  0x10
#define OBJ_FLAG_HAS_SPRITE 2
#define CUR_STATE_UPDATE_sub_80C6B64 4
#define NO_TARGET 0xFF
#define SPAWN_SFX 0xA6

/* register form: calls object_isCurrentPanelValid outside this file, which has no typed C interface yet */
static u32 guest_is_current_panel_valid(void) {
    GUEST_CALL(object_isCurrentPanelValid);
    return R0;
}

/* register form: calls object_getPanelParameters outside this file, which has no typed C interface yet */
static u32 guest_get_panel_params(u32 x, u32 y) {
    R0 = x; R1 = y;
    GUEST_CALL(object_getPanelParameters);
    return R0;
}

/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void guest_sprite_load(u32 gfx, u32 mode, u32 id, u32 entry) {
    R0 = gfx; R1 = mode; R2 = id; R4 = entry;
    GUEST_CALL(sprite_load);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void guest_sprite_no_shadow(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void guest_sprite_set_animation(u32 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void guest_load_animation_data(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void guest_set_palette(u32 pal) {
    R0 = pal;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls object_getFlip outside this file, which has no typed C interface yet */
static u32 guest_get_flip(void) {
    GUEST_CALL(object_getFlip);
    return R0;
}

/* register form: calls sprite_setFlip outside this file, which has no typed C interface yet */
static void guest_set_flip(u32 flip) {
    R0 = flip;
    GUEST_CALL(sprite_setFlip);
}

/* register form: calls battle_objectDispatchPanelChange_80C6CFC outside this file, which has no typed C interface yet */
static void guest_dispatch_panel_change(u32 x, u32 y, u32 target) {
    R0 = x; R1 = y; R2 = target;
    GUEST_CALL(battle_objectDispatchPanelChange_80C6CFC);
}

/* register form: calls object_setCoordinatesFromPanels outside this file, which has no typed C interface yet */
static void guest_set_coords_from_panels(void) {
    GUEST_CALL(object_setCoordinatesFromPanels);
}

/* register form: calls object_createCollisionData outside this file, which has no typed C interface yet */
static u32 guest_create_collision(void) {
    GUEST_CALL(object_createCollisionData);
    return R0;
}

/* register form: calls object_setupCollisionData outside this file, which has no typed C interface yet */
static void guest_setup_collision(u32 col, u32 width, u32 height, u32 flag) {
    R0 = col; R1 = width; R2 = height; R3 = flag; R7 = col;
    GUEST_CALL(object_setupCollisionData);
}

/* register form: calls object_setCollisionHitEffect outside this file, which has no typed C interface yet */
static void guest_set_hit_effect(u32 effect) {
    R0 = effect; R1 = 1;
    GUEST_CALL(object_setCollisionHitEffect);
}

/* register form: calls object_presentCollisionData outside this file, which has no typed C interface yet */
static void guest_present_collision(void) {
    GUEST_CALL(object_presentCollisionData);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void guest_play_sound(u32 sfx) {
    R0 = sfx;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls object_freeMemory outside this file, which has no typed C interface yet */
static u32 guest_free_object(void) {
    GUEST_CALL(object_freeMemory);
    return R0;
}

/* Spawn a battle object from the per-Param1 sprite table (byte_80C6B00).
 * Validates panel, loads sprite/animation/palette, sets up collision with
 * width depending on movement target, plays spawn SFX, and enters the
 * update phase. Returns CUR_STATE_UPDATE_sub_80C6B64 on success or object_freeMemory's
 * result on any early-exit path. */
static u32 init_object_from_table(u32 obj) {
    if (guest_is_current_panel_valid() == 0)
        return guest_free_object();

    /* Panel must carry the spawnable-type bit */
    u32 px = LD8(obj + oBattleObject_PanelX);
    u32 py = LD8(obj + oBattleObject_PanelY);
    if ((guest_get_panel_params(px, py) & PANEL_TYPE_FLAG) == 0)
        return guest_free_object();

    /* Table entry (4 bytes): [0]=gfx id, [1]=anim, [2]=timer, [3]=target panel */
    u32 entry = A_byte_80C6B00 + LD8(obj + oBattleObject_Param1) * TABLE_ENTRY_SIZE;
    ST32(obj + oBattleObject_ExtraVars, entry);

    /* Load sprite from the table entry's graphic ID */
    guest_sprite_load(0x80, 0x10, LD8(entry), entry);
    guest_sprite_no_shadow();

    /* Mark object as having a sprite, set animation from table */
    ST8(obj + oObjectHeader_Flags, (u8)(LD8(obj + oObjectHeader_Flags) | OBJ_FLAG_HAS_SPRITE));
    u32 anim = LD8(entry + 1);
    ST8(obj + oBattleObject_CurAnim, anim);
    ST8(obj + oBattleObject_CurAnimCopy, anim);
    guest_sprite_set_animation(anim);
    guest_load_animation_data();

    /* Initialize frame timers and palette */
    ST16(obj + oBattleObject_Timer, LD8(entry + 2));
    ST16(obj + oBattleObject_Timer2, 4);
    guest_set_palette(0);

    /* Apply the object's current horizontal flip */
    guest_set_flip(guest_get_flip());

    /* Dispatch toward the target panel and place at panel coordinates */
    u32 target = LD8(entry + 3);
    guest_dispatch_panel_change(
        LD8(obj + oBattleObject_PanelX),
        LD8(obj + oBattleObject_PanelY),
        target);
    guest_set_coords_from_panels();

    /* Allocate collision data; abort on failure */
    u32 col = guest_create_collision();
    if (col == 0)
        return guest_free_object();

    /* Fixed objects (target 0xFF) get 4-wide collision; others get 10 */
    u32 width = (target == NO_TARGET) ? 4 : 0xA;
    guest_setup_collision(col, width, 5, 3);

    /* Activate collision, set generic hit effect, and present */
    ST8(col + 1, 1);
    guest_set_hit_effect(0xFF);
    guest_present_collision();

    /* Spawn sound and transition to update phase */
    guest_play_sound(SPAWN_SFX);
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE_sub_80C6B64);
    return CUR_STATE_UPDATE_sub_80C6B64;
}

/* sub_80C6B64(r5 = battle object ptr) -> r0, flags from last callee */
/* register form: original saves r4/r7 via push/pop; helpers pass args through R4/R7
 * which remain modified after callee returns, so restore them here */
Next sub_80C6B64(void) {
    u32 saved_r4 = R4, saved_r7 = R7;
    R0 = init_object_from_table(R5);
    R4 = saved_r4;
    R7 = saved_r7;
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
