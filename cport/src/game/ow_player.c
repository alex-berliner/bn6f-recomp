/*
 * ow_player.c: readable functions of bn6f ow_player.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── calledWhenUsingSlipRun_809D6BC ── */
/* Per-direction overworld animation tables.  Each table holds two array
 * pointers, selected by the byte at eStruct2000aa0+3 (0 outside internet
 * map groups); each array holds one 12-byte entry per facing direction. */

typedef struct {
    u32 r0_out;        /* always 4: also stored to oOWPlayerObject_ActionSubState */
    u32 r1_out;        /* eStruct2000aa0 group byte in internet maps, else 0 */
    u32 r3_out;        /* caller's r3, passed through untouched */
    u32 r6_out;        /* game state pointer */
    u32 anim_array;    /* base of the selected per-direction animation array */
    u32 entry_offset;  /* facing direction * 12 (entry stride) */
} FacingAnimResult;

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool test_event_flag(u16 flag) {
    R0 = flag >> 8;             /* movflag passes the flag id as two bytes */
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();        /* Z set when the flag is clear */
}

/* Apply the overworld player animation for the current facing direction
 * (r5 = OW player object, r10 = toolkit).  While EVENT_1716 is set the
 * SlipRun variant of the table is used instead of the default one, unless
 * EVENT_173F is also set. */
static FacingAnimResult load_facing_anim(u32 r3, u32 player, u32 toolkit) {
    FacingAnimResult out;

    out.r3_out = r3;

    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    out.r6_out = game_state;

    /* The extra group byte only applies to internet map groups. */
    u32 group_byte = 0;
    if ((s32)LD8(game_state + oGameState_MapGroup) >= (s32)INTERNET_MAP_GROUP_START)
        group_byte = LD8(A_eStruct2000aa0 + oS2000aa0_Unk_03);
    out.r1_out = group_byte;

    u32 facing = LD8(player + oOWPlayerObject_FacingDirection);

    /* Pick the table: default unless EVENT_1716 says we are in the
     * SlipRun sequence and EVENT_173F does not override it. */
    u32 table_addr = A_off_809D71C;
    if (test_event_flag(EVENT_1716)) {
        if (test_event_flag(EVENT_IN_SLIPRUN_STATE) || !test_event_flag(EVENT_173F))
            table_addr = A_off_809D728;
    }

    u32 anim_array = LD32(table_addr + group_byte);
    u32 entry = anim_array + facing * 12;

    ST32(player + oOWPlayerObject_Unk_38, LD32(entry + 4));
    ST32(player + oOWPlayerObject_Unk_3c, LD32(entry + 8));
    ST8(player + oOWPlayerObject_AnimationSelect, LD8(entry));
    ST8(player + oOWPlayerObject_ActionSubState, 4);

    out.r0_out = 4;
    out.anim_array = anim_array;
    out.entry_offset = facing * 12;
    return out;
}

/* calledWhenUsingSlipRun_809D6BC(r3, r4, r5 = OW player object, r10 = toolkit)
 * Leaves r0 = 4, r1 = group byte, r3 = input r3, r6 = game state pointer,
 * r7 = address of the selected animation entry.  r4 and r5 pass through
 * untouched (the flag-test callee preserves them).  Flags end as N=0 Z=0
 * with C/V from the final address addition. */
Next calledWhenUsingSlipRun_809D6BC(void) {
    FacingAnimResult out = load_facing_anim(R3, R5, R10);
    R0 = out.r0_out;
    R1 = out.r1_out;
    R3 = out.r3_out;
    R6 = out.r6_out;
    R7 = out.anim_array + out.entry_offset;
    SET_FLAGS_ADD(out.anim_array, out.entry_offset);
    RETURN_TO_CALLER();
}

/* ── ow_player_processAction_809D75E ── */
/* Offsets within the animation/event struct at eStruct200ace0 (0x200ACE0). */
#define oAnimState_TransitionFlag      0x01
#define oAnimState_PendingAnimation    0x1C
#define PENDING_ANIM_NONE              0xFF

/* register form: computed dispatch to player action handler via jump table;
 * the handler modifies caller-visible registers r0-r6 and r8-r12 */
static void ow_player_dispatch_action(u32 player_obj) {
    u32 idx = LD8(player_obj + oOWPlayerObject_ActionSubState);
    R5 = player_obj;
    switch (idx) {
        case 0: GUEST_CALL(sub_809D7A8); break;
        case 4: GUEST_CALL(ow_player_setCounterTalkStateAfterNpcInteract_809D7C8); break;
        case 8: GUEST_CALL(owPlayerObject_clearStateUnk10_809D7D8); break;
    }
}

/* register form: calls owPlayer_isJumptableIndex09At0x10_0809DF28 outside this file, which has no typed C interface yet */
static u32 call_scene_transition_check(void) {
    R0 = PENDING_ANIM_NONE;
    R7 = A_eStruct200ace0;
    GUEST_CALL(owPlayer_isJumptableIndex09At0x10_0809DF28);
    /* Original next instruction: tst r0,r0 (sets N,Z; C,V unchanged from callee) */
    SET_FLAGS_NZ(R0);
    return R0;
}

/* register form: calls owPlayer_setJumptableIndex09AndOthers_809e004 outside this file */
static u32 call_set_jumptable(u32 index, u32 game_state) {
    R0 = index;
    R7 = game_state;
    GUEST_CALL(owPlayer_setJumptableIndex09AndOthers_809e004);
    return R0;
}

/* Process one player overworld action: dispatch the type-specific handler,
 * consume any pending animation it requested, then attempt a scene transition. */
static u32 ow_player_process_action(u32 player_obj, u32 toolkit) {
    /* Run the action handler (dispatch via jump table) */
    ow_player_dispatch_action(player_obj);

    /* Consume pending animation written by the handler, if any */
    u32 anim_struct = A_eStruct200ace0;
    u32 pending = LD8(anim_struct + oAnimState_PendingAnimation);
    if (pending != PENDING_ANIM_NONE) {
        ST8(player_obj + oOWPlayerObject_AnimationSelect, pending);
        ST8(anim_struct + oAnimState_PendingAnimation, PENDING_ANIM_NONE);
    }

    /* Check whether a scene transition should begin */
    u32 transition = call_scene_transition_check();
    if (transition == 0)
        return 0;

    /* Begin scene transition: set flag, clear state, queue jumptable */
    ST8(anim_struct + oAnimState_TransitionFlag, 2);
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    ST8(game_state + oGameState_OWPlayerBusyOrCutscene, 0);
    ST8(player_obj + oOWPlayerObject_Unk_07, 0);

    return call_set_jumptable(0x10, game_state);
}

/* ow_player_processAction_809D75E(r5 = player object, r10 = toolkit) -> r0, nz, c, v */
Next ow_player_processAction_809D75E(void) {
    R0 = ow_player_process_action(R5, R10);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── ow_player_setCounterTalkStateAfterNpcInteract_809D7C8 ── */
/* ow_player_setCounterTalkStateAfterNpcInteract_809D7C8: the player object (r5) just finished interacting with an NPC.
 * Read the NPC's movement flag; if it is 4, store 8 into the player's
 * object field Unk_0a (marks the "talking to counter NPC" state).
 * r0 ends up holding the movement flag (or 8 when the store happens),
 * r7 ends up holding the NPC object pointer. Flags: CMP result, except
 * that in the store path the final "mov r0,#8" refreshes only N/Z while
 * C/V keep the values from the CMP. */
Next ow_player_setCounterTalkStateAfterNpcInteract_809D7C8(void) {
    R7 = LD32(R5 + oOWPlayerObject_interactedNPCObject_58);
    R0 = LD8(R7 + oOverworldNPCObject_MovementFlag_0a);

    SET_FLAGS_CMP(R0, 4);
    if (R0 == 4) {
        R0 = 8;
        ST8(R5 + oOWPlayerObject_ActionSubState, R0);
        /* mov r0,#8 only updates N/Z; C/V stay as the CMP left them */
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── overworldObj_testStateBit10_809DAF8 ── */
/* overworldObj_testStateBit10_809DAF8(r5 = overworld object):
 * Tests the object's state flags at +0x4C for bit 4 (0x10).
 * Returns: r0 = flag value (0 or 0x10), r1 = 0x10, r3 = 0x4C. Flags reflect r0. */
Next overworldObj_testStateBit10_809DAF8(void) {
    R3 = 0x4C;
    R0 = LD16(R5 + R3);
    R1 = 0x10;
    R0 &= R1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── event_TestPlayerAdvanceForward_809DB50 ── */
/* event_TestPlayerAdvanceForward_809DB50: check the queued "player advance forward" event request.
 *
 * Asks TestEventFlagFromImmediate about EVENT_1717_PLAYER_ADVANCE_FORWARD
 * (flag word 0x17, bit 0x17, set up by the movflag pseudo-op). The helper
 * answers through the condition flags, and this function passes those flags
 * straight through to its caller. If the request is still pending, everything
 * is left exactly as the helper left it; otherwise R0 is zeroed ("movs r0,#0")
 * so the caller reads a clean "no". R0 is scratch here, so zeroing it only
 * matters for the caller's view of the result. R3 passes through untouched. */
Next event_TestPlayerAdvanceForward_809DB50(void) {
    /* movflag EVENT_1717_PLAYER_ADVANCE_FORWARD: flag word 0x17, bit 0x17 */
    R0 = 0x17;
    R1 = 0x17;

    GUEST_CALL(TestEventFlagFromImmediate);

    /* bne locret: with the request still pending (Z clear), keep the
     * helper's result as-is; otherwise clear R0, which holds the flag's
     * whole byte and can be nonzero with this flag clear. */
    if (cpu.nz == 0) {
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── owPlayerInputTick_809DB60 ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_u16(u16 bank, u16 flag) {
    R0 = bank;
    R1 = flag;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* !Z if the flag is set */
}

/* register form: calls createOWPlayerInteractionArea_809DBC4 outside this file, which has no typed C interface yet */
static void call_809DBC4(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(createOWPlayerInteractionArea_809DBC4);
}

/* Run the player's overworld input/animation tick:
 *  - prime Unk_34 from a small ROM blob;
 *  - if the slip-run event is set, or arrows are held, refresh Unk_50 from the table at off_809DCEC
 *    indexed by facing;
 *  - if game-state byte 0x11 is set, refresh Unk_48 from the table at off_809DC0C indexed by facing;
 *  - always clear Unk_34, Unk_48 and Unk_50 on the player object. */
static void ow_player_input_tick(u32 player_r5, u32 toolkit_r10) {
    /* Prime Unk_34 with the base animation data. */
    call_809DBC4(A_byte_809DBF4, player_r5 + oOWPlayerObject_Unk_34);

    /* Skip arrows check when the slip-run event is active. */
    bool slip = event_flag_set_u16(0x17, 0x3D); /* EVENT_IN_SLIPRUN_STATE */
    u32 joypad_ptr = LD32(toolkit_r10 + oToolkit_JoypadPtr);
    u32 held = LD16(joypad_ptr);               /* oJoypad_Held */
    bool arrows_held = (held & JOYPAD_ARROWS) != 0;

    if (slip || arrows_held) {
        u32 facing = LD8(player_r5 + oOWPlayerObject_FacingDirection);
        u32 ptr = LD32(A_off_809DCEC + facing * 4);
        call_809DBC4(ptr, player_r5 + oOWPlayerObject_Unk_50);
    }

    u32 gamestate_ptr = LD32(toolkit_r10 + oToolkit_GameStatePtr);
    if (LD8(gamestate_ptr + oGameState_Unk_11) != 0) {
        u32 facing = LD8(player_r5 + oOWPlayerObject_FacingDirection);
        u32 ptr = LD32(A_off_809DC0C + facing * 4);
        call_809DBC4(ptr, player_r5 + oOWPlayerObject_Unk_48);
    }

    /* Always clear the three animation slots on the player object. */
    ST32(player_r5 + oOWPlayerObject_Unk_34, 0);
    ST32(player_r5 + oOWPlayerObject_Unk_48, 0);
    ST32(player_r5 + oOWPlayerObject_Unk_50, 0);
}

Next owPlayerInputTick_809DB60(void) {
    /* r5 must be preserved (per contract). */
    ow_player_input_tick(R5, R10);
    RETURN_TO_CALLER();
}

/* ── createOWPlayerInteractionArea_809DBC4 ── */
/* register form: calls createOWObjectInteractionArea_80037ac outside this file,
 * which has no typed C interface yet */
static void call_create_ow_interaction_area(s32 world_x, s32 world_y, s32 world_z,
                                            u32 radius, u32 flags_10, u32 flags_14,
                                            u32 output_handle, u32 area_struct) {
    u32 saved_r5 = R5;  /* player-object pointer must survive; original pops it after the call */
    R0 = (u32)world_x;
    R1 = (u32)world_y;
    R2 = (u32)world_z;
    R3 = radius;
    R4 = flags_10;
    R5 = flags_14;
    R6 = output_handle;
    R7 = area_struct;
    GUEST_CALL(createOWObjectInteractionArea_80037ac);
    R5 = saved_r5;
}

/* Offset the player's world position by the interaction-area's local offsets,
 * clear the caller's output handle, then create the area. */
static void setup_ow_player_interaction_area(u32 area_ptr, u32 output_ptr,
                                             u32 player_obj_ptr) {
    ST32(output_ptr, 0);

    s32 world_x = (s32)LD32(player_obj_ptr + oOWPlayerObject_X)
                + (s32)LD32(area_ptr + oOWPlayerInteractionArea_XOffset);
    s32 world_y = (s32)LD32(player_obj_ptr + oOWPlayerObject_Y)
                + (s32)LD32(area_ptr + oOWPlayerInteractionArea_YOffset);
    s32 world_z = (s32)LD32(player_obj_ptr + oOWPlayerObject_Z)
                + (s32)LD32(area_ptr + oOWPlayerInteractionArea_ZOffset);

    u32 radius   = LD32(area_ptr + oOWPlayerInteractionArea_Radius_ZReachAmount);
    u32 flags_10 = LD32(area_ptr + oOWPlayerInteractionArea_Flags_10);
    u32 flags_14 = LD32(area_ptr + oOWPlayerInteractionArea_Flags_14);

    call_create_ow_interaction_area(world_x, world_y, world_z,
                                    radius, flags_10, flags_14,
                                    output_ptr, area_ptr);
}

/* createOWPlayerInteractionArea_809DBC4(r0 = interaction-area struct, r1 = output handle ptr, r5 = player object) */
Next createOWPlayerInteractionArea_809DBC4(void) {
    setup_ow_player_interaction_area(R0, R1, R5);
    /* r1, r2, r4, r6, r7, c, v left in registers by the callee's GUEST_CALL; r5 preserved */
    RETURN_TO_CALLER();
}

/* ── owPlayer_TickCountdownOrWait_809DDCC ── */
/* owPlayer_TickCountdownOrWait_809DDCC(r5 = overworld player object):
 * Tick the object's Unk_18 countdown timer. While it is still counting down,
 * decrement it and return 1. Once it hits zero, only fail (return 0) while
 * one of the 0xC00000 status bits of Unk_50 is set; otherwise return 1.
 * r0 = result. r2 = 0xC00000 on the status-check path (untouched otherwise).
 * c/v come from the countdown SUBS on the timer path, and are left as the
 * caller set them on the status-check path (TST only updates nz). */
Next owPlayer_TickCountdownOrWait_809DDCC(void) {
    u32 countdown = LD16(R5 + oOWPlayerObject_Unk_18);

    if (countdown != 0) {
        /* Timer still running: tick it down and report done. */
        SET_FLAGS_CMP(countdown, 1); /* from SUBS r1, #1 */
        ST16(R5 + oOWPlayerObject_Unk_18, countdown - 1);
        R0 = 1;
    } else {
        /* Timer expired: check the blocking status bits in Unk_50. */
        u32 status = LD32(R5 + oOWPlayerObject_Unk_50);
        u32 blocking = status & 0xC00000;
        SET_FLAGS_NZ(blocking); /* from TST r2, r1 */
        R2 = 0xC00000;
        R0 = (blocking != 0) ? 0 : 1;
    }
    RETURN_TO_CALLER();
}

/* ── oowExitSlipRun_809DDF0 ── */
/* Event flags (page 0x17) used by this routine */
#define FLAG_PAGE_sub_809DDF0                  0x17
#define FLAG_EVENT_1716            0x16
#define FLAG_EVENT_IN_SLIPRUN      0x3D
#define FLAG_EVENT_173E            0x3E
#define FLAG_EVENT_173F            0x3F

/* Bit mask indicating the player's slip-run state is still active */
#define SLIPRUN_ACTIVE_MASK        0xC00000u

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_809DDF0(u32 page, u32 index) {
    R0 = page;
    R1 = index;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void event_flag_clear(u32 page, u32 index) {
    R0 = page;
    R1 = index;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls SetEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void event_flag_set_now(u32 page, u32 index) {
    R0 = page;
    R1 = index;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* register form: calls sub_809DE60 outside this file (leaf handler, no typed C interface yet) */
static void run_sliprun_transition(void) {
    GUEST_CALL(sub_809DE60);
}

/* register form: equivalent of "ldr r1,[r5,#o..]; ldr r2,=0xC00000; tst r2,r1" —
 * sets R1, R2, N, Z exactly as the TST does; C and V are preserved. */
static bool sliprun_bit_active(u32 player) {
    R1 = LD32(player + oOWPlayerObject_Unk_50);
    R2 = SLIPRUN_ACTIVE_MASK;
    u32 masked = R1 & R2;
    SET_FLAGS_NZ(masked);
    return masked != 0;
}

/* If the player is in a slip-run state (and the relevant event flags are set),
 * exit it: clear the slip-run and 173E flags, set 173F, and run the transition handler. */
static void sliprun_state_exit(u32 player) {
    if (!event_flag_set_sub_809DDF0(FLAG_PAGE_sub_809DDF0, FLAG_EVENT_1716))
        return;
    if (!event_flag_set_sub_809DDF0(FLAG_PAGE_sub_809DDF0, FLAG_EVENT_IN_SLIPRUN))
        return;
    if (!sliprun_bit_active(player))
        return;

    event_flag_clear(FLAG_PAGE_sub_809DDF0, FLAG_EVENT_IN_SLIPRUN);
    event_flag_clear(FLAG_PAGE_sub_809DDF0, FLAG_EVENT_173E);
    event_flag_set_now(FLAG_PAGE_sub_809DDF0, FLAG_EVENT_173F);
    run_sliprun_transition();
}

/* oowExitSlipRun_809DDF0(r5 = player object) → r0-r3, r6, r7, nz, c, v, lr */
Next oowExitSlipRun_809DDF0(void) {
    sliprun_state_exit(R5);
    RETURN_TO_CALLER();
}

/* ── endSlipRunState_809DE30 ── */
/* Event-flag page byte used by all four flags in this routine */
#define FLAG_PAGE_sub_809DE30           0x17u
#define EVENT_1716_BYTE     0x16u   /* gate: must be set to proceed */
#define EVENT_IN_SLIPRUN    0x3Du   /* player is currently slip-running */
#define EVENT_173F_BYTE     0x3Fu   /* set on slip-run exit */
#define EVENT_173E_BYTE     0x3Eu   /* cleared on slip-run exit */

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool flag_test(u16 page, u16 byte) {
    R0 = page;
    R1 = byte;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();          /* TestEventFlagFromImmediate: Z set ⇒ flag clear */
}

/* register form: calls SetEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void flag_set(u16 page, u16 byte) {
    R0 = page;
    R1 = byte;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void flag_clear(u16 page, u16 byte) {
    R0 = page;
    R1 = byte;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* End slip-run state if the EVENT_1716 gate is active.
 * If the player is mid slip-run, mark the exit event, then clear both the
 * slip-run state and the transition flag.  Output registers/flags are left
 * by the callee on whichever path is taken. */
static void end_sliprun_state(void) {
    if (!flag_test(FLAG_PAGE_sub_809DE30, EVENT_1716_BYTE))
        return;                   /* gate not set – early exit (locret) */

    if (flag_test(FLAG_PAGE_sub_809DE30, EVENT_IN_SLIPRUN))
        flag_set(FLAG_PAGE_sub_809DE30, EVENT_173F_BYTE);

    flag_clear(FLAG_PAGE_sub_809DE30, EVENT_IN_SLIPRUN);
    flag_clear(FLAG_PAGE_sub_809DE30, EVENT_173E_BYTE);
}

Next endSlipRunState_809DE30(void) {
    end_sliprun_state();
    RETURN_TO_CALLER();
}

/* ── ow_gatePendingTileInteraction_809DED4 ── */
/* register form: calls TestPETMenuDataFlag outside this file, which has no typed C interface yet */
static bool pet_menu_data_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestPETMenuDataFlag);
    return !ZERO_FLAG();   /* returns non-zero (Z clear) when the flag is active */
}

/* register form: calls IsCutsceneScriptNonNull outside this file, which has no typed C interface yet */
static bool cutscene_script_is_null(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return ZERO_FLAG();    /* Z set means the script pointer is null (no cutscene) */
}

/* register form: calls ow_tryTileInteraction_809DF00 outside this file, which has no typed C interface yet */
static u32 ow_try_tile_interaction(u32 arg0, u32 player_obj) {
    R0 = arg0;
    R5 = player_obj;
    GUEST_CALL(ow_tryTileInteraction_809DF00);
    return R0;
}

/*
 * Gate a pending overworld tile-interaction request.
 *
 * If the PET-menu data flag is set or a cutscene is playing, the pending
 * action byte (oOWPlayerObject_Unk_07) is cleared and no dispatch occurs.
 * Otherwise the byte indexes a single-entry jump table (off_809DEF8) that
 * dispatches to ow_tryTileInteraction_809DF00.
 *
 * Returns the resulting r0: 0 on the suppressed path, or the callee's value.
 */
static u32 ow_gate_tile_interaction(u32 player_obj) {
    if (pet_menu_data_flag_set(1) || !cutscene_script_is_null()) {
        /* Interaction suppressed – clear the pending action byte */
        ST8(player_obj + oOWPlayerObject_Unk_07, 0);
        return 0;
    }

    /* Dispatch via the jump table (only entry → ow_tryTileInteraction). */
    u32 table_offset = (u32)LD8(player_obj + oOWPlayerObject_Unk_07) << 2;
    return ow_try_tile_interaction(table_offset, player_obj);
}

/* ow_gatePendingTileInteraction_809DED4(r5 = player-object pointer) -> r0; flags left by callees */
Next ow_gatePendingTileInteraction_809DED4(void) {
    R0 = ow_gate_tile_interaction(R5);
    RETURN_TO_CALLER();
}

/* ── ow_tryTileInteraction_809DF00 ── */
/* ow_tryTileInteraction_809DF00(r5 = overworld player object, r10 = toolkit):
 * Called when the player tries to interact with the tile in front of them.
 * ow_findInteractableNPC_809E01C checks whether tile interaction is currently blocked (e.g. by
 * a cutscene); if so, the game state byte oGameState_OWPlayerBusyOrCutscene is set to 1 and
 * the function returns 1. Otherwise the tile's interaction value
 * (oOWPlayerObject_Unk_0f) is copied into oGameState_CoordInteractionValue,
 * and if it is nonzero it is also latched into game state offset 0x03, then
 * the function returns 0. R7 is left pointing at the game state struct; NZ
 * reflect the return value. */
Next ow_tryTileInteraction_809DF00(void) {
    GUEST_CALL(ow_findInteractableNPC_809E01C);

    if (R0 == 0) {
        /* Interaction is blocked: flag the state and report "blocked". */
        R7 = LD32(R10 + oToolkit_GameStatePtr);
        ST8(R7 + oGameState_OWPlayerBusyOrCutscene, 1);
        R0 = 1;
    } else {
        /* Interaction allowed: forward the tile's interaction value. */
        u32 interactionValue = LD8(R5 + oOWPlayerObject_Unk_0f);
        R7 = LD32(R10 + oToolkit_GameStatePtr);
        ST8(R7 + oGameState_CoordInteractionValue, interactionValue);
        if (interactionValue != 0)
            ST8(R7 + 3, interactionValue);
        R0 = 0;
    }

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── owPlayer_isJumptableIndex09At0x10_0809DF28 ── */
/* owPlayer_isJumptableIndex09At0x10_0809DF28(r5 = OWPlayerObject*)
 *
 * Overworld player per-frame dispatcher. Inspects the player's current
 * substate (jumptable index field). Substate 0x10 is the "locked /
 * skip-this-tick" state: in that case we just return 0. Otherwise we
 * fall through into the real per-frame handler (owMapObject_checkPlayerFacingFreeze_809DF36) at
 * eStruct200ace0 and propagate whatever it returns. R7 is loaded with
 * the handler address so callers can recognize which path was taken.
 *
 *   inputs:  r5
 *   outputs: r0, r7, c, v
 *   clobbers: nz
 */
Next owPlayer_isJumptableIndex09At0x10_0809DF28(void) {
    /* Original: push {lr} ... pop {pc}. Preserve lr across the call
     * so that owMapObject_checkPlayerFacingFreeze_809DF36's pop {pc} returns to our caller. */
    u32 saved_lr = LR;
    SP -= 32;
    ST32(SP, saved_lr);

    /* Load the handler address into r7; callers branch on it. */
    R7 = A_eStruct200ace0;

    /* Load jt_index and compare against 0x10 -- sets nz/c/v. */
    u8 jt_index = LD8(R5 + oOWPlayerObject_JumptableIndex_09);
    SET_FLAGS_CMP(jt_index, 0x10);

    if (jt_index == 0x10) {
        /* Locked substate: return 0, leaving c/v as the CMP set them. */
        R0 = 0;
        LR = LD32(SP);
        SP += 32;
        RETURN_TO_CALLER();
    }

    /* Not the locked substate: fall through into the real handler.
     * It returns its own result in r0 and sets c/v itself. */
    LR = LD32(SP);
    SP += 32;
    GUEST_CALL(owMapObject_checkPlayerFacingFreeze_809DF36);
    RETURN_TO_CALLER();
}

/* ── owPlayer_isMoving_809DFA0 ── */
/* owPlayer_isMoving_809DFA0(r5 = overworld player object):
 * Reports whether the player object is still in motion. The player counts as
 * moving as long as any of its current X/Y/Z coordinates differs from the
 * corresponding "next" coordinate it is heading toward.
 *
 * Returns r0 = 1 if some axis still differs, r0 = 0 if the player is
 * stationary. r1/r2 are left holding the last pair of coordinates that was
 * compared, and the condition flags are exactly what the final CMP of that
 * pair left (callers branch on them right after the call). */

/* Compare one axis of the player object: load cur/next, leave them in r1/r2,
 * set the flags as the original's CMP does, and return whether they differ.
 * The caller short-circuits, so only the axes actually compared by the
 * original touch the flags. */
static s32 player_axis_moved(u32 obj, u32 curOff, u32 nextOff) {
    u32 cur = LD32(obj + curOff);
    u32 next = LD32(obj + nextOff);
    R1 = cur;
    R2 = next;
    SET_FLAGS_CMP(cur, next);
    return cur != next;
}

Next owPlayer_isMoving_809DFA0(void) {
    R0 = player_axis_moved(R5, oOWPlayerObject_X, oOWPlayerObject_NextX)
      || player_axis_moved(R5, oOWPlayerObject_Y, oOWPlayerObject_NextY)
      || player_axis_moved(R5, oOWPlayerObject_Z, oOWPlayerObject_NextZ);

    RETURN_TO_CALLER();
}

/* ── owPlayer_setJumptableIndex09AndOthers_809e004 ── */
/* owPlayer_setJumptableIndex09AndOthers_809e004(r0 = value, r5 = overworld player object):
 * Clears the player's Unk_0a byte, then stores r0 either into Unk_0b (when the
 * current jumptable index is 0x10) or into the jumptable index itself (offset 9).
 * Leaves r1 = the previous jumptable index; nz/c/v come from comparing it with 0x10. */
Next owPlayer_setJumptableIndex09AndOthers_809e004(void) {
    u32 value = R0;
    u32 player = R5;

    ST8(player + oOWPlayerObject_ActionSubState, 0);

    u32 jumptableIndex = LD8(player + 9);
    SET_FLAGS_CMP(jumptableIndex, 0x10);

    if (jumptableIndex == 0x10)
        ST8(player + oOWPlayerObject_Unk_0b, value);
    else
        ST8(player + 9, value);

    R1 = jumptableIndex;
    RETURN_TO_CALLER();
}

/* ── ow_findInteractableNPC_809E01C ── */
/* ow_findInteractableNPC_809E01C(r5 = player object):
 * Walk the overworld NPC object array looking for the first NPC the player can
 * interact with (slot in use, interaction not disabled) and record it in the
 * player's interactedNPCObject slot.
 * Returns r0 = 0 when an NPC was found; if the first live NPC has interaction
 * disabled, r0 is that NPC's flags word and nothing is stored; r0 = 1 when no
 * live NPC exists.
 * r1 = 1 and r2 = one-past-end of the NPC array on every exit. */
Next ow_findInteractableNPC_809E01C(void) {
    u32 player = R5;
    u32 npc = A_eOverworldNPCObjects; /* == eOverworldNPCObjects */
    u32 arrayEnd = npc + 0xD80; /* dword_809E048: byte size of the NPC array */

    SET_FLAGS_ADD(0xD80, npc); /* add r2, r2, r7 */
    R1 = 1;
    R2 = arrayEnd;

    while ((s32)npc < (s32)arrayEnd) {
        u32 state = LD32(npc + oOverworldNPCObject_Unk_50);
        if ((state & 1) != 0) {
            /* NPC slot is live: skip it if interaction is disabled. */
            u32 unkFlags = LD32(npc + oOverworldNPCObject_UnkFlags_60);
            SET_FLAGS_NZ(unkFlags & 1); /* OW_NPC_UNK_FLAGS_60 bit 0 */
            if ((unkFlags & 1) == 0) {
                ST32(player + oOWPlayerObject_interactedNPCObject_58, npc);
                R0 = 0;
                RETURN_TO_CALLER();
            }
            R0 = unkFlags;
            RETURN_TO_CALLER();
        }
        npc += oOverworldNPCObject_Size;
        SET_FLAGS_CMP(npc, arrayEnd);
    }

    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── owPlayer_ClearStateBytes_D_E_F_809E04C ── */
/* owPlayer_ClearStateBytes_D_E_F_809E04C:
 * Clear three state bytes (0xD..0xF) of the overworld player object.
 * The caller's r0/r1 are passed through untouched. */
Next owPlayer_ClearStateBytes_D_E_F_809E04C(void) {
    u32 player = A_eOWPlayerObject;

    ST8(player + 0xD, 0); /* byte_2009F4D */
    ST8(player + 0xE, 0); /* byte_2009F4E */
    ST8(player + 0xF, 0); /* byte_2009F4F */

    RETURN_TO_CALLER();
}

/* ── spawnOWPlayerObjectForEnterMap_809e064 ── */
/* spawnOWPlayerObjectForEnterMap_809e064:
 * Spawn the overworld player object when entering a map, then hook it into
 * the game state: record it as the current overworld player object and copy
 * the player's saved X/Y/Z position and facing direction into it.
 *
 * Inputs: r1-r3, r6, r7 are passed through to SpawnOWPlayerObject;
 *         r10 = toolkit pointer.
 * Outputs: r5 = spawned object pointer (0 if none spawned).
 * r2, r3, r4, r6, r7 are preserved: the original pushes r4/r6/r7 and pops
 * them on return, so the scratch game-state pointer it keeps in r6 never
 * escapes to the caller. */
Next spawnOWPlayerObjectForEnterMap_809e064(void) {
    /* Mirror the original's push {r4,r6,r7,lr}: these three registers are
     * restored to their entry values no matter what the spawner did. */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    R0 = 0; /* SpawnOWPlayerObject: create a fresh object */
    R4 = 0;
    GUEST_CALL(SpawnOWPlayerObject);

    u32 player = R5;
    if (player != 0) {
        /* Keep the game-state pointer in a local: the original holds it in
         * r6 only transiently and pops the caller's value back over it. */
        u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

        ST32(gameState + oGameState_OverworldPlayerObjectPtr, player);
        ST32(player + oOWPlayerObject_X, LD32(gameState + oGameState_PlayerX));
        ST32(player + oOWPlayerObject_Y, LD32(gameState + oGameState_PlayerY));
        ST32(player + oOWPlayerObject_Z, LD32(gameState + oGameState_PlayerZ));
        ST8(player + oOWPlayerObject_FacingDirection,
            LD32(gameState + oGameState_FacingDirectionAfterWarp));
    }

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── SetPlayerCanMoveEventFlag ── */
/* SetPlayerCanMoveEventFlag: mark the on-map player as free to move again
 * by setting the "player can move" event flag (flag id 0x17, value 0x14).
 * Inputs: none. Outputs: none; clobbers r0, r1, r3, LR, flags. */
Next SetPlayerCanMoveEventFlag(void) {
    /* SetEventFlagFromImmediate(r0 = flag id, r1 = value to store) */
    R0 = 0x17; /* EVENT_PLAYER_CAN_MOVE */
    R1 = 0x14;
    GUEST_CALL(SetEventFlagFromImmediate);
    RETURN_TO_CALLER();
}

/* ── ClearPlayerCanMoveEventFlag ── */
/* ClearPlayerCanMoveEventFlag: clear the "player can move" event flag
 * (flag word 0x17, bit 0x14 of the event flag work area at 0x2001c88,
 * i.e. the bit at 0x2001f6a). Delegates to the generic flag-clearing
 * helper, which leaves r0/r1/r3 and the nz/c/v flags as its callers
 * expect. */
Next ClearPlayerCanMoveEventFlag(void) {
    R0 = 0x17; /* EVENT_PLAYER_CAN_MOVE: flag word index */
    R1 = 0x14; /* bit index within that word */

    GUEST_CALL(ClearEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── owPlayer_lockPlayerForNonNPCDialogue_809E0B0 ── */
/* owPlayer_lockPlayerForNonNPCDialogue_809E0B0:
 * Lock the player in place for dialogue with non-NPC objects (pressing L,
 * jacking in, talking to objects): sets the "player is busy" byte, marks
 * event EVENT_1718 (which also relocates the map position on jacking in),
 * and clears the player-can-move event flag.
 * Leaves r0/r1/r3 and the flags as SetEventFlagFromImmediate and
 * ClearPlayerCanMoveEventFlag leave them; returns to the caller's LR. */
Next owPlayer_lockPlayerForNonNPCDialogue_809E0B0(void) {
    /* mark the player as locked into dialogue */
    R3 = A_eStruct200ace0;
    ST8(R3, 1);

    /* set event flag EVENT_1718 (bank 0x17, index 0x18) */
    R0 = 0x17;
    R1 = 0x18;
    GUEST_CALL(SetEventFlagFromImmediate);

    /* and forbid free movement until the dialogue releases the player */
    GUEST_CALL(ClearPlayerCanMoveEventFlag);

    RETURN_TO_CALLER();
}

/* ── owPlayer_809E0C8 ── */
/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_owPlayer_809E0C8(u16 flag) {
    R0 = (u8)(flag >> 8);   /* page */
    R1 = (u8)(flag & 0xFF); /* offset within page */
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();  /* Z clear means the flag IS set */
}

/* Synchronize overworld player position into the shared struct at 0x200ACE0.
 * Also writes the player's facing direction (low 3 bits) into the animation-select
 * field, but only when event flag 0x1719 has NOT been set yet. */
static void owPlayer_sync_to_shared_struct(u32 toolkit) {
    u32 shared = A_eStruct200ace0;

    /* Toolkit → game state → overworld player object */
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 player_obj = LD32(game_state + oGameState_OverworldPlayerObjectPtr);

    /* Copy position (X, Y, Z) */
    ST32(shared + oS200ace0_PlayerX, LD32(player_obj + oOWPlayerObject_X));
    ST32(shared + oS200ace0_PlayerY, LD32(player_obj + oOWPlayerObject_Y));
    ST32(shared + oS200ace0_PlayerZ, LD32(player_obj + oOWPlayerObject_Z));

    /* Facing direction is only mirrored when event 0x1719 is still clear */
    if (!event_flag_set_owPlayer_809E0C8(0x1719)) {
        u8 facing = (u8)LD8(player_obj + oOWPlayerObject_FacingDirection) & 7;
        ST8(shared + oS200ace0_owPlayerFixedAnimationSelect_02, facing);
    }
}

Next owPlayer_809E0C8(void) {
    owPlayer_sync_to_shared_struct(R10);
    RETURN_TO_CALLER();
}

/* ── owPlayer_unlockPlayerAfterNonNPCDialogue_809E122 ── */
/* owPlayer_unlockPlayerAfterNonNPCDialogue_809E122:
 * Called after talking to an overworld character that is not an NPC-scripted
 * interaction: unlock the player again. Clears the "dialogue in progress"
 * byte of eStruct200ace0, lets the player move again via the event-flag
 * system, and clears story event flag 0x1719. */
#define EVENT_1719 0x1719

Next owPlayer_unlockPlayerAfterNonNPCDialogue_809E122(void) {
    /* Clear the dialogue-busy byte at eStruct200ace0. */
    ST8(A_eStruct200ace0, 0);

    /* Re-enable player movement. */
    GUEST_CALL(SetPlayerCanMoveEventFlag);

    /* Clear event flag 0x1719 (ClearEventFlagFromImmediate takes the flag
     * number split into a high byte in r0 and a low byte in r1). */
    R0 = EVENT_1719 >> 8;
    R1 = EVENT_1719 & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── owPlayer_setS200ace0_fixedAnimationSelect_809e13c ── */
/* owPlayer_setS200ace0_fixedAnimationSelect_809e13c(r0 = animation id):
 * Store the fixed overworld player animation id into eStruct200ace0+2 and set
 * event flag EVENT_1719 (passed to SetEventFlagFromImmediate as group 0x17,
 * flag 0x19). Other registers and flags come back from that call untouched. */
Next owPlayer_setS200ace0_fixedAnimationSelect_809e13c(void) {
    /* ldr r3, =eStruct200ace0; strb r0, [r3, #2] (field offset 2) */
    ST8(A_eStruct200ace0 + 2, R0);

    /* movflag EVENT_1719; movs r1, #25 */
    R0 = 0x17;
    R1 = 0x19;
    GUEST_CALL(SetEventFlagFromImmediate);

    RETURN_TO_CALLER();
}

/* ── owPlayer_RequestFixedAnimation_809E14C ── */
/* owPlayer_RequestFixedAnimation_809E14C(r0 = fixed-animation id):
 * Called when the overworld player is told to play a fixed animation.
 * owPlayer_isBusyOrCutsceneActive_809E462 decides whether that animation can take effect immediately:
 *   - if it returns 0 (Z set), the id is written to the fixed-animation
 *     select byte at eStruct200ace0+2 (byte_200ACE2) and the event flag
 *     EVENT_1719 (bank 0x17, flag 0x19) is raised;
 *   - otherwise the request is deferred by parking the id in the byte at
 *     eStruct200ace0+0x1c (byte_200ACFC).
 * r4-r7 are preserved (the id is passed to owPlayer_isBusyOrCutsceneActive_809E462 in r4/r7's struct
 * pointer, but both are restored before returning). */
Next owPlayer_RequestFixedAnimation_809E14C(void) {
    u32 anim = R0;
    u32 saved_r4 = R4;
    u32 saved_r7 = R7;

    R4 = anim;                  /* mov r4, r0: owPlayer_isBusyOrCutsceneActive_809E462 reads r4 */
    R7 = A_eStruct200ace0;      /* ldr r7, =eStruct200ace0 */

    /* owPlayer_isBusyOrCutsceneActive_809E462 reports in Z whether the animation can be applied now. */
    GUEST_CALL(owPlayer_isBusyOrCutsceneActive_809E462); /* answers in Z; the original branches on it */

    if (ZERO_FLAG()) {
        /* Approved: select the animation right away and flag the event. */
        ST8(A_eStruct200ace0 + 2, R4);  /* owPlayerFixedAnimationSelect */

        /* movflag EVENT_1719 -> SetEventFlagFromImmediate(0x17, 0x19) */
        R0 = 0x17;
        R1 = 0x19;
        GUEST_CALL(SetEventFlagFromImmediate);
    } else {
        /* Denied: remember the requested animation for later. */
        ST8(A_eStruct200ace0 + oS200ace0_Unk_1c, R4);
    }

    /* pop {r4-r7,pc} */
    R4 = saved_r4;
    R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188 ── */
/* owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188
 *   (r0 = new X, r1 = new Y, r2 = new Z):
 * Shifts the overworld player's current position into its "next" position
 * slots, then writes the given coordinates as the player's new position.
 * Finally mirrors the new position into the shared overworld-player state
 * (s200ace0) via the indirect setter, as a tail call.
 * On return, r3 holds the s200ace0 base address (left by the tail call);
 * r0-r2 and r7 are unchanged. */
Next owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188(void) {
    u32 player = LD32(LD32(R10 + oToolkit_GameStatePtr) + oGameState_OverworldPlayerObjectPtr);

    ST32(player + oOWPlayerObject_NextX, LD32(player + oOWPlayerObject_X));
    ST32(player + oOWPlayerObject_NextY, LD32(player + oOWPlayerObject_Y));
    ST32(player + oOWPlayerObject_NextZ, LD32(player + oOWPlayerObject_Z));

    ST32(player + oOWPlayerObject_X, R0);
    ST32(player + oOWPlayerObject_Y, R1);
    ST32(player + oOWPlayerObject_Z, R2);

    /* Tail call: owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4(r0 = X, r1 = Y, r2 = Z) */
    GUEST_CALL(owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4);
    RETURN_TO_CALLER();
}

/* ── owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4 ── */
/* owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4(r0 = x, r1 = y, r2 = z):
 * Store the player's overworld coordinates into the eStruct200ace0 struct
 * (PlayerX has no named offset macro, the disassembly uses the raw 0x8).
 * r3 is left pointing at that struct. */
Next owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4(void) {
    R3 = A_eStruct200ace0;
    ST32(R3 + 8, R0); /* oS200ace0_PlayerX */
    ST32(R3 + oS200ace0_PlayerY, R1);
    ST32(R3 + oS200ace0_PlayerZ, R2);
    RETURN_TO_CALLER();
}

/* ── ReadOWPlayerObjectCoords ── */
/* ReadOWPlayerObjectCoords: read the overworld player object's position.
 * Inputs:  r10 = toolkit pointer.
 * Outputs: r0 = X, r1 = Y, r2 = Z, r3 = pointer to the OW player object. */
Next ReadOWPlayerObjectCoords(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = LD32(player + oOWPlayerObject_X);
    R1 = LD32(player + oOWPlayerObject_Y);
    R2 = LD32(player + oOWPlayerObject_Z);
    R3 = player;
    RETURN_TO_CALLER();
}

/* ── owPlayer_offsetS200ace0Coords_809e1fa ── */
/* owPlayer_offsetS200ace0Coords_809e1fa(r0 = dx, r1 = dy, r2 = dz):
 * offset the player's overworld position by the given delta. The struct
 * pointer is reloaded from the literal pool into a scratch register; the
 * caller's r7 is saved on entry and restored on exit, so it is preserved.
 * Flags come from the final add (the Z update), which also leaves the new
 * Z in r3. */

Next owPlayer_offsetS200ace0Coords_809e1fa(void) {
    u32 const player = A_eStruct200ace0;

    u32 x = LD32(player + oS200ace0_PlayerX);
    x += R0;
    ST32(player + oS200ace0_PlayerX, x);

    u32 y = LD32(player + oS200ace0_PlayerY);
    y += R1;
    ST32(player + oS200ace0_PlayerY, y);

    u32 oldZ = LD32(player + oS200ace0_PlayerZ);
    SET_FLAGS_ADD(oldZ, R2);
    u32 z = oldZ + R2;
    ST32(player + oS200ace0_PlayerZ, z);

    R3 = z;
    RETURN_TO_CALLER();
}

/* ── returnZero_809E228 ── */
/* returnZero_809E228: always returns 0 (likely a stub/callback). */
Next returnZero_809E228(void) {
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── owPlayer_setInteractionLocked_809e230 ── */
/* owPlayer_setInteractionLocked_809e230:
 * Marks the overworld player object as interaction-locked (e.g. while a
 * cutscene/dialog is running). r10 = toolkit pointer.
 * Outputs: r0 = 1, r3 = overworld player object, NZ set from r0 = 1. */
Next owPlayer_setInteractionLocked_809e230(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    ST8(player + oOWPlayerObject_InteractionLocked, R0);
    R3 = player;

    RETURN_TO_CALLER();
}

/* ── owPlayer_clearInteractionLocked_809e23c ── */
/* owPlayer_clearInteractionLocked_809e23c:
 * Clear the "currently interacting with something" flag on the overworld
 * player object, so the player can start a new interaction afterwards.
 * Inputs:  r10 = toolkit pointer.
 * Outputs: r0 = 0, r3 = overworld player object pointer, nz from r0.
 */
Next owPlayer_clearInteractionLocked_809e23c(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = 0;
    R3 = player;
    SET_FLAGS_NZ(R0);
    ST8(player + oOWPlayerObject_InteractionLocked, 0);

    RETURN_TO_CALLER();
}

/* ── owPlayer_enableWallCollision_809e248 ── */
/* owPlayer_enableWallCollision_809e248:
 * Turn wall collision back on for the overworld player object.
 * R10 is the toolkit pointer; returns R0 = 1 and the player object
 * pointer in R3, with flags set from R0 (the original's Thumb
 * "mov r0, #1" is an LSLS that sets NZ). */
Next owPlayer_enableWallCollision_809e248(void) {
    u32 player = LD32(LD32(R10 + oToolkit_GameStatePtr) + oGameState_OverworldPlayerObjectPtr);

    R0 = 1;
    ST8(player + oOWPlayerObject_wallCollision_0c, 1);
    R3 = player;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── owPlayer_disableWallCollision_809e254 ── */
/* owPlayer_disableWallCollision_809e254: clear the overworld player object's
 * wall-collision flag so the player can walk through walls.
 * outputs: r0 = 0 (FALSE), r3 = player object pointer, nz set from r0. */
Next owPlayer_disableWallCollision_809e254(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = 0; /* FALSE */
    R3 = player;
    SET_FLAGS_NZ(R0);
    ST8(R3 + oOWPlayerObject_wallCollision_0c, R0);

    RETURN_TO_CALLER();
}

/* ── SetOWPlayerFacingDirection ── */
/* SetOWPlayerFacingDirection(r0 = direction):
 * Store the given facing direction into the overworld player object.
 * r10 = toolkit pointer; also returns the player object pointer in r3. */
Next SetOWPlayerFacingDirection(void) {
    u32 *gameState = (u32 *)LD32(R10 + oToolkit_GameStatePtr);
    u32 *player = (u32 *)LD32((u32)gameState + oGameState_OverworldPlayerObjectPtr);
    ST8((u32)player + oOWPlayerObject_FacingDirection, (u8)R0);
    R3 = (u32)player;
    RETURN_TO_CALLER();
}

/* ── GetOWPlayerFacingDirection ── */
/* GetOWPlayerFacingDirection: return the direction the overworld player sprite
 * is currently facing (0..3), read from the player's overworld object. */
Next GetOWPlayerFacingDirection(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 playerObject = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = LD8(playerObject + oOWPlayerObject_FacingDirection);
    RETURN_TO_CALLER();
}

/* ── owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2 ── */

/* owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2:
 * reset the overworld player navi's sprite transform to its defaults
 * (horizontal compress 0x40, vertical compress 0x40, rotation 0) and store
 * them into the eStruct2000aa0 fields. Tail-calls
 * owPlayer_setS2000aa0_param0x0to0x2_809e2c8, which loads the struct pointer
 * into r3 and writes the three parameter bytes. */
Next owPlayer_setS2000aa0_param0x0to0x2_with0x40_0x40_0x0_respectively_809e2c2(void) {
    R0 = DEFAULT_OW_PLAYER_NAVI_HORIZONTAL_COMPRESS;
    R1 = DEFAULT_OW_PLAYER_NAVI_VERTICAL_COMPRESS;
    R2 = DEFAULT_OW_PLAYER_NAVI_ROTATION;
    GUEST_CALL(owPlayer_setS2000aa0_param0x0to0x2_809e2c8);
    RETURN_TO_CALLER();
}

/* ── owPlayer_setS2000aa0_param0x0to0x2_809e2c8 ── */
/* owPlayer_setS2000aa0_param0x0to0x2_809e2c8(r0 = horizontal compress,
 * r1 = vertical compress, r2 = rotation): store the three navi sprite
 * transform settings into the persistent player state struct.
 * r3 = eStruct2000aa0 base address. */
Next owPlayer_setS2000aa0_param0x0to0x2_809e2c8(void) {
    u32 playerState = A_eStruct2000aa0;

    ST8(playerState + 0, R0);
    ST8(playerState + 1, R1);
    ST8(playerState + 2, R2);

    R3 = playerState;
    RETURN_TO_CALLER();
}

/* ── SetOWPlayerNaviColorShader ── */
/* SetOWPlayerNaviColorShader(r0 = shader id):
 * Store the overworld player navi color shader id into the
 * eStruct2000aa0 struct field word_2000AA6 (base + 6).
 * Leaves r3 = eStruct2000aa0 (as the original does for its callers). */
Next SetOWPlayerNaviColorShader(void) {
    R3 = A_eStruct2000aa0;
    ST16(R3 + 6, R0); /* word_2000AA6 - 0x2000aa0 == 6 */
    RETURN_TO_CALLER();
}

/* ── ZeroOWPlayerNaviPaletteIndex ── */
/* ZeroOWPlayerNaviPaletteIndex: 0809E2FC-0809E2FE THUMB, bn6f ow_player.s:2412
 * Zero the overworld player Navi's palette index (the byte at 0x2000AA5).
 * This entry point is tail-merged with SetOWPlayerNaviPaletteIndex, which
 * performs the same store using the index passed in r0; both leave the
 * pointer to the 0x2000AA0 struct in r3 (and r0 = 0 from the `mov r0, #0`). */
Next ZeroOWPlayerNaviPaletteIndex(void) {
    R3 = A_eStruct2000aa0;
    R0 = 0;
    ST8(R3 + 5 /* oS2000aa0_OWPlayerNaviPaletteIndex: byte_2000AA5 - 0x2000aa0 */, 0);
    RETURN_TO_CALLER();
}

/* ── SetOWPlayerNaviPaletteIndex ── */
/* SetOWPlayerNaviPaletteIndex(r0 = navi palette index):
 * stores the overworld player navi's palette index into the eStruct2000aa0
 * struct (field byte_2000AA5), leaving r3 pointing at that struct. */
Next SetOWPlayerNaviPaletteIndex(void) {
    R3 = A_eStruct2000aa0;
    ST8(R3 + 5 /* byte_2000AA5 - 0x2000aa0 */, R0);
    RETURN_TO_CALLER();
}

/* ── owPlayer_zeroS2000AA0Param0x4_809e312 ── */
/* owPlayer_zeroS2000AA0Param0x4_809e312: clear the player sprite's byte at
 * 0x2000AA4 (the S2000aa0 struct's Sprite_04 field).
 *
 * The original is a two-instruction pair:
 *   owPlayer_zeroS2000AA0Param0x4:  mov r0, #0   (sets Z, clears N)
 *   owPlayer_setS2000AA0Param0x4:   ldr r3, =eStruct2000aa0
 *                                   strb r0, [r3, #4]
 * The setter is tiny, so it is inlined here; the observable results are
 * r0 = 0, r3 = &eStruct2000aa0, one byte stored at 0x2000AA4, and the Z
 * flag left set by the `mov r0, #0`. */

/* Field offset from the disassembly: strb r0, [r3, #oS2000aa0_Sprite_04]
 * (byte_2000AA4 - 0x2000aa0). */

Next owPlayer_zeroS2000AA0Param0x4_809e312(void) {
    R0 = 0;
    SET_FLAGS_NZ(R0);               /* mov r0, #0 */

    R3 = A_eStruct2000aa0;
    ST8(A_eStruct2000aa0 + oS2000aa0_Sprite_04, R0);   /* strb r0, [r3, #4] */

    RETURN_TO_CALLER();
}

/* ── owPlayer_setS2000AA0Param0x4_809e314 ── */
/* owPlayer_setS2000AA0Param0x4_809e314(r0 = sprite id):
 * Store the overworld player's current sprite id into byte 4 of the
 * eStruct2000aa0 struct (byte_2000AA4). Leaves the struct's address in r3. */
Next owPlayer_setS2000AA0Param0x4_809e314(void) {
    R3 = A_eStruct2000aa0;
    ST8(R3 + 4, R0);
    RETURN_TO_CALLER();
}

/* ── owMapObject_isHoveringElevator_0809E3A2 ── */
/* owMapObject_isHoveringElevator_0809E3A2: test the flag byte at byte_2000AA8 (eStruct2000aa0 + 8).
 * Outputs: r3 = eStruct2000aa0 base address, Z flag set iff the byte is 0. */
Next owMapObject_isHoveringElevator_0809E3A2(void) {
    R3 = A_eStruct2000aa0;
    R0 = LD8(A_eStruct2000aa0 + 8); /* byte_2000AA8 */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_809E3AA ── */
/* sub_809E3AA:
 * Clear the flag byte at byte_2000AA8 (offset 8 of eStruct2000aa0).
 * Leaves the struct's address in r3. */
Next sub_809E3AA(void) {
    R3 = A_eStruct2000aa0;
    ST8(R3 + 8, 0);
    RETURN_TO_CALLER();
}

/* ── resetStruct2000aa0State_809E3B2 ── */
/* resetStruct2000aa0State_809E3B2: reset the state struct at 0x2000AA0 — clear the byte flag
 * at +0x0B and the halfword counters at +0x0C, +0x10, +0x14 and +0x18. */
Next resetStruct2000aa0State_809E3B2(void) {
    u32 state = A_eStruct2000aa0;

    ST8(state + 0x0B, 0);
    ST16(state + 0x0C, 0);
    ST16(state + 0x10, 0);
    ST16(state + 0x14, 0);
    ST16(state + 0x18, 0);

    RETURN_TO_CALLER();
}

/* ── clampPlayerPosition_809E3D6 ── */
/* Clamp the overworld player object's position against the bounding box held in
 * the eStruct2000aa0 structure. Reads through the toolbox to find the player
 * object, then pushes primary/secondary coordinates towards the limits stored
 * in the structure (max/min pairs at offsets 0xC/0x14 and 0x10/0x18).
 * If the structure's enable byte is clear, this is a no-op. */
static void clamp_player_position(u32 toolbox_ptr) {
    u32 clamps = A_eStruct2000aa0;
    if (LD8(clamps + 0xB) == 0)
        return;

    u32 state = LD32(toolbox_ptr + oToolkit_GameStatePtr);
    u32 player = LD32(state + oGameState_OverworldPlayerObjectPtr);
    /* The assembly loads oGameState_MapGroup and compares it to 0x80, but the
     * result is never used — only the side effect of loading the player ptr
     * matters, so we don't need to repeat that here. */

    s32 primary   = LD32(player + 0x1C);
    s32 secondary = LD32(player + 0x20);

    s32 hi_primary = LD32(clamps + 0xC);
    if (hi_primary > primary) {
        ST32(player + 0x1C, (u32)hi_primary);
        ST32(player + 0x20, LD32(player + 0x2C));
        return;
    }

    s32 lo_primary = LD32(clamps + 0x14);
    if (lo_primary < primary) {
        ST32(player + 0x1C, (u32)lo_primary);
        ST32(player + 0x20, LD32(player + 0x2C));
        return;
    }

    s32 hi_secondary = LD32(clamps + 0x10);
    if (hi_secondary > secondary) {
        ST32(player + 0x20, (u32)hi_secondary);
        ST32(player + 0x1C, LD32(player + 0x28));
        return;
    }

    s32 lo_secondary = LD32(clamps + 0x18);
    if (lo_secondary < secondary) {
        ST32(player + 0x20, (u32)lo_secondary);
        ST32(player + 0x1C, LD32(player + 0x28));
    }
}

Next clampPlayerPosition_809E3D6(void) {
    clamp_player_position(R10);
    RETURN_TO_CALLER();
}

/* ── getOWPlayerSpriteFrameParameters_809E434 ── */
/* getOWPlayerSpriteFrameParameters_809E434:
 * Load the overworld player object from the current game state and return its
 * current sprite frame parameters (r0-r3, set by sprite_getFrameParameters).
 * r10 = toolkit, r5 is preserved. */
Next getOWPlayerSpriteFrameParameters_809E434(void) {
    u32 savedR5 = R5;

    /* toolkit->gameState->overworldPlayerObject */
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    R5 = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    GUEST_CALL(sprite_getFrameParameters);

    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── MakeOWPlayerVisible ── */
/* MakeOWPlayerVisible: set the "visible" bit (0x02) in the overworld player
 * object's flags so its sprite is rendered again.
 *   inputs:  R10 = toolkit pointer
 *   outputs: R0 = updated flags byte, R1 = OBJECT_FLAG_VISIBLE (2),
 *            R3 = player object pointer; nz flags reflect the updated flags.
 */
Next MakeOWPlayerVisible(void) {
    /* Flags byte lives at offset 0 of the player object. */
    u32 player = LD32(LD32(R10 + oToolkit_GameStatePtr)
                      + oGameState_OverworldPlayerObjectPtr);

    R0 = LD8(player);
    R1 = 2;                 /* OBJECT_FLAG_VISIBLE */
    R0 |= R1;
    SET_FLAGS_NZ(R0);
    ST8(player, R0);
    R3 = player;

    RETURN_TO_CALLER();
}

/* ── MakeOWPlayerInvisible ── */
/* MakeOWPlayerInvisible: hide the player's overworld sprite.
 * Follows the game-state pointer chain from the toolkit (r10), then clears
 * the "visible" bit (0x02) of the player object's attribute byte.
 * Outputs: r0 = new flag byte, r1 = 2, r3 = player object pointer;
 * nz flags set from the new flag byte. */
Next MakeOWPlayerInvisible(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    u32 flags = LD8(player);
    R1 = 2; /* OBJECT_FLAG_VISIBLE */
    flags &= ~R1;
    ST8(player, flags);

    R0 = flags;
    R3 = player;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── owPlayer_isBusyOrCutsceneActive_809E462 ── */
/* owPlayer_isBusyOrCutsceneActive_809E462:
 * Reads the flag byte at oGameState_OWPlayerBusyOrCutscene from the current game state.
 * Returns r0 = that byte, r1 = game state pointer, and sets NZ flags on it
 * so callers can branch on whether the flag is zero. */
Next owPlayer_isBusyOrCutsceneActive_809E462(void) {
    u32 *toolkit = (u32 *)R10;
    u32 gameState = LD32((u32)toolkit + oToolkit_GameStatePtr);

    R1 = gameState;
    R0 = LD8(gameState + oGameState_OWPlayerBusyOrCutscene);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── overworld_CheckPlayerAdvanceForwardTrigger_809E46E ── */
/* overworld_CheckPlayerAdvanceForwardTrigger_809E46E: overworld hook that checks the coordinate trigger at
 * player + 0x1C.  If it is one of the "player advances forward" trigger
 * tiles (0x48..0x4F), store the trigger index (0..7) into the player
 * object at + 0x64 and set the EVENT_1717 event flag. */
#define EVENT_1717_PLAYER_ADVANCE_FORWARD 0x1717u

Next overworld_CheckPlayerAdvanceForwardTrigger_809E46E(void) {
    u32 callerR7 = R7; /* r7 is used as scratch below but must be preserved */

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 player = LD32(gameState + oGameState_OverworldPlayerObjectPtr);

    R0 = player + 0x1C;
    R7 = gameState; /* guest state at the original bl */
    GUEST_CALL(checkCoordinateTrigger_8031a7a);
    s32 trigger = (s32)R0;

    if (trigger >= 0x48 && trigger <= 0x4F) {
        ST16(player + 0x64, (u16)(trigger - 0x48));

        /* SetEventFlagFromImmediate takes the event bank in r0 and the
         * flag index in r1 (movflag EVENT_1717_PLAYER_ADVANCE_FORWARD). */
        R0 = EVENT_1717_PLAYER_ADVANCE_FORWARD >> 8;  /* 0x17 */
        R1 = EVENT_1717_PLAYER_ADVANCE_FORWARD & 0xFF; /* 0x17 */
        GUEST_CALL(SetEventFlagFromImmediate);
    }

    R7 = callerR7;
    RETURN_TO_CALLER();
}

#undef EVENT_1717_PLAYER_ADVANCE_FORWARD

/* ── owPlayer_toggleUsingCopybot_809e4bc ── */
/* owPlayer_toggleUsingCopybot_809e4bc (0x0809E4BC):
 * Toggle EVENT_COPYBOT_ACTIVE, then reload the overworld player sprite showing
 * the copybot companion (sprite 0x37) if the flag is now set, or sprite 0 otherwise.
 * Sets the player's animation, loads animation frames, disables shadow, refreshes sprite.
 *
 * inputs:   r10 = toolkit pointer
 * outputs:  r1, r2 (left by sprite_update/sprite_load)
 *           r4, r5, r7 (preserved – restored from stack as original does)
 * clobbers: r0, r3, nz, c, v, lr
 * preserved: r4, r5, r6, r7
 */

#define EVENT_COPYBOT_ENTRY   0x17  /* event-flag byte index for COPYBOT_ACTIVE */
#define EVENT_COPYBOT_BIT     0x29  /* event-flag bit index within that byte    */
#define OW_SPRITE_SLOT        0x80  /* overworld sprite slot for the player     */
#define OW_SPRITE_INDEX       0x18  /* sprite entry index within that slot      */
#define SPRITE_ID_COPYBOT     0x37  /* copybot companion sprite ID              */

Next owPlayer_toggleUsingCopybot_809e4bc(void) {
    /* Save callee-saved registers (original: push {r4-r7,lr}) */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* Toggle the "copybot active" event flag */
    R0 = EVENT_COPYBOT_ENTRY;
    R1 = EVENT_COPYBOT_BIT;
    GUEST_CALL(ToggleEventFlagFromImmediate);

    /* Toolkit → GameState → OverworldPlayerObject (original: r5 = [[r10]] chain) */
    u32 playerObj = LD32(LD32(R10 + oToolkit_GameStatePtr)
                         + oGameState_OverworldPlayerObjectPtr);

    /* Determine sprite ID: 0x37 if flag is now set, 0 otherwise.
     * (original: mov r4,#0x37; bl Test; bne keep; mov r4,#0) */
    R0 = EVENT_COPYBOT_ENTRY;
    R1 = EVENT_COPYBOT_BIT;
    GUEST_CALL(TestEventFlagFromImmediate);
    u32 spriteId = ZERO_FLAG() ? 0 : SPRITE_ID_COPYBOT;

    /* Load the player's overworld sprite slot with the chosen companion sprite.
     * Original passes spriteId in R2 (via r4) and has R5 = playerObj; sprite_load
     * reads these from the register file. */
    R0 = OW_SPRITE_SLOT;
    R1 = OW_SPRITE_INDEX;
    R2 = spriteId;
    R4 = spriteId;
    R5 = playerObj;
    R6 = savedR6;
    R7 = savedR7;
    GUEST_CALL(sprite_load);

    /* Apply the player's current animation selection */
    R0 = LD8(playerObj + oOWPlayerObject_AnimationSelect);
    GUEST_CALL(sprite_setAnimation);

    /* Build animation frames, suppress shadow, push updated sprite to display */
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    GUEST_CALL(sprite_update);

    /* Restore callee-saved registers (original: pop {r4-r7,pc}) */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;

    RETURN_TO_CALLER();
}

/* ── sub_809E4F8 ── */
/* sub_809E4F8(r10 = Toolkit*):
 * Draw the overworld player. Loads the sprite (tile 0x80, attr 0x18),
 * selects the animation stored at byte 0x14 of the player object,
 * fetches tile/palette data, disables the shadow, then blits.
 *
 * R5 is used as a scratch pointer to the player object across the
 * subroutine calls; it is saved and restored to satisfy the preserved
 * contract (the original achieves this via push {r4-r7}/pop). */
Next sub_809E4F8(void) {
    u32 savedR5 = R5;

    /* Walk Toolkit → GameState → Overworld player object. */
    R5 = R10;
    R5 = LD32(R5 + oToolkit_GameStatePtr);
    R5 = LD32(R5 + oGameState_OverworldPlayerObjectPtr);

    /* Load the player sprite: tile index 0x80, attribute 0x18, no flags. */
    R0 = 0x80;
    R1 = 0x18;
    R2 = 0;
    GUEST_CALL(sprite_load);

    /* Apply the animation frame stored in the player object. */
    R0 = LD8(R5 + 0x14);
    GUEST_CALL(sprite_setAnimation);

    /* Fetch tile/palette data, suppress shadow, render. */
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    GUEST_CALL(sprite_update);

    R5 = savedR5;
    RETURN_TO_CALLER();
}
