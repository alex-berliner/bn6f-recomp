/*
 * asm25.c: readable functions of bn6f asm25.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── busStation_runBusMoveSequence_controller_808F900 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline void call_ClearEventFlagFromImmediate(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

typedef struct { u32 r4, r5, r6, r7; } call_sub_808FA7C_result;
/* register form: generated wrapper: sub_808FA7C has no typed C interface yet */
static inline call_sub_808FA7C_result call_sub_808FA7C(u32 r4, u32 r5, u32 r6, u32 r7) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(sub_808FA7C);
    return (call_sub_808FA7C_result){ R4, R5, R6, R7 };
}

/* register form: generated wrapper: owPlayer_disableWallCollision_809e254 has no typed C interface yet */
static inline void call_owPlayer_disableWallCollision_809e254(void) {
    GUEST_CALL(owPlayer_disableWallCollision_809e254);
}

typedef struct { u32 r2, r3, r4, r5, r6, r7; bool z, n; } call_owPlayer_stepTowardTarget_result;
/* register form: generated wrapper: owPlayer_stepTowardTarget has no typed C interface yet */
static inline call_owPlayer_stepTowardTarget_result call_owPlayer_stepTowardTarget(void) {
    GUEST_CALL(owPlayer_stepTowardTarget);
    return (call_owPlayer_stepTowardTarget_result){ R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r4, r5; } call_sub_808FA84_result;
/* register form: generated wrapper: sub_808FA84 has no typed C interface yet */
static inline call_sub_808FA84_result call_sub_808FA84(u32 r4, u32 r5, u32 r6, u32 r7) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(sub_808FA84);
    return (call_sub_808FA84_result){ R4, R5 };
}

/* register form: generated wrapper: ZeroOWPlayerNaviPaletteIndex has no typed C interface yet */
static inline void call_ZeroOWPlayerNaviPaletteIndex(void) {
    GUEST_CALL(ZeroOWPlayerNaviPaletteIndex);
}

typedef struct { u32 r1, r2, r4, r5, r6, r7; bool c, v; } call_ow_spawnMapObject1b_result;
/* register form: generated wrapper: ow_spawnMapObject1b has no typed C interface yet */
static inline call_ow_spawnMapObject1b_result call_ow_spawnMapObject1b(u32 r5) {
    R5 = r5;
    GUEST_CALL(ow_spawnMapObject1b);
    return (call_ow_spawnMapObject1b_result){ R1, R2, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5; } call_ow_seatPlayerForBusRide_result;
/* register form: generated wrapper: ow_seatPlayerForBusRide has no typed C interface yet */
static inline call_ow_seatPlayerForBusRide_result call_ow_seatPlayerForBusRide(void) {
    GUEST_CALL(ow_seatPlayerForBusRide);
    return (call_ow_seatPlayerForBusRide_result){ R2, R3, R4, R5 };
}

typedef struct { u32 r3; } call_owPlayer_enableWallCollision_809e248_result;
/* register form: generated wrapper: owPlayer_enableWallCollision_809e248 has no typed C interface yet */
static inline call_owPlayer_enableWallCollision_809e248_result call_owPlayer_enableWallCollision_809e248(void) {
    GUEST_CALL(owPlayer_enableWallCollision_809e248);
    return (call_owPlayer_enableWallCollision_809e248_result){ R3 };
}

typedef struct { u32 r4, r5, r6, r7; } call_resetStruct2000aa0State_809E3B2_result;
/* register form: generated wrapper: resetStruct2000aa0State_809E3B2 has no typed C interface yet */
static inline call_resetStruct2000aa0State_809E3B2_result call_resetStruct2000aa0State_809E3B2(void) {
    GUEST_CALL(resetStruct2000aa0State_809E3B2);
    return (call_resetStruct2000aa0State_809E3B2_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r7; } call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188_result;
/* register form: generated wrapper: owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188 has no typed C interface yet */
static inline call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188_result call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188);
    return (call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188_result){ R7 };
}

typedef struct { u32 r1; bool c, v; } call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result;
/* register form: generated wrapper: owPlayer_setS200ace0_fixedAnimationSelect_809e13c has no typed C interface yet */
static inline call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c(u32 r0) {
    R0 = r0;
    GUEST_CALL(owPlayer_setS200ace0_fixedAnimationSelect_809e13c);
    return (call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result){ R1, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: SetOWPlayerFacingDirection has no typed C interface yet */
static inline void call_SetOWPlayerFacingDirection(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetOWPlayerFacingDirection);
}

typedef struct { u32 r1, r3; bool v; } call_reqBBS_clearFlag_8140A0C_result;
/* register form: generated wrapper: reqBBS_clearFlag_8140A0C has no typed C interface yet */
static inline call_reqBBS_clearFlag_8140A0C_result call_reqBBS_clearFlag_8140A0C(void) {
    GUEST_CALL(reqBBS_clearFlag_8140A0C);
    return (call_reqBBS_clearFlag_8140A0C_result){ R1, R3, OVERFLOW_FLAG() };
}

/* Offsets into the bus-ride state block (the struct pointed at by off_808F5D4). */
#define BUS_STATE       (A_byte_2000312 - 0x2000310)  /* byte: current sequence step   */
#define BUS_ANIM_SELECT (A_byte_200031F - 0x2000310)  /* byte: arrival animation       */
#define BUS_FACING      (A_byte_2000320 - 0x2000310)  /* byte: arrival facing          */
#define BUS_RIDE_DONE   (A_byte_2000321 - 0x2000310)  /* byte: ride-started marker     */
#define BUS_ARRIVE_X    (A_word_2000322  - 0x2000310) /* halfword: arrival x, 1/65536  */
#define BUS_ARRIVE_Y    (A_dword_2000324 - 0x2000310) /* halfword: arrival y, 1/65536  */

/* Result of the sequence tick: r0 is the status (0 = ride finished,
 * 1 = still running); r4-r7 are pass-through, since the original pops
 * them back to their entry values no matter which exit it takes. */
typedef struct {
    u32 r0;
    u32 r4, r5, r6, r7;
} BusRideResult;

/* Run one tick of the bus-ride cutscene state machine. */
static BusRideResult run_bus_ride_sequence(u32 r4, u32 r5, u32 r6, u32 r7) {
    BusRideResult out;
    out.r0 = 1;                 /* default exit: the ride is still in progress */
    out.r4 = r4;
    out.r5 = r5;
    out.r6 = r6;
    out.r7 = r7;

    u32 bus   = LD32(A_off_808F5D4);
    u32 state = LD8(bus + BUS_STATE);

    /* Step 0: the ride has not started yet - mark it started, clear the
     * two gate event flags and set up the bus approach. */
    if ((s32)state <= 0) {
        ST8(bus + BUS_RIDE_DONE, 0xFF);
        call_ClearEventFlagFromImmediate(0xB, 0xE4);    /* EVENT_BE4 */
        call_ClearEventFlagFromImmediate(0xB, 0xE5);    /* EVENT_BE5 */
        call_sub_808FA7C_result begin = call_sub_808FA7C(state, bus, r6, r7);
        state = begin.r4;
        bus   = begin.r5;
        r6    = begin.r6;
        r7    = begin.r7;
        call_owPlayer_disableWallCollision_809e254();
        state = state + 1;
        ST8(bus + BUS_STATE, state);
    }

    /* Step 1: step the player towards the bus until it has reached it. */
    if ((s32)state <= 1) {
        if (!call_owPlayer_stepTowardTarget().z)
            return out;                 /* player still walking: keep riding */
        state = state + 1;
        ST8(bus + BUS_STATE, state);
    }

    /* Step 2: the bus has pulled in - reset the player's navi palette,
     * spawn the bus map object and seat the player inside it. */
    if ((s32)state <= 2) {
        call_sub_808FA84_result arrive = call_sub_808FA84(state, bus, r6, r7);
        state = arrive.r4;
        bus   = arrive.r5;
        call_ZeroOWPlayerNaviPaletteIndex();
        call_ow_spawnMapObject1b(bus);
        call_ow_seatPlayerForBusRide();
        state = state + 1;
        ST8(bus + BUS_STATE, state);
        return out;
    }

    /* Step 3: ride over - walk the player off the bus, then restore wall
     * collision and place the player at the arrival coordinates. */
    if (!call_owPlayer_stepTowardTarget().z)
        return out;

    call_owPlayer_enableWallCollision_809e248();
    state = 0;
    ST8(bus + BUS_STATE, state);
    call_resetStruct2000aa0State_809E3B2();

    u32 x = (u32)LD16(bus + BUS_ARRIVE_X) << 16;
    u32 y = (u32)LD16(bus + BUS_ARRIVE_Y) << 16;
    call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188(
        x, y, 0x00400000);

    call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c(LD8(bus + BUS_ANIM_SELECT));
    call_SetOWPlayerFacingDirection(LD8(bus + BUS_FACING));
    call_reqBBS_clearFlag_8140A0C();

    out.r0 = 0;                 /* ride complete */
    return out;
}

/* busStation_runBusMoveSequence_controller_808F900(r4-r7 passed through) -> r0, nz */
Next busStation_runBusMoveSequence_controller_808F900(void) {
    BusRideResult res = run_bus_ride_sequence(R4, R5, R6, R7);
    R0 = res.r0;
    R4 = res.r4;
    R5 = res.r5;
    R6 = res.r6;
    R7 = res.r7;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── ow_startBeastOutHit ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: GetOWPlayerFacingDirection has no typed C interface yet */
static inline u32 call_GetOWPlayerFacingDirection(void) {
    GUEST_CALL(GetOWPlayerFacingDirection);
    return R0;
}

typedef struct { u32 r1, r2, r3, r4, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_ow_startBeastOutHit;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_ow_startBeastOutHit call_PlaySoundEffect_ow_startBeastOutHit(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_ow_startBeastOutHit){ R1, R2, R3, R4, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* ow_startBeastOutHit -- overworld "Beast Out seized by a hit" kickoff.
 *
 * Writes two bytes into the overworld player object (the pointer at
 * off_808F5D4, object base 0x2000310): the hit direction, looked up from the
 * direction the player is facing, and a 15-frame timer.  Then plays the hit
 * sound effect.  r2 is untouched by the original before the sound call, so it
 * goes straight through as PlaySoundEffect's second argument; r5 is saved and
 * restored, so it leaves as it entered. */

/* Field offsets in the overworld player object. */
#define PLAYER_HIT_DIR     0x18   /* byte_2000328: direction/pose for the hit */
#define PLAYER_HIT_TIMER   0x17   /* dword_2000324+3: countdown, 15 frames */

static u32 start_beast_out_hit(u32 soundArg2, u32 savedR5) {
    u32 player = LD32(A_off_808F5D4);

    /* byte_808FD14 maps facing direction -> hit direction (5,5,7,7,1,1,3,3). */
    u32 facing = call_GetOWPlayerFacingDirection();
    u32 hitDir = LD8(A_byte_808FD14 + facing);

    ST8(player + PLAYER_HIT_DIR, hitDir);
    ST8(player + PLAYER_HIT_TIMER, 0xF);

    call_PlaySoundEffect_ow_startBeastOutHit(SOUND_HIT_6B, soundArg2);

    return savedR5;   /* r5 is only scratch here; the original pops it back */
}

Next ow_startBeastOutHit(void) {
    R5 = start_beast_out_hit(R2, R5);
    SET_FLAGS_NZ(0);          /* movs r0, #0 before the return */
    RETURN_TO_CALLER();
}

/* ── battle_runBattleFrameForCyberWorld_808FEA4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5; } call_battleObject_togglePlayerAllyLockout_808FF44_result;
/* register form: generated wrapper: battleObject_togglePlayerAllyLockout_808FF44 has no typed C interface yet */
static inline call_battleObject_togglePlayerAllyLockout_808FF44_result call_battleObject_togglePlayerAllyLockout_808FF44(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_togglePlayerAllyLockout_808FF44);
    return (call_battleObject_togglePlayerAllyLockout_808FF44_result){ R5 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_battle_callAIDispatch_8046818_result;
/* register form: generated wrapper: battle_callAIDispatch_8046818 has no typed C interface yet */
static inline call_battle_callAIDispatch_8046818_result call_battle_callAIDispatch_8046818(void) {
    GUEST_CALL(battle_callAIDispatch_8046818);
    return (call_battle_callAIDispatch_8046818_result){ R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r1, r2, r3, r8, r9, r10, r11, r12; bool c, v; } call_battleObject_updateSpriteGfxForAll_8046854_result;
/* register form: generated wrapper: battleObject_updateSpriteGfxForAll_8046854 has no typed C interface yet */
static inline call_battleObject_updateSpriteGfxForAll_8046854_result call_battleObject_updateSpriteGfxForAll_8046854(void) {
    GUEST_CALL(battleObject_updateSpriteGfxForAll_8046854);
    return (call_battleObject_updateSpriteGfxForAll_8046854_result){ R1, R2, R3, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* One battle frame of the Cyber World overlay.
 *
 * The frame logic only applies in the Cyber World maps (map group 0x83,
 * or group 0x85 map 2).  There it re-applies the player-ally lockout,
 * dispatches enemy AI, and refreshes every object's sprite graphics.
 *
 * The callees trash r4-r7 (and r5 in particular is repointed by
 * togglePlayerAllyLockout), which is why the original pushes them; the
 * adapter saves and restores them around the typed call.  r8-r12 are
 * outputs: on the early-exit paths the original leaves them untouched,
 * on the full path they are whatever updateSpriteGfxForAll leaves. */
struct CyberFrameRegs {
    u32 r8, r9, r10, r11, r12;
};

static struct CyberFrameRegs runCyberWorldFrame(u32 toolkit, u32 r8, u32 r9,
                                                u32 r10, u32 r11, u32 r12)
{
    struct CyberFrameRegs out;
    out.r8 = r8;
    out.r9 = r9;
    out.r10 = r10;
    out.r11 = r11;
    out.r12 = r12;

    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 mapGroup  = LD8(gameState + oGameState_MapGroup);
    u32 mapNumber = LD8(gameState + oGameState_MapNumber);

    /* Only run in the Cyber World maps. */
    if (mapGroup != 0x83 && (mapGroup != 0x85 || mapNumber != 2))
        return out;

    /* Cyber World state handle from the pointer table. */
    u32 stateHandle = LD32(A_off_808FDB8);

    /* Re-apply the ally lockout; the callee repoints r5 to the state
     * struct it worked on, and that value is what the byte below reads. */
    u32 state = call_battleObject_togglePlayerAllyLockout_808FF44(stateHandle).r5;

    /* byte_2000315 (state+5): when set, skip AI dispatch this frame. */
    if (LD8(state + 5) != 0)
        return out;

    call_battle_callAIDispatch_8046818();

    call_battleObject_updateSpriteGfxForAll_8046854_result gfx =
        call_battleObject_updateSpriteGfxForAll_8046854();
    out.r8  = gfx.r8;
    out.r9  = gfx.r9;
    out.r10 = gfx.r10;
    out.r11 = gfx.r11;
    out.r12 = gfx.r12;
    return out;
}

Next battle_runBattleFrameForCyberWorld_808FEA4(void) {
    /* Save r4-r7: the callees clobber them, the original push/pop keeps
     * them intact for the caller. */
    u32 saveR4 = R4, saveR5 = R5, saveR6 = R6, saveR7 = R7;

    struct CyberFrameRegs out =
        runCyberWorldFrame(R10, R8, R9, R10, R11, R12);

    R4 = saveR4, R5 = saveR5, R6 = saveR6, R7 = saveR7;

    R8 = out.r8, R9 = out.r9, R10 = out.r10, R11 = out.r11, R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── battleObject_togglePlayerAllyLockout_808FF44 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2; bool z, n; } call_TestPETMenuDataFlag_result;
/* register form: generated wrapper: TestPETMenuDataFlag has no typed C interface yet */
static inline call_TestPETMenuDataFlag_result call_TestPETMenuDataFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestPETMenuDataFlag);
    return (call_TestPETMenuDataFlag_result){ R1, R2, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3, r4, r5, r6, r7; } call_sub_80468BA_result;
/* register form: generated wrapper: sub_80468BA has no typed C interface yet */
static inline call_sub_80468BA_result call_sub_80468BA(void) {
    GUEST_CALL(sub_80468BA);
    return (call_sub_80468BA_result){ R3, R4, R5, R6, R7 };
}

typedef struct { u32 r1, r4, r5, r6, r7; } call_battleObject_initAIChipQueueSlot_80468C6_result;
/* register form: generated wrapper: battleObject_initAIChipQueueSlot_80468C6 has no typed C interface yet */
static inline call_battleObject_initAIChipQueueSlot_80468C6_result call_battleObject_initAIChipQueueSlot_80468C6(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(battleObject_initAIChipQueueSlot_80468C6);
    return (call_battleObject_initAIChipQueueSlot_80468C6_result){ R1, R4, R5, R6, R7 };
}

/* Toggle the ally lockout state of the player's battle object (byte at +5).
 * If the PET menu data flag is set, lock the ally out (store 1).
 * Otherwise, if the ally was locked out, clear its AI chip queue and
 * unlock it (store 0); if it was already unlocked, do nothing. */
static u32 battleObject_togglePlayerAllyLockout(u32 object) {
    /* TestPETMenuDataFlag: Z clear when the flag is set */
    if (!call_TestPETMenuDataFlag(1).z) {
        ST8(object + 5, 1);
    } else if (LD8(object + 5) != 0) {
        call_sub_80468BA();
        call_battleObject_initAIChipQueueSlot_80468C6(0, 0, 0);
        ST8(object + 5, 0);
    }
    return object;
}

/* battleObject_togglePlayerAllyLockout_808FF44(r5 = battle object) -> r5
 * The object pointer is passed through unchanged; only the flag byte is written. */
Next battleObject_togglePlayerAllyLockout_808FF44(void) {
    R5 = battleObject_togglePlayerAllyLockout(R5);
    RETURN_TO_CALLER();
}
