/*
 * chatbox.c: readable functions of bn6f chatbox.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

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

/* ── chatbox_uncompMapTextArchives_803FD3C ── */
/* chatbox_uncompMapTextArchives_803FD3C:
 * Decompress the map-area text archive. Loads compressed text entry #1 via
 * chatbox_selectCompTextByMap_8040730 and LZ77-expands it into the shared
 * decompression buffer at eDecompressionBuf2034A00. */
Next chatbox_uncompMapTextArchives_803FD3C(void) {
    /* Select the compressed map text (index 1) ... */
    R0 = 1;
    GUEST_CALL(chatbox_selectCompTextByMap_8040730);

    /* ... and expand it into the decompression buffer. */
    R1 = A_eDecompressionBuf2034A00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── chatbox_uncompMapTextArchives_803FD54 ── */
/* chatbox_uncompMapTextArchives_803FD54:
 * Decompress the compressed text archive selected for the current map
 * (via chatbox_selectCompTextByMap_8040794) into the shared LZ77
 * decompression buffer at DecompressionBuf2033400. Always returns 0. */
Next chatbox_uncompMapTextArchives_803FD54(void) {
    /* R0 = pointer to the LZ77-compressed text archive for this map */
    GUEST_CALL(chatbox_selectCompTextByMap_8040794);

    /* R1 = destination buffer for the decompressed text */
    R1 = A_DecompressionBuf2033400;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_202da04 ── */
/* chatbox_runScript_202da04(r0 = script id):
 * Runs script `r0` from the fixed text archive eTextScript202DA04 by
 * forwarding to chatbox_runScript(archive, scriptID). */
Next chatbox_runScript_202da04(void) {
    R1 = R0;
    R0 = A_eTextScript202DA04;
    GUEST_CALL(chatbox_runScript);
    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_803FD9C ── */
/* chatbox_runScript_803FD9C(r0 = TextScriptArchive *, r1 = script index):
 *
 * Alternate entry point of the chatbox script runner. The full routine lives
 * at chatbox_runScript_803FDA4, whose shared body begins at loc_803FDA8; the
 * two entry points differ only in the initial value of the resume flag r4
 * (this one clears it, meaning "start the script from the top").
 *
 * This entry pushes {r4,r5,lr} before branching into the shared body, and the
 * body finishes with pop {r4,r5,pc}, so the frame must exist on the guest
 * stack: the body reads r5 and the return address from it and restores SP.
 */
Next chatbox_runScript_803FD9C(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_lr = LR;

    /* push {r4,r5,lr}: r4 at lowest address, lr at highest */
    u32 frame = SP - 12;
    SP = frame;
    ST32(frame, saved_r4);
    ST32(frame + 4, saved_r5);
    ST32(frame + 8, saved_lr);

    /* mov r4, #FALSE: no interrupted script to resume */
    R4 = 0;

    /* Tail-branch into the shared body of the script runner; it pops
     * {r4,r5,pc} on exit, returning straight to our caller. */
    GUEST_CALL(loc_803FDA8);

    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_803FE08 ── */
/* chatbox_runScript_803FE08(r0 = TextScriptArchive *archive, r1 = scriptIdx):
 * Entry point for the chatbox script runner. It sets up the runner's state
 * flag (r4 = FALSE, i.e. "script not finished") and tail-branches into the
 * shared script-running body at loc_803FE14, which produces the outputs
 * (r5, r6, r7). Everything else — r0, r1, r5, r6, r7, r10 — is passed
 * straight through to the body.
 *
 * The original pushes {r4,r5,lr} before the branch and never pops it: the
 * tail-called body's epilogue consumes this frame, so the slots must really
 * be written to the guest stack (r4 at SP-12, r5 at SP-8, lr at SP-4). */
Next chatbox_runScript_803FE08(void) {
    u32 frame = SP -= 12;

    /* push {r4,r5,lr} */
    ST32(frame, R4);
    ST32(frame + 4, R5);
    ST32(frame + 8, LR);

    /* mov r4, #FALSE — start the script from its beginning. */
    R4 = 0;

    /* b loc_803FE14 — tail-call into the script runner proper; its
     * results in r5/r6/r7 are this function's results. */
    GUEST_CALL(loc_803FE14);
    RETURN_TO_CALLER();
}

/* ── chatbox_checkAdvanceTextPrint_8040154 ── */
/* chatbox_checkAdvanceTextPrint_8040154(r5 = chatbox):
 * Called each frame while the chatbox is printing text. If the player is
 * holding B or has just pressed A (or the text has no per-character print
 * delay), reset the chatbox's state-machine step to 0 so it moves on to the
 * next phase; otherwise leave it alone.
 * Returns r2 = 2 if B was held, r2 = 1 otherwise. */
Next chatbox_checkAdvanceTextPrint_8040154(void) {
    if (LD16(R5 + oChatbox_JoypadHeld) & 2) {   /* B currently held */
        R2 = 2;
        ST8(R5 + oChatbox_JumpTableOffset_11, 0);
        RETURN_TO_CALLER();
    }

    R2 = 1;
    /* A newly pressed, or the print speed is 0 (text completes instantly) */
    if ((LD16(R5 + oChatbox_JoypadUp) & 1) != 0 || LD8(R5 + 8) == 0) {
        ST8(R5 + oChatbox_JumpTableOffset_11, 0);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_CopyBackgroundTiles_8040344 ── */
/* chatbox_CopyBackgroundTiles_8040344 (r5 = chatbox):
 * Redraws the chatbox background: pick the tile-id array matching the
 * chatbox's current background graphic and open state, then let
 * CopyBackgroundTiles blit it at the chatbox's current position/size.
 * Preserves r5; r1-r4 end up as CopyBackgroundTiles left them. */
Next chatbox_CopyBackgroundTiles_8040344(void) {
    u32 self = R5;

    /* Match the original frame exactly: while the callee runs, [SP] holds
     * the chatbox pointer and [SP + 4] the return address, so the callee
     * sees the same stack arguments as in the original. */
    SP -= 8;
    ST32(SP, self);
    ST32(SP + 4, LR);

    /* tileIds = spritePtrArr8045CEC[bgId * 8 + openState]
     * (8 pointer slots per background id, selected by the open state) */
    u32 bgId = LD8(self + 0x1D2);                       /* current background tileset id */
    u32 openState = LD8(self + oChatbox_OpenState_10);  /* which open-state variant */
    R3 = LD32(A_spritePtrArr8045CEC + (bgId << 5) + (openState << 2));

    R0 = LD8(self + oChatbox_CurTileXBlockPos); /* j: horizontal start position */
    R1 = LD8(self + oChatbox_CurTileYBlockPos); /* i: vertical start position */
    R2 = 0;                                     /* use 32x32 tile block #0 */
    R4 = LD8(self + oChatbox_CurTileWidth16);   /* j size in tiles */
    R5 = LD8(self + oChatbox_CurTileHeight16);  /* i size in tiles */

    GUEST_CALL(CopyBackgroundTiles);

    R5 = LD32(SP); /* pop {r5} - restore the chatbox pointer for our caller */
    SP += 8;       /* discard the saved-LR slot (pop {pc} is RETURN_TO_CALLER) */
    RETURN_TO_CALLER();
}

/* ── chatbox_runTrainScript ── */
/* chatbox_runTrainScript(r0 = script index):
 * Run one of the train-ride text scripts (eTextScript2034A04) by index via
 * the generic script runner. All other registers pass through to it. */
Next chatbox_runTrainScript(void) {
    u32 script_idx = R0;

    R1 = script_idx;
    R0 = A_eTextScript2034A04;
    GUEST_CALL(chatbox_runScript);

    RETURN_TO_CALLER();
}

/* ── chatbox_advanceNextState_8040C9C ── */
/* Chatbox state offsets within the chatbox work area (r5). */
#define CHATBOX_NEXT_STATE  0x1F0  /* mode/state byte this function updates */
#define CHATBOX_1F1         0x1F1  /* read by the original, result unused */
#define CHATBOX_1F2         0x1F2  /* cleared when restarting from a mid state */

/* chatbox_advanceNextState_8040C9C(r5 = chatbox work area):
 * Advance the chatbox state byte at +0x1F0. State 0 stays 0, state 3 stays 3,
 * any other state clears +0x1F2 (touching +0x1F1 as the original does) and
 * moves to state 1.
 * r0/r1 are preserved; c/v are left as the original's last CMP leaves them
 * (CMP #0 when the state was 0, CMP #3 otherwise). */
Next chatbox_advanceNextState_8040C9C(void) {
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

/* ── chatbox_closeAndReset_80410F8 ── */
/* chatbox_closeAndReset_80410F8(r5 = chatbox): clear the chatbox's text flags (mask 0x101:
 * "window open" + "text finished"), reset its open-state to 3 (fully closed),
 * and return 1. r1 and the flags' C/V are left untouched; Z/N reflect r0 = 1. */
Next chatbox_closeAndReset_80410F8(void) {
    R0 = 0x101;
    GUEST_CALL(chatbox_clearFlags_3e);
    ST8(R5 + oChatbox_OpenState_10, 3u);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_E9_newline ── */
/* chatbox_E9_newline(r4 = script/cursor position, r5 = chatbox object):
 * Step the text-box graphics loader forward after printing a newline.
 * Increments the box's gfx load state and clears its per-line scratch
 * field. Once the state reaches 3 the box graphics are fully loaded and
 * a "ready" flag at +0x8C is set; otherwise the pointer for the new
 * state, taken from a table at byte_803FCE4, is stored at +0x7C.
 * Always increments r4 and returns 0 in r0. */
Next chatbox_E9_newline(void) {
    u32 state = (u32)LD8(R5 + oChatbox_BoxGfxLoadState_0F) + 1;

    ST8(R5 + oChatbox_BoxGfxLoadState_0F, state);
    ST32(R5 + oChatbox_Unk_70, 0);

    if (state == 3) {
        /* Box graphics fully loaded: raise the ready flag. */
        R0 = 1;
        R2 = 0x8C;
        ST32(R5 + 0x8C, R0);
    } else {
        /* Still loading: fetch the entry for the new state. */
        R2 = state - 1;
        R3 = R2 * 4;
        R0 = LD32(A_byte_803FCE4 + R3);
        ST32(R5 + oChatbox_Unk_7C, R0);
    }

    SET_FLAGS_ADD(R4, 1);
    R4 = R4 + 1;

    R0 = 0;
    SET_FLAGS_NZ(0);

    RETURN_TO_CALLER();
}

/* ── chatbox_setEventFlagAndAdvanceEventCursor_80411B0 ── */
/* chatbox_setEventFlagAndAdvanceEventCursor_80411B0(r0 = event flag id, r4 = cursor into an event list):
 * set the event flag passed in r0, then advance the r4 cursor by one
 * 4-byte entry. Flags are left as the trailing `add r4, #4` leaves them. */
Next chatbox_setEventFlagAndAdvanceEventCursor_80411B0(void) {
    GUEST_CALL(SetEventFlag);
    SET_FLAGS_ADD(R4, 4);
    R4 += 4;
    RETURN_TO_CALLER();
}

/* ── chatbox_ClearEntryWaitFlagAndAdvanceEntry_80411BC ── */
/* chatbox_ClearEntryWaitFlagAndAdvanceEntry_80411BC(r0 = event flag bitfield, r4 = chatbox state pointer):
 * Clears the text-advance event flag for the current chatbox entry, then
 * advances the entry pointer by one slot (4 bytes). Flags come from the
 * pointer increment, matching the original's ADDS. */
Next chatbox_ClearEntryWaitFlagAndAdvanceEntry_80411BC(void) {
    GUEST_CALL(ClearEventFlag);

    R4 += 4;
    SET_FLAGS_ADD(R4, 4);

    RETURN_TO_CALLER();
}

/* ── chatbox_cmdSetEventFlag_8041218 ── */
/* chatbox_cmdSetEventFlag_8041218(r4 = pointer to current chatbox command record,
 *                 r5 = chatbox state):
 * Executes the "set event flag" command: byte 2 of the 3-byte record selects
 * one of the 32-bit flag words in the flag table at r5+0x4C, and that word
 * is passed to SetEventFlag. Afterwards r4 is advanced by 3 to point at the
 * next command record.
 * Returns with r0/r1/r3 (and the flags) as left by SetEventFlag, and flags
 * also updated by the `adds r4, #3`. */
Next chatbox_cmdSetEventFlag_8041218(void) {
    /* flag word index = record byte 2; each entry is 4 bytes, table base 0x4C */
    u32 flagWord = LD32(R5 + ((LD8(R4 + 2) << 2) + 0x4C));

    R0 = flagWord;
    GUEST_CALL(SetEventFlag);

    /* advance to the next command record; ADDS sets the outgoing NZCV */
    u32 nextRecord = R4 + 3;
    SET_FLAGS_ADD(R4, 3);
    R4 = nextRecord;

    RETURN_TO_CALLER();
}

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

/* ── chatbox_cmdRunScriptByGameProgress_8041748 ── */
/* chatbox_cmdRunScriptByGameProgress_8041748: chatbox script command "run script depending on game progress".
 *
 * r4 points at a 6-byte command laid out as:
 *   [2] = lowest game-progress value that satisfies the condition
 *   [3] = highest game-progress value that satisfies the condition
 *   [4] = sub-script ID to run when the condition holds   (0xFF = none)
 *   [5] = sub-script ID to run when the condition fails   (0xFF = none)
 *
 * The current game progress is read from the game state, the matching
 * sub-script (if any) is started through chatbox_config_80408D0, and r4 is
 * advanced past the command. Always returns r0 = 1. */
Next chatbox_cmdRunScriptByGameProgress_8041748(void) {
    u32 minProgress = LD8(R4 + 2);
    u32 maxProgress = LD8(R4 + 3);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 progress  = LD8(gameState + 6 /* oGameState_GameProgress */);

    /* Pick which of the two trailing script IDs applies. */
    u32 idOffset;
    if ((s32)progress < (s32)minProgress || (s32)progress > (s32)maxProgress)
        idOffset = 5;
    else
        idOffset = 4;

    u32 scriptID = LD8(R4 + idOffset);

    R0 = idOffset;      /* first parameter of the callee is unused */
    R1 = scriptID;
    R2 = maxProgress;
    R3 = progress;

    if (scriptID != 0xFF) {
        /* chatbox_config_80408D0(notUsed, scriptID): start the sub-script. */
        GUEST_CALL(chatbox_config_80408D0);
    } else {
        /* No sub-script for this branch: skip the whole 6-byte command. */
        u32 next = R4 + 6;
        SET_FLAGS_ADD(R4, 6);
        R4 = next;
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_stepScriptEntry_80417A4 ── */
/* chatbox_stepScriptEntry_80417A4(r4 = pointer to the current chatbox script entry):
 * Peek at the byte at offset 3 of the current script entry.  If it is the
 * 0xFF terminator, the entry is "go to next line": advance the cursor by one
 * entry (r4 += 4).  Otherwise hand the byte to the chatbox script interpreter
 * as a script ID (the first argument is always the constant 3; the original
 * also sets r0 = 2 first, but that store is dead).
 * Always returns r0 = 1.  On the terminator path r1 still holds the byte read
 * and r4 is the advanced cursor; on the interpreter path r1/r4 are whatever
 * the callee leaves.  Flags end up as after "mov r0, #1" (nz = 1 != 0),
 * except that on the terminator path c/v still come from the r4 += 4 add. */
Next chatbox_stepScriptEntry_80417A4(void) {
    R0 = 3;                              /* fixed notUsed argument */
    R1 = LD8(R4 + 3);                    /* script ID or end-of-entry marker */

    if (R1 != 0xFF) {
        GUEST_CALL(chatbox_config_80408D0);
    } else {
        u32 nextEntry = R4 + 4;
        SET_FLAGS_ADD(R4, 4);            /* carry/overflow of the cursor bump */
        R4 = nextEntry;
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_startCurNaviScript_80419B4 ── */
/* chatbox_startCurNaviScript_80419B4(r4 = chatbox work area, r2/r5/r10 = caller context):
 * Start the chatbox script belonging to the currently equipped PET navi.
 * Reads the navi's script ID from r4[navi + 2]; 0xFF means "no script for
 * this navi", in which case r4 is just advanced past the table by 0xE.
 * Otherwise hands the script ID to the chatbox config loader (which ignores
 * its first argument; r0 still holds the navi number from GetCurPETNavi).
 * Always returns r0 = 1. */
Next chatbox_startCurNaviScript_80419B4(void) {
    GUEST_CALL(GetCurPETNavi);      /* R0 = current PET navi number */

    R1 = LD8(R4 + R0 + 2);          /* script ID slot for this navi */
    if (R1 != 0xFF) {
        GUEST_CALL(chatbox_config_80408D0);
        R0 = 1;
        SET_FLAGS_NZ(R0);           /* movs r0,#1: nz only; c/v left as the callee set them */
    } else {
        SET_FLAGS_ADD(R4, 0xE);     /* adds r4,#0xE: sets c/v (nz overwritten below) */
        R4 += 0xE;
        R0 = 1;
        SET_FLAGS_NZ(R0);           /* movs r0,#1 */
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_processScriptEntry_80419D0 ── */
/* chatbox_processScriptEntry_80419D0: process one chatbox script entry.
 * r4 points at the current entry. Its state byte ([r4+2]) is passed to
 * sub_8048C24, which returns a value deciding which of the two trailing
 * bytes holds the script ID (offset 3 when nonzero, offset 4 when zero).
 * - If that ID is 0xFF (end-of-script marker), advance r4 to the next
 *   entry (r4 += 5) and return.
 * - Otherwise hand the ID to chatbox_config_80408D0 and return.
 * Always returns r0 = 1. r5/r6/r7 are preserved. */
Next chatbox_processScriptEntry_80419D0(void) {
    u32 entry = R4;

    /* Ask the chatbox state handler which script-ID byte is current. */
    R0 = LD8(entry + 2);
    GUEST_CALL(sub_8048C24);
    u32 state = R0;

    u32 idOffset = (state == 0) ? 4 : 3;
    u32 scriptID = LD8(entry + idOffset);

    /* The ID is loaded into r1 on both paths (the original's ldrb happens
     * before the 0xFF test), so set it before branching. */
    R1 = scriptID;

    if (scriptID == 0xFF) {
        /* End marker: skip past this entry to the next one. */
        R4 = entry + 5;
        SET_FLAGS_ADD(entry, 5);
    } else {
        /* Configure the chatbox with this entry's script ID.
         * r0 (the state-check result) is an unused parameter here. */
        GUEST_CALL(chatbox_config_80408D0);
    }

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

/* ── chatbox_F3_control ── */
/* chatbox_F3_control(r4 = pointer to the current F3 chatbox opcode):
 * Executes one F3 (chatbox show/hide) opcode from the text script.
 * Byte [r4+1] is the opcode:
 *   0 -> show the chatbox at full size (64 units) and skip 2 bytes
 *   1 -> hide the chatbox and skip 2 bytes
 *   2 or any other value -> show the chatbox with the length taken from
 *       byte [r4+2] and skip 3 bytes
 * Always returns 1 in r0. r1 and r2 are preserved; the C/V flags reflect
 * the pointer bump (ADDS), N/Z are clear from the final MOV r0,#1. */
static void chatbox_F3_advance(u32 step) {
    u32 old = R4;
    R4 = old + step;
    SET_FLAGS_ADD(old, step);
}

Next chatbox_F3_control(void) {
    u32 op = LD8(R4 + 1);

    if (op == 0) {
        /* show chatbox at full size */
        R0 = 0x40;
        GUEST_CALL(chatbox_set_eFlags2009F38);
        chatbox_F3_advance(2);
    } else if (op == 1) {
        /* hide chatbox */
        R0 = 0x40;
        GUEST_CALL(chatbox_clear_eFlags2009F38);
        chatbox_F3_advance(2);
    } else {
        /* show chatbox with an explicit per-opcode size */
        R0 = LD8(R4 + 2);
        GUEST_CALL(chatbox_set_eFlags2009F38);
        chatbox_F3_advance(3);
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_cmd_playerFixedAnim_8041EE8 ── */
/* owPlayer_RequestFixedAnimation_809E14C is not declared in the shared headers, so forward-declare it
 * here; it is a normal `Next ... (void)` game function. */
Next owPlayer_RequestFixedAnimation_809E14C(void);

/* chatbox_cmd_playerFixedAnim_8041EE8 — chatbox script command handler.
 *
 * Inputs:  r4 = pointer into the chatbox script (the command's operand
 *              byte sits at [r4 + 2]),
 *          r5 = chatbox work area,
 *          r10 = toolkit.
 *
 * What it does:
 *   1. sets bit 0 of the chatbox 0x3E flag word (the original loads the
 *      immediate 1 directly; no named constant exists for this bit),
 *   2. copies byte 0x14 of the overworld player object into the chatbox
 *      work area at offset 0x3C,
 *   3. passes the command's operand byte (and the player object in r7) to
 *      owPlayer_RequestFixedAnimation_809E14C, whose result comes back in r1,
 *   4. advances the script pointer past the 3-byte command.
 *
 * Outputs: r0 = 0, r1 = owPlayer_RequestFixedAnimation_809E14C's result, r2 = copied player byte,
 *          r4 = advanced script pointer, r7 = player object pointer.
 *          Flags: N/Z from "mov r0,#0" (Z set, N clear), C/V left over
 *          from the ADDS on the script pointer. r5, r6 and lr preserved.
 */

Next chatbox_cmd_playerFixedAnim_8041EE8(void) {
    /* mark the flag bit for this command in the chatbox state */
    R0 = 1; /* FLAGS_3E bit 0 */
    GUEST_CALL(chatbox_setflags_3e);

    /* look up the overworld player object and copy its byte 0x14
     * into the chatbox work area */
    R7 = LD32(LD32(R10 + oToolkit_GameStatePtr) + oGameState_OverworldPlayerObjectPtr);
    R2 = LD8(R7 + 0x14);
    ST8(R5 + 0x3C, R2);

    /* operand byte of the current script command. (The original's
     * "cmp r0,#7 / bgt next-line" here is a no-op — both paths fall
     * through, and the call below overwrites the flags anyway.) */
    R0 = LD8(R4 + 2);

    GUEST_CALL(owPlayer_RequestFixedAnimation_809E14C);

    /* advance to the next 3-byte command. The increment's carry/overflow
     * must survive, and mov r0,#0 afterwards forces N=0/Z=1 on top of
     * them without touching C/V. */
    u32 prevPos = R4;
    R4 = prevPos + 3;
    SET_FLAGS_ADD(prevPos, 3);
    R0 = 0;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── chatbox_scriptWaitPlayerFrameStep_8041F1C ── */
/* chatbox_scriptWaitPlayerFrameStep_8041F1C: chatbox tick handler.
 *
 * Sets chatbox flag bit 1 (via chatbox_setflags_3e), then looks up the
 * overworld player object through the toolkit's game state and asks for
 * its sprite frame parameters. Bit 0x80 of those parameters signals that
 * the current frame-sequence step has finished; while it is clear the
 * caller keeps waiting.
 * r4 is the text-script cursor: it is advanced by 2 once the bit is set.
 *
 * Returns r0 = 1 when the script cursor should advance, 0 otherwise.
 * NZ is set from r0 on both paths; on the advancing path C/V come from
 * the r4 += 2 addition. r1 is left holding frameParams & 0x80; r5 is
 * preserved. */
Next chatbox_scriptWaitPlayerFrameStep_8041F1C(void) {
    R0 = 1; /* FLAGS_3E bit 1 */
    GUEST_CALL(chatbox_setflags_3e);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    u32 savedR5 = R5;
    R5 = LD32(gameState + oGameState_OverworldPlayerObjectPtr);
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;
    R5 = savedR5;

    R1 = frameParams & 0x80;

    if (R1 != 0) {
        /* frame step finished: consume the next script word */
        SET_FLAGS_ADD(R4, 2);
        R4 = R4 + 2;
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        /* still animating: stay on the current script word */
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_scriptStepSyncPlayerTextState_8041F44 ── */
/* chatbox_scriptStepSyncPlayerTextState_8041F44: chatbox script step.
 *
 * Sets chatbox flag bit 0x0001, then reads the counter at [r5+0x3C]
 * (r5 = chatbox state). While that counter is still 7 or less it is copied
 * into the overworld player object's field 0x10 (which holds the current
 * text state). Finally calls owPlayer_RequestFixedAnimation_809E14C, advances the script cursor (r4)
 * by 2 and returns 0 with N/Z cleared; carry/overflow are left as the
 * "adds r4, #2" before it left them.
 *
 * Inputs:  r4 = script cursor, r5 = chatbox state ptr, r10 = toolkit.
 * Outputs: r0 = 0, r1 = whatever owPlayer_RequestFixedAnimation_809E14C leaves, r4 advanced by 2,
 *          r7 = player object (unchanged if the counter was > 7),
 *          nz/c/v as described above.
 * Preserved: r5, r6.
 */
Next chatbox_scriptStepSyncPlayerTextState_8041F44(void) {
    R0 = 1; /* FLAGS_3E_UNK_0001 */
    GUEST_CALL(chatbox_setflags_3e);

    R1 = R5 + 0x3C;
    u32 counter = LD8(R1);
    if ((s32)counter <= 7) {
        u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
        R7 = LD32(gameState + oGameState_OverworldPlayerObjectPtr);
        ST8(R7 + 0x10, counter);
    }

    /* owPlayer_RequestFixedAnimation_809E14C still sees the counter in r0 and the chatbox field
     * address in r1, mirroring the register state at this point. */
    R0 = counter;
    GUEST_CALL(owPlayer_RequestFixedAnimation_809E14C);

    u32 newCursor = R4 + 2;
    SET_FLAGS_ADD(R4, 2); /* carry/overflow come from this add */
    R4 = newCursor;

    R0 = 0;
    SET_FLAGS_NZ(R0);     /* ...but N/Z come from "mov r0, #0" */
    RETURN_TO_CALLER();
}

/* ── chatbox_PlayScriptSoundCue_80422D4 ── */
/* chatbox_PlayScriptSoundCue_80422D4: play the sound cue at the current chatbox script position.
 * r4 points at a 4-byte cue record; bytes 2-3 hold the 16-bit sound effect
 * id (little-endian). Plays it, steps r4 past the record and returns r0 = 1.
 * Flags on return: NZ from "r0 = 1", C/V from the r4 += 4 add. */
Next chatbox_PlayScriptSoundCue_80422D4(void) {
    /* The original does push {lr} ... pop {pc}; the saved value is LR itself.
     * Reproduce the push so that SP during the PlaySoundEffect call matches
     * the original (the callee's stack activity must land at the same
     * addresses), then pop before returning. */
    SP -= 4;
    ST32(SP, LR);

    u32 soundId = (u32)LD8(R4 + 2) | ((u32)LD8(R4 + 3) << 8);

    R0 = soundId;
    /* The original's byte-merge leaves the shifted high byte in r1 at the
     * call site; hand PlaySoundEffect the same register contents. */
    R1 = (u32)LD8(R4 + 3) << 8;
    GUEST_CALL(PlaySoundEffect);

    /* Advance to the next cue record (the ADD sets C/V for the caller). */
    SET_FLAGS_ADD(R4, 4);
    R4 += 4;

    /* Report success; MOV r0, #1 refreshes N/Z while C/V stay from the add. */
    R0 = 1;
    SET_FLAGS_NZ(R0);

    SP += 4;
    RETURN_TO_CALLER();
}

/* ── chatbox_cmdPlayMusic_80422E8 ── */
/* chatbox_cmdPlayMusic_80422E8(r4 = chatbox script pointer):
 * Play the music track given as a little-endian u16 in the current chatbox
 * script command, then advance the script pointer past the command.
 * Leaves r4 pointing at the next command and returns r0 = 1.
 * Flags on exit: n/z from "movs r0, #1", c/v from "adds r4, #4". */
Next chatbox_cmdPlayMusic_80422E8(void) {
    u32 songLow  = LD8(R4 + 2);
    u32 songHigh = LD8(R4 + 3);

    /* Match the original's register state at the call: r1 holds the
     * shifted high byte, r0 the full halfword. */
    R1 = songHigh << 8;
    R0 = songLow | R1;
    GUEST_CALL(PlayMusic);      /* PlayMusic(song) */

    {
        u32 old = R4;
        R4 = old + 4;
        SET_FLAGS_ADD(old, 4);  /* c/v from "adds r4, #4" */
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);           /* n/z from "movs r0, #1" */

    RETURN_TO_CALLER();
}

/* ── chatbox_scriptOpClearFlag40_80422FC ── */
/* chatbox_scriptOpClearFlag40_80422FC: chatbox script opcode handler.
 * Clears chatbox status flag 0x40 (via chatbox_clearFlags_3e), advances the
 * script pointer in r4 past this opcode (2 bytes), and returns r0 = 1 so the
 * chatbox keeps running the script.
 * Flags left for the caller: N/Z from the final `mov r0, #1`, C/V from the
 * pointer increment. */
Next chatbox_scriptOpClearFlag40_80422FC(void) {
    /* clear flag FLAGS_3E_UNK_0040 in the chatbox status field */
    R0 = FLAGS_3E_UNK_0040;
    GUEST_CALL(chatbox_clearFlags_3e);

    /* step the script pointer over this opcode */
    u32 prevPtr = R4;
    R4 = prevPtr + 2;
    SET_FLAGS_ADD(prevPtr, 2);

    /* signal the chatbox to continue */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_stateSetFlag40Advance_804230C ── */
/* chatbox_stateSetFlag40Advance_804230C: step the chatbox forward.
 * Sets chatbox flag 0x40 (via chatbox_setflags_3e), advances the caller's
 * chatbox state pointer (r4) by 2 bytes, and returns 1.
 * Flags on return: N/Z from the result value 1, C/V from the r4 += 2 add.
 * Preserves r1 (chatbox_setflags_3e is a leaf and keeps r1/r4 intact). */
Next chatbox_stateSetFlag40Advance_804230C(void) {
    R0 = FLAGS_3E_UNK_0040;
    GUEST_CALL(chatbox_setflags_3e);

    SET_FLAGS_ADD(R4, 2);   /* ADDS r4, #2 */
    R4 += 2;

    R0 = 1;
    SET_FLAGS_NZ(1);        /* MOVS r0, #1 sets N/Z, leaves C/V */

    RETURN_TO_CALLER();
}

/* ── chatbox_scriptCmdSetFlags3e80_8042350 ── */
/* chatbox_scriptCmdSetFlags3e80_8042350: set chatbox flag bit FLAGS_3E_UNK_0080, then advance the
 * caller's chatbox cursor (r4) by 2 and return 1.  r1 is preserved.
 * Flags: N/Z come from the final "movs r0, #1"; C/V are carried over from
 * the "adds r4, #2" that precedes it. */
Next chatbox_scriptCmdSetFlags3e80_8042350(void) {
    u32 old_r4 = R4; /* r4 is live across the setflags call */

    R0 = FLAGS_3E_UNK_0080;
    GUEST_CALL(chatbox_setflags_3e); /* (int mask) -> void */

    R4 = old_r4 + 2;
    SET_FLAGS_ADD(old_r4, 2);

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER(); /* pop {pc}: return to the saved LR, unchanged */
}

/* ── chatbox_textScriptAction_openShop_80424BC ── */
/* chatbox_textScriptAction_openShop_80424BC: chatbox text-script action used at shop counters.
 * Sets chatbox flag 0x3E bit 1 (the "close chatbox" mask, FLAGS_3E_UNK_0001
 * in the disassembly), and if this script step hasn't fired yet, launches
 * the shop whose id is stored at [r4+2] and marks the step as taken by
 * storing 1 into the text-script state byte at [r5]+4. Returns 0 in r0.
 *
 * Flag note: the final `mov r0, #0` only rewrites N and Z; C and V reach
 * the caller from the `cmp r0, #0` (C=1, V=0) on the skip path, or from
 * subsystem_launchShop on the launch path. */
Next chatbox_textScriptAction_openShop_80424BC(void) {
    R0 = 1;                    /* FLAGS_3E_UNK_0001: close-chatbox flag */
    GUEST_CALL(chatbox_setflags_3e);

    u32 state = LD8(R5 + 4);   /* oChatbox_TextScriptState_04 */
    SET_FLAGS_CMP(state, 0);   /* cmp r0, #0: N/Z from value, C=1, V=0 */
    if (state == 0) {
        R0 = LD8(R4 + 2);      /* shop id for subsystem_launchShop */
        GUEST_CALL(subsystem_launchShop);
        ST8(R5 + 4, 1);        /* mark step as done */
    }

    R0 = 0;                    /* mov r0, #0: N=0, Z=1; C/V untouched */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_processScriptEntry_8042678 ── */
/* chatbox_processScriptEntry_8042678: handle one chatbox script entry.
 *
 * r4 points at the chatbox entry to process.  The chatbox core helper
 * (sub_803F798) is consulted first; when it reports 0 a sound effect is
 * played.  Its result is then reused as an offset, and the script ID byte
 * at r4+offset+2 is examined:
 *   - any value other than 0xFF is forwarded to chatbox_config_80408D0
 *     (which starts the corresponding text script),
 *   - 0xFF marks the end of the entry, so r4 is advanced past it (5 bytes).
 * Always returns 1 in R0. */
Next chatbox_processScriptEntry_8042678(void) {
    u32 savedR4 = R4;

    /* Consult the chatbox core; it gets the caller's r0..r3 unchanged.
     * Its result doubles as the offset of the script-ID field. */
    GUEST_CALL(sub_803F798);
    u32 state = R0;
    R4 = savedR4;                 /* pop {r4}: keep the caller's entry pointer */

    if (state == 0) {
        /* Nothing pending in the chatbox core: play the blip. */
        R0 = SOUND_UNK_74;
        GUEST_CALL(PlaySoundEffect);
        R4 = savedR4;             /* pop {r0,r4}: sound call may clobber r4 */
    }

    R1 = LD8(R4 + state + 2);
    if (R1 != 0xFF) {
        /* Hand the script ID to the chatbox configuration routine. */
        R0 = state + 2;
        GUEST_CALL(chatbox_config_80408D0);
        R0 = 1;
        SET_FLAGS_NZ(R0);         /* mov r0, #1 */
    } else {
        /* End marker: skip this 5-byte entry. */
        SET_FLAGS_ADD(R4, 5);     /* add r4, #5 sets carry/overflow */
        R4 = R4 + 5;
        R0 = 1;
        SET_FLAGS_NZ(R0);         /* mov r0, #1 */
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_setFlag0x1000AndAdvanceStep_80426E0 ── */
/* chatbox_setFlag0x1000AndAdvanceStep_80426E0: raise chatbox flag 0x1000 (via chatbox_setflags_3e), then
 * advance the current chatbox state's step counter (r4) by 2. Returns r0 = 1.
 * Final flags: N/Z from the "mov r0,#1", C/V from the "adds r4,#2". */
Next chatbox_setFlag0x1000AndAdvanceStep_80426E0(void) {
    R0 = 0x1000;
    GUEST_CALL(chatbox_setflags_3e);

    {
        u32 prev = R4;
        R4 = prev + 2;
        SET_FLAGS_ADD(prev, 2);
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
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

/* ── chatbox_setCutsceneStateByte_8042FF4 ── */
/* chatbox_setCutsceneStateByte_8042FF4(r4 = chatbox script command pointer, r10 = toolkit):
 * Chatbox script opcode handler. Copies the byte at cmd[3] into the
 * cutscene state's byte array (base + 8) at index cmd[2], then advances
 * the command pointer past this 4-byte opcode.
 * Leaves r0 = 1 (result code for the script interpreter), r1 = the value
 * written, r2 = cutscene state byte-array base, r4 = next command.
 * Flags: nz from "mov r0, #1", c/v from the final "adds r4, #4". */
Next chatbox_setCutsceneStateByte_8042FF4(void) {
    u32 prevR4 = R4;

    R2 = LD32(R10 + oToolkit_CutsceneStatePtr) + 8; /* cutscene state byte array */
    R0 = LD8(R4 + 2); /* index into the byte array */
    R1 = LD8(R4 + 3); /* byte to store */
    ST8(R2 + R0, R1);

    R4 = prevR4 + 4;
    SET_FLAGS_ADD(prevR4, 4);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_setTextParams_8045EFC ── */
/* chatbox_setTextParams_8045EFC(r0..r3 = four chatbox parameters):
 * Store the four arguments into the active chatbox (looked up through the
 * toolkit); they are the text/pointer fields the chatbox task reads back
 * later. All four argument registers and the chatbox pointer are untouched
 * on return. */
Next chatbox_setTextParams_8045EFC(void) {
    u32 chatbox = LD32(R10 + oToolkit_ChatboxPtr);

    ST32(chatbox + oChatbox_Unk_4C, R0);
    ST32(chatbox + oChatbox_Unk_50, R1);
    ST32(chatbox + oChatbox_Unk_54, R2);
    ST32(chatbox + oChatbox_Unk_58, R3);

    RETURN_TO_CALLER();
}

/* ── chatbox_set_eFlags2009F38 ── */
/* chatbox_set_eFlags2009F38(r0 = bits to set): set the given bits in the
 * eFlags2009F38 event-flag word. r1 and r2 are preserved; the nz flags
 * reflect the new flag value. */
Next chatbox_set_eFlags2009F38(void) {
    u32 newFlags = LD32(A_eFlags2009F38) | R0;
    ST32(A_eFlags2009F38, newFlags);
    SET_FLAGS_NZ(newFlags);
    RETURN_TO_CALLER();
}

/* ── chatbox_clear_eFlags2009F38 ── */
/* chatbox_clear_eFlags2009F38(r0 = bit mask): clear the given bits in the
 * chatbox event flags at 0x2009F38. r0 = mask of bits to clear.
 * Leaves r1 and r2 untouched; flags reflect the new flag word. */
Next chatbox_clear_eFlags2009F38(void) {
    u32 mask = R0;
    u32 flags = LD32(A_eFlags2009F38) & ~mask;

    SET_FLAGS_NZ(flags);
    ST32(A_eFlags2009F38, flags);
    RETURN_TO_CALLER();
}

/* ── chatbox_mask_eFlags2009F38 ── */
/* chatbox_mask_eFlags2009F38(r0 = flag bit(s)):
 * Return the given flag bits as they currently stand in the chatbox/
 * cutscene flag word at eFlags2009F38. R1 is left untouched, and the
 * NZ flags reflect the masked value so callers can branch on it. */
Next chatbox_mask_eFlags2009F38(void) {
    R0 &= LD32(A_eFlags2009F38);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_GetState_8045F4C ── */
/* chatbox_GetState_8045F4C: read the low three bits of the event-flags word at eFlags2009F38
 * (the chatbox/text state selector). Returns the masked value in r0 and sets
 * nz on it, like the original's ANDS. r1 is preserved. */
Next chatbox_GetState_8045F4C(void) {
    u32 saved_r1 = R1;
    u32 flags = LD32(A_eFlags2009F38);

    R0 = flags & 7;
    SET_FLAGS_NZ(R0);
    R1 = saved_r1;
    RETURN_TO_CALLER();
}

/* ── chatbox_resetAndClearBuffers_8045F60 ── */
/* chatbox_resetAndClearBuffers_8045F60(r5 = chatbox work area):
 * Reset the chatbox: clear its state fields, then wipe the text buffer at
 * unk_200BEA0 (0xF00 bytes) and the glyph/tile buffer at dword_200CDA0
 * (same 0xF00 size, still in r1 from the first call).
 * Leaves r1 = 0xF00 and the flags from the last ZeroFillByEightWords. */
Next chatbox_resetAndClearBuffers_8045F60(void) {
    ST32(R5 + oChatbox_Unk_70, 0);
    ST8(R5 + oChatbox_BoxGfxLoadState_0F, 0);
    ST8(R5 + oChatbox_Unk_0E, 0);
    ST32(R5 + oChatbox_Unk_7C, 0);

    R0 = A_unk_200BEA0;
    R1 = 0xF00;
    GUEST_CALL(ZeroFillByEightWords);

    R0 = A_dword_200CDA0; /* r1 still holds the 0xF00 length */
    GUEST_CALL(ZeroFillByEightWords);

    RETURN_TO_CALLER();
}
