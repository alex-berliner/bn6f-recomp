/*
 * asm32_8119438.c: readable functions of bn6f asm32.s, 0x08119438-0x0811CAEC, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_8119438 ── */
/* Chip graphics & description rendering for the menu/selection panel.
 * Reads the chip ID from the panel object, blits the chip's tile graphic and
 * palettes into VRAM, queues DMA, fills the special-effect region, and renders
 * the text description. */

/* Chip panel object field offsets */
#define oChipPanel_VisualSet      0x05
#define oChipPanel_PowerFlag      0x0A
#define oChipPanel_AttrIndex      0x0C
#define oChipPanel_ChipId         0x0E

/* ChipData struct field offsets */
#define oChipData_AttrByte        0x06
#define oChipData_GfxFlags        0x09
#define oChipData_AttackPower     0x1A
#define oChipData_CustomGfxPtr    0x24
#define oChipData_CustomPalPtr    0x28

/* VRAM / palette constants */
#define TILE_VRAM_BASE            A_vObjectTiles          /* 0x6010000 */
#define TILESET_BYTE_SIZE         0x800
#define GFX_SRC_ROW_BYTES         0xE0
#define GFX_VRAM_ROW_STRIDE       0x100
#define GFX_NUM_ROWS              6
#define PAL_SET_BYTES             0x20
#define ATTR_PAL_BYTES            0x40
#define CUSTOM_PAL_BYTES          0x80
#define FX_REGION_OFFSET          0x8C0
#define FX_REGION_SIZE            0x100
#define FX_FILL_PATTERN           0xAAAA
#define CHIP_FX_HIGH_POWER_THRESH 1000  /* 0x3E8 */

/* Text rendering constants */
#define TEXT_LINE_LEN             0x0A
#define TEXT_MODE                 3

/* Submenu state check values */
#define SUBMENU_PAGE_CHIPS        0x18
#define SUBMENU_SUB_CHIPS         0x1C

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 fetch_chip_data(u16 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls sub_81195E4 outside this file, which has no typed C interface yet */
static void draw_chip_placeholder(void) {
    R0 = 0;
    GUEST_CALL(sub_81195E4);
}

/* register form: calls ZeroFillByEightWords outside this file, which has no typed C interface yet */
static void zero_fill_vram(u32 dest, u32 size) {
    R0 = dest;
    R1 = size;
    GUEST_CALL(ZeroFillByEightWords);
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_tile_dma(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void copy_palette_data(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(CopyWords);
}

/* register form: calls sub_8010CE0 outside this file, which has no typed C interface yet */
static u32 get_chip_filler_high_power(u16 chip_id) {
    R0 = chip_id;
    GUEST_CALL(sub_8010CE0);
    return R0;
}

/* register form: calls sub_8010C76 outside this file, which has no typed C interface yet */
static u32 get_chip_filler_low_power(u16 chip_id) {
    R0 = chip_id;
    GUEST_CALL(sub_8010C76);
    return R0;
}

/* register form: calls sub_8119594 outside this file, which has no typed C interface yet */
static void fill_chip_fx_region(u32 pattern, u32 vram_dest) {
    R0 = pattern;
    R1 = vram_dest;
    GUEST_CALL(sub_8119594);
}

/* register form: calls renderTextGfx_8045F8C outside this file, which has no typed C interface yet */
static void render_chip_description(u32 script, u32 chip_num, u32 desc_ptr,
                                    u32 text_vram, u32 line_len, u32 mode, u32 color) {
    R0 = script;
    R1 = chip_num;
    R2 = desc_ptr;
    R3 = text_vram;
    R4 = line_len;
    R5 = mode;
    R6 = color;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);
}

static void render_chip_gfx_and_text(u32 panel) {
    u16 chip_id = LD16(panel + oChipPanel_ChipId);
    u8  vis_set = LD8(panel + oChipPanel_VisualSet);

    /* No chip selected or custom graphic missing: draw placeholder and stop. */
    if (chip_id == 0) {
        draw_chip_placeholder();
        return;
    }

    u32 chip_data = fetch_chip_data(chip_id);
    u32 custom_gfx_src = LD32(chip_data + oChipData_CustomGfxPtr);
    if (custom_gfx_src == 0) {
        draw_chip_placeholder();
        return;
    }

    /* Determine VRAM destination from the visual-set pointer table. */
    u32 tile_entry = LD32(A_off_8119128 + (u32)vis_set * 4);
    u32 vram_dest = (LD32(tile_entry) << 5) + TILE_VRAM_BASE;

    /* Zero-fill the entire tileset area, then DMA the 6 rows of chip art. */
    zero_fill_vram(vram_dest, TILESET_BYTE_SIZE);

    u32 src_row = custom_gfx_src;
    u32 dst_row = vram_dest;
    for (u32 row = 0; row < GFX_NUM_ROWS; row++) {
        queue_tile_dma(src_row, dst_row, GFX_SRC_ROW_BYTES);
        src_row += GFX_SRC_ROW_BYTES;
        dst_row += GFX_VRAM_ROW_STRIDE;
    }

    /* Copy the chip's custom palette into work-RAM then DMA it to VRAM. */
    u32 pal_entry = LD32(A_off_8119170 + (u32)vis_set * 4);
    u32 pal_dst = A_byte_3001550 + ((u32)LD8(pal_entry) << 5);
    copy_palette_data(LD32(chip_data + oChipData_CustomPalPtr), pal_dst, PAL_SET_BYTES);

    u8 attr_idx = LD8(panel + oChipPanel_AttrIndex);
    u32 attr_pal_src = A_dword_86E2E98 + (u32)attr_idx * ATTR_PAL_BYTES;
    queue_tile_dma(attr_pal_src, vram_dest + TILESET_BYTE_SIZE, ATTR_PAL_BYTES);

    /* Copy and DMA the secondary palette set (from off_8119304). */
    u32 pal2_entry = LD32(A_off_8119304 + (u32)vis_set * 4);
    u32 pal2_dst = A_byte_3001550 + ((u32)LD8(pal2_entry) << 5);
    copy_palette_data(A_byte_86E587C, pal2_dst, PAL_SET_BYTES);

    /* Copy and DMA the attribute-indexed palette (0x80 bytes). */
    u8 attr_byte = LD8(chip_data + oChipData_AttrByte);
    u32 attr_pfx_src = A_dword_86E3B9C + (u32)attr_byte * CUSTOM_PAL_BYTES;
    queue_tile_dma(attr_pfx_src, vram_dest + 0x840, CUSTOM_PAL_BYTES);

    /* Copy and DMA the third palette set (from off_8119330). */
    u32 pal3_entry = LD32(A_off_8119330 + (u32)vis_set * 4);
    u32 pal3_dst = A_byte_3001550 + ((u32)LD8(pal3_entry) << 5);
    copy_palette_data(A_dword_86E43DC, pal3_dst, PAL_SET_BYTES);

    /* Fill the special-effects region based on chip graphics flags. */
    u8 gfx_flags = LD8(chip_data + oChipData_GfxFlags);
    u16 attack_power = LD16(chip_data + oChipData_AttackPower);

    if (gfx_flags & 0x10) {
        /* Bit 4: fill with 0xAAAA pattern. */
        fill_chip_fx_region(FX_FILL_PATTERN, vram_dest);
    } else if (gfx_flags & 0x02) {
        /* Bit 1: power-dependent fill. */
        u32 fill_val;
        if ((s32)attack_power <= (s32)CHIP_FX_HIGH_POWER_THRESH) {
            fill_val = attack_power;
        } else {
            /* High power: consult submenu context for the fill value. */
            u8 power_flag = LD8(panel + oChipPanel_PowerFlag);
            if (power_flag != 0 &&
                LD8(A_sSubmenu) == SUBMENU_PAGE_CHIPS &&
                LD8(A_sSubmenu + 1) == SUBMENU_SUB_CHIPS) {
                fill_val = get_chip_filler_high_power(chip_id);
            } else {
                fill_val = get_chip_filler_low_power(chip_id);
            }
        }
        fill_chip_fx_region(fill_val, vram_dest);
    } else {
        /* Neither flag: clear the FX region. */
        zero_fill_vram(vram_dest + FX_REGION_OFFSET, FX_REGION_SIZE);
    }

    /* Render the chip description text into the tileset VRAM. */
    u32 text_script = A_TextScriptChipDesc1;
    if ((s32)chip_id <= 0xFF)
        text_script = A_TextScriptChipDescriptions0;

    u32 desc_data = LD32(A_dword_811919C + (u32)vis_set * 4);
    u32 text_entry = LD32(A_off_8119128 + (u32)vis_set * 4);
    u32 text_bg = (LD32(text_entry + 8) << 5) + TILE_VRAM_BASE;

    render_chip_description(text_script, (u32)chip_id & 0xFF, desc_data,
                            text_bg, TEXT_LINE_LEN, TEXT_MODE, A_dword_86A5D60);
}

/* sub_8119438(r5 = chip_panel_ptr) -> outputs r1 r2 r3 + flags from callees;
 * r4-r7 are preserved (original restores them via push/pop). */
Next sub_8119438(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    render_chip_gfx_and_text(R5);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_811974C ── */
/* sub_811974C(r5 = state byte block):
 * Clear bit 1 of the byte at [r5] and write 0xC to the byte at [r5,#9].
 * Leaves r0 = 0xC, r1 = 2, and NZ flags from the "mov r0, #0xC". */
Next sub_811974C(void) {
    ST8(R5, LD8(R5) & ~2u);   /* bic r0, r1 with r1 = 2 */
    R1 = 2;
    R0 = 0xC;
    ST8(R5 + 9, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── npc_stepChainSetState0x10_811975C ── */
/* npc_stepChainSetState0x10_811975C: advance the state machine on the object in r5.
 * Runs sub_8119438's step, marks the object's state byte as 0x10
 * (state done/waiting to advance), then runs sprite_stepZoomShrinkOut_811976C's step.
 * r4-r9 are preserved; r0 and the flags come back from sprite_stepZoomShrinkOut_811976C. */
Next npc_stepChainSetState0x10_811975C(void) {
    GUEST_CALL(sub_8119438);

    ST8(R5 + 9, 0x10);

    GUEST_CALL(sprite_stepZoomShrinkOut_811976C);

    RETURN_TO_CALLER();
}

/* ── sprite_stepZoomShrinkOut_811976C ── */
/* sprite_stepZoomShrinkOut_811976C(r5 = sprite state record):
 * One step of a sprite's zoom animation. The scale byte at +0x12 counts down
 * in steps of 0x30; while it is still above 0x40 the new scale is applied to
 * the sprite. Once it drops to 0x40 or below (including wrapping below zero,
 * since the comparison is signed on the full 32-bit subtract result) the zoom
 * is finished: the scale snaps back to 0xC0, the animation counter at +0x09
 * is cleared and the sprite is marked as no longer scalable. The "scaling"
 * bit (0x02) is set in the state byte at +0x00 on every step.
 * On return the N/Z flags are whatever the called sprite routine left. */
Next sprite_stepZoomShrinkOut_811976C(void) {
    u32 scale;

    /* Set the "scale in progress" flag (r1 = 2 stays live into the call). */
    R1 = 2;
    ST8(R5, LD8(R5) | R1);

    /* Step the scale byte down by 0x30; the subtract wraps in 32 bits just
     * like the original `sub r0, #0x30`, and the store keeps only the low
     * byte. The signed comparison against 0x40 uses the full value. */
    scale = LD8(R5 + 0x12) - 0x30;
    ST8(R5 + 0x12, scale);

    if ((s32)scale > 0x40) {
        /* Still shrinking: apply the intermediate scale. */
        R0 = 0;
        R1 = scale;
        R2 = 0x40;
        GUEST_CALL(sprite_setScaleParameters);
    } else {
        /* Zoom finished: reset the scale and drop the scaling state. */
        ST8(R5 + 0x12, 0xC0);
        ST8(R5 + 9, 0);
        R0 = 0;
        GUEST_CALL(sprite_makeUnscalable);
    }

    RETURN_TO_CALLER();
}

/* ── npc_dispatchStateHandler_811979C ── */
/* npc_dispatchStateHandler_811979C(r5 = object):
 * Dispatch helper: looks up a handler in the jump table at off_81197B0,
 * indexed by the object's state byte at offset +9, and calls it with
 * r0 = handler address (thumb) and r1 = state index. The table currently
 * has a single entry, sub_81197B4+1. When the handler returns, its
 * results (registers and flags) go straight back to our caller, so this
 * behaves as a tail call. */
Next npc_dispatchStateHandler_811979C(void) {
    u32 state = LD8(R5 + 9);

    R0 = LD32(A_off_81197B0 + state);   /* handler address from the jump table */
    R1 = state;                         /* index passed along to the handler */

    GUEST_CALL(sub_81197B4);
    RETURN_TO_CALLER();
}

/* ── nullStub_8119800 ── */
/* nullStub_8119800: plain return (push {lr}; pop {pc} — equivalent to bx lr,
 * leaves all registers untouched). */
Next nullStub_8119800(void) {
    RETURN_TO_CALLER();
}

/* ── setFieldsInThreeTableEntries_811980C ── */
/* setFieldsInThreeTableEntries_811980C(r0 = halfword value for struct+0x0E, r1 = byte value for struct+0x0C,
 *             r2 = first table index, r3 = halfword value for struct+0x18):
 * For the three table entries starting at index r2, look up the matching
 * struct in the table rooted at 0x2008450 (getStructFrom2008450 returns a
 * status in r0 and the struct pointer in r1). If an entry exists, store the
 * three given values into it (halfword +0x0E, byte +0x0C, halfword +0x18).
 * r4-r7 are callee-saved in the original (push/pop), so they are left
 * untouched; r0/r1 hold the last loop iteration's results, and the flags
 * come from the final loop counter compare (always equal on exit). */
Next setFieldsInThreeTableEntries_811980C(void) {
    u32 halfwordE  = R0;   /* value stored at struct+0x0E */
    u32 byteC      = R1;   /* value stored at struct+0x0C */
    u32 first      = R2;   /* first of the three entries to fill in */
    u32 halfword18 = R3;   /* value stored at struct+0x18 */

    u32 index = first;
    u32 end   = first + 3;

    for (; (s32)index < (s32)end; index++) {
        R0 = index;
        GUEST_CALL(getStructFrom2008450);   /* -> r0 = status, r1 = struct */
        if (R0 != 0) {
            ST16(R1 + 0xE, halfwordE);
            ST8(R1 + 0xC, byteC);
            ST16(R1 + 0x18, halfword18);
            R0 = halfword18;    /* matches the original's last `ldr r0, [sp,#0xC]` */
        }
        /* r0/r1 keep the last call's values when the entry was empty */
    }

    SET_FLAGS_CMP(index, end);
    RETURN_TO_CALLER();
}

/* ── sprite_setPositionForIdRange3_8119854 ── */
/* sprite_setPositionForIdRange3_8119854(r0 = x, r1 = y, r2 = first sprite id):
 * Position the group of three sprites whose ids run from r2 to r2+2 at
 * the coordinates (x, y). Each id is looked up in the sprite table rooted
 * at 0x2008450; ids with no entry there are skipped. The lookup's r1
 * result is handed to sprite_setCoordinates in r5, and its r2/r3 results
 * are forwarded to that call unchanged. On return the flags are those of
 * the loop's final `cmp id, end` (i.e. the loop counter against its limit). */
Next sprite_setPositionForIdRange3_8119854(void) {
    u32 x = R0;
    u32 y = R1;
    u32 firstId = R2;
    u32 savedR5 = R5;   /* r5 is borrowed as an argument below, then restored */

    u32 end = firstId + 3;
    u32 id = firstId;
    while ((s32)id < (s32)end) {
        R0 = id;
        GUEST_CALL(getStructFrom2008450);
        if (R0 != 0) {              /* sprite exists: move it */
            R5 = R1;                /* lookup's second result -> r5 argument */
            R0 = x;
            R1 = y;
            /* r2/r3 still hold the lookup's results and are passed on as-is */
            GUEST_CALL(sprite_setCoordinates);
        }
        id++;
    }
    SET_FLAGS_CMP(id, end);

    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sub_8119ACC ── */
/* sub_8119ACC: indirect call through the three-entry jump table at
 * off_8119AE0. The byte at [r5+8] is used as a *byte offset* into the
 * table (values 0, 4, 8 select the three handlers), so it is added to
 * the table base before the word load, not used as an entry index.
 * The handler is entered exactly as `bx r0` in the original leaves the
 * registers: R0 = the table word (the handler's own Thumb address),
 * R1 = the selector byte, R2-R12 and the flags untouched. When the
 * handler returns, the original executes `pop {pc}` and returns to this
 * routine's caller, so the caller sees whatever the handler left in
 * R0-R3 and the flags. */
Next sub_8119ACC(void) {
    u32 selector = LD8(R5 + 8);

    R0 = LD32(A_off_8119AE0 + selector); /* handler address, as `bx r0` sees it */
    R1 = selector;

    if (selector == 0)
        GUEST_CALL(sub_8119AEC);
    else if (selector == 4)
        GUEST_CALL(sprite_applyComputedPaletteAndUpdate_8119BD0);
    else if (selector == 8)
        GUEST_CALL(sub_8119BE0);
    /* any other selector would branch to garbage past the table; the
     * game never produces one, so there is nothing to do here */

    RETURN_TO_CALLER();
}

/* ── sprite_applyComputedPaletteAndUpdate_8119BD0 ── */
/* sprite_applyComputedPaletteAndUpdate_8119BD0: refresh one sprite's palette and push the update through.
 * Calls sub_8119C08 (which computes the palette index into r0), applies it
 * with sprite_setPalette, then runs sprite_update. */
Next sprite_applyComputedPaletteAndUpdate_8119BD0(void) {
    GUEST_CALL(sub_8119C08);
    /* sub_8119C08 leaves the palette index in r0 for sprite_setPalette */
    GUEST_CALL(sprite_setPalette);
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── object_setSpritePosition_8119BE8 ── */
/* object_setSpritePosition_8119BE8(r0 = x, r1 = y, r2 = object index):
 * Look up the object's struct in the table rooted at 0x2008450; if the
 * lookup succeeds, move its sprite to (x, y). The lookup also yields a
 * value in r1 which is handed to sprite_setCoordinates via r5.
 * r4-r7 (and r3) are preserved for the caller. */
Next object_setSpritePosition_8119BE8(void) {
    u32 x = R0;
    u32 y = R1;

    /* r0 = struct lookup for object r2; the callee's null check gives
     * the Z flag the original branches on. */
    R0 = R2;
    GUEST_CALL(getStructFrom2008450); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
        u32 sprite = R1;   /* sprite/bank id returned by the lookup */
        u32 savedR5 = R5;  /* sprite_setCoordinates clobbers r5 */

        R5 = sprite;
        R0 = x;
        R1 = y;
        GUEST_CALL(sprite_setCoordinates);

        R5 = savedR5;      /* original restores the caller's r5 via pop */
    }

    RETURN_TO_CALLER();
}

/* ── pet_GetNaviStatsBaseOrEmpty_8119C58 ── */
/* pet_GetNaviStatsBaseOrEmpty_8119C58(r0 = base pointer):
 * Ask which navi is currently in the PET, then read its stats byte at
 * offset 0xE. If that byte is 0xFF (no navi selected), return base+4,
 * otherwise return base unchanged. r1 always comes back as 0xE (the
 * stats offset, preserved through the helper call). r7 is untouched. */
Next pet_GetNaviStatsBaseOrEmpty_8119C58(void) {
    u32 base = R0;

    GUEST_CALL(GetCurPETNavi);            /* -> navi id in R0 */

    R1 = 0xE;
    GUEST_CALL(GetCurPETNaviStatsByte);   /* (navi id, 0xE) -> stats byte in R0 */

    R0 = ((u8)R0 == 0xFF) ? base + 4 : base;
    R1 = 0xE;
    RETURN_TO_CALLER();
}

/* ── registry_setEntryStatusByKey_8119C74 ── */
/* registry_setEntryStatusByKey_8119C74(r0 = flag, r1 = key):
 * Look up the registry entry belonging to the key in r1 via
 * getStructFrom2008450. That call returns non-zero in r0 when an entry
 * was found, and leaves the address of the entry in r1 (it clobbers the
 * incoming r1). When found, mark the entry's status byte: 0x23 when the
 * flag argument is set, otherwise 1. Do nothing when no entry matches. */
Next registry_setEntryStatusByKey_8119C74(void) {
    u32 flag = R0;   /* callee clobbers r0, so keep the flag aside */
    u32 key = R1;

    R0 = key;
    GUEST_CALL(getStructFrom2008450); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
        u32 entry = R1;   /* r1 now points at the found registry entry */
        ST8(entry, (flag != 0) ? 0x23 : 1);
    }

    RETURN_TO_CALLER();
}

/* ── entity_RunStateHandler_8119E40 ── */
/* entity_RunStateHandler_8119E40: jump-table dispatcher for the state handler at r5.
 *
 * The byte at [r5+8] selects one of three handlers. It is used as a *byte
 * offset* into the table at off_8119E54 (whose entries sit at +0, +4 and
 * +8), so the game stores 0, 4 or 8 in it -- not a 0/1/2 index.
 *
 * The call is a tail call: the original sets LR to the `pop {pc}` that
 * follows, so the handler returns straight back to this function's caller.
 * The handler receives r0 = its own table entry (Thumb address, exactly as
 * loaded from the table), r1 = the selector byte, and r2-r12 untouched.
 * Registers and flags after the call are passed through. */
Next entity_RunStateHandler_8119E40(void) {
    u32 sel = LD8(R5 + 8);
    u32 handler = LD32(A_off_8119E54 + sel); /* off_8119E54: sub_8119E60+1, object_runTypeUpdateThenSpriteUpdate_811A0A4+1, sub_811A1EC+1 */

    R0 = handler;
    R1 = sel;

    switch (sel) {
    case 0: GUEST_CALL(sub_8119E60); break;
    case 4: GUEST_CALL(object_runTypeUpdateThenSpriteUpdate_811A0A4); break;
    case 8: GUEST_CALL(sub_811A1EC); break;
    /* any other byte offset would make the original jump to garbage; it
     * never occurs in practice, so there is nothing to do */
    }

    RETURN_TO_CALLER();
}

/* ── object_runTypeUpdateThenSpriteUpdate_811A0A4 ── */
/* object_runTypeUpdateThenSpriteUpdate_811A0A4: per-frame update for the battle object whose struct is in R5.
 * The type byte at [R5+4] selects one of four update routines from the jump
 * table at off_811A0BC (entries 0 and 3 share a handler). The selected
 * routine is called through the table — so it is entered with R0 = table
 * entry and R1 = shifted index, exactly as the original's "ldr/bx" leaves
 * them — and afterwards the object's sprite state is advanced by
 * sprite_update. The original wraps this in push {lr}/pop {pc}, so it
 * returns to the return address it was entered with, not to whatever LR the
 * last call left behind. */
Next object_runTypeUpdateThenSpriteUpdate_811A0A4(void) {
    u32 returnAddress = LR; /* push {lr} */

    u32 type = LD8(R5 + 4);
    u32 index = type << 2;

    /* The original's LSL leaves N/Z from the shifted index and C clear
     * (no bit is shifted out of a byte value). */
    SET_FLAGS_NZ(index);
    cpu.c = 0;

    /* ldr r0, [table, index]; mov lr, pc; bx r0 */
    u32 handler = LD32(A_off_811A0BC + index);
    R0 = handler;
    R1 = index;
    LR = 0x0811A0B2; /* return point: the sprite_update call below */
    dispatch(handler);

    /* bl sprite_update */
    GUEST_CALL(sprite_update);

    LR = returnAddress; /* pop {pc} */
    RETURN_TO_CALLER();
}

/* ── battle_initScalableSpriteObject_811A11C ── */
/* battle_initScalableSpriteObject_811A11C(r5 = battle object):
 * Give the object's sprite (number 0x90) scaling support, program its
 * initial scale parameters (0x90 source height, 0x40 dest height) and set
 * the object's state byte at +9 to 8 (scalable-sprite mode). */
Next battle_initScalableSpriteObject_811A11C(void) {
    u32 obj = R5;

    R0 = 0x90;
    ST8(obj + 0x12, 0x90);              /* sprite id 0x90 */
    GUEST_CALL(sprite_makeScalable);
    obj = R5;                            /* callee may clobber r5 */

    R0 = 0;
    R1 = 0x90;
    R2 = 0x40;
    GUEST_CALL(sprite_setScaleParameters); /* leaf call: preserves r5 */

    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(obj + 9, 8);                     /* object state: scalable */
    RETURN_TO_CALLER();
}

/* ── npc_ClearBusyFlagAndResetState_811A138 ── */
/* npc_ClearBusyFlagAndResetState_811A138(r5 = state object):
 * Clear the bit-1 flag in the object's status byte and set its byte at
 * offset 9 to 0xC (a state/timer reset for this object).
 * Leaves r0 = 0xC and r1 = 2, with NZ flags set as by "mov r0, #0xC". */
Next npc_ClearBusyFlagAndResetState_811A138(void) {
    ST8(R5, LD8(R5) & ~2u); /* status byte: clear bit 1 */
    R1 = 2;
    R0 = 0xC;
    ST8(R5 + 9, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── stateObj_advanceToState0x10_811A148 ── */
/* stateObj_advanceToState0x10_811A148(r5 = state object pointer):
 * If the byte at r5+4 is still zero, run its initialization (sub_8119EEC).
 * Then mark the object with state 0x10 at r5+9 and advance it via sprite_scaleShrinkStep_811A160.
 * r5 is preserved; r0 on return is whatever sprite_scaleShrinkStep_811A160 left there. */
Next stateObj_advanceToState0x10_811A148(void) {
    if (LD8(R5 + 4) == 0)
        GUEST_CALL(sub_8119EEC);

    ST8(R5 + 9, 0x10);

    GUEST_CALL(sprite_scaleShrinkStep_811A160);
    RETURN_TO_CALLER();
}

/* ── sprite_scaleShrinkStep_811A160 ── */
/* sprite_scaleShrinkStep_811A160(r5 = sprite/effect object):
 * Shrink the object's scale value (field 0x12) by 0x20 and mark it (field 0,
 * bit 1) as being scaled. If the scale is still above 0x40, hand the new scale
 * to sprite_setScaleParameters (base 0, new scale, target 0x40). Once it drops
 * to 0x40 or below, snap the scale back to 0xC0, clear field 9, and make the
 * sprite unscalable — the shrink animation has finished its cycle.
 * Leaves r4-r7 untouched; r0-r3 and the flags are whatever the called sprite
 * routine leaves behind. */
Next sprite_scaleShrinkStep_811A160(void) {
    u32 scale;

    ST8(R5, LD8(R5) | 2);

    scale = LD8(R5 + 0x12) - 0x20;
    ST8(R5 + 0x12, scale);

    if ((s32)scale > 0x40) {
        R0 = 0;
        R1 = scale;
        R2 = 0x40;
        GUEST_CALL(sprite_setScaleParameters);
    } else {
        ST8(R5 + 0x12, 0xC0);
        ST8(R5 + 9, 0);
        R0 = 0;
        GUEST_CALL(sprite_makeUnscalable);
    }

    RETURN_TO_CALLER();
}

/* ── object_runUpdateDispatch_811A190 ── */
/* object_runUpdateDispatch_811A190: per-object update entry point.
 * r5 = object. The byte at offset 9 in the object selects its update
 * handler from the handler table at off_811A1AC and that handler runs
 * first (the original leaves the table entry itself in r0 and the type
 * byte in r1 when it jumps to it). Afterwards two shared update passes,
 * sub_811A29C and sub_811A2EC, run; r0/r1 flow out of the last one.
 * The table currently contains a single entry, sub_811A1B0. */
Next object_runUpdateDispatch_811A190(void) {
    u32 type = LD8(R5 + 9);

    /* Call the object's own handler. */
    R0 = LD32(A_off_811A1AC + type);
    R1 = type;
    GUEST_CALL(sub_811A1B0);

    /* Shared update passes. */
    GUEST_CALL(sub_811A29C);
    GUEST_CALL(sub_811A2EC);

    RETURN_TO_CALLER();
}

/* ── sub_811A1E8 ── */
/* sub_811A1E8: empty stub — immediately returns to the caller without
 * touching any registers or memory (the original is just `push {lr}` /
 * `pop {pc}`). Kept as a symbol because the game calls it as a hook. */
Next sub_811A1E8(void) {
    RETURN_TO_CALLER();
}

/* ── setSlotFieldsFromStartTo3_811A1F4 ── */
/* setSlotFieldsFromStartTo3_811A1F4(r0 = halfword value, r1 = byte value, r2 = first slot, r3 = halfword value):
 * Walk the entity slots starting at r2, up to but not including slot 4, and for
 * every slot that currently has a struct registered, refresh three of its fields:
 *   +0x0C (byte)  = r1
 *   +0x0E (half)  = r0
 *   +0x18 (half)  = r3
 * getStructFrom2008450 signals "no struct for this slot" by returning r0 = 0
 * (that is what the original's beq tests); the struct address to write to comes
 * back in r1 and may hold junk when the slot is empty, so only r0 may be tested.
 * Although the original moves the loop counter into r4, it saves and restores
 * r4 around the routine (push {r4,lr} / pop {r4,pc}), so r4 is left unchanged. */
Next setSlotFieldsFromStartTo3_811A1F4(void) {
    u16 val0E = (u16)R0;
    u8  val0C = (u8)R1;
    u16 val18 = (u16)R3;

    u32 slot = R2;
    do {
        R0 = slot;
        GUEST_CALL(getStructFrom2008450); /* answers in Z; the original branches on it */
        if (!ZERO_FLAG()) {
            ST16(R1 + 0xE, val0E);
            ST8(R1 + 0xC, val0C);
            ST16(R1 + 0x18, val18);
        }
        slot++;
    } while ((s32)slot < 4);

    RETURN_TO_CALLER();
}

/* ── sprite_moveSlotGroupXY_811A220 ── */
/* sprite_moveSlotGroupXY_811A220(r0 = x, r1 = y, r2 = first slot index):
 * For the four object slots starting at the index in r2, look each entry up
 * in the table at 0x2008450 and, if an object exists there, move its sprite
 * to (x, y).
 *
 * Flags on return come from the final slot/index comparison (callers test V).
 * r2/r3 are whatever the last callee left them as; r4-r7 are preserved. */
Next sprite_moveSlotGroupXY_811A220(void) {
    u32 x = R0;
    u32 y = R1;
    u32 slot = R2;
    u32 slotEnd = slot + 4;
    u32 entryR5 = R5; /* clobbered as a call argument below, restored on exit */

    for (;;) {
        R0 = slot;
        GUEST_CALL(getStructFrom2008450); /* answers in Z; the original branches on it */

        if (!ZERO_FLAG()) {
            /* sprite_setCoordinates takes the object's sprite handle in r5 */
            R5 = R1;
            R0 = x;
            R1 = y;
            GUEST_CALL(sprite_setCoordinates);
        }

        slot++;
        SET_FLAGS_CMP(slot, slotEnd);
        if ((s32)slot >= (s32)slotEnd)
            break;
    }

    R5 = entryR5;
    RETURN_TO_CALLER();
}

/* ── submenu_compareCursorBytes_811A24C ── */
/* submenu_compareCursorBytes_811A24C(r5 = menu object):
 * Dispatcher helper for the submenu: reads the object's state byte and,
 * unless it is in state 2 or 3, loads the submenu struct and compares its
 * two cursor bytes (unk_0E vs unk_0F) so the caller can branch on the
 * result. R6 ends up holding the sSubmenu address in that case; in states
 * 2/3 R6 is left untouched and the flags come from the matching CMP. */
Next submenu_compareCursorBytes_811A24C(void) {
    u32 state = LD8(R5 + 5);

    if (state == 2) {
        /* cmp r0, #2 taken as equal */
        SET_FLAGS_CMP(state, 2);
        RETURN_TO_CALLER();
    }
    if (state == 3) {
        /* cmp r0, #3 taken as equal */
        SET_FLAGS_CMP(state, 3);
        RETURN_TO_CALLER();
    }

    R6 = A_sSubmenu;
    /* cmp unk_0E, unk_0F: set flags for the caller's follow-up branch */
    SET_FLAGS_CMP(LD8(R6 + 0xE), LD8(R6 + 0xF)); /* sSubmenu.unk_0E vs .unk_0F */
    RETURN_TO_CALLER();
}

/* ── menu_getTableEntryForStateIndex_811A268 ── */
/* menu_getTableEntryForStateIndex_811A268:
 * Fetch one entry from the table at dword_201FC20 (the table base IS the RAM
 * address dword_201FC20; the original computes the entry pointer by adding the
 * index directly to it). The entry index is the sum of the two halfwords at
 * offsets 0x40 and 0x3C of the active menu state block: sSubmenu while its
 * first byte equals 8, otherwise unk_2037780. Each entry is four bytes wide.
 *
 * Outputs: r0 = entry halfword at +0, r1 = entry byte at +2, r2 = entry byte
 *          at +3, r3 = pointer to the entry.
 * Flags:   nz set from r0 (as the original's tst r0, r0 does).
 * Preserved: r4-r7. */
Next menu_getTableEntryForStateIndex_811A268(void) {
    u32 state = A_sSubmenu;
    if (LD8(state) != 8)
        state = A_unk_2037780;

    u32 index = LD16(state + 0x40) + LD16(state + 0x3C);
    u32 entry = A_dword_201FC20 + (index << 2);

    R0 = LD16(entry);
    R1 = LD8(entry + 2);
    R2 = LD8(entry + 3);
    R3 = entry;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_runObjectStateHandler_811B1EC ── */
/* battle_runObjectStateHandler_811B1EC: run the current state handler of the battle object in r5.
 * The state id is stored at r5+8 and is used directly as a byte offset into
 * a three-entry jump table (states 0, 4 and 8). The handler is entered with
 * r0 = its own address and r1 = the state id; every other register passes
 * through untouched, and the callee's registers and flags are handed back
 * to our caller (the original tails into the callee via bx/pop {pc}). */
Next battle_runObjectStateHandler_811B1EC(void) {
    u32 state = LD8(R5 + 8);

    R1 = state;
    R0 = LD32(A_off_811B200 + state); /* handler address; table is byte-indexed */

    switch (state) {
    case 0:
        GUEST_CALL(sub_811B20C);
        break;
    case 4:
        GUEST_CALL(sprite_objectUpdateAnimation_811B284);
        break;
    case 8:
        GUEST_CALL(sub_811B290);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_811B20C ── */
/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void init_gui_sprite_sub_811B20C(u32 gfxId, u32 resourceAddr) {
    R0 = gfxId;
    R1 = resourceAddr;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_set_animation_sub_811B20C(u32 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_load_animation_data_sub_811B20C(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_no_shadow_sub_811B20C(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void sprite_set_palette_sub_811B20C(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void sprite_set_priority_sub_811B20C(u32 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void slot_list_store_halfword_and_clear_field_sub_811B20C(u32 value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void sprite_set_sub_entry_palette_bank_sub_811B20C(u32 bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void sprite_set_sub_record_field_a_sub_811B20C(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sub_811B298 outside this file, which has no typed C interface yet */
static void call_sub_811B298(void) {
    GUEST_CALL(sub_811B298);
}

/* register form: calls sprite_objectUpdateAnimation_811B284 outside this file, which has no typed C interface yet */
static void sprite_object_update_animation(void) {
    GUEST_CALL(sprite_objectUpdateAnimation_811B284);
}

/* Initialise the sprite of a GUI object: mark it state 0x23, bind the GUI
 * sprite resource selected by the object's kind byte, pick the animation,
 * apply the standard palette/priority/slot settings, then run the first
 * animation update. Leaves r0-r3 holding whatever the last callee left. */
static void setup_object_sprite(u32 obj) {
    ST8(obj, 0x23);                                  /* obj->state = 0x23 */

    u32 kind = LD8(obj + 4);
    init_gui_sprite_sub_811B20C(9, LD32(A_off_811B278 + kind * 4)); /* unk_2020484 / unk_202458C */

    /* Animation: 0 when the kind byte is clear; otherwise 2 once byte_203778C
     * is set, else 1. The kind byte is re-read after the sprite init call,
     * exactly as the original does. */
    kind = LD8(obj + 4);
    u32 anim;
    if (kind == 0) {
        anim = 0;
    } else if (LD8(A_unk_2037780 + 0xC) != 0) {      /* byte_203778C */
        anim = 2;
    } else {
        anim = 1;
    }

    sprite_set_animation_sub_811B20C(anim);
    sprite_load_animation_data_sub_811B20C();
    sprite_no_shadow_sub_811B20C();
    sprite_set_palette_sub_811B20C(0);
    sprite_set_priority_sub_811B20C(2);
    slot_list_store_halfword_and_clear_field_sub_811B20C(0x130);
    sprite_set_sub_entry_palette_bank_sub_811B20C(2);
    sprite_set_sub_record_field_a_sub_811B20C(5);
    call_sub_811B298();
    ST8(obj + 8, 4);                                 /* obj->field08 = 4 */
    ST8(obj + 0xC, 0);                               /* obj->field0C = 0 */
    sprite_object_update_animation();
}

/* sub_811B20C(r4, r5 = object, r6, r7, lr): no result of its own; flags and
 * r0-r3 are left exactly as the final callee leaves them. */
Next sub_811B20C(void) {
    setup_object_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_objectUpdateAnimation_811B284 ── */
/* sprite_objectUpdateAnimation_811B284: update step for an animated object — first runs the
 * object's own per-frame handler (sub_811B298), then advances its
 * sprite animation via sprite_update. */
Next sprite_objectUpdateAnimation_811B284(void) {
    GUEST_CALL(sub_811B298);
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_811B298 ── */
/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_set_animation(u8 mode) {
    R0 = mode;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_load_animation_data(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_no_shadow(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void call_set_coordinates(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

/* Update a sprite's animation and position.
 * sprite_ptr: address of the sprite object (R5 from caller).
 *   offset 4  – sprite type (index into per-type data/coordinate tables)
 *   offset 0xC – current animation index
 * Returns the coordinate-table base for this type (left in R2 for the caller). */
static u32 sprite_update_animation(u32 sprite_ptr) {
    u8 type = (u8)LD8(sprite_ptr + 4);
    u32 data_ptr = LD32(A_off_811B2E4 + type * 4);

    /* Types other than the default may need their animation swapped */
    if (type != 0) {
        u8 target = (u8)LD8(data_ptr + 0xC);
        if ((u8)LD8(sprite_ptr + 0xC) != target) {
            ST8(sprite_ptr + 0xC, target);
            /* Mode 2 = normal switch; mode 1 = reset (used when target index is 0) */
            call_set_animation(target == 0 ? 1 : 2);
            call_load_animation_data();
            call_no_shadow();
        }
    }

    /* Pick the x-coordinate from the type's table, indexed by the data struct's animation */
    u32 coord_table = LD32(A_off_811B2F0 + type * 4);
    u32 x = LD32(coord_table + (u8)LD8(data_ptr + 0xC) * 4);
    call_set_coordinates(x, 0x20);

    return coord_table;
}

/* sub_811B298(r5 = sprite ptr) -> r2 = coord table base */
Next sub_811B298(void) {
    R2 = sprite_update_animation(R5);
    RETURN_TO_CALLER();
}

/* ── sub_811B414 ── */
/* Register-form helpers for external game functions called during GUI sprite setup. */

/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void call_initGuiSprite_sub_811B414(u32 sprite_id, u32 a2) {
    R0 = sprite_id;
    R1 = a2;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sub_80101F8 outside this file, which has no typed C interface yet */
static u32 call_getAnimationId(void) {
    GUEST_CALL(sub_80101F8);
    return R0;
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_setAnimation_sub_811B414(u32 anim_id) {
    R0 = anim_id;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_loadAnimationData_sub_811B414(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_noShadow_sub_811B414(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_setPalette_sub_811B414(u32 pal) {
    R0 = pal;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_setPriority_sub_811B414(u32 pri) {
    R0 = pri;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void call_slotList_storeHalfword_sub_811B414(u32 value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void call_setSubEntryPaletteBank(u32 bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void call_setSubRecordFieldA_sub_811B414(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_updateWrapper_811B45C outside this file, which has no typed C interface yet */
static void call_updateWrapper(void) {
    GUEST_CALL(sprite_updateWrapper_811B45C);
}

/* Initialize GUI sprite #0x2c (Navi panel element):
 * set active flag, create sprite, load animation, configure palette/priority,
 * write slot-list halfword, set sub-entry attributes, then update the wrapper. */
static void init_guisprite_2c(u32 gui_arg, u32 state_ptr) {
    /* Mark the slot active */
    ST8(state_ptr, 1);

    /* Create and configure the sprite */
    call_initGuiSprite_sub_811B414(0x2c, gui_arg);
    u32 anim = call_getAnimationId();
    call_setAnimation_sub_811B414(anim);
    call_loadAnimationData_sub_811B414();
    call_noShadow_sub_811B414();
    call_setPalette_sub_811B414(0);
    call_setPriority_sub_811B414(2);

    /* Store 0xc0 halfword in the OAM slot list */
    call_slotList_storeHalfword_sub_811B414(0xc0);

    /* Sub-entry palette bank = 6, record field = 4 */
    call_setSubEntryPaletteBank(6);
    call_setSubRecordFieldA_sub_811B414(4);

    /* Mark sub-slot type in the state structure */
    ST8(state_ptr + 8, 4);

    /* Final sprite wrapper update (leaves r0 and flags in emulated regs) */
    call_updateWrapper();
}

/* sub_811B414(r1 = gui_arg, r5 = state_ptr, r4/r6 preserved) -> r0 (from wrapper update), nzcv from callee */
Next sub_811B414(void) {
    init_guisprite_2c(R1, R5);
    RETURN_TO_CALLER();
}

/* ── sprite_updateWrapper_811B45C ── */
/* sprite_updateWrapper_811B45C: thin wrapper that simply performs a sprite update with the
 * arguments already set up by the caller; all results (r0-r3 and flags)
 * come straight back from sprite_update. */
Next sprite_updateWrapper_811B45C(void) {
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_811B48C ── */
/* sub_811B48C(r0 = flag, r1 = table id):
 * Look up the entry for id r1 in the table at 0x2008450. If the lookup
 * succeeds, write a byte at the address the helper leaves in r1: 0x23 when
 * the incoming flag r0 was nonzero, otherwise 1. The helper's r0 result and
 * the saved flag are left in r0/r2 for the caller. */
Next sub_811B48C(void) {
    u32 flag = R0;

    R0 = R1;
    GUEST_CALL(getStructFrom2008450); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
        /* the helper leaves the destination byte's address in r1 */
        R2 = flag;
        ST8(R1, flag != 0 ? 0x23 : 1);
    }
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

/* ── sprite_dispatchObjectUpdate_811BDCC ── */
/* sprite_dispatchObjectUpdate_811BDCC — run one object's update step.
 *
 * R5 points to the object being updated. The kind byte at R5+9 selects an
 * update handler from the table at off_811BDE8, which is entered with the
 * handler's own address in r0 and the kind byte in r1. (The built-in table's
 * only entry is sub_811BDEC, i.e. kind 0; any other kind would read garbage
 * from the code that follows the pool, so kind is always 0 here.)
 *
 * After the handler returns, object_checkStatusFlag_811BE90 reports a status in r0 and leaves
 * the zero flag reflecting it: when it reports zero the object's sprite does
 * not need to be advanced and sprite_update is skipped. The flags left by
 * the last called function are passed on to the caller unchanged. */
Next sprite_dispatchObjectUpdate_811BDCC(void) {
    u32 kind = LD8(R5 + 9);

    R0 = LD32(A_off_811BDE8 + kind);   /* handler address from the dispatch table */
    R1 = kind;
    GUEST_CALL(sub_811BDEC);

    GUEST_CALL(object_checkStatusFlag_811BE90);   /* answers in Z; the original branches on it */
    if (ZERO_FLAG())
        RETURN_TO_CALLER();

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_811BDEC ── */
/* sub_811BDEC: thin trampoline — performs sub_811BE9C and returns.
 * All of the real work (and all outputs: r0-r3, flags, lr) belongs to
 * sub_811BE9C; this wrapper exists only so callers can branch through it.
 * r5 is an input to the callee and passes through untouched. */
Next sub_811BDEC(void) {
    GUEST_CALL(sub_811BE9C);
    RETURN_TO_CALLER();
}

/* ── sprite_setPositionByStructIndex_811BDFC ── */
/* sprite_setPositionByStructIndex_811BDFC(r0 = x, r1 = y, r2 = index into the struct table at 0x2008450):
 * Look up the entry for this index; if one exists, set the sprite's
 * coordinates to (x, y), passing the entry to sprite_setCoordinates in r5.
 * r5 is only borrowed for the call — like r4/r6/r7, it is preserved for
 * the caller (the original keeps it across push/pop). */
Next sprite_setPositionByStructIndex_811BDFC(void) {
    u32 arg0 = R0;
    u32 arg1 = R1;
    u32 savedR5 = R5;

    R0 = R2;
    GUEST_CALL(getStructFrom2008450); /* answers in Z; the original branches on it */

    /* Z set means there is no live entry for this index. Otherwise the
     * entry comes back in R1 and is handed to sprite_setCoordinates in R5. */
    if (!ZERO_FLAG()) {
        R5 = R1;
        R0 = arg0;
        R1 = arg1;
        GUEST_CALL(sprite_setCoordinates);
    }

    /* Equivalent of the original's pop {r4-r7}: the caller's r5 survives
     * both the temporary use above and any clobbering by the callee. */
    R5 = savedR5;

    RETURN_TO_CALLER();
}

/* ── object_checkStatusFlag_811BE90 ── */
/* object_checkStatusFlag_811BE90(r5 = object/state pointer):
 * Check a byte-sized status flag indexed by the value at [r5+0xC] against the
 * table at byte_811BF94; the Z flag reports whether that flag byte is zero.
 * Returns the loaded byte in r0 and the table address in r2. */
Next object_checkStatusFlag_811BE90(void) {
    u32 flag_index = LD32(R5 + 0xC);
    R2 = A_byte_811BF94;
    R0 = LD8(R2 + flag_index);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811C1CC ── */
/* ------------------------------------------------------------------ *
 * Register-form helpers for the sprite/GUI routines this function
 * drives. None of them has a typed C interface yet, so each one just
 * loads the guest argument registers and calls through.
 * ------------------------------------------------------------------ */

/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void init_gui_sprite_sub_811C1CC(u32 spriteId, u32 template) {
    R0 = spriteId;
    R1 = template;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_set_animation_sub_811C1CC(u32 animation) {
    R0 = animation;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_load_animation_data_sub_811C1CC(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_no_shadow_sub_811C1CC(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void sprite_set_palette_sub_811C1CC(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void sprite_set_priority_sub_811C1CC(u32 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void slot_list_store_halfword_and_clear_field_sub_811C1CC(u32 value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void sprite_set_sub_entry_palette_bank_sub_811C1CC(u32 bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void sprite_set_sub_record_field_a_sub_811C1CC(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setPositionById_811C23C outside this file, which has no typed C interface yet */
static void sprite_set_position_by_id(u32 x, u32 y, u32 id) {
    R0 = x;
    R1 = y;
    R2 = id;
    GUEST_CALL(sprite_setPositionById_811C23C);
}

/* register form: calls sprite_updateAnimPassthrough_811C22C outside this file, which has no typed C interface yet */
static void sprite_update_anim_passthrough(void) {
    /* Called last with nothing after it: like the original, this call is
     * what decides the final r0-r3 and the nz/c/v flags the caller sees. */
    GUEST_CALL(sprite_updateAnimPassthrough_811C22C);
}

/* ------------------------------------------------------------------ *
 * Create and configure the on-screen GUI sprite (resource 0x69) for
 * the object at `work`:
 *   - tag the object and remember its position slot (offset 3),
 *   - give it animation 0, no shadow, palette 0, priority 2,
 *   - store 0xF0 (240) through the slot list,
 *   - set sub-entry/sub-record attributes and field at offset 8 to 4,
 *   - place it at (120, 40) using the id from offset 3,
 *   - run one animation passthrough update.
 * r4/r6/r7 are preserved; r0-r3 and the flags end up exactly as
 * sprite_updateAnimPassthrough_811C22C leaves them.
 * ------------------------------------------------------------------ */
static void setup_gui_sprite_object(u32 work) {
    ST8(work, 0x23);                       /* object type / handler id */

    init_gui_sprite_sub_811C1CC(0x69, A_unk_20222A0);  /* spawn the GUI sprite resource */
    sprite_set_animation_sub_811C1CC(0);
    sprite_load_animation_data_sub_811C1CC();
    sprite_no_shadow_sub_811C1CC();
    sprite_set_palette_sub_811C1CC(0);
    sprite_set_priority_sub_811C1CC(2);
    slot_list_store_halfword_and_clear_field_sub_811C1CC(0xF0);
    sprite_set_sub_entry_palette_bank_sub_811C1CC(5);
    sprite_set_sub_record_field_a_sub_811C1CC(3);

    ST8(work + 8, 4);                      /* sub-state for later updates */

    sprite_set_position_by_id(0x78, 0x28, LD8(work + 3));
    sprite_update_anim_passthrough();
}

/* sub_811C1CC(r4, r5 = object work area, r6, r7) -> r0-r3, nz, c, v from
 * the final sprite_updateAnimPassthrough call; r4-r7 preserved. */
Next sub_811C1CC(void) {
    setup_gui_sprite_object(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_updateAnimPassthrough_811C22C ── */
/* sprite_updateAnimPassthrough_811C22C: update the current sprite animation frame (thin wrapper around
 * sprite_update). All arguments are passed through in the registers untouched;
 * results and flags are whatever sprite_update leaves behind. */
Next sprite_updateAnimPassthrough_811C22C(void) {
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sprite_setPositionById_811C23C ── */
/* sprite_setPositionById_811C23C(r0 = coord a, r1 = coord b, r2 = lookup key):
 * Look up the record for r2 in the table behind getStructFrom2008450.
 * If a record was found (r0 != 0), reposition the sprite identified by
 * (r0, r1) using the handle returned in r1 of the lookup. Otherwise
 * nothing happens. r4-r7 are preserved. */
Next sprite_setPositionById_811C23C(void) {
    u32 coordA = R0;
    u32 coordB = R1;
    u32 savedR5 = R5;   /* caller's r5: only used as a call argument below */

    R0 = R2;
    GUEST_CALL(getStructFrom2008450);

    if (R0 != 0) {
        R5 = R1;        /* lookup's second result: handle for sprite_setCoordinates */
        R0 = coordA;
        R1 = coordB;
        GUEST_CALL(sprite_setCoordinates);
        R5 = savedR5;   /* restore the caller's r5 after the helper call */
    }
    RETURN_TO_CALLER();
}
