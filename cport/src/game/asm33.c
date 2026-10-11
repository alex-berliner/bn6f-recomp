/*
 * asm33.c: readable functions of bn6f asm33.s, up to 0x081263D0, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_8123558 ── */
/* register form: calls sub_8123BC0 outside this file, which has no typed C interface yet */
static void call_8123bc0(void) {
    GUEST_CALL(sub_8123BC0);
}

/* register form: calls sub_8123BE0 outside this file, which has no typed C interface yet */
static void call_8123be0(void) {
    GUEST_CALL(sub_8123BE0);
}

/* register form: calls sub_8123C00 outside this file, which has no typed C interface yet */
static void call_8123c00(void) {
    GUEST_CALL(sub_8123C00);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet.
 * Returns r0 and reports whether Z was set (i.e. the beq would be taken). */
static u32 call_is_screen_fade_active_sub_8123558(bool *beq_taken) {
    GUEST_CALL(IsScreenFadeActive);
    *beq_taken = ZERO_FLAG();
    return R0;
}

typedef struct {
    u32 r0;
    u32 nz_source;   /* value for SET_FLAGS_NZ: 0 → Z=1, non-zero → Z=0 */
} BattleUpdateResult;

/* Run three battle-update sub-routines; if the screen fade is NOT active,
 * store 4 into the battle-object state byte.  Returns the final r0 value
 * and the appropriate source for SET_FLAGS_NZ. */
static BattleUpdateResult battle_state_update(u32 battle_obj) {
    call_8123bc0();
    call_8123be0();
    call_8123c00();

    bool beq;
    u32 fade_r0 = call_is_screen_fade_active_sub_8123558(&beq);

    if (beq) {
        /* Z set: skip the store, return r0 and flags from IsScreenFadeActive */
        BattleUpdateResult res = { fade_r0, 0 };
        return res;
    }

    /* Z clear: mov r0, #4 then strb r0, [r5, #2] */
    ST8(battle_obj + 2, 4);
    BattleUpdateResult res = { 4, 4 };
    return res;
}

/* sub_8123558(r5 = battle object ptr) -> r0, nz c v */
Next sub_8123558(void) {
    BattleUpdateResult res = battle_state_update(R5);
    R0 = res.r0;
    SET_FLAGS_NZ(res.nz_source);
    RETURN_TO_CALLER();
}

/* ── sub_8123644 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool query_screen_fade(u32 *out_r0) {
    GUEST_CALL(IsScreenFadeActive);
    *out_r0 = R0;
    return ZERO_FLAG();   /* Z set → fade inactive → caller's beq is taken */
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void call_chatbox_reset(void) {
    GUEST_CALL(chatbox_8040818);
}

/* If no screen fade is active, return IsScreenFadeActive's R0 unchanged (beq skips stores).
 * If a fade IS active, reset three status bytes at status_ptr to 0x20/0x00/0x00 and return 0.
 * NZCV flags persist from the last GUEST_CALL; MOV-immediate and STRB do not alter flags. */
static u32 clear_chatbox_during_fade(u32 status_ptr) {
    u32 fade_r0;

    if (query_screen_fade(&fade_r0))
        return fade_r0;           /* beq path: R0 and flags from IsScreenFadeActive */

    call_chatbox_reset();         /* flags from chatbox_8040818 */
    ST8(status_ptr,     0x20);
    ST8(status_ptr + 1, 0x00);
    ST8(status_ptr + 2, 0x00);
    return 0;                     /* mov r0, #0 does NOT set flags */
}

/* sub_8123644(r5 = status_ptr) -> r0, nzcv */
Next sub_8123644(void) {
    R0 = clear_chatbox_during_fade(R5);
    RETURN_TO_CALLER();
}

/* ── sub_812365C ── */
/* sub_812365C: dispatch stub.
 * Reads the selector byte at [R5+3] and jumps to the matching routine from
 * the two-entry table at off_8123670. Every register except R0/R1 and the
 * condition flags are passed straight through, so the selected handler runs
 * with the caller's state and its results (r0-r12, nz/c/v) flow unchanged
 * back to the original caller. */
Next sub_812365C(void) {
    u32 mode = LD8(R5 + 3);

    /* The original loads the handler address into R0 (and the selector into
     * R1) as a side effect of the table lookup before branching to it;
     * reproduce that so the callee sees the same register state. */
    R1 = mode;
    if (mode == 0) {
        R0 = 0x08123679;            /* off_8123670[0] = sub_8123678 (Thumb) */
        GUEST_CALL(sub_8123678);
    } else {
        R0 = 0x0812368D;            /* off_8123670[1] = sub_812368C (Thumb) */
        GUEST_CALL(sub_812368C);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8123678 ── */
/* sub_8123678(r5 = object pointer):
 * Runs a check by calling sprite_testFrameParamsMaskC0_81190F8 with argument 0. If it reports
 * success, writes state 4 to the object's byte at offset 3 and calls
 * sub_8123D70 to advance it; otherwise returns immediately.
 * Flags on return are those left by whichever callee ran last. */
Next sub_8123678(void) {
    R0 = 0;
    GUEST_CALL(sprite_testFrameParamsMaskC0_81190F8);   /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = 4;
        ST8(R5 + 3, R0);
        GUEST_CALL(sub_8123D70);
    }
    RETURN_TO_CALLER();
}

/* ── sub_812368C ── */
/* Sub-menu struct offsets (r5 points to this structure) */
#define oMenu_State         2
#define oMenu_NaviIndex     0xC
#define oMenu_PetSlot       0x14
#define oMenu_Cursor        0x20

/* PET navi stat byte index for the "send-to-PET" transition flag */
#define STAT_IDX_TRANSITION 0x2D

/* Menu state codes written to oMenu_State */
#define MSTATE_BACK        4
#define MSTATE_TRANSITION  0xC
#define MSTATE_CONFIRMED   0x10

/* Fade parameters for SetScreenFade */
#define FADE_TYPE          0xC
#define FADE_AMOUNT        0x10

/* Sound ID for navi send rejection */
#define SOUND_NAVI_REJECT  0x182u

/* Dialog IDs shown after successful send (upper nibble of navi status vs 2) */
#define DIALOG_NOT_READY   8
#define DIALOG_READY       9

/* Lookup table: mode 0 -> byte-table at 0x140C08, mode 1 -> byte-table at 0xC */
#define A_navi_state_tables  0x08123768u

/* register form: calls IsButtonPressed outside this file, which has no typed C interface yet */
static bool is_button_pressed(u32 button) {
    R0 = button;
    GUEST_CALL(IsButtonPressed);
    return !ZERO_FLAG();
}

/* register form: calls sub_811B314 outside this file, which has no typed C interface yet */
static void call_811B314(u32 a0, u32 a1, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = a0; R1 = a1; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_811B314);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_sub_812368C(u32 sound) {
    R0 = sound;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls sub_8119118 outside this file, which has no typed C interface yet;
 * leaves r0-r2 and nz/c/v for the caller */
static void call_8119118(u32 a0, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = a0; R3 = r3; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8119118);
}

/* register form: calls sub_8123E08 outside this file, which has no typed C interface yet;
 * returns mode in R0 and r2 in *out_r2 */
static u32 get_navi_mode(u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10, u32 *out_r2) {
    R3 = r3; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8123E08);
    if (out_r2) *out_r2 = R2;
    return R0;
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u8 get_pet_stat(u32 pet_slot, u32 stat_idx, u32 r2, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = pet_slot; R1 = stat_idx; R2 = r2; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (u8)R0;
}

/* register form: calls SetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static void set_pet_stat(u32 pet_slot, u32 stat_idx, u32 value,
                         u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = pet_slot; R1 = stat_idx; R2 = value; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(SetCurPETNaviStatsByte);
}

/* register form: calls sub_8120D10 outside this file, which has no typed C interface yet;
 * R4 carries the saved stat as context */
static bool call_8120D10_is_zero(u32 flag, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = flag; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8120D10);
    return ZERO_FLAG();
}

/* register form: calls sub_8137884 outside this file, which has no typed C interface yet */
static u32 get_navi_status(u32 navi_idx, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = navi_idx; R2 = r2; R3 = r3; R4 = r4; R5 = r5; R6 = r6; R7 = r7;
    GUEST_CALL(sub_8137884);
    return R0;
}

/* register form: calls sub_8123A40 outside this file, which has no typed C interface yet;
 * returns updated r5 in *out_r5 (used for subsequent state write) */
static void call_8123A40(u32 dialog_id, u32 r1_val, u32 r6_val, u32 r7, u32 r10, u32 *out_r5) {
    R0 = dialog_id; R1 = r1_val; R6 = r6_val; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8123A40);
    *out_r5 = R5;
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void set_screen_fade(u32 a0, u32 a1, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = a0; R1 = a1; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls sub_8123D70 outside this file, which has no typed C interface yet;
 * returns r0-r3 and nz/c/v */
static void call_8123D70(u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8123D70);
}

/* register form: calls menu_moveCursorUpDown_811FA22 outside this file, which has no typed C interface yet */
static u16 move_menu_cursor(u32 held, u32 step, u32 wrap, u16 cursor, u32 r4_val,
                            u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = held; R1 = step; R2 = wrap; R3 = cursor; R4 = r4_val;
    R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    return (u16)R0;
}

/* Handle one frame of the PET navi sub-menu.
 * menu_ptr (r5) -> sub-menu state; toolkit_ptr (r10) -> toolkit;
 * r3-r7 carried through to callees as preserved arguments. */
static void pet_navi_menu_frame(u32 r2, u32 r3, u32 r4, u32 menu_ptr,
                                u32 r6, u32 r7, u32 toolkit_ptr) {

    /* --- B pressed: go back --- */
    if (is_button_pressed(JOYPAD_B)) {
        ST8(menu_ptr + oMenu_State, MSTATE_BACK);
        call_811B314(1, 0, r4, menu_ptr, r6, r7, toolkit_ptr);
        play_sound_sub_812368C(SOUND_EXIT_SUBMENU);
        call_8119118(0, r3, r4, menu_ptr, r6, r7, toolkit_ptr);
        return;
    }

    /* --- A pressed: confirm selection --- */
    if (is_button_pressed(JOYPAD_A)) {
        play_sound_sub_812368C(SOUND_MENU_SELECT);

        u32 mode = get_navi_mode(r3, r4, menu_ptr, r6, r7, toolkit_ptr, NULL);
        u32 table_addr = LD32(A_navi_state_tables + mode * 4);
        u16 cursor = (u16)LD16(menu_ptr + oMenu_Cursor);
        u8 state = (u8)LD8(table_addr + cursor);
        ST8(menu_ptr + oMenu_State, state);

        if (state != MSTATE_TRANSITION) {
            set_screen_fade(FADE_TYPE, FADE_AMOUNT, r4, menu_ptr, r6, r7, toolkit_ptr);
            call_8123D70(r4, menu_ptr, r6, r7, toolkit_ptr);
            return;
        }

        /* State 0xC: attempt to send navi to PET */
        u32 pet_slot = LD8(menu_ptr + oMenu_PetSlot);
        u32 navi_idx = LD8(menu_ptr + oMenu_NaviIndex);

        u8 saved_stat = get_pet_stat(pet_slot, STAT_IDX_TRANSITION,
                                     r2, r4, menu_ptr, r6, r7, toolkit_ptr);
        set_pet_stat(pet_slot, STAT_IDX_TRANSITION, navi_idx,
                     r4, menu_ptr, r6, r7, toolkit_ptr);

        if (call_8120D10_is_zero(1, saved_stat, menu_ptr, r6, r7, toolkit_ptr)) {
            /* Rejected: back to previous menu */
            ST8(menu_ptr + oMenu_State, MSTATE_BACK);
            call_811B314(1, 0, r4, menu_ptr, r6, r7, toolkit_ptr);
            play_sound_sub_812368C(SOUND_NAVI_REJECT);
            call_8119118(0, r3, r4, menu_ptr, r6, r7, toolkit_ptr);
            return;
        }

        /* Accepted: restore original stat, show status dialog */
        set_pet_stat(pet_slot, STAT_IDX_TRANSITION, saved_stat,
                     r4, menu_ptr, r6, r7, toolkit_ptr);

        u32 status = get_navi_status(navi_idx, r2, r3, r4, menu_ptr, r6, r7) >> 4;
        u32 dialog = (status >= 2) ? DIALOG_READY : DIALOG_NOT_READY;

        u32 updated_r5;
        call_8123A40(dialog, STAT_IDX_TRANSITION, dialog, r7, toolkit_ptr, &updated_r5);
        ST8(updated_r5 + oMenu_State, MSTATE_CONFIRMED);
        call_8119118(0, r3, r4, menu_ptr, r6, r7, toolkit_ptr);
        return;
    }

    /* --- Neither A nor B: move cursor --- */
    u32 cursor_mode = get_navi_mode(r3, 3, menu_ptr, r6, r7, toolkit_ptr, NULL);
    u32 step = (cursor_mode != 0) ? 1u : 3u;

    u32 joypad_ptr = LD32(toolkit_ptr + oToolkit_JoypadPtr);
    u32 held = LD16(joypad_ptr + oJoypad_LowSensitivityHeld);
    u16 cur = (u16)LD16(menu_ptr + oMenu_Cursor);

    u16 new_cursor = move_menu_cursor(held, step, 1, cur, step,
                                      menu_ptr, r6, r7, toolkit_ptr);
    ST16(menu_ptr + oMenu_Cursor, new_cursor);

    call_8123D70(r4, menu_ptr, r6, r7, toolkit_ptr);
}

/* sub_812368C: PET navi sub-menu frame handler
 * inputs: r2 r3 r4 r5 r6 r7 r10 lr; preserved: r4 r5 r6 r7 r10 */
Next sub_812368C(void) {
    u32 sr4 = R4, sr5 = R5, sr6 = R6, sr7 = R7, sr10 = R10;
    pet_navi_menu_frame(R2, R3, R4, R5, R6, R7, R10);
    R4 = sr4; R5 = sr5; R6 = sr6; R7 = sr7; R10 = sr10;
    RETURN_TO_CALLER();
}

/* ── chipFolder_initGfx_812386C ── */
/* Register form: calls sub_811FB84 outside this file, which has no typed C interface yet */
static u32 resolve_decomp_pointer(u32 source, u32 size) {
    R0 = source;
    R1 = size;
    GUEST_CALL(sub_811FB84);
    return R0;
}

/* Register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decompress_copy_data(u32 table) {
    R0 = table;
    GUEST_CALL(decompAndCopyData);
}

/* Register form: calls sub_8123300 outside this file, which has no typed C interface yet */
static void load_text_script_block(u32 table, u32 dest, u32 buffer) {
    R0 = table;
    R1 = dest;
    R2 = buffer;
    GUEST_CALL(sub_8123300);
}

/* Register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_background_tiles(u32 j, u32 i, u32 block, u32 tile_ids, u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = block;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* Initialize the chip-folder PET screen: build a decompression descriptor table from a
 * ROM source table, decompress tile/sprite/palette graphics, load folder-page text
 * scripts, and copy background grid tiles into tile block 1. */
static void chipFolder_initGfx(void) {
    /* register form: save/restore caller registers preserved across GUEST_CALLs (original's push/pop of r4-r7) */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    u32 out_base = A_unk_20096E0;  /* scratch RAM for the descriptor table */
    u32 tbl      = A_off_812391C;  /* ROM source table: {src, size, arg1, arg2} rows */
    u32 out_off  = 0;
    u32 in_off   = 0;

    /* Resolve each compressed pointer and copy the two trailing args into a flat
     * {resolved_ptr, arg1, arg2} triple per row (4 rows × 16 bytes = 0x40). */
    for (int row = 0; row < 4; row++) {
        u32 src = LD32(tbl + in_off);  in_off += 4;
        u32 sz  = LD32(tbl + in_off);  in_off += 4;

        ST32(out_base + out_off, resolve_decomp_pointer(src, sz));  out_off += 4;

        ST32(out_base + out_off, LD32(tbl + in_off));  out_off += 4;  in_off += 4;
        ST32(out_base + out_off, LD32(tbl + in_off));  out_off += 4;  in_off += 4;
    }
    ST32(out_base + out_off, 0);  /* null terminator for the descriptor list */

    /* Decompress chip-folder GFX using the built table and a second static table */
    decompress_copy_data(out_base);
    decompress_copy_data(A_dword_8123964);

    /* Load compressed text scripts for the folder page labels */
    load_text_script_block(A_off_81238E4, A_eTextScript201C220, A_eDecompBuffer2013A00);

    /* Copy background tile data for the folder grid (two regions in tile block 1) */
    copy_background_tiles(0, 0, 1, A_eTextScript201BA20, 0x1E, 0x14);
    copy_background_tiles(9, 6, 1, A_eTextScript201BF20, 0x14, 0xD);

    /* register form: restore preserved caller registers (original's pop {r4-r7,pc}) */
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
}

/* chipFolder_initGfx_812386C -> r2/r3 from last CopyBackgroundTiles, r4-r7 preserved */
Next chipFolder_initGfx_812386C(void) {
    chipFolder_initGfx();
    RETURN_TO_CALLER();
}

/* ── sub_8123AB8 ── */
/* The tileset ID for the current field: byte at obj+0xC indexes a ROM table
 * of u16 offsets; the u16 at (obj + offset) is the tileset number. */
static u32 get_tileset_id(u32 battle_obj) {
    u32 index  = LD8(battle_obj + 0xC);
    u32 offset = LD8(A_dword_8123BBC + index);
    return LD16(battle_obj + offset);
}

/* register form: callee needs r5,r7 as inputs and may clobber r4,r6;
 * the original saves r4-r7 via push/pop which C cannot express here */
static void call_prep_wide(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D6E0;
    R3 = 0xB;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(sub_81200EC);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r4,r5,r7 as inputs; saves r4-r7 around GUEST_CALL */
static void call_prep_narrow(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D7C0;
    R3 = A_byte_8123394;
    R4 = 0x6007200;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(sub_8120390);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r4,r5,r7 as inputs; saves r4-r7 around GUEST_CALL */
static void call_prep_left_panel(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D7F8;
    R3 = (0xAu << 12) | 0x35Cu;
    R4 = 0x6006B80;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(sub_81203E4);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r5,r7 as inputs; saves r4-r7 around GUEST_CALL */
static void call_prep_mid_strip(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D830;
    R3 = (0xBu << 12) | 0x216u;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(sub_8120458);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r5,r7 as inputs; saves r4-r7 around GUEST_CALL */
static void call_prep_right_edge(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D84C;
    R3 = (9u << 12) | 0x34Fu;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(sub_81204C4);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r4,r5 as inputs; saves r4-r7 around GUEST_CALL */
static void call_copy_tiles(u32 j, u32 i, u32 tile_ids, u32 j_size, u32 i_size) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = j;
    R1 = i;
    R2 = 2;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* Build the five battle-background tile regions for the current field.
 * Each region: read tileset ID → call a prep routine (decompresses tile data
 * into a staging buffer) → call CopyBackgroundTiles to blit into tile block 2. */
static void draw_battle_background(u32 battle_obj) {
    call_prep_wide(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0xE, 8, A_unk_201D6E0, 8, 0xA);

    call_prep_narrow(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0xC, 8, A_unk_201D7C0, 2, 0xA);

    call_prep_left_panel(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0x16, 8, A_unk_201D7F8, 2, 0xA);

    call_prep_mid_strip(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0x18, 8, A_unk_201D830, 1, 0xA);

    call_prep_right_edge(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0x19, 8, A_unk_201D84C, 2, 0xA);
}

/* sub_8123AB8(r5 = battle_object_ptr) — preserves r4–r7, c/v from last callee */
Next sub_8123AB8(void) {
    draw_battle_background(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8123BC0 ── */
/* sub_8123BC0:
 * Passes a fixed argument set to sub_8123C20 and returns its result untouched.
 * Arguments are all constants: the word table at dword_8123BDC, the byte
 * table at byte_81233D8, 1 in r1, 4 in r3 and a length of 0x1C in r4.
 * Like the original, this wrapper preserves r4-r7 across the call (it only
 * sets r4 as an argument, then restores the caller's value afterwards). */
Next sub_8123BC0(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    R0 = A_dword_8123BDC; /* pointer to the word table (0x302F2E) */
    R1 = 1;
    R2 = A_byte_81233D8;  /* pointer to the byte table */
    R3 = 4;
    R4 = 0x1C;            /* entry count */
    GUEST_CALL(sub_8123C20);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── sub_8123BE0 ── */
/* sub_8123BE0: thin wrapper around sub_8123C20 with a fixed argument set.
 * r0 points at the 3-byte constant in the literal pool (dword_8123BFC =
 * 0x5A5856), r2 at the byte_81233E0 work area; the remaining arguments
 * (0x20, 5, 0x5C, 0) select its mode/geometry. The callee's r4/r6
 * arguments are set locally — the caller's r4-r7 are saved around the
 * call and restored on return. */
Next sub_8123BE0(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    R0 = A_dword_8123BFC; /* source data: the three bytes 0x5A, 0x58, 0x56 */
    R1 = 0x20;
    R2 = A_byte_81233E0;
    R3 = 5;
    R4 = 0x5C;
    R6 = 0;
    GUEST_CALL(sub_8123C20);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── sub_8123C00 ── */
/* sub_8123C00: prepare a fixed set of arguments and forward them to sub_8123C20.
 * The color/palette table at dword_8123C1C (0x5B5957 = three BGR555 entries) and
 * the work buffer byte_81233E8 are passed by address; the remaining arguments
 * (0x40, 6, 0x60, -, 1) are constants. r4-r7 are restored to their entry values
 * on return, matching the original's push/pop around the call. */
Next sub_8123C00(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R0 = A_dword_8123C1C;   /* color data */
    R1 = 0x40;
    R2 = A_byte_81233E8;    /* destination buffer */
    R3 = 6;
    R4 = 0x60;
    R6 = 1;

    GUEST_CALL(sub_8123C20);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8123D54 ── */
/* sub_8123D54(r0 = selection): call the string/label printer at sub_80465A0
 * with one of two fixed tables — byte_81233F0 when r0 is 0, byte_81233F8
 * otherwise. R1 emerges holding the original r0 (the callee leaves it alone),
 * and the callee's flag results pass through to the caller. */
Next sub_8123D54(void) {
    u32 which = R0;

    R1 = which;
    R0 = (which != 0) ? A_byte_81233F8 : A_byte_81233F0;

    GUEST_CALL(sub_80465A0);

    RETURN_TO_CALLER();
}

/* ── sub_8123E08 ── */
/* sub_8123E08(r5 = state pointer):
 * Checks bit 0x20 of the status byte unk_20018EC[*(u8*)(r5 + 0xC)].
 * Returns r0 = r2 = 1 if the bit is set, 0 otherwise.
 * Flags are left from the AND with 0x20 (NZ), as callers may branch on them. */
Next sub_8123E08(void) {
    u32 index = LD8(R5 + 0xC);
    u32 bit = LD8(A_unk_20018EC + index) & 0x20;

    SET_FLAGS_NZ(bit);

    u32 isSet = (bit != 0);
    R0 = isSet;
    R2 = isSet;
    RETURN_TO_CALLER();
}

/* ── submenu_subchip_81240BC ── */
/* register form: calls sub_81244EC outside this file, which has no typed C interface yet */
static void call_submenu_update(void) {
    GUEST_CALL(sub_81244EC);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool call_screen_fade_active(u32 *r0_out) {
    GUEST_CALL(IsScreenFadeActive);
    *r0_out = R0;
    return ZERO_FLAG();
}

struct SubResult {
    u32 r0;       /* final R0 for the caller */
    u32 nz_seed;  /* value for SET_FLAGS_NZ to reproduce N,Z */
};

/* Advance the subchip submenu one frame.
 * If the screen fade is active, return IsScreenFadeActive's r0 (Z=1 ⇒ N=0).
 * Otherwise, mark the sub-menu ready (byte 4 at UnkSubMenu+2) and return 4. */
static struct SubResult subchip_submenu_tick(u32 state) {
    call_submenu_update();

    u32 fade_r0;
    if (call_screen_fade_active(&fade_r0)) {
        struct SubResult r = { fade_r0, 0 };
        return r;
    }

    ST8(state + 2, 4);
    struct SubResult r = { 4, 4 };
    return r;
}

/* submenu_subchip_81240BC(r5 = UnkSubMenu* state) -> r0, nz c v */
Next submenu_subchip_81240BC(void) {
    struct SubResult res = subchip_submenu_tick(R5);
    R0 = res.r0;
    SET_FLAGS_NZ(res.nz_seed);
    RETURN_TO_CALLER();
}

/* ── submenu_subchip_8124134 ── */
/* submenu_subchip_8124134(r5 = sub-menu state struct):
 * If the chatbox is currently open (flag 0x80 of the chatbox state mask),
 * leave the sub-menu state untouched; otherwise reset its byte at +2 to 4
 * (restores the default sub-menu selection/mode after closing the chatbox).
 * Returns r0 = chatbox flag check result, or 4 if the state byte was reset.
 * Flags: those left by chatbox_mask_eFlags2009F38, or NZ from the stored value. */
Next submenu_subchip_8124134(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    if (R0 == 0) {
        /* Chatbox not open: restore the default state byte. */
        R0 = 4;
        ST8(R5 + 2, R0);
        SET_FLAGS_NZ(R0);
    }
    /* Otherwise keep the flags from the chatbox flag check. */
    RETURN_TO_CALLER();
}

/* ── sub_8124384 ── */
/* Register-form call helpers for game functions outside this file */

/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet */
static void zero_fill_by_byte(u32 addr, u32 num_bytes) {
    R0 = addr;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByByte);
}

/* register form: calls CheckKeyItem outside this file, which has no typed C interface yet */
static u32 check_key_item(u32 item_id) {
    R0 = item_id;
    GUEST_CALL(CheckKeyItem);
    return R0;
}

/*
 * Zero the 16-byte key-item status buffer at unk_201C400, then populate it
 * with (item_id, CheckKeyItem result) byte-pairs for items 0x80 through 0x85.
 */
static void fill_key_item_table(void) {
    u32 base = A_unk_201C400;
    zero_fill_by_byte(base, 0x10);

    u32 offset = 0;
    for (u32 item = 0x80; item <= 0x85; item++) {
        u32 owned = check_key_item(item);
        ST8(base + offset, item);
        offset++;
        ST8(base + offset, owned);
        offset++;
    }
}

/* sub_8124384() — no meaningful args or return; preserves r2–r7 via push/pop. */
Next sub_8124384(void) {
    fill_key_item_table();
    RETURN_TO_CALLER();
}

/* ── bg_CopyTileBlock2At2_6_8x12_81245BC ── */
/* bg_CopyTileBlock2At2_6_8x12_81245BC: copy a 6x8 block of background tiles.
 *
 * Prepares the arguments for CopyBackgroundTiles (a leaf routine that takes
 * six arguments and may clobber the low registers, including r4/r5) and calls
 * it, preserving the caller's r4-r7 across the call. r1 and r2 are left as
 * CopyBackgroundTiles leaves them; the condition flags are likewise whatever
 * the callee's last flag-setting instruction produced. */
Next bg_CopyTileBlock2At2_6_8x12_81245BC(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    R0 = 2;              /* j: destination tile column */
    R1 = 6;              /* i: destination tile row */
    R2 = 2;              /* which 32x32 tile block to copy from */
    R3 = A_unk_201EC2C;  /* tile_ids: source tile id table */
    R4 = 8;              /* j_size */
    R5 = 0xC;            /* i_size */

    GUEST_CALL(CopyBackgroundTiles);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── chatbox_runScript201C100_812474C ── */
/* chatbox_runScript201C100_812474C(r0 = script index):
 * Run one text script from the chatbox text archive (eTextScript201C100).
 * All register and flag outputs are just whatever chatbox_runScript leaves. */
Next chatbox_runScript201C100_812474C(void) {
    u32 scriptIdx = R0;

    R1 = scriptIdx;                 /* archive in r0, script index in r1 */
    R0 = A_eTextScript201C100;
    GUEST_CALL(chatbox_runScript_803FD9C);

    RETURN_TO_CALLER();
}

/* ── HandleLibraryMenu8124B3C ── */
/* register form: indirect dispatch through a 3-entry jump table indexed by a byte
 * state variable at [R5+1]; GUEST_CALL requires a static target per case, and the
 * callee receives R0 = its own thumb entry address and R1 = the state byte,
 * matching the original ldr / bx sequence. Valid state values are 0, 4, 8. */
static void dispatch_library_menu(void) {
    u32 state = LD8(R5 + 1);
    R1 = state;
    switch (state) {
        case 0: R0 = 0x08124B5D; GUEST_CALL(sub_8124B5C); break;
        case 4: R0 = 0x08124C31; GUEST_CALL(sub_8124C30); break;
        case 8: R0 = 0x08125255; GUEST_CALL(sub_8125254); break;
    }
}

/* HandleLibraryMenu8124B3C: trampoline dispatching to one of three library-menu
 * handlers based on the byte at [object + 1]. */
Next HandleLibraryMenu8124B3C(void) {
    dispatch_library_menu();
    RETURN_TO_CALLER();
}

/* ── sub_8124B5C ── */
/* register form: calls zeroFillVRAM outside this file, which has no typed C interface yet */
static void call_zeroFillVRAM(void) {
    GUEST_CALL(zeroFillVRAM);
}

/* register form: calls ZeroFill_byte_3001960 outside this file, which has no typed C interface yet */
static void call_zeroFillByteTable(void) {
    GUEST_CALL(ZeroFill_byte_3001960);
}

/* register form: calls ZeroFillGFX30025c0 outside this file, which has no typed C interface yet */
static void call_zeroFillGFXData(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
}

/* register form: calls sub_80015FC outside this file, which has no typed C interface yet */
static void call_sub_80015FC(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_80015FC);
}

/* register form: calls sub_8126818 outside this file, which has no typed C interface yet */
static void call_sub_8126818(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_8126818);
}

/* register form: calls initRefs_buildAndDecompressTable_8125280 outside this file, which has no typed C interface yet */
static void call_initRefsTable(void) {
    GUEST_CALL(initRefs_buildAndDecompressTable_8125280);
}

/* register form: calls sub_81254FC outside this file, which has no typed C interface yet; clobbers r10 */
static void call_sub_81254FC(void) {
    GUEST_CALL(sub_81254FC);
}

/* register form: calls sub_81207F8 outside this file, which has no typed C interface yet */
static u32 call_sub_81207F8(u32 index) {
    R0 = index;
    GUEST_CALL(sub_81207F8);
    return R0;
}

/* register form: calls sub_81258F8 outside this file, which has no typed C interface yet */
static u32 call_sub_81258F8(void) {
    GUEST_CALL(sub_81258F8);
    return R0;
}

/* register form: calls sub_8125994 outside this file, which has no typed C interface yet */
static u32 call_sub_8125994(void) {
    GUEST_CALL(sub_8125994);
    return R0;
}

/* register form: calls sub_8125A6C outside this file, which has no typed C interface yet */
static u32 call_sub_8125A6C(void) {
    GUEST_CALL(sub_8125A6C);
    return R0;
}

/* register form: calls sub_8125B3C outside this file, which has no typed C interface yet */
static u32 call_sub_8125B3C(void) {
    GUEST_CALL(sub_8125B3C);
    return R0;
}

/* register form: calls sub_8125C08 outside this file, which has no typed C interface yet */
static u32 call_sub_8125C08(void) {
    GUEST_CALL(sub_8125C08);
    return R0;
}

/* register form: calls sub_8046664 outside this file, which has no typed C interface yet; clobbers r1 */
static void call_sub_8046664_sub_8124B5C(void) {
    GUEST_CALL(sub_8046664);
}

/* register form: calls sub_80465A0 outside this file, which has no typed C interface yet */
static void call_sub_80465A0(u32 ptr) {
    R0 = ptr;
    GUEST_CALL(sub_80465A0);
}

/* register form: calls sub_81252C0 outside this file, which has no typed C interface yet */
static void call_sub_81252C0(void) {
    GUEST_CALL(sub_81252C0);
}

/* register form: calls sub_8125D10 outside this file, which has no typed C interface yet */
static void call_sub_8125D10_sub_8124B5C(void) {
    GUEST_CALL(sub_8125D10);
}

/* register form: calls sub_812546C outside this file, which has no typed C interface yet; clobbers r7 */
static void call_sub_812546C(void) {
    GUEST_CALL(sub_812546C);
}

/* register form: calls sub_8120CC8 outside this file, which has no typed C interface yet */
static u32 call_sub_8120CC8(u32 slot, u32 data) {
    R0 = slot; R1 = data;
    GUEST_CALL(sub_8120CC8);
    return R0 | (R1 << 4);  /* combine both callee outputs as the caller does */
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet; clobbers r1 */
static void call_SetScreenFade(int level, int speed) {
    R0 = level; R1 = speed;
    GUEST_CALL(SetScreenFade);
}

/* Initialize VRAM, GFX, render-info header, battle-state table, and trigger a screen fade.
 * battle_data (r5) is the main battle-state struct; toolkit (r10) provides RenderInfoPtr. */
static u32 init_battle_data(u32 battle_data, u32 toolkit) {
    /* Clear VRAM, byte table at 0x3001960, and GFX data at 0x30025C0 */
    call_zeroFillVRAM();
    call_zeroFillByteTable();
    call_zeroFillGFXData();

    call_sub_80015FC(0x10);

    /* Reset render-info header: first u16 to 0x7F40, six following u16s to 0 */
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render_info, 0x7F40);
    ST16(render_info + 0x10, 0);
    ST16(render_info + 0x12, 0);
    ST16(render_info + 0x14, 0);
    ST16(render_info + 0x16, 0);
    ST16(render_info + 0x18, 0);
    ST16(render_info + 0x1A, 0);

    call_sub_8126818(0);

    /* Fixed initial state bytes */
    ST8(battle_data + 1, 4);
    ST8(battle_data + 2, 0);

    call_initRefsTable();
    call_sub_81254FC();

    /* Timer counters */
    ST32(battle_data + 0x74, 0xA0);
    ST32(battle_data + 0x78, 0xA0);

    /* Per-index lookup table slots */
    ST32(battle_data + 0x58, call_sub_81207F8(0));
    ST32(battle_data + 0x5C, call_sub_81207F8(1));
    ST32(battle_data + 0x60, call_sub_81207F8(2));

    u32 lookup_5 = call_sub_81207F8(5);
    ST32(battle_data + 0x64, lookup_5);

    /* Battle mode byte: 4 if lookup[5] non-zero, else 3 */
    ST8(battle_data + 0xE, lookup_5 ? 4 : 3);

    ST32(battle_data + 0x68, call_sub_81207F8(4));

    /* Sub-field initialization (each callee returns a u16) */
    ST16(battle_data + 0x12, call_sub_81258F8());
    ST16(battle_data + 0x1C, call_sub_8125994());
    ST16(battle_data + 0x26, call_sub_8125A6C());
    ST16(battle_data + 0x30, call_sub_8125B3C());
    ST16(battle_data + 0x3A, call_sub_8125C08());

    /* Additional subsystems */
    call_sub_8046664_sub_8124B5C();
    call_sub_80465A0(A_byte_8124AE0);
    call_sub_81252C0();
    call_sub_8125D10_sub_8124B5C();
    call_sub_812546C();

    /* Store combined lookup byte from sub_8120CC8 */
    ST8(battle_data + 0xC, call_sub_8120CC8(0, A_dword_8124A94));

    /* Trigger screen fade in */
    call_SetScreenFade(8, 0x10);

    return 0;
}

/* sub_8124B5C(r5 = battle_data, r10 = toolkit) -> r0=0 */
Next sub_8124B5C(void) {
    R0 = init_battle_data(R5, R10);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_81250E0 ── */
/* Run five battle sub-updates, then check if a screen fade is active.
 * If active: write 8 to [ptr+1] and output r0=8 (flags N=0,Z=0 from MOV).
 * If not active: output r0 from IsScreenFadeActive (flags N=0,Z=1).
 * C and V are always IsScreenFadeActive's (MOV/CMP don't alter them here).
 * Returns a value whose zero/nonzero lets the adapter reproduce N/Z correctly. */

/* register form: calls sub_812674C outside this file, which has no typed C interface yet */
static void call_sub_812674C(void) {
    GUEST_CALL(sub_812674C);
}

/* register form: calls sub_81267A8 outside this file, which has no typed C interface yet */
static void call_sub_81267A8(void) {
    GUEST_CALL(sub_81267A8);
}

/* register form: calls sub_8125444 outside this file, which has no typed C interface yet */
static void call_sub_8125444(void) {
    GUEST_CALL(sub_8125444);
}

/* register form: calls sub_8125D10 outside this file, which has no typed C interface yet */
static void call_sub_8125D10_sub_81250E0(void) {
    GUEST_CALL(sub_8125D10);
}

/* register form: calls sub_81264F4 outside this file, which has no typed C interface yet */
static void call_sub_81264F4(void) {
    GUEST_CALL(sub_81264F4);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static u32 call_is_screen_fade_active_sub_81250E0(bool *fade_is_active) {
    GUEST_CALL(IsScreenFadeActive);
    *fade_is_active = !ZERO_FLAG();   /* Z clear → fade is active */
    return R0;
}

static u32 battle_update_and_fade_check(u32 fade_ptr, u32 *out_r0) {
    call_sub_812674C();
    call_sub_81267A8();
    call_sub_8125444();
    call_sub_8125D10_sub_81250E0();
    call_sub_81264F4();

    bool fade_active;
    u32 ret = call_is_screen_fade_active_sub_81250E0(&fade_active);

    if (fade_active) {
        ST8(fade_ptr + 1, 8);
        *out_r0 = 8;
        return 8;     /* SET_FLAGS_NZ(8) → N=0, Z=0, matching MOV r0,#8 */
    }
    *out_r0 = ret;
    return 0;         /* SET_FLAGS_NZ(0) → N=0, Z=1, matching IsScreenFadeActive's Z */
}

/* sub_81250E0() → nz, r0, c, v */
Next sub_81250E0(void) {
    u32 r0_val;
    u32 flag_enc = battle_update_and_fade_check(R5, &r0_val);
    R0 = r0_val;
    SET_FLAGS_NZ(flag_enc);
    RETURN_TO_CALLER();
}

/* ── sub_8125170 ── */
/* register form: calls sub_812674C outside this file, which has no typed C interface yet */
static void call_812674C(void) {
    GUEST_CALL(sub_812674C);
}

/* register form: calls sub_81267A8 outside this file, which has no typed C interface yet */
static void call_81267A8(void) {
    GUEST_CALL(sub_81267A8);
}

/* register form: calls sub_8125444 outside this file, which has no typed C interface yet */
static void call_8125444(void) {
    GUEST_CALL(sub_8125444);
}

/* register form: calls sub_8126114 outside this file, which has no typed C interface yet */
static void call_8126114(void) {
    GUEST_CALL(sub_8126114);
}

/* register form: calls sub_8126554 outside this file, which has no typed C interface yet */
static void call_8126554(void) {
    GUEST_CALL(sub_8126554);
}

/* register form: calls sub_81267F0 outside this file, which has no typed C interface yet */
static void call_81267F0(void) {
    GUEST_CALL(sub_81267F0);
}

/* register form: IsScreenFadeActive's condition flags must reach the caller
 * verbatim when Z is set (fade inactive → beq taken), but must be partially
 * overwritten by MOV R0,#8's N/Z when Z is clear (fade active → store path).
 * A single SET_FLAGS call in the adapter cannot express both paths, and the
 * C/V flags always originate from IsScreenFadeActive regardless of branch. */
static u32 fade_check_and_store(u32 battle_obj) {
    GUEST_CALL(IsScreenFadeActive);
    if (ZERO_FLAG())
        return R0;                /* fade inactive: beq, flags already correct */
    R0 = 8;
    ST8(battle_obj + 1, 8);       /* advance phase byte */
    SET_FLAGS_NZ(R0);             /* mov r0, #8 → N=0, Z=0; C,V unchanged */
    return 8;
}

/* Run one tick of the battle visual-transition sequence.
 * Calls six stage-update routines, then checks whether the screen-fade
 * animation is still playing. If it is, writes phase byte 8 into the
 * battle object at [r5+1]. Returns the resulting R0. */
static u32 battle_transition_tick(u32 battle_obj) {
    call_812674C();
    call_81267A8();
    call_8125444();
    call_8126114();
    call_8126554();
    call_81267F0();
    return fade_check_and_store(battle_obj);
}

/* sub_8125170(r5 = battle-object pointer) -> r0; flags from IsScreenFadeActive path */
Next sub_8125170(void) {
    R0 = battle_transition_tick(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8125254 ── */
#define EVENT_CHATBOX_FLAG 0x1711

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8125254(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();  /* Z set → flag clear; Z clear → flag set */
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void event_flag_clear(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void chatbox_close(void) {
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls chatbox_clear_eFlags2009F38 outside this file, which has no typed C interface yet */
static void chatbox_clear_eflags(u32 bits) {
    R0 = bits;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
}

/* register form: calls pet_menuExitToMap_811F708 outside this file, which has no typed C interface yet */
static void pet_menu_exit_to_map(void) {
    GUEST_CALL(pet_menuExitToMap_811F708);
}

/* Handle the chatbox/pet-menu decision gated on EVENT_CHATBOX_FLAG.
 * If the flag is set: close the chatbox, clear eFlags bit 0x40, clear the flag, return 1.
 * Otherwise: exit the pet menu back to the map, return 0. */
static u32 handle_chatbox_flag_action(void) {
    if (event_flag_set_sub_8125254(EVENT_CHATBOX_FLAG)) {
        chatbox_close();
        chatbox_clear_eflags(0x40);
        event_flag_clear(EVENT_CHATBOX_FLAG);
        return 1;
    }
    pet_menu_exit_to_map();
    return 0;
}

/* sub_8125254() -> r0, nz (c,v left by last callee) */
Next sub_8125254(void) {
    R0 = handle_chatbox_flag_action();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── initRefs_buildAndDecompressTable_8125280 ── */
/* initRefs_buildAndDecompressTable_8125280: assemble the "init references" table in RAM at unk_2029000 from
 * the ROM pointer table at off_8125310, then decompress-and-copy the two data
 * blobs that follow it.
 *
 * The source table holds 5 entries of 4 words each (0x50 bytes total):
 *   word 0..1 - passed to sub_811FB84, whose result is stored instead
 *   word 2..3 - copied verbatim
 * A final 0 word terminates the assembled table. r4-r7 are preserved. */
Next initRefs_buildAndDecompressTable_8125280(void) {
    u32 src = A_off_8125310;  /* init-ref entries in ROM */
    u32 dst = A_unk_2029000;  /* assembled table in RAM */

    for (u32 entry = 0; entry < 0x50 / 16; entry++) {
        R0 = LD32(src);
        src += 4;
        R1 = LD32(src);
        src += 4;
        GUEST_CALL(sub_811FB84);
        ST32(dst, R0);
        dst += 4;

        for (u32 word = 0; word < 2; word++) {
            ST32(dst, LD32(src));
            dst += 4;
            src += 4;
        }
    }
    ST32(dst, 0);

    /* decompress the assembled table, then the blob described by it */
    R0 = A_unk_2029000;
    GUEST_CALL(decompAndCopyData);
    R0 = A_byte_8125368;
    GUEST_CALL(decompAndCopyData);

    RETURN_TO_CALLER();
}

/* ── menu_confirmExitAndFade_812580C ── */
/* menu_confirmExitAndFade_812580C(r0 = value stored into the menu state at [r5+2]):
 * record the caller's selection, play the "exit submenu" sound effect,
 * then start a screen fade (fade kind 0xC, duration 0x10).
 * Only r0/r1/lr and the flags are clobbered; r3 and r10 pass through. */
Next menu_confirmExitAndFade_812580C(void) {
    u8 selection = (u8)R0;

    ST8(R5 + 2, selection);

    R0 = SOUND_EXIT_SUBMENU;
    GUEST_CALL(PlaySoundEffect);

    R0 = 0xC;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    RETURN_TO_CALLER();
}

/* ── playSoundEffect_7A_81258E8 ── */
/* playSoundEffect_7A_81258E8: play sound effect 0x7A. All other registers (r1-r7, r10)
 * pass through to the callee and are preserved unchanged. */
Next playSoundEffect_7A_81258E8(void) {
    R0 = 0x7A;
    GUEST_CALL(PlaySoundEffect);
    RETURN_TO_CALLER();
}

/* ── sub_81258F8 ── */
/* Chip availability table: 8 bytes per slot (u16 chip_id, u8 status, u8 pad) */
#define CHIP_TABLE_ADDR        A_unk_201DC20
#define CHIP_TABLE_SIZE        0x320
#define CHIP_COUNT_MAX_sub_81258F8         0x13F

/* Event-flag ranges used to classify chip ownership state */
#define CHIP_FLAG_OWNED        0x1E20   /* already in the player's library */
#define CHIP_FLAG_ACQUIRED     0x2220   /* newly acquired this run */
#define CHIP_FLAG_CAN_ACQUIRE  0x2020   /* available to acquire */
#define CHIP_FLAG_ALREADY_GOT  0x1711   /* "has ever been obtained" check */

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word(u32 addr, u32 size) {
    R0 = addr;
    R1 = size;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_81258F8(s32 chip_id) {
    R0 = (u32)chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_81258F8(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_from_immediate(u32 page, u32 bit) {
    R0 = page;
    R1 = bit;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

typedef struct {
    s32 count;
    u32 r2_out;   /* leftover r2 from the loop's last iteration (output register) */
} ChipTableResult;

/* Build the chip availability table in RAM.
 *
 * Iterates chips 1..319. A chip qualifies when:
 *   - byte 0x09 has bit 0x40 (special-type chip)
 *   - byte 0x16 bit 0 is clear (not disabled)
 *   - byte 0x07 == 0         (category zero only)
 *
 * For each qualifying chip the group index (byte 0x15 − 1) selects a 4-byte slot.
 * The slot stores the chip id (u16) and a status byte at offset +2:
 *   0 = already owned, 1 = newly acquired, 2 = acquirable but previously obtained.
 *
 * Returns the count written and the final value of r2 left by the loop
 * (0x40 when the last chip failed the special-bit test, 1 otherwise). */
static ChipTableResult build_chip_availability_table(void) {
    zero_fill_by_word(CHIP_TABLE_ADDR, CHIP_TABLE_SIZE);

    s32 count = 0;
    u32 r2 = 0;

    for (s32 chip = 1; chip <= CHIP_COUNT_MAX_sub_81258F8; chip++) {
        u32 data = get_chip_data_sub_81258F8(chip);

        /* Original sets r2 = 0x40 unconditionally after the call. */
        r2 = 0x40;
        if (!(LD8(data + 9) & 0x40))
            continue;

        /* Original sets r2 = 1 for the disabled-bit test. */
        r2 = 1;
        if (LD8(data + 0x16) & 1)
            continue;

        if (LD8(data + 7) != 0)
            continue;

        u32 group = (u32)LD8(data + 0x15) - 1;
        u32 entry = CHIP_TABLE_ADDR + (group << 2);
        count++;

        if (event_flag_set_sub_81258F8(CHIP_FLAG_OWNED + (u32)chip)) {
            ST16(entry, (u32)chip);
            ST8(entry + 2, 0);
        } else if (event_flag_set_sub_81258F8(CHIP_FLAG_ACQUIRED + (u32)chip)) {
            ST16(entry, (u32)chip);
            ST8(entry + 2, 1);
        } else if (event_flag_set_sub_81258F8(CHIP_FLAG_CAN_ACQUIRE + (u32)chip)) {
            ST16(entry, (u32)chip);
            ST8(entry + 2,
                event_flag_set_from_immediate(
                    CHIP_FLAG_ALREADY_GOT >> 8,
                    CHIP_FLAG_ALREADY_GOT & 0xFF) ? 2 : 1);
        }
    }

    return (ChipTableResult){count, r2};
}

/* sub_81258F8() -> r0 = chip count, r2 = loop leftover */
Next sub_81258F8(void) {
    ChipTableResult res = build_chip_availability_table();
    R0 = (u32)res.count;
    R2 = res.r2_out;
    RETURN_TO_CALLER();
}

/* ── sub_8125994 ── */
/* Collect available chips into a category-indexed buffer, then compact it.
 *
 * Scans chip IDs 1..0x13F. A chip qualifies if its kind (field 7) is 1,
 * it carries attribute bit 0x40 (field 9), is not hidden (field 0x16 bit 0),
 * and one of three event-flag ranges is unlocked for it. The chip ID and a
 * category byte (0, 1, or 2) are written into a per-type slot in the buffer
 * at dword_201E420. After the scan, empty (zero) slots are compacted away
 * by pulling subsequent entries forward.
 *
 * Returns the number of chips that passed the qualification tests. */

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_sub_8125994(u32 mem, u32 byte_size) {
    R0 = mem;
    R1 = byte_size;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 chip_data_ptr(s32 chip_id) {
    R0 = (u32)chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8125994(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool flag_1711_set(void) {
    R0 = 0x17;
    R1 = 0x11;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

static s32 scan_chips_into_buffer(void) {
    const u32 buf_base = A_dword_201E420;
    const u32 buf_end  = A_unk_201E998;
    const s32 MAX_CHIPS = 0x13F;

    /* 0x578 bytes = 350 four-byte slots, one per possible chip type */
    zero_fill_sub_8125994(buf_base, 0x578);

    s32 count = 0;

    for (s32 id = 1; id <= MAX_CHIPS; id++) {
        u32 data = chip_data_ptr(id);

        /* Slot offset in the buffer is determined by chip usage-type (field 0x15). */
        u32 slot_off   = ((u32)LD8(data + 0x15) - 1) << 2;
        u32 write_addr = buf_base + slot_off;

        /* Chip must have "available" attribute (bit 0x40 in field 9). */
        if ((LD8(data + 9) & 0x40) == 0)
            continue;

        /* Chip must not be hidden (bit 0 in field 0x16). */
        if ((LD8(data + 0x16) & 1) != 0)
            continue;

        /* Chip kind (field 7) must equal 1 (normal chip). */
        if ((u32)LD8(data + 7) != 1)
            continue;

        /* Three-tier event-flag gate, checked in priority order. */
        if (event_flag_set_sub_8125994((u32)(0x1E20 + id))) {
            /* Category 0: early availability flag unlocked. */
            count++;
            ST16(write_addr, (u16)id);
            ST8(write_addr + 2, 0);
        } else if (event_flag_set_sub_8125994((u32)(0x2220 + id))) {
            /* Category 1: mid-game flag unlocked. */
            count++;
            ST16(write_addr, (u16)id);
            ST8(write_addr + 2, 1);
        } else if (event_flag_set_sub_8125994((u32)(0x2020 + id))) {
            /* Category 1 or 2 depending on special flag 0x1711. */
            count++;
            ST16(write_addr, (u16)id);
            ST8(write_addr + 2, flag_1711_set() ? 2 : 1);
        }
    }

    /* Compact: for each zero slot, pull subsequent non-zero entries forward. */
    for (s32 i = 0; i < MAX_CHIPS; i++) {
        u32 pos = buf_base + (u32)(i * 4);
        if (LD32(pos) != 0)
            continue;

        u32 dst = pos;
        for (u32 src = pos + 4; src < buf_end; src += 4) {
            u32 val = LD32(src);
            if (val == 0)
                continue;
            ST32(dst, val);
            ST32(src, 0);
            dst += 4;
        }
    }

    return count;
}

/* sub_8125994() -> r0 = chip count, r2 = 0 (left by compaction loop) */
Next sub_8125994(void) {
    R0 = (u32)scan_chips_into_buffer();
    R2 = 0;   /* the compaction inner loop always leaves r2 cleared */
    RETURN_TO_CALLER();
}

/* ── sub_8125A6C ── */
/* Field offsets within the ChipData struct (base returned by getChip8021DA8). */
#define oChipData_PackType   0x15
#define oChipData_Flags      9
#define oChipData_Type       7

#define CHIP_COUNT_MAX_sub_8125A6C       0x13F
#define PACK_DATA_BASE       A_dword_201EC20
#define PACK_DATA_SIZE       0x578
#define PACK_DATA_END        A_unk_201F198

/* Event-flag page bases (per-chip offset = 0x1E20 + idx, etc.). */
#define FLAG_READY_BASE      0x1E20
#define FLAG_USED_BASE       0x2220
#define FLAG_UNUSED_BASE     0x2020
#define SPECIAL_FLAG_PAGE    0x17
#define SPECIAL_FLAG_OFFSET  0x11

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_sub_8125A6C(u32 addr, u32 size) {
    R0 = addr;
    R1 = size;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_ptr(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8125A6C(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_from_pair(u8 page, u8 offset) {
    R0 = page;
    R1 = offset;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/*
 * Build the chip-pack availability table for the chip library screen.
 *
 * Zero the destination buffer, then scan every chip (1..0x13F).  A chip
 * qualifies if its flags byte has bit 6 set and its type field equals 2.
 * Each qualifying chip is placed into a slot determined by (PackType - 1),
 * storing the chip index (u16) and a readiness code (u8 at +2):
 *   0 – "ready"     (flag base 0x1E20)
 *   1 – "used"      (flag base 0x2220) or upgrade when special flag is set
 *   2 – "unused" + special flag EVENT_17:11 is set
 *
 * After filling, compact the slot array so non-zero entries move to the
 * front with gaps removed.
 *
 * Returns the number of accepted entries.  r2 after compaction is always 0
 * (the inner loop's last write to r2 is either a loaded zero or an explicit
 * `mov r2, #0`).
 */
static u32 build_pack_data(void) {
    zero_fill_sub_8125A6C(PACK_DATA_BASE, PACK_DATA_SIZE);

    u32 count = 0;

    for (u32 idx = 1; idx <= CHIP_COUNT_MAX_sub_8125A6C; idx++) {
        u32 chip = get_chip_ptr(idx);
        u32 slot_off = ((u32)LD8(chip + oChipData_PackType) - 1) << 2;
        u32 flags = LD8(chip + oChipData_Flags);

        if (!(flags & 0x40))
            continue;

        u32 slot_addr = PACK_DATA_BASE + slot_off;

        if ((u32)LD8(chip + oChipData_Type) != 2)
            continue;

        if (event_flag_set_sub_8125A6C(FLAG_READY_BASE + idx)) {
            count++;
            ST16(slot_addr, idx);
            ST8(slot_addr + 2, 0);
        } else if (event_flag_set_sub_8125A6C(FLAG_USED_BASE + idx)) {
            count++;
            ST16(slot_addr, idx);
            ST8(slot_addr + 2, 1);
        } else if (event_flag_set_sub_8125A6C(FLAG_UNUSED_BASE + idx)) {
            count++;
            ST16(slot_addr, idx);
            ST8(slot_addr + 2,
                event_flag_set_from_pair(SPECIAL_FLAG_PAGE, SPECIAL_FLAG_OFFSET) ? 2 : 1);
            /* The original re-reads the stored halfword and skips advancing
             * the write cursor only when it is zero; since idx >= 1 it is
             * never zero, so the "add r6, #4" path is unreachable here. */
        }
    }

    /* Compact: for each empty slot, shift the next non-zero entry into it. */
    for (u32 i = 0; i < CHIP_COUNT_MAX_sub_8125A6C; i++) {
        u32 cur = PACK_DATA_BASE + i * 4;
        if (LD32(cur) != 0)
            continue;

        u32 dst = cur;
        for (u32 src = cur + 4; (s32)src < (s32)PACK_DATA_END; src += 4) {
            u32 val = LD32(src);
            if (val == 0)
                continue;
            ST32(dst, val);
            ST32(src, 0);
            dst += 4;
        }
    }

    return count;
}

/* sub_8125A6C() -> r0 = accepted chip count, r2 = 0 (compaction scratch).
 * r3 is a callee side-effect from the last TestEventFlag/…FromImmediate call.
 * r4-r7 are preserved by the original's push/pop (never modified here). */
Next sub_8125A6C(void) {
    R0 = build_pack_data();
    R2 = 0;
    RETURN_TO_CALLER();
}

/* ── sub_8125C08 ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_sub_8125C08(u32 addr, u32 bytes) {
    R0 = addr;
    R1 = bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet.
 * Preserves r3-r7; clobbers r1, r2 (both overwritten by caller). */
static u32 get_chip_data_sub_8125C08(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet.
 * Preserves r2, r4-r7; clobbers r3 (returned so the adapter can reproduce it). */
static bool event_flag_test(u32 flag, u32 *out_r3) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    *out_r3 = R3;
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet.
 * Preserves r2, r4-r7; clobbers r3 (returned). */
static bool event_flag_immediate(u32 page, u32 idx, u32 *out_r3) {
    R0 = page;
    R1 = idx;
    GUEST_CALL(TestEventFlagFromImmediate);
    *out_r3 = R3;
    return !ZERO_FLAG();
}

typedef struct {
    u32 count;
    u32 r2;
    u32 r3;
} FilterResult;

/* Build the COM-chip availability filter.
 * Scans chips 0x140..0x15E; for each COM-type chip whose event flag is set,
 * records the chip ID and a status byte in a packed slot.  Then compacts
 * the word array by shifting non-zero entries toward the front.
 * Returns chip count (r0), final r2 (compaction scratch), final r3 (loop scratch). */
static FilterResult build_chip_filter(void) {
    const u32 base = A_dword_201FC20;
    zero_fill_sub_8125C08(base, 0x578);

    u32 count = 0;
    u32 r2 = 0;
    u32 r3 = 0x140;

    /* Main pass: scan 31 chips (0x140..0x15E) */
    for (s32 i = 0; i <= 0x1E; i++) {
        u32 chip_id = 0x140 + i;
        r3 = 0x140;

        u32 data = get_chip_data_sub_8125C08(chip_id);

        u32 kind = LD8(data + 0x15);
        r2 = ((kind - 1) << 2) + base;

        if (LD8(data + 7) != 4)
            continue;

        /* COM chip: check three flag ranges */
        if (event_flag_test(0x1F60 + i, &r3)) {
            count++;
            r3 = chip_id;
            ST16(r2, r3);
            ST8(r2 + 2, 0);
            continue;
        }

        u32 flag_a = 0x2220 + i + (0x1F60 - 0x1E20);
        if (event_flag_test(flag_a, &r3)) {
            count++;
            r3 = chip_id;
            ST16(r2, r3);
            ST8(r2 + 2, 1);
            continue;
        }

        u32 flag_b = 0x2020 + i + (0x1F60 - 0x1E20);
        if (!event_flag_test(flag_b, &r3))
            continue;

        count++;
        r3 = chip_id;
        ST16(r2, r3);

        u32 status = event_flag_immediate(0x17, 0x11, &r3) ? 2 : 1;
        ST8(r2 + 2, status);
    }

    /* Compaction pass: shift non-zero words forward, zero the tail */
    const u32 end = A_unk_2020198;
    for (s32 i = 0; i < 0x13F; i++) {
        u32 pos = base + i * 4;
        if (LD32(pos) != 0)
            continue;

        u32 dest = pos;
        for (u32 src = pos + 4; (s32)src < (s32)end; src += 4) {
            r2 = LD32(src);
            if (r2 == 0)
                continue;
            ST32(dest, r2);
            ST32(src, 0);
            dest += 4;
            r2 = 0;
        }
    }

    FilterResult res = { count, r2, r3 };
    return res;
}

/* sub_8125C08(r2 r3 r4 r5 r6 r7 r10 lr in) -> (r0 r2 r3 r4 r5 r6 r7 out; r4-r7 preserved) */
Next sub_8125C08(void) {
    FilterResult res = build_chip_filter();
    R0 = res.count;
    R2 = res.r2;
    R3 = res.r3;
    RETURN_TO_CALLER();
}

/* ── sub_8125E78 ── */
/* Background tile drawing for the battle arena, dispatching on the object's
 * alliance type (offset 0x60). Reads tile id at +0x2c and row at +0x74. */

/* register form: calls sub_81261FC outside this file, which has no typed C interface yet */
static void call_init_bg(u16 tile_id, u32 tileset) {
    R0 = tile_id;
    R1 = tileset;
    GUEST_CALL(sub_81261FC);
}

/* register form: calls sub_812625C outside this file, which has no typed C interface yet */
static void call_set_tile_block(u32 row, u32 width, u32 tileset, u32 tile) {
    R0 = row;
    R1 = width;
    R2 = tileset;
    R3 = tile;
    GUEST_CALL(sub_812625C);
}

/* register form: sub_812626C takes 6 args (r0-r5); save caller's r4, r5 */
static void call_place_tile(u16 tile_id, u32 map, u32 tileset,
                            u32 x, u32 y, u32 tile_num) {
    u32 saved_r4 = R4, saved_r5 = R5;
    R0 = tile_id;
    R1 = map;
    R2 = tileset;
    R3 = x;
    R4 = y;
    R5 = tile_num;
    GUEST_CALL(sub_812626C);
    R4 = saved_r4;
    R5 = saved_r5;
}

/* register form: calls bg_drawTileStripTileBlock2_8126404 outside this file */
static void call_draw_strip(u32 row, u32 width, u32 tileset, u32 tile) {
    R0 = row;
    R1 = width;
    R2 = tileset;
    R3 = tile;
    GUEST_CALL(bg_drawTileStripTileBlock2_8126404);
}

/* register form: sub_8126430 takes 5 args (r0-r4); save caller's r4 */
static void call_copy_tilemap(u16 tile_id, u32 map, u32 tileset,
                              u32 src, u32 dst) {
    u32 saved_r4 = R4;
    R0 = tile_id;
    R1 = map;
    R2 = tileset;
    R3 = src;
    R4 = dst;
    GUEST_CALL(sub_8126430);
    R4 = saved_r4;
}

/* register form: calls bg_drawTileRegion2x14_8126484 outside this file */
static void call_draw_region(u32 row, u32 width, u32 tileset) {
    R0 = row;
    R1 = width;
    R2 = tileset;
    GUEST_CALL(bg_drawTileRegion2x14_8126484);
}

/* register form: calls sub_8126494 outside this file, which has no typed C interface yet */
static void call_update_map(u16 tile_id, u32 map, u32 tileset) {
    R0 = tile_id;
    R1 = map;
    R2 = tileset;
    GUEST_CALL(sub_8126494);
}

/* register form: calls bg_copyTilePatchBlock2_81264E4 outside this file */
static void call_copy_patch(u32 row, u32 width, u32 tileset) {
    R0 = row;
    R1 = width;
    R2 = tileset;
    GUEST_CALL(bg_copyTilePatchBlock2_81264E4);
}

static void draw_arena_tiles(u32 obj) {
    u32 alliance = LD32(obj + 0x60);
    if (alliance == 0)
        return;

    u16 tile_id = (u16)LD16(obj + 0x2c);
    u32 row = LD32(obj + 0x74) - 0x71;

    if (alliance == 5) {
        /* Dark Chip team layout */
        call_init_bg(tile_id, A_byte_2017A00);
        call_set_tile_block(row + 1, 4, A_byte_2017A00, 5);
        call_place_tile(tile_id, A_dword_201EC20, A_byte_2017A00, 0xa, 0xb, 0x70);
        call_draw_strip(row + 6, 4, A_byte_2017A00, 0xa);
        call_copy_tilemap(tile_id, A_dword_201EC20, A_byte_2017A00,
                          A_byte_8124A14, 0x6007400);
        call_draw_region(row + 4, 4, A_byte_2017A00);
        call_update_map(tile_id, A_dword_201EC20, A_byte_2017A00);
        call_copy_patch(row + 0xe, 4, A_byte_2017A00);
    } else {
        /* Regular team layout */
        call_place_tile(tile_id, A_dword_201EC20, A_byte_2017A00, 0xa, 0xb, 0x71);
        call_draw_strip(row + 3, 4, A_byte_2017A00, 0xa);
        call_copy_tilemap(tile_id, A_dword_201EC20, A_byte_2017A00,
                          A_byte_8124A14, 0x6007400);
        call_draw_region(row + 1, 4, A_byte_2017A00);
        call_update_map(tile_id, A_dword_201EC20, A_byte_2017A00);
        call_copy_patch(row + 0xb, 4, A_byte_2017A00);
    }
}

/* sub_8125E78(r5 = arena object ptr) */
Next sub_8125E78(void) {
    draw_arena_tiles(R5);
    RETURN_TO_CALLER();
}
