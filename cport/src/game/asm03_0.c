/*
 * asm03_0.c: readable functions of bn6f asm03_0.s, up to 0x080299B6, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_beastJumpLockOn_8026554 ── */
/*
 * Beast Out jump lock-on dispatch (0x08026554).
 *
 * The caller supplies a panel coordinate (r0 = x, r1 = y) and a check id in
 * r2.  If the coordinate is inside the 6x3 playfield the check id selects a
 * handler from the game's jump table; otherwise entry 0 (the "own panel"
 * lookup) runs.  The handler is entered with the caller's live register state
 * -- the dispatch leaves r2 = handler address and r3 = jump table address, and
 * copies the panel coordinates into r6/r7 for the handlers to read -- and its
 * results in r0, r1, r5 and r8-r12 are this function's outputs.  r6/r7 are
 * restored to the caller's values on return.
 */

/* Jump table at jt_8026584: one handler address per lock-on check id. */
#define LOCKON_JUMP_TABLE A_jt_8026584

/* Run the lock-on check selected by `mode` for panel (x, y).
 *
 * register form: the handler is reached through the game's own jump table and
 * must be entered with the live register state (r0/r1 = coordinates, r2 = its
 * own entry address, r3 = table address, r6/r7 = coordinates, r4/r5 untouched)
 * and leaves its results in r0, r1, r5 and r8-r12, which this function
 * outputs.  No typed call can pass that state through, so the guest registers
 * are driven directly around the single indirect call.
 */
static void run_lockon_check(u32 x, u32 y, u32 mode) {
    /* Coordinates outside the playfield fall back to check 0. */
    if ((s32)x < 1 || (s32)x > 6 || (s32)y < 1 || (s32)y > 3)
        mode = 0;

    u32 saved_r6 = R6;          /* caller's r6/r7, restored after the call */
    u32 saved_r7 = R7;
    u32 table = LOCKON_JUMP_TABLE;
    u32 handler = LD32(table + mode * 4);

    R0 = x;                     /* handler arguments: r0/r1 = panel coords   */
    R1 = y;
    R6 = x;                     /* handlers read the coords from r6/r7       */
    R7 = y;
    R2 = handler;               /* dispatch leaves r2 = handler address ...  */
    R3 = table;                 /* ... and r3 = jump table address           */

    GUEST_CALL_ADDR(handler);

    R6 = saved_r6;
    R7 = saved_r7;
}

/* battle_beastJumpLockOn_8026554(r0 = panel x, r1 = panel y, r2 = check id).
 * Everything the selected handler leaves in r0, r1, r5 and r8-r12 is already
 * in the output registers, so there is nothing to write back here. */
Next battle_beastJumpLockOn_8026554(void) {
    run_lockon_check(R0, R1, R2);
    RETURN_TO_CALLER();
}

/* ── battle_checkFacingOffsets ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; } call_sub_8026450_result_battle_checkFacingOffsets;
/* register form: generated wrapper: sub_8026450 has no typed C interface yet */
static inline call_sub_8026450_result_battle_checkFacingOffsets call_sub_8026450_battle_checkFacingOffsets(u32 r0, u32 r1, u32 r2, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(sub_8026450);
    return (call_sub_8026450_result_battle_checkFacingOffsets){ R0, R1, R2, R3, R4, R5, R6, R7 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_battle_checkFacingOffsets;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battle_checkFacingOffsets call_object_getFrontDirection_battle_checkFacingOffsets(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battle_checkFacingOffsets){ R0 };
}

/* Scan the facing-offset table (byte_8026735, terminated by 0x7F):
 * for each signed offset, compute front_direction × offset + base and
 * test it against the target position via sub_8026450.
 * Returns the first non-zero hit, or 0 when the table is exhausted.
 * Also returns r1-r3 as left by the final sub_8026450 call. */
struct facing_offsets_result {
    u32 r0, r1, r2, r3;
};

static struct facing_offsets_result scan_facing_offsets(u32 target, u32 objPtr, u32 base, u32 dir) {
    struct facing_offsets_result out;
    u32 candidate = base;
    u32 table = A_byte_8026735;

    for (;;) {
        call_sub_8026450_result_battle_checkFacingOffsets res = call_sub_8026450_battle_checkFacingOffsets(candidate, dir, target, objPtr, base, dir);
        if (res.r0 != 0) {
            out.r0 = res.r0;
            out.r1 = res.r1;
            out.r2 = res.r2;
            out.r3 = res.r3;
            return out;
        }

        u32 front = call_object_getFrontDirection_battle_checkFacingOffsets(objPtr).r0;
        s32 off = (s32)LDS8(table);
        if (off == 0x7f) {
            /* table terminator reached */
            out.r0 = 0;
            out.r1 = (u32)off;   /* r1 = 0x7f from the ldrsb */
            out.r2 = res.r2;
            out.r3 = res.r3;
            return out;
        }
        table += 1;
        candidate = front * (u32)off + base;
    }
}

/* battle_checkFacingOffsets(r0 = target, r5 = object ptr, r6 = base, r7 = dir) -> r0 r1 r2 r3; nz c v */
Next battle_checkFacingOffsets(void) {
    struct facing_offsets_result r = scan_facing_offsets(R0, R5, R6, R7);
    R0 = r.r0;
    R1 = r.r1;
    R2 = r.r2;
    R3 = r.r3;
    SET_FLAGS_CMP(R0, 0);
    RETURN_TO_CALLER();
}

/* ── battle_probePanelChainRows_8026622 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; } call_sub_8026450_result;
/* register form: generated wrapper: sub_8026450 has no typed C interface yet */
static inline call_sub_8026450_result call_sub_8026450(u32 r0, u32 r1, u32 r2, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(sub_8026450);
    return (call_sub_8026450_result){ R0, R1, R2, R3, R4, R5, R6, R7 };
}

/* Result of the panel probe: the (r0, r1) pair the routine leaves behind. */
typedef struct { u32 r0; u32 r1; } PanelProbe;

/*
 * Walk the signed-byte offset table at byte_8026730, probing one candidate
 * panel per entry with sub_8026450. The first probe tests the object's own
 * panel X on row `row`, using table byte_802673A when the object already
 * stands on that row and byte_802673C otherwise. Each later entry gives a
 * Y offset from `row` and is probed at X = probeX with byte_802673C.
 * Stop at the first nonzero probe result, or at the table's 0x7F terminator.
 */
static PanelProbe probe_panel_chain(u32 object, u32 probeX, u32 row) {
    u32 table = (LD8(object + oBattleObject_PanelY) == row)
                    ? A_byte_802673A : A_byte_802673C;
    u32 x = LD8(object + oBattleObject_PanelX);
    u32 y = row;
    u32 entry = A_byte_8026730;

    for (;;) {
        call_sub_8026450_result hit =
            call_sub_8026450(x, y, table, object, probeX, row);
        if (hit.r0 != 0)
            return (PanelProbe){ hit.r0, hit.r1 };

        s32 yOffset = LDS8(entry);
        if (yOffset == 0x7F)
            return (PanelProbe){ 0, 0x7F };

        entry += 1;
        y = (u32)(yOffset + row);
        x = probeX;
        table = A_byte_802673C;
    }
}

/* battle_probePanelChainRows_8026622(r5 = object, r6 = probeX, r7 = row) -> r0 r1 (r5 unchanged) */
Next battle_probePanelChainRows_8026622(void) {
    u32 object = R5;
    PanelProbe result = probe_panel_chain(object, R6, R7);
    R0 = result.r0;
    R1 = result.r1;
    R5 = object;
    RETURN_TO_CALLER();
}

/* ── battle_runFacingOffsetCheck_802665A ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; } call_battle_checkFacingOffsets_result_sub_802665A;
/* register form: generated wrapper: battle_checkFacingOffsets has no typed C interface yet */
static inline call_battle_checkFacingOffsets_result_sub_802665A call_battle_checkFacingOffsets_sub_802665A(u32 r0, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battle_checkFacingOffsets);
    return (call_battle_checkFacingOffsets_result_sub_802665A){ R0, R1, R2, R3, R4, R5, R6, R7 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_sub_80265FE_result;
/* register form: generated wrapper: sub_80265FE has no typed C interface yet */
static inline call_sub_80265FE_result call_sub_80265FE(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(sub_80265FE);
    return (call_sub_80265FE_result){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_runFacingOffsetCheck_802665A: run the facing-offset check for the current actor against the
 * table at byte_802675A, then post-process the result with sub_80265FE.
 * The caller's r4-r7 are the battle-context registers both callees expect;
 * r0/r1/r5 coming back from sub_80265FE are the results callers use. */

typedef struct {
    u32 r0;
    u32 r1;
    u32 r5;
} FacingCheckResult;

static FacingCheckResult sub_802665A_body(u32 r4, u32 r5, u32 r6, u32 r7) {
    /* Point r0 at the facing-offset table and run the shared check, which
     * reads the actor state carried in r4-r7. */
    call_battle_checkFacingOffsets_result_sub_802665A facing =
        call_battle_checkFacingOffsets_sub_802665A(A_byte_802675A, r4, r5, r6, r7);

    /* Feed the interesting outputs (r0, r1, r5) into the follow-up step. */
    call_sub_80265FE_result result =
        call_sub_80265FE(facing.r0, facing.r1, facing.r5);

    FacingCheckResult out;
    out.r0 = result.r0;
    out.r1 = result.r1;
    out.r5 = result.r5;
    return out;
}

/* battle_runFacingOffsetCheck_802665A() -> r0 r1 r5 (r2/r3 and the flags are scratch for callers) */
Next battle_runFacingOffsetCheck_802665A(void) {
    FacingCheckResult result = sub_802665A_body(R4, R5, R6, R7);
    R0 = result.r0;
    R1 = result.r1;
    R5 = result.r5;
    RETURN_TO_CALLER();
}

/* ── battle_checkFacingOffsets_table_8026672 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_battle_checkFacingOffsets_result_sub_8026672;
/* register form: generated wrapper: battle_checkFacingOffsets has no typed C interface yet */
static inline call_battle_checkFacingOffsets_result_sub_8026672 call_battle_checkFacingOffsets_sub_8026672(u32 r0, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battle_checkFacingOffsets);
    return (call_battle_checkFacingOffsets_result_sub_8026672){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_checkFacingOffsets_table_8026672: run battle_checkFacingOffsets against the facing-offset table
 * baked into the code at byte_802676E. The caller's context in r4-r7 is
 * passed straight through; the callee's r0/r1 come back as this function's
 * result, and r5 is simply whatever the callee left behind. */
static call_battle_checkFacingOffsets_result_sub_8026672 check_facing_offsets_table(u32 r4, u32 r5, u32 r6, u32 r7) {
    return call_battle_checkFacingOffsets_sub_8026672(A_byte_802676E, r4, r5, r6, r7);
}

Next battle_checkFacingOffsets_table_8026672(void) {
    call_battle_checkFacingOffsets_result_sub_8026672 res = check_facing_offsets_table(R4, R5, R6, R7);
    R0 = res.r0;
    R1 = res.r1;
    RETURN_TO_CALLER();
}

/* ── sub_8026694 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_battle_checkFacingOffsets_result_sub_8026694;
/* register form: generated wrapper: battle_checkFacingOffsets has no typed C interface yet */
static inline call_battle_checkFacingOffsets_result_sub_8026694 call_battle_checkFacingOffsets_sub_8026694(u32 r0, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battle_checkFacingOffsets);
    return (call_battle_checkFacingOffsets_result_sub_8026694){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_8026694: run battle_checkFacingOffsets on a fixed parameter block.
 *
 * The only work here is loading the address byte_8026791 into r0 and tail-calling
 * battle_checkFacingOffsets with r4-r7 passed straight through. That callee produces
 * this function's outputs (r0, r1, r5) and leaves r4/r6/r7 as it received them,
 * so the wrapper's registers are already the final answer: nothing needs to be
 * copied through the typed function or the adapter.
 */
static void sub_8026694_body(u32 r4, u32 r5, u32 r6, u32 r7)
{
    call_battle_checkFacingOffsets_sub_8026694(A_byte_8026791, r4, r5, r6, r7);
}

Next sub_8026694(void) {
    sub_8026694_body(R4, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── battle_checkFacingOffsetsDefaultTable_80266DE ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_battle_checkFacingOffsets_result_sub_80266DE;
/* register form: generated wrapper: battle_checkFacingOffsets has no typed C interface yet */
static inline call_battle_checkFacingOffsets_result_sub_80266DE call_battle_checkFacingOffsets_sub_80266DE(u32 r0, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battle_checkFacingOffsets);
    return (call_battle_checkFacingOffsets_result_sub_80266DE){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_checkFacingOffsetsDefaultTable_80266DE: run battle_checkFacingOffsets against the fixed table at
 * byte_80267AB, passing the caller's r4-r7 straight through to it. The
 * useful results are the r0/r1 pair (and r5) that battle_checkFacingOffsets
 * leaves behind; flags are not part of this function's contract. */
static call_battle_checkFacingOffsets_result_sub_80266DE check_facing_default_table(u32 r4, u32 r5, u32 r6, u32 r7) {
    return call_battle_checkFacingOffsets_sub_80266DE(A_byte_80267AB, r4, r5, r6, r7);
}

/* battle_checkFacingOffsetsDefaultTable_80266DE() -> r0 r1 r5 (r4/r6/r7 preserved, nz/c/v/r2/r3 clobbered) */
Next battle_checkFacingOffsetsDefaultTable_80266DE(void) {
    call_battle_checkFacingOffsets_result_sub_80266DE res = check_facing_default_table(R4, R5, R6, R7);
    R0 = res.r0;
    R1 = res.r1;
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── camera_isObjectInFrontOfCamera_80269D0 ── */
/* camera_isObjectInFrontOfCamera_80269D0: ask battle_GetObjectByteAt14_800A9E2 for a status value and reduce it to a boolean:
 * r0 = 1 if any of bits 0/2 (mask 5) is set, otherwise r0 = 0.
 * The call's carry/overflow flags pass through unchanged to the caller. */
Next camera_isObjectInFrontOfCamera_80269D0(void) {
    R0 = 0;
    GUEST_CALL(battle_GetObjectByteAt14_800A9E2);

    R1 = 5;
    R0 = (R0 & R1) != 0 ? 1 : 0;

    RETURN_TO_CALLER();
}

/* ── camera_shouldApplyBgScrollToBothLayerPairs_80269E2 ── */
/* eS20364C0 RAM pointer (r5 in the original). */
#define eS20364C0_PTR A_eS20364C0

/* Replicates camera_shouldApplyBgScrollToBothLayerPairs_80269E2:
 *   - r0 starts at 0.
 *   - Read byte 0 as r1; if r1 is 0 or 8, r0 becomes 1 (and that CMP's flags win).
 *   - Otherwise read byte 1 as r1; if r1 is any of {0, 8, 4, 0xC, 0x2C, 0x30, 0x34,
 *     0x10, 0x14, 0x18, 0x1C, 0x20}, r0 becomes 1 and that CMP's flags win.
 *   - Else r0 stays 0 and the final CMP r1, #0x20 (BNE taken) sets the flags.
 * Returns the r0 result and the operands of the final CMP so the adapter can
 * set c/v exactly as the original does. */
static u32 check_es20364c0(u8 *last_cmp_r1, u8 *last_cmp_op) {
    u8 b0 = LD8(eS20364C0_PTR);
    u8 b1 = LD8(eS20364C0_PTR + 1);
    u32 result = 0;

    if (b0 == 0) {
        result = 1;
        *last_cmp_r1 = b0; *last_cmp_op = 0;
    } else if (b0 == 8) {
        result = 1;
        *last_cmp_r1 = b0; *last_cmp_op = 8;
    } else if (b1 == 0) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0;
    } else if (b1 == 8) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 8;
    } else if (b1 == 4) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 4;
    } else if (b1 == 0x0C) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x0C;
    } else if (b1 == 0x2C) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x2C;
    } else if (b1 == 0x30) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x30;
    } else if (b1 == 0x34) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x34;
    } else if (b1 == 0x10) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x10;
    } else if (b1 == 0x14) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x14;
    } else if (b1 == 0x18) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x18;
    } else if (b1 == 0x1C) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x1C;
    } else if (b1 == 0x20) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x20;
    } else {
        /* Fall through: b1 != 0x20, BNE taken, r0 stays 0. Final CMP was r1, #0x20. */
        result = 0;
        *last_cmp_r1 = b1; *last_cmp_op = 0x20;
    }
    return result;
}

/* camera_shouldApplyBgScrollToBothLayerPairs_80269E2(r5 = eS20364C0 pointer) -> r0, sets c, v from the final CMP */
Next camera_shouldApplyBgScrollToBothLayerPairs_80269E2(void) {
    u8 last_r1, last_op;
    u32 r = check_es20364c0(&last_r1, &last_op);
    R0 = r;
    SET_FLAGS_CMP((u32)last_r1, (u32)last_op);
    RETURN_TO_CALLER();
}

/* ── sub_8026A28 ── */
/* Dispatch one step of the state machine whose object lives at eS20364C0.
 *
 * The object's state byte (oS20364C0_JumpOffset00) selects an entry in the
 * handler table at off_8026A3C:
 *   +0  sub_8026A50
 *   +4  custMenuMainMaybe_8026A88        (custom menu main loop)
 *   +8  object_runFrameUpdate_8026A6C
 * The chosen handler is called with r5 = state object (r0 still holds the
 * state byte and r1 the table entry, exactly as the original leaves them)
 * and returns a result in r0. Everything the handler leaves in the other
 * registers passes straight through to this routine's caller; only r5 is
 * restored, matching the original's push {r5,lr} / pop {r5,pc}. */

/* register form: the handler is reached through the object's jump table
 * (ldr r1,[r1,r0]; mov lr,pc; bx r1), so the call must go through
 * GUEST_CALL_ADDR, and the register state it leaves behind (r0, r2-r4,
 * r6, r8-r12) is sub_8026A28's own output contract. No typed signature can
 * express a call that returns ten live registers. */
static void run_state_step(void) {
    u32 savedR5 = R5;                    /* push {r5}: caller's r5 */

    R5 = A_eS20364C0;                    /* the state object (self) */
    u32 state = LD8(A_eS20364C0 + oS20364C0_JumpOffset00);
    R0 = state;                          /* live at handler entry, as in the original */
    R1 = LD32(A_off_8026A3C + state);    /* handler address from the jump table */

    GUEST_CALL_ADDR(R1);                 /* run the selected handler */

    R5 = savedR5;                        /* pop {r5}: r5 is preserved for the caller */
}

Next sub_8026A28(void) {
    run_state_step();
    RETURN_TO_CALLER();
}

/* ── sub_8026A50 ── */
/* sub_8026A50(self: r5 = *S20364C0) -> u8 in r0 (always 0)
 *
 * Stores the value returned by battle_allocateActorIndex_802FE48 into the object's byte at
 * offset 0x0E, starts sprite_setAffineTransform_802FE7A with (0, 0x40, 0x40, value), sets the
 * object's state word to 4, and returns 0. r1-r3 end as sprite_setAffineTransform_802FE7A
 * left them (it preserves its arguments); flags come from "mov r0, #0".
 */
Next sub_8026A50(void) {
    u32 result;

    GUEST_CALL(battle_allocateActorIndex_802FE48);            /* -> r0 */

    result = R0;
    ST8(R5 + oS20364C0_ActorIndex, result); /* remember the result on the object */

    R3 = result;                        /* arguments for sprite_setAffineTransform_802FE7A */
    R1 = 0x40;
    R2 = 0x40;
    R0 = 0;
    GUEST_CALL(sprite_setAffineTransform_802FE7A);            /* r1/r2/r3 pass through unchanged */

    ST32(R5, 4);                        /* object state = 4 */

    R0 = 0;                             /* return value + NZ/C/V from mov r0,#0 */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_runFrameUpdate_8026A6C ── */
/* object_runFrameUpdate_8026A6C(r5 = self object, struct S20364C0) -> r0 = self->04 (u8):
 * Run this object's standard per-frame update sequence: refresh its
 * animation/timer state, then pass its +0x0E attribute through the shared
 * flags_clearFlagWordBit_802FE6A/sub_80062EC helpers, run the object-wide update passes
 * (sound_playCrossTransformationJingle_802A3CC, battle_DispatchPendingEvent_802A0F8), and finally report the object's state byte
 * stored at +0x04. Flags are left as battle_DispatchPendingEvent_802A0F8 returned them; r6/r7 are
 * preserved for the caller. */
Next object_runFrameUpdate_8026A6C(void) {
    u32 self = R5;

    GUEST_CALL(battle_customGauge_EndTurn_8027D78);

    R0 = LD8(self + 0x0E);
    GUEST_CALL(flags_clearFlagWordBit_802FE6A);
    GUEST_CALL(sub_80062EC);

    GUEST_CALL(sound_playCrossTransformationJingle_802A3CC);
    GUEST_CALL(battle_DispatchPendingEvent_802A0F8);

    R0 = LD8(self + 0x04);
    RETURN_TO_CALLER();
}

/* ── custMenuMainMaybe_8026A88 ── */
/* Register contract for the custom-menu state machine step:
 * dispatches the handler selected by oS20364C0_JumpOffset01 from the table at
 * off_8026AA4, then runs the shared per-frame update custMenu_dispatchState_802A2B0. Both are
 * called with the menu work area (0x020364C0) in r5; the state handler also
 * receives the raw table offset in r0 and its own table entry in r1, exactly
 * as the original's jump-table dispatch leaves the registers. */

/* register form: calls the custom-menu state handlers (battle_custMenuAnimateCursorToOK_8026B04,
 * custMenuUpdateWaitSelectionState_8026CCC, scrollStrip_update_8026BF4, battleObject_chipSelectOverlayTick,
 * sub_8026DB0, battle_endChipSelectionBeginTurn_8026DC4, chatbox_stateMaybeFinishText_8026E4C,
 * battle_updateObjectByKind_8026E98, battle_runTutorialDialog_8026FC0, battleObject_chipSelectStateTick, sub_80271F8,
 * sub_802728C, sub_80273A4, chatbox_resetTextStateIfHidden_80273EC,
 * sub_8027406, sub_802753E, battleObject_chipSelectUpdate, battleObjectStep, battleObject_chipSelectAnimTick_802770C,
 * sub_8027834, battle_customScreenChipUseAnimStep, sub_802794A, battleObject_chipSelectTick, sub_8027A58) outside
 * this file; they have no typed C interface yet */
static void run_state_handler(u32 state_offset, u32 self) {
    u32 handler = LD32(A_off_8026AA4 + state_offset);

    R0 = state_offset;  /* handlers see the table offset in r0 */
    R1 = handler;       /* the original holds the table entry in r1 across bx */
    R5 = self;          /* handlers receive the menu work area in r5 */

    /* JumpOffset01 always holds one of the word offsets into the handler
     * table, so switch on it directly; the table entry above is what the
     * original would have jumped through. */
    switch (state_offset) {
    case 0x00: GUEST_CALL(battle_custMenuAnimateCursorToOK_8026B04); break;
    case 0x04: GUEST_CALL(custMenuUpdateWaitSelectionState_8026CCC); break;
    case 0x08: GUEST_CALL(scrollStrip_update_8026BF4); break;
    case 0x0C: GUEST_CALL(battleObject_chipSelectOverlayTick); break;
    case 0x10: GUEST_CALL(sub_8026DB0); break;
    case 0x14: GUEST_CALL(battle_endChipSelectionBeginTurn_8026DC4); break;
    case 0x18: GUEST_CALL(chatbox_stateMaybeFinishText_8026E4C); break;
    case 0x1C: GUEST_CALL(battle_updateObjectByKind_8026E98); break;
    case 0x20: GUEST_CALL(battle_runTutorialDialog_8026FC0); break;
    case 0x24: GUEST_CALL(battleObject_chipSelectStateTick); break;
    case 0x28: GUEST_CALL(sub_80271F8); break;
    case 0x2C: GUEST_CALL(sub_802728C); break;
    case 0x30: GUEST_CALL(sub_80273A4); break;
    case 0x34: GUEST_CALL(chatbox_resetTextStateIfHidden_80273EC); break;
    case 0x38: GUEST_CALL(sub_8027406); break;
    case 0x3C: GUEST_CALL(sub_802753E); break;
    case 0x40: GUEST_CALL(battleObject_chipSelectUpdate); break;
    case 0x44: GUEST_CALL(battleObjectStep); break;
    case 0x48: GUEST_CALL(battleObject_chipSelectAnimTick_802770C); break;
    case 0x4C: GUEST_CALL(sub_8027834); break;
    case 0x50: GUEST_CALL(battle_customScreenChipUseAnimStep); break;
    case 0x54: GUEST_CALL(sub_802794A); break;
    case 0x58: GUEST_CALL(battleObject_chipSelectTick); break;
    case 0x5C: GUEST_CALL(sub_8027A58); break;
    default: break; /* unreachable: the state byte is always a table offset */
    }
}

/* register form: calls custMenu_dispatchState_802A2B0 outside this file, which has no typed C interface yet */
static void run_menu_update(u32 self) {
    R5 = self;  /* custMenu_dispatchState_802A2B0 receives the menu work area in r5 */
    GUEST_CALL(custMenu_dispatchState_802A2B0);
}

typedef struct CustMenuStepResult {
    u32 result;      /* value for r0: always 0 */
    u32 caller_r5;   /* value for r5: the original pushes/pops r5 around the dispatch */
} CustMenuStepResult;

/* Advance the custom menu state machine one frame: run the handler for the
 * current state, then the shared per-frame update. r0 comes back 0 (the
 * original's `movs r0,#0`, which sets Z set / N clear and leaves C/V alone). */
static CustMenuStepResult cust_menu_main_step(u32 caller_r5) {
    u32 self = A_eS20364C0;                /* fixed work area 0x020364C0 */
    u32 state_offset = LD8(self + oS20364C0_JumpOffset01);

    run_state_handler(state_offset, self);
    run_menu_update(self);

    {
        CustMenuStepResult out;
        out.result = 0;
        out.caller_r5 = caller_r5;
        return out;
    }
}

/* custMenuMainMaybe_8026A88(void) -> r0 = 0 with Z set / N clear (from the
 * original's final `movs r0,#0`), r5 preserved, other registers as the
 * callees leave them. */
Next custMenuMainMaybe_8026A88(void) {
    CustMenuStepResult out = cust_menu_main_step(R5);
    R0 = out.result;
    R5 = out.caller_r5;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_custMenuAnimateCursorToOK_8026B04 ── */
/* Number of tile entries per animation row */
#define TILEMAP_COLS       0x14
/* Byte stride between successive tilemap columns */
#define TILEMAP_STRIDE     0x1E
/* Total animation countdown value */
#define TILE_ANIM_TOTAL    0x78
/* Tile rows per animation frame */
#define TILE_ROW_COUNT     0xF
/* Camera scroll step (dword_8026CC8) */
#define CAMERA_SCROLL_STEP 0x18000
/* Jump offset for the trigger-selection state */
#define JUMP_OFFSET_TRIGGER   0x2C
/* Jump offset for wait-selection state */
#define JUMP_OFFSET_WAIT_SEL  4

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_effect(u32 sound_id) {
    R0 = sound_id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls battle_setNaviStatusGfx_801E71C outside this file, which has no typed C interface yet */
static void bg_scroll_apply(u32 scroll_delta, u32 progress) {
    R0 = scroll_delta;
    R1 = progress;
    GUEST_CALL(battle_setNaviStatusGfx_801E71C);
}

/* register form: calls dispatch_801DACC outside this file, which has no typed C interface yet */
static void dispatch_bg_flags(u32 flags) {
    R0 = flags;
    GUEST_CALL(dispatch_801DACC);
}

/* register form: calls battle_eStruct2035280_postEvent_801E0A0 outside this file, which has no typed C interface yet */
static void tile_bg_setup(u32 rows, u32 start) {
    R0 = rows;
    R1 = start;
    GUEST_CALL(battle_eStruct2035280_postEvent_801E0A0);
}

/* register form: calls bgmap_WriteTileIdAtBlock_800187C outside this file, which has no typed C interface yet */
static void write_bg_tile(u32 row, u32 col, u32 bg_index, u16 tile_id) {
    R0 = row;
    R1 = col;
    R2 = bg_index;
    R3 = tile_id;
    GUEST_CALL(bgmap_WriteTileIdAtBlock_800187C);
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 get_battle_mode_sub_8026B04(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls RetrieveAndRenderEnemyVirusNamesInBattle_801E574 outside this file, which has no typed C interface yet */
static void render_enemy_virus_names(void) {
    GUEST_CALL(RetrieveAndRenderEnemyVirusNamesInBattle_801E574);
}

/* register form: calls battle_damagePlayerLeave1HP_8013FD0 outside this file, which has no typed C interface yet */
static void damage_player_leave_1hp(u32 player_index) {
    R0 = player_index;
    GUEST_CALL(battle_damagePlayerLeave1HP_8013FD0);
}

/* register form: battleObject_chipSelectAnimTick_8029C08 reads r2-r7 as object method state carried across the
 * entire function's register flow (camera ptr in r2, scroll const in r3, etc.).
 * Typed C cannot express "value lives in a specific guest register across multiple calls." */
static void s20364c0_finalize(u32 self) {
    R5 = self;
    GUEST_CALL(battleObject_chipSelectAnimTick_8029C08);
}

/* Update the tile-scrolling battle object (S20364C0).
 * self:    pointer to the state struct (r5)
 * toolkit: global toolkit pointer (r10)
 * r4, r6:  caller-saved values that battleObject_chipSelectAnimTick_8029C08 reads via those registers.
 * R2/R3 are written as guest registers inside the camera path because the game
 * passes them forward through multiple GUEST_CALLs to battleObject_chipSelectAnimTick_8029C08. */
static void s20364c0_update(u32 self, u32 toolkit, u32 r4, u32 r6) {
    /* --- Initialization (only when state flag is zero) --- */
    if (LD8(self + oS20364C0_StateHandled) == 0) {
        play_sound_effect(SOUND_SELECT_79);

        u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
        ST16(render_info + 0x18, TILE_ANIM_TOTAL);

        bg_scroll_apply(0, render_info);
        dispatch_bg_flags(0x80);
        tile_bg_setup(TILE_ROW_COUNT, 0);

        ST32(self + 0x44, TILE_ROW_COUNT);
        ST32(self + 0x40, TILE_ANIM_TOTAL);
        ST8(self + oS20364C0_StateHandled, TILE_ANIM_TOTAL);
    }

    /* --- Tile-rendering loop --- */
    u32 progress = LD32(self + 0x40) - 0xc;
    u32 rows = ((4 & progress) >> 2) + 1;

    for (u32 row_iter = 0; row_iter < rows; row_iter++) {
        u32 tile_index = LD32(self + 0x44) - 1;
        ST32(self + 0x44, tile_index);
        u32 tilemap_ptr = A_unk_2035000 + tile_index * 2;

        for (u32 col = 0; col < TILEMAP_COLS; col++) {
            u16 tile_id = LD16(tilemap_ptr);
            write_bg_tile(tile_index, col, 3, tile_id);
            tilemap_ptr += TILEMAP_STRIDE;
        }
    }

    /* --- Camera adjustment: if not in battle mode 1, scroll camera up --- */
    if (get_battle_mode_sub_8026B04() != 1) {
        u32 camera = LD32(toolkit + oToolkit_CameraPtr);
        ST32(camera + 0x34, LD32(camera + 0x34) - CAMERA_SCROLL_STEP);
        /* register form: battleObject_chipSelectAnimTick_8029C08 reads r2/r3 as accumulated register state; C locals
         * cannot flow into those guest registers through the intervening GUEST_CALLs */
        R2 = camera;
        R3 = CAMERA_SCROLL_STEP;
    }

    /* --- Write back progress and scroll position --- */
    ST32(self + 0x40, progress);
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render_info + 0x18, progress);
    bg_scroll_apply(TILE_ANIM_TOTAL - progress, progress);

    /* --- When animation completes (progress reached zero): resolve outcome --- */
    if (progress == 0) {
        u32 battle_state = LD32(toolkit + oToolkit_BattleStatePtr);

        if (LD8(battle_state + oBattleState_BattlePhase) == 1) {
            render_enemy_virus_names();
        } else {
            damage_player_leave_1hp(0);
            damage_player_leave_1hp(1);
        }

        /* Choose next state based on the mode byte in self */
        u8 mode = LD8(self + oS20364C0_FirstBattleTutorialStep);
        bool go_wait;

        if (mode == 0xFF) {
            go_wait = true;
        } else if (mode != 5) {
            go_wait = false;
        } else {
            u32 bs = LD32(toolkit + oToolkit_BattleStatePtr);
            go_wait = (LD8(bs + 7) >= 5);
        }

        if (go_wait) {
            ST8(self + oS20364C0_JumpOffset01, JUMP_OFFSET_WAIT_SEL);
        } else {
            ST8(self + oS20364C0_JumpOffset01, JUMP_OFFSET_TRIGGER);
        }
        ST16(self + oS20364C0_StateHandled, 0);
    }

    /* --- Finalize: call object method that produces this function's return registers --- */
    s20364c0_finalize(self);
}

/* battle_custMenuAnimateCursorToOK_8026B04(r5=self, r10=toolkit, r4, r6=state for battleObject_chipSelectAnimTick_8029C08) -> all regs + flags */
Next battle_custMenuAnimateCursorToOK_8026B04(void) {
    s20364c0_update(R5, R10, R4, R6);
    RETURN_TO_CALLER();
}

/* ── scrollStrip_update_8026BF4 ── */
/* register form: calls dispatch_801DACC outside this file, which has no typed C interface yet */
static void call_dispatch_801DACC(u32 flags) {
    R0 = flags;
    GUEST_CALL(dispatch_801DACC);
}

/* register form: calls sub_8029D80 outside this file, which has no typed C interface yet */
static void call_sub_8029D80(void) {
    GUEST_CALL(sub_8029D80);
}

/* register form: calls clearBattleStateUnk11Flag_800A9D6 outside this file, which has no typed C interface yet */
static void call_clear_battle_state_unk11_flag(u32 bit) {
    R0 = bit;
    GUEST_CALL(clearBattleStateUnk11Flag_800A9D6);
}

/* register form: CopyBackgroundTiles takes 6 args in R0-R5 and clobbers R4-R7
 * which hold the loop's saved self/scroll state; R1-R3 output values flow into
 * the subsequent battleObject_chipSelectAnimTick_8029C08 call as register arguments. */
static void call_copy_bg_tiles(u32 j, u32 i, u32 block, u32 tile_ids, u32 j_size, u32 i_size) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    R0 = j; R1 = i; R2 = block; R3 = tile_ids; R4 = j_size; R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 call_get_battle_mode(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls battle_setNaviStatusGfx_801E71C outside this file, which has no typed C interface yet */
static void call_sub_801E71C(u32 delta, u32 value) {
    R0 = delta;
    R1 = value;
    GUEST_CALL(battle_setNaviStatusGfx_801E71C);
}

/* register form: battleObject_chipSelectAnimTick_8029C08 takes R2-R7 as register context. R2/R3 carry either
 * the camera pointer + scroll constant (mode != 1) or opaque CopyBackgroundTiles
 * output (mode 1), which can only live in guest registers between the calls. */
static void call_sub_8029C08(u32 self) {
    R5 = self;
    GUEST_CALL(battleObject_chipSelectAnimTick_8029C08);
}

#define TILE_BLOCK_ALL_ZERO  ((u32)0x08026C88)

/* Background-scroll state update (S20364C0 object in r5, toolkit in r10):
 *  - one-shot init (zero extra pointers, dispatch BG update, clear a battle flag)
 *  - scroll the tile map one column (or two) and copy background tiles
 *  - shift camera by one tile-block unless in battle mode 1
 *  - publish scroll position to state object and render-info
 *  - notify scroll subsystem; wrap at end of strip
 *  - per-frame update callback                                    */
static void sub_8026BF4_impl(u32 self, u32 toolkit) {
    /* First-run init: set the gate flag, clear extra pointers, dispatch
     * a BG update, and clear battle-state bit 2 of unk11. */
    if (LD8(self + oS20364C0_StateHandled) == FALSE) {
        ST32(self + oS20364C0_Extra_Unk_44, 0);
        ST32(self + oS20364C0_Extra_Unk_40, 0);
        ST8(self + oS20364C0_StateHandled, TRUE);
        call_dispatch_801DACC(0x2000);
        call_sub_8029D80();
        call_clear_battle_state_unk11_flag(4);
    }

    /* Compute the new scroll position and how many tile columns to copy
     * (2 if bit-2 of scroll_pos is clear, else 1). */
    u32 scroll_pos = LD32(self + oS20364C0_Extra_Unk_40) + 0xC;
    u32 iterations = (((4u & scroll_pos) >> 2) ^ 1u) + 1u;

    /* Increment the tile counter and copy one column of BG tiles per pass. */
    for (u32 n = iterations; n != 0; n--) {
        u32 j = LD32(self + oS20364C0_Extra_Unk_44);
        ST32(self + oS20364C0_Extra_Unk_44, j + 1);
        call_copy_bg_tiles(j, 0, 3, TILE_BLOCK_ALL_ZERO, 1, 0x14);
    }

    /* Outside battle mode 1, shift the camera Y by one tile-block (0x18000).
     * R2/R3 must hold the camera pointer and constant as register arguments
     * for the battleObject_chipSelectAnimTick_8029C08 call that follows. */
    if (call_get_battle_mode() != 1) {
        u32 camera = LD32(toolkit + oToolkit_CameraPtr);
        ST32(camera + 0x34, LD32(camera + 0x34) + 0x18000);
        /* register form: R2/R3 are register args consumed by battleObject_chipSelectAnimTick_8029C08 below */
        R2 = camera;
        R3 = 0x18000;
    }

    /* Publish the new scroll position to the state object and render-info. */
    ST32(self + oS20364C0_Extra_Unk_40, scroll_pos);
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render_info + 0x18, scroll_pos);

    /* Notify the scrolling subsystem; wrap the strip when fully scrolled. */
    call_sub_801E71C(0x78 - scroll_pos, scroll_pos);
    if (scroll_pos == 0x78) {
        ST8(self + 0x1, LD8(self + 0xB));
        ST8(self + 0x2, 0);
    }

    /* Per-frame update callback. */
    call_sub_8029C08(self);
}

/* scrollStrip_update_8026BF4(self: * S20364C0 $r5, toolkit: * $r10) */
Next scrollStrip_update_8026BF4(void) {
    sub_8026BF4_impl(R5, R10);
    RETURN_TO_CALLER();
}

/* ── custMenuUpdateWaitSelectionState_8026CCC ── */
/* custMenuUpdateWaitSelectionState_8026CCC — per-frame update of the S20364C0 custom-menu object (self in R5).
 *
 * Asks battleObject_computePuppeteerSwitchElement_802A220 for the menu's current result:
 *   - anything but 0xFF: latch it into the object at +0x38, switch the object
 *     to its 0x40 handler (which routes through custMenuMainMaybe_8026A88 and
 *     triggers battleObject_chipSelectUpdate), clear the frame counter at +0x40, and return.
 *   - 0xFF (nothing chosen yet): run the menu's update handlers with the
 *     current joypad state and tick the frame counter at +0x40. */
Next custMenuUpdateWaitSelectionState_8026CCC(void) {
    GUEST_CALL(battleObject_computePuppeteerSwitchElement_802A220);
    u32 result = R0;

    if (result != 0xFF) {
        SET_FLAGS_CMP(result, 0xFF);

        /* record the selection and hand control to the 0x40 state handler */
        ST16(R5 + oS20364C0_PuppeteerSwitchElement, result);
        ST8(R5 + 1, 0x40);   /* oS20364C0_JumpOffset01 */
        ST8(R5 + 2, 0);      /* oS20364C0_StateHandled */
        ST32(R5 + 0x40, 0);

        R0 = 0;
        SET_FLAGS_NZ(0);
    } else {
        /* menu still open: feed it the joypad, update it, tick its counter */
        u32 savedR7 = R7;

        R7 = LD32(R10 + 4);   /* oToolkit_JoypadPtr */

        GUEST_CALL(custMenuSomeHandler_8028B74); /* (self, joypad) */
        GUEST_CALL(battle_custMenuDispatchSelection_8028820);
        GUEST_CALL(battleObject_chipSelectAnimTick_8029C08);                 /* (self) */
        GUEST_CALL(sub_802899C);
        GUEST_CALL(battle_endingBgUpdate_8029D34);

        u32 counter = LD32(R5 + 0x40);
        R0 = counter + 1;
        ST32(R5 + 0x40, R0);
        SET_FLAGS_ADD(counter, 1);

        /* r7 is only scratch here; the original pops it back for the caller */
        R7 = savedR7;
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_stateMaybeFinishText_8026E4C ── */
/* chatbox_stateMaybeFinishText_8026E4C (r5 = chatbox state struct, S20364C0):
 * Advance the chatbox one step. If flag 0x80 of eFlags2009F38 is set, ask
 * battleObject_computePuppeteerSwitchElement_802A220 whether the text is still busy: while it reports 0xFF, just
 * run the periodic update. Otherwise (flag clear, or text finished) play
 * sound effect 0x9E, set the state byte at +1 to 4, clear the timer
 * halfword at +2, and run the periodic update. */
Next chatbox_stateMaybeFinishText_8026E4C(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    if (R0 != 0) {
        GUEST_CALL(battleObject_computePuppeteerSwitchElement_802A220);
        if (R0 == 0xFF) {
            GUEST_CALL(battleObject_chipSelectAnimTick_8029C08);
            RETURN_TO_CALLER();
        }
        GUEST_CALL(chatbox_8040818);
    }

    R0 = 0x9E;
    GUEST_CALL(PlaySoundEffect);

    ST8(R5 + 1, 4);
    ST16(R5 + 2, 0);

    GUEST_CALL(battleObject_chipSelectAnimTick_8029C08);
    RETURN_TO_CALLER();
}

/* ── battle_updateObjectByKind_8026E98 ── */
/* battle_updateObjectByKind_8026E98(self = R5): one update step for a battle object.
 *
 * Reads the global battle effect flags and passes 4 in R4 if the 0x200000
 * effect is active, 0 otherwise. It then dispatches through the handler
 * table at off_8026EBC, indexed by the kind byte at self+2, calling the
 * selected handler with the kind in R0, its own thumb pointer in R1,
 * self in R5 and the effect flag in R4. Finally it runs battleObject_chipSelectAnimTick_8029C08 on
 * the same object.
 *
 * R4 is only the effect flag for the duration of the calls; the caller's
 * value is restored on return (push/pop {r4} in the original).
 *
 * Flags before the calls mirror the original's TST/MOV pair:
 *   - effect present: N/Z from "movs r4, #4"
 *   - effect absent:  N/Z from "tst r0, #0x200000" (Z set)
 * C/V are untouched by both. The flags on return are whatever
 * battleObject_chipSelectAnimTick_8029C08 leaves. */
Next battle_updateObjectByKind_8026E98(void) {
    u32 self = R5;
    u32 saved_r4 = R4;

    R4 = 0;
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 0x200000) {
        R4 = 4;
        SET_FLAGS_NZ(4);
    } else {
        SET_FLAGS_NZ(effects & 0x200000);
    }

    u32 kind = LD8(self + 2);
    R0 = kind;
    R1 = LD32(A_off_8026EBC + kind); /* thumb handler pointer from the ROM table */
    switch (kind) {
    case 0: GUEST_CALL(runBattleDialog_8026EC8); break;
    case 4: GUEST_CALL(battle_custMenuTickPlayerChipCursor_8026F1A); break;
    case 8: GUEST_CALL(sub_8026FAA); break;
    }

    GUEST_CALL(battleObject_chipSelectAnimTick_8029C08);
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── runBattleDialog_8026EC8 ── */
/* Register form: GetBattleEffects has no typed C interface yet. */
static u32 get_battle_effects(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* Register form: GetBattleMode has no typed C interface yet. */
static u32 get_battle_mode_sub_8026EC8(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* Register form: chatbox_runScript has no typed C interface yet. */
static void chatbox_run_script(u32 archive, u32 script_idx) {
    R0 = archive;
    R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

/* Register form: PlaySoundEffect has no typed C interface yet. */
static void play_sound_effect_sub_8026EC8(u32 id) {
    R0 = id;
    GUEST_CALL(PlaySoundEffect);
}

/* Runs the tutorial/battle-run dialog script appropriate for the current state
 * and plays the unselect jingle. r5 points to a small battle-state object
 * (+2 = script-result slot, +0xC = tutorial-index byte); r4 is a base pointer
 * into the per-row text-script indices used by the run-away dialog. */
static void run_battle_dialog(u32 base_r4, u32 state_ptr) {
    u32 effects = get_battle_effects();

    if (effects & 0x20) {
        ST16(state_ptr + 2, 4);
        chatbox_run_script(A_TextScriptBattleRunDialog, base_r4);
    } else {
        ST16(state_ptr + 2, 8);
        if (get_battle_mode_sub_8026EC8() == 5) {
            chatbox_run_script(LD32(A_TextScriptFirstBattleTutorials + 0x0C), 0x23);
        } else {
            u32 idx = LD8(state_ptr + 0xC);
            if (idx == 0xFF) {
                chatbox_run_script(A_TextScriptBattleRunDialog, base_r4 + 3);
            } else {
                chatbox_run_script(LD32(A_TextScriptFirstBattleTutorials + (idx << 2)),
                                   LD8(A_byte_8027034 + idx));
            }
        }
    }

    play_sound_effect_sub_8026EC8(SOUND_UNSELECT_7B);
}

Next runBattleDialog_8026EC8(void) {
    run_battle_dialog(R4, R5);
    RETURN_TO_CALLER();
}

/* ── battle_runTutorialDialog_8026FC0 ── */
/* Struct offsets for the battle-run tutorial state object (S20364C0). */
#define oBattleRunSelf_Timer  1
#define oBattleRunSelf_State  2
#define oBattleRunSelf_Mode   4

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 call_GetBattleEffects(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls battle_ClearPendingEventFlag_802A0EC outside this file, which has no typed C interface yet */
static void call_battle_ClearPendingEventFlag_802A0EC(void) {
    GUEST_CALL(battle_ClearPendingEventFlag_802A0EC);
}

/* register form: calls chatbox_mask_eFlags2009F38 outside this file, which has no typed C interface yet */
static u32 call_chatbox_mask_eFlags2009F38(u32 flag) {
    R0 = flag;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return R0;
}

/* register form: calls chatbox_runScript outside this file, which has no typed C interface yet */
static void call_chatbox_runScript(u32 archive, u32 script_idx) {
    R0 = archive;
    R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

/* Drive the "battle is about to run away" tutorial dialog.
 *  - On first entry (state == 0): mark the tutorial seen (state := 1),
 *    clear the pending event flag, then run the tutorial script at page
 *    1 (mode == 2) or page 2 (mode != 2), plus the page offset (4) that
 *    comes from GetBattleEffects() & 0x200000.
 *  - On subsequent entries, while chatbox flag 0x80 is busy, do nothing.
 *    Once it clears, set timer = 0x14 and zero the 16-bit state field. */
static u32 run_battle_run_tutorial(u32 self) {
    u32 page_offset = (call_GetBattleEffects() & 0x200000u) ? 4u : 0u;

    u8 state = LD8(self + oBattleRunSelf_State);
    if (state == 0) {
        ST8(self + oBattleRunSelf_State, 1);
        call_battle_ClearPendingEventFlag_802A0EC();
        /* mov r1, #1; if (mode != 2) mov r1, #2; add r1, r4
         * So r1 == 1 when mode == 2, else r1 == 2; then +page_offset. */
        u32 script_idx = (LD8(self + oBattleRunSelf_Mode) == 2) ? 1u : 2u;
        script_idx += page_offset;
        call_chatbox_runScript(A_TextScriptBattleRunDialog, (u8)script_idx);
        return 0;
    }

    u32 mask = call_chatbox_mask_eFlags2009F38(0x80);
    if (mask != 0)
        return (u32)mask;
    ST8(self + oBattleRunSelf_Timer, 0x14);
    ST16(self + oBattleRunSelf_State, 0);
    return 0;
}

Next battle_runTutorialDialog_8026FC0(void) {
    u32 result = run_battle_run_tutorial(R5);
    SET_FLAGS_NZ(result);
    RETURN_TO_CALLER();
}

/* ── sub_802728C ── */
/* Battle chat window object (S20364C0): fields used by the state dispatcher. */
#define oChatbox_State 2     /* u8: state byte, used as a raw BYTE offset into the
                              * handler table below, so live states are 0,4,8,12,16 */
#define oChatbox_Work  0x40  /* word handed to every state handler in r4 */

/* State-handler table.  The original picks the target with an unscaled
 * "ldr r1,[r1,r0]": the state byte indexes the table in bytes, one word slot
 * per state value (0x00,0x04,0x08,0x0C,0x10). */
#define CHATBOX_HANDLER_TABLE 0x080272A4

/* The five table entries (function addresses with the Thumb bit set). */
#define CHATBOX_H_LOAD_OBJ_AND_SET_STATE4 (0x080272B8 + 1) /* state 0x00 */
#define CHATBOX_H_80272D0                 (0x080272D0 + 1) /* state 0x04 */
#define CHATBOX_H_SET_STATE4_IF_FLAG80    (0x08027320 + 1) /* state 0x08 */
#define CHATBOX_H_8027332                 (0x08027332 + 1) /* state 0x0C */
#define CHATBOX_H_8027394                 (0x08027394 + 1) /* state 0x10 */

/* register form: calls battle_loadObjTableEntryAndSetState4_80272B8 outside this file, which has no typed C interface yet */
static void chatbox_state_load_obj_table_entry(u32 self, u32 work, u32 state) {
    R0 = state;                                  /* the state byte itself */
    R1 = CHATBOX_H_LOAD_OBJ_AND_SET_STATE4;      /* handler's own address, as the table load leaves r1 */
    R4 = work;
    R5 = self;
    GUEST_CALL(battle_loadObjTableEntryAndSetState4_80272B8);
}

/* register form: calls battle_tutorialRunScriptStep outside this file, which has no typed C interface yet */
static void chatbox_state_80272D0(u32 self, u32 work, u32 state) {
    R0 = state;
    R1 = CHATBOX_H_80272D0;
    R4 = work;
    R5 = self;
    GUEST_CALL(battle_tutorialRunScriptStep);
}

/* register form: calls chatbox_setState4IfFlag80Clear_8027320 outside this file, which has no typed C interface yet */
static void chatbox_state_set_state4_if_flag80_clear(u32 self, u32 work, u32 state) {
    R0 = state;
    R1 = CHATBOX_H_SET_STATE4_IF_FLAG80;
    R4 = work;
    R5 = self;
    GUEST_CALL(chatbox_setState4IfFlag80Clear_8027320);
}

/* register form: calls sub_8027332 outside this file, which has no typed C interface yet */
static void chatbox_state_8027332(u32 self, u32 work, u32 state) {
    R0 = state;
    R1 = CHATBOX_H_8027332;
    R4 = work;
    R5 = self;
    GUEST_CALL(sub_8027332);
}

/* register form: calls sub_8027394 outside this file, which has no typed C interface yet */
static void chatbox_state_8027394(u32 self, u32 work, u32 state) {
    R0 = state;
    R1 = CHATBOX_H_8027394;
    R4 = work;
    R5 = self;
    GUEST_CALL(sub_8027394);
}

/* register form: calls battleObject_chipSelectAnimTick_8029C08 outside this file, which has no typed C interface yet */
static void chatbox_common_update(void) {
    /* No argument: run with the r5 the state handler left behind, exactly as
     * the original's "bl battleObject_chipSelectAnimTick_8029C08" does (it never reloads r5). */
    GUEST_CALL(battleObject_chipSelectAnimTick_8029C08);
}

/* Advance the battle chat window one frame: run the handler picked by the
 * state byte from the table at 0x080272A4, then always run the common
 * follow-up update.  Returns the caller's r4, which the original only saves
 * and restores around the whole sequence. */
static u32 chatbox_step(u32 self, u32 saved_r4) {
    u32 work = LD32(self + oChatbox_Work);
    u32 state = LD8(self + oChatbox_State);

    /* Same fetch the original performs: an unscaled word load from the handler
     * table at the raw byte offset stored in the state field. */
    u32 handler = LD32(CHATBOX_HANDLER_TABLE + state);

    switch (handler) {
    case CHATBOX_H_LOAD_OBJ_AND_SET_STATE4: /* load obj table entry, move to state 4 */
        chatbox_state_load_obj_table_entry(self, work, state);
        break;
    case CHATBOX_H_80272D0:
        chatbox_state_80272D0(self, work, state);
        break;
    case CHATBOX_H_SET_STATE4_IF_FLAG80:    /* state 4 unless flag 0x80 is set */
        chatbox_state_set_state4_if_flag80_clear(self, work, state);
        break;
    case CHATBOX_H_8027332:
        chatbox_state_8027332(self, work, state);
        break;
    case CHATBOX_H_8027394:
        chatbox_state_8027394(self, work, state);
        break;
    default:
        /* Unreachable with live game data: any other state byte would send
         * the original's bx outside the table (a crash). */
        break;
    }

    chatbox_common_update();
    return saved_r4;
}

/* sub_802728C(r5 = chat window object).
 * r2/r3/r5/r6/r8-r12 end up as whatever the callees leave behind. */
Next sub_802728C(void) {
    R4 = chatbox_step(R5, R4);
    RETURN_TO_CALLER();
}

/* ── battle_loadObjTableEntryAndSetState4_80272B8 ── */
/* battle_loadObjTableEntryAndSetState4_80272B8: load the AI/behavior table entry selected by the object's
 * state byte (+0xC) into the object (+0x40) and switch its state to 4.
 * Inputs: R5 = object pointer. Outputs: R0 = 4, R1 = table base, nz/c. */
Next battle_loadObjTableEntryAndSetState4_80272B8(void) {
    u32 tableBase = A_off_8027BEC;

    u32 index = LD8(R5 + 0xC);
    /* carry flag comes from the bit shifted out of the LSL by 2
     * (a byte value shifted left twice can never set it, but keep it exact) */
    u32 carryOut = (index >> 30) & 1;

    ST32(R5 + 0x40, LD32(tableBase + (index << 2)));

    R1 = tableBase;
    R0 = 4;
    cpu.c = carryOut;
    SET_FLAGS_NZ(R0); /* flags as left by "mov r0, #4" */

    ST16(R5 + 2, 4);
    RETURN_TO_CALLER();
}

/* ── chatbox_setState4IfFlag80Clear_8027320 ── */
/* chatbox_setState4IfFlag80Clear_8027320: test the 0x80 bit of the chatbox event flags (eFlags2009F38).
 * If it is not set, write halfword 4 to [r5+2] (presumably a chatbox state
 * or timer field). Either way r0 holds the flag-test result and the flags
 * reflect the last comparison/move as in the original. */
Next chatbox_setState4IfFlag80Clear_8027320(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    SET_FLAGS_CMP(R0, 0);
    if (R0 == 0) {
        R0 = 4;
        ST16(R5 + 2, R0);
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8027332 ── */
/* sub_8027332 -- a scripted-batch step: spawn the rest of the current batch of
 * entries (one sub_8001040 call each), then count the step's wait timer down
 * and end the step when it expires or the player cuts it short with buttons. */

/* fields of the owning object (r5) */
#define oScriptStep_State      0x02  /* halfword: state number; 4 moves the step on */
#define oScriptStep_ScriptPos  0x40  /* word: where to resume the script           */
#define oScriptStep_WaitTimer  0x44  /* word: frames of waiting left               */

/* ROM table (byte_8027388) supplying sub_8001040's r2 argument, indexed by the
 * low 7 bits of an entry's flags byte (first words 0, 0x40000000, then the ROM
 * data that follows -- as far in as the original can read). */
#define ENTRY_ARG2_TABLE A_byte_8027388

/* register form: calls sub_8001040 outside this file, which has no typed C interface yet.
 * register form: also returns the call's leftover r3 -- the original hands that value back
 * to its callers whenever it skips the joypad read, and it exists only in the guest
 * register file, so C cannot reconstruct it. */
static u32 spawn_batch_entry(u8 a, u8 b, u32 arg2) {
    R0 = a;
    R1 = b;
    R2 = arg2;
    R3 = 0xB37A;  /* fixed arguments at this call site */
    R4 = 1;
    GUEST_CALL(sub_8001040);
    return R3;
}

/* Entries are three bytes (a, b, flags) beginning one byte past script_pos; bit 7 of
 * the flags byte marks the batch's last entry, its low 7 bits pick the r2 argument.
 * After the batch, the wait timer ticks down and the step ends -- script position
 * saved, state 4 -- when it reaches zero, or early on the A button once no more than
 * 0x44 frames remain, or on the B button at any point in that window.
 * Returns the value to leave in r3: the joypad pointer when it was read, otherwise
 * the last spawn call's leftover r3. */
static u32 run_batch_and_wait(u32 script_pos, u32 owner, u32 toolkit) {
    u32 pos = script_pos + 1;  /* skip the byte in front of the first entry */
    u32 leftover_r3;

    bool last_entry;
    do {
        u8 flags = LD8(pos + 2);
        leftover_r3 = spawn_batch_entry(LD8(pos), LD8(pos + 1),
                          LD32(ENTRY_ARG2_TABLE + ((flags & 0x7F) << 2)));
        last_entry = (LD8(pos + 2) & 0x80) != 0;  /* re-read after the call, as the original does */
        pos += 3;
    } while (!last_entry);

    u32 frames_left = LD32(owner + oScriptStep_WaitTimer) - 1;
    ST32(owner + oScriptStep_WaitTimer, frames_left);

    u32 r3 = leftover_r3;
    bool step_done = frames_left == 0;
    if (!step_done && (s32)frames_left <= 0x44) {
        u32 joypad = LD32(toolkit + oToolkit_JoypadPtr);
        r3 = joypad;  /* the original keeps the joypad pointer in r3 from here on */
        step_done = (LD16(joypad + 2) & 1) != 0   /* A held    */
                 || (LD16(joypad + 4) & 2) != 0;  /* B pressed */
    }

    if (step_done) {
        ST32(owner + oScriptStep_ScriptPos, pos);
        ST16(owner + oScriptStep_State, 4);
    }

    return r3;
}

/* sub_8027332(r4 = entry-list position, r5 = owning object, r10 = toolkit).
 * r2 keeps whatever the last spawn call left there; r5/r6/r10 are untouched. */
Next sub_8027332(void) {
    R3 = run_batch_and_wait(R4, R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_8027394 ── */
/* sub_8027394(r5 = object/state pointer):
 * Reset the entity at r5 to state 4 and clear its halfword field.
 * Leaves r0 = 0 with flags set accordingly. */
Next sub_8027394(void) {
    ST8(R5 + 1, 4);
    R0 = 0;
    ST16(R5 + 2, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80273A4 ── */
/* Battle-object mode dispatcher (sub_80273A4).
 *
 * The original is a trampoline: it reads the object's mode byte at [self+2],
 * loads [self+0x40] into r4 as the handler's context argument, looks the
 * handler up in a jump table indexed by the raw mode byte, and tail-calls it
 * with mov lr,pc / bx.  The callee's register results (r2, r3, r5-r12) and
 * flags are simply whatever the handler leaves behind; the trailing
 * pop {r4,pc} restores the caller's r4 and returns to the original caller.
 * The table lookup is not range-checked, so any mode byte outside 0..0x10
 * still jumps to whatever word sits at table+mode - reproduced here with a
 * call through the computed address. */

/* register form: the handler address comes from a jump table at runtime, so
 * the call must go through its code address; the handler is entered with
 * r0 = mode byte, r1 = table entry (as the original leaves it), r4 = context. */
static void call_mode_handler(u32 target, u32 mode, u32 context) {
    R0 = mode;
    R1 = target;
    R4 = context;
    GUEST_CALL_ADDR(target);
}

static void dispatch_battle_object_mode(u32 self) {
    u32 context = LD32(self + 0x40);          /* per-object handler context */
    u32 mode    = LD8(self + 2);              /* handler selector byte */
    u32 target  = LD32(A_off_80273B8 + mode); /* jump-table entry, byte-indexed */

    call_mode_handler(target, mode, context);
}

/* sub_80273A4(r5 = battle object, r10/lr pass through to the handler)
 * -> r2 r3 r5 r6 r8 r9 r10 r11 r12 from the handler; r4 preserved. */
Next sub_80273A4(void) {
    u32 r4_in = R4;
    dispatch_battle_object_mode(R5);
    R4 = r4_in;                /* pop {r4,pc} restores the caller's r4 */
    RETURN_TO_CALLER();
}

/* ── battle_object_SelectHandlerByMode_80273CC ── */
/* battle_object_SelectHandlerByMode_80273CC (R5 = battle object):
 * The object re-selects its current handler: read the mode byte at +0xC,
 * look up the routine pointer indexed by it in the table at off_8027C84,
 * store that pointer into the object at +0x40, and set the halfword state
 * field at +2 to 4.
 *
 * On return: R0 = 4, R1 = table address, N/Z from "MOV r0,#4" (both clear)
 * and C from the "LSL r0,r0,#2" (bit 30 of the mode byte). */
Next battle_object_SelectHandlerByMode_80273CC(void) {
    u32 mode = LD8(R5 + 0xC);

    R1 = A_off_8027C84;

    /* lsl r0, r0, #2 — index = mode * 4, and C is the bit shifted out
     * (bit 30 of the byte).  Emulate that carry exactly with a doubling
     * add: x + x carries out bit 31 of x, so doubling (mode << 1) carries
     * out bit 30 of mode, just like the shift.  N/Z here are overwritten
     * by the MOV below, same as in the original. */
    SET_FLAGS_ADD(mode << 1, mode << 1);
    u32 index = mode << 2;

    R0 = LD32(R1 + index);
    ST32(R5 + 0x40, R0);

    R0 = 4;
    ST16(R5 + 2, R0);

    SET_FLAGS_NZ(R0); /* mov r0, #4 updates N and Z only */
    RETURN_TO_CALLER();
}

/* ── sub_80273E4 ── */
/* sub_80273E4(r5 = pointer to a byte):
 * store 8 into the byte at r5; r0 = 8. */
Next sub_80273E4(void) {
    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(R5, R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_resetTextStateIfHidden_80273EC ── */
/* chatbox_resetTextStateIfHidden_80273EC(r5 = textbox object S20364C0):
 * Unless the chatbox "shown" flag (0x80 of eFlags2009F38) is set, reset the
 * textbox state: copy the stored line count/character at +0xD into the active
 * slot at +1 and clear the halfword at +2 (character timer/index).
 * Then update the textbox via battleObject_chipSelectAnimTick_8029C08. */
Next chatbox_resetTextStateIfHidden_80273EC(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (R0 == 0) {
        ST8(R5 + 1, LD8(R5 + 0xD));
        ST16(R5 + 2, 0);
    }
    GUEST_CALL(battleObject_chipSelectAnimTick_8029C08); /* self stays in r5 */
    RETURN_TO_CALLER();
}

/* ── battleObject_chipSelectAnimTick_802770C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_battleObject_chipSelectAnimTick_8029C08_result;
/* register form: generated wrapper: battleObject_chipSelectAnimTick_8029C08 has no typed C interface yet */
static inline call_battleObject_chipSelectAnimTick_8029C08_result call_battleObject_chipSelectAnimTick_8029C08(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_chipSelectAnimTick_8029C08);
    return (call_battleObject_chipSelectAnimTick_8029C08_result){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battleObject_chipSelectAnimTick_802770C -- one tick of the chip-select
 * animation for a battle object.
 *
 * The object's current animation step lives in the byte at self+2.  That byte
 * is a byte offset into the handler table at off_8027724, and the tick starts
 * by calling the selected handler through it (mov lr, pc / bx r1 in the
 * original), with r0 = step and r1 still holding the target address.  After
 * the handler returns, battleObject_chipSelectAnimTick_8029C08 runs on the
 * (possibly updated) object pointer.
 *
 * Register flow, matching the original:
 *   - r8-r12 are only ever written by the state handler; 8029C08 preserves
 *     them, so they stay in the guest register file and never pass through C.
 *   - the final r2-r6 come from 8029C08, so they are returned to the adapter
 *     explicitly.  r0, r1, r7 and the flags are clobbers callers ignore.
 */

struct ChipSelectTickResult {
    u32 r2;
    u32 r3;
    u32 r4;
    u32 r5;
    u32 r6;
};

/* register form: the step handler is reached through a jump-table address
 * (mov lr, pc; bx r1), and the handler's r5 (object pointer) has to be read
 * back out of the guest register file after the call, since the handler may
 * update it.  Its other results are left in the guest registers on purpose:
 * r8-r12 must survive into this function's callers, and r10 is consumed
 * directly from there by the 8029C08 wrapper. */
static u32 run_chip_select_state_handler(u32 handler, u32 step) {
    R0 = step;          /* original calls the handler with r0 = step byte */
    R1 = handler;       /* ...and r1 still holding the branch target */
    GUEST_CALL_ADDR(handler);
    return R5;
}

static struct ChipSelectTickResult chip_select_anim_tick(u32 self) {
    u32 step = LD8(self + 2);
    u32 handler = LD32(A_off_8027724 + step);

    self = run_chip_select_state_handler(handler, step);

    call_battleObject_chipSelectAnimTick_8029C08_result tick =
        call_battleObject_chipSelectAnimTick_8029C08(self);

    struct ChipSelectTickResult out;
    out.r2 = tick.r2;
    out.r3 = tick.r3;
    out.r4 = tick.r4;
    out.r5 = tick.r5;
    out.r6 = tick.r6;
    return out;
}

/* battleObject_chipSelectAnimTick_802770C(r5 = self, r10) -> r2 r3 r4 r5 r6
 * (r8-r12 pass through from the state handler) */
Next battleObject_chipSelectAnimTick_802770C(void) {
    struct ChipSelectTickResult out = chip_select_anim_tick(R5);
    R2 = out.r2;
    R3 = out.r3;
    R4 = out.r4;
    R5 = out.r5;
    R6 = out.r6;
    RETURN_TO_CALLER();
}

/* ── object_setStatePhase4_8027738 ── */
/* object_setStatePhase4_8027738: initialize some object state at r5.
 * Writes: [r5+0x40] = 0 (u32), [r5+0x17] = 1 (u8), [r5+0xf] = 1 (u8), [r5+2] = 4 (u16).
 * No register outputs; the original also leaves r0 = 4, but callers ignore it. */
static void init_object_state(u32 obj) {
    ST32(obj + 0x40, 0);
    ST8(obj + 0x17, 1);
    ST8(obj + 0xF, 1);
    ST16(obj + 2, 4);
}

Next object_setStatePhase4_8027738(void) {
    init_object_state(R5);
    RETURN_TO_CALLER();
}

/* ── startFadeShakeTransition_802774C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3, r5; } call_SetScreenFade_result_sub_802774C;
/* register form: generated wrapper: SetScreenFade has no typed C interface yet */
static inline call_SetScreenFade_result_sub_802774C call_SetScreenFade_sub_802774C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetScreenFade);
    return (call_SetScreenFade_result_sub_802774C){ R3, R5 };
}

typedef struct { u32 r2; } call_sub_80302B6_result;
/* register form: generated wrapper: cameraSetShake has no typed C interface yet */
static inline call_sub_80302B6_result call_sub_80302B6(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(cameraSetShake);
    return (call_sub_80302B6_result){ R2 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_sub_802774C;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_802774C call_PlaySoundEffect_sub_802774C(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_802774C){ R1, R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Play the screen-fade + sound sequence that opens a transition, then mark
 * the work object (r5) with state 8. Field 0x40 is cleared first, the fade
 * is set up (SetScreenFade), a helper kicks off the transition, and three
 * sound effects play in order. r2 flows from cameraSetShake through the three
 * PlaySoundEffect calls (each leaves the next r2 behind); the registers the
 * original merely leaves behind (r2-r6, r10) come straight from the last
 * wrapper call, so nothing needs to be written back by hand. */
static void start_screen_fade_transition(u32 work) {
    ST32(work + 0x40, 0);                /* clear work+0x40 */

    call_SetScreenFade_sub_802774C(0x64, 8);         /* fade setup: 100 / 8 */

    u32 r2 = call_sub_80302B6(1, 0x28).r2;
    r2 = call_PlaySoundEffect_sub_802774C(0x193, r2).r2;              /* 0x94 + 0xFF */
    r2 = call_PlaySoundEffect_sub_802774C(SOUND_MENU_SELECT, r2).r2;
    call_PlaySoundEffect_sub_802774C(0xBC, r2);

    ST16(work + 2, 8);                   /* work state = 8 */
}

/* startFadeShakeTransition_802774C(r5 = work object, r10 = context for cameraSetShake)
 * -> leaves r2 r3 r4 r5 r6 r10 as the callees left them. */
Next startFadeShakeTransition_802774C(void) {
    start_screen_fade_transition(R5);
    RETURN_TO_CALLER();
}

/* ── object_tickFadeHoldThenSetState12_802777C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r3; } call_IsScreenFadeActive_result;
/* register form: generated wrapper: IsScreenFadeActive has no typed C interface yet */
static inline call_IsScreenFadeActive_result call_IsScreenFadeActive(void) {
    GUEST_CALL(IsScreenFadeActive);
    return (call_IsScreenFadeActive_result){ R0, R1, R3 };
}

/* Advance the fade-hold counter stored at [obj+0x40].  Once it has counted up
 * to 50 and no screen fade is currently running, write 0xC into the halfword
 * state field at [obj+2].  While the counter is still below 50, or while a
 * fade is active, nothing else happens this tick.
 *
 * r3 is untouched by this code itself; on the path that calls
 * IsScreenFadeActive it is simply whatever that callee leaves behind, so the
 * wrapper call takes care of it and nothing is passed through here. */
static void tick_fade_hold_counter(u32 obj) {
    u32 count = LD32(obj + 0x40) + 1;
    ST32(obj + 0x40, count);

    if ((s32)count >= 0x32 && call_IsScreenFadeActive().r0 == 0)
        ST16(obj + 2, 0xC);
}

/* object_tickFadeHoldThenSetState12_802777C(r5 = object work area) */
Next object_tickFadeHoldThenSetState12_802777C(void) {
    tick_fade_hold_counter(R5);
    RETURN_TO_CALLER();
}

/* ── battle_customScreenExitRotateChips_8027796 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3, r4, r5, r6; } call_battle_getActorEmotion_802A564_result_sub_8027796;
/* register form: generated wrapper: battle_getActorEmotion_802A564 has no typed C interface yet */
static inline call_battle_getActorEmotion_802A564_result_sub_8027796 call_battle_getActorEmotion_802A564_sub_8027796(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_getActorEmotion_802A564);
    return (call_battle_getActorEmotion_802A564_result_sub_8027796){ R0, R2, R3, R4, R5, R6 };
}

typedef struct { u32 r5, r7; } call_e_loadHudBattleFormByte_result;
/* register form: generated wrapper: e_loadHudBattleFormByte has no typed C interface yet */
static inline call_e_loadHudBattleFormByte_result call_e_loadHudBattleFormByte(u32 r0) {
    R0 = r0;
    GUEST_CALL(e_loadHudBattleFormByte);
    return (call_e_loadHudBattleFormByte_result){ R5, R7 };
}

typedef struct { u32 r0; } call_getLocOfActiveChips_8027E1C_result_sub_8027796;
/* register form: generated wrapper: getLocOfActiveChips_8027E1C has no typed C interface yet */
static inline call_getLocOfActiveChips_8027E1C_result_sub_8027796 call_getLocOfActiveChips_8027E1C_sub_8027796(u32 r0) {
    R0 = r0;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    return (call_getLocOfActiveChips_8027E1C_result_sub_8027796){ R0 };
}

typedef struct { u32 r4, r5, r6, r7; } call_sprite_CopyObjTilesToVramSlot_80281D4_result;
/* register form: generated wrapper: sprite_CopyObjTilesToVramSlot_80281D4 has no typed C interface yet */
static inline call_sprite_CopyObjTilesToVramSlot_80281D4_result call_sprite_CopyObjTilesToVramSlot_80281D4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(sprite_CopyObjTilesToVramSlot_80281D4);
    return (call_sprite_CopyObjTilesToVramSlot_80281D4_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r0, r5, r6; } call_sub_802A034_result;
/* register form: generated wrapper: sub_802A034 has no typed C interface yet */
static inline call_sub_802A034_result call_sub_802A034(void) {
    GUEST_CALL(sub_802A034);
    return (call_sub_802A034_result){ R0, R5, R6 };
}

typedef struct { u32 r2, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_customScreen_ExitSequence_8028E32_result;
/* register form: generated wrapper: customScreen_ExitSequence_8028E32 has no typed C interface yet */
static inline call_customScreen_ExitSequence_8028E32_result call_customScreen_ExitSequence_8028E32(u32 r5) {
    R5 = r5;
    GUEST_CALL(customScreen_ExitSequence_8028E32);
    return (call_customScreen_ExitSequence_8028E32_result){ R2, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r1, r2, r3, r5; bool c, v; } call_SetScreenFade_result_sub_8027796;
/* register form: generated wrapper: SetScreenFade has no typed C interface yet */
static inline call_SetScreenFade_result_sub_8027796 call_SetScreenFade_sub_8027796(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetScreenFade);
    return (call_SetScreenFade_result_sub_8027796){ R1, R2, R3, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Exit sequence for the chip custom screen: rotate the chip-id list, release
 * the freed chip's sprite tiles, mark the new slot as taken and start leaving
 * the screen.
 *
 * The original pushes r4/r6 and pops them at the end, so their entry values
 * must be handed back unchanged; the adapter therefore passes them through.
 * Every other output register (r2, r3, r5, r8-r12) is left behind by the
 * callees themselves, so nothing needs to be computed for them here. */
struct ExitRegs { u32 r4; u32 r6; };

static struct ExitRegs custom_screen_exit(u32 battleObj, u32 r4in, u32 r6in) {
    /* Pick the HUD form byte: 0xC when the actor's emotion is 1, else 0. */
    u32 form = 0;
    if (call_battle_getActorEmotion_802A564_sub_8027796(battleObj).r0 == 1)
        form = 0xC;
    call_e_loadHudBattleFormByte(form);

    /* Rotate the chip-id list at battleObj+0x48 down one slot; its length
     * lives at [battleObj+8]. The byte falling off the end is kept aside and
     * written into the slot that opens up at the front. */
    u32 count = LD8(battleObj + 8) - 1;
    u32 slot = battleObj + 0x48 + count;
    u32 overflow = LD8(slot);

    while (count != 0) {
        slot--;
        u32 chipId = LD8(slot);
        ST8(slot + 1, chipId);

        /* Re-upload this chip's sprite tiles into its VRAM slot; the tile
         * number is the low 9 bits of the chip header halfword. */
        u32 chip = call_getLocOfActiveChips_8027E1C_sub_8027796(chipId).r0;
        u32 tileNum = (LD16(LD32(chip + 8)) << 23) >> 23;
        call_sprite_CopyObjTilesToVramSlot_80281D4(tileNum, count);
        count--;
    }

    /* The vacated first slot receives the byte pushed off the end. */
    ST8(battleObj + 0x48, overflow);

    /* Free slot 0's sprite, then mark chip slot 0xB as taken (state 2). */
    u32 sprite = call_sub_802A034().r0;
    call_sprite_CopyObjTilesToVramSlot_80281D4(sprite, 0);

    u32 slotB = call_getLocOfActiveChips_8027E1C_sub_8027796(0xB).r0;
    ST8(slotB + 7, 2);

    /* Run the custom-screen exit sequence and start the fade-out. */
    call_customScreen_ExitSequence_8028E32(battleObj);
    call_SetScreenFade_sub_8027796(0x60, 8);

    /* HUD state 0x10 = currently leaving the custom screen. */
    ST16(battleObj + 2, 0x10);

    struct ExitRegs kept = { r4in, r6in };
    return kept;
}

/* battle_customScreenExitRotateChips_8027796(r5 = battle object, r10 = ...) -> r2 r3 r4 r5 r6 r8 r9 r10 r11 r12 */
Next battle_customScreenExitRotateChips_8027796(void) {
    struct ExitRegs kept = custom_screen_exit(R5, R4, R6);
    R4 = kept.r4;
    R6 = kept.r6;
    RETURN_TO_CALLER();
}

/* ── sub_8027A84 ── */
/* Reset the battle-actor's [r5+0x40] counter to 0 and its [r5+2] halfword field to 4. */
static void reset_actor_fields(u32 actor) {
    ST32(actor + 0x40, 0);
    ST16(actor + 2, 4);
}

/* sub_8027A84(r5 = actor) -> nothing */
Next sub_8027A84(void) {
    reset_actor_fields(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8027CCC ── */
/* unk_2035000 is a grid of u16 entries (tile indices); one row of the grid
 * is 30 bytes, i.e. 15 entries. */
#define UNK_2035000_ROW_BYTES 0x1E

/* Stamp a width x height rectangle of running tile indices into the grid at
 * 0x02035000, starting with firstTile at (column, row).
 *
 * columnWise (r5 == 1): consecutive indices run down a column; stepping one
 *   column right advances by the rectangle's height, and the next row of the
 *   rectangle starts one index after the row above.
 * otherwise: consecutive indices run across a row, and each row continues
 *   where the row above ended.
 *
 * Returns the index that follows the filled rectangle (firstTile plus the
 * number of cells written), which the original leaves in r2. */
static u32 fill_grid_rect(u32 column, u32 row, u32 firstTile, u32 width, u32 height, u32 columnWise) {
    u32 step = (columnWise == 1) ? height : 1; /* index change per column */

    u32 rowAddr = A_unk_2035000 + row * UNK_2035000_ROW_BYTES + column * 2;
    u32 rowStartTile = firstTile; /* first index written on the current row */
    u32 tile = firstTile;         /* running index (original r2) */

    do {
        u32 addr = rowAddr;
        u32 remaining = width;

        tile = rowStartTile;
        do {
            ST16(addr, tile);
            addr += 2;
            tile += step;
        } while (--remaining != 0);

        rowAddr += UNK_2035000_ROW_BYTES; /* one grid row down */

        if (step == 1) {
            rowStartTile = tile; /* next row simply carries on from here */
        } else {
            rowStartTile += 1;   /* next row starts one index later */
            tile -= step - 1;    /* index that follows the rectangle so far */
        }
    } while (--height != 0);

    return tile;
}

/* sub_8027CCC(r0 = column, r1 = row, r2 = first tile index, r3 = width,
 *             r4 = height, r5 = numbering order) -> r2 = index after the block.
 * The original pushes r6/r7 and pops them back before returning, so it leaves
 * them (and r0/r1/r3-r5) as the caller had them; they are simply not touched. */
Next sub_8027CCC(void) {
    R2 = fill_grid_rect(R0, R1, R2, R3, R4, R5);
    RETURN_TO_CALLER();
}

/* ── chip_SelectNameTextTable_8027D10 ── */
/* chip_SelectNameTextTable_8027D10(r0 = chip id): pick the chip-name text table for a chip id.
 * The id is truncated to 9 bits; ids above 0xFF select the second name
 * table (TextScriptChipNames1) and wrap around to 8 bits.
 * Outputs: r0 = pointer to the name table, r1 = index into it. */
Next chip_SelectNameTextTable_8027D10(void) {
    u32 chipId = (R0 << 23) >> 23;      /* 9-bit chip id */

    u32 tablePtr = A_off_8027D2C;       /* -> TextScriptChipNames0 */
    if ((s32)chipId > 0xFF) {
        tablePtr += 4;                  /* -> TextScriptChipNames1 */
        chipId = (chipId << 24) >> 24;  /* wrap into the second table */
    }

    R0 = LD32(tablePtr);
    R1 = chipId;
    RETURN_TO_CALLER();
}

/* ── chip_getScript_8027D34 ── */
/* chip_getScript_8027D34(r0 = chip id):
 * Resolves the text script table holding a chip's description. Standard chip
 * ids (0x00-0x0FF) use the regular table; ids above 0x0FF (EX/Giga chips) use
 * the second table and are indexed modulo 0x100.
 * Outputs: r0 = pointer stored at the head of the selected table,
 *          r1 = the chip's index within that table. */
Next chip_getScript_8027D34(void) {
    /* Literal-pool entries pointing at the two description script tables. */
    u32 tablePtr = A_off_8027D50;

    u32 chip = R0 & 0x1FF;
    if (chip > 0xFF) {
        tablePtr += sizeof(u32); /* second table: EX/Giga chip descriptions */
        chip &= 0xFF;
    }

    R0 = LD32(tablePtr);
    R1 = chip;
    RETURN_TO_CALLER();
}

/* ── battle_countFolderChips_8027D58 ── */
/* battle_countFolderChips_8027D58: scan the equipped battle folder's chip ID list (up to 30
 * entries) for its 0xFFFF terminator.
 * Returns: r0 = number of chips stored before the terminator (30 if the
 *          folder is full and has none),
 *          r2 = address of the terminator entry (or one past the last
 *          entry when the scan ran off the end),
 *          r3 = number of entries left unscanned at that point.
 * The original uses r4 as scratch for the loaded ID but restores it on
 * return, so it is simply left untouched here. */
Next battle_countFolderChips_8027D58(void) {
    u32 cursor = A_eBattleFolder;
    u32 remaining = 0x1E;

    while (remaining != 0) {
        if (LD16(cursor) == 0xFFFF)
            break;
        cursor += 2;
        remaining -= 1;
    }

    R0 = 0x1E - remaining;
    R2 = cursor;
    R3 = remaining;
    RETURN_TO_CALLER();
}

/* ── battle_customGauge_EndTurn_8027D78 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battle_setNaviStatusGfx_801E71C has no typed C interface yet */
static inline void call_sub_801E71C_battle_customGauge_EndTurn_8027D78(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_setNaviStatusGfx_801E71C);
}

typedef struct { u32 r4, r5, r6, r7, r8, r9, r10, r11, r12; bool c, v; } call_dispatch_801DACC_result_battle_customGauge_EndTurn_8027D78;
/* register form: generated wrapper: dispatch_801DACC has no typed C interface yet */
static inline call_dispatch_801DACC_result_battle_customGauge_EndTurn_8027D78 call_dispatch_801DACC_battle_customGauge_EndTurn_8027D78(u32 r0) {
    R0 = r0;
    GUEST_CALL(dispatch_801DACC);
    return (call_dispatch_801DACC_result_battle_customGauge_EndTurn_8027D78){ R4, R5, R6, R7, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool v; } call_sub_801E0A0_result;
/* register form: generated wrapper: battle_eStruct2035280_postEvent_801E0A0 has no typed C interface yet */
static inline call_sub_801E0A0_result call_sub_801E0A0(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(battle_eStruct2035280_postEvent_801E0A0);
    return (call_sub_801E0A0_result){ R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_eStruct2035280_SetRequestBits_801BECC_result_battle_customGauge_EndTurn_8027D78;
/* register form: generated wrapper: eStruct2035280_SetRequestBits_801BECC has no typed C interface yet */
static inline call_eStruct2035280_SetRequestBits_801BECC_result_battle_customGauge_EndTurn_8027D78 call_eStruct2035280_SetRequestBits_801BECC_battle_customGauge_EndTurn_8027D78(u32 r0) {
    R0 = r0;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);
    return (call_eStruct2035280_SetRequestBits_801BECC_result_battle_customGauge_EndTurn_8027D78){ R1, R2 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool c, v; } call_dispatch_801DA48_result_battle_customGauge_EndTurn_8027D78;
/* register form: generated wrapper: dispatch_801DA48 has no typed C interface yet */
static inline call_dispatch_801DA48_result_battle_customGauge_EndTurn_8027D78 call_dispatch_801DA48_battle_customGauge_EndTurn_8027D78(u32 r0) {
    R0 = r0;
    GUEST_CALL(dispatch_801DA48);
    return (call_dispatch_801DA48_result_battle_customGauge_EndTurn_8027D78){ R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r3; } call_battle_IsBattleEnding_800A97A_result;
/* register form: generated wrapper: battle_IsBattleEnding_800A97A has no typed C interface yet */
static inline call_battle_IsBattleEnding_800A97A_result call_battle_IsBattleEnding_800A97A(void) {
    GUEST_CALL(battle_IsBattleEnding_800A97A);
    return (call_battle_IsBattleEnding_800A97A_result){ R0, R3 };
}

typedef struct { u32 r0, r1, r4; } call_battle_testFlag0x40_800A8F8_result;
/* register form: generated wrapper: battle_testFlag0x40_800A8F8 has no typed C interface yet */
static inline call_battle_testFlag0x40_800A8F8_result call_battle_testFlag0x40_800A8F8(void) {
    GUEST_CALL(battle_testFlag0x40_800A8F8);
    return (call_battle_testFlag0x40_800A8F8_result){ R0, R1, R4 };
}

typedef struct { u32 r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_sub_801DF0C_result;
/* register form: generated wrapper: battle_customGaugeResetForNewTurn has no typed C interface yet */
static inline call_sub_801DF0C_result call_sub_801DF0C(void) {
    GUEST_CALL(battle_customGaugeResetForNewTurn);
    return (call_sub_801DF0C_result){ R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* register form: generated wrapper: GetBattleMode has no typed C interface yet */
static inline u32 call_GetBattleMode_battle_customGauge_EndTurn_8027D78(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

typedef struct { u32 r4, r5, r6, r8, r9, r10, r11, r12; } call_sub_801DEEE_result;
/* register form: generated wrapper: battle_customGaugeResetGfxAndRequest has no typed C interface yet */
static inline call_sub_801DEEE_result call_sub_801DEEE(void) {
    GUEST_CALL(battle_customGaugeResetGfxAndRequest);
    return (call_sub_801DEEE_result){ R4, R5, R6, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r4, r5, r6, r8, r9, r10, r11, r12; } call_custGauge_Init_801DED0_result;
/* register form: generated wrapper: custGauge_Init_801DED0 has no typed C interface yet */
static inline call_custGauge_Init_801DED0_result call_custGauge_Init_801DED0(void) {
    GUEST_CALL(custGauge_Init_801DED0);
    return (call_custGauge_Init_801DED0_result){ R4, R5, R6, R8, R9, R10, R11, R12 };
}

/* register form: generated wrapper: ClearCustGauge has no typed C interface yet */
static inline void call_ClearCustGauge(void) {
    GUEST_CALL(ClearCustGauge);
}

/* Request-bit constants used with dispatch_801DA48 / SetRequestBits pairs. */
#define REQ_ENDING      0x400u
#define REQ_CUSTOM      0x20000u
#define REQ_GAUGE_MODE1 0x20u
#define REQ_GAUGE_OTHER 0x10u

/* End-of-turn Custom Gauge handler.  Clears the pending-trigger field,
 * resets the gauge counter, then picks an ending / re-initialisation path
 * depending on battle state and flag 0x40.  `toolkit` is the r10 value
 * (Toolkit pointer) that callers pass in. */
static void custom_gauge_end_turn(u32 toolkit) {
    /* Zero the pending-trigger halfword in the render-info structure. */
    u32 render = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render + 0x18, 0);

    call_sub_801E71C_battle_customGauge_EndTurn_8027D78(0);            /* gauge display reset */
    call_dispatch_801DACC_battle_customGauge_EndTurn_8027D78(0x80);    /* mark end-of-turn request */
    call_sub_801E0A0(0, 0);         /* clear gauge counter */

    /* Request battle-ending flag 0x400 (SetRequestBits first, then dispatch). */
    call_eStruct2035280_SetRequestBits_801BECC_battle_customGauge_EndTurn_8027D78(REQ_ENDING);
    call_dispatch_801DA48_battle_customGauge_EndTurn_8027D78(REQ_ENDING);

    /* Abort if the battle is already ending. */
    if (!call_battle_IsBattleEnding_800A97A().r0) {
        u32 bstate = LD32(toolkit + oToolkit_BattleStatePtr);
        u32 state7 = LD8(bstate + 7);

        if (state7 == 1) {
            /* Normal ending path (battle-state byte 7 == 1). */
            if (call_battle_testFlag0x40_800A8F8().r0) {
                call_sub_801DF0C();               /* flag-set end-of-turn */
            } else if (call_GetBattleMode_battle_customGauge_EndTurn_8027D78() == 1) {
                call_sub_801DEEE();               /* mode-1 cleanup */
            } else {
                call_custGauge_Init_801DED0();    /* re-initialise gauge */
            }
        } else {
            /* Alternate path (battle-state byte 7 != 1). */
            if (call_battle_testFlag0x40_800A8F8().r0) {
                call_dispatch_801DA48_battle_customGauge_EndTurn_8027D78(REQ_CUSTOM);
                call_eStruct2035280_SetRequestBits_801BECC_battle_customGauge_EndTurn_8027D78(REQ_CUSTOM);
            } else {
                call_ClearCustGauge();
                if (call_GetBattleMode_battle_customGauge_EndTurn_8027D78() == 1) {
                    call_dispatch_801DA48_battle_customGauge_EndTurn_8027D78(REQ_GAUGE_MODE1);
                    call_eStruct2035280_SetRequestBits_801BECC_battle_customGauge_EndTurn_8027D78(REQ_GAUGE_MODE1);
                } else {
                    call_dispatch_801DA48_battle_customGauge_EndTurn_8027D78(REQ_GAUGE_OTHER);
                    call_eStruct2035280_SetRequestBits_801BECC_battle_customGauge_EndTurn_8027D78(REQ_GAUGE_OTHER);
                }
            }
        }
    }
}

Next battle_customGauge_EndTurn_8027D78(void) {
    custom_gauge_end_turn(R10);
    RETURN_TO_CALLER();
}

/* ── resetBattleRecordSlots_8027E90 ── */
/* Battle mode flag used to select the panel-status branch. */
enum {
    BATTLE_MODE_SINGLE = 1,
};

/* Offsets into a battle-record slot (12 slots, 0xC bytes each). */
#define SLOT_SIZE         0xC
#define SLOT_COUNT_sub_8027E90        12
#define PANEL_MODE_OFFSET 0x84
#define PANEL_FLAG_OFFSET 0x8B
#define PANEL_MODE_VALUE_SINGLE 3
#define PANEL_MODE_VALUE_OTHER  2

/* ROM address of the per-slot template table (each entry: 4-byte panel-id pointer). */
#define SLOT_TEMPLATE_TABLE_PTR A_dword_802A7CC

/* RAM address of the battle-mode configuration struct. */
#define BATTLE_CONFIG_PTR A_dword_20349A0

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 get_battle_mode_sub_8027E90(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls checkBattleEventGate_8029FB4 outside this file, which has no typed C interface yet */
static bool sub_8029FB4_retbool(void) {
    GUEST_CALL(checkBattleEventGate_8029FB4);
    return R0 != 0;
}

/* register form: calls battle_checkEmotionAllowsAction_802A57E outside this file, which has no typed C interface yet */
static bool sub_802A57E_retbool(void) {
    GUEST_CALL(battle_checkEmotionAllowsAction_802A57E);
    return R0 != 0;
}

/* resetBattleRecordSlots_8027E90: initialise the 12 battle-record slots from the ROM template, then set the
 * panel-status byte on the slot array header according to the current battle mode.
 * slot_array_ptr (r4) points to the slot array. Returns the address one past the last slot
 * (r4 + 12 * 0xC), matching the original's final r2. */
static u32 reset_battle_record(u32 slot_array_ptr) {
    u32 template_addr = SLOT_TEMPLATE_TABLE_PTR;
    u32 slot_addr = slot_array_ptr;
    for (u32 i = 0; i < SLOT_COUNT_sub_8027E90; i++) {
        ST32(slot_addr,     LD32(template_addr));
        ST32(slot_addr + 4, 0);
        ST32(slot_addr + 8, 0);
        template_addr += 4;
        slot_addr += SLOT_SIZE;
    }

    u32 mode = get_battle_mode_sub_8027E90();
    if (mode == BATTLE_MODE_SINGLE) {
        if (LD8(BATTLE_CONFIG_PTR + 4) == 0) {
            ST8(slot_array_ptr + PANEL_MODE_OFFSET, PANEL_MODE_VALUE_SINGLE);
        }
    } else if (sub_8029FB4_retbool()) {
        u32 flag = 1;
        if (sub_802A57E_retbool()) {
            flag = 0;
        }
        ST8(slot_array_ptr + PANEL_MODE_OFFSET, PANEL_MODE_VALUE_OTHER);
        ST8(slot_array_ptr + PANEL_FLAG_OFFSET, (u8)flag);
    }

    return slot_addr;
}

Next resetBattleRecordSlots_8027E90(void) {
    R2 = reset_battle_record(R4);
    RETURN_TO_CALLER();
}

/* ── battle_BuildFolderEntryRecords_8027EE8 ── */
/* battle_BuildFolderEntryRecords_8027EE8(r4 = destination records, r5 = source header struct, r6 = slot limit):
 * Fill battle folder entries into 12-byte records. The record count is the
 * byte at [r5,#5], clamped to the limit in r6; each record gets a zero byte
 * at +0 and a pointer to its entry inside eBattleFolder (2 bytes per entry)
 * at +8. r4 and r6 are preserved by the original (push/pop), so the record
 * walk happens in a local pointer. r2 and r3 both end at 0 after filling;
 * when nothing is written, r3 holds the count byte that was read. */
Next battle_BuildFolderEntryRecords_8027EE8(void) {
    u32 count = LD8(R5 + 5);

    if (count == 0 || R6 == 0) {
        R3 = count;             /* r3 keeps the ldrb result on this path */
        RETURN_TO_CALLER();
    }

    u32 slots = ((s32)count < (s32)R6) ? count : R6;
    u32 dst = R4;               /* local walk pointer; r4 itself is restored */
    u32 entry = A_eBattleFolder;/* &eBattleFolder[0], 2 bytes per entry */

    /* The byte at +0 is always 0, not the record index. */
    for (u32 i = slots; i != 0; i--) {
        ST8(dst, 0);
        ST32(dst + 8, entry);
        dst += 0xC;
        entry += 2;
    }

    R2 = 0;
    R3 = 0;
    RETURN_TO_CALLER();
}

/* ── battle_clearActiveChipIfMegaGiga_8027F6A ── */
/* battle_clearActiveChipIfMegaGiga_8027F6A: r7 points at a chip-folder state block whose byte at offset 1
 * holds the currently selected active-chip index. Look up that index in the
 * active-chips table (getLocOfActiveChips_8027E1C) and read the chip's class
 * byte from the entry. If the selected chip is a Mega (0xA) or Giga (0xB)
 * chip, reset the selection index to 0xFF (no selection). */
Next battle_clearActiveChipIfMegaGiga_8027F6A(void) {
    R0 = LD8(R7 + 1);
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 chipClass = LD8(R0);

    if (chipClass == 0xA || chipClass == 0xB)
        ST8(R7 + 1, 0xFF);

    RETURN_TO_CALLER();
}

/* ── table_findFirstOccupiedEntry_8028050 ── */
/* table_findFirstOccupiedEntry_8028050(r4 = table cursor, r5 = record):
 * Scans a table of 12-byte entries starting at r4. A leading byte of 0x0A or
 * 0x0B marks an empty entry, so skip those; any other byte marks an occupied
 * entry, which stops the scan. Stores the index of the first occupied entry
 * (or 0x0A if all 12 entries were empty) to r5[7]. r4 and r6 are callee-saved
 * and end unchanged; the cursor/count live in locals (the original pushes and
 * pops r4/r6 around the scan). */
Next table_findFirstOccupiedEntry_8028050(void) {
    u32 entry = R4;
    u32 slot = 0;

    while (LD8(entry) == 0xA || LD8(entry) == 0xB) {
        entry += 0xC;
        if (++slot == 0xC) {
            /* No occupied entry in the whole table: report 0x0A. */
            slot = 0xA;
            break;
        }
    }

    ST8(R5 + 7, slot);
    RETURN_TO_CALLER();
}

/* ── folder_findFirstChipWithFlag_802806C ── */
/* Folder-slot scan helpers used by folder_findFirstChipWithFlag_802806C. */
#define SLOT_COUNT   10   /* number of chip slots walked */
#define SLOT_STRIDE  0xC  /* bytes per slot entry */

/* Slot entry layout:
 *   +0: u8  slot state flag (0 = examine this slot)
 *   +8: u32 pointer to a halfword identifying the slot's chip */

/* folder_findFirstChipWithFlag_802806C(r4 = chip slot list, r5 = owner struct):
 * Walk the ten slots in order.  For each slot flagged 0 at +0, map the
 * halfword at [slot+8] to a chip id (battle_getChipWordIfUsable, masked to 9 bits),
 * fetch that chip's data, and test the 0x20 flag in its status byte
 * (+9).  The first matching slot's index is stored at [r5+7] and the
 * scan stops.  [r5+7] is left alone when it is already non-zero or no
 * slot matches.
 * r2/r3/r5 pass through unchanged, and the caller expects its r4/r6
 * back intact. */
Next folder_findFirstChipWithFlag_802806C(void) {
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    if (LD8(R5 + 7) != 0) /* slot choice already recorded */
        RETURN_TO_CALLER();

    u32 slot = R4;
    for (u32 i = 0; i < SLOT_COUNT; i++) {
        R0 = LD8(slot);
        if (R0 == 0) {
            R0 = LD16(LD32(slot + 8));
            R4 = slot; /* keep the guest view of the loop state in sync */
            R6 = i;
            GUEST_CALL(battle_getChipWordIfUsable); /* chip word -> chip id */
            R0 &= 0x1FF;             /* keep the 9-bit chip id */
            GUEST_CALL(getChip8021DA8);
            R0 = LD8(R0 + 9); /* chip status byte */
            R1 = 0x20;
            if (R0 & 0x20) {
                ST8(R5 + 7, i);
                break;
            }
        }
        slot += SLOT_STRIDE;
    }

    R4 = savedR4;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── selectChipTableEntryIntoActiveSlot9_80280A2 ── */
/* selectChipTableEntryIntoActiveSlot9_80280A2: register one of the table entries (byte_802A828, halfwords indexed
 * 1..11) into the object at R5.
 *
 * R5+0x10 holds a selection index. Index 0 and anything above 11 are ignored, as
 * is any entry whose bit is already set in the "taken" bitmask word_20349AC
 * (dword_20349A0 + 0xC). Otherwise the entry's halfword is copied to R5+0x22 and
 * active-chip slot 9 is cleared and pointed at that field.
 *
 * On return R2 = (1 << index) whenever the index was in range (it is computed
 * before the bitmask test, so it is also set when the entry turns out to be
 * taken); R4 is preserved. */
Next selectChipTableEntryIntoActiveSlot9_80280A2(void) {
    u32 index = LD8(R5 + 0x10);

    if (index != 0 && index <= 0xB) {
        u32 takenMask = LD16(A_dword_20349A0 + 0xC);
        u32 entryBit = 1u << index;

        R2 = entryBit;

        if (!(takenMask & entryBit)) {
            /* The table pointer lives in r4 between the lookup and the call;
             * r4 itself is saved/restored by the original, so it is unchanged
             * on return. */
            u32 savedR4 = R4;
            u32 table = A_byte_802A828;

            R4 = table + (index - 1) * 2;
            ST16(R5 + 0x22, LD16(R4));

            R0 = 9;
            R1 = table; /* the original still holds the table base in r1 here */
            GUEST_CALL(getLocOfActiveChips_8027E1C);
            u32 slot = R0;

            ST8(slot, 0);
            ST8(slot + 4, 0);
            ST32(slot + 8, R5 + 0x22);

            R4 = savedR4;
        }
    }

    RETURN_TO_CALLER();
}

/* ── battle_spawnMirrorFacingHelper_80280E0 ── */
/* battle_spawnMirrorFacingHelper_80280E0: set up a helper object entry (at R4+0x60) once R5's object
 * signals that it is ready.  Reads R5's state byte (+0x10) and facing byte
 * (+0x16); writes kind/hit-table/behaviour bytes into the new entry.
 * R2 ends up holding R5's facing byte when the setup path runs. */
Next battle_spawnMirrorFacingHelper_80280E0(void) {
    /* Nothing to do if the object is already active/finished. */
    if (LD8(R5 + 0x10) != 0)
        RETURN_TO_CALLER();

    /* Ask battle_checkActiveSlot8NaviStatReady_802A5DA whether the trigger condition holds; bail out if not. */
    GUEST_CALL(battle_checkActiveSlot8NaviStatReady_802A5DA);
    if (R0 == 0)
        RETURN_TO_CALLER();

    u32 entry = R4 + 0x60;

    ST8(entry,       4);              /* object kind */
    ST8(entry + 0xC, 5);              /* ... */
    ST8(entry + 3,   0xB);            /* behaviour/table id */
    ST8(entry + 0xE, 7);              /* ... */

    /* Mirror the source object's facing (+0x16): entry+4 = 1 - facing. */
    R2 = LD8(R5 + 0x16);
    u32 dir = (1u - R2) & 0xFF;
    ST8(entry + 4, dir);

    /* When facing is 1 (dir == 0), also flag the entry at +7. */
    if (dir == 0)
        ST8(entry + 7, 1);

    RETURN_TO_CALLER();
}

/* ── gfx_flushDataListAndContinue_802811C ── */
/* gfx_flushDataListAndContinue_802811C: flush the dataList's pending GFX transfers, then continue with
 * gfx_queueIndexedRecordTiles_802812C seeded by the byte at [R5+0x10]. R4-R7 are passed through. */
Next gfx_flushDataListAndContinue_802811C(void) {
    /* dataList (table of GFX data references) */
    R0 = A_off_802A744;
    GUEST_CALL(QueueGFXTransfersInList);

    R0 = LD8(R5 + 0x10);
    GUEST_CALL(gfx_queueIndexedRecordTiles_802812C);

    RETURN_TO_CALLER();
}

/* ── gfx_queueIndexedRecordTiles_802812C ── */
/* gfx_queueIndexedRecordTiles_802812C(r0 = record index):
 * Queue a record's graphics for transfer: the two 0x40-byte halves of its
 * 0x80-byte block go to VRAM 0x060170E0 and 0x06017160, and a 0x20-byte
 * chunk reached through the record goes to byte_30016B0. */
Next gfx_queueIndexedRecordTiles_802812C(void) {
    u32 savedR4 = R4;
    u32 record = R0;

    /* The record handle stays in r4 across all the calls below. */
    R4 = record;

    /* Pointer to the record's 0x80-byte graphics block. */
    R0 = record;
    GUEST_CALL(getIndexed80hRecordPtr_8028164);

    /* First half to 0x060170E0, then the second half (which directly
     * follows the first in the block) to 0x06017160. */
    R1 = 0x60170E0;
    R2 = 0x40;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    R0 += R2;                       /* r2 still holds the 0x40 chunk size */

    R1 = 0x6017160;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Fetch the extra chunk associated with the record... */
    R0 = record;
    GUEST_CALL(getPermutedRecord32_8028172);

    /* ...and queue it (0x20 bytes) to byte_30016B0. */
    R1 = A_byte_30016B0;
    R2 = 0x20;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── getIndexed80hRecordPtr_8028164 ── */
/* getIndexed80hRecordPtr_8028164(r0 = index):
 * Look up index in byte_802819C, then compute the address of that entry's
 * 0x80-byte record in the dat38_86 table (table base + entry * 0x80).
 * Returns the address in r0. */
Next getIndexed80hRecordPtr_8028164(void) {
    u8 entry = LD8(A_byte_802819C + R0);
    R0 = A_byte_86F5834 + entry * 0x80;
    RETURN_TO_CALLER();
}

/* ── getPermutedRecord32_8028172 ── */
/* getPermutedRecord32_8028172(r0 = index):
 * Selects one of the 32-byte records laid out at byte_86E56FC. The index is
 * run through the permutation table at off_802818C (entries {3,0,4,1,5,2,6,0},
 * which remaps slot order), then scaled by the 0x20-byte record size and added
 * to the record base. Returns the record address in r0. */
Next getPermutedRecord32_8028172(void) {
    u32 index = R0;

    u32 entry = LD8(A_off_802818C + index);
    R0 = A_byte_86E56FC + entry * 0x20;

    RETURN_TO_CALLER();
}

/* ── battle_getChipElementData_80281A8 ── */
/* battle_getChipElementData_80281A8(r0 = chip id):
 * Returns the element/attribute data associated with a battle chip.
 * The id is masked to its 9-bit chip number; the sentinel id 0x1FF
 * ("no chip") maps to the default data blob byte_86E601C instead of
 * doing a chip-data lookup. For a real chip, the pointer stored at
 * offset 0x20 in its ChipData record is returned.
 * Returns: r0 = data pointer. r1/r2 are preserved. */
Next battle_getChipElementData_80281A8(void) {
    u32 chipId = R0 & 0x1FF;

    if (chipId == 0x1FF) {
        /* Invalid/placeholder chip: hand back the fallback data. */
        R0 = A_byte_86E601C;
    } else {
        /* Look up the chip record and fetch its data pointer (r1/r2 are
         * callee-saved here, so keep them across the call like the
         * original's push/pop does). */
        u32 savedR1 = R1, savedR2 = R2;
        R0 = chipId;
        GUEST_CALL(getChip8021DA8);
        R1 = savedR1;
        R2 = savedR2;
        R0 = LD32(R0 + 0x20);
    }

    RETURN_TO_CALLER();
}

/* ── gfx_QueueNextChunkedGfxTransfer_80281C4 ── */
/* gfx_QueueNextChunkedGfxTransfer_80281C4(r0 = source, r1 = destination, r3 = context for battle_getChipElementData_80281A8):
 * Let battle_getChipElementData_80281A8 set up the next graphics chunk (it adjusts r0 = source and
 * r1 = destination), queue it as an 0x80-sized eight-word-aligned GFX
 * transfer, then advance the destination pointer past the chunk just queued.
 * Returns r1 = destination + 0x80; r2 and r3 are preserved (the original
 * saves them around the calls and restores them on the way out). */
Next gfx_QueueNextChunkedGfxTransfer_80281C4(void) {
    u32 saved_r2 = R2;
    u32 saved_r3 = R3;

    /* Set up source/destination for this chunk. */
    GUEST_CALL(battle_getChipElementData_80281A8);

    /* Queue the chunk transfer: QueueEightWordAlignedGFXTransfer(src, dest, 0x80). */
    R2 = 0x80;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Destination pointer moves past the chunk we just queued. */
    R1 += 0x80;

    /* Restore the registers the original pops back before returning. */
    R2 = saved_r2;
    R3 = saved_r3;
    RETURN_TO_CALLER();
}

/* ── sprite_CopyObjTilesToVramSlot_80281D4 ── */
/* sprite_CopyObjTilesToVramSlot_80281D4(r1 = slot index, r0/r2..r7 = passthrough to gfx_QueueNextChunkedGfxTransfer_80281C4):
 * Points at one 0x80-byte chunk of OBJ VRAM tile memory, 0x0600A4A0 + 0x80 * r1,
 * and lets gfx_QueueNextChunkedGfxTransfer_80281C4 perform the (de)compression/copy for that chunk.
 * r1 and r2 are saved across the call and restored for the caller. */
Next sprite_CopyObjTilesToVramSlot_80281D4(void) {
    u32 saved_r1 = R1;
    u32 saved_r2 = R2;

    u32 vram_offset = 0x80 * saved_r1;
    u32 vram_dest = 0x0600A4A0 + vram_offset;

    R1 = vram_dest;
    R2 = vram_offset;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R1 = saved_r1;
    R2 = saved_r2;
    RETURN_TO_CALLER();
}

/* ── gfx_QueueNext64ByteChunk_8028204 ── */
/* gfx_QueueNext64ByteChunk_8028204: queue one 0x40-byte graphics DMA chunk.
 *
 * Entry: r1 = destination address for this chunk.  The helper data_getRecordPtrById_8028214
 * fills in r0 with the matching source address, the transfer is queued with
 * size 0x40, and r1 is advanced past the chunk so the caller can immediately
 * queue the next one.  r2/r3 are scratch for the queue call and are restored
 * before returning.  Flags come from the `adds r1, #0x40` pointer bump. */
Next gfx_QueueNext64ByteChunk_8028204(void) {
    u32 saved_r2 = R2;
    u32 saved_r3 = R3;

    /* Resolve the source address (leaves r1-r3 untouched). */
    GUEST_CALL(data_getRecordPtrById_8028214);

    /* QueueEightWordAlignedGFXTransfer(src = r0, dest = r1, size = 0x40). */
    R2 = 0x40;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Restore the caller's r2/r3 and advance the destination pointer. */
    R2 = saved_r2;
    R3 = saved_r3;
    u32 prev_r1 = R1;
    R1 = prev_r1 + 0x40;
    SET_FLAGS_ADD(prev_r1, 0x40);

    RETURN_TO_CALLER();
}

/* ── data_getRecordPtrById_8028214 ── */
/* data_getRecordPtrById_8028214(r0 = record id):
 * Returns a pointer to a 64-byte record for the given id.
 * Ids 0x1B and 0x1C are special-cased and return byte_802A700 instead.
 * Any other id above the table is clamped to the last entry (0x1B).
 * Result in r0; r2 is preserved. */
Next data_getRecordPtrById_8028214(void) {
    u32 id = R0;

    if (id == 0x1B || id == 0x1C) {
        R0 = A_byte_802A700;
    } else {
        if ((s32)id > 0x1C)
            id = 0x1B;
        R0 = A_dword_86E591C + (id << 6);
    }
    RETURN_TO_CALLER();
}

/* ── sprite_clearObjVramEntry_8028238 ── */
/* sprite_clearObjVramEntry_8028238(r0 = entry index):
 * Clears one 0xC0-byte entry of OBJ tile VRAM: the block at
 * 0x6009C20 + index*0xC0 is filled/reset by gfx_QueueNextChunkedGfxTransfer_80281C4 (0x1FF = fill pattern).
 * R2-R7 pass through untouched, so the callee's register contract is preserved. */
Next sprite_clearObjVramEntry_8028238(void) {
    u32 entryOffset = R0 * 0xC0;   /* 0xC0 bytes per entry */

    R1 = 0x6009C20 + entryOffset;  /* VRAM destination for this entry */
    R0 = 0x1FF;                    /* fill value for gfx_QueueNextChunkedGfxTransfer_80281C4 */

    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);
    RETURN_TO_CALLER();
}

/* ── chipSelect_QueueEntryIconGfx_8028284 ── */
/* chipSelect_QueueEntryIconGfx_8028284: queue the chip-icon graphics for one entry of the chip
 * selection screen.
 *   r4 -> entry struct: byte at +7 selects the source (2 = use the fixed
 *   id 0x1FF), word at +8 points to the chip's id/code halfword.
 * Calls getChipID_802A54E to map the chip halfword to an icon id, then
 * streams the icon tiles: first the chunked transfer header, then the
 * 64-byte tile chunk for id / 512. Flags on return are whatever the last
 * callee leaves, as in the original. */
Next chipSelect_QueueEntryIconGfx_8028284(void) {
    u32 entry = R4;

    u32 chipId;
    if (LD8(entry + 7) == 2) {
        /* Fixed id: this entry does not reference a real chip. */
        chipId = 0x1FF;
    } else {
        R0 = LD16(LD32(entry + 8));
        GUEST_CALL(getChipID_802A54E);
        chipId = R0;
    }

    R0 = chipId;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R0 = LD16(LD32(entry + 8));
    GUEST_CALL(getChipID_802A54E);
    R0 = R0 >> 9;
    GUEST_CALL(gfx_QueueNext64ByteChunk_8028204);

    RETURN_TO_CALLER();
}

/* ── nullsub_14 ── */
/* nullsub_14: 080282AC THUMB — empty stub; does nothing. */
Next nullsub_14(void) {
    RETURN_TO_CALLER();
}

/* ── battle_queueGfxVariant ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3, r4, r5, r6, r7; bool z, n, c, v; } call_QueueEightWordAlignedGFXTransfer_result_battle_queueGfxVariant;
/* register form: generated wrapper: QueueEightWordAlignedGFXTransfer has no typed C interface yet */
static inline call_QueueEightWordAlignedGFXTransfer_result_battle_queueGfxVariant call_QueueEightWordAlignedGFXTransfer_battle_queueGfxVariant(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    return (call_QueueEightWordAlignedGFXTransfer_result_battle_queueGfxVariant){ R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_queueGfxVariant (0x080282AE)
 * Queues a 0x100-byte graphics transfer for a battle object. The source is
 * byte_86E79CC, or byte_86E79CC+0x100 when the object's byte at +7 is nonzero
 * (the object uses the second variant of the graphics). The destination
 * pointer arrives in r1; r4 points to the battle object.
 * Everything except the source selection is delegated to
 * QueueEightWordAlignedGFXTransfer, whose r3/r4/r7 results and flags are the
 * outputs of this function as well. */
static call_QueueEightWordAlignedGFXTransfer_result_battle_queueGfxVariant queue_battle_gfx(u32 dest, u32 object) {
    u32 src = A_byte_86E79CC;

    if (LD8(object + 7) != 0)
        src += 0x100;

    return call_QueueEightWordAlignedGFXTransfer_battle_queueGfxVariant(src, dest, 0x100);
}

/* battle_queueGfxVariant(r1 = dest, r4 = battle object, lr) -> r3 r4 r7 (+ nz c v) */
Next battle_queueGfxVariant(void) {
    call_QueueEightWordAlignedGFXTransfer_result_battle_queueGfxVariant queued = queue_battle_gfx(R1, R4);

    R3 = queued.r3;
    R4 = queued.r4;
    R7 = queued.r7;
    /* nz/c/v are whatever QueueEightWordAlignedGFXTransfer left behind; the
     * wrapper call above already produced them, so nothing to set here. */
    RETURN_TO_CALLER();
}

/* ── battle_endActionSetup_8028310 ── */
/* battle_endActionSetup_8028310: apply the two standard "end of battle action" setup calls.
 * First resets something with the value 0x1FF, then triggers action 0x1D.
 * All argument registers (r1-r7) are passed through untouched for the callees. */
Next battle_endActionSetup_8028310(void) {
    R0 = 0x1FF;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R0 = 0x1D;
    GUEST_CALL(gfx_QueueNext64ByteChunk_8028204);

    RETURN_TO_CALLER();
}

/* ── battle_loadGfxChunkBySelector_8028320 ── */
/* battle_loadGfxChunkBySelector_8028320: load battle graphics, selected by R7.
 *
 * R7 == 1: queue an eight-word-aligned GFX transfer of a 0x100-byte block
 *          (the graphics chunk at byte_86E79CC + 0x300) into the destination
 *          buffer passed in R1.
 * R7 != 1: run battle_loadGfxChunkBySelectorWithRows_802868C on the data table at byte_802A700 with selector
 *          R3 = 3, then advance the destination pointer in R1 by 0xC0
 *          (the size of one loaded chunk), setting the flags from that add.
 *
 * R0-R3 and the nz/c/v flags are returned exactly as the original leaves
 * them: in the queued-transfer path they come from the queue routine, in
 * the other path from the ADD that bumps R1. R4-R7 are untouched. */
Next battle_loadGfxChunkBySelector_8028320(void) {
    if (R7 == 1) {
        R0 = A_byte_86E79CC + 0x300;  /* source chunk in ROM */
        R2 = 0x100;                   /* chunk size */
        GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    } else {
        R0 = A_byte_802A700;
        R3 = 3;
        GUEST_CALL(battle_loadGfxChunkBySelectorWithRows_802868C);
        u32 bumped = R1 + 0xC0;
        SET_FLAGS_ADD(R1, 0xC0);      /* flags of ADDS r1, r1, #0xC0 */
        R1 = bumped;
    }
    RETURN_TO_CALLER();
}

/* ── bg_ReloadTileBlock3Screen_80283B0 ── */
/* bg_ReloadTileBlock3Screen_80283B0: reinitialize whatever sub_80283C8 tracks, then load a full
 * background screen into 32x32 tile block 3 from the tile map at
 * unk_2035186 (15 tiles wide, 6 tiles tall).
 *
 * Register contract: takes r4/r5/r10 from the caller and restores r4/r5
 * on return; r0-r3 and the flags on exit are whatever CopyBackgroundTiles
 * leaves behind. */
Next bg_ReloadTileBlock3Screen_80283B0(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    /* Run the companion setup routine first. */
    GUEST_CALL(sub_80283C8);

    /* CopyBackgroundTiles(j, i, which_tile_block_32x32, tile_ids, j_size, i_size) */
    R0 = 0;               /* j = 0 */
    R1 = 0xD;             /* i = 13 */
    R2 = 3;               /* tileBlock32x32 = 3 */
    R3 = A_unk_2035186;   /* tileIds */
    R4 = 0xF;             /* j_size = 15 */
    R5 = 6;               /* i_size = 6 */
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore the caller's r4/r5 (the original saves them across both calls). */
    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── sub_80283C8 ── */
/* sub_80283C8: set the sprite palettes of the ten active-chip (custom screen)
 * icons from each slot's kind byte.
 *
 * Kinds 1-3 keep whatever palette their icon already has; kind 0xA uses the
 * standard palette; kinds 0/6/7 pick between the standard and an alternate
 * palette via the flag byte at offset 7; any other kind uses a third palette.
 * The chosen palette goes into the top six bits of the four attribute
 * halfwords of the slot's icon sprites; the low ten bits are kept.
 *
 * The original leaves loop leftovers in r0/r1/r2 and callers may read them, so
 * they are reproduced exactly: r0 ends as the last value it held -- the last
 * stored attribute halfword, or the last slot's kind byte if that slot was
 * skipped -- r1 as 0x3FF once any icon was recolored (otherwise the leftover
 * r1 of getLocOfActiveChips), r2 as the last palette used, or 0 if the last
 * slot was skipped. */

#define ACTIVE_CHIP_SLOT_COUNT 10
#define ACTIVE_CHIP_STRIDE     0xC
#define ACTIVE_CHIP_KIND       0    /* slot's kind byte: selects the palette */
#define ACTIVE_CHIP_FLAG_BYTE  7    /* kinds 0/6/7 only: 1 picks the alternate palette */

#define ATTR_PALETTE_MASK      0x3FF /* low 10 bits of each attribute halfword are kept */

/* the four attribute halfwords per icon: two sprites, two halfwords each */
static const u8 iconAttrOffsets[4] = { 0x00, 0x02, 0x1E, 0x20 };

/* indices into the palette halfword table at byte_8028470: 0xB000, 0xC000, 0x9000 */
enum { PALETTE_STANDARD = 0, PALETTE_ALTERNATE = 1, PALETTE_OTHER = 2 };

/* register form: calls getLocOfActiveChips_8027E1C outside this file, which has no typed C interface yet */
static u32 active_chip_slots(u32 *leftoverR1) {
    R0 = 0;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    *leftoverR1 = R1;           /* becomes our r1 output if no icon is recolored */
    return R0;
}

/* Register values sub_80283C8 leaves behind for its caller. */
typedef struct {
    u32 lastAttrStored; /* r0: last stored attribute halfword, or the last
                           slot's kind byte if that slot was skipped */
    u32 attrMask;       /* r1: 0x3FF after the first recolor, otherwise the
                           leftover r1 of getLocOfActiveChips */
    u32 lastPalette;    /* r2: palette applied to the last slot, 0 if skipped */
} ChipIconPaletteResults;

static ChipIconPaletteResults recolor_active_chip_icons(void) {
    u32 leftoverR1;
    u32 chip = active_chip_slots(&leftoverR1);

    ChipIconPaletteResults out;
    out.lastAttrStored = chip;  /* r0 holds the chip-list pointer until the first kind read */
    out.attrMask = leftoverR1;  /* r1 keeps the callee's leftover until a recolor runs */
    out.lastPalette = 0;

    for (u32 slot = 0; slot < ACTIVE_CHIP_SLOT_COUNT; slot++, chip += ACTIVE_CHIP_STRIDE) {
        out.lastPalette = 0;        /* the original re-clears r2 at the top of every pass */

        u8 kind = LD8(chip + ACTIVE_CHIP_KIND);
        out.lastAttrStored = kind;  /* r0 now holds this slot's kind byte */

        if (kind == 1 || kind == 2 || kind == 3)
            continue;               /* these kinds keep the palette they already have */

        int paletteIndex;
        if (kind == 0xA) {
            paletteIndex = PALETTE_STANDARD;
        } else if (kind == 0 || kind == 6 || kind == 7) {
            paletteIndex = (LD8(chip + ACTIVE_CHIP_FLAG_BYTE) == 1)
                               ? PALETTE_ALTERNATE : PALETTE_STANDARD;
        } else {
            paletteIndex = PALETTE_OTHER;
        }
        u16 palette = LD16(A_byte_8028470 + 2 * paletteIndex);

        u32 icon = LD32(A_off_802843C + 4 * slot); /* this slot's icon sprites */
        for (int i = 0; i < 4; i++) {
            u32 attr = icon + iconAttrOffsets[i];
            out.lastAttrStored = (LD16(attr) & ATTR_PALETTE_MASK) | palette;
            ST16(attr, out.lastAttrStored);
        }
        out.attrMask = ATTR_PALETTE_MASK;
        out.lastPalette = palette;
    }

    return out;
}

Next sub_80283C8(void) {
    ChipIconPaletteResults out = recolor_active_chip_icons();
    R0 = out.lastAttrStored;
    R1 = out.attrMask;
    R2 = out.lastPalette;
    SET_FLAGS_CMP(ACTIVE_CHIP_SLOT_COUNT, ACTIVE_CHIP_SLOT_COUNT); /* loop always finishes all ten slots */
    RETURN_TO_CALLER();
}

/* ── battleObject_runCustomScreenChipPickerEffect_8028476 ── */
/* battleObject_runCustomScreenChipPickerEffect_8028476 - run whatever is recorded as the "active" chip entry for the side
 * the given battle object fights on.  getLocOfActiveChips returns the side's
 * active-chip record; the record's first byte (the entry kind) picks the
 * handler.  That kind byte is also loaded into r1 before the dispatch, so it is
 * this function's r1 output whenever no handler runs (unknown kinds). */

/* Battle object byte at +7: the side (alliance) whose active chips to look up. */
#define BATTLEOBJ_SIDE 7

/* Offsets inside the active-chip record returned by getLocOfActiveChips. */
#define ACTIVE_REC_KIND    0 /* u8:  kind of active entry (dispatch below)      */
#define ACTIVE_REC_ID_DATA 8 /* u32*: record whose first u16 is a raw chip id */

/* register form: calls getLocOfActiveChips_8027E1C outside this file, which has no typed C
 * interface yet, then performs the original's follow-up "ldrb r1,[r0]": the kind byte is
 * loaded into guest r1, an output register the caller sees whenever no handler runs, so it
 * has to be written here rather than returned through C. */
static u32 locate_active_chips(u32 side, u8 *kind) {
    R0 = side;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    *kind = LD8(R0);
    R1 = *kind;
    return R0;
}

/* register form: calls getChipID_802A54E outside this file, which has no typed C interface yet */
static u32 get_chip_id_sub_8028476(u32 raw_id, u8 kind) {
    R0 = raw_id;
    R1 = kind; /* the original still has the kind byte in r1 at this call */
    GUEST_CALL(getChipID_802A54E);
    return R0;
}

/* register form: calls battleObject_loadChipPanelGfx_80284E2 outside this file, which has no typed C interface yet */
static void activate_chip(u32 chip_id) {
    R0 = chip_id;
    /* r1 stays as getChipID left it, exactly as in the original's tail call */
    GUEST_CALL(battleObject_loadChipPanelGfx_80284E2);
}

/* register form: calls beastout_QueuePaletteGFXAndSheets_80286D4 outside this file, which has no typed C interface yet */
static void active_kind_1(u32 rec, u8 kind) {
    R0 = rec;
    R1 = kind;
    GUEST_CALL(beastout_QueuePaletteGFXAndSheets_80286D4);
}

/* register form: calls beastout_loadSelectionScreenChipUseGfx outside this file, which has no typed C interface yet */
static void active_kind_2(u32 rec, u8 kind) {
    R0 = rec;
    R1 = kind;
    GUEST_CALL(beastout_loadSelectionScreenChipUseGfx);
}

/* register form: calls beastout_loadCustomScreenGfx outside this file, which has no typed C interface yet */
static void active_kind_3(u32 rec, u8 kind) {
    R0 = rec;
    R1 = kind;
    GUEST_CALL(beastout_loadCustomScreenGfx);
}

/* register form: calls beastout_loadSelectionScreenGfx outside this file, which has no typed C interface yet */
static void active_kind_4_or_5(u32 rec, u8 kind) {
    R0 = rec;
    R1 = kind;
    GUEST_CALL(beastout_loadSelectionScreenGfx);
}

/* register form: calls battle_drawCustomChipPreview outside this file, which has no typed C interface yet */
static void active_kind_6_or_7(u32 raw_id, u8 kind) {
    R0 = raw_id;
    R1 = kind;
    GUEST_CALL(battle_drawCustomChipPreview);
}

/* register form: calls beastout_loadChipUseGfx outside this file, which has no typed C interface yet */
static void active_kind_8_or_9(u32 rec, u8 kind) {
    R0 = rec;
    R1 = kind;
    GUEST_CALL(beastout_loadChipUseGfx);
}

/* Dispatch on the active-chip record's kind byte.  Kind 0 is a plain chip: its
 * raw id is resolved to a real chip id and then activated.  Kinds 6/7 also
 * fetch the raw id but hand it over unresolved.  Every other known kind has
 * its own handler and receives the record itself; unknown kinds run nothing. */
static void run_active_chip(u32 object) {
    u8 kind;
    u32 rec = locate_active_chips(LD8(object + BATTLEOBJ_SIDE), &kind);

    switch (kind) {
    case 0:
        activate_chip(get_chip_id_sub_8028476(LD16(LD32(rec + ACTIVE_REC_ID_DATA)), kind));
        break;
    case 1:
        active_kind_1(rec, kind);
        break;
    case 2:
        active_kind_2(rec, kind);
        break;
    case 3:
        active_kind_3(rec, kind);
        break;
    case 4:
    case 5:
        active_kind_4_or_5(rec, kind);
        break;
    case 6:
    case 7:
        active_kind_6_or_7(LD16(LD32(rec + ACTIVE_REC_ID_DATA)), kind);
        break;
    case 8:
    case 9:
        active_kind_8_or_9(rec, kind);
        break;
    default:
        /* Unknown kind: nothing runs and r1 keeps the kind byte that
         * locate_active_chips already wrote there, as in the original. */
        break;
    }
}

/* battleObject_runCustomScreenChipPickerEffect_8028476(r5 = battle object).  All outputs (r1-r12, lr) are whatever the
 * guest handlers left in the register file; the port computes none itself. */
Next battleObject_runCustomScreenChipPickerEffect_8028476(void) {
    run_active_chip(R5);
    RETURN_TO_CALLER();
}

/* ── beastout_LoadSelectionScreenSpriteSheets_802869E ── */
/* beastout_LoadSelectionScreenSpriteSheets_802869E: load the three sprite sheets of the Beast Out selection screen
 * into VRAM by calling battle_loadGfxChunkBySelectorWithRows_802868C once per sheet.
 *   r0 = graphics source data, r1 = VRAM destination, r3 = sheet slot.
 * r2 is an extra argument for battle_loadGfxChunkBySelectorWithRows_802868C that this function passes through
 * untouched (it is listed as preserved), so each call sees the caller's value.
 * On return r1 = 0x6009B60 and r3 = 3 from the last setup, matching the original. */
Next beastout_LoadSelectionScreenSpriteSheets_802869E(void) {
    /* Sheet 1: byte_802A6C0 graphics -> 0x6009AA0, slot 1 */
    R0 = A_byte_802A6C0;
    R1 = 0x6009AA0;
    R3 = 1;
    GUEST_CALL(battle_loadGfxChunkBySelectorWithRows_802868C);

    /* Sheet 2: byte_802A680 graphics -> 0x6009AE0, slot 2 */
    R0 = A_byte_802A680;
    R1 = 0x6009AE0;
    R3 = 2;
    GUEST_CALL(battle_loadGfxChunkBySelectorWithRows_802868C);

    /* Sheet 3: byte_802A6C0 graphics again -> 0x6009B60, slot 3 */
    R0 = A_byte_802A6C0;
    R1 = 0x6009B60;
    R3 = 3;
    GUEST_CALL(battle_loadGfxChunkBySelectorWithRows_802868C);

    RETURN_TO_CALLER();
}

/* ── beastout_QueuePaletteGFXAndSheets_80286D4 ── */
/* register form: calls loadOneSpriteTileRow_80287D2 outside this file, which has no typed C interface yet */
static void call_sub_80287D2(void) {
    GUEST_CALL(loadOneSpriteTileRow_80287D2);
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls beastout_LoadSelectionScreenSpriteSheets_802869E outside this file, which has no typed C interface yet */
static void load_selection_screen_sprite_sheets(void) {
    GUEST_CALL(beastout_LoadSelectionScreenSpriteSheets_802869E);
}

/* Queue two DMA tile transfers (map + tileset) selected by a palette index in the
 * battle state's alliance field, then load the selection-screen sprite sheets. */
static void do_palette_gfx_and_sheets(u32 state) {
    call_sub_80287D2();
    u32 idx = (LD8(state + 8) != 0) ? 4 : 0;
    u32 gfx_src = LD32(A_off_8028708 + idx);
    queue_gfx(gfx_src, 0x6009560, 0x540);
    u32 tileset_src = LD32(A_off_8028714 + idx);
    queue_gfx(tileset_src, A_unk_3001AA0, 0x20);
    load_selection_screen_sprite_sheets();
}

Next beastout_QueuePaletteGFXAndSheets_80286D4(void) {
    do_palette_gfx_and_sheets(R5);
    RETURN_TO_CALLER();
}

/* ── beastout_loadSelectionScreenChipUseGfx ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; } call_loadOneSpriteTileRow_80287D2_result;
/* register form: generated wrapper: loadOneSpriteTileRow_80287D2 has no typed C interface yet */
static inline call_loadOneSpriteTileRow_80287D2_result call_loadOneSpriteTileRow_80287D2(void) {
    GUEST_CALL(loadOneSpriteTileRow_80287D2);
    return (call_loadOneSpriteTileRow_80287D2_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r3, r4, r5, r6, r7; } call_QueueEightWordAlignedGFXTransfer_result_beastout_loadSelectionScreenChipUseGfx;
/* register form: generated wrapper: QueueEightWordAlignedGFXTransfer has no typed C interface yet */
static inline call_QueueEightWordAlignedGFXTransfer_result_beastout_loadSelectionScreenChipUseGfx call_QueueEightWordAlignedGFXTransfer_beastout_loadSelectionScreenChipUseGfx(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    return (call_QueueEightWordAlignedGFXTransfer_result_beastout_loadSelectionScreenChipUseGfx){ R3, R4, R5, R6, R7 };
}

typedef struct { u32 r0; } call_getLocOfActiveChips_8027E1C_result_beastout_loadSelectionScreenChipUseGfx;
/* register form: generated wrapper: getLocOfActiveChips_8027E1C has no typed C interface yet */
static inline call_getLocOfActiveChips_8027E1C_result_beastout_loadSelectionScreenChipUseGfx call_getLocOfActiveChips_8027E1C_beastout_loadSelectionScreenChipUseGfx(u32 r0) {
    R0 = r0;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    return (call_getLocOfActiveChips_8027E1C_result_beastout_loadSelectionScreenChipUseGfx){ R0 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7; } call_beastout_LoadSelectionScreenSpriteSheets_802869E_result;
/* register form: generated wrapper: beastout_LoadSelectionScreenSpriteSheets_802869E has no typed C interface yet */
static inline call_beastout_LoadSelectionScreenSpriteSheets_802869E_result call_beastout_LoadSelectionScreenSpriteSheets_802869E(void) {
    GUEST_CALL(beastout_LoadSelectionScreenSpriteSheets_802869E);
    return (call_beastout_LoadSelectionScreenSpriteSheets_802869E_result){ R2, R3, R4, R5, R6, R7 };
}

/* Beast-out selection screen: load the chip-use graphics.
 *
 * The original queues two GFX DMA transfers and then reloads the
 * selection-screen sprite sheets:
 *   1. loadOneSpriteTileRow_80287D2 stages one tile row of chip-use art,
 *   2. that staged row is queued to the VRAM tile area at 0x6009560,
 *   3. the 0x20-byte icon sheet of the currently selected chip (slot 11 in
 *      the active-chip list, byte at +6 = chip index) is staged in IWRAM
 *      and queued as a second transfer,
 *   4. beastout_LoadSelectionScreenSpriteSheets_802869E finishes by
 *      reloading the sprite sheets.
 *
 * All of r2-r7 are left behind by the last callee, and the intermediates
 * flow through the guest registers the wrappers preserve, so the typed
 * function itself returns nothing.
 */
static void beastout_loadSelectionScreenChipUseGfx_typed(void) {
    /* Stage the chip-use tile row that gets transferred below. */
    call_loadOneSpriteTileRow_80287D2();

    /* Queue the staged tile row: src byte_8723034 -> VRAM 0x6009560, 0x540 bytes. */
    call_QueueEightWordAlignedGFXTransfer_beastout_loadSelectionScreenChipUseGfx(A_byte_8723034, 0x6009560, 0x540);

    /* Look up the selected chip in the active-chip list (kind 0xB) and read
     * its index from offset 6; each icon sheet is 0x20 bytes. */
    u32 chipList = call_getLocOfActiveChips_8027E1C_beastout_loadSelectionScreenChipUseGfx(0xB).r0;
    u32 chipIndex = LD8(chipList + 6);
    u32 iconSrc = A_dword_8725814 + chipIndex * 0x20;

    /* Queue the selected chip's icon sheet: iconSrc -> IWRAM unk_3001AA0, 0x20 bytes. */
    call_QueueEightWordAlignedGFXTransfer_beastout_loadSelectionScreenChipUseGfx(iconSrc, A_unk_3001AA0, 0x20);

    /* Reload the selection-screen sprite sheets on top of the new graphics. */
    call_beastout_LoadSelectionScreenSpriteSheets_802869E();
}

/* beastout_loadSelectionScreenChipUseGfx: - -> r2 r3 r4 r5 r6 r7 */
Next beastout_loadSelectionScreenChipUseGfx(void) {
    beastout_loadSelectionScreenChipUseGfx_typed();
    RETURN_TO_CALLER();
}

/* ── loadOneSpriteTileRow_80287D2 ── */
/* register form: calls battle_loadGfxChunkBySelectorWithRows_802868C outside this file, which has no typed C interface yet */
static void sprite_descriptor_fill(void) {
    /* Copy 8 bytes of sprite descriptor table (byte_802A6C0) into the
     * engine's 8-byte sprite descriptor slot at 0x6009360. */
    R0 = (u32)A_byte_802A6C0;
    R1 = 0x6009360;
    R3 = 8;
    GUEST_CALL(battle_loadGfxChunkBySelectorWithRows_802868C);
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file,
 * which has no typed C interface yet */
static void queue_sprite_tile_gfx(void) {
    /* Queue a 0x20-byte tile row from ROM (byte_86E587C) into VRAM
     * (unk_3001A80). */
    R0 = (u32)A_byte_86E587C;
    R1 = (u32)A_unk_3001A80;
    R2 = 0x20;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* Queue a sprite tile row into VRAM and prime the sprite descriptor slot. */
static void load_one_sprite_tile_row(void) {
    sprite_descriptor_fill();
    queue_sprite_tile_gfx();
}

/* loadOneSpriteTileRow_80287D2(r2, r4, r5, r6, r7, lr) -> r4 r5 r6 r7 (preserved) */
Next loadOneSpriteTileRow_80287D2(void) {
    load_one_sprite_tile_row();
    RETURN_TO_CALLER();
}

/* ── battleObject_computeBButtonOffsets_8028894 ── */
/* battleObject_computeBButtonOffsets_8028894: pick a pair of offsets from the ID byte at [R5+7].
 * IDs 0-4 index the first half of the byte_80288B0 table (R1 stays 0),
 * IDs 5+ index the second half and start R1 at 0x18 instead.
 * Returns R0 = index*16 + 8 and R1 = start + 0x68 (flags set from that add),
 * and leaves the byte_80288B0 table address in R6. */
Next battleObject_computeBButtonOffsets_8028894(void) {
    u32 index = LD8(R5 + 7);
    u32 r1Base = 0;

    if (index >= 5) {
        r1Base = 0x18;
        index -= 5;
    }

    R6 = A_byte_80288B0;
    R0 = (index << 4) + 8;
    R1 = r1Base + 0x68;
    SET_FLAGS_ADD(r1Base, 0x68);

    RETURN_TO_CALLER();
}

/* ── loadConstArgsAndPairTable_80288D0 ── */
/* loadConstArgsAndPairTable_80288D0: load arguments for a follow-up call.
 * r0 = 0x5B, r1 = 0x6E (flags left over from computing r1 = 0x70 - 2),
 * r6 = address of the 16-entry byte-pair table at byte_80288E4
 * (entries look like <value, flag> pairs, e.g. dialog/choice parameters). */
Next loadConstArgsAndPairTable_80288D0(void) {
    R0 = 0x58 + 3;
    SET_FLAGS_CMP(0x70, 2);
    R1 = 0x70 - 2;
    R6 = A_byte_80288E4;
    RETURN_TO_CALLER();
}

/* ── chatbox_getWindowLayout_8028904 ── */
/* chatbox_getWindowLayout_8028904: returns a fixed set-up for a text/message box:
 *   r0 = 0x5B and r1 = 0x87 (box coordinates: x=91, y=135),
 *   r6 = address of the byte table at byte_8028918 (entry data used by the caller).
 * The constants are the tail of "mov r0,#0x58; add r0,#3" / "mov r1,#0x88; sub r1,#1". */

typedef struct {
    u32 box_x;
    u32 box_y;
    u32 entry_table;
} TextBoxSetup;

static TextBoxSetup get_textbox_setup(void) {
    TextBoxSetup setup;
    setup.box_x = 0x58 + 3;          /* 0x5B */
    setup.box_y = 0x88 - 1;          /* 0x87 */
    setup.entry_table = A_byte_8028918;
    return setup;
}

/* chatbox_getWindowLayout_8028904(lr = return address) -> r0, r1, r6 */
Next chatbox_getWindowLayout_8028904(void) {
    TextBoxSetup setup = get_textbox_setup();
    R0 = setup.box_x;
    R1 = setup.box_y;
    R6 = setup.entry_table;
    RETURN_TO_CALLER();
}

/* ── sub_802899C ── */
/* sub_802899C(r5 = battle object, r10 = toolkit):
 * While a battle effect is active (battleState+0x17 != 0), stream this
 * object's graphics to the hardware:
 *   - [r5+0x40] selects the source: its low 3 bits are a bank id, and when
 *     the bank is 0 one of two 0x200-byte graphics blocks at dword_86E1238
 *     (chosen by bit 3) is queued into VRAM at 0x6016CC0.
 *   - a FIFO record is then pushed for the cartridge-side handler either way.
 * r4/r5 (and r6/r7) end unchanged; r0-r3 and the flags are whatever
 * fifo_pushRecord16_8009FF8 leaves behind, or the state-test result on the
 * early exit. */
Next sub_802899C(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 effectState = LD8(battleState + 0x17);
    SET_FLAGS_CMP(effectState, 0);
    if (effectState == 0) {
        R0 = effectState;
        RETURN_TO_CALLER();
    }

    u32 bankSelect = LD32(R5 + 0x40);
    u32 gfxBank = bankSelect & 7;
    /* the original holds these intermediate values in r4/r5 across the calls */
    R4 = gfxBank;
    R5 = bankSelect;

    if (gfxBank == 0) {
        u32 srcOffset = (bankSelect & 8) << 6; /* 0 or 0x200 */
        R0 = A_dword_86E1238 + srcOffset;
        R1 = 0x6016CC0;
        R2 = 0x200;
        GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    }

    R0 = 0x80020064;
    R1 = 0xB766;
    R2 = 1;
    R3 = 0;
    GUEST_CALL(fifo_pushRecord16_8009FF8);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── custMenuPressOK_8028D3A ── */
/* custMenuPressOK_8028D3A: "OK" pressed in the custom screen menu.
 * r5 = menu state struct (oS20364C0_*). Runs the pre-OK bookkeeping calls,
 * dispatches one of the two highlight handlers depending on the current
 * selection state, then triggers the handler switch in custMenuMainMaybe_8026A88
 * by setting the jump offset at r5+1 to 8, clears the counter at r5+2, plays the
 * select sound and returns 0 in r0 (Z set, as after Thumb `mov r0, #0`).
 * The original returns via `pop {pc}`, i.e. to the LR captured on entry, so the
 * entry LR must be preserved across the guest calls. */
Next custMenuPressOK_8028D3A(void) {
    u32 return_address = LR;
    u32 menu = R5;

    GUEST_CALL(custMenu_confirmAndProcessSelectedChips_8029110);
    GUEST_CALL(sub_80293F8);

    if (LD8(menu + oS20364C0_HasPendingAction) != 0) {
        R0 = LD8(menu + oS20364C0_HasPendingAction);
        GUEST_CALL(sub_802937A);
    } else {
        u16 unk3a = LD16(menu + oS20364C0_CursorAnimPhase);
        if (unk3a != 0) {
            R0 = unk3a;
            GUEST_CALL(sub_80293B0);
        }
    }

    /* hand control back to the custom menu main loop with jump offset 8
     * (these two fields are addressed by raw offsets in the disassembly) */
    ST8(menu + 1, 8);
    ST16(menu + 2, 0);

    R0 = SOUND_SELECT_82;
    GUEST_CALL(PlaySoundEffect);

    LR = return_address;
    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── customScreen_ExitSequence_8028E32 ── */
/* customScreen_ExitSequence_8028E32: exit sequence for the Custom screen.
 * Runs the close-out steps (update the custom screen state after the player
 * unselects a chip, then the two follow-up updates) and finally calls
 * battle_customScreenReloadFromChipEntries_8028250 with r0 = 0 before returning to the caller. */
Next customScreen_ExitSequence_8028E32(void) {
    u32 return_address = LR;

    GUEST_CALL(classifyActiveChipElements_8028E4C);
    GUEST_CALL(updateCustomScreen_WhenUnselectingChip_8028EC8);
    GUEST_CALL(battle_updateActiveChipSlotB_state_8028F48);
    GUEST_CALL(battle_updateExtraSlotChipKeepFlag_8028F84);

    R0 = 0;
    GUEST_CALL(battle_customScreenReloadFromChipEntries_8028250);

    LR = return_address;
    RETURN_TO_CALLER();
}

/* ── updateCustomScreen_WhenUnselectingChip_8028EC8 ── */
/* register form: calls getLocOfActiveChips_8027E1C outside this file, which has no typed C interface yet */
static u32 get_loc_of_active_chips(u32 param) {
    u32 s2 = R2, s3 = R3, s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = param;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 res = R0;
    R2 = s2; R3 = s3; R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return res;
}

/* register form: calls getChipID_802A54E outside this file, which has no typed C interface yet */
static u32 get_chip_id(u16 raw_data) {
    u32 s1 = R1, s2 = R2, s3 = R3, s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = (u32)raw_data;
    GUEST_CALL(getChipID_802A54E);
    u32 res = R0;
    R1 = s1; R2 = s2; R3 = s3; R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return res;
}

/* register form: calls chipIDIs13f_8029FF4 outside this file, which has no typed C interface yet */
static bool chip_id_is_13f(u32 id) {
    u32 s2 = R2, s3 = R3, s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = id;
    GUEST_CALL(chipIDIs13f_8029FF4);
    bool res = R0 != 0;
    R2 = s2; R3 = s3; R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return res;
}

#define SLOT_SIZE 12
#define MAX_ACTIVE_SLOTS 10
#define STAR_CODE 0x1A
#define SPECIAL_CODE_LO 0x1B
#define SPECIAL_CODE_HI 0x1C
#define ACTIVE_CHIP_OFF 0x20
#define SELECTED_COUNT_OFF 8
#define MONOCODE_OFF 9
#define ACTIVE_CODE_OFF 0x0A
#define SELECTED_FLAG_OFF 7

/*
 * Mark each of the 10 active-chip slots as selectable (0) or unselectable (1).
 * state: custom-screen control block.
 * Returns the final scratch value that the loop leaves in r2 (used as output).
 */
static u32 update_chip_slot_selections(u32 state, u32 r2_scratch) {
    u32 slot = get_loc_of_active_chips(0);

    for (int i = 0; i < MAX_ACTIVE_SLOTS; i++) {
        /* Skip non-empty slots and slots already marked "selected" (2) */
        if (LD8(slot) != 0) { slot += SLOT_SIZE; continue; }
        if (LD8(slot + SELECTED_FLAG_OFF) == 2) { slot += SLOT_SIZE; continue; }

        /* If 5 or more chips already selected, no more can be added */
        if (LD8(state + SELECTED_COUNT_OFF) >= 5) {
            ST8(slot + SELECTED_FLAG_OFF, 1);
            slot += SLOT_SIZE;
            continue;
        }

        /* Decode the chip's code and ID from its data pointer */
        u32 data_ptr = LD32(slot + 8);
        u32 chip_val = get_chip_id((u16)LD16(data_ptr));
        u32 code = chip_val >> 9;
        u32 id = chip_val & 0x1FF;
        u32 flag = 0;  /* assume selectable; set to 1 if a check rejects it */

        /* Chips with special ID 0x13f are always selectable */
        if (chip_id_is_13f(id)) {
            ST8(slot + SELECTED_FLAG_OFF, flag);
            slot += SLOT_SIZE;
            continue;
        }

        /* Codes 0x1B–0x1C: special active-code gating */
        if (code >= SPECIAL_CODE_LO && code <= SPECIAL_CODE_HI) {
            u32 active_code = LD8(state + ACTIVE_CODE_OFF);
            if (active_code == 0) {
                flag = 0;
            } else if (active_code == code) {
                flag = 0;
            } else {
                flag = 1;
            }
            ST8(slot + SELECTED_FLAG_OFF, flag);
            slot += SLOT_SIZE;
            continue;
        }

        /* Normal code path: check against active chip */
        {
            s32 active_chip = (s32)LDS16(state + ACTIVE_CHIP_OFF);
            r2_scratch = (u32)active_chip;

            if (active_chip != -1 && id == (u32)active_chip) {
                ST8(slot + SELECTED_FLAG_OFF, flag);  /* selectable */
                slot += SLOT_SIZE;
                continue;
            }
        }

        /* Monocode path (active_chip == -1, or chip ID didn't match active) */
        {
            u32 mono = LD8(state + MONOCODE_OFF);
            r2_scratch = mono;

            if (mono == 0xFF) {
                flag = 1;
                ST8(slot + SELECTED_FLAG_OFF, flag);
                slot += SLOT_SIZE;
                continue;
            }

            if (mono == STAR_CODE) {
                /* All selected chips are stars so far → always compatible */
                ST8(slot + SELECTED_FLAG_OFF, flag);
                slot += SLOT_SIZE;
                continue;
            }
        }

        /* Mixed codes: star wildcard or exact monocode match required */
        if (code == STAR_CODE) {
            /* Chip is a star → selectable regardless of monocode */
            flag = 0;
        } else if (code == r2_scratch) {
            /* Chip code matches the current monocode */
            flag = 0;
        } else {
            flag = 1;
        }
        ST8(slot + SELECTED_FLAG_OFF, flag);
        slot += SLOT_SIZE;
    }

    return r2_scratch;
}

/* updateCustomScreen_WhenUnselectingChip_8028EC8(r2,r4,r5,r6,r7,r10) -> r2,r3; r4,r5,r6,r7 preserved */
Next updateCustomScreen_WhenUnselectingChip_8028EC8(void) {
    R2 = update_chip_slot_selections(R5, R2);
    R3 = 0;
    RETURN_TO_CALLER();
}

/* ── battle_updateActiveChipSlotB_state_8028F48 ── */
/* battle_updateActiveChipSlotB_state_8028F48(r4..r7, r10, lr = caller context; r5 = battle object):
 * Bookkeeping for slot 11 of the active-chip list (the chip currently being
 * handled on the custom screen).  getLocOfActiveChips_8027E1C returns that
 * slot's record.  If the record is in state 2 and its byte at +7 has not yet
 * been set to 2, this decides the byte at +7:
 *   - cleared to 0 when the battle object's byte at +8 is below 5, the helper
 *     battle_checkEmotionAllowsAction_802A57E reports non-zero, and the object's byte at +0x19 is zero;
 *   - set to 1 in every other case.
 * The early-exit paths leave the record untouched.  r4-r7 are preserved and
 * r2/r3 pass through unchanged. */
Next battle_updateActiveChipSlotB_state_8028F48(void) {
    R0 = 0xB;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 chip = R0;

    if (LD8(chip) != 2)
        RETURN_TO_CALLER();
    if (LD8(chip + 7) == 2)
        RETURN_TO_CALLER();

    u32 flag;
    if ((s32)LD8(R5 + 8) >= 5) {
        flag = 1;
    } else {
        /* battle_checkEmotionAllowsAction_802A57E is queried with the object's byte at +8 in r0, as in
         * the original call site; it reports non-zero when the chip may be
         * unmarked. */
        R0 = LD8(R5 + 8);
        GUEST_CALL(battle_checkEmotionAllowsAction_802A57E);
        u32 allowed = R0;
        flag = (allowed != 0 && LD8(R5 + 0x19) == 0) ? 0 : 1;
    }
    ST8(chip + 7, flag);

    RETURN_TO_CALLER();
}

/* ── battle_updateExtraSlotChipKeepFlag_8028F84 ── */
/* battle_updateExtraSlotChipKeepFlag_8028F84: update the "this chip is still on the field" flag (offset 7) of
 * active chip slot 8.
 *
 * getLocOfActiveChips_8027E1C(n) returns a pointer to active-chip slot record n.
 * The flag is cleared only when a chip is in slot 8, its flag is neither 2 nor 3,
 * the chip folder selection at R5 names another chip (byte +8 != 0), and that
 * other chip no longer has an active slot (its record's state byte is 0).
 * In every other reachable case the flag is set to 1; when slot 8 is empty or
 * its flag is 2 nothing is written. */
Next battle_updateExtraSlotChipKeepFlag_8028F84(void) {
    R0 = 8;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 slot = R0;

    /* No chip occupying slot 8: nothing to do. */
    if (LD8(slot) != 8)
        RETURN_TO_CALLER();

    u8 state = LD8(slot + 7);

    /* Flag 2 means another handler already took care of this slot. */
    if (state == 2)
        RETURN_TO_CALLER();

    u8 keep;
    if (state == 3) {
        keep = 1;
    } else {
        /* R5 holds the chip-selection state: byte +8 is the currently
         * selected chip's index (0 = none), and the table at +0x47 maps
         * that index to the partner chip's slot number. */
        u32 sel = LD8(R5 + 8);
        if (sel == 0) {
            keep = 1;
        } else {
            R0 = LD8(R5 + sel + 0x47);
            GUEST_CALL(getLocOfActiveChips_8027E1C);
            /* The partner's slot record is empty -> the chip is gone. */
            keep = (LD8(R0) != 0);
        }
    }

    ST8(slot + 7, keep);
    RETURN_TO_CALLER();
}

/* ── customScreen_ExitAction_8029032 ── */
#define TILE_VRAM_SLOT    0x1FF
#define SOUND_CONFIRM     0x1D2  /* 0xd3 + 0xff */
#define CHIP_ATTR_OFFSET  0x55   /* base offset for pending-jack-in attribute */

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_sub_8029032(u32 sound_id) {
    R0 = sound_id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls battle_ClearPendingEventFlag_802A0EC outside this file, which has no typed C interface yet */
static void clear_pending_event_flag(void) {
    GUEST_CALL(battle_ClearPendingEventFlag_802A0EC);
}

/* register form: calls getLocOfActiveChips_8027E1C outside this file, which has no typed C interface yet */
static u32 get_active_chips_loc(u32 index) {
    R0 = index;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    return R0;
}

/* register form: calls sprite_CopyObjTilesToVramSlot_80281D4 outside this file, which has no typed C interface yet */
static void copy_obj_tiles(u32 slot, u32 counter, u32 chip_ptr) {
    R0 = slot;
    R1 = counter;
    R4 = chip_ptr;
    GUEST_CALL(sprite_CopyObjTilesToVramSlot_80281D4);
}

/* register form: calls customScreen_redrawChipSlot_8029CD4 outside this file, which has no typed C interface yet */
static void update_chip_display(u32 count) {
    R0 = count; R1 = 0; R2 = 0;
    GUEST_CALL(customScreen_redrawChipSlot_8029CD4);
}

/* register form: calls sub_802A00C outside this file, which has no typed C interface yet */
static u32 check_chip_condition(u32 value, u32 chip_ptr) {
    R0 = value;
    R4 = chip_ptr;
    GUEST_CALL(sub_802A00C);
    return R0;
}

/* register form: calls customScreen_ExitSequence_8028E32 outside this file, which has no typed C interface yet */
static void exit_custom_screen(void) {
    GUEST_CALL(customScreen_ExitSequence_8028E32);
}

/* register form: calls battleObject_runCustomScreenChipPickerEffect_8028476 outside this file, which has no typed C interface yet */
static void post_exit_cleanup(void) {
    GUEST_CALL(battleObject_runCustomScreenChipPickerEffect_8028476);
}

/* Handle the confirm/exit action on the custom battle-screen chip submenu.
 * state_ptr points to screen state: byte at +8 = selection count,
 * +0x19 = jack-in pending flag, +0x1a/+0x1b/+0x55+idx = pending attribute slots. */
static void custom_screen_exit_action(u32 state_ptr) {
    u8 selection = LD8(state_ptr + 8);

    if (selection == 0) {
        if (LD8(state_ptr + 0x19) == 0) {
            play_sound_sub_8029032(SOUND_CANT_JACK_IN);
            return;
        }
        u8 pending_idx = LD8(state_ptr + 0x1b);
        ST8(state_ptr + CHIP_ATTR_OFFSET + pending_idx, 0);
        ST8(state_ptr + 0x1a, 0);
        ST8(state_ptr + 0x19, 0);
        play_sound_sub_8029032(SOUND_CONFIRM);
        clear_pending_event_flag();
    } else {
        u32 chip_ptr = get_active_chips_loc(LD8(state_ptr + selection + 0x47));

        if (LD8(chip_ptr) == 2) {
            u8 new_sel = LD8(state_ptr + 8) - 1;
            ST8(state_ptr + 8, new_sel);
            copy_obj_tiles(TILE_VRAM_SLOT, new_sel, chip_ptr);
            update_chip_display(new_sel);
            clear_pending_event_flag();
            play_sound_sub_8029032(SOUND_EXIT_SUBMENU);
            play_sound_sub_8029032(SOUND_CONFIRM);
            ST8(state_ptr + 0x17, 0);
        } else {
            u16 chip_data = LD16(LD32(chip_ptr + 8));
            if (check_chip_condition(chip_data, chip_ptr) != 0) {
                play_sound_sub_8029032(SOUND_EXIT_SUBMENU);
                play_sound_sub_8029032(SOUND_CONFIRM);
                clear_pending_event_flag();
            }
            u8 new_sel = LD8(state_ptr + 8) - 1;
            ST8(state_ptr + 8, new_sel);
            copy_obj_tiles(TILE_VRAM_SLOT, new_sel, chip_ptr);
            update_chip_display(new_sel);

            if (LD8(chip_ptr + 5) != 0) {
                u8 extra = LD8(chip_ptr + 5);
                ST8(chip_ptr + 5, 0);
                ST8(chip_ptr + 4, LD8(chip_ptr + 4) & ~0x3E);
                u32 extra_loc = get_active_chips_loc(extra);
                ST8(extra_loc + 7, 0);
            }
        }
        ST8(chip_ptr + 7, 0);
    }
    exit_custom_screen();
    post_exit_cleanup();
    play_sound_sub_8029032(SOUND_EXIT_SUBMENU);
}

/* customScreen_ExitAction_8029032(r5 = screen state ptr) -> outputs r2 r3 r5 r6 r7 r8 r9 r10 r11 r12; preserves r4 r7 */
Next customScreen_ExitAction_8029032(void) {
    u32 saved_r4 = R4;
    custom_screen_exit_action(R5);
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── processChipCommandQueue_8029224 ── */
/* Chip command queue base in work RAM. */
#define CHIP_QUEUE_BASE 0x2033000

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_8029224(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* Walk the chip-use command queue at CHIP_QUEUE_BASE, processing opcode slots.
 *
 * Each slot holds a s16 opcode; the preceding u16 is the chip index.
 * Recognised opcodes (0xB8, 0xB9, 0xC0, 0xC1, 0xC3) are validated against the
 * chip's attribute byte (offset 9 of ChipData).  On success the side-effect is
 * applied, a status bit from the target object is merged, and the entry is
 * removed by shifting all following slots down until the 0xFFFF sentinel.
 * The scanner then re-reads the same slot to pick up the shifted-in entry.
 *
 * NOTE: r3 (the target object for the merge) is clobbered by the shift loop,
 * exactly as in the original — subsequent iterations use the stale pointer.  */
static void process_chip_command_queue(u32 target_obj) {
    u32 r3 = target_obj;
    u32 pos = CHIP_QUEUE_BASE + 2;

    for (;;) {
        s16 code = LDS16(pos);
        if (code == -1)
            break;

        /* ---- dispatch: required / forbidden attribute masks ---- */
        u32 need_mask, forbid_mask;
        switch (code) {
        case 0xC0: case 0xC3: case 0xB8:
            need_mask = 2;  forbid_mask = 0; break;
        case 0xC1:
            need_mask = 4;  forbid_mask = 0; break;
        case 0xB9:
            need_mask = 2;  forbid_mask = 1; break;
        default:
            pos += 2;
            continue;
        }

        /* ---- look up chip data for the index stored just before the opcode ---- */
        u32 chip_base = pos - 2;
        u32 chip = get_chip_data_sub_8029224(LD16(chip_base));
        u8 attr = LD8(chip + 9);

        if ((attr & need_mask) == 0 || (attr & forbid_mask) != 0) {
            pos += 2;
            continue;
        }

        /* ---- per-command side effect on the queue entry ---- */
        code = LDS16(pos);  /* reload; original does a second ldrsh here */
        if (code == 0xB8) {
            ST16(chip_base + 0x3C, LD16(chip_base + 0x3C) | 2);
        } else if (code == 0xB9) {
            ST16(chip_base + 0x3C, LD16(chip_base + 0x3C) | 4);
        } else {
            /* 0xC0 / 0xC1 / 0xC3: accumulate field +0xC into +0x16 */
            ST16(pos + 0x16, LD16(pos + 0x16) + LD16(pos + 0xC));
        }

        /* ---- merge bit 0 of r3+0x3C into r3+0x3A ---- */
        u16 bit0 = LD16(r3 + 0x3C) & 1;
        ST16(r3 + 0x3A, LD16(r3 + 0x3A) | bit0);

        /* ---- shift every following slot down by 2 bytes until sentinel ---- */
        u32 dst = pos;
        r3 = pos + 2;  /* src — this overwrites the caller's r3 (clobbered) */

        for (;;) {
            ST16(dst + 0x3C, LD16(r3 + 0x3C));
            ST16(dst + 0x0C, LD16(r3 + 0x0C));
            ST16(dst + 0x24, LD16(r3 + 0x24));
            s16 moved = LDS16(r3);
            ST16(dst, moved);
            r3 += 2;
            dst += 2;
            if (moved == -1)
                break;
        }

        pos -= 2;  /* back up so the +2 below keeps us on the same slot */
        pos += 2;  /* net no-op: re-examine the slot that now holds the shifted-in entry */
    }
}

/* processChipCommandQueue_8029224(r3 = target object) — no outputs, clobbers r0–r3, flags, lr */
Next processChipCommandQueue_8029224(void) {
    process_chip_command_queue(R3);
    RETURN_TO_CALLER();
}

/* ── sub_80293F8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_getLocOfActiveChips_8027E1C_result;
/* register form: generated wrapper: getLocOfActiveChips_8027E1C has no typed C interface yet */
static inline call_getLocOfActiveChips_8027E1C_result call_getLocOfActiveChips_8027E1C(u32 r0) {
    R0 = r0;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    return (call_getLocOfActiveChips_8027E1C_result){ R0 };
}

/* register form: generated wrapper: GetBattleMode has no typed C interface yet */
static inline u32 call_GetBattleMode_sub_80293F8(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* Walk the selected-chip list in the chip-select HUD. For each entry resolve
 * its chip record; if the chip is ID 2 (linked) and battle mode is active,
 * follow the link to the underlying record. When the final chip record has
 * kind 0 (an active chip): clear its in-flight firing flag (if set), write
 * -1 to its hit counter, and decrement the battle-state active-chip count.
 * r1/r2/r3 are left at whatever the last loop iteration set them to. */
typedef struct {
    u32 r1, r2, r3;
} ChipWalkResult;

static ChipWalkResult walk_selected_chips(u32 chips_base, u32 toolkit,
                                           u32 in_r1, u32 in_r2, u32 in_r3) {
    u32 r1 = in_r1, r2 = in_r2, r3 = in_r3;
    u32 count = LD8(chips_base + oS20364C0_NumChipsSelected);
    u32 idx = 0x48;
    /* r7 = 0 - 1 in the original; stored as halfword -1 to hit counters */
    u16 const hit_sentinel = (u16)(0 - 1);

    while (count != 0) {
        u32 chip_id = LD8(chips_base + idx);
        u32 chip_ptr = call_getLocOfActiveChips_8027E1C(chip_id).r0;
        r1 = LD8(chip_ptr);

        if (r1 == 2) {
            /* Linked chip: only follow through in standard battle mode */
            u32 mode = call_GetBattleMode_sub_80293F8();
            if (mode != 1) {
                /* r1 stays 2; skip rest of body */
                idx = idx + 1;
                count = count - 1;
                continue;
            }
            chip_ptr = call_getLocOfActiveChips_8027E1C(LD8(chip_ptr + 4)).r0;
            r1 = LD8(chip_ptr);
        }

        if (r1 == 0) {
            /* Active chip found – reset its state */
            r1 = LD8(chip_ptr + 4);  /* bit-0 = in-flight firing flag */
            if (r1 & 1) {
                r2 = LD32(toolkit + oToolkit_BattleStatePtr);
                ST8(r2 + 0x17, 0);
            }

            ST16(LD32(chip_ptr + 8), hit_sentinel);

            r2 = LD32(toolkit + oToolkit_BattleStatePtr);
            r1 = 0x44;
            r3 = LD8(r2 + r1);
            if (r3) {
                r1 = 0x45;
                r3 = LD8(r2 + r1);
                r3 = r3 - 1;
                ST8(r2 + r1, r3);
            }
        }

        idx = idx + 1;
        count = count - 1;
    }

    ChipWalkResult result = { r1, r2, r3 };
    return result;
}

Next sub_80293F8(void) {
    ChipWalkResult res = walk_selected_chips(R5, R10, R1, R2, R3);
    R1 = res.r1;
    R2 = res.r2;
    R3 = res.r3;
    /* r4, r6, r7 preserved by the original's push/pop – left unchanged */
    RETURN_TO_CALLER();
}

/* ── battle_compactFolderChipList_802945A ── */
/* battle_compactFolderChipList_802945A: compact the battle folder chip list (30 shorts at eBattleFolder).
 * Drops every empty slot (ID 0xFFFF), shifts the kept entries to the front,
 * and pads the freed tail with 0xFFFF so the list always has 30 entries.
 * r4/r6/r7 are only used as scratch by the original, which pushes and pops
 * them, so their values are preserved for the caller. */
Next battle_compactFolderChipList_802945A(void) {
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;

    u32 folder = A_eBattleFolder;
    u32 emptyId = 0xFFFFFFFFu;  /* 0xFFFF sign-extended, the "no chip" ID */
    u32 readOff = 0;            /* byte offset of slot being examined */
    u32 writeOff = 0;           /* byte offset of next free slot */
    u32 slotsToFill = 30;       /* entries still needed to fill the list */

    while (readOff < 0x3C) {
        s32 entry = LDS16(folder + readOff);
        if (entry != (s32)emptyId) {
            ST16(folder + writeOff, (u16)entry);
            writeOff += 2;
            slotsToFill--;
        }
        readOff += 2;
    }

    /* Pad whatever is left of the list with empty slots. */
    while (slotsToFill != 0) {
        ST16(folder + writeOff, (u16)emptyId);
        writeOff += 2;
        slotsToFill--;
    }

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_scanChipSeq_80295C8 ── */
/* Scans a chip/program u16 sequence in a battle buffer.
 * First verifies `count` consecutive words all match the target at [data_ptr+4],
 * then walks backwards through a trailing region validating a descending value
 * pattern (0x1A is a wildcard, at most one allowed, value must not go negative).
 * Returns field [data_ptr+2] and adjusted offset on success, or 0 on failure. */

typedef struct {
    u32 r0;       /* result: field from data_ptr+2 on success, 0 on failure */
    u32 r1;       /* on success: r6-8; on failure: last r1 value from loop */
    u32 flag_a;   /* left operand of the final CMP/SUBS for flags */
    u32 flag_b;   /* right operand of the final CMP/SUBS for flags */
    u32 nz_val;   /* value for the final MOVS (0 on failure, r1 on success) */
} SeqScanResult;

static SeqScanResult scan_chip_sequence(u32 count, u32 data_ptr, u32 offset, u32 base) {
    u32 r6 = offset;
    u32 target = LD16(data_ptr + 4);
    u32 r0, r1 = target;

    /* First pass: verify `count` consecutive u16s all equal target */
    u32 remaining = count;
    for (;;) {
        r0 = LD16(base + r6);
        if (r0 != target)
            return (SeqScanResult){0, r1, r0, target, 0};
        r6 += 2;
        remaining--;
        if (remaining == 0)
            break;
    }

    /* Advance past matched block into the trailing descending region */
    u32 wildcard_count = 0;
    r6 += 8;
    r0 = LD16(base + r6) + 1;

    /* Second pass: walk `count` entries validating descending pattern */
    u32 iterations = count;
    for (;;) {
        r1 = LD16(base + r6);
        if (r1 == 0x1A) {
            /* First wildcard at the boundary: re-derive expected from prior entry */
            if (wildcard_count == 0 && r0 == 0x1B)
                r0 = LD16(base + r6 - 2) + 2;
            r1 = r0 - 1;
            wildcard_count++;
        }
        r0--;
        if (r0 != r1)
            return (SeqScanResult){0, r1, r0, r1, 0};
        r6 -= 2;
        iterations--;
        if (iterations == 0)
            break;
    }

    /* Final constraints: at most one wildcard, value must not go below 0 */
    if ((s32)wildcard_count > 1)
        return (SeqScanResult){0, r1, wildcard_count, 1, 0};
    if ((s32)r0 < 0)
        return (SeqScanResult){0, r1, r0, 0, 0};

    /* Success: return data field and adjusted scan position */
    r0 = LD16(data_ptr + 2);
    r1 = r6 - 8;
    return (SeqScanResult){r0, r1, r6, 8, r1};
}

/* battle_scanChipSeq_80295C8(r2 = count, r3 = data_ptr, r6 = buffer_offset, r7 = buffer_base) -> r0 r1 nzcv */
Next battle_scanChipSeq_80295C8(void) {
    SeqScanResult res = scan_chip_sequence(R2, R3, R6, R7);
    R0 = res.r0;
    R1 = res.r1;
    SET_FLAGS_CMP(res.flag_a, res.flag_b);
    SET_FLAGS_NZ(res.nz_val);
    RETURN_TO_CALLER();
}

/* ── matchHalfwordPatternEntry_802961A ── */
/* matchHalfwordPatternEntry_802961A: compare a run of halfwords against a reference list.
 * r2 = number of halfwords to check, r3 = pointer to a reference record
 * (result halfword at +2, expected halfword list at +4), r6 = source
 * stride, r7 = source base pointer. The halfwords at r7+r6, r7+r6+2, ...
 * are compared against the list at r3+4, r3+6, ... for r2 entries.
 * If all of them match, r0 = halfword at r3+2 and r1 = r6; otherwise
 * r0 = 0 and r1 = the expected halfword that did not match.
 * Flags: nz from the final result move, c/v from the last comparison;
 * r2, r4 and r7 are preserved. */
Next matchHalfwordPatternEntry_802961A(void) {
    u32 remaining = R2;
    u32 src = R7 + R6; /* halfwords being tested */
    u32 ref = R3 + 4;  /* expected halfword list */

    u32 actual, expected;
    do {
        actual = LD16(src);
        expected = LD16(ref);
        /* the comparison is the last instruction that sets C and V */
        SET_FLAGS_CMP(actual, expected);
        if (actual != expected) {
            /* mismatch: r0 = 0, r1 = the expected halfword that failed */
            R0 = 0;
            R1 = expected;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }
        src += 2;
        ref += 2;
    } while (--remaining != 0);

    /* all halfwords matched (last comparison was equal, so C=1, V=0):
     * return the record's result halfword */
    R0 = LD16(R3 + 2);
    R1 = R6;
    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── custMenu_80298F4 ── */
/* First battle tutorial custMenu handler.
 *
 * The menu struct's state byte at +0x0C selects one of six tutorial checks.
 * Each check returns the text script index to show, or 0 when there is
 * nothing to show this frame. When a script is produced it is played through
 * the chatbox (TextScriptFirstBattleTutorials is a table of text archives,
 * one entry per tutorial state), then the menu is parked while the text
 * plays: the old value of the timer byte at +0x01 is saved to +0x0D, the
 * timer is restarted at 0x34 and the halfword counter at +0x02 is cleared.
 *
 * Returns 1 if a script was started, 0 otherwise (state 0xFF, states with no
 * check, or a check that returned nothing).
 *
 * Only the menu pointer (r5) is actually used; r4/r6/r7/r10 pass through
 * untouched, and r2/r3 are left exactly as the called game routines leave
 * them, matching the original.
 */

/* register form: calls battleTutorialCheckCustomScreen outside this file, which has no typed C interface yet */
static u32 tutorial_script_0(u32 state) {
    R0 = state;
    GUEST_CALL(battleTutorialCheckCustomScreen);
    return R0;
}

/* register form: calls battle_getPrepStatus_80299B6 outside this file, which has no typed C interface yet */
static u32 tutorial_script_1(u32 state) {
    R0 = state;
    GUEST_CALL(battle_getPrepStatus_80299B6);
    return R0;
}

/* register form: calls battleTutorial_getMessageId outside this file, which has no typed C interface yet */
static u32 tutorial_script_3(u32 state) {
    R0 = state;
    GUEST_CALL(battleTutorial_getMessageId);
    return R0;
}

/* register form: calls battle_tutorialCheckEmotionMsg outside this file, which has no typed C interface yet */
static u32 tutorial_script_4(u32 state) { /* tutorial states 4 and 5 share this check */
    R0 = state;
    GUEST_CALL(battle_tutorialCheckEmotionMsg);
    return R0;
}

/* register form: calls t1_checkCustomChipMsg outside this file, which has no typed C interface yet */
static u32 tutorial_script_6(u32 state) {
    R0 = state;
    GUEST_CALL(t1_checkCustomChipMsg);
    return R0;
}

/* register form: calls chatbox_runScript outside this file, which has no typed C interface yet */
static void run_chatbox_script(u32 archive, u32 script_idx) {
    R0 = archive;
    R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

static u32 first_battle_tutorial_menu(u32 menu) {
    u32 state = LD8(menu + 0xC);

    u32 script;
    switch (state) {
    case 0:  script = tutorial_script_0(state); break;
    case 1:  script = tutorial_script_1(state); break;
    case 3:  script = tutorial_script_3(state); break;
    case 4:
    case 5:  script = tutorial_script_4(state); break;
    case 6:  script = tutorial_script_6(state); break;
    default: return 0; /* 0xFF: no tutorial active; state 2 and others have no check */
    }

    if (script == 0)
        return 0; /* the check found nothing to say this frame */

    /* Pick the text archive for this tutorial state and start the script. */
    u32 archive = LD32(A_TextScriptFirstBattleTutorials + state * 4);
    run_chatbox_script(archive, script);

    /* Park the menu while the text is on screen. */
    ST8(menu + 0xD, LD8(menu + 1)); /* remember the previous timer value */
    ST8(menu + 1, 0x34);            /* restart the timer at 0x34 ticks */
    ST16(menu + 2, 0);              /* clear the halfword counter */
    return 1;
}

/* custMenu_80298F4(r5 = custMenu) -> r0; r2/r3 reflect the callees, r4-r7/r10 untouched */
Next custMenu_80298F4(void) {
    R0 = first_battle_tutorial_menu(R5);
    RETURN_TO_CALLER();
}
