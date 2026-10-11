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
