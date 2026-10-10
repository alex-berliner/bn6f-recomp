/*
 * leaves_farm_2.c: small functions from the model farm, set 2.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

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

/* ── IsGFXAnimActive ── */
/* IsGFXAnimActive(r0 = animation slot index):
 * Test the state byte of the given graphics animation slot and set the
 * Z/N flags from it, so callers can branch on "animation active".
 * r4-r7 are preserved untouched; r0-r1 are scratch. */
Next IsGFXAnimActive(void) {
    u32 state = LD8(A_eGFXAnimState0 + oGFXAnimState_Size * R0);
    SET_FLAGS_NZ(state);
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
        u32 callbacks = A_eBGScrollCallbacks_Callback0;

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
    u32 state = A_eGFXAnimState0 + animIndex * oGFXAnimState_Size;
    ST8(state, 0); /* clear the state byte: animation no longer running */

    R0 = animIndex;
    GUEST_CALL(Terminate_ePalette20097a0_Transform);

    RETURN_TO_CALLER();
}

/* ── Terminate_ePalette20097a0_Transform ── */
/* Terminate_ePalette20097a0_Transform(r0 = index into the ePalette20097a0 palette array):
 * zeroes the first byte (the live/active flag) of the selected palette entry.
 * Leaves r0 = 0, r1 = entry size, r2 = address of the cleared byte;
 * nz reflects the stored zero, c/v come from the address computation (ADDS). */
Next Terminate_ePalette20097a0_Transform(void) {
    u32 base = A_ePalette20097a0_0_Unk_00;
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

/* ── clearWord_e200AC1C ── */
/* clearWord_e200AC1C: clear the word at dword_200AC1C (set it to 0). */
Next clearWord_e200AC1C(void) {
    ST32(A_dword_200AC1C, 0);
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

/* ── stub_returnZero_8000E30 ── */
/* stub_returnZero_8000E30: a stub that simply returns 0 in r0.
 * The original saves r4-r7, clears r4, moves it to r0, and immediately
 * restores r4-r7, so the only observable effect is R0 = 0. */
Next stub_returnZero_8000E30(void) {
    R0 = 0;
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

/* ── palette_clearEPalette20097a0_80023A8 ── */
/* palette_clearEPalette20097a0_80023A8: zero-fill the palette object at ePalette20097a0
 * (0xD8 bytes, cleared word by word).
 * r0/r1 are scratch; r2/r3 and LR are untouched. */
Next palette_clearEPalette20097a0_80023A8(void) {
    R0 = A_ePalette20097a0_0_Unk_00;
    R1 = 0xD8;
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

/* ── zeroFill_e20094C0 ── */
/* zeroFill_e20094C0: clear the eGFXAnimStates block (0x1B0 bytes) to zero.
 * Inputs/outputs r2/r3 pass through untouched. */
Next zeroFill_e20094C0(void) {
    R0 = A_eGFXAnimState0;
    R1 = 0x1B0;
    GUEST_CALL(ZeroFillByWord);
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

/* ── zeroFill_e20097A0 ── */
/* zeroFill_e20097A0: clear the 0x108-byte palette buffer at ePalette20097a0
 * with zeroes (one word at a time, via ZeroFillByWord).
 * Flags on return are whatever ZeroFillByWord leaves. */
Next zeroFill_e20097A0(void) {
    R0 = A_ePalette20097a0_0_Unk_00;
    R1 = 0x108;
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

/* ── Clear_eStruct2000780 ── */
/* Clear_eStruct2000780: zero out the 0x48-byte scenario effect state block
 * at eScenarioEffectState2000780. Doesn't touch r2-r7. */
Next Clear_eStruct2000780(void) {
    R0 = A_eScenarioEffectState2000780;
    R1 = 0x48;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── InitializeOWPlayerObjectStruct ── */
/* InitializeOWPlayerObjectStruct:
 * Initializes the player's overworld object struct (object type 0) by
 * forwarding to the generic struct initializer. All other arguments
 * (r2, r3, r4, r7) are passed through unchanged; results and flags come
 * straight from the callee. */
#define OVERWORLD_PLAYER_OBJECT 0

Next InitializeOWPlayerObjectStruct(void) {
    R0 = OVERWORLD_PLAYER_OBJECT;
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeOverworldMapObjectStructs ── */
/* InitializeOverworldMapObjectStructs: reinitialize every overworld-map
 * object struct (object type 5, the "overworld map object" type).
 * Its r2/r3/r4/r7 arguments are passed straight through to
 * InitializeStructsOfObjectType, which does all the work. */
Next InitializeOverworldMapObjectStructs(void) {
    R0 = 5; /* OVERWORLD_MAP_OBJECT */
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeOverworldNPCObjectStructs ── */
/* InitializeOverworldNPCObjectStructs: initialize every overworld object struct
 * of object type 2 (NPC). Delegates to InitializeStructsOfObjectType; r0-r3 are
 * that callee's arguments/scratch, r4 and r7 must survive the call. */
Next InitializeOverworldNPCObjectStructs(void) {
    R0 = 2; /* OVERWORLD_NPC_OBJECT */
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeT3BattleObjectStructs ── */
/* InitializeT3BattleObjectStructs(r2, r3, r4, r7 = pass-through arguments):
 * Reset all battle object structs of object type 3 (T3_BATTLE_OBJECT); the
 * caller's r2/r3/r4/r7 arguments are forwarded untouched to
 * InitializeStructsOfObjectType. */
Next InitializeT3BattleObjectStructs(void) {
    R0 = 3; /* T3_BATTLE_OBJECT */
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── InitializeT4BattleObjectStructs ── */
/* InitializeT4BattleObjectStructs:
 * Re-initialize every battle object struct of the T4 (tourney battle) object
 * type (type constant 4). The value is passed straight through to
 * InitializeStructsOfObjectType, which does all the work; the caller's
 * arguments in r2/r3 and its saved r4/r7 must reach it untouched, and the
 * outputs are simply whatever that callee leaves behind. */
Next InitializeT4BattleObjectStructs(void) {
    R0 = 4; /* T4_BATTLE_OBJECT */
    GUEST_CALL(InitializeStructsOfObjectType);
    RETURN_TO_CALLER();
}

/* ── PauseBattle ── */
/* PauseBattle (0800A028): set the battle-paused flag on the current game state.
 * r10 = toolkit; returns r0 = 1, r1 = game state pointer, NZ from r0 = 1. */
Next PauseBattle(void) {
    R1 = LD32(R10 + oToolkit_GameStatePtr);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    ST8(R1 + oGameState_BattlePaused, R0);
    RETURN_TO_CALLER();
}

/* ── RemoveGivenAliveBattleActorFromList ── */
/* RemoveGivenAliveBattleActorFromList(r0 = actor, r10 = toolkit):
 * Walks the battle state's list of 8 alive player actor slots and clears
 * every slot that holds the given actor.
 * Leaves r1 pointing just past the list, r2 = value of the last slot read
 * (0 if it matched), r3 = 8, and the flags from `cmp r3, #8`. */
Next RemoveGivenAliveBattleActorFromList(void) {
    u32 actor = R0;
    u32 entry = LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_AlivePlayerActors;

    R3 = 0;
    while ((s32)R3 < 8) {
        R2 = LD32(entry);
        if (R2 == actor) {
            R2 = 0;
            ST32(entry, R2);
        }
        entry += 4;
        R3 += 1;
    }

    R1 = entry;
    SET_FLAGS_CMP(R3, 8);
    RETURN_TO_CALLER();
}

/* ── SetPrimaryToolkitPointersWrapper ── */
/* SetPrimaryToolkitPointersWrapper: 08006C18 THUMB
 * A trivial trampoline: it just tail-calls SetPrimaryToolkitPointers, which
 * sets up the primary toolkit pointers (leaving its results in r2/r3/r10),
 * and then returns straight to whoever called this wrapper. r3 passes
 * through untouched. */
Next SetPrimaryToolkitPointersWrapper(void) {
    GUEST_CALL(SetPrimaryToolkitPointers);
    RETURN_TO_CALLER();
}

/* ── UnpauseBattle ── */
/* UnpauseBattle: clear the "battle paused" flag of the current game state. */
Next UnpauseBattle(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    ST8(gameState + oGameState_BattlePaused, 0);
    RETURN_TO_CALLER();
}

/* ── UpdateBattleObjectLinkedList ── */
/* Offsets used by the battle object linked list (fields at the tail of each object,
 * which is why the caller passes r5 = object end address).
 * The sentinel's "prev" field sits at offset 0, like the objects' own prev field. */
#define oBattleObject_LinkedList_Prev           0
#define oBattleObject_LinkedList_Next           4
#define oBattleObjectsLinkedListSentinel_Prev   0

/* UpdateBattleObjectLinkedList (0x08003428):
 * Append the newly created battle object (whose end address the caller left in r5) to
 * the tail of the battle object list, i.e. right before the sentinel that marks the
 * end of the list.
 *   inputs:  r5 = end of the new object (object start = r5 - oBattleObject_LinkedList_Size)
 *   outputs: r0 = new object, r1 = sentinel, r2 = previous last object,
 *            nz/c/v from the subtraction that computed the object address
 * List layout: ... <-> prev <-> new <-> sentinel                             */
Next UpdateBattleObjectLinkedList(void) {
    /* C: the object being inserted (r5 points one object-size past its start) */
    u32 newObject = R5 - oBattleObject_LinkedList_Size;
    SET_FLAGS_CMP(R5, oBattleObject_LinkedList_Size);

    /* S: the list sentinel; its prev field holds the current last object */
    u32 sentinel = A_eBattleObjectsLinkedListSentinel_Prev;

    /* P: the object currently last in the list (the sentinel itself if the list is empty) */
    u32 prev = LD32(sentinel + oBattleObjectsLinkedListSentinel_Prev);

    /* prev <-> new: make the old last object point forward to the new one */
    ST32(prev + oBattleObject_LinkedList_Next, newObject);

    /* new's backward link goes to the old last object */
    ST32(newObject + oBattleObject_LinkedList_Prev, prev);

    /* new's forward link goes to the sentinel (it is now the last object) */
    ST32(newObject + oBattleObject_LinkedList_Next, sentinel);

    /* the sentinel's backward link now points at the new last object */
    ST32(sentinel + oBattleObjectsLinkedListSentinel_Prev, newObject);

    R0 = newObject;
    R1 = sentinel;
    R2 = prev;
    RETURN_TO_CALLER();
}

/* ── battle_clearEnemyFadeinList ── */
/* battle_clearEnemyFadeinList:
 * Reset the enemy fade-in list used during battle setup. Asks getNextEntryAddress_800AAAE
 * (called with r0 = 0) for the list's base address, then clears the first
 * 0x80 bytes of it. Returns with r1 = 0x80 (the size that was cleared). */
Next battle_clearEnemyFadeinList(void) {
    R0 = 0;
    GUEST_CALL(getNextEntryAddress_800AAAE);   /* r0 = base address of the fade-in list */

    R1 = 0x80;                 /* size in bytes */
    GUEST_CALL(ZeroFillByWord); /* zero-fill r1 bytes starting at r0 */

    RETURN_TO_CALLER();
}

/* ── battle_clearFlags ── */
/* battle_clearFlags(r0 = flag bits to clear):
 * Clears the given bits of the battle state's halfword flag field.
 * r10 = toolkit pointer; r1 = battle state pointer, r2 = new flags. */
Next battle_clearFlags(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 flags = LD16(battleState + oBattleState_Unk_32) & ~R0;

    ST16(battleState + oBattleState_Unk_32, flags);
    R1 = battleState;
    R2 = flags;
    RETURN_TO_CALLER();
}

/* ── battle_isBattleOver ── */
/* battle_isBattleOver: check whether the current battle should end.
 * Reads the battle state through the toolkit pointer in r10.
 * Returns r0 = 0 only when all end-of-battle conditions hold:
 * the two state flags (oBattleState_Unk_12/13) must be set and
 * oBattleState_Unk_0b must be clear; otherwise r0 = 1.
 * r3 = the battle state pointer; r1 = the last flag byte examined;
 * flags reflect the last test. */
Next battle_isBattleOver(void) {
    R0 = 1;

    R3 = LD32(R10 + oToolkit_BattleStatePtr);

    u32 flag12 = LD8(R3 + oBattleState_Unk_12);
    R1 = flag12;
    SET_FLAGS_NZ(flag12);
    if (flag12 == 0)
        RETURN_TO_CALLER();

    u32 flag13 = LD8(R3 + oBattleState_Unk_13);
    R1 = flag13;
    SET_FLAGS_NZ(flag13);
    if (flag13 == 0)
        RETURN_TO_CALLER();

    u32 flag0b = LD8(R3 + oBattleState_Unk_0b);
    R1 = flag0b;
    SET_FLAGS_NZ(flag0b);
    if (flag0b != 0)
        RETURN_TO_CALLER();

    /* All conditions met: battle is over. */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_isPaused ── */
/* battle_isPaused (0x0800A03C):
 * Read the battle-paused flag from the current game state.
 * r10 = toolkit pointer. Returns r0 = oGameState_BattlePaused byte and
 * leaves Z set iff the game is paused (r0 == 0); r1 = game state pointer. */
Next battle_isPaused(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 paused = LD8(gameState + oGameState_BattlePaused);

    R0 = paused;
    R1 = gameState;
    SET_FLAGS_NZ(paused);

    RETURN_TO_CALLER();
}

/* ── battle_networkInvert ── */
/* battle_networkInvert(r0 = value, r10 = toolkit):
 * XORs r0 with the byte at battleState->Unk_0d and returns the result in r0.
 * Also leaves the battle state pointer in r1, the toggled byte in r2, and the
 * NZ flags set from the result (the original's EOR is its last flag setter). */
Next battle_networkInvert(void) {
    u32 value = R0;

    R1 = LD32(R10 + oToolkit_BattleStatePtr);
    R2 = LD8(R1 + oBattleState_Unk_0d);
    R0 = value ^ R2;

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_setFlags ── */
/* battle_setFlags(r0 = flag bits):
 * Sets the given bits in the battle state's flags halfword
 * (BattleState + oBattleState_Unk_32), where the battle state is
 * taken from the toolkit (r10).
 * Leaves r1 = battle state pointer, r2 = new flags value. */
Next battle_setFlags(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    u32 flags = LD16(battle + oBattleState_Unk_32) | R0;
    ST16(battle + oBattleState_Unk_32, flags);

    R1 = battle;
    R2 = flags;
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getBattleResult ── */
/* eStruct200A008_getBattleResult: return the battle-result flag stored at
 * byte_200A009 (one byte past the start of the OW player object table).
 * Outputs: r0 = flag value, r1 = eOWPlayerObjectEnd (0x200A008). */
Next eStruct200A008_getBattleResult(void) {
    R1 = A_eOWPlayerObject_SpriteData_Size;   /* literal pool value: eOWPlayerObjectEnd */
    R0 = LD8(R1 + 1);                         /* byte_200A009 */
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getUnk04 ── */
/* eStruct200A008_getUnk04: return the u16 field at 0x200A00C (the player's
 * overworld object struct's "Unk04" field). Result in r0. */
Next eStruct200A008_getUnk04(void) {
    R0 = LD16(A_eOWPlayerObject_SpriteData_Size + 4);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getUnk06 ── */
/* eStruct200A008_getUnk06:
 * read the halfword field at offset 6 of the struct at eOWPlayerObjectEnd
 * (word_200A00E) and return it in r0. */
#define word_200A00E (A_eOWPlayerObject_SpriteData_Size + 6)

Next eStruct200A008_getUnk06(void) {
    R0 = LD16(word_200A00E);
    RETURN_TO_CALLER();
}

#undef word_200A00E

/* ── eStruct200A008_setBattleResult ── */
/* eStruct200A008_setBattleResult(r0 = battle result):
 * Store the battle result byte at byte_200A009 (one byte past
 * eOWPlayerObjectEnd), leaving the field's base address in r1. */
Next eStruct200A008_setBattleResult(void) {
    R1 = A_eOWPlayerObject_SpriteData_Size;
    ST8(R1 + 1, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk00 ── */
/* eStruct200A008_setUnk00(r0 = value):
 * stores a single byte at the eOWPlayerObject_SpriteData_Size location. */
Next eStruct200A008_setUnk00(void) {
    ST8(A_eOWPlayerObject_SpriteData_Size, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk02 ── */
/* eStruct200A008_setUnk02(r0 = value):
 * Stores r0 into the byte at offset 2 of the struct at 0x200A008
 * (byte_200A00A), leaving the struct base address in r1. */
Next eStruct200A008_setUnk02(void) {
    u32 value = R0;

    R1 = A_eOWPlayerObject_SpriteData_Size;
    ST8(R1 + 2, value);

    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk03 ── */
/* eStruct200A008_setUnk03(r0 = value):
 * Store the low byte of r0 into the byte just past the end of the overworld
 * player object (byte_200A00B, the "unk03" field of struct 200A008).
 * Leaves r1 = end-of-player-object address (200A008), as the original does. */
Next eStruct200A008_setUnk03(void) {
    u32 base = A_eOWPlayerObject_SpriteData_Size;

    ST8(base + 3, R0);
    R1 = base;

    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk04 ── */
/* eStruct200A008_setUnk04(r0 = value):
 * Write a 16-bit value to the field at offset +4 of the struct based at
 * eOWPlayerObject_SpriteData_Size (i.e. word_200A00C). Used as a simple
 * setter for that field; nothing else is touched. */
Next eStruct200A008_setUnk04(void) {
    u32 structBase = A_eOWPlayerObject_SpriteData_Size; /* 0x200A008 */
    ST16(structBase + 4, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk06 ── */
/* eStruct200A008_setUnk06(r0 = value):
 * Store r0 as a halfword at word_200A00E — the +0x6 field of the
 * struct whose base is the literal eOWPlayerObjectEnd. */
Next eStruct200A008_setUnk06(void) {
    ST16(A_eOWPlayerObject_SpriteData_Size + 6, R0);
    RETURN_TO_CALLER();
}

/* ── encryption_xorBufferWithKey_8006df6 ── */
/* Key slot used by the buffer encryption routines: the literal at
 * eUnusedExtraToolkitPtrsOffset+4 holds the XOR key
 * (dword_2001064 in RAM, kept as an offset from 0x2001060). */
#define ENCRYPTION_KEY_SLOT (A_eUnusedExtraToolkitPtrsOffset + 4)

/* encryption_xorBufferWithKey_8006df6(r0 = buffer, r1 = byte count):
 * XOR every byte of the buffer with the current cipher key, walking from
 * the last byte down to the first, then write the key back to its slot.
 * The buffer is transformed in place (encrypt and decrypt are the same
 * operation for XOR). */
Next encryption_xorBufferWithKey_8006df6(void) {
    u32 buf = R0;
    s32 i = (s32)R1 - 1;
    u32 key = LD32(ENCRYPTION_KEY_SLOT);

    do {
        ST8(buf + i, LD8(buf + i) ^ key);
        i--;
    } while (i >= 0);

    ST32(ENCRYPTION_KEY_SLOT, key);
    RETURN_TO_CALLER();
}

/* ── rollRngSeedIntoExtraToolkitPtrs_8006e26 ── */
/* rollRngSeedIntoExtraToolkitPtrs_8006e26: roll the hardware RNG until it produces something other
 * than 0xFFFFFFFF, then store its bitwise complement at
 * eUnusedExtraToolkitPtrsOffset + 4. r4-r7 are untouched. */
Next rollRngSeedIntoExtraToolkitPtrs_8006e26(void) {
    u32 value;

    do {
        GUEST_CALL(GetRNG);
        value = ~R0;          /* ~RNG: 0 only if the RNG returned 0xFFFFFFFF */
    } while (value == 0);

    ST32(A_eUnusedExtraToolkitPtrsOffset + 4, value);
    RETURN_TO_CALLER();
}

/* ── encryption_copyByteXor6fToToolkitBuffer_8006e3c ── */
/* encryption_copyByteXor6fToToolkitBuffer_8006e3c(r0 = index):
 * Obfuscation step for stored data: take the byte at byte_20004E0[index],
 * XOR it with 0x6F, and write the result into the buffer pointed to by
 * the toolkit's oToolkit_Unk2004a8c_Ptr entry at the same index.
 * All registers (including r0 and r5/r7) are preserved. */
Next encryption_copyByteXor6fToToolkitBuffer_8006e3c(void) {
    u32 index = R0;

    u32 table = A_byte_20004E0;
    u32 buffer = LD32(R10 + oToolkit_Unk2004a8c_Ptr);

    ST8(buffer + index, LD8(table + index) ^ 0x6F);

    RETURN_TO_CALLER();
}

/* ── encryption_applyPack_8006e70 ── */
/* encryption_applyPack_8006e70(r0 = pack slot index):
 * Re-encrypts one byte of pack data: the source byte lives in the table at
 * byte_20008A0, and its XOR-0x81 counterpart is written into the pack buffer
 * (Toolkit.unk_2004C20, reached via the pointer in r10). All registers are
 * preserved; r10 is only read. */
Next encryption_applyPack_8006e70(void) {
    u32 index = R0;

    u32 packBuffer = LD32(R10 + oToolkit_Unk2004c20_Ptr);
    ST8(packBuffer + index, LD8(A_byte_20008A0 + index) ^ 0x81);

    RETURN_TO_CALLER();
}

/* ── encryption_bugfrags_8006fac ── */
/* encryption_bugfrags_8006fac:
 * Recomputes the obfuscated BugFrag counter. Reads the protected BugFrag
 * count from the game state; unless save-flag bit 1 is set, stores its
 * bitwise complement into the checksum slot (oGameState_Unk_70). Then
 * scrambles the count with the value at dword_20018B8 and writes the
 * result to the display/state pointer at oToolkit_Unk2005030_Ptr.
 * All argument registers (r0-r7) are preserved; the original push/pop
 * exists only to give the body scratch registers. */
Next encryption_bugfrags_8006fac(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 bugfrags = LD32(gameState + oGameState_ProtectedBugfrags);

    if (!(LD32(gameState + oGameState_Unk_74) & 2))
        ST32(gameState + oGameState_Unk_70, ~bugfrags);

    u32 scramble = LD32(A_dword_20018B8);
    u32 output = LD32(R10 + oToolkit_Unk2005030_Ptr);
    ST32(output, bugfrags ^ scramble);

    RETURN_TO_CALLER();
}

/* ── encryption_navicustMaybe_8006e50 ── */
/* encryption_navicustMaybe_8006e50(r0 = index, r10 = toolkit):
 * Check whether the checksum byte at byte_20004E0[index] (stored XOR-0x6F)
 * matches the byte at Unk2004a8c[index]. Sets r0 = 1 on mismatch; r0 is
 * otherwise clobbered (callers ignore it on the match path).
 * Flags reflect the comparison, as the original's CMP/MOV would leave them. */
Next encryption_navicustMaybe_8006e50(void) {
    u32 index = R0;
    u32 table = LD32(R10 + oToolkit_Unk2004a8c_Ptr);

    u32 expected = (u8)LD8(A_byte_20004E0 + index) ^ 0x6F;
    u32 actual   = (u8)LD8(table + index);

    SET_FLAGS_CMP(expected, actual);
    if (expected != actual) {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── encryption_save_memSetFlags_8006E0E ── */
/* encryption_save_memSetFlags_8006E0E(r0 = save buffer, r1 = byte count):
 * Toggle the save data's obfuscation: every byte of the buffer is XORed
 * with the key kept at eUnusedExtraToolkitPtrsOffset+4, walking backwards
 * from the last byte to the first (applying it twice restores the data,
 * so one routine serves both encrypt and decrypt).
 *
 * This is a do-while: the body runs once even for a count of <= 0, and a
 * negative index wraps around the address space exactly as the ARM
 * ldrb/strb with a negative register offset does.
 *
 * Note that although the original loads the key into r7 while working,
 * it pushes/pops r4-r7 around the body, so r4-r7 (including r7) are left
 * with their incoming values. The key is only persisted through memory.
 * The last XORed byte is left in r3; r0, r1 and the flags are scratch. */
Next encryption_save_memSetFlags_8006E0E(void) {
    u32 buf = R0;
    u32 key = LD32(A_eUnusedExtraToolkitPtrsOffset + 4);
    s32 i = (s32)R1 - 1;

    do {
        u32 addr = buf + (u32)i; /* negative i wraps, as on the hardware */
        u32 x = LD8(addr) ^ key;
        R3 = x;
        ST8(addr, x);
    } while (--i >= 0);

    ST32(A_eUnusedExtraToolkitPtrsOffset + 4, key);
    RETURN_TO_CALLER();
}

/* ── encryption_testPack_8006e84 ── */
/* encryption_testPack_8006e84(r0 = pack slot index, r10 = toolkit):
 * Verify an encrypted pack data byte. Compares (byte_20008A0[index] ^ 0x81)
 * against the byte stored at the same index in the buffer pointed to by
 * toolkit->Unk2004c20. On mismatch r0 is set to 1; on match r0 is left as the
 * index that was passed in. The flags always reflect the comparison, so
 * callers may branch on equality either way.
 * Clobbers r0; preserves r1-r7. */
Next encryption_testPack_8006e84(void) {
    u32 index = R0;

    u8 decrypted = LD8(A_byte_20008A0 + index) ^ 0x81;
    u8 stored = LD8(LD32(R10 + oToolkit_Unk2004c20_Ptr) + index);

    if (decrypted == stored) {
        /* Flags come from the CMP against the stored byte. */
        SET_FLAGS_CMP(decrypted, stored);
    } else {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── encryption_zenny_8006f54 ── */
/* encryption_zenny_8006f54:
 * Re-encrypt the player's zenny counter. Stores the raw (plaintext) amount
 * where Unk2005028 points, and keeps a bitwise-NOT mirror of it in the game
 * state (an anti-tamper check) unless that mirror is currently suppressed
 * by bit 0 of oGameState_Unk_74. The stored value is the plaintext amount
 * XORed with the global obfuscation key at dword_2000060.
 * All argument registers (r0-r7) are preserved; r10 is the toolkit pointer. */
Next encryption_zenny_8006f54(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 zenny = LD32(gameState + oGameState_ProtectedZenny);

    /* Maintain the inverted copy of the zenny amount unless it is held. */
    if (!(LD32(gameState + oGameState_Unk_74) & 1))
        ST32(gameState + oGameState_Unk_6c, ~zenny);

    /* Store zenny XOR the global key into the encrypted wallet pointer. */
    u32 key = LD32(A_dword_2000060);
    ST32(LD32(R10 + oToolkit_Unk2005028_Ptr), zenny ^ key);

    RETURN_TO_CALLER();
}

/* ── getBattleSettingsFromList0 ── */
/* getBattleSettingsFromList0(r0 = index): return a pointer to entry `index`
 * (16 bytes each) of battleSettingsList0. Clobbers r1 like the original. */
Next getBattleSettingsFromList0(void) {
    R1 = (u32)R0 * 0x10;
    R0 = A_BattleSettings + R1;
    RETURN_TO_CALLER();
}

/* ── nullsub_8005B6A ── */
/* nullsub_8005B6A: an empty stub; it preserves r5 and does nothing else. */
Next nullsub_8005B6A(void) {
    RETURN_TO_CALLER();
}

/* ── object_Clear3RAMBytes_800371A ── */
/* object_Clear3RAMBytes_800371A:
 * zero out the three single-byte flags used to track object state. */
Next object_Clear3RAMBytes_800371A(void) {
    ST8(A_byte_2036778, 0);
    ST8(A_dword_203CA7C, 0);
    ST8(A_byte_2036830, 0);
    RETURN_TO_CALLER();
}

/* ── relatedToIsBattleOver_800A11C ── */
/* relatedToIsBattleOver_800A11C(r5 = battle object, r10 = toolkit):
 * An actor has finished (or is leaving) the battle: decrement its alliance's
 * member-count byte in the battle state, then remove it from the list of
 * alive actors that still get their turn. */
Next relatedToIsBattleOver_800A11C(void) {
    u32 *battleState = (u32 *)LD32(R10 + oToolkit_BattleStatePtr);
    u32 countSlot = (u32)LD8(R5 + oBattleObject_Alliance) + oBattleState_Unk_12;

    /* One fewer member of this actor's alliance. */
    ST8((u32)battleState + countSlot, (u8)LD8((u32)battleState + countSlot) - 1);

    /* Drop this actor from every alive-actor slot it occupies. */
    u32 *slot = (u32 *)((u32)battleState + oBattleState_AliveBattleActors);
    u32 i;
    for (i = 0; i < 8; i++) {
        if (*slot == R5)
            *slot = 0;
        slot++;
    }

    R2 = i;                 /* loop counter ends at 8 */
    SET_FLAGS_CMP(i, 8);
    RETURN_TO_CALLER();
}

/* ── screenFade_80062C8 ── */
/* screenFade_80062C8: read the current screen fade state.
 * eScreenFade is the fade controller struct; its +1 byte is the fade mode
 * (returned in r0) and its +3 byte is the current fade level (returned in r1). */
Next screenFade_80062C8(void) {
    u32 fade = A_eScreenFade;

    R1 = LD8(fade + 3); /* current fade intensity */
    R0 = LD8(fade + 1); /* fade mode/step */

    RETURN_TO_CALLER();
}

/* ── setTwoStructs_800A840 ── */
/* setTwoStructs_800A840(r0 = battle result byte, r10 = toolkit):
 * Writes the byte into the current battle state, then updates the battle
 * result via eStruct200A008_setBattleResult. Returns that function's r1. */
Next setTwoStructs_800A840(void) {
    u32 resultByte = R0;

    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    ST8(battleState + oBattleState_Unk_1f, resultByte);

    /* eStruct200A008_setBattleResult takes the same byte in r0 and reads
     * the toolkit from r10; both are already in place. Its result in r1
     * is passed through to the caller. */
    GUEST_CALL(eStruct200A008_setBattleResult);

    RETURN_TO_CALLER();
}

/* ── spawnMegaMan_80073CC ── */
/* spawnMegaMan_80073CC(r6 = *EnemySetup):
 * Spawn Mega Man (via sub_800753C) at the panel encoded in the setup's
 * position byte -- low 3 bits are the panel x, bits 4+ the panel y -- on
 * the side of the field given by the setup's alliance flag (bit 0).
 * Flags on exit are whatever sub_800753C leaves. */
Next spawnMegaMan_80073CC(void) {
    u32 self = R6;

    u32 pos = LD8(self + 1);
    u32 panel_x = pos & 7;
    u32 panel_y = pos >> 4;

    u32 alliance = LD8(self) & 1;

    R0 = panel_x;
    R1 = panel_y;
    R2 = alliance;
    R3 = 1;
    GUEST_CALL(sub_800753C);

    RETURN_TO_CALLER();
}

/* ── scenario_initEffectState_8003962 ── */
/* scenario_initEffectState_8003962: initialize the scenario effect state block at 0x2000780.
 * If the block is not already initialized (byte at +2 is zero), clear its
 * 0x48 bytes, then mark it initialized by storing 1 at byte +3. */
Next scenario_initEffectState_8003962(void) {
    u32 state = A_eScenarioEffectState2000780;

    if (LD8(state + 2) == 0) {
        R0 = state;
        R1 = 0x48;
        GUEST_CALL(ZeroFillByWord);
    }

    ST8(state + 3, 1);
    RETURN_TO_CALLER();
}

/* ── scenario_ClearEffectActiveFlag_800399A ── */
/* scenario_ClearEffectActiveFlag_800399A: clear the top bit of the scenario effect state byte, keeping
 * the low 7 bits (the effect id/state) intact. Clobbers r0/r1 only; the
 * original's r4-r7 push/pop just preserves them, which locals already do. */
Next scenario_ClearEffectActiveFlag_800399A(void) {
    u32 state = A_eScenarioEffectState2000780;

    ST8(state, LD8(state) & 0x7F);

    RETURN_TO_CALLER();
}

/* ── battle_ClearEStruct2001010_8003A58 ── */
/* battle_ClearEStruct2001010_8003A58: clear the eStruct2001010 battle work area (0x48 bytes).
 * Takes no arguments and returns nothing; r2-r7 are untouched. */
Next battle_ClearEStruct2001010_8003A58(void) {
    R0 = A_eStruct2001010;
    R1 = 0x48;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── event_ackNewFlagByte_8003AEA ── */
/* event_ackNewFlagByte_8003AEA: clear bit 7 of the event-status byte at eStruct2001010.
 * This acknowledges the pending event flag so it no longer reads as "new".
 * Returns r1 = 0x7F (the mask used) with nz flags set from the masked byte. */
Next event_ackNewFlagByte_8003AEA(void) {
    u32 flags = LD8(A_eStruct2001010) & 0x7F;
    ST8(A_eStruct2001010, flags);
    R1 = 0x7F;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── sub_8003C70 ── */
/* sub_8003C70: reset the something tracked by sub_80028C0 (r0 = 0 as its argument). */
Next sub_8003C70(void) {
    R0 = 0;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── sub_8004298 ── */
/* sub_8004298: invoke sub_80028C0 with argument 3 and return.
 * (The mov r0, #0x30 / ldr r3 bytes after the pop are dead code from a
 * previous tail of the function and are never executed.) */
Next sub_8004298(void) {
    R0 = 3;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── sub_8004590 ── */
/* sub_8004590: wrapper that invokes sub_80028C0 with argument 4 (tail call:
 * returns directly to our caller, LR is passed through unchanged). */
Next sub_8004590(void) {
    R0 = 4;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── gameMode_switchToMode2_80046F8 ── */
/* gameMode_switchToMode2_80046F8: switch the game into mode 2 by calling sub_80028C0.
 * No inputs; sub_80028C0 receives r0 = 2. */
Next gameMode_switchToMode2_80046F8(void) {
    R0 = 2;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── sub_80049B0 ── */
/* sub_80049B0: invoke sub_80028C0 with argument 5; result comes back in r2. */
Next sub_80049B0(void) {
    R0 = 5;
    GUEST_CALL(sub_80028C0);
    RETURN_TO_CALLER();
}

/* ── warp_ArmPendingTransition_8005EEC ── */
/* warp_ArmPendingTransition_8005EEC(r0 = warp data ptr, r1 = warp index, r2 = map group transition type,
 *              r10 = toolkit):
 * Arm the warp state machine: point it at the target warp's data, record which
 * warp within the group to load, and what kind of map-group transition to play. */
Next warp_ArmPendingTransition_8005EEC(void) {
    u32 warp = LD32(R10 + oToolkit_Warp2011bb0_Ptr);

    ST8(warp + oWarp2011bb0_Unk_10, 1);
    ST8(warp + oWarp2011bb0_WarpIndex, R1 + 1);
    ST32(warp + oWarp2011bb0_WarpDataPtr, R0);
    ST8(warp + oWarp2011bb0_MapGroupTransitionType, R2);

    RETURN_TO_CALLER();
}

/* ── warp_LoadCurrentAndReadUnk10_8005F28 ── */
/* warp_LoadCurrentAndReadUnk10_8005F28: load the warp helper struct for the current area (found via the
 * toolkit in r10) and fetch its byte at oWarp2011bb0_Unk_10.
 * Returns r0 = warp struct pointer, r1 = that byte; flags reflect (r1 - 0),
 * so callers typically test whether the byte is zero. */
Next warp_LoadCurrentAndReadUnk10_8005F28(void) {
    R0 = LD32(R10 + oToolkit_Warp2011bb0_Ptr);
    R1 = LD8(R0 + oWarp2011bb0_Unk_10);
    SET_FLAGS_CMP(R1, 0);
    RETURN_TO_CALLER();
}

/* ── runSetupStepAndClearWorkArea_8005F6C ── */
/* runSetupStepAndClearWorkArea_8005F6C: run the sub_802FDB0 setup step, then wipe the associated
 * work memory via cleareMemory_802FF2C. r2/r3 and the overflow flag are
 * whatever cleareMemory_802FF2C leaves behind (callers rely on this). */
Next runSetupStepAndClearWorkArea_8005F6C(void) {
    GUEST_CALL(sub_802FDB0);
    GUEST_CALL(cleareMemory_802FF2C);
    RETURN_TO_CALLER();
}

/* ── sub_8005F78 ── */
/* sub_8005F78: reset/clear routine — runs two memory-clearing helpers in
 * sequence and returns. r1-r3 and the V flag pass through from the last
 * callee; r4-r7 are preserved across the call. */
Next sub_8005F78(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(loc_802FDB4);
    GUEST_CALL(cleareMemory_802FF2C);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_80062D0 ── */
/* sub_80062D0: entry stub into the shared routine at loc_80062DA.
 *
 * It saves r5/lr with push {r5,lr} purely so that the shared routine's
 * epilogue (pop {r5,pc}) has matching values on the stack: r5 comes back
 * unchanged and the return goes to this function's caller. The only real
 * work here is setting r0 = 0 before branching; r3 and r5 pass through
 * untouched, everything else is scratch per the contract. */
Next sub_80062D0(void) {
    u32 ret = LR;

    /* push {r5,lr} — the shared epilogue pops these back */
    SP -= 8;
    ST32(SP, R5);
    ST32(SP + 4, ret);

    R0 = 0;
    GUEST_CALL(loc_80062DA);
    RETURN_TO_CALLER();
}

/* ── sub_80062EC ── */
/* sub_80062EC: run the two sub-steps sub_80062D0 and sub_80062D6 in order.
 * All caller-visible results (flags, r1, r2, r3, r5) come from the second call. */
Next sub_80062EC(void) {
    GUEST_CALL(sub_80062D0);
    GUEST_CALL(sub_80062D6);
    RETURN_TO_CALLER();
}

/* ── screenFade_InitFadeBlock_800634C ── */
/* screenFade_InitFadeBlock_800634C(r0 = fade block selector): initialize one screen-fade state block.
 *
 * The caller passes an offset into the eScreenFade array (block index * 0x10).
 * The high nibble of r0 is the fade's step count and is stored as the block's
 * speed field; the rest of the block's state bytes are cleared so the fade
 * starts over from the beginning.
 *
 * Outputs: r2 = r0 >> 4. r0 is zeroed and r5 is restored by the epilogue. */
Next screenFade_InitFadeBlock_800634C(void) {
    u32 savedR5 = R5;
    u32 sel = R0;

    u32 fade = A_eScreenFade + sel;

    R2 = sel >> 4;              /* fade step count */
    ST8(fade + 0xC, R2);        /* speed/steps field */

    ST8(fade + 3, 0);           /* frame counter */
    ST8(fade + 0, 0);           /* current step */
    ST8(fade + 2, 0);           /* fade direction/state */
    ST8(fade + 0xD, 0);         /* pending flag */

    R0 = 0;                     /* epilogue leaves r0 = 0 */
    R5 = savedR5;               /* pop {r5,pc} restores the caller's r5 */

    RETURN_TO_CALLER();
}

/* ── sub_8006910 ── */
/* sub_8006910: store 0x80 (a "busy/locked" flag) into the byte at 20081B0.
 * Also leaves r0 = 0x80 and r1 = the byte's address, with NZ flags set
 * from r0 (non-zero, positive) as the MOV does. */
Next sub_8006910(void) {
    R1 = A_byte_20081B0;
    R0 = 0x80;
    SET_FLAGS_NZ(R0);
    ST8(R1, R0);
    RETURN_TO_CALLER();
}

/* ── gameState_GetUnk74_8006DEC ── */
/* gameState_GetUnk74_8006DEC(): return the current game state's field at oGameState_Unk_74.
 * Inputs: none (R10 = toolkit pointer).
 * Outputs: r0 = game state -> Unk_74. Preserves r4-r7. */
Next gameState_GetUnk74_8006DEC(void) {
    u32 *toolkit = (u32 *)R10;
    u32 *gameState = (u32 *)LD32((u32)toolkit + oToolkit_GameStatePtr);
    R0 = LD32((u32)gameState + oGameState_Unk_74);
    RETURN_TO_CALLER();
}

/* ── battle_SpawnEnemyObjects_8007358 ── */
/* battle_SpawnEnemyObjects_8007358: spawn the enemy objects for this battle.
 * Reads the enemy setup array from the current battle settings (via the battle
 * state in r5) and creates one battle object per entry, then runs the post-spawn
 * setup routine. All registers except r0/r1 are left untouched. */
Next battle_SpawnEnemyObjects_8007358(void) {
    u32 settings = LD32(R5 + oBattleState_BattleSettings);
    R0 = LD32(settings + oBattleSettings_EnemySetupArrPtr);

    GUEST_CALL(SpawnBattleObjectUsingBattleEntityConfig_8007368);
    GUEST_CALL(sub_80AA88C);

    RETURN_TO_CALLER();
}

/* ── battle_cueThenSetState4_800799C ── */
/* battle_cueThenSetState4_800799C (r5 = battle state):
 * Play the cue handled by sub_801FE24, then mark the battle state by
 * storing 4 into its byte at offset 2. Flags are left as the final
 * "mov r0, #4" leaves them (N/Z from the value 4, C/V untouched). */
Next battle_cueThenSetState4_800799C(void) {
    GUEST_CALL(sub_801FE24);

    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 2, R0);

    RETURN_TO_CALLER();
}

/* ── battle_stateCountdownTick_8007BD0 ── */
/* battle_stateCountdownTick_8007BD0: battle-state countdown tick.
 * r5 = battle state object. If its phase byte (offset 3) is still zero,
 * start a 10-frame countdown and advance the phase to 4. Otherwise count
 * the halfword at oBattleState_Unk_28 down by one; when it underflows,
 * write 4 to the halfword at offset 2 (oBattleState_Unk_02_03).
 * R0 ends as 4 when the countdown was started or finished, otherwise as
 * the decremented timer value. Flags come from the last flag-setting
 * instruction of the taken path: MOVS #4 (nz only) on the start/expire
 * paths, and the SUBS #1 on the still-counting path. */
Next battle_stateCountdownTick_8007BD0(void) {
    if (LD8(R5 + 3) != 0) {
        /* countdown already running: tick it down once */
        u32 prev = LD16(R5 + oBattleState_Unk_28);
        u32 timer = prev - 1;
        ST16(R5 + oBattleState_Unk_28, timer);
        if ((s32)timer >= 0) {
            /* still counting: flags are those of the subtract */
            R0 = timer;
            SET_FLAGS_CMP(prev, 1);
        } else {
            /* expired: switch the state's phase word to 4 */
            ST16(R5 + 2, 4);
            R0 = 4;
            SET_FLAGS_CMP(prev, 1); /* c/v from the subtract */
            SET_FLAGS_NZ(R0);       /* nz from the final MOVS #4 */
        }
    } else {
        /* not started yet: set timer to 10 and enter phase 4 */
        ST16(R5 + oBattleState_Unk_28, 0xA);
        R0 = 4;
        ST8(R5 + 3, R0);
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── fifo_pushRecord16_8009FF8 ── */
/* fifo_pushRecord16_8009FF8(r0..r3 = four words of a record):
 * Append one 16-byte record to the FIFO at dword_3002180. The write slot is
 * selected by the counter word at byte_3000EA8, which is then incremented
 * (truncated to 28 bits by the original's lsl/lsr pair, so only the low
 * 28 bits ever wrap/grow).
 * r4/r6/r7 are saved and restored by the original, so they are preserved.
 * The flags are those of `adds r4, #1` on the new count (callers test it). */
Next fifo_pushRecord16_8009FF8(void) {
    u32 count = LD32(A_byte_3000EA8);
    u32 slot = A_dword_3002180 + (count << 4);

    ST32(slot + 0x00, R0);
    ST32(slot + 0x04, R1);
    ST32(slot + 0x08, R2);
    ST32(slot + 0x0C, R3);

    /* lsl r4,#4 / lsr r4,#4: keep only the low 28 bits of the counter */
    count = (count << 4) >> 4;
    SET_FLAGS_ADD(count, 1);
    count += 1;
    ST32(A_byte_3000EA8, count);

    RETURN_TO_CALLER();
}

/* ── sub_800A01C ── */
/* sub_800A01C: clear the flag/state word at byte_3000EA8.
 * Returns r0 = 0, r1 = &byte_3000EA8, with Z set (result of "mov r0, #0"). */
Next sub_800A01C(void) {
    R0 = 0;
    R1 = A_byte_3000EA8;
    ST32(R1, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── clearBattleCounters_800A0C6 ── */
/* clearBattleCounters_800A0C6: clear the two 8-byte counter pairs at dword_2036820. */
Next clearBattleCounters_800A0C6(void) {
    ST32(A_dword_2036820, 0);
    ST32(A_dword_2036820 + 4, 0);
    ST32(A_dword_2036820 + 8, 0);
    ST32(A_dword_2036820 + 0xC, 0);
    RETURN_TO_CALLER();
}

/* ── input_recordStateChangeForEntry_800A0D6 ── */
/* input_recordStateChangeForEntry_800A0D6(r0 = entry index, r1 = new value):
 * Updates one 8-byte entry of the dword_2036820 array (entry size 8, so the
 * index is scaled by 3):
 *   +2: previous value, replaced by r1
 *   +4: bits that changed 0 -> 1 (newly set)
 *   +6: bits that changed 1 -> 0 (newly cleared)
 * Leaves r2 = newly cleared bits, r3 = entry address, and c/v from the
 * address computation (ADDS base + index*8). */
Next input_recordStateChangeForEntry_800A0D6(void) {
    u32 entry = A_dword_2036820 + (R0 << 3);
    SET_FLAGS_ADD(A_dword_2036820, R0 << 3);
    R3 = entry;

    u32 prev = LD16(entry + 2);
    ST16(entry + 2, R1);            /* store the new value */
    ST16(entry + 4, ~prev & R1);    /* bits newly set */
    prev &= ~R1;
    ST16(entry + 6, prev);          /* bits newly cleared */
    R2 = prev;

    RETURN_TO_CALLER();
}

/* ── battle_DecrementAllianceCounterSlot4_800A104 ── */
/* battle_DecrementAllianceCounterSlot4_800A104(r5 = battle object, r10 = toolkit):
 * Count down one of the alliance counters in the battle state (the slot
 * selected by this object's alliance + 4), but only while the AI data's
 * byte at offset oAIData_Unk_02 is zero. The decremented value is left
 * in r2; if the early-out is taken, r2 is left untouched. */
Next battle_DecrementAllianceCounterSlot4_800A104(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    if (LD8(aiData + 2 /* oAIData_Unk_02 */) == 0) {
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        u32 slot = LD8(R5 + oBattleObject_Alliance) + 4;

        u32 count = LD8(battleState + slot) - 1;
        ST8(battleState + slot, count);

        R2 = count;
    }

    RETURN_TO_CALLER();
}

/* ── battle_getStateUnk40_800A704 ── */
/* battle_getStateUnk40_800A704: return the value stored at oBattleState_Unk_40 of the
 * current battle state (accessed through the r10 toolkit pointer). */
Next battle_getStateUnk40_800A704(void) {
    R0 = LD32(LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_Unk_40);
    RETURN_TO_CALLER();
}

/* ── sfx_playSfx0C_800A79C ── */
/* sfx_playSfx0C_800A79C: play sound effect #0x0C (battle_SetStateUnk5c_80013A2 is the sfx player). */
Next sfx_playSfx0C_800A79C(void) {
    R0 = 0xC;
    GUEST_CALL(battle_SetStateUnk5c_80013A2);
    RETURN_TO_CALLER();
}

/* ── battle_getStateByte0d_800A7E2 ── */
/* battle_getStateByte0d_800A7E2: return the byte at oBattleState_Unk_0d of the current battle state
 * (r10 = toolkit). */
Next battle_getStateByte0d_800A7E2(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD8(battleState + oBattleState_Unk_0d);
    RETURN_TO_CALLER();
}

/* ── battle_getStateByteNibbles_800A832 ── */
/* battle_getStateByteNibbles_800A832: read the battle state's Unk_1f byte and split it into nibbles.
 * Outputs: r0 = low nibble, r1 = high nibble, r2 = 0xF;
 * nz from the low nibble, c from bit 3 of the original byte (the LSR carry-out). */
Next battle_getStateByteNibbles_800A832(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 stateByte = LD8(battleState + oBattleState_Unk_1f);

    R1 = stateByte >> 4;
    R0 = stateByte & 0xF;
    R2 = 0xF;

    SET_FLAGS_NZ(R0);
    cpu.c = (stateByte >> 3) & 1;

    RETURN_TO_CALLER();
}

/* ── sub_800A86E ── */
/* sub_800A86E: scan the current battle state's 8 "alive battle actor" slots.
 * Every slot is read: r3 ends up holding the raw contents of the last slot
 * (0 if empty), r2 the AI-data pointer of the last occupied slot (or its
 * incoming value if none was occupied), and r0/r1 the unk_0e byte / HP of
 * that actor (HP is skipped when unk_0e is 0xFF). r4 and r7 are preserved;
 * the flags left behind are those of the final loop-counter compare (8 vs 8). */
Next sub_800A86E(void) {
    u32 slotPtr = LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_AliveBattleActors;
    u32 actor;
    u32 aiData = R2; /* r2 keeps its incoming value if no slot is occupied */

    for (u32 i = 0; i < 8; i++) {
        actor = LD32(slotPtr);
        if (actor != 0) {
            aiData = LD32(actor + oBattleObject_AIDataPtr);
            R0 = LD8(aiData + oAIData_Unk_0e);
            if (R0 != 0xFF)
                R1 = LD16(actor + oBattleObject_HP);
        }
        slotPtr += 4;
    }

    R2 = aiData;
    R3 = actor;             /* contents of the last slot, occupied or not */
    SET_FLAGS_CMP(8, 8);    /* cmp r4, #8 with r4 == 8 at loop exit */
    RETURN_TO_CALLER();
}

/* ── battle_testFlag0x40_800A8F8 ── */
/* battle_testFlag0x40_800A8F8: test battle flag 0x40 and return the result.
 * r0 = 1 if the flag is set, 0 if not; nz/c/v reflect the final `mov r0, r4`
 * (encoded as adds r0, r4, #0, so C and V are cleared). */
Next battle_testFlag0x40_800A8F8(void) {
    u32 result;

    GUEST_CALL(TestBattleFlag_0x40); /* answers in Z; the original branches on it */

    result = ZERO_FLAG() ? 0 : 1;
    R0 = result;
    SET_FLAGS_ADD(result, 0);

    RETURN_TO_CALLER();
}

/* ── sub_800A954 ── */
/* sub_800A954: run initIdListStruct_800A964 over each of the two work buffers at
 * byte_20349C0 and byte_2034A10. r2/r3/r4 pass through untouched. */
Next sub_800A954(void) {
    R0 = A_byte_20349C0;
    GUEST_CALL(initIdListStruct_800A964);

    R0 = A_byte_2034A10;
    GUEST_CALL(initIdListStruct_800A964);

    RETURN_TO_CALLER();
}

/* ── initIdListStruct_800A964 ── */
/* initIdListStruct_800A964(r0 = struct pointer):
 * Initialize a record list structure: clear the whole 0xC8-byte block to zero,
 * then mark the 12-byte ID array at offset 2 as empty (0xFF). */
Next initIdListStruct_800A964(void) {
    u32 mem = R0;
    u32 savedR4 = R4;

    /* ZeroFillByWord(mem, 0x50 words = 0xC8 bytes) */
    R1 = 0x50;
    GUEST_CALL(ZeroFillByWord);

    /* ByteFill(mem + 2, 12, 0xFF): every entry id starts as "unused" */
    R0 = mem + 2;
    R1 = 0xC;
    R2 = 0xFF;
    GUEST_CALL(ByteFill);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_GetObjectByteAt14_800A9E2 ── */
/* battle_GetObjectByteAt14_800A9E2(r0 = battle object index, r10 = toolkit):
 * returns the byte at battleState + 0x14 + r0 (an object stat the caller
 * wants to inspect); r3 = battle state pointer, flags from the index add. */
Next battle_GetObjectByteAt14_800A9E2(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    R3 = battle;
    R0 += 0x14;
    SET_FLAGS_ADD(R0, 0x14);
    R0 = LD8(battle + R0);
    RETURN_TO_CALLER();
}

/* ── sub_800AA06 ── */
/* sub_800AA06(r0 = handle passed through to getNextEntryAddress_800AAAE, r5 = expected id):
 * Asks getNextEntryAddress_800AAAE (called with r0 unchanged) for a pointer, then checks
 * whether the id field at that pointer matches r5. Returns r0 = 1 on match,
 * 0 otherwise (also left in r2, with flags from the move). */
Next sub_800AA06(void) {
    GUEST_CALL(getNextEntryAddress_800AAAE);

    u32 match = 0;
    if (LD32(R0) == R5)
        match = 1;

    R2 = match;
    R0 = match;
    SET_FLAGS_ADD(match, 0);
    RETURN_TO_CALLER();
}

/* ── getNextEntryAddress_800AAAE ── */
/* getNextEntryAddress_800AAAE(r0 = element index):
 * Computes the address of entry `r0` in the array at unk_2038170, whose
 * elements are 0x20 bytes each. Returns the address in r0 and the array
 * base in r1 (callers use r1 as the read/write base register). */
Next getNextEntryAddress_800AAAE(void) {
    u32 base = A_unk_2038170;

    R0 = base + R0 * 0x20;
    R1 = base;
    RETURN_TO_CALLER();
}

/* ── sub_800AAF2 ── */
/* sub_800AAF2:
 * Passes 0 in r0 to clearIndexEntryPair_800AAFC and tail-returns its results
 * (r2/r3/r4 flow straight out of the callee). */
Next sub_800AAF2(void) {
    R0 = 0;
    GUEST_CALL(clearIndexEntryPair_800AAFC);
    RETURN_TO_CALLER();
}

/* ── clearWorkArea_2000FA0_800AB22 ── */
/* clearWorkArea_2000FA0_800AB22: clear word_2000FA0 (0x1A halfwords, i.e. 52 bytes).
 * R2/R3 pass through untouched; flags come from ZeroFillByHalfword. */
Next clearWorkArea_2000FA0_800AB22(void) {
    R0 = A_word_2000FA0;
    R1 = 0x1A;
    GUEST_CALL(ZeroFillByHalfword);
    RETURN_TO_CALLER();
}

/* ── table_GetByte16Row_203EAE0_800AB3A ── */
/* table_GetByte16Row_203EAE0_800AB3A(r0 = row, r1 = column):
 * read one byte from a 16-byte-per-row table at byte_203EAE0.
 * Returns the byte in r0 and the table address of that byte in r3. */
Next table_GetByte16Row_203EAE0_800AB3A(void) {
    u32 row = R0, column = R1;
    u32 entry = A_byte_203EAE0 + row * 0x10;
    R0 = LD8(entry + column);
    R3 = entry;
    RETURN_TO_CALLER();
}

/* ── tableAddSaturatedByte_800AB46 ── */
/* tableAddSaturatedByte_800AB46(r0 = row, r1 = column, r2 = addend):
 * Adds r2 to one byte of the table at byte_203EAE0 (16-byte rows selected
 * by r0, byte within the row by r1), saturating the result at 0xFF.
 * Sets c/v from the CMP against 0xFF and leaves the row base address in r3. */
Next tableAddSaturatedByte_800AB46(void) {
    u32 entry = A_byte_203EAE0 + R0 * 0x10;

    u32 sum = LD8(entry + R1) + R2;
    SET_FLAGS_CMP(sum, 0xFF);
    if ((s32)sum > 0xFF)
        sum = 0xFF;

    ST8(entry + R1, sum);
    R3 = entry;
    RETURN_TO_CALLER();
}

/* ── battle_bumpTimerAndReloadSubcount_800AE44 ── */
/* battle_bumpTimerAndReloadSubcount_800AE44: advance the battle timer — bump the byte at Unk_1c and
 * reload the timeout counter at Unk_1d with 10. r10 = toolkit pointer. */
Next battle_bumpTimerAndReloadSubcount_800AE44(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);

    ST8(battleState + oBattleState_Unk_1c, LD8(battleState + oBattleState_Unk_1c) + 1);
    ST8(battleState + oBattleState_Unk_1d, 10);

    /* output: r1 = battle state pointer (the original keeps r1 live) */
    R1 = battleState;
    RETURN_TO_CALLER();
}

/* ── sub_800B460 ── */
/* sub_800B460: zero the 32-bit values at 0x203F4A0 and 0x203F5A0. */
Next sub_800B460(void) {
    ST32(A_dword_203F4A0, 0);
    ST32(A_dword_203F5A0, 0);
    RETURN_TO_CALLER();
}

/* ── battle_doBothTrackedIdsMatch_800B46C ── */
/* battle_doBothTrackedIdsMatch_800B46C(r0 = id): r0 = 1 if both tracked ids (dword_203F4A0 and
 * dword_203F5A0) equal the given id, 0 otherwise.
 * Also hands the loaded values back in r1/r2 and the id in r3; flags are
 * left from the last comparison performed, so callers may branch on them. */
Next battle_doBothTrackedIdsMatch_800B46C(void) {
    u32 id = R0;
    u32 idA = LD32(A_dword_203F4A0);
    u32 idB = LD32(A_dword_203F5A0);

    R3 = id;
    R1 = idA;
    R2 = idB;

    if (idA != id) {
        R0 = 0;
        SET_FLAGS_CMP(idA, id);
    } else if (idB != id) {
        R0 = 0;
        SET_FLAGS_CMP(idB, id);
    } else {
        R0 = 1;
        SET_FLAGS_CMP(idB, id);
    }
    RETURN_TO_CALLER();
}

/* ── sub_800B75A ── */
/* sub_800B75A: store 0xFF (a "cancel/back out" flag?) into byte_203C960. */
Next sub_800B75A(void) {
    ST8(A_byte_203C960, 0xFF);
    RETURN_TO_CALLER();
}

/* ── warp_getCurrentWarpEntry_8005f32 ── */
/* warp_getCurrentWarpEntry_8005f32: fetch the current warp entry from the warp state (r10 = toolkit).
 * Returns r0 = pointer to the warp data table,
 *         r1 = warp index - 1 (index of the warp being transitioned out of),
 *         r2 = map group transition type. */
Next warp_getCurrentWarpEntry_8005f32(void) {
    u32 warpState = LD32(R10 + oToolkit_Warp2011bb0_Ptr);

    R0 = LD32(warpState + oWarp2011bb0_WarpDataPtr);
    R1 = LD8(warpState + oWarp2011bb0_WarpIndex) - 1;
    R2 = LD8(warpState + oWarp2011bb0_MapGroupTransitionType);

    RETURN_TO_CALLER();
}

/* ── warp_setSubsystemIndexTo0x10AndOthers_8005f00 ── */
/* warp_setSubsystemIndexTo0x10AndOthers_8005f00:
 * Prepares a warp: runs the pre-warp helper (fade-out etc., seeing the
 * caller's arguments unchanged), switches the game state's subsystem index
 * (first byte of the GameState struct) to 0x10 — the warp subsystem — and
 * then hands control to the subsystem runner with R0 = 0x10 and
 * R4 = game state. r4-r7 are preserved for the caller. */
Next warp_setSubsystemIndexTo0x10AndOthers_8005f00(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* pre-warp processing; runs with the caller's registers intact */
    GUEST_CALL(warp_ArmPendingTransition_8005EEC);

    /* point the game state at subsystem 0x10 */
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    R4 = gameState;
    R0 = 0x10;
    ST8(gameState, 0x10);   /* GameState->subsystemIndex (offset 0) */

    /* run the (now selected) subsystem */
    GUEST_CALL(timer_tickCountdownTable_8035738);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── zeroFill_800AB70 ── */
/* zeroFill_800AB70: clear the 0x20-byte block at byte_203EAE0.
 * r2/r3 are passed through untouched. */
Next zeroFill_800AB70(void) {
    R0 = A_byte_203EAE0;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── BasicBattleObjectInitialize ── */
/* BasicBattleObjectInitialize(r0 = object, r1 = panel X, r2 = panel Y, r3 = element,
 *                             r5 = related object, r6 = damage/stamina-counter disabler):
 * fill in the basic fields of a battle object from the caller's arguments.
 * The object's alliance/flip setting is copied from the related object, and the
 * loaded value is returned in r1. */
Next BasicBattleObjectInitialize(void) {
    u32 object = R0;

    ST8(object + oBattleObject_PanelX, R1);
    ST8(object + oBattleObject_PanelY, R2);
    ST8(object + oBattleObject_Element, R3);
    ST32(object + oBattleObject_DamageAndStaminaDamageCounterDisabler, R6);

    R1 = LD16(R5 + oBattleObject_AllianceAndDirectionFlip);
    ST16(object + oBattleObject_AllianceAndDirectionFlip, R1);

    ST32(object + oBattleObject_RelatedObject1Ptr, R5);
    RETURN_TO_CALLER();
}

/* ── ClearAIData_Unk_44_Flag ── */
/* ClearAIData_Unk_44_Flag(r0 = mask, r5 = battle object):
 * clear the bits of `mask` in the AI data's Unk_44 field.
 * Leaves r1 = updated field value, r3 = AI data pointer, NZ set on r1. */
Next ClearAIData_Unk_44_Flag(void) {
    u32 *aiData = (u32 *)LD32(R5 + oBattleObject_AIDataPtr);
    u32 flags = LD32((u32)aiData + oAIData_Unk_44) & ~R0;
    ST32((u32)aiData + oAIData_Unk_44, flags);
    R1 = flags;
    R3 = (u32)aiData;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── ClearCustGauge ── */
/* ClearCustGauge: clear the custom gauge (zero its progress counter at
 * word_20352A0), then let battle_clearFlags reset the gauge-related flag
 * bits (0x12). Result registers come from battle_clearFlags. */
Next ClearCustGauge(void) {
    ST16(A_eStruct2035280 + 0x20, 0);

    R0 = 0x12;
    GUEST_CALL(battle_clearFlags);

    RETURN_TO_CALLER();
}

/* ── GetAIData_Unk_44_Flag ── */
/* GetAIData_Unk_44_Flag(r5 = battle object):
 * Loads the object's AI data pointer and returns its unknown-44 flag.
 * Outputs: r0 = oAIData_Unk_44 value, r3 = AI data pointer. */
Next GetAIData_Unk_44_Flag(void) {
    R3 = LD32(R5 + oBattleObject_AIDataPtr);
    R0 = LD32(R3 + oAIData_Unk_44);
    RETURN_TO_CALLER();
}

/* ── GetBattleNaviStatsByte ── */
/* GetBattleNaviStatsByte(r0 = alliance side flag, r1 = NaviStats byte offset):
 * Fetch the NaviStats record for the requested side and return the single
 * byte at the caller-chosen offset. r6 is preserved; r1 comes back unchanged
 * (GetBattleNaviStatsAddr does not touch it). */
Next GetBattleNaviStatsByte(void) {
    u32 offset = R1;

    GUEST_CALL(GetBattleNaviStatsAddr); /* (alliance: bool) -> * NaviStats */

    R0 = LD8(R0 + offset);
    RETURN_TO_CALLER();
}

/* ── GetBattleNaviStatsHword ── */
/* GetBattleNaviStatsHword(r0 = alliance, r1 = stat offset, r6 = preserved):
 * fetch a halfword from the current battle navi's NaviStats; r0 = the value.
 * The offset in r1 selects which stat (see NaviStats field layout). */
Next GetBattleNaviStatsHword(void) {
    u32 statOffset = R1;

    /* GetBattleNaviStatsAddr(alliance) -> pointer to the NaviStats struct */
    GUEST_CALL(GetBattleNaviStatsAddr);

    R0 = LD16(R0 + statOffset);
    RETURN_TO_CALLER();
}

/* ── GetCurPETNaviStatsByte ── */
/* GetCurPETNaviStatsByte(r0 = field offset selector for the PET's current navi):
 * Looks up which navi is currently selected in the PET, indexes the NaviStats
 * array with it, and reads one byte out of that navi's stats struct.
 *   r1 (in/out): byte offset within the NaviStats struct; preserved.
 *   r0 (out): the requested stats byte.
 *   r3 (out): address of the byte that was read.
 *   c, v (out): flags from the address computation (ADDS). */
Next GetCurPETNaviStatsByte(void) {
    u32 fieldOffset = R1;

    /* Ask which navi is active on the PET; returns its index in the stats array. */
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    u32 naviIndex = R0;
    R1 = fieldOffset; /* the helper clobbers r1; the original keeps it intact */

    /* Walk to the current navi's entry in the stats array. */
    u32 statsBase = LD32(R10 + oToolkit_NaviStatsPtr);
    u32 entryOffset = naviIndex * oNaviStats_Size;
    R3 = statsBase + entryOffset;
    SET_FLAGS_ADD(statsBase, entryOffset);

    R0 = LD8(R3 + fieldOffset);
    RETURN_TO_CALLER();
}

/* ── GetCurPETNaviStatsHword ── */
/* GetCurPETNaviStatsHword(r0 = which_navi, r1 = which_stat) -> r0:
 * Read one u16 stat of the navi currently selected on the PET.
 * Looks up the navi's stats-slot index, then reads the halfword at
 * NaviStatsPtr + index*oNaviStats_Size + which_stat.
 * Also leaves r3 = the address that was read from, and flags from the
 * final address addition (ADDS). */
Next GetCurPETNaviStatsHword(void) {
    u32 which_stat = R1; /* saved across the helper call, as the original does */

    /* r0 = stats index of the navi currently equipped in the PET */
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    R1 = which_stat;

    u32 stats_base = LD32(R10 + oToolkit_NaviStatsPtr);
    u32 slot_offset = R0 * oNaviStats_Size;
    u32 stat_addr = stats_base + slot_offset;
    SET_FLAGS_ADD(stats_base, slot_offset);

    R0 = LD16(stat_addr + which_stat);
    R3 = stat_addr;
    RETURN_TO_CALLER();
}

/* ── GetNaviStatsByte ── */
/* GetNaviStatsByte(r0 = record selector (booleanized: any nonzero value
 *                   selects the same record), r1 = field offset within the
 *                   navi stats record, r10 = toolkit):
 * reads one byte from the selected navi stats record; r0 = byte read,
 * r3 = address of that byte. */
Next GetNaviStatsByte(void) {
    u32 record = (R0 != 0) ? 1 : 0;      /* cmp r0, #0 / movne r0, #1 */
    u32 stats = LD32(R10 + oToolkit_NaviStatsPtr) + record * oNaviStats_Size;
    R3 = stats;
    R0 = LD8(stats + R1);
    RETURN_TO_CALLER();
}

/* ── GetNaviStatsIndexGivenCurPETNavi ── */
/* GetNaviStatsIndexGivenCurPETNavi(r0 = current PET navi ID):
 * Look up which Navi stats slot the currently selected PET navi uses.
 * Megaman maps to index 0, all other navis map to index 1. */
Next GetNaviStatsIndexGivenCurPETNavi(void) {
    R0 = LD8(A_CurPETNaviToNaviStatsIndexTable + R0);
    RETURN_TO_CALLER();
}

/* ── SetAIDataUnk0x48Flag ── */
/* SetAIDataUnk0x48Flag(r0 = flag bits, r5 = battle object):
 * set the given bits in the object's AI data word at offset 0x48.
 * Leaves r1 = updated word and r3 = AI data pointer for the caller. */
Next SetAIDataUnk0x48Flag(void) {
    u32 *aiData = (u32 *)LD32(R5 + oBattleObject_AIDataPtr);
    u32 flags = LD32((u32)aiData + oAIData_Unk_48) | R0;
    ST32((u32)aiData + oAIData_Unk_48, flags);
    R1 = flags;
    R3 = (u32)aiData;
    RETURN_TO_CALLER();
}

/* ── SetBattleNaviStatsByte ── */
/* SetBattleNaviStatsByte(r0 = alliance flag, r1 = NaviStats field offset,
 *                        r2 = byte value):
 * Writes one byte into the NaviStats record of the requested alliance,
 * e.g. updating a stat after a netbattle. r6/r7 are untouched (the
 * original pushes/pops them around the call). */
Next SetBattleNaviStatsByte(void) {
    u32 offset = R1;
    u32 value = R2;

    /* R0 already holds the alliance flag; returns the NaviStats pointer. */
    GUEST_CALL(GetBattleNaviStatsAddr);

    ST8(R0 + offset, value);
    RETURN_TO_CALLER();
}

/* ── SetBattleNaviStatsByte_AllianceFromBattleObject ── */
/* SetBattleNaviStatsByte_AllianceFromBattleObject(r1 = offset into NaviStats,
 * r2 = byte value, r5 = battle object):
 * Fetches the NaviStats record for the battle object's navi -- choosing
 * between the real one and its ghost copy via the object's alliance flag --
 * and stores one byte at the given offset in that record. */
Next SetBattleNaviStatsByte_AllianceFromBattleObject(void) {
    u32 statsOffset = R1;
    u32 value = R2;

    /* alliance flag (0 = player's own navi, nonzero = linked/ghost side)
     * selects which NaviStats record to edit */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(GetBattleNaviStatsAddr); /* r0 = &NaviStats */

    ST8(R0 + statsOffset, value);
    RETURN_TO_CALLER();
}

/* ── SetCurPETNaviStatsByte ── */
/* SetCurPETNaviStatsByte(r0 = stats selector, r1 = field offset, r2 = byte value):
 * Writes one byte into a NaviStats record. r0 is resolved by
 * GetNaviStatsIndexGivenCurPETNavi; the result selects the record
 * (recordOffset = result * oNaviStats_Size is added to the toolkit's
 * NaviStats base pointer), and value is stored at record + fieldOffset.
 * r1 and r2 pass through unchanged; flags reflect the pointer add. */
Next SetCurPETNaviStatsByte(void) {
    u32 fieldOffset = R1; /* byte offset within the NaviStats record */
    u32 value = R2;       /* byte to store */

    /* r0 already holds the caller's selector for the helper. */
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);

    u32 recordOffset = R0 * oNaviStats_Size;
    u32 statsBase = LD32(R10 + oToolkit_NaviStatsPtr);
    u32 record = statsBase + recordOffset;
    SET_FLAGS_ADD(statsBase, recordOffset);

    ST8(record + fieldOffset, value);

    R0 = recordOffset;
    R1 = fieldOffset;
    R2 = value;
    R3 = record;
    RETURN_TO_CALLER();
}

/* ── SetCurPETNaviStatsHword ── */
/* SetCurPETNaviStatsHword(r0 = navi id, r1 = field offset in oNaviStats, r2 = value,
 * r10 = toolkit):
 * Writes a halfword into the stats struct of the navi currently selected on the
 * PET. The stats array lives at toolkit->NaviStatsPtr; the entry for the current
 * PET navi is picked via GetNaviStatsIndexGivenCurPETNavi.
 * Leaves r0 = offset of that navi's stats entry, r3 = address written to,
 * flags set by the pointer computation (r1, r2 preserved). */
Next SetCurPETNaviStatsHword(void) {
    u32 fieldOffset = R1;
    u32 value = R2;

    /* Which slot in the stats array does the current PET navi occupy?
     * The callee clobbers r1/r2, so the caller saves them around the call. */
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    R1 = fieldOffset;
    R2 = value;

    u32 entryOffset = R0 * oNaviStats_Size;

    u32 statsBase = LD32(R10 + oToolkit_NaviStatsPtr);
    u32 entry = statsBase + entryOffset;

    ST16(entry + fieldOffset, value);

    R0 = entryOffset;
    R3 = entry;
    SET_FLAGS_ADD(statsBase, entryOffset);
    RETURN_TO_CALLER();
}

/* ── SetNaviStatsByte ── */
/* SetNaviStatsByte(r0 = record index (normalized to 0/1), r1 = field offset,
 * r2 = value, r10 = toolkit):
 * Write r2 into Toolkit->NaviStats[index].field[r1].
 * Leaves r0 = byte offset of the record (index * oNaviStats_Size) and
 * r3 = address of the written byte; c/v from the address addition. */
Next SetNaviStatsByte(void) {
    /* normalize the record index to a clean 0/1 */
    if (R0 != 0)
        R0 = 1;

    /* NaviStats is an array of oNaviStats_Size-byte records */
    R0 = R0 * oNaviStats_Size;
    R3 = LD32(R10 + oToolkit_NaviStatsPtr);

    R3 = R3 + R0;
    SET_FLAGS_ADD(R3, R0);

    ST8(R3 + R1, R2);
    RETURN_TO_CALLER();
}

/* ── ZeroAllNaviStatsMood ── */
/* ZeroAllNaviStatsMood: clear the Mood byte of every navi stats entry.
 * Calls SetNaviStatsByte(i, oNaviStats_Mood, 0) for i = 0..6.
 * r4 is preserved (pushed/popped by the original); the final flags come from
 * the loop's last comparison (cmp r4, #7 with r4 = 7), and r0-r3 hold whatever
 * the last SetNaviStatsByte call left in them. */
Next ZeroAllNaviStatsMood(void) {
    u32 navi;

    for (navi = 0; (s32)navi < 7; navi++) {
        R0 = navi;
        R1 = oNaviStats_Mood;
        R2 = 0;
        GUEST_CALL(SetNaviStatsByte);
    }

    SET_FLAGS_CMP(navi, 7);
    RETURN_TO_CALLER();
}

/* ── callPossiblyGetBattleEmotion_8015B54 ── */
/* callPossiblyGetBattleEmotion_8015B54: thin wrapper around
 * possiblyGetBattleEmotion_8015B64. All arguments and results pass straight
 * through in the registers (r0 = alliance flag in, r0..r3 = emotion /
 * transformation / mood / Beast-Out counter out); it exists only so callers
 * can tail-call the lookup without managing LR themselves. */
Next callPossiblyGetBattleEmotion_8015B54(void) {
    GUEST_CALL(possiblyGetBattleEmotion_8015B64);
    RETURN_TO_CALLER();
}

/* ── getBattleHandAddr_8010018 ── */
/* getBattleHandAddr_8010018(r0 = hand index):
 * returns the address of battle hand slot r0 * 0x50 bytes into byte_20349C0.
 * r0 = &byte_20349C0 + index * 0x50, r1 = &byte_20349C0. */
Next getBattleHandAddr_8010018(void) {
    R1 = A_byte_20349C0;
    R0 = R0 * 0x50 + R1;
    RETURN_TO_CALLER();
}

/* ── nullsub_10 ── */
/* nullsub_10: does nothing; kept as a placeholder so call sites stay addressable. */
Next nullsub_10(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_3 ── */
/* nullsub_3: empty placeholder function; does nothing. */
Next nullsub_3(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_42 ── */
/* nullsub_42: empty stub (does nothing). */
Next nullsub_42(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_45 ── */
/* nullsub_45: does nothing; presumably a stub for a removed feature call. */
Next nullsub_45(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_46 ── */
/* nullsub_46: 08013F80 THUMB.
 * Empty stub — the original is just `mov pc, lr`, so it has no effect.
 * Kept because callers still branch here (e.g. an optional hook point). */
Next nullsub_46(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_51 ── */
/* nullsub_51: does nothing; likely a stub replacing a removed routine. */
Next nullsub_51(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_58 ── */
/* nullsub_58: empty stub at 0x0801BFEC — does nothing. */
Next nullsub_58(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_6 ── */
/* nullsub_6: empty stub; does nothing and returns immediately. */
Next nullsub_6(void) {
    RETURN_TO_CALLER();
}

/* ── object_clearCollisionRegion ── */
/* object_clearCollisionRegion(r5 = battle object):
 * Zero the object's collision region byte (offset 1 in its collision data),
 * leaving the collision data pointer in r0 and 0 in r1
 * (flags set as by "mov r1, #0"). */
Next object_clearCollisionRegion(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    R0 = collisionData;
    R1 = 0;
    ST8(collisionData + 1, R1); /* oCollisionData_Region == 1 */

    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── object_clearFlag ── */
/* object_clearFlag(r0 = flag mask, r5 = battle object):
 * clear the given bits of the object's ObjectFlags1 in its collision data.
 * Returns r1 = collision data pointer, r2 = updated flags; NZ set on the flags value. */
Next object_clearFlag(void) {
    u32 flagsMask = R0;

    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags1);
    R2 &= ~flagsMask;
    SET_FLAGS_NZ(R2);
    ST32(R1 + oCollisionData_ObjectFlags1, R2);

    RETURN_TO_CALLER();
}

/* ── object_clearFlag2 ── */
/* object_clearFlag2(r0 = flag bits to clear, r5 = battle object):
 * Clears the given bits in the object's ObjectFlags2 field.
 * Leaves r1 = collision data pointer, r2 = new flags value, NZ from that value. */
Next object_clearFlag2(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 flags = LD32(collision + oCollisionData_ObjectFlags2) & ~R0;

    ST32(collision + oCollisionData_ObjectFlags2, flags);

    R1 = collision;
    R2 = flags;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── object_genericDestroy ── */
/* object_genericDestroy(r5 = object):
 * Generic battle-object destructor: release the object's reserved panel
 * tiles, its collision data, and finally the object memory itself.
 * Flags and r0-r3 are whatever object_freeMemory leaves behind. */
Next object_genericDestroy(void) {
    GUEST_CALL(RemoveBattleObjectPanelReserves);

    R0 = LD32(R5 + oBattleObject_CollisionDataPtr);
    GUEST_CALL(object_freeCollisionData);

    GUEST_CALL(object_freeMemory);
    RETURN_TO_CALLER();
}

/* ── object_getFlag ── */
/* object_getFlag(r5 = battle object):
 * returns the object's first collision flag word from its collision data. */
Next object_getFlag(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    R0 = LD32(collisionData + oCollisionData_ObjectFlags1);
    RETURN_TO_CALLER();
}

/* ── object_getFlag2 ── */
/* object_getFlag2: read the ObjectFlags2 field of the object's collision data.
 * Inputs:  r5 = battle object
 * Outputs: r0 = ObjectFlags2 */
Next object_getFlag2(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R0 = LD32(collision + oCollisionData_ObjectFlags2);
    RETURN_TO_CALLER();
}

/* ── object_removeCollisionData ── */
/* object_removeCollisionData (0x0801A00E):
 * THUMB trampoline into the ARM helper _object_removeCollisionData.
 * The helper is invoked with the caller's r4-r7 (and r10) still in place as
 * its arguments; the surrounding push/pop in the original exists only to
 * preserve r4-r7 and the original return address across the call, since the
 * ARM helper does not follow the standard callee-saved register discipline.
 * Results come back in r1-r3 and the condition flags; r4-r7 are unchanged. */
Next object_removeCollisionData(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    GUEST_CALL(_object_removeCollisionData);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── object_setAnimation ── */
/* object_setAnimation(r0 = animation id, r5 = battle object):
 * Start playing the given animation on the object: store it as the current
 * animation, then write 0xFF into the "queued animation" slot, which the
 * animation task picks up on its next update.
 * The call to nullsub_3 is an empty hook; it changes nothing, so the value
 * stored as the current animation is the animation id passed in r0.
 * Returns r0 = 0xFF (the value written to CurAnimCopy) and
 * r1 = the object's NameID, which the original loads before the hook call. */
Next object_setAnimation(void) {
    u32 anim = R0;
    u32 obj = R5;

    R1 = LD16(obj + oBattleObject_NameID);

    ST8(obj + oBattleObject_CurAnim, anim);
    ST8(obj + oBattleObject_CurAnimCopy, 0xFF);

    R0 = 0xFF;
    RETURN_TO_CALLER();
}

/* ── object_setCollisionHitEffect ── */
/* object_setCollisionHitEffect(r0 = hit effect, r5 = battle object):
 * store the hit-effect type into the object's collision data.
 * r3 = collision data pointer. */
Next object_setCollisionHitEffect(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    R3 = collisionData;
    ST8(collisionData + 9, R0); /* offset 9 = oCollisionData_HitEffect */

    RETURN_TO_CALLER();
}

/* ── object_setCollisionPanelsToCurrent ── */
/* object_setCollisionPanelsToCurrent(r4 = ?, r5 = battle object, lr = return):
 * Copy the object's current panel coordinates into its collision data record.
 * Leaves r0 = panel X, r1 = panel Y, r2 = collision data pointer; preserves r4. */
Next object_setCollisionPanelsToCurrent(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);

    u32 panelX = LD8(R5 + oBattleObject_PanelX);
    ST8(collision + oCollisionData_PanelX, panelX);
    R0 = panelX;

    u32 panelY = LD8(R5 + oBattleObject_PanelY);
    ST8(collision + oCollisionData_PanelY, panelY);
    R1 = panelY;

    R2 = collision;
    R4 = R4; /* r4 is preserved */
    RETURN_TO_CALLER();
}

/* ── object_setCollisionRegion ── */
/* object_setCollisionRegion(r0 = region index, r5 = battle object):
 * Write the region index into the object's collision data
 * (offset oCollisionData_Region == 1, no macro is defined for it).
 * Returns r1 = the object's collision data pointer. */
Next object_setCollisionRegion(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST8(collisionData + 1 /* oCollisionData_Region */, R0);

    R1 = collisionData;
    RETURN_TO_CALLER();
}

/* ── object_setDefaultCounterTime ── */
/* object_setDefaultCounterTime:
 * Reset the counter timer of the object the caller is working on (the callee
 * picks the object up from r5/r10) to the game's default of 0x10 frames.
 * Everything else (result registers, flags, LR) is whatever the callee leaves
 * behind; this wrapper only supplies the default time in r0. */
Next object_setDefaultCounterTime(void) {
    R0 = 0x10;                       /* default counter time */
    GUEST_CALL(object_setCounterTime);
    RETURN_TO_CALLER();
}

/* ── object_setFlag1 ── */
/* object_setFlag1(r0 = flag bits, r5 = battle object):
 * OR the given bits into the object's CollisionData flags1 word.
 * Returns: r1 = collision data pointer, r2 = updated flags word, nz = flags word. */
Next object_setFlag1(void) {
    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags1) | R0;
    ST32(R1 + oCollisionData_ObjectFlags1, R2);
    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── object_setFlag2 ── */
/* object_setFlag2(r0 = flag bits, r5 = battle object):
 * set the given bits in the object's "flags 2" field of its collision data.
 * Returns r1 = collision data pointer, r2 = updated flags, and NZ from the ORR. */
Next object_setFlag2(void) {
    u32 flags = R0;

    R1 = LD32(R5 + oBattleObject_CollisionDataPtr);
    R2 = LD32(R1 + oCollisionData_ObjectFlags2) | flags;
    ST32(R1 + oCollisionData_ObjectFlags2, R2);

    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── battle_SetObjectAIDataField78_800F318 ── */
/* battle_SetObjectAIDataField78_800F318(r0 = value, r5 = battle object):
 * Store r0 into the object's AI data at offset oAIData_Unk_78, if it has any. */
Next battle_SetObjectAIDataField78_800F318(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    if (aiData != 0)
        ST32(aiData + oAIData_Unk_78, R0);

    R1 = aiData;
    RETURN_TO_CALLER();
}

/* ── battle_getObjectAllianceTeam_800F49E ── */
/* battle_getObjectAllianceTeam_800F49E(r5 = battle object):
 * Look up the object's alliance entry: r0 = allianceTeams[alliance],
 * r1 = pointer to the allianceTeams table (dword_203CFA0). */
Next battle_getObjectAllianceTeam_800F49E(void) {
    u32 allianceTable = A_dword_203CFA0;
    u32 alliance = LD8(R5 + oBattleObject_Alliance);

    R1 = allianceTable;
    R0 = LD32(allianceTable + alliance * 4);
    RETURN_TO_CALLER();
}

/* ── battle_snapshotPlayerHP_800FAE0 ── */
/* battle_snapshotPlayerHP_800FAE0: snapshot the player's current HP into the battle state
 * (oBattleState_Unk_34), used to track HP changes over the course of a turn.
 *
 * inputs:  r4 r5 r6 r10 (r10 = toolkit) lr
 * outputs: r2 r4 r5 r6 (r4/r5/r6 preserved; r2 as left by battle_findPlayer)
 * clobbers: nz c v r0 r1 lr
 */
Next battle_snapshotPlayerHP_800FAE0(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    /* Look up the player object (argument: which alliance to search). */
    R0 = LD8(battle + oBattleState_Unk_0d);
    GUEST_CALL(battle_findPlayer);

    /* If a player exists, remember its HP. */
    if (R0 != 0)
        ST16(battle + oBattleState_Unk_34, LD16(R0 + oBattleObject_HP));

    RETURN_TO_CALLER();
}

/* ── battle_aiGetVersionedTableEntry_800FE12 ── */
/* battle_aiGetVersionedTableEntry_800FE12(r0 = table base, r5 = battle object):
 * Reads one entry from the AI table at r0. For AI version 4 the table entry at
 * offset 8 is used and scaled by the byte at oAIData_Unk_0c; any other version
 * indexes the table with version * 2.
 * Returns r0 = value, r1 = AI data pointer, r2 = version * 2 / multiplier. */
Next battle_aiGetVersionedTableEntry_800FE12(void) {
    u32 table = R0;
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 version = LD8(aiData + oAIData_Version_16);

    R1 = aiData;

    if (version == 4) {
        u32 mult = LD8(aiData + oAIData_Unk_0c);
        R0 = LD16(table + 8) * mult;
        R2 = mult;
    } else {
        R0 = LD16(table + version * 2);
        R2 = version * 2;
    }

    RETURN_TO_CALLER();
}

/* ── battle_getAiDifficultyTier_800FE36 ── */
/* battle_getAiDifficultyTier_800FE36(r5 = battle object):
 * Maps the AI data's difficulty byte to a tier (0-2) in r0, but only when the
 * AI data version is exactly 4. Note the original clobbers r0 with the loaded
 * version byte even when it bails out early, so r0 = version on that path.
 * Also returns: r1 = AI data pointer, r2 = raw difficulty byte (version 4 only).
 * Flags reflect the last comparison performed, as in the original. */
Next battle_getAiDifficultyTier_800FE36(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    R1 = aiData;

    /* ldrb writes the version into r0 before the check, so r0 holds it
     * even if we return early below. */
    u32 version = LD8(aiData + oAIData_Version_16);
    R0 = version;
    SET_FLAGS_CMP(version, 4);
    if (version != 4) {
        /* Wrong AI data version: bail out, flags from the version compare. */
        RETURN_TO_CALLER();
    }

    u32 difficulty = LD8(aiData + oAIData_Unk_0c);
    R2 = difficulty;

    R0 = 0;
    SET_FLAGS_CMP(difficulty, 3);
    if ((s32)difficulty > 3) {
        R0 = 1;
        SET_FLAGS_CMP(difficulty, 6);
        if ((s32)difficulty > 6) {
            R0 = 2;
            /* Thumb "mov rN, #imm" sets NZ; the final MOV leaves those flags. */
            SET_FLAGS_NZ(R0);
        }
    }
    RETURN_TO_CALLER();
}

/* ── battle_getPlayerAIData_8010022 ── */
/* battle_getPlayerAIData_8010022(r0 = alliance):
 * return the AI data pointer of the player's battle object
 * (r0 selects which side is "the player": false = player, true = opponent).
 * r0 = player's BattleObject.AIDataPtr. */
Next battle_getPlayerAIData_8010022(void) {
    GUEST_CALL(battle_findPlayer); /* in: R0 = alliance; out: R0 = player object */
    R0 = LD32(R0 + oBattleObject_AIDataPtr);
    RETURN_TO_CALLER();
}

/* ── battle_objectClearPendingHitIfCollisionProcessed_8010198 ── */
/* battle_objectClearPendingHitIfCollisionProcessed_8010198(r5 = battle object):
 * When this object's collision data records a pending hit, clear the hit
 * counter, but only if the collision actually set some effect flags.
 * Outputs: r3 = the object's collision data pointer. */
Next battle_objectClearPendingHitIfCollisionProcessed_8010198(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    /* Nothing queued this frame? */
    if (LD16(collision + oCollisionData_Unk_26) != 0) {
        /* Only consume the hit if it produced collision effects. */
        if (LD32(collision + oCollisionData_FlagsFromCollision) != 0)
            ST16(collision + oCollisionData_Unk_26, 0);
    }
    RETURN_TO_CALLER();
}

/* ── battle_resetCollisionState_80101C4 ── */
/* battle_resetCollisionState_80101C4 (r5 = battle object): reset the object's collision state.
 * Zeroes the collision data counter at Unk_26 and clears flag bit 2
 * (object_clearFlag arg) on the object's collision data. */
Next battle_resetCollisionState_80101C4(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST16(collision + oCollisionData_Unk_26, 0);

    R0 = 4;
    R1 = collision;
    GUEST_CALL(object_clearFlag);

    RETURN_TO_CALLER();
}

/* ── battle_GetAllianceFactionFromBattleObject_80103A8 ── */
/* battle_GetAllianceFactionFromBattleObject_80103A8: check the battle object's "alliance" stat byte (index 0x31) via
 * GetBattleNaviStatsByte_AllianceFromBattleObject and report it as a boolean:
 * r0 = 3 when the byte is nonzero, r0 = 0 when it is zero. */
Next battle_GetAllianceFactionFromBattleObject_80103A8(void) {
    R1 = 0x31; /* stats byte to read */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    R0 = (R0 != 0) ? 3 : 0;
    RETURN_TO_CALLER();
}

/* ── battle_preUpdateAndGetPlayerObject_80103EC ── */
/* battle_preUpdateAndGetPlayerObject_80103EC: run the standard pre-battle-update hook (battle_getStateByte0d_800A7E2), then
 * locate the player's battle object; result pointer in R0 (with c/v flags
 * from battle_findPlayer's tail). */
Next battle_preUpdateAndGetPlayerObject_80103EC(void) {
    GUEST_CALL(battle_getStateByte0d_800A7E2);        /* side effects / state refresh */
    GUEST_CALL(battle_findPlayer);  /* R0 = pointer to the player object */
    RETURN_TO_CALLER();
}

/* ── battle_aiAddTotalDamageTaken_8010548 ── */
/* battle_aiAddTotalDamageTaken_8010548(r4 = battle object, r0 = damage taken):
 * Add r0 to the battle object's TotalDamageTaken AI stat, clamping the
 * running total at 0xFFFF. Leaves r2 = 0xFFFF (the clamp constant). */
Next battle_aiAddTotalDamageTaken_8010548(void) {
    u32 damage = R0;
    u32 ai = LD32(R5 + oBattleObject_AIDataPtr);

    u32 total = LD16(ai + oAIData_TotalDamageTaken) + damage;
    SET_FLAGS_CMP(total, 0xFFFF);
    if ((s32)total > (s32)0xFFFF)
        total = 0xFFFF;

    ST16(ai + oAIData_TotalDamageTaken, total);
    R2 = 0xFFFF;
    RETURN_TO_CALLER();
}

/* ── battle_IsNoCustomPlusActive_80107C0 ── */
/* battle_IsNoCustomPlusActive_80107C0: check the battle effects for the "no Custom+" style flag
 * (GetBattleEffects bit 3). Returns 3 if that effect is active, 0 otherwise. */
Next battle_IsNoCustomPlusActive_80107C0(void) {
    GUEST_CALL(GetBattleEffects);
    if (R0 & 8)
        R3 = 3;
    else
        R3 = 0;
    RETURN_TO_CALLER();
}

/* ── battle_objectNameIdToPopupHandler_8010DD0 ── */
/* battle_objectNameIdToPopupHandler_8010DD0(r5 = battle object):
 * Fetches the battle object's name ID and tail-calls battle_DispatchEnemyAiRoutine_8010DDA, which
 * continues the name-popup handling with it. Returns battle_DispatchEnemyAiRoutine_8010DDA's result
 * in r0 (r1 is scratch); flags are whatever battle_DispatchEnemyAiRoutine_8010DDA leaves. */
Next battle_objectNameIdToPopupHandler_8010DD0(void) {
    R0 = LD16(R5 + oBattleObject_NameID);
    GUEST_CALL(battle_DispatchEnemyAiRoutine_8010DDA);
    RETURN_TO_CALLER();
}

/* ── battle_getObjectNameID_8011020 ── */
/* battle_getObjectNameID_8011020: forward the battle object's NameID (in R5) to sub_801102A. */
Next battle_getObjectNameID_8011020(void) {
    R0 = LD16(R5 + oBattleObject_NameID);
    GUEST_CALL(sub_801102A);
    RETURN_TO_CALLER();
}

/* ── sub_8012C34 ── */
/* sub_8012C34(r0 = value, r1 = flag bits):
 * OR in flag bits selected by r1 into r0. Bit 2 of r1 adds 0x4000,
 * bit 4 adds 0x2000. Returns r0 with the bits applied; r3 is left
 * at 4 as a side effect of the original's scratch register use. */
Next sub_8012C34(void) {
    u32 flags = R1;

    if (flags & 0x2)
        R0 |= 0x4000;
    if (flags & 0x4)
        R0 |= 0x2000;

    R3 = 4;
    RETURN_TO_CALLER();
}

/* ── battle_InitNaviStatsByIndex_8013422 ── */
/* battle_InitNaviStatsByIndex_8013422(r0 = navi index):
 * Initialize the stats struct for one BattleNavi: the navi records are 0x64
 * bytes each and start at eBattleNaviStats0, so compute the struct's address
 * and fill it with the default stats.
 * Returns r2 = eBattleNaviStats0 (base of the stats array). */
Next battle_InitNaviStatsByIndex_8013422(void) {
    u32 statsBase = A_eBattleNaviStats;

    R2 = statsBase;
    R0 = statsBase + R0 * 100; /* &eBattleNaviStats0[index * 0x64] */
    GUEST_CALL(initNaviStats_WithDefaultStatsMaybe_8013438);

    RETURN_TO_CALLER();
}

/* ── battle_applyStatusTickDamage_80142B0 ── */
/* battle_applyStatusTickDamage_80142B0(r5 = battle object, r10 = ...):
 * If battle effects indicate "confusion"-style damage over time, set the
 * object's pending damage to 10 (fixed damage of 1000 with the usual
 * decimal scaling). */
Next battle_applyStatusTickDamage_80142B0(void) {
    u32 effects = R0;
    GUEST_CALL(GetBattleEffects);

    if (R0 & 8) {
        ST16(R5 + oBattleObject_Damage, 0xA);
    }

    RETURN_TO_CALLER();
}

/* ── battle_clearAIDataField4c_8014424 ── */
/* battle_clearAIDataField4c_8014424: clear the AI state field (oAIData_Unk_4c) of the battle
 * object's AI data. Returns 0 in r1. */
Next battle_clearAIDataField4c_8014424(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    R1 = 0;
    ST32(aiData + oAIData_Unk_4c, 0);

    RETURN_TO_CALLER();
}

/* ── battle_beginTurnOpenProcess_80144C0 ── */
/* battle_beginTurnOpenProcess_80144C0 — begin the turn-open processing for the battle object whose
 * pointer the caller left in r5.
 *
 * sub_80144CA is a second entry point that shares the whole body below; it
 * enters with r7 = 1 where this entry clears r7 to 0 first.  On the shared
 * tail that flag only decides whether sub_800FEEC runs, so it is kept in the
 * guest r7 register exactly as the original does. */
Next battle_beginTurnOpenProcess_80144C0(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR7 = R7;

    /* shared-tail flag: this entry takes the full path */
    R7 = 0;
    GUEST_CALL(sub_801390C);

    /* wipe this object's chip hand: 0xC halfwords just past the hand header */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(getBattleHandAddr_8010018);
    R0 += 0x26;
    R1 = 0xC;
    GUEST_CALL(ZeroFillByHalfword);

    R0 = 0x20;
    GUEST_CALL(ClearAIDataUnk0x48Flag);

    GUEST_CALL(GetBattleEffects);
    if ((R0 & 8) != 0) {
        R0 = LD8(R5 + oBattleObject_Alliance);
        GUEST_CALL(battle_networkInvert);
        if (R0 == 0) {
            /* rerun the processing against the opposing player object */
            R0 = LD8(R5 + oBattleObject_Alliance) ^ 1;
            R1 = 1;
            GUEST_CALL(battle_findPlayer);
            if (R0 != 0) {
                R5 = R0;
                GUEST_CALL(releaseSlotByValue_801DC36);
                R5 = savedR5;
            }
        }
    }

    GUEST_CALL(battle_IsNoCustomPlusActive_80107C0);
    R0 = R3;
    GUEST_CALL(battle_SetObjectHitModifier_8019F86);
    R0 = 1;
    GUEST_CALL(object_setCollisionRegion);
    GUEST_CALL(sub_8012EA8);

    /* the sub_80144CA entry (r7 = 1) skips this call */
    if (R7 == 0)
        GUEST_CALL(sub_800FEEC);

    GUEST_CALL(sub_8014536);
    GUEST_CALL(sub_801086C);
    GUEST_CALL(battle_applySelfDamageFromBattleEffect_80142C2);

    /* restore the callee-saved registers the original pops back */
    R4 = savedR4;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── timer_resetAndStart_801482C ── */
/* timer_resetAndStart_801482C:
 * Reset the counter at dword_20367F0 to zero and set the flag byte at
 * byte_20367F4 to 1, marking the tracked state as active/fresh.
 * Returns the struct address in r1. */
Next timer_resetAndStart_801482C(void) {
    u32 state = A_dword_20367F0;

    ST32(state, 0);
    ST8(state + 4, 1);

    R1 = state;
    RETURN_TO_CALLER();
}

/* ── sub_8015940 ── */
/* sub_8015940: reset the byte_203CED0 record — the marker byte and the
 * dword_203CED4 flag are both set to 0xFF — then ask battle_preUpdateAndGetPlayerObject_80103EC for a
 * value and store it at byte_203CED8. The c/v flags seen by the caller
 * are whatever battle_preUpdateAndGetPlayerObject_80103EC left behind; r4-r6 and r10 pass through. */
Next sub_8015940(void) {
    u32 record = A_byte_203CED0;

    ST8(record, 0xFF);
    ST8(record + 4, 0xFF);      /* dword_203CED4 - byte_203CED0 */

    /* The original keeps the record pointer in r5 across the call. */
    u32 saved_r5 = R5;
    R5 = record;
    GUEST_CALL(battle_preUpdateAndGetPlayerObject_80103EC);
    R5 = saved_r5;

    ST32(record + 8, R0);       /* byte_203CED8 - byte_203CED0 */
    RETURN_TO_CALLER();
}

/* ── sub_8015C2C ── */
/* sub_8015C2C: return 0x80 (128). */
Next sub_8015C2C(void) {
    R0 = 0x80;
    RETURN_TO_CALLER();
}

/* ── battle_objectTickTimerAndSetPhaseC_80174AA ── */
/* battle_objectTickTimerAndSetPhaseC_80174AA (r5 = battle object): tick the object's action timer down by
 * one. When it reaches zero, reset the timer and advance the object to
 * phase 12 (0xC). r0 and the flags reflect the decrement, as in the original
 * (flags come from the SUBS; the expired path then re-runs them through the
 * phase write, which only refreshes nz). */
Next battle_objectTickTimerAndSetPhaseC_80174AA(void) {
    u32 timer = LD16(R5 + oBattleObject_Timer);
    u32 remaining = timer - 1;

    ST16(R5 + oBattleObject_Timer, remaining);
    R0 = remaining;
    SET_FLAGS_CMP(timer, 1); /* flags of "subs r0, #1" */

    if (remaining == 0) {
        /* Timer expired: restart it and switch to phase 12. */
        ST16(R5 + oBattleObject_Timer, 0);
        R0 = 0xC;
        ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── getIndexedSignedBytePair_8018842 ── */
/* getIndexedSignedBytePair_8018842(r0, r1 = arguments for the lookup, r2 = entry index, r4 = unused):
 * Looks up a two-byte table entry: the entry base comes from sub_8018886
 * (called with r0/r1 as passed in), offset by twice the index in r2.
 * Outputs: r0 = first byte (signed), r1 = second byte (signed) of the entry.
 * r2/r3/r4 are left untouched. */
Next getIndexedSignedBytePair_8018842(void) {
    u32 addr = R2 + R2;

    GUEST_CALL(sub_8018886);
    addr += R0;

    R0 = (s8)LD8(addr);
    R1 = (s8)LD8(addr + 1);
    RETURN_TO_CALLER();
}

/* ── battle_SetObjectHitModifier_8019F86 ── */
/* battle_SetObjectHitModifier_8019F86(r0 = hit modifier value, r5 = battle object):
 * store the object's hit modifier into its collision data.
 * Returns (r1) the pointer to the collision data that was written. */
Next battle_SetObjectHitModifier_8019F86(void) {
    u32 *collisionData = (u32 *)LD32(R5 + oBattleObject_CollisionDataPtr);
    ST8((u32)collisionData + oCollisionData_HitModifierBase, R0);
    R1 = (u32)collisionData;
    RETURN_TO_CALLER();
}

/* ── battleObject_SetSecondaryElementWeakness_8019F9E ── */
/* battleObject_SetSecondaryElementWeakness_8019F9E(r0 = element, r5 = battle object):
 * set the object's secondary element weakness in its collision data. */
Next battleObject_SetSecondaryElementWeakness_8019F9E(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST8(collisionData + oCollisionData_SecondaryElementWeakness, R0);
    RETURN_TO_CALLER();
}

/* ── clearScratchBuffer_2034F60_8019FA4 ── */
/* clearScratchBuffer_2034F60_8019FA4: wipe the 0xA0-byte scratch buffer at unk_2034F60.
 * Inputs: r2, r3 (passed through untouched). */
Next clearScratchBuffer_2034F60_8019FA4(void) {
    R0 = A_unk_2034F60;
    R1 = 0xA0;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── object_getTableEntry_801A0BA ── */
/* object_getTableEntry_801A0BA(r0 = column index, r1 = row index):
 * fetch one 32-bit entry from the two-dimensional table at byte_8019C7C
 * (8 bytes per row, 4 bytes per entry). Leaves r0 = table entry,
 * r1 = byte offset of the entry within the table, r3 = table base.
 * The C/V flags come from the offset addition, as in the original. */
Next object_getTableEntry_801A0BA(void) {
    u32 column = R0;
    u32 row = R1;

    R0 = column << 2;            /* byte offset of the entry within its row */
    R3 = A_byte_8019C7C;         /* base of the lookup table */
    R1 = row << 3;               /* byte offset of the row */

    /* add r1, r1, r0 — flags come from the original operand values */
    SET_FLAGS_ADD(R1, R0);
    R1 = R1 + R0;                /* absolute offset of the entry in the table */

    R0 = LD32(R3 + R1);          /* the table entry itself */

    RETURN_TO_CALLER();
}

/* ── battle_object_clearFlagsAndCollisionCounter_801A284 ── */
/* battle_object_clearFlagsAndCollisionCounter_801A284(r5 = battle object):
 * Clear the object's 0x800 status flag and its secondary flag 8, then zero
 * the halfword at oCollisionData_Unk_1c of its collision data.
 * Returns r3 = pointer to the object's collision data. */
Next battle_object_clearFlagsAndCollisionCounter_801A284(void) {
    R0 = 0x800;
    GUEST_CALL(object_clearFlag);

    R0 = 8;
    GUEST_CALL(object_clearFlag2);

    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collision + oCollisionData_Unk_1c, 0);
    R3 = collision;

    RETURN_TO_CALLER();
}

/* ── battleObject_clearHitFlags_801A29A ── */
/* battleObject_clearHitFlags_801A29A(r5 = battle object): clear the object's flag bits 0x10000 in
 * both flag words and reset the halfword at oCollisionData_Unk_2a of its
 * collision data. Leaves r0 = 0 (flags updated accordingly). */
Next battleObject_clearHitFlags_801A29A(void) {
    R0 = 0x10000;
    GUEST_CALL(object_clearFlag);

    R0 = 0x10000;
    GUEST_CALL(object_clearFlag2);

    R0 = 0;
    R3 = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(R3 + oCollisionData_Unk_2a, R0);

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_ClearObjectCollisionResult_801A5E2 ── */
/* battle_ClearObjectCollisionResult_801A5E2(r5 = battle object):
 * Reset the object's collision result: clear the collision data fields at
 * offsets 0x48 and 0x4C (the field at 0x4C directly follows the one at 0x48). */
Next battle_ClearObjectCollisionResult_801A5E2(void) {
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST32(collisionData + oCollisionData_Unk_48, 0);
    ST32(collisionData + oCollisionData_Unk_48 + 4, 0); /* offset 0x4C */

    RETURN_TO_CALLER();
}

/* ── battle_resetFlashingInvisTimer_801A67E ── */
/* battle_resetFlashingInvisTimer_801A67E(r5 = battle object):
 * Reset the object's "flashing while invisible" timer to zero.
 * Returns the collision data pointer in r3. */
Next battle_resetFlashingInvisTimer_801A67E(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collision + oCollisionData_FlashingInvisTimer, 0);
    R3 = collision;
    RETURN_TO_CALLER();
}

/* ── battle_objectDieOnCollisionHit_801A6D6 ── */
/* battle_objectDieOnCollisionHit_801A6D6(r5 = battle object):
 * Check the object's collision data (passed to battle_checkHPDigit4State_801A6E8 in r4); if it
 * reports a hit, the object's HP is zeroed. r4 is saved and restored around
 * the call, r7 passes through untouched. */
Next battle_objectDieOnCollisionHit_801A6D6(void) {
    u32 savedR4 = R4;

    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    R4 = collisionData;

    GUEST_CALL(battle_checkHPDigit4State_801A6E8);

    if (R0 != 0)
        ST16(R5 + oBattleObject_HP, 0);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_clearAIDataAndBarrier_801A7F4 ── */
/* battle_clearAIDataAndBarrier_801A7F4(r5 = battle object):
 * Clear the object's AI state field at oAIData_Unk_60 and drop its
 * barrier flag (offset 6 in its collision data). */
Next battle_clearAIDataAndBarrier_801A7F4(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST32(aiData + oAIData_Unk_60, 0);
    ST8(collision + 6, 0);

    RETURN_TO_CALLER();
}

/* ── calc8DirBitmaskTowardsPoint_801BE04 ── */
/* calc8DirBitmaskTowardsPoint_801BE04(r0 = x_a, r1 = y_a, r2 = x_b, r3 = y_b):
 * Compute the 8-way direction from point B toward point A as a small bitmask:
 * the y difference contributes 1 (A above B) or 2 (A below B), the x
 * difference contributes 4 (A left of B) or 8 (A right of B), and equal
 * coordinates contribute 0. E.g. A up-and-right of B yields 8 | 2 = 0xA.
 *
 * The comparisons must be exact signed comparisons of the operands (as the
 * original's subs + bgt/blt sequence achieves via the overflow flag), so
 * they are done directly rather than by subtracting, which could overflow.
 * Leaves c/v reflecting the final subtraction r1 - r3, like the original. */
Next calc8DirBitmaskTowardsPoint_801BE04(void) {
    u32 step_x = (s32)R0 > (s32)R2 ? 8
               : (s32)R0 < (s32)R2 ? 4
               :                     0;
    u32 step_y = (s32)R1 > (s32)R3 ? 2
               : (s32)R1 < (s32)R3 ? 1
               :                     0;

    R0 = step_x | step_y;

    /* Last flag-setting instruction of the original is SUBS r1, r1, r3. */
    SET_FLAGS_CMP(R1, R3);
    RETURN_TO_CALLER();
}

/* ── flagSetBits_eStruct2035280_801BEB8 ── */
/* flagSetBits_eStruct2035280_801BEB8(r0 = flag bits to set):
 * Sets the given bits in the event/flag word at eStruct2035280+0x44.
 * Leaves r1 = eStruct2035280, r2 = the updated word, and nz from the OR. */
Next flagSetBits_eStruct2035280_801BEB8(void) {
    u32 flags = LD32(A_eStruct2035280 + 0x44) | R0;

    R1 = A_eStruct2035280;
    R2 = flags;
    ST32(A_eStruct2035280 + 0x44, flags);
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── flags_clearBits_20352C4_801BEC2 ── */
/* flags_clearBits_20352C4_801BEC2(r0 = mask): clear the bits of `mask` in the flag word at
 * eStruct2035280+0x44 (dword_20352C4), i.e. mark those flags as off.
 * Leaves r1 = eStruct2035280, r2 = updated word, and NZ set on it. */
Next flags_clearBits_20352C4_801BEC2(void) {
    u32 mask = R0;
    u32 flags;

    R1 = A_eStruct2035280;
    flags = LD32(R1 + 0x44);
    flags &= ~mask;
    ST32(R1 + 0x44, flags);
    R2 = flags;

    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── eStruct2035280_SetRequestBits_801BECC ── */
/* eStruct2035280_SetRequestBits_801BECC(r0 = flag bits): set the given bits in the request dword at
 * eStruct2035280+0x40 (dword_20352C0). Leaves the struct pointer in r1 and the
 * updated dword in r2. */
Next eStruct2035280_SetRequestBits_801BECC(void) {
    u32 *flags = (u32 *)A_eStruct2035280;
    R1 = A_eStruct2035280;
    R2 = LD32(A_eStruct2035280 + 0x40) | R0;
    ST32(A_eStruct2035280 + 0x40, R2);
    RETURN_TO_CALLER();
}

/* ── flags_clearBits_eStruct2035280_801BED6 ── */
/* flags_clearBits_eStruct2035280_801BED6(r0 = bit mask): clear the given bits in dword_20352C0.
 * Returns the pointer in r1 and the updated value in r2, with flags set
 * on the new value (callers branch on it). */
Next flags_clearBits_eStruct2035280_801BED6(void) {
    u32 mask = R0;
    u32 *field = (u32 *)(A_eStruct2035280 + 0x40); /* dword_20352C0 */

    R1 = (u32)A_eStruct2035280;
    R2 = LD32(field) & ~mask;
    ST32(field, R2);
    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── sub_801BFEE ── */
/* sub_801BFEE: enable (pass 1 to sub_801C002); all results (r0-r3, flags)
 * come straight from that call. */
Next sub_801BFEE(void) {
    R0 = 1;
    GUEST_CALL(sub_801C002);
    RETURN_TO_CALLER();
}

/* ── sub_801BFF8 ── */
/* sub_801BFF8: invoke sub_801C002 with r0 = 0 and let its results
 * (registers and flags) pass straight through to the caller. */
Next sub_801BFF8(void) {
    R0 = 0;
    GUEST_CALL(sub_801C002);
    RETURN_TO_CALLER();
}

/* ── battle_RequestEndBattle_801C06E ── */
/* battle_RequestEndBattle_801C06E: turn the "end battle" flag on.
 * Sets the argument to 1 and tail-calls sub_801C082 (the shared
 * flag-setting routine), which returns straight to this function's caller. */
Next battle_RequestEndBattle_801C06E(void) {
    R0 = 1;
    GUEST_CALL(sub_801C082);
    RETURN_TO_CALLER();
}

/* ── sub_801C078 ── */
/* sub_801C078: entry point that invokes the main routine with r0 = 0
 * (no arguments); all results come straight from sub_801C082. */
Next sub_801C078(void) {
    R0 = 0;
    GUEST_CALL(sub_801C082);
    RETURN_TO_CALLER();
}

/* ── easeValueTowardTarget_801C1EA ── */
/* easeValueTowardTarget_801C1EA(r0 = value, r1 = target, r4 = state object):
 * Marks the object as active (bytes at +8/+9 set to 1), then nudges the
 * value one step toward the target. The step is proportional to the
 * remaining distance (ease-out: an eighth of the gap, plus 2), so the
 * value closes in quickly at first and slows down near the target.
 * The step is clamped so the value never overshoots past the target.
 * Returns r0 = new value, r2 = the step that was taken. */
Next easeValueTowardTarget_801C1EA(void) {
    R2 = 1;
    ST8(R4 + 8, R2);
    ST8(R4 + 9, R2);

    /* ease-out step toward the target */
    R2 = (u32)((s32)(R0 - R1) >> 3) + 2;
    R0 -= R2;

    SET_FLAGS_CMP(R0, R1);
    if ((s32)R0 < (s32)R1) {
        /* overshot: land exactly on the target */
        R0 = R1;
        SET_FLAGS_ADD(R1, 0);
    }
    RETURN_TO_CALLER();
}

/* ── event_raiseFlag8000AndDispatch_801CEFA ── */
/* event_raiseFlag8000AndDispatch_801CEFA: raise the 0x8000 flag via flags_clearBits_eStruct2035280_801BED6, then hand the same
 * flag to dispatch_801DACC so the pending event/transition takes effect. */
Next event_raiseFlag8000AndDispatch_801CEFA(void) {
    R0 = 1u << 15;                  /* event flag 0x8000 */
    GUEST_CALL(flags_clearBits_eStruct2035280_801BED6);

    R0 = 1u << 15;
    GUEST_CALL(dispatch_801DACC);

    RETURN_TO_CALLER();
}

/* ── sub_801DB54 ── */
/* sub_801DB54: call flags_clearBits_20352C4_801BEC2 with r0 = 1 (flags and r0-r2 come from the callee). */
Next sub_801DB54(void) {
    R0 = 1;
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801DB60 ── */
/* sub_801DB60: invoke some game action with argument 1.
 * Sets R0 = 1 and tail-calls flagSetBits_eStruct2035280_801BEB8; all outputs (r0-r2, flags)
 * come from that callee. */
Next sub_801DB60(void) {
    /* mov r0, #1; lsl r0, r0, #0 (identity shift, value stays 1) */
    R0 = 1;

    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);

    /* pop {pc}: return to the caller with the callee's results intact */
    RETURN_TO_CALLER();
}

/* ── sub_801DB6C ── */
/* sub_801DB6C: call flags_clearBits_20352C4_801BEC2 with r0 = 2, built as
 *     mov r0, #1 ; lsl r0, r0, #1
 * flags_clearBits_20352C4_801BEC2 performs no flag-setting instruction on its return path,
 * so the flags the caller sees are the ones that lsl leaves behind:
 * result 2 gives N=0, Z=0, and the carry out of a 1-bit left shift of 1
 * (old bit 31) is 0. */
Next sub_801DB6C(void) {
    R0 = 2;              /* 1 << 1 */
    SET_FLAGS_NZ(R0);    /* N=0, Z=0 from the shifted result */
    cpu.c = 0;           /* carry out of lsl r0, #1: bit 31 of 1 was clear */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801DB78 ── */
/* sub_801DB78: wrapper that calls flagSetBits_eStruct2035280_801BEB8 with r0 = 2.
 * The value is built as "mov r0,#1; lsl r0,#1", and the LSL is a
 * flag-setting instruction: it loads the carry flag with the bit
 * shifted out of bit 31 (0 here). flagSetBits_eStruct2035280_801BEB8's reported flags depend
 * on the incoming carry, so this must be reproduced exactly. */
Next sub_801DB78(void) {
    R0 = 1;
    SET_FLAGS_ADD(R0, R0);   /* same flags as "lsl r0, r0, #1" on 1: N=0, Z=0, C=0 */
    R0 = R0 + R0;            /* r0 = 2 */
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── sub_801DC6E ── */
/* sub_801DC6E: invoke flagSetBits_eStruct2035280_801BEB8 with r0 = 4 (bit flag 1 << 2).
 *
 * The LSL that builds the argument is the last flag-setting instruction
 * before the call, and flagSetBits_eStruct2035280_801BEB8 preserves the flags, so the state it
 * leaves must survive to our caller: shifting 1 left by 2 pushes no set
 * bit out, giving N=0, Z=0, C=0 (result 4, no bits shifted out).
 * SET_FLAGS_ADD(1, 3) reproduces exactly those flags (sum 4, no carry). */
Next sub_801DC6E(void) {
    R0 = 1u;
    R1 = 2u;
    R0 = R0 << R1;          /* r0 = 1 << 2 = 4 */
    SET_FLAGS_ADD(1u, 3u);  /* N=0, Z=0, C=0 — same flags as the LSL */

    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── sub_801DF8C ── */
/* sub_801DF8C(r0 = value): store a halfword into word_20352A2 (the 0x22 field of
 * the eStruct2035280 struct). */
Next sub_801DF8C(void) {
    ST16(A_eStruct2035280 + 0x22, R0);
    RETURN_TO_CALLER();
}

/* ── sub_801E006 ── */
/* sub_801E006: wrapper that invokes flagSetBits_eStruct2035280_801BEB8 with the argument 0x40
 * (built as mov r0,#1; lsl r0,r0,#6).
 *
 * The immediate LSL is the last flag-setting instruction of this function
 * that the callee leaves untouched, and callers branch on these flags right
 * after the call, so its flag effects must be reproduced:
 *   result 0x40 -> N=0, Z=0; shifted-out bit 26 of 1 -> C=0.
 * SET_FLAGS_ADD(R0, 0) yields exactly that carry/N/Z combination. */
Next sub_801E006(void) {
    R0 = 1 << 6;              /* argument for flagSetBits_eStruct2035280_801BEB8 */
    SET_FLAGS_ADD(R0, 0);     /* flags as left by `lsl r0, r0, #6` */
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── setStruct2035280Field26_801E0C8 ── */
/* setStruct2035280Field26_801E0C8(r0 = value):
 * Store a halfword into word_20352A6 (field 0x26 of the eStruct2035280
 * structure). R5 is only used as a scratch pointer inside the original and
 * is restored on exit, so it must be left unchanged here. */
Next setStruct2035280Field26_801E0C8(void) {
    u32 base = A_eStruct2035280;

    ST16(base + 0x26, R0);
    RETURN_TO_CALLER();
}

/* ── sub_801E5E0 ── */
/* sub_801E5E0: call flags_clearBits_20352C4_801BEC2 with r0 = 0x4000.
 * The wrapper itself contributes nothing but the argument: r0-r2 and the
 * flags after return are exactly what flags_clearBits_20352C4_801BEC2 leaves behind. Note that
 * the original's `mov r0,#1` + `lsl r0,r0,#14` sequence is a Thumb shifted
 * move, which sets C from the bit shifted out (bit 18 of 1 = 0), so the call
 * is entered with C clear — flags_clearBits_20352C4_801BEC2 relies on that on paths where it
 * preserves C. */
Next sub_801E5E0(void) {
    R0 = 1 << 14;
    SET_FLAGS_ADD(R0, 0);   /* N=0, Z=0, C=0, matching the LSL result */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801E5EC ── */
/* sub_801E5EC: invoke flagSetBits_eStruct2035280_801BEB8 with r0 = 0x4000 (1 << 14).
 * All results (r0-r2) come straight from the callee; the callee also leaves
 * N/Z alone, but passes the C flag through unchanged, so we must reproduce
 * the C=0 that the original's "lsl r0, r0, #14" leaves behind (shifting the
 * value 1 left by 14 shifts no 1-bits out of the register). */
Next sub_801E5EC(void) {
    R0 = 1u << 14;
    /* Same flag state the lsl leaves: N=0, Z=0, C=0. */
    SET_FLAGS_ADD(R0, 0);
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── sub_801E71C ── */
/* sub_801E71C(r0 = value):
 * Stores the byte in r0 into byte_2035292 (field 0x12 of eStruct2035280).
 * r1 is preserved; r0's incoming value is not meaningful afterwards. */
Next sub_801E71C(void) {
    ST8(A_eStruct2035280 + 0x12, R0);
    RETURN_TO_CALLER();
}

/* ── naviStatus_SetGfxAndStatus_801E724 ── */
/* naviStatus_SetGfxAndStatus_801E724(r0 = new navi status gfx index, r1 = new navi status value):
 * Updates the navi status state at eStruct2035280. The current display index
 * (0x17) is reused if it is set, otherwise falls back to the value at 0x15;
 * it is written to 0x1F. Stores the new status at 0x16, the new gfx index at
 * 0x17, and marks 0x13 as pending with 0xFF. r5 is saved/restored by the
 * original, so it is left unchanged. r2 = the display index written. */
Next naviStatus_SetGfxAndStatus_801E724(void) {
    u32 gfxIdx = R0;
    u32 status = R1;
    u32 state  = A_eStruct2035280;

    u32 displayIdx = LD8(state + 0x17);
    if (displayIdx == 0xFF)
        displayIdx = LD8(state + 0x15);

    ST8(state + 0x1F, displayIdx);
    ST8(state + 0x16, status);
    ST8(state + 0x17, gfxIdx);
    ST8(state + 0x13, 0xFF);

    R0 = 0xFF;
    R2 = displayIdx;
    RETURN_TO_CALLER();
}

/* ── sub_801E73C ── */
/* sub_801E73C: invoke flags_clearBits_20352C4_801BEC2 with r0 = 0x8000.
 *
 * The constant has no Thumb MOV encoding, so the original builds it as
 * 1 << 15 — and that LSL is the last flag-setting instruction on the path
 * through this wrapper, because flags_clearBits_20352C4_801BEC2 itself leaves the condition
 * flags untouched. Callers therefore see the shift's flag results
 * (N = 0, Z = 0, carry-out = 0) after this function returns, so they must
 * be produced explicitly. LR ends up holding the BL's return address
 * (0x801E747), which the GUEST_CALL return trampoline reproduces. */
Next sub_801E73C(void) {
    R0 = 1;
    R0 <<= 15;          /* r0 = 0x8000; shifting 1 by 15 carries out a 0 bit */
    SET_FLAGS_NZ(R0);   /* N = 0, Z = 0 from the shifted result */
    cpu.c = 0;          /* carry out of the LSL */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801E748 ── */
/* sub_801E748: invoke flagSetBits_eStruct2035280_801BEB8 with the argument 0x8000 (bit 15 set).
 * The literal "mov r0,#1; lsl r0,r0,#15" also updates the flags, and since
 * the callee preserves them on exit, its carry-out (0 for this operand)
 * is what our caller observes. */
Next sub_801E748(void) {
    R0 = 1u << 15;
    /* flag state after "lsl r0, r0, #15": result 0x8000 -> N=0, Z=0,
     * carry out of bit 17 of the old value 1 -> C=0. V is unaffected. */
    SET_FLAGS_NZ(R0);
    cpu.c = 0;
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── setByteFieldCapped99_801E828 ── */
/* setByteFieldCapped99_801E828(r1 = value, r5 = struct pointer):
 * Clamp the byte value to at most 99, store it at [r5+9], then let
 * sub_801E838 refresh whatever depends on it (results and flags are
 * whatever that call leaves behind). */
Next setByteFieldCapped99_801E828(void) {
    u32 value = R1;

    if ((s32)value > 99)
        value = 99;

    ST8(R5 + 9, value);
    R1 = value;

    GUEST_CALL(sub_801E838);

    RETURN_TO_CALLER();
}
