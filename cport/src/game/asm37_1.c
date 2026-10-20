/*
 * asm37_1.c: readable functions of bn6f asm37_1.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── eventStruct_setActiveFlag_81418AA ── */
/* eventStruct_setActiveFlag_81418AA: raise the "event struct active" flag byte (0x200101D <- 1) and
 * return 0 with Z set (the final Thumb "mov r0,#0" sets N/Z).
 * r4/r5/r7 are pushed and popped unchanged, so the caller's values survive. */
static void set_event_struct_flag(void) {
    /* ldr r5, =eStruct2001010 ; strb 1, [r5, #0xD] -- the flag byte is the
     * second byte of dword_200101C, i.e. eStruct2001010 + 0xD. */
    ST8(A_dword_200101C + 1, 1);
}

Next eventStruct_setActiveFlag_81418AA(void) {
    set_event_struct_flag();
    SET_FLAGS_NZ(0);   /* mov r0, #0: Z set, N clear */
    RETURN_TO_CALLER();
}

/* ── clear_eStruct2001010_flagByte0D_81418B6 ── */
/* clear_eStruct2001010_flagByte0D_81418B6 clears one status byte in the fixed struct at 0x2001010:
 * the byte at offset 0x0D (labelled dword_200101C+1 in the disassembly)
 * is written with zero. The pushed r4-r7 are popped back untouched, and
 * the final "mov r0, #0" leaves Z set and N clear for the caller. */

/* r4/r5/r7 are listed as outputs but the original restores them from the
 * stack with their entry values, so they are passed in and passed back. */
typedef struct { u32 r4, r5, r7; } SavedRegs;

static SavedRegs clear_struct_flag_byte(u32 r4, u32 r5, u32 r7) {
    ST8(A_eStruct2001010 + 0xD, 0);
    SavedRegs kept;
    kept.r4 = r4;
    kept.r5 = r5;
    kept.r7 = r7;
    return kept;
}

/* clear_eStruct2001010_flagByte0D_81418B6() -> nz, r4, r5, r7 (r0 clobbered) */
Next clear_eStruct2001010_flagByte0D_81418B6(void) {
    SavedRegs kept = clear_struct_flag_byte(R4, R5, R7);
    R4 = kept.r4;
    R5 = kept.r5;
    R7 = kept.r7;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── battle_performBeastOutBattleAiTick_81419A0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; bool z, n; } call_pet_testIsMapGroupAllowed_8141AA8_result;
/* register form: generated wrapper: pet_testIsMapGroupAllowed_8141AA8 has no typed C interface yet */
static inline call_pet_testIsMapGroupAllowed_8141AA8_result call_pet_testIsMapGroupAllowed_8141AA8(u32 r4, u32 r5, u32 r6, u32 r7) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(pet_testIsMapGroupAllowed_8141AA8);
    return (call_pet_testIsMapGroupAllowed_8141AA8_result){ R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3, r4, r5, r6, r7; } call_sub_80468BA_result;
/* register form: generated wrapper: initWorkArea5_80468BA has no typed C interface yet */
static inline call_sub_80468BA_result call_sub_80468BA(void) {
    GUEST_CALL(initWorkArea5_80468BA);
    return (call_sub_80468BA_result){ R3, R4, R5, R6, R7 };
}

typedef struct { u32 r4, r5, r6, r7; bool v; } call_battleObject_initAIChipQueueSlot_80468C6_result;
/* register form: generated wrapper: battleObject_initAIChipQueueSlot_80468C6 has no typed C interface yet */
static inline call_battleObject_initAIChipQueueSlot_80468C6_result call_battleObject_initAIChipQueueSlot_80468C6(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(battleObject_initAIChipQueueSlot_80468C6);
    return (call_battleObject_initAIChipQueueSlot_80468C6_result){ R4, R5, R6, R7, OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_battle_callAIDispatch_8046818_result;
/* register form: generated wrapper: battle_callAIDispatch_8046818 has no typed C interface yet */
static inline call_battle_callAIDispatch_8046818_result call_battle_callAIDispatch_8046818(void) {
    GUEST_CALL(battle_callAIDispatch_8046818);
    return (call_battle_callAIDispatch_8046818_result){ R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_battleObject_updateSpriteGfxForAll_8046854_result;
/* register form: generated wrapper: battleObject_updateSpriteGfxForAll_8046854 has no typed C interface yet */
static inline call_battleObject_updateSpriteGfxForAll_8046854_result call_battleObject_updateSpriteGfxForAll_8046854(void) {
    GUEST_CALL(battleObject_updateSpriteGfxForAll_8046854);
    return (call_battleObject_updateSpriteGfxForAll_8046854_result){ R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r4, r5, r6, r7; } call_battle_gfx_renderLatticeOrSpinner_811E0BA_result;
/* register form: generated wrapper: battle_gfx_renderLatticeOrSpinner_811E0BA has no typed C interface yet */
static inline call_battle_gfx_renderLatticeOrSpinner_811E0BA_result call_battle_gfx_renderLatticeOrSpinner_811E0BA(void) {
    GUEST_CALL(battle_gfx_renderLatticeOrSpinner_811E0BA);
    return (call_battle_gfx_renderLatticeOrSpinner_811E0BA_result){ R4, R5, R6, R7 };
}

typedef struct { bool z, n; } call_IsCutsceneScriptNonNull_result;
/* register form: generated wrapper: IsCutsceneScriptNonNull has no typed C interface yet */
static inline call_IsCutsceneScriptNonNull_result call_IsCutsceneScriptNonNull(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return (call_IsCutsceneScriptNonNull_result){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { bool z, n; } call_owPlayer_isBusyOrCutsceneActive_809E462_result;
/* register form: generated wrapper: owPlayer_isBusyOrCutsceneActive_809E462 has no typed C interface yet */
static inline call_owPlayer_isBusyOrCutsceneActive_809E462_result call_owPlayer_isBusyOrCutsceneActive_809E462(void) {
    GUEST_CALL(owPlayer_isBusyOrCutsceneActive_809E462);
    return (call_owPlayer_isBusyOrCutsceneActive_809E462_result){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r1, r3; bool z, n, c, v; } call_IsScreenFadeActive_result_battle_performBeastOutBattleAiTick_81419A0;
/* register form: generated wrapper: IsScreenFadeActive has no typed C interface yet */
static inline call_IsScreenFadeActive_result_battle_performBeastOutBattleAiTick_81419A0 call_IsScreenFadeActive_battle_performBeastOutBattleAiTick_81419A0(void) {
    GUEST_CALL(IsScreenFadeActive);
    return (call_IsScreenFadeActive_result_battle_performBeastOutBattleAiTick_81419A0){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1; bool z, n; } call_chatbox_mask_eFlags2009F38_result_battle_performBeastOutBattleAiTick_81419A0;
/* register form: generated wrapper: chatbox_mask_eFlags2009F38 has no typed C interface yet */
static inline call_chatbox_mask_eFlags2009F38_result_battle_performBeastOutBattleAiTick_81419A0 call_chatbox_mask_eFlags2009F38_battle_performBeastOutBattleAiTick_81419A0(u32 r0) {
    R0 = r0;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return (call_chatbox_mask_eFlags2009F38_result_battle_performBeastOutBattleAiTick_81419A0){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r4, r5, r6, r7; bool z, n; } call_battle_canBeastOut_8141AD2_result;
/* register form: generated wrapper: battle_canBeastOut_8141AD2 has no typed C interface yet */
static inline call_battle_canBeastOut_8141AD2_result call_battle_canBeastOut_8141AD2(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_canBeastOut_8141AD2);
    return (call_battle_canBeastOut_8141AD2_result){ R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: battle_beastSpEvolveStep_8141A10 has no typed C interface yet */
static inline void call_battle_beastSpEvolveStep_8141A10(u32 r4, u32 r5, u32 r6, u32 r7) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battle_beastSpEvolveStep_8141A10);
}

/* register form: generated wrapper: battleObject_tickBeastOutActionCooldown_8141A70 has no typed C interface yet */
static inline void call_battleObject_tickBeastOutActionCooldown_8141A70(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_tickBeastOutActionCooldown_8141A70);
}

/* Per-frame beast-out battle AI tick.
 * Gates beast evolution / action-cooldown behind map-group check, one-shot init,
 * per-frame helpers, cutscene/busy/fade/chatbox gates, and the pet subsystem.
 * Original saves r4-r7 via push/pop; we restore r5 (the only preserved register
 * not also listed as an input) in the adapter. */

#define A_byte_2001013  0x2001013u  /* offset +3 within eStruct2001010 */

static void performBeastOutBattleAiTick(u32 r4, u32 r6, u32 r7, u32 r10) {
    /* Check whether the pet's map group is allowed in battle. */
    call_pet_testIsMapGroupAllowed_8141AA8_result petOk =
        call_pet_testIsMapGroupAllowed_8141AA8(r4, A_eStruct2001010, r6, r7);
    if (!petOk.z)
        return;                         /* group not allowed → exit */

    /* One-shot "initialise this battle" flag at eStruct2001010 + 3. */
    u32 flagAddr = A_eStruct2001010 + (A_byte_2001013 - 0x2001010);
    if (LD8(flagAddr) != 0) {
        ST8(flagAddr, 0);              /* consume the flag */
        call_sub_80468BA();
        call_battleObject_initAIChipQueueSlot_80468C6(1, 0, 0);
        call_battleObject_initAIChipQueueSlot_80468C6(2, 1, 0);
    }

    /* Unconditional per-frame helpers (these set r4-r12, r10). */
    call_battle_callAIDispatch_8046818();
    call_battleObject_updateSpriteGfxForAll_8046854_result gfx =
        call_battleObject_updateSpriteGfxForAll_8046854();
    call_battle_gfx_renderLatticeOrSpinner_811E0BA_result lattice =
        call_battle_gfx_renderLatticeOrSpinner_811E0BA();

    /* r10 (toolkit ptr) is live from updateSpriteGfxForAll; subsequent
     * gate calls read it but do not modify it. */
    u32 toolkitPtr = gfx.r10;

    /* Gate: no cutscene script running (Z must be set). */
    if (!call_IsCutsceneScriptNonNull().z)
        return;

    /* Gate: overworld player not busy / no cutscene (Z must be set). */
    if (!call_owPlayer_isBusyOrCutsceneActive_809E462().z)
        return;

    /* Gate: screen fade must be active (Z clear → NE to continue). */
    if (call_IsScreenFadeActive_battle_performBeastOutBattleAiTick_81419A0().z)
        return;

    /* Gate: chatbox mask flag 0x80 must be clear (Z set → continue). */
    if (!call_chatbox_mask_eFlags2009F38_battle_performBeastOutBattleAiTick_81419A0(0x80).z)
        return;

    /* Gate: game subsystem index must be 4 (pet subsystem). */
    u32 gameStatePtr = LD32(toolkitPtr + oToolkit_GameStatePtr);
    if (LD8(gameStatePtr + oGameState_SubsystemIndex) != 4)
        return;

    /* Decide: beast-out or tick the action cooldown.
     * Uses the live r5 left by renderLatticeOrSpinner. */
    call_battle_canBeastOut_8141AD2_result can =
        call_battle_canBeastOut_8141AD2(lattice.r5);

    if (can.z)
        call_battle_beastSpEvolveStep_8141A10(can.r4, can.r5, can.r6, can.r7);
    else
        call_battleObject_tickBeastOutActionCooldown_8141A70(can.r5);
}

/* battle_performBeastOutBattleAiTick_81419A0(r4, r6, r7, r10, lr) */
Next battle_performBeastOutBattleAiTick_81419A0(void) {
    u32 saved_r5 = R5; /* register form: original saves/restores r5 via push {r4-r7,lr}/pop; wrappers modify it */
    performBeastOutBattleAiTick(R4, R6, R7, R10);
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── battle_getShiftedEnemyTableByte_81420B0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_sub_8142080_result;
/* register form: generated wrapper: battle_getEnemyIndexFromMapGroup_8142080 has no typed C interface yet */
static inline call_sub_8142080_result call_sub_8142080(void) {
    GUEST_CALL(battle_getEnemyIndexFromMapGroup_8142080);
    return (call_sub_8142080_result){ R0 };
}

/* Look up a byte from the table at dword_81420C4 using the value battle_getEnemyIndexFromMapGroup_8142080
 * computes (from r10), and shift it into the top byte of the result. */
static u32 pick_shifted_table_entry(u32 index_source) {
    u32 index = call_sub_8142080().r0;
    return (u32)LD8(A_dword_81420C4 + index) << 16;
}

/* battle_getShiftedEnemyTableByte_81420B0: r10 -> r0 (r4-r7 preserved) */
Next battle_getShiftedEnemyTableByte_81420B0(void) {
    R0 = pick_shifted_table_entry(R10);
    RETURN_TO_CALLER();
}

/* ── scenarioEffectAdvanceStep_8142178 ── */
/* scenarioEffectAdvanceStep_8142178: bump the scenario effect step counter (byte at 0x200078E).
 * Increments the counter by one and stores it back; the byte store wraps
 * naturally at 8 bits. r4-r7 are untouched (the original only pushes/pops
 * them), and r0/flags are clobbered. */
static void scenario_effect_advance_step(void) {
    u32 counter = LD8(A_byte_200078E);

    ST8(A_byte_200078E, counter + 1);
}

/* scenarioEffectAdvanceStep_8142178() -> nothing (r4-r7 preserved) */
Next scenarioEffectAdvanceStep_8142178(void) {
    scenario_effect_advance_step();
    RETURN_TO_CALLER();
}

/* ── getField0x18OfScenarioEffectState2000780_81421e0 ── */
/* Reads the field at offset 0x18 (byte_2000798) of the scenario effect state
 * at eScenarioEffectState2000780. The original only clobbers r0; r4-r7 are
 * pushed and popped unchanged, so they pass through untouched. */
static u32 read_scenario_effect_state_field_18(void) {
    return LD32(A_eScenarioEffectState2000780
                + (A_byte_2000798 - A_eScenarioEffectState2000780));
}

/* inputs: lr (unused)   outputs: r0 (r4-r7 preserved) */
Next getField0x18OfScenarioEffectState2000780_81421e0(void) {
    R0 = read_scenario_effect_state_field_18();
    RETURN_TO_CALLER();
}

/* ── cutscene_dispatchEventFlags_8142618 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3, r4, r5; bool z, n; } call_check_playerCanAdvanceForward_8034C36_result;
/* register form: generated wrapper: check_playerCanAdvanceForward_8034C36 has no typed C interface yet */
static inline call_check_playerCanAdvanceForward_8034C36_result call_check_playerCanAdvanceForward_8034C36(void) {
    GUEST_CALL(check_playerCanAdvanceForward_8034C36);
    return (call_check_playerCanAdvanceForward_8034C36_result){ R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r4, r5, r7; } call_battle_getSoulWeaponBase_81427CE_result;
/* register form: generated wrapper: battle_getSoulWeaponBase_81427CE has no typed C interface yet */
static inline call_battle_getSoulWeaponBase_81427CE_result call_battle_getSoulWeaponBase_81427CE(void) {
    GUEST_CALL(battle_getSoulWeaponBase_81427CE);
    return (call_battle_getSoulWeaponBase_81427CE_result){ R0, R4, R5, R7 };
}

typedef struct { bool z, n; } call_TestEventFlag_result_cutscene_dispatchEventFlags_8142618;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_cutscene_dispatchEventFlags_8142618 call_TestEventFlag_cutscene_dispatchEventFlags_8142618(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_cutscene_dispatchEventFlags_8142618){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r1, r3; bool z, n, c, v; } call_TestEventFlagFromImmediate_result_cutscene_dispatchEventFlags_8142618;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_cutscene_dispatchEventFlags_8142618 call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_cutscene_dispatchEventFlags_8142618){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline void call_SetEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
}

typedef struct { u32 r2; bool z, n; } call_sub_80357AE_result;
/* register form: generated wrapper: sub_80357AE has no typed C interface yet */
static inline call_sub_80357AE_result call_sub_80357AE(void) {
    GUEST_CALL(sub_80357AE);
    return (call_sub_80357AE_result){ R2, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r5; bool z, n; } call_StartCutscene_result_cutscene_dispatchEventFlags_8142618;
/* register form: generated wrapper: StartCutscene has no typed C interface yet */
static inline call_StartCutscene_result_cutscene_dispatchEventFlags_8142618 call_StartCutscene_cutscene_dispatchEventFlags_8142618(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(StartCutscene);
    return (call_StartCutscene_result_cutscene_dispatchEventFlags_8142618){ R2, R5, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r1, r3; bool z, n, c, v; } call_ClearEventFlagFromImmediate_result_cutscene_dispatchEventFlags_8142618;
/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline call_ClearEventFlagFromImmediate_result_cutscene_dispatchEventFlags_8142618 call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
    return (call_ClearEventFlagFromImmediate_result_cutscene_dispatchEventFlags_8142618){ R0, R1, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Event flag groups and IDs for cutscene gating (from movflag macros). */
#define EVT_GROUP_7   7
#define EVT_742       0x42
#define EVT_743       0x43
#define EVT_746       0x46
#define EVT_747       0x47

/* Cutscene script addresses loaded from literal pools. */
#define SCRIPT_ADVANCE_FORWARD  A_byte_8089554
#define SCRIPT_EVENT_746        A_byte_80893CC
#define SCRIPT_EVENT_747        A_dword_8089128

/*
 * Dispatch event-flag-gated cutscenes for an overworld interaction.
 *
 * 1. Confirm the player can advance forward and the soul-weapon event flag is set.
 * 2. Check EVENT_742 / EVENT_743 to decide whether to start the "advance forward"
 *    cutscene (via sub_80357AE) or to set one of the flags for the first time.
 * 3. Independently check EVENT_746 and EVENT_747, starting their cutscenes if set,
 *    then clearing both.
 *
 * The context pointer (r10) is used to read player state at offset 0x3C.
 */
static void dispatch_event_cutscenes(u32 context) {
    /* Cannot advance forward: skip to flag clearing */
    if (call_check_playerCanAdvanceForward_8034C36().z) {
        /* fall through to clear */
    } else if (call_TestEventFlag_cutscene_dispatchEventFlags_8142618(call_battle_getSoulWeaponBase_81427CE().r0).z) {
        /* Soul weapon flag not set: skip to clear */
    } else {
        bool set_743_only = false;
        bool state_byte_nonzero = false;
        bool skip_clear = false;

        /* Test EVENT_742: if the flag is NOT set (z), check player state byte */
        if (!call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_742).z) {
            /* EVENT_742 IS set – check EVENT_743 */
            if (call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_743).z) {
                /* EVENT_743 not set: set it and skip the clear block */
                call_SetEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_743);
                skip_clear = true;
            } else if (call_sub_80357AE().z) {
                /* EVENT_743 is set and sub_80357AE confirms: start cutscene */
                call_StartCutscene_cutscene_dispatchEventFlags_8142618(SCRIPT_ADVANCE_FORWARD, 0);
                /* fall through to clear */
            }
            /* else: sub_80357AE returned non-zero, fall through to clear */
        } else {
            /* EVENT_742 NOT set: read player state byte at r10+0x3C, +0x11 */
            u32 battle_data = LD32(context + 0x3C);
            if (LD8(battle_data + 0x11) != 0) {
                call_SetEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_742);
            }
            /* Whether we set it or not, skip the clear block */
            skip_clear = true;
        }

        /* Clear advance-forward flags unless the path skipped it */
        if (!skip_clear) {
            call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_742);
            call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_743);
        }

        /* EVENT_746 cutscene */
        if (!call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_746).z)
            call_StartCutscene_cutscene_dispatchEventFlags_8142618(SCRIPT_EVENT_746, 0);

        /* EVENT_747 cutscene */
        if (!call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_747).z)
            call_StartCutscene_cutscene_dispatchEventFlags_8142618(SCRIPT_EVENT_747, 0);

        /* Clear both regardless of whether we entered the early branches */
        call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_746);
        call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_747);
    }

    /* Paths that jumped to loc_814266C (clear + fall through to loc_814267C) */
    /* Those need the 742/743 clear AND the 746/747 section. */
    /* Re-do for the two outer beq targets: */
}

/*
 * The control flow has two outer early-exits (z from check_playerCanAdvanceForward
 * or z from TestEventFlag) that go to loc_814266C, which clears 742/743 then falls
 * through to loc_814267C (the 746/747 section). The inner paths either go directly
 * to loc_814267C (skip clear) or go through loc_814266C.  Restructure:
 */
static void cutscene_dispatch(u32 context) {
    bool do_clear_742_743;

    if (call_check_playerCanAdvanceForward_8034C36().z) {
        do_clear_742_743 = true;
    } else if (call_TestEventFlag_cutscene_dispatchEventFlags_8142618(call_battle_getSoulWeaponBase_81427CE().r0).z) {
        do_clear_742_743 = true;
    } else {
        do_clear_742_743 = false;

        if (!call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_742).z) {
            /* EVENT_742 is set; check EVENT_743 */
            if (call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_743).z) {
                /* 743 not set: set it, skip clear */
                call_SetEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_743);
            } else {
                /* 743 is set: check if we should start the advance cutscene */
                if (call_sub_80357AE().z) {
                    call_StartCutscene_cutscene_dispatchEventFlags_8142618(SCRIPT_ADVANCE_FORWARD, 0);
                }
                do_clear_742_743 = true;
            }
        } else {
            /* EVENT_742 NOT set: check player state byte */
            u32 battle_data = LD32(context + 0x3C);
            if (LD8(battle_data + 0x11) != 0)
                call_SetEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_742);
            /* Skip clear (original goes to loc_814267C directly) */
        }
    }

    if (do_clear_742_743) {
        call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_742);
        call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_743);
    }

    /* EVENT_746 section */
    if (!call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_746).z)
        call_StartCutscene_cutscene_dispatchEventFlags_8142618(SCRIPT_EVENT_746, 0);

    /* EVENT_747 section */
    if (!call_TestEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_747).z)
        call_StartCutscene_cutscene_dispatchEventFlags_8142618(SCRIPT_EVENT_747, 0);

    call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_746);
    call_ClearEventFlagFromImmediate_cutscene_dispatchEventFlags_8142618(EVT_GROUP_7, EVT_747);
}

/* cutscene_dispatchEventFlags_8142618(r10) -> r2, r3, r5, r7 from callees */
Next cutscene_dispatchEventFlags_8142618(void) {
    cutscene_dispatch(R10);
    RETURN_TO_CALLER();
}

/* ── GetSoulWeaponCursorCameraOffsetForMap ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_GetSoulWeaponsMapIndex_result;
/* register form: generated wrapper: GetSoulWeaponsMapIndex has no typed C interface yet */
static inline call_GetSoulWeaponsMapIndex_result call_GetSoulWeaponsMapIndex(void) {
    GUEST_CALL(GetSoulWeaponsMapIndex);
    return (call_GetSoulWeaponsMapIndex_result){ R0 };
}

/* Per-map camera offsets for the soul-weapon cursor. Each entry is two signed
 * bytes: X offset, Y offset. Indexed by GetSoulWeaponsMapIndex(). */
extern const s8 SoulWeaponMapBasedCameraOffsets[];

#define SOUL_WEAPON_OFFSET_TABLE A_SoulWeaponMapBasedCameraOffsets

/* Look up the cursor camera offset for the current soul-weapon map.
 * out_x = X offset (signed), out_y = Y offset (signed). */
static void get_soul_weapon_cursor_camera_offset(s32 *out_x, s32 *out_y) {
    call_GetSoulWeaponsMapIndex_result idx = call_GetSoulWeaponsMapIndex();
    const u8 *entry = (const u8 *)SOUL_WEAPON_OFFSET_TABLE + (idx.r0 << 1);
    *out_x = LDS8((u32)entry + 0);
    *out_y = LDS8((u32)entry + 1);
}

/* r10 in (unchanged on return), r0/r1 out: cursor X/Y camera offset for the soul-weapon map */
Next GetSoulWeaponCursorCameraOffsetForMap(void) {
    s32 x, y;
    get_soul_weapon_cursor_camera_offset(&x, &y);
    R0 = (u32)x;
    R1 = (u32)y;
    RETURN_TO_CALLER();
}

/* ── ow_getPlayerTileMapIndex_8142990 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3; } call_ReadOWPlayerObjectCoords_result;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result call_ReadOWPlayerObjectCoords(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result){ R0, R1, R2, R3 };
}

typedef struct { u32 r0; } call_sub_8142816_result;
/* register form: generated wrapper: ow_getCursorMapTileIndex has no typed C interface yet */
static inline call_sub_8142816_result call_sub_8142816(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ow_getCursorMapTileIndex);
    return (call_sub_8142816_result){ R0 };
}

/* ow_getPlayerTileMapIndex_8142990: fetch the overworld player object's coordinates, then hand the
 * first two of them to ow_getCursorMapTileIndex and return its result.
 * r4-r7 are only saved/restored around the calls, so they end unchanged. */
static u32 get_player_coords_and_process(void) {
    call_ReadOWPlayerObjectCoords_result coords = call_ReadOWPlayerObjectCoords();
    return call_sub_8142816(coords.r0, coords.r1).r0;
}

/* ow_getPlayerTileMapIndex_8142990(r10 = context) -> r0; r4-r7 preserved */
Next ow_getPlayerTileMapIndex_8142990(void) {
    R0 = get_player_coords_and_process();
    RETURN_TO_CALLER();
}

/* ── battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5; } call_battleObject_scenarioEffectStateAddr_81421D8_result;
/* register form: generated wrapper: battleObject_scenarioEffectStateAddr_81421D8 has no typed C interface yet */
static inline call_battleObject_scenarioEffectStateAddr_81421D8_result call_battleObject_scenarioEffectStateAddr_81421D8(void) {
    GUEST_CALL(battleObject_scenarioEffectStateAddr_81421D8);
    return (call_battleObject_scenarioEffectStateAddr_81421D8_result){ R0, R4, R5 };
}

typedef struct { bool c, v; } call_ZeroFillByWord_result;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result call_ZeroFillByWord_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r5, r6, r7; } call_sub_8142990_result;
/* register form: generated wrapper: ow_getPlayerTileMapIndex_8142990 has no typed C interface yet */
static inline call_sub_8142990_result call_sub_8142990(void) {
    GUEST_CALL(ow_getPlayerTileMapIndex_8142990);
    return (call_sub_8142990_result){ R0, R5, R6, R7 };
}

typedef struct { u32 r0, r4, r5, r6; } call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA;
/* register form: generated wrapper: battle_getScenarioEffectStateField_81421D0 has no typed C interface yet */
static inline call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA call_battle_getScenarioEffectStateField_81421D0_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA(void) {
    GUEST_CALL(battle_getScenarioEffectStateField_81421D0);
    return (call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA){ R0, R4, R5, R6 };
}

/* register form: generated wrapper: GetOWPlayerFacingDirection has no typed C interface yet */
static inline u32 call_GetOWPlayerFacingDirection(void) {
    GUEST_CALL(GetOWPlayerFacingDirection);
    return R0;
}

/* Initialize a 4-entry (16-byte) player-offset matrix for a scenario-effect
 * camera. Each table is indexed by a scenario-effect state field (0..3) and
 * offset by the player's even facing (0,2,4,6). Records are 8 bytes; only the
 * first two s8 bytes (dx,dy) are read, and a dx of 0x7F terminates the stream.
 *
 * For every (dx,dy) pair the function combines it with a per-component base
 * word (low byte → x, high byte → y), producing a 16-bit packed offset written
 * into the zero-filled destination buffer.
 *
 * Returns the dy byte of the terminating sentinel (always 0x7F in practice)
 * which the original leaves in r3 on exit. */
static u32 init_player_offset_matrix(void) {
    /* Obtain the 16-byte destination buffer (scenario-effect state slot) */
    call_battleObject_scenarioEffectStateAddr_81421D8_result state =
        call_battleObject_scenarioEffectStateAddr_81421D8();
    u32 dest = state.r0;

    /* Zero-initialize the buffer */
    call_ZeroFillByWord_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA(dest, 0x10);

    /* Base word: low byte seeds x, upper bytes seed y */
    call_sub_8142990_result base = call_sub_8142990();
    u32 mode_word = base.r0;

    /* Select one of the four facing-direction offset tables */
    call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA sel =
        call_battle_getScenarioEffectStateField_81421D0_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA();
    u32 table_index = sel.r0;

    /* Table-of-pointers at off_8142A1C: word at [index*4] is the record base */
    u32 table_base = LD32(A_off_8142A1C + (table_index << 2));

    /* Facing: mask to even (0,2,4,6) to pick a 2-byte-aligned sub-record */
    u32 facing = call_GetOWPlayerFacingDirection();
    u32 p = table_base + ((facing >> 1) << 1);

    /* Walk 8-byte records; read (dx,dy) s8 pair; stop when dx == 0x7F */
    for (;;) {
        s32 dx = LDS8(p);
        s32 dy = LDS8(p + 1);
        if (dx == 0x7F)
            return (u32)dy;

        u32 low  = (mode_word & 0xFF) + (u32)dx;
        u32 high = ((mode_word >> 8) + (u32)dy) << 8;
        ST32(dest, low | high);

        dest += 4;
        p += 8;
    }
}

/* battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA:
 *   inputs:  r10 lr  (r10 passes through to callees)
 *   outputs: r3  (dy byte of terminating sentinel)
 *            r5 is listed as output+preserved; the original's pop restores it,
 *            so we do not write R5.
 *   preserved: r4 r5 r6 r7 (restored by the original's push/pop; wrapper calls
 *            modify them internally but the framework accounts for that). */
Next battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA(void) {
    R3 = init_player_offset_matrix();
    RETURN_TO_CALLER();
}

/* ── init_queueBeastSpGfx ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; } call_QueueEightWordAlignedGFXTransfer_result;
/* register form: generated wrapper: QueueEightWordAlignedGFXTransfer has no typed C interface yet */
static inline call_QueueEightWordAlignedGFXTransfer_result call_QueueEightWordAlignedGFXTransfer(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    return (call_QueueEightWordAlignedGFXTransfer_result){ R4, R5, R6, R7 };
}

/* Queue a VRAM transfer that uploads the Beast-Out SP sprite graphics.
 * The callee leaves the queued transfer's tracking values in r4/r7, which the
 * caller of this function expects in its own r4/r7, so we pass them through.
 * Returns { r4, r5, r6, r7 } as left by QueueEightWordAlignedGFXTransfer. */
static call_QueueEightWordAlignedGFXTransfer_result queue_beast_sp_gfx(void) {
    return call_QueueEightWordAlignedGFXTransfer(A_dword_8619730, 0x6006C00, 0x420);
}

/* init_queueBeastSpGfx() -> r4 r7 (from QueueEightWordAlignedGFXTransfer) */
Next init_queueBeastSpGfx(void) {
    call_QueueEightWordAlignedGFXTransfer_result res = queue_beast_sp_gfx();
    R4 = res.r4;
    R7 = res.r7;
    RETURN_TO_CALLER();
}

/* ── battle_resolveMapTileAddr_8142CB0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r5; } call_sub_8142D58_result;
/* register form: generated wrapper: ow_calcMapTileOffset_8142D58 has no typed C interface yet */
static inline call_sub_8142D58_result call_sub_8142D58(void) {
    GUEST_CALL(ow_calcMapTileOffset_8142D58);
    return (call_sub_8142D58_result){ R0, R5 };
}

typedef struct { u32 r0, r5; } call_sub_8142DA0_result;
/* register form: generated wrapper: map_resolveTileAddress_8142DA0 has no typed C interface yet */
static inline call_sub_8142DA0_result call_sub_8142DA0(u32 r0) {
    R0 = r0;
    GUEST_CALL(map_resolveTileAddress_8142DA0);
    return (call_sub_8142DA0_result){ R0, R5 };
}

/* Resolve a map-tile data pointer: combines the tile offset returned by
 * map_resolveTileAddress_8142DA0 with the tile-array base inside the MapTilesState (0x200BE70). */
static u32 resolve_map_tile_address(void) {
    /* register form: call_sub_8142D58's wrapper reads R5 for the MapTilesState pointer */
    R5 = A_eMapTilesState200be70;

    call_sub_8142D58_result step1 = call_sub_8142D58();
    call_sub_8142DA0_result step2 = call_sub_8142DA0(step1.r0);

    /* step2.r5 is the battle-relative tiles-state pointer (ow_calcMapTileOffset_8142D58's r5 output
       carried into map_resolveTileAddress_8142DA0).  Offset 0xC holds the tile-table base; the field
       at +8 gives the per-tile stride base.  The final address is the tile offset
       from step2.r0 added to the tile array computed here. */
    u32 tiles_state  = step2.r5;
    u32 tile_table   = LD32(tiles_state + 0xC);   /* dword_200BE7C */
    u32 tile_array   = LD32(tile_table + 8) + tile_table;

    return step2.r0 + tile_array;
}

/* battle_resolveMapTileAddr_8142CB0(r10 = battle context) -> r0 (absolute tile address); r5-r7 preserved */
Next battle_resolveMapTileAddr_8142CB0(void) {
    u32 saved_r5 = R5;
    R0 = resolve_map_tile_address();
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── ow_stampMapTileTemplate ── */
/* Row stride in u16 units of the tile template data tables. */
#define TEMPLATE_ROW_STRIDE 8

/* Stamp a tile template onto a map-tile destination buffer.
 *   dest      – address of the map tile entry to write
 *   tile_type – index (0 or 1) selecting the template table entry
 *
 * Reads the destination row width from the global map-tiles state byte,
 * then copies a u16 header pair and three groups of columns from the
 * selected template into the destination at the appropriate row offsets. */
static void stamp_map_tile_template(u32 dest, u32 tile_type) {
    u32 dest_row_width = LD8(A_eMapTilesState200be70);
    u32 template = LD32(A_off_8142D44 + tile_type * 4);

    /* Copy the two u16 header fields at byte offsets 6 and 8. */
    ST16(dest + 6, LD16(template + 6));
    ST16(dest + 8, LD16(template + 8));

    /* Row 1: columns 1 through 6. */
    for (u32 col = 1; col < 7; col++) {
        u32 src_off = (TEMPLATE_ROW_STRIDE * 1 + col) << 1;
        u32 dst_off = (dest_row_width * 1 + col) << 1;
        ST16(dest + dst_off, LD16(template + src_off));
    }

    /* Row 2: columns 1 through 6. */
    for (u32 col = 1; col < 7; col++) {
        u32 src_off = (TEMPLATE_ROW_STRIDE * 2 + col) << 1;
        u32 dst_off = (dest_row_width * 2 + col) << 1;
        ST16(dest + dst_off, LD16(template + src_off));
    }

    /* Row 3: columns 3 through 4. */
    for (u32 col = 3; col < 5; col++) {
        u32 src_off = (TEMPLATE_ROW_STRIDE * 3 + col) << 1;
        u32 dst_off = (dest_row_width * 3 + col) << 1;
        ST16(dest + dst_off, LD16(template + src_off));
    }
}

/* ow_stampMapTileTemplate(r0 = dest addr, r1 = tile type) – outputs r5,r6,r7
 * are caller-preserved (push/pop); only guest memory is modified. */
Next ow_stampMapTileTemplate(void) {
    stamp_map_tile_template(R0, R1);
    RETURN_TO_CALLER();
}

/* ── ow_calcMapTileOffset_8142D58 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ow_getAreaMapTilesOffset has no typed C interface yet */
static inline u32 call_ow_getAreaMapTilesOffset(void) {
    GUEST_CALL(ow_getAreaMapTilesOffset);
    return R0;
}

/* Compute the byte offset within the area map tile array for the current
 * camera position. The position byte (0x2000B48) is centered at 0x80;
 * the tile-count byte (eMapTilesState200be70) determines the row stride. */
static u32 calc_map_tile_offset(void) {
    u8 tile_count = LD8(A_eMapTilesState200be70);
    u8 position   = LD8(A_byte_2000B48);

    /* Signed distance from center (128), kept as unsigned magnitude */
    u32 delta;
    if ((s32)position >= 0x80)
        delta = position - 0x80;
    else
        delta = 0x80 - position;

    /* offset = 4 * delta * (tile_count - 2)  — wraps mod 2^32 as in ARM */
    u32 offset = 4 * delta * ((u32)tile_count - 2);

    u32 base = call_ow_getAreaMapTilesOffset();

    if ((s32)position >= 0x80)
        return base - offset;
    return base + offset;
}

/* ow_calcMapTileOffset_8142D58() -> r0 */
Next ow_calcMapTileOffset_8142D58(void) {
    R0 = calc_map_tile_offset();
    RETURN_TO_CALLER();
}

/* ── map_resolveTileAddress_8142DA0 ── */
/* Resolve a tile's memory address by adding/subtracting a computed offset
 * from a base pointer.  The first byte of the map-tiles state is the map
 * width; the byte at tile-data+0x9 is an index where bit 7 selects the
 * direction of the adjustment (>=0x80 → add, <0x80 → subtract).
 *
 * offset = ((delta * 2 * width + delta * 4) << 1)   [delta = |index - 0x80|]
 *
 * r5 (the tile-state pointer) is preserved for the caller. */
static u32 resolve_map_tile_addr(u32 base_addr, u32 tile_state_ptr, u32 tile_data_ptr) {
    u32 map_width = LD8(tile_state_ptr);
    u32 tile_byte = LD8(tile_data_ptr + 9);
    u32 delta, offset;

    if (tile_byte >= 0x80) {
        delta = tile_byte - 0x80;
        offset = (delta * 2 * map_width + delta * 4) << 1;
        return base_addr + offset;
    } else {
        delta = 0x80 - tile_byte;
        offset = (delta * 2 * map_width + delta * 4) << 1;
        return base_addr - offset;
    }
}

/* map_resolveTileAddress_8142DA0(r0 = base address) -> r0 = adjusted tile address */
Next map_resolveTileAddress_8142DA0(void) {
    R0 = resolve_map_tile_addr(R0, A_eMapTilesState200be70, A_unk_2000B40);
    RETURN_TO_CALLER();
}

/* ── ow_getAreaMapTilesOffset ── */
/* Tile-offset lookup table for area maps: one halfword per map area. */
#define AREA_MAP_TILE_OFFSETS A_byte_8142E0C

/* Return the tile offset used to draw the area map for the current map.
 * Map group 0x85 is the special Sky Area, which always uses entry 3;
 * every other group uses its own area id stored at GameState+5.
 * Reads: r10 = toolkit, [toolkit + oToolkit_GameStatePtr] = game state. */
static u32 ow_getAreaMapTilesOffset_impl(u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 map_group  = LD8(game_state + oGameState_MapGroup);

    u32 area_id;
    if (map_group == 0x85)
        area_id = 3;                                  /* Sky Area */
    else
        area_id = LD8(game_state + 5);                /* current area number */

    return LD16(AREA_MAP_TILE_OFFSETS + (area_id << 1));
}

/* ow_getAreaMapTilesOffset(r10 = toolkit) -> r0 */
Next ow_getAreaMapTilesOffset(void) {
    R0 = ow_getAreaMapTilesOffset_impl(R10);
    RETURN_TO_CALLER();
}
