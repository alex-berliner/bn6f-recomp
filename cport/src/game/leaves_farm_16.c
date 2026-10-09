/*
 * leaves_farm_16.c: small functions from the model farm, set 16.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_80C6CBA ── */
/* Field offsets/state constants used here that game.h spells differently
 * (the assembler listing calls them oBattleObject_Param1 /
 * oBattleObject_CurStateActionPhaseAndPhaseInitialized / CUR_STATE_DESTROY):
 *   +0x04: Param1            (u8, per-object behaviour parameter)
 *   +0x08: current state word (CUR_STATE_DESTROY = 8 marks the object dead) */
#define oBattleObject_Param1 4
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized 8
#define CUR_STATE_DESTROY 8

/* sub_80C6CBA(r5 = battle object):
 * Death tick. Objects with Param1 < 12 vanish as soon as their sprite
 * animation reports the end-of-animation bit (0x80); objects with
 * Param1 >= 12 linger for Timer2 more ticks first. Once the trigger fires,
 * the object is parked offscreen (X = 0xF0), its collision region is
 * cleared, and it is marked destroyed. Flags: nz from the last test/mov,
 * c/v carried over from the getFrameParameters call or the timer subtract. */
Next sub_80C6CBA(void) {
    if (LD8(R5 + oBattleObject_Param1) < 0xC) {
        /* Ask the current sprite whether its animation has finished. */
        GUEST_CALL(sprite_getFrameParameters);
        R1 = 0x80;
        SET_FLAGS_NZ(R0 & R1);
        if ((R0 & 0x80) == 0)
            RETURN_TO_CALLER();          /* still animating, keep the object */
    } else {
        /* Linger phase: count Timer2 down to zero before vanishing. */
        u32 timer = LD16(R5 + oBattleObject_Timer2);
        SET_FLAGS_CMP(timer, 1);
        R0 = timer - 1;
        ST16(R5 + oBattleObject_Timer2, R0);
        if (R0 != 0)
            RETURN_TO_CALLER();          /* time not up yet */
    }

    /* Vanish: park offscreen, drop the hitbox, mark the object destroyed. */
    ST16(R5 + oBattleObject_X16, 0xF0);
    GUEST_CALL(object_clearCollisionRegion);
    R0 = CUR_STATE_DESTROY;
    SET_FLAGS_NZ(R0);
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, R0);
    RETURN_TO_CALLER();
}

/* ── sub_80DA974 ── */
/* sub_80DA974 (r5 = battle object, r10 = toolkit):
 * Death phase of an object that lingers briefly before despawning.
 * On the first entry it copies the object's halfword parameter at +4 into
 * the battle state, switches to animation 1, loads a 90-frame timer,
 * clears its collision region and marks the phase initialized (4).
 * Every entry then counts the timer down; once it expires the object is
 * switched to the DESTROY state (offset +8 = 8).
 * Outputs: R0 = decremented timer (or 8 on the destroy path), R1 = the
 * parameter value on the setup path, flags from the decrement (or from
 * `mov r0, #8` on the destroy path; c/v untouched there). */
#define CUR_STATE_DESTROY_sub_80DA974 8

Next sub_80DA974(void) {
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        /* one-time setup for this phase */
        u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
        R1 = LD16(R5 + 4); /* oBattleObject_Param1 */
        ST16(battle + oBattleState_Unk_36, R1);

        ST8(R5 + oBattleObject_CurAnim, 1);
        ST16(R5 + oBattleObject_Timer, 0x5A);

        GUEST_CALL(object_clearCollisionRegion);

        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    }

    /* count the remaining time down; keep going while it is still positive */
    u32 timer = LD16(R5 + oBattleObject_Timer);
    SET_FLAGS_CMP(timer, 1); /* subs r0, #1 */
    timer -= 1;
    ST16(R5 + oBattleObject_Timer, timer);
    R0 = timer;

    if ((s32)timer > 0)
        RETURN_TO_CALLER();

    /* timer expired: switch the object to the destroy state */
    R0 = CUR_STATE_DESTROY_sub_80DA974;
    SET_FLAGS_NZ(R0); /* mov r0, #CUR_STATE_DESTROY_sub_80DA974 only updates n/z */
    ST32(R5 + 8 /* oBattleObject_CurStateActionPhaseAndPhaseInitialized */, R0);
    RETURN_TO_CALLER();
}

/* ── sub_81169D8 ── */
/* sub_81169D8: run one step of an object's exit/disappear sequence.
 * While the signed 16-bit flag at r7+0x10 is set, do nothing. Otherwise
 * count the byte timer at r5+0xC down; when it reaches zero, put the
 * object into state 4 with animation id 0x23 and reload its sprite
 * (animation 1, no shadow).
 *
 * r4/r5 are preserved; r2/r3 are never touched. r0/r1 keep the values
 * the original leaves behind on the early-exit paths. */
Next sub_81169D8(void) {
    /* Nothing to do while the +0x10 flag is set. (r0 holds the 0x10
     * offset the original computes before the load.) */
    R0 = 0x10;
    s32 flag = LDS16(R7 + 0x10);
    R1 = (u32)flag;
    SET_FLAGS_NZ(flag);
    if (flag != 0)
        RETURN_TO_CALLER();

    /* Tick the timer down; still busy until it hits zero. */
    u32 timer = LD8(R5 + 0xC);
    R0 = timer - 1;
    ST8(R5 + 0xC, timer - 1);
    SET_FLAGS_CMP(timer, 1);
    if (timer != 1)
        RETURN_TO_CALLER();

    /* Timer expired: switch to the exit state and reset the sprite. */
    ST8(R5 + 9, 4);
    ST8(R5 + 0, 0x23);
    R0 = 1;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    RETURN_TO_CALLER();
}

/* ── sub_8116F34 ── */
/* Jump table jt_8116F4C: Thumb entry points of the per-object update handlers,
 * exactly as they appear in the table the original indexes through. */
enum {
    ADDR_sub_8116F60                       = 0x08116F60 + 1,
    ADDR_battle_objectUpdateWithFieldC_8116F8C = 0x08116F8C + 1,
    ADDR_sub_8116F9C                       = 0x08116F9C + 1,
};

/* sub_8116F34: run one battle object's type-specific update, then its sprite update.
 * R5 points at the battle object; the byte at obj+4 selects which handler runs:
 *   0/3 -> sub_8116F60, 1/4 -> battle_objectUpdateWithFieldC_8116F8C, 2 -> sub_8116F9C.
 * For fidelity with the original `mov lr, pc; bx r0`, each handler is entered with
 * R0 = its own jump-table entry and R1 = type*4 (result of the `lsl r1, r1, #2`).
 * Tail behaviour: after the handler returns, sprite_update is called on the object. */
Next sub_8116F34(void) {
    u32 kind = LD8(R5 + 4);

    switch (kind) {
    case 0:
    case 3:
        R0 = ADDR_sub_8116F60;
        R1 = kind << 2;
        GUEST_CALL(sub_8116F60);
        break;

    case 1:
    case 4:
        R0 = ADDR_battle_objectUpdateWithFieldC_8116F8C;
        R1 = kind << 2;
        GUEST_CALL(battle_objectUpdateWithFieldC_8116F8C);
        break;

    case 2:
        R0 = ADDR_sub_8116F9C;
        R1 = kind << 2;
        GUEST_CALL(sub_8116F9C);
        break;

    default:
        /* Unreachable: the object's update kind is always 0..4. (The original
         * would index past jt_8116F4C and jump to garbage.) */
        break;
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── sub_8117084 ── */
/* sub_8117084(r0 = halfword A, r1 = halfword B, r2 = first struct index):
 * Walks the three struct indices r2..r2+2. For each index, looks up the
 * corresponding struct from the pool at 0x2008450; while a struct is found,
 * its halfwords at +0xC and +0xE are filled with the two values passed in.
 * Stops at the first index that has no struct (lookup returns r0 == 0).
 * On a normal run-through the flags are those of the final index comparison;
 * on an early abort they are whatever the lookup left behind.
 * r4-r7 are untouched, and r1/r2 end up holding the values the last lookup
 * returned, exactly as in the original. */
Next sub_8117084(void) {
    u32 valueA = R0;
    u32 valueB = R1;
    u32 index = R2;
    u32 endIndex = index + 3;

    for (;;) {
        R0 = index;
        GUEST_CALL(getStructFrom2008450);
        if (R0 == 0)
            break; /* no struct for this index: stop, keeping the callee's flags/registers */

        u32 entry = R1; /* struct pointer returned by the lookup */
        ST16(entry + 0xC, valueA);
        ST16(entry + 0xE, valueB);
        R0 = valueB;

        index++;
        if ((s32)index >= (s32)endIndex) {
            SET_FLAGS_CMP(index, endIndex);
            break;
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_81179E4 ── */
/* sub_81179E4: render the description of the selected key item.
 * r4 = key item menu state (halfword at +2 is the item id),
 * r7 = menu context ([r7+0x24] selects which gfx block to render),
 * r10 = toolkit. The result of the key item lookup is stored into the
 * toolkit's unk_200A220 work struct. */
Next sub_81179E4(void) {
    /* original push {r4-r7,lr}: r4-r7 are read here and restored on exit */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* look up the key item (id in the halfword at menu_state+2) */
    R0 = LD16(R4 + 2);
    GUEST_CALL(CheckKeyItem);

    /* remember the lookup result in the unk_200A220 work struct */
    u32 work = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(work + 8, R0);

    /* render the item's text graphic:
     * r0 = source selector from the menu context, r1 = 0x12,
     * r2 = text buffer, r3 = VRAM destination,
     * r4/r5 = tile layout, r6 = source data, r7 = 0 */
    R0 = LD32(R7 + 0x24);
    R1 = 0x12;
    R2 = A_unk_2016200;
    R3 = 0x6013520;
    R4 = 2;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* pop {r4-r7} */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_811976C ── */
/* sub_811976C(r5 = sprite state record):
 * One step of a sprite's zoom animation. The scale byte at +0x12 counts down
 * in steps of 0x30; while it is still above 0x40 the new scale is applied to
 * the sprite. Once it drops to 0x40 or below (including wrapping below zero,
 * since the comparison is signed on the full 32-bit subtract result) the zoom
 * is finished: the scale snaps back to 0xC0, the animation counter at +0x09
 * is cleared and the sprite is marked as no longer scalable. The "scaling"
 * bit (0x02) is set in the state byte at +0x00 on every step.
 * On return the N/Z flags are whatever the called sprite routine left. */
Next sub_811976C(void) {
    u32 scale;

    /* Set the "scale in progress" flag (r1 = 2 stays live into the call). */
    R1 = 2;
    ST8(R5, LD8(R5) | R1);

    /* Step the scale byte down by 0x30; the subtract wraps in 32 bits just
     * like the original `sub r0, #0x30`, and the store keeps only the low
     * byte. The signed comparison against 0x40 uses the full value. */
    scale = LD8(R5 + 0x12) - 0x30;
    ST8(R5 + 0x12, scale);

    if ((s32)scale > 0x40) {
        /* Still shrinking: apply the intermediate scale. */
        R0 = 0;
        R1 = scale;
        R2 = 0x40;
        GUEST_CALL(sprite_setScaleParameters);
    } else {
        /* Zoom finished: reset the scale and drop the scaling state. */
        ST8(R5 + 0x12, 0xC0);
        ST8(R5 + 9, 0);
        R0 = 0;
        GUEST_CALL(sprite_makeUnscalable);
    }

    RETURN_TO_CALLER();
}

/* ── sub_811980C ── */
/* sub_811980C(r0 = halfword value for struct+0x0E, r1 = byte value for struct+0x0C,
 *             r2 = first table index, r3 = halfword value for struct+0x18):
 * For the three table entries starting at index r2, look up the matching
 * struct in the table rooted at 0x2008450 (getStructFrom2008450 returns a
 * status in r0 and the struct pointer in r1). If an entry exists, store the
 * three given values into it (halfword +0x0E, byte +0x0C, halfword +0x18).
 * r4-r7 are callee-saved in the original (push/pop), so they are left
 * untouched; r0/r1 hold the last loop iteration's results, and the flags
 * come from the final loop counter compare (always equal on exit). */
Next sub_811980C(void) {
    u32 halfwordE  = R0;   /* value stored at struct+0x0E */
    u32 byteC      = R1;   /* value stored at struct+0x0C */
    u32 first      = R2;   /* first of the three entries to fill in */
    u32 halfword18 = R3;   /* value stored at struct+0x18 */

    u32 index = first;
    u32 end   = first + 3;

    for (; (s32)index < (s32)end; index++) {
        R0 = index;
        GUEST_CALL(getStructFrom2008450);   /* -> r0 = status, r1 = struct */
        if (R0 != 0) {
            ST16(R1 + 0xE, halfwordE);
            ST8(R1 + 0xC, byteC);
            ST16(R1 + 0x18, halfword18);
            R0 = halfword18;    /* matches the original's last `ldr r0, [sp,#0xC]` */
        }
        /* r0/r1 keep the last call's values when the entry was empty */
    }

    SET_FLAGS_CMP(index, end);
    RETURN_TO_CALLER();
}

/* ── sub_8119854 ── */
/* sub_8119854(r0 = x, r1 = y, r2 = first sprite id):
 * Position the group of three sprites whose ids run from r2 to r2+2 at
 * the coordinates (x, y). Each id is looked up in the sprite table rooted
 * at 0x2008450; ids with no entry there are skipped. The lookup's r1
 * result is handed to sprite_setCoordinates in r5, and its r2/r3 results
 * are forwarded to that call unchanged. On return the flags are those of
 * the loop's final `cmp id, end` (i.e. the loop counter against its limit). */
Next sub_8119854(void) {
    u32 x = R0;
    u32 y = R1;
    u32 firstId = R2;
    u32 savedR5 = R5;   /* r5 is borrowed as an argument below, then restored */

    u32 end = firstId + 3;
    u32 id = firstId;
    while ((s32)id < (s32)end) {
        R0 = id;
        GUEST_CALL(getStructFrom2008450);
        if (R0 != 0) {              /* sprite exists: move it */
            R5 = R1;                /* lookup's second result -> r5 argument */
            R0 = x;
            R1 = y;
            /* r2/r3 still hold the lookup's results and are passed on as-is */
            GUEST_CALL(sprite_setCoordinates);
        }
        id++;
    }
    SET_FLAGS_CMP(id, end);

    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sub_811A0A4 ── */
/* sub_811A0A4: per-frame update for the battle object whose struct is in R5.
 * The type byte at [R5+4] selects one of four update routines from the jump
 * table at off_811A0BC (entries 0 and 3 share a handler). The selected
 * routine is called through the table — so it is entered with R0 = table
 * entry and R1 = shifted index, exactly as the original's "ldr/bx" leaves
 * them — and afterwards the object's sprite state is advanced by
 * sprite_update. The original wraps this in push {lr}/pop {pc}, so it
 * returns to the return address it was entered with, not to whatever LR the
 * last call left behind. */
Next sub_811A0A4(void) {
    u32 returnAddress = LR; /* push {lr} */

    u32 type = LD8(R5 + 4);
    u32 index = type << 2;

    /* The original's LSL leaves N/Z from the shifted index and C clear
     * (no bit is shifted out of a byte value). */
    SET_FLAGS_NZ(index);
    cpu.c = 0;

    /* ldr r0, [table, index]; mov lr, pc; bx r0 */
    u32 handler = LD32(A_off_811A0BC + index);
    R0 = handler;
    R1 = index;
    LR = 0x0811A0B2; /* return point: the sprite_update call below */
    dispatch(handler);

    /* bl sprite_update */
    GUEST_CALL(sprite_update);

    LR = returnAddress; /* pop {pc} */
    RETURN_TO_CALLER();
}

/* ── sub_811A160 ── */
/* sub_811A160(r5 = sprite/effect object):
 * Shrink the object's scale value (field 0x12) by 0x20 and mark it (field 0,
 * bit 1) as being scaled. If the scale is still above 0x40, hand the new scale
 * to sprite_setScaleParameters (base 0, new scale, target 0x40). Once it drops
 * to 0x40 or below, snap the scale back to 0xC0, clear field 9, and make the
 * sprite unscalable — the shrink animation has finished its cycle.
 * Leaves r4-r7 untouched; r0-r3 and the flags are whatever the called sprite
 * routine leaves behind. */
Next sub_811A160(void) {
    u32 scale;

    ST8(R5, LD8(R5) | 2);

    scale = LD8(R5 + 0x12) - 0x20;
    ST8(R5 + 0x12, scale);

    if ((s32)scale > 0x40) {
        R0 = 0;
        R1 = scale;
        R2 = 0x40;
        GUEST_CALL(sprite_setScaleParameters);
    } else {
        ST8(R5 + 0x12, 0xC0);
        ST8(R5 + 9, 0);
        R0 = 0;
        GUEST_CALL(sprite_makeUnscalable);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80EBD3C ── */
/* sub_80EBD3C(r5 = battle object, r7 = AI attack vars):
 * Per-tick update for an AI-controlled attack. On the first tick (state byte
 * at [r7,#1] still 0) the object detaches its linked object, clears AI field
 * 0x68, plays the wind-up animation (7), and arms a short timer (3) before
 * switching to the "attacking" state (4). Every tick then counts the timer
 * down in r0; once it goes negative the object leaves the attack state.
 *
 * Flags out: from the timer subtraction, or from object_exitAttackState when
 * that call is made. r0 ends as timer-1 (or whatever the exit call leaves). */
Next sub_80EBD3C(void) {
    if (LD8(R7 + 1) == 0) {         /* oAIAttackVars_Unk_01: state byte */
        /* First tick: tear down the previous attack's leftovers and start
         * the wind-up animation. */
        ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);

        u32 ai = LD32(R5 + oBattleObject_AIDataPtr);
        ST32(ai + oAIData_Unk_68, 0);

        R0 = 7;                 /* animation id */
        R1 = ai;                /* AI data the animation applies to */
        GUEST_CALL(object_setAnimation);

        ST16(R7 + oAIAttackVars_Unk_10, 3);   /* attack timer */
        ST8(R7 + 1, 4);                       /* state: attack in progress */
    }

    /* Count the attack timer down in r0, as the original does; on underflow
     * the attack is over. */
    R0 = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(R0, 1);       /* flags of the original's SUBS r0, #1 */
    R0 -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, R0);

    if ((s32)R0 < 0) {          /* timer was 0: leave the attack state */
        GUEST_CALL(object_exitAttackState);
    }

    RETURN_TO_CALLER();
}
