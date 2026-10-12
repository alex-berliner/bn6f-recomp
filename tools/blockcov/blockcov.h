/*
 * blockcov.h: block coverage of the lifted originals in a host difftest build (tools/blockcov).
 *
 * Only a coverage build sees this file: tools/blockcov/blockcov.py builds a copy of cport whose
 * difftest_lifted*.c carry COV_HIT(ADDR) before every instruction of every L_NAME body, and compiles
 * everything with `-DDIFFTEST_COVERAGE -include blockcov.h`. The normal builds are untouched (the hooks
 * difftest.c calls are empty macros there).
 *
 * The lifted original runs in a forked child; what it reaches and what it loads goes into memory shared
 * with the parent, which keeps a trial's hits only when the trial is compared (in domain), as the
 * difftest decides, and prints them (`cov ...` lines on stdout).
 */
#pragma once
#include "cpu.h"

#define COV_MAX_HITS  (1u << 16)
#define COV_MAX_LOADS 8192u

typedef struct {
    u32 gen;                      /* trial number: a slot stamped with it was hit in this trial */
    u32 nhits, nloads;
    u32 logloads;                 /* the lifted original is running: log its loads */
    u32 hits[COV_MAX_HITS];       /* instruction addresses, in the order first reached */
    struct { u32 addr, size, value; } loads[COV_MAX_LOADS];
} CovShared;

extern CovShared *g_cov;
extern u32 *g_cov_stamp;          /* per instruction slot: the trial that last reached it */

/* ROM halfwords from 0x08000000 (8 MB), then IWRAM halfwords (32 KB) */
static inline u32 cov_slot(u32 a) {
    return a >= 0x08000000u ? ((a - 0x08000000u) >> 1) & 0x3FFFFFu : 0x400000u + (((a - 0x03000000u) >> 1) & 0x3FFFu);
}

#define COV_HIT(a) do { u32 s_ = cov_slot(a); \
        if (g_cov_stamp[s_] != g_cov->gen) { g_cov_stamp[s_] = g_cov->gen; \
            if (g_cov->nhits < COV_MAX_HITS) g_cov->hits[g_cov->nhits++] = (a); } } while (0)

static inline u32 cov_load(u32 a, u32 size, u32 v) {
    if (g_cov->logloads && g_cov->nloads < COV_MAX_LOADS && a >= 0x02000000u && a < 0x04000400u) {
        u32 n = g_cov->nloads++;
        g_cov->loads[n].addr = a;
        g_cov->loads[n].size = size;
        g_cov->loads[n].value = v;
    }
    return v;
}

/* cpu.h's host loads (TARGET_FLAT), with every load the original makes logged */
#undef LD32
#undef LD16
#undef LD8
#undef LDS16
#undef LDS8
#define LD32(a)  cov_load((u32)(a), 4, flat_ld32((u32)(a)))
#define LD16(a)  cov_load((u32)(a), 2, flat_ld16((u32)(a)))
#define LD8(a)   cov_load((u32)(a), 1, (u32)*(volatile u8 *)(uintptr_t)(u32)(a))
#define LDS16(a) cov_load((u32)(a), 2, ((u32)(a) & 1) ? (u32)(s32)*(volatile s8 *)(uintptr_t)(u32)(a) \
                                                      : (u32)(s32)*(volatile s16 *)(uintptr_t)(u32)(a))
#define LDS8(a)  cov_load((u32)(a), 1, (u32)(s32)*(volatile s8 *)(uintptr_t)(u32)(a))

/* the difftest's hooks (cport/host/difftest.c; empty there unless DIFFTEST_COVERAGE) */
void cov_trial_begin(void);
void cov_trial_end(const char *name, const char *label, int t);
#define COV_TRIAL_BEGIN()              cov_trial_begin()
#define COV_LIFTED(on)                 (g_cov->logloads = (on))
#define COV_TRIAL_END(name, label, t)  cov_trial_end((name), (label), (t))
