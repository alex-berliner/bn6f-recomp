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

/* ── cross_loadEntryFields_8098CCC ── */
/* Select one entry of the table at off_8098C28 (0x14-byte records) using the
 * index byte in the work area, copy its fixed fields into the work area, and
 * if the record names a partner character, write that id plus the 0xFF
 * terminator into the partner state block (state+0x3C).
 * Always ends with r0 = 0 and Z set; r2 keeps the partner state pointer. */
static u32 load_cross_entry(u32 work, u32 state) {
    u32 entry = A_off_8098C28 + 0x14 * (u32)LD8(work + 4);
    ST32(work + 0x34, entry);

    ST8(work + 5, LD8(entry + 0x13));
    ST8(work + 6, LD8(entry + 0x10));
    ST8(work + 7, LD8(entry + 0x11));

    u32 partner = LD32(state + 0x3C);
    u8 partner_id = LD8(entry + 0x12);
    if (partner_id != 0xFF) {
        ST8(partner + 0x16, partner_id);
        ST8(partner + 0x17, 0xFF);
    }
    return partner;
}

/* cross_loadEntryFields_8098CCC(r5 = work area, r10 = state block) -> nz, r2, r0 = 0 */
Next cross_loadEntryFields_8098CCC(void) {
    R2 = load_cross_entry(R5, R10);
    R0 = 0;
    SET_FLAGS_NZ(R0);
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

/* ── ow_cutsceneLoadEntry ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r3; } call_DecompressTextArchiveForCutscene_result;
/* register form: generated wrapper: DecompressTextArchiveForCutscene has no typed C interface yet */
static inline call_DecompressTextArchiveForCutscene_result call_DecompressTextArchiveForCutscene(u32 r0) {
    R0 = r0;
    GUEST_CALL(DecompressTextArchiveForCutscene);
    return (call_DecompressTextArchiveForCutscene_result){ R0, R2, R3 };
}

/* register form: generated wrapper: SetOWPlayerFacingDirection has no typed C interface yet */
static inline void call_SetOWPlayerFacingDirection_ow_cutsceneLoadEntry(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetOWPlayerFacingDirection);
}

typedef struct { bool v; } call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_ow_cutsceneLoadEntry;
/* register form: generated wrapper: owPlayer_setS200ace0_fixedAnimationSelect_809e13c has no typed C interface yet */
static inline call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_ow_cutsceneLoadEntry call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_ow_cutsceneLoadEntry(u32 r0) {
    R0 = r0;
    GUEST_CALL(owPlayer_setS200ace0_fixedAnimationSelect_809e13c);
    return (call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_result_ow_cutsceneLoadEntry){ OVERFLOW_FLAG() };
}

typedef struct { u32 r3; } call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_result_ow_cutsceneLoadEntry;
/* register form: generated wrapper: owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4 has no typed C interface yet */
static inline call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_result_ow_cutsceneLoadEntry call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_ow_cutsceneLoadEntry(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4);
    return (call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_result_ow_cutsceneLoadEntry){ R3 };
}

/* register form: generated wrapper: camera_writeUnk03_14_80301b2 has no typed C interface yet */
static inline void call_camera_writeUnk03_14_80301b2_ow_cutsceneLoadEntry(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(camera_writeUnk03_14_80301b2);
}

/* register form: generated wrapper: SetCameraXYZ has no typed C interface yet */
static inline void call_SetCameraXYZ_ow_cutsceneLoadEntry(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(SetCameraXYZ);
}

typedef struct { u32 r1, r3; bool v; } call_ClearEventFlagFromImmediate_result_ow_cutsceneLoadEntry;
/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline call_ClearEventFlagFromImmediate_result_ow_cutsceneLoadEntry call_ClearEventFlagFromImmediate_ow_cutsceneLoadEntry(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
    return (call_ClearEventFlagFromImmediate_result_ow_cutsceneLoadEntry){ R1, R3, OVERFLOW_FLAG() };
}

#define EVENT_EVENT_CUR_DIR_LOCKED 0x15

/* Load and apply one overworld cutscene entry.
 * Reads the current entry index from context+6, looks up a 20-byte record in the
 * cutscene table, optionally decompresses its text archive, advances the index,
 * and (if the entry's flag bit 0 is set) configures the player and camera.
 * Always stores bit 7 of the flags byte to context+8 (direction-lock status).
 * Returns 0, so the caller sees Z set and N clear. */
static s32 ow_cutsceneLoadEntry_logic(u32 context) {
    u32 entryIdx = LD8(context + 6);
    u32 tableEntry = A_byte_8098E9C + (0x14 * entryIdx);

    /* Decompress the text archive referenced by this entry (if any) */
    u32 archivePtr = LD32(tableEntry);
    if (archivePtr != 0) {
        call_DecompressTextArchiveForCutscene_result res =
            call_DecompressTextArchiveForCutscene(archivePtr);
        ST32(context + 0x30, res.r0);
    }

    /* Advance the cutscene state to the next entry index stored in field 4 */
    ST8(context + 6, LD8(tableEntry + 4));

    /* Bit 0 of the flags field (byte +7) gates the player/camera setup block */
    u32 flags = LD8(tableEntry + 7);
    if (flags & 1) {
        /* Set the player's facing direction from field +5 */
        call_SetOWPlayerFacingDirection_ow_cutsceneLoadEntry(LD8(tableEntry + 5));

        /* Select a fixed animation based on fields +6 and +5 */
        call_owPlayer_setS200ace0_fixedAnimationSelect_809e13c_ow_cutsceneLoadEntry(
            LD8(tableEntry + 6) + LD8(tableEntry + 5));

        /* Player position: signed shorts at +8, +10, +12 converted to 16.16 fixed */
        u32 px = (u32)((s16)LDS16(tableEntry + 8))  << 16;
        u32 py = (u32)((s16)LDS16(tableEntry + 10)) << 16;
        u32 pz = (u32)((s16)LDS16(tableEntry + 12)) << 16;
        call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_ow_cutsceneLoadEntry(px, py, pz);

        /* Reset a camera auxiliary field */
        call_camera_writeUnk03_14_80301b2_ow_cutsceneLoadEntry(0, py);

        /* Camera position: signed shorts at +14, +16, +18 in 16.16 fixed */
        u32 cx = (u32)((s16)LDS16(tableEntry + 14)) << 16;
        u32 cy = (u32)((s16)LDS16(tableEntry + 16)) << 16;
        u32 cz = (u32)((s16)LDS16(tableEntry + 18)) << 16;
        call_SetCameraXYZ_ow_cutsceneLoadEntry(cx, cy, cz);

        /* Unlock the player's facing direction */
        call_ClearEventFlagFromImmediate_ow_cutsceneLoadEntry(0x17, EVENT_EVENT_CUR_DIR_LOCKED);
    }

    /* Store bit 7 of the flags field as the direction-lock indicator */
    ST8(context + 8, (flags >> 7) & 1);

    return 0;
}

Next ow_cutsceneLoadEntry(void) {
    R0 = ow_cutsceneLoadEntry_logic(R5);
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

/* ── ow_applySpawnEntry ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: SetOWPlayerFacingDirection has no typed C interface yet */
static inline void call_SetOWPlayerFacingDirection_ow_applySpawnEntry(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetOWPlayerFacingDirection);
}

typedef struct { u32 r3; } call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_result_ow_applySpawnEntry;
/* register form: generated wrapper: owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4 has no typed C interface yet */
static inline call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_result_ow_applySpawnEntry call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_ow_applySpawnEntry(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4);
    return (call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_result_ow_applySpawnEntry){ R3 };
}

/* register form: generated wrapper: camera_writeUnk03_14_80301b2 has no typed C interface yet */
static inline void call_camera_writeUnk03_14_80301b2_ow_applySpawnEntry(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(camera_writeUnk03_14_80301b2);
}

/* register form: generated wrapper: SetCameraXYZ has no typed C interface yet */
static inline void call_SetCameraXYZ_ow_applySpawnEntry(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(SetCameraXYZ);
}

typedef struct { u32 r1, r3; bool c, v; } call_ClearEventFlagFromImmediate_result_ow_applySpawnEntry;
/* register form: generated wrapper: ClearEventFlagFromImmediate has no typed C interface yet */
static inline call_ClearEventFlagFromImmediate_result_ow_applySpawnEntry call_ClearEventFlagFromImmediate_ow_applySpawnEntry(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ClearEventFlagFromImmediate);
    return (call_ClearEventFlagFromImmediate_result_ow_applySpawnEntry){ R1, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* ow_applySpawnEntry: applies a scripted player/camera state from the spawn table to the OW state.
 * r5 points to the overworld state; the script entry is selected by state[4] (0x14 bytes each). */

/* camera_writeUnk03_14 wrapper that preserves r10/r11 in the caller frame. */
static void call_camera_writeUnk03_preserve(u32 r0, u32 r1) {
    /* register form: camera_writeUnk03_14 uses r10 as a base pointer; the caller must not have
     * r10 shifted into its slot.  A single push of r10 around the call prevents the trampoline
     * from landing on a stale value. */
    SP -= 4;
    ST32(SP, R10);
    call_camera_writeUnk03_14_80301b2_ow_applySpawnEntry(r0, r1);
    R10 = LD32(SP);
    SP += 4;
}

/* Apply one script entry; returns 0 (always).  If the entry's coord flag is set, also calls
 * SetOWPlayerFacingDirection / owPlayer coords / camera write / SetCameraXYZ / ClearEventFlag. */
static u32 apply_spawn_script(u32 state) {
    u8 index = (u8)LD8(state + 4);
    u32 entry = A_byte_809929C + 0x14u * index;

    u32 dest = LD32(entry);
    if (dest != 0)
        ST32(state + 0x30, dest);

    ST8(state + 5, LD8(entry + 4));

    if (LD8(entry + 7) == 0)
        return 0;

    call_SetOWPlayerFacingDirection_ow_applySpawnEntry(LD8(entry + 5));

    s32 px = (s32)LDS16(entry + 0x8) << 16;
    s32 py = (s32)LDS16(entry + 0xA) << 16;
    s32 pz = (s32)LDS16(entry + 0xC) << 16;
    call_owPlayer_indirectlySetPlayerCoordsMaybe_809e1a4_ow_applySpawnEntry(px, py, pz);

    call_camera_writeUnk03_preserve(0, py);

    s32 cx = (s32)LDS16(entry + 0xE) << 16;
    s32 cy = (s32)LDS16(entry + 0x10) << 16;
    s32 cz = (s32)LDS16(entry + 0x12) << 16;
    call_SetCameraXYZ_ow_applySpawnEntry(cx, cy, cz);

    call_ClearEventFlagFromImmediate_ow_applySpawnEntry(0x17u, 0x15u);
    return 0;
}

/* ow_applySpawnEntry(r5 = OW state ptr, r10 = global, lr) -> nz r2 r3 r4 r5 r7; clobbers c v r0 r1 */
Next ow_applySpawnEntry(void) {
    R0 = apply_spawn_script(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── camera_followOverworldPlayer ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_camera_writeUnk03_14_80301b2_result;
/* register form: generated wrapper: camera_writeUnk03_14_80301b2 has no typed C interface yet */
static inline call_camera_writeUnk03_14_80301b2_result call_camera_writeUnk03_14_80301b2_camera_followOverworldPlayer(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(camera_writeUnk03_14_80301b2);
    return (call_camera_writeUnk03_14_80301b2_result){ R2 };
}

/* Follow the overworld player for the camera: passes the player object's
 * field at +0x1C to the camera-write helper alongside mode 1.
 * r10 (toolkit pointer) is read as an argument; the callee sets r2. */
static void followOverworldPlayerCamera(u32 toolkit) {
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 playerObj = LD32(gameState + oGameState_OverworldPlayerObjectPtr);
    call_camera_writeUnk03_14_80301b2_camera_followOverworldPlayer(1, playerObj + 0x1C);
}

/* camera_followOverworldPlayer(r10 = toolkit) -> r0=0, nz, r2 from callee */
Next camera_followOverworldPlayer(void) {
    followOverworldPlayerCamera(R10);
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── pet_exitToPetMenuReturnZero_809ADA8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3, r8, r9, r10, r11, r12; } call_pet_returnToPetMenu_result;
/* register form: generated wrapper: pet_returnToPetMenu has no typed C interface yet */
static inline call_pet_returnToPetMenu_result call_pet_returnToPetMenu(u32 r4, u32 r5, u32 r6, u32 r7, u32 r9, u32 r11, u32 r12) {
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R9 = r9;
    R11 = r11;
    R12 = r12;
    GUEST_CALL(pet_returnToPetMenu);
    return (call_pet_returnToPetMenu_result){ R2, R3, R8, R9, R10, R11, R12 };
}

/* Forward everything to pet_returnToPetMenu and then report failure/success
 * as 0 in r0, like the original ("bl pet_returnToPetMenu; mov r0, #0").
 * pet_returnToPetMenu's own register results (r2, r3, r8-r12) are already in
 * the guest registers after the wrapper call, and r4-r7 are preserved, so
 * only r0 and the flags need to be produced here. */
static u32 return_to_pet_menu(u32 r4, u32 r5, u32 r6, u32 r7, u32 r9, u32 r11, u32 r12) {
    (void)call_pet_returnToPetMenu(r4, r5, r6, r7, r9, r11, r12);
    return 0;
}

/* pet_exitToPetMenuReturnZero_809ADA8: leave the current screen and return to the PET menu.
 * inputs: r4 r5 r6 r7 r9 r10 r11 r12 (r10 is kept as on entry by the wrapper)
 * outputs: nz from the returned 0; r2 r3 r8-r12 come from pet_returnToPetMenu. */
Next pet_exitToPetMenuReturnZero_809ADA8(void) {
    R0 = return_to_pet_menu(R4, R5, R6, R7, R9, R11, R12);
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

/* ── camera_restartWithScriptHead ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_SetCutsceneCameraScript_result;
/* register form: generated wrapper: SetCutsceneCameraScript has no typed C interface yet */
static inline call_SetCutsceneCameraScript_result call_SetCutsceneCameraScript(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetCutsceneCameraScript);
    return (call_SetCutsceneCameraScript_result){ R2 };
}

/* Prepare the cutscene camera script buffer from the coordinate data the camera
 * struct points to, flag the camera for restart, and load the script.
 * Returns the r2 value produced by SetCutsceneCameraScript. */
static u32 camera_restart_with_script_head(u32 camera) {
    u32 script_data = LD32(camera + 4);
    ST32(camera + 0x70, script_data);

    u32 buf = A_byte_20065E0;
    ST8(buf, 0);

    /* Pack each 16-bit coordinate field (upper half) into two bytes, shifted left by 4 */
    u32 val = (LD32(script_data + 0) >> 16) << 4;
    ST8(buf + (A_byte_20065E1 - A_byte_20065E0), val);
    ST8(buf + (A_byte_20065E1 - A_byte_20065E0) + 1, val >> 8);

    val = (LD32(script_data + 4) >> 16) << 4;
    ST8(buf + (A_byte_20065E3 - A_byte_20065E0), val);
    ST8(buf + (A_byte_20065E3 - A_byte_20065E0) + 1, val >> 8);

    val = (LD32(script_data + 8) >> 16) << 4;
    ST8(buf + (A_byte_20065E5 - A_byte_20065E0), val);
    ST8(buf + (A_byte_20065E5 - A_byte_20065E0) + 1, val >> 8);

    /* Trigger byte: 8 at the last slot */
    ST8(buf + (A_byte_20065E7 - A_byte_20065E0), 8);

    /* Mark the camera struct: restart flag and script-pointer slot */
    ST8(camera + 0x13, 1);
    ST32(camera + 0x3C, buf);

    /* Load the script into the camera system */
    return call_SetCutsceneCameraScript(buf).r2;
}

/* camera_restartWithScriptHead(r5 = camera ptr) -> nz r2 r3 */
Next camera_restartWithScriptHead(void) {
    R2 = camera_restart_with_script_head(R5);
    R3 = (A_byte_20065E5 - A_byte_20065E0) + 1;   /* last byte offset after the third pair */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── ow_player_updateCoords_809C7FC ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188_result;
/* register form: generated wrapper: owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188 has no typed C interface yet */
static inline call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188_result call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188);
    return (call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188_result){ R3 };
}

/* Per-frame player coordinate update (an overworld object callback).
 * r5 holds the player object; the word at obj+0x70 points at its coordinate
 * block, whose three words (offsets +0, +4, +8) are pushed through the shared
 * helper that copies them to the "next" coords, writes them back and then
 * applies them again through its own indirection.
 *
 * Everything the original leaves behind for its callers is already in place
 * after the helper call: r2/r3 are the helper's leftovers and r4-r7 are
 * untouched (the original's push/pop around the call restores them anyway),
 * so only the flags need to be set here (mov r0,#0 at the end clears N and
 * sets Z; r0 itself is a clobber and callers never read it). */
static void ow_run_player_coord_update(u32 player) {
    u32 coords = LD32(player + 0x70);
    call_owPlayer_copyCoordsToNextCoordsWritePlayerCoordsThenIndirectlySetPlayerCoordsMaybe_809e188(
        LD32(coords + 0),
        LD32(coords + 4),
        LD32(coords + 8));
}

Next ow_player_updateCoords_809C7FC(void) {
    ow_run_player_coord_update(R5);
    SET_FLAGS_NZ(0);
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

/* ── loadWorkByteFromStepTable_809C830 ── */
/* loadWorkByteFromStepTable_809C830 -- copy one lookup byte into the work object.
 *
 * The state block pointed to by [r5+0x70] holds two selector bytes:
 *   byte 0xC picks one of the four words in the table at off_809C850,
 *   byte 0xD indexes into the word that was picked.
 * The byte read that way is stored at [r5+5].
 *
 * Register contract: r5 in (work object); r2 left holding the selected
 * table word and r3 the state pointer for the caller; r4-r7 restored;
 * flags end as "mov r0,#0" leaves them (Z set, N clear); r0/r1 scratch.
 */
struct Sub809C830Result {
    u32 r2;   /* selected table word */
    u32 r3;   /* state pointer loaded from [r5+0x70] */
};

static struct Sub809C830Result lookup_work_byte(u32 obj) {
    struct Sub809C830Result out;

    u32 state = LD32(obj + 0x70);
    u32 table_word = LD32(A_off_809C850 + (LD8(state + 0xC) << 2));
    u32 index = LD8(state + 0xD);

    ST8(obj + 5, LD8(table_word + index));

    out.r2 = table_word;
    out.r3 = state;
    return out;
}

Next loadWorkByteFromStepTable_809C830(void) {
    struct Sub809C830Result out = lookup_work_byte(R5);
    R2 = out.r2;
    R3 = out.r3;
    SET_FLAGS_NZ(0);
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

/* ── cutscene_clearEntryEventFlags_809C940 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { bool c, v; } call_ClearEventFlag_result_sub_809C940;
/* register form: generated wrapper: ClearEventFlag has no typed C interface yet */
static inline call_ClearEventFlag_result_sub_809C940 call_ClearEventFlag_sub_809C940(u32 r0) {
    R0 = r0;
    GUEST_CALL(ClearEventFlag);
    return (call_ClearEventFlag_result_sub_809C940){ CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Clear every event flag in the cutscene state's entry flag bitfield.
 * Takes and returns r5, which the original preserves; r10 is read by
 * ClearEventFlag and stays as on entry. */
static u32 clear_cutscene_entry_flags(u32 r5) {
    u32 cutsceneState = LD32(A_eCutsceneState + 0x70);
    u32 entryFlagBitfield = LD32(cutsceneState + 0x14);

    call_ClearEventFlag_sub_809C940((u16)entryFlagBitfield);
    return r5;
}

Next cutscene_clearEntryEventFlags_809C940(void) {
    R5 = clear_cutscene_entry_flags(R5);
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

/* ── sub_809CA84 ── */
struct TileLookupResult {
    u32 value;   /* r0: byte from tile data + 1, or 0 if no tile data */
    u32 stride;  /* r2: 0x18 if tile data found, else unchanged */
};

/* Look up a byte in the current map's tile data table at the player's
 * position index (offset 0x83 of the overworld player object).
 * Returns 0 when the tile has no entry; otherwise the byte at offset 0xD
 * in the tile record for the player's slot, plus 1.
 * The caller also uses r2 as the record stride (0x18) when a hit occurs. */
static struct TileLookupResult tile_lookup(u32 toolkit, u32 prev_stride) {
    struct TileLookupResult res;
    res.value  = 0;
    res.stride = prev_stride;

    u32 player_index = LD8(A_eOWPlayerObject + 0x83);
    u32 game_state   = LD32(toolkit + oToolkit_GameStatePtr);
    u32 group_index  = (LD8(game_state + oGameState_MapGroup) - 0x90) << 2;
    u32 map_index    = LD8(game_state + oGameState_MapNumber) << 2;

    u32 table_row = LD32(A_off_809C0F0 + group_index);
    u32 tile_ptr  = LD32(table_row + map_index);

    if (tile_ptr != 0) {
        res.stride = 0x18;
        res.value  = LD8(tile_ptr + (player_index - 0x30) * 0x18 + 0xD) + 1;
    }

    return res;
}

/* sub_809CA84(r10 = toolkit) -> r0, r2; preserves r4-r7 */
Next sub_809CA84(void) {
    struct TileLookupResult res = tile_lookup(R10, R2);
    R0 = res.value;
    R2 = res.stride;
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
