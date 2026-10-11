/*
 * asm28_0.c: readable functions of bn6f asm28_0.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_8098BE8 ── */
#define MAIL_ADD_BASE           0x1CA0
#define MAIL_RECEIVED_FLAG_BASE 0x1D20
#define MAIL_DONE_FLAG_BASE     0x1DA0

/* register form: calls addMail_802f238 outside this file, which has no typed C interface yet */
static void add_mail(u32 mail_entry) {
    R0 = mail_entry;
    GUEST_CALL(addMail_802f238);
}

/* register form: calls ClearEventFlag outside this file, which has no typed C interface yet */
static void clear_event_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(ClearEventFlag);
}

/* register form: calls SetEventFlag outside this file, which has no typed C interface yet */
static void set_event_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(SetEventFlag);
}

/* Deliver the mail referenced by a cutscene event and mark it received/done.
 * data is the cutscene command struct: [data+6] = mail id (0xFF = none),
 * [data+7] = done flag (0 = nothing to do).  Always returns 0. */
static u32 deliver_cutscene_mail(u32 data) {
    u32 mail_id = LD8(data + 6);
    if (mail_id == 0xFF)
        return 0;
    add_mail(MAIL_ADD_BASE + mail_id);
    if (LD8(data + 7) == 0)
        return 0;
    /* Re-read mail id: addMail may have altered the struct. */
    mail_id = LD8(data + 6);
    clear_event_flag(MAIL_RECEIVED_FLAG_BASE + mail_id);
    mail_id = LD8(data + 6);
    set_event_flag(MAIL_DONE_FLAG_BASE + mail_id);
    return 0;
}

/* sub_8098BE8(r5 = cutscene data ptr) -> r0 = 0, flags */
Next sub_8098BE8(void) {
    R0 = deliver_cutscene_mail(R5);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_809C968 ── */
/* register form: calls sub_8034C36 outside this file, which has no typed C interface yet */
static bool call_8034C36(void) {
    GUEST_CALL(sub_8034C36);
    return !ZERO_FLAG();  /* Z set = should not proceed */
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_set_sub_809C968(u32 group, u32 index) {
    R0 = group;
    R1 = index;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();  /* Z set = flag clear */
}

/* register form: calls SetEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void event_flag_enable(u32 group, u32 index) {
    R0 = group;
    R1 = index;
    GUEST_CALL(SetEventFlagFromImmediate);
}

/* register form: calls ClearEventFlagFromImmediate outside this file, which has no typed C interface yet */
static void event_flag_disable(u32 group, u32 index) {
    R0 = group;
    R1 = index;
    GUEST_CALL(ClearEventFlagFromImmediate);
}

/* register form: calls IsCutsceneScriptNonNull outside this file, which has no typed C interface yet */
static bool cutscene_script_active(void) {
    GUEST_CALL(IsCutsceneScriptNonNull);
    return !ZERO_FLAG();
}

/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool test_event_flag(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();
}

/* register form: calls CheckKeyItem outside this file, which has no typed C interface yet */
static u32 check_key_item(u32 item) {
    R0 = item;
    GUEST_CALL(CheckKeyItem);
    return R0;
}

/* register form: calls StartCutscene outside this file, which has no typed C interface yet */
static void start_cutscene(u32 script, u32 param) {
    R0 = script;
    R1 = param;
    GUEST_CALL(StartCutscene);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_effect(u32 sound) {
    R0 = sound;
    GUEST_CALL(PlaySoundEffect);
}

/* Cutscene lookup: only entered when both 1B9 and 1BA flags are set.
 * Returns false always (caller must do cleanup). */
static bool attempt_cutscene_entry(u32 toolkit) {
    u32 gs = LD32(toolkit + oToolkit_GameStatePtr);

    /* Only proceed if the current coordinate interaction value is zero */
    if (LD8(gs + oGameState_CoordInteractionValue) != 0)
        return false;

    u8 playerCoord = LD8(A_eOWPlayerObject + 0x83);
    u8 mapGroup    = LD8(gs + oGameState_MapGroup);
    u8 mapNumber   = LD8(gs + oGameState_MapNumber);

    /* Two-level pointer table indexed by (mapGroup - 0x90) and mapNumber */
    u32 table1 = LD32(A_off_809C0F0 + ((u32)(mapGroup - 0x90) << 2));
    u32 entryBase = LD32(table1 + ((u32)mapNumber << 2));
    if (entryBase == 0)
        return false;

    /* Each entry is 0x18 bytes, indexed by (playerCoord - 0x30) */
    u32 entry = entryBase + ((u32)(playerCoord - 0x30) * 0x18);

    /* Skip if this entry's event flag is already set */
    if (test_event_flag(LD32(entry + 0x10)))
        return false;

    /* Check whether the player has enough of key item 0x2C */
    u32 haveCount = check_key_item(0x2C);
    u32 required  = (u32)LD8(entry + 0xD) + 1;
    if ((s32)haveCount < (s32)required) {
        play_sound_effect(SOUND_CANT_JACK_IN);
        return false;
    }

    start_cutscene(A_byte_809C354, entry);
    return false;
}

/* Main logic: returns true if the caller should skip flag cleanup (early-return paths),
 * false if both 1BA and 1B9 must be cleared. */
static bool process_coord_interaction(u32 toolkit) {
    if (!call_8034C36())
        return false;

    /* EVENT_224 must be active to proceed */
    if (!event_flag_set_sub_809C968(2, 0x24))
        return false;

    if (event_flag_set_sub_809C968(1, 0xB9)) {
        /* EVENT_1B9 is set: check the 1BA companion */
        if (!event_flag_set_sub_809C968(1, 0xBA)) {
            event_flag_enable(1, 0xBA);
            return true;  /* early return, no cleanup */
        }
        return attempt_cutscene_entry(toolkit);
    }

    /* EVENT_1B9 is not set: try to record a valid coord interaction */
    u32 gs = LD32(toolkit + oToolkit_GameStatePtr);
    if ((s32)LD8(gs + oGameState_MapGroup) < 0x80)
        return false;
    if (cutscene_script_active())
        return false;

    u8 coord = LD8(gs + oGameState_CoordInteractionValue);
    if (coord == 0 || (s32)coord < 0x30 || (s32)coord > 0x33)
        return false;

    /* Store the interaction coord into the player object and set EVENT_1B9 */
    ST8(A_eOWPlayerObject + 0x83, coord);
    event_flag_enable(1, 0xB9);
    return true;  /* early return, no cleanup */
}

/* Clear both companion flags on the cleanup path */
static void cleanup_coord_flags(void) {
    event_flag_disable(1, 0xBA);
    event_flag_disable(1, 0xB9);
}

static void handle_coord_interaction(u32 toolkit) {
    if (!process_coord_interaction(toolkit))
        cleanup_coord_flags();
}

/* sub_809C968(r10 = toolkit pointer) → r2 (callee leftover), r4-r7/r10 preserved */
Next sub_809C968(void) {
    handle_coord_interaction(R10);
    RETURN_TO_CALLER();
}
