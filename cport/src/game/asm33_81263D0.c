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

/* ── battle_loadChipIconSprites_8126430 ── */
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

/* battle_loadChipIconSprites_8126430:
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

Next battle_loadChipIconSprites_8126430(void) {
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

/* ── packSelectionSlots_8126494 ── */
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

/* packSelectionSlots_8126494(r0 = chipIndex, r1 = baseAddr, r2 = slotAddr) */
Next packSelectionSlots_8126494(void) {
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
/* sub_81264F4: fixed-argument wrapper around battle_queueAnimatedOamEntry.
 *
 * It loads a constant argument set (two screen coordinates and two sizes in
 * r0-r3, a data table pointer in r4, a hardware-register mirror address in
 * r6, and a second table pointer in r7) and tail-calls battle_queueAnimatedOamEntry, which
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

    GUEST_CALL(battle_queueAnimatedOamEntry);

    /* pop {r4-r7} - restore the caller's register values */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── wrapper_sub8126570_fixedArgs_812650C ── */
/* wrapper_sub8126570_fixedArgs_812650C: parameter-filling wrapper around battle_queueAnimatedOamEntry.
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

    GUEST_CALL(battle_queueAnimatedOamEntry);

    /* pop {r4-r7,pc} -- r4-r7 end exactly as they came in; r2/r3 are
     * whatever battle_queueAnimatedOamEntry left behind. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8126524 ── */
/* sub_8126524: thin wrapper around battle_queueAnimatedOamEntry.
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

    GUEST_CALL(battle_queueAnimatedOamEntry);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_8126554 ── */
/* sub_8126554: thin argument-setup wrapper around battle_queueAnimatedOamEntry.
 * It loads a fixed set of arguments (counts/positions 90, 32, 60, 12 plus
 * pointers to the data tables byte_8124A84 and dword_8124A94, and the
 * 0x40000000 flag word), invokes battle_queueAnimatedOamEntry, and hands its r1-r3 results
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

    GUEST_CALL(battle_queueAnimatedOamEntry);

    /* pop {r4-r7}: the callee's r4-r7 do not escape this wrapper */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── drawBattleRowSprites_81265DC ── */
/* register form: calls sprite_positionSubmenuEntries_8116FDC outside this file, which has no typed C interface yet */
static void draw_row_cell(u32 x, u32 anim, u32 tile_offset) {
    R0 = x;
    R1 = anim;
    R2 = tile_offset;
    GUEST_CALL(sprite_positionSubmenuEntries_8116FDC);
}

/* Battle-row sprite setup: iterates r7 = 1, then 0. For each, the screen x is
 * base[(r7<<2)] + ((battle_x - 0x100) << 3), clamped to >= -40 via up to two
 * 0x120 wrap-arounds. Then blits the row sprite via sprite_positionSubmenuEntries_8116FDC at tile
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

/* drawBattleRowSprites_81265DC(r4, r5, r6, r7, lr) -> r3 r4 r5 r6 r7 */
Next drawBattleRowSprites_81265DC(void) {
    draw_battle_rows(R5);
    RETURN_TO_CALLER();
}

/* ── battle_updateChipSprite_8126630 ── */
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

/* register form: calls battle_spawnEStructRecordsFromTable_80465A0 outside this file, which has no typed C interface yet */
static void report_unexpected_ptr(u32 pointer) {
    R0 = pointer;
    GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);
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

/* register form: calls battleObject_initChipSpriteSlotBuffer_8126818 outside this file, which has no typed C interface yet */
static void trigger_slot_effect(u32 arg) {
    R0 = arg;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);
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

/* battle_updateChipSprite_8126630(obj_ptr = r5) — updates sprite tables, fields, and position. */
Next battle_updateChipSprite_8126630(void) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    update_battle_sprite(R5);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    RETURN_TO_CALLER();
}

/* ── sub_812674C ── */
/* Helper for sub_812674C: blit `tiles` tiles of the graphic whose tile data
 * lives at byte_8124A8C into the buffer at dword_8124A94 via battle_queueAnimatedOamEntry,
 * with the first tile at x-coordinate `x`, y = 0x14.  `attr` carries the
 * extra attribute bits for the blit (0x10000000 vs. none).  Whatever
 * battle_queueAnimatedOamEntry leaves in R3 stays there, since the caller returns it. */
static void blit_graphic(u32 x, u32 tiles, u32 attr)
{
    R0 = x;
    R1 = 0x14;
    R2 = 0;
    R3 = tiles;
    R4 = A_byte_8124A8C;
    R6 = attr;
    R7 = A_dword_8124A94;
    GUEST_CALL(battle_queueAnimatedOamEntry);
}

/* sub_812674C(r5 = reveal state):
 * Progressively reveals a two-part tile graphic.  Step counter is at
 * R5+0xD, the limit at R5+0xE.  Once the step is non-zero the left part
 * (x = 0x58, 15 tiles, attribute 0x10000000) is drawn; the right part
 * (x = 0xE2, 16 tiles, no attribute bits) is drawn either right away
 * (step 0) or while the table byte_81267A0 still maps the step counter to
 * a segment index below the limit.  battle_queueAnimatedOamEntry's r3 result is passed
 * through to the caller; r4-r7 are preserved. */
Next sub_812674C(void) {
    /* The original pushes r4-r7 and pops them before returning, so the
     * callee's scratch use of them must not leak out. */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    if (LD8(R5 + 0xD) != 0) {
        /* Reveal in progress: draw the left part first. */
        blit_graphic(0x58, 0xF, 0x10000000);

        /* Keep drawing the right part only while segments remain: the
         * table maps the step counter to the number of segments already
         * done, and we stop once it reaches the limit at R5+0xE.  Both
         * state bytes are re-read here because battle_queueAnimatedOamEntry may have
         * advanced the animation. */
        u32 limit = LD8(R5 + 0xE);
        u32 segment = LD8(A_byte_81267A0 + LD8(R5 + 0xD));
        if ((s32)segment < (s32)limit)
            blit_graphic(0xE2, 0x10, 0);
    } else {
        /* Step 0: no left part yet, just draw the right part. */
        blit_graphic(0xE2, 0x10, 0);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
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
        GUEST_CALL(battle_petNaviDisplayInit_8126B6C);   /* state 0 handler */
        break;
    case 4:
        GUEST_CALL(battle_petNaviDisplayUpdate_8126CC0);   /* state 4 handler */
        break;
    case 8:
        GUEST_CALL(pet_menuScreenTick_8126FF0);   /* state 8 handler */
        break;
    default:
        /* Unreachable in the game: the handlers keep the state byte within
         * the table. The original would branch through a garbage word. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_petNaviDisplayInit_8126B6C ── */
/* register form: calls zeroFillVRAM outside this file, which has no typed C interface yet */
static void call_zero_fill_vram(void) { GUEST_CALL(zeroFillVRAM); }

/* register form: calls ZeroFill_byte_3001960 outside this file, which has no typed C interface yet */
static void call_zero_fill_3001960(void) { GUEST_CALL(ZeroFill_byte_3001960); }

/* register form: calls ZeroFillGFX30025c0 outside this file, which has no typed C interface yet */
static void call_zero_fill_gfx(void) { GUEST_CALL(ZeroFillGFX30025c0); }

/* register form: calls toolkit_applyOverlayPalette_80015FC outside this file, which has no typed C interface yet */
static void call_sub_80015FC(u32 arg) { R0 = arg; GUEST_CALL(toolkit_applyOverlayPalette_80015FC); }

/* register form: battle_resetEStructRecords_8046664 takes toolkit ptr in R7; callee may clobber R7 (preserved by outer fn) */
static void call_sub_8046664(u32 toolkit) {
    u32 saved = R7;
    R7 = toolkit;
    GUEST_CALL(battle_resetEStructRecords_8046664);
    R7 = saved;
}

/* register form: calls initTitleScreens_81270D0 outside this file, which has no typed C interface yet */
static void call_sub_81270D0(void) { GUEST_CALL(initTitleScreens_81270D0); }

/* register form: calls sub_8127800 outside this file, which has no typed C interface yet */
static void call_sub_8127800(void) { GUEST_CALL(sub_8127800); }

/* register form: calls battleObject_advanceAnimTableBy0xFF_8120CC8 outside this file, which has no typed C interface yet.
 * Returns the packed tile-attribute byte: (R0 | (R1 << 4)) & 0xFF. */
static u32 call_get_tile_attr(u32 idx, u32 table_addr) {
    R0 = idx; R1 = table_addr;
    GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
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

/* register form: petNaviTryGiveElementFolder_8120D10 takes state byte in R4; callee may clobber R4 (preserved by outer fn).
 * Returns R1 (script index) and sets *out_zero from Z flag. */
static u32 call_sub_8120D10(u32 arg, u32 state_byte, bool *out_zero) {
    u32 saved = R4;
    R0 = arg; R4 = state_byte;
    GUEST_CALL(petNaviTryGiveElementFolder_8120D10);
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

/* register form: calls petNaviIntro_812741C outside this file, which has no typed C interface yet */
static void call_sub_812741C(void) { GUEST_CALL(petNaviIntro_812741C); }

/* register form: calls background_drawSceneSection_81275A8 outside this file */
static void call_draw_scene_section(void) { GUEST_CALL(background_drawSceneSection_81275A8); }

/* register form: calls battle_renderChipSlots_81275C8 outside this file, which has no typed C interface yet */
static void call_sub_81275C8(void) { GUEST_CALL(battle_renderChipSlots_81275C8); }

/* register form: calls bg_copyTileBlockFromStructOffset_8127618 outside this file */
static void call_bg_copy_tile_block(void) { GUEST_CALL(bg_copyTileBlockFromStructOffset_8127618); }

/* register form: calls battle_buildPetNaviList_8127638 outside this file, which has no typed C interface yet */
static void call_sub_8127638(void) { GUEST_CALL(battle_buildPetNaviList_8127638); }

/* register form: calls sub_8127718 outside this file, which has no typed C interface yet */
static void call_sub_8127718(void) { GUEST_CALL(sub_8127718); }

/* register form: calls bg_Draw8x8TileBlock3FromTable_81277E0 outside this file */
static void call_draw_8x8_block3(void) { GUEST_CALL(bg_Draw8x8TileBlock3FromTable_81277E0); }

/* register form: calls chatbox_event163Progress_81273B4 outside this file, which has no typed C interface yet */
static void call_sub_81273B4(void) { GUEST_CALL(chatbox_event163Progress_81273B4); }

/* register form: calls paintMapEventTileBlock_81273F0 outside this file, which has no typed C interface yet */
static void call_sub_81273F0(void) { GUEST_CALL(paintMapEventTileBlock_81273F0); }

/* register form: calls sub_8127060 outside this file, which has no typed C interface yet */
static void call_sub_8127060(void) { GUEST_CALL(sub_8127060); }

/* register form: calls sub_8119C94 outside this file; returns both R0 and R1 (tile-set base ptrs) */
static void call_get_tileset_ptrs(u32 *out0, u32 *out1) {
    GUEST_CALL(sub_8119C94);
    *out0 = R0; *out1 = R1;
}

/* register form: calls battle_spawnEStructRecordsFromTable_80465A0 outside this file, which has no typed C interface yet */
static void call_sub_80465A0(u32 arg) { R0 = arg; GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0); }

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

    /* Setup callbacks (battle_resetEStructRecords_8046664 reads toolkit via R7) */
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

/* battle_petNaviDisplayInit_8126B6C(r5 = scene struct, r10 = toolkit) -> all regs/flags from callees */
Next battle_petNaviDisplayInit_8126B6C(void) {
    pet_navi_scene_init(R5, R10);
    RETURN_TO_CALLER();
}

/* ── chatbox_fadeGuardThenRunState_8126DF8 ── */
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

/* register form: calls battle_dispatchNextCmd_81272C4 outside this file, which has no typed C interface yet */
static void tail_call_sub_81272C4(void) {
    GUEST_CALL(battle_dispatchNextCmd_81272C4);
}

/* Clear chatbox state when a screen fade is active, then jump to the battle-state handler. */
static void fade_chatbox_guard(u32 chatbox_struct_addr) {
    if (screen_fade_active_sub_8126DF8()) {
        clear_chatbox_eflags_sub_8126DF8(0x40);
        ST8(chatbox_struct_addr + 2, 4); /* set chatbox state byte to "hidden" */
    }
    tail_call_sub_81272C4(); /* sets r0-r7, lr, nz/c/v; reads r2, r4, r5, r10 */
}

/* chatbox_fadeGuardThenRunState_8126DF8(r5 = chatbox struct ptr, r2/r4/r10 = context passed through to battle_dispatchNextCmd_81272C4) */
Next chatbox_fadeGuardThenRunState_8126DF8(void) {
    fade_chatbox_guard(R5);
    RETURN_TO_CALLER();
}

/* ── pet_menuScreenTick_8126FF0 ── */
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

/* register form: calls battle_updateEStructSprites_80465F8 outside this file, which has no typed C interface yet */
static void menu_draw(void) {
    GUEST_CALL(battle_updateEStructSprites_80465F8);
}

/* register form: calls battle_dispatchNextCmd_81272C4 outside this file, which has no typed C interface yet */
static void menu_anim_tick(void) {
    GUEST_CALL(battle_dispatchNextCmd_81272C4);
}

/* register form: calls pet_drawBattleChipCursorOam_8127374 outside this file, which has no typed C interface yet */
static void menu_state_tick(void) {
    GUEST_CALL(pet_drawBattleChipCursorOam_8127374);
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

/* register form: calls link_resetHardwareAndClearLinkBuffers_81440D8 outside this file, which has no typed C interface yet */
static void sub_81440D8_call(void) {
    GUEST_CALL(link_resetHardwareAndClearLinkBuffers_81440D8);
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet */
static void sub_8046664_call(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
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

/* pet_menuScreenTick_8126FF0(r5 = battle_state ptr, r10 = toolkit ptr) -> (flags, r0-r4 from callees) */
Next pet_menuScreenTick_8126FF0(void) {
    pet_menu_screen_tick(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8127060 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r6, r7; bool c, v; } call_CopyBackgroundTiles_result_sub_8127060;
/* register form: generated wrapper: CopyBackgroundTiles has no typed C interface yet */
static inline call_CopyBackgroundTiles_result_sub_8127060 call_CopyBackgroundTiles_sub_8127060(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles);
    return (call_CopyBackgroundTiles_result_sub_8127060){ R1, R2, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Bundle of registers the original frame preserves via push/pop. We pass
 * them through the typed function so the call wrappers (which write r4/r5
 * as call arguments and let CopyBackgroundTiles clobber them) cannot leak
 * stale values out past the function. */
typedef struct { u32 r4; u32 r5; u32 r6; u32 r7; u32 r10; } PreservedRegs;

/* CopyBackgroundTiles on the five slots that make up one row of the folder
 * list. The first copy draws the 30x20 folder-name banner at absolute
 * (0,0). The next four are icon strips whose horizontal position is
 * offset by the column byte (offset 0xD) of the row-descriptor record
 * that was passed in r5. */
static PreservedRegs copy_list_row_tiles(u32 r4_in, u32 row_ptr, u32 r6_in, u32 r7_in, u32 r10_in) {
    (void)r4_in; (void)r6_in; (void)r7_in; (void)r10_in;

    /* Folder-name banner: 30x20 tiles from tile-block 1 at (0,0). */
    call_CopyBackgroundTiles_sub_8127060(0, 0, 1, A_eTextScript201BA20, 0x1E, 0x14);

    /* Icon strip #1: column + 0x0F, 2x2 tiles. */
    s8 col = (s8)LDS8(row_ptr + 0xD);
    call_CopyBackgroundTiles_sub_8127060((u32)(col + 0x0F), 2, 2, A_unk_201CF20, 0x0E, 0x0A);

    /* Icon strip #2: column + 0x22, 2x2 tiles. */
    col = (s8)LDS8(row_ptr + 0xD);
    call_CopyBackgroundTiles_sub_8127060((u32)(col + 0x22), 2, 2, A_unk_201D120, 0x0E, 0x0A);

    /* Icon strip #3: column + 0x30, 2x2 tiles (one column narrower). */
    col = (s8)LDS8(row_ptr + 0xD);
    call_CopyBackgroundTiles_sub_8127060((u32)(col + 0x30), 2, 2, A_unk_201D320, 0x0D, 0x0A);

    /* Icon strip #4: column + 1, 4x2 tiles (wider). */
    col = (s8)LDS8(row_ptr + 0xD);
    call_CopyBackgroundTiles_sub_8127060((u32)(col + 1), 4, 2, A_unk_201D520, 0x0C, 8);

    return (PreservedRegs){ r4_in, row_ptr, r6_in, r7_in, r10_in };
}

/* sub_8127060(r5 = row-descriptor record, r10) -> r5, r7 preserved */
Next sub_8127060(void) {
    PreservedRegs p = copy_list_row_tiles(R4, R5, R6, R7, R10);
    R4  = p.r4;
    R5  = p.r5;
    R6  = p.r6;
    R7  = p.r7;          /* output: r7 = entry r5 ("mov r7, r5"), untouched otherwise */
    R10 = p.r10;
    RETURN_TO_CALLER();
}

/* ── initTitleScreens_81270D0 ── */
/* register form: calls getKeyedTableEntry_811FB84 outside this file, which has no typed C interface yet */
static u32 sub_811FB84_call(u32 src_addr, u32 size) {
    R0 = src_addr;
    R1 = size;
    GUEST_CALL(getKeyedTableEntry_811FB84);
    return R0;
}

/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decompAndCopyData_call(u32 initRefs) {
    R0 = initRefs;
    GUEST_CALL(decompAndCopyData);
}

/* register form: calls battle_loadTileGridByNavi_8123300 outside this file, which has no typed C interface yet */
static void sub_8123300_call(u32 script_table, u32 dst, u32 decomp_buf) {
    R0 = script_table;
    R1 = dst;
    R2 = decomp_buf;
    GUEST_CALL(battle_loadTileGridByNavi_8123300);
}

/* Build the unknown_20096E0 dispatch table by reading the per-entry header
 * (a source/size pair from off_8127160), inflating it through getKeyedTableEntry_811FB84, and
 * copying the 12 bytes that follow into the table. Then run the two
 * decompAndCopyData initializers and finally battle_loadTileGridByNavi_8123300 with the script list. */
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

/* initTitleScreens_81270D0: initialize the unknown_20096E0 dispatch table and the
 * text-script/decompression tables used by the title screens. */
Next initTitleScreens_81270D0(void) {
    build_dispatch_table_and_init();
    RETURN_TO_CALLER();
}

/* ── battle_dispatchNextCmd_81272C4 ── */
/* register form: calls battleObject_dispatchCmdSpawnChild_812730C outside this file, which has no typed C interface yet;
 * preserves R4 (declared preserved output) across the callee via save/restore */
static u32 call_sub_812730C(u32 source, u32 type, u32 param, u32 flag, u32 pool_off) {
    u32 saved_r4 = R4;
    R0 = source; R1 = type; R2 = param; R3 = flag;
    R4 = A_byte_81268FC;
    R6 = pool_off;
    R7 = A_dword_81268F8;
    GUEST_CALL(battleObject_dispatchCmdSpawnChild_812730C);
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

/* battle_dispatchNextCmd_81272C4(r4 = saved, r5 = battle-object ptr, r10, lr) */
Next battle_dispatchNextCmd_81272C4(void) {
    sub_81272C4_impl(R5);
    RETURN_TO_CALLER();
}

/* ── chatbox_event163Progress_81273B4 ── */
/* chatbox_event163Progress_81273B4:
 * Event-163 chatbox script handler. If the event flag is set, advance the
 * event's progress counter (obtained via sub_8121198 from the toolkit-side
 * record) by one, capping the "finished" state at 15, and queue the matching
 * text script entry (18 normally, 19 once complete).
 * Preserves r4-r7 across the callee calls, as the original's push/pop does. */
Next chatbox_event163Progress_81273B4(void) {
    /* save r4-r7 like the original's push {r4-r7,lr} */
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    /* test event flag 163 */
    R0 = 1;
    R1 = 0x63;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (ZERO_FLAG()) {
        R4 = r4; R5 = r5; R6 = r6; R7 = r7;
        RETURN_TO_CALLER();
    }

    /* the per-event data record hangs off the toolkit object */
    u32 data = LD32(R10 + oToolkit_Unk200a220_Ptr);

    GUEST_CALL(sub_8121198);          /* returns current progress in R0 */
    u32 progress = R0 + 1;

    u32 textId;
    if (progress == 0xF) {
        progress = 0x13;              /* event complete */
        textId = 0x13;
    } else {
        ST32(data + 0xC, progress);   /* save incremented progress */
        textId = 0x12;
    }

    /* show the chatbox text for this stage */
    R0 = textId;
    R1 = 0;
    R2 = 0x600AC00;
    R3 = A_eTextScript201BF20;
    R4 = A_unk_201A600;
    R6 = data;
    GUEST_CALL(chatbox_renderGlyphViaTable);

    /* restore r4-r7 like the original's pop {r4-r7,pc} */
    R4 = r4; R5 = r5; R6 = r6; R7 = r7;
    RETURN_TO_CALLER();
}

/* ── paintMapEventTileBlock_81273F0 ── */
/* paintMapEventTileBlock_81273F0(r5 = map event object):
 * If event flag EVENT_163 is set, CopyBackgroundTiles paints a 3x2 block of
 * tiles (tile ids from byte_8126B10) at the object's position: the signed
 * byte at +0x0D plus 2 is the destination row (j), 9 is the column (i),
 * tile block 3, size 3x2.
 * Preserved: r4, r6, r7; r5 returns unchanged (original saves/restores it
 * around the CopyBackgroundTiles call where r5 = i_size = 2). */
Next paintMapEventTileBlock_81273F0(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;
    u32 obj = R5;

    /* TestEventFlagFromImmediate reads r0 = 1 and r1 = flag id (movflag). */
    R0 = 1;
    R1 = EVENT_163;
    GUEST_CALL(TestEventFlagFromImmediate);

    if (!ZERO_FLAG()) { /* flag set: paint the event tile block */
        R0 = (u32)((s8)LD8(obj + 0x0D) + 2); /* j: destination row */
        R1 = 9;                              /* i: destination column */
        R2 = 3;                              /* which_tile_block_32x32 */
        R3 = A_byte_8126B10;                 /* tile_ids */
        R4 = 3;                              /* j_size */
        R5 = 2;                              /* i_size */
        R7 = obj;                            /* callee sees r7 = r5 */
        GUEST_CALL(CopyBackgroundTiles);
    }

    /* Restore the saved registers the original pops back. */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
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

/* ── battle_renderChipSlots_81275C8 ── */
/* register form: calls chatbox_renderGlyph_8127580 outside this file, which has no typed C interface yet */
static void render_chip_slot(u32 slot_index, u32 palette_offset,
                             u32 sel_byte, u32 toolkit_addr) {
    u32 saved_r4 = R4;
    ST32(toolkit_addr + 8, sel_byte);
    R0 = slot_index;
    R1 = palette_offset;
    R2 = 0x6009000;
    R3 = A_eTextScript201BF20;
    R4 = A_unk_2018A00;
    GUEST_CALL(chatbox_renderGlyph_8127580);
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

Next battle_renderChipSlots_81275C8(void) {
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

/* ── battle_buildPetNaviList_8127638 ── */
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

/* register form: calls battle_getCurNaviTableEntry outside this file, which has no typed C interface yet */
static u32 get_secondary_navi_table(u32 start_index) {
    R4 = start_index;
    GUEST_CALL(battle_getCurNaviTableEntry);
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

/* battle_buildPetNaviList_8127638(toolkit = r5) — builds PET navi list, preserves r4-r7 via stack */
Next battle_buildPetNaviList_8127638(void) {
    process_pet_navi_table(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8127718 ── */
/* sub_8127718(r5 = textbox print state):
 * Prints pending characters of the text buffer byte_201D74C into the textbox
 * tile area at VRAM 0x600A000. r5+0x24 holds the index of the next character
 * to print and r5+0x26 the index one past the last pending one; when they are
 * equal there is nothing to do. Otherwise up to four characters are rendered,
 * each as a 0x200-byte slice at successively higher VRAM offsets, by calling
 * chatbox_renderGlyph_8127580. Characters with code >= 0x40 are first passed through
 * sub_8127760 (extended/control character handling), which may adjust the
 * code left in r0 before it is forwarded to chatbox_renderGlyph_8127580.
 *
 * r4-r7 are work registers here but must be restored for the caller, so their
 * entry values are saved and written back on exit. */
Next sub_8127718(void) {
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    R7 = LD16(R5 + 0x24);          /* index of next character to print */
    u32 endIndex = LD16(R5 + 0x26); /* one past the last pending character */

    if (R7 != endIndex) {
        R6 = 0;                    /* byte offset of the current slice in VRAM */
        do {
            R0 = LD8(A_byte_201D74C + R7); /* character code from the text buffer */

            if (R0 >= 0x40)
                GUEST_CALL(sub_8127760);   /* may rewrite the code in r0 */

            /* Draw this character: r0 = code, r1 = slice offset,
             * r2 = textbox VRAM base, r3 = script data, r4 = font table. */
            R1 = R6;
            R2 = 0x600A000;
            R3 = A_TextScript873EA50;
            R4 = A_byte_2019A00;
            GUEST_CALL(chatbox_renderGlyph_8127580);

            R7 += 1;               /* advance to the next character */
            R6 += 0x200;           /* advance to the next VRAM slice */
        } while ((s32)R6 <= 0x600);
    }

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
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

/* ── sub_8127800 ── */
/* sub_8127800(r10, lr):
 * Walk two parallel event-flag tables (group A: story/social flags,
 * group B: completion/chip flags). If any pair (A[i], B[i]) has either
 * flag set, set EVENT_F7 and stop; otherwise keep walking until exhausted.
 */
Next sub_8127800(void) {
    static const u32 group_a[13] = {
        0x00000321, 0x00000327, 0x0000032D, 0x00000333, 0x00000339,
        0x00000345, 0x0000034B, 0x00000351, 0x00000357, 0x0000035D,
        0x0000033F, 0x00000369, 0x00000303,
    };
    static const u32 group_b[13] = {
        0x000000AC, 0x000000AD, 0x000000AE, 0x000000AF, 0x000000B0,
        0x000000B1, 0x000000B2, 0x000000B3, 0x000000B4, 0x000000B5,
        0x000000B6, 0x000000B7, 0x000000B8,
    };

    for (u32 i = 0; i < 13; i++) {
        R0 = group_a[i];
        GUEST_CALL(TestEventFlag);
        if (!ZERO_FLAG()) {
        match:
            /* any hit sets EVENT_F7 */
            R0 = 0;
            R1 = 0xF7;
            GUEST_CALL(SetEventFlagFromImmediate);
            break;
        }
        R0 = group_b[i];
        GUEST_CALL(TestEventFlag);
        if (ZERO_FLAG()) {
            /* neither flag of the pair set -> advance */
            continue;
        }
        goto match;
    }

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

/* ── sub_8127990 ── */
/* sub_8127990(r0 = some_ptr, r10 = ?, lr = ?):
 *   prepares the pet-submenu on return and, if r0 is non-NULL,
 *   marks sSubmenu.unk_0D = 1 so the caller will display its menu.
 *   Output: r2 is preserved (callers rely on this).
 */
Next sub_8127990(void) {
    u32 ptr = R0;

    GUEST_CALL(pet_prepareMenuOnReturn_811F6C0);

    if (ptr != 0) {
        ST8(A_sSubmenu + 0xD, 1);
    }

    RETURN_TO_CALLER();
}

/* ── sub_81279A8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result_sub_81279A8;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_sub_81279A8 call_TestEventFlag_sub_81279A8(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_sub_81279A8){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

#define EVENT_FLAG_BASE 0x1DA0

/* Scan the story-progress byte table. For each entry whose event flag is NOT yet set,
 * check the parallel attribute table (4 bytes per entry); if any has bit 0x80, return 0
 * immediately. Otherwise return 1.  dword_2001140 holds the entry count; byte_2006530 is
 * the story-progress byte table; byte_812851C is the attribute table indexed by 4*byte. */
static u32 check_story_event_flags(void) {
    u32 count = LD32(A_dword_2001140);
    if (count == 0)
        return 1;

    for (u32 i = 0; i < count; i++) {
        u8 tbl_byte = LD8(A_byte_2006530 + i);
        u32 flag = (u32)tbl_byte + EVENT_FLAG_BASE;

        /* TestEventFlag leaves Z set when the flag is NOT active */
        if (!call_TestEventFlag_sub_81279A8(flag).z)
            continue;          /* flag is set – nothing to check */

        /* Flag is clear: look up the 4-byte-stride attribute entry */
        u8 attr = LD8(A_byte_812851C + 4u * tbl_byte);
        if (attr & 0x80)
            return 0;
    }
    return 1;
}

/* sub_81279A8() -> r0 = 0 or 1, nz from that value; r4-r7 preserved by push/pop */
Next sub_81279A8(void) {
    u32 result = check_story_event_flags();
    R0 = result;
    SET_FLAGS_NZ(result);
    RETURN_TO_CALLER();
}

/* ── battle_sceneReset_8127A1C ── */
/* Fade timer value stored to the render-info header on scene init. */
#define RENDER_FADE_TIMER  0x1F40

/* Two output registers from battleObject_advanceAnimTableBy0xFF_8120CC8 (fade-palette lookup). */
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

/* register form: calls toolkit_applyOverlayPalette_80015FC outside this file, which has no typed C interface yet */
static void call_init_something(u32 arg) {
    R0 = arg;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void call_set_screen_fade(u32 a, u32 b) {
    R0 = a; R1 = b;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet */
static void call_sub_8046664_sub_8127A1C(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
}

/* register form: calls battle_setupSceneTiles_8127CB8 outside this file, which has no typed C interface yet */
static void call_sub_8127CB8(void) {
    GUEST_CALL(battle_setupSceneTiles_8127CB8);
}

/* register form: calls battleObject_advanceAnimTableBy0xFF_8120CC8 outside this file, which has no typed C interface yet */
static FadePair call_fade_lookup(u32 a, u32 b) {
    R0 = a; R1 = b;
    GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
    FadePair p; p.step = R0; p.frame = R1;
    return p;
}

/* register form: calls sub_81283A0 outside this file, which has no typed C interface yet */
static void call_sub_81283A0(void) {
    GUEST_CALL(sub_81283A0);
}

/* register form: calls battle_fieldLockApply outside this file, which has no typed C interface yet */
static void call_sub_8128500(void) {
    GUEST_CALL(battle_fieldLockApply);
}

/* register form: calls palette_fillBufferWithFadeByte_8128260 outside this file, which has no typed C interface yet */
static void call_palette_fill_fade(void) {
    GUEST_CALL(palette_fillBufferWithFadeByte_8128260);
}

/* register form: calls npc_runFlagConditionalText outside this file, which has no typed C interface yet */
static void call_sub_8127E9C(void) {
    GUEST_CALL(npc_runFlagConditionalText);
}

/* register form: calls npc_renderEntryRows outside this file, which has no typed C interface yet */
static void call_sub_8128008(void) {
    GUEST_CALL(npc_renderEntryRows);
}

/* register form: calls bg_ReloadBackgroundRegion_5_3_81281C0 outside this file, which has no typed C interface yet */
static void call_bg_reload_region(void) {
    GUEST_CALL(bg_ReloadBackgroundRegion_5_3_81281C0);
}

/* register form: calls sub_81281E0 outside this file, which has no typed C interface yet */
static void call_sub_81281E0(void) {
    GUEST_CALL(sub_81281E0);
}

/* register form: calls battle_checkSubFlags_8128280 outside this file, which has no typed C interface yet */
static void call_sub_8128280(void) {
    GUEST_CALL(battle_checkSubFlags_8128280);
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

/* battle_sceneReset_8127A1C(r5 = transition-state ptr, r10 = toolkit ptr) — full scene reset */
Next battle_sceneReset_8127A1C(void) {
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
    /* State handler from off_8127AF8 (setFadeModeIfFading_8127B08, B18, B94, BE4), entered
     * as the original: r0 = handler, r1 = state byte (chatbox_completeCutsceneStep reads r1). */
    R1 = LD8(R5 + 2);
    R0 = LD32(0x08127AF8u + R1);
    GUEST_CALL_ADDR(R0);

    /* Common update steps, run regardless of the state handler above. */
    GUEST_CALL(sub_81281E0);
    GUEST_CALL(fieldEntityTileAnimTick_8127F54);
    GUEST_CALL(sub_8127FE4);
    GUEST_CALL(battle_checkSubFlags_8128280);
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    GUEST_CALL(battle_updateEStructSprites_80465F8);

    RETURN_TO_CALLER();
}

/* ── setFadeModeIfFading_8127B08 ── */
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

/* setFadeModeIfFading_8127B08(r5 = battle-object pointer) -> r0, nz c v */
Next setFadeModeIfFading_8127B08(void) {
    R0 = set_fade_mode_if_active(R5);
    RETURN_TO_CALLER();
}

/* ── submenu_HandleInput_8127B18 ── */
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

/* register form: calls npc_renderEntryRows outside this file, which has no typed C interface yet */
static void call_sub_8128008_sub_8127B18(void) {
    GUEST_CALL(npc_renderEntryRows);
}

/* register form: calls bg_ReloadBackgroundRegion_5_3_81281C0 outside this file, which has no typed C interface yet */
static void call_bg_reload_background_region_5_3_81281C0(void) {
    GUEST_CALL(bg_ReloadBackgroundRegion_5_3_81281C0);
}

/* register form: calls battleObject_advanceAnimTableBy0xFF_8120CC8 outside this file, which has no typed C interface yet */
static void call_sub_8120CC8(u32 a, u32 b, u32 *out0, u32 *out1) {
    R0 = a;
    R1 = b;
    GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
    *out0 = (u8)R0;
    *out1 = (u8)R1;
}

/* register form: calls menu_adjustCursorOrValue_811F7F8 outside this file, which has no typed C interface yet */
static u32 call_sub_811F7F8(u32 a, u32 b, u32 c, u32 d) {
    R0 = a;
    R1 = b;
    R2 = c;
    R3 = d;
    GUEST_CALL(menu_adjustCursorOrValue_811F7F8);
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

Next submenu_HandleInput_8127B18(void) {
    submenu_handle_input_8127B18(R5, R10);
    RETURN_TO_CALLER();
}

/* ── scene_updateClosing_8127C64 ── */
/* register form: calls sub_81281E0 outside this file, which has no typed C interface yet */
static void scene_step_81281E0(void) {
    GUEST_CALL(sub_81281E0);
}

/* register form: calls fieldEntityTileAnimTick_8127F54 outside this file, which has no typed C interface yet */
static void scene_step_8127F54(void) {
    GUEST_CALL(fieldEntityTileAnimTick_8127F54);
}

/* register form: calls sub_8127FE4 outside this file, which has no typed C interface yet */
static void scene_step_8127FE4(void) {
    GUEST_CALL(sub_8127FE4);
}

/* register form: calls battle_checkSubFlags_8128280 outside this file, which has no typed C interface yet */
static void scene_step_8128280(void) {
    GUEST_CALL(battle_checkSubFlags_8128280);
}

/* register form: calls entity_runAllUpdateHandlers_80465BC outside this file, which has no typed C interface yet */
static void run_all_entity_update_handlers(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
}

/* register form: calls battle_updateEStructSprites_80465F8 outside this file, which has no typed C interface yet */
static void scene_step_80465F8(void) {
    GUEST_CALL(battle_updateEStructSprites_80465F8);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static u32 screen_fade_active_sub_8127C64(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();          /* IsScreenFadeActive: Z set when no fade is running */
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet */
static void scene_step_8046664(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
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
     * or a byte store into oGameState_PendingByte */
    if (mode & 1)
        ST32(game_state + 0x78, exit_value);
    else
        ST8(game_state + oGameState_PendingByte, (u8)exit_value);

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

/* scene_updateClosing_8127C64: run the closing scene; r0-r3 and the flags on exit are whatever
 * the last callee left behind, r5/r10 are untouched. */
Next scene_updateClosing_8127C64(void) {
    run_closing_scene(R5, R10);
    RETURN_TO_CALLER();
}

/* ── battle_setupSceneTiles_8127CB8 ── */
/* register form: calls getKeyedTableEntry_811FB84 outside this file, which has no typed C interface yet */
static u32 compute_tile_descriptor(u32 source, u32 size) {
    R0 = source;
    R1 = size;
    GUEST_CALL(getKeyedTableEntry_811FB84);
    return R0;
}

/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decompress_data(u32 init_refs) {
    R0 = init_refs;
    GUEST_CALL(decompAndCopyData);
}

/* register form: calls battle_loadTileGridByNavi_8123300 outside this file, which has no typed C interface yet */
static void decompress_text_table(u32 table, u32 text_dest, u32 decomp_buf) {
    R0 = table;
    R1 = text_dest;
    R2 = decomp_buf;
    GUEST_CALL(battle_loadTileGridByNavi_8123300);
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

/* battle_setupSceneTiles_8127CB8(r4, r5 = battle_state, r6, r7, r10, lr) -> preserves r4-r7 */
Next battle_setupSceneTiles_8127CB8(void) {
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

/* ── sub_8127EF4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n; } call_TestEventFlag_result_sub_8127EF4;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_sub_8127EF4 call_TestEventFlag_sub_8127EF4(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_sub_8127EF4){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

#define FLAG_LO_BASE 0x1CA0u
#define FLAG_HI_BASE 0x1D20u
#define FLAG_TABLE   A_byte_812851C

/* Count story event flag pairs (lo/hi) in [0x1CA0, 0x1D20) where both flags are
 * set and the table entry's high bit matches the object byte at offset 0xD's low bit. */
static u32 count_matched_flags(u32 obj_ptr) {
    u32 count = 0;
    for (u32 idx = 0; idx < (FLAG_HI_BASE - FLAG_LO_BASE); idx++) {
        if (call_TestEventFlag_sub_8127EF4(FLAG_LO_BASE + idx).z)
            continue;
        if (call_TestEventFlag_sub_8127EF4(FLAG_HI_BASE + idx).z)
            continue;

        u8 table_byte = LD8(FLAG_TABLE + idx * 4);
        u8 obj_byte = LD8(obj_ptr + 0xD);

        bool table_high = (table_byte & 0x80) != 0;
        bool obj_low = (obj_byte & 1) != 0;

        if (table_high == obj_low)
            count++;
    }
    return count;
}

/* sub_8127EF4(r5 = object ptr, r10 passthrough) -> r0 = matched count */
Next sub_8127EF4(void) {
    R0 = count_matched_flags(R5);
    RETURN_TO_CALLER();
}

/* ── fieldEntityTileAnimTick_8127F54 ── */
/* fieldEntityTileAnimTick_8127F54(r5 = entity, r10, lr):
 *   Per-frame update for an entity on the field: if it is no longer active
 *   (flags cleared or state id == 0x0C terminator), bail out. Otherwise load
 *   an affine-bg parameter block from the entity, push it through battleObject_queueOamFromTableRow_811FAF4
 *   along with the tile index in the col/row table, then tick the entity's
 *   duration timer. When the timer hits zero, pull the next (tile, duration)
 *   pair from the dword_8127980 sequence via battleObject_advanceAnimTableBy0xFF_8120CC8 and store them back. */
Next fieldEntityTileAnimTick_8127F54(void) {
    u32 entity = R5;

    /* Bail if the entity has been cleared (flag byte == 0) or its state
     * id is 0x0C (terminator). */
    if (LD8(entity + 0x1E) == 0)         RETURN_TO_CALLER();
    if (LD8(entity + 0x02) == 0x0C)      RETURN_TO_CALLER();

    /* Build the affine-bg setup word and forward it (plus the tile id and
     * the per-column tile table) to battleObject_queueOamFromTableRow_811FAF4. The 0x40000000 constant
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
        GUEST_CALL(battleObject_queueOamFromTableRow_811FAF4);
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
         * dword_8127980 sequence via battleObject_advanceAnimTableBy0xFF_8120CC8. */
        R1 = A_dword_8127980;
        GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
        ST8(entity + 0x0C, (u8)R0);
        ST8(entity + 0x0E, (u8)R1);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8127FE4 ── */
/* sub_8127FE4(r5 = object pointer)
 * Wrapper that fills out the argument set and forwards to object_computeAndQueueOamEntry_811FA98
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

    /* object_computeAndQueueOamEntry_811FA98 needs R4=4, R6=oSomeTable_30CA4C, R7=0x35. */
    R4 = 4;
    R6 = 0x30CA4C;
    R7 = 0x35;

    GUEST_CALL(object_computeAndQueueOamEntry_811FA98);

    R4 = save_r4;
    R6 = save_r6;
    R7 = save_r7;

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

/* ── sub_81281E0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r5, r6, r7; } call_sub_802FE28_result;
/* register form: generated wrapper: sprite_queueOamEntryThunk_802FE28 has no typed C interface yet */
static inline call_sub_802FE28_result call_sub_802FE28(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);
    return (call_sub_802FE28_result){ R0, R5, R6, R7 };
}

/* Display up to four icon sprites via the sprite-queue routine.
 * r5 points to a battle-object state struct:
 *   [0x1E] u16  number of icons (0 = skip; values above 4 are clamped to 4),
 *   [0x24] u16  index of the first icon's record in the dword_2027590 array
 *               (each record is 0x20 bytes; the index is re-read every iteration).
 * Per icon i:
 *   - screen position: halfword pair i from the byte_8128250 ROM table,
 *     packed as (first << 16 | second) into attr01's low 16 bits.
 *   - tile number: high halfword of word 0 of the record, bit 7 cleared,
 *     scaled by 4 and offset by base tile 0x210.
 *   - attr2: (0xA << 12) | (2 << 10) | tile = 0xA800 | tile.
 * Calls sprite_queueOamEntryThunk_802FE28(attr01, attr2, 0, 2) for each icon.
 * r5 is unchanged on return (it is saved/restored via push/pop in the original). */
static u32 show_icon_sprites(u32 state) {
    u32 count = LD16(state + 0x1E);
    if (count == 0)
        return state;
    if (count > 4)
        count = 4;

    for (u32 i = 0; i < count; i++) {
        /* packed on-screen position of icon i (two halfwords in ROM) */
        u32 posAddr = A_byte_8128250 + i * 4;
        u32 packedPos = (LD16(posAddr) << 16) | LD16(posAddr + 2);

        /* sprite record: base + (firstIndex + i) * 32 bytes */
        u32 record = A_dword_2027590 + (LD16(state + 0x24) + i) * 0x20;

        /* tile number from high halfword of record word 0 */
        u32 tile = 0x210 + (((LD32(record) >> 16) & ~0x80u) << 2);

        /* attr01 = OBJ base | packedPos; attr2 = 0xA800 | tile */
        call_sub_802FE28(0x40000000u | packedPos,
                         0xA800u | tile,
                         0,
                         2);
    }

    return state;
}

/* sub_81281E0(r5 = state ptr) -> r5 (unchanged) */
Next sub_81281E0(void) {
    R5 = show_icon_sprites(R5);
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

/* ── battle_checkSubFlags_8128280 ── */
/* battle_checkSubFlags_8128280: For each of up to 4 entries in the battle object table, check the
 * "sub flag" derived from the object's flags+0x18 high half; if the matching
 * event flag (sub_flag + 0x1DA0) is not set, call battleObject_tickOamSubFlagAnimSlot_81282C0 with the index.
 *
 *   r5 = battle object pointer
 *   r10 = (preserved, used by caller)
 *
 *   inputs:  r5 r10 lr
 *   outputs: r5
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r6 r7
 */
Next battle_checkSubFlags_8128280(void) {
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
            GUEST_CALL(battleObject_tickOamSubFlagAnimSlot_81282C0);
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_81283A0 ── */
/* sub_81283A0: Builds the battle-arena object array for the current encounter.
 * Reads the participant list, looks up each member's 4-byte descriptor in a
 * static table, and writes one 32-byte entry per matching member into the
 * battle-object buffer (dword_2027590). */

#define OBJECT_BUFFER_SIZE  0x1000
#define ENTRY_SIZE          0x20

/* register form: ZeroFillByEightWords takes (dst, size) in r0/r1; callees
 * in this function may clobber r4-r7 which the original restores via push/pop */
static void zero_fill_object_buffer(u32 dst, u32 size) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = dst;
    R1 = size;
    GUEST_CALL(ZeroFillByEightWords);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: battle_countFlaggedActions_81284B4 reads r4 (output buffer base) and returns a
 * counter pair in r0/r1; it may clobber r4-r7 */
static u32 seed_entry_counter(u32 buf, u32 *out_alt) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R4 = buf;
    GUEST_CALL(battle_countFlaggedActions_81284B4);
    *out_alt = R1;
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return R0;
}

/* register form: battle_fieldLockDispatch_8120018 takes (buf, count, flag2, flag3) in r0-r3;
 * it may clobber r4-r7 */
static void finalize_object_list(u32 buf, u16 count, u8 arg2, u8 arg3) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = buf;
    R1 = count;
    R2 = arg2;
    R3 = arg3;
    GUEST_CALL(battle_fieldLockDispatch_8120018);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

static void build_battle_objects(u32 object_ptr) {
    const u32 buf = A_dword_2027590;

    /* Clear the entire object buffer */
    zero_fill_object_buffer(buf, OBJECT_BUFFER_SIZE);

    /* Seed the per-entry counter from encounter logic */
    u32 alt;
    u32 primary = seed_entry_counter(buf, &alt);

    /* Select counter based on team-membership flag (bit 0 of object+0xD) */
    bool in_team = (LD8(object_ptr + 0x0D) & 1) != 0;
    u32 counter = in_team ? primary : alt;

    /* Number of active arena participants; skip everything if none */
    u32 count = LD8(A_dword_2001140);
    if (count == 0)
        return;

    /* Reset the written-entry counter in the object */
    ST16(object_ptr + 0x1E, 0);

    u32 out = buf;

    for (u32 i = 0; i < count; i++) {
        u8 id = LD8(A_byte_2006530 + i);
        u32 tbl = A_byte_812851C + (u32)id * 4;
        u8 flag_byte = LD8(tbl);

        /* Write entry only when bit 0x80 of the table flag matches team status */
        bool table_flag = (flag_byte & 0x80) != 0;
        if (in_team != table_flag)
            continue;

        u32 inv = (~counter) & 0xFFFF;

        /* Assemble and store the 32-byte object entry */
        ST32(out + 0x00, ((u32)flag_byte << 16) | inv);
        ST8(out + 0x04, LD8(tbl + 1));
        ST32(out + 0x08, ((u32)LD8(tbl + 2) << 16) | inv);
        ST32(out + 0x0C, ((u32)LD8(tbl + 3) << 16) | inv);
        ST8(out + 0x10, (u8)(id * 2));
        ST8(out + 0x14, (u8)(id * 2 + 1));
        ST32(out + 0x18, ((u32)id << 16) | id);
        ST32(out + 0x1C, (counter << 16) | counter);

        counter--;

        /* Track entries written in the object's halfword at +0x1E */
        ST16(object_ptr + 0x1E, LD16(object_ptr + 0x1E) + 1);
        out += ENTRY_SIZE;
    }

    /* Hand the assembled list to the next stage if at least one entry was written */
    u16 entries = LD16(object_ptr + 0x1E);
    if (entries != 0) {
        u8 a2 = LD8(A_byte_2000FE0);
        u8 a3 = LD8(A_byte_2000FE0 + 1);
        finalize_object_list(buf, entries, a2, a3);
    }
}

/* sub_81283A0(r5 = battle-object pointer) */
Next sub_81283A0(void) {
    build_battle_objects(R5);
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

/* ── battle_countFlaggedActions_81284B4 ── */
/* Count entries in the tag index table whose first byte has bit 7 set vs unset.
 * dword_2001140: number of entries to scan (loop bound; signed do-while runs at least once).
 * byte_2006530: byte array of tag indices into byte_812851C.
 * byte_812851C: 4-byte-per-entry table; byte 0 of each entry carries the flag. */

typedef struct { u32 set_count; u32 reset_count; } TagCounts;

static TagCounts count_tag_entries(void) {
    u32 remaining   = LD32(A_dword_2001140);
    u32 idx_ptr     = A_byte_2006530;
    u32 set_count   = 0;
    u32 reset_count = 0;

    /* do-while: body always executes at least once; loop while remaining (signed) > 0 */
    do {
        u32 tag_index   = LD8(idx_ptr);
        u32 entry_flags = LD8(A_byte_812851C + tag_index * 4);
        if (entry_flags & 0x80)
            set_count++;
        else
            reset_count++;
        idx_ptr++;
        remaining--;
    } while ((s32)remaining > 0);

    return (TagCounts){ set_count, reset_count };
}

/* battle_countFlaggedActions_81284B4 -> r0 = set_count, r1 = reset_count (r4-r7 preserved by original push/pop) */
Next battle_countFlaggedActions_81284B4(void) {
    TagCounts result = count_tag_entries();
    R0 = result.set_count;
    R1 = result.reset_count;
    RETURN_TO_CALLER();
}

/* ── HandleKeyItemMenu8128730 ── */
/* HandleKeyItemMenu8128730 — dispatcher for the key item menu.
 *
 * The byte at [r5,#1] is the menu's current step, stored as a byte offset
 * (0 / 4 / 8) into the handler table at off_8128744. The selected handler
 * runs with the caller's register state intact; this routine then simply
 * returns, so the handler's results (r5, r8-r12) pass straight through
 * to the caller. */
Next HandleKeyItemMenu8128730(void) {
    switch (LD8(R5 + 1)) {
    case 0: /* offset into off_8128744[0] */
        GUEST_CALL(sub_8128750);
        break;
    case 4: /* offset into off_8128744[1] */
        GUEST_CALL(sub_81287B4);
        break;
    case 8: /* offset into off_8128744[2] */
        GUEST_CALL(sub_81288E4);
        break;
    }
    RETURN_TO_CALLER();
}

/* ── sub_8128750 ── */
/* sub_8128750: battle/title screen init - clears VRAM/OAM, sets up render info,
 * initialises the toolkit state block, runs the various sub-initialisers and
 * kicks off the screen-fade animation. */
Next sub_8128750(void) {
    /* Reset all GBA video buffers to zero. */
    GUEST_CALL(zeroFillVRAM);
    GUEST_CALL(ZeroFill_byte_3001960);
    GUEST_CALL(ZeroFillGFX30025c0);

    /* Stage = 0x10 (some "scene" indicator for toolkit_applyOverlayPalette_80015FC). */
    R0 = 0x10;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);

    /* r10 holds the toolkit pointer; mirror it into r7 for the render-info setup. */
    R7 = R10;
    u32 renderInfo = LD32(R7 + oToolkit_RenderInfoPtr);

    /* Default render-info layout: tile base 0x1F40, all other slot offsets zero. */
    ST16(renderInfo + 0x00, 0x1F40);
    ST16(renderInfo + 0x10, 0);
    ST16(renderInfo + 0x12, 0);
    ST16(renderInfo + 0x14, 0);
    ST16(renderInfo + 0x16, 0);
    ST16(renderInfo + 0x18, 0);
    ST16(renderInfo + 0x1A, 0);

    /* State byte = 4, timer byte = 0. */
    ST8(R5 + 0x01, 4);
    ST8(R5 + 0x02, 0);

    GUEST_CALL(init_loadScreenAssets);

    /* A couple of flag bytes cleared. */
    ST8(R5 + 0x0F, 0);
    ST8(R5 + 0x10, 0);

    /* Two "object slot" halfwords and one byte seeded with 0xFF (no-object sentinel). */
    ST16(R5 + 0x26, 0xFF);
    ST16(R5 + 0x22, 0xFF);
    ST8(R5 + 0x0E, 0xFF);

    GUEST_CALL(submenu_buildKeyItemList);
    GUEST_CALL(sub_8128A44);
    GUEST_CALL(sub_8128AB0);
    GUEST_CALL(sub_8128B34);

    /* Fade from black (8 steps, 0x10 == palette bank 1). */
    R0 = 8;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    RETURN_TO_CALLER();
}

/* ── sub_81287B4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5; } call_battle_queueHpUpdate_8128C20_result;
/* register form: generated wrapper: battle_queueHpUpdate_8128C20 has no typed C interface yet */
static inline call_battle_queueHpUpdate_8128C20_result call_battle_queueHpUpdate_8128C20(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_queueHpUpdate_8128C20);
    return (call_battle_queueHpUpdate_8128C20_result){ R5 };
}

typedef struct { u32 r5; } call_sub_8128B34_result;
/* register form: generated wrapper: sub_8128B34 has no typed C interface yet */
static inline call_sub_8128B34_result call_sub_8128B34(void) {
    GUEST_CALL(sub_8128B34);
    return (call_sub_8128B34_result){ R5 };
}

/* sub_81287B4 - battle menu scene step dispatcher.
 *
 * The routine picks one of three handler routines from the jump table at
 * off_81287D0, indexed by the step byte stored at [r5+2], and enters it with
 * "mov lr, pc; bx r0" - i.e. the handler sees the caller's entire register
 * file and sub_81287B4 keeps whatever the handler leaves in r5/r8-r12.
 * Afterwards two common follow-ups run unconditionally:
 *   battle_queueHpUpdate_8128C20 (gets the handler's r5, returns an updated r5)
 *   sub_8128B34                  (gets r10, returns an updated r5)
 */

/* Registers the original leaves behind when the step is done. */
typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
} SceneStepResult;

/* register form: the jump-table dispatch hands the whole caller register file
 * to the selected handler ("mov lr, pc; bx r0") and its results are whatever
 * the handler leaves in r5/r8-r12. No C calling convention can express a
 * whole-register call, so this helper programs the guest registers directly
 * around the single indirect call, then performs the two common follow-ups. */
static SceneStepResult run_scene_step(u32 a_r0, u32 a_r1, u32 a_r2, u32 a_r3,
                                      u32 a_r4, u32 a_r5, u32 a_r6, u32 a_r7,
                                      u32 a_r8, u32 a_r9, u32 a_r10,
                                      u32 a_r11, u32 a_r12)
{
    SceneStepResult result;

    /* Step byte selects the handler; it is already a byte offset into the
     * table of Thumb entry points (sceneStep / menu step handlers). */
    u32 step = LD8(a_r5 + 2);
    u32 handler = LD32(A_off_81287D0 + step);

    R0 = a_r0; R1 = a_r1; R2 = a_r2; R3 = a_r3;
    R4 = a_r4; R5 = a_r5; R6 = a_r6; R7 = a_r7;
    R8 = a_r8; R9 = a_r9; R10 = a_r10; R11 = a_r11; R12 = a_r12;

    GUEST_CALL_ADDR(handler);

    /* Keep what the handler left behind; the two follow-ups preserve
     * r8-r12, so these are the final values for everything but r5. */
    result.r5 = R5;
    result.r8 = R8;
    result.r9 = R9;
    result.r10 = R10;
    result.r11 = R11;
    result.r12 = R12;

    /* Common follow-up 1: queue an HP update for the (possibly moved) state. */
    result.r5 = call_battle_queueHpUpdate_8128C20(result.r5).r5;

    /* Common follow-up 2: reads r10, which is unchanged from entry here. */
    result.r5 = call_sub_8128B34().r5;

    return result;
}

Next sub_81287B4(void) {
    SceneStepResult out = run_scene_step(R0, R1, R2, R3, R4, R5, R6, R7,
                                         R8, R9, R10, R11, R12);
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── sceneStep_81287DC ── */
/* sceneStep_81287DC:
 *   inputs:  r5 r10 lr
 *   outputs: r5
 *
 * Scene step routine. If a screen fade is currently active, mark the
 * scene as needing to be cancelled (r5->field_2 = 4). Then run the two
 * per-frame scene helpers (battleObjectAffineEnqueue_8128B4C / sub_8128B98). Finally snapshot
 * the live position/animation fields into the "previous" slots of the
 * scene object so the next frame can compare against them.
 */
Next sceneStep_81287DC(void) {
    /* push {lr} ... pop {pc}: the original saves LR only to throw it
       away on return, so no real stack frame is needed here. */

    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        ST8(R5 + 2, 4);
    }

    GUEST_CALL(battleObjectAffineEnqueue_8128B4C);
    GUEST_CALL(sub_8128B98);

    /* snapshot live state into the "prev" fields */
    ST16(R5 + 0x26, LD16(R5 + 0x24));
    ST16(R5 + 0x22, LD16(R5 + 0x20));
    ST8 (R5 + 0x0E, LD8 (R5 + 0x0D));

    RETURN_TO_CALLER();
}

/* ── sub_81288E4 ── */
/* sub_81288E4: run the PET menu's closing steps (two cleanup routines), then
 * back out to the map screen unless a screen fade is still in progress.
 * Takes no arguments; r5 passes through unchanged. */
Next sub_81288E4(void) {
    GUEST_CALL(battleObjectAffineEnqueue_8128B4C);
    GUEST_CALL(battle_queueHpUpdate_8128C20);
    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG())
        GUEST_CALL(pet_menuExitToMap_811F708);
    RETURN_TO_CALLER();
}

/* ── sub_8128A44 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3, r8, r9, r10, r11, r12; } call_renderTextGfx_8045F8C_result;
/* register form: generated wrapper: renderTextGfx_8045F8C has no typed C interface yet */
static inline call_renderTextGfx_8045F8C_result call_renderTextGfx_8045F8C(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(renderTextGfx_8045F8C);
    return (call_renderTextGfx_8045F8C_result){ R3, R8, R9, R10, R11, R12 };
}

/* sub_8128A44: renders one row of up to 8 text glyphs into VRAM tile data by
 * calling renderTextGfx once per character.  The state object at r5 provides
 * a start index (+0x24) and a total char count (+0x1E).  Each glyph's
 * attribute byte is fetched from the script table with stride 2, and the tile
 * is written 0x280 bytes apart in the destination buffer. */

typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
} RenderRowResult;

#define TILE_SIZE          0x280u
#define VRAM_TILE_DATA     0x6004020u
#define PALETTE_SLOT       0xAu
#define ATTR_STRIDE        2u
#define MAX_GLYPHS_PER_ROW 8

static RenderRowResult render_text_row(u32 state) {
    /* register form: r4-r7 are preserved across this function but the
     * renderTextGfx wrapper clobbers them; save before loop, restore after. */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    u32 saved_r8 = R8, saved_r9 = R9, saved_r10 = R10;
    u32 saved_r11 = R11, saved_r12 = R12;

    RenderRowResult out = { state, saved_r8, saved_r9, saved_r10, saved_r11, saved_r12 };

    u32 scriptBase = A_eTextScript201BF00;
    u32 scriptByteOffset = (u32)LD16(state + 0x24) * ATTR_STRIDE;
    u32 charCount = LD16(state + 0x1E);

    if (charCount == 0) {
        /* register form: restore preserved r4-r7 (no calls made, already correct) */
        R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
        return out;
    }

    u32 remaining = charCount - scriptByteOffset;
    u32 glyphCount = MAX_GLYPHS_PER_ROW;
    if ((s32)remaining < (s32)MAX_GLYPHS_PER_ROW)
        glyphCount = remaining;

    for (u32 i = 0; i < glyphCount; i++) {
        u32 attr = LD8(scriptBase + ATTR_STRIDE * (scriptByteOffset + i));
        u32 tileOffset = TILE_SIZE * i;

        call_renderTextGfx_8045F8C_result res = call_renderTextGfx_8045F8C(
            A_TextScript873D9FC,
            attr,
            A_byte_2017A00 + tileOffset,
            VRAM_TILE_DATA + tileOffset,
            PALETTE_SLOT,
            1,
            A_dword_86A5D60,
            0
        );

        out.r8  = res.r8;
        out.r9  = res.r9;
        out.r10 = res.r10;
        out.r11 = res.r11;
        out.r12 = res.r12;
    }

    /* register form: restore caller's preserved r4-r7 after clobbering calls */
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;

    return out;
}

/* sub_8128A44(r5 = state ptr, r10) -> r5 r8 r9 r10 r11 r12 */
Next sub_8128A44(void) {
    RenderRowResult res = render_text_row(R5);
    R5  = res.r5;
    R8  = res.r8;
    R9  = res.r9;
    R10 = res.r10;
    R11 = res.r11;
    R12 = res.r12;
    RETURN_TO_CALLER();
}

/* ── sub_8128AB0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_HalfwordFill_result;
/* register form: generated wrapper: HalfwordFill has no typed C interface yet */
static inline call_HalfwordFill_result call_HalfwordFill(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(HalfwordFill);
    return (call_HalfwordFill_result){ R2 };
}

/* Build a 4-row interleaved tile-pair table in the staging buffer (unk_201C040).
 * First fills 0x170 halfwords with the blank tile 0x7200, then overwrites with
 * incrementing tile IDs starting at 0x7201 in two interleaved half-rows per
 * outer iteration (max 4).  The row budget [r5+0x1E] − 2*[r5+0x24] gates how
 * many half-row checks pass before the outer loop terminates early; a zero
 * value in [r5+0x1E] skips the table build entirely. */
static void build_tilemap(u32 object) {
    u16 total_rows = LD16(object + 0x1E);
    u16 skip       = LD16(object + 0x24);
    u32 remaining  = (u32)total_rows - (u32)(skip << 1);

    u32 base = A_unk_201C040;
    call_HalfwordFill(base, 0x170, 0x7200);

    if (total_rows == 0)
        return;

    u32 buf_a = base;
    u32 buf_b = base + 0x2E;
    u16 tile  = 0x7201;
    u32 outer = 4;

    for (;;) {
        /* First half-row: 10 paired halfwords at byte offsets 0..18. */
        u32 off = 0;
        for (u32 i = 0; i < 0x14; i += 2) {
            ST16(buf_a + off, tile++);
            ST16(buf_b + off, tile++);
            off += 2;
        }

        remaining -= 1;
        if (remaining == 0)
            break;

        /* Second half-row: 10 paired halfwords at byte offsets 26..44. */
        off += 6;
        for (u32 i = 0; i < 0x14; i += 2) {
            ST16(buf_a + off, tile++);
            ST16(buf_b + off, tile++);
            off += 2;
        }

        remaining -= 1;
        if (remaining == 0)
            break;

        /* Advance to the next row-pair block (0x2E halfwords × 2). */
        buf_a += 0x5C;
        buf_b += 0x5C;

        outer--;
        if (outer == 0)
            break;
    }
}

/* sub_8128AB0(r5 = object pointer) -> r5 preserved */
Next sub_8128AB0(void) {
    build_tilemap(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8128B34 ── */
/* sub_8128B34: copy the 23x8 tile block at unk_201C040 into background tile
 * block 2 (a 32x32 tile block), at position j=4, i=3.
 * CopyBackgroundTiles takes its size arguments in r4/r5 (game's own convention);
 * r4-r7 must still be preserved for the caller. */
Next sub_8128B34(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    R0 = 4;             /* j */
    R1 = 3;             /* i */
    R2 = 2;             /* tileBlock32x32 */
    R3 = A_unk_201C040; /* tileIds */
    R4 = 0x17;          /* j_size */
    R5 = 8;             /* i_size */
    GUEST_CALL(CopyBackgroundTiles);

    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── battleObjectAffineEnqueue_8128B4C ── */
/* battleObjectAffineEnqueue_8128B4C: Battle object rendering entry — pack a tile/affine descriptor and
 * submit it via the affine-OBJ queue (battleObject_queueOamFromTableRow_811FAF4). Reads the battle-object
 * "visible" flag at +0x1E; if unset, returns immediately. Outputs the resulting
 * tile attribute byte back into the object at +0xC.
 *
 * inputs:  r5, r10, lr
 * outputs: r5
 * clobbers: nz, c, v, r0, r1, r2, r3, lr
 * preserved: r4, r5, r6, r7
 */
Next battleObjectAffineEnqueue_8128B4C(void) {
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
    GUEST_CALL(battleObject_queueOamFromTableRow_811FAF4);
    ST8(R5 + 0xC, (u8)R0);

done:
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_8128B98 ── */
/* sub_8128B98(r5 = object, r10 = toolkit):
 *   If the object's flag at +0x1E is zero, do nothing.
 *   Otherwise, if the current state (positions +D/+E and pair at +20/+22 vs +24/+26) hasn't
 *   actually changed, do nothing.
 *   Otherwise look up a script byte in eTextScript201BF00 indexed by (pos_y + pos_y2,
 *   pos_x). If it's 0x2C, also stash the result of CheckKeyItem at chatbox+0x4C.
 *   Finally run the script via chatbox_runScript_803FD78 with archive unk_201C1B0.
 *
 *   inputs:  r5 r10 lr
 *   outputs: r5
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r4 r5 r6 r7
 */
Next sub_8128B98(void) {
    /* If the flag at +0x1E is zero, bail out. */
    if (LD16(R5 + 0x1E) == 0)
        RETURN_TO_CALLER();

    /* Compare positions: pos_x (+D) vs prev_pos_x (+E), and the two 16-bit fields
     * (+24 vs +26) and (+20 vs +22). If all four match, bail out. */
    u8 curX  = LD8(R5 + 0xD);
    u8 prevX = LD8(R5 + 0xE);
    if (curX == prevX) {
        u16 a24 = LD16(R5 + 0x24);
        u16 a26 = LD16(R5 + 0x26);
        if (a24 == a26) {
            u16 a20 = LD16(R5 + 0x20);
            u16 a22 = LD16(R5 + 0x22);
            if (a20 == a22)
                RETURN_TO_CALLER();
        }
    }

    /* Index into eTextScript201BF00:
     *   idx = (LD16(+24) + LD16(+20)) * 2 + LD8(+D), * 2
     * i.e. (v24+v20)*4 + curX*2. */
    u16 v24 = LD16(R5 + 0x24);
    u16 v20 = LD16(R5 + 0x20);
    u32 idx = ((u32)(v24 + v20) << 1) + curX;
    idx = idx << 1;
    u8 scriptByte = LD8(A_eTextScript201BF00 + idx);

    if (scriptByte == 0x2C) {
        /* Save scriptByte across the CheckKeyItem call. */
        u32 saveSP = SP - 4;
        SP = saveSP;
        ST32(saveSP, scriptByte);

        R0 = scriptByte;
        GUEST_CALL(CheckKeyItem);

        SP += 4;
        u8 saved = (u8)LD32(saveSP);

        u32 cb = LD32(R10 + oToolkit_ChatboxPtr);
        ST32(cb + 0x4C, R0);

        scriptByte = saved;
    }

    R0 = A_unk_201C1B0;
    R1 = scriptByte;
    GUEST_CALL(chatbox_runScript_803FD78);

    RETURN_TO_CALLER();
}

/* ── checkTextScriptEnd_8128BF0 ── */
/* checkTextScriptEnd_8128BF0(r5 = dialogue object):
 *   If the dialogue is still being drawn (state word at +0x1E) and the script
 *   byte selected by (text_id_hi + text_id_lo)*2 + speaker_id is 0x40
 *   (the "end of script" sentinel), kick the object into state 8 and trigger
 *   a fast fade-out so the scene can close. */
Next checkTextScriptEnd_8128BF0(void) {
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

/* ── battle_queueHpUpdate_8128C20 ── */
/* battle_queueHpUpdate_8128C20(r5 = battle object pointer):
 * Build a coordinate from the battle object's HP field (parity + half) and queue
 * a object_computeAndQueueOamEntry_811FA98 call via a command buffer (opcode 0xE1.0x13, key 0x20002).
 * Preserves r4, r5, r6, r7 across the call. */
Next battle_queueHpUpdate_8128C20(void) {
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

    GUEST_CALL(object_computeAndQueueOamEntry_811FA98);

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

/* ── battle_initScene_8129248 ── */
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

/* register form: calls toolkit_applyOverlayPalette_80015FC outside this file, which has no typed C interface yet */
static void sub_80015FC_call(u32 arg) {
    R0 = arg;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet */
static void sub_8046664_call_sub_8129248(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
}

/* register form: calls scene_loadAndStartCutscene_812AF18 outside this file, which has no typed C interface yet */
static void scene_load_and_start_cutscene(void) {
    GUEST_CALL(scene_loadAndStartCutscene_812AF18);
}

/* register form: calls battleObject_advanceAnimTableBy0xFF_8120CC8 outside this file, which has no typed C interface yet.
 * Returns the r0/r1 the callee left behind. */
static void sub_8120CC8_call(u32 a0, u32 a1, u32 *out0, u32 *out1) {
    R0 = a0;
    R1 = a1;
    GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
    *out0 = R0;
    *out1 = R1;
}

/* Init battle scene: clear VRAM/GFX, prime render info, start opening cutscene,
 * then write the variant bytes from the dword_8129180 table into the state struct.
 * Returns the r0/r1 the original leaves: r0 = 0 (final `mov r0, #0`),
 * r1 = the second byte battleObject_advanceAnimTableBy0xFF_8120CC8 produced (still in r1 at return). */
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
     * byte battleObject_advanceAnimTableBy0xFF_8120CC8 returned in r1, still live in r1 at the pop. */
    *out_r0 = 0;
    *out_r1 = hi;
}

Next battle_initScene_8129248(void) {
    u32 out_r0, out_r1;

    init_battle_scene_sub_8129248(R10, R5, &out_r0, &out_r1);

    R0 = out_r0;
    R1 = out_r1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── dispatchBattleState_81292A8 ── */
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
    case 0: GUEST_CALL(battle_initFullCutscene_81292D8); break;
    case 1: GUEST_CALL(battle_submenuInputUpdate_812933C); break;
    case 2: GUEST_CALL(scene_runFadePopTick_8129400); break;
    case 3: GUEST_CALL(menu_yesNoPromptUpdate); break;
    case 4: GUEST_CALL(battle_updateEntitiesAndStartFadeOut); break;
    case 5: GUEST_CALL(battle_endBattleSave); break;
    case 6: GUEST_CALL(battle_frameUpdateAndStopRfuAfterFade); break;
    }
}

/* dispatchBattleState_81292A8: dispatch to battle state handler via jump table.
 * inputs:  r5 (state object pointer), r2–r12, lr (all forwarded to callee)
 * outputs: all registers and flags set by the selected handler */
Next dispatchBattleState_81292A8(void) {
    dispatch_battle_state();
    RETURN_TO_CALLER();
}

/* ── battle_initFullCutscene_81292D8 ── */
/* register form: calls sprite_drawIfFlagSet_812B504 outside this file, which has no typed C interface yet */
static void call_draw_battle_sprite(void) {
    GUEST_CALL(sprite_drawIfFlagSet_812B504);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool call_is_screen_fade_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    return ZERO_FLAG();  /* Z set → fade active → beq taken → early return */
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet */
static void call_init_cutscene_scene(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
}

/* register form: calls clearStruct200BC30_803EBAC outside this file, which has no typed C interface yet */
static void call_clear_bg_struct(void) {
    GUEST_CALL(clearStruct200BC30_803EBAC);
}

/* register form: calls battle_spawnEStructRecordsFromTable_80465A0 outside this file, which has no typed C interface yet */
static void call_process_tile_data(u32 ptr) {
    R0 = ptr;
    GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);
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

/* battle_initFullCutscene_81292D8(battle_ptr = r5, r4 = r4) -> r4 (preserved); r0-r3 + flags from callee */
Next battle_initFullCutscene_81292D8(void) {
    R4 = play_cutscene_setup(R5, R4);
    RETURN_TO_CALLER();
}

/* ── battle_submenuInputUpdate_812933C ── */
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

/* register form: calls battle_updateEStructSprites_80465F8 outside this file, which has no typed C interface yet */
static void run_secondary_update(void) {
    GUEST_CALL(battle_updateEStructSprites_80465F8);
}

/* register form: calls submenu_positionOptionSprite outside this file, which has no typed C interface yet */
static void set_menu_tick(u32 ticks) {
    R0 = ticks;
    GUEST_CALL(submenu_positionOptionSprite);
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

/* battle_submenuInputUpdate_812933C(r5 = menu state ptr, r10 = toolkit ptr) */
Next battle_submenuInputUpdate_812933C(void) {
    menu_input_update(R5, R10);
    RETURN_TO_CALLER();
}

/* ── scene_runFadePopTick_8129400 ── */
/* scene_runFadePopTick_8129400: per-frame update for the screen-fade driven pop scene.
 * Inputs:  r5 = scene state struct, r10 = some entity ref, lr = return addr
 * Outputs: r5/r8/r9/r10/r11/r12 preserved with caller-supplied values
 * Clobbers: r0/r1/r2/r3/r4/r6/lr and NZCV
 */
Next scene_runFadePopTick_8129400(void) {
    /* If a screen fade is already running, skip the "pop in" path and just
     * continue driving the scene's normal update tick. */
    GUEST_CALL(IsScreenFadeActive);
    if (ZERO_FLAG() == 0) {
        GUEST_CALL(battle_resetEStructRecords_8046664);
        ST8(R5 + 0xD, 0);
        ST8(R5 + 0x02, 0x0C);
        GUEST_CALL(clearStruct200BC30_803EBAC);

        /* Install the per-scene palette/screen data and bring up the BG. */
        R0 = A_byte_8129168;
        GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);
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
    GUEST_CALL(battle_updateEStructSprites_80465F8);
    R0 = 0x0D;
    GUEST_CALL(submenu_positionOptionSprite);

    RETURN_TO_CALLER();
}

/* ── menu_runScreens3Step_81295D8 ── */
/* menu_runScreens3Step_81295D8: per-frame update of the eStruct200BC30 object.
 * inputs:  r4 r5 r6 r10 lr
 * outputs: r5 r8 r9 r10 r11 r12
 * preserved: r6 r7 r8 r9 */
Next menu_runScreens3Step_81295D8(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    GUEST_CALL(battle_updateEStructSprites_80465F8);

    GUEST_CALL(IsScreenFadeActive);
    if (ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    GUEST_CALL(eStruct200BC30_getJumpOffset00);
    /* original: cmp r0, #0; ble loc_8129620 (branch if r0 <= 0, i.e. N or Z) */
    SET_FLAGS_CMP((s32)R0, 0);
    if (NEGATIVE_FLAG() || ZERO_FLAG()) {
        goto intro;
    }

    /* R0 still holds the positive jump offset; pass it to sub_8049D42 */
    GUEST_CALL(sub_8049D42);
    /* original: bne loc_8129620 (branch if return != 0) */
    if (!ZERO_FLAG()) {
        goto intro;
    }

    {
        /* original: ldrb r1,[r5,#0x1b]; mov r0,#4; tst r0,r1; beq loc_8129610 */
        u32 r1 = LD8(R5 + 0x1B);
        u32 r0 = 4;
        SET_FLAGS_NZ(r0 & r1);
        if (ZERO_FLAG()) {
            /* BEQ taken: bit was clear -> run initSaveStateBlock_8048F9C(1), then set the bit */
            R0 = 1;
            GUEST_CALL(initSaveStateBlock_8048F9C);
            /* original: mov r0,#4; ldrb r1,[r5,#0x1b]; orr r1,r0; strb r1,[r5,#0x1b] */
            r0 = 4;
            r1 = LD8(R5 + 0x1B) | r0;
            SET_FLAGS_NZ(r1);
            ST8(R5 + 0x1B, (u8)r1);
        } else {
            /* bit was set -> clear it, write 4 at r5+1, write 0 at r5+2 */
            /* original: mov r0,#4; ldrb r1,[r5,#0x1b]; bic r1,r0; strb r1,[r5,#0x1b] */
            r0 = 4;
            r1 = LD8(R5 + 0x1B) & ~r0;
            ST8(R5 + 0x1B, (u8)r1);
            /* original: mov r0,#4; strb r0,[r5,#1] */
            r0 = 4;
            ST8(R5 + 1, (u8)r0);
            /* original: mov r0,#0 (sets nz); strb r0,[r5,#2] */
            r0 = 0;
            SET_FLAGS_NZ(r0);
            ST8(R5 + 2, (u8)r0);
        }
    }
    RETURN_TO_CALLER();

intro:
    /* original: bl screens3_loadBackgroundTiles (no R0 set; holds return of the call that led here) */
    GUEST_CALL(screens3_loadBackgroundTiles);
    /* original: ldr r0,=byte_812913C; bl loadEntryPairTable_812B1DC */
    R0 = A_byte_812913C;
    GUEST_CALL(loadEntryPairTable_812B1DC);
    /* original: mov r0,#0xe; bl sub_812B2C4 */
    R0 = 0xE;
    GUEST_CALL(sub_812B2C4);
    /* original: mov r0,#4; strb r0,[r5,#2] */
    R0 = 4;
    ST8(R5 + 2, (u8)R0);
    /* original: mov r0,#0; strb r0,[r5,#3]
     * Important: R0 must be 0 here, because the next instruction is `bl loadEntityJumpOffset00Gfx_812B530`
     * and the original assembles `mov r0,#0` *before* the strb, leaving R0=0
     * for the call. ST8 does not modify guest R0, so we keep R0=0 explicitly. */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 3, (u8)R0);
    GUEST_CALL(loadEntityJumpOffset00Gfx_812B530);
    /* original: mov r0,#1; bl chatbox_runScript_803FD9C_on_eTextScript201BA20 */
    R0 = 1;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);
    /* original: mov r0,#0x40; bl chatbox_set_eFlags2009F38 */
    R0 = 0x40;
    GUEST_CALL(chatbox_set_eFlags2009F38);
    /* original: mov r0,#8; mov r1,#0x10; bl SetScreenFade */
    R0 = 8;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);
    RETURN_TO_CALLER();
}

/* ── sub_8129658 ── */
/* sub_8129658: battle AI state dispatcher.
 *
 * The AI object at R5 keeps its current state id at offset +3. That byte is
 * already scaled by 4 (states are 0x00, 0x04, ... 0x18) and is used directly
 * as the offset into the handler table at off_8129670 — the Thumb
 * `ldr r0, [r0, r1]` does not shift the index register.
 *
 * Per this dispatcher family's convention (ldr r0,[table,state]; mov lr,pc;
 * bx r0), each handler is entered with:
 *   R0 = the handler's own address exactly as stored in the table
 *        (Thumb pointer, e.g. sub_812968C+1),
 *   R1 = the state id,
 * and it returns back into this dispatcher. Afterwards the battle event
 * routine battle_runFrameUpdate runs with event id 0xE in R0.
 *
 * Passes through: in r4 r5 r6 r7 r10, out r5 r8 r9 r10 r11 r12. */
Next sub_8129658(void) {
    u32 state = LD8(R5 + 3);

    /* Load the handler pointer with a byte offset (no <<2) and set up the
     * handler's entry arguments: R0 = own Thumb address, R1 = state id. */
    R0 = LD32(A_off_8129670 + state);
    R1 = state;

    switch (state) {
    case 0x00: GUEST_CALL(sub_812968C); break;
    case 0x04: GUEST_CALL(sub_81296A0); break;
    case 0x08: GUEST_CALL(sub_81296D8); break;
    case 0x0C: GUEST_CALL(battle_object_advanceState3_8129738); break;
    case 0x10: GUEST_CALL(sub_8129758); break;
    case 0x14: GUEST_CALL(sub_812977C); break;
    case 0x18: GUEST_CALL(battle_runSubsystemSlice_81297C0); break;
    }

    R0 = 0xE;
    GUEST_CALL(battle_runFrameUpdate);

    RETURN_TO_CALLER();
}

/* ── sub_812968C ── */
/* sub_812968C(r5 -> some struct):
 * If the screen fade is active, clear chatbox flag 0x40 and store 4 at struct+3.
 * inputs:  r5 lr
 * outputs: -
 * clobbers: nz c v r0 r1 r3 lr
 * preserved: r2 */
Next sub_812968C(void) {
    GUEST_CALL(IsScreenFadeActive);
    if (ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    R0 = 0x40;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 3, (u8)R0);

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

/* ── sub_81296D8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3, r4, r5, r6, r7, r10; } call_menu_moveCursorUpDown_811FA22_result_sub_81296D8;
/* register form: generated wrapper: menu_moveCursorUpDown_811FA22 has no typed C interface yet */
static inline call_menu_moveCursorUpDown_811FA22_result_sub_81296D8 call_menu_moveCursorUpDown_811FA22_sub_81296D8(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    return (call_menu_moveCursorUpDown_811FA22_result_sub_81296D8){ R0, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r2, r3, r5, r6, r7; bool c, v; } call_sub_812B3FC_result;
/* register form: generated wrapper: sub_812B3FC has no typed C interface yet */
static inline call_sub_812B3FC_result call_sub_812B3FC(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(sub_812B3FC);
    return (call_sub_812B3FC_result){ R2, R3, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; bool z, n; } call_IsButtonPressed_result_sub_81296D8;
/* register form: generated wrapper: IsButtonPressed has no typed C interface yet */
static inline call_IsButtonPressed_result_sub_81296D8 call_IsButtonPressed_sub_81296D8(u32 r0) {
    R0 = r0;
    GUEST_CALL(IsButtonPressed);
    return (call_IsButtonPressed_result_sub_81296D8){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_81296D8;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_81296D8 call_PlaySoundEffect_sub_81296D8(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_81296D8){ R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_SetScreenFade_result;
/* register form: generated wrapper: SetScreenFade has no typed C interface yet */
static inline call_SetScreenFade_result call_SetScreenFade(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetScreenFade);
    return (call_SetScreenFade_result){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Handle cursor movement and A/B button presses on a menu sub-page.
 * menu: sub-page descriptor, toolkit: global toolkit pointer. */
static void submenu_cursor_and_buttons(u32 menu, u32 r6, u32 r7, u32 toolkit) {
    u32 joypad = LD32(toolkit + oToolkit_JoypadPtr);
    u32 pressed = LD16(joypad + oJoypad_Pressed);
    u32 cursor = LD8(menu + 0xE);

    call_menu_moveCursorUpDown_811FA22_result_sub_81296D8 mv =
        call_menu_moveCursorUpDown_811FA22_sub_81296D8(pressed, 3, 1,
                                           cursor, cursor, menu, r6, r7);

    if (mv.r0 != cursor) {
        /* Cursor moved: commit new index and redraw the list. */
        ST8(menu + 0xE, mv.r0);
        call_sub_812B3FC(cursor, menu);
        return;
    }

    /* Cursor stayed – check A button to confirm selection. */
    if (!call_IsButtonPressed_sub_81296D8(JOYPAD_A).z) {
        call_PlaySoundEffect_sub_81296D8(SOUND_MENU_SELECT, mv.r2);
        ST8(menu + 3, 0xC);
        ST8(menu + 0x1B, LD8(menu + 0x1B) | 1);
        ST8(menu + 0x1C, 0);
        ST8(menu + 0x1E, 0);
    } else if (!call_IsButtonPressed_sub_81296D8(JOYPAD_B).z) {
        /* B: exit sub-menu with fade-out. */
        call_PlaySoundEffect_sub_81296D8(SOUND_EXIT_SUBMENU, mv.r2);
        ST8(menu + 3, 0x18);
        call_SetScreenFade(0xC, 0x10);
    }
    /* Neither A nor B: nothing to do. */
}

/* sub_81296D8(r5=menu, r6, r7, r10=toolkit) -> r4 r5 r6 r10; r4 r6 r7 r10 preserved */
Next sub_81296D8(void) {
    u32 saved_r4 = R4;
    submenu_cursor_and_buttons(R5, R6, R7, R10);
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── battle_object_advanceState3_8129738 ── */
/* battle_object_advanceState3_8129738(r5 = battle object ptr):
 * If battleObject_advanceChipAnimFrames_812B318(3) succeeds, mark the object's state: store 3 at +0x1D,
 * call sub_81325E4 on its +0xE byte and store the result at +2,
 * then clear +3 to 0. Skips all of that if battleObject_advanceChipAnimFrames_812B318 returns nonzero. */
Next battle_object_advanceState3_8129738(void) {
    /* Try to acquire something via battleObject_advanceChipAnimFrames_812B318(arg=3); nonzero -> bail out. */
    R0 = 3;
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);
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
 * Reads entity field at offset 0x1D and asks battleObject_advanceChipAnimFrames_812B318 what to do with it.
 * If battleObject_advanceChipAnimFrames_812B318 signals a follow-up (r1 != 0), call sub_812B218.
 * If the chosen action is 3, set the entity's state byte to 0x14 and call
 * sprite_setEntityAnimation(0, 0).
 * Outputs: r4 = battleObject_advanceChipAnimFrames_812B318's r2 (the action), r5/r6 preserved. */
Next sub_8129758(void) {
    /* Ask battleObject_advanceChipAnimFrames_812B318 what to do with entity[0x1D]. */
    R0 = (u32)LD8(R5 + 0x1D);
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);
    /* r4 = action returned in r2 */
    R4 = R2;

    /* If battleObject_advanceChipAnimFrames_812B318 set r1, follow up with sub_812B218. */
    if (R1 != 0) {
        GUEST_CALL(sub_812B218);
    }

    /* Action 3: enter state 0x14 and call sprite_setEntityAnimation(0, 0). */
    if (R4 == 3) {
        R0 = 0x14;
        ST8(R5 + 3, (u8)R0);
        R0 = 0;
        R1 = 0;
        GUEST_CALL(sprite_setEntityAnimation);
    }

    RETURN_TO_CALLER();
}

/* ── sub_812977C ── */
/* sub_812977C: scan the 3-slot unk_20251A0 array; if any slot has its type byte non-zero
 * while its flag (bit 31) is clear, return early. Otherwise clear the flag bits of all
 * slots, write 8 to r5->field_3, call sub_812B3FC, and clear bit 0 of r5->field_0x1b.
 *
 * inputs:  r5 r10 lr
 * outputs: r4 r5 r6
 * clobbers: nz c v r0 r1 r2 r3 lr
 * preserved: r7
 */
Next sub_812977C(void) {
    /* r4 is loaded from unk_20251A0 at the start (before any early return), so on
     * either return path r4 = unk_20251A0. r5 is preserved unchanged. r6 ends as 0xC
     * on the fall-through path, or as the breaking loop index on the early return. */
    u32 r5 = R5;
    u32 r6;

    R4 = A_unk_20251A0;

    /* First pass: look for an "in-use" slot (flag bit 31 clear AND type byte at +7
     * non-zero). */
    for (r6 = 0; (s32)r6 < 0xC; r6 += 4) {
        u32 slot = LD32(R4 + r6);
        slot &= ~(1u << 31);
        if (LD8(slot + 7) != 0) {
            /* Early return. */
            R5 = r5;
            R6 = r6;
            RETURN_TO_CALLER();
        }
    }

    /* Second pass: clear the flag bit (bit 31) of every slot. */
    {
        u32 r1 = A_unk_20251A0;
        for (r6 = 0; (s32)r6 < 0xC; r6 += 4) {
            u32 slot = LD32(r1 + r6);
            slot &= ~(1u << 31);
            ST32(r1 + r6, slot);
        }
    }

    ST8(r5 + 3, 8);
    GUEST_CALL(sub_812B3FC);

    {
        u8 flags = LD8(r5 + 0x1b);
        flags &= (u8)~1;
        ST8(r5 + 0x1b, flags);
    }

    R5 = r5;
    R6 = r6;
    RETURN_TO_CALLER();
}

/* ── battle_runSubsystemSlice_81297C0 ── */
/* battle_runSubsystemSlice_81297C0: per-frame "play" / intro / etc. controller
 *
 * Called every tick while a cutscene-like state is active. Runs the entity
 * update pass, runs the queued-effects pass, and if a screen fade is in
 * progress locks the controller's state to 4 (playing) with sub-state 0.
 *
 *   inputs:  r5 = controller state struct ptr, r10 = ?, lr
 *   outputs: r4 r5 r6 r8 r9 r10 r11 r12
 *   clobbers: r0 r1 r2 r3 lr, nz c v */
Next battle_runSubsystemSlice_81297C0(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    GUEST_CALL(battle_updateEStructSprites_80465F8);

    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        /* Fade in progress: force the controller into state 4 / sub 0. */
        ST8(R5 + 1, 4);
        ST8(R5 + 2, 0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_81297DC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool z, n, c, v; } call_sub_812B25C_result_sub_81297DC;
/* register form: generated wrapper: battle_runFrameUpdate has no typed C interface yet */
static inline call_sub_812B25C_result_sub_81297DC call_sub_812B25C_sub_81297DC(u32 r0, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_runFrameUpdate);
    return (call_sub_812B25C_result_sub_81297DC){ R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Dispatch through the battle-state jump table at off_81297F4, then post message 0x12. */

/* register form: indirect call through game function-pointer table */
static void dispatch_jump(u32 target) {
    GUEST_CALL_ADDR(target);
}

/* register form: the jump-table handler is external guest code that communicates via
 * registers; after it returns we must read the mutated R4/R5/R6 to pass into
 * call_sub_812B25C_sub_81297DC, which leaves R5/R8–R12 in place for the caller. */
static void battle_dispatch_and_post(u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    u8 state_byte = LD8(r5 + 3);
    u32 target    = LD32(A_off_81297F4 + state_byte);

    /* Restore the registers the handler expects from the original caller. */
    R4  = r4;
    R5  = r5;
    R6  = r6;
    R7  = r7;
    R10 = r10;
    R1  = state_byte;

    dispatch_jump(target);

    /* Handler returned to the point after bx; post follow-up message 0x12. */
    call_sub_812B25C_sub_81297DC(0x12, R4, R5, R6);
}

/* sub_81297DC(r4, r5, r6, r7, r10) -> r5 r8 r9 r10 r11 r12 */
Next sub_81297DC(void) {
    battle_dispatch_and_post(R4, R5, R6, R7, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8129808 ── */
/* sub_8129808: takes r5 = some battle object pointer (has a byte at +0x1d and +3),
 * reads its state index byte, calls battleObject_advanceChipAnimFrames_812B318, and based on results either
 * does nothing or flips the object's state to 4 and calls sprite_setEntityAnimation(1,0).
 * Preserves r4/r5/r6 for the caller; clobbers r0-r3 and lr internally. */
Next sub_8129808(void) {
    u8 state_index = LD8(R5 + 0x1D);

    R0 = state_index;
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);
    u32 r4_save = R2;          /* battleObject_advanceChipAnimFrames_812B318's result in r2 -> cached in r4 */

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
    GUEST_CALL(sprite_setEntityAnimation);

    RETURN_TO_CALLER();
}

/* ── sub_812982C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6; } call_sub_812B39C_result;
/* register form: generated wrapper: sub_812B39C has no typed C interface yet */
static inline call_sub_812B39C_result call_sub_812B39C(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(sub_812B39C);
    return (call_sub_812B39C_result){ R4, R5, R6 };
}

/*
 * sub_812982C: Scan three battle-slot pointers at unk_20251A0 (stride 4, count 3).
 * Each pointer uses bit 31 as an "active" flag; the low 31 bits are an object address.
 * If any active object has a non-zero byte at offset +7, bail out with R6 = that slot's
 * byte-offset.  Otherwise clear all three active flags, set the stage byte at R5+3 to 8,
 * advance the state machine via sub_812B39C, and clear bit 0 of the byte at R5+0x1B.
 */

#define SLOT_MASK 0x80000000u
#define SLOT_COUNT 3
#define SLOT_STRIDE 4

struct sub_812982C_out {
    u32 r4;
    u32 r5;
    u32 r6;
};

/* register form: sub_812B39C does not modify R6 (pass-through output); the original
 * enters the call with R6 == SLOT_COUNT*SLOT_STRIDE (0xc) from the loop counter.
 * The wrapper sets only R4/R5 from its parameters, so we must restore R6 here. */
static call_sub_812B39C_result call_state_advance(u32 slots, u32 state) {
    R6 = SLOT_COUNT * SLOT_STRIDE;   /* 0xc – loop-end value */
    return call_sub_812B39C(slots, state);
}

static struct sub_812982C_out sub_812982C_impl(u32 battle_state) {
    u32 slots_base = A_unk_20251A0;

    /* Pass 1: bail if any slot's object has a non-zero byte at +7. */
    for (u32 off = 0; off < SLOT_COUNT * SLOT_STRIDE; off += SLOT_STRIDE) {
        u32 ptr = LD32(slots_base + off) & ~SLOT_MASK;
        if (LD8(ptr + 7) != 0) {
            struct sub_812982C_out early = { slots_base, battle_state, off };
            return early;
        }
    }

    /* Pass 2: clear the active flag (bit 31) on all three slot pointers. */
    for (u32 off = 0; off < SLOT_COUNT * SLOT_STRIDE; off += SLOT_STRIDE) {
        ST32(slots_base + off, LD32(slots_base + off) & ~SLOT_MASK);
    }

    /* Set stage byte: battle_state+3 = 8. */
    ST8(battle_state + 3, 8);

    /* Advance the state machine (reads R4=slots, R5=battle_state, R10; outputs R4,R5,R6). */
    call_sub_812B39C_result cr = call_state_advance(slots_base, battle_state);

    /* Clear bit 0 of the active-state flag at battle_state+0x1B. */
    ST8(battle_state + 0x1B, LD8(battle_state + 0x1B) & ~1u);

    struct sub_812982C_out result = { cr.r4, cr.r5, cr.r6 };
    return result;
}

/* sub_812982C(r5 = battle-state ptr, r10, lr) -> r4 r5 r6 */
Next sub_812982C(void) {
    struct sub_812982C_out out = sub_812982C_impl(R5);
    R4 = out.r4;
    R5 = out.r5;
    R6 = out.r6;
    RETURN_TO_CALLER();
}

/* ── sub_8129870 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3, r4, r5, r6, r7, r10; } call_menu_moveCursorUpDown_811FA22_result_sub_8129870;
/* register form: generated wrapper: menu_moveCursorUpDown_811FA22 has no typed C interface yet */
static inline call_menu_moveCursorUpDown_811FA22_result_sub_8129870 call_menu_moveCursorUpDown_811FA22_sub_8129870(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    return (call_menu_moveCursorUpDown_811FA22_result_sub_8129870){ R0, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r4, r5, r6; } call_sub_812B39C_result_sub_8129870;
/* register form: generated wrapper: sub_812B39C has no typed C interface yet */
static inline call_sub_812B39C_result_sub_8129870 call_sub_812B39C_sub_8129870(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(sub_812B39C);
    return (call_sub_812B39C_result_sub_8129870){ R4, R5, R6 };
}

typedef struct { u32 r1; bool z, n; } call_IsButtonPressed_result_sub_8129870;
/* register form: generated wrapper: IsButtonPressed has no typed C interface yet */
static inline call_IsButtonPressed_result_sub_8129870 call_IsButtonPressed_sub_8129870(u32 r0) {
    R0 = r0;
    GUEST_CALL(IsButtonPressed);
    return (call_IsButtonPressed_result_sub_8129870){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_8129870;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_8129870 call_PlaySoundEffect_sub_8129870(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_8129870){ R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Handle D-pad cursor movement and A/B confirm/cancel for a 3-item vertical menu.
 * On cursor change, save the new index and notify the menu via sub_812B39C.
 * On A press, play the select sound and advance to the "selected" sub-state.
 * On B press, play the exit sound and advance to the "cancel" sub-state. */
static void menu_vpad_handler(u32 r5, u32 r6, u32 r7, u32 r10) {
    u16 pressed = LD16(LD32(r10 + oToolkit_JoypadPtr) + oJoypad_Pressed);
    u8 old_index = LD8(r5 + 0x12);

    /* Cursor helper expects all eight input registers matching the original. */
    call_menu_moveCursorUpDown_811FA22_result_sub_8129870 cur =
        call_menu_moveCursorUpDown_811FA22_sub_8129870(pressed, 3, 1,
                                          (u32)old_index, (u32)old_index, r5, r6, r7);

    if (cur.r4 != cur.r0) {
        /* Selection moved: persist the new index and redraw. */
        ST8(r5 + 0x12, cur.r0);
        call_sub_812B39C_sub_8129870(cur.r4, r5);
    } else if (!call_IsButtonPressed_sub_8129870(JOYPAD_A).z) {
        /* A button: confirm current selection. */
        call_PlaySoundEffect_sub_8129870(SOUND_MENU_SELECT, cur.r2);
        ST8(r5 + 3, 0xC);
        ST8(r5 + 0x1C, 0);
        ST8(r5 + 0x1E, 0);
        ST8(r5 + 0x1B, LD8(r5 + 0x1B) | 1);
    } else if (!call_IsButtonPressed_sub_8129870(JOYPAD_B).z) {
        /* B button: cancel / go back. */
        call_PlaySoundEffect_sub_8129870(SOUND_EXIT_SUBMENU, cur.r2);
        ST8(r5 + 3, 0x10);
        ST8(r5 + 0x1C, 0);
        ST8(r5 + 0x1E, 0);
        ST8(r5 + 0x1B, LD8(r5 + 0x1B) | 1);
    }
}

/* sub_8129870(r5 = menu state ptr, r6, r7, r10 = toolkit ptr) -> r4 r5 r6 r10 */
Next sub_8129870(void) {
    menu_vpad_handler(R5, R6, R7, R10);
    RETURN_TO_CALLER();
}

/* ── battleObj_setCanceledPose_81298D4 ── */
/* battleObj_setCanceledPose_81298D4(r5 = object):
 *   Initialize a battle object to the "battle-canceled / show-fingers / etc."
 *   state if preconditions are met, then return.
 *
 *   Inputs:  r5 (object pointer, caller-saved)
 *   Outputs: r4, r5, r6 (preserved across the call)
 *   Clobbers: r0, r1, r2, r3, lr, nz, c, v */
Next battleObj_setCanceledPose_81298D4(void) {
    /* First, ask the toolkit/engine whether this update path is even
     * permitted right now (mode 3 = whatever the bsy check is). */
    R0 = 3;
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);
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
 * battleObject_advanceChipAnimFrames_812B318 must allow it (r0 != 0 -> skip); on success set r5[0x1D]=3, r5[2]=4,
 * r5[3]=0x10, r5[0x12]=r5[0x13]=0. */
Next sub_81298F8(void) {
    R0 = 3;
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);
    if (R0 != 0) {
        /* battleObject_advanceChipAnimFrames_812B318 refused; leave the object untouched. */
    } else {
        ST8(R5 + 0x1D, 3);
        ST8(R5 + 2,    4);
        ST8(R5 + 3,    0x10);
        ST8(R5 + 0x12, 0);
        ST8(R5 + 0x13, 0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8129918 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool z, n, c, v; } call_sub_812B25C_result;
/* register form: generated wrapper: battle_runFrameUpdate has no typed C interface yet */
static inline call_sub_812B25C_result call_sub_812B25C(u32 r0, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_runFrameUpdate);
    return (call_sub_812B25C_result){ R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * Text-box command step (sub_8129918).
 *
 * Byte 3 of the object at r5 selects the next text-command handler out of the
 * jump table at off_8129930. The byte already holds the table's byte offset
 * (0x00, 0x04, 0x08, 0x0C, 0x10, 0x14 -> sub_8129948, sub_812996C, sub_81299B0,
 * sub_8129A20, battle_dealChipsOnFadeIn, sub_8129A54), so it indexes the table directly.
 *
 * The original dispatches with `mov lr, pc; bx r0`, so the handler starts with
 * r0 = its own table entry, r1 = the state byte, and every other register
 * (r2-r7, r10-r12, SP) exactly as the caller of sub_8129918 left them. When it
 * returns, the common follow-up battle_runFrameUpdate runs with r0 = 0x12 and whatever
 * r4/r5/r6 the handler left behind; battle_runFrameUpdate's results (r5, r8-r12) are
 * this function's results.
 */

/* The dispatched handler may change r4/r5/r6, and the follow-up call to
 * battle_runFrameUpdate must see the changed values, so the dispatch helper reads them
 * back out of the guest registers and hands them to the typed code. */
typedef struct {
    u32 r4, r5, r6;
} handler_regs;

/* register form: the dispatch is a call through a jump-table address
 * (mov lr, pc; bx r0), which no generated wrapper covers. */
static handler_regs call_text_handler(u32 state) {
    u32 handler = LD32(A_off_8129930 + state);

    R0 = handler;   /* the handler runs with r0 = its own table entry */
    R1 = state;     /* ...and r1 = the raw state byte */
    GUEST_CALL_ADDR(handler);

    handler_regs after;
    after.r4 = R4;
    after.r5 = R5;
    after.r6 = R6;
    return after;
}

typedef struct {
    u32 r5, r8, r9, r10, r11, r12;
} text_step_result;

static text_step_result sub_8129918_body(u32 obj) {
    u32 state = LD8(obj + 3);
    handler_regs h = call_text_handler(state);

    /* r1/r2/r3/r7/r8/r9/r11/r12 and r10 reach battle_runFrameUpdate exactly as the
     * dispatched handler left them: the wrapper only loads its parameter
     * registers (r0, r4, r5, r6) and passes the live guest registers through. */
    call_sub_812B25C_result follow = call_sub_812B25C(0x12, h.r4, h.r5, h.r6);

    text_step_result out;
    out.r5 = follow.r5;
    out.r8 = follow.r8;
    out.r9 = follow.r9;
    out.r10 = follow.r10;
    out.r11 = follow.r11;
    out.r12 = follow.r12;
    return out;
}

Next sub_8129918(void) {
    text_step_result out = sub_8129918_body(R5);
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── sub_8129948 ── */
/* sub_8129948(r5 = some struct):
 * Reads the byte at r5+0x1D and passes it to battleObject_advanceChipAnimFrames_812B318.
 * Saves battleObject_advanceChipAnimFrames_812B318's r2 in r4. If its r1 is nonzero, also calls sub_812B218.
 * If r4 == 3, sets state byte at r5+3 to 4 and calls sprite_setEntityAnimation(3, 0). */
Next sub_8129948(void) {
    u8 slot = LD8(R5 + 0x1D);

    R0 = slot;
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);

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
        GUEST_CALL(sprite_setEntityAnimation);
    }

    RETURN_TO_CALLER();
}

/* ── sub_812996C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6; } call_sub_812B3CC_result;
/* register form: generated wrapper: sub_812B3CC has no typed C interface yet */
static inline call_sub_812B3CC_result call_sub_812B3CC(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(sub_812B3CC);
    return (call_sub_812B3CC_result){ R4, R5, R6 };
}

/* Walk the 3-slot pointer table at unk_20251A0. If every slot's
 * bit-31-stripped pointer has a zero byte at offset 7, clear bit 31
 * in-place, flag r5+3, run sub_812B3CC, and clear bit 0 of r5+0x1B.
 * Otherwise bail out early.
 * Returns r4, r5, r6 as left by the function (r6 = loop offset on
 * early exit; 0xC if it went through the second loop and sub_812B3CC
 * is called – matching the original register state). */
typedef struct { u32 r4, r5, r6; } TableWalkResult;

static TableWalkResult walk_pointer_table(u32 obj) {
    const u32 table = A_unk_20251A0;
    const u32 mask = 0x80000000;

    /* Pass 1: for each slot, clear bit 31 of the pointer and test byte +7. */
    for (u32 off = 0; off < 0xC; off += 4) {
        u32 ptr = LD32(table + off) & ~mask;
        if (LD8(ptr + 7) != 0) {
            return (TableWalkResult){ table, obj, off };
        }
    }

    /* Pass 2: strip bit 31 from every slot in place. */
    for (u32 off = 0; off < 0xC; off += 4) {
        ST32(table + off, LD32(table + off) & ~mask);
    }

    /* Mark the object's byte 3 as 8 (state indicator). */
    ST8(obj + 3, 8);

    /* Hand off to the battle helper; it reads r4, r5, r10. */
    call_sub_812B3CC(table, obj);

    /* Clear bit 0 of the status byte at r5+0x1B (using input r5, matching
     * the original register state after the bl). */
    ST8(obj + 0x1B, LD8(obj + 0x1B) & ~1);

    /* Return r4=table, r5=obj, r6=0xC (loop index from pass 2). */
    return (TableWalkResult){ table, obj, 0xC };
}

/* sub_812996C(r5, r10) -> r4, r5, r6 */
Next sub_812996C(void) {
    TableWalkResult out = walk_pointer_table(R5);
    R4 = out.r4;
    R5 = out.r5;
    R6 = out.r6;
    RETURN_TO_CALLER();
}

/* ── sub_81299B0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3, r4, r5, r6, r7, r10; } call_menu_moveCursorUpDown_811FA22_result_sub_81299B0;
/* register form: generated wrapper: menu_moveCursorUpDown_811FA22 has no typed C interface yet */
static inline call_menu_moveCursorUpDown_811FA22_result_sub_81299B0 call_menu_moveCursorUpDown_811FA22_sub_81299B0(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    return (call_menu_moveCursorUpDown_811FA22_result_sub_81299B0){ R0, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r4, r5, r6; } call_sub_812B3CC_result_sub_81299B0;
/* register form: generated wrapper: sub_812B3CC has no typed C interface yet */
static inline call_sub_812B3CC_result_sub_81299B0 call_sub_812B3CC_sub_81299B0(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(sub_812B3CC);
    return (call_sub_812B3CC_result_sub_81299B0){ R4, R5, R6 };
}

typedef struct { u32 r1; bool z, n; } call_IsButtonPressed_result_sub_81299B0;
/* register form: generated wrapper: IsButtonPressed has no typed C interface yet */
static inline call_IsButtonPressed_result_sub_81299B0 call_IsButtonPressed_sub_81299B0(u32 r0) {
    R0 = r0;
    GUEST_CALL(IsButtonPressed);
    return (call_IsButtonPressed_result_sub_81299B0){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_81299B0;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_81299B0 call_PlaySoundEffect_sub_81299B0(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_81299B0){ R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { bool z, n; } call_TestEventFlagFromImmediate_result_sub_81299B0;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_sub_81299B0 call_TestEventFlagFromImmediate_sub_81299B0(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_sub_81299B0){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_chatbox_runScript_803FD9C_on_eTextScript201BA20_result;
/* register form: generated wrapper: chatbox_runScript_803FD9C_on_eTextScript201BA20 has no typed C interface yet */
static inline call_chatbox_runScript_803FD9C_on_eTextScript201BA20_result call_chatbox_runScript_803FD9C_on_eTextScript201BA20(u32 r0, u32 r4, u32 r5) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);
    return (call_chatbox_runScript_803FD9C_on_eTextScript201BA20_result){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r4, r5, r6; } call_sub_812B414_result_sub_81299B0;
/* register form: generated wrapper: battle_selectTarget_812B414 has no typed C interface yet */
static inline call_sub_812B414_result_sub_81299B0 call_sub_812B414_sub_81299B0(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(battle_selectTarget_812B414);
    return (call_sub_812B414_result_sub_81299B0){ R4, R5, R6 };
}

#define JOYPAD_A_sub_81299B0  1
#define JOYPAD_B_sub_81299B0  2
#define EVENT_F2  0xF2

/* Handle menu cursor in the battle submenu.
 * r5 = state, r6/r7 = passthrough, r10 = toolkit.
 * Outputs r4/r5/r6/r10 are set as side-effects of the callee wrappers. */
static void menu_cursor_handler(u32 input_r5, u32 input_r6, u32 input_r7, u32 input_r10) {
    /* Read joypad, ask the generic cursor mover to advance through 3 items. */
    u32 joypad = LD16(LD32(input_r10 + oToolkit_JoypadPtr) + oJoypad_Pressed);
    u32 cursor_index = LD8(input_r5 + 0x12);

    call_menu_moveCursorUpDown_811FA22_result_sub_81299B0 cur = call_menu_moveCursorUpDown_811FA22_sub_81299B0(
        joypad, 3, 1, cursor_index, cursor_index, input_r5, input_r6, input_r7);

    if (cur.r4 != cur.r0) {
        /* Cursor moved – store new index and refresh the submenu display. */
        ST8(cur.r5 + 0x12, cur.r0);
        call_sub_812B3CC_sub_81299B0(cur.r4, cur.r5);
        return;
    }

    /* Cursor unchanged – check A (confirm). */
    if (!call_IsButtonPressed_sub_81299B0(JOYPAD_A_sub_81299B0).z) {
        call_PlaySoundEffect_result_sub_81299B0 snd = call_PlaySoundEffect_sub_81299B0(SOUND_MENU_SELECT, cur.r2);

        /* Item 1 with event F2 set → run special chatbox; otherwise confirm normally. */
        if (LD8(snd.r5 + 0x12) == 1 && call_TestEventFlagFromImmediate_sub_81299B0(0, EVENT_F2).z) {
            call_chatbox_runScript_803FD9C_on_eTextScript201BA20(5, snd.r4, snd.r5);
        } else {
            call_sub_812B414_sub_81299B0(snd.r4, snd.r5);
        }
        return;
    }

    /* A not pressed – check B (back). */
    if (call_IsButtonPressed_sub_81299B0(JOYPAD_B_sub_81299B0).z) {
        return;
    }

    /* B pressed – exit the submenu. */
    call_PlaySoundEffect_result_sub_81299B0 snd = call_PlaySoundEffect_sub_81299B0(SOUND_EXIT_SUBMENU, cur.r2);
    ST8(snd.r5 + 3,    0x0C);
    ST8(snd.r5 + 0x1C, 0);
    ST8(snd.r5 + 0x1E, 0);
    ST8(snd.r5 + 0x1B, LD8(snd.r5 + 0x1B) | 1);
}

/* sub_81299B0(r5 r6 r7 r10) -> r4 r5 r6 r10 */
Next sub_81299B0(void) {
    menu_cursor_handler(R5, R6, R7, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8129A20 ── */
Next sub_8129A20(void) {
    /* Tail-call wrapper: forwards to sub_81298F8 with the same
     * preserved-register contract (r4/r5/r6/r7), so the call-and-return
     * is equivalent to invoking the callee directly. */
    GUEST_CALL(sub_81298F8);
    RETURN_TO_CALLER();
}

/* ── sub_8129A98 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool z, n, c, v; } call_sub_812B25C_result_sub_8129A98;
/* register form: generated wrapper: battle_runFrameUpdate has no typed C interface yet */
static inline call_sub_812B25C_result_sub_8129A98 call_sub_812B25C_sub_8129A98(u32 r0, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_runFrameUpdate);
    return (call_sub_812B25C_result_sub_8129A98){ R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* ------------------------------------------------------------------ */
/* sub_8129A98 — one step of the opponent's battle-menu state machine. */
/*                                                                     */
/* The battle object's phase byte (obj+3) selects one of six AI step   */
/* routines from the dispatch table at off_8129AB0.  The chosen routine */
/* runs with the caller's context in r4-r7/r10 and may update r4/r5    */
/* (e.g. sub_8129B34 hands back a new object pointer and state).  The  */
/* shared follow-up battle_runFrameUpdate is then called with command 0x12 and   */
/* those updated registers; its results are what the callers observe.  */
/* ------------------------------------------------------------------ */

/* Dispatch table: phase byte -> Thumb routine address (bit 0 set).
 * 0 decideBattleAction_8129AC8, 1 chatbox_runScriptIfSlotsFree, 2 sub_8129B34,
 * 3 chipSelectInit_8129B90,     4 sub_8129BB0, 5 sub_8129BDC. */
#define AI_STEP_TABLE A_off_8129AB0

/* r4/r5/r6 as a phase handler leaves them for the follow-up call. */
typedef struct { u32 r4, r5, r6; } PhaseHandlerResult;

/* Runs the AI step routine selected by the phase byte.
 * The original reaches it with `mov lr, pc; bx r0`, so the guest registers
 * must hold exactly what the original holds at that branch: r0 = handler
 * address, r1 = phase byte, r2/r3 left as the caller of sub_8129A98 had
 * them, and the context in r4-r7/r10.  Returns the handler's r4/r5/r6,
 * which the follow-up call must see.
 * register form: indirect call through the jump table at off_8129AB0. */
static PhaseHandlerResult run_phase_handler(u32 handler, u32 phase,
                                            u32 r4, u32 r5, u32 r6,
                                            u32 r7, u32 r10) {
    R0 = handler;
    R1 = phase;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R10 = r10;
    GUEST_CALL_ADDR(handler);
    PhaseHandlerResult left = { R4, R5, R6 };
    return left;
}

/* Result registers the callers of sub_8129A98 observe. */
typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
} AiStepResult;

/* One full AI-menu step: dispatch on the phase byte, then the shared
 * follow-up call with command 0x12 on whatever context the handler left. */
static AiStepResult battle_ai_step(u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    AiStepResult out;

    u32 phase = LD8(r5 + 3);
    u32 handler = LD32(AI_STEP_TABLE + phase);
    PhaseHandlerResult left = run_phase_handler(handler, phase,
                                                r4, r5, r6, r7, r10);

    /* battle_runFrameUpdate also reads r10 as it stands after the handler, exactly
     * like the original's bl. */
    call_sub_812B25C_result_sub_8129A98 after = call_sub_812B25C_sub_8129A98(0x12, left.r4, left.r5, left.r6);

    out.r5  = after.r5;
    out.r8  = after.r8;
    out.r9  = after.r9;
    out.r10 = after.r10;
    out.r11 = after.r11;
    out.r12 = after.r12;
    return out;
}

/* sub_8129A98(r4, r5, r6, r7, r10) -> r5 r8 r9 r10 r11 r12 */
Next sub_8129A98(void) {
    AiStepResult out = battle_ai_step(R4, R5, R6, R7, R10);
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── decideBattleAction_8129AC8 ── */
/* decideBattleAction_8129AC8(r5 = entity, lr = return):
 *   Picks a battle-side action for the entity at r5: asks battleObject_advanceChipAnimFrames_812B318 what
 *   kind to do (r2 = kind, r1 = valid). If valid and kind == 2 we mark the
 *   entity's action byte (r5+3 = 4) and trigger the move event (sprite_setEntityAnimation);
 *   otherwise we delegate to battleObject_advanceChipAnimFrames_812B318's sibling sub_812B218.
 *   outputs: r4 r5 r6  preserved: r4 r5 r6 r7 */
Next decideBattleAction_8129AC8(void) {
    /* Ask the battle-side helper which action this entity should take. */
    R0 = LD8(R5 + 0x1D);
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);
    /* r1 = valid, r2 = kind. Match the original: r4 picks up r2, then we
     * test r1 with TST/SUBS so the Z flag reflects it. */
    u32 kind = R2;

    if (R1 == 0)
        RETURN_TO_CALLER();

    if (kind == 2) {
        ST8(R5 + 3, 4);
        R0 = 2;
        R1 = 0;
        GUEST_CALL(sprite_setEntityAnimation);
    } else {
        GUEST_CALL(sub_812B218);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8129B34 ── */
/* sub_8129B34: folder-style menu tick.
 *   inputs:  r5 = menu state struct, r6 = ?, r7 = ?, r10 = toolkit
 *   outputs: r4 clobbered, r5/r6/r7/r10 preserved
 *   Clobbers r0-r3, lr and nz/c/v. */
Next sub_8129B34(void) {
    /* While the screen is fading, do nothing. */
    GUEST_CALL(IsScreenFadeActive);
    if (ZERO_FLAG())
        RETURN_TO_CALLER();

    /* Joypad-driven cursor movement (up/down). */
    u32 joypadPtr = LD32(R10 + oToolkit_JoypadPtr);
    u32 pressed   = LD16(joypadPtr + oJoypad_Pressed);
    u8  oldIndex  = LD8(R5 + 0x12);

    R0 = pressed;
    R1 = 2;
    R2 = 1;
    R3 = oldIndex;
    R4 = oldIndex;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    SET_FLAGS_CMP(R4, R0);

    if (!ZERO_FLAG()) {
        /* Cursor moved: commit new index and refresh. */
        ST8(R5 + 0x12, (u8)R0);
        GUEST_CALL(sub_812B3B4);
        RETURN_TO_CALLER();
    }

    /* A button: select. */
    R0 = JOYPAD_A;
    GUEST_CALL(IsButtonPressed);
    if (!ZERO_FLAG()) {
        R0 = SOUND_MENU_SELECT;
        GUEST_CALL(PlaySoundEffect);
        GUEST_CALL(battle_selectTarget_812B414);
        RETURN_TO_CALLER();
    }

    /* B button: leave the submenu. */
    R0 = JOYPAD_B;
    GUEST_CALL(IsButtonPressed);
    if (ZERO_FLAG())
        RETURN_TO_CALLER();

    R0 = SOUND_EXIT_SUBMENU;
    GUEST_CALL(PlaySoundEffect);
    ST8(R5 + 0x03, 0x0C);            /* set menu state to "exit" */
    ST8(R5 + 0x1C, 0);
    ST8(R5 + 0x1E, 0);
    u8 flags = LD8(R5 + 0x1B);
    flags |= 0x01;
    SET_FLAGS_NZ(flags);
    ST8(R5 + 0x1B, flags);
    RETURN_TO_CALLER();
}

/* ── chipSelectInit_8129B90 ── */
/* chipSelectInit_8129B90(r5 = control struct pointer):
 *   Ask battleObject_advanceChipAnimFrames_812B318 for state 2; if granted, initialise the chip-selection
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
Next chipSelectInit_8129B90(void) {
    /* battleObject_advanceChipAnimFrames_812B318(r0 = 2): request chip-use state 2. */
    R0 = 2;
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);

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

/* ── sub_8129C28 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool z, n, c, v; } call_sub_812B25C_result_sub_8129C28;
/* register form: generated wrapper: battle_runFrameUpdate has no typed C interface yet */
static inline call_sub_812B25C_result_sub_8129C28 call_sub_812B25C_sub_8129C28(u32 r0, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_runFrameUpdate);
    return (call_sub_812B25C_result_sub_8129C28){ R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_8129C28 — one step of a state machine.
 * The state byte at [r5+3] selects a handler from the table at off_8129C40.
 * The handler runs with r0 = its own (Thumb) address and r1 = the state index.
 * When it returns, battle_runFrameUpdate(0x13) is called with the handler's leftover
 * r4/r5/r6, and that call's register results are the results of the step. */

/* The dispatch handlers' leftover r4/r5/r6, which feed the follow-up call. */
typedef struct {
    u32 r4;
    u32 r5;
    u32 r6;
} HandlerRegs;

/* register form: indirect call through a code address (mov lr, pc; bx r0);
 * the handler addresses only exist in the table at off_8129C40, so no
 * generated wrapper covers them. Runs the Thumb handler at `handler` and
 * reads back the guest r4/r5/r6 it leaves behind. */
static HandlerRegs run_state_handler_sub_8129C28(u32 handler, u32 index) {
    HandlerRegs left;
    R0 = handler;
    R1 = index;
    GUEST_CALL_ADDR(handler);
    left.r4 = R4;
    left.r5 = R5;
    left.r6 = R6;
    return left;
}

/* Output registers of the whole step that callers observe. */
typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
} StepResult;

/* Typed body of sub_8129C28. r7 and r10 are only listed so their entry
 * values stay live: this function never touches the guest registers it
 * does not write, so they flow through untouched (r10 reaches
 * battle_runFrameUpdate unchanged, as the original requires). */
static StepResult sub_8129C28_step(u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    StepResult out;

    /* Dispatch on the current state. */
    u32 index = LD8(r5 + 3);
    u32 handler = LD32(A_off_8129C40 + index);
    HandlerRegs left = run_state_handler_sub_8129C28(handler, index);

    /* Tail work: battle_runFrameUpdate(0x13) with the handler's register state. */
    call_sub_812B25C_result_sub_8129C28 tail =
        call_sub_812B25C_sub_8129C28(0x13, left.r4, left.r5, left.r6);

    out.r5 = tail.r5;
    out.r8 = tail.r8;
    out.r9 = tail.r9;
    out.r10 = tail.r10;
    out.r11 = tail.r11;
    out.r12 = tail.r12;
    return out;
}

Next sub_8129C28(void) {
    StepResult out = sub_8129C28_step(R4, R5, R6, R7, R10);
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── sub_8129C74 ── */
/* sub_8129C74(r5 = battle object):
 * Reconciles the object against a lookup done by battleObject_advanceChipAnimFrames_812B318:
 *  - battleObject_advanceChipAnimFrames_812B318 is called with the byte at +0x1D and reports back a flag in r1
 *    and a value in r2 (its r0 result is discarded);
 *  - if the object's state byte at +0x20 is 2 and the flag is 0, there is
 *    nothing to do;
 *  - if the reported value differs from the state byte, sub_812B218 fixes the
 *    object up instead;
 *  - otherwise store 4 at +0x03 and call sprite_setEntityAnimation with a selector based on
 *    the id at +0x02 (0x14 -> 4, 0x18 -> 5, anything else -> 7) and r1 = 0.
 * The original pushes r4 only to use it as scratch for the compare and pops the
 * saved value back on return, so r4 leaves this function holding its incoming
 * value; r5/r6/r7 are untouched throughout. */
Next sub_8129C74(void) {
    u32 savedR4 = R4;
    u32 self = R5;

    R0 = LD8(self + 0x1D);
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);
    u32 flag = R1;
    u32 value = R2;

    R3 = 0x20;
    R0 = LD8(self + 0x20);
    u32 state = R0;

    if (state == 2 && flag == 0) {
        R4 = savedR4;
        RETURN_TO_CALLER();
    }

    R4 = value;
    if (value != state) {
        GUEST_CALL(sub_812B218);
        R4 = savedR4;
        RETURN_TO_CALLER();
    }

    ST8(self + 3, 4);

    u32 id = LD8(self + 2);
    u32 selector = 7;
    if (id == 0x14)
        selector = 4;
    else if (id == 0x18)
        selector = 5;

    R0 = selector;
    R1 = 0;
    GUEST_CALL(sprite_setEntityAnimation);
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_8129D00 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3, r4, r5, r6, r7, r10; } call_menu_moveCursorUpDown_811FA22_result_sub_8129D00;
/* register form: generated wrapper: menu_moveCursorUpDown_811FA22 has no typed C interface yet */
static inline call_menu_moveCursorUpDown_811FA22_result_sub_8129D00 call_menu_moveCursorUpDown_811FA22_sub_8129D00(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    return (call_menu_moveCursorUpDown_811FA22_result_sub_8129D00){ R0, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7; bool c, v; } call_sub_812B3E4_result;
/* register form: generated wrapper: sub_812B3E4 has no typed C interface yet */
static inline call_sub_812B3E4_result call_sub_812B3E4(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(sub_812B3E4);
    return (call_sub_812B3E4_result){ R2, R3, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; bool z, n; } call_IsButtonPressed_result_sub_8129D00;
/* register form: generated wrapper: IsButtonPressed has no typed C interface yet */
static inline call_IsButtonPressed_result_sub_8129D00 call_IsButtonPressed_sub_8129D00(u32 r0) {
    R0 = r0;
    GUEST_CALL(IsButtonPressed);
    return (call_IsButtonPressed_result_sub_8129D00){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_8129D00;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_8129D00 call_PlaySoundEffect_sub_8129D00(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_8129D00){ R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r4, r5, r6; } call_sub_812B414_result_sub_8129D00;
/* register form: generated wrapper: battle_selectTarget_812B414 has no typed C interface yet */
static inline call_sub_812B414_result_sub_8129D00 call_sub_812B414_sub_8129D00(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(battle_selectTarget_812B414);
    return (call_sub_812B414_result_sub_8129D00){ R4, R5, R6 };
}

/* sub_8129D00: handle joystick cursor movement and A/B confirm/back for a menu list. */

typedef struct {
    u32 r4, r5, r6, r10;
} sub_8129D00_result;

static sub_8129D00_result menu_handle_list_input(u32 r5, u32 r6, u32 r7, u32 r10) {
    /* Read the joypad pressed-this-frame bitmask via the toolkit pointer. */
    u32 joypad = LD16(LD32(r10 + oToolkit_JoypadPtr) + oJoypad_Pressed);
    /* List item count at byte offset 0x20; current cursor at byte offset 0x13. */
    u32 count  = LD8(r5 + 0x20);
    u32 cursor = LD8(r5 + 0x13);

    /* Move the cursor (wrap enabled in r2).  The callee also returns r10 unchanged. */
    call_menu_moveCursorUpDown_811FA22_result_sub_8129D00 mc =
        call_menu_moveCursorUpDown_811FA22_sub_8129D00(joypad, count, 1, cursor, cursor, r5, r6, r7);
    u32 new_cursor = mc.r0;

    sub_8129D00_result res = { cursor, r5, r6, r10 };

    if (cursor != new_cursor) {
        /* Cursor moved: store new index and refresh whatever depends on it. */
        ST8(r5 + 0x13, new_cursor);
        call_sub_812B3E4_result s = call_sub_812B3E4(cursor, r5);
        res.r4 = s.r4;
        res.r5 = s.r5;
        res.r6 = r6;      /* preserved – callee may have touched it */
        res.r10 = r10;    /* preserved */
    } else if (!call_IsButtonPressed_sub_8129D00(JOYPAD_A).z) {
        /* Confirm: play select sound then let the target-chooser take over. */
        call_PlaySoundEffect_sub_8129D00(SOUND_MENU_SELECT, mc.r2);
        call_sub_812B414_result_sub_8129D00 s = call_sub_812B414_sub_8129D00(cursor, r5);
        res.r4 = s.r4;
        res.r5 = s.r5;
        res.r6 = r6;
        res.r10 = r10;
    } else if (!call_IsButtonPressed_sub_8129D00(JOYPAD_B).z) {
        /* Back out: play exit sound, mark list as closing (offset 3 ← 0x0C),
         * clear two sub-fields at 0x1C/0x1E, and set bit-0 of the flag byte at 0x1B. */
        call_PlaySoundEffect_sub_8129D00(SOUND_EXIT_SUBMENU, mc.r2);
        ST8(r5 + 0x03, 0x0C);
        ST8(r5 + 0x1C, 0);
        ST8(r5 + 0x1E, 0);
        ST8(r5 + 0x1B, LD8(r5 + 0x1B) | 1);
        /* r4 keeps the old cursor value – no callee touched it past this point. */
    }
    /* No button: fall through – all outputs equal the defaults above. */

    return res;
}

/* sub_8129D00(r5 = list base, r6 r7 r10 preserved) -> r4 r5 r6 r10 */
Next sub_8129D00(void) {
    sub_8129D00_result res = menu_handle_list_input(R5, R6, R7, R10);
    R4  = res.r4;
    R5  = res.r5;
    R6  = res.r6;
    R10 = res.r10;
    RETURN_TO_CALLER();
}

/* ── sub_8129D58 ── */
/* sub_8129D58(r5 = object): check the object's state at offset 0x20 via
 * battleObject_advanceChipAnimFrames_812B318; if it reports "not busy" (returns 0), reset the object into
 * its initial state (timer mode 3, timer 8, counters cleared). */
Next sub_8129D58(void) {
    u32 obj = R5;

    R0 = LD8(obj + 0x20);
    GUEST_CALL(battleObject_advanceChipAnimFrames_812B318);

    if (R0 == 0) {
        ST8(obj + 0x1D, 3);
        ST8(obj + 0x02, 8);
        ST8(obj + 0x03, 0);
        ST8(obj + 0x13, 0);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8129F04 ── */
/* sub_8129F04: run one step of an eStruct (200BC30) entity.
 *
 * The entity's step selector is fetched via eStruct200BC30_getJumpOffset00 and
 * used to index a small handler table (off_8129F1C):
 *   entry 0 -> sub_8129F28, entries 1 and 2 -> sub_8129FE0.
 * After the handler returns, battle_stepWaitForStateRunUpdate performs the step's epilogue.
 *
 * All other registers (r2-r6, r8-r12) are pass-through inputs to the handler
 * and its results (r5, r8-r12) flow straight back to the caller. */
Next sub_8129F04(void) {
    /* which step does this entity run next? */
    GUEST_CALL(eStruct200BC30_getJumpOffset00);

    /* dispatch to the selected step handler */
    switch (R0) {
    case 0:
        GUEST_CALL(sub_8129F28);
        break;
    default: /* table entries 1 and 2 both point here */
        GUEST_CALL(sub_8129FE0);
        break;
    }

    /* common epilogue after the step handler */
    GUEST_CALL(battle_stepWaitForStateRunUpdate);

    RETURN_TO_CALLER();
}

/* ── sub_8129F28 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_sprite_drawIfFlagSet_812B504_result;
/* register form: generated wrapper: sprite_drawIfFlagSet_812B504 has no typed C interface yet */
static inline call_sprite_drawIfFlagSet_812B504_result call_sprite_drawIfFlagSet_812B504(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_drawIfFlagSet_812B504);
    return (call_sprite_drawIfFlagSet_812B504_result){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Drawing one battle object (sub_8129F28).
 *
 * The record pointer arrives in r5; the kind byte at record+2 selects one of
 * two draw routines through the two-entry jump table at off_8129F40. That
 * handler does the type-specific work and hands back updated state in
 * r5/r6/r10/r11. Afterwards sprite_drawIfFlagSet_812B504 commits the sprite
 * to the OAM if the record's visibility flag is set; it takes the record in
 * r5, updates r5/r6 again and preserves r10 (and never touches r11). */

/* Register results of a draw step: the outputs of sub_8129F28. */
typedef struct {
    u32 r5, r6, r10, r11;
} DrawState;

/* register form: the draw routine is reached through a jump table
 * (mov lr, pc; bx rN) and is entered with the full live register state —
 * r0 = handler address, r1 = the kind byte, r2..r12 as on entry — while its
 * results come back in r5/r6/r10/r11. No typed signature can express that,
 * so this helper performs the single indirect call itself and returns the
 * registers that matter. */
static DrawState call_draw_handler(u32 r2, u32 r3, u32 r4, u32 r5, u32 r6,
                                   u32 r8, u32 r9, u32 r10, u32 r11, u32 r12)
{
    u32 kind = LD8(r5 + 2);
    u32 handler = LD32(A_off_8129F40 + kind);

    R0 = handler;                 /* the original branches with r0 = handler */
    R1 = kind;
    R2 = r2; R3 = r3; R4 = r4;
    R5 = r5; R6 = r6;
    R8 = r8; R9 = r9; R10 = r10; R11 = r11; R12 = r12;

    GUEST_CALL_ADDR(handler);

    DrawState out = { R5, R6, R10, R11 };
    return out;
}

/* Dispatch on the record's kind byte, then commit the sprite if visible. */
static DrawState draw_object(u32 r2, u32 r3, u32 r4, u32 r5, u32 r6,
                             u32 r8, u32 r9, u32 r10, u32 r11, u32 r12)
{
    DrawState st = call_draw_handler(r2, r3, r4, r5, r6, r8, r9, r10, r11, r12);

    call_sprite_drawIfFlagSet_812B504_result committed =
        call_sprite_drawIfFlagSet_812B504(st.r5);

    st.r5 = committed.r5;
    st.r6 = committed.r6;
    /* r10 survives the sprite call unchanged; r11 is not touched by it. */
    return st;
}

Next sub_8129F28(void) {
    DrawState st = draw_object(R2, R3, R4, R5, R6, R8, R9, R10, R11, R12);
    R5 = st.r5;
    R6 = st.r6;
    R10 = st.r10;
    R11 = st.r11;
    RETURN_TO_CALLER();
}

/* ── sub_8129F48 ── */
/* sub_8129F48(r5 = e-Link work area):
 * Prepare the e-Link hardware/session for a new use. Goes through the CRT0
 * entry trampoline, marks the work area as state 4, resets the link state
 * machine, registers the transfer buffers (0x2000 bytes tagged 0x42523545),
 * runs the init helpers, and finally jumps into the e-Link main routine.
 * r5/r6/r10/r11 are outputs as left by the callees. */
Next sub_8129F48(void) {
    /* enter via the CRT0 trampoline first */
    GUEST_CALL(crt0_entry_trampoline_8000288);

    /* mark the work area: state byte = 4 */
    R0 = 4;
    ST8(R5 + 2, R0);

    GUEST_CALL(eLinkState_ResetForNewUse_803DE88);
    GUEST_CALL(sub_803DEAC);

    /* set up the link transfer: 0x10 units, mode 3/2, index 0 */
    R0 = 0x10;
    R1 = 3;
    R2 = 2;
    R3 = 0;
    GUEST_CALL(sub_803DCE8);

    GUEST_CALL(sub_803EF60);
    GUEST_CALL(sub_803EA78);

    /* attach the receive buffer: tag 0x42523545 ("ERUB"), 0x2000 bytes, base 0 */
    R0 = 0x42523545;
    R1 = 0x2000;
    R2 = 0;
    GUEST_CALL(sub_803DCD8);

    /* hand control to the e-Link main routine */
    GUEST_CALL(sub_81322E8);

    RETURN_TO_CALLER();
}

/* ── sub_8129FE0 ── */
/* sub_8129FE0: dispatch to the current mode handler of the object whose
 * struct pointer arrives in r5.
 *
 * The byte at [r5,#2] is not a plain mode index — the original uses it
 * directly as a *byte* offset into the handler table at 08129FF4
 * ("ldrb r1, [r5,#2]; ldr r0, [r0,r1]"), i.e. the object stores its mode
 * pre-multiplied by 4 (mode 0 -> offset 0, mode 1 -> offset 4, ...).
 * The call is a tail call in disguise: "mov lr, pc; bx r0" makes the
 * handler return to the "pop {pc}" right after the bx, which returns
 * straight to this function's caller.
 *
 * Table contents (byte offset -> handler):
 *   0  -> menu_subMenu_dispatchByIndex_812A008   (mode 0)
 *   4  -> object_timerDialogueUpdate   (mode 1)
 *   8  -> rfu_initLinkSessionState   (mode 2)
 *  12  -> sub_812A14C   (mode 3)
 *  16  -> sub_812A0CC   (mode 4)
 *
 * Inputs r4, r6 and r10 are forwarded to the handlers simply by leaving
 * the guest register file untouched; the handlers' results (r5 r6 r10
 * r11) are likewise left in place. Nothing may touch the registers
 * between the handler returning and the return to our caller.
 */
Next sub_8129FE0(void) {
    switch (LD8(R5 + 2)) {
    case 0:
        GUEST_CALL(menu_subMenu_dispatchByIndex_812A008);
        break;
    case 4:
        GUEST_CALL(object_timerDialogueUpdate);
        break;
    case 8:
        GUEST_CALL(rfu_initLinkSessionState);
        break;
    case 12:
        GUEST_CALL(sub_812A14C);
        break;
    case 16:
        GUEST_CALL(sub_812A0CC);
        break;
    default:
        /* Unreachable in the real game: the original would load a word
         * from the ROM bytes at table+offset — past the table for offsets
         * >= 20, or an address-rotated garbage word for offsets that are
         * not multiples of 4 — and jump to it. */
        break;
    }
    RETURN_TO_CALLER();
}

/* ── menu_subMenu_dispatchByIndex_812A008 ── */
/* menu_subMenu_dispatchByIndex_812A008: redraw the e-Link sprite, run its palette/animation start,
 * then if something was just received (rfu_initLinkSession != 0) reset the e-Link
 * state machine and switch it to state 4 with sub-state 3.
 * r5 points at the e-Link work area; R5 and R6 pass through unchanged. */
Next menu_subMenu_dispatchByIndex_812A008(void) {
    GUEST_CALL(sprite_drawIfFlagSet_812B504);
    GUEST_CALL(crt0_start_800027C);

    GUEST_CALL(rfu_initLinkSession);
    if (R0 != 0) {
        GUEST_CALL(eLinkState_ResetForNewUse_803DE88);
        ST8(R5 + 2, 4);        /* e-Link state = 4 */
        ST16(R5 + 0x28, 3);    /* sub-state = 3 */
    }
    RETURN_TO_CALLER();
}

/* ── pet_tickEntitiesAndExitToMap_812AEC8 ── */
/* register form: calls entity_runAllUpdateHandlers_80465BC outside this file, which has no typed C interface yet */
static void run_entity_updates_sub_812AEC8(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
}

/* register form: calls battle_updateEStructSprites_80465F8 outside this file, which has no typed C interface yet */
static void tick_secondary_handlers(void) {
    GUEST_CALL(battle_updateEStructSprites_80465F8);
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

/* pet_tickEntitiesAndExitToMap_812AEC8() -> void; output registers and flags pass through from callees. */
Next pet_tickEntitiesAndExitToMap_812AEC8(void) {
    process_tick_and_fade_exit();
    RETURN_TO_CALLER();
}

/* ── sub_812AEE0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool z, n, c, v; } call_sub_812B26C_result;
/* register form: generated wrapper: battle_stepWaitForStateRunUpdate has no typed C interface yet */
static inline call_sub_812B26C_result call_sub_812B26C(u32 r5, u32 r6) {
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_stepWaitForStateRunUpdate);
    return (call_sub_812B26C_result){ R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Battle object update dispatcher (sub_812AEE0).
 *
 * r5 points to a battle object; byte 2 of the object selects one update
 * handler from the table at off_812AEF8. The original performs the call
 * with `mov lr, pc; bx r0`, so each handler is entered with r0 = its own
 * table entry (handlers reach the parameter data stored after their code
 * through it) and r1 = the selector byte, while r5/r6/r10 pass through
 * unchanged. When the handler returns, the shared post-update routine
 * battle_stepWaitForStateRunUpdate runs with the r5/r6 the handler left behind; its results
 * in r5 r8 r9 r10 r11 r12 are this function's outputs. */

/* register form: one-shot helper for the dispatch-table call — the
 * handler address is only known at runtime (mov lr, pc; bx r0), so no
 * generated wrapper covers it, and the handler must see r5/r6/r10
 * exactly as they sit in the guest register file. */
static u32 call_handler_from_table(u32 handler, u32 table_entry, u32 selector) {
    R0 = table_entry;
    R1 = selector;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* register form: the handler is selected at runtime from the dispatch
 * table, so which registers it clobbers (notably r5/r6, which the
 * post-update routine then consumes) cannot be expressed in typed C —
 * the state after the indirect call has to be read back from the guest
 * register file, and the final outputs written back there. */
static void update_battle_object(void) {
    u32 selector = LD8(R5 + 2);
    u32 handler  = LD32(A_off_812AEF8 + selector);

    /* mov lr, pc; bx r0: run the selected update handler with
     * r0 = its own table entry, r1 = the selector byte. */
    call_handler_from_table(handler, handler, selector);

    /* Shared tail: the post-update step runs on the object state as the
     * handler left it (r10 passes through the wrapper untouched). */
    call_sub_812B26C_result post = call_sub_812B26C(R5, R6);

    R5  = post.r5;
    R8  = post.r8;
    R9  = post.r9;
    R10 = post.r10;
    R11 = post.r11;
    R12 = post.r12;
}

Next sub_812AEE0(void) {
    /* Everything happens in register form: update_battle_object() runs
     * the dispatched handler plus the shared post-update routine and
     * leaves the outputs (r5 r8 r9 r10 r11 r12) in the guest registers. */
    update_battle_object();
    RETURN_TO_CALLER();
}

/* ── sub_812AEFC ── */
/* sub_812AEFC: chatbox input/mask handling.
 * Calls chatbox_mask_eFlags2009F38 to test two eFlags. If eFlag 0x80 is clear, OR
 * if eFlag 0x8 is set, calls sub_8132614. Otherwise returns without calling it. */
Next sub_812AEFC(void) {
    /* Test eFlag 0x80. */
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (ZERO_FLAG()) {
        /* eFlag 0x80 clear: run sub_8132614. */
        GUEST_CALL(sub_8132614);
    } else {
        /* eFlag 0x80 set: test eFlag 0x8. If set, return; else run sub_8132614. */
        R0 = 8;
        GUEST_CALL(chatbox_mask_eFlags2009F38);
        if (!ZERO_FLAG()) {
            GUEST_CALL(sub_8132614);
        }
    }
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
 * the first two words form one reference that getKeyedTableEntry_811FB84 resolves to a single
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
        GUEST_CALL(getKeyedTableEntry_811FB84);
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
 * its descriptor (off_812AFE4) to the text-script engine (battle_loadTileGridByNavi_8123300).
 * Result registers (r1-r7) are whatever battle_loadTileGridByNavi_8123300 leaves behind. */
Next textScript_loadAndRunEmbeddedArchive_812AF84(void) {
    /* Decompress the packed data blob into work RAM. */
    R0 = A_byte_812B168;
    GUEST_CALL(decompAndCopyData);

    /* Point the text engine at the decompressed script and run it. */
    R0 = A_off_812AFE4;
    R1 = A_eTextScript201BA20;
    R2 = A_eDecompBuffer2013A00;
    GUEST_CALL(battle_loadTileGridByNavi_8123300);

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

/* ── loadEntryPairTable_812B1DC ── */
/* loadEntryPairTable_812B1DC: load a 4-byte entry pair table into a scratch buffer.
 *
 * Walks a small packed array starting at *r0 of records of the form
 * { u8 a, u8 b, u16 c }; the array ends with a sentinel byte 0xFF.
 * Each record is decoded via battle_initEmotionTableEntry_8046670(a, b, c) and the result is
 * stored into the 4-slot scratch buffer at unk_20251A0, indexed by slot
 * number (0..3). The scratch buffer is zeroed up to 0x10 bytes first.
 *
 * inputs:  r0 = pointer to the entry table
 * outputs: none meaningful (r4/r5/r6 left as the index walker state)
 */
Next loadEntryPairTable_812B1DC(void) {
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
    GUEST_CALL(battle_initEmotionTableEntry_8046670);

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
        GUEST_CALL(battle_initEmotionTableEntry_8046670);
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

/* ── sub_812B218 ── */
/* sub_812B218(r5 = record whose byte at +0x1E selects a slot in unk_20251A0):
 * If the u32 entry at unk_20251A0[slot] is still empty, request a new entry
 * from battle_initEmotionTableEntry_8046670 (r0 = 0x1C, r1 = slot + 1, r2 = (slot << 16) | 0x01000001,
 * r3 = 1), tag the result with the high bit, store it into the table and bump
 * the slot counter byte. r4-r7 are saved/restored around the call, and r0
 * ends up holding the new counter value (or the existing entry). */
Next sub_812B218(void) {
    u32 slot = LD8(R5 + 0x1E);
    u32 entry = LD32(A_unk_20251A0 + slot * 4);

    if (entry == 0) {
        R0 = 0x1C;
        R1 = slot + 1;
        R2 = (slot << 16) | 0x01000001;
        R3 = 1;
        GUEST_CALL(battle_initEmotionTableEntry_8046670);
        R0 |= 0x80000000;
        R1 = A_unk_20251A0;
        ST32(A_unk_20251A0 + slot * 4, R0);
        R0 = slot + 1;
        ST8(R5 + 0x1E, R0);
    } else {
        R0 = entry;
    }
    RETURN_TO_CALLER();
}

/* ── battleObject_advanceChipAnimFrames_812B318 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r3, r4, r5; } call_sub_811A768_result;
/* register form: generated wrapper: sub_811A768 has no typed C interface yet */
static inline call_sub_811A768_result call_sub_811A768(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(sub_811A768);
    return (call_sub_811A768_result){ R0, R3, R4, R5 };
}

/* battleObject_advanceChipAnimFrames_812B318: walk the deallocation table unk_20251A0.
 * For each of slot_count entries (indexed 0 … slot_count−1):
 *   slot == 0            → tally as null (null_count++)
 *   bit 31 of slot set   → tally as locked  (high_count++)
 *   bit 31 clear         → call sub_811A768(−0x10, slot & 0x7FFF_FFFF)
 *                          on success zero the slot entry and clear result.
 * A per-level threshold table (byte_812B394) and an 8-bit counter at
 * [entity_base+0x1C] (incremented once before the loop) govern early exit.
 * Returns r0 = 1 (all OK) or 0 (a slot was deallocated),
 *   r1 = null_count, r2 = high_count, r3 = last callee r3. */

typedef struct { u32 r0, r1, r2, r3; } Result_812B318;

static Result_812B318 process_dealloc_slots(u32 slot_count, u32 entity_base, u32 r3_in)
{
    u32 null_count = 0;
    u32 high_count = 0;
    u32 success    = 1;   /* becomes 0 when a slot is actually freed */
    u32 r3         = r3_in;

    /* Bump the per-entity 8-bit counter at base+0x1C once before the loop. */
    ST8(entity_base + 0x1C, (u8)(LD8(entity_base + 0x1C) + 1));

    u32 i = 1;  /* loop index: slot i−1 is checked, threshold at byte_812B394[i] */
    do {
        u32 offset = (i - 1) << 2;
        u32 slot   = LD32(A_unk_20251A0 + offset);

        if (slot == 0) {
            null_count++;
        } else if ((s32)slot < 0) {
            /* Bit 31 set: entry is locked */
            high_count++;
        } else {
            /* Bit 31 clear: try to deallocate via sub_811A768(−0x10, slot) */
            call_sub_811A768_result res = call_sub_811A768(
                0xFFFFFFF0u, slot & 0x7FFFFFFFu);
            r3 = res.r3;
            if (res.r0 == 0) {
                ST32(A_unk_20251A0 + offset, 0);  /* clear the table entry */
                success = 0;
            }
        }

        /* Early-exit when the level threshold reaches the counter.
         * Both values are u8 (0–255), so signed bge == unsigned >= here. */
        if ((s32)LD8(A_byte_812B394 + i) >= (s32)LD8(entity_base + 0x1C))
            break;

        i++;
    } while ((s32)i < (s32)(slot_count + 1u));

    Result_812B318 out = { success, null_count, high_count, r3 };
    return out;
}

/* battleObject_advanceChipAnimFrames_812B318(r0=slot_count, r5=entity_base) → r0,r1,r2,r3 */
Next battleObject_advanceChipAnimFrames_812B318(void) {
    Result_812B318 res = process_dealloc_slots(R0, R5, R3);
    R0 = res.r0;
    R1 = res.r1;
    R2 = res.r2;
    R3 = res.r3;
    SET_FLAGS_ADD(R0, 0);
    RETURN_TO_CALLER();
}

/* ── sub_812B39C ── */
/* sub_812B39C: show a chatbox line for this object's current text variant.
 *   r5 -> object; r10 = caller context (unused here).
 *   Picks one of the eTextScript201BA20 scripts indexed by the object's
 *   byte at offset 0x12, then runs it through the chatbox runner. */
Next sub_812B39C(void) {
    u8 index = LD8(R5 + 0x12);
    u32 script_base = A_dword_812B3B0;
    u8 variant = LD8(script_base + index);

    R0 = variant;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);

    RETURN_TO_CALLER();
}

/* ── sub_812B3B4 ── */
/* sub_812B3B4(r5 = struct*, r4 r10 = ignored, lr = ignored):
 *   Look up a byte from a jump-table-like blob using the low byte of r5->0x12,
 *   then run it as a chatbox script on eTextScript201BA20.
 *   inputs:  r4 r5 r10 lr
 *   outputs: c v r2 r3 r4 r5 r6 lr
 *   clobbers (free for us): nz r0 r1
 *   preserved: r6 r7 */
Next sub_812B3B4(void) {
    /* Jump table: dword_812B3C8 is just the address 0x00000D0C,
     * but the original treats it as a label used by the disassembler. */
    u32 table = A_dword_812B3C8;
    /* Index = low byte of the field at offset 0x12 in the struct r5 points to. */
    u32 index = LD8(R5 + 0x12);
    /* Read the script index from the table, then run it. */
    u32 script = LD8(table + index);
    R0 = script;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);
    RETURN_TO_CALLER();
}

/* ── sub_812B3CC ── */
/* sub_812B3CC(r5 = battle object, [r5+0x12] = message id)
 * Looks up a script index from a small table at dword_812B3E0 and runs
 * the chatbox script dispatcher. */
Next sub_812B3CC(void) {
    u8 msg_id = LD8(R5 + 0x12);
    R0 = msg_id;
    R1 = A_dword_812B3E0;
    R0 = LD8(R1 + R0);
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);
    RETURN_TO_CALLER();
}

/* ── sub_812B3E4 ── */
Next sub_812B3E4(void) {
    /* Fetch a one-byte index from r5+0x13, use it to look up a byte in the
     * table at dword_812B3F8, then feed that byte to the chatbox script
     * runner for eTextScript201BA20. */
    u8 idx = LD8(R5 + 0x13);
    u8 byte = LD8((u32)A_dword_812B3F8 + idx);

    R0 = byte;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);

    RETURN_TO_CALLER();
}

/* ── sub_812B3FC ── */
Next sub_812B3FC(void) {
    /* Read the small index field out of the r5 struct (offset 0xE),
     * use it to pick one byte from the dword_812B410 table, and feed
     * that byte to the chatbox script runner on eTextScript201BA20. */
    u8 index = LD8(R5 + 0xE);
    u8 arg = LD8(A_dword_812B410 + index);

    R0 = arg;
    GUEST_CALL(chatbox_runScript_803FD9C_on_eTextScript201BA20);

    RETURN_TO_CALLER();
}

/* ── battle_selectTarget_812B414 ── */
/* battle_selectTarget_812B414(r5 = battle object, r10 = ?, lr = return address):
 * mark the battle object as the active target, clear its counters, flag it
 * for selection, then trigger chatbox script 0x23.
 * outputs: r4 r5 r6; clobbers r0 r1 r2 r3 lr nz cv. */
Next battle_selectTarget_812B414(void) {
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

/* ── sub_812B430 ── */
/* sub_812B430: Render two title-screen style text GFX panels and copy two
 * tile blocks in the background. The source "panel" pointer arrives in
 * r10 and contains pointers to two sub-structs at offsets 0x38 and 0x40,
 * the latter holding the tile dimensions that get copied onto the former.
 *
 * r4-r7 are callee-scratch here (the assembly pushes/pops them so the
 * helpers can clobber them), so we just save and restore them to preserve
 * the caller's values. */
Next sub_812B430(void) {
    u32 save_r4 = R4;
    u32 save_r5 = R5;
    u32 save_r6 = R6;
    u32 save_r7 = R7;

    /* First text render: eTextScript201BA20 into VRAM 0x06007200,
     * slot 0x20. */
    R0 = A_eTextScript201BA20;
    R1 = 0x20;
    R2 = A_byte_2017A00;
    R3 = 0x06007200;
    R4 = 0xA;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* Copy the source panel's (0x40)->width/height into the dest's
     * (0x38)->width/height slots. */
    {
        u32 src    = LD32(R10 + 0x40);
        u32 width  = LD16(src + 0x8);
        u32 height = LD16(src + 0xA);
        u32 dst    = LD32(R10 + 0x38);
        ST32(dst + 0x8, width);
        ST32(dst + 0xC, height);
    }

    /* Second text render: eTextScript201BA20 into VRAM 0x06007480,
     * slot 0x21. */
    R0 = A_eTextScript201BA20;
    R1 = 0x21;
    R2 = A_unk_2017C80;
    R3 = 0x06007480;
    R4 = 0xA;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* Copy two 2x2-tile blocks of tile-id tables onto the background. */
    R0 = 0x10; R1 = 4; R2 = 2; R3 = A_byte_8129188; R4 = 0xA; R5 = 2;
    GUEST_CALL(CopyBackgroundTiles);

    R0 = 0x11; R1 = 6; R2 = 2; R3 = A_byte_81291B0; R4 = 0xA; R5 = 2;
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore r4-r7 so the caller sees them unchanged. */
    R4 = save_r4;
    R5 = save_r5;
    R6 = save_r6;
    R7 = save_r7;

    RETURN_TO_CALLER();
}

/* ── sub_812B4AC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r6, r7; bool c, v; } call_call_sub_3005EBA_result;
/* register form: generated wrapper: call_sub_3005EBA has no typed C interface yet */
static inline call_call_sub_3005EBA_result call_call_sub_3005EBA(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(call_sub_3005EBA);
    return (call_call_sub_3005EBA_result){ R1, R2, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* The window-enqueue routine trashes r4-r7 even though this function must
 * preserve them, so their entry values round-trip through the adapter. */
struct sub_812B4AC_kept {
    u32 r4, r5, r6, r7;
};

/* Queue the two text windows of the submenu:
 * window 0x10 is 4 columns by 2 rows and window 0x11 is 6 columns by 2 rows,
 * both placed at column 0x0A on row 2. */
static struct sub_812B4AC_kept open_submenu_windows(u32 r4, u32 r5, u32 r6, u32 r7) {
    call_call_sub_3005EBA(0x10, 4, 2, 0, 0xA, 2);
    call_call_sub_3005EBA(0x11, 6, 2, 0, 0xA, 2);
    struct sub_812B4AC_kept kept = { r4, r5, r6, r7 };
    return kept;
}

/* sub_812B4AC: open the submenu's two windows, then return to the caller.
 * r4-r7 are preserved by the original (it saves and restores them around the
 * calls), so the adapter hands them in and writes the unchanged values back. */
Next sub_812B4AC(void) {
    struct sub_812B4AC_kept kept = open_submenu_windows(R4, R5, R6, R7);
    R4 = kept.r4;
    R5 = kept.r5;
    R6 = kept.r6;
    R7 = kept.r7;
    RETURN_TO_CALLER();
}

/* ── sprite_drawIfFlagSet_812B504 ── */
/* sprite_drawIfFlagSet_812B504(r5 = pointer to an object):
 * Test the flag byte at r5+0x1B for bit 4 (0x10). If set, invoke
 * battleObject_queueOamFromTableRow_811FAF4 with a fixed argument block (0xC80008, 0x20004, 0,
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

        GUEST_CALL(battleObject_queueOamFromTableRow_811FAF4);

        R4 = savedR4;   /* the original pushes/pops r4 around the call */
    }
    RETURN_TO_CALLER();
}

/* ── loadEntityJumpOffset00Gfx_812B530 ── */
/* loadEntityJumpOffset00Gfx_812B530(r5 = entity):
 *  - Ask the entity for its jump-table offset 0 entry (function pointer / index).
 *  - Use that as an index into an array of LZ77-compressed graphics blob pointers.
 *  - Decompress the chosen blob into the LZ77 decompression buffer.
 *  - DMA 0x100 bytes of the decompressed graphics (skipping the 4-byte LZ77 header)
 *    into VRAM at 0x06014700.
 *  - Set the entity's flag byte at offset 0x1B (OR-in 0x10).
 */
Next loadEntityJumpOffset00Gfx_812B530(void) {
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
