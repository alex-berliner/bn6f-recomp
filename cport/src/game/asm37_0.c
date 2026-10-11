/*
 * asm37_0.c: readable functions of bn6f asm37_0.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── toolkit_resetToolState_813B768 ── */
/* toolkit_resetToolState_813B768:
 * Reset the tool's state: clear the halfword behind the toolkit's
 * Unk2004334 pointer and zero 0x10 bytes behind its Unk200431c pointer.
 * r2 is passed through unchanged. */
Next toolkit_resetToolState_813B768(void) {
    u32 *toolkit = (u32 *)R10;

    ST16(LD32((u32)toolkit + oToolkit_Unk2004334_Ptr), 0);

    R0 = LD32((u32)toolkit + oToolkit_Unk200431c_Ptr);
    R1 = 0x10;
    GUEST_CALL(ZeroFillByByte);

    RETURN_TO_CALLER();
}

/* ── zeroFill_813B934 ── */
/* zeroFill_813B934:
 * Clear the scratch work buffers hanging off the toolkit struct that r10 points
 * to: the 0x188-byte block at oToolkit_Unk2004190_Ptr and the 0x40-byte block
 * at oToolkit_Unk200414c_Ptr. r2/r3 and r10 are preserved. */
Next zeroFill_813B934(void) {
    u32 toolkit = R10;

    R0 = LD32(toolkit + oToolkit_Unk2004190_Ptr);
    R1 = 0x188;
    GUEST_CALL(ZeroFillByWord);

    R0 = LD32(toolkit + oToolkit_Unk200414c_Ptr);
    R1 = 0x40;
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── getIndexedDataTable_813B9E0 ── */
/* getIndexedDataTable_813B9E0(r0 = table index): select one of three data tables by index and
 * return its address in r0. No bounds check in the original, so the index is
 * used as-is. */
Next getIndexedDataTable_813B9E0(void) {
    u32 index = R0;

    /* Table at off_813B9F0: byte_813B1EC, byte_813B2CD, byte_813B3AE. */
    R0 = LD32(A_off_813B9F0 + (index << 2));

    RETURN_TO_CALLER();
}

/* ── toolkit_getUnk2004190EntryPtr_813B9FC ── */
/* toolkit_getUnk2004190EntryPtr_813B9FC(r0 = entry index):
 * Returns the address of entry `r0` in the 8-bytes-per-entry array whose base
 * pointer is stored in the toolkit struct at oToolkit_Unk2004190_Ptr.
 * Outputs: r0 = base + index*8, r1 = array base, r2 = 8,
 * and flags (notably V) as set by the final ADDS. */
Next toolkit_getUnk2004190EntryPtr_813B9FC(void) {
    u32 base = LD32(R10 + oToolkit_Unk2004190_Ptr);
    u32 offset = R0 * 8;

    R1 = base;
    R2 = 8;
    R0 = offset + base;
    SET_FLAGS_ADD(offset, base);
    RETURN_TO_CALLER();
}

/* ── battle_initState_813BBD4 ── */
/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet */
static void zero_fill(u32 addr, u32 num_bytes) {
    R0 = addr;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByByte);
}

/* register form: calls battle_categorizeRandomIds_813BC1C outside this file, which has no typed C interface yet */
static void call_sub_813BC1C(u32 p) {
    R0 = p;
    GUEST_CALL(battle_categorizeRandomIds_813BC1C);
}

/* register form: calls sub_813BC98 outside this file, which has no typed C interface yet */
static void call_sub_813BC98(u32 p) {
    R0 = p;
    GUEST_CALL(sub_813BC98);
}

/* register form: calls sub_813BD24 outside this file, which has no typed C interface yet */
static void call_sub_813BD24(void) {
    GUEST_CALL(sub_813BD24);
}

/* register form: calls sub_813BE8C outside this file, which has no typed C interface yet */
static void call_sub_813BE8C(void) {
    GUEST_CALL(sub_813BE8C);
}

/* register form: calls sub_813BEA8 outside this file, which has no typed C interface yet */
static void call_sub_813BEA8(void) {
    GUEST_CALL(sub_813BEA8);
}

/* register form: calls battleItems_countByType_813C584 outside this file, which has no typed C interface yet */
static void call_sub_813C584(u32 p) {
    R0 = p;
    GUEST_CALL(battleItems_countByType_813C584);
}

/* Initialize the toolkit's battle-related state: zero several buffers, then run the
 * sub-init routines in sequence on the pointer pulled from the toolkit struct. */
static void init_battle_state(u32 toolkit_ptr) {
    u32 state_ptr = LD32(toolkit_ptr + oToolkit_Unk200431c_Ptr);
    zero_fill(state_ptr, 0x10);
    zero_fill(A_unk_2006CF8, 0x31);
    zero_fill(A_unk_2006C88, 0x31);
    zero_fill(A_unk_2006CC0, 0x31);
    call_sub_813BC1C(state_ptr);
    call_sub_813BC98(state_ptr);
    call_sub_813BD24();
    call_sub_813BE8C();
    call_sub_813BEA8();
    call_sub_813C584(state_ptr);
}

/* battle_initState_813BBD4(r10 -> toolkit_ptr) -> r0..r12 unchanged */
Next battle_initState_813BBD4(void) {
    init_battle_state(R10);
    RETURN_TO_CALLER();
}

/* ── battle_categorizeRandomIds_813BC1C ── */
#define ID_SLOTS     7
#define ID_SENTINEL  0xFF

/* register form: calls sub_813B9B4 outside this file, which has no typed C interface yet */
static u32 get_random_id(u32 index, u32 count) {
    R0 = index;
    R1 = count;
    GUEST_CALL(sub_813B9B4);
    return R0;
}

/* register form: calls toolkit_getUnk2004190EntryPtr_813B9FC outside this file, which has no typed C interface yet */
static u32 get_toolkit_entry_ptr(u32 entry_num) {
    R0 = entry_num;
    GUEST_CALL(toolkit_getUnk2004190EntryPtr_813B9FC);
    return R0;
}

/* register form: calls sub_813B780 outside this file, which has no typed C interface yet */
static u32 lookup_object_by_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(sub_813B780);
    return R0;
}

/* Categorize seven random IDs into two target arrays, skipping duplicates and
 * zero entries.  byte_array_ptr indexes a per-ID occurrence counter that is
 * incremented for type-1 objects. */
static void process_categorized_ids(u32 byte_array_ptr) {
    u32 seen_ids[ID_SLOTS];
    for (int i = 0; i < ID_SLOTS; i++)
        seen_ids[i] = ID_SENTINEL;

    int slot = 0;
    for (int idx = ID_SLOTS - 1; idx >= 0; idx--) {
        u32 id = get_random_id(idx, 3);

        /* Reject duplicates or zero IDs */
        bool duplicate = false;
        for (int i = 0; i < ID_SLOTS; i++) {
            if (id == seen_ids[i]) {
                duplicate = true;
                break;
            }
        }
        if (duplicate || id == 0) {
            slot++;
            continue;
        }

        seen_ids[slot] = id;
        slot++;

        /* Look up the toolkit entry; skip if its flag word is zero */
        u32 entry_ptr = get_toolkit_entry_ptr(id - 1);
        u16 flag = LD16(entry_ptr);
        if (flag == 0)
            continue;

        /* Classify the object by its type byte */
        u32 obj_ptr = lookup_object_by_flag(flag);
        u8 obj_type = LD8(obj_ptr + 1);
        u32 target_addr;

        if (obj_type == 1) {
            u8 offset = LD8(obj_ptr + 4);
            ST8(byte_array_ptr + offset, LD8(byte_array_ptr + offset) + 1);
            target_addr = A_unk_2006CC0;
        } else if (obj_type == 2) {
            target_addr = A_unk_2006CC0;
        } else {
            target_addr = A_unk_2006CF8;
        }

        ST8(target_addr + idx, (u8)id);
    }
}

/* battle_categorizeRandomIds_813BC1C(r0 = byte_array_ptr) */
Next battle_categorizeRandomIds_813BC1C(void) {
    process_categorized_ids(R0);
    RETURN_TO_CALLER();
}

/* ── getWorkAreaPtr_2006CF8_813BD0C ── */
/* getWorkAreaPtr_2006CF8_813BD0C: trivial getter -- loads the address of the work buffer
 * unk_2006CF8 into r7 and returns. */
Next getWorkAreaPtr_2006CF8_813BD0C(void) {
    R7 = A_unk_2006CF8;
    RETURN_TO_CALLER();
}

/* ── sub_813BD10 ── */
/* sub_813BD10: load the address of unk_2006C88 into r7.
 * A tiny getter used by callers that then access that RAM area. */
Next sub_813BD10(void) {
    R7 = A_unk_2006C88;
    RETURN_TO_CALLER();
}

/* ── getWorkAreaPtr_2006CC0_813BD14 ── */
/* getWorkAreaPtr_2006CC0_813BD14: return the base address of the unk_2006CC0 data block in R7. */
Next getWorkAreaPtr_2006CC0_813BD14(void) {
    R7 = A_unk_2006CC0;
    RETURN_TO_CALLER();
}

/* ── getUnk200431cTableByteCapped3_813BF0C ── */
/* getUnk200431cTableByteCapped3_813BF0C(r0 = index): look up byte at index r0 in the table pointed to
 * by the toolkit's Unk200431c pointer, clamped to 3.
 * Returns r0 = clamped value, r1 = table pointer. */
Next getUnk200431cTableByteCapped3_813BF0C(void) {
    u32 table = LD32(R10 + oToolkit_Unk200431c_Ptr);
    u32 value = LD8(table + R0);

    if ((s32)value > 3)
        value = 3;

    R0 = value;
    R1 = table;
    RETURN_TO_CALLER();
}

/* ── eventFlag1720IsSet_813BF1C ── */
/* event_1720 is the 16-bit flag 0x1720; TestEventFlagFromImmediate takes it split as
 * hi byte in R0 and low byte in R1. */
#define EVENT_1720_HI 0x17
#define EVENT_1720_LO 0x20

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_1720_is_set(void) {
    R0 = EVENT_1720_HI;
    R1 = EVENT_1720_LO;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* Z set => flag clear */
}

/* Returns 1 when event flag 0x1720 is set, 0 when it is clear.
 * Inputs r4 r5 r6 r7 are preserved across this call (typed function leaves them alone).
 * Outputs: nz v r3 r4 r5 r6 r7 — the caller branches on NZ and consumes r3 as the result,
 * but r3 is left by a callee in the original ("output by callee"), so the adapter simply
 * re-runs that callee path and lets R3 fall out untouched from the helper. The function
 * itself returns the 0/1 value that becomes R0 (and thus sets NZ). */
static u32 check_gate_flag(void) {
    if (event_flag_1720_is_set())
        return 1;
    return 0;
}

Next eventFlag1720IsSet_813BF1C(void) {
    R0 = check_gate_flag();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── eraseMem_ClearBuffer_813C324 ── */
/* eraseMem_ClearBuffer_813C324: clear the 8-byte record at unk_2000090 (EraseMem buffer).
 * r2 is passed through unchanged. */
Next eraseMem_ClearBuffer_813C324(void) {
    R0 = A_unk_2000090;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);

    RETURN_TO_CALLER();
}

/* ── reloadCurNaviStatBoosts_813c3ac ── */
/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u8 get_cur_pet_navi_reloadCurNaviStatBoosts_813c3ac(void) {
    GUEST_CALL(GetCurPETNavi);
    return (u8)R0;
}

/* register form: calls applyNaviStatsMaybe_813C458 outside this file, which has no typed C interface yet */
static void apply_navi_stats_maybe(void) {
    GUEST_CALL(applyNaviStatsMaybe_813C458);
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_reloadCurNaviStatBoosts_813c3ac(u8 page, u8 offset) {
    R0 = page;
    R1 = offset;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls sub_8121154 outside this file, which has no typed C interface yet */
static void call_sub_8121154(void) {
    GUEST_CALL(sub_8121154);
}

/* register form: calls GetCurPETNaviStatsHword outside this file, which has no typed C interface yet */
static u16 get_navi_stat(u8 navi, u8 stat) {
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsHword);
    return (u16)R0;
}

/* register form: calls SetCurPETNaviStatsHword outside this file, which has no typed C interface yet */
static void set_navi_stat(u8 navi, u8 stat, u16 value) {
    R0 = navi;
    R1 = stat;
    R2 = value;
    GUEST_CALL(SetCurPETNaviStatsHword);
}

/* Reload current navi HP stats.
 * Real world (map_group < 0x80): full-heal slot 0 and active navi if EVENT_PET_NAVI_ACTIVE.
 * Internet (map_group >= 0x80): clamp CurHP to MaxHP for slot 0, then for the active navi.
 */
static void reload_cur_navi_stat_boosts(u32 toolkit) {
    /* If no PET navi (GetCurPETNavi returns 0), apply navi stats and check EVENT_163 */
    if (get_cur_pet_navi_reloadCurNaviStatBoosts_813c3ac() == 0) {
        apply_navi_stats_maybe();
        if (event_flag_set_reloadCurNaviStatBoosts_813c3ac(1, 0x63)) { /* EVENT_163 */
            call_sub_8121154();
        }
    }

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u8 map_group = LD8(game_state + oGameState_MapGroup);

    if ((s32)map_group < (s32)0x80) {
        /* Real world: full heal if EVENT_PET_NAVI_ACTIVE is set */
        if (!event_flag_set_reloadCurNaviStatBoosts_813c3ac(0x17, 0x1D)) { /* EVENT_PET_NAVI_ACTIVE */
            return;
        }

        u16 max_hp = get_navi_stat(0, oNaviStats_MaxHP);
        set_navi_stat(0, oNaviStats_CurHP, max_hp);

        u8 navi = get_cur_pet_navi_reloadCurNaviStatBoosts_813c3ac();
        max_hp = get_navi_stat(navi, oNaviStats_MaxHP);
        navi = get_cur_pet_navi_reloadCurNaviStatBoosts_813c3ac();
        set_navi_stat(navi, oNaviStats_CurHP, max_hp);
    } else {
        /* Internet: clamp CurHP down to MaxHP (never up) for slot 0 */
        u16 max_hp = get_navi_stat(0, oNaviStats_MaxHP);
        u16 cur_hp = get_navi_stat(0, oNaviStats_CurHP);
        if ((s32)cur_hp > (s32)max_hp) {
            set_navi_stat(0, oNaviStats_CurHP, max_hp);
        }

        /* Then clamp for the active navi */
        u8 navi = get_cur_pet_navi_reloadCurNaviStatBoosts_813c3ac();
        max_hp = get_navi_stat(navi, oNaviStats_MaxHP);
        navi = get_cur_pet_navi_reloadCurNaviStatBoosts_813c3ac();
        cur_hp = get_navi_stat(navi, oNaviStats_CurHP);

        /* register form: the original's final `cmp r0, r4` / `ble .ret` leaves comparison
         * flags in NZCV that callers branch on; no subsequent GUEST_CALL overwrites them
         * on this path, and a plain C comparison cannot set guest CPU flags. */
        SET_FLAGS_CMP(cur_hp, max_hp);
        if ((s32)cur_hp > (s32)max_hp) {
            navi = get_cur_pet_navi_reloadCurNaviStatBoosts_813c3ac();
            set_navi_stat(navi, oNaviStats_CurHP, max_hp);
        }
    }
}

/* reloadCurNaviStatBoosts_813c3ac(r10 = toolkit ptr) -> all registers + flags */
Next reloadCurNaviStatBoosts_813c3ac(void) {
    reload_cur_navi_stat_boosts(R10);
    RETURN_TO_CALLER();
}

/* ── applyNaviStatsMaybe_813C458 ── */
/* applyNaviStatsMaybe_813C458:
 * Re-apply the equipped NaviCust programs / PET navi stats. Runs the whole
 * "refresh navi customisation" pipeline: rebuild some derived tables, reset
 * the current PET navi to slot 0, clear a story event flag, re-apply the
 * navicust programs for that navi, run follow-up fixups, then restore the
 * navi the player actually had selected and refresh the HUD.
 *
 * All arguments are passed through untouched in r2, r5-r12; r4 is
 * callee-saved (the original pushes/pops it around the calls). */
Next applyNaviStatsMaybe_813C458(void) {
    u32 callerR4 = R4; /* saved across the call sequence, like push {r4} */

    GUEST_CALL(battle_initState_813BBD4); /* rebuild navi-related tables */
    GUEST_CALL(battle_refreshPETNaviStats_8136C24); /* ... */

    GUEST_CALL(GetCurPETNavi);
    u32 naviId = R0;

    /* Temporarily select navi slot 0 so its (stock) stats get applied */
    R0 = 0;
    GUEST_CALL(SetCurPETNavi);

    GUEST_CALL(sub_813C678); /* apply the stock navi's stats */

    /* ClearEventFlagFromImmediate(EVENT_170D): movflag loads r0=0x17, r1=0xD */
    R0 = 0x17;
    R1 = 0xD;
    GUEST_CALL(ClearEventFlagFromImmediate);

    /* applyNavicustPrograms takes the navi whose programs to apply in r4 */
    R4 = naviId;
    GUEST_CALL(applyNavicustPrograms_813C684);

    GUEST_CALL(sub_813CBCC); /* post-application fixups */

    /* Put the player's actual navi back and refresh its display */
    R0 = naviId;
    GUEST_CALL(SetCurPETNavi);

    GUEST_CALL(sub_803CE44);

    R4 = callerR4; /* pop {r4} */
    RETURN_TO_CALLER();
}

/* ── checkCurPETNaviStats1FIsOne_813C490 ── */
/* checkCurPETNaviStats1FIsOne_813C490: read stat byte 0x1F of the PET's current navi and report
 * whether it equals 1 (some cross-menu "navi is set" check).
 * Returns the check result (0 or 1) in r0; r4 is preserved. */
Next checkCurPETNaviStats1FIsOne_813C490(void) {
    u32 result = 0;

    R0 = 0;     /* a1 = stats offset base (unused by the callee) */
    R1 = 0x1F;  /* a2 = byte index to read */
    GUEST_CALL(GetCurPETNaviStatsByte);

    if (R0 == 1)
        result = 1;

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── battleItems_countByType_813C584 ── */
/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet */
static void zero_fill_sub_813C584(u32 addr, u32 size) {
    R0 = addr;
    R1 = size;
    GUEST_CALL(ZeroFillByByte);
}

/* register form: calls CheckKeyItem outside this file, which has no typed C interface yet */
static u32 check_key_item(u32 item_id) {
    R0 = item_id;
    GUEST_CALL(CheckKeyItem);
    return R0;
}

/* register form: calls getIndexedDataTable_813B9E0 outside this file, which has no typed C interface yet */
static u32 get_indexed_data_table(u32 index) {
    R0 = index;
    GUEST_CALL(getIndexedDataTable_813B9E0);
    return R0;
}

/* register form: calls sub_813B9B4 outside this file, which has no typed C interface yet */
static u32 lookup_battle_item(u32 col, u32 row) {
    R0 = col;
    R1 = row;
    GUEST_CALL(sub_813B9B4);
    return R0;
}

/* register form: calls toolkit_getUnk2004190EntryPtr_813B9FC outside this file, which has no typed C interface yet */
static u32 get_item_entry_ptr(u32 item_id) {
    R0 = item_id;
    GUEST_CALL(toolkit_getUnk2004190EntryPtr_813B9FC);
    return R0;
}

/* register form: calls sub_813B780 outside this file, which has no typed C interface yet */
static u32 resolve_item_object(u32 item_index) {
    R0 = item_index;
    GUEST_CALL(sub_813B780);
    return R0;
}

#define ITEM_BUFFER_SIZE   0x31
#define GRID_DIMENSION     7
#define GRID_ROW_STRIDE    15
#define GRID_BASE_ROW      5
#define GRID_BASE_COL      5
#define KEY_ITEM_ID        0x71
#define SLOT_TYPE_MATCH    3

/*
 * Gather battle-network items from a 7x7 grid, sort them by ID,
 * and increment per-type counts in the caller-supplied array.
 *
 * count_array: pointer to a byte-per-type tally array (r0 on entry).
 */
static void gather_and_count_items(u32 count_array) {
    /* Clear the two scratch buffers */
    zero_fill_sub_813C584(A_byte_2006D68, ITEM_BUFFER_SIZE);
    zero_fill_sub_813C584(A_byte_2006DA0, ITEM_BUFFER_SIZE);

    /*
     * Scan the 7x7 item grid.  Each cell whose type byte equals 3
     * contributes a battle-item entry (if one exists).
     */
    int found_count = 0;
    for (int row = 0; row < GRID_DIMENSION; row++) {
        for (int col = 0; col < GRID_DIMENSION; col++) {
            u32 key_status = check_key_item(KEY_ITEM_ID);
            u32 table      = get_indexed_data_table(key_status);
            u32 offset     = (u32)(row + GRID_BASE_ROW) * GRID_ROW_STRIDE
                           + (u32)(col + GRID_BASE_COL);
            if (LD8(table + offset) == SLOT_TYPE_MATCH) {
                u32 entry = lookup_battle_item(col, row);
                if (entry != 0) {
                    ST8(A_byte_2006D68 + found_count, entry);
                    found_count++;
                }
            }
        }
    }

    /*
     * Stable counting-sort: for each possible ID value 1..48,
     * copy the first matching entry from buffer1 into buffer2.
     */
    int sorted_count = 0;
    for (int target = 1; target < ITEM_BUFFER_SIZE; target++) {
        for (int i = 0; i < ITEM_BUFFER_SIZE; i++) {
            if (LD8(A_byte_2006D68 + i) == (u8)target) {
                ST8(A_byte_2006DA0 + sorted_count, (u8)target);
                sorted_count++;
                break;
            }
        }
    }

    /*
     * Walk the sorted list until the first zero sentinel.  For each item,
     * resolve its object, read the type field (offset 4), and increment
     * count_array[type].
     */
    for (int i = 0; LD8(A_byte_2006DA0 + i) != 0; i++) {
        u32 entry_ptr  = get_item_entry_ptr(LD8(A_byte_2006DA0 + i) - 1);
        u16 item_index = LD16(entry_ptr);
        u32 object     = resolve_item_object(item_index);
        u8 type_field  = LD8(object + 4);
        ST8(count_array + type_field, LD8(count_array + type_field) + 1);
    }
}

/* battleItems_countByType_813C584(r0 = count_array) -> r2, r3 */
Next battleItems_countByType_813C584(void) {
    gather_and_count_items(R0);
    R2 = A_byte_2006DA0;
    RETURN_TO_CALLER();
}

/* ── sub_813C678 ── */
/* sub_813C678: clear the halfword referenced by the toolkit's Unk2004334 pointer
 * (r10 = toolkit). */
Next sub_813C678(void) {
    u32 ptr = LD32(R10 + oToolkit_Unk2004334_Ptr);
    ST16(ptr, 0);
    RETURN_TO_CALLER();
}

/* ── battle_clearIncompatibleChipSlot_813CEA0 ── */
/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u8 get_pet_navi_stat_sub_813CEA0(u32 a1, u32 a2) {
    R0 = a1; R1 = a2;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (u8)R0;
}

/* register form: calls SetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static void set_pet_navi_stat_sub_813CEA0(u32 a1, u32 a2, u32 a3) {
    R0 = a1; R1 = a2; R2 = a3;
    GUEST_CALL(SetCurPETNaviStatsByte);
}

/* register form: calls split9BitsFromBitfield_8021AE0 then getChip8021DA8;
 * returns chip data pointer in R0, leaves R2 set (high chip-ID part – the output) */
static u32 decode_chip_from_bitfield(u32 bitfield) {
    R0 = bitfield;
    GUEST_CALL(split9BitsFromBitfield_8021AE0);
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/*
 * Inspect the PET Navi's equipped chip slot.  If the chip's element level
 * exceeds the Navi's own stat byte 9, the slot is marked as consumed (0xFF).
 * `toolkit` is the Toolkit struct pointer (R10 at entry).
 */
static void check_chip_vs_navi_stat(u32 toolkit) {
    /* stat 9: the Navi's threshold */
    u32 navi_stat = get_pet_navi_stat_sub_813CEA0(0, 9);

    /* stat 0x2D: index of the chip pack currently in use */
    u32 chip_index = get_pet_navi_stat_sub_813CEA0(0, 0x2D);

    /* slot = chip_index + 0x2E */
    u32 slot_index = chip_index + 0x2E;

    /* read the current slot value; 0xFF means already empty */
    u32 slot_val = get_pet_navi_stat_sub_813CEA0(0, slot_index);
    if (slot_val == 0xFF)
        return;

    /* locate the u16 chip-ID entry in the pack's data table */
    u32 chip_table = LD32(toolkit + oToolkit_S_Chip_2002178_Ptr);
    u32 entry_addr = (chip_index * 0x3C) + chip_table + (slot_val << 1);
    u16 chip_bitfield = LD16(entry_addr);

    /* split the 18-bit field into two 9-bit halves, fetch chip record */
    u32 chip_ptr = decode_chip_from_bitfield(chip_bitfield);
    u32 element_level = LD8(chip_ptr + 8);

    /* if chip element <= navi stat, no action needed */
    if (element_level <= navi_stat)
        return;

    /* clear the slot so the Navi can't use this chip */
    set_pet_navi_stat_sub_813CEA0(0, slot_index, 0xFF);
}

/* battle_clearIncompatibleChipSlot_813CEA0(r4, r5, r6, r7, r10, lr) -> r2 */
Next battle_clearIncompatibleChipSlot_813CEA0(void) {
    check_chip_vs_navi_stat(R10);
    /* R2 is set by decode_chip_from_bitfield's GUEST_CALL; left alone on early exits */
    RETURN_TO_CALLER();
}

/* ── test0x200bc50_0x5_813D60C ── */
/* test0x200bc50_0x5_813D60C:
 * Test the byte at eS200BC50+5 and return it (and the struct address) so
 * callers can branch on whether the flag is set.
 *   r0 = byte value, r1 = &eS200BC50, flags = Z/N from the byte. */
Next test0x200bc50_0x5_813D60C(void) {
    u32 flags = LD8(A_eS200BC50 + 5);

    R0 = flags;
    R1 = A_eS200BC50;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── sub_813D62C ── */
/* sub_813D62C(r0 = value): store the value into byte_200BC55 (offset 5 of
 * the eS200BC50 struct); r1 = struct base address. */
Next sub_813D62C(void) {
    u32 base = A_eS200BC50;
    ST8(base + 5, R0);
    R1 = base;
    RETURN_TO_CALLER();
}

/* ── link_RequestDisconnect_813D8C4 ── */
/* link_RequestDisconnect_813D8C4:
 * Signal "link connection teardown requested" by storing 4 into the link
 * state work area's state field (dword_2009A28, at offset 0x58 of the
 * pointer that the literal pool carries; the port exposes that pointer
 * value as A_eLinkState). */
Next link_RequestDisconnect_813D8C4(void) {
    ST32(A_eLinkState + 0x58, 4);
    RETURN_TO_CALLER();
}

/* ── zeroFill_813D960 ── */
/* zeroFill_813D960: clear the 16-byte work area at eS200BC50, but preserve the
 * single byte at offset 7 (byte_200BC57), which must survive the wipe.
 * r2-r4 and r7 are untouched; r0 is scratch and r1 passes through from
 * ZeroFillByWord, as do the C/V flags from its last flag-setting instruction. */
Next zeroFill_813D960(void) {
    u32 savedByte;

    R0 = A_eS200BC50;
    savedByte = LD8(R0 + 7);

    R1 = 0x10;
    GUEST_CALL(ZeroFillByWord);

    ST8(A_eS200BC50 + 7, savedByte);
    RETURN_TO_CALLER();
}
