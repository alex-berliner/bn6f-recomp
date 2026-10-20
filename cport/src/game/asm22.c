/*
 * asm22.c: readable functions of bn6f asm22.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── startScreen_runBeastCutscene_8086FD8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: reqBBS_81409F4 has no typed C interface yet */
static inline u32 call_reqBBS_81409F4(void) {
    GUEST_CALL(reqBBS_81409F4);
    return R0;
}

typedef struct { u32 r1, r3; bool z, n; } call_TestEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8 call_TestEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r1, r3; } call_ClearEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8;
/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline call_ClearEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8 call_ClearEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
    return (call_ClearEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8){ R1, R3 };
}

typedef struct { u32 r1, r3; } call_SetEventFlagFromImmediate_result;
/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline call_SetEventFlagFromImmediate_result call_SetEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
    return (call_SetEventFlagFromImmediate_result){ R1, R3 };
}

typedef struct { bool z, n; } call_sub_80871C0_result;
/* register form: generated wrapper: npc_checkStateEventFlag_80871C0 has no typed C interface yet */
static inline call_sub_80871C0_result call_sub_80871C0(u32 r5) {
    R5 = r5;
    GUEST_CALL(npc_checkStateEventFlag_80871C0);
    return (call_sub_80871C0_result){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r4, r5, r6, r7; } call_sound_queueSfxWithVolume_result;
/* register form: generated wrapper: sound_queueSfxWithVolume has no typed C interface yet */
static inline call_sound_queueSfxWithVolume_result call_sound_queueSfxWithVolume(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(sound_queueSfxWithVolume);
    return (call_sound_queueSfxWithVolume_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r4, r7; } call_sub_8086F18_result;
/* register form: generated wrapper: sub_8086F18 has no typed C interface yet */
static inline call_sub_8086F18_result call_sub_8086F18(void) {
    GUEST_CALL(sub_8086F18);
    return (call_sub_8086F18_result){ R4, R7 };
}

typedef struct { u32 r2; } call_spawn_spawnObjectSwarm_result;
/* register form: generated wrapper: spawn_spawnObjectSwarm has no typed C interface yet */
static inline call_spawn_spawnObjectSwarm_result call_spawn_spawnObjectSwarm(void) {
    GUEST_CALL(spawn_spawnObjectSwarm);
    return (call_spawn_spawnObjectSwarm_result){ R2 };
}

typedef struct { u32 r2; } call_StartCutscene_result;
/* register form: generated wrapper: StartCutscene has no typed C interface yet */
static inline call_StartCutscene_result call_StartCutscene(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(StartCutscene);
    return (call_StartCutscene_result){ R2 };
}

/* startScreen_runBeastCutscene_8086FD8: dispatches the Beast-cutscene transition.
 *
 * On first call (state byte 1 == 0) it routes by the BBS request value or the
 * priority-ordered event flags 5F1/5F3/5F4.  On subsequent calls (byte 1 != 0)
 * it checks whether the player has already seen the relevant event flag for the
 * current map; if so it runs the cleanup sequence, otherwise it sets the flag
 * and starts the appropriate cutscene.
 *
 * r5 is a pointer to the per-cutscene state struct; its register value is
 * preserved (only memory through r5 is written). */

static void beast_cutscene_transition(u32 state, u32 toolkit, u32 r2_in) {
    /* --- first invocation: state byte 1 clear --- */
    if (LD8(state + 1) == 0) {
        u32 bbs = call_reqBBS_81409F4();

        if ((s32)bbs >= (s32)0xB0 && (s32)bbs < (s32)0xC0) {
            /* Variant in [0xB0,0xC0): store offset, pick script by npc_checkStateEventFlag_80871C0 / flag 5F2. */
            u32 variant = bbs - 0xB0;
            ST8(state + 0xC, variant);

            call_sub_80871C0_result sc = call_sub_80871C0(state);
            u32 script;
            if (!sc.z) {
                script = A_byte_8086D5C;
            } else {
                call_TestEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8 t =
                    call_TestEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(5, 0xF2);
                script = !t.z ? A_byte_8086D74 : A_byte_8086D5C;
            }
            call_StartCutscene(script, variant);
            return;
        }

        /* Out-of-range BBS: priority event-flag ladder 5F1 → 5F3 → 5F4. */
        call_TestEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8 t5f1 =
            call_TestEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(5, 0xF1);
        if (!t5f1.z) {
            ST8(state + 9, 1);
            call_StartCutscene(A_byte_8086D2C + 4, t5f1.r1);
            return;
        }

        call_TestEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8 t5f3 =
            call_TestEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(5, 0xF3);
        if (!t5f3.z) {
            ST8(state + 5, 1);
            ST8(state + 7, 1);
            ST8(state + 1, 1);
            call_ClearEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(5, 0xF3);
            call_SetEventFlagFromImmediate_result set5f2 =
                call_SetEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(5, 0xF2);
            call_StartCutscene(A_off_8086D1C + 4, set5f2.r1);
            return;
        }

        call_TestEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8 t5f4 =
            call_TestEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(5, 0xF4);
        if (t5f4.z) {
            /* No flag set: nothing to do. */
            return;
        }
        ST8(state + 5, 2);
        ST8(state + 7, 2);
        call_ClearEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8 clr5f4 =
            call_ClearEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(5, 0xF4);
        call_StartCutscene(A_off_8086D1C + 4, clr5f4.r1);
        return;
    }

    /* --- subsequent invocations: state byte 1 non-zero --- */
    ST8(state + 1, 0);

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 map_group  = LD8(game_state + oGameState_MapGroup);
    u32 map_number = 0;
    bool seen_already;   /* true → cleanup, false → set flag + play cutscene */
    u32 flag_bank = 0, flag_lo = 0, script_addr = 0;

    if (map_group == 0x85) {
        /* Green Town Foyer: use event FC0. */
        flag_bank = 0xF;
        flag_lo   = 0xC0;
        script_addr = A_byte_8086460 + 8;
    } else {
        map_number = LD8(game_state + oGameState_MapNumber);
        flag_bank  = 5;
        if (map_number == 0) {
            flag_lo = 0xE1;
            script_addr = A_byte_8086340 + 0x14;
        } else if (map_number == 1) {
            flag_lo = 0xE2;
            script_addr = A_byte_8086340 + 0x68;
        } else {
            flag_lo = 0xE3;
            script_addr = A_byte_8086340 + 0xBC;
        }
    }

    call_TestEventFlagFromImmediate_result_startScreen_runBeastCutscene_8086FD8 tmap =
        call_TestEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(flag_bank, flag_lo);

    if (!tmap.z) {
        /* Flag already set → run cleanup sequence. */
        seen_already = true;
    } else {
        /* Not yet seen → record it and play the cutscene. */
        call_SetEventFlagFromImmediate_result setmap =
            call_SetEventFlagFromImmediate_startScreen_runBeastCutscene_8086FD8(flag_bank, flag_lo);
        call_StartCutscene(script_addr, setmap.r1);
        return;
    }

    /* Cleanup path (loc_80870D8): optional SFX, then sub_8086F18 + object swarm. */
    if (map_group == 0x81) {
        call_sound_queueSfxWithVolume(0x23, r2_in);
    }
    call_sub_8086F18();
    call_spawn_spawnObjectSwarm();
}

/* Thin adapter: state struct ptr (r5), toolkit (r10), r2 for SFX volume call.
 * Output r5 is preserved (same register value as input). */
Next startScreen_runBeastCutscene_8086FD8(void) {
    beast_cutscene_transition(R5, R10, R2);
    RETURN_TO_CALLER();
}

/* ── npc_checkStateEventFlag_80871C0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result_sub_80871C0;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_sub_80871C0 call_TestEventFlag_sub_80871C0(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_sub_80871C0){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Check the event flag selected by the caller's state.
 * The byte at r5+0xC, doubled, indexes a table of u16 flag ids at
 * byte_80871D8; TestEventFlag is called with that id.  This function's
 * only observable result is the N/Z state TestEventFlag leaves behind
 * (Z clear when the flag is set), so the typed function just performs
 * the call and lets those flags stand. */
static void check_state_event_flag(u32 statePtr) {
    u32 offset = (u32)LD8(statePtr + 0xC) << 1;
    u32 flagId = LD16(A_byte_80871D8 + offset);
    call_TestEventFlag_sub_80871C0(flagId);
}

Next npc_checkStateEventFlag_80871C0(void) {
    check_state_event_flag(R5);
    RETURN_TO_CALLER();
}
