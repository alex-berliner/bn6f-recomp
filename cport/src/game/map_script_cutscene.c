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

    R5 = A_eMapScriptState_Unk_00;
    R0 = LD32(R5 + oMapScriptState_SecondaryContinuousMapScriptPtr);

    if (R0 != 0) {
        R4 = saved_r12;                     /* mov r4, r12 — handler sees saved r12 */
        R12 = A_map_script_cutscene;        /* ldr r6/jumptable; mov r12, r6 */
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
    ST16(unk2001c04 + oS2001c04_Unk_12, 0);
    ST16(unk2001c04 + oS2001c04_Unk_14, 0);

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
 * literal pool references via A_eWarp2011bb0End), so the script starts fresh.
 * Then calls camera_writeUnk03_14_80301b2(0) to reset the camera's pending
 * write state; the final r1/r2 values are whatever that callee leaves. */
Next SetCutsceneCameraScript(void) {
    /* ldr r1, =eCutsceneCameraInfo */
    u32 cameraInfo = A_eWarp2011bb0End;

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

/* ── CutsceneCameraCmd_end ── */
/* CutsceneCameraCmd_end:
 * "end" camera script command: finish the current camera command and signal
 * success by returning 0 (flags set accordingly for callers that branch on it). */
Next CutsceneCameraCmd_end(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
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

/* ── MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player ── */
/* register form: calls ReadOWPlayerObjectCoords outside this file, which has no typed C interface yet */
static void read_ow_player_coords(u32 *x, u32 *y, u32 *z) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    *x = R0;
    *y = R1;
    *z = R2;
}

/* register form: calls ReadMapScriptHalfword outside this file, which has no typed C interface yet */
static u32 read_map_script_halfword_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(u32 offset) {
    R6 = offset;
    GUEST_CALL(ReadMapScriptHalfword);
    return R4;
}

/* register form: calls ReadMapScriptByte outside this file, which has no typed C interface yet */
static u32 read_map_script_byte_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(u32 offset) {
    R6 = offset;
    GUEST_CALL(ReadMapScriptByte);
    return R4;
}

/* register form: calls ReadMapScriptWord outside this file, which has no typed C interface yet */
static u32 read_map_script_word_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(u32 offset) {
    R6 = offset;
    GUEST_CALL(ReadMapScriptWord);
    return R4;
}

/* register form: calls SpawnOverworldMapObject outside this file, which has no typed C interface yet.
 * The callee returns the new object handle in r5 and clobbers it, so we save and restore
 * the caller's r5 across the GUEST_CALL. */
static u32 spawn_overworld_map_object(u32 obj_type, u32 x, u32 y, u32 z,
                                      u32 slot_ptr, u32 caller_r5) {
    R0 = obj_type;
    R1 = x;
    R2 = y;
    R3 = z;
    R6 = slot_ptr;
    GUEST_CALL(SpawnOverworldMapObject);
    u32 handle = R5;
    R5 = caller_r5;
    return handle;
}

/*
 * Spawn an overworld map object at a position relative to the OW player.
 *
 * The script supplies three 16-bit signed offsets (x,y,z) that are shifted
 * into the upper half of a 32-bit fixed-point value and added to the player's
 * current coordinates.  A byte selects the object type and a word carries
 * additional data.  After spawning, the returned handle is stored in the
 * CutsceneState slot identified by the low nibble of the slot index.
 */
static u32 spawn_ow_map_object_relative(u32 slot_index, u32 base_addr) {
    /* slot pointer = base + ((index & 0xF) * 4) + field offset */
    u32 slot_ptr = base_addr + ((slot_index & 0xF) << 2)
                              + oCutsceneState_owMapObjectPtrs_44;

    u32 px, py, pz;
    read_ow_player_coords(&px, &py, &pz);

    /* relative offsets from script, placed in upper 16 bits (0x10 shift) */
    u32 abs_x = px + (read_map_script_halfword_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(3) << 16);
    u32 abs_y = py + (read_map_script_halfword_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(5) << 16);
    u32 abs_z = pz + (read_map_script_halfword_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(7) << 16);

    u32 obj_type = read_map_script_byte_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(2);
    read_map_script_word_MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(9);

    u32 handle = spawn_overworld_map_object(obj_type, abs_x, abs_y, abs_z,
                                           slot_ptr, base_addr);
    ST32(slot_ptr, handle);

    return 1;
}

/*
 * MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player
 *   r4 = slot index (only low 4 bits used), r5 = CutsceneState base pointer
 *   advances r7 by 0xD, returns 1 in r0, flags from r7+0xD
 */
Next MapScriptSubCmd_spawn_ow_map_object_relative_to_ow_player(void) {
    u32 old_r7 = R7;
    R0 = spawn_ow_map_object_relative(R4, R5);
    R7 = old_r7 + 0xD;
    SET_FLAGS_ADD(old_r7, 0xD);
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
