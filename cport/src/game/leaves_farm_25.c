/*
 * leaves_farm_25.c: small functions from the model farm, set 25.
 *
 * Small functions converted one at a time by the model farm, each checked against
 * the lifted original before it was kept.
 */
#include "game.h"
#include "runtime.h"

/* ── countNaviFolderChipsWithElement_8120AE8 ── */
/* countNaviFolderChipsWithElement_8120AE8(r0 = element to match):
 * Counts how many chips in the current navi's regular folder have the given
 * element. Each navi has its own 30-entry (u16) folder inside the chip list
 * pointed to by oToolkit_S_Chip_2002178_Ptr; the navi's stat byte 0x2D picks
 * which 60-byte block. Returns the match count in r0.
 * r2/r3 are left holding whatever getChip8021DA8 left on its last call,
 * exactly as in the original; r4-r7 are preserved (pushed/popped). */
Next countNaviFolderChipsWithElement_8120AE8(void) {
    u32 wanted = R0;

    /* Folder base = chip list + (current navi's stat 0x2D) * 60 bytes. */
    GUEST_CALL(GetCurPETNavi);              /* r0 = current navi id */
    R1 = 0x2D;
    GUEST_CALL(GetCurPETNaviStatsByte);     /* r0 = navi's folder selector */
    u32 folder = LD32(R10 + oToolkit_S_Chip_2002178_Ptr) + R0 * 0x3C;

    u32 count = 0;
    for (u32 i = 0; i < 0x3C; i += 2) {
        R0 = LD16(folder + i);                  /* packed folder entry */
        GUEST_CALL(split9BitsFromBitfield_8021AE0); /* r0 = chip id */
        GUEST_CALL(getChip8021DA8);             /* r0 = &ChipData */
        if (LD8(R0 + 7) == wanted)              /* ChipData+7 = element */
            count++;
    }

    R0 = count;
    RETURN_TO_CALLER();
}

/* ── gfx_fillTilemapConsecutiveBanded_8120C94 ── */
/* gfx_fillTilemapConsecutiveBanded_8120C94(r0 = destination, r1 = width in halfwords, r2 = number of
 * 2-row bands, r3 = first value):
 *
 * Zero-fills the destination area (2*width halfwords per band), then fills
 * it with the consecutive halfword values r3, r3+1, r3+2, ... in row-major
 * order.  The fill is written one band at a time: each pass fills a row and
 * the row directly below it, alternating between the two so the values run
 * consecutively down each column pair, then skips ahead two rows.
 *
 * r4-r7 are preserved; r0-r3 and the flags are scratch. */
Next gfx_fillTilemapConsecutiveBanded_8120C94(void) {
    u32 dst = R0;
    u32 width = R1;   /* halfwords per row */
    u32 bands = R2;   /* number of 2-row bands to fill */
    u32 value = R3;   /* first index written */

    /* Clear the whole area first; the size is counted in halfwords
     * (2*width per band).  The fill loop below overwrites it all. */
    R1 = width * 2 * bands;
    GUEST_CALL(ZeroFillByHalfword);

    u32 rowBytes = width * 2;   /* byte width of one row */
    u32 p = dst;

    /* Note: like the original, the band loop body always runs at least
     * once, even when bands == 0 (it still fills one band). */
    u32 band = 0;
    do {
        /* Fill this row and the next one together. */
        u32 col = 0;
        u32 nextRow = rowBytes;   /* offset of the same column one row down */
        do {
            ST16(p + col, value);
            value++;
            ST16(p + nextRow, value);
            value++;
            col += 2;
            nextRow = rowBytes + col;
        } while ((s32)col < (s32)rowBytes);

        /* Advance past the two rows just filled. */
        p += nextRow;
        band++;
    } while ((s32)band < (s32)bands);

    SET_FLAGS_CMP(band, bands);
    RETURN_TO_CALLER();
}

/* ── battle_stateEnter_loadSprite0x80_80C4B48 ── */
/* battle_stateEnter_loadSprite0x80_80C4B48: state-entry handler for a battle object (r5 = object).
 *
 * Loads the object's sprite resources, selects and starts the animation for
 * this state, switches the object into the "update" phase, and then runs the
 * per-frame update hook (sub_80C4B84).
 *
 * The original pushes/pops r4 around the calls, so R4 must be preserved even
 * though callees clobber it. r5, r6 and r10 are left untouched. */

/* Phase value stored into oBattleObject_CurPhaseAndPhaseInitialized. */
#define CUR_STATE_UPDATE 4

Next battle_stateEnter_loadSprite0x80_80C4B48(void) {
    u32 saved_r4 = R4;

    /* Load sprite resources: sprite 0x80, animation 0x14, palette 0x16. */
    R0 = 0x80;
    R1 = 0x14;
    R2 = 0x16;
    GUEST_CALL(sprite_load);

    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);

    /* Set flag bit 1 in the object header flags (at offset 0). */
    ST8(R5, LD8(R5) | 2);

    /* Ask which animation this state starts with (result in r1) and apply it. */
    GUEST_CALL(sub_80C4C52);
    u32 anim = R1;
    ST8(R5 + oBattleObject_CurAnim, anim);
    ST8(R5 + oBattleObject_CurAnimCopy, anim);
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);

    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_update);

    /* Advance to the "update" phase of the current state action. */
    ST32(R5 + oBattleObject_CurPhaseAndPhaseInitialized, CUR_STATE_UPDATE);

    /* Run the per-frame update for this state; its results in r0-r3 are ours. */
    GUEST_CALL(sub_80C4B84);

    /* pop {r4, pc}: restore the caller's r4. */
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── battle_object_stepLifetimeDestroyOnHit_80C532E ── */
/* battle_object_stepLifetimeDestroyOnHit_80C532E(r5 = battle object):
 * Tear-down/step handler for a battle object that owns collision data.
 * Removes the object's collision data, spawns its "hit" effect, copies the
 * collision result flags into the object's extra var (if it keeps one), then
 * either lets the object live another tick (no collision flags and its
 * lifetime timer is still positive) or destroys it: clears its collision
 * region, frees the collision data block and frees the object itself.
 *
 * Exit state: r5/r4/r6/r8/r10 preserved, r7 = the object's collision data
 * pointer; r0-r3 and the flags are whatever the last helper call left. */
Next battle_object_stepLifetimeDestroyOnHit_80C532E(void) {
    u32 obj = R5;

    GUEST_CALL(object_removeCollisionData);
    GUEST_CALL(object_spawnCollisionEffect);

    u32 colData = LD32(obj + oBattleObject_CollisionDataPtr);
    u32 hitFlags = LD32(colData + oCollisionData_FlagsFromCollision);
    u32 flagsOut = LD32(obj + oBattleObject_ExtraVars + 0x10);
    R7 = colData;

    /* If the object records collision results, store just the flag bits it
     * cares about (bits 26-27 of the collision flags). */
    if (flagsOut != 0) {
        u32 masked = hitFlags & 0xC000000;
        ST32(flagsOut, masked);
        R2 = masked;
    }

    if (hitFlags != 0) {
        /* Collided with something: destroy the object this frame. */
        R0 = hitFlags;
        R1 = flagsOut;
    } else {
        /* Count the lifetime timer down; destroy it once it expires. */
        u32 timer = LD16(obj + oBattleObject_Timer) - 1;
        ST16(obj + oBattleObject_Timer, (u16)timer);
        R0 = timer;
        R1 = flagsOut;
        if ((s32)timer > 0) {
            /* Still alive: refresh the (now empty) collision data and return. */
            GUEST_CALL(object_presentCollisionData);
            RETURN_TO_CALLER();
        }
    }

    GUEST_CALL(object_clearCollisionRegion);
    R0 = LD32(obj + oBattleObject_CollisionDataPtr);
    GUEST_CALL(object_freeCollisionData);
    GUEST_CALL(object_freeMemory);
    RETURN_TO_CALLER();
}

/* ── battle_spawnTypeFObjectInitState_80C6548 ── */
/* battle_spawnTypeFObjectInitState_80C6548: spawn a battle object and fill in its starting state.
 *
 * The caller passes the new object's state in the registers:
 *   r0 = panel X, r1 = panel Y, r2 = element, r3 = extra spawn argument,
 *   r5 = object whose alliance byte the new object inherits,
 *   r6 = value for the damage/stamina counter disabler field.
 *
 * The registers are stashed, object_spawnType3 is called with r0 = 0xF
 * (r1-r3 still holding the caller's values), and it leaves the new object
 * pointer in r5 (NULL if the spawn failed). That pointer becomes the store
 * target for the field initialisation and is also what ends up in r0.
 * The stashed r0-r2 are restored shifted into r1-r3, r5 gets its entry
 * value back, and on the success path r1 is finally reloaded with the
 * source object's alliance byte on the way out. */
Next battle_spawnTypeFObjectInitState_80C6548(void) {
    u32 panelX    = R0;
    u32 panelY    = R1;
    u32 element   = R2;
    u32 srcObject = R5; /* alliance source; also what r5 is restored to */
    u32 damageSet = R6;

    R0 = 0xF;
    GUEST_CALL(object_spawnType3);

    u32 newObject = R5; /* spawn result as left by object_spawnType3 */

    R1 = panelX;
    R2 = panelY;
    R3 = element;
    R5 = srcObject;
    R0 = newObject;

    if (newObject != 0) {
        ST8(newObject + oBattleObject_PanelX, panelX);
        ST8(newObject + oBattleObject_PanelY, panelY);
        ST8(newObject + oBattleObject_Element, element);

        u32 alliance = LD8(srcObject + oBattleObject_Alliance);
        R1 = alliance;
        ST8(newObject + oBattleObject_Alliance, alliance);

        ST32(newObject + oBattleObject_DamageAndStaminaDamageCounterDisabler, damageSet);
    }

    RETURN_TO_CALLER();
}

/* ── aiAttack_stepStateMachine_80EB04C ── */
/* aiAttack_stepStateMachine_80EB04C: run one step of the AI attack-state machine for the object in R7.
 *
 * The state byte oAIAttackVars_Unk_00 is a byte offset into the handler table
 * at off_80EB074 (entries are 4 bytes apart, so it holds 0, 4, 8, 0xC or 0x10).
 * The selected handler is entered with r0 = state byte and r1 = its own table
 * entry (the original's `mov lr, pc / bx r1` dispatch), and may clobber the
 * flags and LR, which pass straight through to this function's caller.
 *
 * After the handler returns: if it latched "busy" into oAIAttackVars_Unk_30,
 * we're done. Otherwise we poll the D-pad; if the currently held direction
 * differs from the one remembered in oAIAttackVars_Unk_0c, store 1 into
 * oAIAttackVars_Unk_30.
 */
Next aiAttack_stepStateMachine_80EB04C(void) {
    u32 state = LD8(R7); /* oAIAttackVars_Unk_00 is at offset 0 */

    R0 = state;                          /* handler input: current state */
    R1 = LD32(A_off_80EB074 + state);    /* handler address from the table */
    switch (state) {
    case 0x00: GUEST_CALL(sub_80EB088); break;
    case 0x04: GUEST_CALL(sub_80EB128); break;
    case 0x08: GUEST_CALL(aiAttack_TimerTickEnd_80EB194); break;
    case 0x0C: GUEST_CALL(sub_80EB1C4); break;
    case 0x10: GUEST_CALL(sub_80EB1F8); break;
    default: break; /* unreachable: the state byte is always a table offset */
    }

    /* tst busy, busy: skip the direction poll if the handler flagged busy */
    u32 busy = LD32(R7 + oAIAttackVars_Unk_30);
    R0 = busy;
    SET_FLAGS_NZ(busy);
    if (busy != 0)
        RETURN_TO_CALLER();

    /* busy is 0 here, so r0 = 0 going into the button poll */
    GUEST_CALL(playerObject_checkDirectionButtons_800FA54);

    /* r1 ends up holding the remembered direction either way */
    R1 = LD8(R7 + oAIAttackVars_Unk_0c);
    SET_FLAGS_CMP(R0, R1);
    if (R0 != R1) {
        R0 = 1;                              /* mov r0, #1 also sets nz */
        ST32(R7 + oAIAttackVars_Unk_30, R0);
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── battle_getAiIndexFlag_80EBD68 ── */
/* battle_getAiIndexFlag_80EBD68(r5 = battle object):
 * For an AI-controlled actor (actor type == 2), return the per-AI-index byte
 * from the table at off_80EBD80 (custom AI slot flags); anything else returns 0.
 * Flags are left as the original leaves them: from CMP actorType,#2, with the
 * "mov r0,#0" path additionally forcing N=0/Z=1 (Thumb MOV-immediate sets flags). */
Next battle_getAiIndexFlag_80EBD68(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 actorType = LD8(aiData + 0);        /* oAIData_ActorType */

    /* CMP r1, #2 */
    SET_FLAGS_CMP(actorType, 2);

    if (actorType == 2) {
        u32 aiIndex = LD8(aiData + 1);      /* oAIData_AIIndex */
        R0 = LD8(A_off_80EBD80 + aiIndex);
        /* only ldrb's since the CMP: flags stay as CMP left them */
    } else {
        R0 = 0;
        /* mov r0, #0 in Thumb sets N and Z; C and V keep the CMP's values */
        SET_FLAGS_NZ(0);
    }
    RETURN_TO_CALLER();
}

/* ── aiAttack_genericSpawnAndCountdown_80EBF30 ── */
/* aiAttack_genericSpawnAndCountdown_80EBF30: per-tick update of an AI-driven attack (r4 = owning battle
 * object, r7 = oAIAttackVars state block). On the first tick (Unk_01 == 0)
 * it plays the attack animation, spawns the actual attack object from the
 * AI's attack data, flags the object and marks the attack as started; every
 * tick it then counts the Unk_10 timer down and, once it goes negative,
 * switches back to AI state 4 (Unk_00).
 *
 * Note: game.h has no macros for the oAIAttackVars fields at offsets 0/1,
 * nor for the object flag bit, so their numeric offsets are used here. */
Next aiAttack_genericSpawnAndCountdown_80EBF30(void) {
    u32 self = R4;   /* battle object running this attack */
    u32 state = R7;  /* oAIAttackVars block for this attack */

    if (LD8(state + 1) == 0) { /* oAIAttackVars_Unk_01: attack not started yet */
        /* First tick: start the attack. */
        R0 = 10; /* attack animation id */
        GUEST_CALL(object_setAnimation);
        /* object_setAnimation returns the object in r0, which
         * object_setDefaultCounterTime then takes as its argument. */
        GUEST_CALL(object_setDefaultCounterTime);

        /* Spawn the attack object: r4 = attack id 0xD,
         * r7 = attack parameters inside the AI data block. */
        R4 = 0xD;
        R7 = LD32(R5 + oBattleObject_AIDataPtr) + 0x68;
        GUEST_CALL(spawn_t1_0x5_tempAttackObject_80B8E30);
        R7 = state;

        R0 = 1u << 22; /* OBJECT_FLAGS_UNK_BIT_22_BIT */
        GUEST_CALL(object_setFlag1);

        ST8(state + 1, 4);   /* Unk_01 = 4: attack is now started */
        ST16(state + oAIAttackVars_Unk_10, 1); /* one tick of attack left */
    }

    /* Count the attack timer down; when it goes negative, return to state 4. */
    u32 timer = LD16(state + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(timer, 1); /* flags of timer - 1 */
    R0 = timer - 1;
    ST16(state + oAIAttackVars_Unk_10, R0);

    if ((s32)R0 < 0) {
        R0 = 4;
        ST16(state, R0); /* oAIAttackVars_Unk_00 = 4: back to AI state 4 */
        SET_FLAGS_NZ(R0); /* n/z from the new state, c/v from the subtract */
    }

    R4 = self;
    RETURN_TO_CALLER();
}

/* ── battle_aiWindRackAttackStep_80EBFF4 ── */
/* battle_aiWindRackAttackStep_80EBFF4(r5 = battle object, r7 = AI attack vars):
 * Wind-rack attack AI step. On the first run (attack state byte at r7+1 == 0)
 * it starts the attack animation (10), bumps the counter at
 * oAIData_Unk_68->0x10, sets the timer oAIAttackVars_Unk_10 to 10 and marks
 * the attack as started (state byte = 4). Every run then counts Unk_10 down;
 * while it is still >= 0 the attack continues. When it expires, the related
 * object reference is cleared, the pointer at oAIData_Unk_68 is reset to NULL
 * and the object leaves its attack state.
 * Flags/r0 on return: those of the timer decrement (SUBS) if the attack is
 * still running; otherwise whatever object_exitAttackState leaves. */
Next battle_aiWindRackAttackStep_80EBFF4(void) {
    if (LD8(R7 + 1 /* oAIAttackVars_Unk_01 */) == 0) {
        /* First tick of the attack: play animation 10 and init state. */
        u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
        u32 counterBlock = LD32(aiData + oAIData_Unk_68);

        R1 = LD8(counterBlock + 0x10) + 1;
        ST8(counterBlock + 0x10, R1);

        R0 = 10;
        GUEST_CALL(object_setAnimation);

        ST16(R7 + oAIAttackVars_Unk_10, 10);
        ST8(R7 + 1 /* oAIAttackVars_Unk_01 */, 4);
    }

    /* Count the attack timer down. */
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    u32 remaining = timer - 1;
    ST16(R7 + oAIAttackVars_Unk_10, remaining);
    SET_FLAGS_CMP(timer, 1);

    if ((s32)remaining >= 0) {
        /* Still active: keep going next frame. */
        R0 = remaining;
        RETURN_TO_CALLER();
    }

    /* Timer expired: detach and exit the attack state. */
    ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(aiData + oAIData_Unk_68, 0);

    R0 = 0;
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── battle_EndRelatedObject2Anim_80F06CE ── */
/* battle_EndRelatedObject2Anim_80F06CE(r4, r5 = battle object):
 * If the battle object has a secondary ("related object 2") sprite attached,
 * end its current animation. Flags reflect the animation-end call when it runs,
 * or the null-check test otherwise. */
Next battle_EndRelatedObject2Anim_80F06CE(void) {
    u32 related = LD32(R5 + oBattleObject_RelatedObject2Ptr);

    R0 = related;
    if (related != 0) {
        GUEST_CALL(battle_EndObjectCurrentAnim_80C44D2);
    } else {
        SET_FLAGS_NZ(related);
    }

    RETURN_TO_CALLER();
}

/* ── ai_attackStateStep_810A080 ── */
/* ai_attackStateStep_810A080: per-frame step of an AI opponent's attack state.
 * r4 = AI data block (owner), r6 = AI state struct, r7 = AI attack variables.
 *
 * Behavior:
 *  - On the first tick (state subfield at +2 == 0) the attack is initialized:
 *    the per-attack step handler (sub_810A21A) is installed in Unk_28, the
 *    attack id for this game version (byte_8109F46 indexed by version) is
 *    stored in Unk_18, object_setAttack0(10) starts the attack, and the
 *    substate counter is set to 4.
 *  - On later ticks the attack's own handler has already run; when its
 *    phase flag Unk_1a is clear the AI moves to state 12 (attack finished
 *    but something still pending), otherwise the whole AI state is reset
 *    to 0 so a new action can be chosen.
 * Leaves r0 = 4 (init path) or 0 (teardown paths) with nz reflecting it. */
Next ai_attackStateStep_810A080(void) {
    if (LD8(R6 + 2) == 0) {
        /* Begin the attack: install handler and version-specific attack id. */
        ST32(R7 + oAIAttackVars_Unk_28, A_sub_810A21A | 1);
        ST16(R7 + oAIAttackVars_Unk_18,
             LD8(LD8(R4 + oAIData_Version_16) + A_byte_8109F46));

        R0 = 0xA;
        GUEST_CALL(object_setAttack0);

        ST8(R6 + 2, 4);
        R0 = 4;
        SET_FLAGS_NZ(R0);
    } else if (LD8(R7 + oAIAttackVars_Unk_1a) == 0) {
        /* Attack phase not done yet: hand control to AI state 12. */
        ST8(R6 + 0, 0xC);
        ST16(R6 + 2, 0);
        R0 = 0;
        SET_FLAGS_NZ(R0);
    } else {
        /* Attack fully finished: clear the AI state. */
        ST8(R6 + 0, 0);
        ST16(R6 + 2, 0);
        R0 = 0;
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── t1_0x5e_pinkRingOnCounter_80C4B18 ── */
/* t1_0x5e_pinkRingOnCounter_80C4B18: tick function of the tutorial battle
 * object that draws the pink "counter hit" ring over MegaMan.
 *
 * Each tick it runs the handler for the object's current state (the handler
 * table holds one routine per state: init, update, free), and then animates
 * the object's sprite unless the battle is in a time-stop or is paused.
 * Omitting those two guards is what keeps the pink ring from showing up when
 * MegaMan lands a counter hit. */
enum {
    PINKRING_STATE_INIT = 0,
    PINKRING_STATE_RUN  = 4,
    PINKRING_STATE_FREE = 8,
};

/* Offset of the current-state byte in a battle object (oBattleObject_CurState). */
#define OBJ_CUR_STATE_OFFSET 8

Next t1_0x5e_pinkRingOnCounter_80C4B18(void) {
    u32 state = LD8(R5 + OBJ_CUR_STATE_OFFSET);

    /* Dispatch to the state handler; the handler sees R0 = state and
     * R1 = handler address, exactly as the original leaves them. */
    R0 = state;
    R1 = LD32(A_off_80C4B3C + state);
    switch (state) {
    case PINKRING_STATE_INIT: GUEST_CALL(battle_stateEnter_loadSprite0x80_80C4B48);       break;
    case PINKRING_STATE_RUN:  GUEST_CALL(sub_80C4B84);       break;
    case PINKRING_STATE_FREE: GUEST_CALL(object_freeMemory); break;
    }

    /* Freeze the sprite while time-stopped (e.g. during a counter hit). */
    GUEST_CALL(battle_isTimeStop);
    if (R0 != 0)
        RETURN_TO_CALLER();

    /* ...and while the battle is paused. */
    GUEST_CALL(battle_isPaused);
    if (R0 != 0)
        RETURN_TO_CALLER();

    GUEST_CALL(UpdateBattleObjectSprite);

    RETURN_TO_CALLER();
}

/* ── SpawnOWPlayerObject ── */
/*
 * Overworld player object pool.
 *
 * eOWPlayerObject holds a pool of fixed 0xC8-byte object slots; the u32 at
 * eActiveOWPlayerObjectBitfield tracks which slots are live, one bit per
 * slot, scanned from bit 31 (first slot) downwards.
 */
#define OW_PLAYER_SLOT_SIZE 0xC8

/* SpawnOWPlayerObject(r0..r4 = initialiser values for the new object):
 * Claims the first free slot in the overworld player object pool, stores the
 * caller's arguments into it and returns the slot address in r5 (r5 = 0 if
 * the pool is full). r0 = 0 on success; r1-r4 pass through unchanged on both
 * paths. Flags: on failure they come from the final CMP slot/poolEnd
 * (carry set, since the pool end was reached); on success N=0/Z=1 with
 * carry clear (the mask-building LSL and the in-loop CMP both shift/compare
 * out zero bits, and the free-slot TST clears N and sets Z). */
Next SpawnOWPlayerObject(void) {
    u32 arg0 = R0, arg1 = R1, arg2 = R2, arg3 = R3, arg4 = R4;

    u32 bitfieldAddr = A_eActiveOWPlayerObjectBitfield;
    u32 bits = LD32(bitfieldAddr);
    u32 mask = 0x80000000u;                          /* r1 = 0x80 << 24 */
    u32 slot = A_eOWPlayerObject;
    u32 poolEnd = A_eOWPlayerObject_SpriteData_Size; /* = eOWPlayerObjectEnd */

    /* The LSL that builds the mask shifts a zero bit out, clearing carry;
       it stays clear through the loop (each LSR shifts out a zero, and the
       in-loop CMP has slot < poolEnd, which also produces carry = 0). */
    cpu.c = 0;

    /* Scan the bitfield from the top bit for the first free (clear) slot. */
    while ((bits & mask) != 0) {
        mask >>= 1;
        slot += OW_PLAYER_SLOT_SIZE;
        SET_FLAGS_CMP(slot, poolEnd);
        if ((s32)slot >= (s32)poolEnd) {
            /* Pool exhausted: report failure in r5; the CMP above leaves
               the flags as the original does (MOV does not touch them). */
            R5 = 0;
            RETURN_TO_CALLER();
        }
    }

    /* Mark the slot as in use. */
    ST32(bitfieldAddr, bits | mask);

    /* Initialise the new object from the caller's arguments. */
    ST8(slot, 9);                               /* object kind: overworld player */
    ST8(slot + 1, arg0);
    ST32(slot + 4, arg4);
    ST32(slot + 8, 0);
    ST32(slot + 0x1C, arg1);
    ST32(slot + 0x20, arg2);
    ST32(slot + 0x24, arg3);

    R0 = 0;
    R5 = slot;
    SET_FLAGS_NZ(0);   /* same N/Z the free-bit TST leaves behind */
    RETURN_TO_CALLER();
}

/* ── StartBattle ── */
/* StartBattle (0x08005BC8): start a battle from the overworld.
 * r0 = pointer to the battle data for the fight, r1 = flag ("also refresh the
 * player's overworld state"), r10 = toolkit. Preserves r2, r4-r7 and r10.
 *
 * Stores the battle data pointer, snapshots the current map group so the
 * post-battle code can return to the right place, optionally updates the
 * player's game state, switches the state machine to the battle subsystem
 * (index 8) and plays the battle-start jingle. */
Next StartBattle(void) {
    u32 battleData = R0;
    u32 updatePlayerState = R1;

    /* r4-r7 are scratch in the original (pushed/popped), so their entry
     * values have to be restored before returning. */
    u32 savedR4 = R4;
    u32 savedR5 = R5;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 workArea = LD32(R10 + oToolkit_S2001c04_Ptr);

    ST32(gameState + oGameState_CurBattleDataPtr, battleData);

    /* Bit 0x4000 of the battle settings' effect mask: the battle data
     * pointer is also handed to the S2001c04 work area. */
    GUEST_CALL(GetBattleEffectsFromBattleSettings);
    u32 effects = R0;
    if (effects & 0x4000)
        ST32(workArea + oS2001c04_Unk_1c, battleData);

    /* Remember the map group the battle was started from
     * (oGameState_MapGroup is at offset 4 of the game state). */
    u32 mapGroup = LD16(gameState + 4);
    ST16(gameState + oGameState_LastMapGroup, mapGroup);

    if (updatePlayerState != 0)
        GUEST_CALL(updatePlayerGameState_800107A);

    /* Queue the battle init state machine as the next one; the subsystem
     * index is the first byte of the game state (offset 0). */
    ST8(gameState, 8);

    /* musicGameState_8000784 also looks at the game state pointer, the
     * work area pointer and the update flag (r5-r7); it is entered with
     * r0 = 8 like in the original. */
    R0 = 8;
    R5 = gameState;
    R6 = workArea;
    R7 = updatePlayerState;
    GUEST_CALL(musicGameState_8000784);

    R0 = SOUND_BATTLE_START;
    R5 = gameState;
    R6 = workArea;
    R7 = updatePlayerState;
    GUEST_CALL(PlaySoundEffect);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── anticheat_modCardsInit_8007036 ── */
/* anticheat_modCardsInit_8007036:
 * Builds the 256-byte table at byte_2000670 used by the card-modification
 * anticheat check. For every index (filled top-down from 0xFF to 0) a random
 * byte is rolled until it differs from 0x8D; the visible table stores
 * byte ^ 0x8D while the hidden shadow buffer (r10 ->
 * oToolkit_Unk2005038_Ptr) stores the bitwise complement, letting the game
 * cross-check the two copies later. */
Next anticheat_modCardsInit_8007036(void) {
    u32 shadowTable = LD32(R10 + oToolkit_Unk2005038_Ptr);
    u32 modCardsTable = A_byte_2000670;

    /* Caller registers that the RNG helper is free to clobber (r0-r3);
     * r4-r7 are kept in locals so the loop survives the call. */
    u32 saveR1 = R1, saveR2 = R2, saveR3 = R3;
    u32 saveR4 = R4, saveR5 = R5, saveR6 = R6, saveR7 = R7;

    s32 i;
    for (i = 0xFF; i >= 0; i--) {
        u8 entry;
        do {
            R7 = (u32)i;            /* the helper observes the current index */
            GUEST_CALL(GetRNG);
            entry = (u8)(R0 ^ 0x8D);
        } while (entry == 0);       /* reroll 0x8D: entry must be nonzero */

        ST8(modCardsTable + (u32)i, entry);
        ST8(shadowTable + (u32)i, (u8)~entry);
    }

    R1 = saveR1; R2 = saveR2; R3 = saveR3;
    R4 = saveR4; R5 = saveR5; R6 = saveR6; R7 = saveR7;
    RETURN_TO_CALLER();
}

/* ── encryption_bugfrags_8006fd0 ── */
/* encryption_bugfrags_8006fd0:
 * Anti-tamper check on the protected BugFrag counter. The counter keeps a
 * one's-complement shadow copy in the game state; if the two no longer agree,
 * the tamper flag (bit 1 of GameState_Unk_74) is set. The counter is then
 * XOR-obfuscated with a key and compared against its stored mirror, and the
 * result is reported in the flags only: Z set = count intact, Z clear =
 * count was modified. r0-r7 are preserved (the original pushes/pops them, so
 * the "mov r0,#1" on the mismatch path only exists to clear the Z flag).
 */
Next encryption_bugfrags_8006fd0(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);

    u32 bugfrags = LD32(gameState + oGameState_ProtectedBugfrags);

    /* Counter vs. its one's-complement shadow: refresh the tamper flag on mismatch. */
    u32 shadow = LD32(gameState + oGameState_Unk_70);
    if (bugfrags != ~shadow) {
        u32 stateFlags = LD32(gameState + oGameState_Unk_74);
        ST32(gameState + oGameState_Unk_74, stateFlags | 2);
    }

    /* Compare the XOR-obfuscated counter against its stored mirror. */
    u32 obfuscated = bugfrags ^ LD32(A_dword_20018B8);
    u32 mirror = LD32(LD32(R10 + oToolkit_Unk2005030_Ptr));

    SET_FLAGS_CMP(obfuscated, mirror);
    if (obfuscated != mirror)
        SET_FLAGS_NZ(1); /* mov r0,#1: clear Z, C/V stay from the compare above */

    RETURN_TO_CALLER();
}

/* ── encryption_zenny_8006f78 ── */
/* encryption_zenny_8006f78: zenny anti-tamper check.
 *
 * Validates the protected copy of the player's zenny two ways:
 *   1. oGameState_ProtectedZenny must be the bitwise complement of the
 *      shadow copy in oGameState_Unk_6c; otherwise bit 0 of
 *      oGameState_Unk_74 (cheat-detected flags) is set.
 *   2. oGameState_ProtectedZenny XOR the key in dword_2000060 must equal
 *      the checksum stored behind the pointer at
 *      toolkit + oToolkit_Unk2005028_Ptr.
 *
 * Leaves the Z flag set (from CMP r0, r1) when the keyed checksum still
 * matches, and clears it (via MOVS r0, #1) when it doesn't. r0-r7 are
 * pushed/popped by the original, so all of them come back unchanged. */
Next encryption_zenny_8006f78(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 zenny = LD32(gameState + oGameState_ProtectedZenny);

    if (zenny != ~LD32(gameState + oGameState_Unk_6c)) {
        u32 flags = LD32(gameState + oGameState_Unk_74);
        ST32(gameState + oGameState_Unk_74, flags | 1);
    }

    u32 keyedZenny = zenny ^ LD32(A_dword_2000060);
    /* pointer from the toolkit, then the checksum it points at (two loads) */
    u32 expected = LD32(LD32(R10 + oToolkit_Unk2005028_Ptr));

    if (keyedZenny == expected)
        SET_FLAGS_CMP(keyedZenny, expected);
    else
        SET_FLAGS_NZ(1);
    RETURN_TO_CALLER();
}

/* ── modifyBattleStateBasedOnAliveBattleActorsAndUnkCond_8007778 ── */
/* modifyBattleStateBasedOnAliveBattleActorsAndUnkCond_8007778
 * (r5 = battle object, r10 = toolkit):
 * Claim the first free slot in the object's alliance's "alive battle actors"
 * list in the battle state (4 slots per alliance).  On success, stores the
 * object pointer in that slot and, unless the object's AI data byte 2 is 1,
 * also increments the alive-actor counter kept right after the lists.
 * Returns 1 in r0 when a slot was claimed, 0 when all four slots were already
 * occupied.  r2/r3 are left as scratch values from the last examined slot,
 * and the c/v flags reflect the original's last flag-setting operation on
 * each path. */
Next modifyBattleStateBasedOnAliveBattleActorsAndUnkCond_8007778(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 alliance = LD8(R5 + oBattleObject_Alliance);

    /* Per-alliance block of 4 actor slots in the battle state. */
    u32 slotAddr = battleState + alliance * 0x10 + oBattleState_AliveBattleActors;

    u32 entry = 0;
    for (u32 slot = 0; slot < 4; slot++, slotAddr += 4) {
        entry = LD32(slotAddr);
        if (entry == 0) {
            /* Found a free slot: register this object in it. */
            ST32(slotAddr, R5);

            u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
            u32 unk02 = LD8(aiData + 2);
            if (unk02 != 1) {
                /* Count the object as alive for its alliance. */
                u32 counterAddr = battleState + alliance + 4;
                u32 oldCount = LD8(counterAddr);
                u32 count = oldCount + 1;
                ST8(counterAddr, count);
                R2 = count;
                SET_FLAGS_ADD(oldCount, 1);
            } else {
                /* Flagged objects skip the counter; flags stay from the
                 * comparison against 1. */
                R2 = unk02;
                SET_FLAGS_CMP(unk02, 1);
            }
            R0 = 1;
            R3 = slotAddr;
            RETURN_TO_CALLER();
        }
    }

    /* No free slot in this alliance's list. */
    R2 = entry;
    R3 = slotAddr;
    R0 = 0;
    SET_FLAGS_CMP(4, 4);
    RETURN_TO_CALLER();
}

/* ── object_freeMemory ── */
/* object_freeMemory(r5 = battle object):
 * Releases a battle object: clears its flags byte, frees its slot in the
 * per-type allocation bitmap (slots are handed out from the top bit down,
 * so slot n is bit 31-n), unlinks it from the doubly-linked list of active
 * objects, and calls sprite_makeUnscalable on the list node/header.
 *
 * On return, r0-r3 and the flags are whatever sprite_makeUnscalable left;
 * r5 is preserved. */

/* Field offsets into a battle object (not yet named in game.h). */
#define oBattleObject_Flags         0x00  /* byte: status flags           */
#define oBattleObject_Type          0x02  /* byte: low nibble = type id   */
#define oBattleObject_ListIndex     0x03  /* byte: slot in the bitmap     */
#define oBattleObject_LinkedList_Prev 0x00 /* node = object - LinkedList_Size */
#define oBattleObject_LinkedList_Next 0x04

Next object_freeMemory(void) {
    u32 obj = R5;

    /* the object is no longer in use */
    ST8(obj + oBattleObject_Flags, 0);

    /* free the object's slot in its type's allocation bitmap */
    u32 type = LD8(obj + oBattleObject_Type) & 0xF;
    u32 bitmap = LD32(A_ActiveObjectBitfieldPointers + type * 4);
    u32 listIndex = LD8(obj + oBattleObject_ListIndex);
    u32 slotBit = (listIndex < 32) ? (0x80000000u >> listIndex) : 0;
    ST32(bitmap, LD32(bitmap) & ~slotBit);

    /* unlink the object from the doubly-linked active object list; the
     * list node sits in the header just before the object proper */
    u32 node = obj - oBattleObject_LinkedList_Size;
    u32 prev = LD32(node + oBattleObject_LinkedList_Prev);
    u32 next = LD32(node + oBattleObject_LinkedList_Next);
    ST32(prev + oBattleObject_LinkedList_Next, next);
    ST32(next + oBattleObject_LinkedList_Prev, prev);

    /* clear the sprite scaling of the object's header; pass the same
     * register state the original does at the call */
    R0 = node;
    R1 = prev;
    R2 = next;
    R3 = slotBit;
    GUEST_CALL(sprite_makeUnscalable);

    RETURN_TO_CALLER();
}

/* ── mainState_resetGameAndGoto28_8005524 ── */
/* mainState_resetGameAndGoto28_8005524: tear down and re-initialize the current game mode.
 *
 * Frees every object of type 0x21, runs the standard init passes
 * (scene setup, work-area clear, scenario effect state, toolkit
 * pointer randomization, navi stat boost reload), then advances the
 * main jumptable to state 0x28. R10 holds the toolkit pointer on
 * entry; the jumptable index byte lives at offset 0 in the toolkit
 * (oToolkit_MainJumptableIndexPtr), and is reached via r7 = r10. */
Next mainState_resetGameAndGoto28_8005524(void) {
    R0 = 0x21; /* object type to free */
    GUEST_CALL(FreeAllObjectsOfSpecifiedTypes);

    GUEST_CALL(sub_8004702);
    GUEST_CALL(resetAllGraphicsState_8005F40);
    GUEST_CALL(runSetupStepAndClearWorkArea_8005F6C);
    GUEST_CALL(sub_80027C4);
    GUEST_CALL(zeroFill_80024A2);
    GUEST_CALL(scenario_initEffectState_8003962);
    GUEST_CALL(zeroFill_8003AB2);
    GUEST_CALL(RandomizeExtraToolkitPointers);
    GUEST_CALL(reloadCurNaviStatBoosts_813c3ac);

    /* Advance the main jumptable: r7 = r10 (toolkit), then store the
     * next state index 0x28 into *(&toolkit + oToolkit_MainJumptableIndexPtr).
     * That pointer field is at offset 0, so it is simply *toolkit. */
    R7 = R10;
    R0 = LD32(R7);
    R1 = 0x28;
    SET_FLAGS_NZ(R1); /* movs r1, #0x28 is the last flag-setting instruction */
    ST8(R0, R1);

    RETURN_TO_CALLER();
}

/* ── battle_stepSubstateToState4_8007C14 ── */
/* battle_stepSubstateToState4_8007C14: one step of the battle-state machine at R5.
 *
 * If the current state's handler (sub_801FE5E) reports it is still running,
 * the state is kept alive: on its first step it is initialized by
 * sub_803DDA4 and marked with sub-state 4; afterwards sub_803DE24 is polled
 * and, unless it reports completion for an object of kind 2 (byte at R4+1),
 * the state is torn down (sub_81440D8, sub_801FE64). Once the state ends
 * (or the handler already reported idle), control passes to battle state 4
 * by updating the state fields at R5.
 *
 * Preserves r4-r7; returns flags and r0 as the original does. */
Next battle_stepSubstateToState4_8007C14(void) {
    /* Is the current state's handler still busy? */
    GUEST_CALL(sub_801FE5E);
    if (R0 != 0) {
        if (LD8(R5 + 3) == 0) {
            /* First step in this state: run its init routine and mark it
             * as started with sub-state 4. */
            GUEST_CALL(sub_803DDA4);
            R0 = 4;
            ST8(R5 + 3, R0);
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }

        /* May the state finish now? */
        GUEST_CALL(sub_803DE24);
        if (R0 != 0) {
            R0 = LD8(R4 + 1);
            if (R0 == 2) {
                /* Kind-2 objects hold the state open: stop here. */
                SET_FLAGS_CMP(R0, 2);
                RETURN_TO_CALLER();
            }
        }

        /* State is over: clean up and notify the handler. */
        GUEST_CALL(sub_81440D8);
        GUEST_CALL(sub_801FE64);
    }

    /* Advance to battle state 4 and clear the pending sub-state fields. */
    R0 = 4;
    ST8(R5 + 1, R0);
    R0 = 0;
    ST16(R5 + 2, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_waitLinkSync_8009478 ── */
/* battle_waitLinkSync_8009478: link-battle sync wait handler for the current battle state
 * (r5). On first entry it resets the remote-ID tracker and starts a frame
 * timer. It then waits until both sides report the same tracked ID; once
 * they match it shuts the tracker down and signals readiness, otherwise it
 * counts frames and, after 180 frames (~3 s), forces the battle to move on
 * by switching the state index to 0xC.
 * Outputs: nz/c/v as left by the last flag-setting op; r0 = 4 on the
 * init/match paths, the incremented timer value (or 0xC on timeout) on
 * the wait path. */
Next battle_waitLinkSync_8009478(void) {
    u32 state = R5;

    if (LD8(state + 3) == 0) {
        /* First run: reset the tracked-ID state and start the wait timer. */
        GUEST_CALL(sub_800B428);
        ST16(state + oBattleState_Unk_28, 0);
        ST8(state + 3, 4);
        R0 = 4;
        SET_FLAGS_NZ(4);
        RETURN_TO_CALLER();
    }

    R0 = 0x1F2F3F4F;
    GUEST_CALL(battle_doBothTrackedIdsMatch_800B46C);

    if (R0 != 0) {
        /* Both sides agree: shut down the tracker and signal readiness. */
        GUEST_CALL(sub_800B460);
        GUEST_CALL(sub_800B444);
        ST16(state + 2, 4);
        R0 = 4;
        SET_FLAGS_NZ(4);
        RETURN_TO_CALLER();
    }

    /* Still waiting: count frames; r0 stays equal to the incremented
     * timer. After 180 frames, force progress by switching the battle
     * state to its next handler (state index lives at offset 0). */
    u32 timer = LD16(state + oBattleState_Unk_28) + 1;
    ST16(state + oBattleState_Unk_28, timer);
    R0 = timer;
    SET_FLAGS_CMP(timer, 0xB4);
    if ((s32)timer > 0xB4) {
        R0 = 0xC;
        ST32(state + 0, 0xC);
        SET_FLAGS_NZ(0xC);
    }
    RETURN_TO_CALLER();
}

/* ── battle_checkFlowAdvanceFlags_800A046 ── */
/* battle_checkFlowAdvanceFlags_800A046: decide whether the battle flow may advance this frame.
 * Returns 0xFF while the battle is over or time is stopped, otherwise
 * reports the state of the two flag halfwords at dword_2036820
 * (bit 3 of +4 wins over bit 3 of +0xC). Also leaves the symbol address
 * in R3 and the test mask (8) in R2 for its callers. */
Next battle_checkFlowAdvanceFlags_800A046(void) {
    u32 result = 0xFF; /* default: caller must wait */

    GUEST_CALL(battle_isBattleOver);
    if (R0 == 0) {
        GUEST_CALL(battle_isTimeStop);
        if (R0 == 0) {
            u32 flags = A_dword_2036820;

            R3 = flags;
            R2 = 8;

            if ((LD16(flags + 4) & 8) != 0)          /* dword_2036824 bit 3 */
                result = 0;
            else if ((LD16(flags + 0xC) & 8) != 0)   /* dword_203682C bit 3 */
                result = 1;
        }
    }

    R0 = result;
    RETURN_TO_CALLER();
}

/* ── battle_getPhaseMode_800A152 ── */
/* battle_getPhaseMode_800A152: pick the current battle-mode id from the battle state.
 * Returns 0 while time is stopped; otherwise one of 1/2/7 depending on
 * which phase flags are set in the battle state:
 *   - flag at +4 clear: 2, or 1 once the flag at +0x0d is set
 *   - flag at +4 set, flag at +5 clear: 1, or 2 once +0x0d is set
 *   - flag at +4 set, flag at +5 set:   7 if flag at +0x0b is set, else 0
 * Also leaves the last-read state byte in r1 and the +0x0d byte in r2
 * for the caller. Flags come from the last TST of that byte (or from
 * battle_isTimeStop on the time-stop path). */
Next battle_getPhaseMode_800A152(void) {
    GUEST_CALL(battle_isTimeStop); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = 0;
        RETURN_TO_CALLER();
    }

    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u8 unk0d = LD8(battle + oBattleState_Unk_0d);
    u8 unk04 = LD8(battle + 4); /* oBattleState_Unk_04 */

    R2 = unk0d;
    R1 = unk04;

    if (unk04 == 0) {
        R0 = (unk0d != 0) ? 1 : 2;
        SET_FLAGS_NZ(unk0d);
    } else {
        u8 unk05 = LD8(battle + 5); /* oBattleState_Unk_05 */
        R1 = unk05;
        if (unk05 == 0) {
            R0 = (unk0d != 0) ? 2 : 1;
            SET_FLAGS_NZ(unk0d);
        } else {
            R1 = LD8(battle + oBattleState_Unk_0b);
            R0 = (R1 != 0) ? 7 : 0;
            SET_FLAGS_NZ(R1);
        }
    }
    RETURN_TO_CALLER();
}

/* ── pet_getCurNaviStatusPair_800A506 ── */
/* pet_getCurNaviStatusPair_800A506: read the two status bytes of the navi currently in the PET.
 *
 * Each navi's save block is addressed through battle_getNaviStatsByte_800A540(navi, offset).
 * Byte 0x2D selects an entry, and the pair read here is the 16-bit value
 * stored at offsets 0x56 + 2*entry (low byte) and 0x57 + 2*entry (high byte).
 *
 * In battle mode 1 no navi data is available; both results come back as 0xFF.
 *
 * Returns r0 = low byte, r1 = high byte; flags reflect r1 (from the final
 * `mov r1, r0` of the original). */
Next pet_getCurNaviStatusPair_800A506(void) {
    GUEST_CALL(GetBattleMode);
    SET_FLAGS_CMP(R0, 1);
    if ((s32)R0 == 1) {
        R0 = 0xFF;
        R1 = 0xFF;
        SET_FLAGS_NZ(0xFF);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(GetCurPETNavi);
    u32 navi = R0;

    /* entry selector for this navi; the pair lives at 0x56 + 2*entry */
    R0 = navi;
    R1 = 0x2D;
    GUEST_CALL(battle_getNaviStatsByte_800A540);
    u32 pairOffset = R0 << 1;

    R0 = navi;
    R1 = 0x56 + pairOffset;
    GUEST_CALL(battle_getNaviStatsByte_800A540);
    u32 low = R0;   /* the original parks this on the stack while fetching the high byte */

    R0 = navi;
    R1 = 0x57 + pairOffset;
    GUEST_CALL(battle_getNaviStatsByte_800A540);
    R1 = R0;                        /* mov r1, r0 == adds r1, r0, #0: sets nz, clears c and v */
    SET_FLAGS_ADD(R0, 0);
    R0 = low;

    RETURN_TO_CALLER();
}

/* ── battle_updatePhaseState_800AE0C ── */
/* battle_updatePhaseState_800AE0C: advance the battle's phase state machine.
 *
 * - If the current phase (Unk_1c) has moved past the previously recorded one
 *   (Unk_1b), record the new phase and, once it reaches 2 or more, kick off
 *   the setup routine for that phase (sub_801E228) unless the current battle
 *   mode is 6, which handles its own setup.
 * - Then count the phase timer (Unk_1d) down; when it expires, reset the
 *   phase back to 0.
 */
Next battle_updatePhaseState_800AE0C(void) {
    u32 savedR4 = R4;
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u32 phase = LD8(battle + oBattleState_Unk_1c);
    u32 prevPhase = LD8(battle + oBattleState_Unk_1b);

    SET_FLAGS_CMP(phase, prevPhase);
    if ((s32)phase > (s32)prevPhase) {
        ST8(battle + oBattleState_Unk_1b, phase);
        SET_FLAGS_CMP(phase, 2);
        if ((s32)phase >= 2) {
            GUEST_CALL(GetBattleMode);
            SET_FLAGS_CMP(R0, 6);
            if (R0 != 6) {
                /* hand the new phase to the setup routine; it expects the
                 * battle state in r4 */
                R0 = phase - 2;
                R4 = battle;
                GUEST_CALL(sub_801E228);
                R4 = savedR4;
            }
        }
    }

    {
        u32 phaseTimer = LD8(battle + oBattleState_Unk_1d);
        SET_FLAGS_NZ(phaseTimer);
        if (phaseTimer != 0) {
            ST8(battle + oBattleState_Unk_1d, phaseTimer - 1);
        } else {
            ST8(battle + oBattleState_Unk_1c, 0);
        }
    }

    RETURN_TO_CALLER();
}

/* ── sub_800AF84 ── */
/* sub_800AF84: choose a battle-mode pair and hand it to sub_800AC20.
 *
 *   If battle effect bit 3 (0x8) is clear:
 *     mode = effect bit 0 (0 or 1), arg1 = 0, r3 = 1, r2 = raw effects value.
 *   If bit 3 is set:
 *     mode = 2, arg1 = battleState->oBattleState_Unk_0d with its low bit
 *     flipped, unless battle_getStateByteNibbles_800A832 returned 1.
 *
 * r4-r7 and r10 pass through untouched; everything else is whatever
 * sub_800AC20 leaves behind. */
Next sub_800AF84(void) {
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 8) {
        GUEST_CALL(battle_getStateByteNibbles_800A832);
        u32 nibble = R0;

        u32 statePtr = LD32(R10 + oToolkit_BattleStatePtr);
        u32 stateByte = LD8(statePtr + oBattleState_Unk_0d);

        if (nibble != 1) {
            R2 = 1;
            stateByte ^= R2;
        }
        R1 = stateByte;
        R0 = 2;
    } else {
        R0 = (effects & 1) != 0;
        R1 = 0;
        R2 = effects;
        R3 = 1;
    }

    GUEST_CALL(sub_800AC20);
    RETURN_TO_CALLER();
}

/* ── encryption_applyPackList_800B110 ── */
/* encryption_applyPackList_800B110: walk the halfword table at word_800B128 (pack indices,
 * 0-terminated) and re-apply each encryption pack through
 * encryption_applyPack_8006e70. Nothing else in the table region is touched.
 * Preserves r1-r7 like the original (r4 is pushed/popped there). */
Next encryption_applyPackList_800B110(void) {
    u32 savedR4 = R4;

    for (u32 entry = A_word_800B128;; entry += 2) {
        u16 packId = LD16(entry);
        if (packId == 0)
            break;
        R0 = packId;
        /* the original holds the table cursor in r4 across the call */
        R4 = entry;
        GUEST_CALL(encryption_applyPack_8006e70);
    }

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_backupStateForRestart_800B3A2 ── */
/* battle_backupStateForRestart_800B3A2: back up the battle state before a restart/continue.
 *
 * Copies the 0x50-word battle work buffer and the current Navi's stats
 * block (0x64 words) into their backup buffers, writes a "data valid"
 * marker word, snapshots a small 0x10-word block, and finally invokes
 * sub_80200A4(0x32) to finish the snapshot.
 *
 * Inputs:  r3 (unused), r10 = toolkit pointer, lr = return address.
 * Outputs: r2/r3 and the c/v flags as left by the final sub_80200A4 call. */
Next battle_backupStateForRestart_800B3A2(void) {
    /* Back up the 0x50-word battle work area. */
    R0 = A_byte_20366C0;
    R1 = A_dword_203CBE4;
    R2 = 0x50;
    GUEST_CALL(CopyWords);

    /* Mark the backup as valid. */
    ST32(A_dword_203CBE0, 0x56789123);

    /* Copy the current battle Navi's stats block (0x64 words) to its backup. */
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD8(battleState + 0xD);
    GUEST_CALL(GetBattleNaviStatsAddr); /* R0 = NaviStats of the active Navi */

    R1 = A_byte_203CC34;
    R2 = 0x64;
    GUEST_CALL(CopyWords);

    /* Back up the 0x10-word block (bag/inventory snapshot). */
    R0 = A_byte_203CED0;
    R1 = A_byte_203CC98;
    R2 = 0x10;
    GUEST_CALL(CopyWords);

    /* Run the snapshot post-processing step. */
    R0 = 0x32;
    GUEST_CALL(sub_80200A4);

    RETURN_TO_CALLER();
}

/* ── sub_800B79A ── */
/* sub_800B79A: dispatch the command id in r0 to one of five handlers.
 * Valid ids are 0x11E..0x122, mapping to table entries 0..4; anything else
 * is ignored. The selected handler is invoked with its own table entry in
 * r0 (the original does this by loading the pointer and doing `bx r0`),
 * and when it returns, control goes back to this function's caller.
 * Trashes r0, r1, r6, r7 and the flags; passes everything else through. */
Next sub_800B79A(void) {
    s32 id = (s32)R0;

    R1 = 0x122;
    if (id > 0x122)
        RETURN_TO_CALLER();

    R1 = 0x11E;
    if (id < 0x11E)
        RETURN_TO_CALLER();

    u32 index = (u32)(id - 0x11E);

    /* Handlers see their own table entry in r0, exactly as the original
     * leaves it after `ldr r0, [r1,r0]`. */
    R0 = LD32(A_off_800B7BC + index * 4);

    switch (index) {
    case 0: GUEST_CALL(sub_800B7D0); break;
    case 1: GUEST_CALL(sub_800B7E8); break;
    case 2: GUEST_CALL(sub_800B800); break;
    case 3: GUEST_CALL(nullsub_2);   break;
    case 4: GUEST_CALL(sub_800B80E); break;
    }

    RETURN_TO_CALLER();
}

/* ── ShuffleByteList_PrimaryRNG ── */
/* ShuffleByteList_PrimaryRNG(r0 = byte array, r1 = array length, r2 = swap count):
 * Shuffle the byte list by performing r2 random swaps. Each swap draws two
 * independent random numbers and reduces each to an array index with the BIOS
 * Div call (rand % length, remainder left in r1). r0-r2 come back unchanged,
 * r3 holds the first index of the last swap, and r4-r7 are preserved. */
Next ShuffleByteList_PrimaryRNG(void) {
    u32 array = R0;
    u32 count = R1;
    u32 swapsLeft = R2;

    do {
        /* First index: draw a random number, then divide it by the array
         * length with BIOS Div (svc 6); the remainder in r1 is the index. */
        R0 = array;
        R1 = count;
        R2 = swapsLeft;
        GUEST_CALL(GetPositiveSignedRNG);
        R1 = count;                 /* Div divisor (the RNG call may clobber r1) */
        swi_call(6);                /* Div: R0 = quotient, R1 = remainder */
        u32 first = R1;
        R3 = first;

        /* Second index, drawn the same way. R0 still holds the first
         * division's quotient here, exactly as in the original. */
        R2 = swapsLeft;
        GUEST_CALL(GetPositiveSignedRNG);
        R1 = count;
        swi_call(6);
        u32 second = R1;

        /* Swap the two selected bytes. */
        u8 firstByte = LD8(array + first);
        u8 secondByte = LD8(array + second);
        ST8(array + first, secondByte);
        ST8(array + second, firstByte);

        /* The original pops r0-r3 back from its stack around here; r1/r3 are
         * already correct, so restore the remaining two. */
        R0 = array;
        R2 = swapsLeft;
        R3 = first;

        /* Count this swap down. The flags come from this decrement (the
         * original's SUBS r2, #1); the loop ends once it reaches zero. */
        SET_FLAGS_CMP(swapsLeft, 1);
        swapsLeft--;
    } while (swapsLeft != 0);

    R0 = array;
    R1 = count;
    R2 = 0;                         /* the final decrement left r2 at zero */
    RETURN_TO_CALLER();
}

/* ── zeroFillVRAM ── */
/* zeroFillVRAM: clear all of VRAM by zeroing it in four chunks.
 * BG modes use 0x6000000-0x600BFFF (0x40 units each), and the last
 * chunk 0x600C000-0x600DFFF (0x20 units). Leaves r1 = 0x20. */
Next zeroFillVRAM(void) {
    struct { u32 addr; u32 size; } chunks[4] = {
        { A_vram_6000000, 0x40 },
        { 0x6004000,      0x40 },
        { 0x6008000,      0x40 },
        { 0x600C000,      0x20 },
    };

    for (int i = 0; i < 4; i++) {
        R0 = chunks[i].addr;
        R1 = chunks[i].size;
        GUEST_CALL(ZeroFillByEightWords);
    }

    R0 = chunks[3].addr;
    R1 = chunks[3].size;
    RETURN_TO_CALLER();
}

/* ── initRefs_buildAndDecompressTable_8125280 ── */
/* initRefs_buildAndDecompressTable_8125280: assemble the "init references" table in RAM at unk_2029000 from
 * the ROM pointer table at off_8125310, then decompress-and-copy the two data
 * blobs that follow it.
 *
 * The source table holds 5 entries of 4 words each (0x50 bytes total):
 *   word 0..1 - passed to sub_811FB84, whose result is stored instead
 *   word 2..3 - copied verbatim
 * A final 0 word terminates the assembled table. r4-r7 are preserved. */
Next initRefs_buildAndDecompressTable_8125280(void) {
    u32 src = A_off_8125310;  /* init-ref entries in ROM */
    u32 dst = A_unk_2029000;  /* assembled table in RAM */

    for (u32 entry = 0; entry < 0x50 / 16; entry++) {
        R0 = LD32(src);
        src += 4;
        R1 = LD32(src);
        src += 4;
        GUEST_CALL(sub_811FB84);
        ST32(dst, R0);
        dst += 4;

        for (u32 word = 0; word < 2; word++) {
            ST32(dst, LD32(src));
            dst += 4;
            src += 4;
        }
    }
    ST32(dst, 0);

    /* decompress the assembled table, then the blob described by it */
    R0 = A_unk_2029000;
    GUEST_CALL(decompAndCopyData);
    R0 = A_byte_8125368;
    GUEST_CALL(decompAndCopyData);

    RETURN_TO_CALLER();
}

/* ── text_decodeCharTiles8_801263D0_81263D0 ── */
/* text_decodeCharTiles8_801263D0_81263D0(r0 = source bytes, r2/r3 = destination halfword buffers,
 * r6 = base tile index): decode a run of character codes into tile map
 * entries. Each byte below 0xE5 becomes a pair of tiles (base + code*2
 * and base + code*2 + 1) written as halfwords to r2/r3, which advance
 * two bytes per character. The run ends at the 0xE5 terminator (or when
 * 8 entries have been produced); remaining slots up to 8 are zero-filled.
 * r0/r1 are scratch; r4-r7 are preserved. */
Next text_decodeCharTiles8_801263D0_81263D0(void) {
    u32 src = R0;
    u32 baseTile = R6;
    u32 dstLo = R2;
    u32 dstHi = R3;
    u32 count = 0;

    /* Decode characters until the 0xE5 terminator. */
    for (;;) {
        u32 code = LD8(src + count);
        if ((s32)code >= 0xE5)
            break;

        u32 tile = baseTile + (code << 1);
        ST16(dstLo, tile);
        ST16(dstHi, tile + 1);
        dstLo += 2;
        dstHi += 2;
        count++;
    }

    /* Zero-fill up to the fixed width of 8 entries. */
    while (count < 8) {
        ST16(dstLo, 0);
        ST16(dstHi, 0);
        dstLo += 2;
        dstHi += 2;
        count++;
    }

    R2 = dstLo;
    R3 = dstHi;
    RETURN_TO_CALLER();
}

/* ── npc_checkEntryEventFlagOrHidden_812818C ── */
/* npc_checkEntryEventFlagOrHidden_812818C(r2 = default result, r3 = addend for the table index, r5 = object ptr):
 * Reads the halfword at object+0x24, adds r3, and uses the sum as an index into
 * the 0x20-byte-stride table at dword_2027590.  The upper halfword of the entry's
 * word at +0x18, plus 0x1D20, forms an event flag id.  If that event flag is set,
 * r2 keeps its incoming value; if it is clear, r2 = 0x8000.
 * r0, r1, r3, r4, r5, r10 and lr are left unchanged (r0/r1/r3/r4 via the
 * original's push/pop, the rest because they are simply never written). */
Next npc_checkEntryEventFlagOrHidden_812818C(void) {
    u32 object = R5;
    u32 tableEntry = A_dword_2027590 + ((u32)LD16(object + 0x24) + R3) * 0x20;
    u32 flagId = (LD32(tableEntry + 0x18) >> 16) + 0x1D20;

    /* registers live across the TestEventFlag call must survive it */
    u32 inR0 = R0, inR1 = R1, inR3 = R3, inR4 = R4;
    u32 result = R2;

    R0 = flagId;
    GUEST_CALL(TestEventFlag); /* result: Z set if the flag is clear (r0 is the flag's whole byte) */
    if (cpu.nz == 0)
        result = 0x8000;

    R0 = inR0;
    R1 = inR1;
    R2 = result;
    R3 = inR3;
    R4 = inR4;
    R5 = object;
    RETURN_TO_CALLER();
}

/* ── decomp_buildInitRefTable_812AF3C ── */
/* decomp_buildInitRefTable_812AF3C: build the init-reference table at unk_201D020 from the template
 * at off_812B024, then decompress-and-copy the data it describes.
 *
 * The source table is 0x60 bytes, i.e. six groups of four words. In each group
 * the first two words form one reference that sub_811FB84 resolves to a single
 * pointer-sized entry; the remaining two words are copied through unchanged.
 * The rebuilt table (24 words) is terminated with a 0 word and handed to
 * decompAndCopyData, which expands the referenced data into place. */
Next decomp_buildInitRefTable_812AF3C(void) {
    u32 dest = A_unk_201D020;
    u32 src  = A_off_812B024;
    u32 out  = 0;   /* write offset into the rebuilt table */

    for (u32 in = 0; in < 0x60; in += 16) {
        /* resolve the two-word reference that opens each group */
        R0 = LD32(src + in);
        R1 = LD32(src + in + 4);
        GUEST_CALL(sub_811FB84);
        ST32(dest + out, R0);
        out += 4;

        /* copy the two plain words that follow it */
        ST32(dest + out, LD32(src + in + 8));
        out += 4;
        ST32(dest + out, LD32(src + in + 12));
        out += 4;
    }

    /* NUL-terminate the reference table */
    ST32(dest + out, 0);

    R0 = dest;
    GUEST_CALL(decompAndCopyData);
    RETURN_TO_CALLER();
}

/* ── CutsceneCamera_GetAutoScrollDeltaForCoord ── */
/* CutsceneCamera_GetAutoScrollDeltaForCoord(r3 = signed coordinate delta):
 * Computes the per-frame auto-scroll step for one axis of the camera.
 * The magnitude is divided by 16, clamped to at most 0x00100000, and if the
 * result falls below 0x800 the original (unshifted) magnitude is used instead
 * so small deltas still move by their full amount. The sign is preserved.
 * Outputs: r3 = clamped delta, r4 = 0x800 (the minimum step threshold).
 * Clobbers r0 and the flags; neither is observed by callers. */
Next CutsceneCamera_GetAutoScrollDeltaForCoord(void) {
    const u32 MAX_STEP = 0x00100000;    /* 0x10 << 16: 1.0 in 12.20 fixed point */
    const u32 MIN_STEP = MAX_STEP >> 9; /* 0x800 */

    u32 delta = R3;
    u32 negative = (s32)delta < 0;
    u32 magnitude = negative ? 0 - delta : delta;

    u32 step = magnitude >> 4;
    if ((s32)step > (s32)MAX_STEP)
        step = MAX_STEP;
    if ((s32)step < (s32)MIN_STEP)
        step = magnitude; /* below the floor: fall back to the raw magnitude */

    R3 = negative ? 0 - step : step;
    R4 = MIN_STEP;
    RETURN_TO_CALLER();
}

/* ── CutsceneCmd_run_or_stop_cutscene_camera_script ── */
/* CutsceneCmd_run_or_stop_cutscene_camera_script (0x0803816A): map-script
 * command handler for starting/stopping a cutscene camera script.
 *
 * r5 = cutscene state block, r7 = current read position in the map script.
 *
 * Command layout: a flag byte, followed -- only when the flag is 0 -- by a
 * word selecting the camera script to play:
 *   flag == 0: run the camera script named by the following word
 *   flag != 0: stop the running camera script and recenter on the player
 *
 * Always returns r0 = 1 (keep the script interpreter running) and advances
 * r7 past the consumed operands. */
Next CutsceneCmd_run_or_stop_cutscene_camera_script(void) {
    R6 = 1;                            /* operand size 1: flag byte */
    GUEST_CALL(ReadMapScriptByte);     /* flag byte -> R4 */

    if (R4 == 0) {
        /* Run: the next word is the camera script to play. */
        R6 = 2;                        /* operand size 2: script word */
        GUEST_CALL(ReadMapScriptWord); /* script word -> R4 */

        ST32(R5 + oCutsceneState_CutsceneCameraScriptPtr, R4);
        R0 = R4;
        GUEST_CALL(SetCutsceneCameraScript);
        ST8(R5 + oCutsceneState_CutsceneCameraScriptActive, 1);

        R7 += 6;                       /* skip flag byte + script word */
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        /* Stop: drop the active camera script and recenter the camera. */
        R0 = 0;
        ST32(R5 + oCutsceneState_CutsceneCameraScriptPtr, R0);
        GUEST_CALL(cutsceneCamera_focusCameraOnPlayerMaybe_8036faa);
        ST8(R5 + oCutsceneState_CutsceneCameraScriptActive, 0);

        R7 += 2;                       /* skip flag byte */
        R0 = 1;
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}
