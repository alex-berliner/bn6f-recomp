/*
 * leaves_farm_14.c: small functions from the model farm, set 14.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_80DA9FE ── */
/* sub_80DA9FE(r0 = entry index):
 * Pick one of the eight halfwords inside the 16-byte table entry at
 * byte_80211A0 + index*16, using a positive RNG value masked to 0..7.
 * r0 = the selected halfword; r4 and r7 are untouched. */
Next sub_80DA9FE(void) {
    u32 entry = A_byte_80211A0 + ((u32)R0 << 4);

    GUEST_CALL(GetPositiveSignedRNG);

    R0 = LD16(entry + (R0 & 0xE));
    RETURN_TO_CALLER();
}

/* ── sub_80E0F2E ── */
/* sub_80E0F2E(r0 = selector, r5 = battle object):
 * Reset an object's extra-var timer to 0x15 (r0 == 1) or 8 (otherwise).
 * If the timer actually changed, reload the object's sprite animation
 * (sprite 0x80, animation 0x14), drop any queued animation, and clear
 * the "shadowless" flag (bit 3) in the object header.
 * Leaves r3 = value sprite_noShadow leaves behind, or the old timer
 * value when the early-exit path is taken; r4/r5 preserved. */
Next sub_80E0F2E(void) {
    u32 animId = (R0 == 1) ? 0x15 : 8;
    u32 oldTimer = LD32(R5 + oBattleObject_ExtraVars + 0xC);

    ST32(R5 + oBattleObject_ExtraVars + 0xC, animId);

    if (oldTimer != animId) {
        R0 = 0x80;
        R1 = 0x14;
        R2 = animId;
        GUEST_CALL(sprite_load);
        GUEST_CALL(sprite_loadAnimationData);
        GUEST_CALL(sprite_noShadow);

        ST8(R5 + oBattleObject_CurAnimCopy, 0xFF);

        /* oObjectHeader_Flags is at offset 0 of the object header */
        ST8(R5, LD8(R5) & ~8u);
    } else {
        /* early exit: original leaves the old timer value in r3 */
        R3 = oldTimer;
    }

    RETURN_TO_CALLER();
}

/* ── sub_80E3BEA ── */
/* sub_80E3BEA (r5 = battle object):
 * Update this object's position via sub_80E3D1A — first from its current
 * (X, Y), then again from the position advanced by its X velocity — and
 * then hand control to the handler for its current action, selected from
 * the two-entry jump table off_80E3C0C. The handler is entered with
 * r0 = CurAction (offset 9 in the battle object), r1 = its table entry
 * (thumb address), r2 = whatever sub_80E3D1A left there; everything else
 * passes through untouched. When the handler returns, return to
 * sub_80E3BEA's caller. */
Next sub_80E3BEA(void) {
    R0 = LD32(R5 + oBattleObject_X);
    R1 = LD32(R5 + oBattleObject_Y);
    GUEST_CALL(sub_80E3D1A);

    R0 += LD32(R5 + oBattleObject_XVelocity);
    GUEST_CALL(sub_80E3D1A);

    u32 action = LD8(R5 + 9);           /* oBattleObject_CurAction */
    R0 = action;
    R1 = LD32(A_off_80E3C0C + action);  /* handler address, index 0 or 1 */

    if (action == 0)
        GUEST_CALL(sub_80E3C14);
    else
        GUEST_CALL(sub_80E3CC4);

    RETURN_TO_CALLER();
}

/* ── sub_80EB194 ── */
/* Offsets/bit positions referenced by the assembly that aren't in game.h. */
#define OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT 19   /* lsl r0, r0, #19 */
#define oAIAttackVars_Unk_00 0                   /* strh r0, [r7,#0] */

/* sub_80EB194: per-frame tick of an AI attack action. R7 points at the object's
 * AI attack variables.
 *
 * Decrements the action timer (Unk_10) and leaves that value in R0. While it
 * is still counting down there is nothing else to do. Once it reaches zero the
 * object's "busy" flag (0x40) is cleared and the CURRENTLY_MOVING flag is set,
 * the timer is reloaded from Unk_18 (the reloaded value stays in R0 when the
 * AI-layer check sub_80F02A2 is called, as in the original), the animation is
 * reset if the check returns non-zero, and finally the action state (Unk_00)
 * is set to 0xC.
 *
 * Flags left behind: on the early-return path those of the decrement; on the
 * reset path nz from the final MOV (Thumb MOVS) and c/v from the last helper
 * call, matching the original. */
Next sub_80EB194(void) {
    /* timer--; done for this frame unless it just hit zero. R0 keeps the
     * decremented value, as the original leaves it there for callers. */
    R0 = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(R0, 1);               /* SUBS r0, #1 */
    R0 -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, R0);
    if (R0 != 0)
        RETURN_TO_CALLER();

    /* Timer expired: no longer blocked, and the object starts moving. */
    R0 = 0x40;
    GUEST_CALL(object_clearFlag);

    R0 = 1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
    GUEST_CALL(object_setFlag1);

    /* Reload the timer for the next attack and ask the AI layer about it.
     * R0 still holds the reloaded value when sub_80F02A2 is entered. */
    R0 = LD16(R7 + oAIAttackVars_Unk_18);
    ST16(R7 + oAIAttackVars_Unk_10, R0);
    GUEST_CALL(sub_80F02A2);

    if (R0 != 0) {
        R0 = 0;
        GUEST_CALL(object_setAnimation);
    }

    /* Back to the idle/step state. */
    R0 = 0xC;
    SET_FLAGS_NZ(R0);
    ST16(R7 + oAIAttackVars_Unk_00, R0);
    RETURN_TO_CALLER();
}
