/*
 * npc_commands.c — NPC script plumbing (bn6f npc.s): reading script words, the
 * jump and pause commands, enabling/disabling the script, running the
 * secondary script, waiting for a cutscene variable.
 *
 * Drafted by Hyper GLM-5.3 Flash with tools/convert/farm.py (accepted on 40
 * calls captured from the original on the recorded routes, and random
 * inputs), reviewed and integrated by Claude; checked with the host
 * difftest, the CPU oracle and lockstep. Evidence per function: symbols/db.
 */
#include "game.h"
#include "runtime.h"

enum { NPC_CUR_ACTION = 0x09, NPC_ACTION_STOP = 0 };   /* bn6f include/structs/OverworldNPCObject.inc */

/* ReadNPCScriptWord(r0 = script pointer): read a 32-bit little-endian word from
 * the NPC script stream. Returns the word in r0; r1..r3 also hold the shifted
 * byte values the original leaves behind, since callers may inspect them. */
Next ReadNPCScriptWord(void) {
    u32 addr = R0;

    u32 byte0 = LD8(addr + 0);
    u32 byte1 = LD8(addr + 1) << 8;
    u32 byte2 = LD8(addr + 2) << 16;
    u32 byte3 = LD8(addr + 3) << 24;

    R1 = byte0;
    R2 = byte1;
    R3 = byte2;
    R0 = byte3 | byte0 | byte1 | byte2;

    RETURN_TO_CALLER();
}

/* NPCCommand_jump (0809ED88): "jump" NPC script command.
 * Reads the 16-bit operand that follows the opcode (r6 points at the
 * opcode, so the operand is at r6+1) and resumes the script there.
 * The final MOV is really ADDS r6, r0, #0 in Thumb, so it also clears
 * the C and V flags and sets N/Z from the new script pointer. */
Next NPCCommand_jump(void) {
    R0 = R6 + 1;                 /* address of the jump target operand */
    GUEST_CALL(ReadNPCScriptWord);
    SET_FLAGS_ADD(R0, 0);        /* adds r6, r0, #0: N/Z from result, C/V = 0 */
    R6 = R0;                     /* resume the script at the target address */
    RETURN_TO_CALLER();
}


/* NPCCommand_pause(r5 = NPC object, r6 = script instruction pointer):
 * "pause" script command — halt the NPC (CurAction = NPC_ACTION_STOP,
 * clear its movement flags), load the pause length from the operand byte
 * into its Timer, and disable its script handler. Returns with the
 * instruction pointer advanced past the opcode + operand and flags from
 * that increment; r0 passes through from the script-disable call. */
Next NPCCommand_pause(void) {
    ST8(R5 + NPC_CUR_ACTION, NPC_ACTION_STOP);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    ST16(R5 + oOverworldNPCObject_Timer, LD8(R6 + 1));

    GUEST_CALL(npc_disableScript0x19_809f51e);

    SET_FLAGS_ADD(R6, 2);        /* add r6, #2: flags from the old pointer */
    R6 += 2;

    RETURN_TO_CALLER();
}

/* npc_enableScript0x19_809f516(r5 = NPC object):
 * Clear the NPC's script-0x19 "terminate script" flag (re-enables the script),
 * then return r0 = 0 with the flags set from that zero. */
Next npc_enableScript0x19_809f516(void) {
    R0 = 0;
    ST8(R5 + oOverworldNPCObject_TerminateScript_19, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* npc_disableScript0x19_809f51e(r5 = NPC object):
 * Marks the NPC's script as terminated (sets the "disable script 0x19" flag to 1)
 * so the overworld script handler stops running it.
 * Returns r0 = 1 (nz flags reflect that value, as set by the original's MOV). */
Next npc_disableScript0x19_809f51e(void) {
    R0 = 1;
    ST8(R5 + oOverworldNPCObject_TerminateScript_19, 1);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* npc_runSecondaryScriptMaybe_809ebf8 (r5 = NPC): run the NPC's secondary
 * script until a command stops it. Each command's handler comes from
 * NPCCommandsJumptable, entered as the original (r0 = opcode * 4, r6 = the
 * script pointer, r7 = the handler) and returns the advanced pointer in r6.
 * Once the script is stopped (TerminateScript_1f set), count down the
 * secondary timer instead; an NPC without a secondary script does nothing. */
Next npc_runSecondaryScriptMaybe_809ebf8(void) {
    for (;;) {
        R0 = LD8(R5 + oOverworldNPCObject_TerminateScript_1f);
        SET_FLAGS_NZ(R0);                        /* as the original (tst r0, r0) */
        if (R0) {
            GUEST_CALL(npc_decrementSecondaryTimer_809ec1c);
            break;
        }
        R6 = LD32(R5 + oOverworldNPCObject_UnkNPCScriptPtr_5c);
        SET_FLAGS_NZ(R6);
        if (!R6)
            break;
        R0 = LD8(R6) << 2;
        SET_FLAGS_NZ(R0);                        /* lsl r0, r0, #2 */
        cpu.c = 0;
        R7 = LD32(A_NPCCommandsJumptable + R0);
        GUEST_CALL_ADDR(R7);
        ST32(R5 + oOverworldNPCObject_UnkNPCScriptPtr_5c, R6);
    }
    RETURN_TO_CALLER();
}

/* npc_waitCutsceneVar_809ea82 (r5 = overworld NPC object, r10 = toolkit):
 * Script opcode handler for "wait until cutscene variable N equals V". The NPC
 * object carries the index and the expected value of the cutscene variable it
 * is waiting on. Once the variable in the cutscene state matches the expected
 * value, run this NPC's script-0x19 enable routine; afterwards run its
 * secondary script either way. Preserves r4 and r7. */
Next npc_waitCutsceneVar_809ea82(void) {
    u32 savedR4 = R4;
    u32 savedR7 = R7;

    u32 cutsceneState = LD32(R10 + oToolkit_CutsceneStatePtr);
    u32 varIndex      = LD32(R5 + oOverworldNPCObject_CutsceneVarIndexToWaitFor);
    u32 varValue      = LD32(R5 + oOverworldNPCObject_CutsceneVarValueToWaitFor);

    R1 = varValue;
    R4 = cutsceneState;
    R7 = oOverworldNPCObject_CutsceneVarValueToWaitFor;

    /* Watched variable reached its target: release the NPC's follow-up script. */
    R0 = LD8(cutsceneState + varIndex);
    SET_FLAGS_CMP(R0, varValue);         /* as the original (cmp r0, r1) */
    if (R0 == varValue)
        GUEST_CALL(npc_enableScript0x19_809f516);

    /* Continue with the NPC's secondary script regardless. */
    R1 = varValue;
    R4 = cutsceneState;
    GUEST_CALL(npc_runSecondaryScriptMaybe_809ebf8);

    R4 = savedR4;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

