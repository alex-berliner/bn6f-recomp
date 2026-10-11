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
