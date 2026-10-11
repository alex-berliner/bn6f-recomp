/*
 * asm00_1_8005C04.c: readable functions of bn6f asm00_1.s, 0x08005C04-0x0800801C, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

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

/* ── sub_8005DBE ── */
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

/* register form: calls sub_80027C4 outside this file, which has no typed C interface yet */
static void call_sub_80027C4_sub_8005DBE(void) {
    GUEST_CALL(sub_80027C4);
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

/* sub_8005DBE(r10 = toolkit) -> preserves r4-r12 */
Next sub_8005DBE(void) {
    battle_setup_sequence(R10);
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
    GUEST_CALL(sub_80AA88C);

    RETURN_TO_CALLER();
}

/* ── spawnMegaMan_80073CC ── */
/* spawnMegaMan_80073CC(r6 = *EnemySetup):
 * Spawn Mega Man (via sub_800753C) at the panel encoded in the setup's
 * position byte -- low 3 bits are the panel x, bits 4+ the panel y -- on
 * the side of the field given by the setup's alliance flag (bit 0).
 * Flags on exit are whatever sub_800753C leaves. */
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
    GUEST_CALL(sub_800753C);

    RETURN_TO_CALLER();
}

/* ── sub_800753C ── */
/* sub_800753C: spawn a battle object at a panel, initialize it, allocate AI data,
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

/* sub_800753C(r0=panel_x, r1=panel_y, r2=alliance) -> r0 r1 r2 r3, nz c v, r4 r5 r6 r7 preserved */
Next sub_800753C(void) {
    spawn_battle_object(R0, R1, R2);
    RETURN_TO_CALLER();
}

/* ── sub_8007628 ── */
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

/* sub_8007628(r10 = toolkit base) -> r0-r3, nzcv */
Next sub_8007628(void) {
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
 * battle scene is refreshed via sub_800B144, and finally the battle state's
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
    GUEST_CALL(sub_800B144);

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
 * (sub_800B460), all battle structs including the battle statistics are
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

    GUEST_CALL(sub_800B460);
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

/* ── sub_8007A0C ── */
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
    call_init_sprite_state(A_eLogoScreenState_Index_00);

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

/* sub_8007A0C(r5 = *BattleState) -> r0 = 4, r1 from callee, nz/c/v */
Next sub_8007A0C(void) {
    setup_battle_entry(R5);
    R0 = 4;
    SET_FLAGS_NZ(R0);
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
 * If the current state's handler (sub_801FE5E) reports it is still running,
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
    GUEST_CALL(sub_801FE5E);
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

/* ── sub_8007CA0 ── */
/* register form: calls musicGameState_8000784 outside this file, which has no typed C interface yet */
static void tick_music_game_state(void) {
    GUEST_CALL(musicGameState_8000784);
}

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 get_battle_effects_sub_8007CA0(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls battle_getStateByteNibbles_800A832 outside this file */
static u32 get_state_nibbles(void) {
    GUEST_CALL(battle_getStateByteNibbles_800A832);
    return R0;
}

/* register form: sub_800AF50 reads the S2001C04 pointer from r4, no typed C interface yet */
static u32 check_battle_completion(u32 s2001c04) {
    R4 = s2001c04;
    GUEST_CALL(sub_800AF50);
    return R0;
}

/* register form: calls SetDummyBGScrollCallbacks outside this file */
static void set_dummy_bg_scroll(void) {
    GUEST_CALL(SetDummyBGScrollCallbacks);
}

/* register form: calls zeroFill_e20094C0 outside this file */
static void zero_fill_94c0(void) {
    GUEST_CALL(zeroFill_e20094C0);
}

/* register form: calls palette_clearEPalette20097a0_80023A8 outside this file */
static void clear_palette(void) {
    GUEST_CALL(palette_clearEPalette20097a0_80023A8);
}

/* register form: calls battleSettings_802D2B2 outside this file */
static u32 get_battle_settings_ptr(void) {
    GUEST_CALL(battleSettings_802D2B2);
    return R0;
}

/* register form: calls loc_8007204 outside this file, which has no typed C interface yet */
static void init_battle_display(u32 settings) {
    R0 = settings;
    GUEST_CALL(loc_8007204);
}

/* register form: calls setTwoStructs_800A840 outside this file */
static void set_two_structs(u32 mode) {
    R0 = mode;
    GUEST_CALL(setTwoStructs_800A840);
}

/* register form: calls setByte_200AD04_803F4EC outside this file */
static void set_byte_200ad04(u32 val) {
    R0 = val;
    GUEST_CALL(setByte_200AD04_803F4EC);
}

/* register form: calls TestEventFlagFromImmediate outside this file (flag 0x1733) */
static bool flag_1733_set(void) {
    R0 = 0x17;
    R1 = 0x33;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls loc_80071FE outside this file */
static void restore_battle_settings(u32 settings) {
    R0 = settings;
    GUEST_CALL(loc_80071FE);
}

/* register form: calls battle_snapshotPlayerHP_800FAE0 outside this file */
static void snapshot_player_hp(void) {
    GUEST_CALL(battle_snapshotPlayerHP_800FAE0);
}

/* register form: calls sub_800A86E outside this file */
static void battle_sub_800A86E(void) {
    GUEST_CALL(sub_800A86E);
}

/* register form: calls GetCurPETNavi outside this file */
static u32 get_cur_pet_navi_sub_8007CA0(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls SetCurPETNaviStatsHword outside this file */
static void set_pet_navi_hword(u32 navi, u32 offset, u32 value) {
    R0 = navi;
    R1 = offset;
    R2 = value;
    GUEST_CALL(SetCurPETNaviStatsHword);
}

/* register form: callPossiblyGetBattleEmotion_8015B54 returns the emotion value in r2 */
static u32 get_emotion_value(u8 alliance) {
    R0 = alliance;
    GUEST_CALL(callPossiblyGetBattleEmotion_8015B54);
    return R2;
}

/* register form: calls SetCurPETNaviStatsByte outside this file */
static void set_pet_navi_byte(u32 navi, u32 field, u32 value) {
    R0 = navi;
    R1 = field;
    R2 = value;
    GUEST_CALL(SetCurPETNaviStatsByte);
}

/* register form: calls GetBattleNaviStatsByte outside this file */
static u32 get_battle_navi_byte(u32 alliance, u32 field) {
    R0 = alliance;
    R1 = field;
    GUEST_CALL(GetBattleNaviStatsByte);
    return R0;
}

/* register form: calls table_GetByte16Row_203EAE0_800AB3A outside this file */
static u32 table_get_byte16_row(u32 alliance, u32 row) {
    R0 = alliance;
    R1 = row;
    GUEST_CALL(table_GetByte16Row_203EAE0_800AB3A);
    return R0;
}

/* register form: calls battle_updateSlotBestRecords_802C9B8 outside this file */
static void update_slot_best_records(void) {
    GUEST_CALL(battle_updateSlotBestRecords_802C9B8);
}

/* register form: calls SetCurNaviHPToFull outside this file */
static void set_navi_hp_full(void) {
    GUEST_CALL(SetCurNaviHPToFull);
}

/* register form: calls eStruct200A008_redrawOnDirtyFlags_802CA82 outside this file */
static void redraw_on_dirty_flags(void) {
    GUEST_CALL(eStruct200A008_redrawOnDirtyFlags_802CA82);
}

/* register form: calls zeroFill_e20097A0 outside this file */
static void zero_fill_97a0(void) {
    GUEST_CALL(zeroFill_e20097A0);
}

/* register form: calls SetRenderInfoLCDControl outside this file */
static void set_render_lcd(u16 ctrl) {
    R0 = ctrl;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* register form: calls battle_tickCustomScreenEffect_800A892 outside this file */
static void tick_screen_effect(void) {
    GUEST_CALL(battle_tickCustomScreenEffect_800A892);
}

/* register form: calls clear_flags32_20093A4 outside this file */
static void clear_flags32_one(u32 flags) {
    R0 = flags;
    GUEST_CALL(clear_flags32_20093A4);
}

/* register form: calls ClearEventFlagFromImmediate outside this file (flag 0x1722) */
static void clear_flag_1722(void) {
    R0 = 0x17;
    R1 = 0x22;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls SetPrimaryToolkitPointers outside this file */
static void set_primary_toolkit_ptrs(void) {
    GUEST_CALL(SetPrimaryToolkitPointers);
}

/*
 * Main battle state tick.  Called each frame while the state is "battle".
 * Handles battle-end detection, link/battle mode setup, Navi stat updates
 * (emotion, mood, HP), and post-battle cleanup.
 *
 * battle_state: pointer to the active BattleState (was r5)
 * toolkit:      pointer to the Toolkit struct      (was r10)
 */
static void battle_state_tick(u32 battle_state, u32 toolkit) {
    /* Advance music game-state */
    tick_music_game_state();

    /* Load commonly-used toolkit sub-pointers */
    u32 s2001c04 = LD32(toolkit + oToolkit_S2001c04_Ptr);
    u32 jumptable = LD32(toolkit + oToolkit_MainJumptableIndexPtr);
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);

    /* --- Battle-end check (effect bit 0x400) --- */
    u32 effects = get_battle_effects_sub_8007CA0();
    if (effects & 0x400) {
        u32 state = get_state_nibbles();
        if (state != 5 && state != 9 && state != 0xA) {
            u32 completion = check_battle_completion(s2001c04);
            if (completion == 0) {
                /* Battle finished: clear the visual and restore settings */
                set_dummy_bg_scroll();
                zero_fill_94c0();
                clear_palette();
                u32 saved_settings = LD32(battle_state + oBattleState_Unk_18);
                u32 ptr = get_battle_settings_ptr();
                (void)ptr; /* callee result unused, call still needed for side-effects */
                init_battle_display(LD32(battle_state + oBattleState_Unk_18));
                ST32(battle_state + oBattleState_Unk_18, saved_settings);
                return;
            } else {
                /* Determine link/battle mode from completion code */
                u32 mode;
                if (completion == 2)
                    mode = 3;
                else if (completion == 1)
                    mode = 1;
                else
                    mode = 2;
                set_two_structs(mode);
            }
        }
    }

    /* --- Intro state: increment a counter while state == 1 --- */
    u32 state = get_state_nibbles();
    if (state == 1) {
        u32 counter_addr = A_dword_2000B30;
        u32 counter = LD16(counter_addr);
        if ((s32)counter < 0x11) {
            ST16(counter_addr, counter + 1);
        }
    }

    /* --- State 5: post-battle / transition handling --- */
    if (state == 5) {
        effects = get_battle_effects_sub_8007CA0();
        if (effects & 8) {
            goto cleanup;
        }
        set_byte_200ad04(0);
        if (!flag_1733_set()) {
            goto cleanup;
        }
        /* Flag set: tear down the battle display */
        set_dummy_bg_scroll();
        zero_fill_94c0();
        clear_palette();
        u32 settings = LD32(battle_state + oBattleState_BattleSettings);
        restore_battle_settings(settings);
        return;
    }

    /* --- State != 5: active-battle logic --- */
    snapshot_player_hp();
    battle_sub_800A86E();

    /* Effect bit 0x40: update Navi HP hword */
    effects = get_battle_effects_sub_8007CA0();
    if (effects & 0x40) {
        u32 navi = get_cur_pet_navi_sub_8007CA0();
        u32 hword_val = LD16(battle_state + oBattleState_Unk_34);
        if (hword_val == 0) hword_val = 1;
        set_pet_navi_hword(navi, 0x40, hword_val);
    }

    /* Effect bit 8: skip Navi mood/emotion block entirely */
    effects = get_battle_effects_sub_8007CA0();
    if (!(effects & 8)) {
        /* --- Emotion / mood handling --- */
        u32 emotion_r2 = 0x80;
        if (!(effects & 0x10000)) {
            u8 alliance = (u8)LD8(battle_state + oBattleState_Unk_0d);
            emotion_r2 = get_emotion_value(alliance);
        }
        u32 navi = get_cur_pet_navi_sub_8007CA0();
        set_pet_navi_byte(navi, 0xE, emotion_r2);

        /* --- Mood byte (field 0x21): default 3, or stats+1 if table row == 0 --- */
        u32 mood = 3;
        effects = get_battle_effects_sub_8007CA0();
        if (!(effects & 0x400000)) {
            u8 alliance = (u8)LD8(battle_state + oBattleState_Unk_0d);
            mood = get_battle_navi_byte(alliance, 0x21);
            u32 tbl = table_get_byte16_row(alliance, 2);
            if (tbl == 0 && (s32)mood < 3) {
                mood = mood + 1;
            }
        }
        navi = get_cur_pet_navi_sub_8007CA0();
        set_pet_navi_byte(navi, 0x21, mood);

        /* If entering a new battle (state 1), update best records */
        state = get_state_nibbles();
        if (state == 1) {
            update_slot_best_records();
        }

        /* Clear S2001C04 transition counters */
        ST16(s2001c04 + 0x12, 0);
        ST16(s2001c04 + 0x14, 0);
    }

    /* --- Navi HP / special stats when state == 2 (link completed) --- */
    state = get_state_nibbles();
    if (state == 2) {
        effects = get_battle_effects_sub_8007CA0();
        if (effects & 0x800000) {
            u32 tmp = effects; /* preserve effects across callee that clobbers r2 */
            (void)tmp;
            set_navi_hp_full();
        }
        if (effects & 0x1000) {
            u32 navi = get_cur_pet_navi_sub_8007CA0();
            set_pet_navi_byte(navi, 0x21, 3);
        }
        if (effects & 0x40000) {
            u32 navi = get_cur_pet_navi_sub_8007CA0();
            set_pet_navi_byte(navi, 0xE, 0x80);
        }
    }

    /* Effect bit 0x10: unconditional full HP */
    effects = get_battle_effects_sub_8007CA0();
    if (effects & 0x10) {
        set_navi_hp_full();
    }

    /* Redraw the Navi HUD */
    redraw_on_dirty_flags();

cleanup:
    /* --- Post-battle teardown and screen reset --- */
    ST8(game_state + oGameState_BattlePaused, 0);
    zero_fill_97a0();
    set_render_lcd(0x40);
    tick_screen_effect();
    clear_flags32_one(1);
    clear_flag_1722();
    ST8(battle_state + oBattleState_Unk_0a, 0);
    set_primary_toolkit_ptrs();
}

/* sub_8007CA0(self: BattleState* r5, Toolkit* r10) -> void */
Next sub_8007CA0(void) {
    battle_state_tick(R5, R10);
    RETURN_TO_CALLER();
}
