/*
 * map_script_cutscene.c: readable functions of bn6f map_script_cutscene.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── MapScriptCmd_end ── */
/* MapScriptCmd_end: terminates a map script; sets r0 = 0 and returns.
 * Flags: set to reflect the zero result (NZ), as callers may branch on them. */
Next MapScriptCmd_end(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump ── */
/* MapScriptCutsceneCmd_jump (08035924): map-script "jump" command handler.
 *
 * Marks the script reader with command id 1 (r6) and calls ReadMapScriptWord,
 * which consumes the next word from the script and performs the actual jump
 * (updating the script pointer in r4). Afterwards r7 is refreshed from r4,
 * and the command always reports success (r0 = 1). */
Next MapScriptCutsceneCmd_jump(void) {
    R6 = 1;                       /* command id passed to ReadMapScriptWord */
    GUEST_CALL(ReadMapScriptWord);

    /* "mov r7, r4" assembles to adds r7, r4, #0, which sets the flags;
     * the following "mov r0, #1" then only updates n/z, leaving c/v from
     * that add (always 0, since adding 0 cannot carry or overflow). */
    R7 = R4;
    SET_FLAGS_ADD(R4, 0);

    R0 = 1;                       /* command handled */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump_if_progress_in_range ── */
/* MapScriptCutsceneCmd_jump_if_progress_in_range:
 * Map script command "jump if game progress in range".
 *
 * Script layout: [opcode] [min progress] [max progress] [target word]
 * Reads the current game progress, then the min/max bounds and the jump
 * target from the script. If progress lies within [min, max], the script
 * pointer (r7) is set to the target word; otherwise it is advanced past
 * the whole command (r7 += 7). r0 = 1 in both cases.
 *
 * Note: the game progress byte lives at offset 6 of the game state struct
 * (no named macro for it in this build). ReadMapScriptByte/Word return
 * their result in r4 and may look at r0/r1/r7/r10, so r1 must already
 * hold the lower bound before the upper bound is read. */
Next MapScriptCutsceneCmd_jump_if_progress_in_range(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 progress = LD8(gameState + 6);

    R6 = 1;                                  /* read lower bound */
    GUEST_CALL(ReadMapScriptByte);
    u32 minProgress = R4;
    R1 = minProgress;                        /* original keeps it in r1 across the second read */

    R6 = 2;                                  /* read upper bound */
    GUEST_CALL(ReadMapScriptByte);
    u32 maxProgress = R4;

    if ((s32)progress < (s32)minProgress || (s32)progress > (s32)maxProgress) {
        R7 = R7 + 7;                         /* skip the rest of the command */
        SET_FLAGS_ADD(R7, 7);
        R0 = 1;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    R6 = 3;                                  /* read jump target */
    GUEST_CALL(ReadMapScriptWord);
    R7 = R4;                                 /* jump to the target */
    SET_FLAGS_ADD(R4, 0);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump_if_flag_set ── */
/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_map_script_byte_MapScriptCutsceneCmd_jump_if_flag_set(void) {
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    return R4;
}

/* register form: calls ReadMapScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_map_script_halfword_MapScriptCutsceneCmd_jump_if_flag_set(void) {
    R6 = 2;
    GUEST_CALL(ReadMapScriptHalfword);
    return R4;
}

/* register form: calls ReadMapScriptWord outside this file, which has no typed C interface yet */
static u32 read_map_script_word_MapScriptCutsceneCmd_jump_if_flag_set(void) {
    R6 = 4;
    GUEST_CALL(ReadMapScriptWord);
    return R4;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* Result of the cutscene jump-if-flag script command. */
struct CutsceneJump {
    u32 flag;         /* final R4: jump-target word (set) or event flag (skip) */
    u32 position_out; /* final R7: jump target (set) or skip-ahead by 8 (not set) */
};

/*
 * Decide whether the cutscene script jumps to a stored offset or skips the
 * jump-target field.  Reads one byte (or halfword sentinel) from the script,
 * resolves it against the 16-bit flag table, then either reads a 32-bit jump
 * offset or advances the position past the target slot.
 *
 * flag_table (r5): base address of the halfword event-flag table.
 * position   (r7): current script cursor (RAM address).
 */
static struct CutsceneJump cutscene_jump_if_flag(u32 flag_table, u32 position) {
    struct CutsceneJump result;

    /* Read the flag-selector byte from the script stream. */
    u32 selector = read_map_script_byte_MapScriptCutsceneCmd_jump_if_flag_set();

    if (selector == 0xFF) {
        /* Sentinel: the event flag is encoded in the next halfword. */
        selector = read_map_script_halfword_MapScriptCutsceneCmd_jump_if_flag_set();
    } else {
        /* Normal case: selector is a halfword offset into the flag table. */
        selector = LD16(flag_table + selector);
    }

    if (event_flag_set(selector)) {
        /* Flag is set – read the 32-bit jump offset and redirect the script. */
        u32 jump_target = read_map_script_word_MapScriptCutsceneCmd_jump_if_flag_set();
        result.flag = jump_target;
        result.position_out = jump_target;
    } else {
        /* Flag is clear – skip the 8-byte (word) jump-target field. */
        result.flag = selector;
        result.position_out = position + 8;
    }

    return result;
}

/* MapScriptCutsceneCmd_jump_if_flag_set(r5 = flag_table, r7 = position)
 * -> r0 = 1, r4 = jump-target/flag, r7 = new position, nzcv */
Next MapScriptCutsceneCmd_jump_if_flag_set(void) {
    struct CutsceneJump j = cutscene_jump_if_flag(R5, R7);
    R0 = 1;
    R4 = j.flag;
    R7 = j.position_out;
    SET_FLAGS_ADD(1, 0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump_if_flag_range_set ── */
/*
 * MapScriptCutscene command "jump if event-flag range is set" (0x08035992).
 *
 * Inline arguments in the script stream:
 *   +0: byte     - first range description value
 *   +1: halfword - second range description value
 *   +4: word     - script offset to jump to when the range is set
 *
 * The two description values are handed to TestEventFlagRange exactly as the
 * original lays them out (halfword value in r0, byte value in both r1 and
 * r2); that callee answers in the flags. When every flag in the range is
 * set, the inline word is consumed and becomes the new script position
 * (r7); otherwise the interpreter just skips the whole command (r7 += 8,
 * covering byte + halfword + word + padding). Like every script command,
 * this one reports "continue" (r0 = 1) and leaves the last value read in
 * r4 for the interpreter.
 */

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_map_script_byte_MapScriptCutsceneCmd_jump_if_flag_range_set(void) {
    R6 = 1;                        /* read one byte */
    GUEST_CALL(ReadMapScriptByte);
    return R4;                     /* the ReadMapScript* family returns the value in r4 */
}

/* register form: calls ReadMapScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_map_script_halfword_MapScriptCutsceneCmd_jump_if_flag_range_set(u32 r1_value) {
    /* The original keeps the byte just read live in r1 across this call
     * (mov r1, r4 before the BL), so the helper has to restore it. */
    R1 = r1_value;
    R6 = 2;                        /* read two bytes */
    GUEST_CALL(ReadMapScriptHalfword);
    return R4;
}

/* register form: calls ReadMapScriptWord outside this file, which has no typed C interface yet */
static u32 read_map_script_word(void) {
    R6 = 4;                        /* read four bytes */
    GUEST_CALL(ReadMapScriptWord);
    return R4;
}

/* register form: calls TestEventFlagRange outside this file, which has no typed C interface yet */
static bool event_flag_range_set(u32 arg_r0, u32 arg_r1, u32 arg_r2) {
    R0 = arg_r0;
    R1 = arg_r1;
    R2 = arg_r2;
    GUEST_CALL(TestEventFlagRange);
    return !ZERO_FLAG();           /* Z set when the flag range is not fully set */
}

typedef struct {
    u32 jump_target; /* r7 on exit: new script position */
    u32 last_read;   /* r4 on exit: value of the last script read */
    u32 add_lhs;     /* operands of the final ADDS, for C/V flag emulation */
    u32 add_rhs;
} FlagRangeJumpResult;

static FlagRangeJumpResult cmd_jump_if_flag_range_set(u32 script_pos) {
    FlagRangeJumpResult out;

    u32 range_a = read_map_script_byte_MapScriptCutsceneCmd_jump_if_flag_range_set();
    u32 range_b = read_map_script_halfword_MapScriptCutsceneCmd_jump_if_flag_range_set(range_a);

    if (event_flag_range_set(range_b, range_a, range_a)) {
        /* All flags in the range are set: jump to the inline word offset. */
        u32 target = read_map_script_word();
        out.jump_target = target;
        out.last_read = target;
        out.add_lhs = target;      /* adds r7, r4, #0 */
        out.add_rhs = 0;
    } else {
        /* Range not fully set: skip past the command's inline data. */
        out.jump_target = script_pos + 8;
        out.last_read = range_b;
        out.add_lhs = script_pos;  /* adds r7, r7, #8 */
        out.add_rhs = 8;
    }
    return out;
}

Next MapScriptCutsceneCmd_jump_if_flag_range_set(void) {
    FlagRangeJumpResult out = cmd_jump_if_flag_range_set(R7);
    R7 = out.jump_target;
    R4 = out.last_read;
    R0 = 1;
    SET_FLAGS_ADD(out.add_lhs, out.add_rhs); /* C/V as the original's last ADDS leaves them */
    SET_FLAGS_NZ(R0);                        /* mov r0, #1 */
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_jump_if_flag_range_clear ── */
/* MapScriptCutsceneCmd_jump_if_flag_range_clear:
 * Map script command "jump if a range of event flags is clear".
 *
 * Operands, read from the script with the ReadMapScript* helpers (which
 * return their value in r4): a byte giving how many flags to test, then a
 * halfword giving the first flag index. TestEventFlagRange reports whether
 * any flag in [first, first+count) is set. If the whole range is clear, a
 * word operand read afterwards becomes the new script position (jump taken);
 * otherwise the interpreter just steps over the entire command. Both paths
 * return r0 = 1.
 */
Next MapScriptCutsceneCmd_jump_if_flag_range_clear(void) {
    R6 = 1;                          /* ReadMapScript*: fetch a byte operand */
    GUEST_CALL(ReadMapScriptByte);
    u32 flagCount = R4;

    R1 = flagCount;                  /* kept around for TestEventFlagRange */
    R6 = 2;                          /* fetch a halfword operand */
    GUEST_CALL(ReadMapScriptHalfword);

    R0 = R4;                         /* first flag index of the range */
    R2 = R1;                         /* number of flags in the range */
    GUEST_CALL(TestEventFlagRange);  /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {              /* at least one flag set: no jump */
        SET_FLAGS_ADD(R7, 8);
        R7 = R7 + 8;                 /* skip the byte, halfword and word operands */
        R0 = 1;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    R6 = 4;                          /* fetch a word operand */
    GUEST_CALL(ReadMapScriptWord);
    SET_FLAGS_ADD(R4, 0);
    R7 = R4;                         /* jump destination becomes the script position */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_coordinate_trigger_not_equal_cmd_8035b44 ── */
/* Map-script command 0x8035b44 -- "coordinate trigger, not equal".
 * Reads a signed operand byte, folds it (shifted into the top halfword) into the
 * player's third coordinate word and asks checkCoordinateTrigger for the trigger
 * value at the player's position.  While that value equals the next script byte
 * the interpreter merely skips this 7-byte command (script pc += 7); once it
 * differs the pc jumps to the word operand that follows.  Always answers 1
 * ("keep interpreting"). */

/* Raw player-object field offsets as in the disassembly. */
#define PLAYER_COORD_X 0x1C
#define PLAYER_COORD_Y 0x20
#define PLAYER_COORD_Z 0x24

/* Both halves of checkCoordinateTrigger_8031a7a's answer, as the callee leaves
 * them in r0/r1; the command falls back to r1 when r0 comes back zero. */
typedef struct {
    u32 primary;   /* r0 */
    u32 fallback;  /* r1 */
} CoordinateTriggerAnswer;

/* What the command leaves behind for the script interpreter. */
typedef struct {
    u32 script_pc; /* r7: where interpretation continues */
    u32 status;    /* r0: always 1 ("keep running") */
} MapScriptCommandResult;

/* register form: calls ReadMapScriptSignedByte outside this file, which has no typed
 * C interface yet (operand size in r6 = 2, result in r4, as the original relies on) */
static u32 read_map_script_signed_byte(void) {
    R6 = 2;
    GUEST_CALL(ReadMapScriptSignedByte);
    return R4;
}

/* register form: calls ReadMapScriptByte outside this file, which has no typed C
 * interface yet (operand size in r6 = 1, result in r4) */
static u32 read_map_script_byte_MapScriptCutsceneCmd_coordinate_trigger_not_equal_cmd_8035b44(void) {
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    return R4;
}

/* register form: calls ReadMapScriptWord outside this file, which has no typed C
 * interface yet (operand size in r6 = 3, result in r4) */
static u32 read_map_script_word_MapScriptCutsceneCmd_coordinate_trigger_not_equal_cmd_8035b44(void) {
    R6 = 3;
    GUEST_CALL(ReadMapScriptWord);
    return R4;
}

/* register form: calls checkCoordinateTrigger_8031a7a outside this file, which has
 * no typed C interface yet (takes the coordinate triple by guest address) */
static CoordinateTriggerAnswer check_coordinate_trigger(u32 coords_addr) {
    CoordinateTriggerAnswer answer;
    R0 = coords_addr;
    GUEST_CALL(checkCoordinateTrigger_8031a7a);
    answer.primary = R0;
    answer.fallback = R1;
    return answer;
}

/* Equal trigger value -> fall through past this command; not equal -> jump to the
 * word operand. */
static MapScriptCommandResult run_coordinate_trigger_not_equal(u32 script_pc /* r7 */,
                                                               u32 toolkit /* r10 */,
                                                               u32 return_addr /* lr on entry */) {
    MapScriptCommandResult out;

    /* register form: the whole command runs on the guest stack -- push {lr} saves
     * the return address at SP_entry-4 and a 3-word coordinate buffer is placed
     * below it; its guest address is handed to checkCoordinateTrigger and every
     * callee must see the original's sp, so the writes land byte-for-byte alike */
    SP -= 4;                            /* push {lr} */
    ST32(SP, return_addr);

    u32 operand = read_map_script_signed_byte();

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 player = LD32(game_state + oGameState_OverworldPlayerObjectPtr);
    u32 x = LD32(player + PLAYER_COORD_X);
    u32 y = LD32(player + PLAYER_COORD_Y);
    u32 z = LD32(player + PLAYER_COORD_Z) + (operand << 16);

    SP -= 12;                           /* coordinate triple below the saved lr */
    ST32(SP, x);
    ST32(SP + 4, y);
    ST32(SP + 8, z);
    CoordinateTriggerAnswer answer = check_coordinate_trigger(SP);
    SP += 12;

    /* The command uses the callee's r1 answer only when its r0 is zero. */
    u32 trigger_value = answer.primary != 0 ? answer.primary : answer.fallback;

    u32 compare_byte = read_map_script_byte_MapScriptCutsceneCmd_coordinate_trigger_not_equal_cmd_8035b44();
    if (trigger_value == compare_byte)
        out.script_pc = script_pc + 7;            /* equal: skip this 7-byte command */
    else
        out.script_pc = read_map_script_word_MapScriptCutsceneCmd_coordinate_trigger_not_equal_cmd_8035b44();   /* not equal: jump to the word operand */

    SP += 4;                            /* pop {pc} */
    out.status = 1;
    return out;
}

/* 0x8035b44 (r7 = script pc, r10 = toolkit, lr = return address) -> r7, r0 = 1, nz */
Next MapScriptCutsceneCmd_coordinate_trigger_not_equal_cmd_8035b44(void) {
    MapScriptCommandResult result = run_coordinate_trigger_not_equal(R7, R10, LR);
    R7 = result.script_pc;
    R0 = result.status;
    SET_FLAGS_NZ(result.status);
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

/* ── MapScriptCmd_jump_if_map_group_compare_last_map_group ── */
/* MapScriptCmd_jump_if_map_group_compare_last_map_group: 08035C6E-08035CA0 THUMB, bn6f map_script_cutscene.s:675
 *   inputs:  r7 r10 lr
 *   outputs: nz r7
 *   clobbers: c v r0 r1 r4 r6 lr
 *   preserved: -
 *
 * Map-script opcode that compares the current map group against the last map group.
 * ReadMapScriptByte is called to fetch the comparison mode (R6 = 1 means "equal",
 * anything else means "not equal"). On a match the script jumps forward by the offset
 * given in the next two bytes (R6 = 2 for ReadMapScriptWord); otherwise execution just
 * falls through the next 6-byte instruction.
 */
Next MapScriptCmd_jump_if_map_group_compare_last_map_group(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 currentMapGroup = LD16(gameState + oGameState_MapGroup);
    u32 lastMapGroup    = LD16(gameState + oGameState_LastMapGroup);

    /* Read the comparison mode byte; the result ends up in R4. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);

    bool equal;
    {
        u32 r4 = R4;
        if (r4 == 1) {
            /* equal-branch: match when current == last */
            SET_FLAGS_CMP(R0, R1);
            equal = (currentMapGroup == lastMapGroup);
        } else {
            /* not-equal-branch: match when current != last */
            equal = (currentMapGroup != lastMapGroup);
        }
    }

    if (equal) {
        /* Take the jump: read the 16-bit offset and put it in R7. */
        R6 = 2;
        GUEST_CALL(ReadMapScriptWord);
        R7 = R4;
    } else {
        /* No jump: skip past the 2-byte mode + 2-byte offset + 2-byte opcode = 6 bytes. */
        R7 = R7 + 6;
    }

    /* Both return paths leave R0 = 1, so nz is set. */
    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_set_screen_fade ── */
/* MapScriptCutsceneCmd_set_screen_fade: map-script command handler that starts
 * a screen fade. It takes two byte parameters, then calls SetScreenFade with
 * them, advances the script command pointer (r7) by 4 and returns 1.
 *
 * The parameters are located via script argument 1:
 *   - 0xFF means the two parameters follow immediately in the script stream
 *     (arguments 2 and 3, read via ReadMapScriptByte);
 *   - any other value is an offset into the script data (r5) where the two
 *     parameter bytes are stored.
 *
 * ReadMapScriptByte takes the 1-based argument index in r6 and returns the
 * argument byte in r4. */
Next MapScriptCutsceneCmd_set_screen_fade(void) {
    /* Read argument 1: immediate marker (0xFF) or data offset. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    u32 r4 = R4;

    u32 fade_param1, fade_param2;
    if (r4 == 0xFF) {
        /* Immediate form: read both parameter bytes from the script stream. */
        R6 = 2;
        GUEST_CALL(ReadMapScriptByte);
        fade_param1 = r4 = R4;

        R6 = 3;
        GUEST_CALL(ReadMapScriptByte);
        fade_param2 = r4 = R4;
    } else {
        /* Indirect form: parameters live at r5[offset] and r5[offset + 1]. */
        fade_param1 = LD8(R5 + r4);
        r4 = r4 + 1;
        fade_param2 = LD8(R5 + r4);
    }

    R0 = fade_param1;
    R1 = fade_param2;
    GUEST_CALL(SetScreenFade);

    /* r4 ends as the last position touched: the offset+1 in the indirect
     * form, or the second immediate byte in the immediate form. */
    R4 = r4;

    /* Advance to the next map-script command. The add sets c/v (never
     * actually carry here), which the callers can observe; the final
     * `mov r0, #1` then sets n/z on top of them. */
    SET_FLAGS_ADD(R7, 4);
    R7 = R7 + 4;
    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_set_enter_map_screen_fade ── */
/* MapScriptCutsceneCmd_set_enter_map_screen_fade: cutscene command "set
 * enter-map screen fade". Reads the command's two byte operands from the map
 * script stream and stores them as the game state's enter-map fade
 * parameters, then skips past the command (opcode + 2 operand bytes) and
 * reports that one script step was consumed.
 *
 * Inputs:  R7 = current map script pointer, R10 = toolkit, R0 = untouched
 *          script state handed through to ReadMapScriptByte.
 * Outputs: R0 = 1 (one command consumed), R7 advanced by 3,
 *          gameState_EnterMapFadeParam1/2 written, nz from the final MOV.
 */
Next MapScriptCutsceneCmd_set_enter_map_screen_fade(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    R1 = gameState;

    /* Read operand 1 and store it as the first fade parameter. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    ST8(R1 + oGameState_EnterMapFadeParam1, R4);

    /* Read operand 2 and store it as the second fade parameter. */
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);
    ST8(R1 + oGameState_EnterMapFadeParam2, R4);

    R7 += 3; /* skip the opcode and its two operands */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_set_event_flag ── */
/* MapScriptCutsceneCmd_set_event_flag: map script command that sets an event
 * flag. The flag number is read from the script stream: normally a single
 * byte, but a byte of 0xFF is the "wide operand" marker, in which case a
 * 16-bit literal is read instead. Afterwards the script pointer is advanced
 * by 4 and r0 = 1 (command handled) is returned. */
Next MapScriptCutsceneCmd_set_event_flag(void) {
    /* ReadMapScriptByte/ReadMapScriptHalfword expect the operand size in r6
     * (1 = byte, 2 = halfword) and leave the value they consumed in r4. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);

    if (R4 == 0xFF) {
        /* 0xFF marker: the actual flag number is an immediate halfword. */
        R6 = 2;
        GUEST_CALL(ReadMapScriptHalfword);
    } else {
        /* Otherwise the byte is an index into a halfword table at r5. */
        R4 = LD16(R5 + R4);
    }

    R0 = R4;
    GUEST_CALL(SetEventFlag);

    /* Advance the script command pointer past this command
     * (add r7, #4 sets c/v; the following mov r0, #1 then sets nz). */
    u32 advanced = R7 + 4;
    SET_FLAGS_ADD(R7, 4);
    R7 = advanced;

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_set_event_flag_range ── */
/* MapScriptCutsceneCmd_set_event_flag_range (0x08035DF8):
 * Map-script/cutscene command that sets a run of event flags.
 *
 * Reads two parameters out of the script stream — a single byte and a
 * halfword (r6 selects the read size: 1 = byte, 2 = halfword; the readers
 * work from r7/r10 and preserve the other arguments) — then hands them to
 * SetEventFlagRange: the halfword selects the entry flag bitfield and the
 * byte is passed as its remaining argument.
 *
 * Afterwards the 4-byte command (opcode + byte + halfword) is consumed by
 * advancing r7 past it — the ADDS sets the outgoing nz/c/v flags — and the
 * command reports "keep running" by returning 1 in r0. */
Next MapScriptCutsceneCmd_set_event_flag_range(void) {
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    u32 flagRangeByte = R4;

    /* The byte value rides in r1 across the halfword read, since
     * ReadMapScriptHalfword preserves r1. */
    R1 = flagRangeByte;
    R6 = 2;
    GUEST_CALL(ReadMapScriptHalfword);
    u32 entryFlagBitfield = R4;

    R0 = entryFlagBitfield;
    R2 = flagRangeByte;
    GUEST_CALL(SetEventFlagRange);

    /* Consume the 4-byte command; ADDS r7, #4 produces the final flags. */
    u32 scriptPos = R7;
    R7 = scriptPos + 4;
    SET_FLAGS_ADD(scriptPos, 4);

    R0 = 1; /* mov without S: flags stay as the ADDS left them */
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_clear_event_flag_range ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r7; } call_ReadMapScriptByte_result_MapScriptCutsceneCmd_clear_event_flag_range;
/* register form: generated wrapper: ReadMapScriptByte has no typed C interface yet */
static inline call_ReadMapScriptByte_result_MapScriptCutsceneCmd_clear_event_flag_range call_ReadMapScriptByte_MapScriptCutsceneCmd_clear_event_flag_range(u32 r6, u32 r7) {
    R6 = r6;
    R7 = r7;
    GUEST_CALL(ReadMapScriptByte);
    return (call_ReadMapScriptByte_result_MapScriptCutsceneCmd_clear_event_flag_range){ R4, R7 };
}

typedef struct { u32 r4, r6, r7; } call_ReadMapScriptHalfword_result;
/* register form: generated wrapper: ReadMapScriptHalfword has no typed C interface yet */
static inline call_ReadMapScriptHalfword_result call_ReadMapScriptHalfword(u32 r6, u32 r7) {
    R6 = r6;
    R7 = r7;
    GUEST_CALL(ReadMapScriptHalfword);
    return (call_ReadMapScriptHalfword_result){ R4, R6, R7 };
}

typedef struct { u32 r1, r2, r3, r4, r5; } call_ClearEventFlagRange_result;
/* register form: generated wrapper: ClearEventFlagRange has no typed C interface yet */
static inline call_ClearEventFlagRange_result call_ClearEventFlagRange(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(ClearEventFlagRange);
    return (call_ClearEventFlagRange_result){ R1, R2, R3, R4, R5 };
}

/* Map script cutscene command: "clear a range of event flags".
 * Layout of the command in the script stream: one byte giving how many flags
 * to clear, followed by a halfword giving the first flag index. Afterwards
 * the script cursor (r7) is advanced by 6 bytes total (1 + 2 + 3 for the
 * trailing skip done here via the +4). The command returns "keep going" (1),
 * and callers branch on the flags left by the final `mov r0, #1` (Thumb
 * format-3 MOV sets N/Z), i.e. always N=0, Z=0.
 *
 * r2 and r5 are outputs only because the original leaves ClearEventFlagRange's
 * values in them; they are passed through untouched. */

typedef struct {
    u32 cursor;  /* r7: updated script position */
    u32 r2;      /* leftover register state from ClearEventFlagRange */
    u32 r5;      /* leftover register state from ClearEventFlagRange */
} ClearFlagRangeCmdResult;

static ClearFlagRangeCmdResult clear_event_flag_range(u32 cursor /* r7 */) {
    ClearFlagRangeCmdResult out;

    /* Read the number of flags to clear (byte operand, reader mode 1). */
    call_ReadMapScriptByte_result_MapScriptCutsceneCmd_clear_event_flag_range count_read = call_ReadMapScriptByte_MapScriptCutsceneCmd_clear_event_flag_range(1, cursor);
    u32 count = count_read.r4;
    cursor = count_read.r7;

    /* Read the first flag index to clear (halfword operand, reader mode 2). */
    call_ReadMapScriptHalfword_result start_read = call_ReadMapScriptHalfword(2, cursor);
    u32 start_flag = start_read.r4;
    cursor = start_read.r7;

    /* Clear flags start_flag .. start_flag + count - 1 (uses r10 context). */
    call_ClearEventFlagRange_result cleared = call_ClearEventFlagRange(start_flag, count);

    out.cursor = cursor + 4;
    out.r2 = cleared.r2;
    out.r5 = cleared.r5;
    return out;
}

/* MapScriptCutsceneCmd_clear_event_flag_range(r7 = script cursor, r10 = script context)
 *   -> nz, r2, r5, r7 */
Next MapScriptCutsceneCmd_clear_event_flag_range(void) {
    ClearFlagRangeCmdResult res = clear_event_flag_range(R7);

    R7 = res.cursor;
    R2 = res.r2;
    R5 = res.r5;
    SET_FLAGS_NZ(1);  /* from `mov r0, #1`: result 1, so N=0 Z=0 */
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_write_byte ── */
/* MapScriptCutsceneCmd_write_byte(r7 = script cursor, lr = return address):
 *   Read a 16-bit word from the script into r4 (the destination address),
 *   read a single byte from the script into r4, store the byte at that
 *   address, advance the cursor past both reads, and return 1 in r0.
 *   inputs:  r7 lr
 *   outputs: nz r4 r7
 *   clobbers: c v r0 r6 lr
 *   preserved: - */
Next MapScriptCutsceneCmd_write_byte(void) {
    /* bl ReadMapScriptWord -- r6 = 1 selects a 16-bit word read, result in r4 */
    R6 = 1;
    GUEST_CALL(ReadMapScriptWord);

    /* mov r0, r4 -- destination address for the byte store */
    R0 = R4;

    /* bl ReadMapScriptByte -- r6 = 5 selects a single byte read, result in r4 */
    R6 = 5;
    GUEST_CALL(ReadMapScriptByte);

    /* strb r4, [r0] -- write the byte at the destination address */
    ST8(R0, R4);

    /* add r7, #6 -- advance the cursor past the word + byte + 3-byte header */
    R7 += 6;

    /* mov r0, #1 ; pop {pc} */
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

/* ── MapScriptCutsceneCmd_write_gamestate_byte ── */
/* MapScriptCutsceneCmd_write_gamestate_byte (0x08035EF2):
 * Map script cutscene command "write gamestate byte". Reads two bytes from
 * the script stream: a gamestate offset and a value, then stores the value
 * into the gamestate at that offset.
 *   r4  = value to write (and offset adjust)
 *   r7  = script stream pointer (advanced by 3: opcode + 2 operands)
 *   r10 = toolkit (for the gamestate pointer)
 * Always returns r0 = 1. */
Next MapScriptCutsceneCmd_write_gamestate_byte(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    /* First operand: offset within the gamestate. */
    R0 = gameState;
    R6 = 1;                              /* ReadMapScriptByte mode 1 */
    GUEST_CALL(ReadMapScriptByte);
    u32 address = R0 + R4;

    /* Second operand: byte value to store there. */
    R0 = gameState;
    R6 = 2;                              /* ReadMapScriptByte mode 2 */
    GUEST_CALL(ReadMapScriptByte);
    ST8(address, R4);

    /* Consume the opcode and its two operand bytes. */
    u32 scriptPtr = R7;
    R7 = scriptPtr + 3;
    SET_FLAGS_ADD(scriptPtr, 3);

    R0 = 1;
    SET_FLAGS_NZ(R0);                    /* Thumb "mov r0,#1" sets N/Z only */
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_write_eStruct2001c04_byte ── */
/* MapScriptCutsceneCmd_write_eStruct2001c04_byte:
 * Map script "write byte into the S2001c04 struct" command. Reads two bytes
 * from the script stream (an offset and a value, selected via r6 = 1/2) and
 * stores the value at S2001c04[offset]. Consumes 3 script bytes (r7 += 3)
 * and returns 1 in r0. */
Next MapScriptCutsceneCmd_write_eStruct2001c04_byte(void) {
    u32 s2001c04 = LD32(R10 + oToolkit_S2001c04_Ptr);

    /* First script byte: the offset within the struct. */
    R0 = s2001c04;
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    u32 offset = R4;

    /* Second script byte: the value to store. */
    R0 = s2001c04 + offset;
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);

    ST8(R0, R4);

    R7 += 3;
    SET_FLAGS_ADD(R7, 3);

    R0 = 1;
    SET_FLAGS_NZ(R0);
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

/* ── MapScriptCutsceneCmd_load_gfx_anims ── */
/* MapScriptCutsceneCmd_load_gfx_anims: 08035F3E
 * Cutscene opcode: load GFX anim scripts into the GFX anim pool.
 *
 *   ReadMapScriptWord(r7 = script ptr, r6 = 1)
 *       -> writes the FFStop32<[GFXAnimScript]> pointer into r4
 *   LoadGFXAnims(r0 = r4)
 *   advance r7 by 5 (1 opcode byte + u32 payload)
 *   return 1 (keep dispatching) with NZ set
 *
 * Inputs:  r7  cutscene stream pointer (just past the opcode byte)
 * Outputs: r1 r2 r4 r5 r7..r12 preserved from caller
 *          r0 = 1 with NZ set
 * Clobbers: r0, r3, r6, LR
 */
Next MapScriptCutsceneCmd_load_gfx_anims(void) {
    /* Pull the u32 payload following this opcode into r4. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptWord);

    /* Hand the FFStop32<[GFXAnimScript]> pointer off to the loader. */
    R0 = R4;
    GUEST_CALL(LoadGFXAnims);

    /* Skip the 1-byte opcode + 4-byte payload in the cutscene stream. */
    R7 = R7 + 5;

    /* Tell the dispatcher to keep going. */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_terminate_one_or_all_gfx_anims ── */
/* MapScriptCutsceneCmd_terminate_one_or_all_gfx_anims (08035F6A):
 * Map script / cutscene command "stop GFX animation(s)".
 *
 * Reads one byte from the script stream: 0xFF means "stop every GFX anim",
 * any other value is the slot number of a single anim to stop. The command
 * is always 2 bytes long, so the script pointer (r7) advances by 2, and the
 * command reports success by returning 1 in r0. */
Next MapScriptCutsceneCmd_terminate_one_or_all_gfx_anims(void) {
    /* ReadMapScriptByte is called with r6 = 1 (read one byte) and hands the
     * byte it read back in r4. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);

    if (R4 == 0xFF) {
        /* Stop all GFX anims, slots 0 .. NUM_GFX_ANIMS-1. r4 doubles as the
         * loop counter and is preserved by TerminateGFXAnim, so it ends up
         * as NUM_GFX_ANIMS, exactly as in the original. */
        for (R4 = 0; (s32)R4 < (s32)NUM_GFX_ANIMS; R4++) {
            R0 = R4;
            GUEST_CALL(TerminateGFXAnim);
        }
    } else {
        /* Stop only the anim whose slot number was read. */
        R0 = R4;
        GUEST_CALL(TerminateGFXAnim);
    }

    /* Advance the script pointer past this 2-byte command and return 1. */
    R7 = R7 + 2;
    SET_FLAGS_ADD(R7, 2);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_do_pet_effect ── */
/* MapScriptCutsceneCmd_do_pet_effect (08035F98):
 * Map script command that triggers a PET effect. The effect argument is
 * encoded in one of two ways:
 *   - a single script byte used as an index into the table at r5, or
 *   - a 0xFF marker followed by the effect id itself (direct value).
 * ReadMapScriptByte fetches the next script byte into r4; r6 tells it which
 * kind of operand we are currently reading (1 = indirect, 2 = direct).
 * Afterwards the script pointer r7 is advanced by 3 and the command reports
 * success (r0 = 1). The ADDS on r7 leaves C/V set as its carry/overflow,
 * which the script interpreter branches on; the final MOV only updates N/Z. */
Next MapScriptCutsceneCmd_do_pet_effect(void) {
    u32 effect;
    u32 scriptPtrBeforeAdvance;

    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);

    if (R4 == 0xFF) {
        /* 0xFF marker: the effect id follows directly in the script. */
        R6 = 2;
        GUEST_CALL(ReadMapScriptByte);
        effect = R4;
    } else {
        /* Otherwise the byte indexes the effect table at r5. */
        effect = LD8(R5 + R4);
    }

    R0 = effect;
    GUEST_CALL(doPETEffect_8033fc0);

    /* add r7, #3: C/V come from the carry/overflow of this addition. */
    scriptPtrBeforeAdvance = R7;
    R7 = scriptPtrBeforeAdvance + 3;
    SET_FLAGS_ADD(scriptPtrBeforeAdvance, 3);

    /* mov r0, #1: updates N/Z only; C/V keep the ADDS result. */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── RunSecondaryContinuousMapScript ── */
/* register form: the dispatch loop calls jumptable handlers at runtime-computed
 * addresses (no single disassembly name per target), the handler advances the
 * script cursor (r7) through the register file, and loop termination is determined
 * by the handler's Z flag. None of these can be expressed through a typed C return
 * value. The caller's r4-r7 and r12 are saved and restored in place of push/pop. */
static void run_secondary_continuous_map_script_body(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7, saved_r12 = R12;

    R5 = A_eMapScriptState;
    R0 = LD32(R5 + oMapScriptState_SecondaryContinuousMapScriptPtr);

    if (R0 != 0) {
        R4 = saved_r12;                     /* mov r4, r12 — handler sees saved r12 */
        R12 = A_MapScriptCommandJumptable;        /* ldr r6/jumptable; mov r12, r6 */
        R7 = R0;                            /* mov r7, r0 — script cursor */

        for (;;) {
            R6 = R12;                       /* mov r6, r12 — reload table base */
            R0 = LD8(R7) << 2;              /* ldrb r0,[r7]; lsl r0,r0,#2 */
            R0 = LD32(R6 + R0);             /* ldr r0, [r6,r0] — handler address */
            GUEST_CALL_ADDR(R0);            /* mov lr,pc; bx r0 — call handler */
            if (ZERO_FLAG()) break;         /* bne: Z set → stop; Z clear → continue */
        }
    }
    /* Null-script path: original tst r0,r0 sets Z (nz is clobbered by callers),
     * and leaves c/v unchanged — no explicit flag setup needed here. */

    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6;
    R7 = saved_r7; R12 = saved_r12;         /* pop {r4}; mov r12,r4; pop {r4-r7,pc} */
}

/* RunSecondaryContinuousMapScript(): execute every command in the secondary
 * continuous map script (eMapScriptState.SecondaryContinuousMapScriptPtr)
 * through the MapScriptCommandJumptable until a handler returns zero (Z set)
 * or the script pointer is null.  r4-r7 and r12 are preserved; r1-r3, r8-r11
 * and condition flags c/v are set by the last handler executed. */
Next RunSecondaryContinuousMapScript(void) {
    run_secondary_continuous_map_script_body();
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

/* ── ReadMapScriptSignedByte ── */
/* ReadMapScriptSignedByte(r6 = offset, r7 = map script pointer):
 * Reads the signed byte at r7 + r6 and returns it in r4.
 * (r7 is preserved — it still points at the script for the next read.) */
Next ReadMapScriptSignedByte(void) {
    u32 addr = R7 + R6;

    R4 = (u32)(s32)(s8)LD8(addr);
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

/* ── ReadMapScriptWord ── */
/* ReadMapScriptWord: read a 32-bit big-endian word from the map script stream.
 *   r6 = offset into script, r7 = script base pointer
 *   returns the word in r4, leaves r6/r7 set up for the next word (r7 += r6) */
Next ReadMapScriptWord(void) {
    /* add r7, r6  -> combine base + offset */
    u32 base = R7;
    R7 = base + R6;

    /* Build the 32-bit word big-endian: [base+3] [base+2] [base+1] [base] */
    u32 r4 = LD8(R7 + 3);
    R6 = LD8(R7 + 2);
    r4 = (r4 << 8) | R6;
    R6 = LD8(R7 + 1);
    r4 = (r4 << 8) | R6;
    R6 = LD8(R7);

    /* Last shift: only this one updates flags (the last LSL before the ORR chain ends). */
    u32 shifted = r4 << 8;
    SET_FLAGS_NZ(shifted | R6);   /* equivalent: flags set after final LSL/ORR sequence */
    r4 = shifted | R6;
    R4 = r4;

    /* pop {r7, pc} -- restore r7 (base pointer), return with r4 as result via R4 */
    R7 = base;
    RETURN_TO_CALLER();
}

/* ── playCertainMapMusicBasedOnEventByte_8036e44 ── */
/* playCertainMapMusicBasedOnEventByte_8036e44:
 * Pick the music track for the map the player is currently on. The per-map
 * music table is selected by byte 7 of the game state (map sub-area); within
 * it, entries are 8 bytes: a map group byte followed by a pointer to a list
 * of tracks indexed by the map number. If the current group has no entry,
 * fall back to track 0x63. Leaves r1 = map group, r2 = map number; r4-r7
 * are restored to their values on entry. */
Next playCertainMapMusicBasedOnEventByte_8036e44(void) {
    /* r4-r7 are scratch internally but must be restored (original pushes/pops them) */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 musicWork = LD32(R10 + oToolkit_S2001c04_Ptr);

    R1 = LD8(gameState + 4);   /* map group we're in        */
    R2 = LD8(gameState + 5);   /* map number within group   */

    /* select the track table for the current sub-area (game state byte 7) */
    u32 table = LD32(A_off_80360E4 + (LD8(gameState + 7) << 2));

    u32 entry = table;
    u32 track;
    for (;;) {
        u32 entryGroup = LD8(entry);
        if (entryGroup == 0xFF) {
            /* no entry for this map group: use the default track */
            track = 0x63;
            break;
        }
        if (entryGroup == R1) {
            /* found our group: look up the track for this map number */
            track = LD8(LD32(entry + 4) + R2);
            break;
        }
        entry += 8;
    }

    ST8(musicWork + 4, track); /* s2001c04 map music work area, offset 4 */

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── PlayMapMusic ── */
/* PlayMapMusic():
 * Play the music track assigned to the current map. The track number is a
 * byte stored in the map/cutscene state (S2001c04) pointed to by the toolkit. */
Next PlayMapMusic(void) {
    u32 mapState = LD32(R10 + oToolkit_S2001c04_Ptr);
    R0 = LD8(mapState + 4); /* oS2001c04_MapMusic */
    GUEST_CALL(PlayMusic); /* (song: u8) -> () */
    RETURN_TO_CALLER();
}

/* ── StartCutscene ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word(u32 dest, u32 num_bytes) {
    R0 = dest;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* Start a cutscene from the top of `script`, passing it `param`.
 * `toolkit` is the Toolkit (kept in r10) that owns the cutscene state.
 * Returns the default Unk_34 table -- the value the original leaves in r0. */
static u32 start_cutscene(u32 toolkit, u32 script, u32 param) {
    u32 state = LD32(toolkit + oToolkit_CutsceneStatePtr);

    /* Wipe the whole cutscene state, then lay in the new script. */
    zero_fill_by_word(state, oCutsceneState_Size);

    ST32(state + oCutsceneState_CutsceneScriptPos, script);
    ST32(state + oCutsceneState_originalCutsceneScriptPos_40, script);
    ST32(state + oCutsceneState_CutsceneParam32, param);

    /* The three secondary script slots start out parked on the dummy script. */
    ST32(state + oCutsceneState_CutsceneScriptPos2, A_DummyCutsceneScript);
    ST32(state + oCutsceneState_CutsceneScriptPos3, A_DummyCutsceneScript);
    ST32(state + oCutsceneState_CutsceneScriptPos4, A_DummyCutsceneScript);

    /* Dialogue comes from the cutscene text archive; Unk_34 gets its default table. */
    ST32(state + oCutsceneState_TextArchivePtr, A_eTextScript202DA04);
    ST32(state + oCutsceneState_DefaultScriptTable, A_off_8036EC4);

    return A_off_8036EC4;
}

/* StartCutscene(r0 = script, r1 = param, r10 = Toolkit) -> r0 = default Unk_34 table.
 * r1 is preserved (the original pushes it across the fill call and pops it back);
 * r2/r3 and the flags keep whatever ZeroFillByWord left behind, exactly as in the
 * original, so they are not rewritten here. */
Next StartCutscene(void) {
    u32 script = R0;
    u32 param = R1;
    R0 = start_cutscene(R10, script, param);
    R1 = param;
    RETURN_TO_CALLER();
}

/* ── cutscene_end_8036ED4 ── */
/* Script value the cutscene interpreter is parked on when a cutscene ends:
 * the "script position" marker consumed by the dummy cutscene script. */
#define CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1 1

/* cutscene_end_8036ED4: end/abandon the currently running cutscene.
 *
 * Parks the cutscene script interpreter on the dummy "do nothing" script
 * (script position 1), clears the state used to resume a script after a
 * cutscene skip, zeroes two halfword fields of the s2001c04 struct, and
 * clears event flag EVENT_1731.
 *
 * r10 = toolkit. Returns with r1/r3/c/v exactly as ClearEventFlagFromImmediate
 * left them (callers branch on its flags and reuse its r1/r3). */
Next cutscene_end_8036ED4(void) {
    u32 cutscene = LD32(R10 + oToolkit_CutsceneStatePtr);

    /* Point the script interpreter at the dummy cutscene script. */
    ST32(cutscene + oCutsceneState_CutsceneScriptPos,
         CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos2, A_DummyCutsceneScript);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos3, A_DummyCutsceneScript);
    ST32(cutscene + oCutsceneState_CutsceneScriptPos4, A_DummyCutsceneScript);

    /* Nothing to run if the player skips out of this cutscene. */
    ST32(cutscene + oCutsceneState_CutsceneScriptAfterCutsceneSkip, 0);

    /* Clear the two halfword counters in the s2001c04 struct. */
    u32 unk2001c04 = LD32(R10 + oToolkit_S2001c04_Ptr);
    ST16(unk2001c04 + oS2001c04_EncounterStepAccumulator, 0);
    ST16(unk2001c04 + oS2001c04_EncounterRollValue, 0);

    /* ClearEventFlagFromImmediate(EVENT_1731): flag group 0x17, bit 0x31. */
    R0 = 0x17;
    R1 = 0x31;
    GUEST_CALL(ClearEventFlagFromImmediate);

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

/* ── clearCutsceneScriptPosIfMagicValue0x1_8036F24 ── */
/* clearCutsceneScriptPosIfMagicValue0x1_8036F24:
 * If the cutscene's script position holds the magic "dummy" script value,
 * reset both the current and saved script positions to NULL.
 * Leaves r1 = cutscene state pointer; flags reflect the comparison. */
#define CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1 1

Next clearCutsceneScriptPosIfMagicValue0x1_8036F24(void) {
    u32 cutsceneState = LD32(R10 + oToolkit_CutsceneStatePtr);
    u32 scriptPos = LD32(cutsceneState + oCutsceneState_CutsceneScriptPos);

    R1 = cutsceneState;
    SET_FLAGS_CMP(scriptPos, CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1);

    if (scriptPos == CUTSCENE_SCRIPT_UNK_MAGIC_SCRIPT_VALUE_0x1) {
        ST32(cutsceneState + oCutsceneState_CutsceneScriptPos, 0);
        ST32(cutsceneState + oCutsceneState_originalCutsceneScriptPos_40, 0);
    }

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

/* ── cutscene_checkOriginalCutsceneScriptPos_8036F40 ── */
Next cutscene_checkOriginalCutsceneScriptPos_8036F40(void) {
    /* Read the cutscene state's saved script pointer and compare it against
     * the L-Message cutscript sentinel. Result: nz reflects whether r2 == TRUE. */
    u32 cs   = LD32(R10 + oToolkit_CutsceneStatePtr);
    u32 prev = LD32(cs + oCutsceneState_originalCutsceneScriptPos_40);

    if (prev == A_CutsceneScriptLMessage_80991F4)
        R2 = TRUE;
    else
        R2 = FALSE;

    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── cutscene_checkOriginalCutsceneScriptPos_8036F58 ── */
/* cutscene_checkOriginalCutsceneScriptPos_8036F58(r10 = toolkit ptr)
 *
 * Checks whether the cutscene state's original script position is the
 * "AskJackOut" script (CutsceneScriptAskJackOut_80988E4).
 *
 * Outputs: r2 = TRUE if it matches, FALSE otherwise; NZ flags reflect r2.
 */
Next cutscene_checkOriginalCutsceneScriptPos_8036F58(void) {
    u32 toolkit      = R10;
    u32 cutscene     = LD32(toolkit + oToolkit_CutsceneStatePtr);
    u32 origScript   = LD32(cutscene + oCutsceneState_originalCutsceneScriptPos_40);
    u32 askJackOut   = A_CutsceneScriptAskJackOut_80988E4;

    u32 matches = (origScript == askJackOut) ? TRUE : FALSE;

    R2 = matches;
    SET_FLAGS_NZ(matches);
    RETURN_TO_CALLER();
}

/* ── TryCutsceneSkip ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool check_screen_fade(u32 *r1_out) {
    GUEST_CALL(IsScreenFadeActive);
    *r1_out = R1;
    return ZERO_FLAG();          /* Z=1 ⇒ fade is NOT active (original takes beq) */
}

typedef struct { u32 r1; u32 nz_val; } CutsceneSkipResult;

/* Determine whether the player can skip the current cutscene.
 * Fade not running → r1 left by IsScreenFadeActive, Z=1 (nothing to skip).
 * Fade running     → r1 = SELECT, NZ from (pressed & SELECT). */
static CutsceneSkipResult cutscene_skip_test(u32 toolkit) {
    u32 r1_fade;
    if (check_screen_fade(&r1_fade)) {
        CutsceneSkipResult skip_off = { r1_fade, 0 };
        return skip_off;
    }
    u32 joypad  = LD32(toolkit + oToolkit_JoypadPtr);
    u16 pressed = LD16(joypad  + oJoypad_Pressed);
    CutsceneSkipResult skip_on = { JOYPAD_SELECT, (u32)(pressed & JOYPAD_SELECT) };
    return skip_on;
}

/* TryCutsceneSkip(r10 = toolkit) -> r1, nz, c (r3/r4-r7 pass through) */
Next TryCutsceneSkip(void) {
    CutsceneSkipResult r = cutscene_skip_test(R10);
    R1 = r.r1;
    SET_FLAGS_NZ(r.nz_val);
    RETURN_TO_CALLER();
}

/* ── SetCutsceneCameraScript ── */
/* SetCutsceneCameraScript(r0 = script pointer):
 * Points the cutscene camera at a new script and clears its command timer
 * (offsets 0 and 4 of eCutsceneCameraInfo, the struct at 0x2011BD0 that the
 * literal pool references via A_eCutsceneCameraInfo), so the script starts fresh.
 * Then calls camera_writeUnk03_14_80301b2(0) to reset the camera's pending
 * write state; the final r1/r2 values are whatever that callee leaves. */
Next SetCutsceneCameraScript(void) {
    /* ldr r1, =eCutsceneCameraInfo */
    u32 cameraInfo = A_eCutsceneCameraInfo;

    /* str r0, [r1, #oCutsceneCameraInfo_CutsceneCameraScriptPtr] */
    ST32(cameraInfo + 0, R0);
    /* str r0, [r1, #oCutsceneCameraInfo_Timer_CommandInitialized] */
    ST32(cameraInfo + 4, 0);

    /* mov r0, #0; bl camera_writeUnk03_14_80301b2 */
    R0 = 0;
    R1 = cameraInfo;
    GUEST_CALL(camera_writeUnk03_14_80301b2);

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

/* ── RunCutsceneCameraCommand ── */
Next RunCutsceneCameraCommand(void) {
    u32 info = A_eCutsceneCameraInfo;

    /* The command handler is dispatched via `bx r0` and may clobber r4-r7,
     * so the original wraps the dispatch in push/pop {r4-r7}. Save them now
     * and restore them before returning so callers see them unchanged. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 scriptPtr = LD32(info + oCutsceneCameraInfo_CutsceneCameraScriptPtr);
    u8 command = LD8(scriptPtr);

    /* Handler receives the script pointer in r1, the command byte in r2,
     * and (as in the original) the camera-info base in r5. */
    R1 = scriptPtr;
    R2 = command;
    R5 = info;

    switch (command) {
    case 0x00: GUEST_CALL(CutsceneCameraCmd_set_camera_pos); break;
    case 0x04: GUEST_CALL(CutsceneCameraCmd_simple_scroll); break;
    case 0x08: GUEST_CALL(CutsceneCameraCmd_end); break;
    case 0x0C: GUEST_CALL(CutsceneCameraCmd_wait); break;
    case 0x10: GUEST_CALL(CutsceneCameraCmd_init_shake_effect); break;
    case 0x14: GUEST_CALL(CutsceneCameraCmd_auto_scroll_to_player); break;
    case 0x18: GUEST_CALL(CutsceneCameraCmd_smooth_auto_scroll_to_pos); break;
    case 0x1C: GUEST_CALL(CutsceneCameraCmd_smooth_auto_scroll_to_pos_relative_to_ow_player); break;
    case 0x20: GUEST_CALL(CutsceneCameraCmd_smooth_auto_scroll_to_soul_weapons_cursor); break;
    case 0x24: GUEST_CALL(CutsceneCameraCmd_smooth_auto_scroll_cmd_80372ec); break;
    case 0x28: GUEST_CALL(CutsceneCameraCmd_run_text_script); break;
    case 0x2C: GUEST_CALL(CutsceneCameraCmd_wait_chatbox); break;
    case 0x30: GUEST_CALL(CutsceneCameraCmd_call_sub_8001B1C); break;
    case 0x34: GUEST_CALL(CutsceneCameraCmd_call_sub_8001B6C); break;
    case 0x38: GUEST_CALL(CutsceneCameraCmd_set_screen_fade); break;
    case 0x3C: GUEST_CALL(CutsceneCameraCmd_wait_screen_fade); break;
    case 0x40: GUEST_CALL(CutsceneCameraCmd_set_event_flag); break;
    case 0x44: GUEST_CALL(CutsceneCameraCmd_clear_event_flag); break;
    case 0x48: GUEST_CALL(CutsceneCameraCmd_sound_cmd_80373fc); break;
    case 0x4C: GUEST_CALL(CutsceneCameraCmd_play_music); break;
    case 0x50: GUEST_CALL(CutsceneCameraCmd_write_camera_field_03_14); break;
    default: break;
    }

    /* The handler advanced the script pointer in r1; store it back. */
    ST32(info + oCutsceneCameraInfo_CutsceneCameraScriptPtr, R1);

    /* pop {r4-r7}: undo any clobbering the handler did. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;

    /* tst r0, r0 : N/Z from the handler's status, C/V untouched. */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCameraCmd_simple_scroll ── */
/* register form: calls ReadCutsceneCameraScriptHalfword outside this file, which has no typed C interface yet */
static u16 read_script_halfword(u32 offset, u32 script_ptr) {
    R0 = offset;
    R1 = script_ptr;
    GUEST_CALL(ReadCutsceneCameraScriptHalfword);
    return (u16)R0;
}

/* register form: calls ReadCutsceneCameraScriptSignedHalfword outside this file, which has no typed C interface yet */
static s16 read_script_signed_halfword(u32 offset, u32 script_ptr) {
    R0 = offset;
    R1 = script_ptr;
    GUEST_CALL(ReadCutsceneCameraScriptSignedHalfword);
    return (s16)R0;
}

/* Result of one cutscene "simple scroll" command: the values it leaves in the
 * guest output registers, plus operands for a single ADDS that reproduces
 * the path's CPSR (the original's last flag-setting instruction). */
typedef struct {
    u32 r0;
    u32 r1;
    u32 r3;
    u32 r6;
    u32 add_a;
    u32 add_b;
} SimpleScrollResult;

/* Read a signed 8.8 fixed-point delta from script[script_offset], add it to
 * the u32 camera field at `field_offset`, and return the field's pre-scroll
 * value (the last call's return is the Z one the original leaves in r3). */
static u32 scroll_axis(u32 camera, u32 field_offset, u32 script_offset, u32 script_ptr) {
    s16 delta = read_script_signed_halfword(script_offset, script_ptr);
    u32 prev = LD32(camera + field_offset);
    ST32(camera + field_offset, (u32)((s32)delta << 8) + prev);
    return prev;
}

/* Cutscene camera "simple scroll" command.
 *   - First tick (CommandInitialized==0): mark live, load the tick count from
 *     script halfword at offset 1.
 *   - Every tick: read signed 8.8 fixed-point deltas from offsets 3/5/7 and add
 *     them into the camera's X/Y/Z (u32 add in-place).
 *   - Decrement the timer. When it reaches zero or below, clear the initialised
 *     flag and advance the script cursor by 9 bytes (1 cmd + 8 data); otherwise
 *     leave the cursor alone.
 *   - Always returns r0=1. */
static SimpleScrollResult cutscene_camera_simple_scroll(u32 r1_in, u32 info_ptr, u32 toolkit_ptr) {
    if (LD8(info_ptr + oCutsceneCameraInfo_CommandInitialized) == 0) {
        ST8(info_ptr + oCutsceneCameraInfo_CommandInitialized, TRUE);
        ST16(info_ptr + oCutsceneCameraInfo_Timer, read_script_halfword(1, r1_in));
    }

    u32 camera = LD32(toolkit_ptr + oToolkit_CameraPtr);
    scroll_axis(camera, oCamera_X, 3, r1_in);
    scroll_axis(camera, oCamera_Y, 5, r1_in);
    u32 z_pre = scroll_axis(camera, oCamera_Z, 7, r1_in);

    /* u16 timer stored zero-extended; the original does a 32-bit sub so the
     * subsequent signed CMP #0 sees the full 32-bit result. */
    u32 timer = LD16(info_ptr + oCutsceneCameraInfo_Timer) - 1;
    ST16(info_ptr + oCutsceneCameraInfo_Timer, (u16)timer);
    bool done = ((s32)timer <= 0);

    if (done) {
        ST8(info_ptr + oCutsceneCameraInfo_CommandInitialized, FALSE);
    }

    SimpleScrollResult r;
    r.r0 = 1;
    r.r1 = done ? (r1_in + 9) : r1_in;
    r.r3 = z_pre;
    r.r6 = camera;
    if (done) {
        /* Last flag-setter on this path: ADDS r1, r1, #9 (r1 is the input r1,
         * untouched by the preceding mov r0,#0 / strb r0,...). */
        r.add_a = r1_in;
        r.add_b = 9;
    } else {
        /* Last flag-setter is the CMP r0, #0 just before the not-taken BGT
         * (the trailing MOV r0, #1 is a plain MOV and does not change flags).
         * With timer_after > 0 that CMP leaves N=0, Z=0, C=1, V=0.
         * ADDS(2, 0xFFFFFFFF) = 0x00000001 with carry-out 1, signed overflow
         * +2 + (-1) = +1 -> no overflow: C=1, V=0, N=0, Z=0 -- the same CPSR. */
        r.add_a = 2;
        r.add_b = 0xFFFFFFFFu;
    }
    return r;
}

Next CutsceneCameraCmd_simple_scroll(void) {
    /* inputs: r1 r2 r3 r5 r10 lr
     * outputs: nz c v r0 r1 r2 r3 r6 lr
     * clobbers: -
     * preserved: r2 */
    SimpleScrollResult r = cutscene_camera_simple_scroll(R1, R5, R10);
    R0 = r.r0;
    R1 = r.r1;
    R3 = r.r3;
    R6 = r.r6;
    SET_FLAGS_ADD(r.add_a, r.add_b);
    RETURN_TO_CALLER();
}

/* ── CutsceneCameraCmd_end ── */
/* CutsceneCameraCmd_end:
 * "end" camera script command: finish the current camera command and signal
 * success by returning 0 (flags set accordingly for callers that branch on it). */
Next CutsceneCameraCmd_end(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCameraCmd_wait ── */
/* Result of one run of the cutscene camera "wait" command.
 * flag_a/flag_b are the operands the adapter feeds to SET_FLAGS_CMP so the
 * final N/Z/C/V match the original's last flag-setting instruction. */
typedef struct {
    u32 r0;
    u32 r1;
    u32 r2;
    u32 flag_a;
    u32 flag_b;
} CutsceneCameraWaitResult;

/* register form: calls ReadCutsceneCameraScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_cutscene_camera_script_halfword(u32 channel) {
    R0 = channel;
    GUEST_CALL(ReadCutsceneCameraScriptHalfword);
    return R0;
}

/* Cutscene camera "wait" command. On the first run of the command the wait
 * duration is read from the camera script into the timer; every run then
 * ticks the timer down. Once it underflows the command slot is reset and the
 * script cursor (r1) advances past the 3-byte instruction (opcode + halfword). */
static CutsceneCameraWaitResult cutscene_camera_wait(u32 script_pos, u32 camera)
{
    CutsceneCameraWaitResult out;

    if (LD8(camera + oCutsceneCameraInfo_CommandInitialized) == 0) {
        u32 wait_time;
        ST8(camera + oCutsceneCameraInfo_CommandInitialized, TRUE);
        wait_time = read_cutscene_camera_script_halfword(1);
        ST16(camera + oCutsceneCameraInfo_Timer, wait_time);
    }

    /* Tick the countdown; 0 in the timer means "fire this frame". */
    {
        u32 remaining = LD16(camera + oCutsceneCameraInfo_Timer) - 1;
        ST16(camera + oCutsceneCameraInfo_Timer, remaining);

        if ((s32)remaining >= 0) {
            /* Still waiting: the cursor stays put. The original leaves the
             * flags of `mov r0,#1` on top of `cmp r2,#0`, i.e. CMP(1, 0)
             * (N=0, Z=0, C=1, V=0). */
            out.r0 = 1;
            out.r1 = script_pos;
            out.r2 = remaining;
            out.flag_a = 1;
            out.flag_b = 0;
        } else {
            /* Countdown expired: clear the command slot and consume the
             * wait instruction. `adds r1,#3` on a script pointer never
             * carries or overflows, so its flags equal CMP(0, -new cursor)
             * (same N/Z from the new cursor, C=0, V=0). */
            ST8(camera + oCutsceneCameraInfo_CommandInitialized, 0);
            out.r0 = 1;
            out.r1 = script_pos + 3;
            out.r2 = 0;
            out.flag_a = 0;
            out.flag_b = -out.r1;
        }
    }
    return out;
}

/* CutsceneCameraCmd_wait: r1 = script cursor, r5 = camera info, r3 preserved.
 * Always returns r0 = 1 (command consumed one update). */
Next CutsceneCameraCmd_wait(void) {
    CutsceneCameraWaitResult out = cutscene_camera_wait(R1, R5);
    R0 = out.r0;
    R1 = out.r1;
    R2 = out.r2;
    SET_FLAGS_CMP(out.flag_a, out.flag_b);
    RETURN_TO_CALLER();
}

/* ── CutsceneCameraCmd_auto_scroll_to_player ── */
/* register form: calls CutsceneCamera_GetAutoScrollDeltaForCoord outside this file, which has no typed C interface yet */
static u32 auto_scroll_delta_for_coord(u32 distance) {
    R3 = distance;
    GUEST_CALL(CutsceneCamera_GetAutoScrollDeltaForCoord);
    return R3;
}

/* Everything the cutscene dispatcher observes after this command runs. */
typedef struct {
    u32 counter;          /* r1: caller's counter, +1 once the camera has arrived */
    u32 differing_coord;  /* r2: player coordinate that still differs, 0 when done */
    u32 camera_z;         /* r3: camera Z after the scroll step */
    u32 camera;           /* r6: camera object */
    u32 player;           /* r7: overworld player object */
    u32 flag_cmp_a;       /* exit flags come from CMP(flag_cmp_a, flag_cmp_b) */
    u32 flag_cmp_b;
} AutoScrollResult;

/* Advance the camera one auto-scroll step toward the overworld player on all
 * three axes, then check whether the camera now sits exactly on the player.
 * While it does not, the command stays active; once it has arrived, clear the
 * command's wait fields and bump the caller's counter. */
static AutoScrollResult run_auto_scroll_to_player(u32 counter, u32 cmd_state, u32 toolkit)
{
    static const u32 camera_offset[3] = { oCamera_X, oCamera_Y, oCamera_Z };
    static const u32 player_offset[3] = { oOWPlayerObject_X, oOWPlayerObject_Y, oOWPlayerObject_Z };

    AutoScrollResult out;
    u32 camera = LD32(toolkit + oToolkit_CameraPtr);
    u32 player = LD32(LD32(toolkit + oToolkit_GameStatePtr) + oGameState_OverworldPlayerObjectPtr);
    u32 cam_coord[3], player_coord[3];
    int axis;

    for (axis = 0; axis < 3; axis++) {
        u32 coord = LD32(camera + camera_offset[axis]);
        u32 target = LD32(player + player_offset[axis]);
        coord += auto_scroll_delta_for_coord(target - coord);
        ST32(camera + camera_offset[axis], coord);
        cam_coord[axis] = coord;
        player_coord[axis] = target;
    }

    out.camera = camera;
    out.player = player;
    out.camera_z = cam_coord[2];

    for (axis = 0; axis < 3; axis++) {
        if (cam_coord[axis] != player_coord[axis])
            break;
    }

    if (axis == 3) {
        /* Camera has arrived: end the wait and advance the script counter.
         * CMP(0, ~counter) reproduces the exact nz/c/v of the original's
         * ADDS counter,#1 (0 - ~n == n + 1, with matching carry/overflow). */
        ST8(cmd_state + 6, 0);
        ST16(cmd_state + 4, 0);
        out.counter = counter + 1;
        out.differing_coord = 0;
        out.flag_cmp_a = 0;
        out.flag_cmp_b = ~counter;
    } else {
        /* Still scrolling: the original exits right after comparing the first
         * differing axis, so its flags are that axis' camera-vs-player CMP. */
        out.counter = counter;
        out.differing_coord = player_coord[axis];
        out.flag_cmp_a = cam_coord[axis];
        out.flag_cmp_b = player_coord[axis];
    }
    return out;
}

/* CutsceneCameraCmd_auto_scroll_to_player(r1 = counter, r5 = command state, r10 = toolkit)
 * Always answers r0 = 1; r1/r2/r3/r6/r7 carry the dispatcher's live state
 * onward, and the flags are whatever the final compare left behind. */
Next CutsceneCameraCmd_auto_scroll_to_player(void) {
    AutoScrollResult out = run_auto_scroll_to_player(R1, R5, R10);
    R0 = 1, R1 = out.counter, R2 = out.differing_coord, R3 = out.camera_z, R6 = out.camera, R7 = out.player;
    SET_FLAGS_CMP(out.flag_cmp_a, out.flag_cmp_b);
    RETURN_TO_CALLER();
}

/* ── CutsceneCamera_GetAutoScrollDeltaForCoord ── */
/* CutsceneCamera_GetAutoScrollDeltaForCoord(r3 = signed coordinate delta):
 * Computes the per-frame auto-scroll step for one axis of the camera.
 * The magnitude is divided by 16, clamped to at most 0x00100000, and if the
 * result falls below 0x800 the original (unshifted) magnitude is used instead
 * so small deltas still move by their full amount. The sign is preserved.
 * Outputs: r3 = clamped delta, r4 = 0x800 (the minimum step threshold).
 * Clobbers r0 and the flags; neither is observed by callers. */
Next CutsceneCamera_GetAutoScrollDeltaForCoord(void) {
    const u32 MAX_STEP = 0x00100000;    /* 0x10 << 16: 1.0 in 12.20 fixed point */
    const u32 MIN_STEP = MAX_STEP >> 9; /* 0x800 */

    u32 delta = R3;
    u32 negative = (s32)delta < 0;
    u32 magnitude = negative ? 0 - delta : delta;

    u32 step = magnitude >> 4;
    if ((s32)step > (s32)MAX_STEP)
        step = MAX_STEP;
    if ((s32)step < (s32)MIN_STEP)
        step = magnitude; /* below the floor: fall back to the raw magnitude */

    R3 = negative ? 0 - step : step;
    R4 = MIN_STEP;
    RETURN_TO_CALLER();
}

/* ── ReadCutsceneCameraScriptHalfword ── */
/* ReadCutsceneCameraScriptHalfword(r0 = script offset, r1 = script base):
 * reads a little-endian halfword from the cutscene camera script at
 * [r1 + r0] and returns it in r0. r1-r3 are preserved. */
Next ReadCutsceneCameraScriptHalfword(void) {
    u32 pos = R1 + R0;

    /* Read byte-by-byte: the address is generally unaligned, so a
     * 32-bit load would rotate on real hardware and read wrong bytes. */
    R0 = (LD8(pos + 1) << 8) | LD8(pos);

    RETURN_TO_CALLER();
}

/* ── ReadCutsceneCameraScriptSignedHalfword ── */
/* ReadCutsceneCameraScriptSignedHalfword(r0 = camera script pointer, r1 = offset
 * into the script): fetch the signed 16-bit value at script+offset and return it
 * sign-extended in r0.
 *
 * The offset can be odd, so the value is assembled from two byte reads exactly
 * as the original does (a single aligned halfword load would read the wrong
 * bytes); r1-r3 are preserved. */
Next ReadCutsceneCameraScriptSignedHalfword(void) {
    u32 addr = R1 + R0;
    u32 value = LD8(addr) | (LD8(addr + 1) << 8);
    R0 = (u32)(s32)(s16)value;
    RETURN_TO_CALLER();
}

/* ── ReadCutsceneCameraScriptWord ── */
/* Read a 32-bit word from a cutscene camera script stream.
 * r0 = offset into the script, r1 = script base pointer.
 * The word is stored little-endian (byte at addr+0 is least significant).
 * Returns the word in r0; r1, r2 and r3 are left untouched (the original
 * saves/restores r3 around its temporary use). */
static u32 read_script_word_ReadCutsceneCameraScriptWord(u32 offset, u32 base) {
    u32 addr = base + offset;

    return (u32)LD8(addr + 0)
         | ((u32)LD8(addr + 1) <<  8)
         | ((u32)LD8(addr + 2) << 16)
         | ((u32)LD8(addr + 3) << 24);
}

/* ReadCutsceneCameraScriptWord(r0 = offset, r1 = base) -> r0 = word */
Next ReadCutsceneCameraScriptWord(void) {
    R0 = read_script_word_ReadCutsceneCameraScriptWord(R0, R1);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_end_for_map_reload_maybe_8037c64 ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_1741_is_set(void) {
    R0 = 0x17;  /* movflag EVENT_1741: low byte */
    R1 = 0x41;  /* movflag EVENT_1741: high byte */
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();  /* Z set when the flag is clear */
}

/* register form: calls reloadCurNaviStatBoosts_813c3ac outside this file, which has no typed C interface yet */
static void reload_navi_stat_boosts(void) {
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);
}

/* register form: calls cutscene_forceEndCurrent_8036EFE outside this file, which has no typed C interface yet */
static void cutscene_force_end_current(void) {
    GUEST_CALL(cutscene_forceEndCurrent_8036EFE);
}

/* If event flag 0x1741 is not set, reload the current Navi's stat boosts,
 * then unconditionally force-end the active cutscene. Always returns 0. */
static u32 end_cutscene_maybe_reload(void) {
    if (!event_1741_is_set())
        reload_navi_stat_boosts();
    cutscene_force_end_current();
    return 0;
}

/* CutsceneCmd_end_for_map_reload_maybe_8037c64: 080376C4-080376DC THUMB, map_script_cutscene.s:3857 */
Next CutsceneCmd_end_for_map_reload_maybe_8037c64(void) {
    R0 = end_cutscene_maybe_reload();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_pause ── */
/* register form: calls TestCutsceneFlag outside this file, which has no typed C interface yet */
static bool cutscene_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestCutsceneFlag);
    return !ZERO_FLAG();          /* Z is set while the flag is clear */
}

/* register form: calls SetCutsceneFlag outside this file, which has no typed C interface yet */
static void set_cutscene_flag_CutsceneCmd_pause(u32 flag) {
    R0 = flag;
    GUEST_CALL(SetCutsceneFlag);
}

/* register form: calls ClearCutsceneFlag outside this file, which has no typed C interface yet */
static void clear_cutscene_flag_CutsceneCmd_pause(u32 flag) {
    R0 = flag;
    GUEST_CALL(ClearCutsceneFlag);
}

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet
 * (it receives its selector in r6 - the original passes 1 for the command's operand byte and 2
 * for the immediate byte behind an 0xFF escape - and returns the byte it read in r4) */
static u32 read_map_script_byte_CutsceneCmd_pause(u32 selector) {
    R6 = selector;
    GUEST_CALL(ReadMapScriptByte);
    return R4;
}

#define PAUSE_OPERAND_IMMEDIATE 0xFF   /* operand byte meaning "length follows inline" */

/* What one tick of the pause command leaves in registers for the script interpreter. */
struct PauseCmdResult {
    u32 timer_offset;   /* r1: offset of this script's pause timer inside the cutscene state */
    u32 duration;       /* r4: pause length, carried in r4 across ticks */
    u32 command_ptr;    /* r7: script read pointer, moved past the command once it finishes */
    bool finished;      /* r0/nz: 1 when the pause expired and the script may continue */
};

/* CutsceneCmd_pause: hold this cutscene script for a number of ticks.
 * On the first tick the script's pause flag is raised and the length operand is read -
 * either an index into the cutscene state or, after an 0xFF escape byte, an inline
 * immediate - and stored as the pause timer. Every tick (the first included) counts
 * that timer down; when it runs past zero the flag is cleared and the read pointer
 * skips the 3-byte command so interpretation continues with the next one. */
static struct PauseCmdResult cutscene_cmd_pause(u32 cutscene, u32 command_ptr, u32 duration)
{
    struct PauseCmdResult result;

    /* Which of the two script slots this state runs offsets both its flag and its timer. */
    u32 which = LD8(cutscene + oCutsceneState_WhichCutsceneScript);
    u32 pause_flag = which + CUTSCENE_FLAG_SCRIPT_1_PAUSED;
    u32 timer_offset = which + oCutsceneState_CutsceneScript1PauseTimer;

    if (!cutscene_flag_set(pause_flag)) {
        /* Pause not armed yet: raise the flag and fetch the length operand. */
        set_cutscene_flag_CutsceneCmd_pause(pause_flag);

        u32 operand = read_map_script_byte_CutsceneCmd_pause(1);
        if (operand != PAUSE_OPERAND_IMMEDIATE)
            duration = LD8(cutscene + operand);   /* indexed length held in the state */
        else
            duration = read_map_script_byte_CutsceneCmd_pause(2);   /* escaped: the length byte follows */

        ST8(cutscene + timer_offset, duration);
    }

    /* Every tick, including the one that armed it, decrements the timer. */
    u8 timer = LD8(cutscene + timer_offset);
    ST8(cutscene + timer_offset, timer - 1);

    bool expired = (timer == 0);   /* 0 - 1 underflows: the pause has run out */

    result.timer_offset = timer_offset;
    result.duration = duration;
    result.finished = expired;

    if (expired) {
        /* End the pause and resume with the command after this one. */
        clear_cutscene_flag_CutsceneCmd_pause(pause_flag);
        command_ptr += 3;          /* skip the 3-byte pause command */
    }
    result.command_ptr = command_ptr;

    return result;
}

/* CutsceneCmd_pause(r5 = cutscene state, r7 = script read pointer, r4 = carried pause
 * length) -> nz, r1, r4, r7 */
Next CutsceneCmd_pause(void) {
    struct PauseCmdResult r = cutscene_cmd_pause(R5, R7, R4);
    R1 = r.timer_offset;
    R4 = r.duration;
    R7 = r.command_ptr;
    R0 = r.finished;
    SET_FLAGS_NZ(r.finished);
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
    GUEST_CALL(chatbox_mask_eFlags2009F38); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = flagOperand & 0x38;
        GUEST_CALL(chatbox_mask_eFlags2009F38); /* answers in Z; the original branches on it */
        if (ZERO_FLAG()) {
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

/* ── CutsceneCmd_wait_if_player_sprite_cur_frame_not_equal_maybe ── */
/* Cutscene script command: hold the script until the player's overworld
 * sprite reaches a particular animation frame.
 *
 * Reads one byte from the map script (the frame to wait for), asks the
 * engine for the player sprite's current frame, and compares the two.
 * While they differ it returns 0 so the script interpreter keeps waiting;
 * once they match it skips past the command (r7 += 2) and returns 1.
 * The comparison flags are left over from the frame comparison / pointer
 * update so the interpreter can branch on them as well. */
Next CutsceneCmd_wait_if_player_sprite_cur_frame_not_equal_maybe(void) {
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    GUEST_CALL(getOWPlayerSpriteFrameParameters_809E434);

    SET_FLAGS_CMP(R0, R4);
    if (R0 == R4) {
        /* frame matched: consume the command's operand and report success */
        SET_FLAGS_ADD(R7, 2);
        R7 += 2;
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        /* still waiting on the frame */
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

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

/* ── CutsceneCmd_wait_screen_fade ── */
struct FadeWaitResult {
    u32 r0;
    u32 r7;
    u32 add_a;
    u32 add_b;
};

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static struct FadeWaitResult query_screen_fade(u32 r7_in) {
    struct FadeWaitResult res;
    GUEST_CALL(IsScreenFadeActive);

    if (!ZERO_FLAG()) {
        /* Fade still running: advance the script pointer, signal "not done" */
        res.r0 = 1;
        res.r7 = r7_in + 1;
        res.add_a = r7_in;
        res.add_b = 1;
    } else {
        /* Fade finished: leave script pointer, signal "done" */
        res.r0 = 0;
        res.r7 = r7_in;
        /* C and V from IsScreenFadeActive are preserved; encode them
         * as ADDS operands so SET_FLAGS_ADD reproduces them exactly. */
        u32 c = cpu.c;
        u32 v = cpu.v;
        if (!c && !v)       { res.add_a = 0;          res.add_b = 0; }
        else if (!c && v)   { res.add_a = 0x40000000; res.add_b = 0x40000000; }
        else if (c && !v)   { res.add_a = 0xFFFFFFFF; res.add_b = 1; }
        else                { res.add_a = 0x80000000; res.add_b = 0x80000000; }
    }
    return res;
}

/* CutsceneCmd_wait_screen_fade: blocks the script while a screen fade is active.
 * While the fade runs, the script counter (r7) advances each frame and the
 * command reports "continue" (r0 = 1).  Once the fade completes, the counter
 * stays put and the command reports "done" (r0 = 0). */
Next CutsceneCmd_wait_screen_fade(void) {
    struct FadeWaitResult res = query_screen_fade(R7);
    R0 = res.r0;
    R7 = res.r7;
    SET_FLAGS_ADD(res.add_a, res.add_b);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_wait_camera_script ── */
/* CutsceneCmd_wait_camera_script (cutscene command handler):
 * Blocks the cutscene script until the camera script that is running in
 * parallel with it has finished. While the camera script is still active,
 * returns r0 = 0 without advancing the command counter. Once the camera
 * script is done, advances the interpreter's command counter in r7 and
 * returns r0 = 1 so the cutscene continues with the next command.
 * Flags: nz comes from the r0 result; on the "done" path c/v come from
 * the counter increment (as the original's ADDS leaves them). */
Next CutsceneCmd_wait_camera_script(void) {
    u8 cameraScriptActive = LD8(R5 + oCutsceneState_CutsceneCameraScriptActive);

    if (cameraScriptActive != 0) {
        /* Camera script still running: report "not done" yet. */
        R0 = 0;
        SET_FLAGS_NZ(R0);
    } else {
        /* Camera script finished: consume this wait command. */
        SET_FLAGS_ADD(R7, 1);
        R7 = R7 + 1;
        R0 = 1;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_wait_var_equal ── */
/* CutsceneCmd_wait_var_equal: cutscene script command "wait until variable == value".
 *
 * Runs two reads through ReadMapScriptByte (r6 selects the read mode). The
 * first call fetches the byte to compare against; the script stream is then
 * peeked at [r5 + r4] and that byte handed to the second call, which leaves
 * the comparison target in r4.
 *
 * If the returned byte matches r4, the whole 3-byte command is consumed
 * (the script pointer r7 skips past it) and r0 = 1 reports the wait as done.
 * Otherwise r0 = 0 so the command is retried next update, with the script
 * pointer left on it.
 *
 * Flags: all three flag-setting instructions here are Thumb flag-setting
 * forms (cmp r0, r4 / adds r7, #3 / movs r0, #imm), so each one updates the
 * flags as it goes. The final flags are:
 *   equal path    - C/V from the r7 += 3 addition, N/Z from movs r0, #1
 *   not-equal path- C/V from the compare, N/Z from movs r0, #0
 */
Next CutsceneCmd_wait_var_equal(void) {
    /* First read: fetch the byte to compare against. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);

    /* Peek at the next script byte and feed it back as the read position. */
    R0 = LD8(R5 + R4);

    /* Second read: consume the byte and leave the comparison target in r4. */
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);

    /* movs' N/Z results overwrite the compare's on both paths; the compare's
     * C/V survive only on the not-taken path. */
    SET_FLAGS_CMP(R0, R4);
    if (R0 == R4) {
        /* Match: consume the whole command and report done. */
        R7 += 3;
        SET_FLAGS_ADD(R7, 3); /* sets N/Z/C/V, but the movs below resets N/Z */
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        /* Still waiting: keep the script pointer on this command. */
        R0 = 0;
        SET_FLAGS_NZ(R0); /* C/V stay as the compare left them */
    }
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_wait_cutscene_process ── */
/* CutsceneCmd_wait_cutscene_process (0x08037826):
 * Poll the map script to see whether it has reached its end.
 *
 * Reads one byte from the current script through ReadMapScriptByte (called
 * with r6 = 1; it also advances the script read position in r4). The byte
 * fetched is the opcode at the script's current data pointer ([r5 + r4] -> byte).
 *
 * If it is the end-of-script opcode (0x11), the cutscene command pointer in
 * r7 is advanced past this 2-byte command and r0 = 1 ("script finished");
 * otherwise r0 = 0. Flags end up reflecting r0 (with C/V as left by the
 * last flag-setting instruction on each path, as in the original). */
Next CutsceneCmd_wait_cutscene_process(void) {
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);

    u32 opcode = LD8(LD32(R5 + R4));
    SET_FLAGS_CMP(opcode, 0x11);

    if (opcode == 0x11) {
        /* End of script: skip the rest of this cutscene command. */
        SET_FLAGS_ADD(R7, 2);
        R7 += 2;
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        /* Script still running: report "not finished". */
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_cutscene_end ── */
/* CutsceneCmd_cutscene_end: script command that terminates the running
 * cutscene. Always "fails" (R0 = 0) so the script engine stops executing
 * further commands; flags are set from that zero result as callers expect. */
Next CutsceneCmd_cutscene_end(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_spawn_cutscene_process ── */
/* Cutscene flag ids, one per script slot: slots 0-3 are the "script paused"
 * flags, slots 4-7 the "script paused long" ones (hence the +0/+4 pairing). */
#define CUTSCENE_FLAG_SCRIPT_1_PAUSED      0u
#define CUTSCENE_FLAG_SCRIPT_1_PAUSED_LONG 4u

/* CutsceneCmd_spawn_cutscene_process (080378C2)
 * Map script command "spawn cutscene process": the command word gives the
 * offset of one of the cutscene state's script-position fields, and the
 * following byte is the value to write into it. Writing the field (re)starts
 * that cutscene script slot, so both pause flags belonging to that slot are
 * cleared. Afterwards the script pointer is advanced past the 6-byte command
 * and r0 = 1 reports that the script should keep running. */
Next CutsceneCmd_spawn_cutscene_process(void) {
    /* Fetch the word parameter: offset of the target field within the cutscene state. */
    R6 = 2;
    GUEST_CALL(ReadMapScriptWord);

    /* Fetch the byte parameter and store it into cutsceneState->field[offset]. */
    R0 = R4;
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    ST32(R5 + R4, R0);

    /* Turn the field offset into an index over the script-position slots. */
    R4 = (R4 - oCutsceneState_CutsceneScriptPos) >> 2;

    /* Clear both pause flags for this slot so the spawned process can run.
     * leftover unused: ReadMapScriptByte's c, ClearCutsceneFlag's c. The
     * original enters both calls with c = 0 (from its add). ClearCutsceneFlag
     * passes c on only when its shift is 0 (slot 0's first flag), and the
     * second call's shift (4-7) sets it, so the c passed here reaches nothing. */
    R0 = R4 + CUTSCENE_FLAG_SCRIPT_1_PAUSED;
    GUEST_CALL(ClearCutsceneFlag);
    R0 = R4 + CUTSCENE_FLAG_SCRIPT_1_PAUSED_LONG;
    GUEST_CALL(ClearCutsceneFlag);

    /* Advance past the 6-byte command (C/V come from this add). */
    SET_FLAGS_ADD(R7, 6);
    R7 = R7 + 6;

    /* r0 = 1 means "continue the script" (the MOV sets N and Z). */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_kill_cutscene_process ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r7; } call_ReadMapScriptByte_result_CutsceneCmd_kill_cutscene_process;
/* register form: generated wrapper: ReadMapScriptByte has no typed C interface yet */
static inline call_ReadMapScriptByte_result_CutsceneCmd_kill_cutscene_process call_ReadMapScriptByte_CutsceneCmd_kill_cutscene_process(u32 r6, u32 r7) {
    R6 = r6;
    R7 = r7;
    GUEST_CALL(ReadMapScriptByte);
    return (call_ReadMapScriptByte_result_CutsceneCmd_kill_cutscene_process){ R4, R7 };
}

/* Map-script cutscene command "kill cutscene process": reads one operand byte
 * from the cutscene script (the index of the cutscene process slot to kill),
 * then points that slot at DummyCutsceneScript — a script that ends at once,
 * so the process stops running. Afterwards the script position moves past the
 * operand byte. */
typedef struct {
    u32 slot_index;  /* process slot whose script was replaced      -> r4 */
    u32 script_pos;  /* script position after the operand byte      -> r7 */
    u32 result;      /* constant 1 the original leaves in r0; the NZ flags describe it */
} KillCutsceneResult;

static KillCutsceneResult kill_cutscene_process(u32 process_table, u32 script_pos) {
    KillCutsceneResult res;

    /* Fetch the operand byte: ReadMapScriptByte reports the byte in r4 and
     * the position after it in r7 (it leaves r0/r5/r7 alone for this call;
     * the 1 is the fixed selector the original always passes in r6). */
    call_ReadMapScriptByte_result_CutsceneCmd_kill_cutscene_process operand =
        call_ReadMapScriptByte_CutsceneCmd_kill_cutscene_process(1, script_pos);

    /* Point the selected process slot at the dummy (immediately-ending) script. */
    ST32(process_table + operand.r4, A_DummyCutsceneScript);

    res.slot_index = operand.r4;
    res.script_pos = operand.r7 + 2;
    res.result = 1;
    return res;
}

/* CutsceneCmd_kill_cutscene_process(r5 = process table, r7 = script position)
 * -> nz, r4 = slot index, r7 = advanced script position */
Next CutsceneCmd_kill_cutscene_process(void) {
    KillCutsceneResult res = kill_cutscene_process(R5, R7);
    R4 = res.slot_index;
    R7 = res.script_pos;
    SET_FLAGS_NZ(res.result);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_set_cutscene_skip_script ── */
/* CutsceneCmd_set_cutscene_skip_script: cutscene VM command that arms the
 * "skip cutscene" behavior. It reads the 32-bit operand following the command
 * from the script (ReadMapScriptWord, which fetches via r5/r7 and leaves the
 * word in r4), stores it as the script to run once the cutscene is skipped,
 * then advances the script cursor past the command (opcode + 4-byte operand
 * = 5 bytes) and reports success.
 *
 * inputs:  r0 = command id, r5 = cutscene state, r7 = script cursor
 * outputs: r0 = 1, r4 = operand word, r6 = 1, r7 = cursor + 5, nz/c/v,
 *          lr unchanged */
Next CutsceneCmd_set_cutscene_skip_script(void) {
    u32 cursor;

    R6 = 1;
    GUEST_CALL(ReadMapScriptWord);

    ST32(R5 + oCutsceneState_CutsceneScriptAfterCutsceneSkip, R4);

    cursor = R7 + 5;
    SET_FLAGS_ADD(R7, 5);
    R7 = cursor;

    R0 = 1;
    SET_FLAGS_NZ(R0);

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

/* ── MapScriptCutsceneCmd_jump_if_current_navi_not_equal ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r7; } call_ReadMapScriptByte_result_MapScriptCutsceneCmd_jump_if_current_navi_not_equal;
/* register form: generated wrapper: ReadMapScriptByte has no typed C interface yet */
static inline call_ReadMapScriptByte_result_MapScriptCutsceneCmd_jump_if_current_navi_not_equal call_ReadMapScriptByte_MapScriptCutsceneCmd_jump_if_current_navi_not_equal(u32 r6, u32 r7) {
    R6 = r6;
    R7 = r7;
    GUEST_CALL(ReadMapScriptByte);
    return (call_ReadMapScriptByte_result_MapScriptCutsceneCmd_jump_if_current_navi_not_equal){ R4, R7 };
}

typedef struct { u32 r0, r3; } call_GetCurPETNavi_result_MapScriptCutsceneCmd_jump_if_current_navi_not_equal;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result_MapScriptCutsceneCmd_jump_if_current_navi_not_equal call_GetCurPETNavi_MapScriptCutsceneCmd_jump_if_current_navi_not_equal(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result_MapScriptCutsceneCmd_jump_if_current_navi_not_equal){ R0, R3 };
}

typedef struct { u32 r4, r6; } call_ReadMapScriptWord_result;
/* register form: generated wrapper: ReadMapScriptWord has no typed C interface yet */
static inline call_ReadMapScriptWord_result call_ReadMapScriptWord(u32 r6, u32 r7) {
    R6 = r6;
    R7 = r7;
    GUEST_CALL(ReadMapScriptWord);
    return (call_ReadMapScriptWord_result){ R4, R6 };
}

/* Cutscene script command "jump_if_current_navi_not_equal".
 *
 * The script cursor (r7) points at two operands: a navi id byte followed by a
 * 4-byte jump target. If the id differs from the navi currently installed in
 * the PET, execution jumps to the target (the cursor becomes the target
 * address); otherwise the cursor skips past both operands and play continues.
 * Returns the new script cursor position; both paths leave Z/N from the
 * constant 1 the original loads at the end. */
static u32 jump_if_current_navi_not_equal(u32 pos) {
    /* Read the navi id operand (1-byte script read); the callee also reports
     * the cursor position it leaves behind. */
    call_ReadMapScriptByte_result_MapScriptCutsceneCmd_jump_if_current_navi_not_equal id = call_ReadMapScriptByte_MapScriptCutsceneCmd_jump_if_current_navi_not_equal(1, pos);
    u32 wantedNavi = id.r4;
    u32 afterId    = id.r7;

    /* Which navi is installed in the PET right now? */
    u32 currentNavi = call_GetCurPETNavi_MapScriptCutsceneCmd_jump_if_current_navi_not_equal().r0;

    if (currentNavi != wantedNavi) {
        /* Different navi: take the branch, i.e. continue at the word target. */
        call_ReadMapScriptWord_result target = call_ReadMapScriptWord(2, afterId);
        return target.r4;
    }

    /* Same navi: fall through past the id byte and the jump target. */
    return afterId + 6;
}

/* MapScriptCutsceneCmd_jump_if_current_navi_not_equal
 * inputs: r7 = script cursor, r10 (for GetCurPETNavi), lr
 * outputs: r7 = new script cursor, nz (set from the constant 1) */
Next MapScriptCutsceneCmd_jump_if_current_navi_not_equal(void) {
    R7 = jump_if_current_navi_not_equal(R7);
    SET_FLAGS_NZ(1);
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

/* ── CutsceneCmd_run_text_script ── */
/* CutsceneCmd_run_text_script: "run text script" command of the map cutscene
 * interpreter (r5 = cutscene state, r7 = cursor into the command stream,
 * r10 = map script base, used by ReadMapScriptByte).
 *
 * Command encoding in the stream:
 *   <opcode> <slot>         -> script index is byte <slot> of the cutscene state
 *   <opcode> 0xFF <idx:16>  -> script index is given inline as two bytes
 *
 * Runs the indexed script from the state's text archive in the chatbox,
 * advances the cursor past the command and returns r0 = 1 (keep running the
 * cutscene). */
Next CutsceneCmd_run_text_script(void) {
    /* Read the first operand byte of the command. */
    R6 = 1;                             /* ask ReadMapScriptByte for one byte */
    GUEST_CALL(ReadMapScriptByte);
    u32 operand = R4;

    u32 bytesConsumed;
    if (operand == 0xFF) {
        /* 0xFF marker: the script index follows inline as two bytes. */
        R6 = 2;
        GUEST_CALL(ReadMapScriptByte);
        R1 = R4;
        R0 = LD32(R5 + oCutsceneState_TextArchivePtr);
        GUEST_CALL(chatbox_runScript);
        bytesConsumed = 3;              /* opcode + marker + 2 index bytes */
    } else {
        /* The operand is a slot in the cutscene state whose byte value is
         * the actual script index. */
        R1 = LD8(R5 + operand);
        R0 = LD32(R5 + oCutsceneState_TextArchivePtr);
        GUEST_CALL(chatbox_runScript);
        bytesConsumed = 2;              /* opcode + slot byte */
    }

    /* Advance the command cursor past this command. The ADDS updates the
     * flags; the following MOVS r0,#1 then overwrites only n/z. */
    u32 oldCursor = R7;
    R7 = oldCursor + bytesConsumed;
    SET_FLAGS_ADD(oldCursor, bytesConsumed);

    R0 = 1;                             /* tell the interpreter to continue */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_chatbox_cmd_8037a70 ── */
/* CutsceneCmd_chatbox_cmd_8037a70: chatbox cutscene command.
 * Opens/shows the chatbox, advances the cutscene script cursor (r7) by one
 * entry, and returns 1 to signal that the command was handled. */
Next CutsceneCmd_chatbox_cmd_8037a70(void) {
    GUEST_CALL(chatbox_8040818);

    R7 = R7 + 1;
    SET_FLAGS_ADD(R7, 1); /* c/v come from the pointer increment */

    R0 = 1;
    SET_FLAGS_NZ(R0); /* nz come from the final MOV r0, #1 */

    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_set_or_clear_chatbox_flags ── */
/*
 * Cutscene command "chatbox flag" (map script opcode):
 *   - reads a 2-byte value from the script: the chatbox flag id,
 *   - reads a 1-byte value from the script: the set/clear selector
 *     (1 clears the flag, anything else sets it),
 *   - advances the script cursor (r7) by 3 bytes and answers 1.
 * ReadMapScriptByte takes the value width in r6 and returns the value in r4,
 * preserving r0-r2 and r7; the id stays in r0 for the chatbox call.
 */
struct ChatboxFlagCmdResult {
    u32 mode;        /* second byte read: set/clear selector (r4 afterwards) */
    u32 script_ptr;  /* script cursor after the command (r7 afterwards) */
};

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static void read_chatbox_flag_args(u32 *flag_id, u32 *mode) {
    R6 = 2;                    /* first read: 2-byte value (the flag id) */
    GUEST_CALL(ReadMapScriptByte);
    R0 = R4;                   /* keep the flag id in r0 across the second read */
    R6 = 1;                    /* second read: 1-byte value (the selector) */
    GUEST_CALL(ReadMapScriptByte);
    *flag_id = R0;             /* ReadMapScriptByte leaves r0 untouched */
    *mode = R4;
}

/* register form: calls chatbox_set_eFlags2009F38 outside this file, which has no typed C interface yet */
static void set_chatbox_flag(u32 flag_id) {
    R0 = flag_id;
    GUEST_CALL(chatbox_set_eFlags2009F38);
}

/* register form: calls chatbox_clear_eFlags2009F38 outside this file, which has no typed C interface yet */
static void clear_chatbox_flag(u32 flag_id) {
    R0 = flag_id;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
}

/* Apply one chatbox-flag script command starting at script_ptr. */
static void run_chatbox_flag_command(u32 script_ptr, struct ChatboxFlagCmdResult *out) {
    u32 flag_id, mode;

    read_chatbox_flag_args(&flag_id, &mode);
    if (mode == 1)
        clear_chatbox_flag(flag_id);
    else
        set_chatbox_flag(flag_id);

    out->mode = mode;
    out->script_ptr = script_ptr + 3;
}

/* inputs: r1 r2 r7 lr -> outputs: nz c v r0 r1 r2 r4 r6 r7 lr */
Next CutsceneCmd_set_or_clear_chatbox_flags(void) {
    struct ChatboxFlagCmdResult res;
    run_chatbox_flag_command(R7, &res);
    SET_FLAGS_ADD(R7, 3);      /* add r7, #3 supplies c/v ... */
    R7 = res.script_ptr;
    R4 = res.mode;
    R0 = R6 = 1;               /* ... mov r0, #1 supplies n/z; r6 stays 1 from the 1-byte read */
    SET_FLAGS_NZ(1);
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

/* ── CutsceneCmd_decomp_text_archive ── */
/* CutsceneCmd_decomp_text_archive (08037AB6):
 * Cutscene script command "load text archive". Reads a 32-bit value from the
 * script stream; a negative value means the archive is compressed, in which
 * case it is decompressed first. The resulting TextScriptArchive pointer is
 * stored into the cutscene state, the script cursor is advanced past the
 * 4-byte operand (+1 for the command byte), and the command reports "1".
 * Flags on return: n/z from the final `mov r0, #1`, c/v from the cursor add. */
Next CutsceneCmd_decomp_text_archive(void) {
    R6 = 1;
    GUEST_CALL(ReadMapScriptWord);      /* fetched script word ends up in R4 */
    u32 word = R4;

    if ((s32)word < 0) {                /* negative: compressed text archive */
        R0 = word;
        GUEST_CALL(DecompressTextArchiveForCutscene);   /* returns pointer in R0 */
        word = R0;
    }

    ST32(R5 + oCutsceneState_TextArchivePtr, word);

    u32 oldCursor = R7;
    R7 = oldCursor + 5;
    SET_FLAGS_ADD(oldCursor, 5);

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── DecompressTextArchiveForCutscene ── */
/* DecompressTextArchiveForCutscene: 08037AD0-08037AEC THUMB, bn6f map_script_cutscene.s:4516 */
/*   inputs:  r0 r2 r3 lr
 *   outputs: c v r0 r1 r2 r3 lr
 *   clobbers (unused by callers): nz
 *   preserved: - */
/* (CompText *archive) -> TextScriptArchive*
 *
 * Text archives referenced from cutscene scripts carry a "compressed" tag in
 * the top bit of their pointer. If the tag is clear the pointer is used as-is;
 * if it is set, the tagged bit is stripped and the LZ77-compressed data at
 * that address is expanded into the shared decompression buffer, returning a
 * pointer just past its 4-byte header (the script data proper).
 */
Next DecompressTextArchiveForCutscene(void) {
    s32 archive = (s32)R0;

    SET_FLAGS_CMP(archive, 0);
    if (archive >= 0) {
        /* already an uncompressed pointer: hand it back untouched */
        R0 = (u32)archive;
        RETURN_TO_CALLER();
    }

    /* strip the "compressed" tag bit (lsl #1 / lsr #1 clears bit 31) */
    u32 src = ((u32)archive << 1) >> 1;

    R0 = src;
    R1 = A_eDecompressionBuf2034A00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* skip the LZ77 header; the ADD (Thumb format 3) sets the outgoing flags */
    R0 = A_eDecompressionBuf2034A00 + 4;
    SET_FLAGS_ADD(A_eDecompressionBuf2034A00, 4);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_ow_player_sprite_special_with_arg ── */
/*
 * CutsceneCmd_ow_player_sprite_special_with_arg (0x08037B6C)
 *
 * "Set overworld player sprite property" map-script command, 4 bytes long:
 *
 *   +1  handler offset: a BYTE offset into the handler table at
 *       off_8037B9C. The original loads it unscaled (`ldr r3, [r3,r4]`),
 *       so scripts store 0, 4, 8, ... here, not a plain index.
 *   +2  value code: 0xFF means a literal value follows in byte +3; any
 *       other byte is an index into the script variable table at r5
 *   +3  literal value (only read when +2 was 0xFF)
 *
 * The selected handler runs with the value in r0 and r4, its own table
 * entry in r3 and the script pointer in r7 (which it may change). After
 * the handler returns, the whole 4-byte command is skipped (r7 += 4) and
 * the command reports success (r0 = 1).
 */

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_map_script_byte_CutsceneCmd_ow_player_sprite_special_with_arg(u32 r3Value, u32 whichByte) {
    R3 = r3Value;    /* r3 travels with the call: the caller's value for the
                        first read, the handler table entry for the later ones */
    R6 = whichByte;  /* 1 = selector, 2 = value code, 3 = literal value */
    GUEST_CALL(ReadMapScriptByte);
    return R4;       /* ReadMapScriptByte answers in r4 (r0/r3/r5/r7 preserved) */
}

/*
 * The six property handlers are reached through the command's function
 * table, so each one is called in its register form. Their own results are
 * unused: the command overwrites r0 with 1 afterwards. Each helper returns
 * r7 as the handler left it, since the original advances that value.
 */

/* register form: calls WriteOWPlayerLayerIndexOverride outside this file, which has no typed C interface yet */
static u32 call_WriteOWPlayerLayerIndexOverride(u32 tableEntry, u32 value) {
    R0 = value;
    R3 = tableEntry;          /* the original enters the handler via `bx r3` */
    R4 = value;               /* the operand also sits in r4 */
    GUEST_CALL(WriteOWPlayerLayerIndexOverride);
    return R7;
}

/* register form: calls SetOWPlayerFacingDirection outside this file, which has no typed C interface yet */
static u32 call_SetOWPlayerFacingDirection(u32 tableEntry, u32 value) {
    R0 = value;
    R3 = tableEntry;
    R4 = value;
    GUEST_CALL(SetOWPlayerFacingDirection);
    return R7;
}

/* register form: calls owPlayer_setPalette_809e2a0 outside this file, which has no typed C interface yet */
static u32 call_owPlayer_setPalette_809e2a0(u32 tableEntry, u32 value) {
    R0 = value;
    R3 = tableEntry;
    R4 = value;
    GUEST_CALL(owPlayer_setPalette_809e2a0);
    return R7;
}

/* register form: calls owPlayer_setAlpha_8002c7a_809e4a0 outside this file, which has no typed C interface yet */
static u32 call_owPlayer_setAlpha_8002c7a_809e4a0(u32 tableEntry, u32 value) {
    R0 = value;
    R3 = tableEntry;
    R4 = value;
    GUEST_CALL(owPlayer_setAlpha_8002c7a_809e4a0);
    return R7;
}

/* register form: calls SetOWPlayerNaviPaletteIndex outside this file, which has no typed C interface yet */
static u32 call_SetOWPlayerNaviPaletteIndex(u32 tableEntry, u32 value) {
    R0 = value;
    R3 = tableEntry;
    R4 = value;
    GUEST_CALL(SetOWPlayerNaviPaletteIndex);
    return R7;
}

/* register form: calls owPlayer_setS2000AA0Param0x4_809e314 outside this file, which has no typed C interface yet */
static u32 call_owPlayer_setS2000AA0Param0x4_809e314(u32 tableEntry, u32 value) {
    R0 = value;
    R3 = tableEntry;
    R4 = value;
    GUEST_CALL(owPlayer_setS2000AA0Param0x4_809e314);
    return R7;
}

/*
 * Run the handler selected by the table lookup. The original dispatches on
 * the loaded function address itself (`mov lr, pc; bx r3`), so match the
 * loaded word against the six table entries rather than dispatching on the
 * selector byte. Returns r7 as the handler left it.
 */
static u32 run_sprite_special_handler(u32 handler, u32 value, u32 scriptPtr) {
    if (handler == LD32(A_off_8037B9C + 0x00))
        return call_WriteOWPlayerLayerIndexOverride(handler, value);
    if (handler == LD32(A_off_8037B9C + 0x04))
        return call_SetOWPlayerFacingDirection(handler, value);
    if (handler == LD32(A_off_8037B9C + 0x08))
        return call_owPlayer_setPalette_809e2a0(handler, value);
    if (handler == LD32(A_off_8037B9C + 0x0C))
        return call_owPlayer_setAlpha_8002c7a_809e4a0(handler, value);
    if (handler == LD32(A_off_8037B9C + 0x10))
        return call_SetOWPlayerNaviPaletteIndex(handler, value);
    if (handler == LD32(A_off_8037B9C + 0x14))
        return call_owPlayer_setS2000AA0Param0x4_809e314(handler, value);

    /* Any other selector byte loads a misaligned table word and the
     * original would jump through it; that cannot happen in a
     * well-formed script, so just leave the script pointer alone. */
    return scriptPtr;
}

/* Read the command operands and run the selected property handler.
 * Returns the script pointer as the handler left it (still to be advanced by 4). */
static u32 ow_player_sprite_special_command(u32 incomingR3, u32 varTable, u32 scriptPtr) {
    u32 selector = read_map_script_byte_CutsceneCmd_ow_player_sprite_special_with_arg(incomingR3, 1);
    u32 handler  = LD32(A_off_8037B9C + selector);  /* byte offset, loaded unscaled */
    u32 code     = read_map_script_byte_CutsceneCmd_ow_player_sprite_special_with_arg(handler, 2);
    u32 value;

    if (code == 0xFF)
        value = read_map_script_byte_CutsceneCmd_ow_player_sprite_special_with_arg(handler, 3);  /* literal value form */
    else
        value = LD8(varTable + code);              /* script variable reference */

    return run_sprite_special_handler(handler, value, scriptPtr);
}

Next CutsceneCmd_ow_player_sprite_special_with_arg(void) {
    u32 scriptPtr = ow_player_sprite_special_command(R3, R5, R7);
    R7 = scriptPtr + 4;            /* skip the 4-byte command */
    R0 = 1;                        /* the command always reports success */
    SET_FLAGS_ADD(scriptPtr, 4);   /* C/V from the original's `adds r7, #4` */
    SET_FLAGS_NZ(R0);              /* N/Z from the original's final `movs r0, #1` */
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_move_player_in_facing_direction ── */
/* Facing-direction step table in ROM (byte_8037CB4): signed (dx, dy) pairs indexed
 * by the overworld player's facing direction -- 0x10 = full step, 0x08 = diagonal
 * half step, negative = left/up.  Read straight from guest ROM like the original. */

/* Opcode, duration reference, optional immediate duration byte, s16 speed. */
#define MOVE_PLAYER_COMMAND_LENGTH 5

/* What the command leaves in the cutscene interpreter's registers. */
typedef struct {
    u32 status;    /* r0: 1 = command finished, 0 = still waiting */
    u32 r4;        /* r4: step-table address once the move has started, else the value carried in */
    u32 scriptPos; /* r7: cutscene-script cursor, advanced past the command when it finishes */
} MovePlayerResult;

/* register form: calls GetOWPlayerFacingDirection outside this file, which has no typed C interface yet */
static u32 ow_player_facing_direction(void) {
    GUEST_CALL(GetOWPlayerFacingDirection);
    return R0;
}

/* register form: calls TestCutsceneFlag outside this file, which has no typed C interface yet */
static bool cutscene_flag_is_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestCutsceneFlag);
    return !ZERO_FLAG(); /* answers in Z: set means the flag is clear */
}

/* register form: calls SetCutsceneFlag outside this file, which has no typed C interface yet */
static void set_cutscene_flag_CutsceneCmd_move_player_in_facing_direction(u32 flag) {
    R0 = flag;
    GUEST_CALL(SetCutsceneFlag);
}

/* register form: calls ClearCutsceneFlag outside this file, which has no typed C interface yet */
static void clear_cutscene_flag_CutsceneCmd_move_player_in_facing_direction(u32 flag) {
    R0 = flag;
    GUEST_CALL(ClearCutsceneFlag);
}

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet
 * (cutscene state in r5, 1-based parameter number in r6, script cursor in r7; the byte comes back in r4) */
static u32 read_script_parameter_byte(u32 cutscene, u32 paramNumber, u32 scriptPos) {
    R5 = cutscene;
    R6 = paramNumber;
    R7 = scriptPos;
    GUEST_CALL(ReadMapScriptByte);
    return R4;
}

/* register form: calls ReadMapScriptSignedHalfword outside this file, which has no typed C interface yet
 * (same register convention as ReadMapScriptByte; the halfword comes back in r4) */
static u32 read_script_parameter_halfword(u32 cutscene, u32 paramNumber, u32 scriptPos) {
    R5 = cutscene;
    R6 = paramNumber;
    R7 = scriptPos;
    GUEST_CALL(ReadMapScriptSignedHalfword);
    return R4;
}

/* register form: calls owPlayer_offsetS200ace0Coords_809e1fa outside this file, which has no typed C interface yet
 * (the original always passes 0 as the third argument) */
static void ow_player_add_special_coord_offset(s32 dx, s32 dy) {
    R0 = (u32)dx;
    R1 = (u32)dy;
    R2 = 0;
    GUEST_CALL(owPlayer_offsetS200ace0Coords_809e1fa);
}

/* Cutscene script command: glide the overworld player in the direction it is
 * currently facing for a scripted number of frames (auto-walks, pushes, ...).
 *
 * First frame: latch the facing direction, raise cutscene flag 8 (glide in
 * progress) and read the duration -- parameter 1 is either an offset into the
 * cutscene state or 0xFF, meaning the duration is the immediate byte in
 * parameter 2.  Every frame, including the first: read the signed 16-bit speed
 * (parameter 3), scale it by the facing direction's (dx, dy) step and add it to
 * the player's special overworld coordinates, then count the duration down.
 * When it runs out, clear the flag, skip the whole 5-byte command in the script
 * and report the command finished.
 */
static MovePlayerResult move_player_in_facing_direction(u32 cutscene, u32 scriptPos, u32 r4In) {
    MovePlayerResult result;
    result.status = 0;       /* "waiting" unless the move completes below */
    result.r4 = r4In;        /* only overwritten once the player starts moving */
    result.scriptPos = scriptPos;

    /* Without a cutscene script this command does nothing but wait. */
    if (LD8(cutscene + oCutsceneState_WhichCutsceneScript) == 0)
        return result;

    u32 facing = ow_player_facing_direction();

    if (!cutscene_flag_is_set(CUTSCENE_FLAG_8)) {
        /* First frame of the glide: mark it active and load the duration. */
        set_cutscene_flag_CutsceneCmd_move_player_in_facing_direction(CUTSCENE_FLAG_8);
        u32 duration = read_script_parameter_byte(cutscene, 1, scriptPos);
        if (duration == 0xff)
            duration = read_script_parameter_byte(cutscene, 2, scriptPos); /* immediate value */
        else
            duration = LD8(cutscene + duration); /* cutscene-state variable */
        ST8(cutscene + oCutsceneState_OWPlayerCoordSpecialTimer, duration);
    }

    /* Parameter 3 is the per-frame speed; push the player along the facing
     * direction's step vector (the original keeps the table address in r4). */
    u32 speed = read_script_parameter_halfword(cutscene, 3, scriptPos) << 8;
    s32 stepX = (s8)LD8(A_byte_8037CB4 + facing * 2);
    s32 stepY = (s8)LD8(A_byte_8037CB4 + facing * 2 + 1);
    ow_player_add_special_coord_offset((s32)(speed * stepX), (s32)(speed * stepY));
    result.r4 = A_byte_8037CB4;

    /* One frame less to go; the command keeps waiting while the count is positive. */
    s32 framesLeft = (s32)LD8(cutscene + oCutsceneState_OWPlayerCoordSpecialTimer) - 1;
    ST8(cutscene + oCutsceneState_OWPlayerCoordSpecialTimer, framesLeft);
    if (framesLeft > 0)
        return result;

    /* Glide done: drop the flag and skip the whole command in the script. */
    clear_cutscene_flag_CutsceneCmd_move_player_in_facing_direction(CUTSCENE_FLAG_8);
    result.scriptPos = scriptPos + MOVE_PLAYER_COMMAND_LENGTH;
    result.status = 1;
    return result;
}

/* CutsceneCmd_move_player_in_facing_direction(r5 = cutscene state, r7 = script
 * cursor, r4 = value carried in) -> r0/nz, r4, r7.  r1/r2 stay whatever the last
 * guest call left behind. */
Next CutsceneCmd_move_player_in_facing_direction(void) {
    MovePlayerResult result = move_player_in_facing_direction(R5, R7, R4);
    R0 = result.status;
    R4 = result.r4;
    R7 = result.scriptPos;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_write_or_offset_ow_player_fixed_anim_select_8037dac ── */
/* CutsceneCmd_write_or_offset_ow_player_fixed_anim_select_8037dac:
 * Map-script cutscene command that sets the overworld player's fixed
 * animation select. It takes two script operands:
 *   - the animation value: an immediate byte, or (when the first script
 *     read yields the 0xFF marker) an index into the table at R5;
 *   - a flag: when nonzero, the animation value is a relative offset that
 *     gets added to the player's current facing direction.
 * Always reports success (R0 = 1) and advances the script pointer (R7)
 * past this command. */
Next CutsceneCmd_write_or_offset_ow_player_fixed_anim_select_8037dac(void) {
    /* First operand: a mode-2 script read returns 0xFF if the value is
     * stored as a raw byte right after it, otherwise it returns an index
     * into the lookup table at R5 and the value is loaded from there. */
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);
    if (R4 == 0xFF) {
        R6 = 3;
        GUEST_CALL(ReadMapScriptByte);
    } else {
        R4 = LD8(R5 + R4);
    }

    /* Second operand: read a flag; a nonzero flag turns the animation
     * value into an offset from the player's facing direction. */
    R0 = R4;
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    if (R4 != 0) {
        R4 = R0; /* GetOWPlayerFacingDirection preserves R4 */
        GUEST_CALL(GetOWPlayerFacingDirection);
        R0 = R0 + R4;
    }

    /* Apply the animation select to the overworld player. */
    GUEST_CALL(owPlayer_setS200ace0_fixedAnimationSelect_809e13c);

    /* Advance the script pointer past this command and report success. */
    SET_FLAGS_ADD(R7, 4);
    R7 = R7 + 4;
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── MapScriptSubCmd_spawn_ow_map_object ── */
/* MapScriptSubCmd_spawn_ow_map_object (0x08037E4C): map script subcommand that
 * spawns an overworld map object and records it in one of the cutscene state's
 * 16 owMapObjectPtrs slots. */

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_map_script_byte_MapScriptSubCmd_spawn_ow_map_object(u32 command, u32 byte_offset) {
    /* the script readers take the offset inside the current command in r6 and
       the command pointer in r7, and leave the value they read in r4 */
    R6 = byte_offset;
    R7 = command;
    GUEST_CALL(ReadMapScriptByte);
    return R4;
}

/* register form: calls ReadMapScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_map_script_halfword_MapScriptSubCmd_spawn_ow_map_object(u32 command, u32 byte_offset) {
    R6 = byte_offset;
    R7 = command;
    GUEST_CALL(ReadMapScriptHalfword);
    return R4;
}

/* register form: calls ReadMapScriptWord outside this file, which has no typed C interface yet */
static u32 read_map_script_word_MapScriptSubCmd_spawn_ow_map_object(u32 command, u32 byte_offset) {
    R6 = byte_offset;
    R7 = command;
    GUEST_CALL(ReadMapScriptWord);
    return R4;
}

/* register form: calls SpawnOverworldMapObject outside this file, which has no typed C interface yet */
static u32 spawn_overworld_map_object(u32 type, u32 halfword_3, u32 halfword_5, u32 halfword_7,
                                      u32 word_9, u32 slot_address, u32 command) {
    R0 = type;
    R1 = halfword_3 << 16;   /* the halfword parameters ride in the top 16 bits of r1..r3 */
    R2 = halfword_5 << 16;
    R3 = halfword_7 << 16;
    R4 = word_9;
    R6 = slot_address;       /* the cutscene-state slot the new object belongs in */
    R7 = command;
    GUEST_CALL(SpawnOverworldMapObject);
    return R5;               /* the new object's pointer comes back in r5 */
}

/* What the subcommand leaves for the script interpreter: r4 = the command's
 * word parameter, r5 = the cutscene state pointer (unchanged), r7 = the script
 * pointer past this 13-byte command.  r1/r2 are not carried here: the original
 * just leaves whatever SpawnOverworldMapObject last put in them. */
typedef struct {
    u32 word_param;
    u32 cutscene_state;
    u32 next_command;
} OwMapObjectSpawnResult;

/* Handle the "spawn overworld map object" subcommand.
 * cutscene_state (r5) is the running cutscene state, slot (r4) is the slot
 * number taken from the script (only the low 4 bits count, selecting one of
 * the 16 owMapObjectPtrs entries), command (r7) points at the current script
 * command, laid out as:
 *   +2        byte      object type
 *   +3,+5,+7  halfword  parameters handed to the spawn, packed up top
 *   +9        word      extra parameter */
static OwMapObjectSpawnResult spawn_ow_map_object_from_script(u32 cutscene_state, u32 slot, u32 command) {
    u32 slot_address = cutscene_state + oCutsceneState_owMapObjectPtrs_44 + (slot & 0xF) * 4;

    u32 type       = read_map_script_byte_MapScriptSubCmd_spawn_ow_map_object(command, 2);
    u32 halfword_3 = read_map_script_halfword_MapScriptSubCmd_spawn_ow_map_object(command, 3);
    u32 halfword_5 = read_map_script_halfword_MapScriptSubCmd_spawn_ow_map_object(command, 5);
    u32 halfword_7 = read_map_script_halfword_MapScriptSubCmd_spawn_ow_map_object(command, 7);
    u32 word_9     = read_map_script_word_MapScriptSubCmd_spawn_ow_map_object(command, 9);

    u32 object = spawn_overworld_map_object(type, halfword_3, halfword_5, halfword_7,
                                            word_9, slot_address, command);
    ST32(slot_address, object);   /* the slot now owns the spawned object */

    OwMapObjectSpawnResult result;
    result.word_param     = word_9;          /* r4: word parameter passed on to the interpreter */
    result.cutscene_state = cutscene_state;  /* r5: the original preserves it */
    result.next_command   = command + 13;    /* r7: skip the 13-byte command */
    return result;
}

/* MapScriptSubCmd_spawn_ow_map_object(r4 = slot, r5 = cutscene state, r7 = script command)
 * -> nz r1 r2 r4 r5 r7 */
Next MapScriptSubCmd_spawn_ow_map_object(void) {
    OwMapObjectSpawnResult r = spawn_ow_map_object_from_script(R5, R4, R7);
    R4 = r.word_param;
    R5 = r.cutscene_state;
    R7 = r.next_command;
    R0 = 1;            /* subcommand handled; "mov r0, #1" also sets the final nz */
    SET_FLAGS_NZ(R0);  /* r1/r2 stay exactly as SpawnOverworldMapObject left them */
    RETURN_TO_CALLER();
}

/* ── MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc ── */
/* ------------------------------------------------------------------ */
/* Register-form call helpers                                         */
/* ------------------------------------------------------------------ */

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_script_byte(u32 offset, u32 slot_addr, u32 script_cursor) {
    R5 = slot_addr;
    R6 = offset;
    R7 = script_cursor;
    GUEST_CALL(ReadMapScriptByte);
    return R4;                      /* the script readers answer in r4 */
}

/* register form: calls ReadMapScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_script_halfword_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc(u32 offset, u32 slot_addr, u32 script_cursor) {
    R5 = slot_addr;
    R6 = offset;
    R7 = script_cursor;
    GUEST_CALL(ReadMapScriptHalfword);
    return R4;
}

/* register form: calls ReadMapScriptWord outside this file, which has no typed C interface yet */
static u32 read_script_word(u32 offset, u32 slot_addr, u32 script_cursor) {
    R5 = slot_addr;
    R6 = offset;
    R7 = script_cursor;
    GUEST_CALL(ReadMapScriptWord);
    return R4;
}

/* register form: calls SpawnOverworldMapObject outside this file, which has no typed C interface yet */
static u32 spawn_overworld_map_object_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc(u32 kind, u32 x, u32 y, u32 z, u32 arg_word,
                                      u32 slot_addr, u32 script_cursor) {
    R0 = kind;
    R1 = x;
    R2 = y;
    R3 = z;
    R4 = arg_word;                  /* extra argument the spawner reads from r4 */
    R5 = slot_addr;                 /* slot pointer goes in r5 as well... */
    R6 = slot_addr;                 /* ...and in r6 */
    R7 = script_cursor;
    GUEST_CALL(SpawnOverworldMapObject);
    return R5;                      /* the spawned object's pointer comes back in r5 */
}

/* ------------------------------------------------------------------ */
/* Typed implementation                                               */
/* ------------------------------------------------------------------ */

/* Values the command leaves behind for its caller besides r0 = 1. */
typedef struct {
    u32 x, y, z;      /* exit r1..r3: the position the object was spawned at */
    u32 arg_word;     /* exit r4: the script word handed to the spawner */
    u32 slot_addr;    /* exit r6: cutscene-state slot that now holds the object */
} SpawnRelativeResult;

/* Map script sub-command 0x?? (14 bytes): spawn an overworld map object at an
 * overworld NPC's position plus fixed-point offsets, and remember the new
 * object in one of the cutscene state's ow-map-object pointer slots.
 *   byte 2      index of the NPC to spawn relative to
 *   byte 3      object kind (r0 of the spawner)
 *   halfword 4  x offset (fixed point, added shifted left by 16)
 *   halfword 6  y offset
 *   halfword 8  z offset
 *   word   0xA  extra argument passed to the spawner in r4 */
static SpawnRelativeResult spawn_map_object_relative_to_npc(
        u32 slot_sel, u32 cutscene_state, u32 script_cursor)
{
    /* low nibble of the selector picks one of 16 pointer slots in the state */
    u32 slot_addr = cutscene_state + oCutsceneState_owMapObjectPtrs_44
                  + (slot_sel & 0xF) * 4;

    u32 npc_index = read_script_byte(2, slot_addr, script_cursor);
    u32 npc = A_eOverworldNPCObjects + npc_index * oOverworldNPCObject_Size;
    u32 npc_x = LD32(npc + oOverworldNPCObject_X);
    u32 npc_y = LD32(npc + oOverworldNPCObject_Y);
    u32 npc_z = LD32(npc + oOverworldNPCObject_Z);

    u32 off_x = read_script_halfword_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc(4, slot_addr, script_cursor) << 16;
    u32 off_y = read_script_halfword_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc(6, slot_addr, script_cursor) << 16;
    u32 off_z = read_script_halfword_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc(8, slot_addr, script_cursor) << 16;

    u32 new_x = npc_x + off_x;
    u32 new_y = npc_y + off_y;
    u32 new_z = npc_z + off_z;

    u32 kind = read_script_byte(3, slot_addr, script_cursor);
    u32 arg_word = read_script_word(0xA, slot_addr, script_cursor);

    u32 spawned = spawn_overworld_map_object_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc(kind, new_x, new_y, new_z,
                                             arg_word, slot_addr, script_cursor);
    ST32(slot_addr, spawned);       /* cutscene state keeps the new object's pointer */

    SpawnRelativeResult out = { new_x, new_y, new_z, arg_word, slot_addr };
    return out;
}

/* ------------------------------------------------------------------ */
/* Thin adapter                                                       */
/* ------------------------------------------------------------------ */

/* MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc
 * (r4 = pointer-slot selector, r5 = cutscene state, r7 = script cursor)
 * -> r0 = 1, r1..r3 = spawn position, r4 = script word, r5 unchanged,
 *    r6 = pointer slot, r7 = cursor advanced by the 0xE-byte command;
 *    c/v from the cursor addition, nz from the final r0 = 1. */
Next MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_npc(void) {
    u32 saved_r5 = R5, script_cursor = R7;
    SpawnRelativeResult r = spawn_map_object_relative_to_npc(R4, R5, script_cursor);
    R0 = 1, R1 = r.x, R2 = r.y, R3 = r.z;
    R4 = r.arg_word, R5 = saved_r5, R6 = r.slot_addr, R7 = script_cursor + 0xE;
    SET_FLAGS_ADD(script_cursor, 0xE);   /* `adds r7, r7, #0xE` cursor advance */
    SET_FLAGS_NZ(1);                     /* final `mov r0, #1` */
    RETURN_TO_CALLER();
}

/* ── MapScriptSubCmd_free_ow_map_object ── */
/* MapScriptSubCmd_free_ow_map_object: map-script sub-command that releases one
 * overworld map object.
 *   r4: command word; its low nibble is the index into the cutscene state's
 *       owMapObjectPtrs array
 *   r5: cutscene state pointer (preserved)
 *   r7: script cursor, advanced by 2 (the command's operand pair)
 * Always reports success (r0 = 1). */
Next MapScriptSubCmd_free_ow_map_object(void) {
    u32 cutsceneState = R5;

    /* Turn the slot index in r4 into a byte offset within the cutscene state. */
    R1 = 0xF;
    R4 = ((R4 & R1) << 2) + oCutsceneState_owMapObjectPtrs_44;

    u32 object = LD32(cutsceneState + R4);
    if (object != 0) {
        /* Slot in use: clear the registered pointer and hand the object to
         * the deallocator. */
        R0 = 0;
        ST32(object + R4, R0);
        R5 = object;
        GUEST_CALL(FreeOverworldMapObject);
    }

    /* Restore r5 (the callee is free to clobber it), step the script cursor
     * past this command, and report success. */
    R5 = cutsceneState;
    SET_FLAGS_ADD(R7, 2);
    R7 = R7 + 2;
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

/* ── CutsceneCmd_call_native_with_return_value ── */
/* CutsceneCmd_call_native_with_return_value (0x08038028)
 *
 * Map-script cutscene command handler. It first pulls the next script word
 * (ReadMapScriptWord) and then invokes the native command routine whose
 * address the dispatcher left in R4. The native routine reports in R0:
 *   - 0  -> the command is still running, so advance the script cursor (R7)
 *           past this 5-byte command and tell the caller to continue (R0 = 1);
 *   - !=0 -> stop the script and report 0.
 */
Next CutsceneCmd_call_native_with_return_value(void) {
    R6 = 1;                          /* flag consumed by ReadMapScriptWord */
    GUEST_CALL(ReadMapScriptWord);

    GUEST_CALL_ADDR(R4);             /* mov lr, pc; bx r4 -- run native handler */

    if (ZERO_FLAG()) {               /* handler returned 0: keep going */
        R7 += 5;                     /* skip this 5-byte script command */
        R0 = 1;
    } else {                         /* handler returned non-zero: halt script */
        R0 = 0;
    }
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_warp_cmd_8038040 ── */
/*
 * Cutscene map-script command "warp" (08038040).
 *
 * Reads a parameter byte from the script, then:
 *   bit 0x40 set -> warp again to the warp entry the warp system currently
 *                   holds (no operands);
 *   bit 0x20 set -> store the following halfword as the pending warp
 *                   argument in the cutscene state;
 *   otherwise    -> warp now, with the following halfword as argument (or
 *                   the pending argument kept in the cutscene state when
 *                   bit 0x80 is set), followed by one more byte; bit 0 of
 *                   the parameter picks the flavour (subsystem setup 0x14
 *                   instead of 0x10).
 *
 * Always reports r0 = 1 ("keep running the script") and advances the script
 * cursor r7 by the command length.  n/z come from the final `mov r0,#1`,
 * c/v from the `adds r7,#len` cursor update.
 *
 * Register flow worth noting: after the first read the parameter byte lives
 * in r3 for the whole command.  ReadMapScriptByte/Word preserve r3, but the
 * warp_setSubsystemIndex* callees do NOT - they leave their own value in r3
 * (and the original passes whatever warp_getCurrentWarpEntry left in r3 on
 * to warp_setSubsystemIndex).  So r3 is only ever written by the original's
 * single `mov r3, r4` and otherwise simply flows through the calls; the same
 * is done here, and the value the last warp callee leaves in r3 is what the
 * command returns in r3.
 */

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_param_byte(void) {
    R6 = 1;                    /* operand width selector the script reader expects in r6 */
    GUEST_CALL(ReadMapScriptByte);
    R3 = R4;                   /* the original's `mov r3, r4`: parameter byte stays in r3 */
    return R4;
}

/* register form: calls ReadMapScriptWord outside this file, which has no typed C interface yet */
static u32 read_operand_word(u32 selector) {
    R0 = 0x20;                 /* r0 still holds the last tst mask when the call is made */
    R6 = selector;             /* 3: warp argument halfword, 2: pending-argument halfword */
    GUEST_CALL(ReadMapScriptWord);
    return R4;
}

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_screen_byte(u32 warp_arg) {
    /* The original keeps the warp argument in r0/r2 and zeroes r1 across this read. */
    R0 = warp_arg;
    R1 = 0;
    R2 = warp_arg;
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);
    return R4;
}

/* register form: calls warp_getCurrentWarpEntry_8005f32 outside this file, which has no typed C interface yet */
static void load_current_warp_entry(void) {
    R0 = 0x40;                 /* r0 still holds the 0x40 tst mask when the call is made */
    GUEST_CALL(warp_getCurrentWarpEntry_8005f32);
    /* Leaves the current warp entry in r0-r2; r3 flows through untouched. */
}

/* register form: calls warp_setSubsystemIndexTo0x10AndOthers_8005f00 outside this file, which has no typed C interface yet */
static void rewarp_to_subsystem_0x10(void) {
    /* Consumes the warp entry the previous call left in r0-r2 (r3 included as-is). */
    GUEST_CALL(warp_setSubsystemIndexTo0x10AndOthers_8005f00);
    /* Does not preserve r3: whatever it leaves there is the command's exit r3. */
}

/* register form: calls warp_setSubsystemIndexTo0x10AndOthers_8005f00 outside this file, which has no typed C interface yet */
static void warp_to_subsystem_0x10(u32 warp_arg, u32 screen) {
    R0 = warp_arg;
    R1 = 0;
    R2 = screen;
    GUEST_CALL(warp_setSubsystemIndexTo0x10AndOthers_8005f00);
    /* Does not preserve r3: whatever it leaves there is the command's exit r3. */
}

/* register form: calls warp_setSubsystemIndexTo0x14AndOthers_8005f14 outside this file, which has no typed C interface yet */
static void warp_to_subsystem_0x14(u32 warp_arg, u32 screen) {
    R0 = warp_arg;
    R1 = 0;
    R2 = screen;
    GUEST_CALL(warp_setSubsystemIndexTo0x14AndOthers_8005f14);
    /* Does not preserve r3: whatever it leaves there is the command's exit r3. */
}

/* Handle one "warp" cutscene command; returns how far the script cursor moves. */
static u32 cutscene_warp_cmd(u32 cutscene) {
    u32 param = read_param_byte();

    if (param & 0x40) {
        /* Re-warp to the warp entry the warp system currently holds (no operands). */
        load_current_warp_entry();
        rewarp_to_subsystem_0x10();
        return 2;
    }

    if (param & 0x20) {
        /* Remember the following halfword as the argument of a later warp. */
        u32 pending = read_operand_word(2);
        ST32(cutscene + oCutsceneState_DefaultScriptTable, pending);
        return 6;
    }

    /* Warp now: argument halfword (or the pending one for bit 0x80), then a byte. */
    u32 warp_arg = read_operand_word(3);
    if (param & 0x80)
        warp_arg = LD32(cutscene + oCutsceneState_DefaultScriptTable);

    u32 screen = read_screen_byte(warp_arg);

    if (param & 0x01)
        warp_to_subsystem_0x14(warp_arg, screen);   /* bit 0 picks the 0x14 flavour */
    else
        warp_to_subsystem_0x10(warp_arg, screen);

    return 7;
}

/* CutsceneCmd_warp_cmd_8038040(r5 = cutscene state, r7 = script cursor) -> nz c v r0-r7 */
Next CutsceneCmd_warp_cmd_8038040(void) {
    u32 advance = cutscene_warp_cmd(R5);
    R0 = 1;                          /* report "keep running the script" */
    SET_FLAGS_ADD(R7, advance);      /* c/v from the cursor update `adds r7, #len` */
    R7 = R7 + advance;
    SET_FLAGS_NZ(R0);                /* n/z as the original's final `mov r0,#1` leaves them */
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

/* ── MapScriptCutsceneCmd_play_music ── */
/* MapScriptCutsceneCmd_play_music: "play music" map script command.
 *
 * The command's halfword operand is a song id. If it is negative, the music is
 * instead chosen from the byte of the event/cutscene currently running.
 * Always advances the script pointer past the operand and returns 1 so the
 * script keeps running. */
Next MapScriptCutsceneCmd_play_music(void) {
    R6 = 1;
    GUEST_CALL(ReadMapScriptSignedHalfword); /* result comes back in R4 */

    if ((s32)R4 < 0) {
        /* Negative id: play the music tied to the current event byte. */
        GUEST_CALL(playCertainMapMusicBasedOnEventByte_8036e44);
        GUEST_CALL(PlayMapMusic);
    } else {
        /* Non-negative id: play that song directly. */
        R0 = R4;
        GUEST_CALL(PlayMusic);
    }

    /* Skip past the opcode plus the halfword operand in the script. */
    u32 scriptPtr = R7;
    R7 = scriptPtr + 3;
    SET_FLAGS_ADD(scriptPtr, 3);

    R0 = 1; /* tell the script interpreter to continue */
    SET_FLAGS_NZ(R0);
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

/* ── MapScriptCutsceneCmd_give_or_take_item ── */
/* MapScriptCutsceneCmd_give_or_take_item:
 *   inputs:  r5 r7 r8 r9 r10 r11 r12 lr
 *   outputs: nz r1 r2 r4 r5 r7 r8 r9 r10 r11 r12
 *   clobbers (unused by callers): c v r0 r3 r6 lr
 *   preserved: r5
 *
 * Reads three script bytes (item id, count, and a mode flag), then either
 * gives or takes the item based on the mode byte. r7 is advanced past the
 * three-byte command header. Returns 1 in r0 (also sets nz).
 *
 * r7 on entry points at the cutscene command stream. The command layout is:
 *   [op] [item] [count] [mode]
 * r7 is bumped by 4 at the end to skip past it. */
Next MapScriptCutsceneCmd_give_or_take_item(void) {
    /* Read the item id (script byte 1). */
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);

    /* Read the count (script byte 2) into r1. */
    R0 = R4;
    R6 = 3;
    GUEST_CALL(ReadMapScriptByte);

    /* Read the mode (script byte 3) into r4, then give or take. */
    R1 = R4;
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);

    if (R4 == 1) {
        /* takeItem branch: r0 = item id, r1 = count. */
        GUEST_CALL(TakeItem);
    } else {
        /* giveItem branch: r0 = item id, r1 = count. */
        GUEST_CALL(GiveItem);
    }

    /* Skip past the 4-byte command header and signal success. */
    R7 = R7 + 4;
    SET_FLAGS_NZ(1);
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_run_or_stop_cutscene_camera_script ── */
/* CutsceneCmd_run_or_stop_cutscene_camera_script (0x0803816A): map-script
 * command handler for starting/stopping a cutscene camera script.
 *
 * r5 = cutscene state block, r7 = current read position in the map script.
 *
 * Command layout: a flag byte, followed -- only when the flag is 0 -- by a
 * word selecting the camera script to play:
 *   flag == 0: run the camera script named by the following word
 *   flag != 0: stop the running camera script and recenter on the player
 *
 * Always returns r0 = 1 (keep the script interpreter running) and advances
 * r7 past the consumed operands. */
Next CutsceneCmd_run_or_stop_cutscene_camera_script(void) {
    R6 = 1;                            /* operand size 1: flag byte */
    GUEST_CALL(ReadMapScriptByte);     /* flag byte -> R4 */

    if (R4 == 0) {
        /* Run: the next word is the camera script to play. */
        R6 = 2;                        /* operand size 2: script word */
        GUEST_CALL(ReadMapScriptWord); /* script word -> R4 */

        ST32(R5 + oCutsceneState_CutsceneCameraScriptPtr, R4);
        R0 = R4;
        GUEST_CALL(SetCutsceneCameraScript);
        ST8(R5 + oCutsceneState_CutsceneCameraScriptActive, 1);

        R7 += 6;                       /* skip flag byte + script word */
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        /* Stop: drop the active camera script and recenter the camera. */
        R0 = 0;
        ST32(R5 + oCutsceneState_CutsceneCameraScriptPtr, R0);
        GUEST_CALL(cutsceneCamera_focusCameraOnPlayerMaybe_8036faa);
        ST8(R5 + oCutsceneState_CutsceneCameraScriptActive, 0);

        R7 += 2;                       /* skip flag byte */
        R0 = 1;
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_start_fixed_battle ── */
/* Map script command: start a fixed battle.
 *
 * r7 is the script's position counter and r10 the toolkit. The command reads
 * one halfword operand (the battle preset index), starts that battle from
 * battle list 0, reads the operand again for the battle-setup helper, and
 * fades the screen out into the battle. On map groups below the Internet
 * block it first resets the Beast Out counter and the navi's stats/mood, so
 * they don't carry into the fight. It returns 1 to the script runner and
 * advances the position by 3 (opcode byte + one halfword operand).
 *
 * ReadMapScriptHalfword consumes the operand and delivers it in r4 (it
 * preserves r0/r1/r7/r10), so both consumers below take the value from r4. */

struct StartFixedBattleResult {
    u32 returnValue; /* r0: always 1, the command's result for the script runner */
    u32 scriptPos;   /* r7: position advanced past this command */
};

/* register form: calls SetBeastOutCounterTo3 outside this file, which has no typed C interface yet */
static void set_beast_out_counter_to_3(void) {
    GUEST_CALL(SetBeastOutCounterTo3);
}

/* register form: calls ZeroAllNaviStatsMood outside this file, which has no typed C interface yet */
static void zero_all_navi_stats_mood(void) {
    GUEST_CALL(ZeroAllNaviStatsMood);
}

/* register form: calls ReadMapScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_map_script_halfword_CutsceneCmd_start_fixed_battle(void) {
    R6 = 1;                 /* one halfword operand */
    GUEST_CALL(ReadMapScriptHalfword);
    return R4;              /* the halfword comes back in r4 */
}

/* register form: calls getBattleSettingsFromList0 outside this file, which has no typed C interface yet */
static u32 get_battle_settings_from_list0(u32 battleSettingsIdx) {
    R0 = battleSettingsIdx;
    GUEST_CALL(getBattleSettingsFromList0);
    return R0;              /* BattleSettings*, passed through opaquely */
}

/* register form: calls StartBattle outside this file, which has no typed C interface yet */
static void start_battle(u32 settings) {
    R0 = settings;
    R1 = 1;                 /* script-started battles always set this flag */
    GUEST_CALL(StartBattle);
}

/* register form: calls handleDriveOutEvents_803522E outside this file, which has no typed C interface yet */
static void battle_setup_helper(u32 operand) {
    R0 = operand;
    GUEST_CALL(handleDriveOutEvents_803522E);
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void set_screen_fade_CutsceneCmd_start_fixed_battle(u32 fadeTarget, u32 fadeSpeed) {
    R0 = fadeTarget;
    R1 = fadeSpeed;
    GUEST_CALL(SetScreenFade);
}

/* Body of the command; returns the advanced script position (r7). */
static struct StartFixedBattleResult start_fixed_battle(u32 scriptPos, u32 toolkit) {
    struct StartFixedBattleResult res;

    /* Outside the Internet map block, reset the Beast Out counter and the
     * navi's stats/mood before the battle takes over. */
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 mapGroup = LD8(gameState + oGameState_MapGroup);
    if ((s32)mapGroup < (s32)INTERNET_MAP_GROUP_START) {
        set_beast_out_counter_to_3();
        zero_all_navi_stats_mood();
    }

    /* Operand: index into battle list 0 of the preset to fight. */
    u32 battleIdx = read_map_script_halfword_CutsceneCmd_start_fixed_battle();
    start_battle(get_battle_settings_from_list0(battleIdx));

    /* The operand is read a second time and handed to the battle-setup helper. */
    u32 setupOperand = read_map_script_halfword_CutsceneCmd_start_fixed_battle();
    battle_setup_helper(setupOperand);

    /* Fade the screen out into the battle. */
    set_screen_fade_CutsceneCmd_start_fixed_battle(0x2C, 0x10);

    res.returnValue = 1;
    res.scriptPos = scriptPos + 3;
    return res;
}

/* CutsceneCmd_start_fixed_battle(r7 = script position, r10 = toolkit)
 * -> r0 = 1, r7 += 3; flags: c/v from `adds r7, #3`, n/z from `movs r0, #1`.
 * r1/r2/r3/r4/r6 are left as the callees left them. */
Next CutsceneCmd_start_fixed_battle(void) {
    u32 scriptPos = R7;
    struct StartFixedBattleResult res = start_fixed_battle(scriptPos, R10);
    R0 = res.returnValue;
    R7 = res.scriptPos;
    SET_FLAGS_ADD(scriptPos, 3);    /* c/v from `adds r7, #3` */
    SET_FLAGS_NZ(res.returnValue);  /* n/z from `movs r0, #1` */
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_run_eStruct200a6a0_callback ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_run_eStruct200a6a0_Callback_8002484_result;
/* register form: generated wrapper: run_eStruct200a6a0_Callback_8002484 has no typed C interface yet */
static inline call_run_eStruct200a6a0_Callback_8002484_result call_run_eStruct200a6a0_Callback_8002484(void) {
    GUEST_CALL(run_eStruct200a6a0_Callback_8002484);
    return (call_run_eStruct200a6a0_Callback_8002484_result){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* Map-script command "run cutscene callback": invoke the cutscene callback
 * (which leaves its own results in r1..r12 and keeps r10 as it was), then
 * advance the map-script program counter in r7 past this command. The status
 * in r0 is always 1 ("keep running"); the flags the callers see come from
 * that movs r0, #1, i.e. always not-zero/not-negative. The r7 value on entry
 * is dead: the callback writes r7 itself, and only its result is incremented. */
static u32 run_cutscene_cmd(u32 *script_counter) {
    call_run_eStruct200a6a0_Callback_8002484_result res =
        call_run_eStruct200a6a0_Callback_8002484();
    *script_counter = res.r7 + 1;
    return 1;
}

/* MapScriptCutsceneCmd_run_eStruct200a6a0_callback -> nz r2..r5, r7..r12 */
Next MapScriptCutsceneCmd_run_eStruct200a6a0_callback(void) {
    u32 script_counter;
    u32 status = run_cutscene_cmd(&script_counter);
    R7 = script_counter;
    SET_FLAGS_NZ(status);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_do_camera_shake ── */
/* CutsceneCmd_do_camera_shake: 0803822A-08038246 THUMB
 * Reads two bytes from the current map script: the shake magnitude and
 * the shake duration, then kicks off the camera shake effect and advances
 * the script pointer by 4. Returns 1.
 *
 * Contract:
 *   inputs:  r7 r10 lr
 *   outputs: nz r1 r2 r4 r7
 *   clobbers: c v r0 r6 lr
 */
Next CutsceneCmd_do_camera_shake(void) {
    u32 saved_lr = LR;

    /* Read magnitude byte from the script, in slot 1. */
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    /* R0 now holds the magnitude byte read from the script. */

    /* R0 = byte read into R4 (this is the count/slot for the next read). */
    R0 = R4;

    /* Read duration byte from the script, in slot 2. */
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);

    /* R1 = the duration byte the callee wrote into R4. */
    R1 = R4;

    /* Start the camera shake with the magnitude (R0) and duration (R1). */
    GUEST_CALL(camera_initShakeEffect_80302a8);

    /* Consume the two bytes that were just read. */
    R7 = R7 + 4;

    /* Return 1 to indicate the cutscene step was handled. */
    R0 = 1;

    LR = saved_lr;
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_init_scenario_effect ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r7; } call_ReadMapScriptByte_result_MapScriptCutsceneCmd_init_scenario_effect;
/* register form: generated wrapper: ReadMapScriptByte has no typed C interface yet */
static inline call_ReadMapScriptByte_result_MapScriptCutsceneCmd_init_scenario_effect call_ReadMapScriptByte_MapScriptCutsceneCmd_init_scenario_effect(u32 r6, u32 r7) {
    R6 = r6;
    R7 = r7;
    GUEST_CALL(ReadMapScriptByte);
    return (call_ReadMapScriptByte_result_MapScriptCutsceneCmd_init_scenario_effect){ R4, R7 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7; } call_initScenarioEffect_8003914_result;
/* register form: generated wrapper: initScenarioEffect_8003914 has no typed C interface yet */
static inline call_initScenarioEffect_8003914_result call_initScenarioEffect_8003914(u32 r0) {
    R0 = r0;
    GUEST_CALL(initScenarioEffect_8003914);
    return (call_initScenarioEffect_8003914_result){ R1, R2, R3, R4, R5, R6, R7 };
}

/* MapScriptCutsceneCmd_init_scenario_effect: run one "init scenario effect"
 * map-script cutscene command. The command consists of a single byte giving
 * the effect id; read it from the script (with flag 1, as the other cutscene
 * handlers do), pass it to the scenario-effect initializer, and advance the
 * script pointer in r7 past the 2-byte command. Returns the advanced pointer. */
static u32 init_scenario_effect_cmd(u32 script_pos /* r7 on entry */)
{
    /* Read the effect id byte; ReadMapScriptByte also leaves its (possibly
     * updated) r7 in the guest registers for the callee below to inherit. */
    call_ReadMapScriptByte_result_MapScriptCutsceneCmd_init_scenario_effect byte = call_ReadMapScriptByte_MapScriptCutsceneCmd_init_scenario_effect(1, script_pos);

    /* Initialize the scenario effect. It inherits the script pointer in r7
     * and leaves r2/r3/r4/r5/r7 behind in the guest registers; r2, r3 and r5
     * pass through unchanged, as in the original. */
    call_initScenarioEffect_8003914_result effect = call_initScenarioEffect_8003914(byte.r4);

    /* add r7, #2: skip past this command in the script. */
    return effect.r7 + 2;
}

/* MapScriptCutsceneCmd_init_scenario_effect (0x0803827A)
 * inputs: r7; outputs: nz r2 r3 r4 r5 r7.
 * r2/r3/r4/r5 are left exactly as the callees leave them, so the adapter
 * only writes the advanced script pointer and the flags. */
Next MapScriptCutsceneCmd_init_scenario_effect(void)
{
    R7 = init_scenario_effect_cmd(R7);
    SET_FLAGS_NZ(1);   /* final "mov r0, #1" leaves Z clear, N clear */
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_end_scenario_effect ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_endScenarioEffectMaybe_8003940_result;
/* register form: generated wrapper: endScenarioEffectMaybe_8003940 has no typed C interface yet */
static inline call_endScenarioEffectMaybe_8003940_result call_endScenarioEffectMaybe_8003940(u32 r2, u32 r3, u32 r4, u32 r6, u32 r8, u32 r9, u32 r11, u32 r12) {
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R6 = r6;
    R8 = r8;
    R9 = r9;
    R11 = r11;
    R12 = r12;
    GUEST_CALL(endScenarioEffectMaybe_8003940);
    return (call_endScenarioEffectMaybe_8003940_result){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* Map script command "end_scenario_effect": tears down whatever scenario
 * effect is currently running, advances the script cursor (r7) past this
 * one-slot command, and reports success to the interpreter (r0 = 1, so the
 * N/Z flags end up clear). The helper keeps r2/r3/r8/r9/r11/r12 as its own
 * working state and returns them; r4/r5/r6/r10 pass through untouched. */
static call_endScenarioEffectMaybe_8003940_result run_end_scenario_effect(
    u32 r2, u32 r3, u32 r4, u32 r6, u32 r8, u32 r9, u32 r11, u32 r12)
{
    call_endScenarioEffectMaybe_8003940_result out =
        call_endScenarioEffectMaybe_8003940(r2, r3, r4, r6, r8, r9, r11, r12);

    /* The callee leaves r7 alone; this command is one script slot long. */
    out.r7 = out.r7 + 1;
    return out;
}

Next MapScriptCutsceneCmd_end_scenario_effect(void) {
    call_endScenarioEffectMaybe_8003940_result out =
        run_end_scenario_effect(R2, R3, R4, R6, R8, R9, R11, R12);

    /* r4, r5, r6 and r10 are preserved by the callee, so no write-back. */
    R2 = out.r2, R3 = out.r3, R7 = out.r7,
    R8 = out.r8, R9 = out.r9, R11 = out.r11, R12 = out.r12;

    /* Command result: 1 ("done"), left in r0 by the original. */
    R0 = 1;

    /* movs r0, #1 sets N and Z from the constant 1. */
    SET_FLAGS_NZ(1);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_init_minigame_effect ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r7; } call_ReadMapScriptByte_result_MapScriptCutsceneCmd_init_minigame_effect;
/* register form: generated wrapper: ReadMapScriptByte has no typed C interface yet */
static inline call_ReadMapScriptByte_result_MapScriptCutsceneCmd_init_minigame_effect call_ReadMapScriptByte_MapScriptCutsceneCmd_init_minigame_effect(u32 r6, u32 r7) {
    R6 = r6;
    R7 = r7;
    GUEST_CALL(ReadMapScriptByte);
    return (call_ReadMapScriptByte_result_MapScriptCutsceneCmd_init_minigame_effect){ R4, R7 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7; } call_initMinigameEffect_8003a64_result;
/* register form: generated wrapper: initMinigameEffect_8003a64 has no typed C interface yet */
static inline call_initMinigameEffect_8003a64_result call_initMinigameEffect_8003a64(u32 r0) {
    R0 = r0;
    GUEST_CALL(initMinigameEffect_8003a64);
    return (call_initMinigameEffect_8003a64_result){ R1, R2, R3, R4, R5, R6, R7 };
}

/* Cutscene map-script command: read one byte operand and use it to start the
 * associated minigame effect, then skip past the operand in the script stream.
 * Always reports "command handled" (r0 = 1, flags from that value). */
static u32 run_init_minigame_effect_cmd(u32 cursor) {
    /* Read one byte with the fixed argument 1; ReadMapScriptByte also returns
     * the advanced script cursor. */
    call_ReadMapScriptByte_result_MapScriptCutsceneCmd_init_minigame_effect read = call_ReadMapScriptByte_MapScriptCutsceneCmd_init_minigame_effect(1, cursor);

    /* Start the minigame effect for the byte that was read. Its leftover
     * register results (r2-r5) stay as the callee leaves them. */
    call_initMinigameEffect_8003a64(read.r4);

    /* Skip the operand: one byte consumed by the read, one more here. */
    return read.r7 + 2;
}

/* MapScriptCutsceneCmd_init_minigame_effect(r7 = script cursor) -> nz r2 r3 r4 r5 r7 */
Next MapScriptCutsceneCmd_init_minigame_effect(void) {
    R7 = run_init_minigame_effect_cmd(R7);
    SET_FLAGS_NZ(1);
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_add_bbs_message_range ── */
/* MapScriptCutsceneCmd_add_bbs_message_range (0x080382BA)
 * Map-script cutscene command: add a range of BBS (bulletin-board) messages,
 * then advance the script cursor (r7) past the 4-byte command:
 *   +0 opcode, +1 u16 id of the first message, +3 u8 message count. */

#define BBS_RANGE_FIRST_ID_OFFSET 1 /* u16 at command+1: first message id */
#define BBS_RANGE_COUNT_OFFSET    3 /* u8 at command+3: how many to add   */
#define BBS_RANGE_CMD_SIZE        4 /* bytes consumed by the command      */

/* register form: calls ReadMapScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_map_script_halfword_MapScriptCutsceneCmd_add_bbs_message_range(u32 offset) {
    /* Reads the u16 at command+`offset`; this helper answers in r4 instead of r0. */
    R6 = offset;
    GUEST_CALL(ReadMapScriptHalfword);
    return R4;
}

/* What the byte reader hands back; the command consumes both. */
typedef struct {
    u32 start_id; /* r0: id the queued range starts from  */
    s32 count;    /* r4: repeat count read from the script */
} BbsMessageRange;

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static BbsMessageRange read_map_script_byte_MapScriptCutsceneCmd_add_bbs_message_range(u32 start_id, u32 offset) {
    BbsMessageRange range;
    /* Reads the u8 at command+`offset`. The original calls it with the halfword in
     * r0, starts its loop from the r0 it gets back, and takes the repeat count from
     * the r4 it leaves behind — not from the earlier halfword read. */
    R0 = start_id;
    R6 = offset;
    GUEST_CALL(ReadMapScriptByte);
    range.start_id = R0;
    range.count = (s32)R4;
    return range;
}

/* register form: calls reqBBS_addBBSMessage_813e5dc outside this file, which has no typed C interface yet */
static void queue_bbs_message(u32 message_id, u32 count_remaining) {
    /* Queues one BBS message (r0 = id); the original enters this call with the id
     * mirrored in r6 and the remaining count in r4. */
    R0 = message_id;
    R6 = message_id;
    R4 = count_remaining;
    GUEST_CALL(reqBBS_addBBSMessage_813e5dc);
}

/* What the command hands back through the adapter. */
typedef struct {
    u32 script_cursor; /* r7 out: advanced past the whole command            */
    s32 messages_left; /* r4 out: counter remainder (0, or -1 if count was 0) */
} BbsMessageRangeResult;

/* Queue `count` consecutive BBS messages starting at `start_id`, then step the script
 * cursor past the command. The body always runs at least once — the original only
 * tests the counter after the first request — so a count of 0 still queues one
 * message and leaves a remainder of -1. */
static BbsMessageRangeResult add_bbs_message_range(u32 script_cursor) {
    BbsMessageRangeResult result;
    BbsMessageRange range;
    u32 first_id;
    u32 message_id;
    s32 messages_left;

    /* u16 at +1: id the range starts from, handed on to the byte reader in r0. */
    first_id = read_map_script_halfword_MapScriptCutsceneCmd_add_bbs_message_range(BBS_RANGE_FIRST_ID_OFFSET);
    /* u8 at +3: how many messages to add (count comes back in r4, start id in r0). */
    range = read_map_script_byte_MapScriptCutsceneCmd_add_bbs_message_range(first_id, BBS_RANGE_COUNT_OFFSET);

    message_id = range.start_id;
    messages_left = range.count;
    do {
        queue_bbs_message(message_id, (u32)messages_left);
        message_id++;
        messages_left--;
    } while (messages_left > 0);

    result.script_cursor = script_cursor + BBS_RANGE_CMD_SIZE;
    result.messages_left = messages_left;
    return result;
}

/* MapScriptCutsceneCmd_add_bbs_message_range(r7 = script cursor)
 * -> r0 = 1, r7 = cursor + 4, r4 = counter remainder, nz from r0 = 1;
 *    r1/r2 keep whatever the last guest call left in them. */
Next MapScriptCutsceneCmd_add_bbs_message_range(void) {
    BbsMessageRangeResult result = add_bbs_message_range(R7);
    R7 = result.script_cursor;
    R4 = (u32)result.messages_left;
    R0 = 1; /* command handled */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_navi_cmd_80340f6 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r4, r5, r7, r8, r9, r10, r11, r12; } call_endOfSection_NaviSetup_80340F6_result;
/* register form: generated wrapper: endOfSection_NaviSetup_80340F6 has no typed C interface yet */
static inline call_endOfSection_NaviSetup_80340F6_result call_endOfSection_NaviSetup_80340F6(u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r11, u32 r12) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R8 = r8;
    R9 = r9;
    R11 = r11;
    R12 = r12;
    GUEST_CALL(endOfSection_NaviSetup_80340F6);
    return (call_endOfSection_NaviSetup_80340F6_result){ R2, R3, R4, R5, R7, R8, R9, R10, R11, R12 };
}

/* One "navi cmd" cutscene command: run the NaviSetup end-of-section handler on
 * the current script state, then advance the script pointer (r7) past this
 * command. The handler's register results (r2..r12) are passed straight
 * through; r0 is set to 1 afterwards as the command's success result, and the
 * flags the caller sees come from that value. */
static call_endOfSection_NaviSetup_80340F6_result navi_cmd_step(
    u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12)
{
    call_endOfSection_NaviSetup_80340F6_result out =
        call_endOfSection_NaviSetup_80340F6(r4, r5, r6, r7, r8, r9, r11, r12);

    out.r7 += 1; /* advance the cutscene script pointer to the next command */
    return out;
}

/* CutsceneCmd_navi_cmd_80340f6 -> nz, r2 r3 r4 r5 r7 r8 r9 r10 r11 r12 */
Next CutsceneCmd_navi_cmd_80340f6(void) {
    call_endOfSection_NaviSetup_80340F6_result out =
        navi_cmd_step(R4, R5, R6, R7, R8, R9, R10, R11, R12);

    R2 = out.r2, R3 = out.r3, R4 = out.r4, R5 = out.r5, R7 = out.r7,
    R8 = out.r8, R9 = out.r9, R10 = out.r10, R11 = out.r11, R12 = out.r12;

    SET_FLAGS_NZ(1); /* mov r0, #1 */
    RETURN_TO_CALLER();
}

/* ── MapScriptCutsceneCmd_add_mail_range ── */
/* Cutscene script command "add mail range": queue a run of mails into the
 * inbox.  The dispatcher hands the script cursor (this command's position in
 * the script) in r7 and its leftover in r0; the operand readers are given
 * that same cursor every time and never advance it -- the cursor moves only
 * by the final +4 that steps past this 4-byte command.
 *
 * The readers (ReadMapScriptHalfword/Byte) take an argument in r0, an
 * operand selector in r6 and the cursor in r7, and they do NOT preserve r4:
 * the halfword read (whose own result in r0 is discarded) leaves a working
 * value in r4 that is handed to the byte read in r0, and the byte read
 * leaves the length of the mail run in r4.  The entry value of r4 is never
 * used.  The loop tests after the first mail, so one mail is queued even
 * for a zero length.  r1/r2 are command outputs only as leftovers of the
 * last addMail call and are left alone here. */

typedef struct {
    s32 mailsLeft;     /* final r4: the run counter once the loop is done */
    u32 scriptCursor;  /* final r7: the cursor stepped past this command */
} MailRangeResult;

/* register form: calls ReadMapScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_map_script_halfword_MapScriptCutsceneCmd_add_mail_range(u32 argument, u32 operand, u32 scriptCursor) {
    R0 = argument;
    R6 = operand;
    R7 = scriptCursor;
    GUEST_CALL(ReadMapScriptHalfword);
    return R4;    /* the halfword itself (r0) is discarded; the reader's r4 output carries a working value on */
}

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_map_script_byte_MapScriptCutsceneCmd_add_mail_range(u32 argument, u32 operand, u32 scriptCursor, s32 *runLengthOut) {
    R0 = argument;
    R6 = operand;
    R7 = scriptCursor;
    GUEST_CALL(ReadMapScriptByte);
    *runLengthOut = (s32)R4;   /* the reader leaves the run length in r4 */
    return R0;                 /* the byte read: the id of the first mail */
}

/* register form: calls addMail_802f238 outside this file, which has no typed C interface yet */
static void add_mail(u32 mailId, s32 mailsLeft, u32 scriptCursor) {
    R0 = mailId;    /* addMail takes the mail id in r0 and r6, the remaining
                     * count in r4 and the cursor in r7; its r0/r1/r2 results
                     * are not used by this command */
    R4 = (u32)mailsLeft;
    R6 = mailId;
    R7 = scriptCursor;
    GUEST_CALL(addMail_802f238);
}

static MailRangeResult add_mail_range(u32 commandArg, u32 scriptCursor) {
    MailRangeResult result;
    u32 mailId;
    s32 runLength;
    u32 workingValue;

    /* operand 1: a halfword the command does not use */
    workingValue = read_map_script_halfword_MapScriptCutsceneCmd_add_mail_range(commandArg, 1, scriptCursor);

    /* operand 2: the id of the first mail of the run */
    mailId = read_map_script_byte_MapScriptCutsceneCmd_add_mail_range(workingValue, 3, scriptCursor, &runLength);

    /* queue mailId, mailId + 1, ... (always at least one mail) */
    do {
        add_mail(mailId, runLength, scriptCursor);
        mailId++;
    } while (--runLength > 0);

    result.mailsLeft = runLength;
    result.scriptCursor = scriptCursor + 4;   /* skip the rest of the command */
    return result;
}

Next MapScriptCutsceneCmd_add_mail_range(void) {
    MailRangeResult result = add_mail_range(R0, R7);   /* r0 rides through to the first operand read */
    R4 = (u32)result.mailsLeft;
    R7 = result.scriptCursor;
    R0 = 1;            /* the command's status result */
    SET_FLAGS_NZ(1);   /* mov r0, #1: Z clear, N clear */
    RETURN_TO_CALLER();
}

/* ── SetCutsceneFlag ── */
/* SetCutsceneFlag(r0 = flag index, r5 = cutscene state):
 * sets bit (r0 & 0xFF) in the cutscene state's flag word.
 * Returns the updated flag word in r0; r1 is preserved. */
Next SetCutsceneFlag(void) {
    u32 bit = R0 & 0xFF;
    /* ARM LSL by a register yields 0 for shift counts >= 32 */
    u32 mask = (bit < 32) ? (1u << bit) : 0;

    u32 flags = LD32(R5 + oCutsceneState_CutsceneFlags) | mask;
    ST32(R5 + oCutsceneState_CutsceneFlags, flags);

    R0 = flags;
    RETURN_TO_CALLER();
}

/* ── ClearCutsceneFlag ── */
/* ClearCutsceneFlag(r0 = flag bit index, r5 = cutscene state object):
 * clear bit (r0 & 0xFF) of the cutscene flags word. r1, r5 and the carry
 * flag are preserved for the caller; r0 and the nz flags are scratch.
 * The carry the caller sees comes from the flag-setting shift that builds
 * the bit mask: carry = last bit shifted out of (1 << n). */
Next ClearCutsceneFlag(void) {
    u32 bitIndex = R0 & 0xFF;
    u32 mask;

    /* lsls r1, r0: mask = 1 << bitIndex (0 for shift counts >= 32) */
    if (bitIndex < 32)
        mask = 1u << bitIndex;
    else
        mask = 0;

    ST32(R5 + oCutsceneState_CutsceneFlags,
         LD32(R5 + oCutsceneState_CutsceneFlags) & ~mask);

    /* Carry out of the shift: bit 32-n of the value 1.
     * A shift count of 0 leaves the carry flag untouched. */
    if (bitIndex != 0) {
        if (bitIndex == 32)
            SET_FLAGS_ADD(0xFFFFFFFFu, 1); /* carry out = 1 */
        else
            SET_FLAGS_ADD(0xFFFFFFFFu, 0); /* carry out = 0 */
    }

    RETURN_TO_CALLER();
}

/* ── TestCutsceneFlag ── */
/* TestCutsceneFlag(r0 = cutscene flag number, r5 = cutscene state pointer):
 * tests whether the flag numbered r0 is set in the state's cutscene flag
 * word. Leaves r0 = the whole flag word; the Z flag is clear iff the flag
 * is set, so callers branch EQ/NE on the result. r1 is preserved. */
Next TestCutsceneFlag(void) {
    /* Build the flag's bit mask: r1 = 1 << flagNumber, reproducing the
     * barrel shifter's carry output (TST afterwards only touches N/Z). */
    u32 flagNumber = R0 & 0xFF;
    u32 flagBit = 1;

    if (flagNumber != 0) {
        if (flagNumber < 32) {
            flagBit <<= flagNumber;
            cpu.c = 0;                  /* shifting the value 1 left only pushes out zero bits */
        } else {
            flagBit = 0;
            cpu.c = (flagNumber == 32); /* a 32-bit shift pushes out the value's bit 0 */
        }
    }
    /* flagNumber == 0: the mask stays 1 and the carry flag is untouched. */

    u32 flagWord = LD32(R5 + oCutsceneState_CutsceneFlags);
    SET_FLAGS_NZ(flagWord & flagBit);
    R0 = flagWord;
    RETURN_TO_CALLER();
}
