/*
 * asm33.c: readable functions of bn6f asm33.s, up to 0x081263D0, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_loadTileGridByNavi_8123300 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4; } call_sub_8123360_result;
/* register form: generated wrapper: sub_8123360 has no typed C interface yet */
static inline call_sub_8123360_result call_sub_8123360(void) {
    GUEST_CALL(sub_8123360);
    return (call_sub_8123360_result){ R0, R4 };
}

typedef struct { u32 r0, r3; } call_GetCurPETNavi_result;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result call_GetCurPETNavi(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result){ R0, R3 };
}

/* register form: generated wrapper: SWI_LZ77UnCompReadNormalWrite8bit has no typed C interface yet */
static inline void call_SWI_LZ77UnCompReadNormalWrite8bit(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SWI_LZ77UnCompReadNormalWrite8bit);
}

typedef struct { u32 r2, r3; } call_CopyBytes_result;
/* register form: generated wrapper: CopyBytes has no typed C interface yet */
static inline call_CopyBytes_result call_CopyBytes(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyBytes);
    return (call_CopyBytes_result){ R2, R3 };
}

typedef struct { u32 r1, r2, r3; } call_CopyHalfwords_result;
/* register form: generated wrapper: CopyHalfwords has no typed C interface yet */
static inline call_CopyHalfwords_result call_CopyHalfwords(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyHalfwords);
    return (call_CopyHalfwords_result){ R1, R2, R3 };
}

typedef struct { u32 r1, r2, r3; } call_CopyWords_result;
/* register form: generated wrapper: CopyWords has no typed C interface yet */
static inline call_CopyWords_result call_CopyWords(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyWords);
    return (call_CopyWords_result){ R1, R2, R3 };
}

typedef struct { u32 r1, r2, r3; } call_CopyByEightWords_result;
/* register form: generated wrapper: CopyByEightWords has no typed C interface yet */
static inline call_CopyByEightWords_result call_CopyByEightWords(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyByEightWords);
    return (call_CopyByEightWords_result){ R1, R2, R3 };
}

/* Decompress the LZ77 stream referenced by the selected entry of bank_table into out_buf,
 * then copy (header_word >> 8) - 4 halfwords of payload data from out_buf+4 into dest_buf
 * using the widest legal copy unit. The bank index is 0x30 by default, or (pet_navi*4)
 * when sub_8123360 returns non-zero. */
static void decompress_navi_graphic(u32 bank_table, u32 dest_buf, u32 out_buf) {
    u32 bank_index;
    u32 lz_src;
    u32 header_word;
    u32 halfword_count;

    if (call_sub_8123360().r0 != 0)
        bank_index = (u32)call_GetCurPETNavi().r0 << 2;
    else
        bank_index = 0x30;

    /* Decompress bank_table[bank_index] -> out_buf. */
    lz_src = LD32(bank_table + bank_index);
    call_SWI_LZ77UnCompReadNormalWrite8bit(lz_src, out_buf);

    /* LZ77 header: top 24 bits are the payload length in bytes. Subtract the 4-byte
     * header itself to get the post-header payload size in bytes. */
    header_word = LD32(out_buf);
    halfword_count = (header_word >> 8) - 4;

    /* Dispatch on the low bits so the widest legal copy can be used. The original
     * tests 1, then 3, then 0x1F: bit 0 set => CopyBytes, bit 1 set => CopyHalfwords,
     * bits 0-4 set => CopyWords, else CopyByEightWords. */
    if (halfword_count & 1u) {
        call_CopyBytes(out_buf + 4, dest_buf, halfword_count);
    } else if (halfword_count & 2u) {
        call_CopyHalfwords(out_buf + 4, dest_buf, halfword_count);
    } else if (halfword_count & 0x1Fu) {
        call_CopyWords(out_buf + 4, dest_buf, halfword_count);
    } else {
        call_CopyByEightWords(out_buf + 4, dest_buf, halfword_count);
    }
}

/* battle_loadTileGridByNavi_8123300(r0 = bank_table, r1 = dest_buf, r2 = out_buf)
 * Loads/decompresses the current Navi's tile graphic into dest_buf. r1 and r2 are
 * preserved across every callee, so they pass through unchanged. r4 and r7 are also
 * preserved (r4 never appears in the body; r7 is a scratch the inner code restores
 * from the stack). */
Next battle_loadTileGridByNavi_8123300(void) {
    u32 dest_buf = R1;
    u32 out_buf  = R2;

    decompress_navi_graphic(R0, dest_buf, out_buf);

    /* Forward the preserved values back to the caller. */
    R1 = dest_buf;
    R2 = out_buf;
    RETURN_TO_CALLER();
}

/* ── sub_8123360 ── */
/* sub_8123360: () -> bool (r0)
 *
 * Decides whether the current PET navi is considered "present" (used for
 * overworld HUD / dialogue behaviour). Returns TRUE in r0 when:
 *   - a navi is selected in the PET (GetCurPETNavi() != 0) and the
 *     "navi gone" story event (EVENT_172A) has not fired, or
 *   - the same holds, and additionally on normal maps (map group < 0x80)
 *     the EVENT_PET_NAVI_ACTIVE flag is set.
 * On high map groups (>= 0x80, special areas) the active flag is not
 * required. r4 is preserved.
 */

/* TestEventFlagFromImmediate receives the 16-bit flag id split across
 * r0 (high byte) and r1 (low byte) and reports "flag set" via !Z. */
static u32 event_flag_set(u16 flag) {
    R0 = (u32)flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

Next sub_8123360(void) {
    u32 result = FALSE;

    GUEST_CALL(GetCurPETNavi);
    u32 curNavi = R0;

    /* A navi is around unless the story event removed it. */
    if (curNavi != 0 || !event_flag_set(EVENT_172A)) {
        result = TRUE;

        u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
        u32 mapGroup = LD8(gameState + oGameState_MapGroup);

        if ((s32)mapGroup < 0x80) {
            /* On regular maps the navi only counts as present while
             * its "navi active" event flag is up. */
            result = event_flag_set(EVENT_PET_NAVI_ACTIVE);
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── HandleChipFolderMenu8123434 ── */
/* HandleChipFolderMenu8123434: 08123434-0812345C THUMB, bn6f asm33.s:390
 *   inputs:  r2 r4 r5 r6 r7 r10 lr
 *   outputs: r5 r8 r9 r10 r11 r12
 *   clobbers (unused by callers): nz c v r0 r1 r2 r3 r4 r6 r7 lr
 *   preserved: -
 */

/* Offset of the state byte in the chip-folder menu work area (r5).
 * NOTE: the stored value is a *byte offset* into the five-entry handler
 * table at off_8123448 -- the original indexes it with an unshifted
 * "ldr r0,[r0,r1]", so the stored states are 0, 4, 8, 12, 16 and the
 * handler index is state / 4, not state itself. */
#define oFolderMenu_State 1

/* Each handler table entry is one word wide. */
#define FOLDER_MENU_TABLE_STRIDE 4

/* register form: calls chipFolder_screenInit_812345C outside this file, which has no typed C interface yet */
static void folder_menu_state0(u32 state) {
    R0 = 0x0812345D; /* chipFolder_screenInit_812345C+1: the raw bx r0 target the original passes */
    R1 = state;
    GUEST_CALL(chipFolder_screenInit_812345C);
}

/* register form: calls battleObject_runUpdateHandlerByType_8123514 outside this file, which has no typed C interface yet */
static void folder_menu_state1(u32 state) {
    R0 = 0x08123515; /* battleObject_runUpdateHandlerByType_8123514+1 */
    R1 = state;
    GUEST_CALL(battleObject_runUpdateHandlerByType_8123514);
}

/* register form: calls pet_menu_runFrame_81237BC outside this file, which has no typed C interface yet */
static void folder_menu_state2(u32 state) {
    R0 = 0x081237BD; /* pet_menu_runFrame_81237BC+1 */
    R1 = state;
    GUEST_CALL(pet_menu_runFrame_81237BC);
}

/* register form: calls chipFolder_init_812349C outside this file, which has no typed C interface yet */
static void folder_menu_state3(u32 state) {
    R0 = 0x0812349D; /* chipFolder_init_812349C+1 */
    R1 = state;
    GUEST_CALL(chipFolder_init_812349C);
}

/* register form: calls chipFolder_onEnter outside this file, which has no typed C interface yet */
static void folder_menu_state4(u32 state) {
    R0 = 0x081234DD; /* chipFolder_onEnter+1 */
    R1 = state;
    GUEST_CALL(chipFolder_onEnter);
}

/* The chip-folder menu is a five-state machine: the state byte of the work
 * area in r5 selects one of five handlers via the jump table at off_8123448.
 * The chosen handler is entered exactly as the original's "mov lr,pc; bx r0"
 * leaves things: r0 = the handler's own table entry, r1 = the raw state byte.
 * The other live context (r2, r4, r6, r7, r10) rides through the guest
 * registers, and the handler's results (r5, r8-r12) are likewise left in the
 * guest registers by GUEST_CALL, so nothing needs to be passed back up here. */
static void handle_chip_folder_menu(u32 menu) {
    u32 state = LD8(menu + oFolderMenu_State);

    /* The state byte holds handler_index * 4 (see oFolderMenu_State). */
    switch (state / FOLDER_MENU_TABLE_STRIDE) {
    case 0: folder_menu_state0(state); break;
    case 1: folder_menu_state1(state); break;
    case 2: folder_menu_state2(state); break;
    case 3: folder_menu_state3(state); break;
    case 4: folder_menu_state4(state); break;
    }
}

/* HandleChipFolderMenu8123434(r5 = chip-folder menu work area)
 * -> r5, r8-r12, as written by the selected state handler. */
Next HandleChipFolderMenu8123434(void) {
    handle_chip_folder_menu(R5);
    RETURN_TO_CALLER();
}

/* ── chipFolder_screenInit_812345C ── */
/*
 * chipFolder_screenInit_812345C — one-time setup of the chip folder screen.
 *
 * Everything here is calls into other modules (screen state, graphics,
 * text engine, sprite/window registration), so the whole body is a
 * register-form call chain. The final state of r0-r7, lr and the flags
 * is whatever the last callee (chipFolder_initEntryList) leaves behind; r5/r6 and
 * everything else not set up here is preserved by the callees.
 */

/* register form: calls chipFolder_initVramAndState_81237E0 outside this file, which has no typed C interface yet */
static void call_sub_81237E0(u32 arg) {
    R0 = arg;
    GUEST_CALL(chipFolder_initVramAndState_81237E0);
}

/* register form: calls chipFolder_initGfx_812386C outside this file, which has no typed C interface yet */
static void call_chipFolder_initGfx_812386C(void) {
    GUEST_CALL(chipFolder_initGfx_812386C);
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet */
static void call_sub_8046664(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
}

/* register form: calls battle_spawnEStructRecordsFromTable_80465A0 outside this file, which has no typed C interface yet */
static void call_sub_80465A0_sub_812345C(u32 table) {   /* table: guest address of byte_8123400 */
    R0 = table;
    GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);
}

/* register form: calls refreshChipUseStats_8123A80 outside this file, which has no typed C interface yet */
static void call_sub_8123A80(void) {
    GUEST_CALL(refreshChipUseStats_8123A80);
}

/* register form: calls drawBattleBackground_8123AB8 outside this file, which has no typed C interface yet */
static void call_sub_8123AB8(void) {
    GUEST_CALL(drawBattleBackground_8123AB8);
}

/* register form: calls battleObject_handleMoveAlignment_81352A0 outside this file, which has no typed C interface yet */
static void call_sub_81352A0(u32 dst, u32 src, u32 x, u32 y, u32 param) {
    R0 = dst;
    R1 = src;
    R2 = x;
    R3 = y;
    R4 = param;
    GUEST_CALL(battleObject_handleMoveAlignment_81352A0);
}

/* register form: calls battle_setupPETNavis_8135314 outside this file, which has no typed C interface yet */
static void call_sub_8135314(u32 dst, u32 src, u32 x, u32 y, u32 table) {
    R0 = dst;
    R1 = src;
    R2 = x;
    R3 = y;
    R4 = table;
    GUEST_CALL(battle_setupPETNavis_8135314);
}

/* register form: calls chipFolder_initEntryList outside this file, which has no typed C interface yet */
static void call_sub_8123D24(void) {
    GUEST_CALL(chipFolder_initEntryList);
}

/* Bring up the chip folder screen: reset the screen state, load the folder
 * graphics and text engine, register the two on-screen windows at (0x9D, 0x48),
 * then run the screen's own setup routine. */
static void chip_folder_screen_init(void) {
    call_sub_81237E0(1);                                    /* enter screen state */
    call_chipFolder_initGfx_812386C();                      /* load folder graphics */
    call_sub_8046664();                                     /* reset text engine */
    call_sub_80465A0_sub_812345C(A_byte_8123400);                       /* install text/font table */
    call_sub_8123A80();
    call_sub_8123AB8();
    call_sub_81352A0(A_unk_201D220, A_byte_81233D8, 0x9D, 0x48, 4);
    call_sub_8135314(A_unk_201D220, A_off_81234FC, 0x9D, 0x48, A_byte_8123508);
    call_sub_8123D24();                                     /* screen-specific setup */
}

/* chipFolder_screenInit_812345C() -> r0-r7, lr, nz/c/v all as left by chipFolder_initEntryList */
Next chipFolder_screenInit_812345C(void) {
    chip_folder_screen_init();
    RETURN_TO_CALLER();
}

/* ── chipFolder_init_812349C ── */
/* register form: calls chipFolder_initVramAndState_81237E0 outside this file, which has no typed C interface yet */
static void call_sub_81237E0_sub_812349C(void) {
    R0 = 0;
    GUEST_CALL(chipFolder_initVramAndState_81237E0);
}

/* register form: calls chipFolder_initGfx_812386C outside this file, which has no typed C interface yet */
static void call_chipFolder_initGfx_812386C_sub_812349C(void) {
    GUEST_CALL(chipFolder_initGfx_812386C);
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet */
static void call_sub_8046664_sub_812349C(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
}

/* register form: calls battle_spawnEStructRecordsFromTable_80465A0 outside this file, which has no typed C interface yet */
static void call_sub_80465A0_sub_812349C(u32 a1) {
    R0 = a1;
    GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);
}

/* register form: calls refreshChipUseStats_8123A80 outside this file, which has no typed C interface yet */
static void call_sub_8123A80_sub_812349C(void) {
    GUEST_CALL(refreshChipUseStats_8123A80);
}

/* register form: calls drawBattleBackground_8123AB8 outside this file, which has no typed C interface yet */
static void call_sub_8123AB8_sub_812349C(void) {
    GUEST_CALL(drawBattleBackground_8123AB8);
}

/* register form: calls battleObject_handleMoveAlignment_81352A0 outside this file, which has no typed C interface yet */
static void call_sub_81352A0_sub_812349C(u32 a1, u32 a2, u32 a3, u32 a4, u32 a5) {
    R0 = a1; R1 = a2; R2 = a3; R3 = a4; R4 = a5;
    GUEST_CALL(battleObject_handleMoveAlignment_81352A0);
}

/* register form: calls battle_setupPETNavis_8135314 outside this file, which has no typed C interface yet */
static void call_sub_8135314_sub_812349C(u32 a1, u32 a2, u32 a3, u32 a4, u32 a5) {
    R0 = a1; R1 = a2; R2 = a3; R3 = a4; R4 = a5;
    GUEST_CALL(battle_setupPETNavis_8135314);
}

/* register form: calls chipFolder_initEntryList outside this file, which has no typed C interface yet */
static void call_sub_8123D24_sub_812349C(void) {
    GUEST_CALL(chipFolder_initEntryList);
}

/* Initialize the chip folder graphics: zero a flag, build the GFX, the chip
 * handler, the panel layout (two sub_8135xxx calls using the window-pane data
 * at unk_201D220), and finalize the screen setup. */
static void chip_folder_init_gfx(void) {
    call_sub_81237E0_sub_812349C();
    call_chipFolder_initGfx_812386C_sub_812349C();
    call_sub_8046664_sub_812349C();
    call_sub_80465A0_sub_812349C(A_byte_8123400);
    call_sub_8123A80_sub_812349C();
    call_sub_8123AB8_sub_812349C();
    call_sub_81352A0_sub_812349C(A_unk_201D220, A_byte_81233D8, 0x9D, 0x48, 4);
    call_sub_8135314_sub_812349C(A_unk_201D220, A_off_81234FC, 0x9D, 0x48, A_byte_8123508);
    call_sub_8123D24_sub_812349C();
}

Next chipFolder_init_812349C(void) {
    chip_folder_init_gfx();
    RETURN_TO_CALLER();
}

/* ── battle_updateFrame_8123558 ── */
/* register form: calls battleObject_applyAIDataToAttackTable_8123BC0 outside this file, which has no typed C interface yet */
static void call_8123bc0(void) {
    GUEST_CALL(battleObject_applyAIDataToAttackTable_8123BC0);
}

/* register form: calls battleObject_runAttackTableApply_8123BE0 outside this file, which has no typed C interface yet */
static void call_8123be0(void) {
    GUEST_CALL(battleObject_runAttackTableApply_8123BE0);
}

/* register form: calls battleObject_runAITurnPhase_8123C00 outside this file, which has no typed C interface yet */
static void call_8123c00(void) {
    GUEST_CALL(battleObject_runAITurnPhase_8123C00);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet.
 * Returns r0 and reports whether Z was set (i.e. the beq would be taken). */
static u32 call_is_screen_fade_active_sub_8123558(bool *beq_taken) {
    GUEST_CALL(IsScreenFadeActive);
    *beq_taken = ZERO_FLAG();
    return R0;
}

typedef struct {
    u32 r0;
    u32 nz_source;   /* value for SET_FLAGS_NZ: 0 → Z=1, non-zero → Z=0 */
} BattleUpdateResult;

/* Run three battle-update sub-routines; if the screen fade is NOT active,
 * store 4 into the battle-object state byte.  Returns the final r0 value
 * and the appropriate source for SET_FLAGS_NZ. */
static BattleUpdateResult battle_state_update(u32 battle_obj) {
    call_8123bc0();
    call_8123be0();
    call_8123c00();

    bool beq;
    u32 fade_r0 = call_is_screen_fade_active_sub_8123558(&beq);

    if (beq) {
        /* Z set: skip the store, return r0 and flags from IsScreenFadeActive */
        BattleUpdateResult res = { fade_r0, 0 };
        return res;
    }

    /* Z clear: mov r0, #4 then strb r0, [r5, #2] */
    ST8(battle_obj + 2, 4);
    BattleUpdateResult res = { 4, 4 };
    return res;
}

/* battle_updateFrame_8123558(r5 = battle object ptr) -> r0, nz c v */
Next battle_updateFrame_8123558(void) {
    BattleUpdateResult res = battle_state_update(R5);
    R0 = res.r0;
    SET_FLAGS_NZ(res.nz_source);
    RETURN_TO_CALLER();
}

/* ── sub_8123574 ── */
/* sub_8123574: Initialize/advance some battle object state.
 *
 * Sets the object's mode field to 2, runs a chain of setup helpers, kicks off
 * a fade/sound effect via sub_811B314(1, 3), and then if the screen is not
 * currently fading transitions the object's sub-state to 0xC / 0.
 *
 * Inputs:  r5 = object pointer, r6, r10, lr
 * Outputs: r5 preserved (caller-visible scratch).
 * Clobbers: r0-r3, lr, nz/c/v.
 */
Next sub_8123574(void) {
    /* Mode = 2. */
    ST16(R5 + 0x20, 2);

    /* Chain of setup routines (they read r5 implicitly via R5). */
    GUEST_CALL(battleObject_applyAIDataToAttackTable_8123BC0);
    GUEST_CALL(battleObject_runAttackTableApply_8123BE0);
    GUEST_CALL(battleObject_runAITurnPhase_8123C00);
    GUEST_CALL(sub_8123D70);

    /* sub_811B314(1, 3). */
    R0 = 1;
    R1 = 3;
    GUEST_CALL(sub_811B314);

    /* If a screen fade is already active, leave the sub-state alone. */
    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        ST8(R5 + 2, 0xC);
        ST8(R5 + 3, 0);
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_resetIfFading_8123644 ── */
/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool query_screen_fade(u32 *out_r0) {
    GUEST_CALL(IsScreenFadeActive);
    *out_r0 = R0;
    return ZERO_FLAG();   /* Z set → fade inactive → caller's beq is taken */
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void call_chatbox_reset(void) {
    GUEST_CALL(chatbox_8040818);
}

/* If no screen fade is active, return IsScreenFadeActive's R0 unchanged (beq skips stores).
 * If a fade IS active, reset three status bytes at status_ptr to 0x20/0x00/0x00 and return 0.
 * NZCV flags persist from the last GUEST_CALL; MOV-immediate and STRB do not alter flags. */
static u32 clear_chatbox_during_fade(u32 status_ptr) {
    u32 fade_r0;

    if (query_screen_fade(&fade_r0))
        return fade_r0;           /* beq path: R0 and flags from IsScreenFadeActive */

    call_chatbox_reset();         /* flags from chatbox_8040818 */
    ST8(status_ptr,     0x20);
    ST8(status_ptr + 1, 0x00);
    ST8(status_ptr + 2, 0x00);
    return 0;                     /* mov r0, #0 does NOT set flags */
}

/* chatbox_resetIfFading_8123644(r5 = status_ptr) -> r0, nzcv */
Next chatbox_resetIfFading_8123644(void) {
    R0 = clear_chatbox_during_fade(R5);
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
        R0 = 0x0812368D;            /* off_8123670[1] = petNaviMenuFrame_812368C (Thumb) */
        GUEST_CALL(petNaviMenuFrame_812368C);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8123678 ── */
/* sub_8123678(r5 = object pointer):
 * Runs a check by calling sprite_testFrameParamsMaskC0_81190F8 with argument 0. If it reports
 * success, writes state 4 to the object's byte at offset 3 and calls
 * sub_8123D70 to advance it; otherwise returns immediately.
 * Flags on return are those left by whichever callee ran last. */
Next sub_8123678(void) {
    R0 = 0;
    GUEST_CALL(sprite_testFrameParamsMaskC0_81190F8);   /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = 4;
        ST8(R5 + 3, R0);
        GUEST_CALL(sub_8123D70);
    }
    RETURN_TO_CALLER();
}

/* ── petNaviMenuFrame_812368C ── */
/* Sub-menu struct offsets (r5 points to this structure) */
#define oMenu_State         2
#define oMenu_NaviIndex     0xC
#define oMenu_PetSlot       0x14
#define oMenu_Cursor        0x20

/* PET navi stat byte index for the "send-to-PET" transition flag */
#define STAT_IDX_TRANSITION 0x2D

/* Menu state codes written to oMenu_State */
#define MSTATE_BACK        4
#define MSTATE_TRANSITION  0xC
#define MSTATE_CONFIRMED   0x10

/* Fade parameters for SetScreenFade */
#define FADE_TYPE          0xC
#define FADE_AMOUNT        0x10

/* Sound ID for navi send rejection */
#define SOUND_NAVI_REJECT  0x182u

/* Dialog IDs shown after successful send (upper nibble of navi status vs 2) */
#define DIALOG_NOT_READY   8
#define DIALOG_READY       9

/* Lookup table: mode 0 -> byte-table at 0x140C08, mode 1 -> byte-table at 0xC */
#define A_navi_state_tables  0x08123768u

/* register form: calls IsButtonPressed outside this file, which has no typed C interface yet */
static bool is_button_pressed(u32 button) {
    R0 = button;
    GUEST_CALL(IsButtonPressed);
    return !ZERO_FLAG();
}

/* register form: calls sub_811B314 outside this file, which has no typed C interface yet */
static void call_811B314(u32 a0, u32 a1, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = a0; R1 = a1; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_811B314);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_sub_812368C(u32 sound) {
    R0 = sound;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls sub_8119118 outside this file, which has no typed C interface yet;
 * leaves r0-r2 and nz/c/v for the caller */
static void call_8119118(u32 a0, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = a0; R3 = r3; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8119118);
}

/* register form: calls sub_8123E08 outside this file, which has no typed C interface yet;
 * returns mode in R0 and r2 in *out_r2 */
static u32 get_navi_mode(u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10, u32 *out_r2) {
    R3 = r3; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8123E08);
    if (out_r2) *out_r2 = R2;
    return R0;
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u8 get_pet_stat(u32 pet_slot, u32 stat_idx, u32 r2, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = pet_slot; R1 = stat_idx; R2 = r2; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (u8)R0;
}

/* register form: calls SetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static void set_pet_stat(u32 pet_slot, u32 stat_idx, u32 value,
                         u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = pet_slot; R1 = stat_idx; R2 = value; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(SetCurPETNaviStatsByte);
}

/* register form: calls petNaviTryGiveElementFolder_8120D10 outside this file, which has no typed C interface yet;
 * R4 carries the saved stat as context */
static bool call_8120D10_is_zero(u32 flag, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = flag; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(petNaviTryGiveElementFolder_8120D10);
    return ZERO_FLAG();
}

/* register form: calls battle_getPlayerCurState_8137884 outside this file, which has no typed C interface yet */
static u32 get_navi_status(u32 navi_idx, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = navi_idx; R2 = r2; R3 = r3; R4 = r4; R5 = r5; R6 = r6; R7 = r7;
    GUEST_CALL(battle_getPlayerCurState_8137884);
    return R0;
}

/* register form: calls sub_8123A40 outside this file, which has no typed C interface yet;
 * returns updated r5 in *out_r5 (used for subsequent state write) */
static void call_8123A40(u32 dialog_id, u32 r1_val, u32 r6_val, u32 r7, u32 r10, u32 *out_r5) {
    R0 = dialog_id; R1 = r1_val; R6 = r6_val; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8123A40);
    *out_r5 = R5;
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet */
static void set_screen_fade(u32 a0, u32 a1, u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = a0; R1 = a1; R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(SetScreenFade);
}

/* register form: calls sub_8123D70 outside this file, which has no typed C interface yet;
 * returns r0-r3 and nz/c/v */
static void call_8123D70(u32 r4, u32 r5, u32 r6, u32 r7, u32 r10) {
    R4 = r4; R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(sub_8123D70);
}

/* register form: calls menu_moveCursorUpDown_811FA22 outside this file, which has no typed C interface yet */
static u16 move_menu_cursor(u32 held, u32 step, u32 wrap, u16 cursor, u32 r4_val,
                            u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = held; R1 = step; R2 = wrap; R3 = cursor; R4 = r4_val;
    R5 = r5; R6 = r6; R7 = r7; R10 = r10;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    return (u16)R0;
}

/* Handle one frame of the PET navi sub-menu.
 * menu_ptr (r5) -> sub-menu state; toolkit_ptr (r10) -> toolkit;
 * r3-r7 carried through to callees as preserved arguments. */
static void pet_navi_menu_frame(u32 r2, u32 r3, u32 r4, u32 menu_ptr,
                                u32 r6, u32 r7, u32 toolkit_ptr) {

    /* --- B pressed: go back --- */
    if (is_button_pressed(JOYPAD_B)) {
        ST8(menu_ptr + oMenu_State, MSTATE_BACK);
        call_811B314(1, 0, r4, menu_ptr, r6, r7, toolkit_ptr);
        play_sound_sub_812368C(SOUND_EXIT_SUBMENU);
        call_8119118(0, r3, r4, menu_ptr, r6, r7, toolkit_ptr);
        return;
    }

    /* --- A pressed: confirm selection --- */
    if (is_button_pressed(JOYPAD_A)) {
        play_sound_sub_812368C(SOUND_MENU_SELECT);

        u32 mode = get_navi_mode(r3, r4, menu_ptr, r6, r7, toolkit_ptr, NULL);
        u32 table_addr = LD32(A_navi_state_tables + mode * 4);
        u16 cursor = (u16)LD16(menu_ptr + oMenu_Cursor);
        u8 state = (u8)LD8(table_addr + cursor);
        ST8(menu_ptr + oMenu_State, state);

        if (state != MSTATE_TRANSITION) {
            set_screen_fade(FADE_TYPE, FADE_AMOUNT, r4, menu_ptr, r6, r7, toolkit_ptr);
            call_8123D70(r4, menu_ptr, r6, r7, toolkit_ptr);
            return;
        }

        /* State 0xC: attempt to send navi to PET */
        u32 pet_slot = LD8(menu_ptr + oMenu_PetSlot);
        u32 navi_idx = LD8(menu_ptr + oMenu_NaviIndex);

        u8 saved_stat = get_pet_stat(pet_slot, STAT_IDX_TRANSITION,
                                     r2, r4, menu_ptr, r6, r7, toolkit_ptr);
        set_pet_stat(pet_slot, STAT_IDX_TRANSITION, navi_idx,
                     r4, menu_ptr, r6, r7, toolkit_ptr);

        if (call_8120D10_is_zero(1, saved_stat, menu_ptr, r6, r7, toolkit_ptr)) {
            /* Rejected: back to previous menu */
            ST8(menu_ptr + oMenu_State, MSTATE_BACK);
            call_811B314(1, 0, r4, menu_ptr, r6, r7, toolkit_ptr);
            play_sound_sub_812368C(SOUND_NAVI_REJECT);
            call_8119118(0, r3, r4, menu_ptr, r6, r7, toolkit_ptr);
            return;
        }

        /* Accepted: restore original stat, show status dialog */
        set_pet_stat(pet_slot, STAT_IDX_TRANSITION, saved_stat,
                     r4, menu_ptr, r6, r7, toolkit_ptr);

        u32 status = get_navi_status(navi_idx, r2, r3, r4, menu_ptr, r6, r7) >> 4;
        u32 dialog = (status >= 2) ? DIALOG_READY : DIALOG_NOT_READY;

        u32 updated_r5;
        call_8123A40(dialog, STAT_IDX_TRANSITION, dialog, r7, toolkit_ptr, &updated_r5);
        ST8(updated_r5 + oMenu_State, MSTATE_CONFIRMED);
        call_8119118(0, r3, r4, menu_ptr, r6, r7, toolkit_ptr);
        return;
    }

    /* --- Neither A nor B: move cursor --- */
    u32 cursor_mode = get_navi_mode(r3, 3, menu_ptr, r6, r7, toolkit_ptr, NULL);
    u32 step = (cursor_mode != 0) ? 1u : 3u;

    u32 joypad_ptr = LD32(toolkit_ptr + oToolkit_JoypadPtr);
    u32 held = LD16(joypad_ptr + oJoypad_LowSensitivityHeld);
    u16 cur = (u16)LD16(menu_ptr + oMenu_Cursor);

    u16 new_cursor = move_menu_cursor(held, step, 1, cur, step,
                                      menu_ptr, r6, r7, toolkit_ptr);
    ST16(menu_ptr + oMenu_Cursor, new_cursor);

    call_8123D70(r4, menu_ptr, r6, r7, toolkit_ptr);
}

/* petNaviMenuFrame_812368C: PET navi sub-menu frame handler
 * inputs: r2 r3 r4 r5 r6 r7 r10 lr; preserved: r4 r5 r6 r7 r10 */
Next petNaviMenuFrame_812368C(void) {
    u32 sr4 = R4, sr5 = R5, sr6 = R6, sr7 = R7, sr10 = R10;
    pet_navi_menu_frame(R2, R3, R4, R5, R6, R7, R10);
    R4 = sr4; R5 = sr5; R6 = sr6; R7 = sr7; R10 = sr10;
    RETURN_TO_CALLER();
}

/* ── sub_8123790 ── */
/* sub_8123790: chatbox state-restore triggered while a screen fade is active.
 * inputs:  r5, r10, lr
 * outputs: r5
 * clobbers: nz, c, v, r0, r1, r2, r3, lr
 * preserved: r4, r5, r6, r7
 *
 * When a fade is in progress, advance the chatbox step, then copy the
 * chatbox object at r5 into unk_2037780 (0x80 bytes), notify the
 * subsystem using the slot byte at +0xC from r5, and zero the snapshot
 * buffer (0x80 bytes). */
Next sub_8123790(void) {
    GUEST_CALL(IsScreenFadeActive);
    if (ZERO_FLAG()) {
        RETURN_TO_CALLER();
    }

    GUEST_CALL(chatbox_8040818);

    /* CopyWords(src=r5, dst=unk_2037780, size=0x80). */
    R0 = R5;
    R1 = A_unk_2037780;
    R2 = 0x80;
    GUEST_CALL(CopyWords);

    /* Notify the subsystem using the chatbox slot index byte at r5+0xC. */
    R0 = LD8(R5 + 0xC);
    GUEST_CALL(sub_8049DDC);

    /* ZeroFillByWord(r5, 0x80) — clear the chatbox object itself. */
    R0 = R5;
    R1 = 0x80;
    GUEST_CALL(ZeroFillByWord);

    RETURN_TO_CALLER();
}

/* ── pet_menu_runFrame_81237BC ── */
/* register form: stack-saved lr is needed across the frame's helper calls (original `push {lr}` / `pop {pc}`); no typed C equivalent. */
static u32 pet_menu_frame(void) {
    SP -= 4;
    ST32(SP, LR);

    /* register form: calls sfx_playChipUseSound_8123A50 outside this file, which has no typed C interface yet */
    GUEST_CALL(sfx_playChipUseSound_8123A50);
    /* register form: calls pet_menu_drawCharacterStatBlock_8123C98 outside this file, which has no typed C interface yet */
    GUEST_CALL(pet_menu_drawCharacterStatBlock_8123C98);
    /* register form: calls pet_menu_drawStatusIcons_8123E58 outside this file, which has no typed C interface yet */
    GUEST_CALL(pet_menu_drawStatusIcons_8123E58);
    /* register form: calls entity_runAllUpdateHandlers_80465BC outside this file, which has no typed C interface yet */
    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    /* register form: calls battle_updateEStructSprites_80465F8 outside this file, which has no typed C interface yet */
    GUEST_CALL(battle_updateEStructSprites_80465F8);
    /* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        /* register form: calls pet_menuExitToMap_811F708 outside this file, which has no typed C interface yet */
        GUEST_CALL(pet_menuExitToMap_811F708);
    }

    u32 ret = LD32(SP);
    SP += 4;
    return ret;
}

/* Pet menu's per-frame main loop: tick the menu's own update helpers, run the
 * entity update pass, then either keep going or exit back to the field map once
 * any screen fade has finished. */
Next pet_menu_runFrame_81237BC(void) {
    LR = pet_menu_frame();
    RETURN_TO_CALLER();
}

/* ── chipFolder_initVramAndState_81237E0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2; bool v; } call_sub_8120CC8_result_sub_81237E0;
/* register form: generated wrapper: battleObject_advanceAnimTableBy0xFF_8120CC8 has no typed C interface yet */
static inline call_sub_8120CC8_result_sub_81237E0 call_sub_8120CC8_sub_81237E0(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
    return (call_sub_8120CC8_result_sub_81237E0){ R0, R1, R2, OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r3; } call_GetCurPETNavi_result_sub_81237E0;
/* register form: generated wrapper: GetCurPETNavi has no typed C interface yet */
static inline call_GetCurPETNavi_result_sub_81237E0 call_GetCurPETNavi_sub_81237E0(void) {
    GUEST_CALL(GetCurPETNavi);
    return (call_GetCurPETNavi_result_sub_81237E0){ R0, R3 };
}

typedef struct { u32 r2, r3; } call_zeroFillVRAM_result;
/* register form: generated wrapper: zeroFillVRAM has no typed C interface yet */
static inline call_zeroFillVRAM_result call_zeroFillVRAM_sub_81237E0(void) {
    GUEST_CALL(zeroFillVRAM);
    return (call_zeroFillVRAM_result){ R2, R3 };
}

/* register form: generated wrapper: ZeroFill_byte_3001960 has no typed C interface yet */
static inline void call_ZeroFill_byte_3001960(void) {
    GUEST_CALL(ZeroFill_byte_3001960);
}

typedef struct { u32 r0, r3; } call_GetCurPETNaviStatsByte_result;
/* register form: generated wrapper: GetCurPETNaviStatsByte has no typed C interface yet */
static inline call_GetCurPETNaviStatsByte_result call_GetCurPETNaviStatsByte(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (call_GetCurPETNaviStatsByte_result){ R0, R3 };
}

/* register form: generated wrapper: ZeroFillGFX30025c0 has no typed C interface yet */
static inline void call_ZeroFillGFX30025c0(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
}

typedef struct { u32 r2; } call_toolkit_applyOverlayPalette_80015FC_result;
/* register form: generated wrapper: toolkit_applyOverlayPalette_80015FC has no typed C interface yet */
static inline call_toolkit_applyOverlayPalette_80015FC_result call_toolkit_applyOverlayPalette_80015FC(u32 r0) {
    R0 = r0;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);
    return (call_toolkit_applyOverlayPalette_80015FC_result){ R2 };
}

typedef struct { u32 r5; } call_SetScreenFade_result;
/* register form: generated wrapper: SetScreenFade has no typed C interface yet */
static inline call_SetScreenFade_result call_SetScreenFade_sub_81237E0(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetScreenFade);
    return (call_SetScreenFade_result){ R5 };
}

/* Initialise the battle-object state at `obj`.  When `flag` is non-zero
 * (first entry into the battle), the video memory, the 0x3001960 byte
 * buffer and the navi HP snapshot are also wiped. */
static void init_battle_object(u32 flag, u32 battle_state_ptr,
                               u32 obj, u32 toolkit) {
    bool clear_mem = flag != 0;

    /* Zero every timer / sub-state halfword in the object. */
    ST16(obj + 0x20, 0);
    ST16(obj + 0x22, 0);
    ST16(obj + 0x24, 0);
    ST16(obj + 0x26, 0);
    ST16(obj + 0x2A, 0);
    ST16(obj + 0x2C, 0);
    ST16(obj + 0x2E, 0);
    ST16(obj + 0x30, 0);
    ST16(obj + 0x34, 0);
    ST16(obj + 0x36, 0);
    ST16(obj + 0x38, 0);
    ST16(obj + 0x3A, 0);

    /* Save the battle-state pointer and zero the remaining small fields. */
    ST32(obj + 0x4C, battle_state_ptr);
    ST8(obj + 0x10, 0);
    ST8(obj + 0x11, 0);
    ST16(obj + 0x18, 0);

    /* Encode the navi sprite-index byte from the battle-graphics lookup. */
    call_sub_8120CC8_result_sub_81237E0 lookup = call_sub_8120CC8_sub_81237E0(0, A_dword_81233D4);
    ST8(obj + 0x12, (u8)(lookup.r0 | (lookup.r1 << 4)));

    /* Record the currently-selected PET navi id. */
    call_GetCurPETNavi_result_sub_81237E0 navi = call_GetCurPETNavi_sub_81237E0();
    ST8(obj + 0x14, (u8)navi.r0);

    if (clear_mem) {
        /* First entry: wipe VRAM, the byte scratch buffer, and the HP byte. */
        call_zeroFillVRAM_sub_81237E0();
        call_ZeroFill_byte_3001960();
        call_GetCurPETNaviStatsByte_result stats =
            call_GetCurPETNaviStatsByte(LD8(obj + 0x14), 0x2D);   /* r1 = 0x2D: the stat index */
        ST8(obj + 0x0C, (u8)stats.r0);
    }

    /* Clear the graphics buffer and re-apply the overlay palette. */
    call_ZeroFillGFX30025c0();
    call_toolkit_applyOverlayPalette_80015FC(0x10);

    /* Configure the render-info block: BG mode and zero the scanline fields. */
    u32 render = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render + 0x00, 0x1F40);
    ST16(render + 0x10, 0);
    ST16(render + 0x12, 0);
    ST16(render + 0x14, 0);
    ST16(render + 0x16, 0);
    ST16(render + 0x18, 0);
    ST16(render + 0x1A, 0);

    /* Enter the active battle state and begin the 8-frame fade. */
    ST8(obj + 0x01, 4);
    call_SetScreenFade_sub_81237E0(8, 0x10);
    ST8(obj + 0x02, 0);
}

/* chipFolder_initVramAndState_81237E0 – set up a battle-object.
 * r0 = first-init flag, r1 = battle-state ptr, r5 = object, r10 = toolkit.
 * Preserves r4, r5; sets r7 = toolkit. */
Next chipFolder_initVramAndState_81237E0(void) {
    init_battle_object(R0, R1, R5, R10);
    R7 = R10;
    RETURN_TO_CALLER();
}

/* ── chipFolder_initGfx_812386C ── */
/* Register form: calls getKeyedTableEntry_811FB84 outside this file, which has no typed C interface yet */
static u32 resolve_decomp_pointer(u32 source, u32 size) {
    R0 = source;
    R1 = size;
    GUEST_CALL(getKeyedTableEntry_811FB84);
    return R0;
}

/* Register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decompress_copy_data(u32 table) {
    R0 = table;
    GUEST_CALL(decompAndCopyData);
}

/* Register form: calls battle_loadTileGridByNavi_8123300 outside this file, which has no typed C interface yet */
static void load_text_script_block(u32 table, u32 dest, u32 buffer) {
    R0 = table;
    R1 = dest;
    R2 = buffer;
    GUEST_CALL(battle_loadTileGridByNavi_8123300);
}

/* Register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_background_tiles(u32 j, u32 i, u32 block, u32 tile_ids, u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = block;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* Initialize the chip-folder PET screen: build a decompression descriptor table from a
 * ROM source table, decompress tile/sprite/palette graphics, load folder-page text
 * scripts, and copy background grid tiles into tile block 1. */
static void chipFolder_initGfx(void) {
    /* register form: save/restore caller registers preserved across GUEST_CALLs (original's push/pop of r4-r7) */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    u32 out_base = A_unk_20096E0;  /* scratch RAM for the descriptor table */
    u32 tbl      = A_off_812391C;  /* ROM source table: {src, size, arg1, arg2} rows */
    u32 out_off  = 0;
    u32 in_off   = 0;

    /* Resolve each compressed pointer and copy the two trailing args into a flat
     * {resolved_ptr, arg1, arg2} triple per row (4 rows × 16 bytes = 0x40). */
    for (int row = 0; row < 4; row++) {
        u32 src = LD32(tbl + in_off);  in_off += 4;
        u32 sz  = LD32(tbl + in_off);  in_off += 4;

        ST32(out_base + out_off, resolve_decomp_pointer(src, sz));  out_off += 4;

        ST32(out_base + out_off, LD32(tbl + in_off));  out_off += 4;  in_off += 4;
        ST32(out_base + out_off, LD32(tbl + in_off));  out_off += 4;  in_off += 4;
    }
    ST32(out_base + out_off, 0);  /* null terminator for the descriptor list */

    /* Decompress chip-folder GFX using the built table and a second static table */
    decompress_copy_data(out_base);
    decompress_copy_data(A_dword_8123964);

    /* Load compressed text scripts for the folder page labels */
    load_text_script_block(A_off_81238E4, A_eTextScript201C220, A_eDecompBuffer2013A00);

    /* Copy background tile data for the folder grid (two regions in tile block 1) */
    copy_background_tiles(0, 0, 1, A_eTextScript201BA20, 0x1E, 0x14);
    copy_background_tiles(9, 6, 1, A_eTextScript201BF20, 0x14, 0xD);

    /* register form: restore preserved caller registers (original's pop {r4-r7,pc}) */
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
}

/* chipFolder_initGfx_812386C -> r2/r3 from last CopyBackgroundTiles, r4-r7 preserved */
Next chipFolder_initGfx_812386C(void) {
    chipFolder_initGfx();
    RETURN_TO_CALLER();
}

/* ── sfx_playChipUseSound_8123A50 ── */
/* Values that the original loaded from the literal pool at 0x08123A6C..0x08123A7C. */
#define SUB_811FA98_ARG0 0x00DA003Du
#define SUB_811FA98_ARG1 0x00020002u
#define SUB_811FA98_ARG2 0x1Eu
#define SUB_811FA98_ARG4 5u
#define SUB_811FA98_ARG6 0x0C24Cu
#define SUB_811FA98_ARG7 0x46u
/* Lookup table at 0x08123A7C: word 0x382E24, used as a base for one byte. */
#define SUB_8123A50_TABLE 0x08123A7Cu

/* Play the "battle chip use" entry sound for the chip identified by the
 * object at oBattleObject_Alliance. Reads one byte of state, indexes a small
 * ROM table with it to get a halfword offset, pulls a 16-bit sample id from
 * the chip record at that offset, and hands it (with the rest of the args
 * loaded from the literal pool) to the sound driver object_computeAndQueueOamEntry_811FA98. */
static void play_chip_use_sound(u32 r5)
{
    u8 state_byte = LD8(r5 + 0xC);
    u8 offset_byte = LD8(SUB_8123A50_TABLE + state_byte);
    u16 sample_id = LD16(r5 + offset_byte);

    /* register form: calls object_computeAndQueueOamEntry_811FA98 outside this file, which has no typed C interface yet */
    R0 = SUB_811FA98_ARG0;
    R1 = SUB_811FA98_ARG1;
    R2 = SUB_811FA98_ARG2;
    R3 = sample_id;
    R4 = SUB_811FA98_ARG4;
    R5 = r5;
    R6 = SUB_811FA98_ARG6;
    R7 = SUB_811FA98_ARG7;
    GUEST_CALL(object_computeAndQueueOamEntry_811FA98);
}

Next sfx_playChipUseSound_8123A50(void) {
    /* The contract requires r4..r7 to be preserved across the call.
     * Restore them from the input values after object_computeAndQueueOamEntry_811FA98 has run. */
    u32 r4_in = R4, r5_in = R5, r6_in = R6, r7_in = R7;

    play_chip_use_sound(R5);

    R4 = r4_in;
    R5 = r5_in;
    R6 = r6_in;
    R7 = r7_in;
    RETURN_TO_CALLER();
}

/* ── refreshChipUseStats_8123A80 ── */
/* Game state pointers used by refreshChipUseStats_8123A80. */
#define gBattleFieldState A_unk_201D220
#define gBattleOrderList   A_unk_20343E0

/* register form: calls battle_buildChipPackFromFolderEntries_811FCB8 outside this file, which has no typed C interface yet */
static void call_sub_811FCB8(u32 slot, u32 field_state, u32 one, u32 order_list) {
    R0 = slot;
    R1 = field_state;
    R2 = one;
    R3 = order_list;
    GUEST_CALL(battle_buildChipPackFromFolderEntries_811FCB8);
}

/* register form: calls GetCurPETNaviStatsByte outside this file, which has no typed C interface yet */
static u8 navi_stats_byte(u32 navi_id, u32 stat_index) {
    R0 = navi_id;
    R1 = stat_index;
    GUEST_CALL(GetCurPETNaviStatsByte);
    return (u8)R0;
}

/* Refresh a chip-use record: stamp the battle field/order list with the current
 * PET navi's slot id, then store the sum of the Atk-byte and Next-byte stats
 * for that navi into the +0x64 field of the chip-use struct passed in r5. */
static void refresh_chip_stats(u32 me_addr) {
    u8 slot = LD8(me_addr + 0xC);
    call_sub_811FCB8(slot, gBattleFieldState, 1, gBattleOrderList);

    u32 stat_index = (u32)slot * 2u + 0x56u;
    u8 navi_id = LD8(me_addr + 0x14);
    u8 atk  = navi_stats_byte(navi_id, stat_index);
    u8 next = navi_stats_byte(navi_id, stat_index + 1u);
    ST32(me_addr + 0x64, (u32)atk + (u32)next);
}

Next refreshChipUseStats_8123A80(void) {
    refresh_chip_stats(R5);
    RETURN_TO_CALLER();
}

/* ── drawBattleBackground_8123AB8 ── */
/* The tileset ID for the current field: byte at obj+0xC indexes a ROM table
 * of u16 offsets; the u16 at (obj + offset) is the tileset number. */
static u32 get_tileset_id(u32 battle_obj) {
    u32 index  = LD8(battle_obj + 0xC);
    u32 offset = LD8(A_dword_8123BBC + index);
    return LD16(battle_obj + offset);
}

/* register form: callee needs r5,r7 as inputs and may clobber r4,r6;
 * the original saves r4-r7 via push/pop which C cannot express here */
static void call_prep_wide(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D6E0;
    R3 = 0xB;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(battle_drawChipSelectFighterNames_81200EC);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r4,r5,r7 as inputs; saves r4-r7 around GUEST_CALL */
static void call_prep_narrow(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D7C0;
    R3 = A_byte_8123394;
    R4 = 0x6007200;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(battle_loadFolderChipGfx_8120390);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r4,r5,r7 as inputs; saves r4-r7 around GUEST_CALL */
static void call_prep_left_panel(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D7F8;
    R3 = (0xAu << 12) | 0x35Cu;
    R4 = 0x6006B80;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(battle_buildChipFolderOamPack_81203E4);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r5,r7 as inputs; saves r4-r7 around GUEST_CALL */
static void call_prep_mid_strip(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D830;
    R3 = (0xBu << 12) | 0x216u;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(battle_loadChipIconSlots_8120458);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r5,r7 as inputs; saves r4-r7 around GUEST_CALL */
static void call_prep_right_edge(u32 tileset_id, u32 battle_obj) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = tileset_id;
    R1 = A_unk_201D220;
    R2 = A_unk_201D84C;
    R3 = (9u << 12) | 0x34Fu;
    R5 = 5;
    R7 = battle_obj;
    GUEST_CALL(battle_fillChipPackEntries_81204C4);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* register form: callee needs r4,r5 as inputs; saves r4-r7 around GUEST_CALL */
static void call_copy_tiles(u32 j, u32 i, u32 tile_ids, u32 j_size, u32 i_size) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = j;
    R1 = i;
    R2 = 2;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* Build the five battle-background tile regions for the current field.
 * Each region: read tileset ID → call a prep routine (decompresses tile data
 * into a staging buffer) → call CopyBackgroundTiles to blit into tile block 2. */
static void draw_battle_background(u32 battle_obj) {
    call_prep_wide(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0xE, 8, A_unk_201D6E0, 8, 0xA);

    call_prep_narrow(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0xC, 8, A_unk_201D7C0, 2, 0xA);

    call_prep_left_panel(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0x16, 8, A_unk_201D7F8, 2, 0xA);

    call_prep_mid_strip(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0x18, 8, A_unk_201D830, 1, 0xA);

    call_prep_right_edge(get_tileset_id(battle_obj), battle_obj);
    call_copy_tiles(0x19, 8, A_unk_201D84C, 2, 0xA);
}

/* drawBattleBackground_8123AB8(r5 = battle_object_ptr) — preserves r4–r7, c/v from last callee */
Next drawBattleBackground_8123AB8(void) {
    draw_battle_background(R5);
    RETURN_TO_CALLER();
}

/* ── battleObject_applyAIDataToAttackTable_8123BC0 ── */
/* battleObject_applyAIDataToAttackTable_8123BC0:
 * Passes a fixed argument set to battleObject_applyAIDataToAttackTable_8123C20 and returns its result untouched.
 * Arguments are all constants: the word table at dword_8123BDC, the byte
 * table at byte_81233D8, 1 in r1, 4 in r3 and a length of 0x1C in r4.
 * Like the original, this wrapper preserves r4-r7 across the call (it only
 * sets r4 as an argument, then restores the caller's value afterwards). */
Next battleObject_applyAIDataToAttackTable_8123BC0(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    R0 = A_dword_8123BDC; /* pointer to the word table (0x302F2E) */
    R1 = 1;
    R2 = A_byte_81233D8;  /* pointer to the byte table */
    R3 = 4;
    R4 = 0x1C;            /* entry count */
    GUEST_CALL(battleObject_applyAIDataToAttackTable_8123C20);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── battleObject_runAttackTableApply_8123BE0 ── */
/* battleObject_runAttackTableApply_8123BE0: thin wrapper around battleObject_applyAIDataToAttackTable_8123C20 with a fixed argument set.
 * r0 points at the 3-byte constant in the literal pool (dword_8123BFC =
 * 0x5A5856), r2 at the byte_81233E0 work area; the remaining arguments
 * (0x20, 5, 0x5C, 0) select its mode/geometry. The callee's r4/r6
 * arguments are set locally — the caller's r4-r7 are saved around the
 * call and restored on return. */
Next battleObject_runAttackTableApply_8123BE0(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;

    R0 = A_dword_8123BFC; /* source data: the three bytes 0x5A, 0x58, 0x56 */
    R1 = 0x20;
    R2 = A_byte_81233E0;
    R3 = 5;
    R4 = 0x5C;
    R6 = 0;
    GUEST_CALL(battleObject_applyAIDataToAttackTable_8123C20);

    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── battleObject_runAITurnPhase_8123C00 ── */
/* battleObject_runAITurnPhase_8123C00: prepare a fixed set of arguments and forward them to battleObject_applyAIDataToAttackTable_8123C20.
 * The color/palette table at dword_8123C1C (0x5B5957 = three BGR555 entries) and
 * the work buffer byte_81233E8 are passed by address; the remaining arguments
 * (0x40, 6, 0x60, -, 1) are constants. r4-r7 are restored to their entry values
 * on return, matching the original's push/pop around the call. */
Next battleObject_runAITurnPhase_8123C00(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R0 = A_dword_8123C1C;   /* color data */
    R1 = 0x40;
    R2 = A_byte_81233E8;    /* destination buffer */
    R3 = 6;
    R4 = 0x60;
    R6 = 1;

    GUEST_CALL(battleObject_applyAIDataToAttackTable_8123C20);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8123D54 ── */
/* sub_8123D54(r0 = selection): call the string/label printer at battle_spawnEStructRecordsFromTable_80465A0
 * with one of two fixed tables — byte_81233F0 when r0 is 0, byte_81233F8
 * otherwise. R1 emerges holding the original r0 (the callee leaves it alone),
 * and the callee's flag results pass through to the caller. */
Next sub_8123D54(void) {
    u32 which = R0;

    R1 = which;
    R0 = (which != 0) ? A_byte_81233F8 : A_byte_81233F0;

    GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);

    RETURN_TO_CALLER();
}

/* ── sub_8123D98 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5, r6, r7; } call_sub_811FAF4_result;
/* register form: generated wrapper: battleObject_queueOamFromTableRow_811FAF4 has no typed C interface yet */
static inline call_sub_811FAF4_result call_sub_811FAF4(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(battleObject_queueOamFromTableRow_811FAF4);
    return (call_sub_811FAF4_result){ R0, R4, R5, R6, R7 };
}

typedef struct { u32 r0, r1; bool v; } call_sub_8120CC8_result;
/* register form: generated wrapper: battleObject_advanceAnimTableBy0xFF_8120CC8 has no typed C interface yet */
static inline call_sub_8120CC8_result call_sub_8120CC8_sub_8123D98(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
    return (call_sub_8120CC8_result){ R0, R1, OVERFLOW_FLAG() };
}

/* sub_8123D98 - refresh one battle-object spawn slot.
 *
 * Register roles on entry:
 *   r0 = page      : high 16 bits of the descriptor word for battleObject_queueOamFromTableRow_811FAF4
 *   r1 = offset    : low 16 bits base of that descriptor
 *   r2 = table_idx : index into a u16 weight table kept inside the object
 *                    array; 0 means "no table shift"
 *   r3 = slot_off  : offset of the slot byte within the object array (r5)
 *   r4 = fwd_r3    : forwarded as r3 to battleObject_queueOamFromTableRow_811FAF4
 *   r5 = base      : object array pointer
 *   r6 = fwd_r4    : forwarded as r4 to battleObject_queueOamFromTableRow_811FAF4
 *
 * The slot byte is laid out as (tier << 4) | kind. The kind nibble is
 * produced by battleObject_queueOamFromTableRow_811FAF4 (with an immediate battleObject_advanceAnimTableBy0xFF_8120CC8 refill stored into
 * the slot when the old tier was 1), the tier nibble is the old tier minus
 * one. The original preserves r4 with push/pop, so this helper returns it
 * for the adapter to restore - the battleObject_queueOamFromTableRow_811FAF4 wrapper leaves its own
 * argument in the machine R4. */
static u32 refresh_spawn_slot(u32 page, u32 offset, u32 table_idx, u32 slot_off,
                              u32 fwd_r3, u32 base, u32 fwd_r4) {
    /* Descriptor = (page << 16) | (offset + 16 * weight_table[table_idx]),
       where the weight entry lives in the object array itself and counts
       as 0 when table_idx is 0. */
    u32 stride = (table_idx != 0u) ? (u32)LD16(base + table_idx) : 0u;
    u32 desc = (page << 16) | (offset + stride * 0x10u);

    /* Current slot byte: low nibble picks the object, high nibble is the tier. */
    u32 slot_byte = LD8(base + slot_off);
    u32 tier = slot_byte >> 4;

    call_sub_811FAF4_result lookup =
        call_sub_811FAF4(desc, 0x20005u, slot_byte & 0xFu, fwd_r3, fwd_r4);

    u32 new_tier;
    if (tier == 1u) {
        /* Tier 1 is exhausted: ask battleObject_advanceAnimTableBy0xFF_8120CC8 for the follow-up entry and
           store it into the slot right away. r1 afterwards is whatever
           battleObject_advanceAnimTableBy0xFF_8120CC8 leaves there, and feeds the tier field below. */
        call_sub_8120CC8_result refill =
            call_sub_8120CC8_sub_8123D98(lookup.r0, A_dword_81233D4);
        ST8(base + slot_off, refill.r0);
        new_tier = refill.r1 << 4;
    } else {
        new_tier = (tier - 1u) << 4;
    }

    /* Re-read the (possibly refilled) slot byte and put the new tier nibble
       on top of its low nibble. */
    u32 merged = (LD8(base + slot_off) & 0xFu) | new_tier;
    ST8(base + slot_off, merged);

    return fwd_r3; /* the caller's r4, passed through unchanged */
}

/* sub_8123D98(r0 page, r1 offset, r2 table_idx, r3 slot_off,
 *             r4 fwd_r3, r5 base, r6 fwd_r4) -> no outputs
 *
 * The battleObject_queueOamFromTableRow_811FAF4 wrapper overwrites the machine R4 with its fifth argument,
 * but this function must preserve the caller's r4 (the original's pop {r4}).
 * So R4 is read as an argument before the call and written back afterwards
 * with the value the typed function returns. */
Next sub_8123D98(void) {
    R4 = refresh_spawn_slot(R0, R1, R2, R3, R4, R5, R6);
    RETURN_TO_CALLER();
}

/* ── sub_8123E08 ── */
/* sub_8123E08(r5 = state pointer):
 * Checks bit 0x20 of the status byte unk_20018EC[*(u8*)(r5 + 0xC)].
 * Returns r0 = r2 = 1 if the bit is set, 0 otherwise.
 * Flags are left from the AND with 0x20 (NZ), as callers may branch on them. */
Next sub_8123E08(void) {
    u32 index = LD8(R5 + 0xC);
    u32 bit = LD8(A_unk_20018EC + index) & 0x20;

    SET_FLAGS_NZ(bit);

    u32 isSet = (bit != 0);
    R0 = isSet;
    R2 = isSet;
    RETURN_TO_CALLER();
}

/* ── submenu_subchip_dispatch_812407C ── */
/* submenu_subchip_dispatch_812407C(r5 = sub-chip submenu state):
 * Dispatch on the submenu's phase (/4 value in [state,#2]) and, once the
 * phase handler returns, update entity handlers, scene strips and background.
 * As in the original, the phase handler is entered with r0 = its jump-table
 * entry (Thumb address) and r1 = the phase word. */
Next submenu_subchip_dispatch_812407C(void) {
    u32 phase = LD8(R5 + 2);

    R1 = phase;
    switch (phase >> 2) {
    case 0: R0 = 0x081240BD; GUEST_CALL(submenu_subchip_81240BC); break;
    case 1: R0 = 0x081240D1; GUEST_CALL(submenu_subchip_81240D0); break;
    case 2: R0 = 0x08124135; GUEST_CALL(submenu_subchip_8124134); break;
    case 3: R0 = 0x08124145; GUEST_CALL(submenu_subchip_dispatch_8124144); break;
    case 4: R0 = 0x081242D9; GUEST_CALL(submenu_subchip_81242D8); break;
    case 5: R0 = 0x08124309; GUEST_CALL(submenu_subchip_8124308); break;
    case 6: R0 = 0x08124341; GUEST_CALL(submenu_subchip_8124340); break;
    }

    GUEST_CALL(entity_runAllUpdateHandlers_80465BC);
    GUEST_CALL(battle_updateEStructSprites_80465F8);
    GUEST_CALL(sub_812453C);
    GUEST_CALL(background_updateSceneStrips_812475C);

    RETURN_TO_CALLER();
}

/* ── submenu_subchip_81240BC ── */
/* register form: calls submenu_subchip_tickOamAndAdvanceAnimTable_81244EC outside this file, which has no typed C interface yet */
static void call_submenu_update(void) {
    GUEST_CALL(submenu_subchip_tickOamAndAdvanceAnimTable_81244EC);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static bool call_screen_fade_active(u32 *r0_out) {
    GUEST_CALL(IsScreenFadeActive);
    *r0_out = R0;
    return ZERO_FLAG();
}

struct SubResult {
    u32 r0;       /* final R0 for the caller */
    u32 nz_seed;  /* value for SET_FLAGS_NZ to reproduce N,Z */
};

/* Advance the subchip submenu one frame.
 * If the screen fade is active, return IsScreenFadeActive's r0 (Z=1 ⇒ N=0).
 * Otherwise, mark the sub-menu ready (byte 4 at UnkSubMenu+2) and return 4. */
static struct SubResult subchip_submenu_tick(u32 state) {
    call_submenu_update();

    u32 fade_r0;
    if (call_screen_fade_active(&fade_r0)) {
        struct SubResult r = { fade_r0, 0 };
        return r;
    }

    ST8(state + 2, 4);
    struct SubResult r = { 4, 4 };
    return r;
}

/* submenu_subchip_81240BC(r5 = UnkSubMenu* state) -> r0, nz c v */
Next submenu_subchip_81240BC(void) {
    struct SubResult res = subchip_submenu_tick(R5);
    R0 = res.r0;
    SET_FLAGS_NZ(res.nz_seed);
    RETURN_TO_CALLER();
}

/* ── submenu_subchip_81240D0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; bool z, n; } call_IsButtonPressed_result_submenu_subchip_81240D0;
/* register form: generated wrapper: IsButtonPressed has no typed C interface yet */
static inline call_IsButtonPressed_result_submenu_subchip_81240D0 call_IsButtonPressed_submenu_subchip_81240D0(u32 r0) {
    R0 = r0;
    GUEST_CALL(IsButtonPressed);
    return (call_IsButtonPressed_result_submenu_subchip_81240D0){ R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r4, r5, r6, r7, r10; } call_submenu_ConfirmExitAndFade_811FB64_result_submenu_subchip_81240D0;
/* register form: generated wrapper: submenu_ConfirmExitAndFade_811FB64 has no typed C interface yet */
static inline call_submenu_ConfirmExitAndFade_811FB64_result_submenu_subchip_81240D0 call_submenu_ConfirmExitAndFade_811FB64_submenu_subchip_81240D0(u32 r0, u32 r2, u32 r5) {
    R0 = r0;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(submenu_ConfirmExitAndFade_811FB64);
    return (call_submenu_ConfirmExitAndFade_811FB64_result_submenu_subchip_81240D0){ R2, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; bool c, v; } call_PlaySoundEffect_result_submenu_subchip_81240D0;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_submenu_subchip_81240D0 call_PlaySoundEffect_submenu_subchip_81240D0(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_submenu_subchip_81240D0){ R2, R3, R4, R5, R6, R7, R10, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r5, r8, r9, r10, r11, r12; } call_sub_81245D4_result;
/* register form: generated wrapper: sub_81245D4 has no typed C interface yet */
static inline call_sub_81245D4_result call_sub_81245D4(u32 r4, u32 r5) {
    R4 = r4;
    R5 = r5;
    GUEST_CALL(sub_81245D4);
    return (call_sub_81245D4_result){ R2, R5, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r0, r2, r3, r4, r5, r6, r7, r10; } call_menu_moveCursorUpDown_811FA22_result_submenu_subchip_81240D0;
/* register form: generated wrapper: menu_moveCursorUpDown_811FA22 has no typed C interface yet */
static inline call_menu_moveCursorUpDown_811FA22_result_submenu_subchip_81240D0 call_menu_moveCursorUpDown_811FA22_submenu_subchip_81240D0(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(menu_moveCursorUpDown_811FA22);
    return (call_menu_moveCursorUpDown_811FA22_result_submenu_subchip_81240D0){ R0, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r5; } call_sub_81244EC_result;
/* register form: generated wrapper: submenu_subchip_tickOamAndAdvanceAnimTable_81244EC has no typed C interface yet */
static inline call_sub_81244EC_result call_sub_81244EC(u32 r5) {
    R5 = r5;
    GUEST_CALL(submenu_subchip_tickOamAndAdvanceAnimTable_81244EC);
    return (call_sub_81244EC_result){ R5 };
}

/* Sub-chip submenu handler.
 * state (r5): sub-chip submenu state struct.
 *   +0x0C: u8  option_count  (number of sub-chip options; 0 = none)
 *   +0x14: u16 cursor_index  (currently highlighted option)
 * toolkit (r10): game toolkit pointer.
 *
 * Handles Start/B (exit), A (jack-in), and Up/Down (cursor movement). */
static void submenu_subchip_handle(u32 state, u32 r2_in, u32 r4_in,
                                   u32 r6_in, u32 r7_in, u32 toolkit) {
    /* Start or B → confirm exit and fade out */
    if (!call_IsButtonPressed_submenu_subchip_81240D0(JOYPAD_START | JOYPAD_B).z) {
        call_submenu_ConfirmExitAndFade_811FB64_result_submenu_subchip_81240D0 fade =
            call_submenu_ConfirmExitAndFade_811FB64_submenu_subchip_81240D0(8, r2_in, state);
        call_sub_81244EC(fade.r5);
        return;
    }

    /* A pressed and this option slot exists → attempt jack-in */
    if (LD8(state + 0xC) != 0 && !call_IsButtonPressed_submenu_subchip_81240D0(JOYPAD_A).z) {
        u16 cursor = LD16(state + 0x14);
        /* unk_201C400[cursor*2 + 1]: non-zero means the slot has a sub-chip */
        if (LD8(A_unk_201C400 + (u32)cursor * 2 + 1) == 0) {
            /* Empty slot – play "can't jack in" sound */
            call_PlaySoundEffect_result_submenu_subchip_81240D0 snd =
                call_PlaySoundEffect_submenu_subchip_81240D0(SOUND_CANT_JACK_IN, r2_in);
            call_sub_81244EC(snd.r5);
            return;
        }
        /* Filled slot – execute jack-in routine and play select sound */
        call_sub_81245D4_result s5d4 = call_sub_81245D4(r4_in, state);
        call_PlaySoundEffect_result_submenu_subchip_81240D0 snd =
            call_PlaySoundEffect_submenu_subchip_81240D0(SOUND_MENU_SELECT, s5d4.r2);
        call_sub_81244EC(snd.r5);
        return;
    }

    /* Up/Down cursor movement: needs joypad state and more than 1 option */
    u32 joypad = LD32(toolkit + oToolkit_JoypadPtr);
    u16 held   = LD16(joypad + oJoypad_LowSensitivityHeld);
    u32 count  = LD8(state + 0xC);

    if (count > 1) {
        u16 current = LD16(state + 0x14);
        call_menu_moveCursorUpDown_811FA22_result_submenu_subchip_81240D0 mv =
            call_menu_moveCursorUpDown_811FA22_submenu_subchip_81240D0(
                held, count, 1, current,
                r4_in, state, r6_in, r7_in);
        ST16(mv.r5 + 0x14, mv.r0);
        call_sub_81244EC(mv.r5);
    } else {
        call_sub_81244EC(state);
    }
}

/* submenu_subchip_81240D0(r2, r4, r5, r6, r7, r10) → r5 r8 r9 r10 r11 r12 */
Next submenu_subchip_81240D0(void) {
    submenu_subchip_handle(R5, R2, R4, R6, R7, R10);
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

/* ── submenu_subchip_dispatch_8124144 ── */
/* submenu_subchip_dispatch_8124144:
 * Per-frame handler of the sub-chip folder submenu (struct UnkSubMenu in r5).
 * If the menu is not flagged active (+0x11 bit 0), it first yields to the
 * chatbox: while a chatbox message holds the screen (flag 0x80) nothing
 * happens; while it is transitioning (flag 0x10) or the chatbox state
 * machine is still busy, the submenu is forced to state 4 (its "wait"
 * state). Otherwise it runs the per-mode step function selected by
 * state->mode (+3). The mode byte holds a byte offset into jt_8124188
 * (0, 4, 8, ...), not a small index. The handlers are called with
 * r0 = jt_8124188 and r1 = the mode byte, alongside the state in r5. */
Next submenu_subchip_dispatch_8124144(void) {
    u32 state = R5;

    if (!(LD8(state + 0x11) & 1)) {
        /* Chatbox on screen at all? */
        R0 = 0x80;
        GUEST_CALL(chatbox_mask_eFlags2009F38);
        if (R0 == 0)
            RETURN_TO_CALLER();

        /* Chatbox transitioning? Put the submenu on hold. */
        R0 = 0x10;
        GUEST_CALL(chatbox_mask_eFlags2009F38);
        if (R0 != 0) {
            ST8(state + 2, 4);
            RETURN_TO_CALLER();
        }

        /* Chatbox waiting for input? */
        R0 = 0x20;
        GUEST_CALL(chatbox_mask_eFlags2009F38);
        if (R0 == 0)
            RETURN_TO_CALLER();

        /* Chatbox state machine still running? Hold the submenu. */
        GUEST_CALL(chatbox_GetState_8045F4C);
        if (R0 != 0) {
            ST8(state + 2, 4);
            RETURN_TO_CALLER();
        }
    }

    /* Run this frame's step for the current submenu mode. The mode byte
     * is an offset into the jump table, so the cases are 0, 4, 8, ... */
    R0 = A_jt_8124188;
    R1 = LD8(state + 3);
    switch (R1) {
    case 0:
    case 4:
        GUEST_CALL(submenu_subchip_81241A0);
        break;
    case 8:
        GUEST_CALL(submenu_subchip_8124230);
        break;
    case 0xC:
        GUEST_CALL(submenu_subchip_812425C);
        break;
    case 0x10:
        GUEST_CALL(submenu_subchip_812427C);
        break;
    case 0x14:
        GUEST_CALL(submenu_subchip_81242AC);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── submenu_subchip_81242D8 ── */
/* submenu_subchip_81242D8: handles the sub-chip submenu when the player selects a sub-chip. */
Next submenu_subchip_81242D8(void) {
    /* If the chatbox is busy with flag 0x80, skip straight to the post-action call. */
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    SET_FLAGS_NZ(R0);
    if (ZERO_FLAG()) {
        /* Player actually picked a sub-chip: set state to 4 and take one from the inventory. */
        ST8(R5 + 2, 4);
        R0 = LD16(R5 + 0x10);
        R1 = 1;
        GUEST_CALL(TakeItem);
        GUEST_CALL(buildKeyItemFlagsTable_8124384);

        /* Decrement the cursor index by one; clamp so it doesn't exceed the new (lower) count. */
        u32 new_cursor = (u32)LD8(R5 + 0xC) - 1;
        u32 old_cursor = (u32)LD8(R5 + 0x14);
        SET_FLAGS_CMP((s32)new_cursor, (s32)old_cursor);
        if (NEGATIVE_FLAG()) {
            ST8(R5 + 0x14, (u8)new_cursor);
        }
    }
    GUEST_CALL(sub_8124870);
    RETURN_TO_CALLER();
}

/* ── buildKeyItemFlagsTable_8124384 ── */
/* Register-form call helpers for game functions outside this file */

/* register form: calls ZeroFillByByte outside this file, which has no typed C interface yet */
static void zero_fill_by_byte(u32 addr, u32 num_bytes) {
    R0 = addr;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByByte);
}

/* register form: calls CheckKeyItem outside this file, which has no typed C interface yet */
static u32 check_key_item(u32 item_id) {
    R0 = item_id;
    GUEST_CALL(CheckKeyItem);
    return R0;
}

/*
 * Zero the 16-byte key-item status buffer at unk_201C400, then populate it
 * with (item_id, CheckKeyItem result) byte-pairs for items 0x80 through 0x85.
 */
static void fill_key_item_table(void) {
    u32 base = A_unk_201C400;
    zero_fill_by_byte(base, 0x10);

    u32 offset = 0;
    for (u32 item = 0x80; item <= 0x85; item++) {
        u32 owned = check_key_item(item);
        ST8(base + offset, item);
        offset++;
        ST8(base + offset, owned);
        offset++;
    }
}

/* buildKeyItemFlagsTable_8124384() — no meaningful args or return; preserves r2–r7 via push/pop. */
Next buildKeyItemFlagsTable_8124384(void) {
    fill_key_item_table();
    RETURN_TO_CALLER();
}

/* ── subchip_initGfx_81243B0 ── */
/* ------------------------------------------------------------------
 * Subchip graphics initialisation.
 *
 * Builds a table of "init refs" at unk_20096E0 from a static template
 * table: each template entry is { source ptr, size, word, word } and
 * becomes { getKeyedTableEntry_811FB84(ptr, size), word, word } in the output table,
 * terminated by a 0 word. The ref tables are then decompressed and
 * the subchip background tiles are copied into the text layer.
 * ------------------------------------------------------------------ */

/* register form: calls getKeyedTableEntry_811FB84 outside this file, which has no typed C interface yet */
static u32 make_gfx_ref(u32 source, u32 size) {
    R0 = source;
    R1 = size;
    GUEST_CALL(getKeyedTableEntry_811FB84);
    return R0;
}

/* register form: calls decompAndCopyData outside this file, which has no typed C interface yet */
static void decomp_and_copy(u32 table) {
    R0 = table;
    GUEST_CALL(decompAndCopyData);
}

/* register form: calls battle_loadTileGridByNavi_8123300 outside this file, which has no typed C interface yet */
static void setup_text_script(u32 comp_texts, u32 text_script, u32 decomp_buffer) {
    R0 = comp_texts;
    R1 = text_script;
    R2 = decomp_buffer;
    GUEST_CALL(battle_loadTileGridByNavi_8123300);
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet.
 * The game's ABI passes the 5th/6th arguments in r4/r5. */
static void copy_background_tiles_subchip_initGfx_81243B0(u32 j, u32 i, u32 which_tile_block_32x32,
                                  u32 tile_ids, u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = which_tile_block_32x32;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

static void subchip_initGfx_init(void) {
    u32 refs = A_unk_20096E0;   /* output table, fed to decompAndCopyData below */
    u32 template = A_off_8124450; /* 3 entries of { ptr, size, word, word } */

    u32 out = 0;
    for (u32 in = 0; in < 0x30; in += 16) {
        u32 source = LD32(template + in);
        u32 size = LD32(template + in + 4);
        ST32(refs + out, make_gfx_ref(source, size));
        out += 4;
        /* the two trailing words of each entry are copied unchanged */
        for (u32 k = 0; k < 2; k++) {
            ST32(refs + out, LD32(template + in + 8 + k * 4));
            out += 4;
        }
    }
    ST32(refs + out, 0); /* terminate the ref table */

    decomp_and_copy(refs);
    decomp_and_copy(A_byte_8124488);

    setup_text_script(A_off_8124410, A_eTextScript201C100, A_eDecompBuffer2013A00);

    copy_background_tiles_subchip_initGfx_81243B0(0, 0, 1, A_eTextScript201BA00, 0x1E, 0x14);
}

/* subchip_initGfx_81243B0() -> r4 r5 r6 r7 (all preserved; the original
 * pushes r4-r7, works in them, and pops the saved values back) */
Next subchip_initGfx_81243B0(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;
    subchip_initGfx_init();
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── submenu_subchip_tickOamAndAdvanceAnimTable_81244EC ── */
/* submenu_subchip_tickOamAndAdvanceAnimTable_81244EC(r5 = entity ptr, r10 = ?):
 * Per-frame entity update. Loads the active flag at +0xC; if zero, do nothing.
 * Otherwise packs an effect descriptor and dispatches a render/animation call
 * (battleObject_queueOamFromTableRow_811FAF4), then decrements a timer at +0x12; when it hits zero, calls
 * battleObject_advanceAnimTableBy0xFF_8120CC8 to fetch a new (state, duration) pair that is stored at +0xD
 * and +0x12 respectively. */
Next submenu_subchip_tickOamAndAdvanceAnimTable_81244EC(void) {
    /* The original pushes {r4, lr} and uses r4 = 0x40000000 as an argument
     * to battleObject_queueOamFromTableRow_811FAF4, then pops r4 back, preserving it for the caller. */
    u32 saved_r4 = R4;
    R4 = 0x40000000;
    u32 saved_lr = LR;

    if (!LD8(R5 + 0xC))
        goto end;

    /* Effect descriptor: high word = 0x40000 | (field14 * 0x10 + 0x30). */
    u32 effect_descriptor = 0x40000 | (LD16(R5 + 0x14) * 0x10 + 0x30);

    /* Render/animation dispatch:
     *   r1 = 0x10002, r2 = state byte (+0xD),
     *   r3 = byte_8123F34,  r4 = 0x40000000. */
    R0 = effect_descriptor;
    R1 = 0x10002;
    R2 = LD8(R5 + 0xD);
    R3 = A_byte_8123F34;
    GUEST_CALL(battleObject_queueOamFromTableRow_811FAF4);

    /* Decrement timer at +0x12 and test for zero. */
    u8 duration = LD8(R5 + 0x12) - 1;
    ST8(R5 + 0x12, duration);
    SET_FLAGS_NZ(duration);
    if (!ZERO_FLAG())
        goto end;

    /* Timer expired: ask battleObject_advanceAnimTableBy0xFF_8120CC8 for the next (state, duration) pair. */
    R1 = A_dword_8123F3C;
    GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
    ST8(R5 + 0xD, R0 & 0xFF);
    ST8(R5 + 0x12, R1 & 0xFF);

end:
    R4 = saved_r4;
    LR = saved_lr;
    RETURN_TO_CALLER();
}

/* ── sub_812453C ── */
/* sub_812453C(r5 = text state):
 * Renders the next character of the active text entry into the text-box
 * tile buffer, then runs the follow-up update (sub_8124588).
 *
 * State layout at r5:
 *   +0x0C: text box active flag
 *   +0x0E: renderer result flag (cleared here, set from the render call)
 *   +0x0F: current character code (table entry minus 0x80)
 *   +0x14: index of the current text entry in the text table
 *
 * r4-r7 must survive the call to renderTextGfx (the original saves them
 * around the call); r8-r12 are whatever sub_8124588 leaves behind. */
Next sub_812453C(void) {
    R0 = 0;
    ST8(R5 + 0xE, R0);                          /* no rendered glyph pending */

    if (LD8(R5 + 0xC) != 0) {                   /* text box active? */
        /* Look up the current text entry's character code. */
        u32 entryOffs = LD16(R5 + 0x14) << 1;   /* entries are 2 bytes apart */
        u32 charCode = LD8(A_unk_201C400 + entryOffs) - 0x80;
        ST8(R5 + 0xF, charCode);

        /* renderTextGfx may clobber r4-r7, so keep the state pointer safe. */
        u32 state = R5;
        u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;

        R0 = A_eTextScript201C100;              /* text script data */
        R1 = charCode;                          /* character to render */
        R2 = A_unk_201AA00;                     /* glyph/work buffer */
        R3 = 0x6007000;                         /* VRAM destination */
        R4 = 0xB;
        R5 = 3;
        R6 = A_dword_86A5D60;
        R7 = 0;
        GUEST_CALL(renderTextGfx_8045F8C);

        R4 = savedR4;
        R5 = state;
        R6 = savedR6;
        R7 = savedR7;

        ST8(R5 + 0xE, R0);                      /* store render result */
    }

    GUEST_CALL(sub_8124588);
    RETURN_TO_CALLER();
}

/* ── sub_8124588 ── */
/* sub_8124588(r5 = background state struct):
 * Refills a 15x12-tile region of tile block 2 (a 32x32-tile background block).
 * If the flag byte at struct+0xE is nonzero, the tile ids come from the table
 * at unk_201EBDC (via CopyBackgroundTiles); otherwise call_sub_3005EBA is used,
 * which picks the tile source itself (tile ids = 0). */
Next sub_8124588(void) {
    u32 state = R5;
    u32 savedR4 = R4;

    if ((LD8(state + 0xE) & 0xFF) != 0) {
        /* flag set: copy tiles from the static tile-id table */
        R0 = 0xF;              /* j */
        R1 = 0xC;              /* i */
        R2 = 2;                /* tile block 32x32 */
        R3 = A_unk_201EBDC;    /* tile ids */
        R4 = 0xB;              /* j size */
        R5 = 6;                /* i size */
        GUEST_CALL(CopyBackgroundTiles);
    } else {
        /* flag clear: let the callee choose the tile source */
        R0 = 0xF;              /* j */
        R1 = 0xC;              /* i */
        R2 = 2;                /* tile block 32x32 */
        R3 = 0;                /* tile ids */
        R4 = 0xB;              /* j size */
        R5 = 6;                /* i size */
        GUEST_CALL(call_sub_3005EBA);
    }

    /* the original restores r4-r7 from the stack before returning */
    R4 = savedR4;
    R5 = state;
    RETURN_TO_CALLER();
}

/* ── bg_CopyTileBlock2At2_6_8x12_81245BC ── */
/* bg_CopyTileBlock2At2_6_8x12_81245BC: copy a 6x8 block of background tiles.
 *
 * Prepares the arguments for CopyBackgroundTiles (a leaf routine that takes
 * six arguments and may clobber the low registers, including r4/r5) and calls
 * it, preserving the caller's r4-r7 across the call. r1 and r2 are left as
 * CopyBackgroundTiles leaves them; the condition flags are likewise whatever
 * the callee's last flag-setting instruction produced. */
Next bg_CopyTileBlock2At2_6_8x12_81245BC(void) {
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

/* ── sub_81245D4 ── */
/* sub_81245D4(r4, r5 = map object, r10 = toolkit):
 * Handler for an interacted-with map object. On regular map groups it marks
 * the object (state byte at +2 = 8) and runs chatbox script 6. On map groups
 * 0x80 and up it instead dispatches to one of the special handlers listed in
 * the table at off_812460C, indexed by a byte read from unk_201C400 at
 * [object+0x14] * 2 (entries 0x80/0x81 both map to the first handler).
 * Leaves r2 = the dispatched handler address (r0 = selector byte,
 * r1 = (selector - 0x80) * 4 as in the original); r5/r10 pass through. */
Next sub_81245D4(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 mapGroup = LD8(gameState + oGameState_MapGroup);

    if ((s32)mapGroup < 0x80) {
        /* Regular map group: run the object's chatbox script. */
        ST8(R5 + 2, 8);
        R0 = 6;
        GUEST_CALL(chatbox_runScript201C100_812474C);
        RETURN_TO_CALLER();
    }

    /* Map group 0x80+: pick a handler from the dispatch table. */
    u32 select = LD16(R5 + 0x14) * 2;
    u32 kind = LD8(A_unk_201C400 + select);

    R0 = kind;
    R1 = (kind - 0x80) * 4;
    R2 = LD32(A_off_812460C + R1);

    switch (kind) {
    case 0x80: case 0x81: GUEST_CALL(chatbox_textShowNextPage); break;
    case 0x82:            GUEST_CALL(npc_runBeastOutDialog); break;
    case 0x83:            GUEST_CALL(sub_812469C); break;
    case 0x84:            GUEST_CALL(sub_81246D8); break;
    case 0x85:            GUEST_CALL(npc_conditionalChat); break;
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_runScript201C100_812474C ── */
/* chatbox_runScript201C100_812474C(r0 = script index):
 * Run one text script from the chatbox text archive (eTextScript201C100).
 * All register and flag outputs are just whatever chatbox_runScript leaves. */
Next chatbox_runScript201C100_812474C(void) {
    u32 scriptIdx = R0;

    R1 = scriptIdx;                 /* archive in r0, script index in r1 */
    R0 = A_eTextScript201C100;
    GUEST_CALL(chatbox_runScript_803FD9C);

    RETURN_TO_CALLER();
}

/* ── background_updateSceneStrips_812475C ── */
/* ---------------------------------------------------------------------------
 * Background strip refresh for the current scene (background_updateSceneStrips_812475C).
 *
 * Three 8x2-tile strips at column j=0x15 (rows i=8, i=5 and i=2) are either
 * filled from a tile-id table or cleared, depending on the scene state:
 *   - strip i=8 follows the word at state+0x24,
 *   - strip i=5 follows event flag 0x1709,
 *   - strip i=2 follows the word at state+0x28.
 * --------------------------------------------------------------------------- */

/* Event flag tested for the middle strip ("movflag EVENT_1709" = r0:0x17, r1:0x09). */
#define EVENT_1709 0x1709

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_background_tiles_sub_812475C(u32 j, u32 i, u32 tile_block_32x32, const u16 *tile_ids, u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = tile_block_32x32;
    R3 = (u32)tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* register form: calls call_sub_3005EBA outside this file, which has no typed C interface yet */
static void clear_background_tiles(u32 j, u32 i, u32 tile_block_32x32, u32 tile_ids, u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = tile_block_32x32;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(call_sub_3005EBA);
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static u32 test_event_flag_from_immediate(u16 flag) {
    R0 = flag >> 8;          /* the flag id is split over r0 (bank) and r1 (index) */
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();     /* Z set when the flag is clear */
}

/* Redraw the three background strips for the scene described by *state. */
static void update_background_strips(u32 toolkit_r10) {
    u32 state = LD32(toolkit_r10 + oToolkit_S2001c04_Ptr);

    /* Strip at i=8: real tiles when state+0x24 is set, cleared otherwise. */
    if (LD32(state + 0x24) != 0)
        copy_background_tiles_sub_812475C(0x15, 8, 2, (const u16 *)A_unk_201EC8C, 8, 2);
    else
        clear_background_tiles(0x15, 8, 2, 0, 8, 2);

    /* Strip at i=5: real tiles while event flag 0x1709 is set. */
    if (test_event_flag_from_immediate(EVENT_1709))
        copy_background_tiles_sub_812475C(0x15, 5, 2, (const u16 *)A_unk_201ECAC, 8, 2);
    else
        clear_background_tiles(0x15, 5, 2, 0, 8, 2);

    /* Strip at i=2: real tiles when state+0x28 is set, cleared otherwise. */
    if (LD32(state + 0x28) != 0)
        copy_background_tiles_sub_812475C(0x15, 2, 2, (const u16 *)A_unk_201ECCC, 8, 2);
    else
        clear_background_tiles(0x15, 2, 2, 0, 8, 2);
}

/* background_updateSceneStrips_812475C(r4, r5, r6, r7, r10) -> nz c v r0 r1 r2 r3 r4 r5 r6 r7 lr
 * r4-r7 are saved and restored around the calls, exactly like the original's
 * push/pop; r0-r3 and the flags are left as the last callee left them. */
Next background_updateSceneStrips_812475C(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;
    update_background_strips(R10);
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

/* ── sub_81247EC ── */
/* sub_81247EC:
 * Draws six lines of text (the library/help entry descriptions). Each entry i
 * (0..5) renders text id byte_8124864[i] from script TextScript873D9FC into
 * the RAM buffer off_812482C[i], generating tiles to the VRAM destination
 * byte_8124848[i] (0x06004000 + i*0x200), using dword_86B7AE0 as graphics
 * source and an 8x1 tile block.
 * r5/r8-r12 afterwards are whatever renderTextGfx_8045F8C leaves behind;
 * r4/r6/r7 (and r8/r9) come back exactly as they entered. */
Next sub_81247EC(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    for (u32 i = 0; i < 6; i++) {
        R0 = A_TextScript873D9FC;              /* text script bank */
        R1 = LD16(A_byte_8124864 + i * 2);     /* text id: 0x80 .. 0x85 */
        R2 = LD32(A_off_812482C + i * 4);      /* destination text buffer in RAM */
        R3 = LD32(A_byte_8124848 + i * 4);     /* destination VRAM tile address */
        R4 = 8;                                /* width in tiles */
        R5 = 1;                                /* height in tiles */
        R6 = A_dword_86B7AE0;                  /* font/graphics source */
        R7 = 0;
        GUEST_CALL(renderTextGfx_8045F8C);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8124870 ── */
Next sub_8124870(void) {
    u32 score_state = R5;
    u32 width = LD16(score_state + 0x18);
    u32 value = LD16(score_state + 0x1A);

    u32 table = 0x3071E0;
    if ((s32)(value >> 2) >= (s32)width)
        table = 0x3081E0;

    u32 saved_r4 = R4;
    u32 saved_r6 = R6;

    R0 = 0x201C4B0;
    R1 = width;
    R2 = value;
    R3 = table;
    R4 = 2;
    R6 = 3;
    GUEST_CALL(scoreboard_buildScoreBcdTilemap_8120900);

    R4 = saved_r4;
    R6 = saved_r6;

    RETURN_TO_CALLER();
}

/* ── HandleLibraryMenu8124B3C ── */
/* register form: indirect dispatch through a 3-entry jump table indexed by a byte
 * state variable at [R5+1]; GUEST_CALL requires a static target per case, and the
 * callee receives R0 = its own thumb entry address and R1 = the state byte,
 * matching the original ldr / bx sequence. Valid state values are 0, 4, 8. */
static void dispatch_library_menu(void) {
    u32 state = LD8(R5 + 1);
    R1 = state;
    switch (state) {
        case 0: R0 = 0x08124B5D; GUEST_CALL(battle_init_8124B5C); break;
        case 4: R0 = 0x08124C31; GUEST_CALL(sub_8124C30); break;
        case 8: R0 = 0x08125255; GUEST_CALL(chatbox_flagResolve_8125254); break;
    }
}

/* HandleLibraryMenu8124B3C: trampoline dispatching to one of three library-menu
 * handlers based on the byte at [object + 1]. */
Next HandleLibraryMenu8124B3C(void) {
    dispatch_library_menu();
    RETURN_TO_CALLER();
}

/* ── battle_init_8124B5C ── */
/* register form: calls zeroFillVRAM outside this file, which has no typed C interface yet */
static void call_zeroFillVRAM(void) {
    GUEST_CALL(zeroFillVRAM);
}

/* register form: calls ZeroFill_byte_3001960 outside this file, which has no typed C interface yet */
static void call_zeroFillByteTable(void) {
    GUEST_CALL(ZeroFill_byte_3001960);
}

/* register form: calls ZeroFillGFX30025c0 outside this file, which has no typed C interface yet */
static void call_zeroFillGFXData(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
}

/* register form: calls toolkit_applyOverlayPalette_80015FC outside this file, which has no typed C interface yet */
static void call_sub_80015FC(u32 arg) {
    R0 = arg;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);
}

/* register form: calls battleObject_initChipSpriteSlotBuffer_8126818 outside this file, which has no typed C interface yet */
static void call_sub_8126818(u32 arg) {
    R0 = arg;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);
}

/* register form: calls initRefs_buildAndDecompressTable_8125280 outside this file, which has no typed C interface yet */
static void call_initRefsTable(void) {
    GUEST_CALL(initRefs_buildAndDecompressTable_8125280);
}

/* register form: calls sub_81254FC outside this file, which has no typed C interface yet; clobbers r10 */
static void call_sub_81254FC(void) {
    GUEST_CALL(sub_81254FC);
}

/* register form: calls countEventFlagChips_81207F8 outside this file, which has no typed C interface yet */
static u32 call_sub_81207F8(u32 index) {
    R0 = index;
    GUEST_CALL(countEventFlagChips_81207F8);
    return R0;
}

/* register form: calls buildChipAvailabilityTable_81258F8 outside this file, which has no typed C interface yet */
static u32 call_sub_81258F8(void) {
    GUEST_CALL(buildChipAvailabilityTable_81258F8);
    return R0;
}

/* register form: calls buildChipCatalogByType_8125994 outside this file, which has no typed C interface yet */
static u32 call_sub_8125994(void) {
    GUEST_CALL(buildChipCatalogByType_8125994);
    return R0;
}

/* register form: calls packBuildChipLibraryTable_8125A6C outside this file, which has no typed C interface yet */
static u32 call_sub_8125A6C(void) {
    GUEST_CALL(packBuildChipLibraryTable_8125A6C);
    return R0;
}

/* register form: calls library_rebuildCollectedChipList_8125B3C outside this file, which has no typed C interface yet */
static u32 call_sub_8125B3C(void) {
    GUEST_CALL(library_rebuildCollectedChipList_8125B3C);
    return R0;
}

/* register form: calls buildComChipFilter_8125C08 outside this file, which has no typed C interface yet */
static u32 call_sub_8125C08(void) {
    GUEST_CALL(buildComChipFilter_8125C08);
    return R0;
}

/* register form: calls battle_resetEStructRecords_8046664 outside this file, which has no typed C interface yet; clobbers r1 */
static void call_sub_8046664_sub_8124B5C(void) {
    GUEST_CALL(battle_resetEStructRecords_8046664);
}

/* register form: calls battle_spawnEStructRecordsFromTable_80465A0 outside this file, which has no typed C interface yet */
static void call_sub_80465A0(u32 ptr) {
    R0 = ptr;
    GUEST_CALL(battle_spawnEStructRecordsFromTable_80465A0);
}

/* register form: calls sub_81252C0 outside this file, which has no typed C interface yet */
static void call_sub_81252C0(void) {
    GUEST_CALL(sub_81252C0);
}

/* register form: calls battle_drawCustFolderSlots_8125D10 outside this file, which has no typed C interface yet */
static void call_sub_8125D10_sub_8124B5C(void) {
    GUEST_CALL(battle_drawCustFolderSlots_8125D10);
}

/* register form: calls sub_812546C outside this file, which has no typed C interface yet; clobbers r7 */
static void call_sub_812546C(void) {
    GUEST_CALL(sub_812546C);
}

/* register form: calls battleObject_advanceAnimTableBy0xFF_8120CC8 outside this file, which has no typed C interface yet */
static u32 call_sub_8120CC8(u32 slot, u32 data) {
    R0 = slot; R1 = data;
    GUEST_CALL(battleObject_advanceAnimTableBy0xFF_8120CC8);
    return R0 | (R1 << 4);  /* combine both callee outputs as the caller does */
}

/* register form: calls SetScreenFade outside this file, which has no typed C interface yet; clobbers r1 */
static void call_SetScreenFade(int level, int speed) {
    R0 = level; R1 = speed;
    GUEST_CALL(SetScreenFade);
}

/* Initialize VRAM, GFX, render-info header, battle-state table, and trigger a screen fade.
 * battle_data (r5) is the main battle-state struct; toolkit (r10) provides RenderInfoPtr. */
static u32 init_battle_data(u32 battle_data, u32 toolkit) {
    /* Clear VRAM, byte table at 0x3001960, and GFX data at 0x30025C0 */
    call_zeroFillVRAM();
    call_zeroFillByteTable();
    call_zeroFillGFXData();

    call_sub_80015FC(0x10);

    /* Reset render-info header: first u16 to 0x7F40, six following u16s to 0 */
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render_info, 0x7F40);
    ST16(render_info + 0x10, 0);
    ST16(render_info + 0x12, 0);
    ST16(render_info + 0x14, 0);
    ST16(render_info + 0x16, 0);
    ST16(render_info + 0x18, 0);
    ST16(render_info + 0x1A, 0);

    call_sub_8126818(0);

    /* Fixed initial state bytes */
    ST8(battle_data + 1, 4);
    ST8(battle_data + 2, 0);

    call_initRefsTable();
    call_sub_81254FC();

    /* Timer counters */
    ST32(battle_data + 0x74, 0xA0);
    ST32(battle_data + 0x78, 0xA0);

    /* Per-index lookup table slots */
    ST32(battle_data + 0x58, call_sub_81207F8(0));
    ST32(battle_data + 0x5C, call_sub_81207F8(1));
    ST32(battle_data + 0x60, call_sub_81207F8(2));

    u32 lookup_5 = call_sub_81207F8(5);
    ST32(battle_data + 0x64, lookup_5);

    /* Battle mode byte: 4 if lookup[5] non-zero, else 3 */
    ST8(battle_data + 0xE, lookup_5 ? 4 : 3);

    ST32(battle_data + 0x68, call_sub_81207F8(4));

    /* Sub-field initialization (each callee returns a u16) */
    ST16(battle_data + 0x12, call_sub_81258F8());
    ST16(battle_data + 0x1C, call_sub_8125994());
    ST16(battle_data + 0x26, call_sub_8125A6C());
    ST16(battle_data + 0x30, call_sub_8125B3C());
    ST16(battle_data + 0x3A, call_sub_8125C08());

    /* Additional subsystems */
    call_sub_8046664_sub_8124B5C();
    call_sub_80465A0(A_byte_8124AE0);
    call_sub_81252C0();
    call_sub_8125D10_sub_8124B5C();
    call_sub_812546C();

    /* Store combined lookup byte from battleObject_advanceAnimTableBy0xFF_8120CC8 */
    ST8(battle_data + 0xC, call_sub_8120CC8(0, A_dword_8124A94));

    /* Trigger screen fade in */
    call_SetScreenFade(8, 0x10);

    return 0;
}

/* battle_init_8124B5C(r5 = battle_data, r10 = toolkit) -> r0=0 */
Next battle_init_8124B5C(void) {
    R0 = init_battle_data(R5, R10);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8124C30 ── */
/* sub_8124C30: Battle-phase dispatcher.
 *
 * The work object at r5 stores the current battle-phase byte at +0x02.  That byte
 * is used as a *byte* offset into the handler table at off_8124C44 (the original
 * does ldr r0,[r0,r1] with no scaling, so the byte already encodes the table
 * index in 4-byte units: 0, 4, 8 … 0x4C for the 20 entries).  The selected
 * handler runs with r0 = its own address and r1 = the phase byte; r2, r5, r10
 * are passed through untouched.  After the handler returns the dispatcher forces
 * r0 = 0 (which also clears N and sets Z).
 *
 * Everything else (r1, r2, r3, r5, r7–r12, lr, c, v) is whatever the handler left.
 */

#define BATTLE_PHASE_BYTE(r5) ((u32)LD8((r5) + 2))

/* register form: the target is one of twenty thumb handler addresses read from
 * the jump table; those routines have no typed wrappers and the dispatcher must
 * hand every register and the c/v flags straight through to its caller. */
static void dispatch_battle_phase(u32 r2, u32 r5, u32 r10) {
    u32 phase = BATTLE_PHASE_BYTE(r5);
    u32 handler = LD32(A_off_8124C44 + phase);

    R0 = handler;        /* original: bx r0 with the target still in r0       */
    R1 = phase;          /* original: ldrb r1,[r5,#2] before the table load   */
    R2 = r2;
    R5 = r5;
    R10 = r10;
    GUEST_CALL_ADDR(handler);
}

/* sub_8124C30(r2, r5 = work object, r10) -> r0 = 0.
 * r1, r2, r3, r5, r7–r12, lr and c/v pass through from the phase handler. */
Next sub_8124C30(void) {
    dispatch_battle_phase(R2, R5, R10);
    R0 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_8124C94 ── */
Next sub_8124C94(void) {
    R0 = 0;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);

    R0 = 0;
    R1 = 1;
    GUEST_CALL(e_setField13);

    /* Clear the byte at offset 0xD and copy 0x14->0x16 and 0x18->0x1A. */
    ST8(R5 + 0xD, 0);
    ST16(R5 + 0x16, LD16(R5 + 0x14));
    ST16(R5 + 0x1A, LD16(R5 + 0x18));

    /* If a screen fade is in progress, ignore Start/D-Pad so the player
     * can't interrupt it. */
    GUEST_CALL(IsScreenFadeActive);
    if (!ZERO_FLAG()) {
        R0 = 0;
        GUEST_CALL(menu_handleStartAndDpad_8125594);
    }

    GUEST_CALL(battle_runTransition_8125444);
    GUEST_CALL(battle_drawCustFolderSlots_8125D10);
    GUEST_CALL(sub_81264F4);

    /* Refresh the two sprites attached to this menu state. */
    R0 = LD32(R5 + 0x58);
    R1 = LD16(R5 + 0x12);
    R2 = 4;
    GUEST_CALL(sprite_setStructHalfwordPairOnThree_8117084);

    R0 = LD32(R5 + 0x5C);
    R1 = LD16(R5 + 0x1C);
    R2 = 7;
    GUEST_CALL(sprite_setStructHalfwordPairOnThree_8117084);

    GUEST_CALL(sub_812674C);
    GUEST_CALL(sprite_queueBattleOamEntry);

    RETURN_TO_CALLER();
}

/* ── sub_8124CE8 ── */
/* sub_8124CE8: advance the counter at o+0x74 by 2 frames, clamp it at 0x8E
 * (then mark the state as 0x0C = finished/landed), reset another field to
 * 0xFFFFFFFF, refresh the sprite's two halfword pairs and run a few
 * follow-up animations/helper routines. */
Next sub_8124CE8(void) {
    u32 r0 = R0;
    u32 lr = LR;

    R0 = 0;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);

    /* Tick the per-sprite counter at o+0x74 down by 2 (note: it actually
     * decrements, even though the disassembly uses 'sub'). The original
     * uses 'sub' here; behavior must match exactly. */
    u32 counter = LD32(R5 + 0x74) - 2;
    ST32(R5 + 0x74, counter);

    if ((s32)counter <= 0x8E) {
        ST32(R5 + 0x74, 0x8E);
        ST8(R5 + 2, 0x0C);
    }

    ST32(R5 + 0x70, 0xFF);
    GUEST_CALL(battle_runTransition_8125444);
    GUEST_CALL(battle_drawCustFolderSlots_8125D10);
    GUEST_CALL(submenu_redrawFolderScreen);

    /* Update sprite frame/duration pair #4 from the live halfwords. */
    R0 = LD32(R5 + 0x58);
    R1 = LD16(R5 + 0x12);
    R2 = 4;
    GUEST_CALL(sprite_setStructHalfwordPairOnThree_8117084);

    /* Update sprite frame/duration pair #7; the source halfword is
     * conditional on the value at o+0x5C being 0x27. */
    R1 = 0;
    R0 = LD32(R5 + 0x5C);
    if (R0 == 0x27) {
        R1 = LD16(R5 + 0x1C);
    }
    R2 = 7;
    GUEST_CALL(sprite_setStructHalfwordPairOnThree_8117084);

    RETURN_TO_CALLER();
}

/* ── battle_objTickTimerAndRefreshSpritePairs_8124D30 ── */
/* battle_objTickTimerAndRefreshSpritePairs_8124D30 — per-object update step.
 *
 * The object/work pointer arrives in r5; the function body itself only ever
 * reads r5, so every other register is pure pass-through: its exit value is
 * whatever the final guest callee leaves behind, which the guest calls below
 * reproduce exactly. The same holds for the condition flags, whose exit state
 * comes from inside the callees, not from this function's own code.
 */

/* register form: calls battleObject_initChipSpriteSlotBuffer_8126818 outside this file, which has no typed C interface yet */
static void call_sub_8126818_with_zero(void) {
    R0 = 0;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);
}

/* register form: calls battle_runTransition_8125444 outside this file, which has no typed C interface yet */
static void call_sub_8125444_sub_8124D30(void) {
    GUEST_CALL(battle_runTransition_8125444);
}

/* register form: calls battle_drawCustFolderSlots_8125D10 outside this file, which has no typed C interface yet */
static void call_sub_8125D10(void) {
    GUEST_CALL(battle_drawCustFolderSlots_8125D10);
}

/* register form: calls submenu_redrawFolderScreen outside this file, which has no typed C interface yet */
static void call_sub_8125D94(void) {
    GUEST_CALL(submenu_redrawFolderScreen);
}

/* register form: calls sprite_setStructHalfwordPairOnThree_8117084 outside this file, which has no typed C interface yet */
static void set_sprite_halfword_pair(u32 ptr, u32 value, u32 slot) {
    R0 = ptr;
    R1 = value;
    R2 = slot;
    GUEST_CALL(sprite_setStructHalfwordPairOnThree_8117084);
}

/* Tick the object's timer at +0x74 by two; once it reaches 160, clamp it there
 * and clear the state byte at +2. Then store 0xFF into the counter at +0x70,
 * run the three per-object update routines, and rewrite sprite halfword pairs
 * 4 and 7. Pair 7 carries the halfword at +0x1C when the value at +0x5C is
 * 0x27, and 0 otherwise. */
static void update_object_timer_and_sprite_pairs(u32 obj) {
    call_sub_8126818_with_zero();

    u32 timer = LD32(obj + 0x74) + 2;
    ST32(obj + 0x74, timer);
    if ((s32)timer >= 0xA0) {
        ST32(obj + 0x74, 0xA0);
        ST8(obj + 2, 0);
    }

    ST32(obj + 0x70, 0xFF);

    call_sub_8125444_sub_8124D30();
    call_sub_8125D10();
    call_sub_8125D94();

    set_sprite_halfword_pair(LD32(obj + 0x58), LD16(obj + 0x12), 4);

    u32 second = LD32(obj + 0x5C);
    u32 pair7_value = 0;
    if (second == 0x27)
        pair7_value = LD16(obj + 0x1C);
    set_sprite_halfword_pair(second, pair7_value, 7);
}

/* battle_objTickTimerAndRefreshSpritePairs_8124D30: object pointer in r5; r0-r3, the flags and every other
 * register are left exactly as the last guest callee leaves them. */
Next battle_objTickTimerAndRefreshSpritePairs_8124D30(void) {
    update_object_timer_and_sprite_pairs(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8124D78 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5, r6; } call_sub_8126818_result;
/* register form: generated wrapper: battleObject_initChipSpriteSlotBuffer_8126818 has no typed C interface yet */
static inline call_sub_8126818_result call_sub_8126818_sub_8124D78(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);
    return (call_sub_8126818_result){ R5, R6 };
}

typedef struct { u32 r2, r5, r6; } call_sub_811983C_result;
/* register form: generated wrapper: e_setField13 has no typed C interface yet */
static inline call_sub_811983C_result call_sub_811983C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(e_setField13);
    return (call_sub_811983C_result){ R2, R5, R6 };
}

typedef struct { u32 r5, r8, r9, r10, r12; } call_sub_81255F8_result;
/* register form: generated wrapper: sub_81255F8 has no typed C interface yet */
static inline call_sub_81255F8_result call_sub_81255F8(u32 r0, u32 r2, u32 r5) {
    R0 = r0;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sub_81255F8);
    return (call_sub_81255F8_result){ R5, R8, R9, R10, R12 };
}

typedef struct { u32 r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_sub_8125444_result;
/* register form: generated wrapper: battle_runTransition_8125444 has no typed C interface yet */
static inline call_sub_8125444_result call_sub_8125444_sub_8124D78(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_runTransition_8125444);
    return (call_sub_8125444_result){ R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r4, r5, r6, r7, r8, r9; } call_sub_8125D94_result;
/* register form: generated wrapper: submenu_redrawFolderScreen has no typed C interface yet */
static inline call_sub_8125D94_result call_sub_8125D94_sub_8124D78(u32 r5) {
    R5 = r5;
    GUEST_CALL(submenu_redrawFolderScreen);
    return (call_sub_8125D94_result){ R4, R5, R6, R7, R8, R9 };
}

typedef struct { u32 r4, r5, r6, r7; } call_wrapper_sub8126570_fixedArgs_812650C_result;
/* register form: generated wrapper: wrapper_sub8126570_fixedArgs_812650C has no typed C interface yet */
static inline call_wrapper_sub8126570_fixedArgs_812650C_result call_wrapper_sub8126570_fixedArgs_812650C(u32 r5) {
    R5 = r5;
    GUEST_CALL(wrapper_sub8126570_fixedArgs_812650C);
    return (call_wrapper_sub8126570_fixedArgs_812650C_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r4, r5, r6, r7; } call_sprite_setStructHalfwordPairOnThree_8117084_result;
/* register form: generated wrapper: sprite_setStructHalfwordPairOnThree_8117084 has no typed C interface yet */
static inline call_sprite_setStructHalfwordPairOnThree_8117084_result call_sprite_setStructHalfwordPairOnThree_8117084(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(sprite_setStructHalfwordPairOnThree_8117084);
    return (call_sprite_setStructHalfwordPairOnThree_8117084_result){ R4, R5, R6, R7 };
}

typedef struct { u32 r3, r4, r5, r6, r7; } call_sub_812674C_result;
/* register form: generated wrapper: sub_812674C has no typed C interface yet */
static inline call_sub_812674C_result call_sub_812674C_sub_8124D78(u32 r5) {
    R5 = r5;
    GUEST_CALL(sub_812674C);
    return (call_sub_812674C_result){ R3, R4, R5, R6, R7 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; bool z, n, c, v; } call_sub_81267A8_result;
/* register form: generated wrapper: sprite_queueBattleOamEntry has no typed C interface yet */
static inline call_sub_81267A8_result call_sub_81267A8_sub_8124D78(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_queueBattleOamEntry);
    return (call_sub_81267A8_result){ R0, R1, R2, R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Prepare a battle sprite: reset animation controller, enable entity tick,
 * copy current pose halfwords into their previous-frame slots for
 * interpolation, start the entry animation (type 0xA), configure two pose
 * slots via the halfword-pair setter, then run the remaining post-setup
 * and finalise.  `base` is the R5 struct pointer from the caller. */
static void sub_8124D78_impl(u32 base) {
    call_sub_8126818_sub_8124D78(0, base);                              /* reset controller */
    call_sub_811983C_result tick = call_sub_811983C(1, 1);  /* enable tick */

    ST8(base + 0x0D, 1);                                    /* mark in-play */
    ST16(base + 0x20, LD16(base + 0x1E));                   /* prev pose X */
    ST16(base + 0x24, LD16(base + 0x22));                   /* prev pose Y */

    call_sub_81255F8(0xA, tick.r2, base);                  /* entry anim type 0xA */
    call_sub_8125444_sub_8124D78(base);
    call_sub_8125D94_sub_8124D78(base);
    call_wrapper_sub8126570_fixedArgs_812650C(base);

    /* Pose slot 4: sprite pointer at +0x58, halfword at +0x12. */
    call_sprite_setStructHalfwordPairOnThree_8117084(
        LD32(base + 0x58), LD16(base + 0x12), 4);

    /* Pose slot 7: sprite pointer at +0x5C.  If the pointer itself is 0x27,
     * take the saved halfword at +0x1C; otherwise pass 0. */
    u32 sprite_idx = LD32(base + 0x5C);
    u32 pose = (sprite_idx == 0x27) ? (u32)LD16(base + 0x1C) : 0;
    call_sprite_setStructHalfwordPairOnThree_8117084(sprite_idx, pose, 7);

    call_sub_812674C_sub_8124D78(base);
    call_sub_81267A8_sub_8124D78(base);                                 /* finalise, sets r1-r3/c/v */
}

/* sub_8124D78(r5 = battle-object base) — all register outputs are left
 * behind by the callee calls; the adapter writes nothing explicitly. */
Next sub_8124D78(void) {
    sub_8124D78_impl(R5);
    RETURN_TO_CALLER();
}

/* ── sprite_shieldUpdate_8124DCC ── */
/* sprite_shieldUpdate_8124DCC: shield/sprite update for a boss or boss component.
 * Reads/writes fields on the struct whose address is passed in R5, refreshes
 * a few sprite halfword slots, and calls the related update routines. */

#define kShotSprite_HP          0x74
#define kShotSprite_State       0x02
#define kShotSprite_DrawFlag    0x70
#define kShotSprite_PtrA        0x58
#define kShotSprite_Halfword_12 0x12
#define kShotSprite_Ptr60       0x60
#define kShotSprite_Halfword_26 0x26
#define kShotSprite_Ptr5C       0x5C
#define kShotSprite_Halfword_1C 0x1C

#define SHIELD_MIN_HP     0x7C
#define SHIELD_LOCK_HP    0x86
#define SHIELD_LOCK_STATE 0x18
#define SHIELD_DRAW_ON    0xFF

#define SPECIAL_HP        5
#define SPECIAL_PTR       0x27

#define SLOT_HP_FRAME     4
#define SLOT_STATE_FRAME  7

/* register form: calls battleObject_initChipSpriteSlotBuffer_8126818 outside this file, which has no typed C interface yet */
static void call_sub_8126818_sub_8124DCC(u32 arg) {
    R0 = arg;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);
}

/* register form: calls battle_runTransition_8125444 outside this file, which has no typed C interface yet */
static void call_sub_8125444_sub_8124DCC(void) {
    GUEST_CALL(battle_runTransition_8125444);
}

/* register form: calls submenu_redrawFolderScreen outside this file, which has no typed C interface yet */
static void call_sub_8125D94_sub_8124DCC(void) {
    GUEST_CALL(submenu_redrawFolderScreen);
}

/* register form: calls bg_drawArenaTiles_8125E78 outside this file, which has no typed C interface yet */
static void call_sub_8125E78(void) {
    GUEST_CALL(bg_drawArenaTiles_8125E78);
}

/* register form: calls sprite_setStructHalfwordPairOnThree_8117084 outside this file,
 * which has no typed C interface yet */
static void set_sprite_halfword_pair_sub_8124DCC(u32 ptr, u16 hw, u32 slot) {
    R0 = ptr;
    R1 = hw;
    R2 = slot;
    GUEST_CALL(sprite_setStructHalfwordPairOnThree_8117084);
}

/* Shield sprite update. Operates on the struct whose guest address is in R5.
 * Logic mirrors the original: take 2 from the HP field, clamp it to the minimum
 * (with the lock-state side effect), force the draw flag on, run the three
 * update routines, then refresh the halfword slots on the sprite.
 *
 * Note: the original treats R5 as a register form across the whole function,
 * but the field offsets and the call setup make it cleaner to pass the struct
 * pointer as a typed parameter and read it back into R5 around the outside
 * calls -- the only place that *needs* R5 is the GUEST_CALL helpers below. */
static void run_shield_sprite_update(u32 struct_ptr) {
    /* register form: stack buffer whose guest address is held in R5 across
     * the outside calls below; we keep R5 in sync with our typed pointer. */
    R5 = struct_ptr;

    call_sub_8126818_sub_8124DCC(0);

    R5 = struct_ptr;
    u32 hp = LD32(R5 + kShotSprite_HP) - 2;
    ST32(R5 + kShotSprite_HP, hp);
    if ((s32)hp <= (s32)SHIELD_MIN_HP) {
        ST32(R5 + kShotSprite_HP, SHIELD_MIN_HP);
        ST8(R5 + kShotSprite_State, SHIELD_LOCK_STATE);
    }

    R5 = struct_ptr;
    ST32(R5 + kShotSprite_DrawFlag, SHIELD_DRAW_ON);

    R5 = struct_ptr;
    call_sub_8125444_sub_8124DCC();
    R5 = struct_ptr;
    call_sub_8125D94_sub_8124DCC();
    R5 = struct_ptr;
    call_sub_8125E78();

    /* Slot 4: pointer + halfword. If HP is at or below the lock threshold the
     * defaults kick in (pointer from +0x60, halfword cleared to 0); only when
     * the pointer is the expected sentinel does the alternate halfword get
     * substituted in. */
    R5 = struct_ptr;
    u32 ptr_a  = LD32(R5 + kShotSprite_PtrA);
    u16 hw_a   = LD16(R5 + kShotSprite_Halfword_12);
    u32 hp_now = LD32(R5 + kShotSprite_HP);
    if ((s32)hp_now <= (s32)SHIELD_LOCK_HP) {
        hw_a  = 0;
        ptr_a = LD32(R5 + kShotSprite_Ptr60);
        if (ptr_a == SPECIAL_HP) {
            hw_a = LD16(R5 + kShotSprite_Halfword_26);
        }
    }
    set_sprite_halfword_pair_sub_8124DCC(ptr_a, hw_a, SLOT_HP_FRAME);

    /* Slot 7: same pattern with the state pointer. */
    R5 = struct_ptr;
    u32 ptr_b = LD32(R5 + kShotSprite_Ptr5C);
    u16 hw_b  = 0;
    if (ptr_b == SPECIAL_PTR) {
        hw_b = LD16(R5 + kShotSprite_Halfword_1C);
    }
    set_sprite_halfword_pair_sub_8124DCC(ptr_b, hw_b, SLOT_STATE_FRAME);
}

Next sprite_shieldUpdate_8124DCC(void) {
    run_shield_sprite_update(R5);
    RETURN_TO_CALLER();
}

/* ── battle_runIntro_812508C_812509C ── */
/* register form: calls battleObject_initChipSpriteSlotBuffer_8126818 outside this file, which has no typed C interface yet */
static void sub_8126818_reg(u32 arg) {
    R0 = arg;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);
}

/* register form: calls battle_input_tick_8125784 outside this file, which has no typed C interface yet */
static void sub_8125784_reg(u32 arg) {
    R0 = arg;
    GUEST_CALL(battle_input_tick_8125784);
}

/* register form: calls battle_runTransition_8125444 outside this file, which has no typed C interface yet */
static void sub_8125444_reg(void) {
    GUEST_CALL(battle_runTransition_8125444);
}

/* register form: calls sub_8126184 outside this file, which has no typed C interface yet */
static void sub_8126184_reg(void) {
    GUEST_CALL(sub_8126184);
}

/* register form: calls sub_8126114 outside this file, which has no typed C interface yet */
static void sub_8126114_reg(void) {
    GUEST_CALL(sub_8126114);
}

/* register form: calls sub_8126554 outside this file, which has no typed C interface yet */
static void sub_8126554_reg(void) {
    GUEST_CALL(sub_8126554);
}

/* register form: calls sub_812674C outside this file, which has no typed C interface yet */
static void sub_812674C_reg(void) {
    GUEST_CALL(sub_812674C);
}

/* register form: calls sprite_queueBattleOamEntry outside this file, which has no typed C interface yet */
static void sub_81267A8_reg(void) {
    GUEST_CALL(sprite_queueBattleOamEntry);
}

/* register form: calls battle_queueFixedOverlayOam outside this file, which has no typed C interface yet */
static void sub_81267F0_reg(void) {
    GUEST_CALL(battle_queueFixedOverlayOam);
}

/* Initialize battle-state fields on the object pointed to by R5, then run the
 * appropriate intro branch (sub_8126184 vs sub_8126114) based on a flag word,
 * finishing with the standard post-intro calls. */
static void do_battle_intro(u32 obj_ptr) {
    u32 obj = obj_ptr;
    sub_8126818_reg(1);

    ST8(obj + 0x0D, 4);
    ST16(obj + 0x3E, LD16(obj + 0x3C));
    ST16(obj + 0x42, LD16(obj + 0x40));

    sub_8125784_reg(0x28);
    sub_8125444_reg();

    if (LD32(obj + 0x64) != 0) {
        sub_8126114_reg();
    } else {
        sub_8126184_reg();
    }

    sub_8126554_reg();
    sub_812674C_reg();
    sub_81267A8_reg();
    sub_81267F0_reg();
}

Next battle_runIntro_812508C_812509C(void) {
    do_battle_intro(R5);
    RETURN_TO_CALLER();
}

/* ── battle_runUpdatesAndCheckFade_81250E0 ── */
/* Run five battle sub-updates, then check if a screen fade is active.
 * If active: write 8 to [ptr+1] and output r0=8 (flags N=0,Z=0 from MOV).
 * If not active: output r0 from IsScreenFadeActive (flags N=0,Z=1).
 * C and V are always IsScreenFadeActive's (MOV/CMP don't alter them here).
 * Returns a value whose zero/nonzero lets the adapter reproduce N/Z correctly. */

/* register form: calls sub_812674C outside this file, which has no typed C interface yet */
static void call_sub_812674C(void) {
    GUEST_CALL(sub_812674C);
}

/* register form: calls sprite_queueBattleOamEntry outside this file, which has no typed C interface yet */
static void call_sub_81267A8(void) {
    GUEST_CALL(sprite_queueBattleOamEntry);
}

/* register form: calls battle_runTransition_8125444 outside this file, which has no typed C interface yet */
static void call_sub_8125444(void) {
    GUEST_CALL(battle_runTransition_8125444);
}

/* register form: calls battle_drawCustFolderSlots_8125D10 outside this file, which has no typed C interface yet */
static void call_sub_8125D10_sub_81250E0(void) {
    GUEST_CALL(battle_drawCustFolderSlots_8125D10);
}

/* register form: calls sub_81264F4 outside this file, which has no typed C interface yet */
static void call_sub_81264F4(void) {
    GUEST_CALL(sub_81264F4);
}

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet */
static u32 call_is_screen_fade_active_sub_81250E0(bool *fade_is_active) {
    GUEST_CALL(IsScreenFadeActive);
    *fade_is_active = !ZERO_FLAG();   /* Z clear → fade is active */
    return R0;
}

static u32 battle_update_and_fade_check(u32 fade_ptr, u32 *out_r0) {
    call_sub_812674C();
    call_sub_81267A8();
    call_sub_8125444();
    call_sub_8125D10_sub_81250E0();
    call_sub_81264F4();

    bool fade_active;
    u32 ret = call_is_screen_fade_active_sub_81250E0(&fade_active);

    if (fade_active) {
        ST8(fade_ptr + 1, 8);
        *out_r0 = 8;
        return 8;     /* SET_FLAGS_NZ(8) → N=0, Z=0, matching MOV r0,#8 */
    }
    *out_r0 = ret;
    return 0;         /* SET_FLAGS_NZ(0) → N=0, Z=1, matching IsScreenFadeActive's Z */
}

/* battle_runUpdatesAndCheckFade_81250E0() → nz, r0, c, v */
Next battle_runUpdatesAndCheckFade_81250E0(void) {
    u32 r0_val;
    u32 flag_enc = battle_update_and_fade_check(R5, &r0_val);
    R0 = r0_val;
    SET_FLAGS_NZ(flag_enc);
    RETURN_TO_CALLER();
}

/* ── battle_transitionTick_8125170 ── */
/* register form: calls sub_812674C outside this file, which has no typed C interface yet */
static void call_812674C(void) {
    GUEST_CALL(sub_812674C);
}

/* register form: calls sprite_queueBattleOamEntry outside this file, which has no typed C interface yet */
static void call_81267A8(void) {
    GUEST_CALL(sprite_queueBattleOamEntry);
}

/* register form: calls battle_runTransition_8125444 outside this file, which has no typed C interface yet */
static void call_8125444(void) {
    GUEST_CALL(battle_runTransition_8125444);
}

/* register form: calls sub_8126114 outside this file, which has no typed C interface yet */
static void call_8126114(void) {
    GUEST_CALL(sub_8126114);
}

/* register form: calls sub_8126554 outside this file, which has no typed C interface yet */
static void call_8126554(void) {
    GUEST_CALL(sub_8126554);
}

/* register form: calls battle_queueFixedOverlayOam outside this file, which has no typed C interface yet */
static void call_81267F0(void) {
    GUEST_CALL(battle_queueFixedOverlayOam);
}

/* register form: IsScreenFadeActive's condition flags must reach the caller
 * verbatim when Z is set (fade inactive → beq taken), but must be partially
 * overwritten by MOV R0,#8's N/Z when Z is clear (fade active → store path).
 * A single SET_FLAGS call in the adapter cannot express both paths, and the
 * C/V flags always originate from IsScreenFadeActive regardless of branch. */
static u32 fade_check_and_store(u32 battle_obj) {
    GUEST_CALL(IsScreenFadeActive);
    if (ZERO_FLAG())
        return R0;                /* fade inactive: beq, flags already correct */
    R0 = 8;
    ST8(battle_obj + 1, 8);       /* advance phase byte */
    SET_FLAGS_NZ(R0);             /* mov r0, #8 → N=0, Z=0; C,V unchanged */
    return 8;
}

/* Run one tick of the battle visual-transition sequence.
 * Calls six stage-update routines, then checks whether the screen-fade
 * animation is still playing. If it is, writes phase byte 8 into the
 * battle object at [r5+1]. Returns the resulting R0. */
static u32 battle_transition_tick(u32 battle_obj) {
    call_812674C();
    call_81267A8();
    call_8125444();
    call_8126114();
    call_8126554();
    call_81267F0();
    return fade_check_and_store(battle_obj);
}

/* battle_transitionTick_8125170(r5 = battle-object pointer) -> r0; flags from IsScreenFadeActive path */
Next battle_transitionTick_8125170(void) {
    R0 = battle_transition_tick(R5);
    RETURN_TO_CALLER();
}

/* ── sub_8125198 ── */
/* register form: calls battleObject_initChipSpriteSlotBuffer_8126818 outside this file, which has no typed C interface yet */
static void call_sub_8126818_zero(void) {
    R0 = 0;
    GUEST_CALL(battleObject_initChipSpriteSlotBuffer_8126818);
}

/* register form: calls battle_runTransition_8125444 outside this file, which has no typed C interface yet */
static void call_sub_8125444_sub_8125198(void) {
    GUEST_CALL(battle_runTransition_8125444);
}

/* register form: calls bg_drawArenaTiles_8125E78 outside this file, which has no typed C interface yet */
static void call_sub_8125E78_sub_8125198(void) {
    GUEST_CALL(bg_drawArenaTiles_8125E78);
}

/* register form: calls sub_8126184 outside this file, which has no typed C interface yet */
static void call_sub_8126184(void) {
    GUEST_CALL(sub_8126184);
}

/* register form: calls sprite_setStructHalfwordPairOnThree_8117084 outside this file,
 * which has no typed C interface yet */
static void call_sprite_set(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(sprite_setStructHalfwordPairOnThree_8117084);
}

/* Subtract 2 from the object's frame counter at +0x74, clamp it to a minimum of 0x6A,
 * finalize it and update two sprite frame slots from the object's animation state.
 *
 * Operates directly on the object memory at `addr` (R5 passed in). */
static void tick_object_frame(u32 addr) {
    call_sub_8126818_zero();

    u32 frame = LD32(addr + 0x74) - 2;
    ST32(addr + 0x74, frame);

    if ((s32)frame <= 0x6A) {
        ST32(addr + 0x74, 0x6A);
        ST8(addr + 2, 0x30);
    }

    ST32(addr + 0x70, 0xFF);

    call_sub_8125444_sub_8125198();
    call_sub_8125E78_sub_8125198();
    call_sub_8126184();

    /* First sprite slot (index 4): normally 0, but if the object kind is 5, use +0x26. */
    u32 kind1 = LD32(addr + 0x60);
    u32 r1 = 0;
    if (kind1 == 5)
        r1 = LD16(addr + 0x26);
    call_sprite_set(kind1, r1, 4);

    /* Second sprite slot (index 7): normally 0, but if the object kind is 0x27 use +0x1C;
     * and if the clamped frame is <= 0x60 and kind is 0x1D, use +0x3A instead. */
    u32 kind2 = LD32(addr + 0x68);
    r1 = 0;
    if (kind2 == 0x27)
        r1 = LD16(addr + 0x1C);
    u32 frame_after = LD32(addr + 0x74);
    if ((s32)frame_after <= 0x60) {
        r1 = 0;
        u32 kind2b = LD32(addr + 0x5C);
        if (kind2b == 0x1D)
            r1 = LD16(addr + 0x3A);
        kind2 = kind2b;
    }
    call_sprite_set(kind2, r1, 7);
}

Next sub_8125198(void) {
    tick_object_frame(R5);
    RETURN_TO_CALLER();
}

/* ── chatbox_flagResolve_8125254 ── */
#define EVENT_CHATBOX_FLAG 0x1711

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8125254(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();  /* Z set → flag clear; Z clear → flag set */
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void event_flag_clear(u16 flag) {
    R0 = flag >> 8;
    R1 = flag & 0xFF;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls chatbox_8040818 outside this file, which has no typed C interface yet */
static void chatbox_close(void) {
    GUEST_CALL(chatbox_8040818);
}

/* register form: calls chatbox_clear_eFlags2009F38 outside this file, which has no typed C interface yet */
static void chatbox_clear_eflags(u32 bits) {
    R0 = bits;
    GUEST_CALL(chatbox_clear_eFlags2009F38);
}

/* register form: calls pet_menuExitToMap_811F708 outside this file, which has no typed C interface yet */
static void pet_menu_exit_to_map(void) {
    GUEST_CALL(pet_menuExitToMap_811F708);
}

/* Handle the chatbox/pet-menu decision gated on EVENT_CHATBOX_FLAG.
 * If the flag is set: close the chatbox, clear eFlags bit 0x40, clear the flag, return 1.
 * Otherwise: exit the pet menu back to the map, return 0. */
static u32 handle_chatbox_flag_action(void) {
    if (event_flag_set_sub_8125254(EVENT_CHATBOX_FLAG)) {
        chatbox_close();
        chatbox_clear_eflags(0x40);
        event_flag_clear(EVENT_CHATBOX_FLAG);
        return 1;
    }
    pet_menu_exit_to_map();
    return 0;
}

/* chatbox_flagResolve_8125254() -> r0, nz (c,v left by last callee) */
Next chatbox_flagResolve_8125254(void) {
    R0 = handle_chatbox_flag_action();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── initRefs_buildAndDecompressTable_8125280 ── */
/* initRefs_buildAndDecompressTable_8125280: assemble the "init references" table in RAM at unk_2029000 from
 * the ROM pointer table at off_8125310, then decompress-and-copy the two data
 * blobs that follow it.
 *
 * The source table holds 5 entries of 4 words each (0x50 bytes total):
 *   word 0..1 - passed to getKeyedTableEntry_811FB84, whose result is stored instead
 *   word 2..3 - copied verbatim
 * A final 0 word terminates the assembled table. r4-r7 are preserved. */
Next initRefs_buildAndDecompressTable_8125280(void) {
    u32 src = A_off_8125310;  /* init-ref entries in ROM */
    u32 dst = A_unk_2029000;  /* assembled table in RAM */

    for (u32 entry = 0; entry < 0x50 / 16; entry++) {
        R0 = LD32(src);
        src += 4;
        R1 = LD32(src);
        src += 4;
        GUEST_CALL(getKeyedTableEntry_811FB84);
        ST32(dst, R0);
        dst += 4;

        for (u32 word = 0; word < 2; word++) {
            ST32(dst, LD32(src));
            dst += 4;
            src += 4;
        }
    }
    ST32(dst, 0);

    /* decompress the assembled table, then the blob described by it */
    R0 = A_unk_2029000;
    GUEST_CALL(decompAndCopyData);
    R0 = A_byte_8125368;
    GUEST_CALL(decompAndCopyData);

    RETURN_TO_CALLER();
}

/* ── sub_81252C0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r6, r7; } call_call_sub_3005EBA_result;
/* register form: generated wrapper: call_sub_3005EBA has no typed C interface yet */
static inline call_call_sub_3005EBA_result call_call_sub_3005EBA(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(call_sub_3005EBA);
    return (call_call_sub_3005EBA_result){ R6, R7 };
}

typedef struct { u32 r3, r6, r7; } call_CopyBackgroundTiles_result_sub_81252C0;
/* register form: generated wrapper: CopyBackgroundTiles has no typed C interface yet */
static inline call_CopyBackgroundTiles_result_sub_81252C0 call_CopyBackgroundTiles_sub_81252C0(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles);
    return (call_CopyBackgroundTiles_result_sub_81252C0){ R3, R6, R7 };
}

/* Loads background tile graphics and the static + scene-specific tilemaps into VRAM.
 * scene_ptr is the per-scene state pointer (r5 on entry); the tileset index for the
 * scene-specific map lives at [scene_ptr + 0x74]. */
static void load_scene_backgrounds(u32 scene_ptr) {
    /* register form: the original's "push {r4-r7, lr}" / "pop {r4-r7, pc}" means R4-R7
       must be preserved across the wrapper calls, which run the real callees and let them
       write those registers on the live guest state. Snapshot them and put them back. */
    u32 save_r4 = R4, save_r5 = R5, save_r6 = R6, save_r7 = R7;

    /* Blank two 32x32 tile-block slots (2 and 3) so the upcoming tilemap copies
       overwrite only the parts we want to keep. */
    call_call_sub_3005EBA(0, 2, 2, 0, 0x1E, 0x11);
    call_call_sub_3005EBA(0, 2, 3, 0, 0x1E, 2);

    /* Copy the fixed (scripted) background tilemap into VRAM. */
    call_CopyBackgroundTiles_sub_81252C0(0, 0, 1, A_eTextScript201BA20, 0x1E, 0x14);

    /* Copy the scene-specific tilemap; the tileset id source is (scene->0x74) - 0x95. */
    u32 tilemap = LD32(scene_ptr + 0x74) - 0x95;
    call_CopyBackgroundTiles_sub_81252C0(tilemap, 2, 2, A_eTextScript201BF20, 0x5B, 0x11);

    R4 = save_r4; R5 = save_r5; R6 = save_r6; R7 = save_r7;
}

/* sub_81252C0(r5 = scene ptr, r10, lr) -> r5, r6 preserved across the whole call */
Next sub_81252C0(void) {
    load_scene_backgrounds(R5);
    RETURN_TO_CALLER();
}

/* ── sub_812546C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3, r4, r6, r7; } call_CopyBackgroundTiles_result_sub_812546C;
/* register form: generated wrapper: CopyBackgroundTiles has no typed C interface yet */
static inline call_CopyBackgroundTiles_result_sub_812546C call_CopyBackgroundTiles_sub_812546C(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles);
    return (call_CopyBackgroundTiles_result_sub_812546C){ R3, R4, R6, R7 };
}

/* Offsets into the BG-object struct passed in r5. */
#define OBJ_TILE_BASE_OFF  0x74   /* base tile index from which row "j" is derived */
#define OBJ_ALT_FLAG_OFF   0x64   /* nonzero selects the alternate pair of tile-ID tables */

/* Copy 2x3 blocks of 32x32 tiles for the background graphic owned by obj.
 * Rows 0-2 are unconditional; rows 3-4 depend on the alt-flag word.
 * The original reloads the tile-base pointer before every call. */
static void redraw_object_tiles(u32 obj) {
    u32 base;

    /* Row 0 */
    base = LD32(obj + OBJ_TILE_BASE_OFF);
    call_CopyBackgroundTiles_sub_812546C(base - 0x94, 2, 3, A_byte_812489C, 9, 2);

    /* Row 1 */
    base = LD32(obj + OBJ_TILE_BASE_OFF);
    call_CopyBackgroundTiles_sub_812546C(base - 0x82, 2, 3, A_byte_81248C0, 9, 2);

    /* Row 2 */
    base = LD32(obj + OBJ_TILE_BASE_OFF);
    call_CopyBackgroundTiles_sub_812546C(base - 0x70, 2, 3, A_byte_81248E4, 9, 2);

    if (LD32(obj + OBJ_ALT_FLAG_OFF) == 0) {
        /* Row 3 – default tile set only */
        base = LD32(obj + OBJ_TILE_BASE_OFF);
        call_CopyBackgroundTiles_sub_812546C(base - 0x5E, 2, 3, A_byte_812492C, 9, 2);
    } else {
        /* Row 3 – alternate tile set */
        base = LD32(obj + OBJ_TILE_BASE_OFF);
        call_CopyBackgroundTiles_sub_812546C(base - 0x5E, 2, 3, A_byte_8124908, 9, 2);
        /* Row 4 – default tile set */
        base = LD32(obj + OBJ_TILE_BASE_OFF);
        call_CopyBackgroundTiles_sub_812546C(base - 0x4C, 2, 3, A_byte_812492C, 9, 2);
    }
}

/* sub_812546C(r5 = bg-object ptr, r10, lr) -> r3 r4 r5 r6 r7
 * r3/r4/r6/r7 come from the last CopyBackgroundTiles wrapper call.
 * r5 is saved/restored (original push/pop); r7 = original r5 (mov r7,r5 at entry). */
Next sub_812546C(void) {
    u32 obj = R5;
    redraw_object_tiles(obj);
    R5 = obj;
    R7 = obj;
    RETURN_TO_CALLER();
}

/* ── sub_81254FC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r8, r9, r10, r11, r12; } call_renderTextGfx_8045F8C_result;
/* register form: generated wrapper: renderTextGfx_8045F8C has no typed C interface yet */
static inline call_renderTextGfx_8045F8C_result call_renderTextGfx_8045F8C(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(renderTextGfx_8045F8C);
    return (call_renderTextGfx_8045F8C_result){ R2, R8, R9, R10, R11, R12 };
}

/* Draw the five pre-rendered text-graphic strips of the name/label box.
 *
 * Every strip is drawn with the same font buffer (unk_201CC20), glyph
 * width 9, one row, palette data dword_86B7AE0 and layer 0; only the
 * character index, the source glyph buffer and the VRAM destination
 * change.  The original draws them in the order 0, 1, 2, 5, 3 into
 * consecutive 0x400-byte VRAM slots. */
static call_renderTextGfx_8045F8C_result render_text_strips(void) {
    call_renderTextGfx_8045F8C_result last;

    /* strip 0 -> VRAM 0x06009000 */
    last = call_renderTextGfx_8045F8C(A_unk_201CC20, 0, A_unk_2018A00,  0x6009000, 9, 1, A_dword_86B7AE0, 0);
    /* strip 1 -> VRAM 0x06009400 */
    last = call_renderTextGfx_8045F8C(A_unk_201CC20, 1, A_unk_2018E00,  0x6009400, 9, 1, A_dword_86B7AE0, 0);
    /* strip 2 -> VRAM 0x06009800 */
    last = call_renderTextGfx_8045F8C(A_unk_201CC20, 2, A_unk_2019200,  0x6009800, 9, 1, A_dword_86B7AE0, 0);
    /* strip 5 -> VRAM 0x06009C00 */
    last = call_renderTextGfx_8045F8C(A_unk_201CC20, 5, A_unk_2019600,  0x6009C00, 9, 1, A_dword_86B7AE0, 0);
    /* strip 3 -> VRAM 0x0600A000 */
    last = call_renderTextGfx_8045F8C(A_unk_201CC20, 3, A_byte_2019A00, 0x600A000, 9, 1, A_dword_86B7AE0, 0);

    return last;
}

/* sub_81254FC: renders all five text strips.
 * The original pushes r4-r7 and pops them after the calls, so the caller's
 * r4-r7 come out unchanged; the renderTextGfx wrapper, however, leaves its
 * arguments (r6/r7) and its own scratch (r4/r5) behind in those registers,
 * so they are saved here and restored after the calls.
 * Outputs: r8-r12 are the register results of the final renderTextGfx call,
 * r5 is the caller's value (restored). r10 is read by the wrapper as on
 * entry and returned by it unchanged. */
Next sub_81254FC(void) {
    u32 r4_in = R4, r5_in = R5, r6_in = R6, r7_in = R7;
    call_renderTextGfx_8045F8C_result res = render_text_strips();
    R4 = r4_in, R5 = r5_in, R6 = r6_in, R7 = r7_in;
    R8 = res.r8, R9 = res.r9, R10 = res.r10, R11 = res.r11, R12 = res.r12;
    RETURN_TO_CALLER();
}

/* ── battle_input_tick_8125784 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r10; } call_menu_confirmExitAndFade_812580C_result;
/* register form: generated wrapper: menu_confirmExitAndFade_812580C has no typed C interface yet */
static inline call_menu_confirmExitAndFade_812580C_result call_menu_confirmExitAndFade_812580C(u32 r0, u32 r2, u32 r5) {
    R0 = r0;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(menu_confirmExitAndFade_812580C);
    return (call_menu_confirmExitAndFade_812580C_result){ R10 };
}

typedef struct { u32 r10; } call_sub_81266E0_result;
/* register form: generated wrapper: battle_cycleEntryGroupIndex has no typed C interface yet */
static inline call_sub_81266E0_result call_sub_81266E0(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_cycleEntryGroupIndex);
    return (call_sub_81266E0_result){ R10 };
}

typedef struct { u32 r10; } call_playSoundEffect_7A_81258E8_result;
/* register form: generated wrapper: playSoundEffect_7A_81258E8 has no typed C interface yet */
static inline call_playSoundEffect_7A_81258E8_result call_playSoundEffect_7A_81258E8(u32 r2) {
    R2 = r2;
    GUEST_CALL(playSoundEffect_7A_81258E8);
    return (call_playSoundEffect_7A_81258E8_result){ R10 };
}

typedef struct { u32 r5, r7, r9, r10, r12; } call_sub_8125820_result;
/* register form: generated wrapper: submenu_moveListCursor has no typed C interface yet */
static inline call_sub_8125820_result call_sub_8125820(u32 r0, u32 r1, u32 r2, u32 r5, u32 r8) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(submenu_moveListCursor);
    return (call_sub_8125820_result){ R5, R7, R9, R10, R12 };
}

/* battle_input_tick_8125784 — per-frame dispatch for the battle-style menu.
 *
 * Start+B           → confirm exit & fade
 * pressed bit 0     → dedicated handler (battle_cycleEntryGroupIndex)
 * Select held       → write state byte on battle object, play cue 7A
 * cursor field set  → try submenu_moveListCursor navigation when D-pad or A/B held,
 *                     then restore the temporarily-written 16-bit field.
 */

typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r12;
} battle_input_tick_result;

static battle_input_tick_result battle_input_tick(
    u32 held, u32 code_param, u32 obj, u32 toolkit, u32 r9_in, u32 r12_in)
{
    battle_input_tick_result res = { obj, held, r9_in, toolkit, r12_in };

    /* register form: original push {r4-r7,lr} / pop {r4-r7,pc} — GUEST_CALL
       wrappers may clobber R4-R7; save them once and restore before every
       return so the caller sees the entry values. */
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;

    u32 joy = LD32(toolkit + oToolkit_JoypadPtr);
    u16 pressed = LD16(joy + oJoypad_Pressed);

    /* Start+B pressed: fade out and exit the menu. */
    if (pressed & (JOYPAD_START | JOYPAD_B)) {
        call_menu_confirmExitAndFade_812580C(0x44, code_param, obj);
        R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
        return res;
    }

    /* Bit 0 of pressed set: hand off to dedicated handler. */
    if (pressed & 1) {
        call_sub_81266E0(obj);
        R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
        return res;
    }

    /* Select held (bit 5 of held-buttons field at joy+4): choose state byte
       based on obj+0x64 and play sound cue. */
    if (LD16(joy + 4) & 0x20) {
        ST8(obj + 2, (LD32(obj + 0x64) == 0) ? 0x4C : 0x2C);
        call_playSoundEffect_7A_81258E8(code_param);
        R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
        return res;
    }

    /* Cursor navigation: only when obj+0x68 (cursor ptr) is non-zero. */
    u32 cursor = LD32(obj + 0x68);
    if (cursor == 0) {
        R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
        return res;
    }

    /* Temporarily install cursor into 16-bit field at obj+0x3A,
       dispatch navigation via submenu_moveListCursor, then restore. */
    u16 saved_3a = LD16(obj + 0x3A);
    ST16(obj + 0x3A, (u16)cursor);

    u16 held2 = LD16(joy + 4);
    u16 pres2 = LD16(joy);

    if (held2 & 0x300) {
        /* D-pad held: clear A/B bits (mask off 0xC0) then dispatch. */
        call_sub_8125820_result sr =
            call_sub_8125820(held2 & ~0xC0, 7, 0, obj, held);
        res.r9  = sr.r9;
        res.r12 = sr.r12;
        ST32(sr.r5 + 0x70, sr.r7);
        ST16(sr.r5 + 0x3A, saved_3a);
    } else if (!(pres2 & 0x300) && (held2 & 0xC0)) {
        /* No D-pad press but A/B held: dispatch with unmodified held. */
        call_sub_8125820_result sr =
            call_sub_8125820(held2, 7, 0, obj, held);
        res.r9  = sr.r9;
        res.r12 = sr.r12;
        ST32(sr.r5 + 0x70, sr.r7);
        ST16(sr.r5 + 0x3A, saved_3a);
    } else {
        ST16(obj + 0x3A, saved_3a);
    }

    /* register form: restore R4-R7 (pop {r4-r7} equivalent) */
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
    return res;
}

Next battle_input_tick_8125784(void) {
    battle_input_tick_result res =
        battle_input_tick(R0, R2, R5, R10, R9, R12);
    R5  = res.r5;
    R8  = res.r8;
    R9  = res.r9;
    R10 = res.r10;
    R12 = res.r12;
    RETURN_TO_CALLER();
}

/* ── menu_confirmExitAndFade_812580C ── */
/* menu_confirmExitAndFade_812580C(r0 = value stored into the menu state at [r5+2]):
 * record the caller's selection, play the "exit submenu" sound effect,
 * then start a screen fade (fade kind 0xC, duration 0x10).
 * Only r0/r1/lr and the flags are clobbered; r3 and r10 pass through. */
Next menu_confirmExitAndFade_812580C(void) {
    u8 selection = (u8)R0;

    ST8(R5 + 2, selection);

    R0 = SOUND_EXIT_SUBMENU;
    GUEST_CALL(PlaySoundEffect);

    R0 = 0xC;
    R1 = 0x10;
    GUEST_CALL(SetScreenFade);

    RETURN_TO_CALLER();
}

/* ── playSoundEffect_7A_81258E8 ── */
/* playSoundEffect_7A_81258E8: play sound effect 0x7A. All other registers (r1-r7, r10)
 * pass through to the callee and are preserved unchanged. */
Next playSoundEffect_7A_81258E8(void) {
    R0 = 0x7A;
    GUEST_CALL(PlaySoundEffect);
    RETURN_TO_CALLER();
}

/* ── buildChipAvailabilityTable_81258F8 ── */
/* Chip availability table: 8 bytes per slot (u16 chip_id, u8 status, u8 pad) */
#define CHIP_TABLE_ADDR        A_unk_201DC20
#define CHIP_TABLE_SIZE        0x320
#define CHIP_COUNT_MAX_sub_81258F8         0x13F

/* Event-flag ranges used to classify chip ownership state */
#define CHIP_FLAG_OWNED        0x1E20   /* already in the player's library */
#define CHIP_FLAG_ACQUIRED     0x2220   /* newly acquired this run */
#define CHIP_FLAG_CAN_ACQUIRE  0x2020   /* available to acquire */
#define CHIP_FLAG_ALREADY_GOT  0x1711   /* "has ever been obtained" check */

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word(u32 addr, u32 size) {
    R0 = addr;
    R1 = size;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_81258F8(s32 chip_id) {
    R0 = (u32)chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_81258F8(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_from_immediate(u32 page, u32 bit) {
    R0 = page;
    R1 = bit;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

typedef struct {
    s32 count;
    u32 r2_out;   /* leftover r2 from the loop's last iteration (output register) */
} ChipTableResult;

/* Build the chip availability table in RAM.
 *
 * Iterates chips 1..319. A chip qualifies when:
 *   - byte 0x09 has bit 0x40 (special-type chip)
 *   - byte 0x16 bit 0 is clear (not disabled)
 *   - byte 0x07 == 0         (category zero only)
 *
 * For each qualifying chip the group index (byte 0x15 − 1) selects a 4-byte slot.
 * The slot stores the chip id (u16) and a status byte at offset +2:
 *   0 = already owned, 1 = newly acquired, 2 = acquirable but previously obtained.
 *
 * Returns the count written and the final value of r2 left by the loop
 * (0x40 when the last chip failed the special-bit test, 1 otherwise). */
static ChipTableResult build_chip_availability_table(void) {
    zero_fill_by_word(CHIP_TABLE_ADDR, CHIP_TABLE_SIZE);

    s32 count = 0;
    u32 r2 = 0;

    for (s32 chip = 1; chip <= CHIP_COUNT_MAX_sub_81258F8; chip++) {
        u32 data = get_chip_data_sub_81258F8(chip);

        /* Original sets r2 = 0x40 unconditionally after the call. */
        r2 = 0x40;
        if (!(LD8(data + 9) & 0x40))
            continue;

        /* Original sets r2 = 1 for the disabled-bit test. */
        r2 = 1;
        if (LD8(data + 0x16) & 1)
            continue;

        if (LD8(data + 7) != 0)
            continue;

        u32 group = (u32)LD8(data + 0x15) - 1;
        u32 entry = CHIP_TABLE_ADDR + (group << 2);
        count++;

        if (event_flag_set_sub_81258F8(CHIP_FLAG_OWNED + (u32)chip)) {
            ST16(entry, (u32)chip);
            ST8(entry + 2, 0);
        } else if (event_flag_set_sub_81258F8(CHIP_FLAG_ACQUIRED + (u32)chip)) {
            ST16(entry, (u32)chip);
            ST8(entry + 2, 1);
        } else if (event_flag_set_sub_81258F8(CHIP_FLAG_CAN_ACQUIRE + (u32)chip)) {
            ST16(entry, (u32)chip);
            ST8(entry + 2,
                event_flag_set_from_immediate(
                    CHIP_FLAG_ALREADY_GOT >> 8,
                    CHIP_FLAG_ALREADY_GOT & 0xFF) ? 2 : 1);
        }
    }

    return (ChipTableResult){count, r2};
}

/* buildChipAvailabilityTable_81258F8() -> r0 = chip count, r2 = loop leftover */
Next buildChipAvailabilityTable_81258F8(void) {
    ChipTableResult res = build_chip_availability_table();
    R0 = (u32)res.count;
    R2 = res.r2_out;
    RETURN_TO_CALLER();
}

/* ── buildChipCatalogByType_8125994 ── */
/* Collect available chips into a category-indexed buffer, then compact it.
 *
 * Scans chip IDs 1..0x13F. A chip qualifies if its kind (field 7) is 1,
 * it carries attribute bit 0x40 (field 9), is not hidden (field 0x16 bit 0),
 * and one of three event-flag ranges is unlocked for it. The chip ID and a
 * category byte (0, 1, or 2) are written into a per-type slot in the buffer
 * at dword_201E420. After the scan, empty (zero) slots are compacted away
 * by pulling subsequent entries forward.
 *
 * Returns the number of chips that passed the qualification tests. */

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_sub_8125994(u32 mem, u32 byte_size) {
    R0 = mem;
    R1 = byte_size;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 chip_data_ptr(s32 chip_id) {
    R0 = (u32)chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8125994(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool flag_1711_set(void) {
    R0 = 0x17;
    R1 = 0x11;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

static s32 scan_chips_into_buffer(void) {
    const u32 buf_base = A_dword_201E420;
    const u32 buf_end  = A_unk_201E998;
    const s32 MAX_CHIPS = 0x13F;

    /* 0x578 bytes = 350 four-byte slots, one per possible chip type */
    zero_fill_sub_8125994(buf_base, 0x578);

    s32 count = 0;

    for (s32 id = 1; id <= MAX_CHIPS; id++) {
        u32 data = chip_data_ptr(id);

        /* Slot offset in the buffer is determined by chip usage-type (field 0x15). */
        u32 slot_off   = ((u32)LD8(data + 0x15) - 1) << 2;
        u32 write_addr = buf_base + slot_off;

        /* Chip must have "available" attribute (bit 0x40 in field 9). */
        if ((LD8(data + 9) & 0x40) == 0)
            continue;

        /* Chip must not be hidden (bit 0 in field 0x16). */
        if ((LD8(data + 0x16) & 1) != 0)
            continue;

        /* Chip kind (field 7) must equal 1 (normal chip). */
        if ((u32)LD8(data + 7) != 1)
            continue;

        /* Three-tier event-flag gate, checked in priority order. */
        if (event_flag_set_sub_8125994((u32)(0x1E20 + id))) {
            /* Category 0: early availability flag unlocked. */
            count++;
            ST16(write_addr, (u16)id);
            ST8(write_addr + 2, 0);
        } else if (event_flag_set_sub_8125994((u32)(0x2220 + id))) {
            /* Category 1: mid-game flag unlocked. */
            count++;
            ST16(write_addr, (u16)id);
            ST8(write_addr + 2, 1);
        } else if (event_flag_set_sub_8125994((u32)(0x2020 + id))) {
            /* Category 1 or 2 depending on special flag 0x1711. */
            count++;
            ST16(write_addr, (u16)id);
            ST8(write_addr + 2, flag_1711_set() ? 2 : 1);
        }
    }

    /* Compact: for each zero slot, pull subsequent non-zero entries forward. */
    for (s32 i = 0; i < MAX_CHIPS; i++) {
        u32 pos = buf_base + (u32)(i * 4);
        if (LD32(pos) != 0)
            continue;

        u32 dst = pos;
        for (u32 src = pos + 4; src < buf_end; src += 4) {
            u32 val = LD32(src);
            if (val == 0)
                continue;
            ST32(dst, val);
            ST32(src, 0);
            dst += 4;
        }
    }

    return count;
}

/* buildChipCatalogByType_8125994() -> r0 = chip count, r2 = 0 (left by compaction loop) */
Next buildChipCatalogByType_8125994(void) {
    R0 = (u32)scan_chips_into_buffer();
    R2 = 0;   /* the compaction inner loop always leaves r2 cleared */
    RETURN_TO_CALLER();
}

/* ── packBuildChipLibraryTable_8125A6C ── */
/* Field offsets within the ChipData struct (base returned by getChip8021DA8). */
#define oChipData_PackType   0x15
#define oChipData_Flags      9
#define oChipData_Type       7

#define CHIP_COUNT_MAX_sub_8125A6C       0x13F
#define PACK_DATA_BASE       A_dword_201EC20
#define PACK_DATA_SIZE       0x578
#define PACK_DATA_END        A_unk_201F198

/* Event-flag page bases (per-chip offset = 0x1E20 + idx, etc.). */
#define FLAG_READY_BASE      0x1E20
#define FLAG_USED_BASE       0x2220
#define FLAG_UNUSED_BASE     0x2020
#define SPECIAL_FLAG_PAGE    0x17
#define SPECIAL_FLAG_OFFSET  0x11

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_sub_8125A6C(u32 addr, u32 size) {
    R0 = addr;
    R1 = size;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_ptr(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_8125A6C(u16 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_from_pair(u8 page, u8 offset) {
    R0 = page;
    R1 = offset;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/*
 * Build the chip-pack availability table for the chip library screen.
 *
 * Zero the destination buffer, then scan every chip (1..0x13F).  A chip
 * qualifies if its flags byte has bit 6 set and its type field equals 2.
 * Each qualifying chip is placed into a slot determined by (PackType - 1),
 * storing the chip index (u16) and a readiness code (u8 at +2):
 *   0 – "ready"     (flag base 0x1E20)
 *   1 – "used"      (flag base 0x2220) or upgrade when special flag is set
 *   2 – "unused" + special flag EVENT_17:11 is set
 *
 * After filling, compact the slot array so non-zero entries move to the
 * front with gaps removed.
 *
 * Returns the number of accepted entries.  r2 after compaction is always 0
 * (the inner loop's last write to r2 is either a loaded zero or an explicit
 * `mov r2, #0`).
 */
static u32 build_pack_data(void) {
    zero_fill_sub_8125A6C(PACK_DATA_BASE, PACK_DATA_SIZE);

    u32 count = 0;

    for (u32 idx = 1; idx <= CHIP_COUNT_MAX_sub_8125A6C; idx++) {
        u32 chip = get_chip_ptr(idx);
        u32 slot_off = ((u32)LD8(chip + oChipData_PackType) - 1) << 2;
        u32 flags = LD8(chip + oChipData_Flags);

        if (!(flags & 0x40))
            continue;

        u32 slot_addr = PACK_DATA_BASE + slot_off;

        if ((u32)LD8(chip + oChipData_Type) != 2)
            continue;

        if (event_flag_set_sub_8125A6C(FLAG_READY_BASE + idx)) {
            count++;
            ST16(slot_addr, idx);
            ST8(slot_addr + 2, 0);
        } else if (event_flag_set_sub_8125A6C(FLAG_USED_BASE + idx)) {
            count++;
            ST16(slot_addr, idx);
            ST8(slot_addr + 2, 1);
        } else if (event_flag_set_sub_8125A6C(FLAG_UNUSED_BASE + idx)) {
            count++;
            ST16(slot_addr, idx);
            ST8(slot_addr + 2,
                event_flag_set_from_pair(SPECIAL_FLAG_PAGE, SPECIAL_FLAG_OFFSET) ? 2 : 1);
            /* The original re-reads the stored halfword and skips advancing
             * the write cursor only when it is zero; since idx >= 1 it is
             * never zero, so the "add r6, #4" path is unreachable here. */
        }
    }

    /* Compact: for each empty slot, shift the next non-zero entry into it. */
    for (u32 i = 0; i < CHIP_COUNT_MAX_sub_8125A6C; i++) {
        u32 cur = PACK_DATA_BASE + i * 4;
        if (LD32(cur) != 0)
            continue;

        u32 dst = cur;
        for (u32 src = cur + 4; (s32)src < (s32)PACK_DATA_END; src += 4) {
            u32 val = LD32(src);
            if (val == 0)
                continue;
            ST32(dst, val);
            ST32(src, 0);
            dst += 4;
        }
    }

    return count;
}

/* packBuildChipLibraryTable_8125A6C() -> r0 = accepted chip count, r2 = 0 (compaction scratch).
 * r3 is a callee side-effect from the last TestEventFlag/…FromImmediate call.
 * r4-r7 are preserved by the original's push/pop (never modified here). */
Next packBuildChipLibraryTable_8125A6C(void) {
    R0 = build_pack_data();
    R2 = 0;
    RETURN_TO_CALLER();
}

/* ── library_rebuildCollectedChipList_8125B3C ── */
/* Chip collection list at dword_201F420: one u32 entry per library slot,
 * halfword chip id at +0, "how obtained" byte at +2, 0x578 bytes total
 * (list end = unk_201F998). Standard chip ids run 1..0x13F. */
#define CHIP_LIST         A_dword_201F420
#define CHIP_LIST_END     A_unk_201F998
#define CHIP_LIST_BYTES   0x578
#define STANDARD_CHIP_MAX 0x13F

/* "Which libara page registered this chip" flag, passed to the immediate
 * form of the event-flag test as page 0x17 / bit 0x11. */
#define EVENT_1711 0x1711

/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word_sub_8125B3C(u32 dest, u32 num_bytes) {
    R0 = dest;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 chip_data_sub_8125B3C(u32 which_chip) {
    R0 = which_chip;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static u8 event_flag_set_sub_8125B3C(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static u8 immediate_event_flag_set(u16 flag) {
    R0 = flag >> 8;             /* immediate form takes the flag as page/high byte in r0, bit/low byte in r1 */
    R1 = flag & 0xFF;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

typedef struct {
    u32 count;        /* -> r0: number of entries collected */
    u32 scratch_r2;   /* -> r2: value the original leaves behind in r2 */
} ChipListResult;

/* Rebuild the player's collected-chip list: clear it, then for every standard
 * chip that is owned (chip data +9 bit 0x40) and registered (+0x16 bit 1),
 * record chip id and source byte at the slot named by the chip's library
 * index (+0x15, 1-based); the source byte says which "obtained" event flag
 * answered (0x1E20+id, 0x2220+id, or 0x2020+id, the last split by EVENT_1711).
 * Afterwards slide entries down so the list is left without gaps.
 * r3 is left exactly as the last event-flag probe left it: the typed code
 * never touches R3, so the GUEST_CALL leftover stands in for the original's. */
static ChipListResult rebuild_chip_collection_list(void) {
    u32 list = CHIP_LIST;
    zero_fill_by_word_sub_8125B3C(list, CHIP_LIST_BYTES);

    u32 count = 0;
    u32 scratch = 0x40;  /* r2 in the original; its final value is visible to callers */
    for (u32 chip = 1; chip <= STANDARD_CHIP_MAX; chip++) {
        u32 data = chip_data_sub_8125B3C(chip);
        u32 lib_slot = LD8(data + 0x15);          /* library index, 1-based */
        u32 entry = list + (lib_slot - 1) * 4;
        scratch = 0x40;                           /* value the original's bit-test immediate leaves in r2 */
        if ((LD8(data + 9) & 0x40) == 0)
            continue;                             /* chip not owned */
        scratch = 1;
        if ((LD8(data + 0x16) & 1) == 0)
            continue;                             /* chip not registered */
        scratch = entry;
        if (event_flag_set_sub_8125B3C(0x1E20 + chip)) {
            count++;
            ST16(entry, chip);
            ST8(entry + 2, 0);
        } else if (event_flag_set_sub_8125B3C(0x2220 + chip)) {
            count++;
            ST16(entry, chip);
            ST8(entry + 2, 1);
        } else if (event_flag_set_sub_8125B3C(0x2020 + chip)) {
            count++;
            ST16(entry, chip);
            u8 source;
            if (immediate_event_flag_set(EVENT_1711))
                source = 2;
            else
                source = 1;
            ST8(entry + 2, source);
        }
    }

    /* Compact: over every empty slot, pull the following entries down. */
    u32 slot = list;
    for (u32 i = 0; i < STANDARD_CHIP_MAX; i++) {
        if (LD32(slot) == 0) {
            u32 dst = slot;
            for (u32 src = slot + 4; src < CHIP_LIST_END; src += 4) {
                u32 word = LD32(src);
                scratch = word;
                if (word != 0) {
                    ST32(dst, word);
                    ST32(src, 0);
                    dst += 4;
                    scratch = 0;
                }
            }
        }
        slot += 4;
    }

    ChipListResult result;
    result.count = count;
    result.scratch_r2 = scratch;
    return result;
}

/* library_rebuildCollectedChipList_8125B3C() -> r0 = number of collected chips, r2 = leftover scratch.
 * r3 keeps whatever the last event-flag probe left in it; r4-r7 (including
 * the entry count in r5) are popped unchanged, no flags are returned. */
Next library_rebuildCollectedChipList_8125B3C(void) {
    ChipListResult result = rebuild_chip_collection_list();
    R0 = result.count;
    R2 = result.scratch_r2;
    RETURN_TO_CALLER();
}

/* ── buildComChipFilter_8125C08 ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_sub_8125C08(u32 addr, u32 bytes) {
    R0 = addr;
    R1 = bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet.
 * Preserves r3-r7; clobbers r1, r2 (both overwritten by caller). */
static u32 get_chip_data_sub_8125C08(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet.
 * Preserves r2, r4-r7; clobbers r3 (returned so the adapter can reproduce it). */
static bool event_flag_test(u32 flag, u32 *out_r3) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    *out_r3 = R3;
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet.
 * Preserves r2, r4-r7; clobbers r3 (returned). */
static bool event_flag_immediate(u32 page, u32 idx, u32 *out_r3) {
    R0 = page;
    R1 = idx;
    GUEST_CALL(TestEventFlagFromImmediate);
    *out_r3 = R3;
    return !ZERO_FLAG();
}

typedef struct {
    u32 count;
    u32 r2;
    u32 r3;
} FilterResult;

/* Build the COM-chip availability filter.
 * Scans chips 0x140..0x15E; for each COM-type chip whose event flag is set,
 * records the chip ID and a status byte in a packed slot.  Then compacts
 * the word array by shifting non-zero entries toward the front.
 * Returns chip count (r0), final r2 (compaction scratch), final r3 (loop scratch). */
static FilterResult build_chip_filter(void) {
    const u32 base = A_dword_201FC20;
    zero_fill_sub_8125C08(base, 0x578);

    u32 count = 0;
    u32 r2 = 0;
    u32 r3 = 0x140;

    /* Main pass: scan 31 chips (0x140..0x15E) */
    for (s32 i = 0; i <= 0x1E; i++) {
        u32 chip_id = 0x140 + i;
        r3 = 0x140;

        u32 data = get_chip_data_sub_8125C08(chip_id);

        u32 kind = LD8(data + 0x15);
        r2 = ((kind - 1) << 2) + base;

        if (LD8(data + 7) != 4)
            continue;

        /* COM chip: check three flag ranges */
        if (event_flag_test(0x1F60 + i, &r3)) {
            count++;
            r3 = chip_id;
            ST16(r2, r3);
            ST8(r2 + 2, 0);
            continue;
        }

        u32 flag_a = 0x2220 + i + (0x1F60 - 0x1E20);
        if (event_flag_test(flag_a, &r3)) {
            count++;
            r3 = chip_id;
            ST16(r2, r3);
            ST8(r2 + 2, 1);
            continue;
        }

        u32 flag_b = 0x2020 + i + (0x1F60 - 0x1E20);
        if (!event_flag_test(flag_b, &r3))
            continue;

        count++;
        r3 = chip_id;
        ST16(r2, r3);

        u32 status = event_flag_immediate(0x17, 0x11, &r3) ? 2 : 1;
        ST8(r2 + 2, status);
    }

    /* Compaction pass: shift non-zero words forward, zero the tail */
    const u32 end = A_unk_2020198;
    for (s32 i = 0; i < 0x13F; i++) {
        u32 pos = base + i * 4;
        if (LD32(pos) != 0)
            continue;

        u32 dest = pos;
        for (u32 src = pos + 4; (s32)src < (s32)end; src += 4) {
            r2 = LD32(src);
            if (r2 == 0)
                continue;
            ST32(dest, r2);
            ST32(src, 0);
            dest += 4;
            r2 = 0;
        }
    }

    FilterResult res = { count, r2, r3 };
    return res;
}

/* buildComChipFilter_8125C08(r2 r3 r4 r5 r6 r7 r10 lr in) -> (r0 r2 r3 r4 r5 r6 r7 out; r4-r7 preserved) */
Next buildComChipFilter_8125C08(void) {
    FilterResult res = build_chip_filter();
    R0 = res.count;
    R2 = res.r2;
    R3 = res.r3;
    RETURN_TO_CALLER();
}
