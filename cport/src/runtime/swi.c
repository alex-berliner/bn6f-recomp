/*
 * swi.c — the GBA BIOS services BN6 uses.
 *
 * On the GBA target they are serviced by the real BIOS. The portable C
 * versions (used off-hardware, or with -DSWI_IN_C) match the BIOS results,
 * register outputs included, for the inputs the game passes; see GBATEK
 * "BIOS Functions".
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

/* The BIOS's Newton iteration (BIOS 0x404, ARM), register for register:
 * r1 and r3 keep its last estimate and quotient, which callers can see. */
static void bios_sqrt(void) {
    u32 x = R0, r0 = x, r1 = 1, r2, r3, r4;
    while (r0 > r1) {
        r0 >>= 1;
        r1 <<= 1;
    }
    do {
        r0 = x;
        r4 = r1;
        r3 = 0;
        r2 = r1;
        for (;;) {                     /* cmp r2, r0, lsr #1; lslls r2, #1; bcc */
            int lower = r2 < (r0 >> 1);
            if (r2 <= (r0 >> 1))
                r2 <<= 1;
            if (!lower)
                break;
        }
        for (;;) {                     /* r3 = r0 / r1, one bit at a time */
            u32 c = r0 >= r2;
            r3 = r3 * 2 + c;
            if (c)
                r0 -= r2;
            if (r2 == r1)
                break;
            r2 >>= 1;
        }
        r1 = (r1 + r3) >> 1;
    } while (r1 < r4);
    R0 = r4;
    R1 = r1;
    R3 = r3;
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

/* The BIOS refuses a source (or source end) in the BIOS region; a count of 0
 * does nothing (BIOS 0xBA4). */
static int bios_source_ok(u32 src, u32 bytes) {
    if (bytes == 0)
        return 0;
    return (src & 0x0E000000u) && ((src + (bytes & 0x01FFFFFFu)) & 0x0E000000u);
}

/* CpuSet (BIOS 0xB4C, Thumb). 32-bit mode advances r0 and r1 (ldmia/stmia);
 * the routine returns through pop {r3}, which leaves r3 = 0x170, the BIOS
 * SWI handler's return point. */
static void bios_cpuset(void) {
    u32 src = R0, dst = R1, ctl = R2;
    u32 count = ctl & 0x1FFFFF;
    int fill = (ctl >> 24) & 1;
    if (bios_source_ok(src, count * 4)) {
        if ((ctl >> 26) & 1) {
            if (fill) {
                u32 v = LD32(src & ~3u);
                src += 4;
                for (u32 i = 0; i < count; i++, dst += 4)
                    ST32(dst & ~3u, v);
            } else {
                for (u32 i = 0; i < count; i++, src += 4, dst += 4)
                    ST32(dst & ~3u, LD32(src & ~3u));
            }
            R0 = src;
            R1 = dst;
        } else {
            u32 s16 = src & ~1u, d16 = dst & ~1u;
            if (fill) {
                u32 v = LD16(s16);
                for (u32 i = 0; i < count; i++)
                    ST16(d16 + i * 2, v);
            } else {
                for (u32 i = 0; i < count; i++)
                    ST16(d16 + i * 2, LD16(s16 + i * 2));
            }
        }
    }
    R3 = 0x170;
}

/* CpuFastSet (BIOS 0xBC4, ARM): 8 words at a time; r0 (copy only) and r1
 * advance; r3 is the second word of the last block (copy) or the fill
 * value. */
static void bios_cpufastset(void) {
    u32 src = R0, dst = R1, ctl = R2;
    u32 bytes = (ctl & 0x1FFFFF) * 4;
    if (!bios_source_ok(src, bytes))
        return;
    u32 end = dst + bytes;
    if ((ctl >> 24) & 1) {
        u32 v = LD32(src & ~3u);
        for (; (s32)(dst - end) < 0; dst += 32)
            for (u32 k = 0; k < 8; k++)
                ST32((dst & ~3u) + k * 4, v);
        R3 = v;
    } else {
        for (; (s32)(dst - end) < 0; dst += 32, src += 32) {
            for (u32 k = 0; k < 8; k++)
                ST32((dst & ~3u) + k * 4, LD32((src & ~3u) + k * 4));
            R3 = LD32((src & ~3u) + 4);
        }
        R0 = src;
    }
    R1 = dst;
}

/* LZ77UnCompWram (BIOS 0x10FC, ARM), step for step: a back-reference is
 * copied whole even past the declared size; r0 and r1 end after the last
 * byte read and written, r3 is 0 after any back-reference. */
static void bios_lz77_wram(void) {
    u32 src = R0, dst = R1;
    s32 remaining = (s32)(LD32(src & ~3u) >> 8);
    src += 4;
    if (bios_source_ok(src, (u32)remaining)) {
        while (remaining > 0) {
            u32 flags = LD8(src++);
            for (int k = 0; k < 8 && remaining > 0; k++, flags <<= 1) {
                if (flags & 0x80) {
                    u32 b0 = LD8(src++), b1 = LD8(src++);
                    s32 len = (s32)(b0 >> 4) + 3;
                    u32 disp = (((b0 & 0xF) << 8) | b1) + 1;
                    remaining -= len;
                    for (; len > 0; len--, dst++)
                        ST8(dst, LD8(dst - disp));
                    R3 = 0;
                } else {
                    ST8(dst++, LD8(src++));
                    remaining--;
                }
            }
        }
    }
    R0 = src;
    R1 = dst;
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

#if defined(TARGET_GBA) && !defined(SWI_IN_C)
/* On hardware the game's SWIs go to the real BIOS, exactly as the original
 * code did. The guest r0-r3 are passed in and read back. */
#define BIOS_SWI(num)                                                          \
    do {                                                                       \
        register u32 a0 __asm__("r0") = R0, a1 __asm__("r1") = R1;             \
        register u32 a2 __asm__("r2") = R2, a3 __asm__("r3") = R3;             \
        __asm__ volatile("swi " #num                                           \
                         : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3)              \
                         : : "r12", "memory", "cc");                           \
        R0 = a0; R1 = a1; R2 = a2; R3 = a3;                                    \
    } while (0)

void swi_call(u32 n) {
#if defined(LIFT_TRACE)
    if (n == 0x0B || n == 0x0C) {
        rt_log("cpuset src", R0); rt_log("       dst", R1);
        rt_log("       ctl", R2);
        rt_log("        lr", LR);
    }
#endif
    switch (n) {
    case 0x06: BIOS_SWI(0x06); break;
    case 0x08: BIOS_SWI(0x08); break;
    case 0x0A: BIOS_SWI(0x0A); break;
    case 0x0B: BIOS_SWI(0x0B); break;
    case 0x0C: BIOS_SWI(0x0C); break;
    case 0x11: BIOS_SWI(0x11); break;
    case 0x12: BIOS_SWI(0x12); break;
    default: rt_fatal("unimplemented SWI", n);
    }
}
#else
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
    case 0x11: bios_lz77_wram(); break;
    case 0x12: bios_lz77(1); break;
    default: rt_fatal("unimplemented SWI", n);
    }
}
#endif
