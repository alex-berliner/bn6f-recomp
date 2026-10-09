/*
 * leaves_farm_17.c: small functions from the model farm, set 17.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_810971A ── */
/* sub_810971A: pick a random object from the row selected by the current
 * alliance-dependent panel parameters and split its entry byte into two
 * fields:
 *   r0 = low 3 bits of the entry byte,
 *   r1 = upper 4 bits (bit 3 of the byte is discarded by the shifts),
 * with nz taken from the final r0.
 * If the row is empty, r0 stays 0 and the flags come from the count
 * comparison. r2/r3 are left as the helpers left them; r4-r7 preserved. */
Next sub_810971A(void) {
    u32 savedR7 = R7;

    /* The original pushes {r7,lr} and then reserves 8 more bytes; the list
     * of objects that object_getPanelsInRowFiltered fills in lives at SP. */
    SP -= 16;
    u32 rowObjects = SP;

    /* Work out which panel parameters apply for the attacker's alliance. */
    R3 = A_off_8109784;
    GUEST_CALL(GetAllianceDependentPanelParamArgs);

    /* Collect every object standing in that row into the stack buffer. */
    R7 = rowObjects;
    GUEST_CALL(object_getPanelsInRowFiltered);

    u32 count = R0;
    if (count == 0) {
        /* Nothing to choose from: r0 stays 0, flags from the comparison. */
        SET_FLAGS_CMP(R0, 0);
    } else {
        /* Roll a random number and reduce it modulo the row size. */
        R7 = count;
        GUEST_CALL(GetPositiveSignedRNG);

        R1 = count;
        swi_call(6); /* BIOS Div: r0 = rng / count, r1 = rng % count */

        u8 entry = LD8(rowObjects + R1);
        R1 = entry >> 4;  /* upper 4 bits of the entry */
        R0 = entry & 7;   /* low 3 bits (bit 3 is dropped) */
        SET_FLAGS_NZ(R0);
    }

    SP += 16;
    R7 = savedR7;
    RETURN_TO_CALLER();
}
