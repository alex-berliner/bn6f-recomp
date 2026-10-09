/*
 * leaves_farm_28.c: small functions from the model farm, set 28.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── shop_copyItemTableToShopData_8048C98 ── */
/* shop_copyItemTableToShopData_8048C98: copy the shop item table (byte_8047DA0) into the current
 * shop's data buffer (toolkit->shopDataPtr). Each entry is 8 bytes:
 *   +0: item id (byte)   +2: price (halfword)
 *   +4: extra (byte)     +6: count/flags (halfword)
 * Entries whose byte at +1 is zero are empty slots and are skipped, but the
 * destination still advances so slots line up. The table is 0xE80 bytes
 * (464 entries) total. Leaves R2 = remaining byte count after the final
 * iteration (0xFFFFFFF8 when done) and R3 = the last value loaded (on a
 * skipped entry that is the empty-slot check byte itself), as the original
 * does. */
Next shop_copyItemTableToShopData_8048C98(void) {
    u32 src = A_byte_8047DA0;
    u32 dst = LD32(R10 + oToolkit_ShopDataPtr);
    u32 bytesLeft = 0xE80;
    u32 lastLoaded = 0;

    for (;;) {
        bytesLeft -= 8;
        if ((s32)bytesLeft < 0)
            break;

        lastLoaded = LD8(src + 1);    /* empty-slot check byte */
        if (lastLoaded != 0) {
            lastLoaded = LD8(src);        /* item id */
            ST8(dst, lastLoaded);
            lastLoaded = LD16(src + 2);   /* price */
            ST16(dst + 2, lastLoaded);
            lastLoaded = LD8(src + 4);    /* extra byte */
            ST8(dst + 4, lastLoaded);
            lastLoaded = LD16(src + 6);   /* count/flags */
            ST16(dst + 6, lastLoaded);
        }

        src += 8;
        dst += 8;
    }

    R2 = bytesLeft;
    R3 = lastLoaded;
    RETURN_TO_CALLER();
}

/* ── entity_runAllUpdateHandlers_80465BC ── */
/* entity_runAllUpdateHandlers_80465BC: run the update handler of every active record in the
 * 15-entry array at eStructArr2008450 (0x58 bytes per record).
 * A record is active when bit 0 of its flag byte (+0) is set; the byte at
 * +1 selects the handler from the pointer table off_804646C (byte * 4).
 * Each handler is entered with:
 *   r0 = its own address (the table entry), r1 = selector byte * 4,
 *   r4 = incoming r8, r5 = pointer to the record, r6 = incoming r12,
 *   r7 = record index; r2/r3 and r8-r12 pass through unchanged.
 * Exits with c/v from the loop's final "cmp r7, #0xF" (r7 == 15, so carry
 * set, overflow clear) and r1 = 1 if the last record was inactive, else
 * whatever the last handler left in r1. */
Next entity_runAllUpdateHandlers_80465BC(void) {
    u32 inR4 = R4, inR5 = R5, inR6 = R6, inR7 = R7;
    u32 inR8 = R8, inR9 = R9, inR12 = R12;

    u32 record = A_eStructArr2008450;
    for (u32 i = 0; i < 15; i++, record += 0x58) {
        R1 = 1;                              /* mov r1, #1 (kept if inactive) */
        if (LD8(record) & 1) {
            u32 sel = (u32)LD8(record + 1) << 2;
            R1 = sel;
            u32 handler_addr = LD32(A_off_804646C + sel);
            R0 = handler_addr;               /* handler sees its own address */
            R4 = inR8;                       /* copies of r8/r12 kept live in */
            R5 = record;                     /* r4/r6 across the whole loop   */
            R6 = inR12;
            R7 = i;                          /* push {r7} before the call...  */
            GUEST_CALL(dispatch_fast(handler_addr));
            R7 = i;                          /* ...pop {r7} after it          */
        }
    }

    /* Epilogue: r4-r7 and the saved r8/r9/r12 all end up as they came in. */
    R4 = inR4;
    R5 = inR5;
    R6 = inR6;
    R7 = inR7;
    R8 = inR8;
    R9 = inR9;
    R12 = inR12;

    /* Loop exit: the last iteration's cmp r7, #0xF with r7 == 15. */
    SET_FLAGS_CMP(15, 15);

    RETURN_TO_CALLER();
}

/* ── chatbox_uploadWindowTileGfx_804754C ── */
/* chatbox_uploadWindowTileGfx_804754C: upload the current text window's tile graphics.
 *
 * Copies 0x800 bytes of tile data from the staging buffer at byte_2017204
 * into the second half of the BG tile block (offset 0x800 of the toolkit's
 * iBGTileIdBlocks allocation), then renders the text object passed in r5
 * with the standard text-gfx renderer into the text layer (VRAM 0x6002000).
 *
 * Inputs:  r3 (context passed through), r5 = text object, r10 = toolkit.
 * Outputs: r3 r4 r6 r7 r10 and v as left by renderTextGfx_8045F8C;
 *          r5 r8 r9 preserved.
 */
Next chatbox_uploadWindowTileGfx_804754C(void) {
    u32 textObject = R5;

    /* Copy tile gfx from the staging buffer into BG tile block #1. */
    R0 = A_byte_2017204;
    R1 = LD32(R10 + oToolkit_iBGTileIdBlocks_Ptr) + 0x800;
    R2 = 0x800;
    GUEST_CALL(CopyByEightWords); /* (src, dest, size); preserves r3-r7, r10 */

    /* Render the caller's text object. */
    R0 = LD32(textObject + 0x24);
    R1 = 0x10;
    R2 = A_unk_2015E00;
    R3 = 0x6002000;
    R4 = 1;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    R5 = textObject;
    RETURN_TO_CALLER();
}

/* ── background_LoadAreaTiles_8047800 ── */
/* background_LoadAreaTiles_8047800(r5 = background state struct):
 * Redraw both background layers for the area whose id is stored at state+8.
 * The id is an index into the area table starting at 0x15; layer 1 copies its
 * tile graphics from the shared tile-id table eTileIds2018A04 into tile block
 * 2, layer 2 copies its tile map from byte_202F800 into tile block 0.
 * Preserves r4-r7 (the original pushes/pops them around the two calls). */
Next background_LoadAreaTiles_8047800(void) {
    const u32 state = R5;
    const u32 savedR4 = R4;
    const u32 savedR6 = R6;
    const u32 savedR7 = R7;
    const u32 areaId = LD8(state + 8);

    /* Layer 1: tile graphics, 0x15 x 0xC tiles into tile block 2. */
    R0 = areaId - 0x15;
    R1 = 0;
    R2 = 2;
    R3 = A_eTileIds2018A04;
    R4 = 0x15;
    R5 = 0xC;
    R7 = state;
    GUEST_CALL(CopyBackgroundTiles);

    /* Layer 2: tile map, 0x12 x 0xA tiles into tile block 0.
     * Its id is two past the graphics id (re-read from the struct as the
     * original does). */
    R0 = (LD8(state + 8) - 0x15) + 2;
    R1 = 1;
    R2 = 0;
    R3 = A_byte_202F800;
    R4 = 0x12;
    R5 = 0xA;
    R7 = state;
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore the callee-saved registers the original pops. */
    R4 = savedR4;
    R5 = state;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── textScript_copyFolderNameToWide_804A1FC ── */
/* textScript_copyFolderNameToWide_804A1FC(r0 = folder index, r1 = destination buffer):
 * Copy a folder name from the text script folder-name table into the destination
 * buffer, which stores one character per 2 bytes. Copying stops at the 0xE6 end
 * marker (not copied). An 0xE4 escape byte is copied together with the byte that
 * follows it. R4-R7 are preserved; R0/R1 and the flags are clobbered. */
Next textScript_copyFolderNameToWide_804A1FC(void) {
    u32 table = A_TextScriptFolderNames_inc;
    u32 src = table + LD16(table + (R0 << 1));
    u32 dst = R1;

    for (;;) {
        u32 ch = LD8(src);
        if (ch == 0xE6)
            break;
        if (ch == 0xE4) {
            /* two-byte escape sequence: copy it and the next byte verbatim */
            ST8(dst, ch);
            ST8(dst + 1, LD8(src + 1));
            src += 2;
        } else {
            ST8(dst, ch);
            src += 1;
        }
        dst += 2;
    }
    RETURN_TO_CALLER();
}

/* ── text_decompressMessageAssets_80471F8 ── */
/* text_decompressMessageAssets_80471F8: decompress the three LZ77-compressed blobs the message
 * system works from into their EWRAM buffers.
 *
 * The first blob's address is fetched through r5: r5->[0x1C]->[4]. The other
 * two are fixed data files. r4-r7 must come out unchanged. */
Next text_decompressMessageAssets_80471F8(void) {
    u32 sceneSrc = LD32(LD32(R5 + 0x1C) + 4);

    /* Blob reached through the r5 handle -> unk_2029A00. */
    R0 = sceneSrc;
    R1 = A_unk_2029A00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* CompText873DE4C -> eDecomp202BA00. */
    R0 = A_TextScript873D9FC_unk135;
    R1 = A_eDecomp202BA00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* CompText873ECC8 -> eDecompressedTextArchive202DA00. */
    R0 = A_CompText873ECC8;
    R1 = A_eDecompressedTextArchive202DA00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    RETURN_TO_CALLER();
}

/* ── NPCCommand_set_sprite ── */
/* Struct-layout constants for the OverworldNPCSpriteVariations lookup table
 * (entries of {u8 SpriteId, u8 HiddenOAMPieces, u8 PaletteIndex}, 3 bytes each,
 * terminated by a SpriteId of 0xFF). */
#define oOverworldNPCSpriteVariations_SpriteId        0
#define oOverworldNPCSpriteVariations_HiddenOAMPieces 1
#define oOverworldNPCSpriteVariations_PaletteIndex    2
#define oOverworldNPCSpriteVariations_Size            3

/* NPCCommand_set_sprite (0809EF00):
 * r5 = overworld NPC object, r6 = pointer into the NPC command stream.
 * The command's second byte (at [r6+1]) is a sprite id: it is written to the
 * object and looked up in OverworldNPCSpriteVariations. Special sprites
 * override the hidden-OAM piece count and palette index; the 0xFF terminator
 * (or any other sprite) leaves the defaults. The sprite category is forced to
 * 0x18 and the shadow flag is cleared. r6 is advanced past the 2-byte command;
 * the trailing ADDS sets nz/c/v for the caller. */
Next NPCCommand_set_sprite(void) {
    u32 npc = R5;
    u32 cmd = R6;

    u32 spriteId = LD8(cmd + 1);
    ST16(npc + oOverworldNPCObject_NPCSprite, spriteId);
    ST8(npc + oOverworldNPCObject_PaletteIndex, 0);
    ST32(npc + oOverworldNPCObject_HiddenOAMPieces, 0);

    u32 table = A_OverworldNPCSpriteVariations;
    u32 entry = oOverworldNPCSpriteVariations_SpriteId;
    u8 id;
    for (;;) {
        id = LD8(table + entry);
        R2 = id;
        if (id == spriteId || id == 0xFF)
            break;
        entry += oOverworldNPCSpriteVariations_Size;
    }

    if (id == spriteId) {
        /* Special sprite: copy its hidden-OAM count (stored shifted into the
         * top byte, as the original's lsl r0,#24 does) and palette index. */
        R3 = entry + oOverworldNPCSpriteVariations_HiddenOAMPieces;
        R0 = LD8(table + R3) << 24;
        ST32(npc + oOverworldNPCObject_HiddenOAMPieces, R0);
        R3 = entry + oOverworldNPCSpriteVariations_PaletteIndex;
        R0 = LD8(table + R3);
        ST8(npc + oOverworldNPCObject_PaletteIndex, R0);
    } else {
        R3 = entry;
    }

    R0 = 0x18;
    ST32(npc + oOverworldNPCObject_NPCSpriteCategory, R0);

    R0 = LD32(npc + oOverworldNPCObject_UnkFlags_60);
    R1 = OW_NPC_UNK_FLAGS_60_SHADOW_FLAG_0x80;
    R0 &= ~R1;
    ST32(npc + oOverworldNPCObject_UnkFlags_60, R0);

    R6 = cmd + 2;
    SET_FLAGS_ADD(cmd, 2);

    RETURN_TO_CALLER();
}
