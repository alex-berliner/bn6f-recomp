/*
 * asm37_1.c: readable functions of bn6f asm37_1.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── eventStruct_setActiveFlag_81418AA ── */
/* eventStruct_setActiveFlag_81418AA: raise the "event struct active" flag byte (0x200101D <- 1) and
 * return 0 with Z set (the final Thumb "mov r0,#0" sets N/Z).
 * r4/r5/r7 are pushed and popped unchanged, so the caller's values survive. */
static void set_event_struct_flag(void) {
    /* ldr r5, =eStruct2001010 ; strb 1, [r5, #0xD] -- the flag byte is the
     * second byte of dword_200101C, i.e. eStruct2001010 + 0xD. */
    ST8(A_dword_200101C + 1, 1);
}

Next eventStruct_setActiveFlag_81418AA(void) {
    set_event_struct_flag();
    SET_FLAGS_NZ(0);   /* mov r0, #0: Z set, N clear */
    RETURN_TO_CALLER();
}

/* ── clear_eStruct2001010_flagByte0D_81418B6 ── */
/* clear_eStruct2001010_flagByte0D_81418B6 clears one status byte in the fixed struct at 0x2001010:
 * the byte at offset 0x0D (labelled dword_200101C+1 in the disassembly)
 * is written with zero. The pushed r4-r7 are popped back untouched, and
 * the final "mov r0, #0" leaves Z set and N clear for the caller. */

/* r4/r5/r7 are listed as outputs but the original restores them from the
 * stack with their entry values, so they are passed in and passed back. */
typedef struct { u32 r4, r5, r7; } SavedRegs;

static SavedRegs clear_struct_flag_byte(u32 r4, u32 r5, u32 r7) {
    ST8(A_eStruct2001010 + 0xD, 0);
    SavedRegs kept;
    kept.r4 = r4;
    kept.r5 = r5;
    kept.r7 = r7;
    return kept;
}

/* clear_eStruct2001010_flagByte0D_81418B6() -> nz, r4, r5, r7 (r0 clobbered) */
Next clear_eStruct2001010_flagByte0D_81418B6(void) {
    SavedRegs kept = clear_struct_flag_byte(R4, R5, R7);
    R4 = kept.r4;
    R5 = kept.r5;
    R7 = kept.r7;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── battle_getShiftedEnemyTableByte_81420B0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_sub_8142080_result;
/* register form: generated wrapper: battle_getEnemyIndexFromMapGroup_8142080 has no typed C interface yet */
static inline call_sub_8142080_result call_sub_8142080(void) {
    GUEST_CALL(battle_getEnemyIndexFromMapGroup_8142080);
    return (call_sub_8142080_result){ R0 };
}

/* Look up a byte from the table at dword_81420C4 using the value battle_getEnemyIndexFromMapGroup_8142080
 * computes (from r10), and shift it into the top byte of the result. */
static u32 pick_shifted_table_entry(u32 index_source) {
    u32 index = call_sub_8142080().r0;
    return (u32)LD8(A_dword_81420C4 + index) << 16;
}

/* battle_getShiftedEnemyTableByte_81420B0: r10 -> r0 (r4-r7 preserved) */
Next battle_getShiftedEnemyTableByte_81420B0(void) {
    R0 = pick_shifted_table_entry(R10);
    RETURN_TO_CALLER();
}

/* ── scenarioEffectAdvanceStep_8142178 ── */
/* scenarioEffectAdvanceStep_8142178: bump the scenario effect step counter (byte at 0x200078E).
 * Increments the counter by one and stores it back; the byte store wraps
 * naturally at 8 bits. r4-r7 are untouched (the original only pushes/pops
 * them), and r0/flags are clobbered. */
static void scenario_effect_advance_step(void) {
    u32 counter = LD8(A_byte_200078E);

    ST8(A_byte_200078E, counter + 1);
}

/* scenarioEffectAdvanceStep_8142178() -> nothing (r4-r7 preserved) */
Next scenarioEffectAdvanceStep_8142178(void) {
    scenario_effect_advance_step();
    RETURN_TO_CALLER();
}

/* ── getField0x18OfScenarioEffectState2000780_81421e0 ── */
/* Reads the field at offset 0x18 (byte_2000798) of the scenario effect state
 * at eScenarioEffectState2000780. The original only clobbers r0; r4-r7 are
 * pushed and popped unchanged, so they pass through untouched. */
static u32 read_scenario_effect_state_field_18(void) {
    return LD32(A_eScenarioEffectState2000780
                + (A_byte_2000798 - A_eScenarioEffectState2000780));
}

/* inputs: lr (unused)   outputs: r0 (r4-r7 preserved) */
Next getField0x18OfScenarioEffectState2000780_81421e0(void) {
    R0 = read_scenario_effect_state_field_18();
    RETURN_TO_CALLER();
}

/* ── ow_getPlayerTileMapIndex_8142990 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3; } call_ReadOWPlayerObjectCoords_result;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result call_ReadOWPlayerObjectCoords(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result){ R0, R1, R2, R3 };
}

typedef struct { u32 r0; } call_sub_8142816_result;
/* register form: generated wrapper: ow_getCursorMapTileIndex has no typed C interface yet */
static inline call_sub_8142816_result call_sub_8142816(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ow_getCursorMapTileIndex);
    return (call_sub_8142816_result){ R0 };
}

/* ow_getPlayerTileMapIndex_8142990: fetch the overworld player object's coordinates, then hand the
 * first two of them to ow_getCursorMapTileIndex and return its result.
 * r4-r7 are only saved/restored around the calls, so they end unchanged. */
static u32 get_player_coords_and_process(void) {
    call_ReadOWPlayerObjectCoords_result coords = call_ReadOWPlayerObjectCoords();
    return call_sub_8142816(coords.r0, coords.r1).r0;
}

/* ow_getPlayerTileMapIndex_8142990(r10 = context) -> r0; r4-r7 preserved */
Next ow_getPlayerTileMapIndex_8142990(void) {
    R0 = get_player_coords_and_process();
    RETURN_TO_CALLER();
}

/* ── battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5; } call_battleObject_scenarioEffectStateAddr_81421D8_result;
/* register form: generated wrapper: battleObject_scenarioEffectStateAddr_81421D8 has no typed C interface yet */
static inline call_battleObject_scenarioEffectStateAddr_81421D8_result call_battleObject_scenarioEffectStateAddr_81421D8(void) {
    GUEST_CALL(battleObject_scenarioEffectStateAddr_81421D8);
    return (call_battleObject_scenarioEffectStateAddr_81421D8_result){ R0, R4, R5 };
}

typedef struct { bool c, v; } call_ZeroFillByWord_result;
/* register form: generated wrapper: ZeroFillByWord has no typed C interface yet */
static inline call_ZeroFillByWord_result call_ZeroFillByWord_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByWord);
    return (call_ZeroFillByWord_result){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r5, r6, r7; } call_sub_8142990_result;
/* register form: generated wrapper: ow_getPlayerTileMapIndex_8142990 has no typed C interface yet */
static inline call_sub_8142990_result call_sub_8142990(void) {
    GUEST_CALL(ow_getPlayerTileMapIndex_8142990);
    return (call_sub_8142990_result){ R0, R5, R6, R7 };
}

typedef struct { u32 r0, r4, r5, r6; } call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA;
/* register form: generated wrapper: battle_getScenarioEffectStateField_81421D0 has no typed C interface yet */
static inline call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA call_battle_getScenarioEffectStateField_81421D0_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA(void) {
    GUEST_CALL(battle_getScenarioEffectStateField_81421D0);
    return (call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA){ R0, R4, R5, R6 };
}

/* register form: generated wrapper: GetOWPlayerFacingDirection has no typed C interface yet */
static inline u32 call_GetOWPlayerFacingDirection(void) {
    GUEST_CALL(GetOWPlayerFacingDirection);
    return R0;
}

/* Initialize a 4-entry (16-byte) player-offset matrix for a scenario-effect
 * camera. Each table is indexed by a scenario-effect state field (0..3) and
 * offset by the player's even facing (0,2,4,6). Records are 8 bytes; only the
 * first two s8 bytes (dx,dy) are read, and a dx of 0x7F terminates the stream.
 *
 * For every (dx,dy) pair the function combines it with a per-component base
 * word (low byte → x, high byte → y), producing a 16-bit packed offset written
 * into the zero-filled destination buffer.
 *
 * Returns the dy byte of the terminating sentinel (always 0x7F in practice)
 * which the original leaves in r3 on exit. */
static u32 init_player_offset_matrix(void) {
    /* Obtain the 16-byte destination buffer (scenario-effect state slot) */
    call_battleObject_scenarioEffectStateAddr_81421D8_result state =
        call_battleObject_scenarioEffectStateAddr_81421D8();
    u32 dest = state.r0;

    /* Zero-initialize the buffer */
    call_ZeroFillByWord_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA(dest, 0x10);

    /* Base word: low byte seeds x, upper bytes seed y */
    call_sub_8142990_result base = call_sub_8142990();
    u32 mode_word = base.r0;

    /* Select one of the four facing-direction offset tables */
    call_battle_getScenarioEffectStateField_81421D0_result_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA sel =
        call_battle_getScenarioEffectStateField_81421D0_battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA();
    u32 table_index = sel.r0;

    /* Table-of-pointers at off_8142A1C: word at [index*4] is the record base */
    u32 table_base = LD32(A_off_8142A1C + (table_index << 2));

    /* Facing: mask to even (0,2,4,6) to pick a 2-byte-aligned sub-record */
    u32 facing = call_GetOWPlayerFacingDirection();
    u32 p = table_base + ((facing >> 1) << 1);

    /* Walk 8-byte records; read (dx,dy) s8 pair; stop when dx == 0x7F */
    for (;;) {
        s32 dx = LDS8(p);
        s32 dy = LDS8(p + 1);
        if (dx == 0x7F)
            return (u32)dy;

        u32 low  = (mode_word & 0xFF) + (u32)dx;
        u32 high = ((mode_word >> 8) + (u32)dy) << 8;
        ST32(dest, low | high);

        dest += 4;
        p += 8;
    }
}

/* battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA:
 *   inputs:  r10 lr  (r10 passes through to callees)
 *   outputs: r3  (dy byte of terminating sentinel)
 *            r5 is listed as output+preserved; the original's pop restores it,
 *            so we do not write R5.
 *   preserved: r4 r5 r6 r7 (restored by the original's push/pop; wrapper calls
 *            modify them internally but the framework accounts for that). */
Next battleObject_scenarioEffectInitPlayerOffsetMatrix_81429CA(void) {
    R3 = init_player_offset_matrix();
    RETURN_TO_CALLER();
}

/* ── init_queueBeastSpGfx ── */
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

/* Queue a VRAM transfer that uploads the Beast-Out SP sprite graphics.
 * The callee leaves the queued transfer's tracking values in r4/r7, which the
 * caller of this function expects in its own r4/r7, so we pass them through.
 * Returns { r4, r5, r6, r7 } as left by QueueEightWordAlignedGFXTransfer. */
static call_QueueEightWordAlignedGFXTransfer_result queue_beast_sp_gfx(void) {
    return call_QueueEightWordAlignedGFXTransfer(A_dword_8619730, 0x6006C00, 0x420);
}

/* init_queueBeastSpGfx() -> r4 r7 (from QueueEightWordAlignedGFXTransfer) */
Next init_queueBeastSpGfx(void) {
    call_QueueEightWordAlignedGFXTransfer_result res = queue_beast_sp_gfx();
    R4 = res.r4;
    R7 = res.r7;
    RETURN_TO_CALLER();
}

/* ── ow_getAreaMapTilesOffset ── */
/* Tile-offset lookup table for area maps: one halfword per map area. */
#define AREA_MAP_TILE_OFFSETS A_byte_8142E0C

/* Return the tile offset used to draw the area map for the current map.
 * Map group 0x85 is the special Sky Area, which always uses entry 3;
 * every other group uses its own area id stored at GameState+5.
 * Reads: r10 = toolkit, [toolkit + oToolkit_GameStatePtr] = game state. */
static u32 ow_getAreaMapTilesOffset_impl(u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);
    u32 map_group  = LD8(game_state + oGameState_MapGroup);

    u32 area_id;
    if (map_group == 0x85)
        area_id = 3;                                  /* Sky Area */
    else
        area_id = LD8(game_state + 5);                /* current area number */

    return LD16(AREA_MAP_TILE_OFFSETS + (area_id << 1));
}

/* ow_getAreaMapTilesOffset(r10 = toolkit) -> r0 */
Next ow_getAreaMapTilesOffset(void) {
    R0 = ow_getAreaMapTilesOffset_impl(R10);
    RETURN_TO_CALLER();
}
