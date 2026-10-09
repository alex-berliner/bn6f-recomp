/*
 * leaves_misc.c — small functions from across the game converted by the model
 * farm, grouped by bn6f source file until their subsystems are converted.
 *
 * Drafted by Hyper GLM-5.3 Flash with tools/convert/farm.py (accepted on 40
 * calls captured from the original on the recorded routes, and random
 * inputs), reviewed and integrated by Claude; checked with the host
 * difftest, the CPU oracle and lockstep. Evidence per function: symbols/db.
 */
#include "game.h"
#include "runtime.h"

/* ── bn6f asm00_0.s ────────────────────────────────────────────────── */

/* sub_8001D64(r4 = source, r5 = destination):
 * copy a block of 8 words (0x20 bytes) from source to destination.
 * r0 = the last word copied (the value at source + 0x1C). */
Next sub_8001D64(void) {
    u32 src = R4;
    u32 dst = R5;

    for (u32 i = 0; i < 8; i++) {
        ST32(dst + i * 4, LD32(src + i * 4));
    }

    R0 = LD32(src + 7 * 4);
    RETURN_TO_CALLER();
}

/* Field offsets within one entry of the software palette table
 * ePalette20097a0 (the header only exposes the table's base address,
 * so the per-field immediates used by the assembly are defined here). */
enum { PAL20097A0_00 = 0, PAL20097A0_01 = 1, PAL20097A0_02 = 2, PAL20097A0_04 = 4, PAL20097A0_08 = 8 };

/* sub_8002378(r0, r1 = palette data pointer, r2 = color count,
 *             r3 = palette slot index, r4):
 * Fill slot r3 of the software palette table at ePalette20097a0. The
 * slot is only populated when both the data pointer (r1) and the color
 * count (r2) are nonzero; in that case r0/r1/r2/r4 are stored verbatim
 * into the slot's fields. The slot's flag byte is set to 1 when it was
 * filled and 0 otherwise, and that same value is returned in r3.
 * r5-r7 are preserved. */
Next sub_8002378(void) {
    u32 base = A_ePalette20097a0_0_Unk_00;
    u32 offset = R3 * oPalette20097a0_Size;
    u32 filled = 0;

    /* `add r7, r7, r3` is the last instruction on either path that
     * touches c/v, so they come from this address computation. */
    SET_FLAGS_ADD(base, offset);
    u32 entry = base + offset;

    if (R1 != 0 && R2 != 0) {
        ST8(entry + PAL20097A0_01, R0);
        ST32(entry + PAL20097A0_04, R1);
        ST8(entry + PAL20097A0_02, R2);
        ST32(entry + PAL20097A0_08, R4);
        filled = 1;
    }

    ST8(entry + PAL20097A0_00, filled);

    /* Final n/z: from `mov r3,#1` when the slot was filled, otherwise
     * from the failing `tst r1,r1` / `tst r2,r2` (a zero operand gives
     * Z set, N clear) - identical to the flags of `filled`. */
    SET_FLAGS_NZ(filled);
    R3 = filled;
    RETURN_TO_CALLER();
}

/* memory_bcdConvert: convert the value in r0 to packed BCD (8 decimal digits,
 * 4 bits each, most significant digit in the top nibble).
 * If the CMP against 99999999 says the value is not below the limit (N clear),
 * the sentinel 0x99999999 is returned instead and r2/r3 are left untouched.
 * On success r2 holds the packed BCD result, r1 the last divisor used (1) and
 * r3 the wound-down loop counter (0). Flags report on the returned value.
 * r4/r7 are saved and restored. */
#define BCD_MAX_VALUE 99999999u   /* = 0x05F5E0FF, largest convertible value */
#define BCD_SENTINEL  0x99999999u /* "out of range" marker, all BCD 9s */

Next memory_bcdConvert(void) {
    u32 value = R0;

    /* The original branches on the N flag of value - limit, so replicate the
     * subtraction (with wraparound) rather than doing a plain signed compare. */
    u32 diff = value - BCD_MAX_VALUE;
    SET_FLAGS_CMP(value, BCD_MAX_VALUE);

    if ((s32)diff >= 0) {
        /* Out of range: return the sentinel, leave r2/r3 as the caller set them. */
        R0 = BCD_SENTINEL;
        R1 = BCD_MAX_VALUE;
        RETURN_TO_CALLER();
    }

    u32 result = 0;
    u32 table = A_off_8000C34; /* {10000000, 1000000, ..., 10, 1} */
    u32 divisor = 0;

    R3 = 8;
    do {
        result <<= 4;
        divisor = LD32(table);
        table += 4;

        u32 digit = 0;
        while ((s32)value >= (s32)divisor) {
            value -= divisor;
            digit++;
        }
        result |= digit;
    } while (--R3 != 0);

    R0 = result;
    R1 = divisor; /* always 1 after the last pass */
    R2 = result;
    /* mov r0, r2 is encoded as ADDS r0, r2, #0 in Thumb, so it sets N/Z from
     * the result and clears C/V. */
    SET_FLAGS_ADD(result, 0);
    RETURN_TO_CALLER();
}

/* ── bn6f asm00_1.s ────────────────────────────────────────────────── */

/* sub_800647C (r5 = effect work area):
 * Adds the step size at +4 to the counter at +6, clamping it at the limit at
 * +0xA (clearing the "active" byte at +3 once the limit is reached). The top
 * bits of the counter are expanded into a 15-bit RGB color (the same shade in
 * every channel) and passed to two sprite/palette updates via sub_8002378.
 *
 * r4-r7 are callee-saved here (pushed/popped by the original), so they must
 * come out unchanged even though we use r4/r6 as call arguments. Note that r1
 * for the second call is read from r6 *after* the first call, exactly as the
 * original does — it relies on whatever sub_8002378 leaves in r6. */
Next sub_800647C(void) {
    u32 savedR4 = R4;
    u32 work = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 count = (u32)LD16(work + 6) + (u32)LD16(work + 4);
    u32 limit = LD16(work + 0xA);

    if ((s32)count >= (s32)limit) {
        /* effect finished: deactivate and hold the counter at the limit */
        ST8(work + 3, 0);
        count = limit;
    }
    ST16(work + 6, count);

    /* replicate the counter's intensity into R, G and B */
    u32 shade = count >> 4;
    R6 = shade | (shade << 5) | (shade << 10);

    R0 = LD16(work + 8);
    R1 = R6;
    R2 = LD16(work + 0x18);
    R3 = (u32)LD8(work + 0xC) + 0x12;
    R4 = LD32(work + 0x10);
    GUEST_CALL(sub_8002378);

    /* r1 for the second call is whatever the callee left in r6 */
    R0 = LD16(work + 8);
    R1 = R6;
    R2 = LD16(work + 0x1C);
    R3 = (u32)LD8(work + 0xC) + 0x13;
    R4 = LD32(work + 0x14);
    GUEST_CALL(sub_8002378);

    /* pop {r4-r7}: restore the caller's registers */
    R4 = savedR4;
    R5 = work;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* sub_8009338 (r5 = battle state):
 * Per-frame tick of the battle-end sequence.
 *
 * The first time through (byte at +3 still clear), raise flag 4 in the
 * battle state's unk11 flag set, mark the sequence as started and run the
 * one-time setup in sub_8026840. Every frame then polls sub_8026A28:
 *   0 -> sequence still running, nothing to do this frame;
 *   2 -> run battle_stateGotoPhase14_800AABC (one final step) and return;
 *   otherwise the sequence is over: flag both fighters' AI data and switch
 *   the battle state machine to index 0xC with the halfword at +2 cleared. */
Next sub_8009338(void) {
    if (LD8(R5 + 3) == 0) {
        R0 = 4;
        GUEST_CALL(setBattleStateUnk11Flag_800A9CA);
        ST8(R5 + 3, 1);
        GUEST_CALL(sub_8026840);
    }

    GUEST_CALL(sub_8026A28);
    u32 phase = R0;

    if (phase == 0) {
        /* Still in progress: return with the flags of "cmp r0, #0". */
        SET_FLAGS_CMP(phase, 0);
        RETURN_TO_CALLER();
    }

    if (phase == 2) {
        GUEST_CALL(battle_stateGotoPhase14_800AABC);
        RETURN_TO_CALLER();
    }

    /* Battle finished: tell the AI on both sides (alliance 0 and 1). */
    R0 = 0;
    GUEST_CALL(battle_findPlayer);
    SET_FLAGS_CMP(R0, 0);              /* cmp r0, #0 */
    if (R0 != 0) {
        R0 = LD32(R0 + oBattleObject_AIDataPtr);
        R1 = 1;
        ST8(R0 + oAIData_Unk_0f, R1);
    }

    R0 = 1;
    GUEST_CALL(battle_findPlayer);
    SET_FLAGS_CMP(R0, 0);              /* cmp r0, #0 */
    if (R0 != 0) {
        R0 = LD32(R0 + oBattleObject_AIDataPtr);
        R1 = 1;
        ST8(R0 + oAIData_Unk_0f, R1);
    }

    /* Advance to battle state 12 and clear the counter halfword. */
    R0 = 0xC;
    ST8(R5 + 1, R0);
    SET_FLAGS_NZ(R0);                  /* mov r0, #0xC */
    R0 = 0;
    ST16(R5 + 2, R0);
    SET_FLAGS_NZ(R0);                  /* mov r0, #0 */
    RETURN_TO_CALLER();
}

/* ── bn6f asm00_2.s ────────────────────────────────────────────────── */

/* sub_801CA80(r5 = background layout state):
 * For each entry in the u16 table at [r5+0x28], copy a 9x2 strip of map
 * tiles. Entry i gives a destination column as j = 0x1E - entry; a value of
 * 0xFF marks the end of the table. The tile ids for strip i are two
 * interleaved runs of consecutive ids starting at 0xD1DA + 9*i (row 0 takes
 * the even ids, row 1 the odd ids); they are built in a stack buffer and
 * handed to CopyBackgroundTiles for tile block 32x32 #0. The row counter i
 * runs over 0,2,4,6, i.e. at most four strips.
 *
 * The original reserves 20 bytes for the saved registers before its 0x24-byte
 * local buffer, and the buffer address escapes to CopyBackgroundTiles, so both
 * reservations are kept. r4-r7 are clobbered as call arguments but restored,
 * as are the flags from the last CMP on whichever path exits. */
enum { STRIP_BASE_TILE = 0xD1DA }; /* first tile id used by the strips */

Next sub_801CA80(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    SP -= 20;                  /* push {r4-r7,lr} frame: the buffer sits below it */
    u32 buf = SP -= 0x24;      /* 2 rows x 9 halfword tile ids */
    u32 entryPtr = R5 + 0x28;  /* table of destination columns */

    u32 i = 0;
    R1 = i;
    for (;;) {
        R2 = LD16(entryPtr);
        SET_FLAGS_CMP(R2, 0xFF);
        if (R2 == 0xFF)        /* end of table */
            break;

        /* Build the id table: row 0 counts up from the base id, row 1 from base+1. */
        u32 baseTile = STRIP_BASE_TILE + 9 * i;
        for (u32 k = 0; k < 9; k++) {
            ST16(buf + 2 * k, baseTile + 2 * k);
            ST16(buf + 0x12 + 2 * k, baseTile + 1 + 2 * k);
        }

        /* push {r1}: keep the row counter alive across the call */
        SP -= 4;
        ST32(SP, R1);

        R0 = 0x1E - R2;        /* j: destination column */
        R2 = 0;                /* tile block 32x32 */
        R3 = buf;              /* tile ids */
        R4 = 9;                /* j size */
        R5 = 2;                /* i size */
        GUEST_CALL(CopyBackgroundTiles);

        R1 = LD32(SP);         /* pop {r1} */
        SP += 4;

        i += 2;
        entryPtr += 2;
        R1 = i;
        SET_FLAGS_CMP(R1, 8);
        if (i == 8)
            break;
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    SP += 0x24 + 20;
    RETURN_TO_CALLER();
}

/* ── bn6f asm03_0.s ────────────────────────────────────────────────── */

/* sub_8031C5A: enable the flag passed to sub_8031B7C as r0 = 1 (e.g. mark
 * something as done/on); all results (r0, r2, r5, flags) come straight from
 * the callee. */
Next sub_8031C5A(void) {
    R0 = 1;
    GUEST_CALL(sub_8031B7C);
    RETURN_TO_CALLER();
}

/* sub_8031B7C(r0 = query id):
 * Fetches one byte out of the overworld map object's state. With r0 == 1 it
 * follows dword_2013920 to the object, reads its pointer at +0x14, and returns
 * the byte at +1 of that structure; any other r0 returns 0. Callers also
 * receive the intermediate pointers in r5 (object table) and r2 (structure),
 * plus the flags of the `cmp r0, #1` (and on the zero path the subsequent
 * `mov r0, #0` updates n/z on top of them). */
Next sub_8031B7C(void) {
    u32 mode = R0;

    R5 = A_eOverworldMapObject55_Size;
    SET_FLAGS_CMP(mode, 1);             /* cmp r0, #1 */

    if (mode == 1) {
        R2 = LD32(R5 + 0x14);           /* -> dword_2013934 */
        R0 = LD8(R2 + 1);
    } else {
        R0 = 0;
        SET_FLAGS_NZ(R0);               /* mov r0, #0 */
    }

    RETURN_TO_CALLER();
}

/* ── bn6f asm29.s ────────────────────────────────────────────────── */

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

/* ── bn6f asm32.s ────────────────────────────────────────────────── */

/* sub_811CC68: return the value at [r5 + 0xC] in r0.
 * A simple field getter: r5 points to some struct, r0 gets its 0xC field. */
Next sub_811CC68(void) {
    R0 = LD32(R5 + 0xC);
    RETURN_TO_CALLER();
}

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

/* ── bn6f asm33.s ────────────────────────────────────────────────── */

/* sub_8127ACC: per-frame update for a battle object (r5 = object pointer).
 *
 * The byte at offset 2 in the object selects one of four state handlers via
 * the jump table at off_8127AF8. The byte is used directly as a *byte*
 * offset into that word table (no shift in the original), so the valid
 * values are 0, 4, 8 and 12. After the state-specific handler, the common
 * update sequence runs for every object. */
Next sub_8127ACC(void) {
    /* State handler from off_8127AF8 (sub_8127B08, B18, B94, BE4), entered
     * as the original: r0 = handler, r1 = state byte (sub_8127B94 reads r1). */
    R1 = LD8(R5 + 2);
    R0 = LD32(0x08127AF8u + R1);
    GUEST_CALL_ADDR(R0);

    /* Common update steps, run regardless of the state handler above. */
    GUEST_CALL(sub_81281E0);
    GUEST_CALL(sub_8127F54);
    GUEST_CALL(sub_8127FE4);
    GUEST_CALL(sub_8128280);
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    GUEST_CALL(sub_80465F8);

    RETURN_TO_CALLER();
}

/* ── bn6f libs.s ────────────────────────────────────────────────── */

/* libSave_CopyToGamePak(r0 = source, r1 = destination, r2 = byte count):
 * Copy `count` bytes (e.g. save data) to or from Game Pak address space.
 * While doing so, the WS0 field of the Game Pak waitstate control is forced
 * to 3 (the fastest setting) and left that way afterwards. The copy loop is
 * skipped entirely when count == 0. Always returns r3 = -1; r4/r5 (and the
 * waitstate bits above WS0) are preserved. */
Next libSave_CopyToGamePak(void) {
    u32 src = R0;
    u32 dst = R1;
    s32 count = (s32)R2;

    /* Force the Game Pak waitstates to 0b11 while touching the save chip. */
    u32 waitcnt = LD16(A_GamePakWaitstateControl);
    ST16(A_GamePakWaitstateControl, (waitcnt & 0xFFFCu) | 0x3u);

    /* Copy count bytes; the decrement-first structure means count == 0
     * copies nothing. */
    count--;
    if (count != -1) {
        do {
            ST8(dst, LD8(src));
            src++;
            dst++;
        } while (--count != -1);
    }

    R3 = 0xFFFFFFFF;
    RETURN_TO_CALLER();
}

/* ── bn6f main.s ────────────────────────────────────────────────── */

/* main_updateJoypad_80003E4: main-loop joypad update.
 *
 * Refreshes the toolkit's joypad struct from the hardware key state:
 *   +0x00  keys pressed this frame (active high, computed from ~KeyStatus)
 *   +0x02  keys newly pressed this frame (down now but not last frame)
 *   +0x04  keys that register a press from holding: the frame right after
 *          a key starts being held, and repeat ticks once held 16+ frames
 *   +0x06  previous frame's pressed keys
 *   +0x08..0x11  per-key hold counters, one byte per KEYINPUT bit
 *   +0x13  repeat pacing timer, cycling 1,2,3,4,0 (a repeat fires when 0)
 *
 * Leaves r2/r4/r5/r6/r7 set exactly as the original does; its callers read
 * them even though they are not arguments of this call.
 */
Next main_updateJoypad_80003E4(void) {
    u32 joypad = LD32(R10 + 4);

    /* Advance the repeat pacing timer. */
    u32 repeatTimer = LD8(joypad + oJoypad_LowSensitivityTimer) + 1;
    if (repeatTimer > 4)
        repeatTimer = 0;
    ST8(joypad + oJoypad_LowSensitivityTimer, repeatTimer);

    u32 keysNow  = ~LD16(A_KeyStatus);   /* active-high view of the hardware keys */
    u32 keysPrev = LD16(joypad + 0x00);  /* keys that were down last frame */
    ST16(joypad + 0x06, keysPrev);       /* save as "previous" for next frame */
    ST16(joypad + 0x00, keysNow);        /* store new pressed-key state */

    /* Keys down both this frame and last frame go through hold/repeat logic.
     * (The original loads a 0x3FF key mask here but never uses it.) */
    u32 heldKeys = keysNow & keysPrev;

    for (u32 i = 0; i < 10; i++) {
        u32 keyBit = 1u << i;
        u32 counterAddr = joypad + 0x08 + i;

        if (heldKeys & keyBit) {
            u32 count = LD8(counterAddr);
            if (count < 0x10) {
                count++;
                ST8(counterAddr, count);
                if (count != 1) {
                    /* Held past the second frame: suppress until repeat. */
                    heldKeys &= ~keyBit;
                    R2 = ~keyBit;
                } else {
                    /* Second frame of a new press: key still registers. */
                    R2 = 1;
                }
            } else if (repeatTimer != 0) {
                /* Long hold, between repeat ticks: no press this frame. */
                heldKeys &= ~keyBit;
                R2 = ~keyBit;
            } else {
                /* Repeat tick: key fires again, stays in the held set. */
                R2 = count;
            }
        } else {
            /* Key not held: reset its hold counter. */
            ST8(counterAddr, 0);
            R2 = 0;
        }
    }

    ST16(joypad + 0x04, heldKeys);      /* keys that register this frame */
    R4 = keysNow & ~keysPrev;           /* newly pressed keys */
    ST16(joypad + 0x02, R4);
    R5 = ~keysPrev;
    R6 = heldKeys;
    R7 = repeatTimer;

    RETURN_TO_CALLER();
}

