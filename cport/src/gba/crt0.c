/*
 * crt0.c — cartridge entry. The ROM header's first instruction is patched to
 * branch here (tools/mkrom.py). Sets up the host C stacks in the IWRAM block
 * the original game used for its copied routines, then enters gba_boot().
 */
#include "gba.h"

extern char __sys_stack_top[], __irq_stack_top[];

__attribute__((naked, target("arm"), section(".text.crt0"))) void _start(void) {
    __asm__ volatile(
        "mov r0, #0x12\n"          /* IRQ mode */
        "msr cpsr_c, r0\n"
        "ldr sp, =__irq_stack_top\n"
        "mov r0, #0x1f\n"          /* System mode, IRQs still masked by IME */
        "msr cpsr_c, r0\n"
        "ldr sp, =__sys_stack_top\n"
        "ldr r0, =gba_boot\n"
        "bx r0\n"
        ".ltorg\n");
}
