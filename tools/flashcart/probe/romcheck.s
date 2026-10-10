.section .text.start, "ax"
.arm
.global _start
_start:
    mov r0, #0x1F
    msr cpsr_c, r0
    ldr sp, =0x03007F00
    ldr r0, =main
    bx r0
.ltorg
