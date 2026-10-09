/*
 * leaves_farm_27.c: small functions from the model farm, set 27.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_8035756 ── */
/* sub_8035756:
 * Scan the 32-slot table at dword_2000DC0 (8 bytes per slot: +0 = slot's
 * current value, +4 = pending request) and claim the first slot that has a
 * request queued while the slot itself is free: the request is returned and
 * both fields are cleared. Slots with no pending request have their current
 * value cleared; busy slots are left untouched. Returns the claimed request,
 * or 0 if every slot was busy or empty. Flags reflect the returned value. */
Next sub_8035756(void) {
    u32 slot = A_dword_2000DC0;

    for (u32 i = 0; i < 0x20; i++, slot += 8) {
        u32 pending = LD32(slot + 4);
        if (pending == 0) {
            /* Nothing queued on this slot: release it. */
            ST32(slot, 0);
            R3 = 0;
            continue;
        }

        u32 current = LD32(slot);
        if (current != 0) {
            /* Slot is busy and still has a request pending: leave it alone. */
            R3 = current;
            continue;
        }

        /* Free slot with a pending request: take the request and clear the slot. */
        R0 = pending;
        R3 = 0;
        ST32(slot + 4, 0);
        ST32(slot, 0);
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    /* All 32 slots checked, none available. */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── triggerSomeUpdateForInternetOrRealWorld_8033948 ── */
/* Field offsets used below that game.h does not define (from the disassembly):
 *   oGameState_MapGroup        = 4   (current map group inside oGameState)
 *   oS2011E30_Idx_04           = 4   (pending-update selector in the work block)
 *   oS2011E30_CurPETNaviMaxHP  = 8   (halfword: current PET navi's max HP)
 */
#define oGameState_MapGroup        4
#define oS2011E30_Idx_04           4
#define oS2011E30_CurPETNaviMaxHP  8

/* triggerSomeUpdateForInternetOrRealWorld_8033948:
 * Prepare the 16-byte work block at eS2011E30 for an area update. The block is
 * zeroed, then its update selector (offset 4) is set to 1 so that
 * dispatch_80339CC runs onUpdateInRealWorld_8033A96 on real-world maps or
 * onUpdateInInternet_8033B1E on internet maps; on internet maps the current PET
 * navi's max HP is also stored into the block first. Finally the block's
 * graphics transfer is performed.
 *
 * r2/r3 pass through untouched; r4-r7 are preserved (the original push/pops
 * them around the body, while its callees may clobber the copies it keeps in
 * r5/r7). Flags on return are whatever gfxTransfer_8033978 leaves behind. */
Next triggerSomeUpdateForInternetOrRealWorld_8033948(void) {
    u32 entryR4 = R4, entryR5 = R5, entryR6 = R6, entryR7 = R7;
    u32 block = A_eS2011E30;
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 mapGroup = LD8(gameState + oGameState_MapGroup);

    /* Zero the whole 16-byte work block. */
    R0 = block;
    R1 = 0x10;
    GUEST_CALL(ZeroFillByWord);

    if ((s32)mapGroup >= INTERNET_MAP_GROUP_START) {
        /* Internet area: record the current PET navi's max HP, then flag the
         * update so dispatch runs onUpdateInInternet_8033B1E. */
        GUEST_CALL(GetCurPETNavi);
        R5 = block;
        R7 = gameState;
        GUEST_CALL(GetMaxAndCurHPForCurPETNavi_80010D4);
        ST16(block + oS2011E30_CurPETNaviMaxHP, R0);
        R0 = 1;
        ST8(block + oS2011E30_Idx_04, R0);
        /* gfxTransfer_8033978 receives the block in r5 and the game state
         * pointer in r7 (the original keeps both live across the call). */
        R5 = block;
        R7 = gameState;
    } else {
        /* Real-world area: just flag the update so dispatch runs
         * onUpdateInRealWorld_8033A96. */
        R7 = 1;
        ST8(block + oS2011E30_Idx_04, R7);
        R5 = block;
    }

    /* Run the work block's graphics transfer. */
    GUEST_CALL(gfxTransfer_8033978);

    /* The original's push {r4-r7,lr} / pop {r4-r7,pc} restores these even
     * though the callees were free to clobber r5/r7. */
    R4 = entryR4;
    R5 = entryR5;
    R6 = entryR6;
    R7 = entryR7;
    RETURN_TO_CALLER();
}

/* ── chatbox_8040C9C ── */
/* Chatbox state offsets within the chatbox work area (r5). */
#define CHATBOX_NEXT_STATE  0x1F0  /* mode/state byte this function updates */
#define CHATBOX_1F1         0x1F1  /* read by the original, result unused */
#define CHATBOX_1F2         0x1F2  /* cleared when restarting from a mid state */

/* chatbox_8040C9C(r5 = chatbox work area):
 * Advance the chatbox state byte at +0x1F0. State 0 stays 0, state 3 stays 3,
 * any other state clears +0x1F2 (touching +0x1F1 as the original does) and
 * moves to state 1.
 * r0/r1 are preserved; c/v are left as the original's last CMP leaves them
 * (CMP #0 when the state was 0, CMP #3 otherwise). */
Next chatbox_8040C9C(void) {
    u32 savedR0 = R0;
    u32 savedR1 = R1;

    u32 state = LD8(R5 + CHATBOX_NEXT_STATE);

    if (state == 0) {
        /* Fresh chatbox: nothing to do, state stays 0. */
        SET_FLAGS_CMP(state, 0);
        R0 = 0;
    } else if (state == 3) {
        /* Already fully advanced: hold at 3. */
        SET_FLAGS_CMP(state, 3);
        R0 = 3;
    } else {
        /* Interrupted mid-way: reset the sub-state and restart at 1. */
        SET_FLAGS_CMP(state, 3);
        ST8(R5 + CHATBOX_1F2, 0);
        (void)LD8(R5 + CHATBOX_1F1); /* read for its side effects only; result discarded */
        R0 = 1;
    }

    ST8(R5 + CHATBOX_NEXT_STATE, R0);

    /* The original pushed r0/r1 and popped them on return. */
    R0 = savedR0;
    R1 = savedR1;
    RETURN_TO_CALLER();
}

#undef CHATBOX_NEXT_STATE
#undef CHATBOX_1F1
#undef CHATBOX_1F2

/* ── chatbox_EC_label ── */
/* chatbox_EC_label: chatbox script opcode 0xEC handler.
 * r4 points at the current script command (3 bytes: opcode, sub-opcode,
 * operand), r5 at the chatbox object. The handler adds the command's step
 * to the chatbox's Unk_70 counter and four times that to its Unk_7C
 * counter, then skips to the next command and returns 1 (r0 = 1) so the
 * interpreter keeps running. If the sub-opcode byte at [r4+1] is 1, the
 * operand at [r4+2] is the step itself; otherwise it is an index into an
 * 8-byte-per-entry table, making the step operand * 8.
 * On return r1 = updated Unk_7C, r2 = updated Unk_70; the flags are those
 * of the pointer bump (c/v) with n/z from the returned 1. */
Next chatbox_EC_label(void) {
    u32 operand, advance, newCursor, newCounter;

    operand = LD8(R4 + 2);
    if (LD8(R4 + 1) == 1)
        advance = operand;       /* sub-opcode 1: operand is the raw step */
    else
        advance = operand * 8;   /* otherwise: index into 8-byte-wide table */

    newCursor = LD32(R5 + oChatbox_Unk_70) + advance;
    ST32(R5 + oChatbox_Unk_70, newCursor);

    newCounter = LD32(R5 + oChatbox_Unk_7C) + advance * 4;
    ST32(R5 + oChatbox_Unk_7C, newCounter);

    R2 = newCursor;
    R1 = newCounter;

    R4 += 3;                     /* skip past the 3-byte command */
    SET_FLAGS_ADD(R4, 3);

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── chatbox_F1_textspeed ── */
/* chatbox_F1_textspeed(r4 = text script command pointer, r5 = chatbox):
 * Handles a "set text print speed" command in the chatbox text script: the
 * speed parameter (byte at r4+2) is copied into the chatbox's current print
 * speed, and speed+1 is stored at chatbox+0x1F3 (the speed to use afterwards).
 * The chatbox 0x3E flag word records whether the requested speed was 0
 * (0x0040) or not (0x0800).
 * Returns with r0 = 1, r1 = speed+1, r2 = 0x1F3, r4 advanced past the
 * 3-byte command; c/v reflect the r4 + 3 addition, nz is set by the final
 * MOV r0, #1 (Thumb format-3 MOV sets N/Z). */
Next chatbox_F1_textspeed(void) {
    u32 speed = LD8(R4 + 2);

    ST8(R5 + 8, speed); /* oChatbox_TextScriptPrintSpeed */
    /* speed == 0 stores 1 here too, so a single store covers both paths */
    ST8(R5 + 0x1F3, speed + 1); /* chatbox+0x1F3: next print speed */
    R1 = speed + 1;
    R2 = 0x1F3;

    if (speed == 0) {
        R0 = FLAGS_3E_UNK_0040;
        GUEST_CALL(chatbox_setflags_3e); /* (int mask) -> void */
    } else {
        R0 = 0x800; /* FLAGS_3E_UNK_0800 */
        GUEST_CALL(chatbox_setflags_3e); /* (int mask) -> void */
    }

    u32 prevCursor = R4;
    R4 = prevCursor + 3;
    SET_FLAGS_ADD(prevCursor, 3);

    R0 = 1;
    SET_FLAGS_NZ(R0); /* Thumb MOV rd, #imm sets N/Z, leaves C/V */
    RETURN_TO_CALLER();
}

/* ── chatbox_F7_move ── */
/* chatbox_F7_move: 08042830 THUMB.
 * Tail-dispatch for the chatbox state machine: r4 points at the chatbox
 * object and the byte at +1 is its state, which selects one of ten update
 * handlers. The handler runs with R1 = state*4 (and R0 = its own entry
 * address, as the original's computed `mov lr, pc / bx r0` call leaves it)
 * and returns straight to our caller, so its register and flag results are
 * ours. States are always 0-9 in practice; the raw table walk of the
 * original is represented here as a direct switch. */
Next chatbox_F7_move(void) {
    u32 state = LD8(R4 + 1);

    R1 = state << 2;
    switch (state) {
    case 0: R0 = 0x0804286C; GUEST_CALL(sub_804286C); break;
    case 1: R0 = 0x080428B8; GUEST_CALL(sub_80428B8); break;
    case 2: R0 = 0x080428C0; GUEST_CALL(sub_80428C0); break;
    case 3: R0 = 0x080428E4; GUEST_CALL(sub_80428E4); break;
    case 4: R0 = 0x080428EC; GUEST_CALL(sub_80428EC); break;
    case 5: R0 = 0x080428FC; GUEST_CALL(sub_80428FC); break;
    case 6: R0 = 0x08042910; GUEST_CALL(sub_8042910); break;
    case 7: R0 = 0x0804293C; GUEST_CALL(sub_804293C); break;
    case 8: R0 = 0x08042990; GUEST_CALL(sub_8042990); break;
    case 9: R0 = 0x080429D8; GUEST_CALL(sub_80429D8); break;
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_uncompMapTextArchives_803FD08 ── */
/* chatbox_uncompMapTextArchives_803FD08: decompress the three chatbox text
 * archives for the current map into their fixed RAM buffers. Returns 0. */
Next chatbox_uncompMapTextArchives_803FD08(void) {
    /* Map-specific compressed text archive -> eDecompressedTextArchive202DA00. */
    R0 = 0;
    GUEST_CALL(chatbox_selectCompTextByMap_8040730);
    R1 = A_eDecompressedTextArchive202DA00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* Map-specific compressed text archive -> DecompressionBuf2033400. */
    GUEST_CALL(chatbox_selectCompTextByMap_8040794);
    R1 = A_DecompressionBuf2033400;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* Map-specific compressed text script archive -> eDecompressedTextArchive202FA00. */
    GUEST_CALL(chatbox_selectCompTextByMap_80407C8);
    R1 = A_eDecompressedTextArchive202FA00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    R0 = 0;
    RETURN_TO_CALLER();
}
