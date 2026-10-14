/*
 * asm03_0_80299B6.c: readable functions of bn6f asm03_0.s, 0x080299B6-0x0802F2C8, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_getPrepStatus_80299B6 ── */
/* Returns the navi-cust battle preparation status for the given battle object pointer.
 * r3_out reports the final r3 value the original leaves behind: unchanged (r3_in) on the
 * early-exit paths, 0x48 after the mode-1 path, 0x49 after the mode-2+ path. */
typedef struct { u8 status; u32 r3_out; } PrepResult;

static PrepResult get_prep_status(u32 obj_ptr, u32 r3_in) {
    PrepResult r;

    u8 state = LD8(obj_ptr + 7);
    if (state != 0xA) {
        /* Early exit: only the first state byte was examined, r3 untouched. */
        r.status = 0;
        r.r3_out = r3_in;
        return r;
    }

    u8 mode = LD8(obj_ptr + 8);
    if (mode == 0) {
        r.status = 8;
        r.r3_out = r3_in;
        return r;
    }

    if (mode == 1) {
        /* Read the single slot byte at offset 0x48. */
        r.r3_out = 0x48;
        u8 slot0 = LD8(obj_ptr + 0x48);
        r.status = (slot0 != 0) ? 8 : 0xD;
        return r;
    }

    /* mode >= 2: read both slot bytes at 0x48 and 0x49. */
    r.r3_out = 0x49;
    u8 slot0 = LD8(obj_ptr + 0x48);
    u8 slot1 = LD8(obj_ptr + 0x49);

    if (slot0 == 0) {
        /* (0, 1) -> 0, otherwise (0, !=1) -> 0xD */
        r.status = (slot1 == 1) ? 0 : 0xD;
    } else if (slot0 == 1 && slot1 == 0) {
        r.status = 9;
    } else {
        r.status = 8;
    }
    return r;
}

/* battle_getPrepStatus_80299B6(r5 = battle object pointer) -> r0 = prep status, r3 = 0x48/0x49 read */
Next battle_getPrepStatus_80299B6(void) {
    PrepResult r = get_prep_status(R5, R3);
    R0 = r.status;
    R3 = r.r3_out;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── firstBattleTutorial_step_8029A56 ── */
/* register form: calls battle_getObjectStatusCode_8029AAA outside this file, which has no typed C interface yet */
static u32 sub_8029AAA_register_form(void) {
    GUEST_CALL(battle_getObjectStatusCode_8029AAA);
    return R0;
}

/* register form: calls entity_getActionIndex_8029ADA outside this file, which has no typed C interface yet */
static u32 sub_8029ADA_register_form(void) {
    GUEST_CALL(entity_getActionIndex_8029ADA);
    return R0;
}

/* register form: calls sub_8029B16 outside this file, which has no typed C interface yet */
static u32 sub_8029B16_register_form(void) {
    GUEST_CALL(sub_8029B16);
    return R0;
}

/* register form: calls chatbox_runScript outside this file, which has no typed C interface yet */
static void chatbox_runScript_register_form(u32 script_archive, u32 script_idx) {
    R0 = script_archive;
    R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

/* Drive one step of the first-battle tutorial chatbox.
 * Reads the driver state at (obj + 0xC). When the state is 0/1/3, the matching
 * predicate (battle_getObjectStatusCode_8029AAA/ADA/B16) is consulted; a non-zero result runs the
 * tutorial script indexed by `state` from TextScriptFirstBattleTutorials, then
 * stores the current chatbox state byte into offset 0xD, sets the new state to
 * 0x34 and clears the timer at offset 0x2. */
static void driver_step(u32 obj) {
    u32 step = LD8(obj + 0xC);

    /* 0xFF (idle) and any unhandled state: nothing to do. */
    if (step == 0xFF)
        return;

    u32 next_step;
    switch (step) {
    case 0:  next_step = sub_8029AAA_register_form(); break;
    case 1:  next_step = sub_8029ADA_register_form(); break;
    case 3:  next_step = sub_8029B16_register_form(); break;
    default: return;
    }

    if (next_step == 0)
        return;

    /* Look up the script archive pointer, then run the script. */
    u32 archive_entry = LD32(A_TextScriptFirstBattleTutorials + (step << 2));
    chatbox_runScript_register_form(archive_entry, next_step);

    /* Snapshot the chatbox state byte, set new state to 0x34, clear timer. */
    u8 prev_state = LD8(obj + 1);
    ST8(obj + 0xD, prev_state);
    ST8(obj + 1, 0x34);
    ST16(obj + 2, 0);
}

Next firstBattleTutorial_step_8029A56(void) {
    driver_step(R5);
    RETURN_TO_CALLER();
}

/* ── battle_getObjectStatusCode_8029AAA ── */
/* Compute a status code for a battle object based on its mode byte (+7) and sub-state byte (+8).
 * Mode 0 or 2 → depends on sub-state (1→9, else→14).
 * Mode 10 → 16.
 * Mode >2 (signed) → 0; otherwise → 22. */
static u32 get_battle_object_status_code(u32 obj_ptr) {
    u8 mode = (u8)LD8(obj_ptr + 7);
    u8 sub_state = (u8)LD8(obj_ptr + 8);

    if (mode == 0 || mode == 2)
        return (sub_state == 1) ? 9 : 0xE;

    if (mode == 0xA)
        return 0x10;

    /* mode is some other value: check if sub_state > 2 (signed) */
    return ((s32)sub_state > 2) ? 0 : 0x16;
}

/* battle_getObjectStatusCode_8029AAA(r5 = battle object ptr) -> r0 */
Next battle_getObjectStatusCode_8029AAA(void) {
    R0 = get_battle_object_status_code(R5);
    RETURN_TO_CALLER();
}

/* ── entity_getActionIndex_8029ADA ── */
/* entity_getActionIndex_8029ADA: pick an animation/action index for an entity.
 *
 * Inputs:  r5 = pointer to the entity struct (reads bytes at offsets 7 and 8).
 * Outputs: r0 = the chosen action index.
 *
 * The entity's state at byte 7 selects a major branch; byte 8 then refines the
 * choice. Anything that doesn't match a known case falls through to 0. */
Next entity_getActionIndex_8029ADA(void) {
    u8 state  = LD8(R5 + 7);
    u8 detail = LD8(R5 + 8);
    u32 result;

    switch (state) {
    case 0:
        /* state 0 only maps to action 5 when detail == 1, otherwise fall through. */
        result = (detail == 1) ? 5 : 0;
        break;

    case 1:
        if (detail == 2)        result = 0xA;
        else if (detail < 2)    result = 6;
        else                    result = 0;
        break;

    case 0xA:
        result = 0xF;
        break;

    default:
        /* Other primary states: action 7 if detail < 2, otherwise 0. */
        result = (detail < 2) ? 7 : 0;
        break;
    }

    R0 = result;
    /* Return preserves the flags the last comparison left. The default branch
     * ends on the "detail < 2" check; every other branch ends on a MOV of a
     * small positive constant. Set NZ to match the final value returned. */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battleTutorial_first_checkTextStep_8029B1C ── */
/*
 * First-battle tutorial chatbox trigger (r5 = tutorial work struct):
 *   +0x01  state/timer byte
 *   +0x02  u16 timer
 *   +0x0C  current tutorial step
 *   +0x0D  saved copy of the state byte, written when text starts
 *
 * Depending on the current step, a step checker decides whether a text
 * script should play now and returns its index (0 = nothing to show).
 * When a script plays, the task is handed over to its post-text wait
 * state and 1 is returned; otherwise 0.
 */

/* register form: calls sub_8029B86 outside this file, which has no typed C interface yet */
static u32 step0_script_index(void) {
    GUEST_CALL(sub_8029B86);
    return R0;
}

/* register form: calls sub_8029BAC outside this file, which has no typed C interface yet */
static u32 step1_script_index(void) {
    GUEST_CALL(sub_8029BAC);
    return R0;
}

/* register form: calls sub_8029BD6 outside this file, which has no typed C interface yet */
static u32 step3_4_script_index(void) {
    GUEST_CALL(sub_8029BD6);
    return R0;
}

/* register form: calls sub_8029BE6 outside this file, which has no typed C interface yet */
static u32 step6_script_index(void) {
    GUEST_CALL(sub_8029BE6);
    return R0;
}

/* register form: calls chatbox_runScript outside this file, which has no typed C interface yet */
static void chatbox_show_script(u32 archive, u32 script_idx) {
    R0 = archive;
    R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

static u32 tutorial_text_update(u32 work)
{
    u32 step = LD8(work + 0xC);
    u32 script;

    switch (step) {
    case 0: script = step0_script_index();   break;
    case 1: script = step1_script_index();   break;
    case 3:
    case 4: script = step3_4_script_index(); break;
    case 6: script = step6_script_index();   break;
    default:
        /* Step 0xFF (tutorial finished) and unknown steps have no text;
         * the original branches to a shared return-0 epilogue here. */
        return 0;
    }

    if (script == 0)
        return 0;

    /* The table at TextScriptFirstBattleTutorials holds one text archive
     * per step; the checker picked the script inside that archive. */
    chatbox_show_script(LD32(A_TextScriptFirstBattleTutorials + step * 4), script);

    /* Save the old state byte and put the task into its post-text wait. */
    ST8(work + 0xD, LD8(work + 1));
    ST8(work + 1, 0x34);
    ST16(work + 2, 0);
    return 1;
}

Next battleTutorial_first_checkTextStep_8029B1C(void) {
    R0 = tutorial_text_update(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8029D80 ── */
/* sub_8029D80 (asm03_0.s:7860): fixed-argument wrapper around call_sub_3005EBA.
 * It loads the constant argument set (0xF, 4, 3, 0, 7, 2) into r0-r5, makes the
 * call, and returns. The callee's results in r1-r3 pass through to our caller,
 * while r4/r5 — which the callee is free to clobber — are restored to the
 * values sub_8029D80 itself received (the original does this via push/pop). */
Next sub_8029D80(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    R0 = 0xF;
    R1 = 4;
    R2 = 3;
    R3 = 0;
    R4 = 7;
    R5 = 2;

    GUEST_CALL(call_sub_3005EBA);

    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── countUnmaskedEventFlags_8029EC8 ── */
/* countUnmaskedEventFlags_8029EC8:
 * Counts how many of the five event flags listed in the table at byte_8029F4C
 * are currently set AND not masked out in the state word pointed to by
 * dword_20349A0 (bit 5 + index, i.e. bits 5..9 for indices 0..4).
 * A flag counts only if it is set and its mask bit is still clear.
 * Returns the count in r0. */
Next countUnmaskedEventFlags_8029EC8(void) {
    u32 count = 0;

    for (u32 i = 0; i < 5; i++) {
        u32 flag = LD32(A_byte_8029F4C + i * 4);

        /* TestEventFlag sets Z=1 when the flag is clear. */
        R0 = flag;
        GUEST_CALL(TestEventFlag);
        if (ZERO_FLAG())
            continue; /* flag not set: doesn't count */

        u32 maskBits = LD32(A_dword_20349A0);
        u32 maskBit = (1u << i) << 5;
        if ((maskBits & maskBit) == 0)
            count++; /* set and not yet masked out */
    }

    R0 = count;
    RETURN_TO_CALLER();
}

/* ── sub_8029EF8 ── */
/* sub_8029EF8: Build the list of selectable candidates into the caller's work area.
 *
 * Five candidates (index 0..4) are considered.  Candidate i is listed only when
 *  - its story event flag is set (flags 0xE7..0xEB, table at byte_8029F4C),
 *  - bit (5 + i) of the mask word at dword_20349A0 is clear, and
 *  - PET navi stats byte 0x17 does not name it as the active one (it reads 6 + i).
 * Each listed candidate writes its index to work[0x50 + n] and 0 to work[0x55 + n].
 * Returns how many candidates were listed.
 */

#define CANDIDATE_COUNT    5     /* one slot per candidate, indices 0..4 */

#define EVENT_FLAG_TABLE   A_byte_8029F4C  /* ROM table of the candidates' event flags */
#define EXCLUDE_MASK       A_dword_20349A0 /* RAM word: bit 5+index set = candidate unavailable */
#define EXCLUDE_MASK_BIT   5
#define ACTIVE_STATS_BYTE  0x17  /* PET navi stats byte reporting the active candidate */
#define ACTIVE_ID_BASE     6     /* ...as 6+index, i.e. 6..10 for candidates 0..4 */
#define LISTED_ID_OFFSET   0x50  /* work[n]: index of the n-th listed candidate */
#define LISTED_ZERO_OFFSET 0x55  /* work[n]: parallel byte, always cleared to 0 */

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();         /* TestEventFlag sets Z when the flag is clear */
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u8 pet_navi_stats_byte(u32 navi, u32 stat) {
    R0 = navi;
    R1 = stat;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (u8)R0;
}

static u32 list_selectable_candidates(u32 work) {
    u32 listed = 0;

    for (u32 i = 0; i < CANDIDATE_COUNT; i++) {
        if (!event_flag_set((u16)LD32(EVENT_FLAG_TABLE + i * 4)))
            continue;                                   /* not unlocked yet */
        if (LD32(EXCLUDE_MASK) & (1u << (EXCLUDE_MASK_BIT + i)))
            continue;                                   /* masked out */
        if (pet_navi_stats_byte(0, ACTIVE_STATS_BYTE) == ACTIVE_ID_BASE + i)
            continue;                                   /* skip the active candidate */

        ST8(work + LISTED_ID_OFFSET + listed, i);
        ST8(work + LISTED_ZERO_OFFSET + listed, 0);
        listed++;
    }

    return listed;
}

Next sub_8029EF8(void) {
    R0 = list_selectable_candidates(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8029F70 ── */
/* sub_8029F70(r5 = battle-related object):
 * Decides whether some battle condition holds and returns the answer in R0
 * (1 = condition met, 0 = not met). R4 is preserved, as in the original.
 *
 * The condition is met outright in battle mode 8. In modes 0, 0xA and 0xB it
 * is met unless one of the following "veto" checks fires:
 *   - battle effects bit 0x200000 is active,
 *   - event flag 163 (0x63) is set and byte [r5+0x10] is nonzero,
 *   - battle flag 0x40 is set.
 * In any other battle mode the condition is never met. */
Next sub_8029F70(void) {
    u32 result = 0;

    GUEST_CALL(GetBattleMode);
    u32 mode = R0;

    if (mode == 8) {
        result = 1;
    } else if (mode == 0 || mode == 0xA || mode == 0xB) {
        GUEST_CALL(GetBattleEffects);
        if ((R0 & 0x200000) == 0) {
            /* TestEventFlagFromImmediate(EVENT_163): the caller passes the
             * flag id in R1 (R0 = 1, as in the original). */
            R0 = 1;
            R1 = 0x63; /* EVENT_163 */
            GUEST_CALL(TestEventFlagFromImmediate);
            if (!ZERO_FLAG()) {
                /* Event flag 163 is set: require [r5+0x10] to be clear. */
                if (LD8(R5 + 0x10) == 0)
                    result = 1;
            } else {
                /* Event flag 163 is clear: require battle flag 0x40 clear. */
                GUEST_CALL(TestBattleFlag_0x40);
                if (ZERO_FLAG())
                    result = 1;
            }
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── checkBattleEventGate_8029FB4 ── */
/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 get_battle_mode_sub_8029FB4(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8029FB4(u32 group, u32 index) {
    R0 = group;
    R1 = index;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* Z clear means the event flag IS set */
}

/* register form: calls TestBattleFlag_0x40 outside this file, which has no typed C interface yet */
static bool battle_flag_0x40_set(void) {
    GUEST_CALL(TestBattleFlag_0x40);
    return !ZERO_FLAG();
}

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 get_battle_effects_sub_8029FB4(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

#define EFFECT_BIT_0x200000 0x200000

/* Decide whether an overworld event gate is open during a battle.
 * Returns 1 if the battle is in mode 5 (scripted), or if all of:
 *   - the byte at battle-state+0x10 is zero (no active sub-state),
 *   - EVENT_163 is NOT set,
 *   - battle flag 0x40 is NOT set,
 *   - no 0x200000 effect is active,
 *   - EVENT_E0 IS set.
 * Returns 0 otherwise. */
static u32 check_event_gate(u32 battle_state_ptr) {
    if (get_battle_mode_sub_8029FB4() == 5)
        return 1;

    if (LD8(battle_state_ptr + 0x10) != 0)
        return 0;

    if (event_flag_set_sub_8029FB4(1, 0x63))          /* EVENT_163 */
        return 0;

    if (battle_flag_0x40_set())
        return 0;

    if (get_battle_effects_sub_8029FB4() & EFFECT_BIT_0x200000)
        return 0;

    if (!event_flag_set_sub_8029FB4(0, 0xE0))         /* EVENT_E0 */
        return 0;

    return 1;
}

/* checkBattleEventGate_8029FB4(r5 = battle state pointer) -> r0 = gate open? */
Next checkBattleEventGate_8029FB4(void) {
    R0 = check_event_gate(R5);
    RETURN_TO_CALLER();
}

/* ── chipIDIs13f_8029FF4 ── */
/* chipIDIs13f_8029FF4(r0 = chip ID): return 1 in r0 if the ID matches either
 * entry of the two-word chip ID table at dword_802A02C, else 0.
 * All other registers (r1-r7) are preserved. */
Next chipIDIs13f_8029FF4(void) {
    u32 chipID = R0;
    /* r1-r7 must survive the call */
    u32 inR1 = R1, inR2 = R2, inR3 = R3, inR4 = R4, inR5 = R5, inR6 = R6, inR7 = R7;

    u32 table = A_dword_802A02C;
    u32 isMatch = (chipID == LD32(table) || chipID == LD32(table + 4));

    R0 = isMatch;
    R1 = inR1;
    R2 = inR2;
    R3 = inR3;
    R4 = inR4;
    R5 = inR5;
    R6 = inR6;
    R7 = inR7;
    RETURN_TO_CALLER();
}

/* ── sub_802A00C ── */
/* sub_802A00C(r0 = id):
 * Returns r0 = 1 if the low 9 bits of r0 match either of the two words stored
 * at dword_802A02C / dword_802A030 (both 0x13F in this build), else r0 = 0.
 * All other registers are preserved.
 * Flag note: the final "mov r0, r3" is the flag-setting Thumb encoding
 * (LSLS r0, r3, #0): N/Z come from the result, and C/V end up 0. */
Next sub_802A00C(void) {
    u32 id = (R0 << 23) >> 23; /* keep only the low 9 bits */
    u32 table = A_dword_802A02C;

    u32 match = (id == LD32(table)) || (id == LD32(table + 4));

    R0 = match;
    SET_FLAGS_ADD(R0, 0); /* N/Z from result, C = 0, V = 0 */
    RETURN_TO_CALLER();
}

/* ── battle_ClearPendingEventFlag_802A0EC ── */
/* battle_ClearPendingEventFlag_802A0EC: clear the byte flag at byte_20352CC (the battle-related flag in
 * the eStruct2035280 work area), signalling that the pending event is done.
 * Returns r1 = 0, r2 = 0x4C, and flags set from the MOV r2, #0x4C. */
Next battle_ClearPendingEventFlag_802A0EC(void) {
    R0 = A_eStruct2035280;
    R1 = 0;
    R2 = 0x4C; /* byte_20352CC - 0x2035280 */
    SET_FLAGS_NZ(R2);
    ST8(R0 + R2, R1);
    RETURN_TO_CALLER();
}

/* ── battle_DispatchPendingEvent_802A0F8 ── */
/* battle_DispatchPendingEvent_802A0F8: check for a pending battle event and dispatch it.
 *
 * Reads the pending-event byte at byte_20352CC (field 0x4C of eStruct2035280).
 * If an event is pending, dispatches event flags 0x4000; afterwards the
 * pending-event state is always cleared via battle_ClearPendingEventFlag. */
Next battle_DispatchPendingEvent_802A0F8(void) {
    R0 = A_eStruct2035280;
    R1 = 0;
    R2 = 0x4C;                      /* byte_20352CC - eStruct2035280 */
    R1 = LD8(R0 + R2);              /* pending event byte */

    if (R1 != 0) {
        /* Event pending: dispatch it. Flags afterwards are whatever the
         * dispatch routine leaves. */
        R0 = 0x4000;
        GUEST_CALL(dispatch_801DACC);
    } else {
        /* No event: nothing to dispatch. Leave the flags as the original's
         * "tst r1, r1" (with r1 == 0) did. */
        SET_FLAGS_NZ(R1);
    }

    /* Clear the pending-event state before returning. On the dispatch path
     * R1/R2 hold whatever dispatch_801DACC left, exactly as in the original. */
    GUEST_CALL(battle_ClearPendingEventFlag_802A0EC);

    RETURN_TO_CALLER();
}

/* ── battle_setUnk20349A5Flag_802A210 ── */
/* battle_setUnk20349A5Flag_802A210(r5 = battle state):
 * If state->Unk_15 is nonzero, store 0xFF to the byte at dword_20349A0+5
 * (byte_20349A5) and leave that address in r3. The flags left behind are
 * those of the CMP of Unk_15 against 0 (the function's only flag-setting op). */
Next battle_setUnk20349A5Flag_802A210(void) {
    u32 unk15 = LD8(R5 + oBattleState_Unk_15);

    SET_FLAGS_CMP(unk15, 0);
    if (unk15 != 0) {
        R3 = A_dword_20349A0;
        ST8(R3 + 5, 0xFF);
    }
    RETURN_TO_CALLER();
}

/* ── battleObject_computePuppeteerSwitchElement_802A220 ── */
/* battleObject_computePuppeteerSwitchElement_802A220(r2, r5, r10, lr):
 * Return a battle-related selection value (often used as the "selected panel index").
 * r4 starts at 0xFF; many early-returns keep r4 unchanged.
 * Reads toolkit battle state byte 0xD and queries stats byte 0x29, plus battle flags,
 * battle mode, and a panel state struct at dword_20349A0. */
Next battleObject_computePuppeteerSwitchElement_802A220(void) {
    u32 saved_r10 = R10;
    u32 saved_r5 = R5;
    u32 saved_lr = LR;

    /* default return = 0xFF */
    u32 result = 0xFF;

    /* if event flag 0x163 (index 99) is set, bail out */
    u32 has_flag = 0;
    SET_FLAGS_NZ(1 & 0);  /* flag setting for the ldr/load path is irrelevant; callee updates nz */
    R0 = 1;
    R1 = 99;
    GUEST_CALL(TestEventFlagFromImmediate);
    /* TestEventFlagFromImmediate returns !zf -> nz set means present */
    if (!ZERO_FLAG()) {
        goto done;
    }

    /* if battle effects has 0x200000 bit, bail out */
    GUEST_CALL(GetBattleEffects);
    if (R0 & 0x200000) {
        goto done;
    }

    /* battle mode must be 0, 0xA, or 0xB */
    GUEST_CALL(GetBattleMode);
    u32 mode = R0;
    if (mode != 0 && mode != 0xA && mode != 0xB) {
        goto done;
    }

    /* get the player navi's stat byte 0x29 from the battle state */
    u32 battle_state = LD32(saved_r10 + oToolkit_BattleStatePtr);
    u32 navi_id = LD8(battle_state + 0xD);
    R0 = navi_id;
    R1 = 0x29;
    GUEST_CALL(GetBattleNaviStatsByte);
    u32 stat = R0;
    /* require stat < 1 (i.e. == 0) OR stat > 0xB; in practice == 0 */
    if ((s32)stat >= 1 && (s32)stat <= 0xB) {
        goto done;
    }

    /* read panel state struct dword_20349A0 */
    u32 panel = A_dword_20349A0;
    u32 p_byte5 = LD8(panel + 5);
    if (p_byte5 == 0xFF) {
        goto done;
    }

    /* sub_8120B54() returns a candidate slot id */
    GUEST_CALL(sub_8120B54);
    u32 slot = R0;
    if (slot == 0xFF) {
        goto done;
    }

    /* bail if slot matches the 0x3A word in r5 */
    u32 r5_word_3a = LD16(saved_r5 + 0x3A);
    if (slot == r5_word_3a) {
        goto done;
    }

    /* bail if the slot's bit is set in panel+0xE word */
    u32 p_word_e = LD16(panel + 0xE);
    if (p_word_e & (1u << slot)) {
        goto done;
    }

    /* check battle effects bit 8 */
    GUEST_CALL(GetBattleEffects);
    if (!(R0 & 8)) {
        /* path: bit clear -> result bit, then unconditional fall-through picks slot */
        /* The original sets r0=1, compares with 0, and falls through to r4=slot on !=0.
         * r0 (=1) != 0, so the branch to done is NOT taken. */
        goto pick_slot;
    }

    /* bit 8 set: require panel+5 < 2, then fall through to pick_slot */
    if (p_byte5 >= 2) {
        goto done;
    }
    /* r0=1; r0==0 branch not taken; fall through to pick_slot */

pick_slot:
    result = slot;

done:
    R0 = result;
    R1 = R1;        /* unchanged */
    R2 = R2;        /* unchanged */
    R3 = R3;        /* unchanged */
    R4 = R4;        /* unchanged */
    R5 = saved_r5;  /* preserved */
    R6 = R6;        /* unchanged */
    R7 = R7;        /* unchanged */
    R10 = saved_r10;
    LR = saved_lr;
    RETURN_TO_CALLER();
}

/* ── fadeScreenAndAdvanceState_802A2E8 ── */
/* fadeScreenAndAdvanceState_802A2E8: (self: *S20364C0 in r5) -> ()
 * Check some condition (chipSelect_selectedSlotChipHasAttr20_802A394); when it holds, start the 0x54/0x5C
 * screen fade, clear the state byte at +13 and set the jump offset at +12
 * to 4. Leaves N/Z from the final MOV (or from the initial comparison when
 * nothing happened); C/V come from the last callee on the active path. */
Next fadeScreenAndAdvanceState_802A2E8(void) {
    GUEST_CALL(chipSelect_selectedSlotChipHasAttr20_802A394);

    if (R0 != 0) {
        R0 = 0x54;
        R1 = 0xA;
        GUEST_CALL(SetScreenFade);

        R0 = 0x5C;
        R1 = 0xA;
        GUEST_CALL(loc_8006274);

        ST8(R5 + oS20364C0_Unk_13, 0);

        R0 = 4;
        SET_FLAGS_NZ(R0);
        ST8(R5 + oS20364C0_JumpOffset12, 4);
    } else {
        /* flags as left by the CMP r0, #0 before the branch */
        SET_FLAGS_CMP(R0, 0);
    }

    RETURN_TO_CALLER();
}

/* ── chipSelect_selectedSlotChipHasAttr20_802A394 ── */
/* chipSelect_selectedSlotChipHasAttr20_802A394(r5 = chip menu/selection state):
 * Only meaningful in specific menu modes (state byte at r5+1 == 0x18 or 4).
 * In those modes, looks up the currently selected active-chip slot (index at
 * r5+7); if that slot entry is empty, resolves the chip it references and
 * tests its ChipData attribute byte for flag 0x20.
 * Returns R0 = 1 if the flag is set, 0 otherwise.
 * R1 on exit mirrors the original: the slot-busy byte, 0x20 on the flag-test
 * path, or the incoming value when the mode check fails. */
Next chipSelect_selectedSlotChipHasAttr20_802A394(void) {
    u32 result = 0;

    u32 state = LD8(R5 + 1);
    if (state == 0x18 || state == 4) {
        R0 = LD8(R5 + 7);
        GUEST_CALL(getLocOfActiveChips_8027E1C);
        u32 slot = R0;

        R1 = LD8(slot);                 /* 0 = slot entry is empty */
        if (R1 == 0) {
            R0 = LD16(LD32(slot + 8));  /* chip reference from the entry */
            GUEST_CALL(getChipID_802A54E);
            u32 chipId = (R0 << 23) >> 23;  /* keep low 9 bits */

            R0 = chipId;
            GUEST_CALL(getChip8021DA8);
            R1 = 0x20;
            if (LD8(R0 + 9) & 0x20)     /* attribute flag 0x20 set */
                result = 1;
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sound_playCrossTransformationJingle_802A3CC ── */
/* sound_playCrossTransformationJingle_802A3CC: request the pair of sounds played when the Cross/Beast
 * transformation sequence starts (jingle 0x1F, then roar 0x16).
 * Both requests use the generic 0xFFFF/0x100 parameter pair.
 * r4-r7, r10 pass through unchanged; c/v come from the sound dispatcher. */
static void play_transformation_sound(u32 sound_id)
{
    R0 = sound_id;
    R1 = 0xFFFF;
    R2 = 0x80 + 0x80; /* 0x100 */
    GUEST_CALL(sound_dispatchSoundRequest_8000642);
}

Next sound_playCrossTransformationJingle_802A3CC(void) {
    play_transformation_sound(0x1F);
    play_transformation_sound(0x16);
    RETURN_TO_CALLER();
}

/* ── battle_updateBeastOutCounter_802A40C ── */
/* battle_updateBeastOutCounter_802A40C: compute the beast-out counter and store it to dest+6.
 * Reads battle mode, Navi stats, and global overflow to decide how many
 * turns the current Navi has left before a beast transformation expires. */

#define ADDR_BEAST_OVERFLOW_BYTE 0x20349B1u

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 guest_get_battle_mode(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls callPossiblyGetBattleEmotion_8015B54 outside this file, which has no typed C interface yet */
static u8 guest_get_transformation(u8 alliance, u32 battle_state) {
    R0 = alliance;
    R6 = battle_state;
    GUEST_CALL(callPossiblyGetBattleEmotion_8015B54);
    return (u8)R1;
}

/* register form: calls GetBattleNaviStatsByte outside this file, which has no typed C interface yet */
static u8 guest_get_battle_navi_stats_byte(u8 alliance, u16 stat_id) {
    R0 = alliance;
    R1 = stat_id;
    GUEST_CALL(GetBattleNaviStatsByte);
    return (u8)R0;
}

/* register form: calls battle_isBeastOverEnabled_802A5A0 outside this file, which has no typed C interface yet */
static bool guest_is_beast_over_enabled(void) {
    R4 = 0;
    GUEST_CALL(battle_isBeastOverEnabled_802A5A0);
    return R0 != 0;
}

/* Compute the beast-out counter byte and write it to [dest + 6].
 * toolkit: pointer to the Toolkit struct (contains BattleStatePtr).
 * dest:    pointer whose byte at offset 6 receives the computed counter. */
static void beast_update_out_counter(u32 toolkit, u32 dest) {
    u32 battle_state = LD32(toolkit + oToolkit_BattleStatePtr);

    /* Link/cross-fade modes (2,3,4,5,8): counter is always 5. */
    u32 mode = guest_get_battle_mode();
    if (mode == 2 || mode == 3 || mode == 4 || mode == 5 || mode == 8) {
        ST8(dest + 6, 5);
        return;
    }

    u8 alliance = LD8(battle_state + oBattleState_Unk_0d);

    /* Beast/transform forms 0xA and 0x16 bypass the "beast over" override. */
    u8 transformation = guest_get_transformation(alliance, battle_state);
    bool is_beast_form = (transformation == 0xA || transformation == 0x16);

    /* Base counter: Navi stat slot 0xA plus global overflow byte, clamped at 8. */
    u8 stats_a = guest_get_battle_navi_stats_byte(alliance, 0xA);
    u8 overflow_byte = LD8(ADDR_BEAST_OVERFLOW_BYTE);
    u32 total = stats_a + overflow_byte;

    u8 counter;
    s32 overflow;
    if ((s32)total > 8) {
        overflow = (s32)(total - 8);
        counter  = 8;
    } else {
        overflow = 0;
        counter  = (u8)total;
    }

    /* Non-beast form and beast-over mode active: force counter to 0xA,
     * reset overflow to the raw global byte. */
    if (!is_beast_form && guest_is_beast_over_enabled()) {
        counter  = 0xA;
        overflow = (s32)LD8(ADDR_BEAST_OVERFLOW_BYTE);
    }

    /* Mood-based drain: if Navi's agitation (stat 0x63) is nonzero and
     * damage taken >= agitation, shave remaining overflow from the counter. */
    u8 mood = guest_get_battle_navi_stats_byte(alliance, 0x63);
    if (mood != 0) {
        u8 damage = LD8(battle_state + oBattleState_Unk_07);
        if ((s32)damage >= (s32)mood) {
            s32 reduction = (s32)damage - (s32)mood + 1;
            overflow -= reduction;
            if (overflow < 0) {
                s32 adjusted = (s32)counter + overflow;
                counter = (adjusted < 2) ? 2 : (u8)adjusted;
            }
        }
    }

    ST8(dest + 6, counter);
}

/* battle_updateBeastOutCounter_802A40C(r5 = dest, r10 = toolkit) -> writes dest[6]; r4/r5/r6/r7 preserved */
Next battle_updateBeastOutCounter_802A40C(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    beast_update_out_counter(R10, saved_r5);
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── folder_tallyChipsByClass_802A4FC ── */
/* folder_tallyChipsByClass_802A4FC: tally the chips listed in the current folder by class.
 *
 * The folder is stored as a 0xFFFF-terminated list of chip IDs starting
 * 0x32 bytes into the record at byte_20366C0. For each entry, the chip's
 * class (ChipData+7; 0 = standard, 1 = mega, 2 = giga) is looked up via
 * getChip8021DA8 and the matching counter in dword_20367E0 is incremented.
 * If byte_20366C0 is 0xFF (no folder loaded) the routine does nothing.
 *
 * On return the V flag (and N/Z) reflects the terminating CMP, matching
 * the original's last flag-setting instruction on each exit path. */
Next folder_tallyChipsByClass_802A4FC(void) {
    /* The original pushes r4, r6 and r7 and pops them on the way out, so
     * these registers must be unchanged afterwards no matter how we exit
     * (the chip-data lookup may clobber them). */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 folderLoaded = LD8(A_byte_20366C0);
    SET_FLAGS_CMP(folderLoaded, 0xFF);
    if (folderLoaded != 0xFF) {
        u32 entry = A_byte_20366C0 + 0x32;   /* first folder slot (halfword) */
        u32 counters = A_dword_20367E0;      /* per-class tally bytes */

        for (;;) {
            u32 entryValue = LD16(entry);
            SET_FLAGS_CMP(entryValue, 0xFFFF);
            if (entryValue == 0xFFFF)        /* end-of-folder sentinel */
                break;

            u32 chipIndex = entryValue & 0x1FF;   /* low 9 bits = chip ID */
            R0 = chipIndex;
            GUEST_CALL(getChip8021DA8);

            u32 chipClass = LD8(R0 + 7);
            SET_FLAGS_CMP(chipClass, 2);
            if ((s32)chipClass <= 2)
                ST8(counters + chipClass, LD8(counters + chipClass) + 1);

            entry += 2;
        }
    }

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_getActorEmotion_802A564 ── */
/* battle_getActorEmotion_802A564: fetch the current battle emotion for the acting object.
 * r10 = toolkit; r5 = battle object. Loads the battle state's "unk_0d" byte
 * (the alliance/emotion selector) and dispatches on object field 0x15:
 * when it is zero, resolve the emotion (transformation/mood/beast-out side
 * outputs land in r0-r3); otherwise take the alternate lookup. Results are
 * whatever the callee leaves in r0-r3. */
Next battle_getActorEmotion_802A564(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);

    R0 = LD8(battleState + oBattleState_Unk_0d);
    if (LD8(R5 + 0x15) == 0)
        GUEST_CALL(callPossiblyGetBattleEmotion_8015B54);
    else
        GUEST_CALL(sub_8015B5C);

    RETURN_TO_CALLER();
}

/* ── battle_isBeastOverEnabled_802A5A0 ── */
/* battle_isBeastOverEnabled_802A5A0: check whether the "cross beast" state applies this battle.
 * Returns 1 in r0 when all of the following hold:
 *   - the byte at active-chip slot 8 is not 8,
 *   - the battle mode is none of 1, 6, 7 or 9 (special modes),
 *   - the navi stats byte 0x61 of the current battle state equals 1.
 * r4 is preserved (saved/restored by the original), r6 and r10 untouched. */
Next battle_isBeastOverEnabled_802A5A0(void) {
    u32 result = 0;

    R0 = 8;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    if (LD8(R0) != 8) {
        GUEST_CALL(GetBattleMode);
        u32 battleMode = R0;
        if (battleMode != 1 && battleMode != 6 && battleMode != 7 && battleMode != 9) {
            u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
            R0 = LD8(battleState + oBattleState_Unk_0d);
            R1 = 0x61;
            GUEST_CALL(GetBattleNaviStatsByte);
            if (R0 == 1)
                result = 1;
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── battle_checkActiveSlot8NaviStatReady_802A5DA ── */
/* battle_checkActiveSlot8NaviStatReady_802A5DA: check whether the currently selected chip in active slot 8
 * triggers a special navi state. Returns 1 in r0 if so, 0 otherwise.
 * The battle mode must be one of the regular battle modes (0, 5, 8, 10, 11),
 * and navi stats byte 0x60 (indexed by battle state field 0x0d) must be 1. */
Next battle_checkActiveSlot8NaviStatReady_802A5DA(void) {
    u32 result = 0;

    /* Pointer to active chip slot 8 */
    R0 = 8;
    GUEST_CALL(getLocOfActiveChips_8027E1C);

    /* Slot empty marker (0xFF-like value 8 at that offset) means "no" */
    if (LD8(R0) != 8) {
        R0 = 0;
        GUEST_CALL(GetBattleMode);
        u32 mode = R0;

        if (mode == 0 || mode == 5 || mode == 8 || mode == 0xA || mode == 0xB) {
            u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
            R0 = LD8(battleState + oBattleState_Unk_0d);
            R1 = 0x60;
            GUEST_CALL(GetBattleNaviStatsByte);

            if (R0 == 1)
                result = 1;
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── expireBattleFlag_802A646 ── */
/* If the battle-state flag at offset 0x44 is set and the counter at offset 0x45
 * is strictly less than the byte at r5+6, clear the flag. */
static void expire_battle_flag(u32 battle_state_ptr, u32 source) {
    if (LD8(battle_state_ptr + 0x44) == 0)
        return;
    if ((s32)LD8(battle_state_ptr + 0x45) >= (s32)LD8(source + 6))
        return;
    ST8(battle_state_ptr + 0x44, 0);
}

/* register form: r5, r10 and lr arrive in guest registers; the body dereferences
 * a guest struct pointer (battle state) and a guest object, which has no typed
 * C interface, so the typed helper takes raw guest addresses. */
Next expireBattleFlag_802A646(void) {
    expire_battle_flag(LD32(R10 + oToolkit_BattleStatePtr), R5);
    RETURN_TO_CALLER();
}

/* ── sub_802BDB4 ── */
/* sub_802BDB4: tail work after battle_checkTimerWindowSubstate_802C97E -- store the value 4 into the
 * word pointed to by r5 (likely a state/step counter being advanced). */
Next sub_802BDB4(void) {
    GUEST_CALL(battle_checkTimerWindowSubstate_802C97E);

    R0 = 4;
    ST32(R5, R0);
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── objStepFinishSetState4_802BE0C ── */
/* objStepFinishSetState4_802BE0C — finish step for the object in r5 (r10 = toolkit pointer):
 * calls call_sub_3005EBA with the fixed argument set (0, 0, 3, 0, 6, 2),
 * clears the render-info frame counter, calls battle_eStruct2035280_postEvent_801E0A0(0, 0), then
 * writes state 4 into the object's halfword field at +2.
 * Preserves the entry values of r4 and r5 for the caller; exits with
 * NZ flags from "MOV r0, #4" and r1/r2/r3/r10 as battle_eStruct2035280_postEvent_801E0A0 left them. */
Next objStepFinishSetState4_802BE0C(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 toolkit = R10;

    /* call_sub_3005EBA(0, 0, 3, 0, 6, 2) */
    R0 = 0;
    R1 = 0;
    R2 = 3;
    R3 = 0;
    R4 = 6;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    /* clear the render info frame counter: renderInfo = toolkit->renderInfo */
    u32 renderInfo = LD32(toolkit + 8 /* oToolkit_RenderInfoPtr */);
    ST16(renderInfo + oRenderInfo_Unk_18, 0);

    /* battle_eStruct2035280_postEvent_801E0A0(0, 0); r5 is live for the callee, so restore it first */
    R0 = 0;
    R1 = 0;
    R5 = saved_r5;
    GUEST_CALL(battle_eStruct2035280_postEvent_801E0A0);

    /* advance the object's state field to 4 (flags: N=0, Z=0) */
    R0 = 4;
    SET_FLAGS_NZ(4);
    ST16(saved_r5 + 2, 4);

    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── sub_802BE36 ── */
/* ----------------------------------------------------------------------
 * sub_802BE36 -- one per-frame tick of a battle object (r5 -> object).
 *
 * Object fields touched here:
 *   +0x01  u8   state     switched to 4 once the progress counter tops out
 *   +0x02  u16  timer     cleared at the same moment
 *   +0x04  u8   mode      mode 2 skips the per-frame updates below
 *   +0x06  s8   progress  stepped by 2 each tick, clamped at 3
 *   +0x08  u8   id        argument of sub_802C6EC
 *   +0x0e  u8   frame     second argument of sub_802C4E8
 *   +0x1c  ptr  component first argument of sub_802C4E8
 * ---------------------------------------------------------------------- */

/* register form: calls sub_802C4E8 outside this file, which has no typed C interface yet */
static void run_component_update(u32 component, u32 frame) {
    R0 = component;
    R1 = frame;
    GUEST_CALL(sub_802C4E8);
}

/* register form: calls sub_802C6EC outside this file, which has no typed C interface yet */
static void run_display_update(u32 id) {
    R0 = id;
    GUEST_CALL(sub_802C6EC);
}

/* Advance the object's progress counter by 2.  When it reaches 3 the
 * sequence is finished: the counter is pinned at 3, the state moves on
 * to 4 and the timer restarts.  Unless the object is in mode 2 (frozen),
 * its component and display are refreshed afterwards. */
static void tick_object(u32 obj) {
    /* register form: the callees bake the guest stack pointer into r6 (an
     * output the caller reads), so SP must sit exactly where the original's
     * push {lr} leaves it -- 4 below entry, holding the saved return address
     * -- for the whole body; this guest-stack effect cannot be written in
     * plain C. */
    SP -= 4;
    ST32(SP, LR);

    s32 progress = (s8)LD8(obj + 6) + 2;
    ST8(obj + 6, progress);
    if (progress >= 3) {
        ST8(obj + 6, 3);      /* pin the counter at its maximum */
        ST8(obj + 1, 4);      /* switch to the follow-up state */
        ST16(obj + 2, 0);     /* restart the halfword timer */
    }

    if (LD8(obj + 4) != 2) {  /* mode 2: no per-frame updates */
        run_component_update(LD32(obj + 0x1c), LD8(obj + 0xe));
        run_display_update(LD8(obj + 8));
    }

    SP += 4;                  /* the original's pop {pc} */
}

/* sub_802BE36(r5 = object) -> r5 unchanged; r6/r7 as the last callee leaves them */
Next sub_802BE36(void) {
    tick_object(R5);
    RETURN_TO_CALLER();
}

/* ── sub_802BE90 ── */
/* Register-form helper for the one call no generated wrapper covers: the
 * battle object's state handler is reached through the four-entry jump table
 * at off_802BEA4, so its address is only known at runtime. This reproduces
 * the original sequence
 *
 *     push {lr}              ; caller's return address
 *     ldr  r1, =off_802BEA4
 *     ldrb r0, [r5,#2]
 *     ldr  r1, [r1,r0]
 *     mov  lr, pc
 *     bx   r1                ; call table[state]
 *     pop  {pc}
 *
 * register for register. The four stack bytes the push reserves matter: the
 * handlers compute frame-relative addresses, so SP must sit 4 below the entry
 * value for the duration of the call. The handler receives r0 = state byte,
 * r1 = handler address, and every other register exactly as the caller left
 * it; GUEST_CALL_ADDR performs the indirect call itself, including the
 * mov lr,pc return-point bookkeeping. Afterwards SP is popped back and the
 * entry lr is restored so RETURN_TO_CALLER() sends the results to the
 * original caller — the handler's r5-r12 results are already sitting in the
 * guest registers where the caller expects them.
 */
/* register form: indirect call through a runtime jump-table address, plus the
 * caller's push {lr}/pop {pc} around it; cannot be expressed as a typed call */
static void call_state_handler_sub_802BE90(u32 handler_addr, u32 state_byte) {
    u32 entry_lr = LR;

    SP -= 4;
    ST32(SP, entry_lr);
    R0 = state_byte;
    R1 = handler_addr;
    GUEST_CALL_ADDR(handler_addr);
    SP += 4;
    LR = entry_lr;
}

/* Send a battle object to the behavior routine for its current state: the
 * state byte at [r5+2] indexes the handler table at off_802BEA4. The handler
 * runs with the caller's register state, and whatever it leaves in r5-r12 is
 * what this function's caller sees — the dispatch is a tail-call, so nothing
 * here computes results of its own. */
static void dispatch_battle_object_state_sub_802BE90(u32 object) {
    u32 state_byte = LD8(object + 2);
    u32 handler_addr = LD32(A_off_802BEA4 + state_byte);

    call_state_handler_sub_802BE90(handler_addr, state_byte);
}

/* sub_802BE90: dispatch the battle object in r5 to its per-state handler.
 * The other input registers (r2, r4, r6, r7, r10) are live only because the
 * handler consumes them; they pass straight through the untouched guest
 * registers, as do r3/r8/r9/r11/r12. Outputs r5-r12 are left behind by the
 * handler itself, so the adapter writes nothing. */
Next sub_802BE90(void) {
    dispatch_battle_object_state_sub_802BE90(R5);
    RETURN_TO_CALLER();
}

/* ── sub_802BEB4 ── */
/* register form: the callee is reached through the runtime jump table at
 * A_off_802BEC8 (ldr r1,[table,type]; bx r1), so the target address only
 * exists at runtime and no typed wrapper covers the call. The helper sets
 * r0/r1 to the original's register state at the bx (r0 = type byte from
 * [object+3], r1 = the table entry just loaded) and lets every other guest
 * register — and the callee's outputs — pass straight through. */
static void call_jump_table_target(u32 type, u32 target) {
    R0 = type;
    R1 = target;
    GUEST_CALL_ADDR(target);
}

/* Dispatch a battle object's per-type handler: the byte at [object+3]
 * selects an entry (a Thumb address) from the jump table at A_off_802BEC8,
 * which is then called with the original's register state. */
static void sub_802BEB4_dispatch(u32 object_ptr) {
    u32 type = LD8(object_ptr + 3);
    u32 target = LD32(A_off_802BEC8 + type);
    call_jump_table_target(type, target);
}

/* sub_802BEB4(r5 = object pointer): jump-table trampoline to one of the three
 * per-type handlers. The push/pop of lr around the call only preserves the
 * caller's return address, which the call mechanism already handles; all
 * result registers are whatever the callee leaves behind, so the adapter
 * computes nothing itself. */
Next sub_802BEB4(void) {
    sub_802BEB4_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── custMenu_handleInputQueuePop_802BED4 ── */
/* custMenu_handleInputQueuePop_802BED4(r5 = custom-menu object):
 * Pop the next halfword from the menu input queue and act on it.
 *  - Queue empty (value 0): run the "back out" path — set the jump offset
 *    (r5+1) to 8, which routes the next state step to scrollStrip_update_8026BF4 via the
 *    menu main handler, and clear the halfword at r5+2.
 *  - Otherwise: store the popped value into Unk_10, choose the hold timer
 *    for Unk_0b (1 frame when the byte at Unk_0c is set, otherwise 30
 *    frames), and set the byte at r5+3 to 4 to advance the menu state.
 * Final register/flag state (per path): R0 = 0 and nz=0 on the empty path,
 * R0 = 4 and nz=4 on the consumed path; R1 holds the Unk_0c byte there and
 * is left as queue_popHalfword_802C8D4 returned it otherwise. c/v untouched. */
Next custMenu_handleInputQueuePop_802BED4(void) {
    GUEST_CALL(queue_popHalfword_802C8D4);
    u32 value = R0;

    if (value == 0) {
        /* nothing queued: hand control back to the state dispatcher */
        ST8(R5 + 1, 8);         /* oS20364C0_JumpOffset01 */
        ST16(R5 + 2, 0);        /* oS20364C0_Unk_02 */
        R0 = 0;
        SET_FLAGS_NZ(0);
    } else {
        ST16(R5 + oS20364C0_Unk_10, value);

        /* short confirmation hold if Unk_0c says so, otherwise a long one */
        R1 = LD8(R5 + oS20364C0_Unk_0c);
        u32 hold = (R1 != 0) ? 1 : 0x1E;
        ST8(R5 + oS20364C0_Unk_0b, hold);

        R0 = 4;
        ST8(R5 + 3, 4);         /* oS20364C0_Unk_03 */
        SET_FLAGS_NZ(4);
    }
    RETURN_TO_CALLER();
}

/* ── object_countdownTick_802BEFC ── */
/* object_countdownTick_802BEFC(r5 = struct with a countdown byte at +0xB and a state byte at +3):
 * decrement the countdown at [r5+0xB]; when it reaches zero, set [r5+3] to 8
 * (advance to state 8). Leaves r0 = the decremented counter, or 8 if it expired,
 * with flags from the corresponding last flag-setting instruction. */
Next object_countdownTick_802BEFC(void) {
    u32 count = LD8(R5 + 0xB);
    u32 next  = count - 1;

    ST8(R5 + 0xB, next);
    SET_FLAGS_CMP(count, 1); /* flags of SUBS r0, #1 */

    if (next != 0) {
        R0 = next;           /* still counting down */
        RETURN_TO_CALLER();
    }

    R0 = 8;                  /* timer expired */
    SET_FLAGS_NZ(8);         /* flags of MOVS r0, #8 */
    ST8(R5 + 3, 8);
    RETURN_TO_CALLER();
}

/* ── battle_pickObjectAnimState_802BF0C ── */
/* battle_pickObjectAnimState_802BF0C(r5 = battle object):
 * Pick an animation/state index for the object and hand it to battle_drawChipSelectRowTilemapBlock_802C810.
 * If either of the two flag halfwords next to dword_2036820 has bit 0 set,
 * the index comes from the object's byte at +0xC (capped: >1 becomes 0xC,
 * otherwise 4) and is also stored into the object's halfword at +2.
 * Otherwise the index alternates 0/1 with bit 3 of the current frame count.
 * R7 passed to the callee is the flags base +8 on the first path, or the
 * current-frame pointer on the second. R5 and R10 are preserved. */
Next battle_pickObjectAnimState_802BF0C(void) {
    u32 flagBits = LD16(A_dword_2036820 + 4);
    flagBits |= LD16(A_dword_2036820 + 0xC);

    if (flagBits & 1) {
        u32 state = LD8(R5 + 0xC);
        ST16(R5 + 2, state > 1 ? 0xC : 4);
        R7 = A_dword_2036820 + 8;
        R0 = 0;
    } else {
        R7 = LD32(R10 + oToolkit_CurFramePtr);
        R0 = (LD16(R7) & 8) >> 3;
    }

    GUEST_CALL(battle_drawChipSelectRowTilemapBlock_802C810);
    RETURN_TO_CALLER();
}

/* ── dispatchBattleObjectBehavior_802BF4C ── */
/* register form: calls one of three behavior handlers outside this file via a
 * 3-entry Thumb function-pointer table indexed by the byte at [object+3].
 * The callee receives R0=selector, R1=its own function pointer, and a stack
 * frame offset by 4 (from the push {lr}), so SP must be decremented first.
 * The if/else picks the handler; GUEST_CALL cannot do dynamic dispatch. */
static void dispatch_behavior_vtable(void) {
    u32 selector = LD8(R5 + 3);
    R0 = selector;
    R1 = LD32(A_off_802BF60 + selector);

    /* register form: push {lr} – callee builds its frame relative to SP;
     * without this the returned stack-pointer values in r2/r6 are offset by 4 */
    SP -= 4; ST32(SP, LR);

    if (selector == 0)
        GUEST_CALL(setupChipListVisual_802BF6C);
    else if (selector == 4)
        GUEST_CALL(sub_802C044);
    else
        GUEST_CALL(sub_802C0A4);

    /* register form: pop {pc} – undo the push so SP is restored */
    SP += 4;
}

/* dispatchBattleObjectBehavior_802BF4C: vtable tail-dispatch to a battle-object behavior handler */
Next dispatchBattleObjectBehavior_802BF4C(void) {
    dispatch_behavior_vtable();
    RETURN_TO_CALLER();
}

/* ── setupChipListVisual_802BF6C ── */
#define CHIP_LIST_SIZE 0x2A
#define GFX_VRAM_PRIMARY 0x600BD00
#define GFX_PRIMARY_SIZE (CHIP_LIST_SIZE << 6)   /* 0xA80 */
#define GFX_EXTRA_SIZE 0x20

/* register form: calls ShuffleByteList_PrimaryRNG outside this file, which has no typed C interface yet */
static void shuffle_byte_list(u32 list_ptr, u32 count, u32 swaps) {
    R0 = list_ptr; R1 = count; R2 = swaps;
    GUEST_CALL(ShuffleByteList_PrimaryRNG);
}

/* register form: calls decodePackedIdFields_802C54C outside this file, which has no typed C interface yet */
static void decode_packed_id(u32 packed, u32 *type, u32 *sub_id) {
    R0 = packed;
    GUEST_CALL(decodePackedIdFields_802C54C);
    *type = R0;
    *sub_id = R1;
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_802BF6C(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx_transfer_sub_802BF6C(u32 src, u32 dest, u32 size) {
    R0 = src; R1 = dest; R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* Prepare the chip/soul visual: shuffle the chip pool, select image and palette
 * sources based on the packed ID at toolkit+0x10, queue two GFX transfers to VRAM,
 * and mark the operation as "complete, mode 4".
 * Returns the final R7 (chip data pointer for type 0, otherwise the pool base). */
static u32 setup_chip_soul_visual(u32 toolkit) {
    /* Fill the chip pool with sequential indices 0..41, then shuffle them */
    u32 pool = A_unk_2035320;
    for (u32 i = 0; i < CHIP_LIST_SIZE; i++)
        ST8(pool + i, i);
    shuffle_byte_list(pool, CHIP_LIST_SIZE, CHIP_LIST_SIZE);

    /* Decode the packed chip/soul ID to choose image sources */
    u32 packed_id = LD16(toolkit + 0x10);
    u32 image_src, extra_src;
    u32 r7_val = pool;

    if (packed_id == 0xFFFF) {
        image_src = A_dword_8733E74;
        extra_src = A_dword_87343B4;
    } else {
        u32 type, sub_id;
        decode_packed_id(packed_id, &type, &sub_id);

        switch (type) {
        case 0:
            r7_val = get_chip_data_sub_802BF6C(sub_id);
            image_src = LD32(r7_val + 0x24);
            extra_src = LD32(r7_val + 0x28);
            break;
        case 1:
            image_src = A_dword_8732E54;
            extra_src = A_dword_8733394;
            break;
        case 2:
            if (sub_id != 0) {
                image_src = A_dword_87333B4;
                extra_src = A_dword_87338F4;
            } else {
                image_src = A_dword_8733914;
                extra_src = A_dword_8733E54;
            }
            break;
        case 3: {
            u32 offset;
            if ((s32)sub_id <= 1)      offset = 0;
            else if ((s32)sub_id <= 2) offset = 0x20;
            else if ((s32)sub_id <= 3) offset = 0x40;
            else                       offset = 0x60;
            image_src = A_dword_87343F4;
            extra_src = A_dword_8734934 + offset;
            break;
        }
        default:
            image_src = A_dword_8733E74;
            extra_src = A_dword_87343B4;
            break;
        }
    }

    /* Queue image transfer to VRAM, then extra data (palette/tiles) transfer */
    queue_gfx_transfer_sub_802BF6C(image_src, GFX_VRAM_PRIMARY, GFX_PRIMARY_SIZE);
    queue_gfx_transfer_sub_802BF6C(extra_src, A_unk_3001AE0, GFX_EXTRA_SIZE);

    /* Mark operation: clear "in-progress" byte, set mode = 4 (complete) */
    ST8(toolkit + 0xB, 0);
    ST8(toolkit + 3, 4);

    return r7_val;
}

/* setupChipListVisual_802BF6C(r5 = toolkit ptr) -> r7, flags; r0=4 */
Next setupChipListVisual_802BF6C(void) {
    R7 = setup_chip_soul_visual(R5);
    R0 = 4;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── runObjectStateHandler_802C1D4 ── */
/* runObjectStateHandler_802C1D4: two-way handler dispatcher for the object in R5.
 *
 * The object carries a handler selector at offset +3: a byte offset into a
 * two-entry table (0 selects the first handler, 4 the second). The selected
 * handler is invoked with:
 *   R0 = the selector byte (the handler's argument),
 *   R1 = the raw table entry, i.e. the handler's own Thumb address,
 *   LR = the address of the `pop {pc}` that follows the call, so the handler
 *        returns straight through to this function's caller,
 *   SP = four bytes lower than on entry (the pushed LR is visible at [SP] to
 *        the handler's stack-relative addressing).
 * R2-R12 and the flags pass through untouched, and whatever the handler
 * leaves in the registers and flags becomes this function's result. */
Next runObjectStateHandler_802C1D4(void) {
    u32 selector = LD8(R5 + 3);
    u32 handler  = LD32(A_off_802C1E8 + selector);

    /* push {lr}: the handler runs with SP four below our entry SP. */
    SP -= 4;
    ST32(SP, LR);

    R0 = selector;
    R1 = handler;
    if (selector < 4)
        GUEST_CALL(initShuffledOrderTable42_802C1F0);
    else
        GUEST_CALL(sub_802C218);

    /* Handler returned normally; undo the push (its slot was only ever
     * reachable through SP while the handler ran). */
    SP += 4;

    RETURN_TO_CALLER();
}

/* ── initShuffledOrderTable42_802C1F0 ── */
/* initShuffledOrderTable42_802C1F0(r5 = state struct):
 * Fill the 0x2A-byte table at unk_2035320 with 0..0x29, shuffle it with the
 * primary RNG, then reset the progress fields of the struct r5 points to
 * (r5[0xB] = 0, r5[3] = 4). Returns with NZ from the last constant stored. */
Next initShuffledOrderTable42_802C1F0(void) {
    u32 table = A_unk_2035320;

    R7 = table;
    for (u32 i = 0; i < 0x2A; i++)
        ST8(table + i, i);

    R0 = table;
    R1 = 0x2A;
    R2 = 0x2A;
    GUEST_CALL(ShuffleByteList_PrimaryRNG);

    ST8(R5 + 0xB, 0);
    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 3, R0);
    RETURN_TO_CALLER();
}

/* ── sub_802C218 ── */
/* sub_802C218 -- Chip Folder screen opening: redraw the folder window and
 * advance the screen object to its next state.
 *
 * Note: the binary carries a second path (0802C226-0802C27A) that revealed the
 * window one tile per call while the timer at +0xB was below 42 (= the 7x6
 * tiles of the window).  The unconditional branch at 0802C224 jumps straight
 * past its test, so that path is unreachable and is not ported. */

/* chip-folder screen object fields (offsets as in the disassembly) */
#define oFolderScreen_state  2    /* u16: state/phase id */
#define oFolderScreen_timer  0xB  /* u8: frame counter/timer */

/* register form: calls chipFolder_ClearTilemapArea_802C59C outside this file, which has no typed C interface yet */
static void clear_tilemap_area(u32 row_buffer) {
    R0 = row_buffer;
    GUEST_CALL(chipFolder_ClearTilemapArea_802C59C);
}

/* register form: calls blitTileBlockToRowBuffer_802C4B6 outside this file, which has no typed C interface yet */
static void blit_tile_block(u32 x, u32 y, u32 row_buffer, u32 width, u32 height) {
    R0 = x;
    R1 = y;
    R2 = row_buffer;
    R3 = width;
    R4 = height;
    GUEST_CALL(blitTileBlockToRowBuffer_802C4B6);
}

/* register form: the tilemap work buffer must live on the guest stack -- its
 * guest address is handed to the tilemap routines above, which read and write
 * guest memory through it */
static u32 chip_folder_screen_open(u32 screen) {
    SP -= 0x78;               /* saved-lr slot + the original's 0x74-byte frame */
    u32 frame = SP;           /* the original's r6; callers read it afterwards */
    u32 row_buffer = frame + 4;

    clear_tilemap_area(row_buffer);
    blit_tile_block(14, 10, row_buffer, 7, 6);  /* 7x6-tile folder window at tile (14,10) */

    ST16(screen + oFolderScreen_state, 4);      /* switch to the next state */
    ST8(screen + oFolderScreen_timer, 30);      /* with a 30-frame timer */

    SP += 0x78;
    return frame;             /* r6 keeps the address of the released frame */
}

/* sub_802C218(r5 = chip-folder screen object) -> r6 = stack-frame address; r5, r7 unchanged */
Next sub_802C218(void) {
    R6 = chip_folder_screen_open(R5);
    RETURN_TO_CALLER();
}

/* ── battle_advanceFade_802C328 ── */
/* battle_advanceFade_802C328: 0802C328-0802C348 THUMB, bn6f asm03_0.s:12588 */
/*   inputs:  r5 lr
 *   outputs: nz c v r0 r1 r2 r3 r5 lr
 *   clobbers (unused by callers): -
 *   preserved: r5 */

#define FADE_INIT_DURATION  0xC
#define FADE_INIT_MODE      0x10
#define FADE_INIT_FLAG      4      /* byte at battle_state+3: marks fade as started */
#define FADE_ACTIVE_STATE   8      /* word at battle_state: state when fade still running */

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void call_set_screen_fade(u32 duration, u32 mode) {
    R0 = duration;
    R1 = mode;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static u32 call_is_screen_fade_active_sub_802C328(bool *out_active) {
    GUEST_CALL(IsScreenFadeActive);
    *out_active = !ZERO_FLAG();   /* Z=1 → fade complete (inactive); Z=0 → still fading */
    return R0;
}

/*
 * Advance the battle screen-fade state machine.
 * If fade hasn't been initiated yet (byte flag == 0), start it and mark flag.
 * Then poll whether the fade is still in progress; if so, write state 8 to the
 * battle-object word and return 8.  Otherwise return IsScreenFadeActive's r0.
 * Flags (nz/c/v) and r1–r3 are left untouched by IsScreenFadeActive's GUEST_CALL.
 */
static u32 advance_battle_fade(u32 battle_state) {
    if (LD8(battle_state + 3) == 0) {
        call_set_screen_fade(FADE_INIT_DURATION, FADE_INIT_MODE);
        ST8(battle_state + 3, FADE_INIT_FLAG);
    }

    bool fade_still_running;
    u32 fade_r0 = call_is_screen_fade_active_sub_802C328(&fade_still_running);

    if (fade_still_running) {
        ST32(battle_state, FADE_ACTIVE_STATE);
        return FADE_ACTIVE_STATE;
    }

    return fade_r0;
}

Next battle_advanceFade_802C328(void) {
    R0 = advance_battle_fade(R5);
    RETURN_TO_CALLER();
}

/* ── sub_802C348 ── */
/* sub_802C348(r5 = struct pointer): clear the byte at offset 5 of the struct
 * (e.g. reset a state/counter field to zero). Always returns r0 = 0 with
 * zero flag set (the final "mov r0, #0" sets the flags callers branch on). */
Next sub_802C348(void) {
    R0 = 0;
    ST8(R5 + 5, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── setupNaviDisplay_802C34E ── */
/* ── Data-table addresses (ROM-side literal-pool entries) ─────────────────── */
#define NAVI_DISPLAY_BASE   0x20364C0u  /* eS20364C0 – 32-byte navi display struct in EWRAM */
#define PORTRAIT_TABLE      0x0802C3FCu /* 3 × (src, VRAM dest, size) for navi portraits */
#define TILE_SRC_TABLE      0x0802C434u /* 3 × tile-sheet source pointer, one per navi type */
#define BG_XFER_ENTRY       0x0802C424u /* (src, dest, size) for the background tile */
#define EXTRA_XFER_ENTRY    0x0802C448u /* (src, dest, size) for the extra tile sheet */
#define TILE_STAGING_BUF    0x2034B30u  /* unk_2034B30 – EWRAM staging for copied tile sheet */
#define PET_CHIP_VRAM       0x06014000u /* VRAM destination for 80h chip data */
#define PERMUTED_REC_DEST   0x030016B0u /* byte_30016B0 – WRAM dest for permuted record */

#define NAVI_ENTRY_SIZE   12            /* bytes per entry in the indexed attribute array */
#define NAVI_DISPLAY_SIZE 0x20          /* bytes to clear in the display struct */
#define TILE_SHEET_BYTES  (0x36 * 16)   /* 0x360 – size of one navi tile sheet */
#define BATTLEDISPLAY_FLAG 0xE2u        /* "displayed" sentinel written to +6 */

/* ── Register-form helpers (game functions without a typed C interface) ───── */

/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet */
static void zero_fill(u32 dest, u32 count) {
    R0 = dest; R1 = count;
    GUEST_CALL(ZeroFillByByte);
}

/* register form: calls eStruct200A008_setUnk04 outside this file, which has no typed C interface yet */
static void navi_struct_set_field4(u16 value) {
    R0 = value;
    GUEST_CALL(eStruct200A008_setUnk04);
}

/* register form: calls eStruct200A008_setUnk06 outside this file, which has no typed C interface yet */
static void navi_struct_set_field6(u16 value) {
    R0 = value;
    GUEST_CALL(eStruct200A008_setUnk06);
}

/* register form: calls CopyByEightWords outside this file, which has no typed C interface yet */
static void copy8(u32 src, u32 dest, u32 size) {
    R0 = src; R1 = dest; R2 = size;
    GUEST_CALL(CopyByEightWords);
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx(u32 src, u32 dest, u32 size) {
    R0 = src; R1 = dest; R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u32 cur_pet_navi(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls getIndexed80hRecordPtr_8028164 outside this file, which has no typed C interface yet */
static u32 indexed80h_record(u32 navi) {
    R0 = navi;
    GUEST_CALL(getIndexed80hRecordPtr_8028164);
    return R0;
}

/* register form: calls getPermutedRecord32_8028172 outside this file, which has no typed C interface yet */
static u32 permuted_record(u32 navi) {
    R0 = navi;
    GUEST_CALL(getPermutedRecord32_8028172);
    return R0;
}

/* ── Typed implementation ─────────────────────────────────────────────────── */

/* Build the per-navi display struct at eS20364C0 from the battle-attribute
 * entry selected by the current battle-state navi slot, then queue every
 * associated GFX transfer (portrait, tile sheet, background, extra sheet,
 * and the two PET-chip records).
 *
 *   attr_base : start of the 12-byte-per-entry navi-attribute array (caller's r0)
 *   toolkit   : pointer to the game Toolkit struct (caller's r10) */
static void setup_navi_display(u32 attr_base, u32 toolkit) {
    /* Select the attribute entry: slot = battle state byte +0xD */
    u32 bstate = LD32(toolkit + oToolkit_BattleStatePtr);
    u32 slot   = LD8(bstate + oBattleState_Unk_0d);
    u32 entry  = attr_base + slot * NAVI_ENTRY_SIZE;

    /* Clear the 32-byte display struct and copy relevant fields */
    u32 disp = NAVI_DISPLAY_BASE;
    zero_fill(disp, NAVI_DISPLAY_SIZE);

    ST8(disp + 0x04, LD8(entry));        /* navi sprite-selector byte */
    ST8(disp + 0x08, LD8(entry + 1));    /* palette / sub-type byte */
    ST8(disp + 0x0D, LD8(entry + 3));    /* flag byte */
    ST32(disp + 0x1C, LD32(entry + 4));  /* tile-data word 0 */
    ST32(disp + 0x14, LD32(entry + 8));  /* tile-data word 1 */

    /* Push the two halfwords of tile-data word 1 into the navi display object */
    navi_struct_set_field4((u16)LD16(disp + 0x14));
    navi_struct_set_field6((u16)LD16(disp + 0x16));

    /* Mark the struct as active */
    ST8(disp + 0x06, BATTLEDISPLAY_FLAG);

    /* Copy the type-specific tile sheet into the VRAM staging buffer */
    u32 type = LD8(disp + 0x04);
    u32 tile_src = LD32(TILE_SRC_TABLE + type * 4);
    copy8(tile_src, TILE_STAGING_BUF, TILE_SHEET_BYTES);

    /* Queue portrait transfer (3-entry table indexed by type, 12 bytes each) */
    u32 pidx = PORTRAIT_TABLE + type * NAVI_ENTRY_SIZE;
    queue_gfx(LD32(pidx), LD32(pidx + 4), LD32(pidx + 8));

    /* Queue background tile transfer (single fixed entry; loop index is always 0) */
    queue_gfx(LD32(BG_XFER_ENTRY), LD32(BG_XFER_ENTRY + 4), LD32(BG_XFER_ENTRY + 8));

    /* Queue extra tile-sheet transfer (fixed entry → VRAM 0x6009400, 0x5E0 bytes) */
    queue_gfx(LD32(EXTRA_XFER_ENTRY), LD32(EXTRA_XFER_ENTRY + 4), LD32(EXTRA_XFER_ENTRY + 8));

    /* Queue 80h chip GFX for the current PET navi → VRAM 0x6014000 */
    u32 chip = indexed80h_record(cur_pet_navi());
    queue_gfx(chip, PET_CHIP_VRAM, 0x80);

    /* Queue permuted record for the current PET navi → WRAM 0x30016B0 */
    u32 perm = permuted_record(cur_pet_navi());
    queue_gfx(perm, PERMUTED_REC_DEST, 0x20);
}

/* ── Thin adapter ─────────────────────────────────────────────────────────── */

/* setupNaviDisplay_802C34E(r0 = navi-attribute base, r10 = toolkit) → (r4-r7 preserved) */
Next setupNaviDisplay_802C34E(void) {
    setup_navi_display(R0, R10);
    RETURN_TO_CALLER();
}

/* ── battle_chipSelect_handleUpDownInput_802C460 ── */
/* battle_chipSelect_handleUpDownInput_802C460 (r5 = object pointer):
 * Reads two signed bytes from the object at offsets 6 and 7 (two positions
 * along an axis). If they are equal there is nothing to do. Otherwise it
 * computes the distance between them and a start position (the smaller
 * value, offset by 0x18 when the byte at offset 6 is the smaller one) and
 * asks sub_802C490(start, 2, distance, 0x12) to scan between them. If that
 * scan reports a hit in r2, it calls call_sub_3005EBA(scan_result, 3, 0).
 *
 * r4/r5 are preserved around the calls; r1 ends up holding the byte at
 * offset 7 on the early-out path, and whatever the callees leave otherwise.
 * c/v are outputs: on the early-out path they come from the CMP of the two
 * bytes; on the call paths they are whatever the callee left. */
Next battle_chipSelect_handleUpDownInput_802C460(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    s32 first = (s8)LD8(R5 + 6);
    s32 second = (s8)LD8(R5 + 7);
    R1 = second; /* original leaves the loaded byte in r1 */

    /* CMP first, second — the last flag-setting op on the early-out path. */
    SET_FLAGS_CMP(first, second);

    if (first != second) {
        u32 distance;
        u32 start;

        if (first < second) {
            /* first is the lower position: start just past it. */
            distance = second - first;
            start = first + 0x18;
        } else {
            distance = first - second;
            start = second;
        }

        R0 = start;
        R1 = 2;
        R4 = distance;
        R5 = 0x12;
        GUEST_CALL(sub_802C490); /* r2 = nonzero if something was found */

        if (R2 != 0) {
            R2 = 3;
            R3 = 0;
            GUEST_CALL(call_sub_3005EBA);
        }

        R4 = savedR4;
        R5 = savedR5;
    }

    RETURN_TO_CALLER();
}

/* ── blitTileBlockToRowBuffer_802C4B6 ── */
/* blitTileBlockToRowBuffer_802C4B6(r0 = x, r1 = y, r2 = source data, r3 = width, r4 = height):
 * Blit a width×height block of halfwords into the 24-halfword-wide buffer at
 * unk_2034B30, at offset (x + y*24) halfwords. Source rows are consecutive;
 * destination rows advance by 0x30 bytes (24 halfwords) each time.
 * Leaves r0 = width, r1 = height, r2 = one-past-the-end of the source data;
 * flags reflect the final `cmp r1, r4` (i.e. last row counter == height).
 * r5-r7 are pushed/popped by the original, so they end unchanged. */
Next blitTileBlockToRowBuffer_802C4B6(void) {
    u32 src = R2;
    u32 dst = A_unk_2034B30 + (R0 + R1 * 0x18) * 2;

    u32 row = 0;
    do {
        u32 dstRow = dst;
        u32 col = 0;
        do {
            ST16(dstRow, LD16(src));
            src += 2;
            dstRow += 2;
            col++;
        } while (col != R3);
        dst += 0x30; /* stride to the next buffer row */
        row++;
    } while (row != R4);

    R0 = R3; /* column counter as the original leaves it */
    R1 = R4; /* row counter as the original leaves it */
    R2 = src;
    SET_FLAGS_CMP(R1, R4);
    RETURN_TO_CALLER();
}

/* ── sub_802C4E8 ── */
/* Five-big-digit timer display: converts a value to BCD and draws it as five
 * 1x2-tile digits in an M'SS"CC layout (the gaps in the x positions below hold
 * the separator tiles), capped at 9'59"99. Digit glyphs start at tile 0xA0,
 * two tiles per digit, drawn with palette (palette + 9). */

#define TIMER_CAP_BCD        0x95999 /* BCD-coded 9'59"99, largest shown value */
#define TIMER_DIGIT_TILE     0xA0    /* first digit glyph tile ("0" = 0xA0/0xA1) */
#define TIMER_DIGIT_COUNT    5

/* x position (in tiles) of each digit, least significant first -- the
 * original's table at 0x802C538; gaps at 0x0F/0x12 are the separators */
static const u8 timer_digit_x[TIMER_DIGIT_COUNT] = { 0x14, 0x13, 0x11, 0x10, 0x0E };

/* register form: calls memory_bcd_8000D84 outside this file, which has no typed C interface yet */
static u32 to_bcd(u32 value) {
    R0 = value;
    GUEST_CALL(memory_bcd_8000D84);
    return R0;
}

/* register form: calls blitTileBlockToRowBuffer_802C4B6 outside this file (no
 * typed C interface yet); the callee takes the digit's two tile-map entries by
 * guest address, so they must sit in the original's stack slot (entry SP - 12),
 * and the original leaves the loop's r6 (that slot) and r7 (the x table) in
 * those registers for its callers. */
static void blit_digit_tiles(u32 x, u16 top_entry, u16 bottom_entry) {
    SP -= 12;
    ST16(SP, top_entry);
    ST16(SP + 2, bottom_entry);
    R0 = x;    /* column */
    R1 = 4;    /* row */
    R2 = SP;   /* the two tile-map entries on the guest stack */
    R3 = 1;    /* one tile wide */
    R4 = 2;    /* two tiles tall */
    GUEST_CALL(blitTileBlockToRowBuffer_802C4B6);
    R6 = SP;             /* return-time r6 = the digit buffer slot */
    R7 = A_byte_802C538; /* return-time r7 = the digit x-position table */
    SP += 12;
}

/* Draw `time` (converted to BCD) as the five big timer digits, using palette
 * number (palette + 9) for the tile-map entries. */
static void draw_timer(u32 time, u32 palette) {
    u32 digits = to_bcd(time);
    if ((s32)digits > (s32)TIMER_CAP_BCD) /* signed compare, as the original */
        digits = TIMER_CAP_BCD;

    /* tile-map entry base: palette (palette + 9) in bits 12-15,
     * first of the digit's two glyph tiles in the low bits */
    u32 entry_base = ((palette + 9) << 12) | TIMER_DIGIT_TILE;

    for (int i = 0; i < TIMER_DIGIT_COUNT; i++) {
        u32 entry = entry_base + (digits & 0xF) * 2;
        blit_digit_tiles(timer_digit_x[i], entry, entry + 1);
        digits >>= 4; /* next digit, up to the ten-minutes place */
    }
}

/* sub_802C4E8(r0 = time value, r1 = palette) -> r5 unchanged, r6/r7 left as by the loop */
Next sub_802C4E8(void) {
    draw_timer(R0, R1);
    RETURN_TO_CALLER();
}

/* ── decodePackedIdFields_802C54C ── */
/* decodePackedIdFields_802C54C(r0 = packed id):
 * Splits a packed id into its fields and returns them:
 *   r0 = group (bits 14-15; 0xFF if the id is the 0xFFFF sentinel),
 *   r1 = low field (bits 0-8 for group 0, bits 0-13 otherwise),
 *   r2 = middle field (bits 9-13, only produced for group 0;
 *        left untouched for other groups). */
Next decodePackedIdFields_802C54C(void) {
    u32 packed = R0;

    if (packed == 0xFFFF) {
        /* Sentinel id: report it as group 0xFF. */
        R0 = 0xFF;
        R1 = 0xFFFF;
        RETURN_TO_CALLER();
    }

    u32 group = packed >> 14;
    if (group == 0) {
        R0 = 0;
        R2 = packed >> 9;
        R1 = packed & 0x1FF;
    } else {
        R0 = (group == 1) ? 1 : (group == 2) ? 2 : 3;
        R1 = packed & 0x3FFF;
    }
    RETURN_TO_CALLER();
}

/* ── battle_fillChipSelectRow_802C588 ── */
/* battle_fillChipSelectRow_802C588(r0 = destination address):
 * Fill 0x2A halfwords at r0 with the consecutive values 0xC1E8..0xC211.
 * Leaves r1 = 0xC212 (one past the last value written) and r2 = 0
 * (loop counter after exhaustion); r0 points past the filled range. */
Next battle_fillChipSelectRow_802C588(void) {
    u32 dst   = R0;
    u32 value = 0xC1E8;

    for (u32 count = 0x2A; count != 0; count--) {
        ST16(dst, (u16)value);
        value++;
        dst += 2;
    }

    R1 = value; /* 0xC212 */
    R2 = 0;
    R0 = dst;
    RETURN_TO_CALLER();
}

/* ── chipFolder_ClearTilemapArea_802C59C ── */
/* chipFolder_ClearTilemapArea_802C59C(r0 = destination):
 * fill 0x2A halfwords with the value 0x90C6 (used to blank the chip folder
 * screen's VRAM tilemap area). */
Next chipFolder_ClearTilemapArea_802C59C(void) {
    u32 dst = R0;
    const u16 fill = 0x90C6;

    for (u32 i = 0; i < 0x2A; i++, dst += 2)
        ST16(dst, fill);

    RETURN_TO_CALLER();
}

/* ── textWindow_openDialog_802C5B0 ── */
/* textWindow_openDialog_802C5B0(r0 = value to record):
 * Opens the in-game dialog/upgrade window: remembers the caller's value in
 * the toolkit's unk_200A220-backed state, renders the window's text
 * graphics into VRAM, then queues the follow-up draw job (whose r1/r2
 * results are returned to the caller). r4-r7 and r8/r9 are preserved. */
Next textWindow_openDialog_802C5B0(void) {
    u32 value = R0;
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* Reserve the original's 0x70-byte stack frame. Its address briefly
     * lands in r6 and is spilled below SP across the calls, but the frame
     * itself is never otherwise used. */
    u32 frame = SP - 0x70;
    SP = frame;
    SP -= 4;
    ST32(SP, frame);                    /* push {r6} */

    /* Stash the caller's value in the state struct pointed to by the
     * toolkit's oToolkit_Unk200a220_Ptr entry. */
    u32 statePtr = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(statePtr + 8, value);

    /* renderTextGfx_8045F8C(script = TextScript8736D74, 5, tiles = unk_2035320,
     * vram = 0x600BA80, 10, 1, palette = dword_86B7AE0, 8):
     * blit the window's text graphics. */
    R0 = A_TextScript8736D74;
    R1 = 5;
    R2 = A_unk_2035320;
    R3 = 0x600BA80;
    R4 = 0xA;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 8;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* blitTileBlockToRowBuffer_802C4B6(2, 12, byte_802C6C4, 10, 2): queue the window's
     * tilemap/animation update; its r1/r2 are this function's result. */
    R0 = 2;
    R1 = 0xC;
    R2 = A_byte_802C6C4;
    R3 = 0xA;
    R4 = 2;
    GUEST_CALL(blitTileBlockToRowBuffer_802C4B6);

    /* Tear down the frame and hand back the callee's r1/r2/r3, restoring
     * the caller's r4-r7 as the original's pop {r4-r7,pc} does. */
    SP += 4;                            /* pop {r6} (value unused) */
    SP += 0x70;
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_802C6EC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3, r4, r7; } call_memory_bcdConvert_result;
/* register form: generated wrapper: memory_bcdConvert has no typed C interface yet */
static inline call_memory_bcdConvert_result call_memory_bcdConvert(u32 r0) {
    R0 = r0;
    GUEST_CALL(memory_bcdConvert);
    return (call_memory_bcdConvert_result){ R0, R2, R3, R4, R7 };
}

typedef struct { u32 r0; } call_countHexDigits_8000C5C_result;
/* register form: generated wrapper: countHexDigits_8000C5C has no typed C interface yet */
static inline call_countHexDigits_8000C5C_result call_countHexDigits_8000C5C(u32 r0) {
    R0 = r0;
    GUEST_CALL(countHexDigits_8000C5C);
    return (call_countHexDigits_8000C5C_result){ R0 };
}

typedef struct { u32 r2, r7; } call_blitTileBlockToRowBuffer_802C4B6_result;
/* register form: generated wrapper: blitTileBlockToRowBuffer_802C4B6 has no typed C interface yet */
static inline call_blitTileBlockToRowBuffer_802C4B6_result call_blitTileBlockToRowBuffer_802C4B6(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(blitTileBlockToRowBuffer_802C4B6);
    return (call_blitTileBlockToRowBuffer_802C4B6_result){ R2, R7 };
}

#define NUMBER_TILE_BANK 0x90A0u   /* first tile index of the digit glyphs */
#define MAX_DRAWN_VALUE  0xB       /* values above this are clamped to 11 */

/*
 * Render a small number as tile glyphs and blit it into the row buffer.
 *
 * A scratch row is built on the guest stack (SP lowered by 0x1C):
 *   +0x00..+0x13  five u16 tile slots, each duplicated at +0x0A
 *   +0x14         the clamped value (spill slot, as in the original)
 * The slots are pre-filled with a blank tile pair, then digit glyphs are
 * written right-to-left over them.  Values >= 11 draw a single glyph taken
 * from a tile bank shifted 0x1000 indices further in; smaller values are
 * converted to packed BCD first and drawn digit by digit.
 */
static void draw_value_as_tiles(u32 value, u32 entry_r4) {
    /* register form: the scratch row must live at the guest SP because
     * blitTileBlockToRowBuffer is handed that exact stack address (r2 = sp),
     * so SP is lowered and restored around the build instead of using a
     * plain C array. */
    SP -= 0x1C;

    u32 clamped = ((s32)value > (s32)MAX_DRAWN_VALUE) ? MAX_DRAWN_VALUE : value;
    ST32(SP + 0x14, clamped);

    /* Pre-fill every slot, both copies, with a blank tile pair. */
    u32 blank = NUMBER_TILE_BANK + 0x14;
    u32 slot = SP;
    for (s32 i = 0; i < 5; i++) {
        ST16(slot, blank);
        ST16(slot + 0xA, blank + 1);
        slot += 2;
    }

    u32 bank;
    u32 digits;
    s32 digit_count;
    if ((s32)clamped >= (s32)MAX_DRAWN_VALUE) {
        /* 11: a single glyph from the bank 0x1000 tile indices further in. */
        bank = NUMBER_TILE_BANK + (1u << 12);
        digits = MAX_DRAWN_VALUE;
        digit_count = 1;
    } else {
        digits = call_memory_bcdConvert(clamped).r0;      /* packed BCD value */
        digit_count = (s32)call_countHexDigits_8000C5C(digits).r0;
        bank = NUMBER_TILE_BANK;
    }

    /* Write the digit glyphs right-to-left; each slot's second copy gets
     * the following tile (glyph + 1). */
    u32 dst = SP + 8;
    do {
        u32 glyph = (digits & 0xF) * 2 + bank;
        ST16(dst, glyph);
        ST16(dst + 0xA, glyph + 1);
        digits >>= 4;
        dst -= 2;
        digit_count--;
    } while (digit_count > 0);

    /* Blit the finished row: 16x6 area, five tile pairs, 2 rows apart. */
    call_blitTileBlockToRowBuffer_802C4B6(0x10, 6, SP, 5, 2);

    SP += 0x1C;
    /* register form: the blit wrapper passes r4 = 2 as an argument and the
     * callee keeps it, so the guest's r4 is left at 2, while the original
     * pops its saved r4 back over it.  No wrapper returns r4, so typed C
     * has no way to obtain and restore the entry value. */
    R4 = entry_r4;
}

/* sub_802C6EC(r0 = value to draw)
 * r4-r6 are callee-saved (the original pushes/pops them); r7 ends as the
 * blit helper leaves it. */
Next sub_802C6EC(void) {
    draw_value_as_tiles(R0, R4);
    RETURN_TO_CALLER();
}

/* ── queue_popHalfword_802C8D4 ── */
/* queue_popHalfword_802C8D4(r5 = queue state): pop the next halfword from an in-struct queue.
 * Layout at r5: +0xC = read index, +0xD = entry count, +0x14 = halfword entries.
 * Returns the popped value in r0 (0 when the queue is exhausted), r1 = read index
 * (incremented if an entry was popped), r2 = entry offset / count.
 * Flags: c/v from the CMP when the queue is empty, from the index increment otherwise. */
Next queue_popHalfword_802C8D4(void) {
    u32 read = LD8(R5 + 0xC);
    u32 count = LD8(R5 + 0xD);

    if ((s32)read >= (s32)count) {
        /* Queue exhausted: return 0 and leave the index untouched. */
        R0 = 0;
        R1 = read;
        R2 = count;
        SET_FLAGS_CMP(read, count);
    } else {
        /* Fetch the entry at read index and advance the index. */
        u32 offset = 0x14 + read + read;
        R0 = LD16(R5 + offset);
        R1 = read + 1;
        R2 = offset;
        ST8(R5 + 0xC, R1);
        SET_FLAGS_ADD(R1, 1);
    }
    RETURN_TO_CALLER();
}

/* ── battle_getStateTableEntryPtr_802C8EA ── */
/* battle_getStateTableEntryPtr_802C8EA: select the 8-byte entry of the dword_2036820 table indexed by
 * the battle state's current Unk_0d slot.
 *   r10 = toolkit
 *   out r0 = &dword_2036820[Unk_0d], r2 = Unk_0d * 8,
 *   flags as left by the final address add. */
Next battle_getStateTableEntryPtr_802C8EA(void) {
    u32 battleState;

    R1 = R10;
    battleState = LD32(R1 + oToolkit_BattleStatePtr);
    R2 = (u32)LD8(battleState + oBattleState_Unk_0d) * 8;
    R0 = A_dword_2036820 + R2;
    SET_FLAGS_ADD(A_dword_2036820, R2);
    RETURN_TO_CALLER();
}

/* ── battle_checkTimerWindowSubstate_802C97E ── */
/* battle_checkTimerWindowSubstate_802C97E(r5 = battle object, r10 = battle state context):
 * Re-evaluates the object's sub-state byte at [r5+0x0E] based on the battle's
 * state byte 0x0D. battle_findSpecificBossOnOpposingSide_802CA1E maps that state byte to an index into the
 * frame-window tables (0xFF = no window for this state). For a valid index i
 * the object's timer at [r5+0x1C] is compared against the u16 window bounds
 * unk_20018C0[i] (window start) and unk_2000260[i] (window end):
 *
 *   timer <  start              -> [r5+0x0F] = 1, result 2 (window not reached)
 *   start <= timer < end        -> [r5+0x0F] = 1, result 1 (inside the window)
 *   timer >= end                -> result 0            (window finished)
 *   no window (0xFF from call)  -> result 0
 *
 * Returns the result in r0 (also stored to [r5+0x0E]); r1/r2/r3 hold the last
 * table base, table entry and timer value (battle_findSpecificBossOnOpposingSide_802CA1E's r1 in the 0xFF case).
 * Flags are those of `adds r0, result, #0`. */
Next battle_checkTimerWindowSubstate_802C97E(void) {
    u32 savedR4 = R4;
    u32 timer, windowStart, windowEnd, result;

    R4 = 0;

    /* state byte 0x0D -> sub-state index via battle_findSpecificBossOnOpposingSide_802CA1E (also returns r1) */
    GUEST_CALL(battle_getStateByte0d_800A7E2);
    GUEST_CALL(battle_findSpecificBossOnOpposingSide_802CA1E);

    if (R0 == 0xFF) {
        /* no active window: sub-state stays 0; r1 keeps battle_findSpecificBossOnOpposingSide_802CA1E's value,
         * r2/r3 are left untouched */
        result = 0;
    } else {
        u32 index = R0 + R0;    /* index into the u16 tables */

        R1 = A_unk_20018C0;
        R2 = windowStart = LD16(A_unk_20018C0 + index);
        R3 = timer = LD32(R5 + 0x1C);

        if ((s32)timer >= (s32)windowStart) {
            R4 = 1;
            R1 = A_unk_2000260;
            R2 = windowEnd = LD16(A_unk_2000260 + index);
            R3 = timer = LD32(R5 + 0x1C);

            if ((s32)timer < (s32)windowEnd) {
                ST8(R5 + 0xF, 1);   /* inside the window */
                result = 1;
            } else {
                result = 0;         /* window already over */
            }
        } else {
            ST8(R5 + 0xF, 1);       /* window not started yet */
            result = 2;
        }
    }

    R0 = result;
    SET_FLAGS_ADD(result, 0);
    ST8(R5 + 0xE, result);
    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_updateSlotBestRecords_802C9B8 ── */
/* battle_updateSlotBestRecords_802C9B8: update the per-slot record tables with this battle's value.
 *
 * Reads the u32 at battle state +0x40 (the value to record), then asks
 * battle_findSpecificBossOnOpposingSide_802CA1E which table slot the state's byte at +0x0D selects (0xFF =
 * nothing to record).  unk_20018C0 tracks the best (lowest) value seen per
 * slot and unk_2000260 the running value:
 *   - value below the recorded one: overwrite both tables
 *   - otherwise: lower the unk_2000260 entry if the value is below it
 * Returns with the flags left as the original's last comparison set them
 * (the v flag is an output; nz/c are free). */
Next battle_updateSlotBestRecords_802C9B8(void) {
    u32 savedR4 = R4;

    /* R0 = value to record; the byte getter receives it in R0 as the
     * original leaves it there between the two calls. */
    GUEST_CALL(battle_getStateUnk40_800A704);
    u32 value = R0;

    GUEST_CALL(battle_getStateByte0d_800A7E2);

    /* battle_findSpecificBossOnOpposingSide_802CA1E gets the state byte in r0 and the recorded value in r4. */
    R4 = value;
    GUEST_CALL(battle_findSpecificBossOnOpposingSide_802CA1E);
    u32 slot = R0;
    R4 = savedR4;

    if (slot == 0xFF) {
        SET_FLAGS_CMP(R0, 0xFF);
        RETURN_TO_CALLER();
    }

    u32 offset = slot * 2;
    u16 recorded = LD16(A_unk_20018C0 + offset);
    u16 current = LD16(A_unk_2000260 + offset);

    if ((s32)value < (s32)recorded) {
        /* new best value: record it in both tables */
        ST16(A_unk_20018C0 + offset, value);
        ST16(A_unk_2000260 + offset, value);
        SET_FLAGS_CMP(value, recorded);
    } else if ((s32)value < (s32)current) {
        /* below the running value: only lower that one */
        ST16(A_unk_2000260 + offset, value);
        SET_FLAGS_CMP(value, current);
    } else {
        SET_FLAGS_CMP(value, current);
    }
    RETURN_TO_CALLER();
}

/* ── battle_syncEffectSlotBetweenBuffers_802C9EA ── */
/* battle_syncEffectSlotBetweenBuffers_802C9EA: keep the halfword at +8 of the two effect buffers
 * (unk_2035260 / unk_203526C) in sync. Which buffer is source and which is
 * destination depends on oBattleState_Unk_10 in the current battle state.
 * If the value read from the source is 0xFFFF, it is replaced by the result
 * of sub_80AABA4(byte at dst+1) before being written back. The whole thing
 * only runs when GetBattleEffects() reports effect bit 3. */
Next battle_syncEffectSlotBetweenBuffers_802C9EA(void) {
    /* () -> effect bitmask in R0 (R10 = toolkit pointer) */
    GUEST_CALL(GetBattleEffects);

    R1 = 8;
    if ((R0 & R1) != 0) {
        /* Default direction: unk_203526C -> unk_2035260. */
        R6 = A_unk_203526C;
        R7 = A_unk_2035260;

        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        if (LD8(battleState + oBattleState_Unk_10) != 0) {
            /* Swapped direction: unk_2035260 -> unk_203526C. */
            R6 = A_unk_2035260;
            R7 = A_unk_203526C;
        }

        /* Copy the word over, then inspect the halfword copy. */
        ST32(R7 + 8, LD32(R6 + 8));
        u32 word = LD16(R6 + 8);

        R1 = 0xFFFF;
        SET_FLAGS_CMP(word, 0xFFFF);
        if (word == 0xFFFF) {
            /* Slot is empty: compute the value from the destination's byte. */
            R0 = LD8(R7 + 1);
            GUEST_CALL(sub_80AABA4);
            ST16(R7 + 8, R0);
        }
    }
    RETURN_TO_CALLER();
}

/* ── battle_findSpecificBossOnOpposingSide_802CA1E ── */
/* battle_findSpecificBossOnOpposingSide_802CA1E(r0 = side index (0 or 1), r10 = toolkit):
 * Scan the four enemy slots of the side opposite to r0 in the battle state
 * (each side's slots live at offset 0x4C, one 8-byte block per side, four
 * u16 enemy indices each). For each index, look up its version/actor type/AI
 * index; if it is a version-2 enemy of actor type 1 with AI index < 0x14,
 * return its AI index minus 1. Otherwise return 0xFF.
 * r1 on return is the last field compared, exactly as in the original
 * (2 on success); callers of this function ignore it. */
Next battle_findSpecificBossOnOpposingSide_802CA1E(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u32 slot = battle + (R0 ^ 1) * 8 + 0x4C;

    for (u32 i = 0; i < 4; i++, slot += 2) {
        R0 = LD16(slot);
        GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);
        u32 info = R0;

        R1 = LD8(info + 1);              /* actor type */
        if (R1 != 1)
            continue;
        R1 = LD8(info + 2);              /* AI index */
        if ((s32)R1 >= 0x14)
            continue;
        R1 = LD8(info);                  /* version */
        if (R1 != 2)
            continue;

        R0 = LD8(info + 2) - 1;          /* matched: return AI index - 1 */
        RETURN_TO_CALLER();
    }

    R0 = 0xFF;
    RETURN_TO_CALLER();
}

/* ── sound_pushRequest_802CA5C ── */
/* sound_pushRequest_802CA5C(r5 = sound request source):
 * Pack the signed sound id stored at offset 6 of the struct in r5 into a
 * FIFO record for the ARM7 sound handler and push it. The record value is
 *
 *     0x40000000 | (((id << 3) + 13) & 0x1FF) << 16 | 0x15
 *
 * i.e. bit 30 flags the record as a sound request, bits 16..24 carry the
 * packed id (9 bits, as used by the ARM7 side), and the low byte 0x15
 * selects the command. The remaining record words are the fixed tag/data
 * pair 0xB200, 0, 0. R5 is only read; R4/R6/R7 are untouched. */
Next sound_pushRequest_802CA5C(void) {
    u32 id = (u32)(s32)(s8)LD8(R5 + 6);
    u32 packed = ((id << 3) + 13) & 0x1FF;

    R0 = 0x40000000 | (packed << 16) | 0x15;
    R1 = 0xB200;
    R2 = 0;
    R3 = 0;
    GUEST_CALL(fifo_pushRecord16_8009FF8);

    RETURN_TO_CALLER();
}

/* ── eStruct200A008_redrawOnDirtyFlags_802CA82 ── */
/* eStruct200A008_redrawOnDirtyFlags_802CA82:
 * Refresh handler for the eStruct at 0x200A008: whenever its unk04 field is
 * non-zero, run the redraw pair decodePackedIdFields_802C54C + sub_802CAA6, then do the same
 * for unk06. Both fields are checked independently (both may trigger).
 * r4-r7 and everything else are passed through untouched. */
Next eStruct200A008_redrawOnDirtyFlags_802CA82(void) {
    GUEST_CALL(eStruct200A008_getUnk04);
    if (R0 != 0) {
        GUEST_CALL(decodePackedIdFields_802C54C);
        GUEST_CALL(sub_802CAA6);
    }

    GUEST_CALL(eStruct200A008_getUnk06);
    if (R0 != 0) {
        GUEST_CALL(decodePackedIdFields_802C54C);
        GUEST_CALL(sub_802CAA6);
    }

    RETURN_TO_CALLER();
}

/* ── battle_ClearAllianceTargetSlotIfSelf_802CDD0 ── */
/* battle_ClearAllianceTargetSlotIfSelf_802CDD0(r5 = battle object):
 * Clear this object's entry in the current-target slot of its alliance's
 * table (dword_203CFB0: two 0xC-byte alliance records, target pointer at +8).
 * Objects in the second alliance use the second record. Leaves the previous
 * slot value in r1 (0 if it was cleared, i.e. if it pointed at this object). */
Next battle_ClearAllianceTargetSlotIfSelf_802CDD0(void) {
    u32 table = A_dword_203CFB0;

    if (LD8(R5 + oBattleObject_Alliance) != 0)
        table += 0xC;

    R1 = LD32(table + 8);
    if (R1 == R5) {
        R1 = 0;
        ST32(table + 8, R1);
    }

    RETURN_TO_CALLER();
}

/* ── battleObject_DetachFromAllianceSlot_802CDE6 ── */
/* battleObject_DetachFromAllianceSlot_802CDE6: Detach the BattleObject pointed to by r5 from whatever
 * battle-state slot is currently holding it (so it stops being tracked).
 * r5 = BattleObject pointer, lr = return address.
 *
 * Two slots live back-to-back in the arena array at 0x203CFB0:
 *   [0x00] alliance 0's slot      [0x0C] alliance 1's slot
 * Each slot has a pointer at +4 (the current owner) and a flag/field at +8.
 * If r5 is the current owner of its alliance's slot, both the pointer and
 * the secondary field are cleared. */
Next battleObject_DetachFromAllianceSlot_802CDE6(void) {
    u32 slot_base = A_dword_203CFB0;
    u8 alliance = LD8(R5 + oBattleObject_Alliance);

    if ((s32)alliance == 0) {
        slot_base += 0xC;
    }

    if (LD32(slot_base + 4) == R5) {
        ST32(slot_base + 4, 0);
        ST32(slot_base + 8, 0);
    }

    RETURN_TO_CALLER();
}

/* ── wrapCountersAt203CFB0ToU16_802CDFE ── */
/* wrapCountersAt203CFB0ToU16_802CDFE: clear the upper halves of the two 32-bit counters at
 * dword_203CFB0 and dword_203CFB0+0xC, keeping only the low 16 bits. */
Next wrapCountersAt203CFB0ToU16_802CDFE(void) {
    u32 counter = A_dword_203CFB0;

    ST32(counter, LD32(counter) << 16);
    ST32(counter + 0xC, LD32(counter + 0xC) << 16);

    RETURN_TO_CALLER();
}

/* ── battleObject_accumulateFinalDamageUpperBound_802CE10 ── */
/* battleObject_accumulateFinalDamageUpperBound_802CE10(r0 = value, r5 = battle object):
 * Updates one of the two battle side-counters for the object's alliance.
 * If the object is the one currently tracked at (+8) of the counter block,
 * its stored increment (+2) is added to r0; otherwise r0 acts as a new
 * maximum and overwrites the counter (+0) when it is larger.
 * Returns: r0 = updated value, r2 = the tracked object pointer. */
Next battleObject_accumulateFinalDamageUpperBound_802CE10(void) {
    u32 amount = R0;
    u32 object = R5;

    /* Player-side block, or the +0xC partner-side block for allied objects. */
    u32 counters = A_dword_203CFB0;
    if (LD8(object + oBattleObject_Alliance) != 0)
        counters += 0xC;

    R2 = LD32(counters + 8);
    if (object == R2) {
        /* This object owns the counter: accumulate its increment into r0. */
        R0 = amount + LD16(counters + 2);
    } else {
        /* Raise the counter's recorded maximum if r0 exceeds it. */
        if (amount > LD16(counters))
            ST16(counters, amount);
        R0 = amount;
    }
    RETURN_TO_CALLER();
}

/* ── checkBattleOrderMatch_802CE30 ── */
Next checkBattleOrderMatch_802CE30(void) {
    u32 base = A_dword_203CFB0;
    if (!((u32)LD8(R5 + oBattleObject_Alliance) & 0xFF))
        base += 0xc;

    if (LD32(base + 4) == R5 && LD32(base + 8) != 0)
        R0 = 1;
    else
        R0 = 0;

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── zeroFill_802CE54 ── */
/* zeroFill_802CE54: clear the 0x20-byte work buffer unk_2036720.
 * Inputs: r2/r3 (preserved, forwarded unchanged). Flags on exit are
 * whatever ZeroFillByWord leaves behind (callers branch on c/v). */
Next zeroFill_802CE54(void) {
    R0 = A_unk_2036720; /* memBlock: buffer to clear */
    R1 = 0x20;          /* size in bytes */
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── battle_getStoryFlagFromBattleState_802CE60 ── */
/* battle_getStoryFlagFromBattleState_802CE60(r0 = index):
 * Fetch one 16-byte entry from the table at unk_2036720. The index is
 * scrambled by XORing it with the current battle state's shuffle key
 * (oBattleState_Unk_0d) before scaling it to the entry size, so callers
 * get a shuffled-but-deterministic entry for the current battle.
 * Returns r0 = entry halfword at +0, r1 = entry word at +4,
 *         r2 = entry halfword at +2. */
Next battle_getStoryFlagFromBattleState_802CE60(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 shuffleKey = LD8(battleState + oBattleState_Unk_0d);

    u32 entry = A_unk_2036720 + ((R0 ^ shuffleKey) * 0x10);

    R1 = LD32(entry + 4);
    R2 = LD16(entry + 2);
    R0 = LD16(entry);

    RETURN_TO_CALLER();
}

/* ── getTable2036720EntryFields_802CE78 ── */
/* getTable2036720EntryFields_802CE78(r0 = entry index):
 * Fetch a 16-byte record from the table at unk_2036720 and spread its fields
 * across registers: r0 = halfword at +0, r1 = word at +4, r2 = halfword at +2,
 * r3 = word at +8. The address computation sets c/v as the original ADDS does. */
Next getTable2036720EntryFields_802CE78(void) {
    u32 table = A_unk_2036720;
    u32 offset = R0 * 0x10;             /* each record is 16 bytes */

    SET_FLAGS_ADD(offset, table);       /* add r0, r0, r1 (sets c/v) */
    u32 entry = offset + table;

    R3 = LD32(entry + 8);
    R1 = LD32(entry + 4);
    R2 = LD16(entry + 2);
    R0 = LD16(entry);

    RETURN_TO_CALLER();
}

/* ── runPendingHandlerForEntryPair_802CEC8 ── */
/* runPendingHandlerForEntryPair_802CEC8: for each of the two entries in the table at unk_2036720
 * (stride 0x10, entry pointer at +0x8), if the entry exists and its
 * halfword flag at +0x24 is clear, call sub_802CEA6 with the byte value
 * at +0x16 of that entry. r4 is left pointing at the second entry. */
Next runPendingHandlerForEntryPair_802CEC8(void) {
    u32 table = A_unk_2036720;

    for (u32 i = 0; i < 2; i++) {
        u32 entry = LD32(table + i * 0x10 + 8);

        if (entry != 0 && LD16(entry + 0x24) == 0) {
            R0 = LD8(entry + 0x16);
            GUEST_CALL(sub_802CEA6);
        }
    }

    R4 = table + 0x10;
    RETURN_TO_CALLER();
}

/* ── hiveBlockHappensHere_802CEF4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r3; } call_GetAIDataUnk0x48Flag_result;
/* register form: generated wrapper: GetAIDataUnk0x48Flag has no typed C interface yet */
static inline call_GetAIDataUnk0x48Flag_result call_GetAIDataUnk0x48Flag(u32 r5) {
    R5 = r5;
    GUEST_CALL(GetAIDataUnk0x48Flag);
    return (call_GetAIDataUnk0x48Flag_result){ R0, R3 };
}

typedef struct { u32 r2, r3, r4, r5, r10; } call_PlaySoundEffect_result_hiveBlockHappensHere_802CEF4;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_hiveBlockHappensHere_802CEF4 call_PlaySoundEffect_hiveBlockHappensHere_802CEF4(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_hiveBlockHappensHere_802CEF4){ R2, R3, R4, R5, R10 };
}

typedef struct { u32 r0, r1, r2; } call_getTable2036720EntryFields_802CE78_result;
/* register form: generated wrapper: getTable2036720EntryFields_802CE78 has no typed C interface yet */
static inline call_getTable2036720EntryFields_802CE78_result call_getTable2036720EntryFields_802CE78(u32 r0) {
    R0 = r0;
    GUEST_CALL(getTable2036720EntryFields_802CE78);
    return (call_getTable2036720EntryFields_802CE78_result){ R0, R1, R2 };
}

typedef struct { u32 r0, r3; } call_GetAIData_Unk_44_Flag_result;
/* register form: generated wrapper: GetAIData_Unk_44_Flag has no typed C interface yet */
static inline call_GetAIData_Unk_44_Flag_result call_GetAIData_Unk_44_Flag(u32 r5) {
    R5 = r5;
    GUEST_CALL(GetAIData_Unk_44_Flag);
    return (call_GetAIData_Unk_44_Flag_result){ R0, R3 };
}

typedef struct { u32 r3; } call_SetAIData_Unk_44_Flag_result;
/* register form: generated wrapper: SetAIData_Unk_44_Flag has no typed C interface yet */
static inline call_SetAIData_Unk_44_Flag_result call_SetAIData_Unk_44_Flag(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(SetAIData_Unk_44_Flag);
    return (call_SetAIData_Unk_44_Flag_result){ R3 };
}

/* hiveBlockHappensHere_802CEF4
 * Evaluates a hive/antitarget collision and updates the AI flags/collision
 * damage buffer on a hive-block outcome. */

#define FLAG_HIVE_UP        0x00200000u
#define FLAG_HIVE_ACTIVE    0x00000800u
#define FLAG_AI_44_DONE     0x00008200u
#define FLAG_HIGH_OBJ_BITS  0xFF800000u
#define FLAG_HIVE_BROKEN    0x00000200u
#define FLAG_HIVE_REBUILT   0x00008000u
#define FLAG_ANTISWORD      0x00000400u
#define FLAG_SWORD_HIT      0x00002000u
#define FLAG_HIVE_INTACT    0x00020000u
#define MASK_OBJ_FLAGS2_CLR 0x000301BEu
#define MASK_COLL_HIVE_CLR  0x00000040u

#define ALLIANCE_TYPE_HIVE_BB   0xBB
#define ALLIANCE_TYPE_OTHER     0x157
#define ALLIANCE_TYPE_ANTISWORD 0xBC

static void clear_collision_damage(u32 c) {
    u32 panel = c + oCollisionData_PanelDamage1;
    ST16(panel + 0, 0);
    ST16(panel + 2, 0);
    ST16(panel + 4, 0);
    ST16(panel + 6, 0);
    ST16(panel + 8, 0);
    ST16(c + oCollisionData_Unk_8e, 0);
    ST16(c + oCollisionData_Unk_90, 0);
    ST16(c + oCollisionData_Unk_92, 0);
    ST32(c + 0x74, 0);
    ST8(c + oCollisionData_HitModifierFinal, 0);
    ST8(c + oCollisionData_StatusEffectFinal, 0);
    ST16(c + 0xA4, 0);
    ST16(c + oCollisionData_Unk_1c, 0);
    ST16(c + oCollisionData_Unk_2a, 0);
    ST16(c + oCollisionData_Unk_2c, 0);
    ST32(c + oCollisionData_FlagsFromCollision,
         LD32(c + oCollisionData_FlagsFromCollision) & ~MASK_COLL_HIVE_CLR);
    ST32(c + oCollisionData_ObjectFlags2,
         LD32(c + oCollisionData_ObjectFlags2) & ~MASK_OBJ_FLAGS2_CLR);
}

static void hive_block(u32 object) {
    u32 c = LD32(object + oBattleObject_CollisionDataPtr);

    /* --- Hive-up path: riskyhoney panel -- */
    u32 f48 = call_GetAIDataUnk0x48Flag(object).r0;
    if ((f48 & FLAG_HIVE_UP) != 0
        && LD16(c + oCollisionData_PanelDamage2) == 0) {
        u32 p = c + oCollisionData_PanelDamage1;
        u32 or_dmg = LD16(p) | LD16(p + 4) | LD16(p + 6) | LD16(p + 8);
        if (or_dmg != 0) {
            u32 aidata = LD32(object + oBattleObject_AIDataPtr) + 0xA0;
            ST32(aidata + oAIAttackVars_Unk_30, 1);
            (void)call_PlaySoundEffect_hiveBlockHappensHere_802CEF4(0x6E, or_dmg);
            clear_collision_damage(c);
            return;
        }
        clear_collision_damage(c);
        return;
    }

    /* --- Table-driven hive path --- */
    u32 alliance = LD8(object + oBattleObject_Alliance);
    u32 entry = call_getTable2036720EntryFields_802CE78(alliance).r0;

    u32 hive_type;
    u32 threshold;
    if (entry == ALLIANCE_TYPE_HIVE_BB || entry == ALLIANCE_TYPE_OTHER) {
        hive_type = entry;
        threshold = 0xA;
    } else {
        u32 f = call_GetAIDataUnk0x48Flag(object).r0;
        if ((f & FLAG_HIVE_ACTIVE) != 0) {
            hive_type = ALLIANCE_TYPE_HIVE_BB;
            threshold = 1;
        } else {
            /* Not a hive type and not active: check AntiSword */
            if (entry != ALLIANCE_TYPE_ANTISWORD)
                return;
            u32 f44 = call_GetAIData_Unk_44_Flag(object).r0;
            if ((f44 & FLAG_ANTISWORD) != 0) {
                clear_collision_damage(c);
                return;
            }
            u32 col_flags = LD32(c + oCollisionData_FlagsFromCollision);
            if ((col_flags & FLAG_SWORD_HIT) == 0)
                return;
            if ((col_flags & FLAG_HIVE_INTACT) != 0)
                return;
            (void)call_SetAIData_Unk_44_Flag(FLAG_ANTISWORD, object);
            clear_collision_damage(c);
            return;
        }
    }

    /* --- Shared hive-broken logic --- */
    u32 f44 = call_GetAIData_Unk_44_Flag(object).r0;
    if ((f44 & FLAG_AI_44_DONE) != 0) {
        clear_collision_damage(c);
        return;
    }
    u32 col_flags = LD32(c + oCollisionData_FlagsFromCollision);
    if ((col_flags & FLAG_HIGH_OBJ_BITS) == 0)
        return;

    u32 dmg_base = c + 0x82;
    u32 sum = LD16(dmg_base) + LD16(dmg_base + 2) + LD16(dmg_base + 4)
            + LD16(dmg_base + 6) + LD16(dmg_base + 8);

    if ((s32)sum >= (s32)threshold) {
        u32 flag = (hive_type == ALLIANCE_TYPE_HIVE_BB)
                   ? FLAG_HIVE_BROKEN : FLAG_HIVE_REBUILT;
        (void)call_SetAIData_Unk_44_Flag(flag, object);
        clear_collision_damage(c);
        return;
    }
    if (threshold == 1) {
        clear_collision_damage(c);
        return;
    }
}

Next hiveBlockHappensHere_802CEF4(void) {
    hive_block(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_handleImpact_802CFF8 ── */
/* Returns true if the battle object has the 0x40 bit set in its collision
 * data field at offset 0x76 (the "shockwave / impact" trigger flag). */
static bool has_impact_flag(u32 battle_object) {
    u32 collision_data = LD32(battle_object + oBattleObject_CollisionDataPtr);
    u8 flags = LD8(collision_data + 0x76);
    return (flags & 0x40) != 0;
}

/* register form: calls getTable2036720EntryFields_802CE78 outside this file,
 * which has no typed C interface yet */
static u32 get_table_entry_fields(u8 alliance) {
    R0 = alliance;
    GUEST_CALL(getTable2036720EntryFields_802CE78);
    return R0;
}

/* register form: calls sub_802CEA6 outside this file, which has no typed C
 * interface yet */
static void apply_impact_to_table(u8 alliance) {
    R0 = alliance;
    GUEST_CALL(sub_802CEA6);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed
 * C interface yet */
static void play_sound_effect(u32 effect_id) {
    R0 = effect_id;
    GUEST_CALL(PlaySoundEffect);
}

/* Handle the "impact" reaction for a battle object: if the impact flag in its
 * collision data is set, look up its alliance in the table; if an entry is
 * found, apply the impact effect and play the impact sound. */
static void handle_battle_object_impact(u32 battle_object) {
    if (!has_impact_flag(battle_object))
        return;

    u8 alliance = LD8(battle_object + oBattleObject_Alliance);
    if (get_table_entry_fields(alliance) == 0)
        return;

    apply_impact_to_table(alliance);
    play_sound_effect(0x8E);
}

Next battleObject_handleImpact_802CFF8(void) {
    handle_battle_object_impact(R5);
    /* Original's final instruction is a function epilogue (pop {r4,pc}); it
     * leaves flags from PlaySoundEffect (c/v) and leaves R2 untouched. */
    RETURN_TO_CALLER();
}

/* ── getTableEntryPtr32_802D064 ── */
/* getTableEntryPtr32_802D064(r0 = entry index) -> r0 = &byte_203C4A0[entry * 0x20]
 *
 * Address-of helper: computes the address of one 0x20-byte entry in the
 * array at byte_203C4A0. In Thumb, the low-register "add r0, r0, r1" is
 * always ADDS, so this last instruction (not the MUL) is what determines
 * the flags returned to callers: N/Z from the sum, C from the carry out
 * of the addition, V from the signed overflow. */
Next getTableEntryPtr32_802D064(void) {
    u32 product = R0 * 0x20;
    u32 base = A_byte_203C4A0;

    SET_FLAGS_ADD(product, base);  /* Thumb ADD r0, r0, r1 == ADDS */
    R0 = product + base;
    RETURN_TO_CALLER();
}

/* ── initStateCounters100_802D08C ── */
/* initStateCounters100_802D08C(r0 = selector flag passed through to getTableEntryPtr32_802D064):
 * Initialise a state struct: clear the byte at +0x10 and set the two
 * bytes at +0xD and +0xF to 100. Returns the struct pointer in r0 and
 * r1 = 100 (flags reflect r1). */
Next initStateCounters100_802D08C(void) {
    /* getTableEntryPtr32_802D064 resolves the struct to initialise and returns it in r0. */
    GUEST_CALL(getTableEntryPtr32_802D064);
    u32 state = R0;

    ST8(state + 0x10, 0);
    ST8(state + 0xD, 100);
    ST8(state + 0xF, 100);

    R0 = state;
    R1 = 100;
    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── sub_802D150 ── */
/* sub_802D150: clear the 32-bit counter/flag at dword_2006740. */
Next sub_802D150(void) {
    ST32(A_dword_2006740, 0);
    RETURN_TO_CALLER();
}

/* ── GetBattleModeFromBattleSettings ── */
/* GetBattleModeFromBattleSettings(r0 = battle settings pointer):
 * return the battle mode byte stored at offset 3 of the settings struct. */
Next GetBattleModeFromBattleSettings(void) {
    R0 = LD8(R0 + 3);
    RETURN_TO_CALLER();
}

/* ── GetBattleEffects ── */
/* GetBattleEffects: 0802D246-0802D254 THUMB, bn6f asm03_0.s:14417
 * Fetches the battle-effects bitmask for the current battle: load the
 * battle state from the toolkit, its battle settings from the battle
 * state, and delegate to GetBattleEffectsFromBattleSettings. */
Next GetBattleEffects(void) {
    u32 *battleState = (u32 *)LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD32((u32)battleState + oBattleState_BattleSettings);
    GUEST_CALL(GetBattleEffectsFromBattleSettings);
    RETURN_TO_CALLER();
}

/* ── GetBattleEffectsFromBattleSettings ── */
/* GetBattleEffectsFromBattleSettings(r0 = battle settings ptr):
 * returns the optional-effect settings word from the battle settings struct. */
Next GetBattleEffectsFromBattleSettings(void) {
    R0 = LD32(R0 + 8 /* oBattleSettings_UnkOptSettings */);
    RETURN_TO_CALLER();
}

/* ── GetBattleSettingsUnk01 ── */
/* GetBattleSettingsUnk01():
 * Return the settings field (Unk01) of the current battle's BattleSettings,
 * fetched via GetBattleSettingsUnk01FromBattleSettings. */
Next GetBattleSettingsUnk01(void) {
    u32 battleSettings = LD32(LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_BattleSettings);

    R0 = battleSettings;
    GUEST_CALL(GetBattleSettingsUnk01FromBattleSettings);
    /* result already in R0 */
    RETURN_TO_CALLER();
}

/* ── GetBattleSettingsUnk01FromBattleSettings ── */
/* GetBattleSettingsUnk01FromBattleSettings(r0 = battle settings pointer):
 * return the byte at offset 1 (unknown setting 01) of the battle settings struct. */
Next GetBattleSettingsUnk01FromBattleSettings(void) {
    R0 = LD8(R0 + 1);
    RETURN_TO_CALLER();
}

/* ── GetBattlePanelColumnPattern ── */
/* GetBattlePanelColumnPattern:
 * Returns the current battle panel column pattern, reading it from the
 * battle settings of the battle state the toolkit (r10) points at. */
Next GetBattlePanelColumnPattern(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD32(battleState + oBattleState_BattleSettings);
    GUEST_CALL(GetBattlePanelColumnPatternFromBattleSettings);
    RETURN_TO_CALLER();
}

/* ── GetBattlePanelColumnPatternFromBattleSettings ── */
/* GetBattlePanelColumnPatternFromBattleSettings(r0 = battle settings ptr):
 * returns the panel column pattern byte (offset 6) from the battle settings. */
Next GetBattlePanelColumnPatternFromBattleSettings(void) {
    R0 = LD8(R0 + 6);
    RETURN_TO_CALLER();
}

/* ── memBlock_ClearWorkArea_802D638 ── */
/* memBlock_ClearWorkArea_802D638: clear the 0x20-byte work buffer at byte_2000070 (memBlock).
 * Arguments in r2/r3 and those registers are passed through unchanged. */
Next memBlock_ClearWorkArea_802D638(void) {
    R0 = A_byte_2000070; /* memBlock */
    R1 = 0x20;           /* size in bytes */
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── battleObject_incrementCollisionRegion_802D644 ── */
/* battleObject_incrementCollisionRegion_802D644(r0 = index into byte_2000070):
 * Increment the counter byte at byte_2000070[index], clamped at 0xFF
 * (0xFF stays 0xFF instead of wrapping). r2 = incremented (possibly
 * clamped) value; flags reflect the comparison against 0xFF. */
Next battleObject_incrementCollisionRegion_802D644(void) {
    u32 value = (u32)LD8(A_byte_2000070 + R0) + 1;

    SET_FLAGS_CMP(value, 0xFF);
    if ((s32)value <= 0xFF)
        ST8(A_byte_2000070 + R0, value);

    R2 = value;
    RETURN_TO_CALLER();
}

/* ── battle_initPlayerTracker_802D6A0 ── */
/* battle_initPlayerTracker_802D6A0: initialise the battle's player-tracking block (dword_203C970)
 * at the start of a battle:
 *   +0x0: counter, cleared
 *   +0x4: flag, set to 1
 *   +0x8: BattleObject pointer of the player's side   (alliance 0)
 *   +0xC: BattleObject pointer of the enemy's side    (alliance 1)
 * r4-r7/r10 are preserved; r0-r2 and flags are scratch. */
Next battle_initPlayerTracker_802D6A0(void) {
    u32 tracker = A_dword_203C970;

    ST32(tracker, 0);

    R0 = 0;                                  /* player's side */
    GUEST_CALL(battle_findPlayer);
    ST32(tracker + 8, R0);

    R0 = 1;                                  /* enemy's side */
    GUEST_CALL(battle_findPlayer);
    ST32(tracker + 0xC, R0);

    ST8(tracker + 4, 1);

    RETURN_TO_CALLER();
}

/* ── sub_802D6C4 ── */
/* register form: calls sub_802DD1E outside this file, which has no typed C interface yet */
static u32 sub_802DD1E_typed(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_802DD1E);
    return R0;
}

/* register form: calls sub_802DD10 outside this file, which has no typed C interface yet */
static void sub_802DD10_typed(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_802DD10);
}

/* register form: calls sub_802DD4A outside this file, which has no typed C interface yet */
static u32 sub_802DD4A_typed(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_802DD4A);
    return R0;
}

/* tick drive on the two A_dword_203C970-state objects.
 * If the state byte at +3 is 0, run sub_802DD1E on each of the two pointers (base+8, base+0xC),
 * calling sub_802DD10 on whichever returns nonzero, then mark the gate byte +3 with 4.
 * Otherwise poll both with sub_802DD4A; if both return zero, clear byte +4 to 0.
 * Returns byte_203C974 (state at base+4) in r0, with NZ set as for a plain load. */
static u32 tick_state(u32 base) {
    u8 gate = LD8(base + 3);

    if (gate == 0) {
        u32 a = LD32(base + 8);
        if (sub_802DD1E_typed(a) != 0)
            sub_802DD10_typed(a);

        u32 b = LD32(base + 0xC);
        if (sub_802DD1E_typed(b) != 0)
            sub_802DD10_typed(b);

        ST8(base + 3, 4);
    } else {
        u32 a = LD32(base + 8);
        if (sub_802DD4A_typed(a) != 0)
            return LD8(base + 4);
        u32 c = LD32(base + 0xC);
        if (sub_802DD4A_typed(c) != 0)
            return LD8(base + 4);
        ST8(base + 4, 0);
    }
    return LD8(base + 4);
}

Next sub_802D6C4(void) {
    u32 result = tick_state(A_dword_203C970);
    R0 = result;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── ai_CheckSpecialFlagPair_802DCEC ── */
/* ai_CheckSpecialFlagPair_802DCEC(r0 = AI/battle parameter):
 * Decides a special condition for the given parameter. Returns 0 if the
 * parameter is 0; otherwise returns 1 if the AI data +0x48 flag has bit
 * 0x1000 set or the AI data +0x44 flag has bit 0x4000000 set, else 0.
 *
 * Register notes: the original borrows r5 and r7 as scratch (r5 = parameter,
 * r7 = running result) but pops both restored, so the caller sees its incoming
 * r5/r7 unchanged. The flag getters are leaf calls that still read the live
 * scratch state: the first is entered with r1 = incoming r1, r5 = parameter,
 * r7 = 0; the second with r1 = 0x1000 (the =0x4000000 load happens only after
 * that call returns), r5 = parameter, r7 = 0. r1 ends up holding the mask of
 * the last flag tested (untouched on the parameter == 0 path), and r3 holds
 * whatever the flag getters left there.
 */
Next ai_CheckSpecialFlagPair_802DCEC(void) {
    u32 param = R0;
    u32 savedR5 = R5, savedR7 = R7;

    /* Scratch state live across the flag-getter calls */
    R5 = param;
    R7 = 0;

    u32 result;
    if (param == 0) {
        /* "beq loc_802DD0C": parameter 0 short-circuits to "no" */
        result = 0;
    } else {
        result = 1;

        /* flag at AI data + 0x48: bit 0x1000 keeps the "yes" verdict */
        GUEST_CALL(GetAIDataUnk0x48Flag);
        R1 = 0x1000;
        if ((R0 & 0x1000) == 0) {
            /* flag at AI data + 0x44: bit 0x4000000 keeps the "yes" verdict */
            GUEST_CALL(GetAIData_Unk_44_Flag); /* still called with r1 = 0x1000 */
            R1 = A_LCDControl;
            if ((R0 & A_LCDControl) == 0)
                result = 0;
        }
    }

    /* pop {r5,r7}: caller's values are restored */
    R5 = savedR5;
    R7 = savedR7;

    R0 = result;
    /* The closing "mov r0, r7" assembles to adds r0, r7, #0, the last
     * flag-setting instruction on every path: it clears C and V. */
    SET_FLAGS_ADD(R0, 0);
    RETURN_TO_CALLER();
}

/* ── sub_802DD1E ── */
/* sub_802DD1E: stub that always yields 0.
 * The original clears r7, copies it into r0 (an "adds r0, r7, #0", which is why
 * the flags come out as Z set / C, V clear) and immediately returns; the pushes
 * of r5/r7 are undone by the pop, so both are preserved untouched.
 *   inputs:  r0 (ignored)          outputs: r0 = 0, flags nz/c/v
 *   preserved: r5 r7 */
Next sub_802DD1E(void) {
    R0 = 0;
    SET_FLAGS_ADD(0, 0);   /* "adds r0, r7, #0" with r7 = 0: N=0, Z=1, C=0, V=0 */
    RETURN_TO_CALLER();
}

/* ── battleObject_runAIScript_802DE5C ── */
/* battleObject_runAIScript_802DE5C: if test battle flag 0x40 is set, run sub_802E2C4 for
 * indices 0 and 1. Otherwise do nothing. */
Next battleObject_runAIScript_802DE5C(void) {
    GUEST_CALL(TestBattleFlag_0x40); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) { /* the flag is set */
        R0 = 0;
        GUEST_CALL(sub_802E2C4);
        R0 = 1;
        GUEST_CALL(sub_802E2C4);
    }
    RETURN_TO_CALLER();
}

/* ── battle_setupAllianceEntryKind3_802DFC8 ── */
/* battle_setupAllianceEntryKind3_802DFC8(r5 = battle object):
 * Allocate (and zero-fill) a fresh entry in the shared entry pool for this
 * object's alliance, then — if battle flag 0x40 is set — initialize it as an
 * entry of kind 3 anchored at the object's panel x position, and notify
 * sub_802E07C about the alliance.
 * The entry pointer is also left in r4 for callers that inspect it. */
Next battle_setupAllianceEntryKind3_802DFC8(void) {
    u32 alliance = LD8(R5 + oBattleObject_Alliance);

    R0 = alliance;
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);
    u32 entry = R0;

    /* Clear the whole 0x1D0-byte entry. */
    R0 = entry;
    R1 = 0xE8 + 0xE8; /* 0x1D0 bytes */
    GUEST_CALL(ZeroFillByWord);

    GUEST_CALL(battle_testFlag0x40_800A8F8); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        ST8(entry + 0x00, 1);            /* active */
        ST8(entry + 0x0B, 0xFF);
        ST8(entry + 0x10, 1);
        ST8(entry + 0x11, LD8(R5 + oBattleObject_PanelX)); /* panel x */
        ST8(entry + 0x0E, 3);            /* kind */

        R0 = LD8(R5 + oBattleObject_Alliance);
        GUEST_CALL(sub_802E07C);
    }

    RETURN_TO_CALLER();
}

/* ── battle_initWorkArea_802DFFC ── */
/* battle_initWorkArea_802DFFC: initialize the battle work area.
 * Zeroes the 0x3A0-byte battle work buffer, records the battle object table
 * pointer in the battle state, and zeroes an allied structure. r2/r3/r4 are
 * untouched throughout. */
Next battle_initWorkArea_802DFFC(void) {
    /* clear the whole battle work buffer: 0xE8 words = 0x3A0 bytes */
    R0 = A_unk_2036120;
    R1 = 0xE8 << 2;
    GUEST_CALL(ZeroFillByWord);

    /* battle state's object-table pointer field = battle object 0 */
    R0 = A_eT1BattleObject0;
    ST32(A_unk_20362F0 + 0x44, R0); /* dword_2036334 */

    GUEST_CALL(zeroFill_802E544);

    RETURN_TO_CALLER();
}

/* ── getEntryPtr_2036120_1D0_802E070 ── */
/* getEntryPtr_2036120_1D0_802E070(r0 = entry index): return the address of entry `r0` in the
 * array at unk_2036120, whose elements are 0x1D0 bytes each
 * (0xE8 * 2). Leaves r0 = base + index*0x1D0 and r1 = base; flags are
 * those of the final address addition. */
Next getEntryPtr_2036120_1D0_802E070(void) {
    u32 offset = R0 * (0xE8 + 0xE8);

    R1 = A_unk_2036120;
    R0 = offset + A_unk_2036120;
    SET_FLAGS_ADD(offset, A_unk_2036120);
    RETURN_TO_CALLER();
}

/* ── TestBattleFlag_0x40 ── */
/* TestBattleFlag_0x40: test the 0x40 bit of the current battle flags.
 * Returns the masked value in r0 (nonzero if the flag is set) and leaves
 * the zero/negative flags reflecting it, so callers can branch directly. */
Next TestBattleFlag_0x40(void) {
    GUEST_CALL(battle_getFlags);   /* r0 = battle_getFlags() */

    R1 = 0x40;
    R0 = R0 & R1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── sub_802E112 ── */
/* Flag bit (from battle_setFlags) set on the battle state when the run qualifies. */
#define BATTLE_FLAG_0x40  0x40

/* Effect bit in GetBattleEffects() checked at offset 0x21 (0x200000). */
#define BATTLE_EFFECT_0x200000  0x200000

/* Network-battle effect bit in GetBattleEffects(). */
#define BATTLE_EFFECT_NETWORK  0x08

/* Event flag queried by TestEventFlagFromImmediate when not network-flagged. */
#define EVENT_1722_BEAST_LINK_GATE_RELATED  0x1722

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 call_GetBattleMode(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 call_GetBattleEffects(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls notZero_eByte200AD04 outside this file, which has no typed C interface yet.
 * Returns true if the gate-related counter/byte at 0x200AD04 is non-zero. */
static bool call_notZero_eByte200AD04(void) {
    GUEST_CALL(notZero_eByte200AD04);
    return R0 != 0;
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet.
 * TestEventFlagFromImmediate answers in flags: ZF set if the flag is clear. */
static bool call_TestEventFlagFromImmediate(u32 hi, u16 lo) {
    R0 = hi;
    R1 = lo;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls battle_setFlags outside this file, which has no typed C interface yet */
static void call_battle_setFlags(u32 flags) {
    R0 = flags;
    GUEST_CALL(battle_setFlags);
}

static void maybe_set_battle_flag_0x40(void) {
    /* Skip when the current mode disallows it. */
    u32 mode = call_GetBattleMode();
    if (mode == 1 || mode == 7 || mode == 9 || mode == 6)
        return;

    /* Skip while the 0x200000 effect is active. */
    if (call_GetBattleEffects() & BATTLE_EFFECT_0x200000)
        return;

    /* Set the flag if the gate byte is non-zero, or this is a network battle with the gate event set. */
    bool qualify = call_notZero_eByte200AD04();
    if (!qualify) {
        if ((call_GetBattleEffects() & BATTLE_EFFECT_NETWORK) == 0)
            return;
        if (!call_TestEventFlagFromImmediate(EVENT_1722_BEAST_LINK_GATE_RELATED >> 16,
                                             (u16)EVENT_1722_BEAST_LINK_GATE_RELATED))
            return;
    }

    call_battle_setFlags(BATTLE_FLAG_0x40);
}

/* sub_802E112: gate control for setting battle flag 0x40.
 *   Skips in crossover, tomahawk, dustman, or virus-battler modes, and while the
 *   0x200000 battle effect is active. Otherwise sets flag 0x40 via battle_setFlags,
 *   either immediately (if the gate byte is non-zero) or after confirming this is a
 *   network battle with event flag 0x1722 set. */
Next sub_802E112(void) {
    maybe_set_battle_flag_0x40();
    RETURN_TO_CALLER();
}

/* ── battle_DecrementAllianceTimer_802E1D8 ── */
/* battle_DecrementAllianceTimer_802E1D8: tick down the timer field (+0x30) of the record that
 * getEntryPtr_2036120_1D0_802E070 returns for the alliance of the battle object in r5.
 * r1 = the timer's value after the update (0 if it was already 0). */
Next battle_DecrementAllianceTimer_802E1D8(void) {
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);

    u32 timer = LD16(R0 + 0x30);
    if (timer != 0) {
        timer--;
        ST16(R0 + 0x30, timer);
    }
    R1 = timer;
    RETURN_TO_CALLER();
}

/* ── loadIndexedPaletteToBuffer_802E240 ── */
/* loadIndexedPaletteToBuffer_802E240(r0 = palette index):
 * Copies one 60-halfword palette (0x3C halfwords) into the buffer at unk_2008170.
 * The source palette is picked from the word-sized table at off_802E260 by index. */
Next loadIndexedPaletteToBuffer_802E240(void) {
    u32 index = R0;

    u32 src = LD32(A_off_802E260 + index * 4);
    u32 dest = A_unk_2008170;

    R0 = src;
    R1 = dest;
    R2 = 0x3C;
    GUEST_CALL(CopyHalfwords); /* (u16 *src, u16 *dest, int halfwordCount) */

    RETURN_TO_CALLER();
}

/* ── battle_getAllianceEntryStatus_802E4B8 ── */
/* battle_getAllianceEntryStatus_802E4B8(r5 = battle object):
 * Fetch the alliance entry for the battle object's alliance slot and return its
 * status code in r0: 9 if the byte at entry+0x50 is set, 13 if the byte at
 * entry+0x54 is set, otherwise 1. r7 is only used as scratch internally and is
 * restored on return (the original pushes/pops it). r1 = 0x54 is left behind on
 * the paths that read the +0x54 flag byte. */
Next battle_getAllianceEntryStatus_802E4B8(void) {
    /* r0 = this object's alliance slot index */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);

    u32 entry = R0;

    if (LD8(entry + 0x50) != 0) {
        R0 = 9;
        RETURN_TO_CALLER();
    }

    R1 = 0x54;
    R0 = (LD8(entry + 0x54) != 0) ? 0xD : 1;
    RETURN_TO_CALLER();
}

/* ── sub_802E4E4 ── */
/* One-shot bits in the AI-data flag word read by GetAIData_Unk_44_Flag. */
#define AIDATA_FLAG_TIMER  0x02000000u /* literal "timer_2000000" in the disassembly */
#define AIDATA_FLAG_INVULN 0x20000000u /* drives the permanent-invulnerability branch */

/* register form: calls getEntryPtr_2036120_1D0_802E070 outside this file, which has no typed C interface yet */
static u32 alliance_entry_ptr(u32 alliance) {
    R0 = alliance;
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);
    return R0;
}

/* register form: calls GetAIData_Unk_44_Flag outside this file, which has no typed C interface yet */
static u32 ai_data_flags(void) {
    GUEST_CALL(GetAIData_Unk_44_Flag);
    return R0;
}

/* register form: calls ClearAIData_Unk_44_Flag outside this file, which has no typed C interface yet */
static void clear_ai_data_flags(u32 mask) {
    R0 = mask;
    GUEST_CALL(ClearAIData_Unk_44_Flag);
}

/* register form: calls sub_802E1EC outside this file, which has no typed C interface yet */
static void post_entry_write_update(void) {
    /* the original runs this right after each entry-field write below */
    GUEST_CALL(sub_802E1EC);
}

/* register form: calls object_setInvulnerableTime outside this file, which has no typed C interface yet */
static void set_invulnerable_time(u32 frames) {
    R0 = frames;
    GUEST_CALL(object_setInvulnerableTime);
}

/* register form: calls sub_802D310 outside this file, which has no typed C interface yet */
static void post_invulnerability_update(void) {
    /* the original runs this only after granting invulnerability below */
    GUEST_CALL(sub_802D310);
}

/* Consume two one-shot AI-data flag bits on behalf of a battle object, applying
 * each one to the entry for the object's alliance (0x1D0-byte entries, table at
 * 0x2036120):
 *   AIDATA_FLAG_TIMER:  entry byte 0x50 = 1, then the entry post-update.
 *   AIDATA_FLAG_INVULN: entry byte 0x54 = 1, post-update, load the entry's 0x30
 *                       halfword timer with 0x1E0 (480), make the object
 *                       invulnerable indefinitely, then the follow-up update.
 * Both bits are checked even if the first one already fired. */
static void handle_ai_data_flag_triggers(u32 object) {
    u32 entry = alliance_entry_ptr(LD8(object + oBattleObject_Alliance));

    if (ai_data_flags() & AIDATA_FLAG_TIMER) {
        clear_ai_data_flags(AIDATA_FLAG_TIMER);
        ST8(entry + 0x50, 1);
        post_entry_write_update();
    }

    if (ai_data_flags() & AIDATA_FLAG_INVULN) {
        clear_ai_data_flags(AIDATA_FLAG_INVULN);
        ST8(entry + 0x54, 1);
        post_entry_write_update();
        ST16(entry + 0x30, 0x1E0);      /* 480-frame timer */
        set_invulnerable_time(0xFFFF);  /* effectively unlimited */
        post_invulnerability_update();
    }
}

/* sub_802E4E4(r5 = battle object) -> r2, r3 (leftovers from the last callee above) */
Next sub_802E4E4(void) {
    handle_ai_data_flag_triggers(R5);
    RETURN_TO_CALLER();
}

/* ── zeroFill_802E544 ── */
/* zeroFill_802E544: clear the 0x170-byte buffer at unk_20000A0.
 * r2 is untouched: it is an input forwarded to ZeroFillByByte and must be preserved. */
Next zeroFill_802E544(void) {
    R0 = A_unk_20000A0;
    R1 = 0x170; /* 0x71 + 0xFF as computed by the original */
    GUEST_CALL(ZeroFillByByte);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchDestroyEvents_802EF5C ── */
/* battle_dispatchDestroyEvents_802EF5C: run a check (battle_testFlag0x40_800A8F8); if it reports "active", pump the
 * sub_802EF74 state machine with argument 0 followed by argument 1.
 * Flags/results are whatever the last executed call (or the zero check) left. */
Next battle_dispatchDestroyEvents_802EF5C(void) {
    GUEST_CALL(battle_testFlag0x40_800A8F8);

    if (R0 == 0) {
        /* cmp r0, #0 from the original is the last flag-setting op on this path */
        SET_FLAGS_CMP(R0, 0);
        RETURN_TO_CALLER();
    }

    R0 = 0;
    GUEST_CALL(sub_802EF74);

    R0 = 1;
    GUEST_CALL(sub_802EF74);

    RETURN_TO_CALLER();
}

/* ── toolkit_ClearAllEventFlags_802F0C4 ── */
/* toolkit_ClearAllEventFlags_802F0C4(r10 = toolkit):
 * Clears the toolkit's event-flags array (630 halfword entries).
 * r2 and r3 pass through unchanged. */
Next toolkit_ClearAllEventFlags_802F0C4(void) {
    R0 = LD32(R10 + oToolkit_EventFlagsPtr);
    R1 = 630 * 2;
    GUEST_CALL(ZeroFillByHalfword);
    RETURN_TO_CALLER();
}

/* ── eventflags_ClearTwoHalfwordsAt2DE_802F0D8 ── */
/* eventflags_ClearTwoHalfwordsAt2DE_802F0D8:
 * Clears two halfwords of the event flags area (offset 0x2DE in the
 * toolkit's event flags buffer). Common "reset a group of event bits"
 * routine. r2/r3 are preserved; nothing meaningful is returned. */
Next eventflags_ClearTwoHalfwordsAt2DE_802F0D8(void) {
    u32 eventFlags = LD32(R10 + oToolkit_EventFlagsPtr);

    R0 = eventFlags + 0x2DE;
    R1 = 2;
    GUEST_CALL(ZeroFillByHalfword);

    RETURN_TO_CALLER();
}

/* ── eventFlags_ClearRangeAt2C8_802F0F4 ── */
/* eventFlags_ClearRangeAt2C8_802F0F4: clear a stretch of event flags.
 * Zeroes 0x16 halfwords starting at toolkit->eventFlags + 0x2C8. */
Next eventFlags_ClearRangeAt2C8_802F0F4(void) {
    u32 flags = LD32(R10 + oToolkit_EventFlagsPtr);

    R0 = flags + 0x2C8;
    R1 = 0x16;
    GUEST_CALL(ZeroFillByHalfword);

    RETURN_TO_CALLER();
}

/* ── addMail_802f238 ── */
/* addMail_802f238(r0 = mail id):
 * Records a newly received mail. Sets the mail's event flag and its
 * companion flag (id + 0x80), then inserts the mail's inbox slot id
 * (mail id - 0x1CA0) at the front of the inbox entry list (byte_2006530),
 * shifting every existing entry up one slot, and increments the inbox
 * entry count (dword_2001140).
 *
 * On return: r0 = final write index (count minus copies performed),
 * r1 = final read index after its last decrement (0x1CA0 if the inbox was
 * empty), r2 = inbox list address, r3 = last byte copied (whatever the
 * second SetEventFlag left behind when the inbox was empty), c/v from the
 * `count + 1` increment. r4/r6/r7 are preserved. */
Next addMail_802f238(void) {
    u32 mailId = R0;

    R0 = mailId;
    GUEST_CALL(SetEventFlag);           /* "mail received" flag */
    R0 = mailId + 0x80;
    GUEST_CALL(SetEventFlag);           /* companion flag for the same mail */

    u32 slot     = mailId - 0x1CA0;     /* inbox slot index of this mail */
    u32 list     = A_byte_2006530;      /* inbox entries, newest first */
    u32 countPtr = A_dword_2001140;     /* number of entries in the inbox */

    u32 count = LD32(countPtr);
    u32 dst   = count;                  /* r0: index written to */
    if (count != 0) {
        /* Make room at slot 0: copy each entry one slot up, starting from
         * the back. The original copies first and steps the read index down
         * afterwards, looping back while the *old* index is >= 1 signed
         * (sub r1, #1; bge), so it always copies at least once — even when
         * count's signed value is negative. */
        u32 src = count - 1;            /* r1: index read from */
        for (;;) {
            u32 entry = LD8(list + src);
            ST8(list + dst, entry);
            dst--;
            R3 = entry;
            s32 prev = (s32)src;
            src--;
            if (prev < 1)
                break;                  /* sub r1,#1; bge fails */
        }
        R1 = src;
    } else {
        R1 = 0x1CA0;
    }

    ST8(list, (u8)slot);                /* the new mail goes to the front */
    u32 newCount = count + 1;
    ST32(countPtr, newCount);
    SET_FLAGS_ADD(count, 1);            /* flags come from the count increment */

    R0 = dst;
    R2 = list;
    RETURN_TO_CALLER();
}
