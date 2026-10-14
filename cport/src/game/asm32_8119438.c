/*
 * asm32_8119438.c: readable functions of bn6f asm32.s, 0x08119438-0x0811CAEC, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── chipPanel_renderChipGfxAndText_8119438 ── */
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

/* chipPanel_renderChipGfxAndText_8119438(r5 = chip_panel_ptr) -> outputs r1 r2 r3 + flags from callees;
 * r4-r7 are preserved (original restores them via push/pop). */
Next chipPanel_renderChipGfxAndText_8119438(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    render_chip_gfx_and_text(R5);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_81195E4 ── */
/* sub_81195E4(r5 = object pointer, lr = return; r0 preserved)
 * Clear a TileObj's two tile blocks in vObjectTiles.
 *
 * A table (off_8119128) of per-object descriptors is indexed by the
 * byte at r5+5. Each descriptor's first word is the slot of a shared
 * "header" tile block, and its word at +8 is the slot of a "body" tile
 * block. Slot offsets within vObjectTiles are (slot << 5). The two
 * regions are zero-filled via ZeroFillByEightWords (0xA00 and 0x780
 * bytes). r0 is preserved across both calls (the assembly pushes/pops
 * it around the BLs). */
Next sub_81195E4(void) {
    u32 entry = R5;
    u32 saved_r0 = R0;

    u32 desc_ptr  = LD32(A_off_8119128 + (LD8(entry + 5) << 2));
    u32 hdr_slot  = LD32(desc_ptr);
    u32 body_slot = LD32(desc_ptr + 8);

    /* Zero the header tile block: (hdr_slot << 5) + vObjectTiles, 0xA00 bytes. */
    R0 = (hdr_slot << 5) + A_vObjectTiles;
    R1 = 0xA00;
    GUEST_CALL(ZeroFillByEightWords);

    /* Zero the body tile block: (body_slot << 5) + vObjectTiles, 0x780 bytes. */
    R0 = (body_slot << 5) + A_vObjectTiles;
    R1 = 0x780;
    GUEST_CALL(ZeroFillByEightWords);

    R0 = saved_r0;
    RETURN_TO_CALLER();
}

/* ── submenu_updatePanelChipInfoIfDirty_81196F0 ── */
/* register form: path-dependent condition flag propagation — gate-pass inherits flags
 * from chipPanel_renderChipGfxAndText_8119438 (opaque callee result) while gate-fail sets them from local CMP;
 * a no-branch thin adapter cannot express both without overwriting callee flags. */
static void sync_battle_state_regs(void) {
    u32 obj = R5;                    /* r5 = battle object pointer (preserved by push/pop) */
    u32 save_r4 = R4, save_r6 = R6;  /* r4-r7 saved/restored by push/pop originally */

    GUEST_CALL(battleObject_checkPwrAtkTargetSwitch_8119880);         /* gate check: Z=1 means "backup stale" */

    if (ZERO_FLAG()) {
        /* Gate-fail: compare primary fields vs their backups. */
        u16 cur_hp  = (u16)LD16(obj + 0x0E);
        u8  cur_id  = (u8)LD8(obj + 0x0C);
        u16 cur_pos = (u16)LD16(obj + 0x18);
        u16 bk_pos  = (u16)LD16(obj + 0x1A);

        R3 = cur_pos;
        R1 = bk_pos;

        SET_FLAGS_CMP(cur_pos, bk_pos);
        if (cur_pos != bk_pos) {
            goto do_writes;
        }

        u16 bk_hp = (u16)LD16(obj + 0x10);
        SET_FLAGS_CMP(cur_hp, bk_hp);
        if (cur_hp != bk_hp) {
            goto do_writes;
        }

        u8 bk_id = (u8)LD8(obj + 0x0D);
        SET_FLAGS_CMP((u32)cur_id, (u32)bk_id);
        if (cur_id == bk_id) {
            R0 = (u32)bk_id;         /* all match: r0 = backup ID, Z=1 flags kept */
            goto restore;
        }

    do_writes:
        ST16(obj + 0x1A, cur_pos);
        ST16(obj + 0x10, cur_hp);
        ST8(obj + 0x0D, cur_id);
        R0 = 4;
        ST8(obj + 0x09, 4);          /* state = 4 signals backup was stale */

    restore:
        R4 = save_r4;
        R5 = obj;
        R6 = save_r6;
        /* r7, r8, r9 never touched by this path; r10-r12 untouched */
        return;
    }

    /* Gate-pass: call frame update, then unconditional primary→backup copy. */
    GUEST_CALL(chipPanel_renderChipGfxAndText_8119438);         /* flags now from callee */

    /* ldrh/strh/ldrb/strb do not modify CPSR — callee flags survive. */
    u16 hp = (u16)LD16(obj + 0x0E);
    ST16(obj + 0x10, hp);
    u8 id = (u8)LD8(obj + 0x0C);
    ST8(obj + 0x0D, id);
    u16 pos = (u16)LD16(obj + 0x18);
    ST16(obj + 0x1A, pos);
    R0 = pos;                        /* r1, r2, r3 and flags remain from chipPanel_renderChipGfxAndText_8119438 */

    R4 = save_r4;
    R5 = obj;
    R6 = save_r6;
}

/* submenu_updatePanelChipInfoIfDirty_81196F0(r5 = battle object) -> r0, r1, r3, flags */
Next submenu_updatePanelChipInfoIfDirty_81196F0(void) {
    sync_battle_state_regs();
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
 * Runs chipPanel_renderChipGfxAndText_8119438's step, marks the object's state byte as 0x10
 * (state done/waiting to advance), then runs sprite_stepZoomShrinkOut_811976C's step.
 * r4-r9 are preserved; r0 and the flags come back from sprite_stepZoomShrinkOut_811976C. */
Next npc_stepChainSetState0x10_811975C(void) {
    GUEST_CALL(chipPanel_renderChipGfxAndText_8119438);

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
 * has a single entry, submenu_updateChipPalette_81197B4+1. When the handler returns, its
 * results (registers and flags) go straight back to our caller, so this
 * behaves as a tail call. */
Next npc_dispatchStateHandler_811979C(void) {
    u32 state = LD8(R5 + 9);

    R0 = LD32(A_off_81197B0 + state);   /* handler address from the jump table */
    R1 = state;                         /* index passed along to the handler */

    GUEST_CALL(submenu_updateChipPalette_81197B4);
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

/* ── battleObject_checkPwrAtkTargetSwitch_8119880 ── */
/* register form: calls submenu_isOpen_81355D8 outside this file, which has no typed C interface yet */
static bool submenu_is_open_sub_8119880(void) {
    GUEST_CALL(submenu_isOpen_81355D8);
    return !ZERO_FLAG();  /* callee sets Z=1 when submenu is NOT open */
}

/* register form: calls sub_81312F4 outside this file, which has no typed C interface yet */
static u32 get_active_submenu(void) {
    GUEST_CALL(sub_81312F4);
    return R0;
}

/*
 * Gate on the incoming Z flag (ldrb does not set flags in Thumb, so the beq
 * at +0x08 tests whatever Z the caller left).  When Z is clear: r6 = &sSubmenu,
 * probe whether a specific submenu is open (sub_81312F4 returns the active one),
 * then compare the two status bytes at +0xE and +0xF via cmp.
 * Returns (field_E - field_F) for SET_FLAGS_NZ; *out_r6 gets the submenu pointer.
 */
static u32 check_submenu_diff(u32 r5, u32 r6_in, bool incoming_z, u32 *out_r6) {
    /* Perform the memory reads the original executes (values go to clobbered r0/r1) */
    u32 idx = LD8(r5 + 5);
    (void)LD8(A_byte_811935C + idx);

    if (incoming_z) {
        /* Early return: r6 passes through, flags pass through (Z=1 ⇒ N=0 ⇒ result 0) */
        *out_r6 = r6_in;
        return 0;
    }

    u32 submenu = A_sSubmenu;
    if (submenu_is_open_sub_8119880()) {
        submenu = get_active_submenu();
    }
    *out_r6 = submenu;

    u32 field_e = LD8(submenu + 0xE);
    u32 field_f = LD8(submenu + 0xF);
    return field_e - field_f;
}

/* battleObject_checkPwrAtkTargetSwitch_8119880(r5 = context ptr, r6 = submenu ptr in/out, incoming nz) -> r6, nz */
Next battleObject_checkPwrAtkTargetSwitch_8119880(void) {
    u32 out_r6;
    u32 result = check_submenu_diff(R5, R6, ZERO_FLAG(), &out_r6);
    R6 = out_r6;
    SET_FLAGS_NZ(result);
    RETURN_TO_CALLER();
}

/* ── submenu_isStateChanged_81198B0 ── */
/* register form: calls submenu_isOpen_81355D8 outside this file, which has no typed C interface yet */
static bool submenu_is_open_sub_81198B0(void) {
    GUEST_CALL(submenu_isOpen_81355D8);
    return !ZERO_FLAG();
}

/* register form: calls sub_81312F4 outside this file, which has no typed C interface yet */
static u32 get_active_submenu_ptr(void) {
    GUEST_CALL(sub_81312F4);
    return R0;
}

/* Comparison result for the submenu state check. */
typedef struct {
    u32 r1_out;     /* value to write into r1 on return */
    u32 left;       /* byte at +0xE of the active submenu */
    u32 right;      /* byte at +0xF of the active submenu */
} SubmenuCheckResult;

/* Check whether the submenu state at [r5+5] is 1..5 (meaning "skip comparison").
 * If not, determine the active submenu pointer (sub_81312F4 if the popup is open,
 * otherwise the global sSubmenu) and compare the byte pair at +0xE / +0xF.
 * r1_in is passed through unchanged on the "skip" path. */
static SubmenuCheckResult check_submenu_state(u32 submenu_ptr, u32 r1_in) {
    SubmenuCheckResult res;
    u32 state = LD8(submenu_ptr + 5);

    /* States 1–5: "always different" sentinel (original: tst 1,1 → Z=0). */
    if (state >= 1 && state <= 5) {
        res.left = 1;
        res.right = 0;
        res.r1_out = r1_in;
        return res;
    }

    /* Pick the submenu structure whose state bytes we compare. */
    u32 target;
    if (submenu_is_open_sub_81198B0())
        target = get_active_submenu_ptr();
    else
        target = A_sSubmenu;

    res.left = LD8(target + 0xE);
    res.right = LD8(target + 0xF);
    res.r1_out = res.right;
    return res;
}

/* submenu_isStateChanged_81198B0(r5 = submenu ptr, lr) → nz, c, v, r1, lr */
Next submenu_isStateChanged_81198B0(void) {
    SubmenuCheckResult res = check_submenu_state(R5, R1);
    R1 = res.r1_out;
    SET_FLAGS_CMP(res.left, res.right);
    RETURN_TO_CALLER();
}

/* ── sprite_setUnk0x2cFromLoader_81198E8 ── */
/* register form: calls sprite_setUnk0x2c outside this file, which has no typed C interface yet */
static void sprite_set_unk0x2c(u32 base, u8 loader_byte) {
    R0 = base;
    R1 = loader_byte;
    GUEST_CALL(sprite_setUnk0x2c);
}

/* Set the sprite's unk0x2c field from the byte at oSprite+0xC.
 * If that byte is 0xFF, the base argument is 0x40000000; otherwise it is 0.
 * The original reads a byte from the game struct via the pointer in r5 (oSprite). */
static void set_sprite_unk0x2c_from_loader(u32 sprite_ptr) {
    u8 loader_byte = LD8(sprite_ptr + 0xC);
    u32 base = (loader_byte == 0xFF) ? 0x40000000u : 0u;
    sprite_set_unk0x2c(base, loader_byte);
}

Next sprite_setUnk0x2cFromLoader_81198E8(void) {
    set_sprite_unk0x2c_from_loader(R5);
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
        GUEST_CALL(buildPetStatusIconSprite_8119AEC);
    else if (selector == 4)
        GUEST_CALL(sprite_applyComputedPaletteAndUpdate_8119BD0);
    else if (selector == 8)
        GUEST_CALL(sub_8119BE0);
    /* any other selector would branch to garbage past the table; the
     * game never produces one, so there is nothing to do here */

    RETURN_TO_CALLER();
}

/* ── buildPetStatusIconSprite_8119AEC ── */
/* buildPetStatusIconSprite_8119AEC: build the PET-status icon sprite.
 *
 * The original takes the object pointer in R5 and runs through a small state
 * machine: pick which sprite form to display, initialise the sprite, load its
 * animation, hide the shadow, lay out companion sprites, then apply palette /
 * priority / sub-record settings. */

/* Register-form helpers for game functions that have no typed C interface yet.
 * Each sets the input registers, performs GUEST_CALL, and returns whatever the
 * original leaves behind (R0, or a condition flag read with ZERO_FLAG()). */

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u8 get_cur_pet_navi(void) {
    GUEST_CALL(GetCurPETNavi);
    return (u8)R0;
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u8 get_cur_pet_navi_stats_byte(u32 a1, u32 a2) {
    R0 = a1;
    R1 = a2;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (u8)R0;
}

/* register form: calls notZero_eByte200AD04 outside this file, which has no typed C interface yet */
static bool byte_200ad04_nonzero(void) {
    GUEST_CALL(notZero_eByte200AD04);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool test_event_flag_163(void) {
    R0 = 1;
    R1 = 0x63;                          /* EVENT_163 */
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls sub_8119C08 outside this file, which has no typed C interface yet.
 * Returns the R0 the callee leaves behind (passed straight to sprite_setPalette). */
static u32 setup_companion_sprites(void) {
    GUEST_CALL(sub_8119C08);
    return R0;
}

/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void init_gui_sprite_sub_8119AEC(s32 a1, s32 a2) {
    R0 = (u32)a1;
    R1 = (u32)a2;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_set_anim_sub_8119AEC(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_load_anim_data_sub_8119AEC(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_no_shadow_sub_8119AEC(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void sprite_set_palette_sub_8119AEC(s32 palette) {
    R0 = (u32)palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void sprite_set_priority_sub_8119AEC(s32 priority) {
    R0 = (u32)priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void slot_store_halfword(u32 value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void sprite_set_sub_palette_bank(s32 bank) {
    R0 = (u32)bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void sprite_set_sub_record_field_a_sub_8119AEC(s32 value) {
    R0 = (u32)value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_applyComputedPaletteAndUpdate_8119BD0 outside this file, which has no typed C interface yet */
static void sprite_apply_palette(void) {
    GUEST_CALL(sprite_applyComputedPaletteAndUpdate_8119BD0);
}

/* Static table mirroring the 32-bit words at byte_8119B74 in the original ROM.
 * Each entry is a sprite-form ID; index by the chosen form index. */
static const u32 kSpriteFormTable[] = {
    0x1B, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    0x46, 0x47, 0x48, 0x62, 0x5F, 0x54, 0x55, 0x56,
    0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D
};

/* Pick the sprite-form ID the icon should display.
 *
 *  - If a real PET Navi is currently out, use its ID directly as the table index.
 *  - Otherwise read stats byte 0x17. When the "style" flag at 0x200AD04 is set
 *    AND event 163 is clear, offset the index by 0x0C; in every other case the
 *    base index is 0. */
static u32 pick_sprite_form_id(void) {
    u8 navi = get_cur_pet_navi();
    if (navi != 0) {
        return kSpriteFormTable[navi];
    }

    u8 stats = get_cur_pet_navi_stats_byte(0, 0x17);
    u32 index;
    if (byte_200ad04_nonzero() && !test_event_flag_163()) {
        index = 0x0Cu + stats;
    } else {
        index = 0;
    }
    return kSpriteFormTable[index];
}

/* Build the PET-status icon sprite at the object pointed to by `obj` (R5 in
 * the original). Sets up the chosen sprite form, animation, palette and
 * priority, then applies the computed palette. */
static void build_pet_status_icon(u32 obj) {
    u32 form_id = pick_sprite_form_id();

    /* The original stores 0x23 to [R5] before any sprite work, tagging the
     * object so the sprite system knows what it is dealing with. */
    ST8(obj, 0x23);

    init_gui_sprite_sub_8119AEC((s32)form_id, 0);
    sprite_set_anim_sub_8119AEC(0);
    sprite_load_anim_data_sub_8119AEC();
    sprite_no_shadow_sub_8119AEC();
    /* sub_8119C08 returns an R0 that the original passes unchanged to
     * sprite_setPalette. */
    u32 palette = setup_companion_sprites();
    sprite_set_palette_sub_8119AEC((s32)palette);
    sprite_set_priority_sub_8119AEC(2);
    slot_store_halfword(0);              /* dword_8119B6C == 0 */
    sprite_set_sub_palette_bank(0);
    sprite_set_sub_record_field_a_sub_8119AEC(3);
    ST8(obj + 8, 4);
    sprite_apply_palette();
}

Next buildPetStatusIconSprite_8119AEC(void) {
    build_pet_status_icon(R5);
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

/* ── sub_8119C94 ── */
/* sub_8119C94: pick one of the (x, y) offset pairs stored in the table at
 * byte_8119CCC based on the navi currently equipped in the PET, and return
 * the pair in r0 (x) / r1 (y).
 *
 * Entry selection:
 *   - navi 0: entry index = its stats byte 0x17, plus 0x17 more while the
 *     EVENT_163 flag is set (two different "forms" of that navi).
 *   - any other navi n: entry index = 0xB + n.
 * Each table entry is two s16 values (4 bytes). */
Next sub_8119C94(void) {
    s32 entry_index;

    GUEST_CALL(GetCurPETNavi);
    u32 navi_id = R0;

    if (navi_id != 0) {
        entry_index = 0xB + navi_id;
    } else {
        /* Navi 0: read stats byte 0x17 (its form/counter), then shift to
         * the second set of entries when EVENT_163 is set. */
        R0 = 0;
        R1 = 0x17;
        GUEST_CALL(GetCurPETNaviStatsByte);
        entry_index = R0;

        R0 = 1;
        R1 = EVENT_163;
        GUEST_CALL(TestEventFlagFromImmediate);
        if (!ZERO_FLAG())
            entry_index += 0x17;
    }

    u32 entry_addr = A_byte_8119CCC + entry_index * 4;
    R0 = LDS16(entry_addr);
    R1 = LDS16(entry_addr + 2);

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

/* ── sub_8119F88 ── */
/* sub_8119F88(r5 = object):
 * Wipe the object's graphics out of object VRAM. The object's resource entry
 * (selected by the id byte at [r5+5]) holds the starting tile numbers of its
 * two VRAM blocks; each block is zero-filled so the tiles can be reused.
 * Block 1 starts at tile [entry+0] and is 0xA00 bytes (80 tiles) long,
 * block 2 starts at tile [entry+8] and is 0x780 bytes (60 tiles) long.
 * Preserves r0 and r3 for the caller. */
Next sub_8119F88(void) {
    u32 savedR0 = R0;

    u32 entry = LD32(A_off_8119FD0 + (LD8(R5 + 5) << 2));

    /* Zero the first VRAM tile block. */
    R0 = A_vObjectTiles + (LD32(entry) << 5);
    R1 = 0xA00;
    GUEST_CALL(ZeroFillByEightWords);

    /* Zero the second VRAM tile block. */
    R0 = A_vObjectTiles + (LD32(entry + 8) << 5);
    R1 = 0x780;
    GUEST_CALL(ZeroFillByEightWords);

    R0 = savedR0;
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

/* ── stateObj_dispatchByStateByte_811A0CC ── */
/* Jump table base at 0x0811A0E0; entries are 4-byte-aligned, so the state byte
 * at object offset 9 is pre-multiplied (0, 4, 8, 12, 16). */

/* register form: dispatches to the handler chosen by the state byte at obj+9.
 * The target is one of five game functions selected at runtime from a jump table;
 * none has a typed C interface. All registers and flags pass through to/from the
 * callee unchanged. */
static void dispatch_state_handler(u32 obj) {
    switch (LD8(obj + 9)) {
    case 0:  GUEST_CALL(sub_811A0F4);                              break;
    case 4:  GUEST_CALL(battle_initScalableSpriteObject_811A11C);   break;
    case 8:  GUEST_CALL(npc_ClearBusyFlagAndResetState_811A138);    break;
    case 12: GUEST_CALL(stateObj_advanceToState0x10_811A148);       break;
    case 16: GUEST_CALL(sprite_scaleShrinkStep_811A160);            break;
    }
}

/* stateObj_dispatchByStateByte_811A0CC(r5 = state-object ptr) -> everything the chosen handler returns */
Next stateObj_dispatchByStateByte_811A0CC(void) {
    dispatch_state_handler(R5);
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
 * The table currently contains a single entry, submenu_updateChipPreviewSprite_811A1B0. */
Next object_runUpdateDispatch_811A190(void) {
    u32 type = LD8(R5 + 9);

    /* Call the object's own handler. */
    R0 = LD32(A_off_811A1AC + type);
    R1 = type;
    GUEST_CALL(submenu_updateChipPreviewSprite_811A1B0);

    /* Shared update passes. */
    GUEST_CALL(sub_811A29C);
    GUEST_CALL(sub_811A2EC);

    RETURN_TO_CALLER();
}

/* ── submenu_updateChipPreviewSprite_811A1B0 ── */
#define oCursorByte         0x0C   /* u8  current cursor byte */
#define oCursorByteBak      0x0D   /* u8  backup cursor byte */
#define oChipIndex          0x0E   /* u16 selected chip id */
#define oChipIndexBak       0x10   /* u16 backup chip id */
#define oFlagsPtr           0x14   /* u32 ptr to gate-flags byte */
#define oChipData_Palette   0x07   /* u8  palette field inside ChipData */

/* register form: calls submenu_compareCursorBytes_811A24C outside this file,
 * which has no typed C interface yet. Z is set when the two byte sequences match. */
static bool cursor_bytes_match(void) {
    GUEST_CALL(submenu_compareCursorBytes_811A24C);
    return ZERO_FLAG();
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C
 * interface yet. Preserves r4, r5, r6 across the call. */
static u32 get_chip_ptr(u32 chip_index) {
    R0 = chip_index;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls sprite_setPalette outside this file, which has no typed
 * C interface yet. Preserves r1, r4, r5, r6; clobbers r0, r2, r3, lr.
 * After the call cpu.nz/c/v hold the callee's flags. */
static u32 sprite_set_palette(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
    return R0;
}

/* Result struct for the chip-preview handler. */
struct ChipPreviewResult {
    u32 r0;
    u32 r1;
    u32 r4;
    u32 r6;
    u32 nz_val;   /* passed to SET_FLAGS_NZ in the adapter */
};

/*
 * One per-line chip-preview routine.
 *
 * If the two cursor byte sequences match AND gate-bit-1 of the flags byte
 * is set, we bail early: r0/r1 hold the loaded values, r4/r6 pass through
 * from the callee, and the adapter sets N,Z from the TST result (C,V
 * stay from submenu_compareCursorBytes).
 *
 * Otherwise we load the selected chip index and cursor byte, look up the
 * chip's palette, remap it through the ROM constant table at
 * byte_811A1E0, call sprite_setPalette, and save the current selection
 * as the new backup.
 */
static struct ChipPreviewResult chip_preview(u32 menu, u32 r4_in, u32 r6_in) {
    struct ChipPreviewResult res;
    res.r4 = r4_in;
    res.r6 = r6_in;

    if (cursor_bytes_match()) {
        u32 flags_byte = LD8(LD32(menu + oFlagsPtr));
        u32 tst = flags_byte & 2;
        if (tst != 0) {
            /* Gate bit set – skip preview, return early. */
            res.r0  = flags_byte;
            res.r1  = 2;
            res.nz_val = tst;
            return res;
        }
    }

    /* Normal preview path. */
    u16 chip_idx = LD16(menu + oChipIndex);
    u8  cur_byte = LD8(menu + oCursorByte);
    res.r4 = chip_idx;
    res.r6 = cur_byte;

    u32 chip_ptr = get_chip_ptr(chip_idx);
    u8  raw_pal  = LD8(chip_ptr + oChipData_Palette);
    u8  mapped   = LD8(A_byte_811A1E0 + raw_pal);

    /* Load remap table address into r1 (preserved by sprite_setPalette). */
    res.r1 = A_byte_811A1E0;

    u32 r0 = sprite_set_palette(mapped);
    res.r0     = r0;
    res.nz_val = r0;   /* sprite_setPalette flags approximate sign/zero of r0 */

    /* Stash current values so the next comparison detects changes. */
    ST16(menu + oChipIndexBak, chip_idx);
    ST8(menu + oCursorByteBak, cur_byte);

    return res;
}

/* submenu_updateChipPreviewSprite_811A1B0(r5 = cursor-line struct ptr, lr = return addr)
 * outputs: nz c v r0 r1 r2 r3 r4 r6 lr
 * r2, r3, lr are only ever left behind by callees (GUEST_CALLs);
 * the adapter writes only what chip_preview computes. */
Next submenu_updateChipPreviewSprite_811A1B0(void) {
    struct ChipPreviewResult res = chip_preview(R5, R4, R6);
    R0 = res.r0;
    R1 = res.r1;
    R4 = res.r4;
    R6 = res.r6;
    SET_FLAGS_NZ(res.nz_val);
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

/* ── sub_811A29C ── */
/* register form: calls sprite_getUnk0x2c outside this file, which has no typed C interface yet */
static u32 sprite_get_unk2c(void) {
    GUEST_CALL(sprite_getUnk0x2c);
    return R0;
}

/* register form: calls sprite_setUnk0x2c outside this file, which has no typed C interface yet */
static void sprite_set_unk2c(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setUnk0x2c);
}

/* register form: calls menu_getTableEntryForStateIndex_811A268 outside this file,
 * which has no typed C interface yet (returns three values in r0/r1/r2) */
static void menu_get_table_entry(u32 *out_r0, u32 *out_r1, u32 *out_r2) {
    GUEST_CALL(menu_getTableEntryForStateIndex_811A268);
    *out_r0 = R0;
    *out_r1 = R1;
    *out_r2 = R2;
}

/* Tweaks a sprite's unk0x2c based on a menu-state table entry.
 *
 * Active path (taken when r0 != 0 AND (r2 != 0 OR r1 == 0)):
 *   OR in 0x7E, then clear the bit at position (6 - state_byte) of that low byte.
 * Inactive path: clear the 0x7E bits and OR in 0x3E.
 *
 * Inputs/Outputs from the original register contract are r4-r7/lr, preserved across. */
static u32 apply_sprite_tweak(void) {
    u32 r0, r1, r2;

    menu_get_table_entry(&r0, &r1, &r2);

    /* Original: tst r0,r0 (beq CA); tst r2,r2 (bne AE); tst r1,r1 (bne CA); -> AE
     * Active (AE) iff r0 != 0 AND (r2 != 0 OR r1 == 0). */
    if (r0 != 0 && (r2 != 0 || r1 == 0)) {
        u32 state_byte = LD8(A_byte_8124B1C + (r0 - 0x140));
        /* ARM `lsr r2, r4` with r4 as a 32-bit register: shift by >= 32 yields 0. */
        u32 mask = (state_byte < 32) ? (0x40u >> state_byte) : 0;
        u32 unk2c = sprite_get_unk2c() | 0x7E;
        sprite_set_unk2c(unk2c & ~mask);
    } else {
        sprite_set_unk2c((sprite_get_unk2c() & ~0x7E) | 0x3E);
    }
    return 0;
}

Next sub_811A29C(void) {
    apply_sprite_tweak();
    RETURN_TO_CALLER();
}

/* ── sub_811A2EC ── */
/* sub_811A2EC:
 * Adjusts the sprite attribute word at offset 0x2C of the menu-table entry
 * selected for the current state index. The caller's R0-R3 are passed
 * straight through to menu_getTableEntryForStateIndex_811A268, which
 * returns the entry pointer in R0 plus two flag values in R1 and R2.
 *
 * - No entry (R0 == 0), or flags saying "reset" (R2 == 0 but R1 != 0):
 *   clear the whole 0x1FC0 field and store 0xF80 in its place.
 * - Otherwise: OR 0x1FC0 into the attribute word, then clear the single
 *   bit selected by R2 (bit R2+1 of the 0x1000 bit run), leaving that
 *   sub-field at zero while the rest of the field is filled. */
Next sub_811A2EC(void) {
    GUEST_CALL(menu_getTableEntryForStateIndex_811A268);

    u32 entry = R0;
    u32 flag1 = R1;
    u32 flag2 = R2;

    if (entry != 0 && (flag2 != 0 || flag1 == 0)) {
        /* Fill the 0x1FC0 field, then clear the one bit picked by flag2. */
        u32 bitIndex = (flag2 + 1) & 0xFF;          /* ARM LSR uses the low byte */
        u32 clearMask = bitIndex < 32 ? (0x1000u >> bitIndex) : 0;

        R0 = entry;
        GUEST_CALL(sprite_getUnk0x2c);
        R0 = (R0 | 0x1FC0u) & ~clearMask;
        GUEST_CALL(sprite_setUnk0x2c);
    } else {
        /* Reset the whole field to the fixed value 0xF80. */
        R0 = entry;
        GUEST_CALL(sprite_getUnk0x2c);
        R0 = (R0 & ~0x1FC0u) | 0xF80u;
        GUEST_CALL(sprite_setUnk0x2c);
    }

    RETURN_TO_CALLER();
}

/* ── sub_811A414 ── */
/* sub_811A414: state dispatcher for the battle object in r5.
 *
 * Reads the object's state byte at (r5 + 8) and uses it as a raw byte
 * offset into the jump table at off_811A428 (states are stored
 * pre-multiplied by 4: 0 -> sub_811A434, 4 -> sub_811A4DC, 8 -> sub_811A5B4).
 * The chosen handler is entered with r0 = its own table entry (handler
 * address | Thumb bit) and r1 = the raw state byte; every other register
 * passes through untouched. `mov lr, pc; bx r0` makes the handler return to
 * the `pop {pc}` right after the branch, which returns to this function's
 * caller — so the handler's registers (r2, r3, r5, r10, r11) become this
 * function's results.
 */

/* Register results left by a state handler; these are sub_811A414's outputs. */
struct StateHandlerResult {
    u32 r2;
    u32 r3;
    u32 r5;
    u32 r10;
    u32 r11;
};

/* register form: the state handlers are reached through the jump table at
 * off_811A428 (ldr r0,[table]; mov lr,pc; bx r0), i.e. a call through an
 * address that no generated wrapper covers. The handler sees every register
 * except r0/r1 exactly as the dispatcher's caller left it, so only r0 and
 * r1 are set here; the five output registers are read back after the call
 * (a handler that leaves one of them alone passes its incoming value
 * through, as in the original). */
static struct StateHandlerResult call_state_handler(u32 handler, u32 state) {
    R0 = handler; /* table entry as loaded from off_811A428 */
    R1 = state;   /* raw state byte from (r5 + 8) */

    GUEST_CALL_ADDR(handler);

    struct StateHandlerResult out;
    out.r2 = R2;
    out.r3 = R3;
    out.r5 = R5;
    out.r10 = R10;
    out.r11 = R11;
    return out;
}

/* Look up and run the state handler of the object at obj. */
static struct StateHandlerResult dispatch_object_state(u32 obj, u32 r10) {
    (void)r10; /* r10 passes through the register file untouched */
    u32 state = LD8(obj + 8);
    u32 handler = LD32(A_off_811A428 + state);
    return call_state_handler(handler, state);
}

Next sub_811A414(void) {
    struct StateHandlerResult out = dispatch_object_state(R5, R10);
    R2 = out.r2;
    R3 = out.r3;
    R5 = out.r5;
    R10 = out.r10;
    R11 = out.r11;
    RETURN_TO_CALLER();
}

/* ── sub_811A434 ── */
/* sub_811A434 — initialise a slot-list icon sprite.
 *
 * Inputs:  r5 = slot state pointer, r10 = caller scratch, lr
 * Outputs: r2, r3, r5, r10, r11
 *
 * Writes the sprite-id byte (0x23) at the start of the slot struct, then picks
 * a sprite/animation pair based on a global submenu mode byte and the slot's
 * flag byte, builds the GUI sprite with palette/priority tweaks, runs the
 * per-slot helpers, then schedules the next-step handler.
 */

/* Offsets into the slot-state struct (relative to r5). */
#define SLOT_STATE_KIND      0   /* sprite-id byte written at the start    */
#define SLOT_STATE_FLAG      4   /* non-zero → use the "flagged" art pair  */
#define SLOT_STATE_ANIM      5   /* animation index to load                */
#define SLOT_STATE_FLAGS2    8   /* written 4 here at the end              */
#define SLOT_STATE_ANIMCOPY  0xC /* mirror of the animation byte           */

Next sub_811A434(void) {
    u32 slot = R5;

    /* First action of the original: write the sprite-id byte 0x23 at slot+0. */
    ST8(slot + SLOT_STATE_KIND, 0x23);

    /* Choose the sprite id and GUI descriptor. Both `sSubmenu[1] == 0x1C`
     * aliases assign the same address — kept for control-flow fidelity. */
    u32 kind = 0x24;
    u32 art  = (u32)A_byte_20231A0;
    if (LD8((u32)A_sSubmenu + 1) == 0x1C)
        art = (u32)A_byte_20231A0;

    if (LD8(slot + SLOT_STATE_FLAG) != 0) {
        kind = 0x23;
        art  = (u32)A_eGuiSprite2022AA0;
        if (LD8((u32)A_sSubmenu + 1) == 0x1C)
            art = (u32)A_eGuiSprite2022AA0;
    }

    /* Build the GUI sprite. */
    R0 = kind;
    R1 = art;
    GUEST_CALL(initGuiSprite_8002770);

    /* Mirror the animation byte into the secondary slot and load it. */
    u32 anim = LD8(slot + SLOT_STATE_ANIM);
    ST8(slot + SLOT_STATE_ANIMCOPY, (u8)anim);
    R0 = (u8)anim;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    /* Palette 0, priority 2. */
    R0 = 0;
    GUEST_CALL(sprite_setPalette);
    R0 = 2;
    GUEST_CALL(sprite_setPriority_8002E14);

    /* Halfword-from-ROM-table → slotList store.
     * ROM table at byte_811A4BC is loaded into R1 as a u32 pointer; the
     * index is the slot's flag byte (which is 0 or 1 here), shifted left 2. */
    R1 = (u32)A_byte_811A4BC;
    {
        u32 idx   = LD8(slot + SLOT_STATE_FLAG);
        u32 entry = LD32((u32)A_byte_811A4BC + (idx << 2));
        R0 = entry;
    }
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);

    /* Sub-entry palette bank. ROM table at 0x100 (off_811A4D8) holds:
     *   00 01 00 00 (little-endian 0x100), indexed by the slot flag byte. */
    {
        u32 idx   = LD8(slot + SLOT_STATE_FLAG);
        u32 entry = LD8((u32)A_off_811A4D8 + idx);
        R1 = (u32)A_off_811A4D8;
        R0 = entry;
    }
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);

    /* Record field A = 3. */
    R0 = 3;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);

    /* Per-slot base setup. */
    GUEST_CALL(sprite_setPositionByFlags_811A5BC);

    /* Flagged slots get an extra init pass. */
    if (LD8(slot + SLOT_STATE_FLAG) != 0)
        GUEST_CALL(sub_811A640);

    /* Mark slot ready (4) and tail-call the next-step handler. */
    ST8(slot + SLOT_STATE_FLAGS2, 4);
    GUEST_CALL(sub_811A4DC);

    RETURN_TO_CALLER();
}

/* ── sub_811A4DC ── */
/* sub_811A4DC(r5 = sprite/object work pointer):
 * Dispatch to the per-state update handler selected by the state byte at
 * r5+4 (handler table off_811A4F4), then run the shared sprite update.
 * The original performs this as a computed call that returns to the
 * sprite_update call site, so every handler is followed by sprite_update. */
Next sub_811A4DC(void) {
    u32 state = LD8(R5 + 4);

    /* Call the handler for this state (table holds one Thumb pointer per
     * state: sub_811A4FC for state 0, sub_811A500 otherwise). */
    if (state == 0)
        GUEST_CALL(sub_811A4FC);
    else
        GUEST_CALL(sub_811A500);

    /* Shared sprite update, run after whichever handler executed. */
    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── sub_811A4FC ── */
/* sub_811A4FC:
 * Trivial wrapper that pushes LR and pops it into PC, i.e. a no-op tail return. */
Next sub_811A4FC(void) {
    RETURN_TO_CALLER();
}

/* ── sub_811A500 ── */
/* sub_811A500: dispatch to the update routine of the object in r5.
 *
 * The object's type byte (offset 7) indexes a small jump table at
 * off_811A514 (0 -> sub_811A51C, 1 -> sprite_updatePositionAndClamp_811A57C).
 * The original performs `mov lr, pc; bx r0`, so the selected handler runs
 * with every register intact and returns straight to our caller; its
 * r5/r10/r11 are therefore this function's outputs. */

/* register form: the call goes through an address read from the jump table
 * (mov lr, pc; bx rN), and the handlers receive the object in r5, so r5
 * must be live when the call is made */
static void dispatch_object_update(u32 object_ptr) {
    u32 index = LD8(object_ptr + 7) << 2;
    u32 handler = LD32(A_off_811A514 + index);
    R5 = object_ptr;
    GUEST_CALL_ADDR(handler);
}

Next sub_811A500(void) {
    dispatch_object_update(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_updatePositionAndClamp_811A57C ── */
/* sprite_updatePositionAndClamp_811A57C: clamp a sprite's X coordinate against 0x3C (using a per-state
 * clamp table) and reset a state byte, then chain into sub_811A51C.
 *   inputs:  r5 (sprite object), lr
 *   outputs: r5
 *   preserved: r4 r5 r6 r7 */
Next sprite_updatePositionAndClamp_811A57C(void) {
    /* sprite_addCoordinates(0x10, 0) — find slot 0x10, then move origin to (0,0). */
    R0 = 0x10;
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    /* x = sprite_getCoordinates() */
    GUEST_CALL(sprite_getCoordinates);
    s32 x = (s32)R0;

    if (x >= 0x3C) {
        /* Load per-sprite clamp value: table at byte_811A5AC (u16 array),
         * indexed by the low byte of r5+6 (state byte), shifted left by 1. */
        u32 index = (u32)LD8(R5 + 6) << 1;
        u16 clamp = LD16(A_byte_811A5AC + index);

        /* sprite_setCoordinates(0x3C, clamp) — set X to 0x3C, Y to table value. */
        R0 = 0x3C;
        R1 = clamp;
        GUEST_CALL(sprite_setCoordinates);

        /* r5+7 = 0 (clear the state flag). */
        ST8(R5 + 7, 0);
    }

    /* Chain into the next stage. */
    GUEST_CALL(sub_811A51C);

    RETURN_TO_CALLER();
}

/* ── sub_811A5B4 ── */
/* sub_811A5B4: wrapper that clears a sprite's scale counter and marks it
 * unscalable, then returns to caller.
 *   inputs:  r5 lr
 *   outputs: r2 r3 r5
 *   clobbers: nz c v r0 r1 lr
 *   preserved: r4 r5 r6 r7
 */
Next sub_811A5B4(void) {
    GUEST_CALL(sprite_clearScaleCounterAndMakeUnscalable_804668A);
    RETURN_TO_CALLER();
}

/* ── sprite_setPositionByFlags_811A5BC ── */
/* sprite_setPositionByFlags_811A5BC: Position the battle-screen sprite whose index is implied by
 * the object array R5 points into. Each per-frame entry stores three flag
 * bytes (R5+4, R5+7) that pick one of three sprite-coordinate tables and a
 * 4th byte (R5+6) that indexes a {x, y} pair inside the chosen table. The
 * (x, y) pair is then handed to sprite_setCoordinates.
 *
 *   inputs:  r5  pointer into the per-frame object array
 *   outputs: -
 *   clobbers: nz c v r0 r1 r2 r3 lr
 */
Next sprite_setPositionByFlags_811A5BC(void) {
    u32 tbl;

    /* First flag clear? Use the "off-screen" table at byte_811A600. */
    if (LD8(R5 + 4) != 0) {
        /* Inner object not active either? Same off-screen table. */
        if (LD8(R5 + 7) != 0) {
            tbl = A_byte_811A600;
        } else {
            tbl = A_byte_811A5EC;
        }
    } else {
        tbl = A_dword_811A5FC;
    }

    /* Read the chosen (x, y) pair (each is a signed halfword). */
    u32 idx = (u32)LD8(R5 + 6) * 4u;
    s32 x = (s32)LD16(tbl + idx);
    s32 y = (s32)LD16(tbl + idx + 2);

    R0 = (u32)x;
    R1 = (u32)y;
    GUEST_CALL(sprite_setCoordinates);

    RETURN_TO_CALLER();
}

/* ── sub_811A640 ── */
/* sub_811A640: submenu action dispatcher.
 *
 * Looks at the currently open submenu (sSubmenu):
 *  - if the pressed direction (sSubmenu.jo_01) is 0x1C, runs the d-pad
 *    "up" helper sub_811A704;
 *  - otherwise reads the handler address selected by sSubmenu.unk_02
 *    from the table at dword_811A66C and calls it (entry 0 = no action).
 *
 * Both the helper and the handlers are invoked with:
 *   r0 = the table entry itself (Thumb address; the helper sees jo_01),
 *   r1 = *(u8*)(r5 + 6),
 *   r7 = &sSubmenu.
 * r4-r7 are saved/restored around the dispatch, as the original does
 * with its push/pop, so the caller's r4-r7 are preserved. */

/* Thumb entry points stored in the handler table dword_811A66C. */
#define HANDLER_SUB_811A68C 0x0811A68Du /* sub_811A68C+1 */
#define HANDLER_SUB_811A6A0 0x0811A6A1u /* sub_811A6A0+1 */
#define HANDLER_SUB_811A6B4 0x0811A6B5u /* sub_811A6B4+1 */
#define HANDLER_SUB_811A6C8 0x0811A6C9u /* sub_811A6C8+1 */
#define HANDLER_SUB_811A6DC 0x0811A6DDu /* sub_811A6DC+1, also table entries 5-7 */

Next sub_811A640(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;
    u32 submenu = A_sSubmenu;

    u32 joyDir = LD8(submenu + 1); /* sSubmenu.jo_01 */
    if (joyDir == 0x1C) {
        /* D-pad up: dedicated helper. */
        R0 = joyDir;
        R1 = LD8(R5 + 6);
        R7 = submenu;
        GUEST_CALL(sub_811A704);
    } else {
        u32 selection = LD8(submenu + 2); /* sSubmenu.unk_02 */
        u32 handler = LD32(A_dword_811A66C + selection);

        /* Handlers receive their own table entry in r0. */
        R0 = handler;
        if (handler != 0) {
            R1 = LD8(R5 + 6);
            R7 = submenu;
            switch (handler) {
            case HANDLER_SUB_811A68C: GUEST_CALL(sub_811A68C); break;
            case HANDLER_SUB_811A6A0: GUEST_CALL(sub_811A6A0); break;
            case HANDLER_SUB_811A6B4: GUEST_CALL(sub_811A6B4); break;
            case HANDLER_SUB_811A6C8: GUEST_CALL(sub_811A6C8); break;
            default:                  GUEST_CALL(sub_811A6DC); break; /* entries 5-7 */
            }
        }
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_811A68C ── */
/* sub_811A68C
 * inputs:   r1 (index into byte table), r10, lr
 * outputs:  r10, r11
 * reads an unsigned byte from a small table, passes it to sub_811A718.
 */
Next sub_811A68C(void) {
    u32 index_byte = LD8(A_dword_811A69C + R1);
    R0 = index_byte;
    GUEST_CALL(sub_811A718);
    /* r10/r11 are left as sub_811A718 set them; original tail is `pop {pc}`. */
    RETURN_TO_CALLER();
}

/* ── sub_811A6A0 ── */
/* sub_811A6A0(r1 = index): look up a byte in the 0x181716 table at index r1,
 * then call sub_811A718 with that byte in r0.
 *   inputs:  r1 r10 lr
 *   outputs: r10 r11
 *   clobbers: r0 r1 r2 r3 r4 r12 lr, flags nz/c/v
 *   preserved: r5 r6 r7 r8 r9 */
Next sub_811A6A0(void) {
    u8 table = LD8(A_dword_811A6B0 + R1);
    R0 = table;
    GUEST_CALL(sub_811A718);
    RETURN_TO_CALLER();
}

/* ── sub_811A6B4 ── */
Next sub_811A6B4(void) {
    /* inputs:  r1, r10, lr
     * outputs: r10, r11
     * Fetches a byte from the lookup table at dword_811A6C4 indexed by r1
     * and dispatches to sub_811A718 with it as the argument. */
    u8 index_byte = LD8(A_dword_811A6C4 + R1);
    R0 = index_byte;
    GUEST_CALL(sub_811A718);
    RETURN_TO_CALLER();
}

/* ── sub_811A6C8 ── */
/* sub_811A6C8(r1 = index): look up a packed value from a 3-byte table and call sub_811A718
 * with the indexed byte as argument. */
Next sub_811A6C8(void) {
    /* Table at 0x0811A6D8 holds 0x1F1E1D (3 bytes). The index r1 selects one of them. */
    R0 = LD8(0x0811A6D8 + R1);
    GUEST_CALL(sub_811A718);
    RETURN_TO_CALLER();
}

/* ── sub_811A6DC ── */
/* sub_811A6DC(r1 = nav_index, r7 = unit_block, r10 = ?, lr = ?):
 * Returns something via r10/r11 (set up for the caller). Picks an offset byte from
 * a small constant table indexed by nav_index, bumping the index when the unit has
 * no secondary action nav available, then forwards to sub_811A718 which records
 * the chosen nav button. */
Next sub_811A6DC(void) {
    u32 nav_index = R1;
    u8  block_flags = LD8(R7 + 0x1B);

    /* If this is the secondary action nav slot AND the unit has no secondary nav
     * flag set, skip ahead by one. */
    u32 lookup;
    if (nav_index == 2 && (block_flags & 2) == 0)
        lookup = nav_index + 1;
    else
        lookup = nav_index;

    /* dword_811A700 = 0x302F1A19, treated as four bytes: {0x19,0x1A,0x2F,0x30}. */
    R0 = LD8(A_dword_811A700 + lookup);
    GUEST_CALL(sub_811A718);
    RETURN_TO_CALLER();
}

/* ── sub_811A718 ── */
Next sub_811A718(void) {
    u32 text_ptr = R0;
    u32 chapter  = R1;
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    u32 dst_buf       = LD32(A_off_811A740 + (chapter << 2));
    u32 layout_flags = LD32(A_byte_811A754 + (chapter << 2));

    /* renderTextGfx_8045F8C wants r4=8, r5=1, r6=dword_86B7AE0, r7=0
     * (it reads them as its own scratch), but r4-r7 are callee-saved in
     * this function, so we restore the caller's values on return. */
    R4 = 8;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 0;

    R0 = A_eTextScript201BA20;
    R1 = text_ptr;
    R2 = dst_buf;
    R3 = layout_flags;
    GUEST_CALL(renderTextGfx_8045F8C);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── sub_811A768 ── */
/* sub_811A768(r0=sprite, r1=var):
 *   Add (R0, 0) to the sprite via sprite_addCoordinates (which uses R5
 *   as the sprite pointer and writes back into the sprite struct, e.g.
 *   var+0x2c), then read back the x-coordinate with sprite_getCoordinates.
 *   If it is < -48, write 8 to var+8 and return 0; otherwise return 1.
 *   R4 and R5 are preserved across the call (the helper functions use R5
 *   to locate the sprite, so we must set R5=var before invoking them). */
Next sub_811A768(void) {
    u32 var = R1;
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    R5 = var;
    R4 = 1;
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);
    GUEST_CALL(sprite_getCoordinates);

    SET_FLAGS_CMP(R0, 0xFFFFFFD0u);
    if ((s32)R0 < (s32)0xFFFFFFD0) {
        ST8(var + 8, 8);
        R0 = 0;
        R4 = 0;
    } else {
        R0 = 1;
    }

    R4 = saved_r4;
    R5 = saved_r5;

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
        GUEST_CALL(setupGuiObjectSprite_811B20C);
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

/* ── setupGuiObjectSprite_811B20C ── */
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

/* register form: calls sprite_submenuUpdateAnimAndPos_811B298 outside this file, which has no typed C interface yet */
static void call_sub_811B298(void) {
    GUEST_CALL(sprite_submenuUpdateAnimAndPos_811B298);
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

/* setupGuiObjectSprite_811B20C(r4, r5 = object, r6, r7, lr): no result of its own; flags and
 * r0-r3 are left exactly as the final callee leaves them. */
Next setupGuiObjectSprite_811B20C(void) {
    setup_object_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_objectUpdateAnimation_811B284 ── */
/* sprite_objectUpdateAnimation_811B284: update step for an animated object — first runs the
 * object's own per-frame handler (sprite_submenuUpdateAnimAndPos_811B298), then advances its
 * sprite animation via sprite_update. */
Next sprite_objectUpdateAnimation_811B284(void) {
    GUEST_CALL(sprite_submenuUpdateAnimAndPos_811B298);
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sprite_submenuUpdateAnimAndPos_811B298 ── */
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

/* sprite_submenuUpdateAnimAndPos_811B298(r5 = sprite ptr) -> r2 = coord table base */
Next sprite_submenuUpdateAnimAndPos_811B298(void) {
    R2 = sprite_update_animation(R5);
    RETURN_TO_CALLER();
}

/* ── sub_811B314 ── */
/* sub_811B314(r0 = object id, r1 = animation id):
 * Fetch the object's sprite struct from the list at 2008450 (result in r1,
 * "not found" signalled in the Z flag). Give up if there is no struct or if
 * its state byte has the 0x08 flag set. Otherwise start the requested
 * animation on it, load its frame data and hide its shadow.
 * Leaves r2 = 8 once the struct has been examined; r4/r5 are preserved. */
Next sub_811B314(void) {
    u32 animationId = R1;
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    /* mov r4, r1: the animation id travels in r4 for the rest of the call. */
    R4 = animationId;

    GUEST_CALL(getStructFrom2008450);
    if (!ZERO_FLAG()) {
        u32 sprite = R1;

        R2 = 0x08;
        if (!(LD8(sprite) & 0x08)) {
            R5 = sprite;
            R0 = animationId;
            GUEST_CALL(sprite_setAnimation);
            GUEST_CALL(sprite_loadAnimationData);
            GUEST_CALL(sprite_noShadow);
        }
    }

    /* pop {r4,r5}: the original restores both saved registers on every path,
     * so the struct pointer in r5 is only visible to the callees above. */
    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sub_811B3F4 ── */
/* Dispatch one object's per-frame update handler through the jump table
 * jt_811B408. The selector byte at [object+8] picks one of three handlers;
 * each handler is entered with its own entry address in r0 and the selector
 * byte in r1, exactly as the original's `bx r0` sets things up.
 *
 * register form: the call goes through a code address taken from the jump
 * table (mov lr, pc; bx r0), so there is no typed wrapper for the handlers.
 * The callee leaves r2, r3, r5, r10 and r11 behind for the caller. */
static void run_object_update(u32 object) {
    u32 selector = LD8(object + 8);
    u32 handler = LD32(A_jt_811B408 + selector);

    R0 = handler;        /* handlers expect their own entry address in r0 */
    R1 = selector;
    GUEST_CALL_ADDR(handler);
}

/* sub_811B3F4: jump-table dispatch to an object's update handler.
 * Outputs r2/r3/r5/r10/r11 are simply whatever the handler leaves behind;
 * the flags are clobbers and are not propagated. */
Next sub_811B3F4(void) {
    run_object_update(R5);
    RETURN_TO_CALLER();
}

/* ── gui_initSprite0x2C_811B414 ── */
/* Register-form helpers for external game functions called during GUI sprite setup. */

/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void call_initGuiSprite_sub_811B414(u32 sprite_id, u32 a2) {
    R0 = sprite_id;
    R1 = a2;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls getPetNaviStatus_80101F8 outside this file, which has no typed C interface yet */
static u32 call_getAnimationId(void) {
    GUEST_CALL(getPetNaviStatus_80101F8);
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

/* gui_initSprite0x2C_811B414(r1 = gui_arg, r5 = state_ptr, r4/r6 preserved) -> r0 (from wrapper update), nzcv from callee */
Next gui_initSprite0x2C_811B414(void) {
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

/* ── sprite_setCoordsIfExists_811B46C ── */
/* register form: calls getStructFrom2008450 outside this file, which has no typed C interface yet.
 * Returns the struct pointer (R1) if found (Z clear), or 0 if not found. */
static u32 lookup_struct(u32 sprite_id) {
    R0 = sprite_id;
    GUEST_CALL(getStructFrom2008450);
    if (ZERO_FLAG())
        return 0;
    return R1;
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet.
 * Coordinates (x, y) go in R0, R1; the struct pointer goes in R5. */
static void sprite_set_coords(u32 x, u32 y, u32 struct_ptr) {
    R0 = x;
    R1 = y;
    R5 = struct_ptr;
    GUEST_CALL(sprite_setCoordinates);
}

/* Look up the sprite by id; if found, set its screen coordinates. */
static void set_sprite_coords_if_found(u32 x, u32 y, u32 sprite_id) {
    u32 struct_ptr = lookup_struct(sprite_id);
    if (struct_ptr == 0)
        return;
    sprite_set_coords(x, y, struct_ptr);
}

/* sprite_setCoordsIfExists_811B46C(r0 = x, r1 = y, r2 = sprite_id, r4-r7 preserved):
 * conditionally update a sprite's position. Outputs r1, r2 from callees;
 * r4, r5, r6 restored to input (pop in original); flags c, v from last callee. */
Next sprite_setCoordsIfExists_811B46C(void) {
    u32 saved_r5 = R5;
    set_sprite_coords_if_found(R0, R1, R2);
    R5 = saved_r5;
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

/* ── sub_811BD88 ── */
/* sub_811BD88(r5 = PET object state):
 * Picks the sprite slot of the navi to display on the PET screen and
 * advances the object to its next state:
 *   [r5+0x0] = 1   (state: navi display active)
 *   [r5+0xC] = chosen sprite slot
 *   [r5+0x8] = 4   (next update handler id)
 *
 * Slot selection:
 *   - If a navi is currently selected (GetCurPETNavi != 0), the slot is
 *     navi + 0xB (the "selected navi" sprite bank).
 *   - Otherwise the base slot is the navi stats byte at index 0x17; if
 *     the byte at 0x200AD04 is nonzero and EVENT_163 has not yet been
 *     triggered, 0x17 is added (the post-event alternate slot).
 */
Next sub_811BD88(void) {
    u32 callerR4 = R4;

    /* Mark the object as having entered this state. */
    R0 = 1;
    ST8(R5, R0);

    GUEST_CALL(GetCurPETNavi);

    if (R0 != 0) {
        /* Navi selected: its sprites start at slot 0xB. */
        R0 = R0 + 0xB;
    } else {
        /* No navi: take the sprite slot from the navi stats table. */
        R0 = 0;
        R1 = 0x17;
        GUEST_CALL(GetCurPETNaviStatsByte);
        R4 = R0;

        GUEST_CALL(notZero_eByte200AD04);
        if (!ZERO_FLAG()) {
            /* Progress-gated slot: only applies before EVENT_163. */
            R0 = 1;
            R1 = EVENT_163;
            GUEST_CALL(TestEventFlagFromImmediate);
            if (ZERO_FLAG())        /* event not yet triggered */
                R4 = R4 + 0x17;
        }
        R0 = R4;
    }

    ST32(R5 + 0xC, R0);

    GUEST_CALL(sub_811BE1C);

    R0 = 4;
    ST8(R5 + 8, R0);
    GUEST_CALL(sprite_dispatchObjectUpdate_811BDCC);

    R4 = callerR4;
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
/* sub_811BDEC: thin trampoline — performs adjustSpriteByOffset_811BE9C and returns.
 * All of the real work (and all outputs: r0-r3, flags, lr) belongs to
 * adjustSpriteByOffset_811BE9C; this wrapper exists only so callers can branch through it.
 * r5 is an input to the callee and passes through untouched. */
Next sub_811BDEC(void) {
    GUEST_CALL(adjustSpriteByOffset_811BE9C);
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

/* ── sub_811BE1C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6; } call_initGuiSprite_8002770_result;
/* register form: generated wrapper: initGuiSprite_8002770 has no typed C interface yet */
static inline call_initGuiSprite_8002770_result call_initGuiSprite_8002770(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(initGuiSprite_8002770);
    return (call_initGuiSprite_8002770_result){ R4, R5, R6 };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r2, r4, r5; } call_sprite_loadAnimationData_result;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result call_sprite_loadAnimationData(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result){ R2, R4, R5 };
}

/* register form: generated wrapper: sprite_noShadow has no typed C interface yet */
static inline void call_sprite_noShadow(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_noShadow);
}

typedef struct { u32 r2; } call_sprite_setSubEntryPaletteBank_800304A_result;
/* register form: generated wrapper: sprite_setSubEntryPaletteBank_800304A has no typed C interface yet */
static inline call_sprite_setSubEntryPaletteBank_800304A_result call_sprite_setSubEntryPaletteBank_800304A(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
    return (call_sprite_setSubEntryPaletteBank_800304A_result){ R2 };
}

typedef struct { u32 r0, r3; } call_GetCurPETNavi_result;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result call_GetCurPETNavi(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result){ R0, R3 };
}

typedef struct { u32 r0; } call_GetCurPETNaviStatsByte_result;
/* register form: generated wrapper: GetCurPETNaviStatsByte has no typed C interface yet */
static inline call_GetCurPETNaviStatsByte_result call_GetCurPETNaviStatsByte(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (call_GetCurPETNaviStatsByte_result){ R0 };
}

/* register form: generated wrapper: sprite_setPalette has no typed C interface yet */
static inline void call_sprite_setPalette(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPalette);
}

/* register form: generated wrapper: slotList_storeHalfwordAndClearField_800306C has no typed C interface yet */
static inline void call_slotList_storeHalfwordAndClearField_800306C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

typedef struct { u32 r2; } call_sprite_setPriority_8002E14_result;
/* register form: generated wrapper: sprite_setPriority_8002E14 has no typed C interface yet */
static inline call_sprite_setPriority_8002E14_result call_sprite_setPriority_8002E14(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setPriority_8002E14);
    return (call_sprite_setPriority_8002E14_result){ R2 };
}

/* register form: generated wrapper: sprite_setSubRecordFieldA_8003060 has no typed C interface yet */
static inline void call_sprite_setSubRecordFieldA_8003060(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

typedef struct { u32 r3; bool c, v; } call_sprite_setCoordinates_result;
/* register form: generated wrapper: sprite_setCoordinates has no typed C interface yet */
static inline call_sprite_setCoordinates_result call_sprite_setCoordinates(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(sprite_setCoordinates);
    return (call_sprite_setCoordinates_result){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Initialize (or stamp) the GUI sprite for the given slot.
 *  r0 = slot index, r5 = sprite record, r3_in = caller's r3, returned unchanged on early return.
 *  Returns r3 set by sprite_setCoordinates on the full path. */
static u32 init_pet_navi_sprite(u32 slot_index, u32 sprite_record, u32 r3_in) {
    /* Mark the sprite record as "entered" (state 1). */
    ST8(sprite_record + 0, 1);

    /* Look up the sprite-type table for this slot. If zero, the slot has no entry;
     * return with r3 preserved from the caller. */
    u32 table_val = LD8(A_byte_811BF94 + slot_index);
    if (table_val == 0)
        return r3_in;

    /* Bring up the GUI sprite shell. The sprite type comes from the table, the flag is 1. */
    call_initGuiSprite_8002770(table_val, 1, sprite_record);

    /* Mark the sprite record as fully initialized (state 0x23). */
    ST8(sprite_record + 0, 0x23);

    /* Set animation 0, pull in the frame data, disable the shadow, set palette bank 4. */
    call_sprite_setAnimation(0, sprite_record);
    call_sprite_loadAnimationData(sprite_record);
    call_sprite_noShadow(sprite_record);
    call_sprite_setSubEntryPaletteBank_800304A(4, sprite_record);

    /* Pick the palette index from the kind byte at sprite_record+0xC.
     *   - kind 0x17 is the current PET Navi: ask the live Navi for stats byte 0x0E. If the
     *     Navi has no data (0xFF), use the "no data" palette (0x0C); otherwise look the
     *     palette up in the table at index 0x17.
     *   - any other kind: look the palette up directly at offset = kind. */
    u8 kind = LD8(sprite_record + 0xC);
    u8 palette_index;
    if (kind == 0x17) {
        u8 navi = call_GetCurPETNavi().r0;
        u8 stat_byte = call_GetCurPETNaviStatsByte(navi, 0xE).r0;
        if (stat_byte == 0xFF) {
            palette_index = 0x0C;
        } else {
            palette_index = LD8(A_byte_811BFD8 + 0x17);
        }
    } else {
        palette_index = LD8(A_byte_811BFD8 + kind);
    }
    call_sprite_setPalette(palette_index, sprite_record);

    /* Stash a magic halfword (0x120) in the slot list, then set BG priority to 2. */
    call_slotList_storeHalfwordAndClearField_800306C(0x120, sprite_record);
    call_sprite_setPriority_8002E14(2, sprite_record);

    /* Sub-record field A holds a per-kind visual variant. Re-read kind — intervening
     * sprite helpers may have modified sprite_record+0xC. */
    u8 kind2 = LD8(sprite_record + 0xC);
    call_sprite_setSubRecordFieldA_8003060(LD8(A_byte_811BFB6 + kind2), sprite_record);

    /* Coordinates come from a packed (x, y) s16 table indexed by kind. Re-read kind. */
    u8 kind3 = LD8(sprite_record + 0xC);
    u32 xy_base = A_byte_811BF0C + ((u32)kind3 << 2);
    s16 x = (s16)LDS16(xy_base);
    s16 y = (s16)LDS16(xy_base + 2);
    /* ldrsh sign-extends, so cast through s32 to u32. */
    return call_sprite_setCoordinates((u32)(s32)x, (u32)(s32)y, sprite_record).r3;
}

Next sub_811BE1C(void) {
    R3 = init_pet_navi_sprite(R0, R5, R3);
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

/* ── adjustSpriteByOffset_811BE9C ── */
/* Sprite position offset table (signed 16-bit dx,dy pairs indexed by sprite ID). */
#define SPRITE_OFFSET_TABLE A_byte_811BF0C

/* Sprite attribute flag table (one byte per sprite ID). */
#define SPRITE_FLAG_TABLE   A_byte_811BF94

/* Values written to the sprite object's first byte after repositioning. */
#define SPRITE_ATTR_NORMAL  0x01
#define SPRITE_ATTR_SPECIAL 0x23

typedef struct { u32 x, y; } Coords2D;

/* register form: calls sprite_getCoordinates outside this file, which has no typed C interface yet */
static Coords2D get_sprite_coords(void) {
    GUEST_CALL(sprite_getCoordinates);
    Coords2D c = { R0, R1 };
    return c;
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet.
 * Leaves R3 and flags C,V in the guest state (callers rely on this). */
static void set_sprite_coords(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

typedef struct { u32 flag_val, attr, flag_table; } AdjustResult;

/* Reposition a sprite by its per-type offset, then tag it normal or special.
 * The sprite ID is re-read after setCoordinates because the callee may update obj+0xC. */
static AdjustResult adjust_sprite_offset(u32 obj) {
    Coords2D pos = get_sprite_coords();

    u32 id    = LD8(obj + 0xC);
    u32 table = SPRITE_OFFSET_TABLE;
    u32 idx   = id << 2;

    u32 new_x = pos.x + LDS16(table + idx);
    u32 new_y = pos.y + LDS16(table + idx + 2);
    set_sprite_coords(new_x, new_y);

    /* Re-read sprite ID after setCoordinates (callee may have updated [obj+0xC]). */
    u32 flag_table = SPRITE_FLAG_TABLE;
    u32 id_after   = LD8(obj + 0xC);
    u32 flag_val   = LD8(flag_table + id_after);
    u32 attr       = (flag_val != 0) ? SPRITE_ATTR_SPECIAL : SPRITE_ATTR_NORMAL;
    ST8(obj, attr);

    AdjustResult res = { flag_val, attr, flag_table };
    return res;
}

/* adjustSpriteByOffset_811BE9C(r5 = sprite object ptr) -> r0 r1 r2 r3 lr, flags nz c v */
Next adjustSpriteByOffset_811BE9C(void) {
    AdjustResult res = adjust_sprite_offset(R5);
    R0 = res.flag_val;
    R1 = res.attr;
    R2 = res.flag_table;
    /* R3 and C,V left by sprite_setCoordinates GUEST_CALL; only NZ come from TST. */
    SET_FLAGS_NZ(res.flag_val);
    RETURN_TO_CALLER();
}

/* ── sub_811BED4 ── */
/* sub_811BED4(r0 = entry id, r1 = record pointer):
 * Looks up the record identified by r0 in the structure pool at 0x2008450.
 * If no such record exists (lookup clears Z), nothing is written. Otherwise
 * a status byte is stored at offset 0 of the record: 0x23 when the record's
 * kind byte (offset 0x0C) selects a non-zero entry of byte_811BF94, and 1
 * otherwise (or when the caller passed id 0).
 *
 * Note: getStructFrom2008450 updates r1 with the resolved record pointer,
 * so all accesses below must use r1 as it is *after* the call. */
Next sub_811BED4(void) {
    u32 id = R0;

    R0 = R1;
    GUEST_CALL(getStructFrom2008450);
    u32 record = R1;              /* lookup replaces r1 with the record pointer */

    if (ZERO_FLAG())              /* id not present in the pool: leave it alone */
        RETURN_TO_CALLER();

    u32 value = 1;
    if (id != 0) {
        u8 kind = LD8(record + 0xC);
        if (LD8(A_byte_811BF94 + kind) != 0)
            value = 0x23;
    }
    ST8(record, value);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchByClass_811C1AC ── */
/* Dispatch on a battle object's class-selector byte at [obj+8].
 * The three handlers are outside this file with no typed C interface yet.
 * All of R2–R12, R5, and LR pass through to the callee unchanged;
 * the callee's GUEST_CALL leaves every output register (R0, R1, R2–R12, LR)
 * and condition flags in their final state, so the adapter writes nothing back. */

/* register form: calls sprite_setupGuiResourceObject_811C1CC outside this file, which has no typed C interface yet */
static void call_battle_handler_0(u32 obj, u8 idx,
    u32 r2, u32 r3, u32 r4, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 lr) {
    R1 = idx; R2 = r2; R3 = r3; R4 = r4; R5 = obj;
    R6 = r6; R7 = r7; R8 = r8; R9 = r9; R10 = r10; R11 = r11; R12 = r12; LR = lr;
    GUEST_CALL(sprite_setupGuiResourceObject_811C1CC);
}

/* register form: calls sprite_updateAnimPassthrough_811C22C outside this file, which has no typed C interface yet */
static void call_battle_handler_1(u32 obj, u8 idx,
    u32 r2, u32 r3, u32 r4, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 lr) {
    R1 = idx; R2 = r2; R3 = r3; R4 = r4; R5 = obj;
    R6 = r6; R7 = r7; R8 = r8; R9 = r9; R10 = r10; R11 = r11; R12 = r12; LR = lr;
    GUEST_CALL(sprite_updateAnimPassthrough_811C22C);
}

/* register form: calls sub_811C234 outside this file, which has no typed C interface yet */
static void call_battle_handler_2(u32 obj, u8 idx,
    u32 r2, u32 r3, u32 r4, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 lr) {
    R1 = idx; R2 = r2; R3 = r3; R4 = r4; R5 = obj;
    R6 = r6; R7 = r7; R8 = r8; R9 = r9; R10 = r10; R11 = r11; R12 = r12; LR = lr;
    GUEST_CALL(sub_811C234);
}

/* Select handler by the class-offset byte stored at [obj + 8].
 * Valid values are 0, 4, or 8 (word offsets into the dispatch table). */
static void dispatch_battle_object(u32 obj,
    u32 r2, u32 r3, u32 r4, u32 r6, u32 r7, u32 r8, u32 r9, u32 r10, u32 r11, u32 r12, u32 lr) {
    u8 class_off = (u8)LD8(obj + 8);
    if (class_off == 0)
        call_battle_handler_0(obj, class_off, r2, r3, r4, r6, r7, r8, r9, r10, r11, r12, lr);
    else if (class_off == 4)
        call_battle_handler_1(obj, class_off, r2, r3, r4, r6, r7, r8, r9, r10, r11, r12, lr);
    else
        call_battle_handler_2(obj, class_off, r2, r3, r4, r6, r7, r8, r9, r10, r11, r12, lr);
}

/* battle_dispatchByClass_811C1AC(r5 = battle object, r2-r4/r6-r12/lr = handler args) -> all regs + flags */
Next battle_dispatchByClass_811C1AC(void) {
    dispatch_battle_object(R5, R2, R3, R4, R6, R7, R8, R9, R10, R11, R12, LR);
    RETURN_TO_CALLER();
}

/* ── sprite_setupGuiResourceObject_811C1CC ── */
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

/* sprite_setupGuiResourceObject_811C1CC(r4, r5 = object work area, r6, r7) -> r0-r3, nz, c, v from
 * the final sprite_updateAnimPassthrough call; r4-r7 preserved. */
Next sprite_setupGuiResourceObject_811C1CC(void) {
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

/* ── sub_811C9FC ── */
/* sub_811C9FC: dispatches to one of three mode handlers through the jump
 * table at off_811CA10.  The mode selector is the byte at [r5+8]; the
 * byte is used as an index (0, 4, or 8) into the table of handler
 * addresses.  The loaded value still has bit 0 set (the original's
 * "sub_XXX+1" Thumb bit), and `bx r0` transfers to that Thumb target. */

#define MODE_BYTE_OFFSET 8

/* register form: computed branch through a jump table – the target
 * address is read from memory and the original's "bx r0" is preserved
 * exactly via GUEST_CALL_ADDR.  Only R0 (target address) and R1 (the
 * mode byte that the original placed in r1) are written here. */
static u32 dispatch_mode_handler(u32 ctx) {
    u32 mode    = LD8(ctx + MODE_BYTE_OFFSET);
    u32 handler = LD32(A_off_811CA10 + mode);
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* sub_811C9FC(r5 = context whose +8 byte selects the handler) */
Next sub_811C9FC(void) {
    R0 = dispatch_mode_handler(R5);
    RETURN_TO_CALLER();
}

/* ── install_object_state_handler_811CA1C ── */
/* install_object_state_handler: object base passed via guest register r5; the assembly
 * uses r5 to address fields 0/4/8 of the object record (state byte, palette index,
 * next state). r5 is preserved unchanged across the call. */

/* register form: calls loc_8002774 outside this file, which has no typed C interface yet */
static void loc_8002774_call(u32 a1, u32 a2) {
    R0 = a1;
    R1 = a2;
    GUEST_CALL(loc_8002774);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_setAnimation_call(u32 a1) {
    R0 = a1;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_loadAnimationData_call(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void sprite_noShadow_call(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_makeScalable outside this file, which has no typed C interface yet */
static void sprite_makeScalable_call(void) {
    GUEST_CALL(sprite_makeScalable);
}

/* register form: calls sprite_setScaleParameters outside this file, which has no typed C interface yet */
static void sprite_setScaleParameters_call(u32 a1, u32 a2, u32 a3) {
    R0 = a1;
    R1 = a2;
    R2 = a3;
    GUEST_CALL(sprite_setScaleParameters);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void sprite_setSubEntryPaletteBank_800304A_call(u32 a1) {
    R0 = a1;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void slotList_storeHalfwordAndClearField_800306C_call(u32 a1) {
    R0 = a1;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void sprite_setPriority_8002E14_call(u32 a1) {
    R0 = a1;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void sprite_setSubRecordFieldA_8003060_call(u32 a1) {
    R0 = a1;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void sprite_setCoordinates_call(u32 a1, u32 a2) {
    R0 = a1;
    R1 = a2;
    GUEST_CALL(sprite_setCoordinates);
}

/* register form: calls object_SetStateHandler_811CC4E outside this file, which has no typed C interface yet */
static u32 object_SetStateHandler_811CC4E_call(void) {
    GUEST_CALL(object_SetStateHandler_811CC4E);
    return R0;
}

/* register form: calls sub_811CA94 outside this file, which has no typed C interface yet */
static void sub_811CA94_call(void) {
    GUEST_CALL(sub_811CA94);
}

/* register form: calls sprite_clearScaleCounterAndMakeUnscalable_804668A outside this file, which has no typed C interface yet */
static void sprite_clearScaleCounterAndMakeUnscalable_804668A_call(void) {
    GUEST_CALL(sprite_clearScaleCounterAndMakeUnscalable_804668A);
}

/* State-handler installation helper. Object base passed via guest register r5;
 * the assembly reads/writes fields at r5+0 (state byte), r5+4 (palette-bank
 * index) and r5+8 (next-state byte). r5 is preserved across the call.
 * Marks the object as state 0x23 ("setting up sprite"), runs the sprite
 * initialisation sequence, then moves the object to state 4 and installs a
 * new state handler. If the handler installs, sub_811CA94 is invoked;
 * otherwise the just-set-up scalable state is torn down. */
static void install_object_state_handler(u32 obj_ptr) {
    u8 palette_index = LD8(obj_ptr + 4);
    u32 slot_addr;

    /* Mark the object as in state 0x23 ("initialising sprite"). */
    ST8(obj_ptr, 0x23);

    /* Load object graphics/animation tables into VRAM (0x89, &unk_201BA04). */
    loc_8002774_call(0x89, A_unk_201BA04);

    /* Configure the sprite. */
    sprite_setAnimation_call(0);
    sprite_loadAnimationData_call();
    sprite_noShadow_call();
    sprite_makeScalable_call();
    sprite_setScaleParameters_call(0, 0x80, 0x80);

    /* Palette bank = palette_index (r0 = 0 + palette_index). */
    sprite_setSubEntryPaletteBank_800304A_call(palette_index);

    /* Slot-list entry: halfword at (palette_index * 0x30 + 0x80). */
    slot_addr = (u32)palette_index * 0x30 + 0x80;
    slotList_storeHalfwordAndClearField_800306C_call(slot_addr);

    sprite_setPriority_8002E14_call(0);
    sprite_setSubRecordFieldA_8003060_call(0);
    sprite_setCoordinates_call(0, 0);

    /* Move into state 4 (handled by the handler we're about to install). */
    ST8(obj_ptr + 8, 4);

    /* Try to install the new state handler; on failure, undo the scalable
     * setup we just performed. */
    if (object_SetStateHandler_811CC4E_call()) {
        sub_811CA94_call();
    } else {
        sprite_clearScaleCounterAndMakeUnscalable_804668A_call();
    }
}

Next install_object_state_handler_811CA1C(void) {
    install_object_state_handler(R5);
    RETURN_TO_CALLER();
}

/* ── sub_811CA94 ── */
/* sub_811CA94(r5 = battle-object pointer):
 * Run the object's state-machine loop until a handler signals completion
 * (returns with Z = 1), then update flag bit 1 based on the timer field,
 * and refresh the object's sprite.
 *
 * Jump table at 0x0811CAC4 (7 × 4-byte entries, thumb-bit set):
 *   0: battleObject_runFloatingAnim_811CAEC
 *   4: timerCountdown_step_811CBA0
 *   8: battle_matchTrackedSpriteAndShrink_811CBD0
 *  12: sub_811CC02
 *  16: battle_initObjectSpriteAndStartAnimation_811CC0C
 *  20: sub_811CC26
 *  24: sub_811CC3A */
Next sub_811CA94(void) {
    u32 state_offset;

    /* dispatch loop: step the state machine until it reports "done" */
    do {
        /* sub_811CC68 returns a pointer whose halfword is the byte-offset
         * into the jump table for the current state. */
        GUEST_CALL(sub_811CC68);
        state_offset = LD16(R0);

        switch (state_offset) {
            case  0: GUEST_CALL(battleObject_runFloatingAnim_811CAEC);                                            break;
            case  4: GUEST_CALL(timerCountdown_step_811CBA0);                            break;
            case  8: GUEST_CALL(battle_matchTrackedSpriteAndShrink_811CBD0);             break;
            case 12: GUEST_CALL(sub_811CC02);                                            break;
            case 16: GUEST_CALL(battle_initObjectSpriteAndStartAnimation_811CC0C);       break;
            case 20: GUEST_CALL(sub_811CC26);                                            break;
            case 24: GUEST_CALL(sub_811CC3A);                                            break;
            default: GUEST_CALL(battleObject_runFloatingAnim_811CAEC);                                            break;
        }
    } while (!ZERO_FLAG());

    /* flag bit 1 tracks whether the countdown timer (offset 0x1A) has reached
     * the expiry threshold 0x1150; set it once elapsed, clear otherwise. */
    u32 flags = LD8(R5);
    flags &= ~2;
    if ((s32)LD16(R5 + 0x1A) >= 0x1150)
        flags |= 2;
    ST8(R5, flags);

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}
