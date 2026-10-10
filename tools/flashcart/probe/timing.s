.section .text.start, "ax"
.arm
.global _start
_start:
    mov r0, #0x1F
    msr cpsr_c, r0
    ldr sp, =0x03007F00
    ldr r0, =__iwram_lma        @ copy the IWRAM routines
    ldr r1, =__iwram_start
    ldr r2, =__iwram_end
1:  cmp r1, r2
    ldrlo r3, [r0], #4
    strlo r3, [r1], #4
    blo 1b
    ldr r0, =main
    bx r0
.ltorg
