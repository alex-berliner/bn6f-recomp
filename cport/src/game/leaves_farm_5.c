/*
 * leaves_farm_5.c: small functions from the model farm, set 5.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── GetNaviStatsAddrGivenCurPETNavi ── */
/* GetNaviStatsAddrGivenCurPETNavi(r0 = current PET navi id):
 * returns &Toolkit->naviStatsArray[100 * GetNaviStatsIndexGivenCurPETNavi(navi)],
 * i.e. the NaviStats struct of the navi currently set in the PET. */
Next GetNaviStatsAddrGivenCurPETNavi(void) {
    GUEST_CALL(GetNaviStatsIndexGivenCurPETNavi);
    u32 stats = LD32(R10 + oToolkit_NaviStatsPtr);
    R0 = R0 * 100 + stats;
    RETURN_TO_CALLER();
}

/* ── SetBeastOutCounterTo3 ── */
/* SetBeastOutCounterTo3: set the current PET navi's BeastOut counter to 3
 * (re-arms the Beast Out limit, e.g. after a transformation ends). */
Next SetBeastOutCounterTo3(void) {
    GUEST_CALL(GetCurPETNavi); /* -> current navi id in r0 */

    R1 = oNaviStats_BeastOutCounter;
    R2 = 3;
    GUEST_CALL(SetCurPETNaviStatsByte); /* SetCurPETNaviStatsByte(r0, offset, 3) */

    RETURN_TO_CALLER();
}

/* ── object_updateCollisionPanels ── */
/* object_updateCollisionPanels(r4/r6 = scratch (restored), r5 = battle object):
 * Copies the object's current panel coordinates into its collision data record,
 * remembers the previous coordinates, then asks sub_800E994 which way the
 * object should face for the move from the old panel to the new one and stores
 * that as the collision data's direction.
 * Returns r0 = direction, r1/r2/r3 = new Y / old X / old Y (as passed to the
 * helper), with r4 and r6 preserved and the helper's flags left intact. */
Next object_updateCollisionPanels(void) {
    u32 object = R5;
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    u32 collision = LD32(object + oBattleObject_CollisionDataPtr);

    /* Previous panel position stored in the collision data. */
    u32 prevPanelX = LD8(collision + oCollisionData_PanelX);
    u32 prevPanelY = LD8(collision + oCollisionData_PanelY);

    /* Overwrite it with the object's current panel position. */
    u32 panelX = LD8(object + oBattleObject_PanelX);
    ST8(collision + oCollisionData_PanelX, panelX);
    u32 panelY = LD8(object + oBattleObject_PanelY);
    ST8(collision + oCollisionData_PanelY, panelY);

    /* direction = facing for the step (prevX, prevY) -> (panelX, panelY) */
    R0 = panelX;
    R1 = panelY;
    R2 = prevPanelX;
    R3 = prevPanelY;
    R4 = LD8(object + oBattleObject_Alliance);
    R6 = collision;
    GUEST_CALL(sub_800E994);
    u32 direction = R0;

    /* Restore the caller's r4/r6, as the original's push/pop does. */
    R4 = savedR4;
    R6 = savedR6;

    ST8(collision + oCollisionData_Direction, direction);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchActorHandlerByType_800F35C ── */
/* battle_dispatchActorHandlerByType_800F35C: dispatch an actor to its type-specific AI/behavior handler.
 *
 * The real work is done by indexByActorTypeAndAIIdxThenDispatch_800F420,
 * which indexes the table passed in R0 by the actor's type (and AI index)
 * and tail-calls the resulting handler. Here we only supply the dispatch
 * table, whose three entries select among:
 *   off_8109250 - virus handlers
 *   off_80F2604 - navi handlers
 *   off_80EA9A0 - player handlers
 * Everything except R0/R1 is passed through to (and left as set by) the
 * dispatched handler, so the caller sees its results in R2-R12. */
Next battle_dispatchActorHandlerByType_800F35C(void) {
    R0 = A_off_800F36C;
    GUEST_CALL(indexByActorTypeAndAIIdxThenDispatch_800F420);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchActorByTypeAndAIIdx_800F378 ── */
/* battle_dispatchActorByTypeAndAIIdx_800F378: dispatch the battle object described by R0's table through
 * indexByActorTypeAndAIIdxThenDispatch_800F420. The table at off_800F388
 * groups the per-actor-type dispatch tables:
 *   [0] virus, [1] navi, [2] player;
 * the callee indexes it by the object's actor type and AI index, then jumps
 * to the matching handler. This is a plain tail call: flags (c/v) and R1-R12
 * pass straight through from the callee to our caller. */
Next battle_dispatchActorByTypeAndAIIdx_800F378(void) {
    R0 = A_off_800F388;
    GUEST_CALL(indexByActorTypeAndAIIdxThenDispatch_800F420);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchActorAIByTypeAndAIIdx_800F394 ── */
/* battle_dispatchActorAIByTypeAndAIIdx_800F394: run the per-actor handler selected by the actor's type and
 * AI index. R0 points to a table of three handler tables (off_800F3A4)
 * that indexByActorTypeAndAIIdxThenDispatch_800F420 picks from; everything
 * else is forwarded through registers untouched. */
Next battle_dispatchActorAIByTypeAndAIIdx_800F394(void) {
    R0 = A_off_800F3A4;
    GUEST_CALL(indexByActorTypeAndAIIdxThenDispatch_800F420);
    RETURN_TO_CALLER();
}

/* ── ai_dispatchByTypeAndIndex_800F3E8 ── */
/* ai_dispatchByTypeAndIndex_800F3E8: AI dispatch thunk.
 *
 * Loads the address of the handler table at off_800F3F8 (three entries:
 * off_81094D0, off_80F27F8, off_80EAB94) into R0 and tail-calls
 * indexByActorTypeAndAIIdxThenDispatch_800F420, which picks one of the
 * table's routines based on the current actor's type and AI index and
 * runs it. All other arguments and results pass straight through, so
 * this function's register contract is just the dispatched handler's. */
Next ai_dispatchByTypeAndIndex_800F3E8(void) {
    R0 = A_off_800F3F8;
    GUEST_CALL(indexByActorTypeAndAIIdxThenDispatch_800F420);
    RETURN_TO_CALLER();
}

/* ── battle_dispatchActorTurnByTypeAndAIIdx_800F404 ── */
/* battle_dispatchActorTurnByTypeAndAIIdx_800F404: run the current battle actor's turn logic.
 * Looks up the handler for the actor's type/AI index via the version dispatch
 * table at off_800F414 and tail-calls the shared dispatcher with it. */
Next battle_dispatchActorTurnByTypeAndAIIdx_800F404(void) {
    R0 = A_off_800F414;
    GUEST_CALL(indexByActorTypeAndAIIdxThenDispatch_800F420);
    RETURN_TO_CALLER();
}

/* ── battle_drawCardToHand_800FC7C ── */
/* battle_drawCardToHand_800FC7C(r5 = battle object):
 * Draw the next card from the alliance's deck into its battle hand.
 * The hand buffer holds a byte card count followed by 2-byte card slots;
 * a slot value of 0xFFFF marks the end of the deck. Does nothing when the
 * hand already holds 5 cards or the deck is exhausted.
 * Leaves r1 = next card id, r2 = 0xFFFF (deck-empty marker), r3 = hand
 * address; flags come from the last comparison/add performed. */
Next battle_drawCardToHand_800FC7C(void) {
    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(getBattleHandAddr_8010018);

    u32 hand = R0;
    R3 = hand;

    u32 count = LD8(hand);
    if ((s32)count >= 5) {
        /* Hand is full: report the comparison against the hand limit. */
        SET_FLAGS_CMP(count, 5);
        RETURN_TO_CALLER();
    }

    u32 card = LD16(hand + count * 2 + 2);
    R1 = card;
    R2 = 0xFFFF;
    if (card == 0xFFFF) {
        /* Deck is empty: report the comparison against the end marker. */
        SET_FLAGS_CMP(card, 0xFFFF);
        RETURN_TO_CALLER();
    }

    /* Take the card: bump the hand count. */
    ST8(hand, count + 1);
    R0 = count + 1;
    SET_FLAGS_ADD(count, 1);
    RETURN_TO_CALLER();
}

/* ── battle_refreshNaviSpritePaletteFromForm_80100EC ── */
/* battle_refreshNaviSpritePaletteFromForm_80100EC: refresh the navi sprite palette of the battle object whose
 * pointer arrives in R0 (other inputs pass through to the helpers).
 * Reads the battle-stats byte at offset 0x2C: for the two cross/beast
 * transformed states (0x17..0x18) sub_8016A38 handles the sprite, otherwise
 * sub_801002C computes the palette index (returned in R0/R1) and it is
 * applied with sprite_setPalette. */
Next battle_refreshNaviSpritePaletteFromForm_80100EC(void) {
    R1 = 0x2C;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    if ((s32)R0 >= 0x17 && (s32)R0 <= 0x18) {
        /* transformed form: dedicated sprite update */
        GUEST_CALL(sub_8016A38);
    } else {
        /* normal form: pick the palette, then apply it */
        GUEST_CALL(sub_801002C);
        SYNC_SNAP(R0, R1);
        GUEST_CALL(sprite_setPalette);
        SYNC_CHECK(0x801010A, R0, R1);
    }

    RETURN_TO_CALLER();
}

/* ── battle_DispatchEnemyAiRoutine_8010DDA ── */
/* battle_DispatchEnemyAiRoutine_8010DDA(r0 = enemy index):
 * Battle AI dispatcher. Fetches the enemy's (version, actor_type, ai_index)
 * record, then calls the AI routine selected by actor type and AI index.
 * r3-r12 flow through untouched; the AI routine runs with the state the
 * original sets up for it: r0 = routine address, r1 = actor_type * 4,
 * r2 = 0, lr = 0x8010DF4 (so the routine returns to the "pop {pc}" below),
 * Z = 1 / N = 0 / C = 0 from "lsl r2,#2" + "mov r2,#0". When the routine
 * comes back, the pop sends us on to our own caller. */
Next battle_DispatchEnemyAiRoutine_8010DDA(void) {
    /* push {lr}: remember where our caller wants us to return. */
    u32 return_addr = LR;

    /* Look up this enemy's version/actor-type/AI-index record. */
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);
    u32 info = R0;

    u32 actor_type = LD8(info + 1);
    u32 ai_index   = LD8(info + 2);

    /* Two-level dispatch table: one routine table per actor type,
     * one routine per AI index within that table. */
    u32 type_table = LD32(A_off_8010E0C + (actor_type << 2));
    u32 ai_routine = LD32(type_table + (ai_index << 2));

    /* Set up the entry state the AI routine expects. The carry out of
     * "lsl r2, ai_index, #2" is always 0 for a byte value, and
     * "mov r2, #0" clears N and sets Z. */
    R0 = ai_routine;
    R1 = actor_type << 2;
    R2 = 0;
    SET_FLAGS_NZ(R2);
    cpu.c = 0;
    LR = 0x8010DF4;

    /* bx r0: run the selected AI routine; it returns to 0x8010DF4. */
    dispatch(ai_routine);

    /* pop {pc}: hand control back to our own caller. */
    LR = return_addr;
    RETURN_TO_CALLER();
}

/* ── battle_object_detachRelatedObject2_80111B8 ── */
/* battle_object_detachRelatedObject2_80111B8(r5 = battle object):
 * If the object has a "related object 2" attached, run that object's
 * removal routine (sub_80C44C8, called with the pointer in r0) and then
 * detach it by clearing the pointer field.
 * Returns: r0 = the pointer that was read (0 when nothing was attached),
 * r1 = whatever sub_80C44C8 left there (untouched when not attached),
 * flags as the original's last flag-setting instruction leaves them:
 * from the TST of the pointer on the empty path, from MOV r0,#0 otherwise. */
Next battle_object_detachRelatedObject2_80111B8(void) {
    u32 related = LD32(R5 + oBattleObject_RelatedObject2Ptr);

    if (related == 0) {
        R0 = related;
        SET_FLAGS_NZ(R0);
    } else {
        R0 = related;
        GUEST_CALL(sub_80C44C8);
        R0 = 0;
        ST32(R5 + oBattleObject_RelatedObject2Ptr, 0);
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── battle_RollNaviStatChance_8013FAE ── */
/* battle_RollNaviStatChance_8013FAE: random success check based on a battle object's navi stat byte
 * (stat id 0x11, via GetBattleNaviStatsByte_AllianceFromBattleObject).
 * Rolls RNG & 7 and returns 1 if the roll is <= (stat - 1), else 0; if the
 * stat is 0 it always fails.
 * Flag outputs: on the stat == 0 exit they come from the flag-setting
 * `mov r4, r0` (NZ only, C/V untouched); otherwise from the final CMP. */
Next battle_RollNaviStatChance_8013FAE(void) {
    R1 = 0x11;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    u32 stat = R0;

    if (stat == 0) {
        /* Fail immediately; R0 already holds the stat (0) from the callee. */
        SET_FLAGS_NZ(stat);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(GetRNG);
    u32 roll = R0 & 7;
    u32 limit = stat - 1;

    SET_FLAGS_CMP(roll, limit);
    R1 = limit;
    R0 = ((s32)roll > (s32)limit) ? 0u : 1u;
    RETURN_TO_CALLER();
}

/* ── battle_applyNaviStatDamageTick_8013FF8 ── */
/* battle_applyNaviStatDamageTick_8013FF8 (r5 = battle object): damage tick driven by the object's navi
 * stats. Reads stat byte 0x3D and deals that much damage to the object's HP,
 * but never reduces it below 1 (a stat of 0 means no damage at all).
 * Leaves c/v as the original does: from the callee on the early-out path,
 * from "cmp r1,#1" when returning at 1 HP, and from object_subtractHP
 * otherwise. r4/r6 are preserved; r1/r2 are outputs as in the original. */
Next battle_applyNaviStatDamageTick_8013FF8(void) {
    R1 = 0x3D;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    if (R0 != 0) {
        /* Damage cap: stat value - 1, so a weaker object keeps 1 HP. */
        R2 = R0 - 1;
        R1 = LD16(R5 + oBattleObject_HP);

        /* Already at 1 HP: leave it alone. */
        if (R1 == 1) {
            SET_FLAGS_CMP(R1, 1);
            RETURN_TO_CALLER();
        }

        /* If HP exceeds the cap the full stat value hits; otherwise only
         * HP-1 is dealt. R0 already holds the stat value. */
        if ((s32)R1 <= (s32)R2)
            R0 = R1 - 1;

        GUEST_CALL(object_subtractHP);
    }
    RETURN_TO_CALLER();
}

/* ── battle_EnableAIUnk0x48FlagForNonstandardBoard_80141F4 ── */

/* battle_EnableAIUnk0x48FlagForNonstandardBoard_80141F4:
 * Decide whether the AI's 0x48 flag needs to be raised. It is skipped when
 * the board uses one of the standard column patterns (default 0x38,
 * disadvantaged 0x30, advantaged 0x3C) or when the battle mode is the
 * Dustman mini game; otherwise the 0x400 bit is passed to
 * SetAIDataUnk0x48Flag. r1/r3 pass through unchanged from the callees. */
Next battle_EnableAIUnk0x48FlagForNonstandardBoard_80141F4(void) {
    GUEST_CALL(GetBattlePanelColumnPattern);
    u32 pattern = R0;

    if (pattern == 0x38 /* default pattern */
     || pattern == 0x30 /* disadvantaged */
     || pattern == 0x3C /* advantaged */) {
        RETURN_TO_CALLER();
    }

    GUEST_CALL(GetBattleMode);
    if (R0 == BATTLE_MODE_DUSTMAN_MINI_GAME)
        RETURN_TO_CALLER();

    R0 = 0x400;
    GUEST_CALL(SetAIDataUnk0x48Flag);
    RETURN_TO_CALLER();
}

/* ── battle_applySelfDamageFromBattleEffect_80142C2 ── */
/* battle_applySelfDamageFromBattleEffect_80142C2: called with r5 = battle object. If the current battle effects
 * include the 0x8 flag, write a self-damage of 10 into the object's collision
 * data. Leaves r0 = battle effects (or 10), r1 = 8 (or the collision data
 * pointer), and nz reflecting the last executed instruction. */
Next battle_applySelfDamageFromBattleEffect_80142C2(void) {
    u32 self = R5;

    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    R1 = 8;
    if (effects & 8) {
        R0 = 0xA;
        SET_FLAGS_NZ(R0);
        u32 collision = LD32(self + oBattleObject_CollisionDataPtr);
        R1 = collision;
        ST16(collision + oCollisionData_SelfDamage, 0xA);
    } else {
        SET_FLAGS_NZ(effects & 8);
    }

    RETURN_TO_CALLER();
}

/* ── battle_updateAngerTint_80143E4 ── */
/* battle_updateAngerTint_80143E4(r5 = battle object):
 * If the object's AI data records any anger, tint its sprites with color
 * shader 0xF (the "enraged" red flash). Flags pass through to the caller. */
Next battle_updateAngerTint_80143E4(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 anger = LD16(aiData + oAIData_Anger);

    /* tst r0, r0 — flags are observable by callers even when returning early */
    SET_FLAGS_NZ(anger);
    if (anger != 0) {
        R0 = 0xF;
        GUEST_CALL(sprite_setColorShader);
    }
    RETURN_TO_CALLER();
}

/* ── battle_TickAIDataTimer0f_8015A16 ── */
/* battle_TickAIDataTimer0f_8015A16(r0 = battle object):
 * Counts down the one-byte timer at oAIData_Unk_0f of the object's AI data.
 * The timer is left alone when it is disabled (0xFF), already at zero, or when
 * the object's navi stats byte 0x29 is non-zero (i.e. the feature is off).
 *
 * Exits with r1 = AI data pointer and r2 = new timer value on the countdown
 * path (r1 = 0x29 from the stats query on the early bail-outs), and with
 * r5 restored to its incoming value -- the original pushes/pops r5 around
 * the body, only using it as scratch for the object pointer and the call. */
Next battle_TickAIDataTimer0f_8015A16(void) {
    u32 battleObject = R0;
    u32 savedR5 = R5;

    if (battleObject == 0)
        RETURN_TO_CALLER();

    R5 = battleObject; /* object pointer, as the callee expects to see it */
    R1 = 0x29;         /* navi stats byte to query */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    R5 = savedR5;      /* pop {r5}: callers get their r5 back on all paths */

    if (R0 != 0)       /* stats byte set: timer not in use */
        RETURN_TO_CALLER();

    u32 aiData = LD32(battleObject + oBattleObject_AIDataPtr);
    u32 timer = LD8(aiData + oAIData_Unk_0f);
    R1 = aiData;

    if (timer != 0xFF && timer != 0) {
        timer--;
        ST8(aiData + oAIData_Unk_0f, timer);
    }
    R2 = timer;

    RETURN_TO_CALLER();
}

/* ── navi_DecreaseMood_8015C12 ── */
/* navi_DecreaseMood_8015C12(r0 = alliance flag, r1 = amount): fetch the active Navi's stats
 * and reduce its mood by `amount`, never letting it drop below 1. If the mood
 * is already 0, nothing is written. Callers read the flags left by the final
 * CMP with 1 (mood - amount compared against 1), so an underflow to the clamp
 * value still reports "less than 1". */
Next navi_DecreaseMood_8015C12(void) {
    u32 delta = R1;

    GUEST_CALL(GetBattleNaviStatsAddr); /* alliance flag in R0 -> R0 = NaviStats * */
    u32 stats = R0;

    u32 mood = LD8(stats + oNaviStats_Mood);
    SET_FLAGS_NZ(mood); /* TST r1, r1 */
    if (mood != 0) {
        u32 reduced = mood - delta;
        u32 stored = reduced;
        if ((s32)reduced < 1) /* BGE: signed comparison */
            stored = 1;
        ST8(stats + oNaviStats_Mood, stored);
        SET_FLAGS_CMP(reduced, 1);
    }
    RETURN_TO_CALLER();
}

/* ── battle_object_runPhaseHandler_8016396 ── */
/* battle_object_runPhaseHandler_8016396 — phase dispatcher for the battle object in R5.
 *
 * Reads the object's current phase byte and tail-calls the matching
 * handler from the table at off_80163A8:
 *   phase 0 -> sub_80163B4   phase 4 -> sub_801641A   phase 8 -> battle_objectIntroEntryAndActedSetup_8016460
 *
 * The phase value doubles as the byte offset into the table (Thumb loads
 * cannot scale the index register), which is why the stored phase values
 * step by 4 instead of 1.
 *
 * Register note: the original loads the branch target into R1 and leaves
 * it there across the call, so each handler is entered with R1 holding
 * its own Thumb entry address (table word, e.g. battle_objectIntroEntryAndActedSetup_8016460+1) — and the
 * handlers preserve R1. That value must therefore still be set up before
 * the call, exactly as the table load in the original does. The phase
 * byte reaches the handler in R0; all other registers pass through
 * untouched. Any phase outside the table would send the original through
 * a garbage branch target, so it can never occur in practice.
 */
Next battle_object_runPhaseHandler_8016396(void) {
    R0 = LD8(R5 + oBattleObject_CurPhase);

    /* Load the selected handler's address, as the original's
     * "ldr r1, [r1, r0]" does — the callee sees this value in R1. */
    R1 = LD32(A_off_80163A8 + R0);

    switch (R0) {
    case 0: GUEST_CALL(sub_80163B4); break;
    case 4: GUEST_CALL(sub_801641A); break;
    case 8: GUEST_CALL(battle_objectIntroEntryAndActedSetup_8016460); break;
    }

    RETURN_TO_CALLER();
}

/* ── object_applyTintIfFlag16_80168F0 ── */
/* object_applyTintIfFlag16_80168F0(r0 = object):
 * If the object's state flags have bit 16 set, re-tint its sprites with the
 * palette shader whose id was loaded from this function's literal pool.
 * Note: `ldr r0, word_8016908` fetches a full 32-bit pool word, so the id
 * 0x7E94 is accompanied by the next function's `push {lr}` opcode (0xB500)
 * in the upper half; sprite_setColorShader receives that whole value. */
Next object_applyTintIfFlag16_80168F0(void) {
    /* bl object_getFlag — the object pointer in r0 is passed through as-is. */
    GUEST_CALL(object_getFlag);

    /* mov r1,#1; lsl r1,#16: build the flag mask. Shifting the constant 1
     * by 16 always shifts out a zero bit, so this clears the carry flag
     * (callers read c/v after this routine, matching the original's flags). */
    R1 = 1 << 16;
    cpu.c = 0;

    if (R0 & R1) {                 /* tst r0, r1 */
        R0 = 0xB5007E94;           /* ldr r0, word_8016908 */
        GUEST_CALL(sprite_setColorShader);
    }

    RETURN_TO_CALLER();
}

/* ── battleObjectStateDispatcher_8016CE8 ── */
/* battleObjectStateDispatcher_8016CE8: battle object state dispatcher.
 *
 * R5 points to a battle object. Its byte at oBattleObject_Unk_0d holds a
 * *byte offset* into the handler table at off_8016CFC (Thumb `ldr rd,[rn,rm]`
 * adds the index register unscaled), so the three handlers are selected by
 * offsets 0, 4 and 8:
 *
 *   off_8016CFC + 0 -> sub_8016D08
 *   off_8016CFC + 4 -> sub_8016D8C
 *   off_8016CFC + 8 -> battle_objectCountdownPhaseOrReset_8016E3C
 *
 * The handler is invoked with R0 = the state byte and R1 = the handler
 * address left over from the table load; every other register passes through
 * unchanged. After the handler returns, control goes straight back to this
 * function's caller.
 */
Next battleObjectStateDispatcher_8016CE8(void) {
    u32 state = LD8(R5 + oBattleObject_Unk_0d);

    R0 = state;
    R1 = LD32(A_off_8016CFC + state);   /* handler address, as the original leaves it */

    switch (state) {
    case 0:
        GUEST_CALL(sub_8016D08);
        break;
    case 4:
        GUEST_CALL(sub_8016D8C);
        break;
    case 8:
        GUEST_CALL(battle_objectCountdownPhaseOrReset_8016E3C);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_clearObjectStatusFlagsAndTimers_801A264 ── */
/* battle_clearObjectStatusFlagsAndTimers_801A264(r5 = battle object): clear the object's active status flags
 * (both flag words) and reset all of its collision timers (blind, etc.).
 * Leaves r0 = 0, r3 = pointer to the collision data, and Z set (from r0 = 0). */
Next battle_clearObjectStatusFlagsAndTimers_801A264(void) {
    /* Clear status bits in the object's two flag words. */
    R0 = 0x8001E800;
    GUEST_CALL(object_clearFlag);

    R0 = 0x000300E8;
    GUEST_CALL(object_clearFlag2);

    R0 = 0;
    SET_FLAGS_NZ(R0);

    /* Zero out every timer field of the collision data. */
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    ST16(collision + oCollisionData_Unk_1c, 0);
    ST16(collision + oCollisionData_Unk_1e, 0);
    ST16(collision + oCollisionData_BlindTimer, 0);
    ST16(collision + oCollisionData_Unk_22, 0);
    ST16(collision + oCollisionData_Unk_2a, 0);
    ST16(collision + oCollisionData_Unk_2c, 0);

    RETURN_TO_CALLER();
}

/* ── battleObject_resetCollisionState_801A2B0 ── */
/* battleObject_resetCollisionState_801A2B0(r5 = battle object):
 * Reset an object's collision state: clear its status flag bits, zero the
 * collision timer, and restore its Z coordinate from its AI data block.
 * Leaves r0 = restored Z value, r3 = collision data pointer. */
Next battleObject_resetCollisionState_801A2B0(void) {
    /* Clear the object's main and secondary status flags. */
    R0 = 0x80000000;
    GUEST_CALL(object_clearFlag);
    R0 = 0x20000;
    GUEST_CALL(object_clearFlag2);

    /* Zero the collision data timer at offset 0x2c. */
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    ST16(collisionData + oCollisionData_Unk_2c, 0);

    /* Restore the object's Z position from the AI data block. */
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 z = LD16(aiData + oAIData_Unk_3c);
    ST16(R5 + oBattleObject_Z16, z);

    R3 = collisionData;
    R0 = z;
    RETURN_TO_CALLER();
}

/* ── battle_toggleAllianceAndAccumulateCollisionToAiData_801A308 ── */
/* battle_toggleAllianceAndAccumulateCollisionToAiData_801A308 (r5 = battle object):
 * Flip the object's alliance (player <-> enemy), fetch the AI data block for
 * the new side via battle_getPlayerAIData_8010022, then add the collision halfword at
 * CollisionData+0x92 into the AI's byte at oAIData_Unk_10.
 * Preserves r4/r5/r6; returns nothing. */
Next battle_toggleAllianceAndAccumulateCollisionToAiData_801A308(void) {
    u32 savedR4 = R4;
    u32 object = R5;

    /* Toggle the alliance field and get the matching AI data pointer. */
    R0 = LD8(object + oBattleObject_Alliance) ^ 1;
    GUEST_CALL(battle_getPlayerAIData_8010022);
    u32 aiData = R0;

    /* Accumulate the collision value into the AI data byte. */
    u32 collisionData = LD32(object + oBattleObject_CollisionDataPtr);
    u32 collisionValue = LD16(collisionData + 0x92);
    ST8(aiData + oAIData_Unk_10, LD8(aiData + oAIData_Unk_10) + collisionValue);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_incrementAllianceStatOnCollisionHit_801A6B4 ── */
/* battle_incrementAllianceStatOnCollisionHit_801A6B4(r5 = battle object):
 * Check the object's collision data (battle_checkHPDigit4State_801A6E8); if it reports a hit,
 * increment the navi stat byte at offset 0x18 of the object's alliance
 * stats, capped at 7. R4 (the collision data pointer) is restored on return. */
Next battle_incrementAllianceStatOnCollisionHit_801A6B4(void) {
    u32 savedR4 = R4;

    R4 = LD32(R5 + oBattleObject_CollisionDataPtr);
    GUEST_CALL(battle_checkHPDigit4State_801A6E8);

    if (R0 != 0) {
        R1 = 0x18;
        GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

        u32 level = R0 + 1;
        if ((s32)level > 7)
            level = 7;

        R1 = 0x18;
        R2 = level;
        GUEST_CALL(SetBattleNaviStatsByte_AllianceFromBattleObject);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A ── */
/* battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A(r2 = column index, r4 = caller value (preserved), r5 = battle object):
 * When the object's collision data says it is standing on a specific kind of panel
 * (byte at +0xA4 equal to 0xF6), aim the AI's Unk_12 marker two columns ahead,
 * saturating at 7, and run sub_801A77A with the AI data / collision data / object
 * pointers. Otherwise just fall through, leaving the compare flags behind for the
 * caller. r2 (and c/v when the callee runs) come back from sub_801A77A. */
Next battle_aiAimTwoColumnsAheadOnSpecialPanel_801A75A(void) {
    u32 saved_r4 = R4;
    u32 collisionData = LD32(R5 + oBattleObject_CollisionDataPtr);
    u32 collisionByte = LD8(collisionData + 0xA4);

    if (collisionByte == 0xF6) {
        u32 target;
        R3 = LD32(R5 + oBattleObject_AIDataPtr);
        /* The original reads the old Unk_12 value, but it is dead: r0 is
         * immediately overwritten by the add below. */
        target = R2 + 2;
        if ((s32)target > 7)
            target = 7;
        ST8(R3 + oAIData_Unk_12, target);
        R4 = collisionData;      /* sub_801A77A expects the collision data pointer in r4 */
        GUEST_CALL(sub_801A77A);
        R4 = saved_r4;           /* restore the caller's r4, as the original's pop does */
    } else {
        SET_FLAGS_CMP(collisionByte, 0xF6);
    }
    RETURN_TO_CALLER();
}

/* ── battle_checkObjectAction6Flag_801BABE ── */
/* battle_checkObjectAction6Flag_801BABE(r4 = ? passed through to callees, r5 = battle object):
 * Inspects the object's current action and reports what to do about it:
 *   0 - the object is not in action 6,
 *   1 - action 6, but its "flag 2" (bit 1) is clear,
 *   2 - action 6 with flag 2 set; battleObject_clearHitFlags_801A29A is run as a side effect.
 * Result in r0. r4 is restored (the original only pushes/pops it), r5 is
 * preserved, and r1/r2 are scratch. Flags come from the final "mov r0, r4"
 * (an adds r0, r4, #0), so they reflect the result value. */
Next battle_checkObjectAction6Flag_801BABE(void) {
    u32 savedR4 = R4;
    u32 result = 0;

    /* Offset 9 in the battle object struct: current action id. */
    u32 action = LD8(R5 + 9);
    R0 = action; /* r0 still holds the action byte when object_getFlag2 is entered */

    if (action == 6) {
        R4 = 1; /* callees observe r4 = 1 (set before both calls in the original) */

        GUEST_CALL(object_getFlag2); /* r0 = flag2 bits of the object */

        R1 = 2; /* mov r1, #2 before tst — also visible to battleObject_clearHitFlags_801A29A */
        if (R0 & R1) {
            GUEST_CALL(battleObject_clearHitFlags_801A29A); /* r0 = flag2 return value is still in r0 */
            result = 2;
        } else {
            result = 1;
        }
    }

    R4 = savedR4; /* pop {r4} in the original restores the caller's value */
    R0 = result;  /* mov r0, r4 */
    SET_FLAGS_ADD(result, 0);
    RETURN_TO_CALLER();
}

/* ── sub_801C984 ── */
/* sub_801C984: query sub_802CE60 twice and store the answers as 0/1 flags:
 *   [r5+7] = (sub_802CE60(0) != 0)
 *   [r5+9] = (sub_802CE60(1) != 0)
 * The result of the second query is also left in r0, with flags matching
 * the original's final CMP/MOVS. */
Next sub_801C984(void) {
    u32 flag;

    /* First query, normalize the answer to 0/1 and store it. */
    R0 = 0;
    GUEST_CALL(sub_802CE60);
    flag = (R0 != 0);
    ST8(R5 + 7, flag);

    /* Second query: the raw return value is kept in r0 (0 or non-zero,
     * matching the original's conditional `mov r0, #1`), and the flags
     * come from either the CMP or the MOVS, depending on the path. */
    R0 = 1;
    GUEST_CALL(sub_802CE60);
    if (R0 != 0) {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        SET_FLAGS_CMP(R0, 0);
    }
    ST8(R5 + 9, R0);

    RETURN_TO_CALLER();
}

/* ── background_drawPresetTileBlock_801C9C8 ── */
/* background_drawPresetTileBlock_801C9C8(r0 = destination column j, r1 = background id):
 * Draw one of the premade backgrounds. Each entry in the table at
 * byte_801D964 holds 16 halfwords of tile ids (2x3 blocks of 8x8 tiles);
 * the id selects the entry, which is copied from tile block 3 to the
 * background layer at position (j, i=2) with block size 4x2.
 * R0 (j) is passed through to CopyBackgroundTiles untouched; the caller's
 * R4/R5 are preserved across the call. */
Next background_drawPresetTileBlock_801C9C8(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    u32 tileIds = A_byte_801D964 + (R1 << 4); /* entry 1 = 16 halfwords */

    R3 = tileIds; /* tile_ids */
    R1 = 2;       /* i */
    R2 = 3;       /* which_tile_block_32x32 */
    R4 = 4;       /* j_size */
    R5 = 2;       /* i_size */
    GUEST_CALL(CopyBackgroundTiles);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── object_countdownTimer_801CA0C ── */
/* object_countdownTimer_801CA0C: tick the 8-bit countdown timer at [R5+0x0B]. Each call decrements
 * it by one; once it reaches zero it is left at 0xFF... no — once the decrement
 * yields zero, event/flag value 0x100 is reported through flags_clearBits_eStruct2035280_801BED6 and then
 * handed to the dispatcher dispatch_801DACC.
 *
 * Register contract: R5 is an object pointer (preserved). On the "timer not yet
 * expired" path the flags are those of the decrement (SUBS timer, #1) and R0
 * holds the new timer value; on the "expired" path R0-R3 and the flags are
 * whatever the two callees leave behind. */
Next object_countdownTimer_801CA0C(void) {
    u32 old = LD8(R5 + 0xB);
    u32 timer = old - 1;

    ST8(R5 + 0xB, timer);
    SET_FLAGS_CMP(old, 1);      /* flags of SUBS old, #1 */
    R0 = timer;

    if (timer != 0)
        RETURN_TO_CALLER();     /* still counting down */

    /* Timer expired: signal event 0x100. */
    R0 = 0x100;
    GUEST_CALL(flags_clearBits_eStruct2035280_801BED6);

    R0 = 0x100;
    GUEST_CALL(dispatch_801DACC);

    RETURN_TO_CALLER();
}

/* ── sub_801DDD8 ── */
/* sub_801DDD8:
 * Runs a fixed two-call sequence:
 *
 *   call_sub_3005EBA(6, 0, 3, 0, 0x12, 2)   -- six arguments in r0..r5
 *   flags_clearBits_20352C4_801BEC2(0x10, 4, ...)               -- r0 = 1 << 4; r2/r3 still hold
 *                                              whatever the first callee left
 *
 * The caller's r4/r5 are scratch here: they are overwritten with call
 * arguments and restored on the way out.  Everything else on return
 * (r0..r3, flags) is whatever flags_clearBits_20352C4_801BEC2 left behind. */
Next sub_801DDD8(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    /* call_sub_3005EBA(6, 0, 3, 0, 0x12, 2) */
    R0 = 6;
    R1 = 0;
    R2 = 3;
    R3 = 0;
    R4 = 0x12;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    /* mov r0,#1; mov r1,#4; lsl r0,r1: r0 = 0x10.  The Thumb LSL is the
     * last flag-setting instruction before the next call: N/Z come from
     * the result and C from the bit shifted out of 0x00000001, which is
     * 0.  flags_clearBits_20352C4_801BEC2 is entered with exactly those flags. */
    R0 = 1;
    R1 = 4;
    R0 = R0 << 4;
    SET_FLAGS_NZ(R0);
    cpu.c = 0;

    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── custGauge_Init_801DED0 ── */
/* custGauge_Init_801DED0: reset the custom gauge and reload its graphics.
 * Clears the gauge state, queues the gauge tiles from ROM into VRAM,
 * then runs the gauge-init routine (0x10) and dispatches to the
 * gauge handler (0x10). */
Next custGauge_Init_801DED0(void) {
    GUEST_CALL(ClearCustGauge);

    R0 = A_dword_86E489C; /* source: custom gauge tile data in ROM */
    R1 = 0x600C440;       /* destination: VRAM */
    R2 = 0x380;           /* transfer size */
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    R0 = 0x10;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);

    R0 = 0x10;
    GUEST_CALL(dispatch_801DA48);

    RETURN_TO_CALLER();
}

/* ── chatbox_openMessageWindow_801DFEA ── */
/* chatbox_openMessageWindow_801DFEA: lay out and activate a message window.
 *
 * First the window geometry is set up through call_sub_3005EBA:
 * origin (0,0), 0x12 x 3 cells, box style 0x11, palette/layer 2.
 * Then flags_clearBits_20352C4_801BEC2 is called with mode bits 0x40 to start the window.
 *
 * r4/r5 are only used to carry the 5th/6th arguments into
 * call_sub_3005EBA; they are restored to their entry values before
 * returning. r6/r7 are never touched. Exits with r0 = 0x40, r3 = 0,
 * r1/r2 as left by flags_clearBits_20352C4_801BEC2, and the flags of the `lsl r0, r0, #6`
 * (N=0, Z=0, C=0, V unchanged) unless flags_clearBits_20352C4_801BEC2 sets its own. */
Next chatbox_openMessageWindow_801DFEA(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    /* call_sub_3005EBA(0, 0x12, 3, 0, 0x11, 2): build the window */
    R0 = 0;
    R1 = 0x12;
    R2 = 3;
    R3 = 0;
    R4 = 0x11;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    /* flags_clearBits_20352C4_801BEC2(0x40): start the window running */
    R0 = 1 << 6;          /* mov r0,#1; lsl r0,#6 -> 0x40 */
    SET_FLAGS_NZ(R0);     /* result 0x40: N=0, Z=0 */
    cpu.c = 0;            /* no bit shifted out of the lsl */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    /* pop {r4,r5}: the caller's values are live again on return */
    R4 = saved_r4;
    R5 = saved_r5;

    RETURN_TO_CALLER();
}

/* ── battle_spawnObjectFromRecord_801E022 ── */
/* battle_spawnObjectFromRecord_801E022 (r5 = record describing the object to spawn):
 * Spawns a battle object through call_sub_3005EBA, feeding it the two
 * bytes stored at [r5+1] and [r5+2] together with the fixed parameter
 * set (r2 = 3, r3 = 0, r4 = 6, r5 = 2). Afterwards it invokes
 * flags_clearBits_20352C4_801BEC2 with 0x80 (built as mov r0,#1 ; lsl r0,#7).
 *
 * r4/r5 hold caller state and are preserved (the original saves and
 * pops them); r6/r7/r10 pass through untouched. The lsl that forms the
 * 0x80 argument is the routine's last flag-setting instruction: shifting
 * 1 left by 7 shifts out a zero, so the carry flag ends up clear. */
Next battle_spawnObjectFromRecord_801E022(void) {
    u32 callerR4 = R4;
    u32 callerR5 = R5;

    R0 = LD8(R5 + 1);
    R1 = LD8(R5 + 2);
    R2 = 3;
    R3 = 0;
    R4 = 6;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    R0 = 1u << 7; /* 0x80 */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    /* Carry-out of `lsl r0, r0, #7` with r0 = 1 is bit 25 of the old
     * value, which is clear. */
    cpu.c = 0;

    /* Restore the caller's r4/r5, as the original's pop {r4,r5,pc} does. */
    R4 = callerR4;
    R5 = callerR5;
    RETURN_TO_CALLER();
}

/* ── background_applyFixedTilePatch_801E03E ── */
/* background_applyFixedTilePatch_801E03E: refresh a 6x2-tile patch of the background.
 *
 * Reads the tile coordinates (j, i) from eStruct2035280 (offsets 1 and 2),
 * copies a 6x2 block of tile IDs from byte_801EDFC into 32x32 tile block 3,
 * then asks flagSetBits_eStruct2035280_801BEB8 (argument 0x80) to apply the update.
 * Preserves r4/r5 for the caller. */
Next background_applyFixedTilePatch_801E03E(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    u32 coords = A_eStruct2035280;
    u32 j = LD8(coords + 1); /* byte_2035281 */
    u32 i = LD8(coords + 2); /* byte_2035282 */

    R0 = j;
    R1 = i;
    R2 = 3;                  /* which tile block 32x32 */
    R3 = A_byte_801EDFC;     /* tile ids */
    R4 = 6;                  /* j size */
    R5 = 2;                  /* i size */
    GUEST_CALL(CopyBackgroundTiles);

    /* mov r0,#1 ; lsl r0,r0,#7 -> r0 = 0x80.
     * The shift is the last flag-setting instruction here: it sets N/Z
     * from the result (0x80) and C from the bit shifted out of 1 (0). */
    R0 = 1 << 7;
    SET_FLAGS_NZ(R0);
    cpu.c = 0;
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── sub_801E0A0 ── */
/* sub_801E0A0(r0, r1 = two byte parameters):
 * Copies the halfword at +0x24 of the struct pointed to by the entry at
 * +0x48 of eStruct2035280 into eStruct2035280+0x24, stores the two byte
 * parameters at +0x1 and +0x2, then runs the 0x80 (text-box) update path:
 * eStruct2035280_SetRequestBits_801BECC followed by dispatch_801DA48.
 * Leaves R1/R2/R3 and the c/v flags as dispatch_801DA48 left them. */
Next sub_801E0A0(void) {
    u32 boxStruct = A_eStruct2035280;

    u16 counter = LD16(LD32(boxStruct + 0x48) + 0x24);
    ST16(boxStruct + 0x24, counter);

    ST8(boxStruct + 1, R0);
    ST8(boxStruct + 2, R1);

    R3 = boxStruct;
    R0 = 0x80;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);

    R3 = boxStruct;
    R0 = 0x80;
    GUEST_CALL(dispatch_801DA48);

    RETURN_TO_CALLER();
}

/* ── sub_801E8CC ── */
/* sub_801E8CC(r0 = value, r5 = struct pointer):
 * Calls sub_801E95C with r4 = 0 (r0 is saved across the call and restored,
 * so the callee's r0 result is discarded). Afterwards, if the original r0
 * was 0x50 and the byte at [r5+0xC] is non-zero, subtracts 0x10 from the
 * byte at [r5+2] and stores it back. On return, r0 and the nz/c/v flags
 * reflect the outcome:
 *   - r0 != 0x50:          r0 = input value,   flags from CMP r0, 0x50
 *   - r0 == 0x50, [+C]==0: r0 = 0,             flags from CMP r0, 0
 *   - r0 == 0x50, [+C]!=0: r0 = byte - 0x10,   flags from SUBS r0, 0x10 */
Next sub_801E8CC(void) {
    u32 matchValue = R0;
    u32 savedR4 = R4;

    R4 = 0;
    GUEST_CALL(sub_801E95C);

    if (matchValue != 0x50) {
        R0 = matchValue;
        SET_FLAGS_CMP(matchValue, 0x50);
    } else if (LD8(R5 + 0xC) == 0) {
        R0 = 0;
        SET_FLAGS_CMP(0, 0);
    } else {
        u32 timer = LD8(R5 + 2);
        u32 updated = timer - 0x10;
        ST8(R5 + 2, updated);
        R0 = updated;
        SET_FLAGS_CMP(timer, 0x10);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── ai_DealQueuedAttackDamage_8112F34 ── */
/* ai_DealQueuedAttackDamage_8112F34(r7 = AI attack vars):
 * Fire one of the AI opponent's queued attacks: hand the attack's target
 * panel coordinates (Unk_16 = x, Unk_17 = y), an elevation of 0xA00 plus
 * Unk_0f, and the attack's damage (field at offset 8) to the damage
 * routine battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84, with r2/r3 cleared.
 * r4/r6/r7 are only scratch for setting up the call (the original saves
 * and restores them around the callee), so they end unchanged; r1-r3 are
 * whatever the callee leaves behind. */
Next ai_DealQueuedAttackDamage_8112F34(void) {
    u32 vars = R7;
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    R0 = LD8(vars + oAIAttackVars_Unk_16);
    R1 = LD8(vars + oAIAttackVars_Unk_17);
    R4 = 0xA00 + LD8(vars + oAIAttackVars_Unk_0f);
    R2 = 0;
    R3 = 0;
    R6 = LD32(vars + 8);

    GUEST_CALL(battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84);

    /* The original pushed r4/r6/r7 and pops them after the call, so the
     * caller's values survive the callee. */
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;

    RETURN_TO_CALLER();
}

/* ── sub_8116BFC ── */
/* sub_8116BFC(r4, r5 = state object, r6, r7 = state byte ptr):
 * Runs the two phase-update helpers (record_adjustNthSubRecordFieldC_8116C40, sub_8116C78), then checks
 * the byte at [r7]; if it has reached 8, store 8 into [r5+9] as well.
 * Leaves r0 = the loaded byte (or 8 on the store path) and the flags from
 * the final CMP/MOV for the caller to branch on. */
Next sub_8116BFC(void) {
    GUEST_CALL(record_adjustNthSubRecordFieldC_8116C40);
    GUEST_CALL(sub_8116C78);

    u32 counter = LD8(R7);
    R0 = counter;
    SET_FLAGS_CMP(counter, 8);
    if (counter == 8) {
        R0 = 8;             /* mov r0,#8 sets NZ only; C/V stay from the CMP */
        SET_FLAGS_NZ(R0);
        ST8(R5 + 9, 8);
    }
    RETURN_TO_CALLER();
}

/* ── battle_stepForwardAnimWait_8116DEC ── */
/* battle_stepForwardAnimWait_8116DEC(r5 = object state):
 * Runs sprite_addCoordinates for the "step forward" animation (pose 0x16,
 * parameter 0). The call returns the number of frames the animation has
 * run; once that reaches 0x11A the object's state byte at [r5+8] is set
 * to 8 to signal that the movement finished.
 * Flags: as the final CMP (elapsed vs 0x11A), except on the fall-through
 * path where the trailing "mov r0,#8" overwrites only n/z. */
Next battle_stepForwardAnimWait_8116DEC(void) {
    R0 = 0x16;
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    u32 elapsed = R0;
    u32 limit = 0x11A;
    R2 = limit;
    SET_FLAGS_CMP(elapsed, limit);
    if ((s32)elapsed >= (s32)limit) {
        R0 = 8;
        SET_FLAGS_NZ(8);
        ST8(R5 + 8, 8);
    }
    RETURN_TO_CALLER();
}

/* ── stateDispatcher_3states_8116E10 ── */
/* stateDispatcher_3states_8116E10: state dispatcher for the object in R5.
 *
 * Reads the word offset of the current state from [R5+8] and tail-calls the
 * matching handler out of the table at off_8116E24:
 *   offset 0 -> sub_8116E30, 4 -> battle_objectUpdateByKind_8116F34, 8 -> sub_8116FD4.
 *
 * The original does this with `mov lr, pc; bx r0` followed by `pop {pc}`,
 * so the handler returns straight to this function's caller: it is a tail
 * call. The handler is entered with R0 = its own table entry (as the
 * original leaves the loaded target address in R0) and R1 = the table
 * offset; every other register passes through untouched, and the handler's
 * results (registers and flags) become this function's results.
 */
Next stateDispatcher_3states_8116E10(void) {
    u32 sel = LD8(R5 + 8);                      /* word offset into the handler table */
    u32 handler = LD32(A_off_8116E24 + sel);    /* Thumb address of the state handler */

    R1 = sel;
    R0 = handler;

    switch (sel) {
    case 0:
        GUEST_CALL(sub_8116E30);
        break;
    case 4:
        GUEST_CALL(battle_objectUpdateByKind_8116F34);
        break;
    case 8:
        GUEST_CALL(sub_8116FD4);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sub_81176B0 ── */
/* sub_81176B0(r5 = state struct):
 * Mark the entity as active (flag 1) and set its mode/counter field at +8 to 4,
 * then run the shared update at sub_81176C0. Flags and r0-r3 on return come
 * from that call. */
Next sub_81176B0(void) {
    ST8(R5, 1);
    ST8(R5 + 8, 4);

    GUEST_CALL(sub_81176C0);

    RETURN_TO_CALLER();
}

/* ── sub_81176C0 ── */
/* sub_81176C0: tail-call wrapper for sub_81176D0. */
Next sub_81176C0(void) {
    GUEST_CALL(sub_81176D0);
    RETURN_TO_CALLER();
}

/* ── sub_8117A84 ── */
/* sub_8117A84(r0 = record index, r6 = destination buffer):
 * Copies the 0x40-byte record with index r0 from the table at unk_2031344
 * into the buffer pointed to by r6 (via CopyByEightWords), then advances r6
 * past the record. If the index is out of range (> 0x1A), only the advance
 * happens. r3 is preserved. */
Next sub_8117A84(void) {
    if ((s32)R0 <= 0x1A) {
        R0 = (u32)R0 * 0x40 + A_unk_2031344; /* src = table + index * 0x40 */
        R1 = R6;                             /* dest = caller's buffer */
        R2 = 0x40;                           /* size = one record */
        GUEST_CALL(CopyByEightWords);
    }
    R6 += 0x40;
    RETURN_TO_CALLER();
}

/* ── entity_setLookupEntryStateFromFlag_8117D10 ── */
/* entity_setLookupEntryStateFromFlag_8117D10(r0 = flag, r1 = entity):
 * Look up the entity in the table at 0x2008450; if an entry exists for it,
 * store 4 at entry+9 when flag is nonzero, 0 otherwise. If there is no
 * entry, do nothing. */
Next entity_setLookupEntryStateFromFlag_8117D10(void) {
    u32 flag = R0;

    R0 = R1;
    GUEST_CALL(getStructFrom2008450);

    if (R0 != 0) {
        u32 entry = R1;
        ST8(entry + 9, flag != 0 ? 4 : 0);
    }
    RETURN_TO_CALLER();
}

/* ── objectStateDispatcher_8117D2C ── */
/* objectStateDispatcher_8117D2C: state-machine dispatcher for the object in r5.
 * The byte at [r5,#8] is the object's current state, stored as a *byte
 * offset* into the jump table at off_8117D40 (0, 4 or 8) - the original's
 * `ldr r0, [r0, r1]` uses an unscaled register offset, so the value is not
 * a word index. The selected handler is then called with
 * r0 = its own table entry (thumb address, so the handler can find the
 * data/entries that follow it in the table) and r1 = the state byte.
 * All other registers and the condition flags pass straight through to the
 * handler, and whatever the handler leaves becomes this function's result. */



Next objectStateDispatcher_8117D2C(void) {
    u32 state = LD8(R5 + 8); /* byte offset into the jump table: 0, 4 or 8 */
    u32 handler = LD32(A_off_8117D40 + state);

    R0 = handler; /* callee receives its own table entry, not the caller's r0 */
    R1 = state;

    switch (state) {
    case 0:
        GUEST_CALL(sub_8117D4C);
        break;
    case 4:
        GUEST_CALL(sub_8117DC8);
        break;
    case 8:
        GUEST_CALL(sub_8117EC0);
        break;
    default:
        /* Unreachable in practice: the state byte is always 0, 4 or 8. The
           original would jump through whatever word sits at table+state. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sprite_dispatchUpdate_8118FC0 ── */
/* sprite_dispatchUpdate_8118FC0: run the per-object update handler selected by byte 6 of the
 * object whose pointer is in R5, then advance the object's sprite animation.
 *
 * The handler table off_8118FD8 is indexed by that byte:
 *   0 -> nullsub_8118FE0  (a plain "bx lr" stub: no per-object update)
 *   1 -> sub_8118FE4
 * All registers are passed through to the handler untouched, and whatever
 * R0/R1 it leaves behind are forwarded to sprite_update (the original
 * dispatches with "mov lr, pc / bx r0" and then falls into "bl sprite_update"). */
Next sprite_dispatchUpdate_8118FC0(void) {
    u32 handlerIndex = LD8(R5 + 6);

    /* Index 0 selects the nullsub stub; since that stub only does "bx lr",
     * branching to it has no observable effect, so only the real handler
     * needs to run here. */
    if (handlerIndex != 0)
        GUEST_CALL(sub_8118FE4);

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── npc_StateModeDispatcher_8119364 ── */
/* npc_StateModeDispatcher_8119364: dispatcher for the state machine object in r5. Reads the mode
 * byte at [r5+8] and calls one of three handlers via the table at
 * off_8119378 (sub_8119384 / sub_81196A0 / sub_8119804).
 *
 * Note the table is indexed with the raw mode byte (`ldr r0, [r0, r1]`), so
 * the byte itself holds the byte offset: 0, 4 or 8, one entry per 4-byte
 * word. The handler is entered with r0 = its own table entry (thumb address)
 * and r1 = the mode byte; r2..r12 and the flags pass through untouched.
 *
 * The original uses the `mov lr, pc; bx r0` trick with LR pointing at the
 * trailing `pop {pc}`, so when the handler returns, that pop returns straight
 * to this function's caller. Net effect: a plain call, with the handler's
 * registers and flags becoming this function's outputs. */
Next npc_StateModeDispatcher_8119364(void) {
    u32 mode = LD8(R5 + 8);

    R1 = mode;
    R0 = LD32(A_off_8119378 + mode); /* handler address, as the callee sees it */

    switch (mode) {
    case 0:
        GUEST_CALL(sub_8119384);
        break;
    case 4:
        GUEST_CALL(sub_81196A0);
        break;
    case 8:
        GUEST_CALL(sub_8119804);
        break;
    default:
        /* Not reachable in practice: the original would jump through a
         * garbage pointer here. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── npc_dispatchStateHandler_811979C ── */
/* npc_dispatchStateHandler_811979C(r5 = object):
 * Dispatch helper: looks up a handler in the jump table at off_81197B0,
 * indexed by the object's state byte at offset +9, and calls it with
 * r0 = handler address (thumb) and r1 = state index. The table currently
 * has a single entry, sub_81197B4+1. When the handler returns, its
 * results (registers and flags) go straight back to our caller, so this
 * behaves as a tail call. */
Next npc_dispatchStateHandler_811979C(void) {
    u32 state = LD8(R5 + 9);

    R0 = LD32(A_off_81197B0 + state);   /* handler address from the jump table */
    R1 = state;                         /* index passed along to the handler */

    GUEST_CALL(sub_81197B4);
    RETURN_TO_CALLER();
}

/* ── sub_8119ACC ── */
/* sub_8119ACC: indirect call through the three-entry jump table at
 * off_8119AE0. The byte at [r5+8] is used as a *byte offset* into the
 * table (values 0, 4, 8 select the three handlers), so it is added to
 * the table base before the word load, not used as an entry index.
 * The handler is entered exactly as `bx r0` in the original leaves the
 * registers: R0 = the table word (the handler's own Thumb address),
 * R1 = the selector byte, R2-R12 and the flags untouched. When the
 * handler returns, the original executes `pop {pc}` and returns to this
 * routine's caller, so the caller sees whatever the handler left in
 * R0-R3 and the flags. */
Next sub_8119ACC(void) {
    u32 selector = LD8(R5 + 8);

    R0 = LD32(A_off_8119AE0 + selector); /* handler address, as `bx r0` sees it */
    R1 = selector;

    if (selector == 0)
        GUEST_CALL(sub_8119AEC);
    else if (selector == 4)
        GUEST_CALL(sprite_applyComputedPaletteAndUpdate_8119BD0);
    else if (selector == 8)
        GUEST_CALL(sub_8119BE0);
    /* any other selector would branch to garbage past the table; the
     * game never produces one, so there is nothing to do here */

    RETURN_TO_CALLER();
}

/* ── object_setSpritePosition_8119BE8 ── */
/* object_setSpritePosition_8119BE8(r0 = x, r1 = y, r2 = object index):
 * Look up the object's struct in the table rooted at 0x2008450; if the
 * lookup succeeds, move its sprite to (x, y). The lookup also yields a
 * value in r1 which is handed to sprite_setCoordinates via r5.
 * r4-r7 (and r3) are preserved for the caller. */
Next object_setSpritePosition_8119BE8(void) {
    u32 x = R0;
    u32 y = R1;

    /* r0 = struct lookup for object r2; the callee's null check gives
     * the Z flag the original branches on. */
    R0 = R2;
    GUEST_CALL(getStructFrom2008450);

    if (R0 != 0) {
        u32 sprite = R1;   /* sprite/bank id returned by the lookup */
        u32 savedR5 = R5;  /* sprite_setCoordinates clobbers r5 */

        R5 = sprite;
        R0 = x;
        R1 = y;
        GUEST_CALL(sprite_setCoordinates);

        R5 = savedR5;      /* original restores the caller's r5 via pop */
    }

    RETURN_TO_CALLER();
}

/* ── pet_GetNaviStatsBaseOrEmpty_8119C58 ── */
/* pet_GetNaviStatsBaseOrEmpty_8119C58(r0 = base pointer):
 * Ask which navi is currently in the PET, then read its stats byte at
 * offset 0xE. If that byte is 0xFF (no navi selected), return base+4,
 * otherwise return base unchanged. r1 always comes back as 0xE (the
 * stats offset, preserved through the helper call). r7 is untouched. */
Next pet_GetNaviStatsBaseOrEmpty_8119C58(void) {
    u32 base = R0;

    GUEST_CALL(GetCurPETNavi);            /* -> navi id in R0 */

    R1 = 0xE;
    GUEST_CALL(GetCurPETNaviStatsByte);   /* (navi id, 0xE) -> stats byte in R0 */

    R0 = ((u8)R0 == 0xFF) ? base + 4 : base;
    R1 = 0xE;
    RETURN_TO_CALLER();
}

/* ── registry_setEntryStatusByKey_8119C74 ── */
/* registry_setEntryStatusByKey_8119C74(r0 = flag, r1 = key):
 * Look up the registry entry belonging to the key in r1 via
 * getStructFrom2008450. That call returns non-zero in r0 when an entry
 * was found, and leaves the address of the entry in r1 (it clobbers the
 * incoming r1). When found, mark the entry's status byte: 0x23 when the
 * flag argument is set, otherwise 1. Do nothing when no entry matches. */
Next registry_setEntryStatusByKey_8119C74(void) {
    u32 flag = R0;   /* callee clobbers r0, so keep the flag aside */
    u32 key = R1;

    R0 = key;
    GUEST_CALL(getStructFrom2008450);

    if (R0 != 0) {
        u32 entry = R1;   /* r1 now points at the found registry entry */
        ST8(entry, (flag != 0) ? 0x23 : 1);
    }

    RETURN_TO_CALLER();
}

/* ── entity_RunStateHandler_8119E40 ── */
/* entity_RunStateHandler_8119E40: jump-table dispatcher for the state handler at r5.
 *
 * The byte at [r5+8] selects one of three handlers. It is used as a *byte
 * offset* into the table at off_8119E54 (whose entries sit at +0, +4 and
 * +8), so the game stores 0, 4 or 8 in it -- not a 0/1/2 index.
 *
 * The call is a tail call: the original sets LR to the `pop {pc}` that
 * follows, so the handler returns straight back to this function's caller.
 * The handler receives r0 = its own table entry (Thumb address, exactly as
 * loaded from the table), r1 = the selector byte, and r2-r12 untouched.
 * Registers and flags after the call are passed through. */
Next entity_RunStateHandler_8119E40(void) {
    u32 sel = LD8(R5 + 8);
    u32 handler = LD32(A_off_8119E54 + sel); /* off_8119E54: sub_8119E60+1, object_runTypeUpdateThenSpriteUpdate_811A0A4+1, sub_811A1EC+1 */

    R0 = handler;
    R1 = sel;

    switch (sel) {
    case 0: GUEST_CALL(sub_8119E60); break;
    case 4: GUEST_CALL(object_runTypeUpdateThenSpriteUpdate_811A0A4); break;
    case 8: GUEST_CALL(sub_811A1EC); break;
    /* any other byte offset would make the original jump to garbage; it
     * never occurs in practice, so there is nothing to do */
    }

    RETURN_TO_CALLER();
}

/* ── battle_initScalableSpriteObject_811A11C ── */
/* battle_initScalableSpriteObject_811A11C(r5 = battle object):
 * Give the object's sprite (number 0x90) scaling support, program its
 * initial scale parameters (0x90 source height, 0x40 dest height) and set
 * the object's state byte at +9 to 8 (scalable-sprite mode). */
Next battle_initScalableSpriteObject_811A11C(void) {
    u32 obj = R5;

    R0 = 0x90;
    ST8(obj + 0x12, 0x90);              /* sprite id 0x90 */
    GUEST_CALL(sprite_makeScalable);
    obj = R5;                            /* callee may clobber r5 */

    R0 = 0;
    R1 = 0x90;
    R2 = 0x40;
    GUEST_CALL(sprite_setScaleParameters); /* leaf call: preserves r5 */

    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(obj + 9, 8);                     /* object state: scalable */
    RETURN_TO_CALLER();
}

/* ── stateObj_advanceToState0x10_811A148 ── */
/* stateObj_advanceToState0x10_811A148(r5 = state object pointer):
 * If the byte at r5+4 is still zero, run its initialization (sub_8119EEC).
 * Then mark the object with state 0x10 at r5+9 and advance it via sprite_scaleShrinkStep_811A160.
 * r5 is preserved; r0 on return is whatever sprite_scaleShrinkStep_811A160 left there. */
Next stateObj_advanceToState0x10_811A148(void) {
    if (LD8(R5 + 4) == 0)
        GUEST_CALL(sub_8119EEC);

    ST8(R5 + 9, 0x10);

    GUEST_CALL(sprite_scaleShrinkStep_811A160);
    RETURN_TO_CALLER();
}

/* ── object_runUpdateDispatch_811A190 ── */
/* object_runUpdateDispatch_811A190: per-object update entry point.
 * r5 = object. The byte at offset 9 in the object selects its update
 * handler from the handler table at off_811A1AC and that handler runs
 * first (the original leaves the table entry itself in r0 and the type
 * byte in r1 when it jumps to it). Afterwards two shared update passes,
 * sub_811A29C and sub_811A2EC, run; r0/r1 flow out of the last one.
 * The table currently contains a single entry, sub_811A1B0. */
Next object_runUpdateDispatch_811A190(void) {
    u32 type = LD8(R5 + 9);

    /* Call the object's own handler. */
    R0 = LD32(A_off_811A1AC + type);
    R1 = type;
    GUEST_CALL(sub_811A1B0);

    /* Shared update passes. */
    GUEST_CALL(sub_811A29C);
    GUEST_CALL(sub_811A2EC);

    RETURN_TO_CALLER();
}

/* ── battle_runObjectStateHandler_811B1EC ── */
/* battle_runObjectStateHandler_811B1EC: run the current state handler of the battle object in r5.
 * The state id is stored at r5+8 and is used directly as a byte offset into
 * a three-entry jump table (states 0, 4 and 8). The handler is entered with
 * r0 = its own address and r1 = the state id; every other register passes
 * through untouched, and the callee's registers and flags are handed back
 * to our caller (the original tails into the callee via bx/pop {pc}). */
Next battle_runObjectStateHandler_811B1EC(void) {
    u32 state = LD8(R5 + 8);

    R1 = state;
    R0 = LD32(A_off_811B200 + state); /* handler address; table is byte-indexed */

    switch (state) {
    case 0:
        GUEST_CALL(sub_811B20C);
        break;
    case 4:
        GUEST_CALL(sprite_objectUpdateAnimation_811B284);
        break;
    case 8:
        GUEST_CALL(sub_811B290);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── sprite_objectUpdateAnimation_811B284 ── */
/* sprite_objectUpdateAnimation_811B284: update step for an animated object — first runs the
 * object's own per-frame handler (sub_811B298), then advances its
 * sprite animation via sprite_update. */
Next sprite_objectUpdateAnimation_811B284(void) {
    GUEST_CALL(sub_811B298);
    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_811B48C ── */
/* sub_811B48C(r0 = flag, r1 = table id):
 * Look up the entry for id r1 in the table at 0x2008450. If the lookup
 * succeeds, write a byte at the address the helper leaves in r1: 0x23 when
 * the incoming flag r0 was nonzero, otherwise 1. The helper's r0 result and
 * the saved flag are left in r0/r2 for the caller. */
Next sub_811B48C(void) {
    u32 flag = R0;

    R0 = R1;
    GUEST_CALL(getStructFrom2008450);

    if (R0 != 0) {
        /* the helper leaves the destination byte's address in r1 */
        R2 = flag;
        ST8(R1, flag != 0 ? 0x23 : 1);
    }
    RETURN_TO_CALLER();
}

/* ── sprite_dispatchObjectUpdate_811BDCC ── */
/* sprite_dispatchObjectUpdate_811BDCC — run one object's update step.
 *
 * R5 points to the object being updated. The kind byte at R5+9 selects an
 * update handler from the table at off_811BDE8, which is entered with the
 * handler's own address in r0 and the kind byte in r1. (The built-in table's
 * only entry is sub_811BDEC, i.e. kind 0; any other kind would read garbage
 * from the code that follows the pool, so kind is always 0 here.)
 *
 * After the handler returns, object_checkStatusFlag_811BE90 reports a status in r0 and leaves
 * the zero flag reflecting it: when it reports zero the object's sprite does
 * not need to be advanced and sprite_update is skipped. The flags left by
 * the last called function are passed on to the caller unchanged. */
Next sprite_dispatchObjectUpdate_811BDCC(void) {
    u32 kind = LD8(R5 + 9);

    R0 = LD32(A_off_811BDE8 + kind);   /* handler address from the dispatch table */
    R1 = kind;
    GUEST_CALL(sub_811BDEC);

    GUEST_CALL(object_checkStatusFlag_811BE90);
    if (R0 == 0)
        RETURN_TO_CALLER();

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sprite_setPositionByStructIndex_811BDFC ── */
/* sprite_setPositionByStructIndex_811BDFC(r0 = x, r1 = y, r2 = index into the struct table at 0x2008450):
 * Look up the entry for this index; if one exists, set the sprite's
 * coordinates to (x, y), passing the entry to sprite_setCoordinates in r5.
 * r5 is only borrowed for the call — like r4/r6/r7, it is preserved for
 * the caller (the original keeps it across push/pop). */
Next sprite_setPositionByStructIndex_811BDFC(void) {
    u32 arg0 = R0;
    u32 arg1 = R1;
    u32 savedR5 = R5;

    R0 = R2;
    GUEST_CALL(getStructFrom2008450);

    /* R0 == 0 means there is no live entry for this index. Otherwise the
     * entry comes back in R1 and is handed to sprite_setCoordinates in R5. */
    if (R0 != 0) {
        R5 = R1;
        R0 = arg0;
        R1 = arg1;
        GUEST_CALL(sprite_setCoordinates);
    }

    /* Equivalent of the original's pop {r4-r7}: the caller's r5 survives
     * both the temporary use above and any clobbering by the callee. */
    R5 = savedR5;

    RETURN_TO_CALLER();
}

/* ── sprite_setPositionById_811C23C ── */
/* sprite_setPositionById_811C23C(r0 = coord a, r1 = coord b, r2 = lookup key):
 * Look up the record for r2 in the table behind getStructFrom2008450.
 * If a record was found (r0 != 0), reposition the sprite identified by
 * (r0, r1) using the handle returned in r1 of the lookup. Otherwise
 * nothing happens. r4-r7 are preserved. */
Next sprite_setPositionById_811C23C(void) {
    u32 coordA = R0;
    u32 coordB = R1;
    u32 savedR5 = R5;   /* caller's r5: only used as a call argument below */

    R0 = R2;
    GUEST_CALL(getStructFrom2008450);

    if (R0 != 0) {
        R5 = R1;        /* lookup's second result: handle for sprite_setCoordinates */
        R0 = coordA;
        R1 = coordB;
        GUEST_CALL(sprite_setCoordinates);
        R5 = savedR5;   /* restore the caller's r5 after the helper call */
    }
    RETURN_TO_CALLER();
}

/* ── battle_initObjectSpriteAndStartAnimation_811CC0C ── */
/* battle_initObjectSpriteAndStartAnimation_811CC0C: (re)initialize a battle sprite.
 * Asks sub_811CC68 for the object's sprite info, starts the animation
 * named by its halfword field at +2, loads the animation's frame data,
 * then hands the object state 4 to counter_addU32_r5Plus0C_811CC60.
 * Always returns r0 = 1 with NZ set from that final `mov r0, #1`.
 * r4 and r5 are never touched, so they pass through untouched. */
Next battle_initObjectSpriteAndStartAnimation_811CC0C(void) {
    /* Which animation to play: halfword at offset 2 of the sprite info. */
    GUEST_CALL(sub_811CC68);
    R0 = LD16(R0 + 2);
    GUEST_CALL(sprite_setAnimation);   /* (u8 animation) */
    GUEST_CALL(sprite_loadAnimationData);

    R0 = 4;
    GUEST_CALL(counter_addU32_r5Plus0C_811CC60);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_811EC50 ── */
/* sub_811EC50: run the full batch of per-frame updates for this subsystem.
 * Calls the five update steps in order; register and flag state on return
 * is whatever drawCurrentMapName_811F08C (the last callee) leaves behind. */
Next sub_811EC50(void) {
    GUEST_CALL(sub_811EC68);
    GUEST_CALL(sub_811EFB8);
    GUEST_CALL(updateMapNameDisplay_811F014);
    GUEST_CALL(drawMapNamesScreen_811F050);
    GUEST_CALL(drawCurrentMapName_811F08C);
    RETURN_TO_CALLER();
}

/* ── naviSelect_SetupBackgrounds_811EF0C ── */
/* naviSelect_SetupBackgrounds_811EF0C: rebuild the PET/cyberworld background for the navi select screen.
 * Runs the individual background setup passes, then copies the tile graphics
 * for the PET display and the navi select screen. No arguments, no results;
 * r0-r3/flags are whatever the last callee leaves behind. */
Next naviSelect_SetupBackgrounds_811EF0C(void) {
    GUEST_CALL(sub_811F1B8);
    GUEST_CALL(sub_811F214);
    GUEST_CALL(sub_811F0C8);
    GUEST_CALL(sub_811F11C);
    GUEST_CALL(CopyBackgroundTiles_PET_811EF70);
    GUEST_CALL(CopyBackgroundTiles_onNaviSelect_811F54C);
    RETURN_TO_CALLER();
}

/* ── pet_prepareMenuOnReturn_811F6C0 ── */
/* pet_prepareMenuOnReturn_811F6C0(r10 = toolkit):
 * Prepare the PET menu after coming back into it: wipe the submenu's
 * 0x80-byte work area, restore the menu cursor to the page remembered in
 * ePETMenuData, and record the current map group as the last one visited
 * (so the game later knows where the player came from).
 * r2/r3/r4/r5 and the V flag pass through unchanged. */
Next pet_prepareMenuOnReturn_811F6C0(void) {
    u32 toolkit = R10;
    u32 petMenuData = A_ePETMenuData;

    /* Clear the submenu work area; ZeroFillByWord wants a byte count. */
    u32 submenu = LD32(toolkit + oToolkit_SubmenuPtr);
    R0 = submenu;
    R1 = 0x80;
    GUEST_CALL(ZeroFillByWord);

    /* The saved menu selection is a page number; the submenu's first byte
     * is the byte offset of that page (selection * 4). The LSL #2 that
     * produces it also sets carry from bit 30 - always clear here, since
     * the selection is only a byte - and that carry is visible to callers. */
    u32 selection = LD8(petMenuData + 4);
    u32 cursorOffset = selection << 2;
    cpu.c = (selection >> 30) & 1;

    ST8(submenu, cursorOffset);

    /* Copy the current map group into the last-map-group field.
     * (The map group field sits at offset 4 in the game state; only the
     * LastMapGroup offset has a named macro in the disassembly.) */
    u32 gameState = LD32(toolkit + oToolkit_GameStatePtr);
    u32 mapGroup = LD16(gameState + 4);
    ST16(gameState + oGameState_LastMapGroup, mapGroup);

    RETURN_TO_CALLER();
}

/* ── pet_menuExitToMap_811F708 ── */
/* pet_menuExitToMap_811F708: begin the "exit to map" fade and reset the PET menu.
 * Runs the shared fade-entry helper first, sets the game state's map-enter
 * fade parameter to 8, then initializes the PET menu data block:
 *   +0 = 0x14 (menu screen id), +1 = 0 and +8 = 0 (cleared state),
 *   +9 = 3 (menu sub-state).
 * Returns with r0 = 3 and r1 = &ePETMenuData; flags reflect `mov r0, #3`. */
Next pet_menuExitToMap_811F708(void) {
    GUEST_CALL(next_exitToMap_811F728);

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    ST8(gameState + oGameState_EnterMapFadeParam1, 8);

    u32 petMenu = A_ePETMenuData;
    ST8(petMenu,      0x14); /* current PET menu screen */
    ST8(petMenu + 1,  0);    /* cleared: cursor/selection state */
    ST8(petMenu + 8,  0);    /* cleared: sub-menu state */
    ST8(petMenu + 9,  3);    /* menu sub-state = 3 */

    R0 = 3;
    R1 = petMenu;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_ClearBuffer_811FE34 ── */
/* battle_ClearBuffer_811FE34(r0 = buffer address, r2/r3 = preserved by callee contract):
 * zero-fill 0x3C0 bytes at r0 via ZeroFillByEightWords.
 * Leaves r0, r2, r3 untouched; r1 still holds the fill size afterwards. */
Next battle_ClearBuffer_811FE34(void) {
    R1 = 0x3C0;                 /* size consumed by ZeroFillByEightWords */
    GUEST_CALL(ZeroFillByEightWords);
    RETURN_TO_CALLER();
}

/* ── zeroFill_0xA000Words_811FF6C ── */
/* zeroFill_0xA000Words_811FF6C(r0 = start address):
 * Zero-fill 0xA000 words (160 KB) of memory starting at the address in r0.
 * r0, r2 and r3 pass through unchanged; r1/r2/r3 end up as
 * ZeroFillByEightWords leaves them. */
Next zeroFill_0xA000Words_811FF6C(void) {
    R1 = 0xA000;                 /* word count from the literal pool at byte_811FF78 */
    GUEST_CALL(ZeroFillByEightWords);
    RETURN_TO_CALLER();
}

/* ── chargeShotChargeObject_80E0DF0 ── */
/*
 * chargeShotChargeObject_80E0DF0 -- state dispatcher for the charge-shot
 * charge-up battle object.
 *
 * r5 holds the object. Its current-state byte is used directly as a byte
 * offset into the three-entry handler table at off_80E0E04, i.e. the object
 * stores its state pre-multiplied by four:
 *   state 0 -> chargeShotChargeObject_init   (first tick after spawn)
 *   state 4 -> chargeShotChargeObject_update (per-frame behaviour)
 *   state 8 -> object_freeMemory             (object is being removed)
 *
 * The handler is invoked with the state byte in r0 and its own table entry
 * (its own address) in r1; every other register passes through untouched.
 * The original performs the call with "mov lr, pc; bx r1" and follows it
 * with a pop {pc}, so after the handler returns this function simply
 * returns to its own caller.
 */
Next chargeShotChargeObject_80E0DF0(void) {
    /* ldrb r0, [r5, #oBattleObject_CurState] (offset 8 in the object) */
    u32 state = LD8(R5 + 8);

    R0 = state;
    R1 = LD32(A_off_80E0E04 + state); /* handler address from the method table */

    switch (state) {
    case 0: /* table slot 0: spawn/initialisation */
        GUEST_CALL(chargeShotChargeObject_init_80E0E10);
        break;
    case 4: /* table slot 1: per-frame update */
        GUEST_CALL(chargeShotChargeObject_update_80E0E20);
        break;
    default: /* table slot 2 (state 8): release the object's memory */
        GUEST_CALL(object_freeMemory);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── megamanChargeShotAiAttack_80EBE00 ── */
/* megamanChargeShotAttackAiAttack_80EBE00: dispatch one charge-shot AI attack step.
 *
 * Reads the phase byte at offset 0 of the AI attack state (r7) and tail-calls
 * the corresponding charge shot attack routine through the table off_80EBE14:
 *   +0 -> sub_80EBE20, +4 -> sub_80EBE54, +8 -> sub_80EBEB2
 *
 * Note the original indexes the table with the raw byte (`ldr r1, [r1, r0]`),
 * so the stored phase values are byte offsets (0, 4, 8), not entry indices.
 * The tail call ("mov lr, pc; bx r1" + "pop {pc}") means the chosen routine
 * returns straight to our caller, so its register results and flags pass
 * through unchanged.
 *
 * Incoming-register artifacts kept for exactness: R0 = the phase byte (the
 * ldrb overwrites the incoming r0), and R1 = the callee's thumb entry word
 * (e.g. sub_80EBE20+1), an artifact of loading the branch target into r1.
 */
#define CHARGE_ATTACK_TABLE 0x080EBE14

Next megamanChargeShotAiAttack_80EBE00(void) {
    u32 phase = LD8(R7 + oAIAttackVars_Unk_00);

    switch (phase) {
    case 0:
        R0 = phase;
        R1 = LD32(CHARGE_ATTACK_TABLE + 0); /* sub_80EBE20 (thumb) */
        GUEST_CALL(sub_80EBE20);
        break;
    case 4:
        R0 = phase;
        R1 = LD32(CHARGE_ATTACK_TABLE + 4); /* sub_80EBE54 (thumb) */
        GUEST_CALL(sub_80EBE54);
        break;
    case 8:
        R0 = phase;
        R1 = LD32(CHARGE_ATTACK_TABLE + 8); /* sub_80EBEB2 (thumb) */
        GUEST_CALL(sub_80EBEB2);
        break;
    default:
        /* Any other byte would make the original branch to a word read out
         * of the table (i.e. into code/garbage); the game never stores one.
         * Mirror the register state the original would leave behind. */
        R0 = phase;
        R1 = LD32(CHARGE_ATTACK_TABLE + phase);
        break;
    }

    /* Tail call: the callee's registers and flags go straight to our caller. */
    RETURN_TO_CALLER();
}

#undef CHARGE_ATTACK_TABLE

/* ── spawnChargeShotChargeObject_80E0F02 ── */
/* spawnChargeShotChargeObject_80E0F02(r5 = owner object, r7 = result slot):
 * Spawns the charge-up effect object that appears while a charge shot is
 * building (object_spawnType4, 8-byte object). The spawner hands the new
 * object back in r5. If it spawned, the effect is linked to its owner
 * (RelatedObject1Ptr), stored into *r7, and remembers r7 in ExtraVars so it
 * can clear that slot when it dies; its header flags also get bit 4 set.
 * The caller's r5 is preserved (the original push/pop {r5} around the call
 * exists because the spawner clobbers it). NZ reflect the new object pointer,
 * from the "mov r0, r5" null check that gates the linking; v and c are left
 * untouched, as in the original (movs r0, r5 = lsls #0 does not set them). */


Next spawnChargeShotChargeObject_80E0F02(void) {
    u32 owner = R5;       /* object the charge effect belongs to */
    u32 resultSlot = R7;  /* where the caller keeps track of the effect */

    R0 = 8;
    GUEST_CALL(object_spawnType4);

    u32 newObject = R5;      /* spawner returns the new object in r5 */
    SET_FLAGS_NZ(newObject); /* mov r0, r5: null check for the branch below */
    R5 = owner;              /* restore caller's r5 (original pop {r5}) */

    if (newObject != 0) {
        ST32(newObject + oBattleObject_RelatedObject1Ptr, owner);
        ST32(resultSlot, newObject);
        ST32(newObject + oBattleObject_ExtraVars, resultSlot);
        ST8(newObject + oObjectHeader_Flags,
            LD8(newObject + oObjectHeader_Flags) | 4);
    }
    RETURN_TO_CALLER();
}

/* ── battle_refreshObjectCollisionFromExtraVars_80C6AB8 ── */
/* battle_refreshObjectCollisionFromExtraVars_80C6AB8(r5 = battle object):
 * Refresh the object's collision box from its extra-vars data, reapply its
 * panel element effect, and mark a collision status effect if requested.
 * r2 comes back from the element update; r4/r6/r7 pass through untouched. */
Next battle_refreshObjectCollisionFromExtraVars_80C6AB8(void) {
    u32 object = R5;

    /* Update the hitbox: extra-vars byte 1 selects the box, 4x5 its size.
     * (R0 is left as-is; the callee does not read it.) */
    u32 extraVars = LD32(object + oBattleObject_ExtraVars);
    u32 shapeId = LD8(extraVars + 1);
    R1 = 4;
    R2 = 5;
    R3 = shapeId;
    GUEST_CALL(object_updateCollisionData);

    /* Reapply the effect of the panel element this object stands on. */
    R0 = LD8(object + oBattleObject_Element);
    GUEST_CALL(sub_8019F8C);

    /* Extra-vars byte 3 non-zero: flag the collision status effect. */
    extraVars = LD32(object + oBattleObject_ExtraVars);
    u32 statusEffect = LD8(extraVars + 3);
    if (statusEffect != 0) {
        R0 = statusEffect;
        GUEST_CALL(object_setCollisionStatusEffect1);
    }
    RETURN_TO_CALLER();
}

/* ── battle_spawnBasicObjectKind0x16_80C6CE4 ── */
/* battle_spawnBasicObjectKind0x16_80C6CE4(r0..r2 = init args, r5 = context):
 * Spawns a type-3 battle object of kind 0x16. The spawn routine returns the
 * new object pointer in r5. If one was created, it is initialized as a basic
 * battle object with (newObject, arg0, arg1, 0).
 *
 * The original saves r0-r2/r5 on the stack and pops them back shifted by one
 * slot (r0->r1, r1->r2, r2->r3), while r5 is restored to its incoming value. */
Next battle_spawnBasicObjectKind0x16_80C6CE4(void) {
    u32 arg0 = R0;
    u32 arg1 = R1;
    u32 arg2 = R2;
    u32 savedR5 = R5;

    /* Spawn the object; the new object pointer comes back in r5. */
    R0 = 0x16;
    GUEST_CALL(object_spawnType3);
    u32 newObject = R5;

    /* mov r0, r5: keep the new object pointer in r0. */
    R0 = newObject;

    /* pop {r1-r3,r5}: saved args shift into r1-r3, r5 is restored. */
    R1 = arg0;
    R2 = arg1;
    R3 = arg2;
    R5 = savedR5;

    /* Only initialize if an object was actually spawned. */
    if (newObject != 0) {
        R3 = 0;
        GUEST_CALL(BasicBattleObjectInitialize);
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectDispatchPanelChange_80C6CFC ── */
/* battle_objectDispatchPanelChange_80C6CFC(r0, r1 = panel coords, r2 = panel change action):
 * dispatch a panel-state change requested by a battle object. The action code in
 * r2 selects what happens to the panel:
 *   0xFF -> nothing (leave the panel untouched),
 *   3    -> crack the panel,
 *   1    -> break the panel,
 *   else -> set the panel type directly from r0/r1.
 * All arguments (r0-r2 and the callee-saved r4-r7, r10) are passed through to the
 * chosen handler unchanged; its register results are the caller's results. */
Next battle_objectDispatchPanelChange_80C6CFC(void) {
    switch ((u8)R2) {
    case 0xFF:
        /* No-op action: return without touching the panel. */
        break;
    case 3:
        GUEST_CALL(object_crackPanel);
        break;
    case 1:
        GUEST_CALL(object_breakPanel_dup1);
        break;
    default:
        GUEST_CALL(object_setPanelType);
        break;
    }
    RETURN_TO_CALLER();
}

/* ── battle_objectDestroyWhenAnimFrameDone_80C9CC4 ── */
/* State constant used by the battle-object state machine: the "destroyed"
 * state this helper moves an object into once its death animation ends. */
#define CUR_STATE_DESTROY 8

/* battle_objectDestroyWhenAnimFrameDone_80C9CC4 (r5 = battle object):
 * Query the object's current animation frame parameters; while the frame
 * still carries the 0x80 bit the death animation is running, so do nothing.
 * Once the bit clears, drop the object's collision region and switch it to
 * the destroyed state.
 * Leaves r1 = 0x80 (from the TST operand) as the original does. Flags: on the
 * early-out path nz comes from the TST; on the destroy path nz comes from
 * MOVS r0, #CUR_STATE_DESTROY. c/v are whatever the last callee left. */
Next battle_objectDestroyWhenAnimFrameDone_80C9CC4(void) {
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;

    R1 = 0x80; /* operand of the TST below, live in r1 on both paths */

    if (frameParams & 0x80) {
        GUEST_CALL(object_clearCollisionRegion);

        R0 = CUR_STATE_DESTROY;
        ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, R0);
        SET_FLAGS_NZ(R0); /* MOVS r0, #CUR_STATE_DESTROY */
    } else {
        SET_FLAGS_NZ(frameParams & 0x80); /* TST r0, r1 */
    }

    RETURN_TO_CALLER();
}

/* ── object_destroyWrapper_80CB078 ── */
/* object_destroyWrapper_80CB078: thin wrapper that destroys the object pointed to by R0
 * via the generic object destructor; all results and flags come straight
 * from object_genericDestroy. */
Next object_destroyWrapper_80CB078(void) {
    GUEST_CALL(object_genericDestroy);
    RETURN_TO_CALLER();
}

/* ── battle_spawnTypeA9ObjectAtPanel_80DA9DC ── */
/* battle_spawnTypeA9ObjectAtPanel_80DA9DC(r0 = panel X, r1 = panel Y, r2 = element):
 * Spawns the battle object with type ID 0xA9 via object_spawnType3 (which
 * also sees the untouched r1/r2 and r3 = 0, and returns the new object
 * pointer in r5, 0 = none).  The original spills r0-r2/r5 before the call
 * and pops them shifted by one slot, so on return:
 *   r0 = object pointer, r1 = caller's r0, r2 = caller's r1, r3 = caller's r2,
 *   r5 preserved.
 * If an object was created, it is placed on the given panel, given the
 * element, and flag bits 0x14 are set in its header flags (offset 0); r1/r2
 * then hold the updated flags and the OR mask, as in the original.
 * Flags: the "mov r0, r5" is really adds r0, r5, #0, which sets nz and
 * clears c/v; the orr afterwards updates nz only, leaving c = v = 0. */
Next battle_spawnTypeA9ObjectAtPanel_80DA9DC(void) {
    u32 panelX = R0;
    u32 panelY = R1;
    u32 element = R2;
    u32 callerR5 = R5;

    R0 = 0xA9;      /* object type to spawn */
    R3 = 0;
    GUEST_CALL(object_spawnType3);

    u32 object = R5;   /* spawn result: new object pointer, or 0 if none */
    R5 = callerR5;
    R1 = panelX;
    R2 = panelY;
    R3 = element;
    R0 = object;

    /* adds r0, r5, #0 — the flag-setting "mov" */
    SET_FLAGS_ADD(object, 0);

    if (object != 0) {
        ST8(object + oBattleObject_PanelX, panelX);
        ST8(object + oBattleObject_PanelY, panelY);
        ST8(object + oBattleObject_Element, element);

        u32 flags = LD8(object) | 0x14;   /* oObjectHeader_Flags is at offset 0 */
        SET_FLAGS_NZ(flags);              /* orr sets nz only; c/v stay cleared */
        ST8(object, flags);

        R1 = flags;
        R2 = 0x14;
    }
    RETURN_TO_CALLER();
}

/* ── battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84 ── */
/* battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84(r0 = panel X, r1 = panel Y, r2 = element, r5 = object whose
 *             alliance/direction-flip halfword is copied, r6 = damage word):
 * Spawns a battle object of type 0xD3 via object_spawnType3 (the new object
 * comes back in r5, NULL if none could be created) and, on success, fills in
 * its panel position, element, damage/stamina word and the alliance/
 * direction-flip halfword taken from the object originally in r5.
 * On return: r0 = the new object pointer (r5 as returned by the spawner),
 * r1 = old r0, r2 = old r1, r3 = copied halfword (old r2 if spawn failed),
 * r5 = old r5; r4/r6/r7 untouched. */
Next battle_spawnTypeD3ObjectWithPanelElementAndDamage_80DFF84(void) {
    u32 oldR0 = R0;
    u32 oldR1 = R1;
    u32 oldR2 = R2;
    u32 oldR5 = R5;
    u32 oldR6 = R6;

    R0 = 0xD3;                          /* battle object type to spawn */
    GUEST_CALL(object_spawnType3);      /* spawned object returned in r5 */

    u32 obj = R5;
    R0 = obj;                           /* mov r0, r5 (unconditional) */

    /* pop {r1-r3,r5}: saved r0-r2 shift back one register */
    R1 = oldR0;
    R2 = oldR1;
    R3 = oldR2;
    R5 = oldR5;

    if (obj != 0) {
        ST8(obj + oBattleObject_PanelX, oldR0);
        ST8(obj + oBattleObject_PanelY, oldR1);
        ST8(obj + oBattleObject_Element, oldR2);
        ST32(obj + oBattleObject_DamageAndStaminaDamageCounterDisabler, oldR6);
        u32 alliance = LD16(oldR5 + oBattleObject_AllianceAndDirectionFlip);
        ST16(obj + oBattleObject_AllianceAndDirectionFlip, alliance);
        R3 = alliance;                  /* ldrh overwrites r3 on this path */
    }

    RETURN_TO_CALLER();
}

/* ── battle_object_teardownAndFree_80E06E0 ── */
/* battle_object_teardownAndFree_80E06E0: teardown routine for a battle object.
 * Asks sub_800AA92 (with argument 0) whether cleanup should be skipped;
 * if it returns nonzero we bail out early. Otherwise it runs
 * battle_orStateFlags5c_8001382(2) and then releases the object whose pointer that call
 * leaves in r0 via object_freeMemory.
 * Flags on the early-exit path reflect the TST of sub_800AA92's result. */
Next battle_object_teardownAndFree_80E06E0(void) {
    R0 = 0;
    GUEST_CALL(sub_800AA92);

    if (R0 != 0) {
        /* tst r0, r0 */
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    R0 = 2;
    GUEST_CALL(battle_orStateFlags5c_8001382);
    /* r0 still holds battle_orStateFlags5c_8001382's result: the object to free. */
    GUEST_CALL(object_freeMemory);

    RETURN_TO_CALLER();
}

/* ── battle_spawnType4ObjectWithActiveFlag_80E06F8 ── */
/* battle_spawnType4ObjectWithActiveFlag_80E06F8(r1-r3 = arguments forwarded to the spawner, r5 = object header):
 * Spawns a type-4 object through object_spawnType4 (r0 = 2 selects the
 * variant), then sets flag bit 0x04 in the oObjectHeader_Flags byte of the
 * object the spawner leaves in r5, and hands that object back in r0.
 * The caller's r4/r5 are preserved (push/pop in the original). */
Next battle_spawnType4ObjectWithActiveFlag_80E06F8(void) {
    u32 callerR4 = R4;                 /* push {r4,r5,lr} */
    u32 callerR5 = R5;

    R0 = 2;                            /* variant id for object_spawnType4 */
    GUEST_CALL(object_spawnType4);

    u32 object = R5;                   /* object to mark, as left by the spawner */
    ST8(object, LD8(object) | 4);      /* set flag 0x04 in its header */

    R0 = object;                       /* mov r0, r5 */
    R4 = callerR4;                     /* pop {r4,r5,pc} */
    R5 = callerR5;
    RETURN_TO_CALLER();
}

/* ── sprite_updateDestroyWhenAnimEnds_80E08A6 ── */
/* sprite_updateDestroyWhenAnimEnds_80E08A6 (r5 = object header):
 * Sprite update callback for an object whose animation has finished.
 * Asks the sprite for its current frame parameters; if they signal
 * "sequence finished" (bit 7 set), clear the object's flag bit 1 (bit 0x2
 * of the header flags, at offset 0) and move the object into the DESTROY
 * state (8), so the shared object state machine will take it down.
 * Finally ticks the sprite to its next frame.  Returns with whatever
 * r0-r3 and the condition flags sprite_update left behind. */
Next sprite_updateDestroyWhenAnimEnds_80E08A6(void) {
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;

    if (frameParams & 0x80) {
        /* oObjectHeader_Flags == 0 */
        u32 flags = LD8(R5);
        ST8(R5, flags & ~0x2u);
        ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, 8 /* CUR_STATE_DESTROY */);
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── battle_spawnLinkedObject_80E1A6A ── */
/* battle_spawnLinkedObject_80E1A6A(r5 = battle object, r7 = extra vars holder):
 * Spawns a type-0x11 battle object, then, if the object pointer in r5 is
 * non-NULL, wires the object and the holder together:
 *   object->relatedObject1 = object, *holder = object, object->extraVars = holder.
 * Leaves C/V cleared and Z from the object pointer (the flag-setting
 * "mov r0, r5" is really adds r0, r5, #0). */
Next battle_spawnLinkedObject_80E1A6A(void) {
    u32 savedObject = R5;

    R0 = 0x11;
    GUEST_CALL(object_spawnType4);

    /* r0 = r5; the spawn normally leaves r5 untouched, but the original
     * saves/restores it around the call, so keep the caller's value. */
    R0 = R5;
    SET_FLAGS_ADD(R5, 0);
    R5 = savedObject;

    if (R0 != 0) {
        ST32(R0 + oBattleObject_RelatedObject1Ptr, R5);
        if (R7 != 0)
            ST32(R7, R0);
        ST32(R0 + oBattleObject_ExtraVars, R7);
    }

    RETURN_TO_CALLER();
}

/* ── battle_objectFlyAnimUpdateAndDestroy_80E1D54 ── */
/* battle_objectFlyAnimUpdateAndDestroy_80E1D54(r5 = battle object):
 * Advance the object's sprite animation, integrate its Z velocity into its Z
 * position, and switch the object to the "destroy" state action once the
 * sprite's current frame parameters report the end marker (bit 0x80).
 * Leaves r0 = frame parameters (or CUR_STATE_DESTROY_sub_80E1D54 when destroyed), r1 = 0x80,
 * and flags as the last flag-setting instruction of the taken path leaves them. */

enum { CUR_STATE_DESTROY_sub_80E1D54 = 8u };

Next battle_objectFlyAnimUpdateAndDestroy_80E1D54(void) {
    u32 obj = R5;

    GUEST_CALL(sprite_update);

    /* Z += ZVelocity */
    u32 z      = LD32(obj + oBattleObject_Z);
    u32 zSpeed = LD32(obj + oBattleObject_ZVelocity);
    ST32(obj + oBattleObject_Z, z + zSpeed);

    GUEST_CALL(sprite_getFrameParameters);

    R1 = 0x80;
    u32 frameParams = R0;
    SET_FLAGS_NZ(frameParams & R1);
    if (frameParams & 0x80) {
        /* Animation finished: mark the object for destruction. */
        R0 = CUR_STATE_DESTROY_sub_80E1D54;
        SET_FLAGS_NZ(R0);
        ST32(obj + oBattleObject_CurPhaseAndPhaseInitialized, R0);
    }

    RETURN_TO_CALLER();
}

/* ── battle_aiAttackDispatch_80EB436 ── */
/* battle_aiAttackDispatch_80EB436: AI attack dispatcher.
 *
 * The byte at the head of the AI attack-vars block (R7) selects one of the
 * attack routines in the jump table at jt_80EB448. The original tail-calls
 * the selected handler (LR holds the address of the following `pop {pc}`,
 * so the handler's return goes straight back to our caller), which the
 * GUEST_CALL/RETURN_TO_CALLER pair reproduces: the handler's registers and
 * condition flags pass through to the caller untouched.
 *
 * On entry to the handler: R0 = selector byte, R1 = the raw table entry
 * (handler address | Thumb bit), exactly as the original leaves them. */
Next battle_aiAttackDispatch_80EB436(void) {
    u32 slot = LD8(R7);                     /* selector at offset 0 of the block */

    R0 = slot;                              /* handler receives the selector */
    R1 = LD32(A_jt_80EB448 + 4 * slot);     /* table entry (Thumb | 1) */

    if (slot == 0)
        GUEST_CALL(sub_80EB450);
    else
        GUEST_CALL(sub_80EB502);

    RETURN_TO_CALLER();
}

/* ── aiAttackTimerTick_80EB758 ── */
/* The disassembly refers to the attack-phase byte at offset 1 as
 * oAIAttackVars_Unk_01; that name is not in the shared header, so provide it. */

/* aiAttackTimerTick_80EB758: AI attack-state timer tick (r7 = attack vars object).
 * On the first invocation (phase byte still 0) the countdown is initialised
 * to 5 frames and the phase byte is marked started (4).  Every call then
 * decrements the timer; when it goes negative the object leaves its attack
 * state.  Flags left behind are those of the timer decrement (or, when the
 * exit-state routine runs, whatever it leaves). */
Next aiAttackTimerTick_80EB758(void) {
    /* Initialise the attack countdown on the first invocation. */
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        ST16(R7 + oAIAttackVars_Unk_10, 5);
        ST8(R7 + oAIAttackVars_Unk_01, 4);
    }

    /* Decrement the frame counter and store it back. */
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    u32 remaining = timer - 1;
    ST16(R7 + oAIAttackVars_Unk_10, remaining);
    SET_FLAGS_CMP(timer, 1); /* flags as after SUBS timer, #1 */
    R0 = remaining;

    /* Timer expired: leave the attack state. */
    if ((s32)remaining < 0) {
        GUEST_CALL(object_exitAttackState);
    }

    RETURN_TO_CALLER();
}

/* ── aiAttack_dispatchStateHandler_80EBC0E ── */
/* aiAttack_dispatchStateHandler_80EBC0E: dispatch an AI attack sub-state handler.
 * R7 points at the AI attack vars; the state byte in its first field
 * selects the handler from a two-entry table in ROM:
 *   0 -> sub_80EBC28, 1 -> aiAttack_windupTick_80EBD3C.
 * The state byte is passed to the handler in R0 and the handler pointer
 * in R1 (as the original's `ldr r1, [table, state]` leaves it); the
 * `mov lr, pc / bx r1` sequence makes this an effective tail call, so the
 * handler's results (registers and flags) go straight back to our caller. */
Next aiAttack_dispatchStateHandler_80EBC0E(void) {
    u32 attackState = LD8(R7); /* oAIAttackVars state byte (offset 0) */

    R0 = attackState;
    R1 = LD32(A_off_80EBC20 + attackState); /* dispatch table entry */

    if (attackState == 0)
        GUEST_CALL(sub_80EBC28);
    else
        GUEST_CALL(aiAttack_windupTick_80EBD3C); /* state >= 2 reads past the table in the original */

    RETURN_TO_CALLER();
}

/* ── battle_aiAttackStateDispatch_80EC884 ── */
/* The disassembly names the mode byte at offset 0 of the AI attack-vars block
 * oAIAttackVars_Unk_00_sub_80EC884; the shared header only carries the later fields, so
 * supply the offset-0 name here if it is missing. */
enum { oAIAttackVars_Unk_00_sub_80EC884 = 0 };

/* battle_aiAttackStateDispatch_80EC884 (r7 = AI attack-vars block):
 * Trampoline that dispatches to one of the AI attack-state handlers based on
 * the mode byte at oAIAttackVars_Unk_00_sub_80EC884, then returns straight to our caller.
 * The handler is entered with r0 = mode byte and r1 = its own table entry
 * (the thumb entry address loaded from off_80EC898), exactly as the original's
 * "ldr r1, [r1,r0]; mov lr, pc; bx r1; pop {pc}" tail-call leaves things:
 * whatever registers and condition flags the handler leaves behind become
 * this function's outputs. Only mode values 0 and 1 are valid; the jump
 * table has two entries. */
Next battle_aiAttackStateDispatch_80EC884(void) {
    u32 mode = LD8(R7 + oAIAttackVars_Unk_00_sub_80EC884);

    /* The handler also receives the table word (its own thumb entry address)
     * in r1, as the original's ldr r1, [r1, r0] leaves it there. */
    R0 = mode;
    R1 = LD32(A_off_80EC898 + mode);

    if (mode == 0)
        GUEST_CALL(sub_80EC8A0);
    else
        GUEST_CALL(ai_attackTickDelayTimer_80EC90E);

    RETURN_TO_CALLER();
}

/* ── ai_attackTickDown_timerEndExit_8109DBA ── */
/* ai_attackTickDown_timerEndExit_8109DBA (r5 = battle object, r7 = AI attack vars):
 * Tick down the attack timer at oAIAttackVars_Unk_10; the decremented
 * count is left in r0 (and the flags reflect the subtraction). While the
 * timer is still counting, nothing else happens. When it reaches zero,
 * reset the object's current animation, mark the attack as finished
 * (Unk_1a = 1) and call object_exitAttackState with r0 = 1. */
Next ai_attackTickDown_timerEndExit_8109DBA(void) {
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);

    SET_FLAGS_CMP(timer, 1);   /* SUBS timer, #1 */
    timer -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, timer);
    R0 = timer;                /* r0 holds the decremented timer on return */

    if (timer != 0)
        RETURN_TO_CALLER();

    ST8(R5 + oBattleObject_CurAnim, 0);
    ST8(R7 + oAIAttackVars_Unk_1a, 1);

    R0 = 1;
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── battle_objFirstPhaseInitAndCommonUpdate_8109F4C ── */
/* battle_objFirstPhaseInitAndCommonUpdate_8109F4C(r5 = battle object):
 * Per-frame update for a battle object. While the object is still in its
 * first phase (CurPhase == 0), initialize its ExtraVars word (offset 0x64)
 * to Alliance ^ DirectionFlip (alignment adjusted by facing flip), run
 * list_tryAddUniqueEntry_800AA1A with that value, and clear the byte at offset 0x07;
 * this setup runs only once. Afterwards, run the object's common update
 * routine battle_ResetObjectActionState_80165B8 in both cases.
 *
 * Flag contract: battle_ResetObjectActionState_80165B8 only updates the NZ flags, so C/V at return
 * are whatever the `cmp r0, #0` on CurPhase left (C=1, V=0), except on the
 * taken path where list_tryAddUniqueEntry_800AA1A's C/V win. The cmp is therefore reproduced
 * explicitly before the branch. */
Next battle_objFirstPhaseInitAndCommonUpdate_8109F4C(void) {
    u32 phase = LD8(R5 + oBattleObject_CurPhase);

    /* cmp r0, #0 (r0 = CurPhase): byte is unsigned, so never borrows */
    SET_FLAGS_CMP(phase, 0);

    if (phase == 0) {
        u32 side = LD8(R5 + oBattleObject_Alliance) ^ LD8(R5 + oBattleObject_DirectionFlip);

        ST32(R5 + oBattleObject_ExtraVars + 4, side);

        /* list_tryAddUniqueEntry_800AA1A(r0 = alliance ^ facing flip, r1 = facing flip) */
        R0 = side;
        R1 = LD8(R5 + oBattleObject_DirectionFlip);
        GUEST_CALL(list_tryAddUniqueEntry_800AA1A);

        ST8(R5 + 0x07, 0);
    }

    GUEST_CALL(battle_ResetObjectActionState_80165B8);
    RETURN_TO_CALLER();
}

/* ── t4_0x4_80E0844 ── */
/* t4_0x4_80E0844: per-tick dispatcher for a battle object (r5 = object).
 *
 * Reads the object's state byte (offset oBattleObject_CurState = 8) and calls
 * the handler selected from the jump table at off_80E0858. The state byte is
 * used directly as a byte offset into the table, so the values stored in the
 * field are 0, 4 or 8:
 *   0 -> sub_80E0864        (init/idle step)
 *   4 -> sprite_updateDestroyWhenAnimEnds_80E08A6        (main step)
 *   8 -> object_freeMemory  (teardown: release the object)
 *
 * The handler is entered with R0 = state byte and R1 = the table entry it was
 * fetched from (the original's `ldr r1,[r1,r0]; bx r1` leaves the handler's
 * Thumb address in R1). `mov lr, pc; bx r1` followed by `pop {pc}` makes this
 * a pass-through call: the handler's register results and flags flow straight
 * through to our caller, and LR/SP end up unchanged. States outside 0/4/8
 * never occur in the game; the original would index past the table. */
Next t4_0x4_80E0844(void) {
    u32 state = LD8(R5 + 8); /* oBattleObject_CurState */

    R0 = state;
    R1 = LD32(A_off_80E0858 + state);

    switch (state) {
    case 0:
        GUEST_CALL(sub_80E0864);
        break;
    case 4:
        GUEST_CALL(sprite_updateDestroyWhenAnimEnds_80E08A6);
        break;
    default:
        /* state == 8 (and, impossible in practice, anything larger): the
         * table's last entry is the object destructor. */
        GUEST_CALL(object_freeMemory);
        break;
    }

    RETURN_TO_CALLER();
}

/* ── cleareMemory_802FF2C ── */
/* cleareMemory_802FF2C: reset the state tracked in dword_2009A2C and wipe the
 * 0x180-byte buffer at word_200A6F0 (cleared in eight-word chunks).
 * Flags on return are whatever ZeroFillByEightWords leaves behind. */
Next cleareMemory_802FF2C(void) {
    ST32(A_dword_2009A2C, 0);

    R0 = A_word_200A6F0;
    R1 = 0x180;
    GUEST_CALL(ZeroFillByEightWords);

    RETURN_TO_CALLER();
}

/* ── copyObjAttributesToIWRAM_802FE0C ── */
/* copyObjAttributesToIWRAM_802FE0C:
 * Flush the shadow OAM (iOamShadow) into the real hardware OAM at 0x7000000,
 * copying 0x400 bytes one eight-word block at a time. r3 is an incoming
 * argument that must pass through untouched. */
Next copyObjAttributesToIWRAM_802FE0C(void) {
    R0 = A_iOamShadow;  /* src */
    R1 = 0x7000000;     /* dest: OAM */
    R2 = 0x400;         /* size in bytes */
    GUEST_CALL(CopyByEightWords);
    RETURN_TO_CALLER();
}

/* ── sub_8026A50 ── */
/* sub_8026A50(self: r5 = *S20364C0) -> u8 in r0 (always 0)
 *
 * Stores the value returned by sub_802FE48 into the object's byte at
 * offset 0x0E, starts sub_802FE7A with (0, 0x40, 0x40, value), sets the
 * object's state word to 4, and returns 0. r1-r3 end as sub_802FE7A
 * left them (it preserves its arguments); flags come from "mov r0, #0".
 */
Next sub_8026A50(void) {
    u32 result;

    GUEST_CALL(sub_802FE48);            /* -> r0 */

    result = R0;
    ST8(R5 + oS20364C0_Unk_0e, result); /* remember the result on the object */

    R3 = result;                        /* arguments for sub_802FE7A */
    R1 = 0x40;
    R2 = 0x40;
    R0 = 0;
    GUEST_CALL(sub_802FE7A);            /* r1/r2/r3 pass through unchanged */

    ST32(R5, 4);                        /* object state = 4 */

    R0 = 0;                             /* return value + NZ/C/V from mov r0,#0 */
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_runFrameUpdate_8026A6C ── */
/* object_runFrameUpdate_8026A6C(r5 = self object, struct S20364C0) -> r0 = self->04 (u8):
 * Run this object's standard per-frame update sequence: refresh its
 * animation/timer state, then pass its +0x0E attribute through the shared
 * flags_clearFlagWordBit_802FE6A/sub_80062EC helpers, run the object-wide update passes
 * (sound_playCrossTransformationJingle_802A3CC, battle_DispatchPendingEvent_802A0F8), and finally report the object's state byte
 * stored at +0x04. Flags are left as battle_DispatchPendingEvent_802A0F8 returned them; r6/r7 are
 * preserved for the caller. */
Next object_runFrameUpdate_8026A6C(void) {
    u32 self = R5;

    GUEST_CALL(sub_8027D78);

    R0 = LD8(self + 0x0E);
    GUEST_CALL(flags_clearFlagWordBit_802FE6A);
    GUEST_CALL(sub_80062EC);

    GUEST_CALL(sound_playCrossTransformationJingle_802A3CC);
    GUEST_CALL(battle_DispatchPendingEvent_802A0F8);

    R0 = LD8(self + 0x04);
    RETURN_TO_CALLER();
}

/* ── battle_clearActiveChipIfMegaGiga_8027F6A ── */
/* battle_clearActiveChipIfMegaGiga_8027F6A: r7 points at a chip-folder state block whose byte at offset 1
 * holds the currently selected active-chip index. Look up that index in the
 * active-chips table (getLocOfActiveChips_8027E1C) and read the chip's class
 * byte from the entry. If the selected chip is a Mega (0xA) or Giga (0xB)
 * chip, reset the selection index to 0xFF (no selection). */
Next battle_clearActiveChipIfMegaGiga_8027F6A(void) {
    R0 = LD8(R7 + 1);
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 chipClass = LD8(R0);

    if (chipClass == 0xA || chipClass == 0xB)
        ST8(R7 + 1, 0xFF);

    RETURN_TO_CALLER();
}

/* ── battle_getChipElementData_80281A8 ── */
/* battle_getChipElementData_80281A8(r0 = chip id):
 * Returns the element/attribute data associated with a battle chip.
 * The id is masked to its 9-bit chip number; the sentinel id 0x1FF
 * ("no chip") maps to the default data blob byte_86E601C instead of
 * doing a chip-data lookup. For a real chip, the pointer stored at
 * offset 0x20 in its ChipData record is returned.
 * Returns: r0 = data pointer. r1/r2 are preserved. */
Next battle_getChipElementData_80281A8(void) {
    u32 chipId = R0 & 0x1FF;

    if (chipId == 0x1FF) {
        /* Invalid/placeholder chip: hand back the fallback data. */
        R0 = A_byte_86E601C;
    } else {
        /* Look up the chip record and fetch its data pointer (r1/r2 are
         * callee-saved here, so keep them across the call like the
         * original's push/pop does). */
        u32 savedR1 = R1, savedR2 = R2;
        R0 = chipId;
        GUEST_CALL(getChip8021DA8);
        R1 = savedR1;
        R2 = savedR2;
        R0 = LD32(R0 + 0x20);
    }

    RETURN_TO_CALLER();
}

/* ── sprite_clearObjVramEntry_8028238 ── */
/* sprite_clearObjVramEntry_8028238(r0 = entry index):
 * Clears one 0xC0-byte entry of OBJ tile VRAM: the block at
 * 0x6009C20 + index*0xC0 is filled/reset by gfx_QueueNextChunkedGfxTransfer_80281C4 (0x1FF = fill pattern).
 * R2-R7 pass through untouched, so the callee's register contract is preserved. */
Next sprite_clearObjVramEntry_8028238(void) {
    u32 entryOffset = R0 * 0xC0;   /* 0xC0 bytes per entry */

    R1 = 0x6009C20 + entryOffset;  /* VRAM destination for this entry */
    R0 = 0x1FF;                    /* fill value for gfx_QueueNextChunkedGfxTransfer_80281C4 */

    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);
    RETURN_TO_CALLER();
}

/* ── battle_loadGfxChunkBySelector_8028320 ── */
/* battle_loadGfxChunkBySelector_8028320: load battle graphics, selected by R7.
 *
 * R7 == 1: queue an eight-word-aligned GFX transfer of a 0x100-byte block
 *          (the graphics chunk at byte_86E79CC + 0x300) into the destination
 *          buffer passed in R1.
 * R7 != 1: run sub_802868C on the data table at byte_802A700 with selector
 *          R3 = 3, then advance the destination pointer in R1 by 0xC0
 *          (the size of one loaded chunk), setting the flags from that add.
 *
 * R0-R3 and the nz/c/v flags are returned exactly as the original leaves
 * them: in the queued-transfer path they come from the queue routine, in
 * the other path from the ADD that bumps R1. R4-R7 are untouched. */
Next battle_loadGfxChunkBySelector_8028320(void) {
    if (R7 == 1) {
        R0 = A_byte_86E79CC + 0x300;  /* source chunk in ROM */
        R2 = 0x100;                   /* chunk size */
        GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    } else {
        R0 = A_byte_802A700;
        R3 = 3;
        GUEST_CALL(sub_802868C);
        u32 bumped = R1 + 0xC0;
        SET_FLAGS_ADD(R1, 0xC0);      /* flags of ADDS r1, r1, #0xC0 */
        R1 = bumped;
    }
    RETURN_TO_CALLER();
}

/* ── bg_ReloadTileBlock3Screen_80283B0 ── */
/* bg_ReloadTileBlock3Screen_80283B0: reinitialize whatever sub_80283C8 tracks, then load a full
 * background screen into 32x32 tile block 3 from the tile map at
 * unk_2035186 (15 tiles wide, 6 tiles tall).
 *
 * Register contract: takes r4/r5/r10 from the caller and restores r4/r5
 * on return; r0-r3 and the flags on exit are whatever CopyBackgroundTiles
 * leaves behind. */
Next bg_ReloadTileBlock3Screen_80283B0(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;

    /* Run the companion setup routine first. */
    GUEST_CALL(sub_80283C8);

    /* CopyBackgroundTiles(j, i, which_tile_block_32x32, tile_ids, j_size, i_size) */
    R0 = 0;               /* j = 0 */
    R1 = 0xD;             /* i = 13 */
    R2 = 3;               /* tileBlock32x32 = 3 */
    R3 = A_unk_2035186;   /* tileIds */
    R4 = 0xF;             /* j_size = 15 */
    R5 = 6;               /* i_size = 6 */
    GUEST_CALL(CopyBackgroundTiles);

    /* Restore the caller's r4/r5 (the original saves them across both calls). */
    R4 = saved_r4;
    R5 = saved_r5;
    RETURN_TO_CALLER();
}

/* ── customScreen_ExitSequence_8028E32 ── */
/* customScreen_ExitSequence_8028E32: exit sequence for the Custom screen.
 * Runs the close-out steps (update the custom screen state after the player
 * unselects a chip, then the two follow-up updates) and finally calls
 * sub_8028250 with r0 = 0 before returning to the caller. */
Next customScreen_ExitSequence_8028E32(void) {
    u32 return_address = LR;

    GUEST_CALL(sub_8028E4C);
    GUEST_CALL(updateCustomScreen_WhenUnselectingChip_8028EC8);
    GUEST_CALL(battle_updateActiveChipSlotB_state_8028F48);
    GUEST_CALL(battle_updateExtraSlotChipKeepFlag_8028F84);

    R0 = 0;
    GUEST_CALL(sub_8028250);

    LR = return_address;
    RETURN_TO_CALLER();
}

/* ── battle_getActorEmotion_802A564 ── */
/* battle_getActorEmotion_802A564: fetch the current battle emotion for the acting object.
 * r10 = toolkit; r5 = battle object. Loads the battle state's "unk_0d" byte
 * (the alliance/emotion selector) and dispatches on object field 0x15:
 * when it is zero, resolve the emotion (transformation/mood/beast-out side
 * outputs land in r0-r3); otherwise take the alternate lookup. Results are
 * whatever the callee leaves in r0-r3. */
Next battle_getActorEmotion_802A564(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);

    R0 = LD8(battleState + oBattleState_Unk_0d);
    if (LD8(R5 + 0x15) == 0)
        GUEST_CALL(callPossiblyGetBattleEmotion_8015B54);
    else
        GUEST_CALL(sub_8015B5C);

    RETURN_TO_CALLER();
}

/* ── runObjectStateHandler_802C1D4 ── */
/* runObjectStateHandler_802C1D4: two-way handler dispatcher for the object in R5.
 *
 * The object carries a handler selector at offset +3: a byte offset into a
 * two-entry table (0 selects the first handler, 4 the second). The selected
 * handler is invoked with:
 *   R0 = the selector byte (the handler's argument),
 *   R1 = the raw table entry, i.e. the handler's own Thumb address,
 *   LR = the address of the `pop {pc}` that follows the call, so the handler
 *        returns straight through to this function's caller,
 *   SP = four bytes lower than on entry (the pushed LR is visible at [SP] to
 *        the handler's stack-relative addressing).
 * R2-R12 and the flags pass through untouched, and whatever the handler
 * leaves in the registers and flags becomes this function's result. */
Next runObjectStateHandler_802C1D4(void) {
    u32 selector = LD8(R5 + 3);
    u32 handler  = LD32(A_off_802C1E8 + selector);

    /* push {lr}: the handler runs with SP four below our entry SP. */
    SP -= 4;
    ST32(SP, LR);

    R0 = selector;
    R1 = handler;
    if (selector < 4)
        GUEST_CALL(initShuffledOrderTable42_802C1F0);
    else
        GUEST_CALL(sub_802C218);

    /* Handler returned normally; undo the push (its slot was only ever
     * reachable through SP while the handler ran). */
    SP += 4;

    RETURN_TO_CALLER();
}

/* ── battle_isAllianceActiveAndAiFlag0x4000_802DD2A ── */
/* battle_isAllianceActiveAndAiFlag0x4000_802DD2A(r0 = battle object):
 * Returns 1 in r0 only if the battle object's navi-stats byte 0x29 is non-zero
 * AND the AI-data flag word has bit 0x4000 set; otherwise returns 0.
 * On the success path r1 ends as 0x2000 (constant left by the original),
 * otherwise as the callees/test left it. The final "mov r0, r7" of the
 * original is Thumb "adds r0, r7, #0", which is what sets the outgoing
 * flags (N/Z from the result, C = 0, V = 0) on every path. */
Next battle_isAllianceActiveAndAiFlag0x4000_802DD2A(void) {
    u32 savedR7 = R7;
    u32 result = 0;

    R7 = 0;                     /* the callees are called with r7 = 0 */
    R1 = 0x29;                  /* navi stats byte index */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    if ((s32)R0 != 0) {         /* cmp r0, #0; beq */
        GUEST_CALL(GetAIDataUnk0x48Flag);
        if (R0 & 0x4000) {      /* tst r0, r1 (bit 0x4000) */
            R1 = 0x2000;
            result = 1;
        }
    }

    R7 = savedR7;
    R0 = result;
    /* mov r0, r7 == adds r0, r7, #0: sets N/Z from the result, C = 0, V = 0 */
    SET_FLAGS_ADD(result, 0);
    RETURN_TO_CALLER();
}

/* ── sub_802DE5C ── */
/* sub_802DE5C: if test battle flag 0x40 is set, run sub_802E2C4 for
 * indices 0 and 1. Otherwise do nothing. */
Next sub_802DE5C(void) {
    GUEST_CALL(TestBattleFlag_0x40);
    if (R0 != 0) { /* flag test returned set (Z clear in the original) */
        R0 = 0;
        GUEST_CALL(sub_802E2C4);
        R0 = 1;
        GUEST_CALL(sub_802E2C4);
    }
    RETURN_TO_CALLER();
}

/* ── battle_initWorkArea_802DFFC ── */
/* battle_initWorkArea_802DFFC: initialize the battle work area.
 * Zeroes the 0x3A0-byte battle work buffer, records the battle object table
 * pointer in the battle state, and zeroes an allied structure. r2/r3/r4 are
 * untouched throughout. */
Next battle_initWorkArea_802DFFC(void) {
    /* clear the whole battle work buffer: 0xE8 words = 0x3A0 bytes */
    R0 = A_unk_2036120;
    R1 = 0xE8 << 2;
    GUEST_CALL(ZeroFillByWord);

    /* battle state's object-table pointer field = battle object 0 */
    R0 = A_eT1BattleObject0_Flags;
    ST32(A_unk_20362F0 + 0x44, R0); /* dword_2036334 */

    GUEST_CALL(zeroFill_802E544);

    RETURN_TO_CALLER();
}

/* ── sub_802EF5C ── */
/* sub_802EF5C: run a check (battle_testFlag0x40_800A8F8); if it reports "active", pump the
 * sub_802EF74 state machine with argument 0 followed by argument 1.
 * Flags/results are whatever the last executed call (or the zero check) left. */
Next sub_802EF5C(void) {
    GUEST_CALL(battle_testFlag0x40_800A8F8);

    if (R0 == 0) {
        /* cmp r0, #0 from the original is the last flag-setting op on this path */
        SET_FLAGS_CMP(R0, 0);
        RETURN_TO_CALLER();
    }

    R0 = 0;
    GUEST_CALL(sub_802EF74);

    R0 = 1;
    GUEST_CALL(sub_802EF74);

    RETURN_TO_CALLER();
}

/* ── eventflags_ClearTwoHalfwordsAt2DE_802F0D8 ── */
/* eventflags_ClearTwoHalfwordsAt2DE_802F0D8:
 * Clears two halfwords of the event flags area (offset 0x2DE in the
 * toolkit's event flags buffer). Common "reset a group of event bits"
 * routine. r2/r3 are preserved; nothing meaningful is returned. */
Next eventflags_ClearTwoHalfwordsAt2DE_802F0D8(void) {
    u32 eventFlags = LD32(R10 + oToolkit_EventFlagsPtr);

    R0 = eventFlags + 0x2DE;
    R1 = 2;
    GUEST_CALL(ZeroFillByHalfword);

    RETURN_TO_CALLER();
}

/* ── eventFlags_ClearRangeAt2C8_802F0F4 ── */
/* eventFlags_ClearRangeAt2C8_802F0F4: clear a stretch of event flags.
 * Zeroes 0x16 halfwords starting at toolkit->eventFlags + 0x2C8. */
Next eventFlags_ClearRangeAt2C8_802F0F4(void) {
    u32 flags = LD32(R10 + oToolkit_EventFlagsPtr);

    R0 = flags + 0x2C8;
    R1 = 0x16;
    GUEST_CALL(ZeroFillByHalfword);

    RETURN_TO_CALLER();
}
