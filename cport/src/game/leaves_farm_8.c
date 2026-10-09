/*
 * leaves_farm_8.c: small functions from the model farm, set 8.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

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

/* ── sub_81345E8 ── */
/* sub_81345E8: spawn/queue helper that forwards a fixed argument set to
 * sub_813468C. The original pushes r4-r7 and pops them afterwards, so this
 * wrapper preserves r4-r7 even though the callee may clobber them; only
 * r1-r3 (the callee's results) escape to the caller. */
Next sub_81345E8(void) {
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

/* ── owPlayer_unlockPlayerAfterNonNPCDialogue_809E122 ── */
/* owPlayer_unlockPlayerAfterNonNPCDialogue_809E122:
 * Called after talking to an overworld character that is not an NPC-scripted
 * interaction: unlock the player again. Clears the "dialogue in progress"
 * byte of eStruct200ace0, lets the player move again via the event-flag
 * system, and clears story event flag 0x1719. */
#define EVENT_1719 0x1719

Next owPlayer_unlockPlayerAfterNonNPCDialogue_809E122(void) {
    /* Clear the dialogue-busy byte at eStruct200ace0. */
    ST8(A_eStruct200ace0, 0);

    /* Re-enable player movement. */
    GUEST_CALL(SetPlayerCanMoveEventFlag);

    /* Clear event flag 0x1719 (ClearEventFlagFromImmediate takes the flag
     * number split into a high byte in r0 and a low byte in r1). */
    R0 = EVENT_1719 >> 8;
    R1 = EVENT_1719 & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── sub_809E14C ── */
/* sub_809E14C(r0 = fixed-animation id):
 * Called when the overworld player is told to play a fixed animation.
 * sub_809E462 decides whether that animation can take effect immediately:
 *   - if it returns 0 (Z set), the id is written to the fixed-animation
 *     select byte at eStruct200ace0+2 (byte_200ACE2) and the event flag
 *     EVENT_1719 (bank 0x17, flag 0x19) is raised;
 *   - otherwise the request is deferred by parking the id in the byte at
 *     eStruct200ace0+0x1c (byte_200ACFC).
 * r4-r7 are preserved (the id is passed to sub_809E462 in r4/r7's struct
 * pointer, but both are restored before returning). */
Next sub_809E14C(void) {
    u32 anim = R0;
    u32 saved_r4 = R4;
    u32 saved_r7 = R7;

    R4 = anim;                  /* mov r4, r0: sub_809E462 reads r4 */
    R7 = A_eStruct200ace0;      /* ldr r7, =eStruct200ace0 */

    /* sub_809E462 reports in r0 whether the animation can be applied now. */
    GUEST_CALL(sub_809E462);

    if (R0 == 0) {
        /* Approved: select the animation right away and flag the event. */
        ST8(A_eStruct200ace0 + 2, R4);  /* owPlayerFixedAnimationSelect */

        /* movflag EVENT_1719 -> SetEventFlagFromImmediate(0x17, 0x19) */
        R0 = 0x17;
        R1 = 0x19;
        GUEST_CALL(SetEventFlagFromImmediate);
    } else {
        /* Denied: remember the requested animation for later. */
        ST8(A_eStruct200ace0 + oS200ace0_Unk_1c, R4);
    }

    /* pop {r4-r7,pc} */
    R4 = saved_r4;
    R7 = saved_r7;
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

/* ── sub_803EBC8 ── */
/* sub_803EBC8: reset the state used by sub_803EF60's subsystem.
 * Resets the list head at dword_200B1B0 (via sub_803EF84), zeroes the
 * 0x20-byte struct at eStruct200BC30, and then (re)initializes it. */
Next sub_803EBC8(void) {
    R0 = A_dword_200B1B0;
    GUEST_CALL(sub_803EF84);

    R0 = A_eStruct200BC30;
    R1 = 0x20; /* struct size in bytes */
    GUEST_CALL(ZeroFillByWord);

    GUEST_CALL(sub_803EF60);

    RETURN_TO_CALLER();
}

/* ── object_canMove ── */
/* object_canMove (0800E5E2): asks whether the current object is allowed to move.
 *
 * Calls object_getFlag (which reads the object pointer from r4) and tests the
 * returned flag word against the "action blocked" mask 0x5040. Returns
 * r0 = 1 when nothing blocks the object, r0 = 0 when one of the blocking bits
 * is set. r4 is pushed/popped around the call, so it comes out unchanged;
 * r1 is left holding the mask, and the NZ flags reflect the returned boolean. */
Next object_canMove(void) {
    /* push {r4,lr}: the object pointer in r4 must survive the call */
    u32 savedR4 = R4;

    /* r0 = the object's flag word */
    GUEST_CALL(object_getFlag);

    /* mov r4, r0 assembles to the flag-setting adds r4, r0, #0 form
     * (clears C and V); the TST that follows doesn't touch them. */
    SET_FLAGS_ADD(R0, 0);
    R4 = R0;

    /* flag bits that forbid the object from moving/acting */
    R1 = 0x5040;
    u32 blocked = R4 & R1;

    /* pop {r4,pc}: restores the caller's r4 */
    R4 = savedR4;

    if (blocked != 0) {
        R0 = 0; /* a blocking flag is set: the object cannot move */
    } else {
        R0 = 1; /* nothing blocks it: free to move */
    }
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── object_checkPanelParameters ── */
/* object_checkPanelParameters(r0, r1 = panel coordinates, r2 = flag mask that
 * must all be set on the panel, r3 = flag mask that must be clear):
 * queries the panel at the given coordinates via object_getPanelParameters and
 * returns r0 = 1 if the panel exists, has none of the "must be clear" flags,
 * and has every "must be set" flag set; otherwise returns r0 = 0.
 * r4 and r5 are preserved. The masks are moved into r4/r5 before the call, as
 * the callee expects them there.
 * Flags: the exits are `mov r0, #1/#0`, which in Thumb set N/Z from the
 * immediate (N = 0, Z = result == 0). C/V on the first two failure paths are
 * still whatever object_getPanelParameters left behind; on the third check
 * (and its failure path) they come from `cmp r0, r4`. */
Next object_checkPanelParameters(void) {
    u32 mustBeSet = R2;
    u32 mustBeClear = R3;
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    R4 = mustBeSet;
    R5 = mustBeClear;

    GUEST_CALL(object_getPanelParameters); /* r0 = panel flags at (r0, r1) */
    u32 panelFlags = R0;

    u32 result;
    if (panelFlags == 0) {
        /* Panel doesn't exist (tst r0, r0 failed). */
        result = 0;
    } else if ((panelFlags & mustBeClear) != 0) {
        /* Panel has a flag that must be clear (tst r0, r5 failed). */
        result = 0;
    } else {
        /* and r0, r4; cmp r0, r4: the panel must carry every "must be set"
         * flag. This comparison sets C/V on both the success and the failure
         * exit of this path; since (flags & mask) <= mask, carry is set
         * exactly when they are equal, i.e. on success. */
        u32 setFlags = panelFlags & mustBeSet;
        SET_FLAGS_CMP(setFlags, mustBeSet);
        result = (setFlags == mustBeSet) ? 1 : 0;
    }

    /* mov r0, #0/#1 sets N and Z from the result value itself. */
    R0 = result;
    SET_FLAGS_NZ(R0);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── object_setPanelAlliance ── */
/* object_setPanelAlliance(r0 = object, r1 = param, r2 = alliance):
 * Claim the panel the given battle object stands on for `alliance`: look up
 * the object's panel data, and if the panel is in use (panel type byte != 0),
 * store the new alliance id in it, clear its halfword field at
 * oPanelData_Unk_10, and refresh the panel's parameters.
 * Leaves r4-r7 untouched; r3 carries the panel type (early exit) or whatever
 * object_updatePanelParameters leaves in it. */
Next object_setPanelAlliance(void) {
    u32 object = R0;
    u32 param = R1;
    u32 alliance = R2;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    /* The panel helpers receive the three arguments both in r0-r2 and,
     * following this module's calling convention, in r4-r6. */
    R4 = object;
    R5 = param;
    R6 = alliance;

    GUEST_CALL(object_getPanelDataOffset);
    u32 panel = R0;

    u32 type = LD8(panel + 2); /* oPanelData_Type */
    R3 = type;
    if (type != 0) {
        ST8(panel + 3, alliance); /* oPanelData_Alliance */
        ST16(panel + oPanelData_Unk_10, 0);

        R0 = object;
        R1 = param;
        R4 = object;
        R5 = param;
        R6 = alliance;
        GUEST_CALL(object_updatePanelParameters);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── sub_800D618 ── */
/* sub_800D618(r0 = object, r1 = passed through to object_getPanelDataOffset,
 *             r2 = value the panel must match, r4 = passed through):
 * Looks up the panel data of the panel the object stands on and returns 1
 * in r0 only when the panel's byte at offset 4 differs from the panel's
 * alliance byte (offset 3) and equals r2. Returns 0 when there is no panel
 * data or when either check fails. Flags (c/v) on the "no panel data" exit
 * are whatever object_getPanelDataOffset left behind, as in the original. */
Next sub_800D618(void) {
    u32 savedR4 = R4;                 /* r4 is preserved for the caller */
    u32 expected = R2;

    R4 = expected;                    /* object_getPanelDataOffset expects r2 there */
    GUEST_CALL(object_getPanelDataOffset);

    if (R0 == 0) {
        /* No panel data: r0 stays 0, r1/r2 are as the callee left them. */
        R4 = savedR4;
        RETURN_TO_CALLER();
    }

    R1 = LD8(R0 + 4);                 /* oPanelData_Unk_04 */
    R2 = LD8(R0 + 3);                 /* oPanelData_Alliance */

    if (R1 == R2) {
        /* The panel byte matches the panel's alliance: not a match. */
        R0 = 0;
        SET_FLAGS_CMP(R1, R2);
    } else {
        /* Match only when the panel byte equals the requested value. */
        R0 = (R1 == expected);
        SET_FLAGS_CMP(R1, expected);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_814E09C ── */
/* sub_814E09C: convert/copy the 36-word table at off_81C0418 into the buffer
 * whose address arrives in r0. Each word is fetched and then passed through
 * sub_814E0B6 (destination pointer in r0, source address in r2, word value in
 * r3; the converted word comes back in r3) before being stored. Like the
 * original, r12 is left holding the return address. */
Next sub_814E09C(void) {
    u32 dest = R0;
    u32 ret = LR;
    u32 src = A_off_81C0418;

    for (s32 count = 0x24; count > 0; count--) {
        u32 word = LD32(src);

        R0 = dest;
        R1 = (u32)count;
        R2 = src;
        R3 = word;
        R12 = ret;
        GUEST_CALL(sub_814E0B6);

        ST32(dest, R3);
        dest += 4;
        src += 4;
    }

    R12 = ret;
    RETURN_TO_CALLER();
}

/* ── sub_814E21C ── */
/* sub_814E21C:
 * Calls sub_814E0D0 (which fills r1 with a record pointer and r3 with the
 * record's current/new state value), then syncs the state byte at [r1+0x18]:
 * if it differs from r3, the new value is written back and the low nibble of
 * the byte at [r1] is set to 0xF (marking the record as "needs refresh").
 * Flags are left as the last executed flag-setting instruction leaves them:
 * CMP r0, r3 on the equal path, ORR on the update path. */
Next sub_814E21C(void) {
    R12 = LR;
    GUEST_CALL(sub_814E0D0);

    R0 = LD8(R1 + 0x18);
    SET_FLAGS_CMP(R0, R3);
    if (R0 != R3) {
        ST8(R1 + 0x18, R3);
        R2 = 0xF;
        R3 = LD8(R1) | R2;
        ST8(R1, R3);
        SET_FLAGS_NZ(R3);
    }
    RETURN_TO_CALLER();
}

/* ── sub_813B768 ── */
/* sub_813B768:
 * Reset the tool's state: clear the halfword behind the toolkit's
 * Unk2004334 pointer and zero 0x10 bytes behind its Unk200431c pointer.
 * r2 is passed through unchanged. */
Next sub_813B768(void) {
    u32 *toolkit = (u32 *)R10;

    ST16(LD32((u32)toolkit + oToolkit_Unk2004334_Ptr), 0);

    R0 = LD32((u32)toolkit + oToolkit_Unk200431c_Ptr);
    R1 = 0x10;
    GUEST_CALL(ZeroFillByByte);

    RETURN_TO_CALLER();
}

/* ── sub_813C490 ── */
/* sub_813C490: read stat byte 0x1F of the PET's current navi and report
 * whether it equals 1 (some cross-menu "navi is set" check).
 * Returns the check result (0 or 1) in r0; r4 is preserved. */
Next sub_813C490(void) {
    u32 result = 0;

    R0 = 0;     /* a1 = stats offset base (unused by the callee) */
    R1 = 0x1F;  /* a2 = byte index to read */
    GUEST_CALL(GetCurPETNaviStatsByte);

    if (R0 == 1)
        result = 1;

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── zeroFill_813B934 ── */
/* zeroFill_813B934:
 * Clear the scratch work buffers hanging off the toolkit struct that r10 points
 * to: the 0x188-byte block at oToolkit_Unk2004190_Ptr and the 0x40-byte block
 * at oToolkit_Unk200414c_Ptr. r2/r3 and r10 are preserved. */
Next zeroFill_813B934(void) {
    u32 toolkit = R10;

    R0 = LD32(toolkit + oToolkit_Unk2004190_Ptr);
    R1 = 0x188;
    GUEST_CALL(ZeroFillByWord);

    R0 = LD32(toolkit + oToolkit_Unk200414c_Ptr);
    R1 = 0x40;
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── zeroFill_813D960 ── */
/* zeroFill_813D960: clear the 16-byte work area at eS200BC50, but preserve the
 * single byte at offset 7 (byte_200BC57), which must survive the wipe.
 * r2-r4 and r7 are untouched; r0 is scratch and r1 passes through from
 * ZeroFillByWord, as do the C/V flags from its last flag-setting instruction. */
Next zeroFill_813D960(void) {
    u32 savedByte;

    R0 = A_eS200BC50_Index00;
    savedByte = LD8(R0 + 7);

    R1 = 0x10;
    GUEST_CALL(ZeroFillByWord);

    ST8(A_eS200BC50_Index00 + 7, savedByte);
    RETURN_TO_CALLER();
}

/* ── doPETEffect_8033fc0 ── */
/* doPETEffect_8033fc0(r0 = PET effect index):
 * Records the requested PET effect in the eS2011E30 state block and
 * clears its pending-effect byte. When the effect index is 3, also
 * plays the associated sound effect (0x9B).
 * Non-sound path: r0 = effect index, r1 = 0, flags from `cmp r0, #3`.
 * Sound path: r0/r1 and flags are whatever PlaySoundEffect leaves. */
Next doPETEffect_8033fc0(void) {
    u32 effect = R0;

    R1 = 0;
    ST8(A_eS2011E30 + 7, R1);      /* clear pending-effect flag */
    ST8(A_eS2011E30 + 4, effect);  /* store new effect index */

    if (effect == 3) {
        R0 = 0x9B;
        GUEST_CALL(PlaySoundEffect);
    } else {
        R0 = effect;
        SET_FLAGS_CMP(effect, 3);
    }
    RETURN_TO_CALLER();
}

/* ── onUpdateInRealWorld_8033A96 ── */
/* onUpdateInRealWorld_8033A96(self: *S2011E30 in r5):
 * Per-frame update while in the real world: clear two per-frame state
 * flags, run the real-world update logic, then queue a graphics transfer. */
Next onUpdateInRealWorld_8033A96(void) {
    R0 = 0;
    GUEST_CALL(sub_8033B80);

    R0 = 0;
    GUEST_CALL(sub_8033F80);

    GUEST_CALL(sub_8033BE8);

    R0 = 0;
    GUEST_CALL(gfxTransfer_8033DA0);

    RETURN_TO_CALLER();
}

/* ── sub_80341F8 ── */
/* sub_80341F8: prepare the transmission screen.
 * r5 = pointer to a state byte that is set to 4; also switches the LCD into a
 * fixed render mode, refreshes render info, and starts the transmission song.
 * Flag outputs are whatever the final PlayMusic call leaves behind. */
Next sub_80341F8(void) {
    ST8(R5, 4);

    R0 = 0x40; /* LCD control render info */
    GUEST_CALL(SetRenderInfoLCDControl);

    GUEST_CALL(renderInfo_8001788);
    GUEST_CALL(renderInfo_80017A0);

    R0 = SONG_TRANSMISSION;
    GUEST_CALL(PlayMusic);

    RETURN_TO_CALLER();
}

/* ── chatbox_80419B4 ── */
/* chatbox_80419B4(r4 = chatbox work area, r2/r5/r10 = caller context):
 * Start the chatbox script belonging to the currently equipped PET navi.
 * Reads the navi's script ID from r4[navi + 2]; 0xFF means "no script for
 * this navi", in which case r4 is just advanced past the table by 0xE.
 * Otherwise hands the script ID to the chatbox config loader (which ignores
 * its first argument; r0 still holds the navi number from GetCurPETNavi).
 * Always returns r0 = 1. */
Next chatbox_80419B4(void) {
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

/* ── reqBBS_813E616 ── */
/* reqBBS_813E616:
 * Reset the BBS (bulletin board) work areas: fill the board buffer at
 * 0x2001400 with the unused-entry marker 0x40 and clear the two 0x20-byte
 * entry tables at 0x2000FC0 and 0x2000FF0. */
Next reqBBS_813E616(void) {
    R0 = A_byte_2001400;
    R1 = 0x200;
    R2 = 0x40;
    GUEST_CALL(ByteFill);

    R0 = A_unk_2000FC0;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);

    R0 = A_unk_2000FF0;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── reqBBS_initMemory_813F9DA ── */
/* reqBBS_initMemory_813F9DA:
 * Reset the BBS (bulletin board) request state:
 *   - fill the request entry list (0x80 bytes) with 0x80 (free-slot marker)
 *   - clear the "requests sent" counter and one extra word
 * Leaves r2 = 0x80 (the fill value survives both clear calls). */
Next reqBBS_initMemory_813F9DA(void) {
    R0 = A_reqBBS_requestEntriesList;
    R1 = 0x80;
    R2 = 0x80;
    GUEST_CALL(ByteFill);

    R0 = A_reqBBS_numRequestsSent;
    R1 = 4;
    GUEST_CALL(ZeroFillByWord);

    R0 = A_unk_2000770;
    R1 = 4;
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── sub_8046DDC ── */
/* sub_8046DDC: advance the counter at (R5+8) by 2. Once it reaches 0x15,
 * clamp it there and move the object at R5 into state 8 (byte at R5+0).
 * Then runs sub_8047134 (which reads R5) and returns 0. */
Next sub_8046DDC(void) {
    u32 counter = (u32)LD8(R5 + 8) + 2;

    ST8(R5 + 8, counter);

    if ((s32)counter >= 0x15) {
        ST8(R5 + 8, 0x15);
        ST8(R5, 8);
    }

    GUEST_CALL(sub_8047134);

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
