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
        u32 scrIdx  = LD8(gs + oGameState_Unk_15);
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
