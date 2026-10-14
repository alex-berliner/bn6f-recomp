/*
 * asm00_2_801B9E6.c: readable functions of bn6f asm00_2.s, from 0x0801B9E6, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── RunAIAttack ── */
/* RunAIAttack — run one step of the extra AI attack logic for the battle
 * object in R5.
 *
 * Control flow:
 *   - CurAction 0x00-0x0F: dispatch through the handler table passed in
 *     R0 (the caller's own attack table), indexed by CurAction.
 *   - CurAction >= 0x10: dispatch through the shared AIAttackJumptable,
 *     indexed by CurAction - 0x10 — unless attack variable 0x1d is 1,
 *     in which case sub_80EAD9C runs instead of a table handler (it is
 *     still given the table address in r0 and the index in r1).
 *
 * The selected handler is entered with r0 = its own address,
 * r1 = the table byte offset, r2 = attack variable 0x1d (only when
 * CurAction >= 0x10), and r4/r6/r7 pointing at the object's AI data /
 * AI state / attack variables. It returns straight back to
 * RunAIAttack's caller; r4, r6 and r7 are restored to the caller's
 * values first. r1-r3, r5 and r8-r12 pass through to the handler
 * unchanged.
 */
Next RunAIAttack(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 aiState = aiData + oAIData_AIState;
    u32 attackVars = aiData + oAIData_AttackVars;
    u32 callerR4 = R4, callerR6 = R6, callerR7 = R7;
    u32 curAction = LD8(R5 + 9 /* oBattleObject_CurAction */);
    u32 tableIndex;
    u32 handler;

    if (curAction < 0x10) {
        /* caller-supplied handler table */
        tableIndex = curAction;
        handler = LD32(R0 + tableIndex * 4);
    } else {
        tableIndex = curAction - 0x10;
        u32 attackVar1d = LD8(attackVars + oAIAttackVars_Unk_1d);

        if (attackVar1d == 1) {
            /* special case: dedicated routine instead of a table handler;
             * it still sees the table address, the index and the variable */
            R0 = A_AIAttackJumptable;
            R1 = tableIndex;
            R2 = attackVar1d;
            R4 = aiData;
            R6 = aiState;
            R7 = attackVars;
            GUEST_CALL(sub_80EAD9C);
            R4 = callerR4;
            R6 = callerR6;
            R7 = callerR7;
            RETURN_TO_CALLER();
        }
        handler = LD32(A_AIAttackJumptable + tableIndex * 4);
        R2 = attackVar1d;
    }

    /* Dispatch to the chosen handler. The `lsl r1, r1, #2` that forms the
     * table offset sets nz from the offset and its carry out (bit 30 of
     * the index — always 0 here, since the index fits in a byte). */
    R0 = handler;
    R1 = tableIndex * 4;
    R4 = aiData;
    R6 = aiState;
    R7 = attackVars;
    SET_FLAGS_NZ(R1);
    cpu.c = (tableIndex >> 30) & 1;
    GUEST_CALL_ADDR(handler);
    R4 = callerR4;
    R6 = callerR6;
    R7 = callerR7;
    RETURN_TO_CALLER();
}

/* ── applyDamageToPlayer_801ba12 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4; } call_battle_aiAddTotalDamageTaken_8010548_result;
/* register form: generated wrapper: battle_aiAddTotalDamageTaken_8010548 has no typed C interface yet */
static inline call_battle_aiAddTotalDamageTaken_8010548_result call_battle_aiAddTotalDamageTaken_8010548(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battle_aiAddTotalDamageTaken_8010548);
    return (call_battle_aiAddTotalDamageTaken_8010548_result){ R4 };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag_applyDamageToPlayer_801ba12(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

typedef struct { u32 r1, r4; bool c, v; } call_object_subtractHP_result;
/* register form: generated wrapper: object_subtractHP has no typed C interface yet */
static inline call_object_subtractHP_result call_object_subtractHP(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_subtractHP);
    return (call_object_subtractHP_result){ R1, R4, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2; } call_unpackVerActorTyAndAIIdx_800F29C_result;
/* register form: generated wrapper: unpackVerActorTyAndAIIdx_800F29C has no typed C interface yet */
static inline call_unpackVerActorTyAndAIIdx_800F29C_result call_unpackVerActorTyAndAIIdx_800F29C(u32 r0) {
    R0 = r0;
    GUEST_CALL(unpackVerActorTyAndAIIdx_800F29C);
    return (call_unpackVerActorTyAndAIIdx_800F29C_result){ R0, R1, R2 };
}

typedef struct { u32 r1, r2; bool z, n; } call_battle_networkInvert_result;
/* register form: generated wrapper: battle_networkInvert has no typed C interface yet */
static inline call_battle_networkInvert_result call_battle_networkInvert(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_networkInvert);
    return (call_battle_networkInvert_result){ R1, R2, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_applyDamageToPlayer_801ba12;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_applyDamageToPlayer_801ba12 call_PlaySoundEffect_applyDamageToPlayer_801ba12(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_applyDamageToPlayer_801ba12){ R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r2, r3; } call_sprite_forceWhitePalette_result;
/* register form: generated wrapper: sprite_forceWhitePalette has no typed C interface yet */
static inline call_sprite_forceWhitePalette_result call_sprite_forceWhitePalette(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_forceWhitePalette);
    return (call_sprite_forceWhitePalette_result){ R2, R3 };
}

typedef struct { u32 r0, r6, r7; } call_battle_isAllianceActiveAndAiFlag0x4000_802DD2A_result;
/* register form: generated wrapper: battle_isAllianceActiveAndAiFlag0x4000_802DD2A has no typed C interface yet */
static inline call_battle_isAllianceActiveAndAiFlag0x4000_802DD2A_result call_battle_isAllianceActiveAndAiFlag0x4000_802DD2A(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_isAllianceActiveAndAiFlag0x4000_802DD2A);
    return (call_battle_isAllianceActiveAndAiFlag0x4000_802DD2A_result){ R0, R6, R7 };
}

/* register form: generated wrapper: SetAIData_Unk_44_Flag has no typed C interface yet */
static inline void call_SetAIData_Unk_44_Flag(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(SetAIData_Unk_44_Flag);
}

typedef struct { u32 r2; } call_object_setFlag2_result;
/* register form: generated wrapper: object_setFlag2 has no typed C interface yet */
static inline call_object_setFlag2_result call_object_setFlag2(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag2);
    return (call_object_setFlag2_result){ R2 };
}

typedef struct { u32 r2, r3, r5, r6; } call_sub_801A200_result;
/* register form: generated wrapper: battleObject_onCollision_801A200 has no typed C interface yet */
static inline call_sub_801A200_result call_sub_801A200(u32 r5) {
    R5 = r5;
    GUEST_CALL(battleObject_onCollision_801A200);
    return (call_sub_801A200_result){ R2, R3, R5, R6 };
}

/* Apply pending collision damage to the player object: record in AI tally, check
 * the undershirt (shield) flag, subtract HP, play the hurt SFX (or its dark-network
 * variant), flash the sprite white if still alive, then always subtract the collision
 * HP-bar value and handle death flags. */
static void apply_damage_to_player(u32 player, u32 collision, u32 r10_val) {
    u16 damage = LD16(collision + oCollisionData_FinalDamage);

    if (damage != 0) {
        /* Add this hit to the AI's running damage tally. */
        call_battle_aiAddTotalDamageTaken_8010548(damage, player);

        /* Undershirt / navi-shield protection: if the object has the
         * shield flag (bit 0x40000) and the incoming damage would
         * kill it outright, clamp damage so HP stays at 1. */
        u16 hp = LD16(player + oBattleObject_HP);
        if ((s32)hp > 1) {
            u32 obj_flags = call_object_getFlag_applyDamageToPlayer_801ba12(player);
            if ((obj_flags & 0x40000) != 0) {
                hp = LD16(player + oBattleObject_HP);
                if ((s32)hp <= (s32)damage) {
                    damage = hp - 1;
                }
            }
        }

        /* Subtract the (possibly clamped) damage from the player object. */
        call_object_subtractHP_result subtract_res =
            call_object_subtractHP(damage, player);
        u32 hp_remaining = subtract_res.r1;

        /* Pick the hurt sound: default 0x6D, or 0x6B for a dark-network navi. */
        u16 name_id = LD16(player + oBattleObject_NameID);
        call_unpackVerActorTyAndAIIdx_800F29C_result unpack_res =
            call_unpackVerActorTyAndAIIdx_800F29C(name_id);

        if (unpack_res.r0 == 2) {
            u8 alliance = LD8(player + oBattleObject_Alliance);
            call_battle_networkInvert_result invert_res =
                call_battle_networkInvert(alliance);
            if (invert_res.z) {
                /* Network result is zero → dark-network hurt sound. */
                unpack_res.r0 = 0x6B;
            } else {
                unpack_res.r0 = 0x6D;
            }
        } else {
            unpack_res.r0 = 0x6D;
        }
        u32 sfx_id = unpack_res.r0;

        /* register form: r2 for PlaySoundEffect comes from the previous
         * unpack/invert calls as a register side-effect of the original. */
        R2 = unpack_res.r2;
        call_PlaySoundEffect_applyDamageToPlayer_801ba12(sfx_id, unpack_res.r2);

        /* r10_val is used only by the trailing battleObject_onCollision_801A200 wrapper below. */
        (void)r10_val;

        if (hp_remaining != 0) {
            /* Flash the sprite white, then fall through to the
             * collision-HP-bar subtraction. */
            call_sprite_forceWhitePalette(player);
            u16 col_val = LD16(collision + 0x8C);
            call_object_subtractHP(col_val, player);

            if (LD16(player + oBattleObject_HP) != 0) {
                call_sub_801A200(player);
                return;
            }
        } else {
            /* HP hit zero on the first subtract: skip white flash and the
             * second HP subtraction entirely (original beq loc_801BA76). */
        }

        /* Death handling (reached from both paths when HP reaches zero). */
        {
            call_battle_isAllianceActiveAndAiFlag0x4000_802DD2A_result ai_res =
                call_battle_isAllianceActiveAndAiFlag0x4000_802DD2A(player);
            if (ai_res.r0 != 0) {
                /* AI side active: flag 0x8000000 in AI data. */
                call_SetAIData_Unk_44_Flag(A__text, player);
            } else {
                /* Normal object: set death flag. */
                call_object_setFlag2(1, player);
            }
        }

        call_sub_801A200(player);
        return;
    }

    /* damage == 0: skip directly to the collision HP-bar subtraction. */
    {
        u16 col_val = LD16(collision + 0x8C);
        call_object_subtractHP(col_val, player);

        if (LD16(player + oBattleObject_HP) != 0) {
            call_sub_801A200(player);
            return;
        }

        call_battle_isAllianceActiveAndAiFlag0x4000_802DD2A_result ai_res =
            call_battle_isAllianceActiveAndAiFlag0x4000_802DD2A(player);
        if (ai_res.r0 != 0) {
            call_SetAIData_Unk_44_Flag(A__text, player);
        } else {
            call_object_setFlag2(1, player);
        }

        call_sub_801A200(player);
    }
}

Next applyDamageToPlayer_801ba12(void) {
    apply_damage_to_player(R5, R6, R10);
    RETURN_TO_CALLER();
}

/* ── battle_object_postAction4CheckClearFlags_801BA92 ── */
/* battle_object_postAction4CheckClearFlags_801BA92(r5 = battle object):
 * Post-action check for a battle object. If its current action is 4,
 * inspect its flag2 bits:
 *   - bit 14 set: nothing to do (result 1)
 *   - bit 1 clear: nothing to do (result 1)
 *   - otherwise clear flags and collision counter (result 2)
 * Any other action leaves state alone (result 0).
 * Result in r0; r1 ends up as the last mask tested on the flag paths. */
Next battle_object_postAction4CheckClearFlags_801BA92(void) {
    u32 result = 0;

    /* oBattleObject_CurAction is at offset 9 */
    if (LD8(R5 + 9) == 4) {
        result = 1;

        GUEST_CALL(object_getFlag2);
        R1 = 1 << 14;
        if ((R0 & R1) == 0) {
            GUEST_CALL(object_getFlag2);
            R1 = 2;
            if ((R0 & R1) != 0) {
                GUEST_CALL(battle_object_clearFlagsAndCollisionCounter_801A284);
                result = 2;
            }
        }
    }

    R0 = result;
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

/* ── object_reservePanel ── */
/* object_reservePanel(r0 = object whose panel is looked up, r5 = object that
 * wants to claim it):
 * If the panel under r0 exists and has no reserver yet, record r5 as the
 * reserver, set the panel's "reserved" flag (0x80) and the object's
 * "standing on a reserved panel" flag (0x20), and return 1. Otherwise leave
 * everything untouched and return 0. */
Next object_reservePanel(void) {
    u32 reserver = R5;

    /* Which panel (if any) is under the object in r0? */
    GUEST_CALL(object_getPanelDataOffset);
    u32 panel = R0;

    if (panel == 0) {
        /* No panel there: nothing to reserve. */
        R0 = 0;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    u32 currentReserver = LD32(panel + oPanelData_ReserverObjectPtr);
    if (currentReserver != 0) {
        /* Someone else already reserved this panel. */
        R0 = 0;
        R2 = currentReserver;
        SET_FLAGS_NZ(R0);
        RETURN_TO_CALLER();
    }

    /* Claim the panel for r5 and mark it as reserved. */
    ST32(panel + oPanelData_ReserverObjectPtr, reserver);
    R1 = LD32(panel + oPanelData_Flags);
    R2 = 0x80;
    R1 |= R2;
    ST32(panel + oPanelData_Flags, R1);

    /* Flag the reserving object as occupying a reserved panel
     * (oObjectHeader_Flags is at offset 0 of the object header). */
    R1 = 0x20;
    R0 = LD8(reserver) | R1;
    ST8(reserver, R0);

    R0 = 1;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_removePanelReserve ── */
/* object_removePanelReserve(r0, r1 = panel lookup params, r5 = requesting object):
 * Look up the panel at the given position; if it is currently reserved by the
 * given object, clear the reservation (reserver pointer and reserved bit) and
 * return 1, otherwise return 0.
 *
 * Flags: the original's last flag-setting instruction differs per path.
 *   - no panel found:  tst r0, r0, then movs r0, #0  -> N=0 Z=1,
 *                      C/V untouched since object_getPanelDataOffset
 *   - wrong reserver:  cmp r1, r5, then movs r0, #0  -> C/V from the compare,
 *                      N=0 Z=1
 *   - success:         cmp r1, r5, then movs r0, #1  -> C/V from the compare,
 *                      N=0 Z=0
 * On the failure paths r1 holds the panel's reserver pointer (unchanged if no
 * panel was found); on success r1 holds the updated panel flags and r2 the
 * cleared bit mask. */
Next object_removePanelReserve(void) {
    GUEST_CALL(object_getPanelDataOffset);

    u32 panel = R0;
    if (panel == 0) {
        /* No panel at that position; r1/r2 stay as the callee left them. */
        R0 = 0;
        SET_FLAGS_NZ(0);                /* tst r0, r0 / movs r0, #0 */
        RETURN_TO_CALLER();
    }

    u32 reserver = LD32(panel + oPanelData_ReserverObjectPtr);
    /* cmp r1, r5 sets C/V on both remaining paths (MOVS afterwards only
     * rewrites N/Z, so the compare flags survive until the caller). */
    if (reserver != R5) {
        /* Reserved by someone else (or not at all). */
        R1 = reserver;
        R0 = 0;
        SET_FLAGS_CMP(reserver, R5);
        SET_FLAGS_NZ(0);                /* movs r0, #0 */
        RETURN_TO_CALLER();
    }

    /* Panel was reserved by us: release it. */
    ST32(panel + oPanelData_ReserverObjectPtr, 0);
    R2 = 0x80;
    R1 = LD32(panel + oPanelData_Flags) & ~0x80u;
    ST32(panel + oPanelData_Flags, R1);
    R0 = 1;
    SET_FLAGS_CMP(reserver, R5);
    SET_FLAGS_NZ(1);                    /* movs r0, #1 */
    RETURN_TO_CALLER();
}

/* ── RemoveBattleObjectPanelReserves ── */
/* RemoveBattleObjectReserves (0x0801BB78): called with r5 = battle object.
 * If the object's header flags (byte at offset 0) have bit 0x20 set ("this
 * object holds panel reserves"), sweep the whole 7x4 battlefield — columns
 * 7..1 outer-indexed by rows 4..1, exactly the original's downward-counting
 * loop order — and release every panel reserve owned by this object.
 * r6/r7 are restored to their incoming values; r2/r3 end up holding
 * whatever the last callee left there, as in the original. */
Next RemoveBattleObjectPanelReserves(void) {
    if (!(LD8(R5) & 0x20)) /* oObjectHeader_Flags is offset 0 */
        RETURN_TO_CALLER();

    /* row counts down 4..1 (original r7), col counts down 7..1 (original r6) */
    for (u32 row = 4; row != 0; row--) {
        for (u32 col = 7; col != 0; col--) {
            R0 = col;
            R1 = row;
            GUEST_CALL(object_getPanelDataOffset);

            u32 panel = R0;
            if (panel != 0 && LD32(panel + oPanelData_ReserverObjectPtr) == R5) {
                /* this panel is reserved by our object: clear it */
                R0 = col;
                R1 = row;
                GUEST_CALL(object_removePanelReserve);
            }
        }
    }
    RETURN_TO_CALLER();
}

/* ── object_updateSprite ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1; bool z, n; } call_battle_isPaused_result;
/* register form: generated wrapper: battle_isPaused has no typed C interface yet */
static inline call_battle_isPaused_result call_battle_isPaused(void) {
    GUEST_CALL(battle_isPaused);
    return (call_battle_isPaused_result){ R0, R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r0, r1; bool z, n; } call_battle_isTimeStop_result;
/* register form: generated wrapper: battle_isTimeStop has no typed C interface yet */
static inline call_battle_isTimeStop_result call_battle_isTimeStop(void) {
    GUEST_CALL(battle_isTimeStop);
    return (call_battle_isTimeStop_result){ R0, R1, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* register form: generated wrapper: sprite_setAnimation has no typed C interface yet */
static inline void call_sprite_setAnimation(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sprite_setAnimation);
}

typedef struct { u32 r4, r5; } call_sprite_loadAnimationData_result;
/* register form: generated wrapper: sprite_loadAnimationData has no typed C interface yet */
static inline call_sprite_loadAnimationData_result call_sprite_loadAnimationData(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_loadAnimationData);
    return (call_sprite_loadAnimationData_result){ R4, R5 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_sprite_update_result_object_updateSprite;
/* register form: generated wrapper: sprite_update has no typed C interface yet */
static inline call_sprite_update_result_object_updateSprite call_sprite_update_object_updateSprite(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_update);
    return (call_sprite_update_result_object_updateSprite){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Header-flag bits tested by object_updateSprite. */
#define OBJFLAG_UPDATE_ENABLE  0x01u /* clear: object does not tick its sprite */
#define OBJFLAG_NO_SPRITE_TICK 0x08u /* set: skip the sprite update entirely */
#define OBJFLAG_SKIP_TIMESTOP  0x10u /* set: keep ticking even while time is stopped */

/* Encode an N/Z pair as a value for SET_FLAGS_NZ (result 0 -> Z set, bit 31 -> N set). */
static u32 zn_operand(bool zero, bool negative) {
    if (zero) return 0u;
    if (negative) return 0x80000000u;
    return 1u;
}

/* What the adapter must write back: r0/r1 and the N/Z operand.
 * C and V are deliberately never set here: on every exit path the original
 * leaves them as the last called routine (battle_isPaused, battle_isTimeStop
 * or sprite_update) did, and the wrapper calls already left exactly that
 * state behind. Thumb TST (all the in-function tests) preserves C/V too. */
struct updateSprite_out {
    u32 r0;
    u32 r1;
    u32 zn_operand;
};

/* Tick one battle object's sprite: reload its animation if it changed, then
 * advance it, unless the battle state forbids updates this frame. */
static struct updateSprite_out update_object_sprite(u32 obj) {
    struct updateSprite_out out;

    call_battle_isPaused_result paused = call_battle_isPaused();
    if (!paused.z) {                       /* bne -> battle paused, nothing to do */
        out.r0 = paused.r0;
        out.r1 = paused.r1;
        out.zn_operand = zn_operand(paused.z, paused.n);
        return out;
    }

    u32 flags = LD8(obj + oObjectHeader_Flags);

    if ((flags & OBJFLAG_UPDATE_ENABLE) == 0) {      /* tst flags,#1 / beq */
        out.r0 = flags;
        out.r1 = OBJFLAG_UPDATE_ENABLE;
        out.zn_operand = flags & OBJFLAG_UPDATE_ENABLE;
        return out;
    }
    if ((flags & OBJFLAG_NO_SPRITE_TICK) != 0) {     /* tst flags,#8 / bne */
        out.r0 = flags;
        out.r1 = OBJFLAG_NO_SPRITE_TICK;
        out.zn_operand = flags & OBJFLAG_NO_SPRITE_TICK;
        return out;
    }

    /* Value the original still holds in its tail register here:
     * 0x10 if the time-stop check was skipped, else battle_isTimeStop's. */
    u32 tailValue;
    if ((flags & OBJFLAG_SKIP_TIMESTOP) == 0) {      /* tst flags,#0x10 falls through */
        call_battle_isTimeStop_result stopped = call_battle_isTimeStop();
        if (!stopped.z) {                            /* bne -> time stopped, leave as is */
            out.r0 = stopped.r0;
            out.r1 = stopped.r1;
            out.zn_operand = zn_operand(stopped.z, stopped.n);
            return out;
        }
        tailValue = stopped.r1;
    } else {
        tailValue = OBJFLAG_SKIP_TIMESTOP;           /* mov tail,#0x10 stays live */
    }

    /* With collision data linked, a nonzero PreventAnim byte blocks the reload. */
    if (LD32(obj + oBattleObject_CollisionDataPtr) != 0) {
        u32 preventAnim = LD8(obj + oBattleObject_PreventAnim);
        if (preventAnim != 0) {                      /* tst preventAnim / bne */
            out.r0 = preventAnim;
            out.r1 = tailValue;
            out.zn_operand = preventAnim;            /* byte value: N=0, Z=0 */
            return out;
        }
    }

    /* Restart the animation only when the loaded one is not the current one. */
    u32 curAnim = LD8(obj + oBattleObject_CurAnim);
    if (curAnim != LD8(obj + oBattleObject_CurAnimCopy)) {
        call_sprite_setAnimation(curAnim, obj);
        call_sprite_loadAnimationData(obj);
        ST8(obj + oBattleObject_CurAnimCopy, LD8(obj + oBattleObject_CurAnim));
    }

    /* Advance the sprite; the callee defines r0-r5 and all four flags. */
    call_sprite_update_result_object_updateSprite tick = call_sprite_update_object_updateSprite(obj);
    out.r0 = tick.r0;
    out.r1 = tick.r1;
    out.zn_operand = zn_operand(tick.z, tick.n);
    return out;
}

/* object_updateSprite(r5 = battle object, r10 = battle state) ->
 * r0 r1 r2 r3 r4 r5 nz c v. r2-r5 and C/V are not written: like the original,
 * they are simply what the last executed callee left behind. */
Next object_updateSprite(void) {
    struct updateSprite_out out = update_object_sprite(R5);
    R0 = out.r0;
    R1 = out.r1;
    SET_FLAGS_NZ(out.zn_operand);
    RETURN_TO_CALLER();
}

/* ── object_updateSpriteTimestop ── */
/* object_updateSpriteTimestop (r5 = battle object header, r4/r10 unused inputs):
 * advance one battle object's sprite animation. Nothing happens while the
 * battle is paused, if the object has no sprite at all (flags bit 0), or if
 * the time stop has frozen its animation (flags bit 3). Otherwise, when the
 * object's current animation id no longer matches the copy the sprite was
 * last set up with, reapply the animation and reload its frame data, then
 * update the sprite frame. */
Next object_updateSpriteTimestop(void) {
    GUEST_CALL(battle_isPaused);
    if (R0 != 0)                            /* battle paused: sprites hold still */
        RETURN_TO_CALLER();

    R0 = LD8(R5);                           /* object flags (oObjectHeader_Flags, offset 0) */
    R1 = 1;
    if (!(R0 & R1)) {                       /* bit 0 clear: object has no sprite */
        SET_FLAGS_NZ(R0 & R1);
        RETURN_TO_CALLER();
    }

    R1 = 8;
    if (R0 & R1) {                          /* bit 3 set: animation frozen by time stop */
        SET_FLAGS_NZ(R0 & R1);
        RETURN_TO_CALLER();
    }

    R0 = LD8(R5 + oBattleObject_CurAnim);
    R1 = LD8(R5 + oBattleObject_CurAnimCopy);
    if (R0 != R1) {                         /* animation changed since last setup */
        GUEST_CALL(sprite_setAnimation);    /* r0 = new animation id */
        GUEST_CALL(sprite_loadAnimationData);
        ST8(R5 + oBattleObject_CurAnimCopy, LD8(R5 + oBattleObject_CurAnim));
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── UpdateBattleObjectSprite ── */
/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet */
static bool battle_time_is_stopped(u32 *leftover_r1) {
    GUEST_CALL(battle_isTimeStop);
    *leftover_r1 = R1;   /* the caller can still read r1, which this call leaves behind */
    return !ZERO_FLAG(); /* the original branches with bne: Z clear means time is stopped */
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void set_sprite_animation(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void load_sprite_animation_data(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static void update_sprite_UpdateBattleObjectSprite(void) {
    GUEST_CALL(sprite_update);
}

/* register form: the original's early exits return right after a tst, leaving its own
 * r0/r1 and N/Z for the caller (C/V untouched); on the paths that end in a call, those
 * same registers must stay exactly as the last callee left them, and the entry adapter
 * may not branch between the two behaviors, so plain C cannot express this. */
static void return_early_without_update(u32 r0, u32 r1, u32 tst_result) {
    R0 = r0;
    R1 = r1;
    SET_FLAGS_NZ(tst_result);
}

/* UpdateBattleObjectSprite: advance one battle object's sprite for this frame.
 *
 * The update is skipped when the object is inactive, has sprite updates disabled,
 * is caught by an active time stop (unless flagged to keep updating through it),
 * or has collision data with its animation locked (PreventAnim set). A changed
 * CurAnim is applied and cached in CurAnimCopy before the sprite is advanced. */
static void update_battle_object_sprite(u32 obj) {
    u8 flags = LD8(obj + oObjectHeader_Flags);

    if (!(flags & OBJECT_FLAG_ACTIVE)) {
        return_early_without_update(flags, OBJECT_FLAG_ACTIVE, flags & OBJECT_FLAG_ACTIVE);
        return;
    }
    if (flags & OBJECT_FLAG_STOP_SPRITE_UPDATE) {
        return_early_without_update(flags, OBJECT_FLAG_STOP_SPRITE_UPDATE,
                                    flags & OBJECT_FLAG_STOP_SPRITE_UPDATE);
        return;
    }

    /* What the original holds in r1 if it exits at the PreventAnim check below:
     * either the flag constant from its last mov, or battle_isTimeStop's leftover
     * r1 when that call was made (the caller can read r1 in both cases). */
    u32 r1_if_anim_locked = OBJECT_FLAG_UPDATE_DURING_TIMESTOP;
    if (!(flags & OBJECT_FLAG_UPDATE_DURING_TIMESTOP)
        && battle_time_is_stopped(&r1_if_anim_locked))
        return; /* time stopped: the caller sees battle_isTimeStop's own results */

    /* Objects with collision data and a locked animation do not animate. */
    if (LD32(obj + oBattleObject_CollisionDataPtr) != 0) {
        u8 prevent_anim = LD8(obj + oBattleObject_PreventAnim);
        if (prevent_anim != 0) {
            return_early_without_update(prevent_anim, r1_if_anim_locked, prevent_anim);
            return;
        }
    }

    /* Apply a newly chosen animation, then advance the sprite one frame. */
    u8 cur_anim = LD8(obj + oBattleObject_CurAnim);
    if (cur_anim != LD8(obj + oBattleObject_CurAnimCopy)) {
        set_sprite_animation(cur_anim);
        load_sprite_animation_data();
        /* re-read CurAnim: the animation calls may have changed it */
        ST8(obj + oBattleObject_CurAnimCopy, LD8(obj + oBattleObject_CurAnim));
    }
    update_sprite_UpdateBattleObjectSprite();
}

/* UpdateBattleObjectSprite(r5 = battle object) */
Next UpdateBattleObjectSprite(void) {
    update_battle_object_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── object_updateSpritePaused ── */
/* object_updateSpritePaused: 0801BCA6-0801BCD0 THUMB (bn6f asm00_2.s:25827)
 *   inputs:  r5 r10 lr
 *   outputs: nz c v r0 r1 r2 r3 r4 r5 lr
 *   clobbers (unused by callers): -
 *   preserved: r4 r5 */

/* oObjectHeader_Flags bit 0: only objects with this bit keep stepping their sprite here. */
#define OBJ_FLAG_SPRITE_UPDATES 0x01

/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet */
static bool time_stop_active(void) {
    GUEST_CALL(battle_isTimeStop);
    return !ZERO_FLAG();       /* answers in Z: clear while a time-stop holds the battle */
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_set_animation_object_updateSpritePaused(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_load_animation_data_object_updateSpritePaused(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static void sprite_update_frame(void) {
    GUEST_CALL(sprite_update);
}

/* register form: the early-out at the flag check must leave this function's own
 * results in the guest registers (r0 = flags byte, r1 = 1, Z set by the tst),
 * while every other path must keep the last callee's leftover registers and flags.
 * The adapter may not branch, so it can never choose between the two, and plain C
 * cannot read the callees' register/flag leftovers back to route them through a
 * return value. */
static void set_skip_result(u8 flags) {
    R0 = flags;
    R1 = 1;
    SET_FLAGS_NZ(flags & OBJ_FLAG_SPRITE_UPDATES);  /* the tst result: bit clear, so Z set */
}

/* Step the sprite of a battle object whose logic is paused: adopt a newly selected
 * animation, then advance the sprite one frame. Unflagged objects, and the whole
 * battle while a time-stop is active, leave memory and the register file untouched. */
static void update_paused_object_sprite(u32 obj) {
    u8 flags = LD8(obj + oObjectHeader_Flags);
    if (!(flags & OBJ_FLAG_SPRITE_UPDATES)) {
        set_skip_result(flags);
        return;
    }

    if (time_stop_active())
        return;    /* frozen: battle_isTimeStop's register/flag leftovers stand as the result */

    u8 anim = LD8(obj + oBattleObject_CurAnim);
    if (anim != LD8(obj + oBattleObject_CurAnimCopy)) {
        sprite_set_animation_object_updateSpritePaused(anim);
        sprite_load_animation_data_object_updateSpritePaused();
        /* re-read CurAnim, as the original does: the animation calls may adjust it */
        ST8(obj + oBattleObject_CurAnimCopy, LD8(obj + oBattleObject_CurAnim));
    }

    sprite_update_frame();
}

Next object_updateSpritePaused(void) {
    update_paused_object_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── sub_801BCF4 ── */
/* Bits of oObjectHeader_Flags that gate the per-frame sprite update. */
enum {
    OBJF_HAS_SPRITE      = 0x01, /* clear: this object's sprite is not animated */
    OBJF_FROZEN          = 0x08, /* set: sprite update disabled entirely */
    OBJF_IGNORE_TIMESTOP = 0x10, /* set: keeps animating while time is stopped */
};

/* register form: calls battle_isPaused outside this file, which has no typed C interface yet */
static bool battle_is_paused(void) {
    GUEST_CALL(battle_isPaused);
    return !ZERO_FLAG(); /* nonzero result means the battle is paused */
}

/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet */
static bool time_is_stopped(void) {
    GUEST_CALL(battle_isTimeStop);
    return !ZERO_FLAG(); /* nonzero result means time stop is active */
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void sprite_set_animation_sub_801BCF4(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void sprite_load_animation_data_sub_801BCF4(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_update outside this file, which has no typed C interface yet */
static void update_sprite_sub_801BCF4(void) {
    GUEST_CALL(sprite_update);
}

/* Per-frame sprite update for one battle object: load a new animation if the
 * object's current one changed, then advance the sprite. Skipped while the
 * battle is paused or time is stopped (unless the object's flags exempt it),
 * for objects without an animated sprite or with updates frozen, and for
 * collidable objects whose current animation is locked.
 * Returns obj unchanged, as the original preserves r5. */
static u32 battle_object_update_sprite(u32 obj) {
    if (battle_is_paused())
        return obj;

    u8 flags = LD8(obj + oObjectHeader_Flags);
    if (!(flags & OBJF_HAS_SPRITE))
        return obj;
    if (flags & OBJF_FROZEN)
        return obj;
    if (!(flags & OBJF_IGNORE_TIMESTOP) && time_is_stopped())
        return obj;

    /* Only objects with collision data can lock their animation. */
    if (LD32(obj + oBattleObject_CollisionDataPtr) != 0
        && LD8(obj + oBattleObject_PreventAnim) != 0)
        return obj;

    u8 anim = LD8(obj + oBattleObject_CurAnim);
    if (anim != LD8(obj + oBattleObject_CurAnimCopy)) {
        sprite_set_animation_sub_801BCF4(anim);
        sprite_load_animation_data_sub_801BCF4();
        /* Re-read CurAnim: the sprite calls may have touched it. */
        ST8(obj + oBattleObject_CurAnimCopy, LD8(obj + oBattleObject_CurAnim));
    }

    update_sprite_sub_801BCF4();
    return obj;
}

/* sub_801BCF4(r5 = battle object) -> r5 (unchanged) */
Next sub_801BCF4(void) {
    R5 = battle_object_update_sprite(R5);
    RETURN_TO_CALLER();
}

/* ── AddRandomVarianceToTwoCoords ── */
/* AddRandomVarianceToTwoCoords(r0 = variance mask, r1 = coord A, r3 = coord B):
 * Jitter two coordinates by a centered random offset applied in the
 * fixed-point high half:
 *     coord += ((rng & mask) - mask/2) << 16
 * Coord A uses the low 16 bits of the RNG roll, coord B the high 16 bits.
 * r2/r4/r6/r7 must come out untouched; since GetRNG may trash registers,
 * the original saves r0-r3 around the call (and r4/r6/r7 at entry), so we
 * keep everything we have to preserve in locals across the call. */
Next AddRandomVarianceToTwoCoords(void) {
    u32 mask = R0;
    u32 coordA = R1;
    u32 coordB = R3;
    u32 half = mask >> 1;

    /* Values the caller expects back unchanged; GetRNG is free to clobber
     * caller-saved registers, so snapshot them before the call. */
    u32 savedR2 = R2;
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 savedR7 = R7;

    GUEST_CALL(GetRNG);
    u32 rng = R0;

    R1 = coordA + (((rng & mask) - half) << 16);
    R3 = coordB + ((((rng >> 16) & mask) - half) << 16);

    R2 = savedR2;
    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;

    RETURN_TO_CALLER();
}

/* ── calc8DirBitmaskTowardsPoint_801BE04 ── */
/* calc8DirBitmaskTowardsPoint_801BE04(r0 = x_a, r1 = y_a, r2 = x_b, r3 = y_b):
 * Compute the 8-way direction from point B toward point A as a small bitmask:
 * the y difference contributes 1 (A above B) or 2 (A below B), the x
 * difference contributes 4 (A left of B) or 8 (A right of B), and equal
 * coordinates contribute 0. E.g. A up-and-right of B yields 8 | 2 = 0xA.
 *
 * The comparisons must be exact signed comparisons of the operands (as the
 * original's subs + bgt/blt sequence achieves via the overflow flag), so
 * they are done directly rather than by subtracting, which could overflow.
 * Leaves c/v reflecting the final subtraction r1 - r3, like the original. */
Next calc8DirBitmaskTowardsPoint_801BE04(void) {
    u32 step_x = (s32)R0 > (s32)R2 ? 8
               : (s32)R0 < (s32)R2 ? 4
               :                     0;
    u32 step_y = (s32)R1 > (s32)R3 ? 2
               : (s32)R1 < (s32)R3 ? 1
               :                     0;

    R0 = step_x | step_y;

    /* Last flag-setting instruction of the original is SUBS r1, r1, r3. */
    SET_FLAGS_CMP(R1, R3);
    RETURN_TO_CALLER();
}

/* ── nullsub_10 ── */
/* nullsub_10: does nothing; kept as a placeholder so call sites stay addressable. */
Next nullsub_10(void) {
    RETURN_TO_CALLER();
}

/* ── resetBattleBuffers_801BE70 ── */
/* register form: calls ZeroFillByWord outside this file, which has no typed C interface yet */
static void zero_fill_by_word_sub_801BE70(u32 addr, u32 num_bytes) {
    R0 = addr;
    R1 = num_bytes;
    GUEST_CALL(ZeroFillByWord);
}

/* Zero a battery of battle-related RAM blocks and initialise their first fields. */
static void reset_battle_buffers(void) {
    /* Zero the main 0x60-byte battle state struct. */
    zero_fill_by_word_sub_801BE70(A_eStruct2035280, 0x60);

    /* Initialise its count/total fields. */
    ST16(A_eStruct2035280 + 0x22, 0x20);
    ST16(A_eStruct2035280 + 0x26, (u16)0 - 1);   /* 0xFFFF */

    /* Zero the 0x30-byte and 0x50-byte sibling buffers. */
    zero_fill_by_word_sub_801BE70(A_dword_20352E0, 0x30);
    zero_fill_by_word_sub_801BE70(A_byte_203EB50, 0x50);

    /* Clear the two dword slots. */
    ST32(A_dword_203CA48,     0);
    ST32(A_dword_203CA48 + 4, 0);
    ST32(A_dword_20367E0,     0);
}

Next resetBattleBuffers_801BE70(void) {
    reset_battle_buffers();
    RETURN_TO_CALLER();
}

/* ── flagSetBits_eStruct2035280_801BEB8 ── */
/* flagSetBits_eStruct2035280_801BEB8(r0 = flag bits to set):
 * Sets the given bits in the event/flag word at eStruct2035280+0x44.
 * Leaves r1 = eStruct2035280, r2 = the updated word, and nz from the OR. */
Next flagSetBits_eStruct2035280_801BEB8(void) {
    u32 flags = LD32(A_eStruct2035280 + 0x44) | R0;

    R1 = A_eStruct2035280;
    R2 = flags;
    ST32(A_eStruct2035280 + 0x44, flags);
    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── flags_clearBits_20352C4_801BEC2 ── */
/* flags_clearBits_20352C4_801BEC2(r0 = mask): clear the bits of `mask` in the flag word at
 * eStruct2035280+0x44 (dword_20352C4), i.e. mark those flags as off.
 * Leaves r1 = eStruct2035280, r2 = updated word, and NZ set on it. */
Next flags_clearBits_20352C4_801BEC2(void) {
    u32 mask = R0;
    u32 flags;

    R1 = A_eStruct2035280;
    flags = LD32(R1 + 0x44);
    flags &= ~mask;
    ST32(R1 + 0x44, flags);
    R2 = flags;

    SET_FLAGS_NZ(flags);
    RETURN_TO_CALLER();
}

/* ── eStruct2035280_SetRequestBits_801BECC ── */
/* eStruct2035280_SetRequestBits_801BECC(r0 = flag bits): set the given bits in the request dword at
 * eStruct2035280+0x40 (dword_20352C0). Leaves the struct pointer in r1 and the
 * updated dword in r2. */
Next eStruct2035280_SetRequestBits_801BECC(void) {
    u32 *flags = (u32 *)A_eStruct2035280;
    R1 = A_eStruct2035280;
    R2 = LD32(A_eStruct2035280 + 0x40) | R0;
    ST32(A_eStruct2035280 + 0x40, R2);
    RETURN_TO_CALLER();
}

/* ── flags_clearBits_eStruct2035280_801BED6 ── */
/* flags_clearBits_eStruct2035280_801BED6(r0 = bit mask): clear the given bits in dword_20352C0.
 * Returns the pointer in r1 and the updated value in r2, with flags set
 * on the new value (callers branch on it). */
Next flags_clearBits_eStruct2035280_801BED6(void) {
    u32 mask = R0;
    u32 *field = (u32 *)(A_eStruct2035280 + 0x40); /* dword_20352C0 */

    R1 = (u32)A_eStruct2035280;
    R2 = LD32(field) & ~mask;
    ST32(field, R2);
    SET_FLAGS_NZ(R2);
    RETURN_TO_CALLER();
}

/* ── nullsub_58 ── */
/* nullsub_58: empty stub at 0x0801BFEC — does nothing. */
Next nullsub_58(void) {
    RETURN_TO_CALLER();
}

/* ── battle_loadChipPreviewGfxInit_801BFEE ── */
/* battle_loadChipPreviewGfxInit_801BFEE: enable (pass 1 to battle_loadChipPreviewGfxFromHandTable_801C002); all results (r0-r3, flags)
 * come straight from that call. */
Next battle_loadChipPreviewGfxInit_801BFEE(void) {
    R0 = 1;
    GUEST_CALL(battle_loadChipPreviewGfxFromHandTable_801C002);
    RETURN_TO_CALLER();
}

/* ── battle_loadChipPreviewGfxDefault_801BFF8 ── */
/* battle_loadChipPreviewGfxDefault_801BFF8: invoke battle_loadChipPreviewGfxFromHandTable_801C002 with r0 = 0 and let its results
 * (registers and flags) pass straight through to the caller. */
Next battle_loadChipPreviewGfxDefault_801BFF8(void) {
    R0 = 0;
    GUEST_CALL(battle_loadChipPreviewGfxFromHandTable_801C002);
    RETURN_TO_CALLER();
}

/* ── battle_RequestEndBattle_801C06E ── */
/* battle_RequestEndBattle_801C06E: turn the "end battle" flag on.
 * Sets the argument to 1 and tail-calls battle_drawChipCountsOAM_801C082 (the shared
 * flag-setting routine), which returns straight to this function's caller. */
Next battle_RequestEndBattle_801C06E(void) {
    R0 = 1;
    GUEST_CALL(battle_drawChipCountsOAM_801C082);
    RETURN_TO_CALLER();
}

/* ── battle_drawChipCountsOAMWithRetvalZero_801C078 ── */
/* battle_drawChipCountsOAMWithRetvalZero_801C078: entry point that invokes the main routine with r0 = 0
 * (no arguments); all results come straight from battle_drawChipCountsOAM_801C082. */
Next battle_drawChipCountsOAMWithRetvalZero_801C078(void) {
    R0 = 0;
    GUEST_CALL(battle_drawChipCountsOAM_801C082);
    RETURN_TO_CALLER();
}

/* ── easeValueTowardTarget_801C1EA ── */
/* easeValueTowardTarget_801C1EA(r0 = value, r1 = target, r4 = state object):
 * Marks the object as active (bytes at +8/+9 set to 1), then nudges the
 * value one step toward the target. The step is proportional to the
 * remaining distance (ease-out: an eighth of the gap, plus 2), so the
 * value closes in quickly at first and slows down near the target.
 * The step is clamped so the value never overshoots past the target.
 * Returns r0 = new value, r2 = the step that was taken. */
Next easeValueTowardTarget_801C1EA(void) {
    R2 = 1;
    ST8(R4 + 8, R2);
    ST8(R4 + 9, R2);

    /* ease-out step toward the target */
    R2 = (u32)((s32)(R0 - R1) >> 3) + 2;
    R0 -= R2;

    SET_FLAGS_CMP(R0, R1);
    if ((s32)R0 < (s32)R1) {
        /* overshot: land exactly on the target */
        R0 = R1;
        SET_FLAGS_ADD(R1, 0);
    }
    RETURN_TO_CALLER();
}

/* ── renderHpDigits_801C296 ── */
/* register form: calls memory_bcdConvert outside this file, which has no typed C interface yet */
static u32 bcd_convert(u32 value) {
    R0 = value;
    GUEST_CALL(memory_bcdConvert);
    return R0;
}

/* register form: calls countHexDigits_8000C5C outside this file, which has no typed C interface yet */
static u32 count_hex_digits(u32 value) {
    R0 = value;
    GUEST_CALL(countHexDigits_8000C5C);
    return R0;
}

/* Render an HP-style 4-nibble readout derived from the object at src_obj
 * into the struct at dst_readout.
 *
 * The function preserves r3 (it is both an input and an output of the
 * original), so the typed wrapper takes it and returns it unchanged. r4,
 * r6 and r7 are also preserved by the original; they are passed in for the
 * memory operands but not modified by the typed code.
 *
 * Source object (r4):
 *   +0    u8  flags; bit 4 set => use "max - current" mode
 *   +2    u16 current HP (always read)
 *   +0xC  ptr whose +0x26 (u16) is the max-HP reference
 *   +0x10 u32 previously-rendered nibbles (snapshot for diff)
 *
 * Destination readout (r7):
 *   +0..+3  four nibble bytes (one 4-bit glyph each)
 *   +4      s32 pre-blit x-pixel offset from the digit count */
static u32 render_hp_digits(u32 r3_in, u32 r4, u32 r6, u32 r7) {
    (void)r6;
    u32 flags = LD8(r4);
    u32 hp    = LD16(r4 + 2);

    u32 bcd_in = hp;
    if (flags & 0x10) {
        u32 ref = LD32(r4 + 0xC);
        u32 max_hp = LD16(ref + 0x26);
        bcd_in = max_hp - hp;
    }

    u32 bcd = bcd_convert(bcd_in);

    ST8(r7 + 0, bcd & 0xF);
    ST8(r7 + 1, (bcd >> 4)  & 0xF);
    ST8(r7 + 2, (bcd >> 8)  & 0xF);
    ST8(r7 + 3, (bcd >> 12) & 0xF);

    u32 digits = count_hex_digits(bcd);
    s32 x_offset = -(s32)(0x20 - (digits << 2));
    ST32(r7 + 4, (u32)x_offset);

    /* Right-justify: blank the high (leftmost) nibble positions with 0xA. */
    for (u32 i = digits; i < 4; i++) {
        ST8(r7 + i, 0xA);
    }

    /* Snapshot the previous value first, store the new value, then diff. */
    u32 prev = LD32(r4 + 0x10);
    u32 cur  = LD32(r7);
    ST32(r4 + 0x10, cur);

    for (u32 i = 0; i < 4; i++) {
        u32 cur_nib = cur  & 0xF;
        u32 old_nib = prev & 0xF;
        u32 out = (cur_nib == old_nib) ? 0xB : cur_nib;
        ST8(r7 + i, out);
        cur  >>= 8;
        prev >>= 8;
    }

    return r3_in;
}

/* renderHpDigits_801C296(r3, r4=source object, r6, r7=destination readout) -> r3 r4 r6 r7 */
Next renderHpDigits_801C296(void) {
    R3 = render_hp_digits(R3, R4, R6, R7);
    RETURN_TO_CALLER();
}

/* ── chatbox_uploadFrameTileGFX_801C304 ── */
/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_two_tiles(u32 src, u32 dst) {
    R0 = src;
    R1 = dst;
    R2 = 0x20;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    R0 = R0 + R2;
    R1 = R1 + 0x80;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* Decompress tiles for a battle portrait-style effect into VRAM.
 * The effect index comes from the byte at r4+8 and selects one of three
 * source-pointer tables. r7 points to four tile-index bytes; we walk them
 * from offset 3 down to 0. For each tile index != 0xB we look up its base
 * source pointer and queue two 0x20-byte DMA copies (the first to the
 * current dst, the next to dst + 0x80). After each iteration (skip or not),
 * dst advances by 0x20. */
static void decompress_portrait_tiles(u32 r4, u32 r5, u32 r7) {
    u8 effect = LD8(r4 + 8);
    u32 src_table = LD32(A_off_801C34C + ((u32)effect << 2));
    u32 dst = r5;
    for (s32 i = 3; i >= 0; i--) {
        u8 tile_index = LD8(r7 + (u32)i);
        if (tile_index != 0xB) {
            u32 base_src = LD32(src_table + ((u32)tile_index << 2));
            queue_two_tiles(base_src, dst);
        }
        dst += 0x20;
    }
}

Next chatbox_uploadFrameTileGFX_801C304(void) {
    /* R3-R7 are inputs that the original preserves across the call (push/pop
     * restores them). R4, R5, R6, R7 are written by the function body, so we
     * write them back explicitly; R3 is never modified and must be preserved. */
    u32 r3 = R3;
    decompress_portrait_tiles(R4, R5, R7);
    R3 = r3;
    RETURN_TO_CALLER();
}

/* ── sub_801C906 ── */
/* Tile id of the first small digit glyph. Each digit is two vertically stacked tiles
 * (top at DIGIT_GLYPH_TILES + digit*2, bottom one below it), so a nibble indexes its
 * glyph directly; nibble 0xA is the blank used to pad short numbers. */
#define DIGIT_GLYPH_TILES  0xD1A0
#define BLANK_TILE_NIBBLES 0xAAAA

/* register form: calls memory_bcdConvert outside this file, which has no typed C interface yet */
static u32 to_packed_bcd(u16 value) {
    R0 = value;
    GUEST_CALL(memory_bcdConvert);
    return R0;
}

/* register form: calls countHexDigits_8000C5C outside this file, which has no typed C interface yet */
static u32 count_hex_digits_sub_801C906(u32 value) {
    R0 = value;
    GUEST_CALL(countHexDigits_8000C5C);
    return R0;
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_background_tiles_sub_801C906(u32 j, u32 i, u32 which_tile_block_32x32, u32 tile_ids,
                                  u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = which_tile_block_32x32;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx_transfer(u32 queued_src, u32 mut_queued_dest, u32 queued_size) {
    R0 = queued_src;
    R1 = mut_queued_dest;
    R2 = queued_size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* The original pushes r4-r7 on entry and pops them on return (r5 comes back via the
 * mid-function pop and the last callee preserving it), so these must reach the caller
 * with their entry values even though the gfx callees clobber the live registers. */
struct PreservedRegisters {
    u32 r4;
    u32 r5;
    u32 r6;
    u32 r7;
};

/* Redraw an object's four-digit decimal readout (its HP shown on the field):
 * convert the value at +0x24 into decimal digits, stamp them right-aligned with the
 * digit glyphs into background tile block 3 at the object's map position, and queue
 * the 32-byte companion tile selected by the index at +3 for VRAM upload.
 * The preserved registers ride through unchanged, as the original's push/pop guarantees. */
static struct PreservedRegisters draw_number_label(u32 obj, struct PreservedRegisters preserved) {
    /* One decimal digit per nibble; shifting 0xAAAA past the significant digits fills the
     * unused high positions with 0xA, so the number comes out right-aligned and blank-padded. */
    u32 packed = to_packed_bcd(LD16(obj + 0x24));
    packed |= BLANK_TILE_NIBBLES << (count_hex_digits_sub_801C906(packed) * 4);

    /* register form: the 4x2 tile-id buffer must live at a guest stack address,
     * since that address is handed to CopyBackgroundTiles */
    SP -= 0x10;
    u32 tile_ids = SP;

    /* Fill the 4x2 tile grid from the right: each nibble picks a digit glyph
     * (its top tile plus the tile directly beneath it). */
    for (int column = 3; column >= 0; column--) {
        u32 top_tile = DIGIT_GLYPH_TILES + (packed & 0xF) * 2;
        ST16(tile_ids + column * 2, top_tile);
        ST16(tile_ids + 8 + column * 2, top_tile + 1);
        packed >>= 4;
    }

    /* Draw the 4-wide, 2-tall block one tile right of the stored map position. */
    copy_background_tiles_sub_801C906(LD8(obj + 1) + 1, LD8(obj + 2), 3, tile_ids, 4, 2);

    SP += 0x10;

    /* Queue the companion tile: 32 bytes of ROM gfx chosen by the object's index,
     * described by the {source, destination, size} table at off_801C970. */
    u32 upload = A_off_801C970;
    queue_gfx_transfer(LD32(upload) + LD8(obj + 3) * 0x20, LD32(upload + 4), LD32(upload + 8));

    return preserved;
}

/* sub_801C906(r5 = label object) -> r3 (leftover from the gfx transfer call),
 * r4-r7 restored to their entry values */
Next sub_801C906(void) {
    struct PreservedRegisters preserved = { R4, R5, R6, R7 };
    struct PreservedRegisters out = draw_number_label(R5, preserved);
    R4 = out.r4;
    R5 = out.r5;
    R6 = out.r6;
    R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── battle_initObjectStoryFlags_801C984 ── */
/* battle_initObjectStoryFlags_801C984: query battle_getStoryFlagFromBattleState_802CE60 twice and store the answers as 0/1 flags:
 *   [r5+7] = (battle_getStoryFlagFromBattleState_802CE60(0) != 0)
 *   [r5+9] = (battle_getStoryFlagFromBattleState_802CE60(1) != 0)
 * The result of the second query is also left in r0, with flags matching
 * the original's final CMP/MOVS. */
Next battle_initObjectStoryFlags_801C984(void) {
    u32 flag;

    /* First query, normalize the answer to 0/1 and store it. */
    R0 = 0;
    GUEST_CALL(battle_getStoryFlagFromBattleState_802CE60);
    flag = (R0 != 0);
    ST8(R5 + 7, flag);

    /* Second query: the raw return value is kept in r0 (0 or non-zero,
     * matching the original's conditional `mov r0, #1`), and the flags
     * come from either the CMP or the MOVS, depending on the path. */
    R0 = 1;
    GUEST_CALL(battle_getStoryFlagFromBattleState_802CE60);
    if (R0 != 0) {
        R0 = 1;
        SET_FLAGS_NZ(R0);
    } else {
        SET_FLAGS_CMP(R0, 0);
    }
    ST8(R5 + 9, R0);

    RETURN_TO_CALLER();
}

/* ── syncCurrentFieldsAndNotify_801C9A4 ── */
/* syncCurrentFieldsAndNotify_801C9A4: keep the "current" byte fields of the struct at R5 in sync with
 * the "target" ones.  Whenever [R5+8] differs from [R5+7], it is updated and
 * background_drawPresetTileBlock_801C9C8 is notified with event 6; likewise [R5+0xA] vs [R5+9] with
 * event 0x1A.  On return, R0/R1 hold [R5+0xA]/[R5+9] (or the callee's R1 on
 * the second update path), and the flags come from the final compare or from
 * the last background_drawPresetTileBlock_801C9C8 call. */
Next syncCurrentFieldsAndNotify_801C9A4(void) {
    u32 newFirst = LD8(R5 + 7);

    if (LD8(R5 + 8) != newFirst) {
        ST8(R5 + 8, newFirst);
        R0 = 6;
        R1 = newFirst;
        GUEST_CALL(background_drawPresetTileBlock_801C9C8);
    }

    u32 newSecond = LD8(R5 + 9);
    u32 curSecond = LD8(R5 + 0xA);
    R0 = curSecond;
    R1 = newSecond;

    if (curSecond == newSecond) {
        SET_FLAGS_CMP(curSecond, newSecond);
    } else {
        ST8(R5 + 0xA, newSecond);
        R0 = 0x1A;
        GUEST_CALL(background_drawPresetTileBlock_801C9C8);
    }

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

/* ── sub_801CA80 ── */
/* sub_801CA80(r5 = background layout state):
 * For each entry in the u16 table at [r5+0x28], copy a 9x2 strip of map
 * tiles. Entry i gives a destination column as j = 0x1E - entry; a value of
 * 0xFF marks the end of the table. The tile ids for strip i are two
 * interleaved runs of consecutive ids starting at 0xD1DA + 9*i (row 0 takes
 * the even ids, row 1 the odd ids); they are built in a stack buffer and
 * handed to CopyBackgroundTiles for tile block 32x32 #0. The row counter i
 * runs over 0,2,4,6, i.e. at most four strips.
 *
 * The original reserves 20 bytes for the saved registers before its 0x24-byte
 * local buffer, and the buffer address escapes to CopyBackgroundTiles, so both
 * reservations are kept. r4-r7 are clobbered as call arguments but restored,
 * as are the flags from the last CMP on whichever path exits. */
enum { STRIP_BASE_TILE = 0xD1DA }; /* first tile id used by the strips */

Next sub_801CA80(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    SP -= 20;                  /* push {r4-r7,lr} frame: the buffer sits below it */
    u32 buf = SP -= 0x24;      /* 2 rows x 9 halfword tile ids */
    u32 entryPtr = R5 + 0x28;  /* table of destination columns */

    u32 i = 0;
    R1 = i;
    for (;;) {
        R2 = LD16(entryPtr);
        SET_FLAGS_CMP(R2, 0xFF);
        if (R2 == 0xFF)        /* end of table */
            break;

        /* Build the id table: row 0 counts up from the base id, row 1 from base+1. */
        u32 baseTile = STRIP_BASE_TILE + 9 * i;
        for (u32 k = 0; k < 9; k++) {
            ST16(buf + 2 * k, baseTile + 2 * k);
            ST16(buf + 0x12 + 2 * k, baseTile + 1 + 2 * k);
        }

        /* push {r1}: keep the row counter alive across the call */
        SP -= 4;
        ST32(SP, R1);

        R0 = 0x1E - R2;        /* j: destination column */
        R2 = 0;                /* tile block 32x32 */
        R3 = buf;              /* tile ids */
        R4 = 9;                /* j size */
        R5 = 2;                /* i size */
        GUEST_CALL(CopyBackgroundTiles);

        R1 = LD32(SP);         /* pop {r1} */
        SP += 4;

        i += 2;
        entryPtr += 2;
        R1 = i;
        SET_FLAGS_CMP(R1, 8);
        if (i == 8)
            break;
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    SP += 0x24 + 20;
    RETURN_TO_CALLER();
}

/* ── battle_updateObjectAnim_801CB38 ── */
/* Object animation state-machine offsets */
#define OBJ_PREV_FRAME       0x0D
#define OBJ_FRAME            0x0E
#define OBJ_ANIM_STATE       0x10
#define OBJ_PREV_ANIM_STATE  0x11
#define OBJ_ANIM_COUNTER     0x13
#define OBJ_ANIM_LOCKED      0x14
#define OBJ_FLAG_16          0x16
#define OBJ_FLAG_17          0x17
#define OBJ_BUSY_FLAG        0x4C

/* VRAM and buffer addresses */
#define VRAM_OBJ_TILE        0x6017680u
#define VRAM_OBJ_TILE_ALT    0x6017780u
#define TILE_BANK_SIZE       0x100u
#define TILE_ROW_SIZE        0x80u
#define PATTERN_SIZE         0x20u

/* register form: calls battleObject_resolveEmotionPalette_801E6A8 outside this file, which has no typed C interface yet */
static void compute_advance_frame(u32 *state, u32 *frame) {
    GUEST_CALL(battleObject_resolveEmotionPalette_801E6A8);
    *state = R0;
    *frame = R1;
}

/* register form: calls sub_801E69C outside this file, which has no typed C interface yet */
static void compute_hold_frame(u32 *state, u32 *frame) {
    GUEST_CALL(sub_801E69C);
    *state = R0;
    *frame = R1;
}

/* register form: calls battle_shouldUseSimpleAnimMode_801D814 outside this file, which has no typed C interface yet */
static bool gfx_transfer_done(void) {
    GUEST_CALL(battle_shouldUseSimpleAnimMode_801D814);
    return R0 != 0;
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx_transfer_sub_801CB38(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/*
 * Advance the sprite/animation state of a battle object pointed to by obj,
 * then queue the corresponding tile-bitmap and pattern transfers to VRAM.
 *
 * The state machine tracks: anim_state (which animation), frame (which cell),
 * and a countdown counter that gates frame changes.  When the counter's bit 1
 * is set, alternate prev/prev_frame fields are consulted.
 */
static void update_object_animation(u32 obj) {
    u32 state, frame;
    u32 counter_at_check = 0; /* mirrors r7: snapshot of counter used for frame selection */

    u8 counter = LD8(obj + OBJ_ANIM_COUNTER);

    if (counter == 0xFF) {
        /* Uninitialised: reset and seed a fresh animation sequence */
        ST8(obj + OBJ_ANIM_COUNTER, 0);
        compute_advance_frame(&state, &frame);
        ST8(obj + OBJ_ANIM_STATE, state);
        ST8(obj + OBJ_PREV_ANIM_STATE, state);
        ST8(obj + OBJ_FRAME, frame);
        ST8(obj + OBJ_PREV_FRAME, frame);
    } else if (LD8(obj + OBJ_BUSY_FLAG) != 0) {
        /* Busy flag active: hold current stored animation */
        state = LD8(obj + OBJ_BUSY_FLAG);
        frame = LD8(obj + OBJ_FRAME);
    } else {
        /* Normal update: pick next state from sub-state info */
        if (LD8(obj + OBJ_FLAG_17) == 0 || LD8(obj + OBJ_FLAG_16) != 0) {
            compute_hold_frame(&state, &frame);
        } else {
            compute_advance_frame(&state, &frame);
        }

        if (LD8(obj + OBJ_ANIM_LOCKED) != 0) {
            ST8(obj + OBJ_ANIM_STATE, state);
        } else {
            u8 prev_state = LD8(obj + OBJ_ANIM_STATE);
            if (state != prev_state) {
                ST8(obj + OBJ_PREV_ANIM_STATE, prev_state);
                ST8(obj + OBJ_PREV_FRAME, LD8(obj + OBJ_FRAME));
                ST8(obj + OBJ_ANIM_STATE, state);
                ST8(obj + OBJ_FRAME, frame);
                ST8(obj + OBJ_ANIM_COUNTER, 0xC);
            } else if (frame != LD8(obj + OBJ_FRAME)) {
                ST8(obj + OBJ_PREV_FRAME, LD8(obj + OBJ_FRAME));
                ST8(obj + OBJ_FRAME, frame);
            }
        }
    }

    /* Countdown and alternate-state selection (skipped for terminal states 3,5+) */
    if ((s32)state < 5 && state != 3) {
        counter = LD8(obj + OBJ_ANIM_COUNTER);
        counter_at_check = counter;
        if (counter != 0) {
            ST8(obj + OBJ_ANIM_COUNTER, counter - 1);
            if (counter & 2) {
                state = LD8(obj + OBJ_PREV_ANIM_STATE);
            }
        }
    }

    /* Transfer state tile-sheet to VRAM at 0x6017680 */
    u32 tile_src = LD32(A_off_801CD08 + (state << 2));
    queue_gfx_transfer_sub_801CB38(tile_src, VRAM_OBJ_TILE, TILE_BANK_SIZE);

    /* Transfer cell-pattern tile to VRAM at 0x6017780 */
    if (gfx_transfer_done()) {
        u32 cell = LD8(obj + OBJ_FRAME);
        if (counter_at_check & 2) {
            cell = LD8(obj + OBJ_PREV_FRAME);
        }
        u32 src = A_dword_872E994 + (0xA - cell) * TILE_ROW_SIZE;
        queue_gfx_transfer_sub_801CB38(src, VRAM_OBJ_TILE_ALT, TILE_ROW_SIZE);
    } else {
        queue_gfx_transfer_sub_801CB38(A_dword_872D914, VRAM_OBJ_TILE_ALT, TILE_ROW_SIZE);
    }

    /* Transfer character/pattern row to SRAM work buffer (0x30016D0) */
    u32 pattern_src = A_dword_872F114 + (state << 5);
    if (LD8(obj + OBJ_ANIM_STATE) == 3) {
        u8 c = LD8(obj + OBJ_ANIM_COUNTER);
        if (c != 0) {
            ST8(obj + OBJ_ANIM_COUNTER, c - 1);
            if (c & 2) {
                pattern_src = A_byte_801CD80;
            }
        }
    }
    queue_gfx_transfer_sub_801CB38(pattern_src, A_byte_30016D0, PATTERN_SIZE);
}

/* battle_updateObjectAnim_801CB38(obj: r5) -- updates animation state and queues GFX transfers */
Next battle_updateObjectAnim_801CB38(void) {
    u32 saved_r0 = R0;
    update_object_animation(R5);
    R0 = saved_r0;
    RETURN_TO_CALLER();
}

/* ── palette_QueueObjectPalettePair_801CDEC ── */
/* Palette-transfer destination descriptors. The object's byte at +0x12 is
 * shifted into bits 16-23, filling the low half of each descriptor and thus
 * selecting the slot within the target. */
#define XFER_DST_A 0x80004012u
#define XFER_DST_B 0x40200012u
/* Palette-data blobs in ROM passed to the transfer helper. */
#define XFER_DATA_A 0xCBB4u
#define XFER_DATA_B 0xCBBCu

/* palette_QueueObjectPalettePair_801CDEC(r5 = object):
 * Queue two palette transfers for the object (dst A + data A, then dst B +
 * data B), with the slot index taken from the object's byte at +0x12.
 * Objects whose state byte at +0x0F is 5 or 6 are skipped: they still return
 * that byte in r0 (the load for the comparison lands in r0), with the flags
 * from the corresponding CMP. Otherwise r0-r3 end as the second callee left
 * them, and the flags are whatever it leaves. r4-r6 are preserved. */
Next palette_QueueObjectPalettePair_801CDEC(void) {
    /* The original saves r4-r6 on the stack; the callees are only guaranteed
     * to keep r5/r6, so keep our own copies to restore before returning. */
    u32 inR4 = R4, inR5 = R5, inR6 = R6;

    u32 state = LD8(R5 + 0xF);
    R0 = state; /* the comparison load leaves the state byte in r0 */

    if (state == 6) {
        SET_FLAGS_CMP(state, 6);
        RETURN_TO_CALLER();
    }
    if (state == 5) {
        SET_FLAGS_CMP(state, 5);
        RETURN_TO_CALLER();
    }

    u32 slot = (u32)LD8(R5 + 0x12) << 16;

    R2 = 0;
    R3 = 0;

    R0 = XFER_DST_A + slot;
    R1 = XFER_DATA_A;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);

    /* r2/r3 are not re-zeroed here: the original passes on whatever the
     * first call left in them. */
    R0 = XFER_DST_B + slot;
    R1 = XFER_DATA_B;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);

    R4 = inR4;
    R5 = inR5;
    R6 = inR6;
    RETURN_TO_CALLER();
}

/* ── sub_801CE28 ── */
/* sub_801CE28: Advance a generic state-machine step counter (byte_2036840+0x7),
 * then dispatch to the current mode's tick handler via a 4-entry jump table.
 *
 * State block at byte_2036840:
 *   +0x00  mode (table byte-offset: 0, 4, 8, or 12)
 *   +0x07  step counter
 *   +0x08  sub-phase (hold indicator during text printing)
 *
 * Hold: when the counter would reach 5, mode==4 (chatbox printing), and
 * sub-phase is 2 or 4, the counter is NOT written (machine stalls).
 *
 * The handler is called as a sub-routine (bx with lr=epilogue); after it
 * returns, we restore r5 (preserved) and return to our caller.
 */
Next sub_801CE28(void) {
    u32 stateBase = A_byte_2036840;
    u32 savedR5 = R5;  /* r5 is preserved for our caller */

    u32 nextStep = (u32)LD8(stateBase + 0x7) + 1;

    /* Stall the counter during a text-printing hold */
    bool stall = false;
    if (nextStep == 5 && LD8(stateBase) == 4) {
        u8 subPhase = LD8(stateBase + 0x8);
        if (subPhase == 2 || subPhase == 4)
            stall = true;
    }

    if (!stall)
        ST8(stateBase + 0x7, nextStep);

    /* Dispatch: call the handler indexed by mode byte */
    R0 = LD8(stateBase);   /* handler reads mode from r0 */
    R5 = stateBase;        /* handler reads state pointer from r5 */
    u32 handler = LD32(A_off_801CE5C + R0);  /* thumb-mode target */
    GUEST_CALL_ADDR(handler);

    R5 = savedR5;  /* restore preserved register (mirrors pop {r5,pc}) */

    RETURN_TO_CALLER();
}

/* ── screenFadeOutStepTick_801CE6C ── */
/* screenFadeOutStepTick_801CE6C: tick the staged fade driven by the work block at R5.
 *
 * work[7] counts the steps and work[6] is a per-block parameter. Each tick
 * calls sprite_setAffineTransform_802FE7A(0, 0x40, 0xE0 - 0x20 * step, work[6]); the third
 * argument shrinks by 0x20 per step, so the effect fades out in stages.
 * Once the step counter reaches 5 the block moves to state 4 (work[0])
 * and the counter is cleared.
 *
 * On early ticks r0 = step counter; on the finishing tick r0 = 0 and
 * r1..r3 are whatever sprite_setAffineTransform_802FE7A left. Flags: the original ends with
 * `cmp r0,#5` (c/v and, on the early-exit path, nz) followed by a
 * flag-setting `movs r0,#0` on the finishing path, so c/v always come
 * from the comparison and nz is overridden to "zero" only when the
 * counter reached 5. */
Next screenFadeOutStepTick_801CE6C(void) {
    u32 counter = LD8(R5 + 7);

    R0 = 0;
    R1 = 0x40;
    R2 = 0xE0 - counter * 0x20;
    R3 = LD8(R5 + 6);
    GUEST_CALL(sprite_setAffineTransform_802FE7A);

    counter = LD8(R5 + 7);
    if ((s32)counter < 5) {
        R0 = counter;
        SET_FLAGS_CMP(counter, 5);
    } else {
        ST8(R5, 4);        /* state = 4 */
        R0 = 0;
        ST8(R5 + 7, 0);    /* step counter = 0 */
        /* cmp then movs r0,#0: c/v from the comparison, nz from the mov. */
        SET_FLAGS_CMP(counter, 5);
        SET_FLAGS_NZ(0);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_stepTextPrinter_801CE92 ── */
/* chatbox_stepTextPrinter_801CE92(r5 = message window object):
 * Pick the text-advance delay for the current message state (field +7) and
 * start the text printer with sprite_setAffineTransform_802FE7A. Once the state reaches 0x30 the
 * message sequence is over: the object moves to state 8 and field +7 is
 * cleared. States other than 1/2/3/0x2E/0x2F/0x30 skip the printer call.
 * On return r0 = field +7 and the flags reflect CMP r0, #0x30 (or the final
 * "mov r0, #0" when the state was reset). */
Next chatbox_stepTextPrinter_801CE92(void) {
    switch (LD8(R5 + 7)) {
    case 1:
    case 0x2F:
        R2 = 0x34;
        break;
    case 2:
    case 0x2E:
        R2 = 0x38;
        break;
    case 3:
    case 0x30:
        R2 = 0x40;
        break;
    default:
        goto advance_state;
    }

    /* Start the text printer: r0 = 0, r1 = 0x40, r2 = delay, r3 = field +6. */
    R0 = 0;
    R1 = 0x40;
    R3 = LD8(R5 + 6);
    GUEST_CALL(sprite_setAffineTransform_802FE7A);

advance_state:
    /* Reload the message state into r0 (the original always ends with it there). */
    R0 = LD8(R5 + 7);
    SET_FLAGS_CMP(R0, 0x30);
    if ((s32)R0 < 0x30)
        RETURN_TO_CALLER();

    /* Message finished: switch the object to state 8, reset the message state. */
    ST8(R5, 8);
    R0 = 0;
    ST8(R5 + 7, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── flags_clearWordSlicesTaskStep_801CED2 ── */
/* flags_clearWordSlicesTaskStep_801CED2(r5 = task state struct):
 * One step of a multi-frame flag-clearing task. The struct holds:
 *   +6: flag word index the task operates on
 *   +7: step counter (each step covers a 0x20-bit slice, first at bit 0x40)
 * Each call clears this step's 0x40-bit slice of the flag word via
 * sprite_setAffineTransform_802FE7A, then re-reads the counter: once it reaches 5 the task's
 * flag word bit is cleared and state[0] is set to 0xC (task done).
 * Leaves r0-r3 and the flags exactly as the last helper/compare does. */
Next flags_clearWordSlicesTaskStep_801CED2(void) {
    /* Clear this step's slice of the flag word:
     * sprite_setAffineTransform_802FE7A(0, 0x40, 0x40 + step * 0x20, flagWordIndex). */
    R0 = 0;
    R1 = 0x40;
    R2 = 0x40 + LD8(R5 + 7) * 0x20;
    R3 = LD8(R5 + 6);
    GUEST_CALL(sprite_setAffineTransform_802FE7A);

    /* Re-read the counter to see if every slice has been cleared. */
    R0 = LD8(R5 + 7);
    SET_FLAGS_CMP(R0, 5);
    if ((s32)R0 >= 5) {
        /* All slices done: clear the task's flag word bit... */
        R0 = LD8(R5 + 6);
        GUEST_CALL(flags_clearFlagWordBit_802FE6A);

        /* ...and mark the task finished (state[0] = 0xC, r0 = 0xC). */
        R0 = 0xC;
        ST8(R5, R0);
        SET_FLAGS_NZ(R0);
    }
    /* More steps remain: return with r0 = counter, flags from the compare. */
    RETURN_TO_CALLER();
}

/* ── event_raiseFlag8000AndDispatch_801CEFA ── */
/* event_raiseFlag8000AndDispatch_801CEFA: raise the 0x8000 flag via flags_clearBits_eStruct2035280_801BED6, then hand the same
 * flag to dispatch_801DACC so the pending event/transition takes effect. */
Next event_raiseFlag8000AndDispatch_801CEFA(void) {
    R0 = 1u << 15;                  /* event flag 0x8000 */
    GUEST_CALL(flags_clearBits_eStruct2035280_801BED6);

    R0 = 1u << 15;
    GUEST_CALL(dispatch_801DACC);

    RETURN_TO_CALLER();
}

/* ── state_dispatchHandler_801CF0E ── */
/* state_dispatchHandler_801CF0E: state-machine dispatcher for the block at byte_2036840.
 *
 * byte_2036848 holds the current state index; the matching handler from the
 * ROM table at off_801CF28 is tail-called with:
 *   r0 = handler address (as loaded from the table, thumb bit set)
 *   r1 = &off_801CF28 (the table itself)
 *   r7 = &byte_2036840 (the state block the handlers operate on)
 * and the caller's r7 is restored afterwards (the original pushes/pops it
 * around the call). Control never comes back here: the handlers return
 * straight to our caller, so this behaves like a tail call. */
Next state_dispatchHandler_801CF0E(void) {
    u32 callerR7 = R7;

    u32 state = LD8(A_byte_2036840 + 8);
    u32 tableOffset = state * 4;

    /* Condition flags exactly as the original's `lsl r0, r0, #2` leaves them:
     * nz from the shifted index, carry out of bit 30, v untouched. */
    SET_FLAGS_NZ(tableOffset);
    cpu.c = (state >> 30) & 1;

    u32 handler = LD32(A_off_801CF28 + tableOffset);

    /* Registers as the tail-called handler sees them. */
    R0 = handler;
    R1 = A_off_801CF28;
    R7 = A_byte_2036840;

    /* off_801CF28: entries 0-2 share sub_801CF3C, 3 is sub_801CF9E,
     * 4 is sub_801D048. */
    switch (state) {
    case 0:
    case 1:
    case 2:
        GUEST_CALL(sub_801CF3C);
        break;
    case 3:
        GUEST_CALL(sub_801CF9E);
        break;
    case 4:
        GUEST_CALL(sub_801D048);
        break;
    }

    /* pop {r7, pc}: hand our caller back its r7 and return to it. */
    R7 = callerR7;
    RETURN_TO_CALLER();
}

/* ── sub_801CF3C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5, r6, r7; bool z, n, c, v; } call_sub_802FE28_result_sub_801CF3C;
/* register form: generated wrapper: sprite_queueOamEntryThunk_802FE28 has no typed C interface yet */
static inline call_sub_802FE28_result_sub_801CF3C call_sub_802FE28_sub_801CF3C(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(sprite_queueOamEntryThunk_802FE28);
    return (call_sub_802FE28_result_sub_801CF3C){ R0, R4, R5, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Tile/BG-map DMA fill driver, parameter block at r7:
 *   r7+2: high byte of tile column     r7+3: low byte of tile column
 *   r7+4: high byte of BG-map column   r7+5: low byte of BG-map column
 *   r7+6: fill control byte (bits 31:25 of source address)
 *   r7+8: if 1, additionally queue one BG-map fill
 * Tile fill is split into 5 sub-fills (count += 8, addr += 0x20<<16 each). */

#define TILE_SRC_BASE     0x80004100u
#define TILE_FILL_FIRST   0xB370u
#define TILE_ROW_ADVANCE  (0x20u << 16)
#define TILE_FILL_STEP    8
#define TILE_FILL_ITERS   5
#define BGMAP_SRC_BASE    0x40000100u
/* 32-bit LDR from word_801CF9C (a .hword 0xB398 definition): reads 4 bytes
 * and yields 0xB570B398 because the preceding dword_801CF94 (0xB370)
 * supplies the upper halfword on little-endian GBA memory layout. */
#define BGMAP_FILL        0xB570B398u

/* Compose fill source address: base | (hi_byte << 16) | lo_byte | (control << 25) */
static u32 build_fill_src(u32 r7, u32 base, u32 hi_off, u32 lo_off) {
    return base
         | ((u32)LD8(r7 + hi_off) << 16)
         |  (u32)LD8(r7 + lo_off)
         | ((u32)LD8(r7 + 6) << 25);
}

/* Run the five tile fills and optionally the BG-map fill.
 * Returns the final r1 (fill count) left at function exit. */
static u32 queue_map_fills(u32 r7) {
    u32 addr  = build_fill_src(r7, TILE_SRC_BASE, 2, 3);
    u32 count = TILE_FILL_FIRST;

    for (int i = 0; i < TILE_FILL_ITERS; i++) {
        call_sub_802FE28_sub_801CF3C(addr, count, 0, 0);
        addr  += TILE_ROW_ADVANCE;
        count += TILE_FILL_STEP;
    }

    if (LD8(r7 + 8) != 1)
        return count;  /* 0xB398 after five +8 increments */

    /* Conditional BG-map fill */
    u32 bg_addr = build_fill_src(r7, BGMAP_SRC_BASE, 4, 5);
    call_sub_802FE28_sub_801CF3C(bg_addr, BGMAP_FILL, 0, 0);
    return BGMAP_FILL;  /* r1 = 0xB570B398, preserved across the call */
}

/* sub_801CF3C(r7 = parameter block) -> r1, r2 = 0, r3 = 0 */
Next sub_801CF3C(void) {
    R1 = queue_map_fills(R7);
    R2 = 0;
    R3 = 0;
    RETURN_TO_CALLER();
}

/* ── battle_sceneFrameSetup_801DA24 ── */
/* battle_sceneFrameSetup_801DA24: battle-scene per-frame setup.
 * - stores 0x1F09 into the render info block's Unk_0a halfword,
 * - queues the GFX transfers for the battle scene's data list (off_801ECB4),
 * - runs the player object pre-update and records the returned object
 *   pointer into the battle state at dword_20352C8 (eStruct2035280 + 0x48). */
Next battle_sceneFrameSetup_801DA24(void) {
    /* r10 holds the toolkit base; the render info pointer sits at offset 8
     * (oToolkit_RenderInfoPtr in the disassembly). */
    u32 renderInfo = LD32(R10 + 8);

    ST16(renderInfo + oRenderInfo_Unk_0a, 0x1F09);

    /* dataList (off_801ECB4) */
    R0 = A_off_801ECB4;
    GUEST_CALL(QueueGFXTransfersInList);

    GUEST_CALL(battle_preUpdateAndGetPlayerObject_80103EC);

    ST32(A_eStruct2035280 + 0x48, R0); /* dword_20352C8 */

    RETURN_TO_CALLER();
}

/* ── sub_801DB54 ── */
/* sub_801DB54: call flags_clearBits_20352C4_801BEC2 with r0 = 1 (flags and r0-r2 come from the callee). */
Next sub_801DB54(void) {
    R0 = 1;
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── battle_setUnkFlagBit0_eStruct2035280_801DB60 ── */
/* battle_setUnkFlagBit0_eStruct2035280_801DB60: invoke some game action with argument 1.
 * Sets R0 = 1 and tail-calls flagSetBits_eStruct2035280_801BEB8; all outputs (r0-r2, flags)
 * come from that callee. */
Next battle_setUnkFlagBit0_eStruct2035280_801DB60(void) {
    /* mov r0, #1; lsl r0, r0, #0 (identity shift, value stays 1) */
    R0 = 1;

    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);

    /* pop {pc}: return to the caller with the callee's results intact */
    RETURN_TO_CALLER();
}

/* ── sub_801DB6C ── */
/* sub_801DB6C: call flags_clearBits_20352C4_801BEC2 with r0 = 2, built as
 *     mov r0, #1 ; lsl r0, r0, #1
 * flags_clearBits_20352C4_801BEC2 performs no flag-setting instruction on its return path,
 * so the flags the caller sees are the ones that lsl leaves behind:
 * result 2 gives N=0, Z=0, and the carry out of a 1-bit left shift of 1
 * (old bit 31) is 0. */
Next sub_801DB6C(void) {
    R0 = 2;              /* 1 << 1 */
    SET_FLAGS_NZ(R0);    /* N=0, Z=0 from the shifted result */
    cpu.c = 0;           /* carry out of lsl r0, #1: bit 31 of 1 was clear */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801DB78 ── */
/* sub_801DB78: wrapper that calls flagSetBits_eStruct2035280_801BEB8 with r0 = 2.
 * The value is built as "mov r0,#1; lsl r0,#1", and the LSL is a
 * flag-setting instruction: it loads the carry flag with the bit
 * shifted out of bit 31 (0 here). flagSetBits_eStruct2035280_801BEB8's reported flags depend
 * on the incoming carry, so this must be reproduced exactly. */
Next sub_801DB78(void) {
    R0 = 1;
    SET_FLAGS_ADD(R0, R0);   /* same flags as "lsl r0, r0, #1" on 1: N=0, Z=0, C=0 */
    R0 = R0 + R0;            /* r0 = 2 */
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── battleRefreshTrackedObjectArmPos_801DBD4 ── */
/* Object position table: six 8-byte slots at 0x20352E0 (dword_20352E0,
 * addressed here through the eStruct2035280 Size field symbol).  Each slot:
 *   +0  active flag
 *   +1  stored x position
 *   +2  stored y position
 *   +4  pointer to the battle object being tracked                    */
#define POS_TABLE  A_dword_20352E0

/* battleRefreshTrackedObjectArmPos_801DBD4(r5 = battle object):
 * Find the table slot that tracks this battle object and refresh its
 * stored arm position (x, and -y - 8) from the object's sprite data.
 * R0 = 1 (flags as described below) if no slot tracks the object. */
Next battleRefreshTrackedObjectArmPos_801DBD4(void) {
    u32 slot = POS_TABLE;

    for (R3 = 6; R3 != 0; R3--) {
        R2 = LD8(slot);
        if (R2 != 0 && LD32(slot + 4) == R5) {
            /* Found the slot: derive the arm position from the object. */
            R0 = LD16(R5 + oBattleObject_NameID);
            R1 = 3;
            R2 = LD8(R5 + oBattleObject_Alliance);
            R3 = LD8(R5 + oBattleObject_DirectionFlip);
            GUEST_CALL(getBattleArmPositionMaybe_8018810);

            /* R0 = x; negate the y result and raise it by 8 pixels. */
            u32 flippedY = 0 - R1;      /* neg r1, r1 */
            R1 = flippedY - 8;          /* sub r1, #8 */
            SET_FLAGS_CMP(flippedY, 8);
            ST8(slot + 1, R0);
            ST8(slot + 2, R1);
            RETURN_TO_CALLER();
        }
        slot += 8;
    }

    /* No slot tracks this object: R0 = 1.  The original leaves N/Z clear
     * from `mov r0,#1` and C set / V clear from the loop's final
     * `subs r3,#1`; CMP 1,0 reproduces that exact flag state.  R1 keeps
     * its incoming value, R2 the last slot's flag byte and R3 the
     * exhausted counter, as callers may observe. */
    R0 = 1;
    SET_FLAGS_CMP(1, 0);
    RETURN_TO_CALLER();
}

/* ── battleObject_setCustomGaugeBarAnimate_801DC06 ── */
/* battleObject_setCustomGaugeBarAnimate_801DC06(r0 = value byte, r1 = amount byte, r5 = id):
 * Scans the 6-slot registry at dword_20352E0 (8 bytes per slot: +0 "in use"
 * flag, +4 id word) for an active slot whose id matches r5.  On a hit, stores
 * r0 at slot+1 and r1-8 at slot+2, and returns with r0 = value, r1 = amount-8.
 * If no slot matches, returns r0 = 1.  r4/r6/r7 are preserved (pushed/popped);
 * r2 ends as the last "in use" byte read and r3 as the remaining-slot count
 * (0 when the scan ran out of slots). */
Next battleObject_setCustomGaugeBarAnimate_801DC06(void) {
    u32 value = R0;   /* byte written to slot+1 on a hit */
    u32 amount = R1;  /* slot+2 receives amount - 8 */
    u32 id = R5;      /* id being looked up */
    u32 slot = A_dword_20352E0;
    u32 remaining = 6;

    for (;;) {
        u32 in_use = LD8(slot);
        R2 = in_use;      /* original keeps the last flag byte read in r2 */
        R3 = remaining;   /* ...and the not-yet-checked slot count in r3 */
        if (in_use != 0 && LD32(slot + 4) == id)
            break;

        slot += 8;
        SET_FLAGS_CMP(remaining, 1); /* "subs r3, #1" */
        remaining--;
        R3 = remaining;
        if (remaining == 0) {
            /* No matching slot: return 1 ("mov r0, #1" only refreshes nz) */
            R0 = 1;
            SET_FLAGS_NZ(1);
            RETURN_TO_CALLER();
        }
    }

    /* Found: patch the slot's value/amount bytes */
    ST8(slot + 1, value);
    SET_FLAGS_CMP(amount, 8); /* "subs r1, #8" */
    ST8(slot + 2, amount - 8);
    R0 = value;
    R1 = amount - 8;
    RETURN_TO_CALLER();
}

/* ── releaseSlotByValue_801DC36 ── */
/* releaseSlotByValue_801DC36(r5 = entry value to release):
 * Searches the six 8-byte slots of the table at dword_20352E0 (each slot is a
 * u8 "in use" flag followed by a u32 value) for the first occupied slot whose
 * value equals r5, and clears that slot. Leaves r0 = 0 if a slot was cleared,
 * r0 = 1 if none matched. r2 ends up holding the last flag byte read and r3
 * the number of slots that were still unexamined. The c/v flags come from the
 * last comparison executed: the value comparison on a match, the final
 * "subs r3, #1" (1 - 1) when the table is exhausted. */
Next releaseSlotByValue_801DC36(void) {
    u32 slot = A_dword_20352E0; /* == dword_20352E0, start of the slot table */
    u32 slotsLeft = 6;

    while (1) {
        R2 = LD8(slot);
        if (R2 != 0) {
            u32 value = LD32(slot + 4);
            if (value == R5) {
                /* Found it: flag the comparison for the caller, then clear the slot. */
                SET_FLAGS_CMP(value, R5);
                ST32(slot, 0);
                ST32(slot + 4, 0);
                R0 = 0;
                R3 = slotsLeft;
                RETURN_TO_CALLER();
            }
        }

        /* Move to the next slot and count down; the flags of the final
         * decrement (1 - 1) are what the caller sees when nothing matched. */
        slot += 8;
        SET_FLAGS_CMP(slotsLeft, 1);
        slotsLeft--;
        if (slotsLeft == 0)
            break;
    }

    R0 = 1;
    R3 = 0;
    RETURN_TO_CALLER();
}

/* ── battle_setFlag0x04_eStruct2035280_801DC6E ── */
/* battle_setFlag0x04_eStruct2035280_801DC6E: invoke flagSetBits_eStruct2035280_801BEB8 with r0 = 4 (bit flag 1 << 2).
 *
 * The LSL that builds the argument is the last flag-setting instruction
 * before the call, and flagSetBits_eStruct2035280_801BEB8 preserves the flags, so the state it
 * leaves must survive to our caller: shifting 1 left by 2 pushes no set
 * bit out, giving N=0, Z=0, C=0 (result 4, no bits shifted out).
 * SET_FLAGS_ADD(1, 3) reproduces exactly those flags (sum 4, no carry). */
Next battle_setFlag0x04_eStruct2035280_801DC6E(void) {
    R0 = 1u;
    R1 = 2u;
    R0 = R0 << R1;          /* r0 = 1 << 2 = 4 */
    SET_FLAGS_ADD(1u, 3u);  /* N=0, Z=0, C=0 — same flags as the LSL */

    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── sub_801DC7C ── */
/* ---------------------------------------------------------------------------
 * Object status list: four 0x14-byte slots at byte_203EB50.  A slot is free
 * when its kind byte is zero; a claimed slot caches a battle object's name
 * ID and HP (shown by the battle UI) together with the object pointer.
 * ------------------------------------------------------------------------- */
#define STATUS_LIST       A_byte_203EB50
#define STATUS_SLOTS      4
#define STATUS_SLOT_SIZE  0x14

/* fields of one 0x14-byte slot */
#define oStatus_Kind      0x0   /* u8:  0 = free, else a STATUS_KIND_* value */
#define oStatus_HP        0x2   /* u16: object's current HP */
#define oStatus_Param0    0x4   /* u8:  caller byte passed in r0 */
#define oStatus_Param1    0x5   /* u8:  caller byte passed in r1 */
#define oStatus_NameID    0x6   /* u16: object's name ID */
#define oStatus_Unk08     0x8   /* u8:  cleared when the slot is claimed */
#define oStatus_Unk09     0x9   /* u8:  cleared when the slot is claimed */
#define oStatus_Object    0xC   /* u32: owning battle object */
#define oStatus_Unk10     0x10  /* u32: 0x0B0B0B0B stamp written on claim */

#define STATUS_KIND_FREE    0
#define STATUS_KIND_NORMAL  3
#define STATUS_KIND_SPECIAL 0x1B

#define STATUS_UI_REQUEST  4   /* request bit that refreshes the status readouts */

/* register form: calls eStruct2035280_SetRequestBits_801BECC outside this file, which has no typed C interface yet */
static u32 request_status_ui_refresh(u32 bits) {
    R0 = bits;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);
    return R2;    /* the callee's leftover r2; the original's caller sees it in r2 */
}

/* Everything sub_801DC7C leaves in its output registers.  r2, r3 and the c/v
 * flags differ per path, so the typed function reports them for the adapter.
 * r1 and lr are not here: they are either untouched or pure callee leftovers
 * that the GUEST_CALL above already left sitting in the guest registers. */
typedef struct {
    u32 added;   /* r0: 0 = slot claimed, 1 = already listed or list full */
    u32 r2;      /* duplicate/full: object field of the slot the scan stopped on;
                    claim: the SetRequestBits callee's leftover r2 */
    u32 r3;      /* duplicate/full: slots left in the scan; claim: the new kind */
    u32 cmp_a;   /* the adapter runs SET_FLAGS_CMP(cmp_a, cmp_b) to restore c/v */
    u32 cmp_b;
} AddStatusResult;

/* Claim a status slot for `object` (guest address of a battle object),
 * storing the two caller bytes beside it.  The first free slot wins; if the
 * object already owns a slot, or all four slots are taken, nothing is
 * written.  A successful claim pokes request bit 4 so the UI rebuilds. */
static AddStatusResult status_list_add(u32 param0, u32 param1, u32 object)
{
    AddStatusResult out;
    u32 slot = STATUS_LIST;
    u32 slots_left = STATUS_SLOTS;   /* the countdown the original keeps in r3 */
    u32 owner = 0;                   /* object field of the slot the scan stops on */
    bool duplicate = false;

    while (LD8(slot + oStatus_Kind) != STATUS_KIND_FREE) {
        owner = LD32(slot + oStatus_Object);
        if (owner == object) {       /* the object already has a slot */
            duplicate = true;
            break;
        }
        slot += STATUS_SLOT_SIZE;
        slots_left--;
        if (slots_left == 0)         /* all four slots are in use */
            break;
    }

    if (duplicate || slots_left == 0) {
        out.added = 1;
        out.r2 = owner;              /* the slot's object field the scan stopped on */
        out.r3 = slots_left;         /* 0 when the list was full */
        out.cmp_a = 0;               /* last flag op was an equal cmp / a 1-1 sub: C=1, V=0 */
        out.cmp_b = 0;
        return out;
    }

    u16 name_id = LD16(object + oBattleObject_NameID);
    u8 kind = STATUS_KIND_NORMAL;
    if (name_id == 0xEB || (name_id >= 0x49 && name_id <= 0x4E))
        kind = STATUS_KIND_SPECIAL;  /* a few special name IDs use the other kind */

    ST16(slot + oStatus_NameID, name_id);
    ST8(slot + oStatus_Kind, kind);
    ST8(slot + oStatus_Param0, param0);
    ST8(slot + oStatus_Param1, param1);
    ST16(slot + oStatus_HP, LD16(object + oBattleObject_HP));
    ST32(slot + oStatus_Unk10, 0x0B0B0B0B);
    ST32(slot + oStatus_Object, object);
    ST8(slot + oStatus_Unk08, 0);
    ST8(slot + oStatus_Unk09, 0);

    out.added = 0;
    out.r2 = request_status_ui_refresh(STATUS_UI_REQUEST);
    out.r3 = kind;                   /* r3 still holds the kind across the call */
    out.cmp_a = name_id;             /* the name-ID compares are the last flag setters */
    out.cmp_b = 0x4E;
    return out;
}

/* sub_801DC7C(r0 = param0, r1 = param1, r5 = battle object) -> r0.
 * r1 and lr are never written back: the original returns via
 * pop {r4,r6,r7,pc}, so on a claim the caller sees the SetRequestBits
 * callee's leftover r1/lr (which the GUEST_CALL already left in place),
 * and on the other paths both registers are simply unchanged. */
Next sub_801DC7C(void) {
    AddStatusResult out = status_list_add(R0, R1, R5);
    R0 = out.added;
    R2 = out.r2;
    R3 = out.r3;
    SET_FLAGS_CMP(out.cmp_a, out.cmp_b);
    RETURN_TO_CALLER();
}

/* ── sub_801DD34 ── */
/* sub_801DD34(r5 = search id):
 * Scan the four 0x14-byte records at byte_203EB50 and clear the first
 * record that is active (bit 0 of its flag byte set) and whose id field
 * at +0x0C matches r5.  Returns r0 = 0 when a record was cleared, or
 * r0 = 1 when no record matched.  r1 (probe bit), r2 (record pointer)
 * and r3 (records remaining) are left in the scan's final state. */
Next sub_801DD34(void) {
    u32 id = R5;
    u32 record = A_byte_203EB50;
    u32 remaining = 4;

    R1 = 1;
    while (remaining != 0) {
        R2 = record;
        R3 = remaining;

        if (LD8(record) & 1) {
            u32 recId = LD32(record + 0xC);
            SET_FLAGS_CMP(recId, id);
            if (recId == id) {
                /* matched: deactivate the record and report success */
                ST8(record, 0);
                R0 = 0;
                SET_FLAGS_NZ(0);
                RETURN_TO_CALLER();
            }
        }

        record += 0x14;
        remaining--;
    }

    /* no record matched */
    R2 = record;
    R3 = remaining;
    R0 = 1;
    /* c/v as left by the final "sub r3, #1" (1 - 1), then "mov r0, #1" sets N/Z */
    SET_FLAGS_CMP(1, 1);
    SET_FLAGS_NZ(1);
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

/* ── setupBattleBackground_801DDF6 ── */
/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx_transfer_sub_801DDF6(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet */
static void copy_bg_tiles(u32 j, u32 i, u32 block, u32 tile_ids, u32 j_size, u32 i_size) {
    R0 = j;
    R1 = i;
    R2 = block;
    R3 = tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
}

/* register form: calls flagSetBits_eStruct2035280_801BEB8 outside this file, which has no typed C interface yet */
static void set_flag_bits(u32 mask) {
    R0 = mask;
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
}

/* Load a 32-byte GFX chunk into VRAM, stamp background tiles, and enable flag bit 4. */
static void setup_battle_background(void) {
    queue_gfx_transfer_sub_801DDF6(A_dword_86E1C78, A_unk_3001A80, 0x20);
    copy_bg_tiles(6, 0, 3, A_word_801ED6C, 0x12, 2);
    set_flag_bits(0x10); /* 1 << 4 */
}

/* setupBattleBackground_801DDF6: transfers GFX, copies BG tiles, sets flag bit 4.
 * Preserves r4, r5 (clobbered by callees), plus r6, r7. */
Next setupBattleBackground_801DDF6(void) {
    u32 saved_r4 = R4;
    u32 saved_r5 = R5;
    setup_battle_background();
    R4 = saved_r4;
    R5 = saved_r5;
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

/* ── sub_801DF8C ── */
/* sub_801DF8C(r0 = value): store a halfword into word_20352A2 (the 0x22 field of
 * the eStruct2035280 struct). */
Next sub_801DF8C(void) {
    ST16(A_eStruct2035280 + 0x22, R0);
    RETURN_TO_CALLER();
}

/* ── ClearCustGauge ── */
/* ClearCustGauge: clear the custom gauge (zero its progress counter at
 * word_20352A0), then let battle_clearFlags reset the gauge-related flag
 * bits (0x12). Result registers come from battle_clearFlags. */
Next ClearCustGauge(void) {
    ST16(A_eStruct2035280 + 0x20, 0);

    R0 = 0x12;
    GUEST_CALL(battle_clearFlags);

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

/* ── battle_setUnkFlags44_0x40_801E006 ── */
/* battle_setUnkFlags44_0x40_801E006: wrapper that invokes flagSetBits_eStruct2035280_801BEB8 with the argument 0x40
 * (built as mov r0,#1; lsl r0,r0,#6).
 *
 * The immediate LSL is the last flag-setting instruction of this function
 * that the callee leaves untouched, and callers branch on these flags right
 * after the call, so its flag effects must be reproduced:
 *   result 0x40 -> N=0, Z=0; shifted-out bit 26 of 1 -> C=0.
 * SET_FLAGS_ADD(R0, 0) yields exactly that carry/N/Z combination. */
Next battle_setUnkFlags44_0x40_801E006(void) {
    R0 = 1 << 6;              /* argument for flagSetBits_eStruct2035280_801BEB8 */
    SET_FLAGS_ADD(R0, 0);     /* flags as left by `lsl r0, r0, #6` */
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
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

/* ── battle_eStruct2035280_postEvent_801E0A0 ── */
/* battle_eStruct2035280_postEvent_801E0A0(r0, r1 = two byte parameters):
 * Copies the halfword at +0x24 of the struct pointed to by the entry at
 * +0x48 of eStruct2035280 into eStruct2035280+0x24, stores the two byte
 * parameters at +0x1 and +0x2, then runs the 0x80 (text-box) update path:
 * eStruct2035280_SetRequestBits_801BECC followed by dispatch_801DA48.
 * Leaves R1/R2/R3 and the c/v flags as dispatch_801DA48 left them. */
Next battle_eStruct2035280_postEvent_801E0A0(void) {
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

/* ── setStruct2035280Field26_801E0C8 ── */
/* setStruct2035280Field26_801E0C8(r0 = value):
 * Store a halfword into word_20352A6 (field 0x26 of the eStruct2035280
 * structure). R5 is only used as a scratch pointer inside the original and
 * is restored on exit, so it must be left unchanged here. */
Next setStruct2035280Field26_801E0C8(void) {
    u32 base = A_eStruct2035280;

    ST16(base + 0x26, R0);
    RETURN_TO_CALLER();
}

/* ── event_markStepDoneAndPlayFinishEffects_801E0DC ── */
/* event_markStepDoneAndPlayFinishEffects_801E0DC(r5 = struct pointer):
 * Marks the two bytes at r5+8 and r5+0xA as done (0xFF), fires effect 6 and
 * effect 0x1A through call_sub_3005EBA (both with the same parameter set:
 * r1=2, r2=3, r3=0, r4=4, r5=2), then clears event flag bit 0x400 in the
 * flag word at 0x20352C4 via flags_clearBits_20352C4_801BEC2.
 * r4/r5 are call-saved, so they are held in locals while the fixed call
 * arguments occupy the real registers. */
Next event_markStepDoneAndPlayFinishEffects_801E0DC(void) {
    u32 savedR4 = R4;
    u32 savedR5 = R5;

    ST8(R5 + 8, 0xFF);
    ST8(R5 + 0xA, 0xFF);

    R0 = 6;
    R1 = 2;
    R2 = 3;
    R3 = 0;
    R4 = 4;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    R0 = 0x1A;
    R1 = 2;
    R2 = 3;
    R3 = 0;
    R4 = 4;
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    /* lsl r0, r0, #10 on r0 = 1: result 0x400 (N=0, Z=0), the carry out is
     * old bit 22, which is 0. flags_clearBits... leaves the flags untouched,
     * so this is the flag state the caller sees on return. */
    R0 = 1 << 10;
    SET_FLAGS_NZ(R0);
    cpu.c = 0;
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    R4 = savedR4;
    R5 = savedR5;
    RETURN_TO_CALLER();
}

/* ── battle_loadModeGfx_801E10E ── */
/* battle_loadModeGfx_801E10E: prepare the mode-specific battle gfx and raise eStruct flag 0x400.
 *
 * If the current battle mode is not 6, queue a 0x20-word gfx transfer copying
 * the dword_86E1C78 tiles into the buffer at unk_3001A80.  Afterwards set bit
 * 10 (mask 1 << 10) in the eStruct at 0x2035280 via flagSetBits.
 */
Next battle_loadModeGfx_801E10E(void) {
    GUEST_CALL(GetBattleMode);
    u32 battleMode = R0;

    if (battleMode != 6) {
        /* QueueEightWordAlignedGFXTransfer(src = dword_86E1C78,
         *                                  dst = unk_3001A80,
         *                                  size = 0x20 words) */
        R0 = A_dword_86E1C78;
        R1 = A_unk_3001A80;
        R2 = 0x20;
        GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    }

    /* Flag mask 1 << 10, computed as "lsls r0, r0, #10" in the original:
     * result 0x400, N/Z from 0x400, carry-out (bit 22 of the 1) is 0. */
    R0 = 0x400;
    SET_FLAGS_NZ(R0);
    cpu.c = 0; /* the shift pushes a zero bit out into the carry */

    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── battle_redrawCommandMenuWindow_801E1A4 ── */
/* battle_redrawCommandMenuWindow_801E1A4: redraw the battle command-menu window, then do menu upkeep.
 *
 * R5 points at the battle work struct; [R5+0xC] == 2 means the custom
 * screen is open, in which case the window frame is drawn six pixels
 * narrower (and its left edge moves accordingly); otherwise the full-size
 * frame is used. Afterwards bit 0x100 of the battle flag word at
 * 0x20352C4 is cleared, and while the battle is still running and the
 * custom screen is closed, event 0x400 is dispatched to advance the menu.
 *
 * R4-R6 are preserved; R0-R3 end as the last callee left them. On return
 * the flags are those of the last comparison performed, except on the
 * dispatch path, where dispatch_801DA48's own flags are left in place. */
Next battle_redrawCommandMenuWindow_801E1A4(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6;

    /* Frame shift: 6 while the custom screen (state 2) is open, else 0. */
    u32 shift = (LD8(R5 + 0xC) == 2) ? 6 : 0;

    R0 = 7 + (shift >> 1);  /* window left edge */
    R1 = 2;                 /* window top edge  */
    R2 = 3;
    R3 = 0;
    R4 = 0x11 - shift;      /* window width     */
    R5 = 2;
    GUEST_CALL(call_sub_3005EBA);

    R0 = 0x100;
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);

    GUEST_CALL(battle_isBattleOver);

    if (R0 == 1) {
        /* Battle over: stop here, flags as `cmp r0, #1` left them. */
        SET_FLAGS_CMP(R0, 1);
    } else if (shift != 0) {
        /* Custom screen open: stop here, flags as `cmp r6, #0` left them. */
        SET_FLAGS_CMP(shift, 0);
    } else {
        /* Battle running, normal menu: request the next menu phase. */
        R0 = 1 << 10;
        GUEST_CALL(dispatch_801DA48);
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── sub_801E1E4 ── */
/* sub_801E1E4: redraw the backdrop strip of an on-screen window object.
 * r5 points at the owning object; the byte at +0xC selects the layout:
 *   2          -> narrow strip, and the dispatcher is NOT notified
 *   anything   -> dispatcher gets flag 0x400 first, then the full-width strip
 * Afterwards bit 0x100 is set in the flag word kept for the struct at
 * 0x2035280 (marks the background as drawn/dirty). */

/* register form: calls dispatch_801DACC outside this file, which has no typed C interface yet.
 * r4-r6 are put back afterwards: the original keeps them in a single push/pop
 * frame for its whole body, so their entry values must cross every guest call. */
static void dispatch_notify(u32 flags) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6;
    R0 = flags;
    GUEST_CALL(dispatch_801DACC);
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
}

/* register form: calls CopyBackgroundTiles outside this file, which has no typed C interface yet.
 * Its last two operands (j_size, i_size) travel in r4/r5 rather than on the
 * stack, so r4-r6 are restored around the call exactly the way the original's
 * push {r4-r6} ... pop {r4-r6} frame does. */
static void copy_background_tiles_sub_801E1E4(u32 j, u32 i, u32 tile_block_32x32,
                                  const u16 *tile_ids, u32 j_size, u32 i_size) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6;
    R0 = j;
    R1 = i;
    R2 = tile_block_32x32;
    R3 = (u32)tile_ids;
    R4 = j_size;
    R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
}

/* register form: calls flagSetBits_eStruct2035280_801BEB8 outside this file, which has no typed C interface yet.
 * It leaves r3-r6 alone, and being the final guest call of the function, its
 * leftovers in r0-r3 and lr are precisely the outputs callers observe. */
static void set_flag_bits_sub_801E1E4(u32 bits) {
    R0 = bits;
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
}

/* Draw the two-row backdrop strip for the window object at `self`.
 *
 * State byte +0xC == 2 keeps the narrow strip: 8 tiles wide starting at
 * column 11, tile table word_801EE58. Any other value first hands flag 0x400
 * to dispatch_801DACC, then draws the full 17-tile strip from column 7 with
 * tile table word_801EE14. Either way, finish by setting bit 0x100 in the
 * flag word of the struct at 0x2035280. */
static void draw_window_backdrop(u32 self) {
    const u16 *tile_ids;
    u32 tiles_cut = 9;               /* narrow layout drops 9 of the 17 tiles */

    if (LD8(self + 0xC) != 2) {
        tiles_cut = 0;               /* full-width layout */
        dispatch_notify(1u << 10);   /* flag 0x400 */
        tile_ids = (const u16 *)A_word_801EE14;
    } else {
        tile_ids = (const u16 *)A_word_801EE58;
    }

    copy_background_tiles_sub_801E1E4(7 + tiles_cut / 2,  /* left edge column j    */
                          2,                  /* top edge row i        */
                          3,                  /* which 32x32 tile block */
                          tile_ids,
                          17 - tiles_cut,     /* width in tiles        */
                          2);                 /* height in tiles       */

    set_flag_bits_sub_801E1E4(1u << 8);          /* bit 0x100 */
}

/* sub_801E1E4(r5 = window object) -> r0-r3 and lr carry the last callee's
 * leftovers; r4-r12 are returned unchanged. */
Next sub_801E1E4(void) {
    draw_window_backdrop(R5);
    RETURN_TO_CALLER();
}

/* ── eStruct2035280_DrawMessageMode_801E228 ── */
/* register form: calls dispatch_801DACC outside this file, which has no typed C interface yet */
static void call_dispatch_801DACC(u32 flags) {
    R0 = flags;
    GUEST_CALL(dispatch_801DACC);
}

/* register form: calls renderTextGfx_8045F8C outside this file, which has no typed C interface yet */
static void call_renderTextGfx_8045F8C(u32 text, u32 a1, u32 dest, u32 attr) {
    R0 = text;
    R1 = a1;
    R2 = dest;
    R3 = attr;
    R4 = 0x11;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);
}

/* register form: calls eStruct2035280_SetRequestBits_801BECC outside this file, which has no typed C interface yet */
static void call_eStruct2035280_SetRequestBits_801BECC(u32 bits) {
    R0 = bits;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);
}

/* register form: calls dispatch_801DA48 outside this file, which has no typed C interface yet */
static void call_dispatch_801DA48(u32 bits) {
    R0 = bits;
    GUEST_CALL(dispatch_801DA48);
}

/* preserved are passed in and returned so the adapter can restore guest R4-R7 on exit. */
static u32 sub_801E228_logic(u32 mode, u32 r4_in, u32 r5_in, u32 r6_in, u32 r7_in) {
    if (mode == 2)
        mode = 0x13;

    u32 eStruct = A_eStruct2035280;
    u32 const bit8 = 1u << 8;

    if (LD32(eStruct + 0x40) & bit8)
        call_dispatch_801DACC(bit8);

    ST8(eStruct + 0xB, 0x3C);
    ST8(eStruct + 0xC, 0);

    call_renderTextGfx_8045F8C(A_TextScript86F0374, mode, A_byte_203C4E0, 0x6009560);

    call_eStruct2035280_SetRequestBits_801BECC(bit8);
    call_dispatch_801DA48(bit8);

    /* Mark which preserved reg to restore: pack into a single u32 via a struct out-param? */
    (void)r4_in; (void)r5_in; (void)r6_in; (void)r7_in;
    return 0;
}

Next eStruct2035280_DrawMessageMode_801E228(void) {
    u32 r4_save = R4, r5_save = R5, r6_save = R6, r7_save = R7;
    sub_801E228_logic(R0, r4_save, r5_save, r6_save, r7_save);
    R4 = r4_save;
    R5 = r5_save;
    R6 = r6_save;
    R7 = r7_save;
    RETURN_TO_CALLER();
}

/* ── sub_801E270 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r6, r8, r9, r10, r11, r12; } call_dispatch_801DACC_result_sub_801E270;
/* register form: generated wrapper: dispatch_801DACC has no typed C interface yet */
static inline call_dispatch_801DACC_result_sub_801E270 call_dispatch_801DACC_sub_801E270(u32 r0) {
    R0 = r0;
    GUEST_CALL(dispatch_801DACC);
    return (call_dispatch_801DACC_result_sub_801E270){ R6, R8, R9, R10, R11, R12 };
}

typedef struct { u32 r3, r5, r6, r7, r8, r9, r10, r11, r12; bool v; } call_renderTextGfx_8045F8C_result;
/* register form: generated wrapper: renderTextGfx_8045F8C has no typed C interface yet */
static inline call_renderTextGfx_8045F8C_result call_renderTextGfx_8045F8C_sub_801E270(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(renderTextGfx_8045F8C);
    return (call_renderTextGfx_8045F8C_result){ R3, R5, R6, R7, R8, R9, R10, R11, R12, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_eStruct2035280_SetRequestBits_801BECC_result_sub_801E270;
/* register form: generated wrapper: eStruct2035280_SetRequestBits_801BECC has no typed C interface yet */
static inline call_eStruct2035280_SetRequestBits_801BECC_result_sub_801E270 call_eStruct2035280_SetRequestBits_801BECC_sub_801E270(u32 r0) {
    R0 = r0;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);
    return (call_eStruct2035280_SetRequestBits_801BECC_result_sub_801E270){ R1, R2 };
}

typedef struct { u32 r0, r1, r2, r3, r8, r9, r10, r11, r12; } call_dispatch_801DA48_result_sub_801E270;
/* register form: generated wrapper: dispatch_801DA48 has no typed C interface yet */
static inline call_dispatch_801DA48_result_sub_801E270 call_dispatch_801DA48_sub_801E270(u32 r0) {
    R0 = r0;
    GUEST_CALL(dispatch_801DA48);
    return (call_dispatch_801DA48_result_sub_801E270){ R0, R1, R2, R3, R8, R9, R10, R11, R12 };
}

/* Window panel handler for the eStruct2035280 game state.
 *
 * If request bit 8 is pending in the state dword at offset 0x40 and the
 * "visible" flag byte (offset 0xC) is still nonzero (panel not yet
 * dismissed), dispatch the open request first.  On every path except the
 * early bail-out, set the panel timer byte to 0x3C, mark visible, render
 * the script text into the window VRAM region, register the request bit,
 * and dispatch the display update.
 *
 * Returns 0 on the early bail-out, else the final r0 from dispatch_801DA48. */
static u32 handle_window_panel(void) {
    u32 state = A_eStruct2035280;

    /* Check if engine request bit 8 (0x100) is pending */
    if (LD32(state + 0x40) & (1u << 8)) {
        /* If the visible flag is already cleared, the panel was dismissed – bail */
        if (LD8(state + 0xC) == 0)
            return 0;
        /* Notify the engine that the window-open request was processed */
        call_dispatch_801DACC_sub_801E270(1u << 8);
    }

    /* Set the panel countdown timer (byte_203528B) and re-mark as visible */
    ST8(state + 0xB, 0x3C);
    ST8(state + 0xC, 1);

    /* Render text script 86F0374 into the window VRAM area at 0x6009560 */
    call_renderTextGfx_8045F8C_sub_801E270(A_TextScript86F0374,  /* source text       */
                               0xE,                  /* render kind       */
                               A_byte_203C4E0,       /* glyph output buf  */
                               0x6009560,            /* VRAM tile dest    */
                               0x11,                 /* palette index     */
                               1,                    /* tile row count    */
                               A_dword_86B7AE0,      /* tile pattern src  */
                               0);                   /* layer (BG0)       */

    /* Register the window request bit in the engine request structure */
    call_eStruct2035280_SetRequestBits_801BECC_sub_801E270(1u << 8);

    /* Dispatch the display update – its r0 is the function's final result */
    return call_dispatch_801DA48_sub_801E270(1u << 8).r0;
}

/* sub_801E270() – window panel update, called from the main loop.
 * Preserves r4-r7 (the original push/pop); outputs r0, r2, r5, r7,
 * r8-r12 from the final dispatch call. */
Next sub_801E270(void) {
    u32 sv4 = R4, sv5 = R5, sv6 = R6, sv7 = R7;
    u32 result = handle_window_panel();
    R0 = result;
    R4 = sv4;
    R5 = sv5;
    R6 = sv6;
    R7 = sv7;
    RETURN_TO_CALLER();
}

/* ── sub_801E4B0 ── */
/* sub_801E4B0: scan the four u16 slots at +0x28 of the struct passed in r5
 * (0xFF marks the end of the used slots; values above 8 count as 8). Each
 * filled slot is handed to the IWRAM handler call_sub_3005EBA twice - first
 * with mode 0 and a five-word parameter block on the stack, then with mode 3,
 * keeping only the saved slot offset on the stack. Afterwards bit 0x2000 is
 * cleared in the state flag word at 0x20352C4; r0-r3 end up as that flag
 * helper leaves them, exactly like the original. */

#define SLOT_TABLE_OFFSET 0x28   /* four u16 slots in the struct from r5 */
#define SLOT_TABLE_ENTRIES 4
#define SLOT_EMPTY 0xFF

/* register form: calls call_sub_3005EBA outside this file, which has no typed C
 * interface yet. It is invoked twice per slot: besides r0-r3 it is handed
 * further values in r4-r6 and reads a parameter block that the original builds
 * by hand on the guest stack, with the slot offset saved just above that block
 * so the second call finds it as its only stack word - so the whole call
 * sequence, including the SP pushes, is staged here. */
static void dispatch_slot_pair(u32 pos, u32 slot_offset, u32 level, u32 slot_addr) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6;

    SP -= 24;                        /* parameter block plus saved slot offset */
    ST32(SP, pos);
    ST32(SP + 4, slot_offset);
    ST32(SP + 8, 0);
    ST32(SP + 12, level + 1);
    ST32(SP + 16, 2);
    ST32(SP + 20, slot_offset);

    R0 = pos;                        /* first call: mode 0 */
    R1 = slot_offset;
    R2 = 0;
    R3 = 0;
    R4 = level + 1;
    R5 = 2;
    R6 = slot_addr;
    GUEST_CALL(call_sub_3005EBA);

    SP += 20;                        /* drop the block, keep the saved offset */
    R0 = pos;                        /* second call: mode 3 */
    R1 = slot_offset;
    R2 = 3;
    R3 = 0;
    R4 = level + 1;
    R5 = 2;
    R6 = slot_addr;
    GUEST_CALL(call_sub_3005EBA);

    SP += 4;
    R4 = saved_r4;
    R5 = saved_r5;
    R6 = saved_r6;
}

/* register form: calls flags_clearBits_20352C4_801BEC2 outside this file,
 * which has no typed C interface yet. */
static void clear_state_flag_bits(u32 bits) {
    R0 = bits;
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
}

/* Fire the handler for every filled slot of the table, then clear the flag
 * bit that gates this whole feature. */
static void run_slot_table(u32 owner) {
    u32 slot_addr = owner + SLOT_TABLE_OFFSET;
    for (u32 slot_offset = 0; slot_offset < SLOT_TABLE_ENTRIES * 2;
         slot_offset += 2, slot_addr += 2) {
        u32 value = LD16(slot_addr);
        if (value == SLOT_EMPTY)
            break;                   /* first unused slot ends the scan */
        u32 level = value;
        if (level >= 9)
            level = 8;               /* slot values are meant to stay in 0..8 */
        dispatch_slot_pair(0x1D - level, slot_offset, level, slot_addr);
    }
    clear_state_flag_bits(1u << 13);
}

/* sub_801E4B0(r5 = owner struct; lr is the return address, r10 passes through,
 * r0-r3 are whatever the flag helper last left in them). */
Next sub_801E4B0(void) {
    run_slot_table(R5);
    RETURN_TO_CALLER();
}

/* ── RetrieveAndRenderEnemyVirusNamesInBattle_801E574 ── */
/* One rendered name, as returned by selectVirusOrNaviNamesAndWhich_800EC56 in
 * r0/r1: the text script holding the name, and which variant of the name to draw. */
typedef struct {
    u32 text_script;
    u32 name_variant;
} VirusNameText;

/* register form: calls LoadVirusNameIDsForUpToFourSlots_800A998 outside this file, which has no typed C interface yet */
static u32 load_virus_name_ids(u32 name_id_arr) {
    R0 = name_id_arr;
    R4 = name_id_arr; /* the original holds the slot array pointer in r4 across this call */
    GUEST_CALL(LoadVirusNameIDsForUpToFourSlots_800A998);
    return R0;
}

/* register form: calls selectVirusOrNaviNamesAndWhich_800EC56 outside this file, which has no typed C interface yet */
static VirusNameText select_virus_or_navi_name(u32 name_id) {
    VirusNameText name;
    R0 = name_id;
    GUEST_CALL(selectVirusOrNaviNamesAndWhich_800EC56);
    name.text_script = R0;
    name.name_variant = R1;
    return name;
}

/* register form: calls renderTextGfx_8045F8C outside this file, which has no typed C interface yet */
static u32 render_virus_name_text(u32 text_script, u32 name_variant, u32 text_dst, u32 gfx_dst, u32 font_gfx) {
    R0 = text_script;
    R1 = name_variant;
    R2 = text_dst;
    R3 = gfx_dst;
    R4 = 9;       /* name box is nine tiles wide */
    R5 = 1;
    R6 = font_gfx;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);
    return R0;
}

/* register form: calls dispatch_801DA48 outside this file, which has no typed C interface yet.
 * Runs inside the entry frame the original still has pushed ({r4,r5,r7,lr}, done by the
 * caller of this helper); dispatch leaves an SP-derived pointer in r3 and its own r2/r3/r6.
 * The original pops r4/r5/r7 right after this call, so the helper restores them here. */
static void dispatch_name_gfx_update(u32 event, u32 stride, u32 text_dst_end, u32 gfx_dst_end,
                                     u32 slot_ptr_end, u32 font_gfx, u32 last_written,
                                     u32 entry_r4, u32 entry_r5, u32 entry_r7) {
    R0 = event;
    R1 = stride;
    R2 = text_dst_end;
    R3 = gfx_dst_end;
    R4 = slot_ptr_end;
    R6 = font_gfx;
    R7 = last_written;
    GUEST_CALL(dispatch_801DA48);
    R4 = entry_r4;
    R5 = entry_r5;
    R7 = entry_r7;
}

/* Fill the four enemy virus name slots for the current battle, render each name into its
 * own 0x240-byte slice of the text buffer (byte_203EBA0) and tile buffer (0x600BB40), and
 * hand the buffers to the VRAM update dispatcher.
 * The caller's r4/r5/r7 pass through unchanged; r2/r3/r6 end up as dispatch_801DA48 left them. */
static void retrieve_and_render_enemy_virus_names(u32 entry_r4, u32 entry_r5, u32 entry_r7, u32 entry_lr) {
    /* register form: the original's entry push {r4,r5,r7,lr} stays live until the final
     * dispatch_801DA48 call (and every callee in between), and dispatch leaves an
     * SP-derived pointer in r3 — so the frame must exist in guest memory, not just in C. */
    SP -= 16;
    ST32(SP + 0, entry_r4);
    ST32(SP + 4, entry_r5);
    ST32(SP + 8, entry_r7);
    ST32(SP + 12, entry_lr);

    u32 name_id_arr = A_eStruct2035280 + oStruct2035280_VirusNameIdxU16Arr;

    /* Mark all four slots empty (0xFF = no name) before asking the battle state
     * which viruses are actually present. */
    ST16(name_id_arr + 0, 0xFF);
    ST16(name_id_arr + 2, 0xFF);
    ST16(name_id_arr + 4, 0xFF);
    ST16(name_id_arr + 6, 0xFF);

    u32 slot_count = load_virus_name_ids(name_id_arr);

    u32 text_dst = A_byte_203EBA0;
    u32 gfx_dst = 0x600BB40;
    u32 slot_ptr = name_id_arr;
    u32 last_written = 0;
    u32 remaining = slot_count;

    do {
        u32 name_id = LD16(slot_ptr);
        VirusNameText name = select_virus_or_navi_name(name_id);
        last_written = render_virus_name_text(name.text_script, name.name_variant,
                                              text_dst, gfx_dst, A_dword_86B7AE0);
        /* Remember the drawn name's id in the slot it was read from. */
        ST16(slot_ptr, last_written);
        slot_ptr += 2;
        text_dst += 0x240;
        gfx_dst += 0x240;
    } while (--remaining != 0);

    /* 0x2000 (bit 13) tells the dispatcher to upload the freshly rendered name tiles. */
    dispatch_name_gfx_update(0x2000, 0x240, text_dst, gfx_dst, slot_ptr, A_dword_86B7AE0,
                             last_written, entry_r4, entry_r5, entry_r7);

    SP += 16;
}

Next RetrieveAndRenderEnemyVirusNamesInBattle_801E574(void) {
    retrieve_and_render_enemy_virus_names(R4, R5, R7, LR);
    /* r2/r3/r6 already hold dispatch_801DA48's results and r8-r12 pass through untouched. */
    RETURN_TO_CALLER();
}

/* ── sub_801E5E0 ── */
/* sub_801E5E0: call flags_clearBits_20352C4_801BEC2 with r0 = 0x4000.
 * The wrapper itself contributes nothing but the argument: r0-r2 and the
 * flags after return are exactly what flags_clearBits_20352C4_801BEC2 leaves behind. Note that
 * the original's `mov r0,#1` + `lsl r0,r0,#14` sequence is a Thumb shifted
 * move, which sets C from the bit shifted out (bit 18 of 1 = 0), so the call
 * is entered with C clear — flags_clearBits_20352C4_801BEC2 relies on that on paths where it
 * preserves C. */
Next sub_801E5E0(void) {
    R0 = 1 << 14;
    SET_FLAGS_ADD(R0, 0);   /* N=0, Z=0, C=0, matching the LSL result */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801E5EC ── */
/* sub_801E5EC: invoke flagSetBits_eStruct2035280_801BEB8 with r0 = 0x4000 (1 << 14).
 * All results (r0-r2) come straight from the callee; the callee also leaves
 * N/Z alone, but passes the C flag through unchanged, so we must reproduce
 * the C=0 that the original's "lsl r0, r0, #14" leaves behind (shifting the
 * value 1 left by 14 shifts no 1-bits out of the register). */
Next sub_801E5EC(void) {
    R0 = 1u << 14;
    /* Same flag state the lsl leaves: N=0, Z=0, C=0. */
    SET_FLAGS_ADD(R0, 0);
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── battle_initSceneStart_801E5F8 ── */
/* ------------------------------------------------------------------ *
 * Battle scene start-up (battle_initSceneStart_801E5F8)
 *
 * Resets the per-battle work area at eStruct2035280 and hands control
 * to the scene dispatcher: the navi-status fields are cleared, the
 * "special rule" flag byte_203529E is decided from the battle effects,
 * the battle mode and a helper check, and then request bit 0x4000 is
 * raised and dispatched.
 * ------------------------------------------------------------------ */

#define SCENE_REQUEST_BIT       (1u << 14) /* 0x4000: (re)start the battle scene */
#define EFFECTS_NO_SPECIAL_RULE 0x200000   /* effects bit that forces the plain path */

/* ---- register-form helpers for game functions with no typed C interface yet ---- */

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 read_battle_effects(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 read_battle_mode(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls eventFlag1720IsSet_813BF1C outside this file, which has no typed C interface yet */
static u32 sub_813BF1C_result_zero(void) {
    GUEST_CALL(eventFlag1720IsSet_813BF1C);
    return ZERO_FLAG();               /* eventFlag1720IsSet_813BF1C answers in the Z flag */
}

/* register form: calls eStruct2035280_SetRequestBits_801BECC outside this file, which has no typed C interface yet */
static void set_request_bits(u32 bits) {
    R0 = bits;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);
}

/* register form: calls dispatch_801DA48 outside this file, which has no typed C interface yet */
static void dispatch_request(u32 bits) {
    R0 = bits;
    GUEST_CALL(dispatch_801DA48);
}

/* Battle modes 1-5 and 8 (the multiplayer / special battle modes) skip the
 * extra rule check and always take the plain path. */
static u32 mode_forces_plain_path(u32 mode) {
    return (mode >= 1 && mode <= 5) || mode == 8;
}

/* Reset the battle state block and start the battle scene. */
static void init_battle_scene(void) {
    u32 scene = A_eStruct2035280;

    ST8(scene + 0x12, 0x00);          /* byte_2035292 */
    ST8(scene + 0x13, 0xFF);          /* byte_2035293 */
    ST8(scene + 0x17, 0xFF);          /* eStruct2035280_NaviStatusGfxIdx17 */
    ST8(scene + 0x15, 0xFF);          /* byte_2035295 */
    ST16(scene + 0x38, 0x78);         /* word_20352B8 */

    /* byte_203529E becomes 1 only when none of the "plain battle" conditions
     * hold: the effects bit being set, a special battle mode, or the
     * eventFlag1720IsSet_813BF1C check answering zero each force it to 0.  The checks must
     * run in this order and stop as soon as one succeeds (they have side
     * effects), so each is nested instead of merged into one condition. */
    u32 special_rule = 0;
    if ((read_battle_effects() & EFFECTS_NO_SPECIAL_RULE) == 0) {
        u32 mode = read_battle_mode();
        if (!mode_forces_plain_path(mode)) {
            if (!sub_813BF1C_result_zero())
                special_rule = 1;
        }
    }
    ST8(scene + 0x1E, special_rule);  /* byte_203529E */

    set_request_bits(SCENE_REQUEST_BIT);
    dispatch_request(SCENE_REQUEST_BIT);
}

/* battle_initSceneStart_801E5F8() -> r2..r12 all pass through unchanged (r4/r5 are saved and
 * restored by the original's push/pop, the rest are never touched); the
 * flags and r0/r1/lr are clobbers. */
Next battle_initSceneStart_801E5F8(void) {
    init_battle_scene();
    RETURN_TO_CALLER();
}

/* ── battle_setNaviStatusGfx_801E71C ── */
/* battle_setNaviStatusGfx_801E71C(r0 = value):
 * Stores the byte in r0 into byte_2035292 (field 0x12 of eStruct2035280).
 * r1 is preserved; r0's incoming value is not meaningful afterwards. */
Next battle_setNaviStatusGfx_801E71C(void) {
    ST8(A_eStruct2035280 + 0x12, R0);
    RETURN_TO_CALLER();
}

/* ── naviStatus_SetGfxAndStatus_801E724 ── */
/* naviStatus_SetGfxAndStatus_801E724(r0 = new navi status gfx index, r1 = new navi status value):
 * Updates the navi status state at eStruct2035280. The current display index
 * (0x17) is reused if it is set, otherwise falls back to the value at 0x15;
 * it is written to 0x1F. Stores the new status at 0x16, the new gfx index at
 * 0x17, and marks 0x13 as pending with 0xFF. r5 is saved/restored by the
 * original, so it is left unchanged. r2 = the display index written. */
Next naviStatus_SetGfxAndStatus_801E724(void) {
    u32 gfxIdx = R0;
    u32 status = R1;
    u32 state  = A_eStruct2035280;

    u32 displayIdx = LD8(state + 0x17);
    if (displayIdx == 0xFF)
        displayIdx = LD8(state + 0x15);

    ST8(state + 0x1F, displayIdx);
    ST8(state + 0x16, status);
    ST8(state + 0x17, gfxIdx);
    ST8(state + 0x13, 0xFF);

    R0 = 0xFF;
    R2 = displayIdx;
    RETURN_TO_CALLER();
}

/* ── sub_801E73C ── */
/* sub_801E73C: invoke flags_clearBits_20352C4_801BEC2 with r0 = 0x8000.
 *
 * The constant has no Thumb MOV encoding, so the original builds it as
 * 1 << 15 — and that LSL is the last flag-setting instruction on the path
 * through this wrapper, because flags_clearBits_20352C4_801BEC2 itself leaves the condition
 * flags untouched. Callers therefore see the shift's flag results
 * (N = 0, Z = 0, carry-out = 0) after this function returns, so they must
 * be produced explicitly. LR ends up holding the BL's return address
 * (0x801E747), which the GUEST_CALL return trampoline reproduces. */
Next sub_801E73C(void) {
    R0 = 1;
    R0 <<= 15;          /* r0 = 0x8000; shifting 1 by 15 carries out a 0 bit */
    SET_FLAGS_NZ(R0);   /* N = 0, Z = 0 from the shifted result */
    cpu.c = 0;          /* carry out of the LSL */
    GUEST_CALL(flags_clearBits_20352C4_801BEC2);
    RETURN_TO_CALLER();
}

/* ── sub_801E748 ── */
/* sub_801E748: invoke flagSetBits_eStruct2035280_801BEB8 with the argument 0x8000 (bit 15 set).
 * The literal "mov r0,#1; lsl r0,r0,#15" also updates the flags, and since
 * the callee preserves them on exit, its carry-out (0 for this operand)
 * is what our caller observes. */
Next sub_801E748(void) {
    R0 = 1u << 15;
    /* flag state after "lsl r0, r0, #15": result 0x8000 -> N=0, Z=0,
     * carry out of bit 17 of the old value 1 -> C=0. V is unaffected. */
    SET_FLAGS_NZ(R0);
    cpu.c = 0;
    GUEST_CALL(flagSetBits_eStruct2035280_801BEB8);
    RETURN_TO_CALLER();
}

/* ── comm_checkCrossNetBattleReady_801E754 ── */
/* comm_checkCrossNetBattleReady_801E754: inspect cross-net battle state flags.
 * Returns in R0:
 *   0 - bit 15 of dword_20352C0 is clear
 *   1 - flag set, but the byte_2036840/2036848 state doesn't match
 *   2 - flag set and byte_2036840 holds the expected value 4
 * R1/R2 are preserved; C/V are left as the original's last flag-setting
 * instruction leaves them. */
Next comm_checkCrossNetBattleReady_801E754(void) {
    u32 stateWord = LD32(A_eStruct2035280 + 0x40); /* dword_20352C0 */

    /* The original builds the test mask with `mov r0,#1; lsl r0,#15`.
     * That shift leaves C clear, and on the early-exit path nothing after
     * it touches C (the `and` only sets N/Z), so carry stays clear. */
    cpu.c = 0;
    u32 flagBit = (1u << 15) & stateWord;
    SET_FLAGS_NZ(flagBit);

    if (flagBit == 0) {
        R0 = 0;
    } else {
        u8 modeSlot = LD8(A_byte_2036840 + 8); /* byte_2036848 */
        if (modeSlot == 4 || modeSlot == 2) {
            u8 modeByte = LD8(A_byte_2036840); /* byte_2036840 */
            SET_FLAGS_CMP(modeByte, 4);
            R0 = (modeByte == 4) ? 2 : 1;
        } else {
            SET_FLAGS_CMP(modeSlot, 2);
            R0 = 1;
        }
    }
    RETURN_TO_CALLER();
}

/* ── setByteFieldCapped99_801E828 ── */
/* setByteFieldCapped99_801E828(r1 = value, r5 = struct pointer):
 * Clamp the byte value to at most 99, store it at [r5+9], then let
 * battle_queueScoreAndTiles_801E838 refresh whatever depends on it (results and flags are
 * whatever that call leaves behind). */
Next setByteFieldCapped99_801E828(void) {
    u32 value = R1;

    if ((s32)value > 99)
        value = 99;

    ST8(R5 + 9, value);
    R1 = value;

    GUEST_CALL(battle_queueScoreAndTiles_801E838);

    RETURN_TO_CALLER();
}

/* ── battle_queueScoreAndTiles_801E838 ── */
#define TILE_SIZE              0x20
#define VRAM_OBJ_TILE_BASE     0x6016E00
#define VRAM_SCORE_DIGIT_BASE  0x6017300
#define OBJ_TILE_STRIDE        0x80
#define BG_TILE_SIZE           0x20

/* Battle state field offsets (pointer r5) */
#define BS_SCORE_DATA          4   /* u16: tile-data offset for score graphic */
#define BS_DISPLAY_MODE        8   /* u8:  1 = show score digits */
#define BS_BCD_SCORE           9   /* u8:  packed-BCD score (tens<<4 | ones) */

/* External data symbols */
#define NULL_TILE_MARKER       A_byte_801FDC0
#define DIGIT_TILE_TABLE       A_off_801FD64
#define FIXED_BG_SRC           A_byte_86F2900
#define FIXED_BG_DST           A_byte_30016B0

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx_transfer_sub_801E838(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls memory_bcdConvert outside this file, which has no typed C interface yet */
static u32 bcd_to_binary(u8 bcd) {
    R0 = bcd;
    GUEST_CALL(memory_bcdConvert);
    return R0;
}

/* Queue OBJ tile transfers from a source list, optionally draw a 2-digit BCD
 * score at the score-digit VRAM region, and always copy a fixed background tile.
 *
 * tile_list   – pointer to an array of u32 source tile pointers (terminated by
 *               the null-tile marker; read from tile_list+4 onward).
 * battle_state – pointer to the per-battle state block (score, mode, BCD value).
 *
 * Registers r0-r3 and NZCV flags on return are whatever the final
 * QueueEightWordAlignedGFXTransfer call leaves behind. */
static void queue_tile_and_score(u32 tile_list, u32 battle_state) {
    /* --- 5 groups × 4 tiles = 20 tile-pair transfers ---------------- */
    u32 list_cursor = tile_list + 4;          /* r7: next entry in source list */
    s32 groups_remaining = 5;                  /* r4 */
    u32 vram = VRAM_OBJ_TILE_BASE;            /* r1: OBJ tile VRAM write position */
    const u32 null_tile = NULL_TILE_MARKER;

    while (groups_remaining > 0) {
        s32 tiles_in_group = 4;                /* r6 */

        while (tiles_in_group > 0) {
            u32 src = LD32(list_cursor);

            /* If the entry is not the null marker, advance the list cursor.
             * (Null entries are "used in place" – cursor stays.) */
            if (src != null_tile)
                list_cursor += 4;

            /* First row of the 8×8 tile (32 bytes) */
            queue_gfx_transfer_sub_801E838(src, vram, TILE_SIZE);

            /* Second row: source + 0x20, dest + 0x80 */
            queue_gfx_transfer_sub_801E838(src + TILE_SIZE, vram + OBJ_TILE_STRIDE, TILE_SIZE);

            /* Advance VRAM by one tile-row slot (0x20) */
            vram += TILE_SIZE;
            tiles_in_group--;
        }

        /* Jump VRAM past the current group */
        vram += OBJ_TILE_STRIDE;
        groups_remaining--;
    }

    /* --- Optional 2-digit score display (mode == 1) ------------------ */
    if (LD8(battle_state + BS_DISPLAY_MODE) == 1) {
        u32 entry = LD32(list_cursor);

        /* Skip the null marker if the tile list ended here */
        if (entry == null_tile)
            list_cursor += 4;

        /* Store a 16-bit score-data word into the battle state */
        ST16(battle_state + BS_SCORE_DATA, LD16(list_cursor));

        /* Convert the BCD score byte to binary, split into decimal digits */
        u32 binary_score = bcd_to_binary(LD8(battle_state + BS_BCD_SCORE));
        u32 ones  = binary_score & 0xF;
        u32 tens  = binary_score >> 4;

        if (tens == 0) {
            /* Single-digit score: show "0" for tens, shift graphic left by 4 px */
            tens = 10;                         /* digit table index 10 → blank/zero tile */
            u8 lo = LD8(battle_state + BS_SCORE_DATA);
            ST8(battle_state + BS_SCORE_DATA, lo - 4);
        }

        /* Look up tile-source pointers from the digit table (u32 array) */
        u32 tens_src = LD32(DIGIT_TILE_TABLE + tens * 4);
        u32 ones_src = LD32(DIGIT_TILE_TABLE + ones * 4);

        /* Queue 4 transfers: tens row0, ones row0, tens row1, ones row1 */
        u32 dv = VRAM_SCORE_DIGIT_BASE;
        queue_gfx_transfer_sub_801E838(tens_src,                dv, TILE_SIZE);
        dv += TILE_SIZE;
        queue_gfx_transfer_sub_801E838(ones_src,                dv, TILE_SIZE);
        dv += TILE_SIZE;
        queue_gfx_transfer_sub_801E838(tens_src + TILE_SIZE,    dv, TILE_SIZE);
        dv += TILE_SIZE;
        queue_gfx_transfer_sub_801E838(ones_src + TILE_SIZE,    dv, TILE_SIZE);
    }

    /* --- Fixed background tile (always queued) ----------------------- */
    queue_gfx_transfer_sub_801E838(FIXED_BG_SRC, FIXED_BG_DST, BG_TILE_SIZE);
}

/* battle_queueScoreAndTiles_801E838(r4 = tile_list, r5 = battle_state)
 * Preserves r4–r7; outputs r0–r3 and flags from the last GFX transfer call. */
Next battle_queueScoreAndTiles_801E838(void) {
    queue_tile_and_score(R4, R5);
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

/* ── sub_801EA34 ── */
/* sub_801EA34(r1 = VRAM destination, r4 = four source selectors, one nibble each):
 * Queues graphics for four tile groups. For each nibble of r4, taken low to
 * high, the pointer table at off_801FD90 supplies a source address; 0x20 bytes
 * are queued from there to the current destination, and another 0x20 bytes from
 * source+0x20 to destination+0x80. After each pair the destination steps back
 * by 0x20 bytes (one tile), so the four pairs fill descending VRAM positions.
 * r4 is consumed one nibble at a time; r5/r6 are left untouched. */
Next sub_801EA34(void) {
    u32 dest = R1;
    u32 selector = R4;
    const u32 sourceTable = A_off_801FD90;

    for (u32 i = 0; i < 4; i++) {
        u32 src = LD32(sourceTable + (selector & 0xF) * 4);

        /* First 8-word chunk: source table entry -> destination. */
        R0 = src;
        R1 = dest;
        R2 = 0x20;
        GUEST_CALL(QueueEightWordAlignedGFXTransfer);

        /* Second 8-word chunk: source+0x20 -> destination+0x80. */
        R0 = src + 0x20;
        R1 = dest + 0x80;
        R2 = 0x20;
        GUEST_CALL(QueueEightWordAlignedGFXTransfer);

        dest -= 0x20;
        selector >>= 4;
    }

    RETURN_TO_CALLER();
}

/* ── selectPlayerSide_801EA5A ── */
/* register form: reads the side-select word the caller stashed at sp+8.
 * No typed interface exists for the raw sp access, so we keep it in a
 * tiny register-form helper. */
static u32 side_select_word(void) {
    return LD32(SP + 8);
}

/* Pick which side (player 1 or player 2) to use based on a parameter stashed
 * by the caller at sp+8. When that word is 1 we hand back the second side's
 * VRAM/palette base pair; otherwise the first side's. The "base" is a guest
 * address provided by the disassembly's A_ label, so it must stay as a u32. */
static void select_player_side(u32 *out_base, u32 *out_pal_base) {
    u32 side = side_select_word();
    if (side == 1) {
        *out_base = (u32)A_byte_203EFA0;
        *out_pal_base = 0x6017280;
    } else {
        *out_base = (u32)A_byte_203EDA0;
        *out_pal_base = 0x6016E00;
    }
}

Next selectPlayerSide_801EA5A(void) {
    u32 r2, r3;
    select_player_side(&r2, &r3);
    R2 = r2;
    R3 = r3;
    RETURN_TO_CALLER();
}
