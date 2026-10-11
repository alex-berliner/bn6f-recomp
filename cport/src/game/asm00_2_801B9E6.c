/*
 * asm00_2_801B9E6.c: readable functions of bn6f asm00_2.s, from 0x0801B9E6, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── RunAIAttack ── */
/* RunAIAttack — run one step of the extra AI attack logic for the battle
 * object in R5.
 *
 * Control flow:
 *   - CurAction 0x00-0x0F: dispatch through the handler table passed in
 *     R0 (the caller's own attack table), indexed by CurAction.
 *   - CurAction >= 0x10: dispatch through the shared AIAttackJumptable,
 *     indexed by CurAction - 0x10 — unless attack variable 0x1d is 1,
 *     in which case sub_80EAD9C runs instead of a table handler (it is
 *     still given the table address in r0 and the index in r1).
 *
 * The selected handler is entered with r0 = its own address,
 * r1 = the table byte offset, r2 = attack variable 0x1d (only when
 * CurAction >= 0x10), and r4/r6/r7 pointing at the object's AI data /
 * AI state / attack variables. It returns straight back to
 * RunAIAttack's caller; r4, r6 and r7 are restored to the caller's
 * values first. r1-r3, r5 and r8-r12 pass through to the handler
 * unchanged.
 */
Next RunAIAttack(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 aiState = aiData + oAIData_AIState;
    u32 attackVars = aiData + oAIData_AttackVars;
    u32 callerR4 = R4, callerR6 = R6, callerR7 = R7;
    u32 curAction = LD8(R5 + 9 /* oBattleObject_CurAction */);
    u32 tableIndex;
    u32 handler;

    if (curAction < 0x10) {
        /* caller-supplied handler table */
        tableIndex = curAction;
        handler = LD32(R0 + tableIndex * 4);
    } else {
        tableIndex = curAction - 0x10;
        u32 attackVar1d = LD8(attackVars + oAIAttackVars_Unk_1d);

        if (attackVar1d == 1) {
            /* special case: dedicated routine instead of a table handler;
             * it still sees the table address, the index and the variable */
            R0 = A_AIAttackJumptable;
            R1 = tableIndex;
            R2 = attackVar1d;
            R4 = aiData;
            R6 = aiState;
            R7 = attackVars;
            GUEST_CALL(sub_80EAD9C);
            R4 = callerR4;
            R6 = callerR6;
            R7 = callerR7;
            RETURN_TO_CALLER();
        }
        handler = LD32(A_AIAttackJumptable + tableIndex * 4);
        R2 = attackVar1d;
    }

    /* Dispatch to the chosen handler. The `lsl r1, r1, #2` that forms the
     * table offset sets nz from the offset and its carry out (bit 30 of
     * the index — always 0 here, since the index fits in a byte). */
    R0 = handler;
    R1 = tableIndex * 4;
    R4 = aiData;
    R6 = aiState;
    R7 = attackVars;
    SET_FLAGS_NZ(R1);
    cpu.c = (tableIndex >> 30) & 1;
    GUEST_CALL_ADDR(handler);
    R4 = callerR4;
    R6 = callerR6;
    R7 = callerR7;
    RETURN_TO_CALLER();
}

/* ── battle_object_postAction4CheckClearFlags_801BA92 ── */
/* battle_object_postAction4CheckClearFlags_801BA92(r5 = battle object):
 * Post-action check for a battle object. If its current action is 4,
 * inspect its flag2 bits:
 *   - bit 14 set: nothing to do (result 1)
 *   - bit 1 clear: nothing to do (result 1)
 *   - otherwise clear flags and collision counter (result 2)
 * Any other action leaves state alone (result 0).
 * Result in r0; r1 ends up as the last mask tested on the flag paths. */
Next battle_object_postAction4CheckClearFlags_801BA92(void) {
    u32 result = 0;

    /* oBattleObject_CurAction is at offset 9 */
    if (LD8(R5 + 9) == 4) {
        result = 1;

        GUEST_CALL(object_getFlag2);
        R1 = 1 << 14;
        if ((R0 & R1) == 0) {
            GUEST_CALL(object_getFlag2);
            R1 = 2;
            if ((R0 & R1) != 0) {
                GUEST_CALL(battle_object_clearFlagsAndCollisionCounter_801A284);
                result = 2;
            }
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── battle_checkObjectAction6Flag_801BABE ── */
/* battle_checkObjectAction6Flag_801BABE(r4 = ? passed through to callees, r5 = battle object):
 * Inspects the object's current action and reports what to do about it:
 *   0 - the object is not in action 6,
 *   1 - action 6, but its "flag 2" (bit 1) is clear,
 *   2 - action 6 with flag 2 set; battleObject_clearHitFlags_801A29A is run as a side effect.
 * Result in r0. r4 is restored (the original only pushes/pops it), r5 is
 * preserved, and r1/r2 are scratch. Flags come from the final "mov r0, r4"
 * (an adds r0, r4, #0), so they reflect the result value. */
Next battle_checkObjectAction6Flag_801BABE(void) {
    u32 savedR4 = R4;
    u32 result = 0;

    /* Offset 9 in the battle object struct: current action id. */
    u32 action = LD8(R5 + 9);
    R0 = action; /* r0 still holds the action byte when object_getFlag2 is entered */

    if (action == 6) {
        R4 = 1; /* callees observe r4 = 1 (set before both calls in the original) */

        GUEST_CALL(object_getFlag2); /* r0 = flag2 bits of the object */

        R1 = 2; /* mov r1, #2 before tst — also visible to battleObject_clearHitFlags_801A29A */
        if (R0 & R1) {
            GUEST_CALL(battleObject_clearHitFlags_801A29A); /* r0 = flag2 return value is still in r0 */
            result = 2;
        } else {
            result = 1;
        }
    }

    R4 = savedR4; /* pop {r4} in the original restores the caller's value */
    R0 = result;  /* mov r0, r4 */
    SET_FLAGS_ADD(result, 0);
    RETURN_TO_CALLER();
}

/* ── object_reservePanel ── */
/* object_reservePanel(r0 = object whose panel is looked up, r5 = object that
 * wants to claim it):
 * If the panel under r0 exists and has no reserver yet, record r5 as the
 * reserver, set the panel's "reserved" flag (0x80) and the object's
 * "standing on a reserved panel" flag (0x20), and return 1. Otherwise leave
 * everything untouched and return 0. */
Next object_reservePanel(void) {
    u32 reserver = R5;

    /* Which panel (if any) is under the object in r0? */
    GUEST_CALL(object_getPanelDataOffset);
    u32 panel = R0;

    if (panel == 0) {
        /* No panel there: nothing to reserve. */
        R0 = 0;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    u32 currentReserver = LD32(panel + oPanelData_ReserverObjectPtr);
    if (currentReserver != 0) {
        /* Someone else already reserved this panel. */
        R0 = 0;
        R2 = currentReserver;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    /* Claim the panel for r5 and mark it as reserved. */
    ST32(panel + oPanelData_ReserverObjectPtr, reserver);
    R1 = LD32(panel + oPanelData_Flags);
    R2 = 0x80;
    R1 |= R2;
    ST32(panel + oPanelData_Flags, R1);

    /* Flag the reserving object as occupying a reserved panel
     * (oObjectHeader_Flags is at offset 0 of the object header). */
    R1 = 0x20;
    R0 = LD8(reserver) | R1;
    ST8(reserver, R0);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_removePanelReserve ── */
/* object_removePanelReserve(r0, r1 = panel lookup params, r5 = requesting object):
 * Look up the panel at the given position; if it is currently reserved by the
 * given object, clear the reservation (reserver pointer and reserved bit) and
 * return 1, otherwise return 0.
 *
 * Flags: the original's last flag-setting instruction differs per path.
 *   - no panel found:  tst r0, r0, then movs r0, #0  -> N=0 Z=1,
 *                      C/V untouched since object_getPanelDataOffset
 *   - wrong reserver:  cmp r1, r5, then movs r0, #0  -> C/V from the compare,
 *                      N=0 Z=1
 *   - success:         cmp r1, r5, then movs r0, #1  -> C/V from the compare,
 *                      N=0 Z=0
 * On the failure paths r1 holds the panel's reserver pointer (unchanged if no
 * panel was found); on success r1 holds the updated panel flags and r2 the
 * cleared bit mask. */
Next object_removePanelReserve(void) {
    GUEST_CALL(object_getPanelDataOffset);

    u32 panel = R0;
    if (panel == 0) {
        /* No panel at that position; r1/r2 stay as the callee left them. */
        R0 = 0;
        SET_FLAGS_NZ(0);                /* tst r0, r0 / movs r0, #0 */
        RETURN_TO_CALLER();
    }

    u32 reserver = LD32(panel + oPanelData_ReserverObjectPtr);
    /* cmp r1, r5 sets C/V on both remaining paths (MOVS afterwards only
     * rewrites N/Z, so the compare flags survive until the caller). */
    if (reserver != R5) {
        /* Reserved by someone else (or not at all). */
        R1 = reserver;
        R0 = 0;
        SET_FLAGS_CMP(reserver, R5);
        SET_FLAGS_NZ(0);                /* movs r0, #0 */
        RETURN_TO_CALLER();
    }

    /* Panel was reserved by us: release it. */
    ST32(panel + oPanelData_ReserverObjectPtr, 0);
    R2 = 0x80;
    R1 = LD32(panel + oPanelData_Flags) & ~0x80u;
    ST32(panel + oPanelData_Flags, R1);
    R0 = 1;
    SET_FLAGS_CMP(reserver, R5);
    SET_FLAGS_NZ(1);                    /* movs r0, #1 */
    RETURN_TO_CALLER();
}

/* ── RemoveBattleObjectPanelReserves ── */
/* RemoveBattleObjectReserves (0x0801BB78): called with r5 = battle object.
 * If the object's header flags (byte at offset 0) have bit 0x20 set ("this
 * object holds panel reserves"), sweep the whole 7x4 battlefield — columns
 * 7..1 outer-indexed by rows 4..1, exactly the original's downward-counting
 * loop order — and release every panel reserve owned by this object.
 * r6/r7 are restored to their incoming values; r2/r3 end up holding
 * whatever the last callee left there, as in the original. */
Next RemoveBattleObjectPanelReserves(void) {
    if (!(LD8(R5) & 0x20)) /* oObjectHeader_Flags is offset 0 */
        RETURN_TO_CALLER();

    /* row counts down 4..1 (original r7), col counts down 7..1 (original r6) */
    for (u32 row = 4; row != 0; row--) {
        for (u32 col = 7; col != 0; col--) {
            R0 = col;
            R1 = row;
            GUEST_CALL(object_getPanelDataOffset);

            u32 panel = R0;
            if (panel != 0 && LD32(panel + oPanelData_ReserverObjectPtr) == R5) {
                /* this panel is reserved by our object: clear it */
                R0 = col;
                R1 = row;
                GUEST_CALL(object_removePanelReserve);
            }
        }
    }
    RETURN_TO_CALLER();
}

/* ── object_updateSpriteTimestop ── */
/* object_updateSpriteTimestop (r5 = battle object header, r4/r10 unused inputs):
 * advance one battle object's sprite animation. Nothing happens while the
 * battle is paused, if the object has no sprite at all (flags bit 0), or if
 * the time stop has frozen its animation (flags bit 3). Otherwise, when the
 * object's current animation id no longer matches the copy the sprite was
 * last set up with, reapply the animation and reload its frame data, then
 * update the sprite frame. */
Next object_updateSpriteTimestop(void) {
    GUEST_CALL(battle_isPaused);
    if (R0 != 0)                            /* battle paused: sprites hold still */
        RETURN_TO_CALLER();

    R0 = LD8(R5);                           /* object flags (oObjectHeader_Flags, offset 0) */
    R1 = 1;
    if (!(R0 & R1)) {                       /* bit 0 clear: object has no sprite */
        SET_FLAGS_NZ(R0 & R1);
        RETURN_TO_CALLER();
    }

    R1 = 8;
    if (R0 & R1) {                          /* bit 3 set: animation frozen by time stop */
        SET_FLAGS_NZ(R0 & R1);
        RETURN_TO_CALLER();
    }

    R0 = LD8(R5 + oBattleObject_CurAnim);
    R1 = LD8(R5 + oBattleObject_CurAnimCopy);
    if (R0 != R1) {                         /* animation changed since last setup */
        GUEST_CALL(sprite_setAnimation);    /* r0 = new animation id */
        GUEST_CALL(sprite_loadAnimationData);
        ST8(R5 + oBattleObject_CurAnimCopy, LD8(R5 + oBattleObject_CurAnim));
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── AddRandomVarianceToTwoCoords ── */
/* AddRandomVarianceToTwoCoords(r0 = variance mask, r1 = coord A, r3 = coord B):
 * Jitter two coordinates by a centered random offset applied in the
 * fixed-point high half:
 *     coord += ((rng & mask) - mask/2) << 16
 * Coord A uses the low 16 bits of the RNG roll, coord B the high 16 bits.
 * r2/r4/r6/r7 must come out untouched; since GetRNG may trash registers,
 * the original saves r0-r3 around the call (and r4/r6/r7 at entry), so we
 * keep everything we have to preserve in locals across the call. */
Next AddRandomVarianceToTwoCoords(void) {
    u32 mask = R0;
    u32 coordA = R1;
    u32 coordB = R3;
    u32 half = mask >> 1;

    /* Values the caller expects back unchanged; GetRNG is free to clobber
     * caller-saved registers, so snapshot them before the call. */
    u32 savedR2 = R2;
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    GUEST_CALL(GetRNG);
    u32 rng = R0;

    R1 = coordA + (((rng & mask) - half) << 16);
    R3 = coordB + ((((rng >> 16) & mask) - half) << 16);

    R2 = savedR2;
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;

    RETURN_TO_CALLER();
}

/* ── calc8DirBitmaskTowardsPoint_801BE04 ── */
/* calc8DirBitmaskTowardsPoint_801BE04(r0 = x_a, r1 = y_a, r2 = x_b, r3 = y_b):
 * Compute the 8-way direction from point B toward point A as a small bitmask:
 * the y difference contributes 1 (A above B) or 2 (A below B), the x
 * difference contributes 4 (A left of B) or 8 (A right of B), and equal
 * coordinates contribute 0. E.g. A up-and-right of B yields 8 | 2 = 0xA.
 *
 * The comparisons must be exact signed comparisons of the operands (as the
 * original's subs + bgt/blt sequence achieves via the overflow flag), so
 * they are done directly rather than by subtracting, which could overflow.
 * Leaves c/v reflecting the final subtraction r1 - r3, like the original. */
Next calc8DirBitmaskTowardsPoint_801BE04(void) {
    u32 step_x = (s32)R0 > (s32)R2 ? 8
               : (s32)R0 < (s32)R2 ? 4
               :                     0;
    u32 step_y = (s32)R1 > (s32)R3 ? 2
               : (s32)R1 < (s32)R3 ? 1
               :                     0;

    R0 = step_x | step_y;

    /* Last flag-setting instruction of the original is SUBS r1, r1, r3. */
    SET_FLAGS_CMP(R1, R3);
    RETURN_TO_CALLER();
}

/* ── nullsub_10 ── */
/* nullsub_10: does nothing; kept as a placeholder so call sites stay addressable. */
Next nullsub_10(void) {
    RETURN_TO_CALLER();
}

/* ── flagSetBits_eStruct2035280_801BEB8 ── */
/* flagSetBits_eStruct2035280_801BEB8(r0 = flag bits to set):
 * Sets the given bits in the event/flag word at eStruct2035280+0x44.
 * Leaves r1 = eStruct2035280, r2 = the updated word, and nz from the OR. */
Next flagSetBits_eStruct2035280_801BEB8(void) {
    u32 flags = LD32(A_eStruct2035280 + 0x44) | R0;

    R1 = A_eStruct2035280;
    R2 = flags;
    ST32(A_eStruct2035280 + 0x44, flags);
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── flags_clearBits_20352C4_801BEC2 ── */
/* flags_clearBits_20352C4_801BEC2(r0 = mask): clear the bits of `mask` in the flag word at
 * eStruct2035280+0x44 (dword_20352C4), i.e. mark those flags as off.
 * Leaves r1 = eStruct2035280, r2 = updated word, and NZ set on it. */
Next flags_clearBits_20352C4_801BEC2(void) {
    u32 mask = R0;
    u32 flags;

    R1 = A_eStruct2035280;
    flags = LD32(R1 + 0x44);
    flags &= ~mask;
    ST32(R1 + 0x44, flags);
    R2 = flags;

    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── eStruct2035280_SetRequestBits_801BECC ── */
/* eStruct2035280_SetRequestBits_801BECC(r0 = flag bits): set the given bits in the request dword at
 * eStruct2035280+0x40 (dword_20352C0). Leaves the struct pointer in r1 and the
 * updated dword in r2. */
Next eStruct2035280_SetRequestBits_801BECC(void) {
    u32 *flags = (u32 *)A_eStruct2035280;
    R1 = A_eStruct2035280;
    R2 = LD32(A_eStruct2035280 + 0x40) | R0;
    ST32(A_eStruct2035280 + 0x40, R2);
    RETURN_TO_CALLER();
}

/* ── flags_clearBits_eStruct2035280_801BED6 ── */
/* flags_clearBits_eStruct2035280_801BED6(r0 = bit mask): clear the given bits in dword_20352C0.
 * Returns the pointer in r1 and the updated value in r2, with flags set
 * on the new value (callers branch on it). */
Next flags_clearBits_eStruct2035280_801BED6(void) {
    u32 mask = R0;
    u32 *field = (u32 *)(A_eStruct2035280 + 0x40); /* dword_20352C0 */

    R1 = (u32)A_eStruct2035280;
    R2 = LD32(field) & ~mask;
    ST32(field, R2);
    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── nullsub_58 ── */
/* nullsub_58: empty stub at 0x0801BFEC — does nothing. */
Next nullsub_58(void) {
    RETURN_TO_CALLER();
}

/* ── sub_801BFEE ── */
/* sub_801BFEE: enable (pass 1 to sub_801C002); all results (r0-r3, flags)
 * come straight from that call. */
Next sub_801BFEE(void) {
    R0 = 1;
    GUEST_CALL(sub_801C002);
    RETURN_TO_CALLER();
}

/* ── sub_801BFF8 ── */
/* sub_801BFF8: invoke sub_801C002 with r0 = 0 and let its results
 * (registers and flags) pass straight through to the caller. */
Next sub_801BFF8(void) {
    R0 = 0;
    GUEST_CALL(sub_801C002);
    RETURN_TO_CALLER();
}

/* ── battle_RequestEndBattle_801C06E ── */
/* battle_RequestEndBattle_801C06E: turn the "end battle" flag on.
 * Sets the argument to 1 and tail-calls sub_801C082 (the shared
 * flag-setting routine), which returns straight to this function's caller. */
Next battle_RequestEndBattle_801C06E(void) {
    R0 = 1;
    GUEST_CALL(sub_801C082);
    RETURN_TO_CALLER();
}

/* ── sub_801C078 ── */
/* sub_801C078: entry point that invokes the main routine with r0 = 0
 * (no arguments); all results come straight from sub_801C082. */
Next sub_801C078(void) {
    R0 = 0;
    GUEST_CALL(sub_801C082);
    RETURN_TO_CALLER();
}

/* ── easeValueTowardTarget_801C1EA ── */
/* easeValueTowardTarget_801C1EA(r0 = value, r1 = target, r4 = state object):
 * Marks the object as active (bytes at +8/+9 set to 1), then nudges the
 * value one step toward the target. The step is proportional to the
 * remaining distance (ease-out: an eighth of the gap, plus 2), so the
 * value closes in quickly at first and slows down near the target.
 * The step is clamped so the value never overshoots past the target.
 * Returns r0 = new value, r2 = the step that was taken. */
Next easeValueTowardTarget_801C1EA(void) {
    R2 = 1;
    ST8(R4 + 8, R2);
    ST8(R4 + 9, R2);

    /* ease-out step toward the target */
    R2 = (u32)((s32)(R0 - R1) >> 3) + 2;
    R0 -= R2;

    SET_FLAGS_CMP(R0, R1);
    if ((s32)R0 < (s32)R1) {
        /* overshot: land exactly on the target */
        R0 = R1;
        SET_FLAGS_ADD(R1, 0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_801C984 ── */
/* sub_801C984: query sub_802CE60 twice and store the answers as 0/1 flags:
 *   [r5+7] = (sub_802CE60(0) != 0)
 *   [r5+9] = (sub_802CE60(1) != 0)
 * The result of the second query is also left in r0, with flags matching
 * the original's final CMP/MOVS. */
Next sub_801C984(void) {
    u32 flag;

    /* First query, normalize the answer to 0/1 and store it. */
    R0 = 0;
    GUEST_CALL(sub_802CE60);
    flag = (R0 != 0);
    ST8(R5 + 7, flag);

    /* Second query: the raw return value is kept in r0 (0 or non-zero,
     * matching the original's conditional `mov r0, #1`), and the flags
     * come from either the CMP or the MOVS, depending on the path. */
    R0 = 1;
    GUEST_CALL(sub_802CE60);
    if (R0 != 0) {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        SET_FLAGS_CMP(R0, 0);
    }
    ST8(R5 + 9, R0);

    RETURN_TO_CALLER();
}

/* ── syncCurrentFieldsAndNotify_801C9A4 ── */
/* syncCurrentFieldsAndNotify_801C9A4: keep the "current" byte fields of the struct at R5 in sync with
 * the "target" ones.  Whenever [R5+8] differs from [R5+7], it is updated and
 * background_drawPresetTileBlock_801C9C8 is notified with event 6; likewise [R5+0xA] vs [R5+9] with
 * event 0x1A.  On return, R0/R1 hold [R5+0xA]/[R5+9] (or the callee's R1 on
 * the second update path), and the flags come from the final compare or from
 * the last background_drawPresetTileBlock_801C9C8 call. */
Next syncCurrentFieldsAndNotify_801C9A4(void) {
    u32 newFirst = LD8(R5 + 7);

    if (LD8(R5 + 8) != newFirst) {
        ST8(R5 + 8, newFirst);
        R0 = 6;
        R1 = newFirst;
        GUEST_CALL(background_drawPresetTileBlock_801C9C8);
    }

    u32 newSecond = LD8(R5 + 9);
    u32 curSecond = LD8(R5 + 0xA);
    R0 = curSecond;
    R1 = newSecond;

    if (curSecond == newSecond) {
        SET_FLAGS_CMP(curSecond, newSecond);
    } else {
        ST8(R5 + 0xA, newSecond);
        R0 = 0x1A;
        GUEST_CALL(background_drawPresetTileBlock_801C9C8);
    }

    RETURN_TO_CALLER();
}

/* ── background_drawPresetTileBlock_801C9C8 ── */
/* background_drawPresetTileBlock_801C9C8(r0 = destination column j, r1 = background id):
 * Draw one of the premade backgrounds. Each entry in the table at
 * byte_801D964 holds 16 halfwords of tile ids (2x3 blocks of 8x8 tiles);
 * the id selects the entry, which is copied from tile block 3 to the
 * background layer at position (j, i=2) with block size 4x2.
 * R0 (j) is passed through to CopyBackgroundTiles untouched; the caller's
 * R4/R5 are preserved across the call. */
Next background_drawPresetTileBlock_801C9C8(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    u32 tileIds = A_byte_801D964 + (R1 << 4); /* entry 1 = 16 halfwords */

    R3 = tileIds; /* tile_ids */
    R1 = 2;       /* i */
    R2 = 3;       /* which_tile_block_32x32 */
    R4 = 4;       /* j_size */
    R5 = 2;       /* i_size */
    GUEST_CALL(CopyBackgroundTiles);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── object_countdownTimer_801CA0C ── */
/* object_countdownTimer_801CA0C: tick the 8-bit countdown timer at [R5+0x0B]. Each call decrements
 * it by one; once it reaches zero it is left at 0xFF... no — once the decrement
 * yields zero, event/flag value 0x100 is reported through flags_clearBits_eStruct2035280_801BED6 and then
 * handed to the dispatcher dispatch_801DACC.
 *
 * Register contract: R5 is an object pointer (preserved). On the "timer not yet
 * expired" path the flags are those of the decrement (SUBS timer, #1) and R0
 * holds the new timer value; on the "expired" path R0-R3 and the flags are
 * whatever the two callees leave behind. */
Next object_countdownTimer_801CA0C(void) {
    u32 old = LD8(R5 + 0xB);
    u32 timer = old - 1;

    ST8(R5 + 0xB, timer);
    SET_FLAGS_CMP(old, 1);      /* flags of SUBS old, #1 */
    R0 = timer;

    if (timer != 0)
        RETURN_TO_CALLER();     /* still counting down */

    /* Timer expired: signal event 0x100. */
    R0 = 0x100;
    GUEST_CALL(flags_clearBits_eStruct2035280_801BED6);

    R0 = 0x100;
    GUEST_CALL(dispatch_801DACC);

    RETURN_TO_CALLER();
}

/* ── sub_801CA80 ── */
/* sub_801CA80(r5 = background layout state):
 * For each entry in the u16 table at [r5+0x28], copy a 9x2 strip of map
 * tiles. Entry i gives a destination column as j = 0x1E - entry; a value of
 * 0xFF marks the end of the table. The tile ids for strip i are two
 * interleaved runs of consecutive ids starting at 0xD1DA + 9*i (row 0 takes
 * the even ids, row 1 the odd ids); they are built in a stack buffer and
 * handed to CopyBackgroundTiles for tile block 32x32 #0. The row counter i
 * runs over 0,2,4,6, i.e. at most four strips.
 *
 * The original reserves 20 bytes for the saved registers before its 0x24-byte
 * local buffer, and the buffer address escapes to CopyBackgroundTiles, so both
 * reservations are kept. r4-r7 are clobbered as call arguments but restored,
 * as are the flags from the last CMP on whichever path exits. */
enum { STRIP_BASE_TILE = 0xD1DA }; /* first tile id used by the strips */

Next sub_801CA80(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    SP -= 20;                  /* push {r4-r7,lr} frame: the buffer sits below it */
    u32 buf = SP -= 0x24;      /* 2 rows x 9 halfword tile ids */
    u32 entryPtr = R5 + 0x28;  /* table of destination columns */

    u32 i = 0;
    R1 = i;
    for (;;) {
        R2 = LD16(entryPtr);
        SET_FLAGS_CMP(R2, 0xFF);
        if (R2 == 0xFF)        /* end of table */
            break;

        /* Build the id table: row 0 counts up from the base id, row 1 from base+1. */
        u32 baseTile = STRIP_BASE_TILE + 9 * i;
        for (u32 k = 0; k < 9; k++) {
            ST16(buf + 2 * k, baseTile + 2 * k);
            ST16(buf + 0x12 + 2 * k, baseTile + 1 + 2 * k);
        }

        /* push {r1}: keep the row counter alive across the call */
        SP -= 4;
        ST32(SP, R1);

        R0 = 0x1E - R2;        /* j: destination column */
        R2 = 0;                /* tile block 32x32 */
        R3 = buf;              /* tile ids */
        R4 = 9;                /* j size */
        R5 = 2;                /* i size */
        GUEST_CALL(CopyBackgroundTiles);

        R1 = LD32(SP);         /* pop {r1} */
        SP += 4;

        i += 2;
        entryPtr += 2;
        R1 = i;
        SET_FLAGS_CMP(R1, 8);
        if (i == 8)
            break;
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    SP += 0x24 + 20;
    RETURN_TO_CALLER();
}

/* ── palette_QueueObjectPalettePair_801CDEC ── */
/* Palette-transfer destination descriptors. The object's byte at +0x12 is
 * shifted into bits 16-23, filling the low half of each descriptor and thus
 * selecting the slot within the target. */
#define XFER_DST_A 0x80004012u
#define XFER_DST_B 0x40200012u
/* Palette-data blobs in ROM passed to the transfer helper. */
#define XFER_DATA_A 0xCBB4u
#define XFER_DATA_B 0xCBBCu

/* palette_QueueObjectPalettePair_801CDEC(r5 = object):
 * Queue two palette transfers for the object (dst A + data A, then dst B +
 * data B), with the slot index taken from the object's byte at +0x12.
 * Objects whose state byte at +0x0F is 5 or 6 are skipped: they still return
 * that byte in r0 (the load for the comparison lands in r0), with the flags
 * from the corresponding CMP. Otherwise r0-r3 end as the second callee left
 * them, and the flags are whatever it leaves. r4-r6 are preserved. */
Next palette_QueueObjectPalettePair_801CDEC(void) {
    /* The original saves r4-r6 on the stack; the callees are only guaranteed
     * to keep r5/r6, so keep our own copies to restore before returning. */
    u32 inR4 = R4, inR5 = R5, inR6 = R6;

    u32 state = LD8(R5 + 0xF);
    R0 = state; /* the comparison load leaves the state byte in r0 */

    if (state == 6) {
        SET_FLAGS_CMP(state, 6);
        RETURN_TO_CALLER();
    }
    if (state == 5) {
        SET_FLAGS_CMP(state, 5);
        RETURN_TO_CALLER();
    }

    u32 slot = (u32)LD8(R5 + 0x12) << 16;

    R2 = 0;
    R3 = 0;

    R0 = XFER_DST_A + slot;
    R1 = XFER_DATA_A;
    GUEST_CALL(sub_802FE28);

    /* r2/r3 are not re-zeroed here: the original passes on whatever the
     * first call left in them. */
    R0 = XFER_DST_B + slot;
    R1 = XFER_DATA_B;
    GUEST_CALL(sub_802FE28);

    R4 = inR4;
    R5 = inR5;
    R6 = inR6;
    RETURN_TO_CALLER();
}

/* ── screenFadeOutStepTick_801CE6C ── */
/* screenFadeOutStepTick_801CE6C: tick the staged fade driven by the work block at R5.
 *
 * work[7] counts the steps and work[6] is a per-block parameter. Each tick
 * calls sub_802FE7A(0, 0x40, 0xE0 - 0x20 * step, work[6]); the third
 * argument shrinks by 0x20 per step, so the effect fades out in stages.
 * Once the step counter reaches 5 the block moves to state 4 (work[0])
 * and the counter is cleared.
 *
 * On early ticks r0 = step counter; on the finishing tick r0 = 0 and
 * r1..r3 are whatever sub_802FE7A left. Flags: the original ends with
 * `cmp r0,#5` (c/v and, on the early-exit path, nz) followed by a
 * flag-setting `movs r0,#0` on the finishing path, so c/v always come
 * from the comparison and nz is overridden to "zero" only when the
 * counter reached 5. */
Next screenFadeOutStepTick_801CE6C(void) {
    u32 counter = LD8(R5 + 7);

    R0 = 0;
    R1 = 0x40;
    R2 = 0xE0 - counter * 0x20;
    R3 = LD8(R5 + 6);
    GUEST_CALL(sub_802FE7A);

    counter = LD8(R5 + 7);
    if ((s32)counter < 5) {
        R0 = counter;
        SET_FLAGS_CMP(counter, 5);
    } else {
        ST8(R5, 4);        /* state = 4 */
        R0 = 0;
        ST8(R5 + 7, 0);    /* step counter = 0 */
        /* cmp then movs r0,#0: c/v from the comparison, nz from the mov. */
        SET_FLAGS_CMP(counter, 5);
        SET_FLAGS_NZ(0);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_stepTextPrinter_801CE92 ── */
/* chatbox_stepTextPrinter_801CE92(r5 = message window object):
 * Pick the text-advance delay for the current message state (field +7) and
 * start the text printer with sub_802FE7A. Once the state reaches 0x30 the
 * message sequence is over: the object moves to state 8 and field +7 is
 * cleared. States other than 1/2/3/0x2E/0x2F/0x30 skip the printer call.
 * On return r0 = field +7 and the flags reflect CMP r0, #0x30 (or the final
 * "mov r0, #0" when the state was reset). */
Next chatbox_stepTextPrinter_801CE92(void) {
    switch (LD8(R5 + 7)) {
    case 1:
    case 0x2F:
        R2 = 0x34;
        break;
    case 2:
    case 0x2E:
        R2 = 0x38;
        break;
    case 3:
    case 0x30:
        R2 = 0x40;
        break;
    default:
        goto advance_state;
    }

    /* Start the text printer: r0 = 0, r1 = 0x40, r2 = delay, r3 = field +6. */
    R0 = 0;
    R1 = 0x40;
    R3 = LD8(R5 + 6);
    GUEST_CALL(sub_802FE7A);

advance_state:
    /* Reload the message state into r0 (the original always ends with it there). */
    R0 = LD8(R5 + 7);
    SET_FLAGS_CMP(R0, 0x30);
    if ((s32)R0 < 0x30)
        RETURN_TO_CALLER();

    /* Message finished: switch the object to state 8, reset the message state. */
    ST8(R5, 8);
    R0 = 0;
    ST8(R5 + 7, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── flags_clearWordSlicesTaskStep_801CED2 ── */
/* flags_clearWordSlicesTaskStep_801CED2(r5 = task state struct):
 * One step of a multi-frame flag-clearing task. The struct holds:
 *   +6: flag word index the task operates on
 *   +7: step counter (each step covers a 0x20-bit slice, first at bit 0x40)
 * Each call clears this step's 0x40-bit slice of the flag word via
 * sub_802FE7A, then re-reads the counter: once it reaches 5 the task's
 * flag word bit is cleared and state[0] is set to 0xC (task done).
 * Leaves r0-r3 and the flags exactly as the last helper/compare does. */
Next flags_clearWordSlicesTaskStep_801CED2(void) {
    /* Clear this step's slice of the flag word:
     * sub_802FE7A(0, 0x40, 0x40 + step * 0x20, flagWordIndex). */
    R0 = 0;
    R1 = 0x40;
    R2 = 0x40 + LD8(R5 + 7) * 0x20;
    R3 = LD8(R5 + 6);
    GUEST_CALL(sub_802FE7A);

    /* Re-read the counter to see if every slice has been cleared. */
    R0 = LD8(R5 + 7);
    SET_FLAGS_CMP(R0, 5);
    if ((s32)R0 >= 5) {
        /* All slices done: clear the task's flag word bit... */
        R0 = LD8(R5 + 6);
        GUEST_CALL(flags_clearFlagWordBit_802FE6A);

        /* ...and mark the task finished (state[0] = 0xC, r0 = 0xC). */
        R0 = 0xC;
        ST8(R5, R0);
        SET_FLAGS_NZ(R0);
    }
    /* More steps remain: return with r0 = counter, flags from the compare. */
    RETURN_TO_CALLER();
}

/* ── event_raiseFlag8000AndDispatch_801CEFA ── */
/* event_raiseFlag8000AndDispatch_801CEFA: raise the 0x8000 flag via flags_clearBits_eStruct2035280_801BED6, then hand the same
 * flag to dispatch_801DACC so the pending event/transition takes effect. */
Next event_raiseFlag8000AndDispatch_801CEFA(void) {
    R0 = 1u << 15;                  /* event flag 0x8000 */
    GUEST_CALL(flags_clearBits_eStruct2035280_801BED6);

    R0 = 1u << 15;
    GUEST_CALL(dispatch_801DACC);

    RETURN_TO_CALLER();
}

/* ── state_dispatchHandler_801CF0E ── */
/* state_dispatchHandler_801CF0E: state-machine dispatcher for the block at byte_2036840.
 *
 * byte_2036848 holds the current state index; the matching handler from the
 * ROM table at off_801CF28 is tail-called with:
 *   r0 = handler address (as loaded from the table, thumb bit set)
 *   r1 = &off_801CF28 (the table itself)
 *   r7 = &byte_2036840 (the state block the handlers operate on)
 * and the caller's r7 is restored afterwards (the original pushes/pops it
 * around the call). Control never comes back here: the handlers return
 * straight to our caller, so this behaves like a tail call. */
Next state_dispatchHandler_801CF0E(void) {
    u32 callerR7 = R7;

    u32 state = LD8(A_byte_2036840 + 8);
    u32 tableOffset = state * 4;

    /* Condition flags exactly as the original's `lsl r0, r0, #2` leaves them:
     * nz from the shifted index, carry out of bit 30, v untouched. */
    SET_FLAGS_NZ(tableOffset);
    cpu.c = (state >> 30) & 1;

    u32 handler = LD32(A_off_801CF28 + tableOffset);

    /* Registers as the tail-called handler sees them. */
    R0 = handler;
    R1 = A_off_801CF28;
    R7 = A_byte_2036840;

    /* off_801CF28: entries 0-2 share sub_801CF3C, 3 is sub_801CF9E,
     * 4 is sub_801D048. */
    switch (state) {
    case 0:
    case 1:
    case 2:
        GUEST_CALL(sub_801CF3C);
        break;
    case 3:
        GUEST_CALL(sub_801CF9E);
        break;
    case 4:
        GUEST_CALL(sub_801D048);
        break;
    }

    /* pop {r7, pc}: hand our caller back its r7 and return to it. */
    R7 = callerR7;
    RETURN_TO_CALLER();
}

/* ── battle_sceneFrameSetup_801DA24 ── */
/* battle_sceneFrameSetup_801DA24: battle-scene per-frame setup.
 * - stores 0x1F09 into the render info block's Unk_0a halfword,
 * - queues the GFX transfers for the battle scene's data list (off_801ECB4),
 * - runs the player object pre-update and records the returned object
 *   pointer into the battle state at dword_20352C8 (eStruct2035280 + 0x48). */
Next battle_sceneFrameSetup_801DA24(void) {
    /* r10 holds the toolkit base; the render info pointer sits at offset 8
     * (oToolkit_RenderInfoPtr in the disassembly). */
    u32 renderInfo = LD32(R10 + 8);

    ST16(renderInfo + oRenderInfo_Unk_0a, 0x1F09);

    /* dataList (off_801ECB4) */
    R0 = A_dat00;
    GUEST_CALL(QueueGFXTransfersInList);

    GUEST_CALL(battle_preUpdateAndGetPlayerObject_80103EC);

    ST32(A_eStruct2035280 + 0x48, R0); /* dword_20352C8 */

    RETURN_TO_CALLER();
}

/* ── sub_801DB54 ── */
/* sub_801DB54: call flags_clearBits_20352C4_801BEC2 with r0 = 1 (flags and r0-r2 come from the callee). */
Next sub_801DB54(void) {
    R0 = 1;
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801DB60 ── */
/* sub_801DB60: invoke some game action with argument 1.
 * Sets R0 = 1 and tail-calls flagSetBits_eStruct2035280_801BEB8; all outputs (r0-r2, flags)
 * come from that callee. */
Next sub_801DB60(void) {
    /* mov r0, #1; lsl r0, r0, #0 (identity shift, value stays 1) */
    R0 = 1;

    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);

    /* pop {pc}: return to the caller with the callee's results intact */
    RETURN_TO_CALLER();
}

/* ── sub_801DB6C ── */
/* sub_801DB6C: call flags_clearBits_20352C4_801BEC2 with r0 = 2, built as
 *     mov r0, #1 ; lsl r0, r0, #1
 * flags_clearBits_20352C4_801BEC2 performs no flag-setting instruction on its return path,
 * so the flags the caller sees are the ones that lsl leaves behind:
 * result 2 gives N=0, Z=0, and the carry out of a 1-bit left shift of 1
 * (old bit 31) is 0. */
Next sub_801DB6C(void) {
    R0 = 2;              /* 1 << 1 */
    SET_FLAGS_NZ(R0);    /* N=0, Z=0 from the shifted result */
    cpu.c = 0;           /* carry out of lsl r0, #1: bit 31 of 1 was clear */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801DB78 ── */
/* sub_801DB78: wrapper that calls flagSetBits_eStruct2035280_801BEB8 with r0 = 2.
 * The value is built as "mov r0,#1; lsl r0,#1", and the LSL is a
 * flag-setting instruction: it loads the carry flag with the bit
 * shifted out of bit 31 (0 here). flagSetBits_eStruct2035280_801BEB8's reported flags depend
 * on the incoming carry, so this must be reproduced exactly. */
Next sub_801DB78(void) {
    R0 = 1;
    SET_FLAGS_ADD(R0, R0);   /* same flags as "lsl r0, r0, #1" on 1: N=0, Z=0, C=0 */
    R0 = R0 + R0;            /* r0 = 2 */
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── battleRefreshTrackedObjectArmPos_801DBD4 ── */
/* Object position table: six 8-byte slots at 0x20352E0 (dword_20352E0,
 * addressed here through the eStruct2035280 Size field symbol).  Each slot:
 *   +0  active flag
 *   +1  stored x position
 *   +2  stored y position
 *   +4  pointer to the battle object being tracked                    */
#define POS_TABLE  A_eStruct2035280_Size

/* battleRefreshTrackedObjectArmPos_801DBD4(r5 = battle object):
 * Find the table slot that tracks this battle object and refresh its
 * stored arm position (x, and -y - 8) from the object's sprite data.
 * R0 = 1 (flags as described below) if no slot tracks the object. */
Next battleRefreshTrackedObjectArmPos_801DBD4(void) {
    u32 slot = POS_TABLE;

    for (R3 = 6; R3 != 0; R3--) {
        R2 = LD8(slot);
        if (R2 != 0 && LD32(slot + 4) == R5) {
            /* Found the slot: derive the arm position from the object. */
            R0 = LD16(R5 + oBattleObject_NameID);
            R1 = 3;
            R2 = LD8(R5 + oBattleObject_Alliance);
            R3 = LD8(R5 + oBattleObject_DirectionFlip);
            GUEST_CALL(getBattleArmPositionMaybe_8018810);

            /* R0 = x; negate the y result and raise it by 8 pixels. */
            u32 flippedY = 0 - R1;      /* neg r1, r1 */
            R1 = flippedY - 8;          /* sub r1, #8 */
            SET_FLAGS_CMP(flippedY, 8);
            ST8(slot + 1, R0);
            ST8(slot + 2, R1);
            RETURN_TO_CALLER();
        }
        slot += 8;
    }

    /* No slot tracks this object: R0 = 1.  The original leaves N/Z clear
     * from `mov r0,#1` and C set / V clear from the loop's final
     * `subs r3,#1`; CMP 1,0 reproduces that exact flag state.  R1 keeps
     * its incoming value, R2 the last slot's flag byte and R3 the
     * exhausted counter, as callers may observe. */
    R0 = 1;
    SET_FLAGS_CMP(1, 0);
    RETURN_TO_CALLER();
}

/* ── sub_801DC06 ── */
/* sub_801DC06(r0 = value byte, r1 = amount byte, r5 = id):
 * Scans the 6-slot registry at dword_20352E0 (8 bytes per slot: +0 "in use"
 * flag, +4 id word) for an active slot whose id matches r5.  On a hit, stores
 * r0 at slot+1 and r1-8 at slot+2, and returns with r0 = value, r1 = amount-8.
 * If no slot matches, returns r0 = 1.  r4/r6/r7 are preserved (pushed/popped);
 * r2 ends as the last "in use" byte read and r3 as the remaining-slot count
 * (0 when the scan ran out of slots). */
Next sub_801DC06(void) {
    u32 value = R0;   /* byte written to slot+1 on a hit */
    u32 amount = R1;  /* slot+2 receives amount - 8 */
    u32 id = R5;      /* id being looked up */
    u32 slot = A_eStruct2035280_Size;
    u32 remaining = 6;

    for (;;) {
        u32 in_use = LD8(slot);
        R2 = in_use;      /* original keeps the last flag byte read in r2 */
        R3 = remaining;   /* ...and the not-yet-checked slot count in r3 */
        if (in_use != 0 && LD32(slot + 4) == id)
            break;

        slot += 8;
        SET_FLAGS_CMP(remaining, 1); /* "subs r3, #1" */
        remaining--;
        R3 = remaining;
        if (remaining == 0) {
            /* No matching slot: return 1 ("mov r0, #1" only refreshes nz) */
            R0 = 1;
            SET_FLAGS_NZ(1);
            RETURN_TO_CALLER();
        }
    }

    /* Found: patch the slot's value/amount bytes */
    ST8(slot + 1, value);
    SET_FLAGS_CMP(amount, 8); /* "subs r1, #8" */
    ST8(slot + 2, amount - 8);
    R0 = value;
    R1 = amount - 8;
    RETURN_TO_CALLER();
}

/* ── releaseSlotByValue_801DC36 ── */
/* releaseSlotByValue_801DC36(r5 = entry value to release):
 * Searches the six 8-byte slots of the table at dword_20352E0 (each slot is a
 * u8 "in use" flag followed by a u32 value) for the first occupied slot whose
 * value equals r5, and clears that slot. Leaves r0 = 0 if a slot was cleared,
 * r0 = 1 if none matched. r2 ends up holding the last flag byte read and r3
 * the number of slots that were still unexamined. The c/v flags come from the
 * last comparison executed: the value comparison on a match, the final
 * "subs r3, #1" (1 - 1) when the table is exhausted. */
Next releaseSlotByValue_801DC36(void) {
    u32 slot = A_eStruct2035280_Size; /* == dword_20352E0, start of the slot table */
    u32 slotsLeft = 6;

    while (1) {
        R2 = LD8(slot);
        if (R2 != 0) {
            u32 value = LD32(slot + 4);
            if (value == R5) {
                /* Found it: flag the comparison for the caller, then clear the slot. */
                SET_FLAGS_CMP(value, R5);
                ST32(slot, 0);
                ST32(slot + 4, 0);
                R0 = 0;
                R3 = slotsLeft;
                RETURN_TO_CALLER();
            }
        }

        /* Move to the next slot and count down; the flags of the final
         * decrement (1 - 1) are what the caller sees when nothing matched. */
        slot += 8;
        SET_FLAGS_CMP(slotsLeft, 1);
        slotsLeft--;
        if (slotsLeft == 0)
            break;
    }

    R0 = 1;
    R3 = 0;
    RETURN_TO_CALLER();
}

/* ── sub_801DC6E ── */
/* sub_801DC6E: invoke flagSetBits_eStruct2035280_801BEB8 with r0 = 4 (bit flag 1 << 2).
 *
 * The LSL that builds the argument is the last flag-setting instruction
 * before the call, and flagSetBits_eStruct2035280_801BEB8 preserves the flags, so the state it
 * leaves must survive to our caller: shifting 1 left by 2 pushes no set
 * bit out, giving N=0, Z=0, C=0 (result 4, no bits shifted out).
 * SET_FLAGS_ADD(1, 3) reproduces exactly those flags (sum 4, no carry). */
Next sub_801DC6E(void) {
    R0 = 1u;
    R1 = 2u;
    R0 = R0 << R1;          /* r0 = 1 << 2 = 4 */
    SET_FLAGS_ADD(1u, 3u);  /* N=0, Z=0, C=0 — same flags as the LSL */

    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── sub_801DDD8 ── */
/* sub_801DDD8:
 * Runs a fixed two-call sequence:
 *
 *   call_sub_3005EBA(6, 0, 3, 0, 0x12, 2)   -- six arguments in r0..r5
 *   flags_clearBits_20352C4_801BEC2(0x10, 4, ...)               -- r0 = 1 << 4; r2/r3 still hold
 *                                              whatever the first callee left
 *
 * The caller's r4/r5 are scratch here: they are overwritten with call
 * arguments and restored on the way out.  Everything else on return
 * (r0..r3, flags) is whatever flags_clearBits_20352C4_801BEC2 left behind. */
Next sub_801DDD8(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    /* call_sub_3005EBA(6, 0, 3, 0, 0x12, 2) */
    R0 = 6;
    R1 = 0;
    R2 = 3;
    R3 = 0;
    R4 = 0x12;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    /* mov r0,#1; mov r1,#4; lsl r0,r1: r0 = 0x10.  The Thumb LSL is the
     * last flag-setting instruction before the next call: N/Z come from
     * the result and C from the bit shifted out of 0x00000001, which is
     * 0.  flags_clearBits_20352C4_801BEC2 is entered with exactly those flags. */
    R0 = 1;
    R1 = 4;
    R0 = R0 << 4;
    SET_FLAGS_NZ(R0);
    cpu.c = 0;

    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── custGauge_Init_801DED0 ── */
/* custGauge_Init_801DED0: reset the custom gauge and reload its graphics.
 * Clears the gauge state, queues the gauge tiles from ROM into VRAM,
 * then runs the gauge-init routine (0x10) and dispatches to the
 * gauge handler (0x10). */
Next custGauge_Init_801DED0(void) {
    GUEST_CALL(ClearCustGauge);

    R0 = A_dword_86E489C; /* source: custom gauge tile data in ROM */
    R1 = 0x600C440;       /* destination: VRAM */
    R2 = 0x380;           /* transfer size */
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    R0 = 0x10;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);

    R0 = 0x10;
    GUEST_CALL(dispatch_801DA48);

    RETURN_TO_CALLER();
}

/* ── sub_801DF8C ── */
/* sub_801DF8C(r0 = value): store a halfword into word_20352A2 (the 0x22 field of
 * the eStruct2035280 struct). */
Next sub_801DF8C(void) {
    ST16(A_eStruct2035280 + 0x22, R0);
    RETURN_TO_CALLER();
}

/* ── ClearCustGauge ── */
/* ClearCustGauge: clear the custom gauge (zero its progress counter at
 * word_20352A0), then let battle_clearFlags reset the gauge-related flag
 * bits (0x12). Result registers come from battle_clearFlags. */
Next ClearCustGauge(void) {
    ST16(A_eStruct2035280 + 0x20, 0);

    R0 = 0x12;
    GUEST_CALL(battle_clearFlags);

    RETURN_TO_CALLER();
}

/* ── chatbox_openMessageWindow_801DFEA ── */
/* chatbox_openMessageWindow_801DFEA: lay out and activate a message window.
 *
 * First the window geometry is set up through call_sub_3005EBA:
 * origin (0,0), 0x12 x 3 cells, box style 0x11, palette/layer 2.
 * Then flags_clearBits_20352C4_801BEC2 is called with mode bits 0x40 to start the window.
 *
 * r4/r5 are only used to carry the 5th/6th arguments into
 * call_sub_3005EBA; they are restored to their entry values before
 * returning. r6/r7 are never touched. Exits with r0 = 0x40, r3 = 0,
 * r1/r2 as left by flags_clearBits_20352C4_801BEC2, and the flags of the `lsl r0, r0, #6`
 * (N=0, Z=0, C=0, V unchanged) unless flags_clearBits_20352C4_801BEC2 sets its own. */
Next chatbox_openMessageWindow_801DFEA(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    /* call_sub_3005EBA(0, 0x12, 3, 0, 0x11, 2): build the window */
    R0 = 0;
    R1 = 0x12;
    R2 = 3;
    R3 = 0;
    R4 = 0x11;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    /* flags_clearBits_20352C4_801BEC2(0x40): start the window running */
    R0 = 1 << 6;          /* mov r0,#1; lsl r0,#6 -> 0x40 */
    SET_FLAGS_NZ(R0);     /* result 0x40: N=0, Z=0 */
    cpu.c = 0;            /* no bit shifted out of the lsl */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    /* pop {r4,r5}: the caller's values are live again on return */
    R4 = saved_r4;
    R5 = saved_r5;

    RETURN_TO_CALLER();
}

/* ── sub_801E006 ── */
/* sub_801E006: wrapper that invokes flagSetBits_eStruct2035280_801BEB8 with the argument 0x40
 * (built as mov r0,#1; lsl r0,r0,#6).
 *
 * The immediate LSL is the last flag-setting instruction of this function
 * that the callee leaves untouched, and callers branch on these flags right
 * after the call, so its flag effects must be reproduced:
 *   result 0x40 -> N=0, Z=0; shifted-out bit 26 of 1 -> C=0.
 * SET_FLAGS_ADD(R0, 0) yields exactly that carry/N/Z combination. */
Next sub_801E006(void) {
    R0 = 1 << 6;              /* argument for flagSetBits_eStruct2035280_801BEB8 */
    SET_FLAGS_ADD(R0, 0);     /* flags as left by `lsl r0, r0, #6` */
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── battle_spawnObjectFromRecord_801E022 ── */
/* battle_spawnObjectFromRecord_801E022 (r5 = record describing the object to spawn):
 * Spawns a battle object through call_sub_3005EBA, feeding it the two
 * bytes stored at [r5+1] and [r5+2] together with the fixed parameter
 * set (r2 = 3, r3 = 0, r4 = 6, r5 = 2). Afterwards it invokes
 * flags_clearBits_20352C4_801BEC2 with 0x80 (built as mov r0,#1 ; lsl r0,#7).
 *
 * r4/r5 hold caller state and are preserved (the original saves and
 * pops them); r6/r7/r10 pass through untouched. The lsl that forms the
 * 0x80 argument is the routine's last flag-setting instruction: shifting
 * 1 left by 7 shifts out a zero, so the carry flag ends up clear. */
Next battle_spawnObjectFromRecord_801E022(void) {
    u32 callerR4 = R4;
    u32 callerR5 = R5;

    R0 = LD8(R5 + 1);
    R1 = LD8(R5 + 2);
    R2 = 3;
    R3 = 0;
    R4 = 6;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    R0 = 1u << 7; /* 0x80 */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    /* Carry-out of `lsl r0, r0, #7` with r0 = 1 is bit 25 of the old
     * value, which is clear. */
    cpu.c = 0;

    /* Restore the caller's r4/r5, as the original's pop {r4,r5,pc} does. */
    R4 = callerR4;
    R5 = callerR5;
    RETURN_TO_CALLER();
}

/* ── background_applyFixedTilePatch_801E03E ── */
/* background_applyFixedTilePatch_801E03E: refresh a 6x2-tile patch of the background.
 *
 * Reads the tile coordinates (j, i) from eStruct2035280 (offsets 1 and 2),
 * copies a 6x2 block of tile IDs from byte_801EDFC into 32x32 tile block 3,
 * then asks flagSetBits_eStruct2035280_801BEB8 (argument 0x80) to apply the update.
 * Preserves r4/r5 for the caller. */
Next background_applyFixedTilePatch_801E03E(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    u32 coords = A_eStruct2035280;
    u32 j = LD8(coords + 1); /* byte_2035281 */
    u32 i = LD8(coords + 2); /* byte_2035282 */

    R0 = j;
    R1 = i;
    R2 = 3;                  /* which tile block 32x32 */
    R3 = A_byte_801EDFC;     /* tile ids */
    R4 = 6;                  /* j size */
    R5 = 2;                  /* i size */
    GUEST_CALL(CopyBackgroundTiles);

    /* mov r0,#1 ; lsl r0,r0,#7 -> r0 = 0x80.
     * The shift is the last flag-setting instruction here: it sets N/Z
     * from the result (0x80) and C from the bit shifted out of 1 (0). */
    R0 = 1 << 7;
    SET_FLAGS_NZ(R0);
    cpu.c = 0;
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sub_801E0A0 ── */
/* sub_801E0A0(r0, r1 = two byte parameters):
 * Copies the halfword at +0x24 of the struct pointed to by the entry at
 * +0x48 of eStruct2035280 into eStruct2035280+0x24, stores the two byte
 * parameters at +0x1 and +0x2, then runs the 0x80 (text-box) update path:
 * eStruct2035280_SetRequestBits_801BECC followed by dispatch_801DA48.
 * Leaves R1/R2/R3 and the c/v flags as dispatch_801DA48 left them. */
Next sub_801E0A0(void) {
    u32 boxStruct = A_eStruct2035280;

    u16 counter = LD16(LD32(boxStruct + 0x48) + 0x24);
    ST16(boxStruct + 0x24, counter);

    ST8(boxStruct + 1, R0);
    ST8(boxStruct + 2, R1);

    R3 = boxStruct;
    R0 = 0x80;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);

    R3 = boxStruct;
    R0 = 0x80;
    GUEST_CALL(dispatch_801DA48);

    RETURN_TO_CALLER();
}

/* ── setStruct2035280Field26_801E0C8 ── */
/* setStruct2035280Field26_801E0C8(r0 = value):
 * Store a halfword into word_20352A6 (field 0x26 of the eStruct2035280
 * structure). R5 is only used as a scratch pointer inside the original and
 * is restored on exit, so it must be left unchanged here. */
Next setStruct2035280Field26_801E0C8(void) {
    u32 base = A_eStruct2035280;

    ST16(base + 0x26, R0);
    RETURN_TO_CALLER();
}

/* ── event_markStepDoneAndPlayFinishEffects_801E0DC ── */
/* event_markStepDoneAndPlayFinishEffects_801E0DC(r5 = struct pointer):
 * Marks the two bytes at r5+8 and r5+0xA as done (0xFF), fires effect 6 and
 * effect 0x1A through call_sub_3005EBA (both with the same parameter set:
 * r1=2, r2=3, r3=0, r4=4, r5=2), then clears event flag bit 0x400 in the
 * flag word at 0x20352C4 via flags_clearBits_20352C4_801BEC2.
 * r4/r5 are call-saved, so they are held in locals while the fixed call
 * arguments occupy the real registers. */
Next event_markStepDoneAndPlayFinishEffects_801E0DC(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    ST8(R5 + 8, 0xFF);
    ST8(R5 + 0xA, 0xFF);

    R0 = 6;
    R1 = 2;
    R2 = 3;
    R3 = 0;
    R4 = 4;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    R0 = 0x1A;
    R1 = 2;
    R2 = 3;
    R3 = 0;
    R4 = 4;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    /* lsl r0, r0, #10 on r0 = 1: result 0x400 (N=0, Z=0), the carry out is
     * old bit 22, which is 0. flags_clearBits... leaves the flags untouched,
     * so this is the flag state the caller sees on return. */
    R0 = 1 << 10;
    SET_FLAGS_NZ(R0);
    cpu.c = 0;
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── battle_loadModeGfx_801E10E ── */
/* battle_loadModeGfx_801E10E: prepare the mode-specific battle gfx and raise eStruct flag 0x400.
 *
 * If the current battle mode is not 6, queue a 0x20-word gfx transfer copying
 * the dword_86E1C78 tiles into the buffer at unk_3001A80.  Afterwards set bit
 * 10 (mask 1 << 10) in the eStruct at 0x2035280 via flagSetBits.
 */
Next battle_loadModeGfx_801E10E(void) {
    GUEST_CALL(GetBattleMode);
    u32 battleMode = R0;

    if (battleMode != 6) {
        /* QueueEightWordAlignedGFXTransfer(src = dword_86E1C78,
         *                                  dst = unk_3001A80,
         *                                  size = 0x20 words) */
        R0 = A_dword_86E1C78;
        R1 = A_unk_3001A80;
        R2 = 0x20;
        GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    }

    /* Flag mask 1 << 10, computed as "lsls r0, r0, #10" in the original:
     * result 0x400, N/Z from 0x400, carry-out (bit 22 of the 1) is 0. */
    R0 = 0x400;
    SET_FLAGS_NZ(R0);
    cpu.c = 0; /* the shift pushes a zero bit out into the carry */

    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── battle_redrawCommandMenuWindow_801E1A4 ── */
/* battle_redrawCommandMenuWindow_801E1A4: redraw the battle command-menu window, then do menu upkeep.
 *
 * R5 points at the battle work struct; [R5+0xC] == 2 means the custom
 * screen is open, in which case the window frame is drawn six pixels
 * narrower (and its left edge moves accordingly); otherwise the full-size
 * frame is used. Afterwards bit 0x100 of the battle flag word at
 * 0x20352C4 is cleared, and while the battle is still running and the
 * custom screen is closed, event 0x400 is dispatched to advance the menu.
 *
 * R4-R6 are preserved; R0-R3 end as the last callee left them. On return
 * the flags are those of the last comparison performed, except on the
 * dispatch path, where dispatch_801DA48's own flags are left in place. */
Next battle_redrawCommandMenuWindow_801E1A4(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    /* Frame shift: 6 while the custom screen (state 2) is open, else 0. */
    u32 shift = (LD8(R5 + 0xC) == 2) ? 6 : 0;

    R0 = 7 + (shift >> 1);  /* window left edge */
    R1 = 2;                 /* window top edge  */
    R2 = 3;
    R3 = 0;
    R4 = 0x11 - shift;      /* window width     */
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    R0 = 0x100;
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    GUEST_CALL(battle_isBattleOver);

    if (R0 == 1) {
        /* Battle over: stop here, flags as `cmp r0, #1` left them. */
        SET_FLAGS_CMP(R0, 1);
    } else if (shift != 0) {
        /* Custom screen open: stop here, flags as `cmp r6, #0` left them. */
        SET_FLAGS_CMP(shift, 0);
    } else {
        /* Battle running, normal menu: request the next menu phase. */
        R0 = 1 << 10;
        GUEST_CALL(dispatch_801DA48);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── sub_801E5E0 ── */
/* sub_801E5E0: call flags_clearBits_20352C4_801BEC2 with r0 = 0x4000.
 * The wrapper itself contributes nothing but the argument: r0-r2 and the
 * flags after return are exactly what flags_clearBits_20352C4_801BEC2 leaves behind. Note that
 * the original's `mov r0,#1` + `lsl r0,r0,#14` sequence is a Thumb shifted
 * move, which sets C from the bit shifted out (bit 18 of 1 = 0), so the call
 * is entered with C clear — flags_clearBits_20352C4_801BEC2 relies on that on paths where it
 * preserves C. */
Next sub_801E5E0(void) {
    R0 = 1 << 14;
    SET_FLAGS_ADD(R0, 0);   /* N=0, Z=0, C=0, matching the LSL result */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801E5EC ── */
/* sub_801E5EC: invoke flagSetBits_eStruct2035280_801BEB8 with r0 = 0x4000 (1 << 14).
 * All results (r0-r2) come straight from the callee; the callee also leaves
 * N/Z alone, but passes the C flag through unchanged, so we must reproduce
 * the C=0 that the original's "lsl r0, r0, #14" leaves behind (shifting the
 * value 1 left by 14 shifts no 1-bits out of the register). */
Next sub_801E5EC(void) {
    R0 = 1u << 14;
    /* Same flag state the lsl leaves: N=0, Z=0, C=0. */
    SET_FLAGS_ADD(R0, 0);
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── sub_801E71C ── */
/* sub_801E71C(r0 = value):
 * Stores the byte in r0 into byte_2035292 (field 0x12 of eStruct2035280).
 * r1 is preserved; r0's incoming value is not meaningful afterwards. */
Next sub_801E71C(void) {
    ST8(A_eStruct2035280 + 0x12, R0);
    RETURN_TO_CALLER();
}

/* ── naviStatus_SetGfxAndStatus_801E724 ── */
/* naviStatus_SetGfxAndStatus_801E724(r0 = new navi status gfx index, r1 = new navi status value):
 * Updates the navi status state at eStruct2035280. The current display index
 * (0x17) is reused if it is set, otherwise falls back to the value at 0x15;
 * it is written to 0x1F. Stores the new status at 0x16, the new gfx index at
 * 0x17, and marks 0x13 as pending with 0xFF. r5 is saved/restored by the
 * original, so it is left unchanged. r2 = the display index written. */
Next naviStatus_SetGfxAndStatus_801E724(void) {
    u32 gfxIdx = R0;
    u32 status = R1;
    u32 state  = A_eStruct2035280;

    u32 displayIdx = LD8(state + 0x17);
    if (displayIdx == 0xFF)
        displayIdx = LD8(state + 0x15);

    ST8(state + 0x1F, displayIdx);
    ST8(state + 0x16, status);
    ST8(state + 0x17, gfxIdx);
    ST8(state + 0x13, 0xFF);

    R0 = 0xFF;
    R2 = displayIdx;
    RETURN_TO_CALLER();
}

/* ── sub_801E73C ── */
/* sub_801E73C: invoke flags_clearBits_20352C4_801BEC2 with r0 = 0x8000.
 *
 * The constant has no Thumb MOV encoding, so the original builds it as
 * 1 << 15 — and that LSL is the last flag-setting instruction on the path
 * through this wrapper, because flags_clearBits_20352C4_801BEC2 itself leaves the condition
 * flags untouched. Callers therefore see the shift's flag results
 * (N = 0, Z = 0, carry-out = 0) after this function returns, so they must
 * be produced explicitly. LR ends up holding the BL's return address
 * (0x801E747), which the GUEST_CALL return trampoline reproduces. */
Next sub_801E73C(void) {
    R0 = 1;
    R0 <<= 15;          /* r0 = 0x8000; shifting 1 by 15 carries out a 0 bit */
    SET_FLAGS_NZ(R0);   /* N = 0, Z = 0 from the shifted result */
    cpu.c = 0;          /* carry out of the LSL */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801E748 ── */
/* sub_801E748: invoke flagSetBits_eStruct2035280_801BEB8 with the argument 0x8000 (bit 15 set).
 * The literal "mov r0,#1; lsl r0,r0,#15" also updates the flags, and since
 * the callee preserves them on exit, its carry-out (0 for this operand)
 * is what our caller observes. */
Next sub_801E748(void) {
    R0 = 1u << 15;
    /* flag state after "lsl r0, r0, #15": result 0x8000 -> N=0, Z=0,
     * carry out of bit 17 of the old value 1 -> C=0. V is unaffected. */
    SET_FLAGS_NZ(R0);
    cpu.c = 0;
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── comm_checkCrossNetBattleReady_801E754 ── */
/* comm_checkCrossNetBattleReady_801E754: inspect cross-net battle state flags.
 * Returns in R0:
 *   0 - bit 15 of dword_20352C0 is clear
 *   1 - flag set, but the byte_2036840/2036848 state doesn't match
 *   2 - flag set and byte_2036840 holds the expected value 4
 * R1/R2 are preserved; C/V are left as the original's last flag-setting
 * instruction leaves them. */
Next comm_checkCrossNetBattleReady_801E754(void) {
    u32 stateWord = LD32(A_eStruct2035280 + 0x40); /* dword_20352C0 */

    /* The original builds the test mask with `mov r0,#1; lsl r0,#15`.
     * That shift leaves C clear, and on the early-exit path nothing after
     * it touches C (the `and` only sets N/Z), so carry stays clear. */
    cpu.c = 0;
    u32 flagBit = (1u << 15) & stateWord;
    SET_FLAGS_NZ(flagBit);

    if (flagBit == 0) {
        R0 = 0;
    } else {
        u8 modeSlot = LD8(A_byte_2036840 + 8); /* byte_2036848 */
        if (modeSlot == 4 || modeSlot == 2) {
            u8 modeByte = LD8(A_byte_2036840); /* byte_2036840 */
            SET_FLAGS_CMP(modeByte, 4);
            R0 = (modeByte == 4) ? 2 : 1;
        } else {
            SET_FLAGS_CMP(modeSlot, 2);
            R0 = 1;
        }
    }
    RETURN_TO_CALLER();
}

/* ── setByteFieldCapped99_801E828 ── */
/* setByteFieldCapped99_801E828(r1 = value, r5 = struct pointer):
 * Clamp the byte value to at most 99, store it at [r5+9], then let
 * sub_801E838 refresh whatever depends on it (results and flags are
 * whatever that call leaves behind). */
Next setByteFieldCapped99_801E828(void) {
    u32 value = R1;

    if ((s32)value > 99)
        value = 99;

    ST8(R5 + 9, value);
    R1 = value;

    GUEST_CALL(sub_801E838);

    RETURN_TO_CALLER();
}

/* ── sub_801E8CC ── */
/* sub_801E8CC(r0 = value, r5 = struct pointer):
 * Calls sub_801E95C with r4 = 0 (r0 is saved across the call and restored,
 * so the callee's r0 result is discarded). Afterwards, if the original r0
 * was 0x50 and the byte at [r5+0xC] is non-zero, subtracts 0x10 from the
 * byte at [r5+2] and stores it back. On return, r0 and the nz/c/v flags
 * reflect the outcome:
 *   - r0 != 0x50:          r0 = input value,   flags from CMP r0, 0x50
 *   - r0 == 0x50, [+C]==0: r0 = 0,             flags from CMP r0, 0
 *   - r0 == 0x50, [+C]!=0: r0 = byte - 0x10,   flags from SUBS r0, 0x10 */
Next sub_801E8CC(void) {
    u32 matchValue = R0;
    u32 savedR4 = R4;

    R4 = 0;
    GUEST_CALL(sub_801E95C);

    if (matchValue != 0x50) {
        R0 = matchValue;
        SET_FLAGS_CMP(matchValue, 0x50);
    } else if (LD8(R5 + 0xC) == 0) {
        R0 = 0;
        SET_FLAGS_CMP(0, 0);
    } else {
        u32 timer = LD8(R5 + 2);
        u32 updated = timer - 0x10;
        ST8(R5 + 2, updated);
        R0 = updated;
        SET_FLAGS_CMP(timer, 0x10);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}
