/*
 * asm29.c: readable functions of bn6f asm29.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── ho_80A4984 ── */
/* ho_80A4984: overworld map object dispatcher.
 *
 * R5 points to an OverworldMapObject (struct layout: +4 = ObjectID,
 * +8 = handler selector byte). This routine resolves the object's data
 * block inside OverworldMapObjects (base + ObjectID * entry size), leaves
 * it in R7 for the handler, and calls the handler selected by the
 * selector byte. Because the original returns straight through to our
 * caller (mov lr,pc / bx r0 followed only by pop {pc}), this is effectively
 * a tail call: the handler's registers and flags are handed directly to
 * whoever called us.
 */
Next ho_80A4984(void) {
    u32 entrySize = oMapObjectData_Size;

    /* R7 <- &OverworldMapObjects[ObjectID]; the original's ADDS also sets
     * flags, but the handler overwrites them, so nothing to preserve here. */
    u32 objectId = LD8(R5 + 4);           /* oOverworldMapObject.ObjectID */
    R2 = entrySize;
    R7 = A_OverworldMapObjects + objectId * entrySize;

    /* Pick the handler. The table is indexed with the raw selector byte
     * (valid values 0/4/8, i.e. already scaled to .word strides); the
     * loaded entry (Thumb address | 1) is what the handler sees in R0. */
    u32 selector = LD8(R5 + 8);           /* oOverworldMapObject.Unk_08 */
    R1 = selector;
    R0 = LD32(A_jt_80A49A0 + selector);

    switch (selector) {
    case 0x0: GUEST_CALL(sub_80A49B0);   break;
    case 0x4: GUEST_CALL(event_80A4A08); break;
    case 0x8: GUEST_CALL(sub_80A4A90);   break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectEnterState4LatchTilePos_80A4AB8 ── */
/* battle_objectEnterState4LatchTilePos_80A4AB8 (r5 = battle object):
 * Put the object into state 4 and latch its current tile coordinates
 * (bytes at +5/+6) into its position fields (+0x24/+0x28),
 * then run the follow-up update in sub_80A4ACC. */
Next battle_objectEnterState4LatchTilePos_80A4AB8(void) {
    ST8(R5 + 8, 4);                      /* state = 4 */

    /* copy tile coords into the position fields */
    R0 = LD8(R5 + 5);
    ST32(R5 + 0x24, R0);
    R0 = LD8(R5 + 6);
    ST32(R5 + 0x28, R0);

    /* r0 still holds the y coord at the call site, as in the original */
    GUEST_CALL(sub_80A4ACC);

    RETURN_TO_CALLER();
}

/* ── menu_stateDispatch_80A4BDC ── */
/* menu_stateDispatch_80A4BDC: tail-call dispatcher for the menu object in r5.
 *
 * The phase byte at [r5+8] is stored pre-multiplied by 4 (0, 4 or 8), so it
 * serves directly as the byte offset into the handler table at off_80A4BF0.
 * The selected handler is called with R0 = its own table entry (address with
 * thumb bit) and R1 = the raw phase byte; because the original tail-calls it
 * (mov lr, pc / bx r0), the handler's results pass straight through to this
 * function's caller. */
Next menu_stateDispatch_80A4BDC(void) {
    u32 sel = LD8(R5 + 8);

    R1 = sel;
    R0 = LD32(A_off_80A4BF0 + sel); /* handler address from the dispatch table */

    switch (sel) {
    case 0: GUEST_CALL(sub_80A4BFC); break;
    case 4: GUEST_CALL(sprite_tickWithFlagReset_80A4C46); break;
    case 8: GUEST_CALL(sub_80A4C5C); break;
    /* other byte values would index past the table and never occur */
    }

    RETURN_TO_CALLER();
}

/* ── sprite_tickWithFlagReset_80A4C46 ── */
/* sprite_tickWithFlagReset_80A4C46(r4 = unused context, r5 = sprite object):
 * Sprite animation tick: fetch the current frame's parameters, and if the
 * frame carries the 0x80 flag, reset the object's byte at +8 to 8 (probably
 * a sub-action/animation counter), then advance the sprite animation.
 * Flags and r0-r3 on return are whatever sprite_update leaves. */
Next sprite_tickWithFlagReset_80A4C46(void) {
    GUEST_CALL(sprite_getFrameParameters);

    if (R0 & 0x80)
        ST8(R5 + 8, 8);

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── npc_stateDispatchStep_80A51C4 ── */
/* npc_stateDispatchStep_80A51C4(r5 = state struct):
 * Points r7 at a 0x10-byte record in the table at byte_80A4C64, selected by
 * the byte at r5+4, then calls one of five handlers selected by the byte at
 * r5+8, which indexes the table at off_80A51E0 bytewise (entries are 4 bytes
 * apart, so the selector holds 0x00/0x04/0x08/0x0C/0x10):
 *   +0x00 sub_80A51F8   +0x04 sub_80A5268   +0x08 sub_80A52E4
 *   +0x0C sub_80A5304   +0x10 sub_80A5350
 * The original branches through r0, so the handler is entered with
 * r0 = handler address, r1 = raw selector byte, r7 = selected record. */
Next npc_stateDispatchStep_80A51C4(void) {
    u32 recordBase = A_byte_80A4C64;
    u32 recordOff  = 0x10 * (u32)LD8(R5 + 4);

    R7 = recordBase + recordOff;
    SET_FLAGS_ADD(recordBase, recordOff);

    u32 sel    = (u32)LD8(R5 + 8);
    u32 target = LD32(A_off_80A51E0 + sel);

    R0 = target;
    R1 = sel;

    switch (sel) {
    case 0x00: GUEST_CALL(sub_80A51F8); break;
    case 0x04: GUEST_CALL(sub_80A5268); break;
    case 0x08: GUEST_CALL(sub_80A52E4); break;
    case 0x0C: GUEST_CALL(sub_80A5304); break;
    case 0x10: GUEST_CALL(sub_80A5350); break;
    /* Selector outside the table: branch to whatever the load yielded. */
    default:   dispatch(target);        break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_dispatchObjectBehaviorStep_80A9B70 ── */
/* battle_dispatchObjectBehaviorStep_80A9B70: dispatch one battle-object behaviour step.
 *
 * r5 points to the object: r5+4 is a character id selecting a 0x10-byte
 * entry in byte_80A9AA0 (left in r7 for the handler), r5+8 is a byte
 * offset (0, 4 or 8) into the handler table at off_80A9B8C. The selected
 * handler pointer is loaded into r0 (the original does ldr r0,[r0,r1] /
 * mov lr,pc / bx r0, so the callee starts with r0 = its own entry point)
 * and invoked; all other registers pass through unchanged. Control
 * returns straight to this function's caller when the handler finishes,
 * with r0..r7 and the flags as the handler left them. */
Next battle_dispatchObjectBehaviorStep_80A9B70(void) {
    u32 handlerOffset = LD8(R5 + 8);

    R7 = A_byte_80A9AA0 + 0x10 * LD8(R5 + 4);
    SET_FLAGS_ADD(A_byte_80A9AA0, 0x10 * LD8(R5 + 4));

    R0 = LD32(A_off_80A9B8C + handlerOffset);

    switch (handlerOffset) {
    case 0: GUEST_CALL(sub_80A9B9C); break;
    case 4: GUEST_CALL(sub_80A9C26); break;
    case 8: GUEST_CALL(sub_80A9CA6); break;
    }

    RETURN_TO_CALLER();
}

/* ── palette_copyTableRow_80AA104 ── */
/* palette_copyTableRow_80AA104(r0 = row selector, 0..2):
 * Copies one 8-byte entry (a palette row: three colors plus terminator)
 * from the table byte_80AA11C into the buffer byte_20018B0, then
 * returns 0 with the flags set by "mov r0, #0" (N/Z clear, C/V unchanged).
 * r4-r7 are untouched by this routine and by CopyBytes. */
Next palette_copyTableRow_80AA104(void) {
    /* Each entry in byte_80AA11C is 8 bytes: R0 selects which one. */
    R0 = A_byte_80AA11C + (R0 << 3);
    R1 = A_byte_20018B0;
    R2 = 8;
    GUEST_CALL(CopyBytes); /* CopyBytes(src = R0, dest = R1, byteCount = R2) */

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_initSpriteState3_80AA2F8 ── */
/* object_initSpriteState3_80AA2F8: sprite setup for an object whose struct pointer is in r5.
 * Stores the object's initial state fields (+0 = 3, +4 = 0, +8 = 4), loads
 * its sprite (gfx 0x80, palette 0x1C, sprite 0x98), resets the animation,
 * and finishes with the follow-up init in sub_80AA322. */
Next object_initSpriteState3_80AA2F8(void) {
    ST8(R5, 3);                       /* object state = 3 */

    R0 = 0x80;                        /* sprite_load(gfx, palette, sprite) */
    R1 = 0x1C;
    R2 = 0x98;
    GUEST_CALL(sprite_load);

    ST8(R5 + 4, 0);                   /* animation counter = 0 */
    R0 = 0;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    ST8(R5 + 8, 4);                   /* next state = 4 */
    R0 = 4;
    GUEST_CALL(sub_80AA322);

    RETURN_TO_CALLER();
}

/* ── sub_80AA4C0 ── */
/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u32 call_GetCurPETNavi(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u32 call_GetCurPETNaviStatsByte(u32 navi, u32 stat) {
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return R0;
}

/* register form: calls GetCurPETNaviStatsHword outside this file, which has no typed C interface yet */
static u32 call_GetCurPETNaviStatsHword(u32 navi, u32 stat) {
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsHword);
    return R0;
}

/* register form: calls GetRNG outside this file, which has no typed C interface yet */
static u32 call_GetRNG(void) {
    GUEST_CALL(GetRNG);
    return R0;
}

/* register form: calls GetPositiveSignedRNG outside this file, which has no typed C interface yet */
static u32 call_GetPositiveSignedRNG(void) {
    GUEST_CALL(GetPositiveSignedRNG);
    return R0;
}

/* register form: calls sub_80AA5F4 outside this file, which has no typed C interface yet */
static u32 call_sub_80AA5F4(u32 param) {
    R2 = param;
    GUEST_CALL(sub_80AA5F4);
    return R0;
}

/* register form: calls GetBattleSettingsUnk01FromBattleSettings outside this file, which has no typed C interface yet */
static u32 call_GetBattleSettingsUnk01(u32 settings_ptr) {
    R0 = settings_ptr;
    GUEST_CALL(GetBattleSettingsUnk01FromBattleSettings);
    return R0;
}

/* register form: calls BIOS unsigned divide (svc 6) outside this file */
static u32 call_svc_div(u32 dividend, u32 divisor) {
    R0 = dividend;
    R1 = divisor;
    swi_call(6);
    return R0;
}

/* Attempt to pick a random overworld encounter battle.
 * Returns a BattleSettings pointer in r0, or NULL. */
static void try_encounter(u32 toolkit, u32 r1_in, u32 r2_in,
                          u32 *out_r0, u32 *out_r1, u32 *out_r2, u32 *out_r3) {
    /* Load game-state and encounter-tracker pointers from toolkit */
    u32 gs = LD32(toolkit + oToolkit_GameStatePtr);
    u32 s20 = LD32(toolkit + oToolkit_S2001c04_Ptr);

    /* Gate: must be on an overworld map (map group >= 0x80) */
    u32 rg3 = LD8(gs + oGameState_MapGroup);
    if (rg3 < 0x80) {
        *out_r0 = 0; *out_r1 = r1_in; *out_r2 = r2_in; *out_r3 = rg3;
        return;
    }
    rg3 -= 0x80; /* area index */

    /* Encounter-credit gate: (Unk_12 - Unk_14) must be >= 0x40 */
    u32 rg1 = LD16(s20 + oS2001c04_Unk_12);
    u32 rg2 = LD16(s20 + oS2001c04_Unk_14);
    u32 credit = rg1 - rg2;
    if ((s32)credit < (s32)0x40) {
        *out_r0 = 0; *out_r1 = credit; *out_r2 = rg2; *out_r3 = rg3;
        return;
    }

    /* Update encounter counter: Unk_14 = original Unk_12 */
    u32 orig_unk12 = LD16(s20 + oS2001c04_Unk_12);
    ST16(s20 + oS2001c04_Unk_14, orig_unk12);

    /* Look up encounter type from per-area table (16 entries per area) */
    u32 map_num = LD8(gs + oGameState_MapNumber);
    u32 tbl_idx = (rg3 << 4) + map_num;
    rg2 = LD8(A_byte_8020CE4 + tbl_idx);
    rg3 = tbl_idx;
    if (rg2 == 7) {
        *out_r0 = 0; *out_r1 = orig_unk12; *out_r2 = 7; *out_r3 = rg3;
        return;
    }

    /* PET Navi stat 0x28 nonzero overrides encounter type to 0 */
    u32 type = rg2;
    u32 navi = call_GetCurPETNavi();
    if (call_GetCurPETNaviStatsByte(navi, 0x28) != 0)
        type = 0;

    /* Difficulty row from encounter counter (Unk_12 >> 6), capped at 0x10 */
    rg3 = LD16(s20 + oS2001c04_Unk_12) >> 6;
    if ((s32)rg3 >= (s32)0x11)
        rg3 = 0x10;

    /* RNG threshold check against byte table byte_8020C5C[row*8 + type] */
    u8 threshold = LD8(A_byte_8020C5C + (rg3 << 3) + type);
    u32 rng_val = call_GetRNG();
    u32 rg1_ = 0x1F;
    rg2 = rng_val & 0x1F;
    rg3 = threshold;
    if (rg2 >= rg3) {
        *out_r0 = 0; *out_r1 = rg1_; *out_r2 = rg2; *out_r3 = rg3;
        return;
    }

    /* Preset-battle shortcut when Unk_28 flag is set */
    if (LD32(s20 + oS2001c04_Unk_28) != 0) {
        u32 rng2 = call_GetPositiveSignedRNG();
        u32 shifted = rng2 >> 1;
        if ((rng2 & 1) == 0) {
            u32 preset = LD32(s20 + oS2001c04_OptCurBattleDataPtr);
            if (preset != 0 && (preset >> 24) == 8) {
                ST32(gs + oGameState_CurBattleDataPtr, preset);
                rg1 = preset;
                shifted = preset >> 24;
                if (shifted == 8) {
                    *out_r0 = preset; *out_r1 = shifted; *out_r2 = rg2; *out_r3 = rg3;
                    return;
                }
            }
        }
        /* Fall through to normal encounter path */
    }

    /* Obtain battle settings via sub_80AA5F4 (r10/r12 may be clobbered by this call) */
    u32 st = A_dword_2000B30;
    u32 settings;
    bool have = false;

    /* Special: first encounter at this position (counter byte == 0) gets difficulty 0x40 */
    if (LD16(st) == 0 && LD16(st + 2) < 8) {
        settings = call_sub_80AA5F4(0x40);
        if (settings != 0) {
            ST16(st, 1);
            have = true;
        }
    }

    if (!have) {
        /* 1/12 chance (remainder == 6) to request medium difficulty 0x20 */
        u32 rng3 = call_GetPositiveSignedRNG();
        u32 quot = call_svc_div(rng3, 0xC);
        if (quot == 6) {
            settings = call_sub_80AA5F4(0x20);
            if (settings != 0)
                have = true;
        }
    }

    if (!have) {
        /* Fallback: use PET Navi stat 0x27 as difficulty, or 0x1f */
        u32 navi2 = call_GetCurPETNavi();
        settings = call_GetCurPETNaviStatsByte(navi2, 0x27);
        settings = call_sub_80AA5F4(settings);
        if (settings == 0)
            settings = call_sub_80AA5F4(0x1F);
    }

    /* Store selected battle data pointer in game state */
    ST32(gs + oGameState_CurBattleDataPtr, settings);

    /* If Unk_24 is zero, PET Navi stat 0x1E must be nonzero */
    if (LD32(s20 + oS2001c04_Unk_24) == 0) {
        u32 navi3 = call_GetCurPETNavi();
        u32 stat1e = call_GetCurPETNaviStatsByte(navi3, 0x1E);
        if (stat1e == 0) {
            *out_r0 = settings; *out_r1 = stat1e; *out_r2 = rg2; *out_r3 = rg3;
            return;
        }
    }

    /* Power check: (hword_0x3E + 4) / 5 must exceed BattleSettings->unk01 */
    u32 unk01 = call_GetBattleSettingsUnk01(settings);
    u32 navi4 = call_GetCurPETNavi();
    u32 pw = call_GetCurPETNaviStatsHword(navi4, 0x3E);
    u32 pw_quot = call_svc_div(pw + 4, 5);

    if ((s32)pw_quot <= (s32)unk01) {
        /* Underpowered: battle proceeds */
        *out_r0 = settings; *out_r1 = pw_quot; *out_r2 = rg2; *out_r3 = rg3;
        return;
    }

    /* Overpowered: reset encounter counters, no battle */
    ST16(s20 + oS2001c04_Unk_12, 0);
    ST16(s20 + oS2001c04_Unk_14, 0);
    *out_r0 = 0; *out_r1 = pw_quot; *out_r2 = rg2; *out_r3 = rg3;
}

/* sub_80AA4C0(toolkit_ptr=r10) -> r0 BattleSettings ptr, nz flag */
Next sub_80AA4C0(void) {
    u32 r0, r1, r2, r3;
    try_encounter(R10, R1, R2, &r0, &r1, &r2, &r3);
    R0 = r0; R1 = r1; R2 = r2; R3 = r3;
    SET_FLAGS_NZ(r0);
    RETURN_TO_CALLER();
}

/* ── alwaysTrue_80AA6E8 ── */
/* alwaysTrue_80AA6E8:
 * Trivial helper that always reports "true". Sets R0 = 1 and flags NZ for
 * result 1 (N clear, Z clear), then returns to the caller. */
Next alwaysTrue_80AA6E8(void) {
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── isEventFlag212Set_80AA764 ── */
/* isEventFlag212Set_80AA764: returns 1 in r0 if event flag EVENT_212 is set, else 0.
 * Flags (N/Z) reflect the returned value, as the original's final
 * `mov r0, r4` does. */
Next isEventFlag212Set_80AA764(void) {
    u32 result = 0;

    /* TestEventFlagFromImmediate(flag): flag id 0x212 passed as
     * group in r0, number in r1. The answer is the Z flag (r0 is the
     * flag's whole byte). */
    R0 = 2;
    R1 = 0x12;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (cpu.nz != 0)
        result = 1;

    R0 = result;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80AA8E0 ── */
/* sub_80AA8E0(r0-r5 = setup parameters, r10 = toolkit pointer):
 * Forwards a six-word parameter block to sub_80AA910 (which reads its
 * arguments back from the caller's stack at [sp, #n]), then returns
 *   r0 = value returned by sub_80AA910,
 *   r1 = the current battle state's oBattleState_Unk_36 field.
 * If r1 is 0 on entry, the helper is not called and (0, 0) is returned. */
Next sub_80AA8E0(void) {
    u32 setupResult = 0;
    u32 battleFlag = 0;

    if (R1 != 0) {
        u32 savedR7 = R7;

        /* Build the argument block on the stack for sub_80AA910.
         * The two trailing words are zeroed, as the original does. */
        u32 args = SP -= 0x2C;
        ST32(args + 0x00, R0);
        ST32(args + 0x04, R1);
        ST32(args + 0x08, R2);
        ST32(args + 0x0C, R3);
        ST32(args + 0x10, R4);
        ST32(args + 0x14, R5);
        ST32(args + 0x1C, 0);
        ST32(args + 0x20, 0);

        /* sub_80AA910 expects r7 = 0 (r0-r6, r8-r12 keep their incoming
         * values; it preserves the callee-saved ones itself). */
        R7 = 0;
        GUEST_CALL(sub_80AA910);
        SP += 0x2C;
        setupResult = R0;

        /* Read the current battle state's field 0x36 for the second result. */
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        battleFlag = LD16(battleState + oBattleState_Unk_36);

        /* r7 is outgoing-preserved, but the callee saw it as 0. */
        R7 = savedR7;
    }

    R0 = setupResult;
    R1 = battleFlag;
    RETURN_TO_CALLER();
}

/* ── util_nandByteArray40_80AAB88 ── */
/* util_nandByteArray40_80AAB88(r0 = srcA, r1 = srcB, r2 = dst):
 * NAND two 40-byte arrays together: dst[i] = ~(srcA[i] | srcB[i]).
 * Leaves the loop counter (0x28) and the last srcB byte read in r3/r1,
 * with the final CMP's flags (c set, v clear) for callers to branch on. */
Next util_nandByteArray40_80AAB88(void) {
    u32 srcA = R0;      /* r6 in the original */
    u32 srcB = R1;      /* r7 in the original */
    u32 dst = R2;       /* r5 in the original */
    u32 i;              /* r3 in the original */
    u32 lastB = 0;      /* last value loaded into r1 */

    for (i = 0; i < 0x28; i++) {
        u32 a = LD8(srcA + i);
        u32 b = LD8(srcB + i);
        lastB = b;
        ST8(dst + i, ~(a | b));
    }

    R1 = lastB;
    R3 = i;             /* ends at 0x28 */
    SET_FLAGS_CMP(i, 0x28);
    RETURN_TO_CALLER();
}
