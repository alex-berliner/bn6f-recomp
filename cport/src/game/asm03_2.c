/*
 * asm03_2.c: readable functions of bn6f asm03_2.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── renderTextGfx_8045F8C ── */
/* renderTextGfx_8045F8C(r0 = text graphics base, r1 = entry index):
 *
 * Hands a text's graphics data to the glyph renderer sub_3006B94. The
 * graphics base is first stashed in the toolkit's text-renderer state
 * (offset 0x40), then the callee is invoked with r0 = graphics +
 * offsetTable[index], where the leading table of u16 offsets picks which
 * chunk of the graphics to draw, and r1 = r9 as its second argument.
 *
 * The original saves r8/r9 around the call with push/pop pairs (Thumb
 * cannot push high registers directly) and, as a side effect of popping
 * into r1 both times, ends with r1 = the saved r8 value. */
Next renderTextGfx_8045F8C(void) {
    u32 gfx = R0;

    /* Publish the graphics base for the text renderer state. */
    u32 textState = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(textState + 0x40, gfx);

    /* gfx starts with a u16 offset table; entry r1 gives the offset (from
       gfx itself) of the glyph data to render. */
    R0 = gfx + LD16(gfx + (R1 << 1));

    u32 savedR8 = R8;
    u32 savedR9 = R9;
    R1 = R9;                    /* sub_3006B94 takes r9 as its second arg */
    GUEST_CALL(sub_3006B94);
    R1 = savedR8;               /* pop {r1}; mov r8, r1 leaves r1 = old r8 */
    R8 = savedR8;
    R9 = savedR9;

    RETURN_TO_CALLER();
}

/* ── battle_initEStructArr_804657C ── */
/* battle_initEStructArr_804657C: initialise eStructArr2008450.
 * Zero-fills the whole 0x528-byte array (15 entries of 0x58 bytes each),
 * then sets every entry's byte at +2 to 0x20 and its byte at +3 to the
 * 0-based entry index.
 * Preserves r2-r7; leaves r1 as ZeroFillByWord left it. */
Next battle_initEStructArr_804657C(void) {
    R0 = A_eStructArr2008450;
    R1 = 0x528;
    GUEST_CALL(ZeroFillByWord);

    u32 entry = A_eStructArr2008450;
    for (u32 i = 0; i < 15; i++) {
        ST8(entry + 2, 0x20);   /* element type */
        ST8(entry + 3, i);      /* entry index */
        entry += 0x58;
    }

    /* Flags come from the init loop's final `cmp r7, #0xF` (r7 == 0xF here). */
    SET_FLAGS_CMP(0xF, 0xF);
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

/* ── sub_8046664 ── */
/* sub_8046664: run both halves of the (unspecified) update routine:
 * first sub_80466AA, then battle_initEStructArr_804657C. All registers other than the
 * flags are preserved; r4-r7 are untouched here and by both callees. */
Next sub_8046664(void) {
    GUEST_CALL(sub_80466AA);
    GUEST_CALL(battle_initEStructArr_804657C);
    RETURN_TO_CALLER();
}

/* ── sub_8046670 ── */
/* sub_8046670(r0 = id byte, r1 = slot index, r2 = param word):
 * initialize one 0x58-byte entry of eStructArr2008450 for r1*0x58:
 * set the entry's id byte, type 9 marker, param word, and clear its counter.
 * Returns r0 = pointer to the entry, r1 = r1*0x58, flags from ADDS r0, entry, #0. */
Next sub_8046670(void) {
    u32 entry = A_eStructArr2008450 + R1 * 0x58;

    ST8(entry + 1, R0);   /* id byte            */
    ST8(entry,      9);   /* entry type = 9     */
    ST32(entry + 4, R2);  /* parameter          */
    ST32(entry + 8, 0);   /* counter/state = 0  */

    R1 = R1 * 0x58;       /* original leaves the scaled index in r1 */
    R0 = entry;
    SET_FLAGS_ADD(entry, 0);  /* adds r0, entry, #0: N/Z from entry, C set, V clear */
    RETURN_TO_CALLER();
}

/* ── sprite_clearScaleCounterAndMakeUnscalable_804668A ── */
/* sprite_clearScaleCounterAndMakeUnscalable_804668A(r5 = pointer to the sprite's scale/cycle counter):
 * Clears the byte at *r5 to 0, then rebuilds the sprite with
 * sprite_makeUnscalable (called with r0 = 0). r4-r7 are preserved. */
Next sprite_clearScaleCounterAndMakeUnscalable_804668A(void) {
    R0 = 0;             /* mov r0, #0 */
    ST8(R5, R0);        /* strb r0, [r5] */

    GUEST_CALL(sprite_makeUnscalable); /* bl sprite_makeUnscalable */

    RETURN_TO_CALLER();
}

/* ── getStructFrom2008450 ── */
/* getStructFrom2008450(r0 = struct index): address one of the fixed 0x58-byte
 * entries of eStructArr2008450 and load the byte at its start (its "in use"
 * flag). Returns that byte in r0 with NZ flags set from it; r1 ends holding
 * the entry's address and r2 the byte offset 0x58*index, as callers expect.
 *
 * Note: the original's ADDS (base + 0x58*index) is the last instruction that
 * sets C and V — the trailing TST only updates NZ — so the add's carry and
 * overflow must be reproduced for callers that branch on them. */
Next getStructFrom2008450(void) {
    u32 entryOffset = 0x58 * R0;
    u32 entryAddr = A_eStructArr2008450 + entryOffset;

    SET_FLAGS_ADD(A_eStructArr2008450, entryOffset); /* C/V from the ADDS */

    R1 = entryAddr;
    R2 = entryOffset;
    R0 = LD8(entryAddr);

    SET_FLAGS_NZ(R0); /* TST r0, r0: NZ from the loaded byte */
    RETURN_TO_CALLER();
}

/* ── initWorkArea5Entries_80467D8 ── */
/* initWorkArea5Entries_80467D8: clear the 0x1B8-byte work block at byte_2011860, then set up
 * its five 0x58-byte entries: byte 2 = 0x20 (entry "alive"/default value),
 * byte 3 = entry index. r0 = 0x20, r1 = 0x1B8 afterwards; r2-r7 preserved. */
Next initWorkArea5Entries_80467D8(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R0 = A_byte_2011860;
    R1 = 0x1B8; /* 5 entries * 0x58 bytes */
    GUEST_CALL(ZeroFillByWord);

    u32 entry = A_byte_2011860;
    for (u32 i = 0; i < 5; i++) {
        ST8(entry + 2, 0x20); /* entry state */
        ST8(entry + 3, i);    /* entry index */
        entry += 0x58;
    }

    /* Exit state of the original's last CMP r7, #5 (counter ended at 5). */
    SET_FLAGS_CMP(5, 5);
    R0 = 0x20;
    R1 = 0x1B8;
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── reqBBS_initRequestWorkArea_8046CC8 ── */
/* reqBBS_initRequestWorkArea_8046CC8(r0 = BBS request slot index, r10 = toolkit):
 * Prepares the BBS request work area for the selected request:
 * clears the 0x34-byte buffer, stores the slot index and the per-request
 * data pointer/text-offset table, and records the current map group as
 * the group to return to. Leaves r2 = request data table base. */
Next reqBBS_initRequestWorkArea_8046CC8(void) {
    u32 slot = R0;
    u32 toolkit = R10;

    /* Zero-fill the whole request work area (0x34 bytes). */
    R0 = A_byte_2011C10;
    R1 = 0x34;
    GUEST_CALL(ZeroFillByWord);

    ST8(A_byte_2011C10 + 4, slot);

    R2 = A_dword_8046B98;
    ST32(A_byte_2011C10 + 0x1C, R2 + slot * 0x10);
    ST32(A_byte_2011C10 + 0x24, A_reqBBS_requestInfo_textOffsets);

    /* Save the current map group as the "last visited" group.
     * (The map group lives at offset 4 of the game state struct.) */
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    ST16(gameState + oGameState_LastMapGroup, LD16(gameState + 4));

    RETURN_TO_CALLER();
}

/* ── battle_objectAdvanceCounterToState8_8046DDC ── */
/* battle_objectAdvanceCounterToState8_8046DDC: advance the counter at (R5+8) by 2. Once it reaches 0x15,
 * clamp it there and move the object at R5 into state 8 (byte at R5+0).
 * Then runs sub_8047134 (which reads R5) and returns 0. */
Next battle_objectAdvanceCounterToState8_8046DDC(void) {
    u32 counter = (u32)LD8(R5 + 8) + 2;

    ST8(R5 + 8, counter);

    if ((s32)counter >= 0x15) {
        ST8(R5 + 8, 0x15);
        ST8(R5, 8);
    }

    GUEST_CALL(sub_8047134);

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8046DF8 ── */
/* sub_8046DF8: run two update passes (sub_8047950 then sub_8047134) and
 * return 0. Always yields r0 = 0 with N/Z set accordingly. */
Next sub_8046DF8(void) {
    GUEST_CALL(sub_8047950);
    GUEST_CALL(sub_8047134);

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_stepContextScript_8047028 ── */
/* chatbox_stepContextScript_8047028: run the chatbox script attached to the dialogue context in R5.
 *
 * First updates some inter-frame state (sub_8047134), then, while the
 * 0x8 event flag is set, stores the text scroll speed into the context's
 * state byte and starts the script indexed from the context:
 *   - offset 0x12 (halfword) nonzero -> script #1,  speed 8
 *   - offset 0x12 (halfword) zero    -> script #10, speed 32
 * The script itself is read from the archive pointer at offset 0x24.
 *
 * Always returns R0 = 0 (Z set), with R1/R2 left as the callees left them.
 * Preserves R4-R7 and everything from R8 up.
 */
Next chatbox_stepContextScript_8047028(void) {
    GUEST_CALL(sub_8047134);

    /* Proceed only while event flag 0x8 is set. */
    R0 = 8;
    GUEST_CALL(chatbox_mask_eFlags2009F38); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
        /* Pick the script and scroll speed based on the repeat counter. */
        u32 scriptIdx = 1;
        u32 scrollSpeed = 8;

        if (LD16(R5 + 0x12) == 0) {
            scriptIdx = 0xA;
            scrollSpeed = 0x20;
        }

        ST8(R5, scrollSpeed);
        R0 = LD32(R5 + 0x24);
        R1 = scriptIdx;
        GUEST_CALL(chatbox_runScript);
    }

    R0 = 0;
    SET_FLAGS_NZ(R0);
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

/* ── storeObfuscatedRandomToken_804733C ── */
/* storeObfuscatedRandomToken_804733C(r0 = base value, r5 = object):
 * Rolls a random byte (retrying until it is nonzero) and stores an obfuscated
 * code derived from it into object+0x30:
 *
 *     stored = ~(((base + byte) << 8) | (byte_hi << 28) | (byte_lo ^ byte_hi))
 *
 * where byte_hi = byte >> 4 and byte_lo = byte & 0xF.  The low byte of the
 * stored value is a XOR-fold of the random byte, the top nibble repeats
 * byte_hi, and the middle bits hold (base + byte) shifted up by 8.
 *
 * On return r0-r4, r5 and r7 are unchanged (the original pushes/pops r0-r4
 * and never writes r5/r7).  Flags left for the caller: C = 0 (carry out of
 * the `lsl r3,#28` is bit 8 of the masked byte, always 0) and V = signed
 * overflow of `base + byte` (from the `adds r1,r1,r2`); N/Z come from the
 * final `mvn r1,r1`.
 */
Next storeObfuscatedRandomToken_804733C(void) {
    u32 in0 = R0, in1 = R1, in2 = R2, in3 = R3, in4 = R4;
    u32 base = R0;
    u32 obj = R5;

    /* 08047340: draw a random value until its low byte is nonzero */
    u32 byte;
    do {
        GUEST_CALL(GetPositiveSignedRNG);
        byte = R0 & 0xFF;
    } while (byte == 0);

    u32 hi = byte >> 4;
    u32 lo = (byte & 0xF) ^ hi;
    u32 sum = base + byte;
    u32 stored = ~((sum << 8) | (hi << 28) | lo);

    /* 08047362: str r1, [r5,#0x30] */
    ST32(obj + 0x30, stored);

    /* restore what the original's `pop {r0-r4,pc}` restores */
    R0 = in0; R1 = in1; R2 = in2; R3 = in3; R4 = in4; R5 = obj;

    /* flags: V from the adds, C from the later `lsl r3,#28` (always 0),
     * N/Z from the final `mvn r1,r1` */
    SET_FLAGS_ADD(base, byte);
    cpu.c = 0;
    SET_FLAGS_NZ(stored);
    RETURN_TO_CALLER();
}

/* ── checkScrambledCounter_8047366 ── */
/* checkScrambledCounter_8047366(r0 = expected value, r5 = record):
 * Validates a scrambled 32-bit field kept at record+0x30. The field is stored
 * bitwise-inverted (so the game can detect it being cleared to zero) and packed as
 *     bits  8..27 : the actual 20-bit counter
 *     bits 28..31 : a "high" nibble h
 *     bits  0..3  : a "low"  nibble l
 * The value compared against r0 is counter - (h * 16 + (l ^ h)).
 * Only the flags are meaningful to callers; r0/r1/r2/r4 must come back unchanged. */
Next checkScrambledCounter_8047366(void) {
    u32 expected = R0;
    u32 field = ~LD32(R5 + 0x30);

    u32 counter = (field << 4) >> 12;   /* bits 8..27 of the field */
    u32 highNibble = field >> 28;
    u32 lowNibble = field & 0xF;
    u32 tweak = (highNibble << 4) | (lowNibble ^ highNibble);

    SET_FLAGS_CMP(counter - tweak, expected);
    RETURN_TO_CALLER();
}

/* ── findRecordWithHalfwordAtOffset2_804743C ── */
/* findRecordWithHalfwordAtOffset2_804743C: scan R4 8-byte records starting at R7, comparing the halfword
 * at offset +2 of each against R0.  Flag result only: Z set if a match was
 * found (flag value 0), N/Z clear if the search ran out of records (flag
 * value 1).  R1 ends as the last halfword read (unchanged if R4 <= 0);
 * R3-R7 are preserved. */
Next findRecordWithHalfwordAtOffset2_804743C(void) {
    u32 target = R0;
    u32 record = R7;
    s32 count = (s32)R4;
    s32 i;
    u32 notFound = 1;

    for (i = 0; i < count; i++, record += 8) {
        R1 = LD16(record + 2);
        if (target == R1) {
            notFound = 0;
            break;
        }
    }

    SET_FLAGS_NZ(notFound);
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

/* ── sub_804758C ── */
/* sub_804758C: thin wrapper that tail-calls sub_80478D4 (all caller registers
 * are preserved, results come straight from the callee). */
Next sub_804758C(void) {
    GUEST_CALL(sub_80478D4);
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

/* ── stub_doNothingReturnZero_8047CA0 ── */
/* stub_doNothingReturnZero_8047CA0: stub that does nothing except return 0 in r0.
 * Sets the flags as `tst r0, r0` with r0 = 0 would (Z set, N clear);
 * r2-r7 are preserved. */
Next stub_doNothingReturnZero_8047CA0(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8047CB0 ── */
/* sub_8047CB0: a stub that always reports "no"/zero; callers branch on the
 * flags it leaves (Z set, N clear, as produced by `mov r0, #0`). */
Next sub_8047CB0(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── menu_clampListScrollAfterCountChange_8047CB6 ── */
/* menu_clampListScrollAfterCountChange_8047CB6(r5 = object):
 * Re-clamp the two u16 fields at +0x14 and +0x18 of the object at r5 after the
 * value at +0x12 changed: +0x14 is pulled down to value-1 (when value is 1..4)
 * and +0x18 is pulled down to value-5 (when value is 5 or more), each only if
 * it is currently above that bound. Callers only inspect the V flag, which
 * reflects the last comparison performed. */
Next menu_clampListScrollAfterCountChange_8047CB6(void) {
    u32 value = LD16(R5 + 0x12);

    if ((s32)value > 4) {
        /* Clamp +0x18 to at most value-5: the test is value < (+0x18)+5. */
        u32 limit = LD16(R5 + 0x18) + 5;
        SET_FLAGS_CMP(value, limit);
        if ((s32)value >= (s32)limit)
            RETURN_TO_CALLER();             /* already low enough */
        /* The subtract below is flag-setting in the original; V ends up clear. */
        SET_FLAGS_CMP(value, 5);
        ST16(R5 + 0x18, value - 5);
    } else {
        /* For small values the +0x14 check runs instead; the TST used to skip
         * the zero case leaves V untouched, so V still comes from this compare
         * when we return early. */
        SET_FLAGS_CMP(value, 4);
        if (value != 0) {
            u32 reduced = value - 1;
            u32 index = LD16(R5 + 0x14);
            SET_FLAGS_CMP(reduced, index);
            if ((s32)reduced <= (s32)index) /* only lower it, never raise it */
                ST16(R5 + 0x14, reduced);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_8047CDC ── */
/* sub_8047CDC(r1 = value): scale a value by 100 unless a mode flag says otherwise.
 * Loads the first word of the struct at [r5 + 0x1C]; when it equals 1 the value
 * is left untouched, otherwise it is multiplied by 100. All registers except
 * r1 are preserved for the caller. */
Next sub_8047CDC(void) {
    u32 modePtr = LD32(R5 + 0x1C);

    if (LD32(modePtr) != 1)
        R1 = R1 * 100;

    RETURN_TO_CALLER();
}

/* ── copy_table_80469F0_to_202F800_8047CEC ── */
/* copy_table_80469F0_to_202F800_8047CEC: copy the 0x168-word table at byte_80469F0 (ROM) into
 * the working buffer at byte_202F800 (RAM). */
Next copy_table_80469F0_to_202F800_8047CEC(void) {
    R0 = A_byte_80469F0;   /* source: word table in ROM */
    R1 = A_byte_202F800;   /* destination: EWRAM buffer */
    R2 = 0x168;            /* word count */
    GUEST_CALL(CopyWords);
    RETURN_TO_CALLER();
}

/* ── shop_resetStockTable_8048C68 ── */
/* shop_resetStockTable_8048C68:
 * (Re)initialize the shop data buffer owned by the toolkit: clear all 0xE80
 * bytes, then copy the default stock table from ROM over it. The table is a
 * run of 8-byte entries; an entry whose second byte is 0 is an empty slot and
 * is left zeroed instead of copied.
 *
 * inputs:  r2 r3 r10 (toolkit) lr
 * outputs: r2 (leftover byte count from the copy loop, negative on exit)
 * clobbers: r0 r1 r3 lr, flags
 */
Next shop_resetStockTable_8048C68(void) {
    u32 shopData = LD32(R10 + oToolkit_ShopDataPtr);

    R0 = shopData;
    R1 = 0xE80;
    GUEST_CALL(ZeroFillByWord);

    u32 src = A_byte_8047DA0;
    u32 dest = shopData;
    u32 remaining = 0xE80;

    for (;;) {
        remaining -= 8;
        if ((s32)remaining < 0)
            break;

        if (LD8(src + 1) != 0) {
            R0 = src;
            R1 = dest;
            R2 = 8;
            GUEST_CALL(CopyBytes);
        }

        src += 8;
        dest += 8;
    }

    R2 = remaining;
    RETURN_TO_CALLER();
}

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
