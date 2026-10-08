/*
 * palette_effects.c — palette transition effects.
 *
 * Readable replacement for the routines at 0x3005EF0-0x3006028 (bn6f
 * asm38.s), which the original copied into IWRAM. getPalleteAndTransition
 * calls them through its effect table byte_8002450 with:
 *   r0 = effect colour       r2 = effect id * 4 (0..3)
 *   r6 = buffer size in bytes r7 = BGR555 palette buffer
 * Each entry is replaced by f(entry) from the last one down to the first.
 * The caller only relies on r5 afterwards, which these do not touch.
 */
#include "game.h"

/* The buffer is walked by byte offset, exactly like the original's
 * `ldrh r, [r7, r6]` (sizes are always even in practice). */
#define PAL_ENTRY(pal, off) (*(u16 *)((u8 *)(pal) + (off)))

/* Per-channel saturating add of two BGR555 colours: min(a + b, 31). The
 * carry out of each 5-bit channel is detected in parallel (bits 0x8420) and
 * widened to a 0x1F channel mask. */
static inline u32 bgr555_add_saturate(u32 a, u32 b) {
    u32 carry = (((a & b) << 1) + ((a ^ b) & 0x7BDE)) & 0x8420;
    u32 mask = ((((carry << 16) >> 21) + 0x3DEF) ^ 0x3DEF);
    return ((a + b) - mask) | mask;
}

/* Per-channel saturating subtract: max(b - a, 0), same parallel borrow trick. */
static inline u32 bgr555_sub_saturate(u32 a, u32 b) {
    u32 borrow = (((a & ~b) << 1) + ((~b ^ a) & 0x7BDE)) & 0x8420;
    u32 mask = ((((borrow << 16) >> 21) + 0x3DEF) ^ 0x3DEF);
    return (b | mask) - (a | mask);
}

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
IWRAM_CODE Next sub_3005EF0(void) {
    u32 color = R0;
    int subtract = R2 != 0;
    u8 *pal = GUEST_PTR(u8, R7);
    for (s32 off = (s32)R6 - 2; off >= 0; off -= 2) {
        u32 entry = PAL_ENTRY(pal, off);
        PAL_ENTRY(pal, off) = subtract ? bgr555_sub_saturate(color, entry)
                                       : bgr555_add_saturate(color, entry);
    }
    RETURN_TO_CALLER();
}

/* Effects 2/3: fade toward white / black with per-channel factors packed in
 * the effect colour (r0 = factor_r | factor_g << 5 | factor_b << 10). */
IWRAM_CODE Next sub_3005F78(void) {
    u32 fr = R0 & 0x1F, fg = (R0 >> 5) & 0x1F, fb = (R0 >> 10) & 0x1F;
    int darken = R2 == 12;
    u8 *pal = GUEST_PTR(u8, R7);
    for (s32 off = (s32)R6 - 2; off >= 0; off -= 2) {
        u32 entry = PAL_ENTRY(pal, off);
        PAL_ENTRY(pal, off) = darken ? bgr555_darken(entry, fr, fg, fb)
                                     : bgr555_lighten(entry, fr, fg, fb);
    }
    RETURN_TO_CALLER();
}
