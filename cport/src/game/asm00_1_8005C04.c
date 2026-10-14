/*
 * asm00_1_8005C04.c: readable functions of bn6f asm00_1.s, 0x08005C04-0x0800801C, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── map_triggerEnterMapOnWarp_8005C04 ── */
/* map_triggerEnterMapOnWarp_8005C04 — runs when the player finishes a warp:
 * tears down the old map, copies the selected warp table entry into the warp
 * work struct, saves the overworld player state on real-world <-> internet
 * crossings, picks the respawn position, and rewrites the GameState map id so
 * cbGameState_80050EC can enter the target map. */

#define WARP_TRANSITION_EVENT_FLAG 0x171B /* EVENT_171B: set while the warp animation plays */

/* register form: calls FreeAllObjectsOfSpecifiedTypes outside this file, which has no typed C interface yet */
static void free_all_objects_of_specified_types(u32 type_mask) {
    R0 = type_mask;
    GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);
}

/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void copy_words(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(CopyWords);
}

/* register form: calls map_lookupMapIdByGroupAndNumber_8001708 outside this file, which has no typed C interface yet */
static void map_lookup_map_id(u32 map_group, u32 map_number) {
    R0 = map_group;
    R1 = map_number;
    GUEST_CALL(map_lookupMapIdByGroupAndNumber_8001708);
}

/* register form: calls SetRenderInfoLCDControl outside this file, which has no typed C interface yet */
static void set_render_info_lcd_control(u32 lcd_control) {
    R0 = lcd_control;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool test_event_flag(u32 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* the callee clears Z when the flag is set */
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void clear_event_flag(u32 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* Values the original leaves behind in r0-r3 that no callee provides:
 * r0 = 0 (final movs), r1 = map group before the warp, r2 = previous
 * "last map group", r3 = destination Z chosen in .gotWarpInfo.
 * target_map_id is returned so the adapter can reproduce the C flag that
 * lsr r7, r6, #8 leaves behind. */
typedef struct {
    u32 target_map_id;
    u32 old_map_group;
    u32 old_last_map_group;
    u32 player_z;
} EnterMapOnWarpResult;

static EnterMapOnWarpResult trigger_enter_map_on_warp(u32 toolkit) {
    EnterMapOnWarpResult result;

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);

    /* the warp sequence owns this pointer until it is done */
    ST32(game_state + oGameState_Ptr_20, 0);

    /* remove the objects belonging to the map we are leaving (type 0x25) */
    free_all_objects_of_specified_types(0x25);

    /* copy the selected warp table entry (index-1) over the warp work struct */
    u32 warp = LD32(toolkit + oToolkit_Warp2011bb0_Ptr);
    u32 warp_entry = LD32(warp + oWarp2011bb0_WarpDataPtr)
        + (LD8(warp + oWarp2011bb0_WarpIndex) - 1) * oWarpData_Size;
    copy_words(warp_entry, warp, oWarpData_Size);

    /* resolve the incoming map and turn the LCD back on for the new area */
    game_state = LD32(toolkit + oToolkit_GameStatePtr);
    map_lookup_map_id(LD8(game_state + oGameState_MapGroup),
                      LD8(game_state + oGameState_MapNumber));
    set_render_info_lcd_control(0x40);

    warp = LD32(toolkit + oToolkit_Warp2011bb0_Ptr);
    game_state = LD32(toolkit + oToolkit_GameStatePtr);

    /* while the warp animation runs the player state was saved already,
     * so only remember where the overworld player stood on plain warps */
    if (!test_event_flag(WARP_TRANSITION_EVENT_FLAG)) {
        u32 warp_group = LD8(warp + oWarp2011bb0_MapGroup);
        u32 current_group = LD8(game_state + oGameState_MapGroup);
        if ((warp_group ^ current_group) & 0x80) {
            /* real world <-> internet crossing: the state we are leaving
             * behind is stored in the save block of the world we are in
             * (warping to the internet saves the real-world position) */
            u32 saved = game_state
                + ((warp_group & 0x80)
                       ? oGameState_SavedRealWorldCoords_FacingDirection_MapId
                       : oGameState_SavedInternetCoords_FacingDirection_MapId);
            u32 player = LD32(game_state + oGameState_OverworldPlayerObjectPtr);
            ST32(saved + oSavedOWPlayerInfo_X, LD32(player + oOWPlayerObject_X));
            ST32(saved + oSavedOWPlayerInfo_Y, LD32(player + oOWPlayerObject_Y));
            ST32(saved + oSavedOWPlayerInfo_Z, LD32(player + oOWPlayerObject_Z));
            ST32(saved + oSavedOWPlayerInfo_FacingDirection,
                 LD8(player + oOWPlayerObject_FacingDirection));
            ST32(saved + oSavedOWPlayerInfo_MapId,
                 (LD8(game_state + oGameState_MapNumber) << 8)
                     | LD8(game_state + oGameState_MapGroup));
        }
    }

    clear_event_flag(WARP_TRANSITION_EVENT_FLAG);

    /* where the player reappears depends on the kind of crossing */
    u32 transition_type = LD8(warp + oWarp2011bb0_MapGroupTransitionType);
    u32 x, y, z, facing_direction, target_map_id;
    if (transition_type == MAP_GROUP_TRANSITION_TYPE_INTERNET_TO_REAL_WORLD) {
        x = LD32(game_state + oGameState_SavedRealWorldX);
        y = LD32(game_state + oGameState_SavedRealWorldY);
        z = LD32(game_state + oGameState_SavedRealWorldZ);
        facing_direction = LD32(game_state + oGameState_SavedRealWorldFacingDirection);
        target_map_id = LD32(game_state + oGameState_SavedRealWorldMapId);
    } else if (transition_type == MAP_GROUP_TRANSITION_TYPE_REAL_WORLD_TO_INTERNET) {
        x = LD32(game_state + oGameState_SavedInternetX);
        y = LD32(game_state + oGameState_SavedInternetY);
        z = LD32(game_state + oGameState_SavedInternetZ);
        facing_direction = LD32(game_state + oGameState_SavedInternetFacingDirection);
        target_map_id = LD32(game_state + oGameState_SavedInternetMapId);
    } else {
        /* same-world warp: the (just copied) warp entry carries the destination */
        ST8(warp + oWarp2011bb0_Unk_10, 2);
        x = LD32(warp + oWarp2011bb0_X);
        y = LD32(warp + oWarp2011bb0_Y);
        z = LD32(warp + oWarp2011bb0_Z);
        facing_direction = LD8(warp + oWarp2011bb0_FacingDirection);
        target_map_id = LD16(warp + oWarp2011bb0_MapId);
    }

    ST32(game_state + oGameState_PlayerX, x);
    ST32(game_state + oGameState_PlayerY, y);
    ST32(game_state + oGameState_PlayerZ, z);
    ST32(game_state + oGameState_FacingDirectionAfterWarp, facing_direction);

    u32 target_group = target_map_id & 0xFF;
    u32 target_number = (target_map_id >> 8) & 0xFF;

    /* entering a map restarts its subsystem and records where we came from;
     * skipping the subsystem reset leaves a white screen after the warp */
    ST8(game_state + oGameState_SubsystemIndex, 0);
    ST8(game_state + oGameState_LastMapNumber, LD8(game_state + oGameState_MapNumber));
    u32 previous_group = LD8(game_state + oGameState_MapGroup);
    result.old_last_map_group = LD8(game_state + oGameState_LastMapGroup);
    ST8(game_state + oGameState_LastMapGroup, previous_group);
    ST8(game_state + oGameState_MapGroup, target_group);
    ST8(game_state + oGameState_MapNumber, target_number);

    /* reset the warp animation state */
    u32 warp_state = LD32(toolkit + oToolkit_S2001c04_Ptr);
    ST16(warp_state + oS2001c04_Unk_12, 0);
    ST16(warp_state + oS2001c04_Unk_14, 0);

    result.old_map_group = previous_group;
    result.player_z = z;
    result.target_map_id = target_map_id;
    return result;
}

/* map_triggerEnterMapOnWarp_8005C04() -> flags nz c v, r0-r3, preserves r4-r12/lr */
Next map_triggerEnterMapOnWarp_8005C04(void) {
    EnterMapOnWarpResult result = trigger_enter_map_on_warp(R10);
    R0 = 0;                                   /* final mov r0, #0 */
    R1 = result.old_map_group;                /* final r1: map group before the warp */
    R2 = result.old_last_map_group;           /* final r2: previous last map group */
    R3 = result.player_z;                     /* final r3: destination Z from .gotWarpInfo */
    /* lsr r7, r6, #8 leaves C = bit 7 of the target map id (V=0 from the cmps) */
    SET_FLAGS_CMP(0, 1 - ((result.target_map_id >> 7) & 1));
    /* the final movs r0, #0 leaves N clear and Z set */
    SET_FLAGS_NZ(0);
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
    GUEST_CALL(reqBBS_initRequestWorkArea_8046CC8);

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

/* ── battle_initFullSetup_8005DBE ── */
/* register form: calls FreeAllObjectsOfSpecifiedTypes outside this file, which has no typed C interface yet */
static void free_objects_of_type(u32 type_mask, u32 game_state) {
    R0 = type_mask;
    R5 = game_state;
    GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);
}

/* register form: calls resetAllGraphicsState_8005F40 outside this file, which has no typed C interface yet */
static void reset_all_graphics_state(void) {
    GUEST_CALL(resetAllGraphicsState_8005F40);
}

/* register form: calls runSetupStepAndClearWorkArea_8005F6C outside this file, which has no typed C interface yet */
static void run_setup_step_and_clear_work_area(void) {
    GUEST_CALL(runSetupStepAndClearWorkArea_8005F6C);
}

/* register form: calls sprite_initBattleFadeGfxState_80027C4 outside this file, which has no typed C interface yet */
static void call_sub_80027C4_sub_8005DBE(void) {
    GUEST_CALL(sprite_initBattleFadeGfxState_80027C4);
}

/* register form: calls zeroFill_80024A2 outside this file, which has no typed C interface yet */
static void zero_fill_80024A2(void) {
    GUEST_CALL(zeroFill_80024A2);
}

/* register form: calls scenario_initEffectState_8003962 outside this file, which has no typed C interface yet */
static void scenario_init_effect_state(void) {
    GUEST_CALL(scenario_initEffectState_8003962);
}

/* register form: calls zeroFill_8003AB2 outside this file, which has no typed C interface yet */
static void zero_fill_8003AB2(void) {
    GUEST_CALL(zeroFill_8003AB2);
}

/* register form: calls sub_803FB28 outside this file, which has no typed C interface yet */
static void call_sub_803FB28(void) {
    GUEST_CALL(sub_803FB28);
}

/* register form: calls reloadCurNaviStatBoosts_813c3ac outside this file, which has no typed C interface yet */
static void reload_cur_navi_stat_boosts(void) {
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);
}

/* Full battle-entry setup sequence: clear the secondary game-state pointer,
 * free all objects of type 0x25, then run each subsystem's init step in order. */
static void battle_setup_sequence(u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);

    /* Clear the auxiliary state pointer so later code knows it is not yet built. */
    ST32(game_state + oGameState_Ptr_20, 0);

    /* register form: save/restore R5 across the helper call; the original's
     * push {r4-r7,lr}/pop preserves R5 even though FreeAllObjectsOfSpecifiedTypes
     * may leave it at a different value. */
    u32 saved_r5 = R5;
    free_objects_of_type(0x25, game_state);
    R5 = saved_r5;

    reset_all_graphics_state();
    run_setup_step_and_clear_work_area();
    call_sub_80027C4_sub_8005DBE();
    zero_fill_80024A2();
    scenario_init_effect_state();
    zero_fill_8003AB2();
    call_sub_803FB28();
    reload_cur_navi_stat_boosts();
}

/* battle_initFullSetup_8005DBE(r10 = toolkit) -> preserves r4-r12 */
Next battle_initFullSetup_8005DBE(void) {
    battle_setup_sequence(R10);
    RETURN_TO_CALLER();
}

/* ── subsystem_launchMail ── */
/* subsystem_launchMail(r0 = ?, r10 = toolkit, lr = ret):
 *   inputs:  r0 r10 lr
 *   outputs: r2 r3 r4 r5
 *   clobbers: nz c v r0 r1 lr
 *   preserved: r4 r5
 *
 * Hand off the player's current overworld position to the mail subsystem:
 * copy X/Y/Z and facing from the overworld player object into the game
 * state, then request the mail subsystem (0x34) and a screen fade. Returns 0. */
Next subsystem_launchMail(void) {
    GUEST_CALL(sub_8127990);

    u32 toolkit = R10;
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 playerObj = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    /* mirror player position/heading into the game state */
    ST32(gameState + oGameState_PlayerX,      LD32(playerObj + 0x1C));
    ST32(gameState + oGameState_PlayerY,      LD32(playerObj + 0x20));
    ST32(gameState + oGameState_PlayerZ,      LD32(playerObj + 0x24));
    ST8 (gameState + oGameState_FacingDirectionAfterWarp, LD8(playerObj + 0x10));

    /* ask the game loop to switch to subsystem 0x34 (mail) */
    ST8(gameState + oGameState_SubsystemIndex, 0x34);

    /* fade the screen out (palette target 0xC, frames 0x10) */
    R0 = 0x0C;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── warp_ArmPendingTransition_8005EEC ── */
/* warp_ArmPendingTransition_8005EEC(r0 = warp data ptr, r1 = warp index, r2 = map group transition type,
 *              r10 = toolkit):
 * Arm the warp state machine: point it at the target warp's data, record which
 * warp within the group to load, and what kind of map-group transition to play. */
Next warp_ArmPendingTransition_8005EEC(void) {
    u32 warp = LD32(R10 + oToolkit_Warp2011bb0_Ptr);

    ST8(warp + oWarp2011bb0_Unk_10, 1);
    ST8(warp + oWarp2011bb0_WarpIndex, R1 + 1);
    ST32(warp + oWarp2011bb0_WarpDataPtr, R0);
    ST8(warp + oWarp2011bb0_MapGroupTransitionType, R2);

    RETURN_TO_CALLER();
}

/* ── warp_setSubsystemIndexTo0x10AndOthers_8005f00 ── */
/* warp_setSubsystemIndexTo0x10AndOthers_8005f00:
 * Prepares a warp: runs the pre-warp helper (fade-out etc., seeing the
 * caller's arguments unchanged), switches the game state's subsystem index
 * (first byte of the GameState struct) to 0x10 — the warp subsystem — and
 * then hands control to the subsystem runner with R0 = 0x10 and
 * R4 = game state. r4-r7 are preserved for the caller. */
Next warp_setSubsystemIndexTo0x10AndOthers_8005f00(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* pre-warp processing; runs with the caller's registers intact */
    GUEST_CALL(warp_ArmPendingTransition_8005EEC);

    /* point the game state at subsystem 0x10 */
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    R4 = gameState;
    R0 = 0x10;
    ST8(gameState, 0x10);   /* GameState->subsystemIndex (offset 0) */

    /* run the (now selected) subsystem */
    GUEST_CALL(timer_tickCountdownTable_8035738);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── warp_setSubsystemIndexTo0x14AndOthers_8005f14 ── */
/* warp_setSubsystemIndexTo0x14AndOthers_8005f14:
 * Warp-side hook. Runs the pre-warp routine, points the current game state
 * at subsystem 0x14 (the state the warp hands control to next), then runs
 * the follow-up routine. r4-r7 are preserved for the caller; r0-r3 come out
 * as timer_tickCountdownTable_8035738 left them.
 *
 * The pre-warp and follow-up routines are called through the guest, so the
 * saved registers are stashed in C locals (mirroring the original's
 * push {r4-r7,lr} / pop {r4-r7,pc}) rather than trusting the callees. */

/* GameState+0x00 holds the index of the subsystem the main loop should run
 * next. The disassembly names this field oGameState_SubsystemIndex; define
 * it locally if the shared header does not already provide it. */

Next warp_setSubsystemIndexTo0x14AndOthers_8005f14(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    /* pre-warp setup */
    GUEST_CALL(warp_ArmPendingTransition_8005EEC);

    /* queue subsystem 0x14 in the game state */
    R4 = LD32(R10 + oToolkit_GameStatePtr);
    R0 = 0x14;
    ST8(R4 + oGameState_SubsystemIndex, R0);

    /* hand control to the new subsystem */
    GUEST_CALL(timer_tickCountdownTable_8035738);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── warp_LoadCurrentAndReadUnk10_8005F28 ── */
/* warp_LoadCurrentAndReadUnk10_8005F28: load the warp helper struct for the current area (found via the
 * toolkit in r10) and fetch its byte at oWarp2011bb0_Unk_10.
 * Returns r0 = warp struct pointer, r1 = that byte; flags reflect (r1 - 0),
 * so callers typically test whether the byte is zero. */
Next warp_LoadCurrentAndReadUnk10_8005F28(void) {
    R0 = LD32(R10 + oToolkit_Warp2011bb0_Ptr);
    R1 = LD8(R0 + oWarp2011bb0_Unk_10);
    SET_FLAGS_CMP(R1, 0);
    RETURN_TO_CALLER();
}

/* ── warp_getCurrentWarpEntry_8005f32 ── */
/* warp_getCurrentWarpEntry_8005f32: fetch the current warp entry from the warp state (r10 = toolkit).
 * Returns r0 = pointer to the warp data table,
 *         r1 = warp index - 1 (index of the warp being transitioned out of),
 *         r2 = map group transition type. */
Next warp_getCurrentWarpEntry_8005f32(void) {
    u32 warpState = LD32(R10 + oToolkit_Warp2011bb0_Ptr);

    R0 = LD32(warpState + oWarp2011bb0_WarpDataPtr);
    R1 = LD8(warpState + oWarp2011bb0_WarpIndex) - 1;
    R2 = LD8(warpState + oWarp2011bb0_MapGroupTransitionType);

    RETURN_TO_CALLER();
}

/* ── resetAllGraphicsState_8005F40 ── */
/* resetAllGraphicsState_8005F40: reset all graphics state for a fresh scene.
 * Clears VRAM and the various work RAM buffers, installs the dummy
 * BG scroll callbacks, clears the extended palettes, and resets the
 * render info structures. Takes no arguments and returns none, but
 * r2-r7 pass through unchanged. */
Next resetAllGraphicsState_8005F40(void) {
    GUEST_CALL(zeroFillVRAM);                        /* blank out VRAM */
    GUEST_CALL(ZeroFill_byte_3001960);               /* clear sprite/OAM work area */
    GUEST_CALL(SetDummyBGScrollCallbacks);           /* no-op BG scroll until a scene sets real ones */
    GUEST_CALL(zeroFill_e20094C0);                    /* clear EWRAM graphics buffer */
    GUEST_CALL(palette_clearEPalette20097a0_80023A8); /* clear extended palette RAM */
    GUEST_CALL(zeroFill_e2009740);                    /* clear second EWRAM buffer */
    GUEST_CALL(zeroFill_e200F3A0);                    /* clear IWRAM buffer */
    GUEST_CALL(zeroFill_80024A2);                     /* clear misc render state */
    GUEST_CALL(renderInfo_8001788);                   /* reset render info block 1 */
    GUEST_CALL(renderInfo_80017A0);                   /* reset render info block 2 */
    RETURN_TO_CALLER();
}

/* ── runSetupStepAndClearWorkArea_8005F6C ── */
/* runSetupStepAndClearWorkArea_8005F6C: run the sub_802FDB0 setup step, then wipe the associated
 * work memory via cleareMemory_802FF2C. r2/r3 and the overflow flag are
 * whatever cleareMemory_802FF2C leaves behind (callers rely on this). */
Next runSetupStepAndClearWorkArea_8005F6C(void) {
    GUEST_CALL(sub_802FDB0);
    GUEST_CALL(cleareMemory_802FF2C);
    RETURN_TO_CALLER();
}

/* ── sub_8005F78 ── */
/* sub_8005F78: reset/clear routine — runs two memory-clearing helpers in
 * sequence and returns. r1-r3 and the V flag pass through from the last
 * callee; r4-r7 are preserved across the call. */
Next sub_8005F78(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(loc_802FDB4);
    GUEST_CALL(cleareMemory_802FF2C);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── resetGameStateToOpening_8005F84 ── */
/* register form: calls SetRenderInfoLCDControl outside this file, which has no typed C interface yet */
static void set_lcd_render_info(u16 lcd_control) {
    R0 = lcd_control;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* register form: calls npc_clearAllBusyFlags_809F68C outside this file, which has no typed C interface yet */
static void call_sub_809F68C(void) {
    GUEST_CALL(npc_clearAllBusyFlags_809F68C);
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void call_chatbox_8040818(void) {
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls chatbox_clear_eFlags2009F38 outside this file, which has no typed C interface yet */
static void clear_chatbox_flag_block(u32 flag_bits) {
    R0 = flag_bits;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
}

/* Tear the current scene down and send the game back to its opening state:
 * restore the LCD render defaults, release the effect/chatbox state, clear
 * the 0x40 chatbox event-flag block, then rewind the main jumptable to step 4
 * and reset the game state to kind 0, phase 8.
 *
 * Returns the phase byte (8) written last, which the original leaves in r1
 * on exit. r2/r3 and the c/v flags the caller sees are whatever
 * chatbox_clear_eFlags2009F38 (the last callee) left behind, exactly as in
 * the original; r4-r7 are untouched. */
static u32 reset_to_opening_state(u32 toolkit) {
    set_lcd_render_info(0x40);
    call_sub_809F68C();
    call_chatbox_8040818();
    clear_chatbox_flag_block(0x40);

    /* Main jumptable back to step 4. */
    u32 jumptable_index_ptr = LD32(toolkit + oToolkit_MainJumptableIndexPtr);
    ST8(jumptable_index_ptr, 4);

    /* Game state back to kind 0, phase 8. */
    u32 game_state_ptr = LD32(toolkit + oToolkit_GameStatePtr);
    ST8(game_state_ptr, 0);
    ST8(game_state_ptr + 0x16, 8);
    return 8;
}

/* resetGameStateToOpening_8005F84(r10 = toolkit) -> r1 = 8; r2 r3 c v left behind by
 * chatbox_clear_eFlags2009F38; r4-r7 preserved. */
Next resetGameStateToOpening_8005F84(void) {
    R1 = reset_to_opening_state(R10);
    RETURN_TO_CALLER();
}

/* ── screenFade_80062C8 ── */
/* screenFade_80062C8: read the current screen fade state.
 * eScreenFade is the fade controller struct; its +1 byte is the fade mode
 * (returned in r0) and its +3 byte is the current fade level (returned in r1). */
Next screenFade_80062C8(void) {
    u32 fade = A_eScreenFade;

    R1 = LD8(fade + 3); /* current fade intensity */
    R0 = LD8(fade + 1); /* fade mode/step */

    RETURN_TO_CALLER();
}

/* ── sub_80062D0 ── */
/* sub_80062D0: entry stub into the shared routine at loc_80062DA.
 *
 * It saves r5/lr with push {r5,lr} purely so that the shared routine's
 * epilogue (pop {r5,pc}) has matching values on the stack: r5 comes back
 * unchanged and the return goes to this function's caller. The only real
 * work here is setting r0 = 0 before branching; r3 and r5 pass through
 * untouched, everything else is scratch per the contract. */
Next sub_80062D0(void) {
    u32 ret = LR;

    /* push {r5,lr} — the shared epilogue pops these back */
    SP -= 8;
    ST32(SP, R5);
    ST32(SP + 4, ret);

    R0 = 0;
    GUEST_CALL(loc_80062DA);
    RETURN_TO_CALLER();
}

/* ── sub_80062EC ── */
/* sub_80062EC: run the two sub-steps sub_80062D0 and sub_80062D6 in order.
 * All caller-visible results (flags, r1, r2, r3, r5) come from the second call. */
Next sub_80062EC(void) {
    GUEST_CALL(sub_80062D0);
    GUEST_CALL(sub_80062D6);
    RETURN_TO_CALLER();
}

/* ── subsystem_triggerTransition_800630A ── */
/* subsystem_triggerTransition_800630A: tick the two screen-fade slots.
 * Each slot is 0x20 bytes starting at eScreenFade. If a slot is active
 * (byte 0 == 1), byte 1 selects a handler from the table at off_8005FB4,
 * which is entered with r1 = 1 and r2 = the selected index. */
Next subsystem_triggerTransition_800630A(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    for (s32 i = 0; i < 2; i++) {
        u32 slot = A_eScreenFade + i * 0x20;

        R1 = 1;
        if (LD8(slot) == 1) {
            u32 mode = LD8(slot + 1);
            u32 handler = LD32(A_off_8005FB4 + mode);

            R0 = handler;
            R2 = mode;
            R4 = i;
            R5 = slot;
            GUEST_CALL_ADDR(handler);
        }
    }

    R4 = saved_r4;
    R5 = saved_r5;
    SET_FLAGS_CMP(2, 2);
    RETURN_TO_CALLER();
}

/* ── screenFade_StopAndFreePalettes_8006330 ── */
/* screenFade_StopAndFreePalettes_8006330(r0 = fade slot index): stop the screen fade, then free the two
 * palettes (0x12, 0x13) used by the fade effect. r5 is preserved for callers. */
Next screenFade_StopAndFreePalettes_8006330(void) {
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

/* ── screenFade_InitFadeBlock_800634C ── */
/* screenFade_InitFadeBlock_800634C(r0 = fade block selector): initialize one screen-fade state block.
 *
 * The caller passes an offset into the eScreenFade array (block index * 0x10).
 * The high nibble of r0 is the fade's step count and is stored as the block's
 * speed field; the rest of the block's state bytes are cleared so the fade
 * starts over from the beginning.
 *
 * Outputs: r2 = r0 >> 4. r0 is zeroed and r5 is restored by the epilogue. */
Next screenFade_InitFadeBlock_800634C(void) {
    u32 savedR5 = R5;
    u32 sel = R0;

    u32 fade = A_eScreenFade + sel;

    R2 = sel >> 4;              /* fade step count */
    ST8(fade + 0xC, R2);        /* speed/steps field */

    ST8(fade + 3, 0);           /* frame counter */
    ST8(fade + 0, 0);           /* current step */
    ST8(fade + 2, 0);           /* fade direction/state */
    ST8(fade + 0xD, 0);         /* pending flag */

    R0 = 0;                     /* epilogue leaves r0 = 0 */
    R5 = savedR5;               /* pop {r5,pc} restores the caller's r5 */

    RETURN_TO_CALLER();
}

/* ── sub_8006366 ── */
/* Field layout of one entry in the eScreenFade array (the fade this routine steps). */
#define oScreenFade_Started     0x2   /* u8  - latched on the first update; the countdown starts the frame after */
#define oScreenFade_Step        0x4   /* u16 - amount taken off Counter on each update */
#define oScreenFade_Counter     0x6   /* u16 - fade time left; grey level = Counter >> 4 */
#define oScreenFade_Id          0x8   /* u16 - id handed to both palette repaint calls */
#define oScreenFade_EndCounter  0xA   /* u16 - Counter value at which the fade is finished */
#define oScreenFade_PalBase     0xC   /* u8  - base palette slot; repaints use PalBase+0x12 / +0x13 */
#define oScreenFade_Palette1    0x10  /* u32 - palette repainted through slot PalBase+0x12 */
#define oScreenFade_Palette2    0x14  /* u32 - palette repainted through slot PalBase+0x13 */
#define oScreenFade_RepaintArg1 0x18  /* u16 - extra argument of the first repaint call */
#define oScreenFade_RepaintArg2 0x1C  /* u16 - extra argument of the second repaint call */

/* register form: calls sub_8002378 outside this file, which has no typed C interface yet.
 * The grey colour travels in r1 and again in r6 (the original parks a copy in r6 across
 * the call); r4 and r6 are saved and restored so the update as a whole leaves them as it
 * found them, the way the original's push/pop does.  Whatever the callee leaves in r2/r3
 * stays there: on this path those are sub_8006366's results. */
static void repaint_fade_palette(u16 id, u32 color, u16 arg, u32 slot, u32 palette) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    R0 = id;
    R1 = color;
    R2 = arg;
    R3 = slot;
    R4 = palette;
    R6 = color;
    GUEST_CALL(sub_8002378);
    R4 = saved_r4;
    R6 = saved_r6;
}

/* register form: calls screenFade_StopAndFreePalettes_8006330 outside this file, which has
 * no typed C interface yet.  The original reaches the call with r1..r4 = new counter, end
 * counter, old Started flag, 1; the callee passes r3 (and r4) straight through, so r3 must
 * hold the old flag here — it is one of sub_8006366's results.  r4 is restored afterwards
 * as the original's pop does; the callee's r2 leftover is left in place as the other result. */
static void stop_screen_fade(u32 fade_offset, u16 counter, u16 end_counter, u8 started) {
    u32 saved_r4 = R4;
    R0 = fade_offset;
    R1 = counter;
    R2 = end_counter;
    R3 = started;
    R4 = 1;
    GUEST_CALL(screenFade_StopAndFreePalettes_8006330);
    R4 = saved_r4;
}

/* Advance one screen fade by a frame.
 *
 * The first update only latches Started.  From then on Counter ticks down by Step,
 * clamped at zero; while it is still above EndCounter the fade's two palettes are
 * repainted with a plain grey whose brightness follows the countdown, and once Counter
 * has reached EndCounter the fade is stopped and its palettes freed. */
static void screen_fade_update(u32 fade) {
    u16 counter = LD16(fade + oScreenFade_Counter);
    u16 step = LD16(fade + oScreenFade_Step);
    u8 started = LD8(fade + oScreenFade_Started);

    ST8(fade + oScreenFade_Started, 1);

    /* The frame the fade appears on does not tick down yet. */
    if (started) {
        if (counter >= step)
            counter -= step;
        else
            counter = 0;
    }
    ST16(fade + oScreenFade_Counter, counter);

    u16 end_counter = LD16(fade + oScreenFade_EndCounter);
    if (counter <= end_counter) {
        stop_screen_fade(fade - A_eScreenFade, counter, end_counter, started);
        return;
    }

    /* One grey for both palettes, with B = G = R = Counter >> 4. */
    u32 level = counter >> 4;
    u32 color = level | (level << 5) | (level << 10);

    repaint_fade_palette(LD16(fade + oScreenFade_Id), color,
                         LD16(fade + oScreenFade_RepaintArg1),
                         LD8(fade + oScreenFade_PalBase) + 0x12,
                         LD32(fade + oScreenFade_Palette1));
    repaint_fade_palette(LD16(fade + oScreenFade_Id), color,
                         LD16(fade + oScreenFade_RepaintArg2),
                         LD8(fade + oScreenFade_PalBase) + 0x13,
                         LD32(fade + oScreenFade_Palette2));
}

/* sub_8006366(r5 = fade entry): per-frame update of one screen fade.
 * r2/r3 end up holding what the last callee leaves behind (on the stop path r3 carries the
 * old Started flag through the callee), and r4/r5 keep their incoming values, so there
 * are no register results for the adapter to write. */
Next sub_8006366(void) {
    screen_fade_update(R5);
    RETURN_TO_CALLER();
}

/* ── sub_800647C ── */
/* sub_800647C (r5 = effect work area):
 * Adds the step size at +4 to the counter at +6, clamping it at the limit at
 * +0xA (clearing the "active" byte at +3 once the limit is reached). The top
 * bits of the counter are expanded into a 15-bit RGB color (the same shade in
 * every channel) and passed to two sprite/palette updates via sub_8002378.
 *
 * r4-r7 are callee-saved here (pushed/popped by the original), so they must
 * come out unchanged even though we use r4/r6 as call arguments. Note that r1
 * for the second call is read from r6 *after* the first call, exactly as the
 * original does — it relies on whatever sub_8002378 leaves in r6. */
Next sub_800647C(void) {
    u32 savedR4 = R4;
    u32 work = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 count = (u32)LD16(work + 6) + (u32)LD16(work + 4);
    u32 limit = LD16(work + 0xA);

    if ((s32)count >= (s32)limit) {
        /* effect finished: deactivate and hold the counter at the limit */
        ST8(work + 3, 0);
        count = limit;
    }
    ST16(work + 6, count);

    /* replicate the counter's intensity into R, G and B */
    u32 shade = count >> 4;
    R6 = shade | (shade << 5) | (shade << 10);

    R0 = LD16(work + 8);
    R1 = R6;
    R2 = LD16(work + 0x18);
    R3 = (u32)LD8(work + 0xC) + 0x12;
    R4 = LD32(work + 0x10);
    GUEST_CALL(sub_8002378);

    /* r1 for the second call is whatever the callee left in r6 */
    R0 = LD16(work + 8);
    R1 = R6;
    R2 = LD16(work + 0x1C);
    R3 = (u32)LD8(work + 0xC) + 0x13;
    R4 = LD32(work + 0x14);
    GUEST_CALL(sub_8002378);

    /* pop {r4-r7}: restore the caller's registers */
    R4 = savedR4;
    R5 = work;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── fade_tick_80064BE ── */
/*
 * Color-fade stepper (fade_tick_80064BE).
 *
 * The state block arrived in r5 describes a fade that repaints two palette
 * slots a little further every tick:
 *   +0x03 u8   running flag, cleared once the fade has finished
 *   +0x04 u16  step added to the position every tick
 *   +0x06 u16  current fade position
 *   +0x08 u16  first argument for the palette update call
 *   +0x0A u16  end position; reaching it ends the fade
 *   +0x0C u8   base palette slot number
 *   +0x10 u32 / +0x18 u16  description of the first palette slot to repaint
 *   +0x14 u32 / +0x1C u16  description of the second palette slot to repaint
 *
 * After the position is updated and the palettes repainted via sub_8002378
 * (slots base+0x12 and base+0x13), one of the two handlers in the table at
 * off_8006510 is invoked: sub_8006518 while the fade is still running
 * (table index 0), sub_8006580 once it finished (table index 4).
 */

/* register form: calls sub_8002378 outside this file, which has no typed C interface yet.
 * Besides its five arguments, the original keeps the fade color in r6 and the
 * handler-table index in r7 live across this call, so set those too. */
static void set_palette_color(u32 arg0, u32 color, u32 arg2, u32 slot, u32 arg4, u32 index) {
    R0 = arg0;
    R1 = color;
    R2 = arg2;
    R3 = slot;
    R4 = arg4;
    R6 = color;
    R7 = index;
    GUEST_CALL(sub_8002378);
}

/* register form: calls sub_8006518 outside this file, which has no typed C interface yet.
 * The original reaches it through the table at off_8006510 and enters it with the
 * fetched table entry in r0; r1-r7 still hold the values set up for sub_8002378
 * (r3 deliberately keeps whatever sub_8002378 left behind, as in the original). */
static void call_fade_step_handler(u32 handler) {
    R0 = handler;
    GUEST_CALL(sub_8006518);
}

/* register form: calls sub_8006580 outside this file, which has no typed C interface yet.
 * Same entry state as the step handler above. */
static void call_fade_end_handler(u32 handler) {
    R0 = handler;
    GUEST_CALL(sub_8006580);
}

static void fade_tick(u32 state) {
    u32 end = LD16(state + 0xA);
    u32 pos = LD16(state + 6) + LD16(state + 4);
    u32 index = 0;                    /* table entry of the "still running" handler */

    if ((s32)pos >= (s32)end) {
        ST8(state + 3, 0);            /* fade finished: clear the running flag ... */
        pos = end;                    /* ... and clamp the position at the end value */
        index = 4;                    /* table entry of the "finished" handler */
    }
    ST16(state + 6, pos);

    /* Broadcast the position's top 4 bits as one gray level to the R, G and B
     * halves of an RGB555 color. */
    u32 level = pos >> 4;
    u32 color = level | (level << 5) | (level << 10);

    set_palette_color(LD16(state + 8), color, LD16(state + 0x18),
                      LD8(state + 0xC) + 0x12, LD32(state + 0x10), index);
    set_palette_color(LD16(state + 8), color, LD16(state + 0x1C),
                      LD8(state + 0xC) + 0x13, LD32(state + 0x14), index);

    u32 handler = LD32(A_off_8006510 + index);
    if (index == 0)
        call_fade_step_handler(handler);
    else
        call_fade_end_handler(handler);
}

/* fade_tick_80064BE(r5 = fade state block) */
Next fade_tick_80064BE(void) {
    /* The original body runs between push {r4-r7,lr} / pop {r4-r7,pc}: the calls
     * inside clobber r4/r6/r7, but the caller's copies must survive.  r0-r3 and
     * the condition flags are whatever the dispatched handler leaves behind, so
     * the adapter does not write them. */
    u32 r4 = R4, r6 = R6, r7 = R7;
    fade_tick(R5);
    R4 = r4;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── sub_8006580 ── */
/* sub_8006580: tear down the transform/cross-over overlay effect.
 * Frees the two hardware palettes it grabbed (0x14, 0x15), clears its
 * global state, and restores the render info of the toolkit's current
 * screen: clears the 0x40 "effect window" bits from the three halfword
 * fields at +6/+8/+0xA and zeroes the halfword at +2.
 * Inputs: R5 = effect state object, R10 = toolkit. Outputs: R2 = 0x40. */
Next sub_8006580(void) {
    /* Release both hardware palettes used by the transform sprites. */
    R0 = 0x14;
    GUEST_CALL(Terminate_ePalette20097a0_Transform);
    R0 = 0x15;
    GUEST_CALL(Terminate_ePalette20097a0_Transform);

    /* Clear the effect's global state word and the byte at state+0xD. */
    ST32(A_dword_20096D0, 0);
    ST8(R5 + 0xD, 0);

    /* Restore the render info of the screen the toolkit is showing. */
    u32 renderInfo = LD32(R10 + oToolkit_RenderInfoPtr);
    const u32 clearBit = ~0x40;

    ST16(renderInfo + 6, LD16(renderInfo + 6) & clearBit);
    ST16(renderInfo + 8, LD16(renderInfo + 8) & clearBit);
    ST16(renderInfo + 0xA, LD16(renderInfo + 0xA) & clearBit);
    ST16(renderInfo + 2, 0);

    /* The original leaves r2 = 0x40 behind for its caller. */
    R2 = 0x40;
    RETURN_TO_CALLER();
}

/* ── sub_8006910 ── */
/* sub_8006910: store 0x80 (a "busy/locked" flag) into the byte at 20081B0.
 * Also leaves r0 = 0x80 and r1 = the byte's address, with NZ flags set
 * from r0 (non-zero, positive) as the MOV does. */
Next sub_8006910(void) {
    R1 = A_byte_20081B0;
    R0 = 0x80;
    SET_FLAGS_NZ(R0);
    ST8(R1, R0);
    RETURN_TO_CALLER();
}

/* ── SetPrimaryToolkitPointers ── */
/* CopyWords helper — the game's word-copy routine has no typed C interface yet. */
/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void copy_words_SetPrimaryToolkitPointers(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(CopyWords);
}

/* Bytes copied: ToolkitPointersEnd - ToolkitPointers (15 pointer entries). */
#define TOOLKIT_POINTERS_SIZE_SetPrimaryToolkitPointers 0x3C

/* Copy the primary toolkit pointer table (joypad, render info, camera, battle
 * state, chatbox, ...) from ROM into eToolkit.  This re-points every global
 * the toolkit uses at the start of the game's boot sequence.
 *
 * Returns the value the original leaves in r0: it pops its saved LR back off
 * the stack into r0 and returns through it, so r0 ends up holding this
 * function's own return address.  r1/r2 and the nz/c flags are simply left as
 * CopyWords left them. */
static u32 set_primary_toolkit_pointers(u32 saved_return_address) {
    copy_words_SetPrimaryToolkitPointers(A_ToolkitPointers, A_eToolkit, TOOLKIT_POINTERS_SIZE_SetPrimaryToolkitPointers);
    return saved_return_address;
}

/* SetPrimaryToolkitPointers() -> ()
 * Also leaves r10 pointing at eToolkit; callers rely on that as the frame
 * pointer base for all toolkit globals.  LR is passed in because the original
 * hands it back in r0. */
Next SetPrimaryToolkitPointers(void) {
    R0 = set_primary_toolkit_pointers(LR);
    R10 = A_eToolkit;
    RETURN_TO_CALLER();
}

/* ── SetPrimaryToolkitPointersWrapper ── */
/* SetPrimaryToolkitPointersWrapper: 08006C18 THUMB
 * A trivial trampoline: it just tail-calls SetPrimaryToolkitPointers, which
 * sets up the primary toolkit pointers (leaving its results in r2/r3/r10),
 * and then returns straight to whoever called this wrapper. r3 passes
 * through untouched. */
Next SetPrimaryToolkitPointersWrapper(void) {
    GUEST_CALL(SetPrimaryToolkitPointers);
    RETURN_TO_CALLER();
}

/* ── RandomizeExtraToolkitPointers ── */
/* Layout constants from the disassembly. */
#define TOOLKIT_POINTERS_SIZE_RandomizeExtraToolkitPointers   0x3C   /* ToolkitPointersEnd - ToolkitPointers */
#define TOOLKIT_EXTRA_MEM_SIZE   0x35BC /* ToolkitExtraPtrsMemorySize */

/* The original compares the counter after adding 4, so the last pass runs at
 * index 0x54 and the counter is left at 0x58 when the loop exits. */
#define EXTRA_PTR_LOOP_BOUND     0x58   /* ToolkitExtraPtrsOffsetsEnd - ToolkitExtraPtrsOffsets + 4 */

/* Register values the original leaves behind in r0-r3, plus the loop counter
 * its final CMP uses (that counter is also what ends up in r1). */
typedef struct {
    u32 return_address; /* r0: the original pops its saved lr into r0 before bx r0 */
    u32 loop_index;     /* r1: loop counter left at the loop bound */
    u32 block_base;     /* r2: eGameState + stored offset */
    u32 offsets_table;  /* r3: &ToolkitExtraPtrsOffsets */
} ExtraToolkitPtrsExit;

/* register form: calls GetRNG outside this file, which has no typed C interface yet */
static u32 get_rng_RandomizeExtraToolkitPointers(void) {
    GUEST_CALL(GetRNG);
    return R0;
}

/* register form: calls copyWords_80014EC outside this file, which has no typed C interface yet */
static void copy_words_RandomizeExtraToolkitPointers(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(copyWords_80014EC);
}

/* Refill the extra toolkit pointer slots at the end of the ToolkitPointers
 * table in eGameState. The "randomization" is disabled: both the old stored
 * offset and the freshly drawn RNG offset are ANDed with 0, so the offset
 * saved to eUnusedExtraToolkitPtrsOffset is always 0 and the block copy is a
 * self-copy of eGameState. GetRNG still runs, so the RNG state advances
 * exactly as in the original. */
static void randomize_extra_toolkit_pointers(u32 return_address, u32 toolkit_pointers,
                                             ExtraToolkitPtrsExit *out) {
    u32 new_offset = get_rng_RandomizeExtraToolkitPointers() & 0u;                              /* drawn offset, masked to zero */
    u32 old_offset = LD32(A_eUnusedExtraToolkitPtrsOffset) & 0u;  /* read, then masked to zero */

    ST32(A_eUnusedExtraToolkitPtrsOffset, new_offset);

    copy_words_RandomizeExtraToolkitPointers(A_eToolkitExtraPtrsMemory + old_offset, A_eToolkitExtraPtrsMemory + new_offset, TOOLKIT_EXTRA_MEM_SIZE);

    u32 ptr_table  = toolkit_pointers + TOOLKIT_POINTERS_SIZE_RandomizeExtraToolkitPointers;
    u32 block_base = A_eToolkitExtraPtrsMemory + new_offset;

    u32 index = 0;
    while (index < EXTRA_PTR_LOOP_BOUND) {
        u32 offset = LD32(A_ToolkitExtraPtrsOffsets + index);
        ST32(ptr_table + index, block_base + offset);
        index += 4;
    }

    out->return_address = return_address;
    out->loop_index     = index;
    out->block_base     = block_base;
    out->offsets_table  = A_ToolkitExtraPtrsOffsets;
}

/* RandomizeExtraToolkitPointers: rewrites the extra toolkit pointers after a
 * game-state reload. r10 holds the start of the ToolkitPointers table.
 * The original exits with "pop {r0}; bx r0", so r0 ends up holding the saved
 * return address (the entry lr) rather than anything the body computed. */
Next RandomizeExtraToolkitPointers(void) {
    ExtraToolkitPtrsExit out;
    randomize_extra_toolkit_pointers(LR, R10, &out);
    R0 = out.return_address;
    R1 = out.loop_index;
    R2 = out.block_base;
    R3 = out.offsets_table;
    SET_FLAGS_CMP(out.loop_index, EXTRA_PTR_LOOP_BOUND);
    RETURN_TO_CALLER();
}

/* ── SetExtraToolkitPointers ── */
/* ToolkitExtraPtrsOffsetsEnd - ToolkitExtraPtrsOffsets + 4: byte size of the offset table
 * (0x58 = 22 entries * 4 bytes). */
#define TOOLKIT_EXTRA_PTRS_TABLE_SIZE 0x58

/* Copies the toolkit's extra pointer table into the toolkit's GameStatePtr block.
 * For each word in ToolkitExtraPtrsOffsets, reads a word-sized offset, adds the
 * base of the extra memory, and stores the resulting absolute address into the
 * toolkit struct.
 *
 * Iterates over 22 entries (0x58 / 4 = 22) by stepping the byte index by 4 while
 * it is below 0x58.
 *
 * Inputs:  toolkit       = value of r10 on entry (the toolkit struct pointer).
 *          index         = value of r1 on entry (loop counter, starts at 0).
 *          base          = value of r2 on entry.
 *          offsets_addr  = value of r3 on entry.
 * Outputs: r1 = final index (after the last +4, i.e. 0x58).
 *          r2 = base unchanged.
 *          r3 = offsets_addr unchanged.
 *          r4..r7 unchanged.
 * Other regs are local to the typed function. */
static void set_extra_toolkit_pointers(u32 toolkit, u32 index, u32 base,
                                       u32 offsets_addr) {
    u32 dst = toolkit + oToolkit_GameStatePtr;
    u32 off = index;
    while ((s32)off < (s32)TOOLKIT_EXTRA_PTRS_TABLE_SIZE) {
        ST32(dst + off, LD32(offsets_addr + off) + base);
        off += 4;
    }
}

/* register form: calls no outside function; reads RAM constants via LD32 on
 * the addresses that the original's ADR/ldr pseudo-ops resolve to. */
static u32 derive_base(void) {
    u32 r3 = LD32(A_eUnusedExtraToolkitPtrsOffset);
    /* Original: r4 = 0; r3 &= r4 -> r3 = 0. */
    return A_eToolkitExtraPtrsMemory + (r3 & 0);
}

Next SetExtraToolkitPointers(void) {
    set_extra_toolkit_pointers(R10, 0, derive_base(), A_ToolkitExtraPtrsOffsets);
    R1 = TOOLKIT_EXTRA_PTRS_TABLE_SIZE;
    R2 = derive_base();
    R3 = A_ToolkitExtraPtrsOffsets;
    RETURN_TO_CALLER();
}

/* ── encryption_initAll_8006d00 ── */
/* ── helpers for external game functions ── */

/* register form: calls GetRNG outside this file, which has no typed C interface yet */
static u32 get_rng(void) {
    GUEST_CALL(GetRNG);
    return R0;
}

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word(u32 addr, u32 num_bytes) {
    R0 = addr;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls encryption_zenny_8006f54 outside this file, which has no typed C interface yet */
static void init_zenny_encryption(void) {
    GUEST_CALL(encryption_zenny_8006f54);
}

/* register form: calls encryption_bugfrags_8006fac outside this file, which has no typed C interface yet */
static void init_bugfrags_encryption(void) {
    GUEST_CALL(encryption_bugfrags_8006fac);
}

/* register form: calls anticheat_modCardsInit_8007036 outside this file, which has no typed C interface yet */
static void init_modcards_anticheat(void) {
    GUEST_CALL(anticheat_modCardsInit_8007036);
}

/* ── internal helpers ── */

/* Fill a byte array with random XOR-keyed values, storing the bitwise complement
 * in the corresponding toolkit buffer.  Retries each byte until the masked/XORed
 * result is non-zero.  Iterates from count-1 down to 0 to match RNG call order. */
static void fill_encryption_bytes(u32 toolkit, u32 field_off,
                                  u32 target, u32 count, u32 xor_key) {
    u32 buffer = LD32(toolkit + field_off);
    for (u32 i = count; i-- > 0; ) {
        u32 val;
        do {
            val = (get_rng() & 0xFF) ^ xor_key;
        } while (val == 0);
        ST8(target + i, val);
        ST8(buffer + i, ~val);
    }
}

/* Produce a non-zero 12-bit-random XOR encryption key. */
static u32 make_encryption_key(u32 xor_const) {
    u32 val;
    do {
        val = ((get_rng() << 12) >> 12) ^ xor_const;
    } while (val == 0);
    return val;
}

/* ── main logic ── */

/* Initialize all encryption subsystems:
 *   - zero and seed the toolkit extra-block pointers area
 *   - fill three byte-arrays (item names, pet names, short strings) with
 *     keyed random data mirrored into toolkit buffers
 *   - derive Zenny and BugFrag obfuscation keys, then run their inits
 *   - run the mod-cards anti-cheat initialization */
static void encryption_init_all(u32 toolkit) {
    u32 ptrs_addr = A_eUnusedExtraToolkitPtrsOffset;

    /* Clear the extra-block pointer table and seed the RNG slot. */
    zero_fill_by_word(ptrs_addr, 0x80);
    ST32(ptrs_addr, 0);
    ST32(ptrs_addr + 4, get_rng());

    /* Fill 0x200-byte item-name table (XOR 0x6F) and 0x200-byte pet table (XOR 0x81). */
    fill_encryption_bytes(toolkit, oToolkit_Unk2004a8c_Ptr,
                          A_byte_20004E0, 0x200, 0x6F);
    fill_encryption_bytes(toolkit, oToolkit_Unk2004c20_Ptr,
                          A_byte_20008A0, 0x200, 0x81);

    /* Fill 4-byte short-string table (XOR 0xFE). */
    fill_encryption_bytes(toolkit, oToolkit_Unk2004e24_Ptr,
                          A_byte_2001600, 4, 0xFE);

    /* Zenny obfuscation key → initialize Zenny encryption state. */
    ST32(A_dword_2000060, make_encryption_key(0x802412C));
    init_zenny_encryption();

    /* BugFrag obfuscation key → initialize BugFrag encryption state. */
    ST32(A_dword_20018B8, make_encryption_key(0x803ED90));
    init_bugfrags_encryption();

    /* Mod-card anti-cheat tables. */
    init_modcards_anticheat();
}

/* ── thin adapter ── */

/* encryption_initAll_8006d00(r10 = toolkit ptr)
 * Outputs r2-r7 are pass-through (r4-r7 restored via callee-saved convention,
 * r2-r3 never written).  No flag outputs. */
Next encryption_initAll_8006d00(void) {
    encryption_init_all(R10);
    RETURN_TO_CALLER();
}

/* ── gameState_GetUnk74_8006DEC ── */
/* gameState_GetUnk74_8006DEC(): return the current game state's field at oGameState_Unk_74.
 * Inputs: none (R10 = toolkit pointer).
 * Outputs: r0 = game state -> Unk_74. Preserves r4-r7. */
Next gameState_GetUnk74_8006DEC(void) {
    u32 *toolkit = (u32 *)R10;
    u32 *gameState = (u32 *)LD32((u32)toolkit + oToolkit_GameStatePtr);
    R0 = LD32((u32)gameState + oGameState_Unk_74);
    RETURN_TO_CALLER();
}

/* ── encryption_xorBufferWithKey_8006df6 ── */
/* Key slot used by the buffer encryption routines: the literal at
 * eUnusedExtraToolkitPtrsOffset+4 holds the XOR key
 * (dword_2001064 in RAM, kept as an offset from 0x2001060). */
#define ENCRYPTION_KEY_SLOT (A_eUnusedExtraToolkitPtrsOffset + 4)

/* encryption_xorBufferWithKey_8006df6(r0 = buffer, r1 = byte count):
 * XOR every byte of the buffer with the current cipher key, walking from
 * the last byte down to the first, then write the key back to its slot.
 * The buffer is transformed in place (encrypt and decrypt are the same
 * operation for XOR). */
Next encryption_xorBufferWithKey_8006df6(void) {
    u32 buf = R0;
    s32 i = (s32)R1 - 1;
    u32 key = LD32(ENCRYPTION_KEY_SLOT);

    do {
        ST8(buf + i, LD8(buf + i) ^ key);
        i--;
    } while (i >= 0);

    ST32(ENCRYPTION_KEY_SLOT, key);
    RETURN_TO_CALLER();
}

/* ── encryption_save_memSetFlags_8006E0E ── */
/* encryption_save_memSetFlags_8006E0E(r0 = save buffer, r1 = byte count):
 * Toggle the save data's obfuscation: every byte of the buffer is XORed
 * with the key kept at eUnusedExtraToolkitPtrsOffset+4, walking backwards
 * from the last byte to the first (applying it twice restores the data,
 * so one routine serves both encrypt and decrypt).
 *
 * This is a do-while: the body runs once even for a count of <= 0, and a
 * negative index wraps around the address space exactly as the ARM
 * ldrb/strb with a negative register offset does.
 *
 * Note that although the original loads the key into r7 while working,
 * it pushes/pops r4-r7 around the body, so r4-r7 (including r7) are left
 * with their incoming values. The key is only persisted through memory.
 * The last XORed byte is left in r3; r0, r1 and the flags are scratch. */
Next encryption_save_memSetFlags_8006E0E(void) {
    u32 buf = R0;
    u32 key = LD32(A_eUnusedExtraToolkitPtrsOffset + 4);
    s32 i = (s32)R1 - 1;

    do {
        u32 addr = buf + (u32)i; /* negative i wraps, as on the hardware */
        u32 x = LD8(addr) ^ key;
        R3 = x;
        ST8(addr, x);
    } while (--i >= 0);

    ST32(A_eUnusedExtraToolkitPtrsOffset + 4, key);
    RETURN_TO_CALLER();
}

/* ── rollRngSeedIntoExtraToolkitPtrs_8006e26 ── */
/* rollRngSeedIntoExtraToolkitPtrs_8006e26: roll the hardware RNG until it produces something other
 * than 0xFFFFFFFF, then store its bitwise complement at
 * eUnusedExtraToolkitPtrsOffset + 4. r4-r7 are untouched. */
Next rollRngSeedIntoExtraToolkitPtrs_8006e26(void) {
    u32 value;

    do {
        GUEST_CALL(GetRNG);
        value = ~R0;          /* ~RNG: 0 only if the RNG returned 0xFFFFFFFF */
    } while (value == 0);

    ST32(A_eUnusedExtraToolkitPtrsOffset + 4, value);
    RETURN_TO_CALLER();
}

/* ── encryption_copyByteXor6fToToolkitBuffer_8006e3c ── */
/* encryption_copyByteXor6fToToolkitBuffer_8006e3c(r0 = index):
 * Obfuscation step for stored data: take the byte at byte_20004E0[index],
 * XOR it with 0x6F, and write the result into the buffer pointed to by
 * the toolkit's oToolkit_Unk2004a8c_Ptr entry at the same index.
 * All registers (including r0 and r5/r7) are preserved. */
Next encryption_copyByteXor6fToToolkitBuffer_8006e3c(void) {
    u32 index = R0;

    u32 table = A_byte_20004E0;
    u32 buffer = LD32(R10 + oToolkit_Unk2004a8c_Ptr);

    ST8(buffer + index, LD8(table + index) ^ 0x6F);

    RETURN_TO_CALLER();
}

/* ── encryption_navicustMaybe_8006e50 ── */
/* encryption_navicustMaybe_8006e50(r0 = index, r10 = toolkit):
 * Check whether the checksum byte at byte_20004E0[index] (stored XOR-0x6F)
 * matches the byte at Unk2004a8c[index]. Sets r0 = 1 on mismatch; r0 is
 * otherwise clobbered (callers ignore it on the match path).
 * Flags reflect the comparison, as the original's CMP/MOV would leave them. */
Next encryption_navicustMaybe_8006e50(void) {
    u32 index = R0;
    u32 table = LD32(R10 + oToolkit_Unk2004a8c_Ptr);

    u32 expected = (u8)LD8(A_byte_20004E0 + index) ^ 0x6F;
    u32 actual   = (u8)LD8(table + index);

    SET_FLAGS_CMP(expected, actual);
    if (expected != actual) {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── encryption_applyPack_8006e70 ── */
/* encryption_applyPack_8006e70(r0 = pack slot index):
 * Re-encrypts one byte of pack data: the source byte lives in the table at
 * byte_20008A0, and its XOR-0x81 counterpart is written into the pack buffer
 * (Toolkit.unk_2004C20, reached via the pointer in r10). All registers are
 * preserved; r10 is only read. */
Next encryption_applyPack_8006e70(void) {
    u32 index = R0;

    u32 packBuffer = LD32(R10 + oToolkit_Unk2004c20_Ptr);
    ST8(packBuffer + index, LD8(A_byte_20008A0 + index) ^ 0x81);

    RETURN_TO_CALLER();
}

/* ── encryption_testPack_8006e84 ── */
/* encryption_testPack_8006e84(r0 = pack slot index, r10 = toolkit):
 * Verify an encrypted pack data byte. Compares (byte_20008A0[index] ^ 0x81)
 * against the byte stored at the same index in the buffer pointed to by
 * toolkit->Unk2004c20. On mismatch r0 is set to 1; on match r0 is left as the
 * index that was passed in. The flags always reflect the comparison, so
 * callers may branch on equality either way.
 * Clobbers r0; preserves r1-r7. */
Next encryption_testPack_8006e84(void) {
    u32 index = R0;

    u8 decrypted = LD8(A_byte_20008A0 + index) ^ 0x81;
    u8 stored = LD8(LD32(R10 + oToolkit_Unk2004c20_Ptr) + index);

    if (decrypted == stored) {
        /* Flags come from the CMP against the stored byte. */
        SET_FLAGS_CMP(decrypted, stored);
    } else {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── encryption_zenny_8006f54 ── */
/* encryption_zenny_8006f54:
 * Re-encrypt the player's zenny counter. Stores the raw (plaintext) amount
 * where Unk2005028 points, and keeps a bitwise-NOT mirror of it in the game
 * state (an anti-tamper check) unless that mirror is currently suppressed
 * by bit 0 of oGameState_Unk_74. The stored value is the plaintext amount
 * XORed with the global obfuscation key at dword_2000060.
 * All argument registers (r0-r7) are preserved; r10 is the toolkit pointer. */
Next encryption_zenny_8006f54(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 zenny = LD32(gameState + oGameState_ProtectedZenny);

    /* Maintain the inverted copy of the zenny amount unless it is held. */
    if (!(LD32(gameState + oGameState_Unk_74) & 1))
        ST32(gameState + oGameState_Unk_6c, ~zenny);

    /* Store zenny XOR the global key into the encrypted wallet pointer. */
    u32 key = LD32(A_dword_2000060);
    ST32(LD32(R10 + oToolkit_Unk2005028_Ptr), zenny ^ key);

    RETURN_TO_CALLER();
}

/* ── encryption_zenny_8006f78 ── */
/* encryption_zenny_8006f78: zenny anti-tamper check.
 *
 * Validates the protected copy of the player's zenny two ways:
 *   1. oGameState_ProtectedZenny must be the bitwise complement of the
 *      shadow copy in oGameState_Unk_6c; otherwise bit 0 of
 *      oGameState_Unk_74 (cheat-detected flags) is set.
 *   2. oGameState_ProtectedZenny XOR the key in dword_2000060 must equal
 *      the checksum stored behind the pointer at
 *      toolkit + oToolkit_Unk2005028_Ptr.
 *
 * Leaves the Z flag set (from CMP r0, r1) when the keyed checksum still
 * matches, and clears it (via MOVS r0, #1) when it doesn't. r0-r7 are
 * pushed/popped by the original, so all of them come back unchanged. */
Next encryption_zenny_8006f78(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 zenny = LD32(gameState + oGameState_ProtectedZenny);

    if (zenny != ~LD32(gameState + oGameState_Unk_6c)) {
        u32 flags = LD32(gameState + oGameState_Unk_74);
        ST32(gameState + oGameState_Unk_74, flags | 1);
    }

    u32 keyedZenny = zenny ^ LD32(A_dword_2000060);
    /* pointer from the toolkit, then the checksum it points at (two loads) */
    u32 expected = LD32(LD32(R10 + oToolkit_Unk2005028_Ptr));

    if (keyedZenny == expected)
        SET_FLAGS_CMP(keyedZenny, expected);
    else
        SET_FLAGS_NZ(1);
    RETURN_TO_CALLER();
}

/* ── encryption_bugfrags_8006fac ── */
/* encryption_bugfrags_8006fac:
 * Recomputes the obfuscated BugFrag counter. Reads the protected BugFrag
 * count from the game state; unless save-flag bit 1 is set, stores its
 * bitwise complement into the checksum slot (oGameState_Unk_70). Then
 * scrambles the count with the value at dword_20018B8 and writes the
 * result to the display/state pointer at oToolkit_Unk2005030_Ptr.
 * All argument registers (r0-r7) are preserved; the original push/pop
 * exists only to give the body scratch registers. */
Next encryption_bugfrags_8006fac(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 bugfrags = LD32(gameState + oGameState_ProtectedBugfrags);

    if (!(LD32(gameState + oGameState_Unk_74) & 2))
        ST32(gameState + oGameState_Unk_70, ~bugfrags);

    u32 scramble = LD32(A_dword_20018B8);
    u32 output = LD32(R10 + oToolkit_Unk2005030_Ptr);
    ST32(output, bugfrags ^ scramble);

    RETURN_TO_CALLER();
}

/* ── encryption_bugfrags_8006fd0 ── */
/* encryption_bugfrags_8006fd0:
 * Anti-tamper check on the protected BugFrag counter. The counter keeps a
 * one's-complement shadow copy in the game state; if the two no longer agree,
 * the tamper flag (bit 1 of GameState_Unk_74) is set. The counter is then
 * XOR-obfuscated with a key and compared against its stored mirror, and the
 * result is reported in the flags only: Z set = count intact, Z clear =
 * count was modified. r0-r7 are preserved (the original pushes/pops them, so
 * the "mov r0,#1" on the mismatch path only exists to clear the Z flag).
 */
Next encryption_bugfrags_8006fd0(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    u32 bugfrags = LD32(gameState + oGameState_ProtectedBugfrags);

    /* Counter vs. its one's-complement shadow: refresh the tamper flag on mismatch. */
    u32 shadow = LD32(gameState + oGameState_Unk_70);
    if (bugfrags != ~shadow) {
        u32 stateFlags = LD32(gameState + oGameState_Unk_74);
        ST32(gameState + oGameState_Unk_74, stateFlags | 2);
    }

    /* Compare the XOR-obfuscated counter against its stored mirror. */
    u32 obfuscated = bugfrags ^ LD32(A_dword_20018B8);
    u32 mirror = LD32(LD32(R10 + oToolkit_Unk2005030_Ptr));

    SET_FLAGS_CMP(obfuscated, mirror);
    if (obfuscated != mirror)
        SET_FLAGS_NZ(1); /* mov r0,#1: clear Z, C/V stay from the compare above */

    RETURN_TO_CALLER();
}

/* ── anticheat_modCardsInit_8007036 ── */
/* anticheat_modCardsInit_8007036:
 * Builds the 256-byte table at byte_2000670 used by the card-modification
 * anticheat check. For every index (filled top-down from 0xFF to 0) a random
 * byte is rolled until it differs from 0x8D; the visible table stores
 * byte ^ 0x8D while the hidden shadow buffer (r10 ->
 * oToolkit_Unk2005038_Ptr) stores the bitwise complement, letting the game
 * cross-check the two copies later. */
Next anticheat_modCardsInit_8007036(void) {
    u32 shadowTable = LD32(R10 + oToolkit_Unk2005038_Ptr);
    u32 modCardsTable = A_byte_2000670;

    /* Caller registers that the RNG helper is free to clobber (r0-r3);
     * r4-r7 are kept in locals so the loop survives the call. */
    u32 saveR1 = R1, saveR2 = R2, saveR3 = R3;
    u32 saveR4 = R4, saveR5 = R5, saveR6 = R6, saveR7 = R7;

    s32 i;
    for (i = 0xFF; i >= 0; i--) {
        u8 entry;
        do {
            R7 = (u32)i;            /* the helper observes the current index */
            GUEST_CALL(GetRNG);
            entry = (u8)(R0 ^ 0x8D);
        } while (entry == 0);       /* reroll 0x8D: entry must be nonzero */

        ST8(modCardsTable + (u32)i, entry);
        ST8(shadowTable + (u32)i, (u8)~entry);
    }

    R1 = saveR1; R2 = saveR2; R3 = saveR3;
    R4 = saveR4; R5 = saveR5; R6 = saveR6; R7 = saveR7;
    RETURN_TO_CALLER();
}

/* ── set_flags32_20093A4 ── */
/* set_flags32_20093A4(r0 = bits): set the given bits in the global flags word
 * at flags32_20093A4 (read-modify-write). R2 receives the updated word. */
Next set_flags32_20093A4(void) {
    u32 flags = LD32(A_flags32_20093A4);
    flags |= R0;
    ST32(A_flags32_20093A4, flags);
    R2 = flags;
    RETURN_TO_CALLER();
}

/* ── clear_flags32_20093A4 ── */
/* clear_flags32_20093A4(r0 = mask): clear the bits set in mask from the game-wide
 * flags32 word, leaving the updated word in r2. */
Next clear_flags32_20093A4(void) {
    u32 *flags = (u32 *)A_flags32_20093A4;

    R2 = LD32((u32)flags) & ~R0;
    ST32((u32)flags, R2);

    RETURN_TO_CALLER();
}

/* ── reset_flags32_20093A4 ── */
/* reset_flags32_20093A4: clear the flags32_20093A4 dword (a 32-bit game flag word). */
Next reset_flags32_20093A4(void) {
    ST32(A_flags32_20093A4, 0);
    RETURN_TO_CALLER();
}

/* ── camera_resetToOrigin_8007338 ── */
/* camera_resetToOrigin_8007338: reinitialize the camera.
 * Runs the camera's reset routine, then re-inits the camera at the origin
 * of map group 0xF0, map 0, and clears camera field unk03/14. */
Next camera_resetToOrigin_8007338(void) {
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

/* ── battle_SpawnEnemyObjects_8007358 ── */
/* battle_SpawnEnemyObjects_8007358: spawn the enemy objects for this battle.
 * Reads the enemy setup array from the current battle settings (via the battle
 * state in r5) and creates one battle object per entry, then runs the post-spawn
 * setup routine. All registers except r0/r1 are left untouched. */
Next battle_SpawnEnemyObjects_8007358(void) {
    u32 settings = LD32(R5 + oBattleState_BattleSettings);
    R0 = LD32(settings + oBattleSettings_EnemySetupArrPtr);

    GUEST_CALL(SpawnBattleObjectUsingBattleEntityConfig_8007368);
    GUEST_CALL(battle_rollRandomItemDrops_80AA88C);

    RETURN_TO_CALLER();
}

/* ── SpawnBattleObjectUsingBattleEntityConfig_8007368 ── */
/* SpawnBattleObjectUsingBattleEntityConfig_8007368(r0 = EnemySetup list):
 * Walks the battle entity setup list (one 4-byte entry per object to spawn,
 * terminated by an entry whose kind byte is 0xF0) and spawns each battle
 * object through the handler selected by the entry's kind. Afterwards it
 * snapshots the current halfword at oBattleState_Unk_04_05 into
 * oBattleState_Unk_12_13 and copies the alive-actor words over the battle
 * actor slots.
 *
 * Register notes: the dispatch loop keeps the entry pointer in r6 itself, so
 * a handler that repoints r6 (as spawnEnemy_80073E2 does) changes where the
 * next entry is read from; `add r6, #4` advances whatever the handler left.
 * Handlers are entered with r0 = the table byte offset (kind >> 2, values
 * 0x00..0x28), r1 = their own address as loaded from the dispatch table
 * (Thumb bit set, as sub_80074C8 expects) and r6 = the entry pointer, and
 * they inherit r4/r5/r7/r10. Everything they leave in r5/r8-r12 flows
 * straight through to our caller; r6 itself is preserved. */
Next SpawnBattleObjectUsingBattleEntityConfig_8007368(void) {
    u32 callerR6 = R6;
    R6 = R0;

    for (;;) {
        /* Kind is masked with 0xF0: the low nibble is ignored, 0xF0 marks the
         * end of the list (the last valid kind, 0xA0, is 0x28 * 4). */
        u32 kind = LD8(R6) & 0xF0;
        if (kind == 0xF0)
            break;

        /* kind >> 2 is used both as the byte offset into the dispatch table
         * and as the dispatch index the handlers receive in r0. */
        u32 index = kind >> 2;

        R0 = index;
        R1 = LD32(A_off_80073A0 + index);

        switch (index) {
        case 0x00: GUEST_CALL(spawnMegaMan_80073CC);     break; /* the player */
        case 0x04: GUEST_CALL(spawnEnemy_80073E2);       break;
        case 0x08: GUEST_CALL(spawnMysteryData_8007424); break;
        case 0x0C: GUEST_CALL(spawnRock_8007450);        break;
        case 0x10: GUEST_CALL(nullsub_40);               break;
        case 0x14: GUEST_CALL(nullsub_41);               break;
        case 0x18: GUEST_CALL(sub_80074C8);              break;
        case 0x1C: GUEST_CALL(sub_80074DE);              break;
        case 0x20: GUEST_CALL(spawnRockCube_80074FA);    break;
        case 0x24: GUEST_CALL(spawnGuardian_800751C);    break;
        case 0x28: GUEST_CALL(spawnMetalCube_800748A);   break;
        default:   break; /* kinds 0xB0-0xE0 never occur in practice */
        }

        R6 += 4;
    }

    /* Carry the current value of Unk_04_05 over to Unk_12_13 for the newly
     * spawned battle actors. */
    ST16(R5 + oBattleState_Unk_12_13, LD16(R5 + oBattleState_Unk_04_05));

    /* Copy the 8 alive-actor words over the battle actor array. */
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = battleState + oBattleState_AliveBattleActors;
    R1 = battleState + oBattleState_BattleActors;
    R2 = 0x20;
    GUEST_CALL(CopyWords);

    R6 = callerR6;
    RETURN_TO_CALLER();
}

/* ── spawnMegaMan_80073CC ── */
/* spawnMegaMan_80073CC(r6 = *EnemySetup):
 * Spawn Mega Man (via battle_spawnNaviOnPanel_800753C) at the panel encoded in the setup's
 * position byte -- low 3 bits are the panel x, bits 4+ the panel y -- on
 * the side of the field given by the setup's alliance flag (bit 0).
 * Flags on exit are whatever battle_spawnNaviOnPanel_800753C leaves. */
Next spawnMegaMan_80073CC(void) {
    u32 self = R6;

    u32 pos = LD8(self + 1);
    u32 panel_x = pos & 7;
    u32 panel_y = pos >> 4;

    u32 alliance = LD8(self) & 1;

    R0 = panel_x;
    R1 = panel_y;
    R2 = alliance;
    R3 = 1;
    GUEST_CALL(battle_spawnNaviOnPanel_800753C);

    RETURN_TO_CALLER();
}

/* ── spawnEnemy_80073E2 ── */
/* spawnEnemy_80073E2: spawn one enemy from an EnemySetup record (r6).
 *
 * EnemySetup layout:
 *   +0: bit 0 = alliance, remaining bits used to dispatch to this spawner
 *   +1: bits 0-2 = panel x, bits 4-7 = panel y (passed unmasked, as in the original)
 *   +2/3: 12-bit enemy id (viruses, or an enemy navi when > 0xFF)
 *
 * Returns the battle object left by battle_spawnEnemyObject_800768C
 * (its r4/r6/r7 results pass straight through to our caller); r5 is preserved.
 */
Next spawnEnemy_80073E2(void) {
    u32 setup = R6;

    u32 pos = LD8(setup + 1);
    u32 panel_x = pos & 7;   /* bits 0-2 */
    u32 panel_y = pos >> 4;  /* bits 4-7, no mask: a byte with bit 7 set lands on another row */

    /* Low byte plus high nibble shifted up: a u12 enemy id. */
    u32 enemy_idx = LD8(setup + 2) + ((LD8(setup + 3) & 0xF) << 8);

    /* Only one bit of alliance survives; the rest of byte 0 chose this spawner. */
    u32 alliance = LD8(setup) & 1;

    u32 saved_r5 = R5;

    R5 = 0; /* extra argument to battle_spawnEnemyObject_800768C */

    /* Ask for the current battle effects; unless bit 15 is set, the enemy id
     * is first run through sub_800EC80 (which may substitute a different id). */
    R0 = enemy_idx;
    R1 = panel_x;
    R2 = panel_y;
    R3 = alliance;
    GUEST_CALL(GetBattleEffects);
    if (!(R0 & 0x8000)) {
        R0 = enemy_idx;
        GUEST_CALL(sub_800EC80);
        enemy_idx = R0;
    }

    R0 = enemy_idx;
    R1 = panel_x;
    R2 = panel_y;
    R3 = alliance;
    R5 = 0;
    GUEST_CALL(battle_spawnEnemyObject_800768C);

    R5 = saved_r5; /* pop {r5}: r5 must reach the caller unchanged */
    RETURN_TO_CALLER();
}

/* ── spawnMetalCube_800748A ── */
/* spawnMetalCube_800748A: maybe-spawn a MetalCube into the slot table
 * pointed to by r5, based on the EnemySetup panel at r6.
 *
 *   r5 = spawn slot table; slot at +0xB8 is the primary handle,
 *        slot at +0xBC is the secondary.
 *   r6 = *EnemySetup; field1 is a packed byte (high nibble = col,
 *        low 3 bits = row). Bit 5 of the panel parameter word marks
 *        a "metal cube hole" tile.
 *
 * Spawns into [r7] when *either* slot is empty; only skips when both
 * are already populated. The chosen slot pointer is held in r7 and
 * passed (implicitly) into sub_80D85F0.
 */
Next spawnMetalCube_800748A(void) {
    u32 entry_r4 = R4, entry_r5 = R5, entry_r6 = R6, entry_r7 = R7;

    /* r7 = r5 + 0xB8 (primary slot). R7 carries through the callees. */
    u32 r7 = R5 + 0xB8;
    R7 = r7;

    /* If primary is occupied, peek at secondary (+0xBC). Return only
     * when both are populated; otherwise fall through with r7 pointing
     * at the empty slot to fill. */
    if (LD32(r7) != 0) {
        if (LD32(r7 + 4) != 0) {
            R4 = entry_r4;
            R5 = entry_r5;
            R6 = entry_r6;
            R7 = entry_r7;
            RETURN_TO_CALLER();
        }
        r7 += 4;
        R7 = r7;
    }

    /* Unpack (col, row) from EnemySetup.field1 and ask the panel system. */
    u32 packed = LD8(R6 + 1);
    R0 = (packed << 29) >> 29;     /* low 3 bits -> row */
    R1 = packed >> 4;              /* high nibble -> col */
    GUEST_CALL(object_getPanelParameters);

    /* "Hole" tile test = bit 5 of the returned panel parameter word. */
    R4 = (R0 & 0x20) >> 5;

    /* Rebuild (row, col) -- R0/R1 were clobbered -- and set spawn args. */
    packed = LD8(R6 + 1);
    R0 = (packed << 29) >> 29;
    R1 = packed >> 4;
    R2 = 0;
    R6 = 0x64;
    /* R7 already holds r7. */

    GUEST_CALL(sub_80D85F0);
    ST32(r7, R0);

    R4 = entry_r4;
    R5 = entry_r5;
    R6 = entry_r6;
    R7 = entry_r7;
    RETURN_TO_CALLER();
}

/* ── battle_spawnNaviOnPanel_800753C ── */
/* battle_spawnNaviOnPanel_800753C: spawn a battle object at a panel, initialize it, allocate AI data,
 * resolve its NameID, and register it in the alliance entry list.
 * (panel_x: u8, panel_y: u8, alliance: bool) */

/* register form: calls object_spawnType1 outside this file, which has no typed C interface yet */
static u32 helper_spawn_type1(void) {
    R0 = 0; R4 = 0;
    GUEST_CALL(object_spawnType1);
    return R5;
}

/* register form: calls object_getCoordinatesForPanels outside this file, which has no typed C interface yet */
static void helper_panel_to_coords(u32 px, u32 py, u32 *x, u32 *y) {
    R0 = px; R1 = py;
    GUEST_CALL(object_getCoordinatesForPanels);
    *x = R0; *y = R1;
}

/* register form: calls object_createAIData outside this file, which has no typed C interface yet */
static u32 helper_create_aidata(void) {
    GUEST_CALL(object_createAIData);
    return R0;
}

/* register form: calls object_freeMemory outside this file, which has no typed C interface yet */
static void helper_free_memory(void) {
    GUEST_CALL(object_freeMemory);
}

/* register form: calls GetBattleNaviStatsByte outside this file, which has no typed C interface yet */
static u32 helper_navi_stats_byte(u32 alliance, u32 stat) {
    R0 = alliance; R1 = stat;
    GUEST_CALL(GetBattleNaviStatsByte);
    return R0;
}

/* register form: calls modifyBattleStateBasedOnAliveBattleActorsAndUnkCond_8007778 outside this file, which has no typed C interface yet */
static u32 helper_check_alive_state(void) {
    GUEST_CALL(modifyBattleStateBasedOnAliveBattleActorsAndUnkCond_8007778);
    return R0;
}

/* register form: calls sub_800ED80 outside this file, which has no typed C interface yet */
static void helper_teardown_aidata(u32 ptr) {
    R0 = ptr;
    GUEST_CALL(sub_800ED80);
}

/* register form: calls GetVerActorTyAndAIIdx_80182B4 outside this file, which has no typed C interface yet */
static u32 helper_ver_actor_ty_ai(u32 enemy_idx) {
    R0 = enemy_idx;
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);
    return R0;
}

/* register form: calls enemy_getStruct1 outside this file, which has no typed C interface yet */
static u32 helper_enemy_get_struct1(u32 name_id) {
    R0 = name_id;
    GUEST_CALL(enemy_getStruct1);
    return R0;
}

/* register form: calls battle_addObjectToAllianceEntryList_80077D2 outside this file, which has no typed C interface yet */
static void helper_add_to_alliance_list(u32 ai, u32 obj) {
    R4 = ai; R5 = obj;
    GUEST_CALL(battle_addObjectToAllianceEntryList_80077D2);
}

static void spawn_battle_object(u32 panel_x, u32 panel_y, u32 alliance) {
    /* register form: save preserved R4/R5 that callees will modify (original uses push {r4,r5,lr}) */
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    /* Request a free object slot for a new battle actor at the panel. */
    u32 obj = helper_spawn_type1();

    if (obj == 0) {
        /* register form: replicate original's pop {r0-r2} and tst r5,r5; callee clobbered R0-R2 */
        R0 = panel_x; R1 = panel_y; R2 = alliance;
        SET_FLAGS_NZ(obj);
        /* register form: restore preserved regs (original's pop {r4,r5,pc}) */
        R4 = saved_r4; R5 = saved_r5;
        return;
    }

    /* Stamp panel position and alliance onto the new object. */
    ST8(obj + oBattleObject_Alliance, alliance);
    ST8(obj + oBattleObject_PanelX, panel_x);
    ST8(obj + oBattleObject_PanelY, panel_y);
    ST8(obj + oBattleObject_FuturePanelX, panel_x);
    ST8(obj + oBattleObject_FuturePanelY, panel_y);

    /* Convert panel indices to pixel coordinates. */
    u32 coord_x, coord_y;
    helper_panel_to_coords(panel_x, panel_y, &coord_x, &coord_y);
    ST32(obj + oBattleObject_X, coord_x);
    ST32(obj + oBattleObject_Y, coord_y);
    ST32(obj + oBattleObject_Z, 0);

    /* Pause the object's per-frame update while it initialises. */
    ST8(obj + oObjectHeader_Flags,
        LD8(obj + oObjectHeader_Flags) | OBJECT_FLAG_PAUSE_UPDATE);

    /* Allocate AI data for this object. */
    u32 ai = helper_create_aidata();
    ST32(obj + oBattleObject_AIDataPtr, ai);

    if (ai == 0) {
        /* AI allocation failed: free the slot and return; callee sets R0/R1/R2/flags. */
        helper_free_memory();
        /* register form: restore preserved regs (original's pop {r4,r5,pc}) */
        R4 = saved_r4; R5 = saved_r5;
        return;
    }

    /* AI data type 2 = standard Navi behaviour. */
    ST8(ai, 2);

    /* Derive NameID from the alliance's Navi stats table. */
    u32 stats = helper_navi_stats_byte(LD8(obj + oBattleObject_Alliance), 0x29);
    u32 name_id = (stats + 0xFF + 0xA1) & 0xFFFF;
    ST16(obj + oBattleObject_NameID, name_id);

    /* If no living actors remain (special condition), tear down and free AI data. */
    if (helper_check_alive_state() == 0) {
        helper_teardown_aidata(LD32(obj + oBattleObject_AIDataPtr));
        helper_free_memory();
    }

    /* Populate AI fields from the version/actor-type/AI-index lookup table. */
    u32 ver_entry = helper_ver_actor_ty_ai(name_id);
    ST8(ai + 0x16, LD8(ver_entry));
    ST8(ai + 0x17, LD8(ver_entry));
    ST8(ai, LD8(ver_entry + 1));
    ST8(ai + 1, LD8(ver_entry + 2));

    /* Store enemy struct category and mark slot as unassigned. */
    u32 es = helper_enemy_get_struct1(name_id);
    ST8(ai + 3, LD8(es + 2));
    ST8(ai + 0xE, 0xFF);

    /* Register the object in the alliance's entry list; callee sets final R0/R1/R2/flags. */
    helper_add_to_alliance_list(ai, obj);

    /* register form: restore preserved regs (original's pop {r4,r5,pc}) */
    R4 = saved_r4; R5 = saved_r5;
}

/* battle_spawnNaviOnPanel_800753C(r0=panel_x, r1=panel_y, r2=alliance) -> r0 r1 r2 r3, nz c v, r4 r5 r6 r7 preserved */
Next battle_spawnNaviOnPanel_800753C(void) {
    spawn_battle_object(R0, R1, R2);
    RETURN_TO_CALLER();
}

/* ── loadSceneGfx_80075CA ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word_sub_80075CA(u32 dst, u32 num_bytes) {
    R0 = dst;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls QueueGFXTransfersInList outside this file, which has no typed C interface yet */
static void queue_gfx_transfers_in_list(u32 dataRefs) {
    R0 = dataRefs;
    GUEST_CALL(QueueGFXTransfersInList);
}

/* register form: calls SWI_LZ77UnCompReadNormalWrite16bit outside this file, which has no typed C interface yet */
static void lz77_unpack_wide(u32 src, u32 dst) {
    R0 = src;
    R1 = dst;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite16bit);
}

/* Load this scene's graphics:
 * - clear the 64-byte tile buffer at 0x6008000 (the original clears it twice),
 * - queue the VRAM/palette transfers described by the list at off_80075F0,
 * - LZ77-decompress the tile set dword_86DDBA0 to 0x6001460. */
static void load_scene_gfx(void) {
    zero_fill_by_word_sub_80075CA(0x6008000, 0x40);
    zero_fill_by_word_sub_80075CA(0x6008000, 0x40);
    queue_gfx_transfers_in_list(A_off_80075F0);
    lz77_unpack_wide(A_dword_86DDBA0, 0x6001460);
}

/* loadSceneGfx_80075CA() -> r2 r3 r4 r5 r6 r7 (all left unchanged; r0/r1/lr and flags are clobbered) */
Next loadSceneGfx_80075CA(void) {
    load_scene_gfx();
    RETURN_TO_CALLER();
}

/* ── battle_initDisplayTables_8007628 ── */
/* Table-driven init of two display memory regions from toolkit pointers.
 * Each table is a sequence of (u16 byte-offset, u16 value) pairs terminated by offset 0xFF. */

typedef struct {
    u32 base_ptr;    /* r0: second destination region */
    u32 table_end;   /* r1: address of the 0xFF terminator in table 2 */
    u16 offset;      /* r2: terminator marker (always 0xFF on return) */
    u16 value;       /* r3: last value written to the second region */
} InitTablesResult;

static InitTablesResult init_toolkit_display_tables(u32 toolkit) {
    InitTablesResult result;
    u16 offset;
    u16 value;

    /* Initialize BG control / render-info registers */
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
    u32 table = A_word_800765C;
    while ((offset = LD16(table)) != 0xFF) {
        value = LD16(table + 2);
        ST16(render_info + offset, value);
        table += 4;
    }

    /* Initialize palette / BG character-data region */
    result.base_ptr = LD32(toolkit + oToolkit_Unk200f3a0_Ptr);
    table = A_word_8007672;
    while ((offset = LD16(table)) != 0xFF) {
        value = LD16(table + 2);
        ST16(result.base_ptr + offset, value);
        table += 4;
    }

    result.table_end = table;
    result.offset = 0xFF;
    result.value = value;
    return result;
}

/* battle_initDisplayTables_8007628(r10 = toolkit base) -> r0-r3, nzcv */
Next battle_initDisplayTables_8007628(void) {
    InitTablesResult result = init_toolkit_display_tables(R10);
    R0 = result.base_ptr;
    R1 = result.table_end;
    R2 = result.offset;
    R3 = result.value;
    SET_FLAGS_CMP(result.offset, 0xFF);
    RETURN_TO_CALLER();
}

/* ── battle_spawnEnemyObject_800768C ── */
/* battle_spawnEnemyObject_800768C(r0 = enemy idx, r1 = panel x, r2 = panel y, r3 = alliance,
 * r5 = extra argument) -> BattleObject pointer in r0.
 *
 * This is only the frame-setting entry stub of the battle-object spawner:
 * it saves r4-r7/LR, reserves the local frame shared with the spawner body,
 * clears the local at stack_800768C_unk_10, then tail-branches into the
 * shared body at loc_80076A8 (a second entry point inside the spawner).
 * The shared body pops the frame saved here and returns straight to our
 * caller, so control never comes back to this stub. */
Next battle_spawnEnemyObject_800768C(void) {
    /* push {r4-r7,lr} */
    u32 saved = SP -= 20;
    ST32(saved + 0x00, R4);
    ST32(saved + 0x04, R5);
    ST32(saved + 0x08, R6);
    ST32(saved + 0x0C, R7);
    ST32(saved + 0x10, LR);

    /* sub sp, sp, #0x14 — locals: enemy idx / panel x / panel y / alliance
     * / running result; the saved registers sit just above this frame. */
    SP -= 0x14;

    /* movs r4, #0 — also the last flag-setting write before the branch */
    R4 = 0;
    SET_FLAGS_NZ(R4);

    /* str r4, [sp, #stack_800768C_unk_10] — clear the shared local */
    ST32(SP + stack_800768C_unk_10, R4);

    /* b loc_80076A8 — tail-branch into the shared spawner body */
    GUEST_CALL(loc_80076A8);
    RETURN_TO_CALLER();
}

/* ── modifyBattleStateBasedOnAliveBattleActorsAndUnkCond_8007778 ── */
/* modifyBattleStateBasedOnAliveBattleActorsAndUnkCond_8007778
 * (r5 = battle object, r10 = toolkit):
 * Claim the first free slot in the object's alliance's "alive battle actors"
 * list in the battle state (4 slots per alliance).  On success, stores the
 * object pointer in that slot and, unless the object's AI data byte 2 is 1,
 * also increments the alive-actor counter kept right after the lists.
 * Returns 1 in r0 when a slot was claimed, 0 when all four slots were already
 * occupied.  r2/r3 are left as scratch values from the last examined slot,
 * and the c/v flags reflect the original's last flag-setting operation on
 * each path. */
Next modifyBattleStateBasedOnAliveBattleActorsAndUnkCond_8007778(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 alliance = LD8(R5 + oBattleObject_Alliance);

    /* Per-alliance block of 4 actor slots in the battle state. */
    u32 slotAddr = battleState + alliance * 0x10 + oBattleState_AliveBattleActors;

    u32 entry = 0;
    for (u32 slot = 0; slot < 4; slot++, slotAddr += 4) {
        entry = LD32(slotAddr);
        if (entry == 0) {
            /* Found a free slot: register this object in it. */
            ST32(slotAddr, R5);

            u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
            u32 unk02 = LD8(aiData + 2);
            if (unk02 != 1) {
                /* Count the object as alive for its alliance. */
                u32 counterAddr = battleState + alliance + 4;
                u32 oldCount = LD8(counterAddr);
                u32 count = oldCount + 1;
                ST8(counterAddr, count);
                R2 = count;
                SET_FLAGS_ADD(oldCount, 1);
            } else {
                /* Flagged objects skip the counter; flags stay from the
                 * comparison against 1. */
                R2 = unk02;
                SET_FLAGS_CMP(unk02, 1);
            }
            R0 = 1;
            R3 = slotAddr;
            RETURN_TO_CALLER();
        }
    }

    /* No free slot in this alliance's list. */
    R2 = entry;
    R3 = slotAddr;
    R0 = 0;
    SET_FLAGS_CMP(4, 4);
    RETURN_TO_CALLER();
}

/* ── RemoveGivenAliveBattleActorFromList ── */
/* RemoveGivenAliveBattleActorFromList(r0 = actor, r10 = toolkit):
 * Walks the battle state's list of 8 alive player actor slots and clears
 * every slot that holds the given actor.
 * Leaves r1 pointing just past the list, r2 = value of the last slot read
 * (0 if it matched), r3 = 8, and the flags from `cmp r3, #8`. */
Next RemoveGivenAliveBattleActorFromList(void) {
    u32 actor = R0;
    u32 entry = LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_AlivePlayerActors;

    R3 = 0;
    while ((s32)R3 < 8) {
        R2 = LD32(entry);
        if (R2 == actor) {
            R2 = 0;
            ST32(entry, R2);
        }
        entry += 4;
        R3 += 1;
    }

    R1 = entry;
    SET_FLAGS_CMP(R3, 8);
    RETURN_TO_CALLER();
}

/* ── battle_addObjectToAllianceEntryList_80077D2 ── */
/* battle_addObjectToAllianceEntryList_80077D2 (r5 = battle object, r10 = toolkit):
 * Append a battle object to its alliance's entry list in the battle state.
 * Each alliance has an entry counter at battleState+8[alliance] and a list of
 * halfword name-ID slots at battleState+0x4C + alliance*8. The object's name
 * ID is written to the next free slot and the counter is bumped.
 * Leaves r0 = name ID, r1 = list offset, r2 = counter offset, r3 = battle
 * state pointer; flags reflect the final address addition (adds r1, r1, r4).
 * r4 and r6 are saved and restored by the original, so they are preserved. */

#define oBattleState_Unk_08 8    /* per-alliance entry counter base */
#define oBattleState_Unk_4C 0x4C /* per-alliance entry list base */

Next battle_addObjectToAllianceEntryList_80077D2(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);

    u32 nameID = LD16(R5 + oBattleObject_NameID);
    u32 alliance = LD16(R5 + oBattleObject_AllianceAndDirectionFlip);

    /* take (and increment) this alliance's next free entry slot */
    u32 counterOffset = oBattleState_Unk_08 + alliance;
    u32 slot = LD8(battleState + counterOffset);
    ST8(battleState + counterOffset, slot + 1);

    /* store the name ID into the alliance's halfword entry list */
    u32 listOffset = (alliance << 3) + oBattleState_Unk_4C;
    R0 = nameID;
    R2 = counterOffset;
    R3 = battleState;
    SET_FLAGS_ADD(listOffset, slot * 2);
    R1 = listOffset + slot * 2;
    ST16(battleState + R1, nameID);
    RETURN_TO_CALLER();
}

/* ── battle_main_8007800 ── */
/* battle_main_8007800: per-frame battle dispatcher.
 * Runs two pre-frame hooks, dispatches to the active battle-state handler
 * via a jump table (indexed by a byte stored at offset 0 of the BattleState),
 * then performs post-state bookkeeping: increment frame counter, query
 * battle effects, extract state-byte nibbles.
 *
 * Preserved: r4, r5 (saved/restored via push/pop in original).
 * Flags on return: N,Z from TST(R0,R0); C=0; V from last callee. */
Next battle_main_8007800(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    /* Pre-frame global state updates */
    GUEST_CALL(battle_updateCustomGauge_801FE6C);
    GUEST_CALL(battleObject_checkTerminateFlagAndDispatch_8020140);

    /* Fetch BattleState pointer from Toolkit */
    R5 = LD32(R10 + oToolkit_BattleStatePtr);

    /* Dispatch to the handler selected by the state-byte jump table.
     * The byte at offset 0 is used directly as a table byte-offset
     * (entries at 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14). */
    u32 stateByte = LD8(R5 + oBattleState_Index_00);
    switch (stateByte) {
    case 0x00: GUEST_CALL(battle_finishBattle_8007850);           break;
    case 0x04: GUEST_CALL(battle_update_8007A44); break;
    case 0x08: GUEST_CALL(sub_8007B80);           break;
    case 0x0C: GUEST_CALL(sub_8007E62);           break;
    case 0x10: GUEST_CALL(sub_8007F4E);           break;
    default: break; /* 0x14: handler was removed from the ROM */
    }

    /* Increment the per-frame battle counter */
    ST32(R5 + oBattleState_Unk_60, LD32(R5 + oBattleState_Unk_60) + 1);

    /* Query how many battle effects are currently active */
    GUEST_CALL(GetBattleEffects);
    u32 effectsCount = R0;

    /* Get low byte and upper nibble of the current state byte */
    GUEST_CALL(battle_getStateByteNibbles_800A832);

    /* Rearrange for caller: R2 = nibble_hi, R1 = nibble_lo,
     * R0 = raw state byte at offset 0x0A, R3 = effects count */
    R2 = R1;
    R1 = R0;
    R0 = LD8(R5 + oBattleState_Unk_0a);
    R3 = effectsCount;

    /* TST R0, R0: sets N, Z from R0; C cleared; V unchanged */
    SET_FLAGS_NZ(R0);

    /* Restore preserved registers */
    R4 = saved_r4;
    R5 = saved_r5;

    RETURN_TO_CALLER();
}

/* ── battle_dispatchPhase_8007978 ── */
/* battle_dispatchPhase_8007978: battle phase dispatcher.
 *
 * R5 points to the battle state; the byte at offset 2 holds the current
 * phase, already scaled by 4 so it indexes the handler table at
 * off_800798C directly (0x00/0x04/0x08/0x0C). The computed jump leaves
 * R0 = phase byte and R1 = the handler's own address (an artifact of
 * loading the jump target into R1), so both are set before the call.
 * Every other register passes through untouched, and the flags that come
 * back are whatever the handler last set. */
Next battle_dispatchPhase_8007978(void) {
    u32 phase = LD8(R5 + 2);

    R0 = phase;
    R1 = LD32(A_off_800798C + phase); /* handler address, exactly as the table holds it */

    switch (phase) {
    case 0x00: /* cue something, then set the battle state to 4 */
        GUEST_CALL(battle_cueThenSetState4_800799C);
        break;
    case 0x04:
        GUEST_CALL(battle_state_endPhaseWithEffect_80079A8);
        break;
    case 0x08:
        GUEST_CALL(battle_state_finishBattlePhase_80079d0);
        break;
    case 0x0C:
        GUEST_CALL(battle_RevealFieldColumnsStep_80079F0);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_cueThenSetState4_800799C ── */
/* battle_cueThenSetState4_800799C (r5 = battle state):
 * Play the cue handled by sub_801FE24, then mark the battle state by
 * storing 4 into its byte at offset 2. Flags are left as the final
 * "mov r0, #4" leaves them (N/Z from the value 4, C/V untouched). */
Next battle_cueThenSetState4_800799C(void) {
    GUEST_CALL(sub_801FE24);

    R0 = 4;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 2, R0);

    RETURN_TO_CALLER();
}

/* ── battle_state_endPhaseWithEffect_80079A8 ── */
/* battle_state_endPhaseWithEffect_80079A8: finish the current battle phase for the battle state in R5.
 *
 * Reads the active battle-effect bitfield; bit 3 (overlay effect) means the
 * result byte comes from sub_803DD60, otherwise no effect is recorded. The
 * byte is written to the battle state and to eCamera.unk_4C (0x20099CC), the
 * battle scene is refreshed via battle_initBattleStateBuffer_800B144, and finally the battle state's
 * phase byte is set to 8, which battle_dispatchPhase_8007978 picks up to run
 * battle_state_finishBattlePhase.
 *
 * inputs:  r2 r3 r4 r5 (battle state ptr) r6 r7 r10 lr
 * outputs: nz c v r0 r1 r2 r3 r4 r5 r6 r7 lr
 *          nz from the final "mov r0,#8"; c/v are whatever the guest calls
 *          leave behind (untouched on this level).
 * preserved: r4 r5 r6 r7
 */
Next battle_state_endPhaseWithEffect_80079A8(void) {
    u32 effect;

    /* Which battle effects are active? Bit 3 signals a special result. */
    GUEST_CALL(GetBattleEffects);            /* -> R0 = effect bitfield */

    if (R0 & 8)
        GUEST_CALL(sub_803DD60);             /* -> R0 = effect byte */
    else
        R0 = 0;                              /* no effect recorded */

    effect = R0;

    /* Record the effect byte in the battle state and in the camera. */
    ST8(R5 + oBattleState_Unk_0d, effect);
    R1 = 0x20099CC;                          /* eCamera + 0x4C (eCamera.unk_4C) */
    ST8(R1, effect);

    /* Refresh the battle scene / camera state. */
    GUEST_CALL(battle_initBattleStateBuffer_800B144);

    /* Mark the battle phase as finished (phase 8; MOV sets NZ). */
    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 2, R0);

    RETURN_TO_CALLER();
}

/* ── battle_state_finishBattlePhase_80079d0 ── */
/* battle_state_finishBattlePhase_80079d0: battle-end phase hook.
 *
 * Asks the battle-state manager (battle_doBothTrackedIdsMatch_800B46C) whether the battle is finished,
 * using the magic key 0x12345678 as the query token. If it reports "not done",
 * there is nothing to do. Otherwise the battle scene is torn down
 * (battle_resetLinkSyncFlags_800B460), all battle structs including the battle statistics are
 * copied back (battle_copyStructsIncludingBattleStats_800b2d8), and the
 * battle state's phase byte at offset 0x02 is set to 0x0C to trigger the
 * follow-up handler via battle_dispatchPhase_8007978.
 *
 * r5 = battle state struct. r4, r6, r7, r10 are passed through untouched.
 * Flags on return: N/Z always reflect R0 (0 on the early-exit path, 0x0C on
 * the success path); C/V are whatever the last called helper left. */
Next battle_state_finishBattlePhase_80079d0(void) {
    R0 = 0x12345678;
    GUEST_CALL(battle_doBothTrackedIdsMatch_800B46C);

    if (R0 == 0) {
        /* Battle not finished yet: report "zero" to the caller. */
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_resetLinkSyncFlags_800B460);
    GUEST_CALL(battle_copyStructsIncludingBattleStats_800b2d8);

    R0 = 0xC;
    SET_FLAGS_NZ(R0);
    ST8(R5 + 2 /* oBattleState_Unk_02 */, R0);

    RETURN_TO_CALLER();
}

/* ── battle_RevealFieldColumnsStep_80079F0 ── */
/* battle_RevealFieldColumnsStep_80079F0: run one step of the "battle field reveal" sequence.
 * Draws the column pattern for the current battle field (r5 = battle state),
 * then advances the battle state script to state 8 with a cleared timer.
 * Returns with Z set (callers branch on it). */
Next battle_RevealFieldColumnsStep_80079F0(void) {
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

/* ── battle_initEntry_8007A0C ── */
/* register form: calls collision_resetAllHitData_801986C outside this file, which has no typed C interface yet */
static void call_reset_all_hit_data(void) {
    GUEST_CALL(collision_resetAllHitData_801986C);
}

/* register form: calls sub_800C8F0 outside this file, which has no typed C interface yet */
static void call_sub_800C8F0(void) {
    GUEST_CALL(sub_800C8F0);
}

/* register form: calls clearBattleObjectLinkedLists_800318C outside this file, which has no typed C interface yet */
static void call_clear_object_lists(void) {
    GUEST_CALL(clearBattleObjectLinkedLists_800318C);
}

/* register form: calls panel_800BFC4 outside this file, which has no typed C interface yet */
static void call_panel_update(void) {
    GUEST_CALL(panel_800BFC4);
}

/* register form: calls initUncompSpriteState_80028d4 outside this file, which has no typed C interface yet */
static void call_init_sprite_state(u32 screen_state) {
    R0 = screen_state;
    GUEST_CALL(initUncompSpriteState_80028d4);
}

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 call_get_battle_effects(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls decompSprites_80029A8 outside this file, which has no typed C interface yet */
static void call_decomp_sprites(u32 data) {
    R0 = data;
    GUEST_CALL(decompSprites_80029A8);
}

/* register form: calls battle_clearEnemyFadeinList outside this file, which has no typed C interface yet */
static void call_clear_enemy_fadein_list(void) {
    GUEST_CALL(battle_clearEnemyFadeinList);
}

/* Perform all one-time setup when entering a new battle.
 * Clears hit data, resets object/panel lists, initialises the logo-sprite
 * decompressor, optionally loads extra sprite data (unless bit 3 of the
 * battle-effects flags is set), empties the enemy fade-in queue, and
 * writes index 4 so the main loop advances to battle_update_8007A44. */
static void setup_battle_entry(u32 battle_state) {
    call_reset_all_hit_data();
    call_sub_800C8F0();
    call_clear_object_lists();
    call_panel_update();
    call_init_sprite_state(A_eLogoScreenState);

    /* Skip sprite decomp when bit 3 of the battle-effects word is set */
    u32 effects = call_get_battle_effects();
    if (!(effects & 8)) {
        u32 battle_settings = LD32(battle_state + oBattleState_BattleSettings);
        call_decomp_sprites(LD32(battle_settings + 0xC));
    }

    call_clear_enemy_fadein_list();

    /* Set the phase index to 4 → battle_main dispatches battle_update_8007A44 */
    ST32(battle_state + oBattleState_Index_00, 4);
}

/* battle_initEntry_8007A0C(r5 = *BattleState) -> r0 = 4, r1 from callee, nz/c/v */
Next battle_initEntry_8007A0C(void) {
    setup_battle_entry(R5);
    R0 = 4;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8007B80 ── */
/* Dispatch the current battle-state phase handler via the two-entry jump
 * table at off_8007B94.  The byte at BattleState::index_01 (offset 1)
 * selects:  0 → battle_runFrame_8007B9C,  4 → battle_state_tick_8007CA0.
 * r0 must hold the raw phase index when the handler enters (the handler
 * reads it), matching the original ldrb r0 / bx r1 sequence. */

/* register form: the jump-table handler expects r0 = phase index byte;
 * no typed wrapper covers a bx-to-computed-address call, and r5 (self)
 * is already placed by the caller. */
static void battle_state_dispatch(u32 self) {
    R0 = LD8(self + oBattleState_Index_01);
    u32 handler = LD32(A_off_8007B94 + R0);
    GUEST_CALL_ADDR(handler);
}

/* sub_8007B80(r5 = *BattleState) -> (r5 r6 r7 r8 r9 r10 r11 r12 after handler) */
Next sub_8007B80(void) {
    battle_state_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── battle_stateCountdownTick_8007BD0 ── */
/* battle_stateCountdownTick_8007BD0: battle-state countdown tick.
 * r5 = battle state object. If its phase byte (offset 3) is still zero,
 * start a 10-frame countdown and advance the phase to 4. Otherwise count
 * the halfword at oBattleState_Unk_28 down by one; when it underflows,
 * write 4 to the halfword at offset 2 (oBattleState_Unk_02_03).
 * R0 ends as 4 when the countdown was started or finished, otherwise as
 * the decremented timer value. Flags come from the last flag-setting
 * instruction of the taken path: MOVS #4 (nz only) on the start/expire
 * paths, and the SUBS #1 on the still-counting path. */
Next battle_stateCountdownTick_8007BD0(void) {
    if (LD8(R5 + 3) != 0) {
        /* countdown already running: tick it down once */
        u32 prev = LD16(R5 + oBattleState_Unk_28);
        u32 timer = prev - 1;
        ST16(R5 + oBattleState_Unk_28, timer);
        if ((s32)timer >= 0) {
            /* still counting: flags are those of the subtract */
            R0 = timer;
            SET_FLAGS_CMP(prev, 1);
        } else {
            /* expired: switch the state's phase word to 4 */
            ST16(R5 + 2, 4);
            R0 = 4;
            SET_FLAGS_CMP(prev, 1); /* c/v from the subtract */
            SET_FLAGS_NZ(R0);       /* nz from the final MOVS #4 */
        }
    } else {
        /* not started yet: set timer to 10 and enter phase 4 */
        ST16(R5 + oBattleState_Unk_28, 0xA);
        R0 = 4;
        ST8(R5 + 3, R0);
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── eStruct200BC30_dispatchJumpOffset00_8007BF0 ── */
/* eStruct200BC30_dispatchJumpOffset00_8007BF0: dispatch to an e-struct jump handler.
 *
 * eStruct200BC30_getJumpOffset00 decides which handler applies and returns a
 * byte offset into the jump table at off_8007C04; this routine then jumps to
 * the selected entry. The `mov lr, pc / bx r0 / pop {pc}` sequence is just a
 * tail call: the handler returns straight to eStruct200BC30_dispatchJumpOffset00_8007BF0's own caller, so
 * every register (and the flags) the handler leaves behind are this
 * routine's results.
 *
 * Table (off_8007C04): entry 0 -> battle_stepSubstateToState4_8007C14, entry 1 -> sub_8007C50,
 *                      entry 2 -> sub_8007C50, entry 3 -> battle_stepSubstateToState4_8007C14.
 */
Next eStruct200BC30_dispatchJumpOffset00_8007BF0(void) {
    /* Ask the e-struct which handler applies; leaves the table offset in R0. */
    GUEST_CALL(eStruct200BC30_getJumpOffset00);

    /* Pick the handler address out of the table. The original loads the
     * table base into R1 and the selected entry into R0 before jumping, and
     * both are live for the callee, so reproduce that register state. */
    u32 handler = LD32(A_off_8007C04 + R0);
    R0 = handler;
    R1 = A_off_8007C04;

    /* The table only ever holds the two Thumb entry points, and entries 0
     * and 3 are both battle_stepSubstateToState4_8007C14+1, so matching against the first entry
     * separates battle_stepSubstateToState4_8007C14 from sub_8007C50. */
    if (handler == LD32(A_off_8007C04))
        GUEST_CALL(battle_stepSubstateToState4_8007C14);
    else
        GUEST_CALL(sub_8007C50);

    RETURN_TO_CALLER();
}

/* ── battle_stepSubstateToState4_8007C14 ── */
/* battle_stepSubstateToState4_8007C14: one step of the battle-state machine at R5.
 *
 * If the current state's handler (battle_getSubstateFinishedFlag_801FE5E) reports it is still running,
 * the state is kept alive: on its first step it is initialized by
 * sub_803DDA4 and marked with sub-state 4; afterwards sub_803DE24 is polled
 * and, unless it reports completion for an object of kind 2 (byte at R4+1),
 * the state is torn down (sub_81440D8, sub_801FE64). Once the state ends
 * (or the handler already reported idle), control passes to battle state 4
 * by updating the state fields at R5.
 *
 * Preserves r4-r7; returns flags and r0 as the original does. */
Next battle_stepSubstateToState4_8007C14(void) {
    /* Is the current state's handler still busy? */
    GUEST_CALL(battle_getSubstateFinishedFlag_801FE5E);
    if (R0 != 0) {
        if (LD8(R5 + 3) == 0) {
            /* First step in this state: run its init routine and mark it
             * as started with sub-state 4. */
            GUEST_CALL(sub_803DDA4);
            R0 = 4;
            ST8(R5 + 3, R0);
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }

        /* May the state finish now? */
        GUEST_CALL(sub_803DE24);
        if (R0 != 0) {
            R0 = LD8(R4 + 1);
            if (R0 == 2) {
                /* Kind-2 objects hold the state open: stop here. */
                SET_FLAGS_CMP(R0, 2);
                RETURN_TO_CALLER();
            }
        }

        /* State is over: clean up and notify the handler. */
        GUEST_CALL(sub_81440D8);
        GUEST_CALL(sub_801FE64);
    }

    /* Advance to battle state 4 and clear the pending sub-state fields. */
    R0 = 4;
    ST8(R5 + 1, R0);
    R0 = 0;
    ST16(R5 + 2, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
