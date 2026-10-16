/*
 * asm03_0_802F2C8.c: readable functions of bn6f asm03_0.s, from 0x0802F2C8, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── resetPaletteScratch_802F2C8 ── */
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

/* resetPaletteScratch_802F2C8() -> c/v left by ByteFill */
Next resetPaletteScratch_802F2C8(void) {
    reset_palette_scratch(A_byte_2006530, A_dword_2001140);
    RETURN_TO_CALLER();
}

/* ── sprite_queueOamEntryThunk_802FE28 ── */
/* sprite_queueOamEntryThunk_802FE28(r0..r3 = args for sprite_queueOamEntry, lr = return):
 *   Thumb trampoline that tail-calls sprite_queueOamEntry in ARM mode while
 *   passing the same r0..r3 and lr unchanged, so the callee returns directly
 *   to this function's caller. */
Next sprite_queueOamEntryThunk_802FE28(void) {
    /* The original switches to sprite_queueOamEntry's ARM entry (+1 for Thumb
     * bit) by setting LR = return_pc and BXing there, which makes the callee's
     * normal `pop {pc}` return straight to sprite_queueOamEntryThunk_802FE28's caller. */
    GUEST_CALL(sprite_queueOamEntry);
    RETURN_TO_CALLER();
}

/* ── battle_allocateActorIndex_802FE48 ── */
/* battle_allocateActorIndex_802FE48: allocate one slot in the allocation bitmap at dword_2009A2C.
 * Scans bits 31..16 from the top down, sets the first clear bit and returns
 * its index in r0. Returns 0xFF if all 16 slots are already taken.
 * (r1 = bitmap word, r2 = mask of the claimed/failed bit, r3 = bitmap address,
 * as the original leaves them.) */
Next battle_allocateActorIndex_802FE48(void) {
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

/* ── camera_init_802FF4C ── */
/* Saved high registers that the original pushes on entry and pops back before
 * returning, so the caller sees them unchanged. */
typedef struct {
    u32 r5, r6, r7, r8, r9, r12;
} CameraSavedRegisters;

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word_camera_init_802FF4C(u32 mem, u32 num_bytes) {
    R0 = mem;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: camera_doShakeEffect_80301e8 reads the camera pointer from r5 and
 * pass-through values from r6/r7; it has no typed C interface yet */
static void camera_do_shake_effect(u32 camera, u32 saved_r9, u32 saved_r12) {
    R5 = camera;
    R6 = saved_r9;
    R7 = saved_r12;
    GUEST_CALL(camera_doShakeEffect_80301e8);
}

/* register form: calls camera_dispatchUpdate_8030054 outside this file, which has no typed C interface yet */
static void camera_dispatch_update(void) {
    GUEST_CALL(camera_dispatchUpdate_8030054);
}

/* register form: calls sub_80302D0 outside this file, which has no typed C interface yet */
static void run_camera_post_update(void) {
    GUEST_CALL(sub_80302D0);
}

/* register form: calls camera_SavePosition_803011A outside this file, which has no typed C interface yet */
static void camera_save_position(void) {
    GUEST_CALL(camera_SavePosition_803011A);
}

/* Reset the camera for a newly entered map: zero its state (keeping the mode
 * byte and one live field), derive its movement limits from the map size,
 * move it to the player, pick the shake setting for the map group, and run
 * the camera update chain. Returns the saved high registers unchanged, since
 * the original pops them back before returning. */
static CameraSavedRegisters camera_init(u32 x, u32 y, u32 z, u32 map_group, u32 map_number,
                                        u32 toolkit, CameraSavedRegisters saved)
{
    u32 camera = LD32(toolkit + oToolkit_CameraPtr);

    /* Clear the camera block, preserving Unk_03 and Unk_14 across the fill. */
    u32 mode = LD8(camera + 3);
    u32 unk14 = LD32(camera + oCamera_PositionSrcPtr);
    zero_fill_by_word_camera_init_802FF4C(camera, 0x4C);
    ST8(camera + 3, mode);
    ST32(camera + oCamera_PositionSrcPtr, unk14);

    /* Movement limits from the map dimensions, in 1/4-tile fixed point (<< 18):
     * Unk_1c/Unk_20 hold the maxima, Unk_18/Unk_24 the negated minima. */
    u32 map_tiles = A_eMapTilesState200be70;
    u32 x_limit = (LD8(map_tiles + oMapTilesState200be70_MapWidth) - 0x1E) << 18;
    ST32(camera + oCamera_MaxScrollX, x_limit);
    ST32(camera + oCamera_MinX16_16, 0 - x_limit);
    u32 y_limit = (LD8(map_tiles + oMapTilesState200be70_MapHeight) - 0x14) << 18;
    ST32(camera + oCamera_CameraMinZ, y_limit);
    ST32(camera + oCamera_MinY, 0 - y_limit);

    /* Place the camera at the player's position and clear the scroll state. */
    ST32(camera + oCamera_X, x);
    ST32(camera + oCamera_Y, y);
    ST32(camera + oCamera_Z, z);
    ST16(camera + 4, 0);
    ST16(camera + 6, 0);

    /* Shake strength for this map: fixed values for groups >= 0xD0, otherwise
     * a per-group table of byte arrays indexed by the map number. */
    u32 shake;
    if ((s32)map_group >= 0xF0) {
        shake = 8;
    } else if ((s32)map_group >= 0xE0) {
        shake = 0xC;
    } else if ((s32)map_group >= 0xD0) {
        shake = 0x10;
    } else {
        u32 table;
        u32 index;
        if ((s32)map_group >= 0x80) {
            table = A_off_803378C;
            index = map_group - 0x80;
        } else {
            table = A_off_8033770;
            index = map_group;
        }
        shake = LD32(LD32(table + index * 4) + map_number);
    }
    ST8(camera + 2, shake);

    camera_do_shake_effect(camera, saved.r9, saved.r12);
    camera_dispatch_update();
    run_camera_post_update();
    camera_save_position();
    return saved;
}

/* camera_init_802FF4C(r0 = player_x, r1 = player_y, r2 = player_z,
 *                     r3 = map_group, r4 = map_number, r10 = toolkit) -> ()
 * Leaves c/v and r1/r3/r4 as camera_SavePosition_803011A left them, and
 * restores r5-r9/r12 exactly as the original's epilogue does. */
Next camera_init_802FF4C(void) {
    CameraSavedRegisters saved = camera_init(R0, R1, R2, R3, R4, R10,
        (CameraSavedRegisters){ R5, R6, R7, R8, R9, R12 });
    R5 = saved.r5; R6 = saved.r6; R7 = saved.r7;
    R8 = saved.r8; R9 = saved.r9; R12 = saved.r12;
    RETURN_TO_CALLER();
}

/* ── camera_dispatchUpdate_8030054 ── */
/* camera_dispatchUpdate_8030054: camera mode dispatcher.
 *
 * r5 points at the camera object. The byte at offset 2 selects which
 * handler to run; it acts as a byte offset into the handler table at
 * off_8030068, whose entries are:
 *   0x00, 0x04, 0x10 -> camera_updateDerivedOffsets_803007C (default/idle camera update)
 *   0x08             -> battle_clampCameraToBounds_80300AC
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
        GUEST_CALL(battle_clampCameraToBounds_80300AC);
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
    R1 = LD16(R5 + 4);              /* oCamera_ScrollX: current x */
    ST16(R5 + 8, R1);               /* oCamera_SavedScrollX: saved x */
    R1 = LD16(R5 + 6);              /* oCamera_CameraYInt: current y */
    ST16(R5 + oCamera_SavedCameraYInt, R1);  /* oCamera_SavedCameraYInt: saved y */
    RETURN_TO_CALLER();
}

/* ── nullsub_7 ── */
/* nullsub_7: a no-op stub (called for features not present in this build). */
Next nullsub_7(void) {
    RETURN_TO_CALLER();
}

/* ── camera_LoadPositionFromSource_8030126 ── */
/* camera_LoadPositionFromSource_8030126: copy the camera position from the data block referenced by
 * oCamera_PositionSrcPtr into the camera's X/Y/Z fields.  Leaves r7 pointing at that
 * block and r2 holding the Z value (both are read by callers). */
Next camera_LoadPositionFromSource_8030126(void) {
    R7 = LD32(R5 + oCamera_PositionSrcPtr);

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
 * camera_shouldApplyBgScrollToBothLayerPairs_80269E2 -- passing it the scroll deltas on the stack -- whether the
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
     * oCamera_ScrollX/06 = current X/Y, oCamera_SavedScrollX/oCamera_SavedCameraYInt = previous X/Y */
    u32 scrollX = LD16(R5 + 4) - LD16(R5 + 8);
    u32 scrollY = LD16(R5 + 6) - LD16(R5 + oCamera_SavedCameraYInt);

    R1 = scrollX;
    R4 = scrollY;
    R2 = LD16(R5 + oCamera_SavedCameraYInt);

    /* oToolkit_RenderInfoPtr lives at offset 8 in the toolkit struct */
    u32 renderInfo = LD32(R10 + 8);

    R6 = LD16(renderInfo + oRenderInfo_BG1ScrollXAlt) + scrollX;
    R7 = LD16(renderInfo + oRenderInfo_BGScrollY2) + scrollY;
    ST16(renderInfo + oRenderInfo_BG1ScrollXAlt, R6);
    ST16(renderInfo + oRenderInfo_BGScrollY2, R7);

    /* camera_shouldApplyBgScrollToBothLayerPairs_80269E2(r0 = render info, [sp] = scrollX, [sp+4] = scrollY) */
    R0 = renderInfo;
    SP -= 8;
    ST32(SP, scrollX);
    ST32(SP + 4, scrollY);
    GUEST_CALL(camera_shouldApplyBgScrollToBothLayerPairs_80269E2);
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
    R6 = LD16(renderInfo + oRenderInfo_BG2ScrollX) + scrollX;
    u32 baseY = LD16(renderInfo + oRenderInfo_BG3ScrollY);
    R7 = baseY + scrollY;
    ST16(renderInfo + oRenderInfo_BG2ScrollX, R6);
    ST16(renderInfo + oRenderInfo_BG3ScrollY, R7);
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
    ST32(camera + oCamera_PositionSrcPtr, R1);

    R2 = camera;
    RETURN_TO_CALLER();
}

/* ── SetCameraXYZ ── */
/* SetCameraXYZ(r0 = x, r1 = y, r2 = z, r10 = toolkit):
 * write the camera position to the camera object held by the toolkit. */
Next SetCameraXYZ(void) {
    u32 camera = LD32(R10 + oToolkit_CameraPtr);
    ST32(camera + 0x30, R0);
    ST32(camera + 0x34, R1);
    ST32(camera + 0x38, R2);
    R3 = camera;
    RETURN_TO_CALLER();
}

/* ── camera_initShakeEffect_80302a8 ── */
/* camera_initShakeEffect_80302a8:
 *   Triggers a camera shake effect.
 *   r0 = shake magnitude (stored at oCamera_ShakeMagnitude)
 *   r1 = shake duration/period (stored at oCamera_ShakeDuration)
 *   Output: r2 holds the camera struct pointer (loaded from r10).
 */
Next camera_initShakeEffect_80302a8(void) {
    u32 camera = LD32(R10 + oToolkit_CameraPtr);
    ST16(camera + oCamera_ShakeMagnitude, R0);
    ST16(camera + oCamera_ShakeDuration, R1);
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
    u32 camX = LD16(camera + 4) & 0xFF;  /* oCamera_ScrollX */
    u32 camY = LD16(camera + 6) & 0xFF;  /* oCamera_CameraYInt */

    u32 render = LD32(toolkit + 8); /* oToolkit_RenderInfoPtr */

    ST16(render + oRenderInfo_BGScrollXOffset, camX);
    ST16(render + oRenderInfo_BG1ScrollXAlt, camX);
    ST16(render + oRenderInfo_BG2ScrollX, camX);
    ST16(render + oRenderInfo_BGScrollY, camY);
    ST16(render + oRenderInfo_BGScrollY2, camY);
    ST16(render + oRenderInfo_BG3ScrollY, camY);

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

    u32 camX = LD16(camera + 4) & 0xFF; /* oCamera_ScrollX */
    ST16(renderInfo + oRenderInfo_BGScrollXOffset, camX);
    ST16(renderInfo + oRenderInfo_BG1ScrollXAlt, camX);

    u32 camY = LD16(camera + 6) & 0xFF; /* oCamera_CameraYInt */
    ST16(renderInfo + oRenderInfo_BGScrollY, camY);
    ST16(renderInfo + oRenderInfo_BGScrollY2, camY);

    R1 = 0;
    ST16(renderInfo + oRenderInfo_BG2ScrollX, 0);
    ST16(renderInfo + oRenderInfo_BG3ScrollY, 0);

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

    u32 camX = LD16(camera + 4) & 0xFF; /* oCamera_ScrollX */
    u32 camY = LD16(camera + 6) & 0xFF; /* oCamera_CameraYInt */

    /* store the masked camera scroll, then zero it all out again */
    ST16(renderInfo + oRenderInfo_BGScrollXOffset, camX);
    ST16(renderInfo + oRenderInfo_BG1ScrollXAlt, camX);
    ST16(renderInfo + oRenderInfo_BG2ScrollX, camX);
    ST16(renderInfo + oRenderInfo_BGScrollY, camY);
    ST16(renderInfo + oRenderInfo_BGScrollY2, camY);
    ST16(renderInfo + oRenderInfo_BG3ScrollY, camY);

    R1 = 0;
    ST16(renderInfo + oRenderInfo_BGScrollXOffset, 0);
    ST16(renderInfo + oRenderInfo_BGScrollY, 0);
    ST16(renderInfo + oRenderInfo_BG2ScrollX, 0);
    ST16(renderInfo + oRenderInfo_BG3ScrollY, 0);

    R2 = camY;
    R3 = renderInfo;
    R4 = 0xFF;
    R5 = camera;
    R7 = toolkit;
    SET_FLAGS_NZ(0);

    RETURN_TO_CALLER();
}

/* ── initMapTilesState_803037c ── */
/* Tables used to look up a map's data, split by world:
 *  - descriptor tables: [map_group][map_number] -> MapBGDescriptor
 *  - callback tables:   [map_group][map_number] -> 3 function pointers   */
#define REAL_WORLD_CALLBACK_TABLE off_8032F6C /* [*const [[*const Fn; 3]]; REAL_WORLD_NUM_GROUPS] */
#define INTERNET_CALLBACK_TABLE   off_8032F88 /* Nullable<[*const [[*const Fn; 3]]>; INTERNET_NUM_GROUPS] */

/* Per-map callbacks stored in the map tiles state (three function pointers). */
#define MAP_CALLBACK_ENTRY_SIZE 12

/* Set up eMapTilesState200be70 for the given map: resolve the map's BG
 * descriptor (from the real-world or internet table, depending on the map
 * group) and its three per-map callbacks, then record the tilemap's width and
 * height, the palette and tileset pointers, and the address of the buffer the
 * tilemap will be decompressed into. */
static void init_map_tiles_state(u32 map_group, u32 map_number) {
    u32 state = A_eMapTilesState200be70;
    u32 descriptor_table;
    u32 callback_table;

    if ((s32)map_group < INTERNET_MAP_GROUP_START) {
        /* Real-world maps: both tables are always present. */
        descriptor_table = A_off_80329A8;
        callback_table = A_off_8032F6C;
    } else {
        /* Internet maps use their own table set, indexed from 0. */
        descriptor_table = A_off_80329C4;
        callback_table = A_off_8032F88;
        map_group -= INTERNET_MAP_GROUP_START;
    }

    u32 descriptor = LD32(descriptor_table + map_group * 4)
                   + map_number * oMapBGDescriptor_Size;
    ST32(state + oMapTilesState200be70_MapBGDescriptorPtr, descriptor);

    u32 callbacks = LD32(callback_table + map_group * 4)
                  + map_number * MAP_CALLBACK_ENTRY_SIZE;
    ST32(state + oMapTilesState200be70_UnkCallback_18, LD32(callbacks));
    ST32(state + oMapTilesState200be70_UnkCallback_1c, LD32(callbacks + 4));
    ST32(state + oMapTilesState200be70_UnkCallback_20, LD32(callbacks + 8));

    u32 tilemap = LD32(descriptor + oMapBGDescriptor_TilemapPtr);
    u32 decomp_buffer = A_eDecompBuffer2013A00;

    ST8(state + oMapTilesState200be70_MapWidth,
        LD8(tilemap + oMapBGTilemapHeader_Width));
    ST8(state + oMapTilesState200be70_MapHeight,
        LD8(tilemap + oMapBGTilemapHeader_Height));
    ST32(state + oMapTilesState200be70_PalettePtr,
         LD32(descriptor + oMapBGDescriptor_PalettePtr));
    ST32(state + oMapTilesState200be70_TilesetPtr,
         LD32(descriptor + oMapBGDescriptor_TilesetPtr));
    ST32(state + oMapTilesState200be70_UnkCallback0C, decomp_buffer);
}

/* initMapTilesState_803037c(r0 = map_group, r1 = map_number) -> ()
 * r4-r9/r12 are saved and restored by the original, so they end unchanged. */
Next initMapTilesState_803037c(void) {
    init_map_tiles_state(R0, R1);
    RETURN_TO_CALLER();
}

/* ── mapTiles_ResetDimensionsToDefault_80303E8 ── */
/* mapTiles_ResetDimensionsToDefault_80303E8: reset the map tile grid dimensions to the default 32x32. */
Next mapTiles_ResetDimensionsToDefault_80303E8(void) {
    ST8(A_eMapTilesState200be70, 0x20);
    ST8(A_eMapTilesState200be70 + 1, 0x20); /* MapHeight (byte_200BE71) */

    RETURN_TO_CALLER();
}

/* ── decompAndCopyMapTiles_8030472 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3; } call_CopyByEightWords_result;
/* register form: generated wrapper: CopyByEightWords has no typed C interface yet */
static inline call_CopyByEightWords_result call_CopyByEightWords(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyByEightWords);
    return (call_CopyByEightWords_result){ R2, R3 };
}

typedef struct { u32 r1, r2, r3; } call_SWI_LZ77UnCompReadNormalWrite8bit_result;
/* register form: generated wrapper: SWI_LZ77UnCompReadNormalWrite8bit has no typed C interface yet */
static inline call_SWI_LZ77UnCompReadNormalWrite8bit_result call_SWI_LZ77UnCompReadNormalWrite8bit(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);
    return (call_SWI_LZ77UnCompReadNormalWrite8bit_result){ R1, R2, R3 };
}

typedef struct { u32 r2, r3; } call_CopyWords_result;
/* register form: generated wrapper: CopyWords has no typed C interface yet */
static inline call_CopyWords_result call_CopyWords(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyWords);
    return (call_CopyWords_result){ R2, R3 };
}

/* Decompresses and installs the current overworld map's graphics:
 *   1. copies the BG palette (0x1A0 bytes past its 4-byte header) to work RAM,
 *   2. LZ77-decompresses both BG tilesets into the scratch buffer and copies
 *      them to their VRAM offsets,
 *   3. copies the BG tilemap header, then decompresses the tile entries
 *      behind it into the same buffer,
 * and finally dispatches the map state's post-load callback. */

#define BG_VRAM_BASE      0x6000000u
#define BG_TILESET_COUNT  2
#define PALETTE_BYTES     (13u << 5)   /* 0x1A0 */

/* register form: the callback is dispatched with `mov lr, pc; bx r0` and takes
 * its arguments from registers plain C cannot reach: the map state pointer in
 * r5, the tileset list base in r6, its end in r7, and the leftovers of the
 * last LZ77 SWI in r1-r3 (the original even passes the callback its own
 * address in r0). Right after it returns, the original pops r4-r7 and
 * r8/r9/r12; restoring guest registers around an opaque call cannot be
 * expressed in typed C, so the helper saves and restores them itself. */
static void run_map_load_callback(u32 callback, u32 state, u32 tileset_base,
                                  u32 tileset_end,
                                  u32 swi_r1, u32 swi_r2, u32 swi_r3)
{
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    u32 saved_r8 = R8, saved_r9 = R9, saved_r12 = R12;

    R0 = callback;      /* the original branches to r0 holding its own address */
    R1 = swi_r1;
    R2 = swi_r2;
    R3 = swi_r3;
    R5 = state;
    R6 = tileset_base;
    R7 = tileset_end;
    GUEST_CALL_ADDR(callback);

    /* the original's pop {r4-r7} / pop {r1-r3} into r8/r9/r12 */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    R8 = saved_r8;
    R9 = saved_r9;
    R12 = saved_r12;
}

static void decomp_and_copy_map_tiles(void)
{
    u32 state = A_eMapTilesState200be70;

    /* BG palette: skip the 4-byte header, copy 0x1A0 bytes to work RAM. */
    u32 palette_src = LD32(state + oMapTilesState200be70_PalettePtr) + 4;
    call_CopyByEightWords(palette_src, A_palette_3001960, PALETTE_BYTES);

    /* Two BG tilesets (BG1, BG2): LZ77-decompress each into the scratch
     * buffer, then copy the uncompressed data to its VRAM offset. */
    u32 tileset_base = LD32(state + oMapTilesState200be70_TilesetPtr);
    u32 tileset = tileset_base;
    for (u32 i = 0; i < BG_TILESET_COUNT; i++) {
        u32 comp_src = LD32(tileset + oMapBGTilesetHeader_CompressedDataOffset)
                     + tileset_base;
        call_SWI_LZ77UnCompReadNormalWrite8bit(comp_src, A_eDecompBuffer2013A00);

        u32 vram_dest = LD32(tileset + oMapBGTilesetHeader_VRAMOffset)
                      + BG_VRAM_BASE;
        u32 copy_bytes = LD32(tileset + oMapBGTilesetHeader_WordCount) << 2;
        call_CopyByEightWords(A_eDecompBuffer2013A00, vram_dest, copy_bytes);

        tileset += oMapBGTilesetHeader_Size;
    }

    /* BG tilemap: copy the plain header with CopyWords, then LZ77-decompress
     * the tile entries following it into the buffer behind that header. */
    u32 bg_desc = LD32(state + oMapTilesState200be70_MapBGDescriptorPtr);
    u32 tilemap = LD32(bg_desc + oMapBGDescriptor_TilemapPtr);
    call_CopyWords(tilemap, A_eDecompBuffer2013A00, oMapBGTilemapHeader_Size);
    call_SWI_LZ77UnCompReadNormalWrite8bit_result decomp =
        call_SWI_LZ77UnCompReadNormalWrite8bit(
            tilemap + oMapBGTilemapHeader_Size,
            A_eDecompBuffer2013A00 + oMapBGTilemapHeader_Size);

    /* Hand control to the map's post-load callback. */
    run_map_load_callback(LD32(state + oMapTilesState200be70_UnkCallback_1c),
                          state, tileset_base, tileset,
                          decomp.r1, decomp.r2, decomp.r3);
}

Next decompAndCopyMapTiles_8030472(void) {
    decomp_and_copy_map_tiles();
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

/* ── decompressCoordEventData_8030aa4 ── */
/*
 * Coordinate-event record layout (per map):
 *   u32 section_offset[4];   -- offsets into the decompression buffer, filled
 *                               in below once the payload is unpacked
 *   u8  lz77_data[];         -- compressed payload, at record + 0x10
 * The four sections are handed to the map-script / object stream handlers
 * after decompression into the shared scratch buffer DecompBuf_2027A00.
 */

/* register form: calls SWI_LZ77UnCompReadNormalWrite8bit outside this file, which has no typed C interface yet */
static void lz77_uncompress(u32 src, u32 dest) {
    R0 = src;
    R1 = dest;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);
}

/* register form: calls sub_8030B0C outside this file, which has no typed C interface yet */
static void call_sub_8030B0C(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_8030B0C);
}

/* register form: calls sub_8031600 outside this file, which has no typed C interface yet */
static void call_sub_8031600(u32 arg) {
    R0 = arg;
    GUEST_CALL(sub_8031600);
}

/* register form: calls chatbox_loadScriptWordAndResetScroll_803189C outside this file, which has no typed C interface yet */
static void call_chatbox_loadScriptWordAndResetScroll_803189C(u32 arg) {
    R0 = arg;
    GUEST_CALL(chatbox_loadScriptWordAndResetScroll_803189C);
}

/* register form: calls overworldObject55_ReadStreamEntry_8031A68 outside this file, which has no typed C interface yet */
static void call_overworldObject55_ReadStreamEntry_8031A68(u32 arg) {
    R0 = arg;
    GUEST_CALL(overworldObject55_ReadStreamEntry_8031A68);
}

/*
 * Decompress the coordinate-event data for (map_group, map_number) into the
 * shared LZ77 scratch buffer and feed the four resulting sections (object
 * table, ..., chatbox script, object stream) to their consumers.
 *
 * in_r4..in_r12 are the entry values of the callee-saved registers: the
 * original pushes/pops them around the guest calls, and the guest callees
 * really do trash some of them, so they must be written back on exit.
 */
static void decompress_coord_event(s32 map_group, u32 map_number,
                                   u32 in_r4, u32 in_r5, u32 in_r6, u32 in_r7,
                                   u32 in_r8, u32 in_r9, u32 in_r12) {
    u32 table;
    u32 group;
    u32 map_entries;
    u32 record;
    u32 buf;

    /* Internet maps live in a separate table, indexed from the first
     * internet group; everything below that is a real-world group. */
    if (map_group >= (s32)INTERNET_MAP_GROUP_START) {
        table = A_pt_803354C;
        group = (u32)map_group - INTERNET_MAP_GROUP_START;
    } else {
        table = A_pt_8033530;
        group = (u32)map_group;
    }

    /* table[group] -> per-map array of record pointers, then [map_number]. */
    map_entries = LD32(table + group * 4);
    record = LD32(map_entries + map_number * 4);

    buf = A_DecompBuf_2027A00;

    /* Compressed payload sits after the four section offsets. */
    lz77_uncompress(record + 0x10, buf);

    call_sub_8030B0C(LD32(record + 0x00) + buf);
    call_sub_8031600(LD32(record + 0x04) + buf);
    call_chatbox_loadScriptWordAndResetScroll_803189C(LD32(record + 0x08) + buf);
    call_overworldObject55_ReadStreamEntry_8031A68(LD32(record + 0xC) + buf);

    /* register form: the guest callees leave clobbered values in r5/r12 (and
     * may clobber the other callee-saved registers too); the original caller
     * preserves r4-r9/r12 with its push/pop, so the entry values must be
     * written back into the guest registers before returning. */
    R4 = in_r4;
    R5 = in_r5;
    R6 = in_r6;
    R7 = in_r7;
    R8 = in_r8;
    R9 = in_r9;
    R12 = in_r12;
}

/*
 * decompressCoordEventData_8030aa4(r0 = map_group, r1 = map_number) -> ()
 * The original restores r4-r9/r12 from its push/pop, so the typed function
 * receives their entry values and hands them back unchanged.
 */
Next decompressCoordEventData_8030aa4(void) {
    decompress_coord_event((s32)R0, R1, R4, R5, R6, R7, R8, R9, R12);
    RETURN_TO_CALLER();
}

/* ── owObject_lineDraw_8030CAC ── */
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

/* owObject_lineDraw_8030CAC(r0 = OWObjectCoords*, r5 = draw context) -> r0, r3, r5, r7, r8 */
Next owObject_lineDraw_8030CAC(void) {
    R3 = ow_line_draw(R0, R5);
    RETURN_TO_CALLER();
}

/* ── sub_8030DA2 ── */
/* Trail recorder state at dword_2011D10 (disassembly symbols:
 * word_2011D16, byte_2011D18, byte_2011D19, byte_2011D1A, dword_2011D24). */
#define oTrail_LastTile   0x06 /* u16: flattened index of the last tile recorded */
#define oTrail_Pitch      0x08 /* u8: tiles per row used to flatten a position (low bit ignored) */
#define oTrail_RowBias    0x09 /* u8: rows the grid origin is shifted down (halved when used) */
#define oTrail_Count      0x0A /* u8: number of entries recorded so far */
#define oTrail_BytesUsed  0x14 /* u32: bytes of entry data recorded so far */
#define oTrail_Entries    0x18 /* the entries themselves, TRAIL_ENTRY_SIZE bytes each */
#define oTrailEntry_Tile  0x00 /* u16: flattened tile index */
#define oTrailEntry_X     0x04 /* u32: pixel x, aligned down to a multiple of 8 */
#define oTrailEntry_Y     0x08 /* u32: pixel y, aligned down to a multiple of 8 */

#define TRAIL_MAX_ENTRIES 16
#define TRAIL_ENTRY_SIZE  0x10

/* Record the tile containing pixel position (x, y) in the trail recorder at
 * dword_2011D10.  A run of positions over the same tile is recorded only once,
 * and once 16 entries are held further tiles are ignored. */
static void trail_add_tile(s32 x, s32 y) {
    u32 trail = A_dword_2011D10;

    /* Flatten the position into a single tile index: each axis is converted
     * from pixels (8 px per tile) and biased (x by half the pitch, y by half
     * the row bias); the pitch's low bit is dropped for the row stride. */
    u32 pitch = LD8(trail + oTrail_Pitch);
    u32 column = (u32)(x >> 3) + (pitch >> 1);
    u32 row = (u32)(y >> 3) + (LD8(trail + oTrail_RowBias) >> 1);
    u32 tile = row * (pitch & ~1u) + column;

    u32 count = LD8(trail + oTrail_Count);
    if (LD16(trail + oTrail_LastTile) == tile || count + 1 >= TRAIL_MAX_ENTRIES)
        return; /* same tile as the previous entry, or the recorder is full */

    ST16(trail + oTrail_LastTile, tile);
    ST8(trail + oTrail_Count, count + 1);
    ST32(trail + oTrail_BytesUsed, LD32(trail + oTrail_BytesUsed) + TRAIL_ENTRY_SIZE);

    /* Fill the next free entry, indexed by the count from before the increment. */
    u32 entry = trail + oTrail_Entries + TRAIL_ENTRY_SIZE * count;
    ST16(entry + oTrailEntry_Tile, tile);
    ST32(entry + oTrailEntry_X, (u32)x & ~7u);
    ST32(entry + oTrailEntry_Y, (u32)y & ~7u);
}

/* sub_8030DA2(r4 = pixel x, r7 = pixel y) -> r0, r4, r5, r7 unchanged.
 * The original only writes the trail recorder and saves/restores those
 * registers, so the adapter simply leaves them alone. */
Next sub_8030DA2(void) {
    trail_add_tile((s32)R4, (s32)R7);
    RETURN_TO_CALLER();
}

/* ── sub_8030E14 ── */
/* sub_8030E14 — hit-test a segment against a point table and solve the crossing.
 *
 * The segment endpoints arrive split across the argument registers:
 * (x1,y1) = (r2,r4), (x2,y2) = (r3,r5).  `matrix` (r0) points at reference
 * point 0, an (x,y) pair at +0/+4, with reference point 1 at +0xC/+0x10.
 * mode (r6) == 1 makes the test one-sided (reference point 1 is not checked).
 * A hit needs reference point 0 strictly on the negative side of the
 * segment's (extended) line and, unless mode is 1, reference point 1 not on
 * that side.  On a hit each axis of the crossing point is solved by dividing
 * the line equation at reference point 0 by the other axis's delta
 * (battleObject_collisionProjectRay_8031048) and un-biasing by reference point 0's coordinates; a miss
 * answers (0,0).  The remaining output registers carry the working values
 * the original leaves behind on each path. */

typedef struct {
    u32 quotient;  /* battleObject_collisionProjectRay_8031048's result, left in r7 */
    u32 remainder; /* what it leaves in r6 (becomes this function's r6 output) */
} DivideResult;

typedef struct {
    u32 x, y;                          /* r0/r1: crossing offset from reference point 0 */
    u32 reg4, reg6, reg7, reg8, reg9;  /* the other listed output registers */
} SegmentHit;

/* register form: calls battleObject_collisionProjectRay_8031048 outside this file, which has no typed C interface yet */
static DivideResult axis_divide(u32 matrix, u32 cross, u32 side, u32 dividend, u32 divisor)
{
    DivideResult out;
    R0 = matrix;    /* point table the result is taken against */
    R2 = cross;     /* line constant */
    R3 = dividend;  /* numerator, mirrored into r6 as at the call site */
    R4 = divisor;   /* the original's call site duplicates the divisor into r4 */
    R5 = side;      /* sign value still live in r5 at the call site */
    R6 = dividend;
    R7 = divisor;
    GUEST_CALL(battleObject_collisionProjectRay_8031048);
    out.quotient  = R7;
    out.remainder = R6;
    return out;
}

/* Sign of the segment's (extended) line at (px,py): negative on one side,
 * positive on the other, zero on the line. */
static s32 line_side(u32 cross, u32 dy, u32 neg_dx, u32 px, u32 py)
{
    return (s32)(cross + px * dy + py * neg_dx);
}

/* Miss: answer (0,0); r4 keeps the last py*neg_dx product, r6/r7 the segment
 * deltas, r8 the mode stashed on entry, r9 the caller's value untouched. */
static SegmentHit miss(u32 last_product, u32 dy, u32 neg_dx, u32 mode, u32 r9_in)
{
    SegmentHit hit;
    hit.x = 0;
    hit.y = 0;
    hit.reg4 = last_product;
    hit.reg6 = dy;
    hit.reg7 = neg_dx;
    hit.reg8 = mode;
    hit.reg9 = r9_in;
    return hit;
}

static SegmentHit segment_line_hit(u32 matrix, u32 x1, u32 y1, u32 x2, u32 y2,
                                   u32 mode, u32 r9_in)
{
    u32 dy     = y2 - y1;            /* segment y delta */
    u32 neg_dx = x1 - x2;            /* negated segment x delta */
    u32 cross  = x2 * y1 - x1 * y2;  /* constant of the segment's line */

    u32 ref0_x = LD32(matrix);       /* reference point 0 */
    u32 ref0_y = LD32(matrix + 4);

    /* Reference point 0 must lie strictly on the negative side of the line. */
    s32 side0 = line_side(cross, dy, neg_dx, ref0_x, ref0_y);
    if (side0 >= 0)
        return miss(ref0_y * neg_dx, dy, neg_dx, mode, r9_in);

    /* Unless mode 1 makes the test one-sided, reference point 1 must not be
     * on that side too.  The sign value sitting in r5 at the solve calls is
     * the one from whichever test actually ran. */
    u32 live_side = (u32)side0;
    if (mode != 1) {
        u32 ref1_x = LD32(matrix + 0xC);
        u32 ref1_y = LD32(matrix + 0x10);
        s32 side1 = line_side(cross, dy, neg_dx, ref1_x, ref1_y);
        if (side1 < 0)
            return miss(ref1_y * neg_dx, dy, neg_dx, mode, r9_in);
        live_side = (u32)side1;
    }

    /* Hit: solve one axis at a time, dividing out the other axis's delta.
     * The original re-reads reference point 0 after each divide, so mirror
     * those load points. */
    DivideResult y_axis = axis_divide(matrix, cross, live_side,
                                      0u - (cross + ref0_x * dy), neg_dx);
    ref0_y = LD32(matrix + 4);
    u32 y = y_axis.quotient - ref0_y;    /* r8, returned in r1 */

    DivideResult x_axis = axis_divide(matrix, cross, live_side,
                                      0u - (cross + ref0_y * neg_dx), dy);
    ref0_x = LD32(matrix);
    u32 x = x_axis.quotient - ref0_x;    /* r9, returned in r0 */

    SegmentHit hit;
    hit.x = x;
    hit.y = y;
    hit.reg4 = x;                  /* r4 mirrors the x result */
    hit.reg6 = x_axis.remainder;   /* r6/r7 keep the last divide's leftovers */
    hit.reg7 = x_axis.quotient;
    hit.reg8 = y;                  /* r8 mirrors the y result */
    hit.reg9 = x;
    return hit;
}

/* sub_8030E14(r0 = matrix, r2/r4 = (x1,y1), r3/r5 = (x2,y2), r6 = mode,
 *             r9 passed through on a miss)
 *   -> r0/r1 crossing offset from reference point 0 ((0,0) on a miss),
 *      plus the r4/r6..r9 outputs */
Next sub_8030E14(void) {
    SegmentHit hit = segment_line_hit(R0, R2, R4, R3, R5, R6, R9);
    R0 = hit.x, R1 = hit.y, R4 = hit.reg4;
    R6 = hit.reg6, R7 = hit.reg7, R8 = hit.reg8, R9 = hit.reg9;
    RETURN_TO_CALLER();
}

/* ── battle_scrollHitTestLineVsWedge_8030F7C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r6, r7; } call_sub_8031048_result;
/* register form: generated wrapper: battleObject_collisionProjectRay_8031048 has no typed C interface yet */
static inline call_sub_8031048_result call_sub_8031048(u32 r2, u32 r3, u32 r6, u32 r7) {
    R2 = r2;
    R3 = r3;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(battleObject_collisionProjectRay_8031048);
    return (call_sub_8031048_result){ R2, R6, R7 };
}

/* battle_scrollHitTestLineVsWedge_8030F7C: Segment-edge intersection test.
 * obj points to a structure holding two edge endpoints:
 *   edge 0 = (LD32(obj), LD32(obj+4)),  edge 1 = (LD32(obj+0xC), LD32(obj+0x10)).
 * A segment is defined by two points (xA,yA)→(xB,yB).
 * Compute signed distance from each endpoint to the separating edge via
 * battleObject_collisionProjectRay_8031048.  On failure (no edge separates) return 0 in r0/r1.
 * mode == 1 tests only edge 0; otherwise both edges must have the right sign. */
typedef struct {
    u32 r0, r1, r4, r6, r7, r8, r9;
} seg_result;

static seg_result seg_intersect(u32 obj, u32 xA, u32 yA, u32 xB, u32 yB,
                                u32 mode, u32 r9_in)
{
    /* Direction vector and 2-D cross product of the segment */
    u32 dy    = yB - xB;                       /* sub r6, r5, r4 */
    u32 dx    = xA - yA;                       /* sub r7, r2, r3 */
    u32 cross = yA * xB - xA * yB;            /* mul/mul/sub r2 */

    /* Dot product of edge-0 normal with the test point (signed compare) */
    u32 ax = LD32(obj);
    u32 ay = LD32(obj + 4);
    s32 dot0 = (s32)(cross + ax * dy + ay * dx);

    if (dot0 <= 0) {
        /* First edge check fails: r4 retains ay*dx from the mul */
        seg_result fail = {0, 0, ay * dx, dy, dx, mode, r9_in};
        return fail;
    }

    /* If mode != 1, verify edge 1 has opposite sign */
    if (mode != 1) {
        u32 bx = LD32(obj + 0xC);
        u32 by = LD32(obj + 0x10);
        s32 dot1 = (s32)(cross + bx * dy + by * dx);
        if (dot1 > 0) {
            /* Second edge check fails: r4 retains by*dx */
            seg_result fail = {0, 0, by * dx, dy, dx, mode, r9_in};
            return fail;
        }
    }

    /* Success: intersect with edge 0, then edge 1, via battleObject_collisionProjectRay_8031048 */
    u32 s1 = -(ax * dy + cross);
    call_sub_8031048_result res1 = call_sub_8031048(cross, s1, s1, dx);
    u32 r8 = res1.r7 - LD32(obj + 4);

    u32 s2 = -(LD32(obj + 4) * dx + res1.r2);
    call_sub_8031048_result res2 = call_sub_8031048(res1.r2, s2, s2, dy);
    u32 r9 = res2.r7 - LD32(obj);

    seg_result succ = {r9, r8, r9, res2.r6, res2.r7, r8, r9};
    return succ;
}

Next battle_scrollHitTestLineVsWedge_8030F7C(void) {
    seg_result r = seg_intersect(R0, R2, R3, R4, R5, R6, R9);
    R0 = r.r0, R1 = r.r1, R4 = r.r4, R6 = r.r6, R7 = r.r7, R8 = r.r8, R9 = r.r9;
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

/* ── sub_80311B6 ── */
/* sub_80311B6(r0 = object with position fields):
 * Compute how far the object moved since its last recorded position
 * ([+0]-[+0xC] horizontally, [+4]-[+0x10] vertically). If it moved on both
 * axes at once, the delta is discarded (snapped to zero); otherwise it is
 * kept. The delta is written to the camera/scroll state at dword_200F3E0
 * (+0x18/+0x1C), then sprite_worldToScreenPos_80310EC converts the state's
 * world position into screen coordinates. Returns whatever that callee
 * returns in r0 (r6 is left pointing at the object for the callee). */
Next sub_80311B6(void) {
    u32 obj = R0;
    u32 state = A_dword_200F3E0;

    u32 dx = LD32(obj) - LD32(obj + 0xC);
    u32 dy = LD32(obj + 4) - LD32(obj + 0x10);

    if (dx != 0 && dy != 0) {
        /* diagonal movement: drop the delta entirely */
        dx = 0;
        dy = 0;
    }

    ST32(state + 0x18, dx);
    ST32(state + 0x1C, dy);

    R0 = state;
    R6 = obj;
    GUEST_CALL(sprite_worldToScreenPos_80310EC);
    RETURN_TO_CALLER();
}

/* ── sub_80311F4 ── */
/* register form: calls sub_8031122 with current R0,R1,R2; no typed C interface yet */
static void call_8031122(void) {
    GUEST_CALL(sub_8031122);
}

/* register form: calls battleObject_isHitByPanelRectangle_8031570(r2=0,r3=0x20,r4=0x20,r5=0); no typed C interface yet */
static u32 call_8031570(void) {
    R2 = 0; R4 = 0x20; R3 = 0x20; R5 = 0;
    GUEST_CALL(battleObject_isHitByPanelRectangle_8031570);
    return R0;
}

/* register form: calls sub_80311B6 with current R0,R5; no typed C interface yet */
static void call_80311B6(void) {
    GUEST_CALL(sub_80311B6);
}

/* register form: calls sub_8030E14(r2=0,r3=0x20,r4=0x20,r5=0,r6=1); no typed C interface yet */
static void call_8030E14(void) {
    R2 = 0; R4 = 0x20; R3 = 0x20; R5 = 0; R6 = 1;
    GUEST_CALL(sub_8030E14);
}

/* register form: calls camera_moveByDelta_80310B0; no typed C interface yet */
static void call_camera_moveByDelta(s32 dx, s32 dy) {
    R0 = (u32)dx; R1 = (u32)dy;
    GUEST_CALL(camera_moveByDelta_80310B0);
}

/* "neg ; lsr #1 ; neg" on a signed value, matching the original's bit pattern. */
static s32 neg_lsr1_neg(s32 v) {
    u32 a = 0u - (u32)v;
    a >>= 1;
    return (s32)(0u - a);
}

typedef struct {
    u32 r0, r1, r2, r5, r12;
} S119F4Out;

/* register form: the callees (notably sub_80311B6, and sub_8031122) write through
 * their R0 pointer into the saved {r0-r2,r5} stack frame, so we must use the real
 * guest stack to reproduce the exact memory writes and the final popped values
 * of R0/R1/R2/R5. R12 is intentionally not pushed, matching the original. */
static S119F4Out sub_80311F4_body(void) {
    /* push {lr}; push {r0-r2,r5} */
    SP -= 4;
    ST32(SP, LR);
    SP -= 16;
    ST32(SP,     R0);
    ST32(SP + 4, R1);
    ST32(SP + 8, R2);
    ST32(SP + 12, R5);

    call_8031122();

    call_8031570();
    R3 = 0;
    SET_FLAGS_CMP(R0, 0u);
    if (!ZERO_FLAG()) {
        /* pop {r0-r2,r5}, then push {r0-r2,r5} again (net SP unchanged;
         * the frame sits in the same memory so callee writes through R0
         * land in it, matching the original). */
        R0 = LD32(SP);
        R1 = LD32(SP + 4);
        R2 = LD32(SP + 8);
        R5 = LD32(SP + 12);
        SP += 16;
        SP -= 16;
        ST32(SP,     R0);
        ST32(SP + 4, R1);
        ST32(SP + 8, R2);
        ST32(SP + 12, R5);

        call_80311B6();
        call_8030E14();
        s32 dx = neg_lsr1_neg((s32)R0);
        s32 dy = neg_lsr1_neg((s32)R1);
        call_camera_moveByDelta(dx, dy);
    }

    /* loc_8031234: mov r0,r12 ; mov r1,#3 ; orr r0,r1 ; mov r12,r0
     * (ORR sets NZ from r12|3; POP right after doesn't touch flags). */
    R12 |= 3;

    /* pop {r0-r2,r5,pc} */
    R0 = LD32(SP);
    R1 = LD32(SP + 4);
    R2 = LD32(SP + 8);
    R5 = LD32(SP + 12);
    LR = LD32(SP + 16);
    SP += 20;

    return (S119F4Out){ R0, R1, R2, R5, R12 };
}

Next sub_80311F4(void) {
    S119F4Out o = sub_80311F4_body();
    R0 = o.r0;
    R1 = o.r1;
    R2 = o.r2;
    R5 = o.r5;
    R12 = o.r12;
    SET_FLAGS_NZ(o.r12);
    RETURN_TO_CALLER();
}

/* ── camera_initAndActivate_803123E ── */
/* camera_initAndActivate_803123E: initialize camera and mark it as ready/active.
 * inputs:  r0 r5 r12 lr
 * outputs: r0 r1 r2 r3 r5 r12
 * clobbers: r4 r6 r7 r8 r9 lr, nz c v
 * preserved: r0 r1 r2 r5
 *
 * Captures caller-saved regs (r0, r1, r2, r5, lr) onto the stack, lets
 * sub_8031122 set up the camera context, queries the current X/Y position
 * via camera_computeScrollDelta_8030E8C with a (0, 0x20, 0x20, 1) window, halves both coordinates
 * and feeds the delta into camera_moveByDelta_80310B0. Finally ORs 3 into
 * r12 (bit 0 = active, bit 1 = ready) and restores the original r0..r2, r5. */
Next camera_initAndActivate_803123E(void) {
    /* Snapshot r0..r2, r5, lr on the guest stack so the pop can restore them. */
    u32 saved_lr = LR;
    u32 saved_r0 = R0;
    u32 saved_r1 = R1;
    u32 saved_r2 = R2;
    u32 saved_r5 = R5;

    /* sub_8031122: prepare / init the camera context for this caller. */
    GUEST_CALL(sub_8031122);

    /* Query the current camera/scene coordinates with a small window:
     *   result_x = r0, result_y = r1, args: r2=0, r3=0x20, r5=0x20, r6=1. */
    R2 = 0;
    R4 = 0;
    R3 = 0x20;
    R5 = 0x20;
    R6 = 1;
    GUEST_CALL(camera_computeScrollDelta_8030E8C);

    /* Halve the returned X (unsigned) and Y (signed) to get the per-frame delta. */
    R0 = (u32)((u32)R0 >> 1);
    R1 = (u32)((s32)R1 >> 1);

    /* Apply that delta to the camera position. */
    GUEST_CALL(camera_moveByDelta_80310B0);

    /* Mark r12 (camera state word): set bits 0 and 1 (active + ready). */
    u32 state = R12 | 3;
    SET_FLAGS_NZ(state);
    R12 = state;

    /* Restore caller's r0..r2 and r5, then return via the saved lr. */
    R0 = saved_r0;
    R1 = saved_r1;
    R2 = saved_r2;
    R5 = saved_r5;
    LR = saved_lr;
    RETURN_TO_CALLER();
}

/* ── sub_803128E ── */
/* sub_803128E(r0 = ?, r5 = ?, r12 = ?, lr):
 *   If the player-facing pose returned by sub_8031122 lies to the left of a fixed
 *   reference line (test point (0,0) against line (0x20,0)-(0,0x20)), snap the
 *   camera to the right pose (sub_80311B6) and pan it by half the right-pose
 *   delta. Either way, OR 3 into r12 to flag the resolved camera state. */
Next sub_803128E(void) {
    /* Save the live inputs across the callees below. */
    u32 saved_r0 = R0;
    u32 saved_r1 = R1;
    u32 saved_r2 = R2;
    u32 saved_r5 = R5;
    u32 saved_r12 = R12;

    /* Ask sub_8031122 for the player pose. */
    GUEST_CALL(sub_8031122);

    /* Test whether the player is on the left side of the line through (0x20,0)-(0,0x20). */
    R2 = 0;
    R4 = 0x20;
    R3 = 0x20;
    R5 = 0;
    GUEST_CALL(math_pointLeftOfLineTest_80315DC);

    R3 = 0;
    SET_FLAGS_CMP(R0, 0);
    if (!ZERO_FLAG()) {
        /* Restore the original inputs, then ask sub_80311B6 for the right-side pose. */
        R0 = saved_r0;
        R1 = saved_r1;
        R2 = saved_r2;
        R5 = saved_r5;

        GUEST_CALL(sub_80311B6);

        /* Compute the right-pose delta from the same line. */
        R2 = 0;
        R4 = 0x20;
        R3 = 0x20;
        R5 = 0;
        R6 = 1;
        GUEST_CALL(battle_scrollHitTestLineVsWedge_8030F7C);

        /* Pan the camera by half that delta. */
        R0 = R0 >> 1;
        R1 = R1 >> 1;
        GUEST_CALL(camera_moveByDelta_80310B0);
    }

    /* Flag the resolved camera state into r12. */
    R0 = R12 | 3;
    SET_FLAGS_NZ(R0);
    R12 = R0;

    /* Restore the preserved inputs and return. */
    R0 = saved_r0;
    R1 = saved_r1;
    R2 = saved_r2;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── camera_recenterAndSetCtrlBits_803135E ── */
/* camera_recenterAndSetCtrlBits_803135E: recenter camera and set bits 0-1 of the r12 control word.
 *
 * Pushes r0..r2, r5 and lr; calls sub_80311B6 to seed a view; calls
 * battle_scrollHitTestLineVsWedge_8030F7C to compute a delta against a 64x32 box anchored at (0x20, 0x20)
 * with layer 1; halves the delta and feeds it to camera_moveByDelta_80310B0;
 * then ORs 3 into the r12 control word and tail-returns through the saved
 * lr so the caller sees the original r0/r1/r2/r5 intact. */
Next camera_recenterAndSetCtrlBits_803135E(void) {
    /* Reserve the same 20 bytes the original's push {r0-r2,r5,lr} uses,
     * so any callee that peeks at the frame sees the same layout. */
    u32 sp = SP -= 20;
    ST32(sp,      R0);
    ST32(sp + 4,  R1);
    ST32(sp + 8,  R2);
    ST32(sp + 12, R5);
    ST32(sp + 16, LR);

    /* Seed the base view via sub_80311B6 (return values ignored). */
    GUEST_CALL(sub_80311B6);

    /* Ask battle_scrollHitTestLineVsWedge_8030F7C for a delta against a 64x32 box at (0x20, 0x20),
     * layer 1. The function returns the dx/dy in r0/r1. */
    R2 = 0;
    R4 = 0x40;   /* 0x20 + 0x20 */
    R3 = 0x20;
    R5 = 0x20;   /* 0 + 0x20 */
    R6 = 1;
    GUEST_CALL(battle_scrollHitTestLineVsWedge_8030F7C);

    /* Halve the delta (logical >>1) and apply it to the camera. */
    R0 = R0 >> 1;
    R1 = R1 >> 1;
    GUEST_CALL(camera_moveByDelta_80310B0);

    /* Merge bits 0 and 1 into the r12 control word; flags come from
     * the ORR result so callers can branch on nz if they care. */
    R0 = R12 | 3;
    SET_FLAGS_NZ(R0);
    R12 = R0;

    /* pop {r0-r2,r5,pc} -- restore the snapshot and return to caller. */
    R0  = LD32(sp);
    R1  = LD32(sp + 4);
    R2  = LD32(sp + 8);
    R5  = LD32(sp + 12);
    LR  = LD32(sp + 16);
    SP = sp + 20;

    RETURN_TO_CALLER();
}

/* ── battleObject_applyFullscreenShakeAndCameraBump_8031388 ── */
/* battleObject_applyFullscreenShakeAndCameraBump_8031388(r0-r2 = parameters, r5 = parameter, r12 = state flags):
 * Runs the three-step sequence shared by this family of routines:
 *   1. battleObject_applyShakeOffsetAndProject_8031136 on the caller's r0-r2; its r0/r1 results are fed
 *      straight into step 2 (the original never reloads them),
 *   2. sub_8030E14 with the "unbounded" extent markers (r2 = r3 = -1),
 *      an extent count of 0x20, r5 = 0 and r6 = 1,
 *   3. camera_moveByDelta_80310B0 with r1 = 0 to finish.
 * Afterwards the low bit of the r12 state flags is set (state marked
 * active) and nz reflects that value; c/v are whatever the last callee
 * left. r0, r1, r2 and r5 are restored to their entry values (the
 * original's push/pop pair); r3, r4 and r6 return whatever camera_moveByDelta_80310B0
 * left in them. */
Next battleObject_applyFullscreenShakeAndCameraBump_8031388(void) {
    u32 ret_addr = LR;   /* push {lr} */
    u32 arg0 = R0, arg1 = R1, arg2 = R2, arg5 = R5;

    /* Step 1: r0-r2 are passed through unchanged. */
    GUEST_CALL(battleObject_applyShakeOffsetAndProject_8031136);

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
    GUEST_CALL(battle_scrollAccumulateStep_8031176);

    /* step 2: lay out into a 32x32 region with blank origin and one style */
    R2 = 0x20;
    R4 = 0;
    R3 = 0x20;
    R5 = 0x20;
    R6 = 1;
    GUEST_CALL(camera_computeScrollDelta_8030E8C);

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
    GUEST_CALL(battleObject_applyShakeOffsetAndProject_8031136);

    /* Second step: called with r2 = 0x20, r3 = 0, r4 = -1, r5 = -1, r6 = 1
     * (fill/clear style parameters); r12 state word still live. */
    R2 = 0x20;
    R3 = 0;
    R4 = 0 - 1;
    R5 = 0 - 1;
    R6 = 1;
    GUEST_CALL(battle_runTextboxUpdateAndSetStateBit1_8030F04);

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

/* ── battle_scrollTickStepOrStop_80313FC ── */
/* battle_scrollTickStepOrStop_80313FC: runs a three-step sequence (battle_scrollAccumulateStep_8031176; battle_scrollHitTestLineVsWedge_8030F7C with a fixed
 * parameter set of 0x20/0x20/0x20/1; camera_moveByDelta_80310B0 with 0) and then sets bit 1 of
 * the state word passed in r12.  The caller's r0, r1, r2 and r5 are preserved
 * across the calls; the Z/N flags reflect the updated r12 value. */
Next battle_scrollTickStepOrStop_80313FC(void) {
    u32 inR0 = R0, inR1 = R1, inR2 = R2, inR5 = R5;
    u32 state = R12;

    GUEST_CALL(battle_scrollAccumulateStep_8031176);

    R2 = 0;
    R3 = 0x20;
    R4 = 0x20;
    R5 = 0x20;
    R6 = 1;
    GUEST_CALL(battle_scrollHitTestLineVsWedge_8030F7C);

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

/* ── sub_8031420 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battleObject_applyShakeOffsetAndProject_8031136 has no typed C interface yet */
static inline u32 call_sub_8031136(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battleObject_applyShakeOffsetAndProject_8031136);
    return R0;
}

typedef struct { u32 r0, r4, r6, r7, r8, r9; } call_sub_8030E14_result;
/* register form: generated wrapper: sub_8030E14 has no typed C interface yet */
static inline call_sub_8030E14_result call_sub_8030E14(u32 r0, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(sub_8030E14);
    return (call_sub_8030E14_result){ R0, R4, R6, R7, R8, R9 };
}

typedef struct { u32 r3, r4, r6; bool v; } call_camera_moveByDelta_80310B0_result;
/* register form: generated wrapper: camera_moveByDelta_80310B0 has no typed C interface yet */
static inline call_camera_moveByDelta_80310B0_result call_camera_moveByDelta_80310B0(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(camera_moveByDelta_80310B0);
    return (call_camera_moveByDelta_80310B0_result){ R3, R4, R6, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r4, r6, r7, r8, r9; } call_sub_8030F04_result;
/* register form: generated wrapper: battle_runTextboxUpdateAndSetStateBit1_8030F04 has no typed C interface yet */
static inline call_sub_8030F04_result call_sub_8030F04(u32 r0, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_runTextboxUpdateAndSetStateBit1_8030F04);
    return (call_sub_8030F04_result){ R1, R4, R6, R7, R8, R9 };
}

/* Camera edge-scroll driver (sub_8031420).
 *
 * Scrolls the camera off the edge of one map and into the next, in two passes.
 * Each pass re-anchors the camera with battleObject_applyShakeOffsetAndProject_8031136, asks for a scroll delta
 * (sub_8030E14 for the first pass, battle_runTextboxUpdateAndSetStateBit1_8030F04 for the second), applies it
 * with camera_moveByDelta_80310B0, and folds the move's status flag (r3) into
 * the caller's r12 accumulator: bit 0 from the first pass, bit 1 from the
 * second. If the first move reports status 1 the routine returns immediately
 * and skips the second pass.
 *
 * r0/r1/r2/r5 are preserved across the whole routine (the original saves and
 * pops them); r3 ends up holding the last move's status flag. r4/r6 and the
 * condition flags are scratch.
 */

/* Everything the routine leaves behind: r0/r1/r2/r5 unchanged from entry,
 * r3 = status of the last camera move, r12 = accumulated status bits. */
typedef struct {
    u32 r0;
    u32 r1;
    u32 r2;
    u32 r3;
    u32 r5;
    u32 r12;
} EdgeScrollResult;

static EdgeScrollResult camera_edge_scroll(u32 r0, u32 r1, u32 r2, u32 r5, u32 r12)
{
    EdgeScrollResult out;

    /* Pass 1: re-anchor, then request a scroll with extents
     * (x = -1, y = -1, width 0x20, height 0, layer 0) and apply it. */
    u32 anchor = call_sub_8031136(r0, r5);
    call_sub_8030E14_result delta0 =
        call_sub_8030E14(anchor, (u32)-1, (u32)-1, 0x20, 0, 0);
    call_camera_moveByDelta_80310B0_result move0 =
        call_camera_moveByDelta_80310B0(delta0.r0, 0);

    r12 |= move0.r3;

    out.r0 = r0;
    out.r1 = r1;
    out.r2 = r2;
    out.r5 = r5;
    out.r3 = move0.r3;
    out.r12 = r12;

    /* Status 1 after the first move: the scroll is complete, stop here. */
    if (move0.r3 == 1)
        return out;

    /* Pass 2: re-anchor again, then request the opposite scroll
     * (x = 0x20, y = 0, width -1, height -1, layer 0) and apply it. */
    anchor = call_sub_8031136(r0, r5);
    call_sub_8030F04_result delta1 =
        call_sub_8030F04(anchor, 0x20, 0, (u32)-1, (u32)-1, 0);
    call_camera_moveByDelta_80310B0_result move1 =
        call_camera_moveByDelta_80310B0(0, delta1.r1);

    out.r3 = move1.r3;
    out.r12 = r12 | (move1.r3 << 1);
    return out;
}

/* sub_8031420(r0, r5 = camera anchor, r12 = status accumulator)
 *   -> r0 r1 r2 r5 unchanged, r3 = last move status, r12 = accumulated bits */
Next sub_8031420(void) {
    EdgeScrollResult out = camera_edge_scroll(R0, R1, R2, R5, R12);
    R0 = out.r0;
    R1 = out.r1;
    R2 = out.r2;
    R3 = out.r3;
    R5 = out.r5;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── sub_8031478 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: battle_scrollAccumulateStep_8031176 has no typed C interface yet */
static inline u32 call_sub_8031176(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battle_scrollAccumulateStep_8031176);
    return R0;
}

typedef struct { u32 r0, r4, r6, r7, r8, r9; } call_sub_8030E8C_result;
/* register form: generated wrapper: camera_computeScrollDelta_8030E8C has no typed C interface yet */
static inline call_sub_8030E8C_result call_sub_8030E8C(u32 r0, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(camera_computeScrollDelta_8030E8C);
    return (call_sub_8030E8C_result){ R0, R4, R6, R7, R8, R9 };
}

typedef struct { u32 r3, r4, r6; bool v; } call_camera_moveByDelta_80310B0_result_sub_8031478;
/* register form: generated wrapper: camera_moveByDelta_80310B0 has no typed C interface yet */
static inline call_camera_moveByDelta_80310B0_result_sub_8031478 call_camera_moveByDelta_80310B0_sub_8031478(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(camera_moveByDelta_80310B0);
    return (call_camera_moveByDelta_80310B0_result_sub_8031478){ R3, R4, R6, OVERFLOW_FLAG() };
}

/* register form: generated wrapper: battleObject_applyShakeOffsetAndProject_8031136 has no typed C interface yet */
static inline u32 call_sub_8031136_sub_8031478(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battleObject_applyShakeOffsetAndProject_8031136);
    return R0;
}

typedef struct { u32 r1, r4, r6, r7, r8, r9; } call_sub_8030F04_result_sub_8031478;
/* register form: generated wrapper: battle_runTextboxUpdateAndSetStateBit1_8030F04 has no typed C interface yet */
static inline call_sub_8030F04_result_sub_8031478 call_sub_8030F04_sub_8031478(u32 r0, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_runTextboxUpdateAndSetStateBit1_8030F04);
    return (call_sub_8030F04_result_sub_8031478){ R1, R4, R6, R7, R8, R9 };
}

/* Two-pass camera scroll used during battle transitions.
 * Pass 1 nudges the camera horizontally; pass 2 (only if horizontal delta != 1)
 * nudges vertically. r12 accumulates a bitmask of both deltas. */

typedef struct { u32 r3; u32 r12; } cam_scroll_result_t;

static cam_scroll_result_t two_pass_cam_scroll(u32 r0, u32 r5, u32 r12_in) {
    /* Pass 1: adjust battle objects, then nudge camera horizontally (dy = 0). */
    u32 r0a = call_sub_8031176(r0, r5);
    call_sub_8030E8C_result e8c = call_sub_8030E8C(r0a, 0x20, 0x20, 0, 0x20, 0);
    call_camera_moveByDelta_80310B0_result_sub_8031478 cam1 =
        call_camera_moveByDelta_80310B0_sub_8031478(e8c.r0, 0);

    /* Fold horizontal delta (shift 0) into the accumulated mask. */
    u32 r12 = r12_in | cam1.r3;

    /* If the camera shifted by exactly one unit horizontally, skip vertical pass. */
    if (cam1.r3 == 1)
        return (cam_scroll_result_t){ .r3 = cam1.r3, .r12 = r12 };

    /* Pass 2: adjust objects in reverse, then nudge camera vertically (dx = 0). */
    u32 r0b = call_sub_8031136_sub_8031478(r0, r5);
    call_sub_8030F04_result_sub_8031478 f04 =
        call_sub_8030F04_sub_8031478(r0b, 0x20, 0, (u32)(0 - 1), (u32)(0 - 1), 0);
    call_camera_moveByDelta_80310B0_result_sub_8031478 cam2 =
        call_camera_moveByDelta_80310B0_sub_8031478(0, f04.r1);

    /* Fold vertical delta (shift 1) into the mask. */
    r12 |= (cam2.r3 << 1);

    return (cam_scroll_result_t){ .r3 = cam2.r3, .r12 = r12 };
}

/* sub_8031478(r0, r5, r12) -> r0 r1 r2 r3 r5 r12
 * r0, r1, r2, r5 are preserved (saved/restored via stack in the original).
 * r3, r12 are computed by the two-pass scroll. */
Next sub_8031478(void) {
    /* register form: preserve r0-r2,r5 across wrapper calls that clobber them */
    u32 s0 = R0, s1 = R1, s2 = R2, s5 = R5;
    cam_scroll_result_t res = two_pass_cam_scroll(s0, s5, R12);
    R0 = s0, R1 = s1, R2 = s2, R5 = s5, R3 = res.r3, R12 = res.r12;
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

    /* each word is read just before its store, as the original: pos may point into dword_200F3E0 */
    R7 = ((s32)LD32(pos) >> 15) - targetX;
    ST32(A_dword_200F3E0, R7);

    R7 = ((s32)LD32(pos + 4) >> 15) - targetY;
    ST32(A_dword_200F3E0 + 4, R7);

    R7 = ((s32)LD32(pos + 0xC) >> 15) - targetX;
    ST32(A_dword_200F3E0 + 0xC, R7);

    R7 = ((s32)LD32(pos + 0x10) >> 15) - targetY;
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

/* ── battle_aiIndexToActorCount_8031980 ── */
/* battle_aiIndexToActorCount_8031980(r0 = selector):
 * Loads the address of dword_2013940 into r5 and returns a small mode id
 * in r0: 3 when the selector is 1, otherwise 2. The caller can also branch
 * on the flags left behind: n/z come from the returned value, c/v from the
 * comparison against 1. */
Next battle_aiIndexToActorCount_8031980(void) {
    R5 = A_dword_2013940;

    SET_FLAGS_CMP(R0, 1);
    R0 = (R0 == 1) ? 3 : 2;
    /* the final MOVS only rewrites n/z; c/v stay from the CMP above */
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── battleObject_getChipLockColourOne_8031A5E ── */
/* battleObject_getChipLockColourOne_8031A5E: trivial wrapper that calls battle_aiIndexToActorCount_8031980 with r0 = 1.
 * Everything the callee leaves behind (r0, r5, flags, lr at the return
 * address) is passed straight through to our caller. */
Next battleObject_getChipLockColourOne_8031A5E(void) {
    R0 = 1;
    GUEST_CALL(battle_aiIndexToActorCount_8031980);
    RETURN_TO_CALLER();
}

/* ── overworldObject55_ReadStreamEntry_8031A68 ── */
/* overworldObject55_ReadStreamEntry_8031A68(r0 = pointer into a data stream):
 * Reads one word from the stream at r0, stores its low halfword into the
 * eOverwordMapObject55 struct's value field (+4), advances the stream
 * pointer by one entry and records it in the struct (+0), then marks both
 * terminator bytes (+6, +7) as 0xFE. */
Next overworldObject55_ReadStreamEntry_8031A68(void) {
    u32 state = A_dword_2013920;

    u32 value = LD32(R0);
    ST16(state + 4, value);          /* word_2013924: low halfword of the entry */

    R0 = R0 + 4;                     /* skip to the next entry in the stream */
    ST32(state, R0);                 /* dword_2013920: updated read pointer */

    ST8(state + 6, 0xFE);            /* byte_2013926: terminator/sentinel */
    ST8(state + 7, 0xFE);            /* byte_2013927: terminator/sentinel */

    RETURN_TO_CALLER();
}

/* ── chatbox_getCursorBlinkState_8031B7C ── */
/* chatbox_getCursorBlinkState_8031B7C(r0 = query id):
 * Fetches one byte out of the overworld map object's state. With r0 == 1 it
 * follows dword_2013920 to the object, reads its pointer at +0x14, and returns
 * the byte at +1 of that structure; any other r0 returns 0. Callers also
 * receive the intermediate pointers in r5 (object table) and r2 (structure),
 * plus the flags of the `cmp r0, #1` (and on the zero path the subsequent
 * `mov r0, #0` updates n/z on top of them). */
Next chatbox_getCursorBlinkState_8031B7C(void) {
    u32 mode = R0;

    R5 = A_dword_2013920;
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
    GUEST_CALL(textbox_computeCursorBlinkAdvance_80315B8); /* open the message window box */

    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C); /* start the text print */

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
    GUEST_CALL(textbox_computeCursorBlinkAdvance_80315B8);

    /* Redraw/refresh the text box using the blit results in r2..r5. */
    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);

    RETURN_TO_CALLER();
}

/* ── battleObject_checkInAttackRange_08031BE2 ── */
/* battleObject_checkInAttackRange_08031BE2: sequence of three setup/update calls.
 *
 * R0 and R5 arrive as arguments. The first callee (battle_calcRelPosToTarget_803172E) is invoked
 * with them unchanged. Then fixed tile/map parameters are loaded into
 * r2-r5 (0, 0x10, 0xC, 0xC) for math_pointCrossHalfPlaneTest_8031594, and finally chatbox_getCursorBlinkState_8031B7C is
 * called with whatever registers math_pointCrossHalfPlaneTest_8031594 left behind (r0 and r5 are
 * carried through). All register and flag effects on return come from the
 * last callee, so nothing needs to be set explicitly here. */
Next battleObject_checkInAttackRange_08031BE2(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R4 = 0xC;
    R3 = 0x10;
    R5 = 0xC;

    GUEST_CALL(math_pointCrossHalfPlaneTest_8031594);
    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);

    RETURN_TO_CALLER();
}

/* ── battle_aiUpdate_8031BFA ── */
/* battle_aiUpdate_8031BFA:
 * Runs battle_calcRelPosToTarget_803172E on the values the caller left in r0/r5, then invokes
 * battleObject_isHitByPanelRectangle_8031570 with a fixed argument set (r2 = 0, r3 = r4 = 0x10, r5 = 0)
 * and finally chatbox_getCursorBlinkState_8031B7C. Register results and flags are whatever the
 * last callee leaves behind; lr is untouched (the original just pushes
 * lr and pops it back into pc). */
Next battle_aiUpdate_8031BFA(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R4 = 0x10;
    R3 = 0x10;
    R5 = 0;
    GUEST_CALL(battleObject_isHitByPanelRectangle_8031570);

    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);

    RETURN_TO_CALLER();
}

/* ── battle_calcRelPosAndAdvanceTextboxBlink_8031C2A ── */
/* battle_calcRelPosAndAdvanceTextboxBlink_8031C2A(r0, r5 = context):
 * Runs a short helper sequence: first battle_calcRelPosToTarget_803172E on the incoming context,
 * then textbox_computeCursorBlinkAdvance_80315B8 with the fixed arguments (r2 = 1, r3 = 0x10, r4 = 1,
 * r5 = 0x10), and finally chatbox_getCursorBlinkState_8031B7C. The caller sees whatever registers
 * and flags chatbox_getCursorBlinkState_8031B7C leaves behind. */
Next battle_calcRelPosAndAdvanceTextboxBlink_8031C2A(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 1;
    R4 = 1;
    R3 = 0x10;
    R5 = 0x10;
    GUEST_CALL(textbox_computeCursorBlinkAdvance_80315B8);

    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);
    RETURN_TO_CALLER();
}

/* ── battle_aiPointLeftOfLineSideTest_8031C42 ── */
/* battle_aiPointLeftOfLineSideTest_8031C42(r0, r5): thin wrapper that chains three battle-helper calls.
 *
 * The caller's r0/r5 are passed straight through to battle_calcRelPosToTarget_803172E; afterwards
 * a fixed parameter block (r2 = 0, r3 = 14, r4 = 14, r5 = 0) is set up for
 * math_pointLeftOfLineTest_80315DC, and whatever it leaves in r0/r2/r3/r4/r5 is handed on to
 * chatbox_getCursorBlinkState_8031B7C, whose results (registers and flags) become this function's
 * results. */
Next battle_aiPointLeftOfLineSideTest_8031C42(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R3 = 0xE;
    R4 = 0xE;
    R5 = 0;

    GUEST_CALL(math_pointLeftOfLineTest_80315DC);
    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);

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
/* sub_8031C5A: enable the flag passed to chatbox_getCursorBlinkState_8031B7C as r0 = 1 (e.g. mark
 * something as done/on); all results (r0, r2, r5, flags) come straight from
 * the callee. */
Next sub_8031C5A(void) {
    R0 = 1;
    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);
    RETURN_TO_CALLER();
}

/* ── chatbox_renderBattleCursorStep_8031C7C ── */
/* chatbox_renderBattleCursorStep_8031C7C — battle helper that runs a short setup sequence:
 * 1. battle_calcRelPosToTarget_803172E with the caller's r0/r5 arguments,
 * 2. textbox_computeCursorBlinkAdvance_80315B8 with a fixed parameter set (14, 14, 0, 16),
 * 3. chatbox_getCursorBlinkState_8031B7C to finish.
 * All results (r0, r2-r7) and the NZCV flags are whatever the last
 * callee leaves behind. */
Next chatbox_renderBattleCursorStep_8031C7C(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0xE;
    R4 = 0;
    R3 = 0xE;
    R5 = 0x10;

    GUEST_CALL(textbox_computeCursorBlinkAdvance_80315B8);
    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);

    RETURN_TO_CALLER();
}

/* ── battle_aiStorySub3_8031C94 ── */
/* battle_aiStorySub3_8031C94: run a three-step sequence on the object passed in r0/r5.
 * Calls battle_calcRelPosToTarget_803172E first, then textbox_computeCursorBlinkAdvance_80315B8 with a fixed parameter set
 * (r2=0, r3=0x10, r4=2, r5=2), then chatbox_getCursorBlinkState_8031B7C, which picks up whatever
 * r2/r5 textbox_computeCursorBlinkAdvance_80315B8 left behind. r3/r4 must survive the final call.
 * Everything (registers and flags) is simply passed through to the caller. */
Next battle_aiStorySub3_8031C94(void) {
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    R2 = 0;
    R4 = 2;
    R3 = 0x10;
    R5 = 2;
    GUEST_CALL(textbox_computeCursorBlinkAdvance_80315B8);

    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031CAC ── */
/* sub_8031CAC(r0, r5):
 * Compute the relative position to the battle target, run a half-plane test against
 * a point, then perform chatbox_getCursorBlinkState_8031B7C. Acts as a thin wrapper that prepares the
 * arguments for math_pointCrossHalfPlaneTest_8031594. */
Next sub_8031CAC(void) {
    /* Calculate the relative position from r0 toward the current battle target. */
    GUEST_CALL(battle_calcRelPosToTarget_803172E);

    /* Half-plane test arguments:
     *   r2 = 0 (unused/zero)
     *   r4 = 0xE, r3 = 0x10, r5 = 0xE — point/normal used by the test. */
    R2 = 0;
    R4 = 0xE;
    R3 = 0x10;
    R5 = 0xE;

    GUEST_CALL(math_pointCrossHalfPlaneTest_8031594);
    GUEST_CALL(chatbox_getCursorBlinkState_8031B7C);

    RETURN_TO_CALLER();
}
