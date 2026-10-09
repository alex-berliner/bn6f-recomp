/*
 * leaves_farm_24.c: small functions from the model farm, set 24.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

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

/* ── getBattleArmPositionMaybe_8018810 ── */
/* getBattleArmPositionMaybe_8018810(r0 = arm id, r1 = arm index, r2/r3 = battle object):
 * Computes one coordinate pair of an arm/part attachment point on a battle
 * object, mirrored to match the object's facing direction.
 *   - Ids 0xCD..0xFF are not a real arm slot: returns the fixed pair (0, 7).
 *   - Otherwise: sub_800F26C fetches the lookup base for the object pair,
 *     getIndexedSignedBytePair_8018842 reads the signed (x, y) byte offsets
 *     for the given index, and object_getFlipDirection gives the mirror factor.
 * Returns r0 = x * flip, r1 = y, r2 = flip direction. r4-r6 preserved. */
Next getBattleArmPositionMaybe_8018810(void) {
    u32 id = R0, index = R1, objA = R2, objB = R3;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    if ((s32)id >= (s32)0xCD && (s32)id <= (s32)0xFF) {
        /* Invalid slot id: fixed default position */
        R0 = 0;
        R1 = 7;
    } else {
        /* Callees receive the object args again in r4-r6 */
        R4 = index;
        R5 = objA;
        R6 = objB;

        GUEST_CALL(sub_800F26C);
        u32 base0 = R0, base1 = R1;

        /* r2 = index (r4 still holds it; sub_800F26C preserves r4) */
        R2 = R4;
        GUEST_CALL(getIndexedSignedBytePair_8018842);
        u32 offsetX = R0, offsetY = R1;

        R0 = objA;
        R1 = objB;
        GUEST_CALL(object_getFlipDirection);
        u32 flip = R0;

        R0 = offsetX * flip;
        R1 = offsetY;
        R2 = flip;

        R4 = savedR4;
        R5 = savedR5;
        R6 = savedR6;
    }
    RETURN_TO_CALLER();
}

/* ── object_createCollisionData ── */
/* object_createCollisionData: 08019892-080198CE THUMB
 *
 * Allocates a collision-data slot for the battle object at r5. Scans
 * eActiveCollisionDataBitfield from bit 31 down for the first free slot,
 * claims it, zeroes the slot, marks it active and stores its address in
 * the object.
 *
 * inputs:  r5 = battle object, r10 = toolkit (r4 preserved, unused)
 * outputs: r0 = slot address (0 if all 32 slots are taken), r1, r2, r3,
 *          c, v; r4/r5/r10 preserved.
 */
Next object_createCollisionData(void) {
    u32 slotBase = LD32(R10 + oToolkit_CollisionDataPtr);
    u32 bitfieldAddr = A_eActiveCollisionDataBitfield;
    u32 used = LD32(bitfieldAddr);

    /* find the first free slot; each slot is 0xA8 bytes */
    u32 mask = 1u << 31;
    while (used & mask) {
        slotBase += 0xA8;
        mask >>= 1;
        if (mask == 0) {
            /* no free slot: return 0. Flags are those left by the last
             * `lsr r1, r1, #1` (result 0, carry out of the final bit):
             * Z=1, C=1, N=0, V=0. */
            R0 = 0;
            R1 = 0;
            R2 = used;
            R3 = bitfieldAddr;
            SET_FLAGS_CMP(0, 0);
            RETURN_TO_CALLER();
        }
    }

    /* claim the slot */
    used |= mask;
    ST32(bitfieldAddr, used);
    R2 = used;
    R3 = bitfieldAddr;

    /* clear the fixed part and the per-hit part of the slot */
    R0 = slotBase;
    R1 = 0x44;
    GUEST_CALL(ZeroFillByWord);

    R0 = slotBase + 0x48;
    R1 = 0x60;
    GUEST_CALL(ZeroFillByWord);

    /* mark the slot active and link it to the object */
    ST8(slotBase, 1);
    ST32(R5 + oBattleObject_CollisionDataPtr, slotBase);

    R0 = slotBase;
    R1 = 1;
    /* the original ends with `adds r0, r4, #0` before the two stores */
    SET_FLAGS_ADD(slotBase, 0);
    RETURN_TO_CALLER();
}

/* ── object_presentCollisionData ── */
/* object_presentCollisionData (r5 = battle object, r1 = new collision flags, r3 = context):
 * Called when an object is present in a collision. Refreshes the object's
 * collision data block: records the new collision flags, clears one-shot hit
 * state unless time is currently stopped, wipes the per-object interaction
 * block, and finally runs the shared collision handler (sub_300777C) on the
 * collision data.
 * Preserves r4-r7; returns with r0-r3 as left by sub_300777C. */
Next object_presentCollisionData(void) {
    u32 savedR4 = R4;
    u32 object = R5;
    u32 newFlags = R1;

    u32 coll = LD32(object + oBattleObject_CollisionDataPtr);
    R4 = coll; /* callees see r4 = collision data, as in the original */

    /* Merge the flags still pending in the collision data with the ones the
     * current collision produced, then store the new flags passed in r1. */
    u32 flagsFromCollision = LD32(coll + oCollisionData_FlagsFromCollision);
    u32 mergedFlags = LD32(coll + oCollisionData_Unk_54) | flagsFromCollision;
    ST32(coll + oCollisionData_Unk_54, newFlags);

    R0 = mergedFlags;
    R2 = flagsFromCollision;
    GUEST_CALL(battle_isTimeStop);

    if (R0 == 0) {
        /* Time is not stopped: drop the hit modifier, its follow-up byte and
         * the pending collision flags. */
        ST8(coll + oCollisionData_HitModifierFinal, 0);
        ST8(coll + 3, 0);
        ST32(coll + oCollisionData_Unk_54, 0);
    }

    /* Always clear the resolved status effect and zero the object's
     * interaction block (0x40 halfwords = 128 bytes at coll+0x68). */
    ST8(coll + oCollisionData_StatusEffectFinal, 0);
    R0 = coll + 0x68;
    R1 = 0x40;
    GUEST_CALL(ZeroFillByHalfword);

    /* Invoke the collision handler on the (reloaded, possibly updated)
     * collision data pointer. */
    R0 = LD32(object + oBattleObject_CollisionDataPtr);
    GUEST_CALL(sub_300777C);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── object_updateCollisionData ── */
/* object_updateCollisionData(r1 = own element, r2 = opposing element, r3 = hit modifier,
 *   r5 = battle object): refresh the object's collision record — store the hit modifier
 *   and pending damage, look up both sides' collision type flags for the object's
 *   alliance — then re-evaluate it via sub_8019F44. While time is stopped, bit 16 is
 *   OR-ed into the record's own collision type flags (and left in r1 for the callee). */
Next object_updateCollisionData(void) {
    u32 self = R5;              /* battle object whose collision data is refreshed */
    u32 ownElement = R1;        /* element key for the object's own collision entry */
    u32 otherElement = R2;      /* element key for the opposing collision entry */
    u32 inR4 = R4, inR7 = R7;   /* saved/restored by the original's push/pop */

    u32 col = LD32(self + oBattleObject_CollisionDataPtr);
    R7 = col;

    ST8(col + oCollisionData_HitModifierBase, R3);
    R3 = LD16(self + oBattleObject_Damage);
    ST16(col + oCollisionData_SelfDamage, R3);

    /* collision type flag lookup: (alliance, element) */
    R0 = LD8(self + oBattleObject_Alliance);
    R1 = ownElement;
    GUEST_CALL(object_getTableEntry_801A0BA);
    ST32(col + oCollisionData_SelfCollisionTypeFlags, R0);

    R0 = LD8(self + oBattleObject_Alliance);
    R1 = otherElement;
    GUEST_CALL(object_getTableEntry_801A0BA);
    ST32(col + oCollisionData_TargetCollisionTypeFlags, R0);

    GUEST_CALL(battle_isTimeStop);
    if (R0 != 0) {
        R1 = 1 << 16;           /* time-stop marker, still in r1 when sub_8019F44 runs */
        ST32(col + oCollisionData_SelfCollisionTypeFlags,
             LD32(col + oCollisionData_SelfCollisionTypeFlags) | 0x10000);
    }

    R0 = col;
    GUEST_CALL(sub_8019F44);

    R4 = inR4;
    R7 = inR7;
    RETURN_TO_CALLER();
}

/* ── sub_800ED90 ── */
/* sub_800ED90(r0 = battle object):
 * Collect the chip data the object currently has selected for use.
 *
 * If the object's name ID decodes to kind 2, the selection comes from that
 * side's battle hand: getBattleHandAddr_8010018 gives the hand, the byte at
 * its start selects a card entry (2 bytes per slot, after a 2-byte header),
 * and the entry's fields are combined into the results:
 *   r0 = chip ID              (entry +0x00)
 *   r1 = entry sub-value      (entry +0x0C)
 *   r2 = entry +0x18 value plus sub_800EF34(chip) plus entry +0x24 value
 * Otherwise the object's own oBattleObject_Chip field is returned with
 * r1/r2 cleared. r3 always receives the object's ChipsHeld count.
 *
 * Flag contract: V is left as the original's last flag-setting instruction
 * on each path (the kind comparison, or the final adds on the hand path);
 * nz/c are clobbered.
 */
Next sub_800ED90(void) {
    u32 obj = R0;
    u32 savedR5 = R5;
    u32 savedR7 = R7;

    /* The callees below run with r5 = object, exactly like the original
     * (mov r5, r0 before the first call). r5/r7 are pushed by the original
     * and restored on return, so save them here and write them back before
     * returning. */
    R5 = obj;

    R0 = LD16(obj + oBattleObject_NameID);
    GUEST_CALL(unpackVerActorTyAndAIIdx_800F29C);
    u32 kind = R0;
    SET_FLAGS_CMP(kind, 2);

    if (kind != 2) {
        /* Not hand-driven: expose the object's own chip selection. */
        R0 = LD16(obj + oBattleObject_Chip);
        R1 = 0;
        R2 = 0;
        R3 = LD8(obj + oBattleObject_ChipsHeld);
        R5 = savedR5;
        R7 = savedR7;
        RETURN_TO_CALLER();
    }

    /* Address of the card entry selected in this side's battle hand. */
    R0 = LD8(obj + oBattleObject_Alliance);
    GUEST_CALL(getBattleHandAddr_8010018);
    u32 hand = R0;
    u32 entry = hand + 2 * LD8(hand) + 2;
    R7 = entry;

    R0 = LD16(entry);
    R1 = 0;
    GUEST_CALL(sub_800EF34);

    u32 baseValue = LD16(entry + 0x18) + R0;
    u32 extra = LD16(entry + 0x24);
    R2 = baseValue + extra;
    SET_FLAGS_ADD(baseValue, extra);
    R1 = LD16(entry + 0xC);
    R0 = LD16(entry);
    R3 = LD8(obj + oBattleObject_ChipsHeld);
    R5 = savedR5;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_800F1DC ── */
/* sub_800F1DC(r0 = value, r2 = incoming hit/event type, r5 = battle object):
 * Certain incoming types (0x5F..0x61, and the special value 0x150) interact
 * with the target's barrier: if an active barrier is present, add 0x32 (50)
 * to r0. r3 returns the collision data pointer (or 0x150 when the type check
 * fails early); r2 ends up holding the barrier/Unk_16 byte on barrier paths.
 * Flags reflect the original's last comparison/addition on each exit path.
 * r1, r6, r7 and LR are untouched. */
Next sub_800F1DC(void) {
    u32 hitType = R2;

    R3 = 0x150;
    if (hitType != 0x150 && ((s32)hitType < 0x5F || (s32)hitType > 0x61)) {
        /* Not a barrier-relevant type: leave r0 alone. */
        if ((s32)hitType < 0x5F)
            SET_FLAGS_CMP(hitType, 0x5F);
        else
            SET_FLAGS_CMP(hitType, 0x61);
        RETURN_TO_CALLER();
    }

    {
        u32 coll = LD32(R5 + oBattleObject_CollisionDataPtr);
        u32 barrier = LD8(coll + 6); /* oCollisionData_Barrier */

        R3 = coll;
        R2 = barrier;

        if (barrier == 0) {
            /* No barrier active. */
            SET_FLAGS_CMP(barrier, 0);
            RETURN_TO_CALLER();
        }
        if ((s32)barrier >= 0x10) {
            /* Barrier value out of the "active" range. */
            SET_FLAGS_CMP(barrier, 0x10);
            RETURN_TO_CALLER();
        }
        if (barrier == 8) {
            u32 unk16 = LD8(coll + oCollisionData_Unk_16);

            R2 = unk16;
            if (unk16 == 0) {
                /* Variant 8 with no charge left: no adjustment. */
                SET_FLAGS_CMP(unk16, 0);
                RETURN_TO_CALLER();
            }
        }

        /* Active barrier: add 50 to the value in r0. */
        R0 += 0x32;
        SET_FLAGS_ADD(R0, 0x32);
        RETURN_TO_CALLER();
    }
}

/* ── sub_800FAAC ── */
/* sub_800FAAC(r0, r1, r2, r3 = effect parameters, r5 = battle object):
 * Spawns a type-3 battle object on the panel in front of the given object
 * and finishes its setup using the object's alliance stats byte.
 * Returns sub_800FAF6's results in r0-r3; preserves r4-r7. */
Next sub_800FAAC(void) {
    u32 savedR0 = R0, savedR1 = R1, savedR3 = R3;
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;
    u32 object = R5;

    /* The original stashes r0/r1/r3 in the scratch registers r4/r6/r7
     * (they are restored from the stack before returning). */

    /* Work out which panel the object is facing. */
    GUEST_CALL(object_getFrontDirection);
    u32 spawnX = R0 + (u32)LD8(object + oBattleObject_PanelX);
    u32 spawnY = (u32)LD8(object + oBattleObject_PanelY);

    /* Spawn and initialise the type-3 object at that panel.
     * r2 passes through unchanged; r3/r5 carry the caller's values. */
    R0 = spawnX;
    R1 = spawnY;
    R4 = savedR0;
    R6 = savedR1;
    R7 = savedR3;
    GUEST_CALL(battle_SpawnAndInitObjectType3_80C4FFE);
    u32 spawnR2 = R2, spawnR3 = R3;

    /* Read byte 2 of the object's alliance's battle navi stats. */
    R0 = (u32)LD8(object + oBattleObject_Alliance);
    R1 = 2;
    R2 = spawnR2;
    R3 = spawnR3;
    GUEST_CALL(GetBattleNaviStatsByte);
    u32 statsByte = R0;

    /* Complete the new object's setup with the panel position and the
     * alliance-dependent stats byte. */
    R0 = spawnX;
    R1 = spawnY;
    R2 = statsByte;
    R3 = spawnR3;
    R4 = savedR0;
    R5 = object;
    R6 = savedR1;
    R7 = savedR3;
    GUEST_CALL(sub_800FAF6);

    /* r0-r3 hold sub_800FAF6's results; restore the scratch registers,
     * as the original's pop {r4,r6,r7,pc} does. */
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_800FFAA ── */
/* sub_800FFAA(r0 = new BPwrAtk value, r4 = AI data):
 * Beast Out bookkeeping for the AI's held power attack. A Beast value
 * (0x21..0x26) is always written; any other value is only written while the
 * stored value is not itself a Beast value, so the Beast state sticks until
 * replaced. When the stored value is a Beast value afterwards, the AI's
 * B-button remap is corrected (3/4 -> 0, 0x2C -> 0x2B) and r2 reports the
 * value written; otherwise r2 is left untouched for the caller. */
#define oAIData_BPwrAtk 7   /* as referenced by the original asm (ldrb/strb r?,[r4,#7]) */
#define oAIData_BButton 6   /* as referenced by the original asm (ldrb/strb r?,[r4,#6]) */

static int is_beast_value(u32 v)
{
    return (s32)v >= 0x21 && (s32)v <= 0x26;
}

Next sub_800FFAA(void) {
    u32 value = R0;
    u32 ai = R4;
    u32 buttonOut = R2; /* incoming r2 survives the early-exit paths */

    u32 stored = LD8(ai + oAIData_BPwrAtk);
    if (is_beast_value(value) || !is_beast_value(stored))
        ST8(ai + oAIData_BPwrAtk, value);
    /* else: keep the Beast value already stored */

    u32 atk = LD8(ai + oAIData_BPwrAtk);
    if (is_beast_value(atk)) {
        u32 button = LD8(ai + oAIData_BButton);
        if (button == 3 || button == 4) {
            buttonOut = 0;
            ST8(ai + oAIData_BButton, 0);
        } else {
            buttonOut = 0x2B;
            if (button == 0x2C)
                ST8(ai + oAIData_BButton, 0x2B);
        }
    }

    R2 = buttonOut;
    RETURN_TO_CALLER();
}

/* ── sub_8010162 ── */
/* sub_8010162: tick the collision timer of the battle object whose pointer
 * lives at [r5 + oBattleObject_CollisionDataPtr].
 *
 * The halfword counter at oCollisionData_Unk_26 counts down once per call,
 * unless it is parked at 0xFFFF. When the count reaches zero, sound effect
 * 0x94 plays. Afterwards the object's flags are tested for 0x400000: if that
 * bit is set, flag bit 4 is cleared, otherwise flag bit 4 is set.
 *
 * inputs:  r3 r4 r5 r6 r7 r10 lr (only r5 is read)
 * outputs: r2 = whatever the last flag routine left in it;
 *          r3 r4 r5 r6 r7 r10 preserved; flags are clobbered.
 */

/* flag bit set/cleared at the end of the routine; the value the flag
 * routines receive in r0 (dword_8010228/dword_801022C era constant) */
#define OBJECT_FLAGS_UNK_4 4

Next sub_8010162(void) {
    u32 savedR4 = R4;

    u32 col = LD32(R5 + oBattleObject_CollisionDataPtr);
    R4 = col;    /* the flag routines below expect the collision pointer in r4 */
    R2 = 0xFFFF; /* dword_8010228: sentinel value carried in r2 */

    u32 timer = LD16(col + oCollisionData_Unk_26);
    if (timer != 0xFFFF) {
        if (timer == 0) {
            /* Counter already expired: skip the countdown, just drop bit 4. */
            R0 = 4;
            GUEST_CALL(object_clearFlag);
            R4 = savedR4;
            RETURN_TO_CALLER();
        }
        timer--;
        ST16(col + oCollisionData_Unk_26, timer);
        if (timer == 0) {
            /* Countdown finished on this tick: play the warning sound. */
            R0 = 0x94;
            R1 = 0;
            GUEST_CALL(PlaySoundEffect);
        }
    }

    GUEST_CALL(object_getFlag);
    R1 = 0x400000; /* dword_801022C */
    if (R0 & 0x400000) {
        R0 = 4;
        GUEST_CALL(object_clearFlag);
    } else {
        R0 = OBJECT_FLAGS_UNK_4;
        GUEST_CALL(object_setFlag1);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_8010230 ── */
/* sub_8010230 — periodic self-damage tick for an AI battle object (r5 = object).
 *
 * While the battle is running (not time-stopped, not paused) and the object
 * still has more than 1 HP, a timer byte in its AI data (offset 9) counts up
 * once per call. When it reaches the interval looked up from byte_80102A4
 * via the object's navi stats byte 0x18, the object loses 1 HP and the timer
 * restarts at 0. An interval of 0 disables the behaviour and just keeps the
 * timer cleared.
 *
 * r4 and r6 come back unchanged; c/v are left as the last flag-setting
 * operation of the taken exit path left them (a callee's flags, or the CMP
 * that decided the early exit). */
Next sub_8010230(void) {
    u32 savedR4 = R4;

    /* Skip entirely while time is stopped or the battle is paused. */
    GUEST_CALL(battle_isTimeStop);
    if (R0 != 0)
        RETURN_TO_CALLER();
    GUEST_CALL(battle_isPaused);
    if (R0 != 0)
        RETURN_TO_CALLER();

    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    /* Don't drain objects that are about to be destroyed anyway. */
    u32 hp = LD16(R5 + oBattleObject_HP);
    SET_FLAGS_CMP(hp, 1);
    if ((s32)hp <= 1)
        RETURN_TO_CALLER();

    /* How many ticks between self-damage hits for this character. */
    R1 = 0x18;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    u32 interval = LD8(A_byte_80102A4 + R0);

    if (interval == 0) {
        /* No self-damage configured: keep the timer cleared. */
        R0 = 0;
        ST8(aiData + 9, R0);
        RETURN_TO_CALLER();
    }

    /* Tick the timer; once it has counted up to the interval, take 1 HP of
     * damage and restart it. */
    u32 timer = LD8(aiData + 9) + 1;
    ST8(aiData + 9, timer);
    SET_FLAGS_CMP(timer, interval);
    if ((s32)timer < (s32)interval)
        RETURN_TO_CALLER();

    R0 = 1;
    R3 = timer;
    R4 = aiData;
    GUEST_CALL(object_subtractHP);
    R4 = savedR4;

    /* Restart the timer after a hit. */
    R0 = 0;
    ST8(aiData + 9, R0);
    RETURN_TO_CALLER();
}

/* ── sub_8010332 ── */
/* sub_8010332: pick a text/data entry for the current cross-over navi.
 * Returns 1 in certain battle modes, 4 if the navi slot is empty, otherwise a
 * byte from the table at byte_8020FE0: row = 11 * stats[0x29], column = stats[0x2B].
 * r4 is preserved. */
Next sub_8010332(void) {
    GUEST_CALL(GetBattleMode);
    if (R0 == 9) {          /* special battle mode: fixed result */
        R0 = 1;
        RETURN_TO_CALLER();
    }

    R1 = 0x29;              /* navi index/slot byte */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    if (R0 == 0) {          /* no navi registered */
        R0 = 4;
        RETURN_TO_CALLER();
    }

    u32 row_offset = 11 * R0;   /* each table row is 11 bytes */

    R1 = 0x2B;              /* variant byte selecting the entry within the row */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    R0 = LD8(A_byte_8020FE0 + row_offset + R0);
    RETURN_TO_CALLER();
}

/* ── sub_8010D20 ── */
/* sub_8010D20: stub that always returns 0 ("false") in r0.
 * r4 is saved and restored, so all registers except r0 are unchanged. */
Next sub_8010D20(void) {
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── sub_8010D58 ── */
/* sub_8010D58(r0 = handler id, r2/r3 = call arguments):
 * Dispatch table driver. Validates the id with sub_800F4A8; ids that pass
 * are handed to sub_800F4B2 and the call returns 0xFFFF, ids that fail are
 * dispatched to one of the five handlers in the table at off_8010D84.
 * An id of 0xFF is rejected outright with 0xFFFF.
 * On the dispatch path the handler is entered exactly as the original does:
 * r0 = handler address (thumb bit set), r1 = table base, r6 = id * 4.
 * Returns the handler's r0/r1 with r4 cleared; r6 is preserved. */
Next sub_8010D58(void) {
    u32 savedR6 = R6;
    u32 id = R0;

    R6 = id << 2;

    if (id == 0xFF) {
        R0 = 0xFFFF;
    } else {
        GUEST_CALL(sub_800F4A8);
        if ((s32)R0 >= 1) {
            /* id accepted: record it and report "no handler ran" */
            R0 = 1;
            GUEST_CALL(sub_800F4B2);
            R0 = 0xFFFF;
        } else {
            /* tail-call the handler selected by id, passing the table
             * base in r1 and its own address in r0, as the original's
             * `ldr r0,[r1,r6]; mov lr,pc; bx r0` sequence does */
            R1 = A_off_8010D84;
            R0 = LD32(R1 + id * 4);
            switch (id) {
            case 0: GUEST_CALL(sub_8010D98); break;
            case 1: GUEST_CALL(sub_8010DA2); break;
            case 2: GUEST_CALL(sub_8010DAC); break;
            case 3: GUEST_CALL(sub_8010DB6); break;
            case 4: GUEST_CALL(sub_8010DC0); break;
            }
            R4 = 0;
        }
    }

    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── sub_8012ABC ── */
/* sub_8012ABC(r0 = battle object):
 * Returns 1 in r0 when the object's current chip qualifies for a special
 * condition. All of the following must hold:
 *   - the battle mode is not 1,
 *   - the navi stat byte 0x2C is 0x17 or 0x18,
 *   - the chip data has flag bit 1 set but bit 0 clear, and element 0xA.
 * The original saves/restores r6 and r7 around the body, so both registers
 * end unchanged; r2 is left at 2 if the check bailed before the flag tests
 * and at 1 afterwards, matching the original's scratch register usage. */
Next sub_8012ABC(void) {
    u32 result = 0;

    GUEST_CALL(GetBattleMode);
    if (R0 != 1) {
        R1 = 0x2C;
        GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
        s32 stat = (s32)R0;

        if (stat >= 0x17 && stat <= 0x18) {
            GUEST_CALL(getChip8021DA8);
            const u32 chip = R0;
            const u32 flags = LD8(chip + 9);

            R2 = 2;
            if ((flags & 2) != 0) {
                R2 = 1;
                /* bit 0 must be clear and the element must be 0xA */
                if ((flags & 1) == 0 && LD8(chip + 6) == 0xA)
                    result = 1;
            }
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_8012C4A ── */
/* sub_8012C4A(r0 = chip slot index, r1 = flags word):
 * Looks up the chip in the given slot and, if it is a chip of the special
 * kind (flags byte has bit 1 set and bit 0 clear, byte 6 == 0x0A) whose
 * battle-navi state byte 0x2C reads 4 or 0x10, ORs the 0x1000 bit into the
 * caller's flags word. Returns the updated flags word in r0.
 * r2 is left holding 2 when the first flag test fails and 1 otherwise,
 * exactly as the original's scratch register ends up; r4/r5/r6 untouched. */
Next sub_8012C4A(void) {
    u32 result = R1;

    /* R0 (chip slot index) in, R0 (pointer to ChipData) out */
    GUEST_CALL(getChip8021DA8);

    u32 flags = LD8(R0 + 9);
    if (flags & 2) {
        R2 = 1;
        /* bit 0 must be clear, and byte 6 must identify the special kind */
        if (!(flags & 1) && LD8(R0 + 6) == 0x0A) {
            R1 = 0x2C; /* R0 still holds the chip data pointer for the callee */
            GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
            u32 naviState = R0;
            if (naviState == 4 || naviState == 0x10)
                result |= 0x1000;
        }
    } else {
        R2 = 2;
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_8012C7C ── */
/* sub_8012C7C(r0 = battle object):
 * Read the battle object's navi type byte (alliance stats, offset 0x2C) and map it to
 * a constant:
 *   type 0x02 or 0x0E -> 0x4000 if the object pointer is non-null, else 0
 *   type 0x03 or 0x0F -> 0x0028 if the object pointer is non-null, else 0
 *   anything else     -> 0
 * r4 is saved/restored by the original, so it is left untouched for the caller. */
Next sub_8012C7C(void) {
    u32 battleObject = R0;

    R1 = 0x2C;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    u32 naviType = R0;

    u32 result = 0;
    if (naviType == 2 || naviType == 0xE) {
        if (battleObject != 0)
            result = 0x4000;
    } else if (naviType == 3 || naviType == 0xF) {
        if (battleObject != 0)
            result = 0x28;
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_80133EC ── */
/* sub_80133EC: initialize the seven battle "slot" entries in one of two orders.
 * The pointer table at off_801340C selects the order list; entry 4 points to
 * byte_801341B = { 0, 6, 7, 8, 9, A, B }. For each of the 7 entries it calls
 * init_8013B4E(index, order[index]).
 *
 * r5 and r6 are only scratch here (the original saves/restores them), and
 * r0/r1/LR are clobbered by the calls; everything else passes through. */
Next sub_80133EC(void) {
    u32 saved_r5 = R5;
    u32 saved_r6 = R6;

    /* order list = off_801340C[4] (points at byte_801341B) */
    u32 orderList = LD32(A_off_801340C + 4);

    for (u32 i = 0; i < 7; i++) {
        R0 = i;
        R1 = LD8(orderList + i);
        GUEST_CALL(init_8013B4E);
    }

    /* restore the caller's r5/r6, as the original's push/pop does */
    R5 = saved_r5;
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── sub_8014040 ── */
/* sub_8014040 (r0 = alliance flag):
 * HP regeneration tick for the battle player on the given alliance side.
 * Skipped when battle effect bit 0x8 is active or the player's navi stat
 * byte 0x36 is not 1. Otherwise the player is healed by ceil(stat 0x3E / 10)
 * HP (BIOS Div for the rounding-up division), the heal effect (kind 6) is
 * spawned from the player's parameter block at +0x34, and SOUND_UNK_8A plays.
 *
 * Register contract: r4/r5 hold entry values on return (the original saves
 * and pops them); r2/r3 end as spawn_t1_0x0_EffectObject leaves them;
 * c/v on return come from the last flag-setting state of each path
 * (PlaySoundEffect, the stat-byte cmp, or the incoming flags respectively). */
Next sub_8014040(void) {
    u32 savedR4 = R4, savedR5 = R5;

    GUEST_CALL(battle_findPlayer);        /* r0 = alliance flag -> player object */
    u32 player = R0;
    R5 = player;

    GUEST_CALL(GetBattleEffects);
    if (R0 & 8) {                         /* regeneration suppressed this battle */
        R4 = savedR4;
        R5 = savedR5;
        RETURN_TO_CALLER();
    }

    R1 = 0x36;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    if (R0 != 1) {
        SET_FLAGS_CMP(R0, 1);             /* reproduce the original's cmp r0, #1 */
        R4 = savedR4;
        R5 = savedR5;
        RETURN_TO_CALLER();
    }

    /* Heal amount: ceil(navi stat halfword 0x3E / 10). */
    R1 = 0x3E;
    GUEST_CALL(GetBattleNaviStatsHword_AllianceFromBattleObject);
    R0 = R0 + 9;
    R1 = 10;
    swi_call(6);                          /* BIOS Div: r0 = (stat+9)/10, r1 = remainder */
    GUEST_CALL(object_addHP);

    /* Spawn the heal effect over the player: it takes the three words of the
     * player's block at +0x34 as parameters plus a pointer just past them
     * (the ldmia writeback), with the effect kind in r4. */
    R1 = LD32(player + 0x34);
    R2 = LD32(player + 0x38);
    R3 = LD32(player + 0x3C);
    R0 = player + 0x40;
    R4 = 6;
    GUEST_CALL(spawn_t1_0x0_EffectObject);

    R0 = SOUND_UNK_8A;
    GUEST_CALL(PlaySoundEffect);

    R4 = savedR4;                         /* pop {r4,r5}: return entry values */
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sub_8014178 ── */
/* sub_8014178: read two stat bytes of the current navi via
 * GetBattleNaviStatsByte(kind, 8), combine them as index = stat1*3 + stat0,
 * look up byte_80141A0[index] (three entries per navi), and hand that byte to
 * sub_801DF8C. r4/r6 are preserved for the caller. */
Next sub_8014178(void) {
    u32 saved_r4 = R4;

    R0 = 0;                     /* stat 0 of ... */
    R1 = 8;                     /* ... stat kind 8 */
    GUEST_CALL(GetBattleNaviStatsByte);
    u32 stat0 = R0;

    R0 = 1;                     /* stat 1 of the same kind */
    R1 = 8;
    GUEST_CALL(GetBattleNaviStatsByte);

    /* three table entries per value of stat0 */
    u32 index = R0 * 3;
    SET_FLAGS_ADD(index, stat0);
    index += stat0;

    R4 = A_byte_80141A0;        /* table base, as in the original */
    R0 = LD8(A_byte_80141A0 + index);
    GUEST_CALL(sub_801DF8C);

    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_8015B22 ── */
/* sub_8015B22(r0 = name id, r5 = battle object):
 * Store the object's NameID. Name id 0 maps to the default 0x1A0;
 * anything else is stored offset by 0x1AB (i.e. r0 - 1 + 0xAD + 0xFF). */
Next sub_8015B22(void) {
    u16 nameId;

    if (R0 == 0)
        nameId = 0x1A0;
    else
        nameId = (u16)(R0 + 0x1AB);

    ST16(R5 + oBattleObject_NameID, nameId);
    RETURN_TO_CALLER();
}

/* ── sub_8016460 ── */
/* sub_8016460 — turn-start setup for one battle object (r5 = object, r10 = toolkit).
 *
 * If bit 1 of the battle intro state is clear, there is nothing to do yet.
 * Otherwise the object is marked as having acted (CurAction = 1, phase
 * counter cleared). Unless this object's alliance already matches the side
 * recorded in the battle state (or the battle mode is 6), an entry effect
 * is run first via sub_801DC7C; objects whose NameID lies in 0x49..0x4E
 * get different effect arguments. */
Next sub_8016460(void) {
    u32 self = R5;         /* battle object */
    u32 toolkit = R10;     /* toolkit */

    GUEST_CALL(battle_getStateUnk5c_800139A);
    u32 state = R0;

    /* lsr r0, r0, #2 — carry out of the shift is bit 1 of the state;
     * when it is clear the intro sequence is not running yet. */
    cpu.c = (state >> 1) & 1;
    SET_FLAGS_NZ(state >> 2);
    if (!cpu.c) {
        R0 = state >> 2;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(GetBattleMode);
    SET_FLAGS_CMP(R0, 6);                          /* cmp r0, #6 */

    u32 playEntryEffect;
    if (R0 == 6) {
        playEntryEffect = 1;
    } else {
        /* Skip the effect when the object fights on the side the current
         * battle state expects (eor result zero means "alliances equal"). */
        R1 = LD8(LD32(toolkit + oToolkit_BattleStatePtr) + oBattleState_Unk_0d);
        R0 = LD8(self + oBattleObject_Alliance);
        SET_FLAGS_NZ(R0 ^ R1);                     /* eor r0, r1 */
        playEntryEffect = (R0 != 0);
    }

    if (playEntryEffect) {
        u32 nameID = LD16(self + oBattleObject_NameID);
        R0 = 0;
        R1 = 0;
        if (nameID >= 0x49 && nameID <= 0x4E) {
            R0 = -0x20;                            /* neg r0, #0x20 */
            R1 = 3;
        }
        GUEST_CALL(sub_801DC7C);
    }

    /* Mark the object as acting and reset its phase/phase-initialized field. */
    ST8(self + 9, 1);                              /* oBattleObject_CurAction */
    R0 = 0;
    SET_FLAGS_NZ(R0);                              /* mov r0, #0 */
    ST16(self + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    RETURN_TO_CALLER();
}

/* ── sub_80165C2 ── */
/* sub_80165C2: per-frame update for one battle object (object pointer in R5).
 *
 * - If the object's flag bit 0x40000000 is set, run sub_8016EC4 for it instead.
 * - Otherwise, if the battle is currently paused, do nothing this frame.
 * - Otherwise tail-call the object's current phase handler, looked up in the
 *   table off_80165F0 by oBattleObject_CurPhase:
 *       phase 0 -> sub_80165F8
 *       phase 1 -> sub_801664E
 *   The handler's results (R0, R1 and the flags) are passed straight through
 *   to our caller; the original achieves this by returning into a `pop {pc}`
 *   right after the handler returns. */
Next sub_80165C2(void) {
    GUEST_CALL(object_getFlag);            /* R0 = object flags */

    if (R0 & 0x40000000) {                 /* special-flagged object */
        GUEST_CALL(sub_8016EC4);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isPaused);           /* R0 != 0 while battle is paused */
    if (R0 != 0)
        RETURN_TO_CALLER();

    u32 phase = LD8(R5 + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80165F0 + phase);  /* fetch handler from the table */

    R0 = phase;                            /* the original passes these to the */
    R1 = handler;                          /* handler in r0/r1 */
    if (phase == 0)
        GUEST_CALL(sub_80165F8);
    else                                   /* phase 1: the only other entry */
        GUEST_CALL(sub_801664E);

    RETURN_TO_CALLER();
}

/* ── sub_801A554 ── */
/* sub_801A554: apply the battle object's current status effect (r5 = object).
 *
 * The status effect byte in the object's collision data selects an entry from
 * the status effect table (off_80209EC): the high nibble picks a sub-table,
 * the low nibble picks an 8-byte entry within it. Each entry describes one
 * field patch plus one object flag:
 *   +0: word  - flag value to set on the object (object_setFlag2)
 *   +4: halfword - value to write into the collision data
 *   +6: byte  - offset of the field in the collision data to patch
 *
 * Status effects 0x50-0x55 additionally clear flag value 6 afterwards.
 * Outputs: r2 as left by the last flag helper called (or untouched if the
 * object has no status effect). */
Next sub_801A554(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 status = LD8(collisionData + oCollisionData_StatusEffectFinal);

    if (status != 0) {
        u32 entry = LD32(A_off_80209EC + ((status >> 4) - 1) * 4)
                  + (status & 0xF) * 8;
        u16 patchValue = LD16(entry + 4);
        u8 patchOffset = LD8(entry + 6);
        u32 flagValue = LD32(entry);

        ST16(collisionData + patchOffset, patchValue);

        /* Register state at the call mirrors the original: r1 still holds the
         * patched value, r2 the table entry, r3 the collision data. */
        R1 = patchValue;
        R2 = entry;
        R3 = collisionData;
        R0 = flagValue;
        GUEST_CALL(object_setFlag2);

        if ((s32)status >= 0x50 && (s32)status <= 0x55) {
            R0 = 6;
            GUEST_CALL(object_clearFlag2);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_801A6E8 ── */
/* sub_801A6E8: check the custom-panel state byte at +0xA4 of the object in R4.
 * - 0xF4 (panel flagged): return 1.
 * - 0xF7 (panel awaiting check): convert R5's HP to BCD and look for a "4"
 *   digit (a bug/frag digit appearing in the HP readout). If one is found,
 *   return 1; otherwise clear the state byte and return 0.
 * - anything else: return 0.
 * R0 = result; R2/R3 and R4/R5/R7 are left as the original leaves them. */
Next sub_801A6E8(void) {
    enum { STATE_BYTE = 0xA4, PANEL_FLAGGED = 0xF4, PANEL_PENDING = 0xF7 };

    u32 state = LD8(R4 + STATE_BYTE);
    u32 result = 0;

    if (state == PANEL_PENDING) {
        R0 = LD16(R5 + oBattleObject_HP);
        GUEST_CALL(memory_bcdConvert);
        u32 bcd = R0;

        u32 hasFour = 0;
        while (bcd != 0) {
            if ((bcd & 0xF) == 4)
                hasFour = 1;
            bcd >>= 4;
        }

        result = hasFour;
        if (hasFour == 0)
            ST8(R4 + STATE_BYTE, 0); /* no 4-digit: reset the panel state */
    } else if (state == PANEL_FLAGGED) {
        result = 1;
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_801A720 ── */
/* sub_801A720(r5 = battle object):
 * If the object's collision slot 0xA4 holds event 0xF6, raise the navi stats
 * at offsets 0x18 and 0x19 by 2 (capped at 7) and then run sub_801A77A.
 * Flags c/v are left over from the collision comparison on the early-exit
 * path, and from sub_801A77A on the normal path. */
static u32 stat_plus2_capped(u32 value)
{
    u32 raised = value + 2;
    if ((s32)raised > 7) /* original compares signed and keeps the sum if <= 7 */
        raised = 7;
    return raised;
}

Next sub_801A720(void) {
    u32 savedR4 = R4; /* original pushes/pops r4, so it must come out unchanged */
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 eventType = LD8(collisionData + 0xA4);

    if (eventType != 0xF6) {
        SET_FLAGS_CMP(eventType, 0xF6);
        RETURN_TO_CALLER();
    }

    /* bump stat 0x18 by 2, capped at 7 */
    R1 = 0x18;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    R2 = stat_plus2_capped(R0);
    R1 = 0x18;
    GUEST_CALL(SetBattleNaviStatsByte_AllianceFromBattleObject);

    /* bump stat 0x19 by 2, capped at 7 */
    R1 = 0x19;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    R2 = stat_plus2_capped(R0);
    R1 = 0x19;
    GUEST_CALL(SetBattleNaviStatsByte_AllianceFromBattleObject);

    /* sub_801A77A may clobber r2/r4; r2 is part of our output, r4 is restored */
    GUEST_CALL(sub_801A77A);
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_801CDEC ── */
/* Palette-transfer destination descriptors. The object's byte at +0x12 is
 * shifted into bits 16-23, filling the low half of each descriptor and thus
 * selecting the slot within the target. */
#define XFER_DST_A 0x80004012u
#define XFER_DST_B 0x40200012u
/* Palette-data blobs in ROM passed to the transfer helper. */
#define XFER_DATA_A 0xCBB4u
#define XFER_DATA_B 0xCBBCu

/* sub_801CDEC(r5 = object):
 * Queue two palette transfers for the object (dst A + data A, then dst B +
 * data B), with the slot index taken from the object's byte at +0x12.
 * Objects whose state byte at +0x0F is 5 or 6 are skipped: they still return
 * that byte in r0 (the load for the comparison lands in r0), with the flags
 * from the corresponding CMP. Otherwise r0-r3 end as the second callee left
 * them, and the flags are whatever it leaves. r4-r6 are preserved. */
Next sub_801CDEC(void) {
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

/* ── sub_801CE92 ── */
/* sub_801CE92(r5 = message window object):
 * Pick the text-advance delay for the current message state (field +7) and
 * start the text printer with sub_802FE7A. Once the state reaches 0x30 the
 * message sequence is over: the object moves to state 8 and field +7 is
 * cleared. States other than 1/2/3/0x2E/0x2F/0x30 skip the printer call.
 * On return r0 = field +7 and the flags reflect CMP r0, #0x30 (or the final
 * "mov r0, #0" when the state was reset). */
Next sub_801CE92(void) {
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

/* ── sub_801DBD4 ── */
/* Object position table: six 8-byte slots at 0x20352E0 (dword_20352E0,
 * addressed here through the eStruct2035280 Size field symbol).  Each slot:
 *   +0  active flag
 *   +1  stored x position
 *   +2  stored y position
 *   +4  pointer to the battle object being tracked                    */
#define POS_TABLE  A_eStruct2035280_Size

/* sub_801DBD4(r5 = battle object):
 * Find the table slot that tracks this battle object and refresh its
 * stored arm position (x, and -y - 8) from the object's sprite data.
 * R0 = 1 (flags as described below) if no slot tracks the object. */
Next sub_801DBD4(void) {
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

/* ── sub_801E0DC ── */
/* sub_801E0DC(r5 = struct pointer):
 * Marks the two bytes at r5+8 and r5+0xA as done (0xFF), fires effect 6 and
 * effect 0x1A through call_sub_3005EBA (both with the same parameter set:
 * r1=2, r2=3, r3=0, r4=4, r5=2), then clears event flag bit 0x400 in the
 * flag word at 0x20352C4 via flags_clearBits_20352C4_801BEC2.
 * r4/r5 are call-saved, so they are held in locals while the fixed call
 * arguments occupy the real registers. */
Next sub_801E0DC(void) {
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

/* ── sub_801E1A4 ── */
/* sub_801E1A4: redraw the battle command-menu window, then do menu upkeep.
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
Next sub_801E1A4(void) {
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

/* ── custMenuPressOK_8028D3A ── */
/* custMenuPressOK_8028D3A: "OK" pressed in the custom screen menu.
 * r5 = menu state struct (oS20364C0_*). Runs the pre-OK bookkeeping calls,
 * dispatches one of the two highlight handlers depending on the current
 * selection state, then triggers the handler switch in custMenuMainMaybe_8026A88
 * by setting the jump offset at r5+1 to 8, clears the counter at r5+2, plays the
 * select sound and returns 0 in r0 (Z set, as after Thumb `mov r0, #0`).
 * The original returns via `pop {pc}`, i.e. to the LR captured on entry, so the
 * entry LR must be preserved across the guest calls. */
Next custMenuPressOK_8028D3A(void) {
    u32 return_address = LR;
    u32 menu = R5;

    GUEST_CALL(sub_8029110);
    GUEST_CALL(sub_80293F8);

    if (LD8(menu + oS20364C0_Unk_19) != 0) {
        R0 = LD8(menu + oS20364C0_Unk_19);
        GUEST_CALL(sub_802937A);
    } else {
        u16 unk3a = LD16(menu + oS20364C0_Unk_3a);
        if (unk3a != 0) {
            R0 = unk3a;
            GUEST_CALL(sub_80293B0);
        }
    }

    /* hand control back to the custom menu main loop with jump offset 8
     * (these two fields are addressed by raw offsets in the disassembly) */
    ST8(menu + 1, 8);
    ST16(menu + 2, 0);

    R0 = SOUND_SELECT_82;
    GUEST_CALL(PlaySoundEffect);

    LR = return_address;
    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_8026CCC ── */
/* sub_8026CCC — per-frame update of the S20364C0 custom-menu object (self in R5).
 *
 * Asks sub_802A220 for the menu's current result:
 *   - anything but 0xFF: latch it into the object at +0x38, switch the object
 *     to its 0x40 handler (which routes through custMenuMainMaybe_8026A88 and
 *     triggers sub_8027548), clear the frame counter at +0x40, and return.
 *   - 0xFF (nothing chosen yet): run the menu's update handlers with the
 *     current joypad state and tick the frame counter at +0x40. */
Next sub_8026CCC(void) {
    GUEST_CALL(sub_802A220);
    u32 result = R0;

    if (result != 0xFF) {
        SET_FLAGS_CMP(result, 0xFF);

        /* record the selection and hand control to the 0x40 state handler */
        ST16(R5 + oS20364C0_Unk_38, result);
        ST8(R5 + 1, 0x40);   /* oS20364C0_JumpOffset01 */
        ST8(R5 + 2, 0);      /* oS20364C0_Unk_02 */
        ST32(R5 + 0x40, 0);

        R0 = 0;
        SET_FLAGS_NZ(0);
    } else {
        /* menu still open: feed it the joypad, update it, tick its counter */
        u32 savedR7 = R7;

        R7 = LD32(R10 + 4);   /* oToolkit_JoypadPtr */

        GUEST_CALL(custMenuSomeHandler_8028B74); /* (self, joypad) */
        GUEST_CALL(sub_8028820);
        GUEST_CALL(sub_8029C08);                 /* (self) */
        GUEST_CALL(sub_802899C);
        GUEST_CALL(sub_8029D34);

        u32 counter = LD32(R5 + 0x40);
        R0 = counter + 1;
        ST32(R5 + 0x40, R0);
        SET_FLAGS_ADD(counter, 1);

        /* r7 is only scratch here; the original pops it back for the caller */
        R7 = savedR7;
    }

    RETURN_TO_CALLER();
}

/* ── sub_802806C ── */
/* Folder-slot scan helpers used by sub_802806C. */
#define SLOT_COUNT   10   /* number of chip slots walked */
#define SLOT_STRIDE  0xC  /* bytes per slot entry */

/* Slot entry layout:
 *   +0: u8  slot state flag (0 = examine this slot)
 *   +8: u32 pointer to a halfword identifying the slot's chip */

/* sub_802806C(r4 = chip slot list, r5 = owner struct):
 * Walk the ten slots in order.  For each slot flagged 0 at +0, map the
 * halfword at [slot+8] to a chip id (sub_802A53C, masked to 9 bits),
 * fetch that chip's data, and test the 0x20 flag in its status byte
 * (+9).  The first matching slot's index is stored at [r5+7] and the
 * scan stops.  [r5+7] is left alone when it is already non-zero or no
 * slot matches.
 * r2/r3/r5 pass through unchanged, and the caller expects its r4/r6
 * back intact. */
Next sub_802806C(void) {
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    if (LD8(R5 + 7) != 0) /* slot choice already recorded */
        RETURN_TO_CALLER();

    u32 slot = R4;
    for (u32 i = 0; i < SLOT_COUNT; i++) {
        R0 = LD8(slot);
        if (R0 == 0) {
            R0 = LD16(LD32(slot + 8));
            R4 = slot; /* keep the guest view of the loop state in sync */
            R6 = i;
            GUEST_CALL(sub_802A53C); /* chip word -> chip id */
            R0 &= 0x1FF;             /* keep the 9-bit chip id */
            GUEST_CALL(getChip8021DA8);
            R0 = LD8(R0 + 9); /* chip status byte */
            R1 = 0x20;
            if (R0 & 0x20) {
                ST8(R5 + 7, i);
                break;
            }
        }
        slot += SLOT_STRIDE;
    }

    R4 = savedR4;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── sub_80280A2 ── */
/* sub_80280A2: register one of the table entries (byte_802A828, halfwords indexed
 * 1..11) into the object at R5.
 *
 * R5+0x10 holds a selection index. Index 0 and anything above 11 are ignored, as
 * is any entry whose bit is already set in the "taken" bitmask word_20349AC
 * (dword_20349A0 + 0xC). Otherwise the entry's halfword is copied to R5+0x22 and
 * active-chip slot 9 is cleared and pointed at that field.
 *
 * On return R2 = (1 << index) whenever the index was in range (it is computed
 * before the bitmask test, so it is also set when the entry turns out to be
 * taken); R4 is preserved. */
Next sub_80280A2(void) {
    u32 index = LD8(R5 + 0x10);

    if (index != 0 && index <= 0xB) {
        u32 takenMask = LD16(A_dword_20349A0 + 0xC);
        u32 entryBit = 1u << index;

        R2 = entryBit;

        if (!(takenMask & entryBit)) {
            /* The table pointer lives in r4 between the lookup and the call;
             * r4 itself is saved/restored by the original, so it is unchanged
             * on return. */
            u32 savedR4 = R4;
            u32 table = A_byte_802A828;

            R4 = table + (index - 1) * 2;
            ST16(R5 + 0x22, LD16(R4));

            R0 = 9;
            R1 = table; /* the original still holds the table base in r1 here */
            GUEST_CALL(getLocOfActiveChips_8027E1C);
            u32 slot = R0;

            ST8(slot, 0);
            ST8(slot + 4, 0);
            ST32(slot + 8, R5 + 0x22);

            R4 = savedR4;
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_80280E0 ── */
/* sub_80280E0: set up a helper object entry (at R4+0x60) once R5's object
 * signals that it is ready.  Reads R5's state byte (+0x10) and facing byte
 * (+0x16); writes kind/hit-table/behaviour bytes into the new entry.
 * R2 ends up holding R5's facing byte when the setup path runs. */
Next sub_80280E0(void) {
    /* Nothing to do if the object is already active/finished. */
    if (LD8(R5 + 0x10) != 0)
        RETURN_TO_CALLER();

    /* Ask sub_802A5DA whether the trigger condition holds; bail out if not. */
    GUEST_CALL(sub_802A5DA);
    if (R0 == 0)
        RETURN_TO_CALLER();

    u32 entry = R4 + 0x60;

    ST8(entry,       4);              /* object kind */
    ST8(entry + 0xC, 5);              /* ... */
    ST8(entry + 3,   0xB);            /* behaviour/table id */
    ST8(entry + 0xE, 7);              /* ... */

    /* Mirror the source object's facing (+0x16): entry+4 = 1 - facing. */
    R2 = LD8(R5 + 0x16);
    u32 dir = (1u - R2) & 0xFF;
    ST8(entry + 4, dir);

    /* When facing is 1 (dir == 0), also flag the entry at +7. */
    if (dir == 0)
        ST8(entry + 7, 1);

    RETURN_TO_CALLER();
}

/* ── sub_802812C ── */
/* sub_802812C(r0 = record index):
 * Queue a record's graphics for transfer: the two 0x40-byte halves of its
 * 0x80-byte block go to VRAM 0x060170E0 and 0x06017160, and a 0x20-byte
 * chunk reached through the record goes to byte_30016B0. */
Next sub_802812C(void) {
    u32 savedR4 = R4;
    u32 record = R0;

    /* The record handle stays in r4 across all the calls below. */
    R4 = record;

    /* Pointer to the record's 0x80-byte graphics block. */
    R0 = record;
    GUEST_CALL(getIndexed80hRecordPtr_8028164);

    /* First half to 0x060170E0, then the second half (which directly
     * follows the first in the block) to 0x06017160. */
    R1 = 0x60170E0;
    R2 = 0x40;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    R0 += R2;                       /* r2 still holds the 0x40 chunk size */

    R1 = 0x6017160;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Fetch the extra chunk associated with the record... */
    R0 = record;
    GUEST_CALL(sub_8028172);

    /* ...and queue it (0x20 bytes) to byte_30016B0. */
    R1 = A_byte_30016B0;
    R2 = 0x20;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_8028172 ── */
/* sub_8028172(r0 = index):
 * Selects one of the 32-byte records laid out at byte_86E56FC. The index is
 * run through the permutation table at off_802818C (entries {3,0,4,1,5,2,6,0},
 * which remaps slot order), then scaled by the 0x20-byte record size and added
 * to the record base. Returns the record address in r0. */
Next sub_8028172(void) {
    u32 index = R0;

    u32 entry = LD8(A_off_802818C + index);
    R0 = A_byte_86E56FC + entry * 0x20;

    RETURN_TO_CALLER();
}

/* ── sub_802869E ── */
/* sub_802869E: load the three sprite sheets of the Beast Out selection screen
 * into VRAM by calling sub_802868C once per sheet.
 *   r0 = graphics source data, r1 = VRAM destination, r3 = sheet slot.
 * r2 is an extra argument for sub_802868C that this function passes through
 * untouched (it is listed as preserved), so each call sees the caller's value.
 * On return r1 = 0x6009B60 and r3 = 3 from the last setup, matching the original. */
Next sub_802869E(void) {
    /* Sheet 1: byte_802A6C0 graphics -> 0x6009AA0, slot 1 */
    R0 = A_byte_802A6C0;
    R1 = 0x6009AA0;
    R3 = 1;
    GUEST_CALL(sub_802868C);

    /* Sheet 2: byte_802A680 graphics -> 0x6009AE0, slot 2 */
    R0 = A_byte_802A680;
    R1 = 0x6009AE0;
    R3 = 2;
    GUEST_CALL(sub_802868C);

    /* Sheet 3: byte_802A6C0 graphics again -> 0x6009B60, slot 3 */
    R0 = A_byte_802A6C0;
    R1 = 0x6009B60;
    R3 = 3;
    GUEST_CALL(sub_802868C);

    RETURN_TO_CALLER();
}

/* ── sub_8028894 ── */
/* sub_8028894: pick a pair of offsets from the ID byte at [R5+7].
 * IDs 0-4 index the first half of the byte_80288B0 table (R1 stays 0),
 * IDs 5+ index the second half and start R1 at 0x18 instead.
 * Returns R0 = index*16 + 8 and R1 = start + 0x68 (flags set from that add),
 * and leaves the byte_80288B0 table address in R6. */
Next sub_8028894(void) {
    u32 index = LD8(R5 + 7);
    u32 r1Base = 0;

    if (index >= 5) {
        r1Base = 0x18;
        index -= 5;
    }

    R6 = A_byte_80288B0;
    R0 = (index << 4) + 8;
    R1 = r1Base + 0x68;
    SET_FLAGS_ADD(r1Base, 0x68);

    RETURN_TO_CALLER();
}

/* ── sub_80288D0 ── */
/* sub_80288D0: load arguments for a follow-up call.
 * r0 = 0x5B, r1 = 0x6E (flags left over from computing r1 = 0x70 - 2),
 * r6 = address of the 16-entry byte-pair table at byte_80288E4
 * (entries look like <value, flag> pairs, e.g. dialog/choice parameters). */
Next sub_80288D0(void) {
    R0 = 0x58 + 3;
    SET_FLAGS_CMP(0x70, 2);
    R1 = 0x70 - 2;
    R6 = A_byte_80288E4;
    RETURN_TO_CALLER();
}

/* ── sub_8028F48 ── */
/* sub_8028F48(r4..r7, r10, lr = caller context; r5 = battle object):
 * Bookkeeping for slot 11 of the active-chip list (the chip currently being
 * handled on the custom screen).  getLocOfActiveChips_8027E1C returns that
 * slot's record.  If the record is in state 2 and its byte at +7 has not yet
 * been set to 2, this decides the byte at +7:
 *   - cleared to 0 when the battle object's byte at +8 is below 5, the helper
 *     sub_802A57E reports non-zero, and the object's byte at +0x19 is zero;
 *   - set to 1 in every other case.
 * The early-exit paths leave the record untouched.  r4-r7 are preserved and
 * r2/r3 pass through unchanged. */
Next sub_8028F48(void) {
    R0 = 0xB;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 chip = R0;

    if (LD8(chip) != 2)
        RETURN_TO_CALLER();
    if (LD8(chip + 7) == 2)
        RETURN_TO_CALLER();

    u32 flag;
    if ((s32)LD8(R5 + 8) >= 5) {
        flag = 1;
    } else {
        /* sub_802A57E is queried with the object's byte at +8 in r0, as in
         * the original call site; it reports non-zero when the chip may be
         * unmarked. */
        R0 = LD8(R5 + 8);
        GUEST_CALL(sub_802A57E);
        u32 allowed = R0;
        flag = (allowed != 0 && LD8(R5 + 0x19) == 0) ? 0 : 1;
    }
    ST8(chip + 7, flag);

    RETURN_TO_CALLER();
}

/* ── sub_8028F84 ── */
/* sub_8028F84: update the "this chip is still on the field" flag (offset 7) of
 * active chip slot 8.
 *
 * getLocOfActiveChips_8027E1C(n) returns a pointer to active-chip slot record n.
 * The flag is cleared only when a chip is in slot 8, its flag is neither 2 nor 3,
 * the chip folder selection at R5 names another chip (byte +8 != 0), and that
 * other chip no longer has an active slot (its record's state byte is 0).
 * In every other reachable case the flag is set to 1; when slot 8 is empty or
 * its flag is 2 nothing is written. */
Next sub_8028F84(void) {
    R0 = 8;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 slot = R0;

    /* No chip occupying slot 8: nothing to do. */
    if (LD8(slot) != 8)
        RETURN_TO_CALLER();

    u8 state = LD8(slot + 7);

    /* Flag 2 means another handler already took care of this slot. */
    if (state == 2)
        RETURN_TO_CALLER();

    u8 keep;
    if (state == 3) {
        keep = 1;
    } else {
        /* R5 holds the chip-selection state: byte +8 is the currently
         * selected chip's index (0 = none), and the table at +0x47 maps
         * that index to the partner chip's slot number. */
        u32 sel = LD8(R5 + 8);
        if (sel == 0) {
            keep = 1;
        } else {
            R0 = LD8(R5 + sel + 0x47);
            GUEST_CALL(getLocOfActiveChips_8027E1C);
            /* The partner's slot record is empty -> the chip is gone. */
            keep = (LD8(R0) != 0);
        }
    }

    ST8(slot + 7, keep);
    RETURN_TO_CALLER();
}

/* ── sub_802A394 ── */
/* sub_802A394(r5 = chip menu/selection state):
 * Only meaningful in specific menu modes (state byte at r5+1 == 0x18 or 4).
 * In those modes, looks up the currently selected active-chip slot (index at
 * r5+7); if that slot entry is empty, resolves the chip it references and
 * tests its ChipData attribute byte for flag 0x20.
 * Returns R0 = 1 if the flag is set, 0 otherwise.
 * R1 on exit mirrors the original: the slot-busy byte, 0x20 on the flag-test
 * path, or the incoming value when the mode check fails. */
Next sub_802A394(void) {
    u32 result = 0;

    u32 state = LD8(R5 + 1);
    if (state == 0x18 || state == 4) {
        R0 = LD8(R5 + 7);
        GUEST_CALL(getLocOfActiveChips_8027E1C);
        u32 slot = R0;

        R1 = LD8(slot);                 /* 0 = slot entry is empty */
        if (R1 == 0) {
            R0 = LD16(LD32(slot + 8));  /* chip reference from the entry */
            GUEST_CALL(getChipID_802A54E);
            u32 chipId = (R0 << 23) >> 23;  /* keep low 9 bits */

            R0 = chipId;
            GUEST_CALL(getChip8021DA8);
            R1 = 0x20;
            if (LD8(R0 + 9) & 0x20)     /* attribute flag 0x20 set */
                result = 1;
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_802A3CC ── */
/* sub_802A3CC: request the pair of sounds played when the Cross/Beast
 * transformation sequence starts (jingle 0x1F, then roar 0x16).
 * Both requests use the generic 0xFFFF/0x100 parameter pair.
 * r4-r7, r10 pass through unchanged; c/v come from the sound dispatcher. */
static void play_transformation_sound(u32 sound_id)
{
    R0 = sound_id;
    R1 = 0xFFFF;
    R2 = 0x80 + 0x80; /* 0x100 */
    GUEST_CALL(sound_dispatchSoundRequest_8000642);
}

Next sub_802A3CC(void) {
    play_transformation_sound(0x1F);
    play_transformation_sound(0x16);
    RETURN_TO_CALLER();
}

/* ── sub_802A4FC ── */
/* sub_802A4FC: tally the chips listed in the current folder by class.
 *
 * The folder is stored as a 0xFFFF-terminated list of chip IDs starting
 * 0x32 bytes into the record at byte_20366C0. For each entry, the chip's
 * class (ChipData+7; 0 = standard, 1 = mega, 2 = giga) is looked up via
 * getChip8021DA8 and the matching counter in dword_20367E0 is incremented.
 * If byte_20366C0 is 0xFF (no folder loaded) the routine does nothing.
 *
 * On return the V flag (and N/Z) reflects the terminating CMP, matching
 * the original's last flag-setting instruction on each exit path. */
Next sub_802A4FC(void) {
    /* The original pushes r4, r6 and r7 and pops them on the way out, so
     * these registers must be unchanged afterwards no matter how we exit
     * (the chip-data lookup may clobber them). */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 folderLoaded = LD8(A_byte_20366C0);
    SET_FLAGS_CMP(folderLoaded, 0xFF);
    if (folderLoaded != 0xFF) {
        u32 entry = A_byte_20366C0 + 0x32;   /* first folder slot (halfword) */
        u32 counters = A_dword_20367E0;      /* per-class tally bytes */

        for (;;) {
            u32 entryValue = LD16(entry);
            SET_FLAGS_CMP(entryValue, 0xFFFF);
            if (entryValue == 0xFFFF)        /* end-of-folder sentinel */
                break;

            u32 chipIndex = entryValue & 0x1FF;   /* low 9 bits = chip ID */
            R0 = chipIndex;
            GUEST_CALL(getChip8021DA8);

            u32 chipClass = LD8(R0 + 7);
            SET_FLAGS_CMP(chipClass, 2);
            if ((s32)chipClass <= 2)
                ST8(counters + chipClass, LD8(counters + chipClass) + 1);

            entry += 2;
        }
    }

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_802A5A0 ── */
/* sub_802A5A0: check whether the "cross beast" state applies this battle.
 * Returns 1 in r0 when all of the following hold:
 *   - the byte at active-chip slot 8 is not 8,
 *   - the battle mode is none of 1, 6, 7 or 9 (special modes),
 *   - the navi stats byte 0x61 of the current battle state equals 1.
 * r4 is preserved (saved/restored by the original), r6 and r10 untouched. */
Next sub_802A5A0(void) {
    u32 result = 0;

    R0 = 8;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    if (LD8(R0) != 8) {
        GUEST_CALL(GetBattleMode);
        u32 battleMode = R0;
        if (battleMode != 1 && battleMode != 6 && battleMode != 7 && battleMode != 9) {
            u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
            R0 = LD8(battleState + oBattleState_Unk_0d);
            R1 = 0x61;
            GUEST_CALL(GetBattleNaviStatsByte);
            if (R0 == 1)
                result = 1;
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_802A5DA ── */
/* sub_802A5DA: check whether the currently selected chip in active slot 8
 * triggers a special navi state. Returns 1 in r0 if so, 0 otherwise.
 * The battle mode must be one of the regular battle modes (0, 5, 8, 10, 11),
 * and navi stats byte 0x60 (indexed by battle state field 0x0d) must be 1. */
Next sub_802A5DA(void) {
    u32 result = 0;

    /* Pointer to active chip slot 8 */
    R0 = 8;
    GUEST_CALL(getLocOfActiveChips_8027E1C);

    /* Slot empty marker (0xFF-like value 8 at that offset) means "no" */
    if (LD8(R0) != 8) {
        R0 = 0;
        GUEST_CALL(GetBattleMode);
        u32 mode = R0;

        if (mode == 0 || mode == 5 || mode == 8 || mode == 0xA || mode == 0xB) {
            u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
            R0 = LD8(battleState + oBattleState_Unk_0d);
            R1 = 0x60;
            GUEST_CALL(GetBattleNaviStatsByte);

            if (R0 == 1)
                result = 1;
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_802BF0C ── */
/* sub_802BF0C(r5 = battle object):
 * Pick an animation/state index for the object and hand it to sub_802C810.
 * If either of the two flag halfwords next to dword_2036820 has bit 0 set,
 * the index comes from the object's byte at +0xC (capped: >1 becomes 0xC,
 * otherwise 4) and is also stored into the object's halfword at +2.
 * Otherwise the index alternates 0/1 with bit 3 of the current frame count.
 * R7 passed to the callee is the flags base +8 on the first path, or the
 * current-frame pointer on the second. R5 and R10 are preserved. */
Next sub_802BF0C(void) {
    u32 flagBits = LD16(A_dword_2036820 + 4);
    flagBits |= LD16(A_dword_2036820 + 0xC);

    if (flagBits & 1) {
        u32 state = LD8(R5 + 0xC);
        ST16(R5 + 2, state > 1 ? 0xC : 4);
        R7 = A_dword_2036820 + 8;
        R0 = 0;
    } else {
        R7 = LD32(R10 + oToolkit_CurFramePtr);
        R0 = (LD16(R7) & 8) >> 3;
    }

    GUEST_CALL(sub_802C810);
    RETURN_TO_CALLER();
}

/* ── sub_802C4B6 ── */
/* sub_802C4B6(r0 = x, r1 = y, r2 = source data, r3 = width, r4 = height):
 * Blit a width×height block of halfwords into the 24-halfword-wide buffer at
 * unk_2034B30, at offset (x + y*24) halfwords. Source rows are consecutive;
 * destination rows advance by 0x30 bytes (24 halfwords) each time.
 * Leaves r0 = width, r1 = height, r2 = one-past-the-end of the source data;
 * flags reflect the final `cmp r1, r4` (i.e. last row counter == height).
 * r5-r7 are pushed/popped by the original, so they end unchanged. */
Next sub_802C4B6(void) {
    u32 src = R2;
    u32 dst = A_unk_2034B30 + (R0 + R1 * 0x18) * 2;

    u32 row = 0;
    do {
        u32 dstRow = dst;
        u32 col = 0;
        do {
            ST16(dstRow, LD16(src));
            src += 2;
            dstRow += 2;
            col++;
        } while (col != R3);
        dst += 0x30; /* stride to the next buffer row */
        row++;
    } while (row != R4);

    R0 = R3; /* column counter as the original leaves it */
    R1 = R4; /* row counter as the original leaves it */
    R2 = src;
    SET_FLAGS_CMP(R1, R4);
    RETURN_TO_CALLER();
}

/* ── sub_802C54C ── */
/* sub_802C54C(r0 = packed id):
 * Splits a packed id into its fields and returns them:
 *   r0 = group (bits 14-15; 0xFF if the id is the 0xFFFF sentinel),
 *   r1 = low field (bits 0-8 for group 0, bits 0-13 otherwise),
 *   r2 = middle field (bits 9-13, only produced for group 0;
 *        left untouched for other groups). */
Next sub_802C54C(void) {
    u32 packed = R0;

    if (packed == 0xFFFF) {
        /* Sentinel id: report it as group 0xFF. */
        R0 = 0xFF;
        R1 = 0xFFFF;
        RETURN_TO_CALLER();
    }

    u32 group = packed >> 14;
    if (group == 0) {
        R0 = 0;
        R2 = packed >> 9;
        R1 = packed & 0x1FF;
    } else {
        R0 = (group == 1) ? 1 : (group == 2) ? 2 : 3;
        R1 = packed & 0x3FFF;
    }
    RETURN_TO_CALLER();
}

/* ── sub_802C5B0 ── */
/* sub_802C5B0(r0 = value to record):
 * Opens the in-game dialog/upgrade window: remembers the caller's value in
 * the toolkit's unk_200A220-backed state, renders the window's text
 * graphics into VRAM, then queues the follow-up draw job (whose r1/r2
 * results are returned to the caller). r4-r7 and r8/r9 are preserved. */
Next sub_802C5B0(void) {
    u32 value = R0;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* Reserve the original's 0x70-byte stack frame. Its address briefly
     * lands in r6 and is spilled below SP across the calls, but the frame
     * itself is never otherwise used. */
    u32 frame = SP - 0x70;
    SP = frame;
    SP -= 4;
    ST32(SP, frame);                    /* push {r6} */

    /* Stash the caller's value in the state struct pointed to by the
     * toolkit's oToolkit_Unk200a220_Ptr entry. */
    u32 statePtr = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(statePtr + 8, value);

    /* renderTextGfx_8045F8C(script = TextScript8736D74, 5, tiles = unk_2035320,
     * vram = 0x600BA80, 10, 1, palette = dword_86B7AE0, 8):
     * blit the window's text graphics. */
    R0 = A_text_8736D74;
    R1 = 5;
    R2 = A_unk_2035320;
    R3 = 0x600BA80;
    R4 = 0xA;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 8;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* sub_802C4B6(2, 12, byte_802C6C4, 10, 2): queue the window's
     * tilemap/animation update; its r1/r2 are this function's result. */
    R0 = 2;
    R1 = 0xC;
    R2 = A_byte_802C6C4;
    R3 = 0xA;
    R4 = 2;
    GUEST_CALL(sub_802C4B6);

    /* Tear down the frame and hand back the callee's r1/r2/r3, restoring
     * the caller's r4-r7 as the original's pop {r4-r7,pc} does. */
    SP += 4;                            /* pop {r6} (value unused) */
    SP += 0x70;
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_802C97E ── */
/* sub_802C97E(r5 = battle object, r10 = battle state context):
 * Re-evaluates the object's sub-state byte at [r5+0x0E] based on the battle's
 * state byte 0x0D. sub_802CA1E maps that state byte to an index into the
 * frame-window tables (0xFF = no window for this state). For a valid index i
 * the object's timer at [r5+0x1C] is compared against the u16 window bounds
 * unk_20018C0[i] (window start) and unk_2000260[i] (window end):
 *
 *   timer <  start              -> [r5+0x0F] = 1, result 2 (window not reached)
 *   start <= timer < end        -> [r5+0x0F] = 1, result 1 (inside the window)
 *   timer >= end                -> result 0            (window finished)
 *   no window (0xFF from call)  -> result 0
 *
 * Returns the result in r0 (also stored to [r5+0x0E]); r1/r2/r3 hold the last
 * table base, table entry and timer value (sub_802CA1E's r1 in the 0xFF case).
 * Flags are those of `adds r0, result, #0`. */
Next sub_802C97E(void) {
    u32 savedR4 = R4;
    u32 timer, windowStart, windowEnd, result;

    R4 = 0;

    /* state byte 0x0D -> sub-state index via sub_802CA1E (also returns r1) */
    GUEST_CALL(battle_getStateByte0d_800A7E2);
    GUEST_CALL(sub_802CA1E);

    if (R0 == 0xFF) {
        /* no active window: sub-state stays 0; r1 keeps sub_802CA1E's value,
         * r2/r3 are left untouched */
        result = 0;
    } else {
        u32 index = R0 + R0;    /* index into the u16 tables */

        R1 = A_unk_20018C0;
        R2 = windowStart = LD16(A_unk_20018C0 + index);
        R3 = timer = LD32(R5 + 0x1C);

        if ((s32)timer >= (s32)windowStart) {
            R4 = 1;
            R1 = A_unk_2000260;
            R2 = windowEnd = LD16(A_unk_2000260 + index);
            R3 = timer = LD32(R5 + 0x1C);

            if ((s32)timer < (s32)windowEnd) {
                ST8(R5 + 0xF, 1);   /* inside the window */
                result = 1;
            } else {
                result = 0;         /* window already over */
            }
        } else {
            ST8(R5 + 0xF, 1);       /* window not started yet */
            result = 2;
        }
    }

    R0 = result;
    SET_FLAGS_ADD(result, 0);
    ST8(R5 + 0xE, result);
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_802C9B8 ── */
/* sub_802C9B8: update the per-slot record tables with this battle's value.
 *
 * Reads the u32 at battle state +0x40 (the value to record), then asks
 * sub_802CA1E which table slot the state's byte at +0x0D selects (0xFF =
 * nothing to record).  unk_20018C0 tracks the best (lowest) value seen per
 * slot and unk_2000260 the running value:
 *   - value below the recorded one: overwrite both tables
 *   - otherwise: lower the unk_2000260 entry if the value is below it
 * Returns with the flags left as the original's last comparison set them
 * (the v flag is an output; nz/c are free). */
Next sub_802C9B8(void) {
    u32 savedR4 = R4;

    /* R0 = value to record; the byte getter receives it in R0 as the
     * original leaves it there between the two calls. */
    GUEST_CALL(battle_getStateUnk40_800A704);
    u32 value = R0;

    GUEST_CALL(battle_getStateByte0d_800A7E2);

    /* sub_802CA1E gets the state byte in r0 and the recorded value in r4. */
    R4 = value;
    GUEST_CALL(sub_802CA1E);
    u32 slot = R0;
    R4 = savedR4;

    if (slot == 0xFF) {
        SET_FLAGS_CMP(R0, 0xFF);
        RETURN_TO_CALLER();
    }

    u32 offset = slot * 2;
    u16 recorded = LD16(A_unk_20018C0 + offset);
    u16 current = LD16(A_unk_2000260 + offset);

    if ((s32)value < (s32)recorded) {
        /* new best value: record it in both tables */
        ST16(A_unk_20018C0 + offset, value);
        ST16(A_unk_2000260 + offset, value);
        SET_FLAGS_CMP(value, recorded);
    } else if ((s32)value < (s32)current) {
        /* below the running value: only lower that one */
        ST16(A_unk_2000260 + offset, value);
        SET_FLAGS_CMP(value, current);
    } else {
        SET_FLAGS_CMP(value, current);
    }
    RETURN_TO_CALLER();
}

/* ── sub_802C9EA ── */
/* sub_802C9EA: keep the halfword at +8 of the two effect buffers
 * (unk_2035260 / unk_203526C) in sync. Which buffer is source and which is
 * destination depends on oBattleState_Unk_10 in the current battle state.
 * If the value read from the source is 0xFFFF, it is replaced by the result
 * of sub_80AABA4(byte at dst+1) before being written back. The whole thing
 * only runs when GetBattleEffects() reports effect bit 3. */
Next sub_802C9EA(void) {
    /* () -> effect bitmask in R0 (R10 = toolkit pointer) */
    GUEST_CALL(GetBattleEffects);

    R1 = 8;
    if ((R0 & R1) != 0) {
        /* Default direction: unk_203526C -> unk_2035260. */
        R6 = A_unk_203526C;
        R7 = A_unk_2035260;

        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        if (LD8(battleState + oBattleState_Unk_10) != 0) {
            /* Swapped direction: unk_2035260 -> unk_203526C. */
            R6 = A_unk_2035260;
            R7 = A_unk_203526C;
        }

        /* Copy the word over, then inspect the halfword copy. */
        ST32(R7 + 8, LD32(R6 + 8));
        u32 word = LD16(R6 + 8);

        R1 = 0xFFFF;
        SET_FLAGS_CMP(word, 0xFFFF);
        if (word == 0xFFFF) {
            /* Slot is empty: compute the value from the destination's byte. */
            R0 = LD8(R7 + 1);
            GUEST_CALL(sub_80AABA4);
            ST16(R7 + 8, R0);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_802CA1E ── */
/* sub_802CA1E(r0 = side index (0 or 1), r10 = toolkit):
 * Scan the four enemy slots of the side opposite to r0 in the battle state
 * (each side's slots live at offset 0x4C, one 8-byte block per side, four
 * u16 enemy indices each). For each index, look up its version/actor type/AI
 * index; if it is a version-2 enemy of actor type 1 with AI index < 0x14,
 * return its AI index minus 1. Otherwise return 0xFF.
 * r1 on return is the last field compared, exactly as in the original
 * (2 on success); callers of this function ignore it. */
Next sub_802CA1E(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u32 slot = battle + (R0 ^ 1) * 8 + 0x4C;

    for (u32 i = 0; i < 4; i++, slot += 2) {
        R0 = LD16(slot);
        GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);
        u32 info = R0;

        R1 = LD8(info + 1);              /* actor type */
        if (R1 != 1)
            continue;
        R1 = LD8(info + 2);              /* AI index */
        if ((s32)R1 >= 0x14)
            continue;
        R1 = LD8(info);                  /* version */
        if (R1 != 2)
            continue;

        R0 = LD8(info + 2) - 1;          /* matched: return AI index - 1 */
        RETURN_TO_CALLER();
    }

    R0 = 0xFF;
    RETURN_TO_CALLER();
}

/* ── sub_802DFC8 ── */
/* sub_802DFC8(r5 = battle object):
 * Allocate (and zero-fill) a fresh entry in the shared entry pool for this
 * object's alliance, then — if battle flag 0x40 is set — initialize it as an
 * entry of kind 3 anchored at the object's panel x position, and notify
 * sub_802E07C about the alliance.
 * The entry pointer is also left in r4 for callers that inspect it. */
Next sub_802DFC8(void) {
    u32 alliance = LD8(R5 + oBattleObject_Alliance);

    R0 = alliance;
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);
    u32 entry = R0;

    /* Clear the whole 0x1D0-byte entry. */
    R0 = entry;
    R1 = 0xE8 + 0xE8; /* 0x1D0 bytes */
    GUEST_CALL(ZeroFillByWord);

    GUEST_CALL(battle_testFlag0x40_800A8F8);
    if (R0 != 0) {
        ST8(entry + 0x00, 1);            /* active */
        ST8(entry + 0x0B, 0xFF);
        ST8(entry + 0x10, 1);
        ST8(entry + 0x11, LD8(R5 + oBattleObject_PanelX)); /* panel x */
        ST8(entry + 0x0E, 3);            /* kind */

        R0 = LD8(R5 + oBattleObject_Alliance);
        GUEST_CALL(sub_802E07C);
    }

    RETURN_TO_CALLER();
}

/* ── sub_802F668 ── */
/* sub_802F668(r5 = StartScreen state block):
 * (Re)initialize the start screen: clear its counters, reset the menu
 * cursor, start the title-screen music, set up the LCD render info,
 * reload the background tiles, start a fade-in, and write 4 into the
 * jump-table index (offset 1) so the dispatcher at startScreen_RunStateHandler_802F63C runs
 * handler sub_802F6A4 next. */
Next sub_802F668(void) {
    u32 self = R5;

    /* clear the screen's frame/step counters */
    ST8(self + 0xE, 0);
    ST8(self + 0xF, 0);

    /* reset the selected menu entry to the first row (0xA46) */
    ST16(self + 4, 0xA46);

    /* play the title screen music */
    R0 = 1;
    GUEST_CALL(music_80005F2);

    R0 = 0xA;
    GUEST_CALL(sub_80015FC);

    /* configure LCD render control */
    R0 = 0x1340;
    GUEST_CALL(SetRenderInfoLCDControl);

    /* reset the cursor index and reload the background tiles */
    ST8(self + 6, 0);
    GUEST_CALL(copyBGTiles_802FD3C);

    /* fade in from black (type 8, speed 0x10) */
    R0 = 8;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    /* advance to start-screen state 4 (handler sub_802F6A4) */
    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(self + 1, R0); /* oStartScreen_JumpTableOff_01 == 1 */

    RETURN_TO_CALLER();
}

/* ── sub_802F7E8 ── */
/* sub_802F7E8: per-frame check on one of the menu screens. While the Start
 * button is held, wipe the screen's work area, play the menu sound, store
 * the follow-up state (0x0C) into the caller's object (r5) and load the two
 * GFX animation scripts the next state draws with. R7 ends up holding the
 * toolkit pointer (same value the caller passed in R10). */

/* offset of the joypad-pointer field in the toolkit struct (the disassembly
 * calls it oToolkit_JoypadPtr; that macro isn't in game.h, so spell out 4) */
#define TOOLKIT_JOYPAD_PTR 4
/* bit 3 of the held-key halfword: Start button */
#define JOYPAD_START 0x0008

Next sub_802F7E8(void) {
    /* the original does "mov r7, r10" unconditionally: r7 must alias the
     * toolkit pointer on both paths, since callees pick it up from r7 */
    R7 = R10;

    u32 heldKeys = LD16(LD32(R7 + TOOLKIT_JOYPAD_PTR) + 2);

    if (heldKeys & JOYPAD_START) {
        /* zeroFill_813D960 finds its target through the toolkit pointer in r7
         * and reads r5/r10 as well */
        GUEST_CALL(zeroFill_813D960);

        ST8(R5 + 1, 0xC); /* state for the caller's object: leave this screen */

        R0 = SOUND_SELECT_67;
        /* the tst mask is still live in r1 here; PlaySoundEffect reads it */
        R1 = 8;
        GUEST_CALL(PlaySoundEffect);

        R0 = A_byte_802F2E4;
        GUEST_CALL(LoadGFXAnim);
        R0 = A_dword_802F334;
        GUEST_CALL(LoadGFXAnim); /* may update r10; callers see the new value */
    }

    RETURN_TO_CALLER();
}

/* ── sub_8030158 ── */
/* sub_8030158 (r5 = camera state, r10 = toolkit):
 * Scroll the first background layer pair by the camera's movement since the
 * last frame (current camera position minus previous position), then ask
 * sub_80269E2 -- passing it the scroll deltas on the stack -- whether the
 * second layer pair should follow; if it returns 0, scroll that pair too.
 * The original saves/restores r4-r7, so those end unchanged; r1 and r4's
 * scroll deltas are reloaded after the call like the original's pop does,
 * r2 is left holding whatever the callee leaves (the original never reloads
 * it), and the flags come from the TST of the callee's result (early exit)
 * or from the final ADDS when both layer pairs scroll. */
Next sub_8030158(void) {
    /* the original pushes r4-r7 on entry and pops them on exit */
    u32 inR4 = R4, inR6 = R6, inR7 = R7;

    /* camera current minus previous position:
     * oCamera_Unk_04/06 = current X/Y, oCamera_Unk_08/oCamera_Unk_0a = previous X/Y */
    u32 scrollX = LD16(R5 + 4) - LD16(R5 + 8);
    u32 scrollY = LD16(R5 + 6) - LD16(R5 + oCamera_Unk_0a);

    R1 = scrollX;
    R4 = scrollY;
    R2 = LD16(R5 + oCamera_Unk_0a);

    /* oToolkit_RenderInfoPtr lives at offset 8 in the toolkit struct */
    u32 renderInfo = LD32(R10 + 8);

    R6 = LD16(renderInfo + oRenderInfo_Unk_14) + scrollX;
    R7 = LD16(renderInfo + oRenderInfo_Unk_16) + scrollY;
    ST16(renderInfo + oRenderInfo_Unk_14, R6);
    ST16(renderInfo + oRenderInfo_Unk_16, R7);

    /* sub_80269E2(r0 = render info, [sp] = scrollX, [sp+4] = scrollY) */
    R0 = renderInfo;
    SP -= 8;
    ST32(SP, scrollX);
    ST32(SP + 4, scrollY);
    GUEST_CALL(sub_80269E2);
    SP += 8;

    /* the callee may clobber r1/r4; the original pops its saved copies back */
    R1 = scrollX;
    R4 = scrollY;

    if (R0 != 0) {
        /* non-zero: the second layer pair must not scroll */
        SET_FLAGS_NZ(R0);
        R4 = inR4;
        R6 = inR6;
        R7 = inR7;
        RETURN_TO_CALLER();
    }

    R0 = LD32(R10 + 8); /* oToolkit_RenderInfoPtr, reloaded as in the original */
    R6 = LD16(renderInfo + oRenderInfo_Unk_18) + scrollX;
    u32 baseY = LD16(renderInfo + oRenderInfo_Unk_1a);
    R7 = baseY + scrollY;
    ST16(renderInfo + oRenderInfo_Unk_18, R6);
    ST16(renderInfo + oRenderInfo_Unk_1a, R7);
    SET_FLAGS_ADD(baseY, scrollY);

    R4 = inR4;
    R6 = inR6;
    R7 = inR7;
    RETURN_TO_CALLER();
}

/* ── sub_80310B0 ── */
/* sub_80310B0(r0, r1): shift the camera position by a delta.
 * The deltas in r0/r1 are first shifted left by 14 (coarse units to the
 * internal fixed-point scale). If both shifted deltas are zero, nothing
 * happens. Otherwise the camera position struct pointed to by
 * *dword_200F3D0 (x at +0, y at +4) gets the shifted delta plus the current
 * scroll offsets at dword_200F3F8 (x) / dword_200F3FC (y) added to it.
 *
 * Results: r3 = 1 if the position was updated, 0 if skipped; r4 = scroll
 * state base, r6 = new y. c/v come from the final y add on the update path,
 * and from the "r3 == 0" comparison on the skip path. */
Next sub_80310B0(void) {
    u32 dx = R0 << 14;
    u32 dy = R1 << 14;
    u32 moved = 1;

    if (dx == 0 && dy == 0)
        moved = 0;

    R3 = moved;
    if (moved == 0) {
        SET_FLAGS_CMP(R3, 0);
        RETURN_TO_CALLER();
    }

    u32 camPos = LD32(A_dword_200F3D0); /* camera position struct */
    u32 scroll = A_dword_200F3E0;       /* screen scroll state block */

    u32 x = LD32(camPos) + dx;
    x = x + LD32(scroll + 0x18); /* dword_200F3F8: scroll offset x */
    ST32(camPos, x);

    /* The outgoing c/v flags come from the final ADD of scrollY to the
     * delta-adjusted y (before the scroll offset is folded into the value
     * stored). Compute the add operands separately so the flags match. */
    u32 yBase = LD32(camPos + 4) + dy;
    u32 scrollY = LD32(scroll + 0x1C); /* dword_200F3FC: scroll offset y */

    R6 = yBase + scrollY;
    SET_FLAGS_ADD(yBase, scrollY);
    ST32(camPos + 4, R6);

    R4 = scroll;
    RETURN_TO_CALLER();
}

/* ── sub_80310EC ── */
/* sub_80310EC(r0 = sprite position struct, r5 = parent object, r6 = world coords):
 * Converts a sprite's world coordinates to screen coordinates.
 *
 * r5 -> +0x10: camera/screen struct; its +4/+8 fields (<<2) are the pixel
 *              scroll offsets that get subtracted from every result.
 * r6 -> +0x00/+0x04: fixed-point (1/16384) x/y base position,
 *      +0x0C/+0x10: already-scaled x/y positions used as-is.
 * r0 -> +0x18/+0x1C: per-object x/y offsets added to the base position;
 *      results are stored to r0's +0x00/+0x04/+0x0C/+0x10 fields. */
Next sub_80310EC(void) {
    u32 *pos = (u32 *)R0;
    u32 camera = LD32(R5 + 0x10);
    s32 scrollX = (s32)LD32(camera + 4) << 2;
    s32 scrollY = (s32)LD32(camera + 8) << 2;
    u32 *world = (u32 *)R6;

    pos[0] = (u32)((((s32)world[0] + (s32)LD32(R0 + 0x18)) >> 14) - scrollX);
    pos[1] = (u32)((((s32)world[1] + (s32)LD32(R0 + 0x1C)) >> 14) - scrollY);
    pos[3] = (u32)(((s32)world[3] >> 14) - scrollX);
    pos[4] = (u32)(((s32)world[4] >> 14) - scrollY);
    RETURN_TO_CALLER();
}

/* ── sub_803172E ── */
/* sub_803172E(r0 = entity position block, r5 = target block):
 * Writes four values into the dword_200F3E0 scratch array, each being one of
 * the entity's position fields converted from fixed-point (>> 15) minus twice
 * the matching field of the target:
 *   [+0x00] = (x     >> 15) - 2 * target[+0x0C]
 *   [+0x04] = (y     >> 15) - 2 * target[+0x10]
 *   [+0x0C] = (nextX >> 15) - 2 * target[+0x0C]
 *   [+0x10] = (nextY >> 15) - 2 * target[+0x10]
 * Returns r0 = &dword_200F3E0, r6 = the position block pointer, and r7 =
 * the last value stored. */
Next sub_803172E(void) {
    u32 pos = R0;

    /* Target reference coordinates, scaled by two. */
    u32 targetX = LD32(R5 + 0xC) << 1;
    u32 targetY = LD32(R5 + 0x10) << 1;

    R6 = pos;
    R0 = A_dword_200F3E0;

    s32 relX     = (s32)LD32(pos) >> 15;
    s32 relY     = (s32)LD32(pos + 4) >> 15;
    s32 nextRelX = (s32)LD32(pos + 0xC) >> 15;
    s32 nextRelY = (s32)LD32(pos + 0x10) >> 15;

    R7 = relX - targetX;
    ST32(A_dword_200F3E0, R7);

    R7 = relY - targetY;
    ST32(A_dword_200F3E0 + 4, R7);

    R7 = nextRelX - targetX;
    ST32(A_dword_200F3E0 + 0xC, R7);

    R7 = nextRelY - targetY;
    ST32(A_dword_200F3E0 + 0x10, R7);

    RETURN_TO_CALLER();
}

/* ── sub_8113162 ── */
/* sub_8113162: initialize an attack on a battle object.
 * r4 = object supplying the attack index (its +0x16 byte selects one preset),
 * r5 = context object whose +0x13 state byte must pass battle_pickRandomRowObjectFields_810971A's check,
 * r7 = object whose attack fields are (re)initialized from the preset tables.
 * Returns early without touching r7 if the check fails; flags on that path come
 * from the "state == 0" comparison, otherwise from object_setAttack0. */
Next sub_8113162(void) {
    R0 = LD8(R5 + 0x13);
    GUEST_CALL(battle_pickRandomRowObjectFields_810971A);

    /* cmp r0, #0 from the original: caller may branch on these flags. */
    SET_FLAGS_CMP(R0, 0);
    if (R0 == 0)
        RETURN_TO_CALLER();

    /* Attack preset selected by the attacker's +0x16 index. */
    u32 preset = LD8(R4 + 0x16);

    /* Per-preset stat tables: power, element/type, hit behavior, etc. */
    ST8 (R7 + 0xF,  LD8 (A_byte_81130DE + preset));
    ST8 (R7 + 0xE,  LD8 (A_byte_81130D8 + preset));
    ST8 (R7 + 0xC,  LD8 (A_byte_81130C6 + preset));
    ST8 (R7 + 0x18, LD8 (A_byte_81130D2 + preset));
    ST16(R7 + 0x8,  LD8 (A_byte_81130C0 + preset));

    /* Fixed width of 10 for this attack. */
    ST16(R7 + 0xA, 10);

    /* Per-preset word-sized field (e.g. sprite/animation pointer). */
    R1 = preset << 2;
    ST32(R7 + 0x30, LD32(A_byte_81130A8 + R1));

    R0 = 10;
    GUEST_CALL(object_setAttack0);
    RETURN_TO_CALLER();
}

/* ── sub_8116BC0 ── */
/* sub_8116BC0(r5 = falling object state):
 * Move a falling object 8 pixels further down, then check it against the
 * stop depth selected by the state byte at [r5+4] (an index into the
 * halfword table at byte_8116BEC: 0x20, 0x30, ... 0x90). While the object
 * is still above its stop depth, nothing else happens; once it has reached
 * or passed it, snap its coordinate to the stop depth and store phase 4
 * into the state byte at [r5+9].
 *
 * Register results: r0/r1 = the coordinates read back (r0 becomes 4 when
 * the object settles), r2 = state byte * 2, r3 = the table entry
 * (or whatever sprite_setCoordinates leaves in r3 when it is called). */
Next sub_8116BC0(void) {
    R0 = 0;
    R1 = 8;
    GUEST_CALL(sprite_addCoordinates);   /* move down by (0, 8) */
    GUEST_CALL(sprite_getCoordinates);   /* r0 = x, r1 = y */

    R2 = LD8(R5 + 4) << 1;               /* state byte -> halfword offset */
    R3 = LD16(A_byte_8116BEC + R2);      /* stop depth for this state */

    SET_FLAGS_CMP(R3, R1);
    if ((s32)R3 <= (s32)R1) {            /* depth reached: settle here */
        R1 = R3;
        GUEST_CALL(sprite_setCoordinates);
        R0 = 4;
        ST8(R5 + 9, R0);                 /* advance to phase 4 */
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8116C40 ── */
/* sub_8116C40: adjust the 16-bit value at +0xC of an Nth sub-record by +/-2,
 * clamped to 0x16..0x1E.
 *
 * The sub-record pointer comes from record_getNthSubRecordPtr_8116CD4 (called
 * with the caller's registers untouched; it picks the record itself).
 * Direction depends on the bytes at [r7+4] and [r5+4]:
 *   equal     -> increase by 2, clamped so the value never exceeds 0x1E
 *   different -> decrease by 2, clamped so the value never goes below 0x16
 *
 * inputs:  r4 r5 r6 r7 lr
 * outputs: r4 r5 r6 (unchanged)
 * clobbers: nz c v r0 r1 r2 r3 lr
 */
Next sub_8116C40(void) {
    GUEST_CALL(record_getNthSubRecordPtr_8116CD4);
    u32 record = R0;

    s32 limit = 0x16;
    s32 step = -2;
    u32 raise = 0;

    u32 lhs = LD8(R7 + 4);
    u32 rhs = LD8(R5 + 4);
    if (lhs == rhs) {
        limit = 0x1E;
        step = 2;
        raise = 1;
    }

    s32 value = (s32)LD16(record + 0xC) + step;

    if (raise) {
        if (value >= limit)
            value = limit;
    } else {
        if (value <= limit)
            value = limit;
    }

    ST16(record + 0xC, (u32)value);
    RETURN_TO_CALLER();
}

/* ── sub_8116DB4 ── */
/* sub_8116DB4(r5 = sprite object):
 * Keeps a PET-menu-linked sprite (e.g. the MegaMan sprite shown on the PET)
 * in sync with the PET menu's state:
 *   - ePETMenuData+4 (menu substate) being 8 means the sprite should be shown;
 *     the show/hide flag is written to object+0xC, and whenever it changes the
 *     sprite's animation is (re)started with no shadow.
 *   - sprite_update is then run for the object.
 *   - When the menu's main state (ePETMenuData+0) has reached 8, object+0x9 is
 *     set to 8 to mark the sprite as done; r0..r3 and the nz/c/v flags are left
 *     as sprite_update / the final state comparison left them.
 * Preserves r4, r5 and the incoming r7. */
Next sub_8116DB4(void) {
    u32 petMenuSubstate = LD8(A_ePETMenuData + 4);
    u32 showSprite = (petMenuSubstate == 8) ? 1u : 0u;

    /* Update the object's visible flag; restart the animation only on change. */
    ST8(R5 + 0xC, showSprite);
    if ((u8)LD8(R5 + 0xD) != showSprite) {
        ST8(R5 + 0xD, showSprite);
        R0 = showSprite;
        GUEST_CALL(sprite_setAnimation);      /* (u8 a1) -> void */
        GUEST_CALL(sprite_loadAnimationData); /* () -> void */
        GUEST_CALL(sprite_noShadow);          /* () -> void */
    }

    GUEST_CALL(sprite_update);                /* leaves results in r1..r3 */

    u32 petMenuState = LD8(A_ePETMenuData);
    if (petMenuState == 8) {
        /* Menu finished: flag the object with state 8. */
        R0 = 8;
        ST8(R5 + 9, 8);
        SET_FLAGS_NZ(8);
    } else {
        /* Keep the state byte in r0 and the comparison flags for the caller. */
        R0 = petMenuState;
        SET_FLAGS_CMP(petMenuState, 8);
    }
    RETURN_TO_CALLER();
}

/* ── sub_81179AE ── */
/* sub_81179AE: per-frame state dispatcher.
 *
 * Fetches the active work area through sub_8047D84 (which locates it via
 * the singleton pointer at byte_2011C10), then invokes one of four state
 * handlers depending on the state byte at the start of that work area.
 * The handlers run with r1 = state * 4 and r4 = work area; r2/r3 and
 * r5-r12 pass straight through to them. r4-r7 are preserved for the
 * caller, exactly as the original's push/pop does. */
Next sub_81179AE(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R7 = A_byte_2011C10;
    GUEST_CALL(sub_8047D84);

    u32 work = R0;
    u32 state = LD8(work);

    R1 = state << 2;
    R4 = work;
    switch (state) {
    case 0: GUEST_CALL(sub_81179E0); break;
    case 1: GUEST_CALL(keyItemMenu_renderDescription_81179E4); break;
    case 2: GUEST_CALL(sub_8117A14); break;
    default: GUEST_CALL(sub_8117AB4); break; /* state 3; larger values are unreachable */
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8117B94 ── */
/* sub_8117B94(r0 = entry index, r6 = destination buffer):
 * Copies two 32-byte entries from the table at unk_2031204 into the buffer
 * pointed to by r6. The index is split by dividing r0 by 10 (BIOS Div):
 * the quotient selects the first entry and the remainder the second one.
 * On return r6 points past both copied entries; r7 is left untouched. */
Next sub_8117B94(void) {
    u32 dest = R6;

    /* svc 6 = Div: r0 = r0 / r1 (quotient), r1 = r0 % r1 (remainder). */
    R1 = 10;
    swi_call(6);
    u32 quotient = R0;
    u32 remainder = R1;

    /* First entry, skipped when the whole index fits in one table row. */
    if (quotient != 0) {
        R0 = A_unk_2031204 + (quotient << 5);
        R1 = dest;
        R2 = 0x20;
        GUEST_CALL(CopyByEightWords);
    }

    /* Second entry directly below the first one. */
    dest += 0x20;
    R0 = A_unk_2031204 + (remainder << 5);
    R1 = dest;
    R2 = 0x20;
    GUEST_CALL(CopyByEightWords);

    R6 = dest + 0x20;
    RETURN_TO_CALLER();
}

/* ── sub_8117E20 ── */
/* sub_8117E20: per-frame sprite refresh for a battle object.
 * r5 = object struct, r7+0xD = the object's current state byte.
 * The animation to show is the one stored at r5+4, or that plus 5 when the
 * state byte differs from the copy cached at r5+3 (i.e. the object changed
 * state since the animation was last picked). If the resulting animation
 * differs from the one actually loaded (cached at r5+0xC), load it and drop
 * the shadow; finally advance the sprite one frame. */
Next sub_8117E20(void) {
    u32 state       = LD8(R7 + 0xD);
    u32 baseAnim    = LD8(R5 + 4);
    u32 wantedAnim  = (state == LD8(R5 + 3)) ? baseAnim : baseAnim + 5;
    u32 currentAnim = LD8(R5 + 0xC);

    if (wantedAnim != currentAnim) {
        ST8(R5 + 0xC, wantedAnim);
        R0 = wantedAnim;
        R1 = currentAnim;
        GUEST_CALL(sprite_setAnimation);
        GUEST_CALL(sprite_loadAnimationData);
        GUEST_CALL(sprite_noShadow);
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_811A268 ── */
/* sub_811A268:
 * Fetch one entry from the table at dword_201FC20 (the table base IS the RAM
 * address dword_201FC20; the original computes the entry pointer by adding the
 * index directly to it). The entry index is the sum of the two halfwords at
 * offsets 0x40 and 0x3C of the active menu state block: sSubmenu while its
 * first byte equals 8, otherwise unk_2037780. Each entry is four bytes wide.
 *
 * Outputs: r0 = entry halfword at +0, r1 = entry byte at +2, r2 = entry byte
 *          at +3, r3 = pointer to the entry.
 * Flags:   nz set from r0 (as the original's tst r0, r0 does).
 * Preserved: r4-r7. */
Next sub_811A268(void) {
    u32 state = A_sSubmenu;
    if (LD8(state) != 8)
        state = A_unk_2037780;

    u32 index = LD16(state + 0x40) + LD16(state + 0x3C);
    u32 entry = A_dword_201FC20 + (index << 2);

    R0 = LD16(entry);
    R1 = LD8(entry + 2);
    R2 = LD8(entry + 3);
    R3 = entry;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811CBD0 ── */
/* sub_811CBD0 (r5 = object):
 * Copies the position of the sprite the object is currently tracking into
 * the object's own sprite, copies the tracked sprite's size halfwords into
 * the object (offsets 0x14 and 0x1A), shrinks the sprite, and starts a
 * 10-frame counter at r5+0xC. Always returns r0 = 1. */
Next sub_811CBD0(void) {
    /* Fetch a pointer to the sprite data the object is tracking. */
    GUEST_CALL(sub_811CC68);
    u32 tracked = R0;

    /* Move this object's sprite to the tracked sprite's position. */
    R0 = (u32)LDS16(tracked + 2);
    R1 = (u32)LDS16(tracked + 4);
    R2 = 2;
    GUEST_CALL(sprite_setCoordinates);

    /* Fetch the sprite data again and copy its two size halfwords into the
     * object. */
    GUEST_CALL(sub_811CC68);
    tracked = R0;
    u32 width = LD16(tracked + 6);
    ST16(R5 + 0x14, width);
    u32 scale = LD16(tracked + 8);
    ST16(R5 + 0x1A, scale);

    /* Shrink the sprite: scale parameters (0, scale >> 8, scale >> 8). */
    R0 = 0;
    R1 = scale >> 8;
    R2 = scale >> 8;
    GUEST_CALL(sprite_setScaleParameters);

    /* Start a 10-frame countdown in the object. */
    R0 = 10;
    GUEST_CALL(counter_addU32_r5Plus0C_811CC60);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811EC10 ── */
/* sub_811EC10 (r10 = toolkit): (re)initialise the PET menu screen.
 *
 * Zeroes the submenu's 0x80-byte work buffer and the PET menu state
 * struct, marks the menu data as active, seeds the scroll/cursor
 * position with -32, copies the two bytes describing the first menu
 * entry, and finally refreshes the menu's resources. */
Next sub_811EC10(void) {
    u32 savedR4 = R4;

    /* clear the submenu work buffer owned by the toolkit */
    R0 = LD32(R10 + oToolkit_SubmenuPtr);
    R1 = 0x80;
    GUEST_CALL(ZeroFillByWord);

    /* clear the PET menu state and flag it as in use */
    R4 = A_ePETMenuData;
    R0 = R4;
    R1 = 0x18;
    GUEST_CALL(ZeroFillByWord);

    R0 = 1;
    GUEST_CALL(SetPETMenuDataFlag);

    /* starting scroll/cursor offset: -32 */
    ST16(A_ePETMenuData + 0x10, 0xFFE0);

    /* look up the first menu entry's two bytes and store them */
    R0 = 0;
    R1 = A_byte_811EBDA;
    GUEST_CALL(sub_8120CC8);
    ST8(A_ePETMenuData + 0xA, R0);
    ST8(A_ePETMenuData + 0xB, R1);

    GUEST_CALL(sub_8046664);

    /* the entry-side r4 was only scratch; the original pops it back */
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sub_811F014 ── */
/* sub_811F014: refresh the map-name display.
 *
 * 1. sub_803D06C yields a pointer (the current map's name/script data) that
 *    the scene state keeps at +8.
 * 2. The map name table text is rendered as graphics into the tile block at
 *    0x6015240 via the scratch text buffer unk_200C0E0.
 * 3. sub_811F5DC copies the rendered name into the byte_200CF20 buffer. */
Next sub_811F014(void) {
    /* r4-r7 are used as call arguments below but must be preserved for the
     * caller, so keep the entry values aside and restore them before returning. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Current map name pointer -> scene state +8. */
    GUEST_CALL(sub_803D06C);
    u32 sceneState = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(sceneState + 8, R0);

    /* Render the map name text (script 0xF1, palette 7, one line) into VRAM. */
    R0 = A_text_86CB360;      /* TextScriptMapNames */
    R1 = 0xF1;
    R2 = A_unk_200C0E0;
    R3 = 0;
    R4 = 7;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* Copy the rendered name tiles into the map-name buffer byte_200CF20. */
    R0 = A_unk_200C0E0;
    R1 = A_byte_200CF20;
    R2 = 0x6015240;
    R3 = 7;
    GUEST_CALL(sub_811F5DC);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_811F050 ── */
/* sub_811F050: redraw the map-name text screen.
 * Calls sub_803D0F4 and stores its result into the toolkit's unk_200A220
 * struct, renders the "map names" text script into the unk_200C2A0 buffer,
 * then uploads the rendered tiles/tilemap to VRAM at 0x6015400. */
Next sub_811F050(void) {
    /* The original saves/restores r4-r7 around the calls; our callees
     * also expect the row/column arguments in those registers. */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(sub_803D0F4);

    u32 unk200a220 = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(unk200a220 + 8, R0);

    /* Render text script #0xF2 (map names) into the unk_200C2A0 buffer:
     * 6 rows starting at row 0, using the font data at dword_86A5D60. */
    R0 = A_text_86CB360;    /* TextScriptMapNames */
    R1 = 0xF2;
    R2 = A_unk_200C2A0;
    R3 = 0;
    R4 = 6;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* Upload the rendered text from unk_200C2A0 to the VRAM tile blocks
     * at 0x6015400, with byte_200D220 as the tilemap destination info. */
    R0 = A_unk_200C2A0;
    R1 = A_byte_200D220;
    R2 = 0x6015400;
    R3 = 6;
    GUEST_CALL(sub_811F5DC);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_811F08C ── */
/* sub_811F08C: redraw the name of the current map in the map-name display.
 *
 * sub_811F290 resolves the text-script entry for the map the player is
 * currently on; renderTextGfx_8045F8C draws that name into the text work
 * buffer at byte_200C620; sub_811F5DC then blits the rendered tiles into
 * the map-name VRAM slot at 0x6015780. */
Next sub_811F08C(void) {
    /* The original pushes/pops r4-r7 around the calls, so they must come
     * back out of this routine exactly as they came in. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Look up which map-name string to show. */
    GUEST_CALL(sub_811F290);
    u32 mapNameString = R0;

    /* renderTextGfx_8045F8C(stringTable, stringIndex, workBuffer,
     *                        0, 12, 1, fontTable, 0):
     * render the map name text into the work buffer. */
    R0 = A_text_86CB360;       /* TextScriptMapNames string table */
    R1 = mapNameString;
    R2 = A_byte_200C620;       /* rendered-text work buffer */
    R3 = 0;
    R4 = 0xC;                  /* render width */
    R5 = 1;
    R6 = A_dword_86A5D60;      /* font/metadata table */
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* sub_811F5DC(source, sourceMap, vramDest, tileCount):
     * upload the rendered name tiles to VRAM. */
    R0 = A_byte_200C620;
    R1 = A_byte_200C920;
    R2 = 0x6015780;
    R3 = 0xC;
    GUEST_CALL(sub_811F5DC);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_811F214 ── */
/* sub_811F214: walk the three-entry table at dword_811F234 and hand each
 * (word0, word1) pair to sub_802FE28 as (a1, a2, 0, 1).
 *
 * The original keeps the table cursor in r7 and the entry index in r6 and
 * calls sub_802FE28 with those registers live, so the loop is driven through
 * the guest registers as well; r4-r7 are saved and restored exactly like the
 * original's push/pop. */
Next sub_811F214(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R7 = A_dword_811F234;
    for (R6 = 0; (s32)R6 < 3; R6++) {
        R0 = LD32(R7);       /* entry->word0 */
        R1 = LD32(R7 + 4);   /* entry->word1 */
        R2 = 0;
        R3 = 1;
        GUEST_CALL(sub_802FE28);
        R7 += 8;             /* next table entry */
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}
