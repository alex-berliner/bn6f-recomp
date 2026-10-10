/*
 * leaves_farm_9.c: small functions from the model farm, set 9.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

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

/* ── GetBattleNaviStatsAddr ── */
/* GetBattleNaviStatsAddr(r0 = navi index):
 * Returns the address of the given navi's entry in the battle navi stats
 * table: eBattleNaviStats0 + index * sizeof(NaviStats). r1 is left holding
 * the table base address, and the flags reflect the final address addition. */
Next GetBattleNaviStatsAddr(void) {
    u32 offset = R0 * oNaviStats_Size;

    R1 = A_eBattleNaviStats;
    SET_FLAGS_ADD(offset, A_eBattleNaviStats);
    R0 = offset + A_eBattleNaviStats;

    RETURN_TO_CALLER();
}

/* ── getBattleObjectPalette_800F334 ── */
/* getBattleObjectPalette_800F334(r5 = battle object) -> r0 = palette index.
 *
 * Returns the palette to use for a battle object, based on its AI data:
 * viruses index directly by version * unk03, while every other actor looks
 * its palette base up in a small per-version table (byte_800F354) and then
 * scales it by unk03 the same way.
 *
 * Also leaves r1 = actor type and r2 = version (version * unk03 for viruses)
 * in the registers, matching the original.
 */
#define ACTOR_TYPE_VIRUS 0

Next getBattleObjectPalette_800F334(void) {
    u32 aiData    = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData);                    /* oAIData_ActorType is at offset 0 */
    u32 version   = LD8(aiData + oAIData_Version_16);
    u32 unk03     = LD8(aiData + 3);                /* oAIData_Unk_03 */
    u32 palette;

    if (actorType == ACTOR_TYPE_VIRUS) {
        /* Viruses: the palette comes straight from version * unk03. */
        palette = version * unk03;
        version = palette;  /* the original's mul writes back into r2 */
    } else {
        /* Others: byte_800F354 maps the version to a palette base. */
        palette = LD8(A_byte_800F354 + version) * unk03;
    }

    R0 = palette;
    R1 = actorType;
    R2 = version;
    RETURN_TO_CALLER();
}

#undef ACTOR_TYPE_VIRUS

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

/* ── returnBoolIfAIDataFlagsAreSet_8012F3E ── */
/* returnBoolIfAIDataFlagsAreSet_8012F3E:
 * Reports whether the "A button held" condition is met for the current AI
 * data. It returns 1 in r0 only when the 0x44 flag word has none of the
 * 0x1000002F bits set, and the 0x48 flag word has the 0x200 bit clear but
 * the 0x10 bit set; otherwise it returns 0. Flags reflect the result. */
Next returnBoolIfAIDataFlagsAreSet_8012F3E(void) {
    u32 result;

    GUEST_CALL(GetAIData_Unk_44_Flag);
    if ((R0 & 0x1000002F) != 0) {
        result = 0;
    } else {
        GUEST_CALL(GetAIDataUnk0x48Flag);
        if ((R0 & 0x200) != 0)
            result = 0;
        else
            result = (R0 & 0x10) != 0 ? 1 : 0;
    }

    R0 = result;
    SET_FLAGS_NZ(result);
    RETURN_TO_CALLER();
}

/* ── setChipsForPlayerObject_800FDEA ── */
/* setChipsForPlayerObject_800FDEA(r5 = battle object):
 * Record on a battle object which chips it holds from its alliance's battle
 * hand.  The hand buffer starts with a count byte followed by halfword chip
 * entries; this object's entries are the ones after the first `hand[0]` of
 * them, up to the 0xFFFF terminator.  The number of those entries is stored
 * in oBattleObject_ChipsHeld and the first entry's chip ID (or 0xFFFF if
 * there are none) in oBattleObject_Chip.
 *
 * Register side effects left for callers: r1 points at the terminator entry,
 * r2 and r3 both hold 0xFFFF.  r4 and r5 are preserved. */
Next setChipsForPlayerObject_800FDEA(void) {
    u32 object = R5;

    R0 = LD8(object + oBattleObject_Alliance);
    GUEST_CALL(getBattleHandAddr_8010018);
    u32 hand = R0;

    /* Start of this object's section: skip the count byte and the halfword
     * entries it accounts for. */
    u32 cursor = hand + 2 + 2 * LD8(hand);
    u32 firstChip = LD16(cursor);

    /* Count this object's chip entries up to the 0xFFFF terminator. */
    u32 count = 0;
    u32 entry = firstChip;
    while (entry != 0xFFFF) {
        count++;
        cursor += 2;
        entry = LD16(cursor);
    }

    ST8(object + oBattleObject_ChipsHeld, count);
    ST16(object + oBattleObject_Chip, firstChip);

    /* State the original leaves behind in the output registers. */
    R1 = cursor;   /* address of the terminator entry */
    R2 = entry;    /* terminator value, i.e. 0xFFFF */
    R3 = 0xFFFF;
    RETURN_TO_CALLER();
}

/* ── battle_damagePlayerLeave1HP_8013FD0 ── */
/* battle_damagePlayerLeave1HP_8013FD0: deal damage to the player. On entry R0 is the alliance flag used to
 * locate the player's battle object and R0 also carries the base damage amount
 * (read by battle_findPlayer / the stats lookup on entry). The damage is capped so
 * the hit can never reduce the player below 1 HP, then the hit sound is played. */
Next battle_damagePlayerLeave1HP_8013FD0(void) {
    /* R0 (alliance flag / pending damage value) is already set up by the caller. */
    GUEST_CALL(battle_findPlayer);
    u32 player = R0;

    if (player == 0)
        RETURN_TO_CALLER();

    /* Fetch the damage value stored at navi-stats offset 0x54 for this object. */
    R0 = player;
    R1 = 0x54;
    GUEST_CALL(GetBattleNaviStatsHword_AllianceFromBattleObject);
    u32 damage = R0;

    if (damage == 0)
        RETURN_TO_CALLER();

    /* Cap the subtraction at (current HP - 1) so the player survives the hit. */
    u32 hp = LD16(player + 0x24);
    R0 = ((s32)hp > (s32)damage) ? damage : hp - 1;
    GUEST_CALL(object_subtractHP);

    R0 = SOUND_HIT_6B;
    GUEST_CALL(PlaySoundEffect);

    RETURN_TO_CALLER();
}

/* ── getCurNaviStatTier_8014150 ── */
/* getCurNaviStatTier_8014150:
 * Inspect two stat bytes of the currently equipped PET Navi and return a
 * small selector: 2 if stat 0x38 is set, otherwise 1 if stat 0x37 is set,
 * otherwise 0. (r4/r10 are untouched; result in r0.) */
Next getCurNaviStatTier_8014150(void) {
    u32 result = 2;

    GUEST_CALL(GetCurPETNavi);   /* () -> u8: current navi id */
    R1 = 0x38;
    GUEST_CALL(GetCurPETNaviStatsByte); /* (navi, 0x38) -> u8 */

    if (R0 == 0) {
        result = 1;

        GUEST_CALL(GetCurPETNavi);
        R1 = 0x37;
        GUEST_CALL(GetCurPETNaviStatsByte); /* (navi, 0x37) -> u8 */

        if (R0 == 0)
            result = 0;
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── battle_runObjectAI_80143FC ── */
/* battle_runObjectAI_80143FC: run one battle object's AI update, unless the battle is frozen.
 *
 * Does nothing while time-stop or the pause menu is active. Otherwise reads the
 * object's flags: if one of the "AI active" bits (0xC00) is set, run the AI
 * handler sub_8014432; if the object has no AI, clear its AI data field 0x4C.
 *
 * inputs:  r5 (object), r10 (battle context), lr
 * outputs: r1 (whatever the last callee left), v (set only by sub_8014432)   */
Next battle_runObjectAI_80143FC(void) {
    GUEST_CALL(battle_isTimeStop);
    if (R0 != 0)
        RETURN_TO_CALLER();

    GUEST_CALL(battle_isPaused);
    if (R0 != 0)
        RETURN_TO_CALLER();

    GUEST_CALL(object_getFlag);

    R1 = 0xC00;                 /* bits: AI update requested / AI exists */
    if ((R0 & R1) != 0)
        GUEST_CALL(sub_8014432);
    else
        GUEST_CALL(battle_clearAIDataField4c_8014424);

    RETURN_TO_CALLER();
}

/* ── battle_tickPoisonHpLoss_8014498 ── */
/* battle_tickPoisonHpLoss_8014498: decrement this battle object's HP by 1, but never let it drop
 * to 0 here. Does nothing once the battle is over, and only acts when the
 * AI data's flag at oAIData_Unk_36 is set. Leaves the AI data pointer in r3
 * for the caller (except when returning early on battle-over). */
Next battle_tickPoisonHpLoss_8014498(void) {
    GUEST_CALL(battle_isBattleOver);
    if (R0 == 0) {
        u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
        R3 = aiData;

        if (LD16(aiData + oAIData_Unk_36) != 0) {
            u32 hp = LD16(R5 + oBattleObject_HP);
            if (hp != 1) {
                ST16(R5 + oBattleObject_HP, hp - 1);
            }
        }
    }
    RETURN_TO_CALLER();
}

/* ── battle_isObjectInSpecialAiState_80159A2 ── */
/* battle_isObjectInSpecialAiState_80159A2(r0 = object pointer):
 * "Is this object in a special AI state?" check. Returns 1 in r0 when the
 * object pointer is non-NULL and either bit 0x100 of the flag word at
 * AI+0x48 or bit 0x40 of the flag word at AI+0x44 is set; otherwise 0.
 *
 * The AI-data helpers are called with the object pointer in r5 and r7 = 1;
 * r5/r7 are saved and restored around them. The tail "mov r0, r7" is the
 * flag-setting MOVS/ADDS-immediate-zero form, so the function returns with
 * c = 0 and v = 0 on every path (n/z come from the result). */
Next battle_isObjectInSpecialAiState_80159A2(void) {
    u32 savedR5 = R5;
    u32 savedR7 = R7;
    u32 object = R0;
    u32 result = 0;

    if (object != 0) { /* beq tests the flags of "MOVS r5, r0" */
        result = 1;

        R5 = object; /* the helpers take the object pointer in r5 ... */
        R7 = 1;      /* ...and expect r7 = 1 */

        GUEST_CALL(GetAIDataUnk0x48Flag); /* r0 = flag word at AI+0x48 */
        R1 = 0x100;
        if ((R0 & 0x100) == 0) {
            GUEST_CALL(GetAIData_Unk_44_Flag); /* r0 = flag word at AI+0x44 */
            R1 = 0x40;
            if ((R0 & 0x40) == 0)
                result = 0;
        }

        R5 = savedR5;
        R7 = savedR7;
    }

    /* tail of the original: MOVS/ADDS r0, r7, #0 — n/z from the result,
     * carry always clear (adding 0), overflow clear */
    R0 = result;
    SET_FLAGS_ADD(R0, 0);
    RETURN_TO_CALLER();
}

/* ── battle_setPlayerNaviStatsField0E_8015BEC ── */
/* battle_setPlayerNaviStatsField0E_8015BEC(r0 = alliance flag, r1 = byte value):
 * Applies a one-byte update (offset 0x0E) to the NaviStats record of the
 * given side's player, but only if the player object exists and passes two
 * state checks (sub_801445C / sub_8014490, both taking the object in r5).
 * On every exit the flags are those left by the last executed call:
 * the early "no player" exit re-derives them from `cmp r0, #0`.
 * r0 = result of the last call (0 / check result / NaviStats pointer),
 * r1/r2 = as left by that call, r4-r6 preserved. */
Next battle_setPlayerNaviStatsField0E_8015BEC(void) {
    /* The original pushes r4-r6 and reuses them as scratch, restoring the
     * caller's values on every exit path. */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    R6 = R0; /* alliance flag, kept in r6 while calling helpers */
    R4 = R1; /* byte to store, kept in r4 */

    R0 = R6;
    GUEST_CALL(battle_findPlayer); /* -> player BattleObject, or NULL */
    if (R0 == 0) {
        SET_FLAGS_CMP(R0, 0); /* early exit: flags from `cmp r0, #0` */
        R4 = savedR4; R5 = savedR5; R6 = savedR6;
        RETURN_TO_CALLER();
    }
    R5 = R0; /* player object */

    GUEST_CALL(sub_801445C); /* state check #1 (object in r5); NZ set */
    if (R0 != 0) {
        R4 = savedR4; R5 = savedR5; R6 = savedR6;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(sub_8014490); /* state check #2 (object in r5); NZ set */
    if (R0 != 0) {
        R4 = savedR4; R5 = savedR5; R6 = savedR6;
        RETURN_TO_CALLER();
    }

    R0 = R6;
    GUEST_CALL(GetBattleNaviStatsAddr); /* -> NaviStats of this alliance */
    ST8(R0 + 0xE, R4); /* strb r4, [r0,#0xe] */

    R4 = savedR4; R5 = savedR5; R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── battle_ResetObjectActionState_80165B8 ── */
/* The disassembly names this field offset oBattleObject_CurAction; the game.h
 * headers only provide neighbouring fields, so define it here (offset 9). */

/* battle_ResetObjectActionState_80165B8(r5 = battle object):
 * Reset the object's action state: set its current action to 8 and clear the
 * current phase (and its "phase initialized" flag) to 0.
 * Returns r0 = 0 with the NZ flags reflecting that zero. */
Next battle_ResetObjectActionState_80165B8(void) {
    ST8(R5 + oBattleObject_CurAction, 8);
    R0 = 0;
    ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_applyStatusSpriteTint_80168C8 ── */
/* battle_applyStatusSpriteTint_80168C8: give the current battle object's sprite a color-shader tint
 * when it carries status flag 0x800.  The tint index is bit 1 of the value
 * returned by battle_getStateUnk40_800A704, used as a byte offset into the
 * halfword table dword_80168EC = { 0x0000, 0x03FF }
 * (0 = no tint, 0x3FF = full tint). */
Next battle_applyStatusSpriteTint_80168C8(void) {
    GUEST_CALL(object_getFlag);
    u32 statusFlags = R0;

    if ((statusFlags & 0x0800) == 0) {
        /* Flag state on this exit path: carry was cleared by the
         * "lsl r1, #8" that built the test mask (bit 24 of 0x800),
         * and N/Z come from the "tst" (clear here). */
        SET_FLAGS_NZ(statusFlags & 0x0800);
        cpu.c = 0;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_getStateUnk40_800A704);
    u32 shaderOffset = R0 & 2; /* 0 or 2: halfword selector */
    R0 = LD16(A_dword_80168EC + shaderOffset);
    GUEST_CALL(sprite_setColorShader);

    RETURN_TO_CALLER();
}

/* ── battle_objectCountdownPhaseOrReset_8016E3C ── */
/* battle_objectCountdownPhaseOrReset_8016E3C (r5 = battle object):
 * Count down the object's current phase; once the decrement wraps below zero,
 * fully reset the object: clear its flag bitfields, clear its sub-counter,
 * and hand its saved state/action word back to the state machine.
 * Returns the decremented phase in r0 (0 on the reset path). */
Next battle_objectCountdownPhaseOrReset_8016E3C(void) {
    u32 phase = LD8(R5 + oBattleObject_CurPhase);

    /* phase-- (SUBS sets N/Z/C/V for the bge below) */
    SET_FLAGS_CMP(phase, 1);
    phase -= 1;
    ST8(R5 + oBattleObject_CurPhase, phase);

    if ((s32)phase >= 0) {
        /* Original leaves the decremented phase in r0 on this path. */
        R0 = phase;
        RETURN_TO_CALLER();
    }

    /* Phase expired: reset the object. */
    R0 = 0x101000;
    GUEST_CALL(object_clearFlag);
    R0 = 0x10;
    GUEST_CALL(object_clearFlag2);

    R0 = 0;
    ST8(R5 + oBattleObject_Unk_1f, R0);
    ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized,
         LD32(R5 + oBattleObject_Unk_5c));

    /* Thumb `mov r0, #0` is MOVS, so it sets N/Z (C/V still come from
     * object_clearFlag2); the following store doesn't touch flags. */
    SET_FLAGS_NZ(R0);
    ST32(R5 + oBattleObject_Unk_5c, R0);

    RETURN_TO_CALLER();
}

/* ── collision_resetAllHitData_801986C ── */
/* collision_resetAllHitData_801986C: reset the collision system's per-hit data.
 * Clears the "active collision" bitfield, then reinitializes all 32 collision
 * data slots: each slot is zero-filled and its CollisionIndexBit is set to a
 * unique one-hot bit (0x80000000 for slot 0, 0x40000000 for slot 1, ...).
 * Finally clears the collision scratch buffer.
 *
 * Reads the toolkit pointer from r10; preserves r2-r5 and r10. */
Next collision_resetAllHitData_801986C(void) {
    ST32(A_eActiveCollisionDataBitfield, 0);

    u32 indexBit = 1u << 31;
    u32 entry = LD32(R10 + oToolkit_CollisionDataPtr);
    while (indexBit != 0) {
        R0 = entry;
        R1 = oCollisionData_Size;
        GUEST_CALL(ZeroFillByWord);
        ST32(entry + oCollisionData_CollisionIndexBit, indexBit);
        entry += oCollisionData_Size;
        indexBit >>= 1;
    }

    GUEST_CALL(clearScratchBuffer_2034F60_8019FA4);
    RETURN_TO_CALLER();
}

/* ── battle_clearInvisFlashTimer_801A648 ── */
/* battle_clearInvisFlashTimer_801A648(r5 = battle object, r10 = battle state used by battle_isPaused):
 * Tick-down helper for the "flashing invisibility" a fighter gets after
 * being hit: while the battle is unpaused, if the object's invisibility
 * flash timer is running, the hit flag (0x4) is set and the persistent
 * invisibility flag (0x1000) is not, clear the timer so the flashing stops.
 * Returns r3 = the object's collision data pointer (or the incoming r3 if
 * the battle was paused and nothing was touched). */
Next battle_clearInvisFlashTimer_801A648(void) {
    GUEST_CALL(battle_isPaused);
    if (R0 != 0)
        RETURN_TO_CALLER();

    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    u32 flashTimer = LD16(collision + oCollisionData_FlashingInvisTimer);
    u32 hitFlags = LD32(collision + oCollisionData_FlagsFromCollision);

    if (flashTimer != 0 && (hitFlags & 0x4) && !(hitFlags & 0x1000))
        ST16(collision + oCollisionData_FlashingInvisTimer, 0);

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

/* ── chatbox_setState4IfFlag80Clear_8027320 ── */
/* chatbox_setState4IfFlag80Clear_8027320: test the 0x80 bit of the chatbox event flags (eFlags2009F38).
 * If it is not set, write halfword 4 to [r5+2] (presumably a chatbox state
 * or timer field). Either way r0 holds the flag-test result and the flags
 * reflect the last comparison/move as in the original. */
Next chatbox_setState4IfFlag80Clear_8027320(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    SET_FLAGS_CMP(R0, 0);
    if (R0 == 0) {
        R0 = 4;
        ST16(R5 + 2, R0);
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_resetTextStateIfHidden_80273EC ── */
/* chatbox_resetTextStateIfHidden_80273EC(r5 = textbox object S20364C0):
 * Unless the chatbox "shown" flag (0x80 of eFlags2009F38) is set, reset the
 * textbox state: copy the stored line count/character at +0xD into the active
 * slot at +1 and clear the halfword at +2 (character timer/index).
 * Then update the textbox via sub_8029C08. */
Next chatbox_resetTextStateIfHidden_80273EC(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (R0 == 0) {
        ST8(R5 + 1, LD8(R5 + 0xD));
        ST16(R5 + 2, 0);
    }
    GUEST_CALL(sub_8029C08); /* self stays in r5 */
    RETURN_TO_CALLER();
}

/* ── chipSelect_QueueEntryIconGfx_8028284 ── */
/* chipSelect_QueueEntryIconGfx_8028284: queue the chip-icon graphics for one entry of the chip
 * selection screen.
 *   r4 -> entry struct: byte at +7 selects the source (2 = use the fixed
 *   id 0x1FF), word at +8 points to the chip's id/code halfword.
 * Calls getChipID_802A54E to map the chip halfword to an icon id, then
 * streams the icon tiles: first the chunked transfer header, then the
 * 64-byte tile chunk for id / 512. Flags on return are whatever the last
 * callee leaves, as in the original. */
Next chipSelect_QueueEntryIconGfx_8028284(void) {
    u32 entry = R4;

    u32 chipId;
    if (LD8(entry + 7) == 2) {
        /* Fixed id: this entry does not reference a real chip. */
        chipId = 0x1FF;
    } else {
        R0 = LD16(LD32(entry + 8));
        GUEST_CALL(getChipID_802A54E);
        chipId = R0;
    }

    R0 = chipId;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R0 = LD16(LD32(entry + 8));
    GUEST_CALL(getChipID_802A54E);
    R0 = R0 >> 9;
    GUEST_CALL(gfx_QueueNext64ByteChunk_8028204);

    RETURN_TO_CALLER();
}

/* ── battle_DispatchPendingEvent_802A0F8 ── */
/* battle_DispatchPendingEvent_802A0F8: check for a pending battle event and dispatch it.
 *
 * Reads the pending-event byte at byte_20352CC (field 0x4C of eStruct2035280).
 * If an event is pending, dispatches event flags 0x4000; afterwards the
 * pending-event state is always cleared via battle_ClearPendingEventFlag. */
Next battle_DispatchPendingEvent_802A0F8(void) {
    R0 = A_eStruct2035280;
    R1 = 0;
    R2 = 0x4C;                      /* byte_20352CC - eStruct2035280 */
    R1 = LD8(R0 + R2);              /* pending event byte */

    if (R1 != 0) {
        /* Event pending: dispatch it. Flags afterwards are whatever the
         * dispatch routine leaves. */
        R0 = 0x4000;
        GUEST_CALL(dispatch_801DACC);
    } else {
        /* No event: nothing to dispatch. Leave the flags as the original's
         * "tst r1, r1" (with r1 == 0) did. */
        SET_FLAGS_NZ(R1);
    }

    /* Clear the pending-event state before returning. On the dispatch path
     * R1/R2 hold whatever dispatch_801DACC left, exactly as in the original. */
    GUEST_CALL(battle_ClearPendingEventFlag_802A0EC);

    RETURN_TO_CALLER();
}

/* ── fadeScreenAndAdvanceState_802A2E8 ── */
/* fadeScreenAndAdvanceState_802A2E8: (self: *S20364C0 in r5) -> ()
 * Check some condition (chipSelect_selectedSlotChipHasAttr20_802A394); when it holds, start the 0x54/0x5C
 * screen fade, clear the state byte at +13 and set the jump offset at +12
 * to 4. Leaves N/Z from the final MOV (or from the initial comparison when
 * nothing happened); C/V come from the last callee on the active path. */
Next fadeScreenAndAdvanceState_802A2E8(void) {
    GUEST_CALL(chipSelect_selectedSlotChipHasAttr20_802A394);

    if (R0 != 0) {
        R0 = 0x54;
        R1 = 0xA;
        GUEST_CALL(SetScreenFade);

        R0 = 0x5C;
        R1 = 0xA;
        GUEST_CALL(loc_8006274);

        ST8(R5 + oS20364C0_Unk_13, 0);

        R0 = 4;
        SET_FLAGS_NZ(R0);
        ST8(R5 + oS20364C0_JumpOffset12, 4);
    } else {
        /* flags as left by the CMP r0, #0 before the branch */
        SET_FLAGS_CMP(R0, 0);
    }

    RETURN_TO_CALLER();
}

/* ── custMenu_handleInputQueuePop_802BED4 ── */
/* custMenu_handleInputQueuePop_802BED4(r5 = custom-menu object):
 * Pop the next halfword from the menu input queue and act on it.
 *  - Queue empty (value 0): run the "back out" path — set the jump offset
 *    (r5+1) to 8, which routes the next state step to sub_8026BF4 via the
 *    menu main handler, and clear the halfword at r5+2.
 *  - Otherwise: store the popped value into Unk_10, choose the hold timer
 *    for Unk_0b (1 frame when the byte at Unk_0c is set, otherwise 30
 *    frames), and set the byte at r5+3 to 4 to advance the menu state.
 * Final register/flag state (per path): R0 = 0 and nz=0 on the empty path,
 * R0 = 4 and nz=4 on the consumed path; R1 holds the Unk_0c byte there and
 * is left as queue_popHalfword_802C8D4 returned it otherwise. c/v untouched. */
Next custMenu_handleInputQueuePop_802BED4(void) {
    GUEST_CALL(queue_popHalfword_802C8D4);
    u32 value = R0;

    if (value == 0) {
        /* nothing queued: hand control back to the state dispatcher */
        ST8(R5 + 1, 8);         /* oS20364C0_JumpOffset01 */
        ST16(R5 + 2, 0);        /* oS20364C0_Unk_02 */
        R0 = 0;
        SET_FLAGS_NZ(0);
    } else {
        ST16(R5 + oS20364C0_Unk_10, value);

        /* short confirmation hold if Unk_0c says so, otherwise a long one */
        R1 = LD8(R5 + oS20364C0_Unk_0c);
        u32 hold = (R1 != 0) ? 1 : 0x1E;
        ST8(R5 + oS20364C0_Unk_0b, hold);

        R0 = 4;
        ST8(R5 + 3, 4);         /* oS20364C0_Unk_03 */
        SET_FLAGS_NZ(4);
    }
    RETURN_TO_CALLER();
}

/* ── initShuffledOrderTable42_802C1F0 ── */
/* initShuffledOrderTable42_802C1F0(r5 = state struct):
 * Fill the 0x2A-byte table at unk_2035320 with 0..0x29, shuffle it with the
 * primary RNG, then reset the progress fields of the struct r5 points to
 * (r5[0xB] = 0, r5[3] = 4). Returns with NZ from the last constant stored. */
Next initShuffledOrderTable42_802C1F0(void) {
    u32 table = A_unk_2035320;

    R7 = table;
    for (u32 i = 0; i < 0x2A; i++)
        ST8(table + i, i);

    R0 = table;
    R1 = 0x2A;
    R2 = 0x2A;
    GUEST_CALL(ShuffleByteList_PrimaryRNG);

    ST8(R5 + 0xB, 0);
    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 3, R0);
    RETURN_TO_CALLER();
}

/* ── sound_pushRequest_802CA5C ── */
/* sound_pushRequest_802CA5C(r5 = sound request source):
 * Pack the signed sound id stored at offset 6 of the struct in r5 into a
 * FIFO record for the ARM7 sound handler and push it. The record value is
 *
 *     0x40000000 | (((id << 3) + 13) & 0x1FF) << 16 | 0x15
 *
 * i.e. bit 30 flags the record as a sound request, bits 16..24 carry the
 * packed id (9 bits, as used by the ARM7 side), and the low byte 0x15
 * selects the command. The remaining record words are the fixed tag/data
 * pair 0xB200, 0, 0. R5 is only read; R4/R6/R7 are untouched. */
Next sound_pushRequest_802CA5C(void) {
    u32 id = (u32)(s32)(s8)LD8(R5 + 6);
    u32 packed = ((id << 3) + 13) & 0x1FF;

    R0 = 0x40000000 | (packed << 16) | 0x15;
    R1 = 0xB200;
    R2 = 0;
    R3 = 0;
    GUEST_CALL(fifo_pushRecord16_8009FF8);

    RETURN_TO_CALLER();
}

/* ── eStruct200A008_redrawOnDirtyFlags_802CA82 ── */
/* eStruct200A008_redrawOnDirtyFlags_802CA82:
 * Refresh handler for the eStruct at 0x200A008: whenever its unk04 field is
 * non-zero, run the redraw pair decodePackedIdFields_802C54C + sub_802CAA6, then do the same
 * for unk06. Both fields are checked independently (both may trigger).
 * r4-r7 and everything else are passed through untouched. */
Next eStruct200A008_redrawOnDirtyFlags_802CA82(void) {
    GUEST_CALL(eStruct200A008_getUnk04);
    if (R0 != 0) {
        GUEST_CALL(decodePackedIdFields_802C54C);
        GUEST_CALL(sub_802CAA6);
    }

    GUEST_CALL(eStruct200A008_getUnk06);
    if (R0 != 0) {
        GUEST_CALL(decodePackedIdFields_802C54C);
        GUEST_CALL(sub_802CAA6);
    }

    RETURN_TO_CALLER();
}

/* ── getTableEntryPtr32_802D064 ── */
/* getTableEntryPtr32_802D064(r0 = entry index) -> r0 = &byte_203C4A0[entry * 0x20]
 *
 * Address-of helper: computes the address of one 0x20-byte entry in the
 * array at byte_203C4A0. In Thumb, the low-register "add r0, r0, r1" is
 * always ADDS, so this last instruction (not the MUL) is what determines
 * the flags returned to callers: N/Z from the sum, C from the carry out
 * of the addition, V from the signed overflow. */
Next getTableEntryPtr32_802D064(void) {
    u32 product = R0 * 0x20;
    u32 base = A_byte_203C4A0;

    SET_FLAGS_ADD(product, base);  /* Thumb ADD r0, r0, r1 == ADDS */
    R0 = product + base;
    RETURN_TO_CALLER();
}

/* ── battle_initPlayerTracker_802D6A0 ── */
/* battle_initPlayerTracker_802D6A0: initialise the battle's player-tracking block (dword_203C970)
 * at the start of a battle:
 *   +0x0: counter, cleared
 *   +0x4: flag, set to 1
 *   +0x8: BattleObject pointer of the player's side   (alliance 0)
 *   +0xC: BattleObject pointer of the enemy's side    (alliance 1)
 * r4-r7/r10 are preserved; r0-r2 and flags are scratch. */
Next battle_initPlayerTracker_802D6A0(void) {
    u32 tracker = A_dword_203C970;

    ST32(tracker, 0);

    R0 = 0;                                  /* player's side */
    GUEST_CALL(battle_findPlayer);
    ST32(tracker + 8, R0);

    R0 = 1;                                  /* enemy's side */
    GUEST_CALL(battle_findPlayer);
    ST32(tracker + 0xC, R0);

    ST8(tracker + 4, 1);

    RETURN_TO_CALLER();
}

/* ── ai_CheckSpecialFlagPair_802DCEC ── */
/* ai_CheckSpecialFlagPair_802DCEC(r0 = AI/battle parameter):
 * Decides a special condition for the given parameter. Returns 0 if the
 * parameter is 0; otherwise returns 1 if the AI data +0x48 flag has bit
 * 0x1000 set or the AI data +0x44 flag has bit 0x4000000 set, else 0.
 *
 * Register notes: the original borrows r5 and r7 as scratch (r5 = parameter,
 * r7 = running result) but pops both restored, so the caller sees its incoming
 * r5/r7 unchanged. The flag getters are leaf calls that still read the live
 * scratch state: the first is entered with r1 = incoming r1, r5 = parameter,
 * r7 = 0; the second with r1 = 0x1000 (the =0x4000000 load happens only after
 * that call returns), r5 = parameter, r7 = 0. r1 ends up holding the mask of
 * the last flag tested (untouched on the parameter == 0 path), and r3 holds
 * whatever the flag getters left there.
 */
Next ai_CheckSpecialFlagPair_802DCEC(void) {
    u32 param = R0;
    u32 savedR5 = R5, savedR7 = R7;

    /* Scratch state live across the flag-getter calls */
    R5 = param;
    R7 = 0;

    u32 result;
    if (param == 0) {
        /* "beq loc_802DD0C": parameter 0 short-circuits to "no" */
        result = 0;
    } else {
        result = 1;

        /* flag at AI data + 0x48: bit 0x1000 keeps the "yes" verdict */
        GUEST_CALL(GetAIDataUnk0x48Flag);
        R1 = 0x1000;
        if ((R0 & 0x1000) == 0) {
            /* flag at AI data + 0x44: bit 0x4000000 keeps the "yes" verdict */
            GUEST_CALL(GetAIData_Unk_44_Flag); /* still called with r1 = 0x1000 */
            R1 = A_LCDControl;
            if ((R0 & A_LCDControl) == 0)
                result = 0;
        }
    }

    /* pop {r5,r7}: caller's values are restored */
    R5 = savedR5;
    R7 = savedR7;

    R0 = result;
    /* The closing "mov r0, r7" assembles to adds r0, r7, #0, the last
     * flag-setting instruction on every path: it clears C and V. */
    SET_FLAGS_ADD(R0, 0);
    RETURN_TO_CALLER();
}

/* ── startScreen_fadeOutUpdateIfFlag40_802FB90 ── */
/* startScreen_fadeOutUpdateIfFlag40_802FB90(self: *StartScreen (r5)):
 * When the start screen's 16-bit flag field has bit 0x40 set, invoke
 * sub_8001158 with the parameter table at byte_802FBA8 (an update/
 * fade routine). Flags (c/v) and r2/r3 after the call are whatever
 * sub_8001158 leaves; r4-r7 are untouched throughout. */
Next startScreen_fadeOutUpdateIfFlag40_802FB90(void) {
    if (LD16(R5 + 0xA) & 0x40) {
        R0 = A_byte_802FBA8;
        GUEST_CALL(sub_8001158);
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_dispatchIfFlag20_802FBB4 ── */
/* startScreen_dispatchIfFlag20_802FBB4(self = start screen object in R5):
 * If the screen's state halfword (offset 0xA) has flag 0x20 set, invoke
 * sub_8001158 with the address of the byte_802FBCC record (which begins
 * with the word 0x80544004, a pointer followed by padding). Otherwise
 * return immediately. */
#define SCREEN_FLAG_20 0x20u /* bit in the state halfword at +0xA */

Next startScreen_dispatchIfFlag20_802FBB4(void) {
    u16 state = LD16(R5 + 0xA);

    if (state & SCREEN_FLAG_20) {
        R0 = A_byte_802FBCC;
        GUEST_CALL(sub_8001158);
    } else {
        /* Not-taken path: TST r0, r1 is the last flag-setting instruction,
         * so leave N/Z reflecting (state & 0x20). */
        SET_FLAGS_NZ(state & SCREEN_FLAG_20);
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_playSoundIfFlagSet_802FBD8 ── */
/* startScreen_playSoundIfFlagSet_802FBD8(r5 = StartScreen):
 * Check the screen's status flags at +0xA; if bit 4 (0x10) is set, hand the
 * byte table at byte_802FBF0 to sub_8001158, which plays the associated
 * sound/voice clip. Otherwise do nothing.
 *
 * r2/r3 and r4-r7 pass through untouched; on the not-taken path C/V are left
 * exactly as the caller set them (Thumb TST only touches N/Z, which callers
 * of this function ignore). */
Next startScreen_playSoundIfFlagSet_802FBD8(void) {
    if (LD16(R5 + 0xA) & 0x10) {
        R0 = A_byte_802FBF0;
        GUEST_CALL(sub_8001158);
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_onStatusBit2_802FC28 ── */
/* startScreen_onStatusBit2_802FC28(r5 = start screen state):
 * When the start screen's status word has bit 2 set, hand the data table at
 * byte_802FC40 to sub_8001158 (which follows the pointer/params stored there).
 * Flag state on the skip path comes from the TST (N/Z from the AND result,
 * C/V untouched); on the call path it is whatever sub_8001158 leaves behind. */
Next startScreen_onStatusBit2_802FC28(void) {
    u16 status = LD16(R5 + 0xA);

    if (status & 4) {
        R0 = A_byte_802FC40;
        GUEST_CALL(sub_8001158);
    } else {
        SET_FLAGS_NZ(status & 4);
    }

    RETURN_TO_CALLER();
}

/* ── startScreen_playSfxIfFlagSet_802FC4C ── */
/* startScreen_playSfxIfFlagSet_802FC4C(r5 = StartScreen):
 * If the start screen has flagged bit 1 in its state field at +0xA, feed the
 * sound-effect parameter block at byte_802FC64 to sub_8001158 (SFX player).
 * Flags on return: NZ from the bit test if the call was skipped, otherwise
 * whatever sub_8001158 leaves behind. */
Next startScreen_playSfxIfFlagSet_802FC4C(void) {
    u16 screenFlags = LD16(R5 + 0xA);

    R0 = screenFlags;
    R1 = 2;
    SET_FLAGS_NZ(screenFlags & 2);

    if (screenFlags & 2) {
        R0 = A_byte_802FC64;   /* SFX parameter block */
        GUEST_CALL(sub_8001158);
    }

    RETURN_TO_CALLER();
}

/* ── startScreen_runTriggerHandler_802FC9C ── */
/* startScreen_runTriggerHandler_802FC9C(r5 = start screen state):
 * Start screen hook. When bit 0x200 of the state word at +0xA is set, hand the
 * small data table at byte_802FCB4 to sub_8001158 (which consumes it as a
 * parameter block). Otherwise do nothing. */
Next startScreen_runTriggerHandler_802FC9C(void) {
    enum { STARTSCREEN_TRIGGER = 0x200 };

    if (LD16(R5 + 0xA) & STARTSCREEN_TRIGGER) {
        R0 = A_byte_802FCB4;
        GUEST_CALL(sub_8001158);
    }
    RETURN_TO_CALLER();
}

/* ── camera_dispatchUpdate_8030054 ── */
/* camera_dispatchUpdate_8030054: camera mode dispatcher.
 *
 * r5 points at the camera object. The byte at offset 2 selects which
 * handler to run; it acts as a byte offset into the handler table at
 * off_8030068, whose entries are:
 *   0x00, 0x04, 0x10 -> camera_updateDerivedOffsets_803007C (default/idle camera update)
 *   0x08             -> sub_80300AC
 *   0x0C             -> sub_8030104
 *
 * The original clobbers r0/r1 before jumping: it loads the selected handler
 * address (Thumb bit set) into r0 and the selector byte into r1, and the
 * handlers are entered seeing those values. All other registers and the
 * condition flags pass through to the handler untouched, and whatever the
 * handler leaves in them is handed back to our caller.
 */
Next camera_dispatchUpdate_8030054(void) {
    u32 select = LD8(R5 + 2);

    R1 = select;
    R0 = LD32(A_off_8030068 + select); /* handler pointer, as the original loads it */

    switch (select) {
    case 0x00: /* default camera update */
    case 0x04: /* default camera update */
    case 0x10: /* default camera update */
        GUEST_CALL(camera_updateDerivedOffsets_803007C);
        break;

    case 0x08:
        GUEST_CALL(sub_80300AC);
        break;

    case 0x0C:
        GUEST_CALL(sub_8030104);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_8031122 ── */
/* sub_8031122(r0 = state object, r5 = forwarded to callee):
 * Zeroes the two counter/flag words of the state struct at dword_200F3E0
 * (fields dword_200F3F8 and dword_200F3FC), then runs sprite_worldToScreenPos_80310EC on it.
 * The original r0 argument is handed to the callee in r6; r0 comes back
 * holding the callee's result. */
Next sub_8031122(void) {
    u32 stateObj = R0;
    u32 state = A_dword_200F3E0;

    ST32(state + 0x18, 0); /* dword_200F3F8 = 0 */
    ST32(state + 0x1C, 0); /* dword_200F3FC = 0 */

    R0 = state;
    R6 = stateObj;
    GUEST_CALL(sprite_worldToScreenPos_80310EC);

    RETURN_TO_CALLER();
}

/* ── battle_setupMessageWindow_80313B0 ── */
/* battle_setupMessageWindow_80313B0: three-step battle-message/textbox setup.
 *
 * Runs the prepare, layout and finalize passes in sequence, then marks the
 * request as handled by setting bit 0 of the r12 flag word.  The caller's
 * r0, r1, r2 and r5 are preserved across the whole sequence; flags on exit
 * reflect (r12 | 1), as set by the final ORR in the original. */
Next battle_setupMessageWindow_80313B0(void) {
    u32 savedR0 = R0;
    u32 savedR1 = R1;
    u32 savedR2 = R2;
    u32 savedR5 = R5;

    /* step 1: prepare state (reads r0-r2 and the r12 flag word) */
    GUEST_CALL(sub_8031176);

    /* step 2: lay out into a 32x32 region with blank origin and one style */
    R2 = 0x20;
    R4 = 0;
    R3 = 0x20;
    R5 = 0x20;
    R6 = 1;
    GUEST_CALL(sub_8030E8C);

    /* step 3: finalize pass with r1 = 0 */
    R1 = 0;
    GUEST_CALL(camera_moveByDelta_80310B0);

    /* mark the request as handled in the caller's r12 flag word */
    R12 = R12 | 1;

    /* restore the caller's saved registers (the original's push/pop) */
    R0 = savedR0;
    R1 = savedR1;
    R2 = savedR2;
    R5 = savedR5;
    SET_FLAGS_NZ(R12);
    RETURN_TO_CALLER();
}

/* ── battle_runTextboxUpdateAndSetStateBit1_80313D4 ── */
/* battle_runTextboxUpdateAndSetStateBit1_80313D4: run the common "battle text box" update sequence and mark
 * something in the r12 flag word as done (bit 1 set).
 *
 * r0/r1/r2 hold arguments for the first callee, r5/r12 are live state that
 * must survive the whole sequence, so they are saved and restored around it.
 * Leaves r3/r4/r6/r7-r9 as the callees left them, NZ from the final ORR. */
Next battle_runTextboxUpdateAndSetStateBit1_80313D4(void) {
    u32 saved_r0 = R0, saved_r1 = R1, saved_r2 = R2, saved_r5 = R5;
    u32 flag_word = R12;

    /* First step: (reads r1 and the r12 state word) */
    GUEST_CALL(sub_8031136);

    /* Second step: called with r2 = 0x20, r3 = 0, r4 = -1, r5 = -1, r6 = 1
     * (fill/clear style parameters); r12 state word still live. */
    R2 = 0x20;
    R3 = 0;
    R4 = 0 - 1;
    R5 = 0 - 1;
    R6 = 1;
    GUEST_CALL(sub_8030F04);

    /* Third step: called with r0 = 0. Its results in r3/r4/r6 stay live. */
    R0 = 0;
    GUEST_CALL(camera_moveByDelta_80310B0);

    /* Mark the flag word: r12 |= 2, flags from the ORR. */
    R12 = flag_word | 2;
    SET_FLAGS_NZ(R12);

    /* Restore the caller's r0-r2/r5 (the original pushes/pops them). */
    R0 = saved_r0;
    R1 = saved_r1;
    R2 = saved_r2;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── sub_80313FC ── */
/* sub_80313FC: runs a three-step sequence (sub_8031176; sub_8030F7C with a fixed
 * parameter set of 0x20/0x20/0x20/1; camera_moveByDelta_80310B0 with 0) and then sets bit 1 of
 * the state word passed in r12.  The caller's r0, r1, r2 and r5 are preserved
 * across the calls; the Z/N flags reflect the updated r12 value. */
Next sub_80313FC(void) {
    u32 inR0 = R0, inR1 = R1, inR2 = R2, inR5 = R5;
    u32 state = R12;

    GUEST_CALL(sub_8031176);

    R2 = 0;
    R3 = 0x20;
    R4 = 0x20;
    R5 = 0x20;
    R6 = 1;
    GUEST_CALL(sub_8030F7C);

    R0 = 0;
    GUEST_CALL(camera_moveByDelta_80310B0);

    state |= 2;
    R12 = state;
    SET_FLAGS_NZ(R12);

    R0 = inR0;
    R1 = inR1;
    R2 = inR2;
    R5 = inR5;
    RETURN_TO_CALLER();
}

/* ── sub_8031C2A ── */
/* sub_8031C2A(r0, r5 = context):
 * Runs a short helper sequence: first battle_calcRelPosToTarget_803172E on the incoming context,
 * then sub_80315B8 with the fixed arguments (r2 = 1, r3 = 0x10, r4 = 1,
 * r5 = 0x10), and finally sub_8031B7C. The caller sees whatever registers
 * and flags sub_8031B7C leaves behind. */
Next sub_8031C2A(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 1;
    R4 = 1;
    R3 = 0x10;
    R5 = 0x10;
    GUEST_CALL(sub_80315B8);

    GUEST_CALL(sub_8031B7C);
    RETURN_TO_CALLER();
}

/* ── sub_8031C7C ── */
/* sub_8031C7C — battle helper that runs a short setup sequence:
 * 1. battle_calcRelPosToTarget_803172E with the caller's r0/r5 arguments,
 * 2. sub_80315B8 with a fixed parameter set (14, 14, 0, 16),
 * 3. sub_8031B7C to finish.
 * All results (r0, r2-r7) and the NZCV flags are whatever the last
 * callee leaves behind. */
Next sub_8031C7C(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0xE;
    R4 = 0;
    R3 = 0xE;
    R5 = 0x10;

    GUEST_CALL(sub_80315B8);
    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031C94 ── */
/* sub_8031C94: run a three-step sequence on the object passed in r0/r5.
 * Calls battle_calcRelPosToTarget_803172E first, then sub_80315B8 with a fixed parameter set
 * (r2=0, r3=0x10, r4=2, r5=2), then sub_8031B7C, which picks up whatever
 * r2/r5 sub_80315B8 left behind. r3/r4 must survive the final call.
 * Everything (registers and flags) is simply passed through to the caller. */
Next sub_8031C94(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R4 = 2;
    R3 = 0x10;
    R5 = 2;
    GUEST_CALL(sub_80315B8);

    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── SpawnCounterPinkRingMaybe_80C4C12 ── */
/* OBJECT_FLAG_PAUSE_UPDATE: bit in oObjectHeader_Flags that suspends an
 * object's normal update until something clears it. */

/* SpawnCounterPinkRingMaybe_80C4C12
 *   r1 = x, r2 = y, r3 = z, r4 = spawn params,
 *   r5 = owner BattleObject (the one that scored the counter hit),
 *   r7 = pointer that receives the new object.
 *
 * Spawns battle object type 0x5E -- the pink ring that hovers over MegaMan
 * after a counter hit -- copies the owner's alliance/direction-flip state
 * into it, links the two objects together, reports the new ring through the
 * out pointer, and pauses its updates until its script takes over. */
Next SpawnCounterPinkRingMaybe_80C4C12(void) {
    u32 owner = R5;

    /* object_spawnType1(r0 = object type, r1..r3 = x/y/z, r4 = params);
     * the new object comes back in r5. */
    R0 = 0x5E;
    GUEST_CALL(object_spawnType1);
    u32 ring = R5;
    R5 = owner; /* the callee returns in r5, but r5 is preserved for the caller */

    if (ring != 0) {
        /* inherit the owner's alliance / direction-flip halfword */
        ST16(ring + oBattleObject_AllianceAndDirectionFlip,
             LD16(owner + oBattleObject_AllianceAndDirectionFlip));

        /* link ring -> owner, and hand the ring back through the out pointer */
        ST32(ring + oBattleObject_RelatedObject1Ptr, owner);
        ST32(ring + oBattleObject_ExtraVars + 4, R7);
        ST32(R7, ring);

        ST8(ring + oBattleObject_Unk_0c, 1);

        /* set the pause-update flag on the ring's object header;
         * the original leaves the flag value in r2 */
        R2 = OBJECT_FLAG_PAUSE_UPDATE;
        ST8(ring, LD8(ring) | R2);
    }

    RETURN_TO_CALLER();
}

/* ── chargeShotChargeObject_init_80E0E10 ── */
/* chargeShotChargeObject_init_80E0E10(r5 = charge shot battle object):
 * State initializer for the charge-shot charge object: set its extra var to
 * active, advance it to the UPDATE phase, then immediately run its update step
 * (the update's final flag-setting instruction provides our nz/c/v outputs). */


Next chargeShotChargeObject_init_80E0E10(void) {
    ST32(R5 + oBattleObject_ExtraVars + 4, 1);

    R0 = CUR_STATE_UPDATE;
    ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);

    GUEST_CALL(chargeShotChargeObject_update_80E0E20);

    RETURN_TO_CALLER();
}

/* ── battle_spawnObject56LinkedToOwner_80C44A8 ── */
/* battle_spawnObject56LinkedToOwner_80C44A8(r1..r3 = spawn x/y/z, r5 = owner BattleObject):
 * Spawns battle object type 0x56 via object_spawnType1. The original pushes
 * r5 (the owner pointer) before the call, so the callee sees it as its fifth
 * (`params`) stack argument at [SP]. On success the new object is linked
 * back to the owner, the owner's AllianceAndDirectionFlip halfword is copied
 * over, and header flag bits 0x04|0x10 are set. r0 = new object pointer, or
 * NULL if the spawn failed.
 * Flags: `mov r0, r5` is really ADDS r0, r5, #0, so it sets N/Z and clears
 * C/V; on the success path the final ORRS only updates N/Z, leaving C/V at
 * those cleared values. */

/* The flags byte sits at offset 0 of the object header; provide the offset
 * locally in case the shared header does not define the symbolic name. */

Next battle_spawnObject56LinkedToOwner_80C44A8(void) {
    u32 owner = R5;

    /* Reserve the two pushed words ({lr}, {r5}); the callee reads the saved
     * owner pointer at [SP] as its params argument. */
    SP -= 8;
    ST32(SP, owner);
    ST32(SP + 4, LR);

    R0 = 0x56;
    GUEST_CALL(object_spawnType1);

    SP += 8;

    u32 spawned = R5;          /* object_spawnType1 returns its pointer in r5 */
    R0 = spawned;
    SET_FLAGS_ADD(spawned, 0); /* mov r0, r5 == ADDS r0, r5, #0: N/Z set, C=0, V=0 */
    R5 = owner;                /* pop {r5} restores the owner pointer */

    if (spawned == 0)
        RETURN_TO_CALLER();

    ST32(spawned + oBattleObject_RelatedObject1Ptr, owner);
    ST16(spawned + oBattleObject_AllianceAndDirectionFlip,
         LD16(owner + oBattleObject_AllianceAndDirectionFlip));

    /* Set header flag bits 0x04|0x10 in the new object. */
    u32 flags = LD8(spawned + oObjectHeader_Flags) | 0x14;
    R1 = flags;
    R2 = 0x14;
    ST8(spawned + oObjectHeader_Flags, flags);
    SET_FLAGS_NZ(flags);       /* ORRS updates N/Z only; C/V stay 0/0 from the MOV */
    RETURN_TO_CALLER();
}

/* ── battle_object_destroy_80C6AAC ── */
/* battle_object_destroy_80C6AAC(r5 = battle object):
 * Destroy this battle object: clear its collision region, then switch it to
 * the "destroyed" state (CUR_STATE_DESTROY, stored into the packed
 * state/action/phase field at offset 8). Leaves NZ set as the MOV of the
 * state constant does, since callers branch on the state value. */

Next battle_object_destroy_80C6AAC(void) {
    GUEST_CALL(object_clearCollisionRegion);

    R0 = CUR_STATE_DESTROY;
    SET_FLAGS_NZ(R0);
    /* str r0, [r5,#oBattleObject_CurStateActionPhaseAndPhaseInitialized] */
    ST32(R5 + 8, R0);

    RETURN_TO_CALLER();
}

/* ── battle_objectCountdownFireAttackTick_80C9CA2 ── */
/* battle_objectCountdownFireAttackTick_80C9CA2: per-tick countdown for a battle object (r5 = object).
 *
 * Decrements the object's timer; while it is still nonzero there is nothing
 * more to do this frame. Once it reaches zero the object "fires": the game
 * asks for the direction towards the player, sub_80C9CDA places the attack
 * on the panel in front of the object, and the object switches to action 4.
 *
 * The original pushes/pops r4 and r6 around the calls: sub_80C9CDA clobbers
 * r6, but callers expect this routine to preserve it (along with r4/r5/r7),
 * so both are saved and restored on the firing path as well. */

/* The CurAction field has no named macro in the headers; the disassembly
 * stores it at struct offset 9 (strb r0, [r5,#9]). */
#define oBattleObject_CurAction_sub_80C9CA2 9

Next battle_objectCountdownFireAttackTick_80C9CA2(void) {
    u32 obj = R5;
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    /* Tick the countdown (halfword field); flags come from the subtraction. */
    u32 timer = LD16(obj + oBattleObject_Timer);
    R0 = timer - 1;
    ST16(obj + oBattleObject_Timer, R0);
    SET_FLAGS_CMP(timer, 1);

    if (R0 != 0) { /* still counting down */
        RETURN_TO_CALLER();
    }

    /* Timer expired: aim at the panel between the object and the player. */
    GUEST_CALL(object_getFrontDirection); /* r0 = direction to the player */

    R0 += LD8(obj + oBattleObject_PanelX);
    R1 = LD8(obj + oBattleObject_PanelY);
    R3 = LD32(obj + oBattleObject_Z);
    R6 = LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler);

    GUEST_CALL(sub_80C9CDA); /* places the attack; r1/r3/c/v are its outputs */

    /* Attack launched: move the object into its post-attack action state. */
    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(obj + oBattleObject_CurAction_sub_80C9CA2, R0);

    /* Callees preserve r4 but not r6; restore both as the original's pop does. */
    R4 = savedR4;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── battle_objectDestroyWithRelated2Unlink_80CAC44 ── */
/* battle_objectDestroyWithRelated2Unlink_80CAC44: destroy a battle object (r5 = object to destroy).
 * Before destroying it, break the back-link from its related object: if that
 * object's header field (at offset 0) still points at this object, clear it so
 * it doesn't reference a soon-to-be-dead object. Flags and r0-r3 afterwards
 * are whatever object_genericDestroy leaves behind. */
Next battle_objectDestroyWithRelated2Unlink_80CAC44(void) {
    u32 related = LD32(R5 + oBattleObject_RelatedObject2Ptr);

    if (LD32(related) == R5)
        ST32(related, 0);

    GUEST_CALL(object_genericDestroy);
    RETURN_TO_CALLER();
}

/* ── battle_applyElementAttackModIfEnabled_80CAD14 ── */
/* battle_applyElementAttackModIfEnabled_80CAD14(r0 = flag, r5 = battle object):
 * If r0 is nonzero, load the object's element (r2), attack parameter (r4)
 * and damage-counter disable word (r6), then hand off to sub_80CAEC8 to
 * apply the elemental attack modifier. r4/r6 are callee-saved: the original
 * pushes them (with lr) and pops them after the call, so both branches leave
 * them exactly as the caller had them. Either way the Z flag from testing r0
 * is left for the caller. */
Next battle_applyElementAttackModIfEnabled_80CAD14(void) {
    if (R0 != 0) {
        /* push {r4,r6,lr}: reserve the frame so the callee sees the same
         * stack, keeping r4/r6 intact across the call. */
        u32 frame = SP - 12;
        SP = frame;
        ST32(frame, R4);
        ST32(frame + 4, R6);
        ST32(frame + 8, LR);

        R2 = LD8(R5 + oBattleObject_Element);
        R4 = LD8(R5 + 4);
        R6 = LD32(R5 + oBattleObject_DamageAndStaminaDamageCounterDisabler);
        GUEST_CALL(sub_80CAEC8);

        /* pop {r4,r6,pc}: restore the caller's r4/r6 (lr is handled by
         * RETURN_TO_CALLER). Flags are whatever the callee left. */
        R4 = LD32(frame);
        R6 = LD32(frame + 4);
        SP = frame + 12;
    } else {
        /* tst r0, r0: only nz is affected, c/v stay as the caller set them */
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── ai_attackTickDelayTimer_80EC90E ── */
/* ai_attackTickDelayTimer_80EC90E(r5 = battle object, r7 = AI attack vars):
 * Countdown for a delayed AI attack. On the first tick it starts a 10-frame
 * timer and marks the attack as pending; once the timer runs out it detaches
 * the related object, clears the AI data flag and leaves the attack state.
 * Flags and r0 reflect the decremented timer when it is still positive. */
Next ai_attackTickDelayTimer_80EC90E(void) {
    /* First activation: initialize the timer and its state byte. */
    if (LD8(R7 + 1 /* oAIAttackVars_Unk_01 */) == 0) {
        ST16(R7 + oAIAttackVars_Unk_10, 10);
        ST8(R7 + 1, 4);
    }

    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(timer, 1); /* flags of SUBS timer, #1 */
    timer = timer - 1;
    ST16(R7 + oAIAttackVars_Unk_10, timer);

    if ((s32)timer >= 0) {
        R0 = timer;
        RETURN_TO_CALLER();
    }

    /* Timer expired: detach the object we were tracking and exit. */
    ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(aiData + oAIData_Unk_68, 0);

    R0 = 0;
    R1 = aiData;
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── aiAttack_tickTimerAndExitWhenDone_80ECA0C ── */
/* aiAttack_tickTimerAndExitWhenDone_80ECA0C: tick down the AI attack timer in the object's AIAttackVars.
 * When the counter (oAIAttackVars_Unk_10) drops below zero, leave the attack
 * state. Flags reflect the decrement (SUBS r0, #1) when the callee is not
 * taken; otherwise whatever object_exitAttackState leaves is fine. */
Next aiAttack_tickTimerAndExitWhenDone_80ECA0C(void) {
    u32 count = LD16(R7 + oAIAttackVars_Unk_10);
    u32 remaining = count - 1;

    SET_FLAGS_CMP(count, 1);
    ST16(R7 + oAIAttackVars_Unk_10, (u16)remaining);
    R0 = remaining;

    if ((s32)remaining < 0)
        GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── sub_80FDF4C ── */
/* sub_80FDF4C: fetch the address of the dword_80FDF5C work area and pass it
 * to sub_800FE28, which does the real work. Its results (r0, r2) are ours. */
Next sub_80FDF4C(void) {
    R0 = A_dword_80FDF5C;
    GUEST_CALL(sub_800FE28);
    RETURN_TO_CALLER();
}

/* ── sub_80FDF84 ── */
/* sub_80FDF84: calls the helper sub_800FE28 with the constant data table
 * dword_80FDF94 (a single word, 0x02020202) as its argument; the helper's
 * results in r0/r2 are passed straight through to the caller. */
Next sub_80FDF84(void) {
    R0 = A_dword_80FDF94; /* &dword_80FDF94 (== 0x080FDF94, .word 0x02020202) */
    GUEST_CALL(sub_800FE28);
    RETURN_TO_CALLER();
}

/* ── sub_80FDF98 ── */
/* sub_80FDF98: call sub_800FE28 with a pointer to the ROM constant
 * dword_80FDFA8 (word 0x181E242A); returns sub_800FE28's result in r0. */
Next sub_80FDF98(void) {
    R0 = A_dword_80FDFA8;
    GUEST_CALL(sub_800FE28);
    RETURN_TO_CALLER();
}

/* ── battle_aiTickAttackTimer_80FE1FE ── */
/* battle_aiTickAttackTimer_80FE1FE: AI attack driver for one oAIAttackVars block (r7).
 *
 * The byte at offset oAIAttackVars_Unk_01 (offset 1) is the attack's armed
 * flag:
 *  - 0: the attack hasn't been started yet. Arm it: store 4 into the flag
 *       and copy the attack's charge time (Unk_18) into the countdown Unk_10.
 *  - nonzero: count Unk_10 down by one; once it drops to zero (or wraps),
 *       call sub_801171C to actually perform the attack.
 *
 * The caller inspects r0 and the nz/c/v flags afterwards, so they are left
 * exactly as the original instruction stream leaves them; r5/r6/r7 pass
 * through untouched. */
Next battle_aiTickAttackTimer_80FE1FE(void) {
    if (LD8(R7 + 1) == 0) {
        /* start-up: arm the attack and load its countdown */
        ST8(R7 + 1, 4);
        u32 timer = LD16(R7 + oAIAttackVars_Unk_18);
        ST16(R7 + oAIAttackVars_Unk_10, timer);
        SET_FLAGS_NZ(4);        /* flags of the original's `mov r0,#4` */
        R0 = timer;             /* r0 ends holding the value written to Unk_10 */
    } else {
        /* armed: tick the countdown */
        u32 left = LD16(R7 + oAIAttackVars_Unk_10);
        u32 timer = left - 1;
        ST16(R7 + oAIAttackVars_Unk_10, timer);
        SET_FLAGS_CMP(left, 1); /* flags of the original's `subs r0,#1` */
        R0 = timer;
        if ((s32)timer <= 0)    /* bgt not taken: countdown expired */
            GUEST_CALL(sub_801171C);
    }
    RETURN_TO_CALLER();
}

/* ── aiAttackDispatchHandler_80FE452 ── */
/* aiAttackDispatchHandler_80FE452: AI attack command dispatcher.
 *
 * r7 points to the AI attack state block. The byte at offset 0
 * (oAIAttackVars_Unk_00) indexes the handler table at off_80FE464:
 *     entry 0 -> sub_80FE46C
 *     entry 1 -> sub_80FE4AC
 * Note the original indexes with the raw byte (ldr r1, [r1, r0]), so a
 * selector of 4 selects the second entry; in practice the field only ever
 * holds one of those two values.
 *
 * The handler is entered with r0 = selector byte and r1 = the table word
 * that was jumped through (the original never clears r1 after the load),
 * so both are set explicitly here. The call runs, returns to this wrapper,
 * and this function then returns to its own caller with r7 restored
 * (push/pop {r7} in the original). Everything else, including the flags,
 * passes through the handler untouched. */
Next aiAttackDispatchHandler_80FE452(void) {
    u32 selector = LD8(R7);              /* byte at oAIAttackVars_Unk_00 (offset 0) */

    R0 = selector;
    if (selector == 0) {
        R1 = LD32(A_off_80FE464);        /* .word sub_80FE46C+1 */
        GUEST_CALL(sub_80FE46C);
    } else {
        R1 = LD32(A_off_80FE464 + 4);    /* .word sub_80FE4AC+1 */
        GUEST_CALL(sub_80FE4AC);
    }

    RETURN_TO_CALLER();
}

/* ── battle_object_onAction8EndClearExtraVars_8109FA2 ── */
/* battle_object_onAction8EndClearExtraVars_8109FA2(r5 = battle object):
 * When an object has just finished action 8 and still holds extra variable
 * state, run the two cleanup handlers on the ExtraVars[1] pointer, then
 * clear the extra-vars pointer and Param4.
 * Leaves r0 = the last value compared on the early-exit paths (CurAction,
 * or ExtraVars when it was zero), and r0/r1/r3 from the last callee
 * otherwise. Flags match the original's last compare/mov. */
Next battle_object_onAction8EndClearExtraVars_8109FA2(void) {
    /* r0 = oBattleObject_CurAction (offset 9); early out unless action 8 */
    R0 = LD8(R5 + 9);
    if ((s32)R0 != 8) {
        SET_FLAGS_CMP(R0, 8);
        RETURN_TO_CALLER();
    }

    /* Must still be tracking extra vars */
    R0 = LD32(R5 + oBattleObject_ExtraVars);
    if (R0 == 0) {
        SET_FLAGS_CMP(R0, 0);
        RETURN_TO_CALLER();
    }

    /* Free/release whatever the extra vars point at, then delete the
     * object list it references (the pointer is reloaded in case the
     * first call modified it). */
    R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
    GUEST_CALL(sub_800AA40);

    R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
    GUEST_CALL(list_tryAddUniqueEntry_800AA1A);

    /* Clear the extra-vars pointer and Param4 (offset 7) */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    ST32(R5 + oBattleObject_ExtraVars, R0);
    ST8(R5 + 7 /* oBattleObject_Param4 */, R0);

    RETURN_TO_CALLER();
}

/* ── t3_0x16_80C6B40 ── */
/* t3_0x16_80C6B40 — per-frame update for battle object type 3, id 0x16.
 *
 * Runs the handler selected by the object's current state (r5 = battle
 * object), then refreshes the object's sprite. The dispatch table at
 * off_80C6B58 is indexed by the raw state byte; because the table holds
 * 32-bit entries and there is no shift before the load, the state byte
 * stores word offsets:
 *   0 -> sub_80C6B64            (state handler 0)
 *   4 -> sub_80C6C14            (state handler 1)
 *   8 -> object_genericDestroy  (final state: remove the object)
 *
 * The handler is entered with r0 = state byte and r1 = its own table
 * entry; r2-r12 pass through untouched. The original calls it with
 * mov lr, pc / bx r1, so control comes back right after the bx — the
 * state handler returns here rather than tail-calling. Afterwards
 * object_updateSprite finishes the frame using whatever the handler
 * left in the argument registers.
 */
Next t3_0x16_80C6B40(void) {
    u32 state = LD8(R5 + 8 /* oBattleObject_CurState */);

    R0 = state;
    R1 = LD32(A_off_80C6B58 + state); /* handler address from the table */

    switch (state) {
    case 0:
        GUEST_CALL(sub_80C6B64);
        break;
    case 4:
        GUEST_CALL(sub_80C6C14);
        break;
    case 8:
        GUEST_CALL(object_genericDestroy);
        break;
    default:
        /* Unreachable in practice: the original would jump through a
         * garbage table entry here and crash. */
        break;
    }

    /* The state handler has returned; refresh the sprite. */
    GUEST_CALL(object_updateSprite);

    RETURN_TO_CALLER();
}

/* ── t3_0xd3_80DFE40 ── */
/* t3_0xd3_80DFE40(r5 = battle object):
 * Runs the object's current state handler, then refreshes its sprite.
 * The state byte doubles as a byte offset into the handler table (states are
 * stored pre-multiplied by 4: 0/4/8 select the three entries), so no shift
 * is applied when indexing. r0/r1 hold the state and the handler's own table
 * entry when the handler is entered; r0 afterwards is whatever the handler
 * and object_updateSprite leave behind. */
Next t3_0xd3_80DFE40(void) {
    u32 state = LD8(R5 + 8);               /* oBattleObject_CurState */

    R0 = state;                            /* handlers receive the state index */
    R1 = LD32(A_off_80DFE58 + state);      /* ...and their own table entry */

    switch (state) {
    case 0 * 4: GUEST_CALL(sub_80DFE64);          break; /* alive/normal */
    case 1 * 4: GUEST_CALL(sub_80DFEA8);          break; /* second phase */
    case 2 * 4: GUEST_CALL(object_genericDestroy); break; /* destroyed */
    }

    GUEST_CALL(object_updateSprite);

    RETURN_TO_CALLER();
}

/* ── InitializeT1BattleObjectStructs ── */

/* InitializeT1BattleObjectStructs(r2, r3, r4, r7 = args passed through):
 * reset all battle object structs of type T1_BATTLE_OBJECT. */
Next InitializeT1BattleObjectStructs(void) {
    R0 = T1_BATTLE_OBJECT;
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── clearBattleObjectLinkedLists_800318C ── */
/* clearBattleObjectLinkedLists_800318C: reset the battle object linked lists
 * to an empty state before a battle is set up.
 *
 * The battle objects form a doubly linked list with a sentinel node:
 *   - eBattleObjectsLinkedListStart holds first / last pointers (0 / sentinel),
 *   - eBattleObjectsLinkedListSentinel_Prev is the sentinel, pointing back to
 *     the start block and having a NULL next pointer.
 * eUnkBattleObjectLinkedList is cleared to NULL as well.
 * Leaves R2 = list head address, R3 = 0 (as the original does). */
Next clearBattleObjectLinkedLists_800318C(void) {
    u32 start = A_eBattleObjectsLinkedListStart;      /* list head block */
    u32 sentinel = A_eBattleObjectsLinkedListSentinel_Prev; /* sentinel node */
    u32 unk = A_eUnkBattleObjectLinkedList;

    R2 = unk;
    R3 = 0;

    ST32(unk, R3);        /* eUnkBattleObjectLinkedList = NULL */
    ST32(start, R3);      /* list start: first object = NULL */
    ST32(start + 4, sentinel); /* list start: last = sentinel */
    ST32(sentinel, start);     /* sentinel: prev = list start */
    ST32(sentinel + 4, R3);    /* sentinel: next = NULL */

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

/* ── clear_flags32_20093A4 ── */
/* clear_flags32_20093A4(r0 = mask): clear the bits set in mask from the game-wide
 * flags32 word, leaving the updated word in r2. */
Next clear_flags32_20093A4(void) {
    u32 *flags = (u32 *)A_flags32_20093A4;

    R2 = LD32((u32)flags) & ~R0;
    ST32((u32)flags, R2);

    RETURN_TO_CALLER();
}

/* ── reset_flags32_20093A4 ── */
/* reset_flags32_20093A4: clear the flags32_20093A4 dword (a 32-bit game flag word). */
Next reset_flags32_20093A4(void) {
    ST32(A_flags32_20093A4, 0);
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

/* ── set_flags32_20093A4 ── */
/* set_flags32_20093A4(r0 = bits): set the given bits in the global flags word
 * at flags32_20093A4 (read-modify-write). R2 receives the updated word. */
Next set_flags32_20093A4(void) {
    u32 flags = LD32(A_flags32_20093A4);
    flags |= R0;
    ST32(A_flags32_20093A4, flags);
    R2 = flags;
    RETURN_TO_CALLER();
}

/* ── gameState_beginEndingSequence_800596C ── */
/* gameState_beginEndingSequence_800596C (self: *GameState in r5): begin the ending sequence.
 *
 * Sets the two Gregar-story event flags, starts a screen fade, ticks the
 * countdown timer table, then hands control to subsystem 0x10 of the game
 * state machine (which drives sub_800536E via cbGameState_80050EC).
 *
 * Flag contract: NZ comes from the final `movs r0, #0x10` (R0 = 0x10, so
 * N=0 Z=0); C and V are whatever timer_tickCountdownTable_8035738 left
 * behind.  The subsystem index lives at offset 0 of GameState, so the
 * store goes straight through the r5 pointer. */
Next gameState_beginEndingSequence_800596C(void) {
    /* EVENT_1703: flag group 0x17, bit 3 */
    R0 = 0x17;
    R1 = 3;
    GUEST_CALL(SetEventFlagFromImmediate);

    /* EVENT_1738: flag group 0x17, bit 0x38 */
    R0 = 0x17;
    R1 = 0x38;
    GUEST_CALL(SetEventFlagFromImmediate);

    /* fade the screen out (fade kind 0xC, speed 0x10) */
    R0 = 0xC;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    GUEST_CALL(timer_tickCountdownTable_8035738);

    /* switch the game state machine over to the ending subsystem */
    R0 = 0x10;
    SET_FLAGS_NZ(R0);
    ST8(R5, R0);

    RETURN_TO_CALLER();
}

/* ── battle_spawnEnemyObject_800768C ── */
/* battle_spawnEnemyObject_800768C(r0 = enemy idx, r1 = panel x, r2 = panel y, r3 = alliance,
 * r5 = extra argument) -> BattleObject pointer in r0.
 *
 * This is only the frame-setting entry stub of the battle-object spawner:
 * it saves r4-r7/LR, reserves the local frame shared with the spawner body,
 * clears the local at stack_800768C_unk_10, then tail-branches into the
 * shared body at loc_80076A8 (a second entry point inside the spawner).
 * The shared body pops the frame saved here and returns straight to our
 * caller, so control never comes back to this stub. */
Next battle_spawnEnemyObject_800768C(void) {
    /* push {r4-r7,lr} */
    u32 saved = SP -= 20;
    ST32(saved + 0x00, R4);
    ST32(saved + 0x04, R5);
    ST32(saved + 0x08, R6);
    ST32(saved + 0x0C, R7);
    ST32(saved + 0x10, LR);

    /* sub sp, sp, #0x14 — locals: enemy idx / panel x / panel y / alliance
     * / running result; the saved registers sit just above this frame. */
    SP -= 0x14;

    /* movs r4, #0 — also the last flag-setting write before the branch */
    R4 = 0;
    SET_FLAGS_NZ(R4);

    /* str r4, [sp, #stack_800768C_unk_10] — clear the shared local */
    ST32(SP + stack_800768C_unk_10, R4);

    /* b loc_80076A8 — tail-branch into the shared spawner body */
    GUEST_CALL(loc_80076A8);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchPhase_8007978 ── */
/* battle_dispatchPhase_8007978: battle phase dispatcher.
 *
 * R5 points to the battle state; the byte at offset 2 holds the current
 * phase, already scaled by 4 so it indexes the handler table at
 * off_800798C directly (0x00/0x04/0x08/0x0C). The computed jump leaves
 * R0 = phase byte and R1 = the handler's own address (an artifact of
 * loading the jump target into R1), so both are set before the call.
 * Every other register passes through untouched, and the flags that come
 * back are whatever the handler last set. */
Next battle_dispatchPhase_8007978(void) {
    u32 phase = LD8(R5 + 2);

    R0 = phase;
    R1 = LD32(A_off_800798C + phase); /* handler address, exactly as the table holds it */

    switch (phase) {
    case 0x00: /* cue something, then set the battle state to 4 */
        GUEST_CALL(battle_cueThenSetState4_800799C);
        break;
    case 0x04:
        GUEST_CALL(battle_state_endPhaseWithEffect_80079A8);
        break;
    case 0x08:
        GUEST_CALL(battle_state_finishBattlePhase_80079d0);
        break;
    case 0x0C:
        GUEST_CALL(battle_RevealFieldColumnsStep_80079F0);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_state_endPhaseWithEffect_80079A8 ── */
/* battle_state_endPhaseWithEffect_80079A8: finish the current battle phase for the battle state in R5.
 *
 * Reads the active battle-effect bitfield; bit 3 (overlay effect) means the
 * result byte comes from sub_803DD60, otherwise no effect is recorded. The
 * byte is written to the battle state and to eCamera.unk_4C (0x20099CC), the
 * battle scene is refreshed via sub_800B144, and finally the battle state's
 * phase byte is set to 8, which battle_dispatchPhase_8007978 picks up to run
 * battle_state_finishBattlePhase.
 *
 * inputs:  r2 r3 r4 r5 (battle state ptr) r6 r7 r10 lr
 * outputs: nz c v r0 r1 r2 r3 r4 r5 r6 r7 lr
 *          nz from the final "mov r0,#8"; c/v are whatever the guest calls
 *          leave behind (untouched on this level).
 * preserved: r4 r5 r6 r7
 */
Next battle_state_endPhaseWithEffect_80079A8(void) {
    u32 effect;

    /* Which battle effects are active? Bit 3 signals a special result. */
    GUEST_CALL(GetBattleEffects);            /* -> R0 = effect bitfield */

    if (R0 & 8)
        GUEST_CALL(sub_803DD60);             /* -> R0 = effect byte */
    else
        R0 = 0;                              /* no effect recorded */

    effect = R0;

    /* Record the effect byte in the battle state and in the camera. */
    ST8(R5 + oBattleState_Unk_0d, effect);
    R1 = 0x20099CC;                          /* eCamera + 0x4C (eCamera.unk_4C) */
    ST8(R1, effect);

    /* Refresh the battle scene / camera state. */
    GUEST_CALL(sub_800B144);

    /* Mark the battle phase as finished (phase 8; MOV sets NZ). */
    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 2, R0);

    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_dispatchJumpOffset00_8007BF0 ── */
/* eStruct200BC30_dispatchJumpOffset00_8007BF0: dispatch to an e-struct jump handler.
 *
 * eStruct200BC30_getJumpOffset00 decides which handler applies and returns a
 * byte offset into the jump table at off_8007C04; this routine then jumps to
 * the selected entry. The `mov lr, pc / bx r0 / pop {pc}` sequence is just a
 * tail call: the handler returns straight to eStruct200BC30_dispatchJumpOffset00_8007BF0's own caller, so
 * every register (and the flags) the handler leaves behind are this
 * routine's results.
 *
 * Table (off_8007C04): entry 0 -> battle_stepSubstateToState4_8007C14, entry 1 -> sub_8007C50,
 *                      entry 2 -> sub_8007C50, entry 3 -> battle_stepSubstateToState4_8007C14.
 */
Next eStruct200BC30_dispatchJumpOffset00_8007BF0(void) {
    /* Ask the e-struct which handler applies; leaves the table offset in R0. */
    GUEST_CALL(eStruct200BC30_getJumpOffset00);

    /* Pick the handler address out of the table. The original loads the
     * table base into R1 and the selected entry into R0 before jumping, and
     * both are live for the callee, so reproduce that register state. */
    u32 handler = LD32(A_off_8007C04 + R0);
    R0 = handler;
    R1 = A_off_8007C04;

    /* The table only ever holds the two Thumb entry points, and entries 0
     * and 3 are both battle_stepSubstateToState4_8007C14+1, so matching against the first entry
     * separates battle_stepSubstateToState4_8007C14 from sub_8007C50. */
    if (handler == LD32(A_off_8007C04))
        GUEST_CALL(battle_stepSubstateToState4_8007C14);
    else
        GUEST_CALL(sub_8007C50);

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

/* ── warp_setSubsystemIndexTo0x14AndOthers_8005f14 ── */
/* warp_setSubsystemIndexTo0x14AndOthers_8005f14:
 * Warp-side hook. Runs the pre-warp routine, points the current game state
 * at subsystem 0x14 (the state the warp hands control to next), then runs
 * the follow-up routine. r4-r7 are preserved for the caller; r0-r3 come out
 * as timer_tickCountdownTable_8035738 left them.
 *
 * The pre-warp and follow-up routines are called through the guest, so the
 * saved registers are stashed in C locals (mirroring the original's
 * push {r4-r7,lr} / pop {r4-r7,pc}) rather than trusting the callees. */

/* GameState+0x00 holds the index of the subsystem the main loop should run
 * next. The disassembly names this field oGameState_SubsystemIndex; define
 * it locally if the shared header does not already provide it. */

Next warp_setSubsystemIndexTo0x14AndOthers_8005f14(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* pre-warp setup */
    GUEST_CALL(warp_ArmPendingTransition_8005EEC);

    /* queue subsystem 0x14 in the game state */
    R4 = LD32(R10 + oToolkit_GameStatePtr);
    R0 = 0x14;
    ST8(R4 + oGameState_SubsystemIndex, R0);

    /* hand control to the new subsystem */
    GUEST_CALL(timer_tickCountdownTable_8035738);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── GetCurPETNavi ── */
/* GetCurPETNavi: () -> u8
 * Returns the navi currently active on the PET. As a side effect, r3 is
 * left holding the game state pointer, which some callers make use of. */
Next GetCurPETNavi(void) {
    u32 *gameState = (u32 *)LD32(R10 + oToolkit_GameStatePtr);

    R3 = (u32)gameState;
    R0 = LD8((u32)gameState + 1); /* + oGameState_CurPETNavi */

    RETURN_TO_CALLER();
}

/* ── GetRNG ── */
/* GetRNG (0x0800151C): advance the primary RNG seed.
 * new seed = ((seed << 1) + (seed >> 31) + 1) ^ 0x873CA9E5, written back to
 * ePrimaryRngSeed. Leaves r0 = new seed, r1 = XOR constant, r2 = seed << 1,
 * r3 = seed >> 31; r7 is preserved (the original only borrows it as the
 * seed pointer behind its push/pop). Flags: n/z from the final EOR,
 * c/v from the "+1" add (the EOR leaves them untouched). */
#define RNG_SEED_XOR 0x873CA9E5u

Next GetRNG(void) {
    u32 seedPtr = A_ePrimaryRngSeed;

    u32 seed = LD32(seedPtr);             /* ldr r0, [r7] */

    u32 doubled = seed << 1;              /* lsl r2, r0, #1 */
    u32 topBit  = seed >> 31;             /* lsr r3, r0, #31 */
    u32 sum     = doubled + topBit;       /* add r0, r2, r3 (flags overwritten below) */
    u32 bumped  = sum + 1;                /* add r0, #1 */
    SET_FLAGS_ADD(sum, 1);

    u32 next = bumped ^ RNG_SEED_XOR;     /* eor r0, r1 */
    SET_FLAGS_NZ(next);

    ST32(seedPtr, next);                  /* str r0, [r7] */

    R0 = next;
    R1 = RNG_SEED_XOR;
    R2 = doubled;
    R3 = topBit;
    RETURN_TO_CALLER();
}

#undef RNG_SEED_XOR

/* ── GetRNGSecondary ── */
/* GetRNGSecondary: advance the secondary RNG seed (eSecondaryRngSeed) by one step:
 *   seed = ((seed << 1) + (seed >> 31) + 1) ^ 0x873CA9E5
 * The new seed is left in r0; r1-r3 hold intermediates from the original's
 * register allocation. r7 (and lr) are preserved. */
Next GetRNGSecondary(void) {
    u32 seed = LD32(A_eSecondaryRngSeed);
    u32 doubled = seed << 1;   /* lsl r2, r0, #1 */
    u32 carry   = seed >> 31;  /* lsr r3, r0, #31 */
    u32 next    = doubled + carry;

    SET_FLAGS_ADD(next, 1);    /* adds r0, #1 — sets c/v */
    next = next + 1;
    next ^= 0x873CA9E5;        /* eor r0, r1 — sets n/z, leaves c/v */
    SET_FLAGS_NZ(next);

    ST32(A_eSecondaryRngSeed, next);

    R0 = next;
    R1 = 0x873CA9E5;
    R2 = doubled;
    R3 = carry;
    RETURN_TO_CALLER();
}

/* ── GetRenderInfoLCDControl ── */
/* GetRenderInfoLCDControl: read the LCD control value (u16) that the render
 * info block tracks; the toolkit pointer comes in via r10.
 * (Offset 8 in the toolkit struct is its RenderInfo pointer; field 0x00 of
 * RenderInfo is the LCD control halfword.) */
Next GetRenderInfoLCDControl(void) {
    u32 renderInfo = LD32(R10 + 8 /* oToolkit_RenderInfoPtr */);

    R0 = LD16(renderInfo + 0 /* oRenderInfo_Unk_00 */);
    RETURN_TO_CALLER();
}

/* ── SeedRNG ── */
/* SeedRNG: reset the primary RNG seed to its fixed initial value. */
Next SeedRNG(void) {
    ST32(A_ePrimaryRngSeed, 0xA338244F);
    RETURN_TO_CALLER();
}

/* ── SetRenderInfoLCDControl ── */
/* SetRenderInfoLCDControl(r0 = LCD control value, r10 = toolkit):
 * store the halfword into the render info's LCD control field.
 * Returns the render info pointer in r1. */

/* offset of the render-info pointer within the toolkit struct */
#define oToolkit_RenderInfoPtr 8

Next SetRenderInfoLCDControl(void) {
    u32 renderInfo = LD32(R10 + oToolkit_RenderInfoPtr);
    ST16(renderInfo, R0);
    R1 = renderInfo;
    RETURN_TO_CALLER();
}

/* ── ZeroFillByByte ── */
/* ZeroFillByByte(r0 = destination, r1 = byte count): fill the given range
 * with zero bytes, one byte per iteration, walking backwards from the end.
 * r0-r2 and LR are preserved; the flags left behind are those of the final
 * loop-counter decrement (SUBS r1, #1), which callers may branch on. */
Next ZeroFillByByte(void) {
    u32 dest = R0;
    u32 count = R1;

    do {
        /* SUBS count, #1: decrement first, and leave the flags from this
         * subtraction behind (they are what the caller sees on exit). */
        SET_FLAGS_CMP(count, 1);
        count -= 1;
        ST8(dest + count, 0);
    } while (count != 0);

    RETURN_TO_CALLER();
}
