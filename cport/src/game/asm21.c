/*
 * asm21.c: readable functions of bn6f asm21.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── bgScroll_ResetScrollCBCounters_8080D90 ── */
/* bgScroll_ResetScrollCBCounters_8080D90: clear the BG scroll callback counters (8 bytes). */
Next bgScroll_ResetScrollCBCounters_8080D90(void) {
    R0 = A_eBGScrollCBCounters;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);
    RETURN_TO_CALLER();
}

/* ── battle_initBGScrollAndAnimForPuzzleField_8080DA0 ── */
/* battle_initBGScrollAndAnimForPuzzleField_8080DA0: load the BG scroll/GFX-anim/LCD-control data for the current battle field
 * and, in non-battle mode (i.e. story/custom), preload the death animations.
 *
 * Inputs:  r10 = select index into the field tables (passed through from caller)
 * Outputs: r6, r7, r8, r9, r10, r11, r12 (per the disassembly contract)
 * Clobbers: nz, c, v, r0..r3, lr
 */
Next battle_initBGScrollAndAnimForPuzzleField_8080DA0(void) {
    /* sub_8081308 returns the field index used to look up everything else. */
    GUEST_CALL(sub_8081308);
    u32 field = R0;

    /* Scroll callback table for the BG layer: a struct of three function pointers
     * (offset 0/4/8) followed by an LCD-control OR mask at offset 0xC. */
    u32 scroll = A_off_8080E34 + (field << 4);
    R0 = LD32(scroll);
    R1 = LD32(scroll + 4);
    R2 = LD32(scroll + 8);
    GUEST_CALL(SetBGScrollCallbacks);

    /* Apply the OR mask on top of the current LCD control register. */
    GUEST_CALL(GetRenderInfoLCDControl);
    R0 = R0 | LD32(scroll + 0xC);
    GUEST_CALL(SetRenderInfoLCDControl);

    /* BG animation script (one entry per field). */
    u32 bgAnimTable = A_off_8080F98;
    R0 = LD32(bgAnimTable + (field << 2));
    GUEST_CALL(LoadBGAnimData);

    /* Sprite/GFX animation scripts (table indexed by field). */
    u32 gfxAnimTable = A_off_8081220;
    R0 = LD32(gfxAnimTable + (field << 2));
    GUEST_CALL(LoadGFXAnims);

    /* Only the non-battle UI needs them now; the battle scenes have their own. */
    GUEST_CALL(GetBattleMode);
    if (R0 != 1) {
        GUEST_CALL(sub_803537C);
        GUEST_CALL(battle_loadDeathAnims_8035424);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8081308 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result call_TestEventFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Determine the weather-computron puzzle status for the current map.
 * Returns: 7 (not a qualifying map), the battle-settings byte (non-battle path),
 * 0x10 (puzzle solved / not found), or 0x15 (puzzle event flag still unset). */
static u32 weather_comp_status(u32 toolkit) {
    /* If the battle-settings byte at +4 isn't 0xFF we're in a non-standard
     * battle; return that byte unchanged. */
    u32 battle_state = LD32(toolkit + oToolkit_BattleStatePtr);
    u32 battle_settings = LD32(battle_state + oBattleState_BattleSettings);
    u32 settings_byte = LD8(battle_settings + 4);
    if (settings_byte != 0xFF)
        return settings_byte;

    /* Outside qualifying battle – check current map group/number. */
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 map_group = LD8(game_state + 4);   /* oGameState_MapGroup */
    u32 map_number = LD8(game_state + 5);  /* oGameState_MapNumber */
    if (map_group < 0x80)
        return 7;

    /* Index the per-group byte table (pt_808139C). */
    u32 table = LD32(A_pt_808139C + ((map_group - 0x80) << 2));
    if (table == 0)
        return 7;

    u32 weather_byte = LD8(table + map_number);
    if (weather_byte != 0x10)
        return weather_byte;

    /* weather_byte == 0x10 → scan the (map_id, event_flag) rule list.
     * The list is halfword pairs terminated by 0x0000. */
    u16 map_group_hw = LD16(game_state + 4);  /* halfword view of map group */
    u32 cursor = A_word_8081368;

    while (LD16(cursor) != 0) {
        if (LD16(cursor) == map_group_hw) {
            u16 event_flag = LD16(cursor + 2);
            call_TestEventFlag_result res = call_TestEventFlag(event_flag);
            /* Z clear → flag is set → puzzle solved, return 0x10.
             * Z set   → flag not set → puzzle available, return 0x15. */
            if (!res.z)
                return 0x10;
            return 0x15;
        }
        cursor += 4;
    }

    /* No matching rule found; weather_byte (0x10) passes through. */
    return 0x10;
}

/* sub_8081308(r10 = toolkit) → r0 */
Next sub_8081308(void) {
    R0 = weather_comp_status(R10);
    RETURN_TO_CALLER();
}
