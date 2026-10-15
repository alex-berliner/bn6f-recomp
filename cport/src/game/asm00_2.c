/*
 * asm00_2.c: readable functions of bn6f asm00_2.s, up to 0x08012BA2, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_getSelectedChipForUse_800ED90 ── */
/* battle_getSelectedChipForUse_800ED90(r0 = battle object):
 * Collect the chip data the object currently has selected for use.
 *
 * If the object's name ID decodes to kind 2, the selection comes from that
 * side's battle hand: getBattleHandAddr_8010018 gives the hand, the byte at
 * its start selects a card entry (2 bytes per slot, after a 2-byte header),
 * and the entry's fields are combined into the results:
 *   r0 = chip ID              (entry +0x00)
 *   r1 = entry sub-value      (entry +0x0C)
 *   r2 = entry +0x18 value plus battle_chipElementWeaknessBonus_800EF34(chip) plus entry +0x24 value
 * Otherwise the object's own oBattleObject_Chip field is returned with
 * r1/r2 cleared. r3 always receives the object's ChipsHeld count.
 *
 * Flag contract: V is left as the original's last flag-setting instruction
 * on each path (the kind comparison, or the final adds on the hand path);
 * nz/c are clobbered.
 */
Next battle_getSelectedChipForUse_800ED90(void) {
    u32 obj = R0;
    u32 savedR5 = R5;
    u32 savedR7 = R7;

    /* The callees below run with r5 = object, exactly like the original
     * (mov r5, r0 before the first call). r5/r7 are pushed by the original
     * and restored on return, so save them here and write them back before
     * returning. */
    R5 = obj;

    R0 = LD16(obj + oBattleObject_NameID);
    GUEST_CALL(unpackVerActorTyAndAIIdx_800F29C);
    u32 kind = R0;
    SET_FLAGS_CMP(kind, 2);

    if (kind != 2) {
        /* Not hand-driven: expose the object's own chip selection. */
        R0 = LD16(obj + oBattleObject_Chip);
        R1 = 0;
        R2 = 0;
        R3 = LD8(obj + oBattleObject_ChipsHeld);
        R5 = savedR5;
        R7 = savedR7;
        RETURN_TO_CALLER();
    }

    /* Address of the card entry selected in this side's battle hand. */
    R0 = LD8(obj + oBattleObject_Alliance);
    GUEST_CALL(getBattleHandAddr_8010018);
    u32 hand = R0;
    u32 entry = hand + 2 * LD8(hand) + 2;
    R7 = entry;

    R0 = LD16(entry);
    R1 = 0;
    GUEST_CALL(battle_chipElementWeaknessBonus_800EF34);

    u32 baseValue = LD16(entry + 0x18) + R0;
    u32 extra = LD16(entry + 0x24);
    R2 = baseValue + extra;
    SET_FLAGS_ADD(baseValue, extra);
    R1 = LD16(entry + 0xC);
    R0 = LD16(entry);
    R3 = LD8(obj + oBattleObject_ChipsHeld);
    R5 = savedR5;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_addBarrierHitAdjust_800F1DC ── */
/* battle_addBarrierHitAdjust_800F1DC(r0 = value, r2 = incoming hit/event type, r5 = battle object):
 * Certain incoming types (0x5F..0x61, and the special value 0x150) interact
 * with the target's barrier: if an active barrier is present, add 0x32 (50)
 * to r0. r3 returns the collision data pointer (or 0x150 when the type check
 * fails early); r2 ends up holding the barrier/Unk_16 byte on barrier paths.
 * Flags reflect the original's last comparison/addition on each exit path.
 * r1, r6, r7 and LR are untouched. */
Next battle_addBarrierHitAdjust_800F1DC(void) {
    u32 hitType = R2;

    R3 = 0x150;
    if (hitType != 0x150 && ((s32)hitType < 0x5F || (s32)hitType > 0x61)) {
        /* Not a barrier-relevant type: leave r0 alone. */
        if ((s32)hitType < 0x5F)
            SET_FLAGS_CMP(hitType, 0x5F);
        else
            SET_FLAGS_CMP(hitType, 0x61);
        RETURN_TO_CALLER();
    }

    {
        u32 coll = LD32(R5 + oBattleObject_CollisionDataPtr);
        u32 barrier = LD8(coll + 6); /* oCollisionData_Barrier */

        R3 = coll;
        R2 = barrier;

        if (barrier == 0) {
            /* No barrier active. */
            SET_FLAGS_CMP(barrier, 0);
            RETURN_TO_CALLER();
        }
        if ((s32)barrier >= 0x10) {
            /* Barrier value out of the "active" range. */
            SET_FLAGS_CMP(barrier, 0x10);
            RETURN_TO_CALLER();
        }
        if (barrier == 8) {
            u32 unk16 = LD8(coll + oCollisionData_BarrierHP);

            R2 = unk16;
            if (unk16 == 0) {
                /* Variant 8 with no charge left: no adjustment. */
                SET_FLAGS_CMP(unk16, 0);
                RETURN_TO_CALLER();
            }
        }

        /* Active barrier: add 50 to the value in r0. */
        R0 += 0x32;
        SET_FLAGS_ADD(R0, 0x32);
        RETURN_TO_CALLER();
    }
}

/* ── enemy_getStruct1 ── */
/* enemy_getStruct1(r0 = enemy index):
 * Resolves an enemy index to its per-enemy struct pointer.
 * GetVerActorTyAndAIIdx_80182B4 gives (version, actor_type, ai_index); the
 * actor_type selects one of the top-level tables (virus / navi / player) and
 * the ai_index indexes within it.
 * r0 = struct pointer, r2 = AI index * 4. */
Next enemy_getStruct1(void) {
    u32 enemy_idx = R0;

    R0 = enemy_idx;
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4);
    const u8 *info = (const u8 *)R0; /* version, actor_type, ai_index */

    u32 actor_type = LD8((u32)info + 1);
    u32 ai_index   = LD8((u32)info + 2);

    u32 per_type_table = LD32(A_off_800F230 + (actor_type << 2));

    R2 = ai_index << 2;
    R0 = LD32(per_type_table + R2);
    RETURN_TO_CALLER();
}

/* ── enemy_getStruct2 ── */
/* enemy_getStruct2(r0 = enemy_idx):
 * Resolve an enemy index into a pointer to its per-version data record.
 * The version/actor-type/AI lookup returns a triple; the actor type selects one
 * of the virus/navi/player table groups, the AI index picks a table within the
 * group, and the version selects a 6-byte record inside that table.
 * r0 = pointer to the record, r3 = 6 (record size, as in the original). */
Next enemy_getStruct2(void) {
    GUEST_CALL(GetVerActorTyAndAIIdx_80182B4); /* r0 = &{version, actor_type, ai_index} */

    u32 info      = R0;
    u32 version   = LD8(info + 0);
    u32 actorType = LD8(info + 1);
    u32 aiIndex   = LD8(info + 2);

    u32 groupTable = LD32(A_off_800F260 + actorType * 4);
    u32 recordTable = LD32(groupTable + aiIndex * 4);

    R0 = recordTable + version * 6;
    R3 = 6; /* the original leaves the multiplier in r3 */
    RETURN_TO_CALLER();
}

/* ── enemy_getArmPosition_800F26C ── */
/* enemy_getArmPosition_800F26C(r0 = enemy ID):
 * For IDs 0xCD..0xFF, return the 5-byte entry from the table byte_8021220
 * indexed by (ID - 0xCD). For any other ID, treat it as a regular enemy and
 * return the first two bytes of its struct (via enemy_getStruct1) with the
 * remaining outputs zeroed.
 * Outputs: r0..r4 (in the table path, r4 is the 5th byte; in the struct path
 * r4 is left untouched). */
Next enemy_getArmPosition_800F26C(void) {
    u32 id = R0;

    if ((u32)(id - 0xCD) > 0x32) {
        /* Not one of the special IDs: look up the enemy's own struct. */
        R0 = id;
        GUEST_CALL(enemy_getStruct1);
        u32 struct1 = R0;

        R0 = LD8(struct1 + 0);
        R1 = LD8(struct1 + 1);
        R2 = 0;
        R3 = 0;
        RETURN_TO_CALLER();
    }

    /* Special ID: fetch its fixed 5-byte data record. */
    u32 entry = A_byte_8021220 + (id - 0xCD) * 5;
    R0 = LD8(entry + 0);
    R1 = LD8(entry + 1);
    R2 = LD8(entry + 2);
    R3 = LD8(entry + 3);
    R4 = LD8(entry + 4);
    RETURN_TO_CALLER();
}

/* ── nullsub_3 ── */
/* nullsub_3: empty placeholder function; does nothing. */
Next nullsub_3(void) {
    RETURN_TO_CALLER();
}

/* ── object_setAnimation ── */
/* object_setAnimation(r0 = animation id, r5 = battle object):
 * Start playing the given animation on the object: store it as the current
 * animation, then write 0xFF into the "queued animation" slot, which the
 * animation task picks up on its next update.
 * The call to nullsub_3 is an empty hook; it changes nothing, so the value
 * stored as the current animation is the animation id passed in r0.
 * Returns r0 = 0xFF (the value written to CurAnimCopy) and
 * r1 = the object's NameID, which the original loads before the hook call. */
Next object_setAnimation(void) {
    u32 anim = R0;
    u32 obj = R5;

    R1 = LD16(obj + oBattleObject_NameID);

    ST8(obj + oBattleObject_CurAnim, anim);
    ST8(obj + oBattleObject_CurAnimCopy, 0xFF);

    R0 = 0xFF;
    RETURN_TO_CALLER();
}

/* ── battle_SetObjectAIDataField78_800F318 ── */
/* battle_SetObjectAIDataField78_800F318(r0 = value, r5 = battle object):
 * Store r0 into the object's AI data at offset oAIData_BattleObjectPtr, if it has any. */
Next battle_SetObjectAIDataField78_800F318(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    if (aiData != 0)
        ST32(aiData + oAIData_BattleObjectPtr, R0);

    R1 = aiData;
    RETURN_TO_CALLER();
}

/* ── getBattleObjectPalette_800F334 ── */
/* getBattleObjectPalette_800F334(r5 = battle object) -> r0 = palette index.
 *
 * Returns the palette to use for a battle object, based on its AI data:
 * viruses index directly by version * unk03, while every other actor looks
 * its palette base up in a small per-version table (byte_800F354) and then
 * scales it by unk03 the same way.
 *
 * Also leaves r1 = actor type and r2 = version (version * unk03 for viruses)
 * in the registers, matching the original.
 */
#define ACTOR_TYPE_VIRUS 0

Next getBattleObjectPalette_800F334(void) {
    u32 aiData    = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData);                    /* oAIData_ActorType is at offset 0 */
    u32 version   = LD8(aiData + oAIData_Version_16);
    u32 unk03     = LD8(aiData + 3);                /* oAIData_Unk_03 */
    u32 palette;

    if (actorType == ACTOR_TYPE_VIRUS) {
        /* Viruses: the palette comes straight from version * unk03. */
        palette = version * unk03;
        version = palette;  /* the original's mul writes back into r2 */
    } else {
        /* Others: byte_800F354 maps the version to a palette base. */
        palette = LD8(A_byte_800F354 + version) * unk03;
    }

    R0 = palette;
    R1 = actorType;
    R2 = version;
    RETURN_TO_CALLER();
}

#undef ACTOR_TYPE_VIRUS

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

/* ── battle_getObjectAllianceTeam_800F49E ── */
/* battle_getObjectAllianceTeam_800F49E(r5 = battle object):
 * Look up the object's alliance entry: r0 = allianceTeams[alliance],
 * r1 = pointer to the allianceTeams table (dword_203CFA0). */
Next battle_getObjectAllianceTeam_800F49E(void) {
    u32 allianceTable = A_dword_203CFA0;
    u32 alliance = LD8(R5 + oBattleObject_Alliance);

    R1 = allianceTable;
    R0 = LD32(allianceTable + alliance * 4);
    RETURN_TO_CALLER();
}

/* ── dirOffsetFromAlliance_800F9DE ── */
/* register form: calls object_getAllianceDirection outside this file, which has no typed C interface yet;
 * preserves r4 across the GUEST_CALL to mirror the push {r4,lr}/pop {r4,pc} frame in the original. */
static u32 get_alliance_direction(u32 object) {
    u32 saved_r4 = R4;
    R0 = object;
    GUEST_CALL(object_getAllianceDirection);
    u32 result = R0;
    R4 = saved_r4;
    return result;
}

typedef struct { u32 dy; u32 dx; } DirOffset;

/* Direction offset table at byte_800FA14 holds six entries of two signed bytes
 * (dy, dx) each:
 *   index 0: { 0,  0}
 *   index 1: { 0, -1}
 *   index 2: { 0,  1}
 *   index 3: {-1,  0}
 *   index 4: { 1,  0}
 *   index 5: { 0,  0}
 * Look up the entry for `index`, transform `src_dir` through object_getAllianceDirection,
 * and return (alliance_dir * table.dy, table.dx). */
static DirOffset direction_lookup(u32 index, u32 src_dir) {
    u32 alliance_dir = get_alliance_direction(src_dir);
    u32 addr = A_byte_800FA14 + (index * 2);
    s32 dy = (s8)LD8(addr);
    s32 dx = (s8)LD8(addr + 1);
    DirOffset result;
    result.dy = (u32)((s32)alliance_dir * dy);
    result.dx = (u32)dx;
    return result;
}

/* dirOffsetFromAlliance_800F9DE(r0 = direction index, r1 = source direction) -> r0 = dy*alliance, r1 = dx */
Next dirOffsetFromAlliance_800F9DE(void) {
    DirOffset r = direction_lookup(R0, R1);
    R0 = r.dy;
    R1 = r.dx;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_SpawnObjectOnFrontPanel_800FAAC ── */
/* battle_SpawnObjectOnFrontPanel_800FAAC(r0, r1, r2, r3 = effect parameters, r5 = battle object):
 * Spawns a type-3 battle object on the panel in front of the given object
 * and finishes its setup using the object's alliance stats byte.
 * Returns scanFrontPanels_800FAF6's results in r0-r3; preserves r4-r7. */
Next battle_SpawnObjectOnFrontPanel_800FAAC(void) {
    u32 savedR0 = R0, savedR1 = R1, savedR3 = R3;
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;
    u32 object = R5;

    /* The original stashes r0/r1/r3 in the scratch registers r4/r6/r7
     * (they are restored from the stack before returning). */

    /* Work out which panel the object is facing. */
    GUEST_CALL(object_getFrontDirection);
    u32 spawnX = R0 + (u32)LD8(object + oBattleObject_PanelX);
    u32 spawnY = (u32)LD8(object + oBattleObject_PanelY);

    /* Spawn and initialise the type-3 object at that panel.
     * r2 passes through unchanged; r3/r5 carry the caller's values. */
    R0 = spawnX;
    R1 = spawnY;
    R4 = savedR0;
    R6 = savedR1;
    R7 = savedR3;
    GUEST_CALL(battle_SpawnAndInitObjectType3_80C4FFE);
    u32 spawnR2 = R2, spawnR3 = R3;

    /* Read byte 2 of the object's alliance's battle navi stats. */
    R0 = (u32)LD8(object + oBattleObject_Alliance);
    R1 = 2;
    R2 = spawnR2;
    R3 = spawnR3;
    GUEST_CALL(GetBattleNaviStatsByte);
    u32 statsByte = R0;

    /* Complete the new object's setup with the panel position and the
     * alliance-dependent stats byte. */
    R0 = spawnX;
    R1 = spawnY;
    R2 = statsByte;
    R3 = spawnR3;
    R4 = savedR0;
    R5 = object;
    R6 = savedR1;
    R7 = savedR3;
    GUEST_CALL(scanFrontPanels_800FAF6);

    /* r0-r3 hold scanFrontPanels_800FAF6's results; restore the scratch registers,
     * as the original's pop {r4,r6,r7,pc} does. */
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_snapshotPlayerHP_800FAE0 ── */
/* battle_snapshotPlayerHP_800FAE0: snapshot the player's current HP into the battle state
 * (oBattleState_Unk_34), used to track HP changes over the course of a turn.
 *
 * inputs:  r4 r5 r6 r10 (r10 = toolkit) lr
 * outputs: r2 r4 r5 r6 (r4/r5/r6 preserved; r2 as left by battle_findPlayer)
 * clobbers: nz c v r0 r1 lr
 */
Next battle_snapshotPlayerHP_800FAE0(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    /* Look up the player object (argument: which alliance to search). */
    R0 = LD8(battle + oBattleState_PlayerSide);
    GUEST_CALL(battle_findPlayer);

    /* If a player exists, remember its HP. */
    if (R0 != 0)
        ST16(battle + oBattleState_Unk_34, LD16(R0 + oBattleObject_HP));

    RETURN_TO_CALLER();
}

/* ── scanFrontPanels_800FAF6 ── */
/*
 * scanFrontPanels_800FAF6 - forward panel scan for a battle object.
 *
 * Reads the direction the object is facing, then walks one panel at a time
 * from the object's position in that direction, counting the panels it
 * crosses.  The walk stops at the first panel that either has no panel
 * parameters (i.e. is off the field) or whose type bits include the stop mask
 * for the object's alliance (one mask word per alliance in byte_800FB4C).
 * The count is capped at 5, and together with the column selector in r2 it
 * indexes the per-column result table at byte_80209CC, whose byte is the
 * function's answer.
 */

#define FRONT_SCAN_MAX_STEPS 5

/* register form: calls object_getFrontDirection outside this file, which has no typed C interface yet */
static s32 object_front_direction(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(object_getFrontDirection);
    return (s32)R0;
}

/* register form: calls object_getPanelParameters outside this file, which has no typed C interface yet */
static u32 object_panel_parameters(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(object_getPanelParameters);
    return R0;
}

typedef struct {
    u32 table_byte;  /* byte_80209CC[column * 6 + steps]           -> r0 */
    u32 index;       /* column * 6 + steps                         -> r1 */
    u32 index_add_a; /* the two operands of the original's final   */
    u32 index_add_b; /* ADDS, so the adapter can set nz/c/v from it */
} front_scan_result;

static front_scan_result scan_front_panels(u32 x, u32 y, u32 column, u32 object)
{
    front_scan_result out;
    s32 step = object_front_direction(x, y);
    s32 steps = 0;

    for (;;) {
        u32 panel = object_panel_parameters(x, y);
        if (panel == 0)
            break; /* walked off the field */

        u32 alliance = LD8(object + oBattleObject_Alliance);
        u32 stop_mask = LD32(A_byte_800FB4C + alliance * 4);
        if (panel & stop_mask)
            break; /* a panel type reserved against this alliance blocks the way */

        steps++;
        x += step;
    }

    if (steps > FRONT_SCAN_MAX_STEPS)
        steps = FRONT_SCAN_MAX_STEPS;

    out.index_add_a = column * 6;
    out.index_add_b = steps;
    out.index = out.index_add_a + out.index_add_b;
    out.table_byte = LD8(A_byte_80209CC + out.index);
    return out;
}

/* scanFrontPanels_800FAF6(r0 = x, r1 = y, r2 = column, r5 = object) -> r0, r1, r2, nz/c/v */
Next scanFrontPanels_800FAF6(void) {
    front_scan_result out = scan_front_panels(R0, R1, R2, R5);
    R0 = out.table_byte;
    R1 = out.index;
    R2 = 6; /* the original's "mov r2, #6" mul operand is still live at return */
    SET_FLAGS_ADD(out.index_add_a, out.index_add_b);
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

/* ── chip_class_lookup_800FC9E ── */
/* chip_class_lookup: r0==0 reads table0[r1], r0!=0 reads table1[r0].
 * In the original, the r0!=0 path is a single byte load at 0x0800FCD5 + r0
 * from ROM (no length check), so it may return arbitrary ROM bytes for large r0. */
static u32 chip_class_lookup(u32 class, u32 code, u32 *flag_out) {
    if (class == 0) {
        *flag_out = 0;
        return LD8(0x0800FCBC + code);
    } else {
        *flag_out = 8;
        return LD8(0x0800FCD5 + class);
    }
}

/* chip_class_lookup_800FC9E(r0 = class, r1 = code) -> r1 = (0 | 8), r2 = table byte */
Next chip_class_lookup_800FC9E(void) {
    u32 flag;
    R2 = chip_class_lookup(R0, R1, &flag);
    R1 = flag;
    RETURN_TO_CALLER();
}

/* ── object_setDefaultCounterTime ── */
/* object_setDefaultCounterTime:
 * Reset the counter timer of the object the caller is working on (the callee
 * picks the object up from r5/r10) to the game's default of 0x10 frames.
 * Everything else (result registers, flags, LR) is whatever the callee leaves
 * behind; this wrapper only supplies the default time in r0. */
Next object_setDefaultCounterTime(void) {
    R0 = 0x10;                       /* default counter time */
    GUEST_CALL(object_setCounterTime);
    RETURN_TO_CALLER();
}

/* ── setChipsForPlayerObject_800FDEA ── */
/* setChipsForPlayerObject_800FDEA(r5 = battle object):
 * Record on a battle object which chips it holds from its alliance's battle
 * hand.  The hand buffer starts with a count byte followed by halfword chip
 * entries; this object's entries are the ones after the first `hand[0]` of
 * them, up to the 0xFFFF terminator.  The number of those entries is stored
 * in oBattleObject_ChipsHeld and the first entry's chip ID (or 0xFFFF if
 * there are none) in oBattleObject_Chip.
 *
 * Register side effects left for callers: r1 points at the terminator entry,
 * r2 and r3 both hold 0xFFFF.  r4 and r5 are preserved. */
Next setChipsForPlayerObject_800FDEA(void) {
    u32 object = R5;

    R0 = LD8(object + oBattleObject_Alliance);
    GUEST_CALL(getBattleHandAddr_8010018);
    u32 hand = R0;

    /* Start of this object's section: skip the count byte and the halfword
     * entries it accounts for. */
    u32 cursor = hand + 2 + 2 * LD8(hand);
    u32 firstChip = LD16(cursor);

    /* Count this object's chip entries up to the 0xFFFF terminator. */
    u32 count = 0;
    u32 entry = firstChip;
    while (entry != 0xFFFF) {
        count++;
        cursor += 2;
        entry = LD16(cursor);
    }

    ST8(object + oBattleObject_ChipsHeld, count);
    ST16(object + oBattleObject_Chip, firstChip);

    /* State the original leaves behind in the output registers. */
    R1 = cursor;   /* address of the terminator entry */
    R2 = entry;    /* terminator value, i.e. 0xFFFF */
    R3 = 0xFFFF;
    RETURN_TO_CALLER();
}

/* ── battle_aiGetVersionedTableEntry_800FE12 ── */
/* battle_aiGetVersionedTableEntry_800FE12(r0 = table base, r5 = battle object):
 * Reads one entry from the AI table at r0. For AI version 4 the table entry at
 * offset 8 is used and scaled by the byte at oAIData_Unk_0c; any other version
 * indexes the table with version * 2.
 * Returns r0 = value, r1 = AI data pointer, r2 = version * 2 / multiplier. */
Next battle_aiGetVersionedTableEntry_800FE12(void) {
    u32 table = R0;
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 version = LD8(aiData + oAIData_Version_16);

    R1 = aiData;

    if (version == 4) {
        u32 mult = LD8(aiData + oAIData_Unk_0c);
        R0 = LD16(table + 8) * mult;
        R2 = mult;
    } else {
        R0 = LD16(table + version * 2);
        R2 = version * 2;
    }

    RETURN_TO_CALLER();
}

/* ── battle_getAiDifficultyTier_800FE36 ── */
/* battle_getAiDifficultyTier_800FE36(r5 = battle object):
 * Maps the AI data's difficulty byte to a tier (0-2) in r0, but only when the
 * AI data version is exactly 4. Note the original clobbers r0 with the loaded
 * version byte even when it bails out early, so r0 = version on that path.
 * Also returns: r1 = AI data pointer, r2 = raw difficulty byte (version 4 only).
 * Flags reflect the last comparison performed, as in the original. */
Next battle_getAiDifficultyTier_800FE36(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    R1 = aiData;

    /* ldrb writes the version into r0 before the check, so r0 holds it
     * even if we return early below. */
    u32 version = LD8(aiData + oAIData_Version_16);
    R0 = version;
    SET_FLAGS_CMP(version, 4);
    if (version != 4) {
        /* Wrong AI data version: bail out, flags from the version compare. */
        RETURN_TO_CALLER();
    }

    u32 difficulty = LD8(aiData + oAIData_Unk_0c);
    R2 = difficulty;

    R0 = 0;
    SET_FLAGS_CMP(difficulty, 3);
    if ((s32)difficulty > 3) {
        R0 = 1;
        SET_FLAGS_CMP(difficulty, 6);
        if ((s32)difficulty > 6) {
            R0 = 2;
            /* Thumb "mov rN, #imm" sets NZ; the final MOV leaves those flags. */
            SET_FLAGS_NZ(R0);
        }
    }
    RETURN_TO_CALLER();
}

/* ── countActiveNaviBonuses_800FE52 ── */
/* register form: calls GetBattleNaviStatsByte_AllianceFromBattleObject outside this file, which has no typed C interface yet */
static u32 battle_navi_stats_byte(u32 r5_arg, u32 offset) {
    R5 = r5_arg;
    R1 = offset;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    return R0;
}

/* register form: calls GetBattleNaviStatsHword_AllianceFromBattleObject outside this file, which has no typed C interface yet */
static u32 battle_navi_stats_hword(u32 r5_arg, u32 offset) {
    R5 = r5_arg;
    R1 = offset;
    GUEST_CALL(GetBattleNaviStatsHword_AllianceFromBattleObject);
    return R0;
}

/* Counts how many Navi-bonus stat fields are active on the allied BattleObject (R5).
 * Field 0x31 counts only when equal to 1; the rest count when non-zero.
 * Output: r0 = the count. */
static u32 count_active_navi_bonuses(u32 r5_in) {
    u32 count = 0;

    if (battle_navi_stats_byte(r5_in, 0x31) == 1) count++;
    if (battle_navi_stats_byte(r5_in, 0x13) != 0) count++;
    if (battle_navi_stats_byte(r5_in, 0x14) != 0) count++;
    if (battle_navi_stats_byte(r5_in, 0x16) != 0) count++;
    if (battle_navi_stats_hword(r5_in, 0x54) != 0) count++;
    if (battle_navi_stats_byte(r5_in, 0x24) != 0) count++;
    if (battle_navi_stats_byte(r5_in, 0x18) != 0) count++;
    if (battle_navi_stats_byte(r5_in, 0x19) != 0) count++;
    if (battle_navi_stats_byte(r5_in, 0x1A) != 0) count++;
    if (battle_navi_stats_byte(r5_in, 0x63) != 0) count++;

    return count;
}

Next countActiveNaviBonuses_800FE52(void) {
    R0 = count_active_navi_bonuses(R5);
    /* r4 is restored from the stack by the original `pop {r4,pc}`; the typed function
     * never touched R4, so it still holds the caller's saved value. */
    RETURN_TO_CALLER();
}

/* ── sub_800FF5E ── */
/* register form: calls GetBattleNaviStatsByte_AllianceFromBattleObject outside this file, which has no typed C interface yet */
static u32 navi_stat_byte(u32 stat_offset) {
    R1 = stat_offset;   /* the battle object rides in the live r5, as in the original */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    return R0;
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 get_battle_mode_sub_800FF5E(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls ai_setBeastOutPwrAtk_800FFAA outside this file, which has no typed C interface yet */
static void set_beast_out_pwr_atk(u32 ai_data, u32 value) {
    u32 caller_r4 = R4;
    R0 = value;
    R4 = ai_data;    /* the callee takes the AI data block in r4 (battle object stays in the live r5) */
    GUEST_CALL(ai_setBeastOutPwrAtk_800FFAA);
    R4 = caller_r4;  /* the original keeps r4 pushed across the whole function; leave the caller's value */
}

/* (Re)fill the AI data block of the navi that owns battle_object from its alliance's navi
 * stats table: unknown stat 0x04, the B-button ability, the attack powers and the BeastOut
 * attack.  A set stat 0x2C means the navi manages its own AI data and is left untouched. */
static void refresh_ai_data(u32 battle_object) {
    u32 ai = LD32(battle_object + oBattleObject_AIDataPtr);

    if (navi_stat_byte(0x2C) != 0)
        return;              /* this navi manages its AI data itself, leave it alone */

    u32 unk_04;
    if (get_battle_mode_sub_800FF5E() == 9)
        unk_04 = navi_stat_byte(0x44);   /* battle mode 9 has its own stat for this */
    else
        unk_04 = 0xFF;                   /* any other mode: "none" */
    ST8(ai + oAIData_Unk_04, unk_04);

    ST8(ai + oAIData_BButton, navi_stat_byte(4));

    set_beast_out_pwr_atk(ai, navi_stat_byte(5));

    ST8(ai + oAIData_APwrAtk, navi_stat_byte(0x39));
    ST8(ai + oAIData_BLeftAbility, navi_stat_byte(7));
    ST8(ai + oAIData_BeastOutAPwrAtk, 0xFF);   /* BeastOut attack power starts as "none" */
}

/* sub_800FF5E(r5 = battle object): refresh the owner navi's AI data; no outputs. */
Next sub_800FF5E(void) {
    refresh_ai_data(R5);
    RETURN_TO_CALLER();
}

/* ── ai_setBeastOutPwrAtk_800FFAA ── */
/* ai_setBeastOutPwrAtk_800FFAA(r0 = new BPwrAtk value, r4 = AI data):
 * Beast Out bookkeeping for the AI's held power attack. A Beast value
 * (0x21..0x26) is always written; any other value is only written while the
 * stored value is not itself a Beast value, so the Beast state sticks until
 * replaced. When the stored value is a Beast value afterwards, the AI's
 * B-button remap is corrected (3/4 -> 0, 0x2C -> 0x2B) and r2 reports the
 * value written; otherwise r2 is left untouched for the caller. */
#define oAIData_BPwrAtk 7   /* as referenced by the original asm (ldrb/strb r?,[r4,#7]) */
#define oAIData_BButton 6   /* as referenced by the original asm (ldrb/strb r?,[r4,#6]) */

static int is_beast_value(u32 v)
{
    return (s32)v >= 0x21 && (s32)v <= 0x26;
}

Next ai_setBeastOutPwrAtk_800FFAA(void) {
    u32 value = R0;
    u32 ai = R4;
    u32 buttonOut = R2; /* incoming r2 survives the early-exit paths */

    u32 stored = LD8(ai + oAIData_BPwrAtk);
    if (is_beast_value(value) || !is_beast_value(stored))
        ST8(ai + oAIData_BPwrAtk, value);
    /* else: keep the Beast value already stored */

    u32 atk = LD8(ai + oAIData_BPwrAtk);
    if (is_beast_value(atk)) {
        u32 button = LD8(ai + oAIData_BButton);
        if (button == 3 || button == 4) {
            buttonOut = 0;
            ST8(ai + oAIData_BButton, 0);
        } else {
            buttonOut = 0x2B;
            if (button == 0x2C)
                ST8(ai + oAIData_BButton, 0x2B);
        }
    }

    R2 = buttonOut;
    RETURN_TO_CALLER();
}

/* ── ClearAIData_Unk_44_Flag ── */
/* ClearAIData_Unk_44_Flag(r0 = mask, r5 = battle object):
 * clear the bits of `mask` in the AI data's Unk_44 field.
 * Leaves r1 = updated field value, r3 = AI data pointer, NZ set on r1. */
Next ClearAIData_Unk_44_Flag(void) {
    u32 *aiData = (u32 *)LD32(R5 + oBattleObject_AIDataPtr);
    u32 flags = LD32((u32)aiData + oAIData_AIFlags) & ~R0;
    ST32((u32)aiData + oAIData_AIFlags, flags);
    R1 = flags;
    R3 = (u32)aiData;
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── GetAIData_Unk_44_Flag ── */
/* GetAIData_Unk_44_Flag(r5 = battle object):
 * Loads the object's AI data pointer and returns its unknown-44 flag.
 * Outputs: r0 = oAIData_AIFlags value, r3 = AI data pointer. */
Next GetAIData_Unk_44_Flag(void) {
    R3 = LD32(R5 + oBattleObject_AIDataPtr);
    R0 = LD32(R3 + oAIData_AIFlags);
    RETURN_TO_CALLER();
}

/* ── getBattleHandAddr_8010018 ── */
/* getBattleHandAddr_8010018(r0 = hand index):
 * returns the address of battle hand slot r0 * 0x50 bytes into byte_20349C0.
 * r0 = &byte_20349C0 + index * 0x50, r1 = &byte_20349C0. */
Next getBattleHandAddr_8010018(void) {
    R1 = A_byte_20349C0;
    R0 = R0 * 0x50 + R1;
    RETURN_TO_CALLER();
}

/* ── battle_getPlayerAIData_8010022 ── */
/* battle_getPlayerAIData_8010022(r0 = alliance):
 * return the AI data pointer of the player's battle object
 * (r0 selects which side is "the player": false = player, true = opponent).
 * r0 = player's BattleObject.AIDataPtr. */
Next battle_getPlayerAIData_8010022(void) {
    GUEST_CALL(battle_findPlayer); /* in: R0 = alliance; out: R0 = player object */
    R0 = LD32(R0 + oBattleObject_AIDataPtr);
    RETURN_TO_CALLER();
}

/* ── battle_refreshNaviSpritePaletteFromForm_80100EC ── */
/* battle_refreshNaviSpritePaletteFromForm_80100EC: refresh the navi sprite palette of the battle object whose
 * pointer arrives in R0 (other inputs pass through to the helpers).
 * Reads the battle-stats byte at offset 0x2C: for the two cross/beast
 * transformed states (0x17..0x18) sub_8016A38 handles the sprite, otherwise
 * battle_pickPaletteIndexForNavi_801002C computes the palette index (returned in R0/R1) and it is
 * applied with sprite_setPalette. */
Next battle_refreshNaviSpritePaletteFromForm_80100EC(void) {
    R1 = 0x2C;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    if ((s32)R0 >= 0x17 && (s32)R0 <= 0x18) {
        /* transformed form: dedicated sprite update */
        GUEST_CALL(sub_8016A38);
    } else {
        /* normal form: pick the palette, then apply it */
        GUEST_CALL(battle_pickPaletteIndexForNavi_801002C);
        SYNC_SNAP(R0, R1);
        GUEST_CALL(sprite_setPalette);
        SYNC_CHECK(0x801010A, R0, R1);
    }

    RETURN_TO_CALLER();
}

/* ── battle_tickCollisionTimer_8010162 ── */
/* battle_tickCollisionTimer_8010162: tick the collision timer of the battle object whose pointer
 * lives at [r5 + oBattleObject_CollisionDataPtr].
 *
 * The halfword counter at oCollisionData_Unk_26 counts down once per call,
 * unless it is parked at 0xFFFF. When the count reaches zero, sound effect
 * 0x94 plays. Afterwards the object's flags are tested for 0x400000: if that
 * bit is set, flag bit 4 is cleared, otherwise flag bit 4 is set.
 *
 * inputs:  r3 r4 r5 r6 r7 r10 lr (only r5 is read)
 * outputs: r2 = whatever the last flag routine left in it;
 *          r3 r4 r5 r6 r7 r10 preserved; flags are clobbered.
 */

/* flag bit set/cleared at the end of the routine; the value the flag
 * routines receive in r0 (dword_8010228/dword_801022C era constant) */
#define OBJECT_FLAGS_UNK_4 4

Next battle_tickCollisionTimer_8010162(void) {
    u32 savedR4 = R4;

    u32 col = LD32(R5 + oBattleObject_CollisionDataPtr);
    R4 = col;    /* the flag routines below expect the collision pointer in r4 */
    R2 = 0xFFFF; /* dword_8010228: sentinel value carried in r2 */

    u32 timer = LD16(col + oCollisionData_Unk_26);
    if (timer != 0xFFFF) {
        if (timer == 0) {
            /* Counter already expired: skip the countdown, just drop bit 4. */
            R0 = 4;
            GUEST_CALL(object_clearFlag);
            R4 = savedR4;
            RETURN_TO_CALLER();
        }
        timer--;
        ST16(col + oCollisionData_Unk_26, timer);
        if (timer == 0) {
            /* Countdown finished on this tick: play the warning sound. */
            R0 = 0x94;
            R1 = 0;
            GUEST_CALL(PlaySoundEffect);
        }
    }

    GUEST_CALL(object_getFlag);
    R1 = 0x400000; /* dword_801022C */
    if (R0 & 0x400000) {
        R0 = 4;
        GUEST_CALL(object_clearFlag);
    } else {
        R0 = OBJECT_FLAGS_UNK_4;
        GUEST_CALL(object_setFlag1);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_objectClearPendingHitIfCollisionProcessed_8010198 ── */
/* battle_objectClearPendingHitIfCollisionProcessed_8010198(r5 = battle object):
 * When this object's collision data records a pending hit, clear the hit
 * counter, but only if the collision actually set some effect flags.
 * Outputs: r3 = the object's collision data pointer. */
Next battle_objectClearPendingHitIfCollisionProcessed_8010198(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);
    R3 = collision;

    /* Nothing queued this frame? */
    if (LD16(collision + oCollisionData_Unk_26) != 0) {
        /* Only consume the hit if it produced collision effects. */
        if (LD32(collision + oCollisionData_FlagsFromCollision) != 0)
            ST16(collision + oCollisionData_Unk_26, 0);
    }
    RETURN_TO_CALLER();
}

/* ── battle_resetCollisionState_80101C4 ── */
/* battle_resetCollisionState_80101C4 (r5 = battle object): reset the object's collision state.
 * Zeroes the collision data counter at Unk_26 and clears flag bit 2
 * (object_clearFlag arg) on the object's collision data. */
Next battle_resetCollisionState_80101C4(void) {
    u32 collision = LD32(R5 + oBattleObject_CollisionDataPtr);

    ST16(collision + oCollisionData_Unk_26, 0);

    R0 = 4;
    R1 = collision;
    GUEST_CALL(object_clearFlag);

    RETURN_TO_CALLER();
}

/* ── getPetNaviStatus_80101F8 ── */
/* getPetNaviStatus_80101F8: Determines the PET Navi battle status code.
 * Returns the PET Navi index (0xD if the fallback byte at 200AD04 is non-zero,
 * 0 if event flag 0x163 is set or the byte is zero, or the raw Navi ID otherwise). */

#define PET_NAVI_EVENT_BANK   1
#define PET_NAVI_EVENT_OFFSET 0x63
#define PET_NAVI_FALLBACK     0xD

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u32 get_cur_pet_navi(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool pet_navi_event_set(void) {
    R0 = PET_NAVI_EVENT_BANK;
    R1 = PET_NAVI_EVENT_OFFSET;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls notZero_eByte200AD04 outside this file, which has no typed C interface yet */
static bool pet_navi_byte_nonzero(void) {
    GUEST_CALL(notZero_eByte200AD04);
    return !ZERO_FLAG();
}

static u32 resolve_pet_navi_status(void) {
    u32 navi = get_cur_pet_navi();
    if (navi != 0)
        return navi;
    if (pet_navi_event_set())
        return 0;
    if (pet_navi_byte_nonzero())
        return PET_NAVI_FALLBACK;
    return 0;
}

Next getPetNaviStatus_80101F8(void) {
    R0 = resolve_pet_navi_status();
    SET_FLAGS_ADD(R0, 0);
    RETURN_TO_CALLER();
}

/* ── battle_tickAiSelfDrain_8010230 ── */
/* battle_tickAiSelfDrain_8010230 — periodic self-damage tick for an AI battle object (r5 = object).
 *
 * While the battle is running (not time-stopped, not paused) and the object
 * still has more than 1 HP, a timer byte in its AI data (offset 9) counts up
 * once per call. When it reaches the interval looked up from byte_80102A4
 * via the object's navi stats byte 0x18, the object loses 1 HP and the timer
 * restarts at 0. An interval of 0 disables the behaviour and just keeps the
 * timer cleared.
 *
 * r4 and r6 come back unchanged; c/v are left as the last flag-setting
 * operation of the taken exit path left them (a callee's flags, or the CMP
 * that decided the early exit). */
Next battle_tickAiSelfDrain_8010230(void) {
    u32 savedR4 = R4;

    /* Skip entirely while time is stopped or the battle is paused. */
    GUEST_CALL(battle_isTimeStop); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG())
        RETURN_TO_CALLER();
    GUEST_CALL(battle_isPaused); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG())
        RETURN_TO_CALLER();

    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    /* Don't drain objects that are about to be destroyed anyway. */
    u32 hp = LD16(R5 + oBattleObject_HP);
    SET_FLAGS_CMP(hp, 1);
    if ((s32)hp <= 1)
        RETURN_TO_CALLER();

    /* How many ticks between self-damage hits for this character. */
    R1 = 0x18;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    u32 interval = LD8(A_byte_80102A4 + R0);

    if (interval == 0) {
        /* No self-damage configured: keep the timer cleared. */
        R0 = 0;
        ST8(aiData + 9, R0);
        RETURN_TO_CALLER();
    }

    /* Tick the timer; once it has counted up to the interval, take 1 HP of
     * damage and restart it. */
    u32 timer = LD8(aiData + 9) + 1;
    ST8(aiData + 9, timer);
    SET_FLAGS_CMP(timer, interval);
    if ((s32)timer < (s32)interval)
        RETURN_TO_CALLER();

    R0 = 1;
    R3 = timer;
    R4 = aiData;
    GUEST_CALL(object_subtractHP);
    R4 = savedR4;

    /* Restart the timer after a hit. */
    R0 = 0;
    ST8(aiData + 9, R0);
    RETURN_TO_CALLER();
}

/* ── SetAIDataUnk0x48Flag ── */
/* SetAIDataUnk0x48Flag(r0 = flag bits, r5 = battle object):
 * set the given bits in the object's AI data word at offset 0x48.
 * Leaves r1 = updated word and r3 = AI data pointer for the caller. */
Next SetAIDataUnk0x48Flag(void) {
    u32 *aiData = (u32 *)LD32(R5 + oBattleObject_AIDataPtr);
    u32 flags = LD32((u32)aiData + oAIData_Flags) | R0;
    ST32((u32)aiData + oAIData_Flags, flags);
    R1 = flags;
    R3 = (u32)aiData;
    RETURN_TO_CALLER();
}

/* ── battle_getCrossNaviTableByte_8010332 ── */
/* battle_getCrossNaviTableByte_8010332: pick a text/data entry for the current cross-over navi.
 * Returns 1 in certain battle modes, 4 if the navi slot is empty, otherwise a
 * byte from the table at byte_8020FE0: row = 11 * stats[0x29], column = stats[0x2B].
 * r4 is preserved. */
Next battle_getCrossNaviTableByte_8010332(void) {
    GUEST_CALL(GetBattleMode);
    if (R0 == 9) {          /* special battle mode: fixed result */
        R0 = 1;
        RETURN_TO_CALLER();
    }

    R1 = 0x29;              /* navi index/slot byte */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    if (R0 == 0) {          /* no navi registered */
        R0 = 4;
        RETURN_TO_CALLER();
    }

    u32 row_offset = 11 * R0;   /* each table row is 11 bytes */

    R1 = 0x2B;              /* variant byte selecting the entry within the row */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    R0 = LD8(A_byte_8020FE0 + row_offset + R0);
    RETURN_TO_CALLER();
}

/* ── battle_GetAllianceFactionFromBattleObject_80103A8 ── */
/* battle_GetAllianceFactionFromBattleObject_80103A8: check the battle object's "alliance" stat byte (index 0x31) via
 * GetBattleNaviStatsByte_AllianceFromBattleObject and report it as a boolean:
 * r0 = 3 when the byte is nonzero, r0 = 0 when it is zero. */
Next battle_GetAllianceFactionFromBattleObject_80103A8(void) {
    R1 = 0x31; /* stats byte to read */
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);

    R0 = (R0 != 0) ? 3 : 0;
    RETURN_TO_CALLER();
}

/* ── battle_preUpdateAndGetPlayerObject_80103EC ── */
/* battle_preUpdateAndGetPlayerObject_80103EC: run the standard pre-battle-update hook (battle_getStateByte0d_800A7E2), then
 * locate the player's battle object; result pointer in R0 (with c/v flags
 * from battle_findPlayer's tail). */
Next battle_preUpdateAndGetPlayerObject_80103EC(void) {
    GUEST_CALL(battle_getStateByte0d_800A7E2);        /* side effects / state refresh */
    GUEST_CALL(battle_findPlayer);  /* R0 = pointer to the player object */
    RETURN_TO_CALLER();
}

/* ── battle_aiAddTotalDamageTaken_8010548 ── */
/* battle_aiAddTotalDamageTaken_8010548(r4 = battle object, r0 = damage taken):
 * Add r0 to the battle object's TotalDamageTaken AI stat, clamping the
 * running total at 0xFFFF. Leaves r2 = 0xFFFF (the clamp constant). */
Next battle_aiAddTotalDamageTaken_8010548(void) {
    u32 damage = R0;
    u32 ai = LD32(R5 + oBattleObject_AIDataPtr);

    u32 total = LD16(ai + oAIData_TotalDamageTaken) + damage;
    SET_FLAGS_CMP(total, 0xFFFF);
    if ((s32)total > (s32)0xFFFF)
        total = 0xFFFF;

    ST16(ai + oAIData_TotalDamageTaken, total);
    R2 = 0xFFFF;
    RETURN_TO_CALLER();
}

/* ── battle_IsNoCustomPlusActive_80107C0 ── */
/* battle_IsNoCustomPlusActive_80107C0: check the battle effects for the "no Custom+" style flag
 * (GetBattleEffects bit 3). Returns 3 if that effect is active, 0 otherwise. */
Next battle_IsNoCustomPlusActive_80107C0(void) {
    GUEST_CALL(GetBattleEffects);
    if (R0 & 8)
        R3 = 3;
    else
        R3 = 0;
    RETURN_TO_CALLER();
}

/* ── battle_tickBattleObject_80107D4 ── */
/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet */
static bool battle_is_time_stopped(void) {
    GUEST_CALL(battle_isTimeStop);
    return !ZERO_FLAG();
}

/* register form: calls getEntryPtr_2036120_1D0_802E070 outside this file, which has no typed C interface yet */
static u32 get_alliance_entry(u8 alliance) {
    R0 = alliance;
    GUEST_CALL(getEntryPtr_2036120_1D0_802E070);
    return R0;
}

/* Decrement an 8-bit counter in place if it is currently nonzero. */
static void tick_down_u8(u32 addr) {
    u8 v = LD8(addr);
    if (v != 0) {
        ST8(addr, v - 1);
    }
}

/* Decrement a 16-bit counter in place if it is currently nonzero. */
static void tick_down_u16(u32 addr) {
    u16 v = LD16(addr);
    if (v != 0) {
        ST16(addr, v - 1);
    }
}

/* Per-frame battle-object housekeeping.
 *
 * If battle time is stopped, do nothing. Otherwise tick down the AI's chip
 * lockout and unknown-0x15 timers, then tick down the alliance entry's
 * counters at offsets 0x2E, 0x3A and 0x3C if the entry's byte 0 is nonzero.
 * The input r4 is preserved across the call (callee-saved). */
static u32 per_frame_ai_update(u32 orig_r4, u32 battle_object_ptr) {
    if (battle_is_time_stopped()) {
        return orig_r4;
    }

    u32 ai_data_ptr = LD32(battle_object_ptr + oBattleObject_AIDataPtr);
    tick_down_u8(ai_data_ptr + oAIData_ChipLockoutTimer);
    tick_down_u8(ai_data_ptr + oAIData_Unk_15);

    u8 alliance = LD8(battle_object_ptr + oBattleObject_Alliance);
    u32 entry = get_alliance_entry(alliance);

    if (LD8(entry) == 0) {
        return orig_r4;
    }

    tick_down_u16(entry + 0x2E);
    tick_down_u16(entry + 0x3A);
    tick_down_u16(entry + 0x3C);

    return orig_r4;
}

Next battle_tickBattleObject_80107D4(void) {
    R4 = per_frame_ai_update(R4, R5);
    RETURN_TO_CALLER();
}

/* ── stub_alwaysReturnsFalse_8010D20 ── */
/* stub_alwaysReturnsFalse_8010D20: stub that always returns 0 ("false") in r0.
 * r4 is saved and restored, so all registers except r0 are unchanged. */
Next stub_alwaysReturnsFalse_8010D20(void) {
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── dispatchHandlerById_8010D58 ── */
/* dispatchHandlerById_8010D58(r0 = handler id, r2/r3 = call arguments):
 * Dispatch table driver. Validates the id with sub_800F4A8; ids that pass
 * are handed to sub_800F4B2 and the call returns 0xFFFF, ids that fail are
 * dispatched to one of the five handlers in the table at off_8010D84.
 * An id of 0xFF is rejected outright with 0xFFFF.
 * On the dispatch path the handler is entered exactly as the original does:
 * r0 = handler address (thumb bit set), r1 = table base, r6 = id * 4.
 * Returns the handler's r0/r1 with r4 cleared; r6 is preserved. */
Next dispatchHandlerById_8010D58(void) {
    u32 savedR6 = R6;
    u32 id = R0;

    R6 = id << 2;

    if (id == 0xFF) {
        R0 = 0xFFFF;
    } else {
        GUEST_CALL(sub_800F4A8);
        if ((s32)R0 >= 1) {
            /* id accepted: record it and report "no handler ran" */
            R0 = 1;
            GUEST_CALL(sub_800F4B2);
            R0 = 0xFFFF;
        } else {
            /* call the handler selected by id, passing the table base in
             * r1 and its own address in r0, as the original's
             * `ldr r0,[r1,r6]; mov lr,pc; bx r0` sequence does (ids 0-4 are
             * sub_8010D98..sub_8010DC0; a larger id goes wherever the word
             * past the table points, as on the GBA) */
            u32 handler = LD32(A_off_8010D84 + id * 4);
            R1 = A_off_8010D84;
            R0 = handler;
            GUEST_CALL_ADDR(handler);
            R4 = 0;
        }
    }

    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── battle_objectNameIdToPopupHandler_8010DD0 ── */
/* battle_objectNameIdToPopupHandler_8010DD0(r5 = battle object):
 * Fetches the battle object's name ID and tail-calls battle_DispatchEnemyAiRoutine_8010DDA, which
 * continues the name-popup handling with it. Returns battle_DispatchEnemyAiRoutine_8010DDA's result
 * in r0 (r1 is scratch); flags are whatever battle_DispatchEnemyAiRoutine_8010DDA leaves. */
Next battle_objectNameIdToPopupHandler_8010DD0(void) {
    R0 = LD16(R5 + oBattleObject_NameID);
    GUEST_CALL(battle_DispatchEnemyAiRoutine_8010DDA);
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

/* ── nullsub_42 ── */
/* nullsub_42: empty stub (does nothing). */
Next nullsub_42(void) {
    RETURN_TO_CALLER();
}

/* ── battle_getObjectNameID_8011020 ── */
/* battle_getObjectNameID_8011020: forward the battle object's NameID (in R5) to battle_callActorActionHandler_801102A. */
Next battle_getObjectNameID_8011020(void) {
    R0 = LD16(R5 + oBattleObject_NameID);
    GUEST_CALL(battle_callActorActionHandler_801102A);
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

/* ── sub_8011268 ── */
/* Mode dispatcher (0x08011268): the caller passes a mode index in r0. The
 * handler for that mode is read from the jump table at off_801127C and called.
 * The original tail-calls the handler (its "return" pops sub_8011268's saved
 * lr and goes straight back to sub_8011268's caller), so the handler's results
 * in r4-r12 are this function's outputs and are already in the guest register
 * file when the call comes back — nothing needs to be copied through. */

/* register form: no wrapper covers the jump-table handlers (they are reached
 * only through this table), so the call is made through the handler's code
 * address with the entry state the original sets up:
 *   r0 = handler address (the table load overwrites the mode index),
 *   r1 = table base, r2 = 0. */
static void dispatch_mode(u32 mode) {
    u32 handler = LD32(A_off_801127C + (mode << 2));
    R0 = handler;
    R1 = A_off_801127C;
    R2 = 0;
    GUEST_CALL_ADDR(handler);
}

Next sub_8011268(void) {
    dispatch_mode(R0);
    RETURN_TO_CALLER();
}

/* ── sub_8011420 ── */
/* sub_8011420(r0 = value, r1 = value): dispatch on r0.
 * If r0 is zero, forward r1 to sub_8011268; otherwise forward r0
 * (with r0 = 2 as a selector) to sub_8010DF6. */
Next sub_8011420(void) {
    if (R0 == 0) {
        u32 value = R1;
        R0 = value;
        GUEST_CALL(sub_8011268);
    } else {
        u32 value = R0;
        R0 = 2;
        R1 = value;
        GUEST_CALL(sub_8010DF6);
    }
    RETURN_TO_CALLER();
}

/* ── sub_8011450 ── */
/* sub_8011450(r5 = battle object):
 * Runs the "actor finished" handler for this battle object. Reads the object's
 * AI data, looks up a handler for its AI index in a ROM jump table, and calls it
 * with r0 = the object's related object pointer. Objects with actor type 0
 * (no actor) return immediately. */
Next sub_8011450(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData + oAIData_ActorType);
    u32 aiIndex = LD8(aiData + oAIData_AIIndex);

    if (actorType == 0) {
        /* No actor attached: nothing to end. (r1/r2 still hold the values the
         * prologue loaded, matching the original.) */
        R1 = aiIndex;
        R2 = aiData;
        RETURN_TO_CALLER();
    }

    /* Fetch the handler address for this AI index from the ROM jump table
     * (off_8011470), so r1 holds the same value the original passes to the
     * callee, then select the handler to run. */
    R1 = LD32(A_off_8011470 + aiIndex * 4);
    R2 = aiData;
    R0 = LD32(R5 + oBattleObject_RelatedObject2Ptr);

    switch (aiIndex) {
    case 0: case 1: case 9: case 13: case 16: case 18: case 19:
        GUEST_CALL(battle_EndObjectCurrentAnim_80C44D2);
        break;

    case 14:
        GUEST_CALL(sub_80FF668);
        break;

    case 24:
        GUEST_CALL(sub_80C46B6);
        break;

    case 2: case 3: case 4: case 5: case 6: case 7: case 8:
    case 10: case 11: case 12: case 15: case 17:
    case 20: case 21: case 22: case 23:
        /* These AI indices map to a no-op handler. */
        GUEST_CALL(nullsub_43);
        break;

    default:
        /* AI index beyond the table: the original would jump through garbage.
         * Treat it like the no-op handler rather than crashing. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── nullsub_43 ── */
/* nullsub_43: empty stub, called by the game and immediately returns. */
Next nullsub_43(void) {
    RETURN_TO_CALLER();
}

/* ── BasicBattleObjectInitialize ── */
/* BasicBattleObjectInitialize(r0 = object, r1 = panel X, r2 = panel Y, r3 = element,
 *                             r5 = related object, r6 = damage/stamina-counter disabler):
 * fill in the basic fields of a battle object from the caller's arguments.
 * The object's alliance/flip setting is copied from the related object, and the
 * loaded value is returned in r1. */
Next BasicBattleObjectInitialize(void) {
    u32 object = R0;

    ST8(object + oBattleObject_PanelX, R1);
    ST8(object + oBattleObject_PanelY, R2);
    ST8(object + oBattleObject_Element, R3);
    ST32(object + oBattleObject_DamageAndStaminaDamageCounterDisabler, R6);

    R1 = LD16(R5 + oBattleObject_AllianceAndDirectionFlip);
    ST16(object + oBattleObject_AllianceAndDirectionFlip, R1);

    ST32(object + oBattleObject_RelatedObject1Ptr, R5);
    RETURN_TO_CALLER();
}

/* ── battle_isChipSpecialDarkInvisQualifying_8012ABC ── */
/* battle_isChipSpecialDarkInvisQualifying_8012ABC(r0 = battle object):
 * Returns 1 in r0 when the object's current chip qualifies for a special
 * condition. All of the following must hold:
 *   - the battle mode is not 1,
 *   - the navi stat byte 0x2C is 0x17 or 0x18,
 *   - the chip data has flag bit 1 set but bit 0 clear, and element 0xA.
 * The original saves/restores r6 and r7 around the body, so both registers
 * end unchanged; r2 is left at 2 if the check bailed before the flag tests
 * and at 1 afterwards, matching the original's scratch register usage. */
Next battle_isChipSpecialDarkInvisQualifying_8012ABC(void) {
    u32 result = 0;

    GUEST_CALL(GetBattleMode);
    if (R0 != 1) {
        R1 = 0x2C;
        GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
        s32 stat = (s32)R0;

        if (stat >= 0x17 && stat <= 0x18) {
            GUEST_CALL(getChip8021DA8);
            const u32 chip = R0;
            const u32 flags = LD8(chip + 9);

            R2 = 2;
            if ((flags & 2) != 0) {
                R2 = 1;
                /* bit 0 must be clear and the element must be 0xA */
                if ((flags & 1) == 0 && LD8(chip + 6) == 0xA)
                    result = 1;
            }
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── sub_8012AFA ── */
/* BattleNaviStats bytes read below.  Note the original feeds the byte read with
 * NAVI_STAT_NAVI back in as the handle of the NAVI_STAT_FORM lookup (r0 is never
 * reloaded between the two calls), so the port does the same. */
#define NAVI_STAT_NAVI 0x29
#define NAVI_STAT_FORM 0x2C

/* register form: calls GetBattleNaviStatsByte_AllianceFromBattleObject outside this file, which has no typed C interface yet */
static u32 navi_stats_byte(u32 battle_object, u32 stat) {
    R0 = battle_object;
    R1 = stat;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    return R0;
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data(u32 which_chip) {
    R0 = which_chip;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: the original runs its chip-trait tests as "tst r1,r2" and the
 * callers read r2 back afterwards, so the masks have to go into the real
 * register; on paths that never reach the tests r2 keeps the previous guest
 * call's leftover value, which no plain C return value can reproduce */
static void tst_mask_in_r2(u32 mask) {
    R2 = mask;
}

/* Is the navi's current chip a qualifying "power attack" chip with its trigger
 * armed?  Qualifies only for the special navi ids (7 / 0x13), a chip whose trait
 * byte (chip+9) has bit 0x02 set and bit 0x01 clear and whose kind (chip+4) is 4,
 * and - unless force_armed skips the check - an AI power attack that is in
 * mid-state (2) with its button held (1).  The trait-test masks (2, then 1)
 * stay behind in r2 exactly as in the original. */
static bool pwr_atk_chip_ready(u32 battle_object, u32 force_armed, u32 owner_object) {
    u32 navi = navi_stats_byte(battle_object, NAVI_STAT_NAVI);
    if (navi != 7) {
        u32 form = navi_stats_byte(navi, NAVI_STAT_FORM); /* fed the first byte, like the original */
        if (form != 7 && form != 0x13)   /* the two qualifying navi/form ids */
            return false;
    }

    u32 chip = get_chip_data(battle_object);
    u8 traits = LD8(chip + 9);           /* trait bits of the chip */
    tst_mask_in_r2(0x02);
    if (!(traits & 0x02))                /* trait bit 1 must be set ... */
        return false;
    tst_mask_in_r2(0x01);
    if (traits & 0x01)                   /* ... and trait bit 0 must be clear */
        return false;
    if (LD8(chip + 4) != 4)              /* only chip kind 4 qualifies */
        return false;

    if (force_armed != 0)
        return true;

    /* otherwise the power attack has to be armed on the owner's AI data */
    u32 ai_data = LD32(owner_object + oBattleObject_AIDataPtr);
    if (LD8(ai_data + oAIData_PwrAtkState) != 2)
        return false;
    return LD8(ai_data + oAIData_PwrAtkButton) == 1;
}

/* sub_8012AFA(r0 = battle object, r1 = force-armed flag, r5 = object owning the AI data) -> r0
 * (r2's trait-test mask is written inside via tst_mask_in_r2; r6/r7 are left alone) */
Next sub_8012AFA(void) {
    R0 = pwr_atk_chip_ready(R0, R1, R5);
    RETURN_TO_CALLER();
}

/* ── battle_chipMatchesHpFullRestoreAiBranch_8012B4E ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r6; } call_GetBattleNaviStatsByte_AllianceFromBattleObject_result;
/* register form: generated wrapper: GetBattleNaviStatsByte_AllianceFromBattleObject has no typed C interface yet */
static inline call_GetBattleNaviStatsByte_AllianceFromBattleObject_result call_GetBattleNaviStatsByte_AllianceFromBattleObject(u32 r1, u32 r5) {
    R1 = r1;
    R5 = r5;
    GUEST_CALL(GetBattleNaviStatsByte_AllianceFromBattleObject);
    return (call_GetBattleNaviStatsByte_AllianceFromBattleObject_result){ R0, R6 };
}

typedef struct { u32 r0; } call_getChip8021DA8_result_battle_chipMatchesHpFullRestoreAiBranch_8012B4E;
/* register form: generated wrapper: getChip8021DA8 has no typed C interface yet */
static inline call_getChip8021DA8_result_battle_chipMatchesHpFullRestoreAiBranch_8012B4E call_getChip8021DA8_battle_chipMatchesHpFullRestoreAiBranch_8012B4E(u32 r0) {
    R0 = r0;
    GUEST_CALL(getChip8021DA8);
    return (call_getChip8021DA8_result_battle_chipMatchesHpFullRestoreAiBranch_8012B4E){ R0 };
}

/* Result of the "HP full restore" AI branch test: the values the original
 * leaves in r0 (0 = no match, 1 = match), r2 and r6. */
typedef struct {
    u32 r0;
    u32 r2;
    u32 r6;
} ChipFullRestoreAiResult;

/* Decide whether the chip in slot chip_id qualifies for the HP-full-restore
 * AI branch.  battle_object is the acting battle object; skip_ai_state_check
 * (incoming r1) counts as a match without testing the AI's power-attack
 * state.  r6 is only passed through (the original saves/restores it around
 * the stat lookups), and r2 is left untouched on the early-out paths but
 * holds the tst scratch value (2, then 1) on the paths that inspect the
 * chip data. */
static ChipFullRestoreAiResult chip_matches_hp_full_restore_ai_branch(
    u32 chip_id, u32 skip_ai_state_check, u32 battle_object, u32 in_r2, u32 in_r6)
{
    ChipFullRestoreAiResult out;
    out.r6 = in_r6;

    /* The branch only applies for certain values of the navi stats byte,
     * read first at 0x29 and then at 0x2C. */
    u32 stats = call_GetBattleNaviStatsByte_AllianceFromBattleObject(0x29, battle_object).r0;
    if (stats != 6) {
        stats = call_GetBattleNaviStatsByte_AllianceFromBattleObject(0x2C, battle_object).r0;
        if (stats != 6 && stats != 0x12) {
            out.r0 = 0;
            out.r2 = in_r2;         /* early out: r2 never modified */
            return out;
        }
    }

    /* The chip itself must have the "regular attack" flag pair and kind 2. */
    u32 chip = call_getChip8021DA8_battle_chipMatchesHpFullRestoreAiBranch_8012B4E(chip_id).r0;
    u32 chip_flags = LD8(chip + 9);
    if ((chip_flags & 2) == 0) {    /* tst with #2 failed -> r2 still 2 */
        out.r0 = 0;
        out.r2 = 2;
        return out;
    }
    if ((chip_flags & 1) != 0) {    /* tst with #1 succeeded -> r2 now 1 */
        out.r0 = 0;
        out.r2 = 1;
        return out;
    }
    if (LD8(chip + 4) != 2) {
        out.r0 = 0;
        out.r2 = 1;
        return out;
    }

    /* A non-zero incoming r1 is a match without the AI state check. */
    if (skip_ai_state_check != 0) {
        out.r0 = 1;
        out.r2 = 1;
        return out;
    }

    /* Otherwise match only while the AI is holding a charged power attack
     * on the right button. */
    u32 ai = LD32(battle_object + oBattleObject_AIDataPtr);
    out.r2 = 1;
    if (LD8(ai + oAIData_PwrAtkState) != 2) {
        out.r0 = 0;
        return out;
    }
    if (LD8(ai + oAIData_PwrAtkButton) != 1) {
        out.r0 = 0;
        return out;
    }
    out.r0 = 1;
    return out;
}

Next battle_chipMatchesHpFullRestoreAiBranch_8012B4E(void) {
    ChipFullRestoreAiResult out =
        chip_matches_hp_full_restore_ai_branch(R0, R1, R5, R2, R6);
    R0 = out.r0;
    R2 = out.r2;
    R6 = out.r6;
    RETURN_TO_CALLER();
}
