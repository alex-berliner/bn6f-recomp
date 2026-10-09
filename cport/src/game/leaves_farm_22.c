/*
 * leaves_farm_22.c: small functions from the model farm, set 22.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_809FBF2 ── */
/* sub_809FBF2(r0 = map group id, r1 = base pointer):
 * Resolve the pointer for the requested map group. Group 5 (the mystery
 * data groups) uses a table of 12-byte entries and here always selects
 * entry 0; every other group passes the base pointer through unchanged.
 * Outputs: r0 = table pointer. */
Next sub_809FBF2(void) {
    u32 base = R1;

    if (R0 == 5) {
        /* Offset of entry 0 within a 12-byte-entry table: base + 0*8 + 0*4. */
        u32 entryIndex = 0;
        R0 = base + entryIndex * 12;
    } else {
        R0 = base;
    }

    RETURN_TO_CALLER();
}
