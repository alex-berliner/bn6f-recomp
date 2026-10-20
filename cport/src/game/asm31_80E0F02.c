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

/* ── battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2; } call_battle_networkInvert_result;
/* register form: generated wrapper: battle_networkInvert has no typed C interface yet */
static inline call_battle_networkInvert_result call_battle_networkInvert(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_networkInvert);
    return (call_battle_networkInvert_result){ R0, R1, R2 };
}

typedef struct { u32 r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E call_PlaySoundEffect_battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E){ R4, R5, R6, R7, R10 };
}

/* battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E
 *
 * Plays the buster charge sound for a BattleObject that is charging its shot.
 *
 * r5 points to the BattleObject. Its related object's AI data names which
 * button is held: button 2 skips the alliance check entirely; any other value
 * runs the check through battle_networkInvert and stays silent for the
 * opposing side. Then, depending on the charge phase (ExtraVars+0x14) and the
 * charge tick counter (ExtraVars+0x18), one of two sounds is played:
 *   phase 1, counter 0 -> SOUND_BUSTER_CHARGE
 *   phase 2, counter 1 -> SOUND_UNK_72
 * Otherwise nothing is played. r4/r5/r10 and the flags end as the caller
 * expects (flags are listed as clobbers, so no particular values).
 */
static void play_charge_sounds(u32 object)
{
    u32 related = LD32(object + oBattleObject_RelatedObject1Ptr);
    u32 aiData = LD32(related + oBattleObject_AIDataPtr);
    u32 pwrAtkButton = LD8(aiData + oAIData_PwrAtkButton);

    /* r2 at the PlaySoundEffect call: the AI data pointer, or whatever
     * battle_networkInvert left there when the alliance check ran. */
    u32 soundArg2 = aiData;

    if (pwrAtkButton != 2) {
        call_battle_networkInvert_result invert =
            call_battle_networkInvert(LD8(related + oBattleObject_Alliance));
        if (invert.r0 != 0)
            return;                     /* wrong alliance: stay silent */
        soundArg2 = invert.r2;
    }

    u32 chargePhase = LD32(object + oBattleObject_ExtraVars + 0x14);
    if (chargePhase == 1) {
        if (LD32(object + oBattleObject_ExtraVars + 0x18) == 0)
            call_PlaySoundEffect_battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E(SOUND_BUSTER_CHARGE, soundArg2);
    } else if (chargePhase == 2) {
        if (LD32(object + oBattleObject_ExtraVars + 0x18) == 1)
            call_PlaySoundEffect_battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E(SOUND_UNK_72, soundArg2);
    }
}

/* inputs: r5 = BattleObject*, r10, lr
 * outputs: r4 r5 r10 (all left unchanged; the flags are clobbered freely) */
Next battleObject_chargeShotChargeObject_playChargeSounds_80E0F5E(void)
{
    play_charge_sounds(R5);
    RETURN_TO_CALLER();
}

/* ── t4_0xa_80E10A4 ── */
/* t4_0xa_80E10A4 — flash update dispatcher for a battle object.
 *
 * r5 points to a battle object whose Param1 selects one of two flash update
 * routines (0: palette flash, 1: white flash) through a two-entry jump table.
 * This is a pure trampoline: the chosen routine runs with the live registers
 * (r5 = object, r10 = context) and its results in r5 and r8-r12 pass straight
 * through to this function's caller.
 */

/* register form: the callee is reached through a code address taken from the
 * jump table, so it must be invoked with the caller's guest registers intact —
 * r5/r10 have to reach it unchanged and its r5/r8-r12 results have to flow
 * back out untouched, which typed value parameters cannot express. */
static void dispatch_flash_update(void) {
    u32 param1 = LD8(R5 + oBattleObject_Param1);
    u32 target = LD32(A_off_80E10B8 + (param1 << 2));

    /* The original reaches the call with the scaled index in r0 and the table
     * entry in r1; reproduce that state for the dispatched routine. */
    R0 = param1 << 2;
    R1 = target;
    GUEST_CALL_ADDR(target);
}

/* t4_0xa_80E10A4(r5 = battle object, r10 = context) -> r5 r8 r9 r10 r11 r12
 * (all set by the dispatched routine; the trampoline itself only selects it) */
Next t4_0xa_80E10A4(void) {
    dispatch_flash_update();
    RETURN_TO_CALLER();
}

/* ── sub_80E1332 ── */
/* sub_80E1332: pure tail call into battle_spawnLinkedObjType2d.
 *
 * The original pushes its return address, calls battle_spawnLinkedObjType2d, then pops that
 * address straight into the PC, so every register (r1-r7, the c/v flags) and
 * the eventual return target come straight out of the callee. All arguments
 * the caller passed in r0-r4 are simply forwarded untouched. */
Next sub_80E1332(void) {
    u32 return_addr = LR;

    GUEST_CALL(battle_spawnLinkedObjType2d);

    LR = return_addr;
    RETURN_TO_CALLER();
}

/* ── t4_0xf_80E1520 ── */
/*
 * t4_0xf_80E1520 -- snake-arm battle object state dispatcher.
 *
 * Reads the battle object's current state (r5 points to the object) and jumps
 * to the matching handler through the table at off_80E1534:
 *   0: snakearm_initObject               (set-up on the first tick)
 *   1: battleObject_snakeArmTick_80E1566 (per-frame behaviour)
 *   2: object_freeMemoryWrapper_8016C9C                       (shared cleanup stub)
 * The handler is entered with the register file exactly as the original's
 * `mov lr, pc; bx r1` leaves it: r0 = state byte, r1 = its own table entry,
 * everything else untouched.
 */

/* The registers this dispatcher's callers expect back from it. */
typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
} StateHandlerRegs_t4_0xf_80E1520;

/* Register form: the handler is reached through `mov lr, pc; bx r1`, so it
 * must see the live register file rather than C parameters. Only r0 and r1
 * differ from the values this function was entered with (the original
 * overwrites both with the state byte and the table entry before the jump);
 * every other register still holds its incoming value, so it is passed
 * through by simply not touching it. */
static StateHandlerRegs_t4_0xf_80E1520 call_state_handler_t4_0xf_80E1520(u32 handler, u32 state) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);

    StateHandlerRegs_t4_0xf_80E1520 out;
    out.r5 = R5;
    out.r8 = R8;
    out.r9 = R9;
    out.r10 = R10;
    out.r11 = R11;
    out.r12 = R12;
    return out;
}

/* Look up the handler for the object's current state and run it. */
static StateHandlerRegs_t4_0xf_80E1520 run_snake_arm_state(u32 r5) {
    u32 state = LD8(r5 + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E1534 + state);
    return call_state_handler_t4_0xf_80E1520(handler, state);
}

/* t4_0xf_80E1520(r5 = battle object) -> r5 r8 r9 r10 r11 r12 from the handler */
Next t4_0xf_80E1520(void) {
    StateHandlerRegs_t4_0xf_80E1520 out = run_snake_arm_state(R5);
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── snakearm_initObject ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result call_sprite_load(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result){ R4, R5 };
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_snakearm_initObject;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_snakearm_initObject call_sprite_loadAnimationData_snakearm_initObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_snakearm_initObject){ R2, R4, R5 };
}

typedef struct { u32 r3; } call_sprite_noShadow_result_snakearm_initObject;
/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline call_sprite_noShadow_result_snakearm_initObject call_sprite_noShadow_snakearm_initObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
    return (call_sprite_noShadow_result_snakearm_initObject){ R3 };
}

typedef struct { u32 r5; } call_battleObject_snakeArmTick_80E1566_result;
/* register form: generated wrapper: battleObject_snakeArmTick_80E1566 has no typed C interface yet */
static inline call_battleObject_snakeArmTick_80E1566_result call_battleObject_snakeArmTick_80E1566(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battleObject_snakeArmTick_80E1566);
    return (call_battleObject_snakeArmTick_80E1566_result){ R5 };
}

/* Initialise a SnakeArm battle object (runs once when the object spawns):
 * load its sprite resources, mark the header as sprite-initialised, switch
 * the object to the generic UPDATE state handler, and run one tick of its
 * state machine.  Returns the (possibly updated) object pointer. */
static u32 snakearm_init_object(u32 obj, u32 tickR6, u32 tickR7)
{
    /* sprite_load(gfx 0x80, 0xC, 9, obj) fills in the object's sprite and
     * returns the object pointer (r5) alongside the loaded sprite (r4). */
    obj = call_sprite_load(0x80, 0xC, 9, obj).r5;
    obj = call_sprite_loadAnimationData_snakearm_initObject(obj).r5;
    call_sprite_noShadow_snakearm_initObject(obj);          /* snake arms cast no shadow */

    /* Set the "sprite initialised" bit in the object header flags. */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Hand the object over to the UPDATE state handler. */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    /* Run one tick of the snake arm state machine; its r5 result is ours. */
    return call_battleObject_snakeArmTick_80E1566(obj, tickR6, tickR7).r5;
}

/* snakearm_initObject(r5 = object, r6/r7 = tick state, r10 = context) -> r5 */
Next snakearm_initObject(void)
{
    R5 = snakearm_init_object(R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── ai_getTargetObjectPos ── */
/* ai_getTargetObjectPos: read the target object's x/y position bytes through
 * the battle object's AI data block. r5 = battle object -> returns r0 = byte
 * 0x12, r1 = byte 0x13 of its target object. */

/* r5 = battle object; returns target position as (x, y) */
static void get_target_object_pos(u32 battleObject, u32 *outX, u32 *outY) {
    u32 aiData = LD32(battleObject + oBattleObject_AIDataPtr);
    u32 target = LD32(aiData + oAIData_TargetObjectPtr);
    *outX = LD8(target + 0x12);
    *outY = LD8(target + 0x13);
}

/* ai_getTargetObjectPos(r5 = battle object) -> r0 = x, r1 = y */
Next ai_getTargetObjectPos(void) {
    u32 x, y;
    get_target_object_pos(R5, &x, &y);
    R0 = x;
    R1 = y;
    RETURN_TO_CALLER();
}

/* ── battle_flagAiTarget_80E1654 ── */
/* Mark the AI's current target as ... (the target object at r5's AIData gets
 * flagged: word 1 at +0x64 and byte 1 at +0x10), returning the target pointer. */
static u32 flag_ai_target(u32 battleObject) {
    u32 aiData   = LD32(battleObject + oBattleObject_AIDataPtr);
    u32 target   = LD32(aiData + oAIData_TargetObjectPtr);

    ST32(target + 0x64, 1);
    ST8(target + 0x10, 1);
    return target;
}

/* battle_flagAiTarget_80E1654(r5 = battle object) -> r1 = target object pointer */
Next battle_flagAiTarget_80E1654(void) {
    R1 = flag_ai_target(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_computeMoveTargetPanel_80E16CC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battle_aiIterateOverBattleObjectsByRow_80E1704 has no typed C interface yet */
static inline u32 call_battle_aiIterateOverBattleObjectsByRow_80E1704(u32 r4, u32 r6, u32 r7) {
    R4 = r4;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battle_aiIterateOverBattleObjectsByRow_80E1704);
    return R0;
}

/* Packed panel specifier built by the helper below and handed to the row
 * iterator in r4: low byte / high byte encode the two halves of the move
 * target depending on the direction being computed. */
typedef struct {
    u32 iterateResult; /* return value of battle_aiIterateOverBattleObjectsByRow_80E1704 */
    u32 r4Out;         /* the original pops r4 back unchanged, so callers see their own r4 */
} MoveTargetResult;

/* Work out which panel an object should move to and start the iteration over
 * the battle objects on that row.
 *   mode   (r0): requested side; 2 means "stay on the object's own panel"
 *   object (r1): battle object whose panel/side fields are read
 *   arg2   (r2): passed straight through to the iterator as its third argument
 *   r4In       : incoming r4, returned untouched (the original saves and
 *                pops it around the computation) */
static MoveTargetResult compute_move_target_panel(u32 mode, u32 object, u32 arg2, u32 r4In) {
    u32 target;

    if (mode == 2) {
        /* Stay on the object's panel: pack panel X into both bytes. */
        u32 panelX = LD8(object + oBattleObject_PanelX);
        target = panelX | (panelX << 8);
    } else {
        /* Compare the requested side against the object's alliance, XORed
         * with its direction flip (a mirrored sprite fights for the other
         * side). */
        u32 side = LD8(object + oBattleObject_Alliance)
                 ^ LD8(object + oBattleObject_DirectionFlip);

        if ((mode ^ side) == 0) {
            /* Requested side matches the object's effective side:
             * one panel ahead, row selector 6. */
            target = (LD8(object + oBattleObject_PanelX) + 1) | (6u << 8);
        } else {
            /* Opposite side: one panel behind, row selector 1. */
            target = ((LD8(object + oBattleObject_PanelX) - 1) << 8) | 1u;
        }
    }

    MoveTargetResult out;
    out.iterateResult = call_battle_aiIterateOverBattleObjectsByRow_80E1704(target, object, arg2);
    out.r4Out = r4In;
    return out;
}

/* inputs: r0 mode, r1 object, r2 arg2 -> outputs: r0 (iterator result), r4
 * (pass-through). The original pushes r4/r6/r7 at entry and pops them at the
 * end, so r6 and r7 must also reach the caller exactly as they arrived; the
 * wrapper call leaves them holding the iterator's arguments, hence the
 * explicit restore here. */
Next battleObject_computeMoveTargetPanel_80E16CC(void) {
    u32 r6In = R6, r7In = R7;
    MoveTargetResult res = compute_move_target_panel(R0, R1, R2, R4);
    R0 = res.iterateResult;
    R4 = res.r4Out;
    R6 = r6In;
    R7 = r7In;
    RETURN_TO_CALLER();
}

/* ── battle_pickClosestPanelObject_80E1730 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r6; } call_sub_80E175C_result;
/* register form: generated wrapper: sub_80E175C has no typed C interface yet */
static inline call_sub_80E175C_result call_sub_80E175C(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(sub_80E175C);
    return (call_sub_80E175C_result){ R0, R6 };
}

/*
 * Pick which of two battle objects is "closest" to a reference object by
 * panel row: if the reference's PanelY matches exactly one of the two
 * candidates, that candidate is returned directly. If it matches both or
 * neither, sub_80E175C breaks the tie (it is called with the candidates
 * swapped: candidate B first, then candidate A, then the reference).
 *
 * r0 = candidate A, r3 = candidate B, r6 = reference object.
 * Returns the chosen object in r0; r4/r6/r7 end exactly as they started.
 */
static u32 pick_closest_panel_object(u32 objA, u32 objB, u32 ref) {
    u32 refY = LD8(ref + oBattleObject_PanelY);
    u32 aY   = LD8(objA + 0x13);            /* same PanelY field as above */
    u32 bY   = LD8(objB + oBattleObject_PanelY);

    if (refY == aY) {
        if (refY == bY)                     /* row matches both: tie-break */
            return call_sub_80E175C(objB, objA, ref).r0;
        return objA;                        /* row matches only candidate A */
    }
    if (refY == bY)
        return objB;                        /* row matches only candidate B */

    return call_sub_80E175C(objB, objA, ref).r0;  /* matches neither */
}

/* battle_pickClosestPanelObject_80E1730(r0 = candidate A, r3 = candidate B,
 * r6 = reference object) -> r0 = chosen object; r4/r6/r7 preserved. */
Next battle_pickClosestPanelObject_80E1730(void) {
    R0 = pick_closest_panel_object(R0, R3, R6);
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
        if (LD8(R5 + oBattleObject_WobbleOrEffectTimer) != 0x17) {
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
                       + ((u32)LD8(obj + oBattleObject_WobbleOrEffectTimer) << 2));

    R0 = LD8(obj + oBattleObject_PanelX);
    R1 = LD8(obj + oBattleObject_PanelY);
    R2 = LD8(obj + oBattleObject_Element);
    R3 = handler;
    R4 = LD32(obj + oBattleObject_Params);
    R5 = LD32(obj + oBattleObject_RelatedObject1Ptr);
    R6 = LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler)
       + LD16(obj + oBattleObject_Z16Save);
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
    ST8(TIMEFREEZE_SNAPSHOT_BASE,       LD8(obj + oBattleObject_WobbleOrEffectTimer));
    ST8(A_byte_203C961,                 LD8(obj + oBattleObject_Element));
    ST16(A_word_203C962,                LD16(obj + oBattleObject_Z16Save));
    ST32(A_dword_203C964,               LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler));
    ST32(A_byte_203C968,                LD32(obj + oBattleObject_Params));
}

static void timefreeze_run_phase(u32 obj) {
    if (LD8(obj + oBattleObject_PhaseInitialized) == 0) {
        call_timefreeze_handler(obj);

        /* Snapshot the object if the handler left it in phase 0xDD..0x118. */
        u32 phase = LD16(obj + oBattleObject_ChipID);
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
        u32 unk19 = LD8(self + oBattleObject_WobbleOrEffectTimer);
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
        GUEST_CALL(battle_initFlyAnimObject);
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

/* ── t4_0x28_80E32B8 ── */
/* Battle object task step (task type 0x28): run the handler selected by the
 * object's CurState byte. r5 holds the battle object and r10 passes through
 * to the handler untouched. The table at off_80E32CC holds:
 *   0: battleObject_initSpriteFromRelatedOrParam2_80E32D8
 *   1: battleObject_updateMysteryData_80E336E
 *   2: battle_despawnEnemyObject
 * The handler is entered with r0 = state and r1 = its own address, and
 * whatever it leaves in r5/r8-r12 is what this routine leaves for its
 * caller (the original is a tail call: pop {pc} returns straight to the
 * caller's lr). */

/* register form: the handler address comes out of the state table at run
 * time, so the call must go through its address; the handler receives
 * r0 = state and r1 = handler address, and reads the object (r5) and the
 * pass-through value (r10) directly from the register file. */
static void run_curstate_handler_t4_0x28_80E32B8(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E32CC + state);
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

Next t4_0x28_80E32B8(void) {
    run_curstate_handler_t4_0x28_80E32B8(R5);
    RETURN_TO_CALLER();
}

/* ── battle_despawnEnemyObject ── */
#define oBattleObject_Param1_battle_despawnEnemyObject 0x4u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1; bool v; } call_unpackVerActorTyAndAIIdx_800F29C_result;
/* register form: generated wrapper: unpackVerActorTyAndAIIdx_800F29C has no typed C interface yet */
static inline call_unpackVerActorTyAndAIIdx_800F29C_result call_unpackVerActorTyAndAIIdx_800F29C(u32 r0) {
    R0 = r0;
    GUEST_CALL(unpackVerActorTyAndAIIdx_800F29C);
    return (call_unpackVerActorTyAndAIIdx_800F29C_result){ R0, R1, OVERFLOW_FLAG() };
}

typedef struct { u32 r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_sub_8011044_result;
/* register form: generated wrapper: battle_runObjectTeardownByNameID has no typed C interface yet */
static inline call_sub_8011044_result call_sub_8011044(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(battle_runObjectTeardownByNameID);
    return (call_sub_8011044_result){ R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result_battle_despawnEnemyObject;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result_battle_despawnEnemyObject call_object_freeMemory_battle_despawnEnemyObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result_battle_despawnEnemyObject){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Despawn an enemy's battle object (r5 = object).
 *
 * If the object's Param1 is 0xFF it is chained to a related object; set that
 * object's "clean up with owner" flag (bit 2) so it is removed as well.
 * Then unpack the version-specific actor type / AI index from the object's
 * NameID, run the actor-slot bookkeeping (battle_runObjectTeardownByNameID), and finally free the
 * object's memory. Register and flag state on return is exactly what
 * object_freeMemory leaves behind; battle_runObjectTeardownByNameID's r4-r12 results are likewise
 * left in place by its wrapper, so nothing needs to be copied through here. */
static void despawn_enemy_object(u32 obj) {
    if (LD8(obj + oBattleObject_Param1_battle_despawnEnemyObject) == 0xFF) {
        u32 related = LD32(obj + oBattleObject_RelatedObject2Ptr);
        if (related != 0)
            ST8(related + oObjectHeader_Flags,
                LD8(related + oObjectHeader_Flags) | 4);
    }

    call_unpackVerActorTyAndAIIdx_800F29C_result ids =
        call_unpackVerActorTyAndAIIdx_800F29C(LD16(obj + oBattleObject_NameID));
    call_sub_8011044_result slot = call_sub_8011044(ids.r0, ids.r1, obj);
    call_object_freeMemory_battle_despawnEnemyObject(slot.r5);
}

Next battle_despawnEnemyObject(void) {
    despawn_enemy_object(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectSetExtraVar3_80E341E ── */
/* Store a value into a battle object's scratch slot 3 of its extra-vars block
 * (used by the AI as a general-purpose memory cell). */
static void battle_object_set_extra_var_c(u32 object, u32 value) {
    ST32(object + (oBattleObject_ExtraVars + 0xC), value);
}

/* battle_objectSetExtraVar3_80E341E(r0 = object, r1 = value) -> void */
Next battle_objectSetExtraVar3_80E341E(void) {
    battle_object_set_extra_var_c(R0, R1);
    RETURN_TO_CALLER();
}

/* ── battle_finishObjectPhase_80E3950 ── */
/* Advance a battle object past its current phase: when the phase has finished
 * initializing, move it to action 8 (phase done) and clear the phase counter
 * so the next phase starts fresh. Objects not yet initialized are left alone. */
static void finish_battle_object_phase(u32 obj) {
    if (LD8(obj + oBattleObject_PhaseInitialized) != 1)
        return;

    ST8(obj + oBattleObject_CurAction, 8);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
}

/* battle_finishObjectPhase_80E3950(r5 = battle object) -> nothing */
Next battle_finishObjectPhase_80E3950(void) {
    finish_battle_object_phase(R5);
    RETURN_TO_CALLER();
}

/* ── t4_0x30_80E3B70 ── */
/* Dispatch to the BattleObject's state handler from the jump table at
 * 0x080E3B88, then always call object_updateSprite.
 *
 * Table layout (indexed by the CurState byte stored at offset 0/4/8):
 *   state 0 → battleObject_initFromRelatedObject_init_80E3B94
 *   state 4 → battle_objectUpdatePosAndRunAction_80E3BEA
 *   state 8 → object_freeMemory
 *
 * Inputs come via registers (R5 = BattleObject ptr, state byte at R5+8);
 * all output registers and flags are set by the callees. */

#define STATE_HANDLER_TABLE 0x080E3B88u

/* register form: calls battleObject_initFromRelatedObject_init_80E3B94 outside this file, which has no typed C interface yet */
static void state_handler_idle(u32 state) {
    R0 = state;
    GUEST_CALL(battleObject_initFromRelatedObject_init_80E3B94);
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
        GUEST_CALL(battle_timerTriggerRelatedObject);

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

/* ── t4_0x3c_80E4A1C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_battleObject_updateSpriteAnimIfChanged_801BCD0_result;
/* register form: generated wrapper: battleObject_updateSpriteAnimIfChanged_801BCD0 has no typed C interface yet */
static inline call_battleObject_updateSpriteAnimIfChanged_801BCD0_result call_battleObject_updateSpriteAnimIfChanged_801BCD0(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_updateSpriteAnimIfChanged_801BCD0);
    return (call_battleObject_updateSpriteAnimIfChanged_801BCD0_result){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_battleObject_tickSpriteAnim_801BC24_result;
/* register form: generated wrapper: battleObject_tickSpriteAnim_801BC24 has no typed C interface yet */
static inline call_battleObject_tickSpriteAnim_801BC24_result call_battleObject_tickSpriteAnim_801BC24(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_tickSpriteAnim_801BC24);
    return (call_battleObject_tickSpriteAnim_801BC24_result){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t4_0x3c_80E4A1C — per-frame tick for the arm-attack battle object (arrives in r5).
 *
 * The object's BattleObject.CurState byte indexes the handler table at
 * off_80E4A40. Note the original indexes it with the raw byte value
 * ("ldr r1, [r1, r0]" — no *4), so the dispatch must do exactly the same.
 * After the handler returns, CurState is re-read (the handler may have
 * changed it): state 8 gets the full sprite-animation update, any other
 * state just ticks the animation. r5 ends up as the last callee left it. */

/* register form: the original dispatches with "mov lr, pc / bx r1" through an
 * address read from the jump table, so no typed wrapper covers these handlers;
 * the handler may also change r5, which is handed back through out_object. */
static u32 call_curState_handler(u32 handler, u32 state, u32 object, u32 *out_object) {
    R0 = state;    /* handler is entered with r0 = the CurState byte */
    R1 = handler;  /* ... r1 = the handler's own address (as the original leaves it) */
    R5 = object;   /* ... r5 = the battle object */
    GUEST_CALL_ADDR(handler);
    *out_object = R5;   /* pick up the handler's possibly updated object pointer */
    return R0;          /* r0 is clobbered by contract; value unused */
}

/* Typed body: r5 = the battle object. r10 reaches the animation callees
 * untouched (their wrappers keep it as on entry). */
static u32 arm_attack_object_tick(u32 object) {
    /* Dispatch to the handler for the current state (raw byte offset). */
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E4A40 + state);

    u32 handled = object;
    call_curState_handler(handler, state, object, &handled);

    /* The handler may have changed the state: re-read it before branching. */
    if (LD8(handled + oBattleObject_CurState) == 8)
        return call_battleObject_updateSpriteAnimIfChanged_801BCD0(handled).r5;
    return call_battleObject_tickSpriteAnim_801BC24(handled).r5;
}

Next t4_0x3c_80E4A1C(void) {
    R5 = arm_attack_object_tick(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectTickAction3_80E4A4C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3; bool c, v; } call_sprite_getFrameParameters_result_sub_80E4A4C;
/* register form: generated wrapper: sprite_getFrameParameters has no typed C interface yet */
static inline call_sprite_getFrameParameters_result_sub_80E4A4C call_sprite_getFrameParameters_sub_80E4A4C(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_getFrameParameters);
    return (call_sprite_getFrameParameters_result_sub_80E4A4C){ R0, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_sub_8016C9C_result;
/* register form: generated wrapper: object_freeMemoryWrapper_8016C9C has no typed C interface yet */
static inline call_sub_8016C9C_result call_sub_8016C9C(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemoryWrapper_8016C9C);
    return (call_sub_8016C9C_result){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Tick a battle object's current action.
 * If no action is queued (CurAction == 0), start action 3 along with its animation.
 * Otherwise ask the sprite for its frame parameters and, when the 0x80 flag is set
 * (animation finished), run the action-completion routine object_freeMemoryWrapper_8016C9C.
 * The object pointer in r5 is passed through unchanged. */
static void battle_object_tick_action(u32 obj)
{
    if (LD8(obj + oBattleObject_CurAction) == 0) {
        /* Nothing queued: begin action 3 and its matching animation. */
        ST8(obj + oBattleObject_CurAction, 3);
        ST8(obj + oBattleObject_CurAnim, 3);
        return;
    }

    u32 frameParams = call_sprite_getFrameParameters_sub_80E4A4C(obj).r0;
    if (frameParams & 0x80)
        call_sub_8016C9C(obj);
}

/* battle_objectTickAction3_80E4A4C(r5 = battle object) -> r5 unchanged */
Next battle_objectTickAction3_80E4A4C(void) {
    battle_object_tick_action(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnArmAttackObject ── */
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized_battle_spawnArmAttackObject 0x8u
#define CUR_STATE_UPDATE_battle_spawnArmAttackObject 0x4u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_battle_spawnArmAttackObject;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battle_spawnArmAttackObject call_sprite_load_battle_spawnArmAttackObject(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battle_spawnArmAttackObject){ R4, R5 };
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result_battle_spawnArmAttackObject;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battle_spawnArmAttackObject call_sprite_loadAnimationData_battle_spawnArmAttackObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battle_spawnArmAttackObject){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battle_spawnArmAttackObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battle_spawnArmAttackObject(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r3, r4, r5; bool c, v; } call_sprite_update_result;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result call_sprite_update(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result){ R2, R3, R4, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_battle_spawnArmAttackObject;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battle_spawnArmAttackObject call_object_getFlip_battle_spawnArmAttackObject(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battle_spawnArmAttackObject){ R0 };
}

/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline void call_sprite_setFlip_battle_spawnArmAttackObject(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
}

typedef struct { u32 r5; } call_battleObject_updateAttachedArmCoords_80E4AAE_result;
/* register form: generated wrapper: battleObject_updateAttachedArmCoords_80E4AAE has no typed C interface yet */
static inline call_battleObject_updateAttachedArmCoords_80E4AAE_result call_battleObject_updateAttachedArmCoords_80E4AAE(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_updateAttachedArmCoords_80E4AAE);
    return (call_battleObject_updateAttachedArmCoords_80E4AAE_result){ R5 };
}

/* Spawn the "arm attack" effect object: load its sprite, flag the object,
 * start its spawn animation and hand control to the per-frame update state. */

/* battle_spawnArmAttackObject body: obj lives in r5 throughout and is the
 * only value callers see afterwards (r4 is popped, so its callee value dies). */
static u32 spawn_arm_attack_object(u32 obj) {
    /* Load the arm-attack sprite resources (0x80/0xC/0x20 as in the original). */
    obj = call_sprite_load_battle_spawnArmAttackObject(0x80, 0xC, 0x20, obj).r5;
    obj = call_sprite_loadAnimationData_battle_spawnArmAttackObject(obj).r5;
    call_sprite_noShadow_battle_spawnArmAttackObject(obj);

    /* Mark the object with the arm-attack flag bit. */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Start animation #2 (the spawn animation) and refresh its data. */
    ST8(obj + oBattleObject_CurAnim, 2);
    ST8(obj + oBattleObject_CurAnimCopy, 2);
    call_sprite_setAnimation_battle_spawnArmAttackObject(2, obj);

    obj = call_sprite_loadAnimationData_battle_spawnArmAttackObject(obj).r5;
    obj = call_sprite_update(obj).r5;

    /* Mirror the object's facing onto the sprite. */
    call_sprite_setFlip_battle_spawnArmAttackObject(call_object_getFlip_battle_spawnArmAttackObject(obj).r0, obj);

    /* Hand the object over to the update state. */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized_battle_spawnArmAttackObject, CUR_STATE_UPDATE_battle_spawnArmAttackObject);
    return call_battleObject_updateAttachedArmCoords_80E4AAE(obj).r5;
}

/* battle_spawnArmAttackObject(r5 = object, r10 = battle context) -> r5 = object */
Next battle_spawnArmAttackObject(void) {
    R5 = spawn_arm_attack_object(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnLinkedObj3C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3, r4, r5, r6, r7; } call_object_spawnType4_result;
/* register form: generated wrapper: object_spawnType4 has no typed C interface yet */
static inline call_object_spawnType4_result call_object_spawnType4(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType4);
    return (call_object_spawnType4_result){ R3, R4, R5, R6, R7 };
}

/* battle_spawnLinkedObj3C: spawn battle object type 0x3C and link it to a parent object.
 *
 * Inputs (r1-r4): spawn arguments passed straight through to object_spawnType4.
 * r5: parent battle object — the new object inherits its alliance/direction-flip
 *     halfword and stores a back-pointer to it.
 * r7: extra-vars block of the owner; its first word receives the new object.
 *
 * If the spawn fails (object_spawnType4 returns NULL in r5) nothing is linked.
 * r3 and r6 are whatever object_spawnType4 left behind; r5 passes through unchanged
 * (the original saves and restores it around the spawn call). */
typedef struct {
    u32 r3;      /* untouched leftover from object_spawnType4 */
    u32 parent;  /* r5: the parent object, unchanged */
    u32 r6;      /* untouched leftover from object_spawnType4 */
} linked_obj3c_result_t;

static linked_obj3c_result_t spawn_linked_object_3C(u32 arg1, u32 arg2, u32 arg3, u32 arg4,
                                                    u32 parent, u32 extraVars) {
    call_object_spawnType4_result spawn = call_object_spawnType4(0x3C, arg1, arg2, arg3, arg4);
    u32 newObject = spawn.r5;

    if (newObject != 0) {
        /* Copy the parent's alliance/direction-flip and record the mutual link. */
        ST16(newObject + oBattleObject_AllianceAndDirectionFlip,
             LD16(parent + oBattleObject_AllianceAndDirectionFlip));
        ST32(newObject + oBattleObject_RelatedObject1Ptr, parent);
        ST32(newObject + oBattleObject_ExtraVars, extraVars);
        ST32(extraVars, newObject);

        /* Mark the new object with flag bits 0x14. */
        ST8(newObject + oObjectHeader_Flags,
            LD8(newObject + oObjectHeader_Flags) | 0x14u);
    }

    return (linked_obj3c_result_t){ spawn.r3, parent, spawn.r6 };
}

/* r1-r4: spawn args, r5: parent object, r7: extra-vars block -> r3 r5 r6 */
Next battle_spawnLinkedObj3C(void) {
    linked_obj3c_result_t res = spawn_linked_object_3C(R1, R2, R3, R4, R5, R7);
    R3 = res.r3;
    R5 = res.parent;
    R6 = res.r6;
    RETURN_TO_CALLER();
}

/* ── object_scriptedEffectTick ── */
#define oBattleObject_Param1_object_scriptedEffectTick 0x4u
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized_object_scriptedEffectTick 0x8u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r3; } call_battle_isBattleOver_result_object_scriptedEffectTick;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result_object_scriptedEffectTick call_battle_isBattleOver_object_scriptedEffectTick(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result_object_scriptedEffectTick){ R0, R1, R3 };
}

typedef struct { u32 r1, r2, r3, r5, r7; } call_sub_80CE6E6_result;
/* register form: generated wrapper: sub_80CE6E6 has no typed C interface yet */
static inline call_sub_80CE6E6_result call_sub_80CE6E6(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(sub_80CE6E6);
    return (call_sub_80CE6E6_result){ R1, R2, R3, R5, R7 };
}

/* Per-frame tick for a scripted-effect battle object.
 *
 * - If the battle is already over, the object is destroyed immediately.
 * - On the first tick of its action (CurAction == 0) the action is marked
 *   started, one byte is consumed from the object's extra-data stream, and
 *   sub_80CE6E6 spawns the scripted effect from its fields (low 3 bits,
 *   high nibble, element, param1); the action Timer is then seeded with 8.
 * - Every tick the Timer counts down; when it expires, Param2 counts the
 *   remaining repetitions: while any remain, CurAction is cleared so the
 *   effect runs again, otherwise the object is destroyed. */
static void scripted_effect_tick(u32 obj) {
    /* register form: sub_80CE6E6 clobbers r4/r6, but this routine must
       preserve both - the original push {r4,r6} / pop around its call */
    u32 saved_r4 = R4; /* register form: save entry r4 across the callee call */
    u32 saved_r6 = R6; /* register form: save entry r6 across the callee call */

    bool destroyed = false;

    /* battle_isBattleOver reads r10 (battle state): non-zero means over */
    if (call_battle_isBattleOver_object_scriptedEffectTick().r0 != 0) {
        destroyed = true;
    } else {
        /* First frame of the scripted action: kick off the sub-effect. */
        if (LD8(obj + oBattleObject_CurAction) == 0) {
            ST8(obj + oBattleObject_CurAction, 1);

            /* Consume one extra-data byte and split it into fields. */
            u32 extra = LD32(obj + oBattleObject_ExtraVars);
            ST32(obj + oBattleObject_ExtraVars, extra + 1);
            u8 script = LD8(extra);
            u32 low3  = script & 0x07;  /* lsl #29 / lsr #29 */
            u32 high4 = script >> 4;    /* lsr #4 */

            u32 element  = LD8(obj + oBattleObject_Element);
            u32 param1   = LD8(obj + oBattleObject_Param1_object_scriptedEffectTick);
            u32 disabler = LD32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler);

            call_sub_80CE6E6(low3, high4, element, 0, param1, obj, disabler);
            R4 = saved_r4; /* register form: restore r4 (original pop) */
            R6 = saved_r6; /* register form: restore r6 (original pop) */

            ST16(obj + oBattleObject_Timer, 8);
        }

        /* Countdown the action timer, then the repetition counter. */
        s32 timer = (s32)LD16(obj + oBattleObject_Timer) - 1;
        ST16(obj + oBattleObject_Timer, (u32)timer);
        if (timer <= 0) {                       /* bgt stays in the loop only while positive */
            u32 repeats = LD8(obj + oBattleObject_Param2) - 1;
            ST8(obj + oBattleObject_Param2, repeats);
            if (repeats != 0)
                ST8(obj + oBattleObject_CurAction, 0);  /* restart the action */
            else
                destroyed = true;                        /* no repetitions left */
        }
    }

    if (destroyed)
        ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized_object_scriptedEffectTick,
             CUR_STATE_DESTROY);
}

Next object_scriptedEffectTick(void) {
    scripted_effect_tick(R5);
    RETURN_TO_CALLER();
}

/* ── t4_0x41_80E5244 ── */
/* t4_0x41_80E5244: dispatch a battle object's current-state handler.
 * r5 points to the battle object; its CurState byte selects an entry from
 * a jump table at off_80E5258 (the original indexes with the raw byte).
 * The handler is invoked with
 *   r0  = CurState
 *   r1  = handler address (the original's bx r1 leaves the table entry in r1)
 *   r2  = argument passed through from this function's caller
 *   r10 = argument passed through from this function's caller
 * The call is linked (mov lr, pc), so the handler returns into this
 * function, which then returns to its own caller. The handler's register
 * results (r5, r8-r12) are this function's outputs and are simply left
 * in the guest register file by the call. */

/* register form: the handler is chosen at runtime through a jump table, so
 * no typed wrapper exists for it. The guest call must run with the handler's
 * arguments (including r5 and r10) live in the guest registers, and the
 * handler's own register results (r5, r8-r12) must pass through untouched
 * to this function's caller. */
static void run_state_handler_t4_0x41_80E5244(u32 obj, u32 arg2, u32 arg10) {
    u32 state   = LD8(obj + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E5258 + state);

    R0 = state;     /* handler's first argument: the current state */
    R1 = handler;   /* r1 still holds the table entry at the bx */
    R2 = arg2;
    R5 = obj;
    R10 = arg10;
    GUEST_CALL_ADDR(handler);
}

Next t4_0x41_80E5244(void) {
    run_state_handler_t4_0x41_80E5244(R5, R2, R10);
    RETURN_TO_CALLER();
}

/* ── battle_finishActionToStateUpdate_80E5264 ── */
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E5264 0x8u
#define CUR_STATE_UPDATE_sub_80E5264 0x4u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result call_PlaySoundEffect(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result){ R1, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_sub_80E527A_result;
/* register form: generated wrapper: battle_condDispatchRelatedPhase_80E527A has no typed C interface yet */
static inline call_sub_80E527A_result call_sub_80E527A(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_condDispatchRelatedPhase_80E527A);
    return (call_sub_80E527A_result){ R5, R8, R9, R10, R11, R12 };
}

/* Shared tail of the state handler: the object has finished its current action, so
 * store the extra var (3), play sound 0xAD, switch to the "update" state phase and
 * continue into the state dispatcher, whose results are this function's outputs.
 *
 * inputs: r2 (passed through to PlaySoundEffect), r5 (battle object pointer), r10
 * outputs: r5 r8 r9 r10 r11 r12 (all from battle_condDispatchRelatedPhase_80E527A)
 */
static call_sub_80E527A_result finish_state_phase(u32 obj, u32 arg2)
{
    ST32(obj + oBattleObject_ExtraVars + 0xC, 3);
    call_PlaySoundEffect(0xAD, arg2);
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E5264, CUR_STATE_UPDATE_sub_80E5264);
    return call_sub_80E527A(obj);
}

Next battle_finishActionToStateUpdate_80E5264(void) {
    call_sub_80E527A_result res = finish_state_phase(R5, R2);
    R5 = res.r5;
    R8 = res.r8;
    R9 = res.r9;
    R10 = res.r10;
    R11 = res.r11;
    R12 = res.r12;
    RETURN_TO_CALLER();
}

/* ── battle_condDispatchRelatedPhase_80E527A ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_object_dispatchPhaseHandler_result;
/* register form: generated wrapper: object_dispatchPhaseHandler has no typed C interface yet */
static inline call_object_dispatchPhaseHandler_result call_object_dispatchPhaseHandler(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_dispatchPhaseHandler);
    return (call_object_dispatchPhaseHandler_result){ R5, R8, R9, R10, R11, R12 };
}

/* Result registers of object_dispatchPhaseHandler (r5 r8 r9 r10 r11 r12), which
 * this thunk passes through to its own callers. */
typedef call_object_dispatchPhaseHandler_result PhaseHandlerRegs;

/* Tail-shared thunk before object_dispatchPhaseHandler: normally the related
 * object's phase handler is dispatched, but if the related object is currently
 * performing action 4, or its collision flags report the 0x80010000 condition
 * (dword_80E546C), the call is skipped and every output register is left as it
 * came in.
 * Note: the generated wrapper takes r5 as a parameter but passes r10 through
 * from the live register, so the adapter must not have touched R10 yet when
 * this runs — it still holds the r10 the thunk was entered with. */
static PhaseHandlerRegs sub_80E527A_body(u32 object, u32 r8, u32 r9, u32 r10,
                                         u32 r11, u32 r12)
{
    PhaseHandlerRegs unchanged;
    unchanged.r5 = object;
    unchanged.r8 = r8;
    unchanged.r9 = r9;
    unchanged.r10 = r10;
    unchanged.r11 = r11;
    unchanged.r12 = r12;

    u32 related = LD32(object + oBattleObject_RelatedObject1Ptr);
    if (LD8(related + oBattleObject_CurAction) == 4)
        return unchanged;

    u32 collision = LD32(related + oBattleObject_CollisionDataPtr);
    u32 flags1 = LD32(collision + oCollisionData_ObjectFlags1);
    if (flags1 & 0x80010000)
        return unchanged;

    return call_object_dispatchPhaseHandler(object);
}

/* battle_condDispatchRelatedPhase_80E527A(r5 = object, r10 = handler context) -> r5 r8 r9 r10 r11 r12 */
Next battle_condDispatchRelatedPhase_80E527A(void) {
    PhaseHandlerRegs out = sub_80E527A_body(R5, R8, R9, R10, R11, R12);
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── object_dispatchPhaseHandler ── */
/* object_dispatchPhaseHandler — hand a battle object to the handler for the
 * phase it is currently in.
 *
 * The original reads the object's CurPhase byte, uses it to index the
 * two-entry handler table at off_80E52A8, and calls the handler found there
 * with r0 = phase byte and r1 = handler address (mov lr, pc; bx r1). The
 * handler works on the object pointer in r5 and leaves the dispatcher's
 * results in r5 and r8-r12; the dispatcher itself only tails into it, so
 * those registers pass straight through to the dispatcher's caller.
 */

/* register form: the phase handler has no fixed entry point — its address
 * comes out of the jump table, so no generated wrapper covers it. The
 * original enters it with r0 = phase byte and r1 = handler address; the
 * remaining registers (r5 object, r10 context, and the scratch registers the
 * dispatcher is allowed to clobber) must reach it exactly as the caller of
 * the dispatcher left them, so they are not touched here. */
static void call_phase_handler(u32 phase, u32 handler) {
    R0 = phase;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

/* Read the object's current phase, look up its handler in the jump table and
 * run it. Everything the handler leaves in r5/r8-r12 stays in place for the
 * dispatcher's caller. */
static void dispatch_phase_handler(u32 object) {
    u32 phase = LD8(object + oBattleObject_CurPhase);
    u32 handler = LD32(A_off_80E52A8 + phase);
    call_phase_handler(phase, handler);
}

/* object_dispatchPhaseHandler(r5 = battle object, r10 = context)
 * -> r5 r8 r9 r10 r11 r12 as left by the phase handler (results the handler
 * leaves behind, not values this code computes). */
Next object_dispatchPhaseHandler(void) {
    dispatch_phase_handler(R5);
    RETURN_TO_CALLER();
}

/* ── battle_scanPanelRunEnd ── */
#define oBattleObject_Param1_battle_scanPanelRunEnd 0x4u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r3, r5, r7; } call_object_findMatchingPanelRunEnd_result;
/* register form: generated wrapper: object_findMatchingPanelRunEnd has no typed C interface yet */
static inline call_object_findMatchingPanelRunEnd_result call_object_findMatchingPanelRunEnd(u32 r1, u32 r5) {
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_findMatchingPanelRunEnd);
    return (call_object_findMatchingPanelRunEnd_result){ R0, R1, R3, R5, R7 };
}

typedef struct { u32 r1, r2, r3, r5; bool c, v; } call_battle_spawnObject49_result;
/* register form: generated wrapper: battle_spawnObject49 has no typed C interface yet */
static inline call_battle_spawnObject49_result call_battle_spawnObject49(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battle_spawnObject49);
    return (call_battle_spawnObject49_result){ R1, R2, R3, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_scanPanelRunEnd (0x080E52B0): periodic check, run from the scan
 * phase driver, for whether this battle object's panel run has reached its
 * end tile.
 *
 * Gates (any of these ends the scan with no side effects):
 *   - Param3 set and the alliance's battle-state slot already occupied;
 *   - both scratch bytes at obj+0x60/0x61 non-zero (run already marked).
 *
 * Otherwise the run counter at ExtraVars+0xC steps (0 wraps back to 3),
 * object_findMatchingPanelRunEnd looks for the run's end tile, and if it is
 * found object 49 is spawned and the object moves to phase 4 with a
 * 12-frame timer.
 *
 * The original pushes {r4,r6,r7} around the two calls and pops them on the
 * way out, so those registers are preserved even though both callees
 * clobber them; the saved entry values are threaded through and written
 * back by the adapter. r5 is likewise left exactly as the original leaves
 * it: untouched on early paths, and whatever battle_spawnObject49 left on
 * the success path, so the adapter does not write it.
 */
typedef struct {
    u32 r4;
    u32 r6;
    u32 r7;
} scan_panel_run_end_saved;

static scan_panel_run_end_saved scan_panel_run_end_body(u32 obj, u32 toolkit,
                                                        u32 r4_in, u32 r6_in,
                                                        u32 r7_in) {
    scan_panel_run_end_saved saved = { r4_in, r6_in, r7_in };

    /* Gate 1: with Param3 set, the alliance's battle-state slot must be free. */
    if (LD8(obj + oBattleObject_Param3) != 0) {
        u32 battleState = LD32(toolkit + oToolkit_BattleStatePtr);
        u32 slotOffset = (LD8(obj + oBattleObject_Alliance) << 2) + 0xC0;
        if (LD32(battleState + slotOffset) != 0)
            return saved;
    }

    /* Gate 2: the scratch byte pair at obj+0x60 must not both be non-zero.
     * r7 ends up pointing at the second of the two bytes either way. */
    u32 scratch = obj + 0x60;
    if (LD8(scratch) != 0) {
        scratch++;
        if (LD8(scratch) != 0)
            return saved;
    }

    /* Step the run counter, wrapping 0 back to 3. */
    u32 counterAddr = obj + oBattleObject_ExtraVars + 0xC;
    u32 count = LD32(counterAddr) - 1;
    ST32(counterAddr, count);
    if (count == 0) {
        count = 3;
        ST32(counterAddr, count);
    }

    /* register form: the original enters object_findMatchingPanelRunEnd with
     * r7 = scratch (the pointer the search works from); the wrapper's
     * parameters (r1, r5) cannot carry it. */
    R7 = scratch;

    call_object_findMatchingPanelRunEnd_result find =
        call_object_findMatchingPanelRunEnd(count, obj);
    if (find.r0 == 0)
        return saved;

    /* Run end found: spawn the follow-up object from the found tile data and
     * this object's alliance/Param1, then advance the state machine. */
    call_battle_spawnObject49(find.r0, find.r1,
                              LD16(obj + oBattleObject_AllianceAndDirectionFlip),
                              find.r3, LD8(obj + oBattleObject_Param1_battle_scanPanelRunEnd),
                              0, find.r7);

    ST16(obj + oBattleObject_Timer2, 0xC);
    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 4);
    return saved;
}

Next battle_scanPanelRunEnd(void) {
    scan_panel_run_end_saved saved =
        scan_panel_run_end_body(R5, R10, R4, R6, R7);
    R4 = saved.r4;
    R6 = saved.r6;
    R7 = saved.r7;
    RETURN_TO_CALLER();
}

/* ── battle_phaseWaitTimer2 ── */
/* Tick down a battle object's phase-wait timer; when it reaches zero, reset the
 * object's current phase (clearing the "phase initialized" bit with it). */
static void battle_phaseWaitTimer2_impl(u32 obj)
{
    u32 timer = LD16(obj + oBattleObject_Timer2) - 1;

    ST16(obj + oBattleObject_Timer2, timer);
    if (timer != 0)
        return;

    ST16(obj + oBattleObject_CurPhaseAndPhaseInitialized, 0);
}

/* battle_phaseWaitTimer2(r5 = battle object) -> nothing */
Next battle_phaseWaitTimer2(void) {
    battle_phaseWaitTimer2_impl(R5);
    RETURN_TO_CALLER();
}

/* ── battle_destroyObject_80E5314 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battle_clearDropEntryById has no typed C interface yet */
static inline void call_battle_clearDropEntryById(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_clearDropEntryById);
}

typedef struct { u32 r0, r1, r2, r3, r5; bool z, n, c, v; } call_object_freeMemory_result;
/* register form: generated wrapper: object_freeMemory has no typed C interface yet */
static inline call_object_freeMemory_result call_object_freeMemory(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_freeMemory);
    return (call_object_freeMemory_result){ R0, R1, R2, R3, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Tear down a battle object: optionally clear its drop entry, zero its
 * extra-variable block, then return its memory to the object pool.
 * r5 = object; r5 ends unchanged (object_freeMemory preserves it). */
static void destroy_battle_object_sub_80E5314(u32 object) {
    /* Objects whose Param3 is still zero were never assigned a drop,
     * so their drop entry (if any) must be released first. */
    if (LD8(object + oBattleObject_Param3) == 0)
        call_battle_clearDropEntryById(object);

    ST32(object + oBattleObject_ExtraVars, 0);
    call_object_freeMemory(object);
}

/* battle_destroyObject_80E5314(r5 = object, r10 = context) -> r5 preserved */
Next battle_destroyObject_80E5314(void) {
    destroy_battle_object_sub_80E5314(R5);
    RETURN_TO_CALLER();
}

/* ── entity_resetTimerAndCounter_80E5410 ── */
/* Initialize an entity's timer/state fields: set the 8-unit field at +8
 * and clear the counter at +0x60. Returns 0 in r1 (the last value written). */
static u32 reset_entity_fields(u32 entity) {
    ST32(entity + 8, 8);
    ST32(entity + 0x60, 0);
    return 0;
}

/* entity_resetTimerAndCounter_80E5410(r0 = entity) -> r1 */
Next entity_resetTimerAndCounter_80E5410(void) {
    R1 = reset_entity_fields(R0);
    RETURN_TO_CALLER();
}

/* ── battle_clearDropEntryById ── */
/* Clear the drop entry whose id matches, in the battle state's two-slot drop table.
 *
 * r10 holds the toolkit pointer; the drop table hangs off the battle state and
 * holds two entries: an id word at +0/+4 and a paired word at +8/+12 for the
 * first/second slot. Matching entries get both words zeroed.
 *
 * Returns the id word last read from the second slot (0 if the second slot
 * matched and was cleared), which the original leaves behind in r3. */
static u32 clear_drop_entry_by_id(u32 id, u32 toolkit) {
    u32 drops = LD32(toolkit + oToolkit_BattleStatePtr) + oBattleState_Drop;
    u32 lastId = 0;

    for (u32 slot = 0; slot < 2; slot++) {
        u32 entryId = LD32(drops);
        if (entryId == id) {
            ST32(drops, 0);
            ST32(drops + 8, 0);
            entryId = 0;
        }
        drops += 4;
        lastId = entryId;
    }

    return lastId;
}

/* battle_clearDropEntryById(r0 = drop id, r10 = toolkit) -> r3 */
Next battle_clearDropEntryById(void) {
    R3 = clear_drop_entry_by_id(R0, R10);
    RETURN_TO_CALLER();
}

/* ── t4_0x42_80E5470 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_t4_0x42_80E5470;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_t4_0x42_80E5470 call_object_updateSprite_t4_0x42_80E5470(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_t4_0x42_80E5470){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* t4_0x42_80E5470 -- per-tick update for a type-0x42 battle object.
 *
 * The object (r5) carries a state byte; a small table maps it to a state
 * handler that runs first (the state-specific behaviour), and afterwards the
 * shared object_updateSprite runs on the same object. The handlers keep the
 * object pointer in r5, so the pointer is unchanged across the dispatch.
 */

/* Calls the state handler at `handler` (a Thumb code address from the table
 * below) with r0 = cur_state and r5 = object. Only r0 comes back; the
 * handlers use r5 as their object pointer and preserve it. */
static u32 call_state_handler_t4_0x42_80E5470(u32 handler, u32 cur_state, u32 object) { /* register form: calls through a guest code address (noted by the tool) */
    R0 = cur_state;
    R5 = object;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Run this object's state handler, then its sprite update. Returns the
 * object pointer as object_updateSprite leaves it (for output r5). */
static u32 t4_0x42_tick(u32 object) {
    /* CurState is used directly as a byte offset into the handler table
     * (the game stores 0/4/8 there), so no shift is applied. */
    u32 cur_state = LD8(object + oBattleObject_CurState);
    u32 handler   = LD32(A_off_80E5488 + cur_state);

    call_state_handler_t4_0x42_80E5470(handler, cur_state, object);
    return call_object_updateSprite_t4_0x42_80E5470(object).r5;
}

/* r5 = object, r10 = caller's context (passed through untouched).
 * Outputs: r5 from the tick; r8-r12 are whatever the callees left. */
Next t4_0x42_80E5470(void) {
    R5 = t4_0x42_tick(R5);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchObjectAction_80E54DC ── */
/* Battle-object action dispatcher (battle_dispatchObjectAction_80E54DC).
 *
 * Reads the battle object's current action (CurAction byte) and calls the
 * matching handler from the jump table at off_80E54F0:
 *   action 0x00 -> object_slideXAndCheckPanel
 *   action 0x04 -> battle_introAnimPhaseHandler
 *   action 0x08 -> object_destroyIfExtraVarTargetNull
 * (the table is indexed with the raw byte, so only those values land on entries).
 *
 * The original invokes the handler through `mov lr, pc; bx r1` with
 *   r0 = action byte, r1 = handler address, r5 = battle object,
 * and its results are the outgoing r5 and r8-r12, which the handler leaves
 * in the guest registers directly; nothing else needs to be written here.
 */

/* register form: dispatches through the game's jump table with the handler
 * address loaded from guest memory, so the call has to go through
 * GUEST_CALL_ADDR; the callee receives r0/r1/r5 exactly as the original
 * leaves them at its `bx r1`. */
static void run_battle_action_handler(u32 object) {
    u32 action  = LD8(object + oBattleObject_CurAction);
    u32 handler = LD32(A_off_80E54F0 + action);

    R0 = action;   /* handler argument: the raw action byte               */
    R1 = handler;  /* the original leaves the handler address here too    */
    R5 = object;   /* handler argument: the battle object being acted on  */

    GUEST_CALL_ADDR(handler);
}

Next battle_dispatchObjectAction_80E54DC(void) {
    run_battle_action_handler(R5);
    RETURN_TO_CALLER();
}

/* ── t4_0x5a_80E70C8 ── */
#define oBattleObject_Param1_t4_0x5a_80E70C8 0x4u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpritePaused_result_t4_0x5a_80E70C8;
/* register form: generated wrapper: object_updateSpritePaused has no typed C interface yet */
static inline call_object_updateSpritePaused_result_t4_0x5a_80E70C8 call_object_updateSpritePaused_t4_0x5a_80E70C8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpritePaused);
    return (call_object_updateSpritePaused_result_t4_0x5a_80E70C8){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSpriteTimestop_result;
/* register form: generated wrapper: object_updateSpriteTimestop has no typed C interface yet */
static inline call_object_updateSpriteTimestop_result call_object_updateSpriteTimestop(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSpriteTimestop);
    return (call_object_updateSpriteTimestop_result){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * Per-frame update of one battle object (state-handler trampoline).
 *
 * The object's current state selects a handler through the word table at
 * off_80E70EC (battle_restoreBaseSprite / battle_objectMirrorRelatedAndFlinchStep_80E7142 / object_freeMemory);
 * the game indexes that table with the raw oBattleObject_CurState byte,
 * without scaling the index. Afterwards the object's sprite is advanced,
 * either normally (Param1 == 0) or under timestop (Param1 != 0).
 *
 * The object pointer in r5 is preserved throughout (it is the callees'
 * argument and callee-saved), and r8-r12 are whatever the chosen handler
 * leaves behind, exactly as in the original.
 */

/* Call the object's current-state handler through the jump table.
 * register form: the call target is read from a jump table in guest memory,
 * so no generated wrapper covers these callees. */
static u32 call_object_state_handler_t4_0x5a_80E70C8(u32 object) {
    u32 state   = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E70EC + state);

    R0 = state;     /* the original reaches the call with the state byte in r0... */
    R1 = handler;   /* ...and the table entry in r1 */
    R5 = object;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Run the state handler, then the sprite update for this frame.
 * Returns the object pointer for the r5 output. */
static u32 battle_object_step_t4_0x5a_80E70C8(u32 object) {
    call_object_state_handler_t4_0x5a_80E70C8(object);

    if (LD8(object + oBattleObject_Param1_t4_0x5a_80E70C8) == 0)
        call_object_updateSpritePaused_t4_0x5a_80E70C8(object);   /* animation runs normally */
    else
        call_object_updateSpriteTimestop(object); /* animation frozen this frame */

    return object;
}

/* t4_0x5a_80E70C8(r5 = object) -> r5 (r8-r12 left by the state handler) */
Next t4_0x5a_80E70C8(void) {
    R5 = battle_object_step_t4_0x5a_80E70C8(R5);
    RETURN_TO_CALLER();
}

/* ── battle_restoreBaseSprite ── */
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized_battle_restoreBaseSprite 0x8u
#define CUR_STATE_UPDATE_battle_restoreBaseSprite 0x4u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5; } call_sprite_load_result_battle_restoreBaseSprite;
/* register form: generated wrapper: sprite_load has no typed C interface yet */
static inline call_sprite_load_result_battle_restoreBaseSprite call_sprite_load_battle_restoreBaseSprite(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sprite_load);
    return (call_sprite_load_result_battle_restoreBaseSprite){ R4, R5 };
}

typedef struct { u32 r1, r2, r4, r5; } call_sprite_loadAnimationData_result_battle_restoreBaseSprite;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result_battle_restoreBaseSprite call_sprite_loadAnimationData_battle_restoreBaseSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result_battle_restoreBaseSprite){ R1, R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow_battle_restoreBaseSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation_battle_restoreBaseSprite(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r3; bool c, v; } call_sprite_setPalette_result_battle_restoreBaseSprite;
/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline call_sprite_setPalette_result_battle_restoreBaseSprite call_sprite_setPalette_battle_restoreBaseSprite(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
    return (call_sprite_setPalette_result_battle_restoreBaseSprite){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFlip_result_battle_restoreBaseSprite;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result_battle_restoreBaseSprite call_object_getFlip_battle_restoreBaseSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result_battle_restoreBaseSprite){ R0 };
}

typedef struct { u32 r1, r2, r3; bool c, v; } call_sprite_setFlip_result_battle_restoreBaseSprite;
/* register form: generated wrapper: sprite_setFlip has no typed C interface yet */
static inline call_sprite_setFlip_result_battle_restoreBaseSprite call_sprite_setFlip_battle_restoreBaseSprite(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setFlip);
    return (call_sprite_setFlip_result_battle_restoreBaseSprite){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/*
 * battle_restoreBaseSprite (0x080e70f8)
 *
 * Put a battle object back onto its base (idle) sprite: copy the sprite
 * parameters the related object saved at +0x34 back into this object, reload
 * the base graphics/animation, reapply palette and flip, and switch the
 * object's state machine back to the normal update state.
 *
 * Takes the object pointer in r5 and hands it back unchanged (the callees
 * all keep it); r4/r5 end exactly as they started.
 */
static u32 battle_restore_base_sprite(u32 obj) {
    /* Copy the 12 saved bytes at +0x34 of the related object back onto us.
     * Load all three words first, like ldmia/stmia does, in case the ranges
     * ever overlap. */
    u32 src = LD32(obj + oBattleObject_RelatedObject1Ptr) + 0x34;
    u32 dst = obj + 0x34;
    u32 saved0 = LD32(src);
    u32 saved1 = LD32(src + 4);
    u32 saved2 = LD32(src + 8);
    ST32(dst, saved0);
    ST32(dst + 4, saved1);
    ST32(dst + 8, saved2);

    /* Reload the base sprite graphics (tile 0x80, 8 x 15) and its frames. */
    call_sprite_load_battle_restoreBaseSprite(0x80, 8, 0xF, obj);
    call_sprite_loadAnimationData_battle_restoreBaseSprite(obj);
    call_sprite_noShadow_battle_restoreBaseSprite(obj);

    /* Mark the object header as needing a sprite refresh (set flag bit 1). */
    ST8(obj + oObjectHeader_Flags, LD8(obj + oObjectHeader_Flags) | 2);

    /* Restart the base animation (index 8) and load its frame data. */
    ST8(obj + oBattleObject_CurAnim, 8);
    ST8(obj + oBattleObject_CurAnimCopy, 8);
    call_sprite_setAnimation_battle_restoreBaseSprite(8, obj);
    call_sprite_loadAnimationData_battle_restoreBaseSprite(obj);

    /* Back to palette 0, then reapply whatever flip the object currently has. */
    call_sprite_setPalette_battle_restoreBaseSprite(0, obj);
    call_sprite_setFlip_battle_restoreBaseSprite(call_object_getFlip_battle_restoreBaseSprite(obj).r0, obj);

    /* Hand control back to the normal per-frame update state. */
    ST32(obj + oBattleObject_CurStateActionPhaseAndPhaseInitialized_battle_restoreBaseSprite, CUR_STATE_UPDATE_battle_restoreBaseSprite);

    return obj;   /* r5 passes through unchanged */
}

Next battle_restoreBaseSprite(void) {
    R5 = battle_restore_base_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectMirrorRelatedAndFlinchStep_80E7142 ── */
#define oBattleObject_Param1_sub_80E7142 0x4u
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E7142 0x8u
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r3; } call_battle_isBattleOver_result;
/* register form: generated wrapper: battle_isBattleOver has no typed C interface yet */
static inline call_battle_isBattleOver_result call_battle_isBattleOver(void) {
    GUEST_CALL(battle_isBattleOver);
    return (call_battle_isBattleOver_result){ R0, R1, R3 };
}

typedef struct { u32 r0, r1; bool z, n; } call_battle_isTimeStop_result;
/* register form: generated wrapper: battle_isTimeStop has no typed C interface yet */
static inline call_battle_isTimeStop_result call_battle_isTimeStop(void) {
    GUEST_CALL(battle_isTimeStop);
    return (call_battle_isTimeStop_result){ R0, R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: battle_objectFlinchStep has no typed C interface yet */
static inline void call_battle_objectFlinchStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_objectFlinchStep);
}

/* Battle object per-tick step: when the battle is over the object is switched to
 * the destroy state; otherwise it mirrors three words from its related object
 * and then performs a flinch step, unless the related object's current action
 * says otherwise and time is stopped. */

/* r5 = object pointer, r10 = battle context (used by the callees). */
static void sub_80E7142_step(u32 self) {
    if (call_battle_isBattleOver().r0 == 0) {
        u32 related = LD32(self + oBattleObject_RelatedObject1Ptr);

        /* Copy the related object's fields at +0x34..+0x3F onto this object. */
        u32 w0 = LD32(related + 0x34);
        u32 w1 = LD32(related + 0x38);
        u32 w2 = LD32(related + 0x3C);
        ST32(self + 0x34, w0);
        ST32(self + 0x38, w1);
        ST32(self + 0x3C, w2);

        /* A nonzero Param1 means the object must flinch this tick. */
        if (LD8(self + oBattleObject_Param1_sub_80E7142) == 0) {
            u32 action = LD8(related + oBattleObject_CurAction);
            if (action == 0xA) {
                /* Action 0xA only flinches when time is not stopped;
                 * battle_isTimeStop leaves Z set when time is stopped. */
                if (!call_battle_isTimeStop().z)
                    return;
            } else {
                /* Any other related action destroys this object. */
                ST32(self + oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E7142,
                     CUR_STATE_DESTROY);
                return;
            }
        }

        call_battle_objectFlinchStep(self);
        return;
    }

    /* Battle already over: destroy this object. */
    ST32(self + oBattleObject_CurStateActionPhaseAndPhaseInitialized_sub_80E7142,
         CUR_STATE_DESTROY);
}

/* battle_objectMirrorRelatedAndFlinchStep_80E7142(r5 = object, r10 = battle context) -> no outputs */
Next battle_objectMirrorRelatedAndFlinchStep_80E7142(void) {
    sub_80E7142_step(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectFlinchStep ── */
/* Advance a battle object's flinch state machine:
 *  - if it is already finishing the flinch (action 8), do nothing;
 *  - if it is not yet flinching (action != 4), start the flinch pose
 *    (action 4) with a 15-frame timer;
 *  - count the timer down each frame, and once it reaches zero switch to
 *    the recovery animation (anim 9) and end the flinch (action 8). */
static void flinch_step(u32 obj) {
    u32 action = LD8(obj + oBattleObject_CurAction);

    if (action == 8)
        return;                        /* flinch already finished */

    if (action != 4) {
        /* Enter the flinch pose and start its timer. */
        ST8(obj + oBattleObject_CurAction, 4);
        ST16(obj + oBattleObject_Timer, 15);
    }

    /* Tick the timer; keep flinching while it is still positive. */
    u16 timer = (u16)(LD16(obj + oBattleObject_Timer) - 1);
    ST16(obj + oBattleObject_Timer, timer);

    if ((s32)timer <= 0) {
        /* Timer expired: play the recovery animation and end the flinch. */
        ST8(obj + oBattleObject_CurAnim, 9);
        ST8(obj + oBattleObject_CurAction, 8);
    }
}

/* battle_objectFlinchStep(r5 = battle object) -> nothing */
Next battle_objectFlinchStep(void) {
    flinch_step(R5);
    RETURN_TO_CALLER();
}

/* ── battle_markObjectDead_80E71B8 ── */
/* Write the value 8 to object+8 when the object pointer is non-NULL.
 * This is the usual "flag object as destroyed" setter used all over the battle code.
 * r1 is an output: it becomes 8 on a successful store and keeps its incoming
 * value when the pointer is NULL (the original leaves it untouched there). */
static u32 mark_object_at(u32 obj, u32 r1) {
    if (obj == 0)
        return r1;              /* NULL pointer: nothing to write */
    ST32(obj + 8, 8);
    return 8;
}

/* battle_markObjectDead_80E71B8(r0 = object pointer, r1 = incoming r1) -> r1 */
Next battle_markObjectDead_80E71B8(void) {
    R1 = mark_object_at(R0, R1);
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

/* ── t4_0x6b_80E807C ── */
/* t4 battle-object state dispatcher (080E807C).
 *
 * The object pointer arrives in r5. The handler is picked from the jump table
 * at off_80E8090, indexed by the object's current-state byte, and is invoked
 * with the state in r0 while the object pointer stays in r5:
 *   state 0 -> battle_initSpriteObj, state 1 -> battle_updateAttachedSprite, state 2 -> object_freeMemory.
 * The dispatch is a tail call: the original pushes lr, calls the handler with
 * lr = 0x80E8088, and the pop {pc} there returns straight to this function's
 * caller, so whatever the handler leaves in r5 and r8-r12 is this function's
 * result. Those registers are never touched here, so they flow through. */

/* register form: the handler address comes from the state jump table and is
 * reached with mov lr, pc / bx r1, so it must be invoked via GUEST_CALL_ADDR;
 * the callee expects the state in r0 and its r5/r8-r12 results must pass
 * through unchanged to this function's caller. */
static u32 call_state_handler_t4_0x6b_80E807C(u32 handler, u32 state) {
    R0 = state;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Read the object's state byte, load the matching handler from the state
 * table (the raw byte is the word offset, exactly as ldr r1,[r1,r0] does),
 * and run it with the object pointer still in r5. */
static void run_t4_state_handler(u32 object) {
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80E8090 + state);
    call_state_handler_t4_0x6b_80E807C(handler, state);
}

/* r5 = object pointer. r5 and r8-r12 end as the state handler leaves them. */
Next t4_0x6b_80E807C(void) {
    run_t4_state_handler(R5);
    RETURN_TO_CALLER();
}

/* ── battle_spawnObject6B_linkRelated_80E8124 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7; } call_object_spawnType4_result_sub_80E8124;
/* register form: generated wrapper: object_spawnType4 has no typed C interface yet */
static inline call_object_spawnType4_result_sub_80E8124 call_object_spawnType4_sub_80E8124(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(object_spawnType4);
    return (call_object_spawnType4_result_sub_80E8124){ R1, R2, R3, R4, R5, R6, R7 };
}

/* Spawn battle object 0x6B and link the caller's object to it.
 *
 * r1-r4 are passed straight through to object_spawnType4; the spawner reports
 * the newly created object in its r5 result. When it did produce one, the
 * caller's r5 value (an existing battle object) is recorded in the new
 * object's RelatedObject1 slot. r5 itself is popped back unchanged, and the
 * spawner's other results (r2, r3, r6, r7) are simply left in place for the
 * caller. */
static u32 spawn_object_and_link_related(u32 a1, u32 a2, u32 a3, u32 a4, u32 related) {
    call_object_spawnType4_result_sub_80E8124 res = call_object_spawnType4_sub_80E8124(0x6B, a1, a2, a3, a4);

    if (res.r5 != 0)    /* the mov r0,r5 / beq check: skip the store if no object */
        ST32(res.r5 + oBattleObject_RelatedObject1Ptr, related);

    return related;     /* r5 is restored by the original's pop {r5} */
}

/* battle_spawnObject6B_linkRelated_80E8124 (r1-r4 = spawner arguments, r5 = object to link) -> r2 r3 r5 r6 r7 */
Next battle_spawnObject6B_linkRelated_80E8124(void) {
    R5 = spawn_object_and_link_related(R1, R2, R3, R4, R5);
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
 * post-state hook (battleObject_animationAndSpriteUpdate_801BCF4).
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

/* register form: calls battleObject_animationAndSpriteUpdate_801BCF4 outside this file, which has no typed C interface yet */
static void call_post_state_update(void) {
    GUEST_CALL(battleObject_animationAndSpriteUpdate_801BCF4);
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

/* ── battle_runPwrAtkStep ── */
/* Offsets into the battle object (r7) used by the PowerAtk action. */
#define PWR_ATK_STEP_BYTE 0x1E /* current step number; also the byte offset into the handler table */
#define PWR_ATK_ARM_BYTE  0x22 /* set to 1 once the A button is pressed in time */
#define PWR_ATK_WAIT_BYTE 0x23 /* per-call countdown; the A button is only read while it is 0 */

/* register form: the PowerAtk step handler is reached through the jump table
 * at off_80EADD0 (mov lr, pc / bx r1) and is entered with the caller's whole
 * register state -- r0 = step byte, r1 = handler address, r3 = &step byte,
 * r7 = battle object, r2/r4-r6/r8-r12 passed through -- which no typed call
 * can express. The tail after the call also has to live here: it patches r1
 * and r7 in place, and r1/r5/r8-r12 are this function's outputs, so the
 * handler's exit values must survive untouched. */
static u32 call_pwr_atk_handler(u32 obj, u32 r2, u32 r4, u32 r5, u32 r6,
                                u32 r8, u32 r9, u32 r10, u32 r11, u32 r12)
{
    u32 step = LD8(obj + PWR_ATK_STEP_BYTE);
    u32 handler = LD32(A_off_80EADD0 + step);

    R0 = step;         /* the step byte's value, exactly as the original loads it */
    R1 = handler;      /* Thumb address from the jump table */
    R2 = r2;
    R3 = obj + PWR_ATK_STEP_BYTE; /* the handler gets a pointer to the step byte */
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = obj;
    R8 = r8;
    R9 = r9;
    R10 = r10;
    R11 = r11;
    R12 = r12;

    GUEST_CALL_ADDR(handler);

    /* pop {r7}: the original continues with r7 = obj + 0x1E, so its
     * [r7,#4]/[r7,#5] accesses are the object's +0x22/+0x23 bytes. */
    u32 wait = LD8(obj + PWR_ATK_WAIT_BYTE);
    if (wait != 0) {
        /* Still waiting: count one frame down and ignore input.
         * r1 keeps whatever the handler left in it. */
        ST8(obj + PWR_ATK_WAIT_BYTE, wait - 1);
    } else {
        R1 = 1; /* mov r1, #1 happens before the button test */
        if ((LD16(R4 + oAIData_JoypadPressed) & 1) != 0) /* bit 0 = A */
            ST8(obj + PWR_ATK_ARM_BYTE, 1);
    }

    /* pop {r7,pc}: the original returns with the caller's r7 restored. */
    R7 = obj;

    return R0;
}

/* One frame of the PowerAtk action: run the current step's handler, then
 * either tick the input-wait counter or arm the attack on an A press.
 * The registers the handler leaves in r1/r5/r8-r12 stay in place for this
 * function's callers, exactly as in the original. */
static void run_pwr_atk_step(u32 r2, u32 r4, u32 obj, u32 r5, u32 r6,
                             u32 r8, u32 r9, u32 r10, u32 r11, u32 r12)
{
    call_pwr_atk_handler(obj, r2, r4, r5, r6, r8, r9, r10, r11, r12);
}

/* battle_runPwrAtkStep(r2, r4 = AIData, r7 = battle object, r5, r6, r8-r12)
 * Outputs r1/r5/r8-r12 are whatever the step handler left there (r1 = 1 when
 * the wait counter had run out); r7 comes back restored. */
Next battle_runPwrAtkStep(void) {
    run_pwr_atk_step(R2, R4, R7, R5, R6, R8, R9, R10, R11, R12);
    RETURN_TO_CALLER();
}

/* ── battle_commitPanelMove ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_clearFlag has no typed C interface yet */
static inline void call_object_clearFlag_battle_commitPanelMove(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_clearFlag);
}

typedef struct { u32 r3; } call_object_removePanelReserve_result;
/* register form: generated wrapper: object_removePanelReserve has no typed C interface yet */
static inline call_object_removePanelReserve_result call_object_removePanelReserve(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_removePanelReserve);
    return (call_object_removePanelReserve_result){ R3 };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_battle_commitPanelMove(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r3, r4, r6; bool c, v; } call_object_updateCollisionPanels_result_battle_commitPanelMove;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_battle_commitPanelMove call_object_updateCollisionPanels_battle_commitPanelMove(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_battle_commitPanelMove){ R3, R4, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_clearFlag2 has no typed C interface yet */
static inline void call_object_clearFlag2(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_clearFlag2);
}

typedef struct { u32 r3; bool c, v; } call_object_reservePanel_result;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result call_object_reservePanel(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result call_object_setFlag1_battle_commitPanelMove(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result){ R1, R2 };
}

/*
 * Commit a battle object's queued panel move: the panel the object was moving
 * to becomes its current panel, the reservation on the old panel is released,
 * position and collision data are refreshed, the new panel is reserved and
 * sliding is locked until the move animation has played. Also resets the
 * move-request struct pointed to by r3.
 *
 * Register notes carried over from the original:
 *  - r1 is left as object_setFlag1 leaves it and r4 as
 *    object_updateCollisionPanels leaves it; callers read both, and running
 *    the real callees reproduces them, so nothing is written here.
 *  - r6 is preserved (the original saves/restores it), but
 *    object_updateCollisionPanels clobbers it, so the adapter keeps a copy.
 */
static void battle_commitPanelMove_run(u32 moveState, u32 obj) {
    /* Cancel the "panel move pending" flag (bit 22 of the object flags). */
    call_object_clearFlag_battle_commitPanelMove(1u << 22, obj);

    /* The planned panel becomes the current panel. */
    u32 x = LD8(obj + oBattleObject_FuturePanelX);
    ST8(obj + oBattleObject_PanelX, x);
    u32 y = LD8(obj + oBattleObject_FuturePanelY);
    ST8(obj + oBattleObject_PanelY, y);

    /* Give up the old panel's reservation, then recompute the object's
     * pixel coordinates and collision panels from its new panel. */
    call_object_removePanelReserve(x, y, obj);
    call_object_setCoordinatesFromPanels_battle_commitPanelMove(obj);
    call_object_updateCollisionPanels_battle_commitPanelMove(obj);

    /* Clear the step/slide bookkeeping flags (pool literal dword_80EB03C
     * = 0x1040) and the 0x10 flag. */
    call_object_clearFlag_battle_commitPanelMove(0x1040, obj);
    call_object_clearFlag2(0x10, obj);

    /* Reserve the panel we now stand on, keeping the planned coordinates
     * in step with the current ones. */
    x = LD8(obj + oBattleObject_PanelX);
    y = LD8(obj + oBattleObject_PanelY);
    ST8(obj + oBattleObject_FuturePanelX, x);
    ST8(obj + oBattleObject_FuturePanelY, y);
    call_object_reservePanel(x, y, obj);

    /* Forbid further sliding until the move has finished. */
    call_object_setFlag1_battle_commitPanelMove(OBJECT_FLAGS_CANNOT_SLIDE, obj);

    /* Reset the move request: step index 0, wait timer 12, state 4. */
    ST8(moveState + 4, 0);
    ST8(moveState + 5, 0xC);
    ST16(moveState, 4);
}

/* battle_commitPanelMove(r3 = move-request struct, r5 = battle object)
 * -> r1, r4 left behind by the callees; flags and r0/r2/r3 are clobbered. */
Next battle_commitPanelMove(void) {
    u32 savedR6 = R6;    /* preserved overall, but clobbered by a callee */
    battle_commitPanelMove_run(R3, R5);
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── battle_objectCheckGuardAction_80EAF1A ── */
/* Check the battle object's current action: return 0xC when the object is
 * performing action 0x52 (guard/shield-style action), otherwise 0. */
static u32 get_action_result(u32 object) {
    if (LD8(object + oBattleObject_CurAction) == 0x52)
        return 0xC;
    return 0;
}

/* battle_objectCheckGuardAction_80EAF1A(r5 = battle object) -> r0 */
Next battle_objectCheckGuardAction_80EAF1A(void) {
    R0 = get_action_result(R5);
    RETURN_TO_CALLER();
}

/* ── getSlashBeastWaveAttackLockonTypeMaybe_80EAF26 ── */
/* If the object's current action is "slash beast wave attack" (0x41), return the
 * lock-on type from a per-AI-difficulty lookup table; otherwise return 0. */
static u32 get_lockon_type(u32 objectPtr, u32 aiVarsPtr) {
    if (LD8(objectPtr + oBattleObject_CurAction) != 0x41)
        return 0;
    return LD8(A_byte_80EB028 + LD8(aiVarsPtr + oAIAttackVars_AiDifficultyTier));
}

/* getSlashBeastWaveAttackLockonTypeMaybe_80EAF26(r5 = battle object, r7 = AI attack vars, lr) -> r0 */
Next getSlashBeastWaveAttackLockonTypeMaybe_80EAF26(void) {
    R0 = get_lockon_type(R5, R7);
    RETURN_TO_CALLER();
}

/* ── spawnBattleIllusion ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFlip_result;
/* register form: generated wrapper: object_getFlip has no typed C interface yet */
static inline call_object_getFlip_result call_object_getFlip(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlip);
    return (call_object_getFlip_result){ R0 };
}

typedef struct { u32 r0, r2, r3, r5; } call_spawnIllusionObject_80E33FA_result;
/* register form: generated wrapper: spawnIllusionObject_80E33FA has no typed C interface yet */
static inline call_spawnIllusionObject_80E33FA_result call_spawnIllusionObject_80E33FA(u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(spawnIllusionObject_80E33FA);
    return (call_spawnIllusionObject_80E33FA_result){ R0, R2, R3, R5 };
}

typedef struct { u32 r0; } call_GetBattleNaviStatsByte_result;
/* register form: generated wrapper: GetBattleNaviStatsByte has no typed C interface yet */
static inline call_GetBattleNaviStatsByte_result call_GetBattleNaviStatsByte(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(GetBattleNaviStatsByte);
    return (call_GetBattleNaviStatsByte_result){ R0 };
}

/* register form: generated wrapper: battle_objectSetExtraVar3_80E341E has no typed C interface yet */
static inline void call_sub_80E341E(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(battle_objectSetExtraVar3_80E341E);
}

/*
 * Spawn a holographic illusion copy of a battle object, then program its
 * flicker rate from the owning navi's stats.
 *
 * Register dance worth noting in the original: r0-r3 are stashed on the
 * stack around the object_getFlip call and popped back shifted one slot
 * (pop {r1-r3}; pop {r0}), so the spawner receives r1=old r0, r2=old r1,
 * r3=old r2, while the old r3 only offsets the OAM attr1 base. r4, r6 and
 * r7 are saved on entry and popped back before returning, so they must
 * reach the caller exactly as they arrived.
 */

#define ILLUSION_ATTR2       0x83E0u     /* dword_80EB01C: fixed attr2 word */
#define ILLUSION_ATTR1_BASE  0x1010014u  /* dword_80EB020: attr1 base, minus old r3 */
#define DEFAULT_PALETTE      0xFu        /* palette slot used by most name IDs */
#define NAME_ID_FIRST        0xA2        /* special name IDs 0x1A1..0x1AB */
#define NAME_ID_LAST         0xAC
#define STATS_INDEX          0x2C        /* navi stats byte queried for flicker */
#define STAT_MID_LO          0xB         /* fast flicker while stat is 0x0B..0x18 */
#define STAT_MID_HI          0x18
#define FLICKER_FAST         1u
#define FLICKER_SLOW         2u

/* OAM attr2 for the illusion: flip state in the top byte, palette slot 0xFF
 * in the low byte; the slot nibble is 0xF unless the object's name ID falls
 * in the special 0x1A1-0x1AB range, where it becomes 0. */
static u32 illusion_attr2(u32 obj) {
    u32 flip = call_object_getFlip(obj).r0;
    u32 palette = DEFAULT_PALETTE;

    s32 name_id = (s32)LD16(obj + oBattleObject_NameID) - 0xFF;
    if (name_id >= NAME_ID_FIRST && name_id <= NAME_ID_LAST)
        palette = 0;

    return (((flip << 8) + palette) << 16) + 0xFF;
}

/* Exit state: r4/r6/r7 restored, r5 as the spawner leaves it. */
typedef struct { u32 r4, r6, r7; } SpawnBattleIllusionResult;

static SpawnBattleIllusionResult spawn_battle_illusion(u32 a0, u32 a1, u32 a2, u32 a3,
                                                       u32 obj, u32 r4_in, u32 r6_in,
                                                       u32 r7_in) {
    u32 attr2 = illusion_attr2(obj);
    u32 attr1 = ILLUSION_ATTR1_BASE - a3;

    /* r5 rides through the spawner unchanged and is read again below. */
    call_spawnIllusionObject_80E33FA_result spawned =
        call_spawnIllusionObject_80E33FA(a0, a1, a2, attr2, obj,
                                         ILLUSION_ATTR2, attr1);

    if (spawned.r0 != 0) {
        /* Pick the flicker rate from the navi stats of the alliance. */
        u32 illusion = spawned.r0;
        u32 alliance = LD8(spawned.r5 + oBattleObject_Alliance);
        s32 stat = (s32)call_GetBattleNaviStatsByte(alliance, STATS_INDEX).r0;

        u32 flicker;
        if (stat >= STAT_MID_LO && stat <= STAT_MID_HI)
            flicker = FLICKER_FAST;
        else
            flicker = FLICKER_SLOW;
        call_sub_80E341E(illusion, flicker);
    }

    SpawnBattleIllusionResult res;
    res.r4 = r4_in;   /* the original pops these back before returning */
    res.r6 = r6_in;
    res.r7 = r7_in;
    return res;
}

/* spawnBattleIllusion(r0-r3 = spawn arguments, r5 = battle object) -> r5 r6 */
Next spawnBattleIllusion(void) {
    SpawnBattleIllusionResult res =
        spawn_battle_illusion(R0, R1, R2, R3, R5, R4, R6, R7);
    R4 = res.r4;
    R6 = res.r6;
    R7 = res.r7;
    RETURN_TO_CALLER();
}

/* ── aiAttack_stepStateMachine_80EB04C ── */
/* aiAttack_stepStateMachine_80EB04C: run one step of the AI attack-state machine for the object in R7.
 *
 * The state byte oAIAttackVars_HitElement is a byte offset into the handler table
 * at off_80EB074 (entries are 4 bytes apart, so it holds 0, 4, 8, 0xC or 0x10).
 * The selected handler is entered with r0 = state byte and r1 = its own table
 * entry (the original's `mov lr, pc / bx r1` dispatch), and may clobber the
 * flags and LR, which pass straight through to this function's caller.
 *
 * After the handler returns: if it latched "busy" into oAIAttackVars_ScratchVar,
 * we're done. Otherwise we poll the D-pad; if the currently held direction
 * differs from the one remembered in oAIAttackVars_AttackBoost2, store 1 into
 * oAIAttackVars_ScratchVar.
 */
Next aiAttack_stepStateMachine_80EB04C(void) {
    u32 state = LD8(R7); /* oAIAttackVars_HitElement is at offset 0 */

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
    u32 busy = LD32(R7 + oAIAttackVars_ScratchVar);
    R0 = busy;
    SET_FLAGS_NZ(busy);
    if (busy != 0)
        RETURN_TO_CALLER();

    /* busy is 0 here, so r0 = 0 going into the button poll */
    GUEST_CALL(playerObject_checkDirectionButtons_800FA54);

    /* r1 ends up holding the remembered direction either way */
    R1 = LD8(R7 + oAIAttackVars_AttackBoost2);
    SET_FLAGS_CMP(R0, R1);
    if (R0 != R1) {
        R0 = 1;                              /* mov r0, #1 also sets nz */
        ST32(R7 + oAIAttackVars_ScratchVar, R0);
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── aiAttack_TimerTickEnd_80EB194 ── */
/* Offsets/bit positions referenced by the assembly that aren't in game.h. */
#define OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT 19   /* lsl r0, r0, #19 */
#define oAIAttackVars_HitElement 0                   /* strh r0, [r7,#0] */

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
    R0 = LD16(R7 + oAIAttackVars_StepTimer);
    SET_FLAGS_CMP(R0, 1);               /* SUBS r0, #1 */
    R0 -= 1;
    ST16(R7 + oAIAttackVars_StepTimer, R0);
    if (R0 != 0)
        RETURN_TO_CALLER();

    /* Timer expired: no longer blocked, and the object starts moving. */
    R0 = 0x40;
    GUEST_CALL(object_clearFlag);

    R0 = 1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
    GUEST_CALL(object_setFlag1);

    /* Reload the timer for the next attack and ask the AI layer about it.
     * R0 still holds the reloaded value when battle_aiAttack_checkMysteryDataUnk_80F02A2 is entered. */
    R0 = LD16(R7 + oAIAttackVars_AttackDuration);
    ST16(R7 + oAIAttackVars_StepTimer, R0);
    GUEST_CALL(battle_aiAttack_checkMysteryDataUnk_80F02A2);

    if (R0 != 0) {
        R0 = 0;
        GUEST_CALL(object_setAnimation);
    }

    /* Back to the idle/step state. */
    R0 = 0xC;
    SET_FLAGS_NZ(R0);
    ST16(R7 + oAIAttackVars_HitElement, R0);
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
 * oAIAttackVars_AttackElement; that name is not in the shared header, so provide it. */

/* aiAttackTimerTick_80EB758: AI attack-state timer tick (r7 = attack vars object).
 * On the first invocation (phase byte still 0) the countdown is initialised
 * to 5 frames and the phase byte is marked started (4).  Every call then
 * decrements the timer; when it goes negative the object leaves its attack
 * state.  Flags left behind are those of the timer decrement (or, when the
 * exit-state routine runs, whatever it leaves). */
Next aiAttackTimerTick_80EB758(void) {
    /* Initialise the attack countdown on the first invocation. */
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        ST16(R7 + oAIAttackVars_StepTimer, 5);
        ST8(R7 + oAIAttackVars_AttackElement, 4);
    }

    /* Decrement the frame counter and store it back. */
    u32 timer = LD16(R7 + oAIAttackVars_StepTimer);
    u32 remaining = timer - 1;
    ST16(R7 + oAIAttackVars_StepTimer, remaining);
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
    u32 unk03 = LD8(R7 + 3); /* oAIAttackVars_AiDifficultyTier */

    /* cmp r1, #0xB -- branch is taken when the byte differs from 11 */
    SET_FLAGS_CMP(unk03, 0xB);
    u16 command = (unk03 == 0xB) ? 2 : 1;

    ST16(R7 + oAIAttackVars_ChargeCycles, command);
    ST32(R7 + oAIAttackVars_ScratchVar, 0);

    R0 = 8;
    R1 = unk03; /* r1 still holds the byte loaded at the top */
    ST16(R7 + 0, 8); /* oAIAttackVars_HitElement */
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
    if (LD8(R7 + 1) == 0) {         /* oAIAttackVars_AttackElement: state byte */
        /* First tick: tear down the previous attack's leftovers and start
         * the wind-up animation. */
        ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);

        u32 ai = LD32(R5 + oBattleObject_AIDataPtr);
        ST32(ai + oAIData_TempAttackObjectPtr, 0);

        R0 = 7;                 /* animation id */
        R1 = ai;                /* AI data the animation applies to */
        GUEST_CALL(object_setAnimation);

        ST16(R7 + oAIAttackVars_StepTimer, 3);   /* attack timer */
        ST8(R7 + 1, 4);                       /* state: attack in progress */
    }

    /* Count the attack timer down in r0, as the original does; on underflow
     * the attack is over. */
    R0 = LD16(R7 + oAIAttackVars_StepTimer);
    SET_FLAGS_CMP(R0, 1);       /* flags of the original's SUBS r0, #1 */
    R0 -= 1;
    ST16(R7 + oAIAttackVars_StepTimer, R0);

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
