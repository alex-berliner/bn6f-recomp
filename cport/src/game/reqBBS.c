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

/* ── reqBBS_setFlag_e17b0f7_8140A00 ── */
Next reqBBS_setFlag_e17b0f7_8140A00(void) {
    /* entry 17, byte 0, flag 7: set the corresponding event flag. */
    R0 = 0x17;
    R1 = 0;
    GUEST_CALL(SetEventFlagFromImmediate);
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
