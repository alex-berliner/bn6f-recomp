/*
 * battle_panels.c — the battle field's panel tiles (bn6f object.s:1091-1732).
 *
 * The panels of the battle field are drawn on the BG as blocks of 5x3
 * 8x8 tiles in tile block 2. ePanelData holds one 0x20-byte record per
 * panel slot (0x28 slots): the panel's column and row, the tile-set
 * family and side it is drawn with, a one-shot redraw pair, a one-shot
 * overlay and a flag for the strip drawn under the panel.
 *
 * updateBattlePanels_800C5E0 runs every frame of a battle: it first advances the panel
 * tile animations (stepPanelTileAnimations_800C192), then walks the slots and draws every
 * panel that sits on the field, through three helpers:
 *   drawPanel_800C01C  the panel itself (family 0xFF blanks its block)
 *   drawPanelOverlay_800C0BA  one of two overlays drawn over the panel
 *   drawPanelUnderStrip_800C100  the one-tile-high strip under the panel
 * A slot holding no panel is blanked, and sub_800C138 draws whatever
 * shows in its place.
 *
 * Which tiles a panel uses depends on the field's orientation: when
 * BattleState +0xD is set the field is mirrored (the flipped field),
 * the columns run the other way (mirror tables byte_800C0AA and
 * byte_800C182), families 0xB and 0xC swap and the side byte inverts.
 *
 * stepPanelTileAnimations_800C192 cycles the panel tile animations: off_800C1DC is a
 * NULL-terminated list of records that keep their state in the counter
 * array byte_2036740 — one slot for the current frame, one for the
 * frames left before the next — with the frames (tile data *, delay)
 * at +8. When a delay runs out the frame advances (wrapping) and the
 * frame's 32 bytes are copied into the record's IWRAM buffer.
 */
#include "game.h"
#include "runtime.h"
#include <stdbool.h>

/* ---- one slot of ePanelData (0x20 bytes each) ---- */
enum {
    PANEL_ACTIVE    = 0x00,  /* 0: the slot holds no panel */
    PANEL_FLASH     = 0x01,  /* one-shot: draw overlay (value - 1) over the panel */
    PANEL_UNK_02    = 0x02,  /* not read here */
    PANEL_UNK_03    = 0x03,  /* not read here */
    PANEL_ART       = 0x06,  /* tile-set family the panel is drawn with */
    PANEL_SIDE      = 0x07,  /* side variant of that family (player / enemy) */
    PANEL_NEW_ART   = 0x08,  /* one-shot redraw: family drawn while +0xD is set */
    PANEL_NEW_SIDE  = 0x09,
    PANEL_COLUMN    = 0x0A,
    PANEL_ROW       = 0x0B,
    PANEL_STRIP     = 0x0C,  /* draw the strip under the panel */
    PANEL_REDRAW    = 0x0D,  /* draw the +8/+9 family this one frame */
};
#define PANEL_STRIDE 0x20
#define PANEL_COUNT  0x28      /* slots the walker visits */

enum { ART_BLANK = 0xFF };     /* the family that blanks a panel's block */

/* one panel tile animation (off_800C1DC lists pointers to them) */
enum {
    ANIM_FRAME_SLOT  = 0x00,   /* counter slot holding the current frame */
    ANIM_FRAME_COUNT = 0x01,
    ANIM_DELAY_SLOT  = 0x02,   /* counter slot holding frames to the next one */
    ANIM_BUFFER      = 0x04,   /* where the current frame's tiles are copied */
    ANIM_FRAMES      = 0x08,   /* frames: tile data *, delay (u8, at +4) */
};

/* ---- where a panel's tile block sits ---- */

/* BattleState +0xD: the field is mirrored (flipped). */
static u32 field_flipped(void) {
    return LD8(LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_Unk_0d);
}

/* First BG tile column of the 5-tile-wide block of field `column`; each
 * mirror table holds one row of 8 entries per field orientation. */
static u32 block_column(u32 table, u32 column) {
    return LD8(table + (field_flipped() << 3) + column);
}

/* First BG tile row of panel `row`: the field begins at tile row 6 and
 * a panel block is 3 tiles tall. */
static u32 block_row(u32 row) {
    return row * 3 + 6;
}

/* ---- drawing one panel (drawPanel_800C01C, drawPanelOverlay_800C0BA, drawPanelUnderStrip_800C100) ---- */

/* Draw panel (column, row) with tile-set family `art` on `side`. A
 * family holds one 32-byte tile-id set per side and field row (top,
 * middle and bottom panels have different art); ART_BLANK blanks the
 * block instead. On the flipped field families 0xB and 0xC swap and the
 * side inverts — which, as the original, also turns an ART_BLANK
 * request into family 0xC there. */
static void draw_panel(u32 column, u32 row, u32 art, u32 side) {
    u32 flip = field_flipped();
    u32 variant = side ^ flip;
    if (flip != 0 && (s32)art >= 0xB)
        art = (art == 0xC) ? 0xB : 0xC;

    u32 ids;
    if (art == ART_BLANK)
        ids = A_byte_800C08C;                                /* all-zero tile ids */
    else
        ids = A_byte_86DFA98 + (art * 6 + variant * 3 + row - 1) * 0x20;

    R0 = block_column(A_byte_800C0AA, column);
    R1 = block_row(row);
    R2 = 2;                    /* tile block 2 */
    R3 = ids;
    R4 = 5;                    /* tiles wide */
    R5 = 3;                    /* tiles tall */
    GUEST_CALL(iCopyBackgroundTiles);
}

/* Draw overlay `which` (of two) over panel (column, row). */
static void draw_panel_overlay(u32 column, u32 row, u32 which) {
    R0 = block_column(A_byte_800C0AA, column);
    R1 = block_row(row);
    R2 = 2;
    R3 = LD32(A_off_800C0F4 + which * 4);                    /* the overlay tile ids */
    R4 = 5;
    R5 = 3;
    GUEST_CALL(CopyBackgroundTiles);
}

/* Draw the one-tile-high strip of `row` at field `column`, in side
 * variant `side` (inverted on the flipped field). It has its own
 * column mirror table. */
static void draw_panel_strip(u32 column, u32 row, u32 side) {
    R0 = block_column(A_byte_800C182, column);
    R1 = block_row(row);
    R2 = 2;
    R3 = A_byte_86E0498 + ((side ^ field_flipped()) << 5);
    R4 = 5;
    R5 = 1;
    GUEST_CALL(CopyBackgroundTiles);
}

/* ---- the panel tile animations (stepPanelTileAnimations_800C192) ---- */

static void advance_panel_animations(void) {
    u32 counters = A_byte_2036740;
    u32 list = A_off_800C1DC;
    for (;;) {
        u32 anim = LD32(list);
        if (anim == 0)
            return;

        u32 delay_slot = LD8(anim + ANIM_DELAY_SLOT);
        u32 delay = LD8(counters + delay_slot) - 1;
        ST8(counters + delay_slot, delay);
        R2 = delay;                          /* leftover register, as the original */

        if ((s32)delay > 0) {                /* still waiting for the next frame */
            list += 4;
            continue;
        }

        /* the wait is over: the next frame, wrapping, and its delay */
        u32 frame_slot = LD8(anim + ANIM_FRAME_SLOT);
        u32 frame_count = LD8(anim + ANIM_FRAME_COUNT);
        R3 = frame_count;                    /* leftover register, as the original */
        u32 frame = LD8(counters + frame_slot) + 1;
        if ((s32)frame >= (s32)frame_count)
            frame = 0;
        ST8(counters + frame_slot, frame);

        u32 next = anim + ANIM_FRAMES + frame * 8;
        ST8(counters + delay_slot, (u8)LD32(next + 4));
        R0 = LD32(next);                     /* the frame's tiles */
        R1 = LD32(anim + ANIM_BUFFER);
        R2 = 0x20;
        GUEST_CALL(CopyByEightWords);
        list += 4;
    }
}

/* ---- every frame of a battle (updateBattlePanels_800C5E0) ---- */

/* object_isValidPanel returns whether the field has a panel at that
 * column and row; the original branches on the Z it leaves, which
 * mirrors its return value. */
static bool panel_on_field(u32 column, u32 row) {
    R0 = column;
    R1 = row;
    GUEST_CALL(object_isValidPanel);
    return R0 != 0;
}

static void update_panels(void) {
    advance_panel_animations();

    u32 panel = A_ePanelData;
    for (u32 slot = 0; slot < PANEL_COUNT; slot++, panel += PANEL_STRIDE) {
        u32 column = LD8(panel + PANEL_COLUMN);
        u32 row = LD8(panel + PANEL_ROW);

        if (!panel_on_field(column, row))    /* off the field: nothing to draw */
            continue;

        if (LD8(panel + PANEL_ACTIVE) == 0) {
            /* the slot holds no panel: blank its tiles, then draw what
             * shows in its place */
            draw_panel(column, row, ART_BLANK, 0);
            R0 = column;
            R1 = row + 1;
            R2 = LD8(panel + PANEL_SIDE);
            GUEST_CALL(sub_800C138);
            continue;
        }

        if (LD8(panel + PANEL_REDRAW) != 0) {
            /* a one-shot redraw with the panel's changed art */
            ST8(panel + PANEL_REDRAW, 0);
            draw_panel(column, row, LD8(panel + PANEL_NEW_ART),
                       LD8(panel + PANEL_NEW_SIDE));
        } else if (LD8(panel + PANEL_FLASH) != 0) {
            /* a one-shot overlay over the panel */
            u32 which = LD8(panel + PANEL_FLASH) - 1;
            ST8(panel + PANEL_FLASH, 0);
            draw_panel_overlay(column, row, which);
        } else {
            draw_panel(column, row, LD8(panel + PANEL_ART), LD8(panel + PANEL_SIDE));
        }

        if (LD8(panel + PANEL_STRIP) != 0)
            draw_panel_strip(column, row + 1, LD8(panel + PANEL_SIDE));
    }
}

/* ---- entry points for the rest of the game ----
 * Each restores r4-r7 as the original's pops do; r1-r3 are left as the
 * last CopyBackgroundTiles call left them, as the original leaves them. */

Next drawPanel_800C01C(void) {
    const u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    draw_panel(R0, R1, R2, R3);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    RETURN_TO_CALLER();
}

Next drawPanelOverlay_800C0BA(void) {
    const u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    draw_panel_overlay(R0, R1, R2);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    RETURN_TO_CALLER();
}

Next drawPanelUnderStrip_800C100(void) {
    const u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    draw_panel_strip(R0, R1, R2);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* r2 and r3 (its only outputs) come out of the animation pass exactly
 * as the original's loop leaves them: the last delay written, the last
 * frame count, or the copy call's leftovers. */
Next stepPanelTileAnimations_800C192(void) {
    advance_panel_animations();
    RETURN_TO_CALLER();
}

/* leaves the flags as the walker's final `cmp r6, #0x28` does, with the
 * loop run through all 0x28 slots (so r6 has counted up to 0x28). */
Next updateBattlePanels_800C5E0(void) {
    const u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    update_panels();
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    SET_FLAGS_CMP(PANEL_COUNT, PANEL_COUNT);
    RETURN_TO_CALLER();
}
