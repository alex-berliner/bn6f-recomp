/*
 * leaves_farm_1.c: small functions from the model farm, set 1 (start, chatbox, map scripts and 10 more bn6f files).
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── bgScroll_ResetScrollCBCounters_8080D90 ── */
/* bgScroll_ResetScrollCBCounters_8080D90: clear the BG scroll callback counters (8 bytes). */
Next bgScroll_ResetScrollCBCounters_8080D90(void) {
    R0 = A_eBGScrollCBCounters;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);
    RETURN_TO_CALLER();
}

/* ── alwaysTrue_80AA6E8 ── */
/* alwaysTrue_80AA6E8:
 * Trivial helper that always reports "true". Sets R0 = 1 and flags NZ for
 * result 1 (N clear, Z clear), then returns to the caller. */
Next alwaysTrue_80AA6E8(void) {
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── util_nandByteArray40_80AAB88 ── */
/* util_nandByteArray40_80AAB88(r0 = srcA, r1 = srcB, r2 = dst):
 * NAND two 40-byte arrays together: dst[i] = ~(srcA[i] | srcB[i]).
 * Leaves the loop counter (0x28) and the last srcB byte read in r3/r1,
 * with the final CMP's flags (c set, v clear) for callers to branch on. */
Next util_nandByteArray40_80AAB88(void) {
    u32 srcA = R0;      /* r6 in the original */
    u32 srcB = R1;      /* r7 in the original */
    u32 dst = R2;       /* r5 in the original */
    u32 i;              /* r3 in the original */
    u32 lastB = 0;      /* last value loaded into r1 */

    for (i = 0; i < 0x28; i++) {
        u32 a = LD8(srcA + i);
        u32 b = LD8(srcB + i);
        lastB = b;
        ST8(dst + i, ~(a | b));
    }

    R1 = lastB;
    R3 = i;             /* ends at 0x28 */
    SET_FLAGS_CMP(i, 0x28);
    RETURN_TO_CALLER();
}

/* ── SetInterruptCallback ── */
/* SetInterruptCallback(r0 = interrupt index, r1 = handler):
 * Installs `handler` as the callback for interrupt number `interrupt index`.
 * All the real work is done by the crt0 helper _SetInterruptCallback, which
 * reads the index from R0 and the handler pointer from R1 (both already in
 * place from our caller) and stores the pointer into the interrupt dispatch
 * table. The original's `mov lr, pc / bx r2` round trip is just the call
 * linkage to that helper, and its `push {lr} / pop {r0} / bx r0` is the
 * return to the caller, both of which the guest call/return machinery
 * handles here.
 *
 * On exit R2 and R3 hold whatever the helper left in them, R0 is scratched,
 * and R4 passes through untouched. */
Next SetInterruptCallback(void) {
    GUEST_CALL(_SetInterruptCallback);

    RETURN_TO_CALLER();
}

/* ── crt0_start_800027C ── */
/* crt0_start_800027C: start.s trampoline (0800027C).
 *
 * The original code is the standard ARMv4 long-call idiom:
 *   push {lr}              -- remember our caller's return address
 *   ldr r1, =sub_3005E18+1 -- Thumb entry point of the routine
 *   mov lr, pc             -- return address = the `pop {r0}` below
 *   bx r1                  -- call it (no BLX on ARM7TDMI)
 *   pop {r0}               -- execution comes back here...
 *   bx r0                  -- ...and returns to our caller
 *
 * In other words: run sub_3005E18 as a plain subroutine, then return
 * normally. No arguments are passed and no result is produced; R4 and
 * every other register the callee doesn't own must survive untouched.
 */
Next crt0_start_800027C(void) {
    GUEST_CALL(sub_3005E18);
    RETURN_TO_CALLER();
}

/* ── eStruct2038160_clearStruct ── */
/* eStruct2038160_clearStruct: zero out the 4-byte battle object struct
 * at eStruct2038160 (r2, r3 are preserved across the call). */
Next eStruct2038160_clearStruct(void) {
    R0 = A_eT4BattleObjectsEnd; /* =eStruct2038160 */
    R1 = sizeof(u32);
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── eStruct203F7D8_getUnk01 ── */
/* eStruct203F7D8_getUnk01: return the byte at eStruct203F7D8+1. */
Next eStruct203F7D8_getUnk01(void) {
    R0 = LD8(A_eStruct203F7D8 + 1);
    RETURN_TO_CALLER();
}

/* ── sub_801FE5E ── */
/* sub_801FE5E: read the byte at eStruct203F7D8.
 * Returns r0 = that byte, r3 = the struct's address. */
Next sub_801FE5E(void) {
    R3 = A_eStruct203F7D8;
    R0 = LD8(R3);
    RETURN_TO_CALLER();
}

/* ── chipPack_addChipsToSlotCount_8021b5a ── */
/* chipPack_addChipsToSlotCount_8021b5a(r0 = pointer to the chip pack slot's count byte,
 * r2 = number of chips to add):
 * Adds r2 chips to the slot's count, capped at 99. Returns r1 = the new count and
 * r3 = the outcome: 1 if the slot was already full, 0 if the chips fit exactly,
 * 2 if the count had to be clamped to 99. */
Next chipPack_addChipsToSlotCount_8021b5a(void) {
    u32 *countPtr = (u32 *)R0; /* byte-sized field addressed via r0 */
    u32 count = LD8((u32)countPtr);
    u32 status;

    if (count == 99) {
        /* Slot already full; nothing to add. */
        status = 1;
    } else {
        status = 0;
        count += R2;
        if ((s32)count > 99) {
            /* More chips than the slot can hold; clamp to 99. */
            count = 99;
            status = 2;
        }
    }

    ST8((u32)countPtr, count);
    R1 = count;
    R3 = status;
    RETURN_TO_CALLER();
}

/* ── battle_ClearWorkBuffer_8021D08 ── */
/* battle_ClearWorkBuffer_8021D08: clear the work buffer at unk_203A0A0 (0x2E0 halfwords). */
Next battle_ClearWorkBuffer_8021D08(void) {
    R0 = A_unk_203A0A0;
    R1 = 0x2E0;
    GUEST_CALL(ZeroFillByHalfword);
    RETURN_TO_CALLER();
}

/* ── incrementRecordCounterByte_8021D14 ── */
/* incrementRecordCounterByte_8021D14(r0 = record index, r1 = field offset):
 * Increment the byte at unk_203A0A0 + 0x170*r0 + r1 (a per-entry counter),
 * saturating at 0xFF. Returns the (possibly updated) counter value in r2.
 * Flags: on the saturate path they come from CMP count,0xFF; on the
 * increment path they come from the ADDS. */
Next incrementRecordCounterByte_8021D14(void) {
    u32 entry = A_unk_203A0A0 + 0x170 * R0;

    u32 count = LD8(entry + R1);
    SET_FLAGS_CMP(count, 0xFF);
    if ((s32)count < 0xFF) {
        count = count + 1;
        SET_FLAGS_ADD(count, 1);
        ST8(entry + R1, count);
    }
    R2 = count;
    RETURN_TO_CALLER();
}

/* ── sub_809FADE ── */
/* sub_809FADE(r0 = offset within some data area, r10 = toolkit):
 * Look up the pointer stored in the toolkit at oToolkit_Unk2004348_Ptr
 * (a base address in the 0x02004xxx region) and adjust it by an offset.
 * The incoming r0 is a byte offset relative to 0x1400 that is then scaled
 * by 2 (halfword-sized entries), so the result is:
 *   r0 = base + (offset - 0x1400) * 2
 * and r2 = base.
 *   outputs: r0 = base + (offset - 0x1400) * 2, r2 = base */
Next sub_809FADE(void) {
    u32 base = LD32(R10 + oToolkit_Unk2004348_Ptr);

    R2 = base;
    R0 = base + (R0 - 0x1400) * 2;
    RETURN_TO_CALLER();
}

/* ── mysteryData_InitMapGroupTables_809FAF4 ── */
/* mysteryData_InitMapGroupTables_809FAF4: (re)initialize the mystery data map group tables, first the
 * internet-world table, then the real-world table. The table base pointer is
 * passed to sub_809FB04 in r4; r4-r7 are preserved for the caller. */
Next mysteryData_InitMapGroupTables_809FAF4(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R4 = A_InternetMysteryDataMapGroupEntries;
    GUEST_CALL(sub_809FB04);

    R4 = A_RealWorldMysteryDataMapGroupEntries;
    GUEST_CALL(sub_809FB04);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── record_writeU16IfIdMatches_809FDCE ── */
/* record_writeU16IfIdMatches_809FDCE(r0 = id low byte, r1 = id high byte, r2 = value):
 * Forms a 16-bit id from r1:r0 and compares it against the id recorded at
 * byte_2000210+4. On a match, stores r2 as the record's value at +6.
 * Always leaves the stored id in r1. The record pointer only lives in a
 * scratch register inside the original (r6 is pushed/popped around it), so
 * r4-r7 reach the caller untouched. */
Next record_writeU16IfIdMatches_809FDCE(void) {
    u32 record = A_byte_2000210;
    u32 id = (R1 << 8) | R0;
    u32 storedId = LD16(record + 4);

    R1 = storedId;
    if (id == storedId)
        ST16(record + 6, R2);

    SET_FLAGS_CMP(id, storedId);
    RETURN_TO_CALLER();
}

/* ── sub_809FE36 ── */
/* sub_809FE36(r0 = counter, r4 = value to match):
 * If r4 matches the halfword at byte_2000210+6 (some selection/index field),
 * increment r0. r0 is returned unchanged otherwise. */
Next sub_809FE36(void) {
    u16 match = LD16(A_byte_2000210 + 6);

    if ((s32)R4 == (s32)match)
        R0 = R0 + 1;

    RETURN_TO_CALLER();
}

/* ── goldMysteryData_GetEntryDataIfIdMatches_809FE44 ── */
/* goldMysteryData_GetEntryDataIfIdMatches_809FE44(r0 = mystery data id):
 * Compares the id in r0 against the id currently registered in the gold
 * mystery data work area (byte_2000210). If it matches, fetches that
 * entry's data pointer (GoldMysteryDataEntries[idx].data) and its content
 * byte; otherwise leaves r3/r4 untouched for the caller.
 *   outputs: r4 = entry data pointer, r3 = entry content byte (on match) */
Next goldMysteryData_GetEntryDataIfIdMatches_809FE44(void) {
    u32 work = A_byte_2000210;

    if (R0 == (u32)LD16(work + 6)) {
        u32 idx = LD8(work + 3); /* entry index selected this chapter */
        R4 = LD32(A_GoldMysteryDataEntries + idx * 12 + 4);
        R3 = LD8(work + 2);
    }
    RETURN_TO_CALLER();
}

/* ── gameProgress_resetBuffer_803532c ── */
/* gameProgress_resetBuffer_803532c:
 * Reset the player's game-progress flags. Copies the default values from the
 * progress-flag table at byte_8037694 (two-byte entries; only the low byte of
 * each entry is copied) into the working buffer at gameProgressBuffer_20010f0,
 * then clears the current game-progress state in the game state struct.
 * Returns r3 = 0x19 (number of entries copied); r4-r7 are preserved. */
Next gameProgress_resetBuffer_803532c(void) {
    u32 src = A_byte_8037694;
    u32 dst = A_gameProgressBuffer_20010f0;
    u32 i;

    for (i = 0; i < 0x19; i++) {
        ST8(dst, LD8(src));
        src += 2; /* skip the unused second byte of each table entry */
        dst += 1;
    }

    /* Mark that no game progress has been made yet */
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    ST8(gameState + 6 /* oGameState_GameProgress */, 0);

    R3 = i; /* 0x19 */
    RETURN_TO_CALLER();
}

/* ── clearWorkBuffer_2011A40 ── */
/* clearWorkBuffer_2011A40: clear the 12-byte work buffer at byte_2011A40 (0x2011A40). */
Next clearWorkBuffer_2011A40(void) {
    R0 = A_byte_2011A40;
    R1 = 0xC;
    GUEST_CALL(ZeroFillByWord); /* ZeroFillByWord(ptr, num_bytes) */
    RETURN_TO_CALLER();
}

/* ── sub_80342DC ── */
/* sub_80342DC(r5 = state byte to set):
 * Write 4 (phase "4") to the byte pointed at by r5; r0 = 4 and flags report a
 * nonzero result. */
Next sub_80342DC(void) {
    R0 = 4;
    ST8(R5, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80342E4 ── */
/* sub_80342E4: store 8 to *r5 (e.g. mark a battle object's state/counter byte).
 * Outputs: r0 = 8, nz flags set from that value. */
Next sub_80342E4(void) {
    R0 = 8;
    ST8(R5, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── getIndexed20ByteTableEntry_80355DE ── */
/* getIndexed20ByteTableEntry_80355DE(r0 = entry index):
 * Return the address of entry `r0` in the table at byte_80984C8, whose entries
 * are 0x14 (20) bytes each. r0 = entry address, r1 = table base, r2 = byte offset.
 * Flags reflect the address computation (ADDS), as callers branch on c/v. */
Next getIndexed20ByteTableEntry_80355DE(void) {
    R2 = R0 * 0x14;                 /* offset of the entry within the table */
    R1 = A_byte_80984C8;            /* table base address */
    R0 = R1 + R2;                   /* address of the requested entry */
    SET_FLAGS_ADD(R1, R2);
    RETURN_TO_CALLER();
}

/* ── clearWorkBuffer_2000DC0 ── */
/* clearWorkBuffer_2000DC0: zero-fill the 0x100-byte work buffer at dword_2000DC0 (word-wise). */
Next clearWorkBuffer_2000DC0(void) {
    R0 = A_dword_2000DC0; /* buffer address */
    R1 = 0x100;           /* size in bytes */
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── timer_tickCountdownTable_8035738 ── */
/* timer_tickCountdownTable_8035738: tick the countdown table at dword_2000DC0.
 *
 * The table holds 32 entries of { timer, active }. For every entry whose
 * active flag is nonzero, the timer is decremented and written back. If an
 * active entry's timer is already zero the decrement would go negative:
 * processing stops right there and the decremented value is left in r3
 * without being stored.
 *
 * On return r3 holds the last value computed (the last entry's active flag,
 * or its decremented timer); r4-r7 are preserved. The flags are those of
 * the final subtract (early exit) or of the final loop compare r4 == 0x20
 * (normal exit). */
Next timer_tickCountdownTable_8035738(void) {
    u32 entry = A_dword_2000DC0;
    u32 i;

    for (i = 0; i < 0x20; i++, entry += 8) {
        u32 active = LD32(entry + 4);
        R3 = active;
        if (active != 0) {
            u32 timer = LD32(entry);
            R3 = timer - 1;
            if ((s32)R3 < 0) {
                /* timer underflow: bail out without writing it back;
                 * flags are those of the subtract (cmp timer, #1) */
                SET_FLAGS_CMP(timer, 1);
                RETURN_TO_CALLER();
            }
            ST32(entry, R3);
        }
    }

    /* normal exit: flags come from the final cmp r4, #0x20 (r4 == 0x20) */
    SET_FLAGS_CMP(0x20, 0x20);
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

/* ── chatbox_clearEventFlags2009F38 ── */
/* chatbox_clearEventFlags2009F38(r0 = bit mask): clear the given bits in the
 * chatbox event flags at 0x2009F38. r0 = mask of bits to clear.
 * Leaves r1 and r2 untouched; flags reflect the new flag word. */
Next chatbox_clearEventFlags2009F38(void) {
    u32 mask = R0;
    u32 flags = LD32(A_eFlags2009F38) & ~mask;

    SET_FLAGS_NZ(flags);
    ST32(A_eFlags2009F38, flags);
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

/* ── chatbox_setEventFlags2009F38 ── */
/* chatbox_setEventFlags2009F38(r0 = bits to set): set the given bits in the
 * eFlags2009F38 event-flag word. r1 and r2 are preserved; the nz flags
 * reflect the new flag value. */
Next chatbox_setEventFlags2009F38(void) {
    u32 newFlags = LD32(A_eFlags2009F38) | R0;
    ST32(A_eFlags2009F38, newFlags);
    SET_FLAGS_NZ(newFlags);
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

/* ── playSoundEffect_7A_81258E8 ── */
/* playSoundEffect_7A_81258E8: play sound effect 0x7A. All other registers (r1-r7, r10)
 * pass through to the callee and are preserved unchanged. */
Next playSoundEffect_7A_81258E8(void) {
    R0 = 0x7A;
    GUEST_CALL(PlaySoundEffect);
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

/* ── sub_81278D8 ── */
/* sub_81278D8(r5 = struct pointer): return struct + 0x40 + (s8)(r5[0xD]) * 8,
 * i.e. the address of entry [r5[0xD]] of the 8-byte-element array at r5+0x40. */
Next sub_81278D8(void) {
    s32 index = (s8)LD8(R5 + 0xD);
    u32 base = LD32(R5 + 0x40);
    R0 = base + (index * 8);
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

/* ── getIndexedDataTable_813B9E0 ── */
/* getIndexedDataTable_813B9E0(r0 = table index): select one of three data tables by index and
 * return its address in r0. No bounds check in the original, so the index is
 * used as-is. */
Next getIndexedDataTable_813B9E0(void) {
    u32 index = R0;

    /* Table at off_813B9F0: byte_813B1EC, byte_813B2CD, byte_813B3AE. */
    R0 = LD32(A_off_813B9F0 + (index << 2));

    RETURN_TO_CALLER();
}

/* ── toolkit_getUnk2004190EntryPtr_813B9FC ── */
/* toolkit_getUnk2004190EntryPtr_813B9FC(r0 = entry index):
 * Returns the address of entry `r0` in the 8-bytes-per-entry array whose base
 * pointer is stored in the toolkit struct at oToolkit_Unk2004190_Ptr.
 * Outputs: r0 = base + index*8, r1 = array base, r2 = 8,
 * and flags (notably V) as set by the final ADDS. */
Next toolkit_getUnk2004190EntryPtr_813B9FC(void) {
    u32 base = LD32(R10 + oToolkit_Unk2004190_Ptr);
    u32 offset = R0 * 8;

    R1 = base;
    R2 = 8;
    R0 = offset + base;
    SET_FLAGS_ADD(offset, base);
    RETURN_TO_CALLER();
}

/* ── getWorkAreaPtr_2006CF8 ── */
/* getWorkAreaPtr_2006CF8: trivial getter -- loads the address of the work buffer
 * unk_2006CF8 into r7 and returns. */
Next getWorkAreaPtr_2006CF8(void) {
    R7 = A_unk_2006CF8;
    RETURN_TO_CALLER();
}

/* ── sub_813BD10 ── */
/* sub_813BD10: load the address of unk_2006C88 into r7.
 * A tiny getter used by callers that then access that RAM area. */
Next sub_813BD10(void) {
    R7 = A_unk_2006C88;
    RETURN_TO_CALLER();
}

/* ── getWorkAreaPtr_2006CC0 ── */
/* getWorkAreaPtr_2006CC0: return the base address of the unk_2006CC0 data block in R7. */
Next getWorkAreaPtr_2006CC0(void) {
    R7 = A_unk_2006CC0;
    RETURN_TO_CALLER();
}

/* ── getUnk200431cTableByteCapped3_813BF0C ── */
/* getUnk200431cTableByteCapped3_813BF0C(r0 = index): look up byte at index r0 in the table pointed to
 * by the toolkit's Unk200431c pointer, clamped to 3.
 * Returns r0 = clamped value, r1 = table pointer. */
Next getUnk200431cTableByteCapped3_813BF0C(void) {
    u32 table = LD32(R10 + oToolkit_Unk200431c_Ptr);
    u32 value = LD8(table + R0);

    if ((s32)value > 3)
        value = 3;

    R0 = value;
    R1 = table;
    RETURN_TO_CALLER();
}

/* ── eraseMem_ClearBuffer_813C324 ── */
/* eraseMem_ClearBuffer_813C324: clear the 8-byte record at unk_2000090 (EraseMem buffer).
 * r2 is passed through unchanged. */
Next eraseMem_ClearBuffer_813C324(void) {
    R0 = A_unk_2000090;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);

    RETURN_TO_CALLER();
}

/* ── sub_813C678 ── */
/* sub_813C678: clear the halfword referenced by the toolkit's Unk2004334 pointer
 * (r10 = toolkit). */
Next sub_813C678(void) {
    u32 ptr = LD32(R10 + oToolkit_Unk2004334_Ptr);
    ST16(ptr, 0);
    RETURN_TO_CALLER();
}

/* ── sub_813D62C ── */
/* sub_813D62C(r0 = value): store the value into byte_200BC55 (offset 5 of
 * the eS200BC50 struct); r1 = struct base address. */
Next sub_813D62C(void) {
    u32 base = A_eS200BC50_Index00;
    ST8(base + 5, R0);
    R1 = base;
    RETURN_TO_CALLER();
}

/* ── link_RequestDisconnect_813D8C4 ── */
/* link_RequestDisconnect_813D8C4:
 * Signal "link connection teardown requested" by storing 4 into the link
 * state work area's state field (dword_2009A28, at offset 0x58 of the
 * pointer that the literal pool carries; the port exposes that pointer
 * value as A_eCameraEnd). */
Next link_RequestDisconnect_813D8C4(void) {
    ST32(A_eCameraEnd + 0x58, 4);
    RETURN_TO_CALLER();
}

/* ── test0x200bc50_0x5_813D60C ── */
/* test0x200bc50_0x5_813D60C:
 * Test the byte at eS200BC50+5 and return it (and the struct address) so
 * callers can branch on whether the flag is set.
 *   r0 = byte value, r1 = &eS200BC50, flags = Z/N from the byte. */
Next test0x200bc50_0x5_813D60C(void) {
    u32 flags = LD8(A_eS200BC50_Index00 + 5);

    R0 = flags;
    R1 = A_eS200BC50_Index00;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── getStructEntryFlag2008450 ── */
/* getStructEntryFlag2008450(r0 = struct index): address one of the fixed 0x58-byte
 * entries of eStructArr2008450 and load the byte at its start (its "in use"
 * flag). Returns that byte in r0 with NZ flags set from it; r1 ends holding
 * the entry's address and r2 the byte offset 0x58*index, as callers expect.
 *
 * Note: the original's ADDS (base + 0x58*index) is the last instruction that
 * sets C and V — the trailing TST only updates NZ — so the add's carry and
 * overflow must be reproduced for callers that branch on them. */
Next getStructEntryFlag2008450(void) {
    u32 entryOffset = 0x58 * R0;
    u32 entryAddr = A_eStructArr2008450 + entryOffset;

    SET_FLAGS_ADD(A_eStructArr2008450, entryOffset); /* C/V from the ADDS */

    R1 = entryAddr;
    R2 = entryOffset;
    R0 = LD8(entryAddr);

    SET_FLAGS_NZ(R0); /* TST r0, r0: NZ from the loaded byte */
    RETURN_TO_CALLER();
}

/* ── sub_8046664 ── */
/* sub_8046664: run both halves of the (unspecified) update routine:
 * first sub_80466AA, then sub_804657C. All registers other than the
 * flags are preserved; r4-r7 are untouched here and by both callees. */
Next sub_8046664(void) {
    GUEST_CALL(sub_80466AA);
    GUEST_CALL(sub_804657C);
    RETURN_TO_CALLER();
}

/* ── sub_8046670 ── */
/* sub_8046670(r0 = id byte, r1 = slot index, r2 = param word):
 * initialize one 0x58-byte entry of eStructArr2008450 for r1*0x58:
 * set the entry's id byte, type 9 marker, param word, and clear its counter.
 * Returns r0 = pointer to the entry, r1 = r1*0x58, flags from ADDS r0, entry, #0. */
Next sub_8046670(void) {
    u32 entry = A_eStructArr2008450 + R1 * 0x58;

    ST8(entry + 1, R0);   /* id byte            */
    ST8(entry,      9);   /* entry type = 9     */
    ST32(entry + 4, R2);  /* parameter          */
    ST32(entry + 8, 0);   /* counter/state = 0  */

    R1 = R1 * 0x58;       /* original leaves the scaled index in r1 */
    R0 = entry;
    SET_FLAGS_ADD(entry, 0);  /* adds r0, entry, #0: N/Z from entry, C set, V clear */
    RETURN_TO_CALLER();
}

/* ── sprite_clearScaleCounterAndMakeUnscalable_804668A ── */
/* sprite_clearScaleCounterAndMakeUnscalable_804668A(r5 = pointer to the sprite's scale/cycle counter):
 * Clears the byte at *r5 to 0, then rebuilds the sprite with
 * sprite_makeUnscalable (called with r0 = 0). r4-r7 are preserved. */
Next sprite_clearScaleCounterAndMakeUnscalable_804668A(void) {
    R0 = 0;             /* mov r0, #0 */
    ST8(R5, R0);        /* strb r0, [r5] */

    GUEST_CALL(sprite_makeUnscalable); /* bl sprite_makeUnscalable */

    RETURN_TO_CALLER();
}

/* ── sub_8046DF8 ── */
/* sub_8046DF8: run two update passes (sub_8047950 then sub_8047134) and
 * return 0. Always yields r0 = 0 with N/Z set accordingly. */
Next sub_8046DF8(void) {
    GUEST_CALL(sub_8047950);
    GUEST_CALL(sub_8047134);

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── checkScrambledCounter_8047366 ── */
/* checkScrambledCounter_8047366(r0 = expected value, r5 = record):
 * Validates a scrambled 32-bit field kept at record+0x30. The field is stored
 * bitwise-inverted (so the game can detect it being cleared to zero) and packed as
 *     bits  8..27 : the actual 20-bit counter
 *     bits 28..31 : a "high" nibble h
 *     bits  0..3  : a "low"  nibble l
 * The value compared against r0 is counter - (h * 16 + (l ^ h)).
 * Only the flags are meaningful to callers; r0/r1/r2/r4 must come back unchanged. */
Next checkScrambledCounter_8047366(void) {
    u32 expected = R0;
    u32 field = ~LD32(R5 + 0x30);

    u32 counter = (field << 4) >> 12;   /* bits 8..27 of the field */
    u32 highNibble = field >> 28;
    u32 lowNibble = field & 0xF;
    u32 tweak = (highNibble << 4) | (lowNibble ^ highNibble);

    SET_FLAGS_CMP(counter - tweak, expected);
    RETURN_TO_CALLER();
}

/* ── findRecordWithHalfwordAtOffset2_804743C ── */
/* findRecordWithHalfwordAtOffset2_804743C: scan R4 8-byte records starting at R7, comparing the halfword
 * at offset +2 of each against R0.  Flag result only: Z set if a match was
 * found (flag value 0), N/Z clear if the search ran out of records (flag
 * value 1).  R1 ends as the last halfword read (unchanged if R4 <= 0);
 * R3-R7 are preserved. */
Next findRecordWithHalfwordAtOffset2_804743C(void) {
    u32 target = R0;
    u32 record = R7;
    s32 count = (s32)R4;
    s32 i;
    u32 notFound = 1;

    for (i = 0; i < count; i++, record += 8) {
        R1 = LD16(record + 2);
        if (target == R1) {
            notFound = 0;
            break;
        }
    }

    SET_FLAGS_NZ(notFound);
    RETURN_TO_CALLER();
}

/* ── sub_804758C ── */
/* sub_804758C: thin wrapper that tail-calls sub_80478D4 (all caller registers
 * are preserved, results come straight from the callee). */
Next sub_804758C(void) {
    GUEST_CALL(sub_80478D4);
    RETURN_TO_CALLER();
}

/* ── stub_doNothingReturnZero_8047CA0 ── */
/* stub_doNothingReturnZero_8047CA0: stub that does nothing except return 0 in r0.
 * Sets the flags as `tst r0, r0` with r0 = 0 would (Z set, N clear);
 * r2-r7 are preserved. */
Next stub_doNothingReturnZero_8047CA0(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8047CB0 ── */
/* sub_8047CB0: a stub that always reports "no"/zero; callers branch on the
 * flags it leaves (Z set, N clear, as produced by `mov r0, #0`). */
Next sub_8047CB0(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── menu_clampListScrollAfterCountChange_8047CB6 ── */
/* menu_clampListScrollAfterCountChange_8047CB6(r5 = object):
 * Re-clamp the two u16 fields at +0x14 and +0x18 of the object at r5 after the
 * value at +0x12 changed: +0x14 is pulled down to value-1 (when value is 1..4)
 * and +0x18 is pulled down to value-5 (when value is 5 or more), each only if
 * it is currently above that bound. Callers only inspect the V flag, which
 * reflects the last comparison performed. */
Next menu_clampListScrollAfterCountChange_8047CB6(void) {
    u32 value = LD16(R5 + 0x12);

    if ((s32)value > 4) {
        /* Clamp +0x18 to at most value-5: the test is value < (+0x18)+5. */
        u32 limit = LD16(R5 + 0x18) + 5;
        SET_FLAGS_CMP(value, limit);
        if ((s32)value >= (s32)limit)
            RETURN_TO_CALLER();             /* already low enough */
        /* The subtract below is flag-setting in the original; V ends up clear. */
        SET_FLAGS_CMP(value, 5);
        ST16(R5 + 0x18, value - 5);
    } else {
        /* For small values the +0x14 check runs instead; the TST used to skip
         * the zero case leaves V untouched, so V still comes from this compare
         * when we return early. */
        SET_FLAGS_CMP(value, 4);
        if (value != 0) {
            u32 reduced = value - 1;
            u32 index = LD16(R5 + 0x14);
            SET_FLAGS_CMP(reduced, index);
            if ((s32)reduced <= (s32)index) /* only lower it, never raise it */
                ST16(R5 + 0x14, reduced);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_8047CDC ── */
/* sub_8047CDC(r1 = value): scale a value by 100 unless a mode flag says otherwise.
 * Loads the first word of the struct at [r5 + 0x1C]; when it equals 1 the value
 * is left untouched, otherwise it is multiplied by 100. All registers except
 * r1 are preserved for the caller. */
Next sub_8047CDC(void) {
    u32 modePtr = LD32(R5 + 0x1C);

    if (LD32(modePtr) != 1)
        R1 = R1 * 100;

    RETURN_TO_CALLER();
}

/* ── copy_table_80469F0_to_202F800 ── */
/* copy_table_80469F0_to_202F800: copy the 0x168-word table at byte_80469F0 (ROM) into
 * the working buffer at byte_202F800 (RAM). */
Next copy_table_80469F0_to_202F800(void) {
    R0 = A_byte_80469F0;   /* source: word table in ROM */
    R1 = A_byte_202F800;   /* destination: EWRAM buffer */
    R2 = 0x168;            /* word count */
    GUEST_CALL(CopyWords);
    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_eTextScript201BA20 ── */
/* chatbox_runScript_eTextScript201BA20(r0 = script index):
 * run the fixed text script eTextScript201BA20 in the chatbox, starting at
 * the given index. A thin wrapper around chatbox_runScript_803FD9C that just
 * supplies the archive pointer. */
Next chatbox_runScript_eTextScript201BA20(void) {
    u32 scriptIdx = R0;

    R0 = A_eTextScript201BA20; /* TextScriptArchive *archive */
    R1 = scriptIdx;            /* u8 scriptIdx */

    GUEST_CALL(chatbox_runScript_803FD9C);

    RETURN_TO_CALLER();
}

/* ── folder_findSlotInUseIndex_81377EC ── */
/* folder_findSlotInUseIndex_81377EC(r0 = packed chip data): the high nibble of r0 is a
 * folder slot number. Scan the 3-byte in-use table at unk_20018EC (each entry
 * stores a slot number in its high nibble) and return the index of the entry
 * matching that slot, or 3 if no entry is using it.
 * Outputs: r0 = r1 = entry index (3 = not in use), r2 = last entry's slot
 * nibble, r3 = table address. */
Next folder_findSlotInUseIndex_81377EC(void) {
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

/* ── chipPack_stampEntryWithTrayCountAndDecrement_81376b8 ── */
/* chipPack_stampEntryWithTrayCountAndDecrement_81376b8(r0 = chip pack entry base, r1 = chip index):
 * Copies the current count kept at byte_20065B4 into the halfword field at
 * entry_base + 2*chip_index + 4, then decrements that count.
 * Leaves r1 = chip index * 2 and r2 = pointer to the count for the caller. */
Next chipPack_stampEntryWithTrayCountAndDecrement_81376b8(void) {
    u16 count = LD16(A_byte_20065B4);

    /* entry address: r0 - r1 + 2*r1 == r0 + r1 (byte offset of the entry) */
    u32 entry = R0 + R1;
    ST16(entry + 4, count);

    ST16(A_byte_20065B4, count - 1);

    R1 = R1 << 1;          /* byte offset of the entry's code field */
    R2 = A_byte_20065B4;
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

/* ── sub_8137884 ── */
/* sub_8137884(r0 = index):
 * look up a byte in the table at unk_20018EC and return it. */
Next sub_8137884(void) {
    R0 = LD8(A_unk_20018EC + R0);
    RETURN_TO_CALLER();
}

/* ── ClearCutsceneFlag ── */
/* ClearCutsceneFlag(r0 = flag bit index, r5 = cutscene state object):
 * clear bit (r0 & 0xFF) of the cutscene flags word. r1, r5 and the carry
 * flag are preserved for the caller; r0 and the nz flags are scratch.
 * The carry the caller sees comes from the flag-setting shift that builds
 * the bit mask: carry = last bit shifted out of (1 << n). */
Next ClearCutsceneFlag(void) {
    u32 bitIndex = R0 & 0xFF;
    u32 mask;

    /* lsls r1, r0: mask = 1 << bitIndex (0 for shift counts >= 32) */
    if (bitIndex < 32)
        mask = 1u << bitIndex;
    else
        mask = 0;

    ST32(R5 + oCutsceneState_CutsceneFlags,
         LD32(R5 + oCutsceneState_CutsceneFlags) & ~mask);

    /* Carry out of the shift: bit 32-n of the value 1.
     * A shift count of 0 leaves the carry flag untouched. */
    if (bitIndex != 0) {
        if (bitIndex == 32)
            SET_FLAGS_ADD(0xFFFFFFFFu, 1); /* carry out = 1 */
        else
            SET_FLAGS_ADD(0xFFFFFFFFu, 0); /* carry out = 0 */
    }

    RETURN_TO_CALLER();
}

/* ── CutsceneCameraCmd_end ── */
/* CutsceneCameraCmd_end:
 * "end" camera script command: finish the current camera command and signal
 * success by returning 0 (flags set accordingly for callers that branch on it). */
Next CutsceneCameraCmd_end(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_chatbox_show_8037a70 ── */
/* CutsceneCmd_chatbox_show_8037a70: chatbox cutscene command.
 * Opens/shows the chatbox, advances the cutscene script cursor (r7) by one
 * entry, and returns 1 to signal that the command was handled. */
Next CutsceneCmd_chatbox_show_8037a70(void) {
    GUEST_CALL(chatbox_8040818);

    R7 = R7 + 1;
    SET_FLAGS_ADD(R7, 1); /* c/v come from the pointer increment */

    R0 = 1;
    SET_FLAGS_NZ(R0); /* nz come from the final MOV r0, #1 */

    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_cutscene_end ── */
/* CutsceneCmd_cutscene_end: script command that terminates the running
 * cutscene. Always "fails" (R0 = 0) so the script engine stops executing
 * further commands; flags are set from that zero result as callers expect. */
Next CutsceneCmd_cutscene_end(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_set_cutscene_skip_script ── */
/* CutsceneCmd_set_cutscene_skip_script: cutscene VM command that arms the
 * "skip cutscene" behavior. It reads the 32-bit operand following the command
 * from the script (ReadMapScriptWord, which fetches via r5/r7 and leaves the
 * word in r4), stores it as the script to run once the cutscene is skipped,
 * then advances the script cursor past the command (opcode + 4-byte operand
 * = 5 bytes) and reports success.
 *
 * inputs:  r0 = command id, r5 = cutscene state, r7 = script cursor
 * outputs: r0 = 1, r4 = operand word, r6 = 1, r7 = cursor + 5, nz/c/v,
 *          lr unchanged */
Next CutsceneCmd_set_cutscene_skip_script(void) {
    u32 cursor;

    R6 = 1;
    GUEST_CALL(ReadMapScriptWord);

    ST32(R5 + oCutsceneState_CutsceneScriptAfterCutsceneSkip, R4);

    cursor = R7 + 5;
    SET_FLAGS_ADD(R7, 5);
    R7 = cursor;

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_wait_camera_script ── */
/* CutsceneCmd_wait_camera_script (cutscene command handler):
 * Blocks the cutscene script until the camera script that is running in
 * parallel with it has finished. While the camera script is still active,
 * returns r0 = 0 without advancing the command counter. Once the camera
 * script is done, advances the interpreter's command counter in r7 and
 * returns r0 = 1 so the cutscene continues with the next command.
 * Flags: nz comes from the r0 result; on the "done" path c/v come from
 * the counter increment (as the original's ADDS leaves them). */
Next CutsceneCmd_wait_camera_script(void) {
    u8 cameraScriptActive = LD8(R5 + oCutsceneState_CutsceneCameraScriptActive);

    if (cameraScriptActive != 0) {
        /* Camera script still running: report "not done" yet. */
        R0 = 0;
        SET_FLAGS_NZ(R0);
    } else {
        /* Camera script finished: consume this wait command. */
        SET_FLAGS_ADD(R7, 1);
        R7 = R7 + 1;
        R0 = 1;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump ── */
/* MapScriptCutsceneCmd_jump (08035924): map-script "jump" command handler.
 *
 * Marks the script reader with command id 1 (r6) and calls ReadMapScriptWord,
 * which consumes the next word from the script and performs the actual jump
 * (updating the script pointer in r4). Afterwards r7 is refreshed from r4,
 * and the command always reports success (r0 = 1). */
Next MapScriptCutsceneCmd_jump(void) {
    R6 = 1;                       /* command id passed to ReadMapScriptWord */
    GUEST_CALL(ReadMapScriptWord);

    /* "mov r7, r4" assembles to adds r7, r4, #0, which sets the flags;
     * the following "mov r0, #1" then only updates n/z, leaving c/v from
     * that add (always 0, since adding 0 cannot carry or overflow). */
    R7 = R4;
    SET_FLAGS_ADD(R4, 0);

    R0 = 1;                       /* command handled */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── PlayMapMusic ── */
/* PlayMapMusic():
 * Play the music track assigned to the current map. The track number is a
 * byte stored in the map/cutscene state (S2001c04) pointed to by the toolkit. */
Next PlayMapMusic(void) {
    u32 mapState = LD32(R10 + oToolkit_S2001c04_Ptr);
    R0 = LD8(mapState + 4); /* oS2001c04_MapMusic */
    GUEST_CALL(PlayMusic); /* (song: u8) -> () */
    RETURN_TO_CALLER();
}

/* ── ReadCutsceneCameraScriptHalfword ── */
/* ReadCutsceneCameraScriptHalfword(r0 = script offset, r1 = script base):
 * reads a little-endian halfword from the cutscene camera script at
 * [r1 + r0] and returns it in r0. r1-r3 are preserved. */
Next ReadCutsceneCameraScriptHalfword(void) {
    u32 pos = R1 + R0;

    /* Read byte-by-byte: the address is generally unaligned, so a
     * 32-bit load would rotate on real hardware and read wrong bytes. */
    R0 = (LD8(pos + 1) << 8) | LD8(pos);

    RETURN_TO_CALLER();
}

/* ── ReadCutsceneCameraScriptSignedHalfword ── */
/* ReadCutsceneCameraScriptSignedHalfword(r0 = camera script pointer, r1 = offset
 * into the script): fetch the signed 16-bit value at script+offset and return it
 * sign-extended in r0.
 *
 * The offset can be odd, so the value is assembled from two byte reads exactly
 * as the original does (a single aligned halfword load would read the wrong
 * bytes); r1-r3 are preserved. */
Next ReadCutsceneCameraScriptSignedHalfword(void) {
    u32 addr = R1 + R0;
    u32 value = LD8(addr) | (LD8(addr + 1) << 8);
    R0 = (u32)(s32)(s16)value;
    RETURN_TO_CALLER();
}

/* ── cutscene_setCameraScript ── */
/* cutscene_setCameraScript(r0 = script pointer):
 * Points the cutscene camera at a new script and clears its command timer
 * (offsets 0 and 4 of eCutsceneCameraInfo, the struct at 0x2011BD0 that the
 * literal pool references via A_eWarp2011bb0End), so the script starts fresh.
 * Then calls camera_writeUnk03_14_80301b2(0) to reset the camera's pending
 * write state; the final r1/r2 values are whatever that callee leaves. */
Next cutscene_setCameraScript(void) {
    /* ldr r1, =eCutsceneCameraInfo */
    u32 cameraInfo = A_eWarp2011bb0End;

    /* str r0, [r1, #oCutsceneCameraInfo_CutsceneCameraScriptPtr] */
    ST32(cameraInfo + 0, R0);
    /* str r0, [r1, #oCutsceneCameraInfo_Timer_CommandInitialized] */
    ST32(cameraInfo + 4, 0);

    /* mov r0, #0; bl camera_writeUnk03_14_80301b2 */
    R0 = 0;
    R1 = cameraInfo;
    GUEST_CALL(camera_writeUnk03_14_80301b2);

    RETURN_TO_CALLER();
}

/* ── SetCutsceneFlag ── */
/* SetCutsceneFlag(r0 = flag index, r5 = cutscene state):
 * sets bit (r0 & 0xFF) in the cutscene state's flag word.
 * Returns the updated flag word in r0; r1 is preserved. */
Next SetCutsceneFlag(void) {
    u32 bit = R0 & 0xFF;
    /* ARM LSL by a register yields 0 for shift counts >= 32 */
    u32 mask = (bit < 32) ? (1u << bit) : 0;

    u32 flags = LD32(R5 + oCutsceneState_CutsceneFlags) | mask;
    ST32(R5 + oCutsceneState_CutsceneFlags, flags);

    R0 = flags;
    RETURN_TO_CALLER();
}

/* ── cutscene_TestFlag ── */
/* cutscene_TestFlag(r0 = cutscene flag number, r5 = cutscene state pointer):
 * tests whether the flag numbered r0 is set in the state's cutscene flag
 * word. Leaves r0 = the whole flag word; the Z flag is clear iff the flag
 * is set, so callers branch EQ/NE on the result. r1 is preserved. */
Next cutscene_TestFlag(void) {
    /* Build the flag's bit mask: r1 = 1 << flagNumber, reproducing the
     * barrel shifter's carry output (TST afterwards only touches N/Z). */
    u32 flagNumber = R0 & 0xFF;
    u32 flagBit = 1;

    if (flagNumber != 0) {
        if (flagNumber < 32) {
            flagBit <<= flagNumber;
            cpu.c = 0;                  /* shifting the value 1 left only pushes out zero bits */
        } else {
            flagBit = 0;
            cpu.c = (flagNumber == 32); /* a 32-bit shift pushes out the value's bit 0 */
        }
    }
    /* flagNumber == 0: the mask stays 1 and the carry flag is untouched. */

    u32 flagWord = LD32(R5 + oCutsceneState_CutsceneFlags);
    SET_FLAGS_NZ(flagWord & flagBit);
    R0 = flagWord;
    RETURN_TO_CALLER();
}

/* ── cutscene_clearScriptPosIfDummyValue_8036F24 ── */
/* cutscene_clearScriptPosIfDummyValue_8036F24:
 * If the cutscene's script position holds the magic "dummy" script value,
 * reset both the current and saved script positions to NULL.
 * Leaves r1 = cutscene state pointer; flags reflect the comparison. */
#define CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1 1

Next cutscene_clearScriptPosIfDummyValue_8036F24(void) {
    u32 cutsceneState = LD32(R10 + oToolkit_CutsceneStatePtr);
    u32 scriptPos = LD32(cutsceneState + oCutsceneState_CutsceneScriptPos);

    R1 = cutsceneState;
    SET_FLAGS_CMP(scriptPos, CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1);

    if (scriptPos == CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1) {
        ST32(cutsceneState + oCutsceneState_CutsceneScriptPos, 0);
        ST32(cutsceneState + oCutsceneState_originalCutsceneScriptPos_40, 0);
    }

    RETURN_TO_CALLER();
}
