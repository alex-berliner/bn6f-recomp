/*
 * asm28_0.c: readable functions of bn6f asm28_0.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

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
