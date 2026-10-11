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
