/*
 * cpu.c — guest CPU state, mode banking and dynamic dispatch.
 */
#include "cpu.h"
#include "dispatch.h"
#include "runtime.h"

Cpu cpu;

#if defined(LIFT_TRACE)
/* Last 64 control events (kind << 28 | low 28 bits of address). */
volatile u32 g_trace_ring[64];
volatile u32 g_trace_pos;
void trace_event(u32 kind, u32 addr) {
    g_trace_ring[g_trace_pos++ & 63] = (kind << 28) | (addr & 0x0FFFFFFF);
}
extern char __bss_end[];
#define __bss_end_sym __bss_end
void trace_run(GuestFn f) {
    u32 sp = (u32)__builtin_frame_address(0);
    trace_event(6, (u32)f);
    if (sp < (u32)__bss_end_sym + 64)
        rt_fatal("host stack overflow", (u32)f);
}
#else
#define trace_event(k, a) ((void)0)
#endif

/* ── CPSR / mode banking ─────────────────────────────────────────────── */

static int bank_of(u32 mode) {
    switch (mode & 0x1F) {
    case 0x11: return BANK_FIQ;
    case 0x12: return BANK_IRQ;
    case 0x13: return BANK_SVC;
    case 0x17: return BANK_ABT;
    case 0x1B: return BANK_UND;
    default:   return BANK_USR;   /* 0x10 user, 0x1F system */
    }
}

u32 cpu_get_cpsr(void) {
    u32 n = (s32)cpu.nz < 0;
    u32 z = cpu.nz == 0;
    return (n << 31) | (z << 30) | ((cpu.c & 1) << 29) | ((cpu.v & 1) << 28) | (cpu.cpsr_ctl & 0xFF);
}

static void set_flags_from(u32 v) {
    u32 n = v >> 31, z = (v >> 30) & 1;
    /* N and Z share storage; N=1,Z=1 is not producible by an ALU result. */
    cpu.nz = z ? 0 : (n ? 0x80000000u : 1u);
    cpu.c = (v >> 29) & 1;
    cpu.v = (v >> 28) & 1;
}

void cpu_switch_mode(u32 new_ctl) {
    int ob = bank_of(cpu.cpsr_ctl), nb = bank_of(new_ctl);
    if (ob != nb) {
        cpu.bank_sp[ob] = cpu.r[13];
        cpu.bank_lr[ob] = cpu.r[14];
        cpu.bank_spsr[ob] = cpu.spsr;
        cpu.r[13] = cpu.bank_sp[nb];
        cpu.r[14] = cpu.bank_lr[nb];
        cpu.spsr = cpu.bank_spsr[nb];
    }
    cpu.cpsr_ctl = new_ctl & 0xFF;
}

void cpu_set_cpsr(u32 v, u32 mask) {
    if (mask & 8)
        set_flags_from(v);
    if (mask & 1)
        cpu_switch_mode(v & 0xFF);
}

u32 cpu_get_spsr(void) { return cpu.spsr; }

void cpu_set_spsr(u32 v, u32 mask) {
    u32 m = 0;
    if (mask & 1) m |= 0x000000FFu;
    if (mask & 2) m |= 0x0000FF00u;
    if (mask & 4) m |= 0x00FF0000u;
    if (mask & 8) m |= 0xFF000000u;
    cpu.spsr = (cpu.spsr & ~m) | (v & m);
}

/* ── dispatch ────────────────────────────────────────────────────────── */

#if defined(TARGET_GBA)
#define HOT __attribute__((section(IWRAM_SECTION), target("arm"), noinline))
#else
#define HOT
#endif

#if defined(LIFT_TRACE)
void trace_run(GuestFn f);
#endif

HOT void run(GuestFn f) {
    Next n = { f };
    while (n.fn) {
#if defined(LIFT_TRACE)
        trace_run(n.fn);
#endif
        n = n.fn();
    }
}

HOT GuestFn dispatch_lookup(u32 addr) {
    addr &= ~1u;
    GuestFn f = 0;
    if (addr - 0x08000000u < g_dispatch_rom_end - 0x08000000u)
        f = g_dispatch_rom[(addr - 0x08000000u) >> 1];
    else if (addr - 0x03000000u < 0x8000u)
        f = g_dispatch_iwram[(addr - 0x03000000u) >> 1];
    return f ? f : reloc_lookup(addr);
}

HOT void dispatch(u32 addr) {
    trace_event(1, addr);
    GuestFn f = dispatch_lookup(addr);
    if (!f)
        rt_fatal("dispatch miss", addr);
    run(f);
}

HOT Next jump_to(u32 addr) {
    trace_event(2, addr);
    GuestFn f = dispatch_lookup(addr);
    if (f)
        return NEXT(f);
    trace_event(3, addr);
    /* Not a known entry point: this is a return through a register (e.g. an
     * unusual epilogue). Unwind to the frame that owns the continuation. */
    cpu.ret = addr & ~1u;
    return DONE;
}

void undefined_insn(u32 addr) { rt_fatal("undefined instruction", addr); }

void exception_return(u32 addr) { rt_fatal("exception return", addr); }
