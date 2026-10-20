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
/* register form: generated wrapper: thunk_tailLoc808FA90_r0eq0_808FA7C has no typed C interface yet */
static inline call_sub_808FA7C_result call_sub_808FA7C(u32 r4, u32 r5, u32 r6, u32 r7) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(thunk_tailLoc808FA90_r0eq0_808FA7C);
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

/* ── owPlayer_turnTowardTarget_808FA38 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1; } call_ReadOWPlayerObjectCoords_result;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result call_ReadOWPlayerObjectCoords(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result){ R0, R1 };
}

/* register form: generated wrapper: battle_calcAngleOctant_800118A has no typed C interface yet */
static inline u32 call_battle_calcAngleOctant_800118A(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(battle_calcAngleOctant_800118A);
    return R0;
}

typedef struct { bool c, v; } call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_sub_808FA38;
/* register form: generated wrapper: owPlayer_setS200ace0_fixedAnimationSelect_809e13c has no typed C interface yet */
static inline call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_sub_808FA38 call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_sub_808FA38(u32 r0) {
    R0 = r0;
    GUEST_CALL(owPlayer_setS200ace0_fixedAnimationSelect_809e13c);
    return (call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_sub_808FA38){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * owPlayer_turnTowardTarget_808FA38 -- turn the overworld player to face the map's target position.
 *
 * Reads the player's current coordinates, computes the angle octant from the
 * player toward the target position stored in the player object (+0x12/+0x14),
 * and if the player is not already walking in that direction, switches the
 * fixed walk animation (byte at +0x11) to the one the ROM table byte_808FA74
 * picks for that octant: { 0xB, 0xB, 0x9, 0xF, 0xD, 0x0, 0x0, 0x0 }.
 *
 * Octant 0 means no turn is needed. r4/r5 leave exactly as they entered.
 */

/* Angle octant (1..7) -> walk animation id (ROM table at byte_808FA74). */
#define OW_WALK_ANIM_TABLE A_byte_808FA74

/* Offsets within the overworld player object (base 0x2000310). */
#define OWPLAYERAnim      (A_byte_2000321   - 0x2000310)
#define OWPLAYERTargetX   (A_word_2000322   - 0x2000310)
#define OWPLAYERTargetY   (A_dword_2000324  - 0x2000310)

static void player_turn_toward_target(void) {
    u32 player = LD32(A_off_808F5D4);   /* overworld player object pointer */

    call_ReadOWPlayerObjectCoords_result coords = call_ReadOWPlayerObjectCoords();

    /* Target position minus player coordinates, scaled to 16.16 fixed point. */
    u32 dx = ((u32)LD16(player + OWPLAYERTargetX) << 16) - coords.r0;
    u32 dy = ((u32)LD16(player + OWPLAYERTargetY) << 16) - coords.r1;

    u32 octant = call_battle_calcAngleOctant_800118A(dx, dy);
    if (octant == 0)
        return;                         /* already facing the target */

    u32 anim = LD8(OW_WALK_ANIM_TABLE + octant);
    if (anim == LD8(player + OWPLAYERAnim))
        return;                         /* already playing that animation */

    call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_sub_808FA38(anim);
    ST8(player + OWPLAYERAnim, anim);
}

Next owPlayer_turnTowardTarget_808FA38(void) {
    player_turn_toward_target();
    RETURN_TO_CALLER();
}

/* ── thunk_tailLoc808FA90_r0eq0_808FA7C ── */
/*
 * thunk_tailLoc808FA90_r0eq0_808FA7C is a pure thunk: it pushes r4-r7 and the return address, sets
 * r0 = 0 and branches to the routine at 0x0808FA90. The pushed frame is not
 * this function's own — the tail callee ends with pop {r4-r7,pc}, so it
 * restores the caller's r4-r7 and returns straight to thunk_tailLoc808FA90_r0eq0_808FA7C's caller.
 * That is why r4-r7 appear as "outputs" although nothing here computes them:
 * they come back unchanged from the callee's pop. The frame therefore has to
 * be written to the real guest stack, exactly as the original push does.
 */

/* Branch target of "b loc_808FA90" (0x0808FA90). */
#define TAIL_TARGET_808FA90 0x0808FA90u

/* register form: the r4-r7/return-address frame must be written to the real
 * guest stack because the tail callee pops it with pop {r4-r7,pc}, and the
 * transfer happens through the guest code address 0x0808FA90, for which no
 * wrapper is generated. */
static void tail_call_loc_808FA90(void) {
    /* push {r4-r7,lr} */
    SP -= 20;
    ST32(SP, R4);
    ST32(SP + 4, R5);
    ST32(SP + 8, R6);
    ST32(SP + 12, R7);
    ST32(SP + 16, LR);

    /* mov r0, #0 — the single argument of the tail call */
    R0 = 0;

    /* b loc_808FA90. The callee's pop {r4-r7,pc} loads the saved return
     * address into PC, so control goes straight back to thunk_tailLoc808FA90_r0eq0_808FA7C's caller
     * with r4-r7 restored — it never comes back here, exactly as in the
     * original. */
    GUEST_CALL_ADDR(TAIL_TARGET_808FA90);
}

/* thunk_tailLoc808FA90_r0eq0_808FA7C(r4, r5, r6, r7, r10, lr) -> r4 r5 r6 r7 (restored by the callee) */
Next thunk_tailLoc808FA90_r0eq0_808FA7C(void) {
    tail_call_loc_808FA90();
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

/* ── weather_requestMapParams_808FD64 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: req_setFourParams has no typed C interface yet */
static inline void call_req_setFourParams(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(req_setFourParams);
}

/* Per-map parameter table: three 8-byte entries of four halfwords each, read from
 * byte_808FD98. The selected entry's halfwords are passed to req_setFourParams in
 * the high halves of r0-r3 (the "lsl #16" packing the original uses). */
#define MAP_PARAM_TABLE   A_byte_808FD98
#define MAP_PARAM_STRIDE  8
#define UNDERGROUND_MAP_GROUP 0x85   /* map group 0x85 always uses entry 3 */
#define UNDERGROUND_MAP_INDEX 3

/* Load the four halfwords for the current map and hand them to req_setFourParams.
 * r2/r3 afterwards are whatever req_setFourParams leaves there (they are this
 * function's outputs, produced by the callee itself). */
static void request_map_params(u32 toolkit)
{
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);

    u32 index;
    if (LD8(gameState + oGameState_MapGroup) == UNDERGROUND_MAP_GROUP)
        index = UNDERGROUND_MAP_INDEX;
    else
        index = LD8(gameState + oGameState_MapNumber);

    u32 entry = MAP_PARAM_TABLE + index * MAP_PARAM_STRIDE;
    call_req_setFourParams(LD16(entry)     << 16,
                           LD16(entry + 2) << 16,
                           LD16(entry + 4) << 16,
                           LD16(entry + 6) << 16);
}

/* weather_requestMapParams_808FD64(r10 = toolkit) -> r2 r3 left by req_setFourParams */
Next weather_requestMapParams_808FD64(void) {
    request_map_params(R10);
    RETURN_TO_CALLER();
}

/* ── clearWorkArea_808FE64 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_ZeroFillByWord_result_sub_808FE64;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result_sub_808FE64 call_ZeroFillByWord_sub_808FE64(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result_sub_808FE64){ R2 };
}

/* Clear the 0x80-byte work area whose address is stored at dword_808FDBC. */
static void clear_work_area(void) {
    u32 area = LD32(A_dword_808FDBC);   /* off_808FDB8 + 4 */
    call_ZeroFillByWord_sub_808FE64(area, 0x80);
}

/* clearWorkArea_808FE64: zero the work area and return 0 in r0 (clobbered, not an output).
 * inputs: lr; outputs: none; clobbers: nz c v r0 r1 lr; preserved: r2 r3 */
Next clearWorkArea_808FE64(void) {
    clear_work_area();
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

/* ── spawnSevenOverworldMapObjects ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r4, r6; } call_SpawnOverworldMapObject_result_spawnSevenOverworldMapObjects;
/* register form: generated wrapper: SpawnOverworldMapObject has no typed C interface yet */
static inline call_SpawnOverworldMapObject_result_spawnSevenOverworldMapObjects call_SpawnOverworldMapObject_spawnSevenOverworldMapObjects(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(SpawnOverworldMapObject);
    return (call_SpawnOverworldMapObject_result_spawnSevenOverworldMapObjects){ R2, R3, R4, R6 };
}

/* Spawn seven overworld map objects of type 0x1C, all at position (0, 0, 0).
 *
 * The loop counter lives in guest r4, which is also the fifth argument to
 * SpawnOverworldMapObject; whatever that call leaves in r4 is what the
 * original's "add r4, #1" increments, so the next count is taken from the
 * call's r4 result. SpawnOverworldMapObject trashes r5, which is why the
 * original wraps every call in push/pop {r5}, and the whole routine saves
 * r4 (and its return address) with push {r4,lr}. Both saves go through the
 * guest stack so the caller's r4/r5 survive and the memory writes match. */
static u32 spawn_seven_overworld_map_objects(void) {
    /* register form: guest r4/r5 must keep their caller's values across
     * call_SpawnOverworldMapObject_spawnSevenOverworldMapObjects, whose callee leaves its own r4/r5 behind;
     * the original keeps them on the guest stack (push {r4,lr}, push {r5}),
     * so the saves and restores have to touch the guest registers/SP directly. */
    SP -= 8;
    ST32(SP, R4);
    ST32(SP + 4, LR);

    u32 lastSpawnR2 = 0;
    u32 count = 0;

    do {
        SP -= 4;
        ST32(SP, R5);

        call_SpawnOverworldMapObject_result_spawnSevenOverworldMapObjects spawn =
            call_SpawnOverworldMapObject_spawnSevenOverworldMapObjects(0x1C, 0, 0, 0, count);

        R5 = LD32(SP);              /* pop {r5} */
        SP += 4;

        lastSpawnR2 = spawn.r2;
        count = spawn.r4 + 1;       /* add r4, #1 on the r4 the callee left */
    } while ((s32)count < 7);       /* cmp r4, #7; blt */

    R4 = LD32(SP);                  /* pop {r4} (the return address is not
                                     * needed: lr is a clobber and the
                                     * framework returns to the caller
                                     * itself) */
    SP += 8;

    return lastSpawnR2;
}

/* spawnSevenOverworldMapObjects: inputs lr, outputs r2; r4/r5/r6 preserved. */
Next spawnSevenOverworldMapObjects(void) {
    R2 = spawn_seven_overworld_map_objects();
    RETURN_TO_CALLER();
}

/* ── checkObjectField20Zero_808FF8C ── */
/* checkObjectField20Zero_808FF8C: test whether the value at [r5+0x20] (the caller's object pointer, field 0x20)
 * is zero. Returns nonzero (Z clear) when the field is 0, zero (Z set) otherwise. */
static bool field_is_zero(u32 object_ptr) {
    return LD32(object_ptr + 0x20) == 0;
}

/* checkObjectField20Zero_808FF8C(r5 = object pointer) -> nz */
Next checkObjectField20Zero_808FF8C(void) {
    bool empty = field_is_zero(R5);
    SET_FLAGS_NZ(empty);
    RETURN_TO_CALLER();
}

/* ── getAreaTableEntry_808FFC0 ── */
/* Map-entry lookup: pick one of four per-area tables based on the current map
 * group (map group 0x85 always uses the last slot), index it by the byte at
 * [r5,#0xE] (entries are 8 bytes), and return the entry's two halfwords
 * (already shifted so the caller can sign-extend with ASR #16) and two bytes.
 * Inputs:  r5 = object whose +0xE field selects the entry, r10 = toolkit
 * Outputs: r0 = entry+0 (halfword<<16), r1 = entry+2 (halfword<<16),
 *          r2 = entry+4 (byte), r3 = entry+6 (byte) */

typedef struct {
    u32 half0;   /* entry+0, halfword shifted left 16 */
    u32 half1;   /* entry+2, halfword shifted left 16 */
    u32 byte0;   /* entry+4 */
    u32 byte1;   /* entry+6 */
} MapEntryValues;

static MapEntryValues read_map_entry(u32 objPtr, u32 toolkit)
{
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 index;

    if (LD8(gameState + oGameState_MapGroup) == 0x85)
        index = 3;                          /* fixed table slot for this map group */
    else
        index = LD8(gameState + 5);         /* otherwise the game state byte selects the table */

    u32 entry = LD32(A_off_808FFF4 + index * 4);
    entry += LD8(objPtr + 0xE) * 8;         /* entries are 8 bytes apart */

    MapEntryValues v;
    v.half0 = LD16(entry) << 16;
    v.half1 = LD16(entry + 2) << 16;
    v.byte0 = LD8(entry + 4);
    v.byte1 = LD8(entry + 6);
    return v;
}

Next getAreaTableEntry_808FFC0(void) {
    MapEntryValues v = read_map_entry(R5, R10);
    R0 = v.half0;
    R1 = v.half1;
    R2 = v.byte0;
    R3 = v.byte1;
    RETURN_TO_CALLER();
}

/* ── bbs_requestOnEventBE4_809003C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; bool z, n, c, v; } call_TestEventFlagFromImmediate_result_sub_809003C;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_sub_809003C call_TestEventFlagFromImmediate_sub_809003C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_sub_809003C){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: gameState_setUnk09_8001172 has no typed C interface yet */
static inline void call_gameState_setUnk09_8001172(u32 r0) {
    R0 = r0;
    GUEST_CALL(gameState_setUnk09_8001172);
}

typedef struct { u32 r3; } call_reqBBS_setFlag_e17b0f7_8140A00_result;
/* register form: generated wrapper: reqBBS_setFlag_e17b0f7_8140A00 has no typed C interface yet */
static inline call_reqBBS_setFlag_e17b0f7_8140A00_result call_reqBBS_setFlag_e17b0f7_8140A00(void) {
    GUEST_CALL(reqBBS_setFlag_e17b0f7_8140A00);
    return (call_reqBBS_setFlag_e17b0f7_8140A00_result){ R3 };
}

/* The "play sound effect" helper at loc_809E306 lives inside another function,
 * so the tool provides neither a typed wrapper nor an A_ label macro for it.
 * Call it through its code address instead (THUMB destination, bit 0 set). */
#define LOC_809E306_ADDR 0x0809E307u

/* bl loc_809E306 with the sound id in r0. */
static void play_sound_loc_809E306(u32 sound_id) {
    R0 = sound_id;
    /* register form: bl through a code address (local label, no wrapper) */
    GUEST_CALL_ADDR(LOC_809E306_ADDR);
}

/* BBS request hook: once the BE4 story event flag is set, play the mailbox
 * sound, put the game state into substate 3 and raise the BBS request flag.
 * r3 ends up holding reqBBS_setFlag's result via its wrapper (or stays as on
 * entry when the event flag is still clear); r4 is never touched. */
static void bbs_request_scene(void) {
    /* EVENT_BE4 = 0xBE4: bank 0x0B, flag 0xE4. TestEventFlagFromImmediate
     * leaves Z set when the flag is clear. */
    if (call_TestEventFlagFromImmediate_sub_809003C(0xB, 0xE4).z)
        return;

    play_sound_loc_809E306(4);                 /* play sound effect 4 */
    call_gameState_setUnk09_8001172(3);        /* game state substate = 3 */
    call_reqBBS_setFlag_e17b0f7_8140A00();     /* mark BBS request e17b0f7 */
}

/* bbs_requestOnEventBE4_809003C: inputs r10 lr; outputs r3 (from the callee) and r4 (unchanged).
 * Flags are clobbered only, so nothing has to be set explicitly here. */
Next bbs_requestOnEventBE4_809003C(void) {
    bbs_request_scene();
    RETURN_TO_CALLER();
}

/* ── checkCurrentMapEventFlag_80902DC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; bool z, n; } call_TestEventFlag_result_sub_80902DC;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_sub_80902DC call_TestEventFlag_sub_80902DC(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_sub_80902DC){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Flag-ID lookup table for the current map: one u16 flag id per map number,
 * indexed after doubling (each entry is a halfword). */
#define MAP_FLAG_TABLE A_byte_8090300
/* Map group 0x85 ignores the map number and always uses table entry 3. */
#define SPECIAL_MAP_GROUP 0x85
#define SPECIAL_MAP_ENTRY 3

typedef struct {
    u32 r4;        /* passthrough: the original pops r4-r7 untouched */
    u32 r5;
    bool flag_set; /* TestEventFlag result: Z clear when the flag is set */
} MapFlagResult;

/* Test the event flag selected for the map the player is currently on.
 * Returns the r4/r5 the original leaves unchanged plus the flag test result. */
static MapFlagResult check_current_map_event_flag(u32 toolkit, u32 r4, u32 r5) {
    MapFlagResult out;
    out.r4 = r4;
    out.r5 = r5;

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 group = LD8(game_state + oGameState_MapGroup);
    u32 entry = (group == SPECIAL_MAP_GROUP)
        ? SPECIAL_MAP_ENTRY
        : LD8(game_state + oGameState_MapNumber);

    u16 flag_id = LD16(MAP_FLAG_TABLE + entry * 2);
    out.flag_set = !call_TestEventFlag_sub_80902DC(flag_id).z; /* Z clear when the flag is set */
    return out;
}

/* checkCurrentMapEventFlag_80902DC() -> nz r1 r3 (from TestEventFlag), r4/r5 pass through unchanged */
Next checkCurrentMapEventFlag_80902DC(void) {
    MapFlagResult res = check_current_map_event_flag(R10, R4, R5);
    R4 = res.r4;
    R5 = res.r5;
    /* TestEventFlag tested a single low bit of the flag byte, so its N flag is
     * clear and Z reflects the bit; expressing the result as 0/1 reproduces both. */
    SET_FLAGS_NZ(res.flag_set);
    RETURN_TO_CALLER();
}
