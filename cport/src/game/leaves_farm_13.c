/*
 * leaves_farm_13.c: small functions from the model farm, set 13.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
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

/* ── map_setupEnterMapAfterWarp_803423C ── */
/* map_setupEnterMapAfterWarp_803423C: begin the enter-map sequence after a warp.
 * Switches the main jumptable to state 4, blanks video memory and the
 * shared GFX buffer, clears the map-entry fade parameter, then runs the
 * warp-landing handlers and the area music update. */
Next map_setupEnterMapAfterWarp_803423C(void) {
    /* The toolkit pointer lives in r10; the original also mirrors it into
     * r7 and leaves it there on return. */
    R7 = R10;

    /* Main jumptable index := 4 (enter-map state). The index pointer is
     * the first field of the toolkit struct, hence the plain load. */
    R0 = LD32(R10);
    R1 = 4;
    ST8(R0, R1);

    /* Wipe VRAM, the uncompressed GFX buffer at 0x30025C0, and reset the
     * sprite/OAM state so the new map's graphics can be loaded fresh. */
    GUEST_CALL(zeroFillVRAM);
    GUEST_CALL(ZeroFillGFX30025c0);
    GUEST_CALL(sub_8006910);

    /* The original re-establishes r7 = r10 here after the calls. */
    R7 = R10;

    /* Clear the fade parameter used while entering the new map. */
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    R0 = gameState;
    R1 = 0;
    ST8(R0 + oGameState_EnterMapFadeParam1, R1);

    /* Trigger everything that happens on arrival in the new map. */
    GUEST_CALL(map_triggerEnterMapOnWarp_8005C04);

    /* The warp handler leaves the game state pointer unchanged in r0;
     * the enter-map setup below is given it explicitly. */
    R0 = gameState;
    GUEST_CALL(sub_8033FDC);

    /* Update the area music for the new map; it receives whatever r0
     * the previous call returned, exactly as the original does. */
    GUEST_CALL(musicGameState_8000784);

    RETURN_TO_CALLER();
}

/* ── tryStartNotificationCutscene_8035054 ── */
/* tryStartNotificationCutscene_8035054: decide whether to start the "notification" cutscene.
 * Returns r0 = 0 if the cutscene was started, r0 = 1 if it was blocked. */
Next tryStartNotificationCutscene_8035054(void) {
    u32 blocked = 1;

    /* Only allowed while the player has normal control:
     * the slip must not be running and no cutscene may be active. */
    GUEST_CALL(IsNotSlipRunningAndNotInCutscene);
    if (R0 != 0) {
        u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

        /* oGameState_Unk_12 being 0xFF blocks the notification. */
        if (LD8(gameState + oGameState_Unk_12) != 0xFF) {
            /* The notification plays only once: it is guarded by event
             * flag 0x171A, passed here as the (high, low) immediate pair. */
            R0 = 0x17;
            R1 = 0x1A; /* EVENT_171A */
            GUEST_CALL(TestEventFlagFromImmediate);
            if (R0 != 0) {
                R0 = A_CutsceneScriptNotification_809AE68;
                /* r1 still holds what TestEventFlagFromImmediate left there;
                 * the original passes that value on as the cutscene parameter. */
                GUEST_CALL(StartCutscene);
                blocked = 0;
            }
        }
    }

    R0 = blocked;
    RETURN_TO_CALLER();
}

/* ── battle_dispatchObjectBehaviorStep_80A9B70 ── */
/* battle_dispatchObjectBehaviorStep_80A9B70: dispatch one battle-object behaviour step.
 *
 * r5 points to the object: r5+4 is a character id selecting a 0x10-byte
 * entry in byte_80A9AA0 (left in r7 for the handler), r5+8 is a byte
 * offset (0, 4 or 8) into the handler table at off_80A9B8C. The selected
 * handler pointer is loaded into r0 (the original does ldr r0,[r0,r1] /
 * mov lr,pc / bx r0, so the callee starts with r0 = its own entry point)
 * and invoked; all other registers pass through unchanged. Control
 * returns straight to this function's caller when the handler finishes,
 * with r0..r7 and the flags as the handler left them. */
Next battle_dispatchObjectBehaviorStep_80A9B70(void) {
    u32 handlerOffset = LD8(R5 + 8);

    R7 = A_byte_80A9AA0 + 0x10 * LD8(R5 + 4);
    SET_FLAGS_ADD(A_byte_80A9AA0, 0x10 * LD8(R5 + 4));

    R0 = LD32(A_off_80A9B8C + handlerOffset);

    switch (handlerOffset) {
    case 0: GUEST_CALL(sub_80A9B9C); break;
    case 4: GUEST_CALL(sub_80A9C26); break;
    case 8: GUEST_CALL(sub_80A9CA6); break;
    }

    RETURN_TO_CALLER();
}

/* ── palette_copyTableRow_80AA104 ── */
/* palette_copyTableRow_80AA104(r0 = row selector, 0..2):
 * Copies one 8-byte entry (a palette row: three colors plus terminator)
 * from the table byte_80AA11C into the buffer byte_20018B0, then
 * returns 0 with the flags set by "mov r0, #0" (N/Z clear, C/V unchanged).
 * r4-r7 are untouched by this routine and by CopyBytes. */
Next palette_copyTableRow_80AA104(void) {
    /* Each entry in byte_80AA11C is 8 bytes: R0 selects which one. */
    R0 = A_byte_80AA11C + (R0 << 3);
    R1 = A_byte_20018B0;
    R2 = 8;
    GUEST_CALL(CopyBytes); /* CopyBytes(src = R0, dest = R1, byteCount = R2) */

    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_initSpriteState3_80AA2F8 ── */
/* object_initSpriteState3_80AA2F8: sprite setup for an object whose struct pointer is in r5.
 * Stores the object's initial state fields (+0 = 3, +4 = 0, +8 = 4), loads
 * its sprite (gfx 0x80, palette 0x1C, sprite 0x98), resets the animation,
 * and finishes with the follow-up init in sub_80AA322. */
Next object_initSpriteState3_80AA2F8(void) {
    ST8(R5, 3);                       /* object state = 3 */

    R0 = 0x80;                        /* sprite_load(gfx, palette, sprite) */
    R1 = 0x1C;
    R2 = 0x98;
    GUEST_CALL(sprite_load);

    ST8(R5 + 4, 0);                   /* animation counter = 0 */
    R0 = 0;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    ST8(R5 + 8, 4);                   /* next state = 4 */
    R0 = 4;
    GUEST_CALL(sub_80AA322);

    RETURN_TO_CALLER();
}

/* ── sub_80AA8E0 ── */
/* sub_80AA8E0(r0-r5 = setup parameters, r10 = toolkit pointer):
 * Forwards a six-word parameter block to sub_80AA910 (which reads its
 * arguments back from the caller's stack at [sp, #n]), then returns
 *   r0 = value returned by sub_80AA910,
 *   r1 = the current battle state's oBattleState_Unk_36 field.
 * If r1 is 0 on entry, the helper is not called and (0, 0) is returned. */
Next sub_80AA8E0(void) {
    u32 setupResult = 0;
    u32 battleFlag = 0;

    if (R1 != 0) {
        u32 savedR7 = R7;

        /* Build the argument block on the stack for sub_80AA910.
         * The two trailing words are zeroed, as the original does. */
        u32 args = SP -= 0x2C;
        ST32(args + 0x00, R0);
        ST32(args + 0x04, R1);
        ST32(args + 0x08, R2);
        ST32(args + 0x0C, R3);
        ST32(args + 0x10, R4);
        ST32(args + 0x14, R5);
        ST32(args + 0x1C, 0);
        ST32(args + 0x20, 0);

        /* sub_80AA910 expects r7 = 0 (r0-r6, r8-r12 keep their incoming
         * values; it preserves the callee-saved ones itself). */
        R7 = 0;
        GUEST_CALL(sub_80AA910);
        SP += 0x2C;
        setupResult = R0;

        /* Read the current battle state's field 0x36 for the second result. */
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        battleFlag = LD16(battleState + oBattleState_Unk_36);

        /* r7 is outgoing-preserved, but the callee saw it as 0. */
        R7 = savedR7;
    }

    R0 = setupResult;
    R1 = battleFlag;
    RETURN_TO_CALLER();
}

/* ── getOffsetToQuantityOfChipCodeMaybe_8021c7c ── */
/* getOffsetToQuantityOfChipCodeMaybe_8021c7c(r0 = chip id, r1 = chip code):
 * Look up the chip's data (its four codes come first) and find the position of
 * code r1 among them, or 0 if the chip doesn't have that code.  Returns the
 * address of the matching entry in the per-chip table Toolkit.unk_2002230:
 *     table + 12*chipId + codeIndex
 * Leaves r0 = address, r1 = 12*chipId, r2 = chip id, r3 = code index.
 * r4 and r7 are preserved, as in the original. */
Next getOffsetToQuantityOfChipCodeMaybe_8021c7c(void) {
    u32 chipId = R0;
    u32 wantedCode = R1;

    GUEST_CALL(getChip8021DA8); /* R0 = pointer to the chip's data */
    u32 chipData = R0;

    /* Search the chip's four codes for the wanted one. */
    u32 codeIndex;
    for (codeIndex = 0; codeIndex < 4; codeIndex++) {
        if (LD8(chipData + codeIndex) == wantedCode)
            break;
    }
    if (codeIndex == 4)
        codeIndex = 0; /* code not found: fall back to the first slot */

    u32 table = LD32(R10 + oToolkit_Unk2002230_Ptr);
    u32 offset = 12 * chipId;

    R0 = table + offset + codeIndex;
    R1 = offset;
    R2 = chipId;
    R3 = codeIndex;
    RETURN_TO_CALLER();
}

/* ── sChip_loadRecordIntoSlot_8021AB4 ── */
/* sChip_loadRecordIntoSlot_8021AB4(r0 = source record, r1 = slot index):
 * Copies a 0x3C-unit record from r0 into slot r1 of the S-chip table whose
 * base pointer is stored at [r10 + oToolkit_S_Chip_2002178_Ptr] (slot r1
 * begins at base + r1 * 0x3C).  It then scans the copy's 30 halfwords: each
 * one is unpacked into two 9-bit values by split9BitsFromBitfield_8021AE0
 * and handed to sub_8021B2A together with a count of 1.
 * Exits with r2 = 1 and r3 = the result of the last sub_8021B2A call;
 * r4-r7 are preserved (the loop's r6/r7 live only inside this routine). */
Next sChip_loadRecordIntoSlot_8021AB4(void) {
    u32 src = R0;
    u32 slot = R1;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* Destination: slot `slot` of the S-chip table. */
    u32 dest = LD32(R10 + oToolkit_S_Chip_2002178_Ptr) + slot * 0x3C;

    /* CopyWords(src = r0 (unchanged), dest, 0x3C) */
    R1 = dest;
    R2 = 0x3C;
    GUEST_CALL(CopyWords);

    R7 = dest;
    for (R6 = 0; (s32)R6 < 0x3C; R6 += 2) {
        R0 = LD16(dest + R6);
        GUEST_CALL(split9BitsFromBitfield_8021AE0); /* -> r0, r1 */
        R2 = 1;
        GUEST_CALL(sub_8021B2A); /* -> r3 */
    }

    /* The original pops r6/r7 here, restoring the caller's values. */
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}
