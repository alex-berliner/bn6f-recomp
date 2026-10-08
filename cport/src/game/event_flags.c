/*
 * event_flags.c — the game's event (story progress) flags: bn6f asm03_0.s,
 * 0x0802F110-0x0802F238.
 *
 * The flags are a bit array at [toolkit + oToolkit_EventFlagsPtr]: flag n
 * is bit 7 - n % 8 of byte n / 8 (each byte's first flag is its top bit).
 * Flag numbers are 16-bit; the FromImmediate variants take them as two
 * bytes, r0 = high and r1 = low, which costs the game's scripts 4 bytes of
 * code instead of a literal (they fall through into the plain version).
 *
 * Every function leaves the registers and flags the original leaves (r3 =
 * the flag's byte, r1 = its mask, r0 = the byte's value), because lifted
 * callers may read them: callers of TestEventFlag branch on Z.
 */
#include "game.h"
#include "runtime.h"

static inline u32 flag_array(void) { return LD32(R10 + oToolkit_EventFlagsPtr); }
static inline u32 flag_mask(u32 flag) { return 0x80u >> (flag & 7); }

/* What the original's address arithmetic leaves in the flags: V from
 * `add r3, r3, r1` (array + flag / 8), C = 0 (both shifts that follow
 * shift out a zero bit). N and Z are the caller's to set. */
static inline void address_flags(u32 array, u32 flag) {
    SET_FLAGS_ADD(array, flag >> 3);
    cpu.c = 0;
}

static inline u32 from_immediate(void) { return (R0 << 8) | R1; }

/* ---- single flags: r0 = flag ---- */

enum FlagOp { SET, CLEAR, TOGGLE };

static Next change_flag(u32 flag, enum FlagOp op) {
    u32 array = flag_array(), byte = array + (flag >> 3), mask = flag_mask(flag);
    u32 value = LD8(byte);
    value = op == SET ? value | mask : op == CLEAR ? value & ~mask : value ^ mask;
    ST8(byte, value);
    R0 = value;
    R1 = mask;
    R3 = byte;
    address_flags(array, flag);
    SET_FLAGS_NZ(value);                  /* orr / bic / eor */
    RETURN_TO_CALLER();
}

Next SetEventFlag(void)                 { return change_flag(R0, SET); }
Next SetEventFlagFromImmediate(void)    { return change_flag(from_immediate(), SET); }
Next ClearEventFlag(void)               { return change_flag(R0, CLEAR); }
Next ClearEventFlagFromImmediate(void)  { return change_flag(from_immediate(), CLEAR); }
Next ToggleEventFlag(void)              { return change_flag(R0, TOGGLE); }
Next ToggleEventFlagFromImmediate(void) { return change_flag(from_immediate(), TOGGLE); }

/* Z clear if the flag is set (tst r0, r1). */
static Next test_flag(u32 flag) {
    u32 array = flag_array(), byte = array + (flag >> 3), mask = flag_mask(flag);
    R0 = LD8(byte);
    R1 = mask;
    R3 = byte;
    address_flags(array, flag);
    SET_FLAGS_NZ(R0 & mask);
    RETURN_TO_CALLER();
}

Next TestEventFlag(void)              { return test_flag(R0); }
Next TestEventFlagFromImmediate(void) { return test_flag(from_immediate()); }

/* ---- ranges: r0 = first flag, r2 = count; r4, r5 (and r6, r7 for the
 * test) are preserved. The original's loop is `sub r2, #1; bgt`: it runs
 * at least once, and r2 and the flags come from that last subtraction. ---- */

static Next change_range(u32 flag, enum FlagOp op) {
    u32 array = flag_array(), left = R2, before;
    do {
        u32 byte = array + (flag >> 3), mask = flag_mask(flag);
        u32 value = LD8(byte);
        value = op == SET ? value | mask : op == CLEAR ? value & ~mask : value ^ mask;
        ST8(byte, value);
        R0 = value;
        R1 = mask;
        R3 = byte;
        flag++;
        before = left--;
    } while ((s32)before > 1);
    R2 = left;
    SET_FLAGS_CMP(before, 1);
    RETURN_TO_CALLER();
}

Next SetEventFlagRange(void)                 { return change_range(R0, SET); }
Next SetEventFlagRangeFromImmediate(void)    { return change_range(from_immediate(), SET); }
Next ClearEventFlagRange(void)               { return change_range(R0, CLEAR); }
Next ClearEventFlagRangeFromImmediate(void)  { return change_range(from_immediate(), CLEAR); }
Next ToggleEventFlagRange(void)              { return change_range(R0, TOGGLE); }
Next ToggleEventFlagRangeFromImmediate(void) { return change_range(from_immediate(), TOGGLE); }

/* r0 = 1 (Z clear) if every flag in the range is set. */
static Next test_range(u32 flag) {
    u32 array = flag_array(), count = R2, left = count, before, set = 0;
    do {
        u32 byte = array + (flag >> 3), mask = flag_mask(flag);
        if (LD8(byte) & mask)
            set++;
        R1 = mask;
        R3 = byte;
        flag++;
        before = left--;
    } while ((s32)before > 1);
    R2 = left;
    R0 = set == count;
    SET_FLAGS_CMP(set, count);            /* cmp r6, r7: C and V */
    SET_FLAGS_NZ(R0);                     /* tst r0, r0 */
    RETURN_TO_CALLER();
}

Next TestEventFlagRange(void)              { return test_range(R0); }
Next TestEventFlagRangeFromImmediate(void) { return test_range(from_immediate()); }
