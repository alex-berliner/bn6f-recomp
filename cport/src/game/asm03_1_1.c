/*
 * asm03_1_1.c: readable functions of bn6f asm03_1_1.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_803C3E0 ── */
/* sub_803C3E0: test the flag byte at word_2006770+2 and return it.
 * r0 = flag byte; Z set when it is zero, N set when bit 7 is set. */
Next sub_803C3E0(void) {
    u32 flag = LD8(A_word_2006770 + 2);
    R0 = flag;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── keyItems_ResetInventoryAndGrantScenarioItems_803CD74 ── */
/* keyItems_ResetInventoryAndGrantScenarioItems_803CD74: wipe the key-item inventory block (0x190 bytes) and re-grant the
 * starting key items: four copies of item 0x75 and one of item 0.
 * R10 is the toolkit pointer. R0-R2 and flags are clobbered; R4-R7 and up are
 * left untouched, matching the original (it saves/restores R4-R7 itself). */
Next keyItems_ResetInventoryAndGrantScenarioItems_803CD74(void) {
    u32 keyItems = LD32(R10 + oToolkit_KeyItemsPtr);

    R0 = keyItems;
    R1 = 0x190; /* sizeof key-item inventory */
    GUEST_CALL(ZeroFillByWord);

    R0 = 0x75;
    R1 = 4;
    GUEST_CALL(keyItems_RemoveFromInventoryAndGive_803CDF8);

    R0 = 0;
    R1 = 1;
    GUEST_CALL(keyItems_RemoveFromInventoryAndGive_803CDF8);

    RETURN_TO_CALLER();
}

/* ── keyItems_RemoveFromInventoryAndGive_803CDF8 ── */
/* keyItems_RemoveFromInventoryAndGive_803CDF8(r0 = key item slot index, r1 = passed through to GiveItem):
 * mark the given key item slot as empty (not owned) in the player's key
 * item inventory, then hand the item to the player via GiveItem.
 * Leaves r0..r3 as GiveItem left them. */
Next keyItems_RemoveFromInventoryAndGive_803CDF8(void) {
    u32 keyItems = LD32(R10 + oToolkit_KeyItemsPtr);

    ST8(keyItems + R0, 0);

    R2 = keyItems;
    R3 = 0;
    GUEST_CALL(GiveItem);

    RETURN_TO_CALLER();
}

/* ── SetCurNaviHPToFull ── */
/* SetCurNaviHPToFull: set the current PET navi's HP to its maximum.
 * Leaves r0 = 0 and Z set (N clear), matching the original's final "mov r0, #0". */
Next SetCurNaviHPToFull(void) {
    /* Which navi is currently in the PET? */
    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* Read its max HP... */
    R0 = navi;
    R1 = oNaviStats_MaxHP;
    GUEST_CALL(GetCurPETNaviStatsHword);
    u32 maxHP = R0;

    /* ...and store it as the current HP. */
    R0 = navi;
    R1 = oNaviStats_CurHP;
    R2 = maxHP;
    GUEST_CALL(SetCurPETNaviStatsHword);

    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── petnavi_setStat42ToStat3EPlusCounter_803CED4 ── */
/* petnavi_setStat42ToStat3EPlusCounter_803CED4:
 * Reads the current PET navi's stat 0x3E, adds the 16-bit value stored at
 * (*R10->oToolkit_Unk2004334_Ptr), and writes the sum into stat 0x42 of the
 * same navi.
 *
 * Register contract:
 *   inputs:    r4 r6 r7 r10 lr
 *   outputs:   r2 (the computed sum); r3 as left by SetCurPETNaviStatsHword
 *   preserved: r4 r6 r7 (saved/restored around the helper calls)
 *   clobbers:  r0 r1 and flags
 */
Next petnavi_setStat42ToStat3EPlusCounter_803CED4(void) {
    /* Original pushes r4/r6/r7 and pops them after the helper calls,
     * so their incoming values must survive everything below. */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Which navi is currently equipped on the PET. */
    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* Read stat 0x3E of that navi. */
    R0 = navi;
    R1 = 0x3E; /* stat id to read */
    GUEST_CALL(GetCurPETNaviStatsHword);
    u32 baseStat = R0;

    /* Add the 16-bit value tracked by the toolkit's oToolkit_Unk2004334_Ptr. */
    u32 extra = LD16(LD32(R10 + oToolkit_Unk2004334_Ptr));
    u32 total = baseStat + extra;

    /* Store the sum into stat 0x42. */
    R0 = navi;
    R1 = 0x42; /* stat id to write */
    R2 = total;
    GUEST_CALL(SetCurPETNaviStatsHword);

    /* Original leaves r2 = sum and restores r4/r6/r7 from the stack. */
    R2 = total;
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── pet_addCurNaviStat0x3ECapped_803CF3C ── */
/* pet_addCurNaviStat0x3ECapped_803CF3C(r1 = navi slot index; expects r4 == 0x70):
 * Adjusts one PET navi stat word: reads stat 0x3E of navi slot 0, adds
 * r1 * 20 (the per-slot stride), clamps the result to 1000, stores it back,
 * and calls sub_803CF74 to apply the same value/offset pair elsewhere.
 * All registers r4-r12 are preserved. */
Next pet_addCurNaviStat0x3ECapped_803CF3C(void) {
    u32 r4_in = R4, r5_in = R5, r6_in = R6, r7_in = R7;

    if (R4 != 0x70)
        RETURN_TO_CALLER();

    u32 slot_offset = 0x14 * R1; /* byte offset of the navi's own entry */

    /* read the current stat word (navi 0, stat 0x3E) */
    R0 = 0;
    R1 = 0x3E;
    R4 = 0;
    R6 = r6_in;
    R7 = slot_offset;
    GUEST_CALL(GetCurPETNaviStatsHword);

    u32 value = R0 + slot_offset;
    if ((s32)value > 0x3E8)
        value = 0x3E8; /* cap at 1000 */

    /* write the adjusted value back */
    R0 = 0;
    R1 = 0x3E;
    R2 = value;
    R4 = 0;
    R6 = value;
    R7 = slot_offset;
    GUEST_CALL(SetCurPETNaviStatsHword);

    /* propagate the new value (r0) and slot offset (r1) */
    R0 = value;
    R1 = slot_offset;
    R4 = 0;
    R5 = r5_in;
    R6 = value;
    R7 = slot_offset;
    GUEST_CALL(sub_803CF74);

    R4 = r4_in;
    R5 = r5_in;
    R6 = r6_in;
    R7 = r7_in;
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

/* ── logoScreen_finish_803D2A6 ── */
/* logoScreen_finish_803D2A6(self: r5 = *const LogoScreenState):
 * Called when the logo screen finishes: switch the LCD back to the normal
 * render mode (0x40) and reinitialize the shared screen-start state.
 * Preserves r2/r3; returns with r0/r1 and flags as the last callee left them. */
Next logoScreen_finish_803D2A6(void) {
    R0 = 0x40;
    GUEST_CALL(SetRenderInfoLCDControl);
    GUEST_CALL(startScreen_init_802F530);
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

/* ── sub_803DE5C ── */
/* sub_803DE5C: set unk_200A284 (a boolean flag) to 1. */
Next sub_803DE5C(void) {
    ST32(A_unk_200A284, 1);
    RETURN_TO_CALLER();
}

/* ── eLinkState_ResetForNewUse_803DE88 ── */
/* eLinkState_ResetForNewUse_803DE88(r0 = state pointer):
 * Reset the link/camera state block for a new use: clear the eLinkState
 * struct, its 0x10-byte auxiliary buffer and a 0x40-byte work area, then
 * store the given pointer at eLinkState+0x58 (dword_2009A28).
 * Flags and R1 are whatever the final ZeroFillByHalfword call leaves;
 * r2-r7 are untouched. */
Next eLinkState_ResetForNewUse_803DE88(void) {
    u32 statePtr = R0;

    R0 = A_eCameraEnd;
    R1 = 0x58;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = A_byte_200DD10;
    R1 = 0x10;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = A_unk_2009480;
    R1 = 0x40;
    GUEST_CALL(ZeroFillByHalfword);

    ST32(A_eCameraEnd + 0x58, statePtr);

    RETURN_TO_CALLER();
}

/* ── sub_803DEAC ── */
/* sub_803DEAC: tail-call wrapper that simply invokes sub_8144240.
 * r4-r7 are preserved (sub_8144240 keeps them intact). */
Next sub_803DEAC(void) {
    GUEST_CALL(sub_8144240);
    RETURN_TO_CALLER();
}

/* ── init_eStartScreenAnimationControl200B1A0_1 ── */
/* init_eStartScreenAnimationControl200B1A0_1:
 * Initialize the 8-byte e-StartScreen animation control block and mark it
 * active (state 0xB4). Preserves r2/r4; leaves r0 = 0xB4 with flags set. */
Next init_eStartScreenAnimationControl200B1A0_1(void) {
    u32 ctrl = A_eStartScreenAnimationControl200B1A0;

    R0 = ctrl;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);

    R0 = 0xB4;
    SET_FLAGS_NZ(R0);
    ST8(ctrl + 3, R0);
    RETURN_TO_CALLER();
}

/* ── init_eStartScreenAnimationControl200B1A0_2 ── */
/* init_eStartScreenAnimationControl200B1A0_2:
 * Reset the 8-byte extra start-screen animation control struct at
 * eStartScreenAnimationControl200B1A0 (zero fill) and enable it by
 * setting its active flag at offset 3 to 1.
 * r2 and r4 are preserved; flags are left as after `movs r0, #1`. */
Next init_eStartScreenAnimationControl200B1A0_2(void) {
    u32 ctrl = A_eStartScreenAnimationControl200B1A0;

    R0 = ctrl;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);

    R0 = 1;
    SET_FLAGS_NZ(1);
    ST8(ctrl + 3, 1);

    RETURN_TO_CALLER();
}

/* ── startScreen_IsTitleAnimationFinished_803E928 ── */
/* startScreen_IsTitleAnimationFinished_803E928:
 * Check the start screen animation's current state. Loads the animation
 * control byte and compares it against 2 (animation finished).
 * Outputs: R0 = state byte, flags set from `R0 - 2` (Z set when state == 2).
 */
Next startScreen_IsTitleAnimationFinished_803E928(void) {
    R0 = LD8(A_eStartScreenAnimationControl200B1A0);
    SET_FLAGS_CMP(R0, 2);
    RETURN_TO_CALLER();
}

/* ── startScreen_TstZero ── */
/* startScreen_TstZero: always clear the flags as "zero" (Z set, N clear).
 * Despite the name, it unconditionally tests 0 against itself. */
Next startScreen_TstZero(void) {
    R0 = 0;
    SET_FLAGS_NZ(0); /* tst r0, r0 with r0 == 0 */
    RETURN_TO_CALLER();
}

/* ── startScreen_DecrementPressStartPauseAnimationTimer ── */
/* startScreen_DecrementPressStartPauseAnimationTimer (0803E964):
 * Tick down the pause timer (byte at eStartScreenAnimationControl200B1A0+3)
 * that holds the "PRESS START" screen on its current animation frame. When the
 * timer reaches zero (i.e. it was 1), byte 0 of the animation control is set
 * to 1 so the animation advances, and the timer value reported to callers is 1.
 * Outputs: r0 = raw decremented timer, r6/r7 pass through unchanged, and the
 * flags are those of the "sub r0,#1" (or of "mov r0,#1" on the restart path). */
Next startScreen_DecrementPressStartPauseAnimationTimer(void) {
    u32 ctrl = A_eStartScreenAnimationControl200B1A0;

    u32 timer = LD8(ctrl + 3);

    /* timer--; the flags of this subtraction tell callers whether it hit zero */
    SET_FLAGS_CMP(timer, 1);
    timer -= 1;
    ST8(ctrl + 3, timer);

    if (timer != 0) {
        R0 = timer;
        RETURN_TO_CALLER();
    }

    /* timer expired: flag the animation control to move on, timer reads as 1 */
    ST8(ctrl, 1);
    SET_FLAGS_NZ(1); /* mov r0,#1 */
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── runPendingWorkIfFlagged_803EA1C ── */
/* runPendingWorkIfFlagged_803EA1C: service the pending work flagged by byte_200AD04.
 * If notZero_eByte200AD04 reports the flag byte is non-zero, run
 * sub_803F560 to process it; otherwise do nothing. Flags and R0-R3
 * are whatever the (last) callee leaves behind. */
Next runPendingWorkIfFlagged_803EA1C(void) {
    GUEST_CALL(notZero_eByte200AD04);
    if (R0 != 0)
        GUEST_CALL(sub_803F560);
    RETURN_TO_CALLER();
}

/* ── net_isLinkConnected_803EA2C ── */
/* net_isLinkConnected_803EA2C: check the link state flag.
 * r0 = 1 if the byte at eLinkState is 0xC (link connection fully established),
 * otherwise r0 = 0. Flags (nzcv) left as the CMP leaves them. */
Next net_isLinkConnected_803EA2C(void) {
    R0 = 0;
    R1 = LD8(A_eCameraEnd); /* literal pool points at eLinkState */
    SET_FLAGS_CMP(R1, 0xC);
    if (R1 == 0xC)
        R0 = 1;
    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_getJumpOffset00 ── */
/* eStruct200BC30_getJumpOffset00:
 * Returns the jump offset stored at the head of the eStruct200BC30 record.
 * r1 = pointer to the record, r0 = its first byte. */
Next eStruct200BC30_getJumpOffset00(void) {
    R1 = A_eStruct200BC30;
    R0 = LD8(R1);
    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_getJumpOffset00_pushpop ── */
/* eStruct200BC30_getJumpOffset00_pushpop: thin trampoline that tail-calls
 * eStruct200BC30_getJumpOffset00. Results come back in r0/r1 unchanged. */
Next eStruct200BC30_getJumpOffset00_pushpop(void) {
    GUEST_CALL(eStruct200BC30_getJumpOffset00);
    RETURN_TO_CALLER();
}

/* ── clearStruct200BC30_803EBAC ── */
/* clearStruct200BC30_803EBAC: clear the 0x20-byte work struct at 0x200BC30 (eStruct200BC30).
 * Leaves the struct zeroed word-by-word via ZeroFillByWord; r2/r3 pass
 * through untouched, r1 ends as ZeroFillByWord leaves it. */
Next clearStruct200BC30_803EBAC(void) {
    R0 = A_eStruct200BC30;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── resetAndInitStruct200BC30_803EBC8 ── */
/* resetAndInitStruct200BC30_803EBC8: reset the state used by sub_803EF60's subsystem.
 * Resets the list head at dword_200B1B0 (via sub_803EF84), zeroes the
 * 0x20-byte struct at eStruct200BC30, and then (re)initializes it. */
Next resetAndInitStruct200BC30_803EBC8(void) {
    R0 = A_dword_200B1B0;
    GUEST_CALL(sub_803EF84);

    R0 = A_eStruct200BC30;
    R1 = 0x20; /* struct size in bytes */
    GUEST_CALL(ZeroFillByWord);

    GUEST_CALL(sub_803EF60);

    RETURN_TO_CALLER();
}

/* ── sub_803EF60 ── */
/* sub_803EF60: reset some state, then zero-fill the buffer sub_803EF7C
 * returned a pointer to (0x100 bytes, filled eight words at a time). */
Next sub_803EF60(void) {
    GUEST_CALL(sub_803EF7C);   /* returns buffer address in r0 */
    R1 = 0x100;
    GUEST_CALL(ZeroFillByEightWords); /* ZeroFillByEightWords(r0, 0x100) */
    RETURN_TO_CALLER();
}

/* ── sub_803EF7C ── */
/* sub_803EF7C: return eCamera.unk_84 (the camera's end value). */
Next sub_803EF7C(void) {
    R0 = LD32(A_eCameraEnd + 0x34);
    RETURN_TO_CALLER();
}

/* ── sub_803EF84 ── */
/* sub_803EF84(r0 = value):
 * Store r0 into the link-state camera field at eLinkState+0x34, which the
 * disassembly also refers to as eCamera.unk_84 (the eLinkState symbol sits at
 * the same address as A_eCameraEnd, hence the alias in the original literal). */
Next sub_803EF84(void) {
    ST32(A_eCameraEnd + 0x34, R0);
    RETURN_TO_CALLER();
}

/* ── clear_e200AD04 ── */
/* clear_e200AD04: zero the 4 bytes at word_200AD04 via ZeroFillByByte.
 * Preserves r2; clobbers r0/r1. */
Next clear_e200AD04(void) {
    R0 = A_word_200AD04;
    R1 = 4;
    GUEST_CALL(ZeroFillByByte);
    RETURN_TO_CALLER();
}

/* ── setByte_200AD04_803F4EC ── */
/* setByte_200AD04_803F4EC(r0 = value): store the low byte at word_200AD04.
 * Also leaves the address of word_200AD04 in r1. */
Next setByte_200AD04_803F4EC(void) {
    u32 *ptr = (u32 *)A_word_200AD04;

    ST8((u32)ptr, R0);
    R1 = (u32)ptr;

    RETURN_TO_CALLER();
}

/* ── sub_803F500 ── */
/* sub_803F500:
 * Store 0xB4 into word_200AD06 (likely resetting some counter/timer). */
Next sub_803F500(void) {
    ST16(A_word_200AD04 + 2, 0xB4);
    RETURN_TO_CALLER();
}

/* ── notZero_eByte200AD04 ── */
/* notZero_eByte200AD04:
 * Test the event flag byte at word_200AD04 for zero.
 * Outputs: r0 = the byte, r1 = its address, nz flags set from the byte. */
Next notZero_eByte200AD04(void) {
    R1 = A_word_200AD04;
    R0 = LD8(R1);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── tickCountdownGate_803F530 ── */
/* tickCountdownGate_803F530: countdown gate attached to the flag byte at byte_200AD04.
 *
 * While the flag byte is set, consults sub_803F560 each tick; when it
 * approves, decrements the halfword counter at 200AD06. The flag byte is
 * rewritten (via setByte_200AD04_803F4EC) either when the counter reaches
 * zero or when sub_803F560 vetoes immediately. On return, the Z flag
 * reports whether the countdown is still active (r6 == 1 internally means
 * "still counting"); r4/r6 themselves are callee-saved and restored. */
Next tickCountdownGate_803F530(void) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;

    R6 = 0;
    GUEST_CALL(notZero_eByte200AD04);

    if (R0 != 0) {                      /* flag byte at 200AD04 is set */
        R4 = 1;
        GUEST_CALL(sub_803F560); /* answers in Z; the original branches on it */

        if (!ZERO_FLAG()) {
            /* Approved: count one step down. */
            R4 = 0;
            R6 = 1;
            R1 = A_word_200AD04;
            R0 = LD16(R1 + 2);          /* counter at word_200AD06 */
            SET_FLAGS_CMP(R0, 1);       /* subs r0, #1 */
            R0 -= 1;
            ST16(R1 + 2, R0);
            if (R0 == 0) {
                /* Counter finished: clear the flag byte. */
                R6 = 0;
                R0 = R4;
                GUEST_CALL(setByte_200AD04_803F4EC);
            }
            /* else: still counting, skip the flag write. */
        } else {
            /* Vetoed: write the flag byte right away. */
            R0 = R4;
            GUEST_CALL(setByte_200AD04_803F4EC);
        }
    }
    /* Flag byte clear: nothing to do this tick. */

    /* tst r6, r6: Z clear while the countdown is still running. */
    SET_FLAGS_NZ(R6);

    R4 = saved_r4;                      /* pop {r4,r6} */
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── sub_803F798 ── */
/* sub_803F798: shared entry stub for a multi-entry routine.
 * It stores r1-r7 and lr on the stack exactly like `push {r1-r7,lr}`
 * (low word = r1, high word = lr), zeroes r4 as the loop counter seed,
 * and then continues into the shared body at loc_803F7A2, whose
 * epilogue pops these values back (pc = saved lr returns to the caller). */
Next sub_803F798(void) {
    u32 frame = SP - 32;
    SP = frame;

    ST32(frame + 0x00, R1);
    ST32(frame + 0x04, R2);
    ST32(frame + 0x08, R3);
    ST32(frame + 0x0C, R4);
    ST32(frame + 0x10, R5);
    ST32(frame + 0x14, R6);
    ST32(frame + 0x18, R7);
    ST32(frame + 0x1C, LR);

    R4 = 0;

    /* tail jump: loc_803F7A2 pops {r1-r7,pc} at its end */
    GUEST_CALL(loc_803F7A2);
    RETURN_TO_CALLER();
}

/* ── s2001c04_ComputeChecksumOfEwramRegion_803F894 ── */
/* s2001c04_ComputeChecksumOfEwramRegion_803F894: compute a checksum over the 16 bytes at EWRAM
 * 0x2000000 + 0x6700..0x670F, add 24, and store it into the
 * S2001c04 work area at offset 0x68. The field is briefly zeroed
 * first, as in the original. All registers except r0 are preserved. */
Next s2001c04_ComputeChecksumOfEwramRegion_803F894(void) {
    u32 work = LD32(R10 + oToolkit_S2001c04_Ptr);

    ST32(work + 0x68, 0);

    u32 sum = 0;
    for (s32 i = 0x670F; i >= 0; i--)
        sum += LD8(A_ewram_2000000 + i);

    sum += 0x18;
    ST32(work + 0x68, sum);

    R0 = sum;
    RETURN_TO_CALLER();
}

/* ── info_copyTimestampToWorkArea_803F8B2 ── */
/* info_copyTimestampToWorkArea_803F8B2:
 * Copy the 20-byte InfoText timestamp into the work area at
 * [toolkit->S2001c04] + 0x6C. R1/R2/R3 are left as CopyBytes leaves them
 * (callers read their post-call values). */
Next info_copyTimestampToWorkArea_803F8B2(void) {
    R0 = A_EXE6_InfoText_timestamp;              /* src: timestamp buffer */
    R1 = LD32(R10 + oToolkit_S2001c04_Ptr) + 0x6C; /* dest: work area + 0x6C */
    R2 = 0x14;                                   /* 20 bytes */
    GUEST_CALL(CopyBytes);
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

/* ── infoText_IsTimestampUnchanged_803F8F4 ── */
/* infoText_IsTimestampUnchanged_803F8F4: check whether the info-text timestamp is unchanged.
 *
 * Compares the 20-byte timestamp stored in the save buffer
 * (toolkit->S2001c04 + 0x6C) against the master copy at
 * EXE6_InfoText_timestamp. Returns r0 = 0 if all 20 bytes match
 * (the on-screen text is still current), r0 = 1 if any byte differs
 * (the text must be refreshed).
 * Flags: nz reflects the returned value. */
Next infoText_IsTimestampUnchanged_803F8F4(void) {
    u32 master = A_EXE6_InfoText_timestamp;                  /* reference timestamp */
    u32 saved  = LD32(R10 + oToolkit_S2001c04_Ptr) + 0x6C;   /* copy in the save buffer */

    s32 i;
    for (i = 0x13; i >= 0; i--) {
        if (LD8(saved + i) != LD8(master + i)) {
            R0 = 1;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }
    }

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── save_BackupToGamePakWithRetry_803FAB4 ── */
/* save_BackupToGamePakWithRetry_803FAB4: back up the current save block to the game pak, retrying up
 * to three times.  Each attempt refills the RNG seed buffer, copies the
 * 0x80-byte block at dword_20067C0 to the game pak (0xE007F80) and verifies
 * the copy.  Returns r0 = 1 if at least one attempt verified OK, r0 = 0 if
 * all three failed. */
Next save_BackupToGamePakWithRetry_803FAB4(void) {
    const u32 entryR4 = R4;
    const u32 entryR6 = R6;
    const u32 entryR7 = R7;
    u32 failures = 0;
    u32 lastR3 = 0;

    for (u32 attempt = 0; attempt < 3; attempt++) {
        /* Refresh the entropy pool the save seed is drawn from. */
        R4 = entryR4;
        R6 = failures;
        R7 = attempt;
        GUEST_CALL(rng_FillSeedBuffer_803FAEE);

        /* Copy the 0x80-byte save block into the game pak backup area. */
        R0 = A_dword_20067C0;
        R1 = 0xE007F80;
        R2 = 0x80;
        GUEST_CALL(libSave_CopyToGamePak);
        lastR3 = R3;

        /* Verify the copy; a zero result marks the attempt as failed. */
        R0 = A_dword_20067C0;
        R1 = 0xE007F80;
        R2 = 0x80;
        R7 = attempt;
        GUEST_CALL(sub_814D9C4);
        R7 = attempt; /* sub_814D9C4 may clobber r7 */
        if (R0 == 0)
            failures++;
    }

    /* Restore the caller's registers and leave the loop's last state in
     * r2/r3, as the original's pops do. */
    R6 = entryR6;
    R7 = entryR7;
    R2 = 0x80;
    R3 = lastR3;

    if (failures < 3) {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── rng_FillSeedBuffer_803FAEE ── */
/* rng_FillSeedBuffer_803FAEE: fill the 0x80-byte buffer at dword_20067C0 with random 32-bit
 * words drawn from the hardware RNG, one GetRNG call per word.
 * r4-r7 are preserved (the original saves/restores them around the loop). */
Next rng_FillSeedBuffer_803FAEE(void) {
    enum { RNG_FILL_SIZE = 0x80 };  /* bytes to fill (literal =0x80) */

    u32 dst = A_dword_20067C0;

    for (u32 off = 0; off < RNG_FILL_SIZE; off += 4) {
        GUEST_CALL(GetRNG);
        ST32(dst + off, R0);
    }

    RETURN_TO_CALLER();
}

/* ── save_CommitWorkBufferToGamePak_803FB04 ── */
/* save_CommitWorkBufferToGamePak_803FB04(r0 = save work buffer value):
 * Store r0 into the save work buffer at dword_20067C0, then copy 0x80 bytes
 * from that buffer to the Game Pak save area at 0xE007F80.
 * The original pushes/pops r0-r7, so every register is preserved. */
Next save_CommitWorkBufferToGamePak_803FB04(void) {
    u32 inR0 = R0, inR1 = R1, inR2 = R2, inR3 = R3;
    u32 inR4 = R4, inR5 = R5, inR6 = R6, inR7 = R7;

    /* Remember the value being saved, in the save work buffer. */
    ST32(A_dword_20067C0, inR0);

    /* libSave_CopyToGamePak(src = dword_20067C0, dest = 0xE007F80, len = 0x80).
     * The callee also expects the source pointer in r4. */
    R0 = A_dword_20067C0;
    R1 = 0xE007F80;
    R2 = 0x80;
    R4 = A_dword_20067C0;
    GUEST_CALL(libSave_CopyToGamePak);

    /* Restore the caller's registers (the original's pop {r0-r7,pc}). */
    R0 = inR0; R1 = inR1; R2 = inR2; R3 = inR3;
    R4 = inR4; R5 = inR5; R6 = inR6; R7 = inR7;
    RETURN_TO_CALLER();
}
