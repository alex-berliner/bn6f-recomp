@ Cart probe: entry and startup (the header bytes are filled in by mkprobe.py)
.section .text.start, "ax"
.arm
.global _start
_start:
    b real_start
    .space 0xBC
real_start:                         @ 0x080000C0
    mov r0, #0x1F                   @ system mode, IRQs off
    msr cpsr_c, r0
    ldr sp, =0x03007F00
    ldr r0, =main
    bx r0
.ltorg
