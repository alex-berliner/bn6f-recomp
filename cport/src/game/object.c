/*
 * object.c: readable functions of bn6f object.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_800B884 ── */
/* sub_800B884(r0 = object, r1 = value):
 * Resolve an address via byte_203CF00_getEntryPtr_800BF5C (passing r0 through unchanged),
 * then store the saved byte value at [result + 1].
 * Returns r0 = byte_203CF00_getEntryPtr_800BF5C's result, r1 unchanged; flags are whatever
 * byte_203CF00_getEntryPtr_800BF5C leaves behind. */
Next sub_800B884(void) {
    u32 value = R1;

    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);

    ST8(R0 + 1, value);
    R1 = value;
    RETURN_TO_CALLER();
}

/* ── sub_800B892 ── */
/* sub_800B892(r0 = object pointer): return the byte at offset 1 of
 * whatever byte_203CF00_getEntryPtr_800BF5C resolves the object to (its type/flag field). */
Next sub_800B892(void) {
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    R0 = LD8(R0 + 1);
    RETURN_TO_CALLER();
}

/* ── battle_resetObjAnimState_800B89C ── */
/* battle_resetObjAnimState_800B89C(r0 = battle object):
 * Clears an object's animation state. byte_203CF00_getEntryPtr_800BF5C resolves the object's
 * sprite/animation entry, then the byte at +1 (current animation frame) and
 * the word at +8 (animation timer/counter) are reset to 0.
 *
 * Flags: the only flag-setting instructions here are the two Thumb "mov r1,#0"
 * encodings (MOVS, which only affects n/z), so c and v are left exactly as
 * byte_203CF00_getEntryPtr_800BF5C returned them. The stores below set no flags, preserving that. */
Next battle_resetObjAnimState_800B89C(void) {
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C); /* r0 passes through unchanged as the argument */

    u32 anim = R0;

    ST8(anim + 1, 0);
    ST32(anim + 8, 0);

    RETURN_TO_CALLER();
}

/* ── writeByteToWorkSlots0And1_800B8AC ── */
/* writeByteToWorkSlots0And1_800B8AC(r0 = value):
 * Store the given byte into two consecutive single-byte state slots,
 * fetched via byte_203CF00_getEntryPtr_800BF5C with slot indices 0 and 1. r4 is preserved. */
Next writeByteToWorkSlots0And1_800B8AC(void) {
    u32 value = R0;

    R0 = 0;
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    ST8(R0, value);

    R0 = 1;
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    ST8(R0, value);

    RETURN_TO_CALLER();
}

/* ── sub_800B8C2 ── */
/* sub_800B8C2(r0 = slot/index):
 * Calls byte_203CF00_getEntryPtr_800BF5C with the given argument, reads the byte it points to, and
 * reports whether that byte matches the original argument.
 * Returns r0 = 1 if equal, 0 otherwise; r2 is left holding the original argument. */
Next sub_800B8C2(void) {
    u32 arg = R0;

    R0 = arg;
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);

    u8 value = LD8(R0);

    R2 = arg;                      /* original's `pop {r2}` restores the saved r0 */
    R0 = (value == arg) ? 1 : 0;

    RETURN_TO_CALLER();
}

/* ── sub_800B8D8 ── */
/* sub_800B8D8(r0 = entry index):
 * Fetch the entry for this index via byte_203CF00_getEntryPtr_800BF5C and compare its byte at
 * offset 3 (the entry's type/kind field) against the index we were given.
 * Returns r0 = 1 on a match, 0 otherwise; r2 is left holding the index and
 * the flags reflect the comparison (CMP) so callers can branch directly.
 */
Next sub_800B8D8(void) {
    u32 index = R0;

    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    u32 entry = R0;

    u32 kind = LD8(entry + 3);

    R2 = index;
    SET_FLAGS_CMP(kind, index);
    R0 = (kind == index) ? 1 : 0;

    RETURN_TO_CALLER();
}

/* ── object_timefreezeBegin ── */
/* Offsets/constants used here that game.h does not export. */
#define oBattleObject_CurState 8   /* current state byte in the battle object */
#define CUR_STATE_UPDATE      4   /* object state: run normal update logic */

/* object_timefreezeBegin (0x0800B916): start a time freeze for the battle
 * object whose pointer is in R5.
 *
 * - Objects on a non-inverted network queue the freeze dispatch (0x40).
 * - Objects recognized by sub_800B8D8 (player side) set battle flag 4.
 * - The object's per-alliance entry in the freeze counter table
 *   (dword_200F3B8) is cleared, and the object moves to the UPDATE state.
 *
 * Flags on return match the original's last flag-setting instructions:
 * C from the index shift (lsl r0, r0, #2), N/Z from MOV r0, #CUR_STATE_UPDATE.
 * R0 = CUR_STATE_UPDATE, R1 = &dword_200F3B8, R2 = 0 on return. */
Next object_timefreezeBegin(void) {
    /* Only queue the freeze dispatch when the network is not inverted. */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(battle_networkInvert);
    if (R0 == 0) {
        R0 = 0x40;
        GUEST_CALL(dispatch_801DACC);
    }

    /* Player-side objects additionally raise battle flag 4. */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(sub_800B8D8);
    if (R0 != 0) {
        R0 = 4;
        GUEST_CALL(battle_setFlags);
    }

    /* Clear dword_200F3B8[alliance]; the <<2 index shift sets the C flag. */
    u32 alliance = LD8(R5 + oBattleObject_Alliance);
    R0 = alliance << 2;
    cpu.c = (alliance >> 30) & 1;
    R1 = A_dword_200F3B8;
    R2 = 0;
    ST32(R1 + R0, 0);

    /* Mark the object as ready for its normal update. */
    R0 = CUR_STATE_UPDATE;
    SET_FLAGS_NZ(R0);
    ST8(R5 + oBattleObject_CurState, CUR_STATE_UPDATE);
    RETURN_TO_CALLER();
}

/* ── object_dimScreen ── */
/* oBattleObject_CurAction lives at offset 9 (the assembly accesses it as
 * [r5,#9]; the shared header only names it oBattleObject_CurAnim there). */
#define oBattleObject_CurAction 9

/* object_dimScreen(r5 = battle object):
 * Battle-scene step that dims the screen (e.g. when the round is decided).
 * On the first invocation it starts a screen fade, resets the object's timer
 * and marks the phase as initialized; on every invocation it ticks the timer
 * and, once the fade has finished, advances CurAction by 4 and clears
 * CurPhaseAndPhaseInitialized. Flags and R0-R3 are left as the original
 * leaves them: R0 is the (nonzero) IsScreenFadeActive result on the early
 * return, 0 otherwise; N/Z come from the last TST/MOV, C/V from the last
 * Thumb ADDS. */
Next object_dimScreen(void) {
    /* One-time setup: begin the fade to black, reset the timer. */
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        R0 = 0x3C;
        R1 = 4;
        GUEST_CALL(SetScreenFade);
        ST16(R5 + oBattleObject_Timer, 0);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    }

    /* Tick the fade timer every frame. */
    u32 timer = LD16(R5 + oBattleObject_Timer) + 1;
    SET_FLAGS_ADD(timer, 1); /* add r0, #1 sets C/V; N/Z overwritten below */
    ST16(R5 + oBattleObject_Timer, timer);

    /* Wait until the screen fade has completed. */
    GUEST_CALL(IsScreenFadeActive);
    if (R0 != 0) {
        SET_FLAGS_NZ(R0); /* tst r0, r0 */
        RETURN_TO_CALLER();
    }

    /* Fade done: move on to the next action state and reset the phase. */
    u32 action = LD8(R5 + oBattleObject_CurAction) + 4;
    SET_FLAGS_ADD(action, 4);
    ST8(R5 + oBattleObject_CurAction, action);

    SET_FLAGS_NZ(0); /* mov r0, #0 */
    ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── sub_800BDB2 ── */
/* sub_800BDB2 — battle object phase dispatcher.
 *
 * Reads the object's current phase byte and jumps through the table at
 * off_800BDC4 to that phase's handler:
 *   phase 0 -> sub_800BDD0, phase 1 -> sub_800BE0C, phase 2 -> sub_800BE2C.
 *
 * The jump is effectively a tail call: the original sets lr to the following
 * `pop {pc}`, which restores this function's caller, so the handler returns
 * straight to whoever called sub_800BDB2. The handler's results are exactly
 * the outputs of this function (r5, r8-r12); it receives the object in r5 and
 * whatever r10 held, both of which pass through untouched.
 */

/* register form: the handler address is data loaded from the jump table
 * A_off_800BDC4, not a named game function, so no generated wrapper exists
 * and GUEST_CALL_ADDR is the only way to reach it. The guest call leaves the
 * handler's r5/r8-r12 results in the guest registers, which is what the
 * adapter must hand back to the caller. */
static void call_phase_handler_sub_800BDB2(u32 target, u32 phase) {
    R0 = phase;   /* the original keeps the phase byte in r0 across the jump */
    R1 = target;  /* ...and the table entry itself in r1 */
    GUEST_CALL_ADDR(target);
}

static void dispatch_object_phase(u32 object) {
    u32 phase  = LD8(object + oBattleObject_CurPhase);
    u32 target = LD32(A_off_800BDC4 + phase);
    call_phase_handler_sub_800BDB2(target, phase);
}

Next sub_800BDB2(void) {
    dispatch_object_phase(R5);
    RETURN_TO_CALLER();
}

/* ── byte_203CF00_getEntryPtr_800BF5C ── */
/* byte_203CF00_getEntryPtr_800BF5C(r0 = index):
 * Convert a slot index into an address within the byte_203CF00 array,
 * which has 0x50 (80) bytes per entry.
 * Returns: r0 = byte_203CF00 + index * 0x50, r1 = byte_203CF00,
 * flags set by the final address computation (ADDS). */
Next byte_203CF00_getEntryPtr_800BF5C(void) {
    u32 base = A_byte_203CF00;

    u32 result = R0 * 0x50 + base;

    SET_FLAGS_ADD(R0 * 0x50, base);

    R0 = result;
    R1 = base;
    RETURN_TO_CALLER();
}

/* ── zeroFill_800BF66 ── */
/* zeroFill_800BF66: clear the 0xA0-byte work buffer at byte_203CF00.
 * r2/r3 pass through untouched; r0/r1 and flags are whatever
 * ZeroFillByWord leaves behind. */
Next zeroFill_800BF66(void) {
    R0 = A_byte_203CF00;
    R1 = 0xA0;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── sub_800BF88 ── */
/* sub_800BF88: reset the six two-byte selection slots at byte_2036740.
 * Each slot gets its "count" byte cleared to 0 and its partner byte set to a
 * descending default value (0xE, 0xD, ..., 9). Before that it calls
 * battle_buildPlayerPanelGrid_800C4BC (whose r2/r3/r5 results and carry/overflow flags the callers
 * of this function rely on) and stores 0x8C into the state word at
 * byte_203CB04. r4/r6/r7 are only used across the call, so they come back
 * unchanged, as in the original's push/pop. */
Next sub_800BF88(void) {
    /* battle_buildPlayerPanelGrid_800C4BC may clobber r4/r6/r7; the original saves and restores
     * them around the call, so capture them here and put them back. */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    GUEST_CALL(battle_buildPlayerPanelGrid_800C4BC);
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;

    ST32(A_byte_203CB04, 0x8C);

    u32 slots = A_byte_2036740;
    for (u32 i = 0; i < 6; i++) {
        ST8(slots + 2 + 2 * i, 0);      /* even byte: count = 0        */
        ST8(slots + 3 + 2 * i, 0xE - i); /* odd byte: 0xE, 0xD, ... 9 */
    }

    /* Last flag-setting instruction of the original is "mov r0, #9". */
    SET_FLAGS_NZ(9);
    RETURN_TO_CALLER();
}

/* ── battle_buildPlayerPanelGrid_800C4BC ── */
/* register form: calls getTable12Record outside this file, which has no typed C interface yet */
static u32 get_table12_record(u32 arg0, u32 arg1) {
    R0 = arg0; R1 = arg1;
    GUEST_CALL(getTable12Record_800D730_800C6D6);
    return R0;
}

/* register form: calls battle_getPanelEntry outside this file, which has no typed C interface yet */
static u32 get_panel_entry(u32 index) {
    R0 = index;
    GUEST_CALL(battle_getPanelEntry_800C8C2);
    return R0;
}

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill(u32 ptr, u32 bytes) {
    R0 = ptr; R1 = bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls battle_getPanelDataPtr outside this file, which has no typed C interface yet */
static u32 get_panel_data_ptr(u32 col, u32 row) {
    R0 = col; R1 = row;
    GUEST_CALL(battle_getPanelDataPtr_800C6E0);
    return R0;
}

/* register form: calls object_isValidPanel outside this file, which has no typed C interface yet */
static u32 is_valid_panel(u32 col, u32 row) {
    R0 = col; R1 = row;
    GUEST_CALL(object_isValidPanel);
    return R0;
}

/* register form: calls sub_800C488 outside this file, which has no typed C interface yet */
static u32 compute_panel_attr(u32 tile_type) {
    R0 = tile_type;
    GUEST_CALL(sub_800C488);
    return R1;
}

/* register form: calls sub_800C67C outside this file, which has no typed C interface yet */
static void finalize_panels(u32 r2_in, u32 r3_in) {
    R2 = r2_in; R3 = r3_in;
    GUEST_CALL(sub_800C67C);
    /* leaves c, v, r2, r3 in the guest state */
}

/*
 * Build the 5×8 battle-panel grid and per-column entry flags for the
 * player's side.
 *
 *   col_mask – one bit per interior column (1..6) indicating an active panel.
 *   record   – table12 record holding 4-bit HP nibbles per grid cell.
 *
 * After populating ePanelData, calls sub_800C67C which finalises the
 * grid state and answers in r2/r3/c/v.
 */
static void build_battle_panels(u32 arg0, u32 col_mask, u32 r2_in, u32 r3_in) {
    /* Retrieve the table12 record that stores per-panel HP nibbles */
    u32 record = get_table12_record(arg0, col_mask);

    /* Zero the 8 × 8-byte player panel-entry array (0x40 bytes total) */
    u32 entry_base = get_panel_entry(0);
    zero_fill(entry_base, 0x40);

    /* Set byte 0 of each column entry: interior columns get their mask bit,
       edge columns (0 and 7) are marked 0xFF (not player-occupied). */
    entry_base = get_panel_entry(0);
    for (int col = 0; col < 8; col++) {
        u32 val = 0xFF;
        if ((u32)(col - 1) <= 5) {
            val = (col_mask >> (col - 1)) & 1;
        }
        ST8(entry_base + col * 8, (u8)val);
    }

    /* Zero the entire 8 × 5 panel-data grid (0x500 bytes) */
    zero_fill(A_ePanelData, 0x500);

    /* Populate each panel-data entry (stride 0x14 within ePanelData) */
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 8; col++) {
            u32 dp = get_panel_data_ptr(col, row);
            u32 idx = row * 8 + col;

            /* Offset 0x00 – walkability / obstacle flag */
            ST8(dp, LD8(A_byte_800C590 + idx));

            /* Offset 0x02 / 0x06 – 4-bit HP value from the table record */
            u32 hp = 0;
            if (is_valid_panel(col, row)) {
                u32 word = LD32(record + (row - 1) * 4);
                hp = (word >> ((col - 1) * 4)) & 0xF;
            }
            ST8(dp + 2, (u8)hp);
            ST8(dp + 6, (u8)hp);

            /* Offsets 0x03 / 0x04 / 0x07 – column entry's active byte */
            u8 flag = (u8)LD8(get_panel_entry(col));
            ST8(dp + 3, flag);
            ST8(dp + 4, flag);
            ST8(dp + 7, flag);

            /* Offset 0x0C – secondary tile property */
            ST8(dp + 0xC, LD8(A_byte_800C5B8 + idx));

            /* Offsets 0x0A / 0x0B – grid coordinates */
            ST8(dp + 0xA, (u8)col);
            ST8(dp + 0xB, (u8)row);

            /* Offset 0x0E – computed tile attribute from sub_800C488 */
            ST16(dp + 0xE, (u16)compute_panel_attr(LD8(A_byte_800C5B8 + idx)));

            /* Offset 0x12 – constant word 0x708 */
            ST16(dp + 0x12, 0x708);
        }
    }

    /* Finalise the grid; sub_800C67C sets r2, r3, c, v as outputs */
    finalize_panels(r2_in, r3_in);
}

/* battle_buildPlayerPanelGrid_800C4BC(r0 = table arg, r1 = column bitmask, r2/r3 = passthrough to sub_800C67C)
 * -> r2, r3, c, v (from sub_800C67C) */
Next battle_buildPlayerPanelGrid_800C4BC(void) {
    build_battle_panels(R0, R1, R2, R3);
    RETURN_TO_CALLER();
}

/* ── getTable12Record_800D730_800C6D6 ── */
/* getTable12Record_800D730_800C6D6(r0 = table index):
 * Returns the address of entry r0 in a table of 12-byte records at byte_800D730.
 * Output: r0 = byte_800D730 + r0 * 12. */
Next getTable12Record_800D730_800C6D6(void) {
    R0 = R0 * 12 + A_byte_800D730;
    RETURN_TO_CALLER();
}

/* ── battle_getPanelDataPtr_800C6E0 ── */
/* Panel geometry on the battle field: 8 columns per row, and each panel's
 * data entry is 0x20 bytes inside ePanelData. */
#define PANELS_PER_ROW 8
#define PANEL_SIZE     0x20

/* battle_getPanelDataPtr_800C6E0(r0 = panel column, r1 = panel row):
 * compute the address of a panel's data entry in ePanelData.
 * Returns r0 = &ePanelData[(row * 8 + column) * 0x20]; r2 is left at 8
 * (a leftover scratch value callers rely on as an output). */
Next battle_getPanelDataPtr_800C6E0(void) {
    u32 panelIndex = R0 + R1 * PANELS_PER_ROW;

    R0 = panelIndex * PANEL_SIZE + A_ePanelData;
    R2 = PANELS_PER_ROW;

    RETURN_TO_CALLER();
}

/* ── sub_800C6F0 ── */
/* sub_800C6F0(r0 = object):
 *   For each panel column 1..3 owned by `object`, look up its panel data.
 *   If a panel exists and its alliance byte differs from its teammate byte:
 *     - increment r6 (count of such panels)
 *     - OR (1 << col) into the bitmask at [sp+0]
 *     - if the panel's oPanelData_Flags does NOT have any of the bits in 0x00F880080:
 *         - increment r7 (count of panels also satisfying the flags check)
 *         - OR (1 << col) into the bitmask at [sp+4]
 *   Returns:
 *     r0 = total panel count (r6)
 *     r1 = flags-clean panel count (r7)
 *     r2 = bitmask of all counted panels (1..3 bits)
 *     r3 = bitmask of flags-clean panels (1..3 bits)
 */
Next sub_800C6F0(void) {
    u32 object = R0;

    u32 buf = SP;
    SP -= 8;

    ST32(SP + 0, 0);  /* bitmask of all counted panels */
    ST32(SP + 4, 0);  /* bitmask of flags-clean panels */

    u32 total_mask = 0;
    u32 clean_mask = 0;
    u32 total = 0;
    u32 clean = 0;

    for (u32 col = 1; (s32)col <= 3; col++) {
        R0 = object;
        R1 = col;
        GUEST_CALL(object_getPanelDataOffset);

        if (R0 == 0) continue;
        u32 panel = R0;

        u32 alliance = LD8(panel + oPanelData_Alliance);
        u32 unk04    = LD8(panel + oPanelData_Unk_04);
        if (alliance == unk04) continue;

        total++;
        total_mask |= (1u << col);

        u32 flags = LD32(panel + oPanelData_Flags);
        if ((flags & 0x00F880080u) != 0) continue;

        clean++;
        clean_mask |= (1u << col);
    }

    SP = buf;

    R0 = total;
    R1 = clean;
    R2 = total_mask;
    R3 = clean_mask;
    RETURN_TO_CALLER();
}

/* ── battle_updatePanels_800C746 ── */
#define PANEL_TEAMS     6
#define PANEL_COLS      6
#define PANEL_ROWS      3
#define PAIR_BUF_SLOTS  8   /* 16-byte stack scratch → up to 4 (panel, flag) pairs */

/* register form: calls sub_800C6F0 outside this file, which has no typed C interface yet */
static void call_update_team_counter(u32 team, u8 *out0, u8 *out1, u8 *out2, u8 *out3) {
    R0 = team;
    GUEST_CALL(sub_800C6F0);
    *out0 = (u8)R0; *out1 = (u8)R1; *out2 = (u8)R2; *out3 = (u8)R3;
}

/* register form: calls battle_getPanelEntry_800C8C2 outside this file, which has no typed C interface yet */
static u32 call_get_panel_entry(u32 panel_id) {
    R0 = panel_id;
    GUEST_CALL(battle_getPanelEntry_800C8C2);
    return R0;
}

/* register form: calls sub_800C81E outside this file, which has no typed C interface yet */
static void call_clear_panel_pair(u32 panel_id, u32 flag, u32 aux) {
    R0 = panel_id; R1 = flag; R2 = aux;
    GUEST_CALL(sub_800C81E);
}

/* register form: calls object_getPanelDataOffset outside this file, which has no typed C interface yet */
static u32 call_get_panel_data(u32 col, u32 row) {
    R0 = col; R1 = row;
    GUEST_CALL(object_getPanelDataOffset);
    return R0;
}

/*
 * Process team counters, resolve panel-encounter pair events, and tick the
 * 6×3 panel grid's cell timers while toggling alliance-visibility bits.
 * Returns the value that must remain in R3 at function exit (from the last
 * grid cell processed in Section 3).
 */
static u32 process_battle_panels(void) {
    u32 final_r3 = 0;

    /* ── Section 1: advance each team's 8-byte battle-state record ── */
    u32 team_base = A_unk_2034018;
    for (u32 team = 1; team <= PANEL_TEAMS; team++) {
        u8 v0, v1, v2, v3;
        call_update_team_counter(team, &v0, &v1, &v2, &v3);

        ST8(team_base + 2, v0);
        ST8(team_base + 3, v1);
        ST8(team_base + 4, v2);
        ST8(team_base + 5, v3);

        if (v0 == 0)
            ST16(team_base + 6, 0);

        u16 timer = LD16(team_base + 6);
        if (timer != 0) {
            timer--;
            ST16(team_base + 6, timer);
            if (timer == 0)
                ST8(team_base + 1, 1);
        }
        team_base += 8;
    }

    /* ── Section 2: walk byte_203F6B0 event table, collect consecutive
     *             matching panel-encounter pairs, then clear them ── */
    u32 ev = A_byte_203F6B0;
    while (LD8(ev) != 0xFF) {
        u32 panel_id = LD8(ev);
        s32 steps    = LD8(ev + 2);
        s32 pairs    = 0;
        u8  pair_buf[PAIR_BUF_SLOTS * 2];

        for (s32 i = steps; i > 0; i--) {
            u32 entry = call_get_panel_entry(panel_id);
            u8  flag  = LD8(entry + 4);

            if (flag != 0) {
                if (LD8(entry + 1) != 0 && flag == LD8(entry + 5)) {
                    pair_buf[pairs * 2]     = (u8)panel_id;
                    pair_buf[pairs * 2 + 1] = LD8(entry + 5);
                    pairs++;
                } else {
                    pairs = 0;
                }
            }
            panel_id += (s8)LD8(ev + 1);
        }

        for (s32 j = 0; j < pairs; j++) {
            u32 entry = call_get_panel_entry(pair_buf[j * 2]);
            ST8(entry + 1, 0);
            call_clear_panel_pair(pair_buf[j * 2],
                                  pair_buf[j * 2 + 1],
                                  LD8(ev + 3));
        }

        ev += 4;
    }

    /* ── Section 3: tick the 6×3 panel grid; toggle alliance-visibility
     *             on even timer decrements.  The last cell determines
     *             the final R3 the callers read. ── */
    for (u32 row = 1; row <= PANEL_ROWS; row++) {
        for (u32 col = 1; col <= PANEL_COLS; col++) {
            u32 pd       = call_get_panel_data(col, row);
            u8  alliance = LD8(pd + oPanelData_Alliance);
            ST8(pd + oPanelData_Unk_07, alliance);

            u16 cell_timer = LD16(pd + oPanelData_Unk_10);
            if (cell_timer != 0) {
                cell_timer--;
                ST16(pd + oPanelData_Unk_10, cell_timer);

                /* extract bit 2 of the decremented timer */
                u8 toggle = (cell_timer & 4) >> 2;
                final_r3  = toggle;

                u8 vis = LD8(pd + oPanelData_Unk_07);
                ST8(pd + oPanelData_Unk_07, vis ^ toggle);
            } else {
                final_r3 = alliance;
            }
        }
    }

    return final_r3;
}

/*
 * battle_updatePanels_800C746()
 * inputs:  r4 r5 r6 r7 r10 lr
 * outputs: r2 (from last callee), r3 (grid logic), r6 (preserved by pop)
 * clobbers: nz c v r0 r1 lr
 * preserved: r4 r5 r6 r7
 */
Next battle_updatePanels_800C746(void) {
    R3 = process_battle_panels();
    RETURN_TO_CALLER();
}

/* ── countMatchingRun_800C84A ── */
/* countMatchingRun_800C84A(r0 = start row, r1 = step):
 * Measures a run of identically-valued entries in a byte table whose entries
 * are 8 bytes apart. battle_getPanelEntry_800C8C2 is asked for the address of the entry lying
 * next to the start row in the direction given by step (+1/-1); the value
 * stored there is the run's value, and the run grows entry by entry for as
 * long as the row counter stays within the 1..6 range and the entries keep
 * matching it.
 * Returns r0 = run length, r1 = the value the run consists of.
 * r4-r6 are only scratch here and are restored for the caller. */
Next countMatchingRun_800C84A(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;

    u32 row = R0;   /* row the run starts from */
    u32 step = R1;  /* which way to walk along the column (1 or -1) */

    /* The original copies r0->r4 and r1->r5 before the call; the lookup
     * helper takes them as its arguments and preserves r4/r5, so the copies
     * double as both scratch state and arguments. R2/R3/R6 still hold their
     * incoming values here, as in the original. */
    R4 = row;
    R5 = step;
    GUEST_CALL(battle_getPanelEntry_800C8C2);

    u32 cell = R0;              /* address of the first entry of the run */
    u32 stride = 8u * step;     /* entries sit 8 bytes apart */
    u32 runValue = LD8(cell);   /* value every entry of the run shares */
    u32 runLength = 1;

    cell += stride;
    while ((u32)(row - 1) <= 5u && LD8(cell) == runValue) {
        runLength++;
        cell += stride;
        row += step;
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R0 = runLength;
    R1 = runValue;
    RETURN_TO_CALLER();
}

/* ── battle_getPanelEntry_800C8C2 ── */
/* battle_getPanelEntry_800C8C2(r0 = panel index): return a pointer to the panel's entry in the
 * battle panel array at unk_2034010 (each entry is 8 bytes). */
Next battle_getPanelEntry_800C8C2(void) {
    R0 = A_unk_2034010 + 8 * R0;
    RETURN_TO_CALLER();
}

/* ── object_getPanelParameters ── */
/* object_getPanelParameters(r0 = panel x, r1 = panel y):
 * Return the panel's status flags word (or 0 if the coordinates don't
 * name a valid panel). Flags c/v come from object_getPanelDataOffset,
 * nz reflects the null check, as in the original. */
Next object_getPanelParameters(void) {
    GUEST_CALL(object_getPanelDataOffset); /* -> R0 = panel data pointer, or 0 */

    SET_FLAGS_NZ(R0);
    R0 = (R0 != 0) ? LD32(R0 + oPanelData_Flags) : 0;

    RETURN_TO_CALLER();
}

/* ── object_updatePanelParameters ── */
/* object_updatePanelParameters(r0..r1, r4..r7, r10 = object/panel context):
 * Thin trampoline. The original loads the address of the real body into r2,
 * does the classic "mov lr, pc; bx r2" long-call into it, and pops PC, so the
 * observable effect of this wrapper is:
 *   - r2 = address of _object_updatePanelParameters | 1 (Thumb bit set)
 *   - a call to _object_updatePanelParameters with r0..r1, r4..r7, r10 passed
 *     straight through; its results in r0..r7 (and flags) become ours.
 * LR is clobbered by the long-call idiom, which matches the contract. */
Next object_updatePanelParameters(void) {
    /* 0800C92A: ldr r2, off_800C934 // =_object_updatePanelParameters+1 */
    R2 = (A__object_updatePanelParameters | 1);

    /* 0800C92C/0800C92E: mov lr, pc; bx r2 -- call the real body */
    GUEST_CALL(_object_updatePanelParameters);

    /* 0800C930: pop {pc} -- return to the original caller */
    RETURN_TO_CALLER();
}

/* ── object_breakPanel ── */
/* object_breakPanel(r0 = object):
 * Breaks the panel underneath the given object, if there is one and it is
 * breakable. Panels that are already broken (flag 0x10) or belong to an
 * unbreakable kind (hole, holy panel, etc. -- mask 0x0F880080) are left
 * alone. On success the panel's state counter is bumped (after clearing the
 * old state bits), its type and animation are set to "broken", the
 * panel-break sound effect plays and r0 = 1; otherwise r0 = 0.
 * Flags: only c/v are observable, and they are simply whatever
 * object_getPanelDataOffset / PlaySoundEffect left behind (the TSTs in the
 * original don't touch them). */
#define PANEL_FLAG_ALREADY_BROKEN 0x00000010u
#define PANEL_FLAGS_UNBREAKABLE   0x0F880080u
#define PANEL_FLAGS_STATE_MASK    0x00003F5Fu
/* panel data field offsets not present in game.h */
#define PANEL_OFF_TYPE       2
#define PANEL_OFF_ANIMATION  6

Next object_breakPanel(void) {
    GUEST_CALL(object_getPanelDataOffset);

    if (R0 == 0) { /* object isn't standing on a tracked panel */
        R0 = 0;
        RETURN_TO_CALLER();
    }
    u32 panel = R0;

    R1 = LD32(panel + oPanelData_Flags);

    R2 = PANEL_FLAG_ALREADY_BROKEN;
    if ((R1 & R2) == 0) { /* panel not broken yet? */
        R0 = 0;
        RETURN_TO_CALLER();
    }

    R2 = PANEL_FLAGS_UNBREAKABLE;
    if ((R1 & R2) != 0) { /* ...but it's a kind we can't break */
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* Break it: clear the old state, bump the counter, set type + animation. */
    R1 = (R1 & ~PANEL_FLAGS_STATE_MASK) + 1;
    ST32(panel + oPanelData_Flags, R1);
    R2 = 1;
    ST8(panel + PANEL_OFF_TYPE, R2);
    ST8(panel + PANEL_OFF_ANIMATION, R2);

    R0 = 0x97; /* sound effect: panel broken */
    GUEST_CALL(PlaySoundEffect);

    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── object_highlightPanel ── */
/* object_highlightPanel(r0, r1 = object):
 * If the object stands on a valid panel, fetch its panel data offset and
 * set the flag byte at offset 1 (oPanelData_Unk_01) to 1.
 * Returns r0 = panel data offset (or the validity check result when the
 * panel is invalid), r1 = 1 when the flag was written. Flags: those left
 * by the validity check when the panel is invalid, otherwise NZ from the
 * stored value 1 (as with MOV r1,#1) with C/V from getPanelDataOffset.
 * In both paths r0/r1 end up holding the original arguments, since the
 * original pops them back off the stack before branching. */
Next object_highlightPanel(void) {
    u32 object = R0;
    u32 attr = R1;

    GUEST_CALL(object_isValidPanel); /* answers in Z; the original branches on it */

    if (ZERO_FLAG()) {
        /* Not a valid panel: restore the original arguments and return. */
        R0 = object;
        R1 = attr;
        RETURN_TO_CALLER();
    }

    /* Valid panel: get its panel data and mark it as highlighted. */
    R0 = object;
    R1 = attr;
    GUEST_CALL(object_getPanelDataOffset);

    ST8(R0 + 1, 1); /* panelData->Unk_01 = 1 */
    R1 = 1;
    SET_FLAGS_NZ(1);

    RETURN_TO_CALLER();
}

/* ── object_setPanelAlliance ── */
/* object_setPanelAlliance(r0 = object, r1 = param, r2 = alliance):
 * Claim the panel the given battle object stands on for `alliance`: look up
 * the object's panel data, and if the panel is in use (panel type byte != 0),
 * store the new alliance id in it, clear its halfword field at
 * oPanelData_Unk_10, and refresh the panel's parameters.
 * Leaves r4-r7 untouched; r3 carries the panel type (early exit) or whatever
 * object_updatePanelParameters leaves in it. */
Next object_setPanelAlliance(void) {
    u32 object = R0;
    u32 param = R1;
    u32 alliance = R2;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    /* The panel helpers receive the three arguments both in r0-r2 and,
     * following this module's calling convention, in r4-r6. */
    R4 = object;
    R5 = param;
    R6 = alliance;

    GUEST_CALL(object_getPanelDataOffset);
    u32 panel = R0;

    u32 type = LD8(panel + 2); /* oPanelData_Type */
    R3 = type;
    if (type != 0) {
        ST8(panel + 3, alliance); /* oPanelData_Alliance */
        ST16(panel + oPanelData_Unk_10, 0);

        R0 = object;
        R1 = param;
        R4 = object;
        R5 = param;
        R6 = alliance;
        GUEST_CALL(object_updatePanelParameters);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── object_setPanelAllianceTimerLong ── */
/* object_setPanelAllianceTimerLong(r0 = object index):
 * Give the panel-ownership timer of the indexed object a long value
 * (store 0x708 halfword at entry+6 in the object array at 0x2034010).
 * Leaves r0 = array base, r1 = object entry address, r2 = 0x708;
 * c/v come from the ADDS that forms the entry address (MUL sets no flags). */
Next object_setPanelAllianceTimerLong(void) {
    u32 index = R0;
    u32 arrayBase = A_unk_2034010;      /* object array base (literal 0x2034010) */
    u32 offset = index * 8;             /* each object entry is 8 bytes */
    u32 entry = arrayBase + offset;

    R0 = arrayBase;
    R1 = entry;
    SET_FLAGS_ADD(offset, arrayBase);   /* add r1, r1, r0: carry/overflow out */
    R2 = 0x708;
    ST16(entry + 6, R2);

    RETURN_TO_CALLER();
}

/* ── object_isCurrentPanelValid ── */
/* object_isCurrentPanelValid(r5 = battle object):
 * checks whether the panel the object currently occupies is a valid
 * battlefield position; delegates to object_isValidPanel with the
 * object's panel coordinates.
 * outputs: r0/r1 as returned by object_isValidPanel (plus its flags). */
Next object_isCurrentPanelValid(void) {
    R0 = LD8(R5 + oBattleObject_PanelX);
    R1 = LD8(R5 + oBattleObject_PanelY);
    GUEST_CALL(object_isValidPanel);
    RETURN_TO_CALLER();
}

/* ── object_checkPanelParameters ── */
/* object_checkPanelParameters(r0, r1 = panel coordinates, r2 = flag mask that
 * must all be set on the panel, r3 = flag mask that must be clear):
 * queries the panel at the given coordinates via object_getPanelParameters and
 * returns r0 = 1 if the panel exists, has none of the "must be clear" flags,
 * and has every "must be set" flag set; otherwise returns r0 = 0.
 * r4 and r5 are preserved. The masks are moved into r4/r5 before the call, as
 * the callee expects them there.
 * Flags: the exits are `mov r0, #1/#0`, which in Thumb set N/Z from the
 * immediate (N = 0, Z = result == 0). C/V on the first two failure paths are
 * still whatever object_getPanelParameters left behind; on the third check
 * (and its failure path) they come from `cmp r0, r4`. */
Next object_checkPanelParameters(void) {
    u32 mustBeSet = R2;
    u32 mustBeClear = R3;
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    R4 = mustBeSet;
    R5 = mustBeClear;

    GUEST_CALL(object_getPanelParameters); /* r0 = panel flags at (r0, r1) */
    u32 panelFlags = R0;

    u32 result;
    if (panelFlags == 0) {
        /* Panel doesn't exist (tst r0, r0 failed). */
        result = 0;
    } else if ((panelFlags & mustBeClear) != 0) {
        /* Panel has a flag that must be clear (tst r0, r5 failed). */
        result = 0;
    } else {
        /* and r0, r4; cmp r0, r4: the panel must carry every "must be set"
         * flag. This comparison sets C/V on both the success and the failure
         * exit of this path; since (flags & mask) <= mask, carry is set
         * exactly when they are equal, i.e. on success. */
        u32 setFlags = panelFlags & mustBeSet;
        SET_FLAGS_CMP(setFlags, mustBeSet);
        result = (setFlags == mustBeSet) ? 1 : 0;
    }

    /* mov r0, #0/#1 sets N and Z from the result value itself. */
    R0 = result;
    SET_FLAGS_NZ(R0);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── object_isCurrentPanelSolid ── */
/* object_isCurrentPanelSolid (r5 = battle object):
 * check whether the panel the object currently occupies is solid
 * (hole, broken tile, off-grid, ...). Forwards the panel coordinates
 * to object_isPanelSolid as a tail call, so its result registers and
 * flags are passed straight through to the caller. */
Next object_isCurrentPanelSolid(void) {
    R0 = LD8(R5 + oBattleObject_PanelX);
    R1 = LD8(R5 + oBattleObject_PanelY);
    GUEST_CALL(object_isPanelSolid);
    RETURN_TO_CALLER();
}

/* ── object_isPanelSolid ── */
/* object_isPanelSolid(r0 = object, r1 = side):
 * Fetches the panel parameters for the object's position and tests the
 * "solid panel" flag (0x10). Returns the masked flag in r0, with NZ set
 * from it so callers can branch directly on the result. */
Next object_isPanelSolid(void) {
    GUEST_CALL(object_getPanelParameters);
    u32 solidFlag = R0 & 0x10;
    R1 = 0x10;
    R0 = solidFlag;
    SET_FLAGS_NZ(solidFlag);
    RETURN_TO_CALLER();
}

/* ── object_getPanelsIgnoreColumnFiltered ── */
/* register form: calls object_checkPanelParameters outside this file, which has no typed C interface yet */
static bool panel_param_check(s32 col, s32 row, u32 param_a, u32 param_b) {
    R0 = col; R1 = row; R2 = param_a; R3 = param_b;
    GUEST_CALL(object_checkPanelParameters);
    bool result = (R0 != 0);   /* tst r0,r0; beq pattern: skip panel when callee returns 0 */
    /* original does pop {r0-r3} after the call to keep r2/r3 alive across iterations */
    R2 = param_a; R3 = param_b;
    return result;
}

/* Scan rows 1-3 and columns 1-6, skipping the row matching ignore_row.
 * For each panel that passes object_checkPanelParameters, write a byte
 * (col | row<<4) into the output buffer at `buf`, packed sequentially.
 * Returns the number of panels written; r1 is always 4 on exit. */
static s32 scan_panels_filtered(u32 ignore_row, u32 param_a, u32 param_b, u32 buf) {
    s32 count = 0;
    for (s32 row = 1; row <= 3; row++) {
        if ((u32)row == ignore_row)
            continue;
        for (s32 col = 1; col <= 6; col++) {
            if (panel_param_check(col, row, param_a, param_b)) {
                ST8(buf + count, (u8)(col | (row << 4)));
                count++;
            }
        }
    }
    return count;
}

/* object_getPanelsIgnoreColumnFiltered(r0 = ignore_row, r2 = param_a, r3 = param_b, r7 = output buf) -> r0 = count, r1 = 4 */
Next object_getPanelsIgnoreColumnFiltered(void) {
    R0 = scan_panels_filtered(R0, R2, R3, R7);
    R1 = 4;
    RETURN_TO_CALLER();
}

/* ── object_getPanelsInColumnFiltered ── */
/* object_getPanelsInColumnFiltered(r0 = panel base id, r2/r3 = filter params,
 *                                 r7 = output buffer):
 * Scans panel columns 1..3; for each column whose panel is accepted by
 * object_checkPanelParameters, appends the encoded panel byte
 * (base | column << 4) to the output buffer.
 * Returns r0 = number of panels appended. */
Next object_getPanelsInColumnFiltered(void) {
    u32 panelBase = R0;
    u32 param0 = R2;
    u32 param1 = R3;
    u32 buffer = R7;
    u32 count = 0;

    for (u32 column = 1; column <= 3; column++) {
        R0 = panelBase;
        R1 = column;
        R2 = param0;
        R3 = param1;
        GUEST_CALL(object_checkPanelParameters);
        if (R0 != 0) {
            R0 = panelBase;
            R1 = column << 4;
            R0 |= R1;
            ST8(buffer + count, R0);
            count++;
        }
    }

    R0 = count;
    /* Final `mov r0, r6` is encoded as ADD r0, r6, #0, so it sets all flags:
     * C = 0 (no carry out), V = 0, N/Z from the count. */
    SET_FLAGS_ADD(count, 0);
    RETURN_TO_CALLER();
}

/* ── object_getPanelsInRowFiltered ── */
/* object_getPanelsInRowFiltered(r0 = row, r2/r3 = filter parameters,
 *                               r7 = output byte buffer):
 * Walks the six columns of the given row and, for each panel that
 * object_checkPanelParameters accepts, writes its encoded coordinate
 * (column | row << 4) into the buffer pointed to by r7.
 * Returns r0 = number of panels written; r4-r7 are preserved. */
Next object_getPanelsInRowFiltered(void) {
    u32 filterA = R2;
    u32 filterB = R3;
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;

    R5 = R0; /* row to scan */
    R6 = 0;  /* panels written so far */

    for (R4 = 1; (s32)R4 <= 6; R4++) {
        R0 = R4;
        R1 = R5;
        R2 = filterA;
        R3 = filterB;
        GUEST_CALL(object_checkPanelParameters);
        if (R0 != 0) {
            ST8(R7 + R6, R4 | (R5 << 4));
            R6++;
        }
    }

    R0 = R6;
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── object_getClosestPanelMatchingRowFiltered ── */
/* object_getClosestPanelMatchingRowFiltered(r0 = row):
 * Scan rows starting from a panel six steps behind this object's row, walking in the
 * direction opposite to the object's facing side. Return the first row whose panel passes
 * both object_checkPanelParameters() and object_isValidPanel(); return 0 if none qualify.
 * R1, R2, R3, R4, R5 are preserved across the call.
 *
 *   inputs:  r0 lr
 *   outputs: nz c v r0 r1 r2 r3 r4 r5 lr
 *   clobbers: -
 *   preserved: r1 r2 r3 r4 r5
 */
Next object_getClosestPanelMatchingRowFiltered(void) {
    u32 saved_row = R0;
    u32 saved_r1 = R1, saved_r2 = R2, saved_r3 = R3;

    /* bl object_getAllianceDirection -- step direction (1 toward alliance, -1 away). */
    R0 = saved_row;
    GUEST_CALL(object_getAllianceDirection);
    u32 step = 0 - R0;
    SET_FLAGS_NZ(step);

    /* r0 = -5 * saved_row + 6: starting row offset for the scan. */
    u32 row = 6 - 5 * saved_row;

    for (;;) {
        /* Restore R1..R3 in case the callee clobbered them, then call. */
        R0 = row; R1 = saved_r1; R2 = saved_r2; R3 = saved_r3;
        GUEST_CALL(object_checkPanelParameters);
        u32 params_ok = R0;
        if (params_ok) {
            /* Parameters match at this row: return the row, with R1..R3 caller-preserved. */
            R0 = row;
            R1 = saved_r1; R2 = saved_r2; R3 = saved_r3;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }

        /* row += step: move to the next row in the opposite-facing direction. */
        row = row + step;

        /* Restore R1..R3 before calling object_isValidPanel. */
        R0 = row; R1 = saved_r1; R2 = saved_r2; R3 = saved_r3;
        GUEST_CALL(object_isValidPanel);
        u32 valid = R0;
        if (valid) {
            /* Still a valid panel: re-check parameters at the new row. */
            continue;
        }

        /* Panel invalid: give up, R1..R3 caller-preserved. */
        R0 = 0;
        R1 = saved_r1; R2 = saved_r2; R3 = saved_r3;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }
}

/* ── battle_scanPanelsInFront_800D58C ── */
/* battle_scanPanelsInFront_800D58C(r5 = battle object):
 * Sweep the panels in front of the acting object, one panel at a time in
 * the direction it faces, and return the panel index (r0) where the sweep
 * stops. The sweep stops early when battle_countAdjacentPanelsOfOtherAlliance_800D5F0 reports a hit on a panel
 * for the object's alliance, or when the next panel would leave the field
 * (in which case object_isValidPanel's result is returned instead).
 * r4/r6/r7 are saved and restored by the original's push/pop, so they keep
 * their entry values; flags on return come from the final "mov r0, r6". */
Next battle_scanPanelsInFront_800D58C(void) {
    /* The original pushes/pops r4, r6 and r7: they must end unchanged. */
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    /* Start from the panel the object (r5) currently occupies. */
    GUEST_CALL(sub_800D5BA);
    u32 panel = R0;
    R6 = panel;

    /* The object's alliance decides which panels count as blocked. */
    u32 alliance = LD8(R5 + oBattleObject_Alliance);
    R7 = alliance;

    /* Step to the adjacent panel in the direction the object faces. */
    GUEST_CALL(object_getFrontDirection);
    u32 step = R0;
    R4 = step;

    for (;;) {
        /* Something to interact with on this panel: stop here. */
        R0 = panel;
        R1 = alliance;
        R6 = panel;
        GUEST_CALL(battle_countAdjacentPanelsOfOtherAlliance_800D5F0);
        if (R0 != 0)
            break;

        /* Advance one panel; running off the field ends the sweep. */
        panel += step;
        R6 = panel;
        R0 = panel;
        R1 = 1;
        GUEST_CALL(object_isValidPanel);
        if (R0 == 0) {
            panel = R0;
            break;
        }
    }

    R0 = panel;
    SET_FLAGS_NZ(R0);
    R4 = saved_r4;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── battle_countAdjacentPanelsOfOtherAlliance_800D5F0 ── */
/* battle_countAdjacentPanelsOfOtherAlliance_800D5F0(r0 = panel position, r1 = alliance id):
 * Counts the three neighbouring panels (slots 1..3) that hold an object
 * belonging to a *different* alliance; r0 = how many such panels were found.
 * (r1 ends holding the last panel's alliance byte, as in the original.) */
Next battle_countAdjacentPanelsOfOtherAlliance_800D5F0(void) {
    u32 position = R0;
    u32 alliance = R1;
    s32 count = 0;

    for (s32 slot = 1; slot <= 3; slot++) {
        R0 = position;
        R1 = slot;
        GUEST_CALL(object_getPanelDataOffset);

        u32 panel = R0;
        if (panel != 0) {
            /* oPanelData_Alliance is at offset 3 in the panel data struct. */
            R1 = LD8(panel + 3);
            if (R1 != alliance)
                count++;
        }
    }

    /* mov r0, r5 encodes as adds r0, count, #0, so it also fixes up c/v. */
    R0 = count;
    SET_FLAGS_ADD(count, 0);
    RETURN_TO_CALLER();
}

/* ── object_checkPanelTypeMatchesValue_800D618 ── */
/* object_checkPanelTypeMatchesValue_800D618(r0 = object, r1 = passed through to object_getPanelDataOffset,
 *             r2 = value the panel must match, r4 = passed through):
 * Looks up the panel data of the panel the object stands on and returns 1
 * in r0 only when the panel's byte at offset 4 differs from the panel's
 * alliance byte (offset 3) and equals r2. Returns 0 when there is no panel
 * data or when either check fails. Flags (c/v) on the "no panel data" exit
 * are whatever object_getPanelDataOffset left behind, as in the original. */
Next object_checkPanelTypeMatchesValue_800D618(void) {
    u32 savedR4 = R4;                 /* r4 is preserved for the caller */
    u32 expected = R2;

    R4 = expected;                    /* object_getPanelDataOffset expects r2 there */
    GUEST_CALL(object_getPanelDataOffset);

    if (R0 == 0) {
        /* No panel data: r0 stays 0, r1/r2 are as the callee left them. */
        R4 = savedR4;
        RETURN_TO_CALLER();
    }

    R1 = LD8(R0 + 4);                 /* oPanelData_Unk_04 */
    R2 = LD8(R0 + 3);                 /* oPanelData_Alliance */

    if (R1 == R2) {
        /* The panel byte matches the panel's alliance: not a match. */
        R0 = 0;
        SET_FLAGS_CMP(R1, R2);
    } else {
        /* Match only when the panel byte equals the requested value. */
        R0 = (R1 == expected);
        SET_FLAGS_CMP(R1, expected);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── convertBattleObjectXYToPanelXY_800E24C ── */
/* convertBattleObjectXYToPanelXY_800E24C(r5 = battle object):
 * wrapper that reads the object's pixel-space X/Y and converts them to
 * panel coordinates. Returns the panel X/Y in r0/r1 (via the callee). */
Next convertBattleObjectXYToPanelXY_800E24C(void) {
    R0 = LD32(R5 + oBattleObject_X);
    R1 = LD32(R5 + oBattleObject_Y);
    GUEST_CALL(convertXYToPanelXY_800E258);
    RETURN_TO_CALLER();
}

/* ── convertXYToPanelXY_800E258 ── */
/* convertXYToPanelXY_800E258(r0 = X as 16.16 fixed point, r1 = Y as 16.16 fixed point):
 * convert a screen-space position into battle-field panel coordinates.
 * The field's origin sits 160px right / 32px down in screen space and each
 * panel is 40px wide by 24px tall, so panel = (whole-pixel coord + origin) / size.
 * Both divisions are done with the BIOS Divide routine (swi 6), which returns
 * the quotient in r0 and the remainder in r1; only the quotient is used.
 * Returns: r0 = panel column, r1 = panel row.
 * Flag notes: the trailing `mov r1, r0` is really an encoded `lsls r1, r0, #0`,
 * so it refreshes only N/Z (from the panel row). The BIOS Divide is the last
 * instruction that touches C/V, and it leaves both clear, which is the same
 * flag pattern an ADDS-of-zero produces, so we mirror that here. */
Next convertXYToPanelXY_800E258(void) {
    /* Keep only the whole-pixel part of each fixed-point coordinate. */
    s32 screenX = (s32)R0 >> 16;
    s32 screenY = (s32)R1 >> 16;

    /* Panel column = (screenX + 160) / 40 */
    R0 = (u32)(screenX + 0xA0);
    R1 = 0x28;
    swi_call(6);                    /* BIOS Div: r0 = quotient, r1 = remainder */
    u32 panelX = R0;

    /* Panel row = (screenY + 32) / 24 */
    R0 = (u32)(screenY + 0x20);
    R1 = 0x18;
    swi_call(6);

    /* r1 = panel row, with N/Z taken from it and C/V left clear. */
    R1 = R0;
    SET_FLAGS_ADD(R0, 0);

    R0 = panelX;                    /* panel column, saved across the second divide */

    RETURN_TO_CALLER();
}

/* ── object_getCoordinatesForPanels ── */
/* object_getCoordinatesForPanels(r0 = panel column, r1 = panel row):
 * Convert a battle-field panel position into on-screen pixel coordinates as
 * 16.16 fixed point: r0 = x, r1 = y. The panel grid is 40x24 pixels per cell,
 * and the field origin sits 140 px left / 20 px up (the 0xFF740000/0xFFEC0000
 * offsets are -0x8C and -0x14 carried in the high halfword).
 * Flags reflect the final ADDS used for the y coordinate. */
Next object_getCoordinatesForPanels(void) {
    s32 col = (s8)R0;
    s32 row = (s8)R1;

    u32 x = (u32)(col * 40) << 16; /* 40 pixels per panel column */
    u32 y = (u32)(row * 24) << 16; /* 24 pixels per panel row   */

    R0 = x + 0xFF740000u;

    R2 = 0xFFEC0000u;
    R1 = y + R2;
    SET_FLAGS_ADD(y, R2);

    RETURN_TO_CALLER();
}

/* ── object_setCoordinatesFromPanels ── */
/* object_setCoordinatesFromPanels(r5 = battle object):
 * Read the object's panel position, convert it to pixel coordinates and
 * store the result back into the object's X/Y fields.
 * Returns the pixel coordinates in r0 (X) and r1 (Y); flags and r2 are
 * whatever object_getCoordinatesForPanels left behind. */
Next object_setCoordinatesFromPanels(void) {
    u32 object = R5;

    R0 = LD8(object + oBattleObject_PanelX);
    R1 = LD8(object + oBattleObject_PanelY);
    GUEST_CALL(object_getCoordinatesForPanels);

    ST32(object + oBattleObject_X, R0);
    ST32(object + oBattleObject_Y, R1);

    RETURN_TO_CALLER();
}

/* ── object_setPanelsFromCoordinates ── */
/* object_setPanelsFromCoordinates(r5 = battle object):
 * Convert the object's world position (X/Y/Z) into battle panel coordinates
 * and store them in its PanelX/PanelY fields. R0/R1 return the panel
 * coordinates, R2 returns unchanged from the helper; flags are whatever
 * convertXYToPanelXY_800E258 leaves behind. */
Next object_setPanelsFromCoordinates(void) {
    u32 object = R5;

    R0 = LD32(object + oBattleObject_X);
    R1 = LD32(object + oBattleObject_Y);
    R2 = LD32(object + oBattleObject_Z);

    GUEST_CALL(convertXYToPanelXY_800E258);

    ST8(object + oBattleObject_PanelX, R0);
    ST8(object + oBattleObject_PanelY, R1);

    RETURN_TO_CALLER();
}

/* ── object_getEnemyDirection ── */
/* object_getEnemyDirection(r5 = battle object):
 * Returns the object's facing step in R0: +1 for alliance 0 (player side),
 * -1 for alliance 1 (enemy side), as a two's-complement u32. Flags are left
 * as the final NEG (rsbs) of object_getAllianceDirection sets them. */
Next object_getEnemyDirection(void) {
    u32 alliance = LD8(R5 + oBattleObject_Alliance);

    /* body of object_getAllianceDirection: lsl r0,#1; sub r0,#1; neg r0,r0 */
    u32 step = 2 * alliance - 1;
    SET_FLAGS_CMP(0, step);
    R0 = 0 - step;

    RETURN_TO_CALLER();
}

/* ── object_getAllianceDirection ── */
/* object_getAllianceDirection(r0 = alliance id):
 * Maps an alliance id to a facing multiplier: 0 (player side) -> +1,
 * 1 (enemy side) -> -1, i.e. 1 - 2*alliance. The final NEG (rsbs) is the
 * last flag-setting instruction, so N/Z/C/V must come from 0 - flipped. */
Next object_getAllianceDirection(void) {
    u32 doubled = R0 << 1;          /* lsl r0, r0, #1 */
    u32 flipped = doubled - 1;      /* sub r0, #1   -> 2*alliance - 1 */
    SET_FLAGS_CMP(0, flipped);      /* neg r0, r0 = rsbs r0, r0, #0: flags of 0 - flipped */
    R0 = 0 - flipped;               /* ...and the negated result itself */
    RETURN_TO_CALLER();
}

/* ── object_getFrontDirection ── */
/* object_getFrontDirection(r5 = battle object):
 * Loads the object's alliance side and facing flip, then tail-calls
 * object_getFlipDirection(alliance, flip) to get the front-facing direction
 * multiplier (+1 when the object faces toward the enemy side it belongs to,
 * -1 otherwise). r0 = result, r1 = the DirectionFlip byte as loaded. */
Next object_getFrontDirection(void) {
    R0 = LD8(R5 + oBattleObject_Alliance);
    R1 = LD8(R5 + oBattleObject_DirectionFlip);

    /* Tail call: object_getFlipDirection returns straight to our caller. */
    GUEST_CALL(object_getFlipDirection);

    RETURN_TO_CALLER();
}

/* ── object_getFlipDirection ── */
/* object_getFlipDirection(r0 = direction A, r1 = direction B):
 * returns -1 if the two direction flags differ (sprite must be flipped
 * horizontally) or +1 if they match. r0 = result; flags come from the
 * final NEG (RSBS 0, r0) in the original, i.e. from 0 - preNeg value. */
Next object_getFlipDirection(void) {
    u32 differ = (R0 ^ R1) << 1;    /* 2 if directions differ, 0 if equal */
    u32 preNeg = differ - 1;        /* 1 if differing, -1 (0xFFFFFFFF) if equal */

    R0 = 0 - preNeg;                /* -1 or 1 */
    SET_FLAGS_CMP(0, preNeg);       /* NEG sets NZCV on the pre-neg value */
    RETURN_TO_CALLER();
}

/* ── object_subtractHP ── */
/* object_subtractHP(r0 = damage, r5 = battle object):
 * subtract r0 from the object's HP, clamping at zero.
 * Leaves r1 = resulting HP; flags c/v reflect the subtraction. */
Next object_subtractHP(void) {
    u32 oldHP = LD16(R5 + oBattleObject_HP);
    u32 damage = R0;

    u32 newHP = oldHP - damage;
    SET_FLAGS_CMP(oldHP, damage); /* SUBS r1, r1, r0: c = no borrow, v = signed overflow */

    if ((s32)oldHP < (s32)damage) /* bge not taken: HP would go negative */
        newHP = 0;

    ST16(R5 + oBattleObject_HP, newHP);
    R1 = newHP;
    RETURN_TO_CALLER();
}

/* ── object_addHP ── */
Next object_addHP(void) {
    u32 new_hp = (u32)LD16(R5 + oBattleObject_HP) + R0;
    u32 max_hp = (u32)LD16(R5 + oBattleObject_MaxHP);
    SET_FLAGS_CMP(new_hp, max_hp);
    if ((s32)new_hp > (s32)max_hp)
        new_hp = max_hp;
    ST16(R5 + oBattleObject_HP, (u16)new_hp);
    R1 = new_hp;
    RETURN_TO_CALLER();
}

/* ── calcFinalDamageAsPanelDamage1to5SumReturnFinalDamage_r0_PanelDamage6_r1_800E3BE ── */
/* calcFinalDamageAsPanelDamage1to5SumReturnFinalDamage_800E3BE(r4 = preserved, r5 = battle object):
 * Sums the five PanelDamage entries in the object's collision data, writes the sum to
 * FinalDamage, and returns:
 *   r0 = final damage (the PanelDamage1..5 sum)
 *   r1 = PanelDamage6
 *   r3 = the collision data pointer
 */
Next calcFinalDamageAsPanelDamage1to5SumReturnFinalDamage_r0_PanelDamage6_r1_800E3BE(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 panelDamage1 = collision + oCollisionData_PanelDamage1;

    u32 sum = 0;
    for (u32 i = 0; i < 5; i++)
        sum += LD16(panelDamage1 + 2 * i);

    ST16(collision + oCollisionData_FinalDamage, sum);

    R0 = sum;
    R1 = LD16(collision + oCollisionData_PanelDamage6);
    R3 = collision;
    RETURN_TO_CALLER();
}

/* ── object_calculateFinalDamage1 ── */
/* register form: calls object_getPanelDataOffset outside this file, which has no typed C interface yet */
static u32 get_panel_data_offset(u32 panel_x, u32 panel_y) {
    R0 = panel_x;
    R1 = panel_y;
    GUEST_CALL(object_getPanelDataOffset);
    return R0;
}

/* register form: calls sub_802CE10 outside this file, which has no typed C interface yet */
static u32 clamp_final_damage(u32 damage) {
    R0 = damage;
    GUEST_CALL(sub_802CE10);
    return R0;
}

/* Sum the object's five panel-damage entries into its final damage.
 * On holy panels (panel type 5) each entry is halved, rounding up
 * ((damage + 1) >> 1); on normal panels the entries are summed as-is.
 * The total is passed through sub_802CE10 (damage caps etc.) and stored
 * into oCollisionData_FinalDamage.
 * Returns the CollisionDataPtr so the adapter can reproduce the flags of
 * the original's final `add r3, #oCollisionData_FinalDamage`.
 * Leaves r2 as sub_802CE10 left it (callers read it from there). */
static u32 total_panel_damage(u32 object_ptr) {
    u32 collision = LD32(object_ptr + oBattleObject_CollisionDataPtr);

    u32 panel = get_panel_data_offset(LD8(object_ptr + oBattleObject_PanelX),
                                      LD8(object_ptr + oBattleObject_PanelY));

    u32 holy_shift = 0;
    if (LD8(panel + oPanelData_Type) == 5)      /* holy panel? */
        holy_shift = 1;

    /* (1 << shift) - 1: the +1 added before halving so it rounds up */
    u32 holy_bonus = (1u << holy_shift) - 1u;

    u32 total = 0;
    u32 entry = collision + oCollisionData_PanelDamage1;
    for (int i = 0; i < 5; i++) {
        u32 dmg = (LD16(entry) + holy_bonus) >> holy_shift;
        ST16(entry, dmg);
        total += dmg;
        entry += 2;
    }

    total = clamp_final_damage(total);

    u32 final_ptr = collision + oCollisionData_FinalDamage;
    ST16(final_ptr, total);
    return collision;
}

/* object_calculateFinalDamage1(r4 = scratch in, r5 = object, r6 = scratch in, lr)
 * -> r4/r6 restored, r2 as left by sub_802CE10, c/v from collisionPtr + oCollisionData_FinalDamage */
Next object_calculateFinalDamage1(void) {
    u32 collision = total_panel_damage(R5);
    SET_FLAGS_ADD(collision, oCollisionData_FinalDamage);
    RETURN_TO_CALLER();
}

/* ── object_getFlip ── */
/* object_getFlip(r5 = battle object):
 * Returns the object's horizontal orientation: its alliance bit (player/enemy
 * side) XORed with its DirectionFlip override. r0 receives the combined flip
 * flag, r1 the raw DirectionFlip byte (both are live outputs for callers). */
Next object_getFlip(void) {
    u32 flip = LD8(R5 + oBattleObject_DirectionFlip);
    R0 = LD8(R5 + oBattleObject_Alliance) ^ flip;
    R1 = flip;
    RETURN_TO_CALLER();
}

/* ── sub_800E45E ── */
/* sub_800E45E: tail-call wrapper that invokes battle_objectResolveContactOffset_800E468 with r0 = 1.
 * All other registers (and the flags left behind by the callee) pass through. */
Next sub_800E45E(void) {
    R0 = 1;
    GUEST_CALL(battle_objectResolveContactOffset_800E468);
    RETURN_TO_CALLER();
}

/* ── battle_objectResolveContactOffset_800E468 ── */
typedef struct { u32 x, y, extra; } ContactOffset;

/* register form: calls battle_dispatchObjectContactHandler_800E498 outside this file, which has no typed C interface yet */
static ContactOffset dispatch_contact_handler(u8 contact_type) {
    R0 = contact_type;
    GUEST_CALL(battle_dispatchObjectContactHandler_800E498);
    return (ContactOffset){R0, R1, R2};
}

/* register form: calls battle_objectCheckPanelByAlliance_800E5AC outside this file, which has no typed C interface yet */
static bool panel_check_alliance(u32 x, u32 y, u32 panel_y) {
    R0 = x;
    R1 = y;
    R3 = panel_y;
    GUEST_CALL(battle_objectCheckPanelByAlliance_800E5AC);
    return !ZERO_FLAG();  /* non-zero → panel is valid for this alliance */
}

/* Compute the contact offset for a battle object.
 * The raw offset from the contact handler is kept only when the target
 * panel (object position + offset) belongs to the object's alliance;
 * otherwise the result is (0, 0, 0). */
static ContactOffset get_contact_offset(u32 obj) {
    u8 type = LD8(obj + oBattleObject_Unk_0f);
    ContactOffset raw = dispatch_contact_handler(type);

    u32 panel_y = LD8(obj + oBattleObject_PanelY);
    u32 check_x = raw.x + LD8(obj + oBattleObject_PanelX);
    u32 check_y = raw.y + panel_y;

    if (!panel_check_alliance(check_x, check_y, panel_y))
        return (ContactOffset){0, 0, 0};

    return raw;
}

/* battle_objectResolveContactOffset_800E468(r5 = battle object ptr) -> r0,r1,r2 = contact offset; c,v,r3 from panel check callee */
Next battle_objectResolveContactOffset_800E468(void) {
    ContactOffset result = get_contact_offset(R5);
    R0 = result.x;
    R1 = result.y;
    R2 = result.extra;
    RETURN_TO_CALLER();
}

/* ── battle_dispatchObjectContactHandler_800E498 ── */
/* battle_dispatchObjectContactHandler_800E498(r0 = handler index, r5 = battle object, r2/r3 = passed through):
 * Dispatches to one of the battle object's contact handlers selected by r0,
 * passing the object's collision direction in r0. The handler's results (and
 * every register it leaves behind) are handed straight back to our caller. */
Next battle_dispatchObjectContactHandler_800E498(void) {
    u32 index = R0;

    /* The handlers receive the direction field of the object's collision data. */
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R0 = LD8(collision + oCollisionData_Direction);

    /* Handler table (off_800E4B0), in table order: */
    switch (index) {
    case 0: GUEST_CALL(sub_800E4C0); break;
    case 1: GUEST_CALL(battle_getHitModifierOffsets_800E548); break;
    case 2: GUEST_CALL(sub_800E4C8); break;
    case 3: GUEST_CALL(sub_800E500); break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_getHitModifierOffsets_800E548 ── */
/* register form: calls object_getEnemyDirection outside this file, which has no typed C interface yet */
static u32 get_enemy_direction(u32 object_ptr, u32 r4_in, u32 r6_in) {
    R4 = r4_in;
    R5 = object_ptr;
    R6 = r6_in;
    GUEST_CALL(object_getEnemyDirection);
    return R0;
}

/* Table at 0x0800E58C: 3-byte entries indexed by hit-modifier class.
 * Entry[0] = direction multiplier (s8), Entry[1] = X offset (s8), Entry[2] = Y offset (u8). */
#define HIT_MODIFIER_TABLE 0x0800E58Cu

typedef struct {
    u32 result;        /* r0 = direction * multiplier (s32 product) */
    u32 offset_x;      /* r1 = entry[1] sign-extended */
    u32 offset_y;      /* r2 = entry[2] zero-extended */
    u32 collision_ptr; /* r3 = collision data pointer (unchanged by later code) */
} HitModifierResult;

/* battle_getHitModifierOffsets_800E548: compute knockback from enemy direction and hit modifier. */
static HitModifierResult compute_hit_offsets(u32 object_ptr, u32 r4_in, u32 r6_in) {
    u32 direction = get_enemy_direction(object_ptr, r4_in, r6_in);

    u32 collision_ptr = LD32(object_ptr + oBattleObject_CollisionDataPtr);
    u8 modifier = LD8(collision_ptr + oCollisionData_HitModifierFinal);

    /* Bit 7 marks a "final" hit variant; index shifts by 5. */
    u32 index_base = 0;
    if (modifier & 0x80) {
        modifier &= (u8)~0x80;
        index_base = 5;
    }

    /* Count trailing zeros of (modifier >> 2), capped at 4. */
    u32 value = (u32)modifier >> 2;
    u32 shift_count = 0;
    while (true) {
        bool carry_out = (value & 1) != 0;
        value >>= 1;
        if (carry_out) break;
        shift_count++;
        if (shift_count >= 4) break;
    }

    u32 entry_addr = HIT_MODIFIER_TABLE + (shift_count + index_base) * 3;

    s8 multiplier = (s8)LD8(entry_addr);
    s8 offset_x   = (s8)LD8(entry_addr + 1);
    u8 offset_y   = LD8(entry_addr + 2);

    HitModifierResult res;
    res.result        = (u32)((s32)direction * (s32)multiplier);
    res.offset_x      = (u32)(s32)offset_x;
    res.offset_y      = (u32)offset_y;
    res.collision_ptr = collision_ptr;
    return res;
}

/* battle_getHitModifierOffsets_800E548(r5 = battle object) -> r0..r3, flags */
Next battle_getHitModifierOffsets_800E548(void) {
    HitModifierResult e = compute_hit_offsets(R5, R4, R6);
    R0 = e.result;
    R1 = e.offset_x;
    R2 = e.offset_y;
    R3 = e.collision_ptr;
    SET_FLAGS_ADD(1, 0); /* MOV r1,#1 after MUL: N=0 Z=0 C=0 V=0 */
    RETURN_TO_CALLER();
}

/* ── battle_objectCheckPanelByAlliance_800E5AC ── */
/* battle_objectCheckPanelByAlliance_800E5AC(r0, r1 = panel parameters, r5 = battle object):
 *
 * Gate for object/panel interaction. First object_isValidPanel decides
 * whether the panel identified by the two parameters exists at all; it
 * reports "invalid" by returning zero in r0 (and setting Z to match, which
 * is why the original can branch straight after the call). For an invalid
 * panel the whole call returns 0 in r0.
 *
 * Otherwise the panel flag is fetched with object_getFlag. Bit 4 of that
 * flag picks which half of the per-alliance table at byte_800E660 to use,
 * and the object's alliance picks the 8-byte row inside it. The two words
 * stored there are passed, together with the original two parameters, to
 * object_checkPanelParameters, whose r1-r3 results and flags are returned
 * to the caller unchanged.
 *
 * r4-r7 are preserved; r1-r3 are untouched when the panel is invalid and
 * come from object_checkPanelParameters otherwise.
 */
Next battle_objectCheckPanelByAlliance_800E5AC(void) {
    u32 paramA = R0;
    u32 paramB = R1;

    /* isValidPanel and getFlag both see the original r0/r1 (isValidPanel
     * may adjust them and getFlag consumes the result), so R0/R1 are left
     * alone until both calls are done. */
    GUEST_CALL(object_isValidPanel); /* answers in Z; the original branches on it */
    if (ZERO_FLAG()) {                  /* invalid panel */
        R0 = 0;
        SET_FLAGS_NZ(R0);           /* mov r0,#0: Z set, C/V untouched */
        RETURN_TO_CALLER();
    }

    GUEST_CALL(object_getFlag);
    u32 flag = R0;

    u32 entry = (flag & 0x10)       /* bit 4 of the panel flag: table half */
              + ((u32)LD8(R5 + oBattleObject_Alliance) << 3);

    R2 = LD32(A_byte_800E660 + entry);
    R3 = LD32(A_byte_800E660 + entry + 4);
    R0 = paramA;
    R1 = paramB;
    GUEST_CALL(object_checkPanelParameters);

    /* r1-r3 and the flags pass through from object_checkPanelParameters. */
    RETURN_TO_CALLER();
}

/* ── object_canMove ── */
/* object_canMove (0800E5E2): asks whether the current object is allowed to move.
 *
 * Calls object_getFlag (which reads the object pointer from r4) and tests the
 * returned flag word against the "action blocked" mask 0x5040. Returns
 * r0 = 1 when nothing blocks the object, r0 = 0 when one of the blocking bits
 * is set. r4 is pushed/popped around the call, so it comes out unchanged;
 * r1 is left holding the mask, and the NZ flags reflect the returned boolean. */
Next object_canMove(void) {
    /* push {r4,lr}: the object pointer in r4 must survive the call */
    u32 savedR4 = R4;

    /* r0 = the object's flag word */
    GUEST_CALL(object_getFlag);

    /* mov r4, r0 assembles to the flag-setting adds r4, r0, #0 form
     * (clears C and V); the TST that follows doesn't touch them. */
    SET_FLAGS_ADD(R0, 0);
    R4 = R0;

    /* flag bits that forbid the object from moving/acting */
    R1 = 0x5040;
    u32 blocked = R4 & R1;

    /* pop {r4,pc}: restores the caller's r4 */
    R4 = savedR4;

    if (blocked != 0) {
        R0 = 0; /* a blocking flag is set: the object cannot move */
    } else {
        R0 = 1; /* nothing blocks it: free to move */
    }
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── playerObjectMovingToPanelValidityRelated_800E618 ── */
/*
 * Player-object "may move onto this panel?" pre-check.
 *
 * (r0, r1) describe the panel being moved onto, r5 is the moving object.
 * The real verdict comes from object_checkPanelParameters; this function only
 * picks which of the four parameter pairs in the table at byte_800E660 to pass
 * it: the pair is selected by the object's alliance, offset by 0x10 when the
 * object carries flag bit 0x10 or is not standing on a solid panel.
 * Every observable output (r0-r3, nz/c/v, lr) is simply what the last callee
 * left behind, so nothing is computed or rewritten here on the way out.
 */

/* {word0, word1} pairs passed to object_checkPanelParameters, indexed by
 * alliance*8 + (0 or 0x10); the table lives in the original ROM image. */
#define PANEL_PARAM_TABLE A_byte_800E660

/* register form: calls object_isValidPanel outside this file, which has no typed C interface yet */
static bool target_panel_is_valid(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(object_isValidPanel);
    return !ZERO_FLAG();          /* Z set when the panel is not valid */
}

/* register form: calls object_getFlag outside this file, which has no typed C interface yet */
static u32 object_get_flag(void) {
    GUEST_CALL(object_getFlag);
    return R0;
}

/* register form: calls object_isPanelSolid outside this file, which has no typed C interface yet */
static bool panel_is_solid(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(object_isPanelSolid);
    return !ZERO_FLAG();          /* Z set when the panel is not solid */
}

/* register form: calls object_checkPanelParameters outside this file, which has no typed C interface yet */
static void check_panel_parameters(u32 x, u32 y, u32 param_a, u32 param_b) {
    R0 = x;
    R1 = y;
    R2 = param_a;
    R3 = param_b;
    GUEST_CALL(object_checkPanelParameters);
    /* leaves r0-r3 and nz/c/v exactly as object_checkPanelParameters set them */
}

static void player_object_moving_to_panel_check(u32 x, u32 y, u32 obj) {
    if (!target_panel_is_valid(x, y))
        return;                   /* early out: flags (Z set) and r1-r3 stay as object_isValidPanel left them */

    /* Table base offset: 0x10 unless the object has flag bit 0x10 clear and
     * already stands on a solid panel, in which case the first pair is used. */
    u32 base = 0x10;
    if ((object_get_flag() & 0x10) == 0 &&
        panel_is_solid(LD8(obj + oBattleObject_PanelX), LD8(obj + oBattleObject_PanelY)))
        base = 0;

    u32 entry = PANEL_PARAM_TABLE + base + ((u32)LD8(obj + oBattleObject_Alliance) << 3);
    check_panel_parameters(x, y, LD32(entry), LD32(entry + 4));
}

Next playerObjectMovingToPanelValidityRelated_800E618(void) {
    player_object_moving_to_panel_check(R0, R1, R5);
    RETURN_TO_CALLER();
}

/* ── IsR2BetweenR0AndR1 ── */
/* IsR2BetweenR0AndR1: 0800E6E8 THUMB
 * Returns whether r2 lies in (min(r0,r1), max(r0,r1)] — note the lower
 * bound is exclusive, the upper bound inclusive. Result is left in r0
 * and also copied to r3. */
Next IsR2BetweenR0AndR1(void) {
    s32 a = (s32)R0;
    s32 b = (s32)R1;
    s32 value = (s32)R2;
    s32 result;

    if (a <= b) {
        /* r0..r1 given in ascending order. */
        result = (value > a && value <= b);
    } else {
        /* Range was given reversed: r1 is the lower bound. */
        result = (value > b && value <= a);
    }

    R3 = (u32)result;
    R0 = (u32)result;
    RETURN_TO_CALLER();
}

/* ── object_clearInvulnerableTime ── */
/* object_clearInvulnerableTime(r5 = battle object):
 * Reset the object's post-hit invulnerability counter in its collision data
 * and clear the invulnerability flag (bit 8) via object_clearFlag.
 * Leaves r1/r2 as object_clearFlag left them (callers expect that). */
Next object_clearInvulnerableTime(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collisionData + oCollisionData_Unk_28, 0);

    R0 = 8;
    GUEST_CALL(object_clearFlag);

    RETURN_TO_CALLER();
}

/* ── battle_applyFlattenHitStatus_800EB26 ── */
/* battle_applyFlattenHitStatus_800EB26(r5 = battle object):
 * Called after collision processing. If the object took a "flattening" hit
 * (flag 0x40 in its collision flags), check its final status effect:
 * statuses 0x60..0x65 are left untouched, anything else is reset to 0x12
 * and the object gets flag 0x4000 set / flag 0x6 cleared on its flag2 word.
 * Leaves r3 = collision data pointer. */
Next battle_applyFlattenHitStatus_800EB26(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    u32 hitFlags = LD32(collision + oCollisionData_FlagsFromCollision);
    SET_FLAGS_NZ(hitFlags & 0x40);

    if (hitFlags & 0x40) {
        u32 status = LD8(collision + oCollisionData_StatusEffectFinal);

        /* Statuses 0x60..0x65 are preserved as-is; only flag state matters. */
        if ((s32)status >= 0x60 && (s32)status <= 0x65) {
            SET_FLAGS_CMP(status, 0x65);
            RETURN_TO_CALLER();
        }

        /* Any other status is reset and the object is flagged accordingly. */
        ST8(collision + oCollisionData_StatusEffectFinal, 0x12);

        R0 = 0x4000;
        GUEST_CALL(object_setFlag2);

        R0 = 6;
        GUEST_CALL(object_clearFlag2);
    }
    RETURN_TO_CALLER();
}

/* ── object_spawnHiteffect ── */
/* object_spawnHiteffect(r5 = battle object that just took a hit):
 * When a collision lands on this object, react by playing the hit sound
 * and spawning the hit-spark effect over the object. Does nothing while
 * the battle is paused, or when the collision flags don't carry the
 * "spawn hit effect" bit. r5 (and r6/r7/r10) are left untouched. */
Next object_spawnHiteffect(void) {
    u32 savedR4 = R4;

    /* don't spawn anything while the battle is paused */
    GUEST_CALL(battle_isPaused);
    if (R0 != 0)
        RETURN_TO_CALLER();

    /* check the collision flags for the "show hit effect" bit */
    R0 = LD32(LD32(R5 + oBattleObject_CollisionDataPtr) + oCollisionData_FlagsFromCollision);
    R1 = 0x20000;
    u32 hitFlag = R0 & R1;
    SET_FLAGS_NZ(hitFlag);
    if (hitFlag == 0)
        RETURN_TO_CALLER();

    /* SFX: object took a hit */
    R0 = 0x6E;
    GUEST_CALL(PlaySoundEffect);

    /* spawn the hit-spark effect at the object's position (three
     * fixed-point words at +0x34). Lift the last coordinate by 16.0 so
     * the spark appears above the object, then let the helper jitter
     * two of the coordinates by up to 15 units. */
    u32 pos = R5 + 0x34;
    R1 = LD32(pos);
    R2 = LD32(pos + 4);
    R3 = LD32(pos + 8) + 0x100000; /* 0x100000 = 16.0 fixed point */
    R0 = 15;
    GUEST_CALL(AddRandomVarianceToTwoCoords);

    /* request effect type 8 (hit spark) */
    R4 = 8;
    GUEST_CALL(sub_80E08C4);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── selectVirusOrNaviNamesAndWhich_800EC56 ── */
/* selectVirusOrNaviNamesAndWhich_800EC56(r0 = battle chip ID):
 * Pick the text script holding the chip-name strings for the given chip ID:
 * IDs above 0xFF are navi chips, everything else is a regular virus chip.
 * Outputs: r0 = TextScript address (Virus or Navi chip name list),
 *          r1 = chip ID truncated to its low byte. */
Next selectVirusOrNaviNamesAndWhich_800EC56(void) {
    u32 chipId = R0;

    u32 nameScript = A_TextScriptVirusChipNames;
    if ((s32)chipId > 0xFF)
        nameScript = A_TextScriptNaviChipNames;

    R0 = nameScript;
    R1 = chipId & 0xFF;
    RETURN_TO_CALLER();
}

/* ── sub_800EC80 ── */
/* register form: calls stub_returnZero_8000E30 outside this file, which has no typed C interface yet */
static s32 stub_returnZero(void) {
    GUEST_CALL(stub_returnZero_8000E30);
    return (s32)R0;
}

/* register form: calls getCurNaviStatTier_8014150 outside this file, which has no typed C interface yet */
static s32 cur_navi_stat_tier(void) {
    GUEST_CALL(getCurNaviStatTier_8014150);
    return (s32)R0;
}

/* register form: calls GetVerActorTyAndAIIdx_80182B4 outside this file, which has no typed C interface yet */
static u32 enemy_version_and_actor_type(u32 enemy_idx) {
    R0 = enemy_idx;
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);
    return R0;
}

/* Adjust an enemy chip index based on the current navi stat tier, capped by the
 * enemy's version for enemies whose actor type is 0 (bosses). Other actor types
 * pass the index through unchanged. (The original binary also contains a dead
 * actor-type-1 branch capping with 2 - version; it is unreachable and omitted.) */
static u32 adjust_enemy_index(u32 enemy_idx) {
    s32 bonus = stub_returnZero();      /* floor value; the stub always yields 0 */
    s32 tier = cur_navi_stat_tier();
    if (tier > bonus)
        bonus = tier;

    u32 info = enemy_version_and_actor_type(enemy_idx);
    u32 actor_type = LD8(info + 1);
    if (actor_type != 0)
        return enemy_idx;

    s32 version_cap = 5 - (s32)LD8(info);   /* higher version -> smaller cap */
    if (version_cap <= 0)
        return enemy_idx;

    s32 doubled = bonus + bonus;
    if (doubled > version_cap)
        doubled = version_cap;
    return enemy_idx + (u32)doubled;
}

/* sub_800EC80(r0 = enemy_idx) -> r0 (r4/r5/r6/r7 preserved) */
Next sub_800EC80(void) {
    R0 = adjust_enemy_index(R0);
    RETURN_TO_CALLER();
}

/* ── ai_resetAIDataSlots_800ED00 ── */
/* ai_resetAIDataSlots_800ED00: reset the AI actor pool.
 *
 * For each of the eight AI slots, store a one-hot mask word at offset 0x7C:
 * slot i gets 0x80000000 >> i. Then clear the "AI data in use" bitfield.
 *
 * Register side effects (from the final loop iteration and tail):
 *   r0 = 0, r1 = &eUsedAIDataBitfield, r2 = 8, r3 = 0x100 (slot stride),
 *   flags from the loop's last "cmp r2, #8" (equal: Z=1, C=1, V=0). */
Next ai_resetAIDataSlots_800ED00(void) {
    u32 ai = A_eAIData;   /* base of eAIData, one slot per AI actor */
    u32 mask = 0x80000000;
    u32 slot = 0;

    do {
        ST32(ai + 0x7C, mask);
        ai += 0x100;                 /* stride between AI slots */
        mask >>= 1;
        slot++;
    } while (slot < 8);

    ST32(A_eUsedAIDataBitfield, 0);

    R0 = 0;
    R1 = A_eUsedAIDataBitfield;
    R2 = slot;                       /* 8 */
    R3 = 0x100;
    SET_FLAGS_CMP(slot, 8);          /* last "cmp r2, #8": equal, so Z=1, C=1, V=0 */

    RETURN_TO_CALLER();
}

/* ── object_createAIData ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word_object_createAIData(u32 addr, u32 num_bytes) {
    R0 = addr;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

#define AI_DATA_SLOT_COUNT  8
#define AI_DATA_SLOT_STRIDE 0x100 /* oAIData_Size: AIData + AIState + AIAttackVars */

/* What the original leaves in the registers its callers look at. */
struct AIDataAlloc {
    u32 block;        /* r0: the newly claimed AIData block, or NULL if all slots are taken */
    u32 usedBits;     /* r2: eUsedAIDataBitfield value at return */
    u32 usedBitsAddr; /* r3: address of eUsedAIDataBitfield */
    u32 r5;           /* r5: pass-through (the original pops it unchanged) */
};

/* Allocate the first free AI data slot for a battle object. The 8 slots are
 * tracked in the top byte of eUsedAIDataBitfield, bit 31 = slot 0, and live
 * back to back starting at eAIData. On success the slot's bit is set and its
 * AIData/AIState/AIAttackVars fields are cleared. */
static struct AIDataAlloc create_ai_data(u32 r5_in) {
    u32 usedBitsAddr = A_eUsedAIDataBitfield;
    u32 usedBits = LD32(usedBitsAddr);
    u32 mask = 0x80000000;          /* slot 0 lives at bit 31 */
    u32 block = A_eAIData;

    for (u32 slot = 0; slot < AI_DATA_SLOT_COUNT; slot++) {
        if ((usedBits & mask) == 0) {
            usedBits |= mask;       /* mark the slot as used */
            ST32(usedBitsAddr, usedBits);

            zero_fill_by_word_object_createAIData(block, 0x7C);         /* AIData */
            zero_fill_by_word_object_createAIData(block + 0x80, 0x20);  /* AIState */
            zero_fill_by_word_object_createAIData(block + 0xA0, 0x50);  /* AIAttackVars */

            struct AIDataAlloc ok = { block, usedBits, usedBitsAddr, r5_in };
            return ok;
        }
        block += AI_DATA_SLOT_STRIDE;
        mask >>= 1;
    }

    /* No free slot: return NULL, bitfield left untouched. */
    struct AIDataAlloc full = { 0, usedBits, usedBitsAddr, r5_in };
    return full;
}

/* object_createAIData: () -> Nullable<* AIData> */
Next object_createAIData(void) {
    struct AIDataAlloc res = create_ai_data(R5);
    R0 = res.block;
    R2 = res.usedBits;
    R3 = res.usedBitsAddr;
    R5 = res.r5;
    RETURN_TO_CALLER();
}
