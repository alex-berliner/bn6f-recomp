/*
 * leaves_farm_30.c: small functions from the model farm, set 30.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── pet_addCurNaviStat0x3ECapped_803CF3C ── */
/* pet_addCurNaviStat0x3ECapped_803CF3C(r1 = navi slot index; expects r4 == 0x70):
 * Adjusts one PET navi stat word: reads stat 0x3E of navi slot 0, adds
 * r1 * 20 (the per-slot stride), clamps the result to 1000, stores it back,
 * and calls sub_803CF74 to apply the same value/offset pair elsewhere.
 * All registers r4-r12 are preserved. */
Next pet_addCurNaviStat0x3ECapped_803CF3C(void) {
    u32 r4_in = R4, r5_in = R5, r6_in = R6, r7_in = R7;

    if (R4 != 0x70)
        RETURN_TO_CALLER();

    u32 slot_offset = 0x14 * R1; /* byte offset of the navi's own entry */

    /* read the current stat word (navi 0, stat 0x3E) */
    R0 = 0;
    R1 = 0x3E;
    R4 = 0;
    R6 = r6_in;
    R7 = slot_offset;
    GUEST_CALL(GetCurPETNaviStatsHword);

    u32 value = R0 + slot_offset;
    if ((s32)value > 0x3E8)
        value = 0x3E8; /* cap at 1000 */

    /* write the adjusted value back */
    R0 = 0;
    R1 = 0x3E;
    R2 = value;
    R4 = 0;
    R6 = value;
    R7 = slot_offset;
    GUEST_CALL(SetCurPETNaviStatsHword);

    /* propagate the new value (r0) and slot offset (r1) */
    R0 = value;
    R1 = slot_offset;
    R4 = 0;
    R5 = r5_in;
    R6 = value;
    R7 = slot_offset;
    GUEST_CALL(sub_803CF74);

    R4 = r4_in;
    R5 = r5_in;
    R6 = r6_in;
    R7 = r7_in;
    RETURN_TO_CALLER();
}

/* ── tickCountdownGate_803F530 ── */
/* tickCountdownGate_803F530: countdown gate attached to the flag byte at byte_200AD04.
 *
 * While the flag byte is set, consults sub_803F560 each tick; when it
 * approves, decrements the halfword counter at 200AD06. The flag byte is
 * rewritten (via setByte_200AD04_803F4EC) either when the counter reaches
 * zero or when sub_803F560 vetoes immediately. On return, the Z flag
 * reports whether the countdown is still active (r6 == 1 internally means
 * "still counting"); r4/r6 themselves are callee-saved and restored. */
Next tickCountdownGate_803F530(void) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;

    R6 = 0;
    GUEST_CALL(notZero_eByte200AD04);

    if (R0 != 0) {                      /* flag byte at 200AD04 is set */
        R4 = 1;
        GUEST_CALL(sub_803F560);

        if (R0 != 0) {
            /* Approved: count one step down. */
            R4 = 0;
            R6 = 1;
            R1 = A_word_200AD04;
            R0 = LD16(R1 + 2);          /* counter at word_200AD06 */
            SET_FLAGS_CMP(R0, 1);       /* subs r0, #1 */
            R0 -= 1;
            ST16(R1 + 2, R0);
            if (R0 == 0) {
                /* Counter finished: clear the flag byte. */
                R6 = 0;
                R0 = R4;
                GUEST_CALL(setByte_200AD04_803F4EC);
            }
            /* else: still counting, skip the flag write. */
        } else {
            /* Vetoed: write the flag byte right away. */
            R0 = R4;
            GUEST_CALL(setByte_200AD04_803F4EC);
        }
    }
    /* Flag byte clear: nothing to do this tick. */

    /* tst r6, r6: Z clear while the countdown is still running. */
    SET_FLAGS_NZ(R6);

    R4 = saved_r4;                      /* pop {r4,r6} */
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── save_BackupToGamePakWithRetry_803FAB4 ── */
/* save_BackupToGamePakWithRetry_803FAB4: back up the current save block to the game pak, retrying up
 * to three times.  Each attempt refills the RNG seed buffer, copies the
 * 0x80-byte block at dword_20067C0 to the game pak (0xE007F80) and verifies
 * the copy.  Returns r0 = 1 if at least one attempt verified OK, r0 = 0 if
 * all three failed. */
Next save_BackupToGamePakWithRetry_803FAB4(void) {
    const u32 entryR4 = R4;
    const u32 entryR6 = R6;
    const u32 entryR7 = R7;
    u32 failures = 0;
    u32 lastR3 = 0;

    for (u32 attempt = 0; attempt < 3; attempt++) {
        /* Refresh the entropy pool the save seed is drawn from. */
        R4 = entryR4;
        R6 = failures;
        R7 = attempt;
        GUEST_CALL(rng_FillSeedBuffer_803FAEE);

        /* Copy the 0x80-byte save block into the game pak backup area. */
        R0 = A_dword_20067C0;
        R1 = 0xE007F80;
        R2 = 0x80;
        GUEST_CALL(libSave_CopyToGamePak);
        lastR3 = R3;

        /* Verify the copy; a zero result marks the attempt as failed. */
        R0 = A_dword_20067C0;
        R1 = 0xE007F80;
        R2 = 0x80;
        R7 = attempt;
        GUEST_CALL(sub_814D9C4);
        R7 = attempt; /* sub_814D9C4 may clobber r7 */
        if (R0 == 0)
            failures++;
    }

    /* Restore the caller's registers and leave the loop's last state in
     * r2/r3, as the original's pops do. */
    R6 = entryR6;
    R7 = entryR7;
    R2 = 0x80;
    R3 = lastR3;

    if (failures < 3) {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}
