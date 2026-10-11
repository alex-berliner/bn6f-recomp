/*
 * asm00_2_801728E.c: readable functions of bn6f asm00_2.s, 0x0801728E-0x0801B9E6, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_801728E ── */
/* register form: calls battle_testFlag0x40_800A8F8 outside this file, which has no typed C interface yet */
static bool battle_test_flag_0x40(void) {
    GUEST_CALL(battle_testFlag0x40_800A8F8);
    return !ZERO_FLAG();
}

/* register form: calls getEntryPtr_2036120_1D0_802E070 outside this file, which has no typed C interface yet */
static u8 get_entry_flag_byte(u8 alliance) {
    R0 = alliance;
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);
    return LD8(R0 + 0x50);
}

/* register form: calls battle_getStateUnk40_800A704 outside this file, which has no typed C interface yet */
static u32 battle_get_state_unk40(void) {
    GUEST_CALL(battle_getStateUnk40_800A704);
    return R0;
}

/* register form: calls sprite_setColorShader outside this file, which has no typed C interface yet */
static void sprite_set_color_shader(u32 arg) {
    R0 = arg;
    GUEST_CALL(sprite_setColorShader);
}

/* 32-entry palette ramp used to drive the dimmer shader. */
static const u8 kPalRamp[32] = {
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
    0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E,
    0x1E, 0x1C, 0x1A, 0x18, 0x16, 0x14, 0x12, 0x10,
    0x0E, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x00,
};

/* Apply the per-battle dimmer shader to the current object's sprite when the
 * global test-flag 0x40 is set, the object has a non-idle current action, and
 * its alliance's entry table byte at offset 0x50 is non-zero. The shader input
 * is ramp[idx]*33, where idx = (battleState & 0x1F). */
static u32 sub_801728E_core(u32 r3_in, u32 battle_obj, u32 r10_unused) {
    (void)r10_unused;
    if (!battle_test_flag_0x40())
        return r3_in;
    if (LD8(battle_obj + oBattleObject_CurAction) == 0)
        return r3_in;
    u8 alliance = LD8(battle_obj + oBattleObject_Alliance);
    if (get_entry_flag_byte(alliance) == 0)
        return r3_in;
    u32 state = battle_get_state_unk40();
    u8 ramp_idx = (u8)(state & 0x1F);
    u8 ramp_val = kPalRamp[ramp_idx];
    u32 shader_arg = (u32)ramp_val + ((u32)ramp_val << 5);
    sprite_set_color_shader(shader_arg);
    return r3_in;
}

Next sub_801728E(void) {
    R3 = sub_801728E_core(R3, R5, R10);
    SET_FLAGS_NZ(R3);
    RETURN_TO_CALLER();
}

/* ── battle_objectTickTimerAndSetPhaseC_80174AA ── */
/* battle_objectTickTimerAndSetPhaseC_80174AA (r5 = battle object): tick the object's action timer down by
 * one. When it reaches zero, reset the timer and advance the object to
 * phase 12 (0xC). r0 and the flags reflect the decrement, as in the original
 * (flags come from the SUBS; the expired path then re-runs them through the
 * phase write, which only refreshes nz). */
Next battle_objectTickTimerAndSetPhaseC_80174AA(void) {
    u32 timer = LD16(R5 + oBattleObject_Timer);
    u32 remaining = timer - 1;

    ST16(R5 + oBattleObject_Timer, remaining);
    R0 = remaining;
    SET_FLAGS_CMP(timer, 1); /* flags of "subs r0, #1" */

    if (remaining == 0) {
        /* Timer expired: restart it and switch to phase 12. */
        ST16(R5 + oBattleObject_Timer, 0);
        R0 = 0xC;
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80174BE ── */
/* register form: calls sprite_setMosaicSize outside this file, which has no typed C interface yet
 * (original sets both r0 and r1 to the halved timer before the call) */
static void call_sprite_setMosaicSize(u32 half) {
    R0 = half;
    R1 = half;
    GUEST_CALL(sprite_setMosaicSize);
}

/* register form: calls sprite_setAlpha_8002c7a outside this file, which has no typed C interface yet */
static u32 call_sprite_setAlpha(u32 alpha) {
    R0 = alpha;
    GUEST_CALL(sprite_setAlpha_8002c7a);
    return R0;
}

/* register form: calls sprite_clearMosaic outside this file, which has no typed C interface yet */
static void call_sprite_clearMosaic(void) {
    GUEST_CALL(sprite_clearMosaic);
}

/* register form: calls sprite_disableAlpha outside this file, which has no typed C interface yet */
static void call_sprite_disableAlpha(void) {
    GUEST_CALL(sprite_disableAlpha);
}

/* register form: calls battle_ClearAllianceTargetSlotIfSelf_802CDD0 outside this file, which has no typed C interface yet */
static void call_battle_ClearAllianceTargetSlotIfSelf(void) {
    GUEST_CALL(battle_ClearAllianceTargetSlotIfSelf_802CDD0);
}

/* register form: calls battle_getObjectNameID_8011020 outside this file, which has no typed C interface yet */
static void call_battle_getObjectNameID(void) {
    GUEST_CALL(battle_getObjectNameID_8011020);
}

/* Per-branch output: the value to leave in R0 plus the CMP operands whose subtraction
 * reproduces the original's outgoing flags, so the adapter can apply a single SET_FLAGS. */
typedef struct {
    u32 r0;
    u32 cmp_a;
    u32 cmp_b;
} IntroResult;

/* Tick a battle object's appearance timer. While it ramps from 0 to 0x20, shrink the sprite
 * mosaic and fade its alpha. At 0x20 the intro finishes: clear the effects, drop the alive
 * flag, refresh the object's name, and mark the state action phase as initialized. */
static IntroResult tick_intro(u32 object_ptr) {
    u32 timer = (u32)LD16(object_ptr + oBattleObject_Timer) + 1;
    ST16(object_ptr + oBattleObject_Timer, (u16)timer);

    if (timer == 0x20) {
        call_sprite_clearMosaic();
        call_sprite_disableAlpha();
        call_battle_ClearAllianceTargetSlotIfSelf();

        u8 flags = LD8(object_ptr + oObjectHeader_Flags);
        ST8(object_ptr + oObjectHeader_Flags, flags & (u8)~2);

        call_battle_getObjectNameID();
        ST32(object_ptr + oBattleObject_CurStateActionPhaseAndPhaseInitialized, 8);

        /* Flags inherited from CMP r0,#0x20 (equal: Z=1, C=1). The trailing `mov r0,#8`
         * does not update flags, so the outgoing state matches CMP(0x20, 0x20). */
        IntroResult r = { 8, 0x20, 0x20 };
        return r;
    }

    /* Mid-fade: mosaic halves each tick, alpha ramps from 0x10 toward 0. */
    u32 half = timer >> 1;
    call_sprite_setMosaicSize(half);
    u32 r0 = call_sprite_setAlpha(0x10 - half);

    /* Flags come from `neg r0,r0` (RSBS r0,r0,#0) where r0 had just been (half - 0x10);
     * that's equivalent to CMP #0, (half - 0x10). R0 is the clobbered return from setAlpha. */
    IntroResult res = { r0, 0, half - 0x10 };
    return res;
}

Next sub_80174BE(void) {
    IntroResult res = tick_intro(R5);
    R0 = res.r0;
    SET_FLAGS_CMP(res.cmp_a, res.cmp_b);
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

/* ── getIndexedSignedBytePair_8018842 ── */
/* getIndexedSignedBytePair_8018842(r0, r1 = arguments for the lookup, r2 = entry index, r4 = unused):
 * Looks up a two-byte table entry: the entry base comes from sub_8018886
 * (called with r0/r1 as passed in), offset by twice the index in r2.
 * Outputs: r0 = first byte (signed), r1 = second byte (signed) of the entry.
 * r2/r3/r4 are left untouched. */
Next getIndexedSignedBytePair_8018842(void) {
    u32 addr = R2 + R2;

    GUEST_CALL(sub_8018886);
    addr += R0;

    R0 = (s8)LD8(addr);
    R1 = (s8)LD8(addr + 1);
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

/* ── battle_SetObjectHitModifier_8019F86 ── */
/* battle_SetObjectHitModifier_8019F86(r0 = hit modifier value, r5 = battle object):
 * store the object's hit modifier into its collision data.
 * Returns (r1) the pointer to the collision data that was written. */
Next battle_SetObjectHitModifier_8019F86(void) {
    u32 *collisionData = (u32 *)LD32(R5 + oBattleObject_CollisionDataPtr);
    ST8((u32)collisionData + oCollisionData_HitModifierBase, R0);
    R1 = (u32)collisionData;
    RETURN_TO_CALLER();
}

/* ── battleObject_SetSecondaryElementWeakness_8019F9E ── */
/* battleObject_SetSecondaryElementWeakness_8019F9E(r0 = element, r5 = battle object):
 * set the object's secondary element weakness in its collision data. */
Next battleObject_SetSecondaryElementWeakness_8019F9E(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST8(collisionData + oCollisionData_SecondaryElementWeakness, R0);
    RETURN_TO_CALLER();
}

/* ── clearScratchBuffer_2034F60_8019FA4 ── */
/* clearScratchBuffer_2034F60_8019FA4: wipe the 0xA0-byte scratch buffer at unk_2034F60.
 * Inputs: r2, r3 (passed through untouched). */
Next clearScratchBuffer_2034F60_8019FA4(void) {
    R0 = A_unk_2034F60;
    R1 = 0xA0;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── object_removeCollisionData ── */
/* object_removeCollisionData (0x0801A00E):
 * THUMB trampoline into the ARM helper _object_removeCollisionData.
 * The helper is invoked with the caller's r4-r7 (and r10) still in place as
 * its arguments; the surrounding push/pop in the original exists only to
 * preserve r4-r7 and the original return address across the call, since the
 * ARM helper does not follow the standard callee-saved register discipline.
 * Results come back in r1-r3 and the condition flags; r4-r7 are unchanged. */
Next object_removeCollisionData(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    GUEST_CALL(_object_removeCollisionData);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
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

/* ── object_updateCollisionPanels ── */
/* object_updateCollisionPanels(r4/r6 = scratch (restored), r5 = battle object):
 * Copies the object's current panel coordinates into its collision data record,
 * remembers the previous coordinates, then asks sub_800E994 which way the
 * object should face for the move from the old panel to the new one and stores
 * that as the collision data's direction.
 * Returns r0 = direction, r1/r2/r3 = new Y / old X / old Y (as passed to the
 * helper), with r4 and r6 preserved and the helper's flags left intact. */
Next object_updateCollisionPanels(void) {
    u32 object = R5;
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    u32 collision = LD32(object + oBattleObject_CollisionDataPtr);

    /* Previous panel position stored in the collision data. */
    u32 prevPanelX = LD8(collision + oCollisionData_PanelX);
    u32 prevPanelY = LD8(collision + oCollisionData_PanelY);

    /* Overwrite it with the object's current panel position. */
    u32 panelX = LD8(object + oBattleObject_PanelX);
    ST8(collision + oCollisionData_PanelX, panelX);
    u32 panelY = LD8(object + oBattleObject_PanelY);
    ST8(collision + oCollisionData_PanelY, panelY);

    /* direction = facing for the step (prevX, prevY) -> (panelX, panelY) */
    R0 = panelX;
    R1 = panelY;
    R2 = prevPanelX;
    R3 = prevPanelY;
    R4 = LD8(object + oBattleObject_Alliance);
    R6 = collision;
    GUEST_CALL(sub_800E994);
    u32 direction = R0;

    /* Restore the caller's r4/r6, as the original's push/pop does. */
    R4 = savedR4;
    R6 = savedR6;

    ST8(collision + oCollisionData_Direction, direction);
    RETURN_TO_CALLER();
}

/* ── object_setCollisionPanelsToCurrent ── */
/* object_setCollisionPanelsToCurrent(r4 = ?, r5 = battle object, lr = return):
 * Copy the object's current panel coordinates into its collision data record.
 * Leaves r0 = panel X, r1 = panel Y, r2 = collision data pointer; preserves r4. */
Next object_setCollisionPanelsToCurrent(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);

    u32 panelX = LD8(R5 + oBattleObject_PanelX);
    ST8(collision + oCollisionData_PanelX, panelX);
    R0 = panelX;

    u32 panelY = LD8(R5 + oBattleObject_PanelY);
    ST8(collision + oCollisionData_PanelY, panelY);
    R1 = panelY;

    R2 = collision;
    R4 = R4; /* r4 is preserved */
    RETURN_TO_CALLER();
}

/* ── object_clearCollisionRegion ── */
/* object_clearCollisionRegion(r5 = battle object):
 * Zero the object's collision region byte (offset 1 in its collision data),
 * leaving the collision data pointer in r0 and 0 in r1
 * (flags set as by "mov r1, #0"). */
Next object_clearCollisionRegion(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    R0 = collisionData;
    R1 = 0;
    ST8(collisionData + 1, R1); /* oCollisionData_Region == 1 */

    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── object_setCollisionRegion ── */
/* object_setCollisionRegion(r0 = region index, r5 = battle object):
 * Write the region index into the object's collision data
 * (offset oCollisionData_Region == 1, no macro is defined for it).
 * Returns r1 = the object's collision data pointer. */
Next object_setCollisionRegion(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST8(collisionData + 1 /* oCollisionData_Region */, R0);

    R1 = collisionData;
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

/* ── object_getTableEntry_801A0BA ── */
/* object_getTableEntry_801A0BA(r0 = column index, r1 = row index):
 * fetch one 32-bit entry from the two-dimensional table at byte_8019C7C
 * (8 bytes per row, 4 bytes per entry). Leaves r0 = table entry,
 * r1 = byte offset of the entry within the table, r3 = table base.
 * The C/V flags come from the offset addition, as in the original. */
Next object_getTableEntry_801A0BA(void) {
    u32 column = R0;
    u32 row = R1;

    R0 = column << 2;            /* byte offset of the entry within its row */
    R3 = A_byte_8019C7C;         /* base of the lookup table */
    R1 = row << 3;               /* byte offset of the row */

    /* add r1, r1, r0 — flags come from the original operand values */
    SET_FLAGS_ADD(R1, R0);
    R1 = R1 + R0;                /* absolute offset of the entry in the table */

    R0 = LD32(R3 + R1);          /* the table entry itself */

    RETURN_TO_CALLER();
}

/* ── object_setCollisionHitEffect ── */
/* object_setCollisionHitEffect(r0 = hit effect, r5 = battle object):
 * store the hit-effect type into the object's collision data.
 * r3 = collision data pointer. */
Next object_setCollisionHitEffect(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    R3 = collisionData;
    ST8(collisionData + 9, R0); /* offset 9 = oCollisionData_HitEffect */

    RETURN_TO_CALLER();
}

/* ── object_setFlag1 ── */
/* object_setFlag1(r0 = flag bits, r5 = battle object):
 * OR the given bits into the object's CollisionData flags1 word.
 * Returns: r1 = collision data pointer, r2 = updated flags word, nz = flags word. */
Next object_setFlag1(void) {
    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags1) | R0;
    ST32(R1 + oCollisionData_ObjectFlags1, R2);
    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── object_clearFlag ── */
/* object_clearFlag(r0 = flag mask, r5 = battle object):
 * clear the given bits of the object's ObjectFlags1 in its collision data.
 * Returns r1 = collision data pointer, r2 = updated flags; NZ set on the flags value. */
Next object_clearFlag(void) {
    u32 flagsMask = R0;

    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags1);
    R2 &= ~flagsMask;
    SET_FLAGS_NZ(R2);
    ST32(R1 + oCollisionData_ObjectFlags1, R2);

    RETURN_TO_CALLER();
}

/* ── object_getFlag ── */
/* object_getFlag(r5 = battle object):
 * returns the object's first collision flag word from its collision data. */
Next object_getFlag(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    R0 = LD32(collisionData + oCollisionData_ObjectFlags1);
    RETURN_TO_CALLER();
}

/* ── object_setFlag2 ── */
/* object_setFlag2(r0 = flag bits, r5 = battle object):
 * set the given bits in the object's "flags 2" field of its collision data.
 * Returns r1 = collision data pointer, r2 = updated flags, and NZ from the ORR. */
Next object_setFlag2(void) {
    u32 flags = R0;

    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags2) | flags;
    ST32(R1 + oCollisionData_ObjectFlags2, R2);

    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── object_clearFlag2 ── */
/* object_clearFlag2(r0 = flag bits to clear, r5 = battle object):
 * Clears the given bits in the object's ObjectFlags2 field.
 * Leaves r1 = collision data pointer, r2 = new flags value, NZ from that value. */
Next object_clearFlag2(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 flags = LD32(collision + oCollisionData_ObjectFlags2) & ~R0;

    ST32(collision + oCollisionData_ObjectFlags2, flags);

    R1 = collision;
    R2 = flags;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── object_getFlag2 ── */
/* object_getFlag2: read the ObjectFlags2 field of the object's collision data.
 * Inputs:  r5 = battle object
 * Outputs: r0 = ObjectFlags2 */
Next object_getFlag2(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R0 = LD32(collision + oCollisionData_ObjectFlags2);
    RETURN_TO_CALLER();
}

/* ── battle_clearObjectStatusFlagsAndTimers_801A264 ── */
/* battle_clearObjectStatusFlagsAndTimers_801A264(r5 = battle object): clear the object's active status flags
 * (both flag words) and reset all of its collision timers (blind, etc.).
 * Leaves r0 = 0, r3 = pointer to the collision data, and Z set (from r0 = 0). */
Next battle_clearObjectStatusFlagsAndTimers_801A264(void) {
    /* Clear status bits in the object's two flag words. */
    R0 = 0x8001E800;
    GUEST_CALL(object_clearFlag);

    R0 = 0x000300E8;
    GUEST_CALL(object_clearFlag2);

    R0 = 0;
    SET_FLAGS_NZ(R0);

    /* Zero out every timer field of the collision data. */
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    ST16(collision + oCollisionData_Unk_1c, 0);
    ST16(collision + oCollisionData_Unk_1e, 0);
    ST16(collision + oCollisionData_BlindTimer, 0);
    ST16(collision + oCollisionData_Unk_22, 0);
    ST16(collision + oCollisionData_Unk_2a, 0);
    ST16(collision + oCollisionData_Unk_2c, 0);

    RETURN_TO_CALLER();
}

/* ── battle_object_clearFlagsAndCollisionCounter_801A284 ── */
/* battle_object_clearFlagsAndCollisionCounter_801A284(r5 = battle object):
 * Clear the object's 0x800 status flag and its secondary flag 8, then zero
 * the halfword at oCollisionData_Unk_1c of its collision data.
 * Returns r3 = pointer to the object's collision data. */
Next battle_object_clearFlagsAndCollisionCounter_801A284(void) {
    R0 = 0x800;
    GUEST_CALL(object_clearFlag);

    R0 = 8;
    GUEST_CALL(object_clearFlag2);

    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collision + oCollisionData_Unk_1c, 0);
    R3 = collision;

    RETURN_TO_CALLER();
}

/* ── battleObject_clearHitFlags_801A29A ── */
/* battleObject_clearHitFlags_801A29A(r5 = battle object): clear the object's flag bits 0x10000 in
 * both flag words and reset the halfword at oCollisionData_Unk_2a of its
 * collision data. Leaves r0 = 0 (flags updated accordingly). */
Next battleObject_clearHitFlags_801A29A(void) {
    R0 = 0x10000;
    GUEST_CALL(object_clearFlag);

    R0 = 0x10000;
    GUEST_CALL(object_clearFlag2);

    R0 = 0;
    R3 = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(R3 + oCollisionData_Unk_2a, R0);

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battleObject_resetCollisionState_801A2B0 ── */
/* battleObject_resetCollisionState_801A2B0(r5 = battle object):
 * Reset an object's collision state: clear its status flag bits, zero the
 * collision timer, and restore its Z coordinate from its AI data block.
 * Leaves r0 = restored Z value, r3 = collision data pointer. */
Next battleObject_resetCollisionState_801A2B0(void) {
    /* Clear the object's main and secondary status flags. */
    R0 = 0x80000000;
    GUEST_CALL(object_clearFlag);
    R0 = 0x20000;
    GUEST_CALL(object_clearFlag2);

    /* Zero the collision data timer at offset 0x2c. */
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collisionData + oCollisionData_Unk_2c, 0);

    /* Restore the object's Z position from the AI data block. */
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 z = LD16(aiData + oAIData_Unk_3c);
    ST16(R5 + oBattleObject_Z16, z);

    R3 = collisionData;
    R0 = z;
    RETURN_TO_CALLER();
}

/* ── battle_toggleAllianceAndAccumulateCollisionToAiData_801A308 ── */
/* battle_toggleAllianceAndAccumulateCollisionToAiData_801A308 (r5 = battle object):
 * Flip the object's alliance (player <-> enemy), fetch the AI data block for
 * the new side via battle_getPlayerAIData_8010022, then add the collision halfword at
 * CollisionData+0x92 into the AI's byte at oAIData_Unk_10.
 * Preserves r4/r5/r6; returns nothing. */
Next battle_toggleAllianceAndAccumulateCollisionToAiData_801A308(void) {
    u32 savedR4 = R4;
    u32 object = R5;

    /* Toggle the alliance field and get the matching AI data pointer. */
    R0 = LD8(object + oBattleObject_Alliance) ^ 1;
    GUEST_CALL(battle_getPlayerAIData_8010022);
    u32 aiData = R0;

    /* Accumulate the collision value into the AI data byte. */
    u32 collisionData = LD32(object + oBattleObject_CollisionDataPtr);
    u32 collisionValue = LD16(collisionData + 0x92);
    ST8(aiData + oAIData_Unk_10, LD8(aiData + oAIData_Unk_10) + collisionValue);

    R4 = savedR4;
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

/* ── sub_801A506 ── */
/* register form: calls SetAIData_Unk_44_Flag outside this file, which has no typed C interface yet */
static void set_ai_data_unk44_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(SetAIData_Unk_44_Flag);
}

/* If the battle object's collision data has both a non-zero byte at 0x75 and a non-zero
 * halfword at 0x80, set the AI data's unk_44 flag to 0x80000000. Returns the collision
 * data pointer (R3) — the original always loads it before any branch, so callers see the
 * same value either way. */
static u32 maybe_set_ai_unk44_flag(u32 battle_object_ptr) {
    u32 collision_data = LD32(battle_object_ptr + oBattleObject_CollisionDataPtr);
    u8  byte_75 = LD8(collision_data + 0x75);
    if (byte_75 == 0) return collision_data;
    u16 half_80 = LD16(collision_data + 0x80);
    if (half_80 == 0) return collision_data;
    set_ai_data_unk44_flag(0x80000000);
    return collision_data;
}

Next sub_801A506(void) {
    R3 = maybe_set_ai_unk44_flag(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_applyStatusEffect_801A554 ── */
/* battleObject_applyStatusEffect_801A554: apply the battle object's current status effect (r5 = object).
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
Next battleObject_applyStatusEffect_801A554(void) {
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

/* ── battle_ClearObjectCollisionResult_801A5E2 ── */
/* battle_ClearObjectCollisionResult_801A5E2(r5 = battle object):
 * Reset the object's collision result: clear the collision data fields at
 * offsets 0x48 and 0x4C (the field at 0x4C directly follows the one at 0x48). */
Next battle_ClearObjectCollisionResult_801A5E2(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST32(collisionData + oCollisionData_Unk_48, 0);
    ST32(collisionData + oCollisionData_Unk_48 + 4, 0); /* offset 0x4C */

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
    GUEST_CALL(battle_isPaused); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG())
        RETURN_TO_CALLER();

    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    u32 flashTimer = LD16(collision + oCollisionData_FlashingInvisTimer);
    u32 hitFlags = LD32(collision + oCollisionData_FlagsFromCollision);

    if (flashTimer != 0 && (hitFlags & 0x4) && !(hitFlags & 0x1000))
        ST16(collision + oCollisionData_FlashingInvisTimer, 0);

    RETURN_TO_CALLER();
}

/* ── battle_resetFlashingInvisTimer_801A67E ── */
/* battle_resetFlashingInvisTimer_801A67E(r5 = battle object):
 * Reset the object's "flashing while invisible" timer to zero.
 * Returns the collision data pointer in r3. */
Next battle_resetFlashingInvisTimer_801A67E(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collision + oCollisionData_FlashingInvisTimer, 0);
    R3 = collision;
    RETURN_TO_CALLER();
}

/* ── battle_incrementAllianceStatOnCollisionHit_801A6B4 ── */
/* battle_incrementAllianceStatOnCollisionHit_801A6B4(r5 = battle object):
 * Check the object's collision data (battle_checkHPDigit4State_801A6E8); if it reports a hit,
 * increment the navi stat byte at offset 0x18 of the object's alliance
 * stats, capped at 7. R4 (the collision data pointer) is restored on return. */
Next battle_incrementAllianceStatOnCollisionHit_801A6B4(void) {
    u32 savedR4 = R4;

    R4 = LD32(R5 + oBattleObject_CollisionDataPtr);
    GUEST_CALL(battle_checkHPDigit4State_801A6E8);

    if (R0 != 0) {
        R1 = 0x18;
        GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

        u32 level = R0 + 1;
        if ((s32)level > 7)
            level = 7;

        R1 = 0x18;
        R2 = level;
        GUEST_CALL(SetBattleNaviStatsByte_AllianceFromBattleObject);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_objectDieOnCollisionHit_801A6D6 ── */
/* battle_objectDieOnCollisionHit_801A6D6(r5 = battle object):
 * Check the object's collision data (passed to battle_checkHPDigit4State_801A6E8 in r4); if it
 * reports a hit, the object's HP is zeroed. r4 is saved and restored around
 * the call, r7 passes through untouched. */
Next battle_objectDieOnCollisionHit_801A6D6(void) {
    u32 savedR4 = R4;

    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    R4 = collisionData;

    GUEST_CALL(battle_checkHPDigit4State_801A6E8);

    if (R0 != 0)
        ST16(R5 + oBattleObject_HP, 0);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_checkHPDigit4State_801A6E8 ── */
/* battle_checkHPDigit4State_801A6E8: check the custom-panel state byte at +0xA4 of the object in R4.
 * - 0xF4 (panel flagged): return 1.
 * - 0xF7 (panel awaiting check): convert R5's HP to BCD and look for a "4"
 *   digit (a bug/frag digit appearing in the HP readout). If one is found,
 *   return 1; otherwise clear the state byte and return 0.
 * - anything else: return 0.
 * R0 = result; R2/R3 and R4/R5/R7 are left as the original leaves them. */
Next battle_checkHPDigit4State_801A6E8(void) {
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

/* ── battle_buffTwoStatsPlus2OnEventF6_801A720 ── */
/* battle_buffTwoStatsPlus2OnEventF6_801A720(r5 = battle object):
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

Next battle_buffTwoStatsPlus2OnEventF6_801A720(void) {
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

/* ── battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A ── */
/* battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A(r2 = column index, r4 = caller value (preserved), r5 = battle object):
 * When the object's collision data says it is standing on a specific kind of panel
 * (byte at +0xA4 equal to 0xF6), aim the AI's Unk_12 marker two columns ahead,
 * saturating at 7, and run sub_801A77A with the AI data / collision data / object
 * pointers. Otherwise just fall through, leaving the compare flags behind for the
 * caller. r2 (and c/v when the callee runs) come back from sub_801A77A. */
Next battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A(void) {
    u32 saved_r4 = R4;
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 collisionByte = LD8(collisionData + 0xA4);

    if (collisionByte == 0xF6) {
        u32 target;
        R3 = LD32(R5 + oBattleObject_AIDataPtr);
        /* The original reads the old Unk_12 value, but it is dead: r0 is
         * immediately overwritten by the add below. */
        target = R2 + 2;
        if ((s32)target > 7)
            target = 7;
        ST8(R3 + oAIData_Unk_12, target);
        R4 = collisionData;      /* sub_801A77A expects the collision data pointer in r4 */
        GUEST_CALL(sub_801A77A);
        R4 = saved_r4;           /* restore the caller's r4, as the original's pop does */
    } else {
        SET_FLAGS_CMP(collisionByte, 0xF6);
    }
    RETURN_TO_CALLER();
}

/* ── battle_clearAIDataAndBarrier_801A7F4 ── */
/* battle_clearAIDataAndBarrier_801A7F4(r5 = battle object):
 * Clear the object's AI state field at oAIData_Unk_60 and drop its
 * barrier flag (offset 6 in its collision data). */
Next battle_clearAIDataAndBarrier_801A7F4(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST32(aiData + oAIData_Unk_60, 0);
    ST8(collision + 6, 0);

    RETURN_TO_CALLER();
}
