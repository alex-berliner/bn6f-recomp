/*
 * asm03_0_802F2C8.c: readable functions of bn6f asm03_0.s, from 0x0802F2C8, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_802F2C8 ── */
/* register form: calls ByteFill outside this file, which has no typed C interface yet */
static void byte_fill(u32 mem, u32 byteCount, u32 fillValue) {
    R0 = mem;
    R1 = byteCount;
    R2 = fillValue;
    GUEST_CALL(ByteFill);
}

/* Fill the palette scratch buffer at byte_2006530 with 0x80 (blank) and clear dword_2001140. */
static void reset_palette_scratch(u32 paletteBuf, u32 dwordSlot) {
    byte_fill(paletteBuf, 0x80, 0x80);
    ST32(dwordSlot, 0);
}

/* sub_802F2C8() -> c/v left by ByteFill */
Next sub_802F2C8(void) {
    reset_palette_scratch(A_byte_2006530, A_dword_2001140);
    RETURN_TO_CALLER();
}

/* ── startScreen_RunStateHandler_802F63C ── */
/* startScreen_RunStateHandler_802F63C(self: *StartScreen in r5) -> ():
 * Start screen state dispatcher. Reads the screen's state byte and calls the
 * matching handler from the jump table jt_802F654, passing it r0 = handler
 * address | thumb bit and r1 = the state byte, then runs the common
 * post-state routine sub_802FB10 before returning. */

/* Offset of the state byte in StartScreen; named as in the disassembly
 * (ldrb r1, [r5, #oStartScreen_JumpTableOff_01]). */
#define oStartScreen_JumpTableOff_01 1

Next startScreen_RunStateHandler_802F63C(void) {
    u32 state = LD8(R5 + oStartScreen_JumpTableOff_01);

    /* The state byte is a byte offset into the jump table (ldr r0, [r0, r1]
     * indexes it without scaling), so the five entries live at offsets
     * 0, 4, 8, 12 and 16, not 0..4. Load the entry exactly as the original
     * does so r0 carries the handler address | thumb bit into the call. */
    R1 = state;
    R0 = LD32(A_jt_802F654 + state);

    switch (state) {
    case 0:  GUEST_CALL(startScreen_init_802F668); break;                              /* state 0 */
    case 4:  GUEST_CALL(sub_802F6A4); break;                              /* state 1 */
    case 8:  GUEST_CALL(sub_802F6B2); break;                              /* state 2 */
    case 12: GUEST_CALL(startScreen_UpdateAndRenderFrame_802F704); break; /* state 3 */
    case 16: GUEST_CALL(sub_802F710); break;                              /* state 4 */
    }

    GUEST_CALL(sub_802FB10);

    RETURN_TO_CALLER();
}

/* ── startScreen_init_802F668 ── */
/* startScreen_init_802F668(r5 = StartScreen state block):
 * (Re)initialize the start screen: clear its counters, reset the menu
 * cursor, start the title-screen music, set up the LCD render info,
 * reload the background tiles, start a fade-in, and write 4 into the
 * jump-table index (offset 1) so the dispatcher at startScreen_RunStateHandler_802F63C runs
 * handler sub_802F6A4 next. */
Next startScreen_init_802F668(void) {
    u32 self = R5;

    /* clear the screen's frame/step counters */
    ST8(self + 0xE, 0);
    ST8(self + 0xF, 0);

    /* reset the selected menu entry to the first row (0xA46) */
    ST16(self + 4, 0xA46);

    /* play the title screen music */
    R0 = 1;
    GUEST_CALL(music_80005F2);

    R0 = 0xA;
    GUEST_CALL(sub_80015FC);

    /* configure LCD render control */
    R0 = 0x1340;
    GUEST_CALL(SetRenderInfoLCDControl);

    /* reset the cursor index and reload the background tiles */
    ST8(self + 6, 0);
    GUEST_CALL(copyBGTiles_802FD3C);

    /* fade in from black (type 8, speed 0x10) */
    R0 = 8;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    /* advance to start-screen state 4 (handler sub_802F6A4) */
    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(self + 1, R0); /* oStartScreen_JumpTableOff_01 == 1 */

    RETURN_TO_CALLER();
}

/* ── startScreen_UpdateAndRenderFrame_802F704 ── */
/* startScreen_UpdateAndRenderFrame_802F704(r5 = StartScreen *self):
 * advance the start screen one frame — first update its state,
 * then render it. Both helpers take the screen pointer in r5. */
Next startScreen_UpdateAndRenderFrame_802F704(void) {
    GUEST_CALL(sub_802F81C);
    GUEST_CALL(sub_802F8D8);
    RETURN_TO_CALLER();
}

/* ── sub_802F710 ── */
/* register form: calls sub_802F8D8 outside this file, which has no typed C interface yet */
static void start_screen_step(u32 self) {
    R5 = self;
    GUEST_CALL(sub_802F8D8);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool is_screen_fade_active_sub_802F710(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();  /* Z set when fade is complete (not active) */
}

/* register form: calls zeroFill_e20094C0 outside this file, which has no typed C interface yet */
static void zero_fill_jump_table(void) {
    GUEST_CALL(zeroFill_e20094C0);
}

/* register form: calls palette_clearEPalette20097a0_80023A8 outside this file, which has no typed C interface yet */
static void clear_palette_20097a0(void) {
    GUEST_CALL(palette_clearEPalette20097a0_80023A8);
}

/* register form: calls call_803D1AC outside this file, which has no typed C interface yet */
static void reset_display_803D1AC(void) {
    GUEST_CALL(call_803D1AC);
}

/* register form: calls sub_81440D8 outside this file, which has no typed C interface yet */
static void reset_obj_81440D8(void) {
    GUEST_CALL(sub_81440D8);
}

/* register form: calls zeroFill_813D960 outside this file, which has no typed C interface yet */
static void zero_fill_oam(void) {
    GUEST_CALL(zeroFill_813D960);
}

/* register form: calls clear_e200AD04 outside this file, which has no typed C interface yet */
static void clear_render_state_200AD04(void) {
    GUEST_CALL(clear_e200AD04);
}

/* register form: calls sub_803F6B0 outside this file, which has no typed C interface yet */
static void set_screen_mode(u32 mode) {
    R0 = mode;
    GUEST_CALL(sub_803F6B0);
}

/* register form: calls loc_803F512 outside this file, which has no typed C interface yet */
static void reset_animation_803F512(void) {
    GUEST_CALL(loc_803F512);
}

/* register form: calls init_eStartScreenAnimationControl200B1A0_1 outside this file, which has no typed C interface yet */
static void init_start_screen_anim(void) {
    GUEST_CALL(init_eStartScreenAnimationControl200B1A0_1);
}

/* Advance the start-screen fade: one step per frame while the counter is non-zero.
 * If the fade has completed and the counter is still non-zero, trigger a game-load
 * via the jump-table offset.  If the counter reached zero, tear down all rendering
 * and animation state (full reset back to the title). */
static void start_screen_update(u32 self) {
    /* offset 4: fade-transition pending counter (u16, zero-extended) */
    if (LD16(self + 4) != 0)
        start_screen_step(self);

    /* Abort if the screen is no longer mid-fade. */
    if (!is_screen_fade_active_sub_802F710())
        return;

    if (LD16(self + 4) != 0) {
        /* Counter still running: set jump-table offset to 0x0C
         * so the next render dispatch loads the game. */
        ST16(self + oStartScreen_JumpTableOff_00, 0xC);
    } else {
        /* Counter exhausted: full reset of visuals and animation. */
        zero_fill_jump_table();
        clear_palette_20097a0();
        reset_display_803D1AC();
        reset_obj_81440D8();
        zero_fill_oam();
        clear_render_state_200AD04();
        set_screen_mode(0);
        reset_animation_803F512();
        init_start_screen_anim();
    }
}

/* sub_802F710(self: *mut StartScreen r5) -> (clobbers r0-r3 + flags) */
Next sub_802F710(void) {
    start_screen_update(R5);
    RETURN_TO_CALLER();
}

/* ── menu_onStartPressBeginTransition_802F7E8 ── */
/* menu_onStartPressBeginTransition_802F7E8: per-frame check on one of the menu screens. While the Start
 * button is held, wipe the screen's work area, play the menu sound, store
 * the follow-up state (0x0C) into the caller's object (r5) and load the two
 * GFX animation scripts the next state draws with. R7 ends up holding the
 * toolkit pointer (same value the caller passed in R10). */

/* offset of the joypad-pointer field in the toolkit struct (the disassembly
 * calls it oToolkit_JoypadPtr; that macro isn't in game.h, so spell out 4) */
#define TOOLKIT_JOYPAD_PTR 4
/* bit 3 of the held-key halfword: Start button */
#define JOYPAD_START 0x0008

Next menu_onStartPressBeginTransition_802F7E8(void) {
    /* the original does "mov r7, r10" unconditionally: r7 must alias the
     * toolkit pointer on both paths, since callees pick it up from r7 */
    R7 = R10;

    u32 heldKeys = LD16(LD32(R7 + TOOLKIT_JOYPAD_PTR) + 2);

    if (heldKeys & JOYPAD_START) {
        /* zeroFill_813D960 finds its target through the toolkit pointer in r7
         * and reads r5/r10 as well */
        GUEST_CALL(zeroFill_813D960);

        ST8(R5 + 1, 0xC); /* state for the caller's object: leave this screen */

        R0 = SOUND_SELECT_67;
        /* the tst mask is still live in r1 here; PlaySoundEffect reads it */
        R1 = 8;
        GUEST_CALL(PlaySoundEffect);

        R0 = A_byte_802F2E4;
        GUEST_CALL(LoadGFXAnim);
        R0 = A_dword_802F334;
        GUEST_CALL(LoadGFXAnim); /* may update r10; callers see the new value */
    }

    RETURN_TO_CALLER();
}

/* ── startScreen_playSoundIfFlagSet_802FB64 ── */
/* startScreen_playSoundIfFlagSet_802FB64(self: *const StartScreen r5):
 * If the start screen's status halfword has bit 0x80 set, run the sound/data
 * sequence at byte_802FB7C through sub_8001158. Otherwise do nothing. */
Next startScreen_playSoundIfFlagSet_802FB64(void) {
    if (LD16(R5 + 0xA) & 0x80) {
        R0 = A_byte_802FB7C;
        GUEST_CALL(sub_8001158);
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_fadeOutUpdateIfFlag40_802FB90 ── */
/* startScreen_fadeOutUpdateIfFlag40_802FB90(self: *StartScreen (r5)):
 * When the start screen's 16-bit flag field has bit 0x40 set, invoke
 * sub_8001158 with the parameter table at byte_802FBA8 (an update/
 * fade routine). Flags (c/v) and r2/r3 after the call are whatever
 * sub_8001158 leaves; r4-r7 are untouched throughout. */
Next startScreen_fadeOutUpdateIfFlag40_802FB90(void) {
    if (LD16(R5 + 0xA) & 0x40) {
        R0 = A_byte_802FBA8;
        GUEST_CALL(sub_8001158);
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_dispatchIfFlag20_802FBB4 ── */
/* startScreen_dispatchIfFlag20_802FBB4(self = start screen object in R5):
 * If the screen's state halfword (offset 0xA) has flag 0x20 set, invoke
 * sub_8001158 with the address of the byte_802FBCC record (which begins
 * with the word 0x80544004, a pointer followed by padding). Otherwise
 * return immediately. */
#define SCREEN_FLAG_20 0x20u /* bit in the state halfword at +0xA */

Next startScreen_dispatchIfFlag20_802FBB4(void) {
    u16 state = LD16(R5 + 0xA);

    if (state & SCREEN_FLAG_20) {
        R0 = A_byte_802FBCC;
        GUEST_CALL(sub_8001158);
    } else {
        /* Not-taken path: TST r0, r1 is the last flag-setting instruction,
         * so leave N/Z reflecting (state & 0x20). */
        SET_FLAGS_NZ(state & SCREEN_FLAG_20);
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_playSoundIfFlagSet_802FBD8 ── */
/* startScreen_playSoundIfFlagSet_802FBD8(r5 = StartScreen):
 * Check the screen's status flags at +0xA; if bit 4 (0x10) is set, hand the
 * byte table at byte_802FBF0 to sub_8001158, which plays the associated
 * sound/voice clip. Otherwise do nothing.
 *
 * r2/r3 and r4-r7 pass through untouched; on the not-taken path C/V are left
 * exactly as the caller set them (Thumb TST only touches N/Z, which callers
 * of this function ignore). */
Next startScreen_playSoundIfFlagSet_802FBD8(void) {
    if (LD16(R5 + 0xA) & 0x10) {
        R0 = A_byte_802FBF0;
        GUEST_CALL(sub_8001158);
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_printTextIfFlag3_802FBFC ── */
/* startScreen_printTextIfFlag3_802FBFC(r5 = start screen): if the start screen's state field has
 * bit 3 set, hand the text descriptor table at byte_802FC14 to the
 * text printer helper sub_8001158. r2/r3, the c and v flags (and r5-r7)
 * are whatever sub_8001158 leaves behind when it runs. */
Next startScreen_printTextIfFlag3_802FBFC(void) {
    if (LD16(R5 + 0xA) & 8) {
        R0 = A_byte_802FC14;
        GUEST_CALL(sub_8001158); /* () -> void */
    }
    RETURN_TO_CALLER();
}

/* ── startScreen_onStatusBit2_802FC28 ── */
/* startScreen_onStatusBit2_802FC28(r5 = start screen state):
 * When the start screen's status word has bit 2 set, hand the data table at
 * byte_802FC40 to sub_8001158 (which follows the pointer/params stored there).
 * Flag state on the skip path comes from the TST (N/Z from the AND result,
 * C/V untouched); on the call path it is whatever sub_8001158 leaves behind. */
Next startScreen_onStatusBit2_802FC28(void) {
    u16 status = LD16(R5 + 0xA);

    if (status & 4) {
        R0 = A_byte_802FC40;
        GUEST_CALL(sub_8001158);
    } else {
        SET_FLAGS_NZ(status & 4);
    }

    RETURN_TO_CALLER();
}

/* ── startScreen_playSfxIfFlagSet_802FC4C ── */
/* startScreen_playSfxIfFlagSet_802FC4C(r5 = StartScreen):
 * If the start screen has flagged bit 1 in its state field at +0xA, feed the
 * sound-effect parameter block at byte_802FC64 to sub_8001158 (SFX player).
 * Flags on return: NZ from the bit test if the call was skipped, otherwise
 * whatever sub_8001158 leaves behind. */
Next startScreen_playSfxIfFlagSet_802FC4C(void) {
    u16 screenFlags = LD16(R5 + 0xA);

    R0 = screenFlags;
    R1 = 2;
    SET_FLAGS_NZ(screenFlags & 2);

    if (screenFlags & 2) {
        R0 = A_byte_802FC64;   /* SFX parameter block */
        GUEST_CALL(sub_8001158);
    }

    RETURN_TO_CALLER();
}

/* ── startScreen_playFlaggedSfx_802FC70 ── */
/* startScreen_playFlaggedSfx_802FC70: start-screen per-frame helper (r5 = StartScreen state block).
 * If bit 0x100 of the halfword at +0x0A is set, trigger the effect whose
 * parameter table is byte_802FC88 (passed to sub_8001158 in r0).
 *
 * Flag behaviour of the original: the "lsl r1, r1, #4" used to build the
 * test mask (0x10 -> 0x100) is a Thumb shifted-move, which clears C (bit 28
 * of 0x10 shifts out) and leaves V alone; the subsequent TST only updates
 * N/Z. So on the early-out path C is always 0, V is untouched, and N/Z come
 * from (state & 0x100). On the call path the flags are whatever
 * sub_8001158 leaves behind. */
Next startScreen_playFlaggedSfx_802FC70(void) {
    u32 state = LD16(R5 + 0xA);

    /* Emulate the flag effect of the mask setup: LSL r1, r1, #4 cleared C. */
    cpu.c = 0;

    if (state & 0x100) {
        /* r0 = sound/effect parameter table, run it. */
        R0 = A_byte_802FC88;
        GUEST_CALL(sub_8001158);
    } else {
        /* TST r0, r1 result (0 or 0x100, so N clear; Z per the result). */
        SET_FLAGS_NZ(state & 0x100);
    }

    RETURN_TO_CALLER();
}

/* ── startScreen_runTriggerHandler_802FC9C ── */
/* startScreen_runTriggerHandler_802FC9C(r5 = start screen state):
 * Start screen hook. When bit 0x200 of the state word at +0xA is set, hand the
 * small data table at byte_802FCB4 to sub_8001158 (which consumes it as a
 * parameter block). Otherwise do nothing. */
Next startScreen_runTriggerHandler_802FC9C(void) {
    enum { STARTSCREEN_TRIGGER = 0x200 };

    if (LD16(R5 + 0xA) & STARTSCREEN_TRIGGER) {
        R0 = A_byte_802FCB4;
        GUEST_CALL(sub_8001158);
    }
    RETURN_TO_CALLER();
}

/* ── copyBGTiles_802FD3C ── */
/* copyBGTiles_802FD3C: copy the fixed background tile map into the BG.
 * Calls CopyBackgroundTiles with:
 *   j = 0, i = 0            (destination tile origin)
 *   which_tile_block = 1    (32x32 tile block #1)
 *   tile_ids = byte_87F8EB0 (source tile id table)
 *   j_size = 0x20, i_size = 0x14 (32 x 20 tiles)
 * R2 and R4 are passed through unchanged from CopyBackgroundTiles. */
Next copyBGTiles_802FD3C(void) {
    u32 savedR5 = R5;

    R0 = 0;               /* j */
    R1 = 0;               /* i */
    R2 = 1;               /* which_tile_block_32x32 */
    R3 = A_byte_87F8EB0;  /* tile_ids */
    R4 = 0x20;            /* j_size */
    R5 = 0x14;            /* i_size */

    GUEST_CALL(CopyBackgroundTiles);

    /* the original push/pop keeps the caller's r5 intact */
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── copyObjAttributesToIWRAM_802FE0C ── */
/* copyObjAttributesToIWRAM_802FE0C:
 * Flush the shadow OAM (iOamShadow) into the real hardware OAM at 0x7000000,
 * copying 0x400 bytes one eight-word block at a time. r3 is an incoming
 * argument that must pass through untouched. */
Next copyObjAttributesToIWRAM_802FE0C(void) {
    R0 = A_iOamShadow;  /* src */
    R1 = 0x7000000;     /* dest: OAM */
    R2 = 0x400;         /* size in bytes */
    GUEST_CALL(CopyByEightWords);
    RETURN_TO_CALLER();
}

/* ── sub_802FE48 ── */
/* sub_802FE48: allocate one slot in the allocation bitmap at dword_2009A2C.
 * Scans bits 31..16 from the top down, sets the first clear bit and returns
 * its index in r0. Returns 0xFF if all 16 slots are already taken.
 * (r1 = bitmap word, r2 = mask of the claimed/failed bit, r3 = bitmap address,
 * as the original leaves them.) */
Next sub_802FE48(void) {
    u32 addr = A_dword_2009A2C;
    u32 bits = LD32(addr);
    u32 mask = 0x80000000u;
    u32 slot = 0;

    while (bits & mask) {
        mask >>= 1;
        slot++;
        if (slot >= 0x10) {
            /* all 16 slots in use */
            R0 = 0xFF;
            R1 = bits;
            R2 = mask;
            R3 = addr;
            RETURN_TO_CALLER();
        }
    }

    /* claim the first free slot */
    bits |= mask;
    ST32(addr, bits);
    R0 = slot;
    R1 = bits;
    R2 = mask;
    R3 = addr;
    RETURN_TO_CALLER();
}

/* ── flags_clearFlagWordBit_802FE6A ── */
/* flags_clearFlagWordBit_802FE6A(r0 = bit index): clear one bit of the 32-bit flag word at
 * dword_2009A2C.  Bits are numbered from the top, so index 0 corresponds to
 * 0x80000000 and index 31 to 0x00000001; an index of 32 or more leaves the
 * word untouched (the mask shifts out completely).
 * Leaves r1 = updated flag word, r2 = mask of the cleared bit,
 * r3 = &dword_2009A2C, NZ from the updated word, and C as left by the
 * register-controlled LSR (C = 1 only for a shift count of exactly 32). */
Next flags_clearFlagWordBit_802FE6A(void) {
    u32 shift = R0 & 0xFF;
    u32 mask = (shift < 32) ? (0x80000000u >> shift) : 0;

    R3 = A_dword_2009A2C;
    R1 = LD32(R3) & ~mask;
    R2 = mask;
    ST32(R3, R1);

    /* C comes from the `lsr r2, r0` barrel shift: for a count of exactly 32
     * the shifted-out bit 31 of 0x80000000 is set, otherwise C = 0. */
    if (shift == 32)
        SET_FLAGS_CMP(32, 32); /* C = 1 */
    else
        SET_FLAGS_CMP(0, 1);   /* C = 0 */

    /* NZ come from the `bic` that produced the stored word */
    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── cleareMemory_802FF2C ── */
/* cleareMemory_802FF2C: reset the state tracked in dword_2009A2C and wipe the
 * 0x180-byte buffer at word_200A6F0 (cleared in eight-word chunks).
 * Flags on return are whatever ZeroFillByEightWords leaves behind. */
Next cleareMemory_802FF2C(void) {
    ST32(A_dword_2009A2C, 0);

    R0 = A_word_200A6F0;
    R1 = 0x180;
    GUEST_CALL(ZeroFillByEightWords);

    RETURN_TO_CALLER();
}

/* ── camera_dispatchUpdate_8030054 ── */
/* camera_dispatchUpdate_8030054: camera mode dispatcher.
 *
 * r5 points at the camera object. The byte at offset 2 selects which
 * handler to run; it acts as a byte offset into the handler table at
 * off_8030068, whose entries are:
 *   0x00, 0x04, 0x10 -> camera_updateDerivedOffsets_803007C (default/idle camera update)
 *   0x08             -> sub_80300AC
 *   0x0C             -> sub_8030104
 *
 * The original clobbers r0/r1 before jumping: it loads the selected handler
 * address (Thumb bit set) into r0 and the selector byte into r1, and the
 * handlers are entered seeing those values. All other registers and the
 * condition flags pass through to the handler untouched, and whatever the
 * handler leaves in them is handed back to our caller.
 */
Next camera_dispatchUpdate_8030054(void) {
    u32 select = LD8(R5 + 2);

    R1 = select;
    R0 = LD32(A_off_8030068 + select); /* handler pointer, as the original loads it */

    switch (select) {
    case 0x00: /* default camera update */
    case 0x04: /* default camera update */
    case 0x10: /* default camera update */
        GUEST_CALL(camera_updateDerivedOffsets_803007C);
        break;

    case 0x08:
        GUEST_CALL(sub_80300AC);
        break;

    case 0x0C:
        GUEST_CALL(sub_8030104);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── camera_updateDerivedOffsets_803007C ── */
/* camera_updateDerivedOffsets_803007C(r5 = camera state):
 * Recompute the camera's two 16.16 fixed-point offset fields at +4/+6
 * from the pending scroll target nextX/nextY/nextZ, negating the results.
 * Leaves r1/r2/r4/r6/r7/r8 and the flags (from the final `neg r6`) as the
 * original does, since callers inspect them. */
Next camera_updateDerivedOffsets_803007C(void) {
    u32 cam = R5;

    /* Load the pending scroll target and negate X and Y. */
    u32 negNextX = 0 - LD32(cam + oCamera_nextX_3c);
    u32 negNextY = 0 - LD32(cam + oCamera_nextY_40);
    u32 nextZ    = LD32(cam + oCamera_nextZ_44);

    /* Offset 04: negate the high 16 bits of (negX + negY).
     * (The `neg r1` here also sets flags, but they are overwritten later.) */
    R1 = 0 - (u32)((s32)(negNextX + negNextY) >> 16);
    ST16(cam + 4, R1);

    /* Offset 06: (negY - negX) / 2 plus nextZ, negate the high 16 bits.
     * The final `neg r6, r6` is the last flag-setting instruction of the
     * routine, so the flags must come from 0 - shifted (the negation
     * itself), not from the already-negated result. */
    u32 halfDiff = (u32)((s32)(negNextY - negNextX) >> 1);
    u32 shifted  = (u32)((s32)(halfDiff + nextZ) >> 16);
    SET_FLAGS_CMP(0, shifted);          /* flags of RSBS: 0 - shifted */
    R6 = 0 - shifted;
    ST16(cam + 6, R6);

    /* Values callers expect to find in the registers afterwards. */
    R2 = nextZ;
    R4 = 0;
    R7 = negNextY;
    R8 = negNextX;

    RETURN_TO_CALLER();
}

/* ── camera_SavePosition_803011A ── */
/* camera_SavePosition_803011A(r5 = camera struct):
 * Latch the camera's current position into its saved-position fields:
 * Unk_04 -> Unk_08 and Unk_06 -> Unk_0a. Returns r1 = the second halfword. */
Next camera_SavePosition_803011A(void) {
    R1 = LD16(R5 + 4);              /* oCamera_Unk_04: current x */
    ST16(R5 + 8, R1);               /* oCamera_Unk_08: saved x */
    R1 = LD16(R5 + 6);              /* oCamera_Unk_06: current y */
    ST16(R5 + oCamera_Unk_0a, R1);  /* oCamera_Unk_0a: saved y */
    RETURN_TO_CALLER();
}

/* ── nullsub_7 ── */
/* nullsub_7: a no-op stub (called for features not present in this build). */
Next nullsub_7(void) {
    RETURN_TO_CALLER();
}

/* ── camera_LoadPositionFromSource_8030126 ── */
/* camera_LoadPositionFromSource_8030126: copy the camera position from the data block referenced by
 * oCamera_Unk_14 into the camera's X/Y/Z fields.  Leaves r7 pointing at that
 * block and r2 holding the Z value (both are read by callers). */
Next camera_LoadPositionFromSource_8030126(void) {
    R7 = LD32(R5 + oCamera_Unk_14);

    R2 = LD32(R7 + 0);
    ST32(R5 + oCamera_X, R2);

    R2 = LD32(R7 + 4);
    ST32(R5 + oCamera_Y, R2);

    R2 = LD32(R7 + 8);
    ST32(R5 + oCamera_Z, R2);

    RETURN_TO_CALLER();
}

/* ── camera_applyScrollToBgLayers_8030158 ── */
/* camera_applyScrollToBgLayers_8030158 (r5 = camera state, r10 = toolkit):
 * Scroll the first background layer pair by the camera's movement since the
 * last frame (current camera position minus previous position), then ask
 * sub_80269E2 -- passing it the scroll deltas on the stack -- whether the
 * second layer pair should follow; if it returns 0, scroll that pair too.
 * The original saves/restores r4-r7, so those end unchanged; r1 and r4's
 * scroll deltas are reloaded after the call like the original's pop does,
 * r2 is left holding whatever the callee leaves (the original never reloads
 * it), and the flags come from the TST of the callee's result (early exit)
 * or from the final ADDS when both layer pairs scroll. */
Next camera_applyScrollToBgLayers_8030158(void) {
    /* the original pushes r4-r7 on entry and pops them on exit */
    u32 inR4 = R4, inR6 = R6, inR7 = R7;

    /* camera current minus previous position:
     * oCamera_Unk_04/06 = current X/Y, oCamera_Unk_08/oCamera_Unk_0a = previous X/Y */
    u32 scrollX = LD16(R5 + 4) - LD16(R5 + 8);
    u32 scrollY = LD16(R5 + 6) - LD16(R5 + oCamera_Unk_0a);

    R1 = scrollX;
    R4 = scrollY;
    R2 = LD16(R5 + oCamera_Unk_0a);

    /* oToolkit_RenderInfoPtr lives at offset 8 in the toolkit struct */
    u32 renderInfo = LD32(R10 + 8);

    R6 = LD16(renderInfo + oRenderInfo_Unk_14) + scrollX;
    R7 = LD16(renderInfo + oRenderInfo_Unk_16) + scrollY;
    ST16(renderInfo + oRenderInfo_Unk_14, R6);
    ST16(renderInfo + oRenderInfo_Unk_16, R7);

    /* sub_80269E2(r0 = render info, [sp] = scrollX, [sp+4] = scrollY) */
    R0 = renderInfo;
    SP -= 8;
    ST32(SP, scrollX);
    ST32(SP + 4, scrollY);
    GUEST_CALL(sub_80269E2);
    SP += 8;

    /* the callee may clobber r1/r4; the original pops its saved copies back */
    R1 = scrollX;
    R4 = scrollY;

    if (R0 != 0) {
        /* non-zero: the second layer pair must not scroll */
        SET_FLAGS_NZ(R0);
        R4 = inR4;
        R6 = inR6;
        R7 = inR7;
        RETURN_TO_CALLER();
    }

    R0 = LD32(R10 + 8); /* oToolkit_RenderInfoPtr, reloaded as in the original */
    R6 = LD16(renderInfo + oRenderInfo_Unk_18) + scrollX;
    u32 baseY = LD16(renderInfo + oRenderInfo_Unk_1a);
    R7 = baseY + scrollY;
    ST16(renderInfo + oRenderInfo_Unk_18, R6);
    ST16(renderInfo + oRenderInfo_Unk_1a, R7);
    SET_FLAGS_ADD(baseY, scrollY);

    R4 = inR4;
    R6 = inR6;
    R7 = inR7;
    RETURN_TO_CALLER();
}

/* ── camera_writeUnk03_14_80301b2 ── */
/* camera_writeUnk03_14_80301b2(r0 = byte value, r1 = word value):
 * Stores r0 into the camera struct's byte at offset 3 and r1 into its
 * Unk_14 word. R2 is left pointing at the camera struct. */
Next camera_writeUnk03_14_80301b2(void) {
    u32 camera = LD32(R10 + oToolkit_CameraPtr);

    ST8(camera + 3, R0);
    ST32(camera + oCamera_Unk_14, R1);

    R2 = camera;
    RETURN_TO_CALLER();
}

/* ── camera_CopyScrollToRenderLayers_80302F8 ── */
/* camera_CopyScrollToRenderLayers_80302F8: copy the camera's current position (low 8 bits of X and Y)
 * into the render info scroll fields. Called every frame before rendering.
 * r10 = toolkit; leaves r1 = X, r2 = Y, r3 = render info, r5 = camera,
 * r7 = toolkit, r4 = 0xFF, and nz from the last AND. */
Next camera_CopyScrollToRenderLayers_80302F8(void) {
    u32 toolkit = R10;

    u32 camera = LD32(toolkit + oToolkit_CameraPtr);
    u32 camX = LD16(camera + 4) & 0xFF;  /* oCamera_Unk_04 */
    u32 camY = LD16(camera + 6) & 0xFF;  /* oCamera_Unk_06 */

    u32 render = LD32(toolkit + 8); /* oToolkit_RenderInfoPtr */

    ST16(render + oRenderInfo_Unk_10, camX);
    ST16(render + oRenderInfo_Unk_14, camX);
    ST16(render + oRenderInfo_Unk_18, camX);
    ST16(render + oRenderInfo_Unk_12, camY);
    ST16(render + oRenderInfo_Unk_16, camY);
    ST16(render + oRenderInfo_Unk_1a, camY);

    /* Registers left as the original leaves them. */
    R7 = toolkit;
    R5 = camera;
    R4 = 0xFF;
    R3 = render;
    R1 = camX;
    R2 = camY;
    SET_FLAGS_NZ(R2); /* and r2, r4 was the last flag-setting instruction */

    RETURN_TO_CALLER();
}

/* ── renderInfo_SnapScrollToCamera_8030316 ── */
/* renderInfo_SnapScrollToCamera_8030316(r10 = toolkit):
 * Snap the render info to the camera: the low bytes of the camera's +04/+06
 * fields are stored as the scroll offsets at render info +0x10..+0x16, and
 * the +0x18/+0x1A offsets are cleared to zero.
 * Outputs: r1 = 0, r2 = camera y, r3 = render info ptr, r4 = 0xFF,
 * r5 = camera ptr, r7 = toolkit ptr, nz set from r1 = 0. */
Next renderInfo_SnapScrollToCamera_8030316(void) {
    u32 toolkit    = R10;
    u32 camera     = LD32(toolkit + oToolkit_CameraPtr);
    u32 renderInfo = LD32(toolkit + 8); /* oToolkit_RenderInfoPtr */

    u32 camX = LD16(camera + 4) & 0xFF; /* oCamera_Unk_04 */
    ST16(renderInfo + oRenderInfo_Unk_10, camX);
    ST16(renderInfo + oRenderInfo_Unk_14, camX);

    u32 camY = LD16(camera + 6) & 0xFF; /* oCamera_Unk_06 */
    ST16(renderInfo + oRenderInfo_Unk_12, camY);
    ST16(renderInfo + oRenderInfo_Unk_16, camY);

    R1 = 0;
    ST16(renderInfo + oRenderInfo_Unk_18, 0);
    ST16(renderInfo + oRenderInfo_Unk_1a, 0);

    R2 = camY;
    R3 = renderInfo;
    R4 = 0xFF;
    R5 = camera;
    R7 = toolkit;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── render_ClearScrollOffsets_8030336 ── */
/* render_ClearScrollOffsets_8030336 (r10 = toolkit):
 * Reset the render info scroll offsets.  The original reads the camera
 * position, masks each halfword down to 8 bits and stores it into the four
 * offset fields, then immediately overwrites all four with 0 — so the net
 * effect is that they are simply cleared.
 * Leaves r1 = 0 (nz from that), r2 = camera Y & 0xFF, r3 = render info ptr,
 * r4 = 0xFF, r5 = camera ptr, r7 = toolkit ptr. */
Next render_ClearScrollOffsets_8030336(void) {
    u32 toolkit = R10;
    u32 camera = LD32(toolkit + oToolkit_CameraPtr);
    u32 renderInfo = LD32(toolkit + 8); /* oToolkit_RenderInfoPtr */

    u32 camX = LD16(camera + 4) & 0xFF; /* oCamera_Unk_04 */
    u32 camY = LD16(camera + 6) & 0xFF; /* oCamera_Unk_06 */

    /* store the masked camera scroll, then zero it all out again */
    ST16(renderInfo + oRenderInfo_Unk_10, camX);
    ST16(renderInfo + oRenderInfo_Unk_14, camX);
    ST16(renderInfo + oRenderInfo_Unk_18, camX);
    ST16(renderInfo + oRenderInfo_Unk_12, camY);
    ST16(renderInfo + oRenderInfo_Unk_16, camY);
    ST16(renderInfo + oRenderInfo_Unk_1a, camY);

    R1 = 0;
    ST16(renderInfo + oRenderInfo_Unk_10, 0);
    ST16(renderInfo + oRenderInfo_Unk_12, 0);
    ST16(renderInfo + oRenderInfo_Unk_18, 0);
    ST16(renderInfo + oRenderInfo_Unk_1a, 0);

    R2 = camY;
    R3 = renderInfo;
    R4 = 0xFF;
    R5 = camera;
    R7 = toolkit;
    SET_FLAGS_NZ(0);

    RETURN_TO_CALLER();
}

/* ── mapTiles_ResetDimensionsToDefault_80303E8 ── */
/* mapTiles_ResetDimensionsToDefault_80303E8: reset the map tile grid dimensions to the default 32x32. */
Next mapTiles_ResetDimensionsToDefault_80303E8(void) {
    ST8(A_eMapTilesState200be70_MapWidth, 0x20);
    ST8(A_eMapTilesState200be70_MapWidth + 1, 0x20); /* MapHeight (byte_200BE71) */

    RETURN_TO_CALLER();
}

/* ── EnterMap_RunMapGroupAsmFunction_8030A00 ── */
/* EnterMap_RunMapGroupAsmFunction_8030A00(r0 = map group number):
 * Dispatch to the EnterMap handler for the given map group. The group
 * number selects one of three jumptables:
 *   >= UNKNOWN_MAP_GROUP_START (0xF0): "unknown" table, index minus 0xF0
 *   >= INTERNET_MAP_GROUP_START:       internet table, index minus the base
 *   otherwise:                          real-world table, index as-is
 * The handler is entered with R0 = its own jumptable entry address and
 * R1 = the base of its jumptable. The original achieves the call with
 * `mov lr, pc; bx r0`, i.e. the handler returns to the `pop {pc}` right
 * after the call, which returns straight to *our* caller — so this whole
 * function behaves as a tail call and the handler's registers (r2, r4-r12)
 * pass through unchanged to the caller. */

/* Value `mov lr, pc` leaves in LR at 0x08030A1C: the address of the
 * following `pop {pc}` (PC reads as current instruction + 4 in Thumb). */
#define HANDLER_RETURN_ADDRESS 0x08030A20u

Next EnterMap_RunMapGroupAsmFunction_8030A00(void) {
    u32 returnAddress = LR; /* our caller's return address, restored below */
    u32 group = R0;
    u32 table;

    if ((s32)group >= (s32)UNKNOWN_MAP_GROUP_START) {
        table = A_EnterMap_UnkMapGroupJumptable;
        group -= UNKNOWN_MAP_GROUP_START;
    } else if ((s32)group >= (s32)INTERNET_MAP_GROUP_START) {
        table = A_EnterMap_InternetMapGroupJumptable;
        group -= INTERNET_MAP_GROUP_START;
    } else {
        table = A_EnterMap_RealWorldMapGroupJumptable;
    }

    {
        /* Entries are word-sized function addresses. */
        u32 handler = LD32(table + (group << 2));

        R0 = handler; /* the handler sees its own entry address in R0 */
        R1 = table;   /* ...and its jumptable base in R1 */
        LR = HANDLER_RETURN_ADDRESS;

        /* Indirect guest call: the callee runs with the register state
         * above and returns to HANDLER_RETURN_ADDRESS, which hands
         * control back to us just like the original's pop {pc} would. */
        dispatch(handler);
    }

    /* The handler has returned "to the pop {pc}": return to our caller
     * with the handler's results in the registers it left behind. */
    LR = returnAddress;
    RETURN_TO_CALLER();
}

/* ── LoadGFXAnimsForMapGroup ── */
/* LoadGFXAnimsForMapGroup(r0 = map group, r1 = map number) -> ()
 * Loads the GFX anims for the given map group by dispatching through one of
 * three jumptables: real-world groups use their index directly, internet
 * groups and the trailing "unknown" range use indexes relative to the start
 * of their range. Each handler is entered with r0 = index * 4 (the byte
 * offset used to fetch its jumptable entry) and r2 = its own entry address,
 * and returns to the instruction following the dispatch, after which this
 * function returns to its own caller. */
Next LoadGFXAnimsForMapGroup(void)
{
    u32 return_lr = LR;
    s32 group = (s32)R0;
    u32 table;

    if (group >= (s32)UNKNOWN_MAP_GROUP_START) {
        table = A_UnkMapGroupRangeLoadGFXAnimsJumptable;
        group -= UNKNOWN_MAP_GROUP_START;
    } else if (group >= (s32)INTERNET_MAP_GROUP_START) {
        table = A_InternetMapGroupLoadGFXAnimsJumptable;
        group -= INTERNET_MAP_GROUP_START;
    } else {
        table = A_RealWorldMapGroupLoadGFXAnimsJumptable;
    }

    u32 handler = LD32(table + (u32)group * 4);

    R0 = (u32)group << 2;
    R2 = handler;

    /* mov lr, pc: the handler returns to the pop {pc} that follows the
     * dispatch, which returns to this function's caller. */
    LR = 0x08030A50;
    dispatch(handler); /* bx r2 */

    LR = return_lr;
    RETURN_TO_CALLER();
}

/* ── npc_freeAllObjectsThenSpawnObjectsFromList ── */
/* npc_freeAllObjectsThenSpawnObjectsFromList(r0 = pointer to an array of NPC
 * script pointers, terminated by a 0xFF entry):
 * Switch the overworld to a new NPC spawn list. If the game state already
 * points at this list, nothing happens. Otherwise the list pointer is stored
 * in the game state, every existing overworld NPC object is freed, and the
 * list is walked spawning one object per entry until the 0xFF terminator.
 * On return: r0 points at the terminator entry, r1 = number of objects
 * spawned (or the unchanged current list pointer on the early-exit path),
 * r2 = 0xFF; flags come from the last compare (list vs. current on early
 * exit, terminator vs. 0xFF otherwise). r4-r6 are preserved. */
Next npc_freeAllObjectsThenSpawnObjectsFromList(void) {
    /* The original spills r4-r6 and pops them back at the end, so they are
     * preserved no matter what the callees do. */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 list = R0;

    /* Already showing this list: nothing to do. The original loads the
     * current pointer into r1 before comparing, so r1 carries it (and the
     * compare result carries into the flags) on the early-exit path. */
    u32 current = LD32(gameState + oGameState_Ptr_20);
    R1 = current;
    SET_FLAGS_CMP(list, current);

    if (list != current) {
        ST32(gameState + oGameState_Ptr_20, list);

        /* Clear out all currently spawned overworld NPCs (object type 4). */
        R4 = gameState;
        R0 = 4; /* OVERWORLD_NPC_OBJECT_F */
        GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);

        /* Spawn one object per list entry; the 0xFF entry ends the list.
         * r1 counts the objects spawned, r0 walks the entry array and r2
         * holds the current entry's script pointer. */
        u32 index = 0;
        R1 = 0;
        for (;;) {
            u32 script = LD32(list);
            R2 = script;
            SET_FLAGS_CMP(script, 0xFF);
            if (script == 0xFF)
                break;

            R4 = gameState;
            R0 = list;
            GUEST_CALL(npc_spawnObjectThenSetUnk10_TempAnimScriptPtr_8030a8c);

            index++;
            R1 = index;
            list += 4;
            R0 = list;
        }
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── npc_spawnObjectThenSetUnk10_TempAnimScriptPtr_8030a8c ── */
/* npc_spawnObjectThenSetUnk10_TempAnimScriptPtr_8030a8c
 *     (r0 = ?, r1 = which NPC script, r2 = animation script ptr, r3, r4 = spawn args):
 * spawn an overworld NPC object (with the "initial script" argument forced to 0),
 * then store the script selector and the temp animation script pointer into the
 * new object. Leaves the new object pointer in r5 (0 if the spawn failed);
 * r0-r2 and the caller's other registers are restored. */
Next npc_spawnObjectThenSetUnk10_TempAnimScriptPtr_8030a8c(void) {
    /* Keep the arguments across the spawn call, which clobbers r0-r2 and r5. */
    u32 savedR0 = R0;
    u32 whichScript = R1;
    u32 animScriptPtr = R2;

    /* Spawn the object. Passing 0 here writes placeholder values into its
     * position and unk_04 fields, but the NPC script overwrites those anyway. */
    R0 = 0;
    GUEST_CALL(SpawnOverworldNPCObject);

    /* r5 = spawned object, or 0 if the spawn failed. */
    u32 npc = R5;
    if (npc != 0) {
        ST8(npc + oOverworldNPCObject_WhichNPCScript, whichScript);

        /* UnkFlags_60 is normally a flags word, but npc_init_809E590
         * repurposes it as temp storage for the animation script pointer. */
        ST32(npc + oOverworldNPCObject_UnkFlags_60, animScriptPtr);
    }

    /* Restore the arguments exactly as the original's push/pop pair did. */
    R0 = savedR0;
    R1 = whichScript;
    R2 = animScriptPtr;

    RETURN_TO_CALLER();
}

/* ── sub_8030CAC ── */
/* Field offsets in OWObjectCoords used by the Bresenham line routine */
#define OW_CUR_X   2
#define OW_CUR_Y   6
#define OW_TGT_X   0xE
#define OW_TGT_Y   0x12

/* register form: calls sub_8030DA2 outside this file, which has no typed C interface yet.
 * Passes the full register state the original had at each call site. */
static void draw_tile(u32 r0, u32 r1, u32 r2, u32 r3,
                      u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0; R1 = r1; R2 = r2; R3 = r3;
    R4 = r4; R5 = r5; R6 = r6; R7 = r7;
    GUEST_CALL(sub_8030DA2);
}

/* Bresenham line-draw on OWObjectCoords: draws from target to current position.
 * Returns the step_x value that ends up in R3 for the caller. */
static u32 ow_line_draw(u32 obj, u32 ctx) {
    /* First draw at the "next" position. */
    s32 next_x = (s16)LDS16(obj + oOWObjectCoords_NextX);
    s32 next_y = (s16)LDS16(obj + oOWObjectCoords_NextY);
    draw_tile(obj, 0, 0, 0, (u32)next_x, ctx, 0, (u32)next_y);

    /* Deltas from current (offset 2/6) to target (offset 0xE/0x12). */
    s32 dx = (s16)LDS16(obj + OW_CUR_X) - (s16)LDS16(obj + OW_TGT_X);
    s32 dy = (s16)LDS16(obj + OW_CUR_Y) - (s16)LDS16(obj + OW_TGT_Y);

    s32 adx = dx < 0 ? -dx : dx;
    s32 ady = dy < 0 ? -dy : dy;

    if (adx > ady) {
        /* X-dominant: step along x, conditionally adjust y. */
        s32 x = (s16)LDS16(obj + OW_TGT_X);
        s32 end_x = (s16)LDS16(obj + OW_CUR_X);
        s32 y = (s16)LDS16(obj + OW_TGT_Y);
        s32 sx = (dx < 0) ? -1 : 1;
        s32 sy = (dy < 0) ? -1 : 1;
        s32 err = -adx - 1;   /* mvn r1 after making adx positive */

        for (;;) {
            x += sx;
            if (x == end_x) break;
            err += 2 * ady;
            if (err >= adx) { y += sy; err -= 2 * adx; }
            draw_tile((u32)sy, (u32)adx, (u32)ady, (u32)sx,
                      (u32)x, (u32)err, (u32)end_x, (u32)y);
        }

        /* Final draw at cur position. */
        draw_tile(obj, (u32)adx, (u32)ady, (u32)sx,
                  (u32)end_x, ctx, (u32)end_x, (u32)(s16)LDS16(obj + OW_CUR_Y));
        return (u32)sx;

    } else if (adx < ady) {
        /* Y-dominant: step along y, conditionally adjust x. */
        s32 x = (s16)LDS16(obj + OW_TGT_X);
        s32 y = (s16)LDS16(obj + OW_TGT_Y);
        s32 end_y = (s16)LDS16(obj + OW_CUR_Y);
        s32 sx = (dx < 0) ? -1 : 1;
        s32 sy = (dy < 0) ? -1 : 1;
        s32 err = -ady - 1;   /* mvn r2 after making ady positive */

        for (;;) {
            y += sy;
            if (y == end_y) break;
            err += 2 * adx;
            if (err >= ady) { x += sx; err -= 2 * ady; }
            draw_tile((u32)sy, (u32)adx, (u32)ady, (u32)sx,
                      (u32)x, (u32)err, (u32)end_y, (u32)y);
        }

        draw_tile(obj, (u32)adx, (u32)ady, (u32)sx,
                  (u32)(s16)LDS16(obj + OW_CUR_X), ctx,
                  (u32)end_y, (u32)(s16)LDS16(obj + OW_CUR_Y));
        return (u32)sx;

    } else if (dx != 0) {
        /* |dx| == |dy|, diagonal: step both axes every iteration. */
        s32 x = (s16)LDS16(obj + OW_TGT_X);
        s32 y = (s16)LDS16(obj + OW_TGT_Y);
        s32 end_y = (s16)LDS16(obj + OW_CUR_Y);
        s32 sx = (dx < 0) ? -1 : 1;
        s32 sy = (dy < 0) ? -1 : 1;

        for (;;) {
            y += sy;
            if (y == end_y) break;
            x += sx;
            draw_tile((u32)sy, (u32)adx, (u32)ady, (u32)sx,
                      (u32)x, ctx, (u32)end_y, (u32)y);
        }

        draw_tile(obj, (u32)adx, (u32)ady, (u32)sx,
                  (u32)(s16)LDS16(obj + OW_CUR_X), ctx,
                  (u32)end_y, (u32)(s16)LDS16(obj + OW_CUR_Y));
        return (u32)sx;

    } else {
        /* Degenerate: dx == 0 and dy == 0, no line to draw. */
        s32 tgt_y = (s16)LDS16(obj + OW_TGT_Y);
        draw_tile(obj, 0, 0, (u32)tgt_y,
                  (u32)(s16)LDS16(obj + OW_CUR_X), ctx, 0,
                  (u32)(s16)LDS16(obj + OW_CUR_Y));
        return (u32)tgt_y;
    }
}

/* sub_8030CAC(r0 = OWObjectCoords*, r5 = draw context) -> r0, r3, r5, r7, r8 */
Next sub_8030CAC(void) {
    R3 = ow_line_draw(R0, R5);
    RETURN_TO_CALLER();
}

/* ── camera_moveByDelta_80310B0 ── */
/* camera_moveByDelta_80310B0(r0, r1): shift the camera position by a delta.
 * The deltas in r0/r1 are first shifted left by 14 (coarse units to the
 * internal fixed-point scale). If both shifted deltas are zero, nothing
 * happens. Otherwise the camera position struct pointed to by
 * *dword_200F3D0 (x at +0, y at +4) gets the shifted delta plus the current
 * scroll offsets at dword_200F3F8 (x) / dword_200F3FC (y) added to it.
 *
 * Results: r3 = 1 if the position was updated, 0 if skipped; r4 = scroll
 * state base, r6 = new y. c/v come from the final y add on the update path,
 * and from the "r3 == 0" comparison on the skip path. */
Next camera_moveByDelta_80310B0(void) {
    u32 dx = R0 << 14;
    u32 dy = R1 << 14;
    u32 moved = 1;

    if (dx == 0 && dy == 0)
        moved = 0;

    R3 = moved;
    if (moved == 0) {
        SET_FLAGS_CMP(R3, 0);
        RETURN_TO_CALLER();
    }

    u32 camPos = LD32(A_dword_200F3D0); /* camera position struct */
    u32 scroll = A_dword_200F3E0;       /* screen scroll state block */

    u32 x = LD32(camPos) + dx;
    x = x + LD32(scroll + 0x18); /* dword_200F3F8: scroll offset x */
    ST32(camPos, x);

    /* The outgoing c/v flags come from the final ADD of scrollY to the
     * delta-adjusted y (before the scroll offset is folded into the value
     * stored). Compute the add operands separately so the flags match. */
    u32 yBase = LD32(camPos + 4) + dy;
    u32 scrollY = LD32(scroll + 0x1C); /* dword_200F3FC: scroll offset y */

    R6 = yBase + scrollY;
    SET_FLAGS_ADD(yBase, scrollY);
    ST32(camPos + 4, R6);

    R4 = scroll;
    RETURN_TO_CALLER();
}

/* ── sprite_worldToScreenPos_80310EC ── */
/* sprite_worldToScreenPos_80310EC(r0 = sprite position struct, r5 = parent object, r6 = world coords):
 * Converts a sprite's world coordinates to screen coordinates.
 *
 * r5 -> +0x10: camera/screen struct; its +4/+8 fields (<<2) are the pixel
 *              scroll offsets that get subtracted from every result.
 * r6 -> +0x00/+0x04: fixed-point (1/16384) x/y base position,
 *      +0x0C/+0x10: already-scaled x/y positions used as-is.
 * r0 -> +0x18/+0x1C: per-object x/y offsets added to the base position;
 *      results are stored to r0's +0x00/+0x04/+0x0C/+0x10 fields. */
Next sprite_worldToScreenPos_80310EC(void) {
    u32 *pos = (u32 *)R0;
    u32 camera = LD32(R5 + 0x10);
    s32 scrollX = (s32)LD32(camera + 4) << 2;
    s32 scrollY = (s32)LD32(camera + 8) << 2;
    u32 *world = (u32 *)R6;

    pos[0] = (u32)((((s32)world[0] + (s32)LD32(R0 + 0x18)) >> 14) - scrollX);
    pos[1] = (u32)((((s32)world[1] + (s32)LD32(R0 + 0x1C)) >> 14) - scrollY);
    pos[3] = (u32)(((s32)world[3] >> 14) - scrollX);
    pos[4] = (u32)(((s32)world[4] >> 14) - scrollY);
    RETURN_TO_CALLER();
}

/* ── sub_8031122 ── */
/* sub_8031122(r0 = state object, r5 = forwarded to callee):
 * Zeroes the two counter/flag words of the state struct at dword_200F3E0
 * (fields dword_200F3F8 and dword_200F3FC), then runs sprite_worldToScreenPos_80310EC on it.
 * The original r0 argument is handed to the callee in r6; r0 comes back
 * holding the callee's result. */
Next sub_8031122(void) {
    u32 stateObj = R0;
    u32 state = A_dword_200F3E0;

    ST32(state + 0x18, 0); /* dword_200F3F8 = 0 */
    ST32(state + 0x1C, 0); /* dword_200F3FC = 0 */

    R0 = state;
    R6 = stateObj;
    GUEST_CALL(sprite_worldToScreenPos_80310EC);

    RETURN_TO_CALLER();
}

/* ── sub_8031388 ── */
/* sub_8031388(r0-r2 = parameters, r5 = parameter, r12 = state flags):
 * Runs the three-step sequence shared by this family of routines:
 *   1. sub_8031136 on the caller's r0-r2; its r0/r1 results are fed
 *      straight into step 2 (the original never reloads them),
 *   2. sub_8030E14 with the "unbounded" extent markers (r2 = r3 = -1),
 *      an extent count of 0x20, r5 = 0 and r6 = 1,
 *   3. camera_moveByDelta_80310B0 with r1 = 0 to finish.
 * Afterwards the low bit of the r12 state flags is set (state marked
 * active) and nz reflects that value; c/v are whatever the last callee
 * left. r0, r1, r2 and r5 are restored to their entry values (the
 * original's push/pop pair); r3, r4 and r6 return whatever camera_moveByDelta_80310B0
 * left in them. */
Next sub_8031388(void) {
    u32 ret_addr = LR;   /* push {lr} */
    u32 arg0 = R0, arg1 = R1, arg2 = R2, arg5 = R5;

    /* Step 1: r0-r2 are passed through unchanged. */
    GUEST_CALL(sub_8031136);

    /* Step 2: r0/r1 still carry step 1's results here. */
    R2 = (u32)-1;
    R3 = (u32)-1;
    R4 = 0x20;
    R5 = 0;
    R6 = 1;
    GUEST_CALL(sub_8030E14);

    /* Step 3. */
    R1 = 0;
    GUEST_CALL(camera_moveByDelta_80310B0);

    /* Mark the state active: set the low bit of the current r12 flags
     * (orr sets nz from the result, c/v stay as the callee left them). */
    R0 = R12 | 1;
    SET_FLAGS_NZ(R0);
    R12 = R0;

    /* pop {r0-r2,r5,pc}: restore the caller's registers and return to
     * the address the caller left in LR. */
    R0 = arg0;
    R1 = arg1;
    R2 = arg2;
    R5 = arg5;
    LR = ret_addr;
    RETURN_TO_CALLER();
}

/* ── battle_setupMessageWindow_80313B0 ── */
/* battle_setupMessageWindow_80313B0: three-step battle-message/textbox setup.
 *
 * Runs the prepare, layout and finalize passes in sequence, then marks the
 * request as handled by setting bit 0 of the r12 flag word.  The caller's
 * r0, r1, r2 and r5 are preserved across the whole sequence; flags on exit
 * reflect (r12 | 1), as set by the final ORR in the original. */
Next battle_setupMessageWindow_80313B0(void) {
    u32 savedR0 = R0;
    u32 savedR1 = R1;
    u32 savedR2 = R2;
    u32 savedR5 = R5;

    /* step 1: prepare state (reads r0-r2 and the r12 flag word) */
    GUEST_CALL(sub_8031176);

    /* step 2: lay out into a 32x32 region with blank origin and one style */
    R2 = 0x20;
    R4 = 0;
    R3 = 0x20;
    R5 = 0x20;
    R6 = 1;
    GUEST_CALL(sub_8030E8C);

    /* step 3: finalize pass with r1 = 0 */
    R1 = 0;
    GUEST_CALL(camera_moveByDelta_80310B0);

    /* mark the request as handled in the caller's r12 flag word */
    R12 = R12 | 1;

    /* restore the caller's saved registers (the original's push/pop) */
    R0 = savedR0;
    R1 = savedR1;
    R2 = savedR2;
    R5 = savedR5;
    SET_FLAGS_NZ(R12);
    RETURN_TO_CALLER();
}

/* ── battle_runTextboxUpdateAndSetStateBit1_80313D4 ── */
/* battle_runTextboxUpdateAndSetStateBit1_80313D4: run the common "battle text box" update sequence and mark
 * something in the r12 flag word as done (bit 1 set).
 *
 * r0/r1/r2 hold arguments for the first callee, r5/r12 are live state that
 * must survive the whole sequence, so they are saved and restored around it.
 * Leaves r3/r4/r6/r7-r9 as the callees left them, NZ from the final ORR. */
Next battle_runTextboxUpdateAndSetStateBit1_80313D4(void) {
    u32 saved_r0 = R0, saved_r1 = R1, saved_r2 = R2, saved_r5 = R5;
    u32 flag_word = R12;

    /* First step: (reads r1 and the r12 state word) */
    GUEST_CALL(sub_8031136);

    /* Second step: called with r2 = 0x20, r3 = 0, r4 = -1, r5 = -1, r6 = 1
     * (fill/clear style parameters); r12 state word still live. */
    R2 = 0x20;
    R3 = 0;
    R4 = 0 - 1;
    R5 = 0 - 1;
    R6 = 1;
    GUEST_CALL(sub_8030F04);

    /* Third step: called with r0 = 0. Its results in r3/r4/r6 stay live. */
    R0 = 0;
    GUEST_CALL(camera_moveByDelta_80310B0);

    /* Mark the flag word: r12 |= 2, flags from the ORR. */
    R12 = flag_word | 2;
    SET_FLAGS_NZ(R12);

    /* Restore the caller's r0-r2/r5 (the original pushes/pops them). */
    R0 = saved_r0;
    R1 = saved_r1;
    R2 = saved_r2;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── sub_80313FC ── */
/* sub_80313FC: runs a three-step sequence (sub_8031176; sub_8030F7C with a fixed
 * parameter set of 0x20/0x20/0x20/1; camera_moveByDelta_80310B0 with 0) and then sets bit 1 of
 * the state word passed in r12.  The caller's r0, r1, r2 and r5 are preserved
 * across the calls; the Z/N flags reflect the updated r12 value. */
Next sub_80313FC(void) {
    u32 inR0 = R0, inR1 = R1, inR2 = R2, inR5 = R5;
    u32 state = R12;

    GUEST_CALL(sub_8031176);

    R2 = 0;
    R3 = 0x20;
    R4 = 0x20;
    R5 = 0x20;
    R6 = 1;
    GUEST_CALL(sub_8030F7C);

    R0 = 0;
    GUEST_CALL(camera_moveByDelta_80310B0);

    state |= 2;
    R12 = state;
    SET_FLAGS_NZ(R12);

    R0 = inR0;
    R1 = inR1;
    R2 = inR2;
    R5 = inR5;
    RETURN_TO_CALLER();
}

/* ── math_pointCrossHalfPlaneTest_8031594 ── */
/* math_pointCrossHalfPlaneTest_8031594(r0 = pointer to a {x, y} point, r2..r5 = edge coordinates):
 * Half-plane test used for polygon/hit-area rasterization. The point pointed
 * to by r0 is tested against the directed edge running from (r3, r5) to
 * (r2, r4). Returns r0 = 1 when the point lies on the negative (clockwise)
 * side of the edge, r0 = 0 when it lies on the edge or on the other side.
 *
 * The partial terms of the cross product are also left behind in registers
 * as side effects (callers of the surrounding fill routine reuse them):
 *   r6 = by - ay,  r7 = ax - bx,  r2 = bx*ay - ax*by,
 *   r3 = (by - ay)*point.x + (ax - bx)*point.y,
 *   r4 = (ax - bx)*point.y
 */
Next math_pointCrossHalfPlaneTest_8031594(void) {
    u32 pt = R0;
    u32 ax = R2, bx = R3, ay = R4, by = R5;

    u32 dy = by - ay;               /* -> r6 */
    u32 dx = ax - bx;               /* -> r7 */
    u32 base = bx * ay - ax * by;   /* -> r2 */

    R4 = LD32(pt + 4) * dx;         /* (ax - bx) * point.y -> r4 */
    R3 = LD32(pt) * dy + R4;        /* (by - ay)*point.x + r4 term -> r3 */

    s32 cross = (s32)base + (s32)R3;

    R2 = base;
    R6 = dy;
    R7 = dx;
    R0 = (cross < 0) ? 1 : 0;
    RETURN_TO_CALLER();
}

/* ── math_pointLeftOfLineTest_80315DC ── */
/* math_pointLeftOfLineTest_80315DC(point = r0, p1x = r2, p2x = r3, p1y = r4, p2y = r5):
 * Line-side test. The point P = {x, y} is read from the struct at r0, and
 * P1 = (p1x, p1y), P2 = (p2x, p2y) come in as arguments. Computes the signed
 * cross product (P - P1) x (P2 - P1) — i.e. which side of the directed line
 * P1 -> P2 the point P lies on — and returns 1 in r0 when it is positive
 * (P is on the left of P1 -> P2), 0 otherwise. r5 is scratch. */
Next math_pointLeftOfLineTest_80315DC(void) {
    u32 px = LD32(R0);
    u32 py = LD32(R0 + 4);
    u32 p1x = R2, p2x = R3, p1y = R4, p2y = R5;

    /* Left normal of the direction P1 -> P2. */
    u32 normX = p2y - p1y;                       /* -> r6 */
    u32 normY = p1x - p2x;                       /* -> r7 */

    /* Signed distance of P from the line, as n . P + offset where the
     * offset places the line through P1 (== P2). */
    u32 dotX = px * normX;                       /* -> r3 */
    u32 dotY = py * normY;                       /* -> r4 */
    u32 offset = p2x * p1y - p1x * p2y;          /* -> r2 */

    /* The original accumulates the dot products into r3 (add r3, r3, r4),
     * then adds the offset into r5, so r3 ends up holding dotX + dotY. */
    u32 dot = dotX + dotY;                       /* -> r3 */
    u32 side = (s32)(dot + offset);              /* -> r5 */

    R6 = normX;
    R7 = normY;
    R3 = dot;
    R4 = dotY;
    R2 = offset;
    R0 = (s32)side > 0 ? 1u : 0u;

    RETURN_TO_CALLER();
}

/* ── sub_8031600 ── */
/* sub_8031600(r0 = pointer to a text command entry):
 * Feed one text command to the text printer state block at dword_2011A20:
 * copy the command's halfword operand into the printer's current-character
 * slot, advance the printer's read pointer past this 4-byte entry, and set
 * the two trailing state bytes to 0xFE (the printer's idle/end marker). */
Next sub_8031600(void) {
    u32 state = A_dword_2011A20;
    u32 cmd   = R0;

    ST16(state + 4, LD32(cmd));   /* word_2011A24: operand of this command */
    ST32(state, cmd + 4);         /* next command follows this one */
    ST8(state + 6, 0xFE);         /* byte_2011A26 */
    ST8(state + 7, 0xFE);         /* byte_2011A27 */

    RETURN_TO_CALLER();
}

/* ── battle_calcRelPosToTarget_803172E ── */
/* battle_calcRelPosToTarget_803172E(r0 = entity position block, r5 = target block):
 * Writes four values into the dword_200F3E0 scratch array, each being one of
 * the entity's position fields converted from fixed-point (>> 15) minus twice
 * the matching field of the target:
 *   [+0x00] = (x     >> 15) - 2 * target[+0x0C]
 *   [+0x04] = (y     >> 15) - 2 * target[+0x10]
 *   [+0x0C] = (nextX >> 15) - 2 * target[+0x0C]
 *   [+0x10] = (nextY >> 15) - 2 * target[+0x10]
 * Returns r0 = &dword_200F3E0, r6 = the position block pointer, and r7 =
 * the last value stored. */
Next battle_calcRelPosToTarget_803172E(void) {
    u32 pos = R0;

    /* Target reference coordinates, scaled by two. */
    u32 targetX = LD32(R5 + 0xC) << 1;
    u32 targetY = LD32(R5 + 0x10) << 1;

    R6 = pos;
    R0 = A_dword_200F3E0;

    s32 relX     = (s32)LD32(pos) >> 15;
    s32 relY     = (s32)LD32(pos + 4) >> 15;
    s32 nextRelX = (s32)LD32(pos + 0xC) >> 15;
    s32 nextRelY = (s32)LD32(pos + 0x10) >> 15;

    R7 = relX - targetX;
    ST32(A_dword_200F3E0, R7);

    R7 = relY - targetY;
    ST32(A_dword_200F3E0 + 4, R7);

    R7 = nextRelX - targetX;
    ST32(A_dword_200F3E0 + 0xC, R7);

    R7 = nextRelY - targetY;
    ST32(A_dword_200F3E0 + 0x10, R7);

    RETURN_TO_CALLER();
}

/* ── table_GetCurrentEntryHeader_8031760 ── */
/* table_GetCurrentEntryHeader_8031760(r0 = mode): read the header of the current entry from the table
 * whose base pointer is stored at dword_2011A20 (the entry pointer itself is
 * the dword_2011A34 field at +0x14).
 *   r0 = sign-extended byte at +0 (count/charge, signed)
 *   r3 = byte at +1
 *   r2 = entry pointer
 *   r1 = byte at +2 when mode == 1, otherwise 0
 *   r5 = table base pointer */
Next table_GetCurrentEntryHeader_8031760(void) {
    u32 mode = R0;

    R5 = A_dword_2011A20;
    u32 entry = LD32(R5 + 0x14); /* dword_2011A34 */
    R2 = entry;

    R0 = (s8)LD8(entry + 0);
    R3 = LD8(entry + 1);

    if (mode == 1)
        R1 = LD8(entry + 2);
    else
        R1 = 0;

    RETURN_TO_CALLER();
}

/* ── chatbox_loadScriptWordAndResetScroll_803189C ── */
/* chatbox_loadScriptWordAndResetScroll_803189C(r0 = pointer): initialize the state block at dword_2013940 from a
 * pointer stored in r0. The halfword pointed to by r0 is latched into
 * word_2013944, the pointer itself is advanced by 4 and saved at
 * dword_2013940, and both byte_2013946 and byte_2013947 are reset to 0xFE. */
Next chatbox_loadScriptWordAndResetScroll_803189C(void) {
    u32 state = A_dword_2013940;
    u32 value = LD32(R0);

    ST16(state + 4, value);   /* word_2013944 = *r0 */
    ST32(state, R0 + 4);      /* dword_2013940 = r0 + 4 */

    ST8(state + 6, 0xFE);     /* byte_2013946 = 0xFE */
    ST8(state + 7, 0xFE);     /* byte_2013947 = 0xFE */

    RETURN_TO_CALLER();
}

/* ── sub_8031980 ── */
/* sub_8031980(r0 = selector):
 * Loads the address of dword_2013940 into r5 and returns a small mode id
 * in r0: 3 when the selector is 1, otherwise 2. The caller can also branch
 * on the flags left behind: n/z come from the returned value, c/v from the
 * comparison against 1. */
Next sub_8031980(void) {
    R5 = A_dword_2013940;

    SET_FLAGS_CMP(R0, 1);
    R0 = (R0 == 1) ? 3 : 2;
    /* the final MOVS only rewrites n/z; c/v stay from the CMP above */
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── sub_8031A5E ── */
/* sub_8031A5E: trivial wrapper that calls sub_8031980 with r0 = 1.
 * Everything the callee leaves behind (r0, r5, flags, lr at the return
 * address) is passed straight through to our caller. */
Next sub_8031A5E(void) {
    R0 = 1;
    GUEST_CALL(sub_8031980);
    RETURN_TO_CALLER();
}

/* ── overworldObject55_ReadStreamEntry_8031A68 ── */
/* overworldObject55_ReadStreamEntry_8031A68(r0 = pointer into a data stream):
 * Reads one word from the stream at r0, stores its low halfword into the
 * eOverwordMapObject55 struct's value field (+4), advances the stream
 * pointer by one entry and records it in the struct (+0), then marks both
 * terminator bytes (+6, +7) as 0xFE. */
Next overworldObject55_ReadStreamEntry_8031A68(void) {
    u32 state = A_eOverworldMapObject55_Size;

    u32 value = LD32(R0);
    ST16(state + 4, value);          /* word_2013924: low halfword of the entry */

    R0 = R0 + 4;                     /* skip to the next entry in the stream */
    ST32(state, R0);                 /* dword_2013920: updated read pointer */

    ST8(state + 6, 0xFE);            /* byte_2013926: terminator/sentinel */
    ST8(state + 7, 0xFE);            /* byte_2013927: terminator/sentinel */

    RETURN_TO_CALLER();
}

/* ── sub_8031B7C ── */
/* sub_8031B7C(r0 = query id):
 * Fetches one byte out of the overworld map object's state. With r0 == 1 it
 * follows dword_2013920 to the object, reads its pointer at +0x14, and returns
 * the byte at +1 of that structure; any other r0 returns 0. Callers also
 * receive the intermediate pointers in r5 (object table) and r2 (structure),
 * plus the flags of the `cmp r0, #1` (and on the zero path the subsequent
 * `mov r0, #0` updates n/z on top of them). */
Next sub_8031B7C(void) {
    u32 mode = R0;

    R5 = A_eOverworldMapObject55_Size;
    SET_FLAGS_CMP(mode, 1);             /* cmp r0, #1 */

    if (mode == 1) {
        R2 = LD32(R5 + 0x14);           /* -> dword_2013934 */
        R0 = LD8(R2 + 1);
    } else {
        R0 = 0;
        SET_FLAGS_NZ(R0);               /* mov r0, #0 */
    }

    RETURN_TO_CALLER();
}

/* ── text_setupTextboxAndStartPrint_8031BB2 ── */
/* text_setupTextboxAndStartPrint_8031BB2 (r0, r5 = arguments passed through to the first helper):
 * text-engine routine that runs the standard three-step textbox setup:
 * initialise the textbox state, open its window box, then kick off the
 * text print. All further register results are simply whatever the last
 * called helper leaves behind (r2 is set to 8, r3 to 8, r4 to 0 and
 * r5 to 0x10 before the window call). */
Next text_setupTextboxAndStartPrint_8031BB2(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E); /* initialise textbox state (uses r0, r5) */

    R2 = 8;
    R4 = 0;
    R3 = 8;
    R5 = 0x10;
    GUEST_CALL(sub_80315B8); /* open the message window box */

    GUEST_CALL(sub_8031B7C); /* start the text print */

    RETURN_TO_CALLER();
}

/* ── textbox_StepAndRefreshWindow_8031BCA ── */
/* textbox_StepAndRefreshWindow_8031BCA: text-box helper. Runs one step of the text engine, then
 * blits a window region (x=0/16, y=8 wide) and refreshes the box.
 * Inputs arrive in r0/r5 for the first callee; everything else is set up
 * here. All final register/flag state comes out of the last callee. */
Next textbox_StepAndRefreshWindow_8031BCA(void) {
    /* Advance the text engine state (reads r0/r5, returns updated r0). */
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    /* Blit parameters: source column 0, row 8, width 0x10, height 8.
     * Results come back in r2/r3/r4; r5 is preserved by the callee. */
    R2 = 0;
    R4 = 8;
    R3 = 0x10;
    R5 = 8;
    GUEST_CALL(sub_80315B8);

    /* Redraw/refresh the text box using the blit results in r2..r5. */
    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031BE2 ── */
/* sub_8031BE2: sequence of three setup/update calls.
 *
 * R0 and R5 arrive as arguments. The first callee (battle_calcRelPosToTarget_803172E) is invoked
 * with them unchanged. Then fixed tile/map parameters are loaded into
 * r2-r5 (0, 0x10, 0xC, 0xC) for math_pointCrossHalfPlaneTest_8031594, and finally sub_8031B7C is
 * called with whatever registers math_pointCrossHalfPlaneTest_8031594 left behind (r0 and r5 are
 * carried through). All register and flag effects on return come from the
 * last callee, so nothing needs to be set explicitly here. */
Next sub_8031BE2(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R4 = 0xC;
    R3 = 0x10;
    R5 = 0xC;

    GUEST_CALL(math_pointCrossHalfPlaneTest_8031594);
    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031BFA ── */
/* sub_8031BFA:
 * Runs battle_calcRelPosToTarget_803172E on the values the caller left in r0/r5, then invokes
 * sub_8031570 with a fixed argument set (r2 = 0, r3 = r4 = 0x10, r5 = 0)
 * and finally sub_8031B7C. Register results and flags are whatever the
 * last callee leaves behind; lr is untouched (the original just pushes
 * lr and pops it back into pc). */
Next sub_8031BFA(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R4 = 0x10;
    R3 = 0x10;
    R5 = 0;
    GUEST_CALL(sub_8031570);

    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031C2A ── */
/* sub_8031C2A(r0, r5 = context):
 * Runs a short helper sequence: first battle_calcRelPosToTarget_803172E on the incoming context,
 * then sub_80315B8 with the fixed arguments (r2 = 1, r3 = 0x10, r4 = 1,
 * r5 = 0x10), and finally sub_8031B7C. The caller sees whatever registers
 * and flags sub_8031B7C leaves behind. */
Next sub_8031C2A(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 1;
    R4 = 1;
    R3 = 0x10;
    R5 = 0x10;
    GUEST_CALL(sub_80315B8);

    GUEST_CALL(sub_8031B7C);
    RETURN_TO_CALLER();
}

/* ── sub_8031C42 ── */
/* sub_8031C42(r0, r5): thin wrapper that chains three battle-helper calls.
 *
 * The caller's r0/r5 are passed straight through to battle_calcRelPosToTarget_803172E; afterwards
 * a fixed parameter block (r2 = 0, r3 = 14, r4 = 14, r5 = 0) is set up for
 * math_pointLeftOfLineTest_80315DC, and whatever it leaves in r0/r2/r3/r4/r5 is handed on to
 * sub_8031B7C, whose results (registers and flags) become this function's
 * results. */
Next sub_8031C42(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R3 = 0xE;
    R4 = 0xE;
    R5 = 0;

    GUEST_CALL(math_pointLeftOfLineTest_80315DC);
    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}


/* FreeOWPlayerObject(r5 = player object header):
 * Releases the overworld player object. Clears its slot bit
 * (0x80000000 >> listIndex) in the active-overworld-player bitmask,
 * clears the object's flags byte, and makes its sprite unscalable.
 * Leaves r0..r3 and the flags as sprite_makeUnscalable leaves them. */
Next FreeOWPlayerObject(void) {
    u32 obj = R5;

    /* push {lr}: the callee runs with SP four bytes below entry SP,
     * and the saved value becomes the final LR (pop {pc}). */
    u32 returnAddr = LR;
    SP -= 4;
    ST32(SP, returnAddr);

    /* Each active player object owns one bit of the shared bitmask,
     * selected by its position in the object list. A register LSR with
     * a count of 32 or more produces 0 on ARM, i.e. no bit is cleared. */
    u32 listIndex = LD8(obj + oObjectHeader_ListIndex);
    u32 bit = (listIndex < 32) ? (0x80000000u >> listIndex) : 0;

    u32 bitfieldAddr = A_eActiveOWPlayerObjectBitfield;
    u32 cleared = LD32(bitfieldAddr) & ~bit;
    ST32(bitfieldAddr, cleared);

    ST8(obj + oObjectHeader_Flags, 0);

    /* Match the register state the original has at the call site
     * (r0 = the shifted bit, r1 = 0 from the flags store,
     *  r2 = the new bitmask value). */
    R0 = bit;
    R1 = 0;
    R2 = cleared;
    GUEST_CALL(sprite_makeUnscalable);

    /* pop {pc} */
    returnAddr = LD32(SP);
    SP += 4;
    LR = returnAddr;

    RETURN_TO_CALLER();
}

/* ── sub_8031C5A ── */
/* sub_8031C5A: enable the flag passed to sub_8031B7C as r0 = 1 (e.g. mark
 * something as done/on); all results (r0, r2, r5, flags) come straight from
 * the callee. */
Next sub_8031C5A(void) {
    R0 = 1;
    GUEST_CALL(sub_8031B7C);
    RETURN_TO_CALLER();
}

/* ── sub_8031C7C ── */
/* sub_8031C7C — battle helper that runs a short setup sequence:
 * 1. battle_calcRelPosToTarget_803172E with the caller's r0/r5 arguments,
 * 2. sub_80315B8 with a fixed parameter set (14, 14, 0, 16),
 * 3. sub_8031B7C to finish.
 * All results (r0, r2-r7) and the NZCV flags are whatever the last
 * callee leaves behind. */
Next sub_8031C7C(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0xE;
    R4 = 0;
    R3 = 0xE;
    R5 = 0x10;

    GUEST_CALL(sub_80315B8);
    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031C94 ── */
/* sub_8031C94: run a three-step sequence on the object passed in r0/r5.
 * Calls battle_calcRelPosToTarget_803172E first, then sub_80315B8 with a fixed parameter set
 * (r2=0, r3=0x10, r4=2, r5=2), then sub_8031B7C, which picks up whatever
 * r2/r5 sub_80315B8 left behind. r3/r4 must survive the final call.
 * Everything (registers and flags) is simply passed through to the caller. */
Next sub_8031C94(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R4 = 2;
    R3 = 0x10;
    R5 = 2;
    GUEST_CALL(sub_80315B8);

    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}
