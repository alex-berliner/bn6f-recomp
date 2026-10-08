/*
 * swi.c — the GBA BIOS services BN6 uses, implemented in portable C.
 *
 * Results match the BIOS (register outputs included) for the inputs the game
 * passes; see GBATEK "BIOS Functions".
 */
#include "cpu.h"
#include "runtime.h"

static void bios_div(void) {
    s32 num = (s32)R0, den = (s32)R1;
    if (den == 0) {
        /* The real BIOS loops for a very long time; mirror mGBA's results. */
        R0 = num < 0 ? (u32)-1 : 1u;
        R1 = (u32)num;
        R3 = 1;
        return;
    }
    if (den == -1 && num == (s32)0x80000000) {
        R0 = 0x80000000u;
        R1 = 0;
        R3 = 0x80000000u;
        return;
    }
    s32 q = num / den;
    R0 = (u32)q;
    R1 = (u32)(num % den);
    R3 = (u32)(q < 0 ? -q : q);
}

static void bios_sqrt(void) {
    u32 x = R0, r = 0, bit = 1u << 30;
    while (bit > x)
        bit >>= 2;
    while (bit) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    R0 = r;
}

static s32 arctan_poly(s32 i) {
    s32 a = -((i * i) >> 14);
    s32 b = ((0xA9 * a) >> 14) + 0x390;
    b = ((b * a) >> 14) + 0x91C;
    b = ((b * a) >> 14) + 0xFB6;
    b = ((b * a) >> 14) + 0x16AA;
    b = ((b * a) >> 14) + 0x2081;
    b = ((b * a) >> 14) + 0x3651;
    b = ((b * a) >> 14) + 0xA2F9;
    return (i * b) >> 16;
}

static void bios_arctan2(void) {
    s32 x = (s32)R0, y = (s32)R1;
    s32 r;
    if (y == 0) {
        r = x >= 0 ? 0 : 0x8000;
    } else if (x == 0) {
        r = y >= 0 ? 0x4000 : 0xC000;
    } else if (y >= 0) {
        if (x >= 0 && x >= y)
            r = arctan_poly((y << 14) / x);
        else if (x < 0 && -x >= y)
            r = arctan_poly((y << 14) / x) + 0x8000;
        else
            r = 0x4000 - arctan_poly((x << 14) / y);
    } else {
        if (x <= 0 && -x > -y)
            r = arctan_poly((y << 14) / x) + 0x8000;
        else if (x > 0 && x >= -y)
            r = arctan_poly((y << 14) / x) + 0x10000;
        else
            r = 0xC000 - arctan_poly((x << 14) / y);
    }
    R0 = (u32)r & 0xFFFF;
    R3 = 0x170;
}

static void bios_cpuset(void) {
    u32 src = R0, dst = R1, ctl = R2;
    u32 count = ctl & 0x1FFFFF;
    int fill = (ctl >> 24) & 1;
    if ((ctl >> 26) & 1) {
        src &= ~3u;
        dst &= ~3u;
        if (fill) {
            u32 v = LD32(src);
            for (u32 i = 0; i < count; i++)
                ST32(dst + i * 4, v);
        } else {
            for (u32 i = 0; i < count; i++)
                ST32(dst + i * 4, LD32(src + i * 4));
        }
    } else {
        src &= ~1u;
        dst &= ~1u;
        if (fill) {
            u32 v = LD16(src);
            for (u32 i = 0; i < count; i++)
                ST16(dst + i * 2, v);
        } else {
            for (u32 i = 0; i < count; i++)
                ST16(dst + i * 2, LD16(src + i * 2));
        }
    }
}

static void bios_cpufastset(void) {
    u32 src = R0 & ~3u, dst = R1 & ~3u, ctl = R2;
    u32 count = ((ctl & 0x1FFFFF) + 7) & ~7u;
    if ((ctl >> 24) & 1) {
        u32 v = LD32(src);
        for (u32 i = 0; i < count; i++)
            ST32(dst + i * 4, v);
    } else {
        for (u32 i = 0; i < count; i++)
            ST32(dst + i * 4, LD32(src + i * 4));
    }
}

static void bios_lz77(int vram) {
    u32 src = R0, dst = R1;
    u32 header = LD32(src);
    u32 remaining = header >> 8;
    u32 out = dst;
    u32 half = 0;            /* VRAM: pending low byte of a halfword */
    src += 4;
    while (remaining) {
        u32 flags = LD8(src++);
        for (int bit = 0; bit < 8 && remaining; bit++, flags <<= 1) {
            if (flags & 0x80) {
                u32 b0 = LD8(src), b1 = LD8(src + 1);
                src += 2;
                u32 len = (b0 >> 4) + 3;
                u32 disp = (((b0 & 0xF) << 8) | b1) + 1;
                for (; len && remaining; len--, remaining--) {
                    u32 from = out - disp;
                    u32 v;
                    if (vram && (out & 1) && from == out - 1)
                        v = half;
                    else
                        v = LD8(from);
                    if (vram) {
                        if (out & 1) {
                            ST16(out - 1, half | (v << 8));
                        } else {
                            half = v;
                        }
                    } else {
                        ST8(out, v);
                    }
                    out++;
                }
            } else {
                u32 v = LD8(src++);
                if (vram) {
                    if (out & 1)
                        ST16(out - 1, half | (v << 8));
                    else
                        half = v;
                } else {
                    ST8(out, v);
                }
                out++;
                remaining--;
            }
        }
    }
    if (vram && (out & 1))
        ST16(out - 1, half);
}

void swi_call(u32 n) {
#if defined(LIFT_TRACE)
    if (n == 0x0B || n == 0x0C) {
        rt_log("cpuset src", R0); rt_log("       dst", R1);
        rt_log("       ctl", R2);
        rt_log("        lr", LR);
    }
#endif
    switch (n) {
    case 0x06: bios_div(); break;
    case 0x08: bios_sqrt(); break;
    case 0x0A: bios_arctan2(); break;
    case 0x0B: bios_cpuset(); break;
    case 0x0C: bios_cpufastset(); break;
    case 0x11: bios_lz77(0); break;
    case 0x12: bios_lz77(1); break;
    default: rt_fatal("unimplemented SWI", n);
    }
}
