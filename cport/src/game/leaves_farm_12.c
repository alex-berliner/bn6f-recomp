/*
 * leaves_farm_12.c: small functions from the model farm, set 12.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_addObjectToAllianceEntryList_80077D2 ── */
/* battle_addObjectToAllianceEntryList_80077D2 (r5 = battle object, r10 = toolkit):
 * Append a battle object to its alliance's entry list in the battle state.
 * Each alliance has an entry counter at battleState+8[alliance] and a list of
 * halfword name-ID slots at battleState+0x4C + alliance*8. The object's name
 * ID is written to the next free slot and the counter is bumped.
 * Leaves r0 = name ID, r1 = list offset, r2 = counter offset, r3 = battle
 * state pointer; flags reflect the final address addition (adds r1, r1, r4).
 * r4 and r6 are saved and restored by the original, so they are preserved. */

#define oBattleState_Unk_08 8    /* per-alliance entry counter base */
#define oBattleState_Unk_4C 0x4C /* per-alliance entry list base */

Next battle_addObjectToAllianceEntryList_80077D2(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);

    u32 nameID = LD16(R5 + oBattleObject_NameID);
    u32 alliance = LD16(R5 + oBattleObject_AllianceAndDirectionFlip);

    /* take (and increment) this alliance's next free entry slot */
    u32 counterOffset = oBattleState_Unk_08 + alliance;
    u32 slot = LD8(battleState + counterOffset);
    ST8(battleState + counterOffset, slot + 1);

    /* store the name ID into the alliance's halfword entry list */
    u32 listOffset = (alliance << 3) + oBattleState_Unk_4C;
    R0 = nameID;
    R2 = counterOffset;
    R3 = battleState;
    SET_FLAGS_ADD(listOffset, slot * 2);
    R1 = listOffset + slot * 2;
    ST16(battleState + R1, nameID);
    RETURN_TO_CALLER();
}

/* ── battle_objAdvanceStateTimer_8008492 ── */
/* battle_objAdvanceStateTimer_8008492: advance the per-object battle state byte of the object in r5.
 *
 * - In battle mode 5, or when the 0x40 battle flag is clear, the object is
 *   driven by sub_801483C: once it reports "done" (r0 == 0) and the object's
 *   state byte at +2 is still zero, restart the timer and store state 4.
 * - In every other case (flag test passes, or the state byte was already
 *   non-zero) store state 6 at +4 instead.
 *
 * Flags on return follow the original's last flag-setting instruction on
 * each path: tst r0 (sub_801483C result) or the mov of the stored constant.
 * c/v are left as the last callee left them, matching the original. */
Next battle_objAdvanceStateTimer_8008492(void) {
    GUEST_CALL(GetBattleMode);

    if (R0 != 5) {
        GUEST_CALL(TestBattleFlag_0x40); /* answers in Z; the original branches on it */
        if (!ZERO_FLAG()) {
            R0 = 6;
            SET_FLAGS_NZ(R0);           /* mov r0, #6 */
            ST8(R5 + 4, R0);
            RETURN_TO_CALLER();
        }
    }

    GUEST_CALL(sub_801483C);
    SET_FLAGS_NZ(R0);                   /* tst r0, r0 */
    if (R0 != 0)
        RETURN_TO_CALLER();

    if (LD8(R5 + 2) != 0) {
        R0 = 6;
        SET_FLAGS_NZ(R0);               /* mov r0, #6 */
        ST8(R5 + 4, R0);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(timer_resetAndStart_801482C);
    R0 = 4;
    SET_FLAGS_NZ(R0);                   /* mov r0, #4 */
    ST8(R5 + 2, R0);
    RETURN_TO_CALLER();
}

/* ── clearDword_203CA70Record_80084C0 ── */
/* clearDword_203CA70Record_80084C0: clear the 12-byte record at dword_203CA70 (3 words).
 * Flags, r2, r3 and r5 are left exactly as ZeroFillByWord leaves them. */
Next clearDword_203CA70Record_80084C0(void) {
    R0 = A_dword_203CA70;  /* block to clear */
    R1 = 0xC;              /* size in bytes */
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── battle_runState4CountdownOrAdvance_80092C0 ── */
/* battle_runState4CountdownOrAdvance_80092C0: battle-flow helper driven from the battle state object in r5.
 *
 * - If the state byte at offset 3 is already running, count the 16-bit timer
 *   (oBattleState_Unk_28) down; while it is still positive just return with
 *   the decremented timer in r0. When it expires, write 4 into the 16-bit
 *   field at offset 2 (whose upper half is the state byte, so this also
 *   clears it) and return.
 * - Otherwise, if the pending flag oBattleState_Unk_1a is set, start a fresh
 *   countdown of 10, set the state byte to 4 and return.
 * - Otherwise hand control to battle state 8 with sub-state 0.
 *
 * Outputs: r0 = value written by the last MOV (decremented timer on the
 * still-counting path, 4 when the countdown starts/expires, 0 when switching
 * to state 8), r1 = the pending flag read on the state-0 path (0 when it was
 * clear), flags as the original's last flag-setting instruction leaves them
 * (nz from that MOV, c/v left over from the timer SUBS where it ran). */
Next battle_runState4CountdownOrAdvance_80092C0(void) {
    if (LD8(R5 + 3) != 0) {
        /* State already running: tick the countdown timer. */
        u32 timer = LD16(R5 + oBattleState_Unk_28);
        SET_FLAGS_CMP(timer, 1);
        ST16(R5 + oBattleState_Unk_28, timer - 1);
        if ((s32)(timer - 1) >= 0) {
            /* Still counting down. */
            R0 = timer - 1;
        } else {
            /* Timer expired: state 4 via the halfword at offset 2. */
            R0 = 4;
            SET_FLAGS_NZ(4);
            ST16(R5 + 2, 4);
        }
    } else {
        /* Idle: check whether something is pending. */
        R1 = LD8(R5 + oBattleState_Unk_1a);
        if (R1 != 0) {
            /* Begin a fresh 10-frame countdown and enter state 4. */
            ST16(R5 + oBattleState_Unk_28, 10);
            R0 = 4;
            SET_FLAGS_NZ(4);
            ST8(R5 + 3, 4);
        } else {
            /* Nothing pending: switch to battle state 8, sub-state 0. */
            ST8(R5 + 1, 8);
            R0 = 0;
            SET_FLAGS_NZ(0);
            ST16(R5 + 2, 0);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_8009FCC ── */
/* sub_8009FCC: walk the queue of 16-byte entries at dword_3002180 and hand each
 * one to sub_802FE28. byte_3000EA8 holds the number of entries to process.
 * The entry pointer/counter live only inside this routine; r4-r7 and r5's value
 * reach the caller untouched, while r0-r3 and c/v end up as the last call to
 * sub_802FE28 / the final pointer increment left them. */
Next sub_8009FCC(void) {
    u32 count = LD32(A_byte_3000EA8);
    u32 entry = A_dword_3002180;

    while (count != 0) {
        count--;

        u32 a1 = LD32(entry);
        u32 a2 = LD32(entry + 4);
        u32 a3 = LD32(entry + 8);
        u32 a4 = LD32(entry + 0xC);

        /* sub_802FE28 is free to clobber r6/r7, so the loop state is kept in
         * locals across the call; the callee still sees the same register
         * contents the original passes it. */
        R0 = a1;
        R1 = a2;
        R2 = a3;
        R3 = a4;
        R6 = count;
        R7 = entry;
        GUEST_CALL(sub_802FE28);

        u32 next = entry + 0x10;
        SET_FLAGS_ADD(entry, 0x10);
        entry = next;
    }
    RETURN_TO_CALLER();
}

/* ── battle_getNaviStatsByte_800A540 ── */
/* battle_getNaviStatsByte_800A540(r0 = arg0, r1 = arg1): fetch one byte of the currently
 * selected navi's stats. If the battle-effect flag 0x200000 is set, read it
 * from the active navi's stats block (0x203CCE0, keeping r0 as passed in);
 * otherwise read it from the current PET's navi stats.
 * Returns the byte in r0; r4 and r6 are preserved. */
Next battle_getNaviStatsByte_800A540(void) {
    u32 arg0 = R0;
    u32 arg1 = R1;

    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 0x200000) {
        /* effect active: arg0 still holds the battle-effects result */
        R1 = arg1;
        GUEST_CALL(GetNaviStats203CCE0Byte);
    } else {
        R0 = arg0;
        R1 = arg1;
        GUEST_CALL(GetCurPETNaviStatsByte);
    }
    RETURN_TO_CALLER();
}

/* ── battle_canAllianceAct_800A772 ── */
/* battle_canAllianceAct_800A772(r0 = alliance selector (0 = player, else opponent), r5 = battle object):
 * Decide whether this object is allowed to act.
 * Loads the object's AI data and returns 0 immediately if its chip lockout
 * timer is still running. Otherwise checks the battle state's Unk_5c flags
 * (via battle_getStateUnk5c_800139A) for the calling side's bit:
 * bit 2 (0x04) for the player alliance, bit 3 (0x08) for the opponent
 * alliance. Returns r0 = 1 if the bit is set, 0 if not.
 * Side effects matching the original: r1 ends as the tested bit (or the
 * lockout timer in the early-out path) and r2 ends as the caller's r0. */
Next battle_canAllianceAct_800A772(void) {
    u32 callerR0 = R0;

    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 lockoutTimer = LD8(aiData + oAIData_ChipLockoutTimer);
    if (lockoutTimer != 0) {
        R0 = 0;
        R1 = lockoutTimer;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_getStateUnk5c_800139A);
    u32 stateUnk5c = R0;

    /* Which alliance's flag to test is picked by the caller's r0. */
    u32 allianceFlag = (callerR0 == 0) ? 4 : 8;

    R1 = allianceFlag;
    R2 = callerR0;
    R0 = (stateUnk5c & allianceFlag) ? 1 : 0;
    RETURN_TO_CALLER();
}

/* ── battle_countOpponentActorsWithNameIdInRange_800A7A6 ── */
/* battle_countOpponentActorsWithNameIdInRange_800A7A6(r1 = lowest name ID, r2 = highest name ID):
 * Count the opponent-side battle actors whose name ID falls within
 * [r1, r2] (inclusive). The opponent actor list holds four slots
 * (0x10 bytes), each a pointer that may be NULL.
 * Returns r0 = number of matching actors, r3 = 0x10 (loop end value),
 * with the flags left by the final MOV r0, r4 (ADDS r0, r4, #0). */
Next battle_countOpponentActorsWithNameIdInRange_800A7A6(void) {
    u32 minNameID = R1;
    u32 maxNameID = R2;
    u32 actorList = LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_OpponentActors;
    u32 count = 0;

    for (u32 slot = 0; slot < 0x10; slot += 4) {
        u32 actor = LD32(actorList + slot);
        if (actor != 0) {
            u32 nameID = LD16(actor + oBattleObject_NameID);
            if ((s32)nameID >= (s32)minNameID && (s32)nameID <= (s32)maxNameID)
                count++;
        }
    }

    R0 = count;
    R3 = 0x10;
    SET_FLAGS_ADD(R0, 0); /* MOV r0, r4 == ADDS r0, r4, #0 */
    RETURN_TO_CALLER();
}

/* ── compactEntryWords_800AA64 ── */
/* compactEntryWords_800AA64: compact the 7-word entry (0x1C bytes) returned by
 * getNextEntryAddress_800AAAE. Non-zero words are shifted to the front of the
 * entry and the remainder is zero-filled, so the entry has no holes.
 * Leaves R0 = 0, R1/R3 = one past the entry's last word, and flags from the
 * final CMP of the fill loop. The incoming R4 is preserved.
 * Inputs: R0 is passed through to getNextEntryAddress_800AAAE. */
Next compactEntryWords_800AA64(void) {
    u32 savedR4 = R4;

    GUEST_CALL(getNextEntryAddress_800AAAE);
    u32 entry = R0;

    u32 end = entry + 0x1C; /* the entry is 7 words long */
    u32 src = entry;
    u32 dst = entry;

    /* Move every non-zero word to the front of the entry. */
    while (src != end) {
        u32 word = LD32(src);
        if (word != 0) {
            ST32(dst, word);
            dst += 4;
        }
        src += 4;
    }

    /* Zero-fill the unused tail. */
    while (dst != end) {
        ST32(dst, 0);
        dst += 4;
    }

    R0 = 0;
    R1 = dst; /* == end */
    R3 = src; /* == end */
    R4 = savedR4;
    /* The original's last flag-setting instruction is `cmp r1, r4` with
     * r4 still holding the entry end (it is restored only afterwards). */
    SET_FLAGS_CMP(R1, R3);

    RETURN_TO_CALLER();
}

/* ── copyMemory_8001850 ── */
/* copyMemory_8001850:
 * Loads two fixed data blocks with CopyByEightWords:
 *   - 0x800 bytes from dword_86A5520 into VRAM at 0x600D400
 *   - 0x20 bytes from byte_86BEC80 into IWRAM at unk_3001B40
 * Leaves R1/R2/flags as CopyByEightWords left them; R3 is untouched. */
Next copyMemory_8001850(void) {
    R0 = A_dword_86A5520;
    R1 = 0x600D400;
    R2 = 0x800;
    GUEST_CALL(CopyByEightWords);

    R0 = A_byte_86BEC80;
    R1 = A_unk_3001B40;
    R2 = 0x20;
    GUEST_CALL(CopyByEightWords);

    RETURN_TO_CALLER();
}

/* ── copyToVRAMAndClear_iBGTileIdBlocks_Ptr ── */
/* copyToVRAMAndClear_iBGTileIdBlocks_Ptr: 080015D0-080015FC THUMB, bn6f asm00_0.s:2851
 * inputs:  r3 r10 lr   outputs: -   clobbers: nz c v r0 r1 r2 lr   preserved: r3
 *
 * Uploads the toolkit's iBG tile-id block table to the BG tile-id area of VRAM
 * (0x0600E000, 0x2000 bytes), then resets the in-memory table to 0x2FF
 * ("no tile" entries) so the next frame starts from a clean state.
 */
Next copyToVRAMAndClear_iBGTileIdBlocks_Ptr(void) {
    u32 *tileIdBlocks = (u32 *)LD32(R10 + oToolkit_iBGTileIdBlocks_Ptr);

    /* Copy the current tile-id block table into VRAM. */
    R0 = (u32)tileIdBlocks;
    R1 = A_vBGTileIds;      /* 0x0600E000 */
    R2 = 0x2000;
    GUEST_CALL(CopyByEightWords);

    /* Clear the in-memory table, marking every entry as 0x2FF. */
    R0 = (u32)tileIdBlocks;
    R1 = 0x800;
    R2 = 0x2FF02FF;
    GUEST_CALL(FillByEightWords);

    RETURN_TO_CALLER();
}

/* ── clock_breakPlayTimeIntoHMSF_8000DE0 ── */
/* Frame counts used by the in-game clock: the play-time counter is measured
 * in frames at 60 fps, so one second is 60 frames, one minute 3600 and
 * one hour 216000. */
#define FRAMES_PER_SECOND   0x3Cu     /*  60 frames */
#define FRAMES_PER_MINUTE   0xE10u    /*  60 * 60 frames */
#define FRAMES_PER_HOUR     0x34BC0u  /*  60 * 60 * 60 frames */

/* clock_breakPlayTimeIntoHMSF_8000DE0(r0 = frame counter):
 * Breaks the play-time frame counter down into clock units with three
 * chained divisions (BIOS Div, svc 6, quotient in r0 / remainder in r1):
 *   counter / 216000                -> hours
 *   (counter % 216000) / 3600       -> minutes
 *   (counter % 216000) % 3600 / 60  -> seconds
 *   ... % 60                        -> leftover frames
 * The r2/r3 values the caller passes in are irrelevant; Div only reads
 * r0/r1 and the function overwrites r2/r3 with the last division's
 * quotient and remainder. */
Next clock_breakPlayTimeIntoHMSF_8000DE0(void) {
    u32 totalFrames = R0;

    R0 = totalFrames;
    R1 = FRAMES_PER_HOUR;
    swi_call(6);
    u32 hours = R0;
    u32 rem   = R1;

    R0 = rem;
    R1 = FRAMES_PER_MINUTE;
    swi_call(6);
    u32 minutes = R0;
    rem = R1;

    R0 = rem;
    R1 = FRAMES_PER_SECOND;
    swi_call(6);
    u32 seconds = R0;
    u32 frames  = R1;

    R0 = hours;
    R1 = minutes;
    R2 = seconds;
    R3 = frames;
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_run_text_script ── */
/* CutsceneCmd_run_text_script: "run text script" command of the map cutscene
 * interpreter (r5 = cutscene state, r7 = cursor into the command stream,
 * r10 = map script base, used by ReadMapScriptByte).
 *
 * Command encoding in the stream:
 *   <opcode> <slot>         -> script index is byte <slot> of the cutscene state
 *   <opcode> 0xFF <idx:16>  -> script index is given inline as two bytes
 *
 * Runs the indexed script from the state's text archive in the chatbox,
 * advances the cursor past the command and returns r0 = 1 (keep running the
 * cutscene). */
Next CutsceneCmd_run_text_script(void) {
    /* Read the first operand byte of the command. */
    R6 = 1;                             /* ask ReadMapScriptByte for one byte */
    GUEST_CALL(ReadMapScriptByte);
    u32 operand = R4;

    u32 bytesConsumed;
    if (operand == 0xFF) {
        /* 0xFF marker: the script index follows inline as two bytes. */
        R6 = 2;
        GUEST_CALL(ReadMapScriptByte);
        R1 = R4;
        R0 = LD32(R5 + oCutsceneState_TextArchivePtr);
        GUEST_CALL(chatbox_runScript);
        bytesConsumed = 3;              /* opcode + marker + 2 index bytes */
    } else {
        /* The operand is a slot in the cutscene state whose byte value is
         * the actual script index. */
        R1 = LD8(R5 + operand);
        R0 = LD32(R5 + oCutsceneState_TextArchivePtr);
        GUEST_CALL(chatbox_runScript);
        bytesConsumed = 2;              /* opcode + slot byte */
    }

    /* Advance the command cursor past this command. The ADDS updates the
     * flags; the following MOVS r0,#1 then overwrites only n/z. */
    u32 oldCursor = R7;
    R7 = oldCursor + bytesConsumed;
    SET_FLAGS_ADD(oldCursor, bytesConsumed);

    R0 = 1;                             /* tell the interpreter to continue */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_spawn_cutscene_process ── */
/* Cutscene flag ids, one per script slot: slots 0-3 are the "script paused"
 * flags, slots 4-7 the "script paused long" ones (hence the +0/+4 pairing). */
#define CUTSCENE_FLAG_SCRIPT_1_PAUSED      0u
#define CUTSCENE_FLAG_SCRIPT_1_PAUSED_LONG 4u

/* CutsceneCmd_spawn_cutscene_process (080378C2)
 * Map script command "spawn cutscene process": the command word gives the
 * offset of one of the cutscene state's script-position fields, and the
 * following byte is the value to write into it. Writing the field (re)starts
 * that cutscene script slot, so both pause flags belonging to that slot are
 * cleared. Afterwards the script pointer is advanced past the 6-byte command
 * and r0 = 1 reports that the script should keep running. */
Next CutsceneCmd_spawn_cutscene_process(void) {
    /* Fetch the word parameter: offset of the target field within the cutscene state. */
    R6 = 2;
    GUEST_CALL(ReadMapScriptWord);

    /* Fetch the byte parameter and store it into cutsceneState->field[offset]. */
    R0 = R4;
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    ST32(R5 + R4, R0);

    /* Turn the field offset into an index over the script-position slots. */
    R4 = (R4 - oCutsceneState_CutsceneScriptPos) >> 2;

    /* Clear both pause flags for this slot so the spawned process can run.
     * leftover unused: ReadMapScriptByte's c, ClearCutsceneFlag's c. The
     * original enters both calls with c = 0 (from its add). ClearCutsceneFlag
     * passes c on only when its shift is 0 (slot 0's first flag), and the
     * second call's shift (4-7) sets it, so the c passed here reaches nothing. */
    R0 = R4 + CUTSCENE_FLAG_SCRIPT_1_PAUSED;
    GUEST_CALL(ClearCutsceneFlag);
    R0 = R4 + CUTSCENE_FLAG_SCRIPT_1_PAUSED_LONG;
    GUEST_CALL(ClearCutsceneFlag);

    /* Advance past the 6-byte command (C/V come from this add). */
    SET_FLAGS_ADD(R7, 6);
    R7 = R7 + 6;

    /* r0 = 1 means "continue the script" (the MOV sets N and Z). */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump_if_flag_range_clear ── */
/* MapScriptCutsceneCmd_jump_if_flag_range_clear:
 * Map script command "jump if a range of event flags is clear".
 *
 * Operands, read from the script with the ReadMapScript* helpers (which
 * return their value in r4): a byte giving how many flags to test, then a
 * halfword giving the first flag index. TestEventFlagRange reports whether
 * any flag in [first, first+count) is set. If the whole range is clear, a
 * word operand read afterwards becomes the new script position (jump taken);
 * otherwise the interpreter just steps over the entire command. Both paths
 * return r0 = 1.
 */
Next MapScriptCutsceneCmd_jump_if_flag_range_clear(void) {
    R6 = 1;                          /* ReadMapScript*: fetch a byte operand */
    GUEST_CALL(ReadMapScriptByte);
    u32 flagCount = R4;

    R1 = flagCount;                  /* kept around for TestEventFlagRange */
    R6 = 2;                          /* fetch a halfword operand */
    GUEST_CALL(ReadMapScriptHalfword);

    R0 = R4;                         /* first flag index of the range */
    R2 = R1;                         /* number of flags in the range */
    GUEST_CALL(TestEventFlagRange);

    if (R0 != 0) {                   /* at least one flag set: no jump */
        SET_FLAGS_ADD(R7, 8);
        R7 = R7 + 8;                 /* skip the byte, halfword and word operands */
        R0 = 1;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    R6 = 4;                          /* fetch a word operand */
    GUEST_CALL(ReadMapScriptWord);
    SET_FLAGS_ADD(R4, 0);
    R7 = R4;                         /* jump destination becomes the script position */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump_if_progress_in_range ── */
/* MapScriptCutsceneCmd_jump_if_progress_in_range:
 * Map script command "jump if game progress in range".
 *
 * Script layout: [opcode] [min progress] [max progress] [target word]
 * Reads the current game progress, then the min/max bounds and the jump
 * target from the script. If progress lies within [min, max], the script
 * pointer (r7) is set to the target word; otherwise it is advanced past
 * the whole command (r7 += 7). r0 = 1 in both cases.
 *
 * Note: the game progress byte lives at offset 6 of the game state struct
 * (no named macro for it in this build). ReadMapScriptByte/Word return
 * their result in r4 and may look at r0/r1/r7/r10, so r1 must already
 * hold the lower bound before the upper bound is read. */
Next MapScriptCutsceneCmd_jump_if_progress_in_range(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 progress = LD8(gameState + 6);

    R6 = 1;                                  /* read lower bound */
    GUEST_CALL(ReadMapScriptByte);
    u32 minProgress = R4;
    R1 = minProgress;                        /* original keeps it in r1 across the second read */

    R6 = 2;                                  /* read upper bound */
    GUEST_CALL(ReadMapScriptByte);
    u32 maxProgress = R4;

    if ((s32)progress < (s32)minProgress || (s32)progress > (s32)maxProgress) {
        R7 = R7 + 7;                         /* skip the rest of the command */
        SET_FLAGS_ADD(R7, 7);
        R0 = 1;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    R6 = 3;                                  /* read jump target */
    GUEST_CALL(ReadMapScriptWord);
    R7 = R4;                                 /* jump to the target */
    SET_FLAGS_ADD(R4, 0);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_set_screen_fade ── */
/* MapScriptCutsceneCmd_set_screen_fade: map-script command handler that starts
 * a screen fade. It takes two byte parameters, then calls SetScreenFade with
 * them, advances the script command pointer (r7) by 4 and returns 1.
 *
 * The parameters are located via script argument 1:
 *   - 0xFF means the two parameters follow immediately in the script stream
 *     (arguments 2 and 3, read via ReadMapScriptByte);
 *   - any other value is an offset into the script data (r5) where the two
 *     parameter bytes are stored.
 *
 * ReadMapScriptByte takes the 1-based argument index in r6 and returns the
 * argument byte in r4. */
Next MapScriptCutsceneCmd_set_screen_fade(void) {
    /* Read argument 1: immediate marker (0xFF) or data offset. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    u32 r4 = R4;

    u32 fade_param1, fade_param2;
    if (r4 == 0xFF) {
        /* Immediate form: read both parameter bytes from the script stream. */
        R6 = 2;
        GUEST_CALL(ReadMapScriptByte);
        fade_param1 = r4 = R4;

        R6 = 3;
        GUEST_CALL(ReadMapScriptByte);
        fade_param2 = r4 = R4;
    } else {
        /* Indirect form: parameters live at r5[offset] and r5[offset + 1]. */
        fade_param1 = LD8(R5 + r4);
        r4 = r4 + 1;
        fade_param2 = LD8(R5 + r4);
    }

    R0 = fade_param1;
    R1 = fade_param2;
    GUEST_CALL(SetScreenFade);

    /* r4 ends as the last position touched: the offset+1 in the indirect
     * form, or the second immediate byte in the immediate form. */
    R4 = r4;

    /* Advance to the next map-script command. The add sets c/v (never
     * actually carry here), which the callers can observe; the final
     * `mov r0, #1` then sets n/z on top of them. */
    SET_FLAGS_ADD(R7, 4);
    R7 = R7 + 4;
    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_terminate_one_or_all_gfx_anims ── */
/* MapScriptCutsceneCmd_terminate_one_or_all_gfx_anims (08035F6A):
 * Map script / cutscene command "stop GFX animation(s)".
 *
 * Reads one byte from the script stream: 0xFF means "stop every GFX anim",
 * any other value is the slot number of a single anim to stop. The command
 * is always 2 bytes long, so the script pointer (r7) advances by 2, and the
 * command reports success by returning 1 in r0. */
Next MapScriptCutsceneCmd_terminate_one_or_all_gfx_anims(void) {
    /* ReadMapScriptByte is called with r6 = 1 (read one byte) and hands the
     * byte it read back in r4. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);

    if (R4 == 0xFF) {
        /* Stop all GFX anims, slots 0 .. NUM_GFX_ANIMS-1. r4 doubles as the
         * loop counter and is preserved by TerminateGFXAnim, so it ends up
         * as NUM_GFX_ANIMS, exactly as in the original. */
        for (R4 = 0; (s32)R4 < (s32)NUM_GFX_ANIMS; R4++) {
            R0 = R4;
            GUEST_CALL(TerminateGFXAnim);
        }
    } else {
        /* Stop only the anim whose slot number was read. */
        R0 = R4;
        GUEST_CALL(TerminateGFXAnim);
    }

    /* Advance the script pointer past this 2-byte command and return 1. */
    R7 = R7 + 2;
    SET_FLAGS_ADD(R7, 2);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── cutscene_end_8036ED4 ── */
/* Script value the cutscene interpreter is parked on when a cutscene ends:
 * the "script position" marker consumed by the dummy cutscene script. */
#define CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1 1

/* cutscene_end_8036ED4: end/abandon the currently running cutscene.
 *
 * Parks the cutscene script interpreter on the dummy "do nothing" script
 * (script position 1), clears the state used to resume a script after a
 * cutscene skip, zeroes two halfword fields of the s2001c04 struct, and
 * clears event flag EVENT_1731.
 *
 * r10 = toolkit. Returns with r1/r3/c/v exactly as ClearEventFlagFromImmediate
 * left them (callers branch on its flags and reuse its r1/r3). */
Next cutscene_end_8036ED4(void) {
    u32 cutscene = LD32(R10 + oToolkit_CutsceneStatePtr);

    /* Point the script interpreter at the dummy cutscene script. */
    ST32(cutscene + oCutsceneState_CutsceneScriptPos,
         CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos2, A_DummyCutsceneScript);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos3, A_DummyCutsceneScript);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos4, A_DummyCutsceneScript);

    /* Nothing to run if the player skips out of this cutscene. */
    ST32(cutscene + oCutsceneState_CutsceneScriptAfterCutsceneSkip, 0);

    /* Clear the two halfword counters in the s2001c04 struct. */
    u32 unk2001c04 = LD32(R10 + oToolkit_S2001c04_Ptr);
    ST16(unk2001c04 + oS2001c04_Unk_12, 0);
    ST16(unk2001c04 + oS2001c04_Unk_14, 0);

    /* ClearEventFlagFromImmediate(EVENT_1731): flag group 0x17, bit 0x31. */
    R0 = 0x17;
    R1 = 0x31;
    GUEST_CALL(ClearEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── ereader_updateTimerEnable_8144380 ── */
/* ereader_updateTimerEnable_8144380: update the e-Reader timer-enable flag (eTimerEnable200F460)
 * based on the current SIO mode. When the link port is in the 0x8 mode
 * and the secondary byte_200F462 flag is clear, the flag byte is set to
 * that mode value (0x8); in every other case it is cleared to 0. */
Next ereader_updateTimerEnable_8144380(void) {
    u32 sioMode = LD32(A_SIOControlRegister) & 0xC;
    u32 flagBase = A_eTimerEnable200F460;

    if (sioMode == 0x8 && LD8(flagBase + 2) == 0)
        ST8(flagBase, sioMode);
    else
        ST8(flagBase, 0);

    RETURN_TO_CALLER();
}

/* ── sub_8146BF4 ── */
/* sub_8146BF4: read the state byte of the object referenced by dword_2010CC0
 * (dword_2010CC0 -> struct; first byte is its state code).
 * R0 = 1 if the state is 0 or 1, else 0. NZ/C/V are left as the original's
 * final CMP + MOV sequence leaves them so callers can branch on the result. */
Next sub_8146BF4(void) {
    u32 state = LD8(LD32(A_dword_2010CC0));

    SET_FLAGS_CMP(state, 1);
    if (state == 1) {
        R0 = 1;
    } else {
        SET_FLAGS_CMP(state, 0);
        if (state != 0)
            R0 = 0;
        else
            R0 = 1;
    }
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── loadNextQueuedParams_814E190 ── */
/* loadNextQueuedParams_814E190(r0 = owner struct, r1 = work struct):
 * Read the next entry index from the byte queue at r1+0x40, then fetch the
 * three words of that 12-byte entry from the table pointed to by r0+0x30.
 * Each word is passed through sub_814E0B6 and stored to r1+0x24/0x28/0x2C.
 * Like the original, this uses r12 to hold the return address (mov r12, lr;
 * bx r12), so r12 must be set to the incoming LR before returning. */
Next loadNextQueuedParams_814E190(void) {
    R12 = LR;

    u32 cursor = LD32(R1 + 0x40);
    u32 index = LD8(cursor);          /* queue holds 8-bit entry numbers */
    ST32(R1 + 0x40, cursor + 1);

    u32 entry = LD32(R0 + 0x30) + index * 12;
    R2 = entry;

    R3 = LD32(entry + 0);
    GUEST_CALL(sub_814E0B6);
    ST32(R1 + 0x24, R3);

    R3 = LD32(entry + 4);
    GUEST_CALL(sub_814E0B6);
    ST32(R1 + 0x28, R3);

    R3 = LD32(entry + 8);
    GUEST_CALL(sub_814E0B6);
    ST32(R1 + 0x2C, R3);

    RETURN_TO_CALLER();
}

/* ── sprite_crossfadeColorChannels_814E528 ── */
/* Applies one channel of a color crossfade: multiply the source channel by a
 * fixed-point factor (12.14), then saturate at 0xFF. Only values above 0xFF
 * are clamped, matching the original. */
static u32 fade_channel(u32 channel, u32 factor) {
    u32 result = channel * factor;
    result = (u32)((s32)result >> 14);
    if (result > 0xFF)
        result = 0xFF;
    return result;
}

/* sprite_crossfadeColorChannels_814E528(r4 = destination color struct, r5 = source color struct):
 * Crossfade the two color bytes at [r4+2]/[r4+3] toward the source bytes at
 * [r5+0x10]/[r5+0x11]. The signed byte at [r4+0x14] is the fade position
 * (-0x80..0x7F): the first channel fades in with (0x80 + pos), the second
 * fades out with (0x7F - pos). The byte at [r4+0x12] scales both factors.
 * Leaves r2 = the sign-extended fade position. */
Next sprite_crossfadeColorChannels_814E528(void) {
    u32 weight = LD8(R4 + 0x12);
    u32 pos = (u32)(s8)LD8(R4 + 0x14);

    /* Channel 1 rises as the fade position moves from -0x80 toward +0x7F. */
    ST8(R4 + 2, fade_channel(LD8(R5 + 0x10), (0x80 + pos) * weight));

    /* Channel 2 falls over the same range. */
    ST8(R4 + 3, fade_channel(LD8(R5 + 0x11), (0x7F - pos) * weight));

    R2 = pos;
    RETURN_TO_CALLER();
}

/* ── sound_tickAllMusicPlayers_814EA58 ── */
/* sound_tickAllMusicPlayers_814EA58: walk the fixed music-player table gMPlayTable (0x20 entries,
 * 12 bytes each, first word is the player info pointer) and call
 * sub_814F1E8 on every entry. The entry count is a literal pool constant
 * in the original, so its "count == 0" early-out can never be taken.
 * On return r0 holds the return address: the original pops lr into r0
 * and returns with "bx r0", leaving r0 = lr. */

#define MPLAY_TABLE_COUNT      0x20u
#define MPLAY_TABLE_ENTRY_SIZE 0xCu

Next sound_tickAllMusicPlayers_814EA58(void) {
    const u32 *entry = (const u32 *)A_gMPlayTable;

    for (u32 i = MPLAY_TABLE_COUNT; i != 0; i--) {
        R0 = LD32((u32)entry);
        GUEST_CALL(sub_814F1E8);
        entry = (const u32 *)((u32)entry + MPLAY_TABLE_ENTRY_SIZE);
    }

    /* Flags on exit come from the loop's final "cmp r4, #0"; the loop
     * ends with the counter having just decremented to 0. */
    SET_FLAGS_CMP(0, 0);
    R0 = LR;
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

/* ── bg_drawTileStripTileBlock2_8126404 ── */
/* bg_drawTileStripTileBlock2_8126404(r0 = j, r1 = i, r2 = tileIds, r3 = iSize):
 * Redraws a strip of the background from tile block 2 (a 32x32 tile block),
 * 8 tiles wide. All caller arguments are forwarded to the drawing routine:
 * r0/r1 pass straight through, and r3 doubles as the strip's i size.
 *
 * If bit 0 of the flag word at [r7+0x7C] is set, the strip is drawn by
 * sub_3005EBA with no tile id table (r3 = 0); otherwise the tile ids given
 * in r2 are blitted with CopyBackgroundTiles.
 *
 * On return r3 holds whatever the drawing routine left there; r0, r4-r7
 * and r10 are left as they were on entry. */
Next bg_drawTileStripTileBlock2_8126404(void) {
    u32 tileIds = R2;
    u32 stripSize = R3;
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    if (LD32(R7 + 0x7C) & 1) {
        /* Flag set: alternate draw path, filling without a tile id table. */
        R2 = 2;           /* which_tile_block_32x32 = 2 */
        R3 = 0;           /* tile_ids = none */
        R4 = 8;           /* j_size = 8 */
        R5 = stripSize;   /* i_size */
        R6 = stripSize;
        GUEST_CALL(sub_3005EBA);
    } else {
        R2 = 2;           /* which_tile_block_32x32 = 2 */
        R3 = tileIds;     /* tile_ids */
        R4 = 8;           /* j_size = 8 */
        R5 = stripSize;   /* i_size */
        R6 = stripSize;
        GUEST_CALL(CopyBackgroundTiles);
    }

    /* The call used r4-r7 as arguments/scratch; the caller expects them
     * back exactly as it passed them in. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sprite_drawIfFlagSet_812B504 ── */
/* sprite_drawIfFlagSet_812B504(r5 = pointer to an object):
 * Test the flag byte at r5+0x1B for bit 4 (0x10). If set, invoke
 * sub_811FAF4 with a fixed argument block (0xC80008, 0x20004, 0,
 * &dword_8129184, 0x80004000); otherwise return immediately.
 * r0 and r1 are always written (the load and the #0x10 happen before
 * the branch), r4 is preserved around the call, and on the early-exit
 * path the flags come from the TST. */
Next sprite_drawIfFlagSet_812B504(void) {
    u32 flagByte = LD8(R5 + 0x1B);

    R0 = flagByte;   /* ldrb r0, [r5,#0x1b] */
    R1 = 0x10;       /* mov r1, #0x10 */

    if ((flagByte & 0x10) == 0) {
        /* Bit clear: TST result was zero, return with those flags. */
        SET_FLAGS_NZ(flagByte & 0x10);
    } else {
        u32 savedR4 = R4;

        R0 = 0xC80008;
        R1 = 0x20004;
        R2 = 0;
        R3 = A_dword_8129184;
        R4 = 0x80004000;

        GUEST_CALL(sub_811FAF4);

        R4 = savedR4;   /* the original pushes/pops r4 around the call */
    }
    RETURN_TO_CALLER();
}

/* ── ow_tryTileInteraction_809DF00 ── */
/* ow_tryTileInteraction_809DF00(r5 = overworld player object, r10 = toolkit):
 * Called when the player tries to interact with the tile in front of them.
 * ow_findInteractableNPC_809E01C checks whether tile interaction is currently blocked (e.g. by
 * a cutscene); if so, the game state byte oGameState_Unk_10 is set to 1 and
 * the function returns 1. Otherwise the tile's interaction value
 * (oOWPlayerObject_Unk_0f) is copied into oGameState_CoordInteractionValue,
 * and if it is nonzero it is also latched into game state offset 0x03, then
 * the function returns 0. R7 is left pointing at the game state struct; NZ
 * reflect the return value. */
Next ow_tryTileInteraction_809DF00(void) {
    GUEST_CALL(ow_findInteractableNPC_809E01C);

    if (R0 == 0) {
        /* Interaction is blocked: flag the state and report "blocked". */
        R7 = LD32(R10 + oToolkit_GameStatePtr);
        ST8(R7 + oGameState_Unk_10, 1);
        R0 = 1;
    } else {
        /* Interaction allowed: forward the tile's interaction value. */
        u32 interactionValue = LD8(R5 + oOWPlayerObject_Unk_0f);
        R7 = LD32(R10 + oToolkit_GameStatePtr);
        ST8(R7 + oGameState_CoordInteractionValue, interactionValue);
        if (interactionValue != 0)
            ST8(R7 + 3, interactionValue);
        R0 = 0;
    }

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── ow_findInteractableNPC_809E01C ── */
/* ow_findInteractableNPC_809E01C(r5 = player object):
 * Walk the overworld NPC object array looking for the first NPC the player can
 * interact with (slot in use, interaction not disabled) and record it in the
 * player's interactedNPCObject slot.
 * Returns r0 = 0 when an NPC was found; if the first live NPC has interaction
 * disabled, r0 is that NPC's flags word and nothing is stored; r0 = 1 when no
 * live NPC exists.
 * r1 = 1 and r2 = one-past-end of the NPC array on every exit. */
Next ow_findInteractableNPC_809E01C(void) {
    u32 player = R5;
    u32 npc = A_eOverworldNPCObject0_Flags; /* == eOverworldNPCObjects */
    u32 arrayEnd = npc + 0xD80; /* dword_809E048: byte size of the NPC array */

    SET_FLAGS_ADD(0xD80, npc); /* add r2, r2, r7 */
    R1 = 1;
    R2 = arrayEnd;

    while ((s32)npc < (s32)arrayEnd) {
        u32 state = LD32(npc + oOverworldNPCObject_Unk_50);
        if ((state & 1) != 0) {
            /* NPC slot is live: skip it if interaction is disabled. */
            u32 unkFlags = LD32(npc + oOverworldNPCObject_UnkFlags_60);
            SET_FLAGS_NZ(unkFlags & 1); /* OW_NPC_UNK_FLAGS_60 bit 0 */
            if ((unkFlags & 1) == 0) {
                ST32(player + oOWPlayerObject_interactedNPCObject_58, npc);
                R0 = 0;
                RETURN_TO_CALLER();
            }
            R0 = unkFlags;
            RETURN_TO_CALLER();
        }
        npc += oOverworldNPCObject_Size;
        SET_FLAGS_CMP(npc, arrayEnd);
    }

    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── overworld_CheckPlayerAdvanceForwardTrigger_809E46E ── */
/* overworld_CheckPlayerAdvanceForwardTrigger_809E46E: overworld hook that checks the coordinate trigger at
 * player + 0x1C.  If it is one of the "player advances forward" trigger
 * tiles (0x48..0x4F), store the trigger index (0..7) into the player
 * object at + 0x64 and set the EVENT_1717 event flag. */
#define EVENT_1717_PLAYER_ADVANCE_FORWARD 0x1717u

Next overworld_CheckPlayerAdvanceForwardTrigger_809E46E(void) {
    u32 callerR7 = R7; /* r7 is used as scratch below but must be preserved */

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = player + 0x1C;
    R7 = gameState; /* guest state at the original bl */
    GUEST_CALL(checkCoordinateTrigger_8031a7a);
    s32 trigger = (s32)R0;

    if (trigger >= 0x48 && trigger <= 0x4F) {
        ST16(player + 0x64, (u16)(trigger - 0x48));

        /* SetEventFlagFromImmediate takes the event bank in r0 and the
         * flag index in r1 (movflag EVENT_1717_PLAYER_ADVANCE_FORWARD). */
        R0 = EVENT_1717_PLAYER_ADVANCE_FORWARD >> 8;  /* 0x17 */
        R1 = EVENT_1717_PLAYER_ADVANCE_FORWARD & 0xFF; /* 0x17 */
        GUEST_CALL(SetEventFlagFromImmediate);
    }

    R7 = callerR7;
    RETURN_TO_CALLER();
}

#undef EVENT_1717_PLAYER_ADVANCE_FORWARD

/* ── sprite_initialize ── */
/* sprite_initialize(r0 = sprite resource id, r5 = sprite object):
 * reset an object's sprite state. Stores r0+4 (the first resource slot
 * used by the sprite) into Unk_18, zeroes all animation/position/timer
 * fields, marks the current frame as "none" (Unk_28 = 0xFFFFFFFF) and
 * sets the default 8x8 cell size in Unk_15/Unk_16.
 * The opening ADDS is the last instruction that touches c/v, so those
 * flags are outputs; later MOVs only change nz. */
Next sprite_initialize(void) {
    u32 obj = R5;

    /* flags must come from ADDS r0, #4 */
    u32 base = R0;
    R0 = base + 4;
    SET_FLAGS_ADD(base, 4);

    ST32(obj + oObjectSprite_Unk_18, R0);

    /* clear state, timers and coordinates */
    ST8 (obj + 3, 0);
    ST8 (obj + 0, 0);
    ST32(obj + oObjectSprite_Unk_24, 0);
    ST8 (obj + 4, 0);
    ST16(obj + oObjectSprite_Unk_0a, 0);
    ST16(obj + 6, 0);
    ST32(obj + oObjectSprite_Unk_2c, 0);
    ST16(obj + oObjectSprite_Unk_10, 0);
    ST16(obj + oObjectSprite_Unk_12, 0);
    ST8 (obj + oObjectSprite_Unk_14, 0);
    ST8 (obj + 5, 0);
    ST32(obj + oObjectSprite_Unk_30, 0);
    ST32(obj + oObjectSprite_Unk_34, 0);

    /* no active frame: all bits set */
    ST32(obj + oObjectSprite_Unk_28, 0xFFFFFFFF);

    /* default 8x8 sprite cell */
    ST8 (obj + oObjectSprite_Unk_15, 8);
    ST16(obj + oObjectSprite_Unk_16, 8);

    RETURN_TO_CALLER();
}

/* ── sprite_makeUnscalable ── */
/* Offset of the type/sprite-offset byte in an object header (the disassembly
 * reads it as [r5,#oObjectHeader_TypeAndSpriteOffset], i.e. byte 2). */
#define oObjectHeader_TypeAndSpriteOffset 2

/* sprite_makeUnscalable(r5 = object header):
 * Mark the object's sprite as non-scalable. The sprite block lives 16 bytes
 * per unit past the header, according to the high nibble of the header's
 * type/sprite-offset byte; the body works through that pointer (r5 is
 * temporarily redirected there, and the callee needs it), but the original
 * saves/restores r5 around the call, so the header pointer is what survives.
 * It then strips the "scalable" bits from the sprite state (Unk_11 bits 0-1,
 * Unk_13 bits 1-5) and clears the matching bit in the sprite flag word.
 * The Thumb `add r5, r5, r3` is an ADDS: it sets N/Z/C/V, and those C/V
 * survive the TST on the early-exit path (the callee overwrites all flags
 * on the normal path). NZ on the early-exit path comes from the TST. */
Next sprite_makeUnscalable(void) {
    u32 object = R5;

    R3 = LD8(object + oObjectHeader_TypeAndSpriteOffset) & 0xF0;
    SET_FLAGS_ADD(object, R3);          /* ADDS r5, r5, r3 */
    u32 sprite = object + R3;
    R5 = sprite;                        /* body and callee see the sprite block */

    R1 = 3;
    R0 = LD8(sprite + oObjectSprite_Unk_11);
    if ((R0 & R1) == 0) {
        SET_FLAGS_NZ(R0 & R1);          /* tst r1, r0: C/V stay from the ADDS */
        R5 = object;                    /* pop {r5} restores the header pointer */
        RETURN_TO_CALLER();
    }

    ST8(sprite + oObjectSprite_Unk_11, R0 & ~R1);

    R0 = LD8(sprite + oObjectSprite_Unk_13);
    R2 = R0;
    R1 = 0x3E;
    R0 = (R0 & R1) >> 1;                /* bit index within the flag word */
    R2 &= ~R1;
    ST8(sprite + oObjectSprite_Unk_13, R2);
    GUEST_CALL(flags_clearFlagWordBit_802FE6A);

    R5 = object;                        /* pop {r5} restores the header pointer */
    RETURN_TO_CALLER();
}

/* ── NPCCommand_change_movement_direction ── */
/* NPCCommand_change_movement_direction (0x0809F23E):
 * NPC script command that makes the NPC start walking in a new direction.
 *   r5 = NPC object, r6 = current script read position.
 * The operand at [r6+1] is a turn amount relative to the current movement
 * direction (0 = 0 degrees, 2 = 90 degrees, 4 = 180 degrees, 6 = 270 degrees).
 * After re-aiming, the NPC's speed/distance reload values are copied into the
 * active movement parameters and its action is set to "walking".
 * Leaves r2 = movement distance, r4 = movement distance reload offset (as the
 * original does), r0 = whatever the disable-script helper returns, and
 * flags from `add r6, #2`. */
Next NPCCommand_change_movement_direction(void) {
    u32 turn = LD8(R6 + 1);

    /* new direction = current direction + turn amount, wrapped to 8
     * directions (0, 2, 4, 6 don't actually move the NPC: the movement
     * offsets for those directions are undefined) */
    R4 = oOverworldNPCObject_MovementDirectionReload;
    u32 direction = LD8(R5 + R4) + turn;
    direction &= 7;
    ST8(R5 + oOverworldNPCObject_MovementDirection, direction);

    /* transfer movement speed and distance reload values into the active
     * slots (oOverworldNPCObject_MovementSpeed = 4,
     * oOverworldNPCObject_MovementDistance = 5) */
    R2 = LD8(R5 + oOverworldNPCObject_MovementSpeedReload);
    ST8(R5 + 4, R2);

    R2 = LD8(R5 + oOverworldNPCObject_MovementDistanceReload);
    ST8(R5 + 5, R2);
    R4 = oOverworldNPCObject_MovementDistanceReload; /* r4 left as in the original */

    /* set the action to "default moving" (MOVEMENT_FLAG_DEFAULT_MOVING = 4,
     * oOverworldNPCObject_CurAction = 9) and clear the movement state */
    ST8(R5 + 9, 4);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    GUEST_CALL(npc_disableScript0x19_809f51e);

    /* advance the script read position past the command and its operand,
     * setting flags from the add as the original does */
    SET_FLAGS_ADD(R6, 2);
    R6 = R6 + 2;

    RETURN_TO_CALLER();
}

/* ── npc_inChatbox_809EADA ── */
/* npc_inChatbox_809EADA(self: r5 = OverworldNPCObject):
 * Overworld NPC state handler for an NPC standing in a chatbox. Runs the
 * action handler picked by CurAction (field at offset 9) out of the table
 * at off_809EAFC (its only entry is npc_inChatbox_curAction_809EB04), then
 * refreshes the NPC's sprite and interaction areas. If bit 0x40 of
 * UnkFlags_60 is set, sub_809F922 runs afterwards as well. */
Next npc_inChatbox_809EADA(void) {
    /* oOverworldNPCObject_CurAction is at offset 9 */
    u32 action = LD8(R5 + 9);

    /* dispatch to the CurAction handler; r0 carries the action index and
     * r7 the table entry, as in the original */
    R0 = action;
    R7 = LD32(A_off_809EAFC + action);
    GUEST_CALL(npc_inChatbox_curAction_809EB04);

    GUEST_CALL(sprite_update);
    GUEST_CALL(npc_createInteractionAreas_809F526);

    R0 = LD32(R5 + oOverworldNPCObject_UnkFlags_60);
    R1 = OW_NPC_UNK_FLAGS_60_0x40;
    if (R0 & R1) {
        GUEST_CALL(sub_809F922);
    } else {
        /* leave NZ as the TST r0, r1 left them (C/V untouched) */
        SET_FLAGS_NZ(R0 & R1);
    }
    RETURN_TO_CALLER();
}

/* ── countMatchingRun_800C84A ── */
/* countMatchingRun_800C84A(r0 = start row, r1 = step):
 * Measures a run of identically-valued entries in a byte table whose entries
 * are 8 bytes apart. battle_getPanelEntry_800C8C2 is asked for the address of the entry lying
 * next to the start row in the direction given by step (+1/-1); the value
 * stored there is the run's value, and the run grows entry by entry for as
 * long as the row counter stays within the 1..6 range and the entries keep
 * matching it.
 * Returns r0 = run length, r1 = the value the run consists of.
 * r4-r6 are only scratch here and are restored for the caller. */
Next countMatchingRun_800C84A(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;

    u32 row = R0;   /* row the run starts from */
    u32 step = R1;  /* which way to walk along the column (1 or -1) */

    /* The original copies r0->r4 and r1->r5 before the call; the lookup
     * helper takes them as its arguments and preserves r4/r5, so the copies
     * double as both scratch state and arguments. R2/R3/R6 still hold their
     * incoming values here, as in the original. */
    R4 = row;
    R5 = step;
    GUEST_CALL(battle_getPanelEntry_800C8C2);

    u32 cell = R0;              /* address of the first entry of the run */
    u32 stride = 8u * step;     /* entries sit 8 bytes apart */
    u32 runValue = LD8(cell);   /* value every entry of the run shares */
    u32 runLength = 1;

    cell += stride;
    while ((u32)(row - 1) <= 5u && LD8(cell) == runValue) {
        runLength++;
        cell += stride;
        row += step;
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R0 = runLength;
    R1 = runValue;
    RETURN_TO_CALLER();
}

/* ── battle_getPanelEntry_800C8C2 ── */
/* battle_getPanelEntry_800C8C2(r0 = panel index): return a pointer to the panel's entry in the
 * battle panel array at unk_2034010 (each entry is 8 bytes). */
Next battle_getPanelEntry_800C8C2(void) {
    R0 = A_unk_2034010 + 8 * R0;
    RETURN_TO_CALLER();
}

/* ── battle_scanPanelsInFront_800D58C ── */
/* battle_scanPanelsInFront_800D58C(r5 = battle object):
 * Sweep the panels in front of the acting object, one panel at a time in
 * the direction it faces, and return the panel index (r0) where the sweep
 * stops. The sweep stops early when battle_countAdjacentPanelsOfOtherAlliance_800D5F0 reports a hit on a panel
 * for the object's alliance, or when the next panel would leave the field
 * (in which case object_isValidPanel's result is returned instead).
 * r4/r6/r7 are saved and restored by the original's push/pop, so they keep
 * their entry values; flags on return come from the final "mov r0, r6". */
Next battle_scanPanelsInFront_800D58C(void) {
    /* The original pushes/pops r4, r6 and r7: they must end unchanged. */
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    /* Start from the panel the object (r5) currently occupies. */
    GUEST_CALL(sub_800D5BA);
    u32 panel = R0;
    R6 = panel;

    /* The object's alliance decides which panels count as blocked. */
    u32 alliance = LD8(R5 + oBattleObject_Alliance);
    R7 = alliance;

    /* Step to the adjacent panel in the direction the object faces. */
    GUEST_CALL(object_getFrontDirection);
    u32 step = R0;
    R4 = step;

    for (;;) {
        /* Something to interact with on this panel: stop here. */
        R0 = panel;
        R1 = alliance;
        R6 = panel;
        GUEST_CALL(battle_countAdjacentPanelsOfOtherAlliance_800D5F0);
        if (R0 != 0)
            break;

        /* Advance one panel; running off the field ends the sweep. */
        panel += step;
        R6 = panel;
        R0 = panel;
        R1 = 1;
        GUEST_CALL(object_isValidPanel);
        if (R0 == 0) {
            panel = R0;
            break;
        }
    }

    R0 = panel;
    SET_FLAGS_NZ(R0);
    R4 = saved_r4;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── ai_resetAIDataSlots_800ED00 ── */
/* ai_resetAIDataSlots_800ED00: reset the AI actor pool.
 *
 * For each of the eight AI slots, store a one-hot mask word at offset 0x7C:
 * slot i gets 0x80000000 >> i. Then clear the "AI data in use" bitfield.
 *
 * Register side effects (from the final loop iteration and tail):
 *   r0 = 0, r1 = &eUsedAIDataBitfield, r2 = 8, r3 = 0x100 (slot stride),
 *   flags from the loop's last "cmp r2, #8" (equal: Z=1, C=1, V=0). */
Next ai_resetAIDataSlots_800ED00(void) {
    u32 ai = A_eAIData0_ActorType;   /* base of eAIData, one slot per AI actor */
    u32 mask = 0x80000000;
    u32 slot = 0;

    do {
        ST32(ai + 0x7C, mask);
        ai += 0x100;                 /* stride between AI slots */
        mask >>= 1;
        slot++;
    } while (slot < 8);

    ST32(A_eUsedAIDataBitfield, 0);

    R0 = 0;
    R1 = A_eUsedAIDataBitfield;
    R2 = slot;                       /* 8 */
    R3 = 0x100;
    SET_FLAGS_CMP(slot, 8);          /* last "cmp r2, #8": equal, so Z=1, C=1, V=0 */

    RETURN_TO_CALLER();
}

/* ── link_getModeSelector_803DD30 ── */
/* link_getModeSelector_803DD30: derive a small "link state" selector and return it in R0
 * (zero flag set iff the result is 0).
 *
 * It reads the jump offset slot of the structure at eStruct200BC30:
 *   offset 4 -> 0
 *   offset 8 -> 1
 *   anything else (including 0) -> the low 2 bits of the word at
 *   eLinkState + 0x48 (dword_2009A18). */
Next link_getModeSelector_803DD30(void) {
    GUEST_CALL(eStruct200BC30_getJumpOffset00);
    u32 jumpOffset = R0;

    if (jumpOffset == 4) {
        R0 = 0;
    } else if (jumpOffset == 8) {
        R0 = 1;
    } else {
        /* default: dword_2009A18 (eLinkState field 0x48), masked to 2 bits */
        R0 = LD32(A_eCameraEnd + 0x48) & 3;
    }

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── verifyEwramWorkAreaChecksum_803F8C4 ── */
/* verifyEwramWorkAreaChecksum_803F8C4: recompute the checksum over the EWRAM work area
 * (ewram_2000000, 0x6710 bytes) and compare it against the value saved at
 * S2001c04+0x68, which is then cleared.
 * Returns r0 = 0 if the checksum matches (data intact), r0 = 1 otherwise.
 * Sets Z/NZ flags from r0; r1-r7 are preserved. */
Next verifyEwramWorkAreaChecksum_803F8C4(void) {
    u32 state = LD32(R10 + oToolkit_S2001c04_Ptr);
    u32 storedChecksum = LD32(state + 0x68);
    ST32(state + 0x68, 0);

    /* Sum every byte of the work area, plus the fixed 0x18 bias. */
    u32 sum = 0x18;
    for (s32 i = 0x670F; i >= 0; i--)
        sum += LD8(A_ewram_2000000 + i);

    R0 = (sum == storedChecksum) ? 0 : 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── shop_resetStockTable_8048C68 ── */
/* shop_resetStockTable_8048C68:
 * (Re)initialize the shop data buffer owned by the toolkit: clear all 0xE80
 * bytes, then copy the default stock table from ROM over it. The table is a
 * run of 8-byte entries; an entry whose second byte is 0 is an empty slot and
 * is left zeroed instead of copied.
 *
 * inputs:  r2 r3 r10 (toolkit) lr
 * outputs: r2 (leftover byte count from the copy loop, negative on exit)
 * clobbers: r0 r1 r3 lr, flags
 */
Next shop_resetStockTable_8048C68(void) {
    u32 shopData = LD32(R10 + oToolkit_ShopDataPtr);

    R0 = shopData;
    R1 = 0xE80;
    GUEST_CALL(ZeroFillByWord);

    u32 src = A_byte_8047DA0;
    u32 dest = shopData;
    u32 remaining = 0xE80;

    for (;;) {
        remaining -= 8;
        if ((s32)remaining < 0)
            break;

        if (LD8(src + 1) != 0) {
            R0 = src;
            R1 = dest;
            R2 = 8;
            GUEST_CALL(CopyBytes);
        }

        src += 8;
        dest += 8;
    }

    R2 = remaining;
    RETURN_TO_CALLER();
}
