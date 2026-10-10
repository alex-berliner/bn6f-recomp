/*
 * leaves_farm_29.c: small functions from the model farm, set 29.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

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

/* ── applyNaviStatsMaybe_813C458 ── */
/* applyNaviStatsMaybe_813C458:
 * Re-apply the equipped NaviCust programs / PET navi stats. Runs the whole
 * "refresh navi customisation" pipeline: rebuild some derived tables, reset
 * the current PET navi to slot 0, clear a story event flag, re-apply the
 * navicust programs for that navi, run follow-up fixups, then restore the
 * navi the player actually had selected and refresh the HUD.
 *
 * All arguments are passed through untouched in r2, r5-r12; r4 is
 * callee-saved (the original pushes/pops it around the calls). */
Next applyNaviStatsMaybe_813C458(void) {
    u32 callerR4 = R4; /* saved across the call sequence, like push {r4} */

    GUEST_CALL(sub_813BBD4); /* rebuild navi-related tables */
    GUEST_CALL(sub_8136C24); /* ... */

    GUEST_CALL(GetCurPETNavi);
    u32 naviId = R0;

    /* Temporarily select navi slot 0 so its (stock) stats get applied */
    R0 = 0;
    GUEST_CALL(SetCurPETNavi);

    GUEST_CALL(sub_813C678); /* apply the stock navi's stats */

    /* ClearEventFlagFromImmediate(EVENT_170D): movflag loads r0=0x17, r1=0xD */
    R0 = 0x17;
    R1 = 0xD;
    GUEST_CALL(ClearEventFlagFromImmediate);

    /* applyNavicustPrograms takes the navi whose programs to apply in r4 */
    R4 = naviId;
    GUEST_CALL(applyNavicustPrograms_813C684);

    GUEST_CALL(sub_813CBCC); /* post-application fixups */

    /* Put the player's actual navi back and refresh its display */
    R0 = naviId;
    GUEST_CALL(SetCurPETNavi);

    GUEST_CALL(sub_803CE44);

    R4 = callerR4; /* pop {r4} */
    RETURN_TO_CALLER();
}

/* ── GiveChips ── */
/* GiveChips(r0 = chip id, r1 = chip code, r2 = count, ...):
 * Grant `count` copies of the given chip to the player's pack.
 * Re-applies the pack's obfuscation first, marks the chip as obtained
 * in the chip library (event flag 0x1E20 + chip id), locates the pack
 * slot holding that chip's code, adds the copies there, and refreshes
 * the slot's bookkeeping field. Returns 0 when count is 0, otherwise
 * the value the pack-add helper leaves in r3. */
Next GiveChips(void) {
    u32 chipId   = R0;
    u32 chipCode = R1;
    u32 count    = R2;
    u32 savedR4  = R4;   /* restored on exit; borrowed as scratch below */

    /* Re-encrypt/verify the chip pack before modifying it. */
    R0 = chipId;
    R1 = chipCode;
    R2 = count;
    GUEST_CALL(encryption_applyPack_8006e70);

    if (count == 0) {
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* Mark the chip as collected in the chip library. */
    R0 = 0x1E20 + chipId;
    GUEST_CALL(SetEventFlag);

    /* Find the pack slot (and its code field) for this chip. */
    R0 = chipId;
    R1 = chipCode;
    R2 = count;
    GUEST_CALL(getOffsetToQuantityOfChipCodeMaybe_8021c7c);
    u32 slotOffset = R0;
    u32 codeField  = R3;

    /* Add the requested number of copies to that slot.
     * r1 is still whatever the lookup left there, as in the original. */
    R2 = count;
    R4 = codeField;
    GUEST_CALL(addChipsToChipPackOffset_8021b5a);
    u32 result = R3;    /* saved across the next call, like the original's push/pop */

    /* Update the per-code bookkeeping field for that slot. */
    R1 = codeField;
    GUEST_CALL(setUnkFieldOfChipCodeInChipPack_81376b8);

    R0 = result;
    R3 = result;
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── npc_stateDispatchStep_80A51C4 ── */
/* npc_stateDispatchStep_80A51C4(r5 = state struct):
 * Points r7 at a 0x10-byte record in the table at byte_80A4C64, selected by
 * the byte at r5+4, then calls one of five handlers selected by the byte at
 * r5+8, which indexes the table at off_80A51E0 bytewise (entries are 4 bytes
 * apart, so the selector holds 0x00/0x04/0x08/0x0C/0x10):
 *   +0x00 sub_80A51F8   +0x04 sub_80A5268   +0x08 sub_80A52E4
 *   +0x0C sub_80A5304   +0x10 sub_80A5350
 * The original branches through r0, so the handler is entered with
 * r0 = handler address, r1 = raw selector byte, r7 = selected record. */
Next npc_stateDispatchStep_80A51C4(void) {
    u32 recordBase = A_byte_80A4C64;
    u32 recordOff  = 0x10 * (u32)LD8(R5 + 4);

    R7 = recordBase + recordOff;
    SET_FLAGS_ADD(recordBase, recordOff);

    u32 sel    = (u32)LD8(R5 + 8);
    u32 target = LD32(A_off_80A51E0 + sel);

    R0 = target;
    R1 = sel;

    switch (sel) {
    case 0x00: GUEST_CALL(sub_80A51F8); break;
    case 0x04: GUEST_CALL(sub_80A5268); break;
    case 0x08: GUEST_CALL(sub_80A52E4); break;
    case 0x0C: GUEST_CALL(sub_80A5304); break;
    case 0x10: GUEST_CALL(sub_80A5350); break;
    /* Selector outside the table: branch to whatever the load yielded. */
    default:   dispatch(target);        break;
    }

    RETURN_TO_CALLER();
}

/* ── initUncompSpriteState_80028d4 ── */
/* initUncompSpriteState_80028d4(r0 = sprite data pointer):
 * Resets the uncompressed-sprite work block at byte_200DCA0 before a new
 * sprite is decompressed into it:
 *   - the whole 0x50-byte block is zeroed,
 *   - the given data pointer is kept at byte_200DCEC (+0x4C),
 *   - the 0x18-halfword table at +0x04 is filled with 0x7FFF,
 *   - the 0x30-byte buffer at +0x1C is filled with 0xFF.
 * Returns with r2 = 0xFF (last fill value) and the flags left by ByteFill;
 * r3 and r5 are untouched as far as callers are concerned. */
Next initUncompSpriteState_80028d4(void) {
    u32 callerR5 = R5;   /* original pushes r5 and pops it back at the end */
    u32 dataPtr = R0;    /* original spills r0 around the first fill call */
    u32 block = A_byte_200DCA0;

    /* zero the entire state block */
    R0 = block;
    R1 = 0x50;
    GUEST_CALL(ZeroFillByWord);

    /* remember which sprite data this state belongs to */
    ST32(block + 0x4C, dataPtr); /* dword_200DCEC */

    /* clear the halfword table at +4 (0x80<<24 inverted, shifted = 0x7FFF) */
    R0 = block + 4;
    R1 = 0x18;
    R2 = 0x7FFF;
    GUEST_CALL(HalfwordFill);

    /* clear the tile buffer at +0x1C */
    R0 = block + 0x1C;
    R1 = 0x30;
    R2 = 0xFF;
    GUEST_CALL(ByteFill);

    R5 = callerR5;
    RETURN_TO_CALLER();
}

/* ── logoScreen_dispatch_803D1CA ── */
/* logoScreen_dispatch_803D1CA:
 * Logo screen state machine tick. Reads the current state from
 * eLogoScreenState (offset 0) and invokes the matching state handler with
 * the state pointer in r5, then animates the "PRESS START" text.
 *
 * The state field holds a byte offset into the handler table (0x00, 0x04,
 * 0x08, 0x0C, 0x10) -- the original loads the entry with `ldr r0, [table, r1]`
 * with no shift -- so dispatch on that raw offset. Each handler is entered
 * with its own table entry (Thumb address, bit 0 set) in r0, the raw state
 * offset in r1 and the state pointer in r5, mirroring
 * `mov lr, pc; bx r0` in the original. Whatever r0/r1 the handler leaves is
 * carried into the startScreen call afterwards, since the original's `bl`
 * does not set up arguments.
 *
 * r4-r7 are saved around the calls (the original's push/pop), so the
 * caller's copies survive whatever the handlers do to them. */
Next logoScreen_dispatch_803D1CA(void) {
    u32 state = A_eLogoScreenState_Index_00;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    u32 index = LD8(state);                     /* oLogoScreenState_Index_00 */
    u32 handler = LD32(A_off_803D1E4 + index);  /* table entry, Thumb bit set */

    R5 = state;
    R0 = handler;
    R1 = index;

    switch (index) {
    case 0x00: /* logo fade-in / init */
        GUEST_CALL(sub_803D1FC);
        break;
    case 0x04:
        GUEST_CALL(sub_803D24C);
        break;
    case 0x08:
        GUEST_CALL(sub_803D274);
        break;
    case 0x0C:
        GUEST_CALL(sub_803D298);
        break;
    case 0x10: /* done -- hand control back to the scene manager */
        GUEST_CALL(logoScreen_finish_803D2A6);
        break;
    }

    /* The state field only ever holds one of the five offsets above, so the
     * jump table never lands anywhere else. */
    GUEST_CALL(startScreen_AnimatePressStart_803E938);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}
