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
