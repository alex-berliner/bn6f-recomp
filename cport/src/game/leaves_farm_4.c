/*
 * leaves_farm_4.c: small functions from the model farm, set 4.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── CutsceneCmd_nop_80377d0 ── */
/* CutsceneCmd_nop_80377d0: "nop" cutscene command.
 *
 * Calls returnZero_809E228 (which always yields 0) and, as long as it does,
 * advances the cutscene script pointer (r7) past this no-op command and
 * reports "keep running" (r0 = 1). If the call ever returned nonzero, the
 * script would be halted instead (r0 = 0) without advancing. */
Next CutsceneCmd_nop_80377d0(void) {
    GUEST_CALL(returnZero_809E228);

    /* cmp r0, #0 — sets the flags the bne tests (and its C/V survive on
     * the halt path, since movs r0,#0 only rewrites N/Z). */
    SET_FLAGS_CMP(R0, 0);

    if (R0 == 0) {
        /* adds r7, #1 — this sets C (and V), and those flags are what
         * the caller sees, since the following movs only rewrites N/Z. */
        SET_FLAGS_ADD(R7, 1);
        R7 = R7 + 1;          /* skip past this no-op command */

        R0 = 1;               /* continue the script */
        SET_FLAGS_NZ(R0);
    } else {
        R0 = 0;               /* halt the script */
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_set_var ── */
/* CutsceneCmd_set_var — "set variable" cutscene script command (0x08037A2A).
 *
 * Reads two operands from the script stream via ReadMapScriptByte: the first
 * fetch (r6 = 2) yields the destination slot offset in r4, the second
 * (r6 = 1) the value byte in r0. The value is stored into the cutscene
 * variable area at (r5 + slot). The command is three bytes long, so the
 * script pointer r7 is advanced past it, and the handler reports success to
 * the dispatcher by returning r0 = 1. */
Next CutsceneCmd_set_var(void) {
    R6 = 2;                       /* first operand: variable slot */
    GUEST_CALL(ReadMapScriptByte);
    R0 = R4;                      /* slot offset comes back in r4 */

    R6 = 1;                       /* second operand: value byte */
    GUEST_CALL(ReadMapScriptByte);

    ST8(R5 + R4, R0);             /* vars[slot] = value */

    /* Advance the script pointer past the whole command. */
    u32 ptr = R7;
    R7 = ptr + 3;
    SET_FLAGS_ADD(ptr, 3);

    R0 = 1;                       /* tell the dispatcher the command ran */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_switch_case_from_chatbox_flags_bit0_to_2 ── */
/* CutsceneCmd_switch_case_from_chatbox_flags_bit0_to_2 (0x08037AA0):
 * Cutscene "switch" command keyed on the chatbox flag word. Reads the
 * current chatbox state, builds the matching case-table offset from it
 * (flags * 4 + 1), reads the corresponding word out of the map script,
 * and hands that word to the script interpreter in r7. Returns r0 = 1.
 *
 * inputs:  r1 = script context, r7 = script pointer
 * outputs: r0 = 1, r4/r6 as left by ReadMapScriptWord, r7 = case word,
 *          flags N=0 Z=0 C=0 V=0 (see below), r1 preserved */
Next CutsceneCmd_switch_case_from_chatbox_flags_bit0_to_2(void) {
    u32 chatboxFlags;

    /* Bits 0-2 of the chatbox flags select which case to branch to. */
    GUEST_CALL(chatbox_GetState_8045F4C);
    chatboxFlags = R0;

    /* Case table entries are 4 bytes apart, offset by one. Note that r0
     * still holds the flag word here; ReadMapScriptWord reads it as well. */
    R6 = (chatboxFlags << 2) + 1;
    GUEST_CALL(ReadMapScriptWord);

    /* ReadMapScriptWord returns the case word in r4 (r6 = advanced script
     * pointer). "mov r7, r4" assembles as "adds r7, r4, #0", which is why
     * it clears C and V; the following "movs r0, #1" then sets N and Z. */
    R7 = R4;
    SET_FLAGS_ADD(R4, 0);

    R0 = 1;
    SET_FLAGS_NZ(1);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_load_gfx_anim ── */
/* MapScriptCutsceneCmd_load_gfx_anim (0x08035F2A):
 * Map script opcode handler for loading a GFX animation.
 *
 * Entry state: R0 = map script state (passed straight through to the script
 * reader), R7 = script stream cursor. The handler reads one 32-bit operand
 * from the stream (ReadMapScriptWord leaves it in R4), passes it to
 * LoadGFXAnim as the GFXAnimScript pointer, then advances the cursor past
 * the opcode byte plus the 4-byte operand (5 bytes total) and returns 1.
 *
 * Register contract: r5/r8/r9/r12 preserved; flags on return are N/Z from
 * R0 = 1 and C/V from the cursor addition, matching the original's
 * ADDS r7, #5 followed by MOVS r0, #1. */
Next MapScriptCutsceneCmd_load_gfx_anim(void) {
    R6 = 1;                          /* mode flag consumed by ReadMapScriptWord */
    GUEST_CALL(ReadMapScriptWord);   /* reads the operand word -> R4 */

    R0 = R4;                         /* the GFXAnimScript * that was read */
    GUEST_CALL(LoadGFXAnim);

    u32 cursor = R7;
    R7 = cursor + 5;
    SET_FLAGS_ADD(cursor, 5);        /* C/V from the cursor advance */

    R0 = 1;
    SET_FLAGS_NZ(R0);                /* N/Z from the return value */

    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_play_sound ── */
/* MapScriptCutsceneCmd_play_sound: map script command "play sound".
 * Reads the sound effect ID as a halfword from the script (argument in R6),
 * plays it, then advances the script cursor R7 past opcode + operand and
 * returns 1 so the interpreter continues with the next command.
 * Flags come from the `adds r7, #3` cursor advance (MOV sets none). */
Next MapScriptCutsceneCmd_play_sound(void) {
    R6 = 1;                          /* argument for ReadMapScriptHalfword */
    GUEST_CALL(ReadMapScriptHalfword);
    R0 = R4;                         /* the halfword is delivered in R4 */
    GUEST_CALL(PlaySoundEffect);
    u32 cursor = R7;
    R7 = cursor + 3;                 /* skip opcode word + halfword */
    SET_FLAGS_ADD(cursor, 3);
    R0 = 1;                          /* tell the interpreter to keep running */
    RETURN_TO_CALLER();
}

/* ── cutsceneCamera_focusCameraOnPlayerMaybe_8036faa ── */
/* cutsceneCamera_focusCameraOnPlayerMaybe_8036faa:
 * Point the cutscene camera at the player. Loads the game state from the
 * toolkit (r10), fetches the overworld player object from it, and calls
 * camera_writeUnk03_14_80301b2 with the address of its coordinate field
 * (plus the mode argument 1). The `add r1, #imm` on the way in sets the
 * flags, which the caller may inspect, so reproduce that too. */
Next cutsceneCamera_focusCameraOnPlayerMaybe_8036faa(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = 1;
    R1 = player + oOWPlayerObject_Coords;
    /* Thumb "add r1, #imm" is an ADDS: N/Z/C/V all come from this add. */
    SET_FLAGS_ADD(player, oOWPlayerObject_Coords);

    GUEST_CALL(camera_writeUnk03_14_80301b2);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_disable_collision_alternate ── */
/* NPCCommand_disable_collision_alternate(r5 = NPC object):
 * Sets the NPC's "collision disabled" flag (0x10) in its UnkFlags_60 field,
 * then advances the NPC script command counter in r6. Flags are those left
 * by the final ADDS (r6 + 1). r0 = flag bit, r1 = updated flags. */
Next NPCCommand_disable_collision_alternate(void) {
    R0 = OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION_0x10;
    R1 = LD32(R5 + oOverworldNPCObject_UnkFlags_60) | R0;
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, R1);
    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_end_secondary_script ── */
/* NPCCommand_end_secondary_script(r5 = NPC object):
 * Terminate the NPC's secondary (event) script: clear its script pointer and
 * script timer, clear r6 to signal the interpreter that the script ended, and
 * return r0 = NULL. Flags reflect the final MOV (r6 == 0). */
Next NPCCommand_end_secondary_script(void) {
    R0 = 0;
    ST32(R5 + oOverworldNPCObject_UnkNPCScriptPtr_5c, R0);
    ST16(R5 + oOverworldNPCObject_Timer_22, R0);
    R6 = 0;
    SET_FLAGS_NZ(R6);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_jump_with_link ── */
/* NPCCommand_jump_with_link(r5 = NPC object, r6 = script program counter):
 * "call subroutine" opcode of the NPC script VM. Stores the address of the
 * next opcode (PC + 5) in the NPC's link register, then fetches the 32-bit
 * destination address located at PC + 1 and resumes the script there.
 * Leaves r0 = r6 = destination, r4 = link register field offset, and the
 * flags as the final "mov r6, r0" (adds r6, r0, #0) set them. */
Next NPCCommand_jump_with_link(void) {
    /* Link register := return address (opcode is 1 byte + 4-byte operand). */
    R4 = oOverworldNPCObject_LinkRegister;
    ST32(R5 + R4, R6 + 5);

    /* Read the jump destination and continue the script there. */
    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptWord);
    R6 = R0;
    SET_FLAGS_ADD(R6, 0);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_pause_secondary_script ── */
/* NPCCommand_pause_secondary_script(r5 = NPC object, r6 = script instruction pointer):
 * Script command "pause": clears the NPC's pause-related byte, reads the pause
 * length from the operand byte into the object's timer, flags the script to
 * stop running, and steps the pointer past the opcode+operand.
 * Returns r0 = 1, r6 = script pointer advanced by 2, and nz/c/v as set by the
 * ADDS r6, #2 pointer increment. */
Next NPCCommand_pause_secondary_script(void) {
    ST8(R5 + oOverworldNPCObject_Unk_0b, 0);

    R0 = LD8(R6 + 1);
    ST16(R5 + oOverworldNPCObject_Timer_22, R0);

    R0 = 1;
    ST8(R5 + oOverworldNPCObject_TerminateScript_1f, R0);

    SET_FLAGS_ADD(R6, 2);
    R6 += 2;

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_active_and_invisible ── */
/* NPCCommand_set_active_and_invisible(r5 = object header, r6 = script pointer):
 * Marks the object active (flags byte = 1, the OBJECT_FLAG_ACTIVE bit) and
 * advances the NPC script pointer past this command. Sets flags from the
 * pointer increment; r0 is left holding the flag value written. */
Next NPCCommand_set_active_and_invisible(void) {
    R0 = 1; /* OBJECT_FLAG_ACTIVE */
    ST8(R5, R0);

    R6 = R6 + 1;
    SET_FLAGS_ADD(R6 - 1, 1);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_active_and_visible ── */
/* NPCCommand_set_active_and_visible(r5 = object header, r6 = NPC script pointer):
 * Script command: mark the object as both active and visible by writing the
 * flags byte, then advance the script pointer by one. The r6 increment is the
 * original's last flag-setting instruction, so nz/c/v come from it. */
Next NPCCommand_set_active_and_visible(void) {
    R0 = OBJECT_FLAG_ACTIVE | OBJECT_FLAG_VISIBLE;
    ST8(R5, R0);

    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_animation ── */
/* NPCCommand_set_animation(r5 = NPC object, r6 = script command pointer):
 * Script opcode handler: read the animation ID byte that follows the opcode,
 * store it into the NPC's animation selector, and advance the script pointer
 * past the opcode and its argument. Flags reflect the pointer increment. */
Next NPCCommand_set_animation(void) {
    u32 animation_id = LD8(R6 + 1);

    ST8(R5 + oOverworldNPCObject_AnimationSelect, animation_id);
    R0 = animation_id;

    R6 += 2;
    SET_FLAGS_ADD(R6, 2);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_collision_radius ── */
/* NPCCommand_set_collision_radius(r5 = NPC object, r6 = command stream cursor):
 * read a one-byte radius argument from the NPC script and store it as the
 * object's collision radius, then advance the cursor past opcode + argument.
 * Returns r0 = the radius byte, r6 = advanced cursor; flags reflect r6 += 2. */
Next NPCCommand_set_collision_radius(void) {
    u32 radius = LD8(R6 + 1);
    ST8(R5 + oOverworldNPCObject_CollisionRadius, radius);
    R0 = radius;
    R6 = R6 + 2;
    SET_FLAGS_ADD(R6, 2);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_layer_priority_override_to_2 ── */
/* NPCCommand_set_layer_priority_override_to_2: an NPC script command that
 * sets the NPC's layer priority override to 2 (r5 = NPC object), then
 * advances the script cursor (r6). Leaves r0 = 2 and flags from r6 + 1. */
Next NPCCommand_set_layer_priority_override_to_2(void) {
    ST32(R5 + oOverworldNPCObject_LayerPriorityOverride, 2);
    R0 = 2;
    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_npc_palette_index ── */
/* NPCCommand_set_npc_palette_index (r5 = NPC object, r6 = script pointer):
 * Reads the palette byte from the NPC script command stream, stores it into
 * the NPC's palette index field, then advances the script pointer past the
 * command. Returns with r0 = the byte read, r6 = updated script pointer,
 * and flags from the pointer increment (ADDS r6, #2). */
Next NPCCommand_set_npc_palette_index(void) {
    u8 palette = LD8(R6 + 1);

    ST8(R5 + oOverworldNPCObject_PaletteIndex, palette);
    R0 = palette;
    R6 += 2;
    SET_FLAGS_ADD(R6, 2);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_sprite_with_category ── */
/* NPCCommand_set_sprite_with_category (r5 = NPC object, r6 = command stream cursor):
 * Handles the "set NPC sprite" command: the next two bytes of the stream give the
 * sprite index (stored as a halfword) and sprite category (stored as a word).
 * Also clears the object's "is a shadow" flag. Advances the cursor past the
 * operands (3 bytes). Returns the updated flags word in r0, the shadow flag
 * bit in r1, and the advanced cursor in r6; flags reflect (r6 + 3). */
Next NPCCommand_set_sprite_with_category(void) {
    u32 npc = R5;
    u32 cmd = R6;

    /* Sprite index: halfword field in the NPC object. */
    u32 sprite = LD8(cmd + 1);
    ST16(npc + oOverworldNPCObject_NPCSprite, sprite);

    /* Sprite category: word field in the NPC object. */
    ST32(npc + oOverworldNPCObject_NPCSpriteCategory, (u32)LD8(cmd + 2));

    /* This sprite is a regular one, not a shadow: clear the shadow flag. */
    u32 flags = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    flags &= ~OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;
    ST32(npc + oOverworldNPCObject_UnkFlags_60, flags);

    /* Advance the command cursor past opcode + operands, then return. */
    u32 nextCmd = cmd + 3;
    SET_FLAGS_ADD(cmd, 3);

    R0 = flags;
    R1 = OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;
    R6 = nextCmd;

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_text_script_index ── */
/* NPCCommand_set_text_script_index(r5 = NPC object, r6 = script cursor):
 * Read the byte after the current opcode (the text script index), store it in
 * the NPC's TextScriptIndex field, and advance the cursor past both bytes.
 * Returns: r0 = the index that was read, r6 = cursor advanced by 2.
 * Flags reflect the cursor increment. */
Next NPCCommand_set_text_script_index(void) {
    u32 script_index = LD8(R6 + 1);
    ST8(R5 + oOverworldNPCObject_TextScriptIndex, script_index);
    R6 += 2;
    SET_FLAGS_ADD(R6 - 2, 2);
    R0 = script_index;
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_text_script_index_and_ptr_to_decomp_buffer ── */
/* NPCCommand_set_text_script_index_and_ptr_to_decomp_buffer (r5 = NPC object,
 * r6 = pointer to the command's arguments):
 * Text script start command. Reads the script index byte that follows the
 * opcode, stores it in the NPC, marks the NPC as having a text script
 * (chatbox flag 0x400 in UnkFlags_60) and points its script pointer at the
 * text decompression buffer at byte_202FA04. Advances r6 past the command
 * (opcode + index byte) and leaves the ADDS flags for callers to branch on. */
Next NPCCommand_set_text_script_index_and_ptr_to_decomp_buffer(void) {
    u32 npc = R5;
    u32 cmd = R6;

    ST8(npc + oOverworldNPCObject_TextScriptIndex, LD8(cmd + 1));

    ST32(npc + oOverworldNPCObject_UnkFlags_60,
         LD32(npc + oOverworldNPCObject_UnkFlags_60) | 0x400);

    ST32(npc + oOverworldNPCObject_TextScriptPtr, A_byte_202FA04);

    R6 = cmd + 2;
    SET_FLAGS_ADD(cmd, 2);

    /* Registers the original happens to leave behind: */
    R0 = 0x400;
    R1 = oOverworldNPCObject_TextScriptPtr;
    R3 = A_byte_202FA04;

    RETURN_TO_CALLER();
}

/* ── NPCCommand_shift_center ── */
/* NPCCommand_shift_center(r5 = NPC object, r6 = script data pointer):
 * Reads the 3-byte "center offset" payload (X, Y, Z) that follows this script
 * command, copies it into the NPC's center-offset fields, and advances the
 * script data pointer past the command (1 opcode byte + 3 payload bytes).
 * Leaves r0 = Z offset, r1 = 3, flags from "adds r6, #4". */
Next NPCCommand_shift_center(void) {
    s8 centerX = (s8)LD8(R6 + 1);
    s8 centerY = (s8)LD8(R6 + 2);
    s8 centerZ = (s8)LD8(R6 + 3);

    ST8(R5 + oOverworldNPCObject_CenterOffsetX, centerX);
    ST8(R5 + oOverworldNPCObject_CenterOffsetY, centerY);
    ST8(R5 + oOverworldNPCObject_CenterOffsetZ, centerZ);

    R0 = (s8)centerZ;   /* ldrsb sign-extends; last value stays in r0 */
    R1 = 3;
    {
        u32 oldPtr = R6;
        R6 = oldPtr + 4;
        SET_FLAGS_ADD(oldPtr, 4);
    }

    RETURN_TO_CALLER();
}

/* ── NPCCommand_write_cutscene_var ── */
/* NPCCommand_write_cutscene_var: "setvar" opcode handler for cutscene scripts.
 *
 * R6 points into the running cutscene script at the current opcode:
 *   [opcode][var index][value]
 * It stores the value byte into the cutscene's variable buffer at the given
 * index, then advances the script pointer past all three bytes.
 *
 * Outputs: R0 = variable index, R1 = value, R4 = cutscene state pointer,
 * R6 = advanced script pointer, flags set by the pointer update (ADDS r6,#3).
 */
Next NPCCommand_write_cutscene_var(void) {
    u32 cutsceneState = LD32(R10 + oToolkit_CutsceneStatePtr);
    u32 varIndex = LD8(R6 + 1);
    u32 value = LD8(R6 + 2);

    ST8(cutsceneState + varIndex, value);

    R0 = varIndex;
    R1 = value;
    R4 = cutsceneState;
    R6 += 3;
    SET_FLAGS_ADD(R6, 3);

    RETURN_TO_CALLER();
}

/* ── ReadNPCScriptHalfword ── */
/* ReadNPCScriptHalfword(r0 = script pointer):
 * Reads a little-endian 16-bit value from the NPC script and advances nothing
 * (the caller owns the script cursor). Returns the halfword in r0 and the low
 * byte in r1; carry is cleared, as by the original LSL #8 / ORR sequence. */
Next ReadNPCScriptHalfword(void) {
    u32 addr = R0;
    u32 lo = LD8(addr);
    u32 hi = LD8(addr + 1);

    /* lsls hi, hi, #8 shifts a byte left by 8, so the bit shifted out into
     * carry (bit 24 of the source) is always 0. ORR (Thumb ORRS) then sets
     * N/Z from the result but leaves carry untouched. */
    cpu.c = 0;

    R0 = (hi << 8) | lo;
    R1 = lo;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── npc_beginChatboxState_809F5FC ── */
/* Offsets not present in game.h; taken from the disassembly. */
#define oOverworldNPCObject_CurState   8   /* byte  */
#define oOverworldNPCObject_CurAction  9   /* byte  */
#define NPC_CUR_STATE_IN_CHATBOX       8

/* npc_beginChatboxState_809F5FC (r5 = overworld NPC object):
 * Begin talking to an NPC: back up its current action and movement flags,
 * switch it to the "in chatbox" state, and clear the action/movement
 * fields while the conversation runs. */
Next npc_beginChatboxState_809F5FC(void) {
    ST8(R5 + oOverworldNPCObject_Unk_1d,
        LD8(R5 + oOverworldNPCObject_CurAction));
    ST8(R5 + oOverworldNPCObject_Unk_1e,
        LD8(R5 + oOverworldNPCObject_MovementFlag_0a));
    ST8(R5 + oOverworldNPCObject_CurState, NPC_CUR_STATE_IN_CHATBOX);
    ST8(R5 + oOverworldNPCObject_CurAction, 0);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);
    RETURN_TO_CALLER();
}

/* ── npc_overworld_CheckStateFlags50000_809F638 ── */
/* npc_overworld_CheckStateFlags50000_809F638(r5 = NPC object):
 * Checks the NPC's state flags for bits 0x50000 (some movement/animation state
 * marker) and returns 1 in r0 if any of them is set, 0 otherwise.
 * Also leaves the flag word in r1 and the mask 0x50000 in r2, as the original does. */
Next npc_overworld_CheckStateFlags50000_809F638(void) {
    u32 flags = LD32(R5 + oOverworldNPCObject_Unk_54);

    R1 = flags;
    R2 = 0x50000;
    R0 = (flags & 0x50000) != 0;

    RETURN_TO_CALLER();
}

/* ── IsR2BetweenR0AndR1 ── */
/* IsR2BetweenR0AndR1: 0800E6E8 THUMB
 * Returns whether r2 lies in (min(r0,r1), max(r0,r1)] — note the lower
 * bound is exclusive, the upper bound inclusive. Result is left in r0
 * and also copied to r3. */
Next IsR2BetweenR0AndR1(void) {
    s32 a = (s32)R0;
    s32 b = (s32)R1;
    s32 value = (s32)R2;
    s32 result;

    if (a <= b) {
        /* r0..r1 given in ascending order. */
        result = (value > a && value <= b);
    } else {
        /* Range was given reversed: r1 is the lower bound. */
        result = (value > b && value <= a);
    }

    R3 = (u32)result;
    R0 = (u32)result;
    RETURN_TO_CALLER();
}

/* ── calcFinalDamageAsPanelDamage1to5SumReturnFinalDamage_r0_PanelDamage6_r1_800E3BE ── */
/* calcFinalDamageAsPanelDamage1to5SumReturnFinalDamage_800E3BE(r4 = preserved, r5 = battle object):
 * Sums the five PanelDamage entries in the object's collision data, writes the sum to
 * FinalDamage, and returns:
 *   r0 = final damage (the PanelDamage1..5 sum)
 *   r1 = PanelDamage6
 *   r3 = the collision data pointer
 */
Next calcFinalDamageAsPanelDamage1to5SumReturnFinalDamage_r0_PanelDamage6_r1_800E3BE(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 panelDamage1 = collision + oCollisionData_PanelDamage1;

    u32 sum = 0;
    for (u32 i = 0; i < 5; i++)
        sum += LD16(panelDamage1 + 2 * i);

    ST16(collision + oCollisionData_FinalDamage, sum);

    R0 = sum;
    R1 = LD16(collision + oCollisionData_PanelDamage6);
    R3 = collision;
    RETURN_TO_CALLER();
}

/* ── convertBattleObjectXYToPanelXY_800E24C ── */
/* convertBattleObjectXYToPanelXY_800E24C(r5 = battle object):
 * wrapper that reads the object's pixel-space X/Y and converts them to
 * panel coordinates. Returns the panel X/Y in r0/r1 (via the callee). */
Next convertBattleObjectXYToPanelXY_800E24C(void) {
    R0 = LD32(R5 + oBattleObject_X);
    R1 = LD32(R5 + oBattleObject_Y);
    GUEST_CALL(convertXYToPanelXY_800E258);
    RETURN_TO_CALLER();
}

/* ── convertXYToPanelXY_800E258 ── */
/* convertXYToPanelXY_800E258(r0 = X as 16.16 fixed point, r1 = Y as 16.16 fixed point):
 * convert a screen-space position into battle-field panel coordinates.
 * The field's origin sits 160px right / 32px down in screen space and each
 * panel is 40px wide by 24px tall, so panel = (whole-pixel coord + origin) / size.
 * Both divisions are done with the BIOS Divide routine (swi 6), which returns
 * the quotient in r0 and the remainder in r1; only the quotient is used.
 * Returns: r0 = panel column, r1 = panel row.
 * Flag notes: the trailing `mov r1, r0` is really an encoded `lsls r1, r0, #0`,
 * so it refreshes only N/Z (from the panel row). The BIOS Divide is the last
 * instruction that touches C/V, and it leaves both clear, which is the same
 * flag pattern an ADDS-of-zero produces, so we mirror that here. */
Next convertXYToPanelXY_800E258(void) {
    /* Keep only the whole-pixel part of each fixed-point coordinate. */
    s32 screenX = (s32)R0 >> 16;
    s32 screenY = (s32)R1 >> 16;

    /* Panel column = (screenX + 160) / 40 */
    R0 = (u32)(screenX + 0xA0);
    R1 = 0x28;
    swi_call(6);                    /* BIOS Div: r0 = quotient, r1 = remainder */
    u32 panelX = R0;

    /* Panel row = (screenY + 32) / 24 */
    R0 = (u32)(screenY + 0x20);
    R1 = 0x18;
    swi_call(6);

    /* r1 = panel row, with N/Z taken from it and C/V left clear. */
    R1 = R0;
    SET_FLAGS_ADD(R0, 0);

    R0 = panelX;                    /* panel column, saved across the second divide */

    RETURN_TO_CALLER();
}

/* ── object_clearInvulnerableTime ── */
/* object_clearInvulnerableTime(r5 = battle object):
 * Reset the object's post-hit invulnerability counter in its collision data
 * and clear the invulnerability flag (bit 8) via object_clearFlag.
 * Leaves r1/r2 as object_clearFlag left them (callers expect that). */
Next object_clearInvulnerableTime(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collisionData + oCollisionData_Unk_28, 0);

    R0 = 8;
    GUEST_CALL(object_clearFlag);

    RETURN_TO_CALLER();
}

/* ── object_getAllianceDirection ── */
/* object_getAllianceDirection(r0 = alliance id):
 * Maps an alliance id to a facing multiplier: 0 (player side) -> +1,
 * 1 (enemy side) -> -1, i.e. 1 - 2*alliance. The final NEG (rsbs) is the
 * last flag-setting instruction, so N/Z/C/V must come from 0 - flipped. */
Next object_getAllianceDirection(void) {
    u32 doubled = R0 << 1;          /* lsl r0, r0, #1 */
    u32 flipped = doubled - 1;      /* sub r0, #1   -> 2*alliance - 1 */
    SET_FLAGS_CMP(0, flipped);      /* neg r0, r0 = rsbs r0, r0, #0: flags of 0 - flipped */
    R0 = 0 - flipped;               /* ...and the negated result itself */
    RETURN_TO_CALLER();
}

/* ── object_getCoordinatesForPanels ── */
/* object_getCoordinatesForPanels(r0 = panel column, r1 = panel row):
 * Convert a battle-field panel position into on-screen pixel coordinates as
 * 16.16 fixed point: r0 = x, r1 = y. The panel grid is 40x24 pixels per cell,
 * and the field origin sits 140 px left / 20 px up (the 0xFF740000/0xFFEC0000
 * offsets are -0x8C and -0x14 carried in the high halfword).
 * Flags reflect the final ADDS used for the y coordinate. */
Next object_getCoordinatesForPanels(void) {
    s32 col = (s8)R0;
    s32 row = (s8)R1;

    u32 x = (u32)(col * 40) << 16; /* 40 pixels per panel column */
    u32 y = (u32)(row * 24) << 16; /* 24 pixels per panel row   */

    R0 = x + 0xFF740000u;

    R2 = 0xFFEC0000u;
    R1 = y + R2;
    SET_FLAGS_ADD(y, R2);

    RETURN_TO_CALLER();
}

/* ── object_getEnemyDirection ── */
/* object_getEnemyDirection(r5 = battle object):
 * Returns the object's facing step in R0: +1 for alliance 0 (player side),
 * -1 for alliance 1 (enemy side), as a two's-complement u32. Flags are left
 * as the final NEG (rsbs) of object_getAllianceDirection sets them. */
Next object_getEnemyDirection(void) {
    u32 alliance = LD8(R5 + oBattleObject_Alliance);

    /* body of object_getAllianceDirection: lsl r0,#1; sub r0,#1; neg r0,r0 */
    u32 step = 2 * alliance - 1;
    SET_FLAGS_CMP(0, step);
    R0 = 0 - step;

    RETURN_TO_CALLER();
}

/* ── object_getFlip ── */
/* object_getFlip(r5 = battle object):
 * Returns the object's horizontal orientation: its alliance bit (player/enemy
 * side) XORed with its DirectionFlip override. r0 receives the combined flip
 * flag, r1 the raw DirectionFlip byte (both are live outputs for callers). */
Next object_getFlip(void) {
    u32 flip = LD8(R5 + oBattleObject_DirectionFlip);
    R0 = LD8(R5 + oBattleObject_Alliance) ^ flip;
    R1 = flip;
    RETURN_TO_CALLER();
}

/* ── object_getFlipDirection ── */
/* object_getFlipDirection(r0 = direction A, r1 = direction B):
 * returns -1 if the two direction flags differ (sprite must be flipped
 * horizontally) or +1 if they match. r0 = result; flags come from the
 * final NEG (RSBS 0, r0) in the original, i.e. from 0 - preNeg value. */
Next object_getFlipDirection(void) {
    u32 differ = (R0 ^ R1) << 1;    /* 2 if directions differ, 0 if equal */
    u32 preNeg = differ - 1;        /* 1 if differing, -1 (0xFFFFFFFF) if equal */

    R0 = 0 - preNeg;                /* -1 or 1 */
    SET_FLAGS_CMP(0, preNeg);       /* NEG sets NZCV on the pre-neg value */
    RETURN_TO_CALLER();
}

/* ── object_getFrontDirection ── */
/* object_getFrontDirection(r5 = battle object):
 * Loads the object's alliance side and facing flip, then tail-calls
 * object_getFlipDirection(alliance, flip) to get the front-facing direction
 * multiplier (+1 when the object faces toward the enemy side it belongs to,
 * -1 otherwise). r0 = result, r1 = the DirectionFlip byte as loaded. */
Next object_getFrontDirection(void) {
    R0 = LD8(R5 + oBattleObject_Alliance);
    R1 = LD8(R5 + oBattleObject_DirectionFlip);

    /* Tail call: object_getFlipDirection returns straight to our caller. */
    GUEST_CALL(object_getFlipDirection);

    RETURN_TO_CALLER();
}

/* ── object_getPanelParameters ── */
/* object_getPanelParameters(r0 = panel x, r1 = panel y):
 * Return the panel's status flags word (or 0 if the coordinates don't
 * name a valid panel). Flags c/v come from object_getPanelDataOffset,
 * nz reflects the null check, as in the original. */
Next object_getPanelParameters(void) {
    GUEST_CALL(object_getPanelDataOffset); /* -> R0 = panel data pointer, or 0 */

    SET_FLAGS_NZ(R0);
    R0 = (R0 != 0) ? LD32(R0 + oPanelData_Flags) : 0;

    RETURN_TO_CALLER();
}

/* ── object_highlightPanel ── */
/* object_highlightPanel(r0, r1 = object):
 * If the object stands on a valid panel, fetch its panel data offset and
 * set the flag byte at offset 1 (oPanelData_Unk_01) to 1.
 * Returns r0 = panel data offset (or the validity check result when the
 * panel is invalid), r1 = 1 when the flag was written. Flags: those left
 * by the validity check when the panel is invalid, otherwise NZ from the
 * stored value 1 (as with MOV r1,#1) with C/V from getPanelDataOffset.
 * In both paths r0/r1 end up holding the original arguments, since the
 * original pops them back off the stack before branching. */
Next object_highlightPanel(void) {
    u32 object = R0;
    u32 attr = R1;

    GUEST_CALL(object_isValidPanel);

    if (R0 == 0) {
        /* Not a valid panel: restore the original arguments and return. */
        R0 = object;
        R1 = attr;
        RETURN_TO_CALLER();
    }

    /* Valid panel: get its panel data and mark it as highlighted. */
    R0 = object;
    R1 = attr;
    GUEST_CALL(object_getPanelDataOffset);

    ST8(R0 + 1, 1); /* panelData->Unk_01 = 1 */
    R1 = 1;
    SET_FLAGS_NZ(1);

    RETURN_TO_CALLER();
}

/* ── object_isCurrentPanelSolid ── */
/* object_isCurrentPanelSolid (r5 = battle object):
 * check whether the panel the object currently occupies is solid
 * (hole, broken tile, off-grid, ...). Forwards the panel coordinates
 * to object_isPanelSolid as a tail call, so its result registers and
 * flags are passed straight through to the caller. */
Next object_isCurrentPanelSolid(void) {
    R0 = LD8(R5 + oBattleObject_PanelX);
    R1 = LD8(R5 + oBattleObject_PanelY);
    GUEST_CALL(object_isPanelSolid);
    RETURN_TO_CALLER();
}

/* ── object_isCurrentPanelValid ── */
/* object_isCurrentPanelValid(r5 = battle object):
 * checks whether the panel the object currently occupies is a valid
 * battlefield position; delegates to object_isValidPanel with the
 * object's panel coordinates.
 * outputs: r0/r1 as returned by object_isValidPanel (plus its flags). */
Next object_isCurrentPanelValid(void) {
    R0 = LD8(R5 + oBattleObject_PanelX);
    R1 = LD8(R5 + oBattleObject_PanelY);
    GUEST_CALL(object_isValidPanel);
    RETURN_TO_CALLER();
}

/* ── object_isPanelSolid ── */
/* object_isPanelSolid(r0 = object, r1 = side):
 * Fetches the panel parameters for the object's position and tests the
 * "solid panel" flag (0x10). Returns the masked flag in r0, with NZ set
 * from it so callers can branch directly on the result. */
Next object_isPanelSolid(void) {
    GUEST_CALL(object_getPanelParameters);
    u32 solidFlag = R0 & 0x10;
    R1 = 0x10;
    R0 = solidFlag;
    SET_FLAGS_NZ(solidFlag);
    RETURN_TO_CALLER();
}

/* ── object_setCoordinatesFromPanels ── */
/* object_setCoordinatesFromPanels(r5 = battle object):
 * Read the object's panel position, convert it to pixel coordinates and
 * store the result back into the object's X/Y fields.
 * Returns the pixel coordinates in r0 (X) and r1 (Y); flags and r2 are
 * whatever object_getCoordinatesForPanels left behind. */
Next object_setCoordinatesFromPanels(void) {
    u32 object = R5;

    R0 = LD8(object + oBattleObject_PanelX);
    R1 = LD8(object + oBattleObject_PanelY);
    GUEST_CALL(object_getCoordinatesForPanels);

    ST32(object + oBattleObject_X, R0);
    ST32(object + oBattleObject_Y, R1);

    RETURN_TO_CALLER();
}

/* ── object_setPanelAllianceTimerLong ── */
/* object_setPanelAllianceTimerLong(r0 = object index):
 * Give the panel-ownership timer of the indexed object a long value
 * (store 0x708 halfword at entry+6 in the object array at 0x2034010).
 * Leaves r0 = array base, r1 = object entry address, r2 = 0x708;
 * c/v come from the ADDS that forms the entry address (MUL sets no flags). */
Next object_setPanelAllianceTimerLong(void) {
    u32 index = R0;
    u32 arrayBase = A_unk_2034010;      /* object array base (literal 0x2034010) */
    u32 offset = index * 8;             /* each object entry is 8 bytes */
    u32 entry = arrayBase + offset;

    R0 = arrayBase;
    R1 = entry;
    SET_FLAGS_ADD(offset, arrayBase);   /* add r1, r1, r0: carry/overflow out */
    R2 = 0x708;
    ST16(entry + 6, R2);

    RETURN_TO_CALLER();
}

/* ── object_setPanelsFromCoordinates ── */
/* object_setPanelsFromCoordinates(r5 = battle object):
 * Convert the object's world position (X/Y/Z) into battle panel coordinates
 * and store them in its PanelX/PanelY fields. R0/R1 return the panel
 * coordinates, R2 returns unchanged from the helper; flags are whatever
 * convertXYToPanelXY_800E258 leaves behind. */
Next object_setPanelsFromCoordinates(void) {
    u32 object = R5;

    R0 = LD32(object + oBattleObject_X);
    R1 = LD32(object + oBattleObject_Y);
    R2 = LD32(object + oBattleObject_Z);

    GUEST_CALL(convertXYToPanelXY_800E258);

    ST8(object + oBattleObject_PanelX, R0);
    ST8(object + oBattleObject_PanelY, R1);

    RETURN_TO_CALLER();
}

/* ── object_subtractHP ── */
/* object_subtractHP(r0 = damage, r5 = battle object):
 * subtract r0 from the object's HP, clamping at zero.
 * Leaves r1 = resulting HP; flags c/v reflect the subtraction. */
Next object_subtractHP(void) {
    u32 oldHP = LD16(R5 + oBattleObject_HP);
    u32 damage = R0;

    u32 newHP = oldHP - damage;
    SET_FLAGS_CMP(oldHP, damage); /* SUBS r1, r1, r0: c = no borrow, v = signed overflow */

    if ((s32)oldHP < (s32)damage) /* bge not taken: HP would go negative */
        newHP = 0;

    ST16(R5 + oBattleObject_HP, newHP);
    R1 = newHP;
    RETURN_TO_CALLER();
}

/* ── object_updatePanelParameters ── */
/* object_updatePanelParameters(r0..r1, r4..r7, r10 = object/panel context):
 * Thin trampoline. The original loads the address of the real body into r2,
 * does the classic "mov lr, pc; bx r2" long-call into it, and pops PC, so the
 * observable effect of this wrapper is:
 *   - r2 = address of _object_updatePanelParameters | 1 (Thumb bit set)
 *   - a call to _object_updatePanelParameters with r0..r1, r4..r7, r10 passed
 *     straight through; its results in r0..r7 (and flags) become ours.
 * LR is clobbered by the long-call idiom, which matches the contract. */
Next object_updatePanelParameters(void) {
    /* 0800C92A: ldr r2, off_800C934 // =_object_updatePanelParameters+1 */
    R2 = (A__object_updatePanelParameters | 1);

    /* 0800C92C/0800C92E: mov lr, pc; bx r2 -- call the real body */
    GUEST_CALL(_object_updatePanelParameters);

    /* 0800C930: pop {pc} -- return to the original caller */
    RETURN_TO_CALLER();
}

/* ── sub_800B884 ── */
/* sub_800B884(r0 = object, r1 = value):
 * Resolve an address via byte_203CF00_getEntryPtr_800BF5C (passing r0 through unchanged),
 * then store the saved byte value at [result + 1].
 * Returns r0 = byte_203CF00_getEntryPtr_800BF5C's result, r1 unchanged; flags are whatever
 * byte_203CF00_getEntryPtr_800BF5C leaves behind. */
Next sub_800B884(void) {
    u32 value = R1;

    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);

    ST8(R0 + 1, value);
    R1 = value;
    RETURN_TO_CALLER();
}

/* ── sub_800B892 ── */
/* sub_800B892(r0 = object pointer): return the byte at offset 1 of
 * whatever byte_203CF00_getEntryPtr_800BF5C resolves the object to (its type/flag field). */
Next sub_800B892(void) {
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    R0 = LD8(R0 + 1);
    RETURN_TO_CALLER();
}

/* ── battle_resetObjAnimState_800B89C ── */
/* battle_resetObjAnimState_800B89C(r0 = battle object):
 * Clears an object's animation state. byte_203CF00_getEntryPtr_800BF5C resolves the object's
 * sprite/animation entry, then the byte at +1 (current animation frame) and
 * the word at +8 (animation timer/counter) are reset to 0.
 *
 * Flags: the only flag-setting instructions here are the two Thumb "mov r1,#0"
 * encodings (MOVS, which only affects n/z), so c and v are left exactly as
 * byte_203CF00_getEntryPtr_800BF5C returned them. The stores below set no flags, preserving that. */
Next battle_resetObjAnimState_800B89C(void) {
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C); /* r0 passes through unchanged as the argument */

    u32 anim = R0;

    ST8(anim + 1, 0);
    ST32(anim + 8, 0);

    RETURN_TO_CALLER();
}

/* ── writeByteToWorkSlots0And1_800B8AC ── */
/* writeByteToWorkSlots0And1_800B8AC(r0 = value):
 * Store the given byte into two consecutive single-byte state slots,
 * fetched via byte_203CF00_getEntryPtr_800BF5C with slot indices 0 and 1. r4 is preserved. */
Next writeByteToWorkSlots0And1_800B8AC(void) {
    u32 value = R0;

    R0 = 0;
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    ST8(R0, value);

    R0 = 1;
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    ST8(R0, value);

    RETURN_TO_CALLER();
}

/* ── sub_800B8C2 ── */
/* sub_800B8C2(r0 = slot/index):
 * Calls byte_203CF00_getEntryPtr_800BF5C with the given argument, reads the byte it points to, and
 * reports whether that byte matches the original argument.
 * Returns r0 = 1 if equal, 0 otherwise; r2 is left holding the original argument. */
Next sub_800B8C2(void) {
    u32 arg = R0;

    R0 = arg;
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);

    u8 value = LD8(R0);

    R2 = arg;                      /* original's `pop {r2}` restores the saved r0 */
    R0 = (value == arg) ? 1 : 0;

    RETURN_TO_CALLER();
}

/* ── sub_800B8D8 ── */
/* sub_800B8D8(r0 = entry index):
 * Fetch the entry for this index via byte_203CF00_getEntryPtr_800BF5C and compare its byte at
 * offset 3 (the entry's type/kind field) against the index we were given.
 * Returns r0 = 1 on a match, 0 otherwise; r2 is left holding the index and
 * the flags reflect the comparison (CMP) so callers can branch directly.
 */
Next sub_800B8D8(void) {
    u32 index = R0;

    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    u32 entry = R0;

    u32 kind = LD8(entry + 3);

    R2 = index;
    SET_FLAGS_CMP(kind, index);
    R0 = (kind == index) ? 1 : 0;

    RETURN_TO_CALLER();
}

/* ── byte_203CF00_getEntryPtr_800BF5C ── */
/* byte_203CF00_getEntryPtr_800BF5C(r0 = index):
 * Convert a slot index into an address within the byte_203CF00 array,
 * which has 0x50 (80) bytes per entry.
 * Returns: r0 = byte_203CF00 + index * 0x50, r1 = byte_203CF00,
 * flags set by the final address computation (ADDS). */
Next byte_203CF00_getEntryPtr_800BF5C(void) {
    u32 base = A_byte_203CF00;

    u32 result = R0 * 0x50 + base;

    SET_FLAGS_ADD(R0 * 0x50, base);

    R0 = result;
    R1 = base;
    RETURN_TO_CALLER();
}

/* ── getTable12Record_800D730_800C6D6 ── */
/* getTable12Record_800D730_800C6D6(r0 = table index):
 * Returns the address of entry r0 in a table of 12-byte records at byte_800D730.
 * Output: r0 = byte_800D730 + r0 * 12. */
Next getTable12Record_800D730_800C6D6(void) {
    R0 = R0 * 12 + A_byte_800D730;
    RETURN_TO_CALLER();
}

/* ── battle_getPanelDataPtr_800C6E0 ── */
/* Panel geometry on the battle field: 8 columns per row, and each panel's
 * data entry is 0x20 bytes inside ePanelData. */
#define PANELS_PER_ROW 8
#define PANEL_SIZE     0x20

/* battle_getPanelDataPtr_800C6E0(r0 = panel column, r1 = panel row):
 * compute the address of a panel's data entry in ePanelData.
 * Returns r0 = &ePanelData[(row * 8 + column) * 0x20]; r2 is left at 8
 * (a leftover scratch value callers rely on as an output). */
Next battle_getPanelDataPtr_800C6E0(void) {
    u32 panelIndex = R0 + R1 * PANELS_PER_ROW;

    R0 = panelIndex * PANEL_SIZE + A_ePanelData;
    R2 = PANELS_PER_ROW;

    RETURN_TO_CALLER();
}

/* ── sub_800E45E ── */
/* sub_800E45E: tail-call wrapper that invokes sub_800E468 with r0 = 1.
 * All other registers (and the flags left behind by the callee) pass through. */
Next sub_800E45E(void) {
    R0 = 1;
    GUEST_CALL(sub_800E468);
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

/* ── GetOWPlayerFacingDirection ── */
/* GetOWPlayerFacingDirection: return the direction the overworld player sprite
 * is currently facing (0..3), read from the player's overworld object. */
Next GetOWPlayerFacingDirection(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 playerObject = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = LD8(playerObject + oOWPlayerObject_FacingDirection);
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

/* ── SetOWPlayerNaviPaletteIndex ── */
/* SetOWPlayerNaviPaletteIndex(r0 = navi palette index):
 * stores the overworld player navi's palette index into the eStruct2000aa0
 * struct (field byte_2000AA5), leaving r3 pointing at that struct. */
Next SetOWPlayerNaviPaletteIndex(void) {
    R3 = A_eStruct2000aa0;
    ST8(R3 + 5 /* byte_2000AA5 - 0x2000aa0 */, R0);
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

/* ── owPlayer_setS2000AA0Param0x4_809e314 ── */
/* owPlayer_setS2000AA0Param0x4_809e314(r0 = sprite id):
 * Store the overworld player's current sprite id into byte 4 of the
 * eStruct2000aa0 struct (byte_2000AA4). Leaves the struct's address in r3. */
Next owPlayer_setS2000AA0Param0x4_809e314(void) {
    R3 = A_eStruct2000aa0;
    ST8(R3 + 4, R0);
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

/* ── returnZero_809E228 ── */
/* returnZero_809E228: always returns 0 (likely a stub/callback). */
Next returnZero_809E228(void) {
    R0 = 0;
    RETURN_TO_CALLER();
}

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

/* ── spriteLoadMugshot_800275A ── */
/* spriteLoadMugshot_800275A(r0 = mugshot index, r5 = sprite struct):
 * initialize the mugshot sprite for the given index and mark r5's
 * halfword at +0x16 as 0x80 (sprite loaded). */
Next spriteLoadMugshot_800275A(void) {
    R0 = LD32(A_mugshotSpritePtrs + R0 * 4);
    GUEST_CALL(sprite_initialize);
    ST16(R5 + 0x16, 0x80);
    RETURN_TO_CALLER();
}

/* ── sprite_chatbox_80026D6 ── */
/* sprite_chatbox_80026D6: trampoline that jumps to _sprite_update.
 * The sprite update routine is entered with r4 holding its own address
 * (thumb bit set) so it can find its data tables; this thunk sets that
 * up, then on return restores the caller's r4 and returns straight to
 * the caller (pop {r4,pc} uses the lr saved at entry). */
Next sprite_chatbox_80026D6(void) {
    u32 callerR4 = R4;

    R4 = (A__sprite_update | 1);   /* ldr r4, =_sprite_update+1 */
    GUEST_CALL(_sprite_update);    /* mov lr, pc; bx r4 */

    R4 = callerR4;                 /* pop {r4} */
    RETURN_TO_CALLER();            /* pop {pc} */
}

/* ── sprite_getColorShader ── */
/* sprite_getColorShader(r0 = object header pointer):
 * Returns the color/palette shader halfword of the object's sprite. The sprite
 * data starts at the address given by the high nibble of the header's
 * TypeAndSpriteOffset field (a multiple-of-16 byte offset from the header),
 * and the shader value lives at +6 within it. Result in r0. */
Next sprite_getColorShader(void) {
    u32 header = R0;

    u32 spriteOffset = (LD8(header + 2) >> 4) << 4;
    R0 = LD16(header + spriteOffset + 6);

    RETURN_TO_CALLER();
}

/* ── sprite_getFinalPalette ── */
/* Sprite palette lookup helpers. The object header's TypeAndSpriteOffset
 * byte is at offset 2; its upper nibble is the sprite's VRAM tile offset,
 * which indexes the object's sprite record list. oObjectSprite_Unk_15 is
 * the final palette byte of a sprite record. */
#define oObjectHeader_TypeAndSpriteOffset 2

/* sprite_getFinalPalette(r0 = object header pointer):
 * Compute the palette used by an object's current sprite. The header's
 * TypeAndSpriteOffset byte holds the sprite's VRAM offset (in 32-byte
 * tiles) in its upper nibble; that offset locates the sprite's record,
 * whose Unk_15 byte supplies the final palette index. Result: r1 = palette. */
Next sprite_getFinalPalette(void) {
    u32 spriteOffset = LD8(R0 + oObjectHeader_TypeAndSpriteOffset) & 0xF0;
    u32 sprite = R0 + spriteOffset;

    R1 = LD8(sprite + oObjectSprite_Unk_15) & 0xF0;
    RETURN_TO_CALLER();
}

/* ── sprite_getFlip ── */
/* sprite_getFlip(r0 = object header pointer):
 * Returns the flip value (0-3) stored in bits 4-5 of the sprite's Unk_16
 * field. The sprite offset byte's low nibble is masked off (the offset is
 * always a multiple of 16), then used to step from the header to the
 * sprite struct.
 * Outputs: r0 = flip value. */
Next sprite_getFlip(void) {
    u32 header = R0;

    /* oObjectHeader_SpriteOffset (byte at +2): sprite data offset, 16-aligned */
    u32 sprite = header + (LD8(header + 2) & 0xF0);
    R0 = (LD8(sprite + oObjectSprite_Unk_16) & 0x30) >> 4;

    RETURN_TO_CALLER();
}

/* ── sprite_getFrameParameters ── */
/* sprite_getFrameParameters(r5 = object header):
 * Reads the sprite parameters of the object's current animation frame.
 * Computes the sprite data address from the object's type/sprite-offset byte
 * at header +2 (low nibble dropped, so the offset is a multiple of 0x10), then
 * loads:
 *   r0 = attribute byte at sprite +2 (with the 0xC0 flip bits masked off
 *        when the byte at sprite +1 is nonzero),
 *   r1 = r2 = frame index byte at sprite +0,
 *   r3 = sprite data address.
 * Flags: the CMP against #0 leaves c set and v clear, nz per the tested byte. */
Next sprite_getFrameParameters(void) {
    u32 sprite = R5 + (LD8(R5 + 2) & 0xF0);
    u32 flipTest = LD8(sprite + 1);
    u32 attrs = LD8(sprite + 2);

    SET_FLAGS_CMP(flipTest, 0); /* c = 1, v = 0 regardless of the branch below */
    if (flipTest != 0)
        attrs &= ~0xC0u; /* clear the h/v flip bits */

    R0 = attrs;
    R1 = LD8(sprite);
    R2 = R1; /* the original loads the same byte twice */
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* ── sprite_getMosaicScalingParameters ── */
/* sprite_getMosaicScalingParameters(r0 = object header, r10 = toolkit):
 * Looks up the mosaic/scaling entry for the object's sprite.
 *   r0 = parameter byte from toolkit->Unk2009740[2],
 *   r2 = mosaic flag bits (mask 0x0C) from the sprite's Unk_11 field. */
Next sprite_getMosaicScalingParameters(void) {
    u32 obj = R0;

    /* The high nibble of the header's type byte selects the sprite
     * attached to the object, in 16-byte units from the header. */
    u32 sprite = obj + (LD8(obj + 2) & 0xF0);

    R2 = LD8(sprite + oObjectSprite_Unk_11) & 0xC;

    u32 params = LD32(R10 + oToolkit_Unk2009740_Ptr);
    R0 = LD8(params + 2);

    RETURN_TO_CALLER();
}

/* ── sprite_getPalette ── */
/* sprite_getPalette(r0 = object header pointer):
 * The header's type byte (offset 2) holds the sprite's offset from the
 * header in its high nibble (in units of 0x10 bytes). Add that to the
 * header to find the sprite data, and return its palette byte at +4.
 * Outputs: r0 = palette byte, r3 = sprite data pointer. */
Next sprite_getPalette(void) {
    u32 header = R0;

    u32 spriteOffset = (LD8(header + 2 /* oObjectHeader_TypeAndSpriteOffset */) >> 4) << 4;
    u32 spriteData = header + spriteOffset;

    R3 = spriteData;
    R0 = LD8(spriteData + 4 /* oObjectSprite_Unk_04 */);
    RETURN_TO_CALLER();
}

/* ── sprite_getUnk0x2c ── */
/* sprite_getUnk0x2c(r5 = object):
 * The object header's TypeAndSprite field (offset 2) packs the sprite's
 * offset within the object into its high nibble; use it (as a multiple of
 * 0x10) to reach the object's sprite record and return its Unk_2c word.
 * The ADD is the last flag-setting instruction, so carry is produced. */
Next sprite_getUnk0x2c(void) {
    u32 spriteOffset = LD8(R5 + 2) & 0xF0;

    R0 = LD32(R5 + spriteOffset + oObjectSprite_Unk_2c);
    SET_FLAGS_ADD(spriteOffset, R5);

    RETURN_TO_CALLER();
}

/* ── sprite_hasShadow ── */
/* Object struct offsets used here (from the disassembly; the shared header
 * doesn't export them, so define them locally):
 *   +2: type/sprite-select byte -- upper nibble picks which 0x10-byte sprite
 *       struct belongs to this object
 *   +3: sprite state byte within that struct */
#define oObjectHeader_TypeAndSpriteOffset_sprite_hasShadow 2
#define oObjectSprite_Unk_03              3

/* sprite_hasShadow(r5 = object):
 * Mark the object's current sprite as casting a shadow: set bit 0 and clear
 * bit 2 of the sprite state byte.
 * Leaves r0 = new state byte, r1 = 4, r3 = sprite struct address.
 * Flags: N/Z come from the stored byte (the final BIC), while C/V come from
 * the earlier ADDS that built the sprite address -- BIC/ORR leave C/V alone. */
Next sprite_hasShadow(void) {
    u32 select = (LD8(R5 + oObjectHeader_TypeAndSpriteOffset_sprite_hasShadow) >> 4) << 4;

    /* adds r3, r3, r5: this is where C and V are established */
    u32 sprite = select + R5;
    SET_FLAGS_ADD(select, R5);

    u32 state = LD8(sprite + oObjectSprite_Unk_03);
    state |= 1;   /* has shadow */
    state &= ~4;  /* clear the "no shadow" flag */
    ST8(sprite + oObjectSprite_Unk_03, state);
    /* bic only updates N/Z; C/V from the ADDS above must survive */
    SET_FLAGS_NZ(state);

    R0 = state;
    R1 = 4;
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* ── sprite_loadAnimationData ── */
/* sprite_loadAnimationData(r5 = object header, r4 = scratch):
 * Loads the animation data for the sprite attached to an object header.
 * The high nibble of the header's type/sprite-offset byte (at offset 2) is
 * an offset in 16-byte units; r5 is advanced past the header to the sprite
 * data and the shared animation loader is run on it. r4/r5 are preserved
 * for the caller (the original saves them on the stack around the call);
 * r0-r3 and the flags are whatever the loader leaves behind. */
Next sprite_loadAnimationData(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    /* Skip past the object header to this sprite's data block:
     * high nibble of the type/sprite-offset byte, times 16. */
    u32 offset = (LD8(R5 + 2) >> 4) << 4;
    R5 = savedR5 + offset;

    /* Hand the loader its own entry address (Thumb bit set), exactly as the
     * original computed-call does. */
    R4 = (A__sprite_loadAnimationData | 1);

    GUEST_CALL(_sprite_loadAnimationData);

    /* Restore the caller's r4/r5, as the original's pop does. */
    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sprite_loadAnimationData_spritePtrProvidedDirectly ── */
/* sprite_loadAnimationData_spritePtrProvidedDirectly (0x080026B6):
 * Thin trampoline around _sprite_loadAnimationData. The callee receives the
 * caller's r4/r5 untouched and returns its results in r2/r3; this wrapper
 * exists only so that r4 survives the call (the original pushes/pops r4
 * around the branch). */
Next sprite_loadAnimationData_spritePtrProvidedDirectly(void) {
    u32 savedR4 = R4;

    GUEST_CALL(_sprite_loadAnimationData);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── sprite_noShadow ── */
/* sprite_noShadow(r5 = object):
 * Marks the object's current sprite as having no shadow. The sprite record
 * lives at obj + (oObjectHeader_SpriteOffset & ~0xF) (offsets 0x02 in the
 * header, sprite records are 16-byte aligned), and the flag byte at offset 3
 * of the sprite (the "Unk_03" attribute) gets bit 2 set and bit 0 cleared.
 *
 * Returns:
 *   r0 = updated attribute byte, r1 = 1, r3 = address of the sprite record
 *   nz = from the final BIC on the attribute byte; c/v = from the ADDS that
 *   forms the sprite address (logical ops in Thumb don't touch c/v). */
Next sprite_noShadow(void) {
    u32 offset = LD8(R5 + 2);               /* oObjectHeader_SpriteOffset */
    u32 sprite = R5 + (offset & ~0xFu);     /* round down to 16-byte record */
    SET_FLAGS_ADD(sprite, R5);

    u32 attrs = LD8(sprite + 3);            /* oObjectSprite_Unk_03 */
    attrs |= 4;                             /* no-shadow bit on */
    attrs &= ~1u;                           /* bit 0 off */
    SET_FLAGS_NZ(attrs);

    ST8(sprite + 3, attrs);

    R0 = attrs;
    R1 = 1;
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* ── sprite_removeShadow ── */
/* Offsets from the disassembly that game.h doesn't define. */
#define oObjectHeader_TypeAndSpriteOffset_sprite_removeShadow 2 /* header byte: high nibble = sprite struct index */
#define oObjectSprite_Unk_03_sprite_removeShadow              3 /* sprite status byte */

/* sprite_removeShadow(r5 = object header):
 * The high nibble of the header's type byte selects the object's sprite
 * struct (scaled by 16 as the offset from the header). This clears the
 * shadow bit (bit 0) and the bit-2 flag in that sprite's status byte.
 * Leaves r0 = new status byte, r1 = 4, r3 = sprite struct address,
 * nz from the status byte and c/v from the address add. */
Next sprite_removeShadow(void) {
    u32 spriteOffset = LD8(R5 + oObjectHeader_TypeAndSpriteOffset_sprite_removeShadow) & 0xF0;
    u32 sprite = spriteOffset + R5;
    SET_FLAGS_ADD(spriteOffset, R5); /* flags of the original ADDS r3, r3, r5 */

    /* Drop the shadow flag and the bit-2 flag from the sprite status byte. */
    u32 status = LD8(sprite + oObjectSprite_Unk_03_sprite_removeShadow) & ~0x05u;
    SET_FLAGS_NZ(status);
    ST8(sprite + oObjectSprite_Unk_03_sprite_removeShadow, status);

    R0 = status;
    R1 = 4;
    R3 = sprite;
    RETURN_TO_CALLER();
}

/* ── sprite_setAlpha_8002c7a ── */
/* sprite_setAlpha_8002c7a(r0 = alpha, r2 = -, r5 = sprite object, r7 = preserved,
 *                         r10 = toolkit):
 * Trampoline for the shared sprite effect setter: select effect mode 4
 * (per-sprite blend/alpha mode in the object's sprite record) and tail-call
 * sprite_setAlpha, which stores the mode into the object's sprite entry and
 * reprograms the global blend block pointed at by the toolkit (blend mode
 * bits cleared, coefficient A = alpha, coefficient B = 0x10 - alpha, first
 * target bank emptied, second bank filled with 0x3F). All outputs
 * (r0 = 0x3F, r1 = masked target byte, r2 = 0, r3 = blend block, nz/c/v)
 * come from the callee; r5 and r10 pass through untouched and r7 is
 * preserved by it. */
Next sprite_setAlpha_8002c7a(void) {
    R2 = 4;
    GUEST_CALL(sprite_setAlpha);
    RETURN_TO_CALLER();
}

/* ── sprite_setAnimation ── */
/* sprite_setAnimation(r0 = animation index, r5 = object header):
 * Select an animation for the object's sprite. The high nibble of the
 * object's TypeAndSpriteOffset byte (offset 2) holds the sprite slot
 * number; each slot is 16 bytes, so the slot's base is obj + (nibble << 4).
 * The new animation index is written to that slot's first byte. */
Next sprite_setAnimation(void) {
    u32 obj = R5;

    u32 slotOffset = LD8(obj + 2) & 0xF0;
    ST8(obj + slotOffset, R0);

    RETURN_TO_CALLER();
}

/* ── sprite_setCoordinates ── */
/* sprite_setCoordinates(r0 = x, r1 = y, r5 = sprite resource pointer):
 * Byte 2 of the resource holds an index whose high nibble selects a
 * sub-entry 0x10 bytes into the resource; store the coordinates into
 * that entry's position fields.
 * Returns the selected entry address in r3, flags from its computation. */
Next sprite_setCoordinates(void) {
    u32 entry = R5 + (LD8(R5 + 2) & 0xF0);

    ST16(entry + 0xC, R0); /* x */
    ST16(entry + 0xE, R1); /* y */

    R3 = entry;
    SET_FLAGS_ADD(entry, R5);
    RETURN_TO_CALLER();
}

/* ── sprite_setUnk0x2c ── */
/* sprite_setUnk0x2c(r0 = value, r5 = object header):
 * The object header's TypeAndSpriteOffset byte (offset 2) holds the sprite ID
 * in its high nibble. Compute the address of the object's sprite struct from
 * that ID and store the value into its field at oObjectSprite_Unk_2c.
 * Leaves r3 pointing at that sprite struct; flags come from the address add. */
Next sprite_setUnk0x2c(void) {
    u32 typeAndSprite = LD8(R5 + 2);            /* oObjectHeader_TypeAndSpriteOffset */
    u32 spriteId = (typeAndSprite >> 4) << 4;   /* keep only the sprite ID nibble */

    R3 = spriteId + R5;
    SET_FLAGS_ADD(spriteId, R5);

    ST32(R3 + oObjectSprite_Unk_2c, R0);
    RETURN_TO_CALLER();
}

/* ── sprite_zeroColorShader ── */
/* sprite_zeroColorShader(r5 = object header):
 * Compute the address of the object's sprite data (the sprite index in the
 * header's type/sprite byte selects which oObjectSprite struct follows the
 * header) and clear the halfword at offset +6 of that struct, resetting the
 * sprite's color shader / palette effect.
 * Returns r3 = address of the sprite data; c/v reflect the final ADD. */
Next sprite_zeroColorShader(void) {
    /* The low byte at +2 holds the sprite index in its high nibble; the
     * structs are 16 bytes apart, so shift left/right 4 to scale it. */
    u32 sprite = R5 + ((LD8(R5 + 2) >> 4) << 4);

    ST16(sprite + 6, 0);

    R3 = sprite;
    R0 = 0;
    SET_FLAGS_ADD(sprite, R5);
    RETURN_TO_CALLER();
}

/* ── sub_80027B4 ── */
/* sub_80027B4: set the flag at dword_200F350 (A_eS200F348_Size) to 1.
 * Probably marks some sprite/resource table as loaded/available. */
Next sub_80027B4(void) {
    ST32(A_eS200F348_Size, 1);
    RETURN_TO_CALLER();
}

/* ── palette_InitTransferState_80027D4 ── */
/* palette_InitTransferState_80027D4: initialize the palette-transfer state at byte_20098A8.
 * Sets the entry size to 4, the entry count to 10, and the halfword
 * transfer length to 0x2FF for the upcoming palette DMA copy. */
Next palette_InitTransferState_80027D4(void) {
    u32 state = A_ePalette20097a0_21_Size;  /* = byte_20098A8 */

    ST8(state + 1, 4);        /* byte_20098A9 = 4  */
    ST8(state, 0xA);          /* byte_20098A8 = 10 */
    ST16(state + 2, 0x2FF);   /* word_20098AA = 0x2FF */

    RETURN_TO_CALLER();
}

/* ── sub_800286C ── */
/* sub_800286C: clear the 32-bit global dword_200F340 to zero (some
 * global counter/flag reset; no results, no flags). */
Next sub_800286C(void) {
    ST32(A_dword_200F340, 0);
    RETURN_TO_CALLER();
}

/* ── lookupValueForIdInTableAt_200DCA0_8002986 ── */
/* lookupValueForIdInTableAt_200DCA0_8002986(r0 = id, r5 preserved):
 * Search the 12-entry table rooted at byte_200DCA0. Each entry i has a
 * u16 id at table+4+i*2 and a u32 payload at table+0x1C+i*4. Returns the
 * payload of the entry whose id matches r0, or 0 if no entry matches.
 * r1/r2 are scratch in the original; they are set to the same values the
 * original leaves behind (last compared id / loop index) for exactness. */
Next lookupValueForIdInTableAt_200DCA0_8002986(void) {
    u32 table = A_byte_200DCA0;
    u32 key = R0;
    u32 result = 0;
    u32 found = 0;
    u32 lastId = 0;
    u32 i;

    for (i = 0; i < 12; i++) {
        lastId = (u16)LD16(table + 4 + i * 2);
        if (lastId == key) {
            result = LD32(table + 0x1C + i * 4);
            found = 1;
            break;
        }
    }

    if (found) {
        R0 = result;
        R1 = 0x1C + i * 4;      /* address offset used by the original's load */
        R2 = i;
    } else {
        R0 = 0;
        R1 = lastId;            /* id of the final (12th) entry */
        R2 = 12;
    }

    RETURN_TO_CALLER();
}

/* ── obj_getSpriteFlag10AndRenderInfoHigh_8002F3E ── */
/* obj_getSpriteFlag10AndRenderInfoHigh_8002F3E(r0 = object header, r10 = toolkit):
 *   r0 = high byte of the render info halfword at renderInfo+2
 *        (low 8 bits masked off, then shifted down),
 *   r2 = bit 4 (0x10) of the sprite's Unk_11 flag byte. */
Next obj_getSpriteFlag10AndRenderInfoHigh_8002F3E(void) {
    u32 object = R0;

    /* The sprite struct for this object lives at object + (spriteOffset & 0xF0):
     * the header byte at +2 holds its index in the low nibble and its page in bits 4-7. */
    u32 spriteOffset = LD8(object + 2); /* oObjectHeader_SpriteOffset */
    u32 sprite = object + (spriteOffset & 0xF0);

    u32 flag = LD8(sprite + 0x11) & 0x10; /* oObjectSprite_Unk_11 */

    u32 renderInfo = LD32(R10 + 8); /* oToolkit_RenderInfoPtr */
    u32 value = LD16(renderInfo + 2) & ~0xFFu; /* oRenderInfo_Unk_02 */

    R0 = value >> 8;
    R2 = flag;
    RETURN_TO_CALLER();
}

/* ── object_writeSubstructField30_8002FEC ── */
/* object_writeSubstructField30_8002FEC(r0 = value, r5 = object):
 * Store `value` into a field of one of the sub-structures attached to `object`.
 * The high nibble of the byte at object+2 selects which sub-structure
 * (index * 0x10 bytes), and the value goes to its field at +0x30.
 * Returns the address written to in r3; the v flag reflects the address
 * computation (ADDS), as callers expect. */
Next object_writeSubstructField30_8002FEC(void) {
    u32 base = R5;
    u32 offset = (LD8(base + 2) >> 4) << 4; /* high nibble -> byte offset */
    u32 target = offset + base;

    ST32(target + 0x30, R0);

    R3 = target;
    SET_FLAGS_ADD(offset, base);
    RETURN_TO_CALLER();
}

/* ── sprite_setSubEntryPaletteBank_800304A ── */
/* sprite_setSubEntryPaletteBank_800304A(r0 = palette bank, r5 = object base):
 * The high nibble of the object's byte at +2 selects one 0x10-byte
 * sub-entry within the object; store the palette bank (r0) into the high
 * nibble of that sub-entry's byte at +0x15, leaving its low nibble intact.
 * Leaves r0 = bank << 4, r1 = new byte value, r2 = 0xF0, r3 = byte address.
 * Flags: NZ from the stored value, C from the carry-out of the LSL #4,
 * V from the pointer addition. */
Next sprite_setSubEntryPaletteBank_800304A(void) {
    u32 slot = LD8(R5 + 2) & 0xF0;   /* sub-entry index * 0x10 */
    u32 addr = R5 + slot;

    SET_FLAGS_ADD(slot, R5);         /* adds r3, r3, r5 */

    R2 = 0xF0;
    R1 = LD8(addr + 0x15) & ~R2;     /* bic: clear palette nibble */

    cpu.c = (R0 >> 28) & 1;          /* carry out of lsl r0, r0, #4 */
    R0 <<= 4;

    R1 |= R0;                        /* orr: insert new palette nibble */
    SET_FLAGS_NZ(R1);

    ST8(addr + 0x15, R1);
    R3 = addr;
    RETURN_TO_CALLER();
}

/* ── sprite_setSubRecordFieldA_8003060 ── */
/* sprite_setSubRecordFieldA_8003060(r0 = value, r5 = sprite object):
 * Store r0 into a field of the sprite object's 16-byte-aligned record.
 * The object holds a bank/index in bits 4-7 of its byte at +2; masking it
 * back down selects the base of the record, and the value is written to
 * that record's byte at +0xA. r3 is left pointing at the record base.
 * Flags: c/v come from the ADD that forms the record base. */
Next sprite_setSubRecordFieldA_8003060(void) {
    u32 record = R5 + ((LD8(R5 + 2) >> 4) << 4);

    ST8(record + 0xA, R0);

    R3 = record;
    SET_FLAGS_ADD(record, R5);
    RETURN_TO_CALLER();
}

/* ── slotList_storeHalfwordAndClearField_800306C ── */
/* slotList_storeHalfwordAndClearField_800306C(r0 = value, r5 = list base):
 * The high nibble of the byte at r5+2 selects a 16-byte slot in the list.
 * Store r0 as a halfword at slot+8 and clear the 32-bit field at slot+0x24
 * (which extends into the following slot). */
Next slotList_storeHalfwordAndClearField_800306C(void) {
    u32 slot = R5 + (LD8(R5 + 2) & 0xF0);

    ST16(slot + 8, R0);
    ST32(slot + 0x24, 0);

    RETURN_TO_CALLER();
}

/* ── crt0_entry_trampoline_8000288 ── */
/* crt0_entry_trampoline_8000288: trampoline that invokes sub_3005E2C as a leaf call and
 * returns straight back to our own caller. sub_3005E2C's results in
 * r2/r3 are passed through unchanged; r4 is untouched. */
Next crt0_entry_trampoline_8000288(void) {
    GUEST_CALL(sub_3005E2C);
    RETURN_TO_CALLER();
}
