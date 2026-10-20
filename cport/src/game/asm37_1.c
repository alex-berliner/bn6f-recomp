/*
 * asm37_1.c: readable functions of bn6f asm37_1.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

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
