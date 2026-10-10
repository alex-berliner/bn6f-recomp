/*
 * leaves_farm_11.c: small functions from the model farm, set 11.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
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

/* ── enemy_getStruct1 ── */
/* enemy_getStruct1(r0 = enemy index):
 * Resolves an enemy index to its per-enemy struct pointer.
 * GetVerActorTyAndAIIdx_80182B4 gives (version, actor_type, ai_index); the
 * actor_type selects one of the top-level tables (virus / navi / player) and
 * the ai_index indexes within it.
 * r0 = struct pointer, r2 = AI index * 4. */
Next enemy_getStruct1(void) {
    u32 enemy_idx = R0;

    R0 = enemy_idx;
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);
    const u8 *info = (const u8 *)R0; /* version, actor_type, ai_index */

    u32 actor_type = LD8((u32)info + 1);
    u32 ai_index   = LD8((u32)info + 2);

    u32 per_type_table = LD32(A_off_800F230 + (actor_type << 2));

    R2 = ai_index << 2;
    R0 = LD32(per_type_table + R2);
    RETURN_TO_CALLER();
}

/* ── enemy_getStruct2 ── */
/* enemy_getStruct2(r0 = enemy_idx):
 * Resolve an enemy index into a pointer to its per-version data record.
 * The version/actor-type/AI lookup returns a triple; the actor type selects one
 * of the virus/navi/player table groups, the AI index picks a table within the
 * group, and the version selects a 6-byte record inside that table.
 * r0 = pointer to the record, r3 = 6 (record size, as in the original). */
Next enemy_getStruct2(void) {
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4); /* r0 = &{version, actor_type, ai_index} */

    u32 info      = R0;
    u32 version   = LD8(info + 0);
    u32 actorType = LD8(info + 1);
    u32 aiIndex   = LD8(info + 2);

    u32 groupTable = LD32(A_off_800F260 + actorType * 4);
    u32 recordTable = LD32(groupTable + aiIndex * 4);

    R0 = recordTable + version * 6;
    R3 = 6; /* the original leaves the multiplier in r3 */
    RETURN_TO_CALLER();
}

/* ── nullsub_43 ── */
/* nullsub_43: empty stub, called by the game and immediately returns. */
Next nullsub_43(void) {
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

/* ── sub_800F26C ── */
/* sub_800F26C(r0 = enemy ID):
 * For IDs 0xCD..0xFF, return the 5-byte entry from the table byte_8021220
 * indexed by (ID - 0xCD). For any other ID, treat it as a regular enemy and
 * return the first two bytes of its struct (via enemy_getStruct1) with the
 * remaining outputs zeroed.
 * Outputs: r0..r4 (in the table path, r4 is the 5th byte; in the struct path
 * r4 is left untouched). */
Next sub_800F26C(void) {
    u32 id = R0;

    if ((u32)(id - 0xCD) > 0x32) {
        /* Not one of the special IDs: look up the enemy's own struct. */
        R0 = id;
        GUEST_CALL(enemy_getStruct1);
        u32 struct1 = R0;

        R0 = LD8(struct1 + 0);
        R1 = LD8(struct1 + 1);
        R2 = 0;
        R3 = 0;
        RETURN_TO_CALLER();
    }

    /* Special ID: fetch its fixed 5-byte data record. */
    u32 entry = A_byte_8021220 + (id - 0xCD) * 5;
    R0 = LD8(entry + 0);
    R1 = LD8(entry + 1);
    R2 = LD8(entry + 2);
    R3 = LD8(entry + 3);
    R4 = LD8(entry + 4);
    RETURN_TO_CALLER();
}

/* ── aiTrySelectChipFromHand_801336C ── */
/* aiTrySelectChipFromHand_801336C (r5 = battle object):
 * Decide whether the enemy AI should play the chip currently in its battle
 * hand this frame.  The attack-power hint in the AI data must be unset
 * (0xFF, for both the normal value and the Beast-Out override), the AI
 * condition flags must allow acting, and a real chip must be queued in the
 * hand; otherwise the function bails out with r0 = 0.  When every check
 * passes, sub_8013236 performs the actual chip selection and its results
 * (r2/r3, flags) are returned to the caller. */
Next aiTrySelectChipFromHand_801336C(void) {
    /* r1 = the battle object's AI data block */
    R1 = LD32(R5 + oBattleObject_AIDataPtr);

    /* the plain attack-power hint lives at offset 5 (oAIData_APwrAtk) */
    R0 = LD8(R1 + 5);
    SET_FLAGS_CMP(R0, 0xFF);
    if (R0 == 0xFF) {
        /* plain hint not filled in: try the Beast-Out value instead */
        R0 = LD8(R1 + oAIData_BeastOutAPwrAtk);
        SET_FLAGS_CMP(R0, 0xFF);
        if (R0 == 0xFF) {
            /* no attack hint at all: do not select a chip this frame */
            R0 = 0;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }
    }

    /* ask the AI data whether its condition flags allow acting */
    GUEST_CALL(returnBoolIfAIDataFlagsAreSet_8012F3E); /* answers in Z; the original branches on it */
    if (ZERO_FLAG()) {
        R0 = 0;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    /* act only if a real chip is waiting in the battle hand */
    GUEST_CALL(getCurChipInBattleHand_8010004);
    SET_FLAGS_CMP(R0, 0xFFFF);          /* 0xFFFF = hand is empty */
    if (R0 == 0xFFFF) {
        R0 = 0;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    /* all checks passed: pick the chip (r0 = chip id, r1 = 0xFFFF) */
    R1 = 0xFFFF;
    GUEST_CALL(sub_8013236);
    RETURN_TO_CALLER();
}

/* ── battle_aiAddCounterFromCollision_8013B20 ── */
/* battle_aiAddCounterFromCollision_8013B20(r5 = battle object):
 * Checks two bytes of the object's collision data. When byte 0xA4 reports the
 * custom-bar/full-custom state (0x18), the AI's byte at oAIData_Unk_12 is
 * raised by the amount in collision byte 0xA5, saturating at 7.
 * Leaves r3 = collision data pointer; flags come from the last comparison
 * performed (TST on byte 0xA4 when it is zero, otherwise the CMP that decided
 * the exit path). r4 is preserved. */
Next battle_aiAddCounterFromCollision_8013B20(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 ai = LD32(R5 + oBattleObject_AIDataPtr);
    R3 = collision;

    u32 state = LD8(collision + 0xA4);
    SET_FLAGS_NZ(state);
    if (state == 0)
        RETURN_TO_CALLER();

    u32 amount = LD8(collision + 0xA5);
    if (state != 0x18) {
        /* Not the tracked state: just report the amount in r2. */
        SET_FLAGS_CMP(state, 0x18);
        R2 = amount;
        RETURN_TO_CALLER();
    }

    /* Raise the AI counter by the given amount, saturating at 7. */
    u32 counter = amount + LD8(ai + oAIData_Unk_12);
    SET_FLAGS_CMP(counter, 7);
    if (counter > 7)
        counter = 7;
    R2 = counter;
    ST8(ai + oAIData_Unk_12, counter);
    RETURN_TO_CALLER();
}

/* ── battle_applyStateSpriteColorShader_801690A ── */
/* battle_applyStateSpriteColorShader_801690A: recolor a battle object's sprite to match the battle state.
 *
 * If the object's flags (object_getFlag) have bit 0x4000 set, read the current
 * battle phase (battle_getStateUnk40_800A704) and use bit 1 of it to pick one
 * of the two halfword entries in the table word_8016930 ({ 0x0000, 0xFFFF }),
 * then hand the selected color to sprite_setColorShader. Otherwise the sprite
 * is left untouched.
 *
 * Reads the current object via r5/r10 (passed through to object_getFlag).
 * The original never writes r3 itself; it ends up holding whatever
 * sprite_setColorShader leaves behind, which GUEST_CALL reproduces. */
Next battle_applyStateSpriteColorShader_801690A(void) {
    /* R0 = flags of the current battle object */
    GUEST_CALL(object_getFlag);

    if ((R0 & 0x4000) != 0) {
        /* R0 = current battle state; keep only its low bit pair */
        GUEST_CALL(battle_getStateUnk40_800A704);
        R0 &= 2;

        /* Look up the color for this state (0 or 2 bytes into the table)
         * and apply it to the object's sprite. */
        R0 = LD16(A_word_8016930 + R0);
        GUEST_CALL(sprite_setColorShader);
    }
    RETURN_TO_CALLER();
}

/* ── battle_endTurnMarkActed_8017888 ── */
/* battle_endTurnMarkActed_8017888(r5 = battle object):
 * Turn-end handling for a battle object. If the battle flag 0x40 is set, the
 * object's actor type (byte 1 of the GetVerActorTyAndAIIdx record for its
 * NameID) is 2, and the first word of its AI data block (aidata+0x40) is
 * still zero, run sub_80E1620 (r1 = actor type, r7 = AI data block pointer).
 * In every case the object is marked as having acted: CurAction (offset 9)
 * = 8 and the current phase is cleared. r0 = 0 on return.
 * Flags: N/Z from the final "mov r0,#0"; C/V from the last compare or callee. */
Next battle_endTurnMarkActed_8017888(void) {
    u32 saved_r7 = R7;
    u32 obj = R5;

    GUEST_CALL(battle_testFlag0x40_800A8F8); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = LD16(obj + oBattleObject_NameID);
        GUEST_CALL(GetVerActorTyAndAIIdx_80182B4); /* -> {version, actor_type, ai_index} */
        R1 = LD8(R0 + 1); /* actor type */
        if (R1 != 2) {
            SET_FLAGS_CMP(R1, 2);
        } else {
            /* first word of this enemy's AI data block */
            R7 = LD32(obj + oBattleObject_AIDataPtr) + 0x40;
            R0 = LD32(R7);
            if (R0 != 0) {
                SET_FLAGS_CMP(R0, 0);
            } else {
                GUEST_CALL(sub_80E1620);
            }
        }
    }

    R7 = saved_r7; /* r7 is only scratch here; restored on exit */
    ST8(obj + 9, 8); /* CurAction = 8 */
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── battle_spawnHitEffectAtArm_801A42E ── */
/* battle_spawnHitEffectAtArm_801A42E(r5 = battle object):
 * While the object is in a hit/collision state (collision timer byte at
 * +0x74 and hit-flash counter at +0x80 of its collision data are both
 * non-zero), compute the object's arm position and call sub_80E8124 to
 * spawn/draw the associated effect there.
 * Preserves r4-r7; r3 on return is whatever sub_80E8124 left (or the
 * incoming r3 when it returns early). */
Next battle_spawnHitEffectAtArm_801A42E(void) {
    u32 savedR4 = R4;

    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    if (LD8(collisionData + 0x74) != 0 && LD16(collisionData + 0x80) != 0) {
        R0 = LD16(R5 + oBattleObject_NameID);
        R1 = 5;
        R2 = LD8(R5 + oBattleObject_Alliance);
        R3 = LD8(R5 + oBattleObject_DirectionFlip);
        GUEST_CALL(getBattleArmPositionMaybe_8018810);

        u32 posX = R0;
        u32 posY = R1;

        R0 = posX;
        R1 = posX << 16;
        R2 = 0;
        R3 = posY << 16;
        R4 = 0;
        GUEST_CALL(sub_80E8124);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_spawnArmHitEffect_801A4A6 ── */
/* battle_spawnArmHitEffect_801A4A6(r5 = battle object):
 * Checks the object's collision data byte at offset 0xA4; when it reports
 * hit type 0xF4 or 0xF7, looks up where the arm attack connects for this
 * object and hands the result to the battle-effect helper sub_80E8124.
 * Returns with r3 and the c/v flags as left by sub_80E8124; r4-r7 preserved. */
Next battle_spawnArmHitEffect_801A4A6(void) {
    u32 savedR4 = R4;

    u32 hitType = LD8(LD32(R5 + oBattleObject_CollisionDataPtr) + 0xA4);
    if (hitType != 0xF4 && hitType != 0xF7) {
        /* No arm contact: leave with the flags of the original's last CMP. */
        SET_FLAGS_CMP(hitType, 0xF7);
        RETURN_TO_CALLER();
    }

    /* Ask where the arm position lands for this object's name/alliance/facing. */
    R0 = LD16(R5 + oBattleObject_NameID);
    R1 = 5;
    R2 = LD8(R5 + oBattleObject_Alliance);
    R3 = LD8(R5 + oBattleObject_DirectionFlip);
    GUEST_CALL(getBattleArmPositionMaybe_8018810);

    /* Pass the position on: r0 stays as returned, r1 = position << 16,
     * r3 = other result << 16, r2 = 0, r4 = 3. */
    {
        u32 position = R0;
        u32 other = R1;
        R1 = position << 16;
        R2 = 0;
        R3 = other << 16;
        R4 = 3;
        GUEST_CALL(sub_80E8124); /* updates r3 and the c/v flags */
    }

    R4 = savedR4;
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

/* ── EnterMap_RunMapGroupAsmFunction_8030A00 ── */
/* EnterMap_RunMapGroupAsmFunction_8030A00(r0 = map group number):
 * Dispatch to the EnterMap handler for the given map group. The group
 * number selects one of three jumptables:
 *   >= UNKNOWN_MAP_GROUP_START (0xF0): "unknown" table, index minus 0xF0
 *   >= INTERNET_MAP_GROUP_START:       internet table, index minus the base
 *   otherwise:                          real-world table, index as-is
 * The handler is entered with R0 = its own jumptable entry address and
 * R1 = the base of its jumptable. The original achieves the call with
 * `mov lr, pc; bx r0`, i.e. the handler returns to the `pop {pc}` right
 * after the call, which returns straight to *our* caller — so this whole
 * function behaves as a tail call and the handler's registers (r2, r4-r12)
 * pass through unchanged to the caller. */

/* Value `mov lr, pc` leaves in LR at 0x08030A1C: the address of the
 * following `pop {pc}` (PC reads as current instruction + 4 in Thumb). */
#define HANDLER_RETURN_ADDRESS 0x08030A20u

Next EnterMap_RunMapGroupAsmFunction_8030A00(void) {
    u32 returnAddress = LR; /* our caller's return address, restored below */
    u32 group = R0;
    u32 table;

    if ((s32)group >= (s32)UNKNOWN_MAP_GROUP_START) {
        table = A_EnterMap_UnkMapGroupJumptable;
        group -= UNKNOWN_MAP_GROUP_START;
    } else if ((s32)group >= (s32)INTERNET_MAP_GROUP_START) {
        table = A_EnterMap_InternetMapGroupJumptable;
        group -= INTERNET_MAP_GROUP_START;
    } else {
        table = A_EnterMap_RealWorldMapGroupJumptable;
    }

    {
        /* Entries are word-sized function addresses. */
        u32 handler = LD32(table + (group << 2));

        R0 = handler; /* the handler sees its own entry address in R0 */
        R1 = table;   /* ...and its jumptable base in R1 */
        LR = HANDLER_RETURN_ADDRESS;

        /* Indirect guest call: the callee runs with the register state
         * above and returns to HANDLER_RETURN_ADDRESS, which hands
         * control back to us just like the original's pop {pc} would. */
        dispatch(handler);
    }

    /* The handler has returned "to the pop {pc}": return to our caller
     * with the handler's results in the registers it left behind. */
    LR = returnAddress;
    RETURN_TO_CALLER();
}

/* ── LoadGFXAnimsForMapGroup ── */
/* LoadGFXAnimsForMapGroup(r0 = map group, r1 = map number) -> ()
 * Loads the GFX anims for the given map group by dispatching through one of
 * three jumptables: real-world groups use their index directly, internet
 * groups and the trailing "unknown" range use indexes relative to the start
 * of their range. Each handler is entered with r0 = index * 4 (the byte
 * offset used to fetch its jumptable entry) and r2 = its own entry address,
 * and returns to the instruction following the dispatch, after which this
 * function returns to its own caller. */
Next LoadGFXAnimsForMapGroup(void)
{
    u32 return_lr = LR;
    s32 group = (s32)R0;
    u32 table;

    if (group >= (s32)UNKNOWN_MAP_GROUP_START) {
        table = A_UnkMapGroupRangeLoadGFXAnimsJumptable;
        group -= UNKNOWN_MAP_GROUP_START;
    } else if (group >= (s32)INTERNET_MAP_GROUP_START) {
        table = A_InternetMapGroupLoadGFXAnimsJumptable;
        group -= INTERNET_MAP_GROUP_START;
    } else {
        table = A_RealWorldMapGroupLoadGFXAnimsJumptable;
    }

    u32 handler = LD32(table + (u32)group * 4);

    R0 = (u32)group << 2;
    R2 = handler;

    /* mov lr, pc: the handler returns to the pop {pc} that follows the
     * dispatch, which returns to this function's caller. */
    LR = 0x08030A50;
    dispatch(handler); /* bx r2 */

    LR = return_lr;
    RETURN_TO_CALLER();
}

/* ── startScreen_RunStateHandler_802F63C ── */
/* startScreen_RunStateHandler_802F63C(self: *StartScreen in r5) -> ():
 * Start screen state dispatcher. Reads the screen's state byte and calls the
 * matching handler from the jump table jt_802F654, passing it r0 = handler
 * address | thumb bit and r1 = the state byte, then runs the common
 * post-state routine sub_802FB10 before returning. */

/* Offset of the state byte in StartScreen; named as in the disassembly
 * (ldrb r1, [r5, #oStartScreen_JumpTableOff_01]). */
#define oStartScreen_JumpTableOff_01 1

Next startScreen_RunStateHandler_802F63C(void) {
    u32 state = LD8(R5 + oStartScreen_JumpTableOff_01);

    /* The state byte is a byte offset into the jump table (ldr r0, [r0, r1]
     * indexes it without scaling), so the five entries live at offsets
     * 0, 4, 8, 12 and 16, not 0..4. Load the entry exactly as the original
     * does so r0 carries the handler address | thumb bit into the call. */
    R1 = state;
    R0 = LD32(A_jt_802F654 + state);

    switch (state) {
    case 0:  GUEST_CALL(startScreen_init_802F668); break;                              /* state 0 */
    case 4:  GUEST_CALL(sub_802F6A4); break;                              /* state 1 */
    case 8:  GUEST_CALL(sub_802F6B2); break;                              /* state 2 */
    case 12: GUEST_CALL(startScreen_UpdateAndRenderFrame_802F704); break; /* state 3 */
    case 16: GUEST_CALL(sub_802F710); break;                              /* state 4 */
    }

    GUEST_CALL(sub_802FB10);

    RETURN_TO_CALLER();
}

/* ── npc_freeAllObjectsThenSpawnObjectsFromList ── */
/* npc_freeAllObjectsThenSpawnObjectsFromList(r0 = pointer to an array of NPC
 * script pointers, terminated by a 0xFF entry):
 * Switch the overworld to a new NPC spawn list. If the game state already
 * points at this list, nothing happens. Otherwise the list pointer is stored
 * in the game state, every existing overworld NPC object is freed, and the
 * list is walked spawning one object per entry until the 0xFF terminator.
 * On return: r0 points at the terminator entry, r1 = number of objects
 * spawned (or the unchanged current list pointer on the early-exit path),
 * r2 = 0xFF; flags come from the last compare (list vs. current on early
 * exit, terminator vs. 0xFF otherwise). r4-r6 are preserved. */
Next npc_freeAllObjectsThenSpawnObjectsFromList(void) {
    /* The original spills r4-r6 and pops them back at the end, so they are
     * preserved no matter what the callees do. */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 list = R0;

    /* Already showing this list: nothing to do. The original loads the
     * current pointer into r1 before comparing, so r1 carries it (and the
     * compare result carries into the flags) on the early-exit path. */
    u32 current = LD32(gameState + oGameState_Ptr_20);
    R1 = current;
    SET_FLAGS_CMP(list, current);

    if (list != current) {
        ST32(gameState + oGameState_Ptr_20, list);

        /* Clear out all currently spawned overworld NPCs (object type 4). */
        R4 = gameState;
        R0 = 4; /* OVERWORLD_NPC_OBJECT_F */
        GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);

        /* Spawn one object per list entry; the 0xFF entry ends the list.
         * r1 counts the objects spawned, r0 walks the entry array and r2
         * holds the current entry's script pointer. */
        u32 index = 0;
        R1 = 0;
        for (;;) {
            u32 script = LD32(list);
            R2 = script;
            SET_FLAGS_CMP(script, 0xFF);
            if (script == 0xFF)
                break;

            R4 = gameState;
            R0 = list;
            GUEST_CALL(npc_spawnObjectThenSetUnk10_TempAnimScriptPtr_8030a8c);

            index++;
            R1 = index;
            list += 4;
            R0 = list;
        }
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── chatbox_stateMaybeFinishText_8026E4C ── */
/* chatbox_stateMaybeFinishText_8026E4C (r5 = chatbox state struct, S20364C0):
 * Advance the chatbox one step. If flag 0x80 of eFlags2009F38 is set, ask
 * sub_802A220 whether the text is still busy: while it reports 0xFF, just
 * run the periodic update. Otherwise (flag clear, or text finished) play
 * sound effect 0x9E, set the state byte at +1 to 4, clear the timer
 * halfword at +2, and run the periodic update. */
Next chatbox_stateMaybeFinishText_8026E4C(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    if (R0 != 0) {
        GUEST_CALL(sub_802A220);
        if (R0 == 0xFF) {
            GUEST_CALL(sub_8029C08);
            RETURN_TO_CALLER();
        }
        GUEST_CALL(chatbox_8040818);
    }

    R0 = 0x9E;
    GUEST_CALL(PlaySoundEffect);

    ST8(R5 + 1, 4);
    ST16(R5 + 2, 0);

    GUEST_CALL(sub_8029C08);
    RETURN_TO_CALLER();
}

/* ── battle_updateObjectByKind_8026E98 ── */
/* battle_updateObjectByKind_8026E98(self = R5): one update step for a battle object.
 *
 * Reads the global battle effect flags and passes 4 in R4 if the 0x200000
 * effect is active, 0 otherwise. It then dispatches through the handler
 * table at off_8026EBC, indexed by the kind byte at self+2, calling the
 * selected handler with the kind in R0, its own thumb pointer in R1,
 * self in R5 and the effect flag in R4. Finally it runs sub_8029C08 on
 * the same object.
 *
 * R4 is only the effect flag for the duration of the calls; the caller's
 * value is restored on return (push/pop {r4} in the original).
 *
 * Flags before the calls mirror the original's TST/MOV pair:
 *   - effect present: N/Z from "movs r4, #4"
 *   - effect absent:  N/Z from "tst r0, #0x200000" (Z set)
 * C/V are untouched by both. The flags on return are whatever
 * sub_8029C08 leaves. */
Next battle_updateObjectByKind_8026E98(void) {
    u32 self = R5;
    u32 saved_r4 = R4;

    R4 = 0;
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 0x200000) {
        R4 = 4;
        SET_FLAGS_NZ(4);
    } else {
        SET_FLAGS_NZ(effects & 0x200000);
    }

    u32 kind = LD8(self + 2);
    R0 = kind;
    R1 = LD32(A_off_8026EBC + kind); /* thumb handler pointer from the ROM table */
    switch (kind) {
    case 0: GUEST_CALL(sub_8026EC8); break;
    case 4: GUEST_CALL(sub_8026F1A); break;
    case 8: GUEST_CALL(sub_8026FAA); break;
    }

    GUEST_CALL(sub_8029C08);
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── battle_compactFolderChipList_802945A ── */
/* battle_compactFolderChipList_802945A: compact the battle folder chip list (30 shorts at eBattleFolder).
 * Drops every empty slot (ID 0xFFFF), shifts the kept entries to the front,
 * and pads the freed tail with 0xFFFF so the list always has 30 entries.
 * r4/r6/r7 are only used as scratch by the original, which pushes and pops
 * them, so their values are preserved for the caller. */
Next battle_compactFolderChipList_802945A(void) {
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;

    u32 folder = A_eBattleFolder;
    u32 emptyId = 0xFFFFFFFFu;  /* 0xFFFF sign-extended, the "no chip" ID */
    u32 readOff = 0;            /* byte offset of slot being examined */
    u32 writeOff = 0;           /* byte offset of next free slot */
    u32 slotsToFill = 30;       /* entries still needed to fill the list */

    while (readOff < 0x3C) {
        s32 entry = LDS16(folder + readOff);
        if (entry != (s32)emptyId) {
            ST16(folder + writeOff, (u16)entry);
            writeOff += 2;
            slotsToFill--;
        }
        readOff += 2;
    }

    /* Pad whatever is left of the list with empty slots. */
    while (slotsToFill != 0) {
        ST16(folder + writeOff, (u16)emptyId);
        writeOff += 2;
        slotsToFill--;
    }

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── objStepFinishSetState4_802BE0C ── */
/* objStepFinishSetState4_802BE0C — finish step for the object in r5 (r10 = toolkit pointer):
 * calls call_sub_3005EBA with the fixed argument set (0, 0, 3, 0, 6, 2),
 * clears the render-info frame counter, calls sub_801E0A0(0, 0), then
 * writes state 4 into the object's halfword field at +2.
 * Preserves the entry values of r4 and r5 for the caller; exits with
 * NZ flags from "MOV r0, #4" and r1/r2/r3/r10 as sub_801E0A0 left them. */
Next objStepFinishSetState4_802BE0C(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 toolkit = R10;

    /* call_sub_3005EBA(0, 0, 3, 0, 6, 2) */
    R0 = 0;
    R1 = 0;
    R2 = 3;
    R3 = 0;
    R4 = 6;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    /* clear the render info frame counter: renderInfo = toolkit->renderInfo */
    u32 renderInfo = LD32(toolkit + 8 /* oToolkit_RenderInfoPtr */);
    ST16(renderInfo + oRenderInfo_Unk_18, 0);

    /* sub_801E0A0(0, 0); r5 is live for the callee, so restore it first */
    R0 = 0;
    R1 = 0;
    R5 = saved_r5;
    GUEST_CALL(sub_801E0A0);

    /* advance the object's state field to 4 (flags: N=0, Z=0) */
    R0 = 4;
    SET_FLAGS_NZ(4);
    ST16(saved_r5 + 2, 4);

    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── sub_802C460 ── */
/* sub_802C460 (r5 = object pointer):
 * Reads two signed bytes from the object at offsets 6 and 7 (two positions
 * along an axis). If they are equal there is nothing to do. Otherwise it
 * computes the distance between them and a start position (the smaller
 * value, offset by 0x18 when the byte at offset 6 is the smaller one) and
 * asks sub_802C490(start, 2, distance, 0x12) to scan between them. If that
 * scan reports a hit in r2, it calls call_sub_3005EBA(scan_result, 3, 0).
 *
 * r4/r5 are preserved around the calls; r1 ends up holding the byte at
 * offset 7 on the early-out path, and whatever the callees leave otherwise.
 * c/v are outputs: on the early-out path they come from the CMP of the two
 * bytes; on the call paths they are whatever the callee left. */
Next sub_802C460(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    s32 first = (s8)LD8(R5 + 6);
    s32 second = (s8)LD8(R5 + 7);
    R1 = second; /* original leaves the loaded byte in r1 */

    /* CMP first, second — the last flag-setting op on the early-out path. */
    SET_FLAGS_CMP(first, second);

    if (first != second) {
        u32 distance;
        u32 start;

        if (first < second) {
            /* first is the lower position: start just past it. */
            distance = second - first;
            start = first + 0x18;
        } else {
            distance = first - second;
            start = second;
        }

        R0 = start;
        R1 = 2;
        R4 = distance;
        R5 = 0x12;
        GUEST_CALL(sub_802C490); /* r2 = nonzero if something was found */

        if (R2 != 0) {
            R2 = 3;
            R3 = 0;
            GUEST_CALL(call_sub_3005EBA);
        }

        R4 = savedR4;
        R5 = savedR5;
    }

    RETURN_TO_CALLER();
}

/* ── runPendingHandlerForEntryPair_802CEC8 ── */
/* runPendingHandlerForEntryPair_802CEC8: for each of the two entries in the table at unk_2036720
 * (stride 0x10, entry pointer at +0x8), if the entry exists and its
 * halfword flag at +0x24 is clear, call sub_802CEA6 with the byte value
 * at +0x16 of that entry. r4 is left pointing at the second entry. */
Next runPendingHandlerForEntryPair_802CEC8(void) {
    u32 table = A_unk_2036720;

    for (u32 i = 0; i < 2; i++) {
        u32 entry = LD32(table + i * 0x10 + 8);

        if (entry != 0 && LD16(entry + 0x24) == 0) {
            R0 = LD8(entry + 0x16);
            GUEST_CALL(sub_802CEA6);
        }
    }

    R4 = table + 0x10;
    RETURN_TO_CALLER();
}

/* ── battle_getAllianceEntryStatus_802E4B8 ── */
/* battle_getAllianceEntryStatus_802E4B8(r5 = battle object):
 * Fetch the alliance entry for the battle object's alliance slot and return its
 * status code in r0: 9 if the byte at entry+0x50 is set, 13 if the byte at
 * entry+0x54 is set, otherwise 1. r7 is only used as scratch internally and is
 * restored on return (the original pushes/pops it). r1 = 0x54 is left behind on
 * the paths that read the +0x54 flag byte. */
Next battle_getAllianceEntryStatus_802E4B8(void) {
    /* r0 = this object's alliance slot index */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);

    u32 entry = R0;

    if (LD8(entry + 0x50) != 0) {
        R0 = 9;
        RETURN_TO_CALLER();
    }

    R1 = 0x54;
    R0 = (LD8(entry + 0x54) != 0) ? 0xD : 1;
    RETURN_TO_CALLER();
}

/* ── startScreen_playSoundIfFlagSet_802FB64 ── */
/* startScreen_playSoundIfFlagSet_802FB64(self: *const StartScreen r5):
 * If the start screen's status halfword has bit 0x80 set, run the sound/data
 * sequence at byte_802FB7C through sub_8001158. Otherwise do nothing. */
Next startScreen_playSoundIfFlagSet_802FB64(void) {
    if (LD16(R5 + 0xA) & 0x80) {
        R0 = A_byte_802FB7C;
        GUEST_CALL(sub_8001158);
    }
    RETURN_TO_CALLER();
}
