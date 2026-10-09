/*
 * leaves_farm_32.c: small functions from the model farm, set 32.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── rfu_STC_REQ_callback ── */
/* rfu_STC_REQ_callback(r0 = requesting device id, r1 = request command):
 * RFU library callback invoked when a REQ packet arrives.  Reinstalls the
 * library's default async callback, records the request command in the RFU
 * work area, and — if the callback slot is armed (bit 3 of the work area's
 * status byte) — forwards the request to its handler.
 *
 * Flag outputs: on the dispatch path they are whatever sub_814DA68 leaves;
 * on the skip path they come from the arm-bit test (CMP against 0).
 * r4/r5 are preserved: the original pushes them around the call and pops
 * them back (sub_814DA68 sees reqCmd in r4 and srcId in r5).
 *
 * Note on the return: the original exits with `pop {r4,r5}` / `pop {r0}` /
 * `bx r0`.  Because its push saved lr as the third word, that `pop {r0}`
 * loads the *return address* into r0 — so r0 leaves the function holding
 * the entry LR value on both paths. */
Next rfu_STC_REQ_callback(void) {
    u32 srcId   = R0 & 0xFF;      /* byte argument   */
    u32 reqCmd  = R1 & 0xFFFF;    /* halfword argument */
    u32 entryLR = LR;             /* becomes the outgoing r0 (see note above) */
    u32 inR4 = R4, inR5 = R5;     /* entry values, restored on the dispatch path */

    /* Always (re)install the default serial callback. */
    R0 = A_rfu_CB_defaultCallback | 1u;
    GUEST_CALL(STWI_set_Callback_M);

    /* Record the request command in the RFU work area. */
    u32 rfuWork = LD32(A_dword_2010CC4);
    ST16(rfuWork + 0x1C, reqCmd);

    u32 status = LD8(rfuWork);
    if (status & 8) {
        /* Slot armed: dispatch (id, command, handler list pointer). */
        R0 = srcId;
        R1 = reqCmd;
        R2 = LD32(LD32(A_dword_2010CC8));
        R4 = reqCmd;
        R5 = srcId;
        GUEST_CALL(sub_814DA68);
        R4 = inR4;    /* original pops r4/r5 back over the callee's values */
        R5 = inR5;
    } else {
        /* Skip path: exit flags are those of the arm-bit test, r1 keeps
         * the whole status byte as loaded by `ldrb r1, [r0]`. */
        SET_FLAGS_CMP(status & 8, 0);
        R1 = status;
    }

    /* `pop {r0}; bx r0`: r0 exits holding the saved return address. */
    R0 = entryLR;
    RETURN_TO_CALLER();
}
