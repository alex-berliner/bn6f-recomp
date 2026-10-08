/*
 * palette_effects.c — palette transition effects.
 *
 * Readable replacement for the routines at 0x3005EF0-0x3006028 (bn6f
 * asm38.s), which the original copied into IWRAM. getPalleteAndTransition
 * calls them through its effect table byte_8002450 with:
 *   r0 = effect colour       r2 = effect id * 4 (0..3)
 *   r6 = buffer size in bytes r7 = BGR555 palette buffer
 * Each entry is replaced by f(entry) from the last one down to the first.
 * getPalleteAndTransition only relies on r5 afterwards, which these do not
 * touch; palette_addOrSubtractColor is also called by sub_8002818 and the
 * sprite palette upload in asm38.s, so it leaves every register as the
 * original does.
 */
#include "game.h"

/* The buffer is walked by byte offset, exactly like the original's
 * `ldrh r, [r7, r6]` (sizes are always even in practice; LD16/ST16 also
 * follow the ARM7 for odd addresses, so the host build matches too). */
#define PAL_ENTRY_LD(pal, off)    LD16((pal) + (u32)(off))
#define PAL_ENTRY_ST(pal, off, v) ST16((pal) + (u32)(off), (v))

/* Per-channel saturating add of two BGR555 colours: min(a + b, 31). The
 * carry out of each 5-bit channel is detected in parallel (bits 0x8420) and
 * widened to a 0x1F channel mask. */
static inline u32 bgr555_add_mask(u32 a, u32 b) {
    u32 carry = (((a & b) << 1) + ((a ^ b) & 0x7BDE)) & 0x8420;
    return (((carry << 16) >> 21) + 0x3DEF) ^ 0x3DEF;
}
static inline u32 bgr555_add_saturate(u32 a, u32 b, u32 mask) { return ((a + b) - mask) | mask; }

/* Per-channel saturating subtract: max(b - a, 0), same parallel borrow trick. */
static inline u32 bgr555_sub_mask(u32 a, u32 b) {
    u32 borrow = (((a & ~b) << 1) + ((~b ^ a) & 0x7BDE)) & 0x8420;
    return (((borrow << 16) >> 21) + 0x3DEF) ^ 0x3DEF;
}
static inline u32 bgr555_sub_saturate(u32 a, u32 b, u32 mask) { return (b | mask) - (a | mask); }

/* Move each channel of `c` toward 31 (white) by factor/16 of the distance. */
static inline u32 bgr555_lighten(u32 c, u32 fr, u32 fg, u32 fb) {
    u32 r = c & 0x1F, g = (c >> 5) & 0x1F, b = (c >> 10) & 0x1F;
    u32 dr = (((0x1F - r) << 12) * fr) >> 16;
    u32 dg = (((0x1F - g) << 12) * fg) >> 16;
    u32 db = (((0x1F - b) << 12) * fb) >> 16;
    return c + (dr | (dg << 5) | (db << 10));
}

/* Move each channel of `c` toward 0 (black) by factor/16 of its value. */
static inline u32 bgr555_darken(u32 c, u32 fr, u32 fg, u32 fb) {
    u32 r = c & 0x1F, g = (c >> 5) & 0x1F, b = (c >> 10) & 0x1F;
    u32 dr = ((r << 12) * fr) >> 16;
    u32 dg = ((g << 12) * fg) >> 16;
    u32 db = ((b << 12) * fb) >> 16;
    return c - (dr | (dg << 5) | (db << 10));
}

/* Effects 0/1: saturating add / subtract of a constant colour. */
IWRAM_THUMB Next palette_addOrSubtractColor(void) {
    if ((s32)R6 <= 0) {
        SET_FLAGS_CMP(R6, 0);
        RETURN_TO_CALLER();
    }
    u32 color = R0;
    int subtract = R2 != 0;
    u32 pal = R7;
    u32 entry = 0, mask = 0, result = 0;
    s32 off;
    for (off = (s32)R6 - 2; off >= 0; off -= 2) {
        entry = PAL_ENTRY_LD(pal, off);
        mask = subtract ? bgr555_sub_mask(color, entry) : bgr555_add_mask(color, entry);
        result = subtract ? bgr555_sub_saturate(color, entry, mask) : bgr555_add_saturate(color, entry, mask);
        PAL_ENTRY_ST(pal, off, result);
    }
    /* registers as the original's loop and per-colour callback leave them */
    R0 = result;
    R1 = subtract ? (entry | mask) : 0x3DEF;
    R2 = 0x7BDE;
    R3 = 0x8420;
    R4 = mask;
    R6 = (u32)off;
    R8 = 0x3DEF;
    R9 = subtract ? 0x03005F57u : 0x03005F35u;   /* sub_3005F56 / sub_3005F34 (Thumb) */
    R12 = color;
    SET_FLAGS_CMP((u32)off + 2, 2);            /* sub r6, #2 */
    RETURN_TO_CALLER();
}

/* Effects 2/3: fade toward white / black with per-channel factors packed in
 * the effect colour (r0 = factor_r | factor_g << 5 | factor_b << 10). */
IWRAM_THUMB Next palette_fadeTowardWhiteOrBlack(void) {
    u32 fr = R0 & 0x1F, fg = (R0 >> 5) & 0x1F, fb = (R0 >> 10) & 0x1F;
    int darken = R2 == 12;
    u32 pal = R7;
    for (s32 off = (s32)R6 - 2; off >= 0; off -= 2) {
        u32 entry = PAL_ENTRY_LD(pal, off);
        PAL_ENTRY_ST(pal, off, darken ? bgr555_darken(entry, fr, fg, fb)
                                      : bgr555_lighten(entry, fr, fg, fb));
    }
    RETURN_TO_CALLER();
}
