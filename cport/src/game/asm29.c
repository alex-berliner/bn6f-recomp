/*
 * asm29.c: readable functions of bn6f asm29.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── ho_80A4984 ── */
/* ho_80A4984: overworld map object dispatcher.
 *
 * R5 points to an OverworldMapObject (struct layout: +4 = ObjectID,
 * +8 = handler selector byte). This routine resolves the object's data
 * block inside OverworldMapObjects (base + ObjectID * entry size), leaves
 * it in R7 for the handler, and calls the handler selected by the
 * selector byte. Because the original returns straight through to our
 * caller (mov lr,pc / bx r0 followed only by pop {pc}), this is effectively
 * a tail call: the handler's registers and flags are handed directly to
 * whoever called us.
 */
Next ho_80A4984(void) {
    u32 entrySize = oMapObjectData_Size;

    /* R7 <- &OverworldMapObjects[ObjectID]; the original's ADDS also sets
     * flags, but the handler overwrites them, so nothing to preserve here. */
    u32 objectId = LD8(R5 + 4);           /* oOverworldMapObject.ObjectID */
    R2 = entrySize;
    R7 = A_OverworldMapObjects + objectId * entrySize;

    /* Pick the handler. The table is indexed with the raw selector byte
     * (valid values 0/4/8, i.e. already scaled to .word strides); the
     * loaded entry (Thumb address | 1) is what the handler sees in R0. */
    u32 selector = LD8(R5 + 8);           /* oOverworldMapObject.Unk_08 */
    R1 = selector;
    R0 = LD32(A_jt_80A49A0 + selector);

    switch (selector) {
    case 0x0: GUEST_CALL(battleObject_initFromParams_80A49B0);   break;
    case 0x4: GUEST_CALL(event_80A4A08); break;
    case 0x8: GUEST_CALL(ow_freeMapObjectTailcall);   break;
    }

    RETURN_TO_CALLER();
}

/* ── battleObject_initFromParams_80A49B0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_sub_80A49B0;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_sub_80A49B0 call_sprite_load_sub_80A49B0(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_sub_80A49B0){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_sub_80A49B0(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_sub_80A49B0;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_sub_80A49B0 call_sprite_loadAnimationData_sub_80A49B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_sub_80A49B0){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_sub_80A49B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_checkZCoordModifiers_8031612_result;
/* register form: generated wrapper: checkZCoordModifiers_8031612 has no typed C interface yet */
static inline call_checkZCoordModifiers_8031612_result call_checkZCoordModifiers_8031612(u32 r0) {
    R0 = r0;
    GUEST_CALL(checkZCoordModifiers_8031612);
    return (call_checkZCoordModifiers_8031612_result){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result_sub_80A49B0;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result_sub_80A49B0 call_applyLayerEffectToOWObject_8035694_sub_80A49B0(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result_sub_80A49B0){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r1, r2; } call_sprite_setPriority_8002E14_result_sub_80A49B0;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_sub_80A49B0 call_sprite_setPriority_8002E14_sub_80A49B0(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_sub_80A49B0){ R1, R2 };
}

/* register form: generated wrapper: sprite_setUnk0x2c has no typed C interface yet */
static inline void call_sprite_setUnk0x2c(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setUnk0x2c);
}

/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline void call_sprite_setPalette_sub_80A49B0(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
}

typedef struct { u32 r2, r5; } call_event_80A4A08_result;
/* register form: generated wrapper: event_80A4A08 has no typed C interface yet */
static inline call_event_80A4A08_result call_event_80A4A08(u32 r5, u32 r7) {
    R5 = r5;
    R7 = r7;
    GUEST_CALL(event_80A4A08);
    return (call_event_80A4A08_result){ R2, R5 };
}

/* Configuration block layout (r7), byte offsets for readability: */
#define CFG_SPRITE_ID      0   /* u8: first sprite resource argument */
#define CFG_SPRITE_ARG     1   /* u8: second sprite resource argument */
#define CFG_ANIM_INDEX     2   /* u8: initial animation frame         */
#define CFG_Z_MOD_FLAG     3   /* u8: 0xFF → apply Z modifiers       */
#define CFG_UNK0x2C        4   /* u32: value for sprite_setUnk0x2c    */
#define CFG_PALETTE_INDEX  8   /* u8: sprite palette slot             */

/* Object (r5) field offsets used by this function: */
#define OBJ_Z_COORD      0x14  /* u32: Z position / height           */
#define OBJ_Z_SUBSTRUCT  0x0C  /* start of substruct for Z/layer calcs */
#define OBJ_LAYER        8    /* u8: draw layer                      */
#define OBJ_SHADOW       7    /* u8: shadow flag                     */
#define OBJ_FLAGS        6    /* u8: misc flags                      */

/* Initialize a field/battle sprite from the config block at `cfg`:
 * load the sprite resource, set animation and palette, optionally
 * apply Z-coordinate modifiers and layer effects, clear shadow flags,
 * and run the post-init event.  Returns the r2 produced by the event. */
static u32 init_field_sprite(u32 obj, u32 cfg) {
    /* Load the sprite (category 0x80, two resource IDs from config). */
    call_sprite_load_result_sub_80A49B0 sl = call_sprite_load_sub_80A49B0(0x80,
                                                   LD8(cfg + CFG_SPRITE_ID),
                                                   LD8(cfg + CFG_SPRITE_ARG),
                                                   obj);
    obj = sl.r5;   /* sprite_load may return a different object pointer */

    /* Set the initial animation index from config byte 2. */
    call_sprite_setAnimation_sub_80A49B0(LD8(cfg + CFG_ANIM_INDEX), obj);

    /* Load animation data; this also updates obj and may return r2. */
    call_sprite_loadAnimationData_result_sub_80A49B0 la = call_sprite_loadAnimationData_sub_80A49B0(obj);
    obj = la.r5;

    /* Disable the sprite's shadow rendering. */
    call_sprite_noShadow_sub_80A49B0(obj);

    u32 r0;

    /* If byte 3 is 0xFF, temporarily adjust the Z coordinate
     * through the tile/layer-modifier pass. */
    if (LD8(cfg + CFG_Z_MOD_FLAG) == 0xFF) {
        u32 saved_z = LD32(obj + OBJ_Z_COORD);

        /* Query the Z modifier for the substruct at obj+0xC. */
        call_checkZCoordModifiers_8031612_result cz =
            call_checkZCoordModifiers_8031612(obj + OBJ_Z_SUBSTRUCT);
        obj = cz.r5;
        r0  = cz.r0 << 16;
        ST32(obj + OBJ_Z_COORD, r0);

        /* Apply the layer effect using the modified Z. */
        call_applyLayerEffectToOWObject_8035694_result_sub_80A49B0 ae =
            call_applyLayerEffectToOWObject_8035694_sub_80A49B0(obj + OBJ_Z_SUBSTRUCT);
        obj = ae.r5;
        ST32(obj + OBJ_Z_COORD, saved_z);  /* restore the real Z */

        r0 = ae.r0;
    } else {
        /* No Z adjustment: r0 is just the config byte we compared. */
        r0 = LD8(cfg + CFG_Z_MOD_FLAG);
    }

    /* Set sprite priority (obj passed so the callee reads its data). */
    call_sprite_setPriority_8002E14_sub_80A49B0(r0, obj);

    /* Write the 32-bit field at config offset 4 into object word 0x2C. */
    call_sprite_setUnk0x2c(LD32(cfg + CFG_UNK0x2C), obj);

    /* Set the palette index from config byte 8. */
    call_sprite_setPalette_sub_80A49B0(LD8(cfg + CFG_PALETTE_INDEX), obj);

    /* Clear flags and shadow; mark the object's layer. */
    ST8(obj + OBJ_LAYER,  4);
    ST8(obj + OBJ_FLAGS,  0);
    ST8(obj + OBJ_SHADOW, 0);

    /* Fire the post-init event and return its r2. */
    call_event_80A4A08_result ev = call_event_80A4A08(obj, cfg);
    return ev.r2;
}

/* battleObject_initFromParams_80A49B0(r5 = sprite object, r7 = config block, r10 = [layer ctx]) -> r2 */
Next battleObject_initFromParams_80A49B0(void) {
    R2 = init_field_sprite(R5, R7);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchState4Handler_80A4A98 ── */
/* register form: calls battle_objectEnterState4LatchTilePos_80A4AB8 outside this file,
 * which has no typed C interface yet */
static void call_latch_tile_pos(void) {
    GUEST_CALL(battle_objectEnterState4LatchTilePos_80A4AB8);
}

/* register form: calls sub_80A4ACC outside this file, which has no typed C interface yet */
static void call_state_handler_4acc(void) {
    GUEST_CALL(sub_80A4ACC);
}

/* register form: calls ow_freeObject outside this file, which has no typed C interface yet */
static void call_state_handler_4bd4(void) {
    GUEST_CALL(ow_freeObject);
}

/* Dispatch to the battle-object state handler selected by the byte at [r5+8].
 * The table at 0x080A4AAC has 3 word entries at offsets 0, 4, 8; the byte
 * stored in the object is used directly as the offset into the table. */
static void dispatch_battle_state(u32 r5) {
    u8 state_offset = LD8(r5 + 8);
    if (state_offset == 0)
        call_latch_tile_pos();
    else if (state_offset == 4)
        call_state_handler_4acc();
    else
        call_state_handler_4bd4();
}

/* battle_dispatchState4Handler_80A4A98(r5 = battle object ptr) -> all registers, flags (passed through / set by callee) */
Next battle_dispatchState4Handler_80A4A98(void) {
    dispatch_battle_state(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectEnterState4LatchTilePos_80A4AB8 ── */
/* battle_objectEnterState4LatchTilePos_80A4AB8 (r5 = battle object):
 * Put the object into state 4 and latch its current tile coordinates
 * (bytes at +5/+6) into its position fields (+0x24/+0x28),
 * then run the follow-up update in sub_80A4ACC. */
Next battle_objectEnterState4LatchTilePos_80A4AB8(void) {
    ST8(R5 + 8, 4);                      /* state = 4 */

    /* copy tile coords into the position fields */
    R0 = LD8(R5 + 5);
    ST32(R5 + 0x24, R0);
    R0 = LD8(R5 + 6);
    ST32(R5 + 0x28, R0);

    /* r0 still holds the y coord at the call site, as in the original */
    GUEST_CALL(sub_80A4ACC);

    RETURN_TO_CALLER();
}

/* ── menu_stateDispatch_80A4BDC ── */
/* menu_stateDispatch_80A4BDC: tail-call dispatcher for the menu object in r5.
 *
 * The phase byte at [r5+8] is stored pre-multiplied by 4 (0, 4 or 8), so it
 * serves directly as the byte offset into the handler table at off_80A4BF0.
 * The selected handler is called with R0 = its own table entry (address with
 * thumb bit) and R1 = the raw phase byte; because the original tail-calls it
 * (mov lr, pc / bx r0), the handler's results pass straight through to this
 * function's caller. */
Next menu_stateDispatch_80A4BDC(void) {
    u32 sel = LD8(R5 + 8);

    R1 = sel;
    R0 = LD32(A_off_80A4BF0 + sel); /* handler address from the dispatch table */

    switch (sel) {
    case 0: GUEST_CALL(sprite_setupEffect_80A4BFC); break;
    case 4: GUEST_CALL(sprite_tickWithFlagReset_80A4C46); break;
    case 8: GUEST_CALL(sub_80A4C5C); break;
    /* other byte values would index past the table and never occur */
    }

    RETURN_TO_CALLER();
}

/* ── sprite_setupEffect_80A4BFC ── */
/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void call_sprite_load(u32 tile_count, u32 tile_slot, u32 pal_slot) {
    R0 = tile_count;
    R1 = tile_slot;
    R2 = pal_slot;
    GUEST_CALL(sprite_load);
}

/* register form: calls checkZCoordModifiers_8031612 outside this file, which has no typed C interface yet */
static u32 call_checkZCoordModifiers(u32 coord_ptr) {
    R0 = coord_ptr;
    GUEST_CALL(checkZCoordModifiers_8031612);
    return R0;
}

/* register form: calls applyLayerEffectToOWObject_8035694 outside this file, which has no typed C interface yet */
static void call_applyLayerEffectToOWObject(u32 coord_ptr) {
    R0 = coord_ptr;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_sprite_setPriority(void) {
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation(u32 anim) {
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

/* register form: calls sprite_tickWithFlagReset_80A4C46 outside this file, which has no typed C interface yet */
static void call_sprite_tickWithFlagReset(void) {
    GUEST_CALL(sprite_tickWithFlagReset_80A4C46);
}

/* Sprite setup for a shockwave / effect overworld object: writes the "loaded" state byte,
 * loads the sprite graphics into VRAM, applies Z-coordinate and layer-effect transforms to
 * the object's coord triple, configures priority/animation/palette, then kicks off the
 * per-frame tick. Returns the original Z value loaded from offset 0x14, which the caller
 * observes in r6 (the assembly clobbers r6 with that load). */
static u32 setup_effect_sprite(u32 obj) {
    /* state byte = 3 ("sprite loaded") */
    ST8(obj, 3);

    /* sprite_load(tile_count = 0x80, tile_slot, palette_slot) */
    call_sprite_load(0x80, LD8(obj + 6), LD8(obj + 7));

    /* The two transform calls operate on the coord triple at obj+0xC and read a "Z bias"
     * from obj+0x14. The original saves r6 = [obj+0x14], runs checkZCoordModifiers (returns
     * a value to shift <<16 and write into obj+0x14), runs applyLayerEffectToOWObject (which
     * reads the shifted value), then restores [obj+0x14] to the saved value. */
    u32 saved_z = LD32(obj + 0x14);
    u32 z_mod   = call_checkZCoordModifiers(obj + 0xC);
    ST32(obj + 0x14, z_mod << 16);
    call_applyLayerEffectToOWObject(obj + 0xC);
    ST32(obj + 0x14, saved_z);

    /* Priority, animation, palette configuration. */
    call_sprite_setPriority();
    call_sprite_setAnimation(LD8(obj + 4));
    call_sprite_loadAnimationData();
    call_sprite_noShadow();
    call_sprite_setPalette(LD8(obj + 5));

    /* animation-step field = 4 */
    ST8(obj + 8, 4);

    call_sprite_tickWithFlagReset();

    return saved_z;
}

/* sprite_setupEffect_80A4BFC: sprite setup for a shockwave/effect overworld object (writes the
 * state byte, loads the sprite, applies Z/layer transforms, sets up animation+palette,
 * kicks off the per-frame tick). r5 holds the object pointer; r6 is clobbered with
 * the original Z value loaded from offset 0x14. */
Next sprite_setupEffect_80A4BFC(void) {
    R6 = setup_effect_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_tickWithFlagReset_80A4C46 ── */
/* sprite_tickWithFlagReset_80A4C46(r4 = unused context, r5 = sprite object):
 * Sprite animation tick: fetch the current frame's parameters, and if the
 * frame carries the 0x80 flag, reset the object's byte at +8 to 8 (probably
 * a sub-action/animation counter), then advance the sprite animation.
 * Flags and r0-r3 on return are whatever sprite_update leaves. */
Next sprite_tickWithFlagReset_80A4C46(void) {
    GUEST_CALL(sprite_getFrameParameters);

    if (R0 & 0x80)
        ST8(R5 + 8, 8);

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_80A4C5C ── */
/* sub_80A4C5C: tail-call wrapper that frees the current overworld map object
 * and returns to the caller. The data at byte_80A4C64 immediately follows
 * this routine in the ROM and is consumed by other functions. */
Next sub_80A4C5C(void) {
    GUEST_CALL(FreeOverworldMapObject);
    RETURN_TO_CALLER();
}

/* ── npc_stateDispatchStep_80A51C4 ── */
/* npc_stateDispatchStep_80A51C4(r5 = state struct):
 * Points r7 at a 0x10-byte record in the table at byte_80A4C64, selected by
 * the byte at r5+4, then calls one of five handlers selected by the byte at
 * r5+8, which indexes the table at off_80A51E0 bytewise (entries are 4 bytes
 * apart, so the selector holds 0x00/0x04/0x08/0x0C/0x10):
 *   +0x00 mapEntry_update_80A51F8   +0x04 mapEntry_onUpdate_80A5268   +0x08 sub_80A52E4
 *   +0x0C object_beastRoarCutsceneTick   +0x10 sub_80A5350
 * The original branches through r0, so the handler is entered with
 * r0 = handler address, r1 = raw selector byte, r7 = selected record. */
Next npc_stateDispatchStep_80A51C4(void) {
    u32 recordBase = A_byte_80A4C64;
    u32 recordOff  = 0x10 * (u32)LD8(R5 + 4);

    R7 = recordBase + recordOff;
    SET_FLAGS_ADD(recordBase, recordOff);

    u32 sel    = (u32)LD8(R5 + 8);
    u32 target = LD32(A_off_80A51E0 + sel);

    R0 = target;
    R1 = sel;

    switch (sel) {
    case 0x00: GUEST_CALL(mapEntry_update_80A51F8); break;
    case 0x04: GUEST_CALL(mapEntry_onUpdate_80A5268); break;
    case 0x08: GUEST_CALL(sub_80A52E4); break;
    case 0x0C: GUEST_CALL(object_beastRoarCutsceneTick); break;
    case 0x10: GUEST_CALL(sub_80A5350); break;
    /* Selector outside the table: branch to whatever the load yielded. */
    default:   dispatch(target);        break;
    }

    RETURN_TO_CALLER();
}

/* ── mapEntry_update_80A51F8 ── */
/* ---------- register-form helpers for game functions with no typed C interface yet ---------- */

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool test_event_flag_sub_80A51F8(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void sprite_load_register(int sprite_id, int a2, int a3) {
    R0 = sprite_id;
    R1 = a2;
    R2 = a3;
    GUEST_CALL(sprite_load);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void sprite_setPriority_register(u8 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_setAnimation_register(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_loadAnimationData_register(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_noShadow_register(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setUnk0x2c outside this file, which has no typed C interface yet */
static void sprite_setUnk0x2c_register(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setUnk0x2c);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void sprite_setPalette_register(int palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls ClearEventFlag outside this file, which has no typed C interface yet */
static void clear_event_flag_register(u16 flag) {
    R0 = flag;
    GUEST_CALL(ClearEventFlag);
}

/* register form: calls SetEventFlag outside this file, which has no typed C interface yet */
static void set_event_flag_register(u16 flag) {
    R0 = flag;
    GUEST_CALL(SetEventFlag);
}

/* register form: calls mapEntry_onUpdate_80A5268 outside this file, which has no typed C interface yet */
/* leftover unused: mapEntry_onUpdate_80A5268's r0 (it becomes mapEntry_update_80A51F8's r0 and flags, which its callers
 * do not read) */
static u32 call_sub_80A5268(void) {
    GUEST_CALL(mapEntry_onUpdate_80A5268);
    return R0;
}

/* register form: calls sub_80A5350 outside this file, which has no typed C interface yet */
/* leftover unused: sub_80A5350's r0 (it becomes mapEntry_update_80A51F8's r0 and flags, which its callers
 * do not read) */
static u32 call_sub_80A5350(void) {
    GUEST_CALL(sub_80A5350);
    return R0;
}

/* ---------- typed function ---------- */

/* Entry-controller update. If the entry's trigger flag is set, load the sprite for it,
 * clear its two associated flags, and call mapEntry_onUpdate_80A5268. Otherwise set those flags,
 * mark the entry hidden, and call sub_80A5350.
 * The output r0/flags come from whichever tail subroutine was called. */
static u32 update_entry(u32 ctrl_addr, u32 entry_addr) {
    u16 trigger   = LD16(entry_addr);
    u8  sprite_a2 = LD8(entry_addr + 6);
    u8  sprite_a3 = LD8(entry_addr + 7);
    u8  priority  = LD8(entry_addr + 0xC);
    u8  animation = LD8(entry_addr + 8);
    u8  unk0x2c   = LD8(entry_addr + 0xA);
    u8  palette   = LD8(entry_addr + 0xB);
    u16 flag2     = LD16(entry_addr + 2);
    u16 flag4     = LD16(entry_addr + 4);

    if (test_event_flag_sub_80A51F8(trigger)) {
        /* trigger set -> show the entry */
        ST8(ctrl_addr, 3);

        sprite_load_register(0x80, sprite_a2, sprite_a3);
        sprite_setPriority_register(priority);
        sprite_setAnimation_register(animation);
        sprite_loadAnimationData_register();
        sprite_noShadow_register();
        sprite_setUnk0x2c_register((u32)unk0x2c << 24);
        sprite_setPalette_register(palette);

        clear_event_flag_register(flag2);
        clear_event_flag_register(flag4);

        ST8(ctrl_addr + 8, 4);
        return call_sub_80A5268();
    } else {
        /* trigger clear -> hide the entry */
        set_event_flag_register(flag2);
        set_event_flag_register(flag4);
        ST8(ctrl_addr + 8, 0x10);
        return call_sub_80A5350();
    }
}

/* ---------- thin adapter ---------- */

Next mapEntry_update_80A51F8(void) {
    R0 = update_entry(R5, R7);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── mapEntry_onUpdate_80A5268 ── */
/* Sub-object struct accessed via r7:
 *   +0x0: u16 flag_a
 *   +0x2: u16 flag_b
 *   +0x4: u16 flag_c
 *   +0x7: u8  state
 *   +0x8: u8  anim_a
 *   +0x9: u8  anim_b
 *   +0xE: u16 sound_id
 *
 * Parent struct (r5) fields written:
 *   +0x08: u8  next_sub_state
 *   +0x24: u32 zero
 */

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool test_event_flag_sub_80A5268(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();          /* Z set => flag clear */
}

/* register form: calls SetEventFlag outside this file, which has no typed C interface yet */
static void set_event_flag(u16 flag) {
    R0 = flag;
    GUEST_CALL(SetEventFlag);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_effect_sub_80A5268(u16 sound_id) {
    R0 = sound_id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_set_anim(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_load_animation_data(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static void sprite_update_fn(void) {
    GUEST_CALL(sprite_update);
}

/* register form: calls screenFade_80062C8 outside this file, which has no typed C interface yet */
static u32 screen_fade_80062C8(void) {
    GUEST_CALL(screenFade_80062C8);
    return R0;
}

/* Sub-object step dispatch. r7 is the sub-object, r5 the parent; reads sub-state to
 * drive animation/sound updates and writes the next sub-state to the parent. */
static void sub_80A5268_impl(u32 r7, u32 r5) {
    u16 flag_a = LD16(r7 + 0x0);
    if (test_event_flag_sub_80A5268(flag_a))
        goto done;

    /* Common fade tick: if bit 1 of the fade result is clear, fall into the
     * state-driven branches below. */
    {
        u32 fade = screen_fade_80062C8();
        if (!((fade >> 2) & 1)) {
            u8 sub_state = LD8(r7 + 0x7);
            if (sub_state == 1) {
                u8 anim_a = LD8(r7 + 0x8);
                u8 anim_b = LD8(r7 + 0x9);
                sprite_set_anim(anim_b);
                sprite_load_animation_data();
                set_event_flag(LD16(r7 + 0x4));
                ST32(r5 + 0x24, 0);
                ST8(r5 + 0x8, 0x0C);
                goto done;
            } else {
                u16 sound = LD16(r7 + 0xE);
                if (sound != 0xFFFF)
                    play_sound_effect_sub_80A5268(sound);
            }
        }
    }

    /* Shared tail: set flag_c, then branch on anim_b vs anim_a. */
    set_event_flag(LD16(r7 + 0x4));
    {
        u8 anim_a = LD8(r7 + 0x8);
        u8 anim_b = LD8(r7 + 0x9);
        if (anim_b == anim_a) {
            set_event_flag(LD16(r7 + 0x2));
            ST8(r5 + 0x8, 0x10);
        } else {
            sprite_set_anim(anim_b);
            sprite_load_animation_data();
            ST8(r5 + 0x8, 0x08);
        }
    }

done:
    sprite_update_fn();
}

/* mapEntry_onUpdate_80A5268(r2, r4, r5=parent, r6, r7=sub, r10, lr) -> preserved */
Next mapEntry_onUpdate_80A5268(void) {
    /* r7/r5 are inputs the typed function consumes; lr is left as-is by the original
     * (`pop {pc}` returns via the saved lr). No other output is computed here. */
    sub_80A5268_impl(R7, R5);
    RETURN_TO_CALLER();
}

/* ── sub_80A52E4 ── */
/* sub_80A52E4(r5 = object, r7 = object data, r10 = context):
 * Called once per frame for a sprite-driven object. If the current animation
 * frame reports the "end of animation" marker (bit 0x80 of the frame
 * parameters), raise the event whose id is stored at [r7,#2] and mark the
 * object as finished (0x10 at [r5,#8]). Then advance the sprite animation.
 * Returns r2/r5 as left by sprite_update. */
Next sub_80A52E4(void) {
    GUEST_CALL(sprite_getFrameParameters);

    if (R0 & 0x80) {
        /* Animation reached its final frame: signal the waiting event
           and flag the object as done. */
        R0 = LD16(R7 + 2);
        GUEST_CALL(SetEventFlag);
        ST8(R5 + 8, 0x10);
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_80A5350 ── */
/* sub_80A5350: thin wrapper that calls FreeOverworldMapObject and pops PC.
 * Register contract:
 *   inputs:  r5 lr
 *   outputs: nz c v r0 r1 r2 r3 r5 lr
 *   clobbers: -
 *   preserved: r5
 *
 * The original just tail-calls FreeOverworldMapObject (passing lr through) and
 * returns via `pop {pc}`. Since lr is the caller-supplied return address, the
 * typed function can simply invoke the outer helper with no arguments and let
 * the GUEST_CALL sequence leave whatever FreeOverworldMapObject leaves in the
 * registers/flags. */

/* register form: calls FreeOverworldMapObject outside this file, which has no typed C interface yet */
static void call_FreeOverworldMapObject(void) {
    GUEST_CALL(FreeOverworldMapObject);
}

Next sub_80A5350(void) {
    call_FreeOverworldMapObject();
    RETURN_TO_CALLER();
}

/* ── sub_80A5428 ── */
/* Result registers of the mode handler. sub_80A5428's contract exposes
 * r2/r5/r10/r11: r5 and r10 are passed through to the handler and come
 * back possibly updated, r2 and r11 are results the handler leaves
 * behind. A handler that never touches r2/r11 keeps the value the
 * caller had, which falls out naturally from reading the live
 * registers after the call. */
struct HandlerResult {
    u32 r2;
    u32 r5;
    u32 r10;
    u32 r11;
};

/* register form: the mode handlers are reached through the jump table at
 * off_80A543C ("mov lr, pc / bx r0"), so no generated wrapper covers
 * them. The helper seeds r0 = table entry, r1 = mode byte and r5/r10
 * exactly as the original leaves them at the call, performs that single
 * call, then collects the handler's r2/r5/r10/r11. */
static struct HandlerResult dispatch_mode_handler(u32 handlerAddr, u32 mode,
                                                  u32 objectPtr, u32 r10val) {
    struct HandlerResult out;

    R0 = handlerAddr;
    R1 = mode;
    R5 = objectPtr;
    R10 = r10val;
    GUEST_CALL_ADDR(handlerAddr);

    out.r2 = R2;
    out.r5 = R5;
    out.r10 = R10;
    out.r11 = R11;
    return out;
}

/* Pick the handler for this object's mode. The mode byte at [r5+8] is
 * added to the table address directly (the original does
 * "ldrb r1,[r5,#8]; ldr r0,[r0,r1]" with no scaling), then the loaded
 * entry is called with r0 = entry and r1 = mode. */
static struct HandlerResult dispatch_mode(u32 objectPtr, u32 r10val) {
    u32 mode = LD8(objectPtr + 8);
    u32 handler = LD32(A_off_80A543C + mode);
    return dispatch_mode_handler(handler, mode, objectPtr, r10val);
}

/* sub_80A5428: dispatch to the mode handler of the object in r5
 * (r10 rides along). Results: r2 r5 r10 r11 as the handler leaves them. */
Next sub_80A5428(void) {
    struct HandlerResult res = dispatch_mode(R5, R10);
    R2 = res.r2;
    R5 = res.r5;
    R10 = res.r10;
    R11 = res.r11;
    RETURN_TO_CALLER();
}

/* ── sub_80A5448 ── */
/* sub_80A5448: initialize a battle cursor / cursor-like sprite.
 *   r5 = sprite state struct, r10 = ???, lr = return address
 *   outputs: r2, r5 */
Next sub_80A5448(void) {
    u32 sprite_info;
    u32 data_ptr;

    /* mark sprite state as type 3 */
    ST8(R5, 3);

    /* fetch the sprite info + animation data pointer */
    R0 = R5;
    GUEST_CALL(lookupMapEntryPair_80A54A4);
    sprite_info = R0;
    data_ptr = R1;

    /* initial X / Y positions, given as fixed-point halves (h << 16) */
    ST32(R5 + 0x0C, (u32)LD16(data_ptr) << 16);
    ST32(R5 + 0x10, (u32)LD16(data_ptr + 2) << 16);
    ST32(R5 + 0x14, 0);

    /* load the tile graphics for this sprite */
    R0 = 0x80;
    R1 = LD8(sprite_info);
    R2 = LD8(sprite_info + 1);
    GUEST_CALL(sprite_load);

    /* apply animation 0 (idle / default) */
    ST8(R5 + 5, LD8(sprite_info + 2));
    R0 = LD8(R5 + 5);
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    /* sprite priority 2 (in front of most background layers) */
    R0 = 2;
    GUEST_CALL(sprite_setPriority_8002E14);

    /* palette slot for this sprite */
    ST8(R5 + 5, LD8(sprite_info + 3));
    ST8(R5 + 8, 4);

    /* tail call into the per-frame handler */
    GUEST_CALL(sub_80A5494);
    RETURN_TO_CALLER();
}

/* ── sub_80A5494 ── */
/* sub_80A5494:
 *   inputs:  r5 lr
 *   outputs: r2 r5
 *   clobbers: nz c v r0 r1 r3 lr
 *   preserved: r4 r5
 *
 * Thin trampoline that tail-calls sprite_update. */
Next sub_80A5494(void) {
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── lookupMapEntryPair_80A54A4 ── */
/* lookupMapEntryPair_80A54A4(r10 = Toolkit*, r5 = ?):
 * Looks up a per-map entry table for the current map; returns a base pointer and a
 * string/secondary pointer to one of six fixed tables (two arrays of three pointers each).
 *
 * If the current map group is 0x85 (a special case, likely a battle/mini-game area), the
 * index used is 2; otherwise it comes from the map number byte. The index is then left-
 * shifted by 2 to index a pair of pointer tables. A final byte at offset 4 of *r5 acts as
 * a sub-index, multiplied by 4, and is added into both returned pointers.
 *
 * Outputs: r0 = first pointer + sub-offset; r1 = second pointer + sub-offset.
 */
Next lookupMapEntryPair_80A54A4(void) {
    u32 toolkit_base = R10;
    u32 game_state = LD32(toolkit_base + oToolkit_GameStatePtr);
    u32 idx;
    u32 sub;

    /* Pick the table index: forced to 2 when the map group is 0x85,
     * otherwise read it from the map number. */
    if (LD8(game_state + oGameState_MapGroup) == 0x85) {
        idx = 2;
    } else {
        idx = LD8(game_state + oGameState_MapNumber);
    }

    idx <<= 2;

    /* The sub-offset is a byte at offset 4 of *r5, scaled by 4 (element size). */
    sub = (u32)LD8(R5 + 4) * 4;

    /* Two parallel pointer tables, each with three entries, stored as
     * little-endian u32s in the ROM at the addresses the original ldr-pseudo loads.
     *   0x80A54D8: { 0x80A5358, 0x80A5384, 0x80A5384 }
     *   0x80A54E4: { 0x80A53B0, 0x80A53DC, 0x80A5408 }
     * Index by `idx` (already shifted left by 2) and add `sub`. */
    R0 = LD32(0x80A54D8 + idx) + sub;
    R1 = LD32(0x80A54E4 + idx) + sub;

    RETURN_TO_CALLER();
}

/* ── owMapObject_bbsObjectInit_80A5514 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_sub_80A5514;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_sub_80A5514 call_sprite_load_sub_80A5514(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_sub_80A5514){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_sub_80A5514(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_sub_80A5514;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_sub_80A5514 call_sprite_loadAnimationData_sub_80A5514(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_sub_80A5514){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_sub_80A5514(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result_sub_80A5514;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result_sub_80A5514 call_applyLayerEffectToOWObject_8035694_sub_80A5514(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result_sub_80A5514){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r3; } call_sprite_setPriority_8002E14_result_sub_80A5514;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_sub_80A5514 call_sprite_setPriority_8002E14_sub_80A5514(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_sub_80A5514){ R3 };
}

typedef struct { u32 r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_sub_80A5514;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_80A5514 call_PlaySoundEffect_sub_80A5514(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_80A5514){ R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r2, r10, r11; } call_sub_80A5588_result;
/* register form: generated wrapper: owMapObject_bbsObjectUpdate_80A5588 has no typed C interface yet */
static inline call_sub_80A5588_result call_sub_80A5588(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(owMapObject_bbsObjectUpdate_80A5588);
    return (call_sub_80A5588_result){ R2, R10, R11 };
}

/* Spawn/setup an overworld object: assign a sprite (with special id for map group 0x85),
 * load graphics + animation, apply layer effects and priority, remember position,
 * play the entry sound (unless the "no-sound" flag is set), then advance sub-state
 * to 4 and tail-dispatch to the per-state handler owMapObject_bbsObjectUpdate_80A5588.
 *
 * The original's pop restores r4-r7 (including r5, r6), so the caller-visible r5 is
 * always the input value regardless of what the callees do to it. */

typedef struct {
    u32 r2;
    u32 r5;
    u32 r10;
    u32 r11;
} sub_80A5514_result;

static sub_80A5514_result ow_obj_spawn_setup(u32 ow_obj, u32 src_obj, u32 toolkit) {
    /* State byte -> "spawning" (3). */
    ST8(ow_obj, 3);

    /* Pick sprite id: map group 0x85 gets 0x4E, all others get 0x0B. */
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 map_group = LD8(game_state + oGameState_MapGroup);
    u32 sprite_id = (map_group == 0x85) ? 0x4E : 0x0B;

    /* Load the sprite (layer 0x80, tile 0x1C). Returns updated handle in r5. */
    call_sprite_load_result_sub_80A5514 sl = call_sprite_load_sub_80A5514(0x80, 0x1C, sprite_id, ow_obj);
    u32 cur_r5 = sl.r5;

    /* Set animation index from the source object's byte 5. */
    call_sprite_setAnimation_sub_80A5514(LD8(src_obj + 5), cur_r5);

    /* Load animation data (updates handle), then remove the shadow. */
    call_sprite_loadAnimationData_result_sub_80A5514 lad = call_sprite_loadAnimationData_sub_80A5514(cur_r5);
    cur_r5 = lad.r5;
    call_sprite_noShadow_sub_80A5514(cur_r5);

    /* Apply layer effect to the sprite handle at cur_r5+0xC; may rewrite r5, r6, r10. */
    call_applyLayerEffectToOWObject_8035694_result_sub_80A5514 al =
        call_applyLayerEffectToOWObject_8035694_sub_80A5514(cur_r5 + 0xC);
    cur_r5 = al.r5;
    u32 cur_r6 = al.r6;

    /* Set sprite draw priority (r0 and r5 consumed; no relevant outputs tracked). */
    call_sprite_setPriority_8002E14_sub_80A5514(al.r0, cur_r5);

    /* Store the current position as the "previous position" pair. */
    ST32(cur_r5 + 0x24, LD32(cur_r5 + 0x0C));
    ST32(cur_r5 + 0x28, LD32(cur_r5 + 0x10));

    /* Reset animation frame counters, set state timeout, copy animation index. */
    ST8(cur_r5 + 5, 0);
    ST8(cur_r5 + 6, 0);
    ST32(cur_r5 + 0x2C, 0x12);
    ST8(cur_r5 + 7, LD8(cur_r6 + 5));

    /* If "no-sound" flag bit 0 is clear, play the generic entry sound (0x17F). */
    if ((LD8(cur_r5 + 4) & 1) == 0) {
        call_PlaySoundEffect_result_sub_80A5514 se = call_PlaySoundEffect_sub_80A5514(0x17F, 1);
        cur_r5 = se.r5;
        cur_r6 = se.r6;
        /* For a completely unflagged object, start the source's countdown timer. */
        if (LD8(cur_r5 + 4) == 0)
            ST8(cur_r6 + 0xF, 0x0A);
    }

    /* Advance sub-state to 4. */
    ST8(cur_r5 + 8, 4);

    /* Tail-dispatch to per-state handler; original pop restores r5. */
    call_sub_80A5588_result h = call_sub_80A5588(cur_r5, cur_r6);

    sub_80A5514_result res;
    res.r2 = h.r2;
    res.r5 = ow_obj;
    res.r10 = h.r10;
    res.r11 = h.r11;
    return res;
}

/* owMapObject_bbsObjectInit_80A5514(r5 = ow_object, r6 = source_object, r10 = toolkit) -> r2 r5 r10 r11 */
Next owMapObject_bbsObjectInit_80A5514(void) {
    sub_80A5514_result res = ow_obj_spawn_setup(R5, R6, R10);
    R2 = res.r2;
    R5 = res.r5;
    R10 = res.r10;
    R11 = res.r11;
    RETURN_TO_CALLER();
}

/* ── owMapObject_bbsObjectUpdate_80A5588 ── */
/* owMapObject_bbsObjectUpdate_80A5588: cutscene/sprite step (bank 0x68).
 *   inputs:  r5 r6 r10 lr
 *   outputs: r2 r5 r10 r11
 *   clobbers (scratch for callees): nz c v r0 r1 r3 lr
 *   preserved: r4 r5 r6 r7 r8 r9 r12
 *
 * The original pushes only LR, so the stack frame is just the return address.
 * Behaviour: if either of the two cutscene-script-pos checks is non-zero, or
 * the 0x80 chatbox flag is set, or owPlayer_isBusyOrCutsceneActive_809E462 returns non-zero, skip everything
 * (including sprite_update). Otherwise call reqBBS_updateBobbingCameraAndPlayChime_80A55D4, then test EVENT_4EA /
 * EVENT_4EB; if either is set skip to sprite_update. Otherwise, when the byte
 * at r6+8 is not 2, call checkBbsReqFlag_80A5744. sprite_update always runs past the
 * early-out block.
 */
Next owMapObject_bbsObjectUpdate_80A5588(void) {
    /* If either cutscene-script-position probe is non-zero, bail entirely. */
    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F40);
    if (!ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F58);
    if (!ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    /* Chatbox masked-flag 0x80 set?  bail entirely. */
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (!ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    /* owPlayer_isBusyOrCutsceneActive_809E462 returns non-zero?  bail entirely. */
    GUEST_CALL(owPlayer_isBusyOrCutsceneActive_809E462);
    if (!ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    /* Run reqBBS_updateBobbingCameraAndPlayChime_80A55D4 unconditionally past the early-outs. */
    GUEST_CALL(reqBBS_updateBobbingCameraAndPlayChime_80A55D4);

    /* If EVENT_4EA is set, skip the inner block (still sprite_update). */
    R0 = EVENT_4EA >> 8;
    R1 = EVENT_4EA & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (!ZERO_FLAG()) {
        goto update_and_return;
    }

    /* If EVENT_4EB is set, skip the inner block (still sprite_update). */
    R0 = EVENT_4EB >> 8;
    R1 = EVENT_4EB & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (!ZERO_FLAG()) {
        goto update_and_return;
    }

    /* Inner block: only when the r6+8 byte is not 2. */
    if (LD8(R6 + 8) != 2) {
        GUEST_CALL(checkBbsReqFlag_80A5744);
    }

update_and_return:
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_80A55CC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_FreeOverworldMapObject_result;
/* register form: generated wrapper: FreeOverworldMapObject has no typed C interface yet */
static inline call_FreeOverworldMapObject_result call_FreeOverworldMapObject_sub_80A55CC(u32 r5) {
    R5 = r5;
    GUEST_CALL(FreeOverworldMapObject);
    return (call_FreeOverworldMapObject_result){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Frees an overworld map object: passes the object index in r5 straight
 * through to FreeOverworldMapObject and keeps its r2/r5 results. */
typedef struct {
    u32 r2;
    u32 r5;
} FreeMapObjectResult;

static FreeMapObjectResult free_map_object(u32 object_index) {
    call_FreeOverworldMapObject_result res = call_FreeOverworldMapObject_sub_80A55CC(object_index);
    FreeMapObjectResult out;
    out.r2 = res.r2;
    out.r5 = res.r5;
    return out;
}

Next sub_80A55CC(void) {
    FreeMapObjectResult res = free_map_object(R5);
    R2 = res.r2;
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── sub_80A5690 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r4; } call_sub_80A5724_result;
/* register form: generated wrapper: owMap_getHotspotBoundaryForMapGroup_80A5724 has no typed C interface yet */
static inline call_sub_80A5724_result call_sub_80A5724(u32 r6) {
    R6 = r6;
    GUEST_CALL(owMap_getHotspotBoundaryForMapGroup_80A5724);
    return (call_sub_80A5724_result){ R0, R1, R4 };
}

/* sub_80A5690: Test whether the camera position (stored at r5+0xC, r5+0x10)
 * falls within any rectangular region associated with the current map group.
 * Region tables are indexed by a per-map-group selector. For each region
 * entry, the center (x-8, y+8) is scaled to 16.16 fixed-point, the axis
 * distances to the camera are computed, and owMap_getHotspotBoundaryForMapGroup_80A5724 provides a per-axis
 * radius threshold (also scaled to 16.16). The camera is "inside" that
 * region if both distances are at most the corresponding thresholds.
 *
 * Returns r0 = 1 (inside) or 0 (outside); r2 = the last X threshold
 * computed (leftover from the region loop). */

struct sub_80A5690_result {
    u32 r0;   /* 1 = found inside a region, 0 = not found */
    u32 r2;   /* last scaled X threshold (internal, visible to caller) */
};

static struct sub_80A5690_result test_camera_in_map_regions(u32 camera, u32 param_r6, u32 toolkit) {
    /* Determine which region table to use for this map. */
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 regionIndex;
    if (LD8(gameState + oGameState_MapGroup) == 0x85)
        regionIndex = 2;
    else
        regionIndex = LD8(gameState + 5);

    /* Load the region coordinate table and its entry count. */
    u32 regionTable = LD32(A_off_80A5714 + regionIndex * 4);
    u32 entryCount  = LD8(A_dword_80A5720 + regionIndex);
    ST8(camera + 6, entryCount);

    /* Camera position in 16.16 fixed-point. */
    s32 camX = (s32)LD32(camera + 0xC);
    s32 camY = (s32)LD32(camera + 0x10);

    u32 lastThresholdX = 0;

    /* Do-while loop: always executes the body at least once. */
    for (u32 i = 0; ; i++) {
        /* Region center scaled to 16.16, with -8/+8 offset baked in. */
        s32 boxX = ((s32)(s16)LDS16(regionTable + i * 4) - 8) << 16;
        s32 boxY = ((s32)(s16)LDS16(regionTable + i * 4 + 2) + 8) << 16;

        /* Absolute axis distance from region center to camera. */
        s32 dx = (boxX >= camX) ? boxX - camX : camX - boxX;
        s32 dy = (boxY >= camY) ? boxY - camY : camY - boxY;

        /* register form: owMap_getHotspotBoundaryForMapGroup_80A5724 reads its distance parameters from
         * the guest stack at [SP] and [SP+4]; C has no equivalent way to
         * place arguments below the frame pointer for a stack-padded callee. */
        u32 savedSP = SP;
        SP = savedSP - 8;
        ST32(SP,     (u32)dx);
        ST32(SP + 4, (u32)dy);
        call_sub_80A5724_result thresh = call_sub_80A5724(param_r6);
        dx = (s32)LD32(SP);
        dy = (s32)LD32(SP + 4);
        SP = savedSP;

        /* Scale thresholds to 16.16 and test containment. */
        u32 scaledX = (thresh.r0) << 16;
        u32 scaledY = (thresh.r1) << 16;
        lastThresholdX = scaledX;

        if ((s32)dx <= (s32)scaledX && (s32)dy <= (s32)scaledY)
            return (struct sub_80A5690_result){1, lastThresholdX};

        /* Advance loop counter; exit when all entries checked. */
        if (i + 1 >= (s32)entryCount)
            return (struct sub_80A5690_result){0, lastThresholdX};
    }
}

/* sub_80A5690(r5 = camera, r6, r10 = toolkit, lr) -> r0, r2; preserves r5, r6 */
Next sub_80A5690(void) {
    struct sub_80A5690_result res = test_camera_in_map_regions(R5, R6, R10);
    R0 = res.r0;
    R2 = res.r2;
    RETURN_TO_CALLER();
}

/* ── owMap_getHotspotBoundaryForMapGroup_80A5724 ── */
/* owMap_getHotspotBoundaryForMapGroup_80A5724: look up two bytes from ROM tables indexed by a one-byte selector at r6+5.
 *   inputs:  r6 = pointer to some object/struct whose byte at offset 5 is a selector
 *   outputs: r0 = table_A[selector], r1 = table_B[selector]
 *   preserved: r4 */
Next owMap_getHotspotBoundaryForMapGroup_80A5724(void) {
    u32 selector = LD8(R6 + 5);

    /* dword_80A573C packs four selector bytes (0x18, 0x0C, 0x20, 0x0C) into a u32;
     * dword_80A5740 packs four more (0x0C, 0x20, 0x0C, 0x20). The code reads them one
     * byte at a time, indexed by selector. */
    R0 = LD8((u32)A_dword_80A573C + selector);
    R1 = LD8((u32)A_dword_80A5740 + selector);
    RETURN_TO_CALLER();
}

/* ── checkBbsReqFlag_80A5744 ── */
/* checkBbsReqFlag_80A5744: Check if BBS response byte is equal to a sentinel; if not,
 * set an event flag (4, 234 -> EVENT_4EA).
 *
 *   inputs:  r5, r6, r10, lr
 *   outputs: r5, r10, r11 (preserved)
 *   clobbers: r0, r1, r2, r3, lr, nz, c, v
 */
Next checkBbsReqFlag_80A5744(void) {
    u8 response;

    /* Skip everything if no BBS request is pending. */
    response = LD8(R5 + 5);
    if (response == 0)
        goto done;

    /* Issue the BBS request and compare its return code (minus 0xC0) to
     * the saved request byte; only set the event flag on mismatch. */
    GUEST_CALL(reqBBS_81409E4);
    R0 = (u32)R0 - 0xC0;
    if ((u32)R0 == LD8(R6 + 5))
        goto done;

    /* Set event flag 4 / 234. */
    R0 = 4;
    R1 = 0xEA;
    GUEST_CALL(SetEventFlagFromImmediate);

done:
    RETURN_TO_CALLER();
}

/* ── pickBBSMsgForMapGroup_80A5764 ── */
/* pickBBSMsgForMapGroup_80A5764: picks one of two small tables of (u32, u32) pairs based on the current
 * map group, then returns the second word of the entry selected by reqBBS_81409D0(4).
 *
 * The two source tables each hold 5 (id, msg) pairs laid out as 5 little-endian u32s.
 * The id is in the low 16 bits and the msg in the high 16 bits; reqBBS returns an
 * index 0..4 into whichever table matches the area.
 *
 *   inputs:  r10 lr
 *   outputs: r0 r5 r6
 *   clobbers: nz c v r1 r2 r3
 *   preserved: r4 r5 r6 r7
 */
Next pickBBSMsgForMapGroup_80A5764(void) {
    u32 game_state = LD32(R10 + oToolkit_GameStatePtr);
    u32 map_group  = LD8(game_state + 4);

    /* Map group 0x85 (Beast+ gate links area) uses the alternate table. */
    u32 table_addr = (map_group == 0x85) ? (u32)A_byte_80A579C : (u32)A_byte_80A578C;

    R0 = 4;
    GUEST_CALL(reqBBS_81409D0);
    R0 = LD32(table_addr + (R0 << 2));

    RETURN_TO_CALLER();
}

/* ── battleObject_updateBeastOutPwrAtk_80A5828 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r5, r6, r8, r9, r10, r11, r12; } call_sub_80A58E0_result;
/* register form: generated wrapper: owPlayer_setupPrologue_80A58E0 has no typed C interface yet */
static inline call_sub_80A58E0_result call_sub_80A58E0(u32 r4, u32 r5, u32 r6, u32 r7) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(owPlayer_setupPrologue_80A58E0);
    return (call_sub_80A58E0_result){ R0, R5, R6, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_sub_80A58AC_result;
/* register form: generated wrapper: sub_80A58AC has no typed C interface yet */
static inline call_sub_80A58AC_result call_sub_80A58AC(u32 r4, u32 r5, u32 r6, u32 r7) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(sub_80A58AC);
    return (call_sub_80A58AC_result){ R5, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r4, r5; } call_battle_setPlayerDirectionAndAnimFromAIState_80A5890_result;
/* register form: generated wrapper: battle_setPlayerDirectionAndAnimFromAIState_80A5890 has no typed C interface yet */
static inline call_battle_setPlayerDirectionAndAnimFromAIState_80A5890_result call_battle_setPlayerDirectionAndAnimFromAIState_80A5890(u32 r4, u32 r5, u32 r6) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_setPlayerDirectionAndAnimFromAIState_80A5890);
    return (call_battle_setPlayerDirectionAndAnimFromAIState_80A5890_result){ R4, R5 };
}

/* register form: generated wrapper: battle_copyActorIndexToBattleFlags_80A5A5C has no typed C interface yet */
static inline void call_battle_copyActorIndexToBattleFlags_80A5A5C(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_copyActorIndexToBattleFlags_80A5A5C);
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_sprite_update_result_battleObject_updateBeastOutPwrAtk_80A5828;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_battleObject_updateBeastOutPwrAtk_80A5828 call_sprite_update_battleObject_updateBeastOutPwrAtk_80A5828(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_battleObject_updateBeastOutPwrAtk_80A5828){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Update the BeastOutPwrAtk battle-object state machine each frame.
 * obj   (r5): object pointer — byte 0 is state, byte 9 is the start-up phase counter.
 * owner (r6): owner/parent pointer — byte 7 is the dispatch selector.
 * r4, r7: passed through to child routines; r10 is the battle index. */
static void updateBeastOutPwrAtk(u32 r4, u32 obj, u32 owner, u32 r7) {
    (void)r7;
    u8 dispatch = LD8(owner + 7);
    u32 cur = obj;

    if (dispatch == 0) {
        /* Idle: refresh player direction / animation. */
        cur = call_battle_setPlayerDirectionAndAnimFromAIState_80A5890(r4, cur, owner).r5;
    } else if (dispatch == 3) {
        /* Finishing: put the object into state 1, clear the dispatch byte. */
        ST8(cur, 1);
        ST8(owner + 7, 0);
    } else if (dispatch == 1) {
        /* Start-up: on the first frame copy three words from owner into the object. */
        if (LD8(cur + 9) == 0) {
            ST32(cur + 0xC, LD32(owner + 0x88));
            ST32(cur + 0x10, LD32(owner + 0x8C));
            ST32(cur + 0x14, LD32(owner + 0x90));
            ST8(cur, 3);            /* state → 3 (playing) */
            ST8(cur + 9, 1);        /* mark phase as done */
        }
        /* Advance the playback step. */
        call_sub_80A58E0_result step = call_sub_80A58E0(r4, cur, owner, r7);
        if (step.r0 == 0) {
            /* Playback finished: reset dispatch & phase, fall into idle path. */
            ST8(step.r6 + 7, 0);
            ST8(step.r5 + 9, 0);
            cur = call_battle_setPlayerDirectionAndAnimFromAIState_80A5890(
                      r4, step.r5, step.r6).r5;
        } else {
            cur = step.r5;
        }
    } else {
        /* Other dispatch values: generic play routine. */
        cur = call_sub_80A58AC(r4, cur, owner, r7).r5;
    }

    /* Tail common to every path: refresh linked data, then update the sprite. */
    call_battle_copyActorIndexToBattleFlags_80A5A5C(cur);
    call_sprite_update_battleObject_updateBeastOutPwrAtk_80A5828(cur);
}

Next battleObject_updateBeastOutPwrAtk_80A5828(void) {
    updateBeastOutPwrAtk(R4, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── owPlayer_setupPrologue_80A58E0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sub_80A5954 has no typed C interface yet */
static inline void call_sub_80A5954(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(sub_80A5954);
}

typedef struct { u32 r4, r5; } call_sub_80A596C_result;
/* register form: generated wrapper: owPlayer_setDirectionAnimThunk has no typed C interface yet */
static inline call_sub_80A596C_result call_sub_80A596C(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(owPlayer_setDirectionAnimThunk);
    return (call_sub_80A596C_result){ R4, R5 };
}

typedef struct { u32 r0, r4; } call_sub_80A59B8_result;
/* register form: generated wrapper: sub_80A59B8 has no typed C interface yet */
static inline call_sub_80A59B8_result call_sub_80A59B8(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80A59B8);
    return (call_sub_80A59B8_result){ R0, R4 };
}

/*
 * owPlayer_setupPrologue_80A58E0 - battle setup prologue shared with the routine that follows.
 *
 * It saves r4-r7 and its return address on the stack (the shared continuation
 * at loc_80A58FA pops that same frame when it finishes), then runs three
 * initialisation steps:
 *   1. sub_80A5954 with the incoming r5/r6,
 *   2. owPlayer_setDirectionAnimThunk with its counter argument r4 cleared to 0,
 *   3. sub_80A59B8 with the r5 value step 2 produced.
 * Finally it copies the step 3 result into r2 and branches into the shared
 * body at loc_80A58FA, which produces the outgoing r0/r5/r6/r8-r12 and
 * returns straight to owPlayer_setupPrologue_80A58E0's caller through the saved lr.
 */

/* Address of the shared continuation body (the disassembly's loc_80A58FA;
 * given directly as a constant because that label has no A_ macro here). */
#define LOC_80A58FA 0x080A58FAu

/* Branch into the shared continuation at loc_80A58FA.
 * register form: the continuation is only ever entered by a bare branch in
 * the original, so it has to be invoked through its code address; it reads
 * the result pair r0/r2 and the counters r4/r5, and it pops the frame the
 * setup pushed in order to return to owPlayer_setupPrologue_80A58E0's caller. */
static u32 enter_shared_body(u32 result, u32 counter, u32 value5) {
    R0 = result;
    R2 = result;
    R4 = counter;
    R5 = value5;
    GUEST_CALL_ADDR(LOC_80A58FA);
    return R0;
}

/* register form: the pushed r4-r7/lr frame has to sit on the guest stack,
 * because the shared body at loc_80A58FA pops it to return to the caller. */
static u32 run_setup_and_enter(u32 r4, u32 r5, u32 r6, u32 r7, u32 return_addr) {
    /* push {r4-r7,lr} - kept for the shared body's epilogue. */
    SP -= 20;
    ST32(SP, r4);
    ST32(SP + 4, r5);
    ST32(SP + 8, r6);
    ST32(SP + 12, r7);
    ST32(SP + 16, return_addr);

    /* Step 1 works on r5/r6 as passed in. */
    call_sub_80A5954(r5, r6);

    /* Step 2 starts with its counter argument r4 cleared to 0; it updates
     * r4/r5, and step 3 continues from the r5 it leaves behind. */
    call_sub_80A596C_result step2 = call_sub_80A596C(0, r5);

    /* Step 3 yields the value the shared body receives in r0 (the original
     * copies it to r2 with "mov r2, r0"); its r4 is live at the branch. */
    call_sub_80A59B8_result step3 = call_sub_80A59B8(step2.r5);

    return enter_shared_body(step3.r0, step3.r4, step2.r5);
}

Next owPlayer_setupPrologue_80A58E0(void) {
    R0 = run_setup_and_enter(R4, R5, R6, R7, LR);
    RETURN_TO_CALLER();
}

/* ── addToWordAt_80A6E70 ── */
/* Read the 32-bit word at [r5 + r6], add r0 to it, and store the sum back.
 * Returns the old value (the original leaves it in r1); the C and V flags
 * come from the ADDS. */
static u32 add_to_word(u32 base, u32 offset, u32 addend) {
    u32 old = LD32(base + offset);
    ST32(base + offset, old + addend);
    return old;
}

/* addToWordAt_80A6E70(r0 = addend, r5 = base, r6 = offset) -> r1 = old value, flags C/V */
Next addToWordAt_80A6E70(void) {
    u32 addend = R0;
    u32 old = add_to_word(R5, R6, addend);
    R1 = old;
    SET_FLAGS_ADD(addend, old);
    RETURN_TO_CALLER();
}

/* ── battleObject_initBeastSpEnemy_80A6EBC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_battleObject_initBeastSpEnemy_80A6EBC;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battleObject_initBeastSpEnemy_80A6EBC call_sprite_load_battleObject_initBeastSpEnemy_80A6EBC(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battleObject_initBeastSpEnemy_80A6EBC){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_initBeastSpEnemy_80A6EBC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_battleObject_initBeastSpEnemy_80A6EBC;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_initBeastSpEnemy_80A6EBC call_sprite_loadAnimationData_battleObject_initBeastSpEnemy_80A6EBC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_initBeastSpEnemy_80A6EBC){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battleObject_initBeastSpEnemy_80A6EBC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result call_applyLayerEffectToOWObject_8035694(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r1, r2; } call_sprite_setPriority_8002E14_result_battleObject_initBeastSpEnemy_80A6EBC;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_battleObject_initBeastSpEnemy_80A6EBC call_sprite_setPriority_8002E14_battleObject_initBeastSpEnemy_80A6EBC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_battleObject_initBeastSpEnemy_80A6EBC){ R1, R2 };
}

/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline void call_sprite_setPalette_battleObject_initBeastSpEnemy_80A6EBC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
}

/* register form: generated wrapper: sub_80A70C0 has no typed C interface yet */
static inline u32 call_sub_80A70C0(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80A70C0);
    return R0;
}

typedef struct { u32 r3; bool z, n; } call_TestEventFlagFromImmediate_result_battleObject_initBeastSpEnemy_80A6EBC;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_battleObject_initBeastSpEnemy_80A6EBC call_TestEventFlagFromImmediate_battleObject_initBeastSpEnemy_80A6EBC(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_battleObject_initBeastSpEnemy_80A6EBC){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r4, r5, r6, r7; } call_battle_loadBeastSpEnemyStats_result;
/* register form: generated wrapper: battle_loadBeastSpEnemyStats has no typed C interface yet */
static inline call_battle_loadBeastSpEnemyStats_result call_battle_loadBeastSpEnemyStats(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_loadBeastSpEnemyStats);
    return (call_battle_loadBeastSpEnemyStats_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r2, r3, r4, r5, r10; } call_battleObject_updateAnimation_80A6F2C_result;
/* register form: generated wrapper: battleObject_updateAnimation_80A6F2C has no typed C interface yet */
static inline call_battleObject_updateAnimation_80A6F2C_result call_battleObject_updateAnimation_80A6F2C(u32 r4, u32 r5, u32 r6, u32 r7) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battleObject_updateAnimation_80A6F2C);
    return (call_battleObject_updateAnimation_80A6F2C_result){ R2, R3, R4, R5, R10 };
}

/* Initialize a Beast-type overworld enemy sprite: load graphics/animation,
 * apply layer effect, set priority and palette, then copy position data from
 * the beast parameter block (if available), optionally load battle stats for
 * event flag 0x5E5, and spawn the object via the animation update. */
static void init_beast_sp_enemy(u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    /* Mark sprite state as "loading" (3). */
    ST8(r5, 3);

    /* Load sprite graphics: VRAM slot 0x80, palette 0x1C, slot 0x2F. */
    call_sprite_load_result_battleObject_initBeastSpEnemy_80A6EBC sl = call_sprite_load_battleObject_initBeastSpEnemy_80A6EBC(0x80, 0x1C, 0x2F, r5);
    r4 = sl.r4;
    r5 = sl.r5;

    /* Set initial animation frame to 0. */
    call_sprite_setAnimation_battleObject_initBeastSpEnemy_80A6EBC(0, r5);

    /* Load the full animation sequence data. */
    call_sprite_loadAnimationData_result_battleObject_initBeastSpEnemy_80A6EBC lan = call_sprite_loadAnimationData_battleObject_initBeastSpEnemy_80A6EBC(r5);
    r4 = lan.r4;
    r5 = lan.r5;

    /* Disable shadow rendering for this sprite. */
    call_sprite_noShadow_battleObject_initBeastSpEnemy_80A6EBC(r5);

    /* Apply the overworld layer-effect to the object field at sprite + 0xC. */
    call_applyLayerEffectToOWObject_8035694_result lye =
        call_applyLayerEffectToOWObject_8035694(r5 + 0xC);
    r4 = lye.r4;
    r5 = lye.r5;
    r6 = lye.r6;
    r7 = lye.r7;

    /* Set OAM draw priority. */
    call_sprite_setPriority_8002E14_battleObject_initBeastSpEnemy_80A6EBC(lye.r0, r5);

    /* Assign palette index 1. */
    call_sprite_setPalette_battleObject_initBeastSpEnemy_80A6EBC(1, r5);

    /* Retrieve the beast parameter block (position table pointer). */
    u32 params = call_sub_80A70C0(r5);
    if (params == 0)
        return;  /* No data available – exit early. */

    /* Store X and Y positions (16-bit, shifted into upper halfword). */
    r4 = params;
    ST32(r5 + 0xC, (u32)LD16(r4) << 16);
    ST32(r5 + 0x10, (u32)LD16(r4 + 2) << 16);

    /* Zero-initialize remaining movement/animation fields. */
    ST32(r5 + 0x14, 0);
    ST32(r5 + 0x2C, 0);
    ST32(r5 + 0x34, 0);
    ST8(r5 + 6, 0);

    /* Behavior code 0x11 = Beast-type enemy. */
    ST32(r5 + 0x38, 0x11);

    /* Load battle-specific stats only when event flag 0x5E5 is set
     * (TestEventFlagFromImmediate returns Z clear ⇒ flag set). */
    if (!call_TestEventFlagFromImmediate_battleObject_initBeastSpEnemy_80A6EBC(5, 0xE5).z) {
        call_battle_loadBeastSpEnemyStats_result st = call_battle_loadBeastSpEnemyStats(r5);
        r4 = st.r4;
        r5 = st.r5;
        r6 = st.r6;
        r7 = st.r7;
    }

    /* Mark animation state = 4 and spawn the object. */
    ST8(r5 + 8, 4);
    call_battleObject_updateAnimation_80A6F2C(r4, r5, r6, r7);
}

/* battleObject_initBeastSpEnemy_80A6EBC(r4, r5, r6, r7, r10, lr) */
Next battleObject_initBeastSpEnemy_80A6EBC(void) {
    u32 saved_r4 = R4, saved_r5 = R5;  /* pop {r4-r7,pc} restores these */
    init_beast_sp_enemy(R4, R5, R6, R7, R10);
    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── ow_dispatchObjectHandler_80A77A8 ── */
/* Jump table at off_80A77BC, indexed by the type byte stored at object+08.
 * Each entry is a routine called with r0 = its own table address and
 * r1 = the type byte (exactly what the original's `mov lr, pc; bx r0`
 * leaves in the registers). */
static void call_table_handler(u32 handler, u32 selection) { /* register form: calls through a guest code address (noted by the tool) */
    R0 = handler;
    R1 = selection;
    GUEST_CALL_ADDR(handler);
}

/* Dispatch the per-type routine for the object in r5. The routine's results
 * are whatever it leaves in r2-r5 and r8-r12; since the call goes through
 * the real register file, those pass straight through to the caller. */
static void sub_80A77A8_dispatch(u32 object) {
    u32 selection = LD8(object + 8);
    u32 handler = LD32(A_off_80A77BC + selection);
    call_table_handler(handler, selection);
}

/* ow_dispatchObjectHandler_80A77A8(r5 = object) -> r2 r3 r4 r5 r8 r9 r10 r11 r12 from the callee */
Next ow_dispatchObjectHandler_80A77A8(void) {
    sub_80A77A8_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_tickAnimationWithEndEvent_80A77FC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_sprite_getFrameParameters_result_sub_80A77FC;
/* register form: generated wrapper: sprite_getFrameParameters has no typed C interface yet */
static inline call_sprite_getFrameParameters_result_sub_80A77FC call_sprite_getFrameParameters_sub_80A77FC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_getFrameParameters);
    return (call_sprite_getFrameParameters_result_sub_80A77FC){ R0 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_sprite_update_result_sub_80A77FC;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_sub_80A77FC call_sprite_update_sub_80A77FC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_sub_80A77FC){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Advance one sprite's animation: query its frame parameters and, when the
 * "frame finished" bit is set, poke the event byte at sprite+8, then run the
 * sprite's per-frame update.
 *
 * Register contract (from the callers):
 *   inputs:  r5 (sprite work area)
 *   outputs: r2 r3 r4 r5 as left by sprite_update
 *   clobbers (unused by callers): nz c v r0 r1 lr
 *   preserved: r4 r5 */

typedef struct {
    u32 r2;
    u32 r3;
    u32 r4;
    u32 r5;
} SpriteTickRegs;

static SpriteTickTicks_helper_unused; /* (placeholder removed below) */

static SpriteTickRegs sub_80A77FC_typed(u32 sprite)
{
    /* Bit 7 of the frame parameters marks the end of the animation cycle. */
    u32 params = call_sprite_getFrameParameters_sub_80A77FC(sprite).r0;
    if (params & 0x80)
        ST8(sprite + 8, 8);

    call_sprite_update_result_sub_80A77FC upd = call_sprite_update_sub_80A77FC(sprite);

    SpriteTickRegs out;
    out.r2 = upd.r2;
    out.r3 = upd.r3;
    out.r4 = upd.r4;
    out.r5 = upd.r5;
    return out;
}

Next sprite_tickAnimationWithEndEvent_80A77FC(void)
{
    SpriteTickRegs out = sub_80A77FC_typed(R5);
    R2 = out.r2;
    R3 = out.r3;
    R4 = out.r4;
    R5 = out.r5;
    RETURN_TO_CALLER();
}

/* ── object_startFacingStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_object_startFacingStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r5; } call_sprite_loadAnimationData_result_object_startFacingStep;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_object_startFacingStep call_sprite_loadAnimationData_object_startFacingStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_object_startFacingStep){ R5 };
}

/* register form: generated wrapper: addToByteAtR6_80A7BA2 has no typed C interface yet */
static inline void call_sub_80A7BA2(u32 r0, u32 r6) {
    R0 = r0;
    R6 = r6;
    GUEST_CALL(addToByteAtR6_80A7BA2);
}

/* object_startFacingStep: 0x080A79DC
 * Begin a facing step for a battle object: pick the animation for the new
 * facing direction, compute the scaled step offset from the direction table,
 * seed the object's timer with a BIOS Div, and snapshot the start position. */

#define DIV_FIXED_POINT 0x4000u

/* register form: BIOS Div via svc 6 — dividend in R0, divisor in R1;
 * returns the quotient (R0) and reports the remainder (R1) through *rem */
static u32 bios_div_rem(u32 dividend, u32 divisor, u32 *rem) {
    R0 = dividend;
    R1 = divisor;
    swi_call(6);
    *rem = R1;
    return R0;
}

/* r5 = sprite context, r6 = battle object, r7 = step descriptor. Returns 1. */
static u32 start_facing_step(u32 sprite, u32 object, u32 step) {
    /* Face the direction given by the descriptor and load its animation */
    u32 anim = LD8(step + 1);
    ST8(object + 2, anim);

    call_sprite_setAnimation_object_startFacingStep(anim, sprite);
    call_sprite_loadAnimationData_object_startFacingStep(sprite);

    /* Step offset for this direction, taken from the signed byte pair at
     * byte_80A7A38[dir*2], shifted into the high half and scaled by the
     * descriptor's step length, then offset by the sprite's position */
    u32 dir = LD8(object + 2);
    u32 entry = A_byte_80A7A38 + dir * 2;
    u32 scale = LD8(step + 3);
    u32 step_dx = (u32)LDS8(entry) << 16;
    u32 step_dy = (u32)LDS8(entry + 1) << 16;
    ST32(object + 0x1C, step_dx * scale + LD32(sprite + 0xC));
    ST32(object + 0x20, step_dy * scale + LD32(sprite + 0x10));

    /* BIOS Div(0x4000, flip): the quotient seeds the object's timer and
     * 0x4000 minus the remainder becomes the Z-sort halfword */
    u32 flip = LD8(step + 2);
    u32 rem;
    u32 quotient = bios_div_rem(DIV_FIXED_POINT, flip, &rem);
    ST32(object + 0x24, quotient);
    ST16(object + 6, (u16)(DIV_FIXED_POINT - rem));

    /* Remember where the step starts */
    ST32(object + 0x14, LD32(sprite + 0xC));
    ST32(object + 0x18, LD32(sprite + 0x10));

    /* Refresh the object's sprite state (mode 4) */
    call_sub_80A7BA2(4, object);

    return 1;
}

/* object_startFacingStep(r5 = sprite, r6 = object, r7 = step) -> r0 = 1, NZ set */
Next object_startFacingStep(void) {
    R0 = start_facing_step(R5, R6, R7);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── camera_sineEasePanStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6; bool z, n; } call_battle_isInputAllowedOrBattleBusy_80A7C58_result_camera_sineEasePanStep;
/* register form: generated wrapper: battle_isInputAllowedOrBattleBusy_80A7C58 has no typed C interface yet */
static inline call_battle_isInputAllowedOrBattleBusy_80A7C58_result_camera_sineEasePanStep call_battle_isInputAllowedOrBattleBusy_80A7C58_camera_sineEasePanStep(void) {
    GUEST_CALL(battle_isInputAllowedOrBattleBusy_80A7C58);
    return (call_battle_isInputAllowedOrBattleBusy_80A7C58_result_camera_sineEasePanStep){ R5, R6, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: addToByteAtR6_80A7BA2 has no typed C interface yet */
static inline void call_sub_80A7BA2_camera_sineEasePanStep(u32 r0, u32 r6) {
    R0 = r0;
    R6 = r6;
    GUEST_CALL(addToByteAtR6_80A7BA2);
}

/* camera_sineEasePanStep: drives a sinusoidal ease-out pan for a battle object.
 *
 * Calls battle_isInputAllowedOrBattleBusy to get temporary entity/target pointers;
 * uses those for memory reads and writes, but the stack pop restores r5/r6 to
 * their entry values (so they are "preserved outputs"). Returns 0 when the motion
 * is active (or blocked) and 1 when it has completed and the object snapped to target.
 * Flags reflect the return value: mov r0, #N at the end of each path. */

typedef struct {
    u32 flag;  /* 0 or 1; adapter uses this for NZ (from mov r0, #flag) */
} camera_pan_t;

static camera_pan_t step_camera_pan(u32 entity_ptr, u32 target_ptr) {
    camera_pan_t out;

    call_battle_isInputAllowedOrBattleBusy_80A7C58_result_camera_sineEasePanStep busy =
        call_battle_isInputAllowedOrBattleBusy_80A7C58_camera_sineEasePanStep();

    /* Callee provides up-to-date working pointers (not returned to caller;
     * the pop {r4-r7} restores original r5/r6 on every exit). */
    u32 ent = busy.r5;
    u32 tgt = busy.r6;

    if (!busy.z) {
        /* Motion not active / input blocked: return 0. */
        out.flag = 0;
        return out;
    }

    /* Remaining = timer (u16) - expired threshold (u32), signed compare. */
    s32 remaining = (s32)((u32)LD16(tgt + 6) - LD32(tgt + 0x24));

    if (remaining <= 0) {
        /* Easing finished: snap to final target position and notify. */
        ST32(ent + 0xC, LD32(tgt + 0x1C));
        ST32(ent + 0x10, LD32(tgt + 0x20));
        call_sub_80A7BA2_camera_sineEasePanStep(1, tgt);
        out.flag = 1;
        return out;
    }

    /* Still easing: update timer, compute sinusoidal ease factor. */
    ST16(tgt + 6, (u16)remaining);

    /* sinTable index: (64 - remaining/256) * 2 (entries are 16-bit) */
    s32 ease = (s32)LDS16(A_math_sinTable + (0x40u - ((u32)remaining >> 8)) * 2u);

    /* Step = (target - start) >> 8 * ease, then add start back. */
    u32 step_x = (u32)(((s32)(LD32(tgt + 0x1C) - LD32(tgt + 0x14)) >> 8) * ease);
    u32 step_y = (u32)(((s32)(LD32(tgt + 0x20) - LD32(tgt + 0x18)) >> 8) * ease);

    ST32(ent + 0xC, step_x + LD32(tgt + 0x14));
    ST32(ent + 0x10, step_y + LD32(tgt + 0x18));

    out.flag = 0;
    return out;
}

/* camera_sineEasePanStep(r5=entity, r6=target, r10=busy-check arg) -> r5 r6 nz */
Next camera_sineEasePanStep(void) {
    camera_pan_t res = step_camera_pan(R5, R6);
    /* r5 and r6 are preserved (restored by pop) but listed as outputs. */
    SET_FLAGS_NZ(res.flag);
    RETURN_TO_CALLER();
}

/* ── storeByteAt_r6_80A7B9E ── */
/* Store one byte into memory at [r6]: a simple setter for an object field. */
static void store_byte_at(u32 address, u32 value) {
    ST8(address, value);
}

/* storeByteAt_r6_80A7B9E(r0 = value, r6 = destination address) -> returns to caller */
Next storeByteAt_r6_80A7B9E(void) {
    store_byte_at(R6, R0);
    RETURN_TO_CALLER();
}

/* ── addToByteAtR6_80A7BA2 ── */
/* Byte-size "add to memory" helper: the byte at addr is loaded, addend is
 * added to it (full 32-bit ADDS, so C/V reflect that addition), and the low
 * byte is written back. Returns the full sum, which the original leaves in r1. */
static u32 add_to_byte(u32 addr, u32 addend) {
    u32 sum = (u32)LD8(addr) + addend;
    ST8(addr, sum);
    return sum;
}

/* addToByteAtR6_80A7BA2(r0 = addend, r6 = address of the byte) -> r1 = sum, flags from ADDS */
Next addToByteAtR6_80A7BA2(void) {
    u32 sum = add_to_byte(R6, R0);
    R1 = sum;
    /* ADDS r1, r1, r0: recompute the operands from the exact sum so the
     * adapter can set C/V exactly as the original add did. */
    SET_FLAGS_ADD(sum - R0, R0);
    RETURN_TO_CALLER();
}

/* ── battle_checkBeastSpTrigger_80A7BC0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6; bool z, n; } call_battle_isInputAllowedOrBattleBusy_80A7C58_result;
/* register form: generated wrapper: battle_isInputAllowedOrBattleBusy_80A7C58 has no typed C interface yet */
static inline call_battle_isInputAllowedOrBattleBusy_80A7C58_result call_battle_isInputAllowedOrBattleBusy_80A7C58(void) {
    GUEST_CALL(battle_isInputAllowedOrBattleBusy_80A7C58);
    return (call_battle_isInputAllowedOrBattleBusy_80A7C58_result){ R5, R6, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r4, r5, r7; } call_beast_getMapGroupIdBase_result;
/* register form: generated wrapper: beast_getMapGroupIdBase has no typed C interface yet */
static inline call_beast_getMapGroupIdBase_result call_beast_getMapGroupIdBase(u32 r0) {
    R0 = r0;
    GUEST_CALL(beast_getMapGroupIdBase);
    return (call_beast_getMapGroupIdBase_result){ R0, R4, R5, R7 };
}

typedef struct { u32 r1, r3; } call_SetEventFlag_result;
/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline call_SetEventFlag_result call_SetEventFlag_battle_checkBeastSpTrigger_80A7BC0(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
    return (call_SetEventFlag_result){ R1, R3 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_battle_checkBeastSpTrigger_80A7BC0;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battle_checkBeastSpTrigger_80A7BC0 call_PlaySoundEffect_battle_checkBeastSpTrigger_80A7BC0(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battle_checkBeastSpTrigger_80A7BC0){ R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r3; } call_sub_81418F0_result;
/* register form: generated wrapper: sub_81418F0 has no typed C interface yet */
static inline call_sub_81418F0_result call_sub_81418F0(void) {
    GUEST_CALL(sub_81418F0);
    return (call_sub_81418F0_result){ R3 };
}

typedef struct { u32 r1, r2, r5, r7; } call_battle_getAIDataVersionFields_81418E4_result;
/* register form: generated wrapper: battle_getAIDataVersionFields_81418E4 has no typed C interface yet */
static inline call_battle_getAIDataVersionFields_81418E4_result call_battle_getAIDataVersionFields_81418E4(void) {
    GUEST_CALL(battle_getAIDataVersionFields_81418E4);
    return (call_battle_getAIDataVersionFields_81418E4_result){ R1, R2, R5, R7 };
}

typedef struct { u32 r0, r4, r5; } call_battle_getEnemyIndexFromMapGroup_8142080_result;
/* register form: generated wrapper: battle_getEnemyIndexFromMapGroup_8142080 has no typed C interface yet */
static inline call_battle_getEnemyIndexFromMapGroup_8142080_result call_battle_getEnemyIndexFromMapGroup_8142080(void) {
    GUEST_CALL(battle_getEnemyIndexFromMapGroup_8142080);
    return (call_battle_getEnemyIndexFromMapGroup_8142080_result){ R0, R4, R5 };
}

typedef struct { u32 r0, r4, r5; } call_getSetFlagCount_result;
/* register form: generated wrapper: getSetFlagCount has no typed C interface yet */
static inline call_getSetFlagCount_result call_getSetFlagCount(u32 r0) {
    R0 = r0;
    GUEST_CALL(getSetFlagCount);
    return (call_getSetFlagCount_result){ R0, R4, R5 };
}

typedef struct { u32 r1, r3; bool c, v; } call_SetEventFlagFromImmediate_result;
/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline call_SetEventFlagFromImmediate_result call_SetEventFlagFromImmediate(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
    return (call_SetEventFlagFromImmediate_result){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: StartCutscene has no typed C interface yet */
static inline void call_StartCutscene(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(StartCutscene);
}

/* battle_checkBeastSpTrigger_80A7BC0 – Beast-area special trigger check.
 *
 * Called with r5 = trigger object pointer, r10 = toolkit. Returns early unless
 * input is allowed and the battle is idle, the player's game-state ID matches
 * the trigger's ID, and the player is inside the trigger's bounding box.
 * On success: resolves a map group → event flag, sets it, marks the trigger
 * as fired (byte 8 ← 8), plays a sound effect, validates the AI-data flag
 * count against a version field, sets event 0x1731, and launches the Beast
 * cutscene script.
 *
 * All guest registers are maintained by the generated call wrappers.
 */
static void check_beast_sp_trigger(u32 toolkit)
{
    /* 1. Precondition: input allowed and battle not busy.
     *    bne → if Z is clear (not allowed / busy), return without triggering. */
    call_battle_isInputAllowedOrBattleBusy_80A7C58_result pre
        = call_battle_isInputAllowedOrBattleBusy_80A7C58();
    if (!pre.z)
        return;

    u32 trigger = pre.r5;   /* pointer to the active trigger object */

    /* 2. Toolkit → GameState → OverworldPlayerObject. */
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 player     = LD32(game_state + oGameState_OverworldPlayerObjectPtr);

    /* 3. Game-state ID must match the trigger's target ID (offset 0x14). */
    if (LD32(trigger + 0x14) != LD32(player + 0x24))
        return;

    /* 4. Bounding-box check:
     *      origin = (dword_80A7C44, dword_80A7C48)    – both 0x80000
     *      half   = (dword_80A7C4C, dword_80A7C50)    – both 0x100000
     *    rel = (player_pos + origin) − trigger_pos;   need 0 ≤ rel ≤ half   */
    s32 rel_x = (s32)(LD32(player + 0x1C) + LD32(A_dword_80A7C44))
              - (s32)LD32(trigger + 0xC);
    s32 rel_y = (s32)(LD32(player + 0x20) + LD32(A_dword_80A7C48))
              - (s32)LD32(trigger + 0x10);
    if (rel_x < 0)
        return;
    if (rel_y < 0)
        return;
    if (rel_x > (s32)LD32(A_dword_80A7C4C))
        return;
    if (rel_y > (s32)LD32(A_dword_80A7C50))
        return;

    /* 5. Resolve map-group id from trigger byte id, set the event flag. */
    call_beast_getMapGroupIdBase(LD8(trigger + 4));
    call_SetEventFlag_battle_checkBeastSpTrigger_80A7BC0(0);

    /* 6. Mark trigger as fired. */
    ST8(trigger + 8, 8);

    /* 7. Activation sound (r2 is whatever the preceding calls left). */
    call_PlaySoundEffect_battle_checkBeastSpTrigger_80A7BC0(SOUND_UNK_73, 0);

    /* 8. Validate AI-data version fields against the number of set flags. */
    call_sub_81418F0();
    call_battle_getAIDataVersionFields_81418E4_result ai_ver
        = call_battle_getAIDataVersionFields_81418E4();

    /*    battle_getEnemyIndexFromMapGroup may touch r4, but we track
     *    the version-field value in a local, matching the original's
     *    r4 flow (mov r4, r2 before the call, preserved by getSetFlagCount). */
    u32 field_count = ai_ver.r2;

    call_battle_getEnemyIndexFromMapGroup_8142080();

    call_getSetFlagCount_result set_cnt = call_getSetFlagCount(0);
    if (set_cnt.r0 != field_count)
        return;

    /* 9. Set the cutscene event flag (0x1731) via the immediate helper. */
    call_SetEventFlagFromImmediate(0x17, 0x31);

    /* 10. Launch the Beast-area cutscene. */
    call_battle_getEnemyIndexFromMapGroup_8142080_result final_grp
        = call_battle_getEnemyIndexFromMapGroup_8142080();
    call_StartCutscene(A_byte_8089DD8, final_grp.r0);
}

/* battle_checkBeastSpTrigger_80A7BC0(r5 = trigger, r10 = toolkit, lr)
 * → r5 r6 r10 preserved */
Next battle_checkBeastSpTrigger_80A7BC0(void) {
    check_beast_sp_trigger(R10);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchAIActionByAction_80A7E4C ── */
/* battle_dispatchAIActionByAction_80A7E4C
 *
 * Dispatches to one of three AI-action handlers (ow_animWaitStep,
 * object_waitThenSetAnim, sub_80A7EBE) chosen by a byte-offset into a
 * jump table at A_off_80A7E60.  The byte at [r5+9] holds the table
 * byte-offset directly (0, 4, or 8), not an index.
 *
 * The handler runs with the live guest register file and leaves its
 * results in r8-r12 / r10.  r4-r7 are saved and restored around the
 * call (the original's push {r4-r7,lr} / pop {r4-r7,pc}), so only
 * r5 survives as both an unchanged output and a preserved register.
 */

struct AIActionOut {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
};

/* register form: the three handlers are reachable only through a jump-table
 * address (no typed wrapper exists), they read/write the live register file,
 * and their results land in r8-r12 which GUEST_CALL_ADDR cannot return.
 * r4 and r6 are saved across the call because the original's push/pop
 * protects them but a guest-register call does not. */
static struct AIActionOut ai_dispatch_by_kind(u32 object, u32 arg7, u32 arg10) {
    u32 table_off = LD8(object + 9);               /* byte offset (0/4/8) */
    u32 handler   = LD32(A_off_80A7E60 + table_off); /* thumb code address  */

    u32 save_r4 = R4;
    u32 save_r6 = R6;

    /* Set up the registers the handler expects to find live */
    R5  = object;
    R7  = arg7;
    R10 = arg10;

    GUEST_CALL_ADDR(handler);

    /* Restore preserved registers (original's pop {r4-r7}) */
    R4 = save_r4;
    R5 = object;
    R6 = save_r6;
    R7 = arg7;

    struct AIActionOut out;
    out.r5  = object;           /* restored to the entry value */
    out.r8  = R8;
    out.r9  = R9;
    out.r10 = R10;
    out.r11 = R11;
    out.r12 = R12;
    return out;
}

/* battle_dispatchAIActionByAction_80A7E4C(r5 = object, r7, r10) -> r5, r8-r12 */
Next battle_dispatchAIActionByAction_80A7E4C(void) {
    struct AIActionOut out = ai_dispatch_by_kind(R5, R7, R10);
    R5  = out.r5;
    R8  = out.r8;
    R9  = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── sprite_setPalette8IfFrameFlagSet_80A7F14 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3; bool c, v; } call_sprite_getFrameParameters_result_sub_80A7F14;
/* register form: generated wrapper: sprite_getFrameParameters has no typed C interface yet */
static inline call_sprite_getFrameParameters_result_sub_80A7F14 call_sprite_getFrameParameters_sub_80A7F14(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_getFrameParameters);
    return (call_sprite_getFrameParameters_result_sub_80A7F14){ R0, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Check a sprite's frame parameters and mark the sprite (r5) with the
 * "palette 8" byte at offset 9 when the frame's high flag bit is set. */
static void mark_sprite_palette_flag(u32 sprite) {
    u32 params = call_sprite_getFrameParameters_sub_80A7F14(sprite).r0;

    if (params & 0x80)
        ST8(sprite + 9, 8);
}

/* sprite_setPalette8IfFrameFlagSet_80A7F14(r5 = sprite object) -> nothing */
Next sprite_setPalette8IfFrameFlagSet_80A7F14(void) {
    mark_sprite_palette_flag(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_applyFormPalette_80A8020 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result call_sprite_setPalette_sub_80A8020(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sprite_applyFormPalette_80A8020 -- apply the sprite palette matching a work struct's current form.
 *
 * r7 points to a struct whose byte at +2 holds a form index.  That index is
 * looked up in the byte table at off_80A8034 to get a palette number, which
 * sprite_setPalette applies to the sprite data described by r5.
 * Both r5 and r7 are outputs but the original leaves them unchanged
 * (it merely saves/restores them around the call).
 */
static u32 apply_form_palette(u32 sprite, u32 work) {
    u32 formIndex = LD8(work + 2);                       /* ldrb r0, [r7,#2]   */
    u32 palette   = LD8(A_off_80A8034 + formIndex);      /* ldrb r0, [r1,r0]   */
    call_sprite_setPalette_sub_80A8020(palette, sprite);             /* bl sprite_setPalette (r0 = palette, r5 = sprite) */
    return sprite;                                       /* r5 passes through unchanged */
}

/* sprite_applyFormPalette_80A8020(r5 = sprite, r7 = work struct) -> r5 r7 unchanged */
Next sprite_applyFormPalette_80A8020(void) {
    R5 = apply_form_palette(R5, R7);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchCustomJumptableByIndex_80A8208 ── */
/* Custom jumptable dispatch for a battle object (0x080A8208).
 *
 * r5 points at an object whose byte at +8 selects an entry from a three-way
 * handler table; the selected handler is called with r0 = its own address,
 * r1 = the selector byte, r5 = the object and r6 = the shared work area
 * (unk_2000B40). Whatever the handler leaves in r2-r5 and r8-r12 is this
 * function's output, so those registers must pass straight through. */

/* register form: dispatch through a jumptable address. The callee's results
 * in r2-r5/r8-r12 are our caller's outputs and no typed call form can carry
 * them, so the guest register file itself must carry them across the call. */
static void run_jumptable_handler(u32 handler, u32 index, u32 object) {
    R0 = handler;                       /* the original calls with r0 = target */
    R1 = index;
    R5 = object;
    R6 = A_unk_2000B40;                 /* work area the handlers expect */
    GUEST_CALL_ADDR(handler);
}

static void dispatch_object_handler(u32 object) {
    u32 index = LD8(object + 8);
    u32 handler = LD32(A_off_80A821C + index);
    run_jumptable_handler(handler, index, object);
}

Next battle_dispatchCustomJumptableByIndex_80A8208(void) {
    dispatch_object_handler(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_setupBeastSprite ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_battleObject_setupBeastSprite;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battleObject_setupBeastSprite call_sprite_load_battleObject_setupBeastSprite(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battleObject_setupBeastSprite){ R4, R5 };
}

typedef struct { u32 r5; } call_ow_swapMapEventFlags_result;
/* register form: generated wrapper: ow_swapMapEventFlags has no typed C interface yet */
static inline call_ow_swapMapEventFlags_result call_ow_swapMapEventFlags(u32 r5) {
    R5 = r5;
    GUEST_CALL(ow_swapMapEventFlags);
    return (call_ow_swapMapEventFlags_result){ R5 };
}

/* register form: generated wrapper: battleObject_checkSetPriorityByEventFlag_80A82DC has no typed C interface yet */
static inline u32 call_battleObject_checkSetPriorityByEventFlag_80A82DC_battleObject_setupBeastSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_checkSetPriorityByEventFlag_80A82DC);
    return R0;
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_setupBeastSprite(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_battleObject_setupBeastSprite;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_setupBeastSprite call_sprite_loadAnimationData_battleObject_setupBeastSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_setupBeastSprite){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battleObject_setupBeastSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline void call_sprite_setPriority_8002E14_battleObject_setupBeastSprite(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: generated wrapper: getMapAreaDataTableEntry_80A82A4 has no typed C interface yet */
static inline u32 call_sub_80A82A4(u32 r5) {
    R5 = r5;
    GUEST_CALL(getMapAreaDataTableEntry_80A82A4);
    return R0;
}

typedef struct { u32 r2, r3; } call_battleObject_updateSpriteAndMaybeReloadAnim_80A827C_result;
/* register form: generated wrapper: battleObject_updateSpriteAndMaybeReloadAnim_80A827C has no typed C interface yet */
static inline call_battleObject_updateSpriteAndMaybeReloadAnim_80A827C_result call_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_updateSpriteAndMaybeReloadAnim_80A827C);
    return (call_battleObject_updateSpriteAndMaybeReloadAnim_80A827C_result){ R2, R3 };
}

/*
 * battleObject_setupBeastSprite – initialise a Beast-type battle object's sprite.
 *
 * Sets the sprite type to 3 (Beast), loads its graphics, queries draw
 * priority from the current map-event flags, configures animation, shadow,
 * and priority, then packs three 16-bit attribute words into the object's
 * OAM-like upper halves.  Finally it updates the sprite (and possibly
 * reloads its animation) and returns the r2/r3 the update routine leaves
 * behind.
 */

/* Sprite type constant for Beast Link / Full Metal / Regall. */
#define SPRITE_TYPE_BEAST 3

/* Beast graphics: tileset, palette, base tile. */
#define BEAST_TILESET     0x80
#define BEAST_PALETTE     0x1C
#define BEAST_BASE_TILE   0x56

/* Draw-priority layer (2 = mid-ground in the GBA BG/OAM priority table). */
#define BEAST_DRAW_PRIORITY 2

/* Object field offsets used below. */
#define oBeast_Type        0x0   /* u8  sprite type                          */
#define oBeast_EvtPriority 0x5   /* u8  draw priority from event flags       */
#define oBeast_State       0x8   /* u8  lifecycle state (4 = ready)          */
#define oBeast_Attr0       0xC   /* u32 16-bit attr shifted into upper half  */
#define oBeast_Attr1       0x10  /* u32 ditto                                */
#define oBeast_Attr2       0x14  /* u32 ditto                                */

static call_battleObject_updateSpriteAndMaybeReloadAnim_80A827C_result
setup_beast_sprite(u32 object)
{
    /* Mark the object as a Beast sprite. */
    ST8(object + oBeast_Type, SPRITE_TYPE_BEAST);

    /* Load Beast tileset / palette / base-tile into sprite memory. */
    call_sprite_load_battleObject_setupBeastSprite(BEAST_TILESET, BEAST_PALETTE, BEAST_BASE_TILE, object);

    /* Let the map-event-flag swap helper pick up any pending changes. */
    call_ow_swapMapEventFlags(object);

    /* Query whether an event flag overrides the normal draw priority. */
    u32 priority = call_battleObject_checkSetPriorityByEventFlag_80A82DC_battleObject_setupBeastSprite(object);
    ST8(object + oBeast_EvtPriority, priority);

    /* Start the idle animation (animation id = the queried priority). */
    call_sprite_setAnimation_battleObject_setupBeastSprite(priority, object);

    /* Load animation data, disable shadow, set fixed draw priority. */
    call_sprite_loadAnimationData_battleObject_setupBeastSprite(object);
    call_sprite_noShadow_battleObject_setupBeastSprite(object);
    call_sprite_setPriority_8002E14_battleObject_setupBeastSprite(BEAST_DRAW_PRIORITY, object);

    /* Retrieve the three-attribute data block for this Beast. */
    u32 attr = call_sub_80A82A4(object);

    /* Each 16-bit attribute is placed into the upper half of a u32 field
     * (the game uses the upper half for tile / position packing). */
    ST32(object + oBeast_Attr0, LD16(attr)       << 16);
    ST32(object + oBeast_Attr1, LD16(attr + 2)   << 16);
    ST32(object + oBeast_Attr2, LD16(attr + 4)   << 16);

    /* Transition to "ready" state. */
    ST8(object + oBeast_State, 4);

    /* Final sprite update; r2 and r3 are the outputs our caller expects. */
    return call_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(object);
}

/* battleObject_setupBeastSprite(r5 = object, r10 = game state) -> r2, r3 */
Next battleObject_setupBeastSprite(void)
{
    call_battleObject_updateSpriteAndMaybeReloadAnim_80A827C_result result
        = setup_beast_sprite(R5);

    R2 = result.r2;
    R3 = result.r3;

    RETURN_TO_CALLER();
}

/* ── battleObject_updateSpriteAndMaybeReloadAnim_80A827C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battleObject_checkSetPriorityByEventFlag_80A82DC has no typed C interface yet */
static inline u32 call_battleObject_checkSetPriorityByEventFlag_80A82DC_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_checkSetPriorityByEventFlag_80A82DC);
    return R0;
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_battleObject_updateSpriteAndMaybeReloadAnim_80A827C;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battleObject_updateSpriteAndMaybeReloadAnim_80A827C call_sprite_loadAnimationData_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battleObject_updateSpriteAndMaybeReloadAnim_80A827C){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_sprite_update_result_battleObject_updateSpriteAndMaybeReloadAnim_80A827C;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_battleObject_updateSpriteAndMaybeReloadAnim_80A827C call_sprite_update_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_battleObject_updateSpriteAndMaybeReloadAnim_80A827C){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Update a battle object's sprite, and if the event-flag-driven priority differs
 * from the animation/priority byte stored on the object, reload the animation
 * data and clear the shadow before ticking the sprite.
 *
 * Returns the resulting r2, r3 (from sprite_update) and r4, r5 (from
 * sprite_loadAnimationData on the reload path, or the caller's r4 on the
 * skip path). */
typedef struct { u32 r2, r3, r4, r5; } battleObject_update_result;

static battleObject_update_result battleObject_updateSpriteAndMaybeReloadAnim(u32 r4_in, u32 r5) {
    /* Determine the priority dictated by current event flags. */
    u32 priority = call_battleObject_checkSetPriorityByEventFlag_80A82DC_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(r5);

    /* Compare against the animation ID byte stored at offset 5 of the object. */
    u32 desired = LD8(r5 + 5);

    u32 r4 = r4_in;
    u32 r5_cur = r5;

    if (priority != desired) {
        /* Priority changed: set new animation, reload its data, clear shadow. */
        call_sprite_setAnimation_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(priority, r5_cur);

        call_sprite_loadAnimationData_result_battleObject_updateSpriteAndMaybeReloadAnim_80A827C loaded = call_sprite_loadAnimationData_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(r5_cur);
        r4 = loaded.r4;
        r5_cur = loaded.r5;

        call_sprite_noShadow_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(r5_cur);
    }

    /* Always advance the sprite (position, frame, collision, etc.). */
    call_sprite_update_result_battleObject_updateSpriteAndMaybeReloadAnim_80A827C updated = call_sprite_update_battleObject_updateSpriteAndMaybeReloadAnim_80A827C(r5_cur);

    battleObject_update_result result = { updated.r2, updated.r3, r4, r5_cur };
    return result;
}

/* battleObject_updateSpriteAndMaybeReloadAnim_80A827C(r5 = battle object,
 *   r4 = saved value, r10 = toolkit ptr) -> r2 r3 r4 r5 */
Next battleObject_updateSpriteAndMaybeReloadAnim_80A827C(void) {
    battleObject_update_result res = battleObject_updateSpriteAndMaybeReloadAnim(R4, R5);
    R2 = res.r2;
    R3 = res.r3;
    R4 = res.r4;
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── getMapAreaDataTableEntry_80A82A4 ── */
/* Compute a data address for the current map area.
 * The area index is map group 0x85's special entry 3, or the map number for
 * every other group. It selects one of four base addresses from the table at
 * off_80A82CC; the result is that base plus 8 times the byte at [r5,#4]. */
static u32 sub_80A82A4_typed(u32 obj_ptr, u32 toolkit_ptr) {
    u32 game_state = LD32(toolkit_ptr + oToolkit_GameStatePtr);

    u32 area_index;
    if (LD8(game_state + oGameState_MapGroup) == 0x85)
        area_index = 3;
    else
        area_index = LD8(game_state + oGameState_MapNumber);

    u32 base = LD32(A_off_80A82CC + area_index * 4);
    return base + LD8(obj_ptr + 4) * 8;
}

/* getMapAreaDataTableEntry_80A82A4(r5 = object pointer, r10 = toolkit pointer) -> r0 */
Next getMapAreaDataTableEntry_80A82A4(void) {
    R0 = sub_80A82A4_typed(R5, R10);
    RETURN_TO_CALLER();
}

/* ── ow_swapMapEventFlags ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n; } call_TestEventFlagFromImmediate_result_ow_swapMapEventFlags;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_ow_swapMapEventFlags call_TestEventFlagFromImmediate_ow_swapMapEventFlags(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_ow_swapMapEventFlags){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { bool z, n; } call_TestEventFlag_result;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result call_TestEventFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline void call_SetEventFlag_ow_swapMapEventFlags(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
}

/* register form: generated wrapper: ClearEventFlag has no typed C interface yet */
static inline void call_ClearEventFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(ClearEventFlag);
}

/* Event-flag pair used as the gate for this swap: EVENT_9D6 = flag 0x09D6. */
#define EVENT_9D6 0x09D6

/*
 * Swap one map event flag for its counterpart.
 *
 * r5 points at a work object whose byte at +4 selects a halfword within the
 * flag tables; r10 is the toolkit, through which the current game state is
 * reached. The table is picked by map: group 0x85 (the special map) always
 * uses entry 3, anything else uses the map number. Each entry names a pair of
 * flags: if the "current" one is set the "target" one is cleared, otherwise
 * the target one is set. When the gate flag EVENT_9D6 is clear, nothing is
 * touched.
 *
 * Returns objPtr unchanged (the original leaves r5 alone).
 */
static u32 swap_map_event_flags(u32 objPtr, u32 toolkit) {
    /* Gate: do nothing unless EVENT_9D6 is set (Z clear there means set). */
    if (call_TestEventFlagFromImmediate_ow_swapMapEventFlags(EVENT_9D6 >> 8, EVENT_9D6 & 0xFF).z)
        return objPtr;

    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 tableIndex;
    if (LD8(gameState + oGameState_MapGroup) == 0x85)
        tableIndex = 3;
    else
        tableIndex = LD8(gameState + oGameState_MapNumber);

    /* The object byte selects a halfword slot in both flag tables. */
    u32 slotOffset = (u32)LD8(objPtr + 4) * 2;

    u32 currentFlag = LD16(LD32(A_off_80A8374 + tableIndex * 4) + slotOffset);
    u32 targetFlag  = LD16(LD32(A_off_80A8384 + tableIndex * 4) + slotOffset);

    /* Toggle: current flag set -> clear the target, clear -> set it. */
    if (call_TestEventFlag(currentFlag).z)
        call_SetEventFlag_ow_swapMapEventFlags(targetFlag);
    else
        call_ClearEventFlag(targetFlag);

    return objPtr;
}

/* ow_swapMapEventFlags(r5 = work object, r10 = toolkit) -> r5 (unchanged) */
Next ow_swapMapEventFlags(void) {
    R5 = swap_map_event_flags(R5, R10);
    RETURN_TO_CALLER();
}

/* ── ow_initBeastEventActor ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_ow_initBeastEventActor;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_ow_initBeastEventActor call_sprite_load_ow_initBeastEventActor(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_ow_initBeastEventActor){ R4, R5 };
}

typedef struct { u32 r5, r6; } call_ow_computePointFromPlayerAngle_result;
/* register form: generated wrapper: ow_computePointFromPlayerAngle has no typed C interface yet */
static inline call_ow_computePointFromPlayerAngle_result call_ow_computePointFromPlayerAngle(u32 r5) {
    R5 = r5;
    GUEST_CALL(ow_computePointFromPlayerAngle);
    return (call_ow_computePointFromPlayerAngle_result){ R5, R6 };
}

typedef struct { u32 r4, r5; } call_sprite_setAnimFromArgs_result;
/* register form: generated wrapper: sprite_setAnimFromArgs has no typed C interface yet */
static inline call_sprite_setAnimFromArgs_result call_sprite_setAnimFromArgs(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(sprite_setAnimFromArgs);
    return (call_sprite_setAnimFromArgs_result){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_ow_initBeastEventActor(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_ow_initBeastEventActor;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_ow_initBeastEventActor call_sprite_loadAnimationData_ow_initBeastEventActor(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_ow_initBeastEventActor){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_ow_initBeastEventActor(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result_ow_initBeastEventActor;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result_ow_initBeastEventActor call_applyLayerEffectToOWObject_8035694_ow_initBeastEventActor(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result_ow_initBeastEventActor){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r2; } call_sprite_setPriority_8002E14_result_ow_initBeastEventActor;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_ow_initBeastEventActor call_sprite_setPriority_8002E14_ow_initBeastEventActor(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_ow_initBeastEventActor){ R2 };
}

/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline void call_ClearEventFlagFromImmediate_ow_initBeastEventActor(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

typedef struct { bool z, n; } call_TestEventFlagFromImmediate_result_ow_initBeastEventActor;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_ow_initBeastEventActor call_TestEventFlagFromImmediate_ow_initBeastEventActor(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_ow_initBeastEventActor){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3; } call_npc_updateGuarded_result;
/* register form: generated wrapper: npc_updateGuarded has no typed C interface yet */
static inline call_npc_updateGuarded_result call_npc_updateGuarded(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(npc_updateGuarded);
    return (call_npc_updateGuarded_result){ R2, R3 };
}

/* Actor state: BEAST_EVENT = 3, ACTOR_ACTIVE = 4 */
#define STATE_BEAST_EVENT  3
#define STATE_ACTOR_ACTIVE 4

/* Event flag page/offset for the beast-actor gate and the spawn-once guard */
#define FLAG_BEAST_GATE_PAGE    9
#define FLAG_BEAST_GATE_OFFSET  0xFF
#define FLAG_BEAST_SPAWN_PAGE   9
#define FLAG_BEAST_SPAWN_OFFSET 0xD7

/* Layout offsets within the overworld NPC/actor struct */
#define oActor_State         0
#define oActor_AnimId        4
#define oActor_Active        8
#define oActor_Spawned       9
#define oActor_PositionX    0xc
#define oActor_PositionY    0x10
#define oActor_VelAccum     0x14

/* Field offsets in the tilemap / spawn descriptor pointed to by r6 */
#define oSpawn_TilemapX     0x16
#define oSpawn_TilemapY     0x18

/*
 * Initialize a beast event actor: load its sprite, compute a spawn position
 * relative to the player, configure the layer effect, and mark it active.
 * If the one-time-spawn guard flag is clear, write the computed position into
 * the actor and set the spawned byte; then always transition to active state.
 */
static void ow_initBeastEventActor_impl(u32 actor_ptr, u32 spawn_desc, u32 r10) {
    ST8(actor_ptr + oActor_State, STATE_BEAST_EVENT);

    /* Load sprite graphics for this actor (sheet 0x80, frame 0x1c, anim 0x52) */
    call_sprite_load_ow_initBeastEventActor(0x80, 0x1C, 0x52, actor_ptr);
    /* sprite_load returns a possibly-adjusted actor pointer in r5 */
    actor_ptr = call_sprite_load_ow_initBeastEventActor(0x80, 0x1C, 0x52, actor_ptr).r5;

    /* Compute world position offset from the player's facing angle */
    call_ow_computePointFromPlayerAngle_result pos =
        call_ow_computePointFromPlayerAngle(actor_ptr);
    actor_ptr = pos.r5;
    spawn_desc = pos.r6;

    /* Configure animation from the loaded sprite args */
    actor_ptr = call_sprite_setAnimFromArgs(actor_ptr, spawn_desc).r5;

    /* Apply the default animation ID stored at offset 4 */
    call_sprite_setAnimation_ow_initBeastEventActor(LD8(actor_ptr + oActor_AnimId), actor_ptr);

    /* Load per-animation frame data */
    actor_ptr = call_sprite_loadAnimationData_ow_initBeastEventActor(actor_ptr).r5;

    /* Disable the shadow sprite for this actor */
    call_sprite_noShadow_ow_initBeastEventActor(actor_ptr);

    /* Apply the overworld layer-effect descriptor (pointer to the position
     * sub-struct at offset 0xc) */
    call_applyLayerEffectToOWObject_8035694_result_ow_initBeastEventActor fx =
        call_applyLayerEffectToOWObject_8035694_ow_initBeastEventActor(actor_ptr + oActor_PositionX);
    actor_ptr = fx.r5;
    spawn_desc = fx.r6;

    /* Set the sprite's draw priority from the layer result */
    call_sprite_setPriority_8002E14_ow_initBeastEventActor(fx.r0, actor_ptr);

    /* Clear the beast-gate event flag so this actor type won't re-gate */
    call_ClearEventFlagFromImmediate_ow_initBeastEventActor(FLAG_BEAST_GATE_PAGE, FLAG_BEAST_GATE_OFFSET);

    /*
     * If the one-time spawn guard (EVENT_9D7) is NOT set, write the final
     * tilemap position into the actor and mark it spawned; then clear the
     * guard so this path executes only once per game session.
     */
    call_TestEventFlagFromImmediate_result_ow_initBeastEventActor guard =
        call_TestEventFlagFromImmediate_ow_initBeastEventActor(FLAG_BEAST_SPAWN_PAGE, FLAG_BEAST_SPAWN_OFFSET);

    if (!guard.z) {
        u32 tx = (u32)LD16(spawn_desc + oSpawn_TilemapX) << 16;
        u32 ty = (u32)LD16(spawn_desc + oSpawn_TilemapY) << 16;
        ST32(actor_ptr + oActor_PositionX, tx);
        ST32(actor_ptr + oActor_PositionY, ty);
        ST32(actor_ptr + oActor_VelAccum, 0);
        ST8(actor_ptr + oActor_Spawned, 1);
        call_ClearEventFlagFromImmediate_ow_initBeastEventActor(FLAG_BEAST_SPAWN_PAGE, FLAG_BEAST_SPAWN_OFFSET);
    }

    /* Mark the actor active and run its guarded-update routine */
    ST8(actor_ptr + oActor_Active, STATE_ACTOR_ACTIVE);
    call_npc_updateGuarded(actor_ptr, spawn_desc);
}

/* ow_initBeastEventActor(r5 = actor ptr, r6 = spawn descriptor, r10, lr) -> r2 r3 r4 r5 r8-r12 */
Next ow_initBeastEventActor(void) {
    ow_initBeastEventActor_impl(R5, R6, R10);
    RETURN_TO_CALLER();
}

/* ── ow_cleanupNpcAfterCutscene ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; bool z, n; } call_cutscene_checkOriginalCutsceneScriptPos_8036F70_result;
/* register form: generated wrapper: cutscene_checkOriginalCutsceneScriptPos_8036F70 has no typed C interface yet */
static inline call_cutscene_checkOriginalCutsceneScriptPos_8036F70_result call_cutscene_checkOriginalCutsceneScriptPos_8036F70(u32 r0) {
    R0 = r0;
    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F70);
    return (call_cutscene_checkOriginalCutsceneScriptPos_8036F70_result){ R2, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: gameState_setUnk09_8001172 has no typed C interface yet */
static inline void call_gameState_setUnk09_8001172(u32 r0) {
    R0 = r0;
    GUEST_CALL(gameState_setUnk09_8001172);
}

/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline void call_ClearEventFlagFromImmediate_ow_cleanupNpcAfterCutscene(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

typedef struct { u32 r4, r5; } call_reqBBS_clearFlag_8140A0C_result;
/* register form: generated wrapper: reqBBS_clearFlag_8140A0C has no typed C interface yet */
static inline call_reqBBS_clearFlag_8140A0C_result call_reqBBS_clearFlag_8140A0C(void) {
    GUEST_CALL(reqBBS_clearFlag_8140A0C);
    return (call_reqBBS_clearFlag_8140A0C_result){ R4, R5 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_FreeOverworldMapObject_result_ow_cleanupNpcAfterCutscene;
/* register form: generated wrapper: FreeOverworldMapObject has no typed C interface yet */
static inline call_FreeOverworldMapObject_result_ow_cleanupNpcAfterCutscene call_FreeOverworldMapObject_ow_cleanupNpcAfterCutscene(u32 r5) {
    R5 = r5;
    GUEST_CALL(FreeOverworldMapObject);
    return (call_FreeOverworldMapObject_result_ow_cleanupNpcAfterCutscene){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Clean up the overworld NPC once its cutscene has run to the end: count down
 * a short delay, then clear the event flags the scene set and free the map
 * object. Every register output (r2-r5) is whatever the callees leave behind,
 * so the adapter itself writes nothing. */

/* npc: the overworld map object being cleaned up (r5 on entry) */
static void cleanup_npc_after_cutscene(u32 npc) {
    /* Only proceed when the cutscene script is back at its original position.
     * checkOriginalCutsceneScriptPos sets Z when it is. */
    if (!call_cutscene_checkOriginalCutsceneScriptPos_8036F70(A_byte_808C74C).z)
        return;

    /* The object sticks around a few more frames before the cleanup runs. */
    u32 framesLeft = LD8(npc + 7);
    ST8(npc + 7, framesLeft - 1);
    if (framesLeft - 1 != 0)
        return;

    call_gameState_setUnk09_8001172(0);

    /* Clear the flags the cutscene raised. */
    call_ClearEventFlagFromImmediate_ow_cleanupNpcAfterCutscene(9, 0xD8);  /* EVENT_9D8 */
    call_ClearEventFlagFromImmediate_ow_cleanupNpcAfterCutscene(9, 0xFF);  /* EVENT_9FF */
    call_ClearEventFlagFromImmediate_ow_cleanupNpcAfterCutscene(9, 0xD9);  /* EVENT_9D9 */

    call_reqBBS_clearFlag_8140A0C();
    call_FreeOverworldMapObject_ow_cleanupNpcAfterCutscene(npc);
}

/* ow_cleanupNpcAfterCutscene(r5 = map object, r10 = context) -> r2 r3 r4 r5
 * The typed function leaves r2/r3/r4/r5 exactly as the callees (or the entry
 * state, on the early-return paths) leave them, so no output writes here. */
Next ow_cleanupNpcAfterCutscene(void) {
    cleanup_npc_after_cutscene(R5);
    RETURN_TO_CALLER();
}

/* ── getRandomFadeLevel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_reqBBS_81409D0_result;
/* register form: generated wrapper: reqBBS_81409D0 has no typed C interface yet */
static inline call_reqBBS_81409D0_result call_reqBBS_81409D0_getRandomFadeLevel(u32 r0) {
    R0 = r0;
    GUEST_CALL(reqBBS_81409D0);
    return (call_reqBBS_81409D0_result){ R0 };
}

/* Fade levels indexed by a random value: reqBBS_81409D0 called with 8 yields
 * 0..7, which selects one of the eight fade step sizes (0x00, 0x1F, ..., 0xFF). */
static u32 get_random_fade_level(void) {
    u32 index = call_reqBBS_81409D0_getRandomFadeLevel(8).r0;
    return LD8(A_byte_80A8510 + index);
}

/* getRandomFadeLevel() -> r0 = fade level from byte_80A8510[index] */
Next getRandomFadeLevel(void) {
    R0 = get_random_fade_level();
    RETURN_TO_CALLER();
}

/* ── obj_decrementFixedCounterBy4_80A856C ── */
/* Decrement the 32-bit counter at [r5+0x14] by 4<<16, store the raw result back,
 * and return the value clamped at 0 so N/Z reflect the clamped counter
 * (r0 itself is a clobber; only the flags are observed by callers). */
static u32 sub_80A856C_decrement(u32 obj) {
    u32 counter_addr = obj + 0x14;
    s32 counter = (s32)LD32(counter_addr);

    counter -= 4 << 16;
    ST32(counter_addr, (u32)counter);

    if (counter > 0)
        return (u32)counter;
    return 0;
}

/* obj_decrementFixedCounterBy4_80A856C(r5 = object, lr = return address) -> flags nz */
Next obj_decrementFixedCounterBy4_80A856C(void) {
    R0 = sub_80A856C_decrement(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── npc_playerProximityEventTrigger ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n, c, v; } call_TestEventFlagFromImmediate_result_npc_playerProximityEventTrigger;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_npc_playerProximityEventTrigger call_TestEventFlagFromImmediate_npc_playerProximityEventTrigger(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_npc_playerProximityEventTrigger){ ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1; } call_ReadOWPlayerObjectCoords_result_npc_playerProximityEventTrigger;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result_npc_playerProximityEventTrigger call_ReadOWPlayerObjectCoords_npc_playerProximityEventTrigger(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result_npc_playerProximityEventTrigger){ R0, R1 };
}

typedef struct { u32 r0, r2; } call_overworldObject_withinBoxCheck_signed_80011D6_result_npc_playerProximityEventTrigger;
/* register form: generated wrapper: overworldObject_withinBoxCheck_signed_80011D6 has no typed C interface yet */
static inline call_overworldObject_withinBoxCheck_signed_80011D6_result_npc_playerProximityEventTrigger call_overworldObject_withinBoxCheck_signed_80011D6_npc_playerProximityEventTrigger(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(overworldObject_withinBoxCheck_signed_80011D6);
    return (call_overworldObject_withinBoxCheck_signed_80011D6_result_npc_playerProximityEventTrigger){ R0, R2 };
}

/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline void call_SetEventFlagFromImmediate_npc_playerProximityEventTrigger(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* Proximity event trigger for an overworld NPC (r5 = NPC object pointer).
 *
 * While the "player advanced forward" story flag is still clear, check whether
 * the player stands inside the NPC's event box (its x/y fields at +0xC/+0x10,
 * half-extent 8 << 16 subpixels). On a hit, raise the NPC's talk event flag
 * (bank 9, index 0xD9) and arm the object: state byte +8 = 8, script byte
 * +7 = 0x50. r10 (flag bank base) is only used by the flag wrappers. */

#define EVENT_BANK_PLAYER_ADVANCE_FORWARD 0x17
#define EVENT_IDX_PLAYER_ADVANCE_FORWARD  0x17

#define EVENT_BANK_NPC_TALK 9
#define EVENT_IDX_NPC_TALK  0xD9

#define PROXIMITY_HALF_SIZE   (8u << 16)  /* box half-extent passed in r4 */
#define NPC_STATE_TALK        8           /* byte +8: hand the NPC to the talk handler */
#define NPC_SCRIPT_TALK       0x50        /* byte +7: talk script id */
#define WITHIN_BOX_RESULT_HIT 1

static void run_player_proximity_trigger(u32 npc)
{
    /* register form: the box-check wrapper passes its half-extent in guest r4
     * and the callee clobbers r4, but this routine preserves r4 (push/pop in
     * the original), so back it up and restore it around that call. */
    u32 saved_r4 = R4;

    /* TestEventFlagFromImmediate leaves Z clear when the flag is set. */
    if (!call_TestEventFlagFromImmediate_npc_playerProximityEventTrigger(EVENT_BANK_PLAYER_ADVANCE_FORWARD,
                                         EVENT_IDX_PLAYER_ADVANCE_FORWARD).z)
        return;

    /* Player position vs. the NPC's event box. */
    call_ReadOWPlayerObjectCoords_result_npc_playerProximityEventTrigger player = call_ReadOWPlayerObjectCoords_npc_playerProximityEventTrigger();

    call_overworldObject_withinBoxCheck_signed_80011D6_result_npc_playerProximityEventTrigger inBox =
        call_overworldObject_withinBoxCheck_signed_80011D6_npc_playerProximityEventTrigger(
            LD32(npc + 0xC), LD32(npc + 0x10),
            player.r0, player.r1, PROXIMITY_HALF_SIZE);
    R4 = saved_r4;  /* restore the register the original push/pop keeps */

    if (inBox.r0 != WITHIN_BOX_RESULT_HIT)
        return;

    /* Player is in range: mark the event as seen and arm the NPC. */
    call_SetEventFlagFromImmediate_npc_playerProximityEventTrigger(EVENT_BANK_NPC_TALK, EVENT_IDX_NPC_TALK);
    ST8(npc + 8, NPC_STATE_TALK);
    ST8(npc + 7, NPC_SCRIPT_TALK);
}

/* npc_playerProximityEventTrigger(r5 = NPC object, r10 = flag base) -> nothing */
Next npc_playerProximityEventTrigger(void) {
    run_player_proximity_trigger(R5);
    RETURN_TO_CALLER();
}

/* ── npc_syncScreenPosToCamera ── */
/* Copy an NPC's fixed-point screen position into its on-screen coordinates:
 * take the two 32-bit values at [src+0xC] and [src+0x10], drop the low
 * 16 fractional bits, and store the integer parts as halfwords at
 * [dst+0x16] (x) and [dst+0x18] (y). */
static void sync_screen_pos_to_camera(u32 src, u32 dst) {
    s32 x = (s32)LD32(src + 0xC) >> 16;
    s32 y = (s32)LD32(src + 0x10) >> 16;
    ST16(dst + 0x16, (u16)x);
    ST16(dst + 0x18, (u16)y);
}

/* npc_syncScreenPosToCamera(r5 = source object, r6 = destination) -> nothing */
Next npc_syncScreenPosToCamera(void) {
    sync_screen_pos_to_camera(R5, R6);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchObjectByType_80A8728 ── */
/* battle_dispatchObjectByType_80A8728 -- per-type handler dispatch for a battle object.
 *
 * r5 points to an object whose byte at +8 is a type index.  The handler for
 * that type is read from the jump table at off_80A873C and called as an
 * ordinary subroutine ("mov lr, pc; bx r0"): the handler gets the table
 * entry in r0, the type byte in r1, the object still in r5, and r10
 * untouched.  Whatever the handler leaves in r2, r3 and r8-r12 flows
 * straight through to this function's caller, exactly as in the original;
 * r4-r7 are saved and restored around the call by the push/pop pair.
 */

/* Call the selected handler through its table address.
 * register form: the handler's live register state exists only in the guest
 * register file, so the original's push/pop {r4-r7} around the call has to
 * be reproduced by writing those guest registers directly. */
static void call_type_handler(u32 handler, u32 type, u32 r10)
{
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;   /* push {r4-r7} */

    R0 = handler;                  /* the table entry stays in r0 for the callee */
    R1 = type;                     /* ... and the type byte in r1 */
    R10 = r10;
    GUEST_CALL_ADDR(handler);      /* mov lr, pc; bx r0 */

    R4 = r4;                       /* pop {r4-r7}: whatever the handler did */
    R5 = r5;                       /* to them is not visible to the caller */
    R6 = r6;
    R7 = r7;
}

/* Look up the handler for obj's type and run it. */
static void dispatch_object_type(u32 obj, u32 r10)
{
    u32 type = LD8(obj + 8);
    u32 handler = LD32(A_off_80A873C + type);
    call_type_handler(handler, type, r10);
}

/* battle_dispatchObjectByType_80A8728() */
Next battle_dispatchObjectByType_80A8728(void) {
    dispatch_object_type(R5, R10);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_80A8E74 ── */
/* owMapObject_weatherTornado_80A8E74 — Mr. Weather comp tornado (map object type 0x1A).
 *
 * Pure state dispatcher: the object's state byte at +8 (0, 4, 8, 0xC) indexes a table
 * of Thumb handlers (init / active / caught / free-when-animation-ends) and the call
 * is a tail call — the push {lr} / pop {pc} pair makes the chosen handler return
 * straight to this map object's caller. The handler runs with:
 *   r5  = the map object,
 *   r6  = address of eWeatherCompState (its shared state variable),
 *   r10 = the caller's context value, passed through untouched.
 * Everything the handler leaves in r2-r5 and r8-r12 (plus r0/r1 and the flags) is
 * what the callers of this function observe, so those registers must flow through. */

/* register form: dispatch goes through a computed handler address, and it is a tail
 * call — the handler's register side effects (r2-r5, r8-r12) are this function's
 * outputs and cannot be expressed as a C return value. */
static void weather_tornado_dispatch(u32 object, u32 context) {
    R5 = object;
    R10 = context;
    R6 = A_eWeatherCompState;
    /* Handler = table entry selected by the object's state byte. */
    GUEST_CALL_ADDR(LD32(A_off_80A8E88 + LD8(object + 8)));
}

Next owMapObject_weatherTornado_80A8E74(void) {
    weather_tornado_dispatch(R5, R10);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_init_80A8E9C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_owMapObject_weatherTornado_init_80A8E9C;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_owMapObject_weatherTornado_init_80A8E9C call_sprite_load_owMapObject_weatherTornado_init_80A8E9C(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_owMapObject_weatherTornado_init_80A8E9C){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_owMapObject_weatherTornado_init_80A8E9C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_owMapObject_weatherTornado_init_80A8E9C;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_owMapObject_weatherTornado_init_80A8E9C call_sprite_loadAnimationData_owMapObject_weatherTornado_init_80A8E9C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_owMapObject_weatherTornado_init_80A8E9C){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_owMapObject_weatherTornado_init_80A8E9C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result_owMapObject_weatherTornado_init_80A8E9C;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result_owMapObject_weatherTornado_init_80A8E9C call_applyLayerEffectToOWObject_8035694_owMapObject_weatherTornado_init_80A8E9C(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result_owMapObject_weatherTornado_init_80A8E9C){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline void call_sprite_setPriority_8002E14_owMapObject_weatherTornado_init_80A8E9C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: generated wrapper: owMapObject_weatherTornado_loadHome_80A90B0 has no typed C interface yet */
static inline void call_owMapObject_weatherTornado_loadHome_80A90B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_loadHome_80A90B0);
}

typedef struct { bool z, n; } call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_init_80A8E9C;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_init_80A8E9C call_TestEventFlagFromImmediate_owMapObject_weatherTornado_init_80A8E9C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_init_80A8E9C){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { bool z, n; } call_owMapObject_weatherTornado_isCaught_80A91D4_result;
/* register form: generated wrapper: owMapObject_weatherTornado_isCaught_80A91D4 has no typed C interface yet */
static inline call_owMapObject_weatherTornado_isCaught_80A91D4_result call_owMapObject_weatherTornado_isCaught_80A91D4(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_isCaught_80A91D4);
    return (call_owMapObject_weatherTornado_isCaught_80A91D4_result){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3; } call_sub_80A8FFC_result;
/* register form: generated wrapper: sub_80A8FFC has no typed C interface yet */
static inline call_sub_80A8FFC_result call_sub_80A8FFC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80A8FFC);
    return (call_sub_80A8FFC_result){ R2, R3 };
}

/* register form: generated wrapper: owMapObject_weatherTornado_restoreState_80A9268 has no typed C interface yet */
static inline void call_owMapObject_weatherTornado_restoreState_80A9268(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(owMapObject_weatherTornado_restoreState_80A9268);
}

/* register form: generated wrapper: owMapObject_weatherTornado_loadParams_80A908C has no typed C interface yet */
static inline void call_owMapObject_weatherTornado_loadParams_80A908C(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_loadParams_80A908C);
}

typedef struct { u32 r2, r3, r8, r9, r10, r11, r12; } call_owMapObject_weatherTornado_active_80A8F08_result;
/* register form: generated wrapper: owMapObject_weatherTornado_active_80A8F08 has no typed C interface yet */
static inline call_owMapObject_weatherTornado_active_80A8F08_result call_owMapObject_weatherTornado_active_80A8F08(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(owMapObject_weatherTornado_active_80A8F08);
    return (call_owMapObject_weatherTornado_active_80A8F08_result){ R2, R3, R8, R9, R10, R11, R12 };
}

/* Initialize the weather-tornado overworld map object: load its sprite and
 * animation data, apply the map-layer palette tint, set its home position,
 * then configure its wandering state based on event-flag BFB and whether
 * the player already caught it. */

#define EVENT_BFB_PAGE   0xB
#define EVENT_BFB_OFFSET 0xFB

static void owMapObject_weatherTornado_init_impl(u32 obj, u32 sibling) {
    /* State 3 = initializing */
    ST8(obj, 3);

    /* Load tornado sprite: sheet 0x80, frame 0x1C, cell 0x6C */
    call_sprite_load_owMapObject_weatherTornado_init_80A8E9C(0x80, 0x1C, 0x6C, obj);

    /* Set animation index 0x18 (r0 = animation id, r5 = sprite) */
    call_sprite_setAnimation_owMapObject_weatherTornado_init_80A8E9C(0x18, obj);

    /* Load the animation sequence data and clear the shadow flag */
    call_sprite_loadAnimationData_owMapObject_weatherTornado_init_80A8E9C(obj);
    call_sprite_noShadow_owMapObject_weatherTornado_init_80A8E9C(obj);

    /* Tint the sprite palette for the current overworld map layer */
    call_applyLayerEffectToOWObject_8035694_result_owMapObject_weatherTornado_init_80A8E9C eff =
        call_applyLayerEffectToOWObject_8035694_owMapObject_weatherTornado_init_80A8E9C(obj + 0xC);
    /* Apply the priority the layer-effect routine selected */
    call_sprite_setPriority_8002E14_owMapObject_weatherTornado_init_80A8E9C(eff.r0, obj);

    /* Place the tornado at its saved home map coordinates */
    call_owMapObject_weatherTornado_loadHome_80A90B0(obj);

    /* Test event flag BFB – Z set means the flag has NOT been set yet */
    call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_init_80A8E9C flagRes =
        call_TestEventFlagFromImmediate_owMapObject_weatherTornado_init_80A8E9C(EVENT_BFB_PAGE, EVENT_BFB_OFFSET);

    if (flagRes.z) {
        /* Flag not set: tornado starts dormant */
        ST8(obj + 0x28, 0);   /* timer reset */
        ST8(obj + 9, 0);      /* sub-state = none */
        ST8(obj + 0x24, 1);   /* dormant flag */
    } else {
        /* Flag was set: has the player already captured this tornado? */
        call_owMapObject_weatherTornado_isCaught_80A91D4_result caughtRes =
            call_owMapObject_weatherTornado_isCaught_80A91D4(obj);
        if (caughtRes.z) {
            /* Not caught: restore its previously saved wandering state */
            call_owMapObject_weatherTornado_restoreState_80A9268(obj, sibling);
        } else {
            /* Already caught: enter the caught sub-state (0xC) and finish */
            ST8(obj + 8, 0xC);
            call_sub_80A8FFC(obj);
            return;
        }
    }

    /* Common tail for dormant / not-caught paths:
     * load wandering parameters, mark active (state 4), begin movement */
    call_owMapObject_weatherTornado_loadParams_80A908C(obj);
    ST8(obj + 8, 4);
    call_owMapObject_weatherTornado_active_80A8F08(obj, sibling);
}

/* owMapObject_weatherTornado_init_80A8E9C(r5 = obj, r6 = sibling) */
Next owMapObject_weatherTornado_init_80A8E9C(void) {
    owMapObject_weatherTornado_init_impl(R5, R6);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_active_80A8F08 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_active_80A8F08;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_active_80A8F08 call_TestEventFlagFromImmediate_owMapObject_weatherTornado_active_80A8F08(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_active_80A8F08){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_owMapObject_weatherTornado_caught_80A8FA0_result;
/* register form: generated wrapper: owMapObject_weatherTornado_caught_80A8FA0 has no typed C interface yet */
static inline call_owMapObject_weatherTornado_caught_80A8FA0_result call_owMapObject_weatherTornado_caught_80A8FA0(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(owMapObject_weatherTornado_caught_80A8FA0);
    return (call_owMapObject_weatherTornado_caught_80A8FA0_result){ R2, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r2; bool z, n; } call_cutscene_checkOriginalCutsceneScriptPos_8036F40_result;
/* register form: generated wrapper: cutscene_checkOriginalCutsceneScriptPos_8036F40 has no typed C interface yet */
static inline call_cutscene_checkOriginalCutsceneScriptPos_8036F40_result call_cutscene_checkOriginalCutsceneScriptPos_8036F40(void) {
    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F40);
    return (call_cutscene_checkOriginalCutsceneScriptPos_8036F40_result){ R2, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2; bool z, n; } call_cutscene_checkOriginalCutsceneScriptPos_8036F58_result;
/* register form: generated wrapper: cutscene_checkOriginalCutsceneScriptPos_8036F58 has no typed C interface yet */
static inline call_cutscene_checkOriginalCutsceneScriptPos_8036F58_result call_cutscene_checkOriginalCutsceneScriptPos_8036F58(void) {
    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F58);
    return (call_cutscene_checkOriginalCutsceneScriptPos_8036F58_result){ R2, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { bool z, n; } call_chatbox_mask_eFlags2009F38_result_owMapObject_weatherTornado_active_80A8F08;
/* register form: generated wrapper: chatbox_mask_eFlags2009F38 has no typed C interface yet */
static inline call_chatbox_mask_eFlags2009F38_result_owMapObject_weatherTornado_active_80A8F08 call_chatbox_mask_eFlags2009F38_owMapObject_weatherTornado_active_80A8F08(u32 r0) {
    R0 = r0;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return (call_chatbox_mask_eFlags2009F38_result_owMapObject_weatherTornado_active_80A8F08){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r5, r6, r8, r9, r10, r11, r12; } call_owMapObject_weatherTornado_moveOrPause_80A8F48_result;
/* register form: generated wrapper: owMapObject_weatherTornado_moveOrPause_80A8F48 has no typed C interface yet */
static inline call_owMapObject_weatherTornado_moveOrPause_80A8F48_result call_owMapObject_weatherTornado_moveOrPause_80A8F48(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(owMapObject_weatherTornado_moveOrPause_80A8F48);
    return (call_owMapObject_weatherTornado_moveOrPause_80A8F48_result){ R5, R6, R8, R9, R10, R11, R12 };
}

/* register form: generated wrapper: owMapObject_weatherTornado_checkHitPlayer_80A91A4 has no typed C interface yet */
static inline void call_owMapObject_weatherTornado_checkHitPlayer_80A91A4(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_checkHitPlayer_80A91A4);
}

typedef struct { u32 r5, r6, r8, r9, r10, r11, r12; } call_owMapObject_weatherTornado_setPriority_80A91F4_result;
/* register form: generated wrapper: owMapObject_weatherTornado_setPriority_80A91F4 has no typed C interface yet */
static inline call_owMapObject_weatherTornado_setPriority_80A91F4_result call_owMapObject_weatherTornado_setPriority_80A91F4(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_setPriority_80A91F4);
    return (call_owMapObject_weatherTornado_setPriority_80A91F4_result){ R5, R6, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r3, r4, r5; } call_sprite_update_result_owMapObject_weatherTornado_active_80A8F08;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_owMapObject_weatherTornado_active_80A8F08 call_sprite_update_owMapObject_weatherTornado_active_80A8F08(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_owMapObject_weatherTornado_active_80A8F08){ R3, R4, R5 };
}

typedef struct { u32 r2; } call_owMapObject_weatherTornado_saveState_80A9218_result;
/* register form: generated wrapper: owMapObject_weatherTornado_saveState_80A9218 has no typed C interface yet */
static inline call_owMapObject_weatherTornado_saveState_80A9218_result call_owMapObject_weatherTornado_saveState_80A9218(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(owMapObject_weatherTornado_saveState_80A9218);
    return (call_owMapObject_weatherTornado_saveState_80A9218_result){ R2 };
}

/* Event flag constants for TestEventFlagFromImmediate (group 0xB, index 0xEF). */
#define EVENT_BEF  0xBu
#define EVENT_BEF_INDEX 0xEFu

/* Chatbox/dialog mask flag that suppresses tornado movement while talking. */
#define CHATBOX_FLAG 0x80u

/* Tornado state byte written when the "tornado caught" event has fired. */
#define TORNADO_CAUGHT_STATE 8u

/* Advance the weather-tornado overworld object by one frame.
 * Dispatches between "caught" animation, cutscene/chatbox suppression,
 * and normal move-or-pause behavior, then refreshes sprite and state.
 *
 * obj  = pointer to the tornado's overworld map object (r5)
 * data = companion object / data pointer (r6)
 *
 * All guest output registers (r2, r3, r11, …) are left in place by the
 * last wrapper call in the sequence; this helper computes the values needed
 * for internal routing only. */
static void tornado_active_logic(u32 obj, u32 data) {
    u32 cur_r5 = obj;
    u32 cur_r6 = data;

    /* If the "tornado caught before" event has fired, switch to caught state. */
    call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_active_80A8F08 flag = call_TestEventFlagFromImmediate_owMapObject_weatherTornado_active_80A8F08(EVENT_BEF, EVENT_BEF_INDEX);
    if (!flag.z) {
        ST8(obj + 8, TORNADO_CAUGHT_STATE);
        call_owMapObject_weatherTornado_caught_80A8FA0_result caught =
            call_owMapObject_weatherTornado_caught_80A8FA0(cur_r5, cur_r6);
        cur_r5 = caught.r5;
        cur_r6 = caught.r6;
    } else {
        /* Not yet caught: suppress movement during a cutscene or chatbox. */
        call_cutscene_checkOriginalCutsceneScriptPos_8036F40_result c1 =
            call_cutscene_checkOriginalCutsceneScriptPos_8036F40();
        if (c1.z) {
            call_cutscene_checkOriginalCutsceneScriptPos_8036F58_result c2 =
                call_cutscene_checkOriginalCutsceneScriptPos_8036F58();
            if (c2.z) {
                call_chatbox_mask_eFlags2009F38_result_owMapObject_weatherTornado_active_80A8F08 chat =
                    call_chatbox_mask_eFlags2009F38_owMapObject_weatherTornado_active_80A8F08(CHATBOX_FLAG);
                if (chat.z) {
                    /* Free to animate: move or pause the tornado normally. */
                    call_owMapObject_weatherTornado_moveOrPause_80A8F48_result mp =
                        call_owMapObject_weatherTornado_moveOrPause_80A8F48(cur_r5, cur_r6);
                    cur_r5 = mp.r5;
                    cur_r6 = mp.r6;
                    call_owMapObject_weatherTornado_checkHitPlayer_80A91A4(cur_r5);
                }
            }
        }
    }

    /* Common frame-end: set draw priority, refresh sprite, persist state. */
    call_owMapObject_weatherTornado_setPriority_80A91F4_result sp =
        call_owMapObject_weatherTornado_setPriority_80A91F4(cur_r5);
    call_sprite_update_result_owMapObject_weatherTornado_active_80A8F08 su = call_sprite_update_owMapObject_weatherTornado_active_80A8F08(sp.r5);
    call_owMapObject_weatherTornado_saveState_80A9218(su.r5, sp.r6);
}

/* owMapObject_weatherTornado_active_80A8F08(r5 = obj, r6 = data, r10 = ambient) -> r2 r3 r11 */
Next owMapObject_weatherTornado_active_80A8F08(void) {
    tornado_active_logic(R5, R6);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_moveOrPause_80A8F48 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: owMapObject_weatherTornado_step_80A9038 has no typed C interface yet */
static inline void call_owMapObject_weatherTornado_step_80A9038_owMapObject_weatherTornado_moveOrPause_80A8F48(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_step_80A9038);
}

typedef struct { u32 r5, r6, r8, r9, r10, r11, r12; bool z, n; } call_owMapObject_weatherTornado_isBlocked_80A90CC_result_owMapObject_weatherTornado_moveOrPause_80A8F48;
/* register form: generated wrapper: owMapObject_weatherTornado_isBlocked_80A90CC has no typed C interface yet */
static inline call_owMapObject_weatherTornado_isBlocked_80A90CC_result_owMapObject_weatherTornado_moveOrPause_80A8F48 call_owMapObject_weatherTornado_isBlocked_80A90CC_owMapObject_weatherTornado_moveOrPause_80A8F48(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_isBlocked_80A90CC);
    return (call_owMapObject_weatherTornado_isBlocked_80A90CC_result_owMapObject_weatherTornado_moveOrPause_80A8F48){ R5, R6, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r8, r9, r10, r11, r12; } call_owMapObject_weatherTornado_repickDirection_80A90E4_result;
/* register form: generated wrapper: owMapObject_weatherTornado_repickDirection_80A90E4 has no typed C interface yet */
static inline call_owMapObject_weatherTornado_repickDirection_80A90E4_result call_owMapObject_weatherTornado_repickDirection_80A90E4(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(owMapObject_weatherTornado_repickDirection_80A90E4);
    return (call_owMapObject_weatherTornado_repickDirection_80A90E4_result){ R8, R9, R10, R11, R12 };
}

/* register form: generated wrapper: owMapObject_weatherTornado_pickDirection_80A9004 has no typed C interface yet */
static inline void call_owMapObject_weatherTornado_pickDirection_80A9004(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(owMapObject_weatherTornado_pickDirection_80A9004);
}

typedef struct { bool z, n; } call_owMapObject_weatherTornado_isPauseDue_80A9150_result;
/* register form: generated wrapper: owMapObject_weatherTornado_isPauseDue_80A9150 has no typed C interface yet */
static inline call_owMapObject_weatherTornado_isPauseDue_80A9150_result call_owMapObject_weatherTornado_isPauseDue_80A9150(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_isPauseDue_80A9150);
    return (call_owMapObject_weatherTornado_isPauseDue_80A9150_result){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Field offsets within the weather-tornado map object */
#define TORNADO_PHASE        9
#define TORNADO_MOVE_TIMER   0x24
#define TORNADO_PAUSE_TIMER  0x25
#define TORNADO_MOVE_INIT    0x26
#define TORNADO_PAUSE_INIT   0x27

/* Decrement the pause countdown; reset the phase when it reaches zero. */
static void tornado_pause_countdown(u32 obj) {
    u32 remaining = LD8(obj + TORNADO_PAUSE_TIMER) - 1;
    ST8(obj + TORNADO_PAUSE_TIMER, remaining);
    if (remaining == 0)
        ST8(obj + TORNADO_PHASE, 0);
}

/* Advance one frame of the weather-tornado: step along, check for obstacles,
 * re-pick a direction when blocked, and manage the short pause between moves.
 *
 * r10 is received but consumed implicitly by the isBlocked / repickDirection
 * wrappers (they read R10 from the register file). */
static void tornado_move_or_pause(u32 r5, u32 r6, u32 r10) {
    (void)r10;
    u32 phase = LD8(r5 + TORNADO_PHASE);

    /* Phase 1+: we are in the pause-between-moves countdown. */
    if (phase != 0) {
        tornado_pause_countdown(r5);
        return;
    }

    /* Phase 0: decrement the move-along timer. */
    u32 timer = LD8(r5 + TORNADO_MOVE_TIMER) - 1;
    ST8(r5 + TORNADO_MOVE_TIMER, timer);

    if (timer != 0) {
        /* Still walking – take a step, then test for collision. */
        call_owMapObject_weatherTornado_step_80A9038_owMapObject_weatherTornado_moveOrPause_80A8F48(r5);
        call_owMapObject_weatherTornado_isBlocked_80A90CC_result_owMapObject_weatherTornado_moveOrPause_80A8F48 blocked =
            call_owMapObject_weatherTornado_isBlocked_80A90CC_owMapObject_weatherTornado_moveOrPause_80A8F48(r5);
        if (blocked.z)
            return;                       /* path clear – continue next frame */

        /* Blocked: restore timer, choose a new direction. */
        r5 = blocked.r5;
        ST8(r5 + TORNADO_MOVE_TIMER, LD8(r5 + TORNADO_MOVE_INIT));
        call_owMapObject_weatherTornado_repickDirection_80A90E4(r5, blocked.r6);
        return;
    }

    /* Timer expired – pick a fresh direction and decide whether to pause. */
    ST8(r5 + TORNADO_MOVE_TIMER, LD8(r5 + TORNADO_MOVE_INIT));
    call_owMapObject_weatherTornado_pickDirection_80A9004(r5, r6);
    call_owMapObject_weatherTornado_isPauseDue_80A9150_result pause =
        call_owMapObject_weatherTornado_isPauseDue_80A9150(r5);
    if (pause.z)
        return;                           /* keep moving immediately */

    /* Start the inter-move pause: enter phase 1, load countdown, tick once. */
    phase = phase + 1;
    ST8(r5 + TORNADO_PHASE, phase);
    ST8(r5 + TORNADO_PAUSE_TIMER, LD8(r5 + TORNADO_PAUSE_INIT));
    tornado_pause_countdown(r5);
}

/* owMapObject_weatherTornado_moveOrPause_80A8F48(r5, r6, r10)
 * Outputs r5 r6 r8 r9 r10 r11 r12 are left in place by the callees. */
Next owMapObject_weatherTornado_moveOrPause_80A8F48(void) {
    tornado_move_or_pause(R5, R6, R10);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_pickDirection_80A9004 ── */
/* Tornado weather map object: pick a new blowing direction.
 * r5 = the tornado map object, r6 = eWeatherCompState.
 * Draws a random direction (0..7); if it matches the previous direction it is
 * stepped one on (7 wraps to 0). The result is stored as obj+5 and recorded in
 * the weather state's "last direction" slot; obj+0x28 counts the re-picks. */

/* reqBBS_81409D0(a1) -> random value: no generated wrapper covers this call,
 * so use a register-form helper that sets R0, calls, and returns R0. */
static u32 call_reqBBS_81409D0_owMapObject_weatherTornado_pickDirection_80A9004(u32 a1) { /* register form: calls reqBBS_81409D0, which has no typed C interface yet (noted by the tool) */
    R0 = a1;
    GUEST_CALL(reqBBS_81409D0);
    return R0;
}

static void weather_tornado_pick_direction(u32 obj, u32 weather) {
    u32 random = call_reqBBS_81409D0_owMapObject_weatherTornado_pickDirection_80A9004(8);
    u32 slot = LD8(obj + 4);

    /* previous direction, kept in the weather state at 0x31 + slot */
    u32 lastIndex = 0x31 + slot;
    u32 prevDir = LD8(weather + lastIndex);

    /* mirror entry at 0x19 + slot is written back with its own index */
    u32 mirrorIndex = 0x19 + slot;
    ST8(weather + mirrorIndex, mirrorIndex);

    u32 dir = random;
    if (dir == prevDir) {
        /* same direction as last time: move one step on, 7 -> 0 */
        if ((s32)dir < 7)
            dir = dir + 1;
        else
            dir = 0;
    }

    ST8(weather + lastIndex, dir);
    ST8(obj + 5, dir);

    ST8(obj + 0x28, LD8(obj + 0x28) + 1);
}

Next owMapObject_weatherTornado_pickDirection_80A9004(void) {
    weather_tornado_pick_direction(R5, R6);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_step_80A9038 ── */
/* Weather tornado map object offsets */
#define oTornado_Mode        0x05
#define oTornado_PosX        0x0C
#define oTornado_PosY        0x10
#define oTornado_Step        0x2C
#define oTornado_AltStep     0x30

/* Advance the tornado's position by one step.
 * The mode byte at +5 selects a movement direction:
 *   0=down-left  1=left  2=down-right(diag)  3=up
 *   4=up-left(diag)  5=right  6=up-right(diag,else)  7=down
 * Modes 0–1,3–5,7 use the primary step size; 2 and default
 * use the alternate diagonal step size at +0x30. */
static void step_tornado_position(u32 obj) {
    u32 step     = LD32(obj + oTornado_Step);
    u8  mode     = (u8)LD8(obj + oTornado_Mode);
    u32 x        = LD32(obj + oTornado_PosX);
    u32 y        = LD32(obj + oTornado_PosY);

    switch (mode) {
    case 0: x += step; y -= step; break;
    case 1: x += step;             break;
    case 3: y += step;             break;
    case 4: x -= step; y += step;  break;
    case 5: x -= step;             break;
    case 7: y -= step;             break;
    default: {
        u32 diag = LD32(obj + oTornado_AltStep);
        if (mode == 2) {
            x += diag;
            y += diag;
        } else {
            x -= diag;
            y -= diag;
        }
        break;
    }
    }

    ST32(obj + oTornado_PosX, x);
    ST32(obj + oTornado_PosY, y);
}

/* owMapObject_weatherTornado_step_80A9038(r5 = object ptr) */
Next owMapObject_weatherTornado_step_80A9038(void) {
    step_tornado_position(R5);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_loadParams_80A908C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: owMapObject_weatherTornado_getParams_80A916C has no typed C interface yet */
static inline u32 call_owMapObject_weatherTornado_getParams_80A916C_owMapObject_weatherTornado_loadParams_80A908C(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_getParams_80A916C);
    return R0;
}

/* Load the current weather-tornado parameters into an overworld map object.
 *
 * r5 = object to fill in, r10 = weather state (passed through to getParams).
 * Copies from the weather param block into the object:
 *   +0x2C: countdown timer (word)
 *   +0x30: half of that timer (word)
 *   +0x26: param byte 4 (sprite/anim id)
 *   +0x27: param byte 6
 *   +0x07: param byte 0xC
 */
static void load_tornado_params(u32 obj) {
    u32 params = call_owMapObject_weatherTornado_getParams_80A916C_owMapObject_weatherTornado_loadParams_80A908C(obj);

    u32 timer = LD32(params);
    ST32(obj + 0x2C, timer);
    ST32(obj + 0x30, timer >> 1);

    ST8(obj + 0x26, LD16(params + 4));
    ST8(obj + 0x27, LD16(params + 6));
    ST8(obj + 7,     LD16(params + 0xC));
}

Next owMapObject_weatherTornado_loadParams_80A908C(void) {
    load_tornado_params(R5);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_loadHome_80A90B0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: owMapObject_weatherTornado_getParams_80A916C has no typed C interface yet */
static inline u32 call_owMapObject_weatherTornado_getParams_80A916C_owMapObject_weatherTornado_loadHome_80A90B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_getParams_80A916C);
    return R0;
}

/* Load the tornado's home position: fetch the weather-tornado params for the map
 * object, copy its home X/Y coordinates into the object as 16.16 fixed point,
 * and start its timer/counter at 0x40 (also 16.16).
 *   r5 = map object, r10 = context (passed through to getParams) */
static void load_tornado_home(u32 obj) {
    u32 params = call_owMapObject_weatherTornado_getParams_80A916C_owMapObject_weatherTornado_loadHome_80A90B0(obj);

    ST32(obj + 0xC,  (u32)LD16(params + 0x8) << 16);   /* home X */
    ST32(obj + 0x10, (u32)LD16(params + 0xA) << 16);   /* home Y */
    ST32(obj + 0x14, 0x40 << 16);                      /* initial state value */
}

Next owMapObject_weatherTornado_loadHome_80A90B0(void) {
    load_tornado_home(R5);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_isBlocked_80A90CC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_checkCoordinateTrigger_8031a7a_result;
/* register form: generated wrapper: checkCoordinateTrigger_8031a7a has no typed C interface yet */
static inline call_checkCoordinateTrigger_8031a7a_result call_checkCoordinateTrigger_8031a7a(u32 r0) {
    R0 = r0;
    GUEST_CALL(checkCoordinateTrigger_8031a7a);
    return (call_checkCoordinateTrigger_8031a7a_result){ R0, R2, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* Weather tornado map-object blocking check.
 * Asks the coordinate-trigger table about the tile at offset 0xC from the
 * object coordinate in r5; that trigger returns 0xC0 when a tornado is
 * standing there, which means the object's move is blocked.
 * Returns 1 (blocked) or 0 (free). r5/r6/r8-r12 end up holding whatever
 * checkCoordinateTrigger_8031a7a leaves in them, as in the original. */
static u32 weather_tornado_is_blocked(u32 coordBase) {
    u32 trigger = call_checkCoordinateTrigger_8031a7a(coordBase + 0xC).r0;
    return trigger == 0xC0;
}

/* inputs: r5 (base coordinate), r10 (passed through to the trigger check)
 * outputs: r0 = 0/1, nz from that value; other outputs are callee leftovers */
Next owMapObject_weatherTornado_isBlocked_80A90CC(void) {
    u32 blocked = weather_tornado_is_blocked(R5);
    R0 = blocked;
    SET_FLAGS_NZ(blocked);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_repickDirection_80A90E4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r6, r7; } call_reqBBS_81409D0_result_owMapObject_weatherTornado_repickDirection_80A90E4;
/* register form: generated wrapper: reqBBS_81409D0 has no typed C interface yet */
static inline call_reqBBS_81409D0_result_owMapObject_weatherTornado_repickDirection_80A90E4 call_reqBBS_81409D0(u32 r0) {
    R0 = r0;
    GUEST_CALL(reqBBS_81409D0);
    return (call_reqBBS_81409D0_result_owMapObject_weatherTornado_repickDirection_80A90E4){ R0, R6, R7 };
}

/* register form: generated wrapper: owMapObject_weatherTornado_step_80A9038 has no typed C interface yet */
static inline void call_owMapObject_weatherTornado_step_80A9038_owMapObject_weatherTornado_repickDirection_80A90E4(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_step_80A9038);
}

typedef struct { u32 r5, r6, r8, r9, r10, r11, r12; bool z, n; } call_owMapObject_weatherTornado_isBlocked_80A90CC_result_owMapObject_weatherTornado_repickDirection_80A90E4;
/* register form: generated wrapper: owMapObject_weatherTornado_isBlocked_80A90CC has no typed C interface yet */
static inline call_owMapObject_weatherTornado_isBlocked_80A90CC_result_owMapObject_weatherTornado_repickDirection_80A90E4 call_owMapObject_weatherTornado_isBlocked_80A90CC_owMapObject_weatherTornado_repickDirection_80A90E4(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_weatherTornado_isBlocked_80A90CC);
    return (call_owMapObject_weatherTornado_isBlocked_80A90CC_result_owMapObject_weatherTornado_repickDirection_80A90E4){ R5, R6, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Pick a new movement direction for a weather tornado: roll randomly, step,
 * retry until the target tile is walkable, then update the per-weather
 * direction history and bump the spawn counter. */
static void repick_direction(u32 tornado, u32 weather_table) {
    u32 r5 = tornado;
    u32 r6 = weather_table;

    for (;;) {
        /* Roll a direction in [0, 8); reqBBS also clobbers r6/r7 */
        call_reqBBS_81409D0_result_owMapObject_weatherTornado_repickDirection_80A90E4 roll = call_reqBBS_81409D0(8);
        u32 new_dir = roll.r0;
        r6 = roll.r6;

        /* Stash current direction as "previous" */
        u32 cur_dir = LD8(r5 + 5);
        ST8(r5 + 6, cur_dir);
        if (new_dir == cur_dir)
            continue;

        /* Commit new direction and back up position */
        ST8(r5 + 5, new_dir);
        ST32(r5 + 0x18, LD32(r5 + 0xC));
        ST32(r5 + 0x1C, LD32(r5 + 0x10));

        /* Advance one tile */
        call_owMapObject_weatherTornado_step_80A9038_owMapObject_weatherTornado_repickDirection_80A90E4(r5);

        /* Check walkability – z=1 means free (exit loop) */
        call_owMapObject_weatherTornado_isBlocked_80A90CC_result_owMapObject_weatherTornado_repickDirection_80A90E4 blk =
            call_owMapObject_weatherTornado_isBlocked_80A90CC_owMapObject_weatherTornado_repickDirection_80A90E4(r5);
        r5 = blk.r5;
        r6 = blk.r6;

        if (blk.z)
            break;

        /* Blocked: restore position and previous direction, re-roll */
        ST32(r5 + 0xC, LD32(r5 + 0x18));
        ST32(r5 + 0x10, LD32(r5 + 0x1C));
        ST8(r5 + 5, LD8(r5 + 6));
    }

    /* Shift direction history for this weather slot:
     * table[0x31+w] → table[0x19+w], then table[0x31+w] = previous dir. */
    u32 weather = LD8(r5 + 4);
    ST8(r6 + 0x19 + weather, LD8(r6 + 0x31 + weather));
    ST8(r6 + 0x31 + weather, LD8(r5 + 6));

    /* Increment spawn counter */
    ST8(r5 + 0x28, LD8(r5 + 0x28) + 1);
}

/* owMapObject_weatherTornado_repickDirection(r5=tornado, r6=weather_table,
 * r10=context) -> r8 r9 r10 r11 r12 left from isBlocked wrapper */
Next owMapObject_weatherTornado_repickDirection_80A90E4(void) {
    repick_direction(R5, R6);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_isPauseDue_80A9150 ── */
/* Check whether a tornado map object has finished its pause timer.
 * The object keeps a countdown at +0x28; when it has counted down to the
 * threshold stored at +7, the timer is reset to 0 and the caller is told to
 * pause. Returns 1 when the pause fires, 0 otherwise; flags reflect the result
 * (the original's `tst r0, r0`). */
static u32 tornado_pause_due(u32 obj) {
    u8 timer = LD8(obj + 0x28);
    u8 threshold = LD8(obj + 7);

    if ((s32)timer >= (s32)threshold) {
        ST8(obj + 0x28, 0);   /* reset the countdown */
        return 1;
    }
    return 0;
}

/* owMapObject_weatherTornado_isPauseDue_80A9150(r5 = object) -> r0, nz */
Next owMapObject_weatherTornado_isPauseDue_80A9150(void) {
    R0 = tornado_pause_due(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_getParams_80A916C ── */
/* Weather tornado overworld map object: return the address of the parameter
 * record for the current map. The map table (byte_81436B4...) is indexed by
 * map number, except that map group 0x85 (underground) always uses entry 3.
 * Each table entry points to an array of 0x10-byte records; the byte at
 * [object+4] selects which one. */
static u32 weather_tornado_get_params(u32 object, u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);

    u32 map_index;
    if (LD8(game_state + oGameState_MapGroup) == 0x85)
        map_index = 3;
    else
        map_index = LD8(game_state + oGameState_MapNumber);

    /* The word at off_80A9190 holds the table address, which is off_80A9194. */
    u32 table = A_off_80A9194;
    u32 record = LD32(table + map_index * 4);

    u32 slot = (u32)LD8(object + 4) * 0x10;
    return record + slot;
}

/* inputs: r5 = map object, r10 = toolkit; outputs: r0 = parameter record */
Next owMapObject_weatherTornado_getParams_80A916C(void) {
    R0 = weather_tornado_get_params(R5, R10);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_checkHitPlayer_80A91A4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n, c, v; } call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4 call_TestEventFlagFromImmediate_owMapObject_weatherTornado_checkHitPlayer_80A91A4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4){ ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1; } call_ReadOWPlayerObjectCoords_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4 call_ReadOWPlayerObjectCoords_owMapObject_weatherTornado_checkHitPlayer_80A91A4(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4){ R0, R1 };
}

typedef struct { u32 r0, r2; } call_overworldObject_withinBoxCheck_signed_80011D6_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4;
/* register form: generated wrapper: overworldObject_withinBoxCheck_signed_80011D6 has no typed C interface yet */
static inline call_overworldObject_withinBoxCheck_signed_80011D6_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4 call_overworldObject_withinBoxCheck_signed_80011D6_owMapObject_weatherTornado_checkHitPlayer_80A91A4(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(overworldObject_withinBoxCheck_signed_80011D6);
    return (call_overworldObject_withinBoxCheck_signed_80011D6_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4){ R0, R2 };
}

/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline void call_SetEventFlagFromImmediate_owMapObject_weatherTornado_checkHitPlayer_80A91A4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* Weather-tornado overworld map object: check whether the tornado's hitbox
 * covers the player, and if so raise the "player caught in tornado" event.
 * r5 holds the tornado map object; r10 (the overworld state base) is read
 * directly by the callees and must stay untouched. */

/* Flag bank/index pairs used by movflag EVENT_BE4 / EVENT_BE6. */
#define FLAG_BANK_EVENT_B  0x0Bu
#define FLAG_IDX_BE4       0xE4u   /* tornado weather active */
#define FLAG_IDX_BE6       0xE6u   /* player hit by tornado  */

/* Half-extent passed to the box test, as written by the original (0x14 << 16). */
#define TORNADO_BOX_HALF_EXTENT  0x140000u

/* Set FLAG_IDX_BE6 when the player's position falls inside the tornado's box
 * (the object's bounds at +0xC/+0x10 with half-extent 0x140000), provided the
 * tornado weather event (BE4) is set. */
static void tornado_check_hit_player(u32 object) {
    /* No tornado weather active: nothing to check. */
    if (call_TestEventFlagFromImmediate_owMapObject_weatherTornado_checkHitPlayer_80A91A4(FLAG_BANK_EVENT_B, FLAG_IDX_BE4).z)
        return;

    call_ReadOWPlayerObjectCoords_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4 player = call_ReadOWPlayerObjectCoords_owMapObject_weatherTornado_checkHitPlayer_80A91A4();

    call_overworldObject_withinBoxCheck_signed_80011D6_result_owMapObject_weatherTornado_checkHitPlayer_80A91A4 hit =
        call_overworldObject_withinBoxCheck_signed_80011D6_owMapObject_weatherTornado_checkHitPlayer_80A91A4(
            LD32(object + 0xC),          /* box left/top bound  */
            LD32(object + 0x10),         /* box right/bottom bound */
            player.r0, player.r1,
            TORNADO_BOX_HALF_EXTENT);

    if (hit.r0 == 1)
        call_SetEventFlagFromImmediate_owMapObject_weatherTornado_checkHitPlayer_80A91A4(FLAG_BANK_EVENT_B, FLAG_IDX_BE6);
}

/* inputs: r5 r10 lr; outputs: none (flags clobbered, r4 preserved by construction) */
Next owMapObject_weatherTornado_checkHitPlayer_80A91A4(void) {
    tornado_check_hit_player(R5);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_setPriority_80A91F4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n; } call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_setPriority_80A91F4;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_setPriority_80A91F4 call_TestEventFlagFromImmediate_owMapObject_weatherTornado_setPriority_80A91F4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_owMapObject_weatherTornado_setPriority_80A91F4){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline void call_sprite_setPriority_8002E14_owMapObject_weatherTornado_setPriority_80A91F4(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result_owMapObject_weatherTornado_setPriority_80A91F4;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result_owMapObject_weatherTornado_setPriority_80A91F4 call_applyLayerEffectToOWObject_8035694_owMapObject_weatherTornado_setPriority_80A91F4(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result_owMapObject_weatherTornado_setPriority_80A91F4){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* Weather "tornado" overworld map object: sprite priority update.
 *
 * r5 holds the object's work area; the BE6 event flag says whether the
 * tornado is currently rendered as a foreground overlay. When it is, the
 * sprite is forced to priority 1 (in front of the map). Otherwise the
 * priority is derived by the generic layer-effect code, which is pointed at
 * the layer record at r5+0xC and leaves both the priority value (r0) and the
 * object pointer (r5) behind for sprite_setPriority.
 *
 * The other output registers (r6, r8-r12) are only values left behind by
 * applyLayerEffectToOWObject on the layer-effect path; the wrapper call
 * already leaves them in the guest registers, and on the flag-set path the
 * original leaves them unchanged, so nothing needs to be written here. */
static void tornado_set_priority(u32 obj) {
    if (call_TestEventFlagFromImmediate_owMapObject_weatherTornado_setPriority_80A91F4(0xB, 0xE6).z) {   /* EVENT_BE6: bank 11, flag 0xE6; Z set = flag clear */
        /* Tornado not overlaying: let the layer effect decide the priority. */
        call_applyLayerEffectToOWObject_8035694_result_owMapObject_weatherTornado_setPriority_80A91F4 effect =
            call_applyLayerEffectToOWObject_8035694_owMapObject_weatherTornado_setPriority_80A91F4(obj + 0xC);
        call_sprite_setPriority_8002E14_owMapObject_weatherTornado_setPriority_80A91F4(effect.r0, effect.r5);
    } else {
        /* Tornado overlaying: force high sprite priority. */
        call_sprite_setPriority_8002E14_owMapObject_weatherTornado_setPriority_80A91F4(1, obj);
    }
}

/* owMapObject_weatherTornado_setPriority_80A91F4(r5 = object, r10 = context)
 * -> r5 r6 r8 r9 r10 r11 r12 */
Next owMapObject_weatherTornado_setPriority_80A91F4(void) {
    tornado_set_priority(R5);
    RETURN_TO_CALLER();
}

/* ── owMapObject_weatherTornado_saveState_80A9218 ── */
/* Copy weather tornado map object state fields into the save buffer,
 * indexed by the object's slot number. Returns the halfword stride (slot * 2). */
static u32 tornado_save_state(u32 map_obj, u32 save_buf) {
    u32 slot = LD8(map_obj + 4);  /* slot index within the save buffer */

    /* Five byte-sized state fields */
    ST8(save_buf + 0x28 + slot, LD8(map_obj + 9));
    ST8(save_buf + 0x31 + slot, LD8(map_obj + 5));
    ST8(save_buf + 0x37 + slot, LD8(map_obj + 0x24));
    ST8(save_buf + 0x3A + slot, LD8(map_obj + 0x25));
    ST8(save_buf + 0x3D + slot, LD8(map_obj + 0x28));

    /* Two 32-bit fields stored as their upper halfword (16-bit sign-extended shift) */
    ST16(save_buf + 0x40 + slot * 2, (u32)((s32)LD32(map_obj + 0xC) >> 16));
    ST16(save_buf + 0x46 + slot * 2, (u32)((s32)LD32(map_obj + 0x10) >> 16));

    return slot * 2;
}

/* owMapObject_weatherTornado_saveState(r5 = map obj, r6 = save buf) -> r2 */
Next owMapObject_weatherTornado_saveState_80A9218(void) {
    R2 = tornado_save_state(R5, R6);
    RETURN_TO_CALLER();
}

/* ── battle_weatherCompDispatch_80A92B8 ── */
/* Weather-comparison state machine dispatcher (0x080A92B8).
 *
 * r5 holds a battle object; the byte at offset +8 selects the current weather
 * state and indexes the handler table off_80A92CC:
 *   0: sprite_initBeastSp   1: boss_updateStateByStoryFlags_80A931C
 *   2: sub_80A9360          3: freeOverworldMapObject_trampoline_80A93D0
 * Before the call, r6 is loaded with the address of the shared state block
 * eWeatherCompState, and r10 is passed through to the handler untouched. */

/* register form: the handler is chosen at run time from the jump table
 * off_80A92CC, and it expects the object in r5, the shared state block
 * address in r6 and r10 passed through — none of which typed parameters
 * can express. r0 keeps the raw table entry exactly as the original's
 * `mov lr, pc; bx r0` leaves it. */
static void weather_dispatch_handler(u32 object, u32 r10_value) {
    u32 state = LD8(object + 8);
    u32 handler = LD32(A_off_80A92CC + state);

    R0 = handler;             /* table entry (Thumb address), as in the original */
    R1 = state;               /* the state byte the dispatch was based on */
    R5 = object;              /* object work area, also visible to the handler */
    R6 = A_eWeatherCompState; /* shared weather state block */
    R10 = r10_value;          /* passed through to every handler */
    GUEST_CALL_ADDR(handler);
}

/* Typed body: everything the callers observe (r2-r5, r8-r12) is whatever the
 * chosen handler leaves behind, so there is nothing to compute or return. */
static void sub_80A92B8_body(u32 object, u32 r10_value) {
    weather_dispatch_handler(object, r10_value);
}

/* battle_weatherCompDispatch_80A92B8(r5 = object, r10) -> r2 r3 r4 r5 r8 r9 r10 r11 r12 */
Next battle_weatherCompDispatch_80A92B8(void) {
    sub_80A92B8_body(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sprite_initBeastSp ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result call_sprite_load_sprite_initBeastSp(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_sprite_initBeastSp(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_sprite_initBeastSp;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_sprite_initBeastSp call_sprite_loadAnimationData_sprite_initBeastSp(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_sprite_initBeastSp){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_sprite_initBeastSp(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r1, r2; } call_sprite_setPriority_8002E14_result_sprite_initBeastSp;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_sprite_initBeastSp call_sprite_setPriority_8002E14_sprite_initBeastSp(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_sprite_initBeastSp){ R1, R2 };
}

/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline void call_sprite_setPalette_sprite_initBeastSp(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
}

typedef struct { u32 r2, r3, r8, r9, r10, r11, r12; } call_sub_80A931C_result;
/* register form: generated wrapper: boss_updateStateByStoryFlags_80A931C has no typed C interface yet */
static inline call_sub_80A931C_result call_sub_80A931C(u32 r5) {
    R5 = r5;
    GUEST_CALL(boss_updateStateByStoryFlags_80A931C);
    return (call_sub_80A931C_result){ R2, R3, R8, R9, R10, R11, R12 };
}

/* Initialise the Beast Out sprite state (r5) for its owning object (r6).
 *
 * Everything this function leaves behind for its caller (r2/r3/r8-r12) comes
 * from the final call to boss_updateStateByStoryFlags_80A931C, whose wrapper leaves those registers in
 * place; r10 passes straight through to that helper unchanged. r4-r7 are
 * saved and restored by the original's push/pop, so they end up exactly as
 * they entered and need no handling here. */
static void init_beast_out_sprite(u32 sprite, u32 owner) {
    /* Sprite kind: Beast Out. */
    ST8(sprite, 3);

    /* Load the Beast Out graphics (128x28 tiles, 0x6C-byte stride). */
    call_sprite_load_sprite_initBeastSp(0x80, 0x1C, 0x6C, sprite);

    /* Animation, animation data and shadow removal all come from the sprite. */
    call_sprite_setAnimation_sprite_initBeastSp(LD8(sprite + 4), sprite);
    call_sprite_loadAnimationData_sprite_initBeastSp(sprite);
    call_sprite_noShadow_sprite_initBeastSp(sprite);

    /* Priority from the sprite state, palette from the owning object. */
    call_sprite_setPriority_8002E14_sprite_initBeastSp(LD8(sprite + 5), sprite);
    call_sprite_setPalette_sprite_initBeastSp(LD8(owner + 0x1C), sprite);

    /* Reset the animation counters and set the frame delay. */
    ST8(sprite + 6, 0);
    ST8(sprite + 7, 0);
    ST32(sprite + 0x24, 0);
    ST8(sprite + 8, 4);

    /* Beast Out specific finish-up; its register results belong to our caller. */
    call_sub_80A931C(sprite);
}

/* sprite_initBeastSp(r5 = sprite state, r6 = owning object, r10 = context)
 * -> r2 r3 r4 r5 r8 r9 r10 r11 r12 (all left by boss_updateStateByStoryFlags_80A931C, except r4/r5
 * which the original restores to their entry values) */
Next sprite_initBeastSp(void) {
    init_beast_out_sprite(R5, R6);
    RETURN_TO_CALLER();
}

/* ── boss_updateStateByStoryFlags_80A931C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; bool z, n; } call_TestEventFlagFromImmediate_result_sub_80A931C;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_sub_80A931C call_TestEventFlagFromImmediate_sub_80A931C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_sub_80A931C){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r1, r2, r3; } call_sprite_setPriority_8002E14_result_sub_80A931C;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_sub_80A931C call_sprite_setPriority_8002E14_sub_80A931C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_sub_80A931C){ R1, R2, R3 };
}

typedef struct { u32 r1, r2, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_sub_80A931C;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_80A931C call_PlaySoundEffect_sub_80A931C(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_80A931C){ R1, R2, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_sub_80A9360_result;
/* register form: generated wrapper: sub_80A9360 has no typed C interface yet */
static inline call_sub_80A9360_result call_sub_80A9360(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80A9360);
    return (call_sub_80A9360_result){ R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r5; } call_sub_80A93D0_result;
/* register form: generated wrapper: freeOverworldMapObject_trampoline_80A93D0 has no typed C interface yet */
static inline call_sub_80A93D0_result call_sub_80A93D0(u32 r5) {
    R5 = r5;
    GUEST_CALL(freeOverworldMapObject_trampoline_80A93D0);
    return (call_sub_80A93D0_result){ R5 };
}

/* register form: generated wrapper: sprite_setUnk2cToE000000_80A93D8 has no typed C interface yet */
static inline void call_sub_80A93D8(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_setUnk2cToE000000_80A93D8);
}

/* register form: generated wrapper: sub_80A93E8 has no typed C interface yet */
static inline void call_sub_80A93E8(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_80A93E8);
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_sprite_update_result_sub_80A931C;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_sub_80A931C call_sprite_update_sub_80A931C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_sub_80A931C){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Per-frame update for the boss actor (r5 = object, r10 = toolkit context).
 *
 * Three mutually exclusive cases, chosen by story flags:
 *   - EVENT_BE6 set: the "hit" reaction — lift the sprite's priority,
 *     store state 8, play the hit sound and run the hit sub-step.
 *   - EVENT_BE4 set: store state 12 and run that sub-step instead.
 *   - neither: run the two idle sub-steps.
 * sprite_update runs afterwards on every path.
 *
 * The original pushes r4-r7 on entry and pops them on the way out, so those
 * four registers must end with their entry values even though the callees
 * trash the live copies; the entry values are returned to the adapter so it
 * can put them back. */
typedef struct {
    u32 r4; /* entry r4, to restore before returning */
    u32 r5; /* entry r5, to restore before returning */
    u32 r6; /* entry r6, to restore before returning */
    u32 r7; /* entry r7, to restore before returning */
} SavedRegs_sub_80A931C;

static SavedRegs_sub_80A931C sub_80A931C_body(u32 obj, u32 context, u32 r4, u32 r6, u32 r7) {
    SavedRegs_sub_80A931C saved;

    saved.r4 = r4;
    saved.r5 = obj;
    saved.r6 = r6;
    saved.r7 = r7;

    /* TestEventFlagFromImmediate: Z clear when the flag is set. */
    if (!call_TestEventFlagFromImmediate_sub_80A931C(0xB, 0xE6).z) {        /* EVENT_BE6 */
        /* Hit reaction: priority 3, state 8, hit sound, hit sub-step. */
        call_sprite_setPriority_8002E14_result_sub_80A931C prio =
            call_sprite_setPriority_8002E14_sub_80A931C(3, obj);
        ST8(obj + 8, 8);
        /* r2 is whatever sprite_setPriority left behind, as in the original. */
        call_PlaySoundEffect_sub_80A931C(SOUND_HIT_6B, prio.r2);
        call_sub_80A9360(obj);
    } else if (!call_TestEventFlagFromImmediate_sub_80A931C(0xB, 0xE4).z) { /* EVENT_BE4 */
        ST8(obj + 8, 0xC);
        call_sub_80A93D0(obj);
    } else {
        call_sub_80A93D8(obj);
        call_sub_80A93E8(obj);
    }

    call_sprite_update_sub_80A931C(obj);
    return saved;
}

/* boss_updateStateByStoryFlags_80A931C(r5 = object, r10 = context) -> r2 r3 r4 r5 r8 r9 r10 r11 r12
 * (r2/r3 and r8-r12 are whatever the callees leave behind; r4-r7 are
 * restored to their entry values, as the original's pop does). */
Next boss_updateStateByStoryFlags_80A931C(void) {
    SavedRegs_sub_80A931C saved = sub_80A931C_body(R5, R10, R4, R6, R7);
    R4 = saved.r4;
    R5 = saved.r5;
    R6 = saved.r6;
    R7 = saved.r7;
    RETURN_TO_CALLER();
}

/* ── freeOverworldMapObject_trampoline_80A93D0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_FreeOverworldMapObject_result_sub_80A93D0;
/* register form: generated wrapper: FreeOverworldMapObject has no typed C interface yet */
static inline call_FreeOverworldMapObject_result_sub_80A93D0 call_FreeOverworldMapObject_sub_80A93D0(u32 r5) {
    R5 = r5;
    GUEST_CALL(FreeOverworldMapObject);
    return (call_FreeOverworldMapObject_result_sub_80A93D0){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * freeOverworldMapObject_trampoline_80A93D0 is a pure trampoline: it forwards the map-object handle in r5
 * straight to FreeOverworldMapObject and lets everything the callee leaves
 * behind (r0-r3, r5, the N/Z/C/V flags and lr) reach its caller unchanged.
 * The original does this with push {lr} / bl / pop {pc}, i.e. a tail call.
 */

/* Perform the forwarded call and hand back everything FreeOverworldMapObject
 * left in the registers and flags. */
static call_FreeOverworldMapObject_result_sub_80A93D0 free_overworld_map_object_via(u32 mapObject)
{
    return call_FreeOverworldMapObject_sub_80A93D0(mapObject);
}

/* freeOverworldMapObject_trampoline_80A93D0(r5 = map object) -> r0 r1 r2 r3 r5, nz c v */
Next freeOverworldMapObject_trampoline_80A93D0(void) {
    call_FreeOverworldMapObject_result_sub_80A93D0 res = free_overworld_map_object_via(R5);

    R0 = res.r0;
    R1 = res.r1;
    R2 = res.r2;
    R3 = res.r3;
    R5 = res.r5;
    /* nz/c/v and lr are already exactly as FreeOverworldMapObject left them,
     * so there is nothing more to set before returning. */
    RETURN_TO_CALLER();
}

/* ── sprite_setUnk2cToE000000_80A93D8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sprite_setUnk0x2c has no typed C interface yet */
static inline void call_sprite_setUnk0x2c_sub_80A93D8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setUnk0x2c);
}

/* Reset some sprite field to 0xE000000 for the object whose state is in r5.
 * (sprite_setUnk0x2c takes the value in r0 and the object pointer in r5.) */
static void reset_sprite_unk2c(u32 object_state) {
    call_sprite_setUnk0x2c_sub_80A93D8(0xE000000, object_state);
}

/* sprite_setUnk2cToE000000_80A93D8(r5 = object state) -> - */
Next sprite_setUnk2cToE000000_80A93D8(void) {
    reset_sprite_unk2c(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80A93E8 ── */
#define oBattleObject_AnimPhase    6
#define oBattleObject_AnimCounter  7
#define oBattleObject_AnimPosition 0x14
#define ANIM_PHASE_COUNT           0x18
#define ANIM_STEP                  (1 << 14)

/* Oscillate a fixed-point position back and forth over 24 steps per phase.
 * Phase byte toggles 0→1→0 each time the counter wraps at 24. */
static void animate_oscillation(u32 obj) {
    u32 phase = LD8(obj + oBattleObject_AnimPhase);
    u32 counter = LD8(obj + oBattleObject_AnimCounter) + 1;
    ST8(obj + oBattleObject_AnimCounter, counter);

    if (phase == 0) {
        if (counter < ANIM_PHASE_COUNT) {
            ST32(obj + oBattleObject_AnimPosition,
                 LD32(obj + oBattleObject_AnimPosition) - ANIM_STEP);
        } else {
            ST8(obj + oBattleObject_AnimPhase, 1);
            ST8(obj + oBattleObject_AnimCounter, 0);
        }
    } else {
        if (counter < ANIM_PHASE_COUNT) {
            ST32(obj + oBattleObject_AnimPosition,
                 LD32(obj + oBattleObject_AnimPosition) + ANIM_STEP);
        } else {
            ST8(obj + oBattleObject_AnimPhase, 0);
            ST8(obj + oBattleObject_AnimCounter, 0);
        }
    }
}

/* sub_80A93E8(r5 = battle object pointer) */
Next sub_80A93E8(void) {
    animate_oscillation(R5);
    RETURN_TO_CALLER();
}

/* ── weatherMapObjectStateDispatch_80A9430 ── */
/* Weather map-object dispatcher.
 *
 * The object at r5 keeps a small state machine number at offset 8; each state
 * has its own handler in the jump table at off_80A9444:
 *   0: owMapObject_mrWeatherInit_80A9458  (spawn/init)
 *   1: object_mrWeatherStep_80A94DC       (per-frame step)
 *   2: object_fadeSpriteToWhiteStep                        (extra state)
 *   3: owMapObject_freeThunk              (despawn/free)
 * The handler is invoked with r0 = its own address, r1 = the state number,
 * r5 = the object, r6 = address of eWeatherCompState, r10 passed through.
 * Everything the handler leaves in r2-r5/r8-r12 is this routine's output,
 * so those registers are simply left in place after the call.
 */

/* register form: dispatches through the state jump table address; the callee
 * leaves this routine's outputs (r2-r5, r8-r12) directly in the registers. */
static void run_weather_handler(u32 handler, u32 state, u32 objectPtr, u32 r10In) {
    R0 = handler;              /* handlers are entered with their own address in r0 */
    R1 = state;
    R5 = objectPtr;
    R6 = A_eWeatherCompState;  /* handler argument: the weather component's state block */
    R10 = r10In;
    GUEST_CALL_ADDR(handler);
}

static void weather_state_dispatch(u32 objectPtr, u32 r10In) {
    u32 state = LD8(objectPtr + 8);
    u32 handler = LD32(A_off_80A9444 + state);
    run_weather_handler(handler, state, objectPtr, r10In);
}

/* weatherMapObjectStateDispatch_80A9430(r5 = weather object, r10 = pass-through) */
Next weatherMapObjectStateDispatch_80A9430(void) {
    weather_state_dispatch(R5, R10);
    RETURN_TO_CALLER();
}

/* ── object_mrWeatherStep_80A94DC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n, c, v; } call_TestEventFlagFromImmediate_result_object_mrWeatherStep_80A94DC;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_object_mrWeatherStep_80A94DC call_TestEventFlagFromImmediate_object_mrWeatherStep_80A94DC(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_object_mrWeatherStep_80A94DC){ ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r5, r10; } call_object_mrWeatherProximityTrigger_80A95A8_result;
/* register form: generated wrapper: object_mrWeatherProximityTrigger_80A95A8 has no typed C interface yet */
static inline call_object_mrWeatherProximityTrigger_80A95A8_result call_object_mrWeatherProximityTrigger_80A95A8(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(object_mrWeatherProximityTrigger_80A95A8);
    return (call_object_mrWeatherProximityTrigger_80A95A8_result){ R5, R10 };
}

typedef struct { u32 r5, r6, r8, r9, r10, r11, r12; } call_owMapObject_updateLayerPriorityForPlayerZ_80A95EC_result;
/* register form: generated wrapper: owMapObject_updateLayerPriorityForPlayerZ_80A95EC has no typed C interface yet */
static inline call_owMapObject_updateLayerPriorityForPlayerZ_80A95EC_result call_owMapObject_updateLayerPriorityForPlayerZ_80A95EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_updateLayerPriorityForPlayerZ_80A95EC);
    return (call_owMapObject_updateLayerPriorityForPlayerZ_80A95EC_result){ R5, R6, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_sprite_update_result_object_mrWeatherStep_80A94DC;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_object_mrWeatherStep_80A94DC call_sprite_update_object_mrWeatherStep_80A94DC(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_object_mrWeatherStep_80A94DC){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Weather event gate flag: page 11, offset 0xFC (EVENT_BFC) */
#define WEATHER_EVENT_PAGE   0xB
#define WEATHER_EVENT_OFFSET 0xFC

struct weather_step_result {
    u32 r2;
    u32 r3;
    u32 r10;
    u32 r11;
};

/* Advance the weather proximity object:
 * – if the event gate flag is active, test the player's proximity zone;
 * – recompute draw-layer priority from the player's world-Z;
 * – update the object's sprite frame.
 * Returns the registers the caller observes as changed (r2, r3, r10, r11);
 * r4–r9 and r12 are preserved by the caller-saved push/pop in the original. */
static struct weather_step_result weather_step(u32 r5, u32 r6, u32 r10) {
    struct weather_step_result result = {0};

    /* Z set ⇒ event flag is active ⇒ run proximity trigger */
    call_TestEventFlagFromImmediate_result_object_mrWeatherStep_80A94DC test =
        call_TestEventFlagFromImmediate_object_mrWeatherStep_80A94DC(WEATHER_EVENT_PAGE, WEATHER_EVENT_OFFSET);

    if (test.z) {
        call_object_mrWeatherProximityTrigger_80A95A8_result prox =
            call_object_mrWeatherProximityTrigger_80A95A8(r5, r6);
        r5  = prox.r5;
        r10 = prox.r10;
    }

    call_owMapObject_updateLayerPriorityForPlayerZ_80A95EC_result layer =
        call_owMapObject_updateLayerPriorityForPlayerZ_80A95EC(r5);
    r10 = layer.r10;

    call_sprite_update_result_object_mrWeatherStep_80A94DC spr = call_sprite_update_object_mrWeatherStep_80A94DC(layer.r5);

    result.r2  = spr.r2;
    result.r3  = spr.r3;
    result.r10 = r10;
    result.r11 = layer.r11;
    return result;
}

/* object_mrWeatherStep_80A94DC(r5, r6, r10) -> r2, r3, r10, r11 */
Next object_mrWeatherStep_80A94DC(void) {
    struct weather_step_result res = weather_step(R5, R6, R10);
    R2  = res.r2;
    R3  = res.r3;
    R10 = res.r10;
    R11 = res.r11;
    RETURN_TO_CALLER();
}

/* ── object_fadeSpriteToWhiteStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sprite_setColorShader has no typed C interface yet */
static inline void call_sprite_setColorShader(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setColorShader);
}

/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline void call_ClearEventFlagFromImmediate_object_fadeSpriteToWhiteStep(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

typedef struct { u32 r5; } call_owMapObject_freeThunk_result;
/* register form: generated wrapper: owMapObject_freeThunk has no typed C interface yet */
static inline call_owMapObject_freeThunk_result call_owMapObject_freeThunk(u32 r5) {
    R5 = r5;
    GUEST_CALL(owMapObject_freeThunk);
    return (call_owMapObject_freeThunk_result){ R5 };
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_sprite_update_result_object_fadeSpriteToWhiteStep;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_object_fadeSpriteToWhiteStep call_sprite_update_object_fadeSpriteToWhiteStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_object_fadeSpriteToWhiteStep){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Per-frame data for the "fade to white" sprite effect. */
#define FADE_TO_WHITE_TABLE       A_byte_80A9544   /* 16 bytes: Y-shift delta per frame; 0xFF = done */
#define FADE_TO_WHITE_COLOR_TABLE A_byte_80A9558   /* 16 bytes: intensity (RGB5 replicated) per frame */

#define FADE_COMPLETE_MARKER 0xFF

/* Object step counter at +0x24 (frames 0..N, divided by 2 for table index) */
#define oSprite_fadeStep   0x24
/* Object Y position (fixed-point) at +0x14; delta is added in the upper halfword */
#define oSprite_Y          0x14

/* EVENT_BFC = 0x0B, sub-flag = 0xFC (the fade trigger flag for this sprite) */
#define FADE_EVENT_BANK    0x0B
#define FADE_EVENT_FLAG    0xFC

/* Advance one frame of the "fade to white" animation.
 * Each frame shifts the sprite's Y upward and rewrites its color shader toward white.
 * When the fade table returns 0xFF the effect is complete: the triggering event flag is
 * cleared, the map-object thunk is freed, and sprite_update still runs once to reflect
 * the change. sprite_update's r2/r3 become this function's outputs. */
static call_sprite_update_result_object_fadeSpriteToWhiteStep fade_to_white_step(u32 obj) {
    u32 step = LD32(obj + oSprite_fadeStep);
    u32 frame = step >> 1;
    u32 delta = LD8(FADE_TO_WHITE_TABLE + frame);

    if (delta == FADE_COMPLETE_MARKER) {
        /* Fade finished – clear the triggering event flag and free the thunk */
        call_ClearEventFlagFromImmediate_object_fadeSpriteToWhiteStep(FADE_EVENT_BANK, FADE_EVENT_FLAG);
        call_owMapObject_freeThunk(obj);
    } else {
        /* Shift Y upward: delta lands in bits [31:16] of the fixed-point Y field */
        ST32(obj + oSprite_Y, LD32(obj + oSprite_Y) + (delta << 16));

        /* Build a white/gray RGB5 color (each channel gets the same intensity) */
        u32 intensity = LD8(FADE_TO_WHITE_COLOR_TABLE + frame);
        u32 color = intensity | (intensity << 5) | (intensity << 10);
        call_sprite_setColorShader(color, obj);
    }

    /* Advance the step counter and update the sprite (produces r2/r3 outputs) */
    ST32(obj + oSprite_fadeStep, LD32(obj + oSprite_fadeStep) + 1);
    return call_sprite_update_object_fadeSpriteToWhiteStep(obj);
}

/* object_fadeSpriteToWhiteStep(r5 = sprite obj, r10 = event bank) -> r2 r3 r4 r5 */
Next object_fadeSpriteToWhiteStep(void) {
    call_sprite_update_result_object_fadeSpriteToWhiteStep res = fade_to_white_step(R5);
    R2 = res.r2;
    R3 = res.r3;
    /* R4, R5 are preserved (unchanged) – handled automatically */
    RETURN_TO_CALLER();
}

/* ── owMapObject_freeThunk ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_FreeOverworldMapObject_result_owMapObject_freeThunk;
/* register form: generated wrapper: FreeOverworldMapObject has no typed C interface yet */
static inline call_FreeOverworldMapObject_result_owMapObject_freeThunk call_FreeOverworldMapObject_owMapObject_freeThunk(u32 r5) {
    R5 = r5;
    GUEST_CALL(FreeOverworldMapObject);
    return (call_FreeOverworldMapObject_result_owMapObject_freeThunk){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* owMapObject_freeThunk: tail-thunk around FreeOverworldMapObject.
 *
 * The original just pushes its return address, BLs FreeOverworldMapObject with
 * the map-object pointer passed through in r5, and pops the return address
 * back into PC. Every output of this thunk (r0-r3, r5, nz/c/v) is exactly
 * what FreeOverworldMapObject leaves behind, so the generated wrapper's call
 * already produces all of it in the guest state; nothing is computed here.
 * LR is only the return channel (pop {pc} returns to the caller's address),
 * so it is left to the return mechanism. */

/* Run FreeOverworldMapObject on the object pointer from r5. */
static void free_overworld_map_object(u32 object) {
    (void)call_FreeOverworldMapObject_owMapObject_freeThunk(object);
}

Next owMapObject_freeThunk(void) {
    free_overworld_map_object(R5);
    RETURN_TO_CALLER();
}

/* ── map_getPerMapTableRowPtr_80A9570 ── */
/* map_getPerMapTableRowPtr_80A9570: returns the address of an 8-byte-per-row table for the current
 * map, offset by the row index stored at [r5,#4].
 *   index = (mapGroup == 0x85) ? 3 : mapNumber  ->  pick one of four tables
 *   result = table_base + 8 * row_index
 */
static u32 map_table_address(u32 objPtr, u32 toolkitPtr) {
    u32 gameState = LD32(toolkitPtr + oToolkit_GameStatePtr);
    u32 index = (LD8(gameState + oGameState_MapGroup) == 0x85)
                  ? 3
                  : LD8(gameState + oGameState_MapNumber);
    u32 tableBase = LD32(A_off_80A9598 + index * 4);
    return tableBase + LD8(objPtr + 4) * 8;
}

Next map_getPerMapTableRowPtr_80A9570(void) {
    R0 = map_table_address(R5, R10);
    RETURN_TO_CALLER();
}

/* ── owMapObject_updateLayerPriorityForPlayerZ_80A95EC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_ReadOWPlayerObjectCoords_result_owMapObject_updateLayerPriorityForPlayerZ_80A95EC;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result_owMapObject_updateLayerPriorityForPlayerZ_80A95EC call_ReadOWPlayerObjectCoords_owMapObject_updateLayerPriorityForPlayerZ_80A95EC(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result_owMapObject_updateLayerPriorityForPlayerZ_80A95EC){ R2 };
}

typedef struct { u32 r2; } call_sprite_setPriority_8002E14_result;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result call_sprite_setPriority_8002E14_owMapObject_updateLayerPriorityForPlayerZ_80A95EC(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result){ R2 };
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result_owMapObject_updateLayerPriorityForPlayerZ_80A95EC;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result_owMapObject_updateLayerPriorityForPlayerZ_80A95EC call_applyLayerEffectToOWObject_8035694_owMapObject_updateLayerPriorityForPlayerZ_80A95EC(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result_owMapObject_updateLayerPriorityForPlayerZ_80A95EC){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* Update an overworld map object's sprite layer/priority from the player's
 * current Z position. r5 holds the map object; r10 is context the callees
 * read themselves (it comes out unchanged). */
static void update_map_object_layer_priority(u32 object) {
    /* Objects whose flag byte at +7 is clear are skipped entirely. */
    if (LD8(object + 7) == 0)
        return;

    /* Ask where the player is standing; r2 is the player's layer. */
    u32 playerLayer = call_ReadOWPlayerObjectCoords_owMapObject_updateLayerPriorityForPlayerZ_80A95EC().r2;

    if (playerLayer == 0) {
        /* Player on the low layer: the object simply gets priority 3. */
        call_sprite_setPriority_8002E14_owMapObject_updateLayerPriorityForPlayerZ_80A95EC(3, object);
        return;
    }

    /* Otherwise let the object's sprite (+0xC) pick up the tile's layer
     * effect, then set its priority to the value that call produced. */
    u32 priority = call_applyLayerEffectToOWObject_8035694_owMapObject_updateLayerPriorityForPlayerZ_80A95EC(object + 0xC).r0;
    call_sprite_setPriority_8002E14_owMapObject_updateLayerPriorityForPlayerZ_80A95EC(priority, object);
}

/* owMapObject_updateLayerPriorityForPlayerZ_80A95EC
 * inputs: r5 (map object), r10; outputs: r5 r6 r8 r9 r10 r11 r12 are the
 * values the callees leave behind, so the adapter writes nothing itself. */
Next owMapObject_updateLayerPriorityForPlayerZ_80A95EC(void) {
    update_map_object_layer_priority(R5);
    RETURN_TO_CALLER();
}

/* ── setFlagFromMenuSelection_80A9614 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline void call_SetEventFlag_sub_80A9614(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
}

/* Set the event flag selected by the player's current menu selection:
 * r5 points to a work area whose +4 byte is the selection index, which
 * indexes a table of flag IDs at byte_80A9648. SetEventFlag also reads r10. */
static void set_selected_event_flag(u32 work) {
    u32 index = (u32)LD8(work + 4) * 2;
    u16 flag_id = LD16(A_byte_80A9648 + index);
    call_SetEventFlag_sub_80A9614(flag_id);   /* r10 (some flag context) stays as on entry */
}

/* setFlagFromMenuSelection_80A9614(r5 = work area, r10 = flag context) -> - */
Next setFlagFromMenuSelection_80A9614(void) {
    set_selected_event_flag(R5);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchObjectBehaviorStep_80A9B70 ── */
/* battle_dispatchObjectBehaviorStep_80A9B70: dispatch one battle-object behaviour step.
 *
 * r5 points to the object: r5+4 is a character id selecting a 0x10-byte
 * entry in byte_80A9AA0 (left in r7 for the handler), r5+8 is a byte
 * offset (0, 4 or 8) into the handler table at off_80A9B8C. The selected
 * handler pointer is loaded into r0 (the original does ldr r0,[r0,r1] /
 * mov lr,pc / bx r0, so the callee starts with r0 = its own entry point)
 * and invoked; all other registers pass through unchanged. Control
 * returns straight to this function's caller when the handler finishes,
 * with r0..r7 and the flags as the handler left them. */
Next battle_dispatchObjectBehaviorStep_80A9B70(void) {
    u32 handlerOffset = LD8(R5 + 8);

    R7 = A_byte_80A9AA0 + 0x10 * LD8(R5 + 4);
    SET_FLAGS_ADD(A_byte_80A9AA0, 0x10 * LD8(R5 + 4));

    R0 = LD32(A_off_80A9B8C + handlerOffset);

    switch (handlerOffset) {
    case 0: GUEST_CALL(ow_spawnObjectFromConfig_80A9B9C); break;
    case 4: GUEST_CALL(eventFlagNpcUpdate_80A9C26); break;
    case 8: GUEST_CALL(sub_80A9CA6); break;
    }

    RETURN_TO_CALLER();
}

/* ── ow_spawnObjectFromConfig_80A9B9C ── */
/* ow_spawnObjectFromConfig_80A9B9C — bring a freshly spawned overworld object live.
 *
 * r5 = object state block, r7 = spawn config table entry. The routine:
 *   - marks the object "spawning" (state 3) and clears its timer (+0x24),
 *   - resolves the spawn flags from the config (+0/+2/+4): on a repeat spawn
 *     the object's event flag is (re)set and the companion flag cleared;
 *     on a first spawn the object is parked in state 1 instead,
 *   - loads the sprite described by the config (+6/+7) and applies the
 *     animation (+8), unk0x2c (+0xA), palette (+0xB), priority (+0xC, with
 *     0xFF meaning "derive from the object's layer/z") and flip (+0xD),
 *   - marks the object active (state 4 at +8) and calls eventFlagNpcUpdate_80A9C26 to finish.
 *
 * Everything the original leaves behind in r0-r3, r6 and the flags on exit is
 * produced by its last callee (eventFlagNpcUpdate_80A9C26), so it stays in the guest registers
 * untouched; r4/r5/r7-r12 are never written.
 */

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static u32 event_flag_set_sub_80A9B9C(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();          /* TestEventFlag: Z set when the flag is clear */
}

/* register form: calls SetEventFlag outside this file, which has no typed C interface yet */
static void set_event_flag_sub_80A9B9C(u16 flag) {
    R0 = flag;
    GUEST_CALL(SetEventFlag);
}

/* register form: calls ClearEventFlag outside this file, which has no typed C interface yet */
static void clear_event_flag_sub_80A9B9C(u16 flag) {
    R0 = flag;
    GUEST_CALL(ClearEventFlag);
}

/* register form: calls sprite_load outside this file, which has no typed C interface yet */
static void load_object_sprite(u32 kind, u32 resourceId, u32 spriteIndex) {
    R0 = kind;
    R1 = resourceId;
    R2 = spriteIndex;
    GUEST_CALL(sprite_load);
}

/* register form: calls checkZCoordModifiers_8031612 outside this file, which has no typed C
 * interface yet; the original keeps the object's current z in r6 across this call, so it is
 * synced here to match the register state the callee was written against */
static u32 z_coord_modifiers(u32 physics, u32 savedZ) {
    R0 = physics;
    R6 = savedZ;
    GUEST_CALL(checkZCoordModifiers_8031612);
    return R0;
}

/* register form: calls applyLayerEffectToOWObject_8035694 outside this file, which has no
 * typed C interface yet; its r0 return is the derived sprite priority */
static u32 apply_layer_effect(u32 physics) {
    R0 = physics;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return R0;
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C
 * interface yet */
static void set_sprite_priority(u32 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void set_sprite_animation(u32 animation) {
    R0 = animation;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void load_sprite_animation_data(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_shadow_off(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setUnk0x2c outside this file, which has no typed C interface yet */
static void set_sprite_unk0x2c(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setUnk0x2c);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void set_sprite_palette(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setFlip outside this file, which has no typed C interface yet */
static void set_sprite_flip(u32 flip) {
    R0 = flip;
    GUEST_CALL(sprite_setFlip);
}

/* register form: calls eventFlagNpcUpdate_80A9C26 outside this file, which has no typed C interface yet.
 * It continues initialising the object still held in r5 (with r6/r7 alongside) and is the
 * last call of the original, so its r0-r2 leftovers and flags are this function's outputs. */
static void finish_object_init(void) {
    GUEST_CALL(eventFlagNpcUpdate_80A9C26);
}

/* Initialise a just-spawned overworld object from its config entry. */
static void spawn_ow_object(u32 obj, u32 cfg) {
    u32 priority;

    ST8(obj, 3);                      /* state 3: being initialised */
    ST32(obj + 0x24, 0);              /* clear the object's timer/counter */

    if (event_flag_set_sub_80A9B9C(LD16(cfg))) {
        /* Spawn already ran before: (re)set this object's event flag and
         * clear the companion flag at +4. */
        set_event_flag_sub_80A9B9C(LD16(cfg + 2));
        clear_event_flag_sub_80A9B9C(LD16(cfg + 4));
    } else {
        /* First spawn: put the object away in state 1 and clear its event flag. */
        ST8(obj, 1);
        clear_event_flag_sub_80A9B9C(LD16(cfg + 2));
    }

    load_object_sprite(0x80, LD8(cfg + 6), LD8(cfg + 7));

    priority = LD8(cfg + 0xC);
    if (priority == 0xFF) {
        /* Priority 0xFF: derive it from the object's position. The computed z
         * is parked in the object's z field while the layer effect runs, then
         * the original value is put back. */
        u32 savedZ = LD32(obj + 0x14);
        ST32(obj + 0x14, z_coord_modifiers(obj + 0xC, savedZ) << 16);
        priority = apply_layer_effect(obj + 0xC);
        ST32(obj + 0x14, savedZ);
    }
    set_sprite_priority(priority);

    set_sprite_animation(LD8(cfg + 8));
    load_sprite_animation_data();
    sprite_shadow_off();
    set_sprite_unk0x2c(LD8(cfg + 0xA) << 24);
    set_sprite_palette(LD8(cfg + 0xB));
    set_sprite_flip(LD8(cfg + 0xD));

    ST8(obj + 8, 4);                  /* state 4: active */
    finish_object_init();
}

Next ow_spawnObjectFromConfig_80A9B9C(void) {
    spawn_ow_object(R5, R7);
    RETURN_TO_CALLER();
}

/* ── eventFlagNpcUpdate_80A9C26 ── */
/* ------------------------------------------------------------------ */
/* Register-form helpers: thin wrappers for game functions that have  */
/* no typed C interface yet.                                          */
/* ------------------------------------------------------------------ */

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static u32 test_event_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();          /* TestEventFlag: Z set if the flag is clear */
}

/* register form: calls SetEventFlag outside this file, which has no typed C interface yet */
static void set_event_flag_sub_80A9C26(u32 flags) {
    R0 = flags;
    GUEST_CALL(SetEventFlag);
}

/* register form: calls ClearEventFlag outside this file, which has no typed C interface yet */
static void clear_event_flag_sub_80A9C26(u32 flags) {
    R0 = flags;
    GUEST_CALL(ClearEventFlag);
}

/* register form: calls npc_isPlayerNearPos outside this file, which has no typed C interface yet */
static u32 event_npc_check(void) {
    /* r0 is deliberately NOT set here: the original passes along whatever
     * r0 the preceding TestEventFlag call left behind. */
    GUEST_CALL(npc_isPlayerNearPos);
    return R0;
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_set_animation_sub_80A9C26(u32 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_load_animation_data_sub_80A9C26(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_no_shadow_sub_80A9C26(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_effect(u32 id) {
    R0 = id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static void run_sprite_update(void) {
    GUEST_CALL(sprite_update);
}

/* ------------------------------------------------------------------ */
/* Event-flag driven object update.                                   */
/*                                                                    */
/* `script` (r7 in the original) points at a three-word flag record:  */
/*   +0: trigger flag that selects between the two states,            */
/*   +2: flag kept in sync with the object's idle/active state,       */
/*   +4: flag cleared once the object switches to its active state.   */
/*                                                                    */
/* `obj` (r5 in the original) is the object: +0 is its state byte,    */
/* +0x24 a word tracking whether the active animation is showing.     */
/* ------------------------------------------------------------------ */
static void update_event_flag_object(u32 obj, u32 script) {
    u32 trigger_flag = LD16(script);

    if (test_event_flag(trigger_flag)) {
        /* Trigger flag on: object enters state 3, raise the sync flag,
         * retire the one-shot flag. */
        ST8(obj, 3);
        set_event_flag_sub_80A9C26(LD16(script + 2));
        clear_event_flag_sub_80A9C26(LD16(script + 4));
    } else {
        /* Trigger flag off: object rests in state 1, sync flag cleared. */
        ST8(obj, 1);
        clear_event_flag_sub_80A9C26(LD16(script + 2));
    }

    if (test_event_flag(LD16(script))) {
        if (event_npc_check() != 0) {
            /* Condition met: play the "appear" animation (with sound)
             * once, when not already showing it. */
            if (LD32(obj + 0x24) == 0) {
                ST32(obj + 0x24, 1);
                sprite_set_animation_sub_80A9C26(3);
                sprite_load_animation_data_sub_80A9C26();
                sprite_no_shadow_sub_80A9C26();
                play_sound_effect(0x1D8);   /* 0xD9 + 0xFF */
            }
        } else {
            /* Condition gone: play the "disappear" animation once, when
             * the active one is still up. */
            if (LD32(obj + 0x24) != 0) {
                sprite_set_animation_sub_80A9C26(5);
                sprite_load_animation_data_sub_80A9C26();
                sprite_no_shadow_sub_80A9C26();
                ST32(obj + 0x24, 0);
            }
        }
    }

    run_sprite_update();
}

/* eventFlagNpcUpdate_80A9C26(r5 = object, r7 = flag script record)
 *
 * All register outputs (r0-r3 and nz/c/v) are whatever the final
 * sprite_update call leaves behind, and r4/r6/r10/lr are untouched,
 * so the adapter only forwards r5/r7 and returns. */
Next eventFlagNpcUpdate_80A9C26(void) {
    update_event_flag_object(R5, R7);
    RETURN_TO_CALLER();
}

/* ── npc_isPlayerNearPos ── */
/* Returns 1 if the NPC at npc_ptr belongs to the player and its grid position
   is within the threshold of the player's position. Returns 0 otherwise. */
static s32 npc_is_player_near(u32 npc_ptr)
{
    /* Ownership check: player[0x24] must equal npc[0x14] + table entry at +8 */
    s32 player_owner = (s32)LD32(A_eOWPlayerObject + 0x24);
    s32 npc_owner_adj = (s32)(LD32(npc_ptr + 0x14) + (s32)LD32(A_dword_80A9D00 + 8));
    if (player_owner != npc_owner_adj)
        return 0;

    /* Player subpixel position */
    s32 player_x = (s32)LD32(A_eOWPlayerObject + 0x1C);
    s32 player_y = (s32)LD32(A_eOWPlayerObject + 0x20);

    /* NPC coords adjusted by the subpixel grid offsets stored in the data table */
    s32 npc_x = (s32)LD32(npc_ptr + 0xC) + (s32)LD32(A_dword_80A9D00);
    s32 npc_y = (s32)LD32(npc_ptr + 0x10) + (s32)LD32(A_dword_80A9D00 + 4);

    /* Absolute difference in each axis */
    s32 dx = player_x - npc_x;
    if (dx < 0) dx = -dx;

    s32 dy = player_y - npc_y;
    if (dy < 0) dy = -dy;

    /* Both must be within the threshold (table entry at +0xC = 0x100000) */
    s32 limit = (s32)LD32(A_dword_80A9D00 + 0xC);
    if (dx > limit || dy > limit)
        return 0;

    return 1;
}

/* npc_isPlayerNearPos(r5 = NPC pointer) -> r0 = 1 if near, 0 otherwise */
Next npc_isPlayerNearPos(void) {
    R0 = (u32)npc_is_player_near(R5);
    RETURN_TO_CALLER();
}

/* ── ow_dispatchSpriteObjectState_80A9D10 ── */
/* Overworld sprite-object dispatcher. r5 points at an overworld object whose
 * state byte (offset 8) selects one of three routines from the jump table:
 * 0 = ow_initSpriteObject, 1 = ow_spriteObjectUpdate, 2 = sub_80A9DB8.
 * r2 and r10 are passed through to the selected routine, which leaves its
 * results in r2/r3/r8-r12; r4-r7 survive the call (the original pushes and
 * pops them around the dispatch). */

/* Call the routine selected by the object's state byte.
 * register form: the target is reached through the game's jump table with
 * mov lr, pc / bx, and r4-r7 must be restored around the callee exactly as
 * the original's push {r4-r7} / pop {r4-r7} does; C cannot express that
 * around an indirect guest call without touching the registers directly. */
static u32 call_object_routine(u32 kind, u32 arg2, u32 arg5, u32 arg10) {
    u32 target = LD32(A_off_80A9D24 + kind);
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R0 = target;               /* the original jumps with the table entry in r0 */
    R1 = kind;
    R2 = arg2;
    R5 = arg5;
    R10 = arg10;
    GUEST_CALL_ADDR(target);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    return R0;
}

/* Run the routine for one overworld sprite object (arg5 = object pointer). */
static void run_object_routine(u32 arg2, u32 arg5, u32 arg10) {
    u32 kind = LD8(arg5 + 8);
    call_object_routine(kind, arg2, arg5, arg10);
}

/* ow_dispatchSpriteObjectState_80A9D10(r2, r5 = object, r10) -> r2 r3 r4 r5 r8 r9 r10 r11 r12
 * The selected routine's register results are already in the guest registers
 * after the dispatch call, and r4/r5 come back unchanged, so the adapter has
 * nothing to write. */
Next ow_dispatchSpriteObjectState_80A9D10(void) {
    run_object_routine(R2, R5, R10);
    RETURN_TO_CALLER();
}

/* ── ow_initSpriteObject ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r5; } call_ow_applyCutsceneDirOffset_result_ow_initSpriteObject;
/* register form: generated wrapper: ow_applyCutsceneDirOffset has no typed C interface yet */
static inline call_ow_applyCutsceneDirOffset_result_ow_initSpriteObject call_ow_applyCutsceneDirOffset_ow_initSpriteObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(ow_applyCutsceneDirOffset);
    return (call_ow_applyCutsceneDirOffset_result_ow_initSpriteObject){ R1, R2, R3, R5 };
}

typedef struct { u32 r4, r5; } call_sprite_load_result_ow_initSpriteObject;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_ow_initSpriteObject call_sprite_load_ow_initSpriteObject(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_ow_initSpriteObject){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_ow_initSpriteObject(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_ow_initSpriteObject;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_ow_initSpriteObject call_sprite_loadAnimationData_ow_initSpriteObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_ow_initSpriteObject){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_ow_initSpriteObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result_ow_initSpriteObject;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result_ow_initSpriteObject call_applyLayerEffectToOWObject_8035694_ow_initSpriteObject(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result_ow_initSpriteObject){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r2, r3; } call_sprite_setPriority_8002E14_result_ow_initSpriteObject;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_ow_initSpriteObject call_sprite_setPriority_8002E14_ow_initSpriteObject(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_ow_initSpriteObject){ R2, R3 };
}

typedef struct { u32 r2, r3, r8, r9, r10, r11, r12; } call_ow_spriteObjectUpdate_result;
/* register form: generated wrapper: ow_spriteObjectUpdate has no typed C interface yet */
static inline call_ow_spriteObjectUpdate_result call_ow_spriteObjectUpdate(u32 r2, u32 r5) {
    R2 = r2;
    R5 = r5;
    GUEST_CALL(ow_spriteObjectUpdate);
    return (call_ow_spriteObjectUpdate_result){ R2, R3, R8, R9, R10, R11, R12 };
}

/* Initialize an overworld sprite object: mark active, set position, load
 * graphics/animation, apply layer effects, set behavior parameters,
 * then run the first update tick. */
static void ow_init_sprite(u32 r5) {
    ST8(r5, 1);  /* mark sprite as active */

    /* Retrieve cutscene-adjusted position and facing */
    call_ow_applyCutsceneDirOffset_result_ow_initSpriteObject pos = call_ow_applyCutsceneDirOffset_ow_initSpriteObject(r5);
    r5 = pos.r5;

    /* Store position: current at +0xC, previous at +0x30 */
    ST32(r5 + 0xC, pos.r1);
    ST32(r5 + 0x10, pos.r2);
    ST32(r5 + 0x14, pos.r3);
    ST32(r5 + 0x30, pos.r1);
    ST32(r5 + 0x34, pos.r2);
    ST32(r5 + 0x38, pos.r3);

    /* Load sprite tileset: tile 0x80, dimensions 0x1C x 0x9E */
    call_sprite_load_result_ow_initSpriteObject loaded = call_sprite_load_ow_initSpriteObject(0x80, 0x1C, 0x9E, r5);
    r5 = loaded.r5;

    /* Set initial animation and load its frame data */
    call_sprite_setAnimation_ow_initSpriteObject(0, r5);
    call_sprite_loadAnimationData_result_ow_initSpriteObject anim = call_sprite_loadAnimationData_ow_initSpriteObject(r5);
    r5 = anim.r5;

    /* Disable shadow rendering for this object */
    call_sprite_noShadow_ow_initSpriteObject(r5);

    /* Apply the overworld map layer effect to the position fields */
    call_applyLayerEffectToOWObject_8035694_result_ow_initSpriteObject layer =
        call_applyLayerEffectToOWObject_8035694_ow_initSpriteObject(r5 + 0xC);
    r5 = layer.r5;

    /* Set the object's draw priority based on map layer */
    call_sprite_setPriority_8002E14_result_ow_initSpriteObject prio =
        call_sprite_setPriority_8002E14_ow_initSpriteObject(layer.r0, r5);

    /* Initialize frame animation counters */
    ST16(r5 + 0x24, 0);
    ST16(r5 + 0x26, 0);
    ST32(r5 + 0x28, 0x2AAAA);
    ST32(r5 + 0x2C, 0x16C1);

    /* Set movement mode (4) and clear sub-flag */
    ST8(r5 + 8, 4);
    ST8(r5 + 9, 0);

    /* Perform the first per-frame update */
    call_ow_spriteObjectUpdate(prio.r2, r5);
}

Next ow_initSpriteObject(void) {
    ow_init_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── ow_spriteObjectUpdate ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_sprite_update_result;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result call_sprite_update(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* ow_spriteObjectUpdate — per-frame update for one overworld sprite object.
 *
 * r5 points at the object; the byte at object+9 selects which state handler
 * to run, looked up in the jump table at off_80A9DA4 (object_timerStateTick,
 * beastsp_updateMotion, object_tick8SetState, e_decelRiseUpdate, cutscene_setStateActive_80A9EB6). After the state
 * handler returns, sprite_update advances the object's animation frame.
 *
 * The state handlers run on the live guest registers: whatever they leave in
 * r8-r12 is part of this routine's contract. r8 itself is saved and restored
 * around the dispatch (the original's push {r4}/pop {r4}/mov r8,r4 pair).
 */

/* register form: the handler is reached through a jump table and its r8-r12
 * side effects must pass straight through to this routine's callers, so the
 * dispatch has to run on the live guest register file rather than through a
 * typed wrapper. */
static u32 run_object_state_handler(u32 handler, u32 state, u32 r2, u32 r5) {
    u32 saved_r8 = R8;      /* the original preserves r8 across the dispatch */
    R0 = handler;           /* handlers are entered with r0 = handler address */
    R1 = state;             /* r1 = the state byte that selected the handler */
    R2 = r2;
    R5 = r5;
    GUEST_CALL_ADDR(handler);
    R8 = saved_r8;
    return R0;
}

static call_sprite_update_result sprite_object_update(u32 r2, u32 r5) {
    u32 state = LD8(r5 + 9);
    u32 handler = LD32(A_off_80A9DA4 + state);   /* jump table indexed by the state byte */

    run_object_state_handler(handler, state, r2, r5);
    return call_sprite_update(r5);
}

Next ow_spriteObjectUpdate(void) {
    call_sprite_update_result res = sprite_object_update(R2, R5);
    R2 = res.r2;
    R3 = res.r3;
    RETURN_TO_CALLER();
}

/* ── object_timerStateTick ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_object_timerStateTick(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_object_timerStateTick;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_object_timerStateTick call_sprite_loadAnimationData_object_timerStateTick(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_object_timerStateTick){ R2, R4, R5 };
}

typedef struct { u32 r1, r3; bool c, v; } call_sprite_noShadow_result_object_timerStateTick;
/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline call_sprite_noShadow_result_object_timerStateTick call_sprite_noShadow_object_timerStateTick(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
    return (call_sprite_noShadow_result_object_timerStateTick){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3, r5, r10; bool c, v; } call_PlaySoundEffect_result_object_timerStateTick;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_object_timerStateTick call_PlaySoundEffect_object_timerStateTick(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_object_timerStateTick){ R1, R2, R3, R5, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Tick for an object sitting in a timed waiting state.
 *
 * The object counts frames in the halfword at +0x24. On count 5 it briefly
 * drops into sub-state 3; once the count reaches 30 the wait is over: the
 * counter restarts, the sprite switches to animation 1, its shadow is removed,
 * sound 0x123 plays (unless the flag byte at +4 suppresses it) and the object
 * enters state 4.
 *
 * r5 (the object pointer) is preserved: it is both an input and an output.
 * r10 passes through: it is returned unchanged unless PlaySoundEffect runs,
 * in which case the value that call leaves behind is the function's r10.
 * r2 at the PlaySoundEffect call is not set by this routine itself; it is
 * whatever sprite_loadAnimationData left behind, so it is taken from that
 * call's result. */
static u32 timer_state_tick(u32 obj, u32 r10) {
    u32 timer = (u32)LD16(obj + 0x24) + 1;
    ST16(obj + 0x24, timer);

    if (timer == 5) {
        /* short mid-phase beat: park in sub-state 3 and keep waiting */
        ST8(obj, 3);
        return r10;
    }
    if ((s32)timer < 30)
        return r10;                    /* not done waiting yet */

    /* wait finished: restart the counter and reset the sprite */
    ST16(obj + 0x24, 0);
    call_sprite_setAnimation_object_timerStateTick(1, obj);
    call_sprite_loadAnimationData_result_object_timerStateTick anim = call_sprite_loadAnimationData_object_timerStateTick(obj);
    call_sprite_noShadow_object_timerStateTick(obj);

    if (LD8(obj + 4) == 0) {           /* flag byte suppresses the sound */
        call_PlaySoundEffect_result_object_timerStateTick snd = call_PlaySoundEffect_object_timerStateTick(0x123, anim.r2);
        r10 = snd.r10;
    }

    ST8(obj + 9, 4);                   /* enter state 4 */
    return r10;
}

/* object_timerStateTick: 080A9DC0-080A9DFE THUMB, bn6f asm29.s:9999
 *   inputs:  r5 lr   outputs: r5 r10   clobbers: nz c v r0-r3 lr
 *   preserved: r4 r5 r6 r7 r10 */
Next object_timerStateTick(void) {
    u32 obj = R5;
    u32 r10_out = timer_state_tick(obj, R10);
    R5 = obj;
    R10 = r10_out;
    RETURN_TO_CALLER();
}

/* ── beastsp_updateMotion ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_beastsp_updateMotion;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_beastsp_updateMotion call_PlaySoundEffect_beastsp_updateMotion(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_beastsp_updateMotion){ R2, R4, R5, R6, R7, R10 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_beastsp_updateMotion(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_beastsp_updateMotion;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_beastsp_updateMotion call_sprite_loadAnimationData_beastsp_updateMotion(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_beastsp_updateMotion){ R2, R4, R5 };
}

typedef struct { u32 r0, r1, r3; bool z, n, c, v; } call_sprite_noShadow_result_beastsp_updateMotion;
/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline call_sprite_noShadow_result_beastsp_updateMotion call_sprite_noShadow_beastsp_updateMotion(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
    return (call_sprite_noShadow_result_beastsp_updateMotion){ R0, R1, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Beast sprite motion update: advances the animation timer, switches animation at tick 12,
 * resets the timer at tick 30. Returns updated sprite pointer and r10. */

#define BEASTSP_ANIM_SWITCH_TICK   12
#define BEASTSP_TIMER_RESET_TICK   30
#define BEASTSP_NEW_ANIM_INDEX     2
#define BEASTSP_NEW_STATE_BYTE     8
#define BEASTSP_SOUND_EFFECT_ID    0x135  /* 0x36 + 0xff */

typedef struct {
    u32 sprite;
    u32 r10;
} beastsp_updateMotion_result;

static beastsp_updateMotion_result beastsp_updateMotion_body(u32 r2, u32 sprite, u32 r10) {
    /* Accumulate time delta into position, subtract duration from time offset */
    ST32(sprite + 0x14, LD32(sprite + 0x14) + LD32(sprite + 0x28));
    ST32(sprite + 0x28, LD32(sprite + 0x28) - LD32(sprite + 0x2C));

    /* Advance the 16-bit tick counter */
    u32 tick = LD16(sprite + 0x24) + 1;
    ST16(sprite + 0x24, tick);

    if (tick == BEASTSP_ANIM_SWITCH_TICK) {
        /* Transition animation: optionally play sound, then reload sprite animation */
        if (LD8(sprite + 4) == 0) {
            call_PlaySoundEffect_result_beastsp_updateMotion snd = call_PlaySoundEffect_beastsp_updateMotion(BEASTSP_SOUND_EFFECT_ID, r2);
            sprite = snd.r5;
            r10 = snd.r10;
        }
        call_sprite_setAnimation_beastsp_updateMotion(BEASTSP_NEW_ANIM_INDEX, sprite);
        call_sprite_loadAnimationData_result_beastsp_updateMotion anim = call_sprite_loadAnimationData_beastsp_updateMotion(sprite);
        sprite = anim.r5;
        call_sprite_noShadow_beastsp_updateMotion(sprite);
    } else if (tick >= BEASTSP_TIMER_RESET_TICK) {
        /* Reset timer and set transition state */
        ST16(sprite + 0x24, 0);
        ST8(sprite + 9, BEASTSP_NEW_STATE_BYTE);
    }

    beastsp_updateMotion_result res = { sprite, r10 };
    return res;
}

/* beastsp_updateMotion(r5 = sprite, r2 = sound param) -> r5, r10 */
Next beastsp_updateMotion(void) {
    beastsp_updateMotion_result res = beastsp_updateMotion_body(R2, R5, R10);
    R5 = res.sprite;
    R10 = res.r10;
    RETURN_TO_CALLER();
}

/* ── e_decelRiseUpdate ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_sprite_setFlip_result_e_decelRiseUpdate;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_e_decelRiseUpdate call_sprite_setFlip_e_decelRiseUpdate(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_e_decelRiseUpdate){ R2 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_e_decelRiseUpdate(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_e_decelRiseUpdate;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_e_decelRiseUpdate call_sprite_loadAnimationData_e_decelRiseUpdate(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_e_decelRiseUpdate){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_e_decelRiseUpdate(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r5; } call_sub_809C940_result;
/* register form: generated wrapper: cutscene_clearEntryEventFlags_809C940 has no typed C interface yet */
static inline call_sub_809C940_result call_sub_809C940(void) {
    GUEST_CALL(cutscene_clearEntryEventFlags_809C940);
    return (call_sub_809C940_result){ R5 };
}

typedef struct { u32 r1, r2, r3, r5, r10; bool c, v; } call_PlaySoundEffect_result_e_decelRiseUpdate;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_e_decelRiseUpdate call_PlaySoundEffect_e_decelRiseUpdate(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_e_decelRiseUpdate){ R1, R2, R3, R5, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Offsets into the decelerating-rise object (r5) */
#define oRise_PosX    0x0c
#define oRise_PosY    0x10
#define oRise_PosZ    0x14
#define oRise_SndFlag 0x04   /* 0 = play landing sound */
#define oRise_FlipDir 0x05   /* sprite flip byte */
#define oRise_State   0x09   /* behaviour state */
#define oRise_Timer   0x24   /* u16 frame counter */
#define oRise_VelZ    0x28   /* current vertical velocity */
#define oRise_Decel   0x2c   /* per-frame velocity reduction */
#define oRise_BackupX 0x30
#define oRise_BackupY 0x34
#define oRise_BackupZ 0x38

#define RISE_TIMER_MAX  0x1e
#define RISE_LAND_ANIM  5
#define RISE_STATE_LAND 0x10
#define RISE_SND_ID     (0xAE + 0xFF)

/* Advance position and decelerate every frame. When the 30-frame timer expires,
 * switch to the landing animation (flip + anim 5 + reload), remove the shadow,
 * recompute the sprite base via cutscene_clearEntryEventFlags_809C940, teleport to backup coordinates,
 * optionally play the landing sound, and mark state as landed. */
static void decel_rise_update(u32 obj) {
    /* pos_z += vel_z */
    ST32(obj + oRise_PosZ, LD32(obj + oRise_PosZ) + LD32(obj + oRise_VelZ));
    /* vel_z -= decel */
    ST32(obj + oRise_VelZ, LD32(obj + oRise_VelZ) - LD32(obj + oRise_Decel));

    /* Increment timer (u16 storage, u32 comparison) */
    u32 timer = (u32)(LD16(obj + oRise_Timer) + 1);
    ST16(obj + oRise_Timer, (u16)timer);
    if ((s32)timer <= RISE_TIMER_MAX)
        return;

    /* Timer expired – begin landing sequence */
    ST16(obj + oRise_Timer, 0);

    u32 flip = LD8(obj + oRise_FlipDir);
    call_sprite_setFlip_e_decelRiseUpdate(flip, obj);
    call_sprite_setAnimation_e_decelRiseUpdate(RISE_LAND_ANIM, obj);
    obj = call_sprite_loadAnimationData_e_decelRiseUpdate(obj).r5;

    call_sprite_noShadow_e_decelRiseUpdate(obj);
    obj = call_sub_809C940().r5;

    /* Teleport to backup coordinates */
    ST32(obj + oRise_PosX, LD32(obj + oRise_BackupX));
    ST32(obj + oRise_PosY, LD32(obj + oRise_BackupY));
    ST32(obj + oRise_PosZ, LD32(obj + oRise_BackupZ));

    /* Play landing sound when trigger flag is clear */
    if (LD8(obj + oRise_SndFlag) == 0) {
        obj = call_PlaySoundEffect_e_decelRiseUpdate(RISE_SND_ID, LD32(obj + oRise_BackupY)).r5;
    }

    /* Mark state as landed */
    ST8(obj + oRise_State, RISE_STATE_LAND);
}

/* e_decelRiseUpdate(r5 = object pointer, r10 = state) -> r5, r10 */
Next e_decelRiseUpdate(void) {
    decel_rise_update(R5);
    RETURN_TO_CALLER();
}

/* ── cutscene_setStateActive_80A9EB6 ── */
/* Marks the cutscene state as active: writes 1 to the byte at
 * eCutsceneState+8 (byte_2011C58). Touches nothing else. */
static void mark_cutscene_state_active(void) {
    ST8(A_eCutsceneState + 8, 1);
}

/* cutscene_setStateActive_80A9EB6: sets eCutsceneState+8 to 1 (cutscene running).
 * r4-r7 and r5 (listed as an output) are preserved untouched. */
Next cutscene_setStateActive_80A9EB6(void) {
    mark_cutscene_state_active();
    RETURN_TO_CALLER();
}

/* ── ow_logoutDispatchState ── */
/* Dispatch a logout sub-state by the byte at [object+8] into a 3-entry
 * jump table at off_80A9EE0:
 *   0 → ow_startLogoutEffect
 *   4 → sprite_frameTickMarkState8_80A9F30
 *   8 → ow_releaseMapObject
 * The original guards r4–r7 with push/pop around the indirect call. */

#define LOGOUT_STATE_OFFSET  8   /* byte offset of state index inside the object (r5) */
#define LOGOUT_TABLE         A_off_80A9EE0

static void logout_dispatch(u32 object_ptr) {
    u32 state_index = LD8(object_ptr + LOGOUT_STATE_OFFSET);
    u32 target_addr = LD32(LOGOUT_TABLE + state_index);

    /* register form: preserve r4-r7 across the indirect jump-table call,
       matching the original's push/pop {r4-r7,lr} guard */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    GUEST_CALL_ADDR(target_addr);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
}

Next ow_logoutDispatchState(void) {
    logout_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── ow_startLogoutEffect ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r5; } call_ow_applyCutsceneDirOffset_result_ow_startLogoutEffect;
/* register form: generated wrapper: ow_applyCutsceneDirOffset has no typed C interface yet */
static inline call_ow_applyCutsceneDirOffset_result_ow_startLogoutEffect call_ow_applyCutsceneDirOffset_ow_startLogoutEffect(u32 r5) {
    R5 = r5;
    GUEST_CALL(ow_applyCutsceneDirOffset);
    return (call_ow_applyCutsceneDirOffset_result_ow_startLogoutEffect){ R1, R2, R3, R5 };
}

typedef struct { u32 r4, r5; } call_sprite_load_result_ow_startLogoutEffect;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_ow_startLogoutEffect call_sprite_load_ow_startLogoutEffect(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_ow_startLogoutEffect){ R4, R5 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_ow_startLogoutEffect(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_ow_startLogoutEffect;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_ow_startLogoutEffect call_sprite_loadAnimationData_ow_startLogoutEffect(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_ow_startLogoutEffect){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_ow_startLogoutEffect(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r0, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_applyLayerEffectToOWObject_8035694_result_ow_startLogoutEffect;
/* register form: generated wrapper: applyLayerEffectToOWObject_8035694 has no typed C interface yet */
static inline call_applyLayerEffectToOWObject_8035694_result_ow_startLogoutEffect call_applyLayerEffectToOWObject_8035694_ow_startLogoutEffect(u32 r0) {
    R0 = r0;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    return (call_applyLayerEffectToOWObject_8035694_result_ow_startLogoutEffect){ R0, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r1, r2, r3; } call_sprite_setPriority_8002E14_result_ow_startLogoutEffect;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result_ow_startLogoutEffect call_sprite_setPriority_8002E14_ow_startLogoutEffect(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result_ow_startLogoutEffect){ R1, R2, R3 };
}

typedef struct { u32 r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_ow_startLogoutEffect;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_ow_startLogoutEffect call_PlaySoundEffect_ow_startLogoutEffect(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_ow_startLogoutEffect){ R4, R5, R6, R7, R10 };
}

typedef struct { u32 r2, r3, r8; } call_sub_80A9F30_result;
/* register form: generated wrapper: sprite_frameTickMarkState8_80A9F30 has no typed C interface yet */
static inline call_sub_80A9F30_result call_sub_80A9F30(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_frameTickMarkState8_80A9F30);
    return (call_sub_80A9F30_result){ R2, R3, R8 };
}

/* Start the internet "log out" overworld effect: positions the logout
 * sprite, loads its visuals, plays the sound, advances the state machine. */
static void ow_startLogoutEffect_impl(u32 r5, u32 r10) {
    /* state = LOGOUT_START (3) */
    ST8(r5, 3);

    /* Compute the screen-anchored position via the cutscene direction offset */
    call_ow_applyCutsceneDirOffset_result_ow_startLogoutEffect dir = call_ow_applyCutsceneDirOffset_ow_startLogoutEffect(r5);
    ST32(r5 + 0xC, dir.r1);
    ST32(r5 + 0x10, dir.r2);
    ST32(r5 + 0x14, dir.r3);

    /* Instantiate the logout sprite (0x80 / 0x1C on layer 0) */
    call_sprite_load_ow_startLogoutEffect(0x80, 0x1C, 0, r5);
    call_sprite_setAnimation_ow_startLogoutEffect(0, r5);
    call_sprite_loadAnimationData_ow_startLogoutEffect(r5);
    call_sprite_noShadow_ow_startLogoutEffect(r5);

    /* Map the sprite onto the overworld depth layer */
    call_applyLayerEffectToOWObject_8035694_result_ow_startLogoutEffect layer =
        call_applyLayerEffectToOWObject_8035694_ow_startLogoutEffect(r5 + 0xC);

    /* Set rendering priority based on the mapped position */
    call_sprite_setPriority_8002E14_result_ow_startLogoutEffect prio =
        call_sprite_setPriority_8002E14_ow_startLogoutEffect(layer.r0, r5);

    /* Play the "log out" sound (SFX 0x76) */
    call_PlaySoundEffect_ow_startLogoutEffect(SOUND_LOG_OUT_76, prio.r2);

    /* state = LOGOUT_DONE (4) */
    ST8(r5 + 8, 4);

    /* Advance the state machine */
    call_sub_80A9F30(r5);
}

/* ow_startLogoutEffect(r5 = OW object, r10 = battle-state ptr) */
Next ow_startLogoutEffect(void) {
    ow_startLogoutEffect_impl(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sprite_frameTickMarkState8_80A9F30 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_sprite_getFrameParameters_result_sub_80A9F30;
/* register form: generated wrapper: sprite_getFrameParameters has no typed C interface yet */
static inline call_sprite_getFrameParameters_result_sub_80A9F30 call_sprite_getFrameParameters_sub_80A9F30(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_getFrameParameters);
    return (call_sprite_getFrameParameters_result_sub_80A9F30){ R0 };
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_sprite_update_result_sub_80A9F30;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_sub_80A9F30 call_sprite_update_sub_80A9F30(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_sub_80A9F30){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Per-frame sprite work for the object whose pointer is in r5:
 *  - sprite_getFrameParameters reports the current animation frame's parameters;
 *    when bit 0x80 is set, the game writes 8 into the object's flag byte at +8.
 *  - sprite_update then advances the sprite animator.
 * r2/r3 (and the condition flags) are simply whatever sprite_update leaves
 * behind; the wrapper call already places them in the register file.
 * r8's exit value is the value it had on entry -- the original saves it around
 * the calls and restores it afterwards -- so it is passed through unchanged. */
static u32 sprite_frame_tick(u32 obj, u32 r8_entry) {
    u32 frame_params = call_sprite_getFrameParameters_sub_80A9F30(obj).r0;

    if ((frame_params & 0x80) != 0)
        ST8(obj + 8, 8);              /* mark the object for this frame */

    call_sprite_update_sub_80A9F30(obj);
    return r8_entry;                  /* r8 restored to its entry value */
}

/* sprite_frameTickMarkState8_80A9F30(r5 = object pointer) -> r2 r3 from sprite_update, r8 unchanged */
Next sprite_frameTickMarkState8_80A9F30(void) {
    u32 r8_out = sprite_frame_tick(R5, R8);
    R8 = r8_out;
    RETURN_TO_CALLER();
}

/* ── ow_releaseMapObject ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_FreeOverworldMapObject_result_ow_releaseMapObject;
/* register form: generated wrapper: FreeOverworldMapObject has no typed C interface yet */
static inline call_FreeOverworldMapObject_result_ow_releaseMapObject call_FreeOverworldMapObject_ow_releaseMapObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(FreeOverworldMapObject);
    return (call_FreeOverworldMapObject_result_ow_releaseMapObject){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Release a map object slot on the overworld: hand the object in r5 to
 * FreeOverworldMapObject, which frees it and reports r0-r3 (release status and
 * related values). Callers of this thunk only look at r2 and r3, which are
 * whatever FreeOverworldMapObject left there; r4-r7 are untouched. */
static void release_map_object(u32 object, u32 *out_r2, u32 *out_r3) {
    call_FreeOverworldMapObject_result_ow_releaseMapObject res = call_FreeOverworldMapObject_ow_releaseMapObject(object);
    *out_r2 = res.r2;
    *out_r3 = res.r3;
}

/* ow_releaseMapObject(r5 = map object) -> r2 r3 from FreeOverworldMapObject */
Next ow_releaseMapObject(void) {
    u32 out_r2, out_r3;
    release_map_object(R5, &out_r2, &out_r3);
    R2 = out_r2;
    R3 = out_r3;
    RETURN_TO_CALLER();
}

/* ── copyOamEntry_80AA004 ── */
/* Copy 16 bytes from the indexed entry in the oam-related table at byte_80AA01C
 * into the chip/pet header buffer at byte_2001130. Index is the low nybble of r0;
 * each entry is 16 bytes. */
static void copy_oam_template(u32 index) {
    /* register form: calls CopyBytes outside this file, which has no typed C interface yet */
    R0 = A_byte_80AA01C + (index << 4);
    R1 = A_byte_2001130;
    R2 = 0x10;
    GUEST_CALL(CopyBytes);
}

Next copyOamEntry_80AA004(void) {
    copy_oam_template(R0);
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── palette_copyTableRow_80AA104 ── */
/* palette_copyTableRow_80AA104(r0 = row selector, 0..2):
 * Copies one 8-byte entry (a palette row: three colors plus terminator)
 * from the table byte_80AA11C into the buffer byte_20018B0, then
 * returns 0 with the flags set by "mov r0, #0" (N/Z clear, C/V unchanged).
 * r4-r7 are untouched by this routine and by CopyBytes. */
Next palette_copyTableRow_80AA104(void) {
    /* Each entry in byte_80AA11C is 8 bytes: R0 selects which one. */
    R0 = A_byte_80AA11C + (R0 << 3);
    R1 = A_byte_20018B0;
    R2 = 8;
    GUEST_CALL(CopyBytes); /* CopyBytes(src = R0, dest = R1, byteCount = R2) */

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_object_dispatchSpriteState3_80AA2D8 ── */
/* Byte offset in the battle-object struct that selects the sprite-state-3 handler. */
#define oSpriteState3_Sel 8

/* register form: calls object_initSpriteState3 outside this file, which has no typed C interface yet */
static void call_init_sprite_state3(void) {
    GUEST_CALL(object_initSpriteState3_80AA2F8);
}

/* register form: calls sub_80AA322 outside this file, which has no typed C interface yet */
static void call_sprite_state3_update(void) {
    GUEST_CALL(sub_80AA322);
}

/* register form: calls ow_deleteMapObject outside this file, which has no typed C interface yet */
static void call_sprite_state3_done(void) {
    GUEST_CALL(ow_deleteMapObject);
}

/* Jump-table dispatch for sprite-state-3: pick the handler from the selector byte
 * at [obj + oSpriteState3_Sel] (0 = init, 4 = update, 8 = done) and call it. */
static void dispatch_sprite_state3(u32 obj) {
    u32 sel = LD8(obj + oSpriteState3_Sel);
    if (sel == 0)
        call_init_sprite_state3();
    else if (sel == 4)
        call_sprite_state3_update();
    else if (sel == 8)
        call_sprite_state3_done();
}

/* battle_object_dispatchSpriteState3_80AA2D8(r5 = battle object) -> tail-dispatch to a sprite-state-3 handler */
Next battle_object_dispatchSpriteState3_80AA2D8(void) {
    dispatch_sprite_state3(R5);
    RETURN_TO_CALLER();
}

/* ── object_initSpriteState3_80AA2F8 ── */
/* object_initSpriteState3_80AA2F8: sprite setup for an object whose struct pointer is in r5.
 * Stores the object's initial state fields (+0 = 3, +4 = 0, +8 = 4), loads
 * its sprite (gfx 0x80, palette 0x1C, sprite 0x98), resets the animation,
 * and finishes with the follow-up init in sub_80AA322. */
Next object_initSpriteState3_80AA2F8(void) {
    ST8(R5, 3);                       /* object state = 3 */

    R0 = 0x80;                        /* sprite_load(gfx, palette, sprite) */
    R1 = 0x1C;
    R2 = 0x98;
    GUEST_CALL(sprite_load);

    ST8(R5 + 4, 0);                   /* animation counter = 0 */
    R0 = 0;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    ST8(R5 + 8, 4);                   /* next state = 4 */
    R0 = 4;
    GUEST_CALL(sub_80AA322);

    RETURN_TO_CALLER();
}

/* ── sub_80AA322 ── */
/* sub_80AA322(r5 = sprite object pointer, r10 = ?, lr = return):
 * Update an overworld NPC sprite object. Decides layer/animation based on
 * cutscene state, copies overworld coords into the sprite, applies the
 * layer effect, sets sprite priority, and runs the sprite update.
 *
 * Store the chosen mode in the sprite's first byte; mode 1 is the default,
 * mode 3 is used when both target cutscene script positions have been
 * reached. r10/r11/r2 outputs come from the final sprite_update call.
 */
Next sub_80AA322(void) {
    /* Default mode = 1; promoted to 3 once the cutscene reaches the two
     * scripted positions the engine is waiting on. */
    u32 mode = 1;

    /* sub_81279A8: sets flags based on some readiness/state check. */
    R0 = 0; /* arguments unused, set R0 defensively */
    GUEST_CALL(sub_81279A8);
    if (!ZERO_FLAG()) goto pick_mode_done;

    /* Are we at the originally-recorded cutscene position for byte_8088514? */
    R0 = (u32)A_byte_8088514;
    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F70);
    if (!ZERO_FLAG()) goto pick_mode_done;

    /* ...and the one for byte_8088708? */
    R0 = (u32)A_byte_8088708;
    GUEST_CALL(cutscene_checkOriginalCutsceneScriptPos_8036F70);
    if (!ZERO_FLAG()) goto pick_mode_done;

    mode = 3;

pick_mode_done:
    ST8(R5, (u8)mode);

    /* Copy the overworld NPC's transform (x, y, layer) into the sprite at
     * offsets 0x0C, 0x10, 0x14. The base of the OW NPC object array is
     * eOverworldNPCObjects; the fields used live at +0x24/+0x28/+0x2C. */
    u32 ow_base = (u32)A_eOverworldNPCObjects;
    u32 npc_x   = LD32(ow_base + 0x24);
    u32 npc_y   = LD32(ow_base + 0x28);
    u32 npc_lay = LD32(ow_base + 0x2C);
    ST32(R5 + 0x0C, npc_x);
    ST32(R5 + 0x10, npc_y);
    ST32(R5 + 0x14, npc_lay);

    /* Apply the layer effect to that copied transform block. */
    R0 = R5 + 0x0C;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);

    /* Re-evaluate sprite priority and run the per-frame sprite update; this
     * is what fills r2/r10/r11 for the caller. */
    GUEST_CALL(sprite_setPriority_8002E14);
    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── sub_80AA394 ── */
/* sub_80AA394(r5 = work area, r2/r10/r11 = pass-through):
 * Initialise the CopyBot-related work area: pick its slot based on the
 * CopyBot event flag, run its two helpers and fill in the default timers.
 * Callees clobber r4-r7, so they are saved/restored here (push/pop in the
 * original). */
Next sub_80AA394(void) {
    u32 saved_r4 = R4, saved_r6 = R6, saved_r7 = R7;
    u32 work = R5;

    ST8(work, 3);

    /* Byte at +5: 0 when the CopyBot event flag is set, 2 otherwise.
     * TestEventFlagFromImmediate takes the flag as the r0/r1 pair. */
    u32 slot = 0;
    R0 = 0x17;
    R1 = 0x29;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (ZERO_FLAG())
        slot = 2;
    ST8(work + 5, slot);

    R0 = 0;
    GUEST_CALL(sub_80AA3C2);

    ST32(work + 0x24, 0x5A);
    ST32(work + 0x28, 1);
    ST8(work + 8, 4);

    R0 = 4;
    GUEST_CALL(sub_80AA402);

    R4 = saved_r4;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── sub_80AA3C2 ── */
/* sub_80AA3C2(r0 = sprite table index, r5 = overworld object):
 * (Re)loads the sprite resources for an overworld object and restarts its
 * animation.  The sprite table at byte_80AA430 holds one 8-byte slot per
 * index; [obj+5] picks which sprite entry inside that slot to use.
 * Returns r1 = the fourth byte of the table slot (0xFF if there was no
 * sprite to load). */
Next sub_80AA3C2(void) {
    const u32 table = A_byte_80AA430 + (R0 << 3);
    const u32 obj = R5;

    /* First byte of the entry is the sprite id; 0xFF means "no sprite". */
    const u32 entry = table + LD8(obj + 5);
    const u32 spriteId = LD8(entry);

    if (spriteId == 0xFF) {
        R1 = spriteId;
        RETURN_TO_CALLER();
    }

    R0 = 0x80;                  /* sprite_load(0x80, id, entry[1]) */
    R1 = spriteId;
    R2 = LD8(entry + 1);
    GUEST_CALL(sprite_load);

    /* Pick the animation number from [obj+4]; animation 0x18 runs at full
     * speed, every other one at half speed. */
    u32 anim = LD8(obj + 4);
    if (LD8(entry) != 0x18)
        anim >>= 1;

    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_hasShadow);

    R0 = obj + 0xC;
    GUEST_CALL(applyLayerEffectToOWObject_8035694);
    GUEST_CALL(sprite_setPriority_8002E14);

    R1 = LD8(table + 4);
    RETURN_TO_CALLER();
}

/* ── dispatchGameHandlerByIndex_80AA6A4 ── */
/* Jump-table dispatcher. Picks one of 12 handlers indexed by R0 from
 * JumpTable80AA6B8, then tail-calls it. r2..r12/lr are forwarded to the
 * handler and the handler's result comes back in r0 (the original lists
 * r0 and r2..r12 as outputs; r1 is clobbered by the lookup+indirect call). */

/* register form: calls an indexed game handler outside this file. The
 * handler addresses live in the game's static jump table at A_JumpTable80AA6B8
 * and each entry is a Thumb target (already includes the +1 thumb bit).
 * We resolve the address here, set R0 to it, and tail-call via the address
 * just like the original's `mov lr, pc; bx r0` followed by `pop {pc}`. */
static void dispatch_handler(u32 index) {
    u32 target = LD32(A_JumpTable80AA6B8 + (index << 2));
    GUEST_CALL_ADDR(target);
}

/* dispatchGameHandlerByIndex_80AA6A4(r0 = handler index) -> r0 (handler's return value),
 *                                    r2..r12 forwarded from handler */
Next dispatchGameHandlerByIndex_80AA6A4(void) {
    dispatch_handler(R0);
    RETURN_TO_CALLER();
}

/* ── alwaysTrue_80AA6E8 ── */
/* alwaysTrue_80AA6E8:
 * Trivial helper that always reports "true". Sets R0 = 1 and flags NZ for
 * result 1 (N clear, Z clear), then returns to the caller. */
Next alwaysTrue_80AA6E8(void) {
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── isEventFlag212Set_80AA764 ── */
/* isEventFlag212Set_80AA764: returns 1 in r0 if event flag EVENT_212 is set, else 0.
 * Flags (N/Z) reflect the returned value, as the original's final
 * `mov r0, r4` does. */
Next isEventFlag212Set_80AA764(void) {
    u32 result = 0;

    /* TestEventFlagFromImmediate(flag): flag id 0x212 passed as
     * group in r0, number in r1. The answer is the Z flag (r0 is the
     * flag's whole byte). */
    R0 = 2;
    R1 = 0x12;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (cpu.nz != 0)
        result = 1;

    R0 = result;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── isEventFlag213Set_80AA778 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; bool z, n; } call_TestEventFlagFromImmediate_result_sub_80AA778;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_sub_80AA778 call_TestEventFlagFromImmediate_sub_80AA778(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_sub_80AA778){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Returns 1 if event flag EVENT_213 is set, else 0.
 * (movflag EVENT_213 encodes the flag as bank 2, index 0x13 for
 * TestEventFlagFromImmediate; Z is clear when the flag is set.) */
static u32 check_event_213(void) {
    if (!call_TestEventFlagFromImmediate_sub_80AA778(2, 0x13).z)
        return 1;                       /* flag set */
    return 0;                           /* flag clear */
}

/* isEventFlag213Set_80AA778() -> r0 = EVENT_213 state; r4 is preserved (pushed/popped). */
Next isEventFlag213Set_80AA778(void) {
    R0 = check_event_213();
    RETURN_TO_CALLER();
}

/* ── tryEvent214LuckyRoll_80AA78C ── */
/* tryEvent214LuckyRoll_80AA78C: decides whether the EVENT_214 encounter is "lucky".
 * Returns 1 when the event flag (immediate 2) is set and a roll in [0,255]
 * is <= 0xC0 (i.e. with probability ~75% when the flag is set).
 *
 *   inputs:  r10 lr
 *   outputs: r0 r2 r4 r7
 *   clobbers: nz c v r1 r3 lr
 *   preserved: r4 r7
 */
Next tryEvent214LuckyRoll_80AA78C(void) {
    u32 lucky = 0;

    /* TestEventFlagFromImmediate takes the flag index in R1 (low 16 bits used):
     * the original lifts R0=2 (the EVENT_214 selector) and R1=0x14 (20),
     * and TestEventFlagFromImmediate is the variant driven by R1 here. */
    R0 = 2;
    R1 = 0x14;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (ZERO_FLAG()) {
        /* Flag not set: lucky stays 0, fall through. */
    } else {
        GUEST_CALL(GetPositiveSignedRNG);
        /* Original: r0 = r0 & 0xff; cmp r0, #0xc0; bgt skip -> skip on signed >.
         * Use the post-call R0 here; result fits in 0..255. */
        if ((s32)(R0 & 0xFF) <= 0xC0) {
            lucky = 1;
        }
    }

    R0 = lucky;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── get_eventFlag215AsBool ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; bool z, n; } call_TestEventFlagFromImmediate_result_get_eventFlag215AsBool;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_get_eventFlag215AsBool call_TestEventFlagFromImmediate_get_eventFlag215AsBool(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_get_eventFlag215AsBool){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Read the story/scene event flag EVENT_215 (bank in the high byte, index in the
 * low byte) and return it as a plain boolean: 1 when the flag is set, 0 when not.
 * TestEventFlagFromImmediate leaves Z clear when the flag is set. */
static bool read_event_flag_215(void) {
    if (call_TestEventFlagFromImmediate_get_eventFlag215AsBool(EVENT_215 >> 8, EVENT_215 & 0xFF).z)
        return false;   /* flag clear */
    return true;        /* flag set */
}

/* get_eventFlag215AsBool() -> r0 = flag as 0/1 (r4 is saved and restored) */
Next get_eventFlag215AsBool(void) {
    R0 = read_event_flag_215();
    RETURN_TO_CALLER();
}

/* ── isEventFlag216Set ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; bool z, n; } call_TestEventFlagFromImmediate_result_isEventFlag216Set;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_isEventFlag216Set call_TestEventFlagFromImmediate_isEventFlag216Set(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_isEventFlag216Set){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* EVENT_216 is flag bank 2, index 0x16 (the "movflag EVENT_216" of the original). */

typedef struct {
    u32 r0;   /* 1 if the flag is set, else 0 */
    u32 r4;   /* passed through unchanged (the original only borrows r4) */
} EventFlag216Result;

/* Returns whether event flag 216 is currently set. */
static EventFlag216Result check_event_flag_216(u32 saved_r4) {
    EventFlag216Result result;

    /* TestEventFlagFromImmediate: Z is clear when the flag is set. */
    bool is_set = !call_TestEventFlagFromImmediate_isEventFlag216Set(2, 0x16).z;

    result.r0 = is_set ? 1u : 0u;
    result.r4 = saved_r4;
    return result;
}

/* isEventFlag216Set() -> r0 = flag state, r4 = passthrough */
Next isEventFlag216Set(void) {
    EventFlag216Result result = check_event_flag_216(R4);
    R0 = result.r0;
    R4 = result.r4;
    RETURN_TO_CALLER();
}

/* ── sub_80AA824 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r3; } call_enemy_getStruct2_result;
/* register form: generated wrapper: enemy_getStruct2 has no typed C interface yet */
static inline call_enemy_getStruct2_result call_enemy_getStruct2(u32 r0) {
    R0 = r0;
    GUEST_CALL(enemy_getStruct2);
    return (call_enemy_getStruct2_result){ R0, R3 };
}

/* Result of the enemy-icon walk: final accumulator (→r0) and last ORR term (→r2). */
struct enemy_icon_walk_result {
    u32 r0;
    u32 r2;
};

/* Walk the per-enemy state entry list, accumulating icon bits.
 *   Seed: if byte at obj+7 == 2, start flags at 0x40.
 *   Each entry's first byte & 0xF0 selects:
 *     0xF0 → terminator; return accumulated flags.
 *     0x10 → enemy_getStruct2 lookup; OR (1 << nibble) into flags.
 *            If struct byte[2] >= 4, short-circuit return 0x20.
 *     else → advance entry by 4, keep scanning.
 * r2 tracks the most recent `1 << shift` value (unchanged from input if
 * the 0x10 case is never reached). */
static struct enemy_icon_walk_result sub_80AA824_impl(u32 obj, u32 r2_in) {
    u32 flags = (LD8(obj + 7) == 2) ? 0x40u : 0u;
    u32 entry = LD32(obj + 0xC);
    u32 r2 = r2_in;

    for (;;) {
        u32 kind = LD8(entry) & 0xF0u;
        if (kind == 0xF0u)
            return (struct enemy_icon_walk_result){ flags, r2 };

        if (kind != 0x10u) {
            entry += 4;
            continue;
        }

        /* Build struct index: byte[+2] + (nibble from byte[+3]) << 8 */
        u32 idx = LD8(entry + 2) + (((u32)LD8(entry + 3) << 28) >> 20);
        u32 e   = call_enemy_getStruct2(idx).r0;

        u32 shift = (LD16(e) >> 12) & 0xFu;
        r2 = 1u << shift;
        flags |= r2;

        if ((s32)LD8(e + 2) >= 4)
            return (struct enemy_icon_walk_result){ 0x20u, r2 };

        entry += 4;
    }
}

/* sub_80AA824(r0 = enemy object ptr, r2 = in/out flag word) -> r0, r2 */
Next sub_80AA824(void) {
    struct enemy_icon_walk_result res = sub_80AA824_impl(R0, R2);
    R0 = res.r0;
    R2 = res.r2;
    RETURN_TO_CALLER();
}

/* ── battle_rollRandomItemDrops_80AA88C ── */
/* register form: calls GetPositiveSignedRNG outside this file, which has no typed C interface yet */
static u32 get_positive_rng(void) {
    GUEST_CALL(GetPositiveSignedRNG);
    return R0;
}

/* register form: calls sub_80AED50 outside this file, which has no typed C interface yet */
static u32 item_table_for(u32 kind) {
    R0 = kind;
    GUEST_CALL(sub_80AED50);
    return R0;
}

/*
 * Pick a random item drop for one of the four battle-state slots at
 * battle+0x90: for each non-empty slot, look up its item table, roll an
 * entry with the RNG, and remember the last slot that yielded a real item
 * (0xFFFF = none). The chosen slot then receives the item id and a
 * "item waiting" flag. Returns the final loop counter (4), since the
 * callers only see the flags of the loop's terminating comparison.
 */
static u32 roll_random_drop(u32 toolkit) {
    u32 rng = get_positive_rng();
    u32 chosen_state = 0;
    u32 chosen_item = 0;

    u32 slot = LD32(toolkit + oToolkit_BattleStatePtr) + 0x90;

    u32 i;
    for (i = 0; i < 4; i++) {
        u32 state = LD32(slot);
        if (state != 0) {
            u32 table = item_table_for(LD16(state + oBattleState_CountdownTimer));

            rng >>= 4;
            u32 index = rng & 0x1E;   /* halfword-aligned table offset */
            rng >>= 5;

            u32 item = LD16(table + index);
            if (item != 0xFFFF) {
                chosen_state = state;
                chosen_item = item;
            }
        }
        slot += 4;
    }

    if (chosen_state != 0) {
        ST16(chosen_state + 0x2A, chosen_item);   /* Unk_2A: rolled item id */
        ST8(chosen_state + 0x1A, 1);              /* Unk_1A: drop pending */
    }

    return i;
}

/* battle_rollRandomItemDrops_80AA88C(r10 = toolkit) -> flags c/v from the loop exit compare */
Next battle_rollRandomItemDrops_80AA88C(void) {
    SET_FLAGS_CMP(roll_random_drop(R10), 4);
    RETURN_TO_CALLER();
}

/* ── sub_80AA8E0 ── */
/* sub_80AA8E0(r0-r5 = setup parameters, r10 = toolkit pointer):
 * Forwards a six-word parameter block to sub_80AA910 (which reads its
 * arguments back from the caller's stack at [sp, #n]), then returns
 *   r0 = value returned by sub_80AA910,
 *   r1 = the current battle state's oBattleState_Reward2Bonus field.
 * If r1 is 0 on entry, the helper is not called and (0, 0) is returned. */
Next sub_80AA8E0(void) {
    u32 setupResult = 0;
    u32 battleFlag = 0;

    if (R1 != 0) {
        u32 savedR7 = R7;

        /* Build the argument block on the stack for sub_80AA910.
         * The two trailing words are zeroed, as the original does. */
        u32 args = SP -= 0x2C;
        ST32(args + 0x00, R0);
        ST32(args + 0x04, R1);
        ST32(args + 0x08, R2);
        ST32(args + 0x0C, R3);
        ST32(args + 0x10, R4);
        ST32(args + 0x14, R5);
        ST32(args + 0x1C, 0);
        ST32(args + 0x20, 0);

        /* sub_80AA910 expects r7 = 0 (r0-r6, r8-r12 keep their incoming
         * values; it preserves the callee-saved ones itself). */
        R7 = 0;
        GUEST_CALL(sub_80AA910);
        SP += 0x2C;
        setupResult = R0;

        /* Read the current battle state's field 0x36 for the second result. */
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        battleFlag = LD16(battleState + oBattleState_Reward2Bonus);

        /* r7 is outgoing-preserved, but the callee saw it as 0. */
        R7 = savedR7;
    }

    R0 = setupResult;
    R1 = battleFlag;
    RETURN_TO_CALLER();
}

/* ── sub_80AA910 ── */
/* register form: calls GetBattleNaviStatsByte outside this file, which has no typed C interface yet */
static u32 call_navi_stats_byte(u32 navi, u32 index) {
    R0 = navi;
    R1 = index;
    GUEST_CALL(GetBattleNaviStatsByte);
    return R0;
}

/* register form: calls sub_80AAA3C outside this file, which has no typed C interface yet */
static u32 call_select_navi(u32 a0, u32 a1, u32 mode) {
    R0 = a0;
    R1 = a1;
    R2 = mode;
    GUEST_CALL(sub_80AAA3C);
    return R0;
}

/* register form: calls sub_80AAE98 outside this file, which has no typed C interface yet */
static u32 call_get_navi_base(void) {
    GUEST_CALL(sub_80AAE98);
    return R0;
}

/* register form: calls sub_80AAB04 outside this file, which has no typed C interface yet */
static u32 call_apply_chip(u32 base, u32 a1, u32 a2) {
    R0 = base;
    R1 = a1;
    R2 = a2;
    GUEST_CALL(sub_80AAB04);
    return R0;
}

/* register form: calls damage_jitter_80AAA98 outside this file, which has no typed C interface yet;
 * R4 must carry base (caller's r4 is preserved by GUEST_CALL, set it for the callee) */
static u32 call_attr_offset(u32 a0, u32 a1, u32 a2, u32 mode, u32 base) {
    u32 saved_r4 = R4;
    R0 = a0;
    R1 = a1;
    R2 = a2;
    R3 = mode;
    R4 = base;
    GUEST_CALL(damage_jitter_80AAA98);
    u32 result = R0;
    R4 = saved_r4;
    return result;
}

/* register form: calls GetPositiveSignedRNG outside this file, which has no typed C interface yet;
 * R7 carries the offset parameter the RNG uses */
static u32 call_positive_rng(u32 offset) {
    u32 saved_r7 = R7;
    R7 = offset;
    GUEST_CALL(GetPositiveSignedRNG);
    u32 result = R0;
    R7 = saved_r7;
    return result;
}

/* register form: BIOS SWI 6 (divide R0 by R1; remainder in R1) */
static u32 call_divmod_remainder(u32 numerator, u32 modulus) {
    R0 = numerator;
    R1 = modulus;
    swi_call(6);
    return R1;
}

/* Scan two consecutive u16 entries at (base + byte_off) stepping back by 2.
 * An entry is valid if bits 15:14 are zero. Valid entries collected into pool[].
 * Returns how many were found (0, 1, or 2). */
static u32 scan_attr_pair(u32 base, u32 byte_off, u16 pool[2]) {
    u32 count = 0;
    u32 addr = base + byte_off;
    for (u32 i = 0; i < 2; i++) {
        u16 val = LD16(addr);
        if ((val >> 14) == 0)
            pool[count++] = val;
        addr -= 2;
    }
    return count;
}

typedef struct {
    u32 value;
    u32 flag_a;
    u32 flag_b;
} ChipAttrResult;

/*
 * sub_80AA910: pick a chip attribute for the active navi based on stats byte 0x26.
 *
 * Bit 0 → mode A: apply chip via sub_80AAB04.
 * Bit 1 clear → mode B: read one attribute by computed offset.
 * Bit 1 set → mode C: scan attribute table columns for valid entries (bits 15:14 == 0),
 *   then pick one at random via GetPositiveSignedRNG + SWI divide.
 */
static ChipAttrResult pick_chip_attr(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5) {
    ChipAttrResult res;
    res.flag_a = 0;
    res.flag_b = 0;

    u32 stats = call_navi_stats_byte(a5, 0x26);

    /* Mode A: apply chip directly */
    if (stats & 1) {
        call_select_navi(a0, a1, 0);
        u32 base = call_get_navi_base();
        res.value = call_apply_chip(base, a3, a4);
        return res;
    }

    /* Mode B: single attribute by computed offset */
    if (!(stats & 2)) {
        call_select_navi(a0, a1, 0);
        u32 base = call_get_navi_base();
        u32 off = call_attr_offset(a2, a3, a4, 0, base);
        res.value = LD16(base + off);
        return res;
    }

    /* Mode C: random attribute selection via column scan */
    if (call_select_navi(a0, a1, 1) == 0) {
        res.value = (u32)-1;
        return res;
    }

    u32 base = call_get_navi_base();
    u32 offset = call_attr_offset(a2, a3, a4, 2, base);

    if ((s32)offset >= 0x14) {
        base += 0x14;
        offset -= 0x14;
    }

    u16 pool[2];
    u32 saved_offset = offset;
    u32 found_count = 0;

    /* Backward scan: from offset down by 4 while (col - 4) >= 0, i.e. col >= 4.
     * Original: "sub r2, #4; bge" tests the result of r2-4, so the last
     * scanned col satisfies col >= 4; cols 0..3 are not scanned here. */
    for (u32 col = saved_offset; col >= 4; col -= 4) {
        found_count = scan_attr_pair(base, col, pool);
        if (found_count > 0)
            break;
    }

    if (found_count == 0) {
        /* Backward scan exhausted: if saved_offset >= 0x12, give up */
        if ((s32)saved_offset >= 0x12) {
            res.value = (u32)-1;
            return res;
        }

        /* Forward scan: from saved_offset up by 4 while < 0x14 */
        for (u32 col = saved_offset; (s32)col < 0x14; col += 4) {
            found_count = scan_attr_pair(base, col, pool);
            if (found_count > 0)
                break;
        }

        if (found_count == 0) {
            res.value = (u32)-1;
            return res;
        }
    }

    /* Pick a random entry from the pool */
    u32 rng = call_positive_rng(saved_offset);
    u32 remainder = call_divmod_remainder(rng, found_count);

    res.value = pool[remainder];
    res.flag_a = remainder;
    res.flag_b = remainder;
    return res;
}

/* sub_80AA910(r0=a0, r1=a1, r2=a2, r3=a3, r4=a4, r5=a5) -> r0; outputs c,v; clobbers nz,r1-r3,lr */
Next sub_80AA910(void) {
    ChipAttrResult cr = pick_chip_attr(R0, R1, R2, R3, R4, R5);
    R0 = cr.value;
    SET_FLAGS_ADD(cr.flag_a, cr.flag_b);
    RETURN_TO_CALLER();
}

/* ── damage_jitter_80AAA98 ── */
/* register form: calls GetPositiveSignedRNG outside this file, which has no typed C interface yet */
static u32 get_positive_signed_rng(void) {
    GUEST_CALL(GetPositiveSignedRNG);
    return R0;
}

/* Damage jitter for one hit.
 *   param1     base/lookup index (r0)
 *   bonus      signed value compared against (r2/8)*3 (r1)
 *   raw_mag    drives the (r2/8)*3 cap (r2)
 *   mode       0 = lookup+jitter+bonus, 1 = jitter+bonus, 2 = force jitter=2 then mode 0
 * Returns the result left in r0 (the original's last ADDS path's final add). */
static u32 damage_jitter(u32 param1, s32 bonus, u32 raw_mag, u32 mode) {
    u32 cap = 0x14;
    u32 mag_shifted = raw_mag >> 3;
    u32 cmp_value = mag_shifted * 3;            /* (r2/8) + (r2/8) + (r2/8) */

    /* cmp r1, r3 ; ble +2 ; mov r0,#0 — branch on signed */
    if (bonus > (s32)cmp_value)
        cap = 0;

    u32 rng = get_positive_signed_rng();
    u32 step = ((rng >> 8) & 1u) * 2u;          /* (rng>>8) & 1, then doubled */
    u32 lookup = (rng >> 16) & 0xFu;           /* next byte & 0xF */

    for (;;) {
        switch (mode) {
        case 0: {
            u8 base = LD8(A_byte_8020B9C + (param1 << 4) + lookup);
            /* original computes add,add,add (last is ADDS r0,r0,r1 -> r0 = base+step+cap) */
            return (u32)(base + step + cap);
        }
        case 1: {
            /* step + cap; the original's last op here is ADDS r0,r0,r1 */
            return (u32)(step + cap);
        }
        case 2: {
            step = 2;
            mode = 0;
            continue;
        }
        default:
            /* original: b loc_80AAAFA (infinite loop on any other value) */
            continue;
        }
    }
}

/* damage_jitter_80AAA98(r0=param1, r1=bonus, r2=raw_mag, r3=mode, r4=preserved) -> c v r0 r4 */
Next damage_jitter_80AAA98(void) {
    u32 out_r0 = damage_jitter(R0, (s32)R1, R2, R3);
    R0 = out_r0;
    /* Original's last flag-setting instruction on every exit path is
     * ADD(S) r0, r0, r1 (the final step's bonus add). With r1=0 here
     * (the prior add consumed the bonus into r0 then b exit), the flags
     * equal those of ADD r0, #0. */
    SET_FLAGS_ADD(R0, 0);
    /* R4 preserved (typed function never touches it). */
    RETURN_TO_CALLER();
}

/* ── util_nandByteArray40_80AAB88 ── */
/* util_nandByteArray40_80AAB88(r0 = srcA, r1 = srcB, r2 = dst):
 * NAND two 40-byte arrays together: dst[i] = ~(srcA[i] | srcB[i]).
 * Leaves the loop counter (0x28) and the last srcB byte read in r3/r1,
 * with the final CMP's flags (c set, v clear) for callers to branch on. */
Next util_nandByteArray40_80AAB88(void) {
    u32 srcA = R0;      /* r6 in the original */
    u32 srcB = R1;      /* r7 in the original */
    u32 dst = R2;       /* r5 in the original */
    u32 i;              /* r3 in the original */
    u32 lastB = 0;      /* last value loaded into r1 */

    for (i = 0; i < 0x28; i++) {
        u32 a = LD8(srcA + i);
        u32 b = LD8(srcB + i);
        lastB = b;
        ST8(dst + i, ~(a | b));
    }

    R1 = lastB;
    R3 = i;             /* ends at 0x28 */
    SET_FLAGS_CMP(i, 0x28);
    RETURN_TO_CALLER();
}

/* ── sub_80AAE98 ── */
/* sub_80AAE98(r0 = index): return &byte_80AAEA8[index * 0x28] (each entry is 0x28 bytes). */
Next sub_80AAE98(void) {
    R0 = (u32)R0 * 0x28 + (u32)A_byte_80AAEA8;
    RETURN_TO_CALLER();
}
