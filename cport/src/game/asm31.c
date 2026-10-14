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
 * battle_initNaviSprite_80B8CF8 / battleObject_syncSpriteFromOwner_80B8DA6 / object_freeMemory have no typed C interface yet. */
static void call_temp_attack_handler(u32 state, u32 handler_addr, u32 obj_ptr) {
    R0 = state;
    R1 = handler_addr;
    R5 = obj_ptr;
    switch (state) {
    case 0: GUEST_CALL(battle_initNaviSprite_80B8CF8);      break;
    case 4: GUEST_CALL(battleObject_syncSpriteFromOwner_80B8DA6);      break;
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

/* ── battle_initNaviSprite_80B8CF8 ── */
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

/* register form: battleObject_syncSpriteFromOwner_80B8DA6 reads R4 (table entry) internally; original keeps R4
 * = table entry until after the pop restores the caller's value */
static void call_post_init_sub_80B8DA6(u32 tableEntry) {
    R4 = tableEntry;
    GUEST_CALL(battleObject_syncSpriteFromOwner_80B8DA6);
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

/* battle_initNaviSprite_80B8CF8(obj = R5) -> r0-r3 (from battleObject_syncSpriteFromOwner_80B8DA6), nzcv; preserves caller's r4. */
Next battle_initNaviSprite_80B8CF8(void) {
    u32 saved_r4 = R4;
    init_battle_navi_sprite(R5);
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── battleObject_syncSpriteFromOwner_80B8DA6 ── */
/* battleObject_syncSpriteFromOwner_80B8DA6: Sync secondary object (in r5) to its related primary object (in r4):
 *  - copy position offset (0x34..0x40) from r4 into r5, adding XVelocity/ZVelocity
 *    and subtracting the 0xC byte signed shifted left 16 from Y/Z.
 *  - copy bit 1 of related's flags into self's flags (clear bit 1 from r1 mask first).
 *  - mirror color shader / palette / alpha / flip / mosaic from related into self.
 *  - then if ExtraVars[0] != 0, run the sprite update unless time is stopped (or
 *    Param3 set), or unless battle is paused.
 *  - otherwise clear bit 1 of self flags and queue self for destroy.
 *  Preserves r4 r5 r7. */
Next battleObject_syncSpriteFromOwner_80B8DA6(void) {
    u32 r4 = LD32(R5 + oBattleObject_RelatedObject1Ptr);

    /* Copy position block (3 u32s at offset 0x34) from r4 -> r5,
     * adding this frame's XVelocity to X and ZVelocity to Z,
     * and subtracting (signed byte at offset 0xC) << 16 from Y and Z. */
    u32 src = r4 + 0x34;
    u32 dst = R5 + 0x34;
    u32 x = LD32(src);
    u32 y = LD32(src + 4);
    u32 z = LD32(src + 8);
    x += LD32(R5 + oBattleObject_XVelocity);
    z += LD32(R5 + oBattleObject_ZVelocity);
    s32 height = (s32)((s8)LD32(R5 + 0xC)) << 16;
    y -= (u32)height;
    z -= (u32)height;
    ST32(dst, x);
    ST32(dst + 4, y);
    ST32(dst + 8, z);

    /* Copy bit 1 of related's flags into self's flags. */
    u8 self_flags = LD8(R5);
    self_flags &= (u8)~2;
    self_flags |= (u8)(LD8(r4) & 2);
    ST8(R5, self_flags);

    /* Mirror visual state from related into self. */
    R0 = LD32(R5 + oBattleObject_RelatedObject1Ptr);
    GUEST_CALL(sprite_getColorShader);
    GUEST_CALL(sprite_setColorShader);

    R0 = LD32(R5 + oBattleObject_RelatedObject1Ptr);
    GUEST_CALL(sprite_getFinalPalette);
    GUEST_CALL(sprite_setFinalPalette);

    R0 = LD32(R5 + oBattleObject_RelatedObject1Ptr);
    GUEST_CALL(sprite_getMosaicScalingParameters);
    GUEST_CALL(sprite_setAlpha);

    R0 = LD32(R5 + oBattleObject_RelatedObject1Ptr);
    GUEST_CALL(sprite_getFlip);
    GUEST_CALL(sprite_setFlip);

    /* If ExtraVars[0] == 0, mark for destroy and bail. */
    if (LD32(LD32(R5 + oBattleObject_ExtraVars)) == 0) {
        ST8(R5, (u8)(LD8(R5) & (u8)~2));
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
        RETURN_TO_CALLER();
    }

    /* Otherwise: skip the sprite update if Param3 is set, or if time is stopped.
     * Skip the sprite update if battle is paused too. */
    if (LD8(R5 + oBattleObject_Param3) == 0) {
        GUEST_CALL(battle_isTimeStop);
        if (!ZERO_FLAG()) RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isPaused);
    if (!ZERO_FLAG()) RETURN_TO_CALLER();

    GUEST_CALL(object_updateSprite);
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

/* ── t1_0x2c_80C07BC ── */
/* Jump-table trampoline for a battle object's per-state update. */

/* register form: dispatch through a computed code address (jump-table entry) */
static u32 call_handler_addr(u32 addr, u32 state) {
    R0 = state;
    R1 = addr;
    GUEST_CALL_ADDR(addr);
    return R0;
}

/* Read the battle object's state byte (r5 = object pointer), look up the
 * handler in JumpTable80C07CC, and call it.  The handler modifies
 * r5, r8–r12 which emerge as this dispatcher's outputs. */
static void dispatch_battle_object_state(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_JumpTable80C07CC + state);
    call_handler_addr(handler, state);
}

/* t1_0x2c_80C07BC(r5 = battle object) -> r5, r8, r9, r10, r11, r12 */
Next t1_0x2c_80C07BC(void) {
    dispatch_battle_object_state(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C07DC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7, r8, r9, r12; } call_sprite_decompressTemp_result;
/* register form: generated wrapper: sprite_decompressTemp has no typed C interface yet */
static inline call_sprite_decompressTemp_result call_sprite_decompressTemp(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(sprite_decompressTemp);
    return (call_sprite_decompressTemp_result){ R4, R5, R6, R7, R8, R9, R12 };
}

typedef struct { u32 r4, r5; } call_sprite_load_result_sub_80C07DC;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_sub_80C07DC call_sprite_load_sub_80C07DC(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_sub_80C07DC){ R4, R5 };
}

typedef struct { u32 r2, r3, r4, r5; } call_sprite_loadAnimationData_result_sub_80C07DC;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_sub_80C07DC call_sprite_loadAnimationData_sub_80C07DC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_sub_80C07DC){ R2, R3, R4, R5 };
}

/* register form: generated wrapper: sprite_hasShadow has no typed C interface yet */
static inline void call_sprite_hasShadow(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_hasShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_sub_80C07DC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2; bool c, v; } call_object_setCoordinatesFromPanels_result_sub_80C07DC;
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline call_object_setCoordinatesFromPanels_result_sub_80C07DC call_object_setCoordinatesFromPanels_sub_80C07DC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
    return (call_object_setCoordinatesFromPanels_result_sub_80C07DC){ R2, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_sub_80C07DC;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_sub_80C07DC call_object_getFlip_sub_80C07DC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_sub_80C07DC){ R0 };
}

typedef struct { u32 r2; } call_sprite_setFlip_result_sub_80C07DC;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_sub_80C07DC call_sprite_setFlip_sub_80C07DC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_sub_80C07DC){ R2 };
}

/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline void call_sprite_setPalette_sub_80C07DC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
}

typedef struct { u32 r3; bool c, v; } call_sprite_setColorShader_result;
/* register form: generated wrapper: sprite_setColorShader has no typed C interface yet */
static inline call_sprite_setColorShader_result call_sprite_setColorShader(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setColorShader);
    return (call_sprite_setColorShader_result){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_sub_80C0840_result;
/* register form: generated wrapper: sub_80C0840 has no typed C interface yet */
static inline call_sub_80C0840_result call_sub_80C0840(u32 r2, u32 r3, u32 r4, u32 r5, u32 r7) {
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(sub_80C0840);
    return (call_sub_80C0840_result){ R5, R8, R9, R10, R11, R12 };
}

#define CUR_STATE_UPDATE_sub_80C07DC 4

typedef struct {
    u32 r5, r8, r9, r10, r11, r12;
} sub_80C07DC_out;

static sub_80C07DC_out sub_80C07DC_init(u32 r4, u32 r5, u32 r7) {
    /* Decompress tile data from archive 0x0C, slot 0x50. */
    call_sprite_decompressTemp(0xC, 0x50);

    /* Mark the object header as initialized (set bit 1). */
    ST8(r5 + oObjectHeader_Flags, LD8(r5 + oObjectHeader_Flags) | 0x2);

    /* Load the decompressed graphics into VRAM tile-bank 0x80. */
    call_sprite_load_result_sub_80C07DC ld = call_sprite_load_sub_80C07DC(0x80, 0xC, 0x50, r5);
    r5 = ld.r5;

    /* Fetch animation script for the sprite. */
    call_sprite_loadAnimationData_result_sub_80C07DC an1 = call_sprite_loadAnimationData_sub_80C07DC(r5);
    r5 = an1.r5;

    /* Probe for an attached shadow sprite (side-effect only). */
    call_sprite_hasShadow(r5);

    /* Reset the current animation index to frame 0 (live and shadowed copy). */
    ST8(r5 + oBattleObject_CurAnim, 0);
    ST8(r5 + oBattleObject_CurAnimCopy, 0);

    /* Apply animation frame 0. */
    call_sprite_setAnimation_sub_80C07DC(0, r5);

    /* Reload animation data after the frame reset. */
    call_sprite_loadAnimationData_result_sub_80C07DC an2 = call_sprite_loadAnimationData_sub_80C07DC(r5);
    r5 = an2.r5;
    r4 = an2.r4;

    /* Position the object from its home-panel coordinates. */
    call_object_setCoordinatesFromPanels_sub_80C07DC(r5);

    /* Drop depth to ground level. */
    ST32(r5 + oBattleObject_Z, 0);

    /* Mirror sprite to match the object's facing direction. */
    call_object_getFlip_result_sub_80C07DC fl = call_object_getFlip_sub_80C07DC(r5);
    call_sprite_setFlip_result_sub_80C07DC sf = call_sprite_setFlip_sub_80C07DC(fl.r0, r5);
    u32 r2 = sf.r2;

    /* Alternate palette selection when Param1 == 4. */
    u32 pal = (LD8(r5 + oBattleObject_Param1) == 4) ? 1 : 0;
    call_sprite_setPalette_sub_80C07DC(pal, r5);

    /* Full-bright colour shader (0x7FFF = no tint). */
    call_sprite_setColorShader_result cs = call_sprite_setColorShader(0x7FFF, r5);
    u32 r3 = cs.r3;

    /* Transition the state machine to the update phase. */
    ST32(r5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE_sub_80C07DC);

    /* Run the state-specific post-init routine. */
    call_sub_80C0840_result fin = call_sub_80C0840(r2, r3, r4, r5, r7);

    sub_80C07DC_out out = {
        .r5  = fin.r5,
        .r8  = fin.r8,
        .r9  = fin.r9,
        .r10 = fin.r10,
        .r11 = fin.r11,
        .r12 = fin.r12,
    };
    return out;
}

/* sub_80C07DC(r4, r5, r7, r10) -> r5, r8, r9, r10, r11, r12 */
Next sub_80C07DC(void) {
    sub_80C07DC_out r = sub_80C07DC_init(R4, R5, R7);
    R5  = r.r5;
    R8  = r.r8;
    R9  = r.r9;
    R10 = r.r10;
    R11 = r.r11;
    R12 = r.r12;
    RETURN_TO_CALLER();
}

/* ── sub_80C0840 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5; } call_object_updateSprite_result_sub_80C0840;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_sub_80C0840 call_object_updateSprite_sub_80C0840(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_sub_80C0840){ R2, R4, R5 };
}

/* register form: generated wrapper: sub_80C0DB0 has no typed C interface yet */
static inline void call_sub_80C0DB0(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80C0DB0);
}

/* Per-frame update of one battle object (r5 = object).
 *
 * Runs the handler selected by the object's current action through a jump
 * table, then refreshes the object's sprite and applies the shared
 * end-of-update processing. The handler may advance r5 to another object, and
 * so may object_updateSprite; the object current at the end is the one the
 * follow-up processing runs on and the one handed back to the caller in r5. */

/* register form: the per-action handlers are reached through a jump table
 * (mov lr, pc; bx r1) and run on the live guest register state -- r0 carries
 * the action byte, r1 the table entry, r5 the object, and r2/r3/r4/r7/r10 the
 * caller's context -- so this dispatch cannot be expressed as a typed call.
 * The handler's resulting object pointer is read back from r5. */
static u32 run_action_handler(u32 action, u32 entry, u32 object) {
    R0 = action;
    R1 = entry;
    R5 = object;
    GUEST_CALL_ADDR(entry);
    return R5;
}

/* r5 = battle object; returns the object the update ended on. */
static u32 battle_object_update_sub_80C0840(u32 object) {
    u32 action = LD8(object + oBattleObject_CurAction);

    /* The table is indexed with the raw action byte, exactly like the
     * original's ldr r1, [r1, r0] -- no scaling of any kind. */
    u32 entry = LD32(A_off_80C0858 + action);

    /* Dispatch the handler for the current action. */
    u32 obj = run_action_handler(action, entry, object);

    /* Refresh the object's sprite; object_updateSprite hands back the
     * (possibly advanced) current object in r5. */
    obj = call_object_updateSprite_sub_80C0840(obj).r5;

    /* Shared end-of-update processing on the resulting object. */
    call_sub_80C0DB0(obj);

    return obj;
}

Next sub_80C0840(void) {
    R5 = battle_object_update_sub_80C0840(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C0870 ── */
/* Battle object phase dispatcher (0x080C0870).
 *
 * Reads the battle object's current phase byte and uses it directly as a
 * byte offset into the handler table at off_80C0884:
 *   phase 0 -> sub_80C088C
 *   phase 4 -> battleObject_runFirstTurnPhase_80C08EE
 * The original reaches the handler with `mov lr, pc; bx r1`, so the handler
 * returns straight back to this function's caller (the `pop {pc}` here is
 * merely the handler's return path). The handler leaves the outputs r5 and
 * r8-r12 behind; r0/r1 and the flags are scratch. */

/* register form: the handler address is read from the phase table at
 * off_80C0884, so no generated wrapper covers this call. The helper makes
 * that one call with r0 = phase byte and r1 = handler address, exactly as
 * the original's `mov lr, pc; bx r1` does. */
static u32 call_phase_handler_sub_80C0870(u32 handler, u32 phase) {
    R0 = phase;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Look up the handler for the object's current phase and run it. The
 * handler's register results (r5, r8-r12) stay in place for the caller. */
static void run_battle_object_phase(u32 object) {
    u32 phase = LD8(object + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80C0884 + phase);
    call_phase_handler_sub_80C0870(handler, phase);
}

/* sub_80C0870(r5 = battle object) */
Next sub_80C0870(void) {
    run_battle_object_phase(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C088C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
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

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_80C088C;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_80C088C call_PlaySoundEffect_sub_80C088C(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_80C088C){ R1, R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_80C088C: drive one phase of a battle object's flashing intro effect.
 *   - First visit (PhaseInitialized == 0): spawn the effect configured by the three
 *     words at object+0x34 (with +0x140000 added to the third, a coordinate bias),
 *     play the phase sound, reset the tick timer and mark the phase initialized.
 *   - Later visits: set the hide flag, count down the animation length stored in
 *     Unk_19 by 2 per tick, clearing the hide flag on odd ticks to make the object
 *     flash, and once it runs out lock the hide flag, clear the counter and finish
 *     the phase by writing 4 into the cur-phase/initialized halfword.
 */
static void advance_battle_object_phase(u32 object) {
    if (!LD8(object + oBattleObject_PhaseInitialized)) {
        /* Phase start: spawn the effect, sound it off, reset timer, mark initialized. */
        u32 effect_type  = LD32(object + 0x34);
        u32 effect_param = LD32(object + 0x38);
        u32 effect_coord = LD32(object + 0x3C) + 0x00140000;

        /* spawn_t1_0x0_EffectObject takes r1..r4. r0 is loaded with 0x34+r5 but
         * is the address of the args: the caller's r0 already holds that pointer
         * value, and we passed it to spawn through r1 (effect_type). The original
         * leaves r1..r7/NZCV whatever the callee leaves; only the caller's r2 is
         * then reused as the second argument to PlaySoundEffect. */
        call_spawn_t1_0x0_EffectObject_result spawn_result =
            call_spawn_t1_0x0_EffectObject(effect_type, effect_param, effect_coord, 0x26);

        ST8(object + oBattleObject_Unk_19, 0x1F);
        /* PlaySoundEffect: r0 = sound id, r2 = bank/voice; r2 is whatever the
         * effect spawner left behind, which the wrapper preserves for us. */
        call_PlaySoundEffect_sub_80C088C(0x94, spawn_result.r2);
        ST16(object + oBattleObject_Timer, 0);
        ST8(object + oBattleObject_PhaseInitialized, 4);
        return;
    }

    /* Phase already running: flashing hide animation. */
    {
        u32 flags = LD8(object + oObjectHeader_Flags);
        ST8(object + oObjectHeader_Flags, flags | 0x02);

        u32 tick = LD16(object + oBattleObject_Timer) + 1;
        ST16(object + oBattleObject_Timer, tick);
        if (tick & 1) {
            /* odd tick: clear the hide bit again */
            flags = LD8(object + oObjectHeader_Flags);
            ST8(object + oObjectHeader_Flags, flags & ~0x02);
        }
    }

    {
        s32 frames_left = (s32)LD8(object + oBattleObject_Unk_19) - 2;
        ST8(object + oBattleObject_Unk_19, (u8)frames_left);
        if (frames_left > 0)
            return;

        /* Animation over: keep the object hidden and complete the phase. */
        ST8(object + oObjectHeader_Flags, LD8(object + oObjectHeader_Flags) | 0x02);
        ST8(object + oBattleObject_Unk_19, 0);
        ST16(object + oBattleObject_CurPhaseAndPhaseInitialized, 4);
    }
}

/* sub_80C088C(r5 = battle object) -> r5, r10 unchanged */
Next sub_80C088C(void) {
    advance_battle_object_phase(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C0912 ── */
/* sub_80C0912 -- phase dispatcher for one battle object (r5).
 *
 * Reads the object's current phase byte and calls the matching handler
 * from the table at off_80C0924:
 *   0 battle_panelObject_tryTargetAndSetPhase_80C0938
 *   1 sub_80C0956
 *   2 battleObject_jumpUpPhase_80C0988
 *   3 sub_80C09C6
 *   4 sub_80C0A0E
 *
 * The handler is entered with the state the original sets up:
 *   r0 = phase byte
 *   r1 = table entry that was jumped through (the handler's own address)
 *   r2 = forwarded unchanged from the dispatcher's caller
 *   r5 = battle object pointer
 * Whatever the handler leaves in r5 and r8-r12 is this function's result;
 * r0-r4/r6/r7 and the flags are scratch, lr returns to the caller. */

struct PhaseDispatchResult {
    u32 object;   /* r5 */
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
};

/* register form: the phase handler is reached through a jump-table address
 * (ldr r1, [table, phase]; mov lr, pc; bx r1), so no generated wrapper
 * covers it. The helper reproduces the original's register state, makes the
 * single call, and reads the surviving outputs back out of the register
 * file once the handler returns. */
static struct PhaseDispatchResult run_phase_handler_sub_80C0912(u32 handler, u32 phase,
                                                    u32 arg2, u32 object)
{
    R0 = phase;   /* CurPhase value, as the original passes it */
    R1 = handler; /* table entry the original jumps through */
    R2 = arg2;    /* argument forwarded from the dispatcher's caller */
    R5 = object;  /* battle object pointer */

    GUEST_CALL_ADDR(handler);

    struct PhaseDispatchResult out;
    out.object = R5;
    out.r8 = R8;
    out.r9 = R9;
    out.r10 = R10;
    out.r11 = R11;
    out.r12 = R12;
    return out;
}

/* Look up the handler for the object's current phase and run it. */
static struct PhaseDispatchResult dispatch_battle_object_phase(u32 arg2, u32 object)
{
    u32 phase = LD8(object + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80C0924 + phase);
    return run_phase_handler_sub_80C0912(handler, phase, arg2, object);
}

/* r2 = forwarded argument, r5 = battle object -> r5 r8 r9 r10 r11 r12 */
Next sub_80C0912(void) {
    struct PhaseDispatchResult out = dispatch_battle_object_phase(R2, R5);
    R5 = out.object;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── battle_panelObject_tryTargetAndSetPhase_80C0938 ── */
/* battle_panelObject_tryTargetAndSetPhase_80C0938(r5 = battle object pointer):
 * Try to find a panel target via sub_80C0C9A.
 * On success: stash the picked param bytes into the object's Param3/Param4 and set
 *   its phase to 4 (the "execute panel effect" phase).
 * On failure: set its current action to 0xC and its phase to 0 (idle / no target).
 *   r5 is preserved; sub_80C0C9A may clobber nz/c/v and r0..r3. */
Next battle_panelObject_tryTargetAndSetPhase_80C0938(void) {
    GUEST_CALL(sub_80C0C9A);

    if (R0 != 0) {
        ST8(R5 + oBattleObject_Param3, (u8)R0);
        ST8(R5 + oBattleObject_Param4, (u8)R1);
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 4);
    } else {
        ST8(R5 + oBattleObject_CurAction, 0xC);
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80C0956 ── */
/* sub_80C0956(r5 = battle object):
 * Runs the short startup delay at the beginning of an object's phase.
 * On the first call (PhaseInitialized == 0) it plays sound effect 0xBE,
 * switches to animation 2 and arms a 5-frame timer.
 * On later calls it counts the timer down; once it expires, it clears the
 * object's "busy" flag bit and advances to phase 8. */
Next sub_80C0956(void) {
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        /* First entry: start the phase's intro (anim + sound + delay). */
        ST16(R5 + oBattleObject_CurAnimAndCurAnimCopy, 2);
        R0 = 0xBE;
        GUEST_CALL(PlaySoundEffect);
        ST16(R5 + oBattleObject_Timer, 5);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    } else {
        u32 timer = LD16(R5 + oBattleObject_Timer) - 1;
        ST16(R5 + oBattleObject_Timer, timer);
        if ((s32)timer > 0)
            RETURN_TO_CALLER();  /* still waiting */

        /* Timer expired: release the object and move on to phase 8. */
        ST8(R5 + oObjectHeader_Flags, LD8(R5 + oObjectHeader_Flags) & ~2u);
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 8);
    }
    RETURN_TO_CALLER();
}

/* ── battleObject_jumpUpPhase_80C0988 ── */
/* battleObject_jumpUpPhase_80C0988: Z-axis pop-up animation for a battle object.
 *
 * First time through, arms the upward jump (Z velocity 0x0F0000, 4-frame
 * period timer) and stamps the phase-init flag to 4. On later ticks it
 * integrates Z by ZVelocity; once Z has risen past 0xA0 in the high half
 * the object's phase advances to 0xC and we exit. The 4-frame period
 * timer counts down each visit, calling sub_80C0DC2 every 5 frames.
 */
Next battleObject_jumpUpPhase_80C0988(void) {
    if (!LD8(R5 + oBattleObject_PhaseInitialized)) {
        /* First time: launch the object upward and arm the period timer. */
        ST16(R5 + oBattleObject_Timer, 4);
        ST32(R5 + oBattleObject_ZVelocity, 0x0F0000);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    } else {
        /* Integrate Z position from velocity and check landing threshold. */
        s32 z = (s32)LD32(R5 + oBattleObject_Z) + (s32)LD32(R5 + oBattleObject_ZVelocity);
        ST32(R5 + oBattleObject_Z, (u32)z);
        if (((s32)z >> 16) >= 0xA0) {
            /* Landed: advance phase and exit. */
            ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0xC);
            RETURN_TO_CALLER();
        }
    }

    /* Tick down the period timer; call sub_80C0DC2 on roll-over. */
    u32 timer = (u32)((s32)LD16(R5 + oBattleObject_Timer) - 1);
    ST16(R5 + oBattleObject_Timer, (u16)timer);
    if ((s32)timer > 0) {
        RETURN_TO_CALLER();
    }
    ST16(R5 + oBattleObject_Timer, 5);
    GUEST_CALL(sub_80C0DC2);
    RETURN_TO_CALLER();
}

/* ── sub_80C09C6 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2; } call_object_setCoordinatesFromPanels_result_sub_80C09C6;
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline call_object_setCoordinatesFromPanels_result_sub_80C09C6 call_object_setCoordinatesFromPanels_sub_80C09C6(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
    return (call_object_setCoordinatesFromPanels_result_sub_80C09C6){ R1, R2 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_sub_80C0DC2_result;
/* register form: generated wrapper: sub_80C0DC2 has no typed C interface yet */
static inline call_sub_80C0DC2_result call_sub_80C0DC2(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80C0DC2);
    return (call_sub_80C0DC2_result){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Update a bouncing battle object each frame.
 * Manages initialization, vertical movement, ground snap, and phase timer.
 * Returns the (possibly changed) object pointer since sub_80C0DC2 may
 * move the object to a different slot. */
static u32 update_bounce(u32 obj) {
    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        /* First-time bounce setup: timer=1, flip velocity, load panel coords */
        ST16(obj + oBattleObject_Timer, 1);
        ST32(obj + oBattleObject_ZVelocity, 0 - LD32(obj + oBattleObject_ZVelocity));
        ST16(obj + oBattleObject_PanelXY, LD16(obj + oBattleObject_Param3));
        call_object_setCoordinatesFromPanels_sub_80C09C6(obj);
        ST8(obj + oBattleObject_PhaseInitialized, 4);
    } else {
        /* Advance height: Z += ZVelocity */
        u32 z = LD32(obj + oBattleObject_Z);
        u32 z_vel = LD32(obj + oBattleObject_ZVelocity);
        u32 new_z = z + z_vel;
        ST32(obj + oBattleObject_Z, new_z);
        /* ASR by 16 extracts the integer height (signed upper 16 bits) */
        s32 z_high = (s32)new_z >> 16;
        /* BGT uses N==V && Z==0:  N,Z from ASR; V from the ADD */
        bool n = (z_high < 0);
        bool z_is_zero = (z_high == 0);
        bool v_add = ((s32)z >= 0 && (s32)z_vel >= 0 && (s32)new_z < 0) ||
                     ((s32)z < 0 && (s32)z_vel < 0 && (s32)new_z >= 0);
        bool still_above = (n == v_add) && !z_is_zero;

        if (!still_above) {
            /* Landed (or below ground): snap Z, mark phase complete, skip timer */
            ST32(obj + oBattleObject_Z, 0);
            ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0x10);
            return obj;
        }
        /* Still above ground: fall through to timer tick */
    }

    /* Tick the phase timer down */
    u32 timer = LD16(obj + oBattleObject_Timer);
    u32 decremented = timer - 1;
    ST16(obj + oBattleObject_Timer, decremented);
    /* BGT after 32-bit SUBS: equivalent to signed > 0 for 16-bit-range values */
    if ((s32)decremented > 0) {
        return obj;
    }

    /* Timer expired: reset and advance to the next bounce sub-phase */
    ST16(obj + oBattleObject_Timer, 3);
    return call_sub_80C0DC2(obj).r5;
}

/* sub_80C09C6(r5 = object pointer) -> r5 */
Next sub_80C09C6(void) {
    R5 = update_bounce(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C0A0E ── */
/* sub_80C0A0E(r5 = battle object):
 * Phase handler for an object spawning effect 0x26.
 * On the first run (PhaseInitialized == 0): set a 5-frame timer, spawn the
 * effect at the object's position (the word at +0x3C is lifted by 0x140000,
 * which the effect spawner treats as a depth/layer tag), set the object
 * header's flag 0x2 and advance to phase 4.
 * On later runs: count the timer down each frame; when it hits zero, switch
 * the object to action 8 and clear CurPhase/PhaseInitialized. */
Next sub_80C0A0E(void) {
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        /* First entry into this phase: arm the timer and spawn the effect. */
        ST16(R5 + oBattleObject_Timer, 5);

        u32 pos_x = LD32(R5 + 0x34);
        u32 pos_y = LD32(R5 + 0x38);
        u32 pos_z = LD32(R5 + 0x3C) + 0x140000;

        R1 = pos_x;
        R2 = pos_y;
        R3 = pos_z;
        R4 = 0x26; /* effect id */
        GUEST_CALL(spawn_t1_0x0_EffectObject);

        ST8(R5 + oObjectHeader_Flags, LD8(R5 + oObjectHeader_Flags) | 2);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    } else {
        /* Phase already running: wait for the timer to run out. */
        u32 timer = LD16(R5 + oBattleObject_Timer) - 1;
        ST16(R5 + oBattleObject_Timer, timer);
        if ((s32)timer > 0)
            RETURN_TO_CALLER();

        /* Timer expired: move on to action 8 and reset the phase state. */
        ST8(R5 + oBattleObject_CurAction, 8);
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80C0A4C ── */
/* Phase handler dispatch for a battle object: looks up the handler by the
 * object's current phase and tail-calls it. The handler runs with SP four
 * bytes below the caller's SP (one slot below LR's slot) and the saved LR
 * is consumed by the pop, so control returns straight to sub_80C0A4C's
 * caller; the handler's r5/r8-r12 leak back as outputs. */
static u32 phase_handler_addr(u32 phase) {
    return LD32(A_off_80C0A60 + phase);
}

Next sub_80C0A4C(void) {
    u32 phase = LD8(R5 + oBattleObject_CurPhase);
    /* register form: the handler is dispatched through the table at
     * off_80C0A60 with bx, which means it runs with a shifted SP and the
     * guest-link slot populated for the pop {pc} to return to the caller. */
    SP -= 4;
    ST32(SP, LR);
    R0 = phase;
    R1 = phase_handler_addr(phase);
    GUEST_CALL_ADDR(R1);
    SP += 4;
    RETURN_TO_CALLER();
}

/* ── advancePhase3ToPhase4_80C0A6C ── */
/* advancePhase3ToPhase4_80C0A6C — advance the phase-3 → phase-4 transition for a battle object.
 *   inputs:  r5 = battle object pointer
 *   outputs: -
 *   clobbers: nz c v r0
 *
 * First call (PhaseInitialized == 0) starts the transition: anim 3 with a
 * 9-frame timer. Subsequent calls count the timer down; when it reaches
 * zero (signed), the combined phase/state field is set to 4 to land in
 * phase 4.
 */
Next advancePhase3ToPhase4_80C0A6C(void) {
    if (!LD8(R5 + oBattleObject_PhaseInitialized)) {
        /* First entry into the transition: kick off the anim + timer. */
        ST8(R5 + oBattleObject_CurAnim, 3);
        ST16(R5 + oBattleObject_Timer, 9);
        R0 = 4;
        ST8(R5 + oBattleObject_PhaseInitialized, R0);
    } else {
        /* Count the timer down and, once it expires, move to phase 4.
         * The subtraction and signed comparison must be done in 32 bits
         * (mirroring `sub r0,#1` + `bgt` on the full register after the
         * `ldrh` zero-extends into r0); using 16-bit signed math here
         * disagrees for large timer values (e.g. 0xFFFF). */
        u32 timer = (u32)LD16(R5 + oBattleObject_Timer) - 1;
        ST16(R5 + oBattleObject_Timer, (u16)timer);
        if ((s32)timer > 0) {
            /* Still counting — leave the object in this sub-phase. */
            R0 = timer;
        } else {
            /* Timer expired: advance to phase 4. */
            R0 = 4;
            ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_80C0B4A ── */
/* sub_80C0B4A(r5 = battle object):
 * One-tick handler for a battle object's entry phase.
 * First call initializes the phase (CurAnim, ExtraVars, Timer=10, PhaseInitialized=4).
 * Subsequent calls decrement Timer; on expiry, advance CurAction to 0xC and
 * reset the phase so the object can re-initialize. */
Next sub_80C0B4A(void) {
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        /* Phase not yet started: set up initial state */
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST32(R5 + 0x6C, 0);          /* ExtraVars + 0xc */
        ST16(R5 + oBattleObject_Timer, 10);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    } else {
        /* Phase running: count down the timer */
        s32 new_timer = (s32)LD16(R5 + oBattleObject_Timer) - 1;
        ST16(R5 + oBattleObject_Timer, new_timer);
        if (new_timer > 0)
            RETURN_TO_CALLER();
        /* Timer reached zero: switch to next action and clear phase init */
        ST8(R5 + 9, 0xC);  /* CurAction */
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80C0B76 ── */
/* sub_80C0B76: per-phase tick dispatcher for a battle object.
 *
 * Reads the object's CurPhase byte, looks the phase up in the two-entry
 * handler table at off_80C0B88 and tail-calls that handler with
 * r0 = phase, r1 = the handler's own address and r2 = the caller's r2.
 * The original reaches the handler through "mov lr, pc; bx r1" so the
 * handler returns straight back to this function's caller; whatever the
 * handler leaves in r5 and r8-r12 is what the caller sees. */

/* register form: the phase handler is reached through its table address
 * (mov lr, pc; bx r1), which no call_NAME wrapper covers, and it is
 * entered with r0 = phase, r1 = its own address, r2 = the caller's r2. */
static void call_phase_handler_sub_80C0B76(u32 phase, u32 handler, u32 arg2) {
    R0 = phase;
    R1 = handler;
    R2 = arg2;
    GUEST_CALL_ADDR(handler);
}

/* Run the battle object's phase handler. The handler's register results
 * (r5, r8-r12) stay in the guest register file, so nothing is returned. */
static void run_phase_handler_sub_80C0B76(u32 objPtr, u32 arg2) {
    u32 phase = LD8(objPtr + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80C0B88 + phase);
    call_phase_handler_sub_80C0B76(phase, handler, arg2);
}

Next sub_80C0B76(void) {
    run_phase_handler_sub_80C0B76(R5, R2);
    RETURN_TO_CALLER();
}

/* ── battleObject_tickHitReact_80C0B90 ── */
/* battleObject_tickHitReact_80C0B90: per-frame update for a battle object's freeze/hit-react phase.
 *
 * PhaseInit byte acts as a tiny state machine:
 *   0  -> first tick: kick off the reaction anim, play its SFX (0xBE), arm a
 *         5-frame timer, then mark state = 4 so we don't re-enter.
 *   !=0-> count the timer down each tick. When it reaches 0, clear flag 0x02
 *         in the object's header flags and advance CurPhase to 4. */
Next battleObject_tickHitReact_80C0B90(void) {
    u8 phaseInit = LD8(R5 + oBattleObject_PhaseInitialized);

    if (phaseInit == 0) {
        /* First entry: trigger the reaction. */
        ST16(R5 + oBattleObject_CurAnimAndCurAnimCopy, 2);
        R0 = 0xBE;
        GUEST_CALL(PlaySoundEffect);
        ST16(R5 + oBattleObject_Timer, 5);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
        SET_FLAGS_NZ(4);
    } else {
        /* Subsequent ticks: decrement the 16-bit timer (32-bit SUB on the
         * loaded value so the high bits match the original's flags). */
        u32 timerVal = LD16(R5 + oBattleObject_Timer);
        u32 newVal = timerVal - 1;
        SET_FLAGS_NZ(newVal);
        ST16(R5 + oBattleObject_Timer, (u16)newVal);

        /* BGT (signed > 0): taken while the timer still has frames left.
         * For the subtraction here V is always 0, so this reduces to N==0
         * and Z==0, i.e. the result is a positive nonzero 32-bit value. */
        if (NEGATIVE_FLAG() || ZERO_FLAG()) {
            /* Timer expired: clear the busy flag and hand off to phase 4. */
            u8 flags = LD8(R5 + oObjectHeader_Flags);
            flags &= (u8)~2;
            ST8(R5 + oObjectHeader_Flags, flags);
            ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 4);
            SET_FLAGS_NZ(4);
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_80C0C04 ── */
/* sub_80C0C04: phase dispatcher for a battle object.
 *
 * Reads the object's current phase (r5 = object pointer) and tail-calls the
 * matching handler from the two-entry table off_80C0C18:
 *   phase 0 -> battleObject_tickPhase3_80C0C20
 *   phase 1 -> sub_80C0C48
 * The handlers receive the phase in r0 and all other arguments untouched;
 * whatever they leave in r5/r8-r12 is passed straight through to our caller.
 */
Next sub_80C0C04(void) {
    u32 phase = LD8(R5 + oBattleObject_CurPhase);

    R0 = phase; /* the original loads the phase into r0 before jumping */

    switch (phase) {
    case 0:
        GUEST_CALL(battleObject_tickPhase3_80C0C20);
        break;
    default:
        GUEST_CALL(sub_80C0C48);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battleObject_tickPhase3_80C0C20 ── */
/* battleObject_tickPhase3_80C0C20: Battle-object phase-3 init/tick.
 *   inputs:  r5 (battle object), lr
 *   outputs: r5 (preserved)
 *   clobbers: r0, r1, r2, r3, lr
 *
 * On first entry (phase != 4): set timer = 10 and mark phase initialized (=4).
 * Otherwise: decrement timer; when it expires, reset the linked object via
 * sub_80E1332(related_object_1, 0) and advance phase to 4. */
Next battleObject_tickPhase3_80C0C20(void) {
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        ST16(R5 + oBattleObject_Timer, (u16)0xA);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    } else {
        u32 timer = LD16(R5 + oBattleObject_Timer) - 1;
        ST16(R5 + oBattleObject_Timer, (u16)timer);
        if ((s32)timer <= 0) {
            R0 = LD32(R5 + oBattleObject_RelatedObject1Ptr);
            R1 = 0;
            GUEST_CALL(sub_80E1332);
            ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, (u16)4);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_80C0C48 ── */
/* sub_80C0C48(r5 = battle object):
 * First-time init phase of a battle object: derive a parameter from the
 * object's damage, pass it together with its Unk_0c/Unk_0d bytes to
 * sub_80CE224 on the related object, then arm a 90-frame timer and mark
 * the phase as initialized (state 4). On subsequent runs the timer counts
 * down; when it runs out the object moves to the destroy state.
 * r6 is only used as scratch for the call arguments and is restored. */
Next sub_80C0C48(void) {
    u32 obj = R5;
    u32 savedR6 = R6;

    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        /* First run: set the object up, then start the countdown. */
        R0 = LD16(obj + oBattleObject_Damage);
        GUEST_CALL(sub_80E199A);
        u32 derived = R0;

        R0 = LD8(obj + oBattleObject_Unk_0c);
        R1 = LD8(obj + oBattleObject_Unk_0d);
        R5 = LD32(obj + oBattleObject_RelatedObject1Ptr);
        R6 = derived;
        GUEST_CALL(sub_80CE224);
        R5 = obj;
        R6 = savedR6;

        ST16(obj + oBattleObject_Timer, 90);
        ST8(obj + oBattleObject_PhaseInitialized, 4);
    } else {
        /* Countdown running: tick the timer, destroy when it expires. */
        u32 timer = LD16(obj + oBattleObject_Timer);
        ST16(obj + oBattleObject_Timer, timer - 1);
        if ((s32)timer <= 1)
            ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80C0C8C ── */
/* sub_80C0C8C(r5 = battle object):
 *   clears a flag at object->ExtraVars.byte4 then frees the object's memory.
 *   Preserves r5 across the free call so the caller can keep using it. */
Next sub_80C0C8C(void) {
    u8 *extra = (u8 *)LD32(R5 + oBattleObject_ExtraVars + 4);

    ST8((u32)extra, 0);
    GUEST_CALL(object_freeMemory);

    RETURN_TO_CALLER();
}

/* ── sub_80C0D98 ── */
/* sub_80C0D98: Spawn a dimming collision region for a battle object.
 * r0, r1 = base XY / extra args (passed through)
 * r5     = battle object pointer (preserved across the call)
 * Spawns a "dimming" collision region (type 3) using the object's damage/stamina
 * damage counter value as its region index. */
Next sub_80C0D98(void) {
    /* Region type 3 = dimming; Z = 0; world Y offset 0x14.0 */
    R2 = 0;
    R3 = 0x14 << 16;
    /* Sprite tile + palette attribute for the dimming region. */
    R4 = 0x04050001;
    /* Use the object's damage/stamina counter value as the collision region ID. */
    R6 = LD32(R5 + oBattleObject_DamageAndStaminaDamageCounterDisabler);
    R7 = 3;
    GUEST_CALL(object_spawnDimmingCollisionRegion);
    RETURN_TO_CALLER();
}

/* ── sub_80C0DB0 ── */
Next sub_80C0DB0(void) {
    /* Apply the colour shader to the battle object sprite. The shader index
     * is computed from the object's Unk_19 field as: idx * 0x421 (i.e.
     * (idx << 5 + idx) << 5 + idx), selecting one of a packed table. */
    u32 idx = LD8(R5 + oBattleObject_Unk_19);
    R0 = ((idx << 5) + idx) << 5;
    R0 = R0 + idx;
    GUEST_CALL(sprite_setColorShader);
    RETURN_TO_CALLER();
}

/* ── sub_80C0DC2 ── */
/* sub_80C0DC2: apply small random variance to the battle object's position
 * (X/Y) and spawn a t1/0x0 effect object for it.
 *   inputs:  r5 lr
 *   preserved: r5 r6 r7
 */
Next sub_80C0DC2(void) {
    R0 = 0x10;
    R1 = LD32(R5 + oBattleObject_X);
    R2 = LD32(R5 + oBattleObject_Y);
    R3 = LD32(R5 + oBattleObject_Z);
    GUEST_CALL(AddRandomVarianceToTwoCoords);

    R4 = 0x26;
    GUEST_CALL(spawn_t1_0x0_EffectObject);

    RETURN_TO_CALLER();
}

/* ── sub_80C0DD8 ── */
/* sub_80C0DD8: Spawn a type-0x2C battle object (fx / projectile) and configure it
 * with the given panel position, linking it back to the source object.
 *   r0 = panel X coordinate
 *   r1 = panel Y coordinate
 *   r5 = source BattleObject (provides alliance/direction, becomes RelatedObject1)
 *   r6 = damage-counter-disabler value stored in the new object
 *   r7 = pointer whose first byte gets marked active (also stored in ExtraVars+4)
 *
 * Returns with r0/r1/r2 and flags as free scratch; r3–r7 preserved. */
Next sub_80C0DD8(void) {
    /* object_spawnType1 clobbers r3 and r5; save what we need. */
    u32 saved_r3   = R3;
    u32 source_obj = R5;   /* the object this fx was spawned from */
    u32 panel_x    = R0;   /* X coordinate in tile units           */
    u32 panel_y    = R1;   /* Y coordinate in tile units           */

    /* Spawn object type 0x2C; result pointer lands in r5. */
    R0 = 0x2C;
    GUEST_CALL(object_spawnType1);

    R3 = saved_r3;  /* restore preserved register */

    u32 new_obj = R5;  /* freshly spawned BattleObject, or 0 on failure */
    R5 = source_obj;   /* r5 is a preserved output – put it back */

    if (new_obj == 0) {
        /* Spawn failed (pool full); nothing to configure. */
        RETURN_TO_CALLER();
    }

    /* Write panel coordinates into both the canonical and mirror fields. */
    ST8(new_obj + oBattleObject_PanelX, panel_x);
    ST8(new_obj + oBattleObject_Unk_0c, panel_x);
    ST8(new_obj + oBattleObject_PanelY, panel_y);
    ST8(new_obj + oBattleObject_Unk_0d, panel_y);

    /* Inherit alliance and facing direction from the source object. */
    ST16(new_obj + oBattleObject_AllianceAndDirectionFlip,
         LD16(source_obj + oBattleObject_AllianceAndDirectionFlip));

    /* Link metadata: disabler value, related object back-pointer,
     * and the "active" flag byte address in the extra-variable slot. */
    ST32(new_obj + oBattleObject_DamageAndStaminaDamageCounterDisabler, R6);
    ST32(new_obj + oBattleObject_RelatedObject1Ptr, source_obj);
    ST32(new_obj + oBattleObject_ExtraVars + 4, R7);

    /* Mark the referenced byte (e.g. an owner's "has-fx" flag) as active. */
    ST8(R7, 1);

    RETURN_TO_CALLER();
}

/* ── t1_0x2d_80C0E04 ── */
/* t1_0x2d_80C0E04 — battle object state dispatcher.
 *
 * r5 holds a battle object. The object's current-state byte selects a
 * handler from the table at off_80C0E18:
 *   state 0 -> sub_80C0E24
 *   state 1 -> sub_80C0EE2
 *   state 2 -> object_freeMemory
 * The handler is entered with the state byte in r0 and the table entry
 * (the handler's own address) in r1, exactly as the original's
 * "ldr r1, [r1,r0]; mov lr, pc; bx r1" sequence leaves them. The caller's
 * r5-r12 stay live throughout, so whatever the handler leaves in
 * r5/r8-r12 is this function's result; those registers are never touched
 * by any C code here and simply pass through the guest register file.
 */

/* register form: the handler is reached through a code address read from
 * the state table (the original's "mov lr, pc; bx r1" idiom), so the call
 * must go through GUEST_CALL_ADDR. The handler must also see the caller's
 * live r5-r12, which no typed parameter passing can express — they are
 * left untouched in the guest registers and read back by the callee. */
static void run_state_handler(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80C0E18 + state);

    R0 = state;      /* handler argument: the object's current state */
    R1 = handler;    /* the original leaves the table entry in r1 as well */
    GUEST_CALL_ADDR(handler);
}

Next t1_0x2d_80C0E04(void) {
    run_state_handler(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C0E24 ── */
/* sub_80C0E24(r5 = battle object)
 * If ExtraVars resolves to 2 (real Navi), set up the sprite/anim for that battle
 * object using RelatedObject1's stats; otherwise just mark the state and advance. */
Next sub_80C0E24(void) {
    u32 saved_r4 = R4;

    /* RelatedObject1 holds the underlying BattleNavi stats; read its NameID and
     * decode the version/actor/AI index, then stash the result in ExtraVars. */
    u32 related1 = LD32(R5 + oBattleObject_RelatedObject1Ptr);
    R4 = related1;
    R0 = LD16(R4 + oBattleObject_NameID);
    GUEST_CALL(unpackVerActorTyAndAIIdx_800F29C);
    ST32(R5 + oBattleObject_ExtraVars, R0);

    /* Only Navi objects (ExtraVars == 2) get the full sprite/animation treatment. */
    if (R0 == 2) {
        /* chip_class_lookup_800FC9E(r0 = stat_0x29, r1 = stat_0x2C = alliance).
         * The original saves alliance across the second stats read via push/pop. */
        u32 prev_r5 = R5;
        R5 = related1;
        R1 = 0x2C;
        GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
        u32 alliance = R0;
        R1 = 0x29;
        GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
        R1 = alliance;            /* restore the saved stat_0x2C into r1; r0 keeps stat_0x29 */
        R5 = prev_r5;
        GUEST_CALL(chip_class_lookup_800FC9E);

        /* Sprite id 0x80, then load its animation data and apply a shadow. */
        R0 = 0x80;
        GUEST_CALL(sprite_load);
        GUEST_CALL(sprite_loadAnimationData);
        GUEST_CALL(sprite_hasShadow);

        /* Set the HURT flag (bit 1) on this object's header. */
        {
            u8 flags = LD8(R5 + oObjectHeader_Flags);
            flags |= 0x02;
            ST8(R5 + oObjectHeader_Flags, flags);
        }

        /* Copy 12 bytes from RelatedObject1 + 0x34 into this object + 0x34. */
        {
            u32 src = LD32(R5 + oBattleObject_RelatedObject1Ptr) + 0x34;
            u32 v1 = LD32(src);
            u32 v2 = LD32(src + 4);
            u32 v3 = LD32(src + 8);
            u32 dst = R5 + 0x34;
            ST32(dst,     v1);
            ST32(dst + 4, v2);
            ST32(dst + 8, v3);
        }

        /* Run the sprite init on RelatedObject1, then apply its palette to this one. */
        {
            u32 prev_r5 = R5;
            R5 = related1;
            GUEST_CALL(battle_pickPaletteIndexForNavi_801002C);
            R5 = prev_r5;
            GUEST_CALL(sprite_setPalette);
        }

        /* Real-Navi follow-ups, only if ExtraVars is still 2 (re-read, matches asm). */
        if (LD32(R5 + oBattleObject_ExtraVars) == 2) {
            /* sub_8011420(r0 = stat_0x29, r1 = alliance, r2 = 1). Same push/pop dance. */
            {
                u32 prev_r5 = R5;
                R5 = related1;
                R1 = 0x2C;
                GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
                u32 alliance2 = R0;
                R1 = 0x29;
                GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
                R1 = alliance2;
                R5 = prev_r5;
                R2 = 1;
                GUEST_CALL(sub_8011420);
            }

            /* If stat_0x2C (alliance) != 0 and RelatedObject2 exists, mark it as a
             * follow-up sprite (Param3 = 1, OR 0x14 into header flags). */
            {
                u32 prev_r5 = R5;
                R5 = related1;
                R1 = 0x2C;
                GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
                R5 = prev_r5;
                if (R0 != 0) {
                    u32 related2 = LD32(R5 + oBattleObject_RelatedObject2Ptr);
                    if (related2 != 0) {
                        ST8(related2 + oBattleObject_Param3, 1);
                        u8 r2_flags = LD8(related2 + oObjectHeader_Flags);
                        r2_flags |= 0x14;
                        ST8(related2 + oObjectHeader_Flags, r2_flags);
                    }
                }
            }
        }

        /* Sync the sprite flip to the object's facing, then pick the initial animation
         * (3 + Param4) and force an anim refresh (0xFF marker). */
        GUEST_CALL(object_getFlip);
        GUEST_CALL(sprite_setFlip);
        {
            u8 anim = LD8(R5 + oBattleObject_Param4) + 3;
            ST8(R5 + oBattleObject_CurAnim, anim);
            ST8(R5 + oBattleObject_CurAnimCopy, 0xFF);
        }
    }

    /* Advance to the next phase of this update-action's state machine. */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
    GUEST_CALL(sub_80C0EE2);

    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_80C0EE2 ── */
/* sub_80C0EE2 — battle object sub-state: on first entry sets up a 4-frame timer
 * (and pokes a helper on RelatedObject1 when Param4 != 0); once the timer
 * elapses, clears bit 1 of header flags, fires an effect tied to the related
 * object's NameID, optionally tears down the related object, and queues this
 * object for destruction.
 *
 * inputs:  R5 (this), R10, LR
 * outputs: R5, R8, R9, R10, R11, R12
 * clobbers: nz, c, v, R0-R3, R6, R7, LR
 * preserved: R4
 */
Next sub_80C0EE2(void) {
    /* First-pass init: runs only when PhaseInitialized == 0. */
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        if (LD8(R5 + oBattleObject_Param4) != 0) {
            u32 related = LD32(R5 + oBattleObject_RelatedObject1Ptr);
            u32 extra   = LD32(R5 + oBattleObject_ExtraVars);
            if (extra == 2) {
                R0 = related;
                R1 = 0;
                GUEST_CALL(sub_80E1352);
            } else {
                R0 = related;
                GUEST_CALL(sub_80E146C);
            }
        }
        ST16(R5 + oBattleObject_Timer, 4);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    }

    /* Tick the timer down; preserve the original's SUBS-style flags
     * (the bgt below branches on the signed-compare result). */
    u32 timer = LD16(R5 + oBattleObject_Timer);
    SET_FLAGS_CMP(timer, 1);   /* SUBS timer, #1 → compare (timer - 1) vs 0 */
    timer -= 1;
    ST16(R5 + oBattleObject_Timer, (u16)timer);

    /* Branch on the signed-compare flags: bgt → timer > 0 keeps going. */
    if (!NEGATIVE_FLAG() && !ZERO_FLAG()) {
        /* Timer still positive: just run the per-frame sprite/timestop update. */
        GUEST_CALL(object_updateSpriteTimestop);
        RETURN_TO_CALLER();
    }

    /* Timer elapsed: clear bit 1 of the object's header flags. */
    ST8(R5 + oObjectHeader_Flags, LD8(R5 + oObjectHeader_Flags) & ~(u8)2);

    u32 related = LD32(R5 + oBattleObject_RelatedObject1Ptr);

    /* Trigger an effect based on the related object's NameID. */
    R0 = LD16(related + oBattleObject_NameID);
    GUEST_CALL(unpackVerActorTyAndAIIdx_800F29C);
    R2 = 1;
    GUEST_CALL(sub_8011044);

    /* If Param4 == 0, also poke the teardown helper on RelatedObject1. */
    if (LD8(R5 + oBattleObject_Param4) == 0) {
        u32 extra = LD32(R5 + oBattleObject_ExtraVars);
        if (extra == 2) {
            R0 = related;
            GUEST_CALL(sub_80E13DC);
        } else {
            R0 = related;
            GUEST_CALL(sub_80E14AC);
        }
    }

    /* Queue this object for destruction. */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);

    GUEST_CALL(object_updateSpriteTimestop);
    RETURN_TO_CALLER();
}

/* ── battleObj_initSprite_80C3F00 ── */
/* battleObj_initSprite_80C3F00: initialize a battle object's sprite (panels, sprite, anim, palette, flip),
 * set the state phase to CUR_STATE_UPDATE_sub_80C3F00, then tail-call t1_0x54_updateFromRelatedObject_80C3F52. */

#define CUR_STATE_UPDATE_sub_80C3F00 4

/* register form: calls object_setPanelsFromCoordinates outside this file, which has no typed C interface yet */
static void call_object_setPanelsFromCoordinates(void) {
    GUEST_CALL(object_setPanelsFromCoordinates);
}

/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void call_sprite_load_sub_80C3F00(u32 bank, u32 a1, u32 a2) {
    R0 = bank;
    R1 = a1;
    R2 = a2;
    GUEST_CALL(sprite_load);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_sprite_loadAnimationData_sub_80C3F00(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_sprite_noShadow_sub_80C3F00(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation_sub_80C3F00(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_getPalette outside this file, which has no typed C interface yet */
static u32 call_sprite_getPalette(u32 obj) {
    R0 = obj;
    GUEST_CALL(sprite_getPalette);
    return R0;
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_sprite_setPalette_sub_80C3F00(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls object_getFlip outside this file, which has no typed C interface yet */
static u32 call_object_getFlip(void) {
    GUEST_CALL(object_getFlip);
    return R0;
}

/* register form: calls sprite_setFlip outside this file, which has no typed C interface yet */
static void call_sprite_setFlip(u32 flip) {
    R0 = flip;
    GUEST_CALL(sprite_setFlip);
}

/* register form: calls t1_0x54_updateFromRelatedObject_80C3F52 outside this file, which has no typed C interface yet.
 * The original ends with `pop {r4-r7, pc}` whose loaded PC value is the SAVED lr pushed at
 * entry (the caller's lr, not a within-function address). We model that: this function is
 * tail-called by the adapter, which supplies the correct LR. */
/* leftover unused: t1_0x54_updateFromRelatedObject_80C3F52's r0 (battleObj_initSprite_80C3F00's callers read only r5) */
static u32 call_sub_80C3F52(void) {
    GUEST_CALL(t1_0x54_updateFromRelatedObject_80C3F52);
    return R0;
}

/* Typed implementation. The original pops the caller-saved LR pushed at entry and jumps to it,
 * so the visible outputs are whatever R0..R7 end up as after t1_0x54_updateFromRelatedObject_80C3F52, plus the saved R4..R7
 * and LR restored from the stack frame. Since the helpers above already preserve R4..R7 and we
 * never touch them in typed code, only R0 matters here as a flag/source. */
static u32 sub_80C3F00_typed(u32 obj_ptr) {
    u32 obj = obj_ptr;

    call_object_setPanelsFromCoordinates();

    /* sprite_load(0x80, Param1, Param2) */
    call_sprite_load_sub_80C3F00(0x80, LD8(obj + oBattleObject_Param1), LD8(obj + oBattleObject_Param2));

    call_sprite_loadAnimationData_sub_80C3F00();
    call_sprite_noShadow_sub_80C3F00();

    /* OR 0x02 into object header flags. */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Animation id from ExtraVars[4]; store as current anim and call sprite_setAnimation. */
    {
        u32 anim = LD32(obj + oBattleObject_ExtraVars + 4);
        ST8(obj + oBattleObject_CurAnim, anim);
        ST8(obj + oBattleObject_CurAnimCopy, anim);
        call_sprite_setAnimation_sub_80C3F00((u8)anim);
    }

    call_sprite_loadAnimationData_sub_80C3F00();

    /* Palette: default to Param4; if Param3 == 0, inherit from RelatedObject1Ptr's palette. */
    {
        u32 param3 = LD8(obj + oBattleObject_Param3);
        u32 palette;
        if (param3 == 0) {
            u32 related = LD32(obj + oBattleObject_RelatedObject1Ptr);
            palette = call_sprite_getPalette(related);
        } else {
            palette = LD8(obj + oBattleObject_Param4);
        }
        call_sprite_setPalette_sub_80C3F00(palette);
    }

    call_sprite_setFlip(call_object_getFlip());

    /* Mark the object as being in the update phase, then tail-call t1_0x54_updateFromRelatedObject_80C3F52. */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE_sub_80C3F00);

    return call_sub_80C3F52();
}

Next battleObj_initSprite_80C3F00(void) {
    u32 r0 = sub_80C3F00_typed(R5);
    SET_FLAGS_NZ(r0);
    R0 = r0;
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

/* ── battleObject_setupSprite_80C4368 ── */
/* CUR_STATE_UPDATE_sub_80C4368 is the value used by the original when priming the state machine. */
#define CUR_STATE_UPDATE_sub_80C4368 4

/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void sprite_load_guest(u8 sprite_id, u8 tile_id, u8 anim_id) {
    R0 = sprite_id;
    R1 = tile_id;
    R2 = anim_id;
    GUEST_CALL(sprite_load);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_loadAnimationData_guest(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_noShadow_guest(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_getPalette outside this file, which has no typed C interface yet */
static u32 sprite_getPalette_guest(u32 related_object_ptr) {
    R0 = related_object_ptr;
    GUEST_CALL(sprite_getPalette);
    return R0;
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void sprite_setPalette_guest(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_setAnimation_guest(u8 anim_id) {
    R0 = anim_id;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static void sprite_update_guest(void) {
    GUEST_CALL(sprite_update);
}

/* register form: calls battleObject_syncSpriteWithRelatedObject_80C43C4 outside this file, which has no typed C interface yet */
static void sub_80C43C4_guest(void) {
    GUEST_CALL(battleObject_syncSpriteWithRelatedObject_80C43C4);
}

/* Palette pass-through: if the object has no partner (Param2 == 0), use the related
 * object's pointer as the palette; otherwise use palette 0 (the original's `mov r0, #0`). */
static u32 pick_palette(u32 battle_object_ptr) {
    if (LD8(battle_object_ptr + oBattleObject_Param2) == 0)
        return LD32(battle_object_ptr + oBattleObject_RelatedObject1Ptr);
    return 0;
}

/* Set up a battle object's sprite: collision data, sprite/animation data, palette,
 * initial animation, then hand off to battleObject_syncSpriteWithRelatedObject_80C43C4 to finish the state init. */
static void battle_object_sprite_setup(u32 battle_object_ptr) {
    u8 param1 = LD8(battle_object_ptr + oBattleObject_Param1);

    /* Collision data table indexed by Param1. */
    ST32(battle_object_ptr + oBattleObject_CollisionDataPtr,
         LD32(A_off_80C42D4 + ((u32)param1 << 2)));

    /* Sprite tile/anim ids from the per-Param1 entry table. */
    u32 sprite_table_entry = A_byte_80C4320 + (u32)param1 * 2;
    u8 tile_id  = LD8(sprite_table_entry);
    u8 anim_raw = LD8(sprite_table_entry + 1);

    sprite_load_guest(0x80, tile_id, anim_raw);
    sprite_loadAnimationData_guest();
    sprite_noShadow_guest();

    /* Pick a palette: 0 by default, or the related object's pointer when Param2 == 0. */
    u32 palette = pick_palette(battle_object_ptr);
    if (LD8(battle_object_ptr + oBattleObject_Param2) == 0)
        palette = sprite_getPalette_guest(palette);
    sprite_setPalette_guest(palette);

    /* Kick off the requested initial animation. */
    u8 initial_anim = LD8(battle_object_ptr + oBattleObject_Param4);
    ST16(battle_object_ptr + oBattleObject_CurAnimAndCurAnimCopy, initial_anim);
    sprite_setAnimation_guest(initial_anim);
    sprite_loadAnimationData_guest();
    sprite_update_guest();

    ST32(battle_object_ptr + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
         CUR_STATE_UPDATE_sub_80C4368);
    sub_80C43C4_guest();
}

Next battleObject_setupSprite_80C4368(void) {
    battle_object_sprite_setup(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_updateSprite_80C4484 ── */
/* battleObject_updateSprite_80C4484: update a battle object's sprite each frame.
 *
 * Reads the "related object" pointer from the battle object (r5) and runs the
 * standard mosaic-scaling + alpha setup on that sprite. Then dispatches by
 * the Param3 byte of the battle object:
 *   - Param3 != 0   -> call battleObject_updateSpriteAnimIfChanged_801BCD0 (alternate update path).
 *   - Param3 == 0   -> if the battle isn't currently time-stopped, run the
 *                      generic object_updateSprite.
 *
 * Inputs  (registers): r4 r5 r7 r10 lr
 * Outputs (registers): nz c v r0 r1 r2 r3 r4 r5 r7 lr
 * Preserved: r4 r5 r7 (the original never modifies them, so the adapter
 * leaves them as-is).
 */

/* register form: calls sprite_getMosaicScalingParameters outside this file, which has no typed C interface yet */
static void get_mosaic_scaling(u32 related_obj_ptr) {
    R0 = related_obj_ptr;
    GUEST_CALL(sprite_getMosaicScalingParameters);
}

/* register form: calls sprite_setAlpha outside this file, which has no typed C interface yet */
static void set_sprite_alpha(void) {
    GUEST_CALL(sprite_setAlpha);
}

/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet */
static bool battle_is_time_stopped(void) {
    GUEST_CALL(battle_isTimeStop);
    return !ZERO_FLAG();          /* Z set -> not time-stopped; Z clear -> time-stopped */
}

/* register form: calls object_updateSprite outside this file, which has no typed C interface yet */
static void update_sprite(void) {
    GUEST_CALL(object_updateSprite);
}

/* register form: calls battleObject_updateSpriteAnimIfChanged_801BCD0 outside this file, which has no typed C interface yet */
static void alt_object_update(void) {
    GUEST_CALL(battleObject_updateSpriteAnimIfChanged_801BCD0);
}

/* Typed implementation. Performs the same work the original does, with the
 * same memory writes and the same control flow. */
static void sub_80C4484_impl(u32 battle_obj_ptr) {
    /* Pull the related-object pointer out of the battle object and run the
     * standard sprite setup (mosaic scaling parameters, then alpha). */
    get_mosaic_scaling(LD32(battle_obj_ptr + oBattleObject_RelatedObject1Ptr));
    set_sprite_alpha();

    /* Param3 selects which update path to take. */
    if (LD8(battle_obj_ptr + oBattleObject_Param3) != 0) {
        alt_object_update();
    } else if (!battle_is_time_stopped()) {
        update_sprite();
    }
}

/* Thin adapter: pulls the one argument (r5) from the guest register file,
 * calls the typed function, and returns. r4/r7/r10/lr are not touched, so
 * they round-trip unchanged, as does r5 (the original never modifies it). */
Next battleObject_updateSprite_80C4484(void) {
    sub_80C4484_impl(R5);
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
 * current-animation slot and call battleObject_updateSpriteAnimIfChanged_801BCD0 to refresh its animation
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
    GUEST_CALL(battleObject_updateSpriteAnimIfChanged_801BCD0);
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
    case PINKRING_STATE_RUN:  GUEST_CALL(battleObject_refreshAndMirrorRelated_80C4B84);       break;
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
 * per-frame update hook (battleObject_refreshAndMirrorRelated_80C4B84).
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
    GUEST_CALL(battleObject_decideDeathEffectKind_80C4C52);
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
    GUEST_CALL(battleObject_refreshAndMirrorRelated_80C4B84);

    /* pop {r4, pc}: restore the caller's r4. */
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── battleObject_refreshAndMirrorRelated_80C4B84 ── */
/* battleObject_refreshAndMirrorRelated_80C4B84: refresh a battle object – update its animation, alliance gate,
 * mirror related-object state (extra vars, direction/sprite flip), and
 * handle emotion data before freeing memory. */

#define ANIM_ACTIVE     0x02u
#define ANIM_CLEAR_MASK 0xFDu  /* ~ANIM_ACTIVE */
#define REL_FLAG_4      0x04u
#define EXTRA_VARS_OFF  0x34u

/* register form: calls battleObject_decideDeathEffectKind_80C4C52 outside this file, which has no typed C interface yet */
static u32 get_new_anim(u32 obj) {
    R5 = obj;
    GUEST_CALL(battleObject_decideDeathEffectKind_80C4C52);
    return R1;
}

/* register form: calls battle_findPlayerByAlliance_800EB6C outside this file, which has no typed C interface yet */
static u32 check_alliance_valid(u32 alliance) {
    R0 = alliance;
    GUEST_CALL(battle_findPlayerByAlliance_800EB6C);
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

/* battleObject_refreshAndMirrorRelated_80C4B84(r5 = battle object) -> r4 = related object; r0–r3/flags from last callee */
Next battleObject_refreshAndMirrorRelated_80C4B84(void) {
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
/* register form: calls battleObject_initAttackFromDefTable_80C4E7C outside this file, which has no typed C interface yet */
static void call_idle_handler(u32 state, u32 object_ptr) {
    R0 = state;
    R5 = object_ptr;
    GUEST_CALL(battleObject_initAttackFromDefTable_80C4E7C);
}

/* register form: calls battle_object_collisionPhase_80C4F02 outside this file, which has no typed C interface yet */
static void call_active_handler(u32 state, u32 object_ptr) {
    R0 = state;
    R5 = object_ptr;
    GUEST_CALL(battle_object_collisionPhase_80C4F02);
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

/* ── battle_object_collisionPhase_80C4F02 ── */
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

/* battle_object_collisionPhase_80C4F02(r5 = obj) -> r0, r1, r2, r3, nz, c, v */
Next battle_object_collisionPhase_80C4F02(void) {
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

/* ── object_spawnDimmingCollisionRegion ── */
/* object_spawnDimmingCollisionRegion(r0-r3, r4, r6, r7 = spawn arguments):
 * Spawn a collision region via object_spawnCollisionRegion; if one was created,
 * set OBJECT_FLAG_UPDATE_DURING_TIMESTOP in its flags so it keeps animating
 * while time is stopped. Leaves the spawned object pointer (or NULL) in r0.
 * Flags: CMP r0,#0 runs on every path (so carry is always set); on the
 * flag-update path, ORR refreshes N/Z from the stored value. */
Next object_spawnDimmingCollisionRegion(void) {
    GUEST_CALL(object_spawnCollisionRegion);

    u32 object = R0;
    SET_FLAGS_CMP(object, 0);

    if (object != 0) {
        u32 flags = LD8(object + oObjectHeader_Flags) | OBJECT_FLAG_UPDATE_DURING_TIMESTOP;
        SET_FLAGS_NZ(flags);
        ST8(object + oObjectHeader_Flags, flags);
        R1 = flags;
        R2 = OBJECT_FLAG_UPDATE_DURING_TIMESTOP;
    }

    RETURN_TO_CALLER();
}

/* ── t3_0x4_80C53C0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result call_object_updateSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: replicate `mov lr, pc; bx r1` through the
 * jump-table entry at `addr`. The handler may advance the object pointer. */
static u32 dispatch_state_handler(u32 addr, u32 state, u32 obj) {
    /* register form: jump-table GUEST_CALL_ADDR dispatch */
    R0 = state;
    R5 = obj;
    GUEST_CALL_ADDR(addr);
    return R5;
}

/* One frame of battle-object logic: run the per-state handler from
 * the small jump table, then refresh the object's sprite. */
static u32 battle_object_update(u32 obj) {
    u32 state   = LD8(obj + oBattleObject_CurState);
    u32 handler = LD32(A_off_80C53D8 + state);
    obj = dispatch_state_handler(handler, state, obj);
    call_object_updateSprite_result res = call_object_updateSprite(obj);
    return res.r5;
}

/* t3_0x4_80C53C0 (r5 = battle object) -> r5 (+ r8–r12 via callees) */
Next t3_0x4_80C53C0(void) {
    R5 = battle_object_update(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C53E4 ── */
/* sub_80C53E4(r5 = BattleObject*, lr):
 * On object destruction, if the object's Param3 flag is set, crack the panel
 * under it (X,Y) before running the generic destruction routine.
 */
Next sub_80C53E4(void) {
    /* push {lr} */ {
        u32 saved_lr = LR;
        SP -= 4;
        ST32(SP, saved_lr);

        if (LD8(R5 + oBattleObject_Param3) != 0) {
            R0 = LD8(R5 + oBattleObject_PanelX);
            R1 = LD8(R5 + oBattleObject_PanelY);
            GUEST_CALL(object_crackPanel);
        }

        GUEST_CALL(object_genericDestroy);

        LR = LD32(SP);
        SP += 4;
        RETURN_TO_CALLER();
    }
}

/* ── sub_80C54E0 ── */
/* sub_80C54E0 — collision bookkeeping step for the battle object in R5.
 *
 * Drops the object's previous collision data, spawns its hit effect, then
 * checks whether the battle has ended:
 *   - battle over: clear the collision region and move the object into the
 *     destroy state (CUR_STATE_DESTROY), done.
 *   - otherwise: run the object's per-action collision handler, selected by
 *     CurAction through the table at off_80C551C (this table has a single
 *     entry, battleObject_snakeArmUpdate_80C552A), then present the current collision data and
 *     highlight the panel the object is standing on. */
Next sub_80C54E0(void) {
    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);
    GUEST_CALL(battle_isBattleOver);

    if (R0 != 0) {
        GUEST_CALL(object_clearCollisionRegion);
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
        RETURN_TO_CALLER();
    }

    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    if (LD32(collisionData + oCollisionData_FlagsFromCollision) != 0)
        GUEST_CALL(object_clearCollisionRegion);

    /* Dispatch the CurAction handler through the one-entry table; the
     * handler sees R0 = CurAction and R1 = the table entry itself. */
    R0 = LD8(R5 + oBattleObject_CurAction);
    R1 = LD32(A_off_80C551C + R0);
    GUEST_CALL(battleObject_snakeArmUpdate_80C552A);

    GUEST_CALL(object_presentCollisionData);

    R0 = LD8(R5 + oBattleObject_PanelX);
    R1 = LD8(R5 + oBattleObject_PanelY);
    GUEST_CALL(object_highlightPanel);

    RETURN_TO_CALLER();
}

/* ── sub_80C5590 ── */
/* sub_80C5590(r0, r1, r5 = this, lr = return):
 *   Spawn/initialize a sibling battle object from `this` (R5), inheriting its
 *   params, counter/disabler word, extra-vars and element. The newly created
 *   object has its RelatedObject1Ptr wired to this->RelatedObject1Ptr. The
 *   caller's R0/R1 are pushed and restored verbatim across the call. */
Next sub_80C5590(void) {
    /* Push {r0,r1,lr} so the caller can resume with its original R0/R1. */
    u32 frame = SP;
    SP -= 12;
    ST32(SP,        R0);
    ST32(SP + 4,    R1);
    ST32(SP + 8,    LR);

    /* Seed sub_80C5578 with this object's state and ask it to allocate a new
     * battle object (r3 = 0 means "create new"). */
    R3 = 0;
    R4 = LD32(R5 + oBattleObject_Params);
    R6 = LD32(R5 + oBattleObject_DamageAndStaminaDamageCounterDisabler);
    R7 = LD32(R5 + oBattleObject_ExtraVars);
    R2 = LD8(R5 + oBattleObject_Element);
    GUEST_CALL(sub_80C5578);

    /* Link the new object back to the same RelatedObject1Ptr as this one. */
    R1 = LD32(R5 + oBattleObject_RelatedObject1Ptr);
    ST32(R0 + oBattleObject_RelatedObject1Ptr, R1);

    /* Pop {r0,r1,pc}: restore caller's R0/R1 and return via saved LR. */
    R0     = LD32(SP);
    R1     = LD32(SP + 4);
    u32 lr = LD32(SP + 8);
    SP = frame;
    RETURN_TO(lr);
}

/* ── t3_0x8_80C5BB0 ── */
/*
 * t3_0x8_80C5BB0 — battle-object state-machine dispatcher.
 *
 * The jump table off_80C5BC8 is indexed by CurState (byte offsets 0, 4, 8):
 *   0 → sub_80C5BD4       (spawn / initialize)
 *   4 → battle_processCollisionEvent_80C5C40       (active / per-frame logic)
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
        case 4: GUEST_CALL(battle_processCollisionEvent_80C5C40); break;
        case 8: GUEST_CALL(object_genericDestroy); break;
    }
    GUEST_CALL(object_updateSprite);
}

/* t3_0x8_80C5BB0(r5 = battle object) → dispatch on CurState, then update sprite */
Next t3_0x8_80C5BB0(void) {
    dispatch_state_and_update_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C5D84 ── */
/* sub_80C5D84: destroy the battle object's collision region, then if its Param1
 * indicates a sword or wind effect, spawn the corresponding panel-based area
 * attack. Finally mark the object for destruction.
 *
 *   inputs:  r4 r5 lr
 *   outputs: r1 r5
 *   clobbers: nz c v r0 r2 r3 r4 r6 lr
 */
Next sub_80C5D84(void) {
    GUEST_CALL(object_clearCollisionRegion);

    u8 param1 = LD8(R5 + oBattleObject_Param1);

    if (param1 == 1) {
        /* Sword-like effect: spread damage across panels in a wide shape. */
        R0 = LD8(R5 + oBattleObject_PanelX);
        R1 = LD8(R5 + oBattleObject_PanelY);
        R2 = LD8(R5 + oBattleObject_Element);
        R3 = 0;
        R4 = 0x3C << 8;
        R6 = LD32(R5 + oBattleObject_DamageAndStaminaDamageCounterDisabler);
        GUEST_CALL(sub_80C6018);
    } else if (param1 == 2) {
        /* Wind-like effect: spread along the row in a single line. */
        R0 = LD8(R5 + oBattleObject_PanelX);
        R1 = LD8(R5 + oBattleObject_PanelY);
        R2 = LD8(R5 + oBattleObject_Element);
        R3 = 0;
        R6 = LD32(R5 + oBattleObject_DamageAndStaminaDamageCounterDisabler);
        GUEST_CALL(sub_80C68B0);
    }

    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
    RETURN_TO_CALLER();
}

/* ── t3_0x9_80C5DDC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_t3_0x9_80C5DDC;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_t3_0x9_80C5DDC call_object_updateSprite_t3_0x9_80C5DDC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_t3_0x9_80C5DDC){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t3_0x9_80C5DDC: per-state update for the battle object whose pointer is in r5.
 *
 * The object's CurState indexes the jump table at off_80C5DF4
 *   (0 -> sub_80C5E00, 1 -> sub_80C5E44, 2 -> object_genericDestroy),
 * the selected handler is invoked exactly as the original's
 * `mov lr, pc; bx r1` sequence does -- r0 = CurState, r1 = the handler's own
 * entry address, every other register passing through untouched -- and then
 * object_updateSprite refreshes the object's sprite.
 *
 * Outputs r8/r9/r10/r11/r12 are whatever the handler and object_updateSprite
 * leave in the guest register file; the adapter never touches them, so they
 * end exactly as the original leaves them.
 */

/* Invoke one state handler through its jump-table address.
 * register form: the call goes through a table address (the original's
 * `mov lr, pc; bx r1`), so the handler's results exist only in the live
 * guest registers; the object pointer it leaves in r5 is returned from
 * there, since object_updateSprite must run on that (possibly updated)
 * pointer afterwards. */
static u32 call_state_handler(u32 handler, u32 state) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    return R5;
}

/* Typed body: dispatch the object's current state, then update its sprite.
 * Returns the object pointer object_updateSprite leaves in r5. */
static u32 t3_object_stateUpdate(u32 r5) {
    u32 state   = LD8(r5 + oBattleObject_CurState);
    u32 handler = LD32(A_off_80C5DF4 + state);   /* off_80C5DF4: the table the literal pool points at */

    r5 = call_state_handler(handler, state);
    return call_object_updateSprite_t3_0x9_80C5DDC(r5).r5;
}

Next t3_0x9_80C5DDC(void) {
    R5 = t3_object_stateUpdate(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C5E00 ── */
/* sub_80C5E00(r5 = battleObject, r8 = ?, r10 = ?):
 * Initialize a battle object's spawn state: position from panels, copy Param2 into Timer,
 * build and present its collision data, and advance the state machine to CUR_STATE_UPDATE.
 *   inputs:  r5 r8 r10 lr
 *   outputs: r5 r8
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r5 r6 r7 */
Next sub_80C5E00(void) {
    /* Place the object according to its panel coordinates. */
    GUEST_CALL(object_setCoordinatesFromPanels);

    /* Param2 becomes the new Timer value. */
    ST16(R5 + oBattleObject_Timer, LD8(R5 + oBattleObject_Param2));

    /* Try to allocate collision data; bail and free the object on failure. */
    GUEST_CALL(object_createCollisionData);
    SET_FLAGS_CMP(R0, 0);
    if (R0 == 0) {
        GUEST_CALL(object_freeMemory);
        RETURN_TO_CALLER();
    }

    /* Pick collision flags based on Param3:
     *   0 -> 3, 1 -> 1, anything else -> 0. */
    R3 = 3;
    u8 param3 = LD8(R5 + oBattleObject_Param3);
    if (param3 != 0) {
        R3 = 1;
        if (param3 != 1) {
            R3 = 0;
        }
    }

    /* hand = 0xA, group = 5, flags as chosen above. */
    R1 = 0xA;
    R2 = 5;
    GUEST_CALL(object_setupCollisionData);

    /* No hit effect. */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    GUEST_CALL(object_setCollisionHitEffect);

    GUEST_CALL(object_clearCollisionRegion);
    GUEST_CALL(object_presentCollisionData);

    /* Move into the running (update) phase of the state machine. */
    R0 = CUR_STATE_UPDATE;
    SET_FLAGS_NZ(R0);
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, R0);

    RETURN_TO_CALLER();
}

/* ── sub_80C5E44 ── */
/* sub_80C5E44(r5 = battle object):
 * Per-frame collision step for a battle object. Removes the object's old
 * collision data and spawns its contact effect, then either runs the
 * object's current action handler (and presents the resulting collision
 * data), or — if the battle has ended — tears the object down.
 * Outputs: r5, r8-r12 as left by the callees; r4/r7 preserved. */
Next sub_80C5E44(void) {
    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);

    /* Flag 0x10 on the object header skips the battle-over check. */
    u32 flags = LD8(R5 + oObjectHeader_Flags);
    u32 battleOver = 0;
    if (!(flags & 0x10)) {
        GUEST_CALL(battle_isBattleOver);
        battleOver = R0;
    }

    if (!(flags & 0x10) && battleOver) {
        /* Battle is over: drop the object's collision region and mark it
         * for destruction. */
        GUEST_CALL(object_clearCollisionRegion);
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);
        RETURN_TO_CALLER();
    }

    /* Dispatch the handler for the object's current action. The handler is
     * called with r0 = action index and r1 = its own address, and returns
     * here; afterwards the object's collision data is made present. */
    u32 action = LD8(R5 + oBattleObject_CurAction);
    u32 handler = LD32(A_off_80C5E7C + action);
    R0 = action;
    R1 = handler;
    switch (action) {
    case 0:  GUEST_CALL(battleObject_panelBreakEffectTimer_80C5E84); break;
    default: GUEST_CALL(sub_80C5F0C); break;
    }
    GUEST_CALL(object_presentCollisionData);
    RETURN_TO_CALLER();
}

/* ── battleObject_panelBreakEffectTimer_80C5E84 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; } call_object_highlightPanel_result;
/* register form: generated wrapper: object_highlightPanel has no typed C interface yet */
static inline call_object_highlightPanel_result call_object_highlightPanel(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_highlightPanel);
    return (call_object_highlightPanel_result){ R1, R2, R3 };
}

typedef struct { u32 r0, r2, r3; bool c, v; } call_object_getPanelParameters_result;
/* register form: generated wrapper: object_getPanelParameters has no typed C interface yet */
static inline call_object_getPanelParameters_result call_object_getPanelParameters(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(object_getPanelParameters);
    return (call_object_getPanelParameters_result){ R0, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r4, r5; } call_sprite_load_result_sub_80C5E84;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_sub_80C5E84 call_sprite_load_sub_80C5E84(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_sub_80C5E84){ R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_sub_80C5E84(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_sub_80C5E84(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r3, r4, r5; bool c, v; } call_sprite_loadAnimationData_result_sub_80C5E84;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_sub_80C5E84 call_sprite_loadAnimationData_sub_80C5E84(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_sub_80C5E84){ R2, R3, R4, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_sub_80C5E84;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_sub_80C5E84 call_object_getFlip_sub_80C5E84(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_sub_80C5E84){ R0 };
}

typedef struct { u32 r2, r3; bool c, v; } call_sprite_setFlip_result_sub_80C5E84;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_sub_80C5E84 call_sprite_setFlip_sub_80C5E84(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_sub_80C5E84){ R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; } call_object_setCollisionRegion_result;
/* register form: generated wrapper: object_setCollisionRegion has no typed C interface yet */
static inline call_object_setCollisionRegion_result call_object_setCollisionRegion(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionRegion);
    return (call_object_setCollisionRegion_result){ R1 };
}

typedef struct { u32 r1, r2, r4, r5, r6, r7, r10; } call_sub_8109794_result;
/* register form: generated wrapper: sub_8109794 has no typed C interface yet */
static inline call_sub_8109794_result call_sub_8109794(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(sub_8109794);
    return (call_sub_8109794_result){ R1, R2, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_80C5E84;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_80C5E84 call_PlaySoundEffect_sub_80C5E84(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_80C5E84){ R1, R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result call_object_clearCollisionRegion(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result){ R1 };
}

/* battleObject_panelBreakEffectTimer_80C5E84: per-frame handler for the "highlight → teleport" battle-object action.
 * While the timer's bit 2 is clear it repaints the panel highlight; once the timer
 * expires it either swaps to a teleport sprite (success) or aborts (destroy).
 * Returns the final value of R1 (output). R5/R10 are preserved by the callees. */
static u32 battle_highlight_teleport(u32 obj) {
    u32 r1;

    /* --- Highlight phase (every frame while timer bit 2 is clear) --- */
    u16 timer = LD16(obj + oBattleObject_Timer);
    r1 = 4;
    if ((timer & 4) == 0) {
        u32 px = LD8(obj + oBattleObject_PanelX);
        u32 py = LD8(obj + oBattleObject_PanelY);
        r1 = call_object_highlightPanel(px, py).r1;
    }

    /* --- Tick timer; return early if it hasn't expired yet --- */
    timer = LD16(obj + oBattleObject_Timer);
    u16 new_timer = (u16)(timer - 1);
    ST16(obj + oBattleObject_Timer, new_timer);
    if ((s32)(timer - 1) > 0)
        return r1;

    /* --- Timer expired: check whether the destination panel is still valid --- */
    if (LD8(obj + oBattleObject_Param4) != 0) {
        u32 px = LD8(obj + oBattleObject_PanelX);
        u32 py = LD8(obj + oBattleObject_PanelY);
        call_object_getPanelParameters_result params =
            call_object_getPanelParameters(px, py);
        r1 = 0x10;
        if ((params.r0 & 0x10) == 0) {
            /* Destination invalid – cancel: clear collision, mark for destruction */
            r1 = call_object_clearCollisionRegion(obj).r1;
            ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized,
                 CUR_STATE_DESTROY);
            return r1;
        }
    }

    /* --- Success path: teleport complete, swap sprite --- */
    r1 = 0x10;
    call_sprite_load_sub_80C5E84(0x80, 0x10, 0x26, obj);
    call_sprite_noShadow_sub_80C5E84(obj);

    /* Set "sprite-visible" flag bit 1 */
    ST8(obj, LD8(obj) | 2);
    ST8(obj + oBattleObject_CurAnim, 0);
    ST8(obj + oBattleObject_CurAnimCopy, 0);

    call_sprite_setAnimation_sub_80C5E84(0, obj);
    call_sprite_loadAnimationData_sub_80C5E84(obj);

    call_object_getFlip_result_sub_80C5E84 flip = call_object_getFlip_sub_80C5E84(obj);
    call_sprite_setFlip_result_sub_80C5E84 setflip = call_sprite_setFlip_sub_80C5E84(flip.r0, obj);

    r1 = call_object_setCollisionRegion(1, obj).r1;

    /* If Param1 is set, update the panel the object is linked to */
    u32 sound_r2 = setflip.r2;
    if (LD8(obj + oBattleObject_Param1) != 0) {
        u32 px = LD8(obj + oBattleObject_PanelX);
        u32 py = LD8(obj + oBattleObject_PanelY);
        call_sub_8109794_result moved = call_sub_8109794(px, py);
        r1 = moved.r1;
        sound_r2 = moved.r2;
    }

    /* Play the teleport sound effect */
    r1 = call_PlaySoundEffect_sub_80C5E84(0xB9, sound_r2).r1;

    ST8(obj + oBattleObject_CurAction, 4);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);

    return r1;
}

/* battleObject_panelBreakEffectTimer_80C5E84(r5 = battle object pointer) -> r1 */
Next battleObject_panelBreakEffectTimer_80C5E84(void) {
    R1 = battle_highlight_teleport(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80C5F0C ── */
/* sub_80C5F0C(r5 = battle object):
 * Per-frame update for an object in its destruction sequence. On the first
 * invocation (phase 0) it advances the phase and drops the object's collision
 * region so it no longer interacts. Afterwards it checks the current sprite
 * frame's parameters: when the end-of-animation flag (bit 0x80) is set, the
 * object's state is switched to CUR_STATE_DESTROY. Always returns r1 = 0x80. */
Next sub_80C5F0C(void) {
    if (LD8(R5 + oBattleObject_CurPhase) == 0) {
        ST8(R5 + oBattleObject_CurPhase, 1);
        GUEST_CALL(object_clearCollisionRegion);
    }

    GUEST_CALL(sprite_getFrameParameters);
    R1 = 0x80;

    if (R0 & R1)
        ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);

    RETURN_TO_CALLER();
}

/* ── sub_80C5F2C ── */
/* sub_80C5F2C(r0 = panel X, r1 = panel Y, r2 = element, r5 = source battle object,
 *             r6 = damage/stamina counter disabler value):
 * Spawn a type-3 battle object (subtype 9); if successful, initialize its panel position,
 * element, and damage counter from the caller's arguments, then copy the alliance and
 * direction-flip field from the source object.
 * Outputs: r0 = new object (0 on failure), r1 = panel X, r2 = panel Y,
 *          r3 = element (beq path) or alliance word (success path),
 *          r5 = source object (unchanged), C = 0, V = 0. */
Next sub_80C5F2C(void) {
    u32 panel_x = R0;
    u32 panel_y = R1;
    u32 element = R2;
    u32 src_obj = R5;

    /* Spawn the object; result pointer arrives in R5. */
    R0 = 9;
    GUEST_CALL(object_spawnType3);
    u32 new_obj = R5;

    /* mov r0, r5 in Thumb MOVS also behaves as ADDS Rd, Rn, #0 here:
     * sets NZ from the pointer and clears C and V. */
    SET_FLAGS_ADD(new_obj, 0);

    /* Restore the arguments that the callee may have clobbered (the pop in the original). */
    R1 = panel_x;
    R2 = panel_y;
    R3 = element;
    R5 = src_obj;

    if (new_obj == 0) {
        /* Spawn failed – return with r0 = 0. */
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* Initialize the new object's panel position, element, and counter. */
    ST8(new_obj + oBattleObject_PanelX, panel_x);
    ST8(new_obj + oBattleObject_PanelY, panel_y);
    ST8(new_obj + oBattleObject_Element, element);
    ST32(new_obj + oBattleObject_DamageAndStaminaDamageCounterDisabler, R6);

    /* Copy alliance / direction-flip from the source object into the new one. */
    u16 alliance = LD16(src_obj + oBattleObject_AllianceAndDirectionFlip);
    ST16(new_obj + oBattleObject_AllianceAndDirectionFlip, alliance);

    R0 = new_obj;
    R3 = alliance;
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

/* ── sub_80C6438 ── */
/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void call_sprite_load_sub_80C6438(u32 a, u32 b, u32 c) {
    R0 = a; R1 = b; R2 = c;
    GUEST_CALL(sprite_load);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_sprite_noShadow_sub_80C6438(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls object_getCoordinatesForPanels outside this file, which has no typed C interface yet */
static void call_object_getCoordinatesForPanels(u32 obj, u32 px, u32 py) {
    R0 = px; R1 = py;
    GUEST_CALL(object_getCoordinatesForPanels);
    ST32(obj + oBattleObject_X, R0);
    ST32(obj + oBattleObject_Y, R1);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void call_PlaySoundEffect(u32 effect) {
    R0 = effect;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls sub_80C647C outside this file, which has no typed C interface yet */
static void call_sub_80C647C(void) {
    GUEST_CALL(sub_80C647C);
}

/* "Item get" reaction: flag the object active, load its sprite, snap it to its panel's
 * world coordinates with a quick upward hop, play the pickup SFX, then advance to the
 * per-state update routine. */
static void battleobject_item_react(u32 obj) {
    ST8(obj + oObjectHeader_Flags, (u8)(LD8(obj + oObjectHeader_Flags) | 0x02));
    call_sprite_load_sub_80C6438(0x80, 0x0C, 0x13);
    call_sprite_noShadow_sub_80C6438();
    ST8(obj + oBattleObject_CurAnim, 0);
    ST8(obj + oBattleObject_CurAnimCopy, 0xFF);
    ST32(obj + oBattleObject_ZVelocity, 0x80000);
    call_object_getCoordinatesForPanels(obj, LD8(obj + oBattleObject_PanelX), LD8(obj + oBattleObject_PanelY));
    ST32(obj + oBattleObject_Z, 0x1000000);
    call_PlaySoundEffect(0xA1);
    ST8(obj + oBattleObject_CurState, CUR_STATE_UPDATE);
    call_sub_80C647C();
}

Next sub_80C6438(void) {
    battleobject_item_react(R5);
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

/* ── sub_80C6964 ── */
/* sub_80C6964 (r5 = battle object, r8/r10 = caller state passed through):
 * Spawn-time initialization for an object: move it to the pixel position of
 * its current panel, point ExtraVars at its per-type record in byte_80C68D4,
 * face it in the direction it advances in, and set up its collision data.
 * If collision data cannot be allocated the object is freed immediately;
 * otherwise it is handed collision ID 0x3D and switched to CUR_STATE_UPDATE.
 * r5 and r8 are left untouched for the caller. */
Next sub_80C6964(void) {
    u32 obj = R5;
    u32 savedR7 = R7; /* the original pushes r7 and restores it on both exits */

    /* Place the object at the pixel coordinates of its panel. */
    R0 = LD8(obj + oBattleObject_PanelX);
    R1 = LD8(obj + oBattleObject_PanelY);
    GUEST_CALL(object_getCoordinatesForPanels);
    ST32(obj + oBattleObject_X, R0);
    ST32(obj + oBattleObject_Y, R1);

    /* ExtraVars selects this object's 6-byte record in the type table. */
    u32 extraVars = A_byte_80C68D4 + LD8(obj + oBattleObject_Param1) * 6;
    R7 = extraVars; /* the callees below are invoked with r7 pointing at it */
    ST32(obj + oBattleObject_ExtraVars, extraVars);

    /* The object moves toward whatever is in front of it. */
    GUEST_CALL(object_getFrontDirection);
    ST32(obj + oBattleObject_XVelocity, R0);

    /* Allocate collision data; if that fails, free the object and bail out. */
    GUEST_CALL(object_createCollisionData);
    if (R0 == 0) {
        GUEST_CALL(object_freeMemory);
        R7 = savedR7;
        RETURN_TO_CALLER();
    }

    /* R0 still holds the value object_createCollisionData returned. */
    R1 = 0x3D;
    R2 = 5;
    R3 = 0;
    GUEST_CALL(object_setupCollisionData);

    /* Present the collision data with no primary element selected yet. */
    R0 = 0;
    R1 = LD32(obj + oBattleObject_CollisionDataPtr);
    ST8(R1 + oCollisionData_PrimaryElement, 0);
    GUEST_CALL(object_presentCollisionData);

    /* Hand control over to the per-frame update state. */
    R7 = savedR7;
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);
    RETURN_TO_CALLER();
}

/* ── sub_80C69AC ── */
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

typedef struct { u32 r2, r3, r4, r5, r6; bool c, v; } call_object_spawnCollisionEffect_result;
/* register form: generated wrapper: object_spawnCollisionEffect has no typed C interface yet */
static inline call_object_spawnCollisionEffect_result call_object_spawnCollisionEffect(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_spawnCollisionEffect);
    return (call_object_spawnCollisionEffect_result){ R2, R3, R4, R5, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag_sub_80C69AC(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

typedef struct { u32 r2; } call_object_setFlag1_result_sub_80C69AC;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_sub_80C69AC call_object_setFlag1_sub_80C69AC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_sub_80C69AC){ R2 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r8; bool z, n, c, v; } call_object_presentCollisionData_result;
/* register form: generated wrapper: object_presentCollisionData has no typed C interface yet */
static inline call_object_presentCollisionData_result call_object_presentCollisionData(u32 r1, u32 r5, u32 r8) {
    R1 = r1;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(object_presentCollisionData);
    return (call_object_presentCollisionData_result){ R0, R1, R2, R3, R4, R5, R6, R8, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_80C69AC: battle-object per-tick step.
 * Clears collision bookkeeping, respawns effect, handles landing action,
 * dispatches via jump table, then presents collision data.
 *
 * Inputs:  r4, r5 (BattleObject), r7 (preserved), r8, r10, lr
 * Outputs: r5, r8, r9, r10, r11, r12
 * Clobbers: nz c v r0 r1 r2 r3 r4 r6 lr
 * Preserved: r7
 *
 * register form: jump-table dispatch (bx rN to 0x80C6A08 or 0x80C6A34) whose target
 * may branch to 0x80C69E8 (destroy path within this function's range) instead of
 * returning to 0x80C69E2; C control flow cannot express the intra-function branch
 * target, so GUEST_CALL_ADDR's caller-side register state must be set and read
 * directly through the register lvalues. */

/* Jump-table targets: r0=CurAction (byte offset), r1=target addr, r5=object, r7=CollisionDataPtr. */
#define JT_RETURN_ADDR    0x080C69E2u
#define JT_DESTROY_ADDR   0x080C69E8u
#define LANDED_FLAG       0x100u   /* bit 8: 1<<8 */

static u32 battle_object_step_tick(u32 r4_in, u32 obj, u32 r7_entry, u32 r8_in,
                                   u32 r9_in, u32 r10_in, u32 r11_in, u32 r12_in,
                                   u32 *out_r8) {
    /* 1. Remove this tick's collision bookkeeping. */
    call_object_removeCollisionData(r4_in, obj, r7_entry);

    /* 2. Respawn the collision visual effect. */
    obj = call_object_spawnCollisionEffect(obj).r5;

    /* 3. Read collision-data pointer. */
    u32 coll_data = LD32(obj + oBattleObject_CollisionDataPtr);

    /* 4. If contact was recorded and the landed flag is not yet set, lock it in
     *    and switch to action 4 (landed), resetting the phase counter. */
    if (LD32(coll_data + oCollisionData_FlagsFromCollision) != 0) {
        u32 obj_flags = call_object_getFlag_sub_80C69AC(obj);
        if ((obj_flags & LANDED_FLAG) == 0) {
            call_object_setFlag1_sub_80C69AC(LANDED_FLAG, obj);
            ST8(obj + oBattleObject_CurAction, 4);
            ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
        }
    }

    /* 5. Jump-table dispatch: load target from off_80C6A00[CurAction], set guest
     *    registers, call through the address.  The callee runs to completion
     *    (either returning to JT_RETURN_ADDR or branching to JT_DESTROY_ADDR). */
    u32 cur_action = LD8(obj + oBattleObject_CurAction);
    u32 target = LD32(A_off_80C6A00 + cur_action);

    /* register form: see file comment */
    R0 = cur_action;
    R1 = target;
    R5 = obj;
    R7 = coll_data;
    R8 = r8_in;
    R9 = r9_in;
    R10 = r10_in;
    R11 = r11_in;
    R12 = r12_in;
    GUEST_CALL_ADDR(target);
    obj = R5;
    u32 r1_after = R1;
    r8_in = R8;

    /* 6. Present the (possibly updated) collision data. */
    call_object_presentCollisionData_result pres =
        call_object_presentCollisionData(r1_after, obj, r8_in);

    obj = pres.r5;
    *out_r8 = pres.r8;
    return obj;
}

/* sub_80C69AC(r4, r5=BattleObject, r7, r8, r10, lr) -> r5 r8 r9 r10 r11 r12 */
Next sub_80C69AC(void) {
    u32 saved_r7 = R7;
    u32 out_r8;
    R5 = battle_object_step_tick(R4, R5, saved_r7, R8, R9, R10, R11, R12, &out_r8);
    R8 = out_r8;
    R7 = saved_r7;
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

/* ── battle_object_dispatchPhaseHandler_80C6A34 ── */
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

/* battle_object_dispatchPhaseHandler_80C6A34(r5 = BattleObject*) -> r0, r1, nzcv
 * Looks up CurPhase in the two-entry handler table and tail-calls the result. */
Next battle_object_dispatchPhaseHandler_80C6A34(void) {
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
    GUEST_CALL(battleObject_setElement_8019F8C);

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
 *   0 -> battle_objectInitFromParamTable_80C6B64            (state handler 0)
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
        GUEST_CALL(battle_objectInitFromParamTable_80C6B64);
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

/* ── battle_objectInitFromParamTable_80C6B64 ── */
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

/* battle_objectInitFromParamTable_80C6B64(r5 = battle object ptr) -> r0, flags from last callee */
/* register form: original saves r4/r7 via push/pop; helpers pass args through R4/R7
 * which remain modified after callee returns, so restore them here */
Next battle_objectInitFromParamTable_80C6B64(void) {
    u32 saved_r4 = R4, saved_r7 = R7;
    R0 = init_object_from_table(R5);
    R4 = saved_r4;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_80C6C14 ── */
/* sub_80C6C14 (r5 = battle object): tick for an object standing inside an
 * active collision effect (grass/fire/swamp panels etc.).
 *
 * Removes the object's collision data, spawns the panel's effect, then lets
 * the object's current action handler react to the collision. If the battle
 * is already over, or the object is not on a panel that reacts to the
 * effect, the collision region is cleared and the object is destroyed.
 *
 * Preserves r7 (used internally for the object's collision data pointer). */
Next sub_80C6C14(void) {
    u32 savedR7 = R7;

    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);
    GUEST_CALL(battle_isBattleOver);

    if (R0 == 0) {
        /* Panel parameters of the panel the object stands on. */
        R0 = LD8(R5 + oBattleObject_PanelX);
        R1 = LD8(R5 + oBattleObject_PanelY);
        GUEST_CALL(object_getPanelParameters);
        R3 = 0x10;

        /* Bit 0x10: the panel type reacts to collision effects. */
        if ((R0 & 0x10) != 0) {
            R7 = LD32(R5 + oBattleObject_CollisionDataPtr);

            /* If the collision set any flags, clear the region first. */
            R0 = LD32(R7 + oCollisionData_FlagsFromCollision);
            if (R0 != 0)
                GUEST_CALL(object_clearCollisionRegion);

            /* Run the object's current action handler
             * (table off_80C6C58: 0 = tick, 1 = dying/despawn). */
            u32 action = LD8(R5 + oBattleObject_CurAction);
            R1 = LD32(A_off_80C6C58 + action * 4);
            if (action == 0)
                GUEST_CALL(battleObject_kind0x16SnakearmStep_80C6C6A);
            else
                GUEST_CALL(battleObject_dyingTickDespawn_80C6CBA);

            GUEST_CALL(object_presentCollisionData);
            GUEST_CALL(object_highlightCurrentCollisionPanels);

            R7 = savedR7;
            RETURN_TO_CALLER();
        }
    }

    /* Battle over, or the panel doesn't react: tear down the effect and
     * move the object to the destroy state. */
    GUEST_CALL(object_clearCollisionRegion);
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_DESTROY);

    R7 = savedR7;
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
