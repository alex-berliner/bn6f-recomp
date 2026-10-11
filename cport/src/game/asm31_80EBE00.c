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

/* ── sub_80EBF10 ── */
/* Dispatches an AI attack step through a 3-entry jump table indexed by the
 * attack-type byte at oAIAttackVars_Unk_00 (offset 0) within the vars block.
 * Sets r0 = type, r1 = handler address (table entry), lr = return point,
 * then calls the handler. All other registers pass through to the callee. */

/* register form: looks up AI attack jump table and calls the selected handler */
static void call_ai_attack_handler(u32 vars_ptr) {
    u32 attack_type = (u32)LD8(vars_ptr + oAIAttackVars_Unk_00);
    u32 handler = LD32(A_off_80EBF24 + attack_type);
    R0 = attack_type;
    R1 = handler;
    LR = 0x080EBF1C;
    if (attack_type == 0)
        GUEST_CALL(aiAttack_genericSpawnAndCountdown_80EBF30);
    else if (attack_type == 4)
        GUEST_CALL(sub_80EBF6E);
    else
        GUEST_CALL(battle_aiWindRackAttackStep_80EBFF4);
}

/* sub_80EBF10(r7 = AI attack vars ptr, lr = caller return addr)
 * Preserves all caller registers except those the handler modifies. */
Next sub_80EBF10(void) {
    u32 saved_lr = LR;
    call_ai_attack_handler(R7);
    LR = saved_lr;
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

/* ── sub_80ED13E ── */
/* register form: calls sub_80ED154 outside this file, which has no typed C interface yet */
static void call_attack_type_handler(u32 idx, u32 target, u32 ai_vars,
                                     u32 r2, u32 r3, u32 r4, u32 r5, u32 r6,
                                     u32 r8, u32 r9, u32 r10, u32 r11, u32 r12) {
    R0  = idx;
    R1  = target;
    R7  = ai_vars;
    R2  = r2;  R3  = r3;  R4  = r4;  R5  = r5;  R6  = r6;
    R8  = r8;  R9  = r9;  R10 = r10; R11 = r11; R12 = r12;
    GUEST_CALL(sub_80ED154);
}

/* Read the AI attack-type index from the attack variables and dispatch to the
 * matching handler via the single-entry jump table at off_80ED150. */
static void ai_attack_dispatch_sub_80ED13E(u32 ai_vars,
                               u32 r2, u32 r3, u32 r4, u32 r5, u32 r6,
                               u32 r8, u32 r9, u32 r10, u32 r11, u32 r12) {
    u32 idx = LD8(ai_vars + oAIAttackVars_Unk_00);
    u32 target = LD32(A_off_80ED150 + idx);
    call_attack_type_handler(idx, target, ai_vars,
                             r2, r3, r4, r5, r6, r8, r9, r10, r11, r12);
}

/* sub_80ED13E(r7 = AI attack variables, r2..r6/r8..r12 = context) -> all regs */
Next sub_80ED13E(void) {
    ai_attack_dispatch_sub_80ED13E(R7, R2, R3, R4, R5, R6, R8, R9, R10, R11, R12);
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

/* ── nullsub_106 ── */
/* Empty function: just returns to the caller. The typed helper exists only
 * to give the thin adapter something to call without touching registers. */
static void nullsub_106_impl(void) {
    /* no-op */
}

/* nullsub_106: empty function that just returns to caller.
 * inputs: lr
 * outputs: -
 * clobbers: -
 * preserved: - */
Next nullsub_106(void) {
    nullsub_106_impl();
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

/* ── sub_80FE1B4 ── */
/* register form: calls object_setPanelsFromCoordinates outside this file, which has no typed C interface yet */
static void call_object_setPanelsFromCoordinates(u32 r0_val, u32 r1_val) {
    R0 = r0_val;
    R1 = r1_val;
    GUEST_CALL(object_setPanelsFromCoordinates);
}

/* register form: calls object_updateCollisionPanels outside this file, which has no typed C interface yet */
static void call_object_updateCollisionPanels(void) {
    GUEST_CALL(object_updateCollisionPanels);
}

/* register form: calls object_removePanelReserve outside this file, which has no typed C interface yet */
static void call_object_removePanelReserve(u32 r0_val, u32 r1_val) {
    R0 = r0_val;
    R1 = r1_val;
    GUEST_CALL(object_removePanelReserve);
}

/* register form: calls object_setCoordinatesFromPanels outside this file, which has no typed C interface yet */
static void call_object_setCoordinatesFromPanels(void) {
    GUEST_CALL(object_setCoordinatesFromPanels);
}

/* register form: calls object_setCollisionPanelsToCurrent outside this file, which has no typed C interface yet */
static void call_object_setCollisionPanelsToCurrent(void) {
    GUEST_CALL(object_setCollisionPanelsToCurrent);
}

/* register form: calls object_clearFlag outside this file, which has no typed C interface yet */
static void call_object_clearFlag(u32 flag) {
    R0 = flag;
    GUEST_CALL(object_clearFlag);
}

/* register form: calls object_setFlag1 outside this file, which has no typed C interface yet */
static void call_object_setFlag1(u32 flag) {
    R0 = flag;
    GUEST_CALL(object_setFlag1);
}

typedef struct {
    u32 r0;
    u32 add_a;
    u32 add_b;
} MotionStepResult;

/* Advance the battle object by its velocity, refresh its panels, and count
 * down the in-flight move timer (Unk_10). While the timer stays non-negative
 * the object is still gliding (early return). Once it underflows the move
 * commits: the future panel becomes the current panel, reservations are
 * released, coordinates and collision panels are recomputed, the post-move
 * flag bits are updated, and the attack state machine advances to substate
 * 0x0C.
 *
 * Returns r0 plus ADD operands that reproduce the terminal flag pattern:
 *   - early return: `subs r0, #1` -> ADD(orig_timer, 0xFFFFFFFF) (same NZCV,
 *     since ADD(a,-1) is arithmetically SUB(a,1) and the carry/overflow
 *     agree for non-zero a);
 *   - fall-through: terminal flags are N=Z=C=V=0 (LSL sets C=0, nothing else
 *     touches flags before return) -> ADD(1, 0). */
static MotionStepResult step_battle_object_motion(u32 object, u32 ai_vars) {
    u32 x = LD32(object + oBattleObject_X);
    u32 y = LD32(object + oBattleObject_Y);
    u32 vx = LD32(object + oBattleObject_XVelocity);
    u32 vy = LD32(object + oBattleObject_YVelocity);

    ST32(object + oBattleObject_X, x + vx);
    ST32(object + oBattleObject_Y, y + vy);

    /* Original passes r0 = y+vy and r1 = vy into object_setPanelsFromCoordinates. */
    call_object_setPanelsFromCoordinates(y + vy, vy);
    call_object_updateCollisionPanels();

    u32 orig_timer = (u32)LD16(ai_vars + oAIAttackVars_Unk_10);
    u32 timer = orig_timer - 1u;
    ST16(ai_vars + oAIAttackVars_Unk_10, (u16)timer);

    if ((s32)timer >= 0) {
        /* Still gliding. Emulate `subs r0, #1` via ADD(orig_timer, -1). */
        return (MotionStepResult){ timer, orig_timer, 0xFFFFFFFFu };
    }

    /* Landing: future panel becomes the current panel. */
    u8 fx = LD8(object + oBattleObject_FuturePanelX);
    u8 fy = LD8(object + oBattleObject_FuturePanelY);
    ST8(object + oBattleObject_PanelX, fx);
    ST8(object + oBattleObject_PanelY, fy);

    /* Original passes r0 = fx and r1 = fy into object_removePanelReserve. */
    call_object_removePanelReserve(fx, fy);
    call_object_setCoordinatesFromPanels();
    call_object_setCollisionPanelsToCurrent();

    /* End-of-move cleanup + transition to the next attack substate. */
    call_object_clearFlag(0x40);
    call_object_setFlag1(1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT);

    u32 next_state = 0x0C;
    ST16(ai_vars + oAIAttackVars_Unk_00, (u16)next_state);

    /* Terminal flags on fall-through: N=Z=C=V=0. ADD(1,0) yields exactly that. */
    return (MotionStepResult){ next_state, 1u, 0u };
}

Next sub_80FE1B4(void) {
    MotionStepResult s = step_battle_object_motion(R5, R7);
    R0 = s.r0;
    SET_FLAGS_ADD(s.add_a, s.add_b);
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

/* ── sub_80FE21E ── */
/* register form: calls sub_80FE240, AI attack phase 0 handler */
static void call_attack_phase0(u32 state, u32 entry, u32 vars) {
    R0 = state; R1 = entry; R7 = vars;
    GUEST_CALL(sub_80FE240);
    R7 = vars;                       /* original restores r7 via pop {r7,pc} */
}

/* register form: calls battle_aiAttackWindupTick_80FE284, AI attack phase 1 handler */
static void call_attack_phase1(u32 state, u32 entry, u32 vars) {
    R0 = state; R1 = entry; R7 = vars;
    GUEST_CALL(battle_aiAttackWindupTick_80FE284);
    R7 = vars;
}

/* register form: calls sub_80FE2A8, AI attack phase 2 handler */
static void call_attack_phase2(u32 state, u32 entry, u32 vars) {
    R0 = state; R1 = entry; R7 = vars;
    GUEST_CALL(sub_80FE2A8);
    R7 = vars;
}

/* register form: calls sub_80FE30C, AI attack phase 3 handler */
static void call_attack_phase3(u32 state, u32 entry, u32 vars) {
    R0 = state; R1 = entry; R7 = vars;
    GUEST_CALL(sub_80FE30C);
    R7 = vars;
}

/* Tail-dispatch to the current AI attack phase handler.
 * The state byte at [vars+oAIAttackVars_Unk_00] is the table offset (0,4,8,12);
 * it indexes off_80FE230 to pick one of four handlers, each receiving the state
 * in r0 and its own entry address in r1. */
static void ai_attack_dispatch_sub_80FE21E(u32 vars) {
    u32 state = LD8(vars + oAIAttackVars_Unk_00);
    u32 entry = LD32(A_off_80FE230 + state);

    switch (state) {
        case 0:  call_attack_phase0(state, entry, vars); break;
        case 4:  call_attack_phase1(state, entry, vars); break;
        case 8:  call_attack_phase2(state, entry, vars); break;
        case 12: call_attack_phase3(state, entry, vars); break;
        default: break;
    }
}

Next sub_80FE21E(void) {
    ai_attack_dispatch_sub_80FE21E(R7);
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

/* ── sub_80FE30C ── */
/* Countdown phase: when first entered, copy Unk_18 to Unk_10 and start the timer.
 * Each subsequent tick decrements Unk_10; when it reaches zero, clear the attack
 * animation and exit the attack state.
 * r7 = AI attack vars pointer, r5 = battle object pointer. */
static void ai_attack_countdown_tick(u32 battle_object, u32 ai_attack_vars) {
    u8 phase = LD8(ai_attack_vars + oAIAttackVars_Unk_01);
    if (phase == 0) {
        ST8(ai_attack_vars + oAIAttackVars_Unk_01, 4);
        ST16(ai_attack_vars + oAIAttackVars_Unk_10,
             LD16(ai_attack_vars + oAIAttackVars_Unk_18));
        return;
    }
    u32 timer = LD16(ai_attack_vars + oAIAttackVars_Unk_10) - 1;
    ST16(ai_attack_vars + oAIAttackVars_Unk_10, (u16)timer);
    if ((s32)timer > 0)
        return;
    ST8(battle_object + oBattleObject_CurAnim, 0);
    /* register form: calls object_exitAttackState outside this file, which has no typed C interface yet */
    GUEST_CALL(object_exitAttackState);
}

Next sub_80FE30C(void) {
    ai_attack_countdown_tick(R5, R7);
    SET_FLAGS_NZ(R0);
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

/* ── sub_80FE4AC ── */
/* sub_80FE4AC: AI attack-state tick.
 *
 * Inputs:  r5 = BattleObject*, r6/r7 = AI attack vars / context, lr = return address.
 * Outputs: r1.
 * Clobbers: r0, r1, r2, r3, r6, r7, lr, flags.
 *
 * First call initializes the state (sets Unk_01 = 4, CurAnim = 0, copies Unk_18
 * into the timer Unk_10). Subsequent calls tick the timer and, when it reaches
 * zero, exit the attack state via object_exitAttackState.
 */
Next sub_80FE4AC(void) {
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        /* First-time initialization. */
        ST8(R7 + oAIAttackVars_Unk_01, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST16(R7 + oAIAttackVars_Unk_10, LD16(R7 + oAIAttackVars_Unk_18));
    } else {
        /* Tick the countdown timer. */
        u32 timer = LD16(R7 + oAIAttackVars_Unk_10) - 1;
        ST16(R7 + oAIAttackVars_Unk_10, (u16)timer);
        if (!(timer & 0x80000000) && timer != 0) {
            /* timer > 0 signed -> still ticking, return. */
        } else {
            GUEST_CALL(object_exitAttackState);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_80FE5C0 ── */
/* register form: BIOS SVC 6: scale (R0) by fixed-point count (R1). Returns R0. */
static u32 bios_svc6_scale(u32 scale, u32 count) {
    R0 = scale;
    R1 = count;
    swi_call(6);
    return R0;
}

/* Pick the X or Y delta, multiply by the input multiplier, scale, then invoke
 * the BIOS scale helper. Returns the four output registers the original writes. */
static void panel_to_pixel_offset(u32 in_r0, u32 in_r1, u32 in_r2, u32 obj_addr,
                                  u32 *out_r0, u32 *out_r1, u32 *out_r2, u32 *out_r3) {
    u32 r0_val, r1_val, scale;

    if (in_r0 != LD8(obj_addr + oBattleObject_PanelX)) {
        /* X mismatch path: X-delta scaled by in_r2, Y zero, scale 0x00280000. */
        r0_val = (in_r0 - LD8(obj_addr + oBattleObject_PanelX)) * in_r2;
        r1_val = 0;
        scale  = 0x28u << 16;
    } else {
        /* X match path: load PanelY, Y-delta scaled by in_r2, X zero, scale 0x00180000. */
        r1_val = (in_r1 - LD8(obj_addr + oBattleObject_PanelY)) * in_r2;
        r0_val = 0;
        scale  = 0x18u << 16;
    }

    /* The original sets up r0/r1, then overwrites them with the SVC inputs/clears,
     * runs SVC 6, captures the result in r2, restores r0/r1 from the saved pair,
     * and pops PC. r3 keeps the pre-SVC `mov r0,r3` value of the scale all the way
     * out (it is never overwritten after the branch setup). */
    *out_r0 = r0_val;
    *out_r1 = r1_val;
    *out_r3 = scale;
    *out_r2 = bios_svc6_scale(scale, in_r2);
}

Next sub_80FE5C0(void) {
    u32 in_r0 = R0, in_r1 = R1, in_r2 = R2, in_r5 = R5;
    u32 out_r0, out_r1, out_r2, out_r3;

    panel_to_pixel_offset(in_r0, in_r1, in_r2, in_r5,
                          &out_r0, &out_r1, &out_r2, &out_r3);

    R0 = out_r0;
    R1 = out_r1;
    R2 = out_r2;
    R3 = out_r3;
    RETURN_TO_CALLER();
}

/* ── sub_80FE5EE ── */
/* register form: calls object_checkPanelParameters outside this file,
 * which has no typed C interface yet */
static u32 check_panel(u32 x, u32 y, u32 b1, u32 b2) {
    R0 = x; R1 = y; R2 = b1; R3 = b2;
    GUEST_CALL(object_checkPanelParameters);
    return R0;
}

/* register form: calls object_getPanelsIgnoreRowFiltered outside this file,
 * which has no typed C interface yet; r7 = candidate buffer, callee may clobber it */
static void get_panels_filtered(u32 x, u32 y, u32 b1, u32 b2, u32 buf,
                                u32 *out_count, u32 *out_r1) {
    u32 saved_r7 = R7;
    R0 = x; R1 = y; R2 = b1; R3 = b2; R7 = buf;
    GUEST_CALL(object_getPanelsIgnoreRowFiltered);
    *out_count = R0;
    *out_r1 = R1;
    R7 = saved_r7;
}

/* register form: calls GetPositiveSignedRNG outside this file,
 * which has no typed C interface yet */
static u32 positive_rng(void) {
    GUEST_CALL(GetPositiveSignedRNG);
    return R0;
}

/* register form: calls swi_call(6) (ARM_DivMod) for BIOS; returns remainder */
static u32 divmod_remainder(u32 dividend, u32 divisor) {
    R0 = dividend; R1 = divisor;
    swi_call(6);
    return R1;
}

/* Alliance-boundary table: each entry is two u32s at byte_80FE6B8 + alliance*8 */
#define oPanel_XCoord      0x12
#define oPanel_YCoord      0x13

/* Cardinal directions: left, up, right, down (s8 dx/dy pairs from byte_80FE6AC) */
static const s8 NEIGHBOR_DX[4] = {-1,  0,  1,  0};
static const s8 NEIGHBOR_DY[4] = { 0, -1,  0,  1};

/* Select a random walkable panel adjacent to the current tile.
 * Returns the chosen (x, y) packed into *out_x, *out_y.
 * On the fallback-empty path, *out_x = 0 and *out_y carries the callee's r1. */
static void select_panel(u32 target_x, u32 target_y, u32 panel_ptr, u32 battle_obj,
                         u32 *out_x, u32 *out_y) {
    u32 panel_x = LD8(panel_ptr + oPanel_XCoord);
    u32 panel_y = LD8(panel_ptr + oPanel_YCoord);

    u32 alliance = LD8(battle_obj + oBattleObject_Alliance);
    u32 table = A_byte_80FE6B8 + (alliance << 3);
    u32 bound1 = LD32(table);
    u32 bound2 = LD32(table + 4);

    /* register form: stack buffer whose guest address is passed to callees (r7) */
    SP -= 0x30;
    u32 buf = SP + 0x18;

    /* If the target tile differs from the current one, verify the current
     * panel is still passable; if so, stay here. */
    if (target_x != panel_x || target_y != panel_y) {
        if (check_panel(panel_x, panel_y, bound1, bound2) != 0) {
            *out_x = panel_x;
            *out_y = panel_y;
            SP += 0x30;
            return;
        }
    }

    /* Collect up to 4 valid cardinal neighbours into the candidate buffer */
    u32 count = 0;
    for (u32 dir = 0; dir < 4; dir++) {
        u32 nx = (u32)((s32)NEIGHBOR_DX[dir] + (s32)panel_x);
        u32 ny = (u32)((s32)NEIGHBOR_DY[dir] + (s32)panel_y);

        /* Skip if this neighbour is the destination (no need to walk there) */
        if (nx == target_x && ny == target_y)
            continue;

        if (check_panel(nx, ny, bound1, bound2) == 0)
            continue;

        /* Pack: low 5 bits = x, upper bits = y (y shifted left 4) */
        ST8(buf + count, nx | (ny << 4));
        count++;
    }

    /* Fallback: ask the row-filtered panel list when no neighbour qualified */
    if (count == 0) {
        u32 callee_r1;
        /* r1 is panel_y + 1, carried from the last loop iteration (dir 3, dy +1) */
        count = 0;
        get_panels_filtered(target_x, panel_y + 1, bound1, bound2, buf,
                            &count, &callee_r1);
        if (count == 0) {
            /* No valid panels at all; return (0, callee_r1) matching the original */
            *out_x = 0;
            *out_y = callee_r1;
            SP += 0x30;
            return;
        }
    }

    /* Randomly select one candidate: index = RNG % count */
    u32 rng = positive_rng();
    u32 idx = divmod_remainder(rng, count);

    u32 packed = LD8(buf + idx);
    *out_x = (packed << 29) >> 29;   /* packed & 7 – low bits of x */
    *out_y = packed >> 4;

    SP += 0x30;
}

/* sub_80FE5EE(r0=target_x, r1=target_y, r2=panel_ptr, r5=battle_obj) -> r0,r1
 * Outputs r3, r5, r7: r3 is left by callees/SWI; r5 and r7 are preserved. */
Next sub_80FE5EE(void) {
    u32 x, y;
    select_panel(R0, R1, R2, R5, &x, &y);
    R0 = x;
    R1 = y;
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

/* ── sub_8109D98 ── */
/* register form: calls object_clearFlag outside this file, which has no typed C interface yet */
static void call_object_clear_flag(u32 bitfield) {
    R0 = bitfield;
    GUEST_CALL(object_clearFlag);
}

/* register form: calls object_setFlag1 outside this file, which has no typed C interface yet */
static void call_object_set_flag1(u32 flag) {
    R0 = flag;
    GUEST_CALL(object_setFlag1);
}

/* register form: sets SUBS flags for the countdown decrement; typed C cannot express
   cpu flags, and the callee path below will overwrite them via GUEST_CALL if taken. */
static void set_subs_flags_for_decrement(u32 loaded) {
    SET_FLAGS_CMP(loaded, 1);
}

/* Decrement the AI-attack timer at Unk_10.
 * When it reaches zero, mark the object as currently-moving (clear idle flag 0x40,
 * set OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT), reload the timer from Unk_18,
 * and advance the phase at Unk_00 to 0xC.
 * Sets SUBS flags for the non-zero path; on the zero path the callee flags
 * (from GUEST_CALL) overwrite them, which matches the original. */
static u32 ai_attack_countdown(u32 vars) {
    u32 loaded = LD16(vars + oAIAttackVars_Unk_10);
    u32 counter = loaded - 1;
    ST16(vars + oAIAttackVars_Unk_10, counter);
    set_subs_flags_for_decrement(loaded);

    if (counter != 0)
        return counter;

    call_object_clear_flag(0x40);
    call_object_set_flag1(1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT);

    ST16(vars + oAIAttackVars_Unk_10, LD16(vars + oAIAttackVars_Unk_18));
    ST16(vars + oAIAttackVars_Unk_00, 0xC);
    return 0xC;
}

/* sub_8109D98(r7 = AIAttackVars) -> r0, nzcv, r1, r2, lr */
Next sub_8109D98(void) {
    R0 = ai_attack_countdown(R7);
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

/* ── sub_8109DD2 ── */
/* AI attack dispatch: selects one of two handlers using the attack-var type
 * byte stored at the head of the AIAttackVars struct referenced by self. */

/* register form: full-register virtual dispatch — both handlers (sub_8109DEC
 * and sub_8109E4A) receive and return every CPU register, so C parameter
 * passing cannot express the call. */
static void ai_attack_dispatch_sub_8109DD2(u32 self) {
    u8 idx = LD8(self + oAIAttackVars_Unk_00);
    u32 handler_addr = LD32(A_off_8109DE4 + idx);
    R0 = idx;
    R1 = handler_addr;
    u32 saved_r6 = R6;
    if (idx == 0)
        GUEST_CALL(sub_8109DEC);
    else
        GUEST_CALL(sub_8109E4A);
    R6 = saved_r6;
}

Next sub_8109DD2(void) {
    ai_attack_dispatch_sub_8109DD2(R7);
    RETURN_TO_CALLER();
}

/* ── sub_8109E4A ── */
/* register form: calls sub_800AA40 outside this file, which has no typed C interface yet */
static void ai_recalc(u32 extra_vars) {
    R0 = extra_vars;
    GUEST_CALL(sub_800AA40);
}

/* register form: calls list_tryAddUniqueEntry_800AA1A outside this file, which has no typed C interface yet */
static void ai_add_unique(u32 extra_vars) {
    R0 = extra_vars;
    GUEST_CALL(list_tryAddUniqueEntry_800AA1A);
}

/* register form: calls object_exitAttackState outside this file, which has no typed C interface yet */
static u32 ai_exit_attack(u32 r0_val, u32 r1_val) {
    R0 = r0_val;
    R1 = r1_val;
    GUEST_CALL(object_exitAttackState);
    return R0;
}

#define oAIAttackVars_Unk_01 0x01
#define oAIAttackVars_Unk_10_sub_8109E4A 0x10
#define oAIAttackVars_Unk_30_sub_8109E4A 0x30
#define AI_ATTACK_TIMER_INIT 0x28

/* Tick one AI attack countdown.
 * Initialises the timer on first call; decrements each frame.
 * When the timer reaches zero the attack is cleaned up: the extra-vars list
 * is recalculated and updated, the attack pointer is cleared, and the object
 * exits its attack state.
 * Returns the post-decrement timer (path 1) or the callee's r0 (path 2). */
static u32 tick_ai_attack_timer(u32 r5, u32 r7) {
    if (LD8(r7 + oAIAttackVars_Unk_01) == 0) {
        ST8(r7 + oAIAttackVars_Unk_01, 1);
        ST16(r7 + oAIAttackVars_Unk_10_sub_8109E4A, AI_ATTACK_TIMER_INIT);
    }

    u32 timer = LD16(r7 + oAIAttackVars_Unk_10_sub_8109E4A);
    u32 remaining = timer - 1;
    ST16(r7 + oAIAttackVars_Unk_10_sub_8109E4A, remaining);

    if (remaining != 0)
        return remaining;

    /* Timer expired – tear down the attack. */
    u32 extra = LD32(r5 + oBattleObject_ExtraVars + 4);
    ai_recalc(extra);

    extra = LD32(r5 + oBattleObject_ExtraVars + 4);
    ai_add_unique(extra);

    u32 attack_ptr = LD32(r7 + oAIAttackVars_Unk_30_sub_8109E4A);
    ST32(attack_ptr, 0);

    u32 callee_r0 = ai_exit_attack(0, attack_ptr);
    return callee_r0;
}

/* sub_8109E4A(r4, r5, r6, r7, lr) -> nz c v r0 r1 r2 r3 r4 r6 r7 lr */
Next sub_8109E4A(void) {
    R0 = tick_ai_attack_timer(R5, R7);
    SET_FLAGS_CMP(R0 + 1, 1);
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
