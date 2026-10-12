/*
 * asm32_811CAEC.c: readable functions of bn6f asm32.s, from 0x0811CAEC, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── timerCountdown_step_811CBA0 ── */
/* timerCountdown_step_811CBA0 (r5 = state object):
 * Runs the countdown timer at +0x10. The first time it runs (active flag in
 * the byte at +0x9 still clear) it marks itself active and initialises the
 * timer from the halfword at +2 of the entry returned by sub_811CC68.
 * Every call decrements the timer; once it goes negative the timer is reset
 * to 0, the active flag is cleared, 4 is added to the u32 counter at +0xC,
 * and the function returns 1. Otherwise it returns 0. */
Next timerCountdown_step_811CBA0(void) {
    u32 obj = R5;
    u32 timer;

    if (LD8(obj + 9) == 0) {
        u32 entry;
        ST8(obj + 9, 1);
        GUEST_CALL(sub_811CC68);
        entry = R0;
        ST32(obj + 0x10, LD16(entry + 2));
    }

    timer = LD32(obj + 0x10);
    SET_FLAGS_CMP(timer, 1);    /* flags of the original "subs r0, #1" */
    timer -= 1;
    ST32(obj + 0x10, timer);

    if ((s32)timer < 0) {
        /* Timer expired: reset state and bump the event counter. */
        ST8(obj + 9, 0);
        ST32(obj + 0x10, 0);
        R0 = 4;
        GUEST_CALL(counter_addU32_r5Plus0C_811CC60);
        R0 = 1;
        SET_FLAGS_NZ(1);
    } else {
        /* Still counting down. */
        R0 = 0;
        SET_FLAGS_NZ(0);
    }
    RETURN_TO_CALLER();
}

/* ── battle_matchTrackedSpriteAndShrink_811CBD0 ── */
/* battle_matchTrackedSpriteAndShrink_811CBD0 (r5 = object):
 * Copies the position of the sprite the object is currently tracking into
 * the object's own sprite, copies the tracked sprite's size halfwords into
 * the object (offsets 0x14 and 0x1A), shrinks the sprite, and starts a
 * 10-frame counter at r5+0xC. Always returns r0 = 1. */
Next battle_matchTrackedSpriteAndShrink_811CBD0(void) {
    /* Fetch a pointer to the sprite data the object is tracking. */
    GUEST_CALL(sub_811CC68);
    u32 tracked = R0;

    /* Move this object's sprite to the tracked sprite's position. */
    R0 = (u32)LDS16(tracked + 2);
    R1 = (u32)LDS16(tracked + 4);
    R2 = 2;
    GUEST_CALL(sprite_setCoordinates);

    /* Fetch the sprite data again and copy its two size halfwords into the
     * object. */
    GUEST_CALL(sub_811CC68);
    tracked = R0;
    u32 width = LD16(tracked + 6);
    ST16(R5 + 0x14, width);
    u32 scale = LD16(tracked + 8);
    ST16(R5 + 0x1A, scale);

    /* Shrink the sprite: scale parameters (0, scale >> 8, scale >> 8). */
    R0 = 0;
    R1 = scale >> 8;
    R2 = scale >> 8;
    GUEST_CALL(sprite_setScaleParameters);

    /* Start a 10-frame countdown in the object. */
    R0 = 10;
    GUEST_CALL(counter_addU32_r5Plus0C_811CC60);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811CC02 ── */
/* sub_811CC02:
 * Perform the object_SetStateHandler_811CC4E action, then report success (r0 = 1).
 * r1 and the carry flag are whatever object_SetStateHandler_811CC4E left behind. */
Next sub_811CC02(void) {
    GUEST_CALL(object_SetStateHandler_811CC4E);

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── battle_initObjectSpriteAndStartAnimation_811CC0C ── */
/* battle_initObjectSpriteAndStartAnimation_811CC0C: (re)initialize a battle sprite.
 * Asks sub_811CC68 for the object's sprite info, starts the animation
 * named by its halfword field at +2, loads the animation's frame data,
 * then hands the object state 4 to counter_addU32_r5Plus0C_811CC60.
 * Always returns r0 = 1 with NZ set from that final `mov r0, #1`.
 * r4 and r5 are never touched, so they pass through untouched. */
Next battle_initObjectSpriteAndStartAnimation_811CC0C(void) {
    /* Which animation to play: halfword at offset 2 of the sprite info. */
    GUEST_CALL(sub_811CC68);
    R0 = LD16(R0 + 2);
    GUEST_CALL(sprite_setAnimation);   /* (u8 animation) */
    GUEST_CALL(sprite_loadAnimationData);

    R0 = 4;
    GUEST_CALL(counter_addU32_r5Plus0C_811CC60);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811CC26 ── */
/* sub_811CC26(r5 = pointer to a status byte):
 * Sets bit 1 of the status byte at r5 (marking it as "active"/processed),
 * invokes the counter_addU32_r5Plus0C_811CC60 helper with r0 = 2, and returns 1 to the caller.
 * r0 = 1, r1 = whatever the helper left, flags from "mov r0, #1". */
Next sub_811CC26(void) {
    ST8(R5, LD8(R5) | 2);   /* status |= 0x02 */

    R0 = 2;
    GUEST_CALL(counter_addU32_r5Plus0C_811CC60);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811CC3A ── */
/* sub_811CC3A(r5 = pointer to a status byte):
 * Clears bit 1 of the status byte at [r5], invokes counter_addU32_r5Plus0C_811CC60 with the
 * "command" value 2, and always returns 1 (flags set from the result). */
Next sub_811CC3A(void) {
    ST8(R5, LD8(R5) & ~2u);   /* clear the two's bit of [r5] */

    R0 = 2;                   /* argument for counter_addU32_r5Plus0C_811CC60 */
    GUEST_CALL(counter_addU32_r5Plus0C_811CC60);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_SetStateHandler_811CC4E ── */
/* object_SetStateHandler_811CC4E(r5 = object):
 * Resolve the object's current state handler. The byte at +0x04 is a state
 * index; use it to pick an entry from the handler table at off_811C9C0 and
 * store that pointer into the object at +0x0C.
 * Outputs: r0 = selected handler pointer, r1 = handler table base,
 * c = carry out of the index shift (always clear for valid state IDs). */
Next object_SetStateHandler_811CC4E(void) {
    u32 stateIndex = LD8(R5 + 4);

    R1 = A_off_811C9C0;
    R0 = stateIndex << 2;
    cpu.c = (stateIndex >> 30) & 1; /* carry as set by LSL r0, r0, #2 */

    R0 = LD32(R1 + R0);
    ST32(R5 + 0xC, R0);

    RETURN_TO_CALLER();
}

/* ── counter_addU32_r5Plus0C_811CC60 ── */
/* counter_addU32_r5Plus0C_811CC60(r0 = amount, r5 = struct):
 * Adds r0 to the 32-bit counter at struct+0xC and stores the result back.
 * Leaves r1 = the old counter value, r0 = the new counter value, and
 * c/v (plus nz) set as by the ADDS r0, r0, r1. */
Next counter_addU32_r5Plus0C_811CC60(void) {
    u32 amount = R0;
    u32 old_value = LD32(R5 + 0xC);
    u32 new_value = amount + old_value;

    R1 = old_value;
    R0 = new_value;
    ST32(R5 + 0xC, new_value);

    /* Flags come from the actual addition performed: amount + old_value. */
    SET_FLAGS_ADD(amount, old_value);

    RETURN_TO_CALLER();
}

/* ── sub_811CC68 ── */
/* sub_811CC68: return the value at [r5 + 0xC] in r0.
 * A simple field getter: r5 points to some struct, r0 gets its 0xC field. */
Next sub_811CC68(void) {
    R0 = LD32(R5 + 0xC);
    RETURN_TO_CALLER();
}

/* ── TestPETMenuDataFlag ── */
/* TestPETMenuDataFlag(r0 = flag mask):
 * Tests the PET menu flags byte (eFlags200DF25) against the mask in r0 and
 * leaves the Z/N flags set from (flags & mask), so callers can branch on it.
 * Returns: r1 = flags byte, r2 = ePETMenuData address, nz from the test. */
Next TestPETMenuDataFlag(void) {
    u32 flags = LD8(A_ePETMenuData + 5); /* (eFlags200DF25 - 0x200df20) */

    R1 = flags;
    R2 = A_ePETMenuData;
    SET_FLAGS_NZ(flags & R0);
    RETURN_TO_CALLER();
}

/* ── SetPETMenuDataFlag ── */
/* SetPETMenuDataFlag(r0 = flag bits to set):
 * OR the given bits into the PET menu's data flags byte at ePETMenuData+5
 * (eFlags200DF25 - 0x200df20). */
Next SetPETMenuDataFlag(void) {
    ST8(A_ePETMenuData + 5, LD8(A_ePETMenuData + 5) | R0);
    RETURN_TO_CALLER();
}

/* ── ClearPETMenuDataFlag ── */
/* ClearPETMenuDataFlag(r0 = flag bit mask):
 * Clears the given bit(s) of the PET menu data flags byte (eFlags200DF25).
 * Returns the old flags byte in r1 and the ePETMenuData pointer in r2. */
Next ClearPETMenuDataFlag(void) {
    u32 flags = LD8(A_ePETMenuData + 5);

    R1 = flags & ~R0;
    ST8(A_ePETMenuData + 5, R1);
    R2 = A_ePETMenuData;

    RETURN_TO_CALLER();
}

/* ── pet_ClearMenuData_811EC00 ── */
/* pet_ClearMenuData_811EC00: clear the PET menu data struct.
 * Zero-fills the 0x18-byte ePETMenuData block and leaves r4 pointing at it. */
Next pet_ClearMenuData_811EC00(void) {
    R0 = A_ePETMenuData;
    R4 = R0;
    R1 = 0x18;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── pet_initMenu_811EC10 ── */
/* pet_initMenu_811EC10 (r10 = toolkit): (re)initialise the PET menu screen.
 *
 * Zeroes the submenu's 0x80-byte work buffer and the PET menu state
 * struct, marks the menu data as active, seeds the scroll/cursor
 * position with -32, copies the two bytes describing the first menu
 * entry, and finally refreshes the menu's resources. */
Next pet_initMenu_811EC10(void) {
    u32 savedR4 = R4;

    /* clear the submenu work buffer owned by the toolkit */
    R0 = LD32(R10 + oToolkit_SubmenuPtr);
    R1 = 0x80;
    GUEST_CALL(ZeroFillByWord);

    /* clear the PET menu state and flag it as in use */
    R4 = A_ePETMenuData;
    R0 = R4;
    R1 = 0x18;
    GUEST_CALL(ZeroFillByWord);

    R0 = 1;
    GUEST_CALL(SetPETMenuDataFlag);

    /* starting scroll/cursor offset: -32 */
    ST16(A_ePETMenuData + 0x10, 0xFFE0);

    /* look up the first menu entry's two bytes and store them */
    R0 = 0;
    R1 = A_byte_811EBDA;
    GUEST_CALL(sub_8120CC8);
    ST8(A_ePETMenuData + 0xA, R0);
    ST8(A_ePETMenuData + 0xB, R1);

    GUEST_CALL(sub_8046664);

    /* the entry-side r4 was only scratch; the original pops it back */
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_811EC50 ── */
/* sub_811EC50: run the full batch of per-frame updates for this subsystem.
 * Calls the five update steps in order; register and flag state on return
 * is whatever drawCurrentMapName_811F08C (the last callee) leaves behind. */
Next sub_811EC50(void) {
    GUEST_CALL(queueGFXTransfersFromTable_811EC68);
    GUEST_CALL(sub_811EFB8);
    GUEST_CALL(updateMapNameDisplay_811F014);
    GUEST_CALL(drawMapNamesScreen_811F050);
    GUEST_CALL(drawCurrentMapName_811F08C);
    RETURN_TO_CALLER();
}

/* ── queueGFXTransfersFromTable_811EC68 ── */
/* Build a list of GFX resource records into the staging buffer at unk_20096E0 and queue
 * them for DMA. The source is a table of triples whose first word is run through
 * getKeyedTableEntry_811FB84 to produce the "packed" header; then the trailing table at off_811ECB4 is
 * copied verbatim until a NULL terminator. Finally QueueGFXTransfersInList is called. */

/* register form: calls getKeyedTableEntry_811FB84 outside this file, which has no typed C interface yet */
static u32 derive_packed_size(u32 src, u32 size) {
    R0 = src;
    R1 = size;
    GUEST_CALL(getKeyedTableEntry_811FB84);
    return R0;
}

/* register form: calls QueueGFXTransfersInList outside this file, which has no typed C interface yet */
static void queue_gfx_transfers_in_list(u32 *dataRefs) {
    R0 = (u32)dataRefs;
    GUEST_CALL(QueueGFXTransfersInList);
}

static void build_and_queue_gfx_list(void) {
    /* Source records live at the table whose pointer is stored at A_off_811ECFC.
     * Each record is 4 words: { src, size, dest, size }. We produce a 4-word staging
     * entry per record: { derive(src,size), dest, size, src }. The loop runs while
     * srcOff < 0x20 (i.e. exactly two records). */
    u32 srcBase = A_off_811ECFC;   /* r5 in the first loop */
    u32 staging = A_unk_20096E0;   /* r4 (output buffer) */
    u32 srcOff = 0;                /* r7 in the first loop */
    u32 dstOff = 0;                /* r6 */

    while (srcOff < 0x20) {
        u32 src = LD32(srcBase + srcOff);
        srcOff += 4;
        u32 size = LD32(srcBase + srcOff);
        srcOff += 4;

        ST32(staging + dstOff, derive_packed_size(src, size));
        dstOff += 4;

        /* Copy the remaining two words (dest, src) verbatim. */
        for (u32 i = 0; i < 2; i++) {
            ST32(staging + dstOff, LD32(srcBase + srcOff));
            dstOff += 4;
            srcOff += 4;
        }
    }

    /* Append the trailing table at A_off_811ECB4 until a NULL word is seen. */
    {
        u32 tailBase = A_off_811ECB4;
        u32 tailOff = 0;
        u32 word;
        do {
            word = LD32(tailBase + tailOff);
            ST32(staging + dstOff, word);
            tailOff += 4;
            dstOff += 4;
        } while (word != 0);
    }

    /* Kick off the DMA transfers described by the staging buffer. */
    queue_gfx_transfers_in_list((u32 *)staging);
}

Next queueGFXTransfersFromTable_811EC68(void) {
    build_and_queue_gfx_list();
    /* The last flag-setting instruction in the original is `tst r0, r0` with r0 == 0
     * (the NULL terminator), so Z is set. The lifter lists nz as clobbered and the
     * required outputs are c/v, which the body never modifies; we still set nz to
     * match the original tail's TST behaviour. */
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_811ED58 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_LoadGFXAnim_result;
/* register form: generated wrapper: LoadGFXAnim has no typed C interface yet */
static inline call_LoadGFXAnim_result call_LoadGFXAnim_sub_811ED58(u32 r0) {
    R0 = r0;
    GUEST_CALL(LoadGFXAnim);
    return (call_LoadGFXAnim_result){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r4, r5, r6, r7; } call_sub_80465A0_result;
/* register form: generated wrapper: sub_80465A0 has no typed C interface yet */
static inline call_sub_80465A0_result call_sub_80465A0(u32 r0) {
    R0 = r0;
    GUEST_CALL(sub_80465A0);
    return (call_sub_80465A0_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r5, r7; } call_sub_811F1B8_result;
/* register form: generated wrapper: sub_811F1B8 has no typed C interface yet */
static inline call_sub_811F1B8_result call_sub_811F1B8_sub_811ED58(void) {
    GUEST_CALL(sub_811F1B8);
    return (call_sub_811F1B8_result){ R5, R7 };
}

typedef struct { u32 r4, r5, r6, r7; } call_replaySpriteFrames_811F24C_result;
/* register form: generated wrapper: replaySpriteFrames_811F24C has no typed C interface yet */
static inline call_replaySpriteFrames_811F24C_result call_replaySpriteFrames_811F24C(u32 r5) {
    R5 = r5;
    GUEST_CALL(replaySpriteFrames_811F24C);
    return (call_replaySpriteFrames_811F24C_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r5, r7; } call_sub_811F0C8_result;
/* register form: generated wrapper: sub_811F0C8 has no typed C interface yet */
static inline call_sub_811F0C8_result call_sub_811F0C8_sub_811ED58(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_811F0C8);
    return (call_sub_811F0C8_result){ R5, R7 };
}

/* register form: generated wrapper: petCommSaveInit_811F11C has no typed C interface yet */
static inline void call_petCommSaveInit_811F11C(u32 r5) {
    R5 = r5;
    GUEST_CALL(petCommSaveInit_811F11C);
}

typedef struct { u32 r4, r5, r6, r7; } call_CopyBackgroundTiles_PET_811EF70_result;
/* register form: generated wrapper: CopyBackgroundTiles_PET_811EF70 has no typed C interface yet */
static inline call_CopyBackgroundTiles_PET_811EF70_result call_CopyBackgroundTiles_PET_811EF70_sub_811ED58(u32 r5) {
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles_PET_811EF70);
    return (call_CopyBackgroundTiles_PET_811EF70_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r4, r5, r6, r7; } call_CopyBackgroundTiles_onNaviSelect_811F54C_result;
/* register form: generated wrapper: CopyBackgroundTiles_onNaviSelect_811F54C has no typed C interface yet */
static inline call_CopyBackgroundTiles_onNaviSelect_811F54C_result call_CopyBackgroundTiles_onNaviSelect_811F54C_sub_811ED58(u32 r5) {
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles_onNaviSelect_811F54C);
    return (call_CopyBackgroundTiles_onNaviSelect_811F54C_result){ R4, R5, R6, R7 };
}

/* sub_811ED58 (0x0811ED58) THUMB – PET-window animation tick
 * Advances the signed timer at state+0x10 by 4; on overflow reloads GFX
 * scripts, resets bookkeeping bytes, replays sprite frames, then always
 * runs the comm/tile-update tail. Returns the output registers. */

typedef struct {
    u32 r5, r7, r8, r9, r10, r11, r12;
} pet_tick_result;

static pet_tick_result tick_pet_window(u32 state, u32 pet_comm,
                                       u32 r7, u32 r8, u32 r9, u32 r11, u32 r12) {
    /* Advance timer and write back (unsigned halfword). */
    s16 timer = (s16)LDS16(state + 0x10) + 4;
    ST16(state + 0x10, (u16)timer);

    /* Track evolving register values through callees. */
    u32 cur_r5 = state;
    u32 cur_r7 = r7, cur_r8 = r8, cur_r9 = r9, cur_r11 = r11, cur_r12 = r12;

    if (timer >= 0) {
        /* Load first GFX-anim script; modifies r1–r12 (including r5). */
        call_LoadGFXAnim_result g1 = call_LoadGFXAnim_sub_811ED58(A_byte_811EA68);
        cur_r5 = g1.r5;
        cur_r7 = g1.r7; cur_r8 = g1.r8; cur_r9 = g1.r9;
        cur_r11 = g1.r11; cur_r12 = g1.r12;

        /* Load second GFX-anim script; overwrites r1–r12 again. */
        call_LoadGFXAnim_result g2 = call_LoadGFXAnim_sub_811ED58(A_byte_811EAB8);
        cur_r5 = g2.r5;
        cur_r7 = g2.r7; cur_r8 = g2.r8; cur_r9 = g2.r9;
        cur_r11 = g2.r11; cur_r12 = g2.r12;

        /* Reset animation bookkeeping (stores at current r5). */
        ST16(cur_r5 + 0x10, 0);
        ST8(cur_r5 + 9, 0);
        ST8(cur_r5 + 0xE, 0);
        ST8(cur_r5 + 0xD, 2);
        ST8(cur_r5 + 0x0, 4);

        /* Initialise data block (modifies r4–r7). */
        call_sub_80465A0_result a0 = call_sub_80465A0(A_byte_811EB78);
        cur_r7 = a0.r7;

        /* Update PET display state (modifies r5, r7). */
        call_sub_811F1B8_result fb = call_sub_811F1B8_sub_811ED58();
        cur_r5 = fb.r5;
        cur_r7 = fb.r7;

        /* Replay animation sprite frames (modifies r4–r7, takes r5). */
        call_replaySpriteFrames_811F24C_result rp = call_replaySpriteFrames_811F24C(cur_r5);
        cur_r5 = rp.r5;
        cur_r7 = rp.r7;
    }

    /* Common tail: always executed. */

    /* Update PET communication state (modifies r5, r7). */
    call_sub_811F0C8_result f0 = call_sub_811F0C8_sub_811ED58(cur_r5);
    cur_r5 = f0.r5;
    cur_r7 = f0.r7;

    /* Initialise PET communication save buffer. */
    call_petCommSaveInit_811F11C(cur_r5);

    /* Copy PET background tiles (modifies r4–r7). */
    call_CopyBackgroundTiles_PET_811EF70_result bg = call_CopyBackgroundTiles_PET_811EF70_sub_811ED58(cur_r5);
    cur_r5 = bg.r5;
    cur_r7 = bg.r7;

    /* If timer is non-negative, also copy tiles for the current Navi. */
    s16 final_timer = LDS16(cur_r5 + 0x10);
    if (final_timer >= 0) {
        call_CopyBackgroundTiles_onNaviSelect_811F54C_result navi =
            call_CopyBackgroundTiles_onNaviSelect_811F54C_sub_811ED58(cur_r5);
        cur_r5 = navi.r5;
        cur_r7 = navi.r7;
    }

    pet_tick_result out = { cur_r5, cur_r7, cur_r8, cur_r9, pet_comm, cur_r11, cur_r12 };
    return out;
}

/* register form: r7/r8/r9/r10/r11/r12 are pass-through outputs that the
 * lifter did not classify as inputs, so the adapter must read them from
 * the global registers to supply the typed function. */
Next sub_811ED58(void) {
    pet_tick_result out = tick_pet_window(R5, R10, R7, R8, R9, R11, R12);
    R5 = out.r5;
    R7 = out.r7;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── battle_state_tick_811EE98 ── */
/* Floor value for the battle countdown (loaded as a 32-bit constant 0xFFFFFFE0 = -32). */
#define COUNTDOWN_FLOOR (-32)

/* register form: calls IsGFXAnimActive outside this file, which has no typed C interface yet */
static bool gfx_anim_active(u32 anim_id) {
    R0 = anim_id;
    GUEST_CALL(IsGFXAnimActive);
    return !ZERO_FLAG();  /* Z set => anim not active */
}

/* register form: calls sub_811F0C8 outside this file, which has no typed C interface yet */
static void call_sub_811F0C8_sub_811EE98(void) {
    GUEST_CALL(sub_811F0C8);
}

/* register form: calls petCommSaveInit_811F11C outside this file, which has no typed C interface yet */
static void call_sub_811F11C_sub_811EE98(void) {
    GUEST_CALL(petCommSaveInit_811F11C);
}

/* register form: calls CopyBackgroundTiles_PET_811EF70 outside this file, which has no typed C interface yet */
static void call_copy_bg_pet(void) {
    GUEST_CALL(CopyBackgroundTiles_PET_811EF70);
}

/* register form: calls CopyBackgroundTiles_onNaviSelect_811F54C outside this file, which has no typed C interface yet */
static void call_copy_bg_navi_select(void) {
    GUEST_CALL(CopyBackgroundTiles_onNaviSelect_811F54C);
}

/* Decrement the battle countdown by 4 each tick.  If it reaches the floor (-32),
 * clamp it and—provided the PET animation (0x10) is no longer playing—flag the
 * state byte at [state] = 0xC (timeout / defeat).  Then refresh background tiles. */
static void process_battle_state_tick(u32 state) {
    s32 countdown = LDS16(state + 0x10) - 4;
    ST16(state + 0x10, countdown);

    if (countdown <= COUNTDOWN_FLOOR) {
        ST16(state + 0x10, (u16)COUNTDOWN_FLOOR);
        if (!gfx_anim_active(0x10)) {
            ST8(state, 0xC);
        }
    }

    call_sub_811F0C8_sub_811EE98();
    call_sub_811F11C_sub_811EE98();
    call_copy_bg_pet();
    call_copy_bg_navi_select();
}

/* battle_state_tick_811EE98(r5 = battle state ptr) -> void
 * Output r0-r3 / nz / c / v are left by CopyBackgroundTiles_onNaviSelect via GUEST_CALL. */
Next battle_state_tick_811EE98(void) {
    process_battle_state_tick(R5);
    RETURN_TO_CALLER();
}

/* ── naviSelect_SetupBackgrounds_811EF0C ── */
/* naviSelect_SetupBackgrounds_811EF0C: rebuild the PET/cyberworld background for the navi select screen.
 * Runs the individual background setup passes, then copies the tile graphics
 * for the PET display and the navi select screen. No arguments, no results;
 * r0-r3/flags are whatever the last callee leaves behind. */
Next naviSelect_SetupBackgrounds_811EF0C(void) {
    GUEST_CALL(sub_811F1B8);
    GUEST_CALL(sub_811F214);
    GUEST_CALL(sub_811F0C8);
    GUEST_CALL(petCommSaveInit_811F11C);
    GUEST_CALL(CopyBackgroundTiles_PET_811EF70);
    GUEST_CALL(CopyBackgroundTiles_onNaviSelect_811F54C);
    RETURN_TO_CALLER();
}

/* ── initPetSelectScreen_811EF28 ── */
/* register form: calls sub_8046664 outside this file, which has no typed C interface yet */
static void call_sub_8046664_sub_811EF28(void) {
    GUEST_CALL(sub_8046664);
}

/* register form: calls sub_80465A0 outside this file, which has no typed C interface yet */
static void call_sub_80465A0_sub_811EF28(void *a1) {
    R0 = (u32)a1;
    GUEST_CALL(sub_80465A0);
}

/* register form: calls LoadGFXAnim outside this file, which has no typed C interface yet */
static void call_LoadGFXAnim(void *script) {
    R0 = (u32)script;
    GUEST_CALL(LoadGFXAnim);
}

/* register form: calls sub_811F1B8 outside this file, which has no typed C interface yet */
static void call_sub_811F1B8(void) {
    GUEST_CALL(sub_811F1B8);
}

/* register form: calls sub_811F214 outside this file, which has no typed C interface yet */
static void call_sub_811F214(void) {
    GUEST_CALL(sub_811F214);
}

/* register form: calls sub_811F0C8 outside this file, which has no typed C interface yet */
static void call_sub_811F0C8(void) {
    GUEST_CALL(sub_811F0C8);
}

/* register form: calls petCommSaveInit_811F11C outside this file, which has no typed C interface yet */
static void call_sub_811F11C(void) {
    GUEST_CALL(petCommSaveInit_811F11C);
}

/* register form: calls CopyBackgroundTiles_PET_811EF70 outside this file, which has no typed C interface yet */
static void call_CopyBackgroundTiles_PET_811EF70(void) {
    GUEST_CALL(CopyBackgroundTiles_PET_811EF70);
}

/* register form: calls CopyBackgroundTiles_onNaviSelect_811F54C outside this file, which has no typed C interface yet */
static void call_CopyBackgroundTiles_onNaviSelect_811F54C(void) {
    GUEST_CALL(CopyBackgroundTiles_onNaviSelect_811F54C);
}

/* Initialize the PET/Navi-select screen background. */
static void init_pet_select_screen(void *state) {
    u8 *state8 = (u8 *)state;
    u16 *state16 = (u16 *)state;

    state16[0x10 / 2] = 0;
    call_sub_8046664_sub_811EF28();
    call_sub_80465A0_sub_811EF28((void *)A_byte_811EBA4);
    state8[0] = 4;
    call_LoadGFXAnim((void *)A_byte_811EAD4);
    call_LoadGFXAnim((void *)A_byte_811EAEC);
    call_sub_811F1B8();
    call_sub_811F214();
    call_sub_811F0C8();
    call_sub_811F11C();
    call_CopyBackgroundTiles_PET_811EF70();
    call_CopyBackgroundTiles_onNaviSelect_811F54C();
}

/* initPetSelectScreen_811EF28(r5 = screen state) -> void */
Next initPetSelectScreen_811EF28(void) {
    init_pet_select_screen((void *)R5);
    RETURN_TO_CALLER();
}

/* ── updateMapNameDisplay_811F014 ── */
/* updateMapNameDisplay_811F014: refresh the map-name display.
 *
 * 1. protectedZennyOrZero_803D06C yields a pointer (the current map's name/script data) that
 *    the scene state keeps at +8.
 * 2. The map name table text is rendered as graphics into the tile block at
 *    0x6015240 via the scratch text buffer unk_200C0E0.
 * 3. sprite_repackAndQueueTiles_811F5DC copies the rendered name into the byte_200CF20 buffer. */
Next updateMapNameDisplay_811F014(void) {
    /* r4-r7 are used as call arguments below but must be preserved for the
     * caller, so keep the entry values aside and restore them before returning. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Current map name pointer -> scene state +8. */
    GUEST_CALL(protectedZennyOrZero_803D06C);
    u32 sceneState = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(sceneState + 8, R0);

    /* Render the map name text (script 0xF1, palette 7, one line) into VRAM. */
    R0 = A_TextScriptMapNames;      /* TextScriptMapNames */
    R1 = 0xF1;
    R2 = A_unk_200C0E0;
    R3 = 0;
    R4 = 7;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* Copy the rendered name tiles into the map-name buffer byte_200CF20. */
    R0 = A_unk_200C0E0;
    R1 = A_byte_200CF20;
    R2 = 0x6015240;
    R3 = 7;
    GUEST_CALL(sprite_repackAndQueueTiles_811F5DC);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── drawMapNamesScreen_811F050 ── */
/* drawMapNamesScreen_811F050: redraw the map-name text screen.
 * Calls sub_803D0F4 and stores its result into the toolkit's unk_200A220
 * struct, renders the "map names" text script into the unk_200C2A0 buffer,
 * then uploads the rendered tiles/tilemap to VRAM at 0x6015400. */
Next drawMapNamesScreen_811F050(void) {
    /* The original saves/restores r4-r7 around the calls; our callees
     * also expect the row/column arguments in those registers. */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(sub_803D0F4);

    u32 unk200a220 = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(unk200a220 + 8, R0);

    /* Render text script #0xF2 (map names) into the unk_200C2A0 buffer:
     * 6 rows starting at row 0, using the font data at dword_86A5D60. */
    R0 = A_TextScriptMapNames;    /* TextScriptMapNames */
    R1 = 0xF2;
    R2 = A_unk_200C2A0;
    R3 = 0;
    R4 = 6;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* Upload the rendered text from unk_200C2A0 to the VRAM tile blocks
     * at 0x6015400, with byte_200D220 as the tilemap destination info. */
    R0 = A_unk_200C2A0;
    R1 = A_byte_200D220;
    R2 = 0x6015400;
    R3 = 6;
    GUEST_CALL(sprite_repackAndQueueTiles_811F5DC);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── drawCurrentMapName_811F08C ── */
/* drawCurrentMapName_811F08C: redraw the name of the current map in the map-name display.
 *
 * getCurrentMapBgm_811F290 resolves the text-script entry for the map the player is
 * currently on; renderTextGfx_8045F8C draws that name into the text work
 * buffer at byte_200C620; sprite_repackAndQueueTiles_811F5DC then blits the rendered tiles into
 * the map-name VRAM slot at 0x6015780. */
Next drawCurrentMapName_811F08C(void) {
    /* The original pushes/pops r4-r7 around the calls, so they must come
     * back out of this routine exactly as they came in. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Look up which map-name string to show. */
    GUEST_CALL(getCurrentMapBgm_811F290);
    u32 mapNameString = R0;

    /* renderTextGfx_8045F8C(stringTable, stringIndex, workBuffer,
     *                        0, 12, 1, fontTable, 0):
     * render the map name text into the work buffer. */
    R0 = A_TextScriptMapNames;       /* TextScriptMapNames string table */
    R1 = mapNameString;
    R2 = A_byte_200C620;       /* rendered-text work buffer */
    R3 = 0;
    R4 = 0xC;                  /* render width */
    R5 = 1;
    R6 = A_dword_86A5D60;      /* font/metadata table */
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* sprite_repackAndQueueTiles_811F5DC(source, sourceMap, vramDest, tileCount):
     * upload the rendered name tiles to VRAM. */
    R0 = A_byte_200C620;
    R1 = A_byte_200C920;
    R2 = 0x6015780;
    R3 = 0xC;
    GUEST_CALL(sprite_repackAndQueueTiles_811F5DC);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_811F0C8 ── */
/* register form: calls sub_811F184 outside this file, which has no typed C interface yet.
 * sub_811F184(r0 = addr_of_word_3001B2C, r1 = ptr off_811F0F8,
 *             r2 = byte from r5+6, r3 = byte from r5+7) -> r0, r1 */
static void call_sub_811F184(u8 out[2], u8 in6, u8 in7) {
    R0 = A_word_3001B2C;
    R1 = A_off_811F0F8;
    R2 = in6;
    R3 = in7;
    GUEST_CALL(sub_811F184);
    out[0] = (u8)R0;   /* r5+7 (was r0 in the original strb order) */
    out[1] = (u8)R1;   /* r5+6 */
}

/* sub_811F0C8: dispatch on the entity sub-state at r5+4.
 * If sub-state == 8, call sub_811F184 with the bytes at r5+6 / r5+7 and store the
 * results back. Otherwise read the halfword at offset +4 from the constant pool
 * entry pointed to by off_811F0F8[0] and store it into word_3001B2C. */
static void sub_811F0C8_impl(u32 r5, u8 *out_bytes) {
    u8 sub_state = LD8(r5 + 4);
    if (sub_state == 8) {
        u8 in6 = LD8(r5 + 6);
        u8 in7 = LD8(r5 + 7);
        call_sub_811F184(out_bytes, in6, in7);
        ST8(r5 + 7, out_bytes[0]);
        ST8(r5 + 6, out_bytes[1]);
    } else {
        u32 table = LD32(A_off_811F0F8);
        u16 value = LD16(table + 4);
        ST16(A_word_3001B2C, value);
    }
}

Next sub_811F0C8(void) {
    u8 out_bytes[2];
    sub_811F0C8_impl((u32)R5, out_bytes);
    RETURN_TO_CALLER();
}

/* ── petCommSaveInit_811F11C ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set(u16 flag) {
    R0 = flag;
    R1 = 6;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* Z set if the flag is clear */
}

/* Pet-network save handler. Walks the 8-slot pointer table, writes a default
 * halfword to each slot, stamps a per-state override (unless state byte == 8),
 * and applies the EVENT_PET_COMM_SAVE_DISABLED patch when set. */
static void pet_comm_save_init(u32 state_addr) {
    /* Default: write 0x537F to the slot pointed to by each table entry. */
    for (u32 i = 0; i < 0x20; i += 4) {
        ST16(LD32(A_byte_811F158 + i), 0x537F);
    }
    u32 state_byte = LD8(state_addr + 4);
    if (state_byte != 8) {
        ST16(LD32(A_byte_811F158 + (state_byte << 2)), 0x7BFE);
    }
    if (event_flag_set(0x17)) {
        ST16(LD32(A_byte_811F158 + 0x1C), 0x5A7A);
        ST16(LD32(A_byte_811F158 + 0x18), 0x5A7A);
    }
}

Next petCommSaveInit_811F11C(void) {
    /* Preserve the original's r4 (saved on stack) and call the typed helper.
     * After the call, R0..R3/R4 hold the same values as on entry, matching the
     * original which had r4 = ptr_tbl on entry, used it locally, and then
     * restored the saved r4 on pop. The last flag-setting instruction is
     * `cmp r0, #0x20` from the loop; we mirror that with SET_FLAGS_CMP
     * against a representative value. */
    pet_comm_save_init(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811F1B8 ── */
struct ParamPair {
    u32 a;
    u16 b;
};

/* register form: calls sub_802FE28 outside this file, which has no typed C interface yet */
static void call_sub_802FE28(u32 a, u16 b) {
    R0 = a;
    R1 = b;
    R2 = 0;
    R3 = 1;
    GUEST_CALL(sub_802FE28);
}

/* Walk a block of (u32, u16) pairs laid out consecutively. The original increments
 * a counter from 0, calls sub_802FE28 for the current pair, advances the cursor by
 * 8 bytes, increments the counter, and loops while the counter is less than 2
 * (so it iterates twice, processing pairs at offsets 0 and 8). */
static void load_pairs(u32 base) {
    u32 cursor = base;
    u32 count = 0;
    while (count < 2) {
        u32 a = LD32(cursor);
        u16 b = (u16)LD32(cursor + 4);
        call_sub_802FE28(a, b);
        cursor += 8;
        count += 1;
    }
}

/* Initialize two sets of (u32, u16) parameter pairs by passing each through sub_802FE28. */
Next sub_811F1B8(void) {
    load_pairs(A_dword_811F1F0);
    load_pairs(A_dword_811F204);
    RETURN_TO_CALLER();
}

/* ── sub_811F214 ── */
/* sub_811F214: walk the three-entry table at dword_811F234 and hand each
 * (word0, word1) pair to sub_802FE28 as (a1, a2, 0, 1).
 *
 * The original keeps the table cursor in r7 and the entry index in r6 and
 * calls sub_802FE28 with those registers live, so the loop is driven through
 * the guest registers as well; r4-r7 are saved and restored exactly like the
 * original's push/pop. */
Next sub_811F214(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R7 = A_dword_811F234;
    for (R6 = 0; (s32)R6 < 3; R6++) {
        R0 = LD32(R7);       /* entry->word0 */
        R1 = LD32(R7 + 4);   /* entry->word1 */
        R2 = 0;
        R3 = 1;
        GUEST_CALL(sub_802FE28);
        R7 += 8;             /* next table entry */
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── replaySpriteFrames_811F24C ── */
/* replaySpriteFrames_811F24C: tick a counter at r5+0xC (clamped to max 3) and replay the first
 * (count+1) sprite/animation pairs into sub_802FE28. Each pair is two u32 entries
 * from the table at dword_811F278 (8 bytes per entry). */
Next replaySpriteFrames_811F24C(void) {
    /* Bump the local tick counter, clamped so it never exceeds 3 (i.e. count 0..3). */
    u32 count = LD8(R5 + 0xC);
    count = count + 1;
    if ((s32)count <= (s32)3) {
        ST8(R5 + 0xC, count);
    }

    /* Walk the table, replaying each entry via sub_802FE28(lo, hi, 0, 1); the first
     * entry always (the original tests the count only after a call). */
    u32 tbl = A_dword_811F278;
    u32 idx = 0;
    do {
        R0 = LD32(tbl);
        R1 = LD32(tbl + 4);
        R2 = 0;
        R3 = 1;
        GUEST_CALL(sub_802FE28);

        tbl += 8;
        idx += 1;
    } while ((s32)idx < (s32)LD8(R5 + 0xC));

    RETURN_TO_CALLER();
}

/* ── getCurrentMapBgm_811F290 ── */
#define off_811F2B0 0x0811F2B0u

/* Return the BGM id for the current map (0xFF = no BGM assigned). */
static u32 current_map_bgm(u32 game_state_ptr) {
    u32 map_group  = LD8(game_state_ptr + oGameState_MapGroup);
    u32 map_number = LD8(game_state_ptr + oGameState_MapNumber);

    if (map_group >= 0x80) {
        map_group = map_group - 0x80 + 7;
    }

    u32 table_addr = LD32(off_811F2B0 + (map_group << 2));
    return LD8(table_addr + map_number);
}

/* register form: reads game state pointer from r10 via the toolkit */
static u32 game_state_from_toolkit(void) {
    u32 toolkit = R10;
    return LD32(toolkit + oToolkit_GameStatePtr);
}

Next getCurrentMapBgm_811F290(void) {
    R0 = current_map_bgm(game_state_from_toolkit());
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811F508 ── */
/* sub_811F508(r10 = ?, r5 = ?, lr):
 * Chip/custom-data handler invoked during a frame.
 * Reads r5->0x10 flag; if zero, dispatches to sub_811FAF4 with a packed arg,
 * decrements r5->0xB timer, and reloads it via sub_8120CC8 when it hits zero.
 *
 * Output: R7 (preserved register, untouched by this function).
 * Clobbers: R0, R1, R2, R3, LR. */
Next sub_811F508(void) {
    if (LD16(R5 + 0x10) != 0) {
        RETURN_TO_CALLER();
    }

    /* Build r0 = 0x00100000 | (halfword at r5+0x10, treated as zero-extended).
     * The original did: mov r0,#0x10; lsl r0,#0x10; ldrh r1,[r5,#0x10]; orr r0,r1 */
    u32 arg0 = 0x00100000u | (u32)LD16(R5 + 0x10);

    R0 = arg0;
    R1 = 0;                                /* dword_811F53C */
    R2 = LD8(R5 + 0xA);                    /* r5->byte_0A */
    R3 = (u32)A_byte_811EBD0;              /* off_811F540 */
    /* R4 set for callee; preserved across the call. */
    u32 saved_r4 = R4;
    R4 = 0x40000000u;                      /* dword_811F544 */

    GUEST_CALL(sub_811FAF4);
    /* sub_811FAF4 returns in R0. */
    u32 result = R0;
    R4 = saved_r4;

    /* Decrement r5->byte_0B; if non-zero we're done. */
    u8 timer = LD8(R5 + 0xB) - 1;
    ST8(R5 + 0xB, timer);
    if (timer != 0) {
        RETURN_TO_CALLER();
    }

    /* Timer expired: store result into r5->byte_0A, then reload timer. */
    ST8(R5 + 0xA, (u8)result);
    R1 = (u32)A_byte_811EBDA;              /* off_811F548 */
    GUEST_CALL(sub_8120CC8);
    ST8(R5 + 0xB, (u8)R1);

    RETURN_TO_CALLER();
}

/* ── CopyBackgroundTiles_onNaviSelect_811F54C ── */
/* Offsets into the navi-select background work struct (arrives in r5). */
#define BG_SCROLL_TIMER  0xD  /* u8: frames left until the background steps one row */
#define BG_ROW_COUNTER   0xE  /* u8: index of the 32x32 block row being copied */
#define BG_BASE_J        0x10 /* s16: base j coordinate of the copied region */

/* ROM pointer table off_811F5B8: one tile-id array per 32x32 block row. */
#define TILE_ID_TABLE A_off_811F5B8

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u32 get_cur_pet_navi(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet;
   its 5th/6th arguments travel in r4/r5, so the helper puts the caller's r4-r7 back afterwards */
static void copy_background_tiles_block(u32 j, u32 i, u32 tileBlock32x32, u32 tileIds,
                                        u32 keepR4, u32 keepR5, u32 keepR6, u32 keepR7) {
    R0 = j;
    R1 = i;
    R2 = tileBlock32x32;
    R3 = tileIds;
    R4 = 9; /* j_size */
    R5 = 2; /* i_size */
    GUEST_CALL(CopyBackgroundTiles);
    R4 = keepR4;
    R5 = keepR5;
    R6 = keepR6;
    R7 = keepR7;
}

/* Scroll the navi select screen's background up by one 32x32 block row:
 *  - decrement the scroll timer at +0xD; when it reaches zero, reload it with
 *    2 and step the row counter at +0xE (never letting it reach 8),
 *  - then copy every block row from the row counter down to 0, taking the
 *    tile ids from the table entry for that row; while a navi is selected on
 *    the PET, row 3 uses the extra (navi-specific) tile table instead.
 * Returns the counter value the final loop decrement consumed (always 0):
 * the original leaves that decrement's flags behind and r3 = value - 1,
 * while r0-r2 hold whatever CopyBackgroundTiles left in them. */
static u32 navi_select_scroll_background(u32 keepR4, u32 work, u32 keepR6, u32 keepR7) {
    u32 extraRowTable = 0;
    if (get_cur_pet_navi() != 0)
        extraRowTable = 1;

    u32 timer = LD8(work + BG_SCROLL_TIMER);
    timer -= 1;
    ST8(work + BG_SCROLL_TIMER, timer);
    if (timer == 0) {
        ST8(work + BG_SCROLL_TIMER, 2);
        u32 nextRow = LD8(work + BG_ROW_COUNTER) + 1;
        if ((s32)nextRow < 8)
            ST8(work + BG_ROW_COUNTER, nextRow);
    }

    s32 row = (s32)LD8(work + BG_ROW_COUNTER);
    while (row >= 0) {
        s32 baseJ = LDS16(work + BG_BASE_J);
        u32 tableIndex = (u32)row;
        if (row == 3 && extraRowTable != 0)
            tableIndex = 8;
        copy_background_tiles_block((u32)(baseJ + 4), 3 + 2 * (u32)row, 0,
                                    LD32(TILE_ID_TABLE + 4 * tableIndex),
                                    keepR4, work, keepR6, keepR7);
        row--;
    }

    return (u32)(row + 1);
}

/* CopyBackgroundTiles_onNaviSelect_811F54C(r4, r5 = work struct, r6, r7 preserved) */
Next CopyBackgroundTiles_onNaviSelect_811F54C(void) {
    u32 lastCounter = navi_select_scroll_background(R4, R5, R6, R7);
    R3 = lastCounter - 1;
    SET_FLAGS_CMP(lastCounter, 1);
    RETURN_TO_CALLER();
}

/* ── sprite_repackAndQueueTiles_811F5DC ── */
/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx_transfer(u32 r0_val, u32 r1_val, u32 r2_val) {
    R0 = r0_val;
    R1 = r1_val;
    R2 = r2_val;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* Decompose tile height into 4-row / 2-row / 1-row blocks, copying 8-word
 * rows from a strided source (0x20-byte gap between rows) into a packed
 * destination.  When all rows are done, queue the DMA for the original
 * dimensions. */
static void transfer_tiles(u32 src, u32 dst, u32 width, u32 height) {
    u32 height_left = height;
    u32 src_off     = 0;
    u32 dst_off     = 0;
    u32 src_base    = 0;

    while (1) {
        if ((s32)height_left >= 4) {
            /* 4-row block: 2 groups × 4 rows, source stride = 0x100 */
            height_left -= 4;
            for (int group = 0; group < 2; group++) {
                for (int row = 0; row < 4; row++) {
                    for (int i = 0; i < 8; i++) {
                        ST32(dst + dst_off, LD32(src + src_off));
                        dst_off += 4;
                        src_off += 4;
                    }
                    src_off += 0x20; /* inter-row gap in source */
                }
                src_off = src_base + 0x20; /* start next group at second row-set */
            }
            src_base += 0x100;
            src_off = src_base;
        } else if ((s32)height_left >= 2) {
            /* 2-row block: 2 groups × 2 rows, source stride = 0x80 */
            height_left -= 2;
            for (int group = 0; group < 2; group++) {
                for (int row = 0; row < 2; row++) {
                    for (int i = 0; i < 8; i++) {
                        ST32(dst + dst_off, LD32(src + src_off));
                        dst_off += 4;
                        src_off += 4;
                    }
                    src_off += 0x20;
                }
                src_off = src_base + 0x20;
            }
            src_base += 0x80;
            src_off = src_base;
        } else if ((s32)height_left >= 1) {
            /* Single row: 16 contiguous words, no source gap */
            height_left -= 1;
            for (int i = 0; i < 16; i++) {
                ST32(dst + dst_off, LD32(src + src_off));
                dst_off += 4;
                src_off += 4;
            }
            src_base += 0x40;
            src_off = src_base;
        } else {
            /* All rows processed – queue DMA with original parameters */
            queue_gfx_transfer(dst, width, height * 0x40);
            return;
        }
    }
}

/* sprite_repackAndQueueTiles_811F5DC(r0=src, r1=dst, r2=width, r3=height) -> c v r0 r1 r2 r3 */
Next sprite_repackAndQueueTiles_811F5DC(void) {
    transfer_tiles(R0, R1, R2, R3);
    RETURN_TO_CALLER();
}

/* ── pet_prepareMenuOnReturn_811F6C0 ── */
/* pet_prepareMenuOnReturn_811F6C0(r10 = toolkit):
 * Prepare the PET menu after coming back into it: wipe the submenu's
 * 0x80-byte work area, restore the menu cursor to the page remembered in
 * ePETMenuData, and record the current map group as the last one visited
 * (so the game later knows where the player came from).
 * r2/r3/r4/r5 and the V flag pass through unchanged. */
Next pet_prepareMenuOnReturn_811F6C0(void) {
    u32 toolkit = R10;
    u32 petMenuData = A_ePETMenuData;

    /* Clear the submenu work area; ZeroFillByWord wants a byte count. */
    u32 submenu = LD32(toolkit + oToolkit_SubmenuPtr);
    R0 = submenu;
    R1 = 0x80;
    GUEST_CALL(ZeroFillByWord);

    /* The saved menu selection is a page number; the submenu's first byte
     * is the byte offset of that page (selection * 4). The LSL #2 that
     * produces it also sets carry from bit 30 - always clear here, since
     * the selection is only a byte - and that carry is visible to callers. */
    u32 selection = LD8(petMenuData + 4);
    u32 cursorOffset = selection << 2;
    cpu.c = (selection >> 30) & 1;

    ST8(submenu, cursorOffset);

    /* Copy the current map group into the last-map-group field.
     * (The map group field sits at offset 4 in the game state; only the
     * LastMapGroup offset has a named macro in the disassembly.) */
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 mapGroup = LD16(gameState + 4);
    ST16(gameState + oGameState_LastMapGroup, mapGroup);

    RETURN_TO_CALLER();
}

/* ── sub_811F6E0 ── */
Next sub_811F6E0(void) {
    /* Open the PET submenu: clear its 0x80-byte buffer, seed default state,
     * and remember the current map group so it can be restored on close. */
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    u32 submenu = LD32(R10 + oToolkit_SubmenuPtr);
    u32 game_state_ptr_addr = R10 + oToolkit_GameStatePtr;

    R0 = submenu;
    R1 = 0x80;
    GUEST_CALL(ZeroFillByWord);

    R5 = A_ePETMenuData;

    ST8(submenu, 0x10);
    ST8(submenu + 0xD, 1);

    u32 game_state = LD32(game_state_ptr_addr);
    u16 map_group = LD16(game_state + oGameState_MapGroup);
    ST16(game_state + oGameState_LastMapGroup, map_group);

    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── pet_menuExitToMap_811F708 ── */
/* pet_menuExitToMap_811F708: begin the "exit to map" fade and reset the PET menu.
 * Runs the shared fade-entry helper first, sets the game state's map-enter
 * fade parameter to 8, then initializes the PET menu data block:
 *   +0 = 0x14 (menu screen id), +1 = 0 and +8 = 0 (cleared state),
 *   +9 = 3 (menu sub-state).
 * Returns with r0 = 3 and r1 = &ePETMenuData; flags reflect `mov r0, #3`. */
Next pet_menuExitToMap_811F708(void) {
    GUEST_CALL(next_exitToMap_811F728);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    ST8(gameState + oGameState_EnterMapFadeParam1, 8);

    u32 petMenu = A_ePETMenuData;
    ST8(petMenu,      0x14); /* current PET menu screen */
    ST8(petMenu + 1,  0);    /* cleared: cursor/selection state */
    ST8(petMenu + 8,  0);    /* cleared: sub-menu state */
    ST8(petMenu + 9,  3);    /* menu sub-state = 3 */

    R0 = 3;
    R1 = petMenu;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── next_exitToMap_811F728 ── */
/* next_exitToMap_811F728: leave the current game mode and return to the map.
 * Called with r10 = toolkit. Resets the main jumptable to state 4
 * (cbGameState_80050EC), tells the game state to run subsystem 0 (EnterMap),
 * flags the render info with 0x40, plays sound effect 6, refreshes the
 * chatbox, and clears event flag 0x40 in the chatbox event flags. */
Next next_exitToMap_811F728(void) {
    u32 toolkit = R10;

    /* switch the main jumptable to cbGameState_80050EC
     * (the index pointer is at offset 0 of the toolkit) */
    u32 jumptableIndexPtr = LD32(toolkit);
    ST8(jumptableIndexPtr, 4);

    /* have cbGameState_80050EC enter the map subsystem */
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    ST8(gameState, 0); /* oGameState_SubsystemIndex == 0 */

    /* store 0x40 into the render info header (RenderInfoPtr is at offset 8) */
    u32 renderInfo = LD32(toolkit + 8);
    ST16(renderInfo, 0x40);

    /* play sound effect 6 */
    R0 = 6;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);

    /* reset the chatbox state, then refresh the text window */
    GUEST_CALL(npc_clearAllBusyFlags_809F68C);
    GUEST_CALL(chatbox_8040818);

    /* clear chatbox event flag 0x40 */
    R0 = 0x40;
    GUEST_CALL(chatbox_clear_eFlags2009F38);

    RETURN_TO_CALLER();
}

/* ── SubMenuControl ── */
/* Screen ids of the pause ("sub") menu. The submenu state stores the id as a
 * byte offset into the screen handler jump table, so the ids step by 4; the
 * table's remaining slots (0x34..0x48) are NULL and are never selected. */
enum SubmenuScreen {
    SUBMENU_CHIP_FOLDER = 0x00, /* chip folder  (HandleChipFolderMenu8123434) */
    SUBMENU_SUB_CHIP    = 0x04, /* sub chips    (DispatchSubChipMenu8123F5C)  */
    SUBMENU_LIBRARY     = 0x08, /* library      (HandleLibraryMenu8124B3C)    */
    SUBMENU_MEGAMAN     = 0x0C, /* MegaMan page (HandleMegaManStatusMenu8126B4C) */
    SUBMENU_EMAIL       = 0x10, /* e-mail       (HandleEmailMenu81279F8)      */
    SUBMENU_KEY_ITEMS   = 0x14, /* key items    (HandleKeyItemMenu8128730)    */
    SUBMENU_COMM        = 0x18, /* comm menu    (HandleCommMenu81291E8)       */
    SUBMENU_SAVE        = 0x1C, /* save screen  (HandleSaveMenu8132B88)       */
    SUBMENU_UNK_20      = 0x20, /* unidentified (sub_8133200) */
    SUBMENU_UNK_24      = 0x24, /* unidentified (sub_81356D4) */
    SUBMENU_UNK_28      = 0x28, /* unidentified (sub_8136DE0) */
    SUBMENU_UNK_2C      = 0x2C, /* unidentified (sub_81378B4) */
    SUBMENU_UNK_30      = 0x30  /* unidentified (sub_8128CBC) */
};

/* register form: calls GetRNG outside this file, which has no typed C interface yet */
static void advance_rng(void) {
    GUEST_CALL(GetRNG); /* rolled value is discarded; the call just steps the RNG */
}

/* register form: dispatches to a submenu screen handler outside this file,
 * which has no typed C interface yet. The callee is chosen from a jump table
 * at runtime and expects the submenu state pointer in r5 plus two words from
 * SubMenuControl's caller in r2/r3 — a convention no typed C call can express.
 * The handlers are also free to clobber r4-r7: the original push {r4-r7,lr}s
 * them before the dispatch and pop {r4-r7,pc}s them at the end, so park the
 * caller's r4-r7 (and lr, as the push does) on the guest stack and restore
 * them afterwards. */
static void run_screen_handler(u8 screen, u32 submenu, u32 handler_arg2, u32 handler_arg3) {
    SP -= 20;
    ST32(SP,      R4);
    ST32(SP + 4,  R5);
    ST32(SP + 8,  R6);
    ST32(SP + 12, R7);
    ST32(SP + 16, LR);

    R1 = screen; /* screen id, as the table lookup leaves it for the callee */
    R2 = handler_arg2;
    R3 = handler_arg3;
    R5 = submenu;

    switch (screen) {
    case SUBMENU_CHIP_FOLDER: GUEST_CALL(HandleChipFolderMenu8123434); break;
    case SUBMENU_SUB_CHIP:    GUEST_CALL(DispatchSubChipMenu8123F5C); break;
    case SUBMENU_LIBRARY:     GUEST_CALL(HandleLibraryMenu8124B3C); break;
    case SUBMENU_MEGAMAN:     GUEST_CALL(HandleMegaManStatusMenu8126B4C); break;
    case SUBMENU_EMAIL:       GUEST_CALL(HandleEmailMenu81279F8); break;
    case SUBMENU_KEY_ITEMS:   GUEST_CALL(HandleKeyItemMenu8128730); break;
    case SUBMENU_COMM:        GUEST_CALL(HandleCommMenu81291E8); break;
    case SUBMENU_SAVE:        GUEST_CALL(HandleSaveMenu8132B88); break;
    case SUBMENU_UNK_20:      GUEST_CALL(sub_8133200); break;
    case SUBMENU_UNK_24:      GUEST_CALL(sub_81356D4); break;
    case SUBMENU_UNK_28:      GUEST_CALL(sub_8136DE0); break;
    case SUBMENU_UNK_2C:      GUEST_CALL(sub_81378B4); break;
    case SUBMENU_UNK_30:      GUEST_CALL(sub_8128CBC); break;
    default: break; /* remaining table slots are NULL; never selected in-game */
    }

    R4 = LD32(SP);
    R5 = LD32(SP + 4);
    R6 = LD32(SP + 8);
    R7 = LD32(SP + 12);
    SP += 20; /* the original pops the saved lr straight into pc */
}

/* One frame of the pause menu: run the handler of the screen named by the
 * submenu state, then step the RNG — unless the comm screen sits in one of two
 * states (its second screen id reads "comm" or "save") that leave the RNG
 * stream alone. handler_arg2/3 are the words the screen handlers read from
 * r2/r3; they come straight from SubMenuControl's caller and are handed
 * through untouched. */
static void update_submenu(u32 toolkit, u32 handler_arg2, u32 handler_arg3) {
    u32 submenu = LD32(toolkit + oToolkit_SubmenuPtr); /* submenu state */

    run_screen_handler(LD8(submenu), submenu, handler_arg2, handler_arg3);

    /* The handler may have switched screens mid-frame, so look again. */
    if (LD8(submenu) == SUBMENU_COMM) {
        u8 second = LD8(submenu + 1); /* second screen id, same encoding */
        if (second == SUBMENU_COMM || second == SUBMENU_SAVE)
            return;
    }

    advance_rng();
}

/* SubMenuControl(r10 = toolkit); r8-r12 pass through the screen handler,
 * r4-r7 are returned to the caller unchanged. */
Next SubMenuControl(void) {
    update_submenu(R10, R2, R3);
    RETURN_TO_CALLER();
}

/* ── IsButtonPressed ── */
/* IsButtonPressed(r0 = button mask):
 * Tests whether any of the buttons in r0 were newly pressed this frame.
 * Returns r1 = pressed-button word and sets NZ from (r0 & pressed),
 * so callers branch as if on "TST r0, r1". */
Next IsButtonPressed(void) {
    u32 joypad = LD32(R10 + 4);   /* r10 = toolkit, +4 = oToolkit_JoypadPtr */
    u32 pressed = LD16(joypad + 2); /* +2 = oJoypad_Pressed */

    R1 = pressed;
    SET_FLAGS_NZ(R0 & pressed);
    RETURN_TO_CALLER();
}

/* ── menu_moveCursorLeftRight_811FA0C ── */
/* menu_moveCursorLeftRight_811FA0C — horizontal cursor movement.
 *
 * Thin wrapper around the shared cursor mover menu_moveCursor_811FA36.
 * It builds the combined stack frame the mover's epilogue expects to pop
 * (saved r4-r7/lr on top of a 0x10-byte argument block), clears the
 * vertical bit of the direction argument so only left/right movement is
 * possible, and tail-jumps into the shared body. Because the mover pops
 * the save block pushed here, control returns straight from it to this
 * wrapper's caller with r4-r7 restored.
 *
 * Argument block (offsets from SP once the frame is set up):
 *   +0x00: 0x20  (initial repeat delay, frames)
 *   +0x04: 0x10  (auto-repeat rate, frames)
 *   +0x08: r3 as passed in
 *   +0x0C: r2 as passed in (bit 1 also cleared in the register copy)
 * r4 = 2 tells the shared body this is a horizontal move; r0, r1, r5,
 * r6, r7 and r10 pass through untouched.
 */
Next menu_moveCursorLeftRight_811FA0C(void) {
    /* One frame: 5 saved words (r4-r7, lr) + 0x10 bytes of arguments. */
    u32 frame = SP - 0x24;
    SP = frame;

    /* Save block for the shared epilogue (push {r4-r7,lr} layout). */
    ST32(frame + 0x10, R4);
    ST32(frame + 0x14, R5);
    ST32(frame + 0x18, R6);
    ST32(frame + 0x1C, R7);
    ST32(frame + 0x20, LR);

    /* Stack arguments for menu_moveCursor_811FA36. */
    ST32(frame + 0x00, 0x20);
    ST32(frame + 0x04, 0x10);
    ST32(frame + 0x08, R3);
    ST32(frame + 0x0C, R2);

    R2 &= ~2u; /* horizontal only: drop the vertical-move bit */
    R4 = 2;    /* direction mask read by the shared mover */

    GUEST_CALL(menu_moveCursor_811FA36);
    /* The mover popped the frame we built, so SP is back at its entry
     * value and r4-r7 hold the restored values; just return. */
    RETURN_TO_CALLER();
}

/* ── menu_moveCursorUpDown_811FA22 ── */
/* Menu cursor: move one entry back or forward with the d-pad. Two entries
 * build the same frame and share the body at menu_moveCursor_811FA36: menu_moveCursorUpDown_811FA22
 * (up/down, below) and menu_moveCursorLeftRight_811FA0C (lifted; right/left, keys 0x20/0x10).
 *   push {r4-r7,lr}; sub sp, #0x10; frame = {back key, forward key, start, mode}
 * r0 = pressed keys, r1 = number of entries, r2 = mode (bit 0: wrap around
 * at the ends, bit 1: no sound), r3 = current index. Returns r0 = the new
 * index; SOUND_MENU_CUR_MOVE plays when it changes (unless silent). */
enum { CURSOR_BACK_KEY = 0x0, CURSOR_FWD_KEY = 0x4, CURSOR_START = 0x8, CURSOR_MODE = 0xC, CURSOR_FRAME = 0x10 };
enum { MODE_WRAP = 1, MODE_SILENT = 2 };

Next menu_moveCursorUpDown_811FA22(void) {
    u32 sp = SP - 20;                           /* push {r4-r7,lr} */
    ST32(sp, R4); ST32(sp + 4, R5); ST32(sp + 8, R6); ST32(sp + 12, R7); ST32(sp + 16, LR);
    SP = sp - CURSOR_FRAME;
    ST32(SP + CURSOR_START, R3);
    ST32(SP + CURSOR_BACK_KEY, 0x40);           /* KEY_UP */
    ST32(SP + CURSOR_FWD_KEY, 0x80);            /* KEY_DOWN */
    ST32(SP + CURSOR_MODE, R2);
    R2 &= ~MODE_SILENT;                         /* the body tests r2 for the wrap bit */
    return menu_moveCursor_811FA36();
}

Next menu_moveCursor_811FA36(void) {
    u32 keys = R0, count = R1, wrap = R2, start = R3;
    s32 index = (s32)start;
    int moved = 0, wrapped_to_top = 0;
    if (keys & LD32(SP + CURSOR_BACK_KEY)) {
        R3 = LD32(SP + CURSOR_BACK_KEY);
        moved = 1;
        if (--index < 0) {                      /* past the first entry */
            moved = 0;
            if (!wrap) {
                wrapped_to_top = 1;             /* stay on entry 0 */
            } else {                            /* wrap to the last entry */
                index = (s32)count - 1;
                R3 = start;
                moved = (u32)index != start;
            }
        }
    } else {
        R3 = LD32(SP + CURSOR_FWD_KEY);
        if (keys & R3) {
            moved = 1;
            if (++index >= (s32)count) {        /* past the last entry */
                if (wrap) {
                    wrapped_to_top = 1;         /* wrap to entry 0 */
                } else {
                    index--;
                    moved = 0;
                }
            }
        }
    }
    if (wrapped_to_top) {                       /* loc_811FA74 */
        index = 0;
        R3 = start;
        moved = start != 0;
    }
    if (moved) {
        R1 = 2;                                 /* as the original (mov r1, #2) */
        if (!(LD32(SP + CURSOR_MODE) & MODE_SILENT)) {
            R0 = SOUND_MENU_CUR_MOVE;
            GUEST_CALL(PlaySoundEffect);
        }
    }
    R0 = (u32)index;
    SET_FLAGS_ADD(R0, 0);                       /* mov r0, r4 (adds #0): C = V = 0 */

    u32 sp = SP + CURSOR_FRAME;                 /* add sp, #0x10; pop {r4-r7,pc} */
    R4 = LD32(sp); R5 = LD32(sp + 4); R6 = LD32(sp + 8); R7 = LD32(sp + 12);
    u32 ret = LD32(sp + 16);
    SP = sp + 20;
    RETURN_TO(ret);
}

/* ── submenu_ConfirmExitAndFade_811FB64 ── */
/* submenu_ConfirmExitAndFade_811FB64(r0 = selected submenu entry, r5 = submenu state pointer):
 * record the highlighted entry in the submenu state, play the
 * "exit submenu" sound effect, then start a screen fade
 * (duration 0xC, mode 0x10) to leave the submenu. */
Next submenu_ConfirmExitAndFade_811FB64(void) {
    ST8(R5 + 1, R0);

    R0 = SOUND_EXIT_SUBMENU;
    GUEST_CALL(PlaySoundEffect);

    R0 = 0xC;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    RETURN_TO_CALLER();
}

/* ── clearS2001c04Byte6_811FB78 ── */
/* clearS2001c04Byte6_811FB78(r10 = toolkit pointer):
 * clear the byte at offset 6 of the game's S2001c04 work area. */
Next clearS2001c04Byte6_811FB78(void) {
    u32 work = LD32(R10 + oToolkit_S2001c04_Ptr);
    ST8(work + 6, 0);
    RETURN_TO_CALLER();
}

/* ── getKeyedTableEntry_811FB84 ── */
/* getKeyedTableEntry_811FB84: look up an ID in one of five key tables and hand back the
 * pointer stored for it. tableIndex selects a (key list, slot-list table)
 * pair from two parallel tables of pointers; the key list is a series of
 * word-sized IDs terminated by 0xFFFFFFFF. When the key is found, the
 * pointer list at the same position is indexed by the slot number taken
 * from byte 6 of the toolkit's S2001c04 struct (r10) and that word is the
 * result. A key of 0xFFFFFFFF finds nothing and yields 0xFFFFFFFF.
 * r2/r3 end on whatever the scan last left there; r4-r7 are only pushed
 * and popped by the original, so they pass through unchanged. */
typedef struct {
    u32 value;      /* -> r0: slot pointer, or 0xFFFFFFFF if not found */
    u32 lastKey;    /* -> r2: key word the scan ended on */
    u32 slotList;   /* -> r3: slot-pointer list selected on a match */
    u32 r4, r5, r6, r7; /* pass-through: the original pops these back */
} KeyLookup;

static KeyLookup find_key_entry(u32 key, u32 tableIndex, u32 inR4, u32 inR5, u32 inR6, u32 inR7, u32 toolkit) {
    KeyLookup out;
    out.r4 = inR4;
    out.r5 = inR5;
    out.r6 = inR6;
    out.r7 = inR7;

    u32 tableOffset = tableIndex << 2;
    u32 keyList = LD32(A_off_811FBBC + tableOffset);
    u32 slotListTable = LD32(A_off_811FBD4 + tableOffset);

    u32 offset = 0;
    u32 entry = LD32(keyList);

    for (;;) {
        if (entry == key) {
            /* Matched: use the slot chosen by the S2001c04 byte. */
            u32 slotList = LD32(slotListTable + offset);
            u32 slot = (u32)LD8(LD32(toolkit + oToolkit_S2001c04_Ptr) + 6) << 2;
            out.value = LD32(slotList + slot);
            out.lastKey = entry;
            out.slotList = slotList;
            return out;
        }
        offset += 4;
        if (key == 0xFFFFFFFF) {
            /* The key is the terminator itself: no entry matches. */
            out.value = 0xFFFFFFFF;
            out.lastKey = 0xFFFFFFFF;
            out.slotList = keyList;
            return out;
        }
        entry = LD32(keyList + offset);
    }
}

Next getKeyedTableEntry_811FB84(void) {
    KeyLookup out = find_key_entry(R0, R1, R4, R5, R6, R7, R10);
    R0 = out.value;
    R2 = out.lastKey, R3 = out.slotList;
    R4 = out.r4, R5 = out.r5, R6 = out.r6, R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── battle_ClearBuffer_811FE34 ── */
/* battle_ClearBuffer_811FE34(r0 = buffer address, r2/r3 = preserved by callee contract):
 * zero-fill 0x3C0 bytes at r0 via ZeroFillByEightWords.
 * Leaves r0, r2, r3 untouched; r1 still holds the fill size afterwards. */
Next battle_ClearBuffer_811FE34(void) {
    R1 = 0x3C0;                 /* size consumed by ZeroFillByEightWords */
    GUEST_CALL(ZeroFillByEightWords);
    RETURN_TO_CALLER();
}

/* ── registerHitCounter_811FE48 ── */
/* Register a hit on a 30-slot, 4-bytes-per-entry counter array.
 *
 *   entry[i] : u16 id at offset +0, u16 count at offset +2, for i in [0, 30).
 *
 * The id to record arrives in source[0x1C]; the counter array base is given
 * as a guest pointer. We scan the entries:
 *   - empty slot (id == 0)  -> claim it: store the id, set count = 1.
 *   - matching id           -> increment its count.
 *   - otherwise             -> skip to the next 4-byte entry.
 *
 * Returns the final r3: the byte offset of the count field that was touched,
 * or 0x78 if the array was scanned without finding either a match or an empty slot. */
static u32 register_counter_hit(u32 counter_array, u32 source) {
    u16 new_id = LD16(source + 0x1C);
    u32 offset = 0;
    while (offset < 0x78) {
        u16 slot_id = LD16(counter_array + offset);
        if (slot_id == 0) {
            ST16(counter_array + offset, new_id);
            ST16(counter_array + offset + 2, 1);
            return offset + 2;
        }
        if (slot_id == new_id) {
            u16 count = LD16(counter_array + offset + 2);
            ST16(counter_array + offset + 2, count + 1);
            return offset + 2;
        }
        offset += 4;
    }
    return offset;  /* 0x78: array full, nothing matched */
}

Next registerHitCounter_811FE48(void) {
    R3 = register_counter_hit(R0, R5);
    RETURN_TO_CALLER();
}

/* ── buildChipSearchTable_811FE7C ── */
/* Register-form helpers for external game functions */

/* register form: calls zeroFill_0xA000Words_811FF6C outside this file, which has no typed C interface yet */
static void zero_fill_chip_table(u32 address) {
    R0 = address;
    GUEST_CALL(zeroFill_0xA000Words_811FF6C);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_811FE7C(s32 chip_idx) {
    R0 = chip_idx;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls getOffsetToQuantityOfChipCodeMaybe_8021c7c outside this file.
   Callee reads R0=chip_idx, R1=search_item, R4=off, R7=chip_idx; returns ptr in R0, table_index in R3. */
static u32 get_qty_offset(u32 chip_idx, u32 search_item, u32 off, u32 *out_index) {
    R0 = chip_idx;
    R1 = search_item;
    R4 = off;
    R7 = chip_idx;
    GUEST_CALL(getOffsetToQuantityOfChipCodeMaybe_8021c7c);
    *out_index = R3;
    return R0;
}

/* register form: calls loadU16FromTableEntry_81376CC outside this file, which has no typed C interface yet */
static u32 load_u16_from_table(u32 index) {
    R1 = index;
    GUEST_CALL(loadU16FromTableEntry_81376CC);
    return R0;
}

/* register form: calls sub_811FF54 outside this file, which has no typed C interface yet */
static u32 map_chip_element(u8 value) {
    R0 = value;
    GUEST_CALL(sub_811FF54);
    return R0;
}

/* register form: calls sub_811FF80 outside this file. Callee reads R0=chip_idx, R3=composite_key. */
static u32 map_chip_index(u32 chip_idx, u32 key) {
    R0 = chip_idx;
    R3 = key;
    GUEST_CALL(sub_811FF80);
    return R0;
}

/* Build the chip-search table: scan chips 1..0x13A, write valid 0x20-byte entries
   into the zeroed buffer at dest.  mode (r1) controls extra filtering.
   Returns the number of entries written (= bytes / 0x20, matching SWI 6 divide). */
static u32 build_chip_search_table(u32 dest, u32 mode) {
    u32 cur = dest;

    zero_fill_chip_table(dest);

    for (s32 idx = 1; idx <= 0x13A; idx++) {
        u32 chip = get_chip_data_sub_811FE7C(idx);

        /* Extra filters when mode is non-zero: skip chip 0x137, type-2 chips,
           and chips with flag bit 0x20 at offset 9. */
        if (mode != 0) {
            if (idx == 0x137) continue;
            if ((u8)LD8(chip + 7) == 2) continue;
            if ((u8)LD8(chip + 9) & 0x20) continue;
        }

        /* Always skip chips flagged unusable (bit 0x20 at offset 0x16). */
        if ((u8)LD8(chip + 0x16) & 0x20) continue;

        for (u32 off = 0; off <= 3; off++) {
            u32 search_item = (u8)LD8(chip + off);
            if (search_item == 0xFF) continue;

            u32 table_index;
            u32 qty_ptr = get_qty_offset(idx, search_item, off, &table_index);

            u32 quantity = (u8)LD8(qty_ptr);
            if (quantity == 0) continue;

            u32 table_val = load_u16_from_table(table_index);

            /* first_key uses the chip index; key uses the chip's 16-bit field at +0x1C. */
            u32 first_key = ((u32)idx << 7) | search_item;
            u32 key = (LD16(chip + 0x1C) << 7) | search_item;

            /* Write the 0x20-byte lookup entry: (payload << 16) | composite_key per dword. */
            ST32(cur + 0x1C, (table_val << 16) | first_key);
            ST32(cur + 0x00, (LD16(chip + 0x1C) << 16) | key);
            ST32(cur + 0x08, (search_item << 16) | key);
            ST32(cur + 0x04, (LD16(chip + 0x18) << 16) | key);
            ST32(cur + 0x10, (map_chip_element((u8)LD8(chip + 6)) << 16) | key);
            ST32(cur + 0x18, ((u32)LD8(chip + 8) << 16) | key);
            ST32(cur + 0x14, (quantity << 16) | key);
            ST32(cur + 0x0C, (map_chip_index((u32)idx, key) << 16) | key);

            cur += 0x20;
        }
    }

    /* Original does: r0 = cur - dest; r1 = 0x20; SWI 6 (divide).  Exact integer result: */
    return (cur - dest) / 0x20;
}

/* buildChipSearchTable_811FE7C(r0 = dest buffer, r1 = mode) -> r0 = entry count */
Next buildChipSearchTable_811FE7C(void) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = build_chip_search_table(R0, R1);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    RETURN_TO_CALLER();
}

/* ── sub_811FF54 ── */
/* sub_811FF54(r0 = index): look up an entry in the fixed byte table at
 * ROM byte_811FF60; r0 = byte at byte_811FF60[index]. The load is not
 * bounds-checked in the original, so keep it as a raw guest memory read. */
Next sub_811FF54(void) {
    R0 = LD8(A_byte_811FF60 + R0);
    RETURN_TO_CALLER();
}

/* ── zeroFill_0xA000Words_811FF6C ── */
/* zeroFill_0xA000Words_811FF6C(r0 = start address):
 * Zero-fill 0xA000 words (160 KB) of memory starting at the address in r0.
 * r0, r2 and r3 pass through unchanged; r1/r2/r3 end up as
 * ZeroFillByEightWords leaves them. */
Next zeroFill_0xA000Words_811FF6C(void) {
    R1 = 0xA000;                 /* word count from the literal pool at byte_811FF78 */
    GUEST_CALL(ZeroFillByEightWords);
    RETURN_TO_CALLER();
}

/* ── sub_811FF80 ── */
/* sub_811FF80(r0 = which_chip)
 *
 * Classifies a chip based on its element flags (byte at offset 9 of the chip data):
 *   - bit 0x10 set: r0 = r2 = 1, r1 = 1
 *   - bit 0x02 set, chip id > 0x3E7: r0 = r2 = sub_8010C76(which_chip), r1 = 0
 *   - bit 0x02 set, chip id <= 0x3E7: r0 = r2 = chip_id, r1 = 0
 *   - neither: r0 = r2 = 5, r1 = 0
 */
Next sub_811FF80(void) {
    u32 which_chip = R0;

    /* push {r3, r4, lr} */
    u32 saved_r3 = R3;
    u32 saved_r4 = R4;
    u32 saved_lr = LR;
    u32 sp = SP -= 12;
    ST32(sp,     saved_r3);
    ST32(sp + 4, saved_r4);
    ST32(sp + 8, saved_lr);

    R0 = which_chip;
    GUEST_CALL(getChip8021DA8);
    u32 chip = R0;

    u32 r2;
    u8 flags = LD8(chip + 9);
    if (flags & 0x10) {
        /* Element 0x10 set */
        r2 = 1;
        R1 = 1;
    } else if (flags & 0x02) {
        /* Element 0x02 set: clamp chip id at 0x3E7 */
        u16 chip_id = LD16(chip + 0x1A);
        if ((s32)chip_id > 0x3E7) {
            R0 = which_chip;
            GUEST_CALL(sub_8010C76);
            r2 = R0;
        } else {
            r2 = chip_id;
        }
        R1 = 0;
    } else {
        /* Neither element flag set */
        r2 = 5;
        R1 = 0;
    }

    R0 = r2;

    /* pop {r3, r4, pc} */
    sp = SP;
    R3 = LD32(sp);
    R4 = LD32(sp + 4);
    u32 ret = LD32(sp + 8);
    SP = sp + 12;
    RETURN_TO_CALLER();
}

/* ── sub_81202B4 ── */
/* sub_81202B4: expand a text string into pairs of font tile indices.
 *
 * r0 = pointer to the source byte string
 * r2 = pointer to a u16 array receiving the left tile index of each character
 * r3 = pointer to a u16 array receiving the right tile index (left index + 1)
 * r4 = base tile number that every character code is offset by
 * r5 = total number of character slots to fill
 *
 * Encoding:
 *   byte <  0xE4  ordinary character, tile pair (base + ch*2, base + ch*2 + 1)
 *   byte == 0xE4  widened into the 16-bit code 0xE4E4 before the same lookup
 *   byte == 0xE5  control byte: consumed without producing any output
 *   byte >= 0xE6  end of string
 *
 * After the string ends, the remaining slots up to r5 are zero-filled.
 * Returns the number of characters decoded (padding not counted) in r0;
 * r2/r3 are left pointing past the last entry written. r4-r7 are preserved. */
Next sub_81202B4(void) {
    u32 src   = R0;
    u32 dstLo = R2;
    u32 dstHi = R3;
    u32 base  = R4;
    s32 limit = (s32)R5;
    s32 count = 0;
    s32 decoded;

    for (;;) {
        u32 ch = LD8(src);
        if (ch >= 0xE6)             /* end of string */
            break;
        src++;

        if (ch == 0xE5)             /* control byte: no output */
            continue;
        if (ch == 0xE4)             /* widen into a full 16-bit character code */
            ch = (ch << 8) | ch;

        u32 tile = base + (ch << 1);
        ST16(dstLo, tile);
        ST16(dstHi, tile + 1);
        dstLo += 2;
        dstHi += 2;
        count++;
    }
    decoded = count;

    /* zero-fill the unused tail of the destination buffers */
    while (count < limit) {
        ST16(dstLo, 0);
        ST16(dstHi, 0);
        dstLo += 2;
        dstHi += 2;
        count++;
    }

    R0 = decoded;
    R2 = dstLo;
    R3 = dstHi;
    RETURN_TO_CALLER();
}

/* ── sub_8120348 ── */
/* sub_8120348(r0 = passthrough, r1 = tilemap dest, r2 = palette, r3 = run length,
 *             r4 = offset to second run):
 * Fills two halfword runs of a tilemap with (palette << 12) | 0x200 -- the
 * blank text box tile in the requested palette: `r3` halfwords starting at r1,
 * and the same number starting at r1 + 2*r4. Afterwards both end cursors and
 * the remaining length (r4 - r3) are handed to sub_81202B4, which keeps
 * filling from there; sub_804A24C is called in between with what is left of
 * the counter (normally 0) and r6 = 0.
 * Note the fill is a do/while: even a count of 0 (or below) writes one
 * halfword per run, matching the original's sub/bgt loop.
 * All registers, including r4-r7, come back unchanged. */
Next sub_8120348(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    u32 fill = (R2 << 12) | 0x200;   /* tilemap word: blank tile, palette r2 */
    u32 runA = R1;                   /* fill cursor for the first run */
    u32 runB = R1 + (R4 << 1);       /* fill cursor for the second run */
    s32 count = (s32)R3;             /* halfwords to write per run */
    s32 remaining = (s32)R4 - (s32)R3; /* run length left for sub_81202B4 */

    do {
        ST16(runA, fill);
        ST16(runB, fill);
        runA += 2;
        runB += 2;
    } while (--count > 0);

    /* sub_804A24C(count leftover, ..., fill, end cursors, r5, 0, r7) */
    R1 = fill;
    R2 = runA;
    R3 = runB;
    R4 = (u32)count;
    R6 = 0;
    GUEST_CALL(sub_804A24C);

    /* sub_81202B4(r1 as left by sub_804A24C, end cursors, fill, r4 - r3, 0) */
    R2 = runA;
    R3 = runB;
    R4 = fill;
    R5 = (u32)remaining;
    R6 = 0;
    GUEST_CALL(sub_81202B4);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── battle_loadFolderChipGfx_8120390 ── */
/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void copy_words(u32 src, u32 dest, u32 byte_size) {
    R0 = src;
    R1 = dest;
    R2 = byte_size;
    GUEST_CALL(CopyWords);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_8120390(s32 chip_id) {
    R0 = (u32)chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls QueueWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx_transfer_sub_8120390(u32 src, u32 dest, u32 byte_size) {
    R0 = src;
    R1 = dest;
    R2 = byte_size;
    GUEST_CALL(QueueWordAlignedGFXTransfer);
}

/* Load battle-chip graphics for a run of folder entries.
 *
 * First copies the 0x38-byte template record over `dest`. Then, for each of
 * `count` folder entries starting at `entry_array + first_entry * 0x20`:
 *   - if the entry names a chip (ID is the halfword at +0x1C shifted down
 *     past its flag bits), queues that chip's 0x80-byte image into the next
 *     GFX slot;
 *   - otherwise clears the entry's two output words.
 *
 * `gfx_slots` advances by 0x80 per chip and the per-entry output words by
 * 8 per entry. Returns nothing; the original leaves r4-r7 untouched. */
static void load_folder_chip_gfx(u32 first_entry, u32 entry_array,
                                 u32 dest, u32 template_src,
                                 u32 gfx_slots, s32 count) {
    copy_words(template_src, dest, 0x38);

    u32 entry = entry_array + first_entry * 0x20;
    u32 out   = dest;
    u32 gfx   = gfx_slots;

    for (s32 i = 0; i < count; i++) {
        u32 chip_id = (u32)LD16(entry + 0x1C) >> 7;
        if (chip_id != 0) {
            u32 chip = get_chip_data_sub_8120390((s32)chip_id);
            queue_gfx_transfer_sub_8120390(LD32(chip + 0x20), gfx, 0x80);
        } else {
            ST32(out, 0);
            ST32(out + 4, 0);
        }
        entry += 0x20;
        gfx   += 0x80;
        out   += 8;
    }
}

/* battle_loadFolderChipGfx_8120390(r0 = first entry index, r1 = entry array, r2 = dest record,
 *              r3 = template source, r4 = GFX slot base, r5 = entry count)
 * The original pushes/pops r4-r7, so r6/r7 (listed as outputs) simply keep
 * their entry values; the typed function never touches them. */
Next battle_loadFolderChipGfx_8120390(void) {
    load_folder_chip_gfx(R0, R1, R2, R3, R4, (s32)R5);
    RETURN_TO_CALLER();
}

/* ── sub_81203E4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_ZeroFillByWord_result;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result call_ZeroFillByWord(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result){ R3 };
}

typedef struct { u32 r0; } call_getChip8021DA8_result;
/* register form: generated wrapper: getChip8021DA8 has no typed C interface yet */
static inline call_getChip8021DA8_result call_getChip8021DA8(u32 r0) {
    R0 = r0;
    GUEST_CALL(getChip8021DA8);
    return (call_getChip8021DA8_result){ R0 };
}

/* register form: generated wrapper: sub_811FF54 has no typed C interface yet */
static inline u32 call_sub_811FF54(u32 r0) {
    R0 = r0;
    GUEST_CALL(sub_811FF54);
    return R0;
}

/* sub_81203E4 -- build a packed chip-slot list.
 *
 * Zero-fills 0x38 bytes at the output list, then walks `count` consecutive
 * 0x20-byte entries of the table starting at entry_base + entry_index * 0x20.
 * For each entry, the halfword at +0x1C shifted right by 7 tells whether a
 * chip occupies the slot:
 *   - set:    look the chip up, map its id byte through sub_811FF54 to a
 *             slot number, and store two words built from packed 16-bit
 *             halves (slot_base + slot*4, +1, then +0x20002, +0x20003)
 *             at out_list + 8*i and out_list + 8*i + 4;
 *   - clear:  store two zero words there.
 *
 * The loop is a do-while: the original only tests the counter at the bottom
 * (cmp/blt), so the body always executes at least once, even when the count
 * is zero or negative. r4 is bumped by 0x80 per iteration but never read;
 * r4-r7 are pushed/popped, so the only architectural output is r7, which
 * comes back unchanged.
 */

#define SLOT_ENTRY_SIZE     0x20    /* bytes per entry in the source table */
#define SLOT_LIST_SIZE      0x38    /* bytes zero-filled in the output list */
#define SLOT_STATUS_OFFSET  0x1C    /* status halfword within an entry */
#define CHIP_ID_OFFSET      6       /* id byte within the chip data record */
#define SLOT_ADDR_SPACING   0x20002 /* constant added to the second word */

static void build_chip_slot_list(u32 entry_index, u32 entry_base,
                                 u32 out_list, u32 slot_base, u32 count)
{
    call_ZeroFillByWord(out_list, SLOT_LIST_SIZE);

    u32 entry = entry_base + entry_index * SLOT_ENTRY_SIZE;
    u32 out   = out_list;
    s32 i     = 0;

    do {
        u32 present = (u32)LD16(entry + SLOT_STATUS_OFFSET) >> 7;

        u32 word_lo, word_hi;
        if (present != 0) {
            u32 chip = call_getChip8021DA8(present).r0;
            u32 slot = call_sub_811FF54(LD8(chip + CHIP_ID_OFFSET));
            u32 addr = slot_base + slot * 4;
            word_lo = addr | ((addr + 1) << 16);
            word_hi = word_lo + SLOT_ADDR_SPACING;
        } else {
            word_lo = 0;
            word_hi = 0;
        }
        ST32(out, word_lo);
        ST32(out + 4, word_hi);

        entry += SLOT_ENTRY_SIZE;
        out += 8;               /* r7 gains 4 and the index gains 4: 8 per slot */
        i++;
    } while (i < (s32)count);
}

/* sub_81203E4(r0 = entry_index, r1 = entry_base, r2 = out_list,
 *             r3 = slot_base, r5 = count) -> r7 (unchanged, preserved) */
Next sub_81203E4(void) {
    build_chip_slot_list(R0, R1, R2, R3, R5);
    RETURN_TO_CALLER();
}

/* ── sub_81204C4 ── */
/* sub_81204C4(r0 = entry index, r1 = array base, r2 = dest pointer,
 *             r3 = sub_8120504 param, r5 = number of entries (signed)):
 * Iterate over 32-byte battle-chip slots starting at base + index*32.
 * For each slot whose 16-bit field at +0x1C has bits[15:7] non-zero,
 * look up that chip's type byte and pass it to sub_8120504 together
 * with an 8-byte dest slot.  Otherwise zero the dest slot.
 * Dest advances 8 bytes per entry; slot pointer advances 32 bytes. */
Next sub_81204C4(void) {
    s32  count = (s32)R5;
    u32  base  = R1 + R0 * 32;
    u32  dest  = R2;
    u32  param = R3;
    s32  i     = 0;

    do {
        u16 chip_field  = (u16)LD16(base + 0x1C);
        u32 chip_select = (u32)(chip_field >> 7);  /* non-zero → chip present */

        if (chip_select) {
            R0 = chip_select;
            GUEST_CALL(getChip8021DA8);
            R0 = (u8)LD8(R0 + 8);
            R1 = dest;
            R2 = param;
            GUEST_CALL(sub_8120504);
        } else {
            ST32(dest,     0);
            ST32(dest + 4, 0);
        }

        base += 0x20;
        dest += 8;
        i++;
    } while (i < count);

    RETURN_TO_CALLER();
}

/* ── sub_812053C ── */
/* sub_812053C:
 *   Ask sub_81205C0 for a value; if it came back non-negative, build an
 *   address/flag word (low 0x4000 on, top bits and 0x8000 cleared) and
 *   hand it to sub_802FE28 together with (0xC + original r1).
 *   inputs:  r0 r1 r2 lr
 *   outputs: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r5 r6 r7 */
Next sub_812053C(void) {
    /* r4 holds the caller's r1 across the leaf call (preserved register). */
    u32 saved_r1 = R1;

    /* mov r3, #0 ; bl sub_81205C0 */
    R3 = 0;
    GUEST_CALL(sub_81205C0);
    /* tst r0, r0 : sets N/Z from R0; the original blt reads N (taken when R0 < 0). */
    SET_FLAGS_NZ(R0 & R0);

    if (!NEGATIVE_FLAG()) {
        /* bic r0, #0xc0008000 ; ldr r1, =0x4000 ; orr r0, r1 */
        R0 = (R0 & ~(u32)0xC0008000u) | (u32)0x4000u;
        /* mov r1, #0xc ; add r1, r1, r4 */
        R1 = 0xCu + saved_r1;
        /* mov r2, #0 ; mov r3, #1 ; bl sub_802FE28 */
        R2 = 0;
        R3 = 1;
        GUEST_CALL(sub_802FE28);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8120618 ── */
/* Decode one 0x20-byte panel: if the activity flag (bits 7+ of the u16 at +0x1C) is set
 * and the low halfword of the u32 at +0x14 is nonzero, clamp the high halfword to 0x63,
 * divide by 10 to get tens/ones, offset each by src_base, and pack into two consecutive
 * words at dst.  Otherwise store two zeros. */
static void convert_panel(u32 panel_addr, u32 src_base, u32 dst) {
    u16 flags = LD16(panel_addr + 0x1C);
    if ((u32)flags >> 7 == 0) {
        ST32(dst, 0);
        ST32(dst + 4, 0);
        return;
    }

    u32 field = LD32(panel_addr + 0x14);
    if ((u16)field == 0) {
        ST32(dst, 0);
        ST32(dst + 4, 0);
        return;
    }

    u32 high = field >> 16;
    if (high > 0x63)
        high = 0x63;

    /* Equivalent to SVC 6 (unsigned div by 10): quotient = tens digit, remainder = ones */
    u32 quotient  = high / 10;
    u32 remainder = high % 10;

    /* Low 16 bits: src_base when quotient==0, else src_base + (quotient+1)*2 */
    u32 low_bits  = (quotient == 0) ? src_base
                                    : src_base + (quotient + 1) * 2;
    /* High 16 bits: src_base + (remainder+1)*2 */
    u32 high_bits = src_base + (remainder + 1) * 2;

    u32 packed = (high_bits << 16) | low_bits;
    ST32(dst, packed);
    ST32(dst + 4, packed + 0x10001);
}

/* Decode `count` 0x20-byte panels starting at index `first` in the table at `table_base`.
 * Output is two u32 words per panel at `dst` (stride 8).  The loop is do-while: it
 * executes at least once even when count is zero or negative (as signed). */
static void decode_panels(u32 first, u32 table_base, u32 dst,
                          u32 src_base, u32 count) {
    u32 panel = first * 0x20 + table_base;
    u32 i = 0;
    do {
        convert_panel(panel, src_base, dst);
        panel += 0x20;
        dst   += 8;
        i++;
    } while ((s32)i < (s32)count);
}

/* sub_8120618(r0 = first panel index, r1 = table base, r2 = output buffer,
 *             r3 = tile source base, r5 = panel count) */
Next sub_8120618(void) {
    decode_panels(R0, R1, R2, R3, R5);
    RETURN_TO_CALLER();
}

/* ── sub_812068C ── */
/* sub_812068C(r0 = value, r1 = row, r2 = arg2, r3 = arg3):
 * Queries sub_81205C0 for a packed position; if one is found (result >= 0),
 * triggers sub_802FE28 with it, then queries sub_81205C0 again with the
 * position's high half moved down by 8. Callees observe r4 = row, r6 = arg3
 * and r7 = the queried position, as in the original. Preserves r4-r7 and
 * returns with the flags left by the final sub_81205C0 call. */
Next sub_812068C(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    u32 value = R0;
    u32 row   = R1;
    u32 arg2  = R2;
    u32 arg3  = R3;

    R4 = row;   /* mov r4, r1 */
    R6 = arg3;  /* mov r6, r3 */

    /* First query: (value, row, arg2, 1) -> packed position, negative = none. */
    R0 = value;
    R1 = row;
    R2 = arg2;
    R3 = 1;
    GUEST_CALL(sub_81205C0);
    u32 pos = R0;
    R7 = pos;

    if ((s32)pos >= 0) {
        /* Found one: sub_802FE28(pos, row + 0x16, 0, 1). R0 already holds pos. */
        R1 = row + 0x16;
        R2 = 0;
        R3 = 1;
        GUEST_CALL(sub_802FE28);
    }

    /* Re-pack the position: low half unchanged, high half shifted down by 8. */
    u32 lowHalf  = pos << 16 >> 16;
    u32 highHalf = (u32)(((s32)pos >> 16) + 8);
    u32 shifted  = (highHalf << 16) | lowHalf;

    /* Second query: (arg3, row, shifted position, 1). */
    R0 = arg3;
    R1 = row;
    R2 = shifted;
    R3 = 1;
    GUEST_CALL(sub_81205C0);

    /* pop {r4-r7} */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_81206C4 ── */
/* sub_81206C4(r0 = chip entry array, r5 = list header, count at header+0x28):
 * Walks the list of 0x20-byte chip entries backwards (newest first) and hands
 * each one to GiveChips.  Entries whose packed value has no count in the upper
 * 9 bits are skipped.  The list header pointer (r5) is preserved. */
Next sub_81206C4(void) {
    u32 count = LD16(R5 + 0x28);

    if (count != 0) {
        /* Start at the last entry and step back one 0x20-byte slot per pass. */
        u32 entry = R0 + (count - 1) * 0x20;
        u32 i;
        for (i = 0; i < count; i++) {
            /* The field at +0x1C packs two values into its low halfword:
             * bits 15..7 are the amount, bits 6..0 the chip code. */
            u32 packed = LD32(entry + 0x1C) << 16;
            u32 amount = packed >> 23;

            if (amount != 0) {
                R0 = amount;
                R1 = (packed << 9) >> 25;
                R2 = LD32(entry + 0x14) >> 16;  /* high halfword of the +0x14 field */
                GUEST_CALL(GiveChips);
            }
            entry -= 0x20;
        }
    }
    RETURN_TO_CALLER();
}

/* ── countEventFlagChips_81207F8 ── */
/* Register layout for the event-flag table (per iteration):
 *   [0] base flag 1, [2] base flag 2, [4] base flag 3, [6] entry count */
#define TABLE_FLAG1 0
#define TABLE_FLAG2 2
#define TABLE_FLAG3 4
#define TABLE_COUNT 6

/* Support-type table offset added to index (320 decimal) */
#define SUPPORT_TABLE_OFFSET 0x140

/* ChipData field offsets used for filtering */
#define CHIP_ATTR_FLAGS 9
#define CHIP_ATTR_SPECIAL 0x16
#define CHIP_ATTR_ELEMENT 7

/* Chip filter bit masks */
#define CHIP_FLAG_SUPPORT 0x40
#define CHIP_FLAG_MEGAMAN 0x20
#define CHIP_FLAG_ELEMENT 1

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet.
 * Preserves R4-R7 per SYNC_SNAP; clobbers R1-R3 which callers read as outputs. */
static bool event_flag_set_sub_81207F8(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();   /* Z set → flag clear */
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet.
 * Preserves R4-R7 per SYNC_SNAP; clobbers R1-R3 which callers read as outputs. */
static u32 get_chip_data_sub_81207F8(s32 which_chip) {
    R0 = (u32)which_chip;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* Returns the number of event-flagged chips whose type/attributes match category.
 *   4 = support category (uses alt table + SUPPORT_TABLE_OFFSET index),
 *   5 = element-gated chips (requires CHIP_FLAG_ELEMENT on chip),
 *   6 = Mega Man category (requires CHIP_FLAG_MEGAMAN on chip),
 *   0-3 = general chips (fall through to MEGAMAN/element checks).
 *
 * R1 and R2 are written at the same points as the original's ldrb/mov before TST
 * (register form: these are observable output registers with no typed C equivalent).
 * R3 is never written in the function body — callees leave their value in it. */
static u32 count_event_chips(s32 chip_type) {
    u32 table = (chip_type == 4) ? A_word_8120890 : A_word_8120888;
    u32 limit = LD16(table + TABLE_COUNT);
    u32 count = 0;

    for (u32 idx = 0; (s32)idx <= (s32)limit; idx++) {
        /* At least one of three per-entry event flags must be set.
         * Short-circuit matches the original's bne/beq chain. */
        if (!(event_flag_set_sub_81207F8(LD16(table + TABLE_FLAG1) + idx)
           || event_flag_set_sub_81207F8(LD16(table + TABLE_FLAG2) + idx)
           || event_flag_set_sub_81207F8(LD16(table + TABLE_FLAG3) + idx)))
            continue;

        /* Fetch chip data — support type shifts into the support table region */
        s32 chip_idx = (chip_type == 4) ? (s32)(idx + SUPPORT_TABLE_OFFSET) : (s32)idx;
        u32 chip = get_chip_data_sub_81207F8(chip_idx);

        /* Chip attribute filter.
         * register form: R1/R2 hold the last ldrb/mov pair before the TST that
         * determines the branch; they are output registers visible to callers. */
        bool qualifies;

        if (chip_type != 6) {
            /* Non-Mega Man: must have CHIP_FLAG_SUPPORT bit */
            R1 = LD8(chip + CHIP_ATTR_FLAGS);
            R2 = CHIP_FLAG_SUPPORT;
            qualifies = (R1 & R2) != 0;
        } else {
            qualifies = true;  /* type 6 jumps straight to loc_8120856 */
        }

        if (qualifies) {
            if (chip_type == 5) {
                /* Element-gated: require special attribute bit */
                R1 = LD8(chip + CHIP_ATTR_SPECIAL);
                R2 = CHIP_FLAG_ELEMENT;
                qualifies = (R1 & R2) != 0;
            } else {
                /* General or Mega Man path (loc_8120856): */
                R1 = LD8(chip + CHIP_ATTR_FLAGS);
                R2 = CHIP_FLAG_MEGAMAN;
                if (R1 & R2) {
                    qualifies = true;
                } else {
                    R1 = LD8(chip + CHIP_ATTR_SPECIAL);
                    R2 = CHIP_FLAG_ELEMENT;
                    if (R1 & R2) {
                        qualifies = false;
                    } else {
                        u32 elem = LD8(chip + CHIP_ATTR_ELEMENT);
                        qualifies = (s32)elem == chip_type;
                    }
                }
            }
        }

        if (qualifies)
            count++;
    }

    return count;
}

/* countEventFlagChips_81207F8(r0 = chip_type) -> r0 = qualifying chip count; r1/r2/r3 = callee scratch.
 * r4-r7 and lr are preserved via the function-level push/pop in the original;
 * the GUEST_CALLs' callees also preserve them per SYNC_SNAP.
 * r1/r2/r3 are clobbered by the last external call or TST, matching the original. */
Next countEventFlagChips_81207F8(void) {
    R0 = count_event_chips((s32)R0);
    RETURN_TO_CALLER();
}

/* ── scoreboard_buildScoreBcdTilemap_8120900 ── */
/* register form: memory_bcdConvert is an external callee; it reads r7
 * (tile-map pointer) and preserves r4-r6, so we pass r7 and restore
 * all preserved registers after the call */
static u32 bcd_convert(u32 value, u32 tile_map) {
    u32 sr4 = R4, sr5 = R5, sr6 = R6, sr7 = R7;
    R0 = value;
    R7 = tile_map;
    GUEST_CALL(memory_bcdConvert);
    u32 result = R0;
    R4 = sr4; R5 = sr5; R6 = sr6; R7 = sr7;
    return result;
}

/* register form: CopyBackgroundTiles is an external callee; we pass its
 * six arguments in r0-r6 and must restore preserved registers r4-r7 */
static void copy_bg_tiles_sub_8120900(u32 j, u32 i, u32 tile_map, u32 shadow_map) {
    u32 sr4 = R4, sr5 = R5, sr6 = R6, sr7 = R7;
    R0 = j; R1 = i; R2 = 2; R3 = tile_map;
    R4 = 0xB; R5 = 2; R6 = shadow_map;
    GUEST_CALL(CopyBackgroundTiles);
    R4 = sr4; R5 = sr5; R6 = sr6; R7 = sr7;
}

/*
 * Build an 11-column BCD score tilemap: two 4-digit numbers separated by
 * a dash, blank-padded on the left, terminated with a priority-set blank.
 * Then blit the 11x2 grid onto the background.
 *
 * Layout (one u16 per column, normal + shadow copies):
 *   col 0:        base_id            (leading blank / alignment)
 *   col 1..4:     first 4 digits     (left-padded with blanks if needed)
 *   col 5:        base_id + 0x16     (separator dash)
 *   col 6..9:     second 4 digits    (left-padded with blanks if needed)
 *   col 10:       base_id | 0x400    (terminator blank with priority bit)
 *
 * Digit tile = base_id + digit*2 + 2  (even id = foreground, odd = shadow)
 * Blank tile = base_id + 0x18
 */
static void build_bcd_tilemap(u32 tile_map, u32 val1, u32 val2,
                              u32 base_id, u32 bg_j, u32 bg_i) {
    u32 shadow_map = tile_map + 22;

    u32 bcd1 = bcd_convert(val1, tile_map);
    u32 bcd2 = bcd_convert(val2, tile_map);

    /* Column 0: unconditional alignment blank */
    ST16(tile_map, base_id);
    ST16(shadow_map, base_id + 1);

    /* Columns 2, 4, 6, 8: first BCD value (4 nibbles, most-significant first) */
    s32 off = 2;
    bool seen = false;
    for (s32 shift = 12; shift >= 0; shift -= 4) {
        u32 digit = (bcd1 >> shift) & 0xF;
        if (digit == 0 && !seen && off != 8) {
            ST16(tile_map + off, base_id + 0x18);
            ST16(shadow_map + off, base_id + 0x19);
        } else {
            ST16(tile_map + off, base_id + digit * 2 + 2);
            ST16(shadow_map + off, base_id + digit * 2 + 3);
            seen = true;
        }
        off += 2;
    }

    /* Column 10: separator */
    ST16(tile_map + off, base_id + 0x16);
    ST16(shadow_map + off, base_id + 0x17);
    off += 2;

    /* Columns 12, 14, 16, 18: second BCD value */
    seen = false;
    for (s32 shift = 12; shift >= 0; shift -= 4) {
        u32 digit = (bcd2 >> shift) & 0xF;
        if (digit == 0 && !seen && off != 18) {
            ST16(tile_map + off, base_id + 0x18);
            ST16(shadow_map + off, base_id + 0x19);
        } else {
            ST16(tile_map + off, base_id + digit * 2 + 2);
            ST16(shadow_map + off, base_id + digit * 2 + 3);
            seen = true;
        }
        off += 2;
    }

    /* Column 20: terminator blank with background-priority bit (0x400) */
    u32 term = base_id | 0x400;
    ST16(tile_map + off, term);
    ST16(shadow_map + off, term + 1);

    /* Blit the completed 11x2 grid to the background layer */
    copy_bg_tiles_sub_8120900(bg_j, bg_i, tile_map, shadow_map);
}

/* scoreboard_buildScoreBcdTilemap_8120900(r0=tile_map, r1=val1, r2=val2, r3=base_tile, r4=j, r6=i) */
Next scoreboard_buildScoreBcdTilemap_8120900(void) {
    build_bcd_tilemap(R0, R1, R2, R3, R4, R6);
    RETURN_TO_CALLER();
}

/* ── sub_81209DC ── */
/* sub_81209DC(r0 = category index into range table):
 * Selects a random battle element ID and a bias modifier.
 *   1. Looks up the number of possible outcomes for the category (ROM table).
 *   2. Draws a random index in [0, range) via the secondary RNG.
 *   3. If category >= 2, shifts the index by +0x60 into a higher ID band.
 *   4. Draws a second random index in [0, 21) via the primary RNG to pick
 *      a bias/skew byte from a lookup table.
 * Returns: r0 = low byte of the adjusted random ID, r1 = bias table value.
 * r6, r7 are preserved. */
Next sub_81209DC(void) {
    u32 category = R0;

    /* Range table at ROM 0x8120A14: {0x44, 0x60, 0x44}.
     * Out-of-domain indices simply read whatever follows in ROM, matching original. */
    u32 range = LD32(A_off_8120A14 + (category << 2));

    /* Draw a random value and take it modulo the category's range. */
    GUEST_CALL(GetPositiveSignedRNGSecondary);
    R1 = range;
    swi_call(6);
    /* R1 = secondary_rng % range (unsigned remainder) */

    /* Categories 2 and above shift into a different element ID band. */
    if ((s32)category >= 2)
        R1 += 0x60;

    u32 adjusted = R1;

    /* Draw a second random value modulo 21 to select a bias modifier. */
    GUEST_CALL(GetPositiveSignedRNG);
    R1 = 21;
    swi_call(6);

    /* R0 = low byte of the adjusted ID; R1 = bias_table[secondary_index]. */
    R0 = adjusted & 0xFF;
    R1 = LD8(A_byte_8120A20 + R1);

    RETURN_TO_CALLER();
}

/* ── countNaviFolderChipsWithElement_8120AE8 ── */
/* countNaviFolderChipsWithElement_8120AE8(r0 = element to match):
 * Counts how many chips in the current navi's regular folder have the given
 * element. Each navi has its own 30-entry (u16) folder inside the chip list
 * pointed to by oToolkit_S_Chip_2002178_Ptr; the navi's stat byte 0x2D picks
 * which 60-byte block. Returns the match count in r0.
 * r2/r3 are left holding whatever getChip8021DA8 left on its last call,
 * exactly as in the original; r4-r7 are preserved (pushed/popped). */
Next countNaviFolderChipsWithElement_8120AE8(void) {
    u32 wanted = R0;

    /* Folder base = chip list + (current navi's stat 0x2D) * 60 bytes. */
    GUEST_CALL(GetCurPETNavi);              /* r0 = current navi id */
    R1 = 0x2D;
    GUEST_CALL(GetCurPETNaviStatsByte);     /* r0 = navi's folder selector */
    u32 folder = LD32(R10 + oToolkit_S_Chip_2002178_Ptr) + R0 * 0x3C;

    u32 count = 0;
    for (u32 i = 0; i < 0x3C; i += 2) {
        R0 = LD16(folder + i);                  /* packed folder entry */
        GUEST_CALL(split9BitsFromBitfield_8021AE0); /* r0 = chip id */
        GUEST_CALL(getChip8021DA8);             /* r0 = &ChipData */
        if (LD8(R0 + 7) == wanted)              /* ChipData+7 = element */
            count++;
    }

    R0 = count;
    RETURN_TO_CALLER();
}

/* ── sub_8120B54 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; bool z, n; } call_notZero_eByte200AD04_result;
/* register form: generated wrapper: notZero_eByte200AD04 has no typed C interface yet */
static inline call_notZero_eByte200AD04_result call_notZero_eByte200AD04(void) {
    GUEST_CALL(notZero_eByte200AD04);
    return (call_notZero_eByte200AD04_result){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r4, r5, r6; } call_sub_803F674_result;
/* register form: generated wrapper: sub_803F674 has no typed C interface yet */
static inline call_sub_803F674_result call_sub_803F674(void) {
    GUEST_CALL(sub_803F674);
    return (call_sub_803F674_result){ R0, R4, R5, R6 };
}

/* register form: generated wrapper: GetBattleEffects has no typed C interface yet */
static inline u32 call_GetBattleEffects(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

typedef struct { u32 r1; } call_sub_8144D30_result;
/* register form: generated wrapper: sub_8144D30 has no typed C interface yet */
static inline call_sub_8144D30_result call_sub_8144D30(void) {
    GUEST_CALL(sub_8144D30);
    return (call_sub_8144D30_result){ R1 };
}

typedef struct { u32 r1, r2; } call_link_getSlotState_803F740_result;
/* register form: generated wrapper: link_getSlotState_803F740 has no typed C interface yet */
static inline call_link_getSlotState_803F740_result call_link_getSlotState_803F740(u32 r0) {
    R0 = r0;
    GUEST_CALL(link_getSlotState_803F740);
    return (call_link_getSlotState_803F740_result){ R1, R2 };
}

/*
 * sub_8120B54: determine the style-override code for a chip.
 *
 * Checks a RAM guard byte, a battle-state value, and the slot state,
 * then scans a fixed table to map the chip value (r2) to a small code
 * stored in a result table.
 *
 * Outputs r0 = the code, r1/r2 = from callee wrappers,
 * r3 = column index found (or input value on short-circuit paths).
 */

/* Fixed table addresses in ROM. */
#define A_byte_8120BB4 ((u32)0x08120BB4)  /* base-key lookup table (6 u32 entries) */
#define A_byte_8120BCC ((u32)0x08120BCC)  /* result table (6 u32 entries, last = 0xFF) */

typedef struct { u32 r0; u32 r3; } style_result_t;

static style_result_t compute_style_override(u32 chip_value, u32 in_r3) {
    style_result_t res;
    res.r3 = in_r3; /* pass-through on short-circuit paths */

    /* Guard: byte_200AD04.  Z set (byte == 0) => guard clear, skip to default. */
    call_notZero_eByte200AD04_result notzero_res = call_notZero_eByte200AD04();
    if (notzero_res.z) {
        res.r0 = LD32(A_byte_8120BCC + 0x14);
        return res;
    }

    /* Battle phase: sub_803F674 returns state >= 2 means late battle. */
    call_sub_803F674_result state_res = call_sub_803F674();
    if ((s32)state_res.r0 >= 2) {
        /* Check battle-effects flag bit 3 (effect active). */
        u32 effects = call_GetBattleEffects(); /* reads r10, returns r0 */
        if ((effects & 8) == 0) {
            /* No active effect: query alternate table via sub_8144D30, then default. */
            call_sub_8144D30();
            res.r0 = LD32(A_byte_8120BCC + 0x14);
            return res;
        }
    }

    /* Slot check: style-swap slot 4 (r0 = 4). */
    call_link_getSlotState_803F740_result slot_res = call_link_getSlotState_803F740(4);
    if (slot_res.r1 != 0) {
        res.r0 = LD32(A_byte_8120BCC + 0x14);
        return res;
    }

    /* Scan the base-key table: 6 rows × 15 columns.
     * Each row's entry is a base chip value; column offset indexes within 15 consecutive chips.
     * Row offsets into byte_8120BB4: 0, 4, 8, 12, 16, 20 (loop bound 0x18 = 24).
     * Match returns the corresponding entry from byte_8120BCC at the same row offset. */
    for (u32 row_off = 0; row_off < 0x18; row_off += 4) {
        u32 base = LD32(A_byte_8120BB4 + row_off);
        for (u32 col = 0; col < 15; col++) {
            if (base + col == chip_value) {
                res.r0 = LD32(A_byte_8120BCC + row_off);
                res.r3 = col;
                return res;
            }
        }
    }

    /* No match in any row: r3 = 15 (last inner-loop value), lookup at offset 0x14. */
    res.r0 = LD32(A_byte_8120BCC + 0x14);
    res.r3 = 15;
    return res;
}

/* sub_8120B54(r2 = chip_value, r10 = toolkit ptr for GetBattleEffects, lr = caller)
 * -> r0 = style code, r3 = column found (or unchanged), r1/r2/r4/r5/r7/lr left by callees */
Next sub_8120B54(void) {
    style_result_t result = compute_style_override(R2, R3);
    R0 = result.r0;
    R3 = result.r3;
    RETURN_TO_CALLER();
}

/* ── gfx_fillTilemapConsecutiveBanded_8120C94 ── */
/* gfx_fillTilemapConsecutiveBanded_8120C94(r0 = destination, r1 = width in halfwords, r2 = number of
 * 2-row bands, r3 = first value):
 *
 * Zero-fills the destination area (2*width halfwords per band), then fills
 * it with the consecutive halfword values r3, r3+1, r3+2, ... in row-major
 * order.  The fill is written one band at a time: each pass fills a row and
 * the row directly below it, alternating between the two so the values run
 * consecutively down each column pair, then skips ahead two rows.
 *
 * r4-r7 are preserved; r0-r3 and the flags are scratch. */
Next gfx_fillTilemapConsecutiveBanded_8120C94(void) {
    u32 dst = R0;
    u32 width = R1;   /* halfwords per row */
    u32 bands = R2;   /* number of 2-row bands to fill */
    u32 value = R3;   /* first index written */

    /* Clear the whole area first; the size is counted in halfwords
     * (2*width per band).  The fill loop below overwrites it all. */
    R1 = width * 2 * bands;
    GUEST_CALL(ZeroFillByHalfword);

    u32 rowBytes = width * 2;   /* byte width of one row */
    u32 p = dst;

    /* Note: like the original, the band loop body always runs at least
     * once, even when bands == 0 (it still fills one band). */
    u32 band = 0;
    do {
        /* Fill this row and the next one together. */
        u32 col = 0;
        u32 nextRow = rowBytes;   /* offset of the same column one row down */
        do {
            ST16(p + col, value);
            value++;
            ST16(p + nextRow, value);
            value++;
            col += 2;
            nextRow = rowBytes + col;
        } while ((s32)col < (s32)rowBytes);

        /* Advance past the two rows just filled. */
        p += nextRow;
        band++;
    } while ((s32)band < (s32)bands);

    SET_FLAGS_CMP(band, bands);
    RETURN_TO_CALLER();
}

/* ── sub_8120CC8 ── */
/* Read one byte out of a text/data buffer at the given index. A 0xFF byte
 * marks the end of the data: when the caller's index lands on it, the read
 * wraps around to the buffer's first byte and the index reported back to the
 * caller becomes 0; otherwise the requested index passes through untouched.
 * r2/r3 come out as copies of the buffer pointer and index (the original's
 * "mov r2, r1 / mov r3, r0"). */
typedef struct {
    u32 byte;   /* value that ends up in r1 */
    u32 index;  /* value that ends up in r0 */
    u32 tested; /* first byte read; the operand of the original's cmp r1, #0xFF */
} text_byte_result;

static text_byte_result read_text_byte(u32 index, u32 buf) {
    text_byte_result res;
    u32 first = LD8(buf + index);

    res.tested = first;
    if (first == 0xFF) {
        res.byte = LD8(buf + 0); /* wrap around to the start of the buffer */
        res.index = 0;
    } else {
        res.byte = first;
        res.index = index;
    }
    return res;
}

/* sub_8120CC8(r0 = index, r1 = buffer) ->
 *   r0 = index (0 when the slot held the 0xFF end marker), r1 = byte read,
 *   r2 = buffer, r3 = index; C/V as the original's cmp r1, #0xFF leaves them
 *   (C set iff the first byte read was 0xFF, V clear). */
Next sub_8120CC8(void) {
    R2 = R1;
    R3 = R0;
    text_byte_result res = read_text_byte(R0, R1);
    R0 = res.index;
    R1 = res.byte;
    SET_FLAGS_CMP(res.tested, 0xFF);
    RETURN_TO_CALLER();
}

/* ── clearWorkArea_200DCF0_8121144 ── */
/* clearWorkArea_200DCF0_8121144: clear the 0x18-halfword work area at word_200DCF0.
 * Arguments in r2-r7 are unused; they (and the carry/overflow flags left by
 * the zero-fill routine) pass through unchanged. */
Next clearWorkArea_200DCF0_8121144(void) {
    R0 = A_word_200DCF0;
    R1 = 0x18;
    GUEST_CALL(ZeroFillByHalfword);
    RETURN_TO_CALLER();
}
