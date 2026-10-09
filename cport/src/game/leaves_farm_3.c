/*
 * leaves_farm_3.c: small functions from the model farm, set 3.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── zeroFill_e2002230 ── */
/* zeroFill_e2002230: clear the game's extra work-RAM block.
 * Reads the toolkit pointer from r10 and fills the 0xF00-byte block at
 * oToolkit_Unk2002230_Ptr with zeros. Inputs r2, r3 and r10 are preserved;
 * the caller's r3 is returned unchanged. */
Next zeroFill_e2002230(void) {
    u32 block = LD32(R10 + oToolkit_Unk2002230_Ptr);

    R0 = block;
    R1 = 0xF00; /* size of the block in bytes */
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── GetBattleEffects ── */
/* GetBattleEffects: 0802D246-0802D254 THUMB, bn6f asm03_0.s:14417
 * Fetches the battle-effects bitmask for the current battle: load the
 * battle state from the toolkit, its battle settings from the battle
 * state, and delegate to GetBattleEffectsFromBattleSettings. */
Next GetBattleEffects(void) {
    u32 *battleState = (u32 *)LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD32((u32)battleState + oBattleState_BattleSettings);
    GUEST_CALL(GetBattleEffectsFromBattleSettings);
    RETURN_TO_CALLER();
}

/* ── GetBattleEffectsFromBattleSettings ── */
/* GetBattleEffectsFromBattleSettings(r0 = battle settings ptr):
 * returns the optional-effect settings word from the battle settings struct. */
Next GetBattleEffectsFromBattleSettings(void) {
    R0 = LD32(R0 + 8 /* oBattleSettings_UnkOptSettings */);
    RETURN_TO_CALLER();
}

/* ── GetBattleModeFromBattleSettings ── */
/* GetBattleModeFromBattleSettings(r0 = battle settings pointer):
 * return the battle mode byte stored at offset 3 of the settings struct. */
Next GetBattleModeFromBattleSettings(void) {
    R0 = LD8(R0 + 3);
    RETURN_TO_CALLER();
}

/* ── GetBattlePanelColumnPattern ── */
/* GetBattlePanelColumnPattern:
 * Returns the current battle panel column pattern, reading it from the
 * battle settings of the battle state the toolkit (r10) points at. */
Next GetBattlePanelColumnPattern(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD32(battleState + oBattleState_BattleSettings);
    GUEST_CALL(GetBattlePanelColumnPatternFromBattleSettings);
    RETURN_TO_CALLER();
}

/* ── GetBattlePanelColumnPatternFromBattleSettings ── */
/* GetBattlePanelColumnPatternFromBattleSettings(r0 = battle settings ptr):
 * returns the panel column pattern byte (offset 6) from the battle settings. */
Next GetBattlePanelColumnPatternFromBattleSettings(void) {
    R0 = LD8(R0 + 6);
    RETURN_TO_CALLER();
}

/* ── GetBattleSettingsUnk01 ── */
/* GetBattleSettingsUnk01():
 * Return the settings field (Unk01) of the current battle's BattleSettings,
 * fetched via GetBattleSettingsUnk01FromBattleSettings. */
Next GetBattleSettingsUnk01(void) {
    u32 battleSettings = LD32(LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_BattleSettings);

    R0 = battleSettings;
    GUEST_CALL(GetBattleSettingsUnk01FromBattleSettings);
    /* result already in R0 */
    RETURN_TO_CALLER();
}

/* ── GetBattleSettingsUnk01FromBattleSettings ── */
/* GetBattleSettingsUnk01FromBattleSettings(r0 = battle settings pointer):
 * return the byte at offset 1 (unknown setting 01) of the battle settings struct. */
Next GetBattleSettingsUnk01FromBattleSettings(void) {
    R0 = LD8(R0 + 1);
    RETURN_TO_CALLER();
}

/* ── TestBattleFlag_0x40 ── */
/* TestBattleFlag_0x40: test the 0x40 bit of the current battle flags.
 * Returns the masked value in r0 (nonzero if the flag is set) and leaves
 * the zero/negative flags reflecting it, so callers can branch directly. */
Next TestBattleFlag_0x40(void) {
    GUEST_CALL(battle_getFlags);   /* r0 = battle_getFlags() */

    R1 = 0x40;
    R0 = R0 & R1;
    SET_FLAGS_NZ(R0);

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

/* ── chipIDIs13f_8029FF4 ── */
/* chipIDIs13f_8029FF4(r0 = chip ID): return 1 in r0 if the ID matches either
 * entry of the two-word chip ID table at dword_802A02C, else 0.
 * All other registers (r1-r7) are preserved. */
Next chipIDIs13f_8029FF4(void) {
    u32 chipID = R0;
    /* r1-r7 must survive the call */
    u32 inR1 = R1, inR2 = R2, inR3 = R3, inR4 = R4, inR5 = R5, inR6 = R6, inR7 = R7;

    u32 table = A_dword_802A02C;
    u32 isMatch = (chipID == LD32(table) || chipID == LD32(table + 4));

    R0 = isMatch;
    R1 = inR1;
    R2 = inR2;
    R3 = inR3;
    R4 = inR4;
    R5 = inR5;
    R6 = inR6;
    R7 = inR7;
    RETURN_TO_CALLER();
}

/* ── chip_getScript_8027D34 ── */
/* chip_getScript_8027D34(r0 = chip id):
 * Resolves the text script table holding a chip's description. Standard chip
 * ids (0x00-0x0FF) use the regular table; ids above 0x0FF (EX/Giga chips) use
 * the second table and are indexed modulo 0x100.
 * Outputs: r0 = pointer stored at the head of the selected table,
 *          r1 = the chip's index within that table. */
Next chip_getScript_8027D34(void) {
    /* Literal-pool entries pointing at the two description script tables. */
    u32 tablePtr = A_off_8027D50;

    u32 chip = R0 & 0x1FF;
    if (chip > 0xFF) {
        tablePtr += sizeof(u32); /* second table: EX/Giga chip descriptions */
        chip &= 0xFF;
    }

    R0 = LD32(tablePtr);
    R1 = chip;
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

/* ── nullsub_14 ── */
/* nullsub_14: 080282AC THUMB — empty stub; does nothing. */
Next nullsub_14(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_7 ── */
/* nullsub_7: a no-op stub (called for features not present in this build). */
Next nullsub_7(void) {
    RETURN_TO_CALLER();
}

/* ── sub_80269D0 ── */
/* sub_80269D0: ask battle_GetObjectByteAt14_800A9E2 for a status value and reduce it to a boolean:
 * r0 = 1 if any of bits 0/2 (mask 5) is set, otherwise r0 = 0.
 * The call's carry/overflow flags pass through unchanged to the caller. */
Next sub_80269D0(void) {
    R0 = 0;
    GUEST_CALL(battle_GetObjectByteAt14_800A9E2);

    R1 = 5;
    R0 = (R0 & R1) != 0 ? 1 : 0;

    RETURN_TO_CALLER();
}

/* ── battle_loadObjTableEntryAndSetState4_80272B8 ── */
/* battle_loadObjTableEntryAndSetState4_80272B8: load the AI/behavior table entry selected by the object's
 * state byte (+0xC) into the object (+0x40) and switch its state to 4.
 * Inputs: R5 = object pointer. Outputs: R0 = 4, R1 = table base, nz/c. */
Next battle_loadObjTableEntryAndSetState4_80272B8(void) {
    u32 tableBase = A_off_8027BEC;

    u32 index = LD8(R5 + 0xC);
    /* carry flag comes from the bit shifted out of the LSL by 2
     * (a byte value shifted left twice can never set it, but keep it exact) */
    u32 carryOut = (index >> 30) & 1;

    ST32(R5 + 0x40, LD32(tableBase + (index << 2)));

    R1 = tableBase;
    R0 = 4;
    cpu.c = carryOut;
    SET_FLAGS_NZ(R0); /* flags as left by "mov r0, #4" */

    ST16(R5 + 2, 4);
    RETURN_TO_CALLER();
}

/* ── sub_8027394 ── */
/* sub_8027394(r5 = object/state pointer):
 * Reset the entity at r5 to state 4 and clear its halfword field.
 * Leaves r0 = 0 with flags set accordingly. */
Next sub_8027394(void) {
    ST8(R5 + 1, 4);
    R0 = 0;
    ST16(R5 + 2, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_object_SelectHandlerByMode_80273CC ── */
/* battle_object_SelectHandlerByMode_80273CC (R5 = battle object):
 * The object re-selects its current handler: read the mode byte at +0xC,
 * look up the routine pointer indexed by it in the table at off_8027C84,
 * store that pointer into the object at +0x40, and set the halfword state
 * field at +2 to 4.
 *
 * On return: R0 = 4, R1 = table address, N/Z from "MOV r0,#4" (both clear)
 * and C from the "LSL r0,r0,#2" (bit 30 of the mode byte). */
Next battle_object_SelectHandlerByMode_80273CC(void) {
    u32 mode = LD8(R5 + 0xC);

    R1 = A_off_8027C84;

    /* lsl r0, r0, #2 — index = mode * 4, and C is the bit shifted out
     * (bit 30 of the byte).  Emulate that carry exactly with a doubling
     * add: x + x carries out bit 31 of x, so doubling (mode << 1) carries
     * out bit 30 of mode, just like the shift.  N/Z here are overwritten
     * by the MOV below, same as in the original. */
    SET_FLAGS_ADD(mode << 1, mode << 1);
    u32 index = mode << 2;

    R0 = LD32(R1 + index);
    ST32(R5 + 0x40, R0);

    R0 = 4;
    ST16(R5 + 2, R0);

    SET_FLAGS_NZ(R0); /* mov r0, #4 updates N and Z only */
    RETURN_TO_CALLER();
}

/* ── sub_80273E4 ── */
/* sub_80273E4(r5 = pointer to a byte):
 * store 8 into the byte at r5; r0 = 8. */
Next sub_80273E4(void) {
    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(R5, R0);
    RETURN_TO_CALLER();
}

/* ── chip_SelectNameTextTable_8027D10 ── */
/* chip_SelectNameTextTable_8027D10(r0 = chip id): pick the chip-name text table for a chip id.
 * The id is truncated to 9 bits; ids above 0xFF select the second name
 * table (TextScriptChipNames1) and wrap around to 8 bits.
 * Outputs: r0 = pointer to the name table, r1 = index into it. */
Next chip_SelectNameTextTable_8027D10(void) {
    u32 chipId = (R0 << 23) >> 23;      /* 9-bit chip id */

    u32 tablePtr = A_off_8027D2C;       /* -> TextScriptChipNames0 */
    if ((s32)chipId > 0xFF) {
        tablePtr += 4;                  /* -> TextScriptChipNames1 */
        chipId = (chipId << 24) >> 24;  /* wrap into the second table */
    }

    R0 = LD32(tablePtr);
    R1 = chipId;
    RETURN_TO_CALLER();
}

/* ── battle_countFolderChips_8027D58 ── */
/* battle_countFolderChips_8027D58: scan the equipped battle folder's chip ID list (up to 30
 * entries) for its 0xFFFF terminator.
 * Returns: r0 = number of chips stored before the terminator (30 if the
 *          folder is full and has none),
 *          r2 = address of the terminator entry (or one past the last
 *          entry when the scan ran off the end),
 *          r3 = number of entries left unscanned at that point.
 * The original uses r4 as scratch for the loaded ID but restores it on
 * return, so it is simply left untouched here. */
Next battle_countFolderChips_8027D58(void) {
    u32 cursor = A_eBattleFolder;
    u32 remaining = 0x1E;

    while (remaining != 0) {
        if (LD16(cursor) == 0xFFFF)
            break;
        cursor += 2;
        remaining -= 1;
    }

    R0 = 0x1E - remaining;
    R2 = cursor;
    R3 = remaining;
    RETURN_TO_CALLER();
}

/* ── battle_BuildFolderEntryRecords_8027EE8 ── */
/* battle_BuildFolderEntryRecords_8027EE8(r4 = destination records, r5 = source header struct, r6 = slot limit):
 * Fill battle folder entries into 12-byte records. The record count is the
 * byte at [r5,#5], clamped to the limit in r6; each record gets a zero byte
 * at +0 and a pointer to its entry inside eBattleFolder (2 bytes per entry)
 * at +8. r4 and r6 are preserved by the original (push/pop), so the record
 * walk happens in a local pointer. r2 and r3 both end at 0 after filling;
 * when nothing is written, r3 holds the count byte that was read. */
Next battle_BuildFolderEntryRecords_8027EE8(void) {
    u32 count = LD8(R5 + 5);

    if (count == 0 || R6 == 0) {
        R3 = count;             /* r3 keeps the ldrb result on this path */
        RETURN_TO_CALLER();
    }

    u32 slots = ((s32)count < (s32)R6) ? count : R6;
    u32 dst = R4;               /* local walk pointer; r4 itself is restored */
    u32 entry = A_eBattleFolder;/* &eBattleFolder[0], 2 bytes per entry */

    /* The byte at +0 is always 0, not the record index. */
    for (u32 i = slots; i != 0; i--) {
        ST8(dst, 0);
        ST32(dst + 8, entry);
        dst += 0xC;
        entry += 2;
    }

    R2 = 0;
    R3 = 0;
    RETURN_TO_CALLER();
}

/* ── table_findFirstOccupiedEntry_8028050 ── */
/* table_findFirstOccupiedEntry_8028050(r4 = table cursor, r5 = record):
 * Scans a table of 12-byte entries starting at r4. A leading byte of 0x0A or
 * 0x0B marks an empty entry, so skip those; any other byte marks an occupied
 * entry, which stops the scan. Stores the index of the first occupied entry
 * (or 0x0A if all 12 entries were empty) to r5[7]. r4 and r6 are callee-saved
 * and end unchanged; the cursor/count live in locals (the original pushes and
 * pops r4/r6 around the scan). */
Next table_findFirstOccupiedEntry_8028050(void) {
    u32 entry = R4;
    u32 slot = 0;

    while (LD8(entry) == 0xA || LD8(entry) == 0xB) {
        entry += 0xC;
        if (++slot == 0xC) {
            /* No occupied entry in the whole table: report 0x0A. */
            slot = 0xA;
            break;
        }
    }

    ST8(R5 + 7, slot);
    RETURN_TO_CALLER();
}

/* ── gfx_flushDataListAndContinue_802811C ── */
/* gfx_flushDataListAndContinue_802811C: flush the dataList's pending GFX transfers, then continue with
 * sub_802812C seeded by the byte at [R5+0x10]. R4-R7 are passed through. */
Next gfx_flushDataListAndContinue_802811C(void) {
    /* dataList (table of GFX data references) */
    R0 = A_off_802A744;
    GUEST_CALL(QueueGFXTransfersInList);

    R0 = LD8(R5 + 0x10);
    GUEST_CALL(sub_802812C);

    RETURN_TO_CALLER();
}

/* ── getIndexed80hRecordPtr_8028164 ── */
/* getIndexed80hRecordPtr_8028164(r0 = index):
 * Look up index in byte_802819C, then compute the address of that entry's
 * 0x80-byte record in the dat38_86 table (table base + entry * 0x80).
 * Returns the address in r0. */
Next getIndexed80hRecordPtr_8028164(void) {
    u8 entry = LD8(A_byte_802819C + R0);
    R0 = A_dat38_86 + entry * 0x80;
    RETURN_TO_CALLER();
}

/* ── gfx_QueueNextChunkedGfxTransfer_80281C4 ── */
/* gfx_QueueNextChunkedGfxTransfer_80281C4(r0 = source, r1 = destination, r3 = context for battle_getChipElementData_80281A8):
 * Let battle_getChipElementData_80281A8 set up the next graphics chunk (it adjusts r0 = source and
 * r1 = destination), queue it as an 0x80-sized eight-word-aligned GFX
 * transfer, then advance the destination pointer past the chunk just queued.
 * Returns r1 = destination + 0x80; r2 and r3 are preserved (the original
 * saves them around the calls and restores them on the way out). */
Next gfx_QueueNextChunkedGfxTransfer_80281C4(void) {
    u32 saved_r2 = R2;
    u32 saved_r3 = R3;

    /* Set up source/destination for this chunk. */
    GUEST_CALL(battle_getChipElementData_80281A8);

    /* Queue the chunk transfer: QueueEightWordAlignedGFXTransfer(src, dest, 0x80). */
    R2 = 0x80;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Destination pointer moves past the chunk we just queued. */
    R1 += 0x80;

    /* Restore the registers the original pops back before returning. */
    R2 = saved_r2;
    R3 = saved_r3;
    RETURN_TO_CALLER();
}

/* ── sprite_CopyObjTilesToVramSlot_80281D4 ── */
/* sprite_CopyObjTilesToVramSlot_80281D4(r1 = slot index, r0/r2..r7 = passthrough to gfx_QueueNextChunkedGfxTransfer_80281C4):
 * Points at one 0x80-byte chunk of OBJ VRAM tile memory, 0x0600A4A0 + 0x80 * r1,
 * and lets gfx_QueueNextChunkedGfxTransfer_80281C4 perform the (de)compression/copy for that chunk.
 * r1 and r2 are saved across the call and restored for the caller. */
Next sprite_CopyObjTilesToVramSlot_80281D4(void) {
    u32 saved_r1 = R1;
    u32 saved_r2 = R2;

    u32 vram_offset = 0x80 * saved_r1;
    u32 vram_dest = 0x0600A4A0 + vram_offset;

    R1 = vram_dest;
    R2 = vram_offset;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R1 = saved_r1;
    R2 = saved_r2;
    RETURN_TO_CALLER();
}

/* ── gfx_QueueNext64ByteChunk_8028204 ── */
/* gfx_QueueNext64ByteChunk_8028204: queue one 0x40-byte graphics DMA chunk.
 *
 * Entry: r1 = destination address for this chunk.  The helper data_getRecordPtrById_8028214
 * fills in r0 with the matching source address, the transfer is queued with
 * size 0x40, and r1 is advanced past the chunk so the caller can immediately
 * queue the next one.  r2/r3 are scratch for the queue call and are restored
 * before returning.  Flags come from the `adds r1, #0x40` pointer bump. */
Next gfx_QueueNext64ByteChunk_8028204(void) {
    u32 saved_r2 = R2;
    u32 saved_r3 = R3;

    /* Resolve the source address (leaves r1-r3 untouched). */
    GUEST_CALL(data_getRecordPtrById_8028214);

    /* QueueEightWordAlignedGFXTransfer(src = r0, dest = r1, size = 0x40). */
    R2 = 0x40;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Restore the caller's r2/r3 and advance the destination pointer. */
    R2 = saved_r2;
    R3 = saved_r3;
    u32 prev_r1 = R1;
    R1 = prev_r1 + 0x40;
    SET_FLAGS_ADD(prev_r1, 0x40);

    RETURN_TO_CALLER();
}

/* ── data_getRecordPtrById_8028214 ── */
/* data_getRecordPtrById_8028214(r0 = record id):
 * Returns a pointer to a 64-byte record for the given id.
 * Ids 0x1B and 0x1C are special-cased and return byte_802A700 instead.
 * Any other id above the table is clamped to the last entry (0x1B).
 * Result in r0; r2 is preserved. */
Next data_getRecordPtrById_8028214(void) {
    u32 id = R0;

    if (id == 0x1B || id == 0x1C) {
        R0 = A_byte_802A700;
    } else {
        if ((s32)id > 0x1C)
            id = 0x1B;
        R0 = A_dword_86E591C + (id << 6);
    }
    RETURN_TO_CALLER();
}

/* ── battle_endActionSetup_8028310 ── */
/* battle_endActionSetup_8028310: apply the two standard "end of battle action" setup calls.
 * First resets something with the value 0x1FF, then triggers action 0x1D.
 * All argument registers (r1-r7) are passed through untouched for the callees. */
Next battle_endActionSetup_8028310(void) {
    R0 = 0x1FF;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R0 = 0x1D;
    GUEST_CALL(gfx_QueueNext64ByteChunk_8028204);

    RETURN_TO_CALLER();
}

/* ── matchHalfwordPatternEntry_802961A ── */
/* matchHalfwordPatternEntry_802961A: compare a run of halfwords against a reference list.
 * r2 = number of halfwords to check, r3 = pointer to a reference record
 * (result halfword at +2, expected halfword list at +4), r6 = source
 * stride, r7 = source base pointer. The halfwords at r7+r6, r7+r6+2, ...
 * are compared against the list at r3+4, r3+6, ... for r2 entries.
 * If all of them match, r0 = halfword at r3+2 and r1 = r6; otherwise
 * r0 = 0 and r1 = the expected halfword that did not match.
 * Flags: nz from the final result move, c/v from the last comparison;
 * r2, r4 and r7 are preserved. */
Next matchHalfwordPatternEntry_802961A(void) {
    u32 remaining = R2;
    u32 src = R7 + R6; /* halfwords being tested */
    u32 ref = R3 + 4;  /* expected halfword list */

    u32 actual, expected;
    do {
        actual = LD16(src);
        expected = LD16(ref);
        /* the comparison is the last instruction that sets C and V */
        SET_FLAGS_CMP(actual, expected);
        if (actual != expected) {
            /* mismatch: r0 = 0, r1 = the expected halfword that failed */
            R0 = 0;
            R1 = expected;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }
        src += 2;
        ref += 2;
    } while (--remaining != 0);

    /* all halfwords matched (last comparison was equal, so C=1, V=0):
     * return the record's result halfword */
    R0 = LD16(R3 + 2);
    R1 = R6;
    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── sub_8029D80 ── */
/* sub_8029D80 (asm03_0.s:7860): fixed-argument wrapper around call_sub_3005EBA.
 * It loads the constant argument set (0xF, 4, 3, 0, 7, 2) into r0-r5, makes the
 * call, and returns. The callee's results in r1-r3 pass through to our caller,
 * while r4/r5 — which the callee is free to clobber — are restored to the
 * values sub_8029D80 itself received (the original does this via push/pop). */
Next sub_8029D80(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    R0 = 0xF;
    R1 = 4;
    R2 = 3;
    R3 = 0;
    R4 = 7;
    R5 = 2;

    GUEST_CALL(call_sub_3005EBA);

    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── sub_802A00C ── */
/* sub_802A00C(r0 = id):
 * Returns r0 = 1 if the low 9 bits of r0 match either of the two words stored
 * at dword_802A02C / dword_802A030 (both 0x13F in this build), else r0 = 0.
 * All other registers are preserved.
 * Flag note: the final "mov r0, r3" is the flag-setting Thumb encoding
 * (LSLS r0, r3, #0): N/Z come from the result, and C/V end up 0. */
Next sub_802A00C(void) {
    u32 id = (R0 << 23) >> 23; /* keep only the low 9 bits */
    u32 table = A_dword_802A02C;

    u32 match = (id == LD32(table)) || (id == LD32(table + 4));

    R0 = match;
    SET_FLAGS_ADD(R0, 0); /* N/Z from result, C = 0, V = 0 */
    RETURN_TO_CALLER();
}

/* ── battle_ClearPendingEventFlag_802A0EC ── */
/* battle_ClearPendingEventFlag_802A0EC: clear the byte flag at byte_20352CC (the battle-related flag in
 * the eStruct2035280 work area), signalling that the pending event is done.
 * Returns r1 = 0, r2 = 0x4C, and flags set from the MOV r2, #0x4C. */
Next battle_ClearPendingEventFlag_802A0EC(void) {
    R0 = A_eStruct2035280;
    R1 = 0;
    R2 = 0x4C; /* byte_20352CC - 0x2035280 */
    SET_FLAGS_NZ(R2);
    ST8(R0 + R2, R1);
    RETURN_TO_CALLER();
}

/* ── battle_setUnk20349A5Flag_802A210 ── */
/* battle_setUnk20349A5Flag_802A210(r5 = battle state):
 * If state->Unk_15 is nonzero, store 0xFF to the byte at dword_20349A0+5
 * (byte_20349A5) and leave that address in r3. The flags left behind are
 * those of the CMP of Unk_15 against 0 (the function's only flag-setting op). */
Next battle_setUnk20349A5Flag_802A210(void) {
    u32 unk15 = LD8(R5 + oBattleState_Unk_15);

    SET_FLAGS_CMP(unk15, 0);
    if (unk15 != 0) {
        R3 = A_dword_20349A0;
        ST8(R3 + 5, 0xFF);
    }
    RETURN_TO_CALLER();
}

/* ── sub_802BDB4 ── */
/* sub_802BDB4: tail work after sub_802C97E -- store the value 4 into the
 * word pointed to by r5 (likely a state/step counter being advanced). */
Next sub_802BDB4(void) {
    GUEST_CALL(sub_802C97E);

    R0 = 4;
    ST32(R5, R0);
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── object_countdownTick_802BEFC ── */
/* object_countdownTick_802BEFC(r5 = struct with a countdown byte at +0xB and a state byte at +3):
 * decrement the countdown at [r5+0xB]; when it reaches zero, set [r5+3] to 8
 * (advance to state 8). Leaves r0 = the decremented counter, or 8 if it expired,
 * with flags from the corresponding last flag-setting instruction. */
Next object_countdownTick_802BEFC(void) {
    u32 count = LD8(R5 + 0xB);
    u32 next  = count - 1;

    ST8(R5 + 0xB, next);
    SET_FLAGS_CMP(count, 1); /* flags of SUBS r0, #1 */

    if (next != 0) {
        R0 = next;           /* still counting down */
        RETURN_TO_CALLER();
    }

    R0 = 8;                  /* timer expired */
    SET_FLAGS_NZ(8);         /* flags of MOVS r0, #8 */
    ST8(R5 + 3, 8);
    RETURN_TO_CALLER();
}

/* ── sub_802C348 ── */
/* sub_802C348(r5 = struct pointer): clear the byte at offset 5 of the struct
 * (e.g. reset a state/counter field to zero). Always returns r0 = 0 with
 * zero flag set (the final "mov r0, #0" sets the flags callers branch on). */
Next sub_802C348(void) {
    R0 = 0;
    ST8(R5 + 5, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_802C588 ── */
/* sub_802C588(r0 = destination address):
 * Fill 0x2A halfwords at r0 with the consecutive values 0xC1E8..0xC211.
 * Leaves r1 = 0xC212 (one past the last value written) and r2 = 0
 * (loop counter after exhaustion); r0 points past the filled range. */
Next sub_802C588(void) {
    u32 dst   = R0;
    u32 value = 0xC1E8;

    for (u32 count = 0x2A; count != 0; count--) {
        ST16(dst, (u16)value);
        value++;
        dst += 2;
    }

    R1 = value; /* 0xC212 */
    R2 = 0;
    R0 = dst;
    RETURN_TO_CALLER();
}

/* ── chipFolder_ClearTilemapArea_802C59C ── */
/* chipFolder_ClearTilemapArea_802C59C(r0 = destination):
 * fill 0x2A halfwords with the value 0x90C6 (used to blank the chip folder
 * screen's VRAM tilemap area). */
Next chipFolder_ClearTilemapArea_802C59C(void) {
    u32 dst = R0;
    const u16 fill = 0x90C6;

    for (u32 i = 0; i < 0x2A; i++, dst += 2)
        ST16(dst, fill);

    RETURN_TO_CALLER();
}

/* ── queue_popHalfword_802C8D4 ── */
/* queue_popHalfword_802C8D4(r5 = queue state): pop the next halfword from an in-struct queue.
 * Layout at r5: +0xC = read index, +0xD = entry count, +0x14 = halfword entries.
 * Returns the popped value in r0 (0 when the queue is exhausted), r1 = read index
 * (incremented if an entry was popped), r2 = entry offset / count.
 * Flags: c/v from the CMP when the queue is empty, from the index increment otherwise. */
Next queue_popHalfword_802C8D4(void) {
    u32 read = LD8(R5 + 0xC);
    u32 count = LD8(R5 + 0xD);

    if ((s32)read >= (s32)count) {
        /* Queue exhausted: return 0 and leave the index untouched. */
        R0 = 0;
        R1 = read;
        R2 = count;
        SET_FLAGS_CMP(read, count);
    } else {
        /* Fetch the entry at read index and advance the index. */
        u32 offset = 0x14 + read + read;
        R0 = LD16(R5 + offset);
        R1 = read + 1;
        R2 = offset;
        ST8(R5 + 0xC, R1);
        SET_FLAGS_ADD(R1, 1);
    }
    RETURN_TO_CALLER();
}

/* ── battle_getStateTableEntryPtr_802C8EA ── */
/* battle_getStateTableEntryPtr_802C8EA: select the 8-byte entry of the dword_2036820 table indexed by
 * the battle state's current Unk_0d slot.
 *   r10 = toolkit
 *   out r0 = &dword_2036820[Unk_0d], r2 = Unk_0d * 8,
 *   flags as left by the final address add. */
Next battle_getStateTableEntryPtr_802C8EA(void) {
    u32 battleState;

    R1 = R10;
    battleState = LD32(R1 + oToolkit_BattleStatePtr);
    R2 = (u32)LD8(battleState + oBattleState_Unk_0d) * 8;
    R0 = A_dword_2036820 + R2;
    SET_FLAGS_ADD(A_dword_2036820, R2);
    RETURN_TO_CALLER();
}

/* ── battle_ClearAllianceTargetSlotIfSelf_802CDD0 ── */
/* battle_ClearAllianceTargetSlotIfSelf_802CDD0(r5 = battle object):
 * Clear this object's entry in the current-target slot of its alliance's
 * table (dword_203CFB0: two 0xC-byte alliance records, target pointer at +8).
 * Objects in the second alliance use the second record. Leaves the previous
 * slot value in r1 (0 if it was cleared, i.e. if it pointed at this object). */
Next battle_ClearAllianceTargetSlotIfSelf_802CDD0(void) {
    u32 table = A_dword_203CFB0;

    if (LD8(R5 + oBattleObject_Alliance) != 0)
        table += 0xC;

    R1 = LD32(table + 8);
    if (R1 == R5) {
        R1 = 0;
        ST32(table + 8, R1);
    }

    RETURN_TO_CALLER();
}

/* ── wrapCountersAt203CFB0ToU16_802CDFE ── */
/* wrapCountersAt203CFB0ToU16_802CDFE: clear the upper halves of the two 32-bit counters at
 * dword_203CFB0 and dword_203CFB0+0xC, keeping only the low 16 bits. */
Next wrapCountersAt203CFB0ToU16_802CDFE(void) {
    u32 counter = A_dword_203CFB0;

    ST32(counter, LD32(counter) << 16);
    ST32(counter + 0xC, LD32(counter + 0xC) << 16);

    RETURN_TO_CALLER();
}

/* ── sub_802CE10 ── */
/* sub_802CE10(r0 = value, r5 = battle object):
 * Updates one of the two battle side-counters for the object's alliance.
 * If the object is the one currently tracked at (+8) of the counter block,
 * its stored increment (+2) is added to r0; otherwise r0 acts as a new
 * maximum and overwrites the counter (+0) when it is larger.
 * Returns: r0 = updated value, r2 = the tracked object pointer. */
Next sub_802CE10(void) {
    u32 amount = R0;
    u32 object = R5;

    /* Player-side block, or the +0xC partner-side block for allied objects. */
    u32 counters = A_dword_203CFB0;
    if (LD8(object + oBattleObject_Alliance) != 0)
        counters += 0xC;

    R2 = LD32(counters + 8);
    if (object == R2) {
        /* This object owns the counter: accumulate its increment into r0. */
        R0 = amount + LD16(counters + 2);
    } else {
        /* Raise the counter's recorded maximum if r0 exceeds it. */
        if (amount > LD16(counters))
            ST16(counters, amount);
        R0 = amount;
    }
    RETURN_TO_CALLER();
}

/* ── sub_802CE60 ── */
/* sub_802CE60(r0 = index):
 * Fetch one 16-byte entry from the table at unk_2036720. The index is
 * scrambled by XORing it with the current battle state's shuffle key
 * (oBattleState_Unk_0d) before scaling it to the entry size, so callers
 * get a shuffled-but-deterministic entry for the current battle.
 * Returns r0 = entry halfword at +0, r1 = entry word at +4,
 *         r2 = entry halfword at +2. */
Next sub_802CE60(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 shuffleKey = LD8(battleState + oBattleState_Unk_0d);

    u32 entry = A_unk_2036720 + ((R0 ^ shuffleKey) * 0x10);

    R1 = LD32(entry + 4);
    R2 = LD16(entry + 2);
    R0 = LD16(entry);

    RETURN_TO_CALLER();
}

/* ── getTable2036720EntryFields_802CE78 ── */
/* getTable2036720EntryFields_802CE78(r0 = entry index):
 * Fetch a 16-byte record from the table at unk_2036720 and spread its fields
 * across registers: r0 = halfword at +0, r1 = word at +4, r2 = halfword at +2,
 * r3 = word at +8. The address computation sets c/v as the original ADDS does. */
Next getTable2036720EntryFields_802CE78(void) {
    u32 table = A_unk_2036720;
    u32 offset = R0 * 0x10;             /* each record is 16 bytes */

    SET_FLAGS_ADD(offset, table);       /* add r0, r0, r1 (sets c/v) */
    u32 entry = offset + table;

    R3 = LD32(entry + 8);
    R1 = LD32(entry + 4);
    R2 = LD16(entry + 2);
    R0 = LD16(entry);

    RETURN_TO_CALLER();
}

/* ── initStateCounters100_802D08C ── */
/* initStateCounters100_802D08C(r0 = selector flag passed through to sub_802D064):
 * Initialise a state struct: clear the byte at +0x10 and set the two
 * bytes at +0xD and +0xF to 100. Returns the struct pointer in r0 and
 * r1 = 100 (flags reflect r1). */
Next initStateCounters100_802D08C(void) {
    /* sub_802D064 resolves the struct to initialise and returns it in r0. */
    GUEST_CALL(sub_802D064);
    u32 state = R0;

    ST8(state + 0x10, 0);
    ST8(state + 0xD, 100);
    ST8(state + 0xF, 100);

    R0 = state;
    R1 = 100;
    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── sub_802D150 ── */
/* sub_802D150: clear the 32-bit counter/flag at dword_2006740. */
Next sub_802D150(void) {
    ST32(A_dword_2006740, 0);
    RETURN_TO_CALLER();
}

/* ── memBlock_ClearWorkArea_802D638 ── */
/* memBlock_ClearWorkArea_802D638: clear the 0x20-byte work buffer at byte_2000070 (memBlock).
 * Arguments in r2/r3 and those registers are passed through unchanged. */
Next memBlock_ClearWorkArea_802D638(void) {
    R0 = A_byte_2000070; /* memBlock */
    R1 = 0x20;           /* size in bytes */
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── sub_802D644 ── */
/* sub_802D644(r0 = index into byte_2000070):
 * Increment the counter byte at byte_2000070[index], clamped at 0xFF
 * (0xFF stays 0xFF instead of wrapping). r2 = incremented (possibly
 * clamped) value; flags reflect the comparison against 0xFF. */
Next sub_802D644(void) {
    u32 value = (u32)LD8(A_byte_2000070 + R0) + 1;

    SET_FLAGS_CMP(value, 0xFF);
    if ((s32)value <= 0xFF)
        ST8(A_byte_2000070 + R0, value);

    R2 = value;
    RETURN_TO_CALLER();
}

/* ── sub_802DD1E ── */
/* sub_802DD1E: stub that always yields 0.
 * The original clears r7, copies it into r0 (an "adds r0, r7, #0", which is why
 * the flags come out as Z set / C, V clear) and immediately returns; the pushes
 * of r5/r7 are undone by the pop, so both are preserved untouched.
 *   inputs:  r0 (ignored)          outputs: r0 = 0, flags nz/c/v
 *   preserved: r5 r7 */
Next sub_802DD1E(void) {
    R0 = 0;
    SET_FLAGS_ADD(0, 0);   /* "adds r0, r7, #0" with r7 = 0: N=0, Z=1, C=0, V=0 */
    RETURN_TO_CALLER();
}

/* ── getEntryPtr_2036120_1D0_802E070 ── */
/* getEntryPtr_2036120_1D0_802E070(r0 = entry index): return the address of entry `r0` in the
 * array at unk_2036120, whose elements are 0x1D0 bytes each
 * (0xE8 * 2). Leaves r0 = base + index*0x1D0 and r1 = base; flags are
 * those of the final address addition. */
Next getEntryPtr_2036120_1D0_802E070(void) {
    u32 offset = R0 * (0xE8 + 0xE8);

    R1 = A_unk_2036120;
    R0 = offset + A_unk_2036120;
    SET_FLAGS_ADD(offset, A_unk_2036120);
    RETURN_TO_CALLER();
}

/* ── battle_DecrementAllianceTimer_802E1D8 ── */
/* battle_DecrementAllianceTimer_802E1D8: tick down the timer field (+0x30) of the record that
 * getEntryPtr_2036120_1D0_802E070 returns for the alliance of the battle object in r5.
 * r1 = the timer's value after the update (0 if it was already 0). */
Next battle_DecrementAllianceTimer_802E1D8(void) {
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);

    u32 timer = LD16(R0 + 0x30);
    if (timer != 0) {
        timer--;
        ST16(R0 + 0x30, timer);
    }
    R1 = timer;
    RETURN_TO_CALLER();
}

/* ── loadIndexedPaletteToBuffer_802E240 ── */
/* loadIndexedPaletteToBuffer_802E240(r0 = palette index):
 * Copies one 60-halfword palette (0x3C halfwords) into the buffer at unk_2008170.
 * The source palette is picked from the word-sized table at off_802E260 by index. */
Next loadIndexedPaletteToBuffer_802E240(void) {
    u32 index = R0;

    u32 src = LD32(A_off_802E260 + index * 4);
    u32 dest = A_unk_2008170;

    R0 = src;
    R1 = dest;
    R2 = 0x3C;
    GUEST_CALL(CopyHalfwords); /* (u16 *src, u16 *dest, int halfwordCount) */

    RETURN_TO_CALLER();
}

/* ── toolkit_ClearAllEventFlags_802F0C4 ── */
/* toolkit_ClearAllEventFlags_802F0C4(r10 = toolkit):
 * Clears the toolkit's event-flags array (630 halfword entries).
 * r2 and r3 pass through unchanged. */
Next toolkit_ClearAllEventFlags_802F0C4(void) {
    R0 = LD32(R10 + oToolkit_EventFlagsPtr);
    R1 = 630 * 2;
    GUEST_CALL(ZeroFillByHalfword);
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

/* ── zeroFill_802CE54 ── */
/* zeroFill_802CE54: clear the 0x20-byte work buffer unk_2036720.
 * Inputs: r2/r3 (preserved, forwarded unchanged). Flags on exit are
 * whatever ZeroFillByWord leaves behind (callers branch on c/v). */
Next zeroFill_802CE54(void) {
    R0 = A_unk_2036720; /* memBlock: buffer to clear */
    R1 = 0x20;          /* size in bytes */
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── zeroFill_802E544 ── */
/* zeroFill_802E544: clear the 0x170-byte buffer at unk_20000A0.
 * r2 is untouched: it is an input forwarded to ZeroFillByByte and must be preserved. */
Next zeroFill_802E544(void) {
    R0 = A_unk_20000A0;
    R1 = 0x170; /* 0x71 + 0xFF as computed by the original */
    GUEST_CALL(ZeroFillByByte);
    RETURN_TO_CALLER();
}

/* ── onUpdateInInternet_8033B1E ── */
/* onUpdateInInternet_8033B1E(self: *S2011E30 in r5): per-frame update hook
 * while the player is navigating the Internet. Runs the common Internet
 * update step, flags state 1 for sub_8033F80, then redraws the map name
 * display. All callee flag/register effects are passed through to callers. */
Next onUpdateInInternet_8033B1E(void) {
    GUEST_CALL(sub_8033E0C);

    R0 = 1;
    GUEST_CALL(sub_8033F80);

    GUEST_CALL(RenderMapName_8033EE8);

    RETURN_TO_CALLER();
}

/* ── owPlayer_80350a8 ── */
/* owPlayer_80350a8:
 * Spawn the player's overworld object for a map entry, then point the camera
 * at it. The coordinate mode passed to the camera is 1, and the camera target
 * is the spawned object's Coords field (returned in r5 by the spawn call).
 * Leaves the original caller's r5 intact (the spawn result is consumed
 * internally before r5 is restored). */
Next owPlayer_80350a8(void) {
    u32 savedR5 = R5;

    GUEST_CALL(spawnOWPlayerObjectForEnterMap_809e064);

    R0 = 1;
    R1 = R5 + oOWPlayerObject_Coords;

    GUEST_CALL(camera_writeUnk03_14_80301b2);

    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── clear_e200AD04 ── */
/* clear_e200AD04: zero the 4 bytes at word_200AD04 via ZeroFillByByte.
 * Preserves r2; clobbers r0/r1. */
Next clear_e200AD04(void) {
    R0 = A_word_200AD04;
    R1 = 4;
    GUEST_CALL(ZeroFillByByte);
    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_getJumpOffset00 ── */
/* eStruct200BC30_getJumpOffset00:
 * Returns the jump offset stored at the head of the eStruct200BC30 record.
 * r1 = pointer to the record, r0 = its first byte. */
Next eStruct200BC30_getJumpOffset00(void) {
    R1 = A_eStruct200BC30;
    R0 = LD8(R1);
    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_getJumpOffset00_pushpop ── */
/* eStruct200BC30_getJumpOffset00_pushpop: thin trampoline that tail-calls
 * eStruct200BC30_getJumpOffset00. Results come back in r0/r1 unchanged. */
Next eStruct200BC30_getJumpOffset00_pushpop(void) {
    GUEST_CALL(eStruct200BC30_getJumpOffset00);
    RETURN_TO_CALLER();
}

/* ── init_eStartScreenAnimationControl200B1A0_1 ── */
/* init_eStartScreenAnimationControl200B1A0_1:
 * Initialize the 8-byte e-StartScreen animation control block and mark it
 * active (state 0xB4). Preserves r2/r4; leaves r0 = 0xB4 with flags set. */
Next init_eStartScreenAnimationControl200B1A0_1(void) {
    u32 ctrl = A_eStartScreenAnimationControl200B1A0;

    R0 = ctrl;
    R1 = 8;
    GUEST_CALL(ZeroFillByByte);

    R0 = 0xB4;
    SET_FLAGS_NZ(R0);
    ST8(ctrl + 3, R0);
    RETURN_TO_CALLER();
}

/* ── logoScreen_finish_803D2A6 ── */
/* logoScreen_finish_803D2A6(self: r5 = *const LogoScreenState):
 * Called when the logo screen finishes: switch the LCD back to the normal
 * render mode (0x40) and reinitialize the shared screen-start state.
 * Preserves r2/r3; returns with r0/r1 and flags as the last callee left them. */
Next logoScreen_finish_803D2A6(void) {
    R0 = 0x40;
    GUEST_CALL(SetRenderInfoLCDControl);
    GUEST_CALL(startScreen_init_802F530);
    RETURN_TO_CALLER();
}

/* ── notZero_eByte200AD04 ── */
/* notZero_eByte200AD04:
 * Test the event flag byte at word_200AD04 for zero.
 * Outputs: r0 = the byte, r1 = its address, nz flags set from the byte. */
Next notZero_eByte200AD04(void) {
    R1 = A_word_200AD04;
    R0 = LD8(R1);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── startScreen_DecrementPressStartPauseAnimationTimer ── */
/* startScreen_DecrementPressStartPauseAnimationTimer (0803E964):
 * Tick down the pause timer (byte at eStartScreenAnimationControl200B1A0+3)
 * that holds the "PRESS START" screen on its current animation frame. When the
 * timer reaches zero (i.e. it was 1), byte 0 of the animation control is set
 * to 1 so the animation advances, and the timer value reported to callers is 1.
 * Outputs: r0 = raw decremented timer, r6/r7 pass through unchanged, and the
 * flags are those of the "sub r0,#1" (or of "mov r0,#1" on the restart path). */
Next startScreen_DecrementPressStartPauseAnimationTimer(void) {
    u32 ctrl = A_eStartScreenAnimationControl200B1A0;

    u32 timer = LD8(ctrl + 3);

    /* timer--; the flags of this subtraction tell callers whether it hit zero */
    SET_FLAGS_CMP(timer, 1);
    timer -= 1;
    ST8(ctrl + 3, timer);

    if (timer != 0) {
        R0 = timer;
        RETURN_TO_CALLER();
    }

    /* timer expired: flag the animation control to move on, timer reads as 1 */
    ST8(ctrl, 1);
    SET_FLAGS_NZ(1); /* mov r0,#1 */
    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── sub_803C3E0 ── */
/* sub_803C3E0: test the flag byte at word_2006770+2 and return it.
 * r0 = flag byte; Z set when it is zero, N set when bit 7 is set. */
Next sub_803C3E0(void) {
    u32 flag = LD8(A_word_2006770 + 2);
    R0 = flag;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── keyItems_RemoveFromInventoryAndGive_803CDF8 ── */
/* keyItems_RemoveFromInventoryAndGive_803CDF8(r0 = key item slot index, r1 = passed through to GiveItem):
 * mark the given key item slot as empty (not owned) in the player's key
 * item inventory, then hand the item to the player via GiveItem.
 * Leaves r0..r3 as GiveItem left them. */
Next keyItems_RemoveFromInventoryAndGive_803CDF8(void) {
    u32 keyItems = LD32(R10 + oToolkit_KeyItemsPtr);

    ST8(keyItems + R0, 0);

    R2 = keyItems;
    R3 = 0;
    GUEST_CALL(GiveItem);

    RETURN_TO_CALLER();
}

/* ── sub_803DE5C ── */
/* sub_803DE5C: set unk_200A284 (a boolean flag) to 1. */
Next sub_803DE5C(void) {
    ST32(A_unk_200A284, 1);
    RETURN_TO_CALLER();
}

/* ── sub_803DEAC ── */
/* sub_803DEAC: tail-call wrapper that simply invokes sub_8144240.
 * r4-r7 are preserved (sub_8144240 keeps them intact). */
Next sub_803DEAC(void) {
    GUEST_CALL(sub_8144240);
    RETURN_TO_CALLER();
}

/* ── startScreen_IsTitleAnimationFinished_803E928 ── */
/* startScreen_IsTitleAnimationFinished_803E928:
 * Check the start screen animation's current state. Loads the animation
 * control byte and compares it against 2 (animation finished).
 * Outputs: R0 = state byte, flags set from `R0 - 2` (Z set when state == 2).
 */
Next startScreen_IsTitleAnimationFinished_803E928(void) {
    R0 = LD8(A_eStartScreenAnimationControl200B1A0);
    SET_FLAGS_CMP(R0, 2);
    RETURN_TO_CALLER();
}

/* ── runPendingWorkIfFlagged_803EA1C ── */
/* runPendingWorkIfFlagged_803EA1C: service the pending work flagged by byte_200AD04.
 * If notZero_eByte200AD04 reports the flag byte is non-zero, run
 * sub_803F560 to process it; otherwise do nothing. Flags and R0-R3
 * are whatever the (last) callee leaves behind. */
Next runPendingWorkIfFlagged_803EA1C(void) {
    GUEST_CALL(notZero_eByte200AD04);
    if (R0 != 0)
        GUEST_CALL(sub_803F560);
    RETURN_TO_CALLER();
}

/* ── net_isLinkConnected_803EA2C ── */
/* net_isLinkConnected_803EA2C: check the link state flag.
 * r0 = 1 if the byte at eLinkState is 0xC (link connection fully established),
 * otherwise r0 = 0. Flags (nzcv) left as the CMP leaves them. */
Next net_isLinkConnected_803EA2C(void) {
    R0 = 0;
    R1 = LD8(A_eCameraEnd); /* literal pool points at eLinkState */
    SET_FLAGS_CMP(R1, 0xC);
    if (R1 == 0xC)
        R0 = 1;
    RETURN_TO_CALLER();
}

/* ── clearStruct200BC30_803EBAC ── */
/* clearStruct200BC30_803EBAC: clear the 0x20-byte work struct at 0x200BC30 (eStruct200BC30).
 * Leaves the struct zeroed word-by-word via ZeroFillByWord; r2/r3 pass
 * through untouched, r1 ends as ZeroFillByWord leaves it. */
Next clearStruct200BC30_803EBAC(void) {
    R0 = A_eStruct200BC30;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── sub_803EF60 ── */
/* sub_803EF60: reset some state, then zero-fill the buffer sub_803EF7C
 * returned a pointer to (0x100 bytes, filled eight words at a time). */
Next sub_803EF60(void) {
    GUEST_CALL(sub_803EF7C);   /* returns buffer address in r0 */
    R1 = 0x100;
    GUEST_CALL(ZeroFillByEightWords); /* ZeroFillByEightWords(r0, 0x100) */
    RETURN_TO_CALLER();
}

/* ── sub_803EF7C ── */
/* sub_803EF7C: return eCamera.unk_84 (the camera's end value). */
Next sub_803EF7C(void) {
    R0 = LD32(A_eCameraEnd + 0x34);
    RETURN_TO_CALLER();
}

/* ── sub_803EF84 ── */
/* sub_803EF84(r0 = value):
 * Store r0 into the link-state camera field at eLinkState+0x34, which the
 * disassembly also refers to as eCamera.unk_84 (the eLinkState symbol sits at
 * the same address as A_eCameraEnd, hence the alias in the original literal). */
Next sub_803EF84(void) {
    ST32(A_eCameraEnd + 0x34, R0);
    RETURN_TO_CALLER();
}

/* ── setByte_200AD04_803F4EC ── */
/* setByte_200AD04_803F4EC(r0 = value): store the low byte at word_200AD04.
 * Also leaves the address of word_200AD04 in r1. */
Next setByte_200AD04_803F4EC(void) {
    u32 *ptr = (u32 *)A_word_200AD04;

    ST8((u32)ptr, R0);
    R1 = (u32)ptr;

    RETURN_TO_CALLER();
}

/* ── sub_803F500 ── */
/* sub_803F500:
 * Store 0xB4 into word_200AD06 (likely resetting some counter/timer). */
Next sub_803F500(void) {
    ST16(A_word_200AD04 + 2, 0xB4);
    RETURN_TO_CALLER();
}

/* ── sub_803F798 ── */
/* sub_803F798: shared entry stub for a multi-entry routine.
 * It stores r1-r7 and lr on the stack exactly like `push {r1-r7,lr}`
 * (low word = r1, high word = lr), zeroes r4 as the loop counter seed,
 * and then continues into the shared body at loc_803F7A2, whose
 * epilogue pops these values back (pc = saved lr returns to the caller). */
Next sub_803F798(void) {
    u32 frame = SP - 32;
    SP = frame;

    ST32(frame + 0x00, R1);
    ST32(frame + 0x04, R2);
    ST32(frame + 0x08, R3);
    ST32(frame + 0x0C, R4);
    ST32(frame + 0x10, R5);
    ST32(frame + 0x14, R6);
    ST32(frame + 0x18, R7);
    ST32(frame + 0x1C, LR);

    R4 = 0;

    /* tail jump: loc_803F7A2 pops {r1-r7,pc} at its end */
    GUEST_CALL(loc_803F7A2);
    RETURN_TO_CALLER();
}

/* ── s2001c04_ComputeChecksumOfEwramRegion_803F894 ── */
/* s2001c04_ComputeChecksumOfEwramRegion_803F894: compute a checksum over the 16 bytes at EWRAM
 * 0x2000000 + 0x6700..0x670F, add 24, and store it into the
 * S2001c04 work area at offset 0x68. The field is briefly zeroed
 * first, as in the original. All registers except r0 are preserved. */
Next s2001c04_ComputeChecksumOfEwramRegion_803F894(void) {
    u32 work = LD32(R10 + oToolkit_S2001c04_Ptr);

    ST32(work + 0x68, 0);

    u32 sum = 0;
    for (s32 i = 0x670F; i >= 0; i--)
        sum += LD8(A_ewram_2000000 + i);

    sum += 0x18;
    ST32(work + 0x68, sum);

    R0 = sum;
    RETURN_TO_CALLER();
}

/* ── info_copyTimestampToWorkArea_803F8B2 ── */
/* info_copyTimestampToWorkArea_803F8B2:
 * Copy the 20-byte InfoText timestamp into the work area at
 * [toolkit->S2001c04] + 0x6C. R1/R2/R3 are left as CopyBytes leaves them
 * (callers read their post-call values). */
Next info_copyTimestampToWorkArea_803F8B2(void) {
    R0 = A_EXE6_InfoText_timestamp;              /* src: timestamp buffer */
    R1 = LD32(R10 + oToolkit_S2001c04_Ptr) + 0x6C; /* dest: work area + 0x6C */
    R2 = 0x14;                                   /* 20 bytes */
    GUEST_CALL(CopyBytes);
    RETURN_TO_CALLER();
}

/* ── infoText_IsTimestampUnchanged_803F8F4 ── */
/* infoText_IsTimestampUnchanged_803F8F4: check whether the info-text timestamp is unchanged.
 *
 * Compares the 20-byte timestamp stored in the save buffer
 * (toolkit->S2001c04 + 0x6C) against the master copy at
 * EXE6_InfoText_timestamp. Returns r0 = 0 if all 20 bytes match
 * (the on-screen text is still current), r0 = 1 if any byte differs
 * (the text must be refreshed).
 * Flags: nz reflects the returned value. */
Next infoText_IsTimestampUnchanged_803F8F4(void) {
    u32 master = A_EXE6_InfoText_timestamp;                  /* reference timestamp */
    u32 saved  = LD32(R10 + oToolkit_S2001c04_Ptr) + 0x6C;   /* copy in the save buffer */

    s32 i;
    for (i = 0x13; i >= 0; i--) {
        if (LD8(saved + i) != LD8(master + i)) {
            R0 = 1;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }
    }

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── rng_FillSeedBuffer_803FAEE ── */
/* rng_FillSeedBuffer_803FAEE: fill the 0x80-byte buffer at dword_20067C0 with random 32-bit
 * words drawn from the hardware RNG, one GetRNG call per word.
 * r4-r7 are preserved (the original saves/restores them around the loop). */
Next rng_FillSeedBuffer_803FAEE(void) {
    enum { RNG_FILL_SIZE = 0x80 };  /* bytes to fill (literal =0x80) */

    u32 dst = A_dword_20067C0;

    for (u32 off = 0; off < RNG_FILL_SIZE; off += 4) {
        GUEST_CALL(GetRNG);
        ST32(dst + off, R0);
    }

    RETURN_TO_CALLER();
}

/* ── battleFindEntryByFieldByte_8047456 ── */
/* battleFindEntryByFieldByte_8047456(r0 = value to look up, r4 = entry count, r7 = table base):
 * Scan a table of 8-byte entries, comparing r0 against the byte at offset 4
 * of each entry. Sets Z (found) when a match is encountered, clears it when
 * the whole table was searched without a match. r3-r7 are preserved. */
Next battleFindEntryByFieldByte_8047456(void) {
    u32 needle = R0;
    u32 entry = R7;
    u32 notFound = 1;

    for (s32 i = 0; i < (s32)R4; i++) {
        if (LD8(entry + 4) == needle) {
            notFound = 0;
            break;
        }
        entry += 8;
    }

    SET_FLAGS_NZ(notFound);
    RETURN_TO_CALLER();
}

/* ── menu_showDialogOnUnselect_8047A80 ── */
/* menu_showDialogOnUnselect_8047A80(r5 = struct ptr):
 * Puts the owner struct into its "show dialog" state (0x14), makes the
 * chatbox run text script #2 from the text archive stored in the struct,
 * and plays the menu-unselect sound. */
Next menu_showDialogOnUnselect_8047A80(void) {
    /* 0x14 = this struct's dialog/state id */
    ST8(R5, 0x14);

    /* chatbox_runScript(archive, script 2) */
    R0 = LD32(R5 + 0x24);
    R1 = 2;
    GUEST_CALL(chatbox_runScript);

    /* menu-unselect sound effect */
    R0 = SOUND_UNSELECT_68;
    GUEST_CALL(PlaySoundEffect);

    RETURN_TO_CALLER();
}

/* ── zeroFill_e2004348 ── */
/* zeroFill_e2004348:
 * Clear the 0x480-byte struct pointed to by the toolkit's Unk2004348 pointer,
 * filling it with zeroes eight words at a time via ZeroFillByEightWords.
 * All registers other than r0/r1 (and the flags) are preserved. */
Next zeroFill_e2004348(void) {
    u32 structPtr = LD32(R10 + oToolkit_Unk2004348_Ptr);

    R0 = structPtr;
    R1 = 0x480;
    GUEST_CALL(ZeroFillByEightWords);

    RETURN_TO_CALLER();
}

/* ── isEventFlag212Set_80AA764 ── */
/* isEventFlag212Set_80AA764: returns 1 in r0 if event flag EVENT_212 is set, else 0.
 * Flags (N/Z) reflect the returned value, as the original's final
 * `mov r0, r4` does. */
Next isEventFlag212Set_80AA764(void) {
    u32 result = 0;

    /* TestEventFlagFromImmediate(flag): flag id 0x212 passed as
     * group in r0, number in r1. */
    R0 = 2;
    R1 = 0x12;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (R0 != 0)
        result = 1;

    R0 = result;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── GetAllianceDependentPanelParamArgs ── */
/* GetAllianceDependentPanelParamArgs(r3 = panel param table base,
 *                                    r5 = battle object):
 * Reads the two words of the panel param entry selected by the object's
 * alliance (each alliance row is 8 bytes). Returns r2 = first word (param),
 * r3 = second word (args pointer), with flags from "row + 4". */
Next GetAllianceDependentPanelParamArgs(void) {
    u32 alliance = LD8(R5 + oBattleObject_Alliance);
    u32 row = R3 + (alliance << 3);   /* entry is 8 bytes per alliance */

    R2 = LD32(row);
    R3 = LD32(row + 4);
    SET_FLAGS_ADD(row, 4);
    RETURN_TO_CALLER();
}

/* ── nullsub_12 ── */
/* nullsub_12: empty placeholder function; does nothing. */
Next nullsub_12(void) {
    RETURN_TO_CALLER();
}

/* ── nullsub_13 ── */
/* nullsub_13: does nothing; kept as a placeholder for an empty hook point. */
Next nullsub_13(void) {
    RETURN_TO_CALLER();
}

/* ── spawn_t1_0x0_EffectObject ── */
/* spawn_t1_0x0_EffectObject: spawn a type-4 battle object with effect type 0.
 * The remaining arguments (r1-r3) and the r5 context pointer are passed
 * straight through to object_spawnType4. Returns the object the callee
 * leaves in r5 (flags reflect that value), while restoring the caller's r5. */
Next spawn_t1_0x0_EffectObject(void) {
    u32 callerR5 = R5;

    R0 = 0;                            /* effect type 0 */
    GUEST_CALL(object_spawnType4);

    /* mov r0, r5 (encoded as ADDS r0, r5, #0, so it sets the flags) */
    R0 = R5;
    SET_FLAGS_ADD(R5, 0);

    R5 = callerR5;
    RETURN_TO_CALLER();
}

/* ── battle_ResetObjectTimerTo8_80C4072 ── */
/* battle_ResetObjectTimerTo8_80C4072(r0 = object pointer):
 * If the object exists, reset the field at offset +8 to 8 (likely a wait/counter).
 * Leaves r1 untouched when the pointer is null. */
Next battle_ResetObjectTimerTo8_80C4072(void) {
    if (R0 != 0) {
        R1 = 8;
        ST32(R0 + 8, R1);
    }
    RETURN_TO_CALLER();
}

/* ── battle_SetObjectExtraFlag14_80C4086 ── */
/* battle_SetObjectExtraFlag14_80C4086(r0 = battle object):
 * Mark the object's "extra vars" flag at ExtraVars+0x14 (e.g. an on-field
 * status trigger); does nothing when passed a null object pointer. */
Next battle_SetObjectExtraFlag14_80C4086(void) {
    if (R0 != 0)
        ST32(R0 + oBattleObject_ExtraVars + 0x14, 1);
    RETURN_TO_CALLER();
}

/* ── battle_EndObjectCurrentAnim_80C44D2 ── */
/* battle_EndObjectCurrentAnim_80C44D2(r0 = battle object):
 * End the object's current animation: write 0xFF (finished) into its
 * current-animation slot and call sub_801BCD0 to refresh its animation
 * state. A NULL object pointer is a no-op. r4/r5 are preserved.
 * Flags: with a NULL object they come from the tst; otherwise the last
 * flag-setting instruction before the call is the Thumb "mov r5, r0",
 * which is really "adds r5, r0, #0" and clears C/V. */
Next battle_EndObjectCurrentAnim_80C44D2(void) {
    u32 savedR5 = R5;

    SET_FLAGS_NZ(R0);              /* tst r0, r0 */
    if (R0 == 0)
        RETURN_TO_CALLER();

    ST8(R0 + oBattleObject_CurAnimCopy, 0xFF);
    R5 = R0;                       /* helper takes the object in r5 */
    SET_FLAGS_ADD(R0, 0);          /* "mov r5, r0" == adds r5, r0, #0 */
    GUEST_CALL(sub_801BCD0);
    R5 = savedR5;                  /* pop {r5} restores the caller's r5 */
    RETURN_TO_CALLER();
}

/* ── battle_SpawnAndInitObjectType3_80C4FFE ── */
/* battle_SpawnAndInitObjectType3_80C4FFE(r0-r2 = init arguments, r1-r3 also passed to the spawner):
 * Spawns a type-3 battle object via object_spawnType3 (called with r0 = 0;
 * the other arguments pass straight through). The new object comes back in
 * r5; if it is non-NULL, BasicBattleObjectInitialize is run on it with this
 * thunk's original r0-r2 shifted down into its r1-r3. The stack shuffling in
 * the original (push {r0-r2,r5,r6} / pop {r1-r3,r5,r6}) just preserves the
 * incoming arguments across the calls and slides r0-r2 into r1-r3.
 * Flags on the "nothing spawned" path: the "mov r0, r5" is really
 * "adds r0, r5, #0", so N/Z come from the object pointer and C/V are 0
 * (adding zero never carries or overflows). */
Next battle_SpawnAndInitObjectType3_80C4FFE(void) {
    u32 arg0 = R0, arg1 = R1, arg2 = R2;
    u32 savedR5 = R5, savedR6 = R6;

    /* Spawn with type = 0; r1-r3 are still the caller's values here. */
    R0 = 0;
    GUEST_CALL(object_spawnType3);

    u32 object = R5;

    /* adds r0, object, #0: N/Z from the pointer, C = V = 0. */
    SET_FLAGS_ADD(object, 0);

    /* Restore the incoming arguments for the initializer:
     * init(object, arg0, arg1, arg2). */
    R0 = object;
    R1 = arg0;
    R2 = arg1;
    R3 = arg2;
    R5 = savedR5;
    R6 = savedR6;

    if (object != 0)
        GUEST_CALL(BasicBattleObjectInitialize);

    RETURN_TO_CALLER();
}

/* ── battle_objectStepUpOntoElevatedPanel_80C5090 ── */
/* battle_objectStepUpOntoElevatedPanel_80C5090(r5 = battle object):
 * Once this object's Param1 (offset 4) reaches 0x1D, shift it one tile
 * upward — both its Y and Z fixed-point coordinates move by 0x10000.
 * Used when an object steps up onto an elevated panel. */
Next battle_objectStepUpOntoElevatedPanel_80C5090(void) {
    if (LD8(R5 + 4) == 0x1D) {
        ST32(R5 + oBattleObject_Y, LD32(R5 + oBattleObject_Y) + 0x10000);
        ST32(R5 + oBattleObject_Z, LD32(R5 + oBattleObject_Z) + 0x10000);
    }
    RETURN_TO_CALLER();
}

/* ── battleObject_PlayHurtAnimAndSetAction8_80C6524 ── */
/* battleObject_PlayHurtAnimAndSetAction8_80C6524(r5 = battle object):
 * Switch the object to animation 1, play sound effect 0xA2, and set its
 * current action to 8 (field at offset 9, same as the assembly's literal).
 * Exits with r0 = 8 and flags set from that value. */
Next battleObject_PlayHurtAnimAndSetAction8_80C6524(void) {
    ST8(R5 + oBattleObject_CurAnim, 1);

    R0 = 0xA2;
    GUEST_CALL(PlaySoundEffect);

    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 9, R0);

    RETURN_TO_CALLER();
}

/* ── battle_destroyObjectOnFrameFlag_80C6536 ── */
/* battle_destroyObjectOnFrameFlag_80C6536(r5 = battle object):
 * Query the object's sprite for its current frame parameters; if the frame
 * carries the 0x80 "self-destruct" flag, move the object into the destroyed
 * state. Flag outputs come from the AND (or the MOV on the destroy path),
 * with c/v left as sprite_getFrameParameters left them. */
Next battle_destroyObjectOnFrameFlag_80C6536(void) {
    GUEST_CALL(sprite_getFrameParameters);

    R1 = 0x80;
    R0 &= R1;
    SET_FLAGS_NZ(R0);

    if (R0 != 0) {
        R0 = 8; /* CUR_STATE_DESTROY */
        SET_FLAGS_NZ(R0);
        ST8(R5 + 8 /* oBattleObject_CurState */, R0);
    }

    RETURN_TO_CALLER();
}

/* ── battle_spawnAndInitObject0x33_80CA686 ── */
/* battle_spawnAndInitObject0x33_80CA686: spawn a battle object of type 0x33 and hand it the caller's
 * arguments. The new object pointer comes back from object_spawnType3 in r5;
 * if one was spawned, it is initialized via BasicBattleObjectInitialize with
 * the caller's r0-r2 shifted down into r1-r3 (the push/pop pair in the
 * original performs this shuffle). Returns the object pointer in r0
 * (0 if nothing was spawned); r1-r3 are clobbered. */
Next battle_spawnAndInitObject0x33_80CA686(void) {
    u32 arg0 = R0, arg1 = R1, arg2 = R2, arg5 = R5;

    R0 = 0x33;                     /* battle object type to spawn */
    GUEST_CALL(object_spawnType3); /* new object returned in R5 */

    u32 object = R5;
    R0 = object;
    if (object != 0) {
        R1 = arg0;
        R2 = arg1;
        R3 = arg2;
        R5 = arg5;
        GUEST_CALL(BasicBattleObjectInitialize);
    }

    RETURN_TO_CALLER();
}

/* ── battle_object_SetStateUpdatePhase_80CAB88 ── */
/* battle_object_SetStateUpdatePhase_80CAB88(r5 = battle object):
 * Advance the object's state machine: mark the current state as needing its
 * update step to run. Leaves r0 = 4 (flags set by the mov). */
Next battle_object_SetStateUpdatePhase_80CAB88(void) {
    R0 = 4; /* CUR_STATE_UPDATE */
    ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_MirrorRelatedObjectFlag1_80E0616 ── */
/* battle_MirrorRelatedObjectFlag1_80E0616(r5 = battle object): copy the "related object 1" bit of the
 * flag byte. Clears our flag bit 1, then sets it again iff the related
 * object has it set; does nothing if the object has no extra vars.
 * Flags (c/v) reflect `extra == 0`, as the original's CMP leaves them. */
Next battle_MirrorRelatedObjectFlag1_80E0616(void) {
    u32 extra = LD32(R5 + oBattleObject_ExtraVars);

    SET_FLAGS_CMP(extra, 0);
    if (extra != 0) {
        u32 related = LD32(R5 + oBattleObject_RelatedObject1Ptr);

        /* The object header flag byte sits at offset 0 of the object. */
        u32 flags = LD8(R5) & ~2u;

        R2 = 2;
        R1 = LD8(related) & R2;
        ST8(R5, flags | R1);
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectSetTimerFromParam1Table_80E19DC ── */
/* battle_objectSetTimerFromParam1Table_80E19DC(r5 = battle object):
 * Load the object's next Timer value from a lookup table indexed by its
 * Param1 byte, store it into the Timer field, then advance the object's
 * state to CUR_STATE_UPDATE (4) so the state machine continues next frame.
 * Outputs: r0 = CUR_STATE_UPDATE (nz set by the mov), r1 = timer value,
 * r7 = timer table address. */
#define oBattleObject_Param1 4
#define oBattleObject_CurStateActionPhaseAndPhaseInitialized 8
#define CUR_STATE_UPDATE 4

Next battle_objectSetTimerFromParam1Table_80E19DC(void) {
    /* Lookup table of timer values indexed by Param1 (literal pool pointer). */
    R7 = A_dword_80E19F0;

    u32 timer = LD8(R7 + LD8(R5 + oBattleObject_Param1));
    ST16(R5 + oBattleObject_Timer, timer);
    R1 = timer;

    R0 = CUR_STATE_UPDATE;
    SET_FLAGS_NZ(R0); /* flags left by the original's mov r0, #CUR_STATE_UPDATE */
    ST32(R5 + oBattleObject_CurStateActionPhaseAndPhaseInitialized, R0);

    RETURN_TO_CALLER();
}

#undef oBattleObject_Param1
#undef oBattleObject_CurStateActionPhaseAndPhaseInitialized
#undef CUR_STATE_UPDATE

/* ── battle_object_freeWithPhaseReset_80E1A5E ── */
/* battle_object_freeWithPhaseReset_80E1A5E(r5 = battle object):
 * Mark the object's phase as uninitialized and free its memory.
 * Results and flags are those left by object_freeMemory. */
Next battle_object_freeWithPhaseReset_80E1A5E(void) {
    ST8(R5 + oBattleObject_PhaseInitialized, 1);

    R0 = 1;
    GUEST_CALL(object_freeMemory);

    RETURN_TO_CALLER();
}

/* ── battle_objectFreeWrapper_80E1D72 ── */
/* battle_objectFreeWrapper_80E1D72(r0 = object): destructor for a battle object — hand its
 * memory slot back to the object pool via object_freeMemory.
 * All arguments (r0) and carried state (r5) are passed through untouched;
 * r0-r3 and the flags come back from object_freeMemory. */
Next battle_objectFreeWrapper_80E1D72(void) {
    GUEST_CALL(object_freeMemory);
    RETURN_TO_CALLER();
}

/* ── aiAttack_BeginAttackSequence_80EB84C ── */
/* aiAttack_BeginAttackSequence_80EB84C(r7 = AI attack vars):
 * Begin a new attack sequence: pick the attack command (2 when the byte at
 * offset 0x03 is 11, otherwise 1) and store it to Unk_12, clear the state
 * field Unk_30, and reload the attack timer Unk_00 with 8.
 * Leaves r0 = 8, r1 = the Unk_03 byte that was read; flags come from the
 * final MOV r0, #8 (N/Z cleared, C/V untouched). */
Next aiAttack_BeginAttackSequence_80EB84C(void) {
    u32 unk03 = LD8(R7 + 3); /* oAIAttackVars_Unk_03 */

    /* cmp r1, #0xB -- branch is taken when the byte differs from 11 */
    SET_FLAGS_CMP(unk03, 0xB);
    u16 command = (unk03 == 0xB) ? 2 : 1;

    ST16(R7 + oAIAttackVars_Unk_12, command);
    ST32(R7 + oAIAttackVars_Unk_30, 0);

    R0 = 8;
    R1 = unk03; /* r1 still holds the byte loaded at the top */
    ST16(R7 + 0, 8); /* oAIAttackVars_Unk_00 */
    SET_FLAGS_NZ(R0); /* mov r0, #8 -- final N/Z update */
    RETURN_TO_CALLER();
}

/* ── battle_getSpecialActorSlotIfAiMatches_80EBB78 ── */
/* battle_getSpecialActorSlotIfAiMatches_80EBB78(r5 = battle object):
 * Reads the object's AI data and checks for a specific actor: actor type 1
 * with AI index 0x12 and version 4. R0 = that actor's slot (0xC), or 0.
 * R2 ends holding the AI data's version byte; R7 is untouched (the original
 * only uses it as scratch and restores it on return). */
Next battle_getSpecialActorSlotIfAiMatches_80EBB78(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData);                    /* oAIData_ActorType (offset 0) */
    u32 aiIndex = LD8(aiData + 1);                  /* oAIData_AIIndex   (offset 1) */
    u32 version = LD8(aiData + oAIData_Version_16);

    R2 = version;

    u32 slot = 0;
    if (actorType == 1 && aiIndex == 0x12 && version == 4)
        slot = 0xC;

    R0 = slot;
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackWindupTick_80FE284 ── */
/* battle_aiAttackWindupTick_80FE284: AI attack state machine tick.
 * r5 = battle object, r7 = AI attack vars.
 * If the AI is idle (Unk_01 == 0), switch it to its wind-up state (Unk_01 = 4),
 * reset the object animation and start a 30-frame countdown.
 * Otherwise tick the countdown; when it expires, enter the attack state
 * (Unk_00 = 8).
 * Leaves R0 and the NZCV flags as the original's last flag-setting
 * instruction does. */
Next battle_aiAttackWindupTick_80FE284(void) {
    if (LD8(R7 + 1 /* oAIAttackVars_Unk_01 */) == 0) {
        /* Begin the wind-up phase. */
        ST8(R7 + 1 /* oAIAttackVars_Unk_01 */, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        R0 = 0x1E;
        ST16(R7 + oAIAttackVars_Unk_10, R0);
        SET_FLAGS_NZ(R0);
    } else {
        /* Count the wind-up timer down; once it hits zero, attack. */
        u32 remaining = LD16(R7 + oAIAttackVars_Unk_10);
        u32 left = remaining - 1;
        ST16(R7 + oAIAttackVars_Unk_10, left);
        SET_FLAGS_CMP(remaining, 1);
        if ((s32)left > 0) {
            R0 = left;
            RETURN_TO_CALLER();
        }
        R0 = 8;
        ST16(R7 /* oAIAttackVars_Unk_00 */, R0);
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── battle_objectTickWithExtraHandler_8109F6A ── */
/* battle_objectTickWithExtraHandler_8109F6A: per-frame update for one battle object (r5 = object).
 * While the object is still in phase 0, run the handler pointed to by
 * its second extra variable first; the phase tick (sub_80165C2) always
 * runs afterwards, with r0 carrying the phase byte / handler result. */
Next battle_objectTickWithExtraHandler_8109F6A(void) {
    if (LD8(R5 + oBattleObject_CurPhase) == 0) {
        R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
        GUEST_CALL(sub_800AA40);
    }
    GUEST_CALL(sub_80165C2);
    RETURN_TO_CALLER();
}

/* ── sub_8109F7E ── */
/* sub_8109F7E: call the two init steps in sequence (sub_8016B02, then
 * sub_8109FA2) and return; results are whatever the callees leave behind. */
Next sub_8109F7E(void) {
    GUEST_CALL(sub_8016B02);
    GUEST_CALL(sub_8109FA2);
    RETURN_TO_CALLER();
}

/* ── battle_object_updatePhaseTick_8109FC4 ── */
/* battle_object_updatePhaseTick_8109FC4: advance a battle object's phase bookkeeping.
 * If the object (r5) is still in phase 0, initialize its Param4 byte
 * (struct offset 7) to 0 as well; then run the shared per-object
 * update battleObjectStateDispatcher_8016CE8, which also produces the final flags. */
Next battle_object_updatePhaseTick_8109FC4(void) {
    if (LD8(R5 + oBattleObject_CurPhase) == 0)
        ST8(R5 + 7 /* oBattleObject_Param4 */, 0);

    GUEST_CALL(battleObjectStateDispatcher_8016CE8);

    RETURN_TO_CALLER();
}

/* ── ClearPETMenuDataFlag ── */
/* ClearPETMenuDataFlag(r0 = flag bit mask):
 * Clears the given bit(s) of the PET menu data flags byte (eFlags200DF25).
 * Returns the old flags byte in r1 and the ePETMenuData pointer in r2. */
Next ClearPETMenuDataFlag(void) {
    u32 flags = LD8(A_ePETMenuData + 5);

    R1 = flags & ~R0;
    ST8(A_ePETMenuData + 5, R1);
    R2 = A_ePETMenuData;

    RETURN_TO_CALLER();
}

/* ── IsButtonPressed ── */
/* IsButtonPressed(r0 = button mask):
 * Tests whether any of the buttons in r0 were newly pressed this frame.
 * Returns r1 = pressed-button word and sets NZ from (r0 & pressed),
 * so callers branch as if on "TST r0, r1". */
Next IsButtonPressed(void) {
    u32 joypad = LD32(R10 + 4);   /* r10 = toolkit, +4 = oToolkit_JoypadPtr */
    u32 pressed = LD16(joypad + 2); /* +2 = oJoypad_Pressed */

    R1 = pressed;
    SET_FLAGS_NZ(R0 & pressed);
    RETURN_TO_CALLER();
}

/* ── SetPETMenuDataFlag ── */
/* SetPETMenuDataFlag(r0 = flag bits to set):
 * OR the given bits into the PET menu's data flags byte at ePETMenuData+5
 * (eFlags200DF25 - 0x200df20). */
Next SetPETMenuDataFlag(void) {
    ST8(A_ePETMenuData + 5, LD8(A_ePETMenuData + 5) | R0);
    RETURN_TO_CALLER();
}

/* ── TestPETMenuDataFlag ── */
/* TestPETMenuDataFlag(r0 = flag mask):
 * Tests the PET menu flags byte (eFlags200DF25) against the mask in r0 and
 * leaves the Z/N flags set from (flags & mask), so callers can branch on it.
 * Returns: r1 = flags byte, r2 = ePETMenuData address, nz from the test. */
Next TestPETMenuDataFlag(void) {
    u32 flags = LD8(A_ePETMenuData + 5); /* (eFlags200DF25 - 0x200df20) */

    R1 = flags;
    R2 = A_ePETMenuData;
    SET_FLAGS_NZ(flags & R0);
    RETURN_TO_CALLER();
}

/* ── menu_moveCursorLeftRight_811FA0C ── */
/* menu_moveCursorLeftRight_811FA0C — horizontal cursor movement.
 *
 * Thin wrapper around the shared cursor mover menu_moveCursor_811FA36.
 * It builds the combined stack frame the mover's epilogue expects to pop
 * (saved r4-r7/lr on top of a 0x10-byte argument block), clears the
 * vertical bit of the direction argument so only left/right movement is
 * possible, and tail-jumps into the shared body. Because the mover pops
 * the save block pushed here, control returns straight from it to this
 * wrapper's caller with r4-r7 restored.
 *
 * Argument block (offsets from SP once the frame is set up):
 *   +0x00: 0x20  (initial repeat delay, frames)
 *   +0x04: 0x10  (auto-repeat rate, frames)
 *   +0x08: r3 as passed in
 *   +0x0C: r2 as passed in (bit 1 also cleared in the register copy)
 * r4 = 2 tells the shared body this is a horizontal move; r0, r1, r5,
 * r6, r7 and r10 pass through untouched.
 */
Next menu_moveCursorLeftRight_811FA0C(void) {
    /* One frame: 5 saved words (r4-r7, lr) + 0x10 bytes of arguments. */
    u32 frame = SP - 0x24;
    SP = frame;

    /* Save block for the shared epilogue (push {r4-r7,lr} layout). */
    ST32(frame + 0x10, R4);
    ST32(frame + 0x14, R5);
    ST32(frame + 0x18, R6);
    ST32(frame + 0x1C, R7);
    ST32(frame + 0x20, LR);

    /* Stack arguments for menu_moveCursor_811FA36. */
    ST32(frame + 0x00, 0x20);
    ST32(frame + 0x04, 0x10);
    ST32(frame + 0x08, R3);
    ST32(frame + 0x0C, R2);

    R2 &= ~2u; /* horizontal only: drop the vertical-move bit */
    R4 = 2;    /* direction mask read by the shared mover */

    GUEST_CALL(menu_moveCursor_811FA36);
    /* The mover popped the frame we built, so SP is back at its entry
     * value and r4-r7 hold the restored values; just return. */
    RETURN_TO_CALLER();
}

/* ── battle_packObjStateBitToAttrCode_8113154 ── */
/* battle_packObjStateBitToAttrCode_8113154(r4 = battle object, r6 = destination byte):
 * Packs the object's state bit at offset 0x16 (bit 0 only) into a small
 * attribute code: 0 -> 4, 1 -> 8, and stores it to [r6]. Sets r1 = 1 and
 * leaves the flags from the final add. */
Next battle_packObjStateBitToAttrCode_8113154(void) {
    u32 code = (LD8(R4 + 0x16) & 1) << 2;

    R1 = 1;
    SET_FLAGS_ADD(code, 4);
    code = code + 4;

    ST8(R6, code);
    R0 = code;
    RETURN_TO_CALLER();
}

/* ── sub_8116A28 ── */
/* sub_8116A28: conditionally fix up a state byte, then continue into sub_8116A44.
 * If the counter byte pointed to by r7 has reached 8, the byte at r5+8 is also
 * set to 8 (both are kept in sync). Flags/results come from sub_8116A44. */
Next sub_8116A28(void) {
    if (LD8(R7) == 8)
        ST8(R5 + 8, 8);

    GUEST_CALL(sub_8116A44);
    RETURN_TO_CALLER();
}

/* ── record_getNthSubRecordPtr_8116CD4 ── */
/* record_getNthSubRecordPtr_8116CD4(r5 = record pointer): returns r5 + 16 * (high nibble of byte at r5+2),
 * i.e. selects one of sixteen 16-byte sub-records within the record. */
Next record_getNthSubRecordPtr_8116CD4(void) {
    u32 kind = (LD8(R5 + 2) >> 4) << 4;
    R0 = R5 + kind;
    RETURN_TO_CALLER();
}

/* ── battle_objectUpdateWithFieldC_8116F8C ── */
/* battle_objectUpdateWithFieldC_8116F8C: update hook for the object in r5.
 * Runs the "before" update for the object, then passes the object's
 * halfword at +0xC to the "after" update routine. All register results
 * (r0-r3, flags) are simply whatever sub_81170C0 leaves behind. */
Next battle_objectUpdateWithFieldC_8116F8C(void) {
    GUEST_CALL(sub_81170AC);

    R0 = LD16(R5 + 0xC);
    GUEST_CALL(sub_81170C0);

    RETURN_TO_CALLER();
}

/* ── battle_getObjectPairOddByteClamped8_811774C ── */
/* battle_getObjectPairOddByteClamped8_811774C(r5 = object/state pointer):
 * Look up a byte in the table at unk_201C400. The index is taken from the
 * byte at [r5+4], doubled and incremented (i.e. the odd entry of a byte
 * pair), and the result is clamped to at most 8.
 * Returns: r0 = clamped table value, r1 = table index.
 * Flags: as if `cmp value, #8` ran; on the clamp path n/z are then
 * re-set by `mov r0, #8` (c/v keep the comparison's values). */
Next battle_getObjectPairOddByteClamped8_811774C(void) {
    u32 index = (LD8(R5 + 4) << 1) + 1;
    u32 value = LD8(A_unk_201C400 + index);

    SET_FLAGS_CMP(value, 8);
    if ((s32)value > 8) {
        value = 8;
        SET_FLAGS_NZ(value);
    }

    R0 = value;
    R1 = index;
    RETURN_TO_CALLER();
}

/* ── sub_8117764 ── */
/* sub_8117764: return 9, setting Z/N from that value. */
Next sub_8117764(void) {
    R0 = 9;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8117998 ── */
/* sub_8117998: fetches a lookup-table pointer from sub_8047D84, then computes
 * (byte[table] - 1) * 2 + byte[r5 + 4]; r0 = the resulting offset.
 * The entry index is 1-based, hence the -1. r4-r7 and lr are only shuffled
 * through the stack in the original; the "mov r6, #0" there is dead because
 * the pop overwrites r6, so this wrapper simply preserves r4-r7. */
Next sub_8117998(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(sub_8047D84);
    u32 table = R0;

    R0 = ((LD8(table) - 1) << 1) + LD8(savedR5 + 4);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── folder_CountChipsOfCodeOrSkipEntry_8117AA0 ── */
/* folder_CountChipsOfCodeOrSkipEntry_8117AA0(r0 = chip id, r1 = letter code index, r6 = folder entry pointer):
 * If r1 is a regular letter code (0..0x1A), count how many chips in the folder
 * match that id and code (GetChipCountOfCode) and hand the result to
 * sub_8117B94. Otherwise the entry has no letter code to match: skip past it
 * by advancing the folder pointer one entry (0x40 bytes).
 * Results are left in r2..r7 by the callee chain / the pointer bump. */
Next folder_CountChipsOfCodeOrSkipEntry_8117AA0(void) {
    if ((s32)R1 <= 0x1A) {
        /* r0/r1 already hold the chip id and code for GetChipCountOfCode. */
        GUEST_CALL(GetChipCountOfCode);
        /* sub_8117B94 picks up the count (and any other live registers). */
        GUEST_CALL(sub_8117B94);
    } else {
        R6 += 0x40;
    }
    RETURN_TO_CALLER();
}

/* ── nullsub_8118FE0 ── */
/* nullsub_8118FE0: empty stub — pushes and immediately pops LR, doing nothing.
 * Present in the original ROM only to occupy a slot in a function table. */
Next nullsub_8118FE0(void) {
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
 * (state done/waiting to advance), then runs sub_811976C's step.
 * r4-r9 are preserved; r0 and the flags come back from sub_811976C. */
Next npc_stepChainSetState0x10_811975C(void) {
    GUEST_CALL(sub_8119438);

    ST8(R5 + 9, 0x10);

    GUEST_CALL(sub_811976C);

    RETURN_TO_CALLER();
}

/* ── nullStub_8119800 ── */
/* nullStub_8119800: plain return (push {lr}; pop {pc} — equivalent to bx lr,
 * leaves all registers untouched). */
Next nullStub_8119800(void) {
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

/* ── sub_811A1E8 ── */
/* sub_811A1E8: empty stub — immediately returns to the caller without
 * touching any registers or memory (the original is just `push {lr}` /
 * `pop {pc}`). Kept as a symbol because the game calls it as a hook. */
Next sub_811A1E8(void) {
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

/* ── sprite_updateWrapper_811B45C ── */
/* sprite_updateWrapper_811B45C: thin wrapper that simply performs a sprite update with the
 * arguments already set up by the caller; all results (r0-r3 and flags)
 * come straight back from sprite_update. */
Next sprite_updateWrapper_811B45C(void) {
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

/* ── sprite_updateAnimPassthrough_811C22C ── */
/* sprite_updateAnimPassthrough_811C22C: update the current sprite animation frame (thin wrapper around
 * sprite_update). All arguments are passed through in the registers untouched;
 * results and flags are whatever sprite_update leaves behind. */
Next sprite_updateAnimPassthrough_811C22C(void) {
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_811CC02 ── */
/* sub_811CC02:
 * Perform the object_SetStateHandler_811CC4E action, then report success (r0 = 1).
 * r1 and the carry flag are whatever object_SetStateHandler_811CC4E left behind. */
Next sub_811CC02(void) {
    GUEST_CALL(object_SetStateHandler_811CC4E);

    R0 = 1;
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── sub_811CC26 ── */
/* sub_811CC26(r5 = pointer to a status byte):
 * Sets bit 1 of the status byte at r5 (marking it as "active"/processed),
 * invokes the counter_addU32_r5Plus0C_811CC60 helper with r0 = 2, and returns 1 to the caller.
 * r0 = 1, r1 = whatever the helper left, flags from "mov r0, #1". */
Next sub_811CC26(void) {
    ST8(R5, LD8(R5) | 2);   /* status |= 0x02 */

    R0 = 2;
    GUEST_CALL(counter_addU32_r5Plus0C_811CC60);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811CC3A ── */
/* sub_811CC3A(r5 = pointer to a status byte):
 * Clears bit 1 of the status byte at [r5], invokes counter_addU32_r5Plus0C_811CC60 with the
 * "command" value 2, and always returns 1 (flags set from the result). */
Next sub_811CC3A(void) {
    ST8(R5, LD8(R5) & ~2u);   /* clear the two's bit of [r5] */

    R0 = 2;                   /* argument for counter_addU32_r5Plus0C_811CC60 */
    GUEST_CALL(counter_addU32_r5Plus0C_811CC60);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_SetStateHandler_811CC4E ── */
/* object_SetStateHandler_811CC4E(r5 = object):
 * Resolve the object's current state handler. The byte at +0x04 is a state
 * index; use it to pick an entry from the handler table at off_811C9C0 and
 * store that pointer into the object at +0x0C.
 * Outputs: r0 = selected handler pointer, r1 = handler table base,
 * c = carry out of the index shift (always clear for valid state IDs). */
Next object_SetStateHandler_811CC4E(void) {
    u32 stateIndex = LD8(R5 + 4);

    R1 = A_off_811C9C0;
    R0 = stateIndex << 2;
    cpu.c = (stateIndex >> 30) & 1; /* carry as set by LSL r0, r0, #2 */

    R0 = LD32(R1 + R0);
    ST32(R5 + 0xC, R0);

    RETURN_TO_CALLER();
}

/* ── counter_addU32_r5Plus0C_811CC60 ── */
/* counter_addU32_r5Plus0C_811CC60(r0 = amount, r5 = struct):
 * Adds r0 to the 32-bit counter at struct+0xC and stores the result back.
 * Leaves r1 = the old counter value, r0 = the new counter value, and
 * c/v (plus nz) set as by the ADDS r0, r0, r1. */
Next counter_addU32_r5Plus0C_811CC60(void) {
    u32 amount = R0;
    u32 old_value = LD32(R5 + 0xC);
    u32 new_value = amount + old_value;

    R1 = old_value;
    R0 = new_value;
    ST32(R5 + 0xC, new_value);

    /* Flags come from the actual addition performed: amount + old_value. */
    SET_FLAGS_ADD(amount, old_value);

    RETURN_TO_CALLER();
}

/* ── pet_ClearMenuData_811EC00 ── */
/* pet_ClearMenuData_811EC00: clear the PET menu data struct.
 * Zero-fills the 0x18-byte ePETMenuData block and leaves r4 pointing at it. */
Next pet_ClearMenuData_811EC00(void) {
    R0 = A_ePETMenuData;
    R4 = R0;
    R1 = 0x18;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── submenu_ConfirmExitAndFade_811FB64 ── */
/* submenu_ConfirmExitAndFade_811FB64(r0 = selected submenu entry, r5 = submenu state pointer):
 * record the highlighted entry in the submenu state, play the
 * "exit submenu" sound effect, then start a screen fade
 * (duration 0xC, mode 0x10) to leave the submenu. */
Next submenu_ConfirmExitAndFade_811FB64(void) {
    ST8(R5 + 1, R0);

    R0 = SOUND_EXIT_SUBMENU;
    GUEST_CALL(PlaySoundEffect);

    R0 = 0xC;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    RETURN_TO_CALLER();
}

/* ── clearS2001c04Byte6_811FB78 ── */
/* clearS2001c04Byte6_811FB78(r10 = toolkit pointer):
 * clear the byte at offset 6 of the game's S2001c04 work area. */
Next clearS2001c04Byte6_811FB78(void) {
    u32 work = LD32(R10 + oToolkit_S2001c04_Ptr);
    ST8(work + 6, 0);
    RETURN_TO_CALLER();
}

/* ── sub_811FF54 ── */
/* sub_811FF54(r0 = index): look up an entry in the fixed byte table at
 * ROM byte_811FF60; r0 = byte at byte_811FF60[index]. The load is not
 * bounds-checked in the original, so keep it as a raw guest memory read. */
Next sub_811FF54(void) {
    R0 = LD8(A_byte_811FF60 + R0);
    RETURN_TO_CALLER();
}

/* ── clearWorkArea_200DCF0_8121144 ── */
/* clearWorkArea_200DCF0_8121144: clear the 0x18-halfword work area at word_200DCF0.
 * Arguments in r2-r7 are unused; they (and the carry/overflow flags left by
 * the zero-fill routine) pass through unchanged. */
Next clearWorkArea_200DCF0_8121144(void) {
    R0 = A_word_200DCF0;
    R1 = 0x18;
    GUEST_CALL(ZeroFillByHalfword);
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

/* ── textScript_loadAndRunEmbeddedArchive_812AF84 ── */
/* textScript_loadAndRunEmbeddedArchive_812AF84: decompress the embedded text archive (byte_812B168) into the
 * decomp buffer, then hand the decompressed script (eTextScript201BA20) and
 * its descriptor (off_812AFE4) to the text-script engine (sub_8123300).
 * Result registers (r1-r7) are whatever sub_8123300 leaves behind. */
Next textScript_loadAndRunEmbeddedArchive_812AF84(void) {
    /* Decompress the packed data blob into work RAM. */
    R0 = A_byte_812B168;
    GUEST_CALL(decompAndCopyData);

    /* Point the text engine at the decompressed script and run it. */
    R0 = A_off_812AFE4;
    R1 = A_eTextScript201BA20;
    R2 = A_eDecompBuffer2013A00;
    GUEST_CALL(sub_8123300);

    RETURN_TO_CALLER();
}

/* ── clearChipGiftRecordAndTakeChipsSlot0_8137808 ── */
/* clearChipGiftRecordAndTakeChipsSlot0_8137808:
 * Zero the 4-byte record at unk_20018EC, then hand the (now empty) record
 * to takeChipsWhenGivingFolderIfFolderSlotInUseMaybe_813781C with slot 0 in r0.
 * r4 is scratch for the record pointer here, but is restored for the caller. */
Next clearChipGiftRecordAndTakeChipsSlot0_8137808(void) {
    u32 savedR4 = R4;

    u32 record = A_unk_20018EC;

    /* ZeroFillByByte(record, 4) */
    R0 = record;
    R1 = 4;
    R4 = record;
    GUEST_CALL(ZeroFillByByte);

    /* takeChips...(0) — r4 still points at the freshly cleared record */
    R0 = 0;
    GUEST_CALL(takeChipsWhenGivingFolderIfFolderSlotInUseMaybe_813781C);

    R4 = savedR4;
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

/* ── RealClearChain ── */
/* RealClearChain(r0 = chain node): unlink a node from its doubly linked list.
 *
 * Each node links to its list anchor at +0x2C, to the previous node at +0x30
 * and to the next node at +0x34; the anchor keeps the first node at +0x20.
 * If the node is attached, splice it out (fixing up the head pointer when it
 * was the first entry) and clear its anchor link. If it is not attached
 * (anchor == NULL), leave everything untouched.
 *
 * On return: r3 = the anchor link as read, r2 = the node's old "prev" and
 * r1 = its old "next" (both only written when the node was attached, r1 ends
 * as 0 afterwards). Flags: every path ends with Z set / C set / V clear,
 * i.e. "equal". */
Next RealClearChain(void) {
    R3 = LD32(R0 + 0x2C);               /* the list this node is attached to */

    if (R3 != 0) {
        R1 = LD32(R0 + 0x34);           /* next node in the chain */
        R2 = LD32(R0 + 0x30);           /* previous node in the chain */

        if (R2 != 0)
            ST32(R2 + 0x34, R1);        /* prev->next = next */
        else
            ST32(R3 + 0x20, R1);        /* node was first: update list head */

        if (R1 != 0)
            ST32(R1 + 0x30, R2);        /* next->prev = prev */

        R1 = 0;
        ST32(R0 + 0x2C, 0);             /* mark the node as detached */
    }

    /* Both exit paths leave the flags as if comparing 0 against 0. */
    SET_FLAGS_CMP(0, 0);
    RETURN_TO_CALLER();
}

/* ── STWI_init_Callback_M ── */
/* STWI_init_Callback_M: clear the wireless callback handler (pass NULL to
 * STWI_set_Callback_M) so no more STWI callback is invoked. */
Next STWI_init_Callback_M(void) {
    R0 = 0;
    GUEST_CALL(STWI_set_Callback_M);
    RETURN_TO_CALLER();
}

/* ── STWI_init_Callback_S ── */
/* STWI_init_Callback_S:
 * Registers the default (NULL) callback for the STWI wireless lib and
 * returns to our caller. Equivalent to STWI_set_Callback_S(NULL). */
Next STWI_init_Callback_S(void) {
    R0 = 0;
    GUEST_CALL(STWI_set_Callback_S);
    RETURN_TO_CALLER();
}

/* ── STWI_set_Callback_ID ── */
/* STWI_set_Callback_ID(r0 = callback ID):
 * Store the given ID as the STWI (wireless) callback ID in the STWI work
 * struct pointed to by dword_2010CCC. */
Next STWI_set_Callback_ID(void) {
    ST32(LD32(A_dword_2010CCC) + 0x20, R0);
    RETURN_TO_CALLER();
}

/* ── STWI_set_Callback_M ── */
/* STWI_set_Callback_M(r0 = callback function pointer):
 * Store the given callback into the STWI work struct's callback slot (offset 0x18),
 * so the wireless library invokes it when a message arrives. */
Next STWI_set_Callback_M(void) {
    u32 stwiWork = LD32(A_dword_2010CCC);

    ST32(stwiWork + 0x18, R0);

    RETURN_TO_CALLER();
}

/* ── STWI_set_Callback_S ── */
/* STWI_set_Callback_S(r0 = callback function pointer):
 * Register r0 as the "send done" callback in the STWI library's work struct. */
Next STWI_set_Callback_S(void) {
    u32 stwiWork = LD32(A_dword_2010CCC);
    ST32(stwiWork + 0x1C, R0); /* oStwiWork_Callback */
    RETURN_TO_CALLER();
}

/* ── SWI_Div ── */
/* SWI_Div(r0 = dividend, r1 = divisor):
 * Thin wrapper around the BIOS signed-division call (svc 6). The BIOS itself
 * produces the results: r0 = dividend / divisor, r1 = dividend % divisor
 * (both signed), and r3 = abs(quotient) as an unsigned value. r2 and lr are
 * passed through untouched. */
Next SWI_Div(void) {
    swi_call(6);
    RETURN_TO_CALLER();
}

/* ── SoundMainBTM ── */
/* SoundMainBTM(r0 = sound work struct):
 * clear the 64-byte struct at r0 (four STM blocks of {r1-r4}), leaving r0
 * pointing just past it and r1-r3 zeroed. r4 is preserved; r12 receives the
 * incoming r4. Flags come from the final "movs r4, #0" (zero). */
Next SoundMainBTM(void) {
    u32 ptr = R0;

    R12 = R4;   /* saved r4, restored into r4 after the stores */
    R1 = 0;
    R2 = 0;
    R3 = 0;
    /* R4 is momentarily zeroed in the original, then restored from r12;
     * net effect: r4 unchanged. */

    for (u32 off = 0; off < 64; off += 4)
        ST32(ptr + off, 0);
    R0 = ptr + 64;

    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── ply_pend ── */
/* ply_pend(r1 = pending-entry list):
 * Pop the most recently queued entry and make it the current one. The list
 * header holds a count at offset 2; the queued entries start at offset 0x44
 * (4 bytes each), and the "current" slot is at offset 0x40. An empty list is
 * left untouched (except that r2 still holds the count read from the header).
 * Flags come from the count test on the empty path and from the address
 * addition on the pop path. */
Next ply_pend(void) {
    u32 list = R1;

    R2 = LD8(list + 2);
    if (R2 == 0) {
        /* list empty: nothing to pop, only the count test's flags remain */
        SET_FLAGS_CMP(R2, 0);
        RETURN_TO_CALLER();
    }

    R2 = R2 - 1;
    ST8(list + 2, R2);

    R2 = R2 << 2;
    R3 = list + R2;
    SET_FLAGS_ADD(list, R2);

    R2 = LD32(R3 + 0x44);
    ST32(list + 0x40, R2);

    RETURN_TO_CALLER();
}

/* ── rfu_setTimerInterrupt ── */
/* rfu_setTimerInterrupt(r0, r1): set up the hardware timer used by the
 * wireless library, with the two arguments swapped and the first one
 * truncated to 8 bits before being handed to STWI_init_timer.
 * Whatever STWI_init_timer leaves in r2-r5 is passed through to our caller. */
Next rfu_setTimerInterrupt(void) {
    u32 argA = R0;
    u32 argB = R1;

    R0 = argB;
    R1 = argA & 0xFF;
    GUEST_CALL(STWI_init_timer);

    RETURN_TO_CALLER();
}

/* ── sub_8144240 ── */
/* sub_8144240: run both steps of the (likely per-frame) update for this
 * subsystem: sub_8144158 followed by sub_81440D8. Neither takes arguments;
 * whatever the callees leave in r2/r3 is passed through to the caller. */
Next sub_8144240(void) {
    GUEST_CALL(sub_8144158);
    GUEST_CALL(sub_81440D8);
    RETURN_TO_CALLER();
}

/* ── timer_IsEnabled_8144CE0 ── */
/* timer_IsEnabled_8144CE0: return the enabled state of the timer flags at
 * eTimerEnable200F460 (its second byte). */
Next timer_IsEnabled_8144CE0(void) {
    R0 = LD8(A_eTimerEnable200F460 + 1);
    RETURN_TO_CALLER();
}

/* ── sub_814E0B4 ── */
/* sub_814E0B4(r0 = arg, r2 = pointer): read the byte at *r2 into r3 and
 * tail-call sub_814E0B6, which continues the work with that byte.
 * r0 is passed through untouched; flags and results come from the callee. */
Next sub_814E0B4(void) {
    R3 = LD8(R2);
    GUEST_CALL(sub_814E0B6);
    RETURN_TO_CALLER();
}

/* ── sub_814E0B6 ── */
/* sub_814E0B6(r0 preserved, r2 = value to check, r3):
 * Range-check r2 and clear r3 when it is not an acceptable address.
 * Always returns with r0 unchanged. Flags reflect the last check performed:
 *   - if r2 has bit 25 set (>= 0x02000000) the check passes untouched and the
 *     flags come from that top-bit shift,
 *   - otherwise r2 is compared against the ROM constant off_81C0418; values
 *     below it fail (r3 = 0, flags from the comparison's MOV after it),
 *   - values at/above it are split on bit 14 (r2 >> 14) before r3 is cleared. */
Next sub_814E0B6(void) {
    u32 addr = R2;
    u32 saved_r0 = R0; /* the original pushes/pops r0 around its scratch use */

    /* Top-bit test: anything with bit 25 set is accepted as-is. */
    R0 = addr >> 25;
    SET_FLAGS_NZ(R0);
    cpu.c = (addr >> 24) & 1; /* LSR #25 carry: last bit shifted out */
    if (R0 != 0) {
        R0 = saved_r0;
        RETURN_TO_CALLER();
    }

    /* Below 0x02000000: compare against the ROM constant. */
    {
        u32 limit = A_off_81C0418;
        SET_FLAGS_CMP(addr, limit);
        if (addr < limit) {
            R3 = 0; /* mov r3,#0: N,Z only, C/V still from the CMP */
            SET_FLAGS_NZ(R3);
            R0 = saved_r0;
            RETURN_TO_CALLER();
        }
    }

    /* At/above the constant: shift test on bit 14, then clear r3 unless zero. */
    R0 = addr >> 14;
    SET_FLAGS_NZ(R0);
    cpu.c = (addr >> 13) & 1; /* LSR #14 carry: last bit shifted out */
    if (R0 != 0) {
        R3 = 0; /* mov r3,#0: N,Z only, C/V still from the shift/CMP */
        SET_FLAGS_NZ(R3);
    }
    R0 = saved_r0;
    RETURN_TO_CALLER();
}

/* ── sub_814E168 ── */
/* sub_814E168: refresh two halfword fields of the object whose address
 * sub_814E0D0 returns in r0. Field +0x1C receives the callee's r3 doubled;
 * field +0x20 receives that doubled value scaled by the halfword at +0x1E
 * (an 8.8 fixed-point multiply: (doubled * scale) >> 8).
 * The original saves its return address into r12 and returns through it,
 * so r12 must end as the incoming lr; the callee preserves r12.
 * Flags come from the final shift: nz from the result, c from the last bit
 * shifted out (bit 7 of the product). The callee's r0/r2/r3 results stay in
 * r0/r2/r3 as the original leaves them. */
Next sub_814E168(void) {
    R12 = LR;   /* mov r12, lr — the original returns via bx r12 */

    GUEST_CALL(sub_814E0D0);

    u32 obj = R0;

    R3 <<= 1;                       /* doubled value from the callee */
    ST16(obj + 0x1C, R3);

    R2 = LD16(obj + 0x1E);          /* fixed-point scale factor */
    u32 product = R3 * R2;
    cpu.c = (product >> 7) & 1;     /* carry out of the lsr #8 */
    R3 = product >> 8;
    SET_FLAGS_NZ(R3);
    ST16(obj + 0x20, R3);

    RETURN_TO_CALLER();
}

/* ── sub_814E17C ── */
/* sub_814E17C(r1 = object, remaining inputs forwarded to sub_814E0D0):
 * Runs sub_814E0D0, which computes a byte value in r3 from the object state.
 * That byte is written to offset 0xA of the object, and the flag byte at
 * offset 0 gets its 0xC bits forced on (object marked as ...).
 * Returns r2 = 0xC, r3 = updated flag byte; NZ flags come from the ORR.
 * r1 and r12 pass through unchanged (sub_814E0D0 preserves both). */
Next sub_814E17C(void) {
    R12 = LR; /* sub_814E0D0 preserves r12; it still holds our return address */

    u32 obj = R1;
    GUEST_CALL(sub_814E0D0);

    ST8(obj + 0xA, R3);      /* store the computed byte */

    R2 = 0xC;
    R3 = LD8(obj) | R2;      /* set the 0xC bits in the object's flag byte */
    SET_FLAGS_NZ(R3);
    ST8(obj, R3);

    RETURN_TO_CALLER();
}

/* ── battleObj_runUpdateAndSetActive_814E1C0 ── */
/* battleObj_runUpdateAndSetActive_814E1C0: run sub_814E0D0's update for the object pointer in r1, then
 * store the byte result it leaves in r3 at offset 0x12 and set the low two
 * bits of the object's status byte (marking it active/updated).
 * Returns to the caller; flags come from the final ORR. */
Next battleObj_runUpdateAndSetActive_814E1C0(void) {
    R12 = LR;                    /* the original returns through a saved copy of LR */

    GUEST_CALL(sub_814E0D0);

    {
        u32 obj = R1;            /* object pointer, preserved by the callee */

        ST8(obj + 0x12, R3);     /* result byte produced by the call */

        R2 = 3;
        R3 = LD8(obj) | R2;      /* set status bits 0-1 */
        ST8(obj, R3);
        SET_FLAGS_NZ(R3);
    }

    RETURN_TO_CALLER();
}

/* ── sprite_moveYUp64AndMarkDirty_814E1D4 ── */
/* sprite_moveYUp64AndMarkDirty_814E1D4: adjust an object's Y coordinate and mark it as changed.
 * Calls sub_814E0D0 to compute a value (R1 = object pointer, R3 = value),
 * stores value - 0x40 (shifted up 64 pixels/units) into the object's
 * field at +0x14, then sets the low two bits of the object's byte at +0x00
 * (its "dirty"/state flags) so the change is picked up.
 * Returns via the saved LR in R12, as the original does with `bx r12`. */
Next sprite_moveYUp64AndMarkDirty_814E1D4(void) {
    R12 = LR;                     /* sub_814E0D0 clobbers LR; keep our return address */

    GUEST_CALL(sub_814E0D0);      /* R1 = object ptr, R3 = computed value */

    SET_FLAGS_CMP(R3, 0x40);      /* SUBS r3, #0x40 */
    R3 -= 0x40;
    ST8(R1 + 0x14, R3);

    R2 = 3;
    R3 = LD8(R1) | R2;            /* ORR r3, r2 sets N/Z only */
    SET_FLAGS_NZ(R3);
    ST8(R1, R3);

    RETURN_TO_CALLER();
}

/* ── sub_814E1E8 ── */
/* sub_814E1E8: run the check performed by sub_814E0D0, then record its
 * outcome in the state struct whose address is left in r1:
 *   - the status byte returned in r3, biased down by 0x40, is stored
 *     as the result field at offset 0x0E;
 *   - the flag bits 0x0C are set in the header byte at offset 0x00.
 * Returns to the caller through r12, like the original. */
Next sub_814E1E8(void) {
    R12 = LR; /* the original branches back through r12 */

    GUEST_CALL(sub_814E0D0);

    /* store the callee's status byte, biased down by 0x40 */
    u32 result = R3 - 0x40;
    SET_FLAGS_CMP(R3, 0x40); /* SUB r3, #0x40 sets c/v for the caller */
    ST8(R1 + 0xE, result);

    /* set the 0x0C flag bits in the struct's header byte */
    R2 = 0xC;
    u32 header = LD8(R1) | R2;
    SET_FLAGS_NZ(header); /* ORR is the original's last flag-setting op */
    ST8(R1, header);
    R3 = header;

    RETURN_TO_CALLER();
}

/* ── sub_814E1FC ── */
/* sub_814E1FC: finish some state update on the structure pointed to by r1.
 * Calls sub_814E0D0 (which produces a status byte in r3), stores that byte at
 * [r1+0xF], then sets the 0xC flag bits in the state byte at [r1].
 * Preserves the caller's return address in r12, as the original does. */
Next sub_814E1FC(void) {
    u32 state = R1;

    R12 = LR; /* mov r12, lr: keep the return address across the call */

    GUEST_CALL(sub_814E0D0); /* leaves the new status byte in R3 */

    ST8(state + 0xF, R3);

    R3 = LD8(state) | 0xC;
    R2 = 0xC;
    SET_FLAGS_NZ(R3);
    ST8(state, R3);

    RETURN_TO_CALLER();
}

/* ── sub_814E210 ── */
/* sub_814E210: wrapper around sub_814E0D0 — calls it, then stores the byte
 * result it leaves in r3 at [r1 + 0x1B]. Everything else (including r0 and
 * the flags left by the callee) passes through unchanged; returns to the
 * caller's original return address, kept in r12 like the original does. */
Next sub_814E210(void) {
    R12 = LR;
    GUEST_CALL(sub_814E0D0);
    ST8(R1 + 0x1B, R3);
    RETURN_TO_CALLER();
}

/* ── chatbox_PrintBuffer_WriteNextGlyph_814E234 ── */
/* chatbox_PrintBuffer_WriteNextGlyph_814E234: emit one character into the print buffer at r1.
 * Calls sub_814E0D0 to fetch the next glyph code (returned in r3),
 * stores the adjusted glyph index (code - 0x40) at [r1+0xC], then sets
 * bits 0x0C of the state byte at [r1] to mark that a glyph was written.
 * Preserves r1 and r12 across the call; returns to the original caller. */
Next chatbox_PrintBuffer_WriteNextGlyph_814E234(void) {
    R12 = LR; /* original return address; handed back unchanged on exit */

    GUEST_CALL(sub_814E0D0); /* -> R3 = glyph code */

    /* SUBS r3, #0x40: adjust the code and keep its carry/overflow flags,
     * since the ORR below only refreshes nz. */
    SET_FLAGS_CMP(R3, 0x40);
    R3 -= 0x40;
    ST8(R1 + 0xC, R3);

    /* set the "glyph written" flag bits in the state byte (r2 = 0xC,
     * as the original writes the constant via r2) */
    R2 = 0xC;
    R3 = LD8(R1) | R2;
    SET_FLAGS_NZ(R3);
    ST8(R1, R3);

    RETURN_TO_CALLER();
}

/* ── sub_814E7A4 ── */
/* sub_814E7A4(r1 = status struct ptr):
 * Clear the two status bytes at +0x16 and +0x1A, then OR a mode flag into
 * byte 0: 0xC if byte +0x18 is zero, 3 otherwise.
 * Leaves r2 = mode flag, r3 = new byte 0.
 * Flags: N/Z from the OR result; the final ORR leaves C and V alone, so
 * they still hold what the "cmp r2, #0" left there (C set, V clear). */
Next sub_814E7A4(void) {
    ST8(R1 + 0x16, 0);
    ST8(R1 + 0x1A, 0);

    u32 delayByte = LD8(R1 + 0x18);
    SET_FLAGS_CMP(delayByte, 0);    /* C=1 (no borrow), V=0, kept by the ORR */

    u32 mode = (delayByte == 0) ? 0xC : 3;

    u32 flags = LD8(R1) | mode;
    ST8(R1, flags);

    R2 = mode;
    R3 = flags;
    SET_FLAGS_NZ(flags);

    RETURN_TO_CALLER();
}

/* ── streamReadNextByte_814E7C0 ── */
/* streamReadNextByte_814E7C0(r1 = stream state):
 * Reads the next byte from a byte stream whose current position is stored at
 * [r1+0x40], then advances the position. Outputs:
 *   r2 = address the byte was read from (position before the increment)
 *   r3 = the byte value itself. */
Next streamReadNextByte_814E7C0(void) {
    u32 stream = R1;
    u32 pos = LD32(stream + 0x40);
    ST32(stream + 0x40, pos + 1);
    R2 = pos;
    R3 = LD8(pos);
    RETURN_TO_CALLER();
}

/* ── sub_814E7CC ── */
/* sub_814E7CC: tail of a getter/setter pair.
 * Calls streamReadNextByte_814E7C0 (which returns a byte in r3 and a struct pointer in r1),
 * stores the byte at [r1 + 0x19], and if it was zero also calls
 * sub_814E7A4. r12 is kept as the original does (r12 = lr on entry). */
Next sub_814E7CC(void) {
    R12 = LR;

    GUEST_CALL(streamReadNextByte_814E7C0);

    ST8(R1 + 0x19, R3);
    if (R3 == 0) {
        /* Value was zero: run the extra fixup. Flags on this path are
         * whatever sub_814E7A4 leaves behind. */
        GUEST_CALL(sub_814E7A4);
    } else {
        /* Reproduce the `cmp r3, #0` for callers that branch on return. */
        SET_FLAGS_CMP(R3, 0);
    }

    RETURN_TO_CALLER(); /* original returns via `bx r12` with r12 == lr */
}

/* ── sub_814E7E0 ── */
/* sub_814E7E0: wrapper around streamReadNextByte_814E7C0.
 * Calls streamReadNextByte_814E7C0, stores its byte result at [r1 + 0x17], and — when the
 * result is zero — follows up with sub_814E7A4 before returning to the caller
 * (the return address is kept in r12 across both calls). */
Next sub_814E7E0(void) {
    R12 = LR;                       /* mov r12, lr: remember where to return */

    GUEST_CALL(streamReadNextByte_814E7C0);

    ST8(R1 + 0x17, R3);             /* store the byte result into the struct */
    SET_FLAGS_CMP(R3, 0);
    if (R3 == 0)
        GUEST_CALL(sub_814E7A4);    /* extra fixup only when result was zero */

    RETURN_TO_CALLER();             /* bx r12 */
}

/* ── battle_smashFx_SetParams_814E87C ── */
/* battle_smashFx_SetParams_814E87C(r0 = object, r1 = halfword value):
 * If the object's field at +0x34 holds the magic ID 'Smsh' (0x68736D53),
 * store the value (zero-extended to 16 bits) into fields +0x24 and +0x26
 * and store 0x100 into field +0x28.
 * The original pushes LR and pops it into r0 (`pop {r0}; bx r0`), so on
 * exit R0 is the return address. Flags reflect the magic comparison (or,
 * on the match path, the final `lsl r0, r0, #1`). */
Next battle_smashFx_SetParams_814E87C(void) {
    u32 return_address = LR; /* push {lr} ... pop {r0} */

    R2 = R0;
    R1 &= 0xFFFF; /* lsl #16 / lsr #16: keep only the low halfword */

    R3 = LD32(R2 + 0x34);
    R0 = 0x68736D53; /* 'Smsh' */
    SET_FLAGS_CMP(R3, R0);

    if (R3 == R0) {
        ST16(R2 + 0x26, R1);
        ST16(R2 + 0x24, R1);

        R0 = 0x80 << 1; /* 0x100; lsl sets C from the pre-shift top bit */
        cpu.c = 0;
        SET_FLAGS_NZ(R0);
        ST16(R2 + 0x28, R0);
    }

    R0 = return_address;
    RETURN_TO_CALLER();
}

/* ── text_ReadNextChar_814FE34 ── */
/* text_ReadNextChar_814FE34(r1 = text state):
 * Read the next character of a text script: load the byte at the current
 * read pointer, copy it to the state's current-character field, then advance
 * the pointer past it.
 * Returns r0 = advanced read pointer, r2 = the byte that was read;
 * flags reflect the pointer increment (ADDS). */
Next text_ReadNextChar_814FE34(void) {
    u32 state = R1;

    u32 readPos = LD32(state + 0x40);
    u32 ch = LD8(readPos);
    ST8(state + 0x1E, ch);

    readPos = readPos + 1;
    SET_FLAGS_ADD(readPos, 1);
    ST32(state + 0x40, readPos);

    R0 = readPos;
    R2 = ch;
    RETURN_TO_CALLER();
}

/* ── readNextStreamByte_814FE40 ── */
/* readNextStreamByte_814FE40(r1 = record pointer):
 * Copies the next byte of the data stream referenced by [r1+0x40] into the
 * byte field at [r1+0x1F], then advances the stream pointer past it.
 * Outputs: r0 = advanced stream pointer, r2 = the byte that was read;
 * nz/c/v come from the pointer increment (ADDS r0, #1). */
Next readNextStreamByte_814FE40(void) {
    u32 streamPos = LD32(R1 + 0x40);
    u8 value = LD8(streamPos);

    ST8(R1 + 0x1F, value);

    streamPos += 1;
    ST32(R1 + 0x40, streamPos);

    R0 = streamPos;
    R2 = value;
    SET_FLAGS_ADD(streamPos, 1);
    RETURN_TO_CALLER();
}

/* ── umul3232H32 ── */
/* umul3232H32(r0, r1): 64-bit unsigned multiply r0 * r1.
 * Returns the high 32 bits of the product in r0; the low 32 bits are also
 * left in r2 and the high bits in r3 (as the original's umull does).
 * The original switches to ARM just to execute the single umull. */
Next umul3232H32(void) {
    u64 product = (u64)R0 * (u64)R1;
    R2 = (u32)product;        /* low word  */
    R3 = (u32)(product >> 32); /* high word */
    R0 = R3;                  /* result: high word */
    RETURN_TO_CALLER();
}
