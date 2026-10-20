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

/* ── gfxTransfer_8033978 ── */
/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx_transfer(const void *src, void *dest, u32 size) {
    R0 = (u32)src;
    R1 = (u32)dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls gfxTransfer_8033DA0 outside this file, which has no typed C interface yet */
static void call_gfx_transfer_8033DA0(u32 a0) {
    R0 = a0;
    GUEST_CALL(gfxTransfer_8033DA0);
}

/* Transfer graphics into VRAM for the current map group.
 * Internet maps load a richer sprite/font set; regular maps load a smaller one
 * and then trigger a follow-up transfer. */
static void load_map_gfx(void) {
    /* register form: read toolkit/game state through R10 to match the original */
    u32 toolkit = LD32(R10 + oToolkit_GameStatePtr);
    u8 map_group = LD8(toolkit + oGameState_MapGroup);

    if ((s32)map_group >= (s32)INTERNET_MAP_GROUP_START) {
        /* Internet map: load sprites (0x340 bytes) and a small extra block (0x20 bytes). */
        queue_gfx_transfer((const void *)A_byte_86C1F80, (void *)0x6017200, 0x340);
        queue_gfx_transfer((const void *)A_byte_86C1F20, (void *)A_byte_30016D0, 0x20);
    } else {
        /* Regular map: load the standard sprite block, then run the follow-up transfer. */
        queue_gfx_transfer((const void *)A_dword_86C0D20, (void *)0x6017200, 0x100);
        call_gfx_transfer_8033DA0(0);
    }
}

Next gfxTransfer_8033978(void) {
    load_map_gfx();
    RETURN_TO_CALLER();
}

/* ── dispatch_80339CC ── */
/*
 * PET menu scene dispatcher (singleton scene state eS2011E30).
 *
 * Each tick this runs the handler selected by the scene's state byte
 * (oS2011E30_Idx_04). The real-world and internet variants of the scene
 * have separate handler tables; in the real world the CopyBot / PetNavi
 * event flags choose between the two table entries allocated per state
 * (PetNavi active picks the even entry, otherwise the odd one). If the
 * PET menu data check fails the scene does nothing this tick.
 *
 * The original wraps the whole dispatch in push/pop {r4-r7}: the handlers
 * are guest code that clobbers r4-r7 (and r5 is even repurposed to hold
 * the scene object), so the caller's r4-r7 always come back unchanged.
 */

/* register form: calls TestPETMenuDataFlag outside this file, which has no typed C interface yet */
static bool pet_menu_data_flag_set(u32 mode) {
    R0 = mode;
    GUEST_CALL(TestPETMenuDataFlag);
    return !ZERO_FLAG();          /* Z clear: check says "skip the scene update" */
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_dispatch_80339CC(u32 bank, u32 flag) {
    R0 = bank;                    /* movflag loads the flag bank into r0 ... */
    R1 = flag;                    /* ... and the flag index into r1 */
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls callback_noop_8033A7C outside this file, which has no typed C interface yet */
static void call_callback_noop_8033A7C(u32 self) {
    R5 = self;
    GUEST_CALL(callback_noop_8033A7C);
}

/* register form: calls battle_initBattleStartOverlay_8033A80 outside this file, which has no typed C interface yet */
static void call_sub_8033A80(u32 self) {
    R5 = self;
    GUEST_CALL(battle_initBattleStartOverlay_8033A80);
}

/* register form: calls onUpdateInRealWorld_8033A96 outside this file, which has no typed C interface yet */
static void call_onUpdateInRealWorld_8033A96(u32 self) {
    R5 = self;
    GUEST_CALL(onUpdateInRealWorld_8033A96);
}

/* register form: calls battle_runStartAnimFrame outside this file, which has no typed C interface yet */
static void call_sub_8033AB0(u32 self) {
    R5 = self;
    GUEST_CALL(battle_runStartAnimFrame);
}

/* register form: calls battle_resetState_8033AC4 outside this file, which has no typed C interface yet */
static void call_battle_resetState_8033AC4(u32 self) {
    R5 = self;
    GUEST_CALL(battle_resetState_8033AC4);
}

/* register form: calls battle_stepBattleStartOverlay outside this file, which has no typed C interface yet */
static void call_sub_8033ADC(u32 self) {
    R5 = self;
    GUEST_CALL(battle_stepBattleStartOverlay);
}

/* register form: calls battle_queueBattleIntroOam outside this file, which has no typed C interface yet */
static void call_sub_8033AF0(u32 self) {
    R5 = self;
    GUEST_CALL(battle_queueBattleIntroOam);
}

/* register form: calls noop_8033B08 outside this file, which has no typed C interface yet */
static void call_noop_8033B08(u32 self) {
    R5 = self;
    GUEST_CALL(noop_8033B08);
}

/* register form: calls ow_drawInternetHud outside this file, which has no typed C interface yet */
static void call_sub_8033B0C(u32 self) {
    R5 = self;
    GUEST_CALL(ow_drawInternetHud);
}

/* register form: calls onUpdateInInternet_8033B1E outside this file, which has no typed C interface yet */
static void call_onUpdateInInternet_8033B1E(u32 self) {
    R5 = self;
    GUEST_CALL(onUpdateInInternet_8033B1E);
}

/* register form: calls internet_updateOverworldHud outside this file, which has no typed C interface yet */
static void call_sub_8033B30(u32 self) {
    R5 = self;
    GUEST_CALL(internet_updateOverworldHud);
}

/* register form: calls internet_redrawOverworldUi outside this file, which has no typed C interface yet */
static void call_sub_8033B46(u32 self) {
    R5 = self;
    GUEST_CALL(internet_redrawOverworldUi);
}

/* register form: calls ow_drawHudOverlays outside this file, which has no typed C interface yet */
static void call_sub_8033B5C(u32 self) {
    R5 = self;
    GUEST_CALL(ow_drawHudOverlays);
}

/* register form: calls sub_8033B6E outside this file, which has no typed C interface yet */
static void call_sub_8033B6E(u32 self) {
    R5 = self;
    GUEST_CALL(sub_8033B6E);
}

/* Run the real-world handler for the table byte offset `slot`
 * (JumpTable8033A1C entry slot / 4). The scene state byte only reaches 5,
 * so slots stay within the 11-entry table. */
static void run_real_world_handler(u32 slot, u32 self) {
    switch (slot) {
    case 0:                       /* entry 0: state 0 */
        call_callback_noop_8033A7C(self);
        break;
    case 4: case 36: case 40:     /* entries 1, 9, 10 */
        call_sub_8033A80(self);
        break;
    case 8: case 28: case 32:     /* entries 2, 7, 8 */
        call_onUpdateInRealWorld_8033A96(self);
        break;
    case 12:                      /* entry 3 */
        call_sub_8033AB0(self);
        break;
    case 16:                      /* entry 4 */
        call_battle_resetState_8033AC4(self);
        break;
    case 20:                      /* entry 5 */
        call_sub_8033ADC(self);
        break;
    case 24:                      /* entry 6 */
        call_sub_8033AF0(self);
        break;
    }
}

/* Run the internet handler for the table byte offset `slot`
 * (JumpTableInternet8033A4C entry slot / 4). */
static void run_internet_handler(u32 slot, u32 self) {
    switch (slot) {
    case 0:                       /* entry 0: state 0 */
        call_noop_8033B08(self);
        break;
    case 4: case 36: case 40:     /* entries 1, 9, 10 */
        call_sub_8033B0C(self);
        break;
    case 8: case 28: case 32:     /* entries 2, 7, 8 */
        call_onUpdateInInternet_8033B1E(self);
        break;
    case 12:                      /* entry 3 */
        call_sub_8033B30(self);
        break;
    case 16:                      /* entry 4 */
        call_sub_8033B46(self);
        break;
    case 20:                      /* entry 5 */
        call_sub_8033B5C(self);
        break;
    case 24:                      /* entry 6 */
        call_sub_8033B6E(self);
        break;
    }
}

/* Advance the PET menu scene by one tick. The handler callees leave their
 * results in the guest registers (r2, r3, r8-r12 pass through untouched);
 * r4-r7 are restored by the adapter, matching the original's pop. */
static void dispatch_pet_menu_scene(u32 toolkit) {
    if (pet_menu_data_flag_set(1))
        return;

    u32 self = A_eS2011E30;
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 mapGroup = LD8(gameState + oGameState_MapGroup);

    if ((s32)mapGroup >= (s32)INTERNET_MAP_GROUP_START) {
        /* Internet: the state byte indexes the internet table directly. */
        u32 slot = (u32)LD8(self + oS2011E30_Idx_04) * 8;
        run_internet_handler(slot, self);
        return;
    }

    /* Real world: each state owns two table entries; the CopyBot/PetNavi
     * event flags pick between them. State 0 always lands on entry 0 and
     * never consults the flags. */
    u32 state = LD8(self + oS2011E30_Idx_04);
    u32 slot;
    if (state == 0)
        slot = 0;
    else if (event_flag_set_dispatch_80339CC(0x17, 0x29))      /* EVENT_COPYBOT_ACTIVE */
        slot = state * 8 - 4;
    else if (event_flag_set_dispatch_80339CC(0x17, 0x1D))      /* EVENT_PET_NAVI_ACTIVE */
        slot = state * 8;
    else
        slot = state * 8 - 4;                 /* neither flag: odd entry */

    run_real_world_handler(slot, self);
}

/* dispatch_80339CC(r10 = toolkit) -> c, v from the last callee; r2, r3,
 * r8-r12 pass through from the callee; r4-r7 preserved (the original's
 * push/pop {r4-r7} shields them from the handler callees). */
Next dispatch_80339CC(void) {
    /* push {r4-r7,lr}: the handlers are guest code and trash r4-r7. */
    u32 callerR4 = R4, callerR5 = R5, callerR6 = R6, callerR7 = R7;
    dispatch_pet_menu_scene(R10);
    /* pop {r4-r7}: hand the caller's values back, clobbers and all. */
    R4 = callerR4, R5 = callerR5, R6 = callerR6, R7 = callerR7;
    RETURN_TO_CALLER();
}

/* ── callback_noop_8033A7C ── */
/* callback_noop_8033A7C: does nothing; exists only as a callable placeholder
 * (its callers treat it as an empty callback). */
Next callback_noop_8033A7C(void) {
    RETURN_TO_CALLER();
}

/* ── battle_initBattleStartOverlay_8033A80 ── */
Next battle_initBattleStartOverlay_8033A80(void) {
    /* battle_initBattleStartOverlay_8033A80: initialize/resume the three rendering sub-systems with argument 0.
     * Calls sprite_queueOamByFrameIndex_8033B80, battle_drawBattleStartOverlay_8033F80 and gfxTransfer_8033DA0 in sequence, each with r0 = 0. */
    R0 = 0;
    GUEST_CALL(sprite_queueOamByFrameIndex_8033B80);

    R0 = 0;
    GUEST_CALL(battle_drawBattleStartOverlay_8033F80);

    R0 = 0;
    GUEST_CALL(gfxTransfer_8033DA0);

    RETURN_TO_CALLER();
}

/* ── onUpdateInRealWorld_8033A96 ── */
/* onUpdateInRealWorld_8033A96(self: *S2011E30 in r5):
 * Per-frame update while in the real world: clear two per-frame state
 * flags, run the real-world update logic, then queue a graphics transfer. */
Next onUpdateInRealWorld_8033A96(void) {
    R0 = 0;
    GUEST_CALL(sprite_queueOamByFrameIndex_8033B80);

    R0 = 0;
    GUEST_CALL(battle_drawBattleStartOverlay_8033F80);

    GUEST_CALL(battleObject_queueRealWorldNaviIconOAM_8033BE8);

    R0 = 0;
    GUEST_CALL(gfxTransfer_8033DA0);

    RETURN_TO_CALLER();
}

/* ── battle_resetState_8033AC4 ── */
/* battle_resetState_8033AC4: reset/initialize the battle object state (self in r5 = *S2011E30).
 * Runs the four initialization steps in order:
 *   1. sprite_queueOamByFrameIndex_8033B80 with r0 = 0
 *   2. battle_drawBattleStartOverlay_8033F80 with r0 = 0
 *   3. updateChipSlotGfx_8033C68
 *   4. battleObject_queueRealWorldNaviIconOAM_8033BE8
 * Register state (r0-r3, flags) after the call is whatever battleObject_queueRealWorldNaviIconOAM_8033BE8 leaves. */
Next battle_resetState_8033AC4(void) {
    R0 = 0;
    GUEST_CALL(sprite_queueOamByFrameIndex_8033B80);

    R0 = 0;
    GUEST_CALL(battle_drawBattleStartOverlay_8033F80);

    GUEST_CALL(updateChipSlotGfx_8033C68);
    GUEST_CALL(battleObject_queueRealWorldNaviIconOAM_8033BE8);

    RETURN_TO_CALLER();
}

/* ── onUpdateInInternet_8033B1E ── */
/* onUpdateInInternet_8033B1E(self: *S2011E30 in r5): per-frame update hook
 * while the player is navigating the Internet. Runs the common Internet
 * update step, flags state 1 for battle_drawBattleStartOverlay_8033F80, then redraws the map name
 * display. All callee flag/register effects are passed through to callers. */
Next onUpdateInInternet_8033B1E(void) {
    GUEST_CALL(internet_drawHpDigits_8033E0C);

    R0 = 1;
    GUEST_CALL(battle_drawBattleStartOverlay_8033F80);

    GUEST_CALL(RenderMapName_8033EE8);

    RETURN_TO_CALLER();
}

/* ── sprite_queueOamByFrameIndex_8033B80 ── */
/* register form: calls sprite_queueOamEntryThunk_802FE28 outside this file, which has no typed C interface yet */
static void call_sub_802FE28_sub_8033B80(u32 a1, u16 a2, int a3, int a4) {
    R0 = a1;
    R1 = a2;
    R2 = a3;
    R3 = a4;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);
}

/* Address of the per-index offset table in guest ROM. Indexed at runtime by
 * (state byte at r5+5) + r0*0x18, so it must be loaded as raw guest memory. */
#define A_byte_8033BB8 0x08033BB8u

/* Apply the per-index offset and advance the state frame counter.
 * The byte at (byte_8033BB8 + state_byte + r0*0x18) is shifted left 16 and added
 * to a fixed base, forming the routine address handed to sprite_queueOamEntryThunk_802FE28.
 * The state's frame counter (byte at state+5) is then incremented and wrapped at 0x18. */
static void apply_offset_and_tick(u32 index, u32 state_ptr) {
    u32 state_byte = LD8(state_ptr + 5);
    u32 table_index = state_byte + index * 0x18u;
    u32 offset = LD8(A_byte_8033BB8 + table_index);
    u32 routine_addr = 0x80028003u + (offset << 16);
    u32 frame = state_byte + 1;

    call_sub_802FE28_sub_8033B80(routine_addr, 0xC790, 1, 0);

    if (frame >= 0x18)
        frame = 0;
    ST8(state_ptr + 5, (u8)frame);
}

Next sprite_queueOamByFrameIndex_8033B80(void) {
    apply_offset_and_tick(R0, R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_queueRealWorldNaviIconOAM_8033BE8 ── */
/* battleObject_queueRealWorldNaviIconOAM_8033BE8: swap in the current PET navi's battle graphic set.
 *
 * Queues the 0x80-byte graphics chunk belonging to the navi currently set on
 * the PET into VRAM at 0x06017380, then triggers the accompanying change
 * through sprite_queueOamEntryThunk_802FE28.  Normally the graphic index comes straight from
 * GetCurPETNavi(); before event flag EVENT_163 has been set, a nonzero value
 * in byte_200AD04 overrides it with slot 12 instead. */
Next battleObject_queueRealWorldNaviIconOAM_8033BE8(void) {
    /* Which navi is currently set on the PET? */
    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* EVENT_163 (passed to the callee as its bank/index pair).  While it is
     * clear, byte 200AD04 being nonzero forces the alternate slot 12. */
    R0 = 1;
    R1 = 0x63;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (ZERO_FLAG()) {
        GUEST_CALL(notZero_eByte200AD04);
        if (!ZERO_FLAG())
            navi = 0xC;
    }

    /* Queue the navi's graphics chunk (source taken from the 13-entry table
     * at off_8033C24, indexed by navi id) into VRAM. */
    R4 = navi << 2;
    R0 = LD32(A_off_8033C24 + R4);
    R1 = 0x6017380;
    R2 = 0x80;
    GUEST_CALL(QueueWordAlignedGFXTransfer);

    /* Kick off the change. */
    R0 = 0x40120006;
    R1 = 0xC79C;
    R2 = 1;
    R3 = 0;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);

    RETURN_TO_CALLER();
}

/* ── gfxTransfer_8033DA0 ── */
/* gfxTransfer_8033DA0(r0 = base address):
 * Queue a 32-byte PET-navi tileset transfer to VRAM buffer byte_30016D0.
 * The source tileset is chosen from a pointer table indexed by the current
 * navi, overridden to entry 0xC when EVENT_163 is clear and the special
 * RAM byte at 0x200AD04 is non-zero. */
Next gfxTransfer_8033DA0(void) {
    u32 basePtr = R0;

    /* Get the current PET navi index */
    GUEST_CALL(GetCurPETNavi);
    u32 tileIdx = R0;

    /* Check EVENT_163 (flag 0x63, bank 1) */
    R0 = 1;
    R1 = 0x63;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (ZERO_FLAG()) {
        /* EVENT_163 is not set; consult the override byte */
        GUEST_CALL(notZero_eByte200AD04);
        if (!ZERO_FLAG())
            tileIdx = 0xC;
    }

    /* Look up the tileset source pointer from the table and add the base */
    R0 = LD32(A_off_8033DD4 + (tileIdx << 2)) + basePtr;
    R1 = A_byte_30016D0;
    R2 = 0x20;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

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

/* ── resetPetNaviSetup_8033FDC ── */
/* register form: calls reloadCurNaviStatBoosts_813c3ac outside this file, which has no typed C interface yet */
static void reload_navi_stat_boosts(void) {
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void clear_event_flag_sub_8033FDC(u32 entry, u32 byte_idx) {
    R0 = entry; R1 = byte_idx;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls SetEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void set_event_flag_sub_8033FDC(u32 entry, u32 byte_idx) {
    R0 = entry; R1 = byte_idx;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool test_event_flag_sub_8033FDC(u32 entry, u32 byte_idx) {
    R0 = entry; R1 = byte_idx;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG(); /* Z clear → flag is set */
}

/* register form: calls ClearEventFlagRangeFromImmediate outside this file, which has no typed C interface yet */
static void clear_event_flag_range(u32 entry, u32 byte_idx, u32 count) {
    R0 = entry; R1 = byte_idx; R2 = count;
    GUEST_CALL(ClearEventFlagRangeFromImmediate);
}

/* register form: calls SetEventFlagRangeFromImmediate outside this file, which has no typed C interface yet */
static void set_event_flag_range(u32 entry, u32 byte_idx, u32 count) {
    R0 = entry; R1 = byte_idx; R2 = count;
    GUEST_CALL(SetEventFlagRangeFromImmediate);
}

/* register form: calls mysteryData_processMapGroups_809F9DC outside this file, which has no typed C interface yet */
static void call_sub_809F9DC(void) {
    GUEST_CALL(mysteryData_processMapGroups_809F9DC);
}

/* register form: calls SetCurNaviHPToFull outside this file, which has no typed C interface yet */
static void set_cur_navi_hp_full(void) {
    GUEST_CALL(SetCurNaviHPToFull);
}

/* register form: calls SetBeastOutCounterTo3 outside this file, which has no typed C interface yet */
static void set_beast_out_counter_3(void) {
    GUEST_CALL(SetBeastOutCounterTo3);
}

/* register form: calls ZeroAllNaviStatsMood outside this file, which has no typed C interface yet */
static void zero_all_navi_stats_mood(void) {
    GUEST_CALL(ZeroAllNaviStatsMood);
}

/* register form: calls clearBattleStateCounter_800A908 outside this file, which has no typed C interface yet */
static void call_sub_800A908(void) {
    GUEST_CALL(clearBattleStateCounter_800A908);
}

/* register form: calls gameState_setUnk09_8001172 outside this file, which has no typed C interface yet */
static void call_sub_8001172(u32 arg0) {
    R0 = arg0;
    GUEST_CALL(gameState_setUnk09_8001172);
}

/* Fully reset the PET Nav state: heal, reload boosts, clear all "active" flags,
 * reset stats/mood/Beast counter, zero toolkit S2001c04 sub-fields,
 * and adjust story flags depending on EVENT_224 and EVENT_1008. */
static void reset_pet_navi_setup(u32 toolkit) {
    /* Reload stat boosts, then mark PET Nav as no longer active */
    reload_navi_stat_boosts();
    clear_event_flag_sub_8033FDC(0x17, 0x1D); /* EVENT_PET_NAVI_ACTIVE */
    clear_event_flag_sub_8033FDC(0x17, 0x03); /* EVENT_1703 */
    clear_event_flag_sub_8033FDC(0x01, 0x27); /* EVENT_127 */
    clear_event_flag_sub_8033FDC(0x17, 0x21); /* EVENT_1721 */

    /* Restore HP and adjust navi state */
    call_sub_809F9DC();
    set_cur_navi_hp_full();
    set_event_flag_sub_8033FDC(0x17, 0x24);   /* EVENT_1724 */
    clear_event_flag_sub_8033FDC(0x05, 0xDF); /* EVENT_5DF */
    set_event_flag_sub_8033FDC(0x06, 0x8E);   /* EVENT_68E */
    clear_event_flag_sub_8033FDC(0x06, 0x8F); /* EVENT_68F */
    set_event_flag_sub_8033FDC(0x07, 0x40);   /* EVENT_740 */
    clear_event_flag_sub_8033FDC(0x02, 0x25); /* EVENT_225 */

    /* Beast flags: set them if EVENT_224 is set, otherwise clear */
    if (test_event_flag_sub_8033FDC(0x02, 0x24)) { /* EVENT_224 */
        set_event_flag_range(0x01, 0x8E, 9); /* EVENT_18E */
    } else {
        clear_event_flag_range(0x01, 0x2A, 0xA); /* EVENT_12A range */
        clear_event_flag_range(0x01, 0x8E, 9);   /* EVENT_18E range */
    }

    /* Reset Beast counter, navi stats, and mood */
    set_beast_out_counter_3();
    zero_all_navi_stats_mood();
    call_sub_800A908();

    /* Clear PET Nav session flag */
    clear_event_flag_sub_8033FDC(0x17, 0x00); /* EVENT_1700 */

    /* Zero two halfword fields in the S2001c04 sub-structure */
    u32 s2001c04 = LD32(toolkit + oToolkit_S2001c04_Ptr);
    ST16(s2001c04 + 0x12, 0);
    ST16(s2001c04 + 0x14, 0);

    /* Reset navi data via gameState_setUnk09_8001172 */
    call_sub_8001172(0);

    /* Set "navi initialized" flag and clear session-state flags */
    set_event_flag_sub_8033FDC(0x00, 0x79);     /* EVENT_79 */
    clear_event_flag_sub_8033FDC(0x10, 0x06);   /* EVENT_1006 */
    clear_event_flag_sub_8033FDC(0x10, 0x09);   /* EVENT_1009 */
    clear_event_flag_sub_8033FDC(0x10, 0x0A);   /* EVENT_100A */
    clear_event_flag_sub_8033FDC(0x10, 0x1C);   /* EVENT_101C */

    /* Clear/set flag ranges for quest/state tracking */
    clear_event_flag_range(0x01, 0xBC, 0xC); /* EVENT_1BC range */
    clear_event_flag_range(0x01, 0xC8, 6);   /* EVENT_1C8 range */
    set_event_flag_range(0x01, 0xCE, 6);     /* EVENT_1CE range */

    /* If EVENT_1008 is set, swap 1C8/1CE ranges and re-set 101C */
    if (test_event_flag_sub_8033FDC(0x10, 0x08)) { /* EVENT_1008 */
        set_event_flag_sub_8033FDC(0x10, 0x1C);             /* EVENT_101C */
        set_event_flag_range(0x01, 0xC8, 6);    /* EVENT_1C8 range */
        clear_event_flag_range(0x01, 0xCE, 6);  /* EVENT_1CE range */
    }
}

/* resetPetNaviSetup_8033FDC(r10 = toolkit ptr) — resets PET Nav and clears related event flags */
Next resetPetNaviSetup_8033FDC(void) {
    reset_pet_navi_setup(R10);
    RETURN_TO_CALLER();
}

/* ── endOfSection_NaviSetup_80340F6 ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool test_event_flag_navi_80340F6(u32 group, u32 bit) {
    R0 = group;
    R1 = bit;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls SetEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void set_event_flag_navi_80340F6(u32 group, u32 bit) {
    R0 = group;
    R1 = bit;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void clear_event_flag_navi_80340F6(u32 group, u32 bit) {
    R0 = group;
    R1 = bit;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls writeCurPETNaviToS2001c04_Unk07_80010c6 outside this file, which has no typed C interface yet */
static void write_cur_pet_navi_to_s2001c04(void) {
    GUEST_CALL(writeCurPETNaviToS2001c04_Unk07_80010c6);
}

/* register form: calls SetCurPETNavi outside this file, which has no typed C interface yet */
static void set_cur_pet_navi(u32 slot) {
    R0 = slot;
    GUEST_CALL(SetCurPETNavi);
}

/* register form: calls reloadCurNaviBaseStats_8120df0 outside this file, which has no typed C interface yet */
static void reload_cur_navi_base_stats(void) {
    GUEST_CALL(reloadCurNaviBaseStats_8120df0);
}

/* register form: calls reloadCurNaviStatBoosts_813c3ac outside this file, which has no typed C interface yet */
static void reload_cur_navi_stat_boosts(void) {
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);
}

/* register form: calls SetCurNaviHPToFull outside this file, which has no typed C interface yet */
static void set_cur_navi_hp_to_full(void) {
    GUEST_CALL(SetCurNaviHPToFull);
}

/* register form: calls enableGhostNavi_80351C8 outside this file, which has no typed C interface yet */
static void enable_ghost_navi_navi_80340F6(void) {
    GUEST_CALL(enableGhostNavi_80351C8);
}

/* register form: calls SetBeastOutCounterTo3 outside this file, which has no typed C interface yet */
static void set_beast_out_counter_to_3(void) {
    GUEST_CALL(SetBeastOutCounterTo3);
}

/* register form: calls ZeroAllNaviStatsMood outside this file, which has no typed C interface yet */
static void zero_all_navi_stats_mood_navi_80340F6(void) {
    GUEST_CALL(ZeroAllNaviStatsMood);
}

/* register form: calls clearBattleStateCounter_800A908 outside this file, which has no typed C interface yet */
static void sub_800a908_call(void) {
    GUEST_CALL(clearBattleStateCounter_800A908);
}

/* register form: calls loadGameProgressFromGameProgressBuffer_8035354 outside this file, which has no typed C interface yet */
static void load_game_progress_from_buffer(u32 slot) {
    R0 = slot;
    GUEST_CALL(loadGameProgressFromGameProgressBuffer_8035354);
}

/* End-of-section Navi setup tail. Called by the adapter so that clearBattleStateCounter_800A908's
 * GUEST_CALL leaves the final R0..R3 (and nz/c/v flags) that endOfSection_NaviSetup_80340F6
 * returns to its caller -- per the convention, the registers the helper's
 * callee left behind stay there as the function's outputs. */
static void navi_tail(void) {
    zero_all_navi_stats_mood_navi_80340F6();
    sub_800a908_call();
}

/* End-of-section Navi setup. Optionally reloads the chapter save slot, fully
 * reloads the active PET Navi (or just refreshes stat boosts if no swap is
 * required), makes sure the "PET navi active" flag is set, refills the navi's
 * HP, clears a batch of transient event flags, enables the ghost Navi, and
 * zeroes a few scratch fields in the S2001c04 Navi struct. The mood-zero
 * and the post-setup routine follow in navi_tail from the adapter. */
static void navi_80340F6_impl(u32 toolkit_ptr) {
    u32 gs_ptr = LD32(toolkit_ptr + oToolkit_GameStatePtr);
    if (LD8(gs_ptr + oGameState_GameProgress) == 0x80)
        load_game_progress_from_buffer(0);

    if (!test_event_flag_navi_80340F6(0x01, 0x63)) {         /* EVENT 0x01.0x63 (EVENT_163) */
        write_cur_pet_navi_to_s2001c04();
        set_cur_pet_navi(0);
        reload_cur_navi_base_stats();
        reload_cur_navi_stat_boosts();
        write_cur_pet_navi_to_s2001c04();
    } else {
        reload_cur_navi_stat_boosts();
    }

    if (!test_event_flag_navi_80340F6(0x17, 0x2A))          /* EVENT_172A */
        set_event_flag_navi_80340F6(0x17, 0x1D);             /* EVENT_PET_NAVI_ACTIVE */

    set_cur_navi_hp_to_full();

    u32 navi = LD32(toolkit_ptr + oToolkit_S2001c04_Ptr);
    ST32(navi + 0x24, 0);
    ST32(navi + 0x28, 0);

    clear_event_flag_navi_80340F6(0x17, 0x0A);               /* EVENT_170A */
    clear_event_flag_navi_80340F6(0x17, 0x0B);               /* EVENT_170B */
    clear_event_flag_navi_80340F6(0x17, 0x09);               /* EVENT_1709 */
    clear_event_flag_navi_80340F6(0x04, 0xE9);               /* EVENT_4E9 */
    clear_event_flag_navi_80340F6(0x05, 0xF2);               /* EVENT_5F2 */
    set_event_flag_navi_80340F6  (0x05, 0xDF);               /* EVENT_5DF */

    enable_ghost_navi_navi_80340F6();

    clear_event_flag_navi_80340F6(0x17, 0x21);               /* EVENT_1721 */
    clear_event_flag_navi_80340F6(0x17, 0x00);               /* EVENT_1700 */

    u32 navi2 = LD32(toolkit_ptr + oToolkit_S2001c04_Ptr);
    ST16(navi2 + 0x12, 0);
    ST16(navi2 + 0x14, 0);

    set_beast_out_counter_to_3();
}

Next endOfSection_NaviSetup_80340F6(void) {
    navi_80340F6_impl((u32)R10);
    navi_tail();
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

/* ── ho_jackIn_80341B6 ── */
/* Jack-in sequence dispatcher (hooked into the jack-in cutscene).
 *
 * byte_2011A40 holds the current jack-in step as a word offset (0, 4 or 8).
 * Depending on whether the map being entered lies in the internet or the real
 * world, that step selects a handler from one of two ROM jump tables, and the
 * handler runs with:
 *   r0 = its own address, as loaded from the table (entered via bx r0),
 *   r1 = the step offset,
 *   r2 = the game state pointer,
 *   r5 = &byte_2011A40 (the handlers advance the step through it),
 * and the flags from the map-group comparison. The handler's r0-r3 and flag
 * results flow straight through to our caller; r4-r7 come back from the
 * original's pop {r4-r7,pc}, i.e. exactly as our caller left them. */

typedef struct { u32 r4, r5, r6, r7; } JackInSavedRegs;

/* register form: calls transmission_BeginTransmissionScene_80341F8 outside this file, which has no typed C interface yet */
static void call_transmission_scene(u32 stepByte, u32 gameState, u32 mapGroup)
{
    R0 = LD32(A_off_80341DC + stepByte);    /* handler address, entered via bx r0 */
    R1 = stepByte;                          /* current jack-in step (word offset) */
    R2 = gameState;
    R5 = A_byte_2011A40;                    /* handlers reach the step through r5 */
    SET_FLAGS_CMP(mapGroup, INTERNET_MAP_GROUP_START);
    GUEST_CALL(transmission_BeginTransmissionScene_80341F8);
}

/* register form: calls ho_8034218 outside this file, which has no typed C interface yet */
static void call_ho_8034218(u32 stepByte, u32 gameState, u32 mapGroup)
{
    R0 = LD32(A_off_80341DC + stepByte);
    R1 = stepByte;
    R2 = gameState;
    R5 = A_byte_2011A40;
    SET_FLAGS_CMP(mapGroup, INTERNET_MAP_GROUP_START);
    GUEST_CALL(ho_8034218);
}

/* register form: calls map_setupEnterMapAfterWarp_803423C outside this file, which has no typed C interface yet */
static void call_setup_enter_map_after_warp(u32 stepByte, u32 gameState, u32 mapGroup)
{
    R0 = LD32(A_off_80341DC + stepByte);
    R1 = stepByte;
    R2 = gameState;
    R5 = A_byte_2011A40;
    SET_FLAGS_CMP(mapGroup, INTERNET_MAP_GROUP_START);
    GUEST_CALL(map_setupEnterMapAfterWarp_803423C);
}

/* register form: calls sub_80342DC outside this file, which has no typed C interface yet */
static void call_sub_80342DC(u32 stepByte, u32 gameState, u32 mapGroup)
{
    R0 = LD32(A_off_80341E8 + stepByte);
    R1 = stepByte;
    R2 = gameState;
    R5 = A_byte_2011A40;
    SET_FLAGS_CMP(mapGroup, INTERNET_MAP_GROUP_START);
    GUEST_CALL(sub_80342DC);
}

/* register form: calls sub_80342E4 outside this file, which has no typed C interface yet */
static void call_sub_80342E4(u32 stepByte, u32 gameState, u32 mapGroup)
{
    R0 = LD32(A_off_80341E8 + stepByte);
    R1 = stepByte;
    R2 = gameState;
    R5 = A_byte_2011A40;
    SET_FLAGS_CMP(mapGroup, INTERNET_MAP_GROUP_START);
    GUEST_CALL(sub_80342E4);
}

/* register form: calls map_beginEnterMapOnWarpStep4_80342EC outside this file, which has no typed C interface yet */
static void call_begin_enter_map_on_warp_step4(u32 stepByte, u32 gameState, u32 mapGroup)
{
    R0 = LD32(A_off_80341E8 + stepByte);
    R1 = stepByte;
    R2 = gameState;
    R5 = A_byte_2011A40;
    SET_FLAGS_CMP(mapGroup, INTERNET_MAP_GROUP_START);
    GUEST_CALL(map_beginEnterMapOnWarpStep4_80342EC);
}

/* Run the handler for the current jack-in step and return the caller's r4-r7,
 * which the original restores with pop {r4-r7,pc} after the handler returns. */
static JackInSavedRegs run_jack_in_step(u32 toolkit, u32 callerR4, u32 callerR5,
                                        u32 callerR6, u32 callerR7)
{
    JackInSavedRegs restored = { callerR4, callerR5, callerR6, callerR7 };
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 mapGroup  = LD8(gameState + oGameState_MapGroup);
    u32 stepByte  = LD8(A_byte_2011A40);

    if ((s32)mapGroup >= INTERNET_MAP_GROUP_START) {
        /* internet map groups */
        if (stepByte == 4)
            call_sub_80342E4(stepByte, gameState, mapGroup);
        else if (stepByte == 8)
            call_begin_enter_map_on_warp_step4(stepByte, gameState, mapGroup);
        else
            call_sub_80342DC(stepByte, gameState, mapGroup);
    } else {
        /* real-world map groups */
        if (stepByte == 4)
            call_ho_8034218(stepByte, gameState, mapGroup);
        else if (stepByte == 8)
            call_setup_enter_map_after_warp(stepByte, gameState, mapGroup);
        else
            call_transmission_scene(stepByte, gameState, mapGroup);
    }

    return restored;
}

Next ho_jackIn_80341B6(void) {
    JackInSavedRegs restored = run_jack_in_step(R10, R4, R5, R6, R7);
    R4 = restored.r4;
    R5 = restored.r5;
    R6 = restored.r6;
    R7 = restored.r7;
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
    GUEST_CALL(resetPetNaviSetup_8033FDC);

    /* Update the area music for the new map; it receives whatever r0
     * the previous call returned, exactly as the original does. */
    GUEST_CALL(musicGameState_8000784);

    RETURN_TO_CALLER();
}

/* ── fadeTimerTick_80342A4 ── */
/* Offset within the screen-fade state structure (pointed to by R5) */
#define FADE_PHASE    1
#define FADE_TIMER    4

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_sub_80342A4(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();              /* Z set → no fade active */
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void set_screen_fade(u32 phase, u32 intensity) {
    R0 = phase;
    R1 = intensity;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls sub_80343B0 outside this file, which has no typed C interface yet */
static void post_fade_update(void) {
    GUEST_CALL(sub_80343B0);
}

/* Advance the fade countdown for one frame.
 * If a fade is active, decrement its timer; when the timer reaches zero,
 * start the next fade phase at intensity 0x10 and mark the structure
 * phase byte as 8.  Always runs the post-update routine at the end. */
static void fade_timer_tick(u32 fade_state) {
    if (screen_fade_active_sub_80342A4()) {
        u32 timer = LD16(fade_state + FADE_TIMER);
        timer -= 1;
        ST16(fade_state + FADE_TIMER, (u16)timer);

        if ((s32)timer <= 0) {
            set_screen_fade(4, 0x10);
            ST8(fade_state + FADE_PHASE, 8);
        }
    }

    post_fade_update();               /* final callee sets r0-r3 and flags */
}

/* fadeTimerTick_80342A4 (r5 = fade state ptr) → void */
Next fadeTimerTick_80342A4(void) {
    fade_timer_tick(R5);
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
 * the callees (map_triggerEnterMapOnWarp, endOfSection_NaviSetup_80340F6) read. */
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

    /* endOfSection_NaviSetup_80340F6(gameState) — r0 is restored to the game state pointer
     * that the previous call may have clobbered. */
    R0 = gameState;
    GUEST_CALL(endOfSection_NaviSetup_80340F6);

    RETURN_TO_CALLER();
}

/* ── decompJackInAnimationGfx_8034314 ── */
/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void call_decompAndCopyData(u32 table) {
    R0 = table;
    GUEST_CALL(decompAndCopyData);
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet.
 * The original also passes the animation table in r3, and whatever r3 the callee leaves
 * behind is what callers of decompJackInAnimationGfx_8034314 see, so it is set here and
 * deliberately not restored afterwards. */
static void call_sub_8046664(u32 table) {
    R3 = table;
    GUEST_CALL(battle_resetEStructRecords_8046664);
}

/* register form: calls battle_spawnEStructRecordsFromTable_80465A0 outside this file, which has no typed C interface yet.
 * This is the last callee, so the flags it leaves are the ones callers branch on. */
static void call_sub_80465A0(u32 frameList) {
    R0 = frameList;
    GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);
}

/* Decompress the jack-in animation graphics (chip selection screen) and set up the
 * animation state at `obj`: reset its byte at +3, copy the frame count from the
 * animation table header, initialise the animator from that table, and hand it the
 * frame list (tile 0x39 entries for tiles 0..E, 0xFF terminator).
 * r3 and the condition flags are left exactly as the original leaves them: by the
 * callees themselves, not by this code. */
static void jack_in_animation_gfx(u32 obj) {
    call_decompAndCopyData(A_initRefs_8034338);

    ST8(obj + 3, 0);                            /* obj+3 = 0 */
    ST16(obj + 6, LD32(A_off_8034400 + 8));     /* obj+6 = dword_8034408 */

    call_sub_8046664(A_off_8034400);
    call_sub_80465A0(A_byte_8034370);
}

/* decompJackInAnimationGfx_8034314(r5 = animation state object) -> r2 r3 r4-r7, nz c v */
Next decompJackInAnimationGfx_8034314(void) {
    jack_in_animation_gfx(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80343B0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r6, r7; } call_QueueWordAlignedGFXTransfer_result;
/* register form: generated wrapper: QueueWordAlignedGFXTransfer has no typed C interface yet */
static inline call_QueueWordAlignedGFXTransfer_result call_QueueWordAlignedGFXTransfer(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(QueueWordAlignedGFXTransfer);
    return (call_QueueWordAlignedGFXTransfer_result){ R6, R7 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7; bool v; } call_CopyBackgroundTiles_result_sub_80343B0;
/* register form: generated wrapper: CopyBackgroundTiles has no typed C interface yet */
static inline call_CopyBackgroundTiles_result_sub_80343B0 call_CopyBackgroundTiles_sub_80343B0(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles);
    return (call_CopyBackgroundTiles_result_sub_80343B0){ R2, R3, R4, R5, R6, R7, OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_entity_runAllUpdateHandlers_80465BC_result;
/* register form: generated wrapper: entity_runAllUpdateHandlers_80465BC has no typed C interface yet */
static inline call_entity_runAllUpdateHandlers_80465BC_result call_entity_runAllUpdateHandlers_80465BC(void) {
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    return (call_entity_runAllUpdateHandlers_80465BC_result){ R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r0, r1, r2, r3, r8, r9, r10, r11, r12; bool z, n, c, v; } call_sub_80465F8_result;
/* register form: generated wrapper: battle_updateEStructSprites_80465F8 has no typed C interface yet */
static inline call_sub_80465F8_result call_sub_80465F8(void) {
    GUEST_CALL(battle_updateEStructSprites_80465F8);
    return (call_sub_80465F8_result){ R0, R1, R2, R3, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
} UpdateWorldResult;

/* Per-frame advance of the world-map animation table pointed to by state.
 * state+3 holds the current 8-bit row offset into a 12-byte-per-row table
 * rooted at off_8034400; state+6 holds a 16-bit frames-remaining counter.
 * When the counter is still > 1, just decrement it. Otherwise look at the
 * next row slot: if its first word is non-zero, advance to that row; if
 * it is zero (end-of-table sentinel), wrap back to row 0 -- but keep the
 * row pointer on the row that just expired (the original assembly leaves
 * r3 unchanged in that branch, so the palette queued below comes from the
 * old row). Then queue that row's palette into VRAM, re-stamp the standard
 * 32x20 BG tile block, tick every per-frame entity handler, and drain the
 * queued GFX list. */
static UpdateWorldResult update_world_map(u32 state, u32 r10_in) {
    UpdateWorldResult res;
    /* register form: r4/r6/r7 are preserved by the caller but clobbered
     * by QueueWordAlignedGFXTransfer, CopyBackgroundTiles,
     * entity_runAllUpdateHandlers_80465BC, and battle_updateEStructSprites_80465F8, so snapshot
     * them on entry and restore on exit. */
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    u32 saved_r7 = R7;

    u32 base = A_off_8034400;
    u32 row_idx = LD8(state + 3);
    u32 row_ptr = base + row_idx;
    u32 frames_left = LD16(state + 6);
    u32 next_row_ptr;

    if ((s32)frames_left > 1) {
        /* Same row, one fewer tick on its frame counter. */
        frames_left -= 1;
        next_row_ptr = row_ptr;
    } else {
        /* Counter elapsed -- peek at the next 12-byte row slot. */
        u32 next_marker = LD32(row_ptr + 0xC);
        if (next_marker == 0) {
            /* End-of-table sentinel: wrap to row 0 for the *next* tick.
             * The original keeps r3 on the row that just expired, so
             * the palette queued below still comes from that row. */
            row_idx = 0;
            frames_left = LD32(base + (A_dword_8034408 - A_off_8034400));
            next_row_ptr = row_ptr;
        } else {
            /* Step to the next row and pick up its frame count. */
            row_idx += 0xC;
            next_row_ptr = base + row_idx;
            frames_left = LD32(next_row_ptr + 8);
        }
    }

    ST8(state + 3, (u8)row_idx);
    ST16(state + 6, (u16)frames_left);

    /* Queue the chosen row's palette source into palette_3001960. */
    call_QueueWordAlignedGFXTransfer(LD32(next_row_ptr + 4), A_palette_3001960, 0x20);
    /* Re-stamp the standard 32x20 background tile block with the world tile IDs. */
    call_CopyBackgroundTiles_sub_80343B0(0, 0, 1, A_eTileIds2017A04, 0x20, 0x14);
    /* Tick every per-frame entity update handler. */
    call_entity_runAllUpdateHandlers_80465BC();
    /* Drain the queued GFX list; r8/r9/r10/r11/r12 come from here. */
    call_sub_80465F8_result s = call_sub_80465F8();

    /* Restore the preserved registers we snapshotted at entry. */
    R4 = saved_r4;
    R6 = saved_r6;
    R7 = saved_r7;

    /* r10_in is the caller's R10; the callees that need r10 (CopyBackgroundTiles,
     * entity_runAllUpdateHandlers_80465BC) read it straight from the guest
     * register, so this parameter is just along for the ride. */
    (void)r10_in;
    res.r5 = state;
    res.r8 = s.r8;
    res.r9 = s.r9;
    res.r10 = s.r10;
    res.r11 = s.r11;
    res.r12 = s.r12;
    return res;
}

Next sub_80343B0(void) {
    UpdateWorldResult res = update_world_map(R5, R10);
    R5 = res.r5;
    R8 = res.r8;
    R9 = res.r9;
    R10 = res.r10;
    R11 = res.r11;
    R12 = res.r12;
    RETURN_TO_CALLER();
}

/* ── map_8034B4C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_ZeroFillByWord_result;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result call_ZeroFillByWord(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result){ R3 };
}

typedef struct { u32 r4, r5, r6, r7; } call_playCertainMapMusicBasedOnEventByte_8036e44_result;
/* register form: generated wrapper: playCertainMapMusicBasedOnEventByte_8036e44 has no typed C interface yet */
static inline call_playCertainMapMusicBasedOnEventByte_8036e44_result call_playCertainMapMusicBasedOnEventByte_8036e44(void) {
    GUEST_CALL(playCertainMapMusicBasedOnEventByte_8036e44);
    return (call_playCertainMapMusicBasedOnEventByte_8036e44_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r5, r6, r7, r10, r11; } call_sub_8035028_result;
/* register form: generated wrapper: map_dispatchInternetGroupHook has no typed C interface yet */
static inline call_sub_8035028_result call_sub_8035028(void) {
    GUEST_CALL(map_dispatchInternetGroupHook);
    return (call_sub_8035028_result){ R5, R6, R7, R10, R11 };
}

typedef struct { u32 r5, r6, r7; } call_owPlayer_80350a8_result;
/* register form: generated wrapper: owPlayer_80350a8 has no typed C interface yet */
static inline call_owPlayer_80350a8_result call_owPlayer_80350a8(void) {
    GUEST_CALL(owPlayer_80350a8);
    return (call_owPlayer_80350a8_result){ R5, R6, R7 };
}

typedef struct { u32 r2, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_npc_spawnOverworldNPCObjectsForMap_result;
/* register form: generated wrapper: npc_spawnOverworldNPCObjectsForMap has no typed C interface yet */
static inline call_npc_spawnOverworldNPCObjectsForMap_result call_npc_spawnOverworldNPCObjectsForMap(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(npc_spawnOverworldNPCObjectsForMap);
    return (call_npc_spawnOverworldNPCObjectsForMap_result){ R2, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* register form: generated wrapper: sub_803537C has no typed C interface yet */
static inline void call_sub_803537C(void) {
    GUEST_CALL(sub_803537C);
}

typedef struct { u32 r2, r5, r6, r7, r8, r9, r10, r11, r12; } call_sub_80353DA_result;
/* register form: generated wrapper: ow_loadEventGfxAnims has no typed C interface yet */
static inline call_sub_80353DA_result call_sub_80353DA(void) {
    GUEST_CALL(ow_loadEventGfxAnims);
    return (call_sub_80353DA_result){ R2, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r2, r10, r11; } call_StoreMapScriptsThenRunOnInitMapScript_result;
/* register form: generated wrapper: StoreMapScriptsThenRunOnInitMapScript has no typed C interface yet */
static inline call_StoreMapScriptsThenRunOnInitMapScript_result call_StoreMapScriptsThenRunOnInitMapScript(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(StoreMapScriptsThenRunOnInitMapScript);
    return (call_StoreMapScriptsThenRunOnInitMapScript_result){ R2, R10, R11 };
}

typedef struct { u32 r10; } call_PlayMapMusic_result;
/* register form: generated wrapper: PlayMapMusic has no typed C interface yet */
static inline call_PlayMapMusic_result call_PlayMapMusic(u32 r2) {
    R2 = r2;
    GUEST_CALL(PlayMapMusic);
    return (call_PlayMapMusic_result){ R10 };
}

// Typed wrappers are generated for the game functions listed below; call them, don't define them.

/* Load and bring up a map: clear the per-map work block, run the music/event
 * setup helpers and the NPC spawner, run the map's on-init/on-update script
 * pair for the resolved group, then start the map's music.
 *
 * map_group/map_number index the *real-world* table by default; groups past
 * INTERNET_MAP_GROUP_START index the separate Internet table.
 *
 * The original saves r8/r9/r12 across this routine and the saved-back values
 * come from r2-r4 that it pushes before the first call. But none of the called
 * functions return r8/r9/r12 in their results, so they must be preserved by the
 * routine itself: snapshot the guest r8/r9/r12 at entry and restore them on
 * exit, exactly as the assembly does by hand.
 */
static void load_map(u32 map_group, u32 map_number) {
    /* register form: this routine is literally the assembly, register use is unavoidable. */
    u32 saved_r8 = R8;
    u32 saved_r9 = R9;
    u32 saved_r12 = R12;

    /* Clear the 0x40-byte per-map work block at unk_2011EA0. */
    call_ZeroFillByWord(A_unk_2011EA0, 0x40);

    /* Bring the music (event-driven), the gamestate module (map_dispatchInternetGroupHook),
     * the overworld player (owPlayer_80350a8) and the map's NPCs online. */
    call_playCertainMapMusicBasedOnEventByte_8036e44();
    call_sub_8035028();
    call_owPlayer_80350a8();
    call_npc_spawnOverworldNPCObjectsForMap(R5, R6, R7);

    /* sub_803537C runs first, then ow_loadEventGfxAnims on the r0/r1 it leaves behind.
     * Both are typed wrappers; their interface already encodes the leftover
     * semantics, so just chain them. */
    call_sub_803537C();
    call_sub_80353DA();

    /* Pick the script-pointer table for this map group, offset the group down
     * to 0 if it lives in the Internet range. */
    u32 script_tables = A_RealWorldMapScriptPointers;
    if ((s32)map_group >= (s32)INTERNET_MAP_GROUP_START) {
        script_tables = A_InternetMapScriptPointers;
        map_group -= INTERNET_MAP_GROUP_START;
    }

    /* Each group owns a pair of script tables (on-init, on-update), one entry
     * per map number. Resolve both pointer-to-pointer chains. */
    u32 group_entry = script_tables + (map_group << 3);
    u32 on_init_script  = LD32(LD32(group_entry)     + (map_number << 2));
    u32 on_update_script = LD32(LD32(group_entry + 4) + (map_number << 2));

    /* Store the script pair (which runs on_init for this map) and start music. */
    call_StoreMapScriptsThenRunOnInitMapScript(on_init_script, on_update_script);
    call_PlayMapMusic(R2);

    /* Restore the saved high registers exactly as the original's epilogue does. */
    R8 = saved_r8;
    R9 = saved_r9;
    R12 = saved_r12;
}

/* map_8034B4C(r0 = map_group, r1 = map_number) -> () */
Next map_8034B4C(void) {
    load_map(R0, R1);
    RETURN_TO_CALLER();
}

/* ── check_playerCanAdvanceForward_8034C36 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_sub_8034C36(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();          /* Z set when fade is NOT active */
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8034C36(u32 group, u32 flag) {
    R0 = group;
    R1 = flag;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* Z set when flag is NOT set */
}

/* register form: calls owPlayer_isBusyOrCutsceneActive_809E462 outside this file, which has no typed C interface yet */
static bool func_809E462_nonzero(void) {
    GUEST_CALL(owPlayer_isBusyOrCutsceneActive_809E462);
    return !ZERO_FLAG();          /* Z set when result is 0 */
}

/* register form: calls chatbox_mask_eFlags2009F38 outside this file, which has no typed C interface yet */
/* register form: relies on chatbox_mask_eFlags2009F38 leaving r0: its Z is r0 == 0 (and r0, r1), so testing
 * r0 is the original's branch on Z */
static s32 chatbox_mask_flag(s32 flag) {
    R0 = (u32)flag;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return (s32)R0;
}

/* register form: calls IsCutsceneScriptNonNull outside this file, which has no typed C interface yet */
static bool cutscene_script_nonnull(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return !ZERO_FLAG();          /* Z set when script IS null */
}

/* Gate check for player movement: returns true only when the screen is fading,
 * neither the advance-forward nor sliprun event flags are set, owPlayer_isBusyOrCutsceneActive_809E462
 * returns zero, chatbox mask 0x80 is clear, and no cutscene script is loaded. */
static bool player_advance_allowed(void) {
    if (!screen_fade_active_sub_8034C36())
        return false;
    if (event_flag_set_sub_8034C36(0x17, 0x17))       /* EVENT_1717_PLAYER_ADVANCE_FORWARD */
        return false;
    if (event_flag_set_sub_8034C36(0x17, 0x3D))       /* EVENT_IN_SLIPRUN_STATE */
        return false;
    if (func_809E462_nonzero())
        return false;
    if (chatbox_mask_flag(0x80) != 0)
        return false;
    if (cutscene_script_nonnull())
        return false;
    return true;
}

/* check_playerCanAdvanceForward_8034C36() -> r0 (0 or 1), flags from final MOV */
Next check_playerCanAdvanceForward_8034C36(void) {
    R0 = (u32)player_advance_allowed();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── playerAdvanceForwardCheck_8034C6E ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool screen_fade_active_sub_8034C6E(void) {
    GUEST_CALL(IsScreenFadeActive);
    return !ZERO_FLAG();   /* Z set → fade not active */
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8034C6E(u8 page, u8 bit) {
    R0 = page;
    R1 = bit;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();   /* Z set → flag is clear */
}

/* register form: calls owPlayer_isBusyOrCutsceneActive_809E462 outside this file, which has no typed C interface yet */
static bool sub_809E462_nonzero(void) {
    GUEST_CALL(owPlayer_isBusyOrCutsceneActive_809E462);
    return !ZERO_FLAG();
}

/* register form: calls chatbox_mask_eFlags2009F38 outside this file, which has no typed C interface yet */
static bool chatbox_mask_active(u32 flag) {
    R0 = flag;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return !ZERO_FLAG();
}

/* register form: calls IsCutsceneScriptNonNull outside this file, which has no typed C interface yet */
static bool cutscene_script_present(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return !ZERO_FLAG();   /* Z set → script is null */
}

/* Return 1 if all conditions for player forward-advance are met:
 * screen fade active, advance flag clear, no blocking state. */
static u32 can_player_advance_forward(void) {
    if (!screen_fade_active_sub_8034C6E())
        return 0;
    /* EVENT_1717_PLAYER_ADVANCE_FORWARD: page 0x17, bit 0x17 */
    if (event_flag_set_sub_8034C6E(0x17, 0x17))
        return 0;
    if (sub_809E462_nonzero())
        return 0;
    if (chatbox_mask_active(0x80))
        return 0;
    if (cutscene_script_present())
        return 0;
    return 1;
}

/* playerAdvanceForwardCheck_8034C6E() -> r0 (1 or 0), flags nz */
Next playerAdvanceForwardCheck_8034C6E(void) {
    R0 = can_player_advance_forward();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── IsNotSlipRunningAndNotInCutscene ── */
#define EVENT_IN_SLIPRUN_STATE 0x17

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool sliprun_flag_set(void) {
    R0 = EVENT_IN_SLIPRUN_STATE;
    R1 = 61;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();  /* Z clear → flag is set */
}

/* register form: calls IsCutsceneScriptNonNull outside this file, which has no typed C interface yet */
static bool cutscene_script_non_null(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return !ZERO_FLAG();  /* Z clear → script is non-null */
}

/* Returns true unless the slip-run state flag is active or a cutscene is running. */
static bool is_not_slip_running_and_not_in_cutscene(void) {
    if (sliprun_flag_set())
        return false;
    if (cutscene_script_non_null())
        return false;
    return true;
}

/* IsNotSlipRunningAndNotInCutscene() -> r0 (TRUE=1/FALSE=0), nz */
Next IsNotSlipRunningAndNotInCutscene(void) {
    R0 = is_not_slip_running_and_not_in_cutscene();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8034E3C ── */
/* sub_8034E3C(r1 = table selector, r7 -> struct with halfword at +4):
 * Scan one of the u16 lists in off_8034E64 (picked by r1) for the halfword
 * [r7,#4].  Stops at the first match, else at the 0xFFFF terminator.
 * Returns r0 = r2 = byte offset of the match (or of the terminator) and
 * the Z flag from (found ? 0 : 1); r4 is written locally inside the
 * original but then restored from the stack, so it is preserved. */
Next sub_8034E3C(void) {
    u32 listBase = LD32(A_off_8034E64 + (R1 << 2)); /* pointer table entry */
    u16 wanted   = LD16(R7 + 4);

    u32 offset = 0;
    u32 notFound;

    for (;;) {
        u16 value = LD16(listBase + offset);
        if (value == wanted) {
            notFound = 0;
            break;
        }
        offset += 2;
        if (value != 0xFFFF)
            continue;
        notFound = 1; /* hit the list terminator without a match */
        break;
    }

    R0 = offset;
    R2 = offset;
    SET_FLAGS_NZ(notFound);
    /* r4 = 0/1 only feeds the flag test; pop {r4,r6,pc} restores the caller's r4 */
    RETURN_TO_CALLER();
}

/* ── HandleCoordinateInteractionCutscene ── */
/* register form: calls IsNotSlipRunningAndNotInCutscene outside this file, which has no typed C interface yet */
static bool not_slipping_and_not_in_cutscene(void) {
    GUEST_CALL(IsNotSlipRunningAndNotInCutscene);
    return !ZERO_FLAG();          /* answers in Z: clear means it is safe to run a cutscene */
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set_HandleCoordinateInteractionCutscene(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();          /* Z clear means the flag is set */
}

/* register form: calls StartCutscene outside this file, which has no typed C interface yet */
static void start_cutscene_HandleCoordinateInteractionCutscene(u32 script, u32 param) {
    R0 = script;
    R1 = param;
    GUEST_CALL(StartCutscene);
}

/* Start the object-interaction cutscene for the coordinate interaction the player is
 * standing on.  Interaction value 0 means "none"; 0xF0-0xFF are the sixteen ordinary
 * slots of the current map (each guarded by a one-shot event flag), and Graveyard 2
 * additionally has sixteen extra slots at 0x60-0x6F. */
static void handle_coordinate_interaction_cutscene(u32 toolkit) {
    if (!not_slipping_and_not_in_cutscene())
        return;

    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u8 interaction = LD8(gameState + oGameState_CoordInteractionValue);
    if (interaction == 0)
        return;

    u8 mapGroup  = LD8(gameState + oGameState_MapGroup);
    u8 mapNumber = LD8(gameState + oGameState_MapNumber);

    u32 tableBase;   /* array, per map group, of pointers to per-map text-index tables */
    u32 groupIndex;  /* index into tableBase */
    u32 slot;        /* index into the selected map's text-index table */

    if (mapGroup == GROUP_GRAVEYARD_IMMORTAL_AREA
        && mapNumber == MAP_GRAVEYARD2
        && interaction >= 0x60 && interaction <= 0x6F) {
        /* Graveyard 2's extra interactions are listed in the internet-area table,
         * in slots 0x10-0x1F right after the ordinary ones. */
        tableBase  = A_off_8034728;
        groupIndex = mapGroup - INTERNET_MAP_GROUP_START;
        slot       = interaction - 0x60 + 0x10;
    } else {
        if (interaction < 0xF0)
            return;                        /* not an ordinary interaction slot */
        slot = interaction - 0xF0;
        if (event_flag_set_HandleCoordinateInteractionCutscene(EVENT_16C0 + slot))
            return;                        /* one-shot interaction, already triggered */

        if ((s32)mapGroup >= INTERNET_MAP_GROUP_START) {
            tableBase  = A_off_8034728;
            groupIndex = mapGroup - INTERNET_MAP_GROUP_START;
        } else {
            tableBase  = A_off_803461C;
            groupIndex = mapGroup;
        }
    }

    u32 groupTables = LD32(tableBase + groupIndex * 4);
    u32 mapTexts    = LD32(groupTables + mapNumber * 4);
    u8 textIndex    = LD8(mapTexts + slot);
    if (textIndex == 0xFF)
        return;                            /* 0xFF = nothing to show for this slot */

    start_cutscene_HandleCoordinateInteractionCutscene(A_CutsceneScriptObjectInteract_8098358, textIndex);
}

/* HandleCoordinateInteractionCutscene(r10 = toolkit) -> r2 unchanged */
Next HandleCoordinateInteractionCutscene(void) {
    handle_coordinate_interaction_cutscene(R10);
    RETURN_TO_CALLER();
}

/* ── maybeStartEntranceCutscene_8034F68 ── */
/* register form: calls IsCutsceneScriptNonNull outside this file, which has no typed C interface yet */
static bool cutscene_script_non_null_sub_8034F68(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return !ZERO_FLAG();          /* IsCutsceneScriptNonNull: Z clear if non-null */
}

/* register form: calls checkCoordinateTrigger_8031a7a outside this file, which has no typed C interface yet.
 * Writes its trigger-id result into *out_id (game semantics: the second return register). */
static void check_coordinate_trigger_sub_8034F68(u32 coords, u32 *out_id) {
    R0 = coords;
    GUEST_CALL(checkCoordinateTrigger_8031a7a);
    *out_id = R1;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();          /* TestEventFlag: Z set if the flag is clear */
}

/* register form: calls StartCutscene outside this file, which has no typed C interface yet */
static void start_cutscene_sub_8034F68(u32 script_addr, u32 param) {
    R0 = script_addr;
    R1 = param;
    GUEST_CALL(StartCutscene);
}

/* Door-entrance cutscene trigger: when the player's overworld position sits on a door,
 * if the per-door event flag is set and the door's slot is enabled, start the entrance
 * cutscene script for that door (slot id 1..15). */
static void maybe_start_entrance_cutscene(u32 toolkit_r10, u32 saved_r2) {
    (void)saved_r2;               /* caller-preserved; original never wrote r2 here */

    if (cutscene_script_non_null_sub_8034F68())
        return;

    u32 game_state = LD32(toolkit_r10 + oToolkit_GameStatePtr);
    u32 player_obj = LD32(game_state + oGameState_OverworldPlayerObjectPtr);
    u32 coords = player_obj + 0x1C;

    u32 trigger_id;
    check_coordinate_trigger_sub_8034F68(coords, &trigger_id);

    if ((s32)trigger_id < 1 || (s32)trigger_id > 0xF)
        return;

    u32 flag = 0x16F0 + trigger_id;
    if (!event_flag_set(flag))
        return;

    u32 slot_addr = A_unk_2011EA0 + trigger_id * 4;
    if (LD8(slot_addr) == 0)
        return;

    start_cutscene_sub_8034F68(A_CutsceneScriptEntranceInteract_8098384, LD32(slot_addr));
}

Next maybeStartEntranceCutscene_8034F68(void) {
    /* inputs: r4..r12, lr; outputs: r2, r4..r12 */
    u32 r2_in = R2;
    u32 r10_in = R10;
    R2 = r2_in;     /* caller-preserved on this path; original leaves r2 unchanged */
    maybe_start_entrance_cutscene(r10_in, r2_in);
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

        /* oGameState_LastMapChangeCounter being 0xFF blocks the notification. */
        if (LD8(gameState + oGameState_LastMapChangeCounter) != 0xFF) {
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

/* ── npc_spawnOverworldNPCObjectsForMap ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_1721_set(void) {
    R0 = 0x17;  /* EVENT_1721 >> 8 */
    R1 = 0x21;  /* EVENT_1721 & 0xFF */
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();  /* Z set if flag is clear */
}

/* register form: calls npc_freeAllObjectsThenSpawnObjectsFromList outside this file, which has no typed C interface yet */
static void spawn_npc_object_list(u32 script_ptr, u32 map_num_x4, u32 area_array, u32 game_state) {
    R0 = script_ptr;
    R1 = map_num_x4;
    R2 = area_array;
    R3 = game_state;
    GUEST_CALL(npc_freeAllObjectsThenSpawnObjectsFromList);
}

/* Spawn overworld NPC objects for the current map, unless EVENT_1721 is set.
 * Indexes into the per-group NPC list table (real-world or internet) using
 * MapGroup and MapNumber from the game state, then delegates to the spawn routine. */
static void npc_spawn_overworld_npc_objects(u32 toolkit) {
    if (event_flag_1721_set())
        return;

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 map_group  = LD8(game_state + oGameState_MapGroup);
    u32 map_number = LD8(game_state + oGameState_MapNumber);

    u32 npc_list_base;
    if (map_group >= INTERNET_MAP_GROUP_START) {
        npc_list_base = A_NPCList_maps80;
        map_group -= INTERNET_MAP_GROUP_START;
    } else {
        npc_list_base = A_NPCList_maps00;
    }

    u32 area_array = LD32(npc_list_base + (map_group << 2));
    u32 map_num_x4 = map_number << 2;
    u32 script_ptr = LD32(area_array + map_num_x4);

    /* The callee reads R0-R3; mirror what the original assembly had in those
     * registers at the point of the bl. */
    spawn_npc_object_list(script_ptr, map_num_x4, area_array, game_state);
}

/* npc_spawnOverworldNPCObjectsForMap(r10 = toolkit ptr) -> r0-r3 from callees */
Next npc_spawnOverworldNPCObjectsForMap(void) {
    npc_spawn_overworld_npc_objects(R10);
    RETURN_TO_CALLER();
}

/* ── enableGhostNavi_80351C8 ── */
/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool test_event_flag(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG(); /* TestEventFlag: Z set if the flag is clear */
}

/* register form: calls SetEventFlag outside this file, which has no typed C interface yet */
static void set_event_flag_enableGhostNavi_80351C8(u16 flag) {
    R0 = flag;
    GUEST_CALL(SetEventFlag);
}

/* Enable ghost navi events: for each (condition, target) pair in the table,
 * if the condition event flag is already set, set the target event flag.
 * Table is terminated by a 0x0000 condition entry. */
static void enable_ghost_navi(void) {
    u32 table = A_word_80351EC;
    u16 condition;

    while ((condition = LD16(table)) != 0) {
        if (test_event_flag(condition))
            set_event_flag_enableGhostNavi_80351C8(LD16(table + 2));
        table += 4; /* one (condition, target) pair = 4 bytes */
    }
}

/* enableGhostNavi_80351C8: r4-r7 preserved, clobbers r0-r3/lr/nz/c/v */
Next enableGhostNavi_80351C8(void) {
    enable_ghost_navi();
    RETURN_TO_CALLER();
}

/* ── handleDriveOutEvents_803522E ── */
/* register form: calls SetEventFlag outside this file, which has no typed C interface yet */
static void set_event_flag_sub_803522E(u32 flag) {
    R0 = flag;
    GUEST_CALL(SetEventFlag);
}

/* Scan the drive-out event table in guest RAM and set the flag whose trigger
 * matches `event`. The table at A_word_8035250 is a flat array of {trigger,
 * flag} u16 pairs, terminated by a 0xFFFF trigger entry.
 * Returns the final r1 value left in the guest register: the 0xFFFF sentinel
 * that was loaded on the iteration which took the branch to return. */
static u16 handle_drive_out_events(u32 event) {
    u32 p = A_word_8035250;
    u16 r1;
    for (;;) {
        r1 = (u16)LD16(p);
        if (r1 == 0xFF) break;
        if (event == r1) {
            set_event_flag_sub_803522E((u16)LD16(p + 2));
        }
        p += 4;
    }
    return r1;
}

Next handleDriveOutEvents_803522E(void) {
    u16 r1 = handle_drive_out_events(R0);
    R1 = r1;
    SET_FLAGS_CMP(r1, 0xFF);
    RETURN_TO_CALLER();
}

/* ── ghostNaviCheck_8035274 ── */
/* ------------------------------------------------------------------ */
/* Local constants (not defined in the shared headers of this build)  */
/* ------------------------------------------------------------------ */

/* One-shot gate flag: once set, ghost Navi encounters are disabled.
 * movflag EVENT_1700 loads it as group 0x17 / index 0x00. */
#define EVENT_1700 0x1700

/* GameState field holding the current map group (ldrb r0,[r7,#4]). */
#define oGameState_MapGroup_ghostNaviCheck_8035274 4

/* ------------------------------------------------------------------ */
/* Register-form call helpers                                         */
/* ------------------------------------------------------------------ */

/* register form: calls IsNotSlipRunningAndNotInCutscene outside this file, which has no typed C interface yet */
static u32 no_slip_or_cutscene_running(void) {
    GUEST_CALL(IsNotSlipRunningAndNotInCutscene);
    return !ZERO_FLAG();          /* Z set while a slip or cutscene is running */
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static u32 immediate_flag_set(u16 flag) {
    R0 = flag >> 8;               /* flag number is split across r0 (group) / r1 (index) */
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static u32 event_flag_set_ghostNaviCheck_8035274(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls checkCoordinateTrigger_8031a7a outside this file, which has no typed C interface yet */
static s32 check_coordinate_trigger(u32 coords) {
    R0 = coords;
    GUEST_CALL(checkCoordinateTrigger_8031a7a);
    return (s32)R0;               /* coordinate trigger id the player stands on */
}

/* register form: calls StartCutscene outside this file, which has no typed C interface yet */
static void start_cutscene(u32 script, u32 param) {
    R0 = script;
    R1 = param;
    GUEST_CALL(StartCutscene);
}

/* ------------------------------------------------------------------ */
/* Ghost Navi encounter check                                         */
/* ------------------------------------------------------------------ */

/* Table over the ghost Navi coordinate triggers (map groups 0xD0..0xD6),
 * one 6-byte entry per map: { trigger id, required flag, cleared flag },
 * terminated by a trigger id of 0. */
#define GHOST_NAVI_TABLE        A_word_80352DC
#define GHOST_NAVI_ENTRY_SIZE   6

/* Cutscene parameter words, 4 bytes per table entry above. */
#define GHOST_NAVI_PARAMS       A_byte_803530C
#define GHOST_NAVI_PARAM_SIZE   4

/* Shared cutscene script run for every ghost Navi encounter. */
#define GHOST_NAVI_SCRIPT       A_byte_80990B8

/* If the player stands on a ghost Navi coordinate trigger in a high map
 * group (>= 0x80), that ghost's encounter flag is set and its "already
 * beaten" flag is clear, start the ghost Navi cutscene for that map. */
static void ghost_navi_check(u32 toolkit) {
    /* Only act while the player has normal overworld control. */
    if (!no_slip_or_cutscene_running())
        return;
    /* One-shot gate: once this event flag is set, the check is disabled. */
    if (immediate_flag_set(EVENT_1700))
        return;

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    if ((s32)LD8(game_state + oGameState_MapGroup_ghostNaviCheck_8035274) < 0x80)
        return;

    u32 player = LD32(game_state + oGameState_OverworldPlayerObjectPtr);
    s32 trigger = check_coordinate_trigger(player + 0x1C);
    if (trigger < 0xD0 || trigger > 0xEF)
        return;                   /* not on a ghost Navi trigger */

    for (u32 i = 0; ; i++) {
        u32 entry = GHOST_NAVI_TABLE + i * GHOST_NAVI_ENTRY_SIZE;
        u16 id = LD16(entry);
        if (id == 0)
            return;               /* end of table */
        if (id != (u16)trigger)
            continue;             /* different map's ghost */
        if (!event_flag_set_ghostNaviCheck_8035274(LD16(entry + 2)))
            continue;             /* this ghost hasn't appeared yet */
        if (event_flag_set_ghostNaviCheck_8035274(LD16(entry + 4)))
            continue;             /* this ghost was already deleted */

        start_cutscene(GHOST_NAVI_SCRIPT,
                       LD32(GHOST_NAVI_PARAMS + i * GHOST_NAVI_PARAM_SIZE));
        return;
    }
}

/* ghostNaviCheck_8035274: no results in registers; r2/r4-r12 are left as
 * they arrived, and callers do not branch on the flags. */
Next ghostNaviCheck_8035274(void) {
    ghost_navi_check(R10);
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

/* ── sub_803537C ── */
/* Table of (map_group, event_flag) pairs used to auto-fire EVENT_28 when the player
 * enters a matching map. Terminated by an entry with map_group == 0. */
struct autoinc_entry { u16 map_group; u16 event_flag; };

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void clear_event_flag_immediate(u32 flag) {
    R0 = 0;
    R1 = flag;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool test_event_flag_sub_803537C(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls SetEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void set_event_flag_immediate(u32 flag) {
    R0 = 0;
    R1 = flag;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* Scan the EVENT_28 auto-increment table: if the current map group has an entry whose
 * event flag is set, re-arm EVENT_28 (which we cleared first so this routine can also
 * be the trigger). tool is the value the caller had in r10 (toolkit pointer). */
static void apply_autoinc_event_28(u32 tool) {
    u32 game_state = LD32(tool + oToolkit_GameStatePtr);
    u16 map_group = LD16(game_state + oGameState_MapGroup);

    clear_event_flag_immediate(0x28);

    const struct autoinc_entry *tbl = (const struct autoinc_entry *)A_word_80353B8;
    for (;;) {
        if (tbl->map_group == 0)
            break;
        if (tbl->map_group == map_group && test_event_flag_sub_803537C(tbl->event_flag)) {
            set_event_flag_immediate(0x28);
            break;
        }
        tbl++;
    }
}

Next sub_803537C(void) {
    apply_autoinc_event_28(R10);
    RETURN_TO_CALLER();
}

/* ── battle_loadDeathAnims_8035424 ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_28_active(void) {
    R0 = 0;
    R1 = 0x28;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* Z set ⇒ flag not set */
}

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static s32 get_battle_effects_sub_8035424(void) {
    GUEST_CALL(GetBattleEffects);
    return (s32)R0;
}

/* register form: calls LoadGFXAnims outside this file, which has no typed C interface yet */
static void load_gfx_anims(u32 gfx_anim_data) {
    R0 = gfx_anim_data;
    GUEST_CALL(LoadGFXAnims);
}

/* Load death-animation GFX scripts only when EVENT_28 is active
 * and no battle-effect flag bit 3 (value 8) is set. */
static void gate_load_death_gfx_anims(void) {
    if (!event_28_active())
        return;
    if (get_battle_effects_sub_8035424() & 8)
        return;
    load_gfx_anims(A_off_8035448);
}

Next battle_loadDeathAnims_8035424(void) {
    gate_load_death_gfx_anims();
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

/* ── sub_80355EC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: SetNaviStatsByte has no typed C interface yet */
static inline void call_SetNaviStatsByte(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(SetNaviStatsByte);
}

/* Update the "real-world map" flag (byte 0x22) in Navi 0's stats.
 * Walks a packed u16 table of special map IDs terminated by 0xFFFF.
 * If the current map ID is found in the table, the flag is set to 1;
 * otherwise it is cleared to 0. */
static void update_navi_stats_real_world(u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u8 map_group_byte = LD8(game_state + oGameState_MapGroup);
    u32 map_id = LD16(game_state + oGameState_MapGroup);

    /* High-byte of MapGroup >= 0x80 means we are in a real-world map;
     * use the saved real-world map ID instead of the live halfword. */
    if (map_group_byte >= 0x80)
        map_id = LD32(game_state + oGameState_SavedRealWorldMapId);

    /* Scan the u16 table (entry size 2 bytes, sentinel 0xFFFF). */
    u32 entry = A_word_803562C;
    for (;;) {
        u8 low_byte = LD8(entry);
        if (low_byte == 0xFF) {
            /* Sentinel reached – no match found. */
            call_SetNaviStatsByte(0, 0x22, 0);
            return;
        }
        u32 table_value = LD16(entry);
        if (table_value == map_id) {
            /* Map ID present in table – set the real-world flag. */
            call_SetNaviStatsByte(0, 0x22, 1);
            return;
        }
        entry += 2;
    }
}

/* sub_80355EC(r10 = toolkit ptr) -> preserved r4-r7; clobbers r0-r3, lr, flags */
Next sub_80355EC(void) {
    update_navi_stats_real_world(R10);
    RETURN_TO_CALLER();
}

/* ── sub_8035640 ── */
/* sub_8035640(r10 = some struct ptr)
 *
 * Walks a table of { id, event_flag } pairs at byte_8035670. For each entry,
 * if id != 0 AND id == *(u16*)(r10->field_3c + 4) AND the event flag in
 * field2 is set, trigger a graphics/animation load via LoadGFXAnims(off_8035688).
 * An entry with id == 0 terminates the table walk.
 *
 * inputs:  r10, lr
 * outputs: r2 r5 r7 r8 r9 r10 r11 r12
 * clobbers: nz c v r0 r1 r3 lr
 * preserved: r4 r5 r6 r7 r8 r9 r12
 */
Next sub_8035640(void) {
    u32 entry = A_byte_8035670;
    u32 base = LD32(R10 + 0x3C);
    u16 target = LD16(base + 4);

    for (;;) {
        u16 id = LD16(entry);
        SET_FLAGS_NZ(id);
        if (id == 0)
            break;

        u16 lookup = LD16(base + 4);
        SET_FLAGS_CMP(id, lookup);
        if (id != lookup)
            goto next_entry;

        R0 = (u32)LD16(entry + 2);
        GUEST_CALL(TestEventFlag);
        if (ZERO_FLAG())
            break;

        R0 = A_off_8035688;
        GUEST_CALL(LoadGFXAnims);
        break;

next_entry:
        entry += 4;
    }

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
