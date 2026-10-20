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

/* ── storeWorkByte_813B920 ── */
/* Store a byte into the work buffer at 0x20096D8. r0 is the value;
 * r1 ends up holding the buffer address, as the original leaves it. */
static void store_byte_20096D8(u32 value) {
    ST8(A_byte_20096D8, value);
}

Next storeWorkByte_813B920(void) {
    store_byte_20096D8(R0);
    R1 = A_byte_20096D8;
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

/* ── sub_813BB00 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r8, r9, r10, r11, r12; } call_sub_813B790_result;
/* register form: generated wrapper: ai_dispatchFromGlobalByte has no typed C interface yet */
static inline call_sub_813B790_result call_sub_813B790(u32 r0, u32 r5, u32 r8, u32 r9, u32 r11, u32 r12) {
    R0 = r0;
    R5 = r5;
    R8 = r8;
    R9 = r9;
    R11 = r11;
    R12 = r12;
    GUEST_CALL(ai_dispatchFromGlobalByte);
    return (call_sub_813B790_result){ R0, R8, R9, R10, R11, R12 };
}

#define PANEL_SCAN_SIZE 7
#define GRID_STRIDE     0xF
#define TABLE_STRIDE    7
#define WINDOW_OFFSET   2

struct scan_result {
    u32 r0;
    u32 r8;
    u32 r9;
    u32 r11;
    u32 r12;
};

/* Scan a 7×7 window of the 15×15 battle grid centred on the target cell.
 * ai_dispatchFromGlobalByte resolves an occupancy table; each non-zero table entry must
 * sit on a grid cell whose value is 3 (passable).  A zero grid cell aborts
 * the scan immediately ("blocked"), otherwise the all-clear flag is cleared
 * on any mismatch.  Result is 1 when blocked or fully clear, 0 otherwise. */
static struct scan_result scan_panel_window(u32 panel_id, u32 grid_ptr,
                                            u32 row, u32 col,
                                            u32 in_r5, u32 in_r8, u32 in_r9,
                                            u32 in_r11, u32 in_r12) {
    call_sub_813B790_result callee = call_sub_813B790(panel_id, in_r5,
                                                       in_r8, in_r9,
                                                       in_r11, in_r12);

    u32 table = callee.r0;
    u32 grid  = grid_ptr + (row + WINDOW_OFFSET) + (col + WINDOW_OFFSET) * GRID_STRIDE;

    u32 all_clear = 1;
    u32 blocked   = 0;

    for (u32 r = 0; r < PANEL_SCAN_SIZE && !blocked; r++) {
        for (u32 c = 0; c < PANEL_SCAN_SIZE; c++) {
            if (LD8(table + c) == 0)
                continue;
            u32 gv = LD8(grid + c);
            if (gv == 0) {
                blocked = 1;
                break;
            }
            if (gv != 3)
                all_clear = 0;
        }
        table += TABLE_STRIDE;
        grid  += GRID_STRIDE;
    }

    struct scan_result out;
    out.r0  = blocked ? 1u : all_clear;
    out.r8  = callee.r8;
    out.r9  = callee.r9;
    out.r11 = callee.r11;
    out.r12 = callee.r12;
    return out;
}

/* sub_813BB00(r0=panel_id, r1=grid_ptr, r2=row, r3=col, r5,r8,r9,r10,r11,r12)
 *   -> r0, r8, r9, r10, r11, r12; flags nz */
Next sub_813BB00(void) {
    struct scan_result res = scan_panel_window(R0, R1, R2, R3, R5, R8, R9, R11, R12);
    R0 = res.r0;
    R8 = res.r8; R9 = res.r9; R11 = res.r11; R12 = res.r12;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── navicust_checkPieceOverlap ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r8, r9, r10, r11, r12; } call_ai_dispatchFromGlobalByte_result_navicust_checkPieceOverlap;
/* register form: generated wrapper: ai_dispatchFromGlobalByte has no typed C interface yet */
static inline call_ai_dispatchFromGlobalByte_result_navicust_checkPieceOverlap call_ai_dispatchFromGlobalByte_navicust_checkPieceOverlap(u32 r0) {
    R0 = r0;
    GUEST_CALL(ai_dispatchFromGlobalByte);
    return (call_ai_dispatchFromGlobalByte_result_navicust_checkPieceOverlap){ R0, R8, R9, R10, R11, R12 };
}

/* Check whether a 7x7 window scan finds a cell where both the ai_dispatch
 * result grid and the toolkit's Unk200414c grid have non-zero bytes.
 * Returns 1 + the toolkit byte in r1 on first hit, or 0 with loop residue.
 * r8-r12 / r10 are left by ai_dispatch (set by the wrapper call). */

typedef struct {
    u32 r0;  /* 1 = overlap found, 0 = none */
    u32 r1;  /* hit byte value, or leftover column counter */
    u32 r2;  /* current row offset in grid-B coordinates */
} overlap_result;

static overlap_result check_piece_overlap(u32 dispatch_arg, u32 col_in, u32 row_in) {
    call_ai_dispatchFromGlobalByte_result_navicust_checkPieceOverlap ai = call_ai_dispatchFromGlobalByte_navicust_checkPieceOverlap(dispatch_arg);

    /* grid A = ai_dispatch result; grid B = toolkit table shifted to (col-3, row-3) */
    u32 ptr_a = ai.r0;
    u32 base_b = LD32(ai.r10 + oToolkit_Unk200414c_Ptr);
    s32 col_off = (s32)col_in - 3;
    s32 row_off = (s32)row_in - 3;
    u32 ptr_b = base_b + (u32)(7 * row_off + col_off);

    s32 cur_row = row_off;       /* tracks r2: absolute row in grid B */
    s32 cur_col = col_off;       /* tracks r1: absolute col in grid A */
    u32 row_iter = 0;            /* tracks r5: outer loop counter */

    for (;;) {
        /* Assembly reloads r1 = col_off at the top of each outer iteration */
        cur_col = col_off;

        if ((cur_row >= 0) & (cur_row < 7)) {
            for (u32 idx = 0; idx < 7; idx++) {
                if ((cur_col >= 0) & (cur_col < 7)) {
                    if (LD8(ptr_a + idx) != 0) {
                        if (LD8(ptr_b + idx) != 0) {
                            /* overlap hit: r0=1, r1=gridB byte, r2=current row */
                            overlap_result hit = { 1u, (u32)LD8(ptr_b + idx), (u32)cur_row };
                            return hit;
                        }
                    }
                }
                cur_col++;
            }
        }

        /* advance to next row in both grids */
        ptr_a += 7;
        ptr_b += 7;
        cur_row++;
        row_iter++;

        if (row_iter >= 7) {
            /* no overlap in the entire 7x7 scan */
            overlap_result none = { 0u, (u32)cur_col, (u32)cur_row };
            return none;
        }
    }
}

/* navicust_checkPieceOverlap(r0=dispatch_arg, r2=col, r3=row, r10=toolkit) -> r0 r1 r2 nz */
Next navicust_checkPieceOverlap(void) {
    overlap_result res = check_piece_overlap(R0, R2, R3);
    R0 = res.r0;
    R1 = res.r1;
    R2 = res.r2;
    SET_FLAGS_NZ(R0);
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

/* register form: calls battle_updateTargetPanelsAllRows_813BD24 outside this file, which has no typed C interface yet */
static void call_sub_813BD24(void) {
    GUEST_CALL(battle_updateTargetPanelsAllRows_813BD24);
}

/* register form: calls battle_resetIOPadScratchEntries_813BE8C outside this file, which has no typed C interface yet */
static void call_sub_813BE8C(void) {
    GUEST_CALL(battle_resetIOPadScratchEntries_813BE8C);
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

/* register form: calls battle_checkItemFlagFor_813B9B4 outside this file, which has no typed C interface yet */
static u32 get_random_id(u32 index, u32 count) {
    R0 = index;
    R1 = count;
    GUEST_CALL(battle_checkItemFlagFor_813B9B4);
    return R0;
}

/* register form: calls toolkit_getUnk2004190EntryPtr_813B9FC outside this file, which has no typed C interface yet */
static u32 get_toolkit_entry_ptr(u32 entry_num) {
    R0 = entry_num;
    GUEST_CALL(toolkit_getUnk2004190EntryPtr_813B9FC);
    return R0;
}

/* register form: calls battleObject_getByIndex_813B780 outside this file, which has no typed C interface yet */
static u32 lookup_object_by_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(battleObject_getByIndex_813B780);
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

/* ── sub_813BC98 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r3; } call_sub_813B9B4_result;
/* register form: generated wrapper: battle_checkItemFlagFor_813B9B4 has no typed C interface yet */
static inline call_sub_813B9B4_result call_sub_813B9B4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(battle_checkItemFlagFor_813B9B4);
    return (call_sub_813B9B4_result){ R0, R3 };
}

typedef struct { u32 r0, r2; } call_toolkit_getUnk2004190EntryPtr_813B9FC_result_sub_813BC98;
/* register form: generated wrapper: toolkit_getUnk2004190EntryPtr_813B9FC has no typed C interface yet */
static inline call_toolkit_getUnk2004190EntryPtr_813B9FC_result_sub_813BC98 call_toolkit_getUnk2004190EntryPtr_813B9FC_sub_813BC98(u32 r0) {
    R0 = r0;
    GUEST_CALL(toolkit_getUnk2004190EntryPtr_813B9FC);
    return (call_toolkit_getUnk2004190EntryPtr_813B9FC_result_sub_813BC98){ R0, R2 };
}

typedef struct { u32 r0, r2; } call_sub_813B780_result_sub_813BC98;
/* register form: generated wrapper: battleObject_getByIndex_813B780 has no typed C interface yet */
static inline call_sub_813B780_result_sub_813BC98 call_sub_813B780_sub_813BC98(u32 r0) {
    R0 = r0;
    GUEST_CALL(battleObject_getByIndex_813B780);
    return (call_sub_813B780_result_sub_813BC98){ R0, R2 };
}

/* Marker byte array the game keeps at 0x2006C88; one slot per scanned entry. */
#define MARKER_ARRAY A_unk_2006C88

/* Scan the 0x31 ally/entry records for the current battle.
 *
 * Seven fixed slots (partners 0..6) reserve their entry ids, taken from
 * battle_checkItemFlagFor_813B9B4(id, 3) - 1, and every scanned id that is reserved is skipped.
 * For each unreserved id the record pointer comes from the toolkit helper; an
 * empty record (first 16-bit word 0) is skipped.  Otherwise battleObject_getByIndex_813B780 maps
 * the record to an item descriptor: a usable item (type byte == 0) bumps the
 * caller's per-index rank counter at [r0 + item index], while any other type
 * marks the entry in the global marker array as (entry + 1).
 *
 * The original's local scratch array lives in its stack frame; it is never
 * passed to a callee, so a plain C array reproduces it.  R5/R7 are restored by
 * the original's pop and therefore leave the call unchanged. */
static void scan_entries_and_count_ranks(u32 rank_array) {
    u32 reserved_id[7];

    for (u32 partner = 0; partner < 7; partner++)
        reserved_id[partner] = call_sub_813B9B4(partner, 3).r0 - 1;

    for (u32 entry = 0; entry < 0x31; entry++) {
        /* Skip ids claimed by the seven fixed partners. */
        u32 t;
        for (t = 0; t < 7; t++)
            if (reserved_id[t] == entry)
                break;
        if (t < 7)
            continue;

        u32 record = call_toolkit_getUnk2004190EntryPtr_813B9FC_sub_813BC98(entry).r0;
        if (LD16(record) == 0)
            continue;                     /* empty record */

        u32 item = call_sub_813B780_sub_813BC98(LD16(record)).r0;

        if (LD8(item + 1) == 0) {
            /* Usable item: bump the rank counter for its index. */
            u32 idx = LD8(item + 4);
            ST8(rank_array + idx, (u8)(LD8(rank_array + idx) + 1));
        } else {
            /* Unusable item: flag this entry in the marker array. */
            ST8(MARKER_ARRAY + entry, (u8)(entry + 1));
        }
    }
}

/* sub_813BC98(r0 = rank counter array) -> (memory only) */
Next sub_813BC98(void) {
    scan_entries_and_count_ranks(R0);
    RETURN_TO_CALLER();
}

/* ── getWorkAreaPtr_2006CF8_813BD0C ── */
/* getWorkAreaPtr_2006CF8_813BD0C: trivial getter -- loads the address of the work buffer
 * unk_2006CF8 into r7 and returns. */
Next getWorkAreaPtr_2006CF8_813BD0C(void) {
    R7 = A_unk_2006CF8;
    RETURN_TO_CALLER();
}

/* ── battle_loadNavicustProgramTable_813BD10 ── */
/* battle_loadNavicustProgramTable_813BD10: load the address of unk_2006C88 into r7.
 * A tiny getter used by callers that then access that RAM area. */
Next battle_loadNavicustProgramTable_813BD10(void) {
    R7 = A_unk_2006C88;
    RETURN_TO_CALLER();
}

/* ── getWorkAreaPtr_2006CC0_813BD14 ── */
/* getWorkAreaPtr_2006CC0_813BD14: return the base address of the unk_2006CC0 data block in R7. */
Next getWorkAreaPtr_2006CC0_813BD14(void) {
    R7 = A_unk_2006CC0;
    RETURN_TO_CALLER();
}

/* ── battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_battleObject_getByIndex_813B780_result_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38;
/* register form: generated wrapper: battleObject_getByIndex_813B780 has no typed C interface yet */
static inline call_battleObject_getByIndex_813B780_result_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38 call_battleObject_getByIndex_813B780_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38(u32 r0) {
    R0 = r0;
    GUEST_CALL(battleObject_getByIndex_813B780);
    return (call_battleObject_getByIndex_813B780_result_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38){ R0, R2 };
}

typedef struct { u32 r0, r2; } call_toolkit_getUnk2004190EntryPtr_813B9FC_result_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38;
/* register form: generated wrapper: toolkit_getUnk2004190EntryPtr_813B9FC has no typed C interface yet */
static inline call_toolkit_getUnk2004190EntryPtr_813B9FC_result_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38 call_toolkit_getUnk2004190EntryPtr_813B9FC_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38(u32 r0) {
    R0 = r0;
    GUEST_CALL(toolkit_getUnk2004190EntryPtr_813B9FC);
    return (call_toolkit_getUnk2004190EntryPtr_813B9FC_result_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38){ R0, R2 };
}

#define byte_2006C50_count 0x31

/* Walk all slots of the byte_2006C50 lookup table.  For each non-zero slot
 * that does NOT refer back to the caller's own entry, resolve it through
 * the Unk2004190 toolkit, look up the battle object it references, and if
 * that object's "type" byte (offset 3) matches the one we were given, bump
 * the per-kind counter stored in the Unk200431c array. */
static void increment_counters_for_matching_objects(u32 obj_index, u32 caller_entry, u32 tool_ptr) {
    /* Remember the type byte of the object whose callers want the count */
    u32 self_obj_ptr = call_battleObject_getByIndex_813B780_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38(obj_index).r0;
    u32 self_type = LD8(self_obj_ptr + 3);

    /* The counter array is reached through the toolkit at a fixed offset */
    u32 counter_base = LD32(tool_ptr + oToolkit_Unk200431c_Ptr);

    for (u32 i = 0; i < byte_2006C50_count; i++) {
        u32 slot = LD8(A_byte_2006C50 + i);
        if (slot == 0)
            continue;                       /* empty slot */

        u32 entry_idx = slot - 1;
        if (entry_idx == caller_entry)
            continue;                       /* skip our own entry */

        /* Resolve the Unk2004190 record for this entry */
        u32 entry_ptr = call_toolkit_getUnk2004190EntryPtr_813B9FC_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38(entry_idx).r0;
        if (LD16(entry_ptr) == 0)
            continue;                       /* record has no object reference */

        /* Look up the battle object stored in the record */
        u32 ref_obj_ptr = call_battleObject_getByIndex_813B780_battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38(LD16(entry_ptr)).r0;
        if (LD8(ref_obj_ptr + 3) != self_type)
            continue;                       /* different kind – not a match */

        /* Match found: increment the per-kind counter */
        u32 counter_idx = LD8(ref_obj_ptr + 4);
        ST8(counter_base + counter_idx, LD8(counter_base + counter_idx) + 1);
    }
}

/* battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38
 *  inputs:  r0 (object index), r6 (caller entry index), r10 (toolkit ptr), lr
 *  outputs: r6 (preserved by push/pop)
 *  clobbers: nz c v r0 r1 r2 lr
 */
Next battleObject_incrementUnk200431cCounterForMatchingObjects_813BE38(void) {
    increment_counters_for_matching_objects(R0, R6, R10);
    RETURN_TO_CALLER();
}

/* ── sub_813BEA8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ByteFill has no typed C interface yet */
static inline void call_ByteFill(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(ByteFill);
}

typedef struct { u32 r0, r2; } call_toolkit_getUnk2004190EntryPtr_813B9FC_result_sub_813BEA8;
/* register form: generated wrapper: toolkit_getUnk2004190EntryPtr_813B9FC has no typed C interface yet */
static inline call_toolkit_getUnk2004190EntryPtr_813B9FC_result_sub_813BEA8 call_toolkit_getUnk2004190EntryPtr_813B9FC_sub_813BEA8(u32 r0) {
    R0 = r0;
    GUEST_CALL(toolkit_getUnk2004190EntryPtr_813B9FC);
    return (call_toolkit_getUnk2004190EntryPtr_813B9FC_result_sub_813BEA8){ R0, R2 };
}

typedef struct { u32 r0; } call_sub_813B780_result_sub_813BEA8;
/* register form: generated wrapper: battleObject_getByIndex_813B780 has no typed C interface yet */
static inline call_sub_813B780_result_sub_813BEA8 call_sub_813B780_sub_813BEA8(u32 r0) {
    R0 = r0;
    GUEST_CALL(battleObject_getByIndex_813B780);
    return (call_sub_813B780_result_sub_813BEA8){ R0 };
}

/* Collect unique entry IDs from the toolkit's unknown-entry table into the
 * byte_2006D30 buffer (max 0x31 slots), then bump a tally byte when more
 * than four distinct IDs are found.  r7 is used internally but saved via
 * push/pop, so callers observe it unchanged. */
static void scan_unknown_entries(u32 toolkit, u32 id_buf) {
    /* Fill the ID buffer with 0xFF (empty sentinel). */
    call_ByteFill(id_buf, 0x31, 0xFF);

    u32 collected = 0;

    for (u32 idx = 0; idx < 0x31; idx++) {
        /* Get entry pointer for this index. */
        u32 entry = call_toolkit_getUnk2004190EntryPtr_813B9FC_sub_813BEA8(idx).r0;

        /* Skip entries with a zero 16-bit flag at offset 0. */
        u16 flag = LD16(entry);
        if (flag == 0)
            continue;

        /* Resolve the entry's info structure and read its ID byte. */
        u32 info = call_sub_813B780_sub_813BEA8(flag).r0;
        u8 id = LD8(info + 3);

        /* Check whether this ID is already in the buffer. */
        bool seen = false;
        for (u32 j = 0; j < 0x31; j++) {
            if (LD8(id_buf + j) == id) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;

        /* Record the new ID. */
        ST8(id_buf + collected, id);
        collected++;
    }

    /* Update the toolkit's tally counters when more than four IDs collected. */
    if (collected > 4) {
        u32 tally = LD32(toolkit + oToolkit_Unk200431c_Ptr);
        if (collected == 5)
            ST8(tally + 0xB, LD8(tally + 0xB) + 1);
        else
            ST8(tally + 0xC, LD8(tally + 0xC) + 2);
    }
}

/* sub_813BEA8(r10 = toolkit) -> r7 (preserved, no change) */
Next sub_813BEA8(void) {
    scan_unknown_entries(R10, A_byte_2006D30);
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

/* ── navicust_checkRowOccupied ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r8, r9, r10, r11, r12; } call_ai_dispatchFromGlobalByte_result_navicust_checkRowOccupied;
/* register form: generated wrapper: ai_dispatchFromGlobalByte has no typed C interface yet */
static inline call_ai_dispatchFromGlobalByte_result_navicust_checkRowOccupied call_ai_dispatchFromGlobalByte_navicust_checkRowOccupied(u32 r0) {
    R0 = r0;
    GUEST_CALL(ai_dispatchFromGlobalByte);
    return (call_ai_dispatchFromGlobalByte_result_navicust_checkRowOccupied){ R0, R8, R9, R10, R11, R12 };
}

/* Check whether any occupied cell lies in the centre row (offset 3) of a
 * 7×7 window centred on (col, row) within the grid returned by
 * ai_dispatchFromGlobalByte.  Returns 1 if found, 0 otherwise. */
static u32 check_row_occupied(u32 grid_byte, u32 col, u32 row) {
    call_ai_dispatchFromGlobalByte_result_navicust_checkRowOccupied call = call_ai_dispatchFromGlobalByte_navicust_checkRowOccupied(grid_byte);
    u32 grid = call.r0;                 /* pointer to 7×7 byte grid */

    s32 r1 = (s32)col - 3;             /* logical column cursor (window-relative) */
    s32 r2 = (s32)row - 3;             /* logical row cursor */

    for (s32 outer = 0; outer < 7; outer++) {
        s32 r1_saved = r1;             /* original push {r1} */

        if (r2 >= 0 && r2 < 7) {       /* row within the 7×7 grid */
            u32 r3 = 0;               /* byte offset from start of this row */
            while (r3 < 7) {
                if (r1 >= 0 && r1 < 7) {
                    u8 cell = LD8(grid + r3);
                    if (cell != 0 && r2 == 3)
                        return 1;
                }
                r1++;
                r3++;
            }
        }

        r1 = r1_saved;                 /* original pop {r1} */
        grid += 7;                     /* advance to next grid row */
        r2++;
    }

    return 0;
}

Next navicust_checkRowOccupied(void) {
    R0 = check_row_occupied(R0, R1, R2);
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

/* register form: calls pet_reloadCurNaviEffects outside this file, which has no typed C interface yet */
static void call_sub_8121154(void) {
    GUEST_CALL(pet_reloadCurNaviEffects);
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

    GUEST_CALL(pet_syncNaviActiveNaviMaxHP_803CE44);

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

/* register form: calls battle_checkItemFlagFor_813B9B4 outside this file, which has no typed C interface yet */
static u32 lookup_battle_item(u32 col, u32 row) {
    R0 = col;
    R1 = row;
    GUEST_CALL(battle_checkItemFlagFor_813B9B4);
    return R0;
}

/* register form: calls toolkit_getUnk2004190EntryPtr_813B9FC outside this file, which has no typed C interface yet */
static u32 get_item_entry_ptr(u32 item_id) {
    R0 = item_id;
    GUEST_CALL(toolkit_getUnk2004190EntryPtr_813B9FC);
    return R0;
}

/* register form: calls battleObject_getByIndex_813B780 outside this file, which has no typed C interface yet */
static u32 resolve_item_object(u32 item_index) {
    R0 = item_index;
    GUEST_CALL(battleObject_getByIndex_813B780);
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

/* ── navicust_NCP_SlipRunr ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_SetCurPETNaviStatsByte_result;
/* register form: generated wrapper: SetCurPETNaviStatsByte has no typed C interface yet */
static inline call_SetCurPETNaviStatsByte_result call_SetCurPETNaviStatsByte(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(SetCurPETNaviStatsByte);
    return (call_SetCurPETNaviStatsByte_result){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* NaviCust program "SlipRunner": turn the program on in the current PET's
 * stats (SetCurPETNaviStatsByte(0, 0x35, 1)). r10 is untouched and flows
 * through to SetCurPETNaviStatsByte, which reads it as its hidden argument. */
static void ncp_slip_runr_apply(void) {
    call_SetCurPETNaviStatsByte(0, 0x35, 1);
}

/* inputs: r10 lr; outputs: -; clobbers: nz c v r0 r1 r2 r3 lr */
Next navicust_NCP_SlipRunr(void) {
    ncp_slip_runr_apply();
    RETURN_TO_CALLER();
}

/* ── navicust_NCP_BustPack ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r3; } call_GetCurPETNaviStatsByte_result;
/* register form: generated wrapper: GetCurPETNaviStatsByte has no typed C interface yet */
static inline call_GetCurPETNaviStatsByte_result call_GetCurPETNaviStatsByte(u32 r0) {
    R0 = r0;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (call_GetCurPETNaviStatsByte_result){ R0, R3 };
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_SetCurPETNaviStatsByte_result_navicust_NCP_BustPack;
/* register form: generated wrapper: SetCurPETNaviStatsByte has no typed C interface yet */
static inline call_SetCurPETNaviStatsByte_result_navicust_NCP_BustPack call_SetCurPETNaviStatsByte_navicust_NCP_BustPack(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetCurPETNaviStatsByte);
    return (call_SetCurPETNaviStatsByte_result_navicust_NCP_BustPack){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* BustPack: raise each NaviCust stat cap (ATK / DEF / SPD, slots 1-3) by 3,
 * clamped to a maximum of 4.  Operates on the current PET (index 0). */

/* register form: the Get/SetCurPETNaviStatsByte wrappers parameterise only r0
 * (PET index); the stat slot (r1) and new cap value (r2) must already be in
 * the guest register file before each call. */
static void bust_pack(void) {
    for (u32 slot = 1; slot <= 3; slot++) {
        R1 = slot;                                       /* register form: slot for Get */
        u32 stat = call_GetCurPETNaviStatsByte(0).r0;
        s32 value = (s32)(stat + 3);
        if (value > 4)
            value = 4;
        R1 = slot;                                       /* register form: slot for Set */
        R2 = (u32)value;                                 /* register form: new cap for Set */
        call_SetCurPETNaviStatsByte_navicust_NCP_BustPack(0);
    }
}

/* navicust_NCP_BustPack(r10, lr) -> void; clobbers r0-r3, flags, lr */
Next navicust_NCP_BustPack(void) {
    bust_pack();
    RETURN_TO_CALLER();
}

/* ── navicust_NCP_HPPlus400 ── */
/* Navicust program "HP+400": add 400 to the current max-HP bonus.
 * r10 holds the toolkit pointer; the bonus lives in the halfword pointed to
 * by toolkit->Unk2004334 (field offset 0). */
static void ncp_add_hp_plus_400(u32 toolkit)
{
    u32 bonusPtr = LD32(toolkit + oToolkit_Unk2004334_Ptr);
    u16 hpBonus  = LD16(bonusPtr);

    hpBonus += 0x190;               /* +400 HP */
    ST16(bonusPtr, hpBonus);
}

/* navicust_NCP_HPPlus400(r10 = toolkit) -> nothing */
Next navicust_NCP_HPPlus400(void) {
    ncp_add_hp_plus_400(R10);
    RETURN_TO_CALLER();
}

/* ── sub_813CBCC ── */
/* sub_813CBCC: Process PETNavi stat slots 1..0xF by dispatching each slot's
 * registered handler from a computed function-pointer table.
 *
 * First clears the "processed" event flag (page 0x17, bit 0x20 = EVENT_1720).
 * If stat 0x1F == 1 (a special flag), delegates entirely to pet_zeroStatSlotBuffer and returns.
 * Otherwise, walks slots 1 through 0xF:
 *   1. Looks up a capped stat-type byte (0..3) via getUnk...813BF0C.
 *   2. Computes table index = statType*4 + slot*16.
 *   3. Loads the handler address from the table at A_byte_813CC18 + index.
 *   4. If non-null, calls it via indirect branch (bx), then re-sets the event flag.
 *   5. Increments the loop counter (which the handler may have modified).
 *
 * The push/pop of {r4-r7,lr} ensures the caller's r4-r7 are preserved despite
 * handlers freely clobbering registers via the indirect call.
 */

/* EVENT_1720: page 0x17, bit 0x20 */
#define EVENT_1720_PAGE  0x17u
#define EVENT_1720_BIT   0x20u

Next sub_813CBCC(void) {
    /* Save r4-r7 and lr (original: push {r4-r7,lr} / pop {r4-r7,pc}). */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* Clear the "already processed this round" event flag. */
    R0 = EVENT_1720_PAGE;
    R1 = EVENT_1720_BIT;
    GUEST_CALL(ClearEventFlagFromImmediate);

    /* Special path: if PETNavi stat 0x1F == 1, delegate and skip all slots. */
    GUEST_CALL(checkCurPETNaviStats1FIsOne_813C490);
    if ((s32)R0 == 1) {
        GUEST_CALL(pet_zeroStatSlotBuffer);
        R4 = savedR4; R5 = savedR5; R6 = savedR6; R7 = savedR7;
        RETURN_TO_CALLER();
    }

    /* Walk stat slots 1 through 0xF. */
    R6 = 1;
    while ((s32)R6 < 0x10) {
        /* Get the capped stat-type (0..3) for this slot. */
        R0 = R6;
        GUEST_CALL(getUnk200431cTableByteCapped3_813BF0C);

        if (R0 != 0) {
            /* Table offset: statType * 4 + slot * 16 (entries are 32-bit pointers). */
            R0 = (R0 << 2) + (R6 << 4);
            R2 = 0;
            R1 = LD32(A_byte_813CC18 + R0);

            if (R1 != 0) {
                /* Indirect call to the slot handler (original: mov lr, pc; bx r1).
                 * Handler may clobber any register including R6. */
                GUEST_CALL_ADDR(R1);

                /* Mark this slot as processed by re-setting the event flag. */
                R0 = EVENT_1720_PAGE;
                R1 = EVENT_1720_BIT;
                GUEST_CALL(SetEventFlagFromImmediate);
            }
        }

        /* Advance loop counter; handler may have altered R6. */
        R6 = R6 + 1;
    }

    /* Restore caller's r4-r7 (original: pop {r4-r7,pc}). */
    R4 = savedR4; R5 = savedR5; R6 = savedR6; R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_813CDC0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_sub_813CDD8_result;
/* register form: generated wrapper: sub_813CDD8 has no typed C interface yet */
static inline call_sub_813CDD8_result call_sub_813CDD8(u32 r4, u32 r6) {
    R4 = r4;
    R6 = r6;
    GUEST_CALL(sub_813CDD8);
    return (call_sub_813CDD8_result){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_813CDC0: fixed-argument front end for sub_813CDD8.
 * It calls sub_813CDD8 with r4 = 10 and r6 = 2, passes its r2 result
 * through, and preserves r4-r7: the original pushes them around the call
 * and pops them afterwards, so the caller's values survive the callee. */

typedef struct {
    u32 r2;     /* the result register sub_813CDD8 leaves behind */
    u32 r4;     /* caller's r4, restored by the pop */
    u32 r6;     /* caller's r6, restored by the pop */
} Sub813CDC0Result;

/* Run sub_813CDD8 with its fixed arguments and report what the caller
 * sees afterwards: the callee's r2 plus the saved r4/r6 it popped back. */
static Sub813CDC0Result sub_813CDC0_body(u32 caller_r4, u32 caller_r6) {
    call_sub_813CDD8_result res = call_sub_813CDD8(0xA, 2);

    Sub813CDC0Result out;
    out.r2 = res.r2;
    out.r4 = caller_r4;
    out.r6 = caller_r6;
    return out;
}

/* sub_813CDC0(inputs: r10 lr; outputs: r2 r6; preserved: r4-r7) */
Next sub_813CDC0(void) {
    Sub813CDC0Result out = sub_813CDC0_body(R4, R6);
    R2 = out.r2;
    R4 = out.r4;
    R6 = out.r6;
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

/* ── sub_813D5DC ── */
/* sub_813D5DC: tail-call to sub_814692C. */
Next sub_813D5DC(void) {
    GUEST_CALL(sub_814692C);
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

/* ── sub_813DBC0 ── */
/* sub_813DBC0(r0 = some u16, r1 = some u16, r2 = some u16):
 * Initialize the "ERE5B" magic tag block at 0x200B1B0 (0x0E?53B1B0):
 *  +0x0  = "BRE5" tag (0x42523545)
 *  +0x4  = r2, +0x6 = r1, +0x8 = r0  (halfwords)
 *  +0xA  = 0x3C
 * No flags or return value. */
Next sub_813DBC0(void) {
    ST16(A_dword_200B1B0 + 8, (u16)R0);
    ST16(A_dword_200B1B0 + 6, (u16)R1);
    ST16(A_dword_200B1B0 + 4, (u16)R2);
    ST32(A_dword_200B1B0, 0x42523545);
    ST16(A_dword_200B1B0 + 0xA, 0x3C);
    R0 = 0x3C;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
