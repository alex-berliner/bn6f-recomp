/*
 * asm31_80EBE00.c: readable functions of bn6f asm31.s, from 0x080EBE00, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

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

/* ── sub_80EBE54 ── */
/* register form: calls object_setAnimation outside this file, which has no typed C interface yet */
static void set_object_animation(u32 animation) {
    R0 = animation;
    GUEST_CALL(object_setAnimation);
}

/* register form: calls sub_80EB562 outside this file, which has no typed C interface yet */
static void run_charge_handler_80EB562(void) {
    GUEST_CALL(sub_80EB562);
}

/* register form: calls object_setFlag1 outside this file, which has no typed C interface yet */
static void set_object_flag1(u32 mask) {
    R0 = mask;
    GUEST_CALL(object_setFlag1);
}

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound(u32 id) {
    R0 = id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls battle_SpawnObjectOnFrontPanel_800FAAC outside this file, which has no typed C interface yet */
static u32 spawn_object_on_front_panel(u32 element, u32 damage, u32 unk_r3) {
    R0 = element;
    R1 = damage;
    R3 = unk_r3;    /* r2 is deliberately left as the caller left it, as in the original */
    GUEST_CALL(battle_SpawnObjectOnFrontPanel_800FAAC);
    return R0;
}

/* register form: spawn_t1_0x5_tempAttackObject_80B8E30 takes the template id in r0,
 * a template value in r4 and a pointer in r7; r4/r7 are restored afterwards,
 * matching the original's push/pop of r7 and its saved r4 */
static void spawn_temp_attack_object(u32 template_id, u32 template_r4, u32 target_r7,
                                     u32 saved_r4, u32 saved_r7) {
    R0 = template_id;
    R4 = template_r4;
    R7 = target_r7;
    GUEST_CALL(spawn_t1_0x5_tempAttackObject_80B8E30);
    R4 = saved_r4;
    R7 = saved_r7;
}

/* Offsets within the AI attack-vars struct that game.h does not name:
 *   +0x00 = state (u16, oAIAttackVars_Unk_00)
 *   +0x01 = charge-phase byte (oAIAttackVars_Unk_01)
 *   +0x08 = damage (u16, oAIAttackVars_Damage)       */
#define AIATTACK_STATE_OFFSET      0u
#define AIATTACK_PHASE_OFFSET      1u
#define AIATTACK_DAMAGE_OFFSET     8u

/* One tick of the AI buster attack state machine for the oAIAttackVars struct at `atk`.
 * On the warm-up tick it starts the charge animation; on tick 1 it fires the buster
 * shot on the panel in front plus the muzzle-flash temp object; each tick advances
 * the fire timer, and once it passes 4 the attack state resets to 8 (idle).
 * Returns the value the original leaves in r0. */
static u32 ai_buster_attack_tick(u32 atk, u32 r4_in, u32 r5) {
    if (LD8(atk + AIATTACK_PHASE_OFFSET) == 0) {
        /* charge-up tick: animation, per-object hook, then mark the shot as charging */
        set_object_animation(0xE);
        run_charge_handler_80EB562();
        ST8(atk + AIATTACK_PHASE_OFFSET, 4);
        set_object_flag1(1u << 22 /* OBJECT_FLAGS_UNK_BIT_22_BIT */);
        ST16(atk + oAIAttackVars_Unk_10, 0);
    }

    if (LD16(atk + oAIAttackVars_Unk_10) == 1) {
        /* fire tick: buster sound, shot object on the front panel, muzzle flash */
        play_sound(SOUND_BUSTER_6A);
        u32 shot = spawn_object_on_front_panel(
            LD8(atk + oAIAttackVars_Unk_0c),
            LD16(atk + AIATTACK_DAMAGE_OFFSET),
            0x180000u);
        ST16(atk + oAIAttackVars_Unk_12, shot);
        spawn_temp_attack_object(0, 5, r5 + 0x4C, r4_in, atk);
    }

    u32 ticks = LD16(atk + oAIAttackVars_Unk_10) + 1;
    ST16(atk + oAIAttackVars_Unk_10, ticks);
    if (ticks > 4) {
        /* fired all shots: reset the AI attack state */
        ST16(atk + AIATTACK_STATE_OFFSET, 8);
        return 8;
    }
    return ticks;
}

/* sub_80EBE54(r7 = AI attack vars, r4/r5 = caller context used by the spawn helper) -> r0
 * The final flags always come from "cmp <counter>, #4" (the mov r0,#8 on the reset path
 * only overwrites n/z, which cmp already leaves as 0/non-zero for counter > 4), so a
 * single CMP of the returned r0 reproduces n/c/v on every path. */
Next sub_80EBE54(void) {
    R0 = ai_buster_attack_tick(R7, R4, R5);
    SET_FLAGS_CMP(R0, 4);
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

/* ── ai_attackTickDelayTimer_80EC90E ── */
/* ai_attackTickDelayTimer_80EC90E(r5 = battle object, r7 = AI attack vars):
 * Countdown for a delayed AI attack. On the first tick it starts a 10-frame
 * timer and marks the attack as pending; once the timer runs out it detaches
 * the related object, clears the AI data flag and leaves the attack state.
 * Flags and r0 reflect the decremented timer when it is still positive. */
Next ai_attackTickDelayTimer_80EC90E(void) {
    /* First activation: initialize the timer and its state byte. */
    if (LD8(R7 + 1 /* oAIAttackVars_Unk_01 */) == 0) {
        ST16(R7 + oAIAttackVars_Unk_10, 10);
        ST8(R7 + 1, 4);
    }

    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(timer, 1); /* flags of SUBS timer, #1 */
    timer = timer - 1;
    ST16(R7 + oAIAttackVars_Unk_10, timer);

    if ((s32)timer >= 0) {
        R0 = timer;
        RETURN_TO_CALLER();
    }

    /* Timer expired: detach the object we were tracking and exit. */
    ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(aiData + oAIData_Unk_68, 0);

    R0 = 0;
    R1 = aiData;
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── aiAttack_tickTimerAndExitWhenDone_80ECA0C ── */
/* aiAttack_tickTimerAndExitWhenDone_80ECA0C: tick down the AI attack timer in the object's AIAttackVars.
 * When the counter (oAIAttackVars_Unk_10) drops below zero, leave the attack
 * state. Flags reflect the decrement (SUBS r0, #1) when the callee is not
 * taken; otherwise whatever object_exitAttackState leaves is fine. */
Next aiAttack_tickTimerAndExitWhenDone_80ECA0C(void) {
    u32 count = LD16(R7 + oAIAttackVars_Unk_10);
    u32 remaining = count - 1;

    SET_FLAGS_CMP(count, 1);
    ST16(R7 + oAIAttackVars_Unk_10, (u16)remaining);
    R0 = remaining;

    if ((s32)remaining < 0)
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

/* ── sub_80FDF4C ── */
/* sub_80FDF4C: fetch the address of the dword_80FDF5C work area and pass it
 * to sub_800FE28, which does the real work. Its results (r0, r2) are ours. */
Next sub_80FDF4C(void) {
    R0 = A_dword_80FDF5C;
    GUEST_CALL(sub_800FE28);
    RETURN_TO_CALLER();
}

/* ── sub_80FDF84 ── */
/* sub_80FDF84: calls the helper sub_800FE28 with the constant data table
 * dword_80FDF94 (a single word, 0x02020202) as its argument; the helper's
 * results in r0/r2 are passed straight through to the caller. */
Next sub_80FDF84(void) {
    R0 = A_dword_80FDF94; /* &dword_80FDF94 (== 0x080FDF94, .word 0x02020202) */
    GUEST_CALL(sub_800FE28);
    RETURN_TO_CALLER();
}

/* ── sub_80FDF98 ── */
/* sub_80FDF98: call sub_800FE28 with a pointer to the ROM constant
 * dword_80FDFA8 (word 0x181E242A); returns sub_800FE28's result in r0. */
Next sub_80FDF98(void) {
    R0 = A_dword_80FDFA8;
    GUEST_CALL(sub_800FE28);
    RETURN_TO_CALLER();
}

/* ── sub_80FDFE4 ── */
#define AI_TABLE_STRIDE 8
#define AI_DAMAGE_BLOCK_OFFSET 0xA0

/* register form: calls battle_aiGetVersionedTableEntry_800FE12 outside this file, which has no typed C interface yet */
static u32 get_ai_table_entry(u32 entry_addr, u32 table_start, u32 stride) {
    R0 = entry_addr;
    R1 = table_start;
    R2 = stride;
    GUEST_CALL(battle_aiGetVersionedTableEntry_800FE12);
    return R0;
}

/* Write the versioned 32-bit damage value (low 16 from damage table, high 16 from
 * modifier table) into the BattleObject's AI attack data block.
 * index: row into the per-version damage tables.
 * battle_obj: pointer to the BattleObject (r5).
 * Returns the damage sub-block pointer, which the original leaves in r3. */
static u32 store_ai_damage(u32 index, u32 battle_obj) {
    u32 dmg_lo = get_ai_table_entry(index * AI_TABLE_STRIDE + A_byte_80FE020,
                                     A_byte_80FE020, AI_TABLE_STRIDE);
    u32 dmg_hi = get_ai_table_entry(index * AI_TABLE_STRIDE + A_byte_80FE044,
                                     A_byte_80FE044, AI_TABLE_STRIDE);
    u32 combined = (dmg_hi << 16) | dmg_lo;

    u32 ai_base = LD32(battle_obj + oBattleObject_AIDataPtr) + AI_DAMAGE_BLOCK_OFFSET;
    ST32(ai_base + oAIAttackVars_Damage, combined);
    return ai_base;
}

/* sub_80FDFE4(r0 = table index, r5 = BattleObject ptr) -> r3 = damage sub-block ptr; r2 from callee; r4,r6 preserved */
Next sub_80FDFE4(void) {
    R3 = store_ai_damage(R0, R5);
    RETURN_TO_CALLER();
}

/* ── sub_80FE0D4 ── */
/* Register form: pure jump-table trampoline for AI attack-phase dispatch.
 * The only "computation" is an indexed table load followed by a tail-call to one
 * of four handlers that receive/return all guest registers; no typed C interface
 * can express that flow. */
static void ai_attack_phase_dispatch(u32 r7) {
    u32 index = LD8(r7 + oAIAttackVars_Unk_00);
    R0 = index;                                        /* byte the callee may inspect */
    R1 = LD32(A_off_80FE0E8 + index);                  /* Thumb function-pointer from table */

    switch (index) {
    case 0:  GUEST_CALL(sub_80FE0F8);                       break;
    case 4:  GUEST_CALL(sub_80FE130);                       break;
    case 8:  GUEST_CALL(sub_80FE1B4);                       break;
    case 12: GUEST_CALL(battle_aiTickAttackTimer_80FE1FE);  break;
    }

    R7 = r7;   /* original pop {r7,pc} restores the caller's r7 */
}

/* sub_80FE0D4 – dispatch one of four AI attack-phase handlers by jump table.
 * r7 = AIAttackVars pointer; index byte read from oAIAttackVars_Unk_00. */
Next sub_80FE0D4(void) {
    ai_attack_phase_dispatch(R7);
    RETURN_TO_CALLER();
}

/* ── battle_aiTickAttackTimer_80FE1FE ── */
/* battle_aiTickAttackTimer_80FE1FE: AI attack driver for one oAIAttackVars block (r7).
 *
 * The byte at offset oAIAttackVars_Unk_01 (offset 1) is the attack's armed
 * flag:
 *  - 0: the attack hasn't been started yet. Arm it: store 4 into the flag
 *       and copy the attack's charge time (Unk_18) into the countdown Unk_10.
 *  - nonzero: count Unk_10 down by one; once it drops to zero (or wraps),
 *       call sub_801171C to actually perform the attack.
 *
 * The caller inspects r0 and the nz/c/v flags afterwards, so they are left
 * exactly as the original instruction stream leaves them; r5/r6/r7 pass
 * through untouched. */
Next battle_aiTickAttackTimer_80FE1FE(void) {
    if (LD8(R7 + 1) == 0) {
        /* start-up: arm the attack and load its countdown */
        ST8(R7 + 1, 4);
        u32 timer = LD16(R7 + oAIAttackVars_Unk_18);
        ST16(R7 + oAIAttackVars_Unk_10, timer);
        SET_FLAGS_NZ(4);        /* flags of the original's `mov r0,#4` */
        R0 = timer;             /* r0 ends holding the value written to Unk_10 */
    } else {
        /* armed: tick the countdown */
        u32 left = LD16(R7 + oAIAttackVars_Unk_10);
        u32 timer = left - 1;
        ST16(R7 + oAIAttackVars_Unk_10, timer);
        SET_FLAGS_CMP(left, 1); /* flags of the original's `subs r0,#1` */
        R0 = timer;
        if ((s32)timer <= 0)    /* bgt not taken: countdown expired */
            GUEST_CALL(sub_801171C);
    }
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackWindupTick_80FE284 ── */
/* battle_aiAttackWindupTick_80FE284: AI attack state machine tick.
 * r5 = battle object, r7 = AI attack vars.
 * If the AI is idle (Unk_01 == 0), switch it to its wind-up state (Unk_01 = 4),
 * reset the object animation and start a 30-frame countdown.
 * Otherwise tick the countdown; when it expires, enter the attack state
 * (Unk_00 = 8).
 * Leaves R0 and the NZCV flags as the original's last flag-setting
 * instruction does. */
Next battle_aiAttackWindupTick_80FE284(void) {
    if (LD8(R7 + 1 /* oAIAttackVars_Unk_01 */) == 0) {
        /* Begin the wind-up phase. */
        ST8(R7 + 1 /* oAIAttackVars_Unk_01 */, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        R0 = 0x1E;
        ST16(R7 + oAIAttackVars_Unk_10, R0);
        SET_FLAGS_NZ(R0);
    } else {
        /* Count the wind-up timer down; once it hits zero, attack. */
        u32 remaining = LD16(R7 + oAIAttackVars_Unk_10);
        u32 left = remaining - 1;
        ST16(R7 + oAIAttackVars_Unk_10, left);
        SET_FLAGS_CMP(remaining, 1);
        if ((s32)left > 0) {
            R0 = left;
            RETURN_TO_CALLER();
        }
        R0 = 8;
        ST16(R7 /* oAIAttackVars_Unk_00 */, R0);
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── aiAttackDispatchHandler_80FE452 ── */
/* aiAttackDispatchHandler_80FE452: AI attack command dispatcher.
 *
 * r7 points to the AI attack state block. The byte at offset 0
 * (oAIAttackVars_Unk_00) indexes the handler table at off_80FE464:
 *     entry 0 -> sub_80FE46C
 *     entry 1 -> sub_80FE4AC
 * Note the original indexes with the raw byte (ldr r1, [r1, r0]), so a
 * selector of 4 selects the second entry; in practice the field only ever
 * holds one of those two values.
 *
 * The handler is entered with r0 = selector byte and r1 = the table word
 * that was jumped through (the original never clears r1 after the load),
 * so both are set explicitly here. The call runs, returns to this wrapper,
 * and this function then returns to its own caller with r7 restored
 * (push/pop {r7} in the original). Everything else, including the flags,
 * passes through the handler untouched. */
Next aiAttackDispatchHandler_80FE452(void) {
    u32 selector = LD8(R7);              /* byte at oAIAttackVars_Unk_00 (offset 0) */

    R0 = selector;
    if (selector == 0) {
        R1 = LD32(A_off_80FE464);        /* .word sub_80FE46C+1 */
        GUEST_CALL(sub_80FE46C);
    } else {
        R1 = LD32(A_off_80FE464 + 4);    /* .word sub_80FE4AC+1 */
        GUEST_CALL(sub_80FE4AC);
    }

    RETURN_TO_CALLER();
}

/* ── battleObject_dispatch_8108F50 ── */
/* Jump table at 0x08108F68 is indexed by the state byte directly (byte offsets 0, 4, 8
 * into the 3-entry pointer array).  Each handler is called with the full register file
 * intact, matching the original mov-lr / bx-r1 dispatch pattern. */

/* register form: dispatches to the battle-state handler identified by its byte-offset
 * in the 3-entry jump table (0 = sub_8016F56, 4 = battle_8108F74, 8 = sub_8016C4E). */
static void call_battle_state_handler(u32 state) {
    if (state == 0)
        GUEST_CALL(sub_8016F56);
    else if (state == 4)
        GUEST_CALL(battle_8108F74);
    else /* state == 8 */
        GUEST_CALL(sub_8016C4E);
}

/* register form: calls sub_8016E64 outside this file, which has no typed C interface yet
 * (post-dispatch update that drives enemy attack animations). */
static void call_post_dispatch_update(void) {
    GUEST_CALL(sub_8016E64);
}

/* Dispatch a battle object to the handler for its current state, then run the
 * post-dispatch animation/processing step.  Both callees may modify the full
 * register file, so results flow through R0–R12 and flags transparently. */
static void battle_object_dispatch(u32 battle_obj) {
    u32 state = LD8(battle_obj + oBattleObject_CurState);
    call_battle_state_handler(state);
    call_post_dispatch_update();
}

/* battleObject_dispatch_8108F50(r5 = BattleObject ptr) -> all regs + flags */
Next battleObject_dispatch_8108F50(void) {
    battle_object_dispatch(R5);
    RETURN_TO_CALLER();
}

/* ── sub_81095D0 ── */
/* Offsets used here that the shared headers don't declare yet
 * (values taken straight from the disassembly's ldrb/strb displacements). */
#define oObjectHeader_Flags         0   /* byte in the spawned effect's header */
#define oCollisionData_PrimaryElement 2 /* byte in the collision data */

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void sprite_setPalette_raw(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls spawn_t1_0x0_EffectObject outside this file, which has no typed C interface yet */
static u32 spawn_t1_0x0_EffectObject_raw(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4) {
    R0 = a0; R1 = a1; R2 = a2; R3 = a3; R4 = a4;
    GUEST_CALL(spawn_t1_0x0_EffectObject);
    return R0;
}

/* register form: calls enemy_getStruct2 outside this file, which has no typed C interface yet */
static u32 enemy_getStruct2_raw(u32 index) {
    R0 = index;
    GUEST_CALL(enemy_getStruct2);
    return R0;
}

/* Switch a battle object over to a new version (form): refresh its palette,
 * spawn the version-change effect, shift its name ID by the version delta,
 * and reload damage, HP and element from the enemy data table.
 *
 * obj is the battle object (r5). Returns what the original leaves in r2:
 * the object's collision-data pointer, or the incoming r2 untouched on the
 * early-exit path (version fields already equal, nothing happens). */
static u32 battle_object_change_version(u32 obj, u32 r2_in) {
    u32 ai = LD32(obj + oBattleObject_AIDataPtr);
    u32 newVersion = LD8(ai + oAIData_Version_16);
    u32 appliedVersion = LD8(ai + oAIData_Version_17);

    if (newVersion == appliedVersion)
        return r2_in;

    /* Remember which version was applied, and switch the palette to it.
     * Palette index = version * per-object stride. */
    ST8(ai + oAIData_Version_17, newVersion);
    sprite_setPalette_raw(newVersion * LD8(ai + 3));

    /* Spawn the version-change effect (kind 0x26) from the three words at
     * obj+0x34, and mark the new object with flag bit 2. The original's
     * ldmia writeback leaves r0 pointing just past the three words. */
    u32 effectArg0 = LD32(obj + 0x34);
    u32 effectArg1 = LD32(obj + 0x38);
    u32 effectArg2 = LD32(obj + 0x3C);
    u32 effect = spawn_t1_0x0_EffectObject_raw(obj + 0x40, effectArg0, effectArg1, effectArg2, 0x26);
    ST8(effect + oObjectHeader_Flags, LD8(effect + oObjectHeader_Flags) | 4);

    /* Shift the name ID by (new version - applied version); wraps the same
     * way the original's 32-bit SUB/ADD do if the version went down. */
    u32 versionDelta = newVersion - appliedVersion;
    u32 nameID = LD16(obj + oBattleObject_NameID) + versionDelta;
    ST16(obj + oBattleObject_NameID, nameID);

    /* Pull the new version's stats from the enemy data table. */
    u32 stats = enemy_getStruct2_raw(nameID);
    u32 damage = LD16(stats + 4);
    ST16(obj + oBattleObject_Damage, damage);
    u32 collision = LD32(obj + oBattleObject_CollisionDataPtr);
    ST16(collision + oCollisionData_SelfDamage, damage);

    /* Enemy data word 0: low 12 bits are HP, bits 12-15 the element. */
    u32 hpWord = LD16(stats);
    u32 hp = hpWord & 0xFFF;
    ST16(obj + oBattleObject_HP, hp);
    ST16(obj + oBattleObject_MaxHP, hp);
    u32 element = hpWord >> 12;
    ST8(obj + oBattleObject_Element, element);
    ST8(collision + oCollisionData_PrimaryElement, element);

    return collision;
}

/* sub_81095D0(r4/r6/r7 saved, r5 = battle object, r2 = pass-through on the
 * early path) -> r2 = collision-data pointer; r4/r5/r6/r7 end unchanged. */
Next sub_81095D0(void) {
    R2 = battle_object_change_version(R5, R2);
    RETURN_TO_CALLER();
}

/* ── nullsub_13 ── */
/* nullsub_13: does nothing; kept as a placeholder for an empty hook point. */
Next nullsub_13(void) {
    RETURN_TO_CALLER();
}

/* ── GetAllianceDependentPanelParamArgs ── */
/* GetAllianceDependentPanelParamArgs(r3 = panel param table base,
 *                                    r5 = battle object):
 * Reads the two words of the panel param entry selected by the object's
 * alliance (each alliance row is 8 bytes). Returns r2 = first word (param),
 * r3 = second word (args pointer), with flags from "row + 4". */
Next GetAllianceDependentPanelParamArgs(void) {
    u32 alliance = LD8(R5 + oBattleObject_Alliance);
    u32 row = R3 + (alliance << 3);   /* entry is 8 bytes per alliance */

    R2 = LD32(row);
    R3 = LD32(row + 4);
    SET_FLAGS_ADD(row, 4);
    RETURN_TO_CALLER();
}

/* ── battle_pickRandomRowObjectFields_810971A ── */
/* battle_pickRandomRowObjectFields_810971A: pick a random object from the row selected by the current
 * alliance-dependent panel parameters and split its entry byte into two
 * fields:
 *   r0 = low 3 bits of the entry byte,
 *   r1 = upper 4 bits (bit 3 of the byte is discarded by the shifts),
 * with nz taken from the final r0.
 * If the row is empty, r0 stays 0 and the flags come from the count
 * comparison. r2/r3 are left as the helpers left them; r4-r7 preserved. */
Next battle_pickRandomRowObjectFields_810971A(void) {
    u32 savedR7 = R7;

    /* The original pushes {r7,lr} and then reserves 8 more bytes; the list
     * of objects that object_getPanelsInRowFiltered fills in lives at SP. */
    SP -= 16;
    u32 rowObjects = SP;

    /* Work out which panel parameters apply for the attacker's alliance. */
    R3 = A_off_8109784;
    GUEST_CALL(GetAllianceDependentPanelParamArgs);

    /* Collect every object standing in that row into the stack buffer. */
    R7 = rowObjects;
    GUEST_CALL(object_getPanelsInRowFiltered);

    u32 count = R0;
    if (count == 0) {
        /* Nothing to choose from: r0 stays 0, flags from the comparison. */
        SET_FLAGS_CMP(R0, 0);
    } else {
        /* Roll a random number and reduce it modulo the row size. */
        R7 = count;
        GUEST_CALL(GetPositiveSignedRNG);

        R1 = count;
        swi_call(6); /* BIOS Div: r0 = rng / count, r1 = rng % count */

        u8 entry = LD8(rowObjects + R1);
        R1 = entry >> 4;  /* upper 4 bits of the entry */
        R0 = entry & 7;   /* low 3 bits (bit 3 is dropped) */
        SET_FLAGS_NZ(R0);
    }

    SP += 16;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── battle_attackTimerTick_8109CBC ── */
/* battle_attackTimerTick_8109CBC (r5 = battle object, r7 = this attack's variable block):
 * Per-frame tick for an AI attack. Counts the attack's remaining-frames
 * counter (oAIAttackVars_Unk_10) down by one; while it is still non-negative
 * the attack simply stays active for another frame. Once the counter runs out,
 * the attack's teardown helpers are run on the object's extra var, its state
 * word (pointed to by oAIAttackVars_Unk_30) is cleared, and the object leaves
 * the attack state. */
Next battle_attackTimerTick_8109CBC(void) {
    /* Decrement the attack countdown (halfword), keeping the SUBS flags for
     * the early-out path. */
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    SET_FLAGS_CMP(timer, 1);            /* sub r0, #1 */
    timer -= 1;
    R0 = timer;
    ST16(R7 + oAIAttackVars_Unk_10, timer);
    if ((s32)timer >= 0)                /* bge: attack still has frames left */
        RETURN_TO_CALLER();

    /* Timer expired. If the attack still owns a state word, run its two
     * cleanup helpers on the object's extra var, then zero the word. */
    u32 statePtr = LD32(R7 + oAIAttackVars_Unk_30);
    R0 = LD32(statePtr);
    if (R0 != 0) {
        R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
        GUEST_CALL(sub_800AA40);
        R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
        GUEST_CALL(list_tryAddUniqueEntry_800AA1A);
        R0 = 0;
        R1 = statePtr;
        ST32(R1, 0);
    }

    /* Leave the attack state either way. */
    GUEST_CALL(object_exitAttackState);
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

/* ── battle_objectTickWithExtraHandler_8109F6A ── */
/* battle_objectTickWithExtraHandler_8109F6A: per-frame update for one battle object (r5 = object).
 * While the object is still in phase 0, run the handler pointed to by
 * its second extra variable first; the phase tick (battle_objectUpdateDispatchPhase_80165C2) always
 * runs afterwards, with r0 carrying the phase byte / handler result. */
Next battle_objectTickWithExtraHandler_8109F6A(void) {
    if (LD8(R5 + oBattleObject_CurPhase) == 0) {
        R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
        GUEST_CALL(sub_800AA40);
    }
    GUEST_CALL(battle_objectUpdateDispatchPhase_80165C2);
    RETURN_TO_CALLER();
}

/* ── sub_8109F7E ── */
/* sub_8109F7E: call the two init steps in sequence (sub_8016B02, then
 * battle_object_onAction8EndClearExtraVars_8109FA2) and return; results are whatever the callees leave behind. */
Next sub_8109F7E(void) {
    GUEST_CALL(sub_8016B02);
    GUEST_CALL(battle_object_onAction8EndClearExtraVars_8109FA2);
    RETURN_TO_CALLER();
}

/* ── battle_object_onAction8EndClearExtraVars_8109FA2 ── */
/* battle_object_onAction8EndClearExtraVars_8109FA2(r5 = battle object):
 * When an object has just finished action 8 and still holds extra variable
 * state, run the two cleanup handlers on the ExtraVars[1] pointer, then
 * clear the extra-vars pointer and Param4.
 * Leaves r0 = the last value compared on the early-exit paths (CurAction,
 * or ExtraVars when it was zero), and r0/r1/r3 from the last callee
 * otherwise. Flags match the original's last compare/mov. */
Next battle_object_onAction8EndClearExtraVars_8109FA2(void) {
    /* r0 = oBattleObject_CurAction (offset 9); early out unless action 8 */
    R0 = LD8(R5 + 9);
    if ((s32)R0 != 8) {
        SET_FLAGS_CMP(R0, 8);
        RETURN_TO_CALLER();
    }

    /* Must still be tracking extra vars */
    R0 = LD32(R5 + oBattleObject_ExtraVars);
    if (R0 == 0) {
        SET_FLAGS_CMP(R0, 0);
        RETURN_TO_CALLER();
    }

    /* Free/release whatever the extra vars point at, then delete the
     * object list it references (the pointer is reloaded in case the
     * first call modified it). */
    R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
    GUEST_CALL(sub_800AA40);

    R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
    GUEST_CALL(list_tryAddUniqueEntry_800AA1A);

    /* Clear the extra-vars pointer and Param4 (offset 7) */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    ST32(R5 + oBattleObject_ExtraVars, R0);
    ST8(R5 + 7 /* oBattleObject_Param4 */, R0);

    RETURN_TO_CALLER();
}

/* ── battle_object_updatePhaseTick_8109FC4 ── */
/* battle_object_updatePhaseTick_8109FC4: advance a battle object's phase bookkeeping.
 * If the object (r5) is still in phase 0, initialize its Param4 byte
 * (struct offset 7) to 0 as well; then run the shared per-object
 * update battleObjectStateDispatcher_8016CE8, which also produces the final flags. */
Next battle_object_updatePhaseTick_8109FC4(void) {
    if (LD8(R5 + oBattleObject_CurPhase) == 0)
        ST8(R5 + 7 /* oBattleObject_Param4 */, 0);

    GUEST_CALL(battleObjectStateDispatcher_8016CE8);

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
