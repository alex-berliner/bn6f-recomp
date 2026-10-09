/*
 * leaves_farm_7.c: small functions from the model farm, set 7.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── ZeroFillByEightWords ── */
/* CpuFastSet mode bit 24: fill the destination with the single source word
 * instead of copying. */
#define CPUFILL_MODE 0x1000000u

/* ZeroFillByEightWords(r0 = destination, r1 = length in bytes):
 * Zero a buffer through the BIOS CpuFastSet in fill mode (the BIOS copies
 * eight words per burst, hence the name). A single zero word kept briefly
 * on the stack serves as the fill source. r0-r3 and LR are preserved for
 * the caller.
 *
 * Flag behaviour: the last flag-setting instruction before the BIOS call is
 * `mov r0, #0` (encoded MOVS), so the call is entered with N=0, Z=1; C/V are
 * whatever the preceding `mov r1, r0` (encoded ADDS r1, r0, #0) left, i.e.
 * clear for any pointer. The BIOS call itself leaves the flags untouched, so
 * the caller sees N=0, Z=1, C=0, V=0. */
Next ZeroFillByEightWords(void) {
    u32 dest = R0;
    u32 savedR0 = R0, savedR1 = R1, savedR2 = R2, savedR3 = R3;

    /* r2 = fill mode | length in words (the original computed this from
     * `lsr r1, r1, #2` + `orr r2, r1`; its flag side effects are dead). */
    R2 = CPUFILL_MODE | (R1 >> 2);
    R1 = dest;                      /* BIOS destination pointer */

    /* Flags as left by `mov r1, r0` (ADDS r1, r0, #0): C/V clear for any
     * address; N/Z are immediately overwritten below but modelled anyway. */
    SET_FLAGS_ADD(dest, 0);

    /* One zero word on the stack is the fill source. */
    SP -= 4;
    ST32(SP, 0);

    /* `mov r0, #0` (MOVS) is the final flag-setting instruction: N=0, Z=1. */
    SET_FLAGS_NZ(0);
    R0 = SP;                        /* BIOS source pointer */
    GUEST_CALL(SWI_CpuFastSet);

    SP += 4;

    /* The BIOS call clobbers r0-r3; the original pushed them and pops them
     * back, so hand the caller's values through untouched. */
    R0 = savedR0;
    R1 = savedR1;
    R2 = savedR2;
    R3 = savedR3;
    RETURN_TO_CALLER();
}

/* ── ZeroFillByWord ── */
/* ZeroFillByWord(r0 = dest, r1 = size in bytes):
 * Clears `size` bytes at `dest` to zero, a word at a time, via BIOS CpuSet.
 * The zero source word is kept in a stack slot and passed with the "fixed
 * source" mode bit so only the destination is written.
 * Preserves r0-r3 and LR; flags are left by the `adds r1, r0, #0` (the
 * encoding of `mov r1, r0`) followed by `movs r0, #0`:
 * N=0, Z=1, C=0 (dest + 0 never carries), V=0 (no overflow possible). */

/* CpuSet control word: 32-bit transfer with fixed (non-incrementing) source;
 * the low bits hold the transfer length in words. */
#define CPUSET_MODE_FIXED_SRC_32BIT 0x5000000u

Next ZeroFillByWord(void) {
    u32 dest = R0;
    u32 size_bytes = R1;
    u32 saved_r2 = R2;
    u32 saved_r3 = R3;

    /* mov r1, r0 is encoded as adds r1, r0, #0: copy dest into r1 while
     * setting the flags (C and V are cleared, since dest + 0 can neither
     * carry out of bit 31 nor overflow). */
    SET_FLAGS_ADD(dest, 0);

    /* Reserve one word of stack to hold the fill value. */
    u32 zero_word = SP -= 4;
    ST32(zero_word, 0);
    SET_FLAGS_NZ(0);            /* movs r0,#0: Z set, N clear; C/V unchanged */

    R0 = zero_word;             /* src  = the zero word on the stack */
    R1 = dest;                  /* dest = caller's buffer */
    R2 = CPUSET_MODE_FIXED_SRC_32BIT | (size_bytes >> 2); /* word count */
    GUEST_CALL(SWI_CpuSet);

    SP += 4;

    /* pop {r0-r3,pc}: the argument registers survive the call unchanged. */
    R0 = dest;
    R1 = size_bytes;
    R2 = saved_r2;
    R3 = saved_r3;
    RETURN_TO_CALLER();
}

/* ── main_zeroFill_80017EC ── */
/* main_zeroFill_80017EC:
 * Clear the 2-halfword palette buffer at palette_3001960 and the 2-halfword
 * block at 0x5000000 (BG palette memory), halfword by halfword.
 * r2 and r3 are preserved; r3 is the output value. */
Next main_zeroFill_80017EC(void) {
    R0 = A_palette_3001960;
    R1 = 2;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = 0x5000000;
    R1 = 2;
    GUEST_CALL(ZeroFillByHalfword);

    RETURN_TO_CALLER();
}

/* ── musicGameState_8000784 ── */
/* musicGameState_8000784(r0, r2 = sound-driver args, r10 = Toolkit):
 * Forward a music-state request to the sound driver (sound_8000808) with the
 * callback sound_tickAllMusicPlayers_814EA58 in r3, keeping r10 safe across the call. Afterwards,
 * flag the game state so the engine treats background music as switched off
 * (bgMusicIndicator = 0xFF).
 * r0 ends as 0xFF (NZ set by the "mov r0,#0xFF"), r1 = incoming r10,
 * r3 = callback address; r4/r5/r6/r7/r10 pass through untouched. */
Next musicGameState_8000784(void) {
    u32 toolkit = R10;

    R1 = toolkit;                    /* saved r10, passed as arg to the driver */
    R3 = A_sound_tickAllMusicPlayers_814EA58 | 1;          /* music-state handler callback (thumb) */
    GUEST_CALL(sound_8000808);       /* r0 and r2 pass through unchanged */

    /* restore the values the original keeps across the call */
    R10 = toolkit;
    R1 = toolkit;

    /* original loads the gamestate pointer into r7 here, but r7 is then
     * restored from the stack by "pop {r7,pc}" -- it must stay unchanged. */
    u32 gamestate = LD32(toolkit + oToolkit_GameStatePtr);

    R0 = 0xFF;                       /* Thumb "mov r0,#0xFF" sets NZ */
    SET_FLAGS_NZ(R0);
    ST8(gamestate + oGameState_BGMusicIndicator, R0);

    RETURN_TO_CALLER();
}

/* ── bg_ClearTilesAndReloadGraphics_80015B4 ── */
/* bg_ClearTilesAndReloadGraphics_80015B4: clear the BG tile VRAM area (0x600E000..0x600FFFF) and rebuild it.
 * Takes no real arguments; r2/r3/r10 are untouched pass-through state. */
Next bg_ClearTilesAndReloadGraphics_80015B4(void) {
    /* Fill the BG tile IDs region in VRAM with zeros, 8 words at a time. */
    R0 = A_vBGTileIds;
    R1 = 0x2000;
    GUEST_CALL(ZeroFillByEightWords);

    /* Reset the palette/graphics block at 0x30025c0. */
    GUEST_CALL(ZeroFillGFX30025c0);

    /* Reload the BG tile graphics from ROM. */
    GUEST_CALL(copyMemory_8001850);

    RETURN_TO_CALLER();
}

/* ── sprite_gfxAnimState_GotoNext_8002310 ── */
/* sprite_gfxAnimState_GotoNext_8002310(r0 = &GFXAnimData.ParamNext, r4/r5/r6 = scratch passed through,
 * r7 = GFXAnimState):
 * Advance an animation state to its "next" animation: read the linked
 * GFXAnimData slot (top bit cleared as a flag/mask), gather the state's
 * parameters, and hand everything to sub_8002378 to install the new
 * animation. All argument registers are consumed by the callee; r4-r7 are
 * preserved for our caller (the original saves/restores r4 and r7). */
Next sprite_gfxAnimState_GotoNext_8002310(void) {
    u32 animDataPtr = R0;
    u32 savedR4 = R4;
    u32 state = R7;

    /* GFXAnimData.ParamNext with the top bit stripped (<<1 >>1). */
    u32 nextState = (LD32(animDataPtr) << 1) >> 1;

    u32 param0 = LD8(state + oGFXAnimState_Param0);
    u32 param2 = LD8(state + oGFXAnimState_Param2);
    u32 index  = LD8(state + 1 /* oGFXAnimState_Index */);
    u32 param1 = LD32(state + oGFXAnimState_Param1);

    R0 = param0;
    R1 = nextState;
    R2 = param2;
    R3 = index;
    R4 = param1;
    GUEST_CALL(sub_8002378);

    /* The original pops r4/r7 back to their entry values before returning. */
    R4 = savedR4;
    R7 = state;
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

/* ── NPCCommand_give_attached_shadow ── */
/* NPCCommand_give_attached_shadow (0x0809EFB4)
 * NPC script command: detach the NPC's shadow. Clears the shadow flag in the
 * NPC's flags_60 word, tells the sprite layer to stop drawing a shadow, then
 * advances the script instruction pointer (r6) by one. The flags left behind
 * are those of that pointer increment.
 *
 * inputs:  r5 = NPC object, r6 = script instruction pointer, lr
 * outputs: nz c v (from ADDS r6, #1), r0/r1/r3 as left by sprite_noShadow,
 *          r6 = pointer + 1, lr = return address of the sprite_noShadow call */
Next NPCCommand_give_attached_shadow(void) {
    u32 npc = R5;

    /* clear the "has attached shadow" bit */
    u32 flags = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    flags &= ~OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;
    ST32(npc + oOverworldNPCObject_UnkFlags_60, flags);

    sprite_noShadow();

    /* advance the script pointer; ADDS sets nz/c/v for the caller */
    u32 next = R6 + 1;
    SET_FLAGS_ADD(R6, 1);
    R6 = next;

    RETURN_TO_CALLER();
}

/* ── NPCCommand_jump_if_progress_in_range ── */
/* NPCCommand_jump_if_progress_in_range: 0809F270-0809F292 THUMB, bn6f npc.s:2339
 * NPC script opcode: "jump if the game progress is within a range".
 * Script operands at r6: +1 = minimum progress, +2 = maximum progress,
 * +3..+4 = 16-bit jump target (fetched via ReadNPCScriptWord).
 * If min <= game progress <= max, r6 is set to the jump target word;
 * otherwise r6 is advanced past the whole 7-byte command.
 * r0 is the game progress on the skip path, the fetched target on the jump
 * path; r1/r2 hold the bounds on the paths where they were loaded. */
Next NPCCommand_jump_if_progress_in_range(void) {
    u32 scriptPtr = R6;
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 progress = LD8(gameState + 6);      /* oGameState_GameProgress */

    R0 = progress;
    R1 = LD8(scriptPtr + 1);                /* minimum progress */
    if ((s32)progress >= (s32)R1) {
        R2 = LD8(scriptPtr + 2);            /* maximum progress */
        if ((s32)progress <= (s32)R2) {
            /* In range: read the 16-bit target word following the operands. */
            R0 = scriptPtr + 3;
            GUEST_CALL(ReadNPCScriptWord);
            R6 = R0;                        /* movs r6, r0: sets flags like adds r6, r0, #0 */
            SET_FLAGS_ADD(R0, 0);
            RETURN_TO_CALLER();
        }
    }
    /* Out of range: skip this 7-byte command. */
    R6 = scriptPtr + 7;
    SET_FLAGS_ADD(scriptPtr, 7);
    RETURN_TO_CALLER();
}

/* ── NPCCommand_wait_cutscene_var ── */
/* NPCCommand_wait_cutscene_var (0x0809F2DE):
 * NPC script command "wait for cutscene variable". Puts the NPC into the
 * WAIT_CUTSCENE_VAR action (its script stays paused until the watched
 * cutscene variable reaches the given value) and stores the two operand
 * bytes from the script: the variable index and the value to wait for.
 * Inputs:  r5 = NPC object, r6 = script read cursor (operand bytes at +1/+2)
 * Outputs: r6 advanced past the command, flags from `adds r6, #3`, r0 from
 *          the called helper; r7 preserved. */
Next NPCCommand_wait_cutscene_var(void) {
    /* Enter the wait-for-cutscene-var state and clear the movement flag.
     * Offset 9 is the NPC object's current-action byte (CurAction). */
    R0 = MOVEMENT_FLAG_WAIT_CUTSCENE_VAR;
    ST8(R5 + 9, R0);

    R0 = 0;
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, R0);

    /* Operand 1: index of the cutscene variable to watch. */
    R0 = LD8(R6 + 1);
    ST32(R5 + oOverworldNPCObject_CutsceneVarIndexToWaitFor, R0);

    /* Operand 2: value the variable must reach. */
    R0 = LD8(R6 + 2);
    ST32(R5 + oOverworldNPCObject_CutsceneVarValueToWaitFor, R0);

    /* Mark the NPC's script as suspended (action 0x19 handling). */
    GUEST_CALL(npc_disableScript0x19_809f51e);

    /* Consume the command (opcode byte + two operands). */
    R6 += 3;
    SET_FLAGS_ADD(R6, 3);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_wait_mystery_data_taken ── */
/* NPCCommand_wait_mystery_data_taken (0x0809F338):
 * NPC script command handler for "wait until the mystery data has been taken".
 * Marks the NPC as waiting (CurAction = MOVEMENT_FLAG_WAIT_MYSTERY_DATA_TAKEN),
 * clears its movement flags, reads the mystery data id (halfword at script
 * pointer + 1) and stores it into the NPC object, disables the NPC's script
 * processing, then advances the script pointer past the command
 * (opcode + 2-byte id = 3 bytes).
 *
 * inputs:  R5 = NPC object, R6 = script pointer
 * outputs: R6 = script pointer + 3, flags from that ADDS; R0/R1 as left by
 *          the callees; R7 preserved (the original pushes/pops it, but only
 *          uses it as a scratch offset register, so we never touch it). */
Next NPCCommand_wait_mystery_data_taken(void) {
    /* CurAction lives at offset 9 in the NPC object (no named macro). */
    ST8(R5 + 9, MOVEMENT_FLAG_WAIT_MYSTERY_DATA_TAKEN);
    ST16(R5 + oOverworldNPCObject_MovementFlag_0a_Unk_0b, 0);

    R0 = R6 + 1;
    GUEST_CALL(ReadNPCScriptHalfword);
    ST32(R5 + oOverworldNPCObject_MysteryDataId, R0);

    GUEST_CALL(npc_disableScript0x19_809f51e);

    {
        u32 prev = R6;
        R6 = prev + 3;
        SET_FLAGS_ADD(prev, 3);
    }

    RETURN_TO_CALLER();
}

/* ── HandleSaveMenu8132B88 ── */
/* HandleSaveMenu8132B88: dispatcher for the save-menu scene.
 *
 * R5 points at the save-menu work area; the byte at offset 1 holds the
 * scene's current state. The original uses that byte *directly* as a byte
 * offset into the jump table (no <<2), so the stored values are the word
 * offsets themselves:
 *   0 = OpenSaveMenu8132BA8  - open the save menu
 *   4 = SaveMenuUpdate8132C34 - run the save menu (update)
 *   8 = ExitSaveMenu8132CA0  - leave the save menu (called repeatedly
 *                              while the screen dims)
 *
 * At the call the original has R0 = the raw table entry (the handler's
 * address, thumb bit set) and R1 = the state byte; both are reproduced here
 * even though the handlers take no arguments. The handler returns to this
 * function (the `mov lr, pc` / `pop {pc}` pair), which then returns to this
 * function's caller, so the handler's registers and flags pass through.
 */
Next HandleSaveMenu8132B88(void) {
    u32 state = LD8(R5 + 1);

    /* Reproduce the original's table lookup: the raw byte indexes the table
     * by bytes, and the loaded handler pointer ends up in R0. */
    R0 = LD32(A_SaveMenuJumpTable8132B9C + state);
    R1 = state;

    switch (state) {
    case 0: GUEST_CALL(OpenSaveMenu8132BA8);   break;
    case 4: GUEST_CALL(SaveMenuUpdate8132C34); break;
    case 8: GUEST_CALL(ExitSaveMenu8132CA0);   break;
    }

    RETURN_TO_CALLER();
}

/* ── SaveMenuUpdate8132C34 ── */
/* SaveMenuUpdate8132C34: save-menu state dispatcher.
 *
 * R5 points to the save-menu work area; the byte at offset 2 holds the current
 * menu state as a byte offset into JumpTable8132C48 (the original does a plain
 * `ldr r0, [table, state]`, so the stored value is already 4 bytes per entry):
 *   0 -> SaveMenuOpenUpdate8132C50 (menu opening)
 *   4 -> SaveMenuUpdate8132C68     (menu running)
 *
 * The selected handler is entered with r0 = its own table word (including the
 * Thumb bit, exactly as loaded from ROM) and r1 = the raw state byte. It runs
 * as a tail call: the original reaches it via `mov lr, pc; bx r0` and its
 * `pop {pc}` sends the handler's results, registers and flags straight back
 * to this function's caller. */
Next SaveMenuUpdate8132C34(void) {
    u32 state = LD8(R5 + 2);

    /* Reproduce the original's register state at the indirect call:
     * r1 = state byte, r0 = handler address read from the jump table. */
    R1 = state;
    R0 = LD32(A_JumpTable8132C48 + state);

    switch (state) {
    case 0: /* menu is opening */
        GUEST_CALL(SaveMenuOpenUpdate8132C50);
        break;
    case 4: /* menu is active */
        GUEST_CALL(SaveMenuUpdate8132C68);
        break;
    default:
        /* The table only has two entries; no other state occurs in play. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_813399C ── */
/* sub_813399C: virtual-method dispatch on the object at r5, followed by a
 * fixed tail call.
 *
 * The byte at (r5 + 2) selects a handler from the two-entry table at
 * off_81339B4 — both entries hold sub_81339BC+1, so the fetched pointer is
 * always that function. The original performs the call with the classic
 * "mov lr, pc / bx r0" idiom: the handler is entered with R0 still holding
 * the fetched table entry (its own address) and R1 holding the selector,
 * and it returns straight into the call below. */
Next sub_813399C(void) {
    u32 method = LD8(R5 + 2);
    u32 handler = LD32(A_off_81339B4 + method); /* both entries: sub_81339BC+1 */

    R0 = handler;
    R1 = method;
    GUEST_CALL(sub_81339BC);

    GUEST_CALL(sub_8134490);
    RETURN_TO_CALLER();
}

/* ── sub_8134538 ── */
/* sub_8134538: thin wrapper that invokes sub_8134558 with the fixed argument
 * set (226, 28, 30, 36); result and condition flags are whatever the callee
 * leaves behind. r4-r7 pass through untouched. */
Next sub_8134538(void) {
    R0 = 0xE2;
    R1 = 0x1C;
    R2 = 0x1E;
    R3 = 0x24;
    GUEST_CALL(sub_8134558);
    RETURN_TO_CALLER();
}
