/*
 * asm33.c: readable functions of bn6f asm33.s, up to 0x081263D0, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_812365C ── */
/* sub_812365C: dispatch stub.
 * Reads the selector byte at [R5+3] and jumps to the matching routine from
 * the two-entry table at off_8123670. Every register except R0/R1 and the
 * condition flags are passed straight through, so the selected handler runs
 * with the caller's state and its results (r0-r12, nz/c/v) flow unchanged
 * back to the original caller. */
Next sub_812365C(void) {
    u32 mode = LD8(R5 + 3);

    /* The original loads the handler address into R0 (and the selector into
     * R1) as a side effect of the table lookup before branching to it;
     * reproduce that so the callee sees the same register state. */
    R1 = mode;
    if (mode == 0) {
        R0 = 0x08123679;            /* off_8123670[0] = sub_8123678 (Thumb) */
        GUEST_CALL(sub_8123678);
    } else {
        R0 = 0x0812368D;            /* off_8123670[1] = sub_812368C (Thumb) */
        GUEST_CALL(sub_812368C);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8123678 ── */
/* sub_8123678(r5 = object pointer):
 * Runs a check by calling sprite_testFrameParamsMaskC0_81190F8 with argument 0. If it reports
 * success, writes state 4 to the object's byte at offset 3 and calls
 * sub_8123D70 to advance it; otherwise returns immediately.
 * Flags on return are those left by whichever callee ran last. */
Next sub_8123678(void) {
    R0 = 0;
    GUEST_CALL(sprite_testFrameParamsMaskC0_81190F8);   /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = 4;
        ST8(R5 + 3, R0);
        GUEST_CALL(sub_8123D70);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8123BC0 ── */
/* sub_8123BC0:
 * Passes a fixed argument set to sub_8123C20 and returns its result untouched.
 * Arguments are all constants: the word table at dword_8123BDC, the byte
 * table at byte_81233D8, 1 in r1, 4 in r3 and a length of 0x1C in r4.
 * Like the original, this wrapper preserves r4-r7 across the call (it only
 * sets r4 as an argument, then restores the caller's value afterwards). */
Next sub_8123BC0(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    R0 = A_dword_8123BDC; /* pointer to the word table (0x302F2E) */
    R1 = 1;
    R2 = A_byte_81233D8;  /* pointer to the byte table */
    R3 = 4;
    R4 = 0x1C;            /* entry count */
    GUEST_CALL(sub_8123C20);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── sub_8123BE0 ── */
/* sub_8123BE0: thin wrapper around sub_8123C20 with a fixed argument set.
 * r0 points at the 3-byte constant in the literal pool (dword_8123BFC =
 * 0x5A5856), r2 at the byte_81233E0 work area; the remaining arguments
 * (0x20, 5, 0x5C, 0) select its mode/geometry. The callee's r4/r6
 * arguments are set locally — the caller's r4-r7 are saved around the
 * call and restored on return. */
Next sub_8123BE0(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    R0 = A_dword_8123BFC; /* source data: the three bytes 0x5A, 0x58, 0x56 */
    R1 = 0x20;
    R2 = A_byte_81233E0;
    R3 = 5;
    R4 = 0x5C;
    R6 = 0;
    GUEST_CALL(sub_8123C20);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── sub_8123C00 ── */
/* sub_8123C00: prepare a fixed set of arguments and forward them to sub_8123C20.
 * The color/palette table at dword_8123C1C (0x5B5957 = three BGR555 entries) and
 * the work buffer byte_81233E8 are passed by address; the remaining arguments
 * (0x40, 6, 0x60, -, 1) are constants. r4-r7 are restored to their entry values
 * on return, matching the original's push/pop around the call. */
Next sub_8123C00(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R0 = A_dword_8123C1C;   /* color data */
    R1 = 0x40;
    R2 = A_byte_81233E8;    /* destination buffer */
    R3 = 6;
    R4 = 0x60;
    R6 = 1;

    GUEST_CALL(sub_8123C20);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8123D54 ── */
/* sub_8123D54(r0 = selection): call the string/label printer at sub_80465A0
 * with one of two fixed tables — byte_81233F0 when r0 is 0, byte_81233F8
 * otherwise. R1 emerges holding the original r0 (the callee leaves it alone),
 * and the callee's flag results pass through to the caller. */
Next sub_8123D54(void) {
    u32 which = R0;

    R1 = which;
    R0 = (which != 0) ? A_byte_81233F8 : A_byte_81233F0;

    GUEST_CALL(sub_80465A0);

    RETURN_TO_CALLER();
}

/* ── sub_8123E08 ── */
/* sub_8123E08(r5 = state pointer):
 * Checks bit 0x20 of the status byte unk_20018EC[*(u8*)(r5 + 0xC)].
 * Returns r0 = r2 = 1 if the bit is set, 0 otherwise.
 * Flags are left from the AND with 0x20 (NZ), as callers may branch on them. */
Next sub_8123E08(void) {
    u32 index = LD8(R5 + 0xC);
    u32 bit = LD8(A_unk_20018EC + index) & 0x20;

    SET_FLAGS_NZ(bit);

    u32 isSet = (bit != 0);
    R0 = isSet;
    R2 = isSet;
    RETURN_TO_CALLER();
}

/* ── submenu_subchip_8124134 ── */
/* submenu_subchip_8124134(r5 = sub-menu state struct):
 * If the chatbox is currently open (flag 0x80 of the chatbox state mask),
 * leave the sub-menu state untouched; otherwise reset its byte at +2 to 4
 * (restores the default sub-menu selection/mode after closing the chatbox).
 * Returns r0 = chatbox flag check result, or 4 if the state byte was reset.
 * Flags: those left by chatbox_mask_eFlags2009F38, or NZ from the stored value. */
Next submenu_subchip_8124134(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    if (R0 == 0) {
        /* Chatbox not open: restore the default state byte. */
        R0 = 4;
        ST8(R5 + 2, R0);
        SET_FLAGS_NZ(R0);
    }
    /* Otherwise keep the flags from the chatbox flag check. */
    RETURN_TO_CALLER();
}

/* ── bg_CopyTileBlock2At2_6_8x12_81245BC ── */
/* bg_CopyTileBlock2At2_6_8x12_81245BC: copy a 6x8 block of background tiles.
 *
 * Prepares the arguments for CopyBackgroundTiles (a leaf routine that takes
 * six arguments and may clobber the low registers, including r4/r5) and calls
 * it, preserving the caller's r4-r7 across the call. r1 and r2 are left as
 * CopyBackgroundTiles leaves them; the condition flags are likewise whatever
 * the callee's last flag-setting instruction produced. */
Next bg_CopyTileBlock2At2_6_8x12_81245BC(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    R0 = 2;              /* j: destination tile column */
    R1 = 6;              /* i: destination tile row */
    R2 = 2;              /* which 32x32 tile block to copy from */
    R3 = A_unk_201EC2C;  /* tile_ids: source tile id table */
    R4 = 8;              /* j_size */
    R5 = 0xC;            /* i_size */

    GUEST_CALL(CopyBackgroundTiles);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── chatbox_runScript201C100_812474C ── */
/* chatbox_runScript201C100_812474C(r0 = script index):
 * Run one text script from the chatbox text archive (eTextScript201C100).
 * All register and flag outputs are just whatever chatbox_runScript leaves. */
Next chatbox_runScript201C100_812474C(void) {
    u32 scriptIdx = R0;

    R1 = scriptIdx;                 /* archive in r0, script index in r1 */
    R0 = A_eTextScript201C100;
    GUEST_CALL(chatbox_runScript_803FD9C);

    RETURN_TO_CALLER();
}

/* ── initRefs_buildAndDecompressTable_8125280 ── */
/* initRefs_buildAndDecompressTable_8125280: assemble the "init references" table in RAM at unk_2029000 from
 * the ROM pointer table at off_8125310, then decompress-and-copy the two data
 * blobs that follow it.
 *
 * The source table holds 5 entries of 4 words each (0x50 bytes total):
 *   word 0..1 - passed to sub_811FB84, whose result is stored instead
 *   word 2..3 - copied verbatim
 * A final 0 word terminates the assembled table. r4-r7 are preserved. */
Next initRefs_buildAndDecompressTable_8125280(void) {
    u32 src = A_off_8125310;  /* init-ref entries in ROM */
    u32 dst = A_unk_2029000;  /* assembled table in RAM */

    for (u32 entry = 0; entry < 0x50 / 16; entry++) {
        R0 = LD32(src);
        src += 4;
        R1 = LD32(src);
        src += 4;
        GUEST_CALL(sub_811FB84);
        ST32(dst, R0);
        dst += 4;

        for (u32 word = 0; word < 2; word++) {
            ST32(dst, LD32(src));
            dst += 4;
            src += 4;
        }
    }
    ST32(dst, 0);

    /* decompress the assembled table, then the blob described by it */
    R0 = A_unk_2029000;
    GUEST_CALL(decompAndCopyData);
    R0 = A_byte_8125368;
    GUEST_CALL(decompAndCopyData);

    RETURN_TO_CALLER();
}

/* ── menu_confirmExitAndFade_812580C ── */
/* menu_confirmExitAndFade_812580C(r0 = value stored into the menu state at [r5+2]):
 * record the caller's selection, play the "exit submenu" sound effect,
 * then start a screen fade (fade kind 0xC, duration 0x10).
 * Only r0/r1/lr and the flags are clobbered; r3 and r10 pass through. */
Next menu_confirmExitAndFade_812580C(void) {
    u8 selection = (u8)R0;

    ST8(R5 + 2, selection);

    R0 = SOUND_EXIT_SUBMENU;
    GUEST_CALL(PlaySoundEffect);

    R0 = 0xC;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    RETURN_TO_CALLER();
}

/* ── playSoundEffect_7A_81258E8 ── */
/* playSoundEffect_7A_81258E8: play sound effect 0x7A. All other registers (r1-r7, r10)
 * pass through to the callee and are preserved unchanged. */
Next playSoundEffect_7A_81258E8(void) {
    R0 = 0x7A;
    GUEST_CALL(PlaySoundEffect);
    RETURN_TO_CALLER();
}
