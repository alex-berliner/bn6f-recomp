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

/* ── sub_813399C ── */
/* sub_813399C: virtual-method dispatch on the object at r5, followed by a
 * fixed tail call.
 *
 * The byte at (r5 + 2) selects a handler from the two-entry table at
 * off_81339B4 — both entries hold sub_81339BC+1, so the fetched pointer is
 * always that function. The original performs the call with the classic
 * "mov lr, pc / bx r0" idiom: the handler is entered with R0 still holding
 * the fetched table entry (its own address) and R1 holding the selector,
 * and it returns straight into the call below. */
Next sub_813399C(void) {
    u32 method = LD8(R5 + 2);
    u32 handler = LD32(A_off_81339B4 + method); /* both entries: sub_81339BC+1 */

    R0 = handler;
    R1 = method;
    GUEST_CALL(sub_81339BC);

    GUEST_CALL(sub_8134490);
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

/* ── sub_8134538 ── */
/* sub_8134538: thin wrapper that invokes sub_8134558 with the fixed argument
 * set (226, 28, 30, 36); result and condition flags are whatever the callee
 * leaves behind. r4-r7 pass through untouched. */
Next sub_8134538(void) {
    R0 = 0xE2;
    R1 = 0x1C;
    R2 = 0x1E;
    R3 = 0x24;
    GUEST_CALL(sub_8134558);
    RETURN_TO_CALLER();
}

/* ── sub_8134548 ── */
/* sub_8134548: tail-call wrapper for sub_8134558 with fixed arguments
 *   r0 = 0x182, r1 = 0x1C, r2 = 0x28, r3 = 0x2E.
 * Registers r0-r3 and the condition flags are whatever sub_8134558 leaves. */
Next sub_8134548(void) {
    R0 = 0x182;
    R1 = 0x1C;
    R2 = 0x28;
    R3 = 0x2E;
    GUEST_CALL(sub_8134558);
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

/* ── sub_81353DC ── */
/* sub_81353DC: thin wrapper around sub_81348DC. It invokes that routine with
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
    GUEST_CALL(sub_81348DC);

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
