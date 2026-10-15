/*
 * npc_actions.c — overworld NPC script actions (bn6f npc.s): what an NPC
 * does each frame between script steps, chosen by CurAction from
 * NPCCurActionJumptable (npc_standard_809E5E2 in npc_script.c).
 *
 * Positions are 16.16 fixed point (X/Y/Z at +0x24/+0x28/+0x2C; the integer
 * halves X16/Y16/Z16 at +0x26/+0x2A/+0x2E). Walking moves one 8-pixel step
 * per "iteration": NextX/NextY advance by DeltaX/DeltaY each frame, and the
 * step ends exactly on Unk_70/Unk_74 (the step's target).
 *
 * These are reached only through jump tables, so their lifted contracts
 * list every register and flag as an output; each function leaves what the
 * original leaves (lines marked "as the original").
 */
#include "game.h"
#include "runtime.h"

enum {  /* bn6f include/structs/OverworldNPCObject.inc, not in the shared header */
    NPC_MOVEMENT_SPEED = 0x04,       /* walking: pixels/frame * 16; vertical/diagonal moves: frames left */
    NPC_MOVEMENT_DISTANCE = 0x05,    /* walking: steps left; vertical/diagonal moves: 0 up, else down */
    NPC_MOVE_PARAM_06 = 0x06,        /* vertical: Z step per frame; diagonal: axis/direction index */
    NPC_MOVE_PARAM_07 = 0x07,        /* diagonal: horizontal speed */
    NPC_X16 = 0x26, NPC_Y16 = 0x2A,
};
#define NPC_FLAGS_60_KEEP_Z    0x200 /* OW_NPC_UNK_FLAGS_60_0x20 << 4: do not follow the ground height */
#define NPC_STEP_TABLE         0x0809E6F0u  /* jt_809E6F0: walk init, walk update, vertical, diagonal */
#define DIAGONAL_AXES          0x0809E8C4u  /* byte_809E8C4: {field offset, 0 add / 1 subtract} x 4 */

/* Action 0: wait until the timer runs out, then let the script continue. */
Next npc_waitTimer_809e6c8(void) {
    u32 timer = LD16(R5 + oOverworldNPCObject_Timer);
    R0 = timer - 1;
    SET_FLAGS_CMP(timer, 1);              /* as the original (sub r0, #1) */
    ST16(R5 + oOverworldNPCObject_Timer, R0);
    if (R0 == 0)
        GUEST_CALL(npc_enableScript0x19_809f516);
    GUEST_CALL(npc_runSecondaryScriptMaybe_809ebf8);
    RETURN_TO_CALLER();
}

/* Action 1: the current movement step (MovementFlag_0a picks it), then the
 * secondary script. */
Next npc_doNonCutsceneMovement_809E6DC(void) {
    R0 = LD8(R5 + oOverworldNPCObject_MovementFlag_0a);
    R7 = LD32(NPC_STEP_TABLE + R0);
    GUEST_CALL_ADDR(R7);
    GUEST_CALL(npc_runSecondaryScriptMaybe_809ebf8);
    RETURN_TO_CALLER();
}

/* Start one 8-pixel walking step in MovementDirection (0-7; the odd ones
 * move): the step's target, how many frames it takes at MovementSpeed, and
 * the per-frame deltas. Then walking continues with the update (4). */
Next npc_nonCutsceneMovementInit_809E704(void) {
    u32 npc = R5;
    u32 dir = LD8(npc + oOverworldNPCObject_MovementDirection);
    u32 step = A_NPCMovementIterationDeltas + dir * 4;        /* {s16 dx, s16 dy} in pixels */
    u32 x = LD32(npc + oOverworldNPCObject_X), y = LD32(npc + oOverworldNPCObject_Y);
    ST32(npc + oOverworldNPCObject_NextYBackup, x + (LDS16(step) << 16));
    ST32(npc + oOverworldNPCObject_Unk_74, y + (LDS16(step + 2) << 16));

    /* frames = ceil(0x80000 / speed / 0x1000), through the BIOS divide */
    u32 speed = LD8(npc + NPC_MOVEMENT_SPEED);
    R0 = 0x80000; R1 = speed; R3 = 2;     /* as the original (r3 left by the y load) */
    GUEST_CALL(SWI_Div);
    R5 = npc;
    R0 = (R0 + 0xFFF) >> 12;
    ST16(npc + oOverworldNPCObject_Timer, R0);

    u32 sub = A_NPCMovementSubiterationDeltas + dir * 2;      /* {s8 dx, s8 dy}: -1, 0 or 1 */
    u32 dx = (LDS8(sub) * speed) << 12, dy = (LDS8(sub + 1) * speed) << 12;   /* u32: mul/lsl as the original */
    ST32(npc + oOverworldNPCObject_DeltaX, dx);
    ST32(npc + oOverworldNPCObject_DeltaY, dy);
    ST32(npc + oOverworldNPCObject_DeltaZ, 0);
    ST8(npc + oOverworldNPCObject_MovementFlag_0a, 4);

    u32 z = LD32(npc + oOverworldNPCObject_Z);
    ST32(npc + oOverworldNPCObject_NextX, x + dx);
    ST32(npc + oOverworldNPCObject_NextY, y + dy);
    ST32(npc + oOverworldNPCObject_NextZ, z);

    R0 = z; R1 = 0; R3 = 1; R4 = speed; R7 = sub;   /* as the original */
    SET_FLAGS_ADD(z, 0);
    GUEST_CALL(sub_809F5B0);
    RETURN_TO_CALLER();
}

/* Walking, every frame: unless blocked (npc_overworld_CheckStateFlags50000_809F638, skipped for NPCs that
 * do not collide), move to Next*; when the step's frames run out, land
 * exactly on the target and start the next step, or end the walk. */
Next npc_nonCutsceneMovementUpdate_809E7D8(void) {
    u32 npc = R5;
    u32 flags60 = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    R0 = flags60;
    R1 = OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION_0x10;
    if (!(flags60 & OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION_0x10)) {
        GUEST_CALL(npc_overworld_CheckStateFlags50000_809F638);
        if (R0) {                         /* blocked this frame */
            SET_FLAGS_NZ(R0);
            GUEST_CALL(sub_809F5B0);
            RETURN_TO_CALLER();
        }
    }
    ST32(npc + oOverworldNPCObject_X, LD32(npc + oOverworldNPCObject_NextX));
    ST32(npc + oOverworldNPCObject_Y, LD32(npc + oOverworldNPCObject_NextY));
    ST32(npc + oOverworldNPCObject_Z, LD32(npc + oOverworldNPCObject_NextZ));

    u32 timer = LD16(npc + oOverworldNPCObject_Timer);
    R0 = timer - 1;
    SET_FLAGS_CMP(timer, 1);              /* as the original (sub r0, #1) */
    ST16(npc + oOverworldNPCObject_Timer, R0);
    if (R0 == 0) {                        /* step done: snap to its target */
        ST32(npc + oOverworldNPCObject_X, LD32(npc + oOverworldNPCObject_NextYBackup));
        ST32(npc + oOverworldNPCObject_Y, LD32(npc + oOverworldNPCObject_Unk_74));
        u32 steps = LD8(npc + NPC_MOVEMENT_DISTANCE);
        R0 = steps - 1;
        SET_FLAGS_CMP(steps, 1);          /* as the original */
        ST8(npc + NPC_MOVEMENT_DISTANCE, R0);
        if (R0 == 0)
            GUEST_CALL(npc_enableScript0x19_809f516);     /* walk finished */
        else
            GUEST_CALL(npc_nonCutsceneMovementInit_809E704);
        RETURN_TO_CALLER();
    }

    u32 dx = LD32(npc + oOverworldNPCObject_DeltaX), dy = LD32(npc + oOverworldNPCObject_DeltaY);
    u32 nx = LD32(npc + oOverworldNPCObject_NextX), ny = LD32(npc + oOverworldNPCObject_NextY);
    ST32(npc + oOverworldNPCObject_NextX, nx + dx);
    ST32(npc + oOverworldNPCObject_NextY, ny + dy);
    SET_FLAGS_ADD(ny, dy);                /* as the original (add r0, r0, r1) */

    flags60 = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    R0 = flags60;
    R1 = NPC_FLAGS_60_KEEP_Z;
    cpu.c = 0;                            /* lsl r1, r1, #4 */
    SET_FLAGS_NZ(flags60 & NPC_FLAGS_60_KEEP_Z);
    if (!(flags60 & NPC_FLAGS_60_KEEP_Z)) {   /* follow the ground height */
        R0 = npc + oOverworldNPCObject_Coords;
        SET_FLAGS_ADD(oOverworldNPCObject_Coords, npc);
        GUEST_CALL(checkZCoordModifiers_8031612);
        u32 ground = R0;
        R0 = ground << 16;
        SET_FLAGS_NZ(R0);
        cpu.c = (ground >> 16) & 1;       /* lsl r0, r0, #16 */
        ST32(npc + oOverworldNPCObject_Z, R0);
        ST32(npc + oOverworldNPCObject_NextZ, R0);
    }
    GUEST_CALL(sub_809F5B0);
    RETURN_TO_CALLER();
}

/* Vertical move: for MovementSpeed frames, raise (MovementDistance = 0) or
 * lower Z16 by NPC_MOVE_PARAM_06 per frame; then let the script continue. */
Next npc_doVerticalMovement_809e84e(void) {
    u32 npc = R5;
    u32 frames = LD8(npc + NPC_MOVEMENT_SPEED);
    R0 = frames;
    R1 = LD8(npc + NPC_MOVEMENT_DISTANCE);
    R2 = LD8(npc + NPC_MOVE_PARAM_06);
    if (frames == 0) {
        SET_FLAGS_NZ(0);
        GUEST_CALL(npc_enableScript0x19_809f516);
        RETURN_TO_CALLER();
    }
    R0 = frames - 1;
    ST8(npc + NPC_MOVEMENT_SPEED, R0);
    u32 z = LD16(npc + oOverworldNPCObject_Z16);
    if (R1 == 0) {
        R1 = z + R2;
        SET_FLAGS_ADD(z, R2);
    } else {
        R1 = z - R2;
        SET_FLAGS_CMP(z, R2);
    }
    ST16(npc + oOverworldNPCObject_Z16, R1);
    RETURN_TO_CALLER();
}

/* Diagonal move (byte_809E8C4 picks the axis and its direction from
 * NPC_MOVE_PARAM_06): for MovementSpeed frames, move X16 or Y16 by
 * NPC_MOVE_PARAM_07 and Z16 by 1 (up if MovementDistance = 0); then let the
 * script continue. */
Next npc_doDiagonalMovement_809E878(void) {
    u32 npc = R5;
    u32 frames = LD8(npc + NPC_MOVEMENT_SPEED);
    if (frames == 0) {
        R0 = frames;
        SET_FLAGS_NZ(0);
        GUEST_CALL(npc_enableScript0x19_809f516);
        RETURN_TO_CALLER();
    }
    ST8(npc + NPC_MOVEMENT_SPEED, frames - 1);
    u32 axis = DIAGONAL_AXES + LD8(npc + NPC_MOVE_PARAM_06) * 2;
    u32 field = LD8(axis), subtract = LD8(axis + 1), speed = LD8(npc + NPC_MOVE_PARAM_07);
    u32 pos = LD16(npc + field);
    ST16(npc + field, subtract ? pos - speed : pos + speed);

    u32 z = LD16(npc + oOverworldNPCObject_Z16);
    if (LD8(npc + NPC_MOVEMENT_DISTANCE) == 0) {
        R1 = z + 1;
        SET_FLAGS_ADD(z, 1);
    } else {
        R1 = z - 1;
        SET_FLAGS_CMP(z, 1);
    }
    ST16(npc + oOverworldNPCObject_Z16, R1);
    R0 = speed; R2 = field;               /* as the original */
    RETURN_TO_CALLER();
}
