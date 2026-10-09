/*
 * sound_lib_setup.c — m4a, the GBA MusicPlayer2000 sound library, and its
 * setup code (bn6f libs.s, 0x0814DA9C-0x0814F00C).
 *
 * The library keeps its state in a shared info block reached through the
 * pointer word at 0x3007FF0. That block is m4a's SoundInfo: its ident, the
 * PCM DMA counter, the playback rate index (m4a: freq) and the constants
 * m4a derives from it (pcmDmaPeriod, pcmSamplesPerVBlank, pcmFreq,
 * divFreq), followed by the PCM double buffer.
 *
 * SampleFreqSet_814EDC0 picks a sound id's rate group, loads that group's
 * samples-per-vblank from m4a's rate table, derives the driver constants
 * from it and ends in m4aSoundVSyncOn_814EF94. m4aSoundVSyncOff_814EF14
 * takes the PCM DMA back off: it releases the block's ident, stops Timer 0,
 * stops the repeating PCM DMAs, switches both PCM DMA controls to 32-bit
 * transfers and zeroes the PCM buffer. m4aSoundVSyncOn_814EF94 turns it all
 * back on: it re-enables the PCM DMAs, clears their counter, takes the ident
 * back and reloads Timer 0 so that it starts at the beginning of line 159.
 *
 * divsi3_814DA9C is libgcc's __divsi3 — the ROM divides through this one
 * helper on every side of the setup code, so it is kept here as its own
 * function; a zero divisor calls the __div0 nullsub and returns 0.
 */
#include "game.h"
#include "runtime.h"
#include <stdbool.h>

/* m4a's SoundInfo layout (bn6f libs.s), not one of the game's structs. */
enum {
    SOUND_INFO_IDENT                  = 0x00,
    SOUND_INFO_PCM_DMA_COUNTER        = 0x04, /* u8 */
    SOUND_INFO_FREQ                   = 0x08, /* the rate index, 1-12 */
    SOUND_INFO_PCM_DMA_PERIOD         = 0x0B, /* u8 */
    SOUND_INFO_PCM_SAMPLES_PER_VBLANK = 0x10,
    SOUND_INFO_PCM_FREQ               = 0x14,
    SOUND_INFO_DIV_FREQ               = 0x18,
    SOUND_INFO_PCM_BUFFER             = 0x350,
};

/* m4a's own numbers, as the setup code uses them. */
#define SOUND_INFO_PTR         A_byte_3007FF0   /* the word holding the SoundInfo pointer */
#define ID_NUMBER              ((u32)0x68736D53)  /* m4a's ident: 'Smsh' */
#define ID_STEP                ((u32)10)          /* how far the ident moves off and back on */
#define ID_TEST_ADDEND         ((u32)0x978C92AD)  /* -ID_NUMBER: the tag test sums this in */

#define PCM_RATE_TABLE         A_byte_81C058C  /* m4a's pcmSamplesPerVBlank table */
#define PCM_DMA_BUF_SIZE       ((u32)1584)     /* the bytes one PCM buffer holds */
#define PCM_FREQ_NUMERATOR     ((u32)597275)   /* 0x91D1B; with the two below this is */
#define PCM_FREQ_ROUNDING      ((u32)5000)     /* 0x1388  pcmFreq = (597275 * samples */
#define PCM_FREQ_DIVISOR       ((u32)10000)    /* 0x2710      + 5000) / 10000 */
#define TIMER_CLOCK            ((u32)16777216) /* 0x1000000: the timers' 16.78 MHz clock */
#define TIMER_RELOAD_NUMERATOR ((u32)280896)   /* 0x44940: m4a's reload numerator */
#define VCOUNT_START_LINE      ((u8)159)       /* where Timer 0 is restarted */

/* the DMA words m4a writes while taking its two PCM DMAs down and back up */
#define DMA_ACTIVE_BIT         ((u32)0x02000000)
#define DMA_PAUSE_WORD         ((u32)0x84400004)
#define DMA_32BIT_CONTROL      ((u16)0x0400)
#define DMA_RESTART_CONTROL    ((u16)0xB600)

#define SWI_CPU_SET  0x0B               /* CpuSet, used here as CpuFill32 */
#define CPU_FILL_32BIT        ((u32)0x5000318)  /* mode: 32-bit, source fixed, 0x318 words */

/* ── divsi3 ──────────────────────────────────────────────────────────── */

/* The ROM's __divsi3 leaves its division loop's state behind next to the
 * quotient: r1 (the shifted divisor), r2 (the quotient before the sign),
 * r3 (the loop's scale) and r12 (numerator ^ denominator, the sign). */
typedef struct {
    s32 quotient;
    u32 r1, r2, r3, r12;
} Division;

static Division divide_signed(s32 numerator, s32 denominator) {
    u32 dividend = (u32)numerator;
    u32 divisor = (u32)denominator;
    u32 sign = dividend ^ divisor;
    u32 magnitude = 0;   /* the quotient before its sign: r2 */
    u32 scale = 1;

    if (denominator == 0) {
        /* __div0: the ROM calls its nullsub and returns 0; the loop registers
         * keep whatever the caller had in them */
        return (Division){ .quotient = 0, .r1 = 0, .r2 = R2, .r3 = R3, .r12 = R12 };
    }

    if ((s32)divisor < 0)
        divisor = 0u - divisor;
    if ((s32)dividend < 0)
        dividend = 0u - dividend;

    if (dividend < divisor) {
        /* the quotient is 0: the original jumps straight to the sign */
        return (Division){ .quotient = 0, .r1 = divisor, .r2 = 0, .r3 = scale, .r12 = sign };
    }

    /* normalize the divisor to just below the dividend, by fours first, then
     * one bit at a time; `scale` counts the quotient bits alongside */
    u32 limit = 1u << 28;
    while (divisor < limit && divisor < dividend) {
        divisor <<= 4;
        scale <<= 4;
    }
    limit <<= 3;   /* 1 << 31 */
    while (divisor < limit && divisor < dividend) {
        divisor <<= 1;
        scale <<= 1;
    }

    for (;;) {
        /* one restoring-division round, at 1, 1/2, 1/4 and 1/8 of the
         * divisor at once */
        if (dividend >= divisor) {
            dividend -= divisor;
            magnitude |= scale;
        }
        if (dividend >= divisor >> 1) {
            dividend -= divisor >> 1;
            magnitude |= scale >> 1;
        }
        if (dividend >= divisor >> 2) {
            dividend -= divisor >> 2;
            magnitude |= scale >> 2;
        }
        if (dividend >= divisor >> 3) {
            dividend -= divisor >> 3;
            magnitude |= scale >> 3;
        }
        if (dividend == 0)
            break;
        scale >>= 4;
        if (scale == 0)
            break;
        divisor >>= 4;
    }

    s32 quotient = (s32)magnitude;
    if ((s32)sign < 0)
        quotient = (s32)(0u - magnitude);
    return (Division){ .quotient = quotient, .r1 = divisor, .r2 = magnitude, .r3 = scale, .r12 = sign };
}

/* __divsi3's flags: its closing `cmp r4, #0` on the sign, or the
 * `neg r0, r0` that applies it. */
static void division_flags(const Division *division) {
    if ((s32)division->r12 < 0)
        SET_FLAGS_CMP(0, division->r2);   /* neg r0, r0 */
    else
        SET_FLAGS_CMP(division->r12, 0);  /* cmp r4, #0 */
}

Next divsi3_814DA9C(void) {
    s32 numerator = (s32)R0, denominator = (s32)R1;
    Division division = divide_signed(numerator, denominator);

    R0 = (u32)division.quotient;
    R1 = division.r1;
    R2 = division.r2;
    R3 = division.r3;
    R12 = division.r12;

    if (denominator == 0)
        SET_FLAGS_CMP(0, 0);              /* the opening cmp r1, #0 */
    else
        division_flags(&division);
    RETURN_TO_CALLER();
}

/* ── taking the PCM DMA off ──────────────────────────────────────────── */

/* What m4aSoundVSyncOff leaves in r2/r3: the SoundInfo pointer and the tag
 * test constant when the block was already offline, else whatever the CpuSet
 * BIOS call left. */
typedef struct { u32 r2, r3; } VsyncOffRegisters;

static VsyncOffRegisters sound_vsync_off(void) {
    u32 info = LD32(SOUND_INFO_PTR);
    u32 ident = LD32(info + SOUND_INFO_IDENT);

    if ((u32)(ident + ID_TEST_ADDEND) > 1u)  /* neither ID_NUMBER nor ID_NUMBER + 1 */
        return (VsyncOffRegisters){ .r2 = info, .r3 = ID_TEST_ADDEND };

    ST32(info + SOUND_INFO_IDENT, ident + ID_STEP);

    ST16(A_Timer0Control, 0);                    /* stop Timer 0 */

    if (LD32(A_DMA1WordCount) & DMA_ACTIVE_BIT)  /* still repeating: reload the word */
        ST32(A_DMA1WordCount, DMA_PAUSE_WORD);
    if (LD32(A_DMA2WordCount) & DMA_ACTIVE_BIT)
        ST32(A_DMA2WordCount, DMA_PAUSE_WORD);

    ST16(A_DMA1Control, DMA_32BIT_CONTROL);      /* both controls: 32-bit transfers */
    ST16(A_DMA1Control + 0xC, DMA_32BIT_CONTROL);

    u32 zero = SP -= 4;                          /* the word the fill reads over and over */
    ST32(zero, 0);

    R0 = zero;
    R1 = info + SOUND_INFO_PCM_BUFFER;
    R2 = CPU_FILL_32BIT;
    R3 = 0x400;                                  /* as the original: its shifted control word */
    swi_call(SWI_CPU_SET);                       /* CpuFill32 zero over pcmBuffer */

    VsyncOffRegisters left = { .r2 = R2, .r3 = R3 };
    SP += 4;
    return left;
}

Next m4aSoundVSyncOff_814EF14(void) {
    VsyncOffRegisters left = sound_vsync_off();
    R2 = left.r2;
    R3 = left.r3;
    RETURN_TO_CALLER();
}

/* ── turning the PCM DMA back on ─────────────────────────────────────── */

/* What m4aSoundVSyncOn leaves in r1/r2/r3/r12 and the flags behind its two
 * possible endings, for its own callers and for SampleFreqSet_814EDC0, which ends in
 * it. */
typedef struct {
    u32 r1, r2, r3, r12;
    bool started;      /* Timer 0 was restarted, so the flags are that start's */
    u32 negated;       /* the reload before `neg r0, r0`, for C and V */
} VsyncOnRegisters;

static VsyncOnRegisters sound_vsync_on(u32 incoming_r1, u32 incoming_r12) {
    u32 info = LD32(SOUND_INFO_PTR);
    u32 ident = LD32(info + SOUND_INFO_IDENT);

    if (ident == ID_NUMBER) {
        /* already live: nothing to restart, and the registers survive as
         * they came in */
        return (VsyncOnRegisters){
            .r1 = incoming_r1, .r2 = info, .r3 = ident, .r12 = incoming_r12,
            .started = false, .negated = 0,
        };
    }

    /* restart the two PCM DMAs */
    ST16(A_DMA1Control, DMA_RESTART_CONTROL);
    ST16(A_DMA1Control + 0xC, DMA_RESTART_CONTROL);

    ST8(info + SOUND_INFO_PCM_DMA_COUNTER, 0);   /* start filling at the first buffer */
    ST32(info + SOUND_INFO_IDENT, ident - ID_STEP);

    /* m4a's timer timing: wait out the rest of line 159, then wait for line
     * 159 to come around again, so Timer 0 starts at its beginning */
    while (LD8(A_VerticalCounter_LY_) == VCOUNT_START_LINE)
        ;
    while (LD8(A_VerticalCounter_LY_) != VCOUNT_START_LINE)
        ;

    /* the reload: -(280896 / pcmSamplesPerVBlank) */
    Division reload =
        divide_signed((s32)TIMER_RELOAD_NUMERATOR,
                      (s32)LD32(info + SOUND_INFO_PCM_SAMPLES_PER_VBLANK));

    ST16(A_Timer0Counter_Reload, 0u - (u32)reload.quotient);
    ST16(A_Timer0Control, 0x80);                 /* start Timer 0 */

    return (VsyncOnRegisters){
        .r1 = A_Timer0Control, .r2 = reload.r2, .r3 = reload.r3, .r12 = reload.r12,
        .started = true, .negated = (u32)reload.quotient,
    };
}

/* m4aSoundVSyncOn's flags: the `cmp ident, ID_NUMBER` that found it already
 * live, else C and V from the reload's `neg r0, r0` and N and Z from the
 * `mov r0, #0x80` that starts Timer 0. */
static void vsync_on_flags(const VsyncOnRegisters *on) {
    if (on->started) {
        SET_FLAGS_CMP(0, on->negated);
        SET_FLAGS_NZ(0x80);
    } else {
        SET_FLAGS_CMP(ID_NUMBER, ID_NUMBER);
    }
}

Next m4aSoundVSyncOn_814EF94(void) {
    u32 return_address = LR;      /* the original returns through this popped word */
    u32 saved_r4 = R4;

    VsyncOnRegisters on = sound_vsync_on(R1, R12);

    R0 = return_address;
    R1 = on.r1;
    R2 = on.r2;
    R3 = on.r3;
    R4 = saved_r4;
    R12 = on.r12;
    vsync_on_flags(&on);
    RETURN_TO_CALLER();
}

/* ── a sound id's rate group and the constants derived from it ────────── */

static VsyncOnRegisters sample_freq_set(u32 sound_id) {
    sound_vsync_off();

    u32 info = LD32(SOUND_INFO_PTR);

    /* the rate group: bits 16-19 of the sound id */
    u32 freq = (sound_id & 0xF0000u) >> 16;
    ST8(info + SOUND_INFO_FREQ, freq);

    u32 samples_per_vblank = LD16(PCM_RATE_TABLE + (freq - 1u) * 2u);
    ST32(info + SOUND_INFO_PCM_SAMPLES_PER_VBLANK, samples_per_vblank);

    /* pcmDmaPeriod: how many vblanks one PCM buffer lasts */
    ST8(info + SOUND_INFO_PCM_DMA_PERIOD,
        (u8)divide_signed((s32)PCM_DMA_BUF_SIZE, (s32)samples_per_vblank).quotient);

    /* pcmFreq: the mix rate in hundredths of a hertz */
    Division mix =
        divide_signed((s32)(PCM_FREQ_NUMERATOR * samples_per_vblank + PCM_FREQ_ROUNDING),
                      (s32)PCM_FREQ_DIVISOR);
    ST32(info + SOUND_INFO_PCM_FREQ, (u32)mix.quotient);

    /* divFreq: half of the clock divided by the mix rate, rounded up */
    Division clock = divide_signed((s32)TIMER_CLOCK, mix.quotient);
    ST32(info + SOUND_INFO_DIV_FREQ, (u32)(((s32)clock.quotient + 1) >> 1));

    /* the original ends in m4aSoundVSyncOn with the registers this leaves */
    return sound_vsync_on(clock.r1, clock.r12);
}

Next SampleFreqSet_814EDC0(void) {
    u32 return_address = LR;      /* the original returns through this popped word */
    u32 saved_r4 = R4, saved_r5 = R5;

    VsyncOnRegisters on = sample_freq_set(R0);

    R0 = return_address;
    R1 = on.r1;
    R2 = on.r2;
    R3 = on.r3;
    R4 = saved_r4;
    R5 = saved_r5;
    R12 = on.r12;
    vsync_on_flags(&on);
    RETURN_TO_CALLER();
}
