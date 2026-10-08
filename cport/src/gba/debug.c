/*
 * debug.c — logging through mGBA's debug registers (ignored by hardware) and a
 * fatal-error stop that leaves a marker readable by test harnesses.
 */
#include "gba.h"
#include "runtime.h"

#define MGBA_DEBUG_ENABLE (*(volatile u16 *)0x04FFF780)
#define MGBA_DEBUG_FLAGS  (*(volatile u16 *)0x04FFF700)
#define MGBA_DEBUG_STRING ((volatile char *)0x04FFF600)

volatile u32 g_fatal_marker[4];

static void put_hex(volatile char *s, int *i, u32 v) {
    for (int k = 7; k >= 0; k--)
        s[(*i)++] = "0123456789ABCDEF"[(v >> (k * 4)) & 15];
}

void rt_log(const char *msg, u32 value) {
    MGBA_DEBUG_ENABLE = 0xC0DE;
    if (MGBA_DEBUG_ENABLE != 0x1DEA)
        return;
    volatile char *s = MGBA_DEBUG_STRING;
    int i = 0;
    while (*msg && i < 200)
        s[i++] = *msg++;
    s[i++] = ' ';
    s[i++] = '0';
    s[i++] = 'x';
    put_hex(s, &i, value);
    s[i] = 0;
    MGBA_DEBUG_FLAGS = 0x100 | 2;   /* level: warn */
}

void rt_fatal(const char *msg, u32 value) {
    REG_IME = 0;
    /* visible on hardware too: a magenta screen (display on, no layers) */
    REG16(0x05000000) = 0x7C1F;
    REG16(0x04000000) = 0;
    g_fatal_marker[0] = 0xDEADC0DEu;
    g_fatal_marker[1] = value;
    g_fatal_marker[2] = (u32)msg;
    g_fatal_marker[3] = cpu.r[14];
    rt_log(msg, value);
    for (;;)
        ;
}

#if defined(LIFT_TRACE)
static u32 g_mismatch_count;
void trace_ret_mismatch(u32 expected) {
    if (g_mismatch_count++ < 64) {
        rt_log("ret mismatch: expected", expected);
        rt_log("              got", cpu.ret);
    }
}
#endif

#if defined(LIFT_TRACE)
static u32 g_tail_reports;
void trace_tail_violation(u32 ra) {
    if (g_tail_reports++ < 16)
        rt_log("tail call from a callee assumed tail-free, returning to", ra);
}
#endif

#if defined(LIFT_TRACE)
static u32 g_sync_reports;
void trace_sync_violation(u32 ra, int which) {
    if (g_sync_reports++ < 16) {
        rt_log("sync violation after call returning to", ra);
        rt_log("                       kept register #", (u32)which);
    }
}
#endif
