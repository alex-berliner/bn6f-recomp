/*
 * asm02.c: readable functions of bn6f asm02.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── getChip8021DA8 ── */
/* getChip8021DA8(r0 = chip index) -> r0 = &ChipDataArr[idx]
 * Returns the address of one 44-byte ChipData entry; the flags left behind are
 * those of the address addition (callers may branch on c/v). */
Next getChip8021DA8(void) {
    R1 = A_ChipDataArr_8021DA8;
    R2 = 44;                 /* sizeof(ChipData) */
    R0 = R0 * R2;            /* byte offset of the chip's entry */
    SET_FLAGS_ADD(R0, R1);   /* ADDS r0, r0, r1 */
    R0 = R0 + R1;
    RETURN_TO_CALLER();
}

/* ── sChip_loadRecordIntoSlot_8021AB4 ── */
/* sChip_loadRecordIntoSlot_8021AB4(r0 = source record, r1 = slot index):
 * Copies a 0x3C-unit record from r0 into slot r1 of the S-chip table whose
 * base pointer is stored at [r10 + oToolkit_S_Chip_2002178_Ptr] (slot r1
 * begins at base + r1 * 0x3C).  It then scans the copy's 30 halfwords: each
 * one is unpacked into two 9-bit values by split9BitsFromBitfield_8021AE0
 * and handed to pack_addChipByIdSetLibraryFlag_8021B2A together with a count of 1.
 * Exits with r2 = 1 and r3 = the result of the last pack_addChipByIdSetLibraryFlag_8021B2A call;
 * r4-r7 are preserved (the loop's r6/r7 live only inside this routine). */
Next sChip_loadRecordIntoSlot_8021AB4(void) {
    u32 src = R0;
    u32 slot = R1;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Destination: slot `slot` of the S-chip table. */
    u32 dest = LD32(R10 + oToolkit_S_Chip_2002178_Ptr) + slot * 0x3C;

    /* CopyWords(src = r0 (unchanged), dest, 0x3C) */
    R1 = dest;
    R2 = 0x3C;
    GUEST_CALL(CopyWords);

    R7 = dest;
    for (R6 = 0; (s32)R6 < 0x3C; R6 += 2) {
        R0 = LD16(dest + R6);
        GUEST_CALL(split9BitsFromBitfield_8021AE0); /* -> r0, r1 */
        R2 = 1;
        GUEST_CALL(pack_addChipByIdSetLibraryFlag_8021B2A); /* -> r3 */
    }

    /* The original pops r6/r7 here, restoring the caller's values. */
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── split9BitsFromBitfield_8021AE0 ── */
/* split9BitsFromBitfield_8021AE0(r0 = bitfield):
 * Splits a bitfield into its lower and upper 9 bits:
 *   r0 = low 9 bits, r1 = high 9 bits (bitfield >> 9).
 * Bits 16-31 of the input are assumed to be unset.
 *
 * r2 receives the mask word loaded from the literal pool. The pool
 * entry only defines a halfword (0x01FF); the high half of the loaded
 * word comes from the bytes that follow the pool in ROM, which are the
 * next function's `push {lr}` opcode (0xB510). So callers observe
 * r2 = 0xB51001FF. Only the low 9 bits of that word act as the mask,
 * but r2 is a visible output and must match the original exactly. */
Next split9BitsFromBitfield_8021AE0(void) {
    u32 bitfield = R0;
    const u32 Mask9Bits = 0xB51001FF; /* literal pool word at .Mask9Bits */

    R1 = bitfield >> 9;        /* upper 9 bits */
    R2 = Mask9Bits;
    R0 = bitfield & Mask9Bits; /* mask: lower 9 bits */
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── GiveChips ── */
/* GiveChips(r0 = chip id, r1 = chip code, r2 = count, ...):
 * Grant `count` copies of the given chip to the player's pack.
 * Re-applies the pack's obfuscation first, marks the chip as obtained
 * in the chip library (event flag 0x1E20 + chip id), locates the pack
 * slot holding that chip's code, adds the copies there, and refreshes
 * the slot's bookkeeping field. Returns 0 when count is 0, otherwise
 * the value the pack-add helper leaves in r3. */
Next GiveChips(void) {
    u32 chipId   = R0;
    u32 chipCode = R1;
    u32 count    = R2;
    u32 savedR4  = R4;   /* restored on exit; borrowed as scratch below */

    /* Re-encrypt/verify the chip pack before modifying it. */
    R0 = chipId;
    R1 = chipCode;
    R2 = count;
    GUEST_CALL(encryption_applyPack_8006e70);

    if (count == 0) {
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* Mark the chip as collected in the chip library. */
    R0 = 0x1E20 + chipId;
    GUEST_CALL(SetEventFlag);

    /* Find the pack slot (and its code field) for this chip. */
    R0 = chipId;
    R1 = chipCode;
    R2 = count;
    GUEST_CALL(getOffsetToQuantityOfChipCodeMaybe_8021c7c);
    u32 slotOffset = R0;
    u32 codeField  = R3;

    /* Add the requested number of copies to that slot.
     * r1 is still whatever the lookup left there, as in the original. */
    R2 = count;
    R4 = codeField;
    GUEST_CALL(addChipsToChipPackOffset_8021b5a);
    u32 result = R3;    /* saved across the next call, like the original's push/pop */

    /* Update the per-code bookkeeping field for that slot. */
    R1 = codeField;
    GUEST_CALL(setUnkFieldOfChipCodeInChipPack_81376b8);

    R0 = result;
    R3 = result;
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── pack_addChipByIdSetLibraryFlag_8021B2A ── */
/* pack_addChipByIdSetLibraryFlag_8021B2A(r0 = chip id, r1 = search item, r2 = amount):
 * Adds copies of a chip to the spare chip pack. First re-applies the pack
 * encryption; if an amount was requested, sets the chip's library event flag
 * (flag 0x1E20 + chip id), finds the pack slot holding that chip's code, and
 * adds the chips there. Returns the result addChipsToChipPackOffset leaves in
 * r3, or 0 when the amount is zero. r2/r4-r7 are preserved for the caller. */
Next pack_addChipByIdSetLibraryFlag_8021B2A(void) {
    u32 chip_id = R0;
    u32 search_item = R1;
    u32 amount = R2;

    /* re-apply the chip pack encryption before touching the pack */
    R0 = chip_id;
    GUEST_CALL(encryption_applyPack_8006e70);

    if (amount == 0) {
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* record the chip in the event flags (library flag base 0x1E20) */
    R0 = 0x1E20 + chip_id;
    GUEST_CALL(SetEventFlag);

    /* locate the quantity slot in the pack for this chip's code */
    R0 = chip_id;
    R1 = search_item;
    R2 = amount;
    GUEST_CALL(getOffsetToQuantityOfChipCodeMaybe_8021c7c);
    u32 quantity_offset = R0;

    /* add the chips at that slot */
    R0 = quantity_offset;
    R2 = amount;
    GUEST_CALL(addChipsToChipPackOffset_8021b5a);

    R0 = R3; /* pack update result */
    RETURN_TO_CALLER();
}

/* ── addChipsToChipPackOffset_8021b5a ── */
/* addChipsToChipPackOffset_8021b5a(r0 = pointer to the chip pack slot's count byte,
 * r2 = number of chips to add):
 * Adds r2 chips to the slot's count, capped at 99. Returns r1 = the new count and
 * r3 = the outcome: 1 if the slot was already full, 0 if the chips fit exactly,
 * 2 if the count had to be clamped to 99. */
Next addChipsToChipPackOffset_8021b5a(void) {
    u32 *countPtr = (u32 *)R0; /* byte-sized field addressed via r0 */
    u32 count = LD8((u32)countPtr);
    u32 status;

    if (count == 99) {
        /* Slot already full; nothing to add. */
        status = 1;
    } else {
        status = 0;
        count += R2;
        if ((s32)count > 99) {
            /* More chips than the slot can hold; clamp to 99. */
            count = 99;
            status = 2;
        }
    }

    ST8((u32)countPtr, count);
    R1 = count;
    R3 = status;
    RETURN_TO_CALLER();
}

/* ── TakeChips ── */
/* Outcome codes TakeChips reports (in r3, and -- as the original also does -- in r0). */
enum TakeChipsStatus {
    TAKE_TAKEN      = 0, /* chips were removed; the owned count is still positive */
    TAKE_NONE_OWNED = 1, /* the owned count was already zero */
    TAKE_NOT_ENOUGH = 2  /* fewer owned than requested; nothing is taken */
};

/* Where a chip code's owned count lives, from the pack lookup. */
struct ChipCodeQuantity {
    u32 count_addr;  /* address of the owned-count byte (lookup result in r0) */
    u32 empty_value; /* second value the lookup leaves in r3; TakeChips reports it
                        in r1 instead of a count once the count reaches zero */
};

/* Everything TakeChips leaves behind for its caller. */
struct TakeChipsResult {
    u32 count;   /* r1: new owned count, or empty_value once it hit zero */
    u32 status;  /* r3 (and r0): TAKE_* code */
    u32 amount;  /* r2: echoed back unchanged; the original preserves r2 (push/pop
                    around the lookup) even though the lookup may clobber it */
};

/* register form: calls getOffsetToQuantityOfChipCodeMaybe_8021c7c outside this file, which has no typed C interface yet */
static struct ChipCodeQuantity find_chip_code_quantity(u32 chip_idx, u32 chip_code, u32 amount) {
    struct ChipCodeQuantity found;
    R0 = chip_idx;
    R1 = chip_code;
    R2 = amount; /* the take amount doubles as the lookup's third argument */
    GUEST_CALL(getOffsetToQuantityOfChipCodeMaybe_8021c7c);
    found.count_addr = R0;
    found.empty_value = R3;
    return found;
}

/* Remove `amount` copies of a chip from the pack. The owned count never drops
 * below zero: if fewer than `amount` are owned, the take is refused and the
 * count is left alone. When the count reaches zero, `empty_value` from the
 * lookup is reported instead of the count. */
static struct TakeChipsResult take_chips(u32 chip_idx, u32 chip_code, u32 amount) {
    struct ChipCodeQuantity found = find_chip_code_quantity(chip_idx, chip_code, amount);
    struct TakeChipsResult result;
    u32 count = LD8(found.count_addr);
    u32 new_count;
    u32 status;

    if (count == 0) {
        status = TAKE_NONE_OWNED;
        new_count = 0;
    } else {
        status = TAKE_TAKEN;
        new_count = count - amount;
        if ((s32)count < (s32)amount) {
            /* would underflow: refuse the take, keep the current count */
            status = TAKE_NOT_ENOUGH;
            new_count = count;
        }
    }

    ST8(found.count_addr, (u8)new_count);

    result.count = new_count != 0 ? new_count : found.empty_value;
    result.status = status;
    result.amount = amount;
    return result;
}

/* TakeChips(r0 = chip index, r1 = chip code, r2 = amount)
 * -> r0/r3 = status, r1 = new count (or the lookup's empty value), r2 kept intact */
Next TakeChips(void) {
    struct TakeChipsResult result = take_chips(R0, R1, R2);
    R0 = result.status;
    R1 = result.count;
    R2 = result.amount;
    R3 = result.status;
    RETURN_TO_CALLER();
}

/* ── GetChipCountOfCode ── */
/* register form: calls sub_8006EA4 outside this file, which has no typed C interface yet */
static bool chip_code_is_valid(u32 chip_idx, u32 search_item) {
    R0 = chip_idx;
    R1 = search_item;
    GUEST_CALL(sub_8006EA4);
    return ZERO_FLAG();          /* Z set ⇒ code is recognised; fall through to count path */
}

/* register form: calls getOffsetToQuantityOfChipCodeMaybe_8021c7c outside this file, which has no typed C interface yet */
static u32 chip_code_quantity_addr(u32 chip_idx, u32 search_item, u32 off) {
    R0 = chip_idx;
    R1 = search_item;
    R2 = off;
    GUEST_CALL(getOffsetToQuantityOfChipCodeMaybe_8021c7c);
    return R0;                   /* pointer to the quantity byte for this code */
}

/* Return the owned quantity of a chip code, or 0 if the code is unrecognised. */
static u8 get_chip_count_of_code(u32 chip_idx, u32 search_item, u32 off) {
    if (!chip_code_is_valid(chip_idx, search_item))
        return 0;
    u32 addr = chip_code_quantity_addr(chip_idx, search_item, off);
    return (u8)LD8(addr);
}

/* GetChipCountOfCode(r0 = chip_idx, r1 = search_item, r2 = off) -> r0 */
Next GetChipCountOfCode(void) {
    R0 = get_chip_count_of_code(R0, R1, R2);
    RETURN_TO_CALLER();
}

/* ── GetTotalChipCount ── */
/* register form: calls encryption_testPack_8006e84 outside this file, which has no typed C interface yet */
static bool pack_test_passes(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(encryption_testPack_8006e84);
    return ZERO_FLAG();   /* Z=1 → test passed (bne not taken) */
}

typedef struct {
    u32 count;      /* sum of 4 chip-count bytes */
    u32 last_byte;  /* the final byte loaded (leftover in r1) */
} ChipSum;

/* Sum the 4 chip-count bytes for a given chip ID from the toolkit table. */
static ChipSum compute_chip_count(u32 chip_id, u32 toolkit) {
    if (!pack_test_passes(chip_id))
        return (ChipSum){0, 0};

    u32 base  = LD32(toolkit + oToolkit_Unk2002230_Ptr);
    u32 entry = base + chip_id * 12;  /* 12 bytes per chip entry */

    u32 b0 = LD8(entry);
    u32 b1 = LD8(entry + 1);
    u32 b2 = LD8(entry + 2);
    u32 b3 = LD8(entry + 3);

    return (ChipSum){b0 + b1 + b2 + b3, b3};
}

/* GetTotalChipCount: returns the sum of 4 count bytes for chip_id = r0.
 * Calls encryption_testPack first; returns 0 if that test fails.
 * r1 ends with the 4th byte (success) or 0 (failure – unused by callers). */
Next GetTotalChipCount(void) {
    ChipSum res = compute_chip_count(R0, R10);
    R0 = res.count;
    R1 = res.last_byte;
    RETURN_TO_CALLER();
}

/* ── zeroFill_e2002230 ── */
/* zeroFill_e2002230: clear the game's extra work-RAM block.
 * Reads the toolkit pointer from r10 and fills the 0xF00-byte block at
 * oToolkit_Unk2002230_Ptr with zeros. Inputs r2, r3 and r10 are preserved;
 * the caller's r3 is returned unchanged. */
Next zeroFill_e2002230(void) {
    u32 block = LD32(R10 + oToolkit_Unk2002230_Ptr);

    R0 = block;
    R1 = 0xF00; /* size of the block in bytes */
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── getOffsetToQuantityOfChipCodeMaybe_8021c7c ── */
/* getOffsetToQuantityOfChipCodeMaybe_8021c7c(r0 = chip id, r1 = chip code):
 * Look up the chip's data (its four codes come first) and find the position of
 * code r1 among them, or 0 if the chip doesn't have that code.  Returns the
 * address of the matching entry in the per-chip table Toolkit.unk_2002230:
 *     table + 12*chipId + codeIndex
 * Leaves r0 = address, r1 = 12*chipId, r2 = chip id, r3 = code index.
 * r4 and r7 are preserved, as in the original. */
Next getOffsetToQuantityOfChipCodeMaybe_8021c7c(void) {
    u32 chipId = R0;
    u32 wantedCode = R1;

    GUEST_CALL(getChip8021DA8); /* R0 = pointer to the chip's data */
    u32 chipData = R0;

    /* Search the chip's four codes for the wanted one. */
    u32 codeIndex;
    for (codeIndex = 0; codeIndex < 4; codeIndex++) {
        if (LD8(chipData + codeIndex) == wantedCode)
            break;
    }
    if (codeIndex == 4)
        codeIndex = 0; /* code not found: fall back to the first slot */

    u32 table = LD32(R10 + oToolkit_Unk2002230_Ptr);
    u32 offset = 12 * chipId;

    R0 = table + offset + codeIndex;
    R1 = offset;
    R2 = chipId;
    R3 = codeIndex;
    RETURN_TO_CALLER();
}

/* ── battle_countValidChipSlots_8021CA8 ── */
#define NUM_CHIPS           0x140
#define SLOTS_PER_CHIP      4
#define CHIP_TABLE_STRIDE   0xC
#define CHIP_FLAGS_OFFSET   9
#define CHIP_TYPE_OFFSET    7
#define CHIP_EMPTY          0xFF
#define CHIP_FLAGS_EXCLUDE  0x20

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data(s32 chip_index) {
    R0 = (u32)chip_index;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls encryption_testPack_8006e84 outside this file, which has no typed C interface yet */
static bool chip_pack_eligible(s32 chip_index) {
    R0 = (u32)chip_index;
    GUEST_CALL(encryption_testPack_8006e84);
    return ZERO_FLAG();          /* Z set (return 0) means the chip is eligible for counting */
}

/* Iterate all 320 chips; for each eligible one, sum the per-slot entry from the
 * toolkit chip-count table (stride 12). Also accumulate a type-2 subtotal,
 * but the original discards that (it lands in clobbered r1). Returns the grand total. */
static s32 count_valid_chip_slots(u32 toolkit) {
    u32 chip_table_base = LD32(toolkit + oToolkit_Unk2002230_Ptr);
    s32 total = 0;
    s32 type2_total = 0;

    for (s32 idx = 0; idx < NUM_CHIPS; idx++) {
        u32 chip = get_chip_data(idx);
        if (!chip_pack_eligible(idx))
            continue;

        for (s32 slot = 0; slot < SLOTS_PER_CHIP; slot++) {
            u8 slot_val = LD8(chip + slot);
            if (slot_val == CHIP_EMPTY)
                continue;
            if (LD8(chip + CHIP_FLAGS_OFFSET) & CHIP_FLAGS_EXCLUDE)
                continue;

            u8 entry = *(u8 *)(chip_table_base + CHIP_TABLE_STRIDE * idx + slot);
            total += entry;
            if (LD8(chip + CHIP_TYPE_OFFSET) == 2)
                type2_total += entry;
        }
    }
    (void)type2_total;
    return total;
}

/* battle_countValidChipSlots_8021CA8(r10 = toolkit ptr) -> r0 = total chip-slot count */
Next battle_countValidChipSlots_8021CA8(void) {
    R0 = (u32)count_valid_chip_slots(R10);
    RETURN_TO_CALLER();
}

/* ── battle_ClearWorkBuffer_8021D08 ── */
/* battle_ClearWorkBuffer_8021D08: clear the work buffer at unk_203A0A0 (0x2E0 halfwords). */
Next battle_ClearWorkBuffer_8021D08(void) {
    R0 = A_unk_203A0A0;
    R1 = 0x2E0;
    GUEST_CALL(ZeroFillByHalfword);
    RETURN_TO_CALLER();
}

/* ── incrementRecordCounterByte_8021D14 ── */
/* incrementRecordCounterByte_8021D14(r0 = record index, r1 = field offset):
 * Increment the byte at unk_203A0A0 + 0x170*r0 + r1 (a per-entry counter),
 * saturating at 0xFF. Returns the (possibly updated) counter value in r2.
 * Flags: on the saturate path they come from CMP count,0xFF; on the
 * increment path they come from the ADDS. */
Next incrementRecordCounterByte_8021D14(void) {
    u32 entry = A_unk_203A0A0 + 0x170 * R0;

    u32 count = LD8(entry + R1);
    SET_FLAGS_CMP(count, 0xFF);
    if ((s32)count < 0xFF) {
        count = count + 1;
        SET_FLAGS_ADD(count, 1);
        ST8(entry + R1, count);
    }
    R2 = count;
    RETURN_TO_CALLER();
}

/* ── sub_8021D36 ── */
/* sub_8021D36: clear a small WRAM struct and copy four 8-word tables into it.
 * inputs:  lr
 * outputs: -
 * clobbers: nz c v r0 r1 r2 lr
 * preserved: r3
 */
Next sub_8021D36(void) {
    R0 = A_unk_2000AF0;
    R1 = 0x40;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = A_TextScriptJap8021D88;
    R1 = A_unk_2001184;
    R2 = 8;
    GUEST_CALL(CopyWords);

    R0 = A_TextScriptJap8021D88;
    R1 = A_unk_200119C;
    R2 = 8;
    GUEST_CALL(CopyWords);

    R0 = A_byte_8021D8A;
    R1 = A_byte_20007D6;
    R2 = 8;
    GUEST_CALL(CopyWords);

    R0 = A_byte_8021D8A;
    R1 = A_byte_200083A;
    R2 = 8;
    GUEST_CALL(CopyWords);

    RETURN_TO_CALLER();
}
