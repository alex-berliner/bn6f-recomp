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
    GUEST_CALL(sub_811EC68);
    GUEST_CALL(sub_811EFB8);
    GUEST_CALL(updateMapNameDisplay_811F014);
    GUEST_CALL(drawMapNamesScreen_811F050);
    GUEST_CALL(drawCurrentMapName_811F08C);
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
    GUEST_CALL(sub_811F11C);
    GUEST_CALL(CopyBackgroundTiles_PET_811EF70);
    GUEST_CALL(CopyBackgroundTiles_onNaviSelect_811F54C);
    RETURN_TO_CALLER();
}

/* ── updateMapNameDisplay_811F014 ── */
/* updateMapNameDisplay_811F014: refresh the map-name display.
 *
 * 1. sub_803D06C yields a pointer (the current map's name/script data) that
 *    the scene state keeps at +8.
 * 2. The map name table text is rendered as graphics into the tile block at
 *    0x6015240 via the scratch text buffer unk_200C0E0.
 * 3. sub_811F5DC copies the rendered name into the byte_200CF20 buffer. */
Next updateMapNameDisplay_811F014(void) {
    /* r4-r7 are used as call arguments below but must be preserved for the
     * caller, so keep the entry values aside and restore them before returning. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Current map name pointer -> scene state +8. */
    GUEST_CALL(sub_803D06C);
    u32 sceneState = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(sceneState + 8, R0);

    /* Render the map name text (script 0xF1, palette 7, one line) into VRAM. */
    R0 = A_text_86CB360;      /* TextScriptMapNames */
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
    GUEST_CALL(sub_811F5DC);

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
    R0 = A_text_86CB360;    /* TextScriptMapNames */
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
    GUEST_CALL(sub_811F5DC);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── drawCurrentMapName_811F08C ── */
/* drawCurrentMapName_811F08C: redraw the name of the current map in the map-name display.
 *
 * sub_811F290 resolves the text-script entry for the map the player is
 * currently on; renderTextGfx_8045F8C draws that name into the text work
 * buffer at byte_200C620; sub_811F5DC then blits the rendered tiles into
 * the map-name VRAM slot at 0x6015780. */
Next drawCurrentMapName_811F08C(void) {
    /* The original pushes/pops r4-r7 around the calls, so they must come
     * back out of this routine exactly as they came in. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Look up which map-name string to show. */
    GUEST_CALL(sub_811F290);
    u32 mapNameString = R0;

    /* renderTextGfx_8045F8C(stringTable, stringIndex, workBuffer,
     *                        0, 12, 1, fontTable, 0):
     * render the map name text into the work buffer. */
    R0 = A_text_86CB360;       /* TextScriptMapNames string table */
    R1 = mapNameString;
    R2 = A_byte_200C620;       /* rendered-text work buffer */
    R3 = 0;
    R4 = 0xC;                  /* render width */
    R5 = 1;
    R6 = A_dword_86A5D60;      /* font/metadata table */
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* sub_811F5DC(source, sourceMap, vramDest, tileCount):
     * upload the rendered name tiles to VRAM. */
    R0 = A_byte_200C620;
    R1 = A_byte_200C920;
    R2 = 0x6015780;
    R3 = 0xC;
    GUEST_CALL(sub_811F5DC);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
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

/* ── sub_811F5DC ── */
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

/* sub_811F5DC(r0=src, r1=dst, r2=width, r3=height) -> c v r0 r1 r2 r3 */
Next sub_811F5DC(void) {
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
    GUEST_CALL(sub_80015FC);

    /* reset the chatbox state, then refresh the text window */
    GUEST_CALL(sub_809F68C);
    GUEST_CALL(chatbox_8040818);

    /* clear chatbox event flag 0x40 */
    R0 = 0x40;
    GUEST_CALL(chatbox_clear_eFlags2009F38);

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

/* ── battle_ClearBuffer_811FE34 ── */
/* battle_ClearBuffer_811FE34(r0 = buffer address, r2/r3 = preserved by callee contract):
 * zero-fill 0x3C0 bytes at r0 via ZeroFillByEightWords.
 * Leaves r0, r2, r3 untouched; r1 still holds the fill size afterwards. */
Next battle_ClearBuffer_811FE34(void) {
    R1 = 0x3C0;                 /* size consumed by ZeroFillByEightWords */
    GUEST_CALL(ZeroFillByEightWords);
    RETURN_TO_CALLER();
}

/* ── sub_811FE7C ── */
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

/* sub_811FE7C(r0 = dest buffer, r1 = mode) -> r0 = entry count */
Next sub_811FE7C(void) {
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

/* ── sub_8120018 ── */
/* Row stride between battle-object field arrays (bytes). */
#define ROW_STRIDE 0x20u
/* Sentinel marking a zero-lower-half entry during the mode-0 lock/unlock pass. */
#define FIELD_SENTINEL 0x7FFFFFFFu

/* Output registers left by the function's second pass. */
typedef struct {
    u32 r2, r3, r4, r6, r7;
} PassResult;

/* register form: calls the mode-specific handler between the two passes
 * (sub_30069A8 for mode 0, sub_3006A5E for mode 4); neither has a typed
 * C interface yet. Returns whatever the handler leaves in r4. */
static u32 call_handler(u32 mode, u32 base, u32 count, u32 field) {
    R0 = base;
    R1 = count;
    R2 = field;
    R3 = mode;
    if (mode == 0)
        GUEST_CALL(sub_30069A8);
    else
        GUEST_CALL(sub_3006A5E);
    return R4;
}

/* First pass – prepare field entries before the handler runs.
 * mode 0: if an entry's lower 16 bits are zero, stamp it with FIELD_SENTINEL.
 * mode != 0: invert the lower 16 bits of each entry (upper 16 preserved). */
static void pass_prepare(u32 base, u32 count, u32 field, u32 mode) {
    u32 offset = field << 2;
    s32 i = (s32)count - 1;
    do {
        u32 addr = base + (u32)i * ROW_STRIDE + offset;
        u32 val = LD32(addr);
        if (mode == 0) {
            if ((val & 0xFFFFu) == 0)
                ST32(addr, FIELD_SENTINEL);
        } else {
            ST32(addr, (val & 0xFFFF0000u) | (~val & 0xFFFFu));
        }
        i--;
    } while (i >= 0);
}

/* Second pass – restore/process field entries after the handler runs.
 * mode 0: clear entries still holding FIELD_SENTINEL back to zero.
 * mode != 0: invert the lower 16 bits again (restores the pre-first-pass value
 *   if the handler left the upper 16 alone).
 * Returns the register state left at loop exit (r2 = byte offset, r3 = last
 *   processed/stored value, r4 = handler passthrough for mode 0 / upper-16 for
 *   mode != 0, r6 = row byte offset of last iteration, r7 = negative sentinel). */
static PassResult pass_finish(u32 base, u32 count, u32 field, u32 mode, u32 handler_r4) {
    PassResult res;
    res.r2 = field << 2;
    u32 offset = res.r2;

    s32 i = (s32)count - 1;
    u32 r6 = 0, r3 = 0, r4 = handler_r4;

    do {
        r6 = (u32)i * ROW_STRIDE;
        u32 row = base + r6;
        u32 val = LD32(row + offset);

        if (mode == 0) {
            if (val == FIELD_SENTINEL) {
                ST32(row + offset, 0);
                r3 = 0;
            } else {
                r3 = val;
            }
            /* r4 retains the handler's value – untouched by this loop */
        } else {
            u32 upper = val & 0xFFFF0000u;
            r4 = upper;
            r3 = upper | (~val & 0xFFFFu);
            ST32(row + offset, r3);
        }
        i--;
    } while (i >= 0);

    res.r3 = r3;
    res.r4 = r4;
    res.r6 = r6;
    res.r7 = (u32)i;
    return res;
}

/* sub_8120018(base, count, field, mode): two-pass field lock/unlock wrapping a
 * mode-dispatched handler that can inspect or modify the field array in between. */
static PassResult sub_8120018_run(u32 base, u32 count, u32 field, u32 mode) {
    pass_prepare(base, count, field, mode);
    u32 handler_r4 = call_handler(mode, base, count, field);
    return pass_finish(base, count, field, mode, handler_r4);
}

/* Next sub_8120018(r0=base, r1=count, r2=field, r3=mode) → nz, r2-r7 */
Next sub_8120018(void) {
    PassResult res = sub_8120018_run(R0, R1, R2, R3);
    R2 = res.r2;
    R3 = res.r3;
    R4 = res.r4;
    R6 = res.r6;
    R7 = res.r7;
    SET_FLAGS_NZ(res.r7);
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

/* ── sub_81207F8 ── */
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

/* sub_81207F8(r0 = chip_type) -> r0 = qualifying chip count; r1/r2/r3 = callee scratch.
 * r4-r7 and lr are preserved via the function-level push/pop in the original;
 * the GUEST_CALLs' callees also preserve them per SYNC_SNAP.
 * r1/r2/r3 are clobbered by the last external call or TST, matching the original. */
Next sub_81207F8(void) {
    R0 = count_event_chips((s32)R0);
    RETURN_TO_CALLER();
}

/* ── sub_8120900 ── */
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

/* sub_8120900(r0=tile_map, r1=val1, r2=val2, r3=base_tile, r4=j, r6=i) */
Next sub_8120900(void) {
    build_bcd_tilemap(R0, R1, R2, R3, R4, R6);
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

/* ── sub_8120D10 ── */
/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u32 get_cur_pet_navi_sub_8120D10(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u32 get_pet_navi_stat_sub_8120D10(u32 navi_id, u32 index) {
    R0 = navi_id; R1 = index;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return R0;
}

/* register form: calls countNaviFolderChipsWithElement_8120AE8 outside this file, which has no typed C interface yet */
static u32 count_folder_chips(u32 element) {
    R0 = element;
    GUEST_CALL(countNaviFolderChipsWithElement_8120AE8);
    return R0;
}

/* register form: calls GiveFolder outside this file, which has no typed C interface yet */
static void give_folder(u32 folder_id) {
    R0 = folder_id;
    GUEST_CALL(GiveFolder);
}

/* register form: calls isFolderSlotInUse_81377EC outside this file, which has no typed C interface yet */
static u32 folder_slot_status(u32 folder_id) {
    R0 = folder_id;
    GUEST_CALL(isFolderSlotInUse_81377EC);
    return R0;
}

/* register form: calls sub_8137890 outside this file, which has no typed C interface yet */
static void try_folder_swap(u32 slot) {
    R0 = slot;
    GUEST_CALL(sub_8137890);
}

/* register form: calls sub_8120DAC outside this file, which has no typed C interface yet */
static bool folder_swap_succeeded(void) {
    GUEST_CALL(sub_8120DAC);
    return !ZERO_FLAG();
}

/* register form: calls SetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static void set_pet_navi_stat_sub_8120D10(u32 navi_id, u32 index, u32 value) {
    R0 = navi_id; R1 = index; R2 = value;
    GUEST_CALL(SetCurPETNaviStatsByte);
}

/* Decide whether to give the element-0x22 folder to the current PET Navi.
 * Returns the action code: 0 = skip, 0xF = pending, 0x10 = folder given
 * (slot freed directly), 0x11 = folder given after swap. */
static u32 pet_navi_folder_logic(u32 input_param, u32 toolkit) {
    u32 navi_id = get_cur_pet_navi_sub_8120D10();
    u32 status = 0;

    /* Check water (elem 1): give if chip count exceeds stat index 0x0B */
    u32 stat_b = get_pet_navi_stat_sub_8120D10(navi_id, 0xB);
    u32 chips_e1 = count_folder_chips(1);
    bool proceed = ((s32)chips_e1 > (s32)stat_b);

    /* Check fire (elem 2): give if chip count exceeds stat index 0x0C */
    if (!proceed) {
        u32 stat_c = get_pet_navi_stat_sub_8120D10(navi_id, 0xC);
        u32 chips_e2 = count_folder_chips(2);
        proceed = ((s32)chips_e2 > (s32)stat_c);
    }

    if (!proceed)
        return status;

    status = 0xF;

    /* field5 is the folder-slot usage count for element 0x22 */
    u32 s2001c04 = LD32(toolkit + oToolkit_S2001c04_Ptr);
    u32 field5 = LD8(s2001c04 + 5);

    if (field5 == 1) {
        /* Slot is empty (usage count 1 = only the folder header): give directly */
        status = 0x10;
        give_folder(0x22);
        set_pet_navi_stat_sub_8120D10(navi_id, 0x2D, field5 - 1);
    } else {
        u32 slot = folder_slot_status(0x22);

        if ((s32)slot >= 3) {
            /* Slot heavily used: still give the folder */
            status = 0x10;
            give_folder(0x22);
            set_pet_navi_stat_sub_8120D10(navi_id, 0x2D, field5 - 1);
        } else if (input_param != 0) {
            /* Caller has a pending request: defer, just log the status */
            set_pet_navi_stat_sub_8120D10(navi_id, 0x2D, slot);
        } else {
            /* Attempt to free a slot via folder-swap logic */
            try_folder_swap(slot);
            if (folder_swap_succeeded()) {
                status = 0x11;
                give_folder(0x22);
                set_pet_navi_stat_sub_8120D10(navi_id, 0x2D, field5 - 1);
            } else {
                set_pet_navi_stat_sub_8120D10(navi_id, 0x2D, slot);
            }
        }
    }

    return status;
}

/* sub_8120D10(r0 = input_param, r4, r5, r6, r7, r10 = toolkit, lr) -> r1 = status, nz */
Next sub_8120D10(void) {
    u32 status = pet_navi_folder_logic(R0, R10);
    R1 = status;
    SET_FLAGS_NZ(R1);
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
