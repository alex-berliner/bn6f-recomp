/*
 * leaves_farm_33.c: small functions from the model farm, set 33.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── clearSetFlags_80355a8 ── */
/* clearSetFlags_80355a8: clear the five event flags listed below and set EVENT_120.
 * r4-r7 (and r10) are preserved; r0 returns 0.
 * Event flags are addressed as (word index in r0, bit mask in r1). */

static void clear_event_flag(u32 word, u32 bits) {
    R0 = word;
    R1 = bits;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

static void set_event_flag(u32 word, u32 bits) {
    R0 = word;
    R1 = bits;
    GUEST_CALL(SetEventFlagFromImmediate);
}

Next clearSetFlags_80355a8(void) {
    clear_event_flag(6, 0x82);   /* EVENT_682 */
    clear_event_flag(8, 0x5F);   /* EVENT_85F */
    clear_event_flag(10, 0xBA);  /* EVENT_ABA */
    clear_event_flag(12, 0xDC);  /* EVENT_CDC */
    clear_event_flag(12, 0xE0);  /* EVENT_CE0 */

    set_event_flag(1, 0x20);     /* EVENT_120 */

    R0 = 0;
    RETURN_TO_CALLER();
}
