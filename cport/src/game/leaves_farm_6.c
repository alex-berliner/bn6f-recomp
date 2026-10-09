/*
 * leaves_farm_6.c: small functions from the model farm, set 6.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_8031BB2 ── */
/* sub_8031BB2 (r0, r5 = arguments passed through to the first helper):
 * text-engine routine that runs the standard three-step textbox setup:
 * initialise the textbox state, open its window box, then kick off the
 * text print. All further register results are simply whatever the last
 * called helper leaves behind (r2 is set to 8, r3 to 8, r4 to 0 and
 * r5 to 0x10 before the window call). */
Next sub_8031BB2(void) {
    GUEST_CALL(sub_803172E); /* initialise textbox state (uses r0, r5) */

    R2 = 8;
    R4 = 0;
    R3 = 8;
    R5 = 0x10;
    GUEST_CALL(sub_80315B8); /* open the message window box */

    GUEST_CALL(sub_8031B7C); /* start the text print */

    RETURN_TO_CALLER();
}

/* ── sub_8031BCA ── */
/* sub_8031BCA: text-box helper. Runs one step of the text engine, then
 * blits a window region (x=0/16, y=8 wide) and refreshes the box.
 * Inputs arrive in r0/r5 for the first callee; everything else is set up
 * here. All final register/flag state comes out of the last callee. */
Next sub_8031BCA(void) {
    /* Advance the text engine state (reads r0/r5, returns updated r0). */
    GUEST_CALL(sub_803172E);

    /* Blit parameters: source column 0, row 8, width 0x10, height 8.
     * Results come back in r2/r3/r4; r5 is preserved by the callee. */
    R2 = 0;
    R4 = 8;
    R3 = 0x10;
    R5 = 8;
    GUEST_CALL(sub_80315B8);

    /* Redraw/refresh the text box using the blit results in r2..r5. */
    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031BE2 ── */
/* sub_8031BE2: sequence of three setup/update calls.
 *
 * R0 and R5 arrive as arguments. The first callee (sub_803172E) is invoked
 * with them unchanged. Then fixed tile/map parameters are loaded into
 * r2-r5 (0, 0x10, 0xC, 0xC) for math_pointCrossHalfPlaneTest_8031594, and finally sub_8031B7C is
 * called with whatever registers math_pointCrossHalfPlaneTest_8031594 left behind (r0 and r5 are
 * carried through). All register and flag effects on return come from the
 * last callee, so nothing needs to be set explicitly here. */
Next sub_8031BE2(void) {
    GUEST_CALL(sub_803172E);

    R2 = 0;
    R4 = 0xC;
    R3 = 0x10;
    R5 = 0xC;

    GUEST_CALL(math_pointCrossHalfPlaneTest_8031594);
    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031BFA ── */
/* sub_8031BFA:
 * Runs sub_803172E on the values the caller left in r0/r5, then invokes
 * sub_8031570 with a fixed argument set (r2 = 0, r3 = r4 = 0x10, r5 = 0)
 * and finally sub_8031B7C. Register results and flags are whatever the
 * last callee leaves behind; lr is untouched (the original just pushes
 * lr and pops it back into pc). */
Next sub_8031BFA(void) {
    GUEST_CALL(sub_803172E);

    R2 = 0;
    R4 = 0x10;
    R3 = 0x10;
    R5 = 0;
    GUEST_CALL(sub_8031570);

    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── sub_8031C42 ── */
/* sub_8031C42(r0, r5): thin wrapper that chains three battle-helper calls.
 *
 * The caller's r0/r5 are passed straight through to sub_803172E; afterwards
 * a fixed parameter block (r2 = 0, r3 = 14, r4 = 14, r5 = 0) is set up for
 * math_pointLeftOfLineTest_80315DC, and whatever it leaves in r0/r2/r3/r4/r5 is handed on to
 * sub_8031B7C, whose results (registers and flags) become this function's
 * results. */
Next sub_8031C42(void) {
    GUEST_CALL(sub_803172E);

    R2 = 0;
    R3 = 0xE;
    R4 = 0xE;
    R5 = 0;

    GUEST_CALL(math_pointLeftOfLineTest_80315DC);
    GUEST_CALL(sub_8031B7C);

    RETURN_TO_CALLER();
}

/* ── FreeOWPlayerObject ── */
enum { oObjectHeader_ListIndex = 3 };
enum { oObjectHeader_Flags = 0 };

/* FreeOWPlayerObject(r5 = player object header):
 * Releases the overworld player object. Clears its slot bit
 * (0x80000000 >> listIndex) in the active-overworld-player bitmask,
 * clears the object's flags byte, and makes its sprite unscalable.
 * Leaves r0..r3 and the flags as sprite_makeUnscalable leaves them. */
Next FreeOWPlayerObject(void) {
    u32 obj = R5;

    /* push {lr}: the callee runs with SP four bytes below entry SP,
     * and the saved value becomes the final LR (pop {pc}). */
    u32 returnAddr = LR;
    SP -= 4;
    ST32(SP, returnAddr);

    /* Each active player object owns one bit of the shared bitmask,
     * selected by its position in the object list. A register LSR with
     * a count of 32 or more produces 0 on ARM, i.e. no bit is cleared. */
    u32 listIndex = LD8(obj + oObjectHeader_ListIndex);
    u32 bit = (listIndex < 32) ? (0x80000000u >> listIndex) : 0;

    u32 bitfieldAddr = A_eActiveOWPlayerObjectBitfield;
    u32 cleared = LD32(bitfieldAddr) & ~bit;
    ST32(bitfieldAddr, cleared);

    ST8(obj + oObjectHeader_Flags, 0);

    /* Match the register state the original has at the call site
     * (r0 = the shifted bit, r1 = 0 from the flags store,
     *  r2 = the new bitmask value). */
    R0 = bit;
    R1 = 0;
    R2 = cleared;
    GUEST_CALL(sprite_makeUnscalable);

    /* pop {pc} */
    returnAddr = LD32(SP);
    SP += 4;
    LR = returnAddr;

    RETURN_TO_CALLER();
}

/* ── FreeOverworldMapObject ── */
/* FreeOverworldMapObject(r5 = overworld map object):
 * Releases one overworld map object: clears its "active" bit in the global
 * active-objects bitfield, zeroes the object's first byte (its state/slot
 * marker), and tells the sprite system the object is no longer scalable.
 *
 * The bit for a slot is 0x80000000 rotated right by the slot's list index,
 * i.e. index 0 is bit 31 of word 0, index 1 is bit 30, ... index 32 is bit 31
 * of word 1, and so on. R0-R3 come back holding whatever sprite_makeUnscalable
 * left in them. R5 is preserved. */

/* The list-index field sits at offset 3 in the object struct; no named macro
 * for it exists in the headers, so spell the offset out. */
#define oOverworldMapObject_ListIndex 3

Next FreeOverworldMapObject(void) {
    u32 index = LD8(R5 + oOverworldMapObject_ListIndex);

    /* Mask with the single bit owned by this index. ROR by a multiple of 32
     * (including 0) leaves the mask unchanged. */
    u32 shift = index & 0xFF;
    u32 mask  = 0x80000000u;
    if (shift & 31)
        mask = (mask >> (shift & 31)) | (mask << (32 - (shift & 31)));

    u32 bitfield   = A_eActiveOverworldMapObjectsBitfield;
    u32 wordOffset = (index >> 5) << 2;
    u32 word       = LD32(bitfield + wordOffset) & ~mask;
    ST32(bitfield + wordOffset, word);

    ST8(R5, 0); /* object slot is now free */

    R0 = mask;
    R1 = 0;
    R2 = bitfield;
    R3 = word;
    GUEST_CALL(sprite_makeUnscalable);

    RETURN_TO_CALLER();
}

/* ── FreeOverworldNPCObject ── */
/* FreeOverworldNPCObject(r5 = overworld NPC object header):
 * Frees an overworld NPC's object slot: clears the NPC's bit in the
 * active-overworld-NPC bitfield (the slot number is encoded as a mask with
 * bit (31 - listIndex) set), clears the object's flags byte to mark it
 * unused, and tells the sprite system to stop scaling the NPC's sprite.
 * Ends in a call, so r0-r3 and the flags are whatever the callee leaves. */
Next FreeOverworldNPCObject(void) {
    /* Object header offset 3 holds the NPC's index in the object list. */
    u32 listIndex = LD8(R5 + 3);

    /* Slot mask: bit (31 - listIndex) of the shared bitfield. ARM LSR by a
     * register >= 32 yields 0, so out-of-range indices clear nothing. */
    u32 slotMask = (listIndex < 32) ? (0x80000000u >> listIndex) : 0;

    /* Clear this NPC's bit in the active-overworld-NPC bitfield. */
    u32 bitfieldAddr = A_eActiveOverworldNPCObjectsBitfield;
    u32 newBits = LD32(bitfieldAddr) & ~slotMask;
    ST32(bitfieldAddr, newBits);

    /* Mark the object header as unused. */
    ST8(R5, 0);

    /* Release the sprite's scaler state; the callee's register results and
     * flags are this function's results, so just pass the live values on. */
    R0 = slotMask;
    R1 = 0;
    R2 = newBits;
    GUEST_CALL(sprite_makeUnscalable);

    RETURN_TO_CALLER();
}

/* ── ZeroFillObjectInteractionAreas_800378C ── */
/* ZeroFillObjectInteractionAreas_800378C: clear the overworld object
 * interaction-area pool and reset the number of entries it holds.
 * Flags on return come from ZeroFillByWord's final flag-setting instruction. */
Next ZeroFillObjectInteractionAreas_800378C(void) {
    R0 = A_eOWObjectInteractionArea_0_X;
    R1 = 0x280; /* pool size in bytes */
    GUEST_CALL(ZeroFillByWord);

    ST32(A_eNumOWObjectInteractionAreas, 0);
    RETURN_TO_CALLER();
}

/* ── battle_isTimeStopPauseOrBattleFlags0x20_800a0a4 ── */
/* battle_isTimeStopPauseOrBattleFlags0x20_800a0a4:
 * Predicate consulted all over the battle engine to decide whether battle
 * time is currently standing still. It yields 1 when
 *   - battle_isTimeStop() reports an active time-stop, or
 *   - battle_isPaused() reports the game is NOT paused, or
 *   - battle_getFlags() has bit 0x20 set,
 * and 0 only when none of those hold.
 * r1 is left as the last callee left it on the early exits, and is 0x20 on
 * the battle_getFlags() path; callers rely on this. */
Next battle_isTimeStopPauseOrBattleFlags0x20_800a0a4(void) {
    GUEST_CALL(battle_isTimeStop);
    if (R0 != 0) {
        R0 = 1;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isPaused);
    if (R0 == 0) {
        R0 = 1;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_getFlags);
    R1 = 0x20;
    R0 = (R0 & 0x20) != 0;
    RETURN_TO_CALLER();
}

/* ── isSameSubsystem_800A732 ── */
/* isSameSubsystem_800A732: () -> !zf
 * Returns nonzero (Z clear) when the currently loaded subsystem counts as
 * "the same": either no subsystem is currently in use, or the subsystem
 * state byte at eStruct203F7D8+1 still has bit 1 set. On the path where the
 * subsystem is in use, r3 holds the struct address, r1 the state byte and
 * r2 the mask 2; otherwise those registers keep whatever
 * IsCurSubsystemInUse left in them. r4 is restored unchanged.
 *
 * Flag note: the original's final "mov r0, r4" is really an
 * "adds r0, r4, #0", which clears C and V; the following "tst r0, r0"
 * only refreshes N and Z. */
Next isSameSubsystem_800A732(void) {
    u32 same = 1; /* r4 = TRUE */

    GUEST_CALL(IsCurSubsystemInUse); /* -> R0 = bool, flags set */

    if (R0 != 0) {
        /* Subsystem is in use: "same" only while its state bit 1 is set. */
        R3 = A_eStruct203F7D8;
        R1 = LD8(A_eStruct203F7D8 + 1);
        R2 = 2;
        if (!(R1 & R2))
            same = 0; /* FALSE */
    }

    R0 = same;
    SET_FLAGS_ADD(same, 0); /* adds r0, r4, #0: N/Z from result, C = V = 0 */
    RETURN_TO_CALLER();
}

/* ── object_spawnType1 ── */
/* object_spawnType1(r0 = obj_idx, r1 = x, r2 = y, r3 = z, r4 = params)
 * -> r5 = spawned BattleObject (0 if the spawn failed).
 *
 * Spawns a battle object of type 1. The five word arguments are copied into
 * a context block on the stack, which SpawnBattleObjectCommon reads while
 * creating the object. If spawning succeeded, sub_8003400 runs the
 * type-1 specific initialisation on the new object (passed in r5). */
Next object_spawnType1(void) {
    /* Frame layout, matching the original push {r7,lr} + 0x14 byte block:
     *   ctx+0x00..0x10  the five spawn arguments (r0-r4)
     *   ctx+0x14        saved r7 slot
     *   ctx+0x18        saved lr slot */
    u32 ctx = SP -= 0x1C;

    u32 r7_in = R7;
    u32 objIdx = R0, x = R1, y = R2, z = R3, params = R4;

    ST32(ctx + 0x00, objIdx);
    ST32(ctx + 0x04, x);
    ST32(ctx + 0x08, y);
    ST32(ctx + 0x0C, z);
    ST32(ctx + 0x10, params);
    ST32(ctx + 0x14, r7_in);
    ST32(ctx + 0x18, LR);

    /* While the callees run, r7 points just past the argument block,
     * exactly as it does in the original. */
    R7 = ctx + 0x14;

    R0 = 1;              /* object type 1 */
    R1 = ctx;            /* spawn context block */
    GUEST_CALL(SpawnBattleObjectCommon);

    u32 obj = R5;
    if (obj != 0) {
        /* Type-1 specific initialisation of the new object in r5;
         * r0-r3 still hold SpawnBattleObjectCommon's results, as in the
         * original. */
        GUEST_CALL(sub_8003400);
    } else {
        /* Spawn failed: the original's `tst r5, r5` is the last flag
         * setting on this path. */
        SET_FLAGS_NZ(obj);
    }

    R7 = r7_in;
    SP += 0x1C;
    RETURN_TO_CALLER();
}

/* ── object_spawnType3 ── */
/* object_spawnType3(r0-r4 = spawn arguments for the new object):
 * Spawns a battle object of type 3. The five arguments (r0-r4) are handed to
 * SpawnBattleObjectCommon through a 5-word block on the stack; r7 points just
 * past that block while the callee runs. If an object was actually created,
 * its type-3 initializer (sub_8003400) runs immediately.
 * Returns the new object pointer in r5; flags come from the initializer when
 * one ran, otherwise from testing the pointer against zero. */
Next object_spawnType3(void) {
    u32 savedR7 = R7;

    /* Frame: 0x14 bytes of spawn arguments (r0-r4) at the bottom, the saved
     * r7/lr pair above them - the same layout the original pushes/stores. */
    SP -= 0x1C;
    u32 args = SP;
    ST32(args, R0);
    ST32(args + 4, R1);
    ST32(args + 8, R2);
    ST32(args + 12, R3);
    ST32(args + 16, R4);
    ST32(args + 20, savedR7);
    ST32(args + 24, LR);

    /* stmia r7! leaves r7 pointing past the argument block */
    R7 = args + 0x14;

    R0 = 3;                        /* object type 3 */
    R1 = args;                     /* argument block on the stack */
    GUEST_CALL(SpawnBattleObjectCommon);

    /* tst r5, r5: did the spawn succeed? */
    SET_FLAGS_NZ(R5);
    if (R5 != 0)
        GUEST_CALL(sub_8003400);   /* run the new object's initializer */

    SP += 0x1C;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── object_spawnType4 ── */
/* object_spawnType4(r0-r4 = spawn parameters):
 * Spawns a type-4 battle object. The five parameter words are copied into a
 * context block on the stack, which is what SpawnBattleObjectCommon expects
 * in r1. If an object was actually created (non-zero pointer in r5), its
 * initializer sub_8003400 runs immediately with the spawn results still in
 * the registers.
 * Returns the object pointer; r4, r6 and r7 are preserved. */
Next object_spawnType4(void) {
    u32 saved_r7 = R7;

    /* Frame layout as in the original: 8 bytes for the saved r7/lr pair,
     * then the 0x14-byte parameter block at the bottom. */
    u32 block = SP - 0x1C;
    SP = block;
    ST32(block + 0x14, saved_r7);
    ST32(block + 0x18, LR);

    /* stmia block!, {r0-r4}: pack the spawn parameters */
    ST32(block + 0x00, R0);
    ST32(block + 0x04, R1);
    ST32(block + 0x08, R2);
    ST32(block + 0x0C, R3);
    ST32(block + 0x10, R4);

    R0 = 4;             /* object type 4 */
    R1 = block;         /* -> parameter block on the stack */
    R7 = block + 0x14;  /* r7 ends up pointing past the block, as in the original */
    GUEST_CALL(SpawnBattleObjectCommon);

    if (R5 != 0) {
        /* Object created: run its initializer, which reads r0-r5 and r7. */
        GUEST_CALL(sub_8003400);
    } else {
        /* No object: hand the tst r5, r5 flags to the caller. */
        SET_FLAGS_NZ(R5);
    }

    R7 = saved_r7;      /* pop {r7} */
    SP += 0x1C;
    RETURN_TO_CALLER();
}

/* ── sub_80059B4 ── */
/* sub_80059B4 (self: * GameState in r5): on-map warp handler.
 *
 * Clears EVENT_1703, sets EVENT_1738, then plays the on-map warp cutscene
 * (CutsceneScriptOnMapWarp_8098a02) with parameter 0. Disabling the cutscene
 * call makes warps like the one in Lan's HP stop reacting, though they still
 * animate. */
enum { EVENT_1703 = 0x1703 };
enum { EVENT_1738 = 0x1738 };

Next sub_80059B4(void) {
    /* ClearEventFlagFromImmediate/SetEventFlagFromImmediate take the event id
     * split as (bank in r0, index in r1). */
    R0 = EVENT_1703 >> 8;
    R1 = EVENT_1703 & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);

    R0 = EVENT_1738 >> 8;
    R1 = EVENT_1738 & 0xFF;
    GUEST_CALL(SetEventFlagFromImmediate);

    R0 = A_CutsceneScriptOnMapWarp_8098a02;
    R1 = 0;
    GUEST_CALL(StartCutscene);

    RETURN_TO_CALLER();
}

/* ── sub_80059D0 ── */
/* sub_80059D0: in-map warp handler. Raises the two story event flags that
 * mark the warp event as started, then kicks off the "on in-map warp"
 * cutscene. Event flags live in word 0x17; this sets bits 3 and 0x38. */
Next sub_80059D0(void) {
    R0 = 0x17;
    R1 = 3;
    GUEST_CALL(SetEventFlagFromImmediate);   /* set EVENT_1703 */

    R0 = 0x17;
    R1 = 0x38;
    GUEST_CALL(SetEventFlagFromImmediate);   /* set EVENT_1738 */

    R0 = A_CutsceneScriptOnInMapWarp_8098a78;
    R1 = 0;
    GUEST_CALL(StartCutscene);

    RETURN_TO_CALLER();
}

/* ── sub_8006330 ── */
/* sub_8006330(r0 = fade slot index): stop the screen fade, then free the two
 * palettes (0x12, 0x13) used by the fade effect. r5 is preserved for callers. */
Next sub_8006330(void) {
    u32 savedR5 = R5;

    /* r5 = eScreenFade + index: point at this fade's control block */
    R5 = A_eScreenFade + R0;
    GUEST_CALL(screenFade_InitFadeBlock_800634C);

    /* release the two fade palettes owned by this slot */
    R0 = LD8(R5 + 0xC) + 0x12;
    GUEST_CALL(Terminate_ePalette20097a0_Transform);

    R0 = LD8(R5 + 0xC) + 0x13;
    GUEST_CALL(Terminate_ePalette20097a0_Transform);

    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sub_8007338 ── */
/* sub_8007338: reinitialize the camera.
 * Runs the camera's reset routine, then re-inits the camera at the origin
 * of map group 0xF0, map 0, and clears camera field unk03/14. */
Next sub_8007338(void) {
    /* Reset camera state. */
    GUEST_CALL(mapTiles_ResetDimensionsToDefault_80303E8);

    /* camera_init(player_x = 0, player_y = 0, player_z = 0,
     *             map_group = 0xF0, map_number = 0) */
    R0 = 0;
    R1 = 0;
    R2 = 0;
    R3 = 0xF0;
    R4 = 0;
    GUEST_CALL(camera_init_802FF4C);

    /* camera_writeUnk03_14(0) */
    R0 = 0;
    GUEST_CALL(camera_writeUnk03_14_80301b2);

    RETURN_TO_CALLER();
}

/* ── sub_80079D0 ── */
/* sub_80079D0: battle-end phase hook.
 *
 * Asks the battle-state manager (battle_doBothTrackedIdsMatch_800B46C) whether the battle is finished,
 * using the magic key 0x12345678 as the query token. If it reports "not done",
 * there is nothing to do. Otherwise the battle scene is torn down
 * (sub_800B460), all battle structs including the battle statistics are
 * copied back (battle_copyStructsIncludingBattleStats_800b2d8), and the
 * battle state's phase byte at offset 0x02 is set to 0x0C to trigger the
 * follow-up handler via sub_8007978.
 *
 * r5 = battle state struct. r4, r6, r7, r10 are passed through untouched.
 * Flags on return: N/Z always reflect R0 (0 on the early-exit path, 0x0C on
 * the success path); C/V are whatever the last called helper left. */
Next sub_80079D0(void) {
    R0 = 0x12345678;
    GUEST_CALL(battle_doBothTrackedIdsMatch_800B46C);

    if (R0 == 0) {
        /* Battle not finished yet: report "zero" to the caller. */
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(sub_800B460);
    GUEST_CALL(battle_copyStructsIncludingBattleStats_800b2d8);

    R0 = 0xC;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 2 /* oBattleState_Unk_02 */, R0);

    RETURN_TO_CALLER();
}

/* ── sub_80079F0 ── */
/* sub_80079F0: run one step of the "battle field reveal" sequence.
 * Draws the column pattern for the current battle field (r5 = battle state),
 * then advances the battle state script to state 8 with a cleared timer.
 * Returns with Z set (callers branch on it). */
Next sub_80079F0(void) {
    /* Ask which columns of the panel the effect should cover... */
    GUEST_CALL(GetBattlePanelColumnPattern);
    /* ...and apply that pattern to this battle field's layout (offset 6). */
    R1 = R0;
    R0 = LD8(R5 + 6);
    GUEST_CALL(sub_800BF88);

    /* Let the field effect play out. */
    GUEST_CALL(sub_800A3E4);

    /* Move the battle state to its next step (8, offset 1) and reset its
     * 16-bit timer (offset 2), leaving Z set as the original's MOV does. */
    ST8(R5 + 1, 8);
    R0 = 0;
    SET_FLAGS_NZ(R0);
    ST16(R5 + 2, R0);

    RETURN_TO_CALLER();
}

/* ── sub_80092A0 ── */
/* sub_80092A0: battle-state phase dispatcher.
 *
 * r5 points to the battle state; the byte at offset 2 selects an entry in
 * the handler table at off_80092B4 (entries at byte offsets 0, 4 and 8):
 *   0 -> sub_80092C0, 4 -> sub_80092F0, 8 -> sub_8009314.
 *
 * The original does `ldr r1, [table, phase]; mov lr, pc; bx r1`, so the
 * handler is entered with r0 = the phase byte and r1 = its own (Thumb)
 * entry address; r2-r12 pass through untouched. The handler's results
 * (registers and flags) flow straight back to our caller. */
Next sub_80092A0(void) {
    u32 phase = LD8(R5 + 2); /* oBattleState_Unk_02 */
    u32 handler = LD32(A_off_80092B4 + phase);

    R0 = phase;
    R1 = handler;

    switch (handler) {
    case 0x080092C1: /* sub_80092C0 + 1 (Thumb) */
        GUEST_CALL(sub_80092C0);
        break;
    case 0x080092F1: /* sub_80092F0 + 1 (Thumb) */
        GUEST_CALL(sub_80092F0);
        break;
    case 0x08009315: /* sub_8009314 + 1 (Thumb) */
        GUEST_CALL(sub_8009314);
        break;
    default:
        /* Out-of-range phase: the original would jump to whatever word the
         * table lookup produced; no valid game state reaches this. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_800945C ── */
/* sub_800945C: battle-state phase dispatcher.
 *
 * R5 points to the battle state; the phase byte at offset 2 selects one of
 * two handlers from the jump table at off_8009470. The phase byte is passed
 * to the handler in r0, and — exactly as the original's `ldr r1, [r1, r0]`
 * leaves it — r1 holds the selected table entry (the handler's Thumb
 * address, bit 0 set) when the handler starts.
 *
 * The original pushes lr before the call, so the handler runs with SP four
 * bytes lower than on entry; that slot has to exist here too. r2-r12 and
 * the incoming flags pass through to the handler untouched, and whatever
 * the handler leaves in the registers and flags is returned to our caller. */
Next sub_800945C(void) {
    u32 phase = LD8(R5 + 2);                /* ldrb r0, [r5,#2] */

    /* push {lr}: the handler observes the lowered SP */
    SP -= 4;
    ST32(SP, LR);

    R0 = phase;
    R1 = LD32(A_off_8009470 + phase * 4);   /* handler address | 1 */
    if (phase == 0)
        GUEST_CALL(sub_8009478);
    else
        GUEST_CALL(sub_80094B6);

    SP += 4;                                /* pop {pc}: return through LR */
    RETURN_TO_CALLER();
}

/* ── sub_800A892 ── */
/* sub_800A892:
 * Custom battle effect tick: if the active battle effects include the "custom
 * style" effect (0x100) but not the one at bit 3, and battle_getStateByteNibbles_800A832 reports
 * state 2, run the effect's main handler sub_8005DBE. */
Next sub_800A892(void) {
    u32 effects = 0;

    GUEST_CALL(GetBattleEffects);
    effects = R0;

    if ((effects & 0x100) != 0 && (effects & 8) == 0) {
        GUEST_CALL(battle_getStateByteNibbles_800A832);
        if (R0 == 2) {
            GUEST_CALL(sub_8005DBE);
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_800A97A ── */
/* sub_800A97A: check whether the battle is ending.
 * Returns 1 when GetBattleEffects reports the bit-3 effect *and* the battle
 * state's phase counter (byte 7) has reached 15; otherwise returns 0.
 * Flags on exit match the original's last comparison on each path. */
Next sub_800A97A(void) {
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    R1 = 8;
    if (effects & 8) {
        /* Effect bit is set: consult the battle state's phase counter. */
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        R3 = battleState;
        u32 phase = LD8(battleState + 7);
        R1 = phase;

        SET_FLAGS_CMP(phase, 0xF);
        if ((s32)phase >= 0xF) {
            R0 = 1;
            RETURN_TO_CALLER();
        }
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* Effect bit clear: phase counter is never consulted. */
    SET_FLAGS_NZ(effects & 8);
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── sub_800AA92 ── */
/* sub_800AA92: fetch a table pointer from getNextEntryAddress_800AAAE, then count how many
 * of its eight u32 entries are nonzero.
 * outputs: r0 = nonzero count, r1 = last word read, r2 = 0x20 (loop end),
 *          r3 = table pointer; c/v left as the final `cmp r2, #0x20` leaves
 *          them (c set, v clear). */
Next sub_800AA92(void) {
    GUEST_CALL(getNextEntryAddress_800AAAE);
    R3 = R0;

    u32 count = 0;
    for (u32 offset = 0; offset < 0x20; offset += 4) {
        R1 = LD32(R3 + offset);
        if (R1 != 0)
            count++;
    }

    R0 = count;
    R2 = 0x20;
    SET_FLAGS_CMP(R2, 0x20);
    RETURN_TO_CALLER();
}

/* ── sub_800AABC ── */
/* sub_800AABC: advance the battle state into phase 0x14.
 * Runs setTwoStructs_800A840(4), then sets the battle state's phase index
 * to 0x14 and zeroes its halfword at offset 2. r5 is preserved (the original
 * pushes/pops it); r0 = battle state pointer, r1 = 0, flags as by `mov r1, #0`. */
Next sub_800AABC(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    R0 = 4;
    GUEST_CALL(setTwoStructs_800A840);

    battle = LD32(R10 + oToolkit_BattleStatePtr);
    ST8(battle + 1, 0x14);   /* oBattleState_Index_01 */
    ST16(battle + 2, 0);     /* oBattleState_Unk_02_03 */

    R0 = battle;
    R1 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_800B428 ── */
/* sub_800B428:
 * Reinitializes the record whose pointer lives at dword_203CBE4 (via sub_802C8FA),
 * writes the marker word 0x1F2F3F4F to dword_203CBE0, then invokes sub_80200A4
 * with mode 4. sub_80200A4's r1-r3 results pass straight through to our caller. */
Next sub_800B428(void) {
    /* Reset/clear the structure at dword_203CBE4. */
    R0 = A_dword_203CBE4;
    GUEST_CALL(sub_802C8FA);

    /* Stamp the magic value into dword_203CBE0. */
    ST32(A_dword_203CBE0, 0x1F2F3F4F);

    /* Continue with operation mode 4; r1-r3 are left exactly as the callee set them. */
    R0 = 4;
    GUEST_CALL(sub_80200A4);

    RETURN_TO_CALLER();
}

/* ── sub_800B444 ── */
/* sub_800B444: reload the two saved 12-byte blocks into the working battle
 * data area (byte_203F4A4 -> unk_2035260 and byte_203F5A4 -> unk_203526C),
 * then run sub_802C9EA to act on the restored data. */
Next sub_800B444(void) {
    /* first block: 3 words */
    R0 = A_byte_203F4A4;
    R1 = A_unk_2035260;
    R2 = 0xC;
    GUEST_CALL(CopyWords);

    /* second block: 3 words */
    R0 = A_byte_203F5A4;
    R1 = A_unk_203526C;
    R2 = 0xC;
    GUEST_CALL(CopyWords);

    GUEST_CALL(sub_802C9EA);

    RETURN_TO_CALLER();
}

/* ── subsystem_launchShop ── */
/* subsystem_launchShop (0x08005D28):
 * Leave the current map and hand control over to the shop subsystem.
 * Called with the toolkit in r10; saves the current map group as the
 * "last map group" (so the shop can send the player back), refreshes the
 * player's game state, starts a fade-out, and queues the shop subsystem
 * (index 0x20) to be launched by the game state callback.
 *
 * Note: game.h only defines oGameState_LastMapGroup; the other two field
 * offsets are taken straight from the disassembly (MapGroup = +4,
 * SubsystemIndex = +0). */
Next subsystem_launchShop(void) {
    /* Run the pre-transition hook; it takes no arguments and its results
     * are unused, but it may trash r0-r3 like any ATPCS callee. */
    GUEST_CALL(sub_8046CC8);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    /* Remember the map group we are leaving so we can return to it later. */
    ST16(gameState + oGameState_LastMapGroup,
         LD16(gameState + 4 /* oGameState_MapGroup */));

    /* The state update and fade below run with r5 pointing at the game
     * state, matching the original; r5 itself is restored before returning. */
    u32 savedR5 = R5;
    R5 = gameState;

    GUEST_CALL(updatePlayerGameState_800107A);

    /* Begin a screen fade (fade kind 0xC, speed 0x10). */
    R0 = 0xC;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    /* Queue the shop subsystem; the game state handler picks it up.
     * r1 = 0x20 is also the value this function leaves behind. */
    R1 = 0x20;
    ST8(gameState + 0 /* oGameState_SubsystemIndex */, R1);

    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── zeroFill_8003AB2 ── */
/* zeroFill_8003AB2: clear the eStruct2001010 work block (0x48 bytes) once, then
 * flag it as initialized.
 *
 * The block is only zeroed while the "already filled" flag at +0x02 is still 0;
 * afterwards the flag at +0x03 is set so callers know the fill has happened.
 * Inputs come in via r4-r7 but are only read to satisfy the calling
 * convention; nothing the caller passes is actually used.
 * Leaves r1 = 0x48 (the size argument handed to ZeroFillByWord) and the
 * C/V flags untouched, matching the original's exit state. */
Next zeroFill_8003AB2(void) {
    u32 eStruct = A_eStruct2001010;

    if (LD8(eStruct + 2) == 0) {
        /* First run: wipe the whole block. */
        R0 = eStruct;
        R1 = 0x48;
        GUEST_CALL(ZeroFillByWord);
    }

    /* Mark the block as filled. */
    ST8(eStruct + 3, 1);

    RETURN_TO_CALLER();
}

/* ── HandleMegaManStatusMenu8126B4C ── */
/* HandleMegaManStatusMenu8126B4C: top-level dispatcher for the MegaMan
 * status menu. The menu object's state byte ([R5+1]) selects one of three
 * handlers from the jump table at off_8126B60.
 *
 * The state byte is stored as a byte offset into the table (0, 4 or 8),
 * since the original indexes it with an unscaled `ldr r0, [r0, r1]`.
 * The original tail-dispatches to the handler with LR set to its own
 * epilogue, so the handler's return goes straight back to this function's
 * caller; the table word it dispatched through is still in R0 and the raw
 * state byte is still in R1 when the handler starts. */
Next HandleMegaManStatusMenu8126B4C(void) {
    u32 state = LD8(R5 + 1);

    /* Load the table entry exactly as the original does (byte offset),
     * so R0 entering the handler is the dispatched address (Thumb bit set). */
    u32 entry = LD32(A_off_8126B60 + state);

    R0 = entry;
    R1 = state;

    switch (state) {
    case 0:
        GUEST_CALL(sub_8126B6C);   /* state 0 handler */
        break;
    case 4:
        GUEST_CALL(sub_8126CC0);   /* state 4 handler */
        break;
    case 8:
        GUEST_CALL(sub_8126FF0);   /* state 8 handler */
        break;
    default:
        /* Unreachable in the game: the handlers keep the state byte within
         * the table. The original would branch through a garbage word. */
        break;
    }

    RETURN_TO_CALLER();
}

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
 * Runs a check by calling sub_81190F8 with argument 0. If it reports
 * success, writes state 4 to the object's byte at offset 3 and calls
 * sub_8123D70 to advance it; otherwise returns immediately.
 * Flags on return are those left by whichever callee ran last. */
Next sub_8123678(void) {
    R0 = 0;
    GUEST_CALL(sub_81190F8);
    if (R0 != 0) {
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

/* ── sub_81245BC ── */
/* sub_81245BC: copy a 6x8 block of background tiles.
 *
 * Prepares the arguments for CopyBackgroundTiles (a leaf routine that takes
 * six arguments and may clobber the low registers, including r4/r5) and calls
 * it, preserving the caller's r4-r7 across the call. r1 and r2 are left as
 * CopyBackgroundTiles leaves them; the condition flags are likewise whatever
 * the callee's last flag-setting instruction produced. */
Next sub_81245BC(void) {
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

/* ── sub_812474C ── */
/* sub_812474C(r0 = script index):
 * Run one text script from the chatbox text archive (eTextScript201C100).
 * All register and flag outputs are just whatever chatbox_runScript leaves. */
Next sub_812474C(void) {
    u32 scriptIdx = R0;

    R1 = scriptIdx;                 /* archive in r0, script index in r1 */
    R0 = A_eTextScript201C100;
    GUEST_CALL(chatbox_runScript_803FD9C);

    RETURN_TO_CALLER();
}

/* ── sub_81264F4 ── */
/* sub_81264F4: fixed-argument wrapper around sub_8126570.
 *
 * It loads a constant argument set (two screen coordinates and two sizes in
 * r0-r3, a data table pointer in r4, a hardware-register mirror address in
 * r6, and a second table pointer in r7) and tail-calls sub_8126570, which
 * draws/updates something using them.
 *
 * r5 and r10 arrive from the caller and are forwarded untouched. Although
 * r4-r7 are overwritten to set up the call, the original pushes them and
 * pops them back afterwards, so from the caller's point of view r4-r7 are
 * preserved; only the callee's results in r2/r3 escape. */
Next sub_81264F4(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R0 = 0x5A;
    R1 = 0x20;
    R2 = 0x14;
    R3 = 0x0C;
    R4 = A_byte_8124A84;
    R6 = 0x40000000;
    R7 = A_dword_8124A94;

    GUEST_CALL(sub_8126570);

    /* pop {r4-r7} - restore the caller's register values */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── sub_812650C ── */
/* sub_812650C: parameter-filling wrapper around sub_8126570.
 * Sets up the four size arguments and the pointers to the byte_8124A84 /
 * dword_8124A94 data, invokes the worker, and restores its own saved
 * r4-r7 on the way out. r2/r3 (and the flags) come straight from the
 * callee; r0/r1 are scratch. */
Next sub_812650C(void) {
    /* The original push {r4-r7,lr} / pop {r4-r7,pc} preserves the
     * caller's r4-r7 across the call. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    R0 = 0x5A;              /* x extent  = 90 */
    R1 = 0x20;              /* y extent  = 32 */
    R2 = 0x1E;              /* width     = 30 */
    R3 = 0x0C;              /* height    = 12 */
    R4 = A_byte_8124A84;    /* pointer to the byte_8124A84 table */
    R6 = 0x40000000;        /* constant passed through to the worker */
    R7 = A_dword_8124A94;   /* pointer to the dword_8124A94 table */

    GUEST_CALL(sub_8126570);

    /* pop {r4-r7,pc} -- r4-r7 end exactly as they came in; r2/r3 are
     * whatever sub_8126570 left behind. */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8126524 ── */
/* sub_8126524: thin wrapper around sub_8126570.
 * Sets up fixed arguments for the callee: r0-r3 = constants (rect at 90,32,
 * size 40x12), r4 = pointer to byte_8124A84, r6 = flags 0x40000000,
 * r7 = pointer to dword_8124A94. r5 passes through untouched.
 * Like the original's push/pop of r4-r7, those registers are restored to
 * their incoming values after the call; r2-r3 remain whatever the callee
 * leaves behind (callers read them as its outputs). */
Next sub_8126524(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R0 = 0x5A;
    R1 = 0x20;
    R2 = 0x28;
    R3 = 0x0C;
    R4 = A_byte_8124A84;
    R6 = 0x40000000;
    R7 = A_dword_8124A94;

    GUEST_CALL(sub_8126570);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_8126554 ── */
/* sub_8126554: thin argument-setup wrapper around sub_8126570.
 * It loads a fixed set of arguments (counts/positions 90, 32, 60, 12 plus
 * pointers to the data tables byte_8124A84 and dword_8124A94, and the
 * 0x40000000 flag word), invokes sub_8126570, and hands its r1-r3 results
 * and flags straight back to the caller. r4-r7 are preserved for the
 * caller, exactly as the original's push/pop does. */
Next sub_8126554(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    R0 = 0x5A;
    R1 = 0x20;
    R2 = 0x3C;
    R3 = 0xC;
    R4 = A_byte_8124A84;
    R6 = 0x40000000;
    R7 = A_dword_8124A94;

    GUEST_CALL(sub_8126570);

    /* pop {r4-r7}: the callee's r4-r7 do not escape this wrapper */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;

    RETURN_TO_CALLER();
}

/* ── sub_81275A8 ── */
/* sub_81275A8(r5 = background/scene state ptr):
 * Redraws a background section: reads the page/screen index stored at +0xD of
 * the state struct, offsets it by 0x11 (its row in the tile map), and copies an
 * 11x8 block of 32x32-pixel tile blocks (block #3) from the tile-id table
 * byte_8126930 into the background.
 * r4-r7 are work registers of the caller and must be preserved. */
Next sub_81275A8(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    /* Top row (j) of the region to draw: screen index + 0x11. */
    s32 j = (s8)LD8(R5 + 0xD) + 0x11;

    R0 = (u32)j;              /* j: destination row */
    R1 = 3;                   /* i: destination column */
    R2 = 3;                   /* which 32x32 tile block to fill */
    R3 = A_byte_8126930;      /* tile ids */
    R4 = 0xB;                 /* j_size: 11 blocks tall */
    R5 = 8;                   /* i_size: 8 blocks wide */
    GUEST_CALL(CopyBackgroundTiles);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_8127618 ── */
/* sub_8127618(r5 = source struct): redraw a 3x3 block of background tiles.
 * The tile column offset is taken from the signed byte at [r5+0xD] and
 * biased by the base tile 0x23; the tile ids come from byte_81269E0.
 * r2/r3 are whatever CopyBackgroundTiles leaves behind; r4-r7 are restored. */
Next sub_8127618(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    /* first tile index: header byte + base tile 0x23 */
    u32 first_tile = (u32)((s8)LD8(R5 + 0xD)) + 0x23;

    R0 = first_tile;            /* j: starting tile index */
    R1 = 3;                     /* i: starting row */
    R2 = 3;                     /* which_tile_block_32x32 */
    R3 = A_byte_81269E0;        /* tileIds */
    R4 = 0xB;                   /* j_size */
    R5 = 6;                     /* i_size */
    R7 = saved_r5;              /* CopyBackgroundTiles also reads the source struct from r7 */
    GUEST_CALL(CopyBackgroundTiles);

    /* the caller's r4-r7 are preserved (the original pops them from the stack) */
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── sub_81277E0 ── */
/* sub_81277E0: redraw an 8x8 block of background tiles.
 *
 * r5 points to an object whose signed byte at offset 0x0D holds a tile index;
 * adding 0x31 turns it into the index of the first entry in the tile-id table
 * byte_8126A90. Those tile ids are copied into tile block 3 of the 32x32
 * background, starting at tile coordinate (j, 3) and spanning 8x8 tiles.
 *
 * r4-r7 are caller inputs that must come back unchanged, so they are saved
 * around the call (the original pushes/pops them). CopyBackgroundTiles
 * receives the block coordinates in r0-r5 and leaves r2 = 3 as observed by
 * callers. */
Next sub_81277E0(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* j: first tile to draw, from the object's tile index at offset 0x0D. */
    u32 firstTile = (u32)((s8)LD8(R5 + 0xD) + 0x31);

    R0 = firstTile;        /* j: starting tile index */
    R1 = 3;                /* i: starting tile row/column */
    R2 = 3;                /* which 32x32 tile block of the background */
    R3 = A_byte_8126A90;   /* tileIds: tile number table */
    R4 = 8;                /* j_size */
    R5 = 8;                /* i_size */
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore the caller's r4-r7 (the original pop {r4-r7,pc}). */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8127ED4 ── */
/* sub_8127ED4(r0 = text index, [r5+0xD] = variant flag byte):
 * Run one chatbox script from the fixed text archive eTextScript201BA00.
 * If bit 0 of the flag byte at [r5+0xD] is set, the script index is
 * advanced by 10, selecting the alternate variant of the text.
 * Returns with whatever r0-r3 the chatbox script runner leaves behind. */
Next sub_8127ED4(void) {
    u32 scriptIdx = R0;
    u32 variantFlags = LD8(R5 + 0xD);

    R1 = scriptIdx;
    R2 = variantFlags;
    R3 = 1;

    R0 = A_eTextScript201BA00;
    if (variantFlags & 1) {
        R1 = scriptIdx + 0xA;
        GUEST_CALL(chatbox_runScript_803FDA4);
    } else {
        GUEST_CALL(chatbox_runScript_803FD9C);
    }

    RETURN_TO_CALLER();
}

/* ── sub_81281C0 ── */
/* sub_81281C0(r4..r7, r10 = context):
 * If the halfword at [r5+0x1E] is nonzero, redraw one background region:
 * copy the 0x17x8 tile block of map layer j=5, i=3 from tile block 2,
 * using the tile id table at unk_2026700. Otherwise do nothing.
 * Flags, r0-r3 and r4-r7 are passed through to the caller as the original
 * leaves them (r4-r7 preserved across the call, r0-r3 come back from
 * CopyBackgroundTiles on the call path, r0 keeps the tested halfword on the
 * skip path). */
Next sub_81281C0(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    u32 enable = LD16(R5 + 0x1E);
    if (enable != 0) {
        R0 = 5;             /* j */
        R1 = 3;             /* i */
        R2 = 2;             /* tileBlock32x32 */
        R3 = A_unk_2026700; /* tileIds */
        R4 = 0x17;          /* j_size */
        R5 = 8;             /* i_size */
        GUEST_CALL(CopyBackgroundTiles);
        /* CopyBackgroundTiles clobbers r4/r5; the original push/pop restores them. */
        R4 = saved_r4;
        R5 = saved_r5;
    } else {
        /* tst r0, r0 was the last flag-setting instruction on this path. */
        R0 = enable;
        SET_FLAGS_NZ(enable);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8128260 ── */
/* sub_8128260: fill the 0x100-halfword buffer at unk_2026870 with the byte
 * stored at byte_812798A, promoted to a halfword pattern (byte << 8).
 * r3 is preserved; r2 still holds the fill value on return. */
Next sub_8128260(void) {
    R0 = A_unk_2026870;              /* destination buffer */
    R1 = 0x100;                      /* halfword count */
    R2 = (u32)LD8(A_byte_812798A) << 8; /* value byte, spread across the halfword */
    GUEST_CALL(HalfwordFill);
    RETURN_TO_CALLER();
}

/* ── sub_812AFB0 ── */
/* sub_812AFB0:
 * Fill a 30x20-tile screen area with tile IDs taken from the table at
 * unk_201FDA0, using tile block 1, then run the follow-up setup in
 * sub_812B4AC. The results sub_812B4AC leaves in r1-r3 (and the flags it
 * leaves behind) are passed on to the caller; r4-r7 are preserved. */
Next sub_812AFB0(void) {
    /* CopyBackgroundTiles may clobber r4/r5 (it receives 0x1E and 0x14 there),
     * and sub_812B4AC sees whatever it left behind — but this function's
     * callers expect the original r4-r7 back, so keep them aside. */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    /* CopyBackgroundTiles(j = 0, i = 0, tileBlock32x32 = 1,
     *                     tileIds = unk_201FDA0, j_size = 30, i_size = 20) */
    R0 = 0;
    R1 = 0;
    R2 = 1;
    R3 = A_unk_201FDA0;
    R4 = 0x1E;
    R5 = 0x14;
    GUEST_CALL(CopyBackgroundTiles);

    GUEST_CALL(sub_812B4AC);

    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
    R7 = saved_r7;
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

/* ── FillByEightWords ── */
/* FillByEightWords(r0 = destination address, r1 = length in bytes, r2 = fill word):
 * Fills memory via the BIOS CpuFastSet in fixed-source (fill) mode: the word in r2
 * is written to r0 for r1/4 words. The fill word is passed through a slot on the
 * guest stack because CpuFastSet takes its source by pointer. r0-r3 are preserved. */
Next FillByEightWords(void) {
    u32 dest = R0;
    u32 length = R1;
    u32 fill = R2;
    u32 savedR3 = R3;

    u32 fillOnStack = SP -= 4;
    ST32(fillOnStack, fill);

    R0 = fillOnStack;                    /* src: pointer to the fill word */
    R1 = dest;                           /* dst: address to fill */
    R2 = 0x1000000 | (length >> 2);      /* fixed-source mode, count in words */
    GUEST_CALL(SWI_CpuFastSet);

    SP += 4;

    R0 = dest;
    R1 = length;
    R2 = fill;
    R3 = savedR3;
    RETURN_TO_CALLER();
}

/* ── HalfwordFill ── */
/* HalfwordFill(r0 = dest, r1 = byte count, r2 = halfword value):
 * fill r1 bytes at dest with the halfword in r2 by handing CpuSet a
 * halfword-mode fill (0x1000000 | count/2). CpuSet reads the fill value
 * through a source pointer, so the value is parked on the stack for the
 * duration of the call.
 * r0-r3 are restored to their incoming values (the original pushes/pops
 * them); the returned flags are those of the Thumb "mov r1, r0" (LSLS
 * r1, r0, #0), i.e. the same NZCV as computing dest + 0. */

Next HalfwordFill(void) {
    u32 dest = R0;
    u32 byteCount = R1;
    u32 value = R2;
    u32 savedR3 = R3;

    u32 mode = 0x1000000u | (byteCount >> 1); /* halfword transfer, unit count = bytes/2 */

    u32 slot = SP -= 4;
    ST32(slot, value);

    R0 = slot;   /* src: pointer to the fill value */
    R1 = dest;
    R2 = mode;
    R3 = value;
    GUEST_CALL(SWI_CpuSet);

    SP = slot + 4;

    /* pop {r0-r3}: everything back as it arrived */
    R0 = dest;
    R1 = byteCount;
    R2 = value;
    R3 = savedR3;

    SET_FLAGS_ADD(dest, 0); /* flags of "mov r1, r0": n/z from dest, c/v cleared */

    RETURN_TO_CALLER();
}

/* ── PlayMusic ── */
/* PlayMusic (0x080005D4): switch the background music to the given song.
 * r0 = song id. If the requested song is already the current one, nothing
 * happens; otherwise the id is recorded in the game state and the sound
 * engine is told to start it. Song 0x63 (the "music stopped" slot) is
 * handled by a special routine, everything else goes through the M4A player.
 * r1-r7 are saved around the call because the sound routines clobber them;
 * the song comparison flags survive on the "already playing" path. */
Next PlayMusic(void) {
    u32 song = R0;

    /* Save r1-r7 and the return address, exactly like the original's
     * push {r1-r7,lr}, so the sound routines can trash them freely. */
    u32 frame = SP - 32;
    SP = frame;
    ST32(frame + 0,  R1);
    ST32(frame + 4,  R2);
    ST32(frame + 8,  R3);
    ST32(frame + 12, R4);
    ST32(frame + 16, R5);
    ST32(frame + 20, R6);
    ST32(frame + 24, R7);
    ST32(frame + 28, LR);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 currentSong = LD8(gameState + oGameState_BGMusicIndicator);

    if (song == currentSong) {
        /* Nothing to do; the flags from the song comparison survive. */
        SET_FLAGS_CMP(song, currentSong);
    } else {
        ST8(gameState + oGameState_BGMusicIndicator, song);

        /* Both sound routines expect r7 to point at the game state. */
        R7 = gameState;
        if (song == 0x63) {
            GUEST_CALL(sound_dispatchTask_8000630);
        } else {
            GUEST_CALL(m4a_800061E);
        }
    }

    /* pop {r1-r7}: restore the caller's registers across the call
     * (the pc pop is handled by RETURN_TO_CALLER below). */
    R1 = LD32(frame + 0);
    R2 = LD32(frame + 4);
    R3 = LD32(frame + 8);
    R4 = LD32(frame + 12);
    R5 = LD32(frame + 16);
    R6 = LD32(frame + 20);
    R7 = LD32(frame + 24);
    SP = frame + 32;

    RETURN_TO_CALLER();
}

/* ── SetDummyBGScrollCallbacks ── */
/* Field offsets within the BG scroll callback table (rooted at 0x200A880).
 * The lifter only exports the table's base symbol, so spell the strides out. */
#define oBGScrollCallbacks_Callback0      0
#define oBGScrollCallbacks_HBlankCallback 4
#define oBGScrollCallbacks_Callback1      8

/* SetDummyBGScrollCallbacks: install no-op handlers for the BG scroll engine
 * (both per-frame callbacks) and for HBlank, then point the HBlank interrupt
 * slot at the dummy handler. Used on screens that manage their own BG updates.
 * Outputs: r1 = HBlank dummy handler, r2 = callback table (as left by the
 * interrupt dispatcher), lr = return link of the SetInterruptCallback call. */
Next SetDummyBGScrollCallbacks(void) {
    u32 callbacks = A_eBGScrollCallbacks_Callback0;
    u32 bgDummy = A_nullsub_39 | 1;      /* Thumb entry addresses, hence |1 */
    u32 hblankDummy = A_nullsub_38 | 1;

    ST32(callbacks + oBGScrollCallbacks_Callback0, bgDummy);
    ST32(callbacks + oBGScrollCallbacks_Callback1, bgDummy);
    ST32(callbacks + oBGScrollCallbacks_HBlankCallback, hblankDummy);

    R0 = 4; /* interrupt slot: HBlank */
    R1 = hblankDummy;
    R2 = callbacks;
    GUEST_CALL(SetInterruptCallback);

    RETURN_TO_CALLER();
}
