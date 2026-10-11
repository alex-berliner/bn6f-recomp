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
    case 0x0: GUEST_CALL(sub_80A49B0);   break;
    case 0x4: GUEST_CALL(event_80A4A08); break;
    case 0x8: GUEST_CALL(sub_80A4A90);   break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_80A4A98 ── */
/* register form: calls battle_objectEnterState4LatchTilePos_80A4AB8 outside this file,
 * which has no typed C interface yet */
static void call_latch_tile_pos(void) {
    GUEST_CALL(battle_objectEnterState4LatchTilePos_80A4AB8);
}

/* register form: calls sub_80A4ACC outside this file, which has no typed C interface yet */
static void call_state_handler_4acc(void) {
    GUEST_CALL(sub_80A4ACC);
}

/* register form: calls sub_80A4BD4 outside this file, which has no typed C interface yet */
static void call_state_handler_4bd4(void) {
    GUEST_CALL(sub_80A4BD4);
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

/* sub_80A4A98(r5 = battle object ptr) -> all registers, flags (passed through / set by callee) */
Next sub_80A4A98(void) {
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
    case 0: GUEST_CALL(sub_80A4BFC); break;
    case 4: GUEST_CALL(sprite_tickWithFlagReset_80A4C46); break;
    case 8: GUEST_CALL(sub_80A4C5C); break;
    /* other byte values would index past the table and never occur */
    }

    RETURN_TO_CALLER();
}

/* ── sub_80A4BFC ── */
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

/* sub_80A4BFC: sprite setup for a shockwave/effect overworld object (writes the
 * state byte, loads the sprite, applies Z/layer transforms, sets up animation+palette,
 * kicks off the per-frame tick). r5 holds the object pointer; r6 is clobbered with
 * the original Z value loaded from offset 0x14. */
Next sub_80A4BFC(void) {
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

/* ── npc_stateDispatchStep_80A51C4 ── */
/* npc_stateDispatchStep_80A51C4(r5 = state struct):
 * Points r7 at a 0x10-byte record in the table at byte_80A4C64, selected by
 * the byte at r5+4, then calls one of five handlers selected by the byte at
 * r5+8, which indexes the table at off_80A51E0 bytewise (entries are 4 bytes
 * apart, so the selector holds 0x00/0x04/0x08/0x0C/0x10):
 *   +0x00 sub_80A51F8   +0x04 sub_80A5268   +0x08 sub_80A52E4
 *   +0x0C sub_80A5304   +0x10 sub_80A5350
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
    case 0x00: GUEST_CALL(sub_80A51F8); break;
    case 0x04: GUEST_CALL(sub_80A5268); break;
    case 0x08: GUEST_CALL(sub_80A52E4); break;
    case 0x0C: GUEST_CALL(sub_80A5304); break;
    case 0x10: GUEST_CALL(sub_80A5350); break;
    /* Selector outside the table: branch to whatever the load yielded. */
    default:   dispatch(target);        break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_80A51F8 ── */
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

/* register form: calls sub_80A5268 outside this file, which has no typed C interface yet */
static u32 call_sub_80A5268(void) {
    GUEST_CALL(sub_80A5268);
    return R0;
}

/* register form: calls sub_80A5350 outside this file, which has no typed C interface yet */
static u32 call_sub_80A5350(void) {
    GUEST_CALL(sub_80A5350);
    return R0;
}

/* ---------- typed function ---------- */

/* Entry-controller update. If the entry's trigger flag is set, load the sprite for it,
 * clear its two associated flags, and call sub_80A5268. Otherwise set those flags,
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

Next sub_80A51F8(void) {
    R0 = update_entry(R5, R7);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80A5268 ── */
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

/* sub_80A5268(r2, r4, r5=parent, r6, r7=sub, r10, lr) -> preserved */
Next sub_80A5268(void) {
    /* r7/r5 are inputs the typed function consumes; lr is left as-is by the original
     * (`pop {pc}` returns via the saved lr). No other output is computed here. */
    sub_80A5268_impl(R7, R5);
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
    case 0: GUEST_CALL(sub_80A9B9C); break;
    case 4: GUEST_CALL(sub_80A9C26); break;
    case 8: GUEST_CALL(sub_80A9CA6); break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_80A9B9C ── */
/* sub_80A9B9C — bring a freshly spawned overworld object live.
 *
 * r5 = object state block, r7 = spawn config table entry. The routine:
 *   - marks the object "spawning" (state 3) and clears its timer (+0x24),
 *   - resolves the spawn flags from the config (+0/+2/+4): on a repeat spawn
 *     the object's event flag is (re)set and the companion flag cleared;
 *     on a first spawn the object is parked in state 1 instead,
 *   - loads the sprite described by the config (+6/+7) and applies the
 *     animation (+8), unk0x2c (+0xA), palette (+0xB), priority (+0xC, with
 *     0xFF meaning "derive from the object's layer/z") and flip (+0xD),
 *   - marks the object active (state 4 at +8) and calls sub_80A9C26 to finish.
 *
 * Everything the original leaves behind in r0-r3, r6 and the flags on exit is
 * produced by its last callee (sub_80A9C26), so it stays in the guest registers
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

/* register form: calls sub_80A9C26 outside this file, which has no typed C interface yet.
 * It continues initialising the object still held in r5 (with r6/r7 alongside) and is the
 * last call of the original, so its r0-r2 leftovers and flags are this function's outputs. */
static void finish_object_init(void) {
    GUEST_CALL(sub_80A9C26);
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

Next sub_80A9B9C(void) {
    spawn_ow_object(R5, R7);
    RETURN_TO_CALLER();
}

/* ── sub_80A9C26 ── */
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

/* register form: calls sub_80A9CAE outside this file, which has no typed C interface yet */
static u32 event_npc_check(void) {
    /* r0 is deliberately NOT set here: the original passes along whatever
     * r0 the preceding TestEventFlag call left behind. */
    GUEST_CALL(sub_80A9CAE);
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

/* sub_80A9C26(r5 = object, r7 = flag script record)
 *
 * All register outputs (r0-r3 and nz/c/v) are whatever the final
 * sprite_update call leaves behind, and r4/r6/r10/lr are untouched,
 * so the adapter only forwards r5/r7 and returns. */
Next sub_80A9C26(void) {
    update_event_flag_object(R5, R7);
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

/* ── sub_80AA2D8 ── */
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

/* register form: calls sub_80AA36C outside this file, which has no typed C interface yet */
static void call_sprite_state3_done(void) {
    GUEST_CALL(sub_80AA36C);
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

/* sub_80AA2D8(r5 = battle object) -> tail-dispatch to a sprite-state-3 handler */
Next sub_80AA2D8(void) {
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

/* ── sub_80AA4C0 ── */
/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u32 call_GetCurPETNavi(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u32 call_GetCurPETNaviStatsByte(u32 navi, u32 stat) {
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return R0;
}

/* register form: calls GetCurPETNaviStatsHword outside this file, which has no typed C interface yet */
static u32 call_GetCurPETNaviStatsHword(u32 navi, u32 stat) {
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsHword);
    return R0;
}

/* register form: calls GetRNG outside this file, which has no typed C interface yet */
static u32 call_GetRNG(void) {
    GUEST_CALL(GetRNG);
    return R0;
}

/* register form: calls GetPositiveSignedRNG outside this file, which has no typed C interface yet */
static u32 call_GetPositiveSignedRNG(void) {
    GUEST_CALL(GetPositiveSignedRNG);
    return R0;
}

/* register form: calls sub_80AA5F4 outside this file, which has no typed C interface yet */
static u32 call_sub_80AA5F4(u32 param) {
    R2 = param;
    GUEST_CALL(sub_80AA5F4);
    return R0;
}

/* register form: calls GetBattleSettingsUnk01FromBattleSettings outside this file, which has no typed C interface yet */
static u32 call_GetBattleSettingsUnk01(u32 settings_ptr) {
    R0 = settings_ptr;
    GUEST_CALL(GetBattleSettingsUnk01FromBattleSettings);
    return R0;
}

/* register form: calls BIOS unsigned divide (svc 6) outside this file */
static u32 call_svc_div(u32 dividend, u32 divisor) {
    R0 = dividend;
    R1 = divisor;
    swi_call(6);
    return R0;
}

/* Attempt to pick a random overworld encounter battle.
 * Returns a BattleSettings pointer in r0, or NULL. */
static void try_encounter(u32 toolkit, u32 r1_in, u32 r2_in,
                          u32 *out_r0, u32 *out_r1, u32 *out_r2, u32 *out_r3) {
    /* Load game-state and encounter-tracker pointers from toolkit */
    u32 gs = LD32(toolkit + oToolkit_GameStatePtr);
    u32 s20 = LD32(toolkit + oToolkit_S2001c04_Ptr);

    /* Gate: must be on an overworld map (map group >= 0x80) */
    u32 rg3 = LD8(gs + oGameState_MapGroup);
    if (rg3 < 0x80) {
        *out_r0 = 0; *out_r1 = r1_in; *out_r2 = r2_in; *out_r3 = rg3;
        return;
    }
    rg3 -= 0x80; /* area index */

    /* Encounter-credit gate: (Unk_12 - Unk_14) must be >= 0x40 */
    u32 rg1 = LD16(s20 + oS2001c04_Unk_12);
    u32 rg2 = LD16(s20 + oS2001c04_Unk_14);
    u32 credit = rg1 - rg2;
    if ((s32)credit < (s32)0x40) {
        *out_r0 = 0; *out_r1 = credit; *out_r2 = rg2; *out_r3 = rg3;
        return;
    }

    /* Update encounter counter: Unk_14 = original Unk_12 */
    u32 orig_unk12 = LD16(s20 + oS2001c04_Unk_12);
    ST16(s20 + oS2001c04_Unk_14, orig_unk12);

    /* Look up encounter type from per-area table (16 entries per area) */
    u32 map_num = LD8(gs + oGameState_MapNumber);
    u32 tbl_idx = (rg3 << 4) + map_num;
    rg2 = LD8(A_byte_8020CE4 + tbl_idx);
    rg3 = tbl_idx;
    if (rg2 == 7) {
        *out_r0 = 0; *out_r1 = orig_unk12; *out_r2 = 7; *out_r3 = rg3;
        return;
    }

    /* PET Navi stat 0x28 nonzero overrides encounter type to 0 */
    u32 type = rg2;
    u32 navi = call_GetCurPETNavi();
    if (call_GetCurPETNaviStatsByte(navi, 0x28) != 0)
        type = 0;

    /* Difficulty row from encounter counter (Unk_12 >> 6), capped at 0x10 */
    rg3 = LD16(s20 + oS2001c04_Unk_12) >> 6;
    if ((s32)rg3 >= (s32)0x11)
        rg3 = 0x10;

    /* RNG threshold check against byte table byte_8020C5C[row*8 + type] */
    u8 threshold = LD8(A_byte_8020C5C + (rg3 << 3) + type);
    u32 rng_val = call_GetRNG();
    u32 rg1_ = 0x1F;
    rg2 = rng_val & 0x1F;
    rg3 = threshold;
    if (rg2 >= rg3) {
        *out_r0 = 0; *out_r1 = rg1_; *out_r2 = rg2; *out_r3 = rg3;
        return;
    }

    /* Preset-battle shortcut when Unk_28 flag is set */
    if (LD32(s20 + oS2001c04_Unk_28) != 0) {
        u32 rng2 = call_GetPositiveSignedRNG();
        u32 shifted = rng2 >> 1;
        if ((rng2 & 1) == 0) {
            u32 preset = LD32(s20 + oS2001c04_OptCurBattleDataPtr);
            if (preset != 0 && (preset >> 24) == 8) {
                ST32(gs + oGameState_CurBattleDataPtr, preset);
                rg1 = preset;
                shifted = preset >> 24;
                if (shifted == 8) {
                    *out_r0 = preset; *out_r1 = shifted; *out_r2 = rg2; *out_r3 = rg3;
                    return;
                }
            }
        }
        /* Fall through to normal encounter path */
    }

    /* Obtain battle settings via sub_80AA5F4 (r10/r12 may be clobbered by this call) */
    u32 st = A_dword_2000B30;
    u32 settings;
    bool have = false;

    /* Special: first encounter at this position (counter byte == 0) gets difficulty 0x40 */
    if (LD16(st) == 0 && LD16(st + 2) < 8) {
        settings = call_sub_80AA5F4(0x40);
        if (settings != 0) {
            ST16(st, 1);
            have = true;
        }
    }

    if (!have) {
        /* 1/12 chance (remainder == 6) to request medium difficulty 0x20 */
        u32 rng3 = call_GetPositiveSignedRNG();
        u32 quot = call_svc_div(rng3, 0xC);
        if (quot == 6) {
            settings = call_sub_80AA5F4(0x20);
            if (settings != 0)
                have = true;
        }
    }

    if (!have) {
        /* Fallback: use PET Navi stat 0x27 as difficulty, or 0x1f */
        u32 navi2 = call_GetCurPETNavi();
        settings = call_GetCurPETNaviStatsByte(navi2, 0x27);
        settings = call_sub_80AA5F4(settings);
        if (settings == 0)
            settings = call_sub_80AA5F4(0x1F);
    }

    /* Store selected battle data pointer in game state */
    ST32(gs + oGameState_CurBattleDataPtr, settings);

    /* If Unk_24 is zero, PET Navi stat 0x1E must be nonzero */
    if (LD32(s20 + oS2001c04_Unk_24) == 0) {
        u32 navi3 = call_GetCurPETNavi();
        u32 stat1e = call_GetCurPETNaviStatsByte(navi3, 0x1E);
        if (stat1e == 0) {
            *out_r0 = settings; *out_r1 = stat1e; *out_r2 = rg2; *out_r3 = rg3;
            return;
        }
    }

    /* Power check: (hword_0x3E + 4) / 5 must exceed BattleSettings->unk01 */
    u32 unk01 = call_GetBattleSettingsUnk01(settings);
    u32 navi4 = call_GetCurPETNavi();
    u32 pw = call_GetCurPETNaviStatsHword(navi4, 0x3E);
    u32 pw_quot = call_svc_div(pw + 4, 5);

    if ((s32)pw_quot <= (s32)unk01) {
        /* Underpowered: battle proceeds */
        *out_r0 = settings; *out_r1 = pw_quot; *out_r2 = rg2; *out_r3 = rg3;
        return;
    }

    /* Overpowered: reset encounter counters, no battle */
    ST16(s20 + oS2001c04_Unk_12, 0);
    ST16(s20 + oS2001c04_Unk_14, 0);
    *out_r0 = 0; *out_r1 = pw_quot; *out_r2 = rg2; *out_r3 = rg3;
}

/* sub_80AA4C0(toolkit_ptr=r10) -> r0 BattleSettings ptr, nz flag */
Next sub_80AA4C0(void) {
    u32 r0, r1, r2, r3;
    try_encounter(R10, R1, R2, &r0, &r1, &r2, &r3);
    R0 = r0; R1 = r1; R2 = r2; R3 = r3;
    SET_FLAGS_NZ(r0);
    RETURN_TO_CALLER();
}

/* ── sub_80AA5F4 ── */
#define SETTINGS_MAX 29

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_80AA5F4(u32 bank, u32 flag) {
    R0 = bank; R1 = flag;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls sub_80AA6A4 outside this file, which has no typed C interface yet */
static u32 entry_condition_ok(u32 cond) {
    R0 = cond;
    GUEST_CALL(sub_80AA6A4);
    return R0;
}

/* register form: calls sub_80AA824 outside this file, which has no typed C interface yet */
static u32 entry_filter_mask(u32 entry) {
    R0 = entry;
    GUEST_CALL(sub_80AA824);
    return R0;
}

struct SettingsResult {
    u32 entry;
    u32 r1_val;
    u32 cmp_a;
    u32 cmp_b;
};

/*
 * Pick a random battle-settings entry from the area's entry list, filtered
 * by a bitmask and validated by sub_80AA6A4.  The area-specific table is
 * chosen by three event-flag tests (bank 6).  Selection uses iCurrFrame
 * modulo the match count for round-robin cycling.
 */
static struct SettingsResult get_battle_settings(u32 filter, u32 toolkit) {
    struct SettingsResult res;

    /* Select settings-table pointer via event flags (bank 6, 0x7F/0x80/0x81) */
    u32 table_addr;
    if (event_flag_set_sub_80AA5F4(6, 0x7F))
        table_addr = A_off_8020178;
    else if (event_flag_set_sub_80AA5F4(6, 0x80))
        table_addr = A_off_8020180;
    else if (event_flag_set_sub_80AA5F4(6, 0x81))
        table_addr = A_off_8020188;
    else
        table_addr = A_off_8020170;

    /* Read current map location from the game state */
    u32 gs = LD32(toolkit + oToolkit_GameStatePtr);
    u32 map_group = LD8(gs + oGameState_MapGroup);
    u32 map_number = LD8(gs + oGameState_MapNumber);

    /* Bit 7 of the map group selects the alternate table column */
    if (map_group >= 0x80) {
        map_group -= 0x80;
        table_addr += 4;
    }

    /* Two-level dereference: table_addr -> row -> entry-list pointer */
    u32 table = LD32(table_addr);
    u32 row = LD32(table + map_group * 4);
    u32 list = LD32(row + map_number * 4);

    /* Walk 0x10-byte entries; terminator is 0xFF at offset 0 */
    struct { u16 used; u32 pad; u32 ptr; } buf[SETTINGS_MAX];
    u32 count = 0;

    while ((u8)LD8(list) != 0xFF) {
        if (entry_condition_ok((u8)LD8(list + 7))) {
            u32 mask = entry_filter_mask(list);
            if (mask & filter) {
                buf[count].ptr = list;
                count++;
            }
        }
        list += 0x10;
    }

    if (count == 0) {
        /* No matching entries; original exits with CMP 0xFF,0xFF flags (C=1,V=0) */
        res.entry = 0;
        res.r1_val = 0xFF;
        res.cmp_a = 0xFF;
        res.cmp_b = 0xFF;
    } else {
        /* Round-robin: frame % count picks which entry to return */
        u32 frame = LD16(A_iCurrFrame);
        u32 idx = frame % count;
        res.entry = buf[idx].ptr;
        /* Original's last flag-op was SUBS 0-1 in the scan loop (C=0,V=0) */
        res.r1_val = 0xFFFFFFFF;
        res.cmp_a = 0;
        res.cmp_b = 1;
    }

    return res;
}

/* sub_80AA5F4(r2=filter, r10=toolkit) -> r0=entry, r1, c, v; r2/r3 left by callees */
Next sub_80AA5F4(void) {
    struct SettingsResult r = get_battle_settings(R2, R10);
    R0 = r.entry;
    R1 = r.r1_val;
    SET_FLAGS_CMP(r.cmp_a, r.cmp_b);
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

/* ── sub_80AA88C ── */
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
            u32 table = item_table_for(LD16(state + oBattleState_Unk_28));

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

/* sub_80AA88C(r10 = toolkit) -> flags c/v from the loop exit compare */
Next sub_80AA88C(void) {
    SET_FLAGS_CMP(roll_random_drop(R10), 4);
    RETURN_TO_CALLER();
}

/* ── sub_80AA8E0 ── */
/* sub_80AA8E0(r0-r5 = setup parameters, r10 = toolkit pointer):
 * Forwards a six-word parameter block to sub_80AA910 (which reads its
 * arguments back from the caller's stack at [sp, #n]), then returns
 *   r0 = value returned by sub_80AA910,
 *   r1 = the current battle state's oBattleState_Unk_36 field.
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
        battleFlag = LD16(battleState + oBattleState_Unk_36);

        /* r7 is outgoing-preserved, but the callee saw it as 0. */
        R7 = savedR7;
    }

    R0 = setupResult;
    R1 = battleFlag;
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
