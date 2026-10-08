/*
 * irq.c — native IRQ entry.
 *
 * The BIOS calls this (ARM, IRQ mode) through 0x03007FFC with r0-r3, r12 and
 * lr already saved. It runs the lifted IRQ dispatcher sub_3005B00 on a
 * snapshot of the guest register file, exactly as the BIOS would enter the
 * original handler, and restores the interrupted guest state afterwards.
 */
#include "gba.h"
#include "lifted.h"
#include "runtime.h"

#if defined(LIFT_TRACE)
void trace_event(u32 kind, u32 addr);
#else
#define trace_event(k, a) ((void)0)
#endif

/* Word copy of the guest register file (a struct assignment would call
 * newlib's memcpy in ROM). */
static inline __attribute__((always_inline)) void copy_cpu(Cpu *dst, const Cpu *src) {
    _Static_assert(sizeof(Cpu) % 16 == 0, "Cpu is copied in 16-byte groups");
    u32 *d = (u32 *)dst;
    const u32 *s = (const u32 *)src;
    for (unsigned i = 0; i < sizeof(Cpu) / 16; i++)
        __asm__ volatile("ldmia %[s]!, {r0-r3}\n\tstmia %[d]!, {r0-r3}"
                         : [s] "+r"(s), [d] "+r"(d) : : "r0", "r1", "r2", "r3", "memory");
}

/* In IWRAM like the game's own handler: it runs a few times per frame. */
__attribute__((section(".iwram"), target("arm"), noinline)) void irq_entry(void) {
    trace_event(4, REG32(0x04000200));
    Cpu saved;
    copy_cpu(&saved, &cpu);
    u32 old = cpu_get_cpsr();
    cpu_switch_mode(0x92);        /* guest IRQ mode, IRQs masked, ARM state */
    cpu.spsr = old;
    R0 = 0x04000000u;
    LR = 0xFFFFFF00u;             /* "return to BIOS" */
    run(sub_3005B00);
    copy_cpu(&cpu, &saved);
    trace_event(5, 0);
}
