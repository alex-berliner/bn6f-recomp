/*
 * difftest_gen.c — structured inputs for functions whose arguments are
 * whole data structures (random bytes would only exercise the error paths).
 */
#include <string.h>
#include "cpu.h"

typedef void (*GenFn)(Cpu *in, u32 (*rnd)(void));

#define U8(a)  (*(u8 *)(uintptr_t)(u32)(a))
#define U32(a) (*(u32 *)(uintptr_t)(u32)(a))

/* m4a mixer: a SoundInfo with random but valid channels and waves, entered
 * the way SoundMain enters it (frame on the guest stack). */
static void gen_mixer(Cpu *in, u32 (*rnd)(void)) {
    u32 info = 0x02010000u + (rnd() % 0x100) * 4;   /* EWRAM, word aligned */
    static const u32 samples_tab[] = { 96, 112, 128, 160, 176, 192, 224, 304 };
    u32 samples = samples_tab[rnd() % 8];
    u8 max_chans = (u8)(1 + rnd() % 12);
    U8(info + 0x05) = rnd() % 3 ? 0 : (u8)rnd();     /* reverb */
    U8(info + 0x06) = max_chans;
    U8(info + 0x07) = (u8)(rnd() % 3 ? rnd() % 16 : rnd());
    U32(info + 0x18) = rnd() % 0x40;                  /* divFreq */
    u32 dma = 1 + rnd() % 4;
    U8(info + 0x04) = (u8)dma;
    for (u32 c = 0; c < max_chans; c++) {
        u32 ch = info + 0x50 + c * 0x40;
        static const u8 status_tab[] = { 0x80, 0xC0, 0x83, 0x82, 0x81, 0x03, 0x02, 0x01,
                                         0x43, 0x42, 0x07, 0x13, 0x12, 0x00, 0x04, 0x44 };
        U8(ch + 0x00) = status_tab[rnd() % 16] | (rnd() % 4 ? 0 : 0x10);
        U8(ch + 0x01) = rnd() % 2 ? 0x08 : 0x00;     /* fixed / resampled */
        u32 wav = 0x02020000u + (rnd() % 0x400) * 4;
        u32 size = 1 + rnd() % 0x300;
        U32(wav + 0x08) = rnd() % size;               /* loop start */
        U32(wav + 0x0C) = size;
        U8(wav + 0x03) = rnd() % 2 ? 0x40 : 0;        /* looped */
        U32(ch + 0x24) = wav;
        U32(ch + 0x18) = 1 + rnd() % size;            /* count */
        U32(ch + 0x28) = wav + 0x10 + (size - U32(ch + 0x18));
        U32(ch + 0x1C) = rnd() & 0x7FFFFF;            /* fw */
        U32(ch + 0x20) = rnd() % 0x80000;             /* frequency */
        U8(ch + 0x0D) = (u8)(rnd() % 4);              /* echo length */
    }
    u32 sp = in->r[13];
    U32(sp + 0x14) = rnd() % 4 ? 0 : 160 + rnd() % 228;   /* scanline budget */
    U32(sp + 0x18) = info;
    u32 buf = info + 0x350 + (rnd() % 2) * samples;
    U32(sp + 0x08) = buf;
    in->r[0] = info;
    in->r[4] = dma;
    in->r[5] = buf;
    in->r[6] = 0x630;
    in->r[8] = samples;
}

GenFn difftest_generator(const char *name) {
    if (!strcmp(name, "SoundMainRAM_iwram_03005700")) return gen_mixer;
    return 0;
}
