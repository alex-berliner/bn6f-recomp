/*
 * asm00_2_8012BA2.c: readable functions of bn6f asm00_2.s, 0x08012BA2-0x0801728E, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_8012C34 ── */
/* sub_8012C34(r0 = value, r1 = flag bits):
 * OR in flag bits selected by r1 into r0. Bit 2 of r1 adds 0x4000,
 * bit 4 adds 0x2000. Returns r0 with the bits applied; r3 is left
 * at 4 as a side effect of the original's scratch register use. */
Next sub_8012C34(void) {
    u32 flags = R1;

    if (flags & 0x2)
        R0 |= 0x4000;
    if (flags & 0x4)
        R0 |= 0x2000;

    R3 = 4;
    RETURN_TO_CALLER();
}

/* ── battle_orChipCrossNaviFlag_8012C4A ── */
/* battle_orChipCrossNaviFlag_8012C4A(r0 = chip slot index, r1 = flags word):
 * Looks up the chip in the given slot and, if it is a chip of the special
 * kind (flags byte has bit 1 set and bit 0 clear, byte 6 == 0x0A) whose
 * battle-navi state byte 0x2C reads 4 or 0x10, ORs the 0x1000 bit into the
 * caller's flags word. Returns the updated flags word in r0.
 * r2 is left holding 2 when the first flag test fails and 1 otherwise,
 * exactly as the original's scratch register ends up; r4/r5/r6 untouched. */
Next battle_orChipCrossNaviFlag_8012C4A(void) {
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

/* ── battle_getNaviTypeFamilyConstant_8012C7C ── */
/* battle_getNaviTypeFamilyConstant_8012C7C(r0 = battle object):
 * Read the battle object's navi type byte (alliance stats, offset 0x2C) and map it to
 * a constant:
 *   type 0x02 or 0x0E -> 0x4000 if the object pointer is non-null, else 0
 *   type 0x03 or 0x0F -> 0x0028 if the object pointer is non-null, else 0
 *   anything else     -> 0
 * r4 is saved/restored by the original, so it is left untouched for the caller. */
Next battle_getNaviTypeFamilyConstant_8012C7C(void) {
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

/* ── battle_updateNaviAIJoypad_8012DFC ── */
/* Register offsets in the per-navi joypad table (stride 8, based at dword_2036820). */
#define NAVI_JOYPAD_STRIDE  8
#define NAVI_JOYPAD_OFFSET  2

/* register form: calls battle_getPlayerAIData_8010022 outside this file, which has no typed C interface yet */
static u32 get_player_ai_data(u32 *callee_r2) {
    GUEST_CALL(battle_getPlayerAIData_8010022);
    *callee_r2 = R2;
    return R0;
}

/* register form: calls battle_isBattleOver outside this file, which has no typed C interface yet.
 * After the GUEST_CALL the caller does tst r0,r0 which clears C,V and sets N,Z from the result. */
static bool battle_is_over(u32 *callee_r3) {
    GUEST_CALL(battle_isBattleOver);
    *callee_r3 = R3;
    SET_FLAGS_NZ(R0);   /* reproduces tst r0, r0: N,Z from value, C=V=0 */
    return R0 != 0;
}

/* register form: calls GetBattleNaviStatsByte outside this file, which has no typed C interface yet.
 * The caller's next flag-setting instruction is cmp r0,#0x18 (SUBS stats,0x18). */
static u32 get_navi_stats_byte(u32 navi, u32 field, u32 *callee_r1) {
    R0 = navi;
    R1 = field;
    GUEST_CALL(GetBattleNaviStatsByte);
    *callee_r1 = R1;
    SET_FLAGS_CMP(R0, 0x18);  /* stats,0x18: live for the early-exit path */
    return R0;
}

/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet.
 * The callee's flags (NZ) are read by the caller's bne; C,V persist to the return. */
static bool time_stop_active(u32 *callee_r1) {
    GUEST_CALL(battle_isTimeStop);
    *callee_r1 = R1;
    return !ZERO_FLAG();   /* Z clear ⇒ time stop active */
}

typedef struct {
    u32 r1;
    u32 r2;
    u32 r3;
    u32 r7;
} JoypadResult;

/* Advance one AI navi's virtual joypad for a frame.
 * Updates held / released / pressed / up fields from the navi's button state
 * stored in the per-navi table at dword_2036820.  A secondary mirror of the
 * joypad fields (Unk_2a..Unk_30) is zeroed when time-stop is inactive and
 * updated identically when it is active. */
static JoypadResult update_navi_ai_joypad(u32 navi) {
    u32 r2_get;
    u32 ai_data = get_player_ai_data(&r2_get);

    u32 r3_over;
    if (battle_is_over(&r3_over)) {
        ST16(ai_data + oAIData_JoypadHeld,     0);
        ST16(ai_data + oAIData_JoypadReleased, 0);
        ST16(ai_data + oAIData_JoypadPressed,  0);
        ST16(ai_data + oAIData_JoypadUp,       0);
        return (JoypadResult){ 0, r2_get, r3_over, navi };
    }

    u32 r1_stats;
    u32 stats = get_navi_stats_byte(navi, 0x2C, &r1_stats);
    if (stats >= 0x17 && stats <= 0x18)
        return (JoypadResult){ r1_stats, r2_get, r3_over, navi };

    /* Read the navi's current button state from the per-navi table */
    u32 navi_buttons = LD16(A_dword_2036820 + NAVI_JOYPAD_STRIDE * navi + NAVI_JOYPAD_OFFSET);

    /* Main joypad: shift held→released, install new held, derive pressed/up */
    u32 prev_held = LD16(ai_data + oAIData_JoypadHeld);
    ST16(ai_data + oAIData_JoypadReleased, prev_held);
    ST16(ai_data + oAIData_JoypadHeld,     navi_buttons);
    ST16(ai_data + oAIData_JoypadPressed,  (~prev_held) & navi_buttons);
    ST16(ai_data + oAIData_JoypadUp,       (~navi_buttons) & prev_held);

    u32 r1_ts;
    if (!time_stop_active(&r1_ts)) {
        /* Time stop inactive: clear the secondary joypad mirror */
        ST16(ai_data + oAIData_Unk_30, 0);
        ST16(ai_data + oAIData_Unk_2a, 0);
        ST16(ai_data + oAIData_Unk_2c, 0);
        ST16(ai_data + oAIData_Unk_2e, 0);
        return (JoypadResult){ r1_ts, prev_held, navi_buttons, navi };
    }

    /* Time stop active: update secondary joypad mirror with the same edge logic */
    u32 old_unk = LD16(ai_data + oAIData_Unk_2a);
    ST16(ai_data + oAIData_Unk_30, old_unk);
    ST16(ai_data + oAIData_Unk_2a, navi_buttons);
    ST16(ai_data + oAIData_Unk_2c, (~old_unk) & navi_buttons);
    ST16(ai_data + oAIData_Unk_2e, (~navi_buttons) & old_unk);
    return (JoypadResult){ r1_ts, old_unk, navi_buttons, navi };
}

/* battle_updateNaviAIJoypad_8012DFC(r0 = navi_index) -> r1, r2, r3, r7, c, v
 * Flags are produced by the last register-form helper on each path
 * (tst-equivalent, cmp-equivalent, or the callee's own flags). */
Next battle_updateNaviAIJoypad_8012DFC(void) {
    JoypadResult r = update_navi_ai_joypad(R0);
    R1 = r.r1;
    R2 = r.r2;
    R3 = r.r3;
    R7 = r.r7;
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

/* ── DeterminePowerAttackChargeTime ── */
/*
 * DeterminePowerAttackChargeTime (0x08012F62)
 *
 * Works out how many frames the navi's charged power attack takes, based on
 * the battle object's AI data. Entry r0 selects the AIData charge field:
 * JOYPAD_B picks oAIData_BPwrAtk directly, anything else picks
 * oAIData_APwrAtk and additionally consults the navi's transformation -- a
 * Beast Over transformation whose current chip is elementless switches to
 * oAIData_BeastOutAPwrAtk instead. r5 holds the battle object whose AIData
 * pointer is read.
 *
 * The original also leaves the charge-table index in r1 (or 0xFF on the
 * "no data" early return) and the powerAttackChargeTimes base address in r2
 * on the paths that load from it; both are part of the register contract, so
 * they are reproduced exactly -- see the register-form helpers below.
 */

/* register form: calls GetBattleNaviStatsByte_AllianceFromBattleObject outside this file, which has no typed C interface yet */
static u32 get_navi_stats_byte_DeterminePowerAttackChargeTime(u32 battleObject, u32 statsOffset) {
    R0 = battleObject;
    R1 = statsOffset;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    return R0;
}

/* register form: calls getCurChipInBattleHand_8010004 and getChip8021DA8 outside this file, which have no typed C interface yet.
 * The original branches on the Z flag getCurChipInBattleHand_8010004 leaves
 * behind (the disassembly notes that branch can never succeed, since the
 * hand lookup does not set the control codes it seems to test) and feeds its
 * r0 result straight into getChip8021DA8; both behaviours are kept here. */
static u32 current_chip_data(u32 transformation) {
    R0 = transformation;
    GUEST_CALL(getCurChipInBattleHand_8010004);
    if (!ZERO_FLAG())
        GUEST_CALL(getChip8021DA8);   /* r0 still holds the hand chip id */
    return R0;
}

/* register form: the contract lists r2 as an output. On the paths that read
 * the table the original leaves the powerAttackChargeTimes base address in
 * guest r2, while on the 0xFF early return r2 must keep whatever the last
 * callee left behind -- a value that only exists in the guest register file,
 * so the table address has to be placed in r2 exactly where the original's
 * ldr sits, and only on the paths that reach it. */
static u32 load_charge_time(u32 tableIndex) {
    R2 = A_powerAttackChargeTimes_8020404;
    return LD16(R2 + tableIndex);
}

typedef struct {
    u32 chargeTime; /* -> r0: charge frames, or 0xFF when there is no entry */
    u32 r1Value;    /* -> r1: charge-table index, or 0xFF on the early return */
} PowerAttackChargeResult;

static PowerAttackChargeResult determine_power_attack_charge_time(u32 selector, u32 battleObject) {
    PowerAttackChargeResult result;
    u32 aiData = LD32(battleObject + oBattleObject_AIDataPtr);
    u32 pwrAtkOffset;
    u32 currentR0 = selector; /* tracks the original's r0 through the lookup chain */

    if (currentR0 == JOYPAD_B) {
        pwrAtkOffset = 7; /* oAIData_BPwrAtk */
    } else {
        pwrAtkOffset = 5; /* oAIData_APwrAtk */

        /* Beast Out transformations may switch to the Beast-Out field,
         * provided the chip currently in the battle hand is elementless. */
        u32 transformation = get_navi_stats_byte_DeterminePowerAttackChargeTime(currentR0, oNaviStats_Transformation);
        currentR0 = transformation;
        if ((s32)transformation >= (s32)TF_GREGARBEAST
                && (s32)transformation <= (s32)TF_FALZARBEASTOVER) {
            u32 chipPtr = current_chip_data(transformation);
            u32 chipElement = LD8(chipPtr + 6 /* oChipData_ChipElement */);
            currentR0 = chipElement;
            if (chipElement == CHIP_ELEM_NONE)
                pwrAtkOffset = oAIData_BeastOutAPwrAtk;
        }
    }

    /* Charge level (navi stats byte 3) picks the row of the charge table. */
    u32 chargeLevelTimes2 = get_navi_stats_byte_DeterminePowerAttackChargeTime(currentR0, 3) * 2;

    u32 pwrAtkStat = LD8(aiData + pwrAtkOffset);
    if (pwrAtkStat == 0xFF) {
        /* No power-attack data: report the maximum charge time. r2 keeps
         * whatever the last callee left behind, as in the original. */
        result.chargeTime = 0xFF;
        result.r1Value = 0xFF;
    } else {
        result.r1Value = pwrAtkStat * 10 + chargeLevelTimes2;
        result.chargeTime = load_charge_time(result.r1Value);
    }
    return result;
}

Next DeterminePowerAttackChargeTime(void) {
    PowerAttackChargeResult out = determine_power_attack_charge_time(R0, R5);
    R0 = out.chargeTime;
    R1 = out.r1Value;
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

/* ── aiAttackingBuster_8013396 ── */
/* register form: calls returnBoolIfAIDataFlagsAreSet_8012F3E outside this file,
 * which has no typed C interface yet. Z set => AI data flags are set. */
static bool ai_flags_set(u32 battle_object) {
    R0 = battle_object;
    GUEST_CALL(returnBoolIfAIDataFlagsAreSet_8012F3E);
    return !ZERO_FLAG();
}

/* True when the battle object's AI is using its buster BP/atk:
 * the BPwrAtk slot is not 0xFF (no override) AND the AI buster flags are set.
 * Returns 1 on a buster attack, 0 otherwise. */
static u32 ai_attacking_buster(u32 battle_object) {
    u32 ai_data = LD32(battle_object + oBattleObject_AIDataPtr);
    if (LD8(ai_data + oAIData_BPwrAtk) == 0xFF)
        return 0;
    return ai_flags_set(battle_object) ? 1 : 0;
}

/* aiAttackingBuster_8013396: r5 = battle object, lr = return -> nz c v r0 */
Next aiAttackingBuster_8013396(void) {
    R0 = ai_attacking_buster(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_initSlotsOrderPhaseB_80133EC ── */
/* battle_initSlotsOrderPhaseB_80133EC: initialize the seven battle "slot" entries in one of two orders.
 * The pointer table at off_801340C selects the order list; entry 4 points to
 * byte_801341B = { 0, 6, 7, 8, 9, A, B }. For each of the 7 entries it calls
 * init_8013B4E(index, order[index]).
 *
 * r5 and r6 are only scratch here (the original saves/restores them), and
 * r0/r1/LR are clobbered by the calls; everything else passes through. */
Next battle_initSlotsOrderPhaseB_80133EC(void) {
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

/* ── battle_InitNaviStatsByIndex_8013422 ── */
/* battle_InitNaviStatsByIndex_8013422(r0 = navi index):
 * Initialize the stats struct for one BattleNavi: the navi records are 0x64
 * bytes each and start at eBattleNaviStats0, so compute the struct's address
 * and fill it with the default stats.
 * Returns r2 = eBattleNaviStats0 (base of the stats array). */
Next battle_InitNaviStatsByIndex_8013422(void) {
    u32 statsBase = A_eBattleNaviStats0;

    R2 = statsBase;
    R0 = statsBase + R0 * 100; /* &eBattleNaviStats0[index * 0x64] */
    GUEST_CALL(initNaviStats_WithDefaultStatsMaybe_8013438);

    RETURN_TO_CALLER();
}

/* ── initNaviStats_WithDefaultStatsMaybe_8013438 ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word(u32 mem, u32 size) {
    R0 = mem;
    R1 = size;
    GUEST_CALL(ZeroFillByWord);
}

/* Initialize a 0x64-byte Navi battle-stats block with default values. */
static void init_navi_stats(u32 stats) {
    zero_fill_by_word(stats, 0x64);

    /* Core battle / alliance flags */
    ST8(stats + 0x05, 1);       /* active */
    ST8(stats + 0x07, 0xFF);    /* sentinel: no alliance override */
    ST8(stats + 0x08, 0);
    ST8(stats + 0x09, 4);       /* element type */
    ST8(stats + 0x0A, 5);       /* attack pattern */
    ST8(stats + 0x0B, 5);       /* defense pattern */
    ST8(stats + 0x0C, 1);
    ST8(stats + 0x0D, 0);
    ST8(stats + 0x0E, 0x99);    /* default attack power */
    ST8(stats + 0x12, 0xFF);    /* unassigned support flag */
    ST8(stats + 0x18, 0);
    ST8(stats + 0x19, 0);

    /* Custom chip slot area */
    ST8(stats + 0x20, 1);       /* slot enabled */
    ST8(stats + 0x21, 3);       /* slot count */
    ST8(stats + 0x26, 0);       /* extra chip count */
    ST8(stats + 0x27, 0x1F);    /* HP cap per chip */
    ST8(stats + 0x28, 0);
    ST8(stats + 0x2C, 0);
    ST8(stats + 0x2D, 0);

    /* Ranged-element slots (0x2E–0x30): 0xFF = none */
    for (u32 off = 0x2E; off <= 0x30; off++)
        ST8(stats + off, 0xFF);

    /* HP (halfword): max = current = remaining = 100 */
    ST16(stats + 0x3E, 0x64);
    ST16(stats + 0x40, 0x64);
    ST16(stats + 0x42, 0x64);

    /* Support-chip IDs (0x56–0x5B): 0xFF = none */
    for (u32 off = 0x56; off <= 0x5B; off++)
        ST8(stats + off, 0xFF);

    /* Re-set slot count after loop (original writes 0x21 last) */
    ST8(stats + 0x21, 3);
}

/* initNaviStats_WithDefaultStatsMaybe_8013438(r0 = Navi stats block address) */
Next initNaviStats_WithDefaultStatsMaybe_8013438(void) {
    init_navi_stats(R0);
    RETURN_TO_CALLER();
}

/* ── GetBattleNaviStatsAddr ── */
/* GetBattleNaviStatsAddr(r0 = navi index):
 * Returns the address of the given navi's entry in the battle navi stats
 * table: eBattleNaviStats0 + index * sizeof(NaviStats). r1 is left holding
 * the table base address, and the flags reflect the final address addition. */
Next GetBattleNaviStatsAddr(void) {
    u32 offset = R0 * oNaviStats_Size;

    R1 = A_eBattleNaviStats0;
    SET_FLAGS_ADD(offset, A_eBattleNaviStats0);
    R0 = offset + A_eBattleNaviStats0;

    RETURN_TO_CALLER();
}

/* ── SetBattleNaviStatsByte ── */
/* SetBattleNaviStatsByte(r0 = alliance flag, r1 = NaviStats field offset,
 *                        r2 = byte value):
 * Writes one byte into the NaviStats record of the requested alliance,
 * e.g. updating a stat after a netbattle. r6/r7 are untouched (the
 * original pushes/pops them around the call). */
Next SetBattleNaviStatsByte(void) {
    u32 offset = R1;
    u32 value = R2;

    /* R0 already holds the alliance flag; returns the NaviStats pointer. */
    GUEST_CALL(GetBattleNaviStatsAddr);

    ST8(R0 + offset, value);
    RETURN_TO_CALLER();
}

/* ── GetBattleNaviStatsByte ── */
/* GetBattleNaviStatsByte(r0 = alliance side flag, r1 = NaviStats byte offset):
 * Fetch the NaviStats record for the requested side and return the single
 * byte at the caller-chosen offset. r6 is preserved; r1 comes back unchanged
 * (GetBattleNaviStatsAddr does not touch it). */
Next GetBattleNaviStatsByte(void) {
    u32 offset = R1;

    GUEST_CALL(GetBattleNaviStatsAddr); /* (alliance: bool) -> * NaviStats */

    R0 = LD8(R0 + offset);
    RETURN_TO_CALLER();
}

/* ── GetBattleNaviStatsHword ── */
/* GetBattleNaviStatsHword(r0 = alliance, r1 = stat offset, r6 = preserved):
 * fetch a halfword from the current battle navi's NaviStats; r0 = the value.
 * The offset in r1 selects which stat (see NaviStats field layout). */
Next GetBattleNaviStatsHword(void) {
    u32 statOffset = R1;

    /* GetBattleNaviStatsAddr(alliance) -> pointer to the NaviStats struct */
    GUEST_CALL(GetBattleNaviStatsAddr);

    R0 = LD16(R0 + statOffset);
    RETURN_TO_CALLER();
}

/* ── SetNaviStatsByte ── */
/* SetNaviStatsByte(r0 = record index (normalized to 0/1), r1 = field offset,
 * r2 = value, r10 = toolkit):
 * Write r2 into Toolkit->NaviStats[index].field[r1].
 * Leaves r0 = byte offset of the record (index * oNaviStats_Size) and
 * r3 = address of the written byte; c/v from the address addition. */
Next SetNaviStatsByte(void) {
    /* normalize the record index to a clean 0/1 */
    if (R0 != 0)
        R0 = 1;

    /* NaviStats is an array of oNaviStats_Size-byte records */
    R0 = R0 * oNaviStats_Size;
    R3 = LD32(R10 + oToolkit_NaviStatsPtr);

    R3 = R3 + R0;
    SET_FLAGS_ADD(R3, R0);

    ST8(R3 + R1, R2);
    RETURN_TO_CALLER();
}

/* ── GetNaviStatsByte ── */
/* GetNaviStatsByte(r0 = record selector (booleanized: any nonzero value
 *                   selects the same record), r1 = field offset within the
 *                   navi stats record, r10 = toolkit):
 * reads one byte from the selected navi stats record; r0 = byte read,
 * r3 = address of that byte. */
Next GetNaviStatsByte(void) {
    u32 record = (R0 != 0) ? 1 : 0;      /* cmp r0, #0 / movne r0, #1 */
    u32 stats = LD32(R10 + oToolkit_NaviStatsPtr) + record * oNaviStats_Size;
    R3 = stats;
    R0 = LD8(stats + R1);
    RETURN_TO_CALLER();
}

/* ── SetBattleNaviStatsByte_AllianceFromBattleObject ── */
/* SetBattleNaviStatsByte_AllianceFromBattleObject(r1 = offset into NaviStats,
 * r2 = byte value, r5 = battle object):
 * Fetches the NaviStats record for the battle object's navi -- choosing
 * between the real one and its ghost copy via the object's alliance flag --
 * and stores one byte at the given offset in that record. */
Next SetBattleNaviStatsByte_AllianceFromBattleObject(void) {
    u32 statsOffset = R1;
    u32 value = R2;

    /* alliance flag (0 = player's own navi, nonzero = linked/ghost side)
     * selects which NaviStats record to edit */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(GetBattleNaviStatsAddr); /* r0 = &NaviStats */

    ST8(R0 + statsOffset, value);
    RETURN_TO_CALLER();
}

/* ── SetCurPETNaviStatsByte ── */
/* SetCurPETNaviStatsByte(r0 = stats selector, r1 = field offset, r2 = byte value):
 * Writes one byte into a NaviStats record. r0 is resolved by
 * GetNaviStatsIndexGivenCurPETNavi; the result selects the record
 * (recordOffset = result * oNaviStats_Size is added to the toolkit's
 * NaviStats base pointer), and value is stored at record + fieldOffset.
 * r1 and r2 pass through unchanged; flags reflect the pointer add. */
Next SetCurPETNaviStatsByte(void) {
    u32 fieldOffset = R1; /* byte offset within the NaviStats record */
    u32 value = R2;       /* byte to store */

    /* r0 already holds the caller's selector for the helper. */
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);

    u32 recordOffset = R0 * oNaviStats_Size;
    u32 statsBase = LD32(R10 + oToolkit_NaviStatsPtr);
    u32 record = statsBase + recordOffset;
    SET_FLAGS_ADD(statsBase, recordOffset);

    ST8(record + fieldOffset, value);

    R0 = recordOffset;
    R1 = fieldOffset;
    R2 = value;
    R3 = record;
    RETURN_TO_CALLER();
}

/* ── GetCurPETNaviStatsByte ── */
/* GetCurPETNaviStatsByte(r0 = field offset selector for the PET's current navi):
 * Looks up which navi is currently selected in the PET, indexes the NaviStats
 * array with it, and reads one byte out of that navi's stats struct.
 *   r1 (in/out): byte offset within the NaviStats struct; preserved.
 *   r0 (out): the requested stats byte.
 *   r3 (out): address of the byte that was read.
 *   c, v (out): flags from the address computation (ADDS). */
Next GetCurPETNaviStatsByte(void) {
    u32 fieldOffset = R1;

    /* Ask which navi is active on the PET; returns its index in the stats array. */
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    u32 naviIndex = R0;
    R1 = fieldOffset; /* the helper clobbers r1; the original keeps it intact */

    /* Walk to the current navi's entry in the stats array. */
    u32 statsBase = LD32(R10 + oToolkit_NaviStatsPtr);
    u32 entryOffset = naviIndex * oNaviStats_Size;
    R3 = statsBase + entryOffset;
    SET_FLAGS_ADD(statsBase, entryOffset);

    R0 = LD8(R3 + fieldOffset);
    RETURN_TO_CALLER();
}

/* ── SetCurPETNaviStatsHword ── */
/* SetCurPETNaviStatsHword(r0 = navi id, r1 = field offset in oNaviStats, r2 = value,
 * r10 = toolkit):
 * Writes a halfword into the stats struct of the navi currently selected on the
 * PET. The stats array lives at toolkit->NaviStatsPtr; the entry for the current
 * PET navi is picked via GetNaviStatsIndexGivenCurPETNavi.
 * Leaves r0 = offset of that navi's stats entry, r3 = address written to,
 * flags set by the pointer computation (r1, r2 preserved). */
Next SetCurPETNaviStatsHword(void) {
    u32 fieldOffset = R1;
    u32 value = R2;

    /* Which slot in the stats array does the current PET navi occupy?
     * The callee clobbers r1/r2, so the caller saves them around the call. */
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    R1 = fieldOffset;
    R2 = value;

    u32 entryOffset = R0 * oNaviStats_Size;

    u32 statsBase = LD32(R10 + oToolkit_NaviStatsPtr);
    u32 entry = statsBase + entryOffset;

    ST16(entry + fieldOffset, value);

    R0 = entryOffset;
    R3 = entry;
    SET_FLAGS_ADD(statsBase, entryOffset);
    RETURN_TO_CALLER();
}

/* ── GetCurPETNaviStatsHword ── */
/* GetCurPETNaviStatsHword(r0 = which_navi, r1 = which_stat) -> r0:
 * Read one u16 stat of the navi currently selected on the PET.
 * Looks up the navi's stats-slot index, then reads the halfword at
 * NaviStatsPtr + index*oNaviStats_Size + which_stat.
 * Also leaves r3 = the address that was read from, and flags from the
 * final address addition (ADDS). */
Next GetCurPETNaviStatsHword(void) {
    u32 which_stat = R1; /* saved across the helper call, as the original does */

    /* r0 = stats index of the navi currently equipped in the PET */
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    R1 = which_stat;

    u32 stats_base = LD32(R10 + oToolkit_NaviStatsPtr);
    u32 slot_offset = R0 * oNaviStats_Size;
    u32 stat_addr = stats_base + slot_offset;
    SET_FLAGS_ADD(stats_base, slot_offset);

    R0 = LD16(stat_addr + which_stat);
    R3 = stat_addr;
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

/* ── battle_naviAI_tick_8013DA0 ── */
/* register form: calls battle_isPaused outside this file, which has no typed C interface yet */
static bool is_battle_paused(void) {
    GUEST_CALL(battle_isPaused);
    return !ZERO_FLAG();
}

/* register form: calls GetBattleNaviStatsByte_AllianceFromBattleObject outside this file, which has no typed C interface yet */
static u32 get_navi_stat(u32 battle_obj, u32 field) {
    R0 = battle_obj;
    R1 = field;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    return R0;
}

/* register form: calls sub_80143A6 outside this file, which has no typed C interface yet */
static void ai_action_a(u32 r0_val, u32 r1_val) {
    R0 = r0_val;
    R1 = r1_val;
    GUEST_CALL(sub_80143A6);
}

/* register form: calls sub_8014446 outside this file, which has no typed C interface yet */
static void ai_action_b(void) {
    GUEST_CALL(sub_8014446);
}

/* register form: calls GetPositiveSignedRNG outside this file, which has no typed C interface yet */
static u32 get_positive_rng(void) {
    GUEST_CALL(GetPositiveSignedRNG);
    return R0;
}

/* register form: SWI 6 computes value % modulus, result left in R1 */
static u32 swi6_mod(u32 value, u32 modulus) {
    R0 = value;
    R1 = modulus;
    swi_call(6);
    return R1;
}

/* register form: calls battle_setPlayerNaviStatsField0E_8015BEC outside this file, which has no typed C interface yet */
static void set_player_navi_field0e(u32 alliance, u32 value) {
    R0 = alliance;
    R1 = value;
    GUEST_CALL(battle_setPlayerNaviStatsField0E_8015BEC);
}

/* register form: calls sub_80143CE outside this file, which has no typed C interface yet */
static void ai_action_c(void) {
    GUEST_CALL(sub_80143CE);
}

/* register form: calls sub_801443C outside this file, which has no typed C interface yet */
static void ai_action_d(void) {
    GUEST_CALL(sub_801443C);
}

/* AI tick for a battle navi.  Builds a 16-slot probability table from a
 * weight array, picks a new decision state via RNG, and dispatches. */
static void ai_navi_update(u32 battle_obj) {
    if (is_battle_paused())
        return;

    /* Require navis to have three stats present before acting */
    if (!get_navi_stat(battle_obj, 0x24))
        return;
    if (!get_navi_stat(battle_obj, 0x21))
        return;

    u32 stat_2c = get_navi_stat(battle_obj, 0x2C);
    if (stat_2c != 0) {
        /* All stats present and 0x2C non-zero: fire immediate action pair */
        ai_action_a(stat_2c, 0x2C);
        ai_action_b();
        return;
    }

    /* Timer-based decision: increment tick, wait for 60 frames */
    u32 ai_data = LD32(battle_obj + oBattleObject_AIDataPtr);
    u32 timer = (u16)LD16(ai_data + oAIData_Unk_3a) + 1;
    ST16(ai_data + oAIData_Unk_3a, timer);

    if ((s32)timer < (s32)0x3C)
        return;

    /* Timer expired – reset and run action pair before deciding */
    ST16(ai_data + oAIData_Unk_3a, 0);
    ai_action_a(0, 0x2C);
    ai_action_b();

    u32 current_state = LD8(ai_data + oAIData_Unk_0b);

    /* Build 16-entry mark table:
     *  - weight[i] == current_state && state==0 → mark 1 (stay)
     *  - weight[i] == current_state && state!=0 → mark 0 (stay)
     *  - weight[i] != current_state            → mark = weight (transition) */
    u8 mark[16];
    for (int i = 0; i < 16; i++) {
        u32 weight = LD8(A_byte_8013E44 + i);
        if (weight != current_state) {
            mark[i] = weight;
        } else {
            mark[i] = (current_state == 0) ? 1 : 0;
        }
    }

    /* Random selection via RNG modulo table size */
    u32 rng_val = get_positive_rng();
    u32 index = swi6_mod(rng_val, 16);
    u32 new_state = mark[index];
    ST8(ai_data + oAIData_Unk_0b, new_state);

    /* Dispatch based on chosen state */
    if (new_state == 2) {
        ai_action_c();
    } else if (new_state == 1) {
        ai_action_d();
    } else {
        /* state 0 or 3+: set player navi field 0E from lookup table */
        u32 field_val = LD8(A_dword_8013E54 + new_state);
        u32 alliance = LD8(battle_obj + oBattleObject_Alliance);
        set_player_navi_field0e(alliance, field_val);
    }
}

/* battle_naviAI_tick_8013DA0(r5 = battle object) – AI tick */
Next battle_naviAI_tick_8013DA0(void) {
    ai_navi_update(R5);
    RETURN_TO_CALLER();
}

/* ── nullsub_45 ── */
/* nullsub_45: does nothing; presumably a stub for a removed feature call. */
Next nullsub_45(void) {
    RETURN_TO_CALLER();
}

/* ── handlePanelDamage_8013F1E ── */
/* Jump table at off_8013F70, indexed by the result of
 * GetBattleNaviStatsByte_AllianceFromBattleObject(r5, 0x16). */
enum {
    DAMAGE_HANDLER_NULL    = 0,  /* nullsub_46  */
    DAMAGE_HANDLER_F82     = 1,  /* sub_8013F82  */
    DAMAGE_HANDLER_F8C     = 2,  /* sub_8013F8C  */
    DAMAGE_HANDLER_F96     = 3,  /* sub_8013F96  */
};

/* register form: calls object_getFlag2 outside this file, which has no typed C interface yet */
static u32 object_getFlag2_g(u32 obj) {
    R0 = obj;
    GUEST_CALL(object_getFlag2);
    return R0;
}

/* register form: calls GetBattleNaviStatsByte_AllianceFromBattleObject outside this file,
 * which has no typed C interface yet */
static u32 get_navi_stats_byte_alliance_g(u32 obj, u32 index) {
    R0 = obj;
    R1 = index;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    return R0;
}

/* register form: calls sub_8013F82 outside this file, which has no typed C interface yet */
static void call_sub_8013F82(void)  { GUEST_CALL(sub_8013F82); }
/* register form: calls sub_8013F8C outside this file, which has no typed C interface yet */
static void call_sub_8013F8C(void)  { GUEST_CALL(sub_8013F8C); }
/* register form: calls sub_8013F96 outside this file, which has no typed C interface yet */
static void call_sub_8013F96(void)  { GUEST_CALL(sub_8013F96); }

/* Run the per-team damage handler selected by the alliance byte of the attacker. */
static void dispatch_damage_handler(u32 handler_index) {
    switch (handler_index) {
        case DAMAGE_HANDLER_NULL: break;
        case DAMAGE_HANDLER_F82:  call_sub_8013F82(); break;
        case DAMAGE_HANDLER_F8C:  call_sub_8013F8C(); break;
        case DAMAGE_HANDLER_F96:  call_sub_8013F96(); break;
        default: break;
    }
}

/* If this navi dealt panel damage this hit, run its per-team damage handler
 * (and stamp the AI "did damage" latch). Called once per successful hit. */
static void handle_panel_damage(u32 obj) {
    /* Bit 0x104 in object_getFlag2 marks "this object can be damaged". */
    if (!(object_getFlag2_g(obj) & 0x104))
        return;

    u32 collision = LD32(obj + oBattleObject_CollisionDataPtr);
    if (LD32(collision + oCollisionData_FlagsFromCollision) == 0)
        return;

    /* Sum the three panel-damage slots (each holds two targets' damage). */
    u32 dmg_base = collision + oCollisionData_PanelDamage1;
    s32 total = (s32)LD32(dmg_base + 0)
              + (s32)LD32(dmg_base + 4)
              + (s32)LD32(dmg_base + 8);
    if (total == 0)
        return;

    u32 ai = LD32(obj + oBattleObject_AIDataPtr);
    u8 prevent_anim = LD8(obj + oBattleObject_PreventAnim);

    /* Update the AI's "we hit something" latch:
     *   normal path  -> reset to 0
     *   anim-locked  -> set to 1, unless it's already 1 (then bail) */
    if (prevent_anim == 0) {
        ST8(ai + oAIData_Unk_1c, 0);
    } else {
        if (LD8(ai + oAIData_Unk_1c) != 0)
            return;
        ST8(ai + oAIData_Unk_1c, 1);
    }

    dispatch_damage_handler(
        get_navi_stats_byte_alliance_g(obj, 0x16));
}

Next handlePanelDamage_8013F1E(void) {
    handle_panel_damage(R5);
    RETURN_TO_CALLER();
}

/* ── nullsub_46 ── */
/* nullsub_46: 08013F80 THUMB.
 * Empty stub — the original is just `mov pc, lr`, so it has no effect.
 * Kept because callers still branch here (e.g. an optional hook point). */
Next nullsub_46(void) {
    RETURN_TO_CALLER();
}

/* ── battle_RollNaviStatChance_8013FAE ── */
/* battle_RollNaviStatChance_8013FAE: random success check based on a battle object's navi stat byte
 * (stat id 0x11, via GetBattleNaviStatsByte_AllianceFromBattleObject).
 * Rolls RNG & 7 and returns 1 if the roll is <= (stat - 1), else 0; if the
 * stat is 0 it always fails.
 * Flag outputs: on the stat == 0 exit they come from the flag-setting
 * `mov r4, r0` (NZ only, C/V untouched); otherwise from the final CMP. */
Next battle_RollNaviStatChance_8013FAE(void) {
    R1 = 0x11;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    u32 stat = R0;

    if (stat == 0) {
        /* Fail immediately; R0 already holds the stat (0) from the callee. */
        SET_FLAGS_NZ(stat);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(GetRNG);
    u32 roll = R0 & 7;
    u32 limit = stat - 1;

    SET_FLAGS_CMP(roll, limit);
    R1 = limit;
    R0 = ((s32)roll > (s32)limit) ? 0u : 1u;
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

/* ── battle_applyNaviStatDamageTick_8013FF8 ── */
/* battle_applyNaviStatDamageTick_8013FF8 (r5 = battle object): damage tick driven by the object's navi
 * stats. Reads stat byte 0x3D and deals that much damage to the object's HP,
 * but never reduces it below 1 (a stat of 0 means no damage at all).
 * Leaves c/v as the original does: from the callee on the early-out path,
 * from "cmp r1,#1" when returning at 1 HP, and from object_subtractHP
 * otherwise. r4/r6 are preserved; r1/r2 are outputs as in the original. */
Next battle_applyNaviStatDamageTick_8013FF8(void) {
    R1 = 0x3D;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    if (R0 != 0) {
        /* Damage cap: stat value - 1, so a weaker object keeps 1 HP. */
        R2 = R0 - 1;
        R1 = LD16(R5 + oBattleObject_HP);

        /* Already at 1 HP: leave it alone. */
        if (R1 == 1) {
            SET_FLAGS_CMP(R1, 1);
            RETURN_TO_CALLER();
        }

        /* If HP exceeds the cap the full stat value hits; otherwise only
         * HP-1 is dealt. R0 already holds the stat value. */
        if ((s32)R1 <= (s32)R2)
            R0 = R1 - 1;

        GUEST_CALL(object_subtractHP);
    }
    RETURN_TO_CALLER();
}

/* ── GetNaviStatsIndexGivenCurPETNavi ── */
/* GetNaviStatsIndexGivenCurPETNavi(r0 = current PET navi ID):
 * Look up which Navi stats slot the currently selected PET navi uses.
 * Megaman maps to index 0, all other navis map to index 1. */
Next GetNaviStatsIndexGivenCurPETNavi(void) {
    R0 = LD8(A_CurPETNaviToNaviStatsIndexTable + R0);
    RETURN_TO_CALLER();
}

/* ── GetNaviStatsAddrGivenCurPETNavi ── */
/* GetNaviStatsAddrGivenCurPETNavi(r0 = current PET navi id):
 * returns &Toolkit->naviStatsArray[100 * GetNaviStatsIndexGivenCurPETNavi(navi)],
 * i.e. the NaviStats struct of the navi currently set in the PET. */
Next GetNaviStatsAddrGivenCurPETNavi(void) {
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    u32 stats = LD32(R10 + oToolkit_NaviStatsPtr);
    R0 = R0 * 100 + stats;
    RETURN_TO_CALLER();
}

/* ── battle_playerHPRegenTick_8014040 ── */
/* battle_playerHPRegenTick_8014040 (r0 = alliance flag):
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
Next battle_playerHPRegenTick_8014040(void) {
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

/* ── battle_applyNaviStatTableEntry_8014178 ── */
/* battle_applyNaviStatTableEntry_8014178: read two stat bytes of the current navi via
 * GetBattleNaviStatsByte(kind, 8), combine them as index = stat1*3 + stat0,
 * look up byte_80141A0[index] (three entries per navi), and hand that byte to
 * sub_801DF8C. r4/r6 are preserved for the caller. */
Next battle_applyNaviStatTableEntry_8014178(void) {
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

/* ── SetBeastOutCounterTo3 ── */
/* SetBeastOutCounterTo3: set the current PET navi's BeastOut counter to 3
 * (re-arms the Beast Out limit, e.g. after a transformation ends). */
Next SetBeastOutCounterTo3(void) {
    GUEST_CALL(GetCurPETNavi); /* -> current navi id in r0 */

    R1 = oNaviStats_BeastOutCounter;
    R2 = 3;
    GUEST_CALL(SetCurPETNaviStatsByte); /* SetCurPETNaviStatsByte(r0, offset, 3) */

    RETURN_TO_CALLER();
}

/* ── battle_EnableAIUnk0x48FlagForNonstandardBoard_80141F4 ── */

/* battle_EnableAIUnk0x48FlagForNonstandardBoard_80141F4:
 * Decide whether the AI's 0x48 flag needs to be raised. It is skipped when
 * the board uses one of the standard column patterns (default 0x38,
 * disadvantaged 0x30, advantaged 0x3C) or when the battle mode is the
 * Dustman mini game; otherwise the 0x400 bit is passed to
 * SetAIDataUnk0x48Flag. r1/r3 pass through unchanged from the callees. */
Next battle_EnableAIUnk0x48FlagForNonstandardBoard_80141F4(void) {
    GUEST_CALL(GetBattlePanelColumnPattern);
    u32 pattern = R0;

    if (pattern == 0x38 /* default pattern */
     || pattern == 0x30 /* disadvantaged */
     || pattern == 0x3C /* advantaged */) {
        RETURN_TO_CALLER();
    }

    GUEST_CALL(GetBattleMode);
    if (R0 == BATTLE_MODE_DUSTMAN_MINI_GAME)
        RETURN_TO_CALLER();

    R0 = 0x400;
    GUEST_CALL(SetAIDataUnk0x48Flag);
    RETURN_TO_CALLER();
}

/* ── battle_applyStatusTickDamage_80142B0 ── */
/* battle_applyStatusTickDamage_80142B0(r5 = battle object, r10 = ...):
 * If battle effects indicate "confusion"-style damage over time, set the
 * object's pending damage to 10 (fixed damage of 1000 with the usual
 * decimal scaling). */
Next battle_applyStatusTickDamage_80142B0(void) {
    u32 effects = R0;
    GUEST_CALL(GetBattleEffects);

    if (R0 & 8) {
        ST16(R5 + oBattleObject_Damage, 0xA);
    }

    RETURN_TO_CALLER();
}

/* ── battle_applySelfDamageFromBattleEffect_80142C2 ── */
/* battle_applySelfDamageFromBattleEffect_80142C2: called with r5 = battle object. If the current battle effects
 * include the 0x8 flag, write a self-damage of 10 into the object's collision
 * data. Leaves r0 = battle effects (or 10), r1 = 8 (or the collision data
 * pointer), and nz reflecting the last executed instruction. */
Next battle_applySelfDamageFromBattleEffect_80142C2(void) {
    u32 self = R5;

    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    R1 = 8;
    if (effects & 8) {
        R0 = 0xA;
        SET_FLAGS_NZ(R0);
        u32 collision = LD32(self + oBattleObject_CollisionDataPtr);
        R1 = collision;
        ST16(collision + oCollisionData_SelfDamage, 0xA);
    } else {
        SET_FLAGS_NZ(effects & 8);
    }

    RETURN_TO_CALLER();
}

/* ── battle_updateAngerTint_80143E4 ── */
/* battle_updateAngerTint_80143E4(r5 = battle object):
 * If the object's AI data records any anger, tint its sprites with color
 * shader 0xF (the "enraged" red flash). Flags pass through to the caller. */
Next battle_updateAngerTint_80143E4(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 anger = LD16(aiData + oAIData_Anger);

    /* tst r0, r0 — flags are observable by callers even when returning early */
    SET_FLAGS_NZ(anger);
    if (anger != 0) {
        R0 = 0xF;
        GUEST_CALL(sprite_setColorShader);
    }
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
    GUEST_CALL(battle_isTimeStop); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG())
        RETURN_TO_CALLER();

    GUEST_CALL(battle_isPaused); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG())
        RETURN_TO_CALLER();

    GUEST_CALL(object_getFlag);

    R1 = 0xC00;                 /* bits: AI update requested / AI exists */
    if ((R0 & R1) != 0)
        GUEST_CALL(sub_8014432);
    else
        GUEST_CALL(battle_clearAIDataField4c_8014424);

    RETURN_TO_CALLER();
}

/* ── battle_clearAIDataField4c_8014424 ── */
/* battle_clearAIDataField4c_8014424: clear the AI state field (oAIData_Unk_4c) of the battle
 * object's AI data. Returns 0 in r1. */
Next battle_clearAIDataField4c_8014424(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    R1 = 0;
    ST32(aiData + oAIData_Unk_4c, 0);

    RETURN_TO_CALLER();
}

/* ── nullsub_6 ── */
/* nullsub_6: empty stub; does nothing and returns immediately. */
Next nullsub_6(void) {
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

/* ── battle_beginTurnOpenProcess_80144C0 ── */
/* battle_beginTurnOpenProcess_80144C0 — begin the turn-open processing for the battle object whose
 * pointer the caller left in r5.
 *
 * sub_80144CA is a second entry point that shares the whole body below; it
 * enters with r7 = 1 where this entry clears r7 to 0 first.  On the shared
 * tail that flag only decides whether battleObject_initAIData_800FEEC runs, so it is kept in the
 * guest r7 register exactly as the original does. */
Next battle_beginTurnOpenProcess_80144C0(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR7 = R7;

    /* shared-tail flag: this entry takes the full path */
    R7 = 0;
    GUEST_CALL(sub_801390C);

    /* wipe this object's chip hand: 0xC halfwords just past the hand header */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(getBattleHandAddr_8010018);
    R0 += 0x26;
    R1 = 0xC;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = 0x20;
    GUEST_CALL(ClearAIDataUnk0x48Flag);

    GUEST_CALL(GetBattleEffects);
    if ((R0 & 8) != 0) {
        R0 = LD8(R5 + oBattleObject_Alliance);
        GUEST_CALL(battle_networkInvert);
        if (R0 == 0) {
            /* rerun the processing against the opposing player object */
            R0 = LD8(R5 + oBattleObject_Alliance) ^ 1;
            R1 = 1;
            GUEST_CALL(battle_findPlayer);
            if (R0 != 0) {
                R5 = R0;
                GUEST_CALL(releaseSlotByValue_801DC36);
                R5 = savedR5;
            }
        }
    }

    GUEST_CALL(battle_IsNoCustomPlusActive_80107C0);
    R0 = R3;
    GUEST_CALL(battle_SetObjectHitModifier_8019F86);
    R0 = 1;
    GUEST_CALL(object_setCollisionRegion);
    GUEST_CALL(sub_8012EA8);

    /* the sub_80144CA entry (r7 = 1) skips this call */
    if (R7 == 0)
        GUEST_CALL(battleObject_initAIData_800FEEC);

    GUEST_CALL(sub_8014536);
    GUEST_CALL(sub_801086C);
    GUEST_CALL(battle_applySelfDamageFromBattleEffect_80142C2);

    /* restore the callee-saved registers the original pops back */
    R4 = savedR4;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── nullsub_51 ── */
/* nullsub_51: does nothing; likely a stub replacing a removed routine. */
Next nullsub_51(void) {
    RETURN_TO_CALLER();
}

/* ── timer_resetAndStart_801482C ── */
/* timer_resetAndStart_801482C:
 * Reset the counter at dword_20367F0 to zero and set the flag byte at
 * byte_20367F4 to 1, marking the tracked state as active/fresh.
 * Returns the struct address in r1. */
Next timer_resetAndStart_801482C(void) {
    u32 state = A_dword_20367F0;

    ST32(state, 0);
    ST8(state + 4, 1);

    R1 = state;
    RETURN_TO_CALLER();
}

/* ── sub_8015940 ── */
/* sub_8015940: reset the byte_203CED0 record — the marker byte and the
 * dword_203CED4 flag are both set to 0xFF — then ask battle_preUpdateAndGetPlayerObject_80103EC for a
 * value and store it at byte_203CED8. The c/v flags seen by the caller
 * are whatever battle_preUpdateAndGetPlayerObject_80103EC left behind; r4-r6 and r10 pass through. */
Next sub_8015940(void) {
    u32 record = A_byte_203CED0;

    ST8(record, 0xFF);
    ST8(record + 4, 0xFF);      /* dword_203CED4 - byte_203CED0 */

    /* The original keeps the record pointer in r5 across the call. */
    u32 saved_r5 = R5;
    R5 = record;
    GUEST_CALL(battle_preUpdateAndGetPlayerObject_80103EC);
    R5 = saved_r5;

    ST32(record + 8, R0);       /* byte_203CED8 - byte_203CED0 */
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

/* ── battle_TickAIDataTimer0f_8015A16 ── */
/* battle_TickAIDataTimer0f_8015A16(r0 = battle object):
 * Counts down the one-byte timer at oAIData_Unk_0f of the object's AI data.
 * The timer is left alone when it is disabled (0xFF), already at zero, or when
 * the object's navi stats byte 0x29 is non-zero (i.e. the feature is off).
 *
 * Exits with r1 = AI data pointer and r2 = new timer value on the countdown
 * path (r1 = 0x29 from the stats query on the early bail-outs), and with
 * r5 restored to its incoming value -- the original pushes/pops r5 around
 * the body, only using it as scratch for the object pointer and the call. */
Next battle_TickAIDataTimer0f_8015A16(void) {
    u32 battleObject = R0;
    u32 savedR5 = R5;

    if (battleObject == 0)
        RETURN_TO_CALLER();

    R5 = battleObject; /* object pointer, as the callee expects to see it */
    R1 = 0x29;         /* navi stats byte to query */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    R5 = savedR5;      /* pop {r5}: callers get their r5 back on all paths */

    if (R0 != 0)       /* stats byte set: timer not in use */
        RETURN_TO_CALLER();

    u32 aiData = LD32(battleObject + oBattleObject_AIDataPtr);
    u32 timer = LD8(aiData + oAIData_Unk_0f);
    R1 = aiData;

    if (timer != 0xFF && timer != 0) {
        timer--;
        ST8(aiData + oAIData_Unk_0f, timer);
    }
    R2 = timer;

    RETURN_TO_CALLER();
}

/* ── battle_setObjectTextId_8015B22 ── */
/* battle_setObjectTextId_8015B22(r0 = name id, r5 = battle object):
 * Store the object's NameID. Name id 0 maps to the default 0x1A0;
 * anything else is stored offset by 0x1AB (i.e. r0 - 1 + 0xAD + 0xFF). */
Next battle_setObjectTextId_8015B22(void) {
    u16 nameId;

    if (R0 == 0)
        nameId = 0x1A0;
    else
        nameId = (u16)(R0 + 0x1AB);

    ST16(R5 + oBattleObject_NameID, nameId);
    RETURN_TO_CALLER();
}

/* ── callPossiblyGetBattleEmotion_8015B54 ── */
/* callPossiblyGetBattleEmotion_8015B54: thin wrapper around
 * possiblyGetBattleEmotion_8015B64. All arguments and results pass straight
 * through in the registers (r0 = alliance flag in, r0..r3 = emotion /
 * transformation / mood / Beast-Out counter out); it exists only so callers
 * can tail-call the lookup without managing LR themselves. */
Next callPossiblyGetBattleEmotion_8015B54(void) {
    GUEST_CALL(possiblyGetBattleEmotion_8015B64);
    RETURN_TO_CALLER();
}

/* ── possiblyGetBattleEmotion_8015B64 ── */
/* register form: calls GetBattleNaviStatsAddr outside this file, which has no typed C interface yet */
static u32 get_battle_navi_stats_addr(u32 alliance) {
    R0 = alliance;
    GUEST_CALL(GetBattleNaviStatsAddr);
    return R0;
}

/* register form: calls battle_findPlayer outside this file, which has no typed C interface yet */
static u32 battle_find_player(u32 alliance) {
    R0 = alliance;
    GUEST_CALL(battle_findPlayer);
    return R0;
}

/* Everything the caller consumes: the chosen emotion plus the NaviStats fields
 * it reads alongside it. The carry-out pair lets the adapter reproduce the
 * original's C flag without branching (see read_battle_emotion). */
struct BattleEmotion {
    u32 emotion;         /* r0: 0..5 picked from AI state and mood */
    u32 transformation;  /* r1: NaviStats.Transformation */
    u32 mood;            /* r2: NaviStats.Mood */
    u32 beastOutCounter; /* r3: NaviStats.BeastOutCounter */
    u32 carryOne;        /* SET_FLAGS_CMP(carryOne, carryZero) -> the original's C */
    u32 carryZero;
};

/* Decide the player's battle emotion: Beast Out / anger / chip-related AI
 * flags override the mood-based default, and mood 0 always reports "normal".
 *
 * The C flag out mirrors which of the original's comparisons was the last one
 * executed. The only unconditional flag setter is the "adds r5, r0, #0" behind
 * "mov r5, player", which clears C; the TSTs leave C alone; each CMP sets it
 * to "no borrow". So C ends up 1 exactly when a CMP ran and the final
 * CMP mood,#0xFF didn't see mood below 0xFF. */
static struct BattleEmotion read_battle_emotion(u32 alliance) {
    u32 stats = get_battle_navi_stats_addr(alliance);
    u32 player = battle_find_player(alliance);
    u32 ai = LD32(player + oBattleObject_AIDataPtr);

    u32 transformation = LD8(stats + oNaviStats_Transformation);
    u32 mood = LD8(stats + oNaviStats_Mood);

    u32 emotion;
    u32 carry;
    u32 unk36 = LD16(ai + oAIData_Unk_36);
    if (unk36 != 0) {
        /* Branch taken right after the first TST: no CMP ever ran, so C is
         * still the 0 left by "adds r5, r0, #0". */
        emotion = 5;
        carry = 0;
    } else if (mood == 0) {
        emotion = 5;
        carry = 1;          /* last CMP: mood, #0 */
    } else if (LD16(ai + oAIData_Anger) != 0) {
        emotion = 3;
        carry = 1;          /* last CMP: mood, #0 (mood != 0); TST leaves C */
    } else if (LD16(ai + oAIData_Unk_32) != 0) {
        emotion = 1;
        carry = 1;
    } else if (mood == 0xFF) {
        emotion = 2;
        carry = 1;          /* last CMP: mood, #0xFF — equal, no borrow */
    } else {
        emotion = 0;
        carry = 0;          /* last CMP: mood, #0xFF — mood < 0xFF borrows */
    }

    struct BattleEmotion res;
    res.emotion = emotion;
    res.transformation = transformation;
    res.mood = mood;
    res.beastOutCounter = LD8(stats + oNaviStats_BeastOutCounter);
    res.carryOne = carry;
    res.carryZero = 1 - carry;   /* CMP x, 1-x yields C = x and V = 0 */
    return res;
}

/* possiblyGetBattleEmotion_8015B64(r0 = alliance) ->
 *   c: set unless the decision fell through to "emotion 0" or bailed out on
 *      AIData.Unk_36 before any CMP, v: always clear,
 *   r0 = emotion, r1 = transformation, r2 = mood, r3 = beast out counter
 *   (r4-r6 preserved) */
Next possiblyGetBattleEmotion_8015B64(void) {
    struct BattleEmotion res = read_battle_emotion(R0);
    R0 = res.emotion;
    R1 = res.transformation;
    R2 = res.mood;
    R3 = res.beastOutCounter;
    SET_FLAGS_CMP(res.carryOne, res.carryZero);
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

/* ── navi_DecreaseMood_8015C12 ── */
/* navi_DecreaseMood_8015C12(r0 = alliance flag, r1 = amount): fetch the active Navi's stats
 * and reduce its mood by `amount`, never letting it drop below 1. If the mood
 * is already 0, nothing is written. Callers read the flags left by the final
 * CMP with 1 (mood - amount compared against 1), so an underflow to the clamp
 * value still reports "less than 1". */
Next navi_DecreaseMood_8015C12(void) {
    u32 delta = R1;

    GUEST_CALL(GetBattleNaviStatsAddr); /* alliance flag in R0 -> R0 = NaviStats * */
    u32 stats = R0;

    u32 mood = LD8(stats + oNaviStats_Mood);
    SET_FLAGS_NZ(mood); /* TST r1, r1 */
    if (mood != 0) {
        u32 reduced = mood - delta;
        u32 stored = reduced;
        if ((s32)reduced < 1) /* BGE: signed comparison */
            stored = 1;
        ST8(stats + oNaviStats_Mood, stored);
        SET_FLAGS_CMP(reduced, 1);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8015C2C ── */
/* sub_8015C2C: return 0x80 (128). */
Next sub_8015C2C(void) {
    R0 = 0x80;
    RETURN_TO_CALLER();
}

/* ── ZeroAllNaviStatsMood ── */
/* ZeroAllNaviStatsMood: clear the Mood byte of every navi stats entry.
 * Calls SetNaviStatsByte(i, oNaviStats_Mood, 0) for i = 0..6.
 * r4 is preserved (pushed/popped by the original); the final flags come from
 * the loop's last comparison (cmp r4, #7 with r4 = 7), and r0-r3 hold whatever
 * the last SetNaviStatsByte call left in them. */
Next ZeroAllNaviStatsMood(void) {
    u32 navi;

    for (navi = 0; (s32)navi < 7; navi++) {
        R0 = navi;
        R1 = oNaviStats_Mood;
        R2 = 0;
        GUEST_CALL(SetNaviStatsByte);
    }

    SET_FLAGS_CMP(navi, 7);
    RETURN_TO_CALLER();
}

/* ── battle_object_runPhaseHandler_8016396 ── */
/* battle_object_runPhaseHandler_8016396 — phase dispatcher for the battle object in R5.
 *
 * Reads the object's current phase byte and tail-calls the matching
 * handler from the table at off_80163A8:
 *   phase 0 -> battleObject_joinAlly_80163B4   phase 4 -> battle_object_tickHitFlashAlpha_801641A   phase 8 -> battle_objectIntroEntryAndActedSetup_8016460
 *
 * The phase value doubles as the byte offset into the table (Thumb loads
 * cannot scale the index register), which is why the stored phase values
 * step by 4 instead of 1.
 *
 * Register note: the original loads the branch target into R1 and leaves
 * it there across the call, so each handler is entered with R1 holding
 * its own Thumb entry address (table word, e.g. battle_objectIntroEntryAndActedSetup_8016460+1) — and the
 * handlers preserve R1. That value must therefore still be set up before
 * the call, exactly as the table load in the original does. The phase
 * byte reaches the handler in R0; all other registers pass through
 * untouched. Any phase outside the table would send the original through
 * a garbage branch target, so it can never occur in practice.
 */
Next battle_object_runPhaseHandler_8016396(void) {
    R0 = LD8(R5 + oBattleObject_CurPhase);

    /* Load the selected handler's address, as the original's
     * "ldr r1, [r1, r0]" does — the callee sees this value in R1. */
    R1 = LD32(A_off_80163A8 + R0);

    switch (R0) {
    case 0: GUEST_CALL(battleObject_joinAlly_80163B4); break;
    case 4: GUEST_CALL(battle_object_tickHitFlashAlpha_801641A); break;
    case 8: GUEST_CALL(battle_objectIntroEntryAndActedSetup_8016460); break;
    }

    RETURN_TO_CALLER();
}

/* ── battleObject_joinAlly_80163B4 ── */
/* register form: calls list_tryAddUniqueEntry_800AA1A outside this file, which has no typed C interface yet */
static void list_tryAddUniqueEntry(u32 arg) {
    R0 = arg;
    GUEST_CALL(list_tryAddUniqueEntry_800AA1A);
}

/* register form: calls battle_getStateUnk5c_800139A outside this file, which has no typed C interface yet */
static u32 battle_getStateUnk5c(void) {
    GUEST_CALL(battle_getStateUnk5c_800139A);
    return R0;
}

/* register form: calls sub_800AA06 outside this file, which has no typed C interface yet */
static u32 sub_800AA06_query(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_800AA06);
    return R0;
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound(u32 id) {
    R0 = id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls sprite_setAlpha_8002c7a outside this file, which has no typed C interface yet.
 * Sprite alpha sets R1; the original returns immediately after, leaving whatever the callee put in R1. */
static u32 sprite_setAlpha_capture(u32 arg) {
    R0 = arg;
    GUEST_CALL(sprite_setAlpha_8002c7a);
    return R1;
}

/* On the "join same alliance" path the ORR sets r0 = oldFlags | 2, with r1 preloaded to 2.
 * The original's pop path leaves r0 = 8 (the value just written) and r1 = 2. */
static u8 join_same_alliance(u32 obj) {
    u8 flags = LD8(obj) | (u8)2;
    ST8(obj, flags);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 8);
    return 8;
}

/* Common fall-through path: clear bit 1 of oObjectHeader_Flags. The original sets r1 = 2
 * before the BIC, so the function exits with r1 = 2 regardless of r0. */
static u8 clear_flag_bit1(u32 obj) {
    u8 flags = LD8(obj);
    u8 cleared = flags & (u8)~2;
    ST8(obj, cleared);
    return cleared;
}

typedef struct { u32 r0; u32 r1; } R0R1;

/* Phase-initialization step for a battle object.
 *
 * - phaseInitialized == 0 (first entry):
 *     - if the object's alliance differs from the opposing side (`oBattleState_Unk_0d`),
 *       try to add a unique entry to the battle list, store 4 into phaseInitialized,
 *       and fall through to the common BIC path;
 *     - else set bit 1 of oObjectHeader_Flags, write 8 to
 *       oBattleObject_CurPhaseAndPhaseInitialized, and return.
 * - phaseInitialized != 0 (ongoing handling):
 *     - if battle-state bit 0 is set and sub_800AA06(0) is nonzero, play the join
 *       sound effect (0x94), set Timer = 2, Timer2 = 0x10, sprite alpha = 0,
 *       set CurPhase = 4, and return (r1 left as sprite_setAlpha left it).
 *     - otherwise fall through to the common BIC path.
 *
 * On the fall-through paths, r1 is 2 (set by `mov r1, #2` before the BIC). On the
 * same-alliance path, r1 is also 2 (set by `mov r1, #2` before the ORR). The
 * final returns mirror those immediate-r1 setups. */
static R0R1 sub_80163B4_typed(u32 obj, u32 toolkit) {
    u8 phaseInit = LD8(obj + oBattleObject_PhaseInitialized);
    R0R1 out;

    if (phaseInit == 0) {
        u32 battleState = LD32(toolkit + oToolkit_BattleStatePtr);
        u8 allianceObj      = LD8(obj + oBattleObject_Alliance);
        u8 opposingAlliance = LD8(battleState + oBattleState_Unk_0d);

        if (allianceObj == opposingAlliance) {
            out.r0 = join_same_alliance(obj);
            out.r1 = 2;
            return out;
        }
        list_tryAddUniqueEntry(0);
        ST8(obj + oBattleObject_PhaseInitialized, 4);
        /* fall through to common BIC path */
    } else {
        if ((battle_getStateUnk5c() & 1u) != 0 && sub_800AA06_query(0) != 0) {
            play_sound(0x94);
            ST16(obj + oBattleObject_Timer, 2);
            ST16(obj + oBattleObject_Timer2, 0x10);
            out.r1 = sprite_setAlpha_capture(0);
            ST8(obj + oBattleObject_CurPhase, 4);
            out.r0 = 4;
            return out;
        }
        /* fall through to common BIC path */
    }

    /* Common fall-through: clear bit 1 of oObjectHeader_Flags; r1 was set to 2
     * immediately before the BIC, so the function exits with r1 = 2. */
    out.r0 = clear_flag_bit1(obj);
    out.r1 = 2;
    return out;
}

Next battleObject_joinAlly_80163B4(void) {
    R0R1 result = sub_80163B4_typed(R5, R10);
    R0 = result.r0;
    R1 = result.r1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_object_tickHitFlashAlpha_801641A ── */
/* register form: calls sprite_setMosaicSize outside this file, which has no typed C interface yet */
static void sprite_setMosaicSize_guest(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(sprite_setMosaicSize);
}

/* register form: calls sprite_setAlpha_8002c7a outside this file, which has no typed C interface yet */
static void sprite_setAlpha_8002c7a_guest(u32 r0) {
    R0 = r0;
    GUEST_CALL(sprite_setAlpha_8002c7a);
}

/* register form: calls sprite_disableAlpha outside this file, which has no typed C interface yet */
static void sprite_disableAlpha_guest(void) {
    GUEST_CALL(sprite_disableAlpha);
}

/* register form: calls sprite_clearMosaic outside this file, which has no typed C interface yet */
static void sprite_clearMosaic_guest(void) {
    GUEST_CALL(sprite_clearMosaic);
}

/* register form: calls sub_800AA40 outside this file, which has no typed C interface yet.
 * Original captures this callee's final C/V (they survive to the end of
 * battle_object_tickHitFlashAlpha_801641A because MOV/STRH don't touch flags), so forward them. */
static u32 sub_800AA40_guest(u32 r0, bool *out_c, bool *out_v) {
    R0 = r0;
    GUEST_CALL(sub_800AA40);
    *out_c = CARRY_FLAG();
    *out_v = OVERFLOW_FLAG();
    return R1;
}

struct sub_801641A_out {
    u32 r0;
    u32 r1;
    bool c;
    bool v;
};

/* Per-tick update for a battle object. Decrements the frame timer; when
 * it elapses, plays the hit-flash (mosaic + alpha) for `timer2` frames
 * with the in-flash flag set, then on expiry clears the flash effects,
 * runs sub_800AA40(0), and advances the phase word to 8. */
static struct sub_801641A_out sub_801641A_impl(u32 r5, u32 in_r1) {
    struct sub_801641A_out res;
    res.r1 = in_r1;

    u16 timer    = LD16(r5 + oBattleObject_Timer);
    u16 old_time = timer;
    timer -= 1;
    ST16(r5 + oBattleObject_Timer, timer);

    if (timer != 0) {
        /* SUBS r0, #1: r0 = old_time - 1 = timer.
         * C = NOT borrow = (old_time >= 1 unsigned).
         * V = 0 (old_time came from a u16, can't hit INT_MIN). */
        res.r0 = timer;
        res.c  = (old_time >= 1);
        res.v  = false;
        return res;
    }

    /* Frame timer elapsed: reload it and tick the flash duration. */
    ST16(r5 + oBattleObject_Timer, 2);
    u16 timer2 = LD16(r5 + oBattleObject_Timer2);
    timer2 -= 1;
    ST16(r5 + oBattleObject_Timer2, timer2);

    if (timer2 == 0) {
        /* Flash done: clear visual effects, run sub_800AA40(0), then
         * store 8 into the phase word. Final flags = sub_800AA40's. */
        sprite_disableAlpha_guest();
        sprite_clearMosaic_guest();
        res.r1 = sub_800AA40_guest(0, &res.c, &res.v);
        ST16(r5 + oBattleObject_CurPhaseAndPhaseInitialized, 8);
        res.r0 = 8;
        return res;
    }

    /* Mid-flash frame: mosaic = timer2, alpha = 0x10 - timer2.
     *   sub r0, #0x10 ; neg r0, r0  =>  r0 = -(timer2 - 0x10).
     * Final ORR r0, r1 sets NZ from result and clears C, V. */
    sprite_setMosaicSize_guest(timer2, timer2);
    u32 alpha = (u32)(-(s32)((s32)timer2 - 0x10));
    sprite_setAlpha_8002c7a_guest(alpha);
    u8 flags = LD8(r5 + oObjectHeader_Flags) | 0x2;
    ST8(r5 + oObjectHeader_Flags, flags);
    res.r0 = flags;
    res.r1 = 2;
    res.c  = false;
    res.v  = false;
    return res;
}

Next battle_object_tickHitFlashAlpha_801641A(void) {
    struct sub_801641A_out res = sub_801641A_impl(R5, R1);
    R0 = res.r0;
    R1 = res.r1;
    SET_FLAGS_NZ(R0);
    /* Original's tail differs per path: SUBS (C depends on old_time) for
     * the early return, ORR (C=V=0) mid-flash, or the callee's tail on
     * phase advance. SET_FLAGS_NZ fixes N/Z from R0; C/V are forwarded
     * from the typed function because no single SET_FLAGS_* macro can
     * reproduce all three paths' final flags. */
    cpu.c = res.c;
    cpu.v = res.v;
    RETURN_TO_CALLER();
}

/* ── battle_objectIntroEntryAndActedSetup_8016460 ── */
/* battle_objectIntroEntryAndActedSetup_8016460 — turn-start setup for one battle object (r5 = object, r10 = toolkit).
 *
 * If bit 1 of the battle intro state is clear, there is nothing to do yet.
 * Otherwise the object is marked as having acted (CurAction = 1, phase
 * counter cleared). Unless this object's alliance already matches the side
 * recorded in the battle state (or the battle mode is 6), an entry effect
 * is run first via sub_801DC7C; objects whose NameID lies in 0x49..0x4E
 * get different effect arguments. */
Next battle_objectIntroEntryAndActedSetup_8016460(void) {
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

/* ── battle_objectUpdateDispatchPhase_80165C2 ── */
/* battle_objectUpdateDispatchPhase_80165C2: per-frame update for one battle object (object pointer in R5).
 *
 * - If the object's flag bit 0x40000000 is set, run sub_8016EC4 for it instead.
 * - Otherwise, if the battle is currently paused, do nothing this frame.
 * - Otherwise tail-call the object's current phase handler, looked up in the
 *   table off_80165F0 by oBattleObject_CurPhase:
 *       phase 0 -> battle_teardownObject_80165F8
 *       phase 1 -> sub_801664E
 *   The handler's results (R0, R1 and the flags) are passed straight through
 *   to our caller; the original achieves this by returning into a `pop {pc}`
 *   right after the handler returns. */
Next battle_objectUpdateDispatchPhase_80165C2(void) {
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
        GUEST_CALL(battle_teardownObject_80165F8);
    else                                   /* phase 1: the only other entry */
        GUEST_CALL(sub_801664E);

    RETURN_TO_CALLER();
}

/* ── battle_teardownObject_80165F8 ── */
/* register form: calls object_clearCollisionRegion outside this file, which has no typed C interface yet */
static void clear_collision_region(void) {
    GUEST_CALL(object_clearCollisionRegion);
}

/* register form: calls battle_ClearObjectCollisionResult_801A5E2 outside this file, which has no typed C interface yet */
static void clear_object_collision_result(void) {
    GUEST_CALL(battle_ClearObjectCollisionResult_801A5E2);
}

/* register form: calls battle_resetCollisionState_80101C4 outside this file, which has no typed C interface yet */
static void reset_collision_state(void) {
    GUEST_CALL(battle_resetCollisionState_80101C4);
}

/* register form: calls releaseSlotByValue_801DC36 outside this file, which has no typed C interface yet */
static void release_slot_by_value(void) {
    GUEST_CALL(releaseSlotByValue_801DC36);
}

/* register form: calls battle_clearAIDataAndBarrier_801A7F4 outside this file, which has no typed C interface yet */
static void clear_ai_data_and_barrier(void) {
    GUEST_CALL(battle_clearAIDataAndBarrier_801A7F4);
}

/* register form: calls GetVerActorTyAndAIIdx_80182B4 outside this file, which has no typed C interface yet */
static const u8 *get_ver_actor_ty_and_ai_idx(u16 enemy_idx) {
    R0 = enemy_idx;
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);
    return (const u8 *)(u32)R0;
}

/* register form: calls sub_802D644 outside this file, which has no typed C interface yet */
static void dispatch_actor_init_low(u8 ai_index) {
    R0 = ai_index;
    GUEST_CALL(sub_802D644);
}

/* register form: calls sub_802D65E outside this file, which has no typed C interface yet */
static void dispatch_actor_init_high(u8 ai_index) {
    R0 = ai_index;
    GUEST_CALL(sub_802D65E);
}

/* register form: calls relatedToIsBattleOver_800A11C outside this file, which has no typed C interface yet */
static void maybe_signal_battle_over(void) {
    GUEST_CALL(relatedToIsBattleOver_800A11C);
}

/* register form: calls sub_802EF5C outside this file, which has no typed C interface yet */
static void post_teardown_hook(void) {
    GUEST_CALL(sub_802EF5C);
}

/* Tear down one battle object (r5 = object ptr): clear collision region, drop slot,
 * blank chip/related-object state, clear AI and barrier, dispatch a version-specific
 * actor init based on the enemy's (version, actor_type, ai_index) record, optionally
 * signal the battle is over (when param2 < 1), then run a post-teardown hook and
 * set curPhaseAndPhaseInitialized = 4. Returns 4. */
static u32 teardown_battle_object(u32 obj_ptr) {
    clear_collision_region();
    clear_object_collision_result();
    reset_collision_state();
    release_slot_by_value();

    ST8(obj_ptr + oBattleObject_ChipsHeld, 0);
    ST16(obj_ptr + oBattleObject_Chip, (u16)(0 - 1));
    ST32(obj_ptr + oBattleObject_RelatedObject1Ptr, 0);

    clear_ai_data_and_barrier();

    u16 name_id = LD16(obj_ptr + oBattleObject_NameID);
    const u8 *rec = get_ver_actor_ty_and_ai_idx(name_id);

    u8 version = LD8(rec + 0);
    u8 actor_type = LD8(rec + 1);

    if (actor_type == 0) {
        u8 ai_index = LD8(rec + 2);
        if (version < 4) {
            dispatch_actor_init_low(ai_index);
        } else {
            dispatch_actor_init_high(ai_index);
        }
    }

    if (LD8(obj_ptr + oBattleObject_Param2) < 1) {
        maybe_signal_battle_over();
    }

    post_teardown_hook();
    ST16(obj_ptr + oBattleObject_CurPhaseAndPhaseInitialized, 4);
    return 4;
}

Next battle_teardownObject_80165F8(void) {
    R0 = teardown_battle_object(R5);
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

/* ── object_applyTintIfFlag16_80168F0 ── */
/* object_applyTintIfFlag16_80168F0(r0 = object):
 * If the object's state flags have bit 16 set, re-tint its sprites with the
 * palette shader whose id was loaded from this function's literal pool.
 * Note: `ldr r0, word_8016908` fetches a full 32-bit pool word, so the id
 * 0x7E94 is accompanied by the next function's `push {lr}` opcode (0xB500)
 * in the upper half; sprite_setColorShader receives that whole value. */
Next object_applyTintIfFlag16_80168F0(void) {
    /* bl object_getFlag — the object pointer in r0 is passed through as-is. */
    GUEST_CALL(object_getFlag);

    /* mov r1,#1; lsl r1,#16: build the flag mask. Shifting the constant 1
     * by 16 always shifts out a zero bit, so this clears the carry flag
     * (callers read c/v after this routine, matching the original's flags). */
    R1 = 1 << 16;
    cpu.c = 0;

    if (R0 & R1) {                 /* tst r0, r1 */
        R0 = 0xB5007E94;           /* ldr r0, word_8016908 */
        GUEST_CALL(sprite_setColorShader);
    }

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

/* ── stepBattleObjectWobble_8016BFC ── */
/* register form: calls AddRandomVarianceToTwoCoords outside this file, which has no typed C interface yet */
static void apply_random_variance(u32 amplitude, u32 x16, u32 y, u32 z16,
                                  u32 *out_x, u32 *out_y, u32 *out_z) {
    R0 = amplitude;
    R1 = x16;
    R2 = y;
    R3 = z16;
    GUEST_CALL(AddRandomVarianceToTwoCoords);
    *out_x = R1;
    *out_y = R2;
    *out_z = R3;
}

/* Battle-object wobble tick.
 * - If no animation is already in flight, snapshot X16/Z16 into Unk_30/Unk_32 and
 *   arm a 4-frame guard (PreventAnim := 4, Unk_19 := 0).
 * - Read the halfword at collision-data+0x80; if non-zero, force the wobble
 *   counter (Unk_19) to 30.
 * - While the wobble counter is non-zero, decrement it and store a perturbed
 *   (X, Y, Z) triple into the block at offset 0x34.
 * - When the counter has expired, copy Unk_30/Unk_32 back into X16/Z16 (rest pose).
 * Returns the wobble counter value seen (used as the NZ flag source at exit).
 *
 * Register output mapping:
 *   wobble path: r1/r2/r3 = call results, r4 = preserved from entry.
 *   no-wobble:   r1 = LD16(collision-data+0x80), r2/r3/r4 = preserved from entry. */
static u32 step_battle_object_wobble(u32 obj, u32 r1_in, u32 r2_in, u32 r3_in, u32 r4_in,
                                     u32 *out_r1, u32 *out_r2, u32 *out_r3, u32 *out_r4) {
    if (!LD8(obj + oBattleObject_PreventAnim)) {
        ST16(obj + oBattleObject_Unk_30, LD16(obj + oBattleObject_X16));
        ST16(obj + oBattleObject_Unk_32, LD16(obj + oBattleObject_Z16));
        ST8(obj + oBattleObject_Unk_19, 0);
        ST8(obj + oBattleObject_PreventAnim, 4);
    }

    u32 coll_word = LD16(LD32(obj + oBattleObject_CollisionDataPtr) + 0x80);
    if (coll_word)
        ST8(obj + oBattleObject_Unk_19, 0x1E);

    u32 wobble = LD8(obj + oBattleObject_Unk_19);
    if (wobble) {
        ST8(obj + oBattleObject_Unk_19, (u8)(wobble - 1));
        u32 x16 = (u32)LD16(obj + oBattleObject_Unk_30) << 16;
        u32 y   = LD32(obj + oBattleObject_Y);
        u32 z16 = (u32)LD16(obj + oBattleObject_Unk_32) << 16;
        u32 vx = 0, vy = 0, vz = 0;
        apply_random_variance(3, x16, y, z16, &vx, &vy, &vz);
        u32 dst = obj + 0x34;
        ST32(dst,     vx);
        ST32(dst + 4, vy);
        ST32(dst + 8, vz);
        *out_r1 = vx;
        *out_r2 = vy;
        *out_r3 = vz;
    } else {
        ST16(obj + oBattleObject_X16, LD16(obj + oBattleObject_Unk_30));
        ST16(obj + oBattleObject_Z16, LD16(obj + oBattleObject_Unk_32));
        *out_r1 = coll_word;
        *out_r2 = r2_in;
        *out_r3 = r3_in;
    }
    *out_r4 = r4_in;
    return wobble;
}

Next stepBattleObjectWobble_8016BFC(void) {
    u32 out_r1, out_r2, out_r3, out_r4;
    u32 nz_src = step_battle_object_wobble(R5, R1, R2, R3, R4,
                                           &out_r1, &out_r2, &out_r3, &out_r4);
    SET_FLAGS_NZ(nz_src);
    R1 = out_r1;
    R2 = out_r2;
    R3 = out_r3;
    R4 = out_r4;
    RETURN_TO_CALLER();
}

/* ── object_genericDestroy ── */
/* object_genericDestroy(r5 = object):
 * Generic battle-object destructor: release the object's reserved panel
 * tiles, its collision data, and finally the object memory itself.
 * Flags and r0-r3 are whatever object_freeMemory leaves behind. */
Next object_genericDestroy(void) {
    GUEST_CALL(RemoveBattleObjectPanelReserves);

    R0 = LD32(R5 + oBattleObject_CollisionDataPtr);
    GUEST_CALL(object_freeCollisionData);

    GUEST_CALL(object_freeMemory);
    RETURN_TO_CALLER();
}

/* ── battleObjectStateDispatcher_8016CE8 ── */
/* battleObjectStateDispatcher_8016CE8: battle object state dispatcher.
 *
 * R5 points to a battle object. Its byte at oBattleObject_Unk_0d holds a
 * *byte offset* into the handler table at off_8016CFC (Thumb `ldr rd,[rn,rm]`
 * adds the index register unscaled), so the three handlers are selected by
 * offsets 0, 4 and 8:
 *
 *   off_8016CFC + 0 -> battleAdvanceObjectPhase_8016D08
 *   off_8016CFC + 4 -> sub_8016D8C
 *   off_8016CFC + 8 -> battle_objectCountdownPhaseOrReset_8016E3C
 *
 * The handler is invoked with R0 = the state byte and R1 = the handler
 * address left over from the table load; every other register passes through
 * unchanged. After the handler returns, control goes straight back to this
 * function's caller.
 */
Next battleObjectStateDispatcher_8016CE8(void) {
    u32 state = LD8(R5 + oBattleObject_Unk_0d);

    R0 = state;
    R1 = LD32(A_off_8016CFC + state);   /* handler address, as the original leaves it */

    switch (state) {
    case 0:
        GUEST_CALL(battleAdvanceObjectPhase_8016D08);
        break;
    case 4:
        GUEST_CALL(sub_8016D8C);
        break;
    case 8:
        GUEST_CALL(battle_objectCountdownPhaseOrReset_8016E3C);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battleAdvanceObjectPhase_8016D08 ── */
#define MOVE_FLAG_BIT     (1u << 20)   /* "this object is in motion" */
#define PHASE_CLEAR_BIT   0x1000u
#define X_VEL_PER_STEP    0xA0000u
#define Y_VEL_PER_STEP    0x60000u

/* register form: calls object_setFlag1 outside this file, which has no typed C interface yet */
static void call_set_flag1(u32 flag) {
    R0 = flag;
    GUEST_CALL(object_setFlag1);
}

/* register form: calls battle_dispatchActorTurnByTypeAndAIIdx_800F404 outside this file */
static void call_dispatch_actor_turn(void) {
    GUEST_CALL(battle_dispatchActorTurnByTypeAndAIIdx_800F404);
}

/* register form: calls object_setCoordinatesFromPanels outside this file */
static void call_set_coords_from_panels(void) {
    GUEST_CALL(object_setCoordinatesFromPanels);
}

/* register form: calls object_updateCollisionPanels outside this file */
static void call_update_collision_panels(void) {
    GUEST_CALL(object_updateCollisionPanels);
}

/* register form: calls object_clearFlag outside this file, which has no typed C interface yet */
static void call_clear_flag(u32 bit) {
    R0 = bit;
    GUEST_CALL(object_clearFlag);
}

/* register form: calls battle_resetCollisionState_80101C4 outside this file */
static void call_reset_collision_state(void) {
    GUEST_CALL(battle_resetCollisionState_80101C4);
}

/* register form: calls object_removePanelReserve outside this file */
static void call_remove_panel_reserve(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(object_removePanelReserve);
}

/* register form: calls sub_800E45E outside this file, which has no typed C interface yet */
static u32 call_sub_800E45E(u32 *dy_out, u32 *phase_out) {
    GUEST_CALL(sub_800E45E);
    *dy_out = R1;
    *phase_out = R2;
    return R0;
}

/* register form: calls object_reservePanel outside this file */
static void call_reserve_panel(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(object_reservePanel);
}

/* Advance a battle object through one phase tick.
 * Sets the "moving" flag, dispatches the actor's turn, resolves pending panel
 * movement and collisions, releases the old panel, queries the next-step routine,
 * then either queues a panel-advance (signed velocity + new reservation) or
 * resets the object to its idle phase.
 * Returns the phase-state byte that must appear in r0 at exit (4 or 8);
 * nz is derived from it by the adapter.  r1/r2 are left by the final callee. */
static u32 battle_phase_advance(u32 obj) {
    /* Flag this object as moving and give it a turn in the dispatcher. */
    call_set_flag1(MOVE_FLAG_BIT);
    call_dispatch_actor_turn();

    /* Drop ally reference; promote the future panel to the current one. */
    ST32(obj + oBattleObject_RelatedObject1Ptr, 0);
    ST16(obj + oBattleObject_PanelXY, LD16(obj + oBattleObject_FuturePanelXY));
    call_set_coords_from_panels();
    call_update_collision_panels();

    /* Clear the "phase complete" bit and reset per-frame collision state. */
    call_clear_flag(PHASE_CLEAR_BIT);
    call_reset_collision_state();

    /* Release the panel this object currently occupies. */
    call_remove_panel_reserve(LD8(obj + oBattleObject_FuturePanelX),
                              LD8(obj + oBattleObject_FuturePanelY));

    /* Query the phase-transition routine for next-step data. */
    u32 dy_raw, phase_raw;
    u32 dx_raw = call_sub_800E45E(&dy_raw, &phase_raw);

    ST8(obj + oBattleObject_Unk_1c, dx_raw);
    ST8(obj + oBattleObject_Unk_1d, dy_raw);
    ST8(obj + oBattleObject_PhaseInitialized, phase_raw);

    if (phase_raw != 0) {
        /* Signed displacement determines per-step velocity and target panel. */
        s32 dx = (s8)dx_raw;
        s32 dy = (s8)dy_raw;
        ST32(obj + oBattleObject_XVelocity, (u32)(X_VEL_PER_STEP * dx));
        ST32(obj + oBattleObject_YVelocity, (u32)(Y_VEL_PER_STEP * dy));

        u32 new_fx = (u32)((s32)LD8(obj + oBattleObject_PanelX) + dx);
        u32 new_fy = (u32)((s32)LD8(obj + oBattleObject_PanelY) + dy);
        ST8(obj + oBattleObject_FuturePanelX, new_fx);
        ST8(obj + oBattleObject_FuturePanelY, new_fy);
        call_reserve_panel(new_fx, new_fy);

        ST8(obj + oBattleObject_Unk_0d, 4);
        return 4;
    }

    /* No pending phase step – return to idle. */
    ST8(obj + oBattleObject_CurPhase, 0x18);
    ST8(obj + oBattleObject_Unk_0d, 8);
    return 8;
}

/* battleAdvanceObjectPhase_8016D08(r5 = battle object ptr) -> r0 (4 or 8), r1/r2 from callee, nz */
Next battleAdvanceObjectPhase_8016D08(void) {
    R0 = battle_phase_advance(R5);
    SET_FLAGS_NZ(R0);
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

/* ── battleObject_updateSprite_8016E64 ── */
/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static u32 call_sprite_update(void) {
    GUEST_CALL(sprite_update);
    return R0;
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation(u32 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_sprite_loadAnimationData(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet */
static bool call_battle_isTimeStop(void) {
    GUEST_CALL(battle_isTimeStop);
    return !ZERO_FLAG();
}

/* register form: calls battle_isPaused outside this file, which has no typed C interface yet */
static bool call_battle_isPaused(void) {
    GUEST_CALL(battle_isPaused);
    return !ZERO_FLAG();
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 call_GetBattleMode_sub_8016E64(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls object_getFlag outside this file, which has no typed C interface yet */
static u32 call_object_getFlag(void) {
    GUEST_CALL(object_getFlag);
    return R0;
}

/* register form: calls object_getFlag2 outside this file, which has no typed C interface yet */
static u32 call_object_getFlag2(void) {
    GUEST_CALL(object_getFlag2);
    return R0;
}

/* Drives one battle object's sprite animation, gated on several conditions.
 * Side effect: writes oBattleObject_CurAnimCopy, calls sprite_update. */
static void update_battle_object_sprite(u32 obj) {
    /* TST header_flags, #8  -> bne early-out (no r0/r1/flags change in adapter) */
    u8 header_flags = LD8(obj + oObjectHeader_Flags);
    if (header_flags & 8)
        return;

    if (call_battle_isTimeStop())                 /* bne early-out */
        return;

    u16 name_id = LD16(obj + oBattleObject_NameID);
    if (name_id < 0x49 || name_id > 0x4E) {
        u16 hp = LD16(obj + oBattleObject_HP);
        if (hp == 0)                              /* TST hp,hp -> beq early-out */
            return;
    }

    u32 mode = call_GetBattleMode_sub_8016E64();
    if (mode != 1) {                              /* CMP #1 -> beq skip_pause */
        if (call_battle_isPaused())               /* bne early-out */
            return;
    }

    /* TST getFlag, #0x80110C00 */
    if (call_object_getFlag() & 0x80110C00)
        return;
    /* TST getFlag2, #8 */
    if (call_object_getFlag2() & 8)
        return;

    u8 cur_anim = LD8(obj + oBattleObject_CurAnim);
    if (cur_anim != LD8(obj + oBattleObject_CurAnimCopy)) {
        call_sprite_setAnimation(cur_anim);
        call_sprite_loadAnimationData();
        ST8(obj + oBattleObject_CurAnimCopy, cur_anim);
    }

    call_sprite_update();
}

Next battleObject_updateSprite_8016E64(void) {
    update_battle_object_sprite(R5);
    RETURN_TO_CALLER();
}
