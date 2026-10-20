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
