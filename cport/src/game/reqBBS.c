/*
 * reqBBS.c: readable functions of bn6f reqBBS.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── reqBBS_addBBSMessage_813e5dc ── */
/* BBS (bulletin-board) message bookkeeping: event-flag ids and RAM tables
 * used by reqBBS_addBBSMessage. */
#define BBS_FLAG_FIRST      0x17A0         /* event-flag id of BBS message 0 */
#define BBS_FLAG_SECOND     0x19A0         /* second flag range also marked when adding */
#define BBS_FLAG_VALUE      2              /* value both flags are set to */
#define BBS_BOARD_CAPACITY  64             /* message numbers logged per board */
#define BBS_MESSAGE_LOG     A_byte_2001400 /* per board: 64 logged message numbers */
#define BBS_WRITE_INDEX     A_unk_2000FC0  /* per board: u32 index of the next free log slot */

/* The registers the original leaves behind for its caller. */
typedef struct {
    u32 writeIndex; /* r0: the board's write index after the append */
    u32 indexTable; /* r1: base of the per-board write indexes */
    u32 messageLog; /* r2: this board's 64 entries in the message log */
} BbsAddResult;

/* register form: calls SetEventFlag outside this file, which has no typed C interface yet */
static void set_event_flag(u32 flag, u32 value) {
    /* register form: the original pushes its caller's r3/r7 and pops them at
     * the end, so both must survive this guest call; SetEventFlag may clobber
     * the caller-saved r3, which plain typed C cannot express. */
    u32 keepR3 = R3;
    u32 keepR7 = R7;
    R0 = flag;
    R1 = value;
    GUEST_CALL(SetEventFlag);
    R3 = keepR3;
    R7 = keepR7;
}

/* Add the message identified by event flag `flag`
 * (BBS_FLAG_FIRST + board * 64 + number on that board) to its board:
 * mark it in both flag ranges, then append its number to the board's log
 * and bump the board's write index. */
static BbsAddResult add_bbs_message(u32 flag) {
    u32 message = flag - BBS_FLAG_FIRST;   /* message number over all boards */

    set_event_flag(flag, BBS_FLAG_VALUE);
    set_event_flag(BBS_FLAG_SECOND + message, BBS_FLAG_VALUE);

    u32 board = message / BBS_BOARD_CAPACITY;
    u32 number = message % BBS_BOARD_CAPACITY;

    u32 boardLog = BBS_MESSAGE_LOG + board * BBS_BOARD_CAPACITY; /* this board's log */
    u32 boardIndex = BBS_WRITE_INDEX + board * 4;                /* its u32 write index */

    u32 nextSlot = LD32(boardIndex);
    ST8(boardLog + nextSlot, number);   /* log the message number */
    ST32(boardIndex, nextSlot + 1);     /* one log slot used up */

    BbsAddResult result;
    result.writeIndex = nextSlot + 1;
    result.indexTable = BBS_WRITE_INDEX;
    result.messageLog = boardLog;
    return result;
}

/* reqBBS_addBBSMessage_813e5dc(r0 = event flag of the message) -> r0 r1 r2.
 * r3 and r7 come back unchanged (the original pushed/popped them; the helper
 * shields them around the guest calls) and lr stays as the last SetEventFlag
 * call left it, exactly as in the original. */
Next reqBBS_addBBSMessage_813e5dc(void) {
    BbsAddResult result = add_bbs_message(R0);
    R0 = result.writeIndex;
    R1 = result.indexTable;
    R2 = result.messageLog;
    RETURN_TO_CALLER();
}

/* ── reqBBS_813E616 ── */
/* reqBBS_813E616:
 * Reset the BBS (bulletin board) work areas: fill the board buffer at
 * 0x2001400 with the unused-entry marker 0x40 and clear the two 0x20-byte
 * entry tables at 0x2000FC0 and 0x2000FF0. */
Next reqBBS_813E616(void) {
    R0 = A_byte_2001400;
    R1 = 0x200;
    R2 = 0x40;
    GUEST_CALL(ByteFill);

    R0 = A_unk_2000FC0;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);

    R0 = A_unk_2000FF0;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── OpenReqBBSMenu813F474 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlagFromImmediate_result_OpenReqBBSMenu813F474;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_OpenReqBBSMenu813F474 call_TestEventFlagFromImmediate_OpenReqBBSMenu813F474(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_OpenReqBBSMenu813F474){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result call_TestEventFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline void call_SetEventFlag_OpenReqBBSMenu813F474(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
}

/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline void call_ClearEventFlagFromImmediate(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

typedef struct { u32 r5; } call_reqBBS_813FDA8_result;
/* register form: generated wrapper: reqBBS_813FDA8 has no typed C interface yet */
static inline call_reqBBS_813FDA8_result call_reqBBS_813FDA8(void) {
    GUEST_CALL(reqBBS_813FDA8);
    return (call_reqBBS_813FDA8_result){ R5 };
}

typedef struct { u32 r5; } call_reqBBS_813FE54_result;
/* register form: generated wrapper: reqBBS_813FE54 has no typed C interface yet */
static inline call_reqBBS_813FE54_result call_reqBBS_813FE54(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_813FE54);
    return (call_reqBBS_813FE54_result){ R5 };
}

typedef struct { u32 r2, r4, r5, r6, r7; } call_reqBBS_813FEB0_result;
/* register form: generated wrapper: reqBBS_813FEB0 has no typed C interface yet */
static inline call_reqBBS_813FEB0_result call_reqBBS_813FEB0(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_813FEB0);
    return (call_reqBBS_813FEB0_result){ R2, R4, R5, R6, R7 };
}

typedef struct { u32 r5; } call_reqBBS_copyTextDataToRAM_result;
/* register form: generated wrapper: reqBBS_copyTextDataToRAM has no typed C interface yet */
static inline call_reqBBS_copyTextDataToRAM_result call_reqBBS_copyTextDataToRAM(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_copyTextDataToRAM);
    return (call_reqBBS_copyTextDataToRAM_result){ R5 };
}

typedef struct { u32 r5, r8, r9, r10, r11, r12; } call_reqBBS_renderRequestNames_result;
/* register form: generated wrapper: reqBBS_renderRequestNames has no typed C interface yet */
static inline call_reqBBS_renderRequestNames_result call_reqBBS_renderRequestNames(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(reqBBS_renderRequestNames);
    return (call_reqBBS_renderRequestNames_result){ R5, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r5, r10, r11; } call_reqBBS_81405C0_result;
/* register form: generated wrapper: reqBBS_81405C0 has no typed C interface yet */
static inline call_reqBBS_81405C0_result call_reqBBS_81405C0(void) {
    GUEST_CALL(reqBBS_81405C0);
    return (call_reqBBS_81405C0_result){ R5, R10, R11 };
}

/* register form: generated wrapper: renderInfo_8001788 has no typed C interface yet */
static inline void call_renderInfo_8001788(void) {
    GUEST_CALL(renderInfo_8001788);
}

/* register form: generated wrapper: renderInfo_80017A0 has no typed C interface yet */
static inline void call_renderInfo_80017A0(void) {
    GUEST_CALL(renderInfo_80017A0);
}

typedef struct { u32 r2; } call_toolkit_applyOverlayPalette_80015FC_result;
/* register form: generated wrapper: toolkit_applyOverlayPalette_80015FC has no typed C interface yet */
static inline call_toolkit_applyOverlayPalette_80015FC_result call_toolkit_applyOverlayPalette_80015FC(u32 r0) {
    R0 = r0;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);
    return (call_toolkit_applyOverlayPalette_80015FC_result){ R2 };
}

typedef struct { u32 r1, r2, r3, r5; } call_SetScreenFade_result;
/* register form: generated wrapper: SetScreenFade has no typed C interface yet */
static inline call_SetScreenFade_result call_SetScreenFade(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetScreenFade);
    return (call_SetScreenFade_result){ R1, R2, R3, R5 };
}

typedef struct { u32 r2, r4, r5, r6, r7; } call_battle_spawnEStructRecordsFromTable_80465A0_result;
/* register form: generated wrapper: battle_spawnEStructRecordsFromTable_80465A0 has no typed C interface yet */
static inline call_battle_spawnEStructRecordsFromTable_80465A0_result call_battle_spawnEStructRecordsFromTable_80465A0(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);
    return (call_battle_spawnEStructRecordsFromTable_80465A0_result){ R2, R4, R5, R6, R7 };
}

typedef struct { u32 r5, r10, r11; } call_reqBBS_renderSelectedEntry_HeaderText_result;
/* register form: generated wrapper: reqBBS_renderSelectedEntry_HeaderText has no typed C interface yet */
static inline call_reqBBS_renderSelectedEntry_HeaderText_result call_reqBBS_renderSelectedEntry_HeaderText(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_renderSelectedEntry_HeaderText);
    return (call_reqBBS_renderSelectedEntry_HeaderText_result){ R5, R10, R11 };
}

typedef struct {
    u32 r10;
    u32 r11;
} req_bbs_menu_result;

/* Open the Request BBS menu screen.
 * If the intro event flag (0x173A) is set and the current request is new,
 * mark it seen, clear the intro flag, and reset its slot.  Either way,
 * clear the reqBBS state fields at r5, run the screen bring-up chain,
 * apply overlays/fade, spawn entry graphics, and enter either the header
 * view (unread entry selected) or the loading view.
 * Returns final r10 and r11. */
static req_bbs_menu_result open_req_bbs_menu(u32 r5, u32 r10) {
    /* One-time intro-event cleanup */
    if (!call_TestEventFlagFromImmediate_OpenReqBBSMenu813F474(0x17, 0x3A).z) {
        u32 slot = LD8(A_eReqBBSGui + 0xF);
        u32 new_flag = slot + LD32(A_reqBBS_textualPointers + (A_dword_813F364 - 0x813F34C));

        if (!call_TestEventFlag(new_flag).z) {
            u32 seen_flag = slot + LD32(A_reqBBS_textualPointers + (A_byte_813F354 + 0xC - 0x813F34C));
            call_SetEventFlag_OpenReqBBSMenu813F474(seen_flag);
            call_ClearEventFlagFromImmediate(0x17, 0x3A);
            ST8(A_eReqBBSGui + 0xF, 0);
        }
    }

    /* Clear reqBBS state fields */
    ST8(r5 + 8, 0);
    ST16(r5 + 0x20, 0);
    ST16(r5 + 0x24, 0);
    ST16(r5 + 0x26, 0);
    ST8(r5 + 9, 0);
    ST8(r5 + 0xB, 0);

    /* Screen bring-up chain */
    r5 = call_reqBBS_813FDA8().r5;
    r5 = call_reqBBS_813FE54(r5).r5;
    r5 = call_reqBBS_813FEB0(r5).r5;
    r5 = call_reqBBS_copyTextDataToRAM(r5).r5;

    call_reqBBS_renderRequestNames_result names = call_reqBBS_renderRequestNames(LD16(r5 + 0x24), r5);
    r5 = names.r5;
    u32 r11 = names.r11;
    r10 = names.r10;

    call_reqBBS_81405C0_result setup = call_reqBBS_81405C0();
    r5 = setup.r5;
    r10 = setup.r10;
    r11 = setup.r11;

    /* Info overlays and screen fade */
    call_renderInfo_8001788();
    call_renderInfo_80017A0();
    call_toolkit_applyOverlayPalette_80015FC(0x17);
    call_SetScreenFade(8, 0x10);

    /* Spawn entry graphics records */
    call_battle_spawnEStructRecordsFromTable_80465A0(A_reqBBS_entriesGfx);

    /* Choose initial view: header for an unread selected entry, else loading */
    if (LD16(r5 + 0x1E) != 0 && LD8(r5 + 5) == 0) {
        ST8(r5, 4);
        ST8(r5 + 8, 5);
        call_reqBBS_renderSelectedEntry_HeaderText_result hdr = call_reqBBS_renderSelectedEntry_HeaderText(r5);
        r10 = hdr.r10;
        r11 = hdr.r11;
        ST16(LD32(r10 + 8), 0x5E40);
    } else {
        ST8(r5, 0x28);
        ST8(r5 + 8, 5);
        ST16(LD32(r10 + 8), 0x5F40);
    }

    req_bbs_menu_result result;
    result.r10 = r10;
    result.r11 = r11;
    return result;
}

Next OpenReqBBSMenu813F474(void) {
    req_bbs_menu_result out = open_req_bbs_menu(R5, R10);
    R10 = out.r10;
    R11 = out.r11;
    RETURN_TO_CALLER();
}

/* ── ExitReqBBSMenu813F590 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_reqBBS_813F8F0_result;
/* register form: generated wrapper: reqBBS_813F8F0 has no typed C interface yet */
static inline call_reqBBS_813F8F0_result call_reqBBS_813F8F0(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_813F8F0);
    return (call_reqBBS_813F8F0_result){ R2, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r1, r3; bool z, n, c, v; } call_IsScreenFadeActive_result;
/* register form: generated wrapper: IsScreenFadeActive has no typed C interface yet */
static inline call_IsScreenFadeActive_result call_IsScreenFadeActive(void) {
    GUEST_CALL(IsScreenFadeActive);
    return (call_IsScreenFadeActive_result){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r3; } call_owPlayer_unlockPlayerAfterNonNPCDialogue_809E122_result;
/* register form: generated wrapper: owPlayer_unlockPlayerAfterNonNPCDialogue_809E122 has no typed C interface yet */
static inline call_owPlayer_unlockPlayerAfterNonNPCDialogue_809E122_result call_owPlayer_unlockPlayerAfterNonNPCDialogue_809E122(void) {
    GUEST_CALL(owPlayer_unlockPlayerAfterNonNPCDialogue_809E122);
    return (call_owPlayer_unlockPlayerAfterNonNPCDialogue_809E122_result){ R3 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7; bool c, v; } call_resetGameStateToOpening_8005F84_result;
/* register form: generated wrapper: resetGameStateToOpening_8005F84 has no typed C interface yet */
static inline call_resetGameStateToOpening_8005F84_result call_resetGameStateToOpening_8005F84(void) {
    GUEST_CALL(resetGameStateToOpening_8005F84);
    return (call_resetGameStateToOpening_8005F84_result){ R1, R2, R3, R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* ExitReqBBSMenu813F590: exit the BBS request screen after the menu closes.
 * Sets the render-info timer, drives the BBS request state machine, then
 * either defers (fade still in progress) or tears down the overworld state
 * and resets to the opening. Returns modified r10/r11 from reqBBS_813F8F0. */

typedef struct {
    u32 r10;
    u32 r11;
} ExitReqBBSOutput;

static ExitReqBBSOutput exit_req_bbs_menu(u32 r5, u32 r10) {
    /* Set the render-info fade timer to 0x1F40 (5000 ticks). */
    u32 render_info = LD32(r10 + oToolkit_RenderInfoPtr);
    ST16(render_info, 0x1F40);

    /* Drive the BBS request state machine; it may modify r10 and r11. */
    call_reqBBS_813F8F0_result reqBBS_res = call_reqBBS_813F8F0(r5);

    ExitReqBBSOutput out;
    out.r10 = reqBBS_res.r10;
    out.r11 = reqBBS_res.r11;

    /* If the screen fade is still active (Z set), the menu cannot close yet. */
    call_IsScreenFadeActive_result fade = call_IsScreenFadeActive();
    if (fade.z)
        return out;

    /* Fade complete – write the dialog-response byte into the NPC's field. */
    u8 dialog_byte = LD8(r5 + 5);
    u32 npc_base = LD32(r5 + 0x28);
    ST32(LD32(npc_base + 0x20), dialog_byte);

    call_owPlayer_unlockPlayerAfterNonNPCDialogue_809E122();

    /* Clear/initialize the player-control block at toolkit+0x1C.
     * Fields at +1,+3,+5,+7 = 0xFF (active), others = 0 (inactive). */
    u32 ctrl = LD32(out.r10 + 0x1C);
    ST8(ctrl + 9, 0);
    ST8(ctrl + 0xA, 0);
    ST8(ctrl + 1, 0xFF);
    ST8(ctrl + 5, 0xFF);
    ST8(ctrl + 3, 0xFF);
    ST8(ctrl + 7, 0xFF);
    ST8(ctrl + 0, 0);
    ST8(ctrl + 4, 0);
    ST8(ctrl + 2, 0);
    ST8(ctrl + 6, 0);

    /* Clear the overworld-state block at toolkit+0x20. */
    u32 ow_state = LD32(out.r10 + 0x20);
    ST8(ow_state, 0);
    ST8(ow_state + 4, 0);
    ST8(ow_state + 6, 0);

    /* Reset the global game state back to the opening screen. */
    call_resetGameStateToOpening_8005F84();

    return out;
}

/* ExitReqBBSMenu813F590(r5, r10) -> r10, r11 */
Next ExitReqBBSMenu813F590(void) {
    ExitReqBBSOutput out = exit_req_bbs_menu(R5, R10);
    R10 = out.r10;
    R11 = out.r11;
    RETURN_TO_CALLER();
}

/* ── reqBBS_813F6F8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r4, r5, r6, r7; bool v; } call_reqBBS_813FEB0_result_reqBBS_813F6F8;
/* register form: generated wrapper: reqBBS_813FEB0 has no typed C interface yet */
static inline call_reqBBS_813FEB0_result_reqBBS_813F6F8 call_reqBBS_813FEB0_reqBBS_813F6F8(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_813FEB0);
    return (call_reqBBS_813FEB0_result_reqBBS_813F6F8){ R2, R3, R4, R5, R6, R7, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool c, v; } call_reqBBS_813F8F0_result_reqBBS_813F6F8;
/* register form: generated wrapper: reqBBS_813F8F0 has no typed C interface yet */
static inline call_reqBBS_813F8F0_result_reqBBS_813F6F8 call_reqBBS_813F8F0_reqBBS_813F6F8(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_813F8F0);
    return (call_reqBBS_813F8F0_result_reqBBS_813F6F8){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* reqBBS_813F6F8
 *
 * Called from the BBS request handler. Performs a small animation/state update
 * on the toolkit's two scratch objects (offsets 0x9740 and 0xF3A0) and, when
 * the per-request counter at r5+8 drops to zero or below, marks the request
 * as finished (writes 4 at r5+0) and runs the BBS finishing sequence.
 *
 * Inputs:  r5 = request state pointer, r10 = toolkit pointer (and the LR for the call)
 * Outputs: r10 = toolkit pointer (unchanged), r11 unchanged
 */
static void reqBBS_813F6F8_impl(u32 r5, u32 r10) {
    u32 ptr9740 = LD32(r10 + oToolkit_Unk2009740_Ptr);
    u32 obj9740_6 = (u32)LD8(ptr9740 + 6) - 8;
    ST8(ptr9740 + 6, (u8)obj9740_6);
    ST8(ptr9740 + 4, (u8)(obj9740_6 >> 4));

    u32 ptrF3A0 = LD32(r10 + oToolkit_Unk200f3a0_Ptr);
    ST8(ptrF3A0 + 9, 0x1B);

    ST8(ptrF3A0 + 7, (u8)((u32)LD8(ptrF3A0 + 7) + 8));
    ST8(ptrF3A0 + 6, (u8)((u32)LD8(ptrF3A0 + 6) - 8));

    s32 count = (s32)LD8(r5 + 8) - 1;
    ST8(r5 + 8, (u8)count);
    if (count > 0) {
        call_reqBBS_813F8F0_reqBBS_813F6F8(r5);
        return;
    }

    ST8(r5, 4);
    call_reqBBS_813FEB0_reqBBS_813F6F8(r5);

    ptrF3A0 = LD32(r10 + oToolkit_Unk200f3a0_Ptr);
    ST8(ptrF3A0 + 9, 0x3F);
    ST8(ptrF3A0 + 10, 0x3F);
    ST8(ptrF3A0 + 3, 0xFF);
    ST8(ptrF3A0 + 2, 0);
    ST8(ptrF3A0 + 7, 0xFF);
    ST8(ptrF3A0 + 6, 0);

    call_reqBBS_813F8F0_reqBBS_813F6F8(r5);
}

Next reqBBS_813F6F8(void) {
    reqBBS_813F6F8_impl(R5, R10);
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── reqBBS_813F754 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n, v; } call_IsScreenFadeActive_result_reqBBS_813F754;
/* register form: generated wrapper: IsScreenFadeActive has no typed C interface yet */
static inline call_IsScreenFadeActive_result_reqBBS_813F754 call_IsScreenFadeActive_reqBBS_813F754(void) {
    GUEST_CALL(IsScreenFadeActive);
    return (call_IsScreenFadeActive_result_reqBBS_813F754){ R3, ZERO_FLAG(), NEGATIVE_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool c, v; } call_reqBBS_813F8F0_result_reqBBS_813F754;
/* register form: generated wrapper: reqBBS_813F8F0 has no typed C interface yet */
static inline call_reqBBS_813F8F0_result_reqBBS_813F754 call_reqBBS_813F8F0_reqBBS_813F754(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_813F8F0);
    return (call_reqBBS_813F8F0_result_reqBBS_813F754){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Configure the BBS fade/transition parameters stored in the toolkit block
 * pointed to by offset oToolkit_Unk200f3a0_Ptr. */
static void init_fade_block(u32 toolkit) {
    u32 fade_block = LD32(toolkit + oToolkit_Unk200f3a0_Ptr);
    ST8(fade_block + 9,  0x1A);
    ST8(fade_block + 10, 0x36);
    ST8(fade_block + 3,  0);
    ST8(fade_block + 2,  0xFF);
    ST8(fade_block + 7,  0x50);
    ST8(fade_block + 6,  0x50);
}

struct bbs_req_out { u32 r10, r11; };

/* Drive the BBS screen transition: set up fade parameters, force an extra
 * fast-forward command through the 0x9740 control block when no screen fade
 * is already running, then dispatch the actual request. */
static struct bbs_req_out reqBBS_813F754_impl(u32 r5, u32 r10) {
    init_fade_block(r10);

    if (!call_IsScreenFadeActive_reqBBS_813F754().z) {
        /* No fade active – send the fast-forward byte, clear timers,
         * stamp the caller's request buffer, and re-init the fade block. */
        u32 ctrl = LD32(r10 + oToolkit_Unk2009740_Ptr);
        ST8(ctrl,     0xF7);
        ST8(ctrl + 4, 0);
        ST8(ctrl + 6, 0);
        ST8(r5,       0x0C);
        init_fade_block(r10);
    }

    call_reqBBS_813F8F0_result_reqBBS_813F754 res = call_reqBBS_813F8F0_reqBBS_813F754(r5);
    return (struct bbs_req_out){ r10, res.r11 };
}

/* reqBBS_813F754(r5 = caller buffer, r10 = toolkit) -> r10 r11 */
Next reqBBS_813F754(void) {
    struct bbs_req_out out = reqBBS_813F754_impl(R5, R10);
    R10 = out.r10;
    R11 = out.r11;
    RETURN_TO_CALLER();
}

/* ── reqBBS_813F7B0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n, v; } call_IsScreenFadeActive_result_reqBBS_813F7B0;
/* register form: generated wrapper: IsScreenFadeActive has no typed C interface yet */
static inline call_IsScreenFadeActive_result_reqBBS_813F7B0 call_IsScreenFadeActive_reqBBS_813F7B0(void) {
    GUEST_CALL(IsScreenFadeActive);
    return (call_IsScreenFadeActive_result_reqBBS_813F7B0){ R3, ZERO_FLAG(), NEGATIVE_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool c, v; } call_reqBBS_813F8F0_result_reqBBS_813F7B0;
/* register form: generated wrapper: reqBBS_813F8F0 has no typed C interface yet */
static inline call_reqBBS_813F8F0_result_reqBBS_813F7B0 call_reqBBS_813F8F0_reqBBS_813F7B0(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_813F8F0);
    return (call_reqBBS_813F8F0_result_reqBBS_813F7B0){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Write the fixed byte sequence into the f3a0 toolkit buffer. */
static void set_f3a0_bytes(u32 buf) {
    ST8(buf + 9,   0x1A);
    ST8(buf + 0xA, 0x36);
    ST8(buf + 3,   0);
    ST8(buf + 2,   0xFF);
    ST8(buf + 7,   0x50);
    ST8(buf + 6,   0x50);
}

/* Prepare the BBS request: configure the f3a0 buffer, check whether a screen fade
 * is active, optionally write the pending-request marker (0xF7, 0x1C), then
 * dispatch through reqBBS_813F8F0. */
static void bbs_request_setup(u32 toolkit, u32 slot) {
    u32 f3a0 = LD32(toolkit + oToolkit_Unk200f3a0_Ptr);
    set_f3a0_bytes(f3a0);

    /* IsScreenFadeActive: Z set → fade active, skip the pending mark. */
    call_IsScreenFadeActive_result_reqBBS_813F7B0 fade = call_IsScreenFadeActive_reqBBS_813F7B0();
    if (!fade.z) {
        u32 bbs = LD32(toolkit + oToolkit_Unk2009740_Ptr);
        ST8(bbs,     0xF7);
        ST8(bbs + 4, 0);
        ST8(bbs + 6, 0);
        ST8(slot,    0x1C);

        /* Re-initialise the f3a0 buffer after marking the request. */
        f3a0 = LD32(toolkit + oToolkit_Unk200f3a0_Ptr);
        set_f3a0_bytes(f3a0);
    }

    call_reqBBS_813F8F0_reqBBS_813F7B0(slot);
}

/* reqBBS_813F7B0(r5 = slot pointer, r10 = toolkit) -> r0=0, nz; r10/r11 from callee */
Next reqBBS_813F7B0(void) {
    bbs_request_setup(R10, R5);
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── reqBBS_813F868 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; bool z, n; } call_chatbox_mask_eFlags2009F38_result;
/* register form: generated wrapper: chatbox_mask_eFlags2009F38 has no typed C interface yet */
static inline call_chatbox_mask_eFlags2009F38_result call_chatbox_mask_eFlags2009F38(u32 r0) {
    R0 = r0;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return (call_chatbox_mask_eFlags2009F38_result){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r5; } call_chatbox_8040818_result;
/* register form: generated wrapper: chatbox_8040818 has no typed C interface yet */
static inline call_chatbox_8040818_result call_chatbox_8040818(void) {
    GUEST_CALL(chatbox_8040818);
    return (call_chatbox_8040818_result){ R2, R5 };
}

typedef struct { u32 r2, r3, r5; bool v; } call_SetScreenFade_result_reqBBS_813F868;
/* register form: generated wrapper: SetScreenFade has no typed C interface yet */
static inline call_SetScreenFade_result_reqBBS_813F868 call_SetScreenFade_reqBBS_813F868(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetScreenFade);
    return (call_SetScreenFade_result_reqBBS_813F868){ R2, R3, R5, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool c, v; } call_reqBBS_813F8F0_result_reqBBS_813F868;
/* register form: generated wrapper: reqBBS_813F8F0 has no typed C interface yet */
static inline call_reqBBS_813F8F0_result_reqBBS_813F868 call_reqBBS_813F8F0_reqBBS_813F868(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_813F8F0);
    return (call_reqBBS_813F8F0_result_reqBBS_813F868){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Some BBS / chip / map system message: bump a counter, then ask chatbox whether to start
 * the chatbox/screen-fade transition, doing it if needed, then drive reqBBS_813F8F0 and
 * return 0. Preserves r10/r11 to its caller (r10 is the toolkit, r5 holds r10 on return).
 */
static void reqBBS_813F868_typed(u32 r5_host, u32 toolkit_ptr) {
    /* The chip-page counter sits at oToolkit_Unk2009740_Ptr + 6; if it's still under 0x40
     * advance it, write the page index to the field at +4, and prime the music state
     * pointer's +9 with 0x1B. */
    u32 unk2009740 = LD32(toolkit_ptr + oToolkit_Unk2009740_Ptr);
    u32 page_counter = LD8(unk2009740 + 6);
    if ((s32)page_counter < 0x40) {
        page_counter += 8;
        ST8(unk2009740 + 6, (u8)page_counter);
        ST8(unk2009740 + 4, (u8)(page_counter >> 4));
        u32 unk200f3a0 = LD32(toolkit_ptr + oToolkit_Unk200f3a0_Ptr);
        ST8(unk200f3a0 + 9, 0x1B);
    }

    /* chatbox_mask_eFlags2009F38: returns nz that the original tests as r0 != 8. When the
     * chatbox is allowed, run its transition: mark r5 with 0x24, set the music byte to 0x1F,
     * store 5 at r5+8, and call SetScreenFade(0x10, 8). */
    call_chatbox_mask_eFlags2009F38_result mask_r = call_chatbox_mask_eFlags2009F38(8);
    if (!mask_r.z) {
        (void)call_chatbox_8040818();
        ST8(r5_host + 0, 0x24);
        u32 unk200f3a0 = LD32(toolkit_ptr + oToolkit_Unk200f3a0_Ptr);
        ST8(unk200f3a0 + 9, 0x1F);
        ST8(r5_host + 8, 5);
        (void)call_SetScreenFade_reqBBS_813F868(0x10, 8);
    }

    (void)call_reqBBS_813F8F0_reqBBS_813F868(r5_host);
}

Next reqBBS_813F868(void) {
    u32 r5 = R5;
    u32 r10 = R10;
    reqBBS_813F868_typed(r5, r10);
    R10 = r10;
    RETURN_TO_CALLER();
}

/* ── reqBBS_initMemory_813F9DA ── */
/* reqBBS_initMemory_813F9DA:
 * Reset the BBS (bulletin board) request state:
 *   - fill the request entry list (0x80 bytes) with 0x80 (free-slot marker)
 *   - clear the "requests sent" counter and one extra word
 * Leaves r2 = 0x80 (the fill value survives both clear calls). */
Next reqBBS_initMemory_813F9DA(void) {
    R0 = A_reqBBS_requestEntriesList;
    R1 = 0x80;
    R2 = 0x80;
    GUEST_CALL(ByteFill);

    R0 = A_reqBBS_numRequestsSent;
    R1 = 4;
    GUEST_CALL(ZeroFillByWord);

    R0 = A_unk_2000770;
    R1 = 4;
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── reqBBS_813FDA8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3; } call_zeroFillVRAM_result;
/* register form: generated wrapper: zeroFillVRAM has no typed C interface yet */
static inline call_zeroFillVRAM_result call_zeroFillVRAM(void) {
    GUEST_CALL(zeroFillVRAM);
    return (call_zeroFillVRAM_result){ R2, R3 };
}

/* register form: generated wrapper: ZeroFill_byte_3001960 has no typed C interface yet */
static inline void call_ZeroFill_byte_3001960(void) {
    GUEST_CALL(ZeroFill_byte_3001960);
}

typedef struct { u32 r2, r3, r4, r5, r6, r7; } call_decompAndCopyData_result;
/* register form: generated wrapper: decompAndCopyData has no typed C interface yet */
static inline call_decompAndCopyData_result call_decompAndCopyData(u32 r0) {
    R0 = r0;
    GUEST_CALL(decompAndCopyData);
    return (call_decompAndCopyData_result){ R2, R3, R4, R5, R6, R7 };
}

typedef struct { u32 r4, r5, r6; } call_reqBBS_8140600_result;
/* register form: generated wrapper: reqBBS_8140600 has no typed C interface yet */
static inline call_reqBBS_8140600_result call_reqBBS_8140600(void) {
    GUEST_CALL(reqBBS_8140600);
    return (call_reqBBS_8140600_result){ R4, R5, R6 };
}

typedef struct { u32 r2, r3; } call_ZeroFillGFX30025c0_result;
/* register form: generated wrapper: ZeroFillGFX30025c0 has no typed C interface yet */
static inline call_ZeroFillGFX30025c0_result call_ZeroFillGFX30025c0(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
    return (call_ZeroFillGFX30025c0_result){ R2, R3 };
}

typedef struct { u32 r3, r4, r6, r7; } call_sub_8046664_result;
/* register form: generated wrapper: battle_resetEStructRecords_8046664 has no typed C interface yet */
static inline call_sub_8046664_result call_sub_8046664(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
    return (call_sub_8046664_result){ R3, R4, R6, R7 };
}

/* reqBBS_813FDA8 — BBS request-handler init (0813FDA8).
 *
 * Runs the standard loading sequence for this scene:
 *   1. clear VRAM and the palette buffer at 0x3001960,
 *   2. decompress/copy the resource reference table at byte_813FDCC
 *      (tiles, palettes, text script, ...),
 *   3. reqBBS_8140600 sets up the scene state,
 *   4. clear the GFX area at 0x30025c0 and run battle_resetEStructRecords_8046664.
 *
 * The original pushes r5 around the calls and pops it back, so r5 comes out
 * exactly as it went in. r10 is untouched throughout.
 */
static u32 reqBBS_813FDA8_init(u32 r5_in) {
    call_zeroFillVRAM();
    call_ZeroFill_byte_3001960();

    call_decompAndCopyData(A_byte_813FDCC);

    call_reqBBS_8140600();
    call_ZeroFillGFX30025c0();
    call_sub_8046664();

    return r5_in;   /* r5 was pushed on entry and popped on exit */
}

/* reqBBS_813FDA8(r10, lr) -> r5 (r5 unchanged; r0-r3, lr and flags clobbered) */
Next reqBBS_813FDA8(void) {
    R5 = reqBBS_813FDA8_init(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_anim_814004C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n; } call_TestEventFlag_result_reqBBS_anim_814004C;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_reqBBS_anim_814004C call_TestEventFlag_reqBBS_anim_814004C(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_reqBBS_anim_814004C){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: CopyBackgroundTiles has no typed C interface yet */
static inline void call_CopyBackgroundTiles_reqBBS_anim_814004C(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles);
}

/* register form: generated wrapper: call_sub_3005EBA has no typed C interface yet */
static inline void call_call_sub_3005EBA_reqBBS_anim_814004C(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(call_sub_3005EBA);
}

/* For each of 8 BBS request entries on the current sub-screen, check its
 * event flag: if set, draw the entry's 2x2 tile block; if clear, blank it.
 * Then redraw the bottom border row. The BBS state object (r5) is returned
 * unchanged; all other registers are restored by the adapter. */
static u32 draw_bbs_request_tiles(u32 bbs_state, u32 toolkit) {
    u32 entry_base = LD16(bbs_state + 0x24);
    u32 frame = LD16(LD32(toolkit + oToolkit_CurFramePtr));
    u32 palette_off = (frame & 0x1F) << 2;
    u32 tile_ids = LD32(A_pt_81400C8 + palette_off);

    for (u32 i = 0; i < 8; i++) {
        u32 flag_base = LD32(LD32(bbs_state + 0x28) + 0xC);
        u32 event_flag = LD8(A_reqBBS_eRequestEntriesIDs + entry_base + i) + flag_base;

        if (!call_TestEventFlag_reqBBS_anim_814004C(event_flag).z)
            call_CopyBackgroundTiles_reqBBS_anim_814004C(2, 2 * i + 3, 2, tile_ids, 2, 2);
        else
            call_call_sub_3005EBA_reqBBS_anim_814004C(2, 2 * i + 3, 2, 0, 2, 2);
    }

    /* Bottom border row */
    call_CopyBackgroundTiles_reqBBS_anim_814004C(0x15, 0, 2, tile_ids, 2, 2);

    return bbs_state;
}

Next reqBBS_anim_814004C(void) {
    u32 s0 = R0, s1 = R1, s2 = R2, s3 = R3, s4 = R4, s6 = R6, s7 = R7;
    R5 = draw_bbs_request_tiles(R5, R10);
    R0 = s0, R1 = s1, R2 = s2, R3 = s3, R4 = s4, R6 = s6, R7 = s7;
    RETURN_TO_CALLER();
}

/* ── reqBBS_renderRankStars ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool z, n; } call_TestEventFlag_result_reqBBS_renderRankStars;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_reqBBS_renderRankStars call_TestEventFlag_reqBBS_renderRankStars(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_reqBBS_renderRankStars){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: call_sub_3005EBA has no typed C interface yet */
static inline void call_call_sub_3005EBA(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(call_sub_3005EBA);
}

/* register form: generated wrapper: CopyBackgroundTiles has no typed C interface yet */
static inline void call_CopyBackgroundTiles_reqBBS_renderRankStars(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles);
}

/* reqBBS_renderRankStars: render rank-star overlays on the request BBS page.
 * For each of the 8 request slots on the current page, if the slot's event flag
 * is set, prepare a 32×32 tile block and fill it with one star tile column per
 * earned rank star. The star count comes from byte_813F380 indexed by the
 * request-entry id; the tile id block is the constant dword_81402C8.
 * Input: gui (r5) – pointer to reqBBS GUI state. */
static void render_rank_stars(u32 gui) {
    /* register form: wrappers pass arguments through R4-R7, which callees may
     * clobber; the contract lists all four as preserved, so save and restore. */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    u32 page_pos = LD16(gui + oReqBBSGui_PagePos);

    for (u32 slot = 0; slot < 8; slot++) {
        u32 text_ptrs = LD32(gui + 0x28);           /* reqBBS_textualPointers */
        u32 event_base = LD32(text_ptrs + 8);
        u32 entry_id   = LD8(A_reqBBS_eRequestEntriesIDs + page_pos + slot);
        u32 flag       = entry_id + event_base;

        if (!call_TestEventFlag_reqBBS_renderRankStars(flag).z) {
            /* Prepare tile block 2 at column 0x16, row (2*slot + 3). */
            call_call_sub_3005EBA(0x16, 2 * slot + 3, 2, 0, 5, 2);

            /* stars = rank byte for this request + 1 (always >= 1). */
            u32 stars = (u32)LD8(A_byte_813F380 + entry_id) + 1;
            for (u32 j = 0; j < stars; j++) {
                call_CopyBackgroundTiles_reqBBS_renderRankStars(0x1a - j, 2 * slot + 3, 2,
                                         A_dword_81402C8, 1, 2);
            }
        }

        /* Restore preserved registers after any wrapper clobbering. */
        R4 = saved_r4;
        R5 = saved_r5;
        R6 = saved_r6;
        R7 = saved_r7;
    }
}

/* reqBBS_renderRankStars(r5 = gui) -> r5 (unchanged) */
Next reqBBS_renderRankStars(void) {
    render_rank_stars(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_8140358 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r10; } call_reqBBS_81403A8_result;
/* register form: generated wrapper: reqBBS_81403A8 has no typed C interface yet */
static inline call_reqBBS_81403A8_result call_reqBBS_81403A8(u32 r2, u32 r5) {
    R2 = r2;
    R5 = r5;
    GUEST_CALL(reqBBS_81403A8);
    return (call_reqBBS_81403A8_result){ R10 };
}

typedef struct { u32 r8, r9, r10, r11; } call_reqBBS_8140414_result;
/* register form: generated wrapper: reqBBS_8140414 has no typed C interface yet */
static inline call_reqBBS_8140414_result call_reqBBS_8140414(u32 r2, u32 r5) {
    R2 = r2;
    R5 = r5;
    GUEST_CALL(reqBBS_8140414);
    return (call_reqBBS_8140414_result){ R8, R9, R10, R11 };
}

typedef struct { u32 r8, r9, r10, r11; } call_reqBBS_81403C0_result;
/* register form: generated wrapper: reqBBS_81403C0 has no typed C interface yet */
static inline call_reqBBS_81403C0_result call_reqBBS_81403C0(u32 r2, u32 r5) {
    R2 = r2;
    R5 = r5;
    GUEST_CALL(reqBBS_81403C0);
    return (call_reqBBS_81403C0_result){ R8, R9, R10, R11 };
}

typedef struct { u32 r2, r3, r5, r7, r8, r9, r10, r12; bool v; } call_sub_811F7F8_result;
/* register form: generated wrapper: menu_adjustCursorOrValue_811F7F8 has no typed C interface yet */
static inline call_sub_811F7F8_result call_sub_811F7F8(u32 r0, u32 r1, u32 r2, u32 r3, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    GUEST_CALL(menu_adjustCursorOrValue_811F7F8);
    return (call_sub_811F7F8_result){ R2, R3, R5, R7, R8, R9, R10, R12, OVERFLOW_FLAG() };
}

/* Dispatch a BBS joypad request through the toolkit's joypad input.
 *
 * Reads the pressed button bits from the joypad pointed to by the toolkit
 * (R10) and forwards the request to one of four sub-handlers depending on
 * which button (or button combination) is pressed.  On the menu path it also
 * checks the held-button mask against 0x3C0 and, if any bit is set, calls
 * menu_adjustCursorOrValue_811F7F8 and stores the joypad pointer into the state block.
 *
 * R0  – ignored (set to r8 in the original, never read back)
 * R2  – parameter forwarded to every sub-handler
 * R5  – BBS state block (16-bit gate at +0x1E, byte at +0x0B written on menu path)
 * R10 – toolkit pointer (used to reach the joypad input structure)
 *
 * Preserved (original uses push {r4-r7,lr} / pop {r4-r7,pc}): R4, R5, R6, R7.
 * Output registers R10, R11 are left behind by the sub-handler wrappers. */
static void reqBBS_8140358_impl(u32 input_r2, u32 input_r5, u32 input_r10) {
    u32 joypad = LD32(input_r10 + oToolkit_JoypadPtr);
    u32 pressed = LD16(joypad + oJoypad_Pressed);

    /* R button: open / refresh the BBS screen. */
    if (pressed & 2) {
        call_reqBBS_81403A8(input_r2, input_r5);
        return;
    }

    /* If the per-slot action counter is zero there is nothing to dispatch. */
    if (LD16(input_r5 + 0x1E) == 0)
        return;

    /* A button: request the action handler. */
    if (pressed & 4) {
        call_reqBBS_8140414(input_r2, input_r5);
        return;
    }

    /* B button: request the cancel / back handler. */
    if (pressed & 1) {
        call_reqBBS_81403C0(input_r2, input_r5);
        return;
    }

    /* Start + Select + … mask 0x3C0 opens the menu through menu_adjustCursorOrValue_811F7F8. */
    u32 held = LD16(joypad + 4);
    if (held & 0x3C0) {
        call_sub_811F7F8(held, 8, 0, 0, input_r5);
        /* The original uses r7 (the joypad pointer) for the byte value. */
        ST8(input_r5 + 0x0B, joypad);
    }
}

/* register form: original push {r4-r7,lr}/pop {r4-r7,pc} protects R4-R7
 * across callee wrappers that may clobber them (reqBBS_8140414, reqBBS_81403C0
 * do not SYNC_SNAP R4/R7); reproduce the protection without stack writes. */
Next reqBBS_8140358(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    reqBBS_8140358_impl(R2, R5, R10);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    RETURN_TO_CALLER();
}

/* ── reqBBS_81403A8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r5; } call_SetScreenFade_result_reqBBS_81403A8;
/* register form: generated wrapper: SetScreenFade has no typed C interface yet */
static inline call_SetScreenFade_result_reqBBS_81403A8 call_SetScreenFade_reqBBS_81403A8(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetScreenFade);
    return (call_SetScreenFade_result_reqBBS_81403A8){ R1, R2, R3, R5 };
}

typedef struct { u32 r2, r3, r10; bool v; } call_PlaySoundEffect_result_reqBBS_81403A8;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_reqBBS_81403A8 call_PlaySoundEffect_reqBBS_81403A8(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_reqBBS_81403A8){ R2, R3, R10, OVERFLOW_FLAG() };
}

/* Request handler for the BBS menu: start a screen fade, mark the caller's
 * menu-state byte as "transitioning", and play the "unselect" sound.
 *
 * Note on r2: the original never reloads r2 before the sound call, so
 * PlaySoundEffect receives whatever SetScreenFade left in r2, not the
 * caller's r2. SetScreenFade's r5 result equals its r5 on entry (the
 * harness asserts it is unchanged), so the state pointer stays valid.
 *
 * The r10 left behind by PlaySoundEffect is this function's only live
 * output; the V flag it sets is not listed among our outputs. */
static u32 reqBBS_run(u32 statePtr) {
    /* Fade type 0x0C, speed 0x10 */
    call_SetScreenFade_result_reqBBS_81403A8 fade = call_SetScreenFade_reqBBS_81403A8(0xC, 0x10);

    /* Record the menu transition state in the caller's context byte */
    ST8(statePtr, 8);

    /* Play SOUND_UNSELECT_68 with r2 as SetScreenFade left it */
    call_PlaySoundEffect_result_reqBBS_81403A8 snd = call_PlaySoundEffect_reqBBS_81403A8(SOUND_UNSELECT_68, fade.r2);
    return snd.r10;
}

/* reqBBS_81403A8(r2 = caller arg, r5 = menu state byte pointer) -> r10 */
Next reqBBS_81403A8(void) {
    R10 = reqBBS_run(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_81403C0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6, r7, r10; } call_PlaySoundEffect_result_reqBBS_81403C0;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_reqBBS_81403C0 call_PlaySoundEffect_reqBBS_81403C0(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_reqBBS_81403C0){ R5, R6, R7, R10 };
}

typedef struct { u32 r4, r6, r7; } call_call_sub_3005EBA_result;
/* register form: generated wrapper: call_sub_3005EBA has no typed C interface yet */
static inline call_call_sub_3005EBA_result call_call_sub_3005EBA_reqBBS_81403C0(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(call_sub_3005EBA);
    return (call_call_sub_3005EBA_result){ R4, R6, R7 };
}

typedef struct { u32 r10, r11; } call_reqBBS_renderSelectedEntry_HeaderText_result_reqBBS_81403C0;
/* register form: generated wrapper: reqBBS_renderSelectedEntry_HeaderText has no typed C interface yet */
static inline call_reqBBS_renderSelectedEntry_HeaderText_result_reqBBS_81403C0 call_reqBBS_renderSelectedEntry_HeaderText_reqBBS_81403C0(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_renderSelectedEntry_HeaderText);
    return (call_reqBBS_renderSelectedEntry_HeaderText_result_reqBBS_81403C0){ R10, R11 };
}

/* BBS entry-open handler (0x81403C0): stamp the state byte, configure the
 * window-animation slot, play SELECT, launch the easing pass, redraw the
 * header text, then reset the message-box pointer. */

typedef struct { u32 r10; u32 r11; } ReqBBSSelResult;

static ReqBBSSelResult reqBBS_open_selection(u32 r5, u32 r10, u32 r2) {
    /* State byte 0x18 = "opening" */
    ST8(r5, 0x18);

    /* Window animation struct lives at the toolkit pointer + field offset */
    u32 win = LD32(r10 + oToolkit_Unk200f3a0_Ptr);
    ST8(win + 9,   0x1B);
    ST8(win + 0xA, 0x36);
    ST8(win + 3,   0x00);
    ST8(win + 2,   0xFF);
    ST8(win + 7,   0x50);
    ST8(win + 6,   0x50);

    /* SELECT sound – PlaySoundEffect touches r5/r10 but the wrapper restores them */
    (void)call_PlaySoundEffect_reqBBS_81403C0(SOUND_SELECT_67, r2);

    /* Sub-state field 8 = 6 (animation phase) */
    ST8(r5 + 8, 6);

    /* Easing / layout pass: 0×0 offset, size 3, 0×0 base, 0x1E×0x14 grid */
    (void)call_call_sub_3005EBA_reqBBS_81403C0(0, 0, 3, 0, 0x1E, 0x14);

    /* Render the header text; yields the updated toolkit pointer (r10) and r11 */
    call_reqBBS_renderSelectedEntry_HeaderText_result_reqBBS_81403C0 hdr =
        call_reqBBS_renderSelectedEntry_HeaderText_reqBBS_81403C0(r5);

    /* Message box (toolkit_new + 0x20): mark "ready", clear active-slot bytes */
    u32 box = LD32(hdr.r10 + 0x20);
    ST8(box,     0xF7);
    ST8(box + 4, 0x00);
    ST8(box + 6, 0x00);

    return (ReqBBSSelResult){ hdr.r10, hdr.r11 };
}

Next reqBBS_81403C0(void) {
    ReqBBSSelResult res = reqBBS_open_selection(R5, R10, R2);
    R10 = res.r10;
    R11 = res.r11;
    RETURN_TO_CALLER();
}

/* ── reqBBS_81405E8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; } call_CopyBackgroundTiles_result;
/* register form: generated wrapper: CopyBackgroundTiles has no typed C interface yet */
static inline call_CopyBackgroundTiles_result call_CopyBackgroundTiles(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles);
    return (call_CopyBackgroundTiles_result){ R1, R2, R3 };
}

/* Draw the BBS (bulletin board) background: copy a fixed 8x2 block of tiles
 * from the tile id table at byte_813F32C into tile block 32x32 #2 at
 * position (j=10, i=0).
 *
 * The original pushes r4-r7 around the call because CopyBackgroundTiles
 * itself clobbers r4 (and r5) - its nested 8x2 tile loops leave r4 = 18 -
 * and then pops them, so r4-r7 come out unchanged. The wrapper call gives
 * us CopyBackgroundTiles's leftover register values, so the saved r4/r5
 * have to be carried through the call and written back. */

typedef struct {
    u32 r4;
    u32 r5;
} BbsSavedRegs;

static BbsSavedRegs draw_bbs_background(u32 saved_r4, u32 saved_r5) {
    /* CopyBackgroundTiles(j, i, which_tile_block_32x32, tile_ids, j_size, i_size) */
    call_CopyBackgroundTiles(
        0xA,             /* j: destination tile column */
        0,               /* i: destination tile row */
        2,               /* tileBlock32x32 */
        A_byte_813F32C,  /* tileIds */
        8,               /* j size */
        2);              /* i size */

    BbsSavedRegs kept;
    kept.r4 = saved_r4;  /* restore what the original's pop {r4-r7} restores */
    kept.r5 = saved_r5;  /* r5 is an output for callers: unchanged on exit */
    return kept;
}

/* reqBBS_81405E8(lr = return address, r10 = context) -> r5 unchanged */
Next reqBBS_81405E8(void) {
    BbsSavedRegs kept = draw_bbs_background(R4, R5);
    R4 = kept.r4;
    R5 = kept.r5;
    RETURN_TO_CALLER();
}

/* ── reqBBS_8140600 ── */
/* reqBBS_8140600 is one of the bulletin-board "request" handlers in reqBBS.s.
 * Its entire body is `push {r4-r7,lr}` / `pop {r4-r7,pc}`: it accepts a request
 * number and does nothing with it, preserving every register and simply
 * returning to its caller. The typed form therefore just passes r4-r6 through
 * unchanged (r7 and the return address are preserved implicitly). */

struct ReqBBSResult {
    u32 r4;
    u32 r5;
    u32 r6;
};

static struct ReqBBSResult reqBBS_8140600_typed(u32 r4, u32 r5, u32 r6) {
    /* No request processing happens; every input is returned untouched. */
    struct ReqBBSResult result;
    result.r4 = r4;
    result.r5 = r5;
    result.r6 = r6;
    return result;
}

/* reqBBS_8140600() -> r4 r5 r6 (all unchanged; returns to lr) */
Next reqBBS_8140600(void) {
    struct ReqBBSResult result = reqBBS_8140600_typed(R4, R5, R6);
    R4 = result.r4;
    R5 = result.r5;
    R6 = result.r6;
    RETURN_TO_CALLER();
}

/* ── reqBBS_animateCursor ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7; } call_QueueEightWordAlignedGFXTransfer_result;
/* register form: generated wrapper: QueueEightWordAlignedGFXTransfer has no typed C interface yet */
static inline call_QueueEightWordAlignedGFXTransfer_result call_QueueEightWordAlignedGFXTransfer(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    return (call_QueueEightWordAlignedGFXTransfer_result){ R4, R5, R6, R7 };
}

typedef struct { bool c, v; } call_sprite_queueOamEntryThunk_802FE28_result;
/* register form: generated wrapper: sprite_queueOamEntryThunk_802FE28 has no typed C interface yet */
static inline call_sprite_queueOamEntryThunk_802FE28_result call_sprite_queueOamEntryThunk_802FE28(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);
    return (call_sprite_queueOamEntryThunk_802FE28_result){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Animate one frame of the BBS request cursor:
 *  - queue the cursor tileset slice for the current animation timer,
 *  - advance the pose index (wrapping past the 0xFF sentinel),
 *  - queue the resulting OAM sprite entry.
 * The original saves r0-r7 via push/pop; we replicate the preservation explicitly. */
static void reqBBS_animateCursor_impl(u32 gui_ptr) {
    /* register form: the original's push {r0-r7,lr} preserves these across
       the two callee calls which overwrite r0-r7; C locals cannot shadow
       guest-register side effects. */
    u32 saved_r0 = R0, saved_r1 = R1, saved_r2 = R2, saved_r3 = R3;
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    /* Look up the tileset frame index from the per-timer table. */
    u32 timer = LD8(gui_ptr + oReqBBSGui_AnimationTimer1);
    u32 frame_table = A_dword_81406E4 + 2;
    u32 frame = LD8(frame_table + timer);

    /* Queue 0x80 bytes of tile data: src = frame*128 + tileset base, dest = OBJ tile 0. */
    call_QueueEightWordAlignedGFXTransfer((frame << 7) + A_unk_201AA04, 0x6017F80, 0x80);

    /* Sprite Y coordinate from the GUI object's row field: row * 16 + 24. */
    u32 row = LD16(gui_ptr + 0x20);
    u32 y_coord = row * 0x10 + 0x18;

    /* Advance pose index; the 0xFF sentinel in the table means restart at pose 1. */
    u32 pose = LD8(gui_ptr + 9) + 1;
    if (LD8(frame_table + pose) == 0xFF)
        pose = 1;
    ST8(gui_ptr + 9, pose);

    /* Pack OAM attribute word: enable(0x40000000) | tile-bank(3<<16) | y. */
    u32 attr = (3u << 16) | y_coord | 0x40000000;

    /* Queue the cursor sprite's OAM entry (tile 0xC3FC, no flip, layer 3). */
    call_sprite_queueOamEntryThunk_802FE28(attr, 0xC3FC, 0, 3);

    /* Restore the preserved registers. */
    R0 = saved_r0; R1 = saved_r1; R2 = saved_r2; R3 = saved_r3;
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
}

/* 0x08140688: animate BBS request cursor (r5 = request-GUI object). */
Next reqBBS_animateCursor(void) {
    reqBBS_animateCursor_impl(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_81406FC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: sprite_setUnk0x2c has no typed C interface yet */
static inline void call_sprite_setUnk0x2c(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setUnk0x2c);
}

/* reqBBS_81406FC: Build a per-sprite animation mask by banding the HP half-word
 * (r5+0x1E) and stock byte (r5+0x5) each into a band bit and a progress bit,
 * XOR-ing all four bits into 0x3FFFFFFF, then applying the result via
 * sprite_setUnk0x2c on the eStructArr2008450 sprite array. */

/* ARM THUMB register-form LSL of the value 1: shift amount taken from low byte;
 * if the effective shift >= 32 the result is 0 (all bits shifted out). */
static u32 one_lsl(u32 shift) {
    u32 s = shift & 0xFFu;
    if (s >= 32u)
        return 0;
    return 1u << s;
}

static u32 build_sprite_mask(u32 r5) {
    u32 mask = 0x3FFFFFFFu;

    /* --- HP half-word (offset 0x1E): band bit 0x0A..0x0E --- */
    u32 hp = LD16(r5 + 0x1E);
    u32 hp_base, hp_band;
    if (hp >= 0x28u)      { hp_base = 0x28u; hp_band = 0x0Au; }
    else if (hp >= 0x1Eu) { hp_base = 0x1Eu; hp_band = 0x0Bu; }
    else if (hp >= 0x14u) { hp_base = 0x14u; hp_band = 0x0Cu; }
    else if (hp >= 0x0Au) { hp_base = 0x0Au; hp_band = 0x0Du; }
    else                  { hp_base = 0x00u; hp_band = 0x0Eu; }
    mask ^= one_lsl(hp_band);

    /* Progress bit within the band: 9 - (current_hp - band_floor), re-reads HP. */
    u32 hp_delta = LD16(r5 + 0x1E) - hp_base;
    mask ^= one_lsl(9u - hp_delta);

    /* --- Stock byte (offset 0x5): band bit 0x19..0x1D --- */
    u32 stock = LD8(r5 + 5);
    u32 stock_base, stock_band;
    if (stock >= 0x28u)      { stock_base = 0x28u; stock_band = 0x19u; }
    else if (stock >= 0x1Eu) { stock_base = 0x1Eu; stock_band = 0x1Au; }
    else if (stock >= 0x14u) { stock_base = 0x14u; stock_band = 0x1Bu; }
    else if (stock >= 0x0Au) { stock_base = 0x0Au; stock_band = 0x1Cu; }
    else                     { stock_base = 0x00u; stock_band = 0x1Du; }
    mask ^= one_lsl(stock_band);

    /* Progress bit for stock: 9 - delta + 0xF, re-reads stock. */
    u32 stock_delta = LD8(r5 + 5) - stock_base;
    mask ^= one_lsl((9u - stock_delta) + 0xFu);

    return mask;
}

/* reqBBS_81406FC(r5 = sprite data pointer) -> (void); preserves r5 */
Next reqBBS_81406FC(void) {
    /* register form: r5 is preserved across the call because the original pushes/pops it,
       and call_sprite_setUnk0x2c overwrites r5 with the sprite-array address */
    u32 saved_r5 = R5;
    u32 mask = build_sprite_mask(R5);
    call_sprite_setUnk0x2c(mask, A_eStructArr2008450);
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── reqBBS_getRequestBBSRank ── */
/* Request BBS rank: reads the current save's request rank byte straight out of
 * the request-BBS GUI work area (eReqBBSGui + 0xE).
 *
 * The original also happens to leave r1 holding the 0xE offset it loaded, and
 * returns with r4-r7 restored, so those are passed through unchanged. */
#define REQ_BBS_RANK_OFFSET 0xE /* dword_81407A4 */

struct ReqBBSRankResult {
    u32 rank;   /* r0: the rank byte */
    u32 offset; /* r1: left as 0xE by the original's literal load */
    u32 r4, r5, r6, r7;
};

static struct ReqBBSRankResult reqBBS_read_rank(u32 r4, u32 r5, u32 r6, u32 r7)
{
    struct ReqBBSRankResult res;

    res.rank = LD8(A_eReqBBSGui + REQ_BBS_RANK_OFFSET);
    res.offset = REQ_BBS_RANK_OFFSET;
    res.r4 = r4;
    res.r5 = r5;
    res.r6 = r6;
    res.r7 = r7;
    return res;
}

Next reqBBS_getRequestBBSRank(void)
{
    struct ReqBBSRankResult res = reqBBS_read_rank(R4, R5, R6, R7);
    R0 = res.rank;
    R1 = res.offset;
    R4 = res.r4;
    R5 = res.r5;
    R6 = res.r6;
    R7 = res.r7;
    RETURN_TO_CALLER();
}

/* ── reqBBS_81408A0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_SetEventFlag_result_reqBBS_81408A0;
/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline call_SetEventFlag_result_reqBBS_81408A0 call_SetEventFlag_reqBBS_81408A0(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
    return (call_SetEventFlag_result_reqBBS_81408A0){ R3 };
}

/* ReqBBS "request accepted/completed" flag setter: adds the fixed flag-table
 * offset (reqBBS_textualPointers + 0x10) to the request index in r0 and sets
 * that event flag. r10 (the flag-word base) is consumed by SetEventFlag.
 * Returns r5 unchanged (the original only restores it from the stack). */
static u32 set_bbs_request_flag(u32 requestIndex, u32 r5_out) {
    u32 flagId = requestIndex + LD32(A_reqBBS_textualPointers + 0x10);
    call_SetEventFlag_reqBBS_81408A0(flagId);
    return r5_out;
}

Next reqBBS_81408A0(void) {
    R5 = set_bbs_request_flag(R0, R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_81408B4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; } call_SetEventFlag_result_reqBBS_81408B4;
/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline call_SetEventFlag_result_reqBBS_81408B4 call_SetEventFlag_reqBBS_81408B4(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
    return (call_SetEventFlag_result_reqBBS_81408B4){ R1, R3 };
}

/* SetEventFlag wrapper: r10 must stay as on entry, which the wrapper guarantees. */

/* reqBBS_81408B4: mark a BBS request as read. r0 holds the request's flag
 * index; the request-text table's entry at offset 0x18 supplies the base
 * value that is added to it before the flag is set.
 * Outputs r3/r4-r7: r4-r7 are preserved, r3 is whatever SetEventFlag left. */
static void request_bbs_flag(u32 flagIndex) {
    u32 flagBase = LD32(A_reqBBS_textualPointers + 0x18);
    call_SetEventFlag_reqBBS_81408B4(flagIndex + flagBase);
}

Next reqBBS_81408B4(void) {
    request_bbs_flag(R0);
    RETURN_TO_CALLER();
}

/* ── reqBBS_81408C8 ── */
/* reqBBS_81408C8 — read the request ID of the entry selected on the BBS board.
 * r5 points at the board state: halfword 0x20 is the page/cursor index and
 * halfword 0x24 the per-page offset; their sum indexes the ID table.
 * Returns the table byte in r0 and passes r5 through unchanged. */
static u32 reqBBS_selected_entry_id(u32 board_state) {
    u32 entry_index = LD16(board_state + 0x24) + LD16(board_state + 0x20);
    return LD8(A_reqBBS_eRequestEntriesIDs + entry_index);
}

/* inputs: r5 (board state pointer; lr unused)
 * outputs: r0 = entry ID, r5 unchanged */
Next reqBBS_81408C8(void) {
    R0 = reqBBS_selected_entry_id(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_81408DC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_reqBBS_81408C8_result;
/* register form: generated wrapper: reqBBS_81408C8 has no typed C interface yet */
static inline call_reqBBS_81408C8_result call_reqBBS_81408C8(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_81408C8);
    return (call_reqBBS_81408C8_result){ R0 };
}

/* Update the byte at eReqBBSGui+0xF with the value reqBBS_81408C8 computes from r5. */
static void store_req_bbs_gui_byte(u32 arg_r5) {
    u32 value = call_reqBBS_81408C8(arg_r5).r0;

    ST8(A_eReqBBSGui + 0xF, value);
}

/* reqBBS_81408DC(r5 = argument for reqBBS_81408C8) */
Next reqBBS_81408DC(void) {
    store_req_bbs_gui_byte(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_81408F0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; bool z, n; } call_chatbox_mask_eFlags2009F38_result_reqBBS_81408F0;
/* register form: generated wrapper: chatbox_mask_eFlags2009F38 has no typed C interface yet */
static inline call_chatbox_mask_eFlags2009F38_result_reqBBS_81408F0 call_chatbox_mask_eFlags2009F38_reqBBS_81408F0(u32 r0) {
    R0 = r0;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return (call_chatbox_mask_eFlags2009F38_result_reqBBS_81408F0){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r5; } call_reqBBS_81408C8_result_reqBBS_81408F0;
/* register form: generated wrapper: reqBBS_81408C8 has no typed C interface yet */
static inline call_reqBBS_81408C8_result_reqBBS_81408F0 call_reqBBS_81408C8_reqBBS_81408F0(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_81408C8);
    return (call_reqBBS_81408C8_result_reqBBS_81408F0){ R0, R5 };
}

typedef struct { u32 r0, r5; } call_reqBBS_8140868_result;
/* register form: generated wrapper: reqBBS_8140868 has no typed C interface yet */
static inline call_reqBBS_8140868_result call_reqBBS_8140868(u32 r0) {
    R0 = r0;
    GUEST_CALL(reqBBS_8140868);
    return (call_reqBBS_8140868_result){ R0, R5 };
}

typedef struct { u32 r1, r3; bool z, n, c, v; } call_TestEventFlagFromImmediate_result_reqBBS_81408F0;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_reqBBS_81408F0 call_TestEventFlagFromImmediate_reqBBS_81408F0(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_reqBBS_81408F0){ R1, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r5; } call_reqBBS_8140884_result;
/* register form: generated wrapper: reqBBS_8140884 has no typed C interface yet */
static inline call_reqBBS_8140884_result call_reqBBS_8140884(u32 r0) {
    R0 = r0;
    GUEST_CALL(reqBBS_8140884);
    return (call_reqBBS_8140884_result){ R0, R5 };
}

typedef struct { u32 r0, r3; } call_GetCurPETNavi_result_reqBBS_81408F0;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result_reqBBS_81408F0 call_GetCurPETNavi_reqBBS_81408F0(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result_reqBBS_81408F0){ R0, R3 };
}

typedef struct { u32 r3; } call_SetEventFlagFromImmediate_result;
/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline call_SetEventFlagFromImmediate_result call_SetEventFlagFromImmediate_reqBBS_81408F0(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
    return (call_SetEventFlagFromImmediate_result){ R3 };
}

typedef struct { u32 r5; } call_reqBBS_81408A0_result;
/* register form: generated wrapper: reqBBS_81408A0 has no typed C interface yet */
static inline call_reqBBS_81408A0_result call_reqBBS_81408A0(u32 r0) {
    R0 = r0;
    GUEST_CALL(reqBBS_81408A0);
    return (call_reqBBS_81408A0_result){ R5 };
}

/* register form: generated wrapper: reqBBS_81408DC has no typed C interface yet */
static inline void call_reqBBS_81408DC(u32 r5) {
    R5 = r5;
    GUEST_CALL(reqBBS_81408DC);
}

/* BBS request handler: gates on chatbox/EFLAGS mask bits, tests event flags
 * and PET/Navi state, then on success sets flags 0x173A/0x173C and runs the
 * BBS sub-request helper chain.  Returns a result code in r0.
 * r5 is tracked locally for intermediate helper calls but is preserved
 * (pop restores it) so it is not a computed output. */
static u32 bbs_request(u32 r5) {
    /* Gate: chatbox mask bit 0x80 – if Z (bit absent), bail with 0xE. */
    if (call_chatbox_mask_eFlags2009F38_reqBBS_81408F0(0x80).z)
        return 0xE;

    /* Gate: chatbox mask bit 0x20 – if Z (bit absent), bail with 0. */
    if (call_chatbox_mask_eFlags2009F38_reqBBS_81408F0(0x20).z)
        return 0;

    /* Gate: chatbox mask bit 0x110 – if NOT Z (bit present), bail with 0. */
    if (!call_chatbox_mask_eFlags2009F38_reqBBS_81408F0(0x110).z)
        return 0;

    /* Helper pair 1: 81408C8(r5)→r0,r5; 8140868(r0)→r0,r5. */
    call_reqBBS_81408C8_result_reqBBS_81408F0 c1 = call_reqBBS_81408C8_reqBBS_81408F0(r5);
    r5 = c1.r5;
    u32 r0_val = c1.r0;

    call_reqBBS_8140868_result c2 = call_reqBBS_8140868(r0_val);
    r5 = c2.r5;
    r0_val = c2.r0;

    if (r0_val != 0)
        return 0xE;

    /* Event flag 0x173A must be clear (Z set) to continue. */
    if (!call_TestEventFlagFromImmediate_reqBBS_81408F0(0x17, 0x3A).z)
        return 0xF;

    /* Helper pair 2: 81408C8(r5)→r0,r5; 8140884(r0)→r0,r5. */
    call_reqBBS_81408C8_result_reqBBS_81408F0 c3 = call_reqBBS_81408C8_reqBBS_81408F0(r5);
    r5 = c3.r5;
    r0_val = c3.r0;

    call_reqBBS_8140884_result c4 = call_reqBBS_8140884(r0_val);
    r5 = c4.r5;
    r0_val = c4.r0;

    if (r0_val != 0)
        return 0x10;

    /* EVENT_PET_NAVI_ACTIVE must be set (Z clear) to continue. */
    if (call_TestEventFlagFromImmediate_reqBBS_81408F0(0x17, 0x1D).z)
        return 0xD;

    /* No current PET/Navi required. */
    if (call_GetCurPETNavi_reqBBS_81408F0().r0 != 0)
        return 0xD;

    /* Success path: set event flags 0x173A and 0x173C. */
    call_SetEventFlagFromImmediate_reqBBS_81408F0(0x17, 0x3A);
    call_SetEventFlagFromImmediate_reqBBS_81408F0(0x17, 0x3C);

    /* Helper chain 3: 81408C8(r5)→r0,r5; 81408A0(r0)→r5; 81408DC(r5). */
    call_reqBBS_81408C8_result_reqBBS_81408F0 c5 = call_reqBBS_81408C8_reqBBS_81408F0(r5);
    r5 = c5.r5;
    r0_val = c5.r0;

    call_reqBBS_81408A0_result c6 = call_reqBBS_81408A0(r0_val);
    r5 = c6.r5;

    call_reqBBS_81408DC(r5);

    return 0xD;
}

/* reqBBS_81408F0(r5 = BBS toolkit ptr) -> r0 (result code) */
Next reqBBS_81408F0(void) {
    R0 = bbs_request(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_8140974 ── */
/* reqBBS_8140974: read a state byte out of the BBS request board GUI object.
 *
 * The original loads the pointer to the eReqBBSGui instance and returns the byte
 * stored 0x0F bytes into it (its layout constant 0xF lives in dword_8140980).
 * r5 is listed as an output, but the routine only restores it from its own save
 * slot, so it comes back exactly as it came in; the typed function threads it
 * through unchanged, as the register contract requires.
 */

/* One result of the read: the byte value (r0) and the untouched r5 passthrough. */
typedef struct {
    u32 r0;
    u32 r5;
} ReqBBSStateResult;

/* Fetch the state byte from the BBS request GUI at fixed offset 0x0F. */
static ReqBBSStateResult reqBBS_read_state(u32 r5) {
    ReqBBSStateResult out;

    out.r0 = LD8(A_eReqBBSGui + 0x0F);   /* ldr r1, =0xf; ldrb r0, [r0, r1] */
    out.r5 = r5;                         /* preserved verbatim by the original */
    return out;
}

/* reqBBS_8140974() -> r0 = state byte, r5 unchanged */
Next reqBBS_8140974(void) {
    ReqBBSStateResult out = reqBBS_read_state(R5);
    R0 = out.r0;
    R5 = out.r5;
    RETURN_TO_CALLER();
}

/* ── reqBBS_8140984 ── */
/* Clear three single-byte flags at offsets 0xD, 0xE, 0xF in the eReqBBSGui state struct. */
static void clear_reqbbs_flags(void) {
    u8 zero = 0;
    u32 gui = A_eReqBBSGui;
    ST8(gui + 0xD, zero);
    ST8(gui + 0xE, zero);
    ST8(gui + 0xF, zero);
}

Next reqBBS_8140984(void) {
    clear_reqbbs_flags();
    /* The original's last instructions load r2 = 0xF, leaving 0xF in r2. */
    R2 = 0xF;
    RETURN_TO_CALLER();
}

/* ── reqBBS_81409D0 ── */
/* reqBBS_81409D0: divide the incoming argument by a random byte via BIOS svc 6
 * (Div, with r0 = denominator, r1 = numerator) and return the remainder in r0.
 *
 *   inputs:  r0 = dividend, lr
 *   outputs: r0 = remainder, r1 = remainder, r2, r3, r6, r7, lr
 *   clobbers: nz, c, v
 *   preserved: r6, r7
 */
Next reqBBS_81409D0(void) {
    u32 arg = R0;
    u32 saved_r6 = R6;

    /* bl GetRNG -- random 32-bit value returned in r0 */
    GUEST_CALL(GetRNG);

    /* mov r1, #0xff / and r0, r1 -- mask to a 0..255 divisor */
    R0 = R0 & 0xFFu;

    /* mov r1, r6 -- r1 = original argument (numerator) */
    R1 = arg;

    /* svc 6: BIOS Div. r0 = denominator (random byte), r1 = numerator (arg).
     * Returns: r0 = quotient, r1 = remainder. */
    swi_call(6);

    /* mov r0, r1 -- surface the remainder to the caller */
    R0 = R1;

    /* Restore caller's r6. The push {r6,lr} / pop {r6,pc} pair originally saved
     * r6 on entry and restored the same value on exit, so r6 ends up unchanged. */
    R6 = saved_r6;

    RETURN_TO_CALLER();
}

/* ── reqBBS_81409E4 ── */
/* reqBBS_81409E4: trigger-request dispatcher for the overworld player.
 *
 * Asks the overworld player's NPC-script array whether a coordinate
 * trigger fires (e.g. an "on touch" BBS script).
 *
 *   inputs:  r10 lr
 *   outputs: r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 r10 r11 r12 lr
 *   clobbers: nz c v
 *   preserved: r4 r5 r6 r7 r8 r9 r12
 */
Next reqBBS_81409E4(void) {
    /* push {lr} — save the caller's return address on the guest stack. */
    u32 ret_addr = LR;
    SP -= 4;
    ST32(SP, ret_addr);

    /* Toolkit -> GameState -> OverworldPlayerObject + 0x1C (NPC.scriptArrayOffset). */
    R0 = LD32(R10 + oToolkit_GameStatePtr);
    R0 = LD32(R0  + oGameState_OverworldPlayerObjectPtr);
    R0 = R0 + 0x1C;

    /* bl checkCoordinateTrigger_8031a7a — leaves its results in R0..R3 etc. */
    GUEST_CALL(checkCoordinateTrigger_8031a7a);

    /* pop {pc} — restore LR to the caller's return address and return to it. */
    LR = LD32(SP);
    SP += 4;
    RETURN_TO_CALLER();
}

/* ── reqBBS_81409F4 ── */
/* reqBBS_81409F4: read the coord-interaction value from the current game state.
 * Inputs:  r10 = toolkit pointer
 * Outputs: r0 = CoordInteractionValue (u8, zero-extended) */
static u32 reqBBS_81409F4_typed(u32 toolkit) {
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    return LD8(gameState + oGameState_CoordInteractionValue);
}

Next reqBBS_81409F4(void) {
    R0 = reqBBS_81409F4_typed(R10);
    RETURN_TO_CALLER();
}

/* ── reqBBS_setFlag_e17b0f7_8140A00 ── */
Next reqBBS_setFlag_e17b0f7_8140A00(void) {
    /* entry 17, byte 0, flag 7: set the corresponding event flag. */
    R0 = 0x17;
    R1 = 0;
    GUEST_CALL(SetEventFlagFromImmediate);
    RETURN_TO_CALLER();
}

/* ── reqBBS_clearFlag_8140A0C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; bool c, v; } call_ClearEventFlagFromImmediate_result_reqBBS_clearFlag_8140A0C;
/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline call_ClearEventFlagFromImmediate_result_reqBBS_clearFlag_8140A0C call_ClearEventFlagFromImmediate_reqBBS_clearFlag_8140A0C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
    return (call_ClearEventFlagFromImmediate_result_reqBBS_clearFlag_8140A0C){ R1, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* reqBBS_clearFlag_8140A0C: finish a bulletin-board request.
 *
 * Clears the request's event flag (EVENT_1700), then copies the request's
 * state halfword from offset 0x14 of the save work area onto offset 0x12 --
 * the slot the game checks to treat the request as resolved.
 *
 * Register effects visible to callers:
 *   r1     - as left by ClearEventFlagFromImmediate
 *   r3     - 0x12, the destination halfword offset (the original's final r3)
 *   c/v    - as left by ClearEventFlagFromImmediate (the closing
 *            `movs r3, #0x12` only refreshes n/z, which is a clobber here)
 *   r4/r5  - preserved, as by the original's push/pop
 *   lr     - as left by ClearEventFlagFromImmediate; pop {pc} returns through
 *            the stacked value, so the adapter leaves lr alone */

/* Results of the typed worker that the adapter distributes to the registers. */
typedef struct {
    u32 r1;   /* r1 as left by ClearEventFlagFromImmediate */
    u32 r3;   /* final r3: destination halfword offset */
} BbsClearResult;

/* Clear the request's event flag and fold its state onto the "resolved" slot. */
static BbsClearResult bbs_clear_request_flag(u32 toolkit) {
    BbsClearResult out;

    /* movflag EVENT_1700: ClearEventFlagFromImmediate takes the flag number
     * split across registers -- high byte in r0, low byte in r1. */
    call_ClearEventFlagFromImmediate_result_reqBBS_clearFlag_8140A0C cleared =
        call_ClearEventFlagFromImmediate_reqBBS_clearFlag_8140A0C(EVENT_1700 >> 8, EVENT_1700 & 0xFF);

    /* Copy the request state halfword 0x14 -> 0x12 in the 2001c04 work area. */
    u32 work = LD32(toolkit + oToolkit_S2001c04_Ptr);
    u32 state = LD16(work + 0x14);
    ST16(work + 0x12, state);

    out.r1 = cleared.r1;
    out.r3 = 0x12;
    return out;
}

Next reqBBS_clearFlag_8140A0C(void) {
    BbsClearResult out = bbs_clear_request_flag(R10);
    R1 = out.r1;
    R3 = out.r3;
    /* `movs r3, #0x12` is the original's last flag-setting instruction: it
     * refreshes n/z only, so c/v stay as ClearEventFlagFromImmediate left
     * them. */
    SET_FLAGS_NZ(R3);
    RETURN_TO_CALLER();
}

/* ── reqBBS_setFlags_8140A24 ── */
/* reqBBS_setFlags_8140A24: mark the request-board (BBS) event flags as set.
 * Sets the 0x40 flags of EVENT_1640 and the 0x10 flags of EVENT_16C0 via
 * SetEventFlagRangeFromImmediate, then returns 0. */
Next reqBBS_setFlags_8140A24(void) {
    /* EVENT_1640: event word 0x16, 0x40 entries starting at byte flag 0x40 */
    R0 = 0x16;
    R1 = 0x40;
    R2 = 0x40;
    GUEST_CALL(SetEventFlagRangeFromImmediate); /* (u8 entryIdx, u8 byteFlagIdx, int numEntries) */

    /* EVENT_16C0: event word 0x16, 0x10 entries starting at byte flag 0xC0 */
    R0 = 0x16;
    R1 = 0xC0;
    R2 = 0x10;
    GUEST_CALL(SetEventFlagRangeFromImmediate);

    R0 = 0; /* always report success */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
