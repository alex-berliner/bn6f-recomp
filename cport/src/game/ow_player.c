/*
 * ow_player.c: readable functions of bn6f ow_player.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── ow_player_setCounterTalkStateAfterNpcInteract_809D7C8 ── */
/* ow_player_setCounterTalkStateAfterNpcInteract_809D7C8: the player object (r5) just finished interacting with an NPC.
 * Read the NPC's movement flag; if it is 4, store 8 into the player's
 * object field Unk_0a (marks the "talking to counter NPC" state).
 * r0 ends up holding the movement flag (or 8 when the store happens),
 * r7 ends up holding the NPC object pointer. Flags: CMP result, except
 * that in the store path the final "mov r0,#8" refreshes only N/Z while
 * C/V keep the values from the CMP. */
Next ow_player_setCounterTalkStateAfterNpcInteract_809D7C8(void) {
    R7 = LD32(R5 + oOWPlayerObject_interactedNPCObject_58);
    R0 = LD8(R7 + oOverworldNPCObject_MovementFlag_0a);

    SET_FLAGS_CMP(R0, 4);
    if (R0 == 4) {
        R0 = 8;
        ST8(R5 + oOWPlayerObject_Unk_0a, R0);
        /* mov r0,#8 only updates N/Z; C/V stay as the CMP left them */
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── overworldObj_testStateBit10_809DAF8 ── */
/* overworldObj_testStateBit10_809DAF8(r5 = overworld object):
 * Tests the object's state flags at +0x4C for bit 4 (0x10).
 * Returns: r0 = flag value (0 or 0x10), r1 = 0x10, r3 = 0x4C. Flags reflect r0. */
Next overworldObj_testStateBit10_809DAF8(void) {
    R3 = 0x4C;
    R0 = LD16(R5 + R3);
    R1 = 0x10;
    R0 &= R1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── event_TestPlayerAdvanceForward_809DB50 ── */
/* event_TestPlayerAdvanceForward_809DB50: check the queued "player advance forward" event request.
 *
 * Asks TestEventFlagFromImmediate about EVENT_1717_PLAYER_ADVANCE_FORWARD
 * (flag word 0x17, bit 0x17, set up by the movflag pseudo-op). The helper
 * answers through the condition flags, and this function passes those flags
 * straight through to its caller. If the request is still pending, everything
 * is left exactly as the helper left it; otherwise R0 is zeroed ("movs r0,#0")
 * so the caller reads a clean "no". R0 is scratch here, so zeroing it only
 * matters for the caller's view of the result. R3 passes through untouched. */
Next event_TestPlayerAdvanceForward_809DB50(void) {
    /* movflag EVENT_1717_PLAYER_ADVANCE_FORWARD: flag word 0x17, bit 0x17 */
    R0 = 0x17;
    R1 = 0x17;

    GUEST_CALL(TestEventFlagFromImmediate);

    /* bne locret: with the request still pending (Z clear), keep the
     * helper's result as-is; otherwise clear R0, which holds the flag's
     * whole byte and can be nonzero with this flag clear. */
    if (cpu.nz == 0) {
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── owPlayer_TickCountdownOrWait_809DDCC ── */
/* owPlayer_TickCountdownOrWait_809DDCC(r5 = overworld player object):
 * Tick the object's Unk_18 countdown timer. While it is still counting down,
 * decrement it and return 1. Once it hits zero, only fail (return 0) while
 * one of the 0xC00000 status bits of Unk_50 is set; otherwise return 1.
 * r0 = result. r2 = 0xC00000 on the status-check path (untouched otherwise).
 * c/v come from the countdown SUBS on the timer path, and are left as the
 * caller set them on the status-check path (TST only updates nz). */
Next owPlayer_TickCountdownOrWait_809DDCC(void) {
    u32 countdown = LD16(R5 + oOWPlayerObject_Unk_18);

    if (countdown != 0) {
        /* Timer still running: tick it down and report done. */
        SET_FLAGS_CMP(countdown, 1); /* from SUBS r1, #1 */
        ST16(R5 + oOWPlayerObject_Unk_18, countdown - 1);
        R0 = 1;
    } else {
        /* Timer expired: check the blocking status bits in Unk_50. */
        u32 status = LD32(R5 + oOWPlayerObject_Unk_50);
        u32 blocking = status & 0xC00000;
        SET_FLAGS_NZ(blocking); /* from TST r2, r1 */
        R2 = 0xC00000;
        R0 = (blocking != 0) ? 0 : 1;
    }
    RETURN_TO_CALLER();
}

/* ── ow_tryTileInteraction_809DF00 ── */
/* ow_tryTileInteraction_809DF00(r5 = overworld player object, r10 = toolkit):
 * Called when the player tries to interact with the tile in front of them.
 * ow_findInteractableNPC_809E01C checks whether tile interaction is currently blocked (e.g. by
 * a cutscene); if so, the game state byte oGameState_Unk_10 is set to 1 and
 * the function returns 1. Otherwise the tile's interaction value
 * (oOWPlayerObject_Unk_0f) is copied into oGameState_CoordInteractionValue,
 * and if it is nonzero it is also latched into game state offset 0x03, then
 * the function returns 0. R7 is left pointing at the game state struct; NZ
 * reflect the return value. */
Next ow_tryTileInteraction_809DF00(void) {
    GUEST_CALL(ow_findInteractableNPC_809E01C);

    if (R0 == 0) {
        /* Interaction is blocked: flag the state and report "blocked". */
        R7 = LD32(R10 + oToolkit_GameStatePtr);
        ST8(R7 + oGameState_Unk_10, 1);
        R0 = 1;
    } else {
        /* Interaction allowed: forward the tile's interaction value. */
        u32 interactionValue = LD8(R5 + oOWPlayerObject_Unk_0f);
        R7 = LD32(R10 + oToolkit_GameStatePtr);
        ST8(R7 + oGameState_CoordInteractionValue, interactionValue);
        if (interactionValue != 0)
            ST8(R7 + 3, interactionValue);
        R0 = 0;
    }

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── owPlayer_isMoving_809DFA0 ── */
/* owPlayer_isMoving_809DFA0(r5 = overworld player object):
 * Reports whether the player object is still in motion. The player counts as
 * moving as long as any of its current X/Y/Z coordinates differs from the
 * corresponding "next" coordinate it is heading toward.
 *
 * Returns r0 = 1 if some axis still differs, r0 = 0 if the player is
 * stationary. r1/r2 are left holding the last pair of coordinates that was
 * compared, and the condition flags are exactly what the final CMP of that
 * pair left (callers branch on them right after the call). */

/* Compare one axis of the player object: load cur/next, leave them in r1/r2,
 * set the flags as the original's CMP does, and return whether they differ.
 * The caller short-circuits, so only the axes actually compared by the
 * original touch the flags. */
static s32 player_axis_moved(u32 obj, u32 curOff, u32 nextOff) {
    u32 cur = LD32(obj + curOff);
    u32 next = LD32(obj + nextOff);
    R1 = cur;
    R2 = next;
    SET_FLAGS_CMP(cur, next);
    return cur != next;
}

Next owPlayer_isMoving_809DFA0(void) {
    R0 = player_axis_moved(R5, oOWPlayerObject_X, oOWPlayerObject_NextX)
      || player_axis_moved(R5, oOWPlayerObject_Y, oOWPlayerObject_NextY)
      || player_axis_moved(R5, oOWPlayerObject_Z, oOWPlayerObject_NextZ);

    RETURN_TO_CALLER();
}

/* ── owPlayer_setJumptableIndex09AndOthers_809e004 ── */
/* owPlayer_setJumptableIndex09AndOthers_809e004(r0 = value, r5 = overworld player object):
 * Clears the player's Unk_0a byte, then stores r0 either into Unk_0b (when the
 * current jumptable index is 0x10) or into the jumptable index itself (offset 9).
 * Leaves r1 = the previous jumptable index; nz/c/v come from comparing it with 0x10. */
Next owPlayer_setJumptableIndex09AndOthers_809e004(void) {
    u32 value = R0;
    u32 player = R5;

    ST8(player + oOWPlayerObject_Unk_0a, 0);

    u32 jumptableIndex = LD8(player + 9);
    SET_FLAGS_CMP(jumptableIndex, 0x10);

    if (jumptableIndex == 0x10)
        ST8(player + oOWPlayerObject_Unk_0b, value);
    else
        ST8(player + 9, value);

    R1 = jumptableIndex;
    RETURN_TO_CALLER();
}

/* ── ow_findInteractableNPC_809E01C ── */
/* ow_findInteractableNPC_809E01C(r5 = player object):
 * Walk the overworld NPC object array looking for the first NPC the player can
 * interact with (slot in use, interaction not disabled) and record it in the
 * player's interactedNPCObject slot.
 * Returns r0 = 0 when an NPC was found; if the first live NPC has interaction
 * disabled, r0 is that NPC's flags word and nothing is stored; r0 = 1 when no
 * live NPC exists.
 * r1 = 1 and r2 = one-past-end of the NPC array on every exit. */
Next ow_findInteractableNPC_809E01C(void) {
    u32 player = R5;
    u32 npc = A_eOverworldNPCObject0_Flags; /* == eOverworldNPCObjects */
    u32 arrayEnd = npc + 0xD80; /* dword_809E048: byte size of the NPC array */

    SET_FLAGS_ADD(0xD80, npc); /* add r2, r2, r7 */
    R1 = 1;
    R2 = arrayEnd;

    while ((s32)npc < (s32)arrayEnd) {
        u32 state = LD32(npc + oOverworldNPCObject_Unk_50);
        if ((state & 1) != 0) {
            /* NPC slot is live: skip it if interaction is disabled. */
            u32 unkFlags = LD32(npc + oOverworldNPCObject_UnkFlags_60);
            SET_FLAGS_NZ(unkFlags & 1); /* OW_NPC_UNK_FLAGS_60 bit 0 */
            if ((unkFlags & 1) == 0) {
                ST32(player + oOWPlayerObject_interactedNPCObject_58, npc);
                R0 = 0;
                RETURN_TO_CALLER();
            }
            R0 = unkFlags;
            RETURN_TO_CALLER();
        }
        npc += oOverworldNPCObject_Size;
        SET_FLAGS_CMP(npc, arrayEnd);
    }

    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── owPlayer_ClearStateBytes_D_E_F_809E04C ── */
/* owPlayer_ClearStateBytes_D_E_F_809E04C:
 * Clear three state bytes (0xD..0xF) of the overworld player object.
 * The caller's r0/r1 are passed through untouched. */
Next owPlayer_ClearStateBytes_D_E_F_809E04C(void) {
    u32 player = A_eOWPlayerObject;

    ST8(player + 0xD, 0); /* byte_2009F4D */
    ST8(player + 0xE, 0); /* byte_2009F4E */
    ST8(player + 0xF, 0); /* byte_2009F4F */

    RETURN_TO_CALLER();
}

/* ── spawnOWPlayerObjectForEnterMap_809e064 ── */
/* spawnOWPlayerObjectForEnterMap_809e064:
 * Spawn the overworld player object when entering a map, then hook it into
 * the game state: record it as the current overworld player object and copy
 * the player's saved X/Y/Z position and facing direction into it.
 *
 * Inputs: r1-r3, r6, r7 are passed through to SpawnOWPlayerObject;
 *         r10 = toolkit pointer.
 * Outputs: r5 = spawned object pointer (0 if none spawned).
 * r2, r3, r4, r6, r7 are preserved: the original pushes r4/r6/r7 and pops
 * them on return, so the scratch game-state pointer it keeps in r6 never
 * escapes to the caller. */
Next spawnOWPlayerObjectForEnterMap_809e064(void) {
    /* Mirror the original's push {r4,r6,r7,lr}: these three registers are
     * restored to their entry values no matter what the spawner did. */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    R0 = 0; /* SpawnOWPlayerObject: create a fresh object */
    R4 = 0;
    GUEST_CALL(SpawnOWPlayerObject);

    u32 player = R5;
    if (player != 0) {
        /* Keep the game-state pointer in a local: the original holds it in
         * r6 only transiently and pops the caller's value back over it. */
        u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

        ST32(gameState + oGameState_OverworldPlayerObjectPtr, player);
        ST32(player + oOWPlayerObject_X, LD32(gameState + oGameState_PlayerX));
        ST32(player + oOWPlayerObject_Y, LD32(gameState + oGameState_PlayerY));
        ST32(player + oOWPlayerObject_Z, LD32(gameState + oGameState_PlayerZ));
        ST8(player + oOWPlayerObject_FacingDirection,
            LD32(gameState + oGameState_FacingDirectionAfterWarp));
    }

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── SetPlayerCanMoveEventFlag ── */
/* SetPlayerCanMoveEventFlag: mark the on-map player as free to move again
 * by setting the "player can move" event flag (flag id 0x17, value 0x14).
 * Inputs: none. Outputs: none; clobbers r0, r1, r3, LR, flags. */
Next SetPlayerCanMoveEventFlag(void) {
    /* SetEventFlagFromImmediate(r0 = flag id, r1 = value to store) */
    R0 = 0x17; /* EVENT_PLAYER_CAN_MOVE */
    R1 = 0x14;
    GUEST_CALL(SetEventFlagFromImmediate);
    RETURN_TO_CALLER();
}

/* ── ClearPlayerCanMoveEventFlag ── */
/* ClearPlayerCanMoveEventFlag: clear the "player can move" event flag
 * (flag word 0x17, bit 0x14 of the event flag work area at 0x2001c88,
 * i.e. the bit at 0x2001f6a). Delegates to the generic flag-clearing
 * helper, which leaves r0/r1/r3 and the nz/c/v flags as its callers
 * expect. */
Next ClearPlayerCanMoveEventFlag(void) {
    R0 = 0x17; /* EVENT_PLAYER_CAN_MOVE: flag word index */
    R1 = 0x14; /* bit index within that word */

    GUEST_CALL(ClearEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── owPlayer_lockPlayerForNonNPCDialogue_809E0B0 ── */
/* owPlayer_lockPlayerForNonNPCDialogue_809E0B0:
 * Lock the player in place for dialogue with non-NPC objects (pressing L,
 * jacking in, talking to objects): sets the "player is busy" byte, marks
 * event EVENT_1718 (which also relocates the map position on jacking in),
 * and clears the player-can-move event flag.
 * Leaves r0/r1/r3 and the flags as SetEventFlagFromImmediate and
 * ClearPlayerCanMoveEventFlag leave them; returns to the caller's LR. */
Next owPlayer_lockPlayerForNonNPCDialogue_809E0B0(void) {
    /* mark the player as locked into dialogue */
    R3 = A_eStruct200ace0;
    ST8(R3, 1);

    /* set event flag EVENT_1718 (bank 0x17, index 0x18) */
    R0 = 0x17;
    R1 = 0x18;
    GUEST_CALL(SetEventFlagFromImmediate);

    /* and forbid free movement until the dialogue releases the player */
    GUEST_CALL(ClearPlayerCanMoveEventFlag);

    RETURN_TO_CALLER();
}

/* ── owPlayer_unlockPlayerAfterNonNPCDialogue_809E122 ── */
/* owPlayer_unlockPlayerAfterNonNPCDialogue_809E122:
 * Called after talking to an overworld character that is not an NPC-scripted
 * interaction: unlock the player again. Clears the "dialogue in progress"
 * byte of eStruct200ace0, lets the player move again via the event-flag
 * system, and clears story event flag 0x1719. */
#define EVENT_1719 0x1719

Next owPlayer_unlockPlayerAfterNonNPCDialogue_809E122(void) {
    /* Clear the dialogue-busy byte at eStruct200ace0. */
    ST8(A_eStruct200ace0, 0);

    /* Re-enable player movement. */
    GUEST_CALL(SetPlayerCanMoveEventFlag);

    /* Clear event flag 0x1719 (ClearEventFlagFromImmediate takes the flag
     * number split into a high byte in r0 and a low byte in r1). */
    R0 = EVENT_1719 >> 8;
    R1 = EVENT_1719 & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── owPlayer_setS200ace0_fixedAnimationSelect_809e13c ── */
/* owPlayer_setS200ace0_fixedAnimationSelect_809e13c(r0 = animation id):
 * Store the fixed overworld player animation id into eStruct200ace0+2 and set
 * event flag EVENT_1719 (passed to SetEventFlagFromImmediate as group 0x17,
 * flag 0x19). Other registers and flags come back from that call untouched. */
Next owPlayer_setS200ace0_fixedAnimationSelect_809e13c(void) {
    /* ldr r3, =eStruct200ace0; strb r0, [r3, #2] (field offset 2) */
    ST8(A_eStruct200ace0 + 2, R0);

    /* movflag EVENT_1719; movs r1, #25 */
    R0 = 0x17;
    R1 = 0x19;
    GUEST_CALL(SetEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── owPlayer_RequestFixedAnimation_809E14C ── */
/* owPlayer_RequestFixedAnimation_809E14C(r0 = fixed-animation id):
 * Called when the overworld player is told to play a fixed animation.
 * sub_809E462 decides whether that animation can take effect immediately:
 *   - if it returns 0 (Z set), the id is written to the fixed-animation
 *     select byte at eStruct200ace0+2 (byte_200ACE2) and the event flag
 *     EVENT_1719 (bank 0x17, flag 0x19) is raised;
 *   - otherwise the request is deferred by parking the id in the byte at
 *     eStruct200ace0+0x1c (byte_200ACFC).
 * r4-r7 are preserved (the id is passed to sub_809E462 in r4/r7's struct
 * pointer, but both are restored before returning). */
Next owPlayer_RequestFixedAnimation_809E14C(void) {
    u32 anim = R0;
    u32 saved_r4 = R4;
    u32 saved_r7 = R7;

    R4 = anim;                  /* mov r4, r0: sub_809E462 reads r4 */
    R7 = A_eStruct200ace0;      /* ldr r7, =eStruct200ace0 */

    /* sub_809E462 reports in Z whether the animation can be applied now. */
    GUEST_CALL(sub_809E462); /* answers in Z; the original branches on it */

    if (ZERO_FLAG()) {
        /* Approved: select the animation right away and flag the event. */
        ST8(A_eStruct200ace0 + 2, R4);  /* owPlayerFixedAnimationSelect */

        /* movflag EVENT_1719 -> SetEventFlagFromImmediate(0x17, 0x19) */
        R0 = 0x17;
        R1 = 0x19;
        GUEST_CALL(SetEventFlagFromImmediate);
    } else {
        /* Denied: remember the requested animation for later. */
        ST8(A_eStruct200ace0 + oS200ace0_Unk_1c, R4);
    }

    /* pop {r4-r7,pc} */
    R4 = saved_r4;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188 ── */
/* owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188
 *   (r0 = new X, r1 = new Y, r2 = new Z):
 * Shifts the overworld player's current position into its "next" position
 * slots, then writes the given coordinates as the player's new position.
 * Finally mirrors the new position into the shared overworld-player state
 * (s200ace0) via the indirect setter, as a tail call.
 * On return, r3 holds the s200ace0 base address (left by the tail call);
 * r0-r2 and r7 are unchanged. */
Next owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188(void) {
    u32 player = LD32(LD32(R10 + oToolkit_GameStatePtr) + oGameState_OverworldPlayerObjectPtr);

    ST32(player + oOWPlayerObject_NextX, LD32(player + oOWPlayerObject_X));
    ST32(player + oOWPlayerObject_NextY, LD32(player + oOWPlayerObject_Y));
    ST32(player + oOWPlayerObject_NextZ, LD32(player + oOWPlayerObject_Z));

    ST32(player + oOWPlayerObject_X, R0);
    ST32(player + oOWPlayerObject_Y, R1);
    ST32(player + oOWPlayerObject_Z, R2);

    /* Tail call: owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4(r0 = X, r1 = Y, r2 = Z) */
    GUEST_CALL(owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4);
    RETURN_TO_CALLER();
}

/* ── owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4 ── */
/* owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4(r0 = x, r1 = y, r2 = z):
 * Store the player's overworld coordinates into the eStruct200ace0 struct
 * (PlayerX has no named offset macro, the disassembly uses the raw 0x8).
 * r3 is left pointing at that struct. */
Next owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4(void) {
    R3 = A_eStruct200ace0;
    ST32(R3 + 8, R0); /* oS200ace0_PlayerX */
    ST32(R3 + oS200ace0_PlayerY, R1);
    ST32(R3 + oS200ace0_PlayerZ, R2);
    RETURN_TO_CALLER();
}

/* ── ReadOWPlayerObjectCoords ── */
/* ReadOWPlayerObjectCoords: read the overworld player object's position.
 * Inputs:  r10 = toolkit pointer.
 * Outputs: r0 = X, r1 = Y, r2 = Z, r3 = pointer to the OW player object. */
Next ReadOWPlayerObjectCoords(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = LD32(player + oOWPlayerObject_X);
    R1 = LD32(player + oOWPlayerObject_Y);
    R2 = LD32(player + oOWPlayerObject_Z);
    R3 = player;
    RETURN_TO_CALLER();
}

/* ── owPlayer_offsetS200ace0Coords_809e1fa ── */
/* owPlayer_offsetS200ace0Coords_809e1fa(r0 = dx, r1 = dy, r2 = dz):
 * offset the player's overworld position by the given delta. The struct
 * pointer is reloaded from the literal pool into a scratch register; the
 * caller's r7 is saved on entry and restored on exit, so it is preserved.
 * Flags come from the final add (the Z update), which also leaves the new
 * Z in r3. */

Next owPlayer_offsetS200ace0Coords_809e1fa(void) {
    u32 const player = A_eStruct200ace0;

    u32 x = LD32(player + oS200ace0_PlayerX);
    x += R0;
    ST32(player + oS200ace0_PlayerX, x);

    u32 y = LD32(player + oS200ace0_PlayerY);
    y += R1;
    ST32(player + oS200ace0_PlayerY, y);

    u32 oldZ = LD32(player + oS200ace0_PlayerZ);
    SET_FLAGS_ADD(oldZ, R2);
    u32 z = oldZ + R2;
    ST32(player + oS200ace0_PlayerZ, z);

    R3 = z;
    RETURN_TO_CALLER();
}

/* ── returnZero_809E228 ── */
/* returnZero_809E228: always returns 0 (likely a stub/callback). */
Next returnZero_809E228(void) {
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── owPlayer_setInteractionLocked_809e230 ── */
/* owPlayer_setInteractionLocked_809e230:
 * Marks the overworld player object as interaction-locked (e.g. while a
 * cutscene/dialog is running). r10 = toolkit pointer.
 * Outputs: r0 = 1, r3 = overworld player object, NZ set from r0 = 1. */
Next owPlayer_setInteractionLocked_809e230(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    ST8(player + oOWPlayerObject_InteractionLocked, R0);
    R3 = player;

    RETURN_TO_CALLER();
}

/* ── owPlayer_clearInteractionLocked_809e23c ── */
/* owPlayer_clearInteractionLocked_809e23c:
 * Clear the "currently interacting with something" flag on the overworld
 * player object, so the player can start a new interaction afterwards.
 * Inputs:  r10 = toolkit pointer.
 * Outputs: r0 = 0, r3 = overworld player object pointer, nz from r0.
 */
Next owPlayer_clearInteractionLocked_809e23c(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = 0;
    R3 = player;
    SET_FLAGS_NZ(R0);
    ST8(player + oOWPlayerObject_InteractionLocked, 0);

    RETURN_TO_CALLER();
}

/* ── owPlayer_enableWallCollision_809e248 ── */
/* owPlayer_enableWallCollision_809e248:
 * Turn wall collision back on for the overworld player object.
 * R10 is the toolkit pointer; returns R0 = 1 and the player object
 * pointer in R3, with flags set from R0 (the original's Thumb
 * "mov r0, #1" is an LSLS that sets NZ). */
Next owPlayer_enableWallCollision_809e248(void) {
    u32 player = LD32(LD32(R10 + oToolkit_GameStatePtr) + oGameState_OverworldPlayerObjectPtr);

    R0 = 1;
    ST8(player + oOWPlayerObject_wallCollision_0c, 1);
    R3 = player;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── owPlayer_disableWallCollision_809e254 ── */
/* owPlayer_disableWallCollision_809e254: clear the overworld player object's
 * wall-collision flag so the player can walk through walls.
 * outputs: r0 = 0 (FALSE), r3 = player object pointer, nz set from r0. */
Next owPlayer_disableWallCollision_809e254(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = 0; /* FALSE */
    R3 = player;
    SET_FLAGS_NZ(R0);
    ST8(R3 + oOWPlayerObject_wallCollision_0c, R0);

    RETURN_TO_CALLER();
}

/* ── SetOWPlayerFacingDirection ── */
/* SetOWPlayerFacingDirection(r0 = direction):
 * Store the given facing direction into the overworld player object.
 * r10 = toolkit pointer; also returns the player object pointer in r3. */
Next SetOWPlayerFacingDirection(void) {
    u32 *gameState = (u32 *)LD32(R10 + oToolkit_GameStatePtr);
    u32 *player = (u32 *)LD32((u32)gameState + oGameState_OverworldPlayerObjectPtr);
    ST8((u32)player + oOWPlayerObject_FacingDirection, (u8)R0);
    R3 = (u32)player;
    RETURN_TO_CALLER();
}

/* ── GetOWPlayerFacingDirection ── */
/* GetOWPlayerFacingDirection: return the direction the overworld player sprite
 * is currently facing (0..3), read from the player's overworld object. */
Next GetOWPlayerFacingDirection(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 playerObject = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = LD8(playerObject + oOWPlayerObject_FacingDirection);
    RETURN_TO_CALLER();
}

/* ── owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2 ── */

/* owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2:
 * reset the overworld player navi's sprite transform to its defaults
 * (horizontal compress 0x40, vertical compress 0x40, rotation 0) and store
 * them into the eStruct2000aa0 fields. Tail-calls
 * owPlayer_setS2000aa0_param0x0to0x2_809e2c8, which loads the struct pointer
 * into r3 and writes the three parameter bytes. */
Next owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2(void) {
    R0 = DEFAULT_OW_PLAYER_NAVI_HORIZONTAL_COMPRESS;
    R1 = DEFAULT_OW_PLAYER_NAVI_VERTICAL_COMPRESS;
    R2 = DEFAULT_OW_PLAYER_NAVI_ROTATION;
    GUEST_CALL(owPlayer_setS2000aa0_param0x0to0x2_809e2c8);
    RETURN_TO_CALLER();
}

/* ── owPlayer_setS2000aa0_param0x0to0x2_809e2c8 ── */
/* owPlayer_setS2000aa0_param0x0to0x2_809e2c8(r0 = horizontal compress,
 * r1 = vertical compress, r2 = rotation): store the three navi sprite
 * transform settings into the persistent player state struct.
 * r3 = eStruct2000aa0 base address. */
Next owPlayer_setS2000aa0_param0x0to0x2_809e2c8(void) {
    u32 playerState = A_eStruct2000aa0;

    ST8(playerState + 0, R0);
    ST8(playerState + 1, R1);
    ST8(playerState + 2, R2);

    R3 = playerState;
    RETURN_TO_CALLER();
}

/* ── SetOWPlayerNaviColorShader ── */
/* SetOWPlayerNaviColorShader(r0 = shader id):
 * Store the overworld player navi color shader id into the
 * eStruct2000aa0 struct field word_2000AA6 (base + 6).
 * Leaves r3 = eStruct2000aa0 (as the original does for its callers). */
Next SetOWPlayerNaviColorShader(void) {
    R3 = A_eStruct2000aa0;
    ST16(R3 + 6, R0); /* word_2000AA6 - 0x2000aa0 == 6 */
    RETURN_TO_CALLER();
}

/* ── ZeroOWPlayerNaviPaletteIndex ── */
/* ZeroOWPlayerNaviPaletteIndex: 0809E2FC-0809E2FE THUMB, bn6f ow_player.s:2412
 * Zero the overworld player Navi's palette index (the byte at 0x2000AA5).
 * This entry point is tail-merged with SetOWPlayerNaviPaletteIndex, which
 * performs the same store using the index passed in r0; both leave the
 * pointer to the 0x2000AA0 struct in r3 (and r0 = 0 from the `mov r0, #0`). */
Next ZeroOWPlayerNaviPaletteIndex(void) {
    R3 = A_eStruct2000aa0;
    R0 = 0;
    ST8(R3 + 5 /* oS2000aa0_OWPlayerNaviPaletteIndex: byte_2000AA5 - 0x2000aa0 */, 0);
    RETURN_TO_CALLER();
}

/* ── SetOWPlayerNaviPaletteIndex ── */
/* SetOWPlayerNaviPaletteIndex(r0 = navi palette index):
 * stores the overworld player navi's palette index into the eStruct2000aa0
 * struct (field byte_2000AA5), leaving r3 pointing at that struct. */
Next SetOWPlayerNaviPaletteIndex(void) {
    R3 = A_eStruct2000aa0;
    ST8(R3 + 5 /* byte_2000AA5 - 0x2000aa0 */, R0);
    RETURN_TO_CALLER();
}

/* ── owPlayer_zeroS2000AA0Param0x4_809e312 ── */
/* owPlayer_zeroS2000AA0Param0x4_809e312: clear the player sprite's byte at
 * 0x2000AA4 (the S2000aa0 struct's Sprite_04 field).
 *
 * The original is a two-instruction pair:
 *   owPlayer_zeroS2000AA0Param0x4:  mov r0, #0   (sets Z, clears N)
 *   owPlayer_setS2000AA0Param0x4:   ldr r3, =eStruct2000aa0
 *                                   strb r0, [r3, #4]
 * The setter is tiny, so it is inlined here; the observable results are
 * r0 = 0, r3 = &eStruct2000aa0, one byte stored at 0x2000AA4, and the Z
 * flag left set by the `mov r0, #0`. */

/* Field offset from the disassembly: strb r0, [r3, #oS2000aa0_Sprite_04]
 * (byte_2000AA4 - 0x2000aa0). */

Next owPlayer_zeroS2000AA0Param0x4_809e312(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);               /* mov r0, #0 */

    R3 = A_eStruct2000aa0;
    ST8(A_eStruct2000aa0 + oS2000aa0_Sprite_04, R0);   /* strb r0, [r3, #4] */

    RETURN_TO_CALLER();
}

/* ── owPlayer_setS2000AA0Param0x4_809e314 ── */
/* owPlayer_setS2000AA0Param0x4_809e314(r0 = sprite id):
 * Store the overworld player's current sprite id into byte 4 of the
 * eStruct2000aa0 struct (byte_2000AA4). Leaves the struct's address in r3. */
Next owPlayer_setS2000AA0Param0x4_809e314(void) {
    R3 = A_eStruct2000aa0;
    ST8(R3 + 4, R0);
    RETURN_TO_CALLER();
}

/* ── sub_809E3A2 ── */
/* sub_809E3A2: test the flag byte at byte_2000AA8 (eStruct2000aa0 + 8).
 * Outputs: r3 = eStruct2000aa0 base address, Z flag set iff the byte is 0. */
Next sub_809E3A2(void) {
    R3 = A_eStruct2000aa0;
    R0 = LD8(A_eStruct2000aa0 + 8); /* byte_2000AA8 */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_809E3AA ── */
/* sub_809E3AA:
 * Clear the flag byte at byte_2000AA8 (offset 8 of eStruct2000aa0).
 * Leaves the struct's address in r3. */
Next sub_809E3AA(void) {
    R3 = A_eStruct2000aa0;
    ST8(R3 + 8, 0);
    RETURN_TO_CALLER();
}

/* ── resetStruct2000aa0State_809E3B2 ── */
/* resetStruct2000aa0State_809E3B2: reset the state struct at 0x2000AA0 — clear the byte flag
 * at +0x0B and the halfword counters at +0x0C, +0x10, +0x14 and +0x18. */
Next resetStruct2000aa0State_809E3B2(void) {
    u32 state = A_eStruct2000aa0;

    ST8(state + 0x0B, 0);
    ST16(state + 0x0C, 0);
    ST16(state + 0x10, 0);
    ST16(state + 0x14, 0);
    ST16(state + 0x18, 0);

    RETURN_TO_CALLER();
}

/* ── getOWPlayerSpriteFrameParameters_809E434 ── */
/* getOWPlayerSpriteFrameParameters_809E434:
 * Load the overworld player object from the current game state and return its
 * current sprite frame parameters (r0-r3, set by sprite_getFrameParameters).
 * r10 = toolkit, r5 is preserved. */
Next getOWPlayerSpriteFrameParameters_809E434(void) {
    u32 savedR5 = R5;

    /* toolkit->gameState->overworldPlayerObject */
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    R5 = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    GUEST_CALL(sprite_getFrameParameters);

    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── MakeOWPlayerVisible ── */
/* MakeOWPlayerVisible: set the "visible" bit (0x02) in the overworld player
 * object's flags so its sprite is rendered again.
 *   inputs:  R10 = toolkit pointer
 *   outputs: R0 = updated flags byte, R1 = OBJECT_FLAG_VISIBLE (2),
 *            R3 = player object pointer; nz flags reflect the updated flags.
 */
Next MakeOWPlayerVisible(void) {
    /* Flags byte lives at offset 0 of the player object. */
    u32 player = LD32(LD32(R10 + oToolkit_GameStatePtr)
                      + oGameState_OverworldPlayerObjectPtr);

    R0 = LD8(player);
    R1 = 2;                 /* OBJECT_FLAG_VISIBLE */
    R0 |= R1;
    SET_FLAGS_NZ(R0);
    ST8(player, R0);
    R3 = player;

    RETURN_TO_CALLER();
}

/* ── MakeOWPlayerInvisible ── */
/* MakeOWPlayerInvisible: hide the player's overworld sprite.
 * Follows the game-state pointer chain from the toolkit (r10), then clears
 * the "visible" bit (0x02) of the player object's attribute byte.
 * Outputs: r0 = new flag byte, r1 = 2, r3 = player object pointer;
 * nz flags set from the new flag byte. */
Next MakeOWPlayerInvisible(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    u32 flags = LD8(player);
    R1 = 2; /* OBJECT_FLAG_VISIBLE */
    flags &= ~R1;
    ST8(player, flags);

    R0 = flags;
    R3 = player;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── sub_809E462 ── */
/* sub_809E462:
 * Reads the flag byte at oGameState_Unk_10 from the current game state.
 * Returns r0 = that byte, r1 = game state pointer, and sets NZ flags on it
 * so callers can branch on whether the flag is zero. */
Next sub_809E462(void) {
    u32 *toolkit = (u32 *)R10;
    u32 gameState = LD32((u32)toolkit + oToolkit_GameStatePtr);

    R1 = gameState;
    R0 = LD8(gameState + oGameState_Unk_10);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── overworld_CheckPlayerAdvanceForwardTrigger_809E46E ── */
/* overworld_CheckPlayerAdvanceForwardTrigger_809E46E: overworld hook that checks the coordinate trigger at
 * player + 0x1C.  If it is one of the "player advances forward" trigger
 * tiles (0x48..0x4F), store the trigger index (0..7) into the player
 * object at + 0x64 and set the EVENT_1717 event flag. */
#define EVENT_1717_PLAYER_ADVANCE_FORWARD 0x1717u

Next overworld_CheckPlayerAdvanceForwardTrigger_809E46E(void) {
    u32 callerR7 = R7; /* r7 is used as scratch below but must be preserved */

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = player + 0x1C;
    R7 = gameState; /* guest state at the original bl */
    GUEST_CALL(checkCoordinateTrigger_8031a7a);
    s32 trigger = (s32)R0;

    if (trigger >= 0x48 && trigger <= 0x4F) {
        ST16(player + 0x64, (u16)(trigger - 0x48));

        /* SetEventFlagFromImmediate takes the event bank in r0 and the
         * flag index in r1 (movflag EVENT_1717_PLAYER_ADVANCE_FORWARD). */
        R0 = EVENT_1717_PLAYER_ADVANCE_FORWARD >> 8;  /* 0x17 */
        R1 = EVENT_1717_PLAYER_ADVANCE_FORWARD & 0xFF; /* 0x17 */
        GUEST_CALL(SetEventFlagFromImmediate);
    }

    R7 = callerR7;
    RETURN_TO_CALLER();
}

#undef EVENT_1717_PLAYER_ADVANCE_FORWARD
