/*
 * asm03_1_0.c: readable functions of bn6f asm03_1_0.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── triggerSomeUpdateForInternetOrRealWorld_8033948 ── */
/* Field offsets used below that game.h does not define (from the disassembly):
 *   oGameState_MapGroup        = 4   (current map group inside oGameState)
 *   oS2011E30_Idx_04           = 4   (pending-update selector in the work block)
 *   oS2011E30_CurPETNaviMaxHP  = 8   (halfword: current PET navi's max HP)
 */
#define oGameState_MapGroup        4
#define oS2011E30_Idx_04           4
#define oS2011E30_CurPETNaviMaxHP  8

/* triggerSomeUpdateForInternetOrRealWorld_8033948:
 * Prepare the 16-byte work block at eS2011E30 for an area update. The block is
 * zeroed, then its update selector (offset 4) is set to 1 so that
 * dispatch_80339CC runs onUpdateInRealWorld_8033A96 on real-world maps or
 * onUpdateInInternet_8033B1E on internet maps; on internet maps the current PET
 * navi's max HP is also stored into the block first. Finally the block's
 * graphics transfer is performed.
 *
 * r2/r3 pass through untouched; r4-r7 are preserved (the original push/pops
 * them around the body, while its callees may clobber the copies it keeps in
 * r5/r7). Flags on return are whatever gfxTransfer_8033978 leaves behind. */
Next triggerSomeUpdateForInternetOrRealWorld_8033948(void) {
    u32 entryR4 = R4, entryR5 = R5, entryR6 = R6, entryR7 = R7;
    u32 block = A_eS2011E30;
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 mapGroup = LD8(gameState + oGameState_MapGroup);

    /* Zero the whole 16-byte work block. */
    R0 = block;
    R1 = 0x10;
    GUEST_CALL(ZeroFillByWord);

    if ((s32)mapGroup >= INTERNET_MAP_GROUP_START) {
        /* Internet area: record the current PET navi's max HP, then flag the
         * update so dispatch runs onUpdateInInternet_8033B1E. */
        GUEST_CALL(GetCurPETNavi);
        R5 = block;
        R7 = gameState;
        GUEST_CALL(GetMaxAndCurHPForCurPETNavi_80010D4);
        ST16(block + oS2011E30_CurPETNaviMaxHP, R0);
        R0 = 1;
        ST8(block + oS2011E30_Idx_04, R0);
        /* gfxTransfer_8033978 receives the block in r5 and the game state
         * pointer in r7 (the original keeps both live across the call). */
        R5 = block;
        R7 = gameState;
    } else {
        /* Real-world area: just flag the update so dispatch runs
         * onUpdateInRealWorld_8033A96. */
        R7 = 1;
        ST8(block + oS2011E30_Idx_04, R7);
        R5 = block;
    }

    /* Run the work block's graphics transfer. */
    GUEST_CALL(gfxTransfer_8033978);

    /* The original's push {r4-r7,lr} / pop {r4-r7,pc} restores these even
     * though the callees were free to clobber r5/r7. */
    R4 = entryR4;
    R5 = entryR5;
    R6 = entryR6;
    R7 = entryR7;
    RETURN_TO_CALLER();
}

/* ── callback_noop_8033A7C ── */
/* callback_noop_8033A7C: does nothing; exists only as a callable placeholder
 * (its callers treat it as an empty callback). */
Next callback_noop_8033A7C(void) {
    RETURN_TO_CALLER();
}

/* ── onUpdateInRealWorld_8033A96 ── */
/* onUpdateInRealWorld_8033A96(self: *S2011E30 in r5):
 * Per-frame update while in the real world: clear two per-frame state
 * flags, run the real-world update logic, then queue a graphics transfer. */
Next onUpdateInRealWorld_8033A96(void) {
    R0 = 0;
    GUEST_CALL(sub_8033B80);

    R0 = 0;
    GUEST_CALL(sub_8033F80);

    GUEST_CALL(sub_8033BE8);

    R0 = 0;
    GUEST_CALL(gfxTransfer_8033DA0);

    RETURN_TO_CALLER();
}

/* ── battle_resetState_8033AC4 ── */
/* battle_resetState_8033AC4: reset/initialize the battle object state (self in r5 = *S2011E30).
 * Runs the four initialization steps in order:
 *   1. sub_8033B80 with r0 = 0
 *   2. sub_8033F80 with r0 = 0
 *   3. sub_8033C68
 *   4. sub_8033BE8
 * Register state (r0-r3, flags) after the call is whatever sub_8033BE8 leaves. */
Next battle_resetState_8033AC4(void) {
    R0 = 0;
    GUEST_CALL(sub_8033B80);

    R0 = 0;
    GUEST_CALL(sub_8033F80);

    GUEST_CALL(sub_8033C68);
    GUEST_CALL(sub_8033BE8);

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

/* ── doPETEffect_8033fc0 ── */
/* doPETEffect_8033fc0(r0 = PET effect index):
 * Records the requested PET effect in the eS2011E30 state block and
 * clears its pending-effect byte. When the effect index is 3, also
 * plays the associated sound effect (0x9B).
 * Non-sound path: r0 = effect index, r1 = 0, flags from `cmp r0, #3`.
 * Sound path: r0/r1 and flags are whatever PlaySoundEffect leaves. */
Next doPETEffect_8033fc0(void) {
    u32 effect = R0;

    R1 = 0;
    ST8(A_eS2011E30 + 7, R1);      /* clear pending-effect flag */
    ST8(A_eS2011E30 + 4, effect);  /* store new effect index */

    if (effect == 3) {
        R0 = 0x9B;
        GUEST_CALL(PlaySoundEffect);
    } else {
        R0 = effect;
        SET_FLAGS_CMP(effect, 3);
    }
    RETURN_TO_CALLER();
}

/* ── clearWorkBuffer_2011A40_80341AA ── */
/* clearWorkBuffer_2011A40_80341AA: clear the 12-byte work buffer at byte_2011A40 (0x2011A40). */
Next clearWorkBuffer_2011A40_80341AA(void) {
    R0 = A_byte_2011A40;
    R1 = 0xC;
    GUEST_CALL(ZeroFillByWord); /* ZeroFillByWord(ptr, num_bytes) */
    RETURN_TO_CALLER();
}

/* ── transmission_BeginTransmissionScene_80341F8 ── */
/* transmission_BeginTransmissionScene_80341F8: prepare the transmission screen.
 * r5 = pointer to a state byte that is set to 4; also switches the LCD into a
 * fixed render mode, refreshes render info, and starts the transmission song.
 * Flag outputs are whatever the final PlayMusic call leaves behind. */
Next transmission_BeginTransmissionScene_80341F8(void) {
    ST8(R5, 4);

    R0 = 0x40; /* LCD control render info */
    GUEST_CALL(SetRenderInfoLCDControl);

    GUEST_CALL(renderInfo_8001788);
    GUEST_CALL(renderInfo_80017A0);

    R0 = SONG_TRANSMISSION;
    GUEST_CALL(PlayMusic);

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

/* ── sub_80342DC ── */
/* sub_80342DC(r5 = state byte to set):
 * Write 4 (phase "4") to the byte pointed at by r5; r0 = 4 and flags report a
 * nonzero result. */
Next sub_80342DC(void) {
    R0 = 4;
    ST8(R5, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_80342E4 ── */
/* sub_80342E4: store 8 to *r5 (e.g. mark a battle object's state/counter byte).
 * Outputs: r0 = 8, nz flags set from that value. */
Next sub_80342E4(void) {
    R0 = 8;
    ST8(R5, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── map_beginEnterMapOnWarpStep4_80342EC ── */
/* map_beginEnterMapOnWarpStep4_80342EC: begin the "enter map" flow (main jumptable step 4).
 * Marks the new jumptable index, wipes VRAM and the tile GFX pool,
 * programs the fade-in parameters for arriving on the new map, then
 * runs the warp-entry logic and the navi update step.
 * On entry r10 holds the toolkit pointer; it is copied to r7, which
 * the callees (map_triggerEnterMapOnWarp, navi_80340F6) read. */
Next map_beginEnterMapOnWarpStep4_80342EC(void) {
    R7 = R10;

    /* Main flow resumes at jumptable step 4. */
    u32 jumptableIndexPtr = LD32(R7); /* oToolkit_MainJumptableIndexPtr == 0 */
    ST8(jumptableIndexPtr, 4);

    /* Clear the video state before the new scene is built up. */
    GUEST_CALL(zeroFillVRAM);
    GUEST_CALL(ZeroFillGFX30025c0);

    /* Fade-in parameters for arriving on the map: param1 = 0, param2 = 0x10. */
    u32 gameState = LD32(R7 + oToolkit_GameStatePtr);
    ST8(gameState + oGameState_EnterMapFadeParam1, 0);
    ST8(gameState + oGameState_EnterMapFadeParam2, 0x10);

    /* map_triggerEnterMapOnWarp_8005C04(gameState, 0x10) */
    R0 = gameState;
    R1 = 0x10;
    GUEST_CALL(map_triggerEnterMapOnWarp_8005C04);

    /* navi_80340F6(gameState) — r0 is restored to the game state pointer
     * that the previous call may have clobbered. */
    R0 = gameState;
    GUEST_CALL(navi_80340F6);

    RETURN_TO_CALLER();
}

/* ── tryStartNotificationCutscene_8035054 ── */
/* tryStartNotificationCutscene_8035054: decide whether to start the "notification" cutscene.
 * Returns r0 = 0 if the cutscene was started, r0 = 1 if it was blocked. */
Next tryStartNotificationCutscene_8035054(void) {
    u32 blocked = 1;

    /* Only allowed while the player has normal control:
     * the slip must not be running and no cutscene may be active. */
    GUEST_CALL(IsNotSlipRunningAndNotInCutscene); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

        /* oGameState_Unk_12 being 0xFF blocks the notification. */
        if (LD8(gameState + oGameState_Unk_12) != 0xFF) {
            /* Event flag 0x171A requests the notification (text scripts set
             * it), passed here as the (high, low) immediate pair. The answer
             * is the Z flag: r0 is the flag's whole byte, nonzero whenever any
             * of its 8 flags is set. */
            R0 = 0x17;
            R1 = 0x1A; /* EVENT_171A */
            GUEST_CALL(TestEventFlagFromImmediate);
            if (cpu.nz != 0) {
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

/* ── cutscene_TryStartNotification_8035084 ── */
/* cutscene_TryStartNotification_8035084: show the "notification" cutscene.
 * Returns 1 if the cutscene was *not* started (Slip is running, a cutscene is
 * already active, or there is nothing to notify), 0 if it was started. */
Next cutscene_TryStartNotification_8035084(void) {
    u32 result;

    /* Bail out if the Slip effect is running or a cutscene is already active. */
    GUEST_CALL(IsNotSlipRunningAndNotInCutscene); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        /* Ask whether there is a pending notification; if so, start its
         * cutscene, passing the notification kind as the parameter. */
        GUEST_CALL(claimNextPendingRequest_8035756);
        if (R0 != 0) {
            R1 = R0;                                   /* notification parameter */
            R0 = A_CutsceneScriptNotification_8098BB8; /* CutsceneScript */
            GUEST_CALL(StartCutscene);
            result = 0;
        } else {
            result = 1;
        }
    } else {
        result = 1;
    }

    R0 = result;
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

/* ── initGameProgressBuffer_803532c ── */
/* initGameProgressBuffer_803532c:
 * Reset the player's game-progress flags. Copies the default values from the
 * progress-flag table at byte_8037694 (two-byte entries; only the low byte of
 * each entry is copied) into the working buffer at gameProgressBuffer_20010f0,
 * then clears the current game-progress state in the game state struct.
 * Returns r3 = 0x19 (number of entries copied); r4-r7 are preserved. */
Next initGameProgressBuffer_803532c(void) {
    u32 src = A_byte_8037694;
    u32 dst = A_gameProgressBuffer_20010f0;
    u32 i;

    for (i = 0; i < 0x19; i++) {
        ST8(dst, LD8(src));
        src += 2; /* skip the unused second byte of each table entry */
        dst += 1;
    }

    /* Mark that no game progress has been made yet */
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    ST8(gameState + 6 /* oGameState_GameProgress */, 0);

    R3 = i; /* 0x19 */
    RETURN_TO_CALLER();
}

/* ── clearSetFlags_80355a8 ── */
/* clearSetFlags_80355a8: clear the five event flags listed below and set EVENT_120.
 * r4-r7 (and r10) are preserved; r0 returns 0.
 * Event flags are addressed as (word index in r0, bit mask in r1). */

static void clear_event_flag(u32 word, u32 bits) {
    R0 = word;
    R1 = bits;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

static void set_event_flag(u32 word, u32 bits) {
    R0 = word;
    R1 = bits;
    GUEST_CALL(SetEventFlagFromImmediate);
}

Next clearSetFlags_80355a8(void) {
    clear_event_flag(6, 0x82);   /* EVENT_682 */
    clear_event_flag(8, 0x5F);   /* EVENT_85F */
    clear_event_flag(10, 0xBA);  /* EVENT_ABA */
    clear_event_flag(12, 0xDC);  /* EVENT_CDC */
    clear_event_flag(12, 0xE0);  /* EVENT_CE0 */

    set_event_flag(1, 0x20);     /* EVENT_120 */

    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── getIndexed20ByteTableEntry_80355DE ── */
/* getIndexed20ByteTableEntry_80355DE(r0 = entry index):
 * Return the address of entry `r0` in the table at byte_80984C8, whose entries
 * are 0x14 (20) bytes each. r0 = entry address, r1 = table base, r2 = byte offset.
 * Flags reflect the address computation (ADDS), as callers branch on c/v. */
Next getIndexed20ByteTableEntry_80355DE(void) {
    R2 = R0 * 0x14;                 /* offset of the entry within the table */
    R1 = A_byte_80984C8;            /* table base address */
    R0 = R1 + R2;                   /* address of the requested entry */
    SET_FLAGS_ADD(R1, R2);
    RETURN_TO_CALLER();
}

/* ── clearWorkBuffer_2000DC0_80356EC ── */
/* clearWorkBuffer_2000DC0_80356EC: zero-fill the 0x100-byte work buffer at dword_2000DC0 (word-wise). */
Next clearWorkBuffer_2000DC0_80356EC(void) {
    R0 = A_dword_2000DC0; /* buffer address */
    R1 = 0x100;           /* size in bytes */
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── timer_tickCountdownTable_8035738 ── */
/* timer_tickCountdownTable_8035738: tick the countdown table at dword_2000DC0.
 *
 * The table holds 32 entries of { timer, active }. For every entry whose
 * active flag is nonzero, the timer is decremented and written back. If an
 * active entry's timer is already zero the decrement would go negative:
 * processing stops right there and the decremented value is left in r3
 * without being stored.
 *
 * On return r3 holds the last value computed (the last entry's active flag,
 * or its decremented timer); r4-r7 are preserved. The flags are those of
 * the final subtract (early exit) or of the final loop compare r4 == 0x20
 * (normal exit). */
Next timer_tickCountdownTable_8035738(void) {
    u32 entry = A_dword_2000DC0;
    u32 i;

    for (i = 0; i < 0x20; i++, entry += 8) {
        u32 active = LD32(entry + 4);
        R3 = active;
        if (active != 0) {
            u32 timer = LD32(entry);
            R3 = timer - 1;
            if ((s32)R3 < 0) {
                /* timer underflow: bail out without writing it back;
                 * flags are those of the subtract (cmp timer, #1) */
                SET_FLAGS_CMP(timer, 1);
                RETURN_TO_CALLER();
            }
            ST32(entry, R3);
        }
    }

    /* normal exit: flags come from the final cmp r4, #0x20 (r4 == 0x20) */
    SET_FLAGS_CMP(0x20, 0x20);
    RETURN_TO_CALLER();
}

/* ── claimNextPendingRequest_8035756 ── */
/* claimNextPendingRequest_8035756:
 * Scan the 32-slot table at dword_2000DC0 (8 bytes per slot: +0 = slot's
 * current value, +4 = pending request) and claim the first slot that has a
 * request queued while the slot itself is free: the request is returned and
 * both fields are cleared. Slots with no pending request have their current
 * value cleared; busy slots are left untouched. Returns the claimed request,
 * or 0 if every slot was busy or empty. Flags reflect the returned value. */
Next claimNextPendingRequest_8035756(void) {
    u32 slot = A_dword_2000DC0;

    for (u32 i = 0; i < 0x20; i++, slot += 8) {
        u32 pending = LD32(slot + 4);
        if (pending == 0) {
            /* Nothing queued on this slot: release it. */
            ST32(slot, 0);
            R3 = 0;
            continue;
        }

        u32 current = LD32(slot);
        if (current != 0) {
            /* Slot is busy and still has a request pending: leave it alone. */
            R3 = current;
            continue;
        }

        /* Free slot with a pending request: take the request and clear the slot. */
        R0 = pending;
        R3 = 0;
        ST32(slot + 4, 0);
        ST32(slot, 0);
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    /* All 32 slots checked, none available. */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
