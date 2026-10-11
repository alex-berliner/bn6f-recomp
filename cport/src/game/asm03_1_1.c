/*
 * asm03_1_1.c: readable functions of bn6f asm03_1_1.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_803A7B8 ── */
/* sub_803A7B8: scene/audio/display init dispatcher.
 * Calls a sequence of subsystems to set up battle/screen state:
 *   - sub_803BA64 / sub_803BA74 (unspecified subsystems)
 *   - sub_803BAE2(0)              (reset/clear some channel)
 *   - sub_803B522(0x0A); sub_803B674(0x26)  (load+play sound group 0x26)
 *   - sub_803B522(0x0B); sub_803B674(0x27)  (load+play sound group 0x27)
 *   - sub_803A8B4(0)              (some display/object setup)
 *   - sub_803A8E2()               (some display/object setup)
 * Inputs:  r5, r10, lr
 * Outputs: r5, r8, r9, r10, r11, r12 (other clobbers per contract) */
Next sub_803A7B8(void) {
    GUEST_CALL(sub_803BA64);
    GUEST_CALL(sub_803BA74);

    R0 = 0;
    GUEST_CALL(sub_803BAE2);

    R0 = 0x0A;
    GUEST_CALL(sub_803B522);

    R0 = 0x26;
    GUEST_CALL(sub_803B674);

    R0 = 0x0B;
    GUEST_CALL(sub_803B522);

    R0 = 0x27;
    GUEST_CALL(sub_803B674);

    R0 = 0;
    GUEST_CALL(sub_803A8B4);

    GUEST_CALL(sub_803A8E2);

    RETURN_TO_CALLER();
}

/* ── sub_803A7EC ── */
Next sub_803A7EC(void) {
    GUEST_CALL(sub_803BA64);
    GUEST_CALL(sub_803BA74);

    R0 = 1;
    GUEST_CALL(sub_803BAE2);

    R0 = 0xA;
    GUEST_CALL(sub_803B522);

    R0 = 0x26;
    GUEST_CALL(sub_803B674);

    R0 = 0xB;
    GUEST_CALL(sub_803B522);

    R0 = 0x27;
    GUEST_CALL(sub_803B674);

    R0 = 0;
    GUEST_CALL(sub_803A8B4);

    GUEST_CALL(sub_803A8E2);

    RETURN_TO_CALLER();
}

/* ── sub_803A820 ── */
/* sub_803A820: scene/sequence init for a battle-related module.
 * Calls a pair of setup helpers, registers two entries (IDs 10/0x26 and 11/0x27),
 * then runs the main init pair with a 0 argument. */
Next sub_803A820(void) {
    GUEST_CALL(sub_803BA64);
    GUEST_CALL(sub_803BA74);

    R0 = 0x0A;
    GUEST_CALL(sub_803B522);
    R0 = 0x26;
    GUEST_CALL(sub_803B674);

    R0 = 0x0B;
    GUEST_CALL(sub_803B522);
    R0 = 0x27;
    GUEST_CALL(sub_803B674);

    R0 = 0;
    GUEST_CALL(sub_803A8B4);
    GUEST_CALL(sub_803A8E2);

    RETURN_TO_CALLER();
}

/* ── sub_803A886 ── */
/* sub_803A886(r0 = step-index field, r1 = countdown field, r5 = object base):
 *   Advance a multi-step table-driven action. If the step index is non-zero,
 *   tick the countdown and step through the {word, byte} pairs at off_803A910
 *   (8 bytes per entry). When the countdown expires, advance the index and
 *   either reload the next countdown from the table or clear both fields if
 *   the table entry's first word is zero (end of sequence). */
Next sub_803A886(void) {
    u8 index = LD8(R5 + R0);
    SET_FLAGS_NZ(index);

    if (index != 0) {
        u8 counter = LD8(R5 + R1);
        counter -= 1;
        SET_FLAGS_CMP((u32)counter, 1);
        ST8(R5 + R1, counter);

        /* Original `bgt` after `sub r2, #1` skips when the 32-bit signed
         * result > 0. For a zero-extended byte minus 1, only the original
         * values 0 (result 0xFFFFFFFF) and 1 (result 0) are non-positive;
         * everything in [2..0xFF] takes the branch. After storing, the
         * post-decrement counter is therefore 0xFF (was 0) or 0 (was 1)
         * exactly when the step should advance. */
        if (counter == 0 || counter == 0xFF) {
            u8 newIndex = index + 1;
            ST8(R5 + R0, newIndex);

            u32 slotAddr = A_off_803A910 + ((u32)newIndex << 3);
            u32 firstWord = LD32(slotAddr);
            SET_FLAGS_NZ(firstWord);

            if (firstWord == 0) {
                ST8(R5 + R0, 0);
                ST8(R5 + R1, 0);
            } else {
                ST8(R5 + R1, (u8)LD32(slotAddr + 4));
            }
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_803AA24 ── */
/* sub_803AA24: Exit-submenu handler.
 * - inputs:  r2, r5, lr
 * - outputs: r5, r10
 * If sub_803C068(0x10, 0x30) returns non-zero (success), clears [r5+0x13],
 * plays the exit-submenu sound, then runs sub_803A84E. */
Next sub_803AA24(void) {
    R0 = 0x10;
    R1 = 0x30;
    GUEST_CALL(sub_803C068);
    if (!ZERO_FLAG()) {
        ST8(R5 + 0x13, 0);
        R0 = SOUND_EXIT_SUBMENU;
        GUEST_CALL(PlaySoundEffect);
        GUEST_CALL(sub_803A84E);
    }
    RETURN_TO_CALLER();
}

/* ── sub_803AA40 ── */
/* sub_803AA40: dispatch on a battle-object substate byte ([r5+8]).
 * Inputs:  r2 r4 r5 r10 lr
 * Outputs: r5 r8 r9 r10 r11 r12
 * Clobbers: nz c v r0 r1 r2 r3 lr
 * Preserved: r4 r5 r6 r7
 *
 * Reads the substate byte and selects one of eight handlers via a Thumb-target
 * jump table (entries are stored as address|1 so BX switches to Thumb). */
Next sub_803AA40(void) {
    u8 state = LD8(R5 + 8);
    u32 slot;
    if      (state == 0x3F) slot = 0;
    else if (state == 0x40) slot = 1;
    else if (state == 0x3C) slot = 2;
    else if (state == 0x3A) slot = 3;
    else if (state == 0x3B) slot = 4;
    else if (state == 0x3D) slot = 5;
    else if (state == 0x3E) slot = 6;
    else                    slot = 7;

    /* Handlers are stored as (address | 1) so BX enters Thumb mode. */
    u32 handler = LD32(A_off_803AA80 + slot * 4);
    GUEST_CALL_ADDR(handler);
    RETURN_TO_CALLER();
}

/* ── sub_803AAA0 ── */
Next sub_803AAA0(void) {
    /* Try to perform the menu select action (0x30). If it succeeds, play the
     * menu select sound, set state 0x16, and queue the success animation
     * parameters (0x30, 0x70, 0x20) before writing 0x18 to r5+1.
     * Otherwise, play the "can't jack in" sound, set state 0x14, and write
     * 0x1C to r5+1. */

    R0 = 0x30;
    GUEST_CALL(sub_803C248);
    if (ZERO_FLAG()) {
        R0 = SOUND_MENU_SELECT;
        GUEST_CALL(PlaySoundEffect);

        R0 = 0x16;
        GUEST_CALL(sub_803BB2C);

        R0 = 0x30;
        R1 = 0x70;
        R2 = 0x20;
        GUEST_CALL(sub_803C0F6);

        ST8(R5 + 1, 0x18);
    } else {
        R0 = SOUND_CANT_JACK_IN;
        GUEST_CALL(PlaySoundEffect);

        R0 = 0x14;
        GUEST_CALL(sub_803BB2C);

        ST8(R5 + 1, 0x1C);
    }
    RETURN_TO_CALLER();
}

/* ── sub_803ABB4 ── */
/* sub_803ABB4(r2, r5 = some object pointer, lr):
 * Shop/buy-menu confirm handler. If the object's current cursor index
 * (offset 0x10) is below its stored count (offset 0x24), play the menu
 * select sound, clear offset 0x13, then run the menu-flow callbacks
 * sub_803BF70(0x10, 0x30) and sub_803A85C. Otherwise delegate to
 * sub_803ABDC. Returns nothing on the guest stack; output register r10
 * is preserved by the caller contract. */
Next sub_803ABB4(void) {
    u8  cursor     = LD8(R5 + 0x10);
    u16 totalSlots = LD16(R5 + 0x24);

    if (cursor < totalSlots) {
        R0 = SOUND_MENU_SELECT;
        GUEST_CALL(PlaySoundEffect);

        ST8(R5 + 0x13, 0);

        R0 = 0x10;
        R1 = 0x30;
        GUEST_CALL(sub_803BF70);

        GUEST_CALL(sub_803A85C);
    } else {
        GUEST_CALL(sub_803ABDC);
    }

    RETURN_TO_CALLER();
}

/* ── sub_803ABDC ── */
/* sub_803ABDC(r2, r5 = object ptr, lr):
 * If [r5+8] != 0x3F, play the cur-move sound, clear [r5+0x13] and set
 * [r5+8] = 0x3F (mark this battle object as finished/cur-done). */
Next sub_803ABDC(void) {
    u8 state = LD8(R5 + 8);
    SET_FLAGS_CMP((u32)state, 0x3F);
    if (!ZERO_FLAG()) {
        R0 = SOUND_CUR_MOVE_80;
        GUEST_CALL(PlaySoundEffect);

        R0 = 0;
        ST8(R5 + 0x13, R0);
        R0 = 0x3F;
        ST8(R5 + 8, R0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_803AD20 ── */
/* sub_803AD20: dispatcher for "battle chip select" sub-states.
 *
 *   The chip-slot state byte at r5+8 selects one of seven handlers via
 *   the table at off_803AD60; any unrecognised value falls through to
 *   the last entry (sub_803AE98). The original tail-calls the chosen
 *   handler, so we save the real caller's LR (which GUEST_CALL will
 *   clobber) and restore it before returning. r4-r7 are preserved by the
 *   handlers themselves (AAPCS callee-saved).
 */
Next sub_803AD20(void) {
    u32 caller_lr = LR;
    u8  state = LD8(R5 + 8);

    /* Tail-dispatch to the handler chosen by the state byte. */
    switch (state) {
    case 0x3F: GUEST_CALL(sub_803AD80); break;
    case 0x40: GUEST_CALL(sub_803ADB8); break;
    case 0x3C: GUEST_CALL(sub_803ADDA); break;
    case 0x3A: GUEST_CALL(sub_803AE04); break;
    case 0x3B: GUEST_CALL(sub_803AE2A); break;
    case 0x3D: GUEST_CALL(sub_803AE50); break;
    case 0x3E: GUEST_CALL(sub_803AE78); break;
    default:   GUEST_CALL(sub_803AE98); break;
    }

    /* Restore the real caller's return address before returning. */
    LR = caller_lr;
    RETURN_TO_CALLER();
}

/* ── sub_803AD80 ── */
/* sub_803AD80: menu-input handler.
 * Tries to perform action 0x90 (sub_803C248). On success, plays the menu-select cue,
 * triggers a related menu command (0x17), writes sub-menu coords (0x90, 0xD0, 0x20),
 * and sets r5[1] = 0x34. On failure, plays the can't-jack-in cue, triggers 0x15,
 * and sets r5[1] = 0x38.
 *
 *   inputs:  r2 r4 r5 r10 lr
 *   outputs: r10
 *   clobbers (unused by callers): nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r5 r6 r7 r10
 */
Next sub_803AD80(void) {
    R0 = 0x90;
    GUEST_CALL(sub_803C248);
    if (ZERO_FLAG()) {
        /* Action 0x90 succeeded: confirm and advance the menu. */
        R0 = SOUND_MENU_SELECT;
        GUEST_CALL(PlaySoundEffect);

        R0 = 0x17;
        GUEST_CALL(sub_803BB2C);

        R0 = 0x90;
        R1 = 0xD0;
        R2 = 0x20;
        GUEST_CALL(sub_803C0F6);

        ST8(R5 + 1, 0x34);
    } else {
        /* Action 0x90 failed: deny. */
        R0 = SOUND_CANT_JACK_IN;
        GUEST_CALL(PlaySoundEffect);

        R0 = 0x15;
        GUEST_CALL(sub_803BB2C);

        ST8(R5 + 1, 0x38);
    }
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_803AE98 ── */
/* sub_803AE98: Menu "select" handler for some list element (R5 = element).
 * If element->field_0x11 < element->field_0x26, play a select sound, clear
 * field_0x13, then call sub_803BF70(0x11, 0x90) and sub_803A878().
 * Otherwise, fall through to sub_803AEC0().
 * Outputs r10 (preserved by this function's preserved set). */
Next sub_803AE98(void) {
    u8  current = LD8(R5 + 0x11);
    u16 target  = LD16(R5 + 0x26);

    if (current < target) {
        R0 = SOUND_MENU_SELECT;
        GUEST_CALL(PlaySoundEffect);
        ST8(R5 + 0x13, 0);
        R0 = 0x11;
        R1 = 0x90;
        GUEST_CALL(sub_803BF70);
        GUEST_CALL(sub_803A878);
    } else {
        GUEST_CALL(sub_803AEC0);
    }

    RETURN_TO_CALLER();
}

/* ── sub_803BF70 ── */
/* sub_803BF70(r0 = offset, r1 = ?, r5 = obj, lr)
 *   outputs: r5
 *   clobbers: nz c v r0 r1
 *   preserved: r4 r6 r7
 *
 * Appends one 16-bit entry to an animation/frame script for the object at r5.
 * r0 selects which field of r5 holds the write index (low byte), and r1 is an
 * additional base offset added to r5 when writing the halfword.
 */
Next sub_803BF70(void) {
    u32 offset = R0;
    u32 base = R1;
    u32 obj = R5;

    /* Pointer table at off_803C208: indexed by [r5+0x12] * 0x10, then a row
     * picked by [r5+8] * 8; the value we want is at +2 in that row. */
    u32 row = LD8(obj + 0x12);
    u32 tbl = LD32(A_off_803C208 + row * 0x10);
    u32 col = LD8(obj + 8);
    u16 entry = LD16(tbl + col * 8 + 2);

    /* Write the entry as a halfword at (r5 + r1) + ([r5+r0] << 1). */
    u8 idx = LD8(obj + offset);
    ST16((u32)idx * 2 + obj + base, entry);

    /* Bump the index. */
    u8 newIdx = idx + 1;
    ST8(obj + offset, newIdx);

    RETURN_TO_CALLER();
}

/* ── sub_803C248 ── */
/* sub_803C248(r0 = table base, r5 = index):
 * Walk a two-byte-stride table starting at r0+r5, examining the first byte
 * of each entry until a 0xE6 terminator is reached.
 *  - 0x00 and 0xE5 entries are treated as empty and skipped.
 *  - Any other byte marks the slot as occupied and stops the scan.
 *  - On exit, R0 is 1 if all visited entries were empty, 0 otherwise.
 * R4 and R5 must end with their original values (caller contract). */
Next sub_803C248(void) {
    u32 cursor = (u32)R0 + (u32)R5;
    u32 result = 1;

    for (;;) {
        u8 b = LD8(cursor);
        cursor += 2;
        if (b == 0xE6) break;
        if (b == 0xE5) continue;
        if (b == 0x00) continue;
        result = 0;
        break;
    }

    SET_FLAGS_NZ(result);
    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_803C352 ── */
/* Clear the 0x48-byte state block at A_word_2006770, fill two 0x20-byte regions with
 * 0xE5E5 halfwords, write the terminator byte 0xE6 after each region, then call
 * sub_802D150. */

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word(void *dst, u32 num_bytes) {
    R0 = (u32)dst;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls HalfwordFill outside this file, which has no typed C interface yet */
static void halfword_fill(void *dst, u32 num_halfwords, u32 pattern) {
    R0 = (u32)dst;
    R1 = num_halfwords;
    R2 = pattern & 0xFFFF;
    GUEST_CALL(HalfwordFill);
}

/* register form: calls sub_802D150 outside this file, which has no typed C interface yet */
static void call_sub_802D150(void) {
    GUEST_CALL(sub_802D150);
}

/* Initialize the state block: zero it, fill two halfword regions, write terminator
 * bytes, then invoke sub_802D150 via the register-form helper. */
static void init_state_block(void) {
    void *block = (void *)A_word_2006770;
    void *region_a = (u8 *)block + 0x08;
    void *region_b = (u8 *)block + 0x28;

    zero_fill_by_word(block, 0x48);
    halfword_fill(region_a, 0x20, 0xE5E5);
    ST8((u32)region_a + 0x10, 0xE6);
    halfword_fill(region_b, 0x20, 0xE5E5);
    ST8((u32)region_b + 0x16, 0xE6);

    call_sub_802D150();
}

/* sub_803C352: clear & prefill the state block at A_word_2006770, then call sub_802D150.
 * The original's pop {pc} returns directly to the caller, so the C/V outputs are
 * whatever sub_802D150 leaves behind. */
Next sub_803C352(void) {
    init_state_block();
    RETURN_TO_CALLER();
}

/* ── sub_803C3E0 ── */
/* sub_803C3E0: test the flag byte at word_2006770+2 and return it.
 * r0 = flag byte; Z set when it is zero, N set when bit 7 is set. */
Next sub_803C3E0(void) {
    u32 flag = LD8(A_word_2006770 + 2);
    R0 = flag;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── keyItems_ResetInventoryAndGrantScenarioItems_803CD74 ── */
/* keyItems_ResetInventoryAndGrantScenarioItems_803CD74: wipe the key-item inventory block (0x190 bytes) and re-grant the
 * starting key items: four copies of item 0x75 and one of item 0.
 * R10 is the toolkit pointer. R0-R2 and flags are clobbered; R4-R7 and up are
 * left untouched, matching the original (it saves/restores R4-R7 itself). */
Next keyItems_ResetInventoryAndGrantScenarioItems_803CD74(void) {
    u32 keyItems = LD32(R10 + oToolkit_KeyItemsPtr);

    R0 = keyItems;
    R1 = 0x190; /* sizeof key-item inventory */
    GUEST_CALL(ZeroFillByWord);

    R0 = 0x75;
    R1 = 4;
    GUEST_CALL(keyItems_RemoveFromInventoryAndGive_803CDF8);

    R0 = 0;
    R1 = 1;
    GUEST_CALL(keyItems_RemoveFromInventoryAndGive_803CDF8);

    RETURN_TO_CALLER();
}

/* ── GiveItem ── */
/* register form: callee may modify R6/R7/LR; preserved regs save/restored via guest stack */
static void call_encryption_copy(u32 item_id, u32 quantity) {
    u32 sp = SP;
    SP -= 20;
    ST32(SP, R4); ST32(SP + 4, R5); ST32(SP + 8, R6); ST32(SP + 12, R7); ST32(SP + 16, LR);
    R0 = item_id;
    R1 = quantity;
    GUEST_CALL(encryption_copyByteXor6fToToolkitBuffer_8006e3c);
    R4 = LD32(SP); R5 = LD32(SP + 4); R6 = LD32(SP + 8); R7 = LD32(SP + 12); LR = LD32(SP + 16);
    SP = sp;
}

/* register form: reload callee needs R4/R6 as inputs and may clobber preserved regs */
static void call_reload_navi(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r6) {
    u32 sp = SP;
    SP -= 20;
    ST32(SP, R4); ST32(SP + 4, R5); ST32(SP + 8, R6); ST32(SP + 12, R7); ST32(SP + 16, LR);
    R0 = r0; R1 = r1; R2 = r2; R3 = r3; R4 = r4; R6 = r6;
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);
    R4 = LD32(SP); R5 = LD32(SP + 4); R6 = LD32(SP + 8); R7 = LD32(SP + 12); LR = LD32(SP + 16);
    SP = sp;
}

/* register form: pet_add callee needs R4/R6 as inputs and may clobber preserved regs */
static void call_pet_add(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r6) {
    u32 sp = SP;
    SP -= 20;
    ST32(SP, R4); ST32(SP + 4, R5); ST32(SP + 8, R6); ST32(SP + 12, R7); ST32(SP + 16, LR);
    R0 = r0; R1 = r1; R2 = r2; R3 = r3; R4 = r4; R6 = r6;
    GUEST_CALL(pet_addCurNaviStat0x3ECapped_803CF3C);
    R4 = LD32(SP); R5 = LD32(SP + 4); R6 = LD32(SP + 8); R7 = LD32(SP + 12); LR = LD32(SP + 16);
    SP = sp;
}

/* register form: sub_803CFB0 callee may clobber preserved regs */
static void call_post_item_hook(void) {
    u32 sp = SP;
    SP -= 20;
    ST32(SP, R4); ST32(SP + 4, R5); ST32(SP + 8, R6); ST32(SP + 12, R7); ST32(SP + 16, LR);
    GUEST_CALL(sub_803CFB0);
    R4 = LD32(SP); R5 = LD32(SP + 4); R6 = LD32(SP + 8); R7 = LD32(SP + 12); LR = LD32(SP + 16);
    SP = sp;
}

/* Add quantity of a key item to inventory, clamping to per-item maximum.
 * Returns: 0 = normal add, 1 = already at max (no change), 2 = capped at max. */
static u32 give_item_GiveItem(u32 item_id, u32 quantity, u32 toolkit) {
    /* Copy/decrypt item data into toolkit working buffer (side-effect; R0/R1 restored after) */
    call_encryption_copy(item_id, quantity);

    u32 key_ptr = LD32(toolkit + oToolkit_KeyItemsPtr);

    /* Determine maximum stack size for this item slot */
    u32 max_count = 0xFF;
    if ((s32)item_id >= (s32)0x80 && (s32)item_id <= (s32)0x85) {
        max_count = LD8(key_ptr + 0x75);
    } else if (item_id == 0x71) {
        max_count = 2;
    } else if ((s32)item_id >= (s32)0x90) {
        max_count = 9;
    }

    /* Read current count and compute new count + result code */
    u32 current = LD8(key_ptr + item_id);
    u32 result;

    if (current == max_count) {
        result = 1;
    } else {
        result = 0;
        u32 added = current + quantity;
        if ((s32)added > (s32)max_count) {
            result = 2;
            added = max_count;
        }
        current = added;
    }

    ST8(key_ptr + item_id, current);

    /* Reload Navi stat boosts when item 0x71 (the Navi) changes */
    if (item_id == 0x71) {
        call_reload_navi(item_id, quantity, key_ptr, result, current, max_count);
    }

    /* Update PET stat tracking and fire post-item hook */
    call_pet_add(result, quantity, key_ptr, result, item_id, max_count);
    call_post_item_hook();

    return result;
}

/* GiveItem(r0 = item_id, r1 = quantity, r10 = toolkit_ptr) -> r0 = result_code */
Next GiveItem(void) {
    R0 = give_item_GiveItem(R0, R1, R10);
    RETURN_TO_CALLER();
}

/* ── keyItems_RemoveFromInventoryAndGive_803CDF8 ── */
/* keyItems_RemoveFromInventoryAndGive_803CDF8(r0 = key item slot index, r1 = passed through to GiveItem):
 * mark the given key item slot as empty (not owned) in the player's key
 * item inventory, then hand the item to the player via GiveItem.
 * Leaves r0..r3 as GiveItem left them. */
Next keyItems_RemoveFromInventoryAndGive_803CDF8(void) {
    u32 keyItems = LD32(R10 + oToolkit_KeyItemsPtr);

    ST8(keyItems + R0, 0);

    R2 = keyItems;
    R3 = 0;
    GUEST_CALL(GiveItem);

    RETURN_TO_CALLER();
}

/* ── SetCurNaviHPToFull ── */
/* SetCurNaviHPToFull: set the current PET navi's HP to its maximum.
 * Leaves r0 = 0 and Z set (N clear), matching the original's final "mov r0, #0". */
Next SetCurNaviHPToFull(void) {
    /* Which navi is currently in the PET? */
    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* Read its max HP... */
    R0 = navi;
    R1 = oNaviStats_MaxHP;
    GUEST_CALL(GetCurPETNaviStatsHword);
    u32 maxHP = R0;

    /* ...and store it as the current HP. */
    R0 = navi;
    R1 = oNaviStats_CurHP;
    R2 = maxHP;
    GUEST_CALL(SetCurPETNaviStatsHword);

    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── petnavi_setStat42ToStat3EPlusCounter_803CED4 ── */
/* petnavi_setStat42ToStat3EPlusCounter_803CED4:
 * Reads the current PET navi's stat 0x3E, adds the 16-bit value stored at
 * (*R10->oToolkit_Unk2004334_Ptr), and writes the sum into stat 0x42 of the
 * same navi.
 *
 * Register contract:
 *   inputs:    r4 r6 r7 r10 lr
 *   outputs:   r2 (the computed sum); r3 as left by SetCurPETNaviStatsHword
 *   preserved: r4 r6 r7 (saved/restored around the helper calls)
 *   clobbers:  r0 r1 and flags
 */
Next petnavi_setStat42ToStat3EPlusCounter_803CED4(void) {
    /* Original pushes r4/r6/r7 and pops them after the helper calls,
     * so their incoming values must survive everything below. */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Which navi is currently equipped on the PET. */
    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* Read stat 0x3E of that navi. */
    R0 = navi;
    R1 = 0x3E; /* stat id to read */
    GUEST_CALL(GetCurPETNaviStatsHword);
    u32 baseStat = R0;

    /* Add the 16-bit value tracked by the toolkit's oToolkit_Unk2004334_Ptr. */
    u32 extra = LD16(LD32(R10 + oToolkit_Unk2004334_Ptr));
    u32 total = baseStat + extra;

    /* Store the sum into stat 0x42. */
    R0 = navi;
    R1 = 0x42; /* stat id to write */
    R2 = total;
    GUEST_CALL(SetCurPETNaviStatsHword);

    /* Original leaves r2 = sum and restores r4/r6/r7 from the stack. */
    R2 = total;
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── pet_addCurNaviStat0x3ECapped_803CF3C ── */
/* pet_addCurNaviStat0x3ECapped_803CF3C(r1 = navi slot index; expects r4 == 0x70):
 * Adjusts one PET navi stat word: reads stat 0x3E of navi slot 0, adds
 * r1 * 20 (the per-slot stride), clamps the result to 1000, stores it back,
 * and calls sub_803CF74 to apply the same value/offset pair elsewhere.
 * All registers r4-r12 are preserved. */
Next pet_addCurNaviStat0x3ECapped_803CF3C(void) {
    u32 r4_in = R4, r5_in = R5, r6_in = R6, r7_in = R7;

    if (R4 != 0x70)
        RETURN_TO_CALLER();

    u32 slot_offset = 0x14 * R1; /* byte offset of the navi's own entry */

    /* read the current stat word (navi 0, stat 0x3E) */
    R0 = 0;
    R1 = 0x3E;
    R4 = 0;
    R6 = r6_in;
    R7 = slot_offset;
    GUEST_CALL(GetCurPETNaviStatsHword);

    u32 value = R0 + slot_offset;
    if ((s32)value > 0x3E8)
        value = 0x3E8; /* cap at 1000 */

    /* write the adjusted value back */
    R0 = 0;
    R1 = 0x3E;
    R2 = value;
    R4 = 0;
    R6 = value;
    R7 = slot_offset;
    GUEST_CALL(SetCurPETNaviStatsHword);

    /* propagate the new value (r0) and slot offset (r1) */
    R0 = value;
    R1 = slot_offset;
    R4 = 0;
    R5 = r5_in;
    R6 = value;
    R7 = slot_offset;
    GUEST_CALL(sub_803CF74);

    R4 = r4_in;
    R5 = r5_in;
    R6 = r6_in;
    R7 = r7_in;
    RETURN_TO_CALLER();
}

/* ── sub_803D02C ── */
/* register form: calls encryption_zenny_8006f78 outside this file, which has no typed C interface yet */
static bool call_encryption_78(u32 arg0, u32 arg1, u32 arg2, u32 arg3,
                               u32 arg4, u32 arg5, u32 arg6, u32 arg7,
                               u32 toolkit) {
    R0 = arg0; R1 = arg1; R2 = arg2; R3 = arg3;
    R4 = arg4; R5 = arg5; R6 = arg6; R7 = arg7;
    R10 = toolkit;
    GUEST_CALL(encryption_zenny_8006f78);
    return !ZERO_FLAG();   /* true ⇒ bne taken, check failed */
}

/* register form: calls encryption_zenny_8006f54 outside this file, which has no typed C interface yet */
static void call_encryption_54(u32 arg0, u32 arg1, u32 arg2, u32 arg3,
                               u32 arg4, u32 arg5, u32 arg6, u32 arg7,
                               u32 toolkit) {
    R0 = arg0; R1 = arg1; R2 = arg2; R3 = arg3;
    R4 = arg4; R5 = arg5; R6 = arg6; R7 = arg7;
    R10 = toolkit;
    GUEST_CALL(encryption_zenny_8006f54);
}

/* Write arg0 into ProtectedZenny and re-encrypt, but only when the
 * zenny integrity check (encryption_zenny_8006f78) signals success. */
static void protect_zenny(u32 arg0, u32 arg1, u32 arg2, u32 arg3,
                          u32 arg4, u32 arg5, u32 arg6, u32 arg7,
                          u32 toolkit) {
    if (call_encryption_78(arg0, arg1, arg2, arg3,
                           arg4, arg5, arg6, arg7, toolkit))
        return;

    /* R0 still holds arg0 (encryption_78 preserves it) */
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    ST32(game_state + oGameState_ProtectedZenny, arg0);
    call_encryption_54(arg0, game_state, arg2, arg3,
                       arg4, arg5, arg6, arg7, toolkit);
}

/* sub_803D02C(r0..r7 = caller state, r10 = Toolkit)
 *   -> r0..r7 preserved, r1 = last callee result, nz/c/v from callee */
Next sub_803D02C(void) {
    protect_zenny(R0, R1, R2, R3, R4, R5, R6, R7, R10);
    RETURN_TO_CALLER();
}

/* ── sub_803D06C ── */
/* register form: calls encryption_zenny_8006f78 outside this file, which has no typed C interface yet */
static bool encrypted_zenny_check_failed(u32 zenny, u32 arg1, u32 toolkit) {
    R0 = zenny; R1 = arg1; R10 = toolkit;
    GUEST_CALL(encryption_zenny_8006f78);
    return !ZERO_FLAG();          /* non-zero result → BNE taken → check failed */
}

/* Return the protected zenny (via toolkit → game state) when the encryption
 * check passes, or 0 when it fails.  Overwrites R1 with the game-state pointer
 * on the success path, matching the original's MOV/LDR sequence. */
static u32 protected_zenny_or_zero(u32 zenny, u32 arg1, u32 toolkit) {
    if (encrypted_zenny_check_failed(zenny, arg1, toolkit))
        return 0;
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    /* register form: the original overwrites r1 with the game-state pointer
       before using it to load the protected zenny */
    R1 = game_state;
    return LD32(game_state + oGameState_ProtectedZenny);
}

/* sub_803D06C(r0 = zenny, r1 = arg1, r10 = toolkit) -> r0 = protected zenny or 0 */
Next sub_803D06C(void) {
    R0 = protected_zenny_or_zero(R0, R1, R10);
    RETURN_TO_CALLER();
}

/* ── sub_803D0F4 ── */
/* register form: calls encryption_bugfrags_8006fd0 outside this file, which has no typed C interface yet */
static bool encryption_bugfrags_succeeded(u32 arg0, u32 arg1) {
    R0 = arg0;
    R1 = arg1;
    GUEST_CALL(encryption_bugfrags_8006fd0);
    return !ZERO_FLAG();  /* Z clear → non-zero return → encryption active */
}

typedef struct {
    u32 bugfrags;  /* r0: protected bugfrags count, or 0 if encryption active */
    u32 state_ptr; /* r1: game-state pointer (unchanged on the 0 path) */
} BugfragsCheck;

/* Return the protected-bugfrags value from the game state, or 0 if encryption is active. */
static BugfragsCheck get_protected_bugfrags(u32 arg0, u32 arg1, u32 toolkit) {
    if (encryption_bugfrags_succeeded(arg0, arg1))
        return (BugfragsCheck){ 0, arg1 };

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    return (BugfragsCheck){ LD32(game_state + oGameState_ProtectedBugfrags), game_state };
}

/* sub_803D0F4(r0, r1, r10 = toolkit) -> r0 (bugfrags or 0), r1 (game state ptr) */
Next sub_803D0F4(void) {
    BugfragsCheck res = get_protected_bugfrags(R0, R1, R10);
    R0 = res.bugfrags;
    R1 = res.state_ptr;
    RETURN_TO_CALLER();
}

/* ── logoScreen_dispatch_803D1CA ── */
/* logoScreen_dispatch_803D1CA:
 * Logo screen state machine tick. Reads the current state from
 * eLogoScreenState (offset 0) and invokes the matching state handler with
 * the state pointer in r5, then animates the "PRESS START" text.
 *
 * The state field holds a byte offset into the handler table (0x00, 0x04,
 * 0x08, 0x0C, 0x10) -- the original loads the entry with `ldr r0, [table, r1]`
 * with no shift -- so dispatch on that raw offset. Each handler is entered
 * with its own table entry (Thumb address, bit 0 set) in r0, the raw state
 * offset in r1 and the state pointer in r5, mirroring
 * `mov lr, pc; bx r0` in the original. Whatever r0/r1 the handler leaves is
 * carried into the startScreen call afterwards, since the original's `bl`
 * does not set up arguments.
 *
 * r4-r7 are saved around the calls (the original's push/pop), so the
 * caller's copies survive whatever the handlers do to them. */
Next logoScreen_dispatch_803D1CA(void) {
    u32 state = A_eLogoScreenState;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    u32 index = LD8(state);                     /* oLogoScreenState_Index_00 */
    u32 handler = LD32(A_off_803D1E4 + index);  /* table entry, Thumb bit set */

    R5 = state;
    R0 = handler;
    R1 = index;

    switch (index) {
    case 0x00: /* logo fade-in / init */
        GUEST_CALL(sub_803D1FC);
        break;
    case 0x04:
        GUEST_CALL(sub_803D24C);
        break;
    case 0x08:
        GUEST_CALL(sub_803D274);
        break;
    case 0x0C:
        GUEST_CALL(sub_803D298);
        break;
    case 0x10: /* done -- hand control back to the scene manager */
        GUEST_CALL(logoScreen_finish_803D2A6);
        break;
    }

    /* The state field only ever holds one of the five offsets above, so the
     * jump table never lands anywhere else. */
    GUEST_CALL(startScreen_AnimatePressStart_803E938);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_803D1FC ── */
/* register form: calls sub_80015FC outside this file, which has no typed C interface yet */
static void call_sub_80015FC(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_80015FC);
}

/* register form: calls SetRenderInfoLCDControl outside this file, which has no typed C interface yet */
static void call_SetRenderInfoLCDControl(u16 lcd_control, u32 fade_mode) {
    R4 = fade_mode;
    R0 = lcd_control;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* register form: calls renderInfo_8001788 outside this file, which has no typed C interface yet */
static void call_renderInfo_8001788(void) {
    GUEST_CALL(renderInfo_8001788);
}

/* register form: calls renderInfo_80017A0 outside this file, which has no typed C interface yet */
static void call_renderInfo_80017A0(void) {
    GUEST_CALL(renderInfo_80017A0);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void call_SetScreenFade(s32 fade_mode, s32 level) {
    R0 = fade_mode;
    R1 = level;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls resetAllGraphicsState_8005F40 outside this file, which has no typed C interface yet */
static void call_resetAllGraphicsState_8005F40(u32 fade_mode) {
    R4 = fade_mode;
    GUEST_CALL(resetAllGraphicsState_8005F40);
}

/* register form: calls runSetupStepAndClearWorkArea_8005F6C outside this file, which has no typed C interface yet */
static void call_runSetupStepAndClearWorkArea_8005F6C(void) {
    GUEST_CALL(runSetupStepAndClearWorkArea_8005F6C);
}

/* register form: calls sub_80027C4 outside this file, which has no typed C interface yet */
static void call_sub_80027C4(void) {
    GUEST_CALL(sub_80027C4);
}

/* register form: calls logoScreen_loadLogoTiles_803D2B8 outside this file, which has no typed C interface yet */
static void call_logoScreen_loadLogoTiles_803D2B8(void) {
    GUEST_CALL(logoScreen_loadLogoTiles_803D2B8);
}

/* register form: calls music_80005F2 outside this file, which has no typed C interface yet */
static void call_music_80005F2(u8 bg_music_indicator) {
    R0 = bg_music_indicator;
    GUEST_CALL(music_80005F2);
}

/* Initialise the logo/title screen: configure LCD control, fade in, reset graphics,
 * load logo tiles, start BGM, and advance the state machine to index 4.
 * Returns the fade mode (r4): 4 when the transition flag is set, 0xC otherwise. */
static u32 logoScreen_init(u32 state) {
    call_sub_80015FC(0xe);

    /* offset 5 flag selects fade mode and LCD bits */
    u8 transition_flag = LD8(state + oLogoScreenState_Unk_05);
    u32 fade_mode = (transition_flag != 0) ? 4 : 0xC;
    u16 lcd_control = (transition_flag != 0) ? 0xC0 : 0x40;

    call_SetRenderInfoLCDControl(lcd_control, fade_mode);
    call_renderInfo_8001788();
    call_renderInfo_80017A0();
    call_SetScreenFade(fade_mode, 0xFF);
    call_resetAllGraphicsState_8005F40(fade_mode);
    call_runSetupStepAndClearWorkArea_8005F6C();
    call_sub_80027C4();
    call_logoScreen_loadLogoTiles_803D2B8();

    /* Logo-screen BGM (indicator 0x63) */
    call_music_80005F2(0x63);

    /* Advance state to index 4 so logoScreen_dispatch_803D1CA triggers the next step */
    ST8(state + oLogoScreenState_Index_00, 4);

    return fade_mode;
}

/* sub_803D1FC(r5 = *LogoScreenState) -> r0=4, r4=fade_mode, nz/c/v */
Next sub_803D1FC(void) {
    R4 = logoScreen_init(R5);
    R0 = 4;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_803D274 ── */
/* LogoScreenState field offsets */
#define oLogoScreenState_Index_00_sub_803D274   0   /* u8: dispatch state index */
#define oLogoScreenState_Counter    4   /* u8: fade countdown */

#define FADE_NEXT_DURATION  0xC
#define FADE_NEXT_STRENGTH  0x10

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool fade_not_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();              /* Z=1 → fade not active, beq would be taken */
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void start_next_fade(void) {
    R0 = FADE_NEXT_DURATION;
    R1 = FADE_NEXT_STRENGTH;
    GUEST_CALL(SetScreenFade);
}

/*
 * Tick the logo-screen fade countdown.
 *
 * If the screen fade is not active, or the decremented counter is still
 * positive, the countdown simply stops.  Otherwise the next fade is
 * started and the dispatch index is advanced.
 *
 * Returns the final R0 value for the caller:
 *   – 0x00 on "fade not active" (pass through IsScreenFadeActive's R0;
 *     the helper's GUEST_CALL already set r1-r3 / c / v).
 *   – the decremented counter (1..254) on "still counting" (r1-r3 / c / v
 *     remain from IsScreenFadeActive's GUEST_CALL).
 *   – 0x0C on "countdown expired" (r1-r3 / c / v remain from
 *     SetScreenFade's GUEST_CALL; flags are callee-owned).
 */
static u32 logo_fade_tick(u32 state) {
    if (fade_not_active())
        return 0;

    /* Decrement the u8 countdown at state+4. */
    u32 counter = LD8(state + oLogoScreenState_Counter) - 1;
    ST8(state + oLogoScreenState_Counter, counter);

    if ((s32)counter > 0)            /* still counting – stop here */
        return counter;

    /* Countdown expired: chain the next fade stage. */
    start_next_fade();
    ST8(state + oLogoScreenState_Index_00_sub_803D274, FADE_NEXT_DURATION);

    return FADE_NEXT_DURATION;       /* 0x0C */
}

/* sub_803D274(r5 = LogoScreenState*) -> (r0, nz c v, r1 r2 r3, lr) */
Next sub_803D274(void) {
    u32 r0 = logo_fade_tick(R5);
    R0 = r0;
    SET_FLAGS_NZ(r0);
    RETURN_TO_CALLER();
}

/* ── sub_803D298 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static u32 call_is_screen_fade_active_sub_803D298(bool *fade_active) {
    GUEST_CALL(IsScreenFadeActive);
    *fade_active = ZERO_FLAG();
    return R0;
}

/* Skip the logo-finish trigger while a screen fade is running.
 * When the fade is active, return IsScreenFadeActive's r0 unchanged (the beq path).
 * Otherwise write 0x10 into the logo-screen index to queue logoScreen_finish. */
static u32 logo_screen_update(u32 self) {
    bool fade_active;
    u32 r0 = call_is_screen_fade_active_sub_803D298(&fade_active);
    if (fade_active)
        return r0;
    ST8(self + oLogoScreenState_Index_00, 0x10);
    return 0x10;
}

/* sub_803D298(r5 = LogoScreenState ptr) -> r0 r1 r3 lr, nz c v */
Next sub_803D298(void) {
    R0 = logo_screen_update(R5);
    RETURN_TO_CALLER();
}

/* ── logoScreen_finish_803D2A6 ── */
/* logoScreen_finish_803D2A6(self: r5 = *const LogoScreenState):
 * Called when the logo screen finishes: switch the LCD back to the normal
 * render mode (0x40) and reinitialize the shared screen-start state.
 * Preserves r2/r3; returns with r0/r1 and flags as the last callee left them. */
Next logoScreen_finish_803D2A6(void) {
    R0 = 0x40;
    GUEST_CALL(SetRenderInfoLCDControl);
    GUEST_CALL(startScreen_init_802F530);
    RETURN_TO_CALLER();
}

/* ── logoScreen_loadLogoTiles_803D2B8 ── */
Next logoScreen_loadLogoTiles_803D2B8(void) {
    /* The original pushes {r4-r7, lr} and pops them at the end, so r4-r7
     * must be preserved across this routine even though we use them as
     * scratch to pass arguments to CopyBackgroundTiles. */
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    /* Clear VRAM and the GFX scratch buffer so the logo screen starts clean. */
    GUEST_CALL(zeroFillVRAM);
    GUEST_CALL(ZeroFillGFX30025c0);

    /* Decompress the Capcom logo tileset/tilemap (and load both palettes
     * plus the license-message tileset) via the initRefs table. */
    R0 = A_initRefs803D2F0;
    GUEST_CALL(decompAndCopyData);

    /* Copy the Capcom logo tilemap into tile block 1.
     * Args: j, i, which_tile_block_32x32, tile_ids, j_size, i_size. */
    R0 = 0;
    R1 = 0;
    R2 = 1;
    R3 = A_eDecompBuffer2013A00 + 4;
    R4 = 0x20;
    R5 = 0x14;
    GUEST_CALL(CopyBackgroundTiles);

    /* Copy the license-message tilemap into tile block 2. */
    R0 = 8;
    R1 = 0x11;
    R2 = 2;
    R3 = A_CapcomLogoLicenseTilemap_86C41B4;
    R4 = 0xE;
    R5 = 1;
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore the callee-saved registers the prologue preserved. */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_803DCE8 ── */
/* Camera/screen state. Offsets are taken straight from the disassembly so the typed
 * function matches the original byte-for-byte; the field names follow the original
 * comments (eCamera.unk_*) since they are internal camera state. */
#define oCamera_unk_50   0x00
#define oCamera_unk_51   0x01
#define oCamera_unk_52   0x02
#define oCamera_unk_53   0x03
#define oCamera_unk_55   0x05
#define oCamera_unk_58   0x08
#define oCamera_unk_60   0x10
#define oCamera_unk_62   0x12
#define oCamera_unk_64   0x14
#define oCamera_unk_66   0x16
#define oCamera_unk_68   0x18
#define oCamera_unk_70   0x20
#define oCamera_unk_74   0x24
#define oCamera_unk_84   0x34
#define oCamera_unk_88   0x38
#define oCamera_unk_8C   0x3C

/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet */
static void zero_fill_by_byte(void *dst, u32 num_bytes) {
    R0 = (u32)dst;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByByte);
}

/* Initialise the camera/screen state block pointed at by eLinkState.
 *   ptr       -> eCamera.unk_70 / .unk_74 (destination pointer)
 *   layout_ix <<= 4 picks a config record from off_803DC78:
 *                [0]   -> .unk_84
 *                [4]   -> .unk_88  (zero-fill target #1)
 *                [8]   -> .unk_8C  (zero-fill target #2)
 *                [0xC]  -> .unk_50 / .unk_51 (also written to 5/8/0x18 as 0 and
 *                          to 0x10/0x12/0x14/0x16 as 0xFFFF).
 *   r2 -> .unk_53 (u8), r3 -> .unk_52 (u8). */
static void camera_init(u32 ptr, u32 layout_ix, u32 r2, u32 r3) {
    u32 cam = A_eLinkState;

    ST32(cam + oCamera_unk_70, ptr);
    ST32(cam + oCamera_unk_74, ptr);
    ST8 (cam + oCamera_unk_53, r2);
    ST8 (cam + oCamera_unk_52, r3);

    {
        u32 cfg = A_off_803DC78 + (layout_ix << 4);
        u32 v;

        v = LD32(cfg + 0);  ST32(cam + oCamera_unk_84, v);
        v = LD32(cfg + 4);  ST32(cam + oCamera_unk_88, v);
        v = LD32(cfg + 8);  ST32(cam + oCamera_unk_8C, v);
        v = LD8 (cfg + 0xC);
        ST8(cam + oCamera_unk_50, v);
        ST8(cam + oCamera_unk_51, v);
    }

    ST8 (cam + oCamera_unk_55, 0);
    ST8 (cam + oCamera_unk_58, 0);
    ST16(cam + oCamera_unk_68, 0);

    {
        u16 all_ones = (u16)~0;
        ST16(cam + oCamera_unk_60, all_ones);
        ST16(cam + oCamera_unk_62, all_ones);
        ST16(cam + oCamera_unk_64, all_ones);
        ST16(cam + oCamera_unk_66, all_ones);
    }

    zero_fill_by_byte((void *)LD32(cam + oCamera_unk_88), LD32(cam + oCamera_unk_74));
    zero_fill_by_byte((void *)LD32(cam + oCamera_unk_8C), LD32(cam + oCamera_unk_74));
}

Next sub_803DCE8(void) {
    camera_init(R0, R1, R2, R3);
    RETURN_TO_CALLER();
}

/* ── link_getModeSelector_803DD30 ── */
/* link_getModeSelector_803DD30: derive a small "link state" selector and return it in R0
 * (zero flag set iff the result is 0).
 *
 * It reads the jump offset slot of the structure at eStruct200BC30:
 *   offset 4 -> 0
 *   offset 8 -> 1
 *   anything else (including 0) -> the low 2 bits of the word at
 *   eLinkState + 0x48 (dword_2009A18). */
Next link_getModeSelector_803DD30(void) {
    GUEST_CALL(eStruct200BC30_getJumpOffset00);
    u32 jumpOffset = R0;

    if (jumpOffset == 4) {
        R0 = 0;
    } else if (jumpOffset == 8) {
        R0 = 1;
    } else {
        /* default: dword_2009A18 (eLinkState field 0x48), masked to 2 bits */
        R0 = LD32(A_eLinkState + 0x48) & 3;
    }

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_803DE5C ── */
/* sub_803DE5C: set unk_200A284 (a boolean flag) to 1. */
Next sub_803DE5C(void) {
    ST32(A_unk_200A284, 1);
    RETURN_TO_CALLER();
}

/* ── sub_803DE72 ── */
/* register form: calls sub_81445F8 outside this file, which has no typed C interface yet;
   the game function reads r0 (handler pointer) and writes back r0–r3 plus condition flags. */
static void check_and_call_handler(void) {
    u32 handler = LD32(A_unk_200A284);
    R0 = handler;
    if (handler != 0) {
        GUEST_CALL(sub_81445F8);
    } else {
        SET_FLAGS_NZ(0);   /* replicates tst r0,r0 with r0==0: Z set, N/C/V clear */
    }
}

/* sub_803DE72: if the global handler at unk_200A284 is non-null, invoke sub_81445F8
 * (which reads r0 = that handler); otherwise return with r0 = 0 and flags from tst. */
Next sub_803DE72(void) {
    check_and_call_handler();
    RETURN_TO_CALLER();
}

/* ── eLinkState_ResetForNewUse_803DE88 ── */
/* eLinkState_ResetForNewUse_803DE88(r0 = state pointer):
 * Reset the link/camera state block for a new use: clear the eLinkState
 * struct, its 0x10-byte auxiliary buffer and a 0x40-byte work area, then
 * store the given pointer at eLinkState+0x58 (dword_2009A28).
 * Flags and R1 are whatever the final ZeroFillByHalfword call leaves;
 * r2-r7 are untouched. */
Next eLinkState_ResetForNewUse_803DE88(void) {
    u32 statePtr = R0;

    R0 = A_eLinkState;
    R1 = 0x58;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = A_byte_200DD10;
    R1 = 0x10;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = A_unk_2009480;
    R1 = 0x40;
    GUEST_CALL(ZeroFillByHalfword);

    ST32(A_eLinkState + 0x58, statePtr);

    RETURN_TO_CALLER();
}

/* ── sub_803DEAC ── */
/* sub_803DEAC: tail-call wrapper that simply invokes sub_8144240.
 * r4-r7 are preserved (sub_8144240 keeps them intact). */
Next sub_803DEAC(void) {
    GUEST_CALL(sub_8144240);
    RETURN_TO_CALLER();
}

/* ── init_eStartScreenAnimationControl200B1A0_1 ── */
/* init_eStartScreenAnimationControl200B1A0_1:
 * Initialize the 8-byte e-StartScreen animation control block and mark it
 * active (state 0xB4). Preserves r2/r4; leaves r0 = 0xB4 with flags set. */
Next init_eStartScreenAnimationControl200B1A0_1(void) {
    u32 ctrl = A_eStartScreenAnimationControl200B1A0;

    R0 = ctrl;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);

    R0 = 0xB4;
    SET_FLAGS_NZ(R0);
    ST8(ctrl + 3, R0);
    RETURN_TO_CALLER();
}

/* ── init_eStartScreenAnimationControl200B1A0_2 ── */
/* init_eStartScreenAnimationControl200B1A0_2:
 * Reset the 8-byte extra start-screen animation control struct at
 * eStartScreenAnimationControl200B1A0 (zero fill) and enable it by
 * setting its active flag at offset 3 to 1.
 * r2 and r4 are preserved; flags are left as after `movs r0, #1`. */
Next init_eStartScreenAnimationControl200B1A0_2(void) {
    u32 ctrl = A_eStartScreenAnimationControl200B1A0;

    R0 = ctrl;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);

    R0 = 1;
    SET_FLAGS_NZ(1);
    ST8(ctrl + 3, 1);

    RETURN_TO_CALLER();
}

/* ── startScreen_IsTitleAnimationFinished_803E928 ── */
/* startScreen_IsTitleAnimationFinished_803E928:
 * Check the start screen animation's current state. Loads the animation
 * control byte and compares it against 2 (animation finished).
 * Outputs: R0 = state byte, flags set from `R0 - 2` (Z set when state == 2).
 */
Next startScreen_IsTitleAnimationFinished_803E928(void) {
    R0 = LD8(A_eStartScreenAnimationControl200B1A0);
    SET_FLAGS_CMP(R0, 2);
    RETURN_TO_CALLER();
}

/* ── startScreen_TstZero ── */
/* startScreen_TstZero: always clear the flags as "zero" (Z set, N clear).
 * Despite the name, it unconditionally tests 0 against itself. */
Next startScreen_TstZero(void) {
    R0 = 0;
    SET_FLAGS_NZ(0); /* tst r0, r0 with r0 == 0 */
    RETURN_TO_CALLER();
}

/* ── startScreen_DecrementPressStartPauseAnimationTimer ── */
/* startScreen_DecrementPressStartPauseAnimationTimer (0803E964):
 * Tick down the pause timer (byte at eStartScreenAnimationControl200B1A0+3)
 * that holds the "PRESS START" screen on its current animation frame. When the
 * timer reaches zero (i.e. it was 1), byte 0 of the animation control is set
 * to 1 so the animation advances, and the timer value reported to callers is 1.
 * Outputs: r0 = raw decremented timer, r6/r7 pass through unchanged, and the
 * flags are those of the "sub r0,#1" (or of "mov r0,#1" on the restart path). */
Next startScreen_DecrementPressStartPauseAnimationTimer(void) {
    u32 ctrl = A_eStartScreenAnimationControl200B1A0;

    u32 timer = LD8(ctrl + 3);

    /* timer--; the flags of this subtraction tell callers whether it hit zero */
    SET_FLAGS_CMP(timer, 1);
    timer -= 1;
    ST8(ctrl + 3, timer);

    if (timer != 0) {
        R0 = timer;
        RETURN_TO_CALLER();
    }

    /* timer expired: flag the animation control to move on, timer reads as 1 */
    ST8(ctrl, 1);
    SET_FLAGS_NZ(1); /* mov r0,#1 */
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── runPendingWorkIfFlagged_803EA1C ── */
/* runPendingWorkIfFlagged_803EA1C: service the pending work flagged by byte_200AD04.
 * If notZero_eByte200AD04 reports the flag byte is non-zero, run
 * sub_803F560 to process it; otherwise do nothing. Flags and R0-R3
 * are whatever the (last) callee leaves behind. */
Next runPendingWorkIfFlagged_803EA1C(void) {
    GUEST_CALL(notZero_eByte200AD04);
    if (R0 != 0)
        GUEST_CALL(sub_803F560);
    RETURN_TO_CALLER();
}

/* ── net_isLinkConnected_803EA2C ── */
/* net_isLinkConnected_803EA2C: check the link state flag.
 * r0 = 1 if the byte at eLinkState is 0xC (link connection fully established),
 * otherwise r0 = 0. Flags (nzcv) left as the CMP leaves them. */
Next net_isLinkConnected_803EA2C(void) {
    R0 = 0;
    R1 = LD8(A_eLinkState); /* literal pool points at eLinkState */
    SET_FLAGS_CMP(R1, 0xC);
    if (R1 == 0xC)
        R0 = 1;
    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_getJumpOffset00 ── */
/* eStruct200BC30_getJumpOffset00:
 * Returns the jump offset stored at the head of the eStruct200BC30 record.
 * r1 = pointer to the record, r0 = its first byte. */
Next eStruct200BC30_getJumpOffset00(void) {
    R1 = A_eStruct200BC30;
    R0 = LD8(R1);
    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_getJumpOffset00_pushpop ── */
/* eStruct200BC30_getJumpOffset00_pushpop: thin trampoline that tail-calls
 * eStruct200BC30_getJumpOffset00. Results come back in r0/r1 unchanged. */
Next eStruct200BC30_getJumpOffset00_pushpop(void) {
    GUEST_CALL(eStruct200BC30_getJumpOffset00);
    RETURN_TO_CALLER();
}

/* ── clearStruct200BC30_803EBAC ── */
/* clearStruct200BC30_803EBAC: clear the 0x20-byte work struct at 0x200BC30 (eStruct200BC30).
 * Leaves the struct zeroed word-by-word via ZeroFillByWord; r2/r3 pass
 * through untouched, r1 ends as ZeroFillByWord leaves it. */
Next clearStruct200BC30_803EBAC(void) {
    R0 = A_eStruct200BC30;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── resetAndInitStruct200BC30_803EBC8 ── */
/* resetAndInitStruct200BC30_803EBC8: reset the state used by sub_803EF60's subsystem.
 * Resets the list head at dword_200B1B0 (via sub_803EF84), zeroes the
 * 0x20-byte struct at eStruct200BC30, and then (re)initializes it. */
Next resetAndInitStruct200BC30_803EBC8(void) {
    R0 = A_dword_200B1B0;
    GUEST_CALL(sub_803EF84);

    R0 = A_eStruct200BC30;
    R1 = 0x20; /* struct size in bytes */
    GUEST_CALL(ZeroFillByWord);

    GUEST_CALL(sub_803EF60);

    RETURN_TO_CALLER();
}

/* ── sub_803EF60 ── */
/* sub_803EF60: reset some state, then zero-fill the buffer sub_803EF7C
 * returned a pointer to (0x100 bytes, filled eight words at a time). */
Next sub_803EF60(void) {
    GUEST_CALL(sub_803EF7C);   /* returns buffer address in r0 */
    R1 = 0x100;
    GUEST_CALL(ZeroFillByEightWords); /* ZeroFillByEightWords(r0, 0x100) */
    RETURN_TO_CALLER();
}

/* ── sub_803EF7C ── */
/* sub_803EF7C: return eCamera.unk_84 (the camera's end value). */
Next sub_803EF7C(void) {
    R0 = LD32(A_eLinkState + 0x34);
    RETURN_TO_CALLER();
}

/* ── sub_803EF84 ── */
/* sub_803EF84(r0 = value):
 * Store r0 into the link-state camera field at eLinkState+0x34, which the
 * disassembly also refers to as eCamera.unk_84 (the eLinkState symbol sits at
 * the same address as A_eLinkState, hence the alias in the original literal). */
Next sub_803EF84(void) {
    ST32(A_eLinkState + 0x34, R0);
    RETURN_TO_CALLER();
}

/* ── sub_803EFCC ── */
/* sub_803EFCC:
 *   Clear four small scratch/working buffers used by the battle state
 *   machinery (a 0x20-byte struct at 0x0200AC20, a 0x40-byte one at
 *   0x02009AC0, and two 0x200-byte ones at 0x0200BC70 and 0x0200A010)
 *   by ZeroFillByWord.
 *   No register inputs; clobbers r0, r1, nz. Preserves r2, r3 and the
 *   carry/overflow flags. */
Next sub_803EFCC(void) {
    /* The original does push {lr} / pop {pc}; no stack reservation is
     * used (the buffer is only stored below SP after the call returns),
     * so we just leave the stack alone and return via LR. */

    /* Clear 0x20-byte buffer at byte_200AC20. */
    R0 = (u32)A_byte_200AC20;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);

    /* Clear 0x40-byte buffer at unk_2009AC0. */
    R0 = (u32)A_unk_2009AC0;
    R1 = 0x40;
    GUEST_CALL(ZeroFillByWord);

    /* Clear 0x200-byte buffer at byte_200BC70. */
    R0 = (u32)A_byte_200BC70;
    R1 = 0x200;
    GUEST_CALL(ZeroFillByWord);

    /* Clear 0x200-byte buffer at word_200A010. */
    R0 = (u32)A_word_200A010;
    R1 = 0x200;
    GUEST_CALL(ZeroFillByWord);

    /* ZeroFillByWord is a leaf that leaves r2, r3 and the C/V flags
     * untouched (it only writes memory). The last call is the last
     * flag-affecting operation, so c/v and r2/r3 are preserved as the
     * callers expect. */

    RETURN_TO_CALLER();
}

/* ── clear_e200AD04 ── */
/* clear_e200AD04: zero the 4 bytes at word_200AD04 via ZeroFillByByte.
 * Preserves r2; clobbers r0/r1. */
Next clear_e200AD04(void) {
    R0 = A_word_200AD04;
    R1 = 4;
    GUEST_CALL(ZeroFillByByte);
    RETURN_TO_CALLER();
}

/* ── setByte_200AD04_803F4EC ── */
/* setByte_200AD04_803F4EC(r0 = value): store the low byte at word_200AD04.
 * Also leaves the address of word_200AD04 in r1. */
Next setByte_200AD04_803F4EC(void) {
    u32 *ptr = (u32 *)A_word_200AD04;

    ST8((u32)ptr, R0);
    R1 = (u32)ptr;

    RETURN_TO_CALLER();
}

/* ── sub_803F500 ── */
/* sub_803F500:
 * Store 0xB4 into word_200AD06 (likely resetting some counter/timer). */
Next sub_803F500(void) {
    ST16(A_word_200AD04 + 2, 0xB4);
    RETURN_TO_CALLER();
}

/* ── notZero_eByte200AD04 ── */
/* notZero_eByte200AD04:
 * Test the event flag byte at word_200AD04 for zero.
 * Outputs: r0 = the byte, r1 = its address, nz flags set from the byte. */
Next notZero_eByte200AD04(void) {
    R1 = A_word_200AD04;
    R0 = LD8(R1);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── tickCountdownGate_803F530 ── */
/* tickCountdownGate_803F530: countdown gate attached to the flag byte at byte_200AD04.
 *
 * While the flag byte is set, consults sub_803F560 each tick; when it
 * approves, decrements the halfword counter at 200AD06. The flag byte is
 * rewritten (via setByte_200AD04_803F4EC) either when the counter reaches
 * zero or when sub_803F560 vetoes immediately. On return, the Z flag
 * reports whether the countdown is still active (r6 == 1 internally means
 * "still counting"); r4/r6 themselves are callee-saved and restored. */
Next tickCountdownGate_803F530(void) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;

    R6 = 0;
    GUEST_CALL(notZero_eByte200AD04);

    if (R0 != 0) {                      /* flag byte at 200AD04 is set */
        R4 = 1;
        GUEST_CALL(sub_803F560); /* answers in Z; the original branches on it */

        if (!ZERO_FLAG()) {
            /* Approved: count one step down. */
            R4 = 0;
            R6 = 1;
            R1 = A_word_200AD04;
            R0 = LD16(R1 + 2);          /* counter at word_200AD06 */
            SET_FLAGS_CMP(R0, 1);       /* subs r0, #1 */
            R0 -= 1;
            ST16(R1 + 2, R0);
            if (R0 == 0) {
                /* Counter finished: clear the flag byte. */
                R6 = 0;
                R0 = R4;
                GUEST_CALL(setByte_200AD04_803F4EC);
            }
            /* else: still counting, skip the flag write. */
        } else {
            /* Vetoed: write the flag byte right away. */
            R0 = R4;
            GUEST_CALL(setByte_200AD04_803F4EC);
        }
    }
    /* Flag byte clear: nothing to do this tick. */

    /* tst r6, r6: Z clear while the countdown is still running. */
    SET_FLAGS_NZ(R6);

    R4 = saved_r4;                      /* pop {r4,r6} */
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── sub_803F560 ── */
/* Result of sub_803F560: the code left in r0, the value left in r1, and the
 * saved r4/r6/r7 that the original pushes and pops (so they end unchanged). */
struct sub_803F560_result {
    u32 value;     /* r0: 0xFF / 0x80 / 0 */
    u32 r1;
    u32 saved_r4;
    u32 saved_r6;
    u32 saved_r7;
};

/* register form: calls sub_803DEB4 outside this file, which has no typed C interface yet */
static u32 current_battle_mode(u32 *r1_out) {
    R6 = 0xFF;                     /* the caller always passes 0xFF in r6 */
    GUEST_CALL(sub_803DEB4);
    *r1_out = R1;                  /* r1 leftover: becomes the final r1 unless overwritten later */
    return R0;
}

/* register form: calls sub_813DB24 outside this file, which has no typed C interface yet */
static u32 poll_link_partner(u32 *r2_out) {
    R0 = 0;
    R1 = 1;
    R6 = 0xFF;                     /* r6 still holds 0xFF at this point */
    GUEST_CALL(sub_813DB24);
    *r2_out = R2;                  /* the answer also arrives in r2; the caller moves it into r1 */
    return R0;
}

/* register form: calls sub_8144D04 outside this file, which has no typed C interface yet */
static u32 link_player_state(u32 slot, u32 running_result, u32 entry_r4) {
    R0 = slot;
    R7 = slot;                     /* r7 is the loop counter here */
    R4 = entry_r4 + slot;          /* the original bumps r4 once per earlier iteration */
    R6 = running_result;           /* the running 0xFF/0 result is live in r6 */
    GUEST_CALL(sub_8144D04);
    return R0;
}

/* Work out a link-related state code.  Ask the battle mode first: mode 4 keeps
 * 0xFF, any other mode but 2 gives 0x80, and mode 2 polls the link partner.
 * If the poll returns 0xFF its r2 becomes r1 and the code stays 0xFF; otherwise
 * scan the four link slots and drop the code to 0 when slot 1 or 3 is in
 * state 2 (with r1 cleared). */
static struct sub_803F560_result probe_link_state(u32 entry_r4, u32 entry_r6, u32 entry_r7) {
    struct sub_803F560_result out;
    u32 r1_leftover;

    u32 mode = current_battle_mode(&r1_leftover);
    if (mode == 2) {
        u32 r2_leftover;
        u32 poll = poll_link_partner(&r2_leftover);
        if (poll == 0xFF) {
            /* No data: the original moves r2 into r1; r0 falls through as 0xFF. */
            out.value = 0xFF;
            out.r1 = r2_leftover;
        } else {
            u32 result = 0xFF;
            for (u32 slot = 0; slot < 4; slot++) {
                if (link_player_state(slot, result, entry_r4) == 2 && (slot == 1 || slot == 3))
                    result = 0;
            }
            out.value = result;
            out.r1 = 0;
        }
    } else if (mode == 4) {
        out.value = 0xFF;
        out.r1 = r1_leftover;
    } else {
        out.value = 0x80;
        out.r1 = r1_leftover;
    }

    out.saved_r4 = entry_r4;
    out.saved_r6 = entry_r6;
    out.saved_r7 = entry_r7;
    return out;
}

/* sub_803F560: returns a link-state code in r0 (0xFF, 0x80 or 0).
 * N/Z come from the code; C/V are clear, as the original's trailing
 * "mov r0,r6" (a flag-setting ADDS with 0) plus TST leave them.
 * r1 varies by path; r2, r3, r5 and r8-r12 keep whatever the callees left;
 * r4/r6/r7 are restored to their entry values. */
Next sub_803F560(void) {
    struct sub_803F560_result out = probe_link_state(R4, R6, R7);
    R0 = out.value;
    R1 = out.r1;
    R4 = out.saved_r4;
    R6 = out.saved_r6;
    R7 = out.saved_r7;
    SET_FLAGS_ADD(out.value, 0);   /* ADDS value,#0 then TST: N/Z from value, C=V=0 */
    RETURN_TO_CALLER();
}

/* ── sub_803F740 ── */
#define MODE_TABLE_ADDR   0x0803F774u
#define eLinkState_ADDR   0x20099D0u

typedef struct { u32 selector; u32 callee_r2; } ModeSelResult;

/* register form: calls link_getModeSelector_803DD30 outside this file, which has no typed C interface yet */
static ModeSelResult get_mode_selector(u32 index) {
    R0 = index;
    GUEST_CALL(link_getModeSelector_803DD30);
    /* Capture the callee's scratch r2; on the "invalid" path the original
     * never overwrites r2, so whatever the callee left is the visible output. */
    return (ModeSelResult){ R0, R2 };
}

typedef struct { u32 r0; u32 r1; u32 r2; u8 entry; } ModeLookupResult;

/* Look up the mode-selector byte for a link-player index.
 * If the table byte is 0xFF the slot is empty → r0=0, r1=1 (C set).
 * Otherwise read eCamera fields from eLinkState → r0=unk_66, r1=0, r2=unk_60 (C clear).
 * The original's 8*index+entry arithmetic is dead (r1 overwritten immediately after). */
static ModeLookupResult mode_table_lookup(u32 index) {
    ModeSelResult sel = get_mode_selector(index);
    u8 entry = LD8(MODE_TABLE_ADDR + sel.selector);

    ModeLookupResult res;
    res.entry = entry;
    if (entry == 0xFF) {
        res.r0 = 0;
        res.r1 = 1;
        res.r2 = sel.callee_r2;
    } else {
        res.r0 = LD16(eLinkState_ADDR + 0x16);   /* eCamera.unk_66 */
        res.r1 = 0;
        res.r2 = LD16(eLinkState_ADDR + 0x10);   /* eCamera.unk_60 */
    }
    return res;
}

/* sub_803F740(r0 = link index) -> r0, r1, r2, c, v */
Next sub_803F740(void) {
    ModeLookupResult res = mode_table_lookup(R0);
    R0 = res.r0;
    R1 = res.r1;
    R2 = res.r2;
    SET_FLAGS_CMP(res.entry, 0xFF);
    RETURN_TO_CALLER();
}

/* ── sub_803F798 ── */
/* sub_803F798: shared entry stub for a multi-entry routine.
 * It stores r1-r7 and lr on the stack exactly like `push {r1-r7,lr}`
 * (low word = r1, high word = lr), zeroes r4 as the loop counter seed,
 * and then continues into the shared body at loc_803F7A2, whose
 * epilogue pops these values back (pc = saved lr returns to the caller). */
Next sub_803F798(void) {
    u32 frame = SP - 32;
    SP = frame;

    ST32(frame + 0x00, R1);
    ST32(frame + 0x04, R2);
    ST32(frame + 0x08, R3);
    ST32(frame + 0x0C, R4);
    ST32(frame + 0x10, R5);
    ST32(frame + 0x14, R6);
    ST32(frame + 0x18, R7);
    ST32(frame + 0x1C, LR);

    R4 = 0;

    /* tail jump: loc_803F7A2 pops {r1-r7,pc} at its end */
    GUEST_CALL(loc_803F7A2);
    RETURN_TO_CALLER();
}

/* ── s2001c04_ComputeChecksumOfEwramRegion_803F894 ── */
/* s2001c04_ComputeChecksumOfEwramRegion_803F894: compute a checksum over the 16 bytes at EWRAM
 * 0x2000000 + 0x6700..0x670F, add 24, and store it into the
 * S2001c04 work area at offset 0x68. The field is briefly zeroed
 * first, as in the original. All registers except r0 are preserved. */
Next s2001c04_ComputeChecksumOfEwramRegion_803F894(void) {
    u32 work = LD32(R10 + oToolkit_S2001c04_Ptr);

    ST32(work + 0x68, 0);

    u32 sum = 0;
    for (s32 i = 0x670F; i >= 0; i--)
        sum += LD8(A_timer_2000000 + i);

    sum += 0x18;
    ST32(work + 0x68, sum);

    R0 = sum;
    RETURN_TO_CALLER();
}

/* ── info_copyTimestampToWorkArea_803F8B2 ── */
/* info_copyTimestampToWorkArea_803F8B2:
 * Copy the 20-byte InfoText timestamp into the work area at
 * [toolkit->S2001c04] + 0x6C. R1/R2/R3 are left as CopyBytes leaves them
 * (callers read their post-call values). */
Next info_copyTimestampToWorkArea_803F8B2(void) {
    R0 = A_EXE6_InfoText_timestamp;              /* src: timestamp buffer */
    R1 = LD32(R10 + oToolkit_S2001c04_Ptr) + 0x6C; /* dest: work area + 0x6C */
    R2 = 0x14;                                   /* 20 bytes */
    GUEST_CALL(CopyBytes);
    RETURN_TO_CALLER();
}

/* ── verifyEwramWorkAreaChecksum_803F8C4 ── */
/* verifyEwramWorkAreaChecksum_803F8C4: recompute the checksum over the EWRAM work area
 * (ewram_2000000, 0x6710 bytes) and compare it against the value saved at
 * S2001c04+0x68, which is then cleared.
 * Returns r0 = 0 if the checksum matches (data intact), r0 = 1 otherwise.
 * Sets Z/NZ flags from r0; r1-r7 are preserved. */
Next verifyEwramWorkAreaChecksum_803F8C4(void) {
    u32 state = LD32(R10 + oToolkit_S2001c04_Ptr);
    u32 storedChecksum = LD32(state + 0x68);
    ST32(state + 0x68, 0);

    /* Sum every byte of the work area, plus the fixed 0x18 bias. */
    u32 sum = 0x18;
    for (s32 i = 0x670F; i >= 0; i--)
        sum += LD8(A_timer_2000000 + i);

    R0 = (sum == storedChecksum) ? 0 : 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── infoText_IsTimestampUnchanged_803F8F4 ── */
/* infoText_IsTimestampUnchanged_803F8F4: check whether the info-text timestamp is unchanged.
 *
 * Compares the 20-byte timestamp stored in the save buffer
 * (toolkit->S2001c04 + 0x6C) against the master copy at
 * EXE6_InfoText_timestamp. Returns r0 = 0 if all 20 bytes match
 * (the on-screen text is still current), r0 = 1 if any byte differs
 * (the text must be refreshed).
 * Flags: nz reflects the returned value. */
Next infoText_IsTimestampUnchanged_803F8F4(void) {
    u32 master = A_EXE6_InfoText_timestamp;                  /* reference timestamp */
    u32 saved  = LD32(R10 + oToolkit_S2001c04_Ptr) + 0x6C;   /* copy in the save buffer */

    s32 i;
    for (i = 0x13; i >= 0; i--) {
        if (LD8(saved + i) != LD8(master + i)) {
            R0 = 1;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }
    }

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── save_BackupToGamePakWithRetry_803FAB4 ── */
/* save_BackupToGamePakWithRetry_803FAB4: back up the current save block to the game pak, retrying up
 * to three times.  Each attempt refills the RNG seed buffer, copies the
 * 0x80-byte block at dword_20067C0 to the game pak (0xE007F80) and verifies
 * the copy.  Returns r0 = 1 if at least one attempt verified OK, r0 = 0 if
 * all three failed. */
Next save_BackupToGamePakWithRetry_803FAB4(void) {
    const u32 entryR4 = R4;
    const u32 entryR6 = R6;
    const u32 entryR7 = R7;
    u32 failures = 0;
    u32 lastR3 = 0;

    for (u32 attempt = 0; attempt < 3; attempt++) {
        /* Refresh the entropy pool the save seed is drawn from. */
        R4 = entryR4;
        R6 = failures;
        R7 = attempt;
        GUEST_CALL(rng_FillSeedBuffer_803FAEE);

        /* Copy the 0x80-byte save block into the game pak backup area. */
        R0 = A_dword_20067C0;
        R1 = 0xE007F80;
        R2 = 0x80;
        GUEST_CALL(libSave_CopyToGamePak);
        lastR3 = R3;

        /* Verify the copy; a zero result marks the attempt as failed. */
        R0 = A_dword_20067C0;
        R1 = 0xE007F80;
        R2 = 0x80;
        R7 = attempt;
        GUEST_CALL(sub_814D9C4);
        R7 = attempt; /* sub_814D9C4 may clobber r7 */
        if (R0 == 0)
            failures++;
    }

    /* Restore the caller's registers and leave the loop's last state in
     * r2/r3, as the original's pops do. */
    R6 = entryR6;
    R7 = entryR7;
    R2 = 0x80;
    R3 = lastR3;

    if (failures < 3) {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── rng_FillSeedBuffer_803FAEE ── */
/* rng_FillSeedBuffer_803FAEE: fill the 0x80-byte buffer at dword_20067C0 with random 32-bit
 * words drawn from the hardware RNG, one GetRNG call per word.
 * r4-r7 are preserved (the original saves/restores them around the loop). */
Next rng_FillSeedBuffer_803FAEE(void) {
    enum { RNG_FILL_SIZE = 0x80 };  /* bytes to fill (literal =0x80) */

    u32 dst = A_dword_20067C0;

    for (u32 off = 0; off < RNG_FILL_SIZE; off += 4) {
        GUEST_CALL(GetRNG);
        ST32(dst + off, R0);
    }

    RETURN_TO_CALLER();
}

/* ── save_CommitWorkBufferToGamePak_803FB04 ── */
/* save_CommitWorkBufferToGamePak_803FB04(r0 = save work buffer value):
 * Store r0 into the save work buffer at dword_20067C0, then copy 0x80 bytes
 * from that buffer to the Game Pak save area at 0xE007F80.
 * The original pushes/pops r0-r7, so every register is preserved. */
Next save_CommitWorkBufferToGamePak_803FB04(void) {
    u32 inR0 = R0, inR1 = R1, inR2 = R2, inR3 = R3;
    u32 inR4 = R4, inR5 = R5, inR6 = R6, inR7 = R7;

    /* Remember the value being saved, in the save work buffer. */
    ST32(A_dword_20067C0, inR0);

    /* libSave_CopyToGamePak(src = dword_20067C0, dest = 0xE007F80, len = 0x80).
     * The callee also expects the source pointer in r4. */
    R0 = A_dword_20067C0;
    R1 = 0xE007F80;
    R2 = 0x80;
    R4 = A_dword_20067C0;
    GUEST_CALL(libSave_CopyToGamePak);

    /* Restore the caller's registers (the original's pop {r0-r7,pc}). */
    R0 = inR0; R1 = inR1; R2 = inR2; R3 = inR3;
    R4 = inR4; R5 = inR5; R6 = inR6; R7 = inR7;
    RETURN_TO_CALLER();
}

/* ── cb_803FB3C ── */
/* cb_803FB3C: Dispatch the S200F348 game-state callback.
 * Reads the state index from the first byte of eS200F348, jumps to the
 * matching handler (index 0 = init, 4 = playGameOver, 8 = update), then
 * runs the common post-handler sub_803FC14.  r4-r7 are preserved (the
 * original uses push/pop). */

/* register form: calls sub_803FB64 outside this file, which has no typed C interface yet */
static void call_state_init(u32 self) {
    R5 = self;
    GUEST_CALL(sub_803FB64);
}

/* register form: calls playGameOver_803FB9C outside this file, which has no typed C interface yet */
static void call_play_game_over(u32 self) {
    R5 = self;
    GUEST_CALL(playGameOver_803FB9C);
}

/* register form: calls sub_803FBC2 outside this file, which has no typed C interface yet */
static void call_state_update(u32 self) {
    R5 = self;
    GUEST_CALL(sub_803FBC2);
}

/* register form: calls sub_803FC14 outside this file, which has no typed C interface yet */
static void call_post_dispatch(void) {
    GUEST_CALL(sub_803FC14);
}

/* register form: GUEST_CALL callees modify preserved registers as side effects;
 * the original's push {r4-r7} / pop {r4-r7} guarantees restoration, which plain
 * C cannot express without reading/writing the register file directly. */
static void dispatch_s200f348(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    u32 self = A_eS200F348;
    u8 index = LD8(self + oS200F348_Index_00);

    switch (index) {
    case 0: call_state_init(self);       break;
    case 4: call_play_game_over(self);   break;
    case 8: call_state_update(self);     break;
    default:                             break;
    }

    call_post_dispatch();

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
}

Next cb_803FB3C(void) {
    dispatch_s200f348();
    RETURN_TO_CALLER();
}

/* ── sub_803FB64 ── */
/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void call_show_chatbox(u32 context, u32 self) {
    R1 = context;
    R5 = self;
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls sub_80015FC outside this file, which has no typed C interface yet */
static void call_set_display_mode(u32 mode) {
    R0 = mode;
    GUEST_CALL(sub_80015FC);
}

/* register form: calls copyTileData_803FC64 outside this file, which has no typed C interface yet */
static void call_copy_tile_data(void) {
    GUEST_CALL(copyTileData_803FC64);
}

/* register form: calls SetRenderInfoLCDControl outside this file, which has no typed C interface yet */
static void call_set_lcd_mode(u16 control) {
    R0 = control;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* register form: calls renderInfo_8001788 outside this file, which has no typed C interface yet */
static void call_render_info_a(void) {
    GUEST_CALL(renderInfo_8001788);
}

/* register form: calls renderInfo_80017A0 outside this file, which has no typed C interface yet */
static void call_render_info_b(void) {
    GUEST_CALL(renderInfo_80017A0);
}

/* register form: calls sub_803FBE8 outside this file, which has no typed C interface yet */
static void call_scene_teardown(u32 self) {
    R5 = self;
    GUEST_CALL(sub_803FBE8);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void call_screen_fade(u32 speed, u32 target) {
    R0 = speed;
    R1 = target;
    GUEST_CALL(SetScreenFade);
}

/* Trigger the game-over sequence: show chatbox, reset rendering, fade to black,
 * then mark the scene as game-over (index=4) and start the countdown timer.
 * Returns 0xB4 (the down-counter value, matching the final r0). */
static u32 game_over_setup(u32 self, u32 context_r1) {
    call_show_chatbox(context_r1, self);
    call_set_display_mode(0xF);
    call_copy_tile_data();
    call_set_lcd_mode(0x1F40);
    call_render_info_a();
    call_render_info_b();
    call_scene_teardown(self);
    call_screen_fade(8, 0x20);

    /* Mark this scene as "game over" */
    ST8(self + oS200F348_Index_00, 4);
    /* Start the post-game-over countdown (180 frames ≈ 3 seconds) */
    ST16(self + oS200F348_DownCounter, 0xB4);

    return 0xB4;
}

/* sub_803FB64(self: r5, context: r1) -> r0=0xB4 */
Next sub_803FB64(void) {
    R0 = game_over_setup(R5, R1);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── playGameOver_803FB9C ── */
/*
 * playGameOver_803FB9C — per-frame update of the game-over sequence state
 * (self in r5). Each call decrements the state's DownCounter:
 *   - when it reaches 0, start the screen fade (mode 12, speed 8) and switch
 *     the state's Index to 8 (the next cutscene step);
 *   - when it reaches 0xAF, play the game-over jingle.
 * The original leaves r0/r1/flags behind for its caller; r2/r3 and the
 * preserved registers pass through untouched.
 */

/* Values the original leaves behind in the caller-visible registers. */
typedef struct {
    u32 r0;
    u32 r1;
    u32 counter; /* decremented DownCounter; on the countdown path the
                    original's flags come from "cmp counter, 0xAF" */
} PlayGameOverResult;

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static u32 set_screen_fade_playGameOver_803FB9C(u32 mode, u32 speed) {
    R0 = mode;
    R1 = speed;
    GUEST_CALL(SetScreenFade);
    return R1; /* SetScreenFade may clobber r1, and the original leaves its r1 behind */
}

/* register form: calls PlayMusic outside this file, which has no typed C interface yet */
static void play_music(u32 song) {
    R0 = song;
    GUEST_CALL(PlayMusic); /* PlayMusic preserves r0/r1, so r0 keeps the song id */
}

static PlayGameOverResult play_game_over(u32 self) {
    PlayGameOverResult out;

    u32 counter = (u16)LD16(self + oS200F348_DownCounter) - 1;
    ST16(self + oS200F348_DownCounter, counter);
    out.counter = counter;

    if (counter == 0) {
        /* Counter expired: fade the screen out, then advance to the next state. */
        u32 fade_r1 = set_screen_fade_playGameOver_803FB9C(0xC, 8);
        ST8(self + oS200F348_Index_00, 8);
        out.r0 = 8;
        out.r1 = fade_r1;
    } else if (counter == 0xAF) {
        /* Point in the countdown where the game-over jingle starts. */
        play_music(SONG_GAME_OVER);
        out.r0 = SONG_GAME_OVER;
        out.r1 = 0xAF;
    } else {
        out.r0 = counter;
        out.r1 = 0xAF;
    }
    return out;
}

Next playGameOver_803FB9C(void) {
    PlayGameOverResult out = play_game_over(R5);
    R0 = out.r0;
    R1 = out.r1;
    SET_FLAGS_CMP(out.counter, 0xAF); /* last flag write on the countdown path: cmp r0, #0xAF */
    RETURN_TO_CALLER();
}

/* ── sub_803FBC2 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();          /* IsScreenFadeActive: Z set if a screen fade is active */
}

/* register form: calls sub_8006910 outside this file, which has no typed C interface yet */
static void call_sub_8006910(void) {
    GUEST_CALL(sub_8006910);
}

/* register form: calls startScreen_init_802F530 outside this file, which has no typed C interface yet */
static void call_startScreen_init(void) {
    GUEST_CALL(startScreen_init_802F530);
}

/* register form: calls clear_e200AD04 outside this file, which has no typed C interface yet */
static void call_clear_e200AD04(void) {
    GUEST_CALL(clear_e200AD04);
}

/* register form: calls init_eStartScreenAnimationControl200B1A0_2 outside this file, which has no typed C interface yet */
static void call_init_startScreenAnimationControl(void) {
    GUEST_CALL(init_eStartScreenAnimationControl200B1A0_2);
}

/* register form: calls SetRenderInfoLCDControl outside this file, which has no typed C interface yet */
static void set_render_info_lcd_control(u16 val) {
    R0 = val;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* If a screen fade is in progress, do nothing. Otherwise run the start-screen reset
 * sequence (subsystem reinit, clear start-screen control, init animation control)
 * and set the LCD control to 0x40. */
static void maybe_reset_start_screen(void) {
    if (screen_fade_active())
        return;
    call_sub_8006910();
    call_startScreen_init();
    call_clear_e200AD04();
    call_init_startScreenAnimationControl();
    set_render_info_lcd_control(0x40);
}

/* sub_803FBC2: THUMB, 0803FBC2 */
Next sub_803FBC2(void) {
    maybe_reset_start_screen();
    RETURN_TO_CALLER();
}

/* ── sub_803FC14 ── */
typedef struct {
    u32 r0, r1, r2, r3;
    u32 cmp_a, cmp_b;
} Sub803FC14Out;

/* sub_803FC14: refresh the OAM attribute buffer for one frame.
 *
 * Decrements the toolkit's render-info vertical offset (at offset 0x10)
 * by 4 and stores it back, then writes that offset into the decomp
 * buffer at eDecompBuffer2013A00: first four halfwords, then loads
 * the next halfword, subtracts a per-cell delta from byte_803FC50,
 * and writes that into the next eight halfwords, repeating. Exits
 * when either the 0x140-byte buffer is full (bge) or 0x14 cells have
 * been processed (ble). The output register values and the operands
 * of the final comparison (for flag setup) are returned. */
static Sub803FC14Out apply_vram_offset(u32 toolkit_ptr, u32 r1_in) {
    Sub803FC14Out out;
    u32 render_info_ptr = LD32(toolkit_ptr + oToolkit_RenderInfoPtr);
    u32 decomp = A_eDecompBuffer2013A00;
    u32 end = 0x140;
    u32 offset = LD16(render_info_ptr + 0x10) - 4;
    u32 r1_val = r1_in;
    u32 inner = 4;
    u32 pos = 0;
    u32 cell = 0;

    /* Save the new vertical offset back into the toolkit. */
    ST16(render_info_ptr + 0x10, (u16)offset);
    out.r2 = 0x140;
    out.r3 = decomp;

    for (;;) {
        /* Inner loop: store `offset` into `inner` consecutive halfwords,
         * then either loop back (bgt) or fall through to the outer step. */
        do {
            ST16(decomp + pos, (u16)offset);
            pos += 2;
            if ((s32)pos >= (s32)end) {
                /* Buffer full: exit via bge on `cmp pos, 0x140`. */
                out.r0 = offset;
                out.r1 = r1_val;
                out.cmp_a = pos;
                out.cmp_b = end;
                return out;
            }
            inner--;
        } while ((s32)inner > 0);

        /* Outer step: load the next halfword, subtract this row's delta
         * from byte_803FC50, then prepare to write it into the next 8
         * cells. The ble on `cmp cell+1, 0x14` decides whether to loop. */
        offset = LD16(decomp + pos) - LD8(A_byte_803FC50 + cell);
        r1_val = LD8(A_byte_803FC50 + cell);
        inner = 8;
        cell++;
        out.r0 = offset;
        out.r1 = r1_val;
        out.cmp_a = cell;
        out.cmp_b = 0x14;
        if ((s32)cell > 0x14) {
            /* 0x14 cells processed: exit via ble failure. */
            return out;
        }
    }
}

Next sub_803FC14(void) {
    Sub803FC14Out out = apply_vram_offset(R10, R1);

    R0 = out.r0;
    R1 = out.r1;
    R2 = out.r2;
    R3 = out.r3;
    SET_FLAGS_CMP(out.cmp_a, out.cmp_b);
    RETURN_TO_CALLER();
}

/* ── copyTileData_803FC64 ── */
/* register form: calls resetAllGraphicsState_8005F40 outside this file, which has no typed C interface yet */
static void reset_all_graphics_state(void) {
    GUEST_CALL(resetAllGraphicsState_8005F40);
}

/* register form: calls runSetupStepAndClearWorkArea_8005F6C outside this file, which has no typed C interface yet */
static void run_setup_step_and_clear_work_area(void) {
    GUEST_CALL(runSetupStepAndClearWorkArea_8005F6C);
}

/* register form: calls sub_80027C4 outside this file, which has no typed C interface yet */
static void call_sub_80027C4_copyTileData_803FC64(void) {
    GUEST_CALL(sub_80027C4);
}

/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decomp_and_copy_data(const u32 *init_refs) {
    R0 = (u32)init_refs;
    GUEST_CALL(decompAndCopyData);
}

/* register form: calls ZeroFillGFX30025c0 outside this file, which has no typed C interface yet */
static void zero_fill_gfx(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
}

/* register form: CopyBackgroundTiles takes i_size in r5, but the caller keeps
 * r5 alive across the call by pushing/popping it around the bl; a guest
 * register save/restore around an untyped call cannot be expressed as typed C,
 * so this helper saves r5, sets up the call, and restores r5 afterwards. */
static void copy_background_tiles_copyTileData_803FC64(u32 j, u32 i, u32 which_tile_block_32x32,
                                  const u16 *tile_ids, u32 j_size, u32 i_size) {
    u32 saved_r5 = R5;
    R0 = j;
    R1 = i;
    R2 = which_tile_block_32x32;
    R3 = (u32)tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
    R5 = saved_r5;
}

/* One-time graphics setup: restore default graphics state, decompress the
 * shared font/palette data listed in the init table, clear the tile GFX
 * area, then blit the two background tile maps into VRAM tile blocks 1
 * (32x20 tiles) and 2 (30x20 tiles). */
static void copy_tile_data(void) {
    reset_all_graphics_state();
    run_setup_step_and_clear_work_area();
    call_sub_80027C4_copyTileData_803FC64();

    decomp_and_copy_data((const u32 *)A_initRefs803FCA4);
    zero_fill_gfx();

    copy_background_tiles_copyTileData_803FC64(0, 0, 1, (const u16 *)A_eTileIds2018A04, 0x20, 0x14);
    copy_background_tiles_copyTileData_803FC64(0, 0, 2, (const u16 *)A_eTileIds2017A04, 0x1E, 0x14);
}

/* Inputs r0/r1 and r2-r7/r10 are ignored. r5-r7 are preserved (the original
 * pushes/pops r5 and never touches r6/r7); r2-r4 and the flags are whatever
 * the last callee (CopyBackgroundTiles) leaves behind, which the GUEST_CALL
 * in copy_background_tiles_copyTileData_803FC64 reproduces exactly. */
Next copyTileData_803FC64(void) {
    copy_tile_data();
    RETURN_TO_CALLER();
}
