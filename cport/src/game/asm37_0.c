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

/* ── eraseMem_ClearBuffer_813C324 ── */
/* eraseMem_ClearBuffer_813C324: clear the 8-byte record at unk_2000090 (EraseMem buffer).
 * r2 is passed through unchanged. */
Next eraseMem_ClearBuffer_813C324(void) {
    R0 = A_unk_2000090;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);

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

    GUEST_CALL(sub_813BBD4); /* rebuild navi-related tables */
    GUEST_CALL(sub_8136C24); /* ... */

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

/* ── sub_813C678 ── */
/* sub_813C678: clear the halfword referenced by the toolkit's Unk2004334 pointer
 * (r10 = toolkit). */
Next sub_813C678(void) {
    u32 ptr = LD32(R10 + oToolkit_Unk2004334_Ptr);
    ST16(ptr, 0);
    RETURN_TO_CALLER();
}

/* ── test0x200bc50_0x5_813D60C ── */
/* test0x200bc50_0x5_813D60C:
 * Test the byte at eS200BC50+5 and return it (and the struct address) so
 * callers can branch on whether the flag is set.
 *   r0 = byte value, r1 = &eS200BC50, flags = Z/N from the byte. */
Next test0x200bc50_0x5_813D60C(void) {
    u32 flags = LD8(A_eS200BC50_Index00 + 5);

    R0 = flags;
    R1 = A_eS200BC50_Index00;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── sub_813D62C ── */
/* sub_813D62C(r0 = value): store the value into byte_200BC55 (offset 5 of
 * the eS200BC50 struct); r1 = struct base address. */
Next sub_813D62C(void) {
    u32 base = A_eS200BC50_Index00;
    ST8(base + 5, R0);
    R1 = base;
    RETURN_TO_CALLER();
}

/* ── link_RequestDisconnect_813D8C4 ── */
/* link_RequestDisconnect_813D8C4:
 * Signal "link connection teardown requested" by storing 4 into the link
 * state work area's state field (dword_2009A28, at offset 0x58 of the
 * pointer that the literal pool carries; the port exposes that pointer
 * value as A_eCameraEnd). */
Next link_RequestDisconnect_813D8C4(void) {
    ST32(A_eCameraEnd + 0x58, 4);
    RETURN_TO_CALLER();
}

/* ── zeroFill_813D960 ── */
/* zeroFill_813D960: clear the 16-byte work area at eS200BC50, but preserve the
 * single byte at offset 7 (byte_200BC57), which must survive the wipe.
 * r2-r4 and r7 are untouched; r0 is scratch and r1 passes through from
 * ZeroFillByWord, as do the C/V flags from its last flag-setting instruction. */
Next zeroFill_813D960(void) {
    u32 savedByte;

    R0 = A_eS200BC50_Index00;
    savedByte = LD8(R0 + 7);

    R1 = 0x10;
    GUEST_CALL(ZeroFillByWord);

    ST8(A_eS200BC50_Index00 + 7, savedByte);
    RETURN_TO_CALLER();
}
