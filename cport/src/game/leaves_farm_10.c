/*
 * leaves_farm_10.c: small functions from the model farm, set 10.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── copyWords_80014EC ── */
/* copyWords_80014EC(r0 = source, r1 = destination, r2 = byte count):
 * word-aligned memmove. Copies forwards normally, or backwards when the
 * source sits below the destination (overlapping copy). r0-r7 and lr are
 * all preserved; the flags reflect the final loop-counter subtraction. */
Next copyWords_80014EC(void) {
    u32 src = R0;
    u32 dst = R1;
    u32 size = R2;

    if ((s32)src < (s32)dst) {
        /* Destination is above the source: start from the last word and
         * walk backwards so the overlap is copied correctly. */
        size -= 4;
        src += size;
        dst += size;
        do {
            ST32(dst, LD32(src));
            src -= 4;
            dst -= 4;
            SET_FLAGS_CMP(size, 4);
            size -= 4;
        } while ((s32)size >= 0);
    } else {
        /* Safe to copy from the first word upwards. */
        do {
            ST32(dst, LD32(src));
            src += 4;
            dst += 4;
            SET_FLAGS_CMP(size, 4);
            size -= 4;
        } while ((s32)size > 0);
    }

    RETURN_TO_CALLER();
}

/* ── map_lookupMapIdByGroupAndNumber_8001708 ── */
/* map_lookupMapIdByGroupAndNumber_8001708(R0 = map group, R1 = map number):
 * Selects the real-world group table for groups below INTERNET_MAP_GROUP_START
 * and the internet table for the rest (subtracting the base so the index fits),
 * then loads the byte for (group, number) and hands it to sub_80015FC.
 * Flags and r2/r3 on return are whatever sub_80015FC leaves. */
Next map_lookupMapIdByGroupAndNumber_8001708(void) {
    u32 group = R0;
    u32 table;

    if ((s32)group >= INTERNET_MAP_GROUP_START) {
        table = A_off_8033878;              /* [const u8 *; REAL_WORLD_NUM_GROUPS] for internet groups */
        group -= INTERNET_MAP_GROUP_START;
    } else {
        table = A_off_803385C;              /* [const u8 *; REAL_WORLD_NUM_GROUPS] for real-world groups */
    }

    /* Each group entry is a pointer to a per-map-number byte. */
    R3 = LD32(table + group * 4) + R1;
    R0 = LD8(R3);

    GUEST_CALL(sub_80015FC);
    RETURN_TO_CALLER();
}

/* ── renderInfo_8001788 ── */
/* renderInfo_8001788: zero out the sprite attribute fields of the toolkit's
 * RenderInfo block (r10 = toolkit pointer). The pointer sits at offset 8 in
 * the toolkit struct (the disassembly's RenderInfoPtr slot). Clears every
 * halfword counter/offset used when building the render list, so the next
 * frame starts clean. */
Next renderInfo_8001788(void) {
    u32 renderInfo = LD32(R10 + 8);

    ST16(renderInfo + oRenderInfo_Unk_0c, 0);
    ST16(renderInfo + oRenderInfo_Unk_0e, 0);
    ST16(renderInfo + oRenderInfo_Unk_10, 0);
    ST16(renderInfo + oRenderInfo_Unk_12, 0);
    ST16(renderInfo + oRenderInfo_Unk_14, 0);
    ST16(renderInfo + oRenderInfo_Unk_16, 0);
    /* the original writes 0x1A before 0x18; the order has no visible effect */
    ST16(renderInfo + oRenderInfo_Unk_1a, 0);
    ST16(renderInfo + oRenderInfo_Unk_18, 0);

    RETURN_TO_CALLER();
}

/* ── renderInfo_80017A0 ── */
/* renderInfo_80017A0: clear the halfword render-info counter (offset 2) of the
 * render info block pointed to by the current toolkit's field at offset 8.
 * Inputs: r10 = toolkit. Outputs: r1 = 0. */
Next renderInfo_80017A0(void) {
    u32 renderInfo = LD32(R10 + 8 /* oToolkit_RenderInfoPtr */);
    R1 = 0;
    ST16(renderInfo + 2 /* oRenderInfo_Unk_02 */, R1);
    RETURN_TO_CALLER();
}

/* ── sound_8000808 ── */
/* sound_8000808(r0-r3 = sound request):
 * Append a 16-byte sound request (the four words in r0-r3) to the request
 * queue whose address is stored at dword_200A490. The queue header word
 * (offset 0) is the number of pending requests; request slots are 16 bytes
 * each, starting at offset 0xC. If the queue already holds 0x20 requests,
 * the new request is silently dropped.
 *
 * Flag behaviour matches the original: when the queue is full the caller
 * sees the flags of "CMP count, #0x20"; otherwise those of the ADDS that
 * forms the slot address. r4-r7 are preserved. */
Next sound_8000808(void) {
    u32 queue = A_dword_200A490;
    u32 count = LD32(queue);

    if ((s32)count >= 0x20) {
        /* Queue full: drop the request. */
        SET_FLAGS_CMP(count, 0x20);
        RETURN_TO_CALLER();
    }

    /* Claim the next slot. */
    ST32(queue, count + 1);

    /* Slot address: queue base + count * 16 + 0xC (flags come from the
     * final add, as in the original's `adds r7, r7, r5`). */
    u32 slot = (count << 4) + 0xC;
    SET_FLAGS_ADD(slot, queue);
    u32 entry = queue + slot;

    ST32(entry + 0,  R0);
    ST32(entry + 4,  R1);
    ST32(entry + 8,  R2);
    ST32(entry + 12, R3);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_disable_collision ── */
/* NPCCommand_disable_collision(r5 = NPC object):
 * Sets the NPC's "disable collision" flag so the player can walk through it,
 * then advances the NPC script program counter (r6).
 * r0 = flag value, r1 = new flags word; nz/c/v come from the final "adds r6, #1". */

enum { OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION = 0x4 };

Next NPCCommand_disable_collision(void) {
    u32 flags = LD32(R5 + oOverworldNPCObject_UnkFlags_60);

    R0 = OW_NPC_UNK_FLAGS_60_DISABLE_COLLISION;
    R1 = flags | R0;
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, R1);

    R6 += 1;
    SET_FLAGS_ADD(R6, 1);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_disable_npc_interaction ── */
/* NPCCommand_disable_npc_interaction(r5 = NPC object, r6 = script instruction pointer):
 * sets the NPC's "disable interaction" flag and advances the script by one command.
 * Leaves r0 = flag bit, r1 = updated flags value, r6 = next instruction index. */
enum { OW_NPC_UNK_FLAGS_60_DISABLE_INTERACTION = 1 };

Next NPCCommand_disable_npc_interaction(void) {
    R0 = OW_NPC_UNK_FLAGS_60_DISABLE_INTERACTION;

    u32 flags = LD32(R5 + oOverworldNPCObject_UnkFlags_60);
    flags |= R0;
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, flags);
    R1 = flags;

    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_do_not_face_player_when_interacted ── */
/* NPCCommand_do_not_face_player_when_interacted(r5 = NPC object, r6 = script counter):
 * Sets the "face player when interacted" flag (bit 1 of UnkFlags_60) on the NPC
 * and advances the script counter. Flags come from the counter increment. */
enum { OW_NPC_UNK_FLAGS_60_FACE_PLAYER_WHEN_INTERACTED = 2 };

Next NPCCommand_do_not_face_player_when_interacted(void) {
    R0 = OW_NPC_UNK_FLAGS_60_FACE_PLAYER_WHEN_INTERACTED;
    R1 = LD32(R5 + oOverworldNPCObject_UnkFlags_60) | R0;
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, R1);
    R6 = R6 + 1;
    SET_FLAGS_ADD(R6, 1);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_give_detatched_shadow ── */
/* NPCCommand_give_detatched_shadow(r5 = NPC object, r6 = command stream position):
 * NPC script command that detaches the NPC's shadow: sets the shadow flags
 * (0x100 | 0x80) in the object's UnkFlags_60, then calls sprite_hasShadow to
 * set the detached shadow up. Advances the command stream position by one
 * (flags reflect the ADDS increment). */
Next NPCCommand_give_detatched_shadow(void) {
    /* ldr r0, =(...) — the shadow flag pair; only the 0x80 macro exists in this
     * build's headers, so spell the pair out as 0x100 | ..._FLAG_0x80 (== 0x180). */
    u32 shadowFlags = 0x100 | OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;

    u32 flags = LD32(R5 + oOverworldNPCObject_UnkFlags_60);
    ST32(R5 + oOverworldNPCObject_UnkFlags_60, flags | shadowFlags);

    /* sprite_hasShadow(r5 = NPC object, r6 = command stream position) */
    GUEST_CALL(sprite_hasShadow);

    /* add r6, #1 — the original's ADDS sets the flags */
    R6 = R6 + 1;
    SET_FLAGS_ADD(R6 - 1, 1);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_init_hop ── */
/* NPCCommand_init_hop(r5 = NPC object, r6 = current script command pointer):
 * "hop" script command — read the hop speed and hop distance from the two
 * bytes following the opcode, put the NPC into the hop/leap action state,
 * clear its movement flags, and disable its script. The script pointer is
 * then advanced past the 3-byte command (r6 += 3), and NZC are left from
 * that ADDS; r0 ends as whatever the script-disable call returned. */
enum { MOVEMENT_FLAG_HOP_OR_LEAP = 8 };

Next NPCCommand_init_hop(void) {
    ST8(R5 + 4 /* oOverworldNPCObject_MovementSpeed */, LD8(R6 + 1));
    ST8(R5 + 5 /* oOverworldNPCObject_MovementDistance */, LD8(R6 + 2));

    ST8(R5 + 9 /* oOverworldNPCObject_CurAction */, MOVEMENT_FLAG_HOP_OR_LEAP);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    GUEST_CALL(npc_disableScript0x19_809f51e);

    u32 oldPtr = R6;
    R6 = oldPtr + 3;
    SET_FLAGS_ADD(oldPtr, 3);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_init_movement ── */
/* NPCCommand_init_movement(r5 = NPC object, r6 = pointer to the "init movement"
 * command in the NPC's script):
 *
 * Command layout in the script:
 *   +0: opcode
 *   +1: movement direction
 *   +2: movement speed
 *   +3: movement distance
 *   +4: start of the next script command
 *
 * Copies the movement parameters into the NPC object, points the object's
 * script link register past this command, then reads the next script word
 * (the command's argument) via ReadNPCScriptWord.
 *
 * Outputs: r0 = next script word (also in r6), r1-r3 = whatever
 * ReadNPCScriptWord leaves, r4 = oOverworldNPCObject_MovementDistanceReload,
 * flags set from the new r6. */
Next NPCCommand_init_movement(void) {
    u32 npc = R5;
    u32 script = R6;

    /* the NPC's script resumes right after this command's fixed part */
    ST32(npc + oOverworldNPCObject_LinkRegister, script + 8);

    ST8(npc + oOverworldNPCObject_MovementDirectionReload, LD8(script + 1));
    ST8(npc + oOverworldNPCObject_MovementSpeedReload,     LD8(script + 2));
    ST8(npc + oOverworldNPCObject_MovementDistanceReload,  LD8(script + 3));

    R4 = oOverworldNPCObject_MovementDistanceReload;

    /* fetch the word following the command */
    R0 = script + 4;
    GUEST_CALL(ReadNPCScriptWord);

    R6 = R0;
    SET_FLAGS_ADD(R0, 0);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_move_in_direction ── */
/* Offsets/constants used here that aren't in game.h (from the bn6f disassembly). */
#define oOverworldNPCObject_MovementSpeed   0x04 /* NPC movement speed byte        */
#define oOverworldNPCObject_MovementDistance 0x05 /* NPC movement distance byte   */
#define oOverworldNPCObject_CurAction       0x09 /* NPC current action byte        */
#define MOVEMENT_FLAG_DEFAULT_MOVING        0x04 /* default "moving" action id     */

/* NPCCommand_move_in_direction (0x0809EEDA), r5 = NPC object, r6 = script stream.
 *
 * Interprets a "move in direction" script command. The three parameter bytes
 * that follow the opcode (direction, speed, distance) are copied into the NPC's
 * movement state, the NPC is switched to its default moving action with the
 * movement flags cleared, the NPC's script is disabled, and the stream pointer
 * is advanced past the command (opcode + 3 parameter bytes).
 *
 * Register notes: r2 is left holding the last byte read (the distance), the
 * ADDS on r6 sets the flags, and r0 comes back from the callee. */
Next NPCCommand_move_in_direction(void) {
    u32 npc = R5;
    u32 cmd = R6;
    u32 direction, speed, distance;

    direction = LD8(cmd + 1);
    speed     = LD8(cmd + 2);
    distance  = LD8(cmd + 3);

    ST8(npc + oOverworldNPCObject_MovementDirection, direction);
    ST8(npc + oOverworldNPCObject_MovementSpeed, speed);
    ST8(npc + oOverworldNPCObject_MovementDistance, distance);

    ST8(npc + oOverworldNPCObject_CurAction, MOVEMENT_FLAG_DEFAULT_MOVING);
    ST16(npc + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    GUEST_CALL(npc_disableScript0x19_809f51e);

    /* Advance the script stream past this command; the ADDS sets the flags
     * the callers branch on, and r2 keeps the last byte loaded. */
    R2 = distance;
    R6 = cmd + 4;
    SET_FLAGS_ADD(cmd, 4);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_play_music ── */
/* NPCCommand_play_music (0809F36E):
 * Script command handler; r6 points at the current NPC script command.
 * Reads a 16-bit song ID at r6+1, starts it playing, then advances the
 * script pointer by 3 bytes (opcode + halfword). Leaves NZCV from the
 * final pointer update, as the original's ADDS does. */
Next NPCCommand_play_music(void) {
    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptHalfword); /* R0 = song id */
    GUEST_CALL(PlayMusic);

    {
        u32 next = R6 + 3;
        SET_FLAGS_ADD(R6, 3);
        R6 = next;
    }
    RETURN_TO_CALLER();
}

/* ── NPCCommand_play_sound ── */
/* NPCCommand_play_sound (0809F048): "play sound" NPC script command.
 * r6 points at the script instruction: a one-byte opcode followed by a
 * halfword sound effect id. Read the id, play it, and advance r6 past the
 * whole 3-byte instruction. The caller inspects the flags left by the final
 * "adds r6, r6, #3". */
Next NPCCommand_play_sound(void) {
    u32 scriptPos = R6;

    R0 = scriptPos + 1;                 /* id halfword starts after the opcode */
    GUEST_CALL(ReadNPCScriptHalfword);  /* r0 = sound effect id */
    GUEST_CALL(PlaySoundEffect);

    SET_FLAGS_ADD(scriptPos, 3);
    R6 = scriptPos + 3;                 /* skip opcode + halfword */
    RETURN_TO_CALLER();
}

/* ── NPCCommand_run_secondary_script ── */
/* NPCCommand_run_secondary_script (0x0809F16E):
 * Script opcode handler. R6 points just past the opcode byte; read the 32-bit
 * address stored there (at R6+1) and install it as the NPC's secondary script
 * pointer, then advance the script cursor past the opcode + operand (5 bytes).
 * Inputs:  R5 = NPC object, R6 = script cursor, LR = return address.
 * Outputs: R0..R3 as left by ReadNPCScriptWord, R6 advanced, flags from the
 *          final cursor increment. */
Next NPCCommand_run_secondary_script(void) {
    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptWord);
    ST32(R5 + oOverworldNPCObject_UnkNPCScriptPtr_5c, R0);
    R6 += 5;
    SET_FLAGS_ADD(R6, 5);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_alpha ── */
/* NPCCommand_set_alpha (0x0809F15A): NPC script command handler.
 *
 * First turns alpha blending off on the NPC's sprite (sprite_disableAlpha).
 * Then it reads the command's parameter byte at [r6+1] into r0; if it is
 * nonzero, that value is applied as the new alpha level via
 * sprite_setAlpha_8002c7a (overriding the earlier disable).
 *
 * r6 points at the current script command and is advanced past the 2-byte
 * command on exit. The final flags come from `adds r6, #2`. */
Next NPCCommand_set_alpha(void) {
    GUEST_CALL(sprite_disableAlpha);

    R0 = LD8(R6 + 1);           /* parameter byte; r0 is set even if zero */
    if (R0 != 0)
        GUEST_CALL(sprite_setAlpha_8002c7a);

    SET_FLAGS_ADD(R6, 2);
    R6 += 2;
    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_coords ── */
/* NPCCommand_set_coords(r5 = NPC object, r6 = NPC script pointer):
 * Script command that sets the NPC's position. Reads three halfwords from
 * the script (at offsets +1, +3, +5) — X, Y and Z — and stores each into the
 * NPC object as a fixed-point coordinate (halfword shifted into the upper 16
 * bits). Advances the script pointer past the command (7 bytes: opcode +
 * three halfwords). Flags on exit reflect the pointer update (ADDS r6, #7). */
Next NPCCommand_set_coords(void) {
    u32 script = R6;

    R0 = script + 1;
    GUEST_CALL(ReadNPCScriptHalfword);
    R0 <<= 16;
    ST32(R5 + oOverworldNPCObject_X, R0);

    R0 = script + 3;
    GUEST_CALL(ReadNPCScriptHalfword);
    R0 <<= 16;
    ST32(R5 + oOverworldNPCObject_Y, R0);

    R0 = script + 5;
    GUEST_CALL(ReadNPCScriptHalfword);
    R0 <<= 16;
    ST32(R5 + oOverworldNPCObject_Z, R0);

    /* add r6, #7 (flag-setting): advance the script read pointer */
    u32 prev = R6;
    R6 = prev + 7;
    SET_FLAGS_ADD(prev, 7);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_write_hidden_oam_pieces ── */
/* NPCCommand_write_hidden_oam_pieces(r5 = NPC object, r6 = script pointer):
 * Script opcode handler: reads a 32-bit operand from the NPC script and stores
 * it in the NPC's "hidden OAM pieces" field, then skips past the opcode byte
 * and the 4-byte operand (script pointer += 5).
 * Returns the operand in r0; flags come from the pointer update. */
Next NPCCommand_write_hidden_oam_pieces(void) {
    /* The word operand starts one byte after the opcode. */
    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptWord);

    ST32(R5 + oOverworldNPCObject_HiddenOAMPieces, R0);

    R6 += 5; /* opcode byte + 32-bit operand */
    SET_FLAGS_ADD(R6, 5);
    RETURN_TO_CALLER();
}

/* ── setNPCScript_809F506 ── */
/* setNPCScript_809F506(r0 = script, r5 = npc):
 * Assign a new animation/script to an overworld NPC and reset its
 * script-run state (termination flags, chat flag, pending script ptr). */
Next setNPCScript_809F506(void) {
    ST32(R5 + oOverworldNPCObject_AnimationScriptPtr, R0);

    ST8(R5 + oOverworldNPCObject_TerminateScript_19, 0);
    ST8(R5 + oOverworldNPCObject_ChatTriggered, 0);
    ST8(R5 + oOverworldNPCObject_TerminateScript_1f, 0);
    ST32(R5 + oOverworldNPCObject_UnkNPCScriptPtr_5c, 0);

    RETURN_TO_CALLER();
}

/* ── npc_resetToStandardState_809F612 ── */
/* Field offsets / constants used here that the shared headers don't define
 * (from the bn6f disassembly of the overworld NPC object). */
#define oOverworldNPCObject_CurState      8
#define oOverworldNPCObject_CurAction_sub_809F612     9
#define NPC_CUR_STATE_STANDARD            4
#define OW_NPC_UNK_FLAGS_60_0x1000        0x1000
#define OW_NPC_UNK_FLAGS_60_FACE_PLAYER_WHEN_INTERACTED_sub_809F612 0x0002

/* npc_resetToStandardState_809F612(r5 = overworld NPC object):
 * Put an NPC back into its standard state: restore the current action and
 * movement flag stashed in its object, reselect its animation (unless the
 * object's UnkFlags_60 bits say to skip that), and clear the chat-triggered
 * flag. Ends with r0 = 0 and the flags left by `mov r0, #0`. */
Next npc_resetToStandardState_809F612(void) {
    u32 returnAddr = LR;

    ST8(R5 + oOverworldNPCObject_CurState, NPC_CUR_STATE_STANDARD);
    ST8(R5 + oOverworldNPCObject_CurAction_sub_809F612,
        LD8(R5 + oOverworldNPCObject_Unk_1d));
    ST8(R5 + oOverworldNPCObject_MovementFlag_0a,
        LD8(R5 + oOverworldNPCObject_Unk_1e));

    R1 = OW_NPC_UNK_FLAGS_60_0x1000 | OW_NPC_UNK_FLAGS_60_FACE_PLAYER_WHEN_INTERACTED_sub_809F612;
    if ((LD32(R5 + oOverworldNPCObject_UnkFlags_60) & R1) == 0) {
        R0 = LD8(R5 + oOverworldNPCObject_AnimationSelect);
        GUEST_CALL(sprite_setAnimation);
        GUEST_CALL(sprite_loadAnimationData);
    }

    R0 = 0;
    SET_FLAGS_NZ(0);
    ST8(R5 + oOverworldNPCObject_ChatTriggered, 0);

    /* the original returns via pop {pc}, so LR keeps its entry value */
    LR = returnAddr;
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_jump_if_var_equal ── */
/* CutsceneCmd_jump_if_var_equal: 08037914-0803793A THUMB
 * Cutscene command "jump if variable equals value":
 *   - step 1: read the variable id byte from the script
 *   - step 2: read the comparison byte
 *   - if the two match, step 3 reads a word giving the new script offset
 *     and execution continues there (r7 = r4);
 *   - otherwise the whole 7-byte command is skipped (r7 += 7).
 * Inputs:  r5 = script base pointer, r7 = current script offset.
 * Outputs: r0 = 1, r4/r6/r7 updated by the reader helpers, flags from the
 *          final flag-setting instructions (movs r0,#1 / the adds that
 *          update r7). */
Next CutsceneCmd_jump_if_var_equal(void) {
    R6 = 1;                          /* reader step 1: variable id */
    GUEST_CALL(ReadMapScriptByte);
    R0 = LD8(R5 + R4);               /* the byte the reader just consumed */

    R6 = 2;                          /* reader step 2: value to compare against */
    GUEST_CALL(ReadMapScriptByte);

    if (R0 == R4) {
        R6 = 3;                      /* reader step 3: jump destination */
        GUEST_CALL(ReadMapScriptWord);
        R7 = R4;                     /* jump: continue at the new offset */
        SET_FLAGS_ADD(R4, 0);        /* "mov r7, r4" is adds r7, r4, #0 */
        R0 = 1;
        SET_FLAGS_NZ(R0);            /* movs r0, #1 */
    } else {
        R7 += 7;                     /* no jump: step past the command */
        SET_FLAGS_ADD(R7, 7);        /* adds r7, r7, #7 */
        R0 = 1;
        SET_FLAGS_NZ(R0);            /* movs r0, #1 */
    }
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_wait_chatbox ── */
/* CutsceneCmd_wait_chatbox (08037790):
 * Cutscene script command "wait for the chatbox". Reads one flag-byte operand
 * out of the map script (r6 = 1 tells ReadMapScriptByte to fetch a byte
 * operand, delivered in r4), then tests the chatbox state:
 *   - the script keeps waiting only while the 0x80-mask check is satisfied
 *     (nonzero) AND the 0x38-mask check is not (zero).
 * While still waiting it returns r0 = 0 without consuming the command byte,
 * so the command runs again on the next update; once the wait is over it
 * advances the script pointer (r7 += 2, like ADDS) and returns r0 = 1. */
Next CutsceneCmd_wait_chatbox(void) {
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);          /* -> R4 = flag byte operand */
    u32 flagOperand = R4;

    R0 = flagOperand & 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (R0 != 0) {
        R0 = flagOperand & 0x38;
        GUEST_CALL(chatbox_mask_eFlags2009F38);
        if (R0 == 0) {
            /* Condition still pending: wait, don't consume the command. */
            R0 = 0;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }
    }

    /* Wait is over: consume the command byte and let the scene continue.
     * (ADDS r7, #2 - its carry/overflow flags are observable by callers.) */
    SET_FLAGS_ADD(R7, 2);
    R7 = R7 + 2;
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── IsCutsceneScriptNonNull ── */
/* IsCutsceneScriptNonNull: check whether a cutscene is running with a script
 * attached. R10 = toolkit. Leaves R0 = current cutscene script pointer and the
 * NZ flags set from testing it, so callers can branch on "script != NULL". */
Next IsCutsceneScriptNonNull(void) {
    u32 cutsceneState = LD32(R10 + oToolkit_CutsceneStatePtr);

    R0 = LD32(cutsceneState + oCutsceneState_CutsceneScriptPos);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCmd_end ── */
/* MapScriptCmd_end: terminates a map script; sets r0 = 0 and returns.
 * Flags: set to reflect the zero result (NZ), as callers may branch on them. */
Next MapScriptCmd_end(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCmd_jump_if_coord_interaction_value_not_equal ── */
/* MapScriptCmd_jump_if_coord_interaction_value_not_equal (0x08035BB2):
 * Map script command "jump if coord-interaction value != <byte>".
 *
 * The script cursor (r7) sits on the command's byte operand, which is followed
 * by a word operand holding the jump target. The byte is compared against the
 * game state's coord-interaction value: on equality the cursor steps over both
 * operands (no jump), otherwise it is moved onto the target word (jump taken).
 *
 * The script readers take the operand width in r6 (1 = byte, 2 = word) and
 * return the operand in r4 while leaving r0, r7 and r10 untouched, so r0
 * still holds the interaction value at the comparison. Always returns r0 = 1.
 */
Next MapScriptCmd_jump_if_coord_interaction_value_not_equal(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 interactionValue = LD8(gameState + oGameState_CoordInteractionValue);

    R0 = interactionValue; /* park the comparand in r0; the reader preserves it */
    R6 = 1;                /* read a byte operand from the script */
    GUEST_CALL(ReadMapScriptByte);

    if (R0 == R4) {
        /* Values match: don't jump; skip past the byte operand and the word. */
        SET_FLAGS_ADD(R7, 6);
        R7 = R7 + 6;
    } else {
        /* Values differ: read the target word and jump the cursor onto it. */
        R6 = 2;            /* read a word operand from the script */
        GUEST_CALL(ReadMapScriptWord);
        SET_FLAGS_ADD(R4, 0); /* "mov r7, r4" encodes as adds r7, r4, #0 */
        R7 = R4;
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump_if_battle_result_equals ── */
/* MapScriptCutsceneCmd_jump_if_battle_result_equals (0x0803793A)
 *
 * Map-script cutscene command "jump if battle result equals <n>", encoded as
 *   [opcode][expected result byte][32-bit destination address]
 *
 * Reads the expected battle-result byte from the script, asks the battle
 * system for the result that was actually obtained, and on a match continues
 * the script at the 32-bit destination address stored in the command.
 * If the results differ, the script pointer skips past the whole 6-byte
 * command instead. R0 = 1 is returned either way.
 *
 * Entry: R0 = value passed through to eStruct200A008_getBattleResult
 *        R7 = current map script position
 * Exit:  R7 = next script position (destination on match, +6 otherwise)
 */
Next MapScriptCutsceneCmd_jump_if_battle_result_equals(void) {
    R6 = 1;                                     /* operand width: byte */
    GUEST_CALL(ReadMapScriptByte);              /* expected result -> R4 */
    u32 expected = R4;

    GUEST_CALL(eStruct200A008_getBattleResult); /* actual battle result -> R0 */
    u32 battleResult = R0;

    if (battleResult == expected) {
        R6 = 2;                                 /* operand width: word */
        GUEST_CALL(ReadMapScriptWord);          /* destination address -> R4 */
        /* mov r7, r4 assembles to adds r7, r4, #0, so it flags like an add. */
        SET_FLAGS_ADD(R4, 0);
        R7 = R4;
    } else {
        /* add r7, #6 also sets carry/overflow, which stay live until return. */
        SET_FLAGS_ADD(R7, 6);
        R7 += 6;
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);                           /* mov r0, #1 */
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_sound_cmd_803810e ── */
/* MapScriptCutsceneCmd_sound_cmd_803810e: "play sound" cutscene script command.
 * Reads two bytes from the script (first via parameter bank 1, then bank 2),
 * dispatches the resulting sound cue (r0 = second byte, r1 = first byte),
 * marks the background music indicator in the game state (0x63 = music changed),
 * and advances the script pointer past the opcode and its two operands.
 * Returns 1 in r0; also leaves the game-state pointer in r1. */
Next MapScriptCutsceneCmd_sound_cmd_803810e(void) {
    /* First operand byte. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    R1 = R4;

    /* Second operand byte. */
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);
    R0 = R4;

    GUEST_CALL(sound_dispatchCueHandler_800068A);

    /* Flag the BGM as changed so the engine picks the new track up.
     * The original leaves the game-state pointer in r1. */
    R1 = LD32(R10 + oToolkit_GameStatePtr);
    ST8(R1 + oGameState_BGMusicIndicator, 0x63);

    /* Advance the script pointer past opcode + two operand bytes.
     * Thumb "adds" sets C/V from the addition; "movs r0, #1" afterwards
     * only refreshes N/Z, so the carry from this add must survive. */
    R7 = R7 + 3;
    SET_FLAGS_ADD(R7, 3);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_write_word ── */
/* MapScriptCutsceneCmd_write_word (0x08035EDA):
 * Cutscene map-script command "write word". Reads two 32-bit words from the
 * script stream — first the destination address, then the value — stores the
 * value at that address, advances the script cursor past the command byte and
 * both payload words (9 bytes total), and reports success.
 *
 * Leaves R0 = 1; C/V come from the script-cursor add, N/Z from the success
 * result. */
Next MapScriptCutsceneCmd_write_word(void) {
    /* First script word: the RAM address to write to. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptWord);
    u32 dest = R4;

    /* Second script word: the value to store there. */
    R0 = dest;
    R6 = 5;
    GUEST_CALL(ReadMapScriptWord);
    u32 value = R4;

    ST32(dest, value);

    /* Skip the command byte plus the two words. */
    SET_FLAGS_ADD(R7, 9);
    R7 += 9;

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── MapScriptSubCmd_free_all_spawned_ow_map_objects ── */
/* MapScriptSubCmd_free_all_spawned_ow_map_objects (0x08037F90):
 * Cutscene script sub-command: release every overworld map object that the
 * current script spawned. Walks the owMapObjectPtrs array in the cutscene
 * state (r5), freeing each non-null entry and clearing its slot, then
 * advances the script pointer (r7) past this command and returns r0 = 1.
 * The map object itself is passed to FreeOverworldMapObject in r5. */

/* The end-of-array symbol isn't in the shared header; the pointer array
 * spans offsets 0x44..0x6C in the cutscene state, ending at 0x70. */
#define oCutsceneState_owMapObjectPtrsEnd_70 (oCutsceneState_owMapObjectPtrs_44 + 0x2C)

Next MapScriptSubCmd_free_all_spawned_ow_map_objects(void) {
    u32 statePtr = R5;
    /* r1 stays live across the loop: it is zeroed for the slot clear, but
     * the callee is free to clobber it and nothing restores it afterwards. */
    u32 r1 = R1;

    for (R4 = oCutsceneState_owMapObjectPtrs_44;
         (s32)R4 < (s32)oCutsceneState_owMapObjectPtrsEnd_70;
         R4 += 4) {
        u32 object = LD32(statePtr + R4);
        if (object != 0) {
            r1 = 0;
            ST32(statePtr + R4, r1);
            R5 = object;
            GUEST_CALL(FreeOverworldMapObject);
            R5 = statePtr;  /* original pops the saved r5 back after the call */
            r1 = R1;        /* whatever the callee left behind */
        }
    }
    /* R4 leaves the loop at owMapObjectPtrsEnd_70, as in the original. */

    R1 = r1;                /* keep the live r1 in the guest register file */

    SET_FLAGS_ADD(R7, 2);   /* adds r7, #2: skip this script command */
    R7 = R7 + 2;

    R0 = 1;
    SET_FLAGS_NZ(R0);       /* movs r0, #1 sets n/z only; c/v stay from the adds */
    RETURN_TO_CALLER();
}

/* ── ReadMapScriptByte ── */
/* ReadMapScriptByte: fetch the next byte of the map script.
 * The caller passes the script pointer in r7 and a byte offset in r6;
 * the byte at that address is returned in r4. Flags are left as the
 * address computation (adds r7, r7, r6) set them. r7 itself is unchanged. */
Next ReadMapScriptByte(void) {
    u32 addr = R7 + R6;
    SET_FLAGS_ADD(R7, R6);
    R4 = LD8(addr);
    RETURN_TO_CALLER();
}

/* ── ReadMapScriptHalfword ── */
/* ReadMapScriptHalfword: 080360A8 (map script interpreter helper)
 * Read the little-endian halfword at (r7 + r6):
 *   r4 = full 16-bit value, r6 = low byte, r7 unchanged. */
Next ReadMapScriptHalfword(void) {
    u32 addr = R7 + R6;

    u32 high = LD8(addr + 1);
    u32 low = LD8(addr);

    R4 = (high << 8) | low;
    R6 = low;
    RETURN_TO_CALLER();
}

/* ── ReadMapScriptSignedByte ── */
/* ReadMapScriptSignedByte(r6 = offset, r7 = map script pointer):
 * Reads the signed byte at r7 + r6 and returns it in r4.
 * (r7 is preserved — it still points at the script for the next read.) */
Next ReadMapScriptSignedByte(void) {
    u32 addr = R7 + R6;

    R4 = (u32)(s32)(s8)LD8(addr);
    RETURN_TO_CALLER();
}

/* ── ReadMapScriptSignedHalfword ── */
/* ReadMapScriptSignedHalfword(r6 = offset, r7 = script pointer):
 * Reads the little-endian halfword at (r7 + r6) and sign-extends it.
 *   r4 = the sign-extended halfword value,
 *   r6 = its low byte.
 * r7 is only used as a scratch pointer internally and is preserved. */
Next ReadMapScriptSignedHalfword(void) {
    u32 addr = R7 + R6;
    u32 low = LD8(addr);
    u32 high = LD8(addr + 1);

    R6 = low;
    R4 = (u32)((s32)((high << 8) | low) << 16 >> 16);
    RETURN_TO_CALLER();
}

/* ── cutscene_forceEndCurrent_8036EFE ── */
/* Names from the disassembly that this translation unit needs. */
#define CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1 1u /* dummy/end marker script id */
#define EVENT_1731 0x1731u                            /* event flag cleared when the cutscene ends */

/* cutscene_forceEndCurrent_8036EFE: force-end whatever cutscene is running. Point the
 * cutscene script interpreter at the dummy script (which immediately
 * finishes the cutscene), drop any "resume after skip" callback, and
 * clear the event flag that gated the cutscene.
 * Preserves r4-r7; leaves the flags as ClearEventFlagFromImmediate left them. */
Next cutscene_forceEndCurrent_8036EFE(void) {
    u32 cutscene = LD32(R10 + oToolkit_CutsceneStatePtr);

    /* Jump the script interpreter onto the dummy end-of-cutscene script. */
    ST32(cutscene + oCutsceneState_CutsceneScriptPos, CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos2, A_DummyCutsceneScript);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos3, A_DummyCutsceneScript);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos4, A_DummyCutsceneScript);

    /* Nothing to resume into if the player skips the cutscene. */
    ST32(cutscene + oCutsceneState_CutsceneScriptAfterCutsceneSkip, 0);

    /* ClearEventFlagFromImmediate(EVENT_1731): the flag id is passed split
     * as r0 = high byte (0x17), r1 = low byte (0x31). */
    R0 = EVENT_1731 >> 8;
    R1 = EVENT_1731 & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── battle_reloadObjectSprite_8116A04 ── */
/* battle_reloadObjectSprite_8116A04 (r5 = battle object):
 * Refresh the object's sprite from its current frame parameters.
 * If the frame parameters report that the animation must be (re)loaded
 * (bits 0xC0 set), restart it: select animation 0, load its data, remove
 * the shadow, run the follow-up setup in sub_8116A44, and mark the object
 * state byte at +9 as 8 (also leaving that value in r0). Flags are left
 * from the last flag-setting instruction (TST r0, 0xC0 on the skip path,
 * MOV r0, #8 otherwise). */
Next battle_reloadObjectSprite_8116A04(void) {
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;

    /* mov r1, #0xC0 happens before the branch, so R1 is 0xC0 on exit
     * even when the reload is skipped. */
    R1 = 0xC0;

    if ((frameParams & 0xC0) == 0) {
        /* tst r0, r1 -> Z set, nothing to do */
        SET_FLAGS_NZ(frameParams & 0xC0);
        RETURN_TO_CALLER();
    }

    R0 = 0;
    GUEST_CALL(sprite_setAnimation);        /* sprite_setAnimation(0) */
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    GUEST_CALL(sub_8116A44);

    /* obj->state = 8 (MOV #8 is flag-setting in Thumb); r0 ends as 8 too,
     * regardless of what sub_8116A44 left in it. */
    R0 = 8;
    SET_FLAGS_NZ(8);
    ST8(R5 + 9, 8);
    RETURN_TO_CALLER();
}

/* ── sprite_dispatchVariantUpdate_8116B9C ── */
/* sprite_dispatchVariantUpdate_8116B9C(r5 = sprite/object pointer):
 * Run the per-variant update routine selected by the byte at [r5+9], then
 * advance the sprite animation. The selector byte indexes a 3-entry word
 * table, so it only ever holds 0, 4 or 8 (byte offsets into the table).
 * All registers except r0/r1 (clobbered by the call) are passed through. */
Next sprite_dispatchVariantUpdate_8116B9C(void) {
    switch (LD8(R5 + 9)) {
    case 0: GUEST_CALL(sub_8116BC0); break; /* off_8116BB4[0] */
    case 4: GUEST_CALL(sub_8116BFC); break; /* off_8116BB4[1] */
    case 8: GUEST_CALL(sprite_scrollStepAndCullOffscreen_8116C14); break; /* off_8116BB4[2] */
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── sprite_scrollStepAndCullOffscreen_8116C14 ── */
/* sprite_scrollStepAndCullOffscreen_8116C14(r7 = sprite):
 * Nudge the sprite along one axis by 8x the signed value stored at
 * sprite+0x10, then look at the resulting coordinate. If it has moved
 * past zero into negative territory, clear the sprite's scale counter
 * and mark it unscalable (it has scrolled off / shrunk away).
 *
 * Outputs: r0/r1 hold the coordinates returned by sprite_getCoordinates;
 * flags come from the CMP against that coordinate, or (when the reset
 * call is made) from whatever that callee leaves behind. */
Next sprite_scrollStepAndCullOffscreen_8116C14(void) {
    s32 offset = LDS16(R7 + 0x10);

    R0 = (u32)offset << 3;
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    GUEST_CALL(sprite_getCoordinates);
    SET_FLAGS_CMP(R0, 0);

    if ((s32)R0 < 0)
        GUEST_CALL(sprite_clearScaleCounterAndMakeUnscalable_804668A);

    RETURN_TO_CALLER();
}

/* ── battle_objectRunBehaviorStep_8116D6C ── */
/* battle_objectRunBehaviorStep_8116D6C: run one step of the battle object at R5, then refresh its sprite.
 *
 * [R5+9] holds a byte offset into a small table of behavior handlers
 * (off_8116D84: 0 -> battle_moveSpriteLeftUntilX170_8116D90, 4 -> sub_8116DB4, 8 -> battle_stepForwardAnimWait_8116DEC).
 * The original performs an indirect `mov lr, pc; bx r0` call, so each
 * handler returns straight back here; afterwards the object's sprite
 * animation is advanced by sprite_update before returning to the caller.
 * All other registers (R2..R12) are simply passed through to the handler. */
Next battle_objectRunBehaviorStep_8116D6C(void) {
    switch (LD8(R5 + 9)) {
    case 0: GUEST_CALL(battle_moveSpriteLeftUntilX170_8116D90); break;
    case 4: GUEST_CALL(sub_8116DB4); break;
    case 8: GUEST_CALL(battle_stepForwardAnimWait_8116DEC); break;
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── battle_moveSpriteLeftUntilX170_8116D90 ── */
/* battle_moveSpriteLeftUntilX170_8116D90(r5 = battle object):
 * Drift the object's sprite 16 px to the left; once its x coordinate has
 * reached 0xAA or below, clamp it to exactly 0xAA and advance the object's
 * state byte to 4. r2 is left holding the 0xAA boundary constant on both
 * paths, matching the original. Flags reflect the last flag-setting
 * instruction (CMP on the early-out path, MOVS #4 plus callee flags
 * otherwise). */
Next battle_moveSpriteLeftUntilX170_8116D90(void) {
    R0 = -(u32)0x10;                    /* add (-16, 0) to the sprite position */
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    GUEST_CALL(sprite_getCoordinates);  /* r0 = x, r1 = y */

    R2 = 0xAA;                          /* boundary constant, kept in r2 */
    if ((s32)R0 > (s32)R2) {
        SET_FLAGS_CMP(R0, R2);
        RETURN_TO_CALLER();
    }

    R0 = R2;                            /* snap x back to the boundary... */
    GUEST_CALL(sprite_setCoordinates);

    R0 = 4;                             /* ...and mark the object as finished */
    SET_FLAGS_NZ(R0);
    ST8(R5 + 9, R0);
    RETURN_TO_CALLER();
}

/* ── object_dispatchModeHandler_81178D8 ── */
/* object_dispatchModeHandler_81178D8: behaviour dispatcher for the object whose pointer is in R5.
 *
 * The byte at [R5+8] is the object's mode, stored already scaled as a byte
 * offset into the handler pointer table at off_81178EC (the game only uses
 * 0, 4 or 8, which select sub_81178FC / sub_811794C / sub_8117990). The word
 * read from the table is the handler's Thumb entry address (odd). That word
 * is the call target and is also passed to the handler itself in R0, along
 * with the mode byte in R1 and the shared work-area address in R7; R2-R6 and
 * R8-R12 pass through untouched. Whatever the handler leaves in R0/R1 and
 * the flags is what the caller of this dispatcher sees.
 *
 * Note the mode byte is added to the table base *directly* (ldr r0, [r0,r1]),
 * so it must not be re-scaled here -- the loaded word, not mode/4, decides
 * which handler runs.
 */

#define HANDLER_SUB_81178FC 0x081178FDu /* = sub_81178FC + 1 (Thumb) */
#define HANDLER_SUB_811794C 0x0811794Du /* = sub_811794C + 1 (Thumb) */
#define HANDLER_SUB_8117990 0x08117991u /* = sub_8117990 + 1 (Thumb) */

Next object_dispatchModeHandler_81178D8(void) {
    u32 mode = (u8)LD8(R5 + 8);

    R7 = A_byte_2011C10;
    R1 = mode;

    /* Fetch the handler pointer exactly as the original does: the mode byte
     * is added straight to the table base, and the loaded word is both the
     * branch target and the value handed to the callee in R0. */
    u32 handler = LD32(A_off_81178EC + mode);
    R0 = handler;

    if (handler == HANDLER_SUB_81178FC) {
        GUEST_CALL(sub_81178FC);
    } else if (handler == HANDLER_SUB_811794C) {
        GUEST_CALL(sub_811794C);
    } else if (handler == HANDLER_SUB_8117990) {
        GUEST_CALL(sub_8117990);
    }
    /* Any other word can only result from reading past the three-entry table
     * (mode >= 12, which real game states never produce). In that case the
     * loaded pointer simply remains in R0 and nothing else is disturbed. */

    RETURN_TO_CALLER();
}

/* ── sprite_testFrameParamsMaskC0_81190F8 ── */
/* sprite_testFrameParamsMaskC0_81190F8(r0 = sprite handle):
 * Looks up the sprite struct registered for the handle in r0 (via the list at
 * 0x2008450) and reports whether the sprite's current frame carries the status
 * bits 0xC0 in its frame parameters.
 * Returns r0 = 1 if the bits are set, 0 if not (or if no struct was found),
 * with N/Z reflecting r0 and C/V cleared (the original's "mov r0, r4" is an
 * adds r0, r4, #0, which zeroes the carry/overflow flags, and the trailing
 * tst r0, r0 leaves them cleared). r1 ends as 0xC0 on the found path;
 * r4/r5 are preserved. */
Next sprite_testFrameParamsMaskC0_81190F8(void) {
    u32 savedR4 = R4, savedR5 = R5;
    u32 flagged = 0;

    /* The callees see r4 = 0 here, matching the original's working register. */
    R4 = 0;

    /* Fetch the struct for the handle in r0; r0 = NULL if there is none. */
    GUEST_CALL(getStructFrom2008450);        /* -> r0 = struct ptr, r1 = frame info */
    if (R0 != 0) {
        /* The frame-parameter query takes the accompanying value in r5. */
        R5 = R1;
        GUEST_CALL(sprite_getFrameParameters);   /* -> r0 = frame parameter bits */

        /* Test the frame parameter bits against the 0xC0 mask (also left in r1). */
        R1 = 0xC0;
        if (R0 & R1)
            flagged = 1;
    }

    /* Restore the caller's r4/r5 (the original pops them back off the stack). */
    R4 = savedR4;
    R5 = savedR5;

    R0 = flagged;
    /* mov r0, r4 == adds r0, r4, #0: no carry-out and no signed overflow are
     * possible, so this clears C and V (N/Z from r4 as well). The original's
     * tst r0, r0 then re-derives the same N/Z while leaving C/V cleared. */
    SET_FLAGS_ADD(R0, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── submenu_runModeHandler_811BD64 ── */
/* submenu_runModeHandler_811BD64: dispatch the handler for the current submenu mode.
 *
 * r5 points at the menu work area; the byte at +8 holds the selected mode,
 * stored as a byte offset into the handler table at off_811BD7C (0, 4 or 8
 * select the three submenu handlers). r10 is the toolkit; its
 * oToolkit_SubmenuPtr field points at the submenu object the handlers
 * operate on, passed to them in r6. r6 is only a scratch argument register
 * here, so it is saved and restored for the caller. The table entry is also
 * loaded into r0, so the handler receives its own address in r0 (matching
 * the original's ldr r0,[r0,r1] before bx r0). r0, r1 and the flags are
 * passed straight through from the handler. */
Next submenu_runModeHandler_811BD64(void) {
    u32 savedR6 = R6;

    R6 = LD32(R10 + oToolkit_SubmenuPtr);   /* submenu object -> handler's r6 */
    u32 mode = LD8(R5 + 8);                 /* byte offset into handler table */
    R1 = mode;                              /* handler's r1 */
    R0 = LD32(A_off_811BD7C + mode);        /* table entry -> handler's r0 */

    /* Handler table at off_811BD7C:
     *   offset 0: sub_811BD88, offset 4: sprite_dispatchObjectUpdate_811BDCC, offset 8: sub_811BDF4 */
    switch (mode) {
    case 0: GUEST_CALL(sub_811BD88); break;
    case 4: GUEST_CALL(sprite_dispatchObjectUpdate_811BDCC); break;
    case 8: GUEST_CALL(sub_811BDF4); break;
    }

    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── chatbox_checkAdvanceTextPrint_8040154 ── */
/* chatbox_checkAdvanceTextPrint_8040154(r5 = chatbox):
 * Called each frame while the chatbox is printing text. If the player is
 * holding B or has just pressed A (or the text has no per-character print
 * delay), reset the chatbox's state-machine step to 0 so it moves on to the
 * next phase; otherwise leave it alone.
 * Returns r2 = 2 if B was held, r2 = 1 otherwise. */
Next chatbox_checkAdvanceTextPrint_8040154(void) {
    if (LD16(R5 + oChatbox_JoypadHeld) & 2) {   /* B currently held */
        R2 = 2;
        ST8(R5 + oChatbox_JumpTableOffset_11, 0);
        RETURN_TO_CALLER();
    }

    R2 = 1;
    /* A newly pressed, or the print speed is 0 (text completes instantly) */
    if ((LD16(R5 + oChatbox_JoypadUp) & 1) != 0 || LD8(R5 + 8) == 0) {
        ST8(R5 + oChatbox_JumpTableOffset_11, 0);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_stepScriptEntry_80417A4 ── */
/* chatbox_stepScriptEntry_80417A4(r4 = pointer to the current chatbox script entry):
 * Peek at the byte at offset 3 of the current script entry.  If it is the
 * 0xFF terminator, the entry is "go to next line": advance the cursor by one
 * entry (r4 += 4).  Otherwise hand the byte to the chatbox script interpreter
 * as a script ID (the first argument is always the constant 3; the original
 * also sets r0 = 2 first, but that store is dead).
 * Always returns r0 = 1.  On the terminator path r1 still holds the byte read
 * and r4 is the advanced cursor; on the interpreter path r1/r4 are whatever
 * the callee leaves.  Flags end up as after "mov r0, #1" (nz = 1 != 0),
 * except that on the terminator path c/v still come from the r4 += 4 add. */
Next chatbox_stepScriptEntry_80417A4(void) {
    R0 = 3;                              /* fixed notUsed argument */
    R1 = LD8(R4 + 3);                    /* script ID or end-of-entry marker */

    if (R1 != 0xFF) {
        GUEST_CALL(chatbox_config_80408D0);
    } else {
        u32 nextEntry = R4 + 4;
        SET_FLAGS_ADD(R4, 4);            /* carry/overflow of the cursor bump */
        R4 = nextEntry;
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_cmd_playerFixedAnim_8041EE8 ── */
/* owPlayer_RequestFixedAnimation_809E14C is not declared in the shared headers, so forward-declare it
 * here; it is a normal `Next ... (void)` game function. */
Next owPlayer_RequestFixedAnimation_809E14C(void);

/* chatbox_cmd_playerFixedAnim_8041EE8 — chatbox script command handler.
 *
 * Inputs:  r4 = pointer into the chatbox script (the command's operand
 *              byte sits at [r4 + 2]),
 *          r5 = chatbox work area,
 *          r10 = toolkit.
 *
 * What it does:
 *   1. sets bit 0 of the chatbox 0x3E flag word (the original loads the
 *      immediate 1 directly; no named constant exists for this bit),
 *   2. copies byte 0x14 of the overworld player object into the chatbox
 *      work area at offset 0x3C,
 *   3. passes the command's operand byte (and the player object in r7) to
 *      owPlayer_RequestFixedAnimation_809E14C, whose result comes back in r1,
 *   4. advances the script pointer past the 3-byte command.
 *
 * Outputs: r0 = 0, r1 = owPlayer_RequestFixedAnimation_809E14C's result, r2 = copied player byte,
 *          r4 = advanced script pointer, r7 = player object pointer.
 *          Flags: N/Z from "mov r0,#0" (Z set, N clear), C/V left over
 *          from the ADDS on the script pointer. r5, r6 and lr preserved.
 */

Next chatbox_cmd_playerFixedAnim_8041EE8(void) {
    /* mark the flag bit for this command in the chatbox state */
    R0 = 1; /* FLAGS_3E bit 0 */
    GUEST_CALL(chatbox_setflags_3e);

    /* look up the overworld player object and copy its byte 0x14
     * into the chatbox work area */
    R7 = LD32(LD32(R10 + oToolkit_GameStatePtr) + oGameState_OverworldPlayerObjectPtr);
    R2 = LD8(R7 + 0x14);
    ST8(R5 + 0x3C, R2);

    /* operand byte of the current script command. (The original's
     * "cmp r0,#7 / bgt next-line" here is a no-op — both paths fall
     * through, and the call below overwrites the flags anyway.) */
    R0 = LD8(R4 + 2);

    GUEST_CALL(owPlayer_RequestFixedAnimation_809E14C);

    /* advance to the next 3-byte command. The increment's carry/overflow
     * must survive, and mov r0,#0 afterwards forces N=0/Z=1 on top of
     * them without touching C/V. */
    u32 prevPos = R4;
    R4 = prevPos + 3;
    SET_FLAGS_ADD(prevPos, 3);
    R0 = 0;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── chatbox_scriptWaitPlayerFrameStep_8041F1C ── */
/* chatbox_scriptWaitPlayerFrameStep_8041F1C: chatbox tick handler.
 *
 * Sets chatbox flag bit 1 (via chatbox_setflags_3e), then looks up the
 * overworld player object through the toolkit's game state and asks for
 * its sprite frame parameters. Bit 0x80 of those parameters signals that
 * the current frame-sequence step has finished; while it is clear the
 * caller keeps waiting.
 * r4 is the text-script cursor: it is advanced by 2 once the bit is set.
 *
 * Returns r0 = 1 when the script cursor should advance, 0 otherwise.
 * NZ is set from r0 on both paths; on the advancing path C/V come from
 * the r4 += 2 addition. r1 is left holding frameParams & 0x80; r5 is
 * preserved. */
Next chatbox_scriptWaitPlayerFrameStep_8041F1C(void) {
    R0 = 1; /* FLAGS_3E bit 1 */
    GUEST_CALL(chatbox_setflags_3e);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    u32 savedR5 = R5;
    R5 = LD32(gameState + oGameState_OverworldPlayerObjectPtr);
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;
    R5 = savedR5;

    R1 = frameParams & 0x80;

    if (R1 != 0) {
        /* frame step finished: consume the next script word */
        SET_FLAGS_ADD(R4, 2);
        R4 = R4 + 2;
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        /* still animating: stay on the current script word */
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_scriptStepSyncPlayerTextState_8041F44 ── */
/* chatbox_scriptStepSyncPlayerTextState_8041F44: chatbox script step.
 *
 * Sets chatbox flag bit 0x0001, then reads the counter at [r5+0x3C]
 * (r5 = chatbox state). While that counter is still 7 or less it is copied
 * into the overworld player object's field 0x10 (which holds the current
 * text state). Finally calls owPlayer_RequestFixedAnimation_809E14C, advances the script cursor (r4)
 * by 2 and returns 0 with N/Z cleared; carry/overflow are left as the
 * "adds r4, #2" before it left them.
 *
 * Inputs:  r4 = script cursor, r5 = chatbox state ptr, r10 = toolkit.
 * Outputs: r0 = 0, r1 = whatever owPlayer_RequestFixedAnimation_809E14C leaves, r4 advanced by 2,
 *          r7 = player object (unchanged if the counter was > 7),
 *          nz/c/v as described above.
 * Preserved: r5, r6.
 */
Next chatbox_scriptStepSyncPlayerTextState_8041F44(void) {
    R0 = 1; /* FLAGS_3E_UNK_0001 */
    GUEST_CALL(chatbox_setflags_3e);

    R1 = R5 + 0x3C;
    u32 counter = LD8(R1);
    if ((s32)counter <= 7) {
        u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
        R7 = LD32(gameState + oGameState_OverworldPlayerObjectPtr);
        ST8(R7 + 0x10, counter);
    }

    /* owPlayer_RequestFixedAnimation_809E14C still sees the counter in r0 and the chatbox field
     * address in r1, mirroring the register state at this point. */
    R0 = counter;
    GUEST_CALL(owPlayer_RequestFixedAnimation_809E14C);

    u32 newCursor = R4 + 2;
    SET_FLAGS_ADD(R4, 2); /* carry/overflow come from this add */
    R4 = newCursor;

    R0 = 0;
    SET_FLAGS_NZ(R0);     /* ...but N/Z come from "mov r0, #0" */
    RETURN_TO_CALLER();
}

/* ── chatbox_PlayScriptSoundCue_80422D4 ── */
/* chatbox_PlayScriptSoundCue_80422D4: play the sound cue at the current chatbox script position.
 * r4 points at a 4-byte cue record; bytes 2-3 hold the 16-bit sound effect
 * id (little-endian). Plays it, steps r4 past the record and returns r0 = 1.
 * Flags on return: NZ from "r0 = 1", C/V from the r4 += 4 add. */
Next chatbox_PlayScriptSoundCue_80422D4(void) {
    /* The original does push {lr} ... pop {pc}; the saved value is LR itself.
     * Reproduce the push so that SP during the PlaySoundEffect call matches
     * the original (the callee's stack activity must land at the same
     * addresses), then pop before returning. */
    SP -= 4;
    ST32(SP, LR);

    u32 soundId = (u32)LD8(R4 + 2) | ((u32)LD8(R4 + 3) << 8);

    R0 = soundId;
    /* The original's byte-merge leaves the shifted high byte in r1 at the
     * call site; hand PlaySoundEffect the same register contents. */
    R1 = (u32)LD8(R4 + 3) << 8;
    GUEST_CALL(PlaySoundEffect);

    /* Advance to the next cue record (the ADD sets C/V for the caller). */
    SET_FLAGS_ADD(R4, 4);
    R4 += 4;

    /* Report success; MOV r0, #1 refreshes N/Z while C/V stay from the add. */
    R0 = 1;
    SET_FLAGS_NZ(R0);

    SP += 4;
    RETURN_TO_CALLER();
}

/* ── chatbox_cmdPlayMusic_80422E8 ── */
/* chatbox_cmdPlayMusic_80422E8(r4 = chatbox script pointer):
 * Play the music track given as a little-endian u16 in the current chatbox
 * script command, then advance the script pointer past the command.
 * Leaves r4 pointing at the next command and returns r0 = 1.
 * Flags on exit: n/z from "movs r0, #1", c/v from "adds r4, #4". */
Next chatbox_cmdPlayMusic_80422E8(void) {
    u32 songLow  = LD8(R4 + 2);
    u32 songHigh = LD8(R4 + 3);

    /* Match the original's register state at the call: r1 holds the
     * shifted high byte, r0 the full halfword. */
    R1 = songHigh << 8;
    R0 = songLow | R1;
    GUEST_CALL(PlayMusic);      /* PlayMusic(song) */

    {
        u32 old = R4;
        R4 = old + 4;
        SET_FLAGS_ADD(old, 4);  /* c/v from "adds r4, #4" */
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);           /* n/z from "movs r0, #1" */

    RETURN_TO_CALLER();
}

/* ── chatbox_scriptOpClearFlag40_80422FC ── */
/* chatbox_scriptOpClearFlag40_80422FC: chatbox script opcode handler.
 * Clears chatbox status flag 0x40 (via chatbox_clearFlags_3e), advances the
 * script pointer in r4 past this opcode (2 bytes), and returns r0 = 1 so the
 * chatbox keeps running the script.
 * Flags left for the caller: N/Z from the final `mov r0, #1`, C/V from the
 * pointer increment. */
Next chatbox_scriptOpClearFlag40_80422FC(void) {
    /* clear flag FLAGS_3E_UNK_0040 in the chatbox status field */
    R0 = FLAGS_3E_UNK_0040;
    GUEST_CALL(chatbox_clearFlags_3e);

    /* step the script pointer over this opcode */
    u32 prevPtr = R4;
    R4 = prevPtr + 2;
    SET_FLAGS_ADD(prevPtr, 2);

    /* signal the chatbox to continue */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_stateSetFlag40Advance_804230C ── */
/* chatbox_stateSetFlag40Advance_804230C: step the chatbox forward.
 * Sets chatbox flag 0x40 (via chatbox_setflags_3e), advances the caller's
 * chatbox state pointer (r4) by 2 bytes, and returns 1.
 * Flags on return: N/Z from the result value 1, C/V from the r4 += 2 add.
 * Preserves r1 (chatbox_setflags_3e is a leaf and keeps r1/r4 intact). */
Next chatbox_stateSetFlag40Advance_804230C(void) {
    R0 = FLAGS_3E_UNK_0040;
    GUEST_CALL(chatbox_setflags_3e);

    SET_FLAGS_ADD(R4, 2);   /* ADDS r4, #2 */
    R4 += 2;

    R0 = 1;
    SET_FLAGS_NZ(1);        /* MOVS r0, #1 sets N/Z, leaves C/V */

    RETURN_TO_CALLER();
}

/* ── chatbox_textScriptAction_openShop_80424BC ── */
/* chatbox_textScriptAction_openShop_80424BC: chatbox text-script action used at shop counters.
 * Sets chatbox flag 0x3E bit 1 (the "close chatbox" mask, FLAGS_3E_UNK_0001
 * in the disassembly), and if this script step hasn't fired yet, launches
 * the shop whose id is stored at [r4+2] and marks the step as taken by
 * storing 1 into the text-script state byte at [r5]+4. Returns 0 in r0.
 *
 * Flag note: the final `mov r0, #0` only rewrites N and Z; C and V reach
 * the caller from the `cmp r0, #0` (C=1, V=0) on the skip path, or from
 * subsystem_launchShop on the launch path. */
Next chatbox_textScriptAction_openShop_80424BC(void) {
    R0 = 1;                    /* FLAGS_3E_UNK_0001: close-chatbox flag */
    GUEST_CALL(chatbox_setflags_3e);

    u32 state = LD8(R5 + 4);   /* oChatbox_TextScriptState_04 */
    SET_FLAGS_CMP(state, 0);   /* cmp r0, #0: N/Z from value, C=1, V=0 */
    if (state == 0) {
        R0 = LD8(R4 + 2);      /* shop id for subsystem_launchShop */
        GUEST_CALL(subsystem_launchShop);
        ST8(R5 + 4, 1);        /* mark step as done */
    }

    R0 = 0;                    /* mov r0, #0: N=0, Z=1; C/V untouched */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_mask_eFlags2009F38 ── */
/* chatbox_mask_eFlags2009F38(r0 = flag bit(s)):
 * Return the given flag bits as they currently stand in the chatbox/
 * cutscene flag word at eFlags2009F38. R1 is left untouched, and the
 * NZ flags reflect the masked value so callers can branch on it. */
Next chatbox_mask_eFlags2009F38(void) {
    R0 &= LD32(A_eFlags2009F38);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_803FD9C ── */
/* chatbox_runScript_803FD9C(r0 = TextScriptArchive *, r1 = script index):
 *
 * Alternate entry point of the chatbox script runner. The full routine lives
 * at chatbox_runScript_803FDA4, whose shared body begins at loc_803FDA8; the
 * two entry points differ only in the initial value of the resume flag r4
 * (this one clears it, meaning "start the script from the top").
 *
 * This entry pushes {r4,r5,lr} before branching into the shared body, and the
 * body finishes with pop {r4,r5,pc}, so the frame must exist on the guest
 * stack: the body reads r5 and the return address from it and restores SP.
 */
Next chatbox_runScript_803FD9C(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_lr = LR;

    /* push {r4,r5,lr}: r4 at lowest address, lr at highest */
    u32 frame = SP - 12;
    SP = frame;
    ST32(frame, saved_r4);
    ST32(frame + 4, saved_r5);
    ST32(frame + 8, saved_lr);

    /* mov r4, #FALSE: no interrupted script to resume */
    R4 = 0;

    /* Tail-branch into the shared body of the script runner; it pops
     * {r4,r5,pc} on exit, returning straight to our caller. */
    GUEST_CALL(loc_803FDA8);

    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_803FE08 ── */
/* chatbox_runScript_803FE08(r0 = TextScriptArchive *archive, r1 = scriptIdx):
 * Entry point for the chatbox script runner. It sets up the runner's state
 * flag (r4 = FALSE, i.e. "script not finished") and tail-branches into the
 * shared script-running body at loc_803FE14, which produces the outputs
 * (r5, r6, r7). Everything else — r0, r1, r5, r6, r7, r10 — is passed
 * straight through to the body.
 *
 * The original pushes {r4,r5,lr} before the branch and never pops it: the
 * tail-called body's epilogue consumes this frame, so the slots must really
 * be written to the guest stack (r4 at SP-12, r5 at SP-8, lr at SP-4). */
Next chatbox_runScript_803FE08(void) {
    u32 frame = SP -= 12;

    /* push {r4,r5,lr} */
    ST32(frame, R4);
    ST32(frame + 4, R5);
    ST32(frame + 8, LR);

    /* mov r4, #FALSE — start the script from its beginning. */
    R4 = 0;

    /* b loc_803FE14 — tail-call into the script runner proper; its
     * results in r5/r6/r7 are this function's results. */
    GUEST_CALL(loc_803FE14);
    RETURN_TO_CALLER();
}

/* ── sprite_clearMosaic ── */
/* Field offsets the disassembly references symbolically. Define them here in
 * case they are not already provided by the shared header. */
enum { oObjectHeader_TypeAndSpriteOffset = 2 };


/* sprite_clearMosaic(r5 = object):
 * Clears the mosaic bit (0x10) in the object's sprite flag byte.
 * The sprite struct is selected by the high nibble of the object's
 * type/sprite-offset byte, so the flag byte lives at
 * r5 + (typeAndSpriteOffset & 0xF0) + oObjectSprite_Unk_11.
 * Leaves r0 = 0x10 (the cleared mask), r1 = new flag byte,
 * r3 = address of the sprite struct, and flags from the address add. */
Next sprite_clearMosaic(void) {
    u32 obj = R5;

    u32 spriteOffset = LD8(obj + oObjectHeader_TypeAndSpriteOffset) & 0xF0;
    u32 sprite = obj + spriteOffset;

    /* ADDS sets c/v/nz, which callers inspect */
    R3 = sprite;
    SET_FLAGS_ADD(spriteOffset, obj);

    u32 flags = LD8(sprite + oObjectSprite_Unk_11) & ~0x10;
    ST8(sprite + oObjectSprite_Unk_11, flags);

    R0 = 0x10;
    R1 = flags;
    RETURN_TO_CALLER();
}

/* ── sprite_resetObjVars_800289C ── */
/* sprite_resetObjVars_800289C:
 * Reset the sprite object state: clear the counter at dword_20093A8, then
 * fill the 0x388-word table at dword_200A890 with 0xFFFFFFFF via WordFill.
 * Flags on return (including v) are those left by WordFill's last
 * flag-setting instruction; r3 is untouched. */
Next sprite_resetObjVars_800289C(void) {
    ST32(A_dword_20093A8, 0);

    R0 = A_dword_200A890;
    R1 = 0x388;
    R2 = 0xFFFFFFFF;
    GUEST_CALL(WordFill);

    RETURN_TO_CALLER();
}

/* ── sprite_setColorShader ── */
/* Field offsets used by sprite_setColorShader (not declared in game.h). */
enum { oObjectHeader_TypeAndSpriteOffset_sprite_setColorShader = 2 };
enum { oObjectSprite_Unk_06 = 6 };

/* sprite_setColorShader(r0 = color shader id, r5 = object header):
 * Pick the sprite block selected by the high nibble of the object's
 * type/sprite-offset byte and store the color shader id into it.
 * Returns r3 = address of that sprite block, with flags from the ADD. */
Next sprite_setColorShader(void) {
    u32 header = R5;
    u32 shaderId = R0;

    /* Sprite block base = header + (typeAndSpriteOffset high nibble << 4) */
    u32 spriteBlock = header + (LD8(header + oObjectHeader_TypeAndSpriteOffset_sprite_setColorShader) & 0xF0);

    ST16(spriteBlock + oObjectSprite_Unk_06, shaderId);

    R3 = spriteBlock;
    SET_FLAGS_ADD(spriteBlock, header); /* ADDS r3, r3, r5 */
    RETURN_TO_CALLER();
}

/* ── sprite_loadAnimFrameTiles_8002874 ── */
/* sprite_loadAnimFrameTiles_8002874(r5 = object animation state):
 * Advances an object's graphics frame. Fields used:
 *   +0x18  index of the current frame within the animation table
 *   +0x1C  pointer to the animation table pointer
 *   +0x24  pointer to the frame descriptor currently loaded in VRAM
 *   +0x08  VRAM slot number (each slot holds one 8-word tile block)
 * If the frame descriptor for the current index differs from the one already
 * uploaded, store it and queue a transfer that copies its tile data
 * (descriptor words, followed by the tiles) into vObjectTiles[slot].
 * Flags: on the "unchanged" path they come from the frame comparison;
 * on the "upload" path they come from QueueEightWordAlignedGFXTransfer. */
Next sprite_loadAnimFrameTiles_8002874(void) {
    u32 loadedFrame = LD32(R5 + 0x24);
    u32 table = LD32(LD32(R5 + 0x1C));
    u32 currentFrame = table + LD32(R5 + 0x18);

    if (currentFrame == loadedFrame) {
        /* Frame already in VRAM; nothing to do. */
        SET_FLAGS_CMP(currentFrame, loadedFrame);
        RETURN_TO_CALLER();
    }

    ST32(R5 + 0x24, currentFrame);

    u32 tileCount = LD32(currentFrame);              /* first word: tile count */
    u32 src = currentFrame + 4;                      /* tiles follow the header word */
    u32 dest = A_vObjectTiles + ((u32)LD16(R5 + 8) << 5);

    R0 = src;
    R1 = dest;
    R2 = tileCount;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    RETURN_TO_CALLER();
}

/* ── startScreen_TstZero ── */
/* startScreen_TstZero: always clear the flags as "zero" (Z set, N clear).
 * Despite the name, it unconditionally tests 0 against itself. */
Next startScreen_TstZero(void) {
    R0 = 0;
    SET_FLAGS_NZ(0); /* tst r0, r0 with r0 == 0 */
    RETURN_TO_CALLER();
}

/* ── keyItems_ResetInventoryAndGrantScenarioItems_803CD74 ── */
/* keyItems_ResetInventoryAndGrantScenarioItems_803CD74: wipe the key-item inventory block (0x190 bytes) and re-grant the
 * starting key items: four copies of item 0x75 and one of item 0.
 * R10 is the toolkit pointer. R0-R2 and flags are clobbered; R4-R7 and up are
 * left untouched, matching the original (it saves/restores R4-R7 itself). */
Next keyItems_ResetInventoryAndGrantScenarioItems_803CD74(void) {
    u32 keyItems = LD32(R10 + oToolkit_KeyItemsPtr);

    R0 = keyItems;
    R1 = 0x190; /* sizeof key-item inventory */
    GUEST_CALL(ZeroFillByWord);

    R0 = 0x75;
    R1 = 4;
    GUEST_CALL(keyItems_RemoveFromInventoryAndGive_803CDF8);

    R0 = 0;
    R1 = 1;
    GUEST_CALL(keyItems_RemoveFromInventoryAndGive_803CDF8);

    RETURN_TO_CALLER();
}

/* ── petnavi_setStat42ToStat3EPlusCounter_803CED4 ── */
/* petnavi_setStat42ToStat3EPlusCounter_803CED4:
 * Reads the current PET navi's stat 0x3E, adds the 16-bit value stored at
 * (*R10->oToolkit_Unk2004334_Ptr), and writes the sum into stat 0x42 of the
 * same navi.
 *
 * Register contract:
 *   inputs:    r4 r6 r7 r10 lr
 *   outputs:   r2 (the computed sum); r3 as left by SetCurPETNaviStatsHword
 *   preserved: r4 r6 r7 (saved/restored around the helper calls)
 *   clobbers:  r0 r1 and flags
 */
Next petnavi_setStat42ToStat3EPlusCounter_803CED4(void) {
    /* Original pushes r4/r6/r7 and pops them after the helper calls,
     * so their incoming values must survive everything below. */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Which navi is currently equipped on the PET. */
    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* Read stat 0x3E of that navi. */
    R0 = navi;
    R1 = 0x3E; /* stat id to read */
    GUEST_CALL(GetCurPETNaviStatsHword);
    u32 baseStat = R0;

    /* Add the 16-bit value tracked by the toolkit's oToolkit_Unk2004334_Ptr. */
    u32 extra = LD16(LD32(R10 + oToolkit_Unk2004334_Ptr));
    u32 total = baseStat + extra;

    /* Store the sum into stat 0x42. */
    R0 = navi;
    R1 = 0x42; /* stat id to write */
    R2 = total;
    GUEST_CALL(SetCurPETNaviStatsHword);

    /* Original leaves r2 = sum and restores r4/r6/r7 from the stack. */
    R2 = total;
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── eLinkState_ResetForNewUse_803DE88 ── */
/* eLinkState_ResetForNewUse_803DE88(r0 = state pointer):
 * Reset the link/camera state block for a new use: clear the eLinkState
 * struct, its 0x10-byte auxiliary buffer and a 0x40-byte work area, then
 * store the given pointer at eLinkState+0x58 (dword_2009A28).
 * Flags and R1 are whatever the final ZeroFillByHalfword call leaves;
 * r2-r7 are untouched. */
Next eLinkState_ResetForNewUse_803DE88(void) {
    u32 statePtr = R0;

    R0 = A_eCameraEnd;
    R1 = 0x58;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = A_byte_200DD10;
    R1 = 0x10;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = A_unk_2009480;
    R1 = 0x40;
    GUEST_CALL(ZeroFillByHalfword);

    ST32(A_eCameraEnd + 0x58, statePtr);

    RETURN_TO_CALLER();
}

/* ── save_CommitWorkBufferToGamePak_803FB04 ── */
/* save_CommitWorkBufferToGamePak_803FB04(r0 = save work buffer value):
 * Store r0 into the save work buffer at dword_20067C0, then copy 0x80 bytes
 * from that buffer to the Game Pak save area at 0xE007F80.
 * The original pushes/pops r0-r7, so every register is preserved. */
Next save_CommitWorkBufferToGamePak_803FB04(void) {
    u32 inR0 = R0, inR1 = R1, inR2 = R2, inR3 = R3;
    u32 inR4 = R4, inR5 = R5, inR6 = R6, inR7 = R7;

    /* Remember the value being saved, in the save work buffer. */
    ST32(A_dword_20067C0, inR0);

    /* libSave_CopyToGamePak(src = dword_20067C0, dest = 0xE007F80, len = 0x80).
     * The callee also expects the source pointer in r4. */
    R0 = A_dword_20067C0;
    R1 = 0xE007F80;
    R2 = 0x80;
    R4 = A_dword_20067C0;
    GUEST_CALL(libSave_CopyToGamePak);

    /* Restore the caller's registers (the original's pop {r0-r7,pc}). */
    R0 = inR0; R1 = inR1; R2 = inR2; R3 = inR3;
    R4 = inR4; R5 = inR5; R6 = inR6; R7 = inR7;
    RETURN_TO_CALLER();
}

/* ── owPlayer_offsetS200ace0Coords_809e1fa ── */
/* owPlayer_offsetS200ace0Coords_809e1fa(r0 = dx, r1 = dy, r2 = dz):
 * offset the player's overworld position by the given delta. The struct
 * pointer is reloaded from the literal pool into a scratch register; the
 * caller's r7 is saved on entry and restored on exit, so it is preserved.
 * Flags come from the final add (the Z update), which also leaves the new
 * Z in r3. */
enum { oS200ace0_PlayerX = 8 };

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

/* ── owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2 ── */
/* Default sprite transform values for the overworld player navi. */
enum { DEFAULT_OW_PLAYER_NAVI_ROTATION = 0 };

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
enum { oS2000aa0_Sprite_04 = 4 };

Next owPlayer_zeroS2000AA0Param0x4_809e312(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);               /* mov r0, #0 */

    R3 = A_eStruct2000aa0;
    ST8(A_eStruct2000aa0 + oS2000aa0_Sprite_04, R0);   /* strb r0, [r3, #4] */

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

/* ── selectVirusOrNaviNamesAndWhich_800EC56 ── */
/* selectVirusOrNaviNamesAndWhich_800EC56(r0 = battle chip ID):
 * Pick the text script holding the chip-name strings for the given chip ID:
 * IDs above 0xFF are navi chips, everything else is a regular virus chip.
 * Outputs: r0 = TextScript address (Virus or Navi chip name list),
 *          r1 = chip ID truncated to its low byte. */
Next selectVirusOrNaviNamesAndWhich_800EC56(void) {
    u32 chipId = R0;

    u32 nameScript = A_TextScriptVirusChipNames;
    if ((s32)chipId > 0xFF)
        nameScript = A_TextScriptNaviChipNames;

    R0 = nameScript;
    R1 = chipId & 0xFF;
    RETURN_TO_CALLER();
}

/* ── battle_countAdjacentPanelsOfOtherAlliance_800D5F0 ── */
/* battle_countAdjacentPanelsOfOtherAlliance_800D5F0(r0 = panel position, r1 = alliance id):
 * Counts the three neighbouring panels (slots 1..3) that hold an object
 * belonging to a *different* alliance; r0 = how many such panels were found.
 * (r1 ends holding the last panel's alliance byte, as in the original.) */
Next battle_countAdjacentPanelsOfOtherAlliance_800D5F0(void) {
    u32 position = R0;
    u32 alliance = R1;
    s32 count = 0;

    for (s32 slot = 1; slot <= 3; slot++) {
        R0 = position;
        R1 = slot;
        GUEST_CALL(object_getPanelDataOffset);

        u32 panel = R0;
        if (panel != 0) {
            /* oPanelData_Alliance is at offset 3 in the panel data struct. */
            R1 = LD8(panel + 3);
            if (R1 != alliance)
                count++;
        }
    }

    /* mov r0, r5 encodes as adds r0, count, #0, so it also fixes up c/v. */
    R0 = count;
    SET_FLAGS_ADD(count, 0);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchObjectContactHandler_800E498 ── */
/* battle_dispatchObjectContactHandler_800E498(r0 = handler index, r5 = battle object, r2/r3 = passed through):
 * Dispatches to one of the battle object's contact handlers selected by r0,
 * passing the object's collision direction in r0. The handler's results (and
 * every register it leaves behind) are handed straight back to our caller. */
Next battle_dispatchObjectContactHandler_800E498(void) {
    u32 index = R0;

    /* The handlers receive the direction field of the object's collision data. */
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R0 = LD8(collision + oCollisionData_Direction);

    /* Handler table (off_800E4B0), in table order: */
    switch (index) {
    case 0: GUEST_CALL(sub_800E4C0); break;
    case 1: GUEST_CALL(sub_800E548); break;
    case 2: GUEST_CALL(sub_800E4C8); break;
    case 3: GUEST_CALL(sub_800E500); break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_applyFlattenHitStatus_800EB26 ── */
/* battle_applyFlattenHitStatus_800EB26(r5 = battle object):
 * Called after collision processing. If the object took a "flattening" hit
 * (flag 0x40 in its collision flags), check its final status effect:
 * statuses 0x60..0x65 are left untouched, anything else is reset to 0x12
 * and the object gets flag 0x4000 set / flag 0x6 cleared on its flag2 word.
 * Leaves r3 = collision data pointer. */
Next battle_applyFlattenHitStatus_800EB26(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    u32 hitFlags = LD32(collision + oCollisionData_FlagsFromCollision);
    SET_FLAGS_NZ(hitFlags & 0x40);

    if (hitFlags & 0x40) {
        u32 status = LD8(collision + oCollisionData_StatusEffectFinal);

        /* Statuses 0x60..0x65 are preserved as-is; only flag state matters. */
        if ((s32)status >= 0x60 && (s32)status <= 0x65) {
            SET_FLAGS_CMP(status, 0x65);
            RETURN_TO_CALLER();
        }

        /* Any other status is reset and the object is flagged accordingly. */
        ST8(collision + oCollisionData_StatusEffectFinal, 0x12);

        R0 = 0x4000;
        GUEST_CALL(object_setFlag2);

        R0 = 6;
        GUEST_CALL(object_clearFlag2);
    }
    RETURN_TO_CALLER();
}

/* ── zeroFill_800BF66 ── */
/* zeroFill_800BF66: clear the 0xA0-byte work buffer at byte_203CF00.
 * r2/r3 pass through untouched; r0/r1 and flags are whatever
 * ZeroFillByWord leaves behind. */
Next zeroFill_800BF66(void) {
    R0 = A_byte_203CF00;
    R1 = 0xA0;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── SWI_CpuFastSet ── */
/* SWI_CpuFastSet(r0 = src, r1 = dest, r2 = length | mode):
 * BIOS call 0xC. Copies or fills words in 8-word bursts.
 *   r2 bit 24 set: fill dest with the word at [src]; otherwise copy.
 * Length (bits 0-20) is rounded up by the BIOS to a multiple of 8 words;
 * both addresses must be 4-byte aligned. */
Next SWI_CpuFastSet(void) {
    swi_call(0xC);
    RETURN_TO_CALLER();
}

/* ── SWI_CpuSet ── */
/* SWI_CpuSet: thin wrapper around BIOS CpuSet (svc 0xB).
 *   r0 = source address (word-aligned for 32-bit transfers, halfword-aligned for 16-bit)
 *   r1 = destination address (same alignment rules)
 *   r2 = length/mode: bits 0-20 = element count, bit 24 = fixed source (fill),
 *        bit 26 = 32-bit transfers when set, 16-bit otherwise.
 * No return value; the result is the data written to the destination.
 * r0 is clobbered by the BIOS; r1-r3 pass through unchanged. */
Next SWI_CpuSet(void) {
    swi_call(0xB);
    RETURN_TO_CALLER();
}

/* ── callback_noop_8033A7C ── */
/* callback_noop_8033A7C: does nothing; exists only as a callable placeholder
 * (its callers treat it as an empty callback). */
Next callback_noop_8033A7C(void) {
    RETURN_TO_CALLER();
}

/* ── battle_resetState_8033AC4 ── */
/* battle_resetState_8033AC4: reset/initialize the battle object state (self in r5 = *S2011E30).
 * Runs the four initialization steps in order:
 *   1. sub_8033B80 with r0 = 0
 *   2. sub_8033F80 with r0 = 0
 *   3. sub_8033C68
 *   4. sub_8033BE8
 * Register state (r0-r3, flags) after the call is whatever sub_8033BE8 leaves. */
Next battle_resetState_8033AC4(void) {
    R0 = 0;
    GUEST_CALL(sub_8033B80);

    R0 = 0;
    GUEST_CALL(sub_8033F80);

    GUEST_CALL(sub_8033C68);
    GUEST_CALL(sub_8033BE8);

    RETURN_TO_CALLER();
}

/* ── map_beginEnterMapOnWarpStep4_80342EC ── */
/* map_beginEnterMapOnWarpStep4_80342EC: begin the "enter map" flow (main jumptable step 4).
 * Marks the new jumptable index, wipes VRAM and the tile GFX pool,
 * programs the fade-in parameters for arriving on the new map, then
 * runs the warp-entry logic and the navi update step.
 * On entry r10 holds the toolkit pointer; it is copied to r7, which
 * the callees (map_triggerEnterMapOnWarp, navi_80340F6) read. */
Next map_beginEnterMapOnWarpStep4_80342EC(void) {
    R7 = R10;

    /* Main flow resumes at jumptable step 4. */
    u32 jumptableIndexPtr = LD32(R7); /* oToolkit_MainJumptableIndexPtr == 0 */
    ST8(jumptableIndexPtr, 4);

    /* Clear the video state before the new scene is built up. */
    GUEST_CALL(zeroFillVRAM);
    GUEST_CALL(ZeroFillGFX30025c0);

    /* Fade-in parameters for arriving on the map: param1 = 0, param2 = 0x10. */
    u32 gameState = LD32(R7 + oToolkit_GameStatePtr);
    ST8(gameState + oGameState_EnterMapFadeParam1, 0);
    ST8(gameState + oGameState_EnterMapFadeParam2, 0x10);

    /* map_triggerEnterMapOnWarp_8005C04(gameState, 0x10) */
    R0 = gameState;
    R1 = 0x10;
    GUEST_CALL(map_triggerEnterMapOnWarp_8005C04);

    /* navi_80340F6(gameState) — r0 is restored to the game state pointer
     * that the previous call may have clobbered. */
    R0 = gameState;
    GUEST_CALL(navi_80340F6);

    RETURN_TO_CALLER();
}

/* ── cutscene_TryStartNotification_8035084 ── */
/* cutscene_TryStartNotification_8035084: show the "notification" cutscene.
 * Returns 1 if the cutscene was *not* started (Slip is running, a cutscene is
 * already active, or there is nothing to notify), 0 if it was started. */
Next cutscene_TryStartNotification_8035084(void) {
    u32 result;

    /* Bail out if the Slip effect is running or a cutscene is already active. */
    GUEST_CALL(IsNotSlipRunningAndNotInCutscene);
    if (R0 != 0) {
        /* Ask whether there is a pending notification; if so, start its
         * cutscene, passing the notification kind as the parameter. */
        GUEST_CALL(sub_8035756);
        if (R0 != 0) {
            R1 = R0;                                   /* notification parameter */
            R0 = A_CutsceneScriptNotification_8098BB8; /* CutsceneScript */
            GUEST_CALL(StartCutscene);
            result = 0;
        } else {
            result = 1;
        }
    } else {
        result = 1;
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── scene_loadAndStartCutscene_812AF18 ── */
/* scene_loadAndStartCutscene_812AF18: set up a scene — run its init routine, decompress and copy its
 * asset reference table, then load its graphics/text/sprite resources and start
 * its chatbox script. All registers the callers rely on are preserved. */
Next scene_loadAndStartCutscene_812AF18(void) {
    /* Initialize the scene state. */
    GUEST_CALL(sub_812AF3C);

    /* Decompress the asset reference table and copy it into place. */
    R0 = A_off_812B08C;
    GUEST_CALL(decompAndCopyData);

    /* Load the decompressed tile/palette asset. */
    GUEST_CALL(loadDecompressedAsset_812AF78);

    /* Load and run the embedded text script archive. */
    GUEST_CALL(textScript_loadAndRunEmbeddedArchive_812AF84);

    /* Initialize the sprite decomp tables. */
    GUEST_CALL(sprite_decompInitRefs_812AF98);

    /* Scene-specific extra setup. */
    GUEST_CALL(sub_812AFA4);

    /* Start the chatbox overlay for this scene. */
    GUEST_CALL(chatbox_8040818);

    RETURN_TO_CALLER();
}

/* ── battle_initEStructArr_804657C ── */
/* battle_initEStructArr_804657C: initialise eStructArr2008450.
 * Zero-fills the whole 0x528-byte array (15 entries of 0x58 bytes each),
 * then sets every entry's byte at +2 to 0x20 and its byte at +3 to the
 * 0-based entry index.
 * Preserves r2-r7; leaves r1 as ZeroFillByWord left it. */
Next battle_initEStructArr_804657C(void) {
    R0 = A_eStructArr2008450;
    R1 = 0x528;
    GUEST_CALL(ZeroFillByWord);

    u32 entry = A_eStructArr2008450;
    for (u32 i = 0; i < 15; i++) {
        ST8(entry + 2, 0x20);   /* element type */
        ST8(entry + 3, i);      /* entry index */
        entry += 0x58;
    }

    /* Flags come from the init loop's final `cmp r7, #0xF` (r7 == 0xF here). */
    SET_FLAGS_CMP(0xF, 0xF);
    RETURN_TO_CALLER();
}

/* ── initWorkArea5Entries_80467D8 ── */
/* initWorkArea5Entries_80467D8: clear the 0x1B8-byte work block at byte_2011860, then set up
 * its five 0x58-byte entries: byte 2 = 0x20 (entry "alive"/default value),
 * byte 3 = entry index. r0 = 0x20, r1 = 0x1B8 afterwards; r2-r7 preserved. */
Next initWorkArea5Entries_80467D8(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R0 = A_byte_2011860;
    R1 = 0x1B8; /* 5 entries * 0x58 bytes */
    GUEST_CALL(ZeroFillByWord);

    u32 entry = A_byte_2011860;
    for (u32 i = 0; i < 5; i++) {
        ST8(entry + 2, 0x20); /* entry state */
        ST8(entry + 3, i);    /* entry index */
        entry += 0x58;
    }

    /* Exit state of the original's last CMP r7, #5 (counter ended at 5). */
    SET_FLAGS_CMP(5, 5);
    R0 = 0x20;
    R1 = 0x1B8;
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── chatbox_stepContextScript_8047028 ── */
/* chatbox_stepContextScript_8047028: run the chatbox script attached to the dialogue context in R5.
 *
 * First updates some inter-frame state (sub_8047134), then, while the
 * 0x8 event flag is set, stores the text scroll speed into the context's
 * state byte and starts the script indexed from the context:
 *   - offset 0x12 (halfword) nonzero -> script #1,  speed 8
 *   - offset 0x12 (halfword) zero    -> script #10, speed 32
 * The script itself is read from the archive pointer at offset 0x24.
 *
 * Always returns R0 = 0 (Z set), with R1/R2 left as the callees left them.
 * Preserves R4-R7 and everything from R8 up.
 */
Next chatbox_stepContextScript_8047028(void) {
    GUEST_CALL(sub_8047134);

    /* Proceed only while event flag 0x8 is set. */
    R0 = 8;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    if (R0 != 0) {
        /* Pick the script and scroll speed based on the repeat counter. */
        u32 scriptIdx = 1;
        u32 scrollSpeed = 8;

        if (LD16(R5 + 0x12) == 0) {
            scriptIdx = 0xA;
            scrollSpeed = 0x20;
        }

        ST8(R5, scrollSpeed);
        R0 = LD32(R5 + 0x24);
        R1 = scriptIdx;
        GUEST_CALL(chatbox_runScript);
    }

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_objectEnterState4LatchTilePos_80A4AB8 ── */
/* battle_objectEnterState4LatchTilePos_80A4AB8 (r5 = battle object):
 * Put the object into state 4 and latch its current tile coordinates
 * (bytes at +5/+6) into its position fields (+0x24/+0x28),
 * then run the follow-up update in sub_80A4ACC. */
Next battle_objectEnterState4LatchTilePos_80A4AB8(void) {
    ST8(R5 + 8, 4);                      /* state = 4 */

    /* copy tile coords into the position fields */
    R0 = LD8(R5 + 5);
    ST32(R5 + 0x24, R0);
    R0 = LD8(R5 + 6);
    ST32(R5 + 0x28, R0);

    /* r0 still holds the y coord at the call site, as in the original */
    GUEST_CALL(sub_80A4ACC);

    RETURN_TO_CALLER();
}

/* ── menu_stateDispatch_80A4BDC ── */
/* menu_stateDispatch_80A4BDC: tail-call dispatcher for the menu object in r5.
 *
 * The phase byte at [r5+8] is stored pre-multiplied by 4 (0, 4 or 8), so it
 * serves directly as the byte offset into the handler table at off_80A4BF0.
 * The selected handler is called with R0 = its own table entry (address with
 * thumb bit) and R1 = the raw phase byte; because the original tail-calls it
 * (mov lr, pc / bx r0), the handler's results pass straight through to this
 * function's caller. */
Next menu_stateDispatch_80A4BDC(void) {
    u32 sel = LD8(R5 + 8);

    R1 = sel;
    R0 = LD32(A_off_80A4BF0 + sel); /* handler address from the dispatch table */

    switch (sel) {
    case 0: GUEST_CALL(sub_80A4BFC); break;
    case 4: GUEST_CALL(sprite_tickWithFlagReset_80A4C46); break;
    case 8: GUEST_CALL(sub_80A4C5C); break;
    /* other byte values would index past the table and never occur */
    }

    RETURN_TO_CALLER();
}

/* ── sprite_tickWithFlagReset_80A4C46 ── */
/* sprite_tickWithFlagReset_80A4C46(r4 = unused context, r5 = sprite object):
 * Sprite animation tick: fetch the current frame's parameters, and if the
 * frame carries the 0x80 flag, reset the object's byte at +8 to 8 (probably
 * a sub-action/animation counter), then advance the sprite animation.
 * Flags and r0-r3 on return are whatever sprite_update leaves. */
Next sprite_tickWithFlagReset_80A4C46(void) {
    GUEST_CALL(sprite_getFrameParameters);

    if (R0 & 0x80)
        ST8(R5 + 8, 8);

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── getChip8021DA8 ── */
/* getChip8021DA8(r0 = chip index) -> r0 = &ChipDataArr[idx]
 * Returns the address of one 44-byte ChipData entry; the flags left behind are
 * those of the address addition (callers may branch on c/v). */
Next getChip8021DA8(void) {
    R1 = A_ChipDataArr;
    R2 = 44;                 /* sizeof(ChipData) */
    R0 = R0 * R2;            /* byte offset of the chip's entry */
    SET_FLAGS_ADD(R0, R1);   /* ADDS r0, r0, r1 */
    R0 = R0 + R1;
    RETURN_TO_CALLER();
}

/* ── split9BitsFromBitfield_8021AE0 ── */
/* split9BitsFromBitfield_8021AE0(r0 = bitfield):
 * Splits a bitfield into its lower and upper 9 bits:
 *   r0 = low 9 bits, r1 = high 9 bits (bitfield >> 9).
 * Bits 16-31 of the input are assumed to be unset.
 *
 * r2 receives the mask word loaded from the literal pool. The pool
 * entry only defines a halfword (0x01FF); the high half of the loaded
 * word comes from the bytes that follow the pool in ROM, which are the
 * next function's `push {lr}` opcode (0xB510). So callers observe
 * r2 = 0xB51001FF. Only the low 9 bits of that word act as the mask,
 * but r2 is a visible output and must match the original exactly. */
Next split9BitsFromBitfield_8021AE0(void) {
    u32 bitfield = R0;
    const u32 Mask9Bits = 0xB51001FF; /* literal pool word at .Mask9Bits */

    R1 = bitfield >> 9;        /* upper 9 bits */
    R2 = Mask9Bits;
    R0 = bitfield & Mask9Bits; /* mask: lower 9 bits */
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── sub_81340DC ── */
/* sub_81340DC: run the update routine for the data at byte_813314C, then, if
 * battle struct slot 3 currently exists, reposition its sprite to (0xC0, 3)
 * (with the caller's r2/r3 and the struct pointer passed through to
 * sprite_setCoordinates). Preserves r4-r7 like the original's push/pop. */
Next sub_81340DC(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R0 = A_byte_813314C;
    GUEST_CALL(sub_80465A0);

    R0 = 3;
    GUEST_CALL(getStructFrom2008450);

    /* getStructFrom2008450 returns the struct in r1 and null when slot 3 is
     * empty (that is what the original's beq tests); skip the move then. */
    if (R1 != 0) {
        R5 = R1;
        R0 = 0xC0;
        R1 = 3;
        GUEST_CALL(sprite_setCoordinates);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── main_pollGeneralLCDStatus_STAT_LYC_ ── */
/* main_pollGeneralLCDStatus_STAT_LYC_: wait until the LYC bit of the general
 * LCD status register (GeneralLCDStatus_STAT_LYC_) reads as 0, i.e. the
 * display is no longer in the vertical blank/coincidence state being polled.
 * Clobbers r0-r2; returns to caller when the bit clears. */
Next main_pollGeneralLCDStatus_STAT_LYC_(void) {
    u32 *status = (u32 *)A_GeneralLCDStatus_STAT_LYC_;
    while ((u16)LD16((u32)status) & 1) {
        /* spin until STAT bit 0 (vblank flag) is clear */
    }
    RETURN_TO_CALLER();
}

/* ── reqBBS_setFlags_8140A24 ── */
/* reqBBS_setFlags_8140A24: mark the request-board (BBS) event flags as set.
 * Sets the 0x40 flags of EVENT_1640 and the 0x10 flags of EVENT_16C0 via
 * SetEventFlagRangeFromImmediate, then returns 0. */
Next reqBBS_setFlags_8140A24(void) {
    /* EVENT_1640: event word 0x16, 0x40 entries starting at byte flag 0x40 */
    R0 = 0x16;
    R1 = 0x40;
    R2 = 0x40;
    GUEST_CALL(SetEventFlagRangeFromImmediate); /* (u8 entryIdx, u8 byteFlagIdx, int numEntries) */

    /* EVENT_16C0: event word 0x16, 0x10 entries starting at byte flag 0xC0 */
    R0 = 0x16;
    R1 = 0xC0;
    R2 = 0x10;
    GUEST_CALL(SetEventFlagRangeFromImmediate);

    R0 = 0; /* always report success */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_resetPrinterState_801FE00 ── */
/* chatbox_resetPrinterState_801FE00: reset/clear the text-printer state used by message boxes.
 * Zeroes the printer's word-sized state, its halfword flags, marks its
 * current character slot as unused (0xFF), and clears its two buffers.
 * Leaves r2 untouched; r3 ends pointing at the printer state struct. */
Next chatbox_resetPrinterState_801FE00(void) {
    /* clear the pending-word/state field */
    ST32(A_eStruct203F7D8, 0);

    /* clear the printer flags and mark the current char slot empty */
    ST16(A_eStruct2036780 + 2, 0);   /* eFlags2036782 */
    ST8(A_eStruct2036780 + 4, 0xFF); /* byte_2036784  */

    /* keep the struct pointer live in r3, as the original does */
    R3 = A_eStruct2036780;

    /* clear the 0x200-byte text buffer ... */
    R0 = A_dword_203F4A0;
    R1 = 0x200;
    GUEST_CALL(ZeroFillByEightWords);

    /* ... and the 0x100-byte secondary buffer */
    R0 = A_dword_203CBE0;
    R1 = 0x100;
    GUEST_CALL(ZeroFillByEightWords);

    RETURN_TO_CALLER();
}
