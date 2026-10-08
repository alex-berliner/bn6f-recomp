/*
 * irq.c — native IRQ entry.
 *
 * The BIOS calls irq_entry (ARM, IRQ mode) through 0x03007FFC with r0-r3,
 * r12 and lr already saved. It runs the lifted IRQ dispatcher sub_3005B00 on
 * a snapshot of the guest register file, exactly as the BIOS would enter the
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

/* Stack guard: boot.c paints the host stack, so its lowest 32 bytes keep
 * the pattern unless the stack has grown into them. Checked on every
 * interrupt; stopping here (rt_fatal logs and halts) beats the stack
 * silently overwriting .bss below it. */
extern char __bss_end[];
static inline __attribute__((always_inline)) void check_stack_guard(void) {
    const volatile u32 *g = (const volatile u32 *)(((u32)__bss_end + 3) & ~3u);
    for (int i = 0; i < 8; i++)
        if (g[i] != 0xA5A5A5A5u)
            rt_fatal("host stack overflow", (u32)&g[i]);
}

/* In IWRAM like the game's own handler: it runs a few times per frame. */
__attribute__((section(IWRAM_SECTION), target("arm"), noinline, used)) void irq_body(void) {
    trace_event(4, REG32(0x04000200));
    check_stack_guard();
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

/* Like the original handler, the work runs in system mode on the main
 * stack, so the port has one native stack instead of a second one for
 * interrupts. IRQs stay masked meanwhile (the guest handler's nested-IRQ
 * enable only changes the emulated CPSR), so at most one handler is on the
 * stack at a time. */
__attribute__((section(IWRAM_SECTION), target("arm"), naked)) void irq_entry(void) {
    __asm__ volatile(
        "mov r0, #0x9F\n\t"        /* system mode, IRQs masked */
        "msr cpsr_c, r0\n\t"
        "push {r4, lr}\n\t"        /* lr: the interrupted code's (r4 keeps 8-byte steps) */
        "bl irq_body\n\t"
        "pop {r4, lr}\n\t"
        "mov r0, #0x92\n\t"        /* back to IRQ mode, whose lr returns to the BIOS */
        "msr cpsr_c, r0\n\t"
        "bx lr\n\t");
}
