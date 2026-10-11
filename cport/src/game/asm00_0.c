/*
 * asm00_0.c: readable functions of bn6f asm00_0.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── PlaySoundEffect ── */
/* PlaySoundEffect: 080005CC-080005D4 THUMB, bn6f asm00_0.s:34
 * Thin wrapper around the m4a sound engine: forwards the caller's
 * arguments unchanged to m4a_800061E and returns with r1-r7 preserved
 * (as the original's push/pop does). Flags are whatever the callee
 * leaves behind, since the pop does not touch them. */
Next PlaySoundEffect(void) {
    u32 r1 = R1, r2 = R2, r3 = R3, r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    GUEST_CALL(m4a_800061E);

    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── PlayMusic ── */
/* PlayMusic (0x080005D4): switch the background music to the given song.
 * r0 = song id. If the requested song is already the current one, nothing
 * happens; otherwise the id is recorded in the game state and the sound
 * engine is told to start it. Song 0x63 (the "music stopped" slot) is
 * handled by a special routine, everything else goes through the M4A player.
 * r1-r7 are saved around the call because the sound routines clobber them;
 * the song comparison flags survive on the "already playing" path. */
Next PlayMusic(void) {
    u32 song = R0;

    /* Save r1-r7 and the return address, exactly like the original's
     * push {r1-r7,lr}, so the sound routines can trash them freely. */
    u32 frame = SP - 32;
    SP = frame;
    ST32(frame + 0,  R1);
    ST32(frame + 4,  R2);
    ST32(frame + 8,  R3);
    ST32(frame + 12, R4);
    ST32(frame + 16, R5);
    ST32(frame + 20, R6);
    ST32(frame + 24, R7);
    ST32(frame + 28, LR);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 currentSong = LD8(gameState + oGameState_BGMusicIndicator);

    if (song == currentSong) {
        /* Nothing to do; the flags from the song comparison survive. */
        SET_FLAGS_CMP(song, currentSong);
    } else {
        ST8(gameState + oGameState_BGMusicIndicator, song);

        /* Both sound routines expect r7 to point at the game state. */
        R7 = gameState;
        if (song == 0x63) {
            GUEST_CALL(sound_dispatchTask_8000630);
        } else {
            GUEST_CALL(m4a_800061E);
        }
    }

    /* pop {r1-r7}: restore the caller's registers across the call
     * (the pc pop is handled by RETURN_TO_CALLER below). */
    R1 = LD32(frame + 0);
    R2 = LD32(frame + 4);
    R3 = LD32(frame + 8);
    R4 = LD32(frame + 12);
    R5 = LD32(frame + 16);
    R6 = LD32(frame + 20);
    R7 = LD32(frame + 24);
    SP = frame + 32;

    RETURN_TO_CALLER();
}

/* ── m4a_800061E ── */
/* m4a_800061E(r0, r2 = arguments passed straight through):
 * Thin wrapper around the m4a sound driver's request entry point.
 * Calls sound_8000808 with the song-table routine pointer (thumb bit set)
 * in r3 and the current r10 value as an extra argument in r1.
 * sound_8000808 clobbers r10, so r10 is saved on the stack across the call
 * and restored afterwards, exactly as the original does. */
Next m4a_800061E(void) {
    u32 savedR10Slot = SP -= 4;
    ST32(savedR10Slot, R10);

    R1 = R10;
    R3 = (A_m4a_SongNumStart | 1);
    GUEST_CALL(sound_8000808);

    R10 = LD32(savedR10Slot);
    SP += 4;

    RETURN_TO_CALLER();
}

/* ── sound_dispatchTask_8000630 ── */
/* sound_dispatchTask_8000630(r0 = arg0, r2 = arg2, r4-r7/r10 = context):
 * Tail-wrapper around the sound engine's dispatcher sound_8000808.
 * It passes the caller's arguments straight through, supplies the
 * sound task entry point sound_tickAllMusicPlayers_814EA58 as the callback (r3), and hands
 * the caller's r10 to the callee as its second argument (r1) while
 * keeping r10 itself intact across the call.
 * Flags and r10 are returned to the caller unchanged from the callee. */
Next sound_dispatchTask_8000630(void) {
    u32 savedR10 = R10;

    R1 = savedR10;                 /* second argument: caller's r10 */
    R3 = A_sound_tickAllMusicPlayers_814EA58 | 1;        /* callback: sound task routine (thumb) */
    GUEST_CALL(sound_8000808);

    R10 = savedR10;                /* r10 is preserved across the call */
    RETURN_TO_CALLER();
}

/* ── sound_dispatchSoundRequest_8000642 ── */
/* sound_dispatchSoundRequest_8000642(r0 = sound id, r1/r2 = parameters):
 * Dispatch a sound request. The handler routine for the given sound id is
 * looked up in the table at off_8000704 and passed, together with the shared
 * sound callback sub_814F9AC, to the dispatcher sound_8000808.
 *
 * On return: r3 = the incoming r10 (the original stashes r10 across the call
 * and hands it back in r3), r4-r7 and r10 are preserved, and c/v are whatever
 * sound_8000808 last left them as. */
Next sound_dispatchSoundRequest_8000642(void) {
    /* The original pushes r4-r7/r10 around the call; sound_8000808 may
     * clobber r7 and r10, so keep caller values in locals to restore them. */
    u32 caller_r4  = R4;
    u32 caller_r5  = R5;
    u32 caller_r6  = R6;
    u32 caller_r7  = R7;
    u32 caller_r10 = R10;

    /* Fetch the handler pointer for this sound id. */
    R7 = A_off_8000704;                 /* handler table base */
    R0 = LD32(R7 + (R0 << 2));          /* handler for sound id in r0 */

    /* sound_8000808 receives the handler in r0, the raw parameters in r1/r2,
     * the shared callback in r3, and keeps the table pointer in r7. */
    R3 = A_sub_814F9AC | 1;             /* Thumb entry of the callback */
    GUEST_CALL(sound_8000808);

    /* Hand the saved r10 back in r3 and restore everything the dispatcher
     * may have disturbed. These moves do not touch the flags, so c/v from
     * the call pass straight through to the caller. */
    R3  = caller_r10;
    R10 = caller_r10;
    R7  = caller_r7;
    R6  = caller_r6;
    R5  = caller_r5;
    R4  = caller_r4;
    RETURN_TO_CALLER();
}

/* ── sound_dispatchCueHandler_800068A ── */
/* sound_dispatchCueHandler_800068A(r0 = sound cue index, r1 = argument, r10 = context):
 * Look up the handler word for a sound cue in the cue table at off_8000704
 * and dispatch it through sound_8000808 together with the caller's context
 * (r10) and the shared sound callback battle_smashFx_SetParams_814E87C. sound_8000808 uses r10
 * as scratch, so this wrapper saves it around the call; the restored value
 * is handed back to the caller in both r10 and r2. */
Next sound_dispatchCueHandler_800068A(void) {
    u32 cueIndex = R0;
    u32 savedContext = R10;

    R0 = LD32(A_off_8000704 + cueIndex * 4);   /* handler for this cue */
    R2 = savedContext;                         /* context passed as a2  */
    R3 = A_battle_smashFx_SetParams_814E87C | 1;                    /* shared callback       */

    GUEST_CALL(sound_8000808);

    /* sound_8000808 clobbers r10; the original pops its saved copy back
     * and leaves the restored value in r2 as well. */
    R10 = savedContext;
    R2 = savedContext;

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

/* ── zeroFill_80007B2 ── */
/* zeroFill_80007B2: zero-fill the work memBlock (whose address is held at
 * dword_200A490) with 0x20C bytes of zeroes. Preserves r2/r3; the c and v
 * flags are whatever ZeroFillByWord leaves behind. */
Next zeroFill_80007B2(void) {
    R0 = A_dword_200A490;
    R1 = 0x20C; /* numWords */
    GUEST_CALL(ZeroFillByWord);
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

/* ── ZeroFillByByte ── */
/* ZeroFillByByte(r0 = destination, r1 = byte count): fill the given range
 * with zero bytes, one byte per iteration, walking backwards from the end.
 * r0-r2 and LR are preserved; the flags left behind are those of the final
 * loop-counter decrement (SUBS r1, #1), which callers may branch on. */
Next ZeroFillByByte(void) {
    u32 dest = R0;
    u32 count = R1;

    do {
        /* SUBS count, #1: decrement first, and leave the flags from this
         * subtraction behind (they are what the caller sees on exit). */
        SET_FLAGS_CMP(count, 1);
        count -= 1;
        ST8(dest + count, 0);
    } while (count != 0);

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

/* ── CopyBytes ── */
/* CopyBytes(r0 = source end, r1 = destination end, r2 = byte count):
 * Copies r2 bytes backwards, from src[count-1] down to src[0]. Copying in
 * decreasing address order makes this safe for overlapping regions where
 * dst > src (e.g. shifting a buffer's contents towards higher addresses).
 *
 * Outputs: r2 = 0, r3 = the last byte copied (i.e. src[0]).
 * Flags come from the final "subs r2, #1", which is the decrement that
 * produced the 0 that ended the loop — that decrement always started from
 * r2 == 1, so: Z=1, C=1 (no borrow), V=0. */
Next CopyBytes(void) {
    do {
        R2 = R2 - 1;
        R3 = LD8(R0 + R2);
        ST8(R1 + R2, R3);
    } while (R2 != 0);

    /* Equivalent to the last "subs r2, #1" (1 - 1 == 0). */
    SET_FLAGS_CMP(1, 1);
    RETURN_TO_CALLER();
}

/* ── CopyWords ── */
/* CpuSet control bit 26: treat the source as a single word to fill with. */
#define CPUSET_FIXED_SOURCE 0x4000000u

/* CopyWords(r0 = src, r1 = dest, r2 = size in bytes):
 * Word-copy memory via the BIOS CpuSet. The byte count is converted to a
 * word count and ORed with the fixed-source control word. r0-r3 and LR are
 * pushed around the call and popped afterwards; because the copy itself may
 * write over the stack area holding them, the popped values can differ from
 * the saved ones (they must be read back from the guest stack to match). */
Next CopyWords(void) {
    /* push {r0-r3,lr} */
    u32 saved = SP -= 20;
    ST32(saved + 0,  R0);
    ST32(saved + 4,  R1);
    ST32(saved + 8,  R2);
    ST32(saved + 12, R3);
    ST32(saved + 16, LR);

    /* lsr r2, r2, #2: CpuSet takes a unit count, not a byte count.
     * The shift sets C from bit 1 of the original size. */
    u32 word_count = R2 >> 2;
    cpu.c = (R2 >> 1) & 1;

    /* orr r2, r3 (r3 = 0x4000000 from the literal pool): sets N/Z, C kept */
    R2 = word_count | CPUSET_FIXED_SOURCE;
    R3 = CPUSET_FIXED_SOURCE;
    SET_FLAGS_NZ(R2);

    GUEST_CALL(SWI_CpuSet);   /* swi 5: CpuSet(src, dest, word_count | 0x4000000) */

    /* pop {r0-r3,pc}: read back whatever now sits in the save area, since
     * the copy may have overwritten it when dest overlaps the stack. */
    R0 = LD32(saved + 0);
    R1 = LD32(saved + 4);
    R2 = LD32(saved + 8);
    R3 = LD32(saved + 12);
    SP = saved + 20;
    RETURN_TO_CALLER();
}

/* ── CopyByEightWords ── */
/* CopyByEightWords(r0 = src, r1 = dest, r2 = size in bytes):
 * Block-copy memory through the BIOS CpuFastSet call in copy mode (mask 0),
 * which transfers the data in eight-word bursts -- hence the name.
 *
 * The original saves r0-r3 and its return address with a real push, and
 * restores them with a real pop {r0-r3,pc}. The pop reads the values back
 * from the stack, so a copy whose destination overlaps this stack frame
 * changes what the caller sees afterwards -- including the return target.
 * The push/pop must therefore go through guest memory, not local copies.
 *
 * Flags are left as the original's shift/ORR sequence leaves them:
 * NZ from the word count, C from the bit shifted out of "lsr r2, r2, #2". */
Next CopyByEightWords(void) {
    /* push {r0-r3,lr} */
    u32 frame = SP - 20;
    SP = frame;
    ST32(frame,      R0);
    ST32(frame +  4, R1);
    ST32(frame +  8, R2);
    ST32(frame + 12, R3);
    ST32(frame + 16, LR);

    /* CpuFastSet takes the transfer length in words: size / 4.
     * Mode = word count OR copy mask (0): copy words, not fill. */
    u32 words = R2 >> 2;
    SET_FLAGS_NZ(words);      /* ORRS r2, r3 with r3 = 0 */
    cpu.c = (R2 >> 1) & 1;    /* carry-out of the LSR by 2 */

    R2 = words;
    R3 = 0;                   /* .CopyFastCpuSetMask = 0 */
    GUEST_CALL(SWI_CpuFastSet);

    /* pop {r0-r3,pc} -- whatever the stack holds now. */
    R0 = LD32(frame);
    R1 = LD32(frame + 4);
    R2 = LD32(frame + 8);
    R3 = LD32(frame + 12);
    LR = LD32(frame + 16);
    SP = frame + 20;

    /* The original returns through the popped PC, so LR carries the
     * (possibly clobbered) return address into RETURN_TO_CALLER(). */
    RETURN_TO_CALLER();
}

/* ── HalfwordFill ── */
/* HalfwordFill(r0 = dest, r1 = byte count, r2 = halfword value):
 * fill r1 bytes at dest with the halfword in r2 by handing CpuSet a
 * halfword-mode fill (0x1000000 | count/2). CpuSet reads the fill value
 * through a source pointer, so the value is parked on the stack for the
 * duration of the call.
 * r0-r3 are restored to their incoming values (the original pushes/pops
 * them); the returned flags are those of the Thumb "mov r1, r0" (LSLS
 * r1, r0, #0), i.e. the same NZCV as computing dest + 0. */

Next HalfwordFill(void) {
    u32 dest = R0;
    u32 byteCount = R1;
    u32 value = R2;
    u32 savedR3 = R3;

    u32 mode = 0x1000000u | (byteCount >> 1); /* halfword transfer, unit count = bytes/2 */

    u32 slot = SP -= 4;
    ST32(slot, value);

    R0 = slot;   /* src: pointer to the fill value */
    R1 = dest;
    R2 = mode;
    R3 = value;
    GUEST_CALL(SWI_CpuSet);

    SP = slot + 4;

    /* pop {r0-r3}: everything back as it arrived */
    R0 = dest;
    R1 = byteCount;
    R2 = value;
    R3 = savedR3;

    SET_FLAGS_ADD(dest, 0); /* flags of "mov r1, r0": n/z from dest, c/v cleared */

    RETURN_TO_CALLER();
}

/* ── FillByEightWords ── */
/* FillByEightWords(r0 = destination address, r1 = length in bytes, r2 = fill word):
 * Fills memory via the BIOS CpuFastSet in fixed-source (fill) mode: the word in r2
 * is written to r0 for r1/4 words. The fill word is passed through a slot on the
 * guest stack because CpuFastSet takes its source by pointer. r0-r3 are preserved. */
Next FillByEightWords(void) {
    u32 dest = R0;
    u32 length = R1;
    u32 fill = R2;
    u32 savedR3 = R3;

    u32 fillOnStack = SP -= 4;
    ST32(fillOnStack, fill);

    R0 = fillOnStack;                    /* src: pointer to the fill word */
    R1 = dest;                           /* dst: address to fill */
    R2 = 0x1000000 | (length >> 2);      /* fixed-source mode, count in words */
    GUEST_CALL(SWI_CpuFastSet);

    SP += 4;

    R0 = dest;
    R1 = length;
    R2 = fill;
    R3 = savedR3;
    RETURN_TO_CALLER();
}

/* ── clearWord_e200AC1C ── */
/* clearWord_e200AC1C: clear the word at dword_200AC1C (set it to 0). */
Next clearWord_e200AC1C(void) {
    ST32(A_dword_200AC1C, 0);
    RETURN_TO_CALLER();
}

/* ── memory_bcdConvert ── */
/* memory_bcdConvert: convert the value in r0 to packed BCD (8 decimal digits,
 * 4 bits each, most significant digit in the top nibble).
 * If the CMP against 99999999 says the value is not below the limit (N clear),
 * the sentinel 0x99999999 is returned instead and r2/r3 are left untouched.
 * On success r2 holds the packed BCD result, r1 the last divisor used (1) and
 * r3 the wound-down loop counter (0). Flags report on the returned value.
 * r4/r7 are saved and restored. */
#define BCD_MAX_VALUE 99999999u   /* = 0x05F5E0FF, largest convertible value */
#define BCD_SENTINEL  0x99999999u /* "out of range" marker, all BCD 9s */

Next memory_bcdConvert(void) {
    u32 value = R0;

    /* The original branches on the N flag of value - limit, so replicate the
     * subtraction (with wraparound) rather than doing a plain signed compare. */
    u32 diff = value - BCD_MAX_VALUE;
    SET_FLAGS_CMP(value, BCD_MAX_VALUE);

    if ((s32)diff >= 0) {
        /* Out of range: return the sentinel, leave r2/r3 as the caller set them. */
        R0 = BCD_SENTINEL;
        R1 = BCD_MAX_VALUE;
        RETURN_TO_CALLER();
    }

    u32 result = 0;
    u32 table = A_off_8000C34; /* {10000000, 1000000, ..., 10, 1} */
    u32 divisor = 0;

    R3 = 8;
    do {
        result <<= 4;
        divisor = LD32(table);
        table += 4;

        u32 digit = 0;
        while ((s32)value >= (s32)divisor) {
            value -= divisor;
            digit++;
        }
        result |= digit;
    } while (--R3 != 0);

    R0 = result;
    R1 = divisor; /* always 1 after the last pass */
    R2 = result;
    /* mov r0, r2 is encoded as ADDS r0, r2, #0 in Thumb, so it sets N/Z from
     * the result and clears C/V. */
    SET_FLAGS_ADD(result, 0);
    RETURN_TO_CALLER();
}

/* ── countHexDigits_8000C5C ── */
/* countHexDigits_8000C5C(r0 = value):
 * Count how many hexadecimal digits (4-bit nibbles) the value needs,
 * where 0 counts as one digit. Returns the count in r0 (and r1);
 * flags are set from the result, as the original's final
 * "mov r0, r1" (encoded as adds r0, r1, #0) does. */
Next countHexDigits_8000C5C(void) {
    u32 value = R0;

    u32 digits;
    if (value == 0) {
        digits = 1;
    } else {
        digits = 0;
        while (value != 0) {
            digits++;
            value >>= 4;
        }
    }

    R1 = digits;
    R0 = digits;
    SET_FLAGS_ADD(digits, 0);
    RETURN_TO_CALLER();
}

/* ── ShuffleByteList_PrimaryRNG ── */
/* ShuffleByteList_PrimaryRNG(r0 = byte array, r1 = array length, r2 = swap count):
 * Shuffle the byte list by performing r2 random swaps. Each swap draws two
 * independent random numbers and reduces each to an array index with the BIOS
 * Div call (rand % length, remainder left in r1). r0-r2 come back unchanged,
 * r3 holds the first index of the last swap, and r4-r7 are preserved. */
Next ShuffleByteList_PrimaryRNG(void) {
    u32 array = R0;
    u32 count = R1;
    u32 swapsLeft = R2;

    do {
        /* First index: draw a random number, then divide it by the array
         * length with BIOS Div (svc 6); the remainder in r1 is the index. */
        R0 = array;
        R1 = count;
        R2 = swapsLeft;
        GUEST_CALL(GetPositiveSignedRNG);
        R1 = count;                 /* Div divisor (the RNG call may clobber r1) */
        swi_call(6);                /* Div: R0 = quotient, R1 = remainder */
        u32 first = R1;
        R3 = first;

        /* Second index, drawn the same way. R0 still holds the first
         * division's quotient here, exactly as in the original. */
        R2 = swapsLeft;
        GUEST_CALL(GetPositiveSignedRNG);
        R1 = count;
        swi_call(6);
        u32 second = R1;

        /* Swap the two selected bytes. */
        u8 firstByte = LD8(array + first);
        u8 secondByte = LD8(array + second);
        ST8(array + first, secondByte);
        ST8(array + second, firstByte);

        /* The original pops r0-r3 back from its stack around here; r1/r3 are
         * already correct, so restore the remaining two. */
        R0 = array;
        R2 = swapsLeft;
        R3 = first;

        /* Count this swap down. The flags come from this decrement (the
         * original's SUBS r2, #1); the loop ends once it reaches zero. */
        SET_FLAGS_CMP(swapsLeft, 1);
        swapsLeft--;
    } while (swapsLeft != 0);

    R0 = array;
    R1 = count;
    R2 = 0;                         /* the final decrement left r2 at zero */
    RETURN_TO_CALLER();
}

/* ── memory_bcd_8000D84 ── */
/* BCD digits of a broken-down time, packed into one word:
 * byte 3 = hours, byte 2 = minutes, byte 1 = seconds, byte 0 = hundredths. */
struct PackedBcd {
    u32 packed;
    u32 r2_out;   /* value the original leaves in r2 (minutes << 16) */
    u32 r3_out;   /* value the original leaves in r3 (seconds << 8)  */
};

struct Hmsf {
    u32 hours;
    u32 minutes;
    u32 seconds;
    u32 frames;
};

/* Largest play-time frame count shown normally; anything above renders as 99:59:59.99. */
#define MAX_PLAYTIME_FRAMES 0x1499727u

/* register form: calls clock_breakPlayTimeIntoHMSF_8000DE0 outside this file, which has no typed C interface yet */
static struct Hmsf clock_break_playtime_into_hmsf(u32 frames) {
    R0 = frames;
    GUEST_CALL(clock_breakPlayTimeIntoHMSF_8000DE0);
    struct Hmsf t = { R0, R1, R2, R3 };
    return t;
}

/* register form: calls memory_bcdConvert outside this file, which has no typed C interface yet */
static u32 memory_bcd_convert(u32 value) {
    R0 = value;
    GUEST_CALL(memory_bcdConvert);
    return R0;
}

/* register form: BIOS division (swi 6 Div, signed; quotient in r0) */
static s32 bios_div(s32 numerator, s32 denominator) {
    R0 = (u32)numerator;
    R1 = (u32)denominator;
    swi_call(6);
    return (s32)R0;
}

/* Play-time frames -> packed BCD for the save screen's time display.
 * Anything past the cap (~100h of frames) renders as the sentinel 99:59:59.99. */
static struct PackedBcd playtime_to_bcd(u32 frames, u32 r2_in, u32 r3_in) {
    struct PackedBcd out;

    if ((s32)frames > (s32)MAX_PLAYTIME_FRAMES) {
        out.packed = 0x99595999u;   /* BCD 99:59:59.99 */
        out.r2_out = r2_in;
        out.r3_out = r3_in;
        return out;
    }

    struct Hmsf t = clock_break_playtime_into_hmsf(frames);

    u32 hours_bcd   = memory_bcd_convert(t.hours);
    u32 minutes_bcd = memory_bcd_convert(t.minutes);
    u32 seconds_bcd = memory_bcd_convert(t.seconds);
    /* frames (1/60s) -> hundredths of a second */
    u32 centis_bcd  = memory_bcd_convert((u32)bios_div((s32)(t.frames * 100u), 60));

    out.packed = centis_bcd
               + (hours_bcd << 24)
               + (minutes_bcd << 16)
               + (seconds_bcd << 8);
    out.r2_out = minutes_bcd << 16;
    out.r3_out = seconds_bcd << 8;
    return out;
}

/* memory_bcd_8000D84(r0 = play time in frames) -> r0 packed BCD; r2/r3 carry
 * shifted BCD minutes/seconds on the normal path and are untouched on overflow. */
Next memory_bcd_8000D84(void) {
    struct PackedBcd res = playtime_to_bcd(R0, R2, R3);
    R0 = res.packed;
    R2 = res.r2_out;
    R3 = res.r3_out;
    RETURN_TO_CALLER();
}

/* ── clock_breakPlayTimeIntoHMSF_8000DE0 ── */
/* Frame counts used by the in-game clock: the play-time counter is measured
 * in frames at 60 fps, so one second is 60 frames, one minute 3600 and
 * one hour 216000. */
#define FRAMES_PER_SECOND   0x3Cu     /*  60 frames */
#define FRAMES_PER_MINUTE   0xE10u    /*  60 * 60 frames */
#define FRAMES_PER_HOUR     0x34BC0u  /*  60 * 60 * 60 frames */

/* clock_breakPlayTimeIntoHMSF_8000DE0(r0 = frame counter):
 * Breaks the play-time frame counter down into clock units with three
 * chained divisions (BIOS Div, svc 6, quotient in r0 / remainder in r1):
 *   counter / 216000                -> hours
 *   (counter % 216000) / 3600       -> minutes
 *   (counter % 216000) % 3600 / 60  -> seconds
 *   ... % 60                        -> leftover frames
 * The r2/r3 values the caller passes in are irrelevant; Div only reads
 * r0/r1 and the function overwrites r2/r3 with the last division's
 * quotient and remainder. */
Next clock_breakPlayTimeIntoHMSF_8000DE0(void) {
    u32 totalFrames = R0;

    R0 = totalFrames;
    R1 = FRAMES_PER_HOUR;
    swi_call(6);
    u32 hours = R0;
    u32 rem   = R1;

    R0 = rem;
    R1 = FRAMES_PER_MINUTE;
    swi_call(6);
    u32 minutes = R0;
    rem = R1;

    R0 = rem;
    R1 = FRAMES_PER_SECOND;
    swi_call(6);
    u32 seconds = R0;
    u32 frames  = R1;

    R0 = hours;
    R1 = minutes;
    R2 = seconds;
    R3 = frames;
    RETURN_TO_CALLER();
}

/* ── stub_returnZero_8000E30 ── */
/* stub_returnZero_8000E30: a stub that simply returns 0 in r0.
 * The original saves r4-r7, clears r4, moves it to r0, and immediately
 * restores r4-r7, so the only observable effect is R0 = 0. */
Next stub_returnZero_8000E30(void) {
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── pickWeightedIndex_8000E3A ── */
/* Weighted random pick from a table of u8 weights.
 *
 * Entry registers: r0 = weight table, r1 = entry count, r10 = toolkit.
 * The routine first draws the RNG once to size a "stir" (high two bits of
 * that draw plus the low two bits of the current frame counter), burns that
 * many more draws, and keeps the last draw (masked to 16 bits) as the roll.
 * It then sums the whole table, takes the roll modulo the total via BIOS
 * Div, adds one to get a target in [1, total], and walks the table from
 * index 0 until the running weight reaches the target. If the walk runs
 * past the end of the table it falls back to index 0.
 *
 * r0 and r2 both receive the chosen index. r3 receives the last table byte
 * the walk loaded (on the fallback path that is the final entry, not the
 * returned index's byte). r4-r7 and lr are left as the caller set them.
 */
typedef struct {
    u32 index;        /* chosen entry -> r0 and r2 */
    u32 last_weight;  /* last table byte the walk loaded -> r3 */
} weighted_pick;

/* register form: calls GetRNG outside this file, which has no typed C interface yet */
static u32 draw_rng(void) {
    GUEST_CALL(GetRNG);
    return R0;
}

/* register form: BIOS Div (swi 6) — must run with the raw guest registers so
 * the hardware/BIOS side effects match; only the remainder (r1) is used here */
static s32 bios_div_remainder(u32 numerator, u32 denominator) {
    R0 = numerator;
    R1 = denominator;
    swi_call(6);
    return (s32)R1;
}

static weighted_pick pick_weighted_index(u32 table, s32 count, u32 toolkit)
{
    /* Stir the RNG: one draw to size the stir, then that many more. */
    u32 rng = draw_rng();
    u32 frame = LD16(LD32(toolkit + oToolkit_CurFramePtr));
    s32 extra_draws = (s32)(rng >> 30) + (s32)(frame & 3);

    do {
        rng = draw_rng();
    } while (--extra_draws >= 0);
    rng &= 0xFFFF;

    /* Total weight over all entries. The original sums back to front and
     * reads one entry even for a zero-count table (table[-1]), so a
     * do-while keeps that exact behaviour. */
    s32 total = 0;
    s32 i = count - 1;
    do {
        total += LD8(table + i);
    } while (--i >= 0);

    /* Target point in [1, total]; BIOS Div leaves the remainder in r1. */
    s32 target = bios_div_remainder(rng, (u32)total) + 1;

    /* First index whose running weight reaches the target; if the walk
     * passes the end of the table, fall back to index 0. The last byte
     * loaded is kept because the original leaves it behind in r3. */
    weighted_pick res;
    s32 run = 0;
    s32 picked = 0;
    for (;;) {
        res.last_weight = LD8(table + picked);
        run += (s32)res.last_weight;
        if (run >= target)
            break;
        picked++;
        if (picked >= count) {
            picked = 0;
            break;
        }
    }
    res.index = (u32)picked;
    return res;
}

/* pickWeightedIndex_8000E3A(r0 = weight table, r1 = entry count, r10 = toolkit)
 * -> r0 = chosen index, r2 = chosen index, r3 = last weight byte read */
Next pickWeightedIndex_8000E3A(void) {
    weighted_pick pick = pick_weighted_index(R0, (s32)R1, R10);
    R0 = pick.index;
    R2 = pick.index;
    R3 = pick.last_weight;
    RETURN_TO_CALLER();
}

/* ── GetTitleScreenIconCount ── */
/* Title-screen icon bitmask thresholds */
#define TITLE_ICON_E00  0x0E00
#define TITLE_ICON_370  0x0370
#define TITLE_ICON_340  0x0340
#define TITLE_ICON_3BD  0x03BD

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls countEventFlagChips_81207F8 outside this file, which has no typed C interface yet */
static u32 get_progress_stat(u32 index) {
    R0 = index;
    GUEST_CALL(countEventFlagChips_81207F8);
    return R0;
}

/* register form: calls TestEventFlagRangeFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_range_set(u16 flag, u32 count) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    R2 = count;
    GUEST_CALL(TestEventFlagRangeFromImmediate);
    return !ZERO_FLAG();
}

typedef struct { u32 count; u32 bitmask; } TitleScreenIcons;

/* Tally the icons the player has earned for the title screen.
 * Returns the count and a bitmask indicating which of 9 possible slots are filled. */
static TitleScreenIcons count_title_screen_icons(void) {
    u32 count = 0;
    u32 mask = 0;

    /* Flag E00: special side-story event */
    if (event_flag_set(TITLE_ICON_E00)) { count++; mask |= 0x80; }

    /* Nav progress stat 0 >= 200 (full map explored) */
    if (get_progress_stat(0) >= 200) { count++; mask |= 0x40; }

    /* Nav progress stat 1 >= 39 (PET level threshold) */
    if (get_progress_stat(1) >= 39) { count++; mask |= 0x20; }

    /* Nav progress stat 2 >= 5 (chip count threshold) */
    if (get_progress_stat(2) >= 5) { count++; mask |= 0x10; }

    /* Nav progress stat 5 >= 15 (CP count threshold) */
    if (get_progress_stat(5) >= 15) { count++; mask |= 0x08; }

    /* Nav progress stat 4 >= 29 (PET level threshold) */
    if (get_progress_stat(4) >= 29) { count++; mask |= 0x04; }

    /* Flag 370: completed event (icon bit 1) */
    if (event_flag_set(TITLE_ICON_370)) { count++; mask |= 0x02; }

    /* Flag 340: completed event (icon bit 8) */
    if (event_flag_set(TITLE_ICON_340)) { count++; mask |= 0x100; }

    /* Flag range 3BD, 5 events: all story flags cleared (icon bit 9) */
    if (event_flag_range_set(TITLE_ICON_3BD, 5)) { count++; mask |= 0x200; }

    TitleScreenIcons result = { count, mask };
    return result;
}

/* GetTitleScreenIconCount() -> r0 = icon count, r1 = icon bitmask */
Next GetTitleScreenIconCount(void) {
    TitleScreenIcons icons = count_title_screen_icons();
    R0 = icons.count;
    R1 = icons.bitmask;
    RETURN_TO_CALLER();
}

/* ── updatePlayerGameState_800107A ── */
/* updatePlayerGameState_800107A: copy the overworld player object's position
 * (and facing direction) into the game state, e.g. to record where the player
 * stands before a warp. r10 = toolkit; returns r3 = game state pointer. */
Next updatePlayerGameState_800107A(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    ST32(gameState + oGameState_PlayerX, LD32(player + 0x1C));
    ST32(gameState + oGameState_PlayerY, LD32(player + 0x20));
    ST32(gameState + oGameState_PlayerZ, LD32(player + 0x24));
    ST32(gameState + oGameState_FacingDirectionAfterWarp, LD8(player + 0x10));

    R3 = gameState;
    RETURN_TO_CALLER();
}

/* ── GetCurPETNavi ── */
/* GetCurPETNavi: () -> u8
 * Returns the navi currently active on the PET. As a side effect, r3 is
 * left holding the game state pointer, which some callers make use of. */
Next GetCurPETNavi(void) {
    u32 *gameState = (u32 *)LD32(R10 + oToolkit_GameStatePtr);

    R3 = (u32)gameState;
    R0 = LD8((u32)gameState + 1); /* + oGameState_CurPETNavi */

    RETURN_TO_CALLER();
}

/* ── SetCurPETNavi ── */
/* SetCurPETNavi(r0 = navi index): store the selected PET navi in the game
 * state. r3 returns the game state pointer (the last value r3 held). */
Next SetCurPETNavi(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    /* oGameState_CurPETNavi is at offset 1 */
    ST8(gameState + 1, R0);

    R3 = gameState;
    RETURN_TO_CALLER();
}

/* ── writeCurPETNaviToS2001c04_Unk07_80010c6 ── */
/* writeCurPETNaviToS2001c04_Unk07_80010c6:
 * Store the current PET navi ID into the S2001c04 struct's Unk_07 byte.
 * Outputs: r0 = navi ID (as returned by GetCurPETNavi), r3 = S2001c04 pointer. */
Next writeCurPETNaviToS2001c04_Unk07_80010c6(void) {
    GUEST_CALL(GetCurPETNavi); /* -> r0 = current PET navi ID */

    u32 s2001c04 = LD32(R10 + oToolkit_S2001c04_Ptr);
    R3 = s2001c04;
    ST8(s2001c04 + 7 /* oS2001c04_Unk_07 */, R0);

    RETURN_TO_CALLER();
}

/* ── GetMaxAndCurHPForCurPETNavi_80010D4 ── */
/* GetMaxAndCurHPForCurPETNavi_80010D4(r0 = which_navi) -> r0 = current HP,
 * r1 = max HP of the navi currently equipped in the PET.
 * Flags (c, v) come from the final "mov r1, r6" (encoded as ADDS r1, r6, #0),
 * so c = v = 0 and nz reflect max HP. */
Next GetMaxAndCurHPForCurPETNavi_80010D4(void) {
    u32 which_navi = R0;

    R0 = which_navi;
    R1 = oNaviStats_MaxHP;
    GUEST_CALL(GetCurPETNaviStatsHword);   /* r0 = max HP */
    u32 maxHP = R0;

    R0 = which_navi;
    R1 = oNaviStats_CurHP;
    GUEST_CALL(GetCurPETNaviStatsHword);   /* r0 = current HP */

    R1 = maxHP;
    SET_FLAGS_ADD(maxHP, 0);               /* flags of "adds r1, r6, #0" */
    RETURN_TO_CALLER();
}

/* ── scriptListRun_8001158 ── */
/* scriptListRun_8001158(r0 = list ptr):
 * Walk an array of {cmd_id, arg} pairs (terminated by cmd_id == 0) and dispatch
 * each entry to sub_802FE28(cmd_id, arg, 0, 0). */
Next scriptListRun_8001158(void) {
    u32 entry = R0;
    u32 cmd = LD32(entry);
    /* tst r0, r0 — sets NZ from cmd, leaves CV untouched. */
    SET_FLAGS_NZ(cmd);

    while (cmd != 0) {
        /* Dispatch one {cmd, arg} pair. */
        R0 = cmd;
        R1 = LD32(entry + 4);
        R2 = 0;
        R3 = 0;
        GUEST_CALL(sub_802FE28);

        /* add r7, #8 (ADDS) — flags reflect the pointer advance. */
        entry += 8;
        SET_FLAGS_ADD(entry - 8, 8);

        /* Next iteration: load cmd, tst it. */
        cmd = LD32(entry);
        SET_FLAGS_NZ(cmd);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8001172 ── */
/* sub_8001172(r0 = value, r10 = toolkit):
 * Store the byte value into the game state at offset 9 (field after
 * oGameState_Unk_15's group; no named macro exists for this offset),
 * with the game state pointer read from the toolkit struct.
 * Leaves the game state pointer in r3. */
Next sub_8001172(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    ST8(gameState + 9, R0);

    R3 = gameState;
    RETURN_TO_CALLER();
}

/* ── calcAngle_800117C ── */
/* calcAngle_800117C(r0, r1 = vector components as fixed-point values whose
 * meaningful part is the upper 16 bits):
 * computes the angle of that vector via the BIOS ArcTan2 call (swi 0xA) and
 * returns it in r0 as a single byte (0..255 over a full turn). The two
 * components are swapped on the way into the BIOS call (r0 gets r1's upper
 * half, r1 gets r0's), and r2 ends up holding r0's upper half, exactly as
 * the original leaves it. r1 is scratch afterwards. */
Next calcAngle_800117C(void) {
    R2 = (u32)((s32)R0 >> 16);
    R0 = (u32)((s32)R1 >> 16);
    R1 = R2;

    swi_call(0xA);  /* BIOS ArcTan2 -> r0 = angle (0x8000 = half turn) */

    R0 >>= 8;       /* rescale the 16-bit angle to a 0..255 byte */
    RETURN_TO_CALLER();
}

/* ── battle_orStateFlags5c_8001382 ── */
/* battle_orStateFlags5c_8001382(r0 = flag bits): set the given bits in the current battle
 * state's word at oBattleState_Unk_5c.
 * Leaves r1 = battle state pointer and r2 = updated word. */
Next battle_orStateFlags5c_8001382(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);

    R1 = battleState;
    R2 = LD32(battleState + oBattleState_Unk_5c) | R0;
    ST32(battleState + oBattleState_Unk_5c, R2);

    RETURN_TO_CALLER();
}

/* ── battle_getStateUnk5c_800139A ── */
/* battle_getStateUnk5c_800139A: return the value of battleState->Unk_5c.
 * (r10 holds the toolkit pointer on entry.) */
Next battle_getStateUnk5c_800139A(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD32(battleState + oBattleState_Unk_5c);
    RETURN_TO_CALLER();
}

/* ── battle_SetStateUnk5c_80013A2 ── */
/* battle_SetStateUnk5c_80013A2(r0 = value, r10 = toolkit):
 * store a value into the current battle state (oBattleState_Unk_5c).
 * Returns the battle state pointer in r1. */
Next battle_SetStateUnk5c_80013A2(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    ST32(battle + oBattleState_Unk_5c, R0);
    R1 = battle;
    RETURN_TO_CALLER();
}

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

/* ── SeedRNG ── */
/* SeedRNG: reset the primary RNG seed to its fixed initial value. */
Next SeedRNG(void) {
    ST32(A_ePrimaryRngSeed, 0xA338244F);
    RETURN_TO_CALLER();
}

/* ── GetRNG ── */
/* GetRNG (0x0800151C): advance the primary RNG seed.
 * new seed = ((seed << 1) + (seed >> 31) + 1) ^ 0x873CA9E5, written back to
 * ePrimaryRngSeed. Leaves r0 = new seed, r1 = XOR constant, r2 = seed << 1,
 * r3 = seed >> 31; r7 is preserved (the original only borrows it as the
 * seed pointer behind its push/pop). Flags: n/z from the final EOR,
 * c/v from the "+1" add (the EOR leaves them untouched). */
#define RNG_SEED_XOR 0x873CA9E5u

Next GetRNG(void) {
    u32 seedPtr = A_ePrimaryRngSeed;

    u32 seed = LD32(seedPtr);             /* ldr r0, [r7] */

    u32 doubled = seed << 1;              /* lsl r2, r0, #1 */
    u32 topBit  = seed >> 31;             /* lsr r3, r0, #31 */
    u32 sum     = doubled + topBit;       /* add r0, r2, r3 (flags overwritten below) */
    u32 bumped  = sum + 1;                /* add r0, #1 */
    SET_FLAGS_ADD(sum, 1);

    u32 next = bumped ^ RNG_SEED_XOR;     /* eor r0, r1 */
    SET_FLAGS_NZ(next);

    ST32(seedPtr, next);                  /* str r0, [r7] */

    R0 = next;
    R1 = RNG_SEED_XOR;
    R2 = doubled;
    R3 = topBit;
    RETURN_TO_CALLER();
}

#undef RNG_SEED_XOR

/* ── GetPositiveSignedRNG ── */
/* GetPositiveSignedRNG: advance the primary RNG seed (ePrimaryRngSeed).
 *
 * The seed is rotated left by one bit, incremented by one, XORed with a
 * fixed constant and stored back.  The returned value is the new seed with
 * its sign bit cleared, i.e. always non-negative (the LSL #1 / LSR #1 pair
 * simply clears bit 31 and drops the old bit 31 into the carry flag — which
 * is bit 0 of the shifted operand, hence always 0 here).
 *
 * Outputs: r0 = new seed with bit 31 cleared
 *          r2 = old seed << 1, r3 = old seed >> 31 (rotation temporaries)
 *          c  = 0 (bit shifted out of the final LSR #1 is bit 0 of seed<<1)
 *          v  = 0 (cleared by the final LSR #1)
 * Clobbers: nz, r1
 * Preserves: r7 (pushed/popped by the original) */
Next GetPositiveSignedRNG(void) {
    u32 seed = LD32(A_ePrimaryRngSeed);

    /* rotate the seed left by one bit, then increment it */
    u32 rotated = (seed << 1) | (seed >> 31);
    rotated = rotated + 1;

    u32 newSeed = rotated ^ 0x873CA9E5;
    ST32(A_ePrimaryRngSeed, newSeed);

    R2 = seed << 1;
    R3 = seed >> 31;

    /* LSL #1 followed by LSR #1: clear the sign bit (the LSR's shifted-out
     * bit — and therefore the carry flag — is bit 0 of seed<<1, i.e. 0) */
    u32 result = (newSeed << 1) >> 1;
    R0 = result;
    SET_FLAGS_NZ(result);
    cpu.c = 0;
    cpu.v = 0;

    RETURN_TO_CALLER();
}

/* ── GetRNGSecondary ── */
/* GetRNGSecondary: advance the secondary RNG seed (eSecondaryRngSeed) by one step:
 *   seed = ((seed << 1) + (seed >> 31) + 1) ^ 0x873CA9E5
 * The new seed is left in r0; r1-r3 hold intermediates from the original's
 * register allocation. r7 (and lr) are preserved. */
Next GetRNGSecondary(void) {
    u32 seed = LD32(A_eSecondaryRngSeed);
    u32 doubled = seed << 1;   /* lsl r2, r0, #1 */
    u32 carry   = seed >> 31;  /* lsr r3, r0, #31 */
    u32 next    = doubled + carry;

    SET_FLAGS_ADD(next, 1);    /* adds r0, #1 — sets c/v */
    next = next + 1;
    next ^= 0x873CA9E5;        /* eor r0, r1 — sets n/z, leaves c/v */
    SET_FLAGS_NZ(next);

    ST32(A_eSecondaryRngSeed, next);

    R0 = next;
    R1 = 0x873CA9E5;
    R2 = doubled;
    R3 = carry;
    RETURN_TO_CALLER();
}

/* ── GetPositiveSignedRNGSecondary ── */
/* GetPositiveSignedRNGSecondary: advance the secondary RNG seed and return it
 * with the sign bit cleared, so callers always get a non-negative value.
 *   r0 = new positive seed
 *   r2 = old seed << 1, r3 = old seed >> 31 (intermediates kept for callers)
 *   r1 is scratch; r7 is preserved (the original saves/restores it).
 */
Next GetPositiveSignedRNGSecondary(void) {
    u32 *seedPtr = (u32 *)A_eSecondaryRngSeed;
    u32 seed = *seedPtr;

    /* seed = ((seed * 2) + carry-out) + 1, then XOR the RNG constant */
    R2 = seed << 1;
    R3 = seed >> 31;
    seed = (R2 + R3) + 1;
    seed ^= 0x873CA9E5;
    *seedPtr = seed;

    /* Clear the top bit so the result is a positive signed value */
    R0 = (seed << 1) >> 1;

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

/* ── copyToVRAMAndClear_iBGTileIdBlocks_Ptr ── */
/* copyToVRAMAndClear_iBGTileIdBlocks_Ptr: 080015D0-080015FC THUMB, bn6f asm00_0.s:2851
 * inputs:  r3 r10 lr   outputs: -   clobbers: nz c v r0 r1 r2 lr   preserved: r3
 *
 * Uploads the toolkit's iBG tile-id block table to the BG tile-id area of VRAM
 * (0x0600E000, 0x2000 bytes), then resets the in-memory table to 0x2FF
 * ("no tile" entries) so the next frame starts from a clean state.
 */
Next copyToVRAMAndClear_iBGTileIdBlocks_Ptr(void) {
    u32 *tileIdBlocks = (u32 *)LD32(R10 + oToolkit_iBGTileIdBlocks_Ptr);

    /* Copy the current tile-id block table into VRAM. */
    R0 = (u32)tileIdBlocks;
    R1 = A_vBGTileIds;      /* 0x0600E000 */
    R2 = 0x2000;
    GUEST_CALL(CopyByEightWords);

    /* Clear the in-memory table, marking every entry as 0x2FF. */
    R0 = (u32)tileIdBlocks;
    R1 = 0x800;
    R2 = 0x2FF02FF;
    GUEST_CALL(FillByEightWords);

    RETURN_TO_CALLER();
}

/* ── toolkit_applyOverlayPalette_80015FC ── */
/* toolkit_applyOverlayPalette_80015FC: copies an 8-byte palette entry from the ROM table at
 * byte_8001618 into the toolkit render-info slot (offset +4). The
 * entry index is r0; r10 holds the toolkit pointer (the output goes
 * into the render-info write area). */
static void apply_overlay_palette(u32 toolkit, u32 entry_index,
                                  u32 *out_render_info_post) {
    u32 entry_addr = A_byte_8001618 + (entry_index << 3);
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr) + 4;
    ST32(render_info,     LD32(entry_addr));
    ST32(render_info + 4, LD32(entry_addr + 4));
    *out_render_info_post = render_info;
}

Next toolkit_applyOverlayPalette_80015FC(void) {
    u32 render_info_post;
    apply_overlay_palette(R10, R0, &render_info_post);
    R2 = render_info_post;
    R3 = R10;
    SET_FLAGS_ADD(render_info_post - 4, 4);
    RETURN_TO_CALLER();
}

/* ── map_lookupMapIdByGroupAndNumber_8001708 ── */
/* map_lookupMapIdByGroupAndNumber_8001708(R0 = map group, R1 = map number):
 * Selects the real-world group table for groups below INTERNET_MAP_GROUP_START
 * and the internet table for the rest (subtracting the base so the index fits),
 * then loads the byte for (group, number) and hands it to toolkit_applyOverlayPalette_80015FC.
 * Flags and r2/r3 on return are whatever toolkit_applyOverlayPalette_80015FC leaves. */
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

    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);
    RETURN_TO_CALLER();
}

/* ── render_flushDisplayRegisters_800172C ── */
/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void call_copy_words(u32 src, u32 dest, u32 count) {
    R0 = src;
    R1 = dest;
    R2 = count;
    GUEST_CALL(CopyWords);
}

/* register form: calls CopyHalfwords outside this file, which has no typed C interface yet */
static void call_copy_halfwords(u32 src, u32 dest, u32 count) {
    R0 = src;
    R1 = dest;
    R2 = count;
    GUEST_CALL(CopyHalfwords);
}

/* Copy display-registers data from the toolkit's cached tables into hardware I/O.
 * Returns the render_info pointer so the adapter can reproduce the ADDS flag. */
static u32 render_display_registers(u32 tk) {
    /* RenderInfo holds the cached DISPCNT, BG0CTL, mosaic, etc. */
    u32 ri = LD32(tk + oToolkit_RenderInfoPtr);

    /* Mosaic size (halfword at RenderInfo+2) -> MOSAIC register */
    ST16(A_MosaicSize, LD16(ri + oRenderInfo_Unk_02));

    /* BG0 control block: 0x38 bytes starting at RenderInfo+4 */
    call_copy_words(ri + 4, A_BG0Control, 0x38);

    /* Window0 horizontal dimensions: 12 halfwords from toolkit's window table */
    call_copy_halfwords(LD32(tk + oToolkit_Unk200f3a0_Ptr),
                        A_Window0HorizontalDimensions, 0xc);

    /* Color-special-effects selection: word + halfword fade coefficient */
    u32 fx = LD32(tk + oToolkit_Unk2009740_Ptr);
    ST32(A_ColorSpecialEffectsSelection,     LD32(fx));
    ST16(A_ColorSpecialEffectsSelection + 4, LD16(fx + 4));

    /* DISPCNT low halfword from the render-info header */
    ST16(A_LCDControl, LD16(ri));

    return ri;
}

/* render_flushDisplayRegisters_800172C(r10 = Toolkit) -> v from (render_info + 4) */
Next render_flushDisplayRegisters_800172C(void) {
    u32 ri = render_display_registers(R10);
    SET_FLAGS_ADD(ri, 4);
    RETURN_TO_CALLER();
}

/* ── SetRenderInfoLCDControl ── */
/* SetRenderInfoLCDControl(r0 = LCD control value, r10 = toolkit):
 * store the halfword into the render info's LCD control field.
 * Returns the render info pointer in r1. */

/* offset of the render-info pointer within the toolkit struct */
#define oToolkit_RenderInfoPtr 8

Next SetRenderInfoLCDControl(void) {
    u32 renderInfo = LD32(R10 + oToolkit_RenderInfoPtr);
    ST16(renderInfo, R0);
    R1 = renderInfo;
    RETURN_TO_CALLER();
}

/* ── GetRenderInfoLCDControl ── */
/* GetRenderInfoLCDControl: read the LCD control value (u16) that the render
 * info block tracks; the toolkit pointer comes in via r10.
 * (Offset 8 in the toolkit struct is its RenderInfo pointer; field 0x00 of
 * RenderInfo is the LCD control halfword.) */
Next GetRenderInfoLCDControl(void) {
    u32 renderInfo = LD32(R10 + 8 /* oToolkit_RenderInfoPtr */);

    R0 = LD16(renderInfo + 0 /* oRenderInfo_Unk_00 */);
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

/* ── zeroFillVRAM ── */
/* zeroFillVRAM: clear all of VRAM by zeroing it in four chunks.
 * BG modes use 0x6000000-0x600BFFF (0x40 units each), and the last
 * chunk 0x600C000-0x600DFFF (0x20 units). Leaves r1 = 0x20. */
Next zeroFillVRAM(void) {
    struct { u32 addr; u32 size; } chunks[4] = {
        { A_vram_6000000, 0x40 },
        { 0x6004000,      0x40 },
        { 0x6008000,      0x40 },
        { 0x600C000,      0x20 },
    };

    for (int i = 0; i < 4; i++) {
        R0 = chunks[i].addr;
        R1 = chunks[i].size;
        GUEST_CALL(ZeroFillByEightWords);
    }

    R0 = chunks[3].addr;
    R1 = chunks[3].size;
    RETURN_TO_CALLER();
}

/* ── ZeroFill_byte_3001960 ── */
/* ZeroFill_byte_3001960: clear the 4-byte palette buffer at palette_3001960
 * by filling 2 halfwords. r2/r3 pass through untouched; r1 and the carry/
 * overflow flags are left exactly as ZeroFillByHalfword leaves them. */
Next ZeroFill_byte_3001960(void) {
    R0 = A_palette_3001960;
    R1 = 2;
    GUEST_CALL(ZeroFillByHalfword);
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

/* ── copyPalletesToIWRAM_8001808 ── */
/* copyPalletesToIWRAM_8001808:
 * Copy the game's palette table (iPalette3001B60, 0x200 bytes) to the BG
 * palette memory at 0x5000000. r3 is preserved; r0-r2 are scratch. */
Next copyPalletesToIWRAM_8001808(void) {
    R0 = A_iPalette3001B60;   /* source: palette data */
    R1 = 0x5000000;           /* dest: BG palette RAM */
    R2 = 0x20 << 4;           /* size: 0x200 bytes */
    GUEST_CALL(CopyByEightWords);
    RETURN_TO_CALLER();
}

/* ── zeroFill_e2009740 ── */
/* zeroFill_e2009740:
 * Clear the 8-byte scratch struct pointed to by the toolkit's Unk2009740 pointer.
 * Reads the toolkit pointer from the register the caller provides in r10. */
Next zeroFill_e2009740(void) {
    R2 = R10;
    R0 = LD32(R2 + oToolkit_Unk2009740_Ptr);
    R1 = 8;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── zeroFill_e200F3A0 ── */
/* zeroFill_e200F3A0: zero-fill the 12-byte block pointed to by the toolkit's
 * oToolkit_Unk200f3a0_Ptr field. As a side effect r2 is left holding the
 * toolkit pointer (r10), r3 passes through unchanged. */
Next zeroFill_e200F3A0(void) {
    u32 block = LD32(R10 + oToolkit_Unk200f3a0_Ptr);

    R2 = R10;
    R0 = block;
    R1 = 12; /* bytes to clear */
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── ZeroFillGFX30025c0 ── */
/* ZeroFillGFX30025c0 (0800183C): zero out the BG tile ID block buffer.
 * R10 is the toolkit pointer; the buffer address comes from its
 * oToolkit_iBGTileIdBlocks_Ptr field and 0x2000 bytes are cleared.
 * R2/R3 pass through untouched; R1 ends as 0x2000 and the flags are
 * whatever ZeroFillByEightWords left behind. */
Next ZeroFillGFX30025c0(void) {
    R0 = LD32(R10 + oToolkit_iBGTileIdBlocks_Ptr);
    R1 = 0x2000;
    GUEST_CALL(ZeroFillByEightWords);
    RETURN_TO_CALLER();
}

/* ── copyMemory_8001850 ── */
/* copyMemory_8001850:
 * Loads two fixed data blocks with CopyByEightWords:
 *   - 0x800 bytes from dword_86A5520 into VRAM at 0x600D400
 *   - 0x20 bytes from byte_86BEC80 into IWRAM at unk_3001B40
 * Leaves R1/R2/flags as CopyByEightWords left them; R3 is untouched. */
Next copyMemory_8001850(void) {
    R0 = A_dword_86A5520;
    R1 = 0x600D400;
    R2 = 0x800;
    GUEST_CALL(CopyByEightWords);

    R0 = A_byte_86BEC80;
    R1 = A_unk_3001B40;
    R2 = 0x20;
    GUEST_CALL(CopyByEightWords);

    RETURN_TO_CALLER();
}

/* ── bgmap_WriteTileIdAtBlock_800187C ── */
/* bgmap_WriteTileIdAtBlock_800187C(r0 = tile x, r1 = tile y, r2 = 32x32-tile block index, r3 = tile id):
 * Store one tile id (16-bit) into the BG tile-id map at the given position.
 * Address = blocks_base + x*2 + y*64 + block*2048 (each block is a 32x32
 * halfword map, one map row is 32*2 = 64 bytes).
 * Leaves r0 = map offset, r1 = write address, r2 = block*2048, and the
 * nz/c/v flags from the address addition (as the original's ADDS does). */
Next bgmap_WriteTileIdAtBlock_800187C(void) {
    u32 xOff = R0 << 1;
    u32 yOff = R1 << 6;

    R2 = R2 << 11;              /* block offset; r2 is left shifted, like the original */
    R0 = xOff + yOff + R2;      /* total byte offset into the tile-id blocks */

    u32 base = LD32(R10 + oToolkit_iBGTileIdBlocks_Ptr);
    SET_FLAGS_ADD(base, R0);
    R1 = base + R0;

    ST16(R1, R3);
    RETURN_TO_CALLER();
}

/* ── bg_writeTileIdRow_80018E0 ── */
/* Result of bg_writeTileIdRow_80018E0: the values the original leaves in r0-r4.
 * r6/r7 are pushed/popped by the original, so they need no handling here. */
typedef struct {
    u32 x;        /* r0: column just past the last tile written, wrapped to the row */
    u32 mask;     /* r1: always 0x1F, the column mask used by the loop */
    u32 blockOff; /* r2: original r2 << 11, byte offset of the selected tile-id block */
    u32 src;      /* r3: source pointer advanced past the copied halfwords */
    s32 count;    /* r4: loop counter after the run; the final CMP r4,#0 sets the flags */
} BgRowWrite;

/* Copy a horizontal run of tile ids into one of the game's 32x32 BG tile-id
 * maps (r10 -> toolkit -> iBGTileIdBlocks array). `layer` picks the 0x800-byte
 * block (32x32 u16 entries), `y` the row (64 bytes per row), `x` the starting
 * column. Columns wrap at 32 (mask 0x1F), so a run longer than a row continues
 * on the same row from its left edge. Always writes at least one tile, matching
 * the original's do-while (a non-positive count decrements once and stops). */
static BgRowWrite bg_write_tile_row(u32 x, u32 y, u32 layer, u32 src, s32 count, u32 toolkit)
{
    u32 block = LD32(toolkit + oToolkit_iBGTileIdBlocks_Ptr);
    u32 dst = block + (layer << 11) + (y << 6);

    x &= 0x1F;
    do {
        ST16(dst + (x << 1), LD16(src));
        x = (x + 1) & 0x1F;
        src += 2;
        count--;
    } while (count > 0);

    BgRowWrite out;
    out.x = x;
    out.mask = 0x1F;
    out.blockOff = layer << 11;
    out.src = src;
    out.count = count;
    return out;
}

/* bg_writeTileIdRow_80018E0(r0 = start column, r1 = row, r2 = tile-id block, r3 = source tile
 * ids, r4 = count, r10 = toolkit) -> r0-r4, flags from the final CMP r4,#0 */
Next bg_writeTileIdRow_80018E0(void) {
    BgRowWrite out = bg_write_tile_row(R0, R1, R2, R3, (s32)R4, R10);
    R0 = out.x;
    R1 = out.mask;
    R2 = out.blockOff;
    R3 = out.src;
    R4 = out.count;
    SET_FLAGS_CMP(out.count, 0);
    RETURN_TO_CALLER();
}

/* ── fillOamTileStripes_8001930 ── */
/* fillOamTileStripes_8001930(r0 = tile_id_base, r1 = high_bits, r2 = stride_entries, r3 = strip_count, r4 = dst):
 *
 * Fill OAM attribute pairs. The inner loop writes two OAM entries per step
 * (4 bytes each): one at the running row cursor, one at cursor + stride (the
 * partner row in OAM). Four consecutive tile IDs are consumed per step.
 * After the inner loop fills `stride` bytes of the row, r3 is decremented;
 * if it was non-zero we return. */
Next fillOamTileStripes_8001930(void) {
    u32 stride      = R2 << 2;
    u32 tile_id     = R0 | (R1 << 12);
    u32 row_cursor  = R4;
    s32 strips_left = (s32)R3;

    for (;;) {
        /* First OAM entry of the pair. */
        ST16(row_cursor, (u16)tile_id);
        tile_id += 1;
        ST16(row_cursor + 2, (u16)tile_id);
        tile_id += 1;

        /* Second OAM entry: at row_cursor + stride (matches asm's
         * `add r1, r4, r2` using the pre-increment r4). */
        u32 second = row_cursor + stride;
        ST16(second, (u16)tile_id);
        tile_id += 1;
        ST16(second + 2, (u16)tile_id);
        tile_id += 1;

        row_cursor += 4;
        if (row_cursor < R4 + stride) {
            continue;                 /* inner loop: r5 < r2 */
        }

        strips_left -= 1;
        if (strips_left < 0) {
            continue;                 /* r3 was 0: loop again (asm `blt`) */
        }
        break;                        /* r3 >= 1: pop */
    }

    /* R4..R7 are preserved by the original push/pop; we never write them. */
    RETURN_TO_CALLER();
}

/* ── SetBGScrollCallbacks ── */
/* SetBGScrollCallbacks(r0 = scroll callback 0, r1 = scroll callback 1,
 *                      r2 = hblank callback):
 * Installs a set of BG scroll callbacks into the eBGScrollCallbacks table
 * (Callback0 at +0, HBlankCallback at +4, Callback1 at +8) and registers the
 * hblank callback as the handler for IRQ 4 (hblank). If callback 0 is NULL,
 * nothing is stored and the interrupt is left alone. On return, r2 still
 * holds the hblank callback and r3 holds the callbacks table address (when
 * the callbacks were installed); c/v are left as the last flag-setting step
 * (the early-out `tst` leaves them untouched). */
Next SetBGScrollCallbacks(void) {
    SET_FLAGS_NZ(R0); /* tst r0, r0 */

    if (R0 != 0) {
        u32 callbacks = A_eBGScrollCallbacks;

        ST32(callbacks, R0);      /* oBGScrollCallbacks_Callback0     */
        ST32(callbacks + 8, R1);  /* oBGScrollCallbacks_Callback1     */
        ST32(callbacks + 4, R2);  /* oBGScrollCallbacks_HBlankCallback */
        R3 = callbacks;

        /* Re-hook interrupt 4 (hblank) to the hblank callback.
         * `mov r1, r2` decodes as adds r1, r2, #0, so it sets flags. */
        R0 = 4;
        R1 = R2;
        SET_FLAGS_ADD(R2, 0);

        GUEST_CALL(SetInterruptCallback);
    }
    RETURN_TO_CALLER();
}

/* ── SetDummyBGScrollCallbacks ── */
/* Field offsets within the BG scroll callback table (rooted at 0x200A880).
 * The lifter only exports the table's base symbol, so spell the strides out. */
#define oBGScrollCallbacks_Callback0      0
#define oBGScrollCallbacks_HBlankCallback 4
#define oBGScrollCallbacks_Callback1      8

/* SetDummyBGScrollCallbacks: install no-op handlers for the BG scroll engine
 * (both per-frame callbacks) and for HBlank, then point the HBlank interrupt
 * slot at the dummy handler. Used on screens that manage their own BG updates.
 * Outputs: r1 = HBlank dummy handler, r2 = callback table (as left by the
 * interrupt dispatcher), lr = return link of the SetInterruptCallback call. */
Next SetDummyBGScrollCallbacks(void) {
    u32 callbacks = A_eBGScrollCallbacks;
    u32 bgDummy = A_nullsub_39 | 1;      /* Thumb entry addresses, hence |1 */
    u32 hblankDummy = A_nullsub_38 | 1;

    ST32(callbacks + oBGScrollCallbacks_Callback0, bgDummy);
    ST32(callbacks + oBGScrollCallbacks_Callback1, bgDummy);
    ST32(callbacks + oBGScrollCallbacks_HBlankCallback, hblankDummy);

    R0 = 4; /* interrupt slot: HBlank */
    R1 = hblankDummy;
    R2 = callbacks;
    GUEST_CALL(SetInterruptCallback);

    RETURN_TO_CALLER();
}

/* ── BGScrollCB_BG1Diagonal3to2Scroll ── */
/* Field offsets used here (not present in game.h for this build). */
#define oBGScrollCBCounters_Counter0 0
#define oBGScrollCBCounters_Counter1 4
#define oToolkit_RenderInfoPtr       8

/* BGScrollCB_BG1Diagonal3to2Scroll (0x080019B4):
 * Scroll callback for BG1 on the "diagonal 3-to-2" pattern. Both scroll
 * counters are advanced (counter0 by 8, counter1 by 4), each is divided by
 * 16, and the results are written to the render info as the new BG1 scroll
 * offsets. Leaves r1 = render info pointer, r2/r3 = new scroll values and
 * the flags as the final LSR/SUBS of the original leave them. */
Next BGScrollCB_BG1Diagonal3to2Scroll(void) {
    u32 counters = A_eBGScrollCBCounters;

    /* Horizontal counter: -= 8 (no flag update), store, then /16. */
    u32 counter0 = LD32(counters + oBGScrollCBCounters_Counter0);
    counter0 -= 8;
    ST32(counters + oBGScrollCBCounters_Counter0, counter0);
    u32 scrollX = counter0 >> 4;

    /* Vertical counter: -= 4 (SUBS sets N/Z/C/V), store, then /16. */
    u32 counter1 = LD32(counters + oBGScrollCBCounters_Counter1);
    SET_FLAGS_CMP(counter1, 4); /* N/Z/C/V of the subtraction */
    counter1 -= 4;
    ST32(counters + oBGScrollCBCounters_Counter1, counter1);
    u32 scrollY = counter1 >> 4;

    /* The LSR #4 re-set C to the last bit shifted out (bit 3 of the
     * post-subtraction value) and updated N/Z from the result; V is left
     * as the SUBS left it. */
    cpu.c = (counter1 >> 3) & 1;
    SET_FLAGS_NZ(scrollY);

    /* r1 = toolkit render info pointer; store both scroll values there. */
    u32 renderInfo = LD32(R10 + oToolkit_RenderInfoPtr);
    ST16(renderInfo + oRenderInfo_Unk_10, scrollX);
    ST16(renderInfo + oRenderInfo_Unk_12, scrollY);

    R1 = renderInfo;
    R2 = scrollX;
    R3 = scrollY;
    RETURN_TO_CALLER();
}

/* ── BGScrollCB_BG3Diagonal3to2Scroll ── */
/* BGScrollCB_BG3Diagonal3to2Scroll: per-frame scroll callback for the BG3 layer
 * during the "diagonal 3-to-2" scroll. Two fixed-point counters (4 fractional
 * bits) count down at different rates -- counter0 by 8, counter1 by 4 -- and
 * their whole-pixel parts (value >> 4) are stored as the BG3 scroll offsets.
 *
 * Flags out: N/Z from `counter1 >> 4`, C from bit 3 of counter1 (the bit shifted
 * out by the final `lsr r3, r3, #4`), V from the `subs r3, #4` subtraction.
 * r1-r3 out: r1 = render info pointer, r2 = X offset, r3 = Y offset. */
Next BGScrollCB_BG3Diagonal3to2Scroll(void) {
    u32 counters = A_eBGScrollCBCounters;

    /* Step both scroll counters down for this frame. */
    u32 counter0 = LD32(counters) - 8;
    ST32(counters, counter0);

    u32 counter1 = LD32(counters + 4) - 4;
    ST32(counters + 4, counter1);

    /* Fixed-point: take the upper 4 bits as the whole-pixel offsets. */
    u32 offsetX = counter0 >> 4;
    u32 offsetY = counter1 >> 4;

    u32 renderInfo = LD32(R10 + 8);
    ST16(renderInfo + oRenderInfo_Unk_18, offsetX);
    ST16(renderInfo + oRenderInfo_Unk_1a, offsetY);

    R1 = renderInfo;
    R2 = offsetX;
    R3 = offsetY;

    /* Final flags are those of `lsr r3, r3, #4` (N/Z from the shifted value,
     * C = bit shifted out), with V carried over from `subs r3, #4`
     * (overflow only when counter1 is in [0x80000000, 0x80000003]). */
    SET_FLAGS_NZ(offsetY);
    cpu.c = (counter1 >> 3) & 1;
    cpu.v = (counter1 >= 0x80000000u && counter1 < 0x80000004u);

    RETURN_TO_CALLER();
}

/* ── nullsub_39 ── */
/* nullsub_39 does nothing and just returns to the caller. */
static void nullsub_39_impl(void) {
}

Next nullsub_39(void) {
    nullsub_39_impl();
    RETURN_TO_CALLER();
}

/* ── zeroFill_e20094C0 ── */
/* zeroFill_e20094C0: clear the eGFXAnimStates block (0x1B0 bytes) to zero.
 * Inputs/outputs r2/r3 pass through untouched. */
Next zeroFill_e20094C0(void) {
    R0 = A_eGFXAnimStates;
    R1 = 0x1B0;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── TerminateGFXAnim ── */
/* TerminateGFXAnim(r0 = eGFXAnimState index):
 * Marks the given graphics-animation state as inactive (clears its state
 * byte) and tears down the palette transform associated with that slot.
 * The animation index is passed on unchanged to
 * Terminate_ePalette20097a0_Transform; its r1/r2 results and carry/overflow
 * flags are handed straight back to our caller. r4-r7 are preserved. */
Next TerminateGFXAnim(void) {
    u32 animIndex = R0;

    /* state entry for this animation = eGFXAnimStates[index] */
    u32 state = A_eGFXAnimStates + animIndex * oGFXAnimState_Size;
    ST8(state, 0); /* clear the state byte: animation no longer running */

    R0 = animIndex;
    GUEST_CALL(Terminate_ePalette20097a0_Transform);

    RETURN_TO_CALLER();
}

/* ── IsGFXAnimActive ── */
/* IsGFXAnimActive(r0 = animation slot index):
 * Test the state byte of the given graphics animation slot and set the
 * Z/N flags from it, so callers can branch on "animation active".
 * r4-r7 are preserved untouched; r0-r1 are scratch. */
Next IsGFXAnimActive(void) {
    u32 state = LD8(A_eGFXAnimStates + oGFXAnimState_Size * R0);
    SET_FLAGS_NZ(state);
    RETURN_TO_CALLER();
}

/* ── sprite_QueueAnimFrameGfxUpload_8001C44 ── */
/* sprite_QueueAnimFrameGfxUpload_8001C44(r0 = pointer to next anim data ptr, r7 = GFXAnimState):
 * Queue an 8-word-aligned VRAM transfer that uploads this animation frame's
 * graphics: source is (*r0) (GFXAnimData.ParamNext), destination and size
 * come from the anim state's Param0/Param1 slots. */
Next sprite_QueueAnimFrameGfxUpload_8001C44(void) {
    u32 gfxSrc = LD32(R0);                            /* GFXAnimData.ParamNext */
    u32 gfxDest = LD32(R7 + oGFXAnimState_Param0);    /* VRAM destination */
    u32 gfxSize = LD32(R7 + oGFXAnimState_Param1);    /* transfer size */

    R0 = gfxSrc;
    R1 = gfxDest;
    R2 = gfxSize;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    RETURN_TO_CALLER();
}

/* ── sub_8001D64 ── */
/* sub_8001D64(r4 = source, r5 = destination):
 * copy a block of 8 words (0x20 bytes) from source to destination.
 * r0 = the last word copied (the value at source + 0x1C). */
Next sub_8001D64(void) {
    u32 src = R4;
    u32 dst = R5;

    for (u32 i = 0; i < 8; i++) {
        ST32(dst + i * 4, LD32(src + i * 4));
    }

    R0 = LD32(src + 7 * 4);
    RETURN_TO_CALLER();
}

/* ── copyStruct_64bytes_8002000 ── */
/* Copy 16 words (64 bytes) from src to dst. Returns the last word copied. */
static u32 copy_64_bytes(u32 dst, u32 src) {
    for (u32 off = 0; off < 0x40; off += 4)
        ST32(dst + off, LD32(src + off));
    return LD32(src + 0x3C);
}

Next copyStruct_64bytes_8002000(void) {
    R0 = copy_64_bytes(R5, R4);
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

/* ── LoadGFXAnims ── */
/* LoadGFXAnims(r0 = gfx_anim_data_arr: * FFStop32<[GFXAnimScript]>):
 * load every GFX animation script in the NULL-terminated (negative-terminated)
 * array; leaves r0 and the flags as the final CMP leaves them. */
Next LoadGFXAnims(void) {
    u32 list = R0;

    for (;;) {
        u32 script = LD32(list);
        R0 = script;
        SET_FLAGS_CMP(script, 0);
        if ((s32)script < 0)
            break;

        GUEST_CALL(LoadGFXAnim); /* (script: * GFXAnimScript) -> () */
        list += 4;
    }

    RETURN_TO_CALLER();
}

/* ── zeroFill_e20097A0 ── */
/* zeroFill_e20097A0: clear the 0x108-byte palette buffer at ePalette20097a0
 * with zeroes (one word at a time, via ZeroFillByWord).
 * Flags on return are whatever ZeroFillByWord leaves. */
Next zeroFill_e20097A0(void) {
    R0 = A_ePalette20097a0;
    R1 = 0x108;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── sub_8002378 ── */
/* Field offsets within one entry of the software palette table
 * ePalette20097a0 (the header only exposes the table's base address,
 * so the per-field immediates used by the assembly are defined here). */
enum { PAL20097A0_00 = 0, PAL20097A0_01 = 1, PAL20097A0_02 = 2, PAL20097A0_04 = 4, PAL20097A0_08 = 8 };

/* sub_8002378(r0, r1 = palette data pointer, r2 = color count,
 *             r3 = palette slot index, r4):
 * Fill slot r3 of the software palette table at ePalette20097a0. The
 * slot is only populated when both the data pointer (r1) and the color
 * count (r2) are nonzero; in that case r0/r1/r2/r4 are stored verbatim
 * into the slot's fields. The slot's flag byte is set to 1 when it was
 * filled and 0 otherwise, and that same value is returned in r3.
 * r5-r7 are preserved. */
Next sub_8002378(void) {
    u32 base = A_ePalette20097a0;
    u32 offset = R3 * oPalette20097a0_Size;
    u32 filled = 0;

    /* `add r7, r7, r3` is the last instruction on either path that
     * touches c/v, so they come from this address computation. */
    SET_FLAGS_ADD(base, offset);
    u32 entry = base + offset;

    if (R1 != 0 && R2 != 0) {
        ST8(entry + PAL20097A0_01, R0);
        ST32(entry + PAL20097A0_04, R1);
        ST8(entry + PAL20097A0_02, R2);
        ST32(entry + PAL20097A0_08, R4);
        filled = 1;
    }

    ST8(entry + PAL20097A0_00, filled);

    /* Final n/z: from `mov r3,#1` when the slot was filled, otherwise
     * from the failing `tst r1,r1` / `tst r2,r2` (a zero operand gives
     * Z set, N clear) - identical to the flags of `filled`. */
    SET_FLAGS_NZ(filled);
    R3 = filled;
    RETURN_TO_CALLER();
}

/* ── Terminate_ePalette20097a0_Transform ── */
/* Terminate_ePalette20097a0_Transform(r0 = index into the ePalette20097a0 palette array):
 * zeroes the first byte (the live/active flag) of the selected palette entry.
 * Leaves r0 = 0, r1 = entry size, r2 = address of the cleared byte;
 * nz reflects the stored zero, c/v come from the address computation (ADDS). */
Next Terminate_ePalette20097a0_Transform(void) {
    u32 base = A_ePalette20097a0;
    u32 offset = R0 * oPalette20097a0_Size;

    u32 entry = base + offset;
    SET_FLAGS_ADD(base, offset);   /* flags of the ADDS that formed the address */

    ST8(entry, 0);                 /* terminate the palette entry */

    R0 = 0;
    R1 = oPalette20097a0_Size;
    R2 = entry;
    SET_FLAGS_NZ(R0);              /* flags of the final "mov r0, #0" */
    RETURN_TO_CALLER();
}

/* ── palette_clearEPalette20097a0_80023A8 ── */
/* palette_clearEPalette20097a0_80023A8: zero-fill the palette object at ePalette20097a0
 * (0xD8 bytes, cleared word by word).
 * r0/r1 are scratch; r2/r3 and LR are untouched. */
Next palette_clearEPalette20097a0_80023A8(void) {
    R0 = A_ePalette20097a0;
    R1 = 0xD8;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── zeroFill_80024A2 ── */
/* zeroFill_80024A2:
 * Clear the 0x50-byte struct at eStruct200a6a0 by filling it with zero words.
 * R1 and the c/v flags on return are whatever ZeroFillByWord leaves behind. */
Next zeroFill_80024A2(void) {
    R0 = A_eStruct200a6a0;
    R1 = 0x50;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── event_runArmedCallback_80024AE ── */
/* event_runArmedCallback_80024AE: run the registered event callback, if one is armed.
 *
 * eStruct200a6a0 is a callback record: a byte "armed" flag followed by the
 * callback's address. If a callback is armed, it is invoked with its own
 * address in r0; otherwise this returns immediately. The caller's r4-r7
 * are preserved across the call (the original pushes/pops them around it). */
Next event_runArmedCallback_80024AE(void) {
    u32 record = A_eStruct200a6a0;
    u32 armed = LD8(record);

    SET_FLAGS_NZ(armed); /* tst r0, r0 */

    if (armed != 0) {
        u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

        R5 = record;
        R0 = LD32(record + 4); /* the handler runs with its own address in r0 */
        dispatch(R0);

        /* The original returns through a pop {r4-r7,pc} after the callback,
         * so restore the caller's r4-r7 no matter what the callback left. */
        R4 = r4;
        R5 = r5;
        R6 = r6;
        R7 = r7;
    }

    RETURN_TO_CALLER();
}

/* ── resetOverworldScene_800260C ── */
/* register form: calls ZeroFillByEightWords outside this file, which has no typed C interface yet */
static void zero_fill_eight_words(u32 addr, u32 size) {
    R0 = addr;
    R1 = size;
    GUEST_CALL(ZeroFillByEightWords);
}

/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet */
static void zero_fill_byte(u32 addr, u32 size) {
    R0 = addr;
    R1 = size;
    GUEST_CALL(ZeroFillByByte);
}

/* register form: calls copy_8002668 outside this file, which has no typed C interface yet */
static void copy_8002668_(void) {
    GUEST_CALL(copy_8002668);
}

/* register form: calls InitializeOWPlayerObjectStruct outside this file, which has no typed C interface yet */
static void init_ow_player_obj(void) {
    GUEST_CALL(InitializeOWPlayerObjectStruct);
}

/* register form: calls InitializeOverworldNPCObjectStructs outside this file, which has no typed C interface yet */
static void init_ow_npc_objs(void) {
    GUEST_CALL(InitializeOverworldNPCObjectStructs);
}

/* register form: calls InitializeOverworldMapObjectStructs outside this file, which has no typed C interface yet */
static void init_ow_map_objs(void) {
    GUEST_CALL(InitializeOverworldMapObjectStructs);
}

/* register form: calls sprite_resetObjVars_800289C outside this file, which has no typed C interface yet */
static void sprite_reset_obj_vars(void) {
    GUEST_CALL(sprite_resetObjVars_800289C);
}

/* Typed core: reset overworld state on a new scene load.
 *   - Zeros OAM (0x07000000, 0x400 bytes) and sound RAM (0x06010000, 0x8000 bytes).
 *   - Runs copy_8002668 (loader/main copy hook).
 *   - Initializes player, NPC, and map object structs.
 *   - Resets sprite variables.
 *   - Clears the 7-byte unk_200F388 RAM buffer. */
static void copy_800260C_impl(void) {
    zero_fill_eight_words(0x07000000u, 0x400u);
    zero_fill_eight_words(0x06010000u, 0x8000u);

    copy_8002668_();
    init_ow_player_obj();
    init_ow_npc_objs();
    init_ow_map_objs();
    sprite_reset_obj_vars();

    zero_fill_byte(A_unk_200F388, 7u);
}

/* Thin adapter: inputs r2/r3/r4/r7 are untouched by the typed function, so they pass through
 * automatically. The original function's only output (besides preserved inputs) is the
 * side effects above; no registers or flags need to be set. */
Next resetOverworldScene_800260C(void) {
    copy_800260C_impl();
    RETURN_TO_CALLER();
}

/* ── copyPalletesToIWRAM_8002650 ── */
/* copyPalletesToIWRAM_8002650: copy the 0x200-byte palette at iPallete3001750
 * to the BG palette memory at 0x05000200 via CopyByEightWords. */
Next copyPalletesToIWRAM_8002650(void) {
    R0 = A_iPallete3001750;
    R1 = 0x5000200;
    R2 = 0x20 << 4; /* 512 bytes = 32 words * 16 */
    GUEST_CALL(CopyByEightWords);
    RETURN_TO_CALLER();
}
