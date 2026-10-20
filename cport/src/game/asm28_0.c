/*
 * asm28_0.c: readable functions of bn6f asm28_0.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── deliverCutsceneMail_8098BE8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r6, r7; } call_addMail_802f238_result;
/* register form: generated wrapper: addMail_802f238 has no typed C interface yet */
static inline call_addMail_802f238_result call_addMail_802f238(u32 r0) {
    R0 = r0;
    GUEST_CALL(addMail_802f238);
    return (call_addMail_802f238_result){ R1, R2, R3, R4, R6, R7 };
}

/* register form: generated wrapper: ClearEventFlag has no typed C interface yet */
static inline void call_ClearEventFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(ClearEventFlag);
}

typedef struct { u32 r1, r3; bool c, v; } call_SetEventFlag_result;
/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline call_SetEventFlag_result call_SetEventFlag(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
    return (call_SetEventFlag_result){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

#define CUTSCENE_MAIL_BASE  0x1CA0   /* first inbox mail slot for cutscene deliveries */
#define CUTSCENE_CLEAR_BASE 0x1D20   /* "mail not seen" event-flag block base */
#define CUTSCENE_SET_BASE   0x1DA0   /* "mail delivered" event-flag block base */

typedef struct {
    u32 r1;
    u32 r2;
} DeliverMailResult;

/* Deliver a cutscene mail from a script record (r5 + 6 = mail id, r5 + 7 = completion byte).
 * If mail id is 0xFF, nothing happens; r2 passes through unchanged.
 * Otherwise addMail queues the message; if the completion byte is set the
 * "not seen" flag is cleared and the "delivered" flag is set. */
static DeliverMailResult deliver_cutscene_mail(u32 scriptPtr, u32 initialR2) {
    DeliverMailResult result;
    u32 mailId = LD8(scriptPtr + 6);

    if (mailId == 0xFF) {
        result.r1 = 0xFF;
        result.r2 = initialR2;
        return result;
    }

    call_addMail_802f238_result addResult = call_addMail_802f238(CUTSCENE_MAIL_BASE + mailId);
    result.r1 = addResult.r1;
    result.r2 = addResult.r2;

    if (LD8(scriptPtr + 7) == 0)
        return result;

    call_ClearEventFlag(CUTSCENE_CLEAR_BASE + mailId);
    call_SetEventFlag_result setResult = call_SetEventFlag(CUTSCENE_SET_BASE + mailId);
    result.r1 = setResult.r1;

    return result;
}

/* deliverCutsceneMail_8098BE8(r5 = script record ptr, r10 = toolkit) -> r1 r2, nz */
Next deliverCutsceneMail_8098BE8(void) {
    DeliverMailResult r = deliver_cutscene_mail(R5, R2);
    R0 = 0;
    R1 = r.r1;
    R2 = r.r2;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_8098E24 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; bool c, v; } call_SetEventFlag_result_sub_8098E24;
/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline call_SetEventFlag_result_sub_8098E24 call_SetEventFlag_sub_8098E24(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
    return (call_SetEventFlag_result_sub_8098E24){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r3; bool c, v; } call_ClearEventFlag_result;
/* register form: generated wrapper: ClearEventFlag has no typed C interface yet */
static inline call_ClearEventFlag_result call_ClearEventFlag_sub_8098E24(u32 r0) {
    R0 = r0;
    GUEST_CALL(ClearEventFlag);
    return (call_ClearEventFlag_result){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Apply a per-Navi behaviour entry from the chip table.
 * Writes the resolved entry pointer into the chip-use record, copies two
 * overlay bytes, optionally stamps a byte into the battle object's inner
 * struct, then fires any event-flag set/clear encoded in the entry.
 *
 *   chip_use  = chip-use record (r5 on entry)
 *   toolkit   = battle toolkit pointer (r10 on entry)
 *   entry     = A_off_8098CFC + 0x1C * byte at chip_use+4
 *
 * Returns the r1 and r2 values the original leaves in registers on exit. */
struct ChipEntryResult {
    u32 r1;
    u32 r2;
};

static struct ChipEntryResult apply_chip_use_entry(u32 chip_use, u32 toolkit) {
    u8 navi_idx = LD8(chip_use + 4);
    u32 entry = A_off_8098CFC + 0x1Cu * (u32)navi_idx;

    /* Store resolved entry pointer and two overlay bytes into chip-use record */
    ST32(chip_use + 0x34, entry);
    ST8(chip_use + 6, LD8(entry + 0x10));
    ST8(chip_use + 7, LD8(entry + 0x11));

    /* Battle object inner struct (read through toolkit) */
    u32 inner = LD32(toolkit + 0x3C);

    /* Propagate optional overlay byte; 0xFF means "do nothing" */
    u8 overlay = LD8(entry + 0x12);
    if (overlay != 0xFF) {
        ST8(inner + 0x16, overlay);
        ST8(inner + 0x17, 0xFF);
    }

    /* Fire event-flag set/clear pairs; r1 default is 0xFF */
    u32 r1_result = 0xFF;

    u16 set_flag = LD16(entry + 0x14);
    if (set_flag != 0) {
        call_SetEventFlag_result_sub_8098E24 s = call_SetEventFlag_sub_8098E24(set_flag);
        r1_result = s.r1;
    }

    u16 clear_flag = LD16(entry + 0x16);
    if (clear_flag != 0) {
        call_ClearEventFlag_result c = call_ClearEventFlag_sub_8098E24(clear_flag);
        r1_result = c.r1;
    }

    struct ChipEntryResult out = { r1_result, inner };
    return out;
}

/* sub_8098E24(r5 = chip-use record, r10 = toolkit) -> r1, r2, nz */
Next sub_8098E24(void) {
    struct ChipEntryResult res = apply_chip_use_entry(R5, R10);
    R1 = res.r1;
    R2 = res.r2;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_8098E6C ── */
/* sub_8098E6C:
 * Copies three settings bytes from a fixed table entry into the work struct
 * reached through R10+0x3C. The entry index is the byte at R5+4; each entry
 * is 0x1C bytes and bytes 0x18..0x1A are written to struct bytes 6..8.
 * A table byte of 0xFF means "leave the target byte unchanged".
 * Returns R0 = 0 with the NZ flags reflecting that zero, and leaves
 * R1 = entry index, R2 = 0x1C * index in the registers. R4-R7 are preserved
 * (the original's push/pop restores them, despite the table/struct pointers
 * living in R4/R7 mid-function). */
Next sub_8098E6C(void) {
    u32 index = LD8(R5 + 4);
    u32 entry = A_off_8098CFC + 0x1C * index;
    u32 dst = LD32(R10 + 0x3C);

    u32 value = LD8(entry + 0x18);
    if (value != 0xFF)
        ST8(dst + 6, value);

    value = LD8(entry + 0x19);
    if (value != 0xFF)
        ST8(dst + 7, value);

    value = LD8(entry + 0x1A);
    if (value != 0xFF)
        ST8(dst + 8, value);

    R0 = 0;
    R1 = index;
    R2 = 0x1C * index;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── RunLMessageTextScript ── */
/* RunLMessageTextScript: at 0x08099218
 *
 * Runs a chatbox script when the player talks to their PET Navi. The Navi can quip a
 * random joke, recite a poem, or fall back to the current map's text archive based on
 * the Navi's "humor" / "poem" stats and a few event flags.
 *
 * Outputs: nz (zero), r1, r2, r4, r5, r7
 * Clobbers: c, v, r0, r3
 * Preserved: r4, r5, r6, r7
 */

/* Event flag banks/indices consulted by the offline-world PET Navi chatter check. */
#define PET_NAVI_FLAG_BANK    0x17
#define FLAG_COPYBOT_ACTIVE   0x29  /* EVENT_COPYBOT_ACTIVE */
#define FLAG_PET_NAVI_ACTIVE  0x1D  /* EVENT_PET_NAVI_ACTIVE */
#define FLAG_172A             0x2A  /* EVENT_172A */

/* Compressed text archive pointers (top bit set => "compressed ptr" sentinel). */
#define COMP_MEGAMAN_JOKES  0x88738B24u  /* CompText8738B24 | COMPRESSED_PTR_FLAG */
#define COMP_NAVI_POEMS     0x8873A528u  /* CompText873A528 | COMPRESSED_PTR_FLAG */

Next RunLMessageTextScript(void) {
    /* If no PET Navi, take the default path. */
    GUEST_CALL(GetCurPETNavi);
    if (R0 != 0) goto run_default;

    /* Get the current map group from the active GameState via the toolkit. */
    u32 toolkit   = R10;
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 mapGroup  = LD8(gameState + oGameState_MapGroup);

    /* Internet-area maps bypass the event-flag gate. */
    if ((s32)mapGroup >= INTERNET_MAP_GROUP_START) goto have_pet_navi;

    /* Offline worlds: at least one of these flags must be active for the Navi to speak. */
    R0 = PET_NAVI_FLAG_BANK;
    R1 = FLAG_COPYBOT_ACTIVE;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (!ZERO_FLAG()) goto run_default;

    R0 = PET_NAVI_FLAG_BANK;
    R1 = FLAG_PET_NAVI_ACTIVE;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (ZERO_FLAG()) goto run_default;

    R0 = PET_NAVI_FLAG_BANK;
    R1 = FLAG_172A;
    GUEST_CALL(TestEventFlagFromImmediate);
    if (!ZERO_FLAG()) goto run_default;

have_pet_navi:
    /* Humor stat == 1 -> run a MegaMan joke line. */
    GUEST_CALL(GetCurPETNavi);
    R1 = oNaviStats_Humor;
    GUEST_CALL(GetNaviStatsByte);
    SET_FLAGS_CMP(R0, 1);
    if (!ZERO_FLAG()) goto check_poem;

    R0 = COMP_MEGAMAN_JOKES;
    GUEST_CALL(DecompressTextArchiveForCutscene2);
    R1 = 0;
    GUEST_CALL(chatbox_runScript);
    goto done;

check_poem:
    /* Poem stat == 1 -> run a poem line. */
    GUEST_CALL(GetCurPETNavi);
    R1 = oNaviStats_Poem;
    GUEST_CALL(GetNaviStatsByte);
    SET_FLAGS_CMP(R0, 1);
    if (!ZERO_FLAG()) goto run_default;

    R0 = COMP_NAVI_POEMS;
    GUEST_CALL(DecompressTextArchiveForCutscene2);
    R1 = 0;
    GUEST_CALL(chatbox_runScript);
    goto done;

run_default:
    /* Default: open the current map's text archive and run the train station script
     * selected by GameState's script-offset index. */
    GUEST_CALL(chatbox_uncompMapTextArchives_803FD3C);
    {
        u32 gs      = LD32(R10 + oToolkit_GameStatePtr);
        u32 scrIdx  = LD8(gs + oGameState_SavedByte);
        R0 = scrIdx;
        GUEST_CALL(chatbox_runTrainScript);
    }

done:
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_809AF7C ── */
/* sub_809AF7C: Prepare the overworld player for the CopyBot tutorial cutscene.
 *
 * Looks up a per-area entry in a 0x3C-byte table at byte_809ADB4 indexed by the
 * current map, optionally adds 0x10 if the CopyBot event flag is NOT set (so the
 * tutorial scene runs before the player has the CopyBot item), then stores the
 * selected entry pointer in the work area at r5+0x34 and uses it to position
 * and pose the player.
 */
Next sub_809AF7C(void) {
    /* Pointer to the CopyBot scene entry table; each entry is 0x3C bytes. */
    u32 table = A_byte_809ADB4;

    /* The map state pointer lives at r10+0x3C; the scene index is the byte
     * at offset 0x12 of that struct. */
    u32 map_state_ptr = LD32(R10 + 0x3C);
    u32 scene_index   = LD8(map_state_ptr + 0x12);

    /* Select the base entry for this map. */
    u32 entry = table + scene_index * 0x3C;

    /* If the CopyBot event flag (0x1729) hasn't been set yet, advance to the
     * "before CopyBot" variant of the scene by skipping one extra entry. */
    u32 variant_offset = 0;
    R0 = 0x17;          /* event flag bank */
    R1 = 0x29;          /* event flag bit: EVENT_COPYBOT_ACTIVE */
    GUEST_CALL(TestEventFlagFromImmediate);
    if (ZERO_FLAG()) {
        variant_offset = 0x10;
    }
    entry += variant_offset;

    /* Hand the selected entry off to the caller via the work struct at r5. */
    ST32(R5 + 0x34, entry);

    /* Move the player to the entry's saved (x, y, z) coordinates. */
    R0 = LD32(entry + 4);
    R1 = LD32(entry + 8);
    R2 = LD32(entry + 0xC);
    GUEST_CALL(owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4);

    /* Apply the entry's facing direction and animation pose (byte 3 of entry). */
    R0 = LD8(entry + 3);
    GUEST_CALL(SetOWPlayerFacingDirection);

    R0 = LD8(entry + 3);
    GUEST_CALL(owPlayer_setS200ace0_fixedAnimationSelect_809e13c);

    /* Clear the Navi palette slot. */
    GUEST_CALL(ZeroOWPlayerNaviPaletteIndex);

    /* Result 0 (AND with itself -> NZ clear) just to publish a known NZ state. */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── ow_periodicObjectSpawner ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; bool c, v; } call_SpawnOverworldMapObject_result;
/* register form: generated wrapper: SpawnOverworldMapObject has no typed C interface yet */
static inline call_SpawnOverworldMapObject_result call_SpawnOverworldMapObject(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    GUEST_CALL(SpawnOverworldMapObject);
    return (call_SpawnOverworldMapObject_result){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Periodic overworld map-object spawner (per-frame tick handler of an
 * overworld object, r5 = object, r10 = toolkit).
 *
 * The object's signed 16-bit lifetime at +0x1A is decremented every tick.
 * Whether the spawner is "armed" depends on the object's mode bytes:
 *   - byte +6 == 2: armed once the lifetime has run out (<= 0),
 *   - otherwise:    armed while the lifetime is still running (> 0)
 *                   and byte +7 == 2.
 * While armed, a tick counter at +0x18 is incremented and every 8th tick a
 * map object is spawned through SpawnOverworldMapObject, taking its position
 * data from the map state block reached via toolkit->+0x3C.
 *
 * Register contract: outputs nz (set from the final r0 = 1), r2/r3 are left
 * behind by SpawnOverworldMapObject on the spawn path (the wrapper leaves
 * them in place), and r4-r7 are restored to their entry values by the
 * original's push/pop — so they are passed in and handed back unchanged even
 * though the callee runs over them. */
typedef struct {
    u32 r4, r5, r6, r7;    /* saved-register values to restore afterwards */
} OwPeriodicSpawnRegs;

static OwPeriodicSpawnRegs periodic_object_spawn(u32 r4, u32 r5, u32 r6, u32 r7, u32 toolkit)
{
    OwPeriodicSpawnRegs regs;
    regs.r4 = r4; regs.r5 = r5; regs.r6 = r6; regs.r7 = r7;

    /* Count the signed lifetime down; the decrement is done in 32 bits but
     * only the low half is stored back. */
    s32 life = (s32)LDS16(r5 + 0x1A) - 1;
    ST16(r5 + 0x1A, (u16)life);

    bool armed;
    if (LD8(r5 + 6) == 2) {
        armed = life <= 0;                          /* fire after expiry */
    } else {
        armed = life > 0 && LD8(r5 + 7) == 2;       /* fire while alive  */
    }
    if (!armed)
        return regs;

    /* Period gate: bump the tick counter at +0x18, act on every 8th tick. */
    u32 tick = LD16(r5 + 0x18) + 1;
    ST16(r5 + 0x18, (u16)tick);
    if (tick & 7)
        return regs;

    /* toolkit->+0x3C is the map state, its +0x18 the spawn parameter block:
     * +1C/+20/+24 are the three spawn arguments, and the object's mode byte
     * +7 selects the object kind (folded into the high half of r4's constant). */
    u32 map_state  = LD32(toolkit + 0x3C);
    u32 spawn_data = LD32(map_state + 0x18);
    u32 kind       = 0x48000001u | ((u32)LD8(r5 + 7) << 16);

    call_SpawnOverworldMapObject(0xB,
                                 LD32(spawn_data + 0x1C),
                                 LD32(spawn_data + 0x20),
                                 LD32(spawn_data + 0x24),
                                 kind);
    return regs;
}

/* ow_periodicObjectSpawner (0x0809B0D8)
 * inputs: r5 r10 lr; outputs: nz r2 r3 r4 r5 r7; preserved: r4 r5 r6 r7 */
Next ow_periodicObjectSpawner(void) {
    OwPeriodicSpawnRegs regs = periodic_object_spawn(R4, R5, R6, R7, R10);
    R4 = regs.r4, R5 = regs.r5, R6 = regs.r6, R7 = regs.r7;
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── owPlayer_setFixedAnimationFromLookup ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; bool c, v; } call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result;
/* register form: generated wrapper: owPlayer_setS200ace0_fixedAnimationSelect_809e13c has no typed C interface yet */
static inline call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c(u32 r0) {
    R0 = r0;
    GUEST_CALL(owPlayer_setS200ace0_fixedAnimationSelect_809e13c);
    return (call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Result registers of owPlayer_setFixedAnimationFromLookup: r3 is whatever the
 * animation-select callee leaves there; r4/r5/r7 are the saved registers the
 * original pushes and pops, so they come back unchanged. */
typedef struct {
    u32 r3;
    u32 r4;
    u32 r5;
    u32 r7;
} FixedAnimFromLookupResult;

/* Apply the fixed animation looked up from the player's state:
 * [player+0x70] points at the shared state block, whose byte 0x0C indexes the
 * table at dword_809C82C (1,3,5,7) to pick the animation id. r10 must still
 * hold the caller's value for the callee; r4/r5/r7 are passed through because
 * the callee may clobber them and the original restores them on return. */
static FixedAnimFromLookupResult setFixedAnimationFromLookup(u32 player, u32 r4, u32 r7) {
    FixedAnimFromLookupResult out;

    u32 state = LD32(player + 0x70);
    u32 poseIndex = LD8(state + 0xC);
    u32 animId = LD8(A_dword_809C82C + poseIndex);

    call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result call =
        call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c(animId);

    out.r3 = call.r3;
    out.r4 = r4;
    out.r5 = player;
    out.r7 = r7;
    return out;
}

/* owPlayer_setFixedAnimationFromLookup(r5 = player, r10 = context) -> nz r3 r4 r5 r7 */
Next owPlayer_setFixedAnimationFromLookup(void) {
    FixedAnimFromLookupResult out = setFixedAnimationFromLookup(R5, R4, R7);
    R3 = out.r3;
    R4 = out.r4;
    R5 = out.r5;
    R7 = out.r7;
    SET_FLAGS_NZ(0);   /* mov r0, #0: Z set, N clear */
    RETURN_TO_CALLER();
}

/* ── ow_applyCutsceneDirOffset ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; } call_ow_getPlayerPosition_result;
/* register form: generated wrapper: ow_getPlayerPosition has no typed C interface yet */
static inline call_ow_getPlayerPosition_result call_ow_getPlayerPosition(void) {
    GUEST_CALL(ow_getPlayerPosition);
    return (call_ow_getPlayerPosition_result){ R1, R2, R3 };
}

/* Result of the cutscene direction offset: r1/r2/r3 carry the adjusted
 * tile coordinates (each sum shifted back into the high half, as the
 * original does), r5 passes through untouched. */
typedef struct {
    u32 x;    /* r1 */
    u32 y;    /* r2 */
    u32 z;    /* r3 */
    u32 actor; /* r5, unchanged */
} CutsceneDirOffsetResult;

/* Read the player's position, add the per-direction offset selected by
 * the cutscene state's facing byte, and hand the result back in tile
 * coordinates. The actor byte at +4 selects one of the 8-byte offset
 * slots inside the chosen direction's table row. */
static CutsceneDirOffsetResult cutscene_dir_offset(u32 actor) {
    CutsceneDirOffsetResult res;

    call_ow_getPlayerPosition_result pos = call_ow_getPlayerPosition();
    u32 px = pos.r1 >> 16; /* high halfword: tile x */
    u32 py = pos.r2 >> 16; /* high halfword: tile y */
    u32 pz = pos.r3 >> 16; /* high halfword: tile z */

    /* Cutscene state -> current facing direction (0-3). */
    u32 state = LD32(A_eCutsceneState + 0x70);
    u32 dir = LD8(state + 0xC);

    /* Pick the direction's offset row, then the slot within it. */
    u32 row = LD32(A_off_809C8CC + (dir << 2));
    u32 slot = LD8(actor + 4) << 3;
    u32 offsets = row + slot;

    res.x = (px + LD16(offsets)) << 16;
    res.y = (py + LD16(offsets + 2)) << 16;
    res.z = (pz + LD16(offsets + 4)) << 16;
    res.actor = actor;
    return res;
}

/* ow_applyCutsceneDirOffset(r5 = actor, r10 = context) -> r1 r2 r3 r5 */
Next ow_applyCutsceneDirOffset(void) {
    CutsceneDirOffsetResult res = cutscene_dir_offset(R5);
    R1 = res.x;
    R2 = res.y;
    R3 = res.z;
    R5 = res.actor;
    RETURN_TO_CALLER();
}

/* ── e_setEventFlagFromStruct ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r3; bool c, v; } call_SetEventFlag_result_e_setEventFlagFromStruct;
/* register form: generated wrapper: SetEventFlag has no typed C interface yet */
static inline call_SetEventFlag_result_e_setEventFlagFromStruct call_SetEventFlag_e_setEventFlagFromStruct(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetEventFlag);
    return (call_SetEventFlag_result_e_setEventFlagFromStruct){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* e_setEventFlagFromStruct: raise the event flag recorded in an object's state block.
 *
 * r5 points to an object whose field at +0x70 holds a pointer to a state block;
 * the event flag id is the word at +0x10 of that block. It is passed to
 * SetEventFlag (with r10 as the context the wrapper keeps intact), which leaves
 * its r1/r3 results and the C/V flags behind. Afterwards r0 is zeroed, and that
 * constant zero is what callers see in the N/Z flags (N clear, Z set).
 * r4-r7 are pushed/popped by the original, so r4, r5 and r7 - which callers read
 * afterwards - simply keep their entry values; r6 is preserved as well. */

typedef struct {
    u32 r4;
    u32 r5;
    u32 r7;
} EventFlagFromStructResult;

static EventFlagFromStructResult set_event_flag_from_struct(u32 object, u32 r4, u32 r7) {
    EventFlagFromStructResult out;

    u32 state = LD32(object + 0x70);
    u32 flagId = LD32(state + 0x10);
    call_SetEventFlag_e_setEventFlagFromStruct(flagId);

    out.r4 = r4;
    out.r5 = object;
    out.r7 = r7;
    return out;
}

/* inputs: r5 r10 lr; outputs: nz r3 r4 r5 r7; clobbers: c v r0 r1 lr */
Next e_setEventFlagFromStruct(void) {
    EventFlagFromStructResult out = set_event_flag_from_struct(R5, R4, R7);
    R4 = out.r4;
    R5 = out.r5;
    R7 = out.r7;
    SET_FLAGS_NZ(0);              /* mov r0, #0 after the call */
    RETURN_TO_CALLER();
}

/* ── tryStartBbsGuiCutscene_809CF2C ── */
/* tryStartBbsGuiCutscene_809CF2C: try to start the BBS GUI cutscene.
 * If the player can advance forward and a BBS request is pending, start the
 * cutscene script byte_809CEB4 with a parameter taken from eReqBBSGui[0xE]
 * (entry id - 1 + 0x32). r1/r2/r3 are left as StartCutscene leaves them. */
Next tryStartBbsGuiCutscene_809CF2C(void) {
    GUEST_CALL(check_playerCanAdvanceForward_8034C36);
    if (ZERO_FLAG()) /* player cannot advance forward */
        RETURN_TO_CALLER();

    /* reqBBS_81407D8 is called with r0 still holding the previous result;
     * it returns non-zero when the request should open the GUI. */
    GUEST_CALL(reqBBS_81407D8);
    if (R0 == 0)
        RETURN_TO_CALLER();

    R0 = A_byte_809CEB4; /* CutsceneScript */
    R2 = 0xE;
    R1 = LD8(A_eReqBBSGui + 0xE) - 1 + 0x32;
    GUEST_CALL(StartCutscene);

    RETURN_TO_CALLER();
}
