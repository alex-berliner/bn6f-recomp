/*
 * leaves_farm_26.c: small functions from the model farm, set 26.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── CutsceneCmd_write_or_offset_ow_player_fixed_anim_select_8037dac ── */
/* CutsceneCmd_write_or_offset_ow_player_fixed_anim_select_8037dac:
 * Map-script cutscene command that sets the overworld player's fixed
 * animation select. It takes two script operands:
 *   - the animation value: an immediate byte, or (when the first script
 *     read yields the 0xFF marker) an index into the table at R5;
 *   - a flag: when nonzero, the animation value is a relative offset that
 *     gets added to the player's current facing direction.
 * Always reports success (R0 = 1) and advances the script pointer (R7)
 * past this command. */
Next CutsceneCmd_write_or_offset_ow_player_fixed_anim_select_8037dac(void) {
    /* First operand: a mode-2 script read returns 0xFF if the value is
     * stored as a raw byte right after it, otherwise it returns an index
     * into the lookup table at R5 and the value is loaded from there. */
    R6 = 2;
    GUEST_CALL(ReadMapScriptByte);
    if (R4 == 0xFF) {
        R6 = 3;
        GUEST_CALL(ReadMapScriptByte);
    } else {
        R4 = LD8(R5 + R4);
    }

    /* Second operand: read a flag; a nonzero flag turns the animation
     * value into an offset from the player's facing direction. */
    R0 = R4;
    R6 = 1;
    GUEST_CALL(ReadMapScriptByte);
    if (R4 != 0) {
        R4 = R0; /* GetOWPlayerFacingDirection preserves R4 */
        GUEST_CALL(GetOWPlayerFacingDirection);
        R0 = R0 + R4;
    }

    /* Apply the animation select to the overworld player. */
    GUEST_CALL(owPlayer_setS200ace0_fixedAnimationSelect_809e13c);

    /* Advance the script pointer past this command and report success. */
    SET_FLAGS_ADD(R7, 4);
    R7 = R7 + 4;
    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── playCertainMapMusicBasedOnEventByte_8036e44 ── */
/* playCertainMapMusicBasedOnEventByte_8036e44:
 * Pick the music track for the map the player is currently on. The per-map
 * music table is selected by byte 7 of the game state (map sub-area); within
 * it, entries are 8 bytes: a map group byte followed by a pointer to a list
 * of tracks indexed by the map number. If the current group has no entry,
 * fall back to track 0x63. Leaves r1 = map group, r2 = map number; r4-r7
 * are restored to their values on entry. */
Next playCertainMapMusicBasedOnEventByte_8036e44(void) {
    /* r4-r7 are scratch internally but must be restored (original pushes/pops them) */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 musicWork = LD32(R10 + oToolkit_S2001c04_Ptr);

    R1 = LD8(gameState + 4);   /* map group we're in        */
    R2 = LD8(gameState + 5);   /* map number within group   */

    /* select the track table for the current sub-area (game state byte 7) */
    u32 table = LD32(A_off_80360E4 + (LD8(gameState + 7) << 2));

    u32 entry = table;
    u32 track;
    for (;;) {
        u32 entryGroup = LD8(entry);
        if (entryGroup == 0xFF) {
            /* no entry for this map group: use the default track */
            track = 0x63;
            break;
        }
        if (entryGroup == R1) {
            /* found our group: look up the track for this map number */
            track = LD8(LD32(entry + 4) + R2);
            break;
        }
        entry += 8;
    }

    ST8(musicWork + 4, track); /* s2001c04 map music work area, offset 4 */

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── object_breakPanel ── */
/* object_breakPanel(r0 = object):
 * Breaks the panel underneath the given object, if there is one and it is
 * breakable. Panels that are already broken (flag 0x10) or belong to an
 * unbreakable kind (hole, holy panel, etc. -- mask 0x0F880080) are left
 * alone. On success the panel's state counter is bumped (after clearing the
 * old state bits), its type and animation are set to "broken", the
 * panel-break sound effect plays and r0 = 1; otherwise r0 = 0.
 * Flags: only c/v are observable, and they are simply whatever
 * object_getPanelDataOffset / PlaySoundEffect left behind (the TSTs in the
 * original don't touch them). */
#define PANEL_FLAG_ALREADY_BROKEN 0x00000010u
#define PANEL_FLAGS_UNBREAKABLE   0x0F880080u
#define PANEL_FLAGS_STATE_MASK    0x00003F5Fu
/* panel data field offsets not present in game.h */
#define PANEL_OFF_TYPE       2
#define PANEL_OFF_ANIMATION  6

Next object_breakPanel(void) {
    GUEST_CALL(object_getPanelDataOffset);

    if (R0 == 0) { /* object isn't standing on a tracked panel */
        R0 = 0;
        RETURN_TO_CALLER();
    }
    u32 panel = R0;

    R1 = LD32(panel + oPanelData_Flags);

    R2 = PANEL_FLAG_ALREADY_BROKEN;
    if ((R1 & R2) == 0) { /* panel not broken yet? */
        R0 = 0;
        RETURN_TO_CALLER();
    }

    R2 = PANEL_FLAGS_UNBREAKABLE;
    if ((R1 & R2) != 0) { /* ...but it's a kind we can't break */
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* Break it: clear the old state, bump the counter, set type + animation. */
    R1 = (R1 & ~PANEL_FLAGS_STATE_MASK) + 1;
    ST32(panel + oPanelData_Flags, R1);
    R2 = 1;
    ST8(panel + PANEL_OFF_TYPE, R2);
    ST8(panel + PANEL_OFF_ANIMATION, R2);

    R0 = 0x97; /* sound effect: panel broken */
    GUEST_CALL(PlaySoundEffect);

    R0 = 1;
    RETURN_TO_CALLER();
}

/* ── object_dimScreen ── */
/* oBattleObject_CurAction lives at offset 9 (the assembly accesses it as
 * [r5,#9]; the shared header only names it oBattleObject_CurAnim there). */
#define oBattleObject_CurAction 9

/* object_dimScreen(r5 = battle object):
 * Battle-scene step that dims the screen (e.g. when the round is decided).
 * On the first invocation it starts a screen fade, resets the object's timer
 * and marks the phase as initialized; on every invocation it ticks the timer
 * and, once the fade has finished, advances CurAction by 4 and clears
 * CurPhaseAndPhaseInitialized. Flags and R0-R3 are left as the original
 * leaves them: R0 is the (nonzero) IsScreenFadeActive result on the early
 * return, 0 otherwise; N/Z come from the last TST/MOV, C/V from the last
 * Thumb ADDS. */
Next object_dimScreen(void) {
    /* One-time setup: begin the fade to black, reset the timer. */
    if (LD8(R5 + oBattleObject_PhaseInitialized) == 0) {
        R0 = 0x3C;
        R1 = 4;
        GUEST_CALL(SetScreenFade);
        ST16(R5 + oBattleObject_Timer, 0);
        ST8(R5 + oBattleObject_PhaseInitialized, 4);
    }

    /* Tick the fade timer every frame. */
    u32 timer = LD16(R5 + oBattleObject_Timer) + 1;
    SET_FLAGS_ADD(timer, 1); /* add r0, #1 sets C/V; N/Z overwritten below */
    ST16(R5 + oBattleObject_Timer, timer);

    /* Wait until the screen fade has completed. */
    GUEST_CALL(IsScreenFadeActive);
    if (R0 != 0) {
        SET_FLAGS_NZ(R0); /* tst r0, r0 */
        RETURN_TO_CALLER();
    }

    /* Fade done: move on to the next action state and reset the phase. */
    u32 action = LD8(R5 + oBattleObject_CurAction) + 4;
    SET_FLAGS_ADD(action, 4);
    ST8(R5 + oBattleObject_CurAction, action);

    SET_FLAGS_NZ(0); /* mov r0, #0 */
    ST16(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 0);
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── object_getPanelsInRowFiltered ── */
/* object_getPanelsInRowFiltered(r0 = row, r2/r3 = filter parameters,
 *                               r7 = output byte buffer):
 * Walks the six columns of the given row and, for each panel that
 * object_checkPanelParameters accepts, writes its encoded coordinate
 * (column | row << 4) into the buffer pointed to by r7.
 * Returns r0 = number of panels written; r4-r7 are preserved. */
Next object_getPanelsInRowFiltered(void) {
    u32 filterA = R2;
    u32 filterB = R3;
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;

    R5 = R0; /* row to scan */
    R6 = 0;  /* panels written so far */

    for (R4 = 1; (s32)R4 <= 6; R4++) {
        R0 = R4;
        R1 = R5;
        R2 = filterA;
        R3 = filterB;
        GUEST_CALL(object_checkPanelParameters);
        if (R0 != 0) {
            ST8(R7 + R6, R4 | (R5 << 4));
            R6++;
        }
    }

    R0 = R6;
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── object_spawnHiteffect ── */
/* object_spawnHiteffect(r5 = battle object that just took a hit):
 * When a collision lands on this object, react by playing the hit sound
 * and spawning the hit-spark effect over the object. Does nothing while
 * the battle is paused, or when the collision flags don't carry the
 * "spawn hit effect" bit. r5 (and r6/r7/r10) are left untouched. */
Next object_spawnHiteffect(void) {
    u32 savedR4 = R4;

    /* don't spawn anything while the battle is paused */
    GUEST_CALL(battle_isPaused);
    if (R0 != 0)
        RETURN_TO_CALLER();

    /* check the collision flags for the "show hit effect" bit */
    R0 = LD32(LD32(R5 + oBattleObject_CollisionDataPtr) + oCollisionData_FlagsFromCollision);
    R1 = 0x20000;
    u32 hitFlag = R0 & R1;
    SET_FLAGS_NZ(hitFlag);
    if (hitFlag == 0)
        RETURN_TO_CALLER();

    /* SFX: object took a hit */
    R0 = 0x6E;
    GUEST_CALL(PlaySoundEffect);

    /* spawn the hit-spark effect at the object's position (three
     * fixed-point words at +0x34). Lift the last coordinate by 16.0 so
     * the spark appears above the object, then let the helper jitter
     * two of the coordinates by up to 15 units. */
    u32 pos = R5 + 0x34;
    R1 = LD32(pos);
    R2 = LD32(pos + 4);
    R3 = LD32(pos + 8) + 0x100000; /* 0x100000 = 16.0 fixed point */
    R0 = 15;
    GUEST_CALL(AddRandomVarianceToTwoCoords);

    /* request effect type 8 (hit spark) */
    R4 = 8;
    GUEST_CALL(sub_80E08C4);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── object_timefreezeBegin ── */
/* Offsets/constants used here that game.h does not export. */
#define oBattleObject_CurState 8   /* current state byte in the battle object */
#define CUR_STATE_UPDATE      4   /* object state: run normal update logic */

/* object_timefreezeBegin (0x0800B916): start a time freeze for the battle
 * object whose pointer is in R5.
 *
 * - Objects on a non-inverted network queue the freeze dispatch (0x40).
 * - Objects recognized by sub_800B8D8 (player side) set battle flag 4.
 * - The object's per-alliance entry in the freeze counter table
 *   (dword_200F3B8) is cleared, and the object moves to the UPDATE state.
 *
 * Flags on return match the original's last flag-setting instructions:
 * C from the index shift (lsl r0, r0, #2), N/Z from MOV r0, #CUR_STATE_UPDATE.
 * R0 = CUR_STATE_UPDATE, R1 = &dword_200F3B8, R2 = 0 on return. */
Next object_timefreezeBegin(void) {
    /* Only queue the freeze dispatch when the network is not inverted. */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(battle_networkInvert);
    if (R0 == 0) {
        R0 = 0x40;
        GUEST_CALL(dispatch_801DACC);
    }

    /* Player-side objects additionally raise battle flag 4. */
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(sub_800B8D8);
    if (R0 != 0) {
        R0 = 4;
        GUEST_CALL(battle_setFlags);
    }

    /* Clear dword_200F3B8[alliance]; the <<2 index shift sets the C flag. */
    u32 alliance = LD8(R5 + oBattleObject_Alliance);
    R0 = alliance << 2;
    cpu.c = (alliance >> 30) & 1;
    R1 = A_dword_200F3B8;
    R2 = 0;
    ST32(R1 + R0, 0);

    /* Mark the object as ready for its normal update. */
    R0 = CUR_STATE_UPDATE;
    SET_FLAGS_NZ(R0);
    ST8(R5 + oBattleObject_CurState, CUR_STATE_UPDATE);
    RETURN_TO_CALLER();
}

/* ── sub_800BF88 ── */
/* sub_800BF88: reset the six two-byte selection slots at byte_2036740.
 * Each slot gets its "count" byte cleared to 0 and its partner byte set to a
 * descending default value (0xE, 0xD, ..., 9). Before that it calls
 * sub_800C4BC (whose r2/r3/r5 results and carry/overflow flags the callers
 * of this function rely on) and stores 0x8C into the state word at
 * byte_203CB04. r4/r6/r7 are only used across the call, so they come back
 * unchanged, as in the original's push/pop. */
Next sub_800BF88(void) {
    /* sub_800C4BC may clobber r4/r6/r7; the original saves and restores
     * them around the call, so capture them here and put them back. */
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    GUEST_CALL(sub_800C4BC);
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;

    ST32(A_byte_203CB04, 0x8C);

    u32 slots = A_byte_2036740;
    for (u32 i = 0; i < 6; i++) {
        ST8(slots + 2 + 2 * i, 0);      /* even byte: count = 0        */
        ST8(slots + 3 + 2 * i, 0xE - i); /* odd byte: 0xE, 0xD, ... 9 */
    }

    /* Last flag-setting instruction of the original is "mov r0, #9". */
    SET_FLAGS_NZ(9);
    RETURN_TO_CALLER();
}

/* ── sub_800E5AC ── */
/* sub_800E5AC(r0, r1 = panel parameters, r5 = battle object):
 *
 * Gate for object/panel interaction. First object_isValidPanel decides
 * whether the panel identified by the two parameters exists at all; it
 * reports "invalid" by returning zero in r0 (and setting Z to match, which
 * is why the original can branch straight after the call). For an invalid
 * panel the whole call returns 0 in r0.
 *
 * Otherwise the panel flag is fetched with object_getFlag. Bit 4 of that
 * flag picks which half of the per-alliance table at byte_800E660 to use,
 * and the object's alliance picks the 8-byte row inside it. The two words
 * stored there are passed, together with the original two parameters, to
 * object_checkPanelParameters, whose r1-r3 results and flags are returned
 * to the caller unchanged.
 *
 * r4-r7 are preserved; r1-r3 are untouched when the panel is invalid and
 * come from object_checkPanelParameters otherwise.
 */
Next sub_800E5AC(void) {
    u32 paramA = R0;
    u32 paramB = R1;

    /* isValidPanel and getFlag both see the original r0/r1 (isValidPanel
     * may adjust them and getFlag consumes the result), so R0/R1 are left
     * alone until both calls are done. */
    GUEST_CALL(object_isValidPanel);
    if (R0 == 0) {                  /* invalid panel */
        R0 = 0;
        SET_FLAGS_NZ(R0);           /* mov r0,#0: Z set, C/V untouched */
        RETURN_TO_CALLER();
    }

    GUEST_CALL(object_getFlag);
    u32 flag = R0;

    u32 entry = (flag & 0x10)       /* bit 4 of the panel flag: table half */
              + ((u32)LD8(R5 + oBattleObject_Alliance) << 3);

    R2 = LD32(A_byte_800E660 + entry);
    R3 = LD32(A_byte_800E660 + entry + 4);
    R0 = paramA;
    R1 = paramB;
    GUEST_CALL(object_checkPanelParameters);

    /* r1-r3 and the flags pass through from object_checkPanelParameters. */
    RETURN_TO_CALLER();
}
