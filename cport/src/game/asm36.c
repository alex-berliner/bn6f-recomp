/*
 * asm36.c: readable functions of bn6f asm36.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── chatbox_runScript_803FD9C_on_eTextScript201BA20 ── */
/* chatbox_runScript_803FD9C_on_eTextScript201BA20(r0 = script index):
 * run the fixed text script eTextScript201BA20 in the chatbox, starting at
 * the given index. A thin wrapper around chatbox_runScript_803FD9C that just
 * supplies the archive pointer. */
Next chatbox_runScript_803FD9C_on_eTextScript201BA20(void) {
    u32 scriptIdx = R0;

    R0 = A_eTextScript201BA20; /* TextScriptArchive *archive */
    R1 = scriptIdx;            /* u8 scriptIdx */

    GUEST_CALL(chatbox_runScript_803FD9C);

    RETURN_TO_CALLER();
}

/* ── sub_81322B8 ── */
/* sub_81322B8(r5 = pointer, returns table byte):
 * Reads r5->offset 0x12 as an index, returns byte_81322C4[index] in R0.
 * R1 is set to the base address of the lookup table (byte_81322C4). */
Next sub_81322B8(void) {
    u32 index = LD8(R5 + 0x12);
    R1 = A_byte_81322C4;
    R0 = LD8(R1 + index);
    RETURN_TO_CALLER();
}

/* ── sub_81322C8 ── */
/* sub_81322C8:
 *   inputs:  r5
 *   outputs: r0, r1
 *   Reads a field of the object pointed to by r5 and looks up a byte in a
 *   small table; r0 gets the indexed byte and r1 points at the table base. */
Next sub_81322C8(void) {
    u8 idx = LD8(R5 + 0x12);
    R1 = A_byte_81322D4;
    R0 = LD8(R1 + idx);
    RETURN_TO_CALLER();
}

/* ── sub_81322D8 ── */
/* sub_81322D8(r5 = object):
 *   Reads byte at offset 0x12 of the caller's struct (some kind of
 *   small int / sub-index), then uses it to index into a 4-entry
 *   constant table at byte_81322E8. Returns the table byte in r0
 *   and the base address of the table in r1.
 */
Next sub_81322D8(void) {
    u32 index = LD8(R5 + 0x12);
    R1 = (u32)A_byte_81322E4;
    R0 = LD8(R1 + index);
    RETURN_TO_CALLER();
}

/* ── sub_81322E8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sub_803E8F8 has no typed C interface yet */
static inline u32 call_sub_803E8F8(void) {
    GUEST_CALL(sub_803E8F8);
    return R0;
}

/* Class-method dispatcher.
 *
 * sub_803E8F8 looks up the runtime record for the object (its result is the
 * value the eventual handler receives in r2). The byte at state+0x0E is a
 * method index; the table at off_8132304 holds one thunk address per index.
 * Control is transferred to that thunk (mov lr, pc; bx r0), and whatever it
 * leaves in r2..r12 is what our callers see — this function itself only
 * sets the call up and lets the guest state flow through.
 *
 * Callee entry state (as in the original at the bx):
 *   r0 = the thunk address just loaded,
 *   r1 = table base (off_8132304),
 *   r2 = sub_803E8F8's result,
 *   r3..r12 = untouched from our caller (sub_803E8F8 preserves r1/r2/r5,
 *     so r5 still points at the caller's state block).
 */

/* register form: indirect call through a code address (mov lr, pc; bx r0).
 * The thunk must be entered with a specific guest register image, which no
 * typed wrapper can express; the helper builds it and makes the one call. */
static void call_method_thunk(u32 target, u32 table_base, u32 lookup_result) {
    R0 = target;
    R1 = table_base;
    R2 = lookup_result;
    GUEST_CALL_ADDR(target);
}

/* r5 = object state pointer */
static void run_class_dispatch(u32 state) {
    u32 record = call_sub_803E8F8();
    u32 index  = LD8(state + 0xE);
    u32 thunk  = LD32(A_off_8132304 + (index << 2));
    call_method_thunk(thunk, A_off_8132304, record);
}

Next sub_81322E8(void) {
    run_class_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8132310 ── */
/* Menu/object state dispatcher: the byte at object+0x12 selects one of several
 * handlers from the jump table at jt_8132324. The chosen handler is called and
 * its register results (r2-r12) flow straight through to this routine's own
 * caller; r0, r1 and the flags are scratch. */

/* register form: one-shot helper for a call through a code address (the
 * original's "mov lr, pc; bx rN"). It loads R0/R1 with the handler address and
 * jump-table base exactly as the original leaves them, and replicates the
 * original's push {lr} so the callee runs with the same guest SP and the same
 * leftover stack word is left behind in memory. */
static void call_menu_handler(u32 handler, u32 tableBase, u32 returnAddress) {
    R0 = handler;
    R1 = tableBase;
    SP -= 4;
    ST32(SP, returnAddress);
    GUEST_CALL_ADDR(handler);
    SP += 4;
}

/* Read the object's state byte, pick the matching handler from the jump table
 * and call it. r2 and r5 reach the handler untouched through the guest
 * register file, and whatever the handler leaves in r2-r12 is the result. */
static void dispatch_menu_handler(u32 objectPtr, u32 returnAddress) {
    u32 state = LD8(objectPtr + 0x12);
    u32 handler = LD32(A_jt_8132324 + (state << 2));
    call_menu_handler(handler, A_jt_8132324, returnAddress);
}

/* sub_8132310(r2 = argument, r5 = object, lr = caller return) -> r2..r12 */
Next sub_8132310(void) {
    dispatch_menu_handler(R5, LR);
    RETURN_TO_CALLER();
}

/* ── sub_8132330 ── */
/* sub_8132330(r2 = dest, r5 = src):
 * Copy src[0x13] (an index, treated as u8) into a u16 lookup table entry, written to dest+0xA. */
Next sub_8132330(void) {
    u32 index = LD8(R5 + 0x13);
    u32 lookup = LD16(A_byte_8132340 + (index << 1));
    ST16(R2 + 0xA, lookup);
    RETURN_TO_CALLER();
}

/* ── sub_8132344 ── */
/* sub_8132344(r2 = dst field offset 0xA, r5 = struct pointer):
 * reads byte at r5+0x13, uses it as index (scaled by 2) into table byte_8132354,
 * and stores the resulting u16 at dst+0xA. */
Next sub_8132344(void) {
    const u32 tbl = A_byte_8132354;
    u32 idx = LD8(R5 + 0x13);
    u32 entry = LD16(tbl + (idx << 1));
    ST16(R2 + 0xA, entry);
    RETURN_TO_CALLER();
}

/* ── sub_8132358 ── */
/* sub_8132358: Look up a halfword from a small table indexed by a byte
 * field of the object pointed to by r5, and store it at offset 0xA in
 * the structure pointed to by r2. */
Next sub_8132358(void) {
    u32 index = LD8(R5 + 0x13);
    u32 entry = LD16(A_byte_8132368 + (index << 1));
    ST16(R2 + 0xA, entry);
    RETURN_TO_CALLER();
}

/* ── sub_813236C ── */
/* sub_813236C(r2 = dst, r5 = src):
 * read a u8 from src+0x12, use it as an index into a 2-byte-per-entry table at byte_813237C,
 * and store the resulting u16 at dst+0xa. */
Next sub_813236C(void) {
    u8 index = LD8(R5 + 0x12);
    u16 value = LD16(A_byte_813237C + (index << 1));
    ST16(R2 + 0xa, value);
    RETURN_TO_CALLER();
}

/* ── sub_8132380 ── */
/* sub_8132380(r2 = destination addr, r5 = source struct):
 *   Read a byte from source+0x12, use it as an index into the lookup table
 *   at byte_8132390 (each entry is a halfword), and store the resulting
 *   halfword at destination+0xA. */
Next sub_8132380(void) {
    u32 index = LD8(R5 + 0x12);
    u32 table = A_byte_8132390;
    u32 value = LD16(table + index * 2);
    ST16(R2 + 0xA, value);
    RETURN_TO_CALLER();
}

/* ── sub_81325E4 ── */
/* sub_81325E4(r0 = index): fetch a byte from the lookup table at dword_81325F0. */
Next sub_81325E4(void) {
    R1 = A_dword_81325F0;
    R0 = LD8(R1 + R0);
    RETURN_TO_CALLER();
}

/* ── sub_81325F4 ── */
/* sub_81325F4(r0 = index, lr):
 *   Look up a byte from the table at byte_8132600 indexed by r0.
 *   Returns the byte in r0. r1 ends up pointing at the table base. */
Next sub_81325F4(void) {
    R1 = A_byte_8132600;
    R0 = LD8(R1 + R0);
    RETURN_TO_CALLER();
}

/* ── sub_8132614 ── */
/* sub_8132614: Initialize the menu/state record pointed to by R5.
 * inputs:  R5 = object pointer
 * outputs: writes [R5+1] = 8, [R5+2] = dispatch result, [R5+3] = 4
 *
 * Calls sub_803EBB8 first, then dispatches on sub_8132290's return value:
 *   0x18 / 0x1C -> replace field with sub_81325F4(LD8(R5+0x12))
 *   0x20 / 0x24 -> field = 0x0C
 *   default     -> field = 0x10
 */
Next sub_8132614(void) {
    GUEST_CALL(sub_803EBB8);
    ST8(R5 + 1, 8);

    GUEST_CALL(sub_8132290);
    u32 result = R0;

    /* Pick the value written to [R5+2] from the dispatch table. */
    if (result == 0x18 || result == 0x1C) {
        SET_FLAGS_CMP(result, (result == 0x18) ? 0x18 : 0x1C);
        R0 = LD8(R5 + 0x12);
        GUEST_CALL(sub_81325F4);
    } else if (result == 0x20 || result == 0x24) {
        SET_FLAGS_CMP(result, (result == 0x20) ? 0x20 : 0x24);
        R0 = 0x0C;
    } else {
        SET_FLAGS_CMP(result, 0x24);
        R0 = 0x10;
    }

    ST8(R5 + 2, R0);
    /* The original ends with `mov r0, #4` followed by `strb r0, [r5,#3]`,
     * so R0 leaves as 4 (not the last dispatch value). */
    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 3, R0);

    RETURN_TO_CALLER();
}

/* ── HandleSaveMenu8132B88 ── */
/* HandleSaveMenu8132B88: dispatcher for the save-menu scene.
 *
 * R5 points at the save-menu work area; the byte at offset 1 holds the
 * scene's current state. The original uses that byte *directly* as a byte
 * offset into the jump table (no <<2), so the stored values are the word
 * offsets themselves:
 *   0 = OpenSaveMenu8132BA8  - open the save menu
 *   4 = SaveMenuUpdate8132C34 - run the save menu (update)
 *   8 = ExitSaveMenu8132CA0  - leave the save menu (called repeatedly
 *                              while the screen dims)
 *
 * At the call the original has R0 = the raw table entry (the handler's
 * address, thumb bit set) and R1 = the state byte; both are reproduced here
 * even though the handlers take no arguments. The handler returns to this
 * function (the `mov lr, pc` / `pop {pc}` pair), which then returns to this
 * function's caller, so the handler's registers and flags pass through.
 */
Next HandleSaveMenu8132B88(void) {
    u32 state = LD8(R5 + 1);

    /* Reproduce the original's table lookup: the raw byte indexes the table
     * by bytes, and the loaded handler pointer ends up in R0. */
    R0 = LD32(A_SaveMenuJumpTable8132B9C + state);
    R1 = state;

    switch (state) {
    case 0: GUEST_CALL(OpenSaveMenu8132BA8);   break;
    case 4: GUEST_CALL(SaveMenuUpdate8132C34); break;
    case 8: GUEST_CALL(ExitSaveMenu8132CA0);   break;
    }

    RETURN_TO_CALLER();
}

/* ── SaveMenuUpdate8132C34 ── */
/* SaveMenuUpdate8132C34: save-menu state dispatcher.
 *
 * R5 points to the save-menu work area; the byte at offset 2 holds the current
 * menu state as a byte offset into JumpTable8132C48 (the original does a plain
 * `ldr r0, [table, state]`, so the stored value is already 4 bytes per entry):
 *   0 -> SaveMenuOpenUpdate8132C50 (menu opening)
 *   4 -> SaveMenuUpdate8132C68     (menu running)
 *
 * The selected handler is entered with r0 = its own table word (including the
 * Thumb bit, exactly as loaded from ROM) and r1 = the raw state byte. It runs
 * as a tail call: the original reaches it via `mov lr, pc; bx r0` and its
 * `pop {pc}` sends the handler's results, registers and flags straight back
 * to this function's caller. */
Next SaveMenuUpdate8132C34(void) {
    u32 state = LD8(R5 + 2);

    /* Reproduce the original's register state at the indirect call:
     * r1 = state byte, r0 = handler address read from the jump table. */
    R1 = state;
    R0 = LD32(A_JumpTable8132C48 + state);

    switch (state) {
    case 0: /* menu is opening */
        GUEST_CALL(SaveMenuOpenUpdate8132C50);
        break;
    case 4: /* menu is active */
        GUEST_CALL(SaveMenuUpdate8132C68);
        break;
    default:
        /* The table only has two entries; no other state occurs in play. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── SaveMenuOpenUpdate8132C50 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_SaveMenuOpenUpdate8132C50(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();   /* Z set → fade is not active */
}

/* register form: calls chatbox_clear_eFlags2009F38 outside this file, which has no typed C interface yet */
static void clear_chatbox_flags(u32 eflags) {
    R0 = eflags;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
}

/* register form: calls saveMenuTextGfx_8132F4C outside this file, which has no typed C interface yet */
static void call_save_menu_text_gfx(void) {
    GUEST_CALL(saveMenuTextGfx_8132F4C);
}

/* While the screen is fading, clear the chatbox eFlags and mark the save-menu
 * state byte.  Always refresh the save-menu text graphics at the end.
 * menu_ptr is the save-menu object (r5), preserved by every callee. */
static void save_menu_open_update(u32 menu_ptr) {
    if (screen_fade_active_SaveMenuOpenUpdate8132C50()) {
        clear_chatbox_flags(0x40);
        ST8(menu_ptr + 2, 4);   /* state = SAVE_MENU_FADING */
    }
    call_save_menu_text_gfx();
}

/* SaveMenuOpenUpdate8132C50(r5 = menu object ptr) → all regs/flags from saveMenuTextGfx */
Next SaveMenuOpenUpdate8132C50(void) {
    save_menu_open_update(R5);
    RETURN_TO_CALLER();
}

/* ── SaveMenuUpdate8132C68 ── */
/* SaveMenuUpdate8132C68: per-frame update of the save menu.
 * R5 points to the menu's state block; the byte at offset 1 holds the next
 * menu state. When the menu is dismissed (dismiss flag 8 already set, or
 * both chatbox flags 0x20 and 0x110 now set) it stores the fade-out state,
 * starts a screen fade and marks the fade in the chatbox flags. Afterwards
 * it refreshes the menu text graphics either way. */
Next SaveMenuUpdate8132C68(void) {
    u32 menu = R5;
    int startFade;

    /* Was the menu already dismissed? */
    R0 = 8;
    GUEST_CALL(chatbox_mask_eFlags2009F38); /* answers in Z; the original branches on it */
    startFade = !ZERO_FLAG();

    if (!startFade) {
        /* Otherwise fade only once the chatbox text is done (0x20)
         * and the extra confirmation flag (0x110) is also set. */
        R0 = 0x20;
        GUEST_CALL(chatbox_mask_eFlags2009F38); /* answers in Z; the original branches on it */
        if (!ZERO_FLAG()) {
            R0 = 0x110;
            GUEST_CALL(chatbox_mask_eFlags2009F38); /* answers in Z; the original branches on it */
            startFade = !ZERO_FLAG();
        }
    }

    if (startFade) {
        ST8(menu + 1, 8);          /* switch the menu to its fade-out state */
        R0 = 0xC;
        R1 = 0xC;
        GUEST_CALL(SetScreenFade); /* begin fading both screens */
        R0 = 0x40;
        GUEST_CALL(chatbox_set_eFlags2009F38); /* remember the fade started */
    }

    GUEST_CALL(saveMenuTextGfx_8132F4C);
    RETURN_TO_CALLER();
}

/* ── ExitSaveMenu8132CA0 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_ExitSaveMenu8132CA0(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();       /* Z set means no fade is active */
}

/* register form: calls chatbox_set_eFlags2009F38 outside this file, which has no typed C interface yet */
static void chatbox_set_eflags(void) {
    R0 = 0x40;
    GUEST_CALL(chatbox_set_eFlags2009F38);
}

/* register form: calls pet_menuExitToMap_811F708 outside this file, which has no typed C interface yet */
static void pet_menu_exit_to_map(void) {
    GUEST_CALL(pet_menuExitToMap_811F708);
}

/* register form: calls saveMenuTextGfx_8132F4C outside this file, which has no typed C interface yet */
static void save_menu_text_gfx(void) {
    GUEST_CALL(saveMenuTextGfx_8132F4C);
}

/* Exit the save menu: when a screen fade is in progress, reset chatbox e-flags and
 * transition the PET menu back to the map; then refresh save-menu text graphics.
 * All output registers and flags come from the final saveMenuTextGfx call. */
static void exit_save_menu(void) {
    if (screen_fade_active_ExitSaveMenu8132CA0()) {
        chatbox_set_eflags();
        pet_menu_exit_to_map();
    }
    save_menu_text_gfx();
}

/* ExitSaveMenu8132CA0: outputs r0-r3, r10-r12, lr, nz, c, v from saveMenuTextGfx_8132F4C */
Next ExitSaveMenu8132CA0(void) {
    exit_save_menu();
    RETURN_TO_CALLER();
}

/* ── saveMenu_config_textscript_8132CB8 ── */
/* saveMenu_config_textscript_8132CB8: rebuilds the save menu's init-ref list,
 * decompresses the background data, loads the network text script and draws
 * the text-script background tiles.
 *
 * Register contract: the original pushes r4-r7 on entry and pops them on
 * exit, so the listed outputs r4-r7 are just the incoming values restored.
 * Because the CopyBackgroundTiles helper passes its 5th/6th argument in
 * guest R4/R5, the adapter saves the incoming r4-r7 and restores them after
 * the typed function has run. */

/* register form: calls getKeyedTableEntry_811FB84 outside this file, which has no typed C interface yet */
static u32 convert_init_ref(u32 data, u32 size) {
    R0 = data;
    R1 = size;
    GUEST_CALL(getKeyedTableEntry_811FB84);
    return R0;
}

/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decomp_and_copy(u32 list) {
    R0 = list;
    GUEST_CALL(decompAndCopyData);
}

/* register form: calls sub_8123300 outside this file, which has no typed C interface yet */
static void load_network_text_script(u32 ptr_table, u32 script_area, u32 decomp_buffer) {
    R0 = ptr_table;
    R1 = script_area;
    R2 = decomp_buffer;
    GUEST_CALL(sub_8123300);
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet
 * (r4/r5 carry the 5th and 6th arguments; the adapter restores the caller's r4-r7 afterwards) */
static void copy_background_tiles_saveMenu_config_textscript_8132CB8(u32 j, u32 i, u32 tile_block, u32 tile_ids,
                                  u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = tile_block;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* Table at off_8132D58: two entries of four words each --
 * { data pointer, size, extra word, extra word } -- describing the records
 * that get assembled into the init-ref list at unk_20096E0. */

/* Build the init-ref list: for each table entry, store the pointer produced by
 * convert_init_ref() followed by the entry's two extra words verbatim, then a
 * zero terminator. */
static void build_init_ref_list(void) {
    u32 dest = A_unk_20096E0;
    u32 table = A_off_8132D58;
    u32 out = 0;

    for (u32 src = 0; src < 0x20; src += 16) {
        u32 data = LD32(table + src);
        u32 size = LD32(table + src + 4);

        ST32(dest + out, convert_init_ref(data, size));
        out += 4;

        for (u32 k = 2; k < 4; k++) {
            ST32(dest + out, LD32(table + src + k * 4));
            out += 4;
        }
    }

    ST32(dest + out, 0);
}

/* Set up everything the save menu needs before it opens. */
static void save_menu_config_textscript(void) {
    build_init_ref_list();

    /* Decompressing the assembled list supplies part of the background
     * (omitting it leaves the background partially drawn). */
    decomp_and_copy(A_unk_20096E0);

    /* Second compressed blob for the background
     * (omitting it leaves the background black). */
    decomp_and_copy(A_off_8132D80);

    /* Decompress the network text script (omitting it crashes the save menu). */
    load_network_text_script(A_TextScriptNetworkPtrs8132D18,
                             A_eTextScript201BF00,
                             A_eDecompBuffer2013A00);

    /* Copy the 30x20 grid of text-script background tiles from tile block 1. */
    copy_background_tiles_saveMenu_config_textscript_8132CB8(0, 0, 1, A_eTextScript201BA00, 0x1E, 0x14);
}

Next saveMenu_config_textscript_8132CB8(void) {
    /* The original's push/pop of r4-r7: outputs are the incoming values. */
    u32 r4_in = R4, r5_in = R5, r6_in = R6, r7_in = R7;
    save_menu_config_textscript();
    R4 = r4_in; R5 = r5_in; R6 = r6_in; R7 = r7_in;
    RETURN_TO_CALLER();
}

/* ── saveMenu_8132D9C ── */
/* register form: calls renderTextGfx_8045F8C outside this file, which has no typed C interface yet.
 * Saves/restores R4–R7 because the callee uses them as scratch after consuming them as parameters. */
static void render_text(u32 script, u32 index, u32 dest, u32 tilemap,
                        u32 width, u32 height, u32 font, u32 flag) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = script; R1 = index; R2 = dest; R3 = tilemap;
    R4 = width;  R5 = height; R6 = font; R7 = flag;
    GUEST_CALL(renderTextGfx_8045F8C);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: calls sub_8132FB8 outside this file, which has no typed C interface yet */
static void save_slot_update(u32 value, u32 data) {
    R0 = value; R1 = data;
    GUEST_CALL(sub_8132FB8);
}

/* register form: calls storeTableEntryIntoUnk200a220Field8_8132FC4 outside this file, which has no typed C interface yet */
static void store_table_entry(u32 index, u32 data) {
    R0 = index; R1 = data;
    GUEST_CALL(storeTableEntryIntoUnk200a220Field8_8132FC4);
}

/* register form: calls protectedZennyOrZero_803D06C outside this file, which has no typed C interface yet */
static u32 query_save_state(void) {
    GUEST_CALL(protectedZennyOrZero_803D06C);
    return R0;
}

/* Render all text elements of the save-game menu into tilemap 0x6004000–0x6005400.
 * menu_ctx is the save-menu state pointer (r10): field 0x34 = per-slot data,
 * field 0x38 = status-block pointer whose +8 receives the save-state query result. */
static void saveMenu_render(u32 menu_ctx) {
    u32 script  = A_eTextScript201BF00;
    u32 font    = A_dword_86B7AE0;
    u32 slot    = LD32(menu_ctx + 0x34);

    /* Tile-name headers: 4 columns, 8×1 tiles each */
    render_text(script, 0, A_byte_2017A00, 0x6004000, 8, 1, font, 0);
    render_text(script, 1, A_unk_2017E00,  0x6004200, 8, 1, font, 0);
    render_text(script, 2, A_unk_2018200,  0x6004400, 8, 1, font, 0);
    render_text(script, 3, A_unk_2018600,  0x6004600, 8, 1, font, 0);

    /* Three save-file name rows (3×1 tiles), each preceded by a slot update */
    save_slot_update(0x0C, slot);
    render_text(script, 7, A_unk_2018E00, 0x6004A00, 3, 1, font, 0);

    save_slot_update(0x0E, slot);
    render_text(script, 7, A_unk_2019200, 0x6004C00, 3, 1, font, 0);

    save_slot_update(0x10, slot);
    render_text(script, 7, A_unk_2019600, 0x6004E00, 3, 1, font, 0);

    store_table_entry(0x14, slot);

    /* Info line (7×1 tiles) */
    render_text(script, 8, A_byte_2019A00, 0x6005000, 7, 1, font, 0);

    /* Query save state and store result into the status block */
    u32 state = query_save_state();
    u32 status = LD32(menu_ctx + 0x38);
    ST32(status + 8, state);

    /* Footer lines */
    render_text(script, 6, A_unk_2019E00, 0x6005200, 7, 1, font, 0);
    render_text(script, 5, A_unk_201A200, 0x6005400, 5, 1, font, 0);
}

/* saveMenu_8132D9C(r10 = save-menu context pointer) – renders the entire save menu.
 * R4–R9 are preserved; R0–R3, R10–R12, LR and flags are clobbered by callees. */
Next saveMenu_8132D9C(void) {
    saveMenu_render(R10);
    RETURN_TO_CALLER();
}

/* ── saveMenu_8132EF4 ── */
/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_background_tiles_saveMenu_8132EF4(u32 j, u32 i, u32 tile_block_32x32, const u16 *tile_ids,
                                  u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = tile_block_32x32;
    R3 = (u32)tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* Values the original passes through untouched (it pushes r4-r7 and pops them back). */
typedef struct {
    u32 r4;
    u32 r5;
    u32 r6;
    u32 r7;
} PreservedRegs;

/* Draw the save menu's background: the header text tilemap and three window
 * pieces, blitted into tile block 2 of the background at fixed positions. */
static PreservedRegs draw_save_menu_background(u32 r4, u32 r5, u32 r6, u32 r7) {
    copy_background_tiles_saveMenu_8132EF4(6, 3, 2, (const u16 *)A_unk_201C400, 8, 8);
    copy_background_tiles_saveMenu_8132EF4(0x12, 3, 2, (const u16 *)A_unk_201C480, 7, 2);
    copy_background_tiles_saveMenu_8132EF4(0x12, 7, 2, (const u16 *)A_unk_201C4A0, 7, 2);
    copy_background_tiles_saveMenu_8132EF4(0x12, 9, 2, (const u16 *)A_unk_201C4C0, 7, 2);
    {
        PreservedRegs preserved = { r4, r5, r6, r7 };
        return preserved;
    }
}

/* saveMenu_8132EF4() -> r2 r3 r4 r5 r6 r7
 * r4-r7 are preserved; r2/r3 are whatever CopyBackgroundTiles last leaves there. */
Next saveMenu_8132EF4(void) {
    PreservedRegs preserved = draw_save_menu_background(R4, R5, R6, R7);
    R4 = preserved.r4;
    R5 = preserved.r5;
    R6 = preserved.r6;
    R7 = preserved.r7;
    RETURN_TO_CALLER();
}

/* ── saveMenuTextGfx_8132F4C ── */
/* saveMenuTextGfx_8132F4C: build and draw the text graphics used by the
 * save menu.  Runs sub_8132F78 first, then renders the text script
 * eTextScript201BF00 into the VRAM tile area at 0x06004800, with the
 * destination buffer unk_2018A00 and palette data dword_86B7AE0.
 * r0-r3 on return are whatever renderTextGfx_8045F8C left there;
 * r4-r7 are preserved for the caller. */
Next saveMenuTextGfx_8132F4C(void) {
    /* The original pushes r4-r7 and pops them, so they reach the caller
     * unchanged even though both callees receive arguments in them. */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(sub_8132F78);

    /* renderTextGfx_8045F8C(script, scriptBank=4, dest, vram=0x6004800,
     *                       width=7, ?=1, palette, ?=0) */
    R0 = A_eTextScript201BF00;
    R1 = 4;
    R2 = A_unk_2018A00;
    R3 = 0x6004800;
    R4 = 7;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* r0-r3 stay as renderTextGfx_8045F8C returned them. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8132F78 ── */
/* Convert two BCD digits (a high nibble and a low nibble packed in a byte)
 * into a plain binary number in 0..99, e.g. 0x57 -> 57. */
static u32 bcd_byte_to_number(u32 packed)
{
    u32 ones = packed & 0xF;
    u32 tens = (packed >> 4) & 0xF;
    return tens * 10 + ones;
}

/* sub_8132F78:
 * Read a BCD-coded counter from the S2001c04 structure (via memory_bcd_8000D84),
 * split its upper 16 bits into four BCD digits, and store them as two 2-digit
 * numbers in the Unk200a220 structure: the low pair (digits 1-0) at +0xC and
 * the high pair (digits 3-2) at +8. */
Next sub_8132F78(void)
{
    u32 bcdSource = LD32(LD32(R10 + oToolkit_S2001c04_Ptr) + 0x18);

    R0 = bcdSource;
    GUEST_CALL(memory_bcd_8000D84);

    u32 bcd = R0 >> 16; /* only the upper half of the BCD result is used */
    u32 dest = LD32(R10 + oToolkit_Unk200a220_Ptr);

    ST32(dest + 0xC, bcd_byte_to_number(bcd));      /* digits 1..0 */
    ST32(dest + 8,  bcd_byte_to_number(bcd >> 8));  /* digits 3..2 */

    RETURN_TO_CALLER();
}

/* ── sub_8132FB8 ── */
/* sub_8132FB8(r0 = index into table at r1, r10 = toolkit):
 * Read the halfword at table[index] and write it into the struct pointed to
 * by the toolkit's oToolkit_Unk200a220 pointer, at offset +8. */
Next sub_8132FB8(void) {
    u32 value = LD16(R1 + R0);
    u32 structPtr = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(structPtr + 8, value);
    RETURN_TO_CALLER();
}

/* ── storeTableEntryIntoUnk200a220Field8_8132FC4 ── */
/* storeTableEntryIntoUnk200a220Field8_8132FC4(r0 = index, r1 = table base):
 * Read the word at table[index] and store it into the "unk 200a220"
 * work struct (field +8) pointed to by the toolkit. Clobbers r0/r2;
 * everything else passes through untouched. */
Next storeTableEntryIntoUnk200a220Field8_8132FC4(void) {
    u32 work = LD32(R10 + oToolkit_Unk200a220_Ptr);
    u32 value = LD32(R1 + R0);

    ST32(work + 8, value);

    RETURN_TO_CALLER();
}

/* ── decomposeTileBlock_8132FD0 ── */
/* register form: calls SWI 6 (game-custom tile-chain decompression), no typed C interface yet */
static u32 swi6_decompress_tile(u32 tile, u32 param) {
    R0 = tile;
    R1 = param;
    swi_call(6);
    return R0;  /* next tile in chain; 0 means end */
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_bg_tiles_sub_8132FD0(u32 j, u32 i, u32 which_block, u32 tile_ids,
                          u32 j_size, u32 i_size) {
    R0 = j; R1 = i; R2 = which_block; R3 = tile_ids; R4 = j_size; R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* Build the interleaved tile-ID list for one 3-block strip into eDecompBuffer2013A00,
 * then blit it via CopyBackgroundTiles.
 * tile_block_header points to a structure whose u16 fields at +0xC/+0xE/+0x10 give
 * the starting tile of each block's compression chain. */
static void decompose_tile_block(u32 tile_block_header) {
    /* Phase 1: count the chain length of each of the 3 blocks via SWI 6. */
    u32 counts[3];
    u32 total = 5;            /* 5 fixed non-sequential entries per column */
    u32 hdr_off = 0xC;

    for (int blk = 0; blk < 3; blk++) {
        u32 tile = LD16(tile_block_header + hdr_off);
        u32 n = 0;
        do {
            n++;
            tile = swi6_decompress_tile(tile, 0xA);
        } while (tile != 0);
        counts[blk] = n;
        total += n;
        hdr_off += 2;
    }

    /* Phase 2: fill the two-column (left/right) tile-ID buffer.
     * right byte-offset tracks the second column; left tracks the first. */
    u32 buf   = A_eDecompBuffer2013A00;
    u32 left  = 0;
    u32 right = total << 1;   /* byte position where right column begins */

    /* Fixed header pair from word_201C4E0 */
    ST16(buf + left,  LD16(A_word_201C4E0));
    left  += 2;
    ST16(buf + right, LD16(A_word_201C4E0 + 2));
    right += 2;

    /* Block 0: sequential tile IDs from VRAM base 0x30B250 */
    u32 base = 0x30B250;
    for (u32 i = counts[0]; i > 0; i--) {
        ST16(buf + left,  base); left  += 2; base++;
        ST16(buf + right, base); right += 2; base++;
    }
    /* Block 0 trailer: two pairs from unk_201C4F0 (offsets 0,2→left; 4,6→right) */
    u32 extra = A_unk_201C4F0;
    ST16(buf + left,  LD16(extra + 0)); left  += 2;
    ST16(buf + right, LD16(extra + 4)); right += 2;
    ST16(buf + left,  LD16(extra + 2)); left  += 2;
    ST16(buf + right, LD16(extra + 6)); right += 2;

    /* Block 1: sequential tile IDs from VRAM base 0x30B260 */
    base = 0x30B260;
    for (u32 i = counts[1]; i > 0; i--) {
        ST16(buf + left,  base); left  += 2; base++;
        ST16(buf + right, base); right += 2; base++;
    }
    /* Block 1 trailer: two pairs from unk_201C500 */
    extra = A_unk_201C500;
    ST16(buf + left,  LD16(extra + 0)); left  += 2;
    ST16(buf + right, LD16(extra + 4)); right += 2;
    ST16(buf + left,  LD16(extra + 2)); left  += 2;
    ST16(buf + right, LD16(extra + 6)); right += 2;

    /* Block 2: sequential tile IDs from VRAM base 0x30B270 (no trailer) */
    base = 0x30B270;
    for (u32 i = counts[2]; i > 0; i--) {
        ST16(buf + left,  base); left  += 2; base++;
        ST16(buf + right, base); right += 2; base++;
    }

    /* Phase 3: blit the assembled tile list as a 32x32 BG block on layer 2. */
    u32 j_size = right >> 2;   /* number of tile entries in the right column */
    u32 j      = 0x19 - j_size;/* columns remaining (25 total minus used) */
    copy_bg_tiles_sub_8132FD0(j, 5, 2, buf, j_size, 2);
}

/* decomposeTileBlock_8132FD0(r5 = tile_block_header) -> preserves r4-r7 */
Next decomposeTileBlock_8132FD0(void) {
    u32 save_r4 = R4, save_r5 = R5, save_r6 = R6, save_r7 = R7;
    decompose_tile_block(R5);
    R4 = save_r4; R5 = save_r5; R6 = save_r6; R7 = save_r7;
    RETURN_TO_CALLER();
}

/* ── gfx_fillAllBandedTilemaps_81330AC ── */
/* register form: calls gfx_fillTilemapConsecutiveBanded_8120C94 outside this file,
 * which has no typed C interface yet */
static void fill_tilemap_banded(u32 dst, u32 rows, u32 bands, u32 src) {
    R0 = dst;
    R1 = rows;
    R2 = bands;
    R3 = src;
    GUEST_CALL(gfx_fillTilemapConsecutiveBanded_8120C94);
}

/* Descriptor for one banded tilemap fill, as laid out at off_81330E8 and friends. */
typedef struct {
    u32 dst;    /* +00: destination tilemap buffer   */
    u32 src;    /* +04: tile data in IWRAM           */
    u8  rows;   /* +08: number of rows to fill       */
    u8  bands;  /* +09: banding parameter            */
} TilemapFillDesc;

#define TILEMAP_FILL_COUNT 7   /* loop runs while index < 0x1C, stepping by 4 */

/* Refill every background tilemap listed in the off_81330CC descriptor table. */
static void fill_all_tilemaps(void) {
    for (u32 i = 0; i < TILEMAP_FILL_COUNT; i++) {
        u32 desc = LD32(A_off_81330CC + i * 4);
        fill_tilemap_banded(
            LD32(desc + 0),          /* -> dst   */
            LD8(desc + 8),           /* -> rows  */
            LD8(desc + 9),           /* -> bands */
            LD32(desc + 4));         /* -> src   */
    }
}

/* gfx_fillAllBandedTilemaps_81330AC() -> r4 r5 r6 r7 preserved; nz c from the loop's final CMP r6, #0x1C */
Next gfx_fillAllBandedTilemaps_81330AC(void) {
    fill_all_tilemaps();
    SET_FLAGS_CMP(0x1C, 0x1C);       /* loop always exits with r6 == 0x1C: equal */
    RETURN_TO_CALLER();
}

/* ── sub_81333A0 ── */
/* sub_81333A0: dispatch a battle object's type-specific update handler,
 * then sync fields, run entity handlers, and apply post-update effects.
 * R5 = object pointer; object byte [R5+2] is the type (pre-scaled by 4 as
 * a word-table offset), byte [R5+3] is a sub-type/flag. */
Next sub_81333A0(void) {
    /* Jump table dispatch: type values 0,4,8,0xC,0x10 index a 5-entry
     * word table of update handlers. */
    u32 objType = LD8(R5 + 2);
    R1 = objType;   /* callee may inspect the type byte */

    if (objType == 0)
        GUEST_CALL(battle_objectUpdate_8133400);
    else if (objType == 4)
        GUEST_CALL(sub_81337C4);
    else if (objType == 8)
        GUEST_CALL(sub_8133434);
    else if (objType == 0xC)
        GUEST_CALL(sub_8133484);
    else /* objType == 0x10 */
        GUEST_CALL(sub_8133788);

    /* Sync battle state from the object and finalize the handler call */
    GUEST_CALL(battle_setFieldsFromObj_81340FC);
    GUEST_CALL(sub_8134490);

    /* Run the generic entity update pipeline */
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    GUEST_CALL(sub_80465F8);

    /* Post-update: type-specific effect spawning or cleanup */
    objType = LD8(R5 + 2);

    if (objType == 0xC) {
        /* Type 12 path: act only if sub-type is non-zero */
        u32 subType = LD8(R5 + 3);
        if (subType == 0) {
            SET_FLAGS_CMP(subType, 0);
            RETURN_TO_CALLER();
        }
        GUEST_CALL(sub_81345FC);
    } else if (objType == 4 && LD8(R5 + 3) == 0xC) {
        /* Type 4, sub-type 12: no further effects */
        SET_FLAGS_CMP(0xC, 0xC);
        RETURN_TO_CALLER();
    } else {
        /* Default / type-4-with-other-sub-type: spawn fixed effects */
        GUEST_CALL(sub_81345D4);
        GUEST_CALL(battle_spawnFixedArgEffect_wrapper_81345E8);
        GUEST_CALL(sub_8134610);
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectUpdate_8133400 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_sub_8133400(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();  /* Z clear (return value non-zero) means a fade transition is running */
}

/* register form: calls submenu_clearBgIfClosed_8134594 outside this file, which has no typed C interface yet */
static void call_update_battle_logic(void) {
    GUEST_CALL(submenu_clearBgIfClosed_8134594);
}

/* register form: calls sub_8134538 outside this file, which has no typed C interface yet */
static void call_update_animations(void) {
    GUEST_CALL(sub_8134538);
}

/* register form: calls sub_81341A0 outside this file, which has no typed C interface yet */
static void call_update_camera(void) {
    GUEST_CALL(sub_81341A0);
}

/* register form: calls sub_81353DC outside this file, which has no typed C interface yet */
static void call_reset_turn_state(void) {
    GUEST_CALL(sub_81353DC);
}

/* register form: calls sub_81353F0 outside this file, which has no typed C interface yet */
static u32 call_set_substate(u32 state, u32 param) {
    R0 = state;
    R1 = param;
    GUEST_CALL(sub_81353F0);
    return R0;
}

/*
 * Main per-frame update for a battle object.
 * If a screen fade is in progress, force the object's state byte to 4 and
 * its timer byte to 0 so it idles until the fade finishes.  Then advance
 * battle logic, animations, camera, reset the turn, and enter sub-state 5/0
 * followed by 6/1 (the standard "idle / ready" transition pair).
 *
 * The outputs r0-r3, nz, c, v are whatever the final sub_81353F0 call leaves
 * in the guest registers (callee pass-through); r4-r7 are preserved.
 */
static void battle_object_update(u32 object_ptr) {
    if (screen_fade_active_sub_8133400()) {
        ST8(object_ptr + 2, 4);  /* state → idle */
        ST8(object_ptr + 3, 0);  /* timer  → clear */
    }

    call_update_battle_logic();
    call_update_animations();
    call_update_camera();
    call_reset_turn_state();
    call_set_substate(5, 0);
    call_set_substate(6, 1);
}

/* battle_objectUpdate_8133400(r4, r5=battle obj ptr, r6, r7, r10, lr) → r0 r1 r2 r3 nz c v */
Next battle_objectUpdate_8133400(void) {
    battle_object_update(R5);
    /* r0-r3, nz, c, v were set by the last GUEST_CALL in call_set_substate; nothing to write */
    RETURN_TO_CALLER();
}

/* ── sub_81337C4 ── */
/* Folder-edit menu state dispatcher (0x081337C4).
 *
 * The original is a trampoline: it reads the current menu state from byte
 * [r5+3], picks one of four handler entry points from a jump table at
 * off_81337D8, and tail-calls it ("mov lr,pc; bx r0" followed by "pop {pc}",
 * so the handler ultimately returns straight to this function's caller).
 * Everything except r0/r1 (and the flags) flows through to the handler
 * untouched; the handler's final r5/r8-r12 are this function's results.
 */

/* register form: indirect call through a jump table address (the original's
 * "mov lr,pc; bx r0"); the handler must run with SP four bytes below the
 * entry SP and the caller's return address stored there, exactly as the
 * original's "push {lr}" leaves it, since the handler runs in that window. */
static u32 call_state_step(u32 target, u32 step, u32 return_addr) {
    SP -= 4;
    ST32(SP, return_addr);      /* the original's push {lr} */
    R0 = target;                /* the handler sees the table word in r0 ... */
    R1 = step;                  /* ... and the state byte in r1 */
    GUEST_CALL_ADDR(target);
    SP += 4;                    /* the original's pop {pc} discards the slot */
    return R0;
}

/* Pick the handler for the current folder-edit state and call it. */
static u32 folder_edit_dispatch(u32 state_ptr, u32 return_addr) {
    u32 step = LD8(state_ptr + 3);            /* current menu state */
    u32 target = LD32(A_off_81337D8 + step);  /* Thumb entry point (+1) */
    return call_state_step(target, step, return_addr);
}

/* sub_81337C4(r2 r4 r5 r6 r7 r8 r10 lr) -> r5 r8 r9 r10 r11 r12 (flags clobbered) */
Next sub_81337C4(void) {
    R0 = folder_edit_dispatch(R5, LR);
    RETURN_TO_CALLER();
}

/* ── folderEdit_step_813388C ── */
/* register form: calls sub_8133BB4 outside this file, which has no typed C interface yet */
static bool guard_8133BB4_passed(void) {
    GUEST_CALL(sub_8133BB4);
    return ZERO_FLAG();          /* Z set: returned 0, continue past guard */
}

/* register form: calls sub_8133C0C outside this file, which has no typed C interface yet */
static bool guard_8133C0C_passed(void) {
    GUEST_CALL(sub_8133C0C);
    return ZERO_FLAG();
}

/* register form: calls sub_8134A3C outside this file, which has no typed C interface yet */
static bool guard_8134A3C_passed(void) {
    GUEST_CALL(sub_8134A3C);
    return ZERO_FLAG();
}

/* register form: calls sub_811F7F8 outside this file, which has no typed C interface yet */
static bool folder_edit_gesture_matched(u32 joypad_low) {
    R0 = joypad_low;
    R1 = 7;
    R2 = 0;
    R3 = 0xA;
    GUEST_CALL(sub_811F7F8);
    return R0 != 0;              /* original: tst r0,r0; bne post -> matched when R0!=0 */
}

/* register form: calls checkLeftRightInputForFolderEdit_8133E3C outside this file, which has no typed C interface yet */
static bool left_right_for_folder_edit_handled(int arg) {
    R0 = (u32)arg;
    GUEST_CALL(checkLeftRightInputForFolderEdit_8133E3C);
    return !ZERO_FLAG();         /* original: bne post -> handled (skip save) when returned nonzero */
}

/* register form: calls object_saveStateAndSetState8_8133E7C outside this file, which has no typed C interface yet */
static void save_state_and_set_state_8(void) {
    GUEST_CALL(object_saveStateAndSetState8_8133E7C);
}

/* register form: calls sub_8134310 outside this file, which has no typed C interface yet */
static void post_step_8134310(void) { GUEST_CALL(sub_8134310); }

/* register form: calls sub_81345B0 outside this file, which has no typed C interface yet */
static void post_step_81345B0(void) { GUEST_CALL(sub_81345B0); }

/* register form: calls sub_8134548 outside this file, which has no typed C interface yet */
static void post_step_8134548(void) { GUEST_CALL(sub_8134548); }

/* One frame of the folder-edit object step.
 *
 * folder_obj's +0x28 word is the input-locked flag; toolkit's joypad pointer
 * gives the held buttons. All three outer guards must return 0 to proceed;
 * if any returns nonzero we skip straight to the post-step helpers. With the
 * guards passed: when the object is input-locked, the low-sensitivity held
 * buttons are checked for the folder-edit gesture (sub_811F7F8 with id 7,
 * arg2=0, arg3=0xA); a match skips the rest of the body. Otherwise the L/R
 * folder-edit handler runs with argument 3; if it returns nonzero we skip
 * saving, else we save state and switch into state 8. The post-step helpers
 * run unconditionally on every path. */
static void folder_edit_step(u32 folder_obj, u32 toolkit) {
    if (guard_8133BB4_passed()
        && guard_8133C0C_passed()
        && guard_8134A3C_passed()) {
        u16 locked = LD16(folder_obj + 0x28);
        if (locked) {
            u32 joypad_ptr = LD32(toolkit + oToolkit_JoypadPtr);
            u16 joypad_low = LD16(joypad_ptr + oJoypad_LowSensitivityHeld);
            if (folder_edit_gesture_matched(joypad_low))
                goto post;
        }
        if (left_right_for_folder_edit_handled(3))
            goto post;
        save_state_and_set_state_8();
    }
post:
    post_step_8134310();
    post_step_81345B0();
    post_step_8134548();
}

/* folderEdit_step_813388C(r2 r3 r4 r5 r6 r7 r8 r10 lr) -> nz c v r0..r10 r12 lr */
Next folderEdit_step_813388C(void) {
    folder_edit_step(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8133944 ── */
Next sub_8133944(void) {
    /* State-query gate: if sub_8133C0C returns non-zero (state mismatch), skip the
     * input-handling block and go straight to the tail-end draws/updates. */
    GUEST_CALL(sub_8133C0C);
    if (ZERO_FLAG()) {
        /* sub_81347A4 gates the joypad cursor movement block. If non-zero, skip it. */
        GUEST_CALL(sub_81347A4);
        if (ZERO_FLAG()) {
            u32 toolkit   = R10;
            u32 joypadPtr = LD32(toolkit + oToolkit_JoypadPtr);
            u32 held      = LD16(joypadPtr + oJoypad_LowSensitivityHeld);
            u32 cursor    = LD16(R5 + 0x34);
            R0 = held;
            R1 = 7;
            R2 = 1;
            R3 = cursor;
            GUEST_CALL(menu_moveCursorUpDown_811FA22);
            ST16(R5 + 0x34, (u16)R0);
        }
        GUEST_CALL(copyBackgroundTiles_sub_813471C);
        GUEST_CALL(sub_8134740);
    }

    /* Always-run scene updates. */
    GUEST_CALL(submenu_clearBgIfClosed_8134594);
    GUEST_CALL(sub_81345B0);
    GUEST_CALL(sub_8134538);
    GUEST_CALL(sub_8134548);
    GUEST_CALL(sub_81341A0);
    GUEST_CALL(sub_8134310);
    GUEST_CALL(sub_81353DC);

    /* Two palette/OBJ-bank setups: slot 5 in low (0), slot 6 in high (1). */
    R0 = 5; R1 = 0;
    GUEST_CALL(sub_81353F0);
    R0 = 6; R1 = 1;
    GUEST_CALL(sub_81353F0);

    RETURN_TO_CALLER();
}

/* ── sub_813399C ── */
/* sub_813399C: virtual-method dispatch on the object at r5, followed by a
 * fixed tail call.
 *
 * The byte at (r5 + 2) selects a handler from the two-entry table at
 * off_81339B4 — both entries hold petNavi_perFrameUpdate_81339BC+1, so the fetched pointer is
 * always that function. The original performs the call with the classic
 * "mov lr, pc / bx r0" idiom: the handler is entered with R0 still holding
 * the fetched table entry (its own address) and R1 holding the selector,
 * and it returns straight into the call below. */
Next sub_813399C(void) {
    u32 method = LD8(R5 + 2);
    u32 handler = LD32(A_off_81339B4 + method); /* both entries: petNavi_perFrameUpdate_81339BC+1 */

    R0 = handler;
    R1 = method;
    GUEST_CALL(petNavi_perFrameUpdate_81339BC);

    GUEST_CALL(sub_8134490);
    RETURN_TO_CALLER();
}

/* ── petNavi_perFrameUpdate_81339BC ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();
}

/* register form: calls battle_setFieldsFromObj_81340FC outside this file, which has no typed C interface yet */
static void sub_81340FC_call(void) {
    GUEST_CALL(battle_setFieldsFromObj_81340FC);
}

/* register form: calls entity_runAllUpdateHandlers_80465BC outside this file, which has no typed C interface yet */
static void entity_runAllUpdateHandlers_80465BC_call(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
}

/* register form: calls sub_80465F8 outside this file, which has no typed C interface yet */
static void sub_80465F8_call(void) {
    GUEST_CALL(sub_80465F8);
}

/* register form: calls sub_8046664 outside this file, which has no typed C interface yet */
static void sub_8046664_call(void) {
    GUEST_CALL(sub_8046664);
}

/* register form: calls zeroFill_e2002230 outside this file, which has no typed C interface yet */
static void zeroFill_e2002230_call(void) {
    GUEST_CALL(zeroFill_e2002230);
}

/* register form: calls timer_setNoTimeout_8137700 outside this file, which has no typed C interface yet */
static void timer_setNoTimeout_8137700_call(void) {
    GUEST_CALL(timer_setNoTimeout_8137700);
}

/* register form: calls sub_81206C4 outside this file, which has no typed C interface yet */
static void sub_81206C4_call(u32 addr) {
    R0 = addr;
    GUEST_CALL(sub_81206C4);
}

/* register form: calls sub_8120740 outside this file, which has no typed C interface yet */
static void sub_8120740_call(u32 value) {
    R0 = value;
    GUEST_CALL(sub_8120740);
}

/* register form: calls sub_81206F8 outside this file, which has no typed C interface yet */
static void sub_81206F8_call(u32 addr, u32 value) {
    R0 = addr;
    R1 = value;
    GUEST_CALL(sub_81206F8);
}

/* register form: calls SetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static void SetCurPETNaviStatsByte_call(u32 a1, u32 a2, u32 a3) {
    R0 = a1;
    R1 = a2;
    R2 = a3;
    GUEST_CALL(SetCurPETNaviStatsByte);
}

/* register form: calls submenu_clearBgIfClosed_8134594 outside this file, which has no typed C interface yet */
static void sub_8134594_call(void) {
    GUEST_CALL(submenu_clearBgIfClosed_8134594);
}

/* register form: calls sub_8134538 outside this file, which has no typed C interface yet */
static void sub_8134538_call(void) {
    GUEST_CALL(sub_8134538);
}

/* register form: calls sub_81345B0 outside this file, which has no typed C interface yet */
static void sub_81345B0_call(void) {
    GUEST_CALL(sub_81345B0);
}

/* register form: calls sub_8134548 outside this file, which has no typed C interface yet */
static void sub_8134548_call(void) {
    GUEST_CALL(sub_8134548);
}

/* Per-frame PET/Navi update: drive the update pipeline, and while a screen fade
 * is active, refresh the current PET/Navi stats from `state` and reset its
 * small header fields. */
static void refresh_pet_navi_stats(u32 state) {
    sub_81340FC_call();
    entity_runAllUpdateHandlers_80465BC_call();
    sub_80465F8_call();

    if (!screen_fade_active()) {
        /* Stable screen: just run the trailing update pipeline. */
        sub_8134594_call();
        sub_8134538_call();
        sub_81345B0_call();
        sub_8134548_call();
        return;
    }

    /* Screen is fading: clear scratch buffers, disable timeouts, then sync
     * the current PET/Navi's stats from `state` before resetting its header. */
    sub_8046664_call();
    zeroFill_e2002230_call();
    timer_setNoTimeout_8137700_call();

    sub_81206C4_call(A_unk_202A3E0);
    sub_8120740_call(LD8(state + 0xC));
    sub_81206F8_call(A_word_202A020, LD8(state + 0xC));

    if (LD8(state + 2) == 0) {
        u32 navi_idx = LD8(state + 0xC);

        /* Sync the half-word at state+0x1C into stats byte 0x2E+navi. */
        SetCurPETNaviStatsByte_call(
            LD8(state + 0x14),
            navi_idx + 0x2E,
            LD16(state + 0x1C)
        );

        /* Copy the two words at state+0x5C..0x60 into stats bytes
         * 0x56..0x57 (one stats byte per word, indexed by navi). */
        u32 off = 0x5C;
        u32 stats_byte = 0x56;
        while ((s32)off <= (s32)0x60) {
            SetCurPETNaviStatsByte_call(
                LD8(state + 0x14),
                (LD8(state + 0xC) << 1) + stats_byte,
                LD32(state + off)
            );
            stats_byte += 1;
            off += 4;
        }
    }

    /* Reset the small header fields of the state struct. */
    ST8(state + 0, 0);
    ST8(state + 1, 0xC);
    ST8(state + 2, 0);

    sub_8134594_call();
    sub_8134538_call();
    sub_81345B0_call();
    sub_8134548_call();
}

Next petNavi_perFrameUpdate_81339BC(void) {
    refresh_pet_navi_stats(R5);
    RETURN_TO_CALLER();
}

/* ── checkLeftRightInputForFolderEdit_8133E3C ── */
#define oToolkit_JoypadPtr          4
#define oJoypad_LowSensitivityHeld  4

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet.
 * Saves/restores r4–r7 to mimic the original's push {r4-r7}/pop {r4-r7} frame. */
static void play_buzzer(u32 sound_id, u32 held_buttons, u32 view_bit, u32 dir_mask) {
    u32 sv_r4 = R4, sv_r5 = R5, sv_r6 = R6, sv_r7 = R7;
    R0 = sound_id;
    R2 = dir_mask;
    R4 = held_buttons;
    R6 = view_bit;
    R7 = 0;
    GUEST_CALL(PlaySoundEffect);
    R4 = sv_r4; R5 = sv_r5; R6 = sv_r6; R7 = sv_r7;
}

/* Check left/right d-pad for folder-edit navigation.
 * Returns 1 = direction accepted (switch view), 0 = denied (buzzer played),
 * 2 = no direction pressed.
 *   mode_offset: byte offset into the mode table (r0)
 *   mode_base:   pointer to the mode table (r5)
 *   toolkit:     pointer to the toolkit (r10) */
static s32 check_folder_edit(u32 mode_offset, u32 mode_base, u32 toolkit) {
    u32 joypad_ptr = LD32(toolkit + oToolkit_JoypadPtr);
    u32 held = LD16(joypad_ptr + oJoypad_LowSensitivityHeld);
    u32 view = LD8(mode_base + mode_offset) >> 2; /* bit 2: 0=folder, 1=pack */

    s32 result = 2; /* no direction pressed */
    u32 dir = JOYPAD_LEFT;

    if (held & JOYPAD_LEFT) {
        /* Left arrow: allowed in folder view, denied in pack view */
        result = view ? 0 : 1;
    } else {
        dir = JOYPAD_RIGHT;
        if (held & JOYPAD_RIGHT) {
            /* Right arrow: allowed in pack view, denied in folder view */
            result = view ? 1 : 0;
        }
    }

    if (result == 0)
        play_buzzer(0x7A, held, view, dir);

    return result;
}

/* checkLeftRightInputForFolderEdit_8133E3C(r0=offset, r5=base, r10=toolkit) -> r0, nz */
Next checkLeftRightInputForFolderEdit_8133E3C(void) {
    R0 = check_folder_edit(R0, R5, R10);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_saveStateAndSetState8_8133E7C ── */
/* object_saveStateAndSetState8_8133E7C(r5 = object):
 * Remember the object's current state byte in a "previous state" slot
 * (offset 0xD), then set its state to 8. */
Next object_saveStateAndSetState8_8133E7C(void) {
    u32 obj = R5;

    ST8(obj + 0xD, LD8(obj + 3)); /* save old state */
    ST8(obj + 3, 8);              /* new state = 8 */

    RETURN_TO_CALLER();
}

/* ── sub_81340DC ── */
/* sub_81340DC: run the update routine for the data at byte_813314C, then, if
 * battle struct slot 3 currently exists, reposition its sprite to (0xC0, 3)
 * (with the caller's r2/r3 and the struct pointer passed through to
 * sprite_setCoordinates). Preserves r4-r7 like the original's push/pop. */
Next sub_81340DC(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R0 = A_byte_813314C;
    GUEST_CALL(sub_80465A0);

    R0 = 3;
    GUEST_CALL(getStructFrom2008450);

    /* getStructFrom2008450 returns the struct in r1 and null when slot 3 is
     * empty (that is what the original's beq tests); skip the move then. */
    if (R1 != 0) {
        R5 = R1;
        R0 = 0xC0;
        R1 = 3;
        GUEST_CALL(sprite_setCoordinates);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_setFieldsFromObj_81340FC ── */
/* Table entry size in bytes (0x20 per entry). */
#define TABLE_ENTRY_SIZE 0x20

/* Offset of the packed data field within a table entry. */
#define ENTRY_DATA_OFFSET 0x1C

/*
 * Call setFieldsInThreeTableEntries_811980C with the extracted fields.
 * The callee writes r1/r2/r3 and sets c/v; those remain in the guest
 * registers after GUEST_CALL returns.
 */
/* register form: calls setFieldsInThreeTableEntries_811980C outside this file, which has no typed C interface yet */
static void call_setFieldsInThreeTableEntries(u32 fieldHi, u32 fieldLo, u32 zero, u32 entryIdx) {
    R0 = fieldHi;
    R1 = fieldLo;
    R2 = zero;
    R3 = entryIdx;
    GUEST_CALL(setFieldsInThreeTableEntries_811980C);
}

/*
 * Select the appropriate battle-table entry for the object at @p obj,
 * extract its packed fields, and forward them to the shared three-table
 * setter (which writes r1/r2/r3 and sets c/v in the guest registers).
 *
 * If the flag byte at obj+0xE is set, use the "secondary" index fields
 * (obj+0x2A + obj+0x2E) and the alternate table pointer unk_202A3E0;
 * otherwise use (obj+0x20 + obj+0x24) and word_202A020.
 */
static void setBattleFieldsFromObject(u32 obj) {
    u32 tableBase;
    u32 entryIdx;

    u8 secondaryFlag = LD8(obj + 0xE);
    if (secondaryFlag != 0) {
        tableBase = A_unk_202A3E0;
        entryIdx  = LD16(obj + 0x2A) + LD16(obj + 0x2E);
    } else {
        tableBase = A_word_202A020;
        entryIdx  = LD16(obj + 0x20) + LD16(obj + 0x24);
    }

    u32 packedData = LD32(tableBase + entryIdx * TABLE_ENTRY_SIZE + ENTRY_DATA_OFFSET);

    /* Bits [15:7] of packedData — extracted via lsl #16 / lsr #23 in the original. */
    u32 fieldHi = (packedData << 16) >> 23;
    /* Bits [6:0] of packedData. */
    u32 fieldLo = packedData & 0x7F;

    call_setFieldsInThreeTableEntries(fieldHi, fieldLo, 0, entryIdx);
}

/* battle_setFieldsFromObj_81340FC(r5 = battle object ptr) -> c v r1 r2 r3 from callee */
Next battle_setFieldsFromObj_81340FC(void) {
    setBattleFieldsFromObject(R5);
    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_803FD9C_on_eTextScript2028A20 ── */
/* chatbox_runScript_803FD9C_on_eTextScript2028A20(r0 = scriptIdx, r4, r5, r10, lr):
 * Run the text script at index scriptIdx from the eTextScript2028A20 archive.
 * inputs:  r0 r4 r5 r10 lr
 * outputs: nz c v r0 r1 r2 r3 r4 r5 r6 r7 lr
 * preserved: r6 r7 */
Next chatbox_runScript_803FD9C_on_eTextScript2028A20(void) {
    /* Move scriptIdx into r1 (the archive's script-index slot) and pass the
     * eTextScript2028A20 archive as r0 to chatbox_runScript_803FD9C. */
    R1 = R0;
    R0 = A_eTextScript2028A20;
    GUEST_CALL(chatbox_runScript_803FD9C);
    RETURN_TO_CALLER();
}

/* ── sub_8134538 ── */
/* sub_8134538: thin wrapper that invokes object_resolveTableEntry_8134558 with the fixed argument
 * set (226, 28, 30, 36); result and condition flags are whatever the callee
 * leaves behind. r4-r7 pass through untouched. */
Next sub_8134538(void) {
    R0 = 0xE2;
    R1 = 0x1C;
    R2 = 0x1E;
    R3 = 0x24;
    GUEST_CALL(object_resolveTableEntry_8134558);
    RETURN_TO_CALLER();
}

/* ── sub_8134548 ── */
/* sub_8134548: tail-call wrapper for object_resolveTableEntry_8134558 with fixed arguments
 *   r0 = 0x182, r1 = 0x1C, r2 = 0x28, r3 = 0x2E.
 * Registers r0-r3 and the condition flags are whatever object_resolveTableEntry_8134558 leaves. */
Next sub_8134548(void) {
    R0 = 0x182;
    R1 = 0x1C;
    R2 = 0x28;
    R3 = 0x2E;
    GUEST_CALL(object_resolveTableEntry_8134558);
    RETURN_TO_CALLER();
}

/* ── object_resolveTableEntry_8134558 ── */
/* Lookup constants baked into the original via literal-pool loads. */
#define LOOKUP_TYPE       0x20002   /* passed as r1 to sub_811FA98 */
#define LOOKUP_FIELD_ID   0xC24C    /* passed as r6 to sub_811FA98 */
#define LOOKUP_COUNT      0x64      /* passed as r7 to sub_811FA98 */
#define ENTRY_STRIDE_FLD  0x18      /* s16 field in the table entry (stride in 8-byte units) */
#define ENTRY_TYPE_FLD    7         /* passed as r4 to sub_811FA98 */

/* register form: calls sub_811FA98 outside this file, which has no typed C interface yet.
 * Saves and restores r4-r7 so the outer function preserves them for its own callers
 * (the original does this via push/pop). */
static void call_811FA98(u32 arg0, u32 arg1, u32 arg2, u32 arg3,
                         u32 arg4, u32 arg5, u32 arg6, u32 arg7) {
    u32 saved4 = R4, saved5 = R5, saved6 = R6, saved7 = R7;
    R0 = arg0; R1 = arg1; R2 = arg2; R3 = arg3;
    R4 = arg4; R5 = arg5; R6 = arg6; R7 = arg7;
    GUEST_CALL(sub_811FA98);
    R4 = saved4; R5 = saved5; R6 = saved6; R7 = saved7;
}

/* Compute an object-lookup id from a table entry base and index, load two attributes
 * from the entry, and delegate to sub_811FA98 to resolve or query the object.
 *
 * The id packs a row number (index minus stride × 8, scaled left by 16) with a
 * caller-supplied sub-identifier.  The two attribute values are unsigned half-words
 * read at offsets attr_a_off / attr_b_off inside the entry pointed to by table_ptr. */
static void lookup_object_entry(u32 index, u32 table_ptr,
                                u32 sub_id, u32 attr_a_off, u32 attr_b_off) {
    /* Read the per-entry stride (signed 16-bit) and scale by 8-byte entry size. */
    u32 stride_shifted = (u32)(s32)LDS16(table_ptr + ENTRY_STRIDE_FLD) << 3;

    /* id = ((index - stride*8) << 16) | sub_id */
    u32 obj_id = (((u32)index - stride_shifted) << 16) | sub_id;

    /* Two unsigned attribute words at caller-chosen offsets within the entry. */
    u32 attr_a = LD16(table_ptr + attr_a_off);
    u32 attr_b = LD16(table_ptr + attr_b_off);

    call_811FA98(obj_id, LOOKUP_TYPE, attr_a, attr_b,
                 ENTRY_TYPE_FLD, table_ptr, LOOKUP_FIELD_ID, LOOKUP_COUNT);
}

/* object_resolveTableEntry_8134558(r0 = index, r1 = sub_id, r2 = attr_a_off, r3 = attr_b_off,
 *             r5 = entry pointer;  r4 r6 r7 lr preserved) */
Next object_resolveTableEntry_8134558(void) {
    lookup_object_entry(R0, R5, R1, R2, R3);
    RETURN_TO_CALLER();
}

/* ── submenu_clearBgIfClosed_8134594 ── */
/* register form: calls submenu_isOpen_81355D8 outside this file, which has no typed C interface yet */
static bool submenu_is_open_sub_8134594(void) {
    GUEST_CALL(submenu_isOpen_81355D8);
    return !ZERO_FLAG();          /* Z clear → R0 ≠ 0 → submenu is open */
}

/* register form: calls sub_813468C outside this file, which has no typed C interface yet.
 * Saves/restores R4 and R6 because the original push/pop hides the caller's register
 * modifications from the outer function's output contract. */
static void call_813468C_sub_8134594(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a6) {
    u32 saved_r4 = R4, saved_r6 = R6;
    R0 = a0; R1 = a1; R2 = a2; R3 = a3; R4 = a4; R6 = a6;
    GUEST_CALL(sub_813468C);
    R4 = saved_r4; R6 = saved_r6;
}

/* Clear the submenu background tile unless the submenu is already open. */
static void ensure_submenu_cleared(void) {
    if (submenu_is_open_sub_8134594())
        return;
    call_813468C_sub_8134594(0xE7, 0x16, 0, 0x10, A_byte_81331EC, 0);
}

/* submenu_clearBgIfClosed_8134594(): clears the submenu background tile if the submenu is not open. */
Next submenu_clearBgIfClosed_8134594(void) {
    ensure_submenu_cleared();
    RETURN_TO_CALLER();
}

/* ── sub_81345B0 ── */
/* register form: calls sub_813468C outside this file, which has no typed C interface yet */
static void call_813468C(u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 ptr, u32 flag) {
    /* Original pushes {r4-r7} before this call and pops them after, so we save/restore. */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    R0 = arg0; R1 = arg1; R2 = arg2; R3 = arg3; R4 = ptr; R6 = flag;
    GUEST_CALL(sub_813468C);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
}

/* Invoke sub_813468C with fixed ROM-table parameters:
 * r0=0xF3, r1=0x16, r2=0, r3=0x11, r4=&byte_81331EC, r6=0x10000000.
 * r4-r7 are preserved (the original save/restores them via push/pop). */
static void invoke_813468C(void) {
    call_813468C(0xF3, 0x16, 0, 0x11, A_byte_81331EC, 0x10000000);
}

/* sub_81345B0: preserve r4-r7 and tail-call sub_813468C with fixed arguments. */
Next sub_81345B0(void) {
    invoke_813468C();
    RETURN_TO_CALLER();
}

/* ── sub_81345D4 ── */
/* sub_81345D4:
 * Fixed-parameter wrapper around sub_813468C — everything this routine does
 * is fill in one hard-coded argument set and delegate:
 *   r0 = 0x5A (90), r1 = 0x20 (32), r2 = 0x20 (32), r3 = 0x12 (18)
 *   r4 = pointer to the data table at byte_81331F4
 *   r5 = passed through untouched from the caller
 *   r6 = 0x40000000 (literal pool word at byte_81346FC)
 *   r7 = passed through untouched from the caller
 * All caller-saved state (r4-r7) is preserved across the call, exactly as the
 * original's push/pop pair does; only r0-r3/lr and the flags are clobbered. */
Next sub_81345D4(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    R0 = 0x5A;
    R1 = 0x20;
    R2 = 0x20;
    R3 = 0x12;
    R4 = A_byte_81331F4;
    /* R5: forwarded unchanged to the callee */
    R6 = 0x40000000;
    /* R7: forwarded unchanged to the callee */

    GUEST_CALL(sub_813468C);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_spawnFixedArgEffect_wrapper_81345E8 ── */
/* battle_spawnFixedArgEffect_wrapper_81345E8: spawn/queue helper that forwards a fixed argument set to
 * sub_813468C. The original pushes r4-r7 and pops them afterwards, so this
 * wrapper preserves r4-r7 even though the callee may clobber them; only
 * r1-r3 (the callee's results) escape to the caller. */
Next battle_spawnFixedArgEffect_wrapper_81345E8(void) {
    u32 in_r4 = R4, in_r5 = R5, in_r6 = R6, in_r7 = R7;

    R0 = 0xF0;                       /* x position: 240 */
    R1 = 0x20;                       /* y position: 32 */
    R2 = 0x2A;                       /* width/param: 42 */
    R3 = 0x13;                       /* height/param: 19 */
    R4 = A_byte_81331F4;             /* data table used by the callee */
    R6 = 0x40000000;                 /* literal-pool constant */

    GUEST_CALL(sub_813468C);

    /* pop {r4-r7}: undo whatever the callee left in these registers */
    R4 = in_r4;
    R5 = in_r5;
    R6 = in_r6;
    R7 = in_r7;

    RETURN_TO_CALLER();
}

/* ── copyBackgroundTiles_sub_813471C ── */
/* copyBackgroundTiles_sub_813471C: Copy background tiles for an object. The flag byte at r5+0xD
 * selects between a small (2x2) and large (0x15x2) source rectangle; the
 * destination tile block, tile id source, and the call's own j/i sizes are
 * fixed.
 *
 * inputs:  r5
 * outputs: r5 (preserved across the call via push/pop)
 * clobbers: nz c v r0 r1 r2 r3 lr
 * preserved: r4 r5 r6 r7 */
Next copyBackgroundTiles_sub_813471C(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    u32 j_size, i_size;
    if (LD8(R5 + 0xD) != 0) {
        j_size = 0x15;
        i_size = 2;
    } else {
        j_size = 2;
        i_size = 2;
    }

    R0 = j_size;
    R1 = i_size;
    R2 = 0;                       /* which_tile_block_32x32 */
    R3 = A_unk_203486C;           /* tile_ids */
    R4 = 8;
    R5 = 0x11;
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore the caller's r4/r5 (CopyBackgroundTiles clobbers them). */
    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── battle_alignRowCheck_81348DC ── */
/* register form: calls sub_81171F4 outside this file, which has no typed C interface yet */
static void call_set_valid_flag(u32 val, u32 flag_obj) {
    R0 = val;
    R1 = flag_obj;
    GUEST_CALL(sub_81171F4);
}

/* register form: calls sub_81171D4 outside this file, which has no typed C interface yet */
static void call_move_position(u32 x, u32 y, u32 flag_obj, u32 threshold, u32 row_off) {
    u32 saved_r4 = R4;   /* preserve R4 across callee that may clobber it */
    R0 = x; R1 = y; R2 = flag_obj; R3 = threshold; R4 = row_off;
    GUEST_CALL(sub_81171D4);
    R4 = saved_r4;
}

/* battle_alignRowCheck_81348DC: position/row alignment check for a battle object.
 * gate bit-0 (obj+0x4C): 0 → early exit, 1 → verify diff is in [0, threshold]. */
static void battle_align_check(u32 obj, u32 row_off, u32 cur_x, u32 cur_y,
                               u32 flag_obj, u32 threshold) {
    u32 gate = LD32(obj + 0x4C);

    if (!(gate & 1)) {
        /* register form: early-exit path has no callee; MOVS/TST set R0, R1, flags directly */
        R0 = 1;
        R1 = gate;
        SET_FLAGS_NZ(gate & 1);
        return;
    }

    s32 scroll = (s32)LDS16(obj + 0x18);
    u32 adj_x  = cur_x - (u32)(scroll << 3);

    u32 row_top = LD16(obj + 0x1C);
    u32 row_idx = LD16(obj + row_off);
    s32 diff    = (s32)(row_top - row_idx);

    if (diff < 0 || diff > (s32)threshold) {
        call_set_valid_flag(1, flag_obj);
        return;
    }

    u32 adj_y = cur_y + ((u32)diff << 4);
    call_move_position(adj_x, adj_y, flag_obj, threshold, row_off);
    call_set_valid_flag(0, flag_obj);
}

/* battle_alignRowCheck_81348DC(r0=row_off r1=cur_x r2=cur_y r3=flag_obj r4=threshold r5=obj) */
Next battle_alignRowCheck_81348DC(void) {
    battle_align_check(R5, R0, R1, R2, R3, R4);
    RETURN_TO_CALLER();
}

/* ── sub_8134F78 ── */
/* sub_8134F78: flag a result in the S2001c04 buffer (both bytes set to 0xFF).
 * r10 = toolkit pointer; returns 0xFF in r1. */
Next sub_8134F78(void) {
    u32 data = LD32(R10 + oToolkit_S2001c04_Ptr);

    ST8(data, 0xFF);
    ST8(data + 2, 0xFF);

    R1 = 0xFF;
    RETURN_TO_CALLER();
}

/* ── sub_8134FB0 ── */
/* sub_8134FB0(r0 = member id, r5 = object struct):
 * Walk the two tracked member slots of the struct at r5 (their field offsets
 * live in the table at off_8135018).  For each slot whose stored word equals
 * the member id in r0, clear that slot's flag bits (mask from the table at
 * byte_8135030) in the status word at r5+0x4C.  Note that the original loads
 * the mask into r0 itself, so once a slot has matched, any later slot is
 * compared against that mask rather than the original member id.  Afterwards,
 * unless both status bits 0x60 are still set, call sub_8134FE0 to update the
 * struct.
 *
 * r2 ends as the last table entry (or whatever sub_8134FE0 leaves when the
 * call is made).  r4-r7 are pushed on entry and popped on exit, so the loop's
 * writes to r7 are discarded: r4-r7 all end at their entry values. */
Next sub_8134FB0(void) {
    u32 memberId = R0;
    u32 self     = R5;
    u32 entryR4  = R4, entryR6 = R6, entryR7 = R7;
    u32 slotOffset = 0;

    for (u32 i = 0; i < 8; i += 4) {
        slotOffset = LD32(A_off_8135018 + i);
        if (memberId == LD32(self + slotOffset)) {
            /* The mask load goes into r0, clobbering the member id for any
             * remaining slot comparisons. */
            memberId = LD32(A_byte_8135030 + i);
            ST32(self + 0x4C, LD32(self + 0x4C) & ~memberId);
        }
    }

    /* After the loop r2 holds the last table entry; r7's copy of it is
     * thrown away by the final pop. */
    R2 = slotOffset;

    u32 status = LD32(self + 0x4C) & 0x60;
    if (status != 0x60) {
        /* Register state the original enters sub_8134FE0 with:
         * r0 = status & 0x60, r1 = 0x60, r2/r7 = last table entry,
         * r4 = loop counter left at 8. */
        R0 = status;
        R1 = 0x60;
        R4 = 8;
        R7 = slotOffset;
        GUEST_CALL(sub_8134FE0);
        /* pop {r4-r7,pc} restores the entry values (r2 keeps the callee's). */
        R4 = entryR4;
        R7 = entryR7;
    }

    RETURN_TO_CALLER();
}

/* ── sub_8135244 ── */
/* sub_8135244(r5 = timer object):
 * Two-frame countdown. Tick the primary timer at +0x2A each call while it is
 * nonzero; once it has expired, tick the secondary timer at +0x2E instead.
 * Both r0-r2 and r5 are preserved. */
Next sub_8135244(void) {
    if (LD16(R5 + 0x2A) != 0)
        ST16(R5 + 0x2A, LD16(R5 + 0x2A) - 1);
    else if (LD16(R5 + 0x2E) != 0)
        ST16(R5 + 0x2E, LD16(R5 + 0x2E) - 1);
    RETURN_TO_CALLER();
}

/* ── chatbox_clampScrollToTextEnd_813525C ── */
/* chatbox_clampScrollToTextEnd_813525C: keep the message viewer's scroll position within the last
 * screenful of text. r5 = message window object:
 *   +0x28 = total scrollable height,
 *   +0x2E = current scroll offset (lines scrolled off the top),
 *   +0x2A = text origin adjustment.
 * If scrolling would push the view more than 7 lines past the end of the
 * text, pull the scroll offset back and add the excess to the origin, so
 * the visible window stops at the bottom of the message. */
Next chatbox_clampScrollToTextEnd_813525C(void) {
    u32 textHeight = LD16(R5 + 0x28);

    if ((s32)textHeight >= 7) {
        u32 scroll = LD16(R5 + 0x2E);
        u32 overshoot = (scroll + 7) - textHeight;

        if ((s32)(scroll + 7) > (s32)textHeight) {
            scroll -= overshoot;
            ST16(R5 + 0x2E, scroll);
            ST16(R5 + 0x2A, LD16(R5 + 0x2A) + overshoot);
        }
    }

    RETURN_TO_CALLER();
}

/* ── battle_copyStateSlotsAndZeroNext_813527C ── */
/* Size of one data slot in bytes (toolkit/battle-state entry). */
#define SLOT_SIZE 0x20

/* register form: calls CopyByEightWords outside this file, which has no typed C interface yet */
static void copy_by_eight_words(u32 src, u32 dst, u32 size) {
    R0 = src;
    R1 = dst;
    R2 = size;
    /* register form: original writes r3=#0x20 before the call; callee preserves r3,
       so the value must appear in R3 as an output of this function */
    R3 = SLOT_SIZE;
    GUEST_CALL(CopyByEightWords);
}

/* register form: calls ZeroFillByEightWords outside this file, which has no typed C interface yet */
static void zero_fill_by_eight_words(u32 addr, u32 size) {
    R0 = addr;
    R1 = size;
    GUEST_CALL(ZeroFillByEightWords);
}

/* Copy 'count' slots of SLOT_SIZE bytes from src to dst, then zero-fill the
 * next slot at dst + count*SLOT_SIZE.  If count is zero, only the zero-fill
 * happens at dst (and r2/r3 are not written before the fill call). */
static void copy_then_zero_fill(u32 src, u32 dst, u32 count) {
    if (count != 0) {
        u32 total_bytes = count * SLOT_SIZE;
        copy_by_eight_words(src, dst, total_bytes);
        dst += total_bytes;
    }
    zero_fill_by_eight_words(dst, SLOT_SIZE);
}

/* battle_copyStateSlotsAndZeroNext_813527C(r0=src, r1=dst, r2=slot_count) -> copies slots then zero-fills the next one;
 * r4-r7 preserved via push/pop, r0/r1/lr/nz/c/v clobbered. */
Next battle_copyStateSlotsAndZeroNext_813527C(void) {
    copy_then_zero_fill(R0, R1, R2);
    RETURN_TO_CALLER();
}

/* ── sub_81353DC ── */
/* sub_81353DC: thin wrapper around battle_alignRowCheck_81348DC. It invokes that routine with
 * a fixed argument set (r0 = 0x24, r1 = 0xA5, r2 = 0x28, r3 = 4, r4 = 6) and
 * simply lets its results in r2/r3 pass through to the caller. As in the
 * original (push/pop {r4}), the caller's r4 survives even though the callee
 * may clobber it; r5-r7 are untouched. */
Next sub_81353DC(void) {
    u32 saved_r4 = R4;

    R0 = 0x24;
    R1 = 0xA5;
    R2 = 0x28;
    R3 = 4;
    R4 = 6;
    GUEST_CALL(battle_alignRowCheck_81348DC);

    /* restore the caller's r4, matching the original's push/pop */
    R4 = saved_r4;

    RETURN_TO_CALLER();
}

/* ── sub_81353F0 ── */
/* sub_81353F0: tail-call wrapper around sub_8134930.
 * Calls it with the fixed parameters (0x24, 0xA5, 0x28) and chip/flag
 * argument 6, forwarding the caller's r1 in r3 and the caller's r0 in r7
 * (where sub_8134930 expects them). r4-r7 are preserved for the caller;
 * r0-r3 and the condition flags are whatever sub_8134930 left behind. */
Next sub_81353F0(void) {
    u32 r3_arg = R1;        /* caller's r1 becomes sub_8134930's r3 */
    u32 r7_arg = R0;        /* caller's r0 becomes sub_8134930's r7 */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R0 = 0x24;
    R1 = 0xA5;
    R2 = 0x28;
    R3 = r3_arg;
    R4 = 6;
    R7 = r7_arg;            /* r5/r6 pass through unchanged */
    GUEST_CALL(sub_8134930);

    /* restore the callee-saved registers the original pops back */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── battle_CheckSpawnLimit_8135500 ── */
/* battle_CheckSpawnLimit_8135500: Count battle objects whose alliance matches a given value,
 * bucketed by HP tier, then check whether the current object's tier bucket
 * would exceed the per-tier spawn limit.
 *
 * word_202A020 is an array of 30 battle objects (stride 0x20).
 *   offset 0x1C (u16): alliance ID stored in bits 7..15
 *   offset 0x18 (u32): HP stored in bits 16..31 (tier category)
 * The first argument (obj_ptr) has its own tier byte at offset 8.
 *
 * Tier thresholds: <=0x13, <=0x1D, <=0x27, <=0x31, else (buckets 0-4)
 * Tier limits:     5, 4, 3, 2, 1
 *
 * Returns via out-param:
 *   r0: 1 if the limit is reached, 0 otherwise
 *   r1: the capped limit (exceeded) or count+1 (not exceeded)
 *   r2: bucket index 0-4 (exceeded) or unchanged from caller (not exceeded)
 */

typedef struct {
    u32 exceeded;
    u32 count;
    u32 bucket;
} TierResult;

static TierResult check_tier_limit(u32 obj_ptr, u32 alliance, u32 prev_r2) {
    /* Bucket matching battle objects by HP tier */
    u32 counts[5] = {0, 0, 0, 0, 0};
    const u32 limits[5] = {5, 4, 3, 2, 1};

    u32 base = A_word_202A020;
    for (u32 off = 0; off < 0x3C0; off += 0x20) {
        u32 entry = base + off;
        /* Extract alliance from bits 7..15 of u16 at +0x1C */
        u32 obj_alliance = LD16(entry + 0x1C) >> 7;
        if (obj_alliance != alliance)
            continue;

        /* Extract HP tier from upper 16 bits of u32 at +0x18 */
        u32 tier = LD32(entry + 0x18) >> 16;

        if (tier <= 0x13)       counts[0]++;
        else if (tier <= 0x1D)  counts[1]++;
        else if (tier <= 0x27)  counts[2]++;
        else if (tier <= 0x31)  counts[3]++;
        else                    counts[4]++;
    }

    /* Determine which bucket the current object belongs to */
    u32 obj_tier = LD8(obj_ptr + 8);
    u32 bucket;
    if (obj_tier <= 0x13)       bucket = 0;
    else if (obj_tier <= 0x1D)  bucket = 1;
    else if (obj_tier <= 0x27)  bucket = 2;
    else if (obj_tier <= 0x31)  bucket = 3;
    else                        bucket = 4;

    /* Check if adding this object would exceed the per-tier limit */
    u32 proposed = counts[bucket] + 1;
    if (proposed > limits[bucket]) {
        /* Limit reached – report capped count and which tier overflowed */
        TierResult r;
        r.exceeded = 1;
        r.count    = limits[bucket];
        r.bucket   = bucket;
        return r;
    } else {
        /* Room available – return the proposed count; r2 passes through */
        TierResult r;
        r.exceeded = 0;
        r.count    = proposed;
        r.bucket   = prev_r2;
        return r;
    }
}

/* battle_CheckSpawnLimit_8135500(r0 = obj_ptr, r1 = alliance) -> r0(r2) */
Next battle_CheckSpawnLimit_8135500(void) {
    TierResult res = check_tier_limit(R0, R1, R2);
    R0 = res.exceeded;
    R1 = res.count;
    R2 = res.bucket;
    RETURN_TO_CALLER();
}

/* ── submenu_isOpen_81355D8 ── */
/* submenu_isOpen_81355D8: check the currently open submenu (sSubmenu).
 * Returns r1 = 0 if it is 0x20 (the "no submenu / main" state),
 * r1 = 1 if anything else is open. N/Z reflect r1; C/V are left
 * over from the submenu comparison, matching the original. */
Next submenu_isOpen_81355D8(void) {
    u32 currentSubmenu = LD8(A_sSubmenu);

    SET_FLAGS_CMP(currentSubmenu, 0x20);
    if (currentSubmenu == 0x20)
        R1 = 0;
    else
        R1 = 1;
    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── registerBattleStartScript_81355EC ── */
/* battle_setupPETNavis_8135314 is the underlying "register a script block" routine; it lives outside this file
 * and currently has no typed C interface, so the simplest helper is a register-form passthrough. */
/* register form: calls battle_setupPETNavis_8135314 outside this file, which has no typed C interface yet */
static void call_sub_8135314(u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4) {
    R0 = arg0;
    R1 = arg1;
    R2 = arg2;
    R3 = arg3;
    R4 = arg4;
    GUEST_CALL(battle_setupPETNavis_8135314);
}

/* Register the Battle Start script: source data table, script byte-pool base, total script length,
 * step count, and the init-record pointer. Clobbers r0..r3 (battle_setupPETNavis_8135314 may use them); r4..r7
 * are preserved exactly as before the call (they are inputs and outputs). */
Next registerBattleStartScript_81355EC(void) {
    /* Save the incoming r4..r7 across the call. The original only pushes {r4, lr} since r4 is
     * the only callee-clobbered among the preserved set, but we keep the contract clean. */
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    call_sub_8135314(A_word_202A020, A_off_8135604, 0xA5, 0x28, A_byte_8135610);

    R4 = r4; R5 = r5; R6 = r6; R7 = r7;
    RETURN_TO_CALLER();
}

/* ── battle_refreshPETNaviStats_8136C24 ── */
/* Register-form helpers for the PET Navi stats API (no typed C interface yet). */

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u32 get_pet_navi_byte(u32 navi, u32 stat) {
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return R0;
}

/* register form: calls GetCurPETNaviStatsHword outside this file, which has no typed C interface yet */
static u32 get_pet_navi_hword(u32 navi, u32 stat) {
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsHword);
    return R0;
}

/* register form: calls SetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static void set_pet_navi_byte(u32 navi, u32 stat, u32 value) {
    R0 = navi;
    R1 = stat;
    R2 = value;
    GUEST_CALL(SetCurPETNaviStatsByte);
}

/* register form: calls SetCurPETNaviStatsHword outside this file, which has no typed C interface yet */
static void set_pet_navi_hword(u32 navi, u32 stat, u32 value) {
    R0 = navi;
    R1 = stat;
    R2 = value;
    GUEST_CALL(SetCurPETNaviStatsHword);
}

/* register form: calls GetNaviStatsIndexGivenCurPETNavi outside this file, which has no typed C interface yet */
static u32 get_pet_navi_stats_index(u32 navi) {
    R0 = navi;
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    return R0;
}

/* register form: calls init_8013B4E outside this file, which has no typed C interface yet */
static void init_pet_navi_stats(u32 stats_index, u32 flag) {
    R0 = stats_index;
    R1 = flag;
    GUEST_CALL(init_8013B4E);
}

/*
 * Save the current PET Navi's stat block, run the stats-init routine,
 * then restore every stat so the navi keeps its battle values.
 *
 * Stat layout snapshotted (17 fields, one slot unused):
 *   byte:  0x0E, 0x21, 0x22, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
 *          0x09, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B
 *   hword: 0x3E, oNaviStats_CurHP (restored to 0x40)
 */
static void save_restore_pet_navi_stats(void) {
    u32 snap[17];  /* slot [8] of the original stack is never used; packed here */
    u32 i = 0;

    /* Snapshot all PET Navi (index 0) stats into the local buffer. */
    snap[i++] = get_pet_navi_byte(0, 0x0E);
    snap[i++] = get_pet_navi_byte(0, 0x21);
    snap[i++] = get_pet_navi_byte(0, 0x22);
    snap[i++] = get_pet_navi_hword(0, 0x3E);
    snap[i++] = get_pet_navi_byte(0, 0x2C);
    snap[i++] = get_pet_navi_byte(0, 0x2D);
    snap[i++] = get_pet_navi_byte(0, 0x2E);
    snap[i++] = get_pet_navi_byte(0, 0x2F);
    snap[i++] = get_pet_navi_byte(0, 0x30);
    snap[i++] = get_pet_navi_byte(0, 0x09);
    snap[i++] = get_pet_navi_hword(0, oNaviStats_CurHP);
    snap[i++] = get_pet_navi_byte(0, 0x56);
    snap[i++] = get_pet_navi_byte(0, 0x57);
    snap[i++] = get_pet_navi_byte(0, 0x58);
    snap[i++] = get_pet_navi_byte(0, 0x59);
    snap[i++] = get_pet_navi_byte(0, 0x5A);
    snap[i++] = get_pet_navi_byte(0, 0x5B);

    /* Run the per-navi stats initialization (e.g. apply equipment/Cross-Flavor). */
    u32 stats_index = get_pet_navi_stats_index(0);
    init_pet_navi_stats(stats_index, 0);

    /* Restore the saved stat block so the navi's battle values survive init. */
    i = 0;
    set_pet_navi_byte(0, 0x0E, snap[i++]);
    set_pet_navi_byte(0, 0x21, snap[i++]);
    set_pet_navi_byte(0, 0x22, snap[i++]);
    set_pet_navi_hword(0, 0x3E, snap[i++]);
    set_pet_navi_byte(0, 0x2C, snap[i++]);
    set_pet_navi_byte(0, 0x2D, snap[i++]);
    set_pet_navi_byte(0, 0x2E, snap[i++]);
    set_pet_navi_byte(0, 0x2F, snap[i++]);
    set_pet_navi_byte(0, 0x30, snap[i++]);
    set_pet_navi_byte(0, 0x09, snap[i++]);
    set_pet_navi_hword(0, 0x40, snap[i++]);  /* CurHP restored at stat 0x40 */
    set_pet_navi_byte(0, 0x56, snap[i++]);
    set_pet_navi_byte(0, 0x57, snap[i++]);
    set_pet_navi_byte(0, 0x58, snap[i++]);
    set_pet_navi_byte(0, 0x59, snap[i++]);
    set_pet_navi_byte(0, 0x5A, snap[i++]);
    set_pet_navi_byte(0, 0x5B, snap[i++]);
    /* The last SetCurPETNaviStatsByte leaves r1, r2, c, v for the caller. */
}

/* battle_refreshPETNaviStats_8136C24() — no meaningful inputs used; outputs (r1,r2,c,v) left by last callee GUEST_CALL. */
Next battle_refreshPETNaviStats_8136C24(void) {
    save_restore_pet_navi_stats();
    RETURN_TO_CALLER();
}

/* ── setUnkFieldOfChipCodeInChipPack_81376b8 ── */
/* setUnkFieldOfChipCodeInChipPack_81376b8(r0 = chip pack entry base, r1 = chip index):
 * Copies the current count kept at byte_20065B4 into the halfword field at
 * entry_base + 2*chip_index + 4, then decrements that count.
 * Leaves r1 = chip index * 2 and r2 = pointer to the count for the caller. */
Next setUnkFieldOfChipCodeInChipPack_81376b8(void) {
    u16 count = LD16(A_byte_20065B4);

    /* entry address: r0 - r1 + 2*r1 == r0 + r1 (byte offset of the entry) */
    u32 entry = R0 + R1;
    ST16(entry + 4, count);

    ST16(A_byte_20065B4, count - 1);

    R1 = R1 << 1;          /* byte offset of the entry's code field */
    R2 = A_byte_20065B4;
    RETURN_TO_CALLER();
}

/* ── loadU16FromTableEntry_81376CC ── */
/* loadU16FromTableEntry_81376CC(r0 = base pointer, r1 = index):
 * Return the u16 at base + index*2 + 4, i.e. entry [index+2] of the u16
 * array starting at r0.
 *
 * Note: the original also loads the halfword at byte_20065B4 into r3
 * (and its address into r2), but never uses either value -- dead loads
 * with no side effects, so they are dropped here. */
Next loadU16FromTableEntry_81376CC(void) {
    R0 = LD16(R0 + R1 + 4);
    RETURN_TO_CALLER();
}

/* ── clearToolkitUnk2002230RecordFields_81376E8 ── */
/* clearToolkitUnk2002230RecordFields_81376E8: clear the structure pointed to by the toolkit's
 * Unk2002230 pointer. It consists of 0x140 records of 12 bytes each;
 * for every record the four halfwords at offsets 4..0xA are zeroed
 * (the first four bytes are left untouched). Clobbers r0-r2. */
Next clearToolkitUnk2002230RecordFields_81376E8(void) {
    u32 record = LD32(R10 + oToolkit_Unk2002230_Ptr);

    for (u32 i = 0x140; i != 0; i--) {
        ST16(record + 4, 0);
        ST16(record + 6, 0);
        ST16(record + 8, 0);
        ST16(record + 0xA, 0);
        record += 0xC;
    }

    RETURN_TO_CALLER();
}

/* ── timer_setNoTimeout_8137700 ── */
/* timer_setNoTimeout_8137700: store 0x7FFF to byte_20065B4 (halfword), e.g. "no timer" value. */
Next timer_setNoTimeout_8137700(void) {
    ST16(A_byte_20065B4, 0x7FFF);
    R2 = 0xFF;
    RETURN_TO_CALLER();
}

/* ── isFolderSlotInUse_81377EC ── */
/* isFolderSlotInUse_81377EC(r0 = packed chip data): the high nibble of r0 is a
 * folder slot number. Scan the 3-byte in-use table at unk_20018EC (each entry
 * stores a slot number in its high nibble) and return the index of the entry
 * matching that slot, or 3 if no entry is using it.
 * Outputs: r0 = r1 = entry index (3 = not in use), r2 = last entry's slot
 * nibble, r3 = table address. */
Next isFolderSlotInUse_81377EC(void) {
    u32 slotNum = R0 >> 4;
    u32 table = A_unk_20018EC;

    u32 i = 0;
    while (i < 3) {
        R2 = (u32)LD8(table + i) >> 4;
        if (slotNum == R2)
            break;
        i++;
    }
    if (i == 3)
        R1 = 3;
    else
        R1 = i;
    R0 = R1;
    R3 = table;
    RETURN_TO_CALLER();
}

/* ── clearChipGiftRecordAndTakeChipsSlot0_8137808 ── */
/* clearChipGiftRecordAndTakeChipsSlot0_8137808:
 * Zero the 4-byte record at unk_20018EC, then hand the (now empty) record
 * to takeChipsWhenGivingFolderIfFolderSlotInUseMaybe_813781C with slot 0 in r0.
 * r4 is scratch for the record pointer here, but is restored for the caller. */
Next clearChipGiftRecordAndTakeChipsSlot0_8137808(void) {
    u32 savedR4 = R4;

    u32 record = A_unk_20018EC;

    /* ZeroFillByByte(record, 4) */
    R0 = record;
    R1 = 4;
    R4 = record;
    GUEST_CALL(ZeroFillByByte);

    /* takeChips...(0) — r4 still points at the freshly cleared record */
    R0 = 0;
    GUEST_CALL(takeChipsWhenGivingFolderIfFolderSlotInUseMaybe_813781C);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_8137884 ── */
/* sub_8137884(r0 = index):
 * look up a byte in the table at unk_20018EC and return it. */
Next sub_8137884(void) {
    R0 = LD8(A_unk_20018EC + R0);
    RETURN_TO_CALLER();
}
