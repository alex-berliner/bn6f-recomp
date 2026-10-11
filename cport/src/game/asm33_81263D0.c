/*
 * asm33_81263D0.c: readable functions of bn6f asm33.s, from 0x081263D0, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── text_decodeCharTiles8_801263D0_81263D0 ── */
/* text_decodeCharTiles8_801263D0_81263D0(r0 = source bytes, r2/r3 = destination halfword buffers,
 * r6 = base tile index): decode a run of character codes into tile map
 * entries. Each byte below 0xE5 becomes a pair of tiles (base + code*2
 * and base + code*2 + 1) written as halfwords to r2/r3, which advance
 * two bytes per character. The run ends at the 0xE5 terminator (or when
 * 8 entries have been produced); remaining slots up to 8 are zero-filled.
 * r0/r1 are scratch; r4-r7 are preserved. */
Next text_decodeCharTiles8_801263D0_81263D0(void) {
    u32 src = R0;
    u32 baseTile = R6;
    u32 dstLo = R2;
    u32 dstHi = R3;
    u32 count = 0;

    /* Decode characters until the 0xE5 terminator. */
    for (;;) {
        u32 code = LD8(src + count);
        if ((s32)code >= 0xE5)
            break;

        u32 tile = baseTile + (code << 1);
        ST16(dstLo, tile);
        ST16(dstHi, tile + 1);
        dstLo += 2;
        dstHi += 2;
        count++;
    }

    /* Zero-fill up to the fixed width of 8 entries. */
    while (count < 8) {
        ST16(dstLo, 0);
        ST16(dstHi, 0);
        dstLo += 2;
        dstHi += 2;
        count++;
    }

    R2 = dstLo;
    R3 = dstHi;
    RETURN_TO_CALLER();
}

/* ── bg_drawTileStripTileBlock2_8126404 ── */
/* bg_drawTileStripTileBlock2_8126404(r0 = j, r1 = i, r2 = tileIds, r3 = iSize):
 * Redraws a strip of the background from tile block 2 (a 32x32 tile block),
 * 8 tiles wide. All caller arguments are forwarded to the drawing routine:
 * r0/r1 pass straight through, and r3 doubles as the strip's i size.
 *
 * If bit 0 of the flag word at [r7+0x7C] is set, the strip is drawn by
 * sub_3005EBA with no tile id table (r3 = 0); otherwise the tile ids given
 * in r2 are blitted with CopyBackgroundTiles.
 *
 * On return r3 holds whatever the drawing routine left there; r0, r4-r7
 * and r10 are left as they were on entry. */
Next bg_drawTileStripTileBlock2_8126404(void) {
    u32 tileIds = R2;
    u32 stripSize = R3;
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    if (LD32(R7 + 0x7C) & 1) {
        /* Flag set: alternate draw path, filling without a tile id table. */
        R2 = 2;           /* which_tile_block_32x32 = 2 */
        R3 = 0;           /* tile_ids = none */
        R4 = 8;           /* j_size = 8 */
        R5 = stripSize;   /* i_size */
        R6 = stripSize;
        GUEST_CALL(sub_3005EBA);
    } else {
        R2 = 2;           /* which_tile_block_32x32 = 2 */
        R3 = tileIds;     /* tile_ids */
        R4 = 8;           /* j_size = 8 */
        R5 = stripSize;   /* i_size */
        R6 = stripSize;
        GUEST_CALL(CopyBackgroundTiles);
    }

    /* The call used r4-r7 as arguments/scratch; the caller expects them
     * back exactly as it passed them in. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8126430 ── */
/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void copy_words(const u32 *src, u32 *dst, u32 count_u32) {
    R0 = (u32)src;
    R1 = (u32)dst;
    R2 = count_u32;
    GUEST_CALL(CopyWords);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static const void *get_chip_raw(s32 which_chip) {
    R0 = (u32)which_chip;
    GUEST_CALL(getChip8021DA8);
    return (const void *)R0;
}

/* sub_8126430:
 *   Inputs:  r0 (slot index scale), r1 (chip-id table base), r2 (destination),
 *            r3 (source for first copy), r4 (chip-data dest base),
 *            r5/r6/r7 are loop-local in the original but our caller contract gives them in.
 *   Behaviour: copies 0x38 bytes from r3 to r2, then walks 7 entries: each entry is a 4-byte
 *   {u16 chip_id, u8 unused, u8 flag} at r1 + (r0<<2) + 4*idx; if chip_id==0 or flag!=0,
 *   write 0 to {r2+8*idx, r2+8*idx+4}; else load chip and CopyWords(0x80) from
 *   chip->field_0x20 into r4 + 0x80*idx.
 *   The original preserves r4 across the call; r5/r6/r7 are loop-local in the assembly but
 *   the contract says outputs are r4/r5/r6/r7, so we just pass them through unchanged.
 */
static void run_chip_row(u32 r0_in, u32 r1_in, u32 r2_in, u32 r3_in, u32 r4_in,
                         u32 r5_in, u32 r6_in, u32 r7_in) {
    (void)r5_in; (void)r6_in; (void)r7_in;

    /* Stage 1: 0x38-byte copy from r3 to r2 (matches "mov r0,r3 / mov r1,r2 / mov r2,#0x38"). */
    copy_words((const u32 *)r3_in, (u32 *)r2_in, 0x38u);

    /* Stage 2: 7-iteration loop.
     *   r5 = r1 + (r0 << 2)          (table walker)
     *   r6 = r4                      (chip-data dest, += 0x80 each iteration)
     *   r7 = 0..6                    (loop index)
     *   r2 = output walker, += 8 each iteration
     */
    u32 r5 = r1_in + (r0_in << 2);
    u32 r6 = r4_in;
    u32 r7 = 0;
    u32 r2 = r2_in;

    for (; (s32)r7 < 7; r7 = r7 + 1) {
        u16 slot_id = LD16(r5);
        if (slot_id == 0) {
            /* Chip slot empty: zero out the 8-byte output pair. */
            ST32(r2, 0);
            ST32(r2 + 4, 0);
        } else if (LD8(r5 + 2) != 0) {
            /* Slot's high byte set: also zero out the output pair. */
            ST32(r2, 0);
            ST32(r2 + 4, 0);
        } else {
            /* Slot has a chip: pull its data block (field 0x20) and copy 0x80 bytes into r6. */
            const void *chip = get_chip_raw((s32)slot_id);
            u32 chip_data_src = LD32((u32)chip + 0x20);
            copy_words((const u32 *)chip_data_src, (u32 *)r6, 0x80u);
        }

        r5 = r5 + 4;
        r6 = r6 + 0x80u;
        r2 = r2 + 8;
    }
}

Next sub_8126430(void) {
    /* Thin adapter: read the inputs, do the work, write the outputs (r4/r5/r6/r7).
     * The original preserves r4 and the loop uses r5/r6/r7 only internally; the caller
     * contract lists r4..r7 as outputs, so we hand back the incoming values for them
     * (r5/r6/r7 are loop-local and have no defined meaning to callers). */
    u32 r0 = R0, r1 = R1, r2 = R2, r3 = R3, r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    run_chip_row(r0, r1, r2, r3, r4, r5, r6, r7);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── bg_drawTileRegion2x14_8126484 ── */
/* bg_drawTileRegion2x14_8126484(r0 = j, r1 = i, r2 = tileIds, r4 = ?, r5 = ?, r6, r7):
 * Draws a 2x14-tile region of a background at (j, i), using tile IDs taken
 * from r2 and always tile block 2 (32x32). r0/r1 and r4-r7 pass through
 * unchanged; r2 is consumed as the tile_ids argument.
 * On return r3 still holds the tile_ids pointer (as CopyBackgroundTiles
 * leaves it), matching the original, which pops r4-r7 back off the stack. */
Next bg_drawTileRegion2x14_8126484(void) {
    /* The original pushes r4-r7 and pops them after the call, so save the
     * incoming values here and restore them once the callee is done. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Set up the CopyBackgroundTiles(j, i, which_tile_block_32x32,
     * tile_ids, j_size, i_size) arguments. r0 = j and r1 = i are already
     * in place; tile_ids comes in via r2 and moves to r3. */
    R3 = R2;        /* tile_ids */
    R2 = 2;         /* which_tile_block_32x32 */
    R4 = 2;         /* j_size */
    R5 = 0xE;       /* i_size */

    GUEST_CALL(CopyBackgroundTiles);

    /* pop {r4-r7}: hand the caller back its saved registers. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;

    RETURN_TO_CALLER();
}

/* ── sub_8126494 ── */
/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_ptr(u32 which_chip) {
    R0 = which_chip;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* Fill 7 chip-info slots (each 0xC bytes) from 7 chips at baseAddr + chipIndex*4.
 * For each chip index at cursor: if zero, clear the first three u32 fields (12 bytes)
 * and the trailing u16 at offset 0xA.
 * Otherwise load chip data, take byte 5, compute base + byte5*3 into the last three u16
 * fields at offsets 6/8/0xA. The first three u16 fields (offsets 0, 2, 4) are zeroed. */
static void fill_chip_slots(u32 chipIndex, u32 baseAddr, u32 slotAddr) {
    const u32 tableBase = 0x8340;
    u32 cursor = baseAddr + chipIndex * 4;
    u32 out = slotAddr;
    for (u32 i = 0; i < 7; i++) {
        u16 idx = LD16(cursor);
        if (idx == 0) {
            ST32(out, 0);
            ST32(out + 4, 0);
            ST32(out + 8, 0);
            ST16(out + 0xA, 0);
        } else {
            u32 chip = get_chip_ptr(idx);
            u32 field5 = LD8(chip + 5);
            u32 r4 = tableBase + field5 * 3;
            ST16(out + 0, 0);
            ST16(out + 2, 0);
            ST16(out + 4, 0);
            ST16(out + 6, (u16)r4);
            r4 += 1;
            ST16(out + 8, (u16)r4);
            r4 += 1;
            ST16(out + 0xA, (u16)r4);
        }
        cursor += 4;
        out += 0xC;
    }
}

/* sub_8126494(r0 = chipIndex, r1 = baseAddr, r2 = slotAddr) */
Next sub_8126494(void) {
    fill_chip_slots(R0, R1, R2);
    RETURN_TO_CALLER();
}

/* ── bg_copyTilePatchBlock2_81264E4 ── */
/* bg_copyTilePatchBlock2_81264E4(r0 = j, r1 = i, r2 = tile_ids):
 * Copy a fixed-size patch of background tiles by tailoring the arguments of
 * CopyBackgroundTiles: 32x32 tile block #2, j_size 3, i_size 14.
 * The caller's r4-r7 are preserved; r3 comes back as CopyBackgroundTiles left it. */
Next bg_copyTilePatchBlock2_81264E4(void) {
    u32 tile_ids = R2;
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R3 = tile_ids; /* tile_ids: *const u16 */
    R2 = 2;        /* which_tile_block_32x32 */
    R4 = 3;        /* j_size */
    R5 = 0xE;      /* i_size */
    GUEST_CALL(CopyBackgroundTiles);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_81264F4 ── */
/* sub_81264F4: fixed-argument wrapper around sub_8126570.
 *
 * It loads a constant argument set (two screen coordinates and two sizes in
 * r0-r3, a data table pointer in r4, a hardware-register mirror address in
 * r6, and a second table pointer in r7) and tail-calls sub_8126570, which
 * draws/updates something using them.
 *
 * r5 and r10 arrive from the caller and are forwarded untouched. Although
 * r4-r7 are overwritten to set up the call, the original pushes them and
 * pops them back afterwards, so from the caller's point of view r4-r7 are
 * preserved; only the callee's results in r2/r3 escape. */
Next sub_81264F4(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R0 = 0x5A;
    R1 = 0x20;
    R2 = 0x14;
    R3 = 0x0C;
    R4 = A_byte_8124A84;
    R6 = 0x40000000;
    R7 = A_dword_8124A94;

    GUEST_CALL(sub_8126570);

    /* pop {r4-r7} - restore the caller's register values */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── wrapper_sub8126570_fixedArgs_812650C ── */
/* wrapper_sub8126570_fixedArgs_812650C: parameter-filling wrapper around sub_8126570.
 * Sets up the four size arguments and the pointers to the byte_8124A84 /
 * dword_8124A94 data, invokes the worker, and restores its own saved
 * r4-r7 on the way out. r2/r3 (and the flags) come straight from the
 * callee; r0/r1 are scratch. */
Next wrapper_sub8126570_fixedArgs_812650C(void) {
    /* The original push {r4-r7,lr} / pop {r4-r7,pc} preserves the
     * caller's r4-r7 across the call. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    R0 = 0x5A;              /* x extent  = 90 */
    R1 = 0x20;              /* y extent  = 32 */
    R2 = 0x1E;              /* width     = 30 */
    R3 = 0x0C;              /* height    = 12 */
    R4 = A_byte_8124A84;    /* pointer to the byte_8124A84 table */
    R6 = 0x40000000;        /* constant passed through to the worker */
    R7 = A_dword_8124A94;   /* pointer to the dword_8124A94 table */

    GUEST_CALL(sub_8126570);

    /* pop {r4-r7,pc} -- r4-r7 end exactly as they came in; r2/r3 are
     * whatever sub_8126570 left behind. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8126524 ── */
/* sub_8126524: thin wrapper around sub_8126570.
 * Sets up fixed arguments for the callee: r0-r3 = constants (rect at 90,32,
 * size 40x12), r4 = pointer to byte_8124A84, r6 = flags 0x40000000,
 * r7 = pointer to dword_8124A94. r5 passes through untouched.
 * Like the original's push/pop of r4-r7, those registers are restored to
 * their incoming values after the call; r2-r3 remain whatever the callee
 * leaves behind (callers read them as its outputs). */
Next sub_8126524(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R0 = 0x5A;
    R1 = 0x20;
    R2 = 0x28;
    R3 = 0x0C;
    R4 = A_byte_8124A84;
    R6 = 0x40000000;
    R7 = A_dword_8124A94;

    GUEST_CALL(sub_8126570);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_8126554 ── */
/* sub_8126554: thin argument-setup wrapper around sub_8126570.
 * It loads a fixed set of arguments (counts/positions 90, 32, 60, 12 plus
 * pointers to the data tables byte_8124A84 and dword_8124A94, and the
 * 0x40000000 flag word), invokes sub_8126570, and hands its r1-r3 results
 * and flags straight back to the caller. r4-r7 are preserved for the
 * caller, exactly as the original's push/pop does. */
Next sub_8126554(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R0 = 0x5A;
    R1 = 0x20;
    R2 = 0x3C;
    R3 = 0xC;
    R4 = A_byte_8124A84;
    R6 = 0x40000000;
    R7 = A_dword_8124A94;

    GUEST_CALL(sub_8126570);

    /* pop {r4-r7}: the callee's r4-r7 do not escape this wrapper */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── sub_81265DC ── */
/* register form: calls sub_8116FDC outside this file, which has no typed C interface yet */
static void draw_row_cell(u32 x, u32 anim, u32 tile_offset) {
    R0 = x;
    R1 = anim;
    R2 = tile_offset;
    GUEST_CALL(sub_8116FDC);
}

/* Battle-row sprite setup: iterates r7 = 1, then 0. For each, the screen x is
 * base[(r7<<2)] + ((battle_x - 0x100) << 3), clamped to >= -40 via up to two
 * 0x120 wrap-arounds. Then blits the row sprite via sub_8116FDC at tile
 * offset r4 (advancing 3 each iteration). */
static void draw_battle_rows(u32 battle) {
    u32 r4 = 4;
    for (u32 r7 = 1; (s32)r7 >= 0; r7--) {
        u32 base = LD32(A_off_812661C + (r7 << 2));
        u32 coord = LD32(battle + 0x74);
        u32 x_val = base + ((coord - 0x100u) << 3);
        s32 threshold = (s32)0xFFFFFFD8u;  /* -40 */
        if ((s32)x_val < threshold) {
            x_val += 0x120u;
            if ((s32)x_val < threshold)
                x_val += 0x120u;
        }
        draw_row_cell(x_val, 0x1Bu, r4);
        r4 += 3;
    }
}

/* sub_81265DC(r4, r5, r6, r7, lr) -> r3 r4 r5 r6 r7 */
Next sub_81265DC(void) {
    draw_battle_rows(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8126630 ── */
#define SPRITE_KIND_TABLE   A_off_81266C4
#define SLOT_FRAME_TABLE    A_dword_201FC20
#define WARN_BYTE_A         A_byte_8124AE0
#define WARN_BYTE_B         A_byte_8124B08
#define SPRITE_ID           0x2F
#define SPRITE_Y            0x58

/* register form: calls getStructFrom2008450 outside this file, which has no typed C interface yet */
static u32 get_sprite_struct(u32 slot, u32 kind) {
    R0 = slot;
    R4 = kind;
    GUEST_CALL(getStructFrom2008450);
    if (ZERO_FLAG()) return 0;
    return R1;
}

/* register form: calls sub_8046696 outside this file, which has no typed C interface yet */
static void warn_unexpected(u32 code, u32 kind) {
    R0 = code;
    R4 = kind;
    GUEST_CALL(sub_8046696);
}

/* register form: calls sub_80465A0 outside this file, which has no typed C interface yet */
static void report_unexpected_ptr(u32 pointer) {
    R0 = pointer;
    GUEST_CALL(sub_80465A0);
}

/* register form: calls setFieldsInThreeTableEntries_811980C outside this file, which has no typed C interface yet */
static void set_three_entries(u32 val, u32 entry_addr, u32 table) {
    R0 = val;
    R1 = 0xFF;
    R2 = 0;
    R3 = 0;
    R4 = entry_addr;
    R7 = table;
    GUEST_CALL(setFieldsInThreeTableEntries_811980C);
}

/* register form: calls sprite_setPositionForIdRange3_8119854 outside this file, which has no typed C interface yet */
static void set_sprite_position(u32 id, u32 y, u32 z) {
    R0 = id;
    R1 = y;
    R2 = z;
    GUEST_CALL(sprite_setPositionForIdRange3_8119854);
}

/* register form: calls sub_8126818 outside this file, which has no typed C interface yet */
static void trigger_slot_effect(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_8126818);
}

/* register form: calls setSlotFieldsFromStartTo3_811A1F4 outside this file, which has no typed C interface yet */
static void set_slot_fields(u32 val, u32 extra, u32 kind) {
    R0 = val;
    R1 = extra;
    R2 = 0;
    R3 = 0;
    R4 = kind;
    GUEST_CALL(setSlotFieldsFromStartTo3_811A1F4);
}

/* register form: calls sprite_moveSlotGroupXY_811A220 outside this file, which has no typed C interface yet */
static void move_slot_group(u32 id, u32 y, u32 z) {
    R0 = id;
    R1 = y;
    R2 = z;
    GUEST_CALL(sprite_moveSlotGroupXY_811A220);
}

/*
 * Update battle sprite animation data and position for a battle object.
 * obj is the object base pointer (caller's r5).
 * kind byte at obj+0xD: 0-3 → per-kind frame table path, 4 → slot path.
 */
static void update_battle_sprite(u32 obj) {
    u32 kind = LD8(obj + 0xD);

    if (kind != 4) {
        u32 ptr = get_sprite_struct(0, kind);
        if (ptr == 0) return;

        u32 sprite_type = LD8(ptr + 1);
        if (sprite_type != 0x16) {
            warn_unexpected(3, kind);
            report_unexpected_ptr(WARN_BYTE_A);
        }

        u32 table = LD32(SPRITE_KIND_TABLE + (kind << 2));
        u32 entry_addr = obj + (0xA * kind);
        u32 frame = LD16(entry_addr + 0x14) + LD16(entry_addr + 0x18);
        u32 val = LD16(table + (frame << 2));

        set_three_entries(val, entry_addr, table);
        set_sprite_position(SPRITE_ID, SPRITE_Y, 0);
        trigger_slot_effect(0);
    } else {
        u32 ptr = get_sprite_struct(0, kind);
        if (ptr == 0) return;

        u32 sprite_type = LD8(ptr + 1);
        if (sprite_type != 0x1A)
            report_unexpected_ptr(WARN_BYTE_B);

        u32 table = SLOT_FRAME_TABLE;
        u32 frame = LD16(obj + 0x3C) + LD16(obj + 0x40);
        u32 offset = frame << 2;
        u32 val = LD16(table + offset);
        u32 extra = LD8(table + offset + 3);

        set_slot_fields(val, extra, kind);
        move_slot_group(SPRITE_ID, SPRITE_Y, 0);
    }
}

/* sub_8126630(obj_ptr = r5) — updates sprite tables, fields, and position. */
Next sub_8126630(void) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    update_battle_sprite(R5);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    RETURN_TO_CALLER();
}

/* ── HandleMegaManStatusMenu8126B4C ── */
/* HandleMegaManStatusMenu8126B4C: top-level dispatcher for the MegaMan
 * status menu. The menu object's state byte ([R5+1]) selects one of three
 * handlers from the jump table at off_8126B60.
 *
 * The state byte is stored as a byte offset into the table (0, 4 or 8),
 * since the original indexes it with an unscaled `ldr r0, [r0, r1]`.
 * The original tail-dispatches to the handler with LR set to its own
 * epilogue, so the handler's return goes straight back to this function's
 * caller; the table word it dispatched through is still in R0 and the raw
 * state byte is still in R1 when the handler starts. */
Next HandleMegaManStatusMenu8126B4C(void) {
    u32 state = LD8(R5 + 1);

    /* Load the table entry exactly as the original does (byte offset),
     * so R0 entering the handler is the dispatched address (Thumb bit set). */
    u32 entry = LD32(A_off_8126B60 + state);

    R0 = entry;
    R1 = state;

    switch (state) {
    case 0:
        GUEST_CALL(sub_8126B6C);   /* state 0 handler */
        break;
    case 4:
        GUEST_CALL(sub_8126CC0);   /* state 4 handler */
        break;
    case 8:
        GUEST_CALL(sub_8126FF0);   /* state 8 handler */
        break;
    default:
        /* Unreachable in the game: the handlers keep the state byte within
         * the table. The original would branch through a garbage word. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_8126B6C ── */
/* register form: calls zeroFillVRAM outside this file, which has no typed C interface yet */
static void call_zero_fill_vram(void) { GUEST_CALL(zeroFillVRAM); }

/* register form: calls ZeroFill_byte_3001960 outside this file, which has no typed C interface yet */
static void call_zero_fill_3001960(void) { GUEST_CALL(ZeroFill_byte_3001960); }

/* register form: calls ZeroFillGFX30025c0 outside this file, which has no typed C interface yet */
static void call_zero_fill_gfx(void) { GUEST_CALL(ZeroFillGFX30025c0); }

/* register form: calls sub_80015FC outside this file, which has no typed C interface yet */
static void call_sub_80015FC(u32 arg) { R0 = arg; GUEST_CALL(sub_80015FC); }

/* register form: sub_8046664 takes toolkit ptr in R7; callee may clobber R7 (preserved by outer fn) */
static void call_sub_8046664(u32 toolkit) {
    u32 saved = R7;
    R7 = toolkit;
    GUEST_CALL(sub_8046664);
    R7 = saved;
}

/* register form: calls sub_81270D0 outside this file, which has no typed C interface yet */
static void call_sub_81270D0(void) { GUEST_CALL(sub_81270D0); }

/* register form: calls sub_8127800 outside this file, which has no typed C interface yet */
static void call_sub_8127800(void) { GUEST_CALL(sub_8127800); }

/* register form: calls sub_8120CC8 outside this file, which has no typed C interface yet.
 * Returns the packed tile-attribute byte: (R0 | (R1 << 4)) & 0xFF. */
static u32 call_get_tile_attr(u32 idx, u32 table_addr) {
    R0 = idx; R1 = table_addr;
    GUEST_CALL(sub_8120CC8);
    return (R0 | (R1 << 4)) & 0xFF;
}

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u8 call_get_cur_pet_navi(void) {
    GUEST_CALL(GetCurPETNavi);
    return (u8)R0;
}

/* register form: calls GetCurPETNaviStatsHword outside this file, which has no typed C interface yet */
static u16 call_get_navi_stat_hword(u8 navi, u8 stat) {
    R0 = navi; R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsHword);
    return (u16)R0;
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u8 call_get_navi_stat_byte(u8 navi, u8 stat) {
    R0 = navi; R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (u8)R0;
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void call_test_event_flag(u32 page, u32 index) {
    R0 = page; R1 = index;
    GUEST_CALL(TestEventFlagFromImmediate);
}

/* register form: sub_8120D10 takes state byte in R4; callee may clobber R4 (preserved by outer fn).
 * Returns R1 (script index) and sets *out_zero from Z flag. */
static u32 call_sub_8120D10(u32 arg, u32 state_byte, bool *out_zero) {
    u32 saved = R4;
    R0 = arg; R4 = state_byte;
    GUEST_CALL(sub_8120D10);
    *out_zero = ZERO_FLAG();
    u32 result = R1;
    R4 = saved;
    return result;
}

/* register form: chatbox_runScript takes (archive, idx) in R0/R1;
 * callee SYNC shows R4/R5 not explicitly preserved, so save them for the outer function. */
static void call_chatbox_script(u32 archive, u32 idx) {
    u32 saved_r4 = R4, saved_r5 = R5;
    R0 = archive; R1 = idx;
    GUEST_CALL(chatbox_runScript_803FD9C);
    R5 = saved_r5;
    R4 = saved_r4;
}

/* register form: calls chatbox_set_eFlags2009F38 outside this file, which has no typed C interface yet */
static void call_chatbox_set_flags(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_set_eFlags2009F38);
}

/* register form: calls sub_812741C outside this file, which has no typed C interface yet */
static void call_sub_812741C(void) { GUEST_CALL(sub_812741C); }

/* register form: calls background_drawSceneSection_81275A8 outside this file */
static void call_draw_scene_section(void) { GUEST_CALL(background_drawSceneSection_81275A8); }

/* register form: calls sub_81275C8 outside this file, which has no typed C interface yet */
static void call_sub_81275C8(void) { GUEST_CALL(sub_81275C8); }

/* register form: calls bg_copyTileBlockFromStructOffset_8127618 outside this file */
static void call_bg_copy_tile_block(void) { GUEST_CALL(bg_copyTileBlockFromStructOffset_8127618); }

/* register form: calls sub_8127638 outside this file, which has no typed C interface yet */
static void call_sub_8127638(void) { GUEST_CALL(sub_8127638); }

/* register form: calls sub_8127718 outside this file, which has no typed C interface yet */
static void call_sub_8127718(void) { GUEST_CALL(sub_8127718); }

/* register form: calls bg_Draw8x8TileBlock3FromTable_81277E0 outside this file */
static void call_draw_8x8_block3(void) { GUEST_CALL(bg_Draw8x8TileBlock3FromTable_81277E0); }

/* register form: calls sub_81273B4 outside this file, which has no typed C interface yet */
static void call_sub_81273B4(void) { GUEST_CALL(sub_81273B4); }

/* register form: calls sub_81273F0 outside this file, which has no typed C interface yet */
static void call_sub_81273F0(void) { GUEST_CALL(sub_81273F0); }

/* register form: calls sub_8127060 outside this file, which has no typed C interface yet */
static void call_sub_8127060(void) { GUEST_CALL(sub_8127060); }

/* register form: calls sub_8119C94 outside this file; returns both R0 and R1 (tile-set base ptrs) */
static void call_get_tileset_ptrs(u32 *out0, u32 *out1) {
    GUEST_CALL(sub_8119C94);
    *out0 = R0; *out1 = R1;
}

/* register form: calls sub_80465A0 outside this file, which has no typed C interface yet */
static void call_sub_80465A0(u32 arg) { R0 = arg; GUEST_CALL(sub_80465A0); }

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void call_set_screen_fade_sub_8126B6C(u32 mode, u32 level) {
    R0 = mode; R1 = level;
    GUEST_CALL(SetScreenFade);
}

/* Initialise the PET-Navi visual scene: clear VRAM/GFX, configure render-info,
 * fill the scene-control struct at `scene`, run the intro chat script,
 * draw background layers, and fade the screen in. */
static void pet_navi_scene_init(u32 scene, u32 toolkit) {
    /* Clear video and scratch buffers */
    call_zero_fill_vram();
    call_zero_fill_3001960();
    call_zero_fill_gfx();
    call_sub_80015FC(0x1A);

    /* Render-info header: tile ID 0x1F40, then zero remaining half-words */
    u32 rinfo = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(rinfo, 0x1F40);
    ST16(rinfo + 0x10, 0);
    ST16(rinfo + 0x12, 0);
    ST16(rinfo + 0x14, 0);
    ST16(rinfo + 0x16, 0);
    ST16(rinfo + 0x18, 0);
    ST16(rinfo + 0x1A, 0);

    /* Scene display mode = 4, sub-state = 0 */
    ST8(scene + 1, 4);
    ST8(scene + 2, 0);

    /* Setup callbacks (sub_8046664 reads toolkit via R7) */
    call_sub_8046664(toolkit);
    call_sub_81270D0();
    call_sub_8127800();

    /* Zero-clear scene control fields */
    ST8(scene + 0x0C, 0);
    ST8(scene + 0x0D, 0);
    ST8(scene + 0x10, 0);
    ST8(scene + 0x11, 0);
    ST16(scene + 0x1E, 0);
    ST16(scene + 0x24, 0);
    ST16(scene + 0x20, 0);
    ST16(scene + 0x2E, 0);
    ST16(scene + 0x2A, 0);
    ST8(scene + 0x1A, 0);
    ST8(scene + 0x1B, 0);
    ST8(scene + 0x1C, 0);
    ST8(scene + 0x1D, 0);
    ST16(scene + 0x3A, 0);

    /* Defaults: sprite-ID 0xFF, tile-set 0x1DA, active=1, palette=3 */
    ST16(scene + 0x26, 0xFF);
    ST16(scene + 0x32, 0x1DA);
    ST16(scene + 0x38, 1);
    ST16(scene + 0x28, 3);

    /* Tile attribute from texture-table lookup */
    u32 tile_attr = call_get_tile_attr(0, A_dword_81268F8);
    ST8(scene + 0x13, tile_attr);
    ST8(scene + 0x12, tile_attr);
    ST32(scene + 0x48, tile_attr);

    /* Current PET Navi HP */
    u8 navi = call_get_cur_pet_navi();
    ST16(scene + 0x34, call_get_navi_stat_hword(navi, oNaviStats_CurHP));
    ST16(scene + 0x36, call_get_navi_stat_hword(navi, oNaviStats_MaxHP));

    /* Navi stat bytes */
    navi = call_get_cur_pet_navi();
    ST8(scene + 0x14, call_get_navi_stat_byte(navi, 9));
    navi = call_get_cur_pet_navi();
    ST8(scene + 0x15, call_get_navi_stat_byte(navi, 0xB));
    navi = call_get_cur_pet_navi();
    ST8(scene + 0x16, call_get_navi_stat_byte(navi, 0xC));

    /* State byte 0xF is always 2; event-flag test runs when navi==0 but both
     * branches perform the same write, so just store the result. */
    navi = call_get_cur_pet_navi();
    if (navi == 0) {
        call_test_event_flag(1, 0x63); /* EVENT_163 */
    }
    ST8(scene + 0x0F, 2);

    /* Intro chat script: tile-availability check determines index */
    bool tiles_ready;
    u32 script_idx = call_sub_8120D10(0, 2, &tiles_ready);
    if (!tiles_ready) {
        ST8(scene + 2, 0x14);
        call_chatbox_script(A_eTextScript201BF20, script_idx);
    } else if (LD8(scene + 0x10) != 2) {
        call_chatbox_script(A_eTextScript201BF20, 0);
    }

    call_chatbox_set_flags(0x40);

    /* Draw background layers */
    call_sub_812741C();
    call_draw_scene_section();
    call_sub_81275C8();
    call_bg_copy_tile_block();
    call_sub_8127638();
    call_sub_8127718();
    call_draw_8x8_block3();
    call_sub_81273B4();
    call_sub_81273F0();
    call_sub_8127060();

    /* Tile-set pointers into scene struct (+0x40, +0x44) */
    u32 bg0, bg1;
    call_get_tileset_ptrs(&bg0, &bg1);
    ST32(scene + 0x40, bg0 + 0x38);
    ST32(scene + 0x44, bg1 + 0x50);

    /* Final scene object setup and fade-in */
    call_sub_80465A0(A_byte_8126B1C);
    call_set_screen_fade_sub_8126B6C(8, 0x10);
}

/* sub_8126B6C(r5 = scene struct, r10 = toolkit) -> all regs/flags from callees */
Next sub_8126B6C(void) {
    pet_navi_scene_init(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8126DF8 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_sub_8126DF8(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG(); /* Z clear means fade IS active (returned non-zero) */
}

/* register form: calls chatbox_clear_eFlags2009F38 outside this file, which has no typed C interface yet */
static void clear_chatbox_eflags_sub_8126DF8(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
}

/* register form: calls sub_81272C4 outside this file, which has no typed C interface yet */
static void tail_call_sub_81272C4(void) {
    GUEST_CALL(sub_81272C4);
}

/* Clear chatbox state when a screen fade is active, then jump to the battle-state handler. */
static void fade_chatbox_guard(u32 chatbox_struct_addr) {
    if (screen_fade_active_sub_8126DF8()) {
        clear_chatbox_eflags_sub_8126DF8(0x40);
        ST8(chatbox_struct_addr + 2, 4); /* set chatbox state byte to "hidden" */
    }
    tail_call_sub_81272C4(); /* sets r0-r7, lr, nz/c/v; reads r2, r4, r5, r10 */
}

/* sub_8126DF8(r5 = chatbox struct ptr, r2/r4/r10 = context passed through to sub_81272C4) */
Next sub_8126DF8(void) {
    fade_chatbox_guard(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8126FF0 ── */
/* register form: calls entity_runAllUpdateHandlers_80465BC outside this file, which has no typed C interface yet */
static void run_all_entity_updates(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
}

/* register form: calls sub_8123360 outside this file, which has no typed C interface yet */
static bool fade_start_pending(void) {
    GUEST_CALL(sub_8123360);
    return R0 != 0;
}

/* register form: calls sub_811BED4 outside this file, which has no typed C interface yet */
static void begin_fade_transition(u32 arg0, u32 arg1) {
    R0 = arg0;
    R1 = arg1;
    GUEST_CALL(sub_811BED4);
}

/* register form: calls sub_80465F8 outside this file, which has no typed C interface yet */
static void menu_draw(void) {
    GUEST_CALL(sub_80465F8);
}

/* register form: calls sub_81272C4 outside this file, which has no typed C interface yet */
static void menu_anim_tick(void) {
    GUEST_CALL(sub_81272C4);
}

/* register form: calls sub_8127374 outside this file, which has no typed C interface yet */
static void menu_state_tick(void) {
    GUEST_CALL(sub_8127374);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool is_fading(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();  /* Z set → fade not active */
}

/* register form: calls chatbox_clear_eFlags2009F38 outside this file, which has no typed C interface yet */
static void clear_chatbox_eflags(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
}

/* register form: calls sub_81440D8 outside this file, which has no typed C interface yet */
static void sub_81440D8_call(void) {
    GUEST_CALL(sub_81440D8);
}

/* register form: calls sub_8046664 outside this file, which has no typed C interface yet */
static void sub_8046664_call(void) {
    GUEST_CALL(sub_8046664);
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void clear_event_flag(u32 page, u32 index) {
    R0 = page;
    R1 = index;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void show_pet_chatbox(u32 sub, u32 state_flags) {
    R1 = sub;
    R2 = state_flags;
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_bytes(u32 dst, u32 size) {
    R0 = dst;
    R1 = size;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls pet_menuExitToMap_811F708 outside this file, which has no typed C interface yet */
static void pet_exit_to_map(void) {
    GUEST_CALL(pet_menuExitToMap_811F708);
}

/* One tick of the pet-menu screen: update entities, advance fade, and
 * dispatch the current dialogue/choice based on the battle-state flags.
 *
 * battle_state: pointer to the current menu battle-state block
 *   +0x00 (u8) : dialogue result byte (written here)
 *   +0x0C (u32) : bit flags – bit 0 = "first choice", bit 1 = "second choice",
 *                 neither = user exited
 * toolkit:       pointer to the global Toolkit (used for SubmenuPtr) */
static void pet_menu_screen_tick(u32 battle_state, u32 toolkit) {
    run_all_entity_updates();

    /* Start a fade transition if one hasn't been initiated yet */
    if (!fade_start_pending())
        begin_fade_transition(0, 5);

    menu_draw();
    menu_anim_tick();
    menu_state_tick();

    /* Only process dialogue when the screen fade is actively in progress */
    if (!is_fading())
        return;

    clear_chatbox_eflags(0x40);
    sub_81440D8_call();
    sub_8046664_call();

    /* Clear event flag 0x1713 (page 0x17, bit 0x13) */
    clear_event_flag(0x17, 0x13);

    u32 flags = LD32(battle_state + 0xC);
    u8 result;

    if (flags & 1) {
        result = 0x24;
        show_pet_chatbox(1, flags);
    } else if (flags & 2) {
        result = 0x2C;
        show_pet_chatbox(2, flags);
    } else {
        /* Neither choice flag set – user backed out, return to map */
        pet_exit_to_map();
        return;
    }

    clear_chatbox_eflags(0x40);

    /* Clear the submenu buffer (128 bytes) */
    u32 submenu = LD32(toolkit + oToolkit_SubmenuPtr);
    zero_fill_bytes(submenu, 0x80);

    /* Record the dialogue/choice result */
    ST8(battle_state, result);
}

/* sub_8126FF0(r5 = battle_state ptr, r10 = toolkit ptr) -> (flags, r0-r4 from callees) */
Next sub_8126FF0(void) {
    pet_menu_screen_tick(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_81270D0 ── */
/* register form: calls sub_811FB84 outside this file, which has no typed C interface yet */
static u32 sub_811FB84_call(u32 src_addr, u32 size) {
    R0 = src_addr;
    R1 = size;
    GUEST_CALL(sub_811FB84);
    return R0;
}

/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decompAndCopyData_call(u32 initRefs) {
    R0 = initRefs;
    GUEST_CALL(decompAndCopyData);
}

/* register form: calls sub_8123300 outside this file, which has no typed C interface yet */
static void sub_8123300_call(u32 script_table, u32 dst, u32 decomp_buf) {
    R0 = script_table;
    R1 = dst;
    R2 = decomp_buf;
    GUEST_CALL(sub_8123300);
}

/* Build the unknown_20096E0 dispatch table by reading the per-entry header
 * (a source/size pair from off_8127160), inflating it through sub_811FB84, and
 * copying the 12 bytes that follow into the table. Then run the two
 * decompAndCopyData initializers and finally sub_8123300 with the script list. */
static void build_dispatch_table_and_init(void) {
    u32 dst = A_unk_20096E0;
    u32 src = A_off_8127160;
    u32 dst_off = 0;
    u32 src_off = 0;

    while (src_off < 0x20) {
        u32 src_addr = LD32(src + src_off);
        src_off += 4;
        u32 src_size = LD32(src + src_off);
        src_off += 4;

        u32 inflated = sub_811FB84_call(src_addr, src_size);
        ST32(dst + dst_off, inflated);
        dst_off += 4;

        for (u32 i = 0; i < 2; i++) {
            u32 v = LD32(src + src_off);
            ST32(dst + dst_off, v);
            dst_off += 4;
            src_off += 4;
        }
    }
    ST32(dst + dst_off, 0);

    decompAndCopyData_call(A_unk_20096E0);
    decompAndCopyData_call(A_byte_8127188);

    sub_8123300_call(A_off_8127120, A_eTextScript201BF20, A_eDecompBuffer2013A00);
}

/* sub_81270D0: initialize the unknown_20096E0 dispatch table and the
 * text-script/decompression tables used by the title screens. */
Next sub_81270D0(void) {
    build_dispatch_table_and_init();
    RETURN_TO_CALLER();
}

/* ── sub_81272C4 ── */
/* register form: calls sub_812730C outside this file, which has no typed C interface yet;
 * preserves R4 (declared preserved output) across the callee via save/restore */
static u32 call_sub_812730C(u32 source, u32 type, u32 param, u32 flag, u32 pool_off) {
    u32 saved_r4 = R4;
    R0 = source; R1 = type; R2 = param; R3 = flag;
    R4 = A_byte_81268FC;
    R6 = pool_off;
    R7 = A_dword_81268F8;
    GUEST_CALL(sub_812730C);
    R4 = saved_r4;
    return R0;
}

/* register form: sets R0 and NZCV from CMP(lhs, rhs); required because the early-return
 * paths leave a CMP as the final flag-setting instruction after a GUEST_CALL */
static void guest_cmp(u32 lhs, u32 rhs) {
    R0 = lhs;
    SET_FLAGS_CMP(lhs, rhs);
}

/* Dispatch a battle-object command for the object at obj_ptr.
 * If byte[obj_ptr+0x10] is zero: queue CMD_B directly.
 * Otherwise: queue CMD_A, then only fall through to CMD_B when
 *   byte[obj_ptr+0x0F] != 2 AND byte[obj_ptr+0x10] == 1. */
static void sub_81272C4_impl(u32 obj_ptr) {
    u32 active = LD8(obj_ptr + 0x10);

    if (active != 0) {
        /* Queue CMD_A: source 4, flag 0x13, pool offset 0x10000000 */
        call_sub_812730C(4, 0x34, 0, 0x13, 0x10000000);

        u32 state = LD8(obj_ptr + 0x0F);
        if (state == 2) {
            /* State 2: done after CMD_A, leave CMP flags */
            guest_cmp(state, 2);
            return;
        }

        active = LD8(obj_ptr + 0x10);
        if (active != 1) {
            /* Not state 1: done after CMD_A, leave CMP flags */
            guest_cmp(active, 1);
            return;
        }
        /* State 1: fall through to queue CMD_B as well */
    }

    /* CMD_B: source 0xE8, flag 0x12, pool offset 0 */
    call_sub_812730C(0xE8, 0x34, 0, 0x12, 0);
}

/* sub_81272C4(r4 = saved, r5 = battle-object ptr, r10, lr) */
Next sub_81272C4(void) {
    sub_81272C4_impl(R5);
    RETURN_TO_CALLER();
}

/* ── sub_81273B4 ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_163_set(void) {
    R0 = 1;
    R1 = 0x63;  /* (1, 99) encodes flag 163 */
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls sub_8121198 outside this file, which has no typed C interface yet */
static u32 get_toolkit_counter(u32 toolkit) {
    R6 = toolkit;
    GUEST_CALL(sub_8121198);
    return R0;
}

/* register form: calls sub_8127578 outside this file, which has no typed C interface yet */
static void show_text_script(u32 script_num, u32 toolkit) {
    R0 = script_num;
    R1 = 0;
    R2 = 0x600AC00;
    R3 = A_eTextScript201BF20;
    R4 = A_unk_201A600;
    R6 = toolkit;
    GUEST_CALL(sub_8127578);
}

/* Display a progress-tracking text message for flag 163.
 * Increments a counter stored at toolkit+0xC; once it reaches 15 the counter
 * is left alone and a "finished" script (0x13) is shown instead of the
 * "in-progress" script (0x12). */
static void select_and_display_script(u32 toolkit_container) {
    if (!event_flag_163_set())
        return;

    u32 toolkit = LD32(toolkit_container + oToolkit_Unk200a220_Ptr);
    u32 counter = get_toolkit_counter(toolkit) + 1;

    u32 script_num;
    if (counter == 0xF) {
        script_num = 0x13;   /* reached the end – show completion text */
    } else {
        ST32(toolkit + 0xC, counter);
        script_num = 0x12;   /* still progressing – show count text */
    }

    show_text_script(script_num, toolkit);
}

/* sub_81273B4(r10 = toolkit container) – no meaningful return value */
Next sub_81273B4(void) {
    select_and_display_script(R10);
    RETURN_TO_CALLER();
}

/* ── sub_81273F0 ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool flag_163_set(void) {
    R0 = 1;
    R1 = 0x63;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();  /* Z set means flag is clear; we want "flag is set" */
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_bg_tiles_sub_81273F0(u32 j, u32 i, u32 block, const void *ids,
                          u32 j_sz, u32 i_sz, u32 map_ptr) {
    R0 = j;
    R1 = i;
    R2 = block;
    R3 = (u32)ids;
    R4 = j_sz;
    R5 = i_sz;
    R7 = map_ptr;
    GUEST_CALL(CopyBackgroundTiles);
}

/* Paint the 3×2 tile block at the column stored in the map row, gated by event flag 163. */
static void paint_event_tile_block(u32 map_data) {
    if (!flag_163_set())
        return;

    /* Read the tile-column offset (signed byte at map_data+0xD), shift by +2. */
    s32 col = (s32)(s8)LD8(map_data + 0xD) + 2;

    copy_bg_tiles_sub_81273F0((u32)col, 9, 3, (const void *)A_byte_8126B10, 3, 2, map_data);
}

Next sub_81273F0(void) {
    paint_event_tile_block(R5);
    RETURN_TO_CALLER();
}

/* ── sub_812741C ── */
/* Text display constants for the PET Navi intro sequence */
#define NAVI_TEXT_BUFFER   0x6008020u
#define NAVI_TEXT_SCRIPT   A_eTextScript201BF20
#define NAVI_TEXT_DATA     A_byte_2017A00
#define PET_NAVI_STATS_OFF 8          /* toolkit field holding current stat to display */

/* Y screen coordinates (row*16 - 32) for each stat line */
#define STAT_Y_LEVEL  0u
#define STAT_Y_ATK    ((0x40u << 4) - 0x20u)
#define STAT_Y_SPD    ((0x80u << 4) - 0x20u)
#define STAT_Y_TYPE   ((0xC0u << 4) - 0x20u)

#define SPECIES_TABLE_LEN 0x3Au
#define NO_NAVI_MSG       0x51u
#define PET_NAVI_TYPE_STAT  0x17
#define PET_NAVI_SPECIES_STAT 5

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet.
 * Saves r4-r7 because callees may clobber them (original uses push {r4-r7}/pop {r4-r7}). */
static u8 get_cur_pet_navi_sub_812741C(void) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    GUEST_CALL(GetCurPETNavi);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return (u8)R0;
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet. */
static u8 get_pet_navi_stat_sub_812741C(u8 navi, u8 stat) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsByte);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return (u8)R0;
}

/* register form: calls notZero_eByte200AD04 outside this file, which has no typed C interface yet. */
static bool byte200ad04_nonzero(void) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    GUEST_CALL(notZero_eByte200AD04);
    bool result = !ZERO_FLAG();
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return result;
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet. */
static bool event_163_set(void) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = 1;
    R1 = 0x63;   /* EVENT_163 */
    GUEST_CALL(TestEventFlagFromImmediate);
    bool result = !ZERO_FLAG();
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return result;
}

/* register form: calls sub_8127578 outside this file, which has no typed C interface yet.
 * Its 5th argument is passed in R4; we save/restore r4-r7 across the call. */
static void display_navi_text(u32 msg_id, u32 y_coord, u32 buf, u32 script, u32 data) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = msg_id;
    R1 = y_coord;
    R2 = buf;
    R3 = script;
    R4 = data;
    GUEST_CALL(sub_8127578);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* Queue a single PET Navi stat line: store (stat+1) into toolkit temp, then display text. */
static void show_navi_stat(u32 navi_data_ptr, u8 navi, u8 stat_idx, u32 msg_id, u32 y_coord) {
    ST32(navi_data_ptr + PET_NAVI_STATS_OFF, (u32)(get_pet_navi_stat_sub_812741C(navi, stat_idx) + 1));
    display_navi_text(msg_id, y_coord, NAVI_TEXT_BUFFER, NAVI_TEXT_SCRIPT, NAVI_TEXT_DATA);
}

/* Resolve message ID for the type/species line.
 * If type stat (0x17) is non-zero, map via byte_812756C.
 * Otherwise search species (stat 5) in byte_81274F8 and index into byte_8127532;
 * default to index 2 if not found. */
static u8 resolve_navi_type_message(u8 navi) {
    u8 type = get_pet_navi_stat_sub_812741C(navi, PET_NAVI_TYPE_STAT);
    if (type != 0)
        return LD8(A_byte_812756C + type);

    u8 species = get_pet_navi_stat_sub_812741C(navi, PET_NAVI_SPECIES_STAT);
    u32 idx = 0;
    while (idx < SPECIES_TABLE_LEN) {
        if (LD8(A_byte_81274F8 + idx) == species)
            return LD8(A_byte_8127532 + idx);
        idx++;
    }
    return LD8(A_byte_8127532 + 2);  /* species not in table, fallback */
}

/* PET Navi intro screen: displays level, attack, speed stat lines then type/species message.
 * toolkit_base (r10) holds the toolkit pointer. */
static void pet_navi_intro(u32 toolkit_base) {
    u32 navi_ptr = LD32(toolkit_base + oToolkit_Unk200a220_Ptr);

    u8 navi = get_cur_pet_navi_sub_812741C();
    show_navi_stat(navi_ptr, navi, 1, 5, STAT_Y_LEVEL);    /* level */
    show_navi_stat(navi_ptr, navi, 2, 6, STAT_Y_ATK);      /* attack */
    show_navi_stat(navi_ptr, navi, 3, 7, STAT_Y_SPD);      /* speed */

    u8 msg;
    navi = get_cur_pet_navi_sub_812741C();
    if (navi == 0 && byte200ad04_nonzero() && !event_163_set()) {
        /* No Navi bonded, special byte nonzero, event 163 not seen: show placeholder */
        msg = NO_NAVI_MSG;
    } else {
        msg = resolve_navi_type_message(get_cur_pet_navi_sub_812741C());
    }

    display_navi_text(msg, STAT_Y_TYPE, NAVI_TEXT_BUFFER, NAVI_TEXT_SCRIPT, NAVI_TEXT_DATA);
}

/* sub_812741C(r10 = toolkit base pointer) -> preserves r4-r12 */
Next sub_812741C(void) {
    pet_navi_intro(R10);
    RETURN_TO_CALLER();
}

/* ── background_drawSceneSection_81275A8 ── */
/* background_drawSceneSection_81275A8(r5 = background/scene state ptr):
 * Redraws a background section: reads the page/screen index stored at +0xD of
 * the state struct, offsets it by 0x11 (its row in the tile map), and copies an
 * 11x8 block of 32x32-pixel tile blocks (block #3) from the tile-id table
 * byte_8126930 into the background.
 * r4-r7 are work registers of the caller and must be preserved. */
Next background_drawSceneSection_81275A8(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    /* Top row (j) of the region to draw: screen index + 0x11. */
    s32 j = (s8)LD8(R5 + 0xD) + 0x11;

    R0 = (u32)j;              /* j: destination row */
    R1 = 3;                   /* i: destination column */
    R2 = 3;                   /* which 32x32 tile block to fill */
    R3 = A_byte_8126930;      /* tile ids */
    R4 = 0xB;                 /* j_size: 11 blocks tall */
    R5 = 8;                   /* i_size: 8 blocks wide */
    GUEST_CALL(CopyBackgroundTiles);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_81275C8 ── */
/* register form: calls sub_8127580 outside this file, which has no typed C interface yet */
static void render_chip_slot(u32 slot_index, u32 palette_offset,
                             u32 sel_byte, u32 toolkit_addr) {
    u32 saved_r4 = R4;
    ST32(toolkit_addr + 8, sel_byte);
    R0 = slot_index;
    R1 = palette_offset;
    R2 = 0x6009000;
    R3 = A_eTextScript201BF20;
    R4 = A_unk_2018A00;
    GUEST_CALL(sub_8127580);
    R4 = saved_r4;
}

/* Render the three chip slots of a battle toolkit selection row.
 * r5 points at a struct whose bytes 0x14/0x15/0x16 are the per-slot
 * selected chip index; r10 carries a pointer to a struct whose +8
 * field is updated before each slot's render call. r4..r9 preserved. */
static void render_chip_slots(u32 r5, u32 r10) {
    u32 toolkit_obj = LD32(r10 + oToolkit_Unk200a220_Ptr);
    render_chip_slot(8,  0x000, (u32)LD8(r5 + 0x14), toolkit_obj);
    render_chip_slot(9,  0x400, (u32)LD8(r5 + 0x15), toolkit_obj);
    render_chip_slot(0xA, 0x800, (u32)LD8(r5 + 0x16), toolkit_obj);
}

Next sub_81275C8(void) {
    render_chip_slots(R5, R10);
    RETURN_TO_CALLER();
}

/* ── bg_copyTileBlockFromStructOffset_8127618 ── */
/* bg_copyTileBlockFromStructOffset_8127618(r5 = source struct): redraw a 3x3 block of background tiles.
 * The tile column offset is taken from the signed byte at [r5+0xD] and
 * biased by the base tile 0x23; the tile ids come from byte_81269E0.
 * r2/r3 are whatever CopyBackgroundTiles leaves behind; r4-r7 are restored. */
Next bg_copyTileBlockFromStructOffset_8127618(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    /* first tile index: header byte + base tile 0x23 */
    u32 first_tile = (u32)((s8)LD8(R5 + 0xD)) + 0x23;

    R0 = first_tile;            /* j: starting tile index */
    R1 = 3;                     /* i: starting row */
    R2 = 3;                     /* which_tile_block_32x32 */
    R3 = A_byte_81269E0;        /* tileIds */
    R4 = 0xB;                   /* j_size */
    R5 = 6;                     /* i_size */
    R7 = saved_r5;              /* CopyBackgroundTiles also reads the source struct from r7 */
    GUEST_CALL(CopyBackgroundTiles);

    /* the caller's r4-r7 are preserved (the original pops them from the stack) */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_8127638 ── */
/* Compact the PET navi table for the current battle toolkit.
 * Scans toolkit entries for navi-compatible PET navis, builds a compact ID list
 * and a secondary data table, then removes zero gaps from the ID list. */

/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet */
static void zero_fill(u32 dest, u32 size) {
    R0 = dest;
    R1 = size;
    GUEST_CALL(ZeroFillByByte);
}

/* register form: calls toolkit_getUnk2004190EntryPtr_813B9FC outside this file, which has no typed C interface yet */
static u32 toolkit_entry_ptr(u32 index) {
    R0 = index;
    GUEST_CALL(toolkit_getUnk2004190EntryPtr_813B9FC);
    return R0;
}

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u32 get_cur_pet_navi_sub_8127638(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8127638(u32 flag_page, u32 flag) {
    R0 = flag_page;
    R1 = flag;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls sub_81276E4 outside this file, which has no typed C interface yet */
static u32 get_secondary_navi_table(u32 start_index) {
    R4 = start_index;
    GUEST_CALL(sub_81276E4);
    return R0;
}

#define NAVI_LIST_SIZE       0x28
#define TOOLKIT_SCAN_COUNT   0x19
#define COMPACTION_RANGE     0x22
#define NAVI_COUNT_OFFSET    0x1E

static void process_pet_navi_table(u32 toolkit) {
    /* Clear the navi ID list and secondary data table */
    zero_fill(A_byte_201D74C, NAVI_LIST_SIZE);
    zero_fill(A_unk_2024394, NAVI_LIST_SIZE);

    /* First pass: scan toolkit entries; record navi IDs if no PET is active */
    u32 i = 0;
    for (; i < TOOLKIT_SCAN_COUNT; i++) {
        u32 entry = toolkit_entry_ptr(i);
        u16 val = (u16)LD16(entry);
        if (val == 0)
            continue;
        if (get_cur_pet_navi_sub_8127638() != 0)
            continue;
        ST8(A_byte_201D74C + i, (u8)(val >> 2));
        ST16(toolkit + NAVI_COUNT_OFFSET, (u16)(LD16(toolkit + NAVI_COUNT_OFFSET) + 1));
    }

    /* Second pass (event flag 163): read from a secondary table of 8-byte entries */
    if (event_flag_set_sub_8127638(1, 0x63)) {
        u32 tbl = get_secondary_navi_table(i);
        u32 j = 0; /* byte offset into unk_2024394 */

        for (;;) {
            u32 field0 = LD32(tbl);
            if (field0 == (u32)-1)
                break;

            u8 id = (u8)((field0 >> 2) + 0x40);
            ST8(A_byte_201D74C + i, id);
            ST8(A_unk_2024394 + j, id);
            j += 2;

            u32 field1 = LD32(tbl + 4);
            ST16(A_unk_2024394 + j, (u16)field1);
            j += 2;

            ST16(toolkit + NAVI_COUNT_OFFSET, (u16)(LD16(toolkit + NAVI_COUNT_OFFSET) + 1));
            i += 1;
            tbl += 8;
        }
    }

    /* Compact: shift non-zero bytes in the ID list toward the front */
    u32 end = A_byte_201D74C + COMPACTION_RANGE;
    u32 pos = A_byte_201D74C;
    for (u32 k = 0; k < COMPACTION_RANGE; k++, pos++) {
        if (LD8(pos) != 0)
            continue;

        u32 write = pos;
        for (u32 scan = pos + 1; scan < end; scan++) {
            u8 val = LD8(scan);
            if (val == 0)
                continue;
            ST8(write, val);
            ST8(scan, 0);
            write++;
        }
    }
}

/* sub_8127638(toolkit = r5) — builds PET navi list, preserves r4-r7 via stack */
Next sub_8127638(void) {
    process_pet_navi_table(R5);
    RETURN_TO_CALLER();
}

/* ── bg_Draw8x8TileBlock3FromTable_81277E0 ── */
/* bg_Draw8x8TileBlock3FromTable_81277E0: redraw an 8x8 block of background tiles.
 *
 * r5 points to an object whose signed byte at offset 0x0D holds a tile index;
 * adding 0x31 turns it into the index of the first entry in the tile-id table
 * byte_8126A90. Those tile ids are copied into tile block 3 of the 32x32
 * background, starting at tile coordinate (j, 3) and spanning 8x8 tiles.
 *
 * r4-r7 are caller inputs that must come back unchanged, so they are saved
 * around the call (the original pushes/pops them). CopyBackgroundTiles
 * receives the block coordinates in r0-r5 and leaves r2 = 3 as observed by
 * callers. */
Next bg_Draw8x8TileBlock3FromTable_81277E0(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* j: first tile to draw, from the object's tile index at offset 0x0D. */
    u32 firstTile = (u32)((s8)LD8(R5 + 0xD) + 0x31);

    R0 = firstTile;        /* j: starting tile index */
    R1 = 3;                /* i: starting tile row/column */
    R2 = 3;                /* which 32x32 tile block of the background */
    R3 = A_byte_8126A90;   /* tileIds: tile number table */
    R4 = 8;                /* j_size */
    R5 = 8;                /* i_size */
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore the caller's r4-r7 (the original pop {r4-r7,pc}). */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_81278D8 ── */
/* sub_81278D8(r5 = struct pointer): return struct + 0x40 + (s8)(r5[0xD]) * 8,
 * i.e. the address of entry [r5[0xD]] of the 8-byte-element array at r5+0x40. */
Next sub_81278D8(void) {
    s32 index = (s8)LD8(R5 + 0xD);
    u32 base = LD32(R5 + 0x40);
    R0 = base + (index * 8);
    RETURN_TO_CALLER();
}

/* ── sub_8127A1C ── */
/* Fade timer value stored to the render-info header on scene init. */
#define RENDER_FADE_TIMER  0x1F40

/* Two output registers from sub_8120CC8 (fade-palette lookup). */
typedef struct { u32 step, frame; } FadePair;

/* ---- register-form helpers for game functions outside this file ---- */

/* register form: calls zeroFillVRAM outside this file, which has no typed C interface yet */
static void call_zero_fill_vram_sub_8127A1C(void) {
    GUEST_CALL(zeroFillVRAM);
}

/* register form: calls ZeroFill_byte_3001960 outside this file, which has no typed C interface yet */
static void call_zero_fill_byte_3001960(void) {
    GUEST_CALL(ZeroFill_byte_3001960);
}

/* register form: calls ZeroFillGFX30025c0 outside this file, which has no typed C interface yet */
static void call_zero_fill_gfx_sub_8127A1C(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
}

/* register form: calls sub_80015FC outside this file, which has no typed C interface yet */
static void call_init_something(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_80015FC);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void call_set_screen_fade(u32 a, u32 b) {
    R0 = a; R1 = b;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls sub_8046664 outside this file, which has no typed C interface yet */
static void call_sub_8046664_sub_8127A1C(void) {
    GUEST_CALL(sub_8046664);
}

/* register form: calls sub_8127CB8 outside this file, which has no typed C interface yet */
static void call_sub_8127CB8(void) {
    GUEST_CALL(sub_8127CB8);
}

/* register form: calls sub_8120CC8 outside this file, which has no typed C interface yet */
static FadePair call_fade_lookup(u32 a, u32 b) {
    R0 = a; R1 = b;
    GUEST_CALL(sub_8120CC8);
    FadePair p; p.step = R0; p.frame = R1;
    return p;
}

/* register form: calls sub_81283A0 outside this file, which has no typed C interface yet */
static void call_sub_81283A0(void) {
    GUEST_CALL(sub_81283A0);
}

/* register form: calls sub_8128500 outside this file, which has no typed C interface yet */
static void call_sub_8128500(void) {
    GUEST_CALL(sub_8128500);
}

/* register form: calls palette_fillBufferWithFadeByte_8128260 outside this file, which has no typed C interface yet */
static void call_palette_fill_fade(void) {
    GUEST_CALL(palette_fillBufferWithFadeByte_8128260);
}

/* register form: calls sub_8127E9C outside this file, which has no typed C interface yet */
static void call_sub_8127E9C(void) {
    GUEST_CALL(sub_8127E9C);
}

/* register form: calls sub_8128008 outside this file, which has no typed C interface yet */
static void call_sub_8128008(void) {
    GUEST_CALL(sub_8128008);
}

/* register form: calls bg_ReloadBackgroundRegion_5_3_81281C0 outside this file, which has no typed C interface yet */
static void call_bg_reload_region(void) {
    GUEST_CALL(bg_ReloadBackgroundRegion_5_3_81281C0);
}

/* register form: calls sub_81281E0 outside this file, which has no typed C interface yet */
static void call_sub_81281E0(void) {
    GUEST_CALL(sub_81281E0);
}

/* register form: calls sub_8128280 outside this file, which has no typed C interface yet */
static void call_sub_8128280(void) {
    GUEST_CALL(sub_8128280);
}

/* ---- typed function ---- */

/*
 * Full scene-reset routine called when entering/exiting a battle or cutscene.
 * Clears VRAM and graphics buffers, initialises the render-info header,
 * sets the transition/fade state struct at r5, loads a fade palette entry,
 * writes the screen-fade mode byte, and rebuilds backgrounds & palettes.
 */
static void init_battle_scene(u32 state, u32 toolkit) {
    /* 1. Clear display memory */
    call_zero_fill_vram_sub_8127A1C();
    call_zero_fill_byte_3001960();
    call_zero_fill_gfx_sub_8127A1C();

    /* 2. One-time subsystem init (r0=0x10) */
    call_init_something(0x10);

    /* 3. Initialise render-info struct: set fade timer, clear per-plane flags */
    u32 render = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render, RENDER_FADE_TIMER);
    ST16(render + 0x10, 0);
    ST16(render + 0x12, 0);
    ST16(render + 0x14, 0);
    ST16(render + 0x16, 0);
    ST16(render + 0x18, 0);
    ST16(render + 0x1A, 0);

    /* 4. Transition state: mode byte and sub-state byte */
    ST8(state + 1, 4);   /* transition mode = active */
    ST8(state + 2, 0);   /* sub-state = idle */

    /* 5. Start screen fade (type 8, speed 0x10) */
    call_set_screen_fade(8, 0x10);

    /* 6. Additional scene setup calls */
    call_sub_8046664_sub_8127A1C();
    call_sub_8127CB8();

    /* 7. Save current scene ID into transition struct */
    ST16(state + 0x1E, (u16)LD32(A_dword_2001140));

    /* 8. Initialise fade/transition parameters */
    ST16(state + 0x28, 4);      /* step count */
    ST16(state + 0x20, 0);      /* current step */
    ST16(state + 0x24, 0);
    ST16(state + 0x2A, 0);
    ST16(state + 0x2E, 0);
    ST16(state + 0x22, 0xFF);   /* "no entry" markers */
    ST16(state + 0x26, 0xFF);
    ST16(state + 0x2C, 0xFF);
    ST16(state + 0x30, 0xFF);

    /* 9. Look up fade palette step (a=0, b=dword_8127980) */
    FadePair fade = call_fade_lookup(0, A_dword_8127980);
    ST8(state + 0x0C, fade.step);
    ST8(state + 0x0E, fade.frame);
    ST8(state + 0x0F, fade.step);
    ST8(state + 0x10, fade.frame);

    /* 10. Post-fade setup */
    call_sub_81283A0();

    /* 11. Write fade-engine control bytes */
    ST8(A_byte_2000FE0,     7);  /* fade type */
    ST8(A_byte_2000FE0 + 1, 4);  /* fade direction */

    /* 12. Rebuild display: palettes, background map, tile regions */
    call_sub_8128500();
    call_palette_fill_fade();
    call_sub_8127E9C();
    call_sub_8128008();
    call_bg_reload_region();
    call_sub_81281E0();
    call_sub_8128280();
}

/* ---- thin adapter ---- */

/* sub_8127A1C(r5 = transition-state ptr, r10 = toolkit ptr) — full scene reset */
Next sub_8127A1C(void) {
    init_battle_scene(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8127ACC ── */
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

/* ── sub_8127B08 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static u32 call_is_screen_fade_active_sub_8127B08(int *fade_active_out) {
    GUEST_CALL(IsScreenFadeActive);
    *fade_active_out = ZERO_FLAG() ? 0 : 1;   /* Z=1 → not active */
    return R0;
}

/* Write 4 to [obj+2] (set fade mode) when the screen fade is active.
 * When the fade is NOT active, pass through IsScreenFadeActive's r0 unchanged.
 * The callee's flags (n z c v) remain in the emulator from GUEST_CALL;
 * on the active path the original's "mov r0, #4" leaves N=0, Z=0 which
 * matches the callee's Z=0/N=0 (boolean return), so no re-set is needed. */
static u32 set_fade_mode_if_active(u32 obj_ptr) {
    int active;
    u32 r0 = call_is_screen_fade_active_sub_8127B08(&active);
    if (active) {
        r0 = 4;
        ST8(obj_ptr + 2, 4);
    }
    return r0;
}

/* sub_8127B08(r5 = battle-object pointer) -> r0, nz c v */
Next sub_8127B08(void) {
    R0 = set_fade_mode_if_active(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8127B18 ── */
/* register form: calls IsButtonPressed outside this file, which has no typed C interface yet */
static bool is_button_pressed(u32 button) {
    R0 = button;
    GUEST_CALL(IsButtonPressed);
    return !ZERO_FLAG();
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_effect(u32 effect) {
    R0 = effect;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls submenu_ConfirmExitAndFade_811FB64 outside this file, which has no typed C interface yet */
static void confirm_exit_and_fade_811FB64(u32 arg) {
    R0 = arg;
    GUEST_CALL(submenu_ConfirmExitAndFade_811FB64);
}

/* register form: calls sub_8128318 outside this file, which has no typed C interface yet */
static void call_sub_8128318(void) {
    GUEST_CALL(sub_8128318);
}

/* register form: calls sub_8128008 outside this file, which has no typed C interface yet */
static void call_sub_8128008_sub_8127B18(void) {
    GUEST_CALL(sub_8128008);
}

/* register form: calls bg_ReloadBackgroundRegion_5_3_81281C0 outside this file, which has no typed C interface yet */
static void call_bg_reload_background_region_5_3_81281C0(void) {
    GUEST_CALL(bg_ReloadBackgroundRegion_5_3_81281C0);
}

/* register form: calls sub_8120CC8 outside this file, which has no typed C interface yet */
static void call_sub_8120CC8(u32 a, u32 b, u32 *out0, u32 *out1) {
    R0 = a;
    R1 = b;
    GUEST_CALL(sub_8120CC8);
    *out0 = (u8)R0;
    *out1 = (u8)R1;
}

/* register form: calls sub_811F7F8 outside this file, which has no typed C interface yet */
static u32 call_sub_811F7F8(u32 a, u32 b, u32 c, u32 d) {
    R0 = a;
    R1 = b;
    R2 = c;
    R3 = d;
    GUEST_CALL(sub_811F7F8);
    return R0;
}

/* Process B/A/Start and the held-input helper for this submenu screen. */
static void submenu_handle_input_8127B18(u32 state, u32 toolkit) {
    if (is_button_pressed(JOYPAD_B)) {
        confirm_exit_and_fade_811FB64(8);
        return;
    }

    if (is_button_pressed(JOYPAD_A)) {
        if (LD16(state + 0x1E) == 0) {
            confirm_exit_and_fade_811FB64(8);
            return;
        }
        call_sub_8128318();
        call_sub_8128008_sub_8127B18();
        call_bg_reload_background_region_5_3_81281C0();
        play_sound_effect(SOUND_MENU_SELECT);
        return;
    }

    if (LD16(state + 0x1E) == 0)
        return;

    if (is_button_pressed(JOYPAD_START)) {
        ST8(state + 2, 0x0C);
        play_sound_effect(SOUND_MENU_SELECT);
        u32 out0, out1;
        call_sub_8120CC8(0, A_dword_8127980, &out0, &out1);
        ST8(state + 0xF, out0);
        ST8(state + 0x10, out1);
        return;
    }

    u32 joypad = LD32(toolkit + oToolkit_JoypadPtr);
    u32 low_held = LD16(joypad + oJoypad_LowSensitivityHeld);
    if (call_sub_811F7F8(low_held, 4, 0, 0) != 0) {
        call_sub_8128008_sub_8127B18();
        call_bg_reload_background_region_5_3_81281C0();
    }
}

Next sub_8127B18(void) {
    submenu_handle_input_8127B18(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8127C64 ── */
/* register form: calls sub_81281E0 outside this file, which has no typed C interface yet */
static void scene_step_81281E0(void) {
    GUEST_CALL(sub_81281E0);
}

/* register form: calls sub_8127F54 outside this file, which has no typed C interface yet */
static void scene_step_8127F54(void) {
    GUEST_CALL(sub_8127F54);
}

/* register form: calls sub_8127FE4 outside this file, which has no typed C interface yet */
static void scene_step_8127FE4(void) {
    GUEST_CALL(sub_8127FE4);
}

/* register form: calls sub_8128280 outside this file, which has no typed C interface yet */
static void scene_step_8128280(void) {
    GUEST_CALL(sub_8128280);
}

/* register form: calls entity_runAllUpdateHandlers_80465BC outside this file, which has no typed C interface yet */
static void run_all_entity_update_handlers(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
}

/* register form: calls sub_80465F8 outside this file, which has no typed C interface yet */
static void scene_step_80465F8(void) {
    GUEST_CALL(sub_80465F8);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static u32 screen_fade_active_sub_8127C64(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();          /* IsScreenFadeActive: Z set when no fade is running */
}

/* register form: calls sub_8046664 outside this file, which has no typed C interface yet */
static void scene_step_8046664(void) {
    GUEST_CALL(sub_8046664);
}

/* register form: calls sub_8127EF4 outside this file, which has no typed C interface yet */
static u32 get_exit_value_8127EF4(void) {
    GUEST_CALL(sub_8127EF4);
    return R0;
}

/* register form: calls ClearPETMenuDataFlag outside this file, which has no typed C interface yet */
static void clear_pet_menu_data_flag(u32 r0v, u32 r1v, u32 r2v, u32 r3v) {
    R0 = r0v;
    R1 = r1v;
    R2 = r2v;
    R3 = r3v;
    GUEST_CALL(ClearPETMenuDataFlag);
}

/* register form: calls next_exitToMap_811F728 outside this file, which has no typed C interface yet */
static void next_exit_to_map(void) {
    GUEST_CALL(next_exitToMap_811F728);
}

/* register form: calls pet_menuExitToMap_811F708 outside this file, which has no typed C interface yet */
static void pet_menu_exit_to_map(u32 r0v, u32 r1v, u32 r2v, u32 r3v) {
    R0 = r0v;
    R1 = r1v;
    R2 = r2v;
    R3 = r3v;
    GUEST_CALL(pet_menuExitToMap_811F708);
}

/* Per-frame update of the scenario-end cutscene: run the scene's script steps
 * and the entity update handlers. While no screen fade is running yet, stop
 * here and let the scene run again next frame. Once the fade is underway,
 * finalize: stop the scene, hand the exit value produced by sub_8127EF4 to
 * the game state, and return to the map.
 * work: scene object whose +0xD bit 0 selects where the exit value is stored
 *       and which exit routine to use.
 * toolkit: toolkit pointer (oToolkit_GameStatePtr -> game state). */
static void run_closing_scene(u32 work, u32 toolkit) {
    scene_step_81281E0();
    scene_step_8127F54();
    scene_step_8127FE4();
    scene_step_8128280();
    run_all_entity_update_handlers();
    scene_step_80465F8();

    if (!screen_fade_active_sub_8127C64())
        return;                   /* keep the scene running; flags stay as IsScreenFadeActive left them */

    scene_step_8046664();
    u32 exit_value = get_exit_value_8127EF4();

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u8 mode = LD8(work + 0xD);

    /* bit 0 of the mode byte picks a word store into the game state (+0x78)
     * or a byte store into oGameState_Unk_13 */
    if (mode & 1)
        ST32(game_state + 0x78, exit_value);
    else
        ST8(game_state + oGameState_Unk_13, (u8)exit_value);

    /* the original re-reads the mode byte here rather than reusing the value */
    u8 mode_reread = LD8(work + 0xD);

    if (mode_reread & 1) {
        /* the original enters ClearPETMenuDataFlag with r0 = 1, r1 = 1,
         * r2 = 1, r3 = mode byte; next_exitToMap then runs with whatever
         * registers ClearPETMenuDataFlag left behind */
        clear_pet_menu_data_flag(1, 1, 1, mode);
        next_exit_to_map();
    } else {
        /* the original enters pet_menuExitToMap with r0 = mode byte
         * (re-read), r1 = 1, r2 = 1, r3 = mode byte (first read) */
        pet_menu_exit_to_map(mode_reread, 1, 1, mode);
    }
}

/* sub_8127C64: run the closing scene; r0-r3 and the flags on exit are whatever
 * the last callee left behind, r5/r10 are untouched. */
Next sub_8127C64(void) {
    run_closing_scene(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8127CB8 ── */
/* register form: calls sub_811FB84 outside this file, which has no typed C interface yet */
static u32 compute_tile_descriptor(u32 source, u32 size) {
    R0 = source;
    R1 = size;
    GUEST_CALL(sub_811FB84);
    return R0;
}

/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decompress_data(u32 init_refs) {
    R0 = init_refs;
    GUEST_CALL(decompAndCopyData);
}

/* register form: calls sub_8123300 outside this file, which has no typed C interface yet */
static void decompress_text_table(u32 table, u32 text_dest, u32 decomp_buf) {
    R0 = table;
    R1 = text_dest;
    R2 = decomp_buf;
    GUEST_CALL(sub_8123300);
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_bg_tiles(u32 j, u32 i, u32 block, u32 tile_ids, u32 j_size, u32 i_size) {
    u32 save_r4 = R4, save_r5 = R5, save_r6 = R6, save_r7 = R7;
    R0 = j; R1 = i; R2 = block; R3 = tile_ids; R4 = j_size; R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
    R4 = save_r4; R5 = save_r5; R6 = save_r6; R7 = save_r7;
}

/* Build the tile-descriptor table at unk_20096E0 from the 3-entry source table,
 * then decompress scene data and blit background tiles for the current battle.
 * The flag byte at battle_state+0xD selects which tile-set variant is used. */
static void setup_battle_scene_tiles(u32 battle_state) {
    u32 variant_flag = LD8(battle_state + 0xD);

    /* --- build tile descriptor array (9 entries + null terminator) --- */
    u32 dest_off = 0;
    for (u32 src_off = 0; src_off < 0x30; src_off += 16) {
        /* first two words: compute a descriptor via the tile helper */
        u32 desc = compute_tile_descriptor(
            LD32(A_off_8127DB8 + src_off),
            LD32(A_off_8127DB8 + src_off + 4));
        ST32(A_unk_20096E0 + dest_off, desc);
        dest_off += 4;

        /* next two words: copy verbatim (dest-ptr and size) */
        ST32(A_unk_20096E0 + dest_off, LD32(A_off_8127DB8 + src_off + 8));
        dest_off += 4;
        ST32(A_unk_20096E0 + dest_off, LD32(A_off_8127DB8 + src_off + 12));
        dest_off += 4;
    }
    ST32(A_unk_20096E0 + dest_off, 0); /* null-terminate the descriptor list */

    /* --- decompress main scene data --- */
    decompress_data(A_unk_20096E0);
    decompress_data(A_byte_8127DF0);

    /* pick the variant-specific tile-set initRefs based on flag bit 0 */
    u32 variant_refs = ((variant_flag & 1) != 0)
                     ? A_dword_8127D98
                     : A_off_8127D78;
    decompress_data(variant_refs);

    /* decompress the per-slot text/background pointer table into script RAM */
    decompress_text_table(A_byte_8127D38, A_eTextScript201BA00, A_eDecompBuffer2013A00);

    /* blit the 30×20 tile-grid into VRAM using the selected tile-ID array */
    u32 tile_ids = ((variant_flag & 1) != 0)
                 ? A_unk_2028690
                 : A_unk_201C200;
    copy_bg_tiles(0, 0, 1, tile_ids, 0x1E, 0x14);
}

/* sub_8127CB8(r4, r5 = battle_state, r6, r7, r10, lr) -> preserves r4-r7 */
Next sub_8127CB8(void) {
    setup_battle_scene_tiles(R5);
    RETURN_TO_CALLER();
}

/* ── chatbox_runVariantScript_8127ED4 ── */
/* chatbox_runVariantScript_8127ED4(r0 = text index, [r5+0xD] = variant flag byte):
 * Run one chatbox script from the fixed text archive eTextScript201BA00.
 * If bit 0 of the flag byte at [r5+0xD] is set, the script index is
 * advanced by 10, selecting the alternate variant of the text.
 * Returns with whatever r0-r3 the chatbox script runner leaves behind. */
Next chatbox_runVariantScript_8127ED4(void) {
    u32 scriptIdx = R0;
    u32 variantFlags = LD8(R5 + 0xD);

    R1 = scriptIdx;
    R2 = variantFlags;
    R3 = 1;

    R0 = A_eTextScript201BA00;
    if (variantFlags & 1) {
        R1 = scriptIdx + 0xA;
        GUEST_CALL(chatbox_runScript_803FDA4);
    } else {
        GUEST_CALL(chatbox_runScript_803FD9C);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8127F54 ── */
/* sub_8127F54(r5 = entity, r10, lr):
 *   Per-frame update for an entity on the field: if it is no longer active
 *   (flags cleared or state id == 0x0C terminator), bail out. Otherwise load
 *   an affine-bg parameter block from the entity, push it through sub_811FAF4
 *   along with the tile index in the col/row table, then tick the entity's
 *   duration timer. When the timer hits zero, pull the next (tile, duration)
 *   pair from the dword_8127980 sequence via sub_8120CC8 and store them back. */
Next sub_8127F54(void) {
    u32 entity = R5;

    /* Bail if the entity has been cleared (flag byte == 0) or its state
     * id is 0x0C (terminator). */
    if (LD8(entity + 0x1E) == 0)         RETURN_TO_CALLER();
    if (LD8(entity + 0x02) == 0x0C)      RETURN_TO_CALLER();

    /* Build the affine-bg setup word and forward it (plus the tile id and
     * the per-column tile table) to sub_811FAF4. The 0x40000000 constant
     * is passed to the callee as the destination/base argument; r4 must
     * end up back at the caller's value. */
    {
        u32 saved_r4 = R4;
        u32 bg_word  = 0xE0000;
        u32 col_word = (u32)LD16(entity + 0x20) * 0x10 + 0x18;
        u32 setup    = bg_word | col_word;
        u32 tile_id  = LD8(entity + 0x0C);

        R0 = setup;
        R1 = 0x20003;
        R2 = tile_id;
        R3 = A_byte_8127978;
        R4 = 0x40000000;
        GUEST_CALL(sub_811FAF4);
        R4 = saved_r4;
    }

    /* Tick the entity's duration timer; reset it from the table when it
     * expires. */
    {
        u32 timer = LD8(entity + 0x0E) - 1;
        ST8(entity + 0x0E, (u8)timer);
        SET_FLAGS_NZ(timer);
        if (!ZERO_FLAG())                RETURN_TO_CALLER();

        /* Timer expired: pull the next tile/duration pair from the
         * dword_8127980 sequence via sub_8120CC8. */
        R1 = A_dword_8127980;
        GUEST_CALL(sub_8120CC8);
        ST8(entity + 0x0C, (u8)R0);
        ST8(entity + 0x0E, (u8)R1);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8127FE4 ── */
/* sub_8127FE4(r5 = object pointer)
 * Wrapper that fills out the argument set and forwards to sub_811FA98
 * (a large constructor / dispatcher). r5 is preserved across the call. */
Next sub_8127FE4(void) {
    u32 obj = R5;
    u32 save_r4 = R4;
    u32 save_r6 = R6;
    u32 save_r7 = R7;

    R0 = 0xE10014;
    R1 = 0x20002;
    R2 = LD16(obj + 0x1E);
    R3 = LD16(obj + 0x24);

    /* sub_811FA98 needs R4=4, R6=oSomeTable_30CA4C, R7=0x35. */
    R4 = 4;
    R6 = 0x30CA4C;
    R7 = 0x35;

    GUEST_CALL(sub_811FA98);

    R4 = save_r4;
    R6 = save_r6;
    R7 = save_r7;

    RETURN_TO_CALLER();
}

/* ── sub_81280DC ── */
/* NPC entry event table builder: zero-fills the table, then stamps sequential
 * 16-bit event numbers into each NPC's low/high u16 array using three sub-runs
 * with different flag masks.  Per-entry adjusted event and first-mask flag come
 * from npc_checkEntryEventFlagOrHidden. */

#define TABLE_BASE   A_unk_2026700
#define TABLE_SIZE   0x170
#define ENTRY_SIZE   0x5C
#define HIGH_OFFSET  0x2E     /* (0x17 << 1): offset of the high u16 half */
#define FLAG_LOW     0x7000   /* 7 << 12, used in sub-runs 1 (via callee) and 3 */
#define FLAG_MID     0x9000   /* used in sub-run 2 */
#define RUN1_END     0x16
#define RUN2_END     0x28
#define RUN3_END     0x2E

/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet.
 * Preserves caller's r1,r4,r5,r6,r7; leaves r0,r2,r3 as the callee set them. */
static void zero_fill_sub_81280DC(u32 dest, u32 nbytes) {
    u32 sv1 = R1, sv4 = R4, sv5 = R5, sv6 = R6, sv7 = R7;
    R0 = dest;
    R1 = nbytes;
    GUEST_CALL(ZeroFillByByte);
    R1 = sv1; R4 = sv4; R5 = sv5; R6 = sv6; R7 = sv7;
}

/* register form: calls npc_checkEntryEventFlagOrHidden_812818C outside this file,
 * which has no typed C interface yet.  Returns callee r0 (adjusted event number);
 * writes callee r2 (modified flag) into *out_flag.
 * Preserves caller's r1,r4,r5,r6,r7. */
static u32 check_entry_event(u32 event, u32 byte_off, u32 flag, u32 idx, u32 *out_flag) {
    u32 sv1 = R1, sv4 = R4, sv5 = R5, sv6 = R6, sv7 = R7;
    R0 = event;
    R1 = byte_off;
    R2 = flag;
    R3 = idx;
    R4 = 0;
    GUEST_CALL(npc_checkEntryEventFlagOrHidden_812818C);
    *out_flag = R2;
    u32 result = R0;
    R1 = sv1; R4 = sv4; R5 = sv5; R6 = sv6; R7 = sv7;
    return result;
}

typedef struct { u32 r2, r3; } NpcTableResult;

/* Build the NPC event table for `count` entries starting at event `start_event`.
 * Event number accumulates across all entries (never reloaded from stack). */
static NpcTableResult build_npc_table(u32 start_event, u32 count) {
    NpcTableResult res;
    zero_fill_sub_81280DC(TABLE_BASE, TABLE_SIZE);

    if (count == 0) {
        res.r2 = 7;
        res.r3 = FLAG_LOW;
        return res;
    }

    u32 ev = start_event;
    for (u32 idx = 0; idx < count; idx++) {
        u32 byte_off = ENTRY_SIZE * idx;
        u32 lo = TABLE_BASE + byte_off;
        u32 hi = lo + HIGH_OFFSET;
        u32 flag;
        ev = check_entry_event(ev, byte_off, FLAG_LOW, idx, &flag);

        /* Sub-run 1: 11 u16 pairs, mask = callee-returned flag */
        u32 off = 0;
        while (off < RUN1_END) {
            u32 v = ev | flag;
            ST16(lo + off, v);
            ST16(hi + off, v + 1);
            ev += 2;
            off += 2;
        }
        ev += 2;
        off += 2;

        /* Sub-run 2: 8 u16 pairs, mask = 0x9000 */
        while (off < RUN2_END) {
            u32 v = ev | FLAG_MID;
            ST16(lo + off, v);
            ST16(hi + off, v + 1);
            ev += 2;
            off += 2;
        }
        ev += 2;
        off += 2;

        /* Sub-run 3: 3 u16 pairs, mask = FLAG_LOW */
        while (off < RUN3_END) {
            u32 v = ev | FLAG_LOW;
            ST16(lo + off, v);
            ST16(hi + off, v + 1);
            ev += 2;
            off += 2;
        }
    }

    res.r2 = FLAG_LOW;
    res.r3 = count;
    return res;
}

Next sub_81280DC(void) {
    u32 count = R1;
    NpcTableResult res = build_npc_table(R0, count);
    R2 = res.r2;
    R3 = res.r3;
    SET_FLAGS_CMP(res.r3, count);
    RETURN_TO_CALLER();
}

/* ── npc_checkEntryEventFlagOrHidden_812818C ── */
/* npc_checkEntryEventFlagOrHidden_812818C(r2 = default result, r3 = addend for the table index, r5 = object ptr):
 * Reads the halfword at object+0x24, adds r3, and uses the sum as an index into
 * the 0x20-byte-stride table at dword_2027590.  The upper halfword of the entry's
 * word at +0x18, plus 0x1D20, forms an event flag id.  If that event flag is set,
 * r2 keeps its incoming value; if it is clear, r2 = 0x8000.
 * r0, r1, r3, r4, r5, r10 and lr are left unchanged (r0/r1/r3/r4 via the
 * original's push/pop, the rest because they are simply never written). */
Next npc_checkEntryEventFlagOrHidden_812818C(void) {
    u32 object = R5;
    u32 tableEntry = A_dword_2027590 + ((u32)LD16(object + 0x24) + R3) * 0x20;
    u32 flagId = (LD32(tableEntry + 0x18) >> 16) + 0x1D20;

    /* registers live across the TestEventFlag call must survive it */
    u32 inR0 = R0, inR1 = R1, inR3 = R3, inR4 = R4;
    u32 result = R2;

    R0 = flagId;
    GUEST_CALL(TestEventFlag); /* result: Z set if the flag is clear (r0 is the flag's whole byte) */
    if (cpu.nz == 0)
        result = 0x8000;

    R0 = inR0;
    R1 = inR1;
    R2 = result;
    R3 = inR3;
    R4 = inR4;
    R5 = object;
    RETURN_TO_CALLER();
}

/* ── bg_ReloadBackgroundRegion_5_3_81281C0 ── */
/* bg_ReloadBackgroundRegion_5_3_81281C0(r4..r7, r10 = context):
 * If the halfword at [r5+0x1E] is nonzero, redraw one background region:
 * copy the 0x17x8 tile block of map layer j=5, i=3 from tile block 2,
 * using the tile id table at unk_2026700. Otherwise do nothing.
 * Flags, r0-r3 and r4-r7 are passed through to the caller as the original
 * leaves them (r4-r7 preserved across the call, r0-r3 come back from
 * CopyBackgroundTiles on the call path, r0 keeps the tested halfword on the
 * skip path). */
Next bg_ReloadBackgroundRegion_5_3_81281C0(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    u32 enable = LD16(R5 + 0x1E);
    if (enable != 0) {
        R0 = 5;             /* j */
        R1 = 3;             /* i */
        R2 = 2;             /* tileBlock32x32 */
        R3 = A_unk_2026700; /* tileIds */
        R4 = 0x17;          /* j_size */
        R5 = 8;             /* i_size */
        GUEST_CALL(CopyBackgroundTiles);
        /* CopyBackgroundTiles clobbers r4/r5; the original push/pop restores them. */
        R4 = saved_r4;
        R5 = saved_r5;
    } else {
        /* tst r0, r0 was the last flag-setting instruction on this path. */
        R0 = enable;
        SET_FLAGS_NZ(enable);
    }
    RETURN_TO_CALLER();
}

/* ── palette_fillBufferWithFadeByte_8128260 ── */
/* palette_fillBufferWithFadeByte_8128260: fill the 0x100-halfword buffer at unk_2026870 with the byte
 * stored at byte_812798A, promoted to a halfword pattern (byte << 8).
 * r3 is preserved; r2 still holds the fill value on return. */
Next palette_fillBufferWithFadeByte_8128260(void) {
    R0 = A_unk_2026870;              /* destination buffer */
    R1 = 0x100;                      /* halfword count */
    R2 = (u32)LD8(A_byte_812798A) << 8; /* value byte, spread across the halfword */
    GUEST_CALL(HalfwordFill);
    RETURN_TO_CALLER();
}

/* ── sub_8128280 ── */
/* sub_8128280: For each of up to 4 entries in the battle object table, check the
 * "sub flag" derived from the object's flags+0x18 high half; if the matching
 * event flag (sub_flag + 0x1DA0) is not set, call sub_81282C0 with the index.
 *
 *   r5 = battle object pointer
 *   r10 = (preserved, used by caller)
 *
 *   inputs:  r5 r10 lr
 *   outputs: r5
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r6 r7
 */
Next sub_8128280(void) {
    u16 obj_flags = LD16(R5 + 0x24);
    u16 obj_count = LD16(R5 + 0x1E);

    /* Skip everything if there are no entries. */
    if (obj_count == 0) {
        RETURN_TO_CALLER();
    }

    /* The base pointer dword_2027590 is a struct array whose stride is 0x20. */
    u32 base = (u32)A_dword_2027590;
    const u32 STRIDE = 0x20;

    for (u32 i = 0; i < 4; i++) {
        /* Stop the iteration once we've handled the configured number of entries. */
        if (i >= obj_count) {
            break;
        }

        u32 entry_addr = base + (obj_flags + i) * STRIDE;
        u32 raw = LD32(entry_addr + 0x18);
        u16 sub_flag = (u16)(raw >> 16);

        /* TestEventFlag takes the flag id in r0; 0x1DA0 is the base offset
         * where these per-object event flags live. */
        R0 = sub_flag + 0x1DA0;
        GUEST_CALL(TestEventFlag);
        /* If the flag IS set, skip the helper call (bne = flag set path). */
        if (!ZERO_FLAG()) {
            /* TestEventFlag returned nz: flag set, nothing to do. */
        } else {
            R0 = i;
            GUEST_CALL(sub_81282C0);
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_81284A4 ── */
/* sub_81284A4: store the mode/state pair (7, 4) at byte_2000FE0,
 * exactly as the original does via the byte_2000FE0 base pointer. */
Next sub_81284A4(void) {
    R0 = A_byte_2000FE0;   /* original loads the pointer into r0 */
    R1 = 7;
    ST8(R0, R1);
    R1 = 4;
    ST8(R0 + 1, R1);       /* byte_2000FE1 = byte_2000FE0 + 1 */
    RETURN_TO_CALLER();
}

/* ── sub_81284B4 ── */
typedef struct {
    u32 flagged_count;
    u32 unflagged_count;
} FlaggedEntryCount;

/* Walk the action-sequence byte array at byte_2006530 (length from dword_2001140),
 * look up each byte in the 4-byte-per-entry table at byte_812851C, and count
 * entries whose first byte has bit 7 set vs. clear. */
static FlaggedEntryCount count_flagged_entries(void) {
    s32 remaining = (s32)LD32(A_dword_2001140);
    u32 key_ptr   = A_byte_2006530;
    u32 table     = A_byte_812851C;
    u32 flagged   = 0;
    u32 unflagged = 0;

    do {
        u8 key        = (u8)LD8(key_ptr);
        u8 entry_byte = (u8)LD8(table + (u32)key * 4);
        if (entry_byte & 0x80)
            flagged++;
        else
            unflagged++;
        key_ptr++;
        remaining--;
    } while (remaining > 0);

    return (FlaggedEntryCount){ .flagged_count = flagged, .unflagged_count = unflagged };
}

/* sub_81284B4() -> r0 = flagged_count, r1 = unflagged_count; r4–r7 preserved */
Next sub_81284B4(void) {
    FlaggedEntryCount result = count_flagged_entries();
    R0 = result.flagged_count;
    R1 = result.unflagged_count;
    RETURN_TO_CALLER();
}

/* ── sub_81287DC ── */
/* sub_81287DC:
 *   inputs:  r5 r10 lr
 *   outputs: r5
 *
 * Scene step routine. If a screen fade is currently active, mark the
 * scene as needing to be cancelled (r5->field_2 = 4). Then run the two
 * per-frame scene helpers (sub_8128B4C / sub_8128B98). Finally snapshot
 * the live position/animation fields into the "previous" slots of the
 * scene object so the next frame can compare against them.
 */
Next sub_81287DC(void) {
    /* push {lr} ... pop {pc}: the original saves LR only to throw it
       away on return, so no real stack frame is needed here. */

    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        ST8(R5 + 2, 4);
    }

    GUEST_CALL(sub_8128B4C);
    GUEST_CALL(sub_8128B98);

    /* snapshot live state into the "prev" fields */
    ST16(R5 + 0x26, LD16(R5 + 0x24));
    ST16(R5 + 0x22, LD16(R5 + 0x20));
    ST8 (R5 + 0x0E, LD8 (R5 + 0x0D));

    RETURN_TO_CALLER();
}

/* ── sub_8128B4C ── */
/* sub_8128B4C: Battle object rendering entry — pack a tile/affine descriptor and
 * submit it via the affine-OBJ queue (sub_811FAF4). Reads the battle-object
 * "visible" flag at +0x1E; if unset, returns immediately. Outputs the resulting
 * tile attribute byte back into the object at +0xC.
 *
 * inputs:  r5, r10, lr
 * outputs: r5
 * clobbers: nz, c, v, r0, r1, r2, r3, lr
 * preserved: r4, r5, r6, r7
 */
Next sub_8128B4C(void) {
    /* R4 is callee-preserved by this function; stash and restore it. */
    u32 saved_r4 = R4;

    /* Skip entirely when the object isn't flagged visible. */
    if (LD16(R5 + 0x1E) == 0)
        goto done;

    /* Build the affine attribute word:
     *   base = 0x0100000   (affine-enabled OBJ group 1)
     * If bit 0 of the object's flags (+0xD) is set, OR in 0x680000 (large +
     * double-size affine bits), giving the extended affine variant. */
    u32 attr01 = 0x0100000;
    if (LD8(R5 + 0xD) & 0x01)
        attr01 |= 0x680000;

    /* Layer index (u16 at +0x20) -> row stride of 0x10, plus base offset 0x18. */
    u32 layer_off = ((u32)LD16(R5 + 0x20) * 0x10) + 0x18;
    attr01 |= layer_off;

    /* Hand off to the queue helper. Returns the tile-attribute byte that gets
     * stored back into the object at +0xC. */
    R0 = attr01;
    R1 = 0x20002;
    R2 = LD8(R5 + 0xC);
    R3 = (u32)A_byte_8128678;
    R4 = 0x40000000;
    GUEST_CALL(sub_811FAF4);
    ST8(R5 + 0xC, (u8)R0);

done:
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_8128BF0 ── */
/* sub_8128BF0(r5 = dialogue object):
 *   If the dialogue is still being drawn (state word at +0x1E) and the script
 *   byte selected by (text_id_hi + text_id_lo)*2 + speaker_id is 0x40
 *   (the "end of script" sentinel), kick the object into state 8 and trigger
 *   a fast fade-out so the scene can close. */
Next sub_8128BF0(void) {
    /* "still drawing" flag */
    if (LD16(R5 + 0x1E) == 0)
        RETURN_TO_CALLER();

    /* look up the script byte for the (lo, hi) text pair + speaker */
    u32 idx = (LD16(R5 + 0x24) + LD16(R5 + 0x20)) << 1;
    idx = (idx + LD8(R5 + 0xD)) << 1;
    u8 byte = LD8(A_eTextScript201BF00 + idx);
    if (byte != 0x40)
        RETURN_TO_CALLER();

    /* end-of-text: switch the object to state 8 and fade the screen out */
    ST8(R5 + 2, 8);
    R0 = 0x0C;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);
    RETURN_TO_CALLER();
}

/* ── sub_8128C20 ── */
/* sub_8128C20(r5 = battle object pointer):
 * Build a coordinate from the battle object's HP field (parity + half) and queue
 * a sub_811FA98 call via a command buffer (opcode 0xE1.0x13, key 0x20002).
 * Preserves r4, r5, r6, r7 across the call. */
Next sub_8128C20(void) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    u32 hp = LD16(R5 + 0x1E);
    u32 half = hp >> 1;
    u32 coord = (hp - (half << 1)) + half;

    R0 = (0xE1u << 16) | 0x13;
    R1 = 0x20002;
    R2 = coord;
    R3 = LD16(R5 + 0x24);
    R4 = 4;
    R6 = 0xC240;
    R7 = 0x37;

    GUEST_CALL(sub_811FA98);

    R4 = saved_r4;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── HandleCommMenu81291E8 ── */
/* register form: tail-call dispatch through CommMenuJumpTable81291FC.
   The handler address is loaded from the jump table at runtime (indexed by
   byte [r5+1]); there is no single callee name for GUEST_CALL.
   Original pattern: push {lr}; mov lr,pc; bx handler; pop {pc}. */
static void comm_menu_execute(void) {
    SP -= 4;
    ST32(SP, LR);                              /* push {lr} — save caller's return address */
    R1 = LD8(R5 + 1);                          /* handler index from comm-menu state byte */
    R0 = LD32(A_CommMenuJumpTable81291FC + R1);/* target handler entry point */
    LR = 0x081291F4;                           /* mov lr,pc — handler returns to pop {pc} */
    dispatch(R0);                              /* bx r0 — tail-call dispatch to handler */
    LR = LD32(SP);                             /* pop {pc} — restore caller's return address */
    SP += 4;
}

/* HandleCommMenu81291E8: dispatch to a comm-menu handler via jump table. */
Next HandleCommMenu81291E8(void) {
    comm_menu_execute();
    RETURN_TO_CALLER();
}

/* ── sub_8129248 ── */
/* register form: calls zeroFillVRAM outside this file, which has no typed C interface yet */
static void zero_fill_vram(void) {
    GUEST_CALL(zeroFillVRAM);
}

/* register form: calls ZeroFill_byte_3001960 outside this file, which has no typed C interface yet */
static void zero_fill_byte_3001960(void) {
    GUEST_CALL(ZeroFill_byte_3001960);
}

/* register form: calls ZeroFillGFX30025c0 outside this file, which has no typed C interface yet */
static void zero_fill_gfx_30025c0(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
}

/* register form: calls sub_80015FC outside this file, which has no typed C interface yet */
static void sub_80015FC_call(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_80015FC);
}

/* register form: calls sub_8046664 outside this file, which has no typed C interface yet */
static void sub_8046664_call_sub_8129248(void) {
    GUEST_CALL(sub_8046664);
}

/* register form: calls scene_loadAndStartCutscene_812AF18 outside this file, which has no typed C interface yet */
static void scene_load_and_start_cutscene(void) {
    GUEST_CALL(scene_loadAndStartCutscene_812AF18);
}

/* register form: calls sub_8120CC8 outside this file, which has no typed C interface yet.
 * Returns the r0/r1 the callee left behind. */
static void sub_8120CC8_call(u32 a0, u32 a1, u32 *out0, u32 *out1) {
    R0 = a0;
    R1 = a1;
    GUEST_CALL(sub_8120CC8);
    *out0 = R0;
    *out1 = R1;
}

/* Init battle scene: clear VRAM/GFX, prime render info, start opening cutscene,
 * then write the variant bytes from the dword_8129180 table into the state struct.
 * Returns the r0/r1 the original leaves: r0 = 0 (final `mov r0, #0`),
 * r1 = the second byte sub_8120CC8 produced (still in r1 at return). */
static void init_battle_scene_sub_8129248(u32 toolkit_r10, u32 state_r5, u32 *out_r0, u32 *out_r1) {
    u32 render_info_ptr;
    u32 lo, hi;

    zero_fill_vram();
    zero_fill_byte_3001960();
    zero_fill_gfx_30025c0();
    sub_80015FC_call(0x10);
    sub_8046664_call_sub_8129248();

    render_info_ptr = LD32(toolkit_r10 + oToolkit_RenderInfoPtr);
    ST16(render_info_ptr, 0x1F40);
    ST16(render_info_ptr + 0x10, 0);
    ST16(render_info_ptr + 0x12, 0);
    ST16(render_info_ptr + 0x14, 0);
    ST16(render_info_ptr + 0x16, 0);
    ST16(render_info_ptr + 0x18, 0);
    ST16(render_info_ptr + 0x1A, 0);

    scene_load_and_start_cutscene();

    ST8(state_r5 + 1, 4);
    ST8(state_r5 + 2, 0);
    ST8(A_eStruct200BC30 + 0xC, 0x3C);

    sub_8120CC8_call(0, (u32)A_dword_8129180, &lo, &hi);
    ST8(state_r5 + 0x17, (u8)lo);
    ST8(state_r5 + 0x18, (u8)hi);
    ST8(state_r5 + 0x1B, 0);

    /* The original ends with `mov r0, #0` (which also sets NZ for r0==0); r1 is the
     * byte sub_8120CC8 returned in r1, still live in r1 at the pop. */
    *out_r0 = 0;
    *out_r1 = hi;
}

Next sub_8129248(void) {
    u32 out_r0, out_r1;

    init_battle_scene_sub_8129248(R10, R5, &out_r0, &out_r1);

    R0 = out_r0;
    R1 = out_r1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_81292A8 ── */
/* Jump table of battle state handlers, indexed by byte offset at [r5+2]. */
#define BATTLE_STATE_TABLE A_off_81292BC

/* register form: tail-calls the battle-state handler selected by the byte offset
 * stored at [r5+2] via the jump table; all guest registers and flags pass through
 * to the callee, so a typed C signature cannot express this trampoline. */
static void dispatch_battle_state(void) {
    u8 byte_offset = LD8(R5 + 2);
    R0 = LD32(BATTLE_STATE_TABLE + byte_offset);  /* function pointer from jump table */
    R1 = byte_offset;                              /* handler's first argument is the raw offset */

    switch (byte_offset >> 2) {                    /* logical index 0–6 */
    case 0: GUEST_CALL(sub_81292D8); break;
    case 1: GUEST_CALL(sub_812933C); break;
    case 2: GUEST_CALL(sub_8129400); break;
    case 3: GUEST_CALL(sub_8129454); break;
    case 4: GUEST_CALL(sub_812952C); break;
    case 5: GUEST_CALL(sub_812954C); break;
    case 6: GUEST_CALL(sub_812956C); break;
    }
}

/* sub_81292A8: dispatch to battle state handler via jump table.
 * inputs:  r5 (state object pointer), r2–r12, lr (all forwarded to callee)
 * outputs: all registers and flags set by the selected handler */
Next sub_81292A8(void) {
    dispatch_battle_state();
    RETURN_TO_CALLER();
}

/* ── sub_81292D8 ── */
/* register form: calls sprite_drawIfFlagSet_812B504 outside this file, which has no typed C interface yet */
static void call_draw_battle_sprite(void) {
    GUEST_CALL(sprite_drawIfFlagSet_812B504);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool call_is_screen_fade_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();  /* Z set → fade active → beq taken → early return */
}

/* register form: calls sub_8046664 outside this file, which has no typed C interface yet */
static void call_init_cutscene_scene(void) {
    GUEST_CALL(sub_8046664);
}

/* register form: calls clearStruct200BC30_803EBAC outside this file, which has no typed C interface yet */
static void call_clear_bg_struct(void) {
    GUEST_CALL(clearStruct200BC30_803EBAC);
}

/* register form: calls sub_80465A0 outside this file, which has no typed C interface yet */
static void call_process_tile_data(u32 ptr) {
    R0 = ptr;
    GUEST_CALL(sub_80465A0);
}

/* register form: calls bg_LoadFullScreenTilesAndSetup_812AFB0 outside this file, which has no typed C interface yet */
static void call_load_fullscreen_bg(void) {
    GUEST_CALL(bg_LoadFullScreenTilesAndSetup_812AFB0);
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void call_init_chatbox(void) {
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void call_set_screen_fade_sub_81292D8(u32 speed, u32 duration) {
    R0 = speed;
    R1 = duration;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls gameState_GetUnk74_8006DEC outside this file, which has no typed C interface yet */
static u32 call_get_progress_flags(u32 script_id) {
    R4 = script_id;
    GUEST_CALL(gameState_GetUnk74_8006DEC);
    return R0;
}

/* register form: calls chatbox_runScript_803FD9C_on_eTextScript201BA20 outside this file, which has no typed C interface yet */
static void call_run_script(u32 script_id, u32 r1_val) {
    R0 = script_id;
    R1 = r1_val;
    R4 = script_id;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);
}

/* Full-screen cutscene setup: draw sprite, bail if fading, load BG tiles,
 * init chatbox, start fade, remap and run the appropriate e-text script.
 * Returns the original r4 unchanged (callers preserve it).
 * Output registers r0–r3 and flags are left by the final callee. */
static u32 play_cutscene_setup(u32 battle_ptr, u32 r4_orig) {
    /* Draw the battle sprite if its visibility flag is set */
    call_draw_battle_sprite();

    /* Bail while a screen-fade transition is in progress */
    if (call_is_screen_fade_active())
        return r4_orig;

    /* Initialise the scene and mark state byte */
    call_init_cutscene_scene();
    ST8(battle_ptr + 2, 4);

    /* Clear BG struct, process tile data, load full-screen tiles */
    call_clear_bg_struct();
    call_process_tile_data(A_byte_8129158);
    call_load_fullscreen_bg();

    /* Initialise chatbox display (reads ambient r1, r4, r5) */
    call_init_chatbox();

    /* Start screen fade-out: speed 8, duration 0x10 */
    call_set_screen_fade_sub_81292D8(8, 0x10);

    /* Clear sub-state flag */
    ST8(battle_ptr + 0xE, 0);

    /* Compute base e-text script ID from selector byte */
    u32 script_id = 0x5A + (u32)LD8(battle_ptr + 0xD);

    /* Query game-progress bitmask */
    u32 game_flags = call_get_progress_flags(script_id);

    /* Select script variant: bit 0 → 0x5A→0x5F, else bit 1 → 0x5B→0x60 */
    u32 r1_val;
    if (game_flags & 1) {
        r1_val = 1;
        if (script_id == 0x5A)
            script_id = 0x5F;
    } else {
        r1_val = 2;
        if ((game_flags & 2) && script_id == 0x5B)
            script_id = 0x60;
    }

    /* Run the selected script (callee sets r0, r1, flags) */
    call_run_script(script_id, r1_val);

    return r4_orig;
}

/* sub_81292D8(battle_ptr = r5, r4 = r4) -> r4 (preserved); r0-r3 + flags from callee */
Next sub_81292D8(void) {
    R4 = play_cutscene_setup(R5, R4);
    RETURN_TO_CALLER();
}

/* ── sub_812933C ── */
#define JOYPAD_A_sub_812933C  1
#define JOYPAD_B_sub_812933C  2
#define MENU_SCRIPT_TABLE  0x081293FC  /* word 0x5B5A → bytes [0]=0x5A, [1]=0x5B */

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_sub_812933C(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();  /* Z set → fade active → skip input */
}

/* register form: calls menu_moveCursorUpDown_811FA22 outside this file, which has no typed C interface yet */
static u32 move_cursor(u32 pressed, u32 cursor) {
    R0 = pressed; R1 = 2; R2 = 1; R3 = cursor;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    return R0;
}

/* register form: calls gameState_GetUnk74_8006DEC outside this file, which has no typed C interface yet */
static u32 get_game_flags(void) {
    GUEST_CALL(gameState_GetUnk74_8006DEC);
    return R0;
}

/* register form: calls chatbox_runScript_803FD9C_on_eTextScript201BA20 outside this file, which has no typed C interface yet */
static void run_chat_script(u32 script_id) {
    R0 = script_id;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);
}

/* register form: calls IsButtonPressed outside this file, which has no typed C interface yet */
static bool is_pressed(u32 mask) {
    R0 = mask;
    GUEST_CALL(IsButtonPressed);
    return !ZERO_FLAG();
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_sub_812933C(u32 id) {
    R0 = id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls eStruct200BC30_setJumpOffset00 outside this file, which has no typed C interface yet */
static void set_jump_offset00(u32 val) {
    R0 = val;
    GUEST_CALL(eStruct200BC30_setJumpOffset00);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void fade_screen(u32 kind, u32 speed) {
    R0 = kind; R1 = speed;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls submenu_ConfirmExitAndFade_811FB64 outside this file, which has no typed C interface yet */
static void confirm_exit_fade(u32 flag) {
    R0 = flag;
    GUEST_CALL(submenu_ConfirmExitAndFade_811FB64);
}

/* register form: calls entity_runAllUpdateHandlers_80465BC outside this file, which has no typed C interface yet */
static void run_entity_updates_sub_812933C(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
}

/* register form: calls sub_80465F8 outside this file, which has no typed C interface yet */
static void run_secondary_update(void) {
    GUEST_CALL(sub_80465F8);
}

/* register form: calls sub_812B29C outside this file, which has no typed C interface yet */
static void set_menu_tick(u32 ticks) {
    R0 = ticks;
    GUEST_CALL(sub_812B29C);
}

/*
 * Battle-menu input handler: processes cursor movement and button presses
 * (A = confirm current option, B = exit submenu).  state_ptr is the menu
 * state block; toolkit_ptr gives access to the joypad.
 */
static void menu_input_update(u32 state_ptr, u32 toolkit_ptr) {
    if (!screen_fade_active_sub_812933C()) {
        u32 pressed = LD16(LD32(toolkit_ptr + oToolkit_JoypadPtr) + oJoypad_Pressed);
        u32 cursor  = LD8(state_ptr + 0xD);
        u32 new_cur = move_cursor(pressed, cursor);

        if (new_cur != cursor) {
            /* Cursor moved to a different option – show its dialogue */
            ST8(state_ptr + 0xD, new_cur);
            u32 script = LD8(MENU_SCRIPT_TABLE + new_cur);
            u32 gs     = get_game_flags();
            if ((gs & 1) && script == 0x5A) script = 0x5F;
            if ((gs & 2) && script == 0x5B) script = 0x60;
            run_chat_script(script);
        } else if (is_pressed(JOYPAD_A_sub_812933C)) {
            /* Confirm the currently-highlighted option */
            play_sound_sub_812933C(SOUND_MENU_SELECT);
            if (LD8(state_ptr + 0xD) == 0) {
                /* "Exit" selected – reset state and fade out */
                set_jump_offset00(0);
                ST8(state_ptr + 1, 8);
                ST8(state_ptr + 2, 0);
                ST8(state_ptr + 3, 0);
                ST8(state_ptr + 0xE, 0);
                fade_screen(0xC, 0x10);
            } else {
                /* A submenu item – advance and fade */
                ST8(state_ptr + 2, 8);
                ST8(state_ptr + 0x21, 0);
                fade_screen(0xC, 0x10);
            }
        } else if (is_pressed(JOYPAD_B_sub_812933C)) {
            /* Cancel / back out of the menu */
            play_sound_sub_812933C(0x83);
            confirm_exit_fade(0x40);
            fade_screen(0xC, 0x10);
        }
    }

    /* Always tick entity and secondary updates, then advance menu timer */
    run_entity_updates_sub_812933C();
    run_secondary_update();
    set_menu_tick(0xD);
}

/* sub_812933C(r5 = menu state ptr, r10 = toolkit ptr) */
Next sub_812933C(void) {
    menu_input_update(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8129400 ── */
/* sub_8129400: per-frame update for the screen-fade driven pop scene.
 * Inputs:  r5 = scene state struct, r10 = some entity ref, lr = return addr
 * Outputs: r5/r8/r9/r10/r11/r12 preserved with caller-supplied values
 * Clobbers: r0/r1/r2/r3/r4/r6/lr and NZCV
 */
Next sub_8129400(void) {
    /* If a screen fade is already running, skip the "pop in" path and just
     * continue driving the scene's normal update tick. */
    GUEST_CALL(IsScreenFadeActive);
    if (ZERO_FLAG() == 0) {
        GUEST_CALL(sub_8046664);
        ST8(R5 + 0xD, 0);
        ST8(R5 + 0x02, 0x0C);
        GUEST_CALL(clearStruct200BC30_803EBAC);

        /* Install the per-scene palette/screen data and bring up the BG. */
        R0 = A_byte_8129168;
        GUEST_CALL(sub_80465A0);
        GUEST_CALL(bg_LoadFullScreenTilesAndSetup_812AFB0);

        /* Show the dialog box frame; we will fill the body with text next. */
        GUEST_CALL(chatbox_8040818);

        /* Fade the screen out to black while the pop script plays. */
        R0 = 8;
        R1 = 0x10;
        GUEST_CALL(SetScreenFade);

        /* Run the pop-script indexed by the previous "next state" byte
         * (a tiny dispatch table at dword_8129450 holds the script ids). */
        u32 prev = LD8(R5 + 0xD);
        u32 tableAddr = A_dword_8129450;
        u32 scriptId = LD8(tableAddr + prev);
        R0 = scriptId;
        R1 = tableAddr;
        GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);
    }

    /* Common per-frame tail: run entity handlers, refresh scene state,
     * then advance the screen-fade sequencer one tick. */
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    GUEST_CALL(sub_80465F8);
    R0 = 0x0D;
    GUEST_CALL(sub_812B29C);

    RETURN_TO_CALLER();
}

/* ── sub_81296A0 ── */
/* sub_81296A0:
 *   inputs:  r4 r5 r10 lr
 *   outputs: r4 r5 r6
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r6 r7
 *
 * Handles chatbox flag transitions: if flag 0x8 is set, marks the
 * chatbox state (byte at r5+3) and calls sub_812B3FC. Otherwise, if
 * both flag 0x20 and flag 0x110 are set, marks another state byte
 * (r5+2) and triggers a screen fade. */
Next sub_81296A0(void) {
    R0 = 8;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (ZERO_FLAG()) {
        R0 = 0x20;
        GUEST_CALL(chatbox_mask_eFlags2009F38);
        if (!ZERO_FLAG()) {
            R0 = 0x110;
            GUEST_CALL(chatbox_mask_eFlags2009F38);
            if (!ZERO_FLAG()) {
                ST8(R5 + 2, 0x20);
                R0 = 0xC;
                R1 = 0x10;
                GUEST_CALL(SetScreenFade);
            }
        }
    } else {
        ST8(R5 + 3, 8);
        GUEST_CALL(sub_812B3FC);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8129738 ── */
/* sub_8129738(r5 = battle object ptr):
 * If sub_812B318(3) succeeds, mark the object's state: store 3 at +0x1D,
 * call sub_81325E4 on its +0xE byte and store the result at +2,
 * then clear +3 to 0. Skips all of that if sub_812B318 returns nonzero. */
Next sub_8129738(void) {
    /* Try to acquire something via sub_812B318(arg=3); nonzero -> bail out. */
    R0 = 3;
    GUEST_CALL(sub_812B318);
    if (R0 != 0) {
        RETURN_TO_CALLER();
    }

    /* Mark the object as state 3, recompute the +2 field from +0xE, and clear +3. */
    ST8(R5 + 0x1D, (u8)3);
    R0 = LD8(R5 + 0xE);
    GUEST_CALL(sub_81325E4);
    ST8(R5 + 2, (u8)R0);
    ST8(R5 + 3, 0);

    RETURN_TO_CALLER();
}

/* ── sub_8129758 ── */
/* sub_8129758(r5 = entity ptr)
 * Reads entity field at offset 0x1D and asks sub_812B318 what to do with it.
 * If sub_812B318 signals a follow-up (r1 != 0), call sub_812B218.
 * If the chosen action is 3, set the entity's state byte to 0x14 and call
 * sub_811A610(0, 0).
 * Outputs: r4 = sub_812B318's r2 (the action), r5/r6 preserved. */
Next sub_8129758(void) {
    /* Ask sub_812B318 what to do with entity[0x1D]. */
    R0 = (u32)LD8(R5 + 0x1D);
    GUEST_CALL(sub_812B318);
    /* r4 = action returned in r2 */
    R4 = R2;

    /* If sub_812B318 set r1, follow up with sub_812B218. */
    if (R1 != 0) {
        GUEST_CALL(sub_812B218);
    }

    /* Action 3: enter state 0x14 and call sub_811A610(0, 0). */
    if (R4 == 3) {
        R0 = 0x14;
        ST8(R5 + 3, (u8)R0);
        R0 = 0;
        R1 = 0;
        GUEST_CALL(sub_811A610);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8129808 ── */
/* sub_8129808: takes r5 = some battle object pointer (has a byte at +0x1d and +3),
 * reads its state index byte, calls sub_812B318, and based on results either
 * does nothing or flips the object's state to 4 and calls sub_811A610(1,0).
 * Preserves r4/r5/r6 for the caller; clobbers r0-r3 and lr internally. */
Next sub_8129808(void) {
    u8 state_index = LD8(R5 + 0x1D);

    R0 = state_index;
    GUEST_CALL(sub_812B318);
    u32 r4_save = R2;          /* sub_812B318's result in r2 -> cached in r4 */

    if (R1 != 0) {
        GUEST_CALL(sub_812B218);
    }

    if (r4_save != 3) {
        RETURN_TO_CALLER();
    }

    /* r4 == 3: advance the object to the next animation/state step */
    u32 new_state = 4;
    ST8(R5 + 3, new_state);
    R0 = 1;
    R1 = 0;
    GUEST_CALL(sub_811A610);

    RETURN_TO_CALLER();
}

/* ── sub_81298D4 ── */
/* sub_81298D4(r5 = object):
 *   Initialize a battle object to the "battle-canceled / show-fingers / etc."
 *   state if preconditions are met, then return.
 *
 *   Inputs:  r5 (object pointer, caller-saved)
 *   Outputs: r4, r5, r6 (preserved across the call)
 *   Clobbers: r0, r1, r2, r3, lr, nz, c, v */
Next sub_81298D4(void) {
    /* First, ask the toolkit/engine whether this update path is even
     * permitted right now (mode 3 = whatever the bsy check is). */
    R0 = 3;
    GUEST_CALL(sub_812B318);
    if (R0 != 0)
        RETURN_TO_CALLER();             /* not allowed -> bail, leave object alone */

    /* Otherwise, stamp the object into the "post-event" pose:
     *   +0x1D = 3   (state byte)
     *   +0x20 = 2   (sub-state / pose index) */
    ST8(R5 + 0x1D, 3);
    ST8(R5 + 0x20, 2);

    /* Look up the correct sprite index for whatever frame/state byte
     * is at +0x12, then store it as the current animation frame. */
    R0 = LD8(R5 + 0x12);
    GUEST_CALL(sub_81325F4);
    ST8(R5 + 2, (u8)R0);

    /* Frame counter at +0x03 is reset to 0. */
    ST8(R5 + 3, 0);

    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_81298F8 ── */
/* sub_81298F8(r5 = battle object pointer, lr = return address)
 * Initialize a battle object's visual/animation state to its "selected"/attack pose:
 * sub_812B318 must allow it (r0 != 0 -> skip); on success set r5[0x1D]=3, r5[2]=4,
 * r5[3]=0x10, r5[0x12]=r5[0x13]=0. */
Next sub_81298F8(void) {
    R0 = 3;
    GUEST_CALL(sub_812B318);
    if (R0 != 0) {
        /* sub_812B318 refused; leave the object untouched. */
    } else {
        ST8(R5 + 0x1D, 3);
        ST8(R5 + 2,    4);
        ST8(R5 + 3,    0x10);
        ST8(R5 + 0x12, 0);
        ST8(R5 + 0x13, 0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8129948 ── */
/* sub_8129948(r5 = some struct):
 * Reads the byte at r5+0x1D and passes it to sub_812B318.
 * Saves sub_812B318's r2 in r4. If its r1 is nonzero, also calls sub_812B218.
 * If r4 == 3, sets state byte at r5+3 to 4 and calls sub_811A610(3, 0). */
Next sub_8129948(void) {
    u8 slot = LD8(R5 + 0x1D);

    R0 = slot;
    GUEST_CALL(sub_812B318);

    R4 = R2;
    SET_FLAGS_NZ(R1);
    if (R1) {
        GUEST_CALL(sub_812B218);
    }

    SET_FLAGS_CMP(R4, 3);
    if (R4 == 3) {
        ST8(R5 + 3, 4);
        R0 = 3;
        R1 = 0;
        GUEST_CALL(sub_811A610);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8129AC8 ── */
/* sub_8129AC8(r5 = entity, lr = return):
 *   Picks a battle-side action for the entity at r5: asks sub_812B318 what
 *   kind to do (r2 = kind, r1 = valid). If valid and kind == 2 we mark the
 *   entity's action byte (r5+3 = 4) and trigger the move event (sub_811A610);
 *   otherwise we delegate to sub_812B318's sibling sub_812B218.
 *   outputs: r4 r5 r6  preserved: r4 r5 r6 r7 */
Next sub_8129AC8(void) {
    /* Ask the battle-side helper which action this entity should take. */
    R0 = LD8(R5 + 0x1D);
    GUEST_CALL(sub_812B318);
    /* r1 = valid, r2 = kind. Match the original: r4 picks up r2, then we
     * test r1 with TST/SUBS so the Z flag reflects it. */
    u32 kind = R2;

    if (R1 == 0)
        RETURN_TO_CALLER();

    if (kind == 2) {
        ST8(R5 + 3, 4);
        R0 = 2;
        R1 = 0;
        GUEST_CALL(sub_811A610);
    } else {
        GUEST_CALL(sub_812B218);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8129B90 ── */
/* sub_8129B90(r5 = control struct pointer):
 *   Ask sub_812B318 for state 2; if granted, initialise the chip-selection
 *   counter fields on the struct. Preserves r4/r5/r6.
 *   inputs:  r5, lr
 *   outputs: r4, r5, r6 (preserved)
 *   clobbers: r0-r3, lr, nz/c/v
 *
 *   Struct fields touched (offsets from r5):
 *     +0x1D  selection slot            = 3
 *     +0x02  ??                        = 4
 *     +0x03  ??                        = 0x10
 *     +0x12  counter low               = 0
 *     +0x13  counter high              = 0
 */
Next sub_8129B90(void) {
    /* sub_812B318(r0 = 2): request chip-use state 2. */
    R0 = 2;
    GUEST_CALL(sub_812B318);

    /* tst r0, r0 — bne locret: bail out if it refused. */
    SET_FLAGS_NZ(R0);
    if (ZERO_FLAG()) {
        ST8(R5 + 0x1D, 3);
        ST8(R5 + 0x02, 4);
        ST8(R5 + 0x03, 0x10);
        ST8(R5 + 0x12, 0);
        ST8(R5 + 0x13, 0);

        /* Final flag-setter is `mov r0, #0` before the last two strbs. */
        SET_FLAGS_NZ(0);
    }

    RETURN_TO_CALLER();
}

/* ── sub_812AEC8 ── */
/* register form: calls entity_runAllUpdateHandlers_80465BC outside this file, which has no typed C interface yet */
static void run_entity_updates_sub_812AEC8(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
}

/* register form: calls sub_80465F8 outside this file, which has no typed C interface yet */
static void tick_secondary_handlers(void) {
    GUEST_CALL(sub_80465F8);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_sub_812AEC8(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();  /* Z set ⇒ a fade transition is in progress */
}

/* register form: calls pet_menuExitToMap_811F708 outside this file, which has no typed C interface yet */
static void exit_to_overworld_map(void) {
    GUEST_CALL(pet_menuExitToMap_811F708);
}

/* Advance all entity update handlers and secondary ticks, then return to the
 * overworld map unless a screen fade is currently in progress. */
static void process_tick_and_fade_exit(void) {
    run_entity_updates_sub_812AEC8();
    tick_secondary_handlers();
    if (!screen_fade_active_sub_812AEC8())
        exit_to_overworld_map();
}

/* sub_812AEC8() -> void; output registers and flags pass through from callees. */
Next sub_812AEC8(void) {
    process_tick_and_fade_exit();
    RETURN_TO_CALLER();
}

/* ── scene_loadAndStartCutscene_812AF18 ── */
/* scene_loadAndStartCutscene_812AF18: set up a scene — run its init routine, decompress and copy its
 * asset reference table, then load its graphics/text/sprite resources and start
 * its chatbox script. All registers the callers rely on are preserved. */
Next scene_loadAndStartCutscene_812AF18(void) {
    /* Initialize the scene state. */
    GUEST_CALL(decomp_buildInitRefTable_812AF3C);

    /* Decompress the asset reference table and copy it into place. */
    R0 = A_off_812B08C;
    GUEST_CALL(decompAndCopyData);

    /* Load the decompressed tile/palette asset. */
    GUEST_CALL(loadDecompressedAsset_812AF78);

    /* Load and run the embedded text script archive. */
    GUEST_CALL(textScript_loadAndRunEmbeddedArchive_812AF84);

    /* Initialize the sprite decomp tables. */
    GUEST_CALL(sprite_decompInitRefs_812AF98);

    /* Scene-specific extra setup. */
    GUEST_CALL(sub_812AFA4);

    /* Start the chatbox overlay for this scene. */
    GUEST_CALL(chatbox_8040818);

    RETURN_TO_CALLER();
}

/* ── decomp_buildInitRefTable_812AF3C ── */
/* decomp_buildInitRefTable_812AF3C: build the init-reference table at unk_201D020 from the template
 * at off_812B024, then decompress-and-copy the data it describes.
 *
 * The source table is 0x60 bytes, i.e. six groups of four words. In each group
 * the first two words form one reference that sub_811FB84 resolves to a single
 * pointer-sized entry; the remaining two words are copied through unchanged.
 * The rebuilt table (24 words) is terminated with a 0 word and handed to
 * decompAndCopyData, which expands the referenced data into place. */
Next decomp_buildInitRefTable_812AF3C(void) {
    u32 dest = A_unk_201D020;
    u32 src  = A_off_812B024;
    u32 out  = 0;   /* write offset into the rebuilt table */

    for (u32 in = 0; in < 0x60; in += 16) {
        /* resolve the two-word reference that opens each group */
        R0 = LD32(src + in);
        R1 = LD32(src + in + 4);
        GUEST_CALL(sub_811FB84);
        ST32(dest + out, R0);
        out += 4;

        /* copy the two plain words that follow it */
        ST32(dest + out, LD32(src + in + 8));
        out += 4;
        ST32(dest + out, LD32(src + in + 12));
        out += 4;
    }

    /* NUL-terminate the reference table */
    ST32(dest + out, 0);

    R0 = dest;
    GUEST_CALL(decompAndCopyData);
    RETURN_TO_CALLER();
}

/* ── loadDecompressedAsset_812AF78 ── */
/* loadDecompressedAsset_812AF78: decompress the asset referenced by off_812B0E8 into its
 * destination (decompAndCopyData reads the target from the data itself).
 * All arguments beyond the pointer in r0 are ignored, so r2-r7 are
 * untouched and simply preserved. */
Next loadDecompressedAsset_812AF78(void) {
    R0 = A_off_812B0E8;
    GUEST_CALL(decompAndCopyData);
    RETURN_TO_CALLER();
}

/* ── textScript_loadAndRunEmbeddedArchive_812AF84 ── */
/* textScript_loadAndRunEmbeddedArchive_812AF84: decompress the embedded text archive (byte_812B168) into the
 * decomp buffer, then hand the decompressed script (eTextScript201BA20) and
 * its descriptor (off_812AFE4) to the text-script engine (sub_8123300).
 * Result registers (r1-r7) are whatever sub_8123300 leaves behind. */
Next textScript_loadAndRunEmbeddedArchive_812AF84(void) {
    /* Decompress the packed data blob into work RAM. */
    R0 = A_byte_812B168;
    GUEST_CALL(decompAndCopyData);

    /* Point the text engine at the decompressed script and run it. */
    R0 = A_off_812AFE4;
    R1 = A_eTextScript201BA20;
    R2 = A_eDecompBuffer2013A00;
    GUEST_CALL(sub_8123300);

    RETURN_TO_CALLER();
}

/* ── sprite_decompInitRefs_812AF98 ── */
/* sprite_decompInitRefs_812AF98: decompress and copy the initRefs data at byte_812B1A0. */
Next sprite_decompInitRefs_812AF98(void) {
    R0 = A_byte_812B1A0;
    GUEST_CALL(decompAndCopyData);
    RETURN_TO_CALLER();
}

/* ── sub_812AFA4 ── */
/* sub_812AFA4: decompress the data referenced by the literal byte_812B1C0
 * (its initRefs blob) and copy it into place via decompAndCopyData.
 * All arguments were already set up by the caller; R0 must point at the
 * source word stream when the callee is entered. */
Next sub_812AFA4(void) {
    R0 = A_byte_812B1C0;
    GUEST_CALL(decompAndCopyData);
    RETURN_TO_CALLER();
}

/* ── bg_LoadFullScreenTilesAndSetup_812AFB0 ── */
/* bg_LoadFullScreenTilesAndSetup_812AFB0:
 * Fill a 30x20-tile screen area with tile IDs taken from the table at
 * unk_201FDA0, using tile block 1, then run the follow-up setup in
 * sub_812B4AC. The results sub_812B4AC leaves in r1-r3 (and the flags it
 * leaves behind) are passed on to the caller; r4-r7 are preserved. */
Next bg_LoadFullScreenTilesAndSetup_812AFB0(void) {
    /* CopyBackgroundTiles may clobber r4/r5 (it receives 0x1E and 0x14 there),
     * and sub_812B4AC sees whatever it left behind — but this function's
     * callers expect the original r4-r7 back, so keep them aside. */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    /* CopyBackgroundTiles(j = 0, i = 0, tileBlock32x32 = 1,
     *                     tileIds = unk_201FDA0, j_size = 30, i_size = 20) */
    R0 = 0;
    R1 = 0;
    R2 = 1;
    R3 = A_unk_201FDA0;
    R4 = 0x1E;
    R5 = 0x14;
    GUEST_CALL(CopyBackgroundTiles);

    GUEST_CALL(sub_812B4AC);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_812B1DC ── */
/* sub_812B1DC: load a 4-byte entry pair table into a scratch buffer.
 *
 * Walks a small packed array starting at *r0 of records of the form
 * { u8 a, u8 b, u16 c }; the array ends with a sentinel byte 0xFF.
 * Each record is decoded via sub_8046670(a, b, c) and the result is
 * stored into the 4-slot scratch buffer at unk_20251A0, indexed by slot
 * number (0..3). The scratch buffer is zeroed up to 0x10 bytes first.
 *
 * inputs:  r0 = pointer to the entry table
 * outputs: none meaningful (r4/r5/r6 left as the index walker state)
 */
Next sub_812B1DC(void) {
    u32 entries = R0;
    u32 scratch = (u32)A_unk_20251A0;

    /* ZeroFillByWord(unk_20251A0, 0x10) */
    R0 = scratch;
    R1 = 0x10;
    GUEST_CALL(ZeroFillByWord);

    /* The first record is decoded into the top half of the entry. */
    u32 r2 = (u32)LD16(entries + 2);
    u32 r1 = (u32)LD8(entries + 1);
    u32 r0 = (u32)LD8(entries);
    R0 = r0; R1 = r1; R2 = r2;
    GUEST_CALL(sub_8046670);

    /* Walk the rest of the table; each record's result lands in the
     * scratch buffer at slot i. The loop terminates when the byte at
     * the next record's offset 0 reads 0xFF. */
    u32 walker = entries + 4;
    u32 slot = 0;
    for (;;) {
        r2 = (u32)LD16(walker + 2);
        r2 |= slot << 16;
        r1 = (u32)LD8(walker + 1);
        r0 = (u32)LD8(walker);
        R0 = r0; R1 = r1; R2 = r2;
        GUEST_CALL(sub_8046670);
        ST32(scratch + (slot << 2), R0);

        slot += 1;
        walker += 4;

        R0 = (u32)LD8(walker);
        if (R0 != 0xFF) {
            continue;
        }
        break;
    }
    RETURN_TO_CALLER();
}

/* ── sub_812B414 ── */
/* sub_812B414(r5 = battle object, r10 = ?, lr = return address):
 * mark the battle object as the active target, clear its counters, flag it
 * for selection, then trigger chatbox script 0x23.
 * outputs: r4 r5 r6; clobbers r0 r1 r2 r3 lr nz cv. */
Next sub_812B414(void) {
    /* Battle state byte = 0x0C (some "selected"/"active" marker). */
    ST8(R5 + 1, 0x0C);

    /* Clear the per-object counter bytes. */
    ST8(R5 + 2, 0);
    ST8(R5 + 3, 0);

    /* OR the 0x01 bit into the existing flag byte at offset 0x1B. */
    u8 flags = LD8(R5 + 0x1B);
    ST8(R5 + 0x1B, flags | 0x01);

    /* Run chatbox script index 0x23 (the "selection" dialogue line). */
    R0 = 0x23;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);

    RETURN_TO_CALLER();
}

/* ── sprite_drawIfFlagSet_812B504 ── */
/* sprite_drawIfFlagSet_812B504(r5 = pointer to an object):
 * Test the flag byte at r5+0x1B for bit 4 (0x10). If set, invoke
 * sub_811FAF4 with a fixed argument block (0xC80008, 0x20004, 0,
 * &dword_8129184, 0x80004000); otherwise return immediately.
 * r0 and r1 are always written (the load and the #0x10 happen before
 * the branch), r4 is preserved around the call, and on the early-exit
 * path the flags come from the TST. */
Next sprite_drawIfFlagSet_812B504(void) {
    u32 flagByte = LD8(R5 + 0x1B);

    R0 = flagByte;   /* ldrb r0, [r5,#0x1b] */
    R1 = 0x10;       /* mov r1, #0x10 */

    if ((flagByte & 0x10) == 0) {
        /* Bit clear: TST result was zero, return with those flags. */
        SET_FLAGS_NZ(flagByte & 0x10);
    } else {
        u32 savedR4 = R4;

        R0 = 0xC80008;
        R1 = 0x20004;
        R2 = 0;
        R3 = A_dword_8129184;
        R4 = 0x80004000;

        GUEST_CALL(sub_811FAF4);

        R4 = savedR4;   /* the original pushes/pops r4 around the call */
    }
    RETURN_TO_CALLER();
}

/* ── sub_812B530 ── */
/* sub_812B530(r5 = entity):
 *  - Ask the entity for its jump-table offset 0 entry (function pointer / index).
 *  - Use that as an index into an array of LZ77-compressed graphics blob pointers.
 *  - Decompress the chosen blob into the LZ77 decompression buffer.
 *  - DMA 0x100 bytes of the decompressed graphics (skipping the 4-byte LZ77 header)
 *    into VRAM at 0x06014700.
 *  - Set the entity's flag byte at offset 0x1B (OR-in 0x10).
 */
Next sub_812B530(void) {
    u32 entity = R5;

    GUEST_CALL(eStruct200BC30_getJumpOffset00);

    const void *src = (const void *)LD32(A_off_812B55C + R0);
    R0 = (u32)src;
    R1 = (u32)A_eDecompBuffer2013A00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    R0 = (u32)A_eDecompBuffer2013A00 + 4;
    R1 = 0x06014700;
    R2 = 0x100;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    ST8(entity + 0x1B, LD8(entity + 0x1B) | 0x10);
    RETURN_TO_CALLER();
}
