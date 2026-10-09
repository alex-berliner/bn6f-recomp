/*
 * leaves_farm_19.c: small functions from the model farm, set 19.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_803007C ── */
/* sub_803007C(r5 = camera state):
 * Recompute the camera's two 16.16 fixed-point offset fields at +4/+6
 * from the pending scroll target nextX/nextY/nextZ, negating the results.
 * Leaves r1/r2/r4/r6/r7/r8 and the flags (from the final `neg r6`) as the
 * original does, since callers inspect them. */
Next sub_803007C(void) {
    u32 cam = R5;

    /* Load the pending scroll target and negate X and Y. */
    u32 negNextX = 0 - LD32(cam + oCamera_nextX_3c);
    u32 negNextY = 0 - LD32(cam + oCamera_nextY_40);
    u32 nextZ    = LD32(cam + oCamera_nextZ_44);

    /* Offset 04: negate the high 16 bits of (negX + negY).
     * (The `neg r1` here also sets flags, but they are overwritten later.) */
    R1 = 0 - (u32)((s32)(negNextX + negNextY) >> 16);
    ST16(cam + 4, R1);

    /* Offset 06: (negY - negX) / 2 plus nextZ, negate the high 16 bits.
     * The final `neg r6, r6` is the last flag-setting instruction of the
     * routine, so the flags must come from 0 - shifted (the negation
     * itself), not from the already-negated result. */
    u32 halfDiff = (u32)((s32)(negNextY - negNextX) >> 1);
    u32 shifted  = (u32)((s32)(halfDiff + nextZ) >> 16);
    SET_FLAGS_CMP(0, shifted);          /* flags of RSBS: 0 - shifted */
    R6 = 0 - shifted;
    ST16(cam + 6, R6);

    /* Values callers expect to find in the registers afterwards. */
    R2 = nextZ;
    R4 = 0;
    R7 = negNextY;
    R8 = negNextX;

    RETURN_TO_CALLER();
}

/* ── sub_8031388 ── */
/* sub_8031388(r0-r2 = parameters, r5 = parameter, r12 = state flags):
 * Runs the three-step sequence shared by this family of routines:
 *   1. sub_8031136 on the caller's r0-r2; its r0/r1 results are fed
 *      straight into step 2 (the original never reloads them),
 *   2. sub_8030E14 with the "unbounded" extent markers (r2 = r3 = -1),
 *      an extent count of 0x20, r5 = 0 and r6 = 1,
 *   3. sub_80310B0 with r1 = 0 to finish.
 * Afterwards the low bit of the r12 state flags is set (state marked
 * active) and nz reflects that value; c/v are whatever the last callee
 * left. r0, r1, r2 and r5 are restored to their entry values (the
 * original's push/pop pair); r3, r4 and r6 return whatever sub_80310B0
 * left in them. */
Next sub_8031388(void) {
    u32 ret_addr = LR;   /* push {lr} */
    u32 arg0 = R0, arg1 = R1, arg2 = R2, arg5 = R5;

    /* Step 1: r0-r2 are passed through unchanged. */
    GUEST_CALL(sub_8031136);

    /* Step 2: r0/r1 still carry step 1's results here. */
    R2 = (u32)-1;
    R3 = (u32)-1;
    R4 = 0x20;
    R5 = 0;
    R6 = 1;
    GUEST_CALL(sub_8030E14);

    /* Step 3. */
    R1 = 0;
    GUEST_CALL(sub_80310B0);

    /* Mark the state active: set the low bit of the current r12 flags
     * (orr sets nz from the result, c/v stay as the callee left them). */
    R0 = R12 | 1;
    SET_FLAGS_NZ(R0);
    R12 = R0;

    /* pop {r0-r2,r5,pc}: restore the caller's registers and return to
     * the address the caller left in LR. */
    R0 = arg0;
    R1 = arg1;
    R2 = arg2;
    R5 = arg5;
    LR = ret_addr;
    RETURN_TO_CALLER();
}

/* ── sub_80C6A08 ── */
/* sub_80C6A08: step an object sideways by its X velocity and destroy it if the
 * panel it ends up on is not a valid standing spot.  r5 = battle object.
 * Leaves nz/c/v reflecting the panel-validity test (or the destroy state value). */
Next sub_80C6A08(void) {
    /* PanelX is a byte, so the velocity add wraps to one tile dimension. */
    u32 newX = LD8(R5 + oBattleObject_PanelX) + LD32(R5 + oBattleObject_XVelocity);
    ST8(R5 + oBattleObject_PanelX, newX);

    GUEST_CALL(object_setCoordinatesFromPanels);
    GUEST_CALL(object_updateCollisionPanels);
    GUEST_CALL(object_isCurrentPanelValid);

    /* tst r0, r0: is the panel we now occupy walkable? */
    SET_FLAGS_NZ(R0);
    if (R0 == 0) {
        /* Invalid panel: drop the collision region and mark the object for
         * destruction, clearing its "active" flag bit (bit 1, at offset 0). */
        GUEST_CALL(object_clearCollisionRegion);

        R1 = 2;
        ST8(R5, LD8(R5) & ~R1);

        R0 = 8; /* CUR_STATE_DESTROY */
        SET_FLAGS_NZ(R0);
        ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
    }
    RETURN_TO_CALLER();
}
