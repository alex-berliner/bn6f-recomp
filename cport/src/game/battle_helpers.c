/*
 * battle_helpers.c — small battle queries (bn6f object.s, battle.s).
 *
 * Written by outside models with tools/convert/farm.py from the assembly and
 * the lifted C, and kept only after matching the lifted functions on the
 * original's own captured calls and on random inputs; then checked with the
 * lockstep tests like all hand-written code. Model per function: GLM-5.3
 * (battle_isTimeStop, GetBattleMode, GetBattleNaviStatsByte_AllianceFrom-
 * BattleObject), Kimi K3 (sub_800F29C, sub_800C488, object_isValidPanel).
 */
#include "game.h"
#include "runtime.h"

/* battle_isTimeStop (0x0800A098):
 * Reports whether a time-stop effect is currently freezing the battle
 * clock. Reads the battle flags and keeps only the time-stop bit:
 * r0 = flags & TIMESTOP (nonzero means time is stopped), r1 = TIMESTOP,
 * condition flags set from the result. */
#define BATTLE_FLAG_TIMESTOP (1u << 2) /* battle flags bit 2: time stopped */

Next battle_isTimeStop(void) {
    GUEST_CALL(battle_getFlags);   /* r0 = current battle flags */

    R1 = BATTLE_FLAG_TIMESTOP;
    R0 &= R1;                      /* keep only the time-stop bit */
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* GetBattleMode(r10 = toolkit):
 * returns the battle mode recorded in the toolkit's battle state settings. */
Next GetBattleMode(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD32(battleState + oBattleState_BattleSettings);
    GUEST_CALL(GetBattleModeFromBattleSettings);
    RETURN_TO_CALLER();
}

/* GetBattleNaviStatsByte_AllianceFromBattleObject(r5 = *BattleObject, r1 = statOffset):
 * read a single stat byte from the NaviStats block belonging to the object's
 * side (alliance selects the player's or the enemy's stats). */
Next GetBattleNaviStatsByte_AllianceFromBattleObject(void) {
    u32 statOffset = R1;

    /* the object's alliance picks which NaviStats block to use */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(GetBattleNaviStatsAddr);   /* -> *NaviStats */

    R0 = LD8(R0 + statOffset);            /* NaviStats[statOffset] */
    RETURN_TO_CALLER();
}

/* sub_800F29C(r0 = enemy index): fetch the enemy's version/actor-type/AI-index
 * record and unpack it: r0 = actor type, r1 = AI index, r2 = version. */
Next sub_800F29C(void) {
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4); // r0 -> struct { u8 version; u8 actor_type; u8 ai_index; }
    u32 info = R0;
    u32 version    = LD8(info);
    u32 actor_type = LD8(info + 1);
    u32 ai_index   = LD8(info + 2);
    R0 = actor_type;
    R1 = ai_index;
    R2 = version;
    RETURN_TO_CALLER();
}

/* sub_800C488: r1 = 0x1E0 (480) when GetBattleMode() reports mode 1, 0x258
 * (600) otherwise. Its callers (object.s) store r1 as a halfword at offset
 * 0x0E of the structure in r7, several next to 0x708 (1800) at 0x12, so
 * these are likely frame counts. r0, r2 and r3 keep their incoming values. */
Next sub_800C488(void) {
    /* r0/r2/r3 are preserved across this call, as the original's push/pop does */
    u32 savedR0 = R0, savedR2 = R2, savedR3 = R3;

    GUEST_CALL(GetBattleMode);

    R1 = (R0 == 1) ? 0x1E0 : 0x258;

    R0 = savedR0;
    R2 = savedR2;
    R3 = savedR3;
    RETURN_TO_CALLER();
}

/* object_isValidPanel(r0 = panel column, r1 = panel row):
 * check whether 1-based panel coordinates lie inside the 6x3 battle
 * grid (columns 1-6, rows 1-3). Returns r0 = 1 when valid, 0 otherwise.
 * r1 is only decremented once the column check has passed. Flags: N/Z
 * come from the result moved into r0, C/V survive from the last bounds
 * compare that ran. */
Next object_isValidPanel(void) {
    u32 col = R0 - 1;             /* column as 0-based index */
    u32 valid = 0;

    SET_FLAGS_CMP(col, 6);
    if (col < 6) {
        R1 -= 1;                  /* row as 0-based index */
        SET_FLAGS_CMP(R1, 3);
        if (R1 < 3)
            valid = 1;
    }

    R0 = valid;
    SET_FLAGS_NZ(R0);             /* MOVS r0, #0 / #1: sets N/Z, keeps C/V */
    RETURN_TO_CALLER();
}
