/*
 * asm24.c: readable functions of bn6f asm24.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── init_beastSpScenario ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool c, v; } call_ZeroFillByWord_result_init_beastSpScenario;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result_init_beastSpScenario call_ZeroFillByWord_init_beastSpScenario(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result_init_beastSpScenario){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: ow_storePlayerPosDup has no typed C interface yet */
static inline void call_ow_storePlayerPosDup(u32 r5) {
    R5 = r5;
    GUEST_CALL(ow_storePlayerPosDup);
}

typedef struct { u32 r1; bool c, v; } call_ClearEventFlagFromImmediate_result_init_beastSpScenario;
/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline call_ClearEventFlagFromImmediate_result_init_beastSpScenario call_ClearEventFlagFromImmediate_init_beastSpScenario(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
    return (call_ClearEventFlagFromImmediate_result_init_beastSpScenario){ R1, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: gameState_setUnk09_8001172 has no typed C interface yet */
static inline void call_gameState_setUnk09_8001172(u32 r0) {
    R0 = r0;
    GUEST_CALL(gameState_setUnk09_8001172);
}

typedef struct { u32 r3; bool z, n; } call_TestEventFlagFromImmediate_result_init_beastSpScenario;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_init_beastSpScenario call_TestEventFlagFromImmediate_init_beastSpScenario(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_init_beastSpScenario){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r7; } call_sub_808CF50_result;
/* register form: generated wrapper: ai_syncModeEventFlag_808CF50 has no typed C interface yet */
static inline call_sub_808CF50_result call_sub_808CF50(void) {
    GUEST_CALL(ai_syncModeEventFlag_808CF50);
    return (call_sub_808CF50_result){ R7 };
}

typedef struct { u32 r4, r7; } call_init_queueBeastSpGfx_result;
/* register form: generated wrapper: init_queueBeastSpGfx has no typed C interface yet */
static inline call_init_queueBeastSpGfx_result call_init_queueBeastSpGfx(void) {
    GUEST_CALL(init_queueBeastSpGfx);
    return (call_init_queueBeastSpGfx_result){ R4, R7 };
}

typedef struct { u32 r2, r4, r7; } call_sub_808CBA0_result;
/* register form: generated wrapper: spawnOverworldMapObjects_808CBA0 has no typed C interface yet */
static inline call_sub_808CBA0_result call_sub_808CBA0(void) {
    GUEST_CALL(spawnOverworldMapObjects_808CBA0);
    return (call_sub_808CBA0_result){ R2, R4, R7 };
}

typedef struct { u32 r0, r7; } call_ai_testMapGroupEventFlag_result;
/* register form: generated wrapper: ai_testMapGroupEventFlag has no typed C interface yet */
static inline call_ai_testMapGroupEventFlag_result call_ai_testMapGroupEventFlag(void) {
    GUEST_CALL(ai_testMapGroupEventFlag);
    return (call_ai_testMapGroupEventFlag_result){ R0, R7 };
}

typedef struct { u32 r7; } call_init_applyBeastSpScenarioEventFlags_result;
/* register form: generated wrapper: init_applyBeastSpScenarioEventFlags has no typed C interface yet */
static inline call_init_applyBeastSpScenarioEventFlags_result call_init_applyBeastSpScenarioEventFlags(void) {
    GUEST_CALL(init_applyBeastSpScenarioEventFlags);
    return (call_init_applyBeastSpScenarioEventFlags_result){ R7 };
}

typedef struct { u32 r2, r7; } call_init_clearBeastSpEventFlagTable_result;
/* register form: generated wrapper: init_clearBeastSpEventFlagTable has no typed C interface yet */
static inline call_init_clearBeastSpEventFlagTable_result call_init_clearBeastSpEventFlagTable(void) {
    GUEST_CALL(init_clearBeastSpEventFlagTable);
    return (call_init_clearBeastSpEventFlagTable_result){ R2, R7 };
}

typedef struct { u32 r3, r4, r6, r7, r10; bool c, v; } call_sound_8000672_result_init_beastSpScenario;
/* register form: generated wrapper: sound_dispatchSmashHandler_8000672 has no typed C interface yet */
static inline call_sound_8000672_result_init_beastSpScenario call_sound_8000672_init_beastSpScenario(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(sound_dispatchSmashHandler_8000672);
    return (call_sound_8000672_result_init_beastSpScenario){ R3, R4, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Event flag identifiers used during beast-sp scenario initialization. */
#define EVT_GROUP      9
#define EVT_FLAG_9D8   0xD8
#define EVT_FLAG_9D9   0xD9
#define EVT_FLAG_9D3   0xD3

/* Initialize the beast-sp scenario: zero the per-encounter buffer, clear
 * flags, queue graphics, and play the start sound.
 * saved_r5 is the caller's original r5, preserved through the sequence
 * (the wrapper for ow_storePlayerPosDup overwrites R5 with the buffer
 * pointer; the original restores r5 from the stack). */
static u32 init_beastSpScenario_impl(u32 saved_r5) {
    /* Load the scenario buffer pointer and clear 0x98 bytes of state. */
    u32 scenario_buf = LD32(A_dword_808CA44);
    call_ZeroFillByWord_init_beastSpScenario(scenario_buf, 0x98);

    /* Mark byte 3 as initialized (sentinel value 0x20). */
    ST8(scenario_buf + 3, 0x20);

    /* Snapshot the player position into the duplicate slot. */
    call_ow_storePlayerPosDup(scenario_buf);

    /* Clear the two beast-sp encounter-start flags. */
    call_ClearEventFlagFromImmediate_init_beastSpScenario(EVT_GROUP, EVT_FLAG_9D8);
    call_ClearEventFlagFromImmediate_init_beastSpScenario(EVT_GROUP, EVT_FLAG_9D9);

    /* Reset game-state sub-mode to 0. */
    call_gameState_setUnk09_8001172(0);

    /* If flag 9D3 is still set (Z flag set → flag absent → not yet handled),
     * run the extra scenario sub-initialization. */
    if (call_TestEventFlagFromImmediate_init_beastSpScenario(EVT_GROUP, EVT_FLAG_9D3).z)
        call_sub_808CF50();

    /* Unconditionally clear flag 9D3 after the conditional init. */
    call_ClearEventFlagFromImmediate_init_beastSpScenario(EVT_GROUP, EVT_FLAG_9D3);

    /* Queue beast-sp graphics and run core scenario setup. */
    call_init_queueBeastSpGfx();
    call_sub_808CBA0();

    /* Apply scenario event flags only when the map-group test indicates
     * a new encounter (result != 1). */
    if (call_ai_testMapGroupEventFlag().r0 != 1)
        call_init_applyBeastSpScenarioEventFlags();

    /* Clear the event-flag table; its r2 feeds the sound call. */
    call_init_clearBeastSpEventFlagTable_result table = call_init_clearBeastSpEventFlagTable();

    /* Play the scenario-start sound effect (SFX 0x1F, volume 0x100). */
    call_sound_8000672_init_beastSpScenario(0x1F, 0x100, table.r2);

    /* Return original r5: ow_storePlayerPosDup's wrapper left scenario_buf
     * in R5; the original restores it from the saved register on pop. */
    return saved_r5;
}

/* init_beastSpScenario (0x0808CA68): r10 lr in; r2 r3 r5 r7 r10 out. */
Next init_beastSpScenario(void) {
    R5 = init_beastSpScenario_impl(R5);
    RETURN_TO_CALLER();
}

/* ── pet_tryStartRandomEncounterStep_808CB0C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r3; bool z, n, c, v; } call_TestEventFlagFromImmediate_result_pet_tryStartRandomEncounterStep_808CB0C;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_pet_tryStartRandomEncounterStep_808CB0C call_TestEventFlagFromImmediate_pet_tryStartRandomEncounterStep_808CB0C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_pet_tryStartRandomEncounterStep_808CB0C){ R0, R1, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r5, r9, r12; } call_sub_808D11C_result;
/* register form: generated wrapper: sub_808D11C has no typed C interface yet */
static inline call_sub_808D11C_result call_sub_808D11C(void) {
    GUEST_CALL(sub_808D11C);
    return (call_sub_808D11C_result){ R2, R5, R9, R12 };
}

typedef struct { u32 r3, r4, r5, r6, r7, r10; bool c, v; } call_sound_8000672_result_pet_tryStartRandomEncounterStep_808CB0C;
/* register form: generated wrapper: sound_dispatchSmashHandler_8000672 has no typed C interface yet */
static inline call_sound_8000672_result_pet_tryStartRandomEncounterStep_808CB0C call_sound_8000672_pet_tryStartRandomEncounterStep_808CB0C(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(sound_dispatchSmashHandler_8000672);
    return (call_sound_8000672_result_pet_tryStartRandomEncounterStep_808CB0C){ R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; bool z, n; } call_TestPETMenuDataFlag_result;
/* register form: generated wrapper: TestPETMenuDataFlag has no typed C interface yet */
static inline call_TestPETMenuDataFlag_result call_TestPETMenuDataFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestPETMenuDataFlag);
    return (call_TestPETMenuDataFlag_result){ R1, R2, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3; } call_ReadOWPlayerObjectCoords_result;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result call_ReadOWPlayerObjectCoords(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result){ R0, R1, R2, R3 };
}

typedef struct { u32 r5; } call_sub_808CCAC_result;
/* register form: generated wrapper: field_calcFacingTargetTile_808CCAC has no typed C interface yet */
static inline call_sub_808CCAC_result call_sub_808CCAC(u32 r5) {
    R5 = r5;
    GUEST_CALL(field_calcFacingTargetTile_808CCAC);
    return (call_sub_808CCAC_result){ R5 };
}

typedef struct { u32 r2, r5, r9, r10, r11, r12; bool z, n; } call_reqBBS_pollTouchTrigger_808CD04_result;
/* register form: generated wrapper: reqBBS_pollTouchTrigger_808CD04 has no typed C interface yet */
static inline call_reqBBS_pollTouchTrigger_808CD04_result call_reqBBS_pollTouchTrigger_808CD04(void) {
    GUEST_CALL(reqBBS_pollTouchTrigger_808CD04);
    return (call_reqBBS_pollTouchTrigger_808CD04_result){ R2, R5, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r5, r10; } call_pet_randomEncounterRewardStep_808CD24_result;
/* register form: generated wrapper: pet_randomEncounterRewardStep_808CD24 has no typed C interface yet */
static inline call_pet_randomEncounterRewardStep_808CD24_result call_pet_randomEncounterRewardStep_808CD24(u32 r2) {
    R2 = r2;
    GUEST_CALL(pet_randomEncounterRewardStep_808CD24);
    return (call_pet_randomEncounterRewardStep_808CD24_result){ R2, R5, R10 };
}

typedef struct { bool c, v; } call_ClearEventFlagFromImmediate_result_pet_tryStartRandomEncounterStep_808CB0C;
/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline call_ClearEventFlagFromImmediate_result_pet_tryStartRandomEncounterStep_808CB0C call_ClearEventFlagFromImmediate_pet_tryStartRandomEncounterStep_808CB0C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
    return (call_ClearEventFlagFromImmediate_result_pet_tryStartRandomEncounterStep_808CB0C){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r2, r5, r10; } call_jackIn_attemptJackInCheck_808CFB8_result;
/* register form: generated wrapper: jackIn_attemptJackInCheck_808CFB8 has no typed C interface yet */
static inline call_jackIn_attemptJackInCheck_808CFB8_result call_jackIn_attemptJackInCheck_808CFB8(void) {
    GUEST_CALL(jackIn_attemptJackInCheck_808CFB8);
    return (call_jackIn_attemptJackInCheck_808CFB8_result){ R0, R2, R5, R10 };
}

typedef struct { u32 r2, r5, r9, r12; } call_sub_808D108_result;
/* register form: generated wrapper: ow_setupMapTiles_808D108 has no typed C interface yet */
static inline call_sub_808D108_result call_sub_808D108(void) {
    GUEST_CALL(ow_setupMapTiles_808D108);
    return (call_sub_808D108_result){ R2, R5, R9, R12 };
}

typedef struct { u32 r0, r1, r2, r3, r10; } call_battle_pickBattleStateInitPtr_808CC34_result;
/* register form: generated wrapper: battle_pickBattleStateInitPtr_808CC34 has no typed C interface yet */
static inline call_battle_pickBattleStateInitPtr_808CC34_result call_battle_pickBattleStateInitPtr_808CC34(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_pickBattleStateInitPtr_808CC34);
    return (call_battle_pickBattleStateInitPtr_808CC34_result){ R0, R1, R2, R3, R10 };
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_StartCutscene_result_pet_tryStartRandomEncounterStep_808CB0C;
/* register form: generated wrapper: StartCutscene has no typed C interface yet */
static inline call_StartCutscene_result_pet_tryStartRandomEncounterStep_808CB0C call_StartCutscene_pet_tryStartRandomEncounterStep_808CB0C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(StartCutscene);
    return (call_StartCutscene_result_pet_tryStartRandomEncounterStep_808CB0C){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* pet_tryStartRandomEncounterStep_808CB0C: 0808CB0C-0808CBA0 THUMB */

/* Offsets into the encounter-step data block (base loaded from 0x808CA44) */
#define ENC_ACTIVE_OFFSET  0xC  /* u8: non-zero while an encounter is in progress */
#define ENC_STEPS_OFFSET   0x3  /* u8: countdown before re-rolling the encounter */
#define ENC_SAVED_X        0x1A /* u16: player X snapshotted at first activation */
#define ENC_SAVED_Y        0x1C /* u16: player Y snapshotted at first activation */
#define ENC_STEP_RESET     0x20 /* step-counter value written on (re-)activation */

/* Map group where random encounters are suppressed */
#define MAP_GROUP_NO_ENCOUNTER 0x85

/* Sound IDs played when an encounter attempt fires */
#define SOUND_ENCOUNTER_ATTEMPT     0x1F
#define SOUND_ENCOUNTER_ATTEMPT_SUB 0x13B

/* PET-menu data flag index that blocks encounters */
#define PET_MENU_FLAG_INDEX 1

/* Battle-init sentinel meaning "no battle to start" */
#define BATTLE_INIT_NONE 0

/* jackIn check success code */
#define JACKIN_SUCCESS 1

typedef struct { u32 r2, r3, r10, r11; } EncounterStepResult;

/* Drives the overworld random-encounter step machine for the PET area.
 * Returns the registers (r2, r3, r10, r11) that the original leaves for callers.
 * r5/r9/r12 are preserved by the original's push/pop, so they are not returned. */
static EncounterStepResult pet_tryStartRandomEncounterStep(u32 r10, u32 r11) {
    /* Local data-pointer slot (caller's r5 is restored by pop; this is the working copy) */
    u32 dataPtr = LD32(A_dword_808CA44);

    u32 r2 = 0, r3 = 0;

    /* ---- Event gate: both EVENT_9D6 and EVENT_9D8 must be set ---- */
    {
        call_TestEventFlagFromImmediate_result_pet_tryStartRandomEncounterStep_808CB0C f1 =
            call_TestEventFlagFromImmediate_pet_tryStartRandomEncounterStep_808CB0C(9, 0xD6);
        r3 = f1.r3;

        if (!f1.z) {
            /* EVENT_9D6 set: initialise sub-state */
            call_sub_808D11C_result init = call_sub_808D11C();
            r2 = init.r2;
            dataPtr = init.r5;

            call_TestEventFlagFromImmediate_result_pet_tryStartRandomEncounterStep_808CB0C f2 =
                call_TestEventFlagFromImmediate_pet_tryStartRandomEncounterStep_808CB0C(9, 0xD8);
            r3 = f2.r3;
            if (f2.z) {
                /* EVENT_9D8 not set — abort */
                return (EncounterStepResult){r2, r3, r10, r11};
            }

            /* Skip encounter on the dedicated no-encounter map */
            u32 gameStatePtr = LD32(r10 + oToolkit_GameStatePtr);
            u8 mapGroup = LD8(gameStatePtr + oGameState_MapGroup);
            if (mapGroup != MAP_GROUP_NO_ENCOUNTER) {
                call_sound_8000672_result_pet_tryStartRandomEncounterStep_808CB0C snd =
                    call_sound_8000672_pet_tryStartRandomEncounterStep_808CB0C(SOUND_ENCOUNTER_ATTEMPT,
                                       SOUND_ENCOUNTER_ATTEMPT_SUB, r2);
                r3 = snd.r3;
                dataPtr = snd.r5;
                r10 = snd.r10;
            }
        }
    }

    /* ---- PET-menu guard: if the menu flag is set, no encounter ---- */
    {
        call_TestPETMenuDataFlag_result pet = call_TestPETMenuDataFlag(PET_MENU_FLAG_INDEX);
        r2 = pet.r2;
        if (!pet.z) {
            return (EncounterStepResult){r2, r3, r10, r11};
        }
    }

    /* ---- First activation: snapshot player position ---- */
    if (LD8(dataPtr + ENC_ACTIVE_OFFSET) == 0) {
        call_ReadOWPlayerObjectCoords_result coords = call_ReadOWPlayerObjectCoords();
        u32 playerX = (u32)((s32)coords.r0 >> 16);
        u32 playerY = (u32)((s32)coords.r1 >> 16);
        ST16(dataPtr + ENC_SAVED_X, playerX);
        ST16(dataPtr + ENC_SAVED_Y, playerY);
        ST8(dataPtr + ENC_STEPS_OFFSET, ENC_STEP_RESET);
        r2 = coords.r2;
        r3 = coords.r3;
    }

    /* ---- Step-counter tick: when it reaches zero, re-roll the encounter ---- */
    {
        u8 steps = LD8(dataPtr + ENC_STEPS_OFFSET);
        steps--;
        ST8(dataPtr + ENC_STEPS_OFFSET, steps);

        if (steps == 0) {
            call_sub_808CCAC_result reset = call_sub_808CCAC(dataPtr);
            dataPtr = reset.r5;
            ST8(dataPtr + ENC_STEPS_OFFSET, ENC_STEP_RESET);
        }
    }

    /* ---- Touch trigger / jack-in branch ---- */
    {
        call_reqBBS_pollTouchTrigger_808CD04_result touch =
            call_reqBBS_pollTouchTrigger_808CD04();
        r2 = touch.r2;
        dataPtr = touch.r5;
        r10 = touch.r10;
        r11 = touch.r11;

        if (!touch.z) {
            /* Touch confirmed → reward / encounter step */
            call_pet_randomEncounterRewardStep_808CD24_result reward =
                call_pet_randomEncounterRewardStep_808CD24(r2);
            r2 = reward.r2;
            dataPtr = reward.r5;
            r10 = reward.r10;
        } else {
            /* No touch → try the jack-in shortcut */
            call_ClearEventFlagFromImmediate_pet_tryStartRandomEncounterStep_808CB0C(0xB, 0xF1);

            call_jackIn_attemptJackInCheck_808CFB8_result jack =
                call_jackIn_attemptJackInCheck_808CFB8();
            r2 = jack.r2;
            dataPtr = jack.r5;
            r10 = jack.r10;

            if (jack.r0 == JACKIN_SUCCESS) {
                call_sub_808D108_result d108 = call_sub_808D108();
                r2 = d108.r2;
                dataPtr = d108.r5;
            }
        }
    }

    /* ---- Battle initialization ---- */
    {
        call_battle_pickBattleStateInitPtr_808CC34_result battle =
            call_battle_pickBattleStateInitPtr_808CC34(dataPtr);
        r2 = battle.r2;
        r3 = battle.r3;
        r10 = battle.r10;

        if (battle.r0 != BATTLE_INIT_NONE) {
            call_StartCutscene_result_pet_tryStartRandomEncounterStep_808CB0C cut =
                call_StartCutscene_pet_tryStartRandomEncounterStep_808CB0C(battle.r0, battle.r1);
            r2 = cut.r2;
            r3 = cut.r3;
        }
    }

    return (EncounterStepResult){r2, r3, r10, r11};
}

Next pet_tryStartRandomEncounterStep_808CB0C(void) {
    EncounterStepResult out = pet_tryStartRandomEncounterStep(R10, R11);
    R2 = out.r2;
    R3 = out.r3;
    R10 = out.r10;
    R11 = out.r11;
    RETURN_TO_CALLER();
}

/* ── spawnOverworldMapObjects_808CBA0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r6; } call_SpawnOverworldMapObject_result_sub_808CBA0;
/* register form: generated wrapper: SpawnOverworldMapObject has no typed C interface yet */
static inline call_SpawnOverworldMapObject_result_sub_808CBA0 call_SpawnOverworldMapObject_sub_808CBA0(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(SpawnOverworldMapObject);
    return (call_SpawnOverworldMapObject_result_sub_808CBA0){ R2, R3, R6 };
}

/* spawnOverworldMapObjects_808CBA0: spawn the initial batch of overworld map objects.
 *
 * The number of objects depends on where the player is:
 *   - map group 0x85: 12 objects
 *   - any other map with a nonzero MapNumber: 9 objects
 *   - otherwise: 7 objects
 * Each one is spawned as SpawnOverworldMapObject(0x10, 0, 0, 0, index) for
 * index = 0 .. count-1.
 *
 * Register notes: the original brackets the whole routine with
 * push/pop {r4-r7}, so r4-r7 reach the caller exactly as they arrived,
 * even though SpawnOverworldMapObject loads r4 and clobbers r5/r6.
 * The last callee call leaves r2 (and r3, a clobber) behind for the caller. */
static void spawn_overworld_map_objects(u32 toolkit) {
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);

    u32 count;
    if (LD8(gameState + oGameState_MapGroup) == 0x85) {
        count = 12;
    } else if (LD8(gameState + oGameState_MapNumber) != 0) {
        count = 9;
    } else {
        count = 7;
    }

    /* register form: SpawnOverworldMapObject clobbers r5/r6 and its wrapper
     * loads r4, but this function must hand r4-r7 back to its caller
     * unchanged, so they have to be saved and restored around the calls
     * (the original does this with push/pop {r4-r7} / push/pop {r5}). */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;

    for (u32 index = 0; index < count; index++) {
        call_SpawnOverworldMapObject_sub_808CBA0(0x10, 0, 0, 0, index);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
}

/* spawnOverworldMapObjects_808CBA0(r10 = toolkit) -> r2 (left by the last SpawnOverworldMapObject
 * call); r4-r7 end exactly as they arrived, so the adapter leaves them alone. */
Next spawnOverworldMapObjects_808CBA0(void) {
    spawn_overworld_map_objects(R10);
    RETURN_TO_CALLER();
}

/* ── overworld_leaveArea_808CBD8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r6; } call_SpawnOverworldMapObject_result;
/* register form: generated wrapper: SpawnOverworldMapObject has no typed C interface yet */
static inline call_SpawnOverworldMapObject_result call_SpawnOverworldMapObject(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(SpawnOverworldMapObject);
    return (call_SpawnOverworldMapObject_result){ R2, R4, R6 };
}

/* register form: generated wrapper: reqBBS_setFlag_e17b0f7_8140A00 has no typed C interface yet */
static inline void call_reqBBS_setFlag_e17b0f7_8140A00(void) {
    GUEST_CALL(reqBBS_setFlag_e17b0f7_8140A00);
}

typedef struct { u32 r3; } call_gameState_setUnk09_8001172_result;
/* register form: generated wrapper: gameState_setUnk09_8001172 has no typed C interface yet */
static inline call_gameState_setUnk09_8001172_result call_gameState_setUnk09_8001172_sub_808CBD8(u32 r0) {
    R0 = r0;
    GUEST_CALL(gameState_setUnk09_8001172);
    return (call_gameState_setUnk09_8001172_result){ R3 };
}

typedef struct { u32 r3, r10; bool c, v; } call_sound_8000672_result;
/* register form: generated wrapper: sound_dispatchSmashHandler_8000672 has no typed C interface yet */
static inline call_sound_8000672_result call_sound_8000672(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(sound_dispatchSmashHandler_8000672);
    return (call_sound_8000672_result){ R3, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Leaving an area: spawn overworld map object 0x11 (the caller's r4 is the
 * fifth argument), set the BBS flag, switch game state to 3, and play
 * sound 0x1F/0x13B unless the current map group is 0x85.
 *
 * SpawnOverworldMapObject clobbers r5 (not exposed in its wrapper result)
 * and returns r4/r6, so the adapter must restore those three to their
 * entry values, mirroring the original's push/pop of {r5} and the outer
 * pop of {r4-r7}. */
static u32 leave_area_event(u32 callerR4, u32 toolkit) {
    call_SpawnOverworldMapObject_result spawn =
        call_SpawnOverworldMapObject(0x11, 0, 0, 0, callerR4);

    call_reqBBS_setFlag_e17b0f7_8140A00();
    call_gameState_setUnk09_8001172_sub_808CBD8(3);

    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    if (LD8(gameState + oGameState_MapGroup) != 0x85)
        call_sound_8000672(0x1F, 0x13B, spawn.r2);

    return 0;
}

/* overworld_leaveArea_808CBD8(r4 = passed to SpawnOverworldMapObject, r10 = toolkit) -> r0=0, nz.
 * Preserved: r4 r5 r6 r7 r10. Outputs from callees: r2 (Spawn), r3 (gameState/sound). */
Next overworld_leaveArea_808CBD8(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6;
    u32 r0val = leave_area_event(saved_r4, R10);
    R4 = saved_r4, R5 = saved_r5, R6 = saved_r6;
    SET_FLAGS_NZ(r0val);
    RETURN_TO_CALLER();
}

/* ── battle_checkPlayerActorStoryFlag_808CC10 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlagFromImmediate_result_battle_checkPlayerActorStoryFlag_808CC10;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_battle_checkPlayerActorStoryFlag_808CC10 call_TestEventFlagFromImmediate_battle_checkPlayerActorStoryFlag_808CC10(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_battle_checkPlayerActorStoryFlag_808CC10){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3; } call_SetEventFlagFromImmediate_result;
/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline call_SetEventFlagFromImmediate_result call_SetEventFlagFromImmediate(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
    return (call_SetEventFlagFromImmediate_result){ R3 };
}

/* EVENT_9D8, split the way TestEventFlagFromImmediate/SetEventFlagFromImmediate take it:
 * r0 = flag bank, r1 = flag index within the bank. */
#define EVENT_9D8_BANK  9
#define EVENT_9D8_INDEX 0xD8

/* Battle actor header field: the player actor's story/state byte lives at +4. */
#define ACTOR_STATE_BYTE 4

/* Check the player actor's story state: if it is in state 2 and story flag EVENT_9D8
 * is not yet set, set the flag and report that we just did.
 * r4/r5/r6 are only carried through unchanged (the original pops them back). */
typedef struct {
    u32 r4, r5, r6;   /* passed through untouched */
    bool flag_set;    /* true only on the turn where we set EVENT_9D8 */
} story_flag_check;

static story_flag_check check_player_actor_story_flag(u32 r4, u32 r5, u32 r6)
{
    story_flag_check res = { r4, r5, r6, false };

    /* Only the actor in story state 2 triggers the flag. */
    if (LD8(r5 + ACTOR_STATE_BYTE) != 2)
        return res;

    /* Already set? (TestEventFlagFromImmediate leaves Z clear when the flag is set.) */
    if (!call_TestEventFlagFromImmediate_battle_checkPlayerActorStoryFlag_808CC10(EVENT_9D8_BANK, EVENT_9D8_INDEX).z)
        return res;

    /* First time here: latch the flag and remember it. */
    call_SetEventFlagFromImmediate(EVENT_9D8_BANK, EVENT_9D8_INDEX);
    res.flag_set = true;
    return res;
}

/* battle_checkPlayerActorStoryFlag_808CC10(r5 = player actor, r10 = flag context) -> nz r3 r4 r5 r6 */
Next battle_checkPlayerActorStoryFlag_808CC10(void) {
    story_flag_check res = check_player_actor_story_flag(R4, R5, R6);
    R4 = res.r4;
    R5 = res.r5;
    R6 = res.r6;
    /* r3 is left behind by the TestEventFlagFromImmediate wrapper itself when it runs. */
    SET_FLAGS_NZ((u32)res.flag_set);
    RETURN_TO_CALLER();
}

/* ── ow_storePlayerPosDup ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2; } call_ReadOWPlayerObjectCoords_result_ow_storePlayerPosDup;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result_ow_storePlayerPosDup call_ReadOWPlayerObjectCoords_ow_storePlayerPosDup(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result_ow_storePlayerPosDup){ R0, R1, R2 };
}

/* Copy the player's overworld position (fixed point, >>16 to get the integer
 * part) into both position pairs of the object at r5: fields 0x16/0x18 and
 * the duplicates at 0x1A/0x1C. */
static void store_player_pos(u32 playerObj) {
    call_ReadOWPlayerObjectCoords_result_ow_storePlayerPosDup coords = call_ReadOWPlayerObjectCoords_ow_storePlayerPosDup();

    s16 x = (s16)((s32)coords.r0 >> 16);
    s16 y = (s16)((s32)coords.r1 >> 16);

    ST16(playerObj + 0x16, (u16)x);
    ST16(playerObj + 0x18, (u16)y);
    ST16(playerObj + 0x1A, (u16)x);
    ST16(playerObj + 0x1C, (u16)y);
}

/* ow_storePlayerPosDup(r5 = player object, r10 = coords source) -> no outputs */
Next ow_storePlayerPosDup(void) {
    store_player_pos(R5);
    RETURN_TO_CALLER();
}

/* ── field_calcFacingTargetTile_808CCAC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: GetOWPlayerFacingDirection has no typed C interface yet */
static inline u32 call_GetOWPlayerFacingDirection(void) {
    GUEST_CALL(GetOWPlayerFacingDirection);
    return R0;
}

typedef struct { u32 r0, r1; } call_ReadOWPlayerObjectCoords_result_sub_808CCAC;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result_sub_808CCAC call_ReadOWPlayerObjectCoords_sub_808CCAC(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result_sub_808CCAC){ R0, R1 };
}

#define FACING_TILE_OFFSET 0x18

/* Shift the player's tile coords by one tile in the facing direction and
 * store the result (as signed halfwords) into the given object's fields
 * at +0x1A (x) and +0x1C (y).  Directions 0–7 map to the eight cardinal/
 * diagonal facing values; anything ≥ 7 falls into the default (−y) case. */
static void set_facing_adjusted_coords(u32 obj) {
    s32 facing = (s32)call_GetOWPlayerFacingDirection();

    call_ReadOWPlayerObjectCoords_result_sub_808CCAC coords = call_ReadOWPlayerObjectCoords_sub_808CCAC();
    s32 x = (s32)coords.r0 >> 16;
    s32 y = (s32)coords.r1 >> 16;

    switch (facing) {
    case 0: x += FACING_TILE_OFFSET; y -= FACING_TILE_OFFSET; break;
    case 1: x += FACING_TILE_OFFSET; break;
    case 2: x += FACING_TILE_OFFSET; y += FACING_TILE_OFFSET; break;
    case 3: y += FACING_TILE_OFFSET; break;
    case 4: x -= FACING_TILE_OFFSET; y += FACING_TILE_OFFSET; break;
    case 5: x -= FACING_TILE_OFFSET; y -= FACING_TILE_OFFSET; break;
    case 6: x -= FACING_TILE_OFFSET; y -= FACING_TILE_OFFSET; break;
    default: y -= FACING_TILE_OFFSET; break;
    }

    ST16(obj + 0x1A, (u16)(s16)x);
    ST16(obj + 0x1C, (u16)(s16)y);
}

/* field_calcFacingTargetTile_808CCAC(r5 = object pointer, r10 = player context) -> r5 (preserved) */
Next field_calcFacingTargetTile_808CCAC(void) {
    set_facing_adjusted_coords(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_pollTouchTrigger_808CD04 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3, r4, r5, r8, r9, r10, r11, r12; } call_reqBBS_81409E4_result;
/* register form: generated wrapper: reqBBS_81409E4 has no typed C interface yet */
static inline call_reqBBS_81409E4_result call_reqBBS_81409E4(void) {
    GUEST_CALL(reqBBS_81409E4);
    return (call_reqBBS_81409E4_result){ R0, R2, R3, R4, R5, R8, R9, R10, R11, R12 };
}

/* Poll the touch trigger (reqBBS_81409E4) and record the button id.
 *
 * The callee reports which touch input occurred in r0.  If it is one of the
 * ids 0xC0..0xCC, the id is stored into the work area's trigger byte
 * (byte_2000B45, reached through the pointer at dword_808CA44) and the
 * function reports success.  Returns nonzero when a trigger id was recorded.
 *
 * r2/r5/r9/r10/r11/r12 on exit are whatever reqBBS_81409E4 left behind; the
 * generated wrapper already leaves them in place, so nothing here touches them.
 */
static u32 poll_touch_trigger(void) {
    u32 workBase = LD32(A_dword_808CA44);   /* pointer to the work area (0x2000B40) */
    u32 triggerId = call_reqBBS_81409E4().r0;

    if ((s32)triggerId >= (s32)0xC0 && (s32)triggerId <= (s32)0xCC) {
        ST8(workBase + 5, triggerId);       /* byte_2000B45: last touch id */
        return 1;
    }
    return 0;
}

/* reqBBS_pollTouchTrigger_808CD04: no register inputs (r10 passes through to
 * the callee untouched); r0 = result, NZ from the result. */
Next reqBBS_pollTouchTrigger_808CD04(void) {
    R0 = poll_touch_trigger();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── pet_randomEncounterRewardStep_808CD24 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_pet_randomEncounterRewardStep_808CD24(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

typedef struct { bool z, n; } call_TestEventFlag_result_pet_randomEncounterRewardStep_808CD24;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_pet_randomEncounterRewardStep_808CD24 call_TestEventFlag_pet_randomEncounterRewardStep_808CD24(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_pet_randomEncounterRewardStep_808CD24){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline void call_SetEventFlag_pet_randomEncounterRewardStep_808CD24(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
}

typedef struct { u32 r1, r3; bool z, n; } call_TestEventFlagFromImmediate_result_pet_randomEncounterRewardStep_808CD24;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_pet_randomEncounterRewardStep_808CD24 call_TestEventFlagFromImmediate_pet_randomEncounterRewardStep_808CD24(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_pet_randomEncounterRewardStep_808CD24){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_pet_randomEncounterRewardStep_808CD24;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_pet_randomEncounterRewardStep_808CD24 call_PlaySoundEffect_pet_randomEncounterRewardStep_808CD24(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_pet_randomEncounterRewardStep_808CD24){ R2, R4, R5, R6, R7, R10 };
}

/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline void call_SetEventFlagFromImmediate_pet_randomEncounterRewardStep_808CD24(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/*
 * Pet random-encounter reward step.
 *
 * Each AI mode owns a table of u16 "area reward" flag IDs indexed by the
 * pet's area byte (stored as area + 0xC0).  If this area's flag is still
 * clear, set it.  The very first time any reward is granted (tracked by
 * EVENT_BF1 in flag group 0xB, bit 0xF1), also play the jingle.
 *
 * r2, r5, r10 are passed through; PlaySoundEffect may alter r2 so we
 * return its post-call value.
 */

typedef struct { u32 r2, r5, r10; } RewardStepResult;

static RewardStepResult pet_randomEncounterRewardStep_body(u32 r2, u32 r5, u32 r10)
{
    RewardStepResult out = { r2, r5, r10 };

    /* Pet AI battle-state pointer; query the active AI mode for this map group. */
    u32 aiState = LD32(A_dword_808CA44);
    u32 mode = call_ai_getAIModeForMapGroup_808D084_pet_randomEncounterRewardStep_808CD24();

    /* Pick this mode's flag-ID list, then the entry for the current area. */
    u32 flagList = LD32(A_off_808CD68 + mode * 4);
    u32 area = LD8(aiState + 5);               /* area byte lives at +5, base 0xC0 */
    u16 flag = LD16(flagList + (area - 0xC0) * 2);

    /* If the area reward flag is already set, nothing to do. */
    if (!call_TestEventFlag_pet_randomEncounterRewardStep_808CD24(flag).z)           /* Z set = flag not set yet */
        return out;

    call_SetEventFlag_pet_randomEncounterRewardStep_808CD24(flag);

    /* First reward ever: play the jingle once (EVENT_BF1 tracks it). */
    if (call_TestEventFlagFromImmediate_pet_randomEncounterRewardStep_808CD24(0xB, 0xF1).z) {  /* Z set = BF1 not yet set */
        call_PlaySoundEffect_result_pet_randomEncounterRewardStep_808CD24 sfx = call_PlaySoundEffect_pet_randomEncounterRewardStep_808CD24(0xD4, r2);
        out.r2 = sfx.r2;
        out.r10 = sfx.r10;
        call_SetEventFlagFromImmediate_pet_randomEncounterRewardStep_808CD24(0xB, 0xF1);
    }

    return out;
}

/* pet_randomEncounterRewardStep_808CD24(r2, r5, r10) -> r2, r5, r10 */
Next pet_randomEncounterRewardStep_808CD24(void) {
    RewardStepResult out = pet_randomEncounterRewardStep_body(R2, R5, R10);
    R2  = out.r2;
    R5  = out.r5;
    R10 = out.r10;
    RETURN_TO_CALLER();
}

/* ── battle_checkLockedEventTable_808CDC4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result_battle_checkLockedEventTable_808CDC4;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_battle_checkLockedEventTable_808CDC4 call_TestEventFlag_battle_checkLockedEventTable_808CDC4(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_battle_checkLockedEventTable_808CDC4){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3, r4, r5, r6, r7; bool z, n; } call_TestEventFlagRange_result;
/* register form: generated wrapper: TestEventFlagRange has no typed C interface yet */
static inline call_TestEventFlagRange_result call_TestEventFlagRange(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(TestEventFlagRange);
    return (call_TestEventFlagRange_result){ R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Locked-event table used by the battle AI. Each entry is 8 bytes:
 *   [0..1] u16 gate flag   - entry only applies when this event flag is set
 *   [2..3] u16 range start - first flag of the range test
 *   [4..5] u16 range end   - last flag of the range test
 *   [6..7] padding
 * An entry "locks" when its gate flag is set AND one flag in the range is set. */

struct CheckLockedEventsResult {
    u32 index;  /* r1: ai_mode * 3 + matching entry index (0 when nothing matched) */
    u32 r3;     /* r3 exactly as the last TestEventFlag/TestEventFlagRange left it */
    u32 found;  /* 1 when a locked event matched, else 0 - drives the N/Z flags */
};

/* Scan the locked-event table for the current AI map group.
 * Returns whether an entry locked and which one; r3 tracking matters because
 * callers can see the register the last flag-test call left behind. */
static struct CheckLockedEventsResult check_locked_event_table(void) {
    struct CheckLockedEventsResult out;
    u32 ai_mode = call_ai_getAIModeForMapGroup_808D084();
    u32 table = LD32(A_off_808CEBC + ai_mode * 4);
    s32 limit = ((s32)ai_mode < 3) ? 3 : 4;
    u32 last_r3 = 0;
    u32 found = 0;
    s32 i = 0;

    while (i < limit) {
        u32 entry = table + (u32)i * 8;

        /* Gate flag: entry skipped entirely when the flag is clear. */
        call_TestEventFlag_result_battle_checkLockedEventTable_808CDC4 gate = call_TestEventFlag_battle_checkLockedEventTable_808CDC4(LD16(entry));
        last_r3 = gate.r3;
        if (gate.z) {
            i++;
            continue;
        }

        /* Range test: locked when one of the flags in [start, end] is set. */
        call_TestEventFlagRange_result range =
            call_TestEventFlagRange(LD16(entry + 2), LD16(entry + 4));
        last_r3 = range.r3;
        if (!range.z) {
            found = 1;
            break;              /* i stays on the matching entry */
        }
        i++;
    }

    /* On a match the original reports the entry it stopped at; when the whole
     * table is scanned without a match it resets the index to 0. */
    out.index = ai_mode * 3 + (found ? (u32)i : 0);
    out.r3 = last_r3;
    out.found = found;
    return out;
}

/* battle_checkLockedEventTable_808CDC4 (r10 = battle context) ->
 *   nz: Z clear when a locked event matched (tst of the found flag)
 *   r1: ai_mode * 3 + matching entry index
 *   r2: the entry r8 value (the original pops its saved r8 into r2)
 *   r3: leftover from the last flag-test call
 *   r4/r6 are pushed and popped by the original, so they keep their entry
 *   values and the adapter must not touch them. */
Next battle_checkLockedEventTable_808CDC4(void) {
    struct CheckLockedEventsResult res = check_locked_event_table();
    R1 = res.index;
    R2 = R8;
    R3 = res.r3;
    SET_FLAGS_NZ(res.found);
    RETURN_TO_CALLER();
}

/* ── init_clearBeastSpEventFlagTable ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_init_clearBeastSpEventFlagTable(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

typedef struct { bool z, n; } call_TestEventFlag_result_init_clearBeastSpEventFlagTable;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_init_clearBeastSpEventFlagTable call_TestEventFlag_init_clearBeastSpEventFlagTable(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_init_clearBeastSpEventFlagTable){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r4, r5; } call_ClearEventFlagRange_result_init_clearBeastSpEventFlagTable;
/* register form: generated wrapper: ClearEventFlagRange has no typed C interface yet */
static inline call_ClearEventFlagRange_result_init_clearBeastSpEventFlagTable call_ClearEventFlagRange_init_clearBeastSpEventFlagTable(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(ClearEventFlagRange);
    return (call_ClearEventFlagRange_result_init_clearBeastSpEventFlagTable){ R4, R5 };
}

/* Beast SP event-flag table entry (8 bytes):
 *   +0: u16 gate flag   – must be set for this entry to act
 *   +2: u16 start flag  – beginning of the range to clear
 *   +4: u16 end flag    – end of the range to clear */

/* Clear all Beast SP event-flag ranges whose gate flag is set,
 * for the AI mode matching the current map group.
 * Modes 0-2 have 3 entries; modes >= 3 have 4 entries.
 * Returns 3, the r2 loop-end constant the original leaves behind. */
static u32 clear_beast_sp_event_flags(void) {
    u32 mode = call_ai_getAIModeForMapGroup_808D084_init_clearBeastSpEventFlagTable();

    /* ldr r4, off_808CEB8 (= A_off_808CEBC); ldr r4, [r4, r0] */
    u32 entry_base = LD32(A_off_808CEBC + (mode << 2));

    u32 num_entries = ((s32)mode < 3) ? 3 : 4;

    for (u32 i = 0; i < num_entries; i++) {
        u32 offset = i << 3;  /* 8 bytes per entry */

        u16 gate = LD16(entry_base + offset);
        if (!call_TestEventFlag_init_clearBeastSpEventFlagTable((u32)gate).z) {
            /* Flag is set – clear the range it guards */
            u16 start = LD16(entry_base + offset + 2);
            u16 end   = LD16(entry_base + offset + 4);
            call_ClearEventFlagRange_init_clearBeastSpEventFlagTable((u32)start, (u32)end);
        }
    }

    return 3;
}

/* init_clearBeastSpEventFlagTable(r10, lr) -> r2 r7
 * r4-r8 are preserved via push/pop (not reproduced here);
 * r2 receives the loop constant 3. */
Next init_clearBeastSpEventFlagTable(void) {
    R2 = clear_beast_sp_event_flags();
    RETURN_TO_CALLER();
}

/* ── init_applyBeastSpScenarioEventFlags ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_init_applyBeastSpScenarioEventFlags(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline void call_SetEventFlag_init_applyBeastSpScenarioEventFlags(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
}

typedef struct { u32 r3, r4, r5; } call_ClearEventFlagRange_result_init_applyBeastSpScenarioEventFlags;
/* register form: generated wrapper: ClearEventFlagRange has no typed C interface yet */
static inline call_ClearEventFlagRange_result_init_applyBeastSpScenarioEventFlags call_ClearEventFlagRange_init_applyBeastSpScenarioEventFlags(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(ClearEventFlagRange);
    return (call_ClearEventFlagRange_result_init_applyBeastSpScenarioEventFlags){ R3, R4, R5 };
}

/* Table of per-mode flag entry arrays (indexed by AI mode 0..3). */
#define BEAST_SP_FLAG_TABLES 0x0808CEBC

/* Each entry is 8 bytes: [u16 setFlag | u16 clearStart | u16 clearEnd | u16 padding] */

static void apply_beast_sp_scenario_event_flags(void) {
    u32 ai_mode = call_ai_getAIModeForMapGroup_808D084_init_applyBeastSpScenarioEventFlags();

    /* Select the flag-entry table for this AI mode. */
    u32 table_ptr = LD32(BEAST_SP_FLAG_TABLES + (ai_mode << 2));

    /* Modes 0-2 have 3 entries; mode 3 has 4 entries. */
    u32 entry_count = ((s32)ai_mode < 3) ? 3 : 4;

    for (u32 i = 0; i < entry_count; i++) {
        u32 base = table_ptr + (i << 3); /* 8 bytes per entry */

        /* Set the "achieved" flag for this scenario beat. */
        u32 set_flag = LD16(base);
        call_SetEventFlag_init_applyBeastSpScenarioEventFlags(set_flag);

        /* Clear the previous beats' flags in [start, end]. */
        u32 clear_start = LD16(base + 2);
        u32 clear_end   = LD16(base + 4);
        call_ClearEventFlagRange_init_applyBeastSpScenarioEventFlags(clear_start, clear_end);
    }
}

/* 0808CE70: init_applyBeastSpScenarioEventFlags
 * inputs: r10, lr;  outputs: r7 (preserved);  clobbers: r0-r3, lr, nzcv */
Next init_applyBeastSpScenarioEventFlags(void) {
    apply_beast_sp_scenario_event_flags();
    RETURN_TO_CALLER();
}

/* ── ai_testMapGroupEventFlag ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_ai_testMapGroupEventFlag(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result call_TestEventFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Check the event flag associated with the current map group.
 *
 * ai_getAIModeForMapGroup_808D084 returns an AI mode index for the map group
 * (r10); that index selects a u16 flag id from a table at byte_808CF84, and
 * the flag is tested. Returns 1 when the flag is set, 0 otherwise.
 * The wrapper also returns the entry r7 unchanged (an output the original
 * leaves alone on every path, since it only pushes/pops it). */
typedef struct {
    u32 result;   /* -> r0: 1 if the map group's event flag is set, else 0 */
    u32 r7;       /* -> r7: passed through unchanged */
} MapGroupFlagCheck;

static MapGroupFlagCheck test_map_group_event_flag(u32 r7) {
    MapGroupFlagCheck out;

    u32 mode = call_ai_getAIModeForMapGroup_808D084_ai_testMapGroupEventFlag();
    u16 flag = LD16(A_byte_808CF84 + (mode << 1));

    out.result = 0;
    if (!call_TestEventFlag(flag).z)    /* TestEventFlag: Z clear when the flag is set */
        out.result = 1;

    out.r7 = r7;
    return out;
}

Next ai_testMapGroupEventFlag(void) {
    MapGroupFlagCheck out = test_map_group_event_flag(R7);
    R0 = out.result;
    R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── ai_syncModeEventFlag_808CF50 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_sub_808CF50(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

typedef struct { bool z, n; } call_TestEventFlag_result_sub_808CF50;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_sub_808CF50 call_TestEventFlag_sub_808CF50(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_sub_808CF50){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline void call_SetEventFlag_sub_808CF50(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
}

/* Per-map-group AI bookkeeping: look up the story event flag that goes with the
 * current AI mode; if that flag is not set yet, set its partner flag.
 * (byte_808CF7C holds the "test" flag IDs, byte_808CF84 the matching "set"
 * flag IDs, both indexed by AI mode as u16 entries.) */
static void sync_ai_mode_flags(void) {
    u32 modeOffset = call_ai_getAIModeForMapGroup_808D084_sub_808CF50() * 2u;   /* byte offset into the u16 tables */

    /* TestEventFlag leaves Z set when the flag is clear, so only then set it. */
    if (!call_TestEventFlag_sub_808CF50(LD16(A_byte_808CF7C + modeOffset)).z)
        return;

    call_SetEventFlag_sub_808CF50(LD16(A_byte_808CF84 + modeOffset));
}

/* ai_syncModeEventFlag_808CF50() -> r7 (r7 comes back exactly as it entered: the original's
 * pop {r4-r7,pc} restores it from the stack) */
Next ai_syncModeEventFlag_808CF50(void) {
    sync_ai_mode_flags();
    R7 = R7;    /* unchanged output: restored to its entry value */
    RETURN_TO_CALLER();
}

/* ── battle_chatbox_setCursorTextFromEventFlag_808CF8C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlagFromImmediate_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C call_TestEventFlagFromImmediate_battle_chatbox_setCursorTextFromEventFlag_808CF8C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3, r5, r6, r7, r10; } call_PlaySoundEffect_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C call_PlaySoundEffect_battle_chatbox_setCursorTextFromEventFlag_808CF8C(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C){ R2, R3, R5, R6, R7, R10 };
}

/* Battle chatbox: set the cursor text from an event flag (bn6f 0808CF8C).
 *
 * r5 points at an object whose halfword at +6 holds a text id, r10 is the
 * toolkit. If event flag EVENT_9D9 is set, the text id is copied into the
 * chatbox's cursor text field (offset 0x4C) and sound 0x122 plays; if the
 * flag is clear, nothing happens.
 *
 * Returns 1 when the text was written, 0 when the flag was clear; callers
 * branch on the result.
 *
 * Register notes:
 *  - The original computes its 0/1 result in r4 but pops the entry value
 *    back into r4 on exit, so r4 is preserved and the adapter leaves it
 *    untouched; only the N/Z flags carry the result out.
 *  - r2/r3/r6 on the taken path are whatever PlaySoundEffect leaves behind
 *    (and r3 on the early path whatever TestEventFlagFromImmediate leaves);
 *    the wrapper calls leave those guest registers in exactly that state,
 *    so nothing is passed through return values. */
static u32 battle_chatbox_setCursorTextFromEventFlag(u32 textSource, u32 toolkit) {
    /* movflag EVENT_9D9: the flag id travels as page in r0, index in r1.
     * TestEventFlagFromImmediate leaves Z set when the flag is clear. */
    if (call_TestEventFlagFromImmediate_battle_chatbox_setCursorTextFromEventFlag_808CF8C(EVENT_9D9 >> 8, EVENT_9D9 & 0xFF).z)
        return 0;

    u32 chatbox = LD32(toolkit + oToolkit_ChatboxPtr);
    ST32(chatbox + 0x4C, LD16(textSource + 6));

    /* r2 still holds the 0x4C cursor-text offset here, as in the original. */
    call_PlaySoundEffect_battle_chatbox_setCursorTextFromEventFlag_808CF8C(0x122, 0x4C);
    return 1;
}

Next battle_chatbox_setCursorTextFromEventFlag_808CF8C(void) {
    u32 written = battle_chatbox_setCursorTextFromEventFlag(R5, R10);
    SET_FLAGS_NZ(written);
    RETURN_TO_CALLER();
}

/* ── battleObject_computePanelOffsetFromCoords_808D034 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1; } call_ai_getAIModeForChipForMapGroup_808D060_result;
/* register form: generated wrapper: ai_getAIModeForChipForMapGroup_808D060 has no typed C interface yet */
static inline call_ai_getAIModeForChipForMapGroup_808D060_result call_ai_getAIModeForChipForMapGroup_808D060(void) {
    GUEST_CALL(ai_getAIModeForChipForMapGroup_808D060);
    return (call_ai_getAIModeForChipForMapGroup_808D060_result){ R0, R1 };
}

/* Result of battleObject_computePanelOffsetFromCoords: the object's panel
 * coordinates (also stored into the object) plus the flat panel lookup index.
 * r4 is carried through unchanged (the original saves/restores it around the
 * body, so callers see their own incoming r4 again). */
typedef struct {
    u32 r0; /* panel X */
    u32 r1; /* panel Y */
    u32 r2; /* panel index: X * 0x100 + Y */
    u32 r4; /* unchanged incoming r4 */
} PanelOffsetResult;

/* Compute a battle object's panel position from its fixed-point coordinates.
 *
 * ai_getAIModeForChipForMapGroup (found via the battle state in r10) returns a
 * pair of 16.16 fixed-point offsets. Their high halves are added to the high
 * halves of the coordinates, the sums are scaled down to panel units (>> 5)
 * and centered by +0x80. The byte-sized results are stored into the object
 * (obj+8 = X, obj+9 = Y) and the flat index X*256 + Y is built for panel
 * lookups. */
static PanelOffsetResult battleObject_computePanelOffsetFromCoords_impl(
    u32 coordX, u32 coordY, u32 obj, u32 savedR4)
{
    call_ai_getAIModeForChipForMapGroup_808D060_result offset =
        call_ai_getAIModeForChipForMapGroup_808D060();

    u32 sumX = offset.r0 + (u32)((s32)coordX >> 16);
    u32 sumY = offset.r1 + (u32)((s32)coordY >> 16);

    u32 panelX = (u32)((s32)sumX >> 5) + 0x80;
    u32 panelY = (u32)((s32)sumY >> 5) + 0x80;

    ST8(obj + 8, panelX);
    ST8(obj + 9, panelY);

    PanelOffsetResult result;
    result.r0 = panelX;
    result.r1 = panelY;
    result.r2 = panelX * 0x100 + panelY;
    result.r4 = savedR4;
    return result;
}

/* battleObject_computePanelOffsetFromCoords_808D034
 *   inputs:  r0 = coordX, r1 = coordY, r5 = object, r10 = battle state
 *   outputs: r0 = panel X, r1 = panel Y, r2 = panel index
 *            r4, r5 preserved (the original pops them back from its prologue)
 *   (nz/c/v are clobbered, callers do not read them) */
Next battleObject_computePanelOffsetFromCoords_808D034(void) {
    PanelOffsetResult result =
        battleObject_computePanelOffsetFromCoords_impl(R0, R1, R5, R4);
    R0 = result.r0;
    R1 = result.r1;
    R2 = result.r2;
    R4 = result.r4;
    RETURN_TO_CALLER();
}

/* ── ai_getAIModeForChipForMapGroup_808D060 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_ai_getAIModeForChipForMapGroup_808D060(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

/* AI mode table: for each map-group AI mode, a pair of signed chip-selection
 * parameters (offset 0 and offset 1 into byte_808D07C). */
#define AI_MODE_TABLE A_byte_808D07C

typedef struct {
    s32 mode_a;   /* returned in r0 */
    s32 mode_b;   /* returned in r1 */
} AIModeChipPair;

/* Fetch the chip-selection parameter pair for the current map group's AI mode.
 * ai_getAIModeForMapGroup_808D084 returns the mode index (r10 -> r0); each
 * table entry is two signed bytes, loaded with sign extension. */
static AIModeChipPair ai_getAIModeChipPair(void) {
    u32 mode = call_ai_getAIModeForMapGroup_808D084_ai_getAIModeForChipForMapGroup_808D060();
    u32 entry = AI_MODE_TABLE + mode * 2;

    AIModeChipPair pair;
    pair.mode_a = LDS8(entry + 0);
    pair.mode_b = LDS8(entry + 1);
    return pair;
}

/* ai_getAIModeForChipForMapGroup_808D060: r10 -> r0 = mode param 0, r1 = mode param 1 */
Next ai_getAIModeForChipForMapGroup_808D060(void) {
    AIModeChipPair pair = ai_getAIModeChipPair();
    R0 = (u32)pair.mode_a;
    R1 = (u32)pair.mode_b;
    RETURN_TO_CALLER();
}

/* ── ai_getAIModeForMapGroup_808D084 ── */
/* AI mode lookup: map group 0x85 (special area) always uses mode 3,
 * otherwise the mode is read from the game state's AI-mode byte. */
static u32 get_ai_mode_for_map_group(u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);

    if (LD8(game_state + oGameState_MapGroup) == 0x85)
        return 3;
    return LD8(game_state + 5);
}

/* ai_getAIModeForMapGroup_808D084(r10 = toolkit) -> r0 */
Next ai_getAIModeForMapGroup_808D084(void) {
    R0 = get_ai_mode_for_map_group(R10);
    RETURN_TO_CALLER();
}

/* ── ow_setupMapTiles_808D108 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r5, r6, r7; } call_sub_8142CB0_result;
/* register form: generated wrapper: battle_resolveMapTileAddr_8142CB0 has no typed C interface yet */
static inline call_sub_8142CB0_result call_sub_8142CB0(void) {
    GUEST_CALL(battle_resolveMapTileAddr_8142CB0);
    return (call_sub_8142CB0_result){ R0, R5, R6, R7 };
}

typedef struct { u32 r5, r6, r7; } call_ow_stampMapTileTemplate_result;
/* register form: generated wrapper: ow_stampMapTileTemplate has no typed C interface yet */
static inline call_ow_stampMapTileTemplate_result call_ow_stampMapTileTemplate(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ow_stampMapTileTemplate);
    return (call_ow_stampMapTileTemplate_result){ R5, R6, R7 };
}

typedef struct { u32 r2, r7, r9, r12; } call_sub_8142D4C_result;
/* register form: generated wrapper: ow_redrawMapTilemapsCb has no typed C interface yet */
static inline call_sub_8142D4C_result call_sub_8142D4C(void) {
    GUEST_CALL(ow_redrawMapTilemapsCb);
    return (call_sub_8142D4C_result){ R2, R7, R9, R12 };
}

/* Overworld map tile setup (0x0808D108).
 *
 * Fetches the current tile template description, stamps it onto the map with
 * template index 0, then refreshes the derived map state. All of the callers'
 * outputs (r2, r5, r9, r12) are values the callees leave behind: r5 comes from
 * ow_stampMapTileTemplate (the last writer before ow_redrawMapTilemapsCb, which preserves
 * it), and r2/r9/r12 come from ow_redrawMapTilemapsCb itself. */
static void ow_setup_map_tiles(void) {
    u32 tile_template = call_sub_8142CB0().r0;

    call_ow_stampMapTileTemplate(tile_template, 0);
    call_sub_8142D4C();
}

Next ow_setupMapTiles_808D108(void) {
    ow_setup_map_tiles();
    RETURN_TO_CALLER();
}

/* ── sub_808D25C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r12; bool c, v; } call_sub_8048F88_result;
/* register form: generated wrapper: shop_addStockEntry_kind4_8048F88 has no typed C interface yet */
static inline call_sub_8048F88_result call_sub_8048F88(void) {
    GUEST_CALL(shop_addStockEntry_kind4_8048F88);
    return (call_sub_8048F88_result){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_808D25C — run the game-state unpacker shop_addStockEntry_kind4_8048F88, then finish with 0.
 *
 * This function is pure glue: it tail-calls the unpacker (which reads r10 and
 * leaves r1..r9, r12, c, v behind — those are exactly the values its callers
 * observe) and then executes "mov r0, #0", whose only lasting effect is the
 * N/Z flags (N clear, Z set). r0 itself is a clobber, so only the flags and
 * the pass-through registers matter. */

/* Run the unpacker and report the final result value (always 0, matching the
 * original's "mov r0, #0"). The registers shop_addStockEntry_kind4_8048F88 leaves behind stay in
 * the guest register file; nothing here needs to forward them. */
static u32 sub_808D25C_run_unpacker(void) {
    call_sub_8048F88_result regs = call_sub_8048F88();
    (void)regs; /* r2..r12 already hold the unpacker's results in the guest state */
    return 0;
}

Next sub_808D25C(void) {
    u32 result = sub_808D25C_run_unpacker();
    R0 = result;
    SET_FLAGS_NZ(result);
    RETURN_TO_CALLER();
}
