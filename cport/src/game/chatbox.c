/*
 * chatbox.c: readable functions of bn6f chatbox.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── chatbox_uncompMapTextArchives_803FD08 ── */
/* chatbox_uncompMapTextArchives_803FD08: decompress the three chatbox text
 * archives for the current map into their fixed RAM buffers. Returns 0. */
Next chatbox_uncompMapTextArchives_803FD08(void) {
    /* Map-specific compressed text archive -> eDecompressedTextArchive202DA00. */
    R0 = 0;
    GUEST_CALL(chatbox_selectCompTextByMap_8040730);
    R1 = A_eDecompressedTextArchive202DA00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* Map-specific compressed text archive -> DecompressionBuf2033400. */
    GUEST_CALL(chatbox_selectCompTextByMap_8040794);
    R1 = A_DecompressionBuf2033400;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    /* Map-specific compressed text script archive -> eDecompressedTextArchive202FA00. */
    GUEST_CALL(chatbox_selectCompTextByMap_80407C8);
    R1 = A_eDecompressedTextArchive202FA00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── chatbox_uncompMapTextArchives_803FD3C ── */
/* chatbox_uncompMapTextArchives_803FD3C:
 * Decompress the map-area text archive. Loads compressed text entry #1 via
 * chatbox_selectCompTextByMap_8040730 and LZ77-expands it into the shared
 * decompression buffer at eDecompressionBuf2034A00. */
Next chatbox_uncompMapTextArchives_803FD3C(void) {
    /* Select the compressed map text (index 1) ... */
    R0 = 1;
    GUEST_CALL(chatbox_selectCompTextByMap_8040730);

    /* ... and expand it into the decompression buffer. */
    R1 = A_eDecompressionBuf2034A00;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── chatbox_uncompMapTextArchives_803FD54 ── */
/* chatbox_uncompMapTextArchives_803FD54:
 * Decompress the compressed text archive selected for the current map
 * (via chatbox_selectCompTextByMap_8040794) into the shared LZ77
 * decompression buffer at DecompressionBuf2033400. Always returns 0. */
Next chatbox_uncompMapTextArchives_803FD54(void) {
    /* R0 = pointer to the LZ77-compressed text archive for this map */
    GUEST_CALL(chatbox_selectCompTextByMap_8040794);

    /* R1 = destination buffer for the decompressed text */
    R1 = A_DecompressionBuf2033400;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);

    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_202da04 ── */
/* chatbox_runScript_202da04(r0 = script id):
 * Runs script `r0` from the fixed text archive eTextScript202DA04 by
 * forwarding to chatbox_runScript(archive, scriptID). */
Next chatbox_runScript_202da04(void) {
    R1 = R0;
    R0 = A_eTextScript202DA04;
    GUEST_CALL(chatbox_runScript);
    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_803FD78 ── */
/* chatbox_runScript_803FD78(archive = *TextScriptArchive, script_idx = u8) -> void
 *
 * Runs the chatbox script at archive[script_idx], then seeds chatbox->Unk_68
 * with the four-halfword next-script jump block from byte_803FD94 so subsequent
 * ticks fall through to the queued next command.
 */
Next chatbox_runScript_803FD78(void) {
    /* run the script with the incoming archive/script_idx in R0/R1 */
    GUEST_CALL(chatbox_runScript);

    /* src  = byte_803FD94 (four fixed halfwords in code memory)
     * dst  = &chatbox->Unk_68, where chatbox = R10->ChatboxPtr
     * copy 8 bytes (4 halfwords) */
    R0 = A_byte_803FD94;
    R1 = LD32(R10 + oToolkit_ChatboxPtr) + oChatbox_NextScriptDesc;
    R2 = 8;
    GUEST_CALL(CopyHalfwords);

    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_803FD9C ── */
/* chatbox_runScript_803FD9C(r0 = TextScriptArchive *, r1 = script index):
 *
 * Alternate entry point of the chatbox script runner. The full routine lives
 * at chatbox_runScript_803FDA4, whose shared body begins at loc_803FDA8; the
 * two entry points differ only in the initial value of the resume flag r4
 * (this one clears it, meaning "start the script from the top").
 *
 * This entry pushes {r4,r5,lr} before branching into the shared body, and the
 * body finishes with pop {r4,r5,pc}, so the frame must exist on the guest
 * stack: the body reads r5 and the return address from it and restores SP.
 */
Next chatbox_runScript_803FD9C(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_lr = LR;

    /* push {r4,r5,lr}: r4 at lowest address, lr at highest */
    u32 frame = SP - 12;
    SP = frame;
    ST32(frame, saved_r4);
    ST32(frame + 4, saved_r5);
    ST32(frame + 8, saved_lr);

    /* mov r4, #FALSE: no interrupted script to resume */
    R4 = 0;

    /* Tail-branch into the shared body of the script runner; it pops
     * {r4,r5,pc} on exit, returning straight to our caller. */
    GUEST_CALL(loc_803FDA8);

    RETURN_TO_CALLER();
}

/* ── chatbox_runScript_803FE08 ── */
/* chatbox_runScript_803FE08(r0 = TextScriptArchive *archive, r1 = scriptIdx):
 * Entry point for the chatbox script runner. It sets up the runner's state
 * flag (r4 = FALSE, i.e. "script not finished") and tail-branches into the
 * shared script-running body at loc_803FE14, which produces the outputs
 * (r5, r6, r7). Everything else — r0, r1, r5, r6, r7, r10 — is passed
 * straight through to the body.
 *
 * The original pushes {r4,r5,lr} before the branch and never pops it: the
 * tail-called body's epilogue consumes this frame, so the slots must really
 * be written to the guest stack (r4 at SP-12, r5 at SP-8, lr at SP-4). */
Next chatbox_runScript_803FE08(void) {
    u32 frame = SP -= 12;

    /* push {r4,r5,lr} */
    ST32(frame, R4);
    ST32(frame + 4, R5);
    ST32(frame + 8, LR);

    /* mov r4, #FALSE — start the script from its beginning. */
    R4 = 0;

    /* b loc_803FE14 — tail-call into the script runner proper; its
     * results in r5/r6/r7 are this function's results. */
    GUEST_CALL(loc_803FE14);
    RETURN_TO_CALLER();
}

/* ── chatbox_checkAdvanceTextPrint_8040154 ── */
/* chatbox_checkAdvanceTextPrint_8040154(r5 = chatbox):
 * Called each frame while the chatbox is printing text. If the player is
 * holding B or has just pressed A (or the text has no per-character print
 * delay), reset the chatbox's state-machine step to 0 so it moves on to the
 * next phase; otherwise leave it alone.
 * Returns r2 = 2 if B was held, r2 = 1 otherwise. */
Next chatbox_checkAdvanceTextPrint_8040154(void) {
    if (LD16(R5 + oChatbox_JoypadHeld) & 2) {   /* B currently held */
        R2 = 2;
        ST8(R5 + oChatbox_JumpTableOffset_11, 0);
        RETURN_TO_CALLER();
    }

    R2 = 1;
    /* A newly pressed, or the print speed is 0 (text completes instantly) */
    if ((LD16(R5 + oChatbox_JoypadUp) & 1) != 0 || LD8(R5 + 8) == 0) {
        ST8(R5 + oChatbox_JumpTableOffset_11, 0);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_emitTextGridOAM_804021C ── */
/* ROM data tables for the chatbox OAM emitter */
#define TILE_BASE          0xE2B8
#define COL_STRIDE         0x20
#define CHAR_TABLE_STRIDE  8

/* register form: calls sprite_queueOamEntryThunk_802FE28 outside this file, which has no typed C interface yet */
static void emit_oam(u32 attr01, u32 tile, u32 unused, u32 mode) {
    R0 = attr01; R1 = tile; R2 = unused; R3 = mode;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);
}

/* Emit 3 rows × 6 columns of OAM sprite entries for the chatbox text grid.
 * attr01 = (x << 16) | packed_y_attr | (row_pixel_offset + font_shift)
 * tile   = TILE_BASE + per-cell char offset from the layout table */
static void emit_chatbox_grid(u32 chatbox) {
    u32 text_coord   = LD8(chatbox + oChatbox_TextScriptCharIdx);
    u32 font_shift   = LD8(chatbox + oChatbox_CursorY);
    u32 y_table      = A_byte_8040314;
    u32 char_table   = A_byte_8040324;
    u32 row_off_tbl  = A_dword_8040340;

    for (u32 row = 0; row < 3; row++) {
        u32 y_attr = LD32(y_table + row * 4);
        u32 r_off  = LD8(row_off_tbl + row) + font_shift;

        for (u32 col = 0; col < 6; col++) {
            u32 x      = COL_STRIDE * col + text_coord;
            u32 attr01 = (x << 16) | y_attr | r_off;
            u32 tile   = TILE_BASE + LD8(char_table + row * CHAR_TABLE_STRIDE + col);
            emit_oam(attr01, tile, 0, 3);
        }
    }
}

/* chatbox_emitTextGridOAM_804021C(r5 = ChatBoxProperties*)
 * Outputs r1 = input r12 (the epilogue pop sequence leaves r1 with the
 * originally-pushed r12 value).  r2/r3 are whatever sprite_queueOamEntryThunk_802FE28 leaves
 * in them after the last GUEST_CALL. */
Next chatbox_emitTextGridOAM_804021C(void) {
    u32 saved_r12 = R12;
    emit_chatbox_grid(R5);
    R1 = saved_r12;
    SET_FLAGS_CMP(3, 3);
    RETURN_TO_CALLER();
}

/* ── chatbox_CopyBackgroundTiles_8040344 ── */
/* chatbox_CopyBackgroundTiles_8040344 (r5 = chatbox):
 * Redraws the chatbox background: pick the tile-id array matching the
 * chatbox's current background graphic and open state, then let
 * CopyBackgroundTiles blit it at the chatbox's current position/size.
 * Preserves r5; r1-r4 end up as CopyBackgroundTiles left them. */
Next chatbox_CopyBackgroundTiles_8040344(void) {
    u32 self = R5;

    /* Match the original frame exactly: while the callee runs, [SP] holds
     * the chatbox pointer and [SP + 4] the return address, so the callee
     * sees the same stack arguments as in the original. */
    SP -= 8;
    ST32(SP, self);
    ST32(SP + 4, LR);

    /* tileIds = spritePtrArr8045CEC[bgId * 8 + openState]
     * (8 pointer slots per background id, selected by the open state) */
    u32 bgId = LD8(self + 0x1D2);                       /* current background tileset id */
    u32 openState = LD8(self + oChatbox_OpenState_10);  /* which open-state variant */
    R3 = LD32(A_spritePtrArr8045CEC + (bgId << 5) + (openState << 2));

    R0 = LD8(self + oChatbox_CurTileXBlockPos); /* j: horizontal start position */
    R1 = LD8(self + oChatbox_CurTileYBlockPos); /* i: vertical start position */
    R2 = 0;                                     /* use 32x32 tile block #0 */
    R4 = LD8(self + oChatbox_CurTileWidth16);   /* j size in tiles */
    R5 = LD8(self + oChatbox_CurTileHeight16);  /* i size in tiles */

    GUEST_CALL(CopyBackgroundTiles);

    R5 = LD32(SP); /* pop {r5} - restore the chatbox pointer for our caller */
    SP += 8;       /* discard the saved-LR slot (pop {pc} is RETURN_TO_CALLER) */
    RETURN_TO_CALLER();
}

/* ── chatbox_runTrainScript ── */
/* chatbox_runTrainScript(r0 = script index):
 * Run one of the train-ride text scripts (eTextScript2034A04) by index via
 * the generic script runner. All other registers pass through to it. */
Next chatbox_runTrainScript(void) {
    u32 script_idx = R0;

    R1 = script_idx;
    R0 = A_eTextScript2034A04;
    GUEST_CALL(chatbox_runScript);

    RETURN_TO_CALLER();
}

/* ── chatbox_runScript ── */
#define CHATBOX_SIZE                0x230
#define FLAGS_3E_HIDE_CHATBOX_chatbox_runScript       0x100
#define GFX_TRANSFER_SIZE           0x160
#define TILE_COPY_SIZE              0x20
#define CHATBOX_PAGE_STATE_IDX      0x9C
#define CHATBOX_COPY_CHAR_OFFSET    0x94
#define CHATBOX_TILE_GEOM_OFFSET    0x68

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill(u32 mem, u32 size) {
    R0 = mem;
    R1 = size;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls CopyHalfwords outside this file, which has no typed C interface yet */
static void copy_halfwords(u32 src, u32 dest, u32 count) {
    R0 = src;
    R1 = dest;
    R2 = count;
    GUEST_CALL(CopyHalfwords);
}

/* register form: calls chatbox_set_eFlags2009F38 outside this file, which has no typed C interface yet */
static void set_eflags_2009f38(u32 val, u32 flags_addr, u32 tile_width) {
    R0 = val;
    R1 = flags_addr;
    R2 = tile_width;
    GUEST_CALL(chatbox_set_eFlags2009F38);
}

/* register form: chatbox_resetAndClearBuffers reads R3 (page counter) and R5 (chatbox ptr) as context */
static void reset_and_clear_buffers(u32 chatbox, u32 page_count) {
    R2 = 0x1E;
    R3 = page_count;
    R5 = chatbox;
    GUEST_CALL(chatbox_resetAndClearBuffers_8045F60);
}

/* register form: calls chatbox_setflags_3e outside this file, which has no typed C interface yet */
static void set_flags_3e(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_setflags_3e);
}

/* register form: QueueEightWordAlignedGFXTransfer reads R4/R6 as preserved context values */
static void queue_gfx_transfer(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls CopyByEightWords outside this file, which has no typed C interface yet */
static void copy_by_eight_words(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(CopyByEightWords);
}

/* Reset the chatbox, load a script entry from a TextScriptArchive, configure display defaults,
 * queue tilemap transfer, and copy tile/font data into work buffers. */
static void chatbox_runScript_impl(u32 toolkit, u32 archive, u32 script_idx) {
    u32 chatbox = LD32(toolkit + oToolkit_ChatboxPtr);

    /* Preserve four fields across the zero-fill that resets the rest of the struct. */
    u32 saved_4C = LD32(chatbox + oChatbox_TextParam1);
    u32 saved_50 = LD32(chatbox + oChatbox_TextParam2);
    u32 saved_54 = LD32(chatbox + oChatbox_ParamC);
    u32 saved_58 = LD32(chatbox + oChatbox_TextParam4);

    zero_fill(chatbox, CHATBOX_SIZE);

    ST32(chatbox + oChatbox_TextParam1, saved_4C);
    ST32(chatbox + oChatbox_TextParam2, saved_50);
    ST32(chatbox + oChatbox_ParamC, saved_54);
    ST32(chatbox + oChatbox_TextParam4, saved_58);

    /* Store the script archive pointer and selected index. */
    ST32(chatbox + oChatbox_TextScriptPtr, archive);
    ST8(chatbox + 1, script_idx);
    ST8(chatbox + CHATBOX_PAGE_STATE_IDX, script_idx);

    /* Resolve the per-script byte offset from the archive's u16 index table. */
    u32 cursor = archive + LD16(archive + (script_idx << 1));
    ST32(chatbox + oChatbox_TextScriptCursorPtr, cursor);
    ST32(chatbox + oChatbox_CurrTextScriptPtr, cursor);

    /* Configure initial display parameters. */
    ST8(chatbox, 1);
    ST8(chatbox + 8, 2);
    ST32(chatbox + oChatbox_TextScriptSourcePtr, A_unk_200B2B0);
    ST8(chatbox + oChatbox_BoxYX, 1);
    ST32(chatbox + oChatbox_ScriptFlags, 1);
    ST8(chatbox + oChatbox_JumpTableOffset_11, 1);
    ST32(chatbox + oChatbox_PrintDelay, 4);

    /* Copy the tile-geometry constants (8 halfwords from ROM) into the chatbox. */
    copy_halfwords(A_byte_8040494, chatbox + CHATBOX_TILE_GEOM_OFFSET, 8);

    /* Tile coordinate and cursor defaults. */
    ST8(chatbox + oChatbox_CurTileYBlockPos, 0xC);
    ST8(chatbox + oChatbox_CurTileWidth16, 0x1E);
    ST8(chatbox + oChatbox_CurTileHeight16, 8);
    ST8(chatbox + oChatbox_CursorIdx, 0xE2);
    ST8(chatbox + oChatbox_CursorX, 0x8D);
    ST8(chatbox + oChatbox_TextScriptCharIdx, 0x33);
    ST8(chatbox + oChatbox_CursorY, 0x6C);

    /* Additional per-script state slots. */
    ST32(chatbox + 0x84, 0x19);
    ST32(chatbox + 0x88, 0x80);

    /* Script-phase state bytes (idle pair values at 0x1F0..0x1F5). */
    ST8(chatbox + 0x1F0, 2);
    ST8(chatbox + 0x1F1, 2);
    ST8(chatbox + 0x1F2, 0);
    ST8(chatbox + 0x1F3, 0);
    ST8(chatbox + 0x1F4, 3);
    ST8(chatbox + 0x1F5, 3);

    /* Clear the text-event flag word, mark it dirty, then reset buffers. */
    ST32(A_eFlags2009F38, 0);
    set_eflags_2009f38(0x80, A_eFlags2009F38, 0x1E);
    reset_and_clear_buffers(chatbox, 8);
    set_flags_3e(FLAGS_3E_HIDE_CHATBOX_chatbox_runScript);

    /* Mirror the character index and set the page-transfer counter. */
    ST8(chatbox + CHATBOX_COPY_CHAR_OFFSET, LD8(chatbox + oChatbox_TextScriptCharIdx));
    ST32(chatbox + oChatbox_InitialUnk_90, 0xD);

    /* Queue the tilemap BG transfer and copy tile/font data. */
    queue_gfx_transfer(A_dword_86BEB20, 0x600DC80, GFX_TRANSFER_SIZE);
    copy_by_eight_words(A_byte_86BEC80, A_unk_3001B40, TILE_COPY_SIZE);
    copy_by_eight_words(A_dword_86B7AC0, A_byte_3001710, TILE_COPY_SIZE);
}

/* chatbox_runScript(r0 = archive, r1 = script_idx, r10 = toolkit) -> void */
Next chatbox_runScript(void) {
    u32 sr4 = R4, sr5 = R5, sr6 = R6, sr7 = R7;
    chatbox_runScript_impl(R10, R0, R1);
    R4 = sr4; R5 = sr5; R6 = sr6; R7 = sr7;
    RETURN_TO_CALLER();
}

/* ── chatbox_selectCompTextByMap_8040730 ── */
/* Pick the compressed text script that matches the map the player is standing on.
 *
 * off_8040770 holds two rows of two table roots:
 *   row 0 (idx 0): normal per-map completion text   (real world / internet)
 *   row 1 (idx 1): L-button textscripts             (real world / internet)
 * Within a row the lookup goes [world][map group] and, for the normal tables
 * only, one level further down by [map number]; the final index into the
 * resulting pointer array is oGameState_ChatboxMessageSlot. */
typedef struct {
    u32 text;      /* r0: the selected LZ77-compressed text script pointer */
    u32 slotTable; /* r3 before the final index is added, kept for the flags */
    u32 slotOffset;/* r2 as the original leaves it: 4 * oGameState_ChatboxMessageSlot */
    u32 slotPtr;   /* r3 as the original leaves it: &table[Unk_08] */
} CompTextLookup;

static CompTextLookup select_comp_text_by_map_chatbox_selectCompTextByMap_8040730(u32 idx, u32 toolkit)
{
    CompTextLookup out;

    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 mapGroup  = LD8(gameState + oGameState_MapGroup);
    u32 mapNumber = LD8(gameState + oGameState_MapNumber);
    u32 textSlot  = LD8(gameState + oGameState_ChatboxMessageSlot);

    /* Real-world and internet areas use separate halves of every table. */
    u32 worldOffset = 0;
    if ((s32)mapGroup >= INTERNET_MAP_GROUP_START) {
        worldOffset = 4;
        mapGroup -= INTERNET_MAP_GROUP_START;
    }

    /* Each idx selects one row of two roots in off_8040770. */
    u32 rowOffset  = 8 * idx;
    u32 groupTable = LD32(A_off_8040770 + rowOffset + worldOffset);
    u32 slotTable  = LD32(groupTable + 4 * mapGroup);

    /* The normal tables narrow down by map number inside each group first;
     * the L-button tables point straight at the per-slot pointer arrays. */
    if (rowOffset == 0)
        slotTable = LD32(slotTable + 4 * mapNumber);

    out.slotOffset = 4 * textSlot;
    out.slotTable  = slotTable;
    out.slotPtr    = slotTable + out.slotOffset;
    out.text       = LD32(out.slotPtr);
    return out;
}

/* chatbox_selectCompTextByMap_8040730(r0 = idx) -> r0 = text script,
 * r2 = 4 * Unk_08, r3 = &table[Unk_08], flags set as by ADDS r3, r2.
 * r4-r7 are only pushed/popped by the original, so they pass through here
 * untouched. */
Next chatbox_selectCompTextByMap_8040730(void) {
    CompTextLookup out = select_comp_text_by_map_chatbox_selectCompTextByMap_8040730(R0, R10);
    R0 = out.text;
    R2 = out.slotOffset;
    R3 = out.slotPtr;
    SET_FLAGS_ADD(out.slotTable, out.slotOffset);
    RETURN_TO_CALLER();
}

/* ── chatbox_selectCompTextByMap_80407C8 ── */
/* Selects the compressed chat text for the current map.
 *
 * The text tables are three nested arrays: one pointer table per world
 * (real world / internet, picked by whether the map group is >= 0x80,
 * with 0x80 subtracted to index into the internet table), then one entry
 * per map group, then one entry per map number, giving an array of five
 * script pointers indexed by story progress. This lookup always uses
 * progress stage 4.
 *
 * The original leaves r3 pointing at the table slot it loaded the text
 * pointer from and r2 = 16 (the stage-4 stride it just added); callers
 * branch on the flags from that address computation, so both are returned. */
typedef struct {
    u32 text;        /* *const LZ77Compressed<TextScriptArchive> for this map */
    u32 slotAddress; /* guest address of the table slot holding the pointer */
} CompTextSelection;

static CompTextSelection select_comp_text_by_map_chatbox_selectCompTextByMap_80407C8(u32 toolkit)
{
    CompTextSelection result;

    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 group     = LD8(gameState + oGameState_MapGroup);
    u32 mapNumber = LD8(gameState + oGameState_MapNumber);

    /* Pick the world table; internet groups are numbered from 0x80. */
    u32 worldIndex = 0;
    if (group >= INTERNET_MAP_GROUP_START) {
        worldIndex = 1;
        group -= INTERNET_MAP_GROUP_START;
    }

    u32 worldTable = LD32(A_mapPtrs80407FC + worldIndex * 4);
    u32 groupTable = LD32(worldTable + group * 4);
    u32 mapEntry   = LD32(groupTable + mapNumber * 4);

    u32 slotAddress = mapEntry + 4 * 4; /* stage 4 = last story progress */
    result.text        = LD32(slotAddress);
    result.slotAddress = slotAddress;
    return result;
}

/* chatbox_selectCompTextByMap_80407C8
 * (r10 = toolkit) -> r0 = text script, r2 = 16, r3 = slot address, flags nz/c/v */
Next chatbox_selectCompTextByMap_80407C8(void) {
    CompTextSelection sel = select_comp_text_by_map_chatbox_selectCompTextByMap_80407C8(R10);
    R0 = sel.text;
    R2 = 16;
    R3 = sel.slotAddress;
    SET_FLAGS_ADD(sel.slotAddress, 16);
    RETURN_TO_CALLER();
}

/* ── chatbox_config_80408D0 ── */
/* register form: calls chatbox_resetAndClearBuffers_8045F60 outside this file, which has no typed C interface yet */
static void chatbox_reset_buffers(u32 chatbox) {
    R0 = chatbox;
    GUEST_CALL(chatbox_resetAndClearBuffers_8045F60);
}

/* register form: calls chatbox_clearFlags_3e outside this file, which has no typed C interface yet */
static void chatbox_clear_flags_3e(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_clearFlags_3e);
}

/* Configure the chatbox state for the given text script index.
 * Returns the script pointer (r4) and the flag value loaded into r1 just before the ORR. */
static u32 chatbox_config(u32 chatbox, u32 script_idx, u32 *out_r1) {
    /* Record the requested script entry. */
    ST8(chatbox + oChatbox_TextScriptIdx, (u8)script_idx);

    /* Resolve the script cursor from the array of u16 offsets at TextScriptPtr. */
    u32 script_array = LD32(chatbox + oChatbox_TextScriptPtr);
    u16 rel = LD16(script_array + script_idx * 2);
    u32 script_ptr = script_array + rel;

    ST32(chatbox + oChatbox_TextScriptCursorPtr, script_ptr);
    ST32(chatbox + oChatbox_CurrTextScriptPtr, script_ptr);

    /* Clear the small script-state fields. */
    ST8(chatbox + oChatbox_TextScriptState_04, 0);
    ST32(chatbox + oChatbox_TextScriptTimer, 0);
    ST8(chatbox + oChatbox_ChoicesPageTag, 0);
    ST8(chatbox + oChatbox_CurrLine, 0);
    ST8(chatbox + oChatbox_MugshotAnimTableIdx, 0);
    ST8(chatbox + 0x3D, 0);
    ST32(chatbox + 0x80, 0);

    /* Set jump-table offset / Unk_74 to 1. */
    ST8(chatbox + oChatbox_JumpTableOffset_11, 1);
    ST32(chatbox + oChatbox_ScriptFlags, 1);

    /* Mirror the current text-coordinate into unk_94. */
    u32 text_coord = LD8(chatbox + oChatbox_TextScriptCharIdx);
    ST8(chatbox + 0x94, (u8)text_coord);

    /* Reset the chatbox's internal buffers. */
    chatbox_reset_buffers(chatbox);

    /* Clear the two unrelated chatbox flags. */
    u32 mask = FLAGS_3E_UNK_0400 | FLAGS_3E_UNK_0800;
    chatbox_clear_flags_3e(mask);

    /* r1 holds the second flag constant at the moment of the final ORR; r4 holds script_ptr. */
    *out_r1 = FLAGS_3E_UNK_0800;
    return script_ptr;
}

Next chatbox_config_80408D0(void) {
    u32 r1_out;
    R4 = chatbox_config(R5, R1, &r1_out);
    R1 = r1_out;
    /* Final flag-setter is `orr r0, r1` with the OR'd mask: sets N/Z only (no C/V effect).
     * C/V are unchanged by ORR; the original carries whatever state they had before.
     * Match the original by performing the same ORR: NZ reflect the result. */
    SET_FLAGS_NZ(FLAGS_3E_UNK_0400 | FLAGS_3E_UNK_0800);
    RETURN_TO_CALLER();
}

/* ── chatbox_ED_select_8040944 ── */
/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_eight_word_aligned_gfx_transfer(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls sprite_queueOamEntryThunk_802FE28 outside this file, which has no typed C interface yet */
static void call_sub_802FE28(u32 a1, u32 a2, u32 not_used, u32 a4) {
    R0 = a1;
    R1 = a2;
    R2 = not_used;
    R3 = a4;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);
}

/* Drives one frame of the chatbox ED-select animation: streams a tile row into VRAM,
 * advances the frame index (wrapping to 1 at end), and queues the OAM DMA. */
static void chatbox_ed_select_load_frame(u32 chatbox) {
    u32 src_idx = LD8(chatbox + oChatbox_SelectCursorFrame);
    u32 src = ((u32)LD8(A_dword_80409B2 + src_idx) << 7) + A_byte_86A48C0;
    queue_eight_word_aligned_gfx_transfer(src, 0x6017F80u, 0x80u);

    u32 row_index = LD8(chatbox + 0x94);
    u32 x_origin = (u32)(LD32(chatbox + oChatbox_TileBufferOffset) + row_index - 6);
    u32 y_origin = (u32)LD8(chatbox + oChatbox_BoxGfxLoadState_0F) * 0xEu
                 + (u32)LD8(chatbox + oChatbox_CursorY) - 1u;

    /* Advance the frame counter; if the next table entry is 0xFF, wrap to slot 1
     * and pull the tile id from slot 0. Otherwise use the looked-up tile id. */
    u32 frame = LD8(chatbox + oChatbox_SelectCursorFrame);
    u32 next_frame = frame + 1u;
    u32 frame_to_store = next_frame;
    u32 tile_id;
    if (LD8(A_dword_80409B2 + next_frame) == 0xFFu) {
        frame_to_store = 1u;
        tile_id = (u32)LD16(A_dword_80409B2);
    } else {
        tile_id = LD8(A_dword_80409B2 + next_frame);
    }
    ST8(chatbox + oChatbox_SelectCursorFrame, (u8)frame_to_store);

    /* Build OAM attr0 (y in bits 16..23) and attr1 (tile id in bits 0..11 + affine bit
     * merged in via the 0x40000000 OR on attr0 to set the affine flag). */
    u32 attr01 = (x_origin << 16) | y_origin;
    attr01 |= 0x40000000u;
    u16 attr2 = (u16)(0xE3FCu | (0u << 10));
    call_sub_802FE28(attr01, attr2, 0u, 4u);
    (void)tile_id;
}

Next chatbox_ED_select_8040944(void) {
    chatbox_ed_select_load_frame(R5);
    RETURN_TO_CALLER();
}

/* ── chatbox_loadMugshotPortrait_8040B3A ── */
/* The chatbox object (r5 on entry) keeps the sprite it displays in a
 * sub-structure this far into the object. */
#define CHATBOX_SPRITE_OFFSET 0x1F8

/* register form: calls spriteLoadMugshot_800275A outside this file, which has no typed C interface yet */
static void loadMugshotGraphics(u32 mugshotId, u32 attr01, u32 spriteObj) {
    R0 = mugshotId;
    R1 = attr01;                /* r1 is still live across this call in the original */
    R2 = CHATBOX_SPRITE_OFFSET; /* so is r2, still holding the sub-struct offset */
    R5 = spriteObj;
    GUEST_CALL(spriteLoadMugshot_800275A);
}

/* register form: calls sprite_loadAnimationData_spritePtrProvidedDirectly outside this file, which has no typed C interface yet */
static u32 loadSpriteAnimationData(u32 spriteId, u32 spriteObj, u32 chatbox) {
    R0 = spriteId;
    R1 = 6;    /* r1 still holds the value the original stored at sprite+3 */
    R2 = 0x90; /* r2 still holds the chatbox field offset 0x90 */
    R5 = spriteObj;
    R7 = chatbox;
    GUEST_CALL(sprite_loadAnimationData_spritePtrProvidedDirectly);
    return R2; /* callers of the original observe r2 as the loader left it */
}

/* Values callers of the original observe in r1/r2 on return
 * (r3 is whatever the animation loader left in it, reproduced by the
 * guest call itself; r4/r5 pass through unchanged). */
typedef struct {
    u32 r1; /* ends up holding chatbox field 0x88, the value stored at sprite+0xE */
    u32 r2; /* whatever sprite_loadAnimationData left in r2 */
} MugshotShowResult;

/* Set up the chatbox's sprite to show a character mugshot: load the portrait
 * graphics, reset the sprite's state fields and point it at the mugshot's
 * animation data. */
static MugshotShowResult showMugshotInChatbox(u32 mugshotId, u32 attr01, u32 chatbox) {
    u32 sprite = chatbox + CHATBOX_SPRITE_OFFSET;

    loadMugshotGraphics(mugshotId, attr01, sprite);

    ST8(sprite + 0x4, attr01);
    ST16(sprite + 0xA, 3);
    ST8(sprite + 0x0, 0); /* sprite state: idle */
    ST8(sprite + 0x3, 6);
    ST16(sprite + 0x14, 0);

    /* If the chatbox was already in state 3, carry that state over. */
    u8 chatboxState = LD8(chatbox + oChatbox_ChoiceNumOptions);
    if (chatboxState == 3)
        ST8(sprite + 0x0, chatboxState);

    /* Point the sprite at the mugshot's graphics: chatbox field 0x90 holds a
     * resource number that is shifted into a byte offset. */
    u32 spriteId = LD32(chatbox + 0x90) << 4;
    ST8(sprite + 0x15, spriteId);
    u32 r2AfterLoad = loadSpriteAnimationData(spriteId, sprite, chatbox);

    ST16(sprite + 0x8, 0x3C0); /* fixed tile/attribute base */
    ST16(sprite + 0xC, LD32(chatbox + 0x84));
    u32 field88 = LD32(chatbox + 0x88);
    ST8(sprite + 0xE, field88);

    MugshotShowResult out;
    out.r1 = field88;
    out.r2 = r2AfterLoad;
    return out;
}

/* chatbox_loadMugshotPortrait_8040B3A(r0 = mugshotId, r1 = attr01, r4 = unused, r5 = chatbox)
 * -> r1 r2 r3 r7 (r4/r5 preserved) */
Next chatbox_loadMugshotPortrait_8040B3A(void) {
    u32 chatbox = R5;
    /* r4 is never touched by the original, so it passes through unchanged. */
    MugshotShowResult out = showMugshotInChatbox(R0, R1, chatbox);
    R1 = out.r1;
    R2 = out.r2;
    R5 = chatbox; /* the original restores r5; r7 ends up pointing at the chatbox */
    R7 = chatbox;
    RETURN_TO_CALLER();
}

/* ── chatbox_8040B8C ── */
/* Chatbox mugshot fade filter step + sprite render sequence.
 * Original saves r4/r5/r6 via push/pop; the adapter takes care of that. */

#define FLAG_MUGSHOT_ACTIVE  2
#define FLAG_UNK_0004        4
#define FLAG_UNK_0008        8
#define FILTER_DEC           0x421
#define FILTER_INC           0x842
#define OPEN_STATE_OFFSET    oChatbox_OpenState_10
#define FILTER_OFFSET        oChatbox_MugshotColorFilter_40
#define PALETTE_IDX_OFFSET   oChatbox_MugshotPaletteIdx
#define CUR_FRAME_OFFSET     0x1F2
#define PREV_FRAME_OFFSET    0x1F3
#define ANIM_ID_OFFSET       0x1F0
#define SPRITE_BASE_OFFSET   0x1F8
#define SPRITE_ANIM_ID_SLOT  0
#define SPRITE_PALETTE_SLOT  4
#define SPRITE_FILTER_HWORD  6

/* register form: chatbox_maskFlags_3e (no typed C interface) returns Z set when mask is unset */
static bool flag_3e_is_set(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_maskFlags_3e);
    return !ZERO_FLAG();
}

/* register form: chatbox_clearFlags_3e (no typed C interface), r1 = companion value */
static void flag_3e_clear(u32 mask, u32 companion) {
    R0 = mask;
    R1 = companion;
    GUEST_CALL(chatbox_clearFlags_3e);
}

/* register form: sprite_render_sequence chains several sprite helpers that thread register
 * state between calls. Reproduces the original sequence exactly. */
static void sprite_render_sequence(u32 chatbox, u32 sprite_ptr,
                                   u32 adjusted_filter, u32 orig_filter) {
    /* store palette index into the sprite header */
    ST8(sprite_ptr + SPRITE_PALETTE_SLOT, LD8(chatbox + PALETTE_IDX_OFFSET));

    u32 cur  = LD8(chatbox + CUR_FRAME_OFFSET);
    u32 prev = LD8(chatbox + PREV_FRAME_OFFSET);
    if (cur != prev) {
        ST8(chatbox + PREV_FRAME_OFFSET, (u8)cur);
        ST8(sprite_ptr + SPRITE_ANIM_ID_SLOT, LD8(chatbox + ANIM_ID_OFFSET));
        R5 = sprite_ptr;
        GUEST_CALL(sprite_loadAnimationData_spritePtrProvidedDirectly);
    }

    R5 = sprite_ptr;
    GUEST_CALL(sprite_chatbox_80026D6);

    ST16(sprite_ptr + SPRITE_FILTER_HWORD, (u16)orig_filter);

    R5 = sprite_ptr;
    GUEST_CALL(sprite_loadPalette_8002818);

    R5 = sprite_ptr;
    GUEST_CALL(sprite_loadAnimFrameTiles_8002874);

    R0 = 0;
    R1 = 3;
    R5 = sprite_ptr;
    GUEST_CALL(sprite_queueFramePiecesViaSub_8002694);
}

/* chatbox_update_mugshot: drive one step of the mugshot fade and render the sprite. */
static void chatbox_update_mugshot(u32 chatbox) {
    if (!flag_3e_is_set(FLAG_MUGSHOT_ACTIVE))
        return;

    if (!flag_3e_is_set(0x100)) {
        if (LD8(chatbox + OPEN_STATE_OFFSET) != 3)
            return;
    }

    u32 orig_filter = LD32(chatbox + FILTER_OFFSET);
    u32 adjusted = orig_filter;

    if (flag_3e_is_set(FLAG_UNK_0004)) {
        s32 dec = (s32)orig_filter - (s32)FILTER_DEC;
        if (dec < 0) {
            adjusted = 0;
            flag_3e_clear(FLAG_UNK_0004, FILTER_DEC);
        } else {
            adjusted = (u32)dec;
        }
        ST32(chatbox + FILTER_OFFSET, adjusted);
    } else if (flag_3e_is_set(FLAG_UNK_0008)) {
        u32 inc = orig_filter + FILTER_INC;
        adjusted = inc;
        u32 low5 = inc & 0x1F;
        if ((s32)low5 >= 6) {
            flag_3e_clear(FLAG_UNK_0008 | FLAG_MUGSHOT_ACTIVE, 0x1F);
        }
        ST32(chatbox + FILTER_OFFSET, adjusted);
    }

    u32 sprite_ptr = chatbox + SPRITE_BASE_OFFSET;
    sprite_render_sequence(chatbox, sprite_ptr, adjusted, orig_filter);
}

Next chatbox_8040B8C(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6;
    chatbox_update_mugshot(R5);
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── chatbox_8040C44 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; bool z, n; } call_chatbox_maskFlags_3e_result;
/* register form: generated wrapper: chatbox_maskFlags_3e has no typed C interface yet */
static inline call_chatbox_maskFlags_3e_result call_chatbox_maskFlags_3e(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(chatbox_maskFlags_3e);
    return (call_chatbox_maskFlags_3e_result){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Chatbox character handler (0x08040C44).
 *
 * r5 points at the chatbox state struct. The function first runs
 * chatbox_maskFlags_3e with mask 0x10 to refresh the control-code mask;
 * that call's Z flag and its returned r1 classify the pending character:
 *
 *   control character (Z set by the mask call, or r1 == 0, r1 == 0x25,
 *   0x40 <= signed r1 < 0x59, or signed r1 >= 0x5D):
 *     clear the auto-advance byte at +0x1F2, then step the text-advance
 *     state: next (+0x1F0) = cur (+0x1F1) with 2 mapped to 1, unless
 *     next is already 3 (chat finished - left alone).
 *
 *   printable character (signed r1 < 0x40 excluding 0/0x25, or
 *   0x5A..0x5C):
 *     set the auto-advance byte to 1 and copy cur into next.
 *
 * r0/r1/r4/r5 are preserved (the original pushes/pops them). r2 ends as
 * 0x1F0, the last literal loaded on both paths. r3 ends as the pre-store
 * value of +0x1F0 on the control path, and as whatever
 * chatbox_maskFlags_3e left behind on the printable path. Final flags:
 * control path - CMP r3,#3; printable path - N/Z from `mov r1,#1`
 * (N=0, Z=0) with C/V still standing from CMP r1,#0x5D.
 */

/* Offsets into the chatbox state struct; the original loads these from its
 * literal pool at 0x08040C90..0x08040C98. */
#define CB_NEXT_STATE 0x1F0u /* [r5+0x1F0] next text-advance state */
#define CB_CUR_STATE  0x1F1u /* [r5+0x1F1] current text-advance state */
#define CB_ADVANCE    0x1F2u /* [r5+0x1F2] auto-advance flag */

/* Everything the thin adapter needs: the output registers, the operands of
 * the final CMP, the value whose N/Z match the original's final N/Z, and a
 * mask telling it whether r3 was computed here or must keep the value the
 * mask call left in the guest register. */
struct chatbox_8040C44_out {
    u32 r0, r1, r2, r3, r4, r5;
    u32 r3_keep_mask;  /* 0xFFFFFFFF: r3 is our loaded byte; 0: keep callee's r3 */
    u32 cmp_a, cmp_b;  /* operands of the last CMP the original executes */
    u32 nz_src;        /* value whose N/Z reproduce the original's final N/Z */
};

static struct chatbox_8040C44_out chatbox_8040C44_body(u32 in_r0, u32 in_r1, u32 in_r4, u32 in_r5)
{
    struct chatbox_8040C44_out out;

    /* Refresh the control-code mask state for mask 0x10. */
    call_chatbox_maskFlags_3e_result mask = call_chatbox_maskFlags_3e(0x10, in_r5);
    u32 ch = mask.r1;

    /* Classify the character exactly as the original's branch chain does;
     * all the range checks are signed, on the full 32-bit r1. */
    bool control;
    if (!mask.z)                        /* bne: the mask flagged this byte */
        control = true;
    else if (ch == 0 || ch == 0x25)     /* NUL / '%' */
        control = true;
    else if ((s32)ch < (s32)0x40)
        control = false;                /* 0x01..0x3F except 0x25, or negative */
    else if ((s32)ch < (s32)0x59)
        control = true;
    else if ((s32)ch < (s32)0x5D)
        control = false;                /* 0x5A..0x5C */
    else
        control = true;

    u32 cur = LD8(in_r5 + CB_CUR_STATE);

    if (control) {
        /* Stop auto-advancing, then step the state machine unless the
         * chat is already finished (next state 3). */
        ST8(in_r5 + CB_ADVANCE, 0);
        u32 step = (cur == 2) ? 1 : cur;
        u32 old_next = LD8(in_r5 + CB_NEXT_STATE);
        if (old_next != 3)
            ST8(in_r5 + CB_NEXT_STATE, step);

        /* Final flags come from `cmp r3, #3` on the pre-store byte. */
        out.r3 = old_next;
        out.r3_keep_mask = 0xFFFFFFFFu;
        out.cmp_a = old_next;
        out.cmp_b = 3;
        out.nz_src = (old_next == 3) ? 0u : (old_next < 3 ? 0x80000000u : 1u);
    } else {
        /* Keep advancing; the current state becomes the next one. */
        ST8(in_r5 + CB_ADVANCE, 1);
        ST8(in_r5 + CB_NEXT_STATE, cur);

        /* C/V still come from the `cmp r1, #0x5D` that chose this path;
         * N/Z come from the `mov r1, #1` that follows it. r3 stays as the
         * callee left it. */
        out.r3 = 0;
        out.r3_keep_mask = 0;
        out.cmp_a = ch;
        out.cmp_b = 0x5D;
        out.nz_src = 1;
    }

    out.r0 = in_r0;
    out.r1 = in_r1;
    out.r2 = CB_NEXT_STATE;
    out.r4 = in_r4;
    out.r5 = in_r5;
    return out;
}

Next chatbox_8040C44(void) {
    struct chatbox_8040C44_out out = chatbox_8040C44_body(R0, R1, R4, R5);
    R0 = out.r0, R1 = out.r1, R2 = out.r2, R4 = out.r4, R5 = out.r5;
    /* r3: computed by the control path; on the printable path the wrapper's
     * call already left the callee's value in the guest register. */
    R3 = (out.r3 & out.r3_keep_mask) | (R3 & ~out.r3_keep_mask);
    SET_FLAGS_CMP(out.cmp_a, out.cmp_b);
    SET_FLAGS_NZ(out.nz_src);
    RETURN_TO_CALLER();
}

/* ── chatbox_advanceNextState_8040C9C ── */
/* Chatbox state offsets within the chatbox work area (r5). */
#define CHATBOX_NEXT_STATE  0x1F0  /* mode/state byte this function updates */
#define CHATBOX_1F1         0x1F1  /* read by the original, result unused */
#define CHATBOX_1F2         0x1F2  /* cleared when restarting from a mid state */

/* chatbox_advanceNextState_8040C9C(r5 = chatbox work area):
 * Advance the chatbox state byte at +0x1F0. State 0 stays 0, state 3 stays 3,
 * any other state clears +0x1F2 (touching +0x1F1 as the original does) and
 * moves to state 1.
 * r0/r1 are preserved; c/v are left as the original's last CMP leaves them
 * (CMP #0 when the state was 0, CMP #3 otherwise). */
Next chatbox_advanceNextState_8040C9C(void) {
    u32 savedR0 = R0;
    u32 savedR1 = R1;

    u32 state = LD8(R5 + CHATBOX_NEXT_STATE);

    if (state == 0) {
        /* Fresh chatbox: nothing to do, state stays 0. */
        SET_FLAGS_CMP(state, 0);
        R0 = 0;
    } else if (state == 3) {
        /* Already fully advanced: hold at 3. */
        SET_FLAGS_CMP(state, 3);
        R0 = 3;
    } else {
        /* Interrupted mid-way: reset the sub-state and restart at 1. */
        SET_FLAGS_CMP(state, 3);
        ST8(R5 + CHATBOX_1F2, 0);
        (void)LD8(R5 + CHATBOX_1F1); /* read for its side effects only; result discarded */
        R0 = 1;
    }

    ST8(R5 + CHATBOX_NEXT_STATE, R0);

    /* The original pushed r0/r1 and popped them on return. */
    R0 = savedR0;
    R1 = savedR1;
    RETURN_TO_CALLER();
}

#undef CHATBOX_NEXT_STATE
#undef CHATBOX_1F1
#undef CHATBOX_1F2

/* ── chatbox_clearWorkBuffers_8040DDC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_ZeroFillByWord_result;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result call_ZeroFillByWord(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result){ R3 };
}

/* Clear the chatbox work buffers at unk_200BEA0 (0x60 bytes per slot, 14 slots).
 *
 * The caller passes a size/index in r0; the original scales it by 8
 * (mul 0x10, lsr 1) and then calls ZeroFillByWord 14 times, once per slot,
 * always with that same scaled size. The fills overlap: slot i covers
 * base + i*0x60 .. +size, exactly as in the original.
 *
 * The original wraps everything in push {r0-r5,lr} / pop {r0-r5,pc}, so
 * r0-r3 leave exactly as they entered even though ZeroFillByWord's wrapper
 * clobbers r0/r1 (and leaves the callee's r3 behind); the adapter saves and
 * restores them around the call. */
static void clear_chatbox_work_buffers(u32 index) {
    u32 fill_size = index * 0x10 / 2;   /* mov r4,r0; mul r4,#0x10; lsr r4,#1 */
    u32 slot;

    for (slot = 0; (s32)slot <= 0xD; slot++)
        call_ZeroFillByWord(A_unk_200BEA0 + slot * 0x60, fill_size);
}

/* chatbox_clearWorkBuffers_8040DDC(r0 = size/index) -> r0-r5 preserved (push/pop in original) */
Next chatbox_clearWorkBuffers_8040DDC(void) {
    u32 in0 = R0, in1 = R1, in2 = R2, in3 = R3;
    clear_chatbox_work_buffers(in0);
    R0 = in0; R1 = in1; R2 = in2; R3 = in3;
    RETURN_TO_CALLER();
}

/* ── chatbox_E5_nop ── */
/* chatbox_E5_nop(script) -> 2
 * No-op script opcode (0xE5): advance the script pointer past the opcode byte,
 * then return 2 so the chatbox driver knows to keep executing.
 *   inputs:  r4 = script pointer, lr
 *   outputs: r0 = 2, r4 = script pointer + 1
 *   clobbers: nz, c, v
 */
Next chatbox_E5_nop(void) {
    R4 = R4 + 1;
    R0 = 2;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_E6_end ── */
/* Flag constants for the chatbox E-flag system */
#define FLAGS_3E_UNK_0001       1
#define FLAGS_3E_UNK_0008       8
#define FLAGS_3E_HIDE_CHATBOX_chatbox_E6_end   0x100
#define FLAGS_3E_UNK_0002       2

/* Chatbox struct field offsets */
#define oChatbox_Unk_05_chatbox_E6_end           5
#define oChatbox_Visible          0
#define oChatbox_TextScriptState  4
#define CHATBOX_SCRIPT_TABLE_BASE 0x140

/* register form: calls chatbox_setflags_3e outside this file, which has no typed C interface yet */
static void chatbox_setflags_3e_helper(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_setflags_3e);
}

/* register form: calls chatbox_maskFlags_3e outside this file, which has no typed C interface yet.
 * Returns true if the flag was present (Z clear after masking → bne path taken). */
static bool chatbox_maskFlags_3e_helper(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_maskFlags_3e);
    return !ZERO_FLAG();
}

/* register form: calls chatbox_8041090 outside this file, which has no typed C interface yet */
static s32 chatbox_8041090_helper(void) {
    GUEST_CALL(chatbox_8041090);
    return (s32)R0;
}

/* register form: calls chatbox_clear_eFlags2009F38 outside this file, which has no typed C interface yet */
static void chatbox_clear_eFlags_helper(u32 a1) {
    R0 = a1;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
}

/* Advance or hide the chatbox.
 * Returns 1 if a pending message was dequeued, 0 otherwise.
 * On the decrement path, sets R1/R2/R4 directly (see register-form note).
 * On other paths, callees leave R1/R2/R4 via GUEST_CALL side-effects. */
/* register form: the decrement path writes output registers r1/r2/r4 directly;
 * no callee exists to set them and the adapter cannot branch to write them conditionally */
static u32 chatbox_e6_end_impl(u32 self) {
    u32 count = LD8(self + oChatbox_Unk_05_chatbox_E6_end);

    if (count != 0) {
        /* Dequeue next message: decrement counter, load script pointer */
        count = count - 1;
        u32 r2_val = 4 * count;
        u32 r1_val = CHATBOX_SCRIPT_TABLE_BASE + r2_val;
        u32 r4_val = LD32(self + r1_val);
        ST8(self + oChatbox_Unk_05_chatbox_E6_end, count);
        R1 = r1_val;
        R2 = r2_val;
        R4 = r4_val;
        return 1;
    }

    /* Mark that a chatbox advance was requested */
    chatbox_setflags_3e_helper(FLAGS_3E_UNK_0001);

    if (chatbox_maskFlags_3e_helper(FLAGS_3E_HIDE_CHATBOX_chatbox_E6_end)) {
        /* HIDE flag was set: check secondary flag 2 */
        if (chatbox_maskFlags_3e_helper(FLAGS_3E_UNK_0002)) {
            /* Flag 2 was set: mark pending and return 0 (no hide/clear) */
            chatbox_setflags_3e_helper(FLAGS_3E_UNK_0008);
        } else {
            /* Flag 2 was not set (beq → loc_8040EDC): clear and hide */
            chatbox_clear_eFlags_helper(0x80);
            ST8(self + oChatbox_Visible, 0);
            ST8(self + oChatbox_TextScriptState, 0);
        }
    } else {
        /* HIDE flag was not set: try to advance dialog */
        if (chatbox_8041090_helper() != 0) {
            /* Cannot advance (bne → loc_8040EDC): clear and hide */
            chatbox_clear_eFlags_helper(0x80);
            ST8(self + oChatbox_Visible, 0);
            ST8(self + oChatbox_TextScriptState, 0);
        }
        /* If advance returned 0, fall through (callee already left r0=0) */
    }

    return 0;
}

/* chatbox_E6_end(r5 = self) -> r0, nz/c/v/r1/r2/r3/r4/lr */
Next chatbox_E6_end(void) {
    R0 = chatbox_e6_end_impl(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_E7_buttonhalt ── */
/* Flag bits managed by chatbox_setflags_3e / chatbox_clearFlags_3e */
#define FLAGS_3E_UNK_0400_chatbox_E7_buttonhalt 0x0400
#define FLAGS_3E_UNK_0020 0x0020
#define FLAGS_3E_UNK_0001_chatbox_E7_buttonhalt 0x0001

/* Joypad bits, as stored in the chatbox's JoypadUp / JoypadHeld fields */
#define JOYPAD_A   0x0001
#define JOYPAD_B   0x0002
#define JOYPAD_ANY 0x03FF

/* register form: calls chatbox_setflags_3e outside this file, which has no typed C interface yet */
static void set_3e_flags(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_setflags_3e);
}

/* register form: calls chatbox_clearFlags_3e outside this file, which has no typed C interface yet */
static void clear_3e_flags(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_clearFlags_3e);
}

typedef struct {
    u32 script;      /* r4: script pointer, advanced past the opcode once the wait is over */
    u32 script_byte; /* r2: operand byte once the wait is active, else the value passed in */
} ButtonHaltResult;

/* Script opcode E7: pause the script until the player confirms with a button.
 *
 * self is the chatbox and script points at the opcode; the byte after it
 * selects which buttons confirm: 0 means A or B, anything else means any
 * button.  The first few frames only run the BoxYX counter down from 5 (so
 * the button that advanced the previous line is not still held), then a newly
 * pressed button -- or holding B until the counter climbs back to 10 -- ends
 * the pause and consumes the two opcode bytes. */
static ButtonHaltResult chatbox_buttonhalt(u32 self, u32 script, u32 prev_script_byte) {
    ButtonHaltResult out = { script, prev_script_byte };
    bool confirmed = false;
    u16 counter;
    u16 confirm_buttons;

    /* Mark the box as waiting: 0x400 stays for the whole command, 0x21 only
     * while the wait is still running. */
    set_3e_flags(FLAGS_3E_UNK_0400_chatbox_E7_buttonhalt);
    set_3e_flags(FLAGS_3E_UNK_0020 | FLAGS_3E_UNK_0001_chatbox_E7_buttonhalt);
    ST8(self + oChatbox_SubState, 0);

    switch (LD8(self + oChatbox_TextScriptState_04)) {
    case 2: /* already waiting for a button */
        break;

    case 1: /* still counting down; arm the buttons once the count runs out */
        counter = LD16(self + oChatbox_BoxYX);
        if (counter == 0) {
            ST8(self + oChatbox_TextScriptState_04, 2);
            break;
        }
        ST16(self + oChatbox_BoxYX, counter - 1);
        return out;

    default: /* first frame of the pause */
        ST16(self + oChatbox_BoxYX, 5);
        ST8(self + oChatbox_TextScriptState_04, 1);
        return out;
    }

    out.script_byte = LD8(script + oChatbox_TextScriptIdx);
    confirm_buttons = out.script_byte != 0 ? JOYPAD_ANY : JOYPAD_A | JOYPAD_B;

    if (LD16(self + oChatbox_JoypadUp) & confirm_buttons)
        confirmed = true;
    else if (LD16(self + oChatbox_JoypadHeld) & JOYPAD_B) {
        /* holding B ends the pause once the counter gets back to ten */
        counter = LD16(self + oChatbox_BoxYX);
        if ((s16)counter >= 10) /* signed compare, as in the original's bge */
            confirmed = true;
        else
            ST16(self + oChatbox_BoxYX, counter + 1);
    }

    if (confirmed) {
        clear_3e_flags(FLAGS_3E_UNK_0020 | FLAGS_3E_UNK_0001_chatbox_E7_buttonhalt);
        ST8(self + oChatbox_TextScriptState_04, 0);
        ST16(self + oChatbox_BoxYX, 0);
        ST32(self + oChatbox_ScriptFlags, 1);
        out.script = script + 2; /* skip the opcode and its operand byte */
    }
    return out;
}

/* 0x08040EF4: (&self@r5, char *script@r4) -> r0 (always 0), r2, r4 */
Next chatbox_E7_buttonhalt(void) {
    ButtonHaltResult out = chatbox_buttonhalt(R5, R4, R2);
    R0 = 0; /* the original leaves r0 = 0 on every exit path */
    R2 = out.script_byte;
    R4 = out.script;
    RETURN_TO_CALLER();
}

/* ── chatbox_E8_msgbox ── */
/* register form: the jump table dispatch (GUEST_CALL_ADDR) and post-call stride
 * advance read/modify R0 and R4 directly; these registers are set up for the callee
 * and modified by it, which cannot be expressed in plain typed C. */
static u32 msgbox_run(u32 r4_in, u32 r5_in) {
    u32 cmd = LD8(r4_in + 1);
    u32 r3_pre;

    if (cmd >= 0x10) {
        ST8(r5_in + 0x1D2, 6);
        r3_pre = 6;
    } else if (cmd >= 0xC) {
        ST8(r5_in + 0x1D2, 5);
        r3_pre = 5;
    } else if (cmd >= 8) {
        ST8(r5_in + 0x1D2, 4);
        r3_pre = 4;
    } else if (cmd <= 3) {
        ST8(r5_in + 0x1D2, 0);
        r3_pre = 0;
    } else {
        u32 state = LD8(r4_in + 2);
        ST8(r5_in + 0x1D2, state);
        u32 idx = LD8(r4_in + 3);
        if (idx != 0xFF) {
            u32 entry = A_byte_8045DCC + (idx << 2);
            ST8(r5_in + oChatbox_CursorIdx, LD16(entry));
            ST8(r5_in + oChatbox_CursorX, LD16(entry + 2));
            r3_pre = idx << 2;
        } else {
            r3_pre = 0xFF;
        }
    }

    R4 = r4_in;
    R5 = r5_in;
    R1 = cmd;
    R2 = 0x1D2;
    R3 = r3_pre;
    R1 <<= 2;
    GUEST_CALL_ADDR(LD32(A_chatbox_EB_msgboxJumpTable + R1));

    if (R0) {
        u32 stride = LD8(0x0804102C + LD8(R4 + 1));
        R4 = R4 + stride;
        R0 = 0x0804102C;
    }
    return R0;
}

Next chatbox_E8_msgbox(void) {
    R0 = msgbox_run(R4, R5);
    RETURN_TO_CALLER();
}

/* ── chatbox_stepOpenAnimation_804103E ── */
/* register form: calls chatbox_setflags_3e outside this file, which has no typed C interface yet */
static void call_setflags_3e(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_setflags_3e);
}

/* register form: calls chatbox_clearFlags_3e outside this file, which has no typed C interface yet */
static void call_clearflags_3e(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_clearFlags_3e);
}

/* register form: calls chatbox_maskFlags_3e outside this file, which has no typed C interface yet.
 * chatbox_maskFlags_3e leaves Z set when the masked flag is already clear; this helper
 * returns true in that case so the typed function need not read guest flags. */
static bool call_maskflags_3e_zero(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_maskFlags_3e);
    return ZERO_FLAG();
}

#define FLAGS_3E_UNK_0001_chatbox_804103E  1u
#define FLAGS_3E_UNK_0100  0x100u

typedef struct {
    u32 r0;
    u32 r1;
    u32 r2;
} ChatboxStepOut;

/* Advance the chatbox open animation by one tick.
 *
 * The chatbox ticks through OpenState (0 -> 1 -> 2 -> 3). OpenState 3 means
 * the box is fully open; intermediate states may also tick BoxY down. The
 * flag mask 0x04 is the "fully open" sentinel and mask 0x01 is the per-tick
 * trigger cleared on completion.
 *
 * Outputs match the original assembly exactly:
 *   - r0: 0 if the animation is still in progress, 1 if it just finished.
 *   - r1: input value on the OpenState==3 fast path; BoxY-1 while scrolling;
 *     0 once BoxY has been drained (the original zeroes r1 via ldrb/mov).
 *   - r2: input value on paths that do not touch r2; 3 after the "mov r2,#3"
 *     that precedes the OpenState==3 check on the drain path.
 *
 * Flags: the original's last flag-setting instruction on every path is a
 * `mov r0, #X`, which only updates N/Z. The adapter therefore uses
 * SET_FLAGS_NZ; C/V are left as in the guest. */
static ChatboxStepOut chatbox_step_animation(u32 r1_in, u32 r2_in, u32 r5) {
    ChatboxStepOut out;
    out.r1 = r1_in;
    out.r2 = r2_in;

    /* Always set the per-tick trigger flag first. */
    call_setflags_3e(FLAGS_3E_UNK_0001_chatbox_804103E);

    u8 open_state = LD8(r5 + oChatbox_OpenState_10);

    if (open_state == 3) {
        /* Fast path: OpenState already 3. The original branches straight to
         * the maskFlags/finish sequence. */
        bool mask_zero = call_maskflags_3e_zero(4);
        if (!mask_zero) {
            /* mask result non-zero: bne loc_8041076 -> mov r0,#0; pop. */
            out.r0 = 0;
        } else {
            /* mask result zero: clear the trigger flag, return 1. */
            call_clearflags_3e(FLAGS_3E_UNK_0001_chatbox_804103E);
            out.r0 = 1;
        }
        /* r1, r2 untouched on this fast path. */
        return out;
    }

    /* OpenState != 3: drop the high byte of the flag word, then either
     * tick BoxY down or advance OpenState. */
    call_clearflags_3e(FLAGS_3E_UNK_0100);

    u8 box_y = LD8(r5 + oChatbox_BoxY);
    if (box_y != 0) {
        /* Still scrolling: decrement BoxY and report not-done. */
        ST8(r5 + oChatbox_BoxY, box_y - 1);
        out.r0 = 0;
        out.r1 = box_y - 1;
        /* r2 unchanged. */
        return out;
    }

    /* BoxY has reached zero: advance OpenState. The original guards a
     * redundant reload with cmp #0 / beq, but the loaded OpenState is the
     * same value either way. */
    u8 new_open_state = (u8)(open_state + 1);
    ST8(r5 + oChatbox_OpenState_10, new_open_state);

    /* The original does `mov r2, #3` before comparing new_open_state against 3,
     * so r2 holds 3 from here on this drain path. */
    out.r2 = 3;

    if (new_open_state == 3) {
        /* Reached OpenState 3 on this tick: same finish sequence as the
         * fast path, but r1 has been clobbered to 0 by the earlier ldrb of
         * BoxY (and r2 is 3 from above). */
        bool mask_zero = call_maskflags_3e_zero(4);
        if (!mask_zero) {
            out.r0 = 0;
        } else {
            call_clearflags_3e(FLAGS_3E_UNK_0001_chatbox_804103E);
            out.r0 = 1;
        }
        out.r1 = 0;
        return out;
    }

    /* Settled into a non-final state: reset BoxY and report not-done. */
    ST8(r5 + oChatbox_BoxY, 0);
    out.r0 = 0;
    out.r1 = 0;
    /* out.r2 already 3. */
    return out;
}

Next chatbox_stepOpenAnimation_804103E(void) {
    ChatboxStepOut out = chatbox_step_animation(R1, R2, R5);
    R0 = out.r0;
    R1 = out.r1;
    R2 = out.r2;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_closeAndReset_80410F8 ── */
/* chatbox_closeAndReset_80410F8(r5 = chatbox): clear the chatbox's text flags (mask 0x101:
 * "window open" + "text finished"), reset its open-state to 3 (fully closed),
 * and return 1. r1 and the flags' C/V are left untouched; Z/N reflect r0 = 1. */
Next chatbox_closeAndReset_80410F8(void) {
    R0 = 0x101;
    GUEST_CALL(chatbox_clearFlags_3e);
    ST8(R5 + oChatbox_OpenState_10, 3u);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_E9_newline ── */
/* chatbox_E9_newline(r4 = script/cursor position, r5 = chatbox object):
 * Step the text-box graphics loader forward after printing a newline.
 * Increments the box's gfx load state and clears its per-line scratch
 * field. Once the state reaches 3 the box graphics are fully loaded and
 * a "ready" flag at +0x8C is set; otherwise the pointer for the new
 * state, taken from a table at byte_803FCE4, is stored at +0x7C.
 * Always increments r4 and returns 0 in r0. */
Next chatbox_E9_newline(void) {
    u32 state = (u32)LD8(R5 + oChatbox_BoxGfxLoadState_0F) + 1;

    ST8(R5 + oChatbox_BoxGfxLoadState_0F, state);
    ST32(R5 + oChatbox_TileBufferOffset, 0);

    if (state == 3) {
        /* Box graphics fully loaded: raise the ready flag. */
        R0 = 1;
        R2 = 0x8C;
        ST32(R5 + 0x8C, R0);
    } else {
        /* Still loading: fetch the entry for the new state. */
        R2 = state - 1;
        R3 = R2 * 4;
        R0 = LD32(A_byte_803FCE4 + R3);
        ST32(R5 + oChatbox_Unk_7C, R0);
    }

    SET_FLAGS_ADD(R4, 1);
    R4 = R4 + 1;

    R0 = 0;
    SET_FLAGS_NZ(0);

    RETURN_TO_CALLER();
}

/* ── chatbox_setEventFlagAndAdvanceEventCursor_80411B0 ── */
/* chatbox_setEventFlagAndAdvanceEventCursor_80411B0(r0 = event flag id, r4 = cursor into an event list):
 * set the event flag passed in r0, then advance the r4 cursor by one
 * 4-byte entry. Flags are left as the trailing `add r4, #4` leaves them. */
Next chatbox_setEventFlagAndAdvanceEventCursor_80411B0(void) {
    GUEST_CALL(SetEventFlag);
    SET_FLAGS_ADD(R4, 4);
    R4 += 4;
    RETURN_TO_CALLER();
}

/* ── chatbox_ClearEntryWaitFlagAndAdvanceEntry_80411BC ── */
/* chatbox_ClearEntryWaitFlagAndAdvanceEntry_80411BC(r0 = event flag bitfield, r4 = chatbox state pointer):
 * Clears the text-advance event flag for the current chatbox entry, then
 * advances the entry pointer by one slot (4 bytes). Flags come from the
 * pointer increment, matching the original's ADDS. */
Next chatbox_ClearEntryWaitFlagAndAdvanceEntry_80411BC(void) {
    GUEST_CALL(ClearEventFlag);

    R4 += 4;
    SET_FLAGS_ADD(R4, 4);

    RETURN_TO_CALLER();
}

/* ── chatbox_cmdSetEventFlag_8041218 ── */
/* chatbox_cmdSetEventFlag_8041218(r4 = pointer to current chatbox command record,
 *                 r5 = chatbox state):
 * Executes the "set event flag" command: byte 2 of the 3-byte record selects
 * one of the 32-bit flag words in the flag table at r5+0x4C, and that word
 * is passed to SetEventFlag. Afterwards r4 is advanced by 3 to point at the
 * next command record.
 * Returns with r0/r1/r3 (and the flags) as left by SetEventFlag, and flags
 * also updated by the `adds r4, #3`. */
Next chatbox_cmdSetEventFlag_8041218(void) {
    /* flag word index = record byte 2; each entry is 4 bytes, table base 0x4C */
    u32 flagWord = LD32(R5 + ((LD8(R4 + 2) << 2) + 0x4C));

    R0 = flagWord;
    GUEST_CALL(SetEventFlag);

    /* advance to the next command record; ADDS sets the outgoing NZCV */
    u32 nextRecord = R4 + 3;
    SET_FLAGS_ADD(R4, 3);
    R4 = nextRecord;

    RETURN_TO_CALLER();
}

/* ── chatbox_EB_option ── */
/* register form: calls chatbox_setflags_3e outside this file, which has no typed C interface yet */
static void set_chatbox_flags_3e(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_setflags_3e);
}

/* Register state the original leaves behind for its caller. */
typedef struct {
    u32 r2; /* offset (relative to the chatbox) of the new entry's gfx-load-state byte */
    u32 r3; /* word copied from oChatbox_TileBufferOffset */
    u32 r4; /* optionDef + 4, the caller's next option definition */
} ChatboxEbOptionResult;

/* Add one entry to the chatbox's option list. The three parameter bytes of the
 * definition at optionDef ([1], [2], [3]) plus the shared oChatbox_TileBufferOffset word
 * are copied into the free 0x10-byte slot, the entry count is bumped, the
 * choice cursor is reset, and the box's gfx-load-state byte is copied into the
 * slot's trailer. Note the entry addressing uses an offset from the chatbox
 * pointer (the original keeps r2 as a bare offset, not an absolute address). */
static ChatboxEbOptionResult chatbox_eb_option_run(u32 optionDef, u32 chatbox) {
    ChatboxEbOptionResult res;

    set_chatbox_flags_3e(FLAGS_3E_UNK_0010);
    ST16(chatbox + oChatbox_BoxYX, 0xA);

    u32 count = LD32(chatbox + oChatbox_ChoiceSlotCount);
    u32 entryOff = 0xA0 + count * 0x10;

    ST8(chatbox + entryOff,     LD8(optionDef + 1));
    ST8(chatbox + entryOff + 4, LD8(optionDef + 2));
    ST8(chatbox + entryOff + 5, LD8(optionDef + 3));

    u32 unk70 = LD32(chatbox + oChatbox_TileBufferOffset);
    ST32(chatbox + entryOff + 8, unk70);

    count++;
    ST32(chatbox + oChatbox_ChoiceSlotCount, count);
    ST8(chatbox + oChatbox_ChoicesPageTag, (u8)count);
    ST8(chatbox + oChatbox_ChoiceCursorPos, 0);
    ST8(chatbox + entryOff + 0xC, LD8(chatbox + oChatbox_BoxGfxLoadState_0F));

    res.r2 = entryOff + 0xC;
    res.r3 = unk70;
    res.r4 = optionDef + 4;
    return res;
}

/* chatbox_EB_option(r1 = unused, r4 = option definition, r5 = chatbox)
 * -> r0 = 1, r1 = oChatbox_ChoiceSlotCount, r2 = entry offset + 0xC, r3 = oChatbox_TileBufferOffset word,
 *    r4 = optionDef + 4, nz/c/v from ADDS r4, #4. */
Next chatbox_EB_option(void) {
    ChatboxEbOptionResult res = chatbox_eb_option_run(R4, R5);
    R0 = 1;
    R1 = oChatbox_ChoiceSlotCount;
    R2 = res.r2;
    R3 = res.r3;
    R4 = res.r4;
    SET_FLAGS_ADD(res.r4, 0);
    RETURN_TO_CALLER();
}

/* ── chatbox_EC_label ── */
/* chatbox_EC_label: chatbox script opcode 0xEC handler.
 * r4 points at the current script command (3 bytes: opcode, sub-opcode,
 * operand), r5 at the chatbox object. The handler adds the command's step
 * to the chatbox's Unk_70 counter and four times that to its Unk_7C
 * counter, then skips to the next command and returns 1 (r0 = 1) so the
 * interpreter keeps running. If the sub-opcode byte at [r4+1] is 1, the
 * operand at [r4+2] is the step itself; otherwise it is an index into an
 * 8-byte-per-entry table, making the step operand * 8.
 * On return r1 = updated Unk_7C, r2 = updated Unk_70; the flags are those
 * of the pointer bump (c/v) with n/z from the returned 1. */
Next chatbox_EC_label(void) {
    u32 operand, advance, newCursor, newCounter;

    operand = LD8(R4 + 2);
    if (LD8(R4 + 1) == 1)
        advance = operand;       /* sub-opcode 1: operand is the raw step */
    else
        advance = operand * 8;   /* otherwise: index into 8-byte-wide table */

    newCursor = LD32(R5 + oChatbox_TileBufferOffset) + advance;
    ST32(R5 + oChatbox_TileBufferOffset, newCursor);

    newCounter = LD32(R5 + oChatbox_Unk_7C) + advance * 4;
    ST32(R5 + oChatbox_Unk_7C, newCounter);

    R2 = newCursor;
    R1 = newCounter;

    R4 += 3;                     /* skip past the 3-byte command */
    SET_FLAGS_ADD(R4, 3);

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── chatbox_EF_checkflag ── */
/*
 * chatbox script-command dispatcher (0x0804156C).
 *
 * The chatbox work area (r4) holds a command opcode at offset 1. This function
 * looks the opcode up in the handler table jt_ctrl_8041580 and jumps to the
 * matching handler, leaving r0 = table entry and r1 = opcode*4 for the callee.
 * It is a tail call in disguise: `mov lr, pc` points back at the `pop {pc}`,
 * so the handler returns straight to this function's caller, and every register
 * and flag as the handler leaves them becomes this function's result.
 */

/* register form: calls the chatbox script handlers outside this file (the targets
 * of jt_ctrl_8041580), which have no typed C interface yet. The callee runs with
 * this function's register state; its r0..r12 and flags at return are what the
 * caller of chatbox_EF_checkflag must see, which no typed interface can express. */
static void chatbox_dispatch_command(u32 ctx)
{
    u32 cmd = LD8(ctx + 1);                        /* opcode byte selects the handler */
    u32 entry = LD32(A_jt_ctrl_8041580 + cmd * 4); /* handler address | 1, as the original leaves it in r0 */

    R0 = entry;
    R1 = cmd * 4;                                  /* r1 as the original leaves it for the callee */

    switch (cmd) {
    case 0:  GUEST_CALL(chatbox_804171C); break;
    case 1:  GUEST_CALL(chatbox_cmdRunScriptByGameProgress_8041748); break;
    case 2:  GUEST_CALL(chatbox_80416C4); break;
    case 3:  GUEST_CALL(chatbox_8041774); break;
    case 4:  GUEST_CALL(chatbox_80416F0); break;
    case 5:  GUEST_CALL(chatbox_stepScriptEntry_80417A4); break;
    case 6:  GUEST_CALL(chatbox_80417C0); break;
    case 7:  GUEST_CALL(chatbox_80417E4); break;
    case 8:
    case 9:
    case 42: GUEST_CALL(chatbox_evaluateChipScript_8041818); break;
    case 10:
    case 11:
    case 12:
    case 13: GUEST_CALL(chatbox_8042A9C); break;
    case 14:
    case 15:
    case 16:
    case 17: GUEST_CALL(chatbox_8042B38); break;
    case 18:
    case 19:
    case 20:
    case 21: GUEST_CALL(chatbox_8042C18); break;
        /* slots 22-25, 31, 35-39, 44, 45, 48 and 58 are empty (0) in the table;
         * they are never dispatched on real game states, so nothing is called. */
    case 26: GUEST_CALL(chatbox_80418CC); break;
    case 27:
    case 28:
    case 29:
    case 30: GUEST_CALL(chatbox_8043A5C); break;
    case 32: GUEST_CALL(chatbox_8041904); break;
    case 33: GUEST_CALL(chatbox_8041944); break;
    case 34: GUEST_CALL(chatbox_processScriptEntry_80419D0); break;
    case 40: GUEST_CALL(chatbox_8041694); break;
    case 41: GUEST_CALL(chatbox_8041670); break;
    case 43: GUEST_CALL(chatbox_8041964); break;
    case 46: GUEST_CALL(chatbox_804198C); break;
    case 47: GUEST_CALL(chatbox_startCurNaviScript_80419B4); break;
    case 49: GUEST_CALL(chatbox_80419F8); break;
    case 50: GUEST_CALL(chatbox_8041A28); break;
    case 51: GUEST_CALL(chatbox_8041A4C); break;
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57: GUEST_CALL(chatbox_8041A7C); break;
    case 59: GUEST_CALL(chatbox_8041ABC); break;
    default:
        /* out-of-range opcodes would read past the table in the original and
         * jump to garbage; unreachable in practice. */
        break;
    }
}

/* chatbox_EF_checkflag: dispatch the chatbox script command in [r4+1].
 * Everything the caller sees afterwards (all registers, flags) is whatever the
 * dispatched handler left behind, so the adapter only forwards r4 and returns. */
Next chatbox_EF_checkflag(void) {
    chatbox_dispatch_command(R4);
    RETURN_TO_CALLER();
}

/* ── chatbox_8041694 ── */
/* chatbox_8041694 — "show textbox" chatbox script command (r4 = command stream).
 *
 * Bytes +2/+3 hold a 16-bit event flag number and byte +4 a flag count;
 * TestEventFlagRange reports (via its zero flag) whether the flag test fails.
 * Depending on the result, the text script id is read from offset +5 (test
 * passed) or +6 (test failed).  A script id of 0xFF means "no textbox": the
 * command is simply skipped by advancing r4 past all 7 bytes.  Otherwise the
 * id is handed to the chatbox configuration routine, which also receives
 * TestEventFlagRange's return value as its (unused) first argument.
 * Always returns 1 in r0. */
Next chatbox_8041694(void) {
    u32 flagId    = LD8(R4 + 2) | (LD8(R4 + 3) << 8);
    u32 flagCount = LD8(R4 + 4);

    R0 = flagId;
    R2 = flagCount;
    GUEST_CALL(TestEventFlagRange);

    /* flags clear -> id at +6, flags set (bne taken) -> id at +5 */
    R2 = ZERO_FLAG() ? 6 : 5;

    u8 scriptID = LD8(R4 + R2);
    if (scriptID == 0xFF) {
        /* nothing to show: skip this 7-byte command */
        R4 += 7;
    } else {
        R1 = scriptID;
        /* R0 still holds TestEventFlagRange's result; the callee ignores it */
        GUEST_CALL(chatbox_config_80408D0);
    }

    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── chatbox_80416C4 ── */
/* chatbox_80416C4(r4 = chatbox entry, r10 = toolkit):
 * Checks the current map number against the entry's map range (bytes +2 = min,
 * +3 = max) and picks the script byte for it: +4 when the map is inside the
 * range, +5 when it is outside. If that script slot is empty (0xFF), the entry
 * is skipped by advancing r4 to the next entry; otherwise the script is run
 * through chatbox_config_80408D0. Always returns r0 = 1.
 * Also passes back r2 = max map, r3 = current map, r4 = (possibly advanced)
 * entry pointer, and leaves NZ reflecting r0 = 1. */
Next chatbox_80416C4(void) {
    u32 entry = R4;
    u8 minMap = LD8(entry + 2);
    u8 maxMap = LD8(entry + 3);
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u8 mapNumber = LD8(gameState + oGameState_MapNumber);

    /* Byte +4 holds the script for maps in range, +5 for maps out of range. */
    u32 scriptOffset = 5;
    if (mapNumber >= minMap && mapNumber <= maxMap)
        scriptOffset = 4;

    u8 scriptID = LD8(entry + scriptOffset);

    /* These are live outputs for the caller, kept across the callee. */
    R2 = maxMap;
    R3 = mapNumber;

    if (scriptID == 0xFF) {
        /* No script for this map: skip to the next 6-byte entry. */
        SET_FLAGS_ADD(entry, 6);
        R4 = entry + 6;
        R0 = 1;
    } else {
        R1 = scriptID;
        GUEST_CALL(chatbox_config_80408D0);
        R0 = 1;
    }
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_80416F0 ── */
/* chatbox_80416F0(r4 = chatbox script entry, r10 = toolkit):
 * Executes one 6-byte chatbox script entry. Bytes 2/3 of the entry bound the
 * map group; byte 4 holds the script ID to use when the current map group is
 * inside that range, byte 5 the one for when it is not. A script ID of 0xFF
 * means "no script here" — skip to the next entry instead. Always returns 1
 * in r0; r4 is left pointing past the entry on the skip path. */
Next chatbox_80416F0(void) {
    u32 entry = R4;
    u32 mapLow = LD8(entry + 2);
    u32 mapHigh = LD8(entry + 3);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 mapGroup = LD8(gameState + oGameState_MapGroup);

    /* Pick which of the entry's two script IDs applies here. */
    u32 slot = 5;
    if ((s32)mapGroup >= (s32)mapLow && (s32)mapGroup <= (s32)mapHigh)
        slot = 4;

    u32 scriptID = LD8(entry + slot);

    /* These stay live across the callee, matching the original's registers. */
    R2 = mapHigh;
    R3 = mapGroup;

    if (scriptID != 0xFF) {
        R0 = 0;                 /* unused argument */
        R1 = scriptID;
        GUEST_CALL(chatbox_config_80408D0);
    } else {
        entry += 6;             /* no script: advance to the next entry */
        R4 = entry;
    }

    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── chatbox_804171C ── */
/* chatbox_804171C(r4 = pointer to a chatbox script command):
 * Conditional script step. The command stores a u16 event flag at [r4+2]
 * and two script IDs: the one at [r4+4] runs when the flag is set, the one
 * at [r4+5] when it is clear. An ID of 0xFF means "nothing to run", in
 * which case r4 is advanced by 6 (the size of this command) instead.
 * Always returns r0 = 1. */
Next chatbox_804171C(void) {
    u32 script = R4;
    u16 eventFlag = LD8(script + 2) | (LD8(script + 3) << 8);

    /* TestEventFlag reports the flag state through the Z flag
     * (Z clear = flag set) and leaves its r0 untouched for the callee below. */
    R0 = eventFlag;
    GUEST_CALL(TestEventFlag);

    u32 scriptOffset = ZERO_FLAG() ? 5 : 4;
    R2 = scriptOffset;
    R1 = LD8(script + scriptOffset);

    if (R1 == 0xFF) {
        /* Nothing to run: skip past this command. */
        R4 = script + 6;
    } else {
        /* chatbox_config_80408D0(notUsed r0, scriptID r1); r0 still holds
         * whatever TestEventFlag left there, exactly as in the original. */
        GUEST_CALL(chatbox_config_80408D0);
    }
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── chatbox_cmdRunScriptByGameProgress_8041748 ── */
/* chatbox_cmdRunScriptByGameProgress_8041748: chatbox script command "run script depending on game progress".
 *
 * r4 points at a 6-byte command laid out as:
 *   [2] = lowest game-progress value that satisfies the condition
 *   [3] = highest game-progress value that satisfies the condition
 *   [4] = sub-script ID to run when the condition holds   (0xFF = none)
 *   [5] = sub-script ID to run when the condition fails   (0xFF = none)
 *
 * The current game progress is read from the game state, the matching
 * sub-script (if any) is started through chatbox_config_80408D0, and r4 is
 * advanced past the command. Always returns r0 = 1. */
Next chatbox_cmdRunScriptByGameProgress_8041748(void) {
    u32 minProgress = LD8(R4 + 2);
    u32 maxProgress = LD8(R4 + 3);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 progress  = LD8(gameState + 6 /* oGameState_GameProgress */);

    /* Pick which of the two trailing script IDs applies. */
    u32 idOffset;
    if ((s32)progress < (s32)minProgress || (s32)progress > (s32)maxProgress)
        idOffset = 5;
    else
        idOffset = 4;

    u32 scriptID = LD8(R4 + idOffset);

    R0 = idOffset;      /* first parameter of the callee is unused */
    R1 = scriptID;
    R2 = maxProgress;
    R3 = progress;

    if (scriptID != 0xFF) {
        /* chatbox_config_80408D0(notUsed, scriptID): start the sub-script. */
        GUEST_CALL(chatbox_config_80408D0);
    } else {
        /* No sub-script for this branch: skip the whole 6-byte command. */
        u32 next = R4 + 6;
        SET_FLAGS_ADD(R4, 6);
        R4 = next;
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_stepScriptEntry_80417A4 ── */
/* chatbox_stepScriptEntry_80417A4(r4 = pointer to the current chatbox script entry):
 * Peek at the byte at offset 3 of the current script entry.  If it is the
 * 0xFF terminator, the entry is "go to next line": advance the cursor by one
 * entry (r4 += 4).  Otherwise hand the byte to the chatbox script interpreter
 * as a script ID (the first argument is always the constant 3; the original
 * also sets r0 = 2 first, but that store is dead).
 * Always returns r0 = 1.  On the terminator path r1 still holds the byte read
 * and r4 is the advanced cursor; on the interpreter path r1/r4 are whatever
 * the callee leaves.  Flags end up as after "mov r0, #1" (nz = 1 != 0),
 * except that on the terminator path c/v still come from the r4 += 4 add. */
Next chatbox_stepScriptEntry_80417A4(void) {
    R0 = 3;                              /* fixed notUsed argument */
    R1 = LD8(R4 + 3);                    /* script ID or end-of-entry marker */

    if (R1 != 0xFF) {
        GUEST_CALL(chatbox_config_80408D0);
    } else {
        u32 nextEntry = R4 + 4;
        SET_FLAGS_ADD(R4, 4);            /* carry/overflow of the cursor bump */
        R4 = nextEntry;
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_80417E4 ── */
/* chatbox_80417E4 (r4 = pointer to the current chatbox script command):
 * "check key item" command. Layout of the 7-byte command:
 *   +2: key item ID to look up with CheckKeyItem
 *   +3: expected result to compare against
 *   +4/+5/+6: script IDs for the equal / greater / less cases
 * The selected script is started via chatbox_config_80408D0 unless it is the
 * 0xFF end marker, in which case the command pointer is simply advanced past
 * the whole command instead. Always returns 1; r4 holds the (possibly
 * advanced) command pointer and r2 the selected branch offset on exit. */
Next chatbox_80417E4(void) {
    u32 cmd = R4;

    R0 = LD8(cmd + 2);
    GUEST_CALL(CheckKeyItem);
    u32 result = R0;

    u32 expected = LD8(cmd + 3);

    /* Pick which branch ID to use: offset 4/5/6 for equal / greater / less. */
    u32 branch;
    if (result == expected)
        branch = 4;
    else if ((s32)result > (s32)expected)
        branch = 5;
    else
        branch = 6;

    u32 scriptID = LD8(cmd + branch);
    R2 = branch;

    if (scriptID != 0xFF) {
        /* r0 is the unused first argument; it still holds the CheckKeyItem
         * result here, exactly as in the original. */
        R0 = result;
        R1 = scriptID;
        GUEST_CALL(chatbox_config_80408D0);
    } else {
        /* 0xFF script ID: nothing to run, just skip past this command. */
        R4 = cmd + 7;
    }

    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── chatbox_evaluateChipScript_8041818 ── */
/* register form: calls GetChipCountOfCode outside this file, which has no typed C interface yet */
static u32 chip_count_of_code(u32 code, u32 code_param) {
    R0 = code; R1 = code_param;
    GUEST_CALL(GetChipCountOfCode);
    return R0;
}

/* register form: calls chatbox_countChipAcrossFolders outside this file, which has no typed C interface yet */
static u32 chip_limit_of_code(u32 code, u32 code_param) {
    R0 = code; R1 = code_param;
    GUEST_CALL(chatbox_countChipAcrossFolders);
    return R0;
}

/* register form: calls GetTotalChipCount outside this file, which has no typed C interface yet */
static u32 total_chip_count(u32 code) {
    R0 = code;
    GUEST_CALL(GetTotalChipCount);
    return R0;
}

/* register form: calls chatbox_config_80408D0 outside this file, which has no typed C interface yet;
 * the callee overwrites R1, R4, and leaves C/V flags that callers observe on return */
static u32 trigger_script(u32 count_val, u32 script_id, u32 offset,
                          u32 *r4_out, u32 *flag_a_out, u32 *flag_b_out) {
    R0 = count_val; R1 = script_id; R2 = offset;
    GUEST_CALL(chatbox_config_80408D0);
    *r4_out = R4;
    /* Encode callee C,V into (a,b) so that SET_FLAGS_ADD(a,b) reproduces them:
     * c=1,v=0 → 0xFFFFFFFF+1; c=0,v=1 → 0x7FFFFFFF+1; c=1,v=1 → 0x80000000+0x80000001; c=0,v=0 → 0+0 */
    u32 carry = CARRY_FLAG();
    u32 over  = OVERFLOW_FLAG();
    if (carry && over) { *flag_a_out = 0x80000000; *flag_b_out = 0x80000001; }
    else if (carry)     { *flag_a_out = 0xFFFFFFFF; *flag_b_out = 1; }
    else if (over)      { *flag_a_out = 0x7FFFFFFF; *flag_b_out = 1; }
    else                { *flag_a_out = 0;          *flag_b_out = 0; }
    return R1;
}

/* Three-way signed comparison index: 0=equal, 1=greater, 2=less */
static u32 cmp_index(s32 a, s32 b) {
    if (a == b) return 0;
    if (a > b)  return 1;
    return 2;
}

typedef struct {
    u32 r1, r2, r4;
    u32 flag_a, flag_b;
} ChatboxRes;

/* Evaluate a chatbox chip-condition script record at *ptr, advancing past it or triggering a script.
 * Returns the new r1 (script ID or 0xFF), r2 (comparison offset), r4 (pointer advance),
 * and (flag_a, flag_b) to feed SET_FLAGS_ADD for the correct C/V at return. */
static ChatboxRes eval_chip_script(u32 ptr) {
    ChatboxRes res;
    u32 opcode = LD8(ptr + 1);
    u32 code   = LD8(ptr + 2) | ((u32)LD8(ptr + 3) << 8);
    u32 count, offset, step;

    if (opcode == 0x2A) {
        /* Total chip count vs threshold at [ptr+4] */
        count  = total_chip_count(code);
        offset = 5 + cmp_index((s32)count, (s32)LD8(ptr + 4));
        step   = 8;
    } else if (opcode == 9) {
        /* Chip count minus per-code limit vs threshold at [ptr+5] */
        u32 cp    = LD8(ptr + 4);
        count     = chip_count_of_code(code, cp) - chip_limit_of_code(code, cp);
        offset    = 6 + cmp_index((s32)count, (s32)LD8(ptr + 5));
        step      = 9;
    } else {
        /* Chip count of code vs threshold at [ptr+5] */
        u32 cp    = LD8(ptr + 4);
        count     = chip_count_of_code(code, cp);
        offset    = 6 + cmp_index((s32)count, (s32)LD8(ptr + 5));
        step      = 9;
    }

    res.r2 = offset;
    u32 script_byte = LD8(ptr + offset);

    if (script_byte != 0xFF) {
        /* Non-terminator: invoke the script handler, which may rewrite r4 */
        res.r1 = trigger_script(count, script_byte, offset, &res.r4, &res.flag_a, &res.flag_b);
    } else {
        /* Terminator 0xFF: advance record pointer by step (ADDS sets C,V) */
        res.r1      = 0xFF;
        res.r4      = ptr + step;
        res.flag_a  = ptr;
        res.flag_b  = step;
    }
    return res;
}

/* chatbox_evaluateChipScript_8041818(r4 = script record pointer) → r0=1, r1/r2/r4 updated, flags */
Next chatbox_evaluateChipScript_8041818(void) {
    ChatboxRes res = eval_chip_script(R4);
    R1 = res.r1;
    R2 = res.r2;
    R4 = res.r4;
    SET_FLAGS_ADD(res.flag_a, res.flag_b);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_startCurNaviScript_80419B4 ── */
/* chatbox_startCurNaviScript_80419B4(r4 = chatbox work area, r2/r5/r10 = caller context):
 * Start the chatbox script belonging to the currently equipped PET navi.
 * Reads the navi's script ID from r4[navi + 2]; 0xFF means "no script for
 * this navi", in which case r4 is just advanced past the table by 0xE.
 * Otherwise hands the script ID to the chatbox config loader (which ignores
 * its first argument; r0 still holds the navi number from GetCurPETNavi).
 * Always returns r0 = 1. */
Next chatbox_startCurNaviScript_80419B4(void) {
    GUEST_CALL(GetCurPETNavi);      /* R0 = current PET navi number */

    R1 = LD8(R4 + R0 + 2);          /* script ID slot for this navi */
    if (R1 != 0xFF) {
        GUEST_CALL(chatbox_config_80408D0);
        R0 = 1;
        SET_FLAGS_NZ(R0);           /* movs r0,#1: nz only; c/v left as the callee set them */
    } else {
        SET_FLAGS_ADD(R4, 0xE);     /* adds r4,#0xE: sets c/v (nz overwritten below) */
        R4 += 0xE;
        R0 = 1;
        SET_FLAGS_NZ(R0);           /* movs r0,#1 */
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_processScriptEntry_80419D0 ── */
/* chatbox_processScriptEntry_80419D0: process one chatbox script entry.
 * r4 points at the current entry. Its state byte ([r4+2]) is passed to
 * sub_8048C24, which returns a value deciding which of the two trailing
 * bytes holds the script ID (offset 3 when nonzero, offset 4 when zero).
 * - If that ID is 0xFF (end-of-script marker), advance r4 to the next
 *   entry (r4 += 5) and return.
 * - Otherwise hand the ID to chatbox_config_80408D0 and return.
 * Always returns r0 = 1. r5/r6/r7 are preserved. */
Next chatbox_processScriptEntry_80419D0(void) {
    u32 entry = R4;

    /* Ask the chatbox state handler which script-ID byte is current. */
    R0 = LD8(entry + 2);
    GUEST_CALL(sub_8048C24);
    u32 state = R0;

    u32 idOffset = (state == 0) ? 4 : 3;
    u32 scriptID = LD8(entry + idOffset);

    /* The ID is loaded into r1 on both paths (the original's ldrb happens
     * before the 0xFF test), so set it before branching. */
    R1 = scriptID;

    if (scriptID == 0xFF) {
        /* End marker: skip past this entry to the next one. */
        R4 = entry + 5;
        SET_FLAGS_ADD(entry, 5);
    } else {
        /* Configure the chatbox with this entry's script ID.
         * r0 (the state-check result) is an unused parameter here. */
        GUEST_CALL(chatbox_config_80408D0);
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_F1_textspeed ── */
/* chatbox_F1_textspeed(r4 = text script command pointer, r5 = chatbox):
 * Handles a "set text print speed" command in the chatbox text script: the
 * speed parameter (byte at r4+2) is copied into the chatbox's current print
 * speed, and speed+1 is stored at chatbox+0x1F3 (the speed to use afterwards).
 * The chatbox 0x3E flag word records whether the requested speed was 0
 * (0x0040) or not (0x0800).
 * Returns with r0 = 1, r1 = speed+1, r2 = 0x1F3, r4 advanced past the
 * 3-byte command; c/v reflect the r4 + 3 addition, nz is set by the final
 * MOV r0, #1 (Thumb format-3 MOV sets N/Z). */
Next chatbox_F1_textspeed(void) {
    u32 speed = LD8(R4 + 2);

    ST8(R5 + 8, speed); /* oChatbox_TextScriptPrintSpeed */
    /* speed == 0 stores 1 here too, so a single store covers both paths */
    ST8(R5 + 0x1F3, speed + 1); /* chatbox+0x1F3: next print speed */
    R1 = speed + 1;
    R2 = 0x1F3;

    if (speed == 0) {
        R0 = FLAGS_3E_UNK_0040;
        GUEST_CALL(chatbox_setflags_3e); /* (int mask) -> void */
    } else {
        R0 = 0x800; /* FLAGS_3E_UNK_0800 */
        GUEST_CALL(chatbox_setflags_3e); /* (int mask) -> void */
    }

    u32 prevCursor = R4;
    R4 = prevCursor + 3;
    SET_FLAGS_ADD(prevCursor, 3);

    R0 = 1;
    SET_FLAGS_NZ(R0); /* Thumb MOV rd, #imm sets N/Z, leaves C/V */
    RETURN_TO_CALLER();
}

/* ── chatbox_F2_clearmsgbox ── */
/* Chatbox text-script opcode F2 ("clear message box"): abandon the message
 * currently being printed, reset the chatbox's print state, and resume the
 * text script at the opcode that follows this one. */

/* Result of the F2 handler: the value returned in r0 plus the registers the
 * original happens to leave behind for its callers. */
typedef struct {
    u32 result;        /* r0: always 1 (opcode handled) */
    u32 table_ptr;     /* r1: address of the field-offset table (off_8041C48) */
    u32 field_offset;  /* r2: last field offset read from that table (0x1D6) */
    u32 script_ptr;    /* r4: text-script pointer advanced past the opcode */
} ClearMsgboxResult;

/* register form: calls chatbox_set_eFlags2009F38 outside this file, which has no typed C interface yet */
static void set_eFlags_2009F38(u32 flags) {
    R0 = flags;
    GUEST_CALL(chatbox_set_eFlags2009F38);
}

/* register form: calls chatbox_setflags_3e outside this file, which has no typed C interface yet */
static void set_flags_3e_chatbox_F2_clearmsgbox(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_setflags_3e);
}

/* register form: calls chatbox_clearFlags_3e outside this file, which has no typed C interface yet */
static void clear_flags_3e(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_clearFlags_3e);
}

/* register form: calls chatbox_resetAndClearBuffers_8045F60 outside this file, which has no typed C interface yet */
static void reset_and_clear_buffers_chatbox_F2_clearmsgbox(u32 char_idx, u32 buffer) {
    R0 = char_idx;
    R1 = buffer;
    GUEST_CALL(chatbox_resetAndClearBuffers_8045F60);
}

/* script_ptr: current text-script pointer (advanced past the opcode);
 * chatbox:    the chatbox object. */
static ClearMsgboxResult clear_msgbox(u32 script_ptr, u32 chatbox) {
    ClearMsgboxResult out;

    /* Mark event-flag group 0x200 as set. */
    set_eFlags_2009F38(0x200);

    /* If a print speed is configured, mirror that in the 3E flag word:
     * set bit 0, clear bit 6. */
    if (LD8(chatbox + oChatbox_TextScriptPrintSpeed) != 0) {
        set_flags_3e_chatbox_F2_clearmsgbox(FLAGS_3E_UNK_0001);
        clear_flags_3e(FLAGS_3E_UNK_0040);
    }

    /* Stop the "text finished" / "waiting for input" 3E flags. */
    clear_flags_3e(FLAGS_3E_UNK_0400 | FLAGS_3E_UNK_0800);

    /* Step the interpreter past this opcode and reset the print state. */
    script_ptr += 1;
    ST32(chatbox + oChatbox_CurrTextScriptPtr, script_ptr);

    ST8(chatbox + oChatbox_TextScriptState_04, 0);
    ST32(chatbox + oChatbox_TextScriptTimer, 0);
    ST8(chatbox + oChatbox_ChoicesPageTag, 0);
    ST8(chatbox + oChatbox_CurrLine, 0);
    ST8(chatbox + oChatbox_MugshotAnimTableIdx, 0);
    ST8(chatbox + oChatbox_DroppedName, 0);
    ST32(chatbox + oChatbox_ChoiceSlotCount, 0);
    ST32(chatbox + oChatbox_PrintDelay, 4);
    ST8(chatbox + oChatbox_JumpTableOffset_11, 1);
    ST32(chatbox + oChatbox_ScriptFlags, 1);

    /* Carry the current character index over to Unk_94, then have the
     * shared buffer routine reset and clear the print buffers. */
    u32 char_idx = LD8(chatbox + oChatbox_TextScriptCharIdx);
    ST8(chatbox + oChatbox_Unk_94, char_idx);
    reset_and_clear_buffers_chatbox_F2_clearmsgbox(char_idx, chatbox + oChatbox_Unk_94);

    /* Zero the two halfword scratch fields named by the offset table. */
    u32 field_table = A_off_8041C48;
    ST16(chatbox + LD32(field_table), 0);
    u32 last_offset = LD32(field_table + 4);
    ST16(chatbox + last_offset, 0);

    out.result = 1;
    out.table_ptr = field_table;
    out.field_offset = last_offset;
    out.script_ptr = script_ptr;
    return out;
}

/* chatbox_F2_clearmsgbox(r4 = text-script pointer, r5 = chatbox object)
 * -> r0 = 1 (flags NZ set), r1/r2 = leftover table pointer and offset,
 * r4 = advanced script pointer. r3 and r5 are left untouched. */
Next chatbox_F2_clearmsgbox(void) {
    ClearMsgboxResult out = clear_msgbox(R4, R5);
    R0 = out.result;
    R1 = out.table_ptr;
    R2 = out.field_offset;
    R4 = out.script_ptr;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_F3_control ── */
/* chatbox_F3_control(r4 = pointer to the current F3 chatbox opcode):
 * Executes one F3 (chatbox show/hide) opcode from the text script.
 * Byte [r4+1] is the opcode:
 *   0 -> show the chatbox at full size (64 units) and skip 2 bytes
 *   1 -> hide the chatbox and skip 2 bytes
 *   2 or any other value -> show the chatbox with the length taken from
 *       byte [r4+2] and skip 3 bytes
 * Always returns 1 in r0. r1 and r2 are preserved; the C/V flags reflect
 * the pointer bump (ADDS), N/Z are clear from the final MOV r0,#1. */
static void chatbox_F3_advance(u32 step) {
    u32 old = R4;
    R4 = old + step;
    SET_FLAGS_ADD(old, step);
}

Next chatbox_F3_control(void) {
    u32 op = LD8(R4 + 1);

    if (op == 0) {
        /* show chatbox at full size */
        R0 = 0x40;
        GUEST_CALL(chatbox_set_eFlags2009F38);
        chatbox_F3_advance(2);
    } else if (op == 1) {
        /* hide chatbox */
        R0 = 0x40;
        GUEST_CALL(chatbox_clear_eFlags2009F38);
        chatbox_F3_advance(2);
    } else {
        /* show chatbox with an explicit per-opcode size */
        R0 = LD8(R4 + 2);
        GUEST_CALL(chatbox_set_eFlags2009F38);
        chatbox_F3_advance(3);
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_F5_mugshot ── */
/* register form: calls chatbox_maskFlags_3e outside this file, which has no typed C interface yet */
static bool chatbox_flag_3e_is_set(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_maskFlags_3e);
    return !ZERO_FLAG();          /* Z=0 when (flags & mask) != 0, i.e. the flag is set */
}

/* register form: calls chatbox_setflags_3e outside this file, which has no typed C interface yet */
static void chatbox_flag_3e_set(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_setflags_3e);
}

/* register form: calls chatbox_clearFlags_3e outside this file, which has no typed C interface yet */
static void chatbox_flag_3e_clear(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_clearFlags_3e);
}

/* register form: calls chatbox_loadMugshotPortrait_8040B3A outside this file, which has no typed C interface yet.
   After the call, R1/R2/R3/R7 hold the callee's output values; the adapter does not
   rewrite them. */
static void mugshot_refresh(u32 mugshot_id, u32 flag) {
    R0 = mugshot_id;
    R1 = flag;
    GUEST_CALL(chatbox_loadMugshotPortrait_8040B3A);
}

/* Process one chatbox F5 (mugshot) command.
 * cmd points at the 3-byte command record, chatbox is the chatbox struct base.
 * Returns the advanced command pointer.
 * On the subcmd==3 and subcmd==4 paths the original explicitly leaves a register
 * (r1 or r2) that is not produced by any callee; those are written here in
 * register form because the thin adapter cannot branch to decide which output
 * to write on the non-GUEST_CALL paths (on the default path the GUEST_CALL in
 * mugshot_refresh already populates r1/r2/r3/r7 and must not be overwritten). */
static u32 chatbox_process_mugshot(u32 cmd, u32 chatbox) {
    u8 subcmd = LD8(cmd + 1);

    if (subcmd == 1) {
        /* Deactivate mugshot */
        chatbox_flag_3e_clear(FLAGS_3E_MUGSHOT_ACTIVE);
        return cmd + 2;
    }
    if (subcmd == 2) {
        /* Set palette index directly */
        ST8(chatbox + oChatbox_MugshotPaletteIdx, LD8(cmd + 2));
        return cmd + 3;
    }
    if (subcmd == 3) {
        /* Set face/eye override bytes at 0x1F0 and 0x1F1 */
        u8 val = LD8(cmd + 2);
        ST8(chatbox + 0x1F0, val);
        ST8(chatbox + 0x1F1, val);
        /* register form: r1 = 0x1f1 is a computed output of this path
           (ldr r1, off_8041DB8 in the original); the adapter must not
           unconditionally write r1 because on the default path r1 comes
           from the callee GUEST_CALL and must not be clobbered. */
        R1 = 0x1F1;
        return cmd + 3;
    }
    if (subcmd == 4) {
        /* Set mugshot ID as a 32-bit value at offset 0x90 */
        ST32(chatbox + 0x90, (u32)LD8(cmd + 2));
        /* register form: r2 = 0x90 is a computed output of this path
           (mov r2, #0x90 in the original); same rationale as r1 above. */
        R2 = 0x90;
        return cmd + 3;
    }

    /* Default: activate and fully configure the mugshot */
    if (!chatbox_flag_3e_is_set(FLAGS_3E_MUGSHOT_ACTIVE)) {
        chatbox_flag_3e_set(FLAGS_3E_MUGSHOT_ACTIVE);
        chatbox_flag_3e_set(FLAGS_3E_UNK_0004);
        if (!chatbox_flag_3e_is_set(0x1000)) {
            ST32(chatbox + oChatbox_MugshotColorFilter_40, 0x18C6);
        }
    }

    u8 mugshot_id = LD8(cmd + 2);

    /* Look up palette index in (id, palette) table terminated by 0xFF */
    u8 palette_idx = 0;
    {
        u32 tbl = A_byte_8044244;
        u8 entry;
        while ((entry = LD8(tbl)) != 0xFF) {
            if (entry == mugshot_id) {
                palette_idx = LD8(tbl + 1);
                break;
            }
            tbl += 2;
        }
    }
    ST8(chatbox + oChatbox_MugshotPaletteIdx, palette_idx);

    /* Display-state byte: keep 3 from field 0x1F0, otherwise 0 */
    u8 unk_1f0 = LD8(chatbox + 0x1F0);
    u8 extra_byte = (unk_1f0 == 3) ? unk_1f0 : 0;
    u8 flag_byte = 1;

    /* Look up display data in second (id, value) table terminated by 0xFF */
    {
        u32 tbl = A_byte_8044260;
        u8 entry;
        while ((entry = LD8(tbl)) != 0xFF) {
            if (entry == mugshot_id) {
                extra_byte = LD8(tbl + 1);
                flag_byte = 0;
                break;
            }
            tbl += 2;
        }
    }

    ST8(chatbox + oChatbox_ScriptTokenParam, flag_byte);
    ST8(chatbox + oChatbox_ChoiceNumOptions, extra_byte);
    ST8(chatbox + 0x98, extra_byte);
    ST8(chatbox + oChatbox_PageLoadReset, 0xFF);

    /* Trigger mugshot graphic refresh; callee sets R1/R2/R3/R7 as side effects */
    mugshot_refresh(mugshot_id, flag_byte);

    return cmd + 3;
}

/* chatbox_F5_mugshot(r4 = cmd, r5 = chatbox) -> r0=1, nz/c/v from final ADDS+MOV */
Next chatbox_F5_mugshot(void) {
    u32 old_r4 = R4;
    R4 = chatbox_process_mugshot(R4, R5);
    R0 = 1;
    SET_FLAGS_ADD(old_r4, R4 - old_r4);
    RETURN_TO_CALLER();
}

/* ── chatbox_cmd_playerFixedAnim_8041EE8 ── */
/* owPlayer_RequestFixedAnimation_809E14C is not declared in the shared headers, so forward-declare it
 * here; it is a normal `Next ... (void)` game function. */
Next owPlayer_RequestFixedAnimation_809E14C(void);

/* chatbox_cmd_playerFixedAnim_8041EE8 — chatbox script command handler.
 *
 * Inputs:  r4 = pointer into the chatbox script (the command's operand
 *              byte sits at [r4 + 2]),
 *          r5 = chatbox work area,
 *          r10 = toolkit.
 *
 * What it does:
 *   1. sets bit 0 of the chatbox 0x3E flag word (the original loads the
 *      immediate 1 directly; no named constant exists for this bit),
 *   2. copies byte 0x14 of the overworld player object into the chatbox
 *      work area at offset 0x3C,
 *   3. passes the command's operand byte (and the player object in r7) to
 *      owPlayer_RequestFixedAnimation_809E14C, whose result comes back in r1,
 *   4. advances the script pointer past the 3-byte command.
 *
 * Outputs: r0 = 0, r1 = owPlayer_RequestFixedAnimation_809E14C's result, r2 = copied player byte,
 *          r4 = advanced script pointer, r7 = player object pointer.
 *          Flags: N/Z from "mov r0,#0" (Z set, N clear), C/V left over
 *          from the ADDS on the script pointer. r5, r6 and lr preserved.
 */

Next chatbox_cmd_playerFixedAnim_8041EE8(void) {
    /* mark the flag bit for this command in the chatbox state */
    R0 = 1; /* FLAGS_3E bit 0 */
    GUEST_CALL(chatbox_setflags_3e);

    /* look up the overworld player object and copy its byte 0x14
     * into the chatbox work area */
    R7 = LD32(LD32(R10 + oToolkit_GameStatePtr) + oGameState_OverworldPlayerObjectPtr);
    R2 = LD8(R7 + 0x14);
    ST8(R5 + 0x3C, R2);

    /* operand byte of the current script command. (The original's
     * "cmp r0,#7 / bgt next-line" here is a no-op — both paths fall
     * through, and the call below overwrites the flags anyway.) */
    R0 = LD8(R4 + 2);

    GUEST_CALL(owPlayer_RequestFixedAnimation_809E14C);

    /* advance to the next 3-byte command. The increment's carry/overflow
     * must survive, and mov r0,#0 afterwards forces N=0/Z=1 on top of
     * them without touching C/V. */
    u32 prevPos = R4;
    R4 = prevPos + 3;
    SET_FLAGS_ADD(prevPos, 3);
    R0 = 0;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── chatbox_scriptWaitPlayerFrameStep_8041F1C ── */
/* chatbox_scriptWaitPlayerFrameStep_8041F1C: chatbox tick handler.
 *
 * Sets chatbox flag bit 1 (via chatbox_setflags_3e), then looks up the
 * overworld player object through the toolkit's game state and asks for
 * its sprite frame parameters. Bit 0x80 of those parameters signals that
 * the current frame-sequence step has finished; while it is clear the
 * caller keeps waiting.
 * r4 is the text-script cursor: it is advanced by 2 once the bit is set.
 *
 * Returns r0 = 1 when the script cursor should advance, 0 otherwise.
 * NZ is set from r0 on both paths; on the advancing path C/V come from
 * the r4 += 2 addition. r1 is left holding frameParams & 0x80; r5 is
 * preserved. */
Next chatbox_scriptWaitPlayerFrameStep_8041F1C(void) {
    R0 = 1; /* FLAGS_3E bit 1 */
    GUEST_CALL(chatbox_setflags_3e);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    u32 savedR5 = R5;
    R5 = LD32(gameState + oGameState_OverworldPlayerObjectPtr);
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;
    R5 = savedR5;

    R1 = frameParams & 0x80;

    if (R1 != 0) {
        /* frame step finished: consume the next script word */
        SET_FLAGS_ADD(R4, 2);
        R4 = R4 + 2;
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        /* still animating: stay on the current script word */
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_scriptStepSyncPlayerTextState_8041F44 ── */
/* chatbox_scriptStepSyncPlayerTextState_8041F44: chatbox script step.
 *
 * Sets chatbox flag bit 0x0001, then reads the counter at [r5+0x3C]
 * (r5 = chatbox state). While that counter is still 7 or less it is copied
 * into the overworld player object's field 0x10 (which holds the current
 * text state). Finally calls owPlayer_RequestFixedAnimation_809E14C, advances the script cursor (r4)
 * by 2 and returns 0 with N/Z cleared; carry/overflow are left as the
 * "adds r4, #2" before it left them.
 *
 * Inputs:  r4 = script cursor, r5 = chatbox state ptr, r10 = toolkit.
 * Outputs: r0 = 0, r1 = whatever owPlayer_RequestFixedAnimation_809E14C leaves, r4 advanced by 2,
 *          r7 = player object (unchanged if the counter was > 7),
 *          nz/c/v as described above.
 * Preserved: r5, r6.
 */
Next chatbox_scriptStepSyncPlayerTextState_8041F44(void) {
    R0 = 1; /* FLAGS_3E_UNK_0001 */
    GUEST_CALL(chatbox_setflags_3e);

    R1 = R5 + 0x3C;
    u32 counter = LD8(R1);
    if ((s32)counter <= 7) {
        u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
        R7 = LD32(gameState + oGameState_OverworldPlayerObjectPtr);
        ST8(R7 + 0x10, counter);
    }

    /* owPlayer_RequestFixedAnimation_809E14C still sees the counter in r0 and the chatbox field
     * address in r1, mirroring the register state at this point. */
    R0 = counter;
    GUEST_CALL(owPlayer_RequestFixedAnimation_809E14C);

    u32 newCursor = R4 + 2;
    SET_FLAGS_ADD(R4, 2); /* carry/overflow come from this add */
    R4 = newCursor;

    R0 = 0;
    SET_FLAGS_NZ(R0);     /* ...but N/Z come from "mov r0, #0" */
    RETURN_TO_CALLER();
}

/* ── chatbox_loadNaviTextLine_80420BC ── */
/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u8 get_cur_pet_navi(void) {
    GUEST_CALL(GetCurPETNavi);
    return (u8)R0;
}

/* register form: calls CopyBytes outside this file, which has no typed C interface yet */
static void copy_bytes(u32 src, u32 dest, u32 count) {
    R0 = src;
    R1 = dest;
    R2 = count;
    GUEST_CALL(CopyBytes);
}

#define CHATBOX_SLOT_OFFSET     0x64
#define CHATBOX_TEXT_BLOCK_SIZE 0x40
#define CHATBOX_PTR_BASE        0x140

typedef struct {
    u32 dest;
    u32 ptr_offset;
    u32 slot_addr;
} ChatboxResult;

/* Queue one line of chat text: copy the speaking navi's text into the next
 * slot in the buffer pool, advance the line counter and script pointer. */
static ChatboxResult chatbox_load_text(u32 scriptPtr, u32 chatbox) {
    /* Bump the per-chatbox line counter */
    u8 counter = (u8)(LD8(chatbox + oChatbox_TextScriptDepth) + 1);
    ST8(chatbox + oChatbox_TextScriptDepth, counter);

    /* Identify which PET navi is speaking */
    u8 naviId = get_cur_pet_navi();

    /* Resolve the text source: table selector from command byte[2],
     * then a u16 offset per navi within that table */
    u32 selector   = LD8(scriptPtr + 2);
    u32 nameTable  = LD32(A_off_8042118 + selector);
    u32 textOffset = LD16(nameTable + (u32)naviId * 2);
    u32 textSrc    = nameTable + textOffset;

    /* Compute destination slot in the 64-byte-per-entry text pool */
    u32 slotAddr = chatbox + CHATBOX_SLOT_OFFSET;
    u16 slot     = LD16(slotAddr);
    u32 dest     = A_byte_200AFA0 + ((u32)slot << 6);

    /* Copy one block of text into the slot */
    copy_bytes(textSrc, dest, CHATBOX_TEXT_BLOCK_SIZE);

    /* Advance the slot index (re-read in case of aliasing) */
    slot = LD16(slotAddr);
    ST16(slotAddr, (u16)(slot + 1));

    /* Point the chatbox at the newly loaded text */
    ST32(chatbox + oChatbox_TextScriptCursorPtr, dest);

    /* Advance past this 3-byte script command */
    u32 newScript = scriptPtr + 3;
    ST32(chatbox + oChatbox_NextScriptCursorPtr, newScript);

    /* Store the script pointer at the per-line slot: base + (counter-1)*4 */
    u32 addend     = ((u32)counter - 1) * 4;
    u32 ptr_offset = CHATBOX_PTR_BASE + addend;
    ST32(chatbox + ptr_offset, newScript);

    ChatboxResult res = { dest, ptr_offset, slotAddr };
    return res;
}

/* chatbox_loadNaviTextLine_80420BC(r4 = scriptPtr, r5 = chatbox) -> r0=1, r4=dest, r7=slotAddr */
Next chatbox_loadNaviTextLine_80420BC(void) {
    ChatboxResult res = chatbox_load_text(R4, R5);
    R0 = 1;
    R1 = res.ptr_offset;
    R2 = 4;
    R4 = res.dest;
    R7 = res.slot_addr;
    SET_FLAGS_ADD(0, 1);
    RETURN_TO_CALLER();
}

/* ── chatbox_pushTextLevel_8042128 ── */
/*
 * Chatbox nesting step (chatbox_pushTextLevel_8042128).
 *
 * r4 points at a 4-byte text command word: byte 2 is a parameter for the text
 * setup routine, byte 3 selects a chatbox text slot.  r5 is the chatbox work
 * object.  Opening the command adds one nesting level, points the chatbox's
 * text cursor at the script to run (the slot's saved script, or one built by
 * protectedZennyOrZero_803D06C when the command doesn't name a slot), and records where the
 * outer command list resumes once this level closes.
 *
 * Final register states the callers rely on:
 *   r0 = 1, r1 = history slot offset, r2 = 4 (left over from the mul),
 *   r3 = whatever chatbox_formatBcdNumber_8042A14 left in it, r4 = the slot's text bank.
 */
typedef struct {
    u32 result;        /* r0: always 1 */
    u32 historyOffset; /* r1: offset of the history slot that received cmd+4 */
    u32 textBank;      /* r4: byte_200AFA0 + slot*64, the per-slot text buffer */
} ChatboxPushResult;

/* register form: calls protectedZennyOrZero_803D06C outside this file, which has no typed C interface yet */
static u32 default_text_script(u32 kind, u32 arg1, u32 arg2, u32 arg3) {
    R0 = kind; R1 = arg1; R2 = arg2; R3 = arg3;
    GUEST_CALL(protectedZennyOrZero_803D06C);
    /* the original pops r1-r3 back around this call, so put the entry values back */
    R1 = arg1; R2 = arg2; R3 = arg3;
    return R0;
}

/* register form: calls chatbox_formatBcdNumber_8042A14 outside this file, which has no typed C interface yet */
/* leftover unused: chatbox_formatBcdNumber_8042A14's r0 (its only caller discards the result) */
static u32 setup_text_script(u32 scriptPtr, u32 param, u32 textBank) {
    R0 = scriptPtr; R1 = param; R2 = textBank;
    GUEST_CALL(chatbox_formatBcdNumber_8042A14);
    /* r0 is unused here; r3 simply keeps whatever the callee left in it */
    return R0;
}

static ChatboxPushResult chatbox_push_text_level(u32 arg1, u32 arg2, u32 arg3, u32 cmd, u32 chat) {
    ChatboxPushResult out;

    /* one more nested text level is now open */
    ST8(chat + oChatbox_TextScriptDepth, LD8(chat + oChatbox_TextScriptDepth) + 1);

    /* where does the text for this box come from? */
    u32 slot = LD8(cmd + 3);
    u32 scriptPtr;
    if (slot != 0)
        scriptPtr = LD32(chat + 0x4C + (slot - 1) * 4);  /* script saved by the interrupted box */
    else
        scriptPtr = default_text_script(0, arg1, arg2, arg3);

    u32 textBank = A_byte_200AFA0 + (LD8(cmd + 3) << 6);
    setup_text_script(scriptPtr, LD8(cmd + 2), textBank);

    /* the setup routine may have edited the command word, so re-read the slot */
    slot = LD8(cmd + 3);
    textBank = A_byte_200AFA0 + (slot << 6);
    ST32(chat + oChatbox_TextScriptCursorPtr, textBank);

    /* when this level closes, resume the command list after this word */
    cmd += 4;
    ST32(chat + oChatbox_NextScriptCursorPtr, cmd);

    /* stack the resume point for the level we just opened */
    u32 depth = LD8(chat + oChatbox_TextScriptDepth);  /* re-read: the callee may have changed it */
    u32 historyOffset = 0x140 + (depth - 1) * 4;
    ST32(chat + historyOffset, cmd);

    out.result = 1;
    out.historyOffset = historyOffset;
    out.textBank = textBank;
    return out;
}

Next chatbox_pushTextLevel_8042128(void) {
    ChatboxPushResult out = chatbox_push_text_level(R1, R2, R3, R4, R5);
    R0 = out.result;
    R1 = out.historyOffset;
    R2 = 4;                 /* the original's "mov r2, #4" mul scratch survives to the end */
    R4 = out.textBank;
    /* the original ends with adds r4,r0,#0 then movs r0,#1: N/Z from 1, C/V cleared */
    SET_FLAGS_ADD(out.result, 0);
    RETURN_TO_CALLER();
}

/* ── chatbox_804222C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_CopyWords_result;
/* register form: generated wrapper: CopyWords has no typed C interface yet */
static inline call_CopyWords_result call_CopyWords(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyWords);
    return (call_CopyWords_result){ R3 };
}

/* Advance one chatbox text-script entry:
 * - bump the recursion-depth byte,
 * - look up the string (src pointer + word count) from the script table,
 * - copy the words into the next 64-byte slot of the text ring buffer,
 * - bump the slot index, store the cursor and the advanced script pointer,
 * - record the script pointer in a depth-indexed slot (0x140 + (depth-1)*4).
 * Returns the destination address (becomes the new r4). */
static u32 chatbox_advance(u32 script_ptr, u32 chatbox) {
    /* Bump the one-byte recursion depth on the chatbox struct. */
    u32 depth = LD8(chatbox + oChatbox_TextScriptDepth) + 1;
    ST8(chatbox + oChatbox_TextScriptDepth, depth);

    /* Each table entry is 8 bytes: [src u32, word_count u32]. */
    u32 table = A_byte_8043B54 + ((u32)LD8(script_ptr + 2) << 3);
    u32 src   = LD32(table);
    u32 count = LD32(table + 4);

    /* Ring-buffer destination: byte_200AFA0 + slot_index * 64. */
    u32 slot_field = chatbox + 0x64;
    u32 slot_idx   = LD16(slot_field);
    u32 dst        = A_byte_200AFA0 + (slot_idx << 6);

    /* Copy the string words into the slot. */
    call_CopyWords(src, dst, count);

    /* Advance the ring-buffer write slot. */
    ST16(slot_field, LD16(slot_field) + 1);   /* read again after the copy, as the original */

    /* Store cursor and new script position. */
    ST32(chatbox + oChatbox_TextScriptCursorPtr, dst);

    u32 new_ptr = script_ptr + 3;
    ST32(chatbox + oChatbox_NextScriptCursorPtr, new_ptr);

    /* Save script position in the depth-indexed slot. */
    ST32(chatbox + 0x140 + (LD8(chatbox + oChatbox_TextScriptDepth) - 1) * 4, new_ptr);

    return dst;
}

/* chatbox_804222C(r4 = script ptr, r5 = chatbox ptr) -> r0=1, r2=4, r4=dst */
Next chatbox_804222C(void) {
    R4 = chatbox_advance(R4, R5);
    R0 = 1;
    R2 = 4;
    RETURN_TO_CALLER();
}

/* ── chatbox_PlayScriptSoundCue_80422D4 ── */
/* chatbox_PlayScriptSoundCue_80422D4: play the sound cue at the current chatbox script position.
 * r4 points at a 4-byte cue record; bytes 2-3 hold the 16-bit sound effect
 * id (little-endian). Plays it, steps r4 past the record and returns r0 = 1.
 * Flags on return: NZ from "r0 = 1", C/V from the r4 += 4 add. */
Next chatbox_PlayScriptSoundCue_80422D4(void) {
    /* The original does push {lr} ... pop {pc}; the saved value is LR itself.
     * Reproduce the push so that SP during the PlaySoundEffect call matches
     * the original (the callee's stack activity must land at the same
     * addresses), then pop before returning. */
    SP -= 4;
    ST32(SP, LR);

    u32 soundId = (u32)LD8(R4 + 2) | ((u32)LD8(R4 + 3) << 8);

    R0 = soundId;
    /* The original's byte-merge leaves the shifted high byte in r1 at the
     * call site; hand PlaySoundEffect the same register contents. */
    R1 = (u32)LD8(R4 + 3) << 8;
    GUEST_CALL(PlaySoundEffect);

    /* Advance to the next cue record (the ADD sets C/V for the caller). */
    SET_FLAGS_ADD(R4, 4);
    R4 += 4;

    /* Report success; MOV r0, #1 refreshes N/Z while C/V stay from the add. */
    R0 = 1;
    SET_FLAGS_NZ(R0);

    SP += 4;
    RETURN_TO_CALLER();
}

/* ── chatbox_cmdPlayMusic_80422E8 ── */
/* chatbox_cmdPlayMusic_80422E8(r4 = chatbox script pointer):
 * Play the music track given as a little-endian u16 in the current chatbox
 * script command, then advance the script pointer past the command.
 * Leaves r4 pointing at the next command and returns r0 = 1.
 * Flags on exit: n/z from "movs r0, #1", c/v from "adds r4, #4". */
Next chatbox_cmdPlayMusic_80422E8(void) {
    u32 songLow  = LD8(R4 + 2);
    u32 songHigh = LD8(R4 + 3);

    /* Match the original's register state at the call: r1 holds the
     * shifted high byte, r0 the full halfword. */
    R1 = songHigh << 8;
    R0 = songLow | R1;
    GUEST_CALL(PlayMusic);      /* PlayMusic(song) */

    {
        u32 old = R4;
        R4 = old + 4;
        SET_FLAGS_ADD(old, 4);  /* c/v from "adds r4, #4" */
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);           /* n/z from "movs r0, #1" */

    RETURN_TO_CALLER();
}

/* ── chatbox_scriptOpClearFlag40_80422FC ── */
/* chatbox_scriptOpClearFlag40_80422FC: chatbox script opcode handler.
 * Clears chatbox status flag 0x40 (via chatbox_clearFlags_3e), advances the
 * script pointer in r4 past this opcode (2 bytes), and returns r0 = 1 so the
 * chatbox keeps running the script.
 * Flags left for the caller: N/Z from the final `mov r0, #1`, C/V from the
 * pointer increment. */
Next chatbox_scriptOpClearFlag40_80422FC(void) {
    /* clear flag FLAGS_3E_UNK_0040 in the chatbox status field */
    R0 = FLAGS_3E_UNK_0040;
    GUEST_CALL(chatbox_clearFlags_3e);

    /* step the script pointer over this opcode */
    u32 prevPtr = R4;
    R4 = prevPtr + 2;
    SET_FLAGS_ADD(prevPtr, 2);

    /* signal the chatbox to continue */
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_stateSetFlag40Advance_804230C ── */
/* chatbox_stateSetFlag40Advance_804230C: step the chatbox forward.
 * Sets chatbox flag 0x40 (via chatbox_setflags_3e), advances the caller's
 * chatbox state pointer (r4) by 2 bytes, and returns 1.
 * Flags on return: N/Z from the result value 1, C/V from the r4 += 2 add.
 * Preserves r1 (chatbox_setflags_3e is a leaf and keeps r1/r4 intact). */
Next chatbox_stateSetFlag40Advance_804230C(void) {
    R0 = FLAGS_3E_UNK_0040;
    GUEST_CALL(chatbox_setflags_3e);

    SET_FLAGS_ADD(R4, 2);   /* ADDS r4, #2 */
    R4 += 2;

    R0 = 1;
    SET_FLAGS_NZ(1);        /* MOVS r0, #1 sets N/Z, leaves C/V */

    RETURN_TO_CALLER();
}

/* ── chatbox_scriptCmdSetFlags3e80_8042350 ── */
/* chatbox_scriptCmdSetFlags3e80_8042350: set chatbox flag bit FLAGS_3E_UNK_0080, then advance the
 * caller's chatbox cursor (r4) by 2 and return 1.  r1 is preserved.
 * Flags: N/Z come from the final "movs r0, #1"; C/V are carried over from
 * the "adds r4, #2" that precedes it. */
Next chatbox_scriptCmdSetFlags3e80_8042350(void) {
    u32 old_r4 = R4; /* r4 is live across the setflags call */

    R0 = FLAGS_3E_UNK_0080;
    GUEST_CALL(chatbox_setflags_3e); /* (int mask) -> void */

    R4 = old_r4 + 2;
    SET_FLAGS_ADD(old_r4, 2);

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER(); /* pop {pc}: return to the saved LR, unchanged */
}

/* ── chatbox_8042370 ── */
/* chatbox_8042370:
 * Read a 2-byte (sound_id, ???) pair from r4+2, play it as a SE with priority 0x1F,
 * advance r4 by 4, and return 1 (the entry was consumed). */
Next chatbox_8042370(void) {
    u32 sound_id = LD8(R4 + 2);
    u32 param    = LD8(R4 + 3);
    R0 = sound_id;
    R1 = param;
    R2 = 0x1F;
    GUEST_CALL(sound_80006A2);
    R4 += 4;
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── chatbox_FB_special ── */
/* Chatbox "FB special" script command (08042388).
 *
 * The command word pointed to by r4 is a script action: the byte at offset 1 selects an
 * entry in the jump table jt_804239C, and the handler found there is called and simply
 * returns back through this dispatcher to whoever ran the script step.
 *
 * Register state at the call, exactly as the original dispatcher leaves it:
 *   r0    = the raw jump-table entry (handler address | thumb bit)
 *   r1    = action byte * 4 (the scaled table offset)
 *   r2-r12, V flag = passed through untouched
 * The lsl that scales the index also sets nz/c in the original; those derived values are
 * not part of the caller contract, so they are not recreated here. Everything the outer
 * caller sees afterwards (r0-r3/r12, the flags) is whatever the handler left behind. */

/* register form: calls the chatbox action handlers outside this file, which have no typed
 * C interface yet, and the handler must be entered with the jump-table entry in r0, the
 * scaled index in r1 and r2-r12/V untouched, so this cannot be an ordinary typed call. */
static void call_chatbox_action_handler(u32 action)
{
    u32 table_offset = action * 4;

    /* Load the entry from the game's table so r0 matches the original bit for bit
     * (including the thumb bit) and the table stays the single source of truth. */
    R0 = LD32(A_jt_804239C + table_offset);
    R1 = table_offset;

    switch (action) {
    case 0:  GUEST_CALL(chatbox_80423DC);                          break;
    case 1:  GUEST_CALL(chatbox_8042418);                          break;
    case 2:  GUEST_CALL(chatbox_804244C);                          break;
    case 3:  GUEST_CALL(chatbox_804247C);                          break;
    case 5:  GUEST_CALL(chatbox_textScriptAction_openShop_80424BC); break;
    case 6:  GUEST_CALL(chatbox_80424E0);                          break;
    case 8:  GUEST_CALL(chatbox_804252C);                          break;
    case 10: GUEST_CALL(chatbox_8042580);                          break;
    case 11: GUEST_CALL(chatbox_80425A0);                          break;
    case 13: GUEST_CALL(chatbox_80425E0);                          break;
    case 14: GUEST_CALL(chatbox_80425C0);                          break;
    case 15: GUEST_CALL(chatbox_launchMailOption_80425F0);                          break;
    default:
        /* Table entries 4, 7, 9 and 12 are null; the original would jump to address 0
         * and crash. Real scripts never request them. */
        break;
    }
}

/* Read the action byte out of the current script command and run its handler. */
static void run_chatbox_FB_special(u32 script_ptr)
{
    u32 action = LD8(script_ptr + 1);

    call_chatbox_action_handler(action);
}

/* chatbox_FB_special(r4 = current script command pointer)
 * All outputs (r0-r12, nz/c/v) are whatever the dispatched handler left behind, so the
 * adapter only forwards r4 and returns. */
Next chatbox_FB_special(void) {
    run_chatbox_FB_special(R4);
    RETURN_TO_CALLER();
}

/* ── chatbox_textScriptAction_openShop_80424BC ── */
/* chatbox_textScriptAction_openShop_80424BC: chatbox text-script action used at shop counters.
 * Sets chatbox flag 0x3E bit 1 (the "close chatbox" mask, FLAGS_3E_UNK_0001
 * in the disassembly), and if this script step hasn't fired yet, launches
 * the shop whose id is stored at [r4+2] and marks the step as taken by
 * storing 1 into the text-script state byte at [r5]+4. Returns 0 in r0.
 *
 * Flag note: the final `mov r0, #0` only rewrites N and Z; C and V reach
 * the caller from the `cmp r0, #0` (C=1, V=0) on the skip path, or from
 * subsystem_launchShop on the launch path. */
Next chatbox_textScriptAction_openShop_80424BC(void) {
    R0 = 1;                    /* FLAGS_3E_UNK_0001: close-chatbox flag */
    GUEST_CALL(chatbox_setflags_3e);

    u32 state = LD8(R5 + 4);   /* oChatbox_TextScriptState_04 */
    SET_FLAGS_CMP(state, 0);   /* cmp r0, #0: N/Z from value, C=1, V=0 */
    if (state == 0) {
        R0 = LD8(R4 + 2);      /* shop id for subsystem_launchShop */
        GUEST_CALL(subsystem_launchShop);
        ST8(R5 + 4, 1);        /* mark step as done */
    }

    R0 = 0;                    /* mov r0, #0: N=0, Z=1; C/V untouched */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_launchMailOption_80425F0 ── */
/* chatbox_launchMailOption_80425F0: launch the mail subsystem for a chatbox reply option.
 * inputs:  r4 (chatbox object), r5 (text-script state), r10, lr
 * outputs: r0 = 0 (with nz from it), r2/r3 unchanged, r4/r5 preserved
 *
 * First tick: marks FLAGS_3E_UNK_0001, reads the mail index from [r4+2], launches
 * the mail subsystem, and sets the guard byte at [r5+4] so it only runs once. */
Next chatbox_launchMailOption_80425F0(void) {
    /* mark FLAGS_3E_UNK_0001 */
    R0 = 1;
    GUEST_CALL(chatbox_setflags_3e);

    if (LD8(R5 + oChatbox_TextScriptState_04) == 0) {
        /* first tick: launch the mail subsystem with the index stored at [r4+2] */
        R0 = LD8(R4 + 2);
        GUEST_CALL(subsystem_launchMail);

        /* mark the chatbox text-script state as having launched so we don't repeat */
        ST8(R5 + oChatbox_TextScriptState_04, 1);
    }

    /* tail: r0 = 0, flags from that mov (nz set from the value) */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_processScriptEntry_8042678 ── */
/* chatbox_processScriptEntry_8042678: handle one chatbox script entry.
 *
 * r4 points at the chatbox entry to process.  The chatbox core helper
 * (sub_803F798) is consulted first; when it reports 0 a sound effect is
 * played.  Its result is then reused as an offset, and the script ID byte
 * at r4+offset+2 is examined:
 *   - any value other than 0xFF is forwarded to chatbox_config_80408D0
 *     (which starts the corresponding text script),
 *   - 0xFF marks the end of the entry, so r4 is advanced past it (5 bytes).
 * Always returns 1 in R0. */
Next chatbox_processScriptEntry_8042678(void) {
    u32 savedR4 = R4;

    /* Consult the chatbox core; it gets the caller's r0..r3 unchanged.
     * Its result doubles as the offset of the script-ID field. */
    GUEST_CALL(sub_803F798);
    u32 state = R0;
    R4 = savedR4;                 /* pop {r4}: keep the caller's entry pointer */

    if (state == 0) {
        /* Nothing pending in the chatbox core: play the blip. */
        R0 = SOUND_UNK_74;
        GUEST_CALL(PlaySoundEffect);
        R4 = savedR4;             /* pop {r0,r4}: sound call may clobber r4 */
    }

    R1 = LD8(R4 + state + 2);
    if (R1 != 0xFF) {
        /* Hand the script ID to the chatbox configuration routine. */
        R0 = state + 2;
        GUEST_CALL(chatbox_config_80408D0);
        R0 = 1;
        SET_FLAGS_NZ(R0);         /* mov r0, #1 */
    } else {
        /* End marker: skip this 5-byte entry. */
        SET_FLAGS_ADD(R4, 5);     /* add r4, #5 sets carry/overflow */
        R4 = R4 + 5;
        R0 = 1;
        SET_FLAGS_NZ(R0);         /* mov r0, #1 */
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_setFlag0x1000AndAdvanceStep_80426E0 ── */
/* chatbox_setFlag0x1000AndAdvanceStep_80426E0: raise chatbox flag 0x1000 (via chatbox_setflags_3e), then
 * advance the current chatbox state's step counter (r4) by 2. Returns r0 = 1.
 * Final flags: N/Z from the "mov r0,#1", C/V from the "adds r4,#2". */
Next chatbox_setFlag0x1000AndAdvanceStep_80426E0(void) {
    R0 = 0x1000;
    GUEST_CALL(chatbox_setflags_3e);

    {
        u32 prev = R4;
        R4 = prev + 2;
        SET_FLAGS_ADD(prev, 2);
    }

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_stepUpdateAndContinue_8042820 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r4, r5, r6, r7; } call_sub_809CA84_result;
/* register form: generated wrapper: sub_809CA84 has no typed C interface yet */
static inline call_sub_809CA84_result call_sub_809CA84(void) {
    GUEST_CALL(sub_809CA84);
    return (call_sub_809CA84_result){ R0, R2, R4, R5, R6, R7 };
}

/* Chatbox step (0x08042820).
 *
 * sub_809CA84 does the per-frame chatbox processing. It returns the value the
 * chatbox control block should remember (r0) and refreshes the box's working
 * registers (r2, r4, r5, r6, r7); it runs with the caller's r4/r5/r10 still
 * in place, since the wrapper passes no argument registers.
 *
 * This function then:
 *   - stores the returned value into the control block at r5+0x4C, and
 *   - advances the r4 counter by 2.
 *
 * The caller only consumes r0 (= 1, "continue") and the registers sub_809CA84
 * left behind; N/Z/C/V, r1 and lr are clobbered per the contract, so no flag
 * setting is needed here. */
static u32 chatbox_step(void) {
    call_sub_809CA84_result box = call_sub_809CA84();

    /* Record the value sub_809CA84 produced in the chatbox control block. */
    ST32(box.r5 + 0x4C, box.r0);

    /* The chatbox cursor/counter moves up by two each step. */
    return box.r4 + 2;
}

Next chatbox_stepUpdateAndContinue_8042820(void) {
    /* r4 exits advanced by 2 and r0 is the success value; r2/r5/r6/r7 were
       already left in the guest registers by the call to sub_809CA84, so the
       adapter must not write them again. */
    R4 = chatbox_step();
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── chatbox_F7_move ── */
/* chatbox_F7_move: 08042830 THUMB.
 * Tail-dispatch for the chatbox state machine: r4 points at the chatbox
 * object and the byte at +1 is its state, which selects one of ten update
 * handlers. The handler runs with R1 = state*4 (and R0 = its own entry
 * address, as the original's computed `mov lr, pc / bx r0` call leaves it)
 * and returns straight to our caller, so its register and flag results are
 * ours. States are always 0-9 in practice; the raw table walk of the
 * original is represented here as a direct switch. */
Next chatbox_F7_move(void) {
    u32 state = LD8(R4 + 1);

    R1 = state << 2;
    switch (state) {
    case 0: R0 = 0x0804286C; GUEST_CALL(chatbox_setChoiceCursorGeometry); break;
    case 1: R0 = 0x080428B8; GUEST_CALL(sub_80428B8); break;
    case 2: R0 = 0x080428C0; GUEST_CALL(chatbox_initFromScriptEntry_80428C0); break;
    case 3: R0 = 0x080428E4; GUEST_CALL(sub_80428E4); break;
    case 4: R0 = 0x080428EC; GUEST_CALL(sub_80428EC); break;
    case 5: R0 = 0x080428FC; GUEST_CALL(sub_80428FC); break;
    case 6: R0 = 0x08042910; GUEST_CALL(sub_8042910); break;
    case 7: R0 = 0x0804293C; GUEST_CALL(chatbox_placeCenteredTextRun_804293C); break;
    case 8: R0 = 0x08042990; GUEST_CALL(chatbox_centerTextX); break;
    case 9: R0 = 0x080429D8; GUEST_CALL(sub_80429D8); break;
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_initFromScriptEntry_80428C0 ── */
/* chatbox_initFromScriptEntry_80428C0: Initialize chatbox state from a script entry.
 *  inputs:  r4 = script entry pointer, r5 = chatbox struct pointer, lr
 *  outputs: r0 = 1, r2 = 0xC * entry[4], r3 = entry[4], r4 = original r4 + 5
 *  clobbers: r1, nz, c, v */
Next chatbox_initFromScriptEntry_80428C0(void) {
    u8 charIdx = LD8(R4 + 2);
    ST8(R5 + oChatbox_TextScriptCharIdx, charIdx);
    ST8(R5 + oChatbox_CursorIdx, (u8)(charIdx + 0xAF));

    u8 row = LD8(R4 + 3);
    ST8(R5 + oChatbox_CursorY, row);

    u8 stride = LD8(R4 + 4);
    ST8(R5 + oChatbox_CursorX, (u8)(row + 0xC * stride));

    ST8(R5 + 0x94, charIdx);

    R4 += 5;
    R2 = 0xC * stride;
    R3 = stride;

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80428EC ── */
/* sub_80428EC(r4 = source ptr, r5 = chatbox ptr):
 * copy cursor index and an adjacent byte from the source into the chatbox,
 * advance the source pointer by 4, and return 1 in r0. */
Next sub_80428EC(void) {
    u8 cursor = LD8(R4 + 2);
    ST8(R5 + oChatbox_CursorIdx, cursor);
    ST8(R5 + oChatbox_CursorX, LD8(R4 + 3));
    R4 += 4;
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── sub_80428FC ── */
/* sub_80428FC(r4 = source ptr, r5 = dest struct, lr):
 *   Copy two bytes from the source at offsets 2 and 3 into
 *   the destination at offsets 0x84 and 0x88 (32-bit stores),
 *   advance the source pointer by 4, and return 1 in r0. */
Next sub_80428FC(void) {
    ST32(R5 + 0x84, LD8(R4 + 2));
    ST32(R5 + 0x88, LD8(R4 + 3));
    R4 += 4;
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_placeCenteredTextRun_804293C ── */
/* register form: calls chatbox_maskFlags_3e outside this file, which has no typed C interface yet */
static bool chatbox_layout_flag_set(u32 mask) {
    R0 = mask;
    GUEST_CALL(chatbox_maskFlags_3e);
    return !ZERO_FLAG();          /* chatbox_maskFlags_3e: Z set when no masked flag is set */
}

/* Outputs of the chatbox "place next text run" script command. */
typedef struct {
    u32 result;         /* r0: always 1 (command consumed) */
    u32 nextDrawPos;    /* r1: new oChatbox_Unk_7C */
    u32 alignedOffset;  /* r2: start offset rounded down to a multiple of 8 pixels */
    u32 nextCharX;      /* r3: byte stored to chatbox+0x94 */
    u32 scriptPtr;      /* r4: command pointer before the caller steps over it */
} ChatboxPlaceText;

/* Text script command that horizontally places the next run of chatbox text.
 * r4 points at the 3-byte command (byte 2 = run length in tiles), r5 at the
 * chatbox object. Computes the centered start X, records it in the chatbox,
 * and leaves the script pointer for the caller to step past the command. */
static ChatboxPlaceText chatbox_place_text(u32 scriptPtr, u32 chatbox)
{
    ChatboxPlaceText out;

    /* Pixel width of the run: 11 per tile except the last, plus 16 margin. */
    u32 width = 11u * (LD8(scriptPtr + 2) - 1u) + 16u;

    /* Right edge of the text area: 176 with the alternate layout flag, else 240. */
    u32 rightEdge = chatbox_layout_flag_set(2) ? 0xB0u : 0xF0u;

    /* Centered start offset: half of the free space... */
    u32 startX = (rightEdge - width) >> 1;

    /* ...shifted back by the characters already printed on the current line
     * when the layout flag is clear. */
    if (!chatbox_layout_flag_set(2))
        startX -= LD8(chatbox + oChatbox_TextScriptCharIdx);

    u32 charIdx = LD8(chatbox + oChatbox_TextScriptCharIdx);
    u32 nextCharX = startX + charIdx;
    ST8(chatbox + 0x94, nextCharX);

    /* Round the start offset down to a multiple of 8 pixels
     * ((x >> 1) << 1 drops the low bit, << 2 scales to pixels/4)
     * and record it as the position the text renderer draws from next. */
    u32 alignedOffset = (startX >> 1) << 3;
    u32 nextDrawPos = LD32(chatbox + oChatbox_Unk_7C) + alignedOffset;
    ST32(chatbox + oChatbox_Unk_7C, nextDrawPos);

    out.result = 1;
    out.nextDrawPos = nextDrawPos;
    out.alignedOffset = alignedOffset;
    out.nextCharX = nextCharX;
    out.scriptPtr = scriptPtr;
    return out;
}

/* chatbox_placeCenteredTextRun_804293C(r4 = text script command pointer, r5 = chatbox object)
 * -> r0 = 1, r1 = new draw position, r2 = aligned offset, r3 = next char X,
 *    r4 = command pointer + 3.
 * Flags: the original ends with adds r4,#3 then movs r0,#1, leaving
 * N=Z=0 (both results small and positive) and C=V=0 (a script pointer plus
 * 3 cannot carry or overflow). SET_FLAGS_ADD on the same r4+3 yields exactly
 * that N/Z/C/V combination. */
Next chatbox_placeCenteredTextRun_804293C(void) {
    ChatboxPlaceText out = chatbox_place_text(R4, R5);
    R0 = out.result;
    R1 = out.nextDrawPos;
    R2 = out.alignedOffset;
    R3 = out.nextCharX;
    R4 = out.scriptPtr + 3;
    SET_FLAGS_ADD(out.scriptPtr, 3);
    RETURN_TO_CALLER();
}

/* ── sub_80429D8 ── */
/* sub_80429D8(r4 = ?, r5 = chatbox struct):
 *   Internal chatbox helper. Computes a half-width offset (r2/r8), writes
 *   the next script slot index at chatbox + 0x94, bumps an internal
 *   pointer by an 8-byte-aligned slot, and advances r4 by 3. Returns 1.
 *
 *   inputs:  r4 r5 lr
 *   outputs: r0 r2 r3 r4 r6 r7 r8
 *   clobbers: nz c v r1
 *   preserved: r6 r7
 */
Next sub_80429D8(void) {
    /* Total vertical region (0xAC): the baseline the per-row offset is taken against. */
    u32 region = 0x79 + 0x33;

    /* 0-based row index from r4, used to scale row size. */
    u32 row = LD8(R4 + 2) - 1;

    /* Combined row advance: row * 0xB + (8 * 2). */
    u32 base = row * 0xB + (8 * 2);

    /* Half of (region - base); reused as the script cursor offset. */
    u32 half = (region - base) >> 1;
    R8 = half;

    /* Current script cursor (character index). */
    u32 cursor = LD8(R5 + oChatbox_TextScriptCharIdx);

    /* Write the new cursor at chatbox + 0x94. */
    ST8(R5 + 0x94, (u8)(half + cursor));

    /* Bump the chatbox's working pointer by one 8-byte-aligned slot. */
    u32 step = ((half >> 1) << 1) << 2;     /* (half / 2, rounded down) * 8 */
    ST32(R5 + oChatbox_Unk_7C, LD32(R5 + oChatbox_Unk_7C) + step);

    /* Output registers that callers branch on. R2 is the final scaled step. */
    R2 = step;
    R3 = half + cursor;

    /* Step the caller's iterator forward by 3 entries. */
    R4 += 3;

    /* Result = 1. */
    SET_FLAGS_NZ(1);
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── chatbox_formatBcdNumber_8042A14 ── */
/* register form: calls memory_bcdConvert outside this file, which has no typed C interface yet.
 * Callee clobbers R1; save and restore it to honour the preserved-r1 contract. */
static u32 to_bcd(u32 value, u32 flags, u32 buf) {
    u32 saved = R1;
    R0 = value;
    R1 = flags;
    R2 = buf;
    GUEST_CALL(memory_bcdConvert);
    R1 = saved;
    return R0;
}

/* register form: calls countHexDigits_8000C5C outside this file, which has no typed C interface yet.
 * May clobber R1; restore to the caller's original. */
static u32 hex_digit_count(u32 bcd) {
    u32 saved = R1;
    R0 = bcd;
    GUEST_CALL(countHexDigits_8000C5C);
    R1 = saved;
    return R0;
}

/* Format a BCD number into a chatbox glyph string.
 *   value  – integer to display (r0)
 *   flags  – bit 0-3: minimum field width (0 → natural digit count)
 *            bit 6:   1 = pad with glyph 1 (space), 0 = glyph 0 (blank)
 *            bit 7:   1 = right-align (pad before digits), 0 = left-align (pad after)
 *   buf    – write cursor destination (r2)
 * Outputs r3 (padding remainder), r6 (fill glyph), r8 (BCD value). */
static void format_chatbox_number(u32 value, u32 flags, u32 buf,
                                  u32 *out_r3, u32 *out_r6, u32 *out_r8)
{
    u32 bcd    = to_bcd(value, flags, buf);
    u32 digits = hex_digit_count(bcd);

    /* Fill glyph: index 1 (space) when bit 6 set, index 0 otherwise */
    u32 fill = (flags & 0x40) ? 1 : 0;

    /* Desired minimum width from low nibble; 0 means "natural" */
    u32 width = flags & 0xF;
    if (width == 0)
        width = digits;

    /* Signed padding count: > 0 means pad needed, <= 0 means none */
    s32 pad = (s32)width - (s32)digits;

    /* Bit-shift to reach the most-significant BCD nibble */
    s32 shift = (s32)((digits - 1) << 2);

    bool right_align = (flags & 0x80) != 0;

    if (!right_align) {
        /* Left-aligned: write significant digits (MSB → LSB), then trailing pad */
        do {
            u32 nib = (bcd >> (u32)shift) & 0xF;
            ST8(buf, nib + 1);   /* +1 = font glyph base offset */
            buf++;
            shift -= 4;
        } while (shift >= 0);

        while (pad > 0) {
            ST8(buf, fill);
            buf++;
            pad--;
        }
    } else {
        /* Right-aligned: write leading pad first, then digits */
        while (pad > 0) {
            ST8(buf, fill);
            buf++;
            pad--;
        }

        do {
            u32 nib = (bcd >> (u32)shift) & 0xF;
            ST8(buf, nib + 1);
            buf++;
            shift -= 4;
        } while (shift >= 0);
    }

    /* Terminator: glyph 0xE6 followed by two 0x00 bytes */
    ST8(buf, 0xE6);
    ST8(buf + 1, 0);
    ST8(buf + 2, 0);

    *out_r3 = (u32)pad;   /* padding remainder (0 or negative-as-u32) */
    *out_r6 = fill;
    *out_r8 = bcd;
}

/* chatbox_formatBcdNumber_8042A14(r0=value, r1=flags, r2=buffer) -> r3, r6, r7=0, r8; preserves r1,r4,r5 */
Next chatbox_formatBcdNumber_8042A14(void) {
    u32 r3, r6, r8;
    format_chatbox_number(R0, R1, R2, &r3, &r6, &r8);
    R3 = r3;
    R6 = r6;
    R7 = 0;
    R8 = r8;
    RETURN_TO_CALLER();
}

/* ── chatbox_entryMatchesBuffer_8042EE8 ── */
/* Compare one chatbox entry against a byte buffer.
 * r0 selects the entry (index * 8 into the table at byte_8045CCC),
 * r1 is the byte count (treated as signed by the original's bgt loop),
 * r5 points at a record whose +0x4C buffer is compared.
 * Returns 1 if all bytes match, 0 on the first difference.
 * Note: like the original, one byte is compared even when the count is <= 0. */
static u32 chatbox_entry_matches(u32 index, s32 count, u32 record) {
    u32 entry = A_byte_8045CCC + (index << 3);
    u32 buf = record + 0x4C;
    u32 i = 0;

    do {
        if (LD8(entry + i) != LD8(buf + i))
            return 0;
        i++;
    } while (--count > 0);

    return 1;
}

/* chatbox_entryMatchesBuffer_8042EE8(r0 = entry index, r1 = byte count, r5 = record base) -> r0 */
Next chatbox_entryMatchesBuffer_8042EE8(void) {
    R0 = chatbox_entry_matches(R0, (s32)R1, R5);
    RETURN_TO_CALLER();
}

/* ── chatbox_setCutsceneStateByte_8042FF4 ── */
/* chatbox_setCutsceneStateByte_8042FF4(r4 = chatbox script command pointer, r10 = toolkit):
 * Chatbox script opcode handler. Copies the byte at cmd[3] into the
 * cutscene state's byte array (base + 8) at index cmd[2], then advances
 * the command pointer past this 4-byte opcode.
 * Leaves r0 = 1 (result code for the script interpreter), r1 = the value
 * written, r2 = cutscene state byte-array base, r4 = next command.
 * Flags: nz from "mov r0, #1", c/v from the final "adds r4, #4". */
Next chatbox_setCutsceneStateByte_8042FF4(void) {
    u32 prevR4 = R4;

    R2 = LD32(R10 + oToolkit_CutsceneStatePtr) + 8; /* cutscene state byte array */
    R0 = LD8(R4 + 2); /* index into the byte array */
    R1 = LD8(R4 + 3); /* byte to store */
    ST8(R2 + R0, R1);

    R4 = prevR4 + 4;
    SET_FLAGS_ADD(prevR4, 4);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_storeWord_8045EDC ── */
/* Write one 32-bit word into the chatbox's word buffer.
 * r0 = slot index (scaled by 4), r1 = value to store, r10 = toolkit whose
 * oToolkit_ChatboxPtr field points at the active chatbox.
 * The buffer starts at offset 0x4C in the chatbox structure.
 * The original preserves r5 (push/pop), but callers treat it as an output,
 * so it is passed in and returned unchanged. */
static u32 chatbox_store_word(u32 slot, u32 value, u32 toolkit, u32 r5_unchanged) {
    u32 chatbox = LD32(toolkit + oToolkit_ChatboxPtr);

    ST32(chatbox + 0x4C + (slot << 2), value);
    return r5_unchanged;
}

/* chatbox_storeWord_8045EDC(r0 = slot index, r1 = value, r10 = toolkit) -> r5 (unchanged) */
Next chatbox_storeWord_8045EDC(void) {
    R5 = chatbox_store_word(R0, R1, R10, R5);
    RETURN_TO_CALLER();
}

/* ── chatbox_setTextParams_8045EFC ── */
/* chatbox_setTextParams_8045EFC(r0..r3 = four chatbox parameters):
 * Store the four arguments into the active chatbox (looked up through the
 * toolkit); they are the text/pointer fields the chatbox task reads back
 * later. All four argument registers and the chatbox pointer are untouched
 * on return. */
Next chatbox_setTextParams_8045EFC(void) {
    u32 chatbox = LD32(R10 + oToolkit_ChatboxPtr);

    ST32(chatbox + oChatbox_TextParam1, R0);
    ST32(chatbox + oChatbox_TextParam2, R1);
    ST32(chatbox + oChatbox_ParamC, R2);
    ST32(chatbox + oChatbox_TextParam4, R3);

    RETURN_TO_CALLER();
}

/* ── chatbox_set_eFlags2009F38 ── */
/* chatbox_set_eFlags2009F38(r0 = bits to set): set the given bits in the
 * eFlags2009F38 event-flag word. r1 and r2 are preserved; the nz flags
 * reflect the new flag value. */
Next chatbox_set_eFlags2009F38(void) {
    u32 newFlags = LD32(A_eFlags2009F38) | R0;
    ST32(A_eFlags2009F38, newFlags);
    SET_FLAGS_NZ(newFlags);
    RETURN_TO_CALLER();
}

/* ── chatbox_clear_eFlags2009F38 ── */
/* chatbox_clear_eFlags2009F38(r0 = bit mask): clear the given bits in the
 * chatbox event flags at 0x2009F38. r0 = mask of bits to clear.
 * Leaves r1 and r2 untouched; flags reflect the new flag word. */
Next chatbox_clear_eFlags2009F38(void) {
    u32 mask = R0;
    u32 flags = LD32(A_eFlags2009F38) & ~mask;

    SET_FLAGS_NZ(flags);
    ST32(A_eFlags2009F38, flags);
    RETURN_TO_CALLER();
}

/* ── chatbox_mask_eFlags2009F38 ── */
/* chatbox_mask_eFlags2009F38(r0 = flag bit(s)):
 * Return the given flag bits as they currently stand in the chatbox/
 * cutscene flag word at eFlags2009F38. R1 is left untouched, and the
 * NZ flags reflect the masked value so callers can branch on it. */
Next chatbox_mask_eFlags2009F38(void) {
    R0 &= LD32(A_eFlags2009F38);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_GetState_8045F4C ── */
/* chatbox_GetState_8045F4C: read the low three bits of the event-flags word at eFlags2009F38
 * (the chatbox/text state selector). Returns the masked value in r0 and sets
 * nz on it, like the original's ANDS. r1 is preserved. */
Next chatbox_GetState_8045F4C(void) {
    u32 saved_r1 = R1;
    u32 flags = LD32(A_eFlags2009F38);

    R0 = flags & 7;
    SET_FLAGS_NZ(R0);
    R1 = saved_r1;
    RETURN_TO_CALLER();
}

/* ── chatbox_resetAndClearBuffers_8045F60 ── */
/* chatbox_resetAndClearBuffers_8045F60(r5 = chatbox work area):
 * Reset the chatbox: clear its state fields, then wipe the text buffer at
 * unk_200BEA0 (0xF00 bytes) and the glyph/tile buffer at dword_200CDA0
 * (same 0xF00 size, still in r1 from the first call).
 * Leaves r1 = 0xF00 and the flags from the last ZeroFillByEightWords. */
Next chatbox_resetAndClearBuffers_8045F60(void) {
    ST32(R5 + oChatbox_TileBufferOffset, 0);
    ST8(R5 + oChatbox_BoxGfxLoadState_0F, 0);
    ST8(R5 + oChatbox_ResetFlag, 0);
    ST32(R5 + oChatbox_Unk_7C, 0);

    R0 = A_unk_200BEA0;
    R1 = 0xF00;
    GUEST_CALL(ZeroFillByEightWords);

    R0 = A_dword_200CDA0; /* r1 still holds the 0xF00 length */
    GUEST_CALL(ZeroFillByEightWords);

    RETURN_TO_CALLER();
}
