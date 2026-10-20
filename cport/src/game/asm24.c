/*
 * asm24.c: readable functions of bn6f asm24.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── ai_testMapGroupEventFlag ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_ai_testMapGroupEventFlag(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result call_TestEventFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Check the event flag associated with the current map group.
 *
 * ai_getAIModeForMapGroup_808D084 returns an AI mode index for the map group
 * (r10); that index selects a u16 flag id from a table at byte_808CF84, and
 * the flag is tested. Returns 1 when the flag is set, 0 otherwise.
 * The wrapper also returns the entry r7 unchanged (an output the original
 * leaves alone on every path, since it only pushes/pops it). */
typedef struct {
    u32 result;   /* -> r0: 1 if the map group's event flag is set, else 0 */
    u32 r7;       /* -> r7: passed through unchanged */
} MapGroupFlagCheck;

static MapGroupFlagCheck test_map_group_event_flag(u32 r7) {
    MapGroupFlagCheck out;

    u32 mode = call_ai_getAIModeForMapGroup_808D084_ai_testMapGroupEventFlag();
    u16 flag = LD16(A_byte_808CF84 + (mode << 1));

    out.result = 0;
    if (!call_TestEventFlag(flag).z)    /* TestEventFlag: Z clear when the flag is set */
        out.result = 1;

    out.r7 = r7;
    return out;
}

Next ai_testMapGroupEventFlag(void) {
    MapGroupFlagCheck out = test_map_group_event_flag(R7);
    R0 = out.result;
    R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── battleObject_computePanelOffsetFromCoords_808D034 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1; } call_ai_getAIModeForChipForMapGroup_808D060_result;
/* register form: generated wrapper: ai_getAIModeForChipForMapGroup_808D060 has no typed C interface yet */
static inline call_ai_getAIModeForChipForMapGroup_808D060_result call_ai_getAIModeForChipForMapGroup_808D060(void) {
    GUEST_CALL(ai_getAIModeForChipForMapGroup_808D060);
    return (call_ai_getAIModeForChipForMapGroup_808D060_result){ R0, R1 };
}

/* Result of battleObject_computePanelOffsetFromCoords: the object's panel
 * coordinates (also stored into the object) plus the flat panel lookup index.
 * r4 is carried through unchanged (the original saves/restores it around the
 * body, so callers see their own incoming r4 again). */
typedef struct {
    u32 r0; /* panel X */
    u32 r1; /* panel Y */
    u32 r2; /* panel index: X * 0x100 + Y */
    u32 r4; /* unchanged incoming r4 */
} PanelOffsetResult;

/* Compute a battle object's panel position from its fixed-point coordinates.
 *
 * ai_getAIModeForChipForMapGroup (found via the battle state in r10) returns a
 * pair of 16.16 fixed-point offsets. Their high halves are added to the high
 * halves of the coordinates, the sums are scaled down to panel units (>> 5)
 * and centered by +0x80. The byte-sized results are stored into the object
 * (obj+8 = X, obj+9 = Y) and the flat index X*256 + Y is built for panel
 * lookups. */
static PanelOffsetResult battleObject_computePanelOffsetFromCoords_impl(
    u32 coordX, u32 coordY, u32 obj, u32 savedR4)
{
    call_ai_getAIModeForChipForMapGroup_808D060_result offset =
        call_ai_getAIModeForChipForMapGroup_808D060();

    u32 sumX = offset.r0 + (u32)((s32)coordX >> 16);
    u32 sumY = offset.r1 + (u32)((s32)coordY >> 16);

    u32 panelX = (u32)((s32)sumX >> 5) + 0x80;
    u32 panelY = (u32)((s32)sumY >> 5) + 0x80;

    ST8(obj + 8, panelX);
    ST8(obj + 9, panelY);

    PanelOffsetResult result;
    result.r0 = panelX;
    result.r1 = panelY;
    result.r2 = panelX * 0x100 + panelY;
    result.r4 = savedR4;
    return result;
}

/* battleObject_computePanelOffsetFromCoords_808D034
 *   inputs:  r0 = coordX, r1 = coordY, r5 = object, r10 = battle state
 *   outputs: r0 = panel X, r1 = panel Y, r2 = panel index
 *            r4, r5 preserved (the original pops them back from its prologue)
 *   (nz/c/v are clobbered, callers do not read them) */
Next battleObject_computePanelOffsetFromCoords_808D034(void) {
    PanelOffsetResult result =
        battleObject_computePanelOffsetFromCoords_impl(R0, R1, R5, R4);
    R0 = result.r0;
    R1 = result.r1;
    R2 = result.r2;
    R4 = result.r4;
    RETURN_TO_CALLER();
}

/* ── ai_getAIModeForChipForMapGroup_808D060 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_ai_getAIModeForChipForMapGroup_808D060(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

/* AI mode table: for each map-group AI mode, a pair of signed chip-selection
 * parameters (offset 0 and offset 1 into byte_808D07C). */
#define AI_MODE_TABLE A_byte_808D07C

typedef struct {
    s32 mode_a;   /* returned in r0 */
    s32 mode_b;   /* returned in r1 */
} AIModeChipPair;

/* Fetch the chip-selection parameter pair for the current map group's AI mode.
 * ai_getAIModeForMapGroup_808D084 returns the mode index (r10 -> r0); each
 * table entry is two signed bytes, loaded with sign extension. */
static AIModeChipPair ai_getAIModeChipPair(void) {
    u32 mode = call_ai_getAIModeForMapGroup_808D084_ai_getAIModeForChipForMapGroup_808D060();
    u32 entry = AI_MODE_TABLE + mode * 2;

    AIModeChipPair pair;
    pair.mode_a = LDS8(entry + 0);
    pair.mode_b = LDS8(entry + 1);
    return pair;
}

/* ai_getAIModeForChipForMapGroup_808D060: r10 -> r0 = mode param 0, r1 = mode param 1 */
Next ai_getAIModeForChipForMapGroup_808D060(void) {
    AIModeChipPair pair = ai_getAIModeChipPair();
    R0 = (u32)pair.mode_a;
    R1 = (u32)pair.mode_b;
    RETURN_TO_CALLER();
}

/* ── ai_getAIModeForMapGroup_808D084 ── */
/* AI mode lookup: map group 0x85 (special area) always uses mode 3,
 * otherwise the mode is read from the game state's AI-mode byte. */
static u32 get_ai_mode_for_map_group(u32 toolkit) {
    u32 game_state = LD32(toolkit + oToolkit_GameStatePtr);

    if (LD8(game_state + oGameState_MapGroup) == 0x85)
        return 3;
    return LD8(game_state + 5);
}

/* ai_getAIModeForMapGroup_808D084(r10 = toolkit) -> r0 */
Next ai_getAIModeForMapGroup_808D084(void) {
    R0 = get_ai_mode_for_map_group(R10);
    RETURN_TO_CALLER();
}
