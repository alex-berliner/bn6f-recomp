/*
 * chatbox_flags.c — the chatbox's 16-bit flag word (bn6f chatbox.s,
 * oChatbox_flags_3E): set, clear and test bits of it. r5 is the chatbox.
 * Each keeps r1, as the original's push/pop does.
 *
 * Written by Hyper glm-5.3-flash through the farm workflow (tools/wf, all
 * three accepted on their first round against 40 calls captured on
 * battles1 and random inputs); reviewed and integrated by Claude through
 * the convert workflow.
 */
#include "game.h"
#include "runtime.h"

/* chatbox_setflags_3e(r0 = bits, r5 = chatbox): set the bits; nz from the new flag word. */
Next chatbox_setflags_3e(void) {
    u32 flags = LD16(R5 + oChatbox_flags_3E) | R0;
    ST16(R5 + oChatbox_flags_3E, flags);
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* chatbox_clearFlags_3e(r0 = bits, r5 = chatbox): clear the bits (no flags promised). */
Next chatbox_clearFlags_3e(void) {
    ST16(R5 + oChatbox_flags_3E, LD16(R5 + oChatbox_flags_3E) & ~R0);
    RETURN_TO_CALLER();
}

/* chatbox_maskFlags_3e(r0 = bits, r5 = chatbox) -> r0 = those of the bits that are set;
 * nz from it, so callers can branch on "any set". */
Next chatbox_maskFlags_3e(void) {
    R0 &= LD16(R5 + oChatbox_flags_3E);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
