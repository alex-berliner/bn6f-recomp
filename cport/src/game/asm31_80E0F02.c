/*
 * asm31_80E0F02.c: readable functions of bn6f asm31.s, 0x080E0F02-0x080EBE00, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── spawnChargeShotChargeObject_80E0F02 ── */
/* spawnChargeShotChargeObject_80E0F02(r5 = owner object, r7 = result slot):
 * Spawns the charge-up effect object that appears while a charge shot is
 * building (object_spawnType4, 8-byte object). The spawner hands the new
 * object back in r5. If it spawned, the effect is linked to its owner
 * (RelatedObject1Ptr), stored into *r7, and remembers r7 in ExtraVars so it
 * can clear that slot when it dies; its header flags also get bit 4 set.
 * The caller's r5 is preserved (the original push/pop {r5} around the call
 * exists because the spawner clobbers it). NZ reflect the new object pointer,
 * from the "mov r0, r5" null check that gates the linking; v and c are left
 * untouched, as in the original (movs r0, r5 = lsls #0 does not set them). */


Next spawnChargeShotChargeObject_80E0F02(void) {
    u32 owner = R5;       /* object the charge effect belongs to */
    u32 resultSlot = R7;  /* where the caller keeps track of the effect */

    R0 = 8;
    GUEST_CALL(object_spawnType4);

    u32 newObject = R5;      /* spawner returns the new object in r5 */
    SET_FLAGS_NZ(newObject); /* mov r0, r5: null check for the branch below */
    R5 = owner;              /* restore caller's r5 (original pop {r5}) */

    if (newObject != 0) {
        ST32(newObject + oBattleObject_RelatedObject1Ptr, owner);
        ST32(resultSlot, newObject);
        ST32(newObject + oBattleObject_ExtraVars, resultSlot);
        ST8(newObject + oObjectHeader_Flags,
            LD8(newObject + oObjectHeader_Flags) | 4);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80E0F22 ── */
/* sub_80E0F22: clear the field at r0+0x64. */
Next sub_80E0F22(void) {
    R1 = 0;
    ST32(R0 + 0x64, R1);
    RETURN_TO_CALLER();
}

/* ── sub_80E0F28 ── */
/* sub_80E0F28(r0 = object):
 * Set the object's "active/visible" flag (offset 0x64) to 1.
 * Returns the flag value (1) in r1 per the caller's contract. */
Next sub_80E0F28(void) {
    R1 = 1;
    ST32(R0 + 0x64, R1);
    RETURN_TO_CALLER();
}

/* ── battle_setExtraAnimState_80E0F2E ── */
/* battle_setExtraAnimState_80E0F2E(r0 = selector, r5 = battle object):
 * Reset an object's extra-var timer to 0x15 (r0 == 1) or 8 (otherwise).
 * If the timer actually changed, reload the object's sprite animation
 * (sprite 0x80, animation 0x14), drop any queued animation, and clear
 * the "shadowless" flag (bit 3) in the object header.
 * Leaves r3 = value sprite_noShadow leaves behind, or the old timer
 * value when the early-exit path is taken; r4/r5 preserved. */
Next battle_setExtraAnimState_80E0F2E(void) {
    u32 animId = (R0 == 1) ? 0x15 : 8;
    u32 oldTimer = LD32(R5 + oBattleObject_ExtraVars + 0xC);

    ST32(R5 + oBattleObject_ExtraVars + 0xC, animId);

    if (oldTimer != animId) {
        R0 = 0x80;
        R1 = 0x14;
        R2 = animId;
        GUEST_CALL(sprite_load);
        GUEST_CALL(sprite_loadAnimationData);
        GUEST_CALL(sprite_noShadow);

        ST8(R5 + oBattleObject_CurAnimCopy, 0xFF);

        /* oObjectHeader_Flags is at offset 0 of the object header */
        ST8(R5, LD8(R5) & ~8u);
    } else {
        /* early exit: original leaves the old timer value in r3 */
        R3 = oldTimer;
    }

    RETURN_TO_CALLER();
}

/* ── sub_80E1332 ── */
/* sub_80E1332: pure tail call into sub_80C0F52.
 *
 * The original pushes its return address, calls sub_80C0F52, then pops that
 * address straight into the PC, so every register (r1-r7, the c/v flags) and
 * the eventual return target come straight out of the callee. All arguments
 * the caller passed in r0-r4 are simply forwarded untouched. */
Next sub_80E1332(void) {
    u32 return_addr = LR;

    GUEST_CALL(sub_80C0F52);

    LR = return_addr;
    RETURN_TO_CALLER();
}

/* ── t4_0x10_80E17E8 ── */
/* t4_0x10_80E17E8: state dispatcher for the time-freeze battle object.
 *
 * r5 points to the battle object. Its CurState byte is used directly as a
 * byte offset into the handler table at off_80E17FC, so the three known
 * states 0/4/8 select object_timefreezeBegin / sub_80E1808 /
 * object_timefreezeEnd. The chosen handler runs with the caller's registers
 * intact except r0 = state byte and r1 = the loaded table entry; when it
 * returns, the original's `mov lr,pc / bx r1 / pop {pc}` idiom sends control
 * straight back to this routine's own caller, letting the handler's r5 and
 * r8-r12 flow through as the outputs. */

/* register form: the handler target is read from a table at run time and
 * reached through its code address, which no generated wrapper covers; r0
 * and r1 must hold the state byte and the table entry exactly as the
 * original leaves them before branching. */
static void run_timefreeze_state_handler(u32 objectPtr) {
    u32 state = LD8(objectPtr + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E17FC + state);

    R0 = state;    /* the original calls the handler with the state byte in r0 */
    R1 = handler;  /* ...and the loaded table entry in r1 */
    GUEST_CALL_ADDR(handler);
}

Next t4_0x10_80E17E8(void) {
    run_timefreeze_state_handler(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80E1808 ── */
/* Battle-object action dispatch. r5 points at the object; its CurAction byte
 * indexes a table of five handlers (dim screen, two generic object routines,
 * the dim-battle handler, undim screen). The chosen handler runs with the
 * object's live register state -- r0 = CurAction, r1 = its own address,
 * r2-r7/r10 as the caller left them -- and whatever it leaves in r5 and
 * r8-r12 is what our caller receives, so this whole function is one indirect
 * tail call. */

/* register form: the handler is reached by address through the object's
 * action table, so no call_NAME wrapper covers it; it is invoked with the
 * guest register file as the original leaves it, and its register results
 * (r5, r8-r12) must pass through to our caller untouched. */
static u32 run_object_action_handler(u32 handler, u32 action,
                                     u32 r2, u32 r3, u32 r4, u32 r5,
                                     u32 r6, u32 r7, u32 r10) {
    R0 = action;        /* the original calls with r0 = CurAction */
    R1 = handler;       /* ...and r1 still holding the table entry */
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R10 = r10;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Read the object's current action, pick the handler from the dispatch
 * table, and run it. */
static void dispatch_object_action(u32 r2, u32 r3, u32 r4, u32 object,
                                   u32 r6, u32 r7, u32 r10) {
    u32 action = LD8(object + oBattleObject_CurAction);
    u32 handler = LD32(A_off_80E181C + action);
    run_object_action_handler(handler, action, r2, r3, r4, object, r6, r7, r10);
}

Next sub_80E1808(void) {
    dispatch_object_action(R2, R3, R4, R5, R6, R7, R10);
    RETURN_TO_CALLER();
}

/* ── sub_80E1830 ── */
/* Battle-object phase dispatcher.
 *
 * r5 points to a battle object whose CurPhase byte selects one of four
 * per-phase handlers from the jump table at off_80E1844. The handler is
 * invoked with r0 = phase and r1 = its own entry address (the table entry
 * is still in r1 when the callee starts), with r2-r7 passing through
 * unchanged, and returns straight back to sub_80E1830's caller. The
 * handler's results are the updated object pointer in r5 and r8-r12;
 * r4, r6 and r7 are preserved. */

/* register form: the handler's address is read from the object's jump table
 * at run time, so the call goes through a computed code address (the
 * original's mov lr, pc; bx r1) that no generated wrapper covers. */
static void call_phase_handler_sub_80E1830(u32 phase, u32 handler) {
    R0 = phase;      /* handlers receive the phase index in r0 */
    R1 = handler;    /* ... and their own entry address in r1 */
    GUEST_CALL_ADDR(handler);
}

/* Run the phase handler selected by the object's CurPhase byte. */
static void run_object_phase_handler(u32 object) {
    u32 phase = LD8(object + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80E1844 + phase);
    call_phase_handler_sub_80E1830(phase, handler);
}

Next sub_80E1830(void) {
    run_object_phase_handler(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80E1854 ── */
/* sub_80E1854(r5 = battle object):
 * Phase-4 timing helper. On the first call, set up a 30-frame timer, mark the
 * phase as initialized, and (unless Unk_19 says otherwise) notify the related
 * battle object with argument 1. On every call, count the timer down; when it
 * goes below zero, advance the object to phase 4. */
Next sub_80E1854(void) {
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        /* First run: start the 30-frame countdown and flag the phase as set up. */
        ST16(R5 + oBattleObject_Timer, 30);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);

        /* Poke the related object, except when Unk_19 holds the value 0x17. */
        if (LD8(R5 + oBattleObject_Unk_19) != 0x17) {
            R0 = LD32(R5 + oBattleObject_RelatedObject1Ptr);
            R1 = 1;
            GUEST_CALL(sub_80E1332);
        }
    }

    /* Tick the countdown; once it goes negative, move to phase 4. */
    u32 timer = LD16(R5 + oBattleObject_Timer) - 1;
    ST16(R5 + oBattleObject_Timer, timer);
    if ((s32)timer < 0)
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 4);

    RETURN_TO_CALLER();
}

/* ── timefreezeObjectRunFunction_80E1880 ── */
/*
 * Time-freeze object phase runner (0x080E1880).
 *
 * Runs one step of a frozen battle object's phase logic. On the first entry
 * (PhaseInitialized clear) the object's per-type handler is fetched from the
 * jump table at off_802CD5C (indexed by Unk_19) and invoked with the object's
 * panel position / element / params / damage counter in r0-r2, r4, r6, the
 * related object pointer in r5 and the object's state block (obj+0x18) in r7.
 * If the handler leaves the object's Unk_30 phase value inside 0xDD..0x118,
 * the object's key fields are snapshotted into the global buffer at
 * 0x203C960. The phase is then marked initialized (4), and unless animation
 * is suppressed the object's current phase advances to 8.
 */

#define TIMEFREEZE_SNAPSHOT_BASE   A_byte_203C960
#define TIMEFREEZE_STATE_BLOCK     0x18     /* obj + 0x18 passed to the handler in r7 */
#define PHASE_INIT_DONE            4
#define PHASE_ADVANCE              8
#define SNAPSHOT_PHASE_MIN         0xDD     /* Unk_30 in [0xDD, 0xDD+0x3B] triggers the snapshot */
#define SNAPSHOT_PHASE_RANGE       0x3B

/* register form: the per-type handler is reached through a computed jump-table
 * address (mov lr,pc / bx r3), so no generated wrapper exists for it. Its
 * arguments arrive in guest registers R0-R7 and its results are whatever it
 * leaves in R5 and R8-R12, none of which can be expressed as typed C
 * parameters or a return value; the registers the original pushes/pops around
 * the call are saved and restored here instead. */
static void call_timefreeze_handler(u32 obj) {
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 handler = LD32(A_off_802CD5C
                       + ((u32)LD8(obj + oBattleObject_Unk_19) << 2));

    R0 = LD8(obj + oBattleObject_PanelX);
    R1 = LD8(obj + oBattleObject_PanelY);
    R2 = LD8(obj + oBattleObject_Element);
    R3 = handler;
    R4 = LD32(obj + oBattleObject_Params);
    R5 = LD32(obj + oBattleObject_RelatedObject1Ptr);
    R6 = LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler)
       + LD16(obj + oBattleObject_Unk_32);
    R7 = obj + TIMEFREEZE_STATE_BLOCK;

    GUEST_CALL_ADDR(handler);

    /* The original's pop {r5} / pop {r4,r6,r7,...}: the object pointer and
     * the caller's saved registers go back to their incoming values. */
    R4 = savedR4;
    R5 = obj;
    R6 = savedR6;
    R7 = savedR7;
}

/* Copy the object's current state into the global time-freeze snapshot. */
static void snapshot_object_state(u32 obj) {
    ST8(TIMEFREEZE_SNAPSHOT_BASE,       LD8(obj + oBattleObject_Unk_19));
    ST8(A_byte_203C961,                 LD8(obj + oBattleObject_Element));
    ST16(A_word_203C962,                LD16(obj + oBattleObject_Unk_32));
    ST32(A_dword_203C964,               LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler));
    ST32(A_byte_203C968,                LD32(obj + oBattleObject_Params));
}

static void timefreeze_run_phase(u32 obj) {
    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        call_timefreeze_handler(obj);

        /* Snapshot the object if the handler left it in phase 0xDD..0x118. */
        u32 phase = LD16(obj + oBattleObject_Unk_30);
        if ((u32)(phase - SNAPSHOT_PHASE_MIN) <= SNAPSHOT_PHASE_RANGE)
            snapshot_object_state(obj);

        ST8(obj + oBattleObject_PhaseInitialized, PHASE_INIT_DONE);
    }

    /* Unless animation is suppressed, advance to the next phase. */
    if (LD8(obj + oBattleObject_PreventAnim) == 0)
        ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, PHASE_ADVANCE);
}

/* Thin adapter: r5 is the object; r8-r12 are left behind by the handler
 * called inside the typed function (or untouched when it is not called),
 * and r5 is restored to its incoming value there, so nothing to write. */
Next timefreezeObjectRunFunction_80E1880(void) {
    timefreeze_run_phase(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80E18DA ── */
/* sub_80E18DA: tick a battle object's vanish/teleport phase.
 *
 * r5 = battle object pointer.
 *
 * If the phase hasn't been initialized yet (oBattleObject_PhaseInitialized == 0),
 * seed the timer at 0x1E and mark phase initialized = 4. Then decrement the
 * timer; once it would go negative, advance the phase state to 0x0C. */
Next sub_80E18DA(void) {
    u8 phaseInit = (u8)LD8(R5 + oBattleObject_PhaseInitialized);
    if (phaseInit == 0) {
        ST16(R5 + oBattleObject_Timer, 0x1E);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    }

    u16 timer = (u16)LD16(R5 + oBattleObject_Timer);
    u32 result = timer - 1;
    ST16(R5 + oBattleObject_Timer, (u16)result);
    SET_FLAGS_NZ(result);

    if (!NEGATIVE_FLAG())
        RETURN_TO_CALLER();

    ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0x0C);
    RETURN_TO_CALLER();
}

/* ── sub_80E18F8 ── */
/* sub_80E18F8(r5 = battle object):
 * Runs one object's battle "phase" timer. On the phase's first tick it notifies
 * the related object and arms a 30-frame countdown; on later ticks it just
 * counts the timer down. When the timer runs out (or the phase can't run at
 * all) the object's current action is finished and its phase state is reset.
 * Preserves r4-r7; r5 (the object pointer) is an output. */
Next sub_80E18F8(void) {
    u32 self = R5;
    int endPhase;

    if (LD8(self + oBattleObject_PhaseInitialized) == 0) {
        u32 unk19 = LD8(self + oBattleObject_Unk_19);
        if (unk19 == 0 || unk19 == 0x17) {
            /* The phase cannot run in this state: cut it short right away. */
            endPhase = 1;
        } else {
            /* First tick of the phase: tell the related object, then start
             * a 30-frame timer and mark the phase as initialized. */
            R0 = LD32(self + oBattleObject_RelatedObject1Ptr);
            R1 = 0;
            GUEST_CALL(sub_80E1332);

            ST16(self + oBattleObject_Timer, 30);
            ST8(self + oBattleObject_PhaseInitialized, 4);

            /* The original falls straight into the countdown below, so the
             * very first tick already consumes one frame. */
            u32 timer = LD16(self + oBattleObject_Timer) - 1;
            ST16(self + oBattleObject_Timer, timer);
            endPhase = (s32)timer < 0;
        }
    } else {
        /* Phase already running: count the timer down. */
        u32 timer = LD16(self + oBattleObject_Timer) - 1;
        ST16(self + oBattleObject_Timer, timer);
        endPhase = (s32)timer < 0;
    }

    if (endPhase) {
        /* Timer expired: end the current action and reset the phase state. */
        ST8(self + oBattleObject_CurAction, 0x10);
        ST16(self + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80E199A ── */
/* sub_80E199A: maps an attack-bait slot index to a byte offset in the bait table.
 * inputs:  r0 = slot index
 * outputs: r0 = byte offset
 * clobbers: r1, nz, c
 *
 * Each entry is 3 bytes. The masked slot is shifted left by 17, then shifted
 * back right by 16 or 17 depending on bit 15 (the bit LSL #17 shifts out into
 * the carry flag, which BCC then tests), and the result is multiplied by 3. */
Next sub_80E199A(void) {
    u32 slot = R0 & 0x87FF;
    u32 shifted = slot << 17;
    /* LSL #17 shifts bit 15 of the source out into carry; BCC uses that to
     * choose between the LSR #16 and LSR #17 paths. */
    if (slot & 0x8000) {
        R0 = (shifted >> 16) * 3;
    } else {
        R0 = (shifted >> 17) * 3;
    }
    R1 = 3;
    /* MUL sets NZ from the product; C is preserved from the preceding LSR. */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── t4_0x11_80E19BC ── */
#define TABLE_80E19CC  0x80E19CCu

/* register form: dispatch via raw code address (THUMB b-table target), called from outside */
static u32 call_dispatch_target(u32 code_addr, u32 cur_state, u32 table_word, u32 r5) {
    R5 = r5;
    R0 = cur_state;
    R1 = table_word;
    GUEST_CALL_ADDR(code_addr);
    return R0;
}

/* dispatch for CurState byte at R5+oBattleObject_CurState, three-entry table at 0x80E19CC */
static void dispatch_battle_object_state(u32 battle_obj) {
    u32 cur_state   = (u32)LD8(battle_obj + oBattleObject_CurState);
    u32 table_word  = LD32(TABLE_80E19CC + cur_state);
    call_dispatch_target(table_word, cur_state, table_word, battle_obj);
}

Next t4_0x11_80E19BC(void) {
    dispatch_battle_object_state(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectSetTimerFromParam1Table_80E19DC ── */
/* battle_objectSetTimerFromParam1Table_80E19DC(r5 = battle object):
 * Load the object's next Timer value from a lookup table indexed by its
 * Param1 byte, store it into the Timer field, then advance the object's
 * state to CUR_STATE_UPDATE (4) so the state machine continues next frame.
 * Outputs: r0 = CUR_STATE_UPDATE (nz set by the mov), r1 = timer value,
 * r7 = timer table address. */
#define oBattleObject_Param1 4
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized 8
#define CUR_STATE_UPDATE 4

Next battle_objectSetTimerFromParam1Table_80E19DC(void) {
    /* Lookup table of timer values indexed by Param1 (literal pool pointer). */
    R7 = A_dword_80E19F0;

    u32 timer = LD8(R7 + LD8(R5 + oBattleObject_Param1));
    ST16(R5 + oBattleObject_Timer, timer);
    R1 = timer;

    R0 = CUR_STATE_UPDATE;
    SET_FLAGS_NZ(R0); /* flags left by the original's mov r0, #CUR_STATE_UPDATE */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, R0);

    RETURN_TO_CALLER();
}

#undef oBattleObject_Param1
#undef oBattleObject_CurStateActionPhaseAndPhaseInitialized
#undef CUR_STATE_UPDATE

/* ── battle_object_freeWithPhaseReset_80E1A5E ── */
/* battle_object_freeWithPhaseReset_80E1A5E(r5 = battle object):
 * Mark the object's phase as uninitialized and free its memory.
 * Results and flags are those left by object_freeMemory. */
Next battle_object_freeWithPhaseReset_80E1A5E(void) {
    ST8(R5 + oBattleObject_PhaseInitialized, 1);

    R0 = 1;
    GUEST_CALL(object_freeMemory);

    RETURN_TO_CALLER();
}

/* ── battle_spawnLinkedObject_80E1A6A ── */
/* battle_spawnLinkedObject_80E1A6A(r5 = battle object, r7 = extra vars holder):
 * Spawns a type-0x11 battle object, then, if the object pointer in r5 is
 * non-NULL, wires the object and the holder together:
 *   object->relatedObject1 = object, *holder = object, object->extraVars = holder.
 * Leaves C/V cleared and Z from the object pointer (the flag-setting
 * "mov r0, r5" is really adds r0, r5, #0). */
Next battle_spawnLinkedObject_80E1A6A(void) {
    u32 savedObject = R5;

    R0 = 0x11;
    GUEST_CALL(object_spawnType4);

    /* r0 = r5; the spawn normally leaves r5 untouched, but the original
     * saves/restores it around the call, so keep the caller's value. */
    R0 = R5;
    SET_FLAGS_ADD(R5, 0);
    R5 = savedObject;

    if (R0 != 0) {
        ST32(R0 + oBattleObject_RelatedObject1Ptr, R5);
        if (R7 != 0)
            ST32(R7, R0);
        ST32(R0 + oBattleObject_ExtraVars, R7);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80E1A86 ── */
/* sub_80E1A86(r0 = object pointer):
 *   If r0 is non-NULL, set the object's field at offset 8 to 8.
 *   Returns the written value (8) in r1 if the object existed, otherwise
 *   leaves r1 unchanged from the caller. */
Next sub_80E1A86(void) {
    if (R0 != 0) {
        R1 = 8;
        ST32(R0 + 8, R1);
    }
    RETURN_TO_CALLER();
}

/* ── t4_0x12_80E1A90 ── */
/* t4_0x12_80E1A90(r5 = battle object, r10 = ?):
 * Per-state update dispatcher for this object type. Runs the handler selected
 * by the object's CurState (states are stored as 0/4/8, i.e. pre-multiplied
 * table offsets), then refreshes the object's sprite. */
Next t4_0x12_80E1A90(void) {
    u32 state = LD8(R5 + oBattleObject_CurState);

    R0 = state; /* the original enters each handler with r0 = CurState */
    switch (state) {
    case 0: GUEST_CALL(sub_80E1AB4); break;
    case 4: GUEST_CALL(sub_80E1AFA); break;
    case 8: GUEST_CALL(sub_80E1B0A); break;
    }

    GUEST_CALL(object_updateSpritePaused);
    RETURN_TO_CALLER();
}

/* ── sub_80E1AB4 ── */
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E1AB4 0x8u
#define CUR_STATE_UPDATE_sub_80E1AB4 0x4u
/* sub_80E1AB4: Initialize a battle object sprite when it enters its action state.
 * Inputs:  r5 = battle object pointer, lr = return address
 * Outputs: r5 = battle object pointer (preserved)
 * Clobbers: nz c v r0 r1 r2 r3 lr
 */
Next sub_80E1AB4(void) {
    /* Position object using its panel coordinates. */
    GUEST_CALL(object_setCoordinatesFromPanels);

    /* Reset the 16-bit Z coordinate to 0. */
    ST16(R5 + oBattleObject_Z16, 0);

    /* Load sprite tile 0x80, with sub-tile info 8 / 0xC. */
    R0 = 0x80;
    R1 = 8;
    R2 = 0xC;
    GUEST_CALL(sprite_load);

    /* Load the sprite's animation data and disable its shadow. */
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    /* Set the "flag 2" bit on the object's header. */
    u8 Flags0 = LD8(R5);
    ST8(R5, Flags0 | 2);

    /* Set current animation to 0x0E (both the live copy and the saved copy). */
    R0 = 0x0E;
    ST8(R5 + oBattleObject_CurAnim, R0);
    ST8(R5 + oBattleObject_CurAnimCopy, R0);
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);

    /* Use palette 0 and sync flip state with the object's facing. */
    R0 = 0;
    GUEST_CALL(sprite_setPalette);
    GUEST_CALL(object_getFlip);
    GUEST_CALL(sprite_setFlip);

    /* Mark the object's state action phase as "update". */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E1AB4, CUR_STATE_UPDATE_sub_80E1AB4);

    RETURN_TO_CALLER();
}

/* ── sub_80E1AFA ── */
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E1AFA 0x8u
/* sub_80E1AFA(r5 = BattleObject*, r10, lr):
 * If the battle is over, set the battle object's state action phase to CUR_STATE_DESTROY.
 *
 *   inputs:  r5 r10 lr
 *   outputs: -
 *   clobbers: nz r0 r1 r3 lr
 *   preserved: -
 */
Next sub_80E1AFA(void) {
    GUEST_CALL(battle_isBattleOver);
    if (R0 == 0) {
        RETURN_TO_CALLER();
    }

    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E1AFA, CUR_STATE_DESTROY);
    RETURN_TO_CALLER();
}

/* ── sub_80E1B0A ── */
/* sub_80E1B0A:
 *   inputs:  r5 lr
 *   outputs: r5
 *   clobbers: nz c v r0 r1 r2 r3 lr
 *   preserved: r5
 *
 * Frees the given battle object's memory. If the object is currently the
 * "extra vars" head, clear that head pointer before releasing the memory.
 */
Next sub_80E1B0A(void) {
    u32 extra_vars = LD32(R5 + oBattleObject_ExtraVars);

    if (LD32(extra_vars) == R5) {
        ST32(extra_vars, 0);
    }

    R0 = R5;
    GUEST_CALL(object_freeMemory);

    /* The original does `pop {pc}`, but the function's caller-return is handled
     * by our convention via RETURN_TO_CALLER(); r5 is preserved by the contract. */
    RETURN_TO_CALLER();
}

/* ── sub_80E1B1E ── */
/* sub_80E1B1E: spawn a battle object of type 0x12 via object_spawnType4 and
 * hook it up:
 *   r0/r1 = panel x/y written into the new object
 *   r2-r4 = additional spawn parameters, passed straight through
 *   r5    = object whose AllianceAndDirectionFlip word is copied to the spawn
 *   r7    = extra-vars slot: stored into the new object's ExtraVars, and the
 *           new object's pointer is stored back into [r7]
 * If the spawn fails (object pointer == 0) nothing is initialised.
 * r5 is restored to its entry value; the new object pointer itself is only
 * visible through memory (the hook slot), not in a register. */
Next sub_80E1B1E(void) {
    u32 panelX = R0;
    u32 panelY = R1;
    u32 parent = R5;
    u32 hook   = R7;

    R0 = 0x12;
    GUEST_CALL(object_spawnType4);
    u32 obj = R5;               /* object_spawnType4 returns the object in r5 */
    R5 = parent;                /* pop {r1,r2,r5}: entry r5 lives on */
    SET_FLAGS_NZ(obj);

    if (obj != 0) {
        ST8(obj + oBattleObject_PanelX, panelX);
        ST8(obj + oBattleObject_PanelY, panelY);
        ST32(obj + oBattleObject_ExtraVars, hook);
        ST32(hook, obj);        /* two-way link: object <-> hook slot */
        ST16(obj + oBattleObject_AllianceAndDirectionFlip,
             LD16(parent + oBattleObject_AllianceAndDirectionFlip));
    }

    RETURN_TO_CALLER();
}

/* ── sub_80E1B3C ── */
/* sub_80E1B3C(r0 = object):
 * If r0 is non-null, write 8 at offset +8 of the object (set its state/value to 8). */
Next sub_80E1B3C(void) {
    if (R0 != 0) {
        ST32(R0 + 8, 8);
    }
    RETURN_TO_CALLER();
}

/* ── t4_0x14_80E1CF8 ── */
/* register form: dispatches a battle-object update through a computed
 * jump-table pointer; the target is resolved from guest memory at runtime,
 * has no typed C interface, and all guest registers pass through it */
static void battle_object_state_dispatch(void) {
    u32 state = LD8(R5 + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E1D0C + state);

    /* Reproduce the register state the callee sees:
     * r0 = state index, r1 = resolved handler pointer (as in bx r1). */
    R0 = state;
    R1 = handler;

    switch (state) {
    case 0:
        GUEST_CALL(sub_80E1D18);
        break;
    case 4:
        GUEST_CALL(battle_objectFlyAnimUpdateAndDestroy_80E1D54);
        break;
    case 8:
        GUEST_CALL(battle_objectFreeWrapper_80E1D72);
        break;
    }
}

/* t4_0x14_80E1CF8: per-frame battle-object update dispatch via state table.
 * r5 = BattleObject*; reads CurState (offset 8) to pick one of three
 * handlers; all registers pass through the callee. */
Next t4_0x14_80E1CF8(void) {
    battle_object_state_dispatch();
    RETURN_TO_CALLER();
}

/* ── battle_objectFlyAnimUpdateAndDestroy_80E1D54 ── */
/* battle_objectFlyAnimUpdateAndDestroy_80E1D54(r5 = battle object):
 * Advance the object's sprite animation, integrate its Z velocity into its Z
 * position, and switch the object to the "destroy" state action once the
 * sprite's current frame parameters report the end marker (bit 0x80).
 * Leaves r0 = frame parameters (or CUR_STATE_DESTROY_sub_80E1D54 when destroyed), r1 = 0x80,
 * and flags as the last flag-setting instruction of the taken path leaves them. */

enum { CUR_STATE_DESTROY_sub_80E1D54 = 8u };

Next battle_objectFlyAnimUpdateAndDestroy_80E1D54(void) {
    u32 obj = R5;

    GUEST_CALL(sprite_update);

    /* Z += ZVelocity */
    u32 z      = LD32(obj + oBattleObject_Z);
    u32 zSpeed = LD32(obj + oBattleObject_ZVelocity);
    ST32(obj + oBattleObject_Z, z + zSpeed);

    GUEST_CALL(sprite_getFrameParameters);

    R1 = 0x80;
    u32 frameParams = R0;
    SET_FLAGS_NZ(frameParams & R1);
    if (frameParams & 0x80) {
        /* Animation finished: mark the object for destruction. */
        R0 = CUR_STATE_DESTROY_sub_80E1D54;
        SET_FLAGS_NZ(R0);
        ST32(obj + oBattleObject_CurPhaseAndPhaseInitialized, R0);
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectFreeWrapper_80E1D72 ── */
/* battle_objectFreeWrapper_80E1D72(r0 = object): destructor for a battle object — hand its
 * memory slot back to the object pool via object_freeMemory.
 * All arguments (r0) and carried state (r5) are passed through untouched;
 * r0-r3 and the flags come back from object_freeMemory. */
Next battle_objectFreeWrapper_80E1D72(void) {
    GUEST_CALL(object_freeMemory);
    RETURN_TO_CALLER();
}

/* ── t4_0x30_80E3B70 ── */
/* Dispatch to the BattleObject's state handler from the jump table at
 * 0x080E3B88, then always call object_updateSprite.
 *
 * Table layout (indexed by the CurState byte stored at offset 0/4/8):
 *   state 0 → sub_80E3B94
 *   state 4 → battle_objectUpdatePosAndRunAction_80E3BEA
 *   state 8 → object_freeMemory
 *
 * Inputs come via registers (R5 = BattleObject ptr, state byte at R5+8);
 * all output registers and flags are set by the callees. */

#define STATE_HANDLER_TABLE 0x080E3B88u

/* register form: calls sub_80E3B94 outside this file, which has no typed C interface yet */
static void state_handler_idle(u32 state) {
    R0 = state;
    GUEST_CALL(sub_80E3B94);
}

/* register form: calls battle_objectUpdatePosAndRunAction_80E3BEA outside this file, which has no typed C interface yet */
static void state_handler_run(u32 state) {
    R0 = state;
    GUEST_CALL(battle_objectUpdatePosAndRunAction_80E3BEA);
}

/* register form: calls object_freeMemory outside this file, which has no typed C interface yet */
static void state_handler_free(u32 state) {
    R0 = state;
    GUEST_CALL(object_freeMemory);
}

/* register form: calls object_updateSprite outside this file, which has no typed C interface yet */
static void call_update_sprite_t4_0x30_80E3B70(void) {
    GUEST_CALL(object_updateSprite);
}

/* battle_obj: pointer to the BattleObject (was in R5 from the caller). */
static void battle_object_state_dispatch_and_update(u32 battle_obj) {
    u32 state = LD8(battle_obj + oBattleObject_CurState);

    /* Read the handler address from the jump table (matches original memory access). */
    LD32(STATE_HANDLER_TABLE + state);

    switch (state) {
    case 0:  state_handler_idle(state); break;
    case 4:  state_handler_run(state);  break;
    case 8:  state_handler_free(state); break;
    default: break;  /* out-of-range state: original would branch to garbage */
    }

    /* Always update the sprite after the state handler, even if the handler
     * clobbered R5 — matches the original's unconditional bl. */
    call_update_sprite_t4_0x30_80E3B70();
}

/* t4_0x30_80E3B70 — BattleObject per-frame state dispatch + sprite update */
Next t4_0x30_80E3B70(void) {
    battle_object_state_dispatch_and_update(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectUpdatePosAndRunAction_80E3BEA ── */
/* battle_objectUpdatePosAndRunAction_80E3BEA (r5 = battle object):
 * Update this object's position via sub_80E3D1A — first from its current
 * (X, Y), then again from the position advanced by its X velocity — and
 * then hand control to the handler for its current action, selected from
 * the two-entry jump table off_80E3C0C. The handler is entered with
 * r0 = CurAction (offset 9 in the battle object), r1 = its table entry
 * (thumb address), r2 = whatever sub_80E3D1A left there; everything else
 * passes through untouched. When the handler returns, return to
 * battle_objectUpdatePosAndRunAction_80E3BEA's caller. */
Next battle_objectUpdatePosAndRunAction_80E3BEA(void) {
    R0 = LD32(R5 + oBattleObject_X);
    R1 = LD32(R5 + oBattleObject_Y);
    GUEST_CALL(sub_80E3D1A);

    R0 += LD32(R5 + oBattleObject_XVelocity);
    GUEST_CALL(sub_80E3D1A);

    u32 action = LD8(R5 + 9);           /* oBattleObject_CurAction */
    R0 = action;
    R1 = LD32(A_off_80E3C0C + action);  /* handler address, index 0 or 1 */

    if (action == 0)
        GUEST_CALL(sub_80E3C14);
    else
        GUEST_CALL(sub_80E3CC4);

    RETURN_TO_CALLER();
}

/* ── sub_80E3C14 ── */
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E3C14 0x8u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1; bool c, v; } call_object_isCurrentPanelValid_result_sub_80E3C14;
/* register form: generated wrapper: object_isCurrentPanelValid has no typed C interface yet */
static inline call_object_isCurrentPanelValid_result_sub_80E3C14 call_object_isCurrentPanelValid_sub_80E3C14(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_isCurrentPanelValid);
    return (call_object_isCurrentPanelValid_result_sub_80E3C14){ R0, R1, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3; } call_GetAllianceDependentPanelParamArgs_result;
/* register form: generated wrapper: GetAllianceDependentPanelParamArgs has no typed C interface yet */
static inline call_GetAllianceDependentPanelParamArgs_result call_GetAllianceDependentPanelParamArgs(u32 r3, u32 r5) {
    R3 = r3;
    R5 = r5;
    GUEST_CALL(GetAllianceDependentPanelParamArgs);
    return (call_GetAllianceDependentPanelParamArgs_result){ R2, R3 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; } call_object_checkPanelParameters_result;
/* register form: generated wrapper: object_checkPanelParameters has no typed C interface yet */
static inline call_object_checkPanelParameters_result call_object_checkPanelParameters(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_checkPanelParameters);
    return (call_object_checkPanelParameters_result){ R0, R1, R2, R3, R4, R5 };
}

typedef struct { u32 r2, r3, r5, r6; } call_object_setPanelsFromCoordinates_result_sub_80E3C14;
/* register form: generated wrapper: object_setPanelsFromCoordinates has no typed C interface yet */
static inline call_object_setPanelsFromCoordinates_result_sub_80E3C14 call_object_setPanelsFromCoordinates_sub_80E3C14(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setPanelsFromCoordinates);
    return (call_object_setPanelsFromCoordinates_result_sub_80E3C14){ R2, R3, R5, R6 };
}

typedef struct { u32 r1, r2; bool c, v; } call_object_setCoordinatesFromPanels_result;
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline call_object_setCoordinatesFromPanels_result call_object_setCoordinatesFromPanels(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
    return (call_object_setCoordinatesFromPanels_result){ R1, R2, CARRY_FLAG(), OVERFLOW_FLAG() };
}

#define CUR_STATE_DESTROY_sub_80E3C14 8

/* Walk-step AI tick: validate panel, probe ahead, advance or bounce. */
static void battle_walk_step(u32 obj) {
    /* Check whether the object is standing on a valid panel */
    call_object_isCurrentPanelValid_result_sub_80E3C14 panel = call_object_isCurrentPanelValid_sub_80E3C14(obj);

    if (panel.r0 == 0) {
        /* Invalid: detach related object, destroy self */
        u32 rel = LD32(obj + oBattleObject_RelatedObject1Ptr);
        ST32(rel + oBattleObject_ObjectHeader, 0);
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E3C14,
             CUR_STATE_DESTROY_sub_80E3C14);
        return;
    }

    /* Related object must still be alive */
    u32 rel = LD32(obj + oBattleObject_RelatedObject1Ptr);
    if (LD32(rel + oBattleObject_ObjectHeader) == 0) {
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E3C14,
             CUR_STATE_DESTROY_sub_80E3C14);
        return;
    }

    /* If phase != 0 we are mid-walk: skip initialization, go to movement */
    bool go_movement = true;

    if (LD8(obj + oBattleObject_CurPhase) == 0) {
        /* Phase 0: begin a new walk step */
        go_movement = false;
        ST8(obj + oBattleObject_CurPhase, 1);
        ST16(obj + oBattleObject_Timer, LD16(obj + oBattleObject_Timer2));

        /* ExtraVars[2] != 0 triggers a special action instead of walking */
        if (LD32(obj + (oBattleObject_ExtraVars + 8)) != 0) {
            ST8(obj + oBattleObject_CurAction, 4);
            ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
            return;
        }

        /* Determine target panel one step ahead in X-velocity direction */
        s32 vx = (s32)LD32(obj + oBattleObject_XVelocity);
        u32 step_dir = (vx < 0) ? 0xFFFFFFFFu : 1u;
        u32 next_x = (u8)LD8(obj + oBattleObject_PanelX) + step_dir;
        u32 next_y = (u8)LD8(obj + oBattleObject_PanelY);

        call_GetAllianceDependentPanelParamArgs_result args =
            call_GetAllianceDependentPanelParamArgs(A_byte_80E3CB4, obj);

        call_object_checkPanelParameters_result check =
            call_object_checkPanelParameters(next_x, next_y, args.r2, args.r3);

        if (check.r0 != 0) {
            /* Panel not walkable: proceed with movement anyway */
            go_movement = true;
        } else {
            /* Panel is walkable */
            if (LD8(obj + oBattleObject_Param2) != 0)
                return;

            /* Bounce: reverse horizontal velocity, reset phase */
            ST32(obj + oBattleObject_XVelocity,
                 (u32)(-(s32)LD32(obj + oBattleObject_XVelocity)));
            ST8(obj + oBattleObject_CurPhase, 0);
            return;
        }
    }

    if (!go_movement)
        return;

    /* Movement: advance X by velocity and recompute panel position */
    u32 new_x = (s32)LD32(obj + oBattleObject_X)
              + (s32)LD32(obj + oBattleObject_XVelocity);
    ST32(obj + oBattleObject_X, new_x);
    call_object_setPanelsFromCoordinates_sub_80E3C14(obj);

    /* Write panel coordinates to the pointer stored in ExtraVars[1] */
    u32 xy_ptr = LD32(obj + (oBattleObject_ExtraVars + 4));
    ST16(xy_ptr, LD16(obj + oBattleObject_PanelXY));

    /* Decrement walk timer; if still positive the walk continues next frame */
    u32 timer_val = LD16(obj + oBattleObject_Timer);
    u32 decremented = timer_val - 1;  /* 32-bit wrap for the SUBS+bgt test */
    ST16(obj + oBattleObject_Timer, decremented);

    if ((s32)decremented > 0)
        return;

    /* Timer expired: snap coordinates to panel, end walk phase */
    call_object_setCoordinatesFromPanels(obj);
    ST8(obj + oBattleObject_CurPhase, 0);
}

/* sub_80E3C14(r5 = obj) -> preserved r5 */
Next sub_80E3C14(void) {
    battle_walk_step(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80E3D1A ── */
/* sub_80E3D1A(r0 = x, r1 = y, r5 = battle object):
 * Convert (x, y) to a panel position, read the panel's parameter bits, and
 * set the object's ExtraVars+8 flag if the panel type matches the object's
 * alliance (normal-panel mask for alliance 0, dark-panel mask otherwise).
 * r0, r1, r5 are preserved; r2 ends holding the alliance byte. */
Next sub_80E3D1A(void) {
    u32 saved_x = R0, saved_y = R1;
    u32 object = R5;

    GUEST_CALL(convertXYToPanelXY_800E258);   /* in: r0=x, r1=y; out: r0=panel pos */
    GUEST_CALL(object_getPanelParameters);    /* in: r0; out: r0=parameter bits */

    u32 panel_params = R0;
    u32 alliance = LD8(R5 + oBattleObject_Alliance);
    /* Alliance 0 checks the normal-panel bit (0x4000000), others the dark-panel bit. */
    u32 panel_mask = (alliance == 0) ? 0x4000000 : 0x8000000;

    R2 = alliance;
    if (panel_params & panel_mask)
        ST32(R5 + oBattleObject_ExtraVars + 8, 1);

    /* push/pop {r0,r1}: both are returned unchanged; r0's intermediate
     * panel-params/flag value never escapes. */
    R0 = saved_x;
    R1 = saved_y;
    RETURN_TO_CALLER();
}

/* ── ifPanelFlagSetFindBattleActorWithCollisionOnPanelXY_80E7486 ── */
/* ifPanelFlagSetFindBattleActorWithCollisionOnPanelXY_80E7486
 *   r0 = panel X, r1 = panel Y, r2 = panel-flag selector, r10 = toolkit.
 * Checks whether the panel at (X, Y) carries the selected panel flag (the flag bit is
 * taken from the literal table byte_80E74C4, indexed by the selector), and if so scans
 * the battle state's alive-actor list for an actor whose collision data places it on
 * that same panel.
 * Returns:
 *   r0 = pointer to the matching actor, or NULL (flag clear, or no actor on the panel),
 *   r1 = the panel word of the last actor examined — the flag mask if the flag was
 *        clear, the matching panel word on a hit, or the last examined actor's panel
 *        word (0 if it had no collision data) after a full scan,
 *   r3 = the matching alive-actor list slot on a hit, the one-past-the-end slot after a
 *        full scan, and untouched (as object_getPanelParameters left it) when the flag
 *        is clear.
 * c/v are those of object_getPanelParameters on the flag-clear path, and of an "equal"
 * comparison (c = 1, v = 0) on both exits from the scan. r4/r6 are preserved. */
Next ifPanelFlagSetFindBattleActorWithCollisionOnPanelXY_80E7486(void) {
    u32 flagSelector = R2;
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    /* Panels are identified by the word (Y << 8) | X. */
    u32 panelXY = (R1 << 8) + R0;
    R4 = flagSelector << 2;
    R6 = panelXY;

    GUEST_CALL(object_getPanelParameters);
    u32 panelParams = R0;

    /* byte_80E74C4 holds one panel-flag bit per selector. */
    u32 flagMask = LD32(A_byte_80E74C4 + R4);
    if ((panelParams & flagMask) == 0) {
        /* The panel does not carry the wanted flag: nothing can be standing there. */
        R0 = 0;
        R1 = flagMask;
        R4 = savedR4;
        R6 = savedR6;
        RETURN_TO_CALLER();
    }

    /* Walk the battle state's eight alive-actor slots; empty slots are NULL and are
     * skipped without touching r1. */
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 slot = battleState + oBattleState_AliveBattleActors;
    u32 i = 0;
    for (; i < 8; i++) {
        u32 actor = LD32(slot);
        if (actor != 0) {
            R1 = LD32(actor + oBattleObject_CollisionDataPtr);
            if (R1 != 0) {
                R1 = LD16(R1 + oCollisionData_PanelXY);
                if (R1 == panelXY) {
                    /* Found an actor standing on that panel. */
                    R0 = actor;
                    R3 = slot;
                    SET_FLAGS_CMP(R1, panelXY);
                    R4 = savedR4;
                    R6 = savedR6;
                    RETURN_TO_CALLER();
                }
            }
        }
        slot += 4;
    }

    /* No alive actor occupies the panel. */
    R0 = 0;
    R3 = slot;
    SET_FLAGS_CMP(i, 8); /* i == 8 here: same "equal" flags as the original's final cmp */
    R4 = savedR4;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── nullsub_12 ── */
/* nullsub_12: empty placeholder function; does nothing. */
Next nullsub_12(void) {
    RETURN_TO_CALLER();
}

/* ── playerObject_main_80EA460 ── */
/* Dispatch the player battle-object state machine.
 * Reads CurState byte, calls the matching handler, then the shared
 * post-state hook (sub_801BCF4).
 *
 * Jump table (off_80EA478, word-indexed by state byte):
 *   state 0 → playerObject_init_80172F0
 *   state 4 → playerObject_update_80EA484
 *   state 8 → battleObject_destroy_8016C4E
 */

/* register form: calls playerObject_init_80172F0 outside this file, which has no typed C interface yet */
static void call_player_init(u32 state) {
    R0 = state;
    GUEST_CALL(playerObject_init_80172F0);
}

/* register form: calls playerObject_update_80EA484 outside this file, which has no typed C interface yet */
static void call_player_update(u32 state) {
    R0 = state;
    GUEST_CALL(playerObject_update_80EA484);
}

/* register form: calls battleObject_destroy_8016C4E outside this file, which has no typed C interface yet */
static void call_sub_8016C4E(u32 state) {
    R0 = state;
    GUEST_CALL(battleObject_destroy_8016C4E);
}

/* register form: calls sub_801BCF4 outside this file, which has no typed C interface yet */
static void call_post_state_update(void) {
    GUEST_CALL(sub_801BCF4);
}

static void player_state_dispatch(u32 battle_obj) {
    u32 state = LD8(battle_obj + oBattleObject_CurState);

    switch (state) {
        case 0: call_player_init(state);   break;
        case 4: call_player_update(state); break;
        case 8: call_sub_8016C4E(state);   break;
    }

    call_post_state_update();
}

/* playerObject_main_80EA460(r5 = player battle-object pointer) */
Next playerObject_main_80EA460(void) {
    player_state_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── nullsub_105 ── */
/* nullsub_105: an empty stub (part of the AI attack jumptable region; the real
 * handlers are the routines whose addresses follow it in the table).
 * It does nothing and returns immediately. */

/* No-op stub — nothing to do in the typed function. */
static void nullsub_105_body(void) {
}

Next nullsub_105(void) {
    nullsub_105_body();
    RETURN_TO_CALLER();
}

/* ── aiAttack_stepStateMachine_80EB04C ── */
/* aiAttack_stepStateMachine_80EB04C: run one step of the AI attack-state machine for the object in R7.
 *
 * The state byte oAIAttackVars_Unk_00 is a byte offset into the handler table
 * at off_80EB074 (entries are 4 bytes apart, so it holds 0, 4, 8, 0xC or 0x10).
 * The selected handler is entered with r0 = state byte and r1 = its own table
 * entry (the original's `mov lr, pc / bx r1` dispatch), and may clobber the
 * flags and LR, which pass straight through to this function's caller.
 *
 * After the handler returns: if it latched "busy" into oAIAttackVars_Unk_30,
 * we're done. Otherwise we poll the D-pad; if the currently held direction
 * differs from the one remembered in oAIAttackVars_Unk_0c, store 1 into
 * oAIAttackVars_Unk_30.
 */
Next aiAttack_stepStateMachine_80EB04C(void) {
    u32 state = LD8(R7); /* oAIAttackVars_Unk_00 is at offset 0 */

    R0 = state;                          /* handler input: current state */
    R1 = LD32(A_off_80EB074 + state);    /* handler address from the table */
    switch (state) {
    case 0x00: GUEST_CALL(battleObject_aiAttackStepMoveTowardTarget_80EB088); break;
    case 0x04: GUEST_CALL(battle_aiAttack_applyPanelMove_80EB128); break;
    case 0x08: GUEST_CALL(aiAttack_TimerTickEnd_80EB194); break;
    case 0x0C: GUEST_CALL(ai_attackStepRunDecrement_80EB1C4); break;
    case 0x10: GUEST_CALL(sub_80EB1F8); break;
    default: break; /* unreachable: the state byte is always a table offset */
    }

    /* tst busy, busy: skip the direction poll if the handler flagged busy */
    u32 busy = LD32(R7 + oAIAttackVars_Unk_30);
    R0 = busy;
    SET_FLAGS_NZ(busy);
    if (busy != 0)
        RETURN_TO_CALLER();

    /* busy is 0 here, so r0 = 0 going into the button poll */
    GUEST_CALL(playerObject_checkDirectionButtons_800FA54);

    /* r1 ends up holding the remembered direction either way */
    R1 = LD8(R7 + oAIAttackVars_Unk_0c);
    SET_FLAGS_CMP(R0, R1);
    if (R0 != R1) {
        R0 = 1;                              /* mov r0, #1 also sets nz */
        ST32(R7 + oAIAttackVars_Unk_30, R0);
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── aiAttack_TimerTickEnd_80EB194 ── */
/* Offsets/bit positions referenced by the assembly that aren't in game.h. */
#define OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT 19   /* lsl r0, r0, #19 */
#define oAIAttackVars_Unk_00 0                   /* strh r0, [r7,#0] */

/* aiAttack_TimerTickEnd_80EB194: per-frame tick of an AI attack action. R7 points at the object's
 * AI attack variables.
 *
 * Decrements the action timer (Unk_10) and leaves that value in R0. While it
 * is still counting down there is nothing else to do. Once it reaches zero the
 * object's "busy" flag (0x40) is cleared and the CURRENTLY_MOVING flag is set,
 * the timer is reloaded from Unk_18 (the reloaded value stays in R0 when the
 * AI-layer check battle_aiAttack_checkMysteryDataUnk_80F02A2 is called, as in the original), the animation is
 * reset if the check returns non-zero, and finally the action state (Unk_00)
 * is set to 0xC.
 *
 * Flags left behind: on the early-return path those of the decrement; on the
 * reset path nz from the final MOV (Thumb MOVS) and c/v from the last helper
 * call, matching the original. */
Next aiAttack_TimerTickEnd_80EB194(void) {
    /* timer--; done for this frame unless it just hit zero. R0 keeps the
     * decremented value, as the original leaves it there for callers. */
    R0 = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(R0, 1);               /* SUBS r0, #1 */
    R0 -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, R0);
    if (R0 != 0)
        RETURN_TO_CALLER();

    /* Timer expired: no longer blocked, and the object starts moving. */
    R0 = 0x40;
    GUEST_CALL(object_clearFlag);

    R0 = 1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
    GUEST_CALL(object_setFlag1);

    /* Reload the timer for the next attack and ask the AI layer about it.
     * R0 still holds the reloaded value when battle_aiAttack_checkMysteryDataUnk_80F02A2 is entered. */
    R0 = LD16(R7 + oAIAttackVars_Unk_18);
    ST16(R7 + oAIAttackVars_Unk_10, R0);
    GUEST_CALL(battle_aiAttack_checkMysteryDataUnk_80F02A2);

    if (R0 != 0) {
        R0 = 0;
        GUEST_CALL(object_setAnimation);
    }

    /* Back to the idle/step state. */
    R0 = 0xC;
    SET_FLAGS_NZ(R0);
    ST16(R7 + oAIAttackVars_Unk_00, R0);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackDispatch_80EB436 ── */
/* battle_aiAttackDispatch_80EB436: AI attack dispatcher.
 *
 * The byte at the head of the AI attack-vars block (R7) selects one of the
 * attack routines in the jump table at jt_80EB448. The original tail-calls
 * the selected handler (LR holds the address of the following `pop {pc}`,
 * so the handler's return goes straight back to our caller), which the
 * GUEST_CALL/RETURN_TO_CALLER pair reproduces: the handler's registers and
 * condition flags pass through to the caller untouched.
 *
 * On entry to the handler: R0 = selector byte, R1 = the raw table entry
 * (handler address | Thumb bit), exactly as the original leaves them. */
Next battle_aiAttackDispatch_80EB436(void) {
    u32 slot = LD8(R7);                     /* selector at offset 0 of the block */

    R0 = slot;                              /* handler receives the selector */
    R1 = LD32(A_jt_80EB448 + 4 * slot);     /* table entry (Thumb | 1) */

    if (slot == 0)
        GUEST_CALL(sub_80EB450);
    else
        GUEST_CALL(aiAttack_handleMoveDirectionTimerAndStartNewAttack_80EB502);

    RETURN_TO_CALLER();
}

/* ── aiAttackTimerTick_80EB758 ── */
/* The disassembly refers to the attack-phase byte at offset 1 as
 * oAIAttackVars_Unk_01; that name is not in the shared header, so provide it. */

/* aiAttackTimerTick_80EB758: AI attack-state timer tick (r7 = attack vars object).
 * On the first invocation (phase byte still 0) the countdown is initialised
 * to 5 frames and the phase byte is marked started (4).  Every call then
 * decrements the timer; when it goes negative the object leaves its attack
 * state.  Flags left behind are those of the timer decrement (or, when the
 * exit-state routine runs, whatever it leaves). */
Next aiAttackTimerTick_80EB758(void) {
    /* Initialise the attack countdown on the first invocation. */
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        ST16(R7 + oAIAttackVars_Unk_10, 5);
        ST8(R7 + oAIAttackVars_Unk_01, 4);
    }

    /* Decrement the frame counter and store it back. */
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    u32 remaining = timer - 1;
    ST16(R7 + oAIAttackVars_Unk_10, remaining);
    SET_FLAGS_CMP(timer, 1); /* flags as after SUBS timer, #1 */
    R0 = remaining;

    /* Timer expired: leave the attack state. */
    if ((s32)remaining < 0) {
        GUEST_CALL(object_exitAttackState);
    }

    RETURN_TO_CALLER();
}

/* ── aiAttack_BeginAttackSequence_80EB84C ── */
/* aiAttack_BeginAttackSequence_80EB84C(r7 = AI attack vars):
 * Begin a new attack sequence: pick the attack command (2 when the byte at
 * offset 0x03 is 11, otherwise 1) and store it to Unk_12, clear the state
 * field Unk_30, and reload the attack timer Unk_00 with 8.
 * Leaves r0 = 8, r1 = the Unk_03 byte that was read; flags come from the
 * final MOV r0, #8 (N/Z cleared, C/V untouched). */
Next aiAttack_BeginAttackSequence_80EB84C(void) {
    u32 unk03 = LD8(R7 + 3); /* oAIAttackVars_Unk_03 */

    /* cmp r1, #0xB -- branch is taken when the byte differs from 11 */
    SET_FLAGS_CMP(unk03, 0xB);
    u16 command = (unk03 == 0xB) ? 2 : 1;

    ST16(R7 + oAIAttackVars_Unk_12, command);
    ST32(R7 + oAIAttackVars_Unk_30, 0);

    R0 = 8;
    R1 = unk03; /* r1 still holds the byte loaded at the top */
    ST16(R7 + 0, 8); /* oAIAttackVars_Unk_00 */
    SET_FLAGS_NZ(R0); /* mov r0, #8 -- final N/Z update */
    RETURN_TO_CALLER();
}

/* ── battle_getSpecialActorSlotIfAiMatches_80EBB78 ── */
/* battle_getSpecialActorSlotIfAiMatches_80EBB78(r5 = battle object):
 * Reads the object's AI data and checks for a specific actor: actor type 1
 * with AI index 0x12 and version 4. R0 = that actor's slot (0xC), or 0.
 * R2 ends holding the AI data's version byte; R7 is untouched (the original
 * only uses it as scratch and restores it on return). */
Next battle_getSpecialActorSlotIfAiMatches_80EBB78(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData);                    /* oAIData_ActorType (offset 0) */
    u32 aiIndex = LD8(aiData + 1);                  /* oAIData_AIIndex   (offset 1) */
    u32 version = LD8(aiData + oAIData_Version_16);

    R2 = version;

    u32 slot = 0;
    if (actorType == 1 && aiIndex == 0x12 && version == 4)
        slot = 0xC;

    R0 = slot;
    RETURN_TO_CALLER();
}

/* ── aiAttack_dispatchStateHandler_80EBC0E ── */
/* aiAttack_dispatchStateHandler_80EBC0E: dispatch an AI attack sub-state handler.
 * R7 points at the AI attack vars; the state byte in its first field
 * selects the handler from a two-entry table in ROM:
 *   0 -> battleAi_chargeThenFireAttackStep_80EBC28, 1 -> aiAttack_windupTick_80EBD3C.
 * The state byte is passed to the handler in R0 and the handler pointer
 * in R1 (as the original's `ldr r1, [table, state]` leaves it); the
 * `mov lr, pc / bx r1` sequence makes this an effective tail call, so the
 * handler's results (registers and flags) go straight back to our caller. */
Next aiAttack_dispatchStateHandler_80EBC0E(void) {
    u32 attackState = LD8(R7); /* oAIAttackVars state byte (offset 0) */

    R0 = attackState;
    R1 = LD32(A_off_80EBC20 + attackState); /* dispatch table entry */

    if (attackState == 0)
        GUEST_CALL(battleAi_chargeThenFireAttackStep_80EBC28);
    else
        GUEST_CALL(aiAttack_windupTick_80EBD3C); /* state >= 2 reads past the table in the original */

    RETURN_TO_CALLER();
}

/* ── aiAttack_windupTick_80EBD3C ── */
/* aiAttack_windupTick_80EBD3C(r5 = battle object, r7 = AI attack vars):
 * Per-tick update for an AI-controlled attack. On the first tick (state byte
 * at [r7,#1] still 0) the object detaches its linked object, clears AI field
 * 0x68, plays the wind-up animation (7), and arms a short timer (3) before
 * switching to the "attacking" state (4). Every tick then counts the timer
 * down in r0; once it goes negative the object leaves the attack state.
 *
 * Flags out: from the timer subtraction, or from object_exitAttackState when
 * that call is made. r0 ends as timer-1 (or whatever the exit call leaves). */
Next aiAttack_windupTick_80EBD3C(void) {
    if (LD8(R7 + 1) == 0) {         /* oAIAttackVars_Unk_01: state byte */
        /* First tick: tear down the previous attack's leftovers and start
         * the wind-up animation. */
        ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);

        u32 ai = LD32(R5 + oBattleObject_AIDataPtr);
        ST32(ai + oAIData_Unk_68, 0);

        R0 = 7;                 /* animation id */
        R1 = ai;                /* AI data the animation applies to */
        GUEST_CALL(object_setAnimation);

        ST16(R7 + oAIAttackVars_Unk_10, 3);   /* attack timer */
        ST8(R7 + 1, 4);                       /* state: attack in progress */
    }

    /* Count the attack timer down in r0, as the original does; on underflow
     * the attack is over. */
    R0 = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(R0, 1);       /* flags of the original's SUBS r0, #1 */
    R0 -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, R0);

    if ((s32)R0 < 0) {          /* timer was 0: leave the attack state */
        GUEST_CALL(object_exitAttackState);
    }

    RETURN_TO_CALLER();
}

/* ── battle_getAiIndexFlag_80EBD68 ── */
/* battle_getAiIndexFlag_80EBD68(r5 = battle object):
 * For an AI-controlled actor (actor type == 2), return the per-AI-index byte
 * from the table at off_80EBD80 (custom AI slot flags); anything else returns 0.
 * Flags are left as the original leaves them: from CMP actorType,#2, with the
 * "mov r0,#0" path additionally forcing N=0/Z=1 (Thumb MOV-immediate sets flags). */
Next battle_getAiIndexFlag_80EBD68(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData + 0);        /* oAIData_ActorType */

    /* CMP r1, #2 */
    SET_FLAGS_CMP(actorType, 2);

    if (actorType == 2) {
        u32 aiIndex = LD8(aiData + 1);      /* oAIData_AIIndex */
        R0 = LD8(A_off_80EBD80 + aiIndex);
        /* only ldrb's since the CMP: flags stay as CMP left them */
    } else {
        R0 = 0;
        /* mov r0, #0 in Thumb sets N and Z; C and V keep the CMP's values */
        SET_FLAGS_NZ(0);
    }
    RETURN_TO_CALLER();
}
