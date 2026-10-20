/*
 * asm24.c: readable functions of bn6f asm24.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── spawnOverworldMapObjects_808CBA0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r6; } call_SpawnOverworldMapObject_result_sub_808CBA0;
/* register form: generated wrapper: SpawnOverworldMapObject has no typed C interface yet */
static inline call_SpawnOverworldMapObject_result_sub_808CBA0 call_SpawnOverworldMapObject_sub_808CBA0(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(SpawnOverworldMapObject);
    return (call_SpawnOverworldMapObject_result_sub_808CBA0){ R2, R3, R6 };
}

/* spawnOverworldMapObjects_808CBA0: spawn the initial batch of overworld map objects.
 *
 * The number of objects depends on where the player is:
 *   - map group 0x85: 12 objects
 *   - any other map with a nonzero MapNumber: 9 objects
 *   - otherwise: 7 objects
 * Each one is spawned as SpawnOverworldMapObject(0x10, 0, 0, 0, index) for
 * index = 0 .. count-1.
 *
 * Register notes: the original brackets the whole routine with
 * push/pop {r4-r7}, so r4-r7 reach the caller exactly as they arrived,
 * even though SpawnOverworldMapObject loads r4 and clobbers r5/r6.
 * The last callee call leaves r2 (and r3, a clobber) behind for the caller. */
static void spawn_overworld_map_objects(u32 toolkit) {
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);

    u32 count;
    if (LD8(gameState + oGameState_MapGroup) == 0x85) {
        count = 12;
    } else if (LD8(gameState + oGameState_MapNumber) != 0) {
        count = 9;
    } else {
        count = 7;
    }

    /* register form: SpawnOverworldMapObject clobbers r5/r6 and its wrapper
     * loads r4, but this function must hand r4-r7 back to its caller
     * unchanged, so they have to be saved and restored around the calls
     * (the original does this with push/pop {r4-r7} / push/pop {r5}). */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;

    for (u32 index = 0; index < count; index++) {
        call_SpawnOverworldMapObject_sub_808CBA0(0x10, 0, 0, 0, index);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
}

/* spawnOverworldMapObjects_808CBA0(r10 = toolkit) -> r2 (left by the last SpawnOverworldMapObject
 * call); r4-r7 end exactly as they arrived, so the adapter leaves them alone. */
Next spawnOverworldMapObjects_808CBA0(void) {
    spawn_overworld_map_objects(R10);
    RETURN_TO_CALLER();
}

/* ── battle_checkPlayerActorStoryFlag_808CC10 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlagFromImmediate_result_battle_checkPlayerActorStoryFlag_808CC10;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_battle_checkPlayerActorStoryFlag_808CC10 call_TestEventFlagFromImmediate_battle_checkPlayerActorStoryFlag_808CC10(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_battle_checkPlayerActorStoryFlag_808CC10){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3; } call_SetEventFlagFromImmediate_result;
/* register form: generated wrapper: SetEventFlagFromImmediate has no typed C interface yet */
static inline call_SetEventFlagFromImmediate_result call_SetEventFlagFromImmediate(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
    return (call_SetEventFlagFromImmediate_result){ R3 };
}

/* EVENT_9D8, split the way TestEventFlagFromImmediate/SetEventFlagFromImmediate take it:
 * r0 = flag bank, r1 = flag index within the bank. */
#define EVENT_9D8_BANK  9
#define EVENT_9D8_INDEX 0xD8

/* Battle actor header field: the player actor's story/state byte lives at +4. */
#define ACTOR_STATE_BYTE 4

/* Check the player actor's story state: if it is in state 2 and story flag EVENT_9D8
 * is not yet set, set the flag and report that we just did.
 * r4/r5/r6 are only carried through unchanged (the original pops them back). */
typedef struct {
    u32 r4, r5, r6;   /* passed through untouched */
    bool flag_set;    /* true only on the turn where we set EVENT_9D8 */
} story_flag_check;

static story_flag_check check_player_actor_story_flag(u32 r4, u32 r5, u32 r6)
{
    story_flag_check res = { r4, r5, r6, false };

    /* Only the actor in story state 2 triggers the flag. */
    if (LD8(r5 + ACTOR_STATE_BYTE) != 2)
        return res;

    /* Already set? (TestEventFlagFromImmediate leaves Z clear when the flag is set.) */
    if (!call_TestEventFlagFromImmediate_battle_checkPlayerActorStoryFlag_808CC10(EVENT_9D8_BANK, EVENT_9D8_INDEX).z)
        return res;

    /* First time here: latch the flag and remember it. */
    call_SetEventFlagFromImmediate(EVENT_9D8_BANK, EVENT_9D8_INDEX);
    res.flag_set = true;
    return res;
}

/* battle_checkPlayerActorStoryFlag_808CC10(r5 = player actor, r10 = flag context) -> nz r3 r4 r5 r6 */
Next battle_checkPlayerActorStoryFlag_808CC10(void) {
    story_flag_check res = check_player_actor_story_flag(R4, R5, R6);
    R4 = res.r4;
    R5 = res.r5;
    R6 = res.r6;
    /* r3 is left behind by the TestEventFlagFromImmediate wrapper itself when it runs. */
    SET_FLAGS_NZ((u32)res.flag_set);
    RETURN_TO_CALLER();
}

/* ── ow_storePlayerPosDup ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2; } call_ReadOWPlayerObjectCoords_result_ow_storePlayerPosDup;
/* register form: generated wrapper: ReadOWPlayerObjectCoords has no typed C interface yet */
static inline call_ReadOWPlayerObjectCoords_result_ow_storePlayerPosDup call_ReadOWPlayerObjectCoords_ow_storePlayerPosDup(void) {
    GUEST_CALL(ReadOWPlayerObjectCoords);
    return (call_ReadOWPlayerObjectCoords_result_ow_storePlayerPosDup){ R0, R1, R2 };
}

/* Copy the player's overworld position (fixed point, >>16 to get the integer
 * part) into both position pairs of the object at r5: fields 0x16/0x18 and
 * the duplicates at 0x1A/0x1C. */
static void store_player_pos(u32 playerObj) {
    call_ReadOWPlayerObjectCoords_result_ow_storePlayerPosDup coords = call_ReadOWPlayerObjectCoords_ow_storePlayerPosDup();

    s16 x = (s16)((s32)coords.r0 >> 16);
    s16 y = (s16)((s32)coords.r1 >> 16);

    ST16(playerObj + 0x16, (u16)x);
    ST16(playerObj + 0x18, (u16)y);
    ST16(playerObj + 0x1A, (u16)x);
    ST16(playerObj + 0x1C, (u16)y);
}

/* ow_storePlayerPosDup(r5 = player object, r10 = coords source) -> no outputs */
Next ow_storePlayerPosDup(void) {
    store_player_pos(R5);
    RETURN_TO_CALLER();
}

/* ── reqBBS_pollTouchTrigger_808CD04 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3, r4, r5, r8, r9, r10, r11, r12; } call_reqBBS_81409E4_result;
/* register form: generated wrapper: reqBBS_81409E4 has no typed C interface yet */
static inline call_reqBBS_81409E4_result call_reqBBS_81409E4(void) {
    GUEST_CALL(reqBBS_81409E4);
    return (call_reqBBS_81409E4_result){ R0, R2, R3, R4, R5, R8, R9, R10, R11, R12 };
}

/* Poll the touch trigger (reqBBS_81409E4) and record the button id.
 *
 * The callee reports which touch input occurred in r0.  If it is one of the
 * ids 0xC0..0xCC, the id is stored into the work area's trigger byte
 * (byte_2000B45, reached through the pointer at dword_808CA44) and the
 * function reports success.  Returns nonzero when a trigger id was recorded.
 *
 * r2/r5/r9/r10/r11/r12 on exit are whatever reqBBS_81409E4 left behind; the
 * generated wrapper already leaves them in place, so nothing here touches them.
 */
static u32 poll_touch_trigger(void) {
    u32 workBase = LD32(A_dword_808CA44);   /* pointer to the work area (0x2000B40) */
    u32 triggerId = call_reqBBS_81409E4().r0;

    if ((s32)triggerId >= (s32)0xC0 && (s32)triggerId <= (s32)0xCC) {
        ST8(workBase + 5, triggerId);       /* byte_2000B45: last touch id */
        return 1;
    }
    return 0;
}

/* reqBBS_pollTouchTrigger_808CD04: no register inputs (r10 passes through to
 * the callee untouched); r0 = result, NZ from the result. */
Next reqBBS_pollTouchTrigger_808CD04(void) {
    R0 = poll_touch_trigger();
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_checkLockedEventTable_808CDC4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

typedef struct { u32 r3; bool z, n; } call_TestEventFlag_result_battle_checkLockedEventTable_808CDC4;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_battle_checkLockedEventTable_808CDC4 call_TestEventFlag_battle_checkLockedEventTable_808CDC4(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_battle_checkLockedEventTable_808CDC4){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3, r4, r5, r6, r7; bool z, n; } call_TestEventFlagRange_result;
/* register form: generated wrapper: TestEventFlagRange has no typed C interface yet */
static inline call_TestEventFlagRange_result call_TestEventFlagRange(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(TestEventFlagRange);
    return (call_TestEventFlagRange_result){ R3, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Locked-event table used by the battle AI. Each entry is 8 bytes:
 *   [0..1] u16 gate flag   - entry only applies when this event flag is set
 *   [2..3] u16 range start - first flag of the range test
 *   [4..5] u16 range end   - last flag of the range test
 *   [6..7] padding
 * An entry "locks" when its gate flag is set AND one flag in the range is set. */

struct CheckLockedEventsResult {
    u32 index;  /* r1: ai_mode * 3 + matching entry index (0 when nothing matched) */
    u32 r3;     /* r3 exactly as the last TestEventFlag/TestEventFlagRange left it */
    u32 found;  /* 1 when a locked event matched, else 0 - drives the N/Z flags */
};

/* Scan the locked-event table for the current AI map group.
 * Returns whether an entry locked and which one; r3 tracking matters because
 * callers can see the register the last flag-test call left behind. */
static struct CheckLockedEventsResult check_locked_event_table(void) {
    struct CheckLockedEventsResult out;
    u32 ai_mode = call_ai_getAIModeForMapGroup_808D084();
    u32 table = LD32(A_off_808CEBC + ai_mode * 4);
    s32 limit = ((s32)ai_mode < 3) ? 3 : 4;
    u32 last_r3 = 0;
    u32 found = 0;
    s32 i = 0;

    while (i < limit) {
        u32 entry = table + (u32)i * 8;

        /* Gate flag: entry skipped entirely when the flag is clear. */
        call_TestEventFlag_result_battle_checkLockedEventTable_808CDC4 gate = call_TestEventFlag_battle_checkLockedEventTable_808CDC4(LD16(entry));
        last_r3 = gate.r3;
        if (gate.z) {
            i++;
            continue;
        }

        /* Range test: locked when one of the flags in [start, end] is set. */
        call_TestEventFlagRange_result range =
            call_TestEventFlagRange(LD16(entry + 2), LD16(entry + 4));
        last_r3 = range.r3;
        if (!range.z) {
            found = 1;
            break;              /* i stays on the matching entry */
        }
        i++;
    }

    /* On a match the original reports the entry it stopped at; when the whole
     * table is scanned without a match it resets the index to 0. */
    out.index = ai_mode * 3 + (found ? (u32)i : 0);
    out.r3 = last_r3;
    out.found = found;
    return out;
}

/* battle_checkLockedEventTable_808CDC4 (r10 = battle context) ->
 *   nz: Z clear when a locked event matched (tst of the found flag)
 *   r1: ai_mode * 3 + matching entry index
 *   r2: the entry r8 value (the original pops its saved r8 into r2)
 *   r3: leftover from the last flag-test call
 *   r4/r6 are pushed and popped by the original, so they keep their entry
 *   values and the adapter must not touch them. */
Next battle_checkLockedEventTable_808CDC4(void) {
    struct CheckLockedEventsResult res = check_locked_event_table();
    R1 = res.index;
    R2 = R8;
    R3 = res.r3;
    SET_FLAGS_NZ(res.found);
    RETURN_TO_CALLER();
}

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

/* ── ai_syncModeEventFlag_808CF50 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: ai_getAIModeForMapGroup_808D084 has no typed C interface yet */
static inline u32 call_ai_getAIModeForMapGroup_808D084_sub_808CF50(void) {
    GUEST_CALL(ai_getAIModeForMapGroup_808D084);
    return R0;
}

typedef struct { bool z, n; } call_TestEventFlag_result_sub_808CF50;
/* register form: generated wrapper: TestEventFlag has no typed C interface yet */
static inline call_TestEventFlag_result_sub_808CF50 call_TestEventFlag_sub_808CF50(u32 r0) {
    R0 = r0;
    GUEST_CALL(TestEventFlag);
    return (call_TestEventFlag_result_sub_808CF50){ ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline void call_SetEventFlag_sub_808CF50(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
}

/* Per-map-group AI bookkeeping: look up the story event flag that goes with the
 * current AI mode; if that flag is not set yet, set its partner flag.
 * (byte_808CF7C holds the "test" flag IDs, byte_808CF84 the matching "set"
 * flag IDs, both indexed by AI mode as u16 entries.) */
static void sync_ai_mode_flags(void) {
    u32 modeOffset = call_ai_getAIModeForMapGroup_808D084_sub_808CF50() * 2u;   /* byte offset into the u16 tables */

    /* TestEventFlag leaves Z set when the flag is clear, so only then set it. */
    if (!call_TestEventFlag_sub_808CF50(LD16(A_byte_808CF7C + modeOffset)).z)
        return;

    call_SetEventFlag_sub_808CF50(LD16(A_byte_808CF84 + modeOffset));
}

/* ai_syncModeEventFlag_808CF50() -> r7 (r7 comes back exactly as it entered: the original's
 * pop {r4-r7,pc} restores it from the stack) */
Next ai_syncModeEventFlag_808CF50(void) {
    sync_ai_mode_flags();
    R7 = R7;    /* unchanged output: restored to its entry value */
    RETURN_TO_CALLER();
}

/* ── battle_chatbox_setCursorTextFromEventFlag_808CF8C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; bool z, n; } call_TestEventFlagFromImmediate_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C;
/* register form: generated wrapper: TestEventFlagFromImmediate has no typed C interface yet */
static inline call_TestEventFlagFromImmediate_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C call_TestEventFlagFromImmediate_battle_chatbox_setCursorTextFromEventFlag_808CF8C(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(TestEventFlagFromImmediate);
    return (call_TestEventFlagFromImmediate_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C){ R3, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3, r5, r6, r7, r10; } call_PlaySoundEffect_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C call_PlaySoundEffect_battle_chatbox_setCursorTextFromEventFlag_808CF8C(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_battle_chatbox_setCursorTextFromEventFlag_808CF8C){ R2, R3, R5, R6, R7, R10 };
}

/* Battle chatbox: set the cursor text from an event flag (bn6f 0808CF8C).
 *
 * r5 points at an object whose halfword at +6 holds a text id, r10 is the
 * toolkit. If event flag EVENT_9D9 is set, the text id is copied into the
 * chatbox's cursor text field (offset 0x4C) and sound 0x122 plays; if the
 * flag is clear, nothing happens.
 *
 * Returns 1 when the text was written, 0 when the flag was clear; callers
 * branch on the result.
 *
 * Register notes:
 *  - The original computes its 0/1 result in r4 but pops the entry value
 *    back into r4 on exit, so r4 is preserved and the adapter leaves it
 *    untouched; only the N/Z flags carry the result out.
 *  - r2/r3/r6 on the taken path are whatever PlaySoundEffect leaves behind
 *    (and r3 on the early path whatever TestEventFlagFromImmediate leaves);
 *    the wrapper calls leave those guest registers in exactly that state,
 *    so nothing is passed through return values. */
static u32 battle_chatbox_setCursorTextFromEventFlag(u32 textSource, u32 toolkit) {
    /* movflag EVENT_9D9: the flag id travels as page in r0, index in r1.
     * TestEventFlagFromImmediate leaves Z set when the flag is clear. */
    if (call_TestEventFlagFromImmediate_battle_chatbox_setCursorTextFromEventFlag_808CF8C(EVENT_9D9 >> 8, EVENT_9D9 & 0xFF).z)
        return 0;

    u32 chatbox = LD32(toolkit + oToolkit_ChatboxPtr);
    ST32(chatbox + 0x4C, LD16(textSource + 6));

    /* r2 still holds the 0x4C cursor-text offset here, as in the original. */
    call_PlaySoundEffect_battle_chatbox_setCursorTextFromEventFlag_808CF8C(0x122, 0x4C);
    return 1;
}

Next battle_chatbox_setCursorTextFromEventFlag_808CF8C(void) {
    u32 written = battle_chatbox_setCursorTextFromEventFlag(R5, R10);
    SET_FLAGS_NZ(written);
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

/* ── ow_setupMapTiles_808D108 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r5, r6, r7; } call_sub_8142CB0_result;
/* register form: generated wrapper: sub_8142CB0 has no typed C interface yet */
static inline call_sub_8142CB0_result call_sub_8142CB0(void) {
    GUEST_CALL(sub_8142CB0);
    return (call_sub_8142CB0_result){ R0, R5, R6, R7 };
}

typedef struct { u32 r5, r6, r7; } call_ow_stampMapTileTemplate_result;
/* register form: generated wrapper: ow_stampMapTileTemplate has no typed C interface yet */
static inline call_ow_stampMapTileTemplate_result call_ow_stampMapTileTemplate(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ow_stampMapTileTemplate);
    return (call_ow_stampMapTileTemplate_result){ R5, R6, R7 };
}

typedef struct { u32 r2, r7, r9, r12; } call_sub_8142D4C_result;
/* register form: generated wrapper: ow_redrawMapTilemapsCb has no typed C interface yet */
static inline call_sub_8142D4C_result call_sub_8142D4C(void) {
    GUEST_CALL(ow_redrawMapTilemapsCb);
    return (call_sub_8142D4C_result){ R2, R7, R9, R12 };
}

/* Overworld map tile setup (0x0808D108).
 *
 * Fetches the current tile template description, stamps it onto the map with
 * template index 0, then refreshes the derived map state. All of the callers'
 * outputs (r2, r5, r9, r12) are values the callees leave behind: r5 comes from
 * ow_stampMapTileTemplate (the last writer before ow_redrawMapTilemapsCb, which preserves
 * it), and r2/r9/r12 come from ow_redrawMapTilemapsCb itself. */
static void ow_setup_map_tiles(void) {
    u32 tile_template = call_sub_8142CB0().r0;

    call_ow_stampMapTileTemplate(tile_template, 0);
    call_sub_8142D4C();
}

Next ow_setupMapTiles_808D108(void) {
    ow_setup_map_tiles();
    RETURN_TO_CALLER();
}

/* ── sub_808D25C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r12; bool c, v; } call_sub_8048F88_result;
/* register form: generated wrapper: shop_addStockEntry_kind4_8048F88 has no typed C interface yet */
static inline call_sub_8048F88_result call_sub_8048F88(void) {
    GUEST_CALL(shop_addStockEntry_kind4_8048F88);
    return (call_sub_8048F88_result){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R12, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_808D25C — run the game-state unpacker shop_addStockEntry_kind4_8048F88, then finish with 0.
 *
 * This function is pure glue: it tail-calls the unpacker (which reads r10 and
 * leaves r1..r9, r12, c, v behind — those are exactly the values its callers
 * observe) and then executes "mov r0, #0", whose only lasting effect is the
 * N/Z flags (N clear, Z set). r0 itself is a clobber, so only the flags and
 * the pass-through registers matter. */

/* Run the unpacker and report the final result value (always 0, matching the
 * original's "mov r0, #0"). The registers shop_addStockEntry_kind4_8048F88 leaves behind stay in
 * the guest register file; nothing here needs to forward them. */
static u32 sub_808D25C_run_unpacker(void) {
    call_sub_8048F88_result regs = call_sub_8048F88();
    (void)regs; /* r2..r12 already hold the unpacker's results in the guest state */
    return 0;
}

Next sub_808D25C(void) {
    u32 result = sub_808D25C_run_unpacker();
    R0 = result;
    SET_FLAGS_NZ(result);
    RETURN_TO_CALLER();
}
