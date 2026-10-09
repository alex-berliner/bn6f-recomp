/*
 * leaves_farm_15.c: small functions from the model farm, set 15.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── setSlotFieldsFromStartTo3_811A1F4 ── */
/* setSlotFieldsFromStartTo3_811A1F4(r0 = halfword value, r1 = byte value, r2 = first slot, r3 = halfword value):
 * Walk the entity slots starting at r2, up to but not including slot 4, and for
 * every slot that currently has a struct registered, refresh three of its fields:
 *   +0x0C (byte)  = r1
 *   +0x0E (half)  = r0
 *   +0x18 (half)  = r3
 * getStructFrom2008450 signals "no struct for this slot" by returning r0 = 0
 * (that is what the original's beq tests); the struct address to write to comes
 * back in r1 and may hold junk when the slot is empty, so only r0 may be tested.
 * Although the original moves the loop counter into r4, it saves and restores
 * r4 around the routine (push {r4,lr} / pop {r4,pc}), so r4 is left unchanged. */
Next setSlotFieldsFromStartTo3_811A1F4(void) {
    u16 val0E = (u16)R0;
    u8  val0C = (u8)R1;
    u16 val18 = (u16)R3;

    u32 slot = R2;
    do {
        R0 = slot;
        GUEST_CALL(getStructFrom2008450);
        if (R0 != 0) {
            ST16(R1 + 0xE, val0E);
            ST8(R1 + 0xC, val0C);
            ST16(R1 + 0x18, val18);
        }
        slot++;
    } while ((s32)slot < 4);

    RETURN_TO_CALLER();
}

/* ── sprite_moveSlotGroupXY_811A220 ── */
/* sprite_moveSlotGroupXY_811A220(r0 = x, r1 = y, r2 = first slot index):
 * For the four object slots starting at the index in r2, look each entry up
 * in the table at 0x2008450 and, if an object exists there, move its sprite
 * to (x, y).
 *
 * Flags on return come from the final slot/index comparison (callers test V).
 * r2/r3 are whatever the last callee left them as; r4-r7 are preserved. */
Next sprite_moveSlotGroupXY_811A220(void) {
    u32 x = R0;
    u32 y = R1;
    u32 slot = R2;
    u32 slotEnd = slot + 4;
    u32 entryR5 = R5; /* clobbered as a call argument below, restored on exit */

    for (;;) {
        R0 = slot;
        GUEST_CALL(getStructFrom2008450);

        if (R0 != 0) {
            /* sprite_setCoordinates takes the object's sprite handle in r5 */
            R5 = R1;
            R0 = x;
            R1 = y;
            GUEST_CALL(sprite_setCoordinates);
        }

        slot++;
        SET_FLAGS_CMP(slot, slotEnd);
        if ((s32)slot >= (s32)slotEnd)
            break;
    }

    R5 = entryR5;
    RETURN_TO_CALLER();
}

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

/* ── LoadVirusNameIDsForUpToFourSlots_800A998 ── */
/* LoadVirusNameIDsForUpToFourSlots_800A998(r0 = name_id_arr: *u16[4]):
 * Collects the NameIDs of the living actors on one side of the battle
 * (the side selected by oBattleState_Unk_0d, flipped via 1^side) into
 * the caller's 4-slot u16 array, skipping empty actor slots.
 * Returns the number of entries written.
 * r4 is saved and restored by the original, so it stays unchanged. */
Next LoadVirusNameIDsForUpToFourSlots_800A998(void) {
    u32 out = R0;

    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    /* 1 ^ side: flip the actor list selector. */
    u32 side = 1 ^ LD8(battleState + oBattleState_Unk_0d);
    u32 actors = battleState + side * 0x10 + oBattleState_AliveBattleActors;

    u32 byteOffset = 0;
    for (u32 i = 0; i < 4; i++) {
        u32 actor = LD32(actors + i * 4);
        if (actor != 0) {
            ST16(out + byteOffset, LD16(actor + oBattleObject_NameID));
            byteOffset += 2;
        }
    }

    R0 = byteOffset >> 1; /* number of entries to process */
    RETURN_TO_CALLER();
}

/* ── resetAllGraphicsState_8005F40 ── */
/* resetAllGraphicsState_8005F40: reset all graphics state for a fresh scene.
 * Clears VRAM and the various work RAM buffers, installs the dummy
 * BG scroll callbacks, clears the extended palettes, and resets the
 * render info structures. Takes no arguments and returns none, but
 * r2-r7 pass through unchanged. */
Next resetAllGraphicsState_8005F40(void) {
    GUEST_CALL(zeroFillVRAM);                        /* blank out VRAM */
    GUEST_CALL(ZeroFill_byte_3001960);               /* clear sprite/OAM work area */
    GUEST_CALL(SetDummyBGScrollCallbacks);           /* no-op BG scroll until a scene sets real ones */
    GUEST_CALL(zeroFill_e20094C0);                    /* clear EWRAM graphics buffer */
    GUEST_CALL(palette_clearEPalette20097a0_80023A8); /* clear extended palette RAM */
    GUEST_CALL(zeroFill_e2009740);                    /* clear second EWRAM buffer */
    GUEST_CALL(zeroFill_e200F3A0);                    /* clear IWRAM buffer */
    GUEST_CALL(zeroFill_80024A2);                     /* clear misc render state */
    GUEST_CALL(renderInfo_8001788);                   /* reset render info block 1 */
    GUEST_CALL(renderInfo_80017A0);                   /* reset render info block 2 */
    RETURN_TO_CALLER();
}

/* ── pack_addChipByIdSetLibraryFlag_8021B2A ── */
/* pack_addChipByIdSetLibraryFlag_8021B2A(r0 = chip id, r1 = search item, r2 = amount):
 * Adds copies of a chip to the spare chip pack. First re-applies the pack
 * encryption; if an amount was requested, sets the chip's library event flag
 * (flag 0x1E20 + chip id), finds the pack slot holding that chip's code, and
 * adds the chips there. Returns the result addChipsToChipPackOffset leaves in
 * r3, or 0 when the amount is zero. r2/r4-r7 are preserved for the caller. */
Next pack_addChipByIdSetLibraryFlag_8021B2A(void) {
    u32 chip_id = R0;
    u32 search_item = R1;
    u32 amount = R2;

    /* re-apply the chip pack encryption before touching the pack */
    R0 = chip_id;
    GUEST_CALL(encryption_applyPack_8006e70);

    if (amount == 0) {
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* record the chip in the event flags (library flag base 0x1E20) */
    R0 = 0x1E20 + chip_id;
    GUEST_CALL(SetEventFlag);

    /* locate the quantity slot in the pack for this chip's code */
    R0 = chip_id;
    R1 = search_item;
    R2 = amount;
    GUEST_CALL(getOffsetToQuantityOfChipCodeMaybe_8021c7c);
    u32 quantity_offset = R0;

    /* add the chips at that slot */
    R0 = quantity_offset;
    R2 = amount;
    GUEST_CALL(addChipsToChipPackOffset_8021b5a);

    R0 = R3; /* pack update result */
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
