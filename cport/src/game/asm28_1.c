/*
 * asm28_1.c: readable functions of bn6f asm28_1.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── zeroFill_e2004348 ── */
/* zeroFill_e2004348:
 * Clear the 0x480-byte struct pointed to by the toolkit's Unk2004348 pointer,
 * filling it with zeroes eight words at a time via ZeroFillByEightWords.
 * All registers other than r0/r1 (and the flags) are preserved. */
Next zeroFill_e2004348(void) {
    u32 structPtr = LD32(R10 + oToolkit_Unk2004348_Ptr);

    R0 = structPtr;
    R1 = 0x480;
    GUEST_CALL(ZeroFillByEightWords);

    RETURN_TO_CALLER();
}

/* ── sub_809F9DC ── */
/* register form: calls sub_809FD70 outside this file, which has no typed C interface yet */
static void call_sub_809FD70(u32 r12_val) {
    R4 = r12_val;
    GUEST_CALL(sub_809FD70);
}

/* register form: calls ByteFill outside this file, which has no typed C interface yet */
static void call_byte_fill(u32 mem, int count, u8 fill) {
    R0 = mem;
    R1 = (u32)count;
    R2 = (u32)fill;
    GUEST_CALL(ByteFill);
}

/* register form: calls mysteryData_resolveMapGroupEntry_809FBF2 outside this file */
static u32 call_resolve_map_group(u32 entry_type, u32 param) {
    R0 = entry_type;
    R1 = param;
    GUEST_CALL(mysteryData_resolveMapGroupEntry_809FBF2);
    return R0;
}

/* register form: calls sub_809FA68 outside this file, which has no typed C interface yet */
static void call_sub_809FA68(u32 entry_off4, u32 resolved, u32 buf_addr,
                             u32 *out_r0, u32 *out_r1) {
    R0 = entry_off4;
    R1 = resolved;
    R2 = buf_addr;
    GUEST_CALL(sub_809FA68);
    *out_r0 = R0;
    *out_r1 = R1;
}

/* register form: calls sub_809FACE outside this file, which has no typed C interface yet */
static void call_sub_809FACE(u32 r0_val, u32 r1_val, u32 r2_val, u32 r5_val) {
    R0 = r0_val;
    R1 = r1_val;
    R2 = r2_val;
    R5 = r5_val;
    GUEST_CALL(sub_809FACE);
}

/* register form: calls ClearEventFlag outside this file, which has no typed C interface yet */
static void call_clear_event_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(ClearEventFlag);
}

/* register form: calls record_writeU16IfIdMatches_809FDCE outside this file */
static u32 call_record_write(u32 group_type, u32 slots, u32 match_count) {
    R0 = group_type;
    R1 = slots;
    R2 = match_count;
    GUEST_CALL(record_writeU16IfIdMatches_809FDCE);
    return R1;
}

/* register form: calls sub_809FDE0 outside this file, which has no typed C interface yet.
   Sets R4/R6/R7/R12 as inputs; restores R4-R7/R12 (original uses pop to preserve them). */
static void call_sub_809FDE0(u32 r4_in, u32 r6_in, u32 r7_in, u32 r12_in,
                             u32 orig_r4, u32 orig_r5, u32 orig_r6, u32 orig_r7, u32 orig_r12) {
    R4 = r4_in;
    R6 = r6_in;
    R7 = r7_in;
    R12 = r12_in;
    GUEST_CALL(sub_809FDE0);
    R4 = orig_r4;
    R5 = orig_r5;
    R6 = orig_r6;
    R7 = orig_r7;
    R12 = orig_r12;
}

/* Walk the InternetMysteryDataMapGroupEntries table.  For each valid group, iterate its
 * list of 12-byte entry blocks; resolve type-5 entries, accumulate results in a 16-byte
 * scratch buffer on the guest stack, clear their event flags, and record per-group
 * statistics.  Terminates on a group whose type field is 1, calling sub_809FDE0 for
 * final output (r1/r2/r3/flags pass through from that callee). */
static void process_mystery_data_map_groups(void) {
    u32 orig_r4 = R4, orig_r5 = R5, orig_r6 = R6, orig_r7 = R7, orig_r12 = R12;

    /* sub_809FD70 is called with the caller's r12 in r4 (original: mov r4,r12; push r4; bl) */
    call_sub_809FD70(orig_r12);

    u32 group_ptr = A_InternetMysteryDataMapGroupEntries;
    u32 r6 = 0;       /* group type / list pointer, varies by path */
    u32 r7 = orig_r7; /* entry block cursor */
    u32 r12 = orig_r12; /* type-5 match counter; initialized to caller's r12 (original keeps it) */

    for (;;) {
        r6 = LD32(group_ptr);

        /* type 1 = sentinel: finalize and return */
        if (r6 == 1) {
            call_sub_809FDE0(group_ptr, r6, r7, r12,
                             orig_r4, orig_r5, orig_r6, orig_r7, orig_r12);
            return;
        }

        /* type 0 = skip this group */
        if (r6 == 0) {
            group_ptr += 8;
            continue;
        }

        /* valid group: r6 = pointer to list of 4-byte entry-block pointers */
        r6 = LD32(group_ptr + 4);
        u32 list_base = r6;

        while (LD32(r6) != 0) {
            r7 = LD32(r6);

            r12 = 0;

            /* allocate 16-byte scratch buffer on the guest stack */
            SP -= 0x10;
            u32 buf = SP;
            u32 write_pos = buf;

            /* register form: ByteFill needs guest-stack address (SP) as mem */
            call_byte_fill(buf, 0x10, 0xFF);

            /* walk 12-byte entries until type byte is 0 */
            while (LD8(r7) != 0) {
                u32 type = LD8(r7);

                if (type == 5) {
                    u32 param = LD32(r7 + 8);
                    u32 resolved = call_resolve_map_group(type, param);

                    u32 off4 = LD32(r7 + 4);
                    u32 sub_r0, sub_r1;
                    call_sub_809FA68(off4, resolved, buf, &sub_r0, &sub_r1);

                    /* store resolved id byte into scratch buffer */
                    ST8(write_pos, sub_r1);
                    write_pos += 1;

                    u32 flag = (u32)LD16(r7 + 2);
                    call_sub_809FACE(flag, sub_r0, sub_r1, write_pos);

                    call_clear_event_flag(flag);

                    r12++;
                }

                r7 += 0xC;
            }

            SP += 0x10;

            /* record per-block statistics: how many list slots consumed so far */
            u32 slots = (r6 - list_base) >> 2;
            call_record_write(LD32(group_ptr), slots, r12);

            r6 += 4;
        }

        group_ptr += 8;
    }
}

/* sub_809F9DC() -> c v r1 r2 r3 (all from sub_809FDE0 via GUEST_CALL) */
Next sub_809F9DC(void) {
    process_mystery_data_map_groups();
    RETURN_TO_CALLER();
}

/* ── sub_809FADE ── */
/* sub_809FADE(r0 = offset within some data area, r10 = toolkit):
 * Look up the pointer stored in the toolkit at oToolkit_Unk2004348_Ptr
 * (a base address in the 0x02004xxx region) and adjust it by an offset.
 * The incoming r0 is a byte offset relative to 0x1400 that is then scaled
 * by 2 (halfword-sized entries), so the result is:
 *   r0 = base + (offset - 0x1400) * 2
 * and r2 = base.
 *   outputs: r0 = base + (offset - 0x1400) * 2, r2 = base */
Next sub_809FADE(void) {
    u32 base = LD32(R10 + oToolkit_Unk2004348_Ptr);

    R2 = base;
    R0 = base + (R0 - 0x1400) * 2;
    RETURN_TO_CALLER();
}

/* ── mysteryData_InitMapGroupTables_809FAF4 ── */
/* mysteryData_InitMapGroupTables_809FAF4: (re)initialize the mystery data map group tables, first the
 * internet-world table, then the real-world table. The table base pointer is
 * passed to sub_809FB04 in r4; r4-r7 are preserved for the caller. */
Next mysteryData_InitMapGroupTables_809FAF4(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R4 = A_InternetMysteryDataMapGroupEntries;
    GUEST_CALL(sub_809FB04);

    R4 = A_RealWorldMysteryDataMapGroupEntries;
    GUEST_CALL(sub_809FB04);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── mysteryData_resolveMapGroupEntry_809FBF2 ── */
/* mysteryData_resolveMapGroupEntry_809FBF2(r0 = map group id, r1 = base pointer):
 * Resolve the pointer for the requested map group. Group 5 (the mystery
 * data groups) uses a table of 12-byte entries and here always selects
 * entry 0; every other group passes the base pointer through unchanged.
 * Outputs: r0 = table pointer. */
Next mysteryData_resolveMapGroupEntry_809FBF2(void) {
    u32 base = R1;

    if (R0 == 5) {
        /* Offset of entry 0 within a 12-byte-entry table: base + 0*8 + 0*4. */
        u32 entryIndex = 0;
        R0 = base + entryIndex * 12;
    } else {
        R0 = base;
    }

    RETURN_TO_CALLER();
}

/* ── sub_809FC96 ── */
/* sub_809FC96: 0809FC96-0809FD70 THUMB */
/*   inputs:  r0 r4 r5 r6 r7 r10 r12 lr
 *   outputs: c v r0 r1 r2 r3 r4 r5 r6 r7 r12 lr
 *   clobbers (unused by callers): nz
 *   preserved: r4 r5 r6 r7 r12 */

typedef struct { u32 r0, r1, r2, r3, cmp_val; } MysteryDataResult;
typedef struct { u32 out_index, out_base; } GoldEntryPair;

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
/* register form: callee may clobber r12 (not in its sync-check set); save/restore preserved r12 */
static u32 get_cur_pet_navi(void) {
    u32 save_r12 = R12;
    GUEST_CALL(GetCurPETNavi);
    u32 result = R0;
    R12 = save_r12;
    return result;
}

/* register form: calls GetNaviStatsByte outside this file, which has no typed C interface yet */
/* register form: callee may clobber r12; save/restore preserved r12 */
static u32 get_navi_stats_byte(u32 pet_id, u32 byte_index) {
    u32 save_r12 = R12;
    R0 = pet_id; R1 = byte_index;
    GUEST_CALL(GetNaviStatsByte);
    u32 result = R0;
    R12 = save_r12;
    return result;
}

/* register form: calls sub_809FADE outside this file, which has no typed C interface yet */
/* register form: callee may clobber r12; save/restore preserved r12 */
static u32 pet_resolve_lookup(u32 navi_id) {
    u32 save_r12 = R12;
    R0 = navi_id;
    GUEST_CALL(sub_809FADE);
    u32 result = R0;
    R12 = save_r12;
    return result;
}

/* register form: calls mysteryData_resolveMapGroupEntry_809FBF2 outside this file, which has no typed C interface yet */
/* register form: arguments passed in r4-r7 which are preserved by this function's contract; restore after call */
static u32 resolve_map_group_entry(u32 type, u32 data_ptr, u32 entry_ptr,
                                   u32 special, u32 navi_id, u32 pet_info) {
    u32 save_r4 = R4, save_r5 = R5, save_r6 = R6, save_r7 = R7;
    R0 = type; R1 = data_ptr; R4 = entry_ptr;
    R5 = special; R6 = navi_id; R7 = pet_info;
    GUEST_CALL(mysteryData_resolveMapGroupEntry_809FBF2);
    u32 result = R0;
    R4 = save_r4; R5 = save_r5; R6 = save_r6; R7 = save_r7;
    return result;
}

/* register form: calls goldMysteryData_GetEntryDataIfIdMatches_809FE44 outside this file, which has no typed C interface yet */
/* register form: r4 passed as base-pointer argument and modified by callee (output); restore preserved r4 after capturing result */
static GoldEntryPair gold_get_entry(u32 navi_id, u32 index, u32 base_ptr) {
    u32 save_r4 = R4;
    R0 = navi_id; R3 = index; R4 = base_ptr;
    GUEST_CALL(goldMysteryData_GetEntryDataIfIdMatches_809FE44);
    GoldEntryPair r;
    r.out_index = R3;
    r.out_base = R4;
    R4 = save_r4;
    return r;
}

/* Extract the four output fields from a resolved 8-byte gold-data entry.
 * cmp_val reproduces the SUBS (r2, #1) that sets c,v before return. */
static MysteryDataResult extract_gold_output(u32 entry_addr, u32 mystery_type) {
    MysteryDataResult res;
    res.r0 = LD32(entry_addr + 4);
    res.r1 = LD8(entry_addr + 3);
    u32 gold_type = LD8(entry_addr);
    res.cmp_val = (gold_type - 1) * 10 + mystery_type;
    res.r2 = res.cmp_val - 1;
    res.r3 = gold_type;
    return res;
}

/* Cyclically scan 8 gold entries (stride 8) for the (remaining_count+1)-th
 * entry whose type byte is 3 or 7.  Wraps around indefinitely until found. */
static u32 scan_gold_cyclic(u32 base, u32 remaining_count) {
    u32 idx = 0;
    for (;;) {
        u32 pos = base + idx * 8;
        u32 t = LD8(pos);
        if (t == 3 || t == 7) {
            if ((s32)remaining_count - 1 < 0)
                return pos;
            remaining_count -= 1;
        }
        idx++;
        if (idx >= 8)
            idx = 0;
    }
}

/* Look up mystery-data output for the given Navi ID.
 * toolkit_ptr is the caller's r10 (game toolkit struct).
 * Returns four 0xFF values with C=1 if the ID is absent. */
static MysteryDataResult lookup_mystery_data(u32 navi_id, u32 toolkit_ptr) {
    /* Check whether the current PET Navi has stat byte 0x33 == 1 ("special"). */
    u32 pet_id = get_cur_pet_navi();
    u32 special = (get_navi_stats_byte(pet_id, 0x33) == 1) ? 1 : 0;

    /* Resolve current map location to the appropriate mystery-data table. */
    u32 game_state = LD32(toolkit_ptr + oToolkit_GameStatePtr);
    u32 map_group  = LD8(game_state + oGameState_MapGroup);
    u32 map_number = LD8(game_state + oGameState_MapNumber);

    u32 table;
    if (map_group >= 0x80) {
        map_group -= 0x80;
        table = A_InternetMysteryDataMapGroupEntries;
    } else {
        table = A_RealWorldMysteryDataMapGroupEntries;
    }

    /* Two-level index: group table (8-byte entries, word at +4 = sub-list ptr)
     * then sub-list of 4-byte map-number entries. */
    u32 group_entry = LD32(table + map_group * 8 + 4);
    u32 entry_addr  = LD32(group_entry + map_number * 4);

    /* Linear search (stride 0xC) for the entry whose u16 Navi ID at +2 matches. */
    while (LD8(entry_addr) != 0) {
        if (LD16(entry_addr + 2) == navi_id)
            break;
        entry_addr += 0xC;
    }

    if (LD8(entry_addr) == 0) {
        /* Not found: return 0xFF sentinel (C=1, V=0 from CMP 0,0). */
        MysteryDataResult res = { 0xFF, 0xFF, 0xFF, 0xFF, 1 };
        return res;
    }

    /* Found: resolve PET info and mystery-group entry data. */
    u32 pet_info     = pet_resolve_lookup(navi_id);
    u32 mystery_type = LD8(entry_addr);
    u32 data_ptr     = LD32(entry_addr + 8);

    u32 resolved = resolve_map_group_entry(mystery_type, data_ptr, entry_addr,
                                           special, navi_id, pet_info);

    u32 skip_index = LD8(pet_info + 1);

    if (mystery_type == 5 && special) {
        /* Type-5 with special Navi: cyclic scan through gold entries. */
        GoldEntryPair gp = gold_get_entry(navi_id, skip_index, resolved);
        u32 found = scan_gold_cyclic(gp.out_base, gp.out_index);
        return extract_gold_output(found, mystery_type);
    }

    /* Normal path: direct index into the gold-entry array. */
    GoldEntryPair gp = gold_get_entry(navi_id, skip_index, resolved);
    u32 target = gp.out_base + gp.out_index * 8;
    return extract_gold_output(target, mystery_type);
}

/* sub_809FC96(r0 = navi_id, r10 = toolkit_ptr) -> r0-r3, c, v */
Next sub_809FC96(void) {
    MysteryDataResult res = lookup_mystery_data(R0, R10);
    R0 = res.r0;
    R1 = res.r1;
    R2 = res.r2;
    R3 = res.r3;
    SET_FLAGS_CMP(res.cmp_val, 1);
    RETURN_TO_CALLER();
}

/* ── record_writeU16IfIdMatches_809FDCE ── */
/* record_writeU16IfIdMatches_809FDCE(r0 = id low byte, r1 = id high byte, r2 = value):
 * Forms a 16-bit id from r1:r0 and compares it against the id recorded at
 * byte_2000210+4. On a match, stores r2 as the record's value at +6.
 * Always leaves the stored id in r1. The record pointer only lives in a
 * scratch register inside the original (r6 is pushed/popped around it), so
 * r4-r7 reach the caller untouched. */
Next record_writeU16IfIdMatches_809FDCE(void) {
    u32 record = A_byte_2000210;
    u32 id = (R1 << 8) | R0;
    u32 storedId = LD16(record + 4);

    R1 = storedId;
    if (id == storedId)
        ST16(record + 6, R2);

    SET_FLAGS_CMP(id, storedId);
    RETURN_TO_CALLER();
}

/* ── sub_809FDE0 ── */
/* Result of the mystery-data roll: the values the original leaves behind in r1 and r3.
 * (r2 ends up holding whatever the hash callee leaves in the register, and r0 is a
 * clobber, so neither is carried through here.) */
struct MysteryDataRoll {
    u32 r1;
    u32 r3;
};

/* register form: fills a 16-byte scratch buffer on the guest stack with 0x20 through
 * ByteFill and hashes it with sub_8000E3A; both are outside this file with no typed C
 * interface yet, and the buffer's guest address has to live in SP. 0x24 bytes are
 * reserved so the buffer sits at the same guest stack address the original uses (it
 * pushes five registers before subtracting 0x10), which also keeps SP identical while
 * the callees run. */
static u32 hash_scratch_state(u32 stateAddr) {
    SP -= 0x24;
    R0 = SP;
    R1 = 0x10;
    R2 = 0x20;
    GUEST_CALL(ByteFill);
    R0 = SP;
    R1 = LD16(stateAddr + 6);   /* re-read the state halfword, as the original does after the fill */
    GUEST_CALL(sub_8000E3A);
    SP += 0x24;
    return R0;
}

/* Roll which mystery data the current internet map block spawns. The state halfword at
 * byte_2000210+6 is hashed (via a 0x20-filled scratch buffer) into a 1-based slot
 * number; the entry list of map byte+4 (biased by 0x80) / group byte+5 is then walked
 * (12 bytes per entry: type, pad, halfword payload) and the payload of the slot-th
 * type-5 entry is stored back into the state halfword, or 0 when the list ends first.
 * When the state is still zero nothing happens and r1/r3 pass through unchanged. */
static struct MysteryDataRoll roll_mystery_data(u32 inR1, u32 inR3) {
    struct MysteryDataRoll out = { inR1, inR3 };

    u32 state = LD16(A_byte_2000210 + 6);
    if (state == 0)
        return out;

    u32 target = hash_scratch_state(A_byte_2000210) + 1;

    u32 map = LD8(A_byte_2000210 + 4);
    u32 group = LD8(A_byte_2000210 + 5);
    u32 entryList = LD32(A_InternetMysteryDataMapGroupEntries + ((map - 0x80) * 8 + 4));
    u32 entry = LD32(entryList + group * 4);

    u32 hits = 0;
    for (;;) {
        u32 type = LD8(entry);
        if (type == 0) {                      /* end of list: nothing to place */
            ST16(A_byte_2000210 + 6, 0);
            break;
        }
        if (type == 5) {                      /* a mystery-data slot: count it */
            hits++;
            if (hits == target) {             /* target-th slot reached: take it */
                ST16(A_byte_2000210 + 6, LD16(entry + 2));
                break;
            }
        }
        entry += 0xC;
    }

    out.r1 = group << 2;                      /* r1 keeps the group table index shift */
    out.r3 = hits;                            /* r3 keeps the slot counter */
    return out;
}

/* sub_809FDE0() — roll the mystery data for the current internet map block.
 * Inputs r4-r7/r10 and r6 are preserved (the original only borrows them and restores
 * them with its pop); r2 is left exactly as sub_8000E3A leaves it, so the adapter
 * never writes it. Both table-walk exits end on an equal compare, which leaves
 * Z set, N clear, C set and V clear. */
Next sub_809FDE0(void) {
    struct MysteryDataRoll out = roll_mystery_data(R1, R3);
    R1 = out.r1;
    R3 = out.r3;
    SET_FLAGS_CMP(0, 0);
    RETURN_TO_CALLER();
}

/* ── sub_809FE36 ── */
/* sub_809FE36(r0 = counter, r4 = value to match):
 * If r4 matches the halfword at byte_2000210+6 (some selection/index field),
 * increment r0. r0 is returned unchanged otherwise. */
Next sub_809FE36(void) {
    u16 match = LD16(A_byte_2000210 + 6);

    if ((s32)R4 == (s32)match)
        R0 = R0 + 1;

    RETURN_TO_CALLER();
}

/* ── goldMysteryData_GetEntryDataIfIdMatches_809FE44 ── */
/* goldMysteryData_GetEntryDataIfIdMatches_809FE44(r0 = mystery data id):
 * Compares the id in r0 against the id currently registered in the gold
 * mystery data work area (byte_2000210). If it matches, fetches that
 * entry's data pointer (GoldMysteryDataEntries[idx].data) and its content
 * byte; otherwise leaves r3/r4 untouched for the caller.
 *   outputs: r4 = entry data pointer, r3 = entry content byte (on match) */
Next goldMysteryData_GetEntryDataIfIdMatches_809FE44(void) {
    u32 work = A_byte_2000210;

    if (R0 == (u32)LD16(work + 6)) {
        u32 idx = LD8(work + 3); /* entry index selected this chapter */
        R4 = LD32(A_GoldMysteryDataEntries + idx * 12 + 4);
        R3 = LD8(work + 2);
    }
    RETURN_TO_CALLER();
}
