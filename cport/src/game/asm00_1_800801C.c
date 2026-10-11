/*
 * asm00_1_800801C.c: readable functions of bn6f asm00_1.s, from 0x0800801C, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

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

/* ── task484_RunFrame_800927C ── */
/* task484_RunFrame_800927C: run one frame of the task identified by ID 0x484.
 * Requests the task (setting its request bits), dispatches it, then runs
 * the shared post-dispatch steps and clears the active task state. */
Next task484_RunFrame_800927C(void) {
    /* Request the task and let the dispatcher run it. */
    R0 = 0x484;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);

    R0 = 0x484;
    GUEST_CALL(dispatch_801DA48);

    /* Shared bookkeeping after a dispatched task. */
    GUEST_CALL(sub_801E5F8);
    GUEST_CALL(battle_sceneFrameSetup_801DA24);

    /* Clear the task state (r0 = r1 = 0). */
    R0 = 0;
    R1 = 0;
    GUEST_CALL(sub_801E0A0);

    RETURN_TO_CALLER();
}

/* ── battle_dispatchStatePhase_80092A0 ── */
/* battle_dispatchStatePhase_80092A0: battle-state phase dispatcher.
 *
 * r5 points to the battle state; the byte at offset 2 selects an entry in
 * the handler table at off_80092B4 (entries at byte offsets 0, 4 and 8):
 *   0 -> battle_runState4CountdownOrAdvance_80092C0, 4 -> sub_80092F0, 8 -> sub_8009314.
 *
 * The original does `ldr r1, [table, phase]; mov lr, pc; bx r1`, so the
 * handler is entered with r0 = the phase byte and r1 = its own (Thumb)
 * entry address; r2-r12 pass through untouched. The handler's results
 * (registers and flags) flow straight back to our caller. */
Next battle_dispatchStatePhase_80092A0(void) {
    u32 phase = LD8(R5 + 2); /* oBattleState_Unk_02 */
    u32 handler = LD32(A_off_80092B4 + phase);

    R0 = phase;
    R1 = handler;

    switch (handler) {
    case 0x080092C1: /* battle_runState4CountdownOrAdvance_80092C0 + 1 (Thumb) */
        GUEST_CALL(battle_runState4CountdownOrAdvance_80092C0);
        break;
    case 0x080092F1: /* sub_80092F0 + 1 (Thumb) */
        GUEST_CALL(sub_80092F0);
        break;
    case 0x08009315: /* sub_8009314 + 1 (Thumb) */
        GUEST_CALL(sub_8009314);
        break;
    default:
        /* Out-of-range phase: the original would jump to whatever word the
         * table lookup produced; no valid game state reaches this. */
        break;
    }

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

/* ── sub_8009338 ── */
/* sub_8009338 (r5 = battle state):
 * Per-frame tick of the battle-end sequence.
 *
 * The first time through (byte at +3 still clear), raise flag 4 in the
 * battle state's unk11 flag set, mark the sequence as started and run the
 * one-time setup in sub_8026840. Every frame then polls sub_8026A28:
 *   0 -> sequence still running, nothing to do this frame;
 *   2 -> run battle_stateGotoPhase14_800AABC (one final step) and return;
 *   otherwise the sequence is over: flag both fighters' AI data and switch
 *   the battle state machine to index 0xC with the halfword at +2 cleared. */
Next sub_8009338(void) {
    if (LD8(R5 + 3) == 0) {
        R0 = 4;
        GUEST_CALL(setBattleStateUnk11Flag_800A9CA);
        ST8(R5 + 3, 1);
        GUEST_CALL(sub_8026840);
    }

    GUEST_CALL(sub_8026A28);
    u32 phase = R0;

    if (phase == 0) {
        /* Still in progress: return with the flags of "cmp r0, #0". */
        SET_FLAGS_CMP(phase, 0);
        RETURN_TO_CALLER();
    }

    if (phase == 2) {
        GUEST_CALL(battle_stateGotoPhase14_800AABC);
        RETURN_TO_CALLER();
    }

    /* Battle finished: tell the AI on both sides (alliance 0 and 1). */
    R0 = 0;
    GUEST_CALL(battle_findPlayer);
    SET_FLAGS_CMP(R0, 0);              /* cmp r0, #0 */
    if (R0 != 0) {
        R0 = LD32(R0 + oBattleObject_AIDataPtr);
        R1 = 1;
        ST8(R0 + oAIData_Unk_0f, R1);
    }

    R0 = 1;
    GUEST_CALL(battle_findPlayer);
    SET_FLAGS_CMP(R0, 0);              /* cmp r0, #0 */
    if (R0 != 0) {
        R0 = LD32(R0 + oBattleObject_AIDataPtr);
        R1 = 1;
        ST8(R0 + oAIData_Unk_0f, R1);
    }

    /* Advance to battle state 12 and clear the counter halfword. */
    R0 = 0xC;
    ST8(R5 + 1, R0);
    SET_FLAGS_NZ(R0);                  /* mov r0, #0xC */
    R0 = 0;
    ST16(R5 + 2, R0);
    SET_FLAGS_NZ(R0);                  /* mov r0, #0 */
    RETURN_TO_CALLER();
}

/* ── battle_DispatchStatePhase_800945C ── */
/* battle_DispatchStatePhase_800945C: battle-state phase dispatcher.
 *
 * R5 points to the battle state; the phase byte at offset 2 selects one of
 * two handlers from the jump table at off_8009470. The phase byte is passed
 * to the handler in r0, and — exactly as the original's `ldr r1, [r1, r0]`
 * leaves it — r1 holds the selected table entry (the handler's Thumb
 * address, bit 0 set) when the handler starts.
 *
 * The original pushes lr before the call, so the handler runs with SP four
 * bytes lower than on entry; that slot has to exist here too. r2-r12 and
 * the incoming flags pass through to the handler untouched, and whatever
 * the handler leaves in the registers and flags is returned to our caller. */
Next battle_DispatchStatePhase_800945C(void) {
    u32 phase = LD8(R5 + 2);                /* ldrb r0, [r5,#2] */

    /* push {lr}: the handler observes the lowered SP */
    SP -= 4;
    ST32(SP, LR);

    R0 = phase;
    R1 = LD32(A_off_8009470 + phase * 4);   /* handler address | 1 */
    if (phase == 0)
        GUEST_CALL(battle_waitLinkSync_8009478);
    else
        GUEST_CALL(sub_80094B6);

    SP += 4;                                /* pop {pc}: return through LR */
    RETURN_TO_CALLER();
}

/* ── battle_waitLinkSync_8009478 ── */
/* battle_waitLinkSync_8009478: link-battle sync wait handler for the current battle state
 * (r5). On first entry it resets the remote-ID tracker and starts a frame
 * timer. It then waits until both sides report the same tracked ID; once
 * they match it shuts the tracker down and signals readiness, otherwise it
 * counts frames and, after 180 frames (~3 s), forces the battle to move on
 * by switching the state index to 0xC.
 * Outputs: nz/c/v as left by the last flag-setting op; r0 = 4 on the
 * init/match paths, the incremented timer value (or 0xC on timeout) on
 * the wait path. */
Next battle_waitLinkSync_8009478(void) {
    u32 state = R5;

    if (LD8(state + 3) == 0) {
        /* First run: reset the tracked-ID state and start the wait timer. */
        GUEST_CALL(sub_800B428);
        ST16(state + oBattleState_Unk_28, 0);
        ST8(state + 3, 4);
        R0 = 4;
        SET_FLAGS_NZ(4);
        RETURN_TO_CALLER();
    }

    R0 = 0x1F2F3F4F;
    GUEST_CALL(battle_doBothTrackedIdsMatch_800B46C);

    if (R0 != 0) {
        /* Both sides agree: shut down the tracker and signal readiness. */
        GUEST_CALL(sub_800B460);
        GUEST_CALL(sub_800B444);
        ST16(state + 2, 4);
        R0 = 4;
        SET_FLAGS_NZ(4);
        RETURN_TO_CALLER();
    }

    /* Still waiting: count frames; r0 stays equal to the incremented
     * timer. After 180 frames, force progress by switching the battle
     * state to its next handler (state index lives at offset 0). */
    u32 timer = LD16(state + oBattleState_Unk_28) + 1;
    ST16(state + oBattleState_Unk_28, timer);
    R0 = timer;
    SET_FLAGS_CMP(timer, 0xB4);
    if ((s32)timer > 0xB4) {
        R0 = 0xC;
        ST32(state + 0, 0xC);
        SET_FLAGS_NZ(0xC);
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

/* ── fifo_pushRecord16_8009FF8 ── */
/* fifo_pushRecord16_8009FF8(r0..r3 = four words of a record):
 * Append one 16-byte record to the FIFO at dword_3002180. The write slot is
 * selected by the counter word at byte_3000EA8, which is then incremented
 * (truncated to 28 bits by the original's lsl/lsr pair, so only the low
 * 28 bits ever wrap/grow).
 * r4/r6/r7 are saved and restored by the original, so they are preserved.
 * The flags are those of `adds r4, #1` on the new count (callers test it). */
Next fifo_pushRecord16_8009FF8(void) {
    u32 count = LD32(A_byte_3000EA8);
    u32 slot = A_dword_3002180 + (count << 4);

    ST32(slot + 0x00, R0);
    ST32(slot + 0x04, R1);
    ST32(slot + 0x08, R2);
    ST32(slot + 0x0C, R3);

    /* lsl r4,#4 / lsr r4,#4: keep only the low 28 bits of the counter */
    count = (count << 4) >> 4;
    SET_FLAGS_ADD(count, 1);
    count += 1;
    ST32(A_byte_3000EA8, count);

    RETURN_TO_CALLER();
}

/* ── sub_800A01C ── */
/* sub_800A01C: clear the flag/state word at byte_3000EA8.
 * Returns r0 = 0, r1 = &byte_3000EA8, with Z set (result of "mov r0, #0"). */
Next sub_800A01C(void) {
    R0 = 0;
    R1 = A_byte_3000EA8;
    ST32(R1, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── PauseBattle ── */
/* PauseBattle (0800A028): set the battle-paused flag on the current game state.
 * r10 = toolkit; returns r0 = 1, r1 = game state pointer, NZ from r0 = 1. */
Next PauseBattle(void) {
    R1 = LD32(R10 + oToolkit_GameStatePtr);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    ST8(R1 + oGameState_BattlePaused, R0);
    RETURN_TO_CALLER();
}

/* ── UnpauseBattle ── */
/* UnpauseBattle: clear the "battle paused" flag of the current game state. */
Next UnpauseBattle(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    ST8(gameState + oGameState_BattlePaused, 0);
    RETURN_TO_CALLER();
}

/* ── battle_isPaused ── */
/* battle_isPaused (0x0800A03C):
 * Read the battle-paused flag from the current game state.
 * r10 = toolkit pointer. Returns r0 = oGameState_BattlePaused byte and
 * leaves Z set iff the game is paused (r0 == 0); r1 = game state pointer. */
Next battle_isPaused(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 paused = LD8(gameState + oGameState_BattlePaused);

    R0 = paused;
    R1 = gameState;
    SET_FLAGS_NZ(paused);

    RETURN_TO_CALLER();
}

/* ── battle_checkFlowAdvanceFlags_800A046 ── */
/* battle_checkFlowAdvanceFlags_800A046: decide whether the battle flow may advance this frame.
 * Returns 0xFF while the battle is over or time is stopped, otherwise
 * reports the state of the two flag halfwords at dword_2036820
 * (bit 3 of +4 wins over bit 3 of +0xC). Also leaves the symbol address
 * in R3 and the test mask (8) in R2 for its callers. */
Next battle_checkFlowAdvanceFlags_800A046(void) {
    u32 result = 0xFF; /* default: caller must wait */

    GUEST_CALL(battle_isBattleOver);
    if (R0 == 0) {
        GUEST_CALL(battle_isTimeStop);
        if (R0 == 0) {
            u32 flags = A_dword_2036820;

            R3 = flags;
            R2 = 8;

            if ((LD16(flags + 4) & 8) != 0)          /* dword_2036824 bit 3 */
                result = 0;
            else if ((LD16(flags + 0xC) & 8) != 0)   /* dword_203682C bit 3 */
                result = 1;
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── battle_isTimeStopPauseOrBattleFlags0x20_800a0a4 ── */
/* battle_isTimeStopPauseOrBattleFlags0x20_800a0a4:
 * Predicate consulted all over the battle engine to decide whether battle
 * time is currently standing still. It yields 1 when
 *   - battle_isTimeStop() reports an active time-stop, or
 *   - battle_isPaused() reports the game is NOT paused, or
 *   - battle_getFlags() has bit 0x20 set,
 * and 0 only when none of those hold.
 * r1 is left as the last callee left it on the early exits, and is 0x20 on
 * the battle_getFlags() path; callers rely on this. */
Next battle_isTimeStopPauseOrBattleFlags0x20_800a0a4(void) {
    GUEST_CALL(battle_isTimeStop); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = 1;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isPaused);
    if (R0 == 0) {
        R0 = 1;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_getFlags);
    R1 = 0x20;
    R0 = (R0 & 0x20) != 0;
    RETURN_TO_CALLER();
}

/* ── clearBattleCounters_800A0C6 ── */
/* clearBattleCounters_800A0C6: clear the two 8-byte counter pairs at dword_2036820. */
Next clearBattleCounters_800A0C6(void) {
    ST32(A_dword_2036820, 0);
    ST32(A_dword_2036820 + 4, 0);
    ST32(A_dword_2036820 + 8, 0);
    ST32(A_dword_2036820 + 0xC, 0);
    RETURN_TO_CALLER();
}

/* ── input_recordStateChangeForEntry_800A0D6 ── */
/* input_recordStateChangeForEntry_800A0D6(r0 = entry index, r1 = new value):
 * Updates one 8-byte entry of the dword_2036820 array (entry size 8, so the
 * index is scaled by 3):
 *   +2: previous value, replaced by r1
 *   +4: bits that changed 0 -> 1 (newly set)
 *   +6: bits that changed 1 -> 0 (newly cleared)
 * Leaves r2 = newly cleared bits, r3 = entry address, and c/v from the
 * address computation (ADDS base + index*8). */
Next input_recordStateChangeForEntry_800A0D6(void) {
    u32 entry = A_dword_2036820 + (R0 << 3);
    SET_FLAGS_ADD(A_dword_2036820, R0 << 3);
    R3 = entry;

    u32 prev = LD16(entry + 2);
    ST16(entry + 2, R1);            /* store the new value */
    ST16(entry + 4, ~prev & R1);    /* bits newly set */
    prev &= ~R1;
    ST16(entry + 6, prev);          /* bits newly cleared */
    R2 = prev;

    RETURN_TO_CALLER();
}

/* ── battle_DecrementAllianceCounterSlot4_800A104 ── */
/* battle_DecrementAllianceCounterSlot4_800A104(r5 = battle object, r10 = toolkit):
 * Count down one of the alliance counters in the battle state (the slot
 * selected by this object's alliance + 4), but only while the AI data's
 * byte at offset oAIData_Unk_02 is zero. The decremented value is left
 * in r2; if the early-out is taken, r2 is left untouched. */
Next battle_DecrementAllianceCounterSlot4_800A104(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    if (LD8(aiData + 2 /* oAIData_Unk_02 */) == 0) {
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        u32 slot = LD8(R5 + oBattleObject_Alliance) + 4;

        u32 count = LD8(battleState + slot) - 1;
        ST8(battleState + slot, count);

        R2 = count;
    }

    RETURN_TO_CALLER();
}

/* ── relatedToIsBattleOver_800A11C ── */
/* relatedToIsBattleOver_800A11C(r5 = battle object, r10 = toolkit):
 * An actor has finished (or is leaving) the battle: decrement its alliance's
 * member-count byte in the battle state, then remove it from the list of
 * alive actors that still get their turn. */
Next relatedToIsBattleOver_800A11C(void) {
    u32 *battleState = (u32 *)LD32(R10 + oToolkit_BattleStatePtr);
    u32 countSlot = (u32)LD8(R5 + oBattleObject_Alliance) + oBattleState_Unk_12;

    /* One fewer member of this actor's alliance. */
    ST8((u32)battleState + countSlot, (u8)LD8((u32)battleState + countSlot) - 1);

    /* Drop this actor from every alive-actor slot it occupies. */
    u32 *slot = (u32 *)((u32)battleState + oBattleState_AliveBattleActors);
    u32 i;
    for (i = 0; i < 8; i++) {
        if (*slot == R5)
            *slot = 0;
        slot++;
    }

    R2 = i;                 /* loop counter ends at 8 */
    SET_FLAGS_CMP(i, 8);
    RETURN_TO_CALLER();
}

/* ── battle_getPhaseMode_800A152 ── */
/* battle_getPhaseMode_800A152: pick the current battle-mode id from the battle state.
 * Returns 0 while time is stopped; otherwise one of 1/2/7 depending on
 * which phase flags are set in the battle state:
 *   - flag at +4 clear: 2, or 1 once the flag at +0x0d is set
 *   - flag at +4 set, flag at +5 clear: 1, or 2 once +0x0d is set
 *   - flag at +4 set, flag at +5 set:   7 if flag at +0x0b is set, else 0
 * Also leaves the last-read state byte in r1 and the +0x0d byte in r2
 * for the caller. Flags come from the last TST of that byte (or from
 * battle_isTimeStop on the time-stop path). */
Next battle_getPhaseMode_800A152(void) {
    GUEST_CALL(battle_isTimeStop); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = 0;
        RETURN_TO_CALLER();
    }

    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u8 unk0d = LD8(battle + oBattleState_Unk_0d);
    u8 unk04 = LD8(battle + 4); /* oBattleState_Unk_04 */

    R2 = unk0d;
    R1 = unk04;

    if (unk04 == 0) {
        R0 = (unk0d != 0) ? 1 : 2;
        SET_FLAGS_NZ(unk0d);
    } else {
        u8 unk05 = LD8(battle + 5); /* oBattleState_Unk_05 */
        R1 = unk05;
        if (unk05 == 0) {
            R0 = (unk0d != 0) ? 2 : 1;
            SET_FLAGS_NZ(unk0d);
        } else {
            R1 = LD8(battle + oBattleState_Unk_0b);
            R0 = (R1 != 0) ? 7 : 0;
            SET_FLAGS_NZ(R1);
        }
    }
    RETURN_TO_CALLER();
}

/* ── battle_isBattleOver ── */
/* battle_isBattleOver: check whether the current battle should end.
 * Reads the battle state through the toolkit pointer in r10.
 * Returns r0 = 0 only when all end-of-battle conditions hold:
 * the two state flags (oBattleState_Unk_12/13) must be set and
 * oBattleState_Unk_0b must be clear; otherwise r0 = 1.
 * r3 = the battle state pointer; r1 = the last flag byte examined;
 * flags reflect the last test. */
Next battle_isBattleOver(void) {
    R0 = 1;

    R3 = LD32(R10 + oToolkit_BattleStatePtr);

    u32 flag12 = LD8(R3 + oBattleState_Unk_12);
    R1 = flag12;
    SET_FLAGS_NZ(flag12);
    if (flag12 == 0)
        RETURN_TO_CALLER();

    u32 flag13 = LD8(R3 + oBattleState_Unk_13);
    R1 = flag13;
    SET_FLAGS_NZ(flag13);
    if (flag13 == 0)
        RETURN_TO_CALLER();

    u32 flag0b = LD8(R3 + oBattleState_Unk_0b);
    R1 = flag0b;
    SET_FLAGS_NZ(flag0b);
    if (flag0b != 0)
        RETURN_TO_CALLER();

    /* All conditions met: battle is over. */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_setFlags ── */
/* battle_setFlags(r0 = flag bits):
 * Sets the given bits in the battle state's flags halfword
 * (BattleState + oBattleState_Unk_32), where the battle state is
 * taken from the toolkit (r10).
 * Leaves r1 = battle state pointer, r2 = new flags value. */
Next battle_setFlags(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    u32 flags = LD16(battle + oBattleState_Unk_32) | R0;
    ST16(battle + oBattleState_Unk_32, flags);

    R1 = battle;
    R2 = flags;
    RETURN_TO_CALLER();
}

/* ── battle_clearFlags ── */
/* battle_clearFlags(r0 = flag bits to clear):
 * Clears the given bits of the battle state's halfword flag field.
 * r10 = toolkit pointer; r1 = battle state pointer, r2 = new flags. */
Next battle_clearFlags(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 flags = LD16(battleState + oBattleState_Unk_32) & ~R0;

    ST16(battleState + oBattleState_Unk_32, flags);
    R1 = battleState;
    R2 = flags;
    RETURN_TO_CALLER();
}

/* ── battle_initTypeAndFieldFromSettings_800A2F8 ── */
/* battle_initTypeAndFieldFromSettings_800A2F8: initialize battle state fields at battle start.
 *
 * - Stores (navi stats halfword at 0x3E) / 100 into the navi stats byte at
 *   0x2B (via the BIOS divide call).
 * - Copies the battle type and battlefield from the current battle settings
 *   into the battle state, so the rest of the battle code can read them
 *   from one place.
 *
 * Outputs: r3 = battle state pointer; r2 and r4 are untouched. */
Next battle_initTypeAndFieldFromSettings_800A2F8(void) {
    u32 naviStats = LD32(R10 + oToolkit_NaviStatsPtr);

    R0 = LD16(naviStats + 0x3E);
    R1 = 100;
    swi_call(6); /* BIOS Div: R0 = R0 / R1, R1 = remainder */
    ST8(naviStats + 0x2B, R0);

    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u32 settings = LD32(battle + oBattleState_BattleSettings);

    ST8(battle + 6, LD8(settings + 0)); /* battlefield */
    ST8(battle + oBattleState_BattleType, LD8(settings + 3));

    R3 = battle; /* the original leaves the battle state pointer in r3 */
    RETURN_TO_CALLER();
}

/* ── pet_getCurNaviChainedLookup_800A4E0 ── */
/* pet_getCurNaviChainedLookup_800A4E0:
 * If the battle is in mode 1 (special/extra mode), report 0xFF and stop.
 * Otherwise look up a value for the navi currently equipped on the PET and
 * use it as an offset for a second lookup, returning whatever
 * battle_getNaviStatsByte_800A540 leaves in r0/r1. */
Next pet_getCurNaviChainedLookup_800A4E0(void) {
    GUEST_CALL(GetBattleMode);
    if (R0 == 1) {
        R0 = 0xFF;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* First lookup: base value for this navi. */
    R0 = navi;
    R1 = 0x2D;
    GUEST_CALL(battle_getNaviStatsByte_800A540);

    /* Second lookup: same navi, offset by the first result. */
    R0 = navi;
    R1 = 0x2E + R0;
    GUEST_CALL(battle_getNaviStatsByte_800A540);

    RETURN_TO_CALLER();
}

/* ── pet_getCurNaviStatusPair_800A506 ── */
/* pet_getCurNaviStatusPair_800A506: read the two status bytes of the navi currently in the PET.
 *
 * Each navi's save block is addressed through battle_getNaviStatsByte_800A540(navi, offset).
 * Byte 0x2D selects an entry, and the pair read here is the 16-bit value
 * stored at offsets 0x56 + 2*entry (low byte) and 0x57 + 2*entry (high byte).
 *
 * In battle mode 1 no navi data is available; both results come back as 0xFF.
 *
 * Returns r0 = low byte, r1 = high byte; flags reflect r1 (from the final
 * `mov r1, r0` of the original). */
Next pet_getCurNaviStatusPair_800A506(void) {
    GUEST_CALL(GetBattleMode);
    SET_FLAGS_CMP(R0, 1);
    if ((s32)R0 == 1) {
        R0 = 0xFF;
        R1 = 0xFF;
        SET_FLAGS_NZ(0xFF);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* entry selector for this navi; the pair lives at 0x56 + 2*entry */
    R0 = navi;
    R1 = 0x2D;
    GUEST_CALL(battle_getNaviStatsByte_800A540);
    u32 pairOffset = R0 << 1;

    R0 = navi;
    R1 = 0x56 + pairOffset;
    GUEST_CALL(battle_getNaviStatsByte_800A540);
    u32 low = R0;   /* the original parks this on the stack while fetching the high byte */

    R0 = navi;
    R1 = 0x57 + pairOffset;
    GUEST_CALL(battle_getNaviStatsByte_800A540);
    R1 = R0;                        /* mov r1, r0 == adds r1, r0, #0: sets nz, clears c and v */
    SET_FLAGS_ADD(R0, 0);
    R0 = low;

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

/* ── battle_getStateUnk40_800A704 ── */
/* battle_getStateUnk40_800A704: return the value stored at oBattleState_Unk_40 of the
 * current battle state (accessed through the r10 toolkit pointer). */
Next battle_getStateUnk40_800A704(void) {
    R0 = LD32(LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_Unk_40);
    RETURN_TO_CALLER();
}

/* ── getBattleSettingsFromList0 ── */
/* getBattleSettingsFromList0(r0 = index): return a pointer to entry `index`
 * (16 bytes each) of battleSettingsList0. Clobbers r1 like the original. */
Next getBattleSettingsFromList0(void) {
    R1 = (u32)R0 * 0x10;
    R0 = A_BattleSettings + R1;
    RETURN_TO_CALLER();
}

/* ── isSameSubsystem_800A732 ── */
/* isSameSubsystem_800A732: () -> !zf
 * Returns nonzero (Z clear) when the currently loaded subsystem counts as
 * "the same": either no subsystem is currently in use, or the subsystem
 * state byte at eStruct203F7D8+1 still has bit 1 set. On the path where the
 * subsystem is in use, r3 holds the struct address, r1 the state byte and
 * r2 the mask 2; otherwise those registers keep whatever
 * IsCurSubsystemInUse left in them. r4 is restored unchanged.
 *
 * Flag note: the original's final "mov r0, r4" is really an
 * "adds r0, r4, #0", which clears C and V; the following "tst r0, r0"
 * only refreshes N and Z. */
Next isSameSubsystem_800A732(void) {
    u32 same = 1; /* r4 = TRUE */

    GUEST_CALL(IsCurSubsystemInUse); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
        /* Subsystem is in use: "same" only while its state bit 1 is set. */
        R3 = A_eStruct203F7D8;
        R1 = LD8(A_eStruct203F7D8 + 1);
        R2 = 2;
        if (!(R1 & R2))
            same = 0; /* FALSE */
    }

    R0 = same;
    SET_FLAGS_ADD(same, 0); /* adds r0, r4, #0: N/Z from result, C = V = 0 */
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

/* ── sfx_playSfx0C_800A79C ── */
/* sfx_playSfx0C_800A79C: play sound effect #0x0C (battle_SetStateUnk5c_80013A2 is the sfx player). */
Next sfx_playSfx0C_800A79C(void) {
    R0 = 0xC;
    GUEST_CALL(battle_SetStateUnk5c_80013A2);
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

/* ── battle_getStateByte0d_800A7E2 ── */
/* battle_getStateByte0d_800A7E2: return the byte at oBattleState_Unk_0d of the current battle state
 * (r10 = toolkit). */
Next battle_getStateByte0d_800A7E2(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD8(battleState + oBattleState_Unk_0d);
    RETURN_TO_CALLER();
}

/* ── encryption_decryptPackTable30_800a7ea ── */
/* encryption_decryptPackTable30_800a7ea(r0 = address of a table of 30 packed halfwords):
 * Decrypt every entry of the table in place by feeding each halfword to
 * encryption_applyPack_8006e70. Each halfword is split into its low 9 bits
 * (passed in r0) and its high 7 bits (passed in r1), with r2 = 0.
 * The table pointer register (r0 on entry) and the caller's r4/r6 are
 * preserved; r0/r1/r2 are left holding the final split values, and the
 * flags come from the last loop-counter compare. */
Next encryption_decryptPackTable30_800a7ea(void) {
    u32 tableAddr = R0;
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 lastPayload = 0;
    u32 lastKey = 0;
    u32 entry;

    for (entry = 0; entry < 30; entry++) {
        u32 packed = LD16(tableAddr);
        u32 payload = packed & 0x1FF; /* low 9 bits */
        u32 key = packed >> 9;        /* high 7 bits */

        R0 = payload;
        R1 = key;
        R2 = 0;
        R4 = tableAddr; /* loop registers stay live across the call */
        R6 = entry;

        /* The original spills the payload around the call; keep the guest
           stack layout identical in case the callee looks at it. */
        SP -= 4;
        ST32(SP, payload);
        GUEST_CALL(encryption_applyPack_8006e70);
        SP += 4;

        lastPayload = payload;
        lastKey = key;
        tableAddr += 2;
    }

    /* The loop leaves the last split values in r0/r1 and r2 = 0, and the
       pop {r4,r6} restores the caller's registers before returning. */
    R0 = lastPayload;
    R1 = lastKey;
    R2 = 0;
    R4 = savedR4;
    R6 = savedR6;
    SET_FLAGS_CMP(entry, 30); /* final iteration compared counter (0x1E) vs 0x1E */
    RETURN_TO_CALLER();
}

/* ── battle_getStateByteNibbles_800A832 ── */
/* battle_getStateByteNibbles_800A832: read the battle state's Unk_1f byte and split it into nibbles.
 * Outputs: r0 = low nibble, r1 = high nibble, r2 = 0xF;
 * nz from the low nibble, c from bit 3 of the original byte (the LSR carry-out). */
Next battle_getStateByteNibbles_800A832(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 stateByte = LD8(battleState + oBattleState_Unk_1f);

    R1 = stateByte >> 4;
    R0 = stateByte & 0xF;
    R2 = 0xF;

    SET_FLAGS_NZ(R0);
    cpu.c = (stateByte >> 3) & 1;

    RETURN_TO_CALLER();
}

/* ── setTwoStructs_800A840 ── */
/* setTwoStructs_800A840(r0 = battle result byte, r10 = toolkit):
 * Writes the byte into the current battle state, then updates the battle
 * result via eStruct200A008_setBattleResult. Returns that function's r1. */
Next setTwoStructs_800A840(void) {
    u32 resultByte = R0;

    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    ST8(battleState + oBattleState_Unk_1f, resultByte);

    /* eStruct200A008_setBattleResult takes the same byte in r0 and reads
     * the toolkit from r10; both are already in place. Its result in r1
     * is passed through to the caller. */
    GUEST_CALL(eStruct200A008_setBattleResult);

    RETURN_TO_CALLER();
}

/* ── sub_800A86E ── */
/* sub_800A86E: scan the current battle state's 8 "alive battle actor" slots.
 * Every slot is read: r3 ends up holding the raw contents of the last slot
 * (0 if empty), r2 the AI-data pointer of the last occupied slot (or its
 * incoming value if none was occupied), and r0/r1 the unk_0e byte / HP of
 * that actor (HP is skipped when unk_0e is 0xFF). r4 and r7 are preserved;
 * the flags left behind are those of the final loop-counter compare (8 vs 8). */
Next sub_800A86E(void) {
    u32 slotPtr = LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_AliveBattleActors;
    u32 actor;
    u32 aiData = R2; /* r2 keeps its incoming value if no slot is occupied */

    for (u32 i = 0; i < 8; i++) {
        actor = LD32(slotPtr);
        if (actor != 0) {
            aiData = LD32(actor + oBattleObject_AIDataPtr);
            R0 = LD8(aiData + oAIData_Unk_0e);
            if (R0 != 0xFF)
                R1 = LD16(actor + oBattleObject_HP);
        }
        slotPtr += 4;
    }

    R2 = aiData;
    R3 = actor;             /* contents of the last slot, occupied or not */
    SET_FLAGS_CMP(8, 8);    /* cmp r4, #8 with r4 == 8 at loop exit */
    RETURN_TO_CALLER();
}

/* ── battle_tickCustomScreenEffect_800A892 ── */
/* battle_tickCustomScreenEffect_800A892:
 * Custom battle effect tick: if the active battle effects include the "custom
 * style" effect (0x100) but not the one at bit 3, and battle_getStateByteNibbles_800A832 reports
 * state 2, run the effect's main handler sub_8005DBE. */
Next battle_tickCustomScreenEffect_800A892(void) {
    u32 effects = 0;

    GUEST_CALL(GetBattleEffects);
    effects = R0;

    if ((effects & 0x100) != 0 && (effects & 8) == 0) {
        GUEST_CALL(battle_getStateByteNibbles_800A832);
        if (R0 == 2) {
            GUEST_CALL(sub_8005DBE);
        }
    }

    RETURN_TO_CALLER();
}

/* ── battle_testFlag0x40_800A8F8 ── */
/* battle_testFlag0x40_800A8F8: test battle flag 0x40 and return the result.
 * r0 = 1 if the flag is set, 0 if not; nz/c/v reflect the final `mov r0, r4`
 * (encoded as adds r0, r4, #0, so C and V are cleared). */
Next battle_testFlag0x40_800A8F8(void) {
    u32 result;

    GUEST_CALL(TestBattleFlag_0x40); /* answers in Z; the original branches on it */

    result = ZERO_FLAG() ? 0 : 1;
    R0 = result;
    SET_FLAGS_ADD(result, 0);

    RETURN_TO_CALLER();
}

/* ── sub_800A954 ── */
/* sub_800A954: run initIdListStruct_800A964 over each of the two work buffers at
 * byte_20349C0 and byte_2034A10. r2/r3/r4 pass through untouched. */
Next sub_800A954(void) {
    R0 = A_byte_20349C0;
    GUEST_CALL(initIdListStruct_800A964);

    R0 = A_byte_2034A10;
    GUEST_CALL(initIdListStruct_800A964);

    RETURN_TO_CALLER();
}

/* ── initIdListStruct_800A964 ── */
/* initIdListStruct_800A964(r0 = struct pointer):
 * Initialize a record list structure: clear the whole 0xC8-byte block to zero,
 * then mark the 12-byte ID array at offset 2 as empty (0xFF). */
Next initIdListStruct_800A964(void) {
    u32 mem = R0;
    u32 savedR4 = R4;

    /* ZeroFillByWord(mem, 0x50 words = 0xC8 bytes) */
    R1 = 0x50;
    GUEST_CALL(ZeroFillByWord);

    /* ByteFill(mem + 2, 12, 0xFF): every entry id starts as "unused" */
    R0 = mem + 2;
    R1 = 0xC;
    R2 = 0xFF;
    GUEST_CALL(ByteFill);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_IsBattleEnding_800A97A ── */
/* battle_IsBattleEnding_800A97A: check whether the battle is ending.
 * Returns 1 when GetBattleEffects reports the bit-3 effect *and* the battle
 * state's phase counter (byte 7) has reached 15; otherwise returns 0.
 * Flags on exit match the original's last comparison on each path. */
Next battle_IsBattleEnding_800A97A(void) {
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    R1 = 8;
    if (effects & 8) {
        /* Effect bit is set: consult the battle state's phase counter. */
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        R3 = battleState;
        u32 phase = LD8(battleState + 7);
        R1 = phase;

        SET_FLAGS_CMP(phase, 0xF);
        if ((s32)phase >= 0xF) {
            R0 = 1;
            RETURN_TO_CALLER();
        }
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* Effect bit clear: phase counter is never consulted. */
    SET_FLAGS_NZ(effects & 8);
    R0 = 0;
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

/* ── setBattleStateUnk11Flag_800A9CA ── */
/* setBattleStateUnk11Flag_800A9CA(r0 = flag bits, r10 = toolkit):
 * set the given bits in the battle state's Unk_11 flags byte.
 * r3 = pointer to the battle state struct. */
Next setBattleStateUnk11Flag_800A9CA(void) {
    u32 flags = R0;
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    ST8(battle + oBattleState_Unk_11,
        LD8(battle + oBattleState_Unk_11) | flags);

    R3 = battle;
    RETURN_TO_CALLER();
}

/* ── clearBattleStateUnk11Flag_800A9D6 ── */
/* clearBattleStateUnk11Flag_800A9D6(r0 = bit mask, r10 = toolkit):
 * clear the requested bits of the current battle state's Unk_11 flag byte.
 * Leaves r3 pointing at the battle state. */
Next clearBattleStateUnk11Flag_800A9D6(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 flags = LD8(battleState + oBattleState_Unk_11);

    flags &= ~R0;
    ST8(battleState + oBattleState_Unk_11, flags);

    R3 = battleState;
    RETURN_TO_CALLER();
}

/* ── battle_GetObjectByteAt14_800A9E2 ── */
/* battle_GetObjectByteAt14_800A9E2(r0 = battle object index, r10 = toolkit):
 * returns the byte at battleState + 0x14 + r0 (an object stat the caller
 * wants to inspect); r3 = battle state pointer, flags from the index add. */
Next battle_GetObjectByteAt14_800A9E2(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    R3 = battle;
    R0 += 0x14;
    SET_FLAGS_ADD(R0, 0x14);
    R0 = LD8(battle + R0);
    RETURN_TO_CALLER();
}

/* ── battle_networkInvert ── */
/* battle_networkInvert(r0 = value, r10 = toolkit):
 * XORs r0 with the byte at battleState->Unk_0d and returns the result in r0.
 * Also leaves the battle state pointer in r1, the toggled byte in r2, and the
 * NZ flags set from the result (the original's EOR is its last flag setter). */
Next battle_networkInvert(void) {
    u32 value = R0;

    R1 = LD32(R10 + oToolkit_BattleStatePtr);
    R2 = LD8(R1 + oBattleState_Unk_0d);
    R0 = value ^ R2;

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_clearEnemyFadeinList ── */
/* battle_clearEnemyFadeinList:
 * Reset the enemy fade-in list used during battle setup. Asks getNextEntryAddress_800AAAE
 * (called with r0 = 0) for the list's base address, then clears the first
 * 0x80 bytes of it. Returns with r1 = 0x80 (the size that was cleared). */
Next battle_clearEnemyFadeinList(void) {
    R0 = 0;
    GUEST_CALL(getNextEntryAddress_800AAAE);   /* r0 = base address of the fade-in list */

    R1 = 0x80;                 /* size in bytes */
    GUEST_CALL(ZeroFillByWord); /* zero-fill r1 bytes starting at r0 */

    RETURN_TO_CALLER();
}

/* ── sub_800AA06 ── */
/* sub_800AA06(r0 = handle passed through to getNextEntryAddress_800AAAE, r5 = expected id):
 * Asks getNextEntryAddress_800AAAE (called with r0 unchanged) for a pointer, then checks
 * whether the id field at that pointer matches r5. Returns r0 = 1 on match,
 * 0 otherwise (also left in r2, with flags from the move). */
Next sub_800AA06(void) {
    GUEST_CALL(getNextEntryAddress_800AAAE);

    u32 match = 0;
    if (LD32(R0) == R5)
        match = 1;

    R2 = match;
    R0 = match;
    SET_FLAGS_ADD(match, 0);
    RETURN_TO_CALLER();
}

/* ── list_tryAddUniqueEntry_800AA1A ── */
/* list_tryAddUniqueEntry_800AA1A(r5 = value to register):
 * Walk the 8-word list (0x20 bytes) whose address getNextEntryAddress_800AAAE()
 * returns and register the value in the first free (0) slot.
 * Returns r0 = 1 if the value was stored, r0 = 0 if it was already in the list
 * or the list was full. r1 ends up holding the last list word that was read. */
Next list_tryAddUniqueEntry_800AA1A(void) {
    GUEST_CALL(getNextEntryAddress_800AAAE);
    u32 list = R0;
    u32 value = R5;

    u32 offset = 0;
    for (;;) {
        u32 entry = LD32(list + offset);
        R1 = entry;

        if (entry == value) {
            /* already present: nothing to do */
            SET_FLAGS_CMP(entry, value);
            R0 = 0;
            RETURN_TO_CALLER();
        }

        if (entry == 0) {
            /* free slot: claim it for this value */
            SET_FLAGS_CMP(entry, 0);
            ST32(list + offset, value);
            R0 = 1;
            RETURN_TO_CALLER();
        }

        offset += 4;
        SET_FLAGS_CMP(offset, 0x20);
        if (offset == 0x20) {
            /* reached the end without finding it: list is full */
            R0 = 0;
            RETURN_TO_CALLER();
        }
    }
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

/* ── sub_800AA92 ── */
/* sub_800AA92: fetch a table pointer from getNextEntryAddress_800AAAE, then count how many
 * of its eight u32 entries are nonzero.
 * outputs: r0 = nonzero count, r1 = last word read, r2 = 0x20 (loop end),
 *          r3 = table pointer; c/v left as the final `cmp r2, #0x20` leaves
 *          them (c set, v clear). */
Next sub_800AA92(void) {
    GUEST_CALL(getNextEntryAddress_800AAAE);
    R3 = R0;

    u32 count = 0;
    for (u32 offset = 0; offset < 0x20; offset += 4) {
        R1 = LD32(R3 + offset);
        if (R1 != 0)
            count++;
    }

    R0 = count;
    R2 = 0x20;
    SET_FLAGS_CMP(R2, 0x20);
    RETURN_TO_CALLER();
}

/* ── getNextEntryAddress_800AAAE ── */
/* getNextEntryAddress_800AAAE(r0 = element index):
 * Computes the address of entry `r0` in the array at unk_2038170, whose
 * elements are 0x20 bytes each. Returns the address in r0 and the array
 * base in r1 (callers use r1 as the read/write base register). */
Next getNextEntryAddress_800AAAE(void) {
    u32 base = A_unk_2038170;

    R0 = base + R0 * 0x20;
    R1 = base;
    RETURN_TO_CALLER();
}

/* ── battle_stateGotoPhase14_800AABC ── */
/* battle_stateGotoPhase14_800AABC: advance the battle state into phase 0x14.
 * Runs setTwoStructs_800A840(4), then sets the battle state's phase index
 * to 0x14 and zeroes its halfword at offset 2. r5 is preserved (the original
 * pushes/pops it); r0 = battle state pointer, r1 = 0, flags as by `mov r1, #0`. */
Next battle_stateGotoPhase14_800AABC(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    R0 = 4;
    GUEST_CALL(setTwoStructs_800A840);

    battle = LD32(R10 + oToolkit_BattleStatePtr);
    ST8(battle + 1, 0x14);   /* oBattleState_Index_01 */
    ST16(battle + 2, 0);     /* oBattleState_Unk_02_03 */

    R0 = battle;
    R1 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_800AAF2 ── */
/* sub_800AAF2:
 * Passes 0 in r0 to clearIndexEntryPair_800AAFC and tail-returns its results
 * (r2/r3/r4 flow straight out of the callee). */
Next sub_800AAF2(void) {
    R0 = 0;
    GUEST_CALL(clearIndexEntryPair_800AAFC);
    RETURN_TO_CALLER();
}

/* ── clearIndexEntryPair_800AAFC ── */
/* clearIndexEntryPair_800AAFC(r0 = entry index):
 * Fills two 40-word (0x28) arrays with 0xFFFFFFFF, one entry into each.
 * The first array lives at unk_20018C0, the second at unk_2000260; both are
 * laid out as 0x28-byte records selected by the index in r0.
 * Caller's r3 and r4 are preserved; r2 is left at 0xFFFFFFFF.
 */
Next clearIndexEntryPair_800AAFC(void) {
    u32 index = R0;
    u32 callerR4 = R4;

    /* Clear entry `index` of the first table. */
    R0 = A_unk_20018C0 + index * 0x28;
    R1 = 0x28;
    R2 = 0xFFFFFFFF;
    GUEST_CALL(WordFill);

    /* Clear the matching entry of the second table. */
    R0 = A_unk_2000260 + index * 0x28;
    R1 = 0x28;
    R2 = 0xFFFFFFFF;
    GUEST_CALL(WordFill);

    /* Restore the caller's r4 (the tail of `push {r4,lr}` / `pop {r4,pc}`). */
    R4 = callerR4;
    RETURN_TO_CALLER();
}

/* ── clearWorkArea_2000FA0_800AB22 ── */
/* clearWorkArea_2000FA0_800AB22: clear word_2000FA0 (0x1A halfwords, i.e. 52 bytes).
 * R2/R3 pass through untouched; flags come from ZeroFillByHalfword. */
Next clearWorkArea_2000FA0_800AB22(void) {
    R0 = A_word_2000FA0;
    R1 = 0x1A;
    GUEST_CALL(ZeroFillByHalfword);
    RETURN_TO_CALLER();
}

/* ── table_GetByte16Row_203EAE0_800AB3A ── */
/* table_GetByte16Row_203EAE0_800AB3A(r0 = row, r1 = column):
 * read one byte from a 16-byte-per-row table at byte_203EAE0.
 * Returns the byte in r0 and the table address of that byte in r3. */
Next table_GetByte16Row_203EAE0_800AB3A(void) {
    u32 row = R0, column = R1;
    u32 entry = A_byte_203EAE0 + row * 0x10;
    R0 = LD8(entry + column);
    R3 = entry;
    RETURN_TO_CALLER();
}

/* ── tableAddSaturatedByte_800AB46 ── */
/* tableAddSaturatedByte_800AB46(r0 = row, r1 = column, r2 = addend):
 * Adds r2 to one byte of the table at byte_203EAE0 (16-byte rows selected
 * by r0, byte within the row by r1), saturating the result at 0xFF.
 * Sets c/v from the CMP against 0xFF and leaves the row base address in r3. */
Next tableAddSaturatedByte_800AB46(void) {
    u32 entry = A_byte_203EAE0 + R0 * 0x10;

    u32 sum = LD8(entry + R1) + R2;
    SET_FLAGS_CMP(sum, 0xFF);
    if ((s32)sum > 0xFF)
        sum = 0xFF;

    ST8(entry + R1, sum);
    R3 = entry;
    RETURN_TO_CALLER();
}

/* ── zeroFill_800AB70 ── */
/* zeroFill_800AB70: clear the 0x20-byte block at byte_203EAE0.
 * r2/r3 are passed through untouched. */
Next zeroFill_800AB70(void) {
    R0 = A_byte_203EAE0;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── battle_updatePhaseState_800AE0C ── */
/* battle_updatePhaseState_800AE0C: advance the battle's phase state machine.
 *
 * - If the current phase (Unk_1c) has moved past the previously recorded one
 *   (Unk_1b), record the new phase and, once it reaches 2 or more, kick off
 *   the setup routine for that phase (sub_801E228) unless the current battle
 *   mode is 6, which handles its own setup.
 * - Then count the phase timer (Unk_1d) down; when it expires, reset the
 *   phase back to 0.
 */
Next battle_updatePhaseState_800AE0C(void) {
    u32 savedR4 = R4;
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u32 phase = LD8(battle + oBattleState_Unk_1c);
    u32 prevPhase = LD8(battle + oBattleState_Unk_1b);

    SET_FLAGS_CMP(phase, prevPhase);
    if ((s32)phase > (s32)prevPhase) {
        ST8(battle + oBattleState_Unk_1b, phase);
        SET_FLAGS_CMP(phase, 2);
        if ((s32)phase >= 2) {
            GUEST_CALL(GetBattleMode);
            SET_FLAGS_CMP(R0, 6);
            if (R0 != 6) {
                /* hand the new phase to the setup routine; it expects the
                 * battle state in r4 */
                R0 = phase - 2;
                R4 = battle;
                GUEST_CALL(sub_801E228);
                R4 = savedR4;
            }
        }
    }

    {
        u32 phaseTimer = LD8(battle + oBattleState_Unk_1d);
        SET_FLAGS_NZ(phaseTimer);
        if (phaseTimer != 0) {
            ST8(battle + oBattleState_Unk_1d, phaseTimer - 1);
        } else {
            ST8(battle + oBattleState_Unk_1c, 0);
        }
    }

    RETURN_TO_CALLER();
}

/* ── battle_bumpTimerAndReloadSubcount_800AE44 ── */
/* battle_bumpTimerAndReloadSubcount_800AE44: advance the battle timer — bump the byte at Unk_1c and
 * reload the timeout counter at Unk_1d with 10. r10 = toolkit pointer. */
Next battle_bumpTimerAndReloadSubcount_800AE44(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);

    ST8(battleState + oBattleState_Unk_1c, LD8(battleState + oBattleState_Unk_1c) + 1);
    ST8(battleState + oBattleState_Unk_1d, 10);

    /* output: r1 = battle state pointer (the original keeps r1 live) */
    R1 = battleState;
    RETURN_TO_CALLER();
}

/* ── handleVariableDamageChip_800AEE8 ── */
/* Per-battler chip records rooted at byte_20349C0: the player's block comes
 * first and the opponent's follows 0x50 bytes later.  Each block starts with
 * a selector byte that picks one entry; the entry holds the chip id as a u16
 * and, at +0xC, the damage value currently associated with it. */
#define BATTLER_RECORD_STRIDE    0x50
#define ENTRY_DAMAGE_OFFSET      0x0C
#define CHIPFLAG_VARIABLE_DAMAGE 0x80

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_handleVariableDamageChip_800AEE8(u32 which_chip) {
    R0 = which_chip;
    GUEST_CALL(getChip8021DA8);
    /* The original materializes the 0x80 flag-test mask in r2 immediately
     * after each fetch.  When the chip turns out not to be variable-damage
     * that value is what survives to the callers (r2 is an output), so keep
     * r2 in step here; on the damage-call path sub_80109A4 overwrites it,
     * exactly as in the original. */
    R2 = CHIPFLAG_VARIABLE_DAMAGE;
    return R0;
}

/* register form: calls sub_80109A4 outside this file, which has no typed C interface yet */
static u32 variable_chip_damage(u32 which_chip, u32 player) {
    R0 = which_chip;
    R1 = player;
    GUEST_CALL(sub_80109A4);
    /* R2 is deliberately left alone: the outgoing r2 on this path is whatever
     * the callee left, matching the original's r2 after the bl. */
    return R0;
}

/* Refresh the damage of the selected chip in both battlers' records
 * (0 = player, 1 = opponent).  Only chips whose data carries the
 * variable-damage flag (bit 7 of byte 9 of the chip data) are reworked;
 * every other chip keeps its stored damage. */
static void refresh_variable_damage_chips(void) {
    u32 player;
    for (player = 0; player < 2; player++) {
        u32 record = A_byte_20349C0 + player * BATTLER_RECORD_STRIDE;
        u32 entry = record + 2 + LD8(record) * 2;
        u32 chip_id = LD16(entry);
        u32 chip = get_chip_data_handleVariableDamageChip_800AEE8(chip_id);

        if (LD8(chip + 9) & CHIPFLAG_VARIABLE_DAMAGE)
            ST16(entry + ENTRY_DAMAGE_OFFSET,
                 variable_chip_damage(chip_id, player));
    }
}

/* handleVariableDamageChip_800AEE8: refresh variable-damage chip damage for
 * both battlers.  Every outgoing value is produced exactly where the
 * original leaves it: r2 comes back from the guest calls above (0x80 when
 * the second chip is not variable-damage, the callee's value otherwise),
 * r3 and r4-r12 pass through untouched, and the outgoing carry/overflow
 * flags are the ones the last guest call left behind (the original's final
 * TST only touches n/z) - so the adapter has nothing to write back itself. */
Next handleVariableDamageChip_800AEE8(void) {
    refresh_variable_damage_chips();
    RETURN_TO_CALLER();
}

/* ── sub_800AF84 ── */
/* sub_800AF84: choose a battle-mode pair and hand it to sub_800AC20.
 *
 *   If battle effect bit 3 (0x8) is clear:
 *     mode = effect bit 0 (0 or 1), arg1 = 0, r3 = 1, r2 = raw effects value.
 *   If bit 3 is set:
 *     mode = 2, arg1 = battleState->oBattleState_Unk_0d with its low bit
 *     flipped, unless battle_getStateByteNibbles_800A832 returned 1.
 *
 * r4-r7 and r10 pass through untouched; everything else is whatever
 * sub_800AC20 leaves behind. */
Next sub_800AF84(void) {
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 8) {
        GUEST_CALL(battle_getStateByteNibbles_800A832);
        u32 nibble = R0;

        u32 statePtr = LD32(R10 + oToolkit_BattleStatePtr);
        u32 stateByte = LD8(statePtr + oBattleState_Unk_0d);

        if (nibble != 1) {
            R2 = 1;
            stateByte ^= R2;
        }
        R1 = stateByte;
        R0 = 2;
    } else {
        R0 = (effects & 1) != 0;
        R1 = 0;
        R2 = effects;
        R3 = 1;
    }

    GUEST_CALL(sub_800AC20);
    RETURN_TO_CALLER();
}

/* ── encryption_applyPackList_800B110 ── */
/* encryption_applyPackList_800B110: walk the halfword table at word_800B128 (pack indices,
 * 0-terminated) and re-apply each encryption pack through
 * encryption_applyPack_8006e70. Nothing else in the table region is touched.
 * Preserves r1-r7 like the original (r4 is pushed/popped there). */
Next encryption_applyPackList_800B110(void) {
    u32 savedR4 = R4;

    for (u32 entry = A_word_800B128;; entry += 2) {
        u16 packId = LD16(entry);
        if (packId == 0)
            break;
        R0 = packId;
        /* the original holds the table cursor in r4 across the call */
        R4 = entry;
        GUEST_CALL(encryption_applyPack_8006e70);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_backupStateForRestart_800B3A2 ── */
/* battle_backupStateForRestart_800B3A2: back up the battle state before a restart/continue.
 *
 * Copies the 0x50-word battle work buffer and the current Navi's stats
 * block (0x64 words) into their backup buffers, writes a "data valid"
 * marker word, snapshots a small 0x10-word block, and finally invokes
 * sub_80200A4(0x32) to finish the snapshot.
 *
 * Inputs:  r3 (unused), r10 = toolkit pointer, lr = return address.
 * Outputs: r2/r3 and the c/v flags as left by the final sub_80200A4 call. */
Next battle_backupStateForRestart_800B3A2(void) {
    /* Back up the 0x50-word battle work area. */
    R0 = A_byte_20366C0;
    R1 = A_dword_203CBE4;
    R2 = 0x50;
    GUEST_CALL(CopyWords);

    /* Mark the backup as valid. */
    ST32(A_dword_203CBE0, 0x56789123);

    /* Copy the current battle Navi's stats block (0x64 words) to its backup. */
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD8(battleState + 0xD);
    GUEST_CALL(GetBattleNaviStatsAddr); /* R0 = NaviStats of the active Navi */

    R1 = A_byte_203CC34;
    R2 = 0x64;
    GUEST_CALL(CopyWords);

    /* Back up the 0x10-word block (bag/inventory snapshot). */
    R0 = A_byte_203CED0;
    R1 = A_byte_203CC98;
    R2 = 0x10;
    GUEST_CALL(CopyWords);

    /* Run the snapshot post-processing step. */
    R0 = 0x32;
    GUEST_CALL(sub_80200A4);

    RETURN_TO_CALLER();
}

/* ── sub_800B428 ── */
/* sub_800B428:
 * Reinitializes the record whose pointer lives at dword_203CBE4 (via sub_802C8FA),
 * writes the marker word 0x1F2F3F4F to dword_203CBE0, then invokes sub_80200A4
 * with mode 4. sub_80200A4's r1-r3 results pass straight through to our caller. */
Next sub_800B428(void) {
    /* Reset/clear the structure at dword_203CBE4. */
    R0 = A_dword_203CBE4;
    GUEST_CALL(sub_802C8FA);

    /* Stamp the magic value into dword_203CBE0. */
    ST32(A_dword_203CBE0, 0x1F2F3F4F);

    /* Continue with operation mode 4; r1-r3 are left exactly as the callee set them. */
    R0 = 4;
    GUEST_CALL(sub_80200A4);

    RETURN_TO_CALLER();
}

/* ── sub_800B444 ── */
/* sub_800B444: reload the two saved 12-byte blocks into the working battle
 * data area (byte_203F4A4 -> unk_2035260 and byte_203F5A4 -> unk_203526C),
 * then run battle_syncEffectSlotBetweenBuffers_802C9EA to act on the restored data. */
Next sub_800B444(void) {
    /* first block: 3 words */
    R0 = A_byte_203F4A4;
    R1 = A_unk_2035260;
    R2 = 0xC;
    GUEST_CALL(CopyWords);

    /* second block: 3 words */
    R0 = A_byte_203F5A4;
    R1 = A_unk_203526C;
    R2 = 0xC;
    GUEST_CALL(CopyWords);

    GUEST_CALL(battle_syncEffectSlotBetweenBuffers_802C9EA);

    RETURN_TO_CALLER();
}

/* ── sub_800B460 ── */
/* sub_800B460: zero the 32-bit values at 0x203F4A0 and 0x203F5A0. */
Next sub_800B460(void) {
    ST32(A_dword_203F4A0, 0);
    ST32(A_dword_203F5A0, 0);
    RETURN_TO_CALLER();
}

/* ── battle_doBothTrackedIdsMatch_800B46C ── */
/* battle_doBothTrackedIdsMatch_800B46C(r0 = id): r0 = 1 if both tracked ids (dword_203F4A0 and
 * dword_203F5A0) equal the given id, 0 otherwise.
 * Also hands the loaded values back in r1/r2 and the id in r3; flags are
 * left from the last comparison performed, so callers may branch on them. */
Next battle_doBothTrackedIdsMatch_800B46C(void) {
    u32 id = R0;
    u32 idA = LD32(A_dword_203F4A0);
    u32 idB = LD32(A_dword_203F5A0);

    R3 = id;
    R1 = idA;
    R2 = idB;

    if (idA != id) {
        R0 = 0;
        SET_FLAGS_CMP(idA, id);
    } else if (idB != id) {
        R0 = 0;
        SET_FLAGS_CMP(idB, id);
    } else {
        R0 = 1;
        SET_FLAGS_CMP(idB, id);
    }
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setBattleResult ── */
/* eStruct200A008_setBattleResult(r0 = battle result):
 * Store the battle result byte at byte_200A009 (one byte past
 * eOWPlayerObjectEnd), leaving the field's base address in r1. */
Next eStruct200A008_setBattleResult(void) {
    R1 = A_eOWPlayerObject_SpriteData_Size;
    ST8(R1 + 1, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getBattleResult ── */
/* eStruct200A008_getBattleResult: return the battle-result flag stored at
 * byte_200A009 (one byte past the start of the OW player object table).
 * Outputs: r0 = flag value, r1 = eOWPlayerObjectEnd (0x200A008). */
Next eStruct200A008_getBattleResult(void) {
    R1 = A_eOWPlayerObject_SpriteData_Size;   /* literal pool value: eOWPlayerObjectEnd */
    R0 = LD8(R1 + 1);                         /* byte_200A009 */
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk00 ── */
/* eStruct200A008_setUnk00(r0 = value):
 * stores a single byte at the eOWPlayerObject_SpriteData_Size location. */
Next eStruct200A008_setUnk00(void) {
    ST8(A_eOWPlayerObject_SpriteData_Size, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk02 ── */
/* eStruct200A008_setUnk02(r0 = value):
 * Stores r0 into the byte at offset 2 of the struct at 0x200A008
 * (byte_200A00A), leaving the struct base address in r1. */
Next eStruct200A008_setUnk02(void) {
    u32 value = R0;

    R1 = A_eOWPlayerObject_SpriteData_Size;
    ST8(R1 + 2, value);

    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk04 ── */
/* eStruct200A008_setUnk04(r0 = value):
 * Write a 16-bit value to the field at offset +4 of the struct based at
 * eOWPlayerObject_SpriteData_Size (i.e. word_200A00C). Used as a simple
 * setter for that field; nothing else is touched. */
Next eStruct200A008_setUnk04(void) {
    u32 structBase = A_eOWPlayerObject_SpriteData_Size; /* 0x200A008 */
    ST16(structBase + 4, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getUnk04 ── */
/* eStruct200A008_getUnk04: return the u16 field at 0x200A00C (the player's
 * overworld object struct's "Unk04" field). Result in r0. */
Next eStruct200A008_getUnk04(void) {
    R0 = LD16(A_eOWPlayerObject_SpriteData_Size + 4);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk06 ── */
/* eStruct200A008_setUnk06(r0 = value):
 * Store r0 as a halfword at word_200A00E — the +0x6 field of the
 * struct whose base is the literal eOWPlayerObjectEnd. */
Next eStruct200A008_setUnk06(void) {
    ST16(A_eOWPlayerObject_SpriteData_Size + 6, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getUnk06 ── */
/* eStruct200A008_getUnk06:
 * read the halfword field at offset 6 of the struct at eOWPlayerObjectEnd
 * (word_200A00E) and return it in r0. */
#define word_200A00E (A_eOWPlayerObject_SpriteData_Size + 6)

Next eStruct200A008_getUnk06(void) {
    R0 = LD16(word_200A00E);
    RETURN_TO_CALLER();
}

#undef word_200A00E

/* ── eStruct200A008_setUnk03 ── */
/* eStruct200A008_setUnk03(r0 = value):
 * Store the low byte of r0 into the byte just past the end of the overworld
 * player object (byte_200A00B, the "unk03" field of struct 200A008).
 * Leaves r1 = end-of-player-object address (200A008), as the original does. */
Next eStruct200A008_setUnk03(void) {
    u32 base = A_eOWPlayerObject_SpriteData_Size;

    ST8(base + 3, R0);
    R1 = base;

    RETURN_TO_CALLER();
}

/* ── sub_800B75A ── */
/* sub_800B75A: store 0xFF (a "cancel/back out" flag?) into byte_203C960. */
Next sub_800B75A(void) {
    ST8(A_byte_203C960, 0xFF);
    RETURN_TO_CALLER();
}

/* ── sub_800B79A ── */
/* sub_800B79A: dispatch the command id in r0 to one of five handlers.
 * Valid ids are 0x11E..0x122, mapping to table entries 0..4; anything else
 * is ignored. The selected handler is invoked with its own table entry in
 * r0 (the original does this by loading the pointer and doing `bx r0`),
 * and when it returns, control goes back to this function's caller.
 * Trashes r0, r1, r6, r7 and the flags; passes everything else through. */
Next sub_800B79A(void) {
    s32 id = (s32)R0;

    R1 = 0x122;
    if (id > 0x122)
        RETURN_TO_CALLER();

    R1 = 0x11E;
    if (id < 0x11E)
        RETURN_TO_CALLER();

    u32 index = (u32)(id - 0x11E);

    /* Handlers see their own table entry in r0, exactly as the original
     * leaves it after `ldr r0, [r1,r0]`. */
    R0 = LD32(A_off_800B7BC + index * 4);

    switch (index) {
    case 0: GUEST_CALL(sub_800B7D0); break;
    case 1: GUEST_CALL(sub_800B7E8); break;
    case 2: GUEST_CALL(sub_800B800); break;
    case 3: GUEST_CALL(nullsub_2);   break;
    case 4: GUEST_CALL(sub_800B80E); break;
    }

    RETURN_TO_CALLER();
}
