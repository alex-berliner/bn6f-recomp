/*
 * leaves_farm_31.c: small functions from the model farm, set 31.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── STWI_init_timer ── */
/* STWI_init_timer(r0 = callback slot, r1 = timer id):
 * Install STWI_intr_timer as the handler stored at *r0 and record the timer id
 * in the STWI work area (dword_2010CCC). Then enable the timer's IRQ bit in
 * REG_IE, briefly disabling interrupts (REG_IME) around the update so the new
 * bit can't fire mid-write.
 *
 * Results left for the caller:
 *   r2 = REG_IME value as it was before the update
 *   r3 = &REG_IME (InterruptMasterEnableRegister)
 * r4/r5 are clobbered internally but restored, so they stay as they came in. */
Next STWI_init_timer(void) {
    u32 slot = R0;
    u32 timerId = R1;

    /* Hook the timer interrupt handler into the caller's callback slot. */
    ST32(slot, A_STWI_intr_timer | 1);

    /* Save the timer id in the STWI work area. */
    u32 workSlot = A_dword_2010CCC;
    u32 work = LD32(workSlot);
    ST8(work + 0xA, timerId);

    /* Turn interrupts off while we update the enable mask. */
    u32 imeReg = A_InterruptMasterEnableRegister;
    u32 savedIme = LD16(imeReg);
    ST16(imeReg, 0);

    /* Set the IRQ enable bit for the selected timer (bit 3 << timer id),
     * keeping any enables that were already requested. */
    u32 ieReg = A_InterruptEnableRegister;
    u32 idByte = LD8(work + 0xA);          /* reread the id just stored */
    u32 timerBit = LSL_REG(8, idByte & 0xFF);
    ST16(ieReg, timerBit | LD16(ieReg));

    /* Restore the previous interrupt master enable state. */
    ST16(imeReg, savedIme);

    R2 = savedIme;
    R3 = imeReg;
    RETURN_TO_CALLER();
}
