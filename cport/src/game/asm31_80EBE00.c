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
 *   +0 -> battle_aiAttackWindupTick, +4 -> battle_aiBusterAttackTick_80EBE54, +8 -> battleObject_aiAttackTickCharge_80EBEB2
 *
 * Note the original indexes the table with the raw byte (`ldr r1, [r1, r0]`),
 * so the stored phase values are byte offsets (0, 4, 8), not entry indices.
 * The tail call ("mov lr, pc; bx r1" + "pop {pc}") means the chosen routine
 * returns straight to our caller, so its register results and flags pass
 * through unchanged.
 *
 * Incoming-register artifacts kept for exactness: R0 = the phase byte (the
 * ldrb overwrites the incoming r0), and R1 = the callee's thumb entry word
 * (e.g. battle_aiAttackWindupTick+1), an artifact of loading the branch target into r1.
 */
#define CHARGE_ATTACK_TABLE 0x080EBE14

Next megamanChargeShotAiAttack_80EBE00(void) {
    u32 phase = LD8(R7 + oAIAttackVars_HitElement);

    switch (phase) {
    case 0:
        R0 = phase;
        R1 = LD32(CHARGE_ATTACK_TABLE + 0); /* battle_aiAttackWindupTick (thumb) */
        GUEST_CALL(battle_aiAttackWindupTick);
        break;
    case 4:
        R0 = phase;
        R1 = LD32(CHARGE_ATTACK_TABLE + 4); /* battle_aiBusterAttackTick_80EBE54 (thumb) */
        GUEST_CALL(battle_aiBusterAttackTick_80EBE54);
        break;
    case 8:
        R0 = phase;
        R1 = LD32(CHARGE_ATTACK_TABLE + 8); /* battleObject_aiAttackTickCharge_80EBEB2 (thumb) */
        GUEST_CALL(battleObject_aiAttackTickCharge_80EBEB2);
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

/* ── battle_aiBusterAttackTick_80EBE54 ── */
/* register form: calls object_setAnimation outside this file, which has no typed C interface yet */
static void set_object_animation(u32 animation) {
    R0 = animation;
    GUEST_CALL(object_setAnimation);
}

/* register form: calls ai_clearAttackVar outside this file, which has no typed C interface yet */
static void run_charge_handler_80EB562(void) {
    GUEST_CALL(ai_clearAttackVar);
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
 *   +0x00 = state (u16, oAIAttackVars_HitElement)
 *   +0x01 = charge-phase byte (oAIAttackVars_AttackElement)
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
        ST16(atk + oAIAttackVars_StepTimer, 0);
    }

    if (LD16(atk + oAIAttackVars_StepTimer) == 1) {
        /* fire tick: buster sound, shot object on the front panel, muzzle flash */
        play_sound(SOUND_BUSTER_6A);
        u32 shot = spawn_object_on_front_panel(
            LD8(atk + oAIAttackVars_AttackBoost2),
            LD16(atk + AIATTACK_DAMAGE_OFFSET),
            0x180000u);
        ST16(atk + oAIAttackVars_ChargeCycles, shot);
        spawn_temp_attack_object(0, 5, r5 + 0x4C, r4_in, atk);
    }

    u32 ticks = LD16(atk + oAIAttackVars_StepTimer) + 1;
    ST16(atk + oAIAttackVars_StepTimer, ticks);
    if (ticks > 4) {
        /* fired all shots: reset the AI attack state */
        ST16(atk + AIATTACK_STATE_OFFSET, 8);
        return 8;
    }
    return ticks;
}

/* battle_aiBusterAttackTick_80EBE54(r7 = AI attack vars, r4/r5 = caller context used by the spawn helper) -> r0
 * The final flags always come from "cmp <counter>, #4" (the mov r0,#8 on the reset path
 * only overwrites n/z, which cmp already leaves as 0/non-zero for counter > 4), so a
 * single CMP of the returned r0 reproduces n/c/v on every path. */
Next battle_aiBusterAttackTick_80EBE54(void) {
    R0 = ai_buster_attack_tick(R7, R4, R5);
    SET_FLAGS_CMP(R0, 4);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackHandlerDispatch_80EBF10 ── */
/* Dispatches an AI attack step through a 3-entry jump table indexed by the
 * attack-type byte at oAIAttackVars_HitElement (offset 0) within the vars block.
 * Sets r0 = type, r1 = handler address (table entry), lr = return point,
 * then calls the handler. All other registers pass through to the callee. */

/* register form: looks up AI attack jump table and calls the selected handler */
static void call_ai_attack_handler(u32 vars_ptr) {
    u32 attack_type = (u32)LD8(vars_ptr + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80EBF24 + attack_type);
    R0 = attack_type;
    R1 = handler;
    LR = 0x080EBF1C;
    if (attack_type == 0)
        GUEST_CALL(aiAttack_genericSpawnAndCountdown_80EBF30);
    else if (attack_type == 4)
        GUEST_CALL(ai_attackChargedRepeatSpawner);
    else
        GUEST_CALL(battle_aiWindRackAttackStep_80EBFF4);
}

/* battle_aiAttackHandlerDispatch_80EBF10(r7 = AI attack vars ptr, lr = caller return addr)
 * Preserves all caller registers except those the handler modifies. */
Next battle_aiAttackHandlerDispatch_80EBF10(void) {
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

    if (LD8(state + 1) == 0) { /* oAIAttackVars_AttackElement: attack not started yet */
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
        ST16(state + oAIAttackVars_StepTimer, 1); /* one tick of attack left */
    }

    /* Count the attack timer down; when it goes negative, return to state 4. */
    u32 timer = LD16(state + oAIAttackVars_StepTimer);
    SET_FLAGS_CMP(timer, 1); /* flags of timer - 1 */
    R0 = timer - 1;
    ST16(state + oAIAttackVars_StepTimer, R0);

    if ((s32)R0 < 0) {
        R0 = 4;
        ST16(state, R0); /* oAIAttackVars_HitElement = 4: back to AI state 4 */
        SET_FLAGS_NZ(R0); /* n/z from the new state, c/v from the subtract */
    }

    R4 = self;
    RETURN_TO_CALLER();
}

/* ── battle_aiWindRackAttackStep_80EBFF4 ── */
/* battle_aiWindRackAttackStep_80EBFF4(r5 = battle object, r7 = AI attack vars):
 * Wind-rack attack AI step. On the first run (attack state byte at r7+1 == 0)
 * it starts the attack animation (10), bumps the counter at
 * oAIData_TempAttackObjectPtr->0x10, sets the timer oAIAttackVars_StepTimer to 10 and marks
 * the attack as started (state byte = 4). Every run then counts Unk_10 down;
 * while it is still >= 0 the attack continues. When it expires, the related
 * object reference is cleared, the pointer at oAIData_TempAttackObjectPtr is reset to NULL
 * and the object leaves its attack state.
 * Flags/r0 on return: those of the timer decrement (SUBS) if the attack is
 * still running; otherwise whatever object_exitAttackState leaves. */
Next battle_aiWindRackAttackStep_80EBFF4(void) {
    if (LD8(R7 + 1 /* oAIAttackVars_AttackElement */) == 0) {
        /* First tick of the attack: play animation 10 and init state. */
        u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
        u32 counterBlock = LD32(aiData + oAIData_TempAttackObjectPtr);

        R1 = LD8(counterBlock + 0x10) + 1;
        ST8(counterBlock + 0x10, R1);

        R0 = 10;
        GUEST_CALL(object_setAnimation);

        ST16(R7 + oAIAttackVars_StepTimer, 10);
        ST8(R7 + 1 /* oAIAttackVars_AttackElement */, 4);
    }

    /* Count the attack timer down. */
    u32 timer = LD16(R7 + oAIAttackVars_StepTimer);
    u32 remaining = timer - 1;
    ST16(R7 + oAIAttackVars_StepTimer, remaining);
    SET_FLAGS_CMP(timer, 1);

    if ((s32)remaining >= 0) {
        /* Still active: keep going next frame. */
        R0 = remaining;
        RETURN_TO_CALLER();
    }

    /* Timer expired: detach and exit the attack state. */
    ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(aiData + oAIData_TempAttackObjectPtr, 0);

    R0 = 0;
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── sub_80EC350 ── */
/* sub_80EC350(r5 = battle object ptr, r7 = AIAttackVars ptr)
 * AI attack handler: computes the attack outcome via sub_80E192C, and if the
 * opposing alliance slot is empty, optionally invokes battle_queueEffectRequest to trigger
 * an extra effect, then exits the attack state.
 *
 * inputs:  r5 r7 r10 lr
 * outputs: r1 r5 r10
 * clobbers: r0 r2 r3 r7 lr + nz/c/v
 * preserved: r4 r6 r10
 */
Next sub_80EC350(void) {
    /* The function follows the standard ARM push/pop idiom used throughout
     * the original: {r4,r6,lr} on the stack, then {r7} on top. We only need
     * the {r7} push because the outer {r4,r6,lr} lives at SP+0..+11 and is
     * consumed by RETURN_TO_CALLER() automatically; however the original
     * expects R4 and R6 to be restored from the stack at function exit, so
     * we must save them ourselves. */
    u32 sp = SP - 12;
    SP = sp;
    ST32(sp + 0, R4);
    ST32(sp + 4, R6);
    ST32(sp + 8, LR);

    SP -= 4;
    ST32(SP, R7);
    u32 aiVars = R7;

    u32 unk02  = LD8(aiVars + oAIAttackVars_InitFlag);
    u32 unk03  = LD8(aiVars + oAIAttackVars_AiDifficultyTier);
    u32 unk0c  = LD32(aiVars + oAIAttackVars_AttackBoost2);
    u32 damage = LD32(aiVars + oAIAttackVars_Damage);

    /* atkBoost (u16) goes into the high half of r7; unk14 (u16) is the low half. */
    u32 atkBoost = ((u32)LD16(aiVars + oAIAttackVars_AttackBoost)) << 16;
    u32 unk14    = (u32)LD16(aiVars + oAIAttackVars_ChipID);
    u32 atkFlags = unk14 + atkBoost;

    /* sub_80E192C(r0=panelX, r1=panelY, r2=unk02, r3=unk03,
     *             r4=unk0c, r6=damage, r7=atkFlags) -> packed outcome in r0 */
    R0 = LD8(R5 + oBattleObject_PanelX);
    R1 = LD8(R5 + oBattleObject_PanelY);
    R2 = unk02;
    R3 = unk03;
    R4 = unk0c;
    R6 = damage;
    R7 = atkFlags;
    GUEST_CALL(sub_80E192C);
    u32 result = R0;

    /* Restore r7 = AIAttackVars (clobbered by sub_80E192C). */
    R7 = LD32(SP);
    SP += 4;

    /* Save R0 (the result) across the alliance-entry lookup, matching the
     * original `push {r0}` immediately before the lookup. */
    SP -= 4;
    ST32(SP, result);

    R0 = LD8(R5 + oBattleObject_Alliance);
    GUEST_CALL(byte_203CF00_getEntryPtr_800BF5C);
    u32 entry_obj = LD32(R0 + 8);
    SET_FLAGS_NZ(entry_obj);

    /* Restore the result into R0 (matching the original's `pop {r0}` between
     * the tst and the bne). The flags above drive the branch. */
    R0 = LD32(SP);
    SP += 4;

    if (entry_obj != 0) {
        /* The slot already has an object; skip the extra-effect call. */
    } else {
        /* r1 = 1 if unk14 >= 0x170, else 0. */
        u32 r1_arg;
        {
            u32 cur_unk14 = LD16(R7 + oAIAttackVars_ChipID);
            SET_FLAGS_CMP((s32)cur_unk14, 0x170);
            if (cur_unk14 < 0x170) {
                r1_arg = 0;
            } else {
                r1_arg = 1;
            }
        }
        R0 = LD8(R5 + oBattleObject_Alliance);
        R1 = r1_arg;
        R2 = result;
        GUEST_CALL(battle_queueEffectRequest);
    }

    GUEST_CALL(object_exitAttackState);

    /* Pop {r4,r6,pc}: restore R4 and R6, and return to the saved LR. */
    R4 = LD32(SP + 0);
    R6 = LD32(SP + 4);
    u32 ret_lr = LD32(SP + 8);
    SP += 12;
    LR = ret_lr;
    (void)LR;
    RETURN_TO_CALLER();
}

/* ── sub_80EC7A6 ── */
/* AI attack dispatch, sub_80EC7A6.
 *
 * r7 points at the AI's attack-variable block; the byte at
 * oAIAttackVars_HitElement selects an entry of the two-word jump table at
 * off_80EC7B8. The original does `mov lr, pc; bx r1` into the selected
 * handler, so the whole function behaves as a tail call: the registers the
 * handler leaves behind (r1, r5, r8-r12) are this function's outputs, and
 * they come back from the dispatched call itself. The push {lr} / pop {pc}
 * pair only saves and restores the return address, which the port's calling
 * convention handles around the call.
 */

/* register form: performs the original's `mov lr, pc; bx r1` call through the
 * jump table. The target is loaded from a table in guest memory, so no
 * generated wrapper covers it, and the handler's register outputs have to be
 * left in the guest registers for this function's caller. */
static void run_ai_attack_handler(u32 ai_vars) {
    u32 selector = LD8(ai_vars + oAIAttackVars_HitElement);
    u32 target = LD32(A_off_80EC7B8 + selector);

    R0 = selector;    /* the selector byte, as the original passes it on */
    R1 = target;      /* the original still holds the table entry in r1 */

    GUEST_CALL_ADDR(target);
}

/* inputs: r5 r6 r7 r10 lr -> outputs: r1 r5 r8 r9 r10 r11 r12 */
Next sub_80EC7A6(void) {
    run_ai_attack_handler(R7);
    RETURN_TO_CALLER();
}

/* ── sub_80EC7C0 ── */
/* sub_80EC7C0: AI attack controller — Thunder/Lightning-style attack that
 * spawns a projectile on its second tick and finishes after four ticks.
 *   inputs:  r5 (caster battle object), r7 (caster AI attack vars), r10, lr
 *   outputs: r1, r5, r8, r9
 */
Next sub_80EC7C0(void) {
    /* First call (Unk_01 == 0): set up animation, counter time and flags. */
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        R0 = 0xE;
        GUEST_CALL(object_setAnimation);

        GUEST_CALL(ai_clearAttackVar);

        GUEST_CALL(object_setDefaultCounterTime);

        R0 = 1u << OBJECT_FLAGS_UNK_BIT_22_BIT;
        GUEST_CALL(object_setFlag1);

        ST8(R7 + oAIAttackVars_AttackElement, 4);
        ST16(R7 + oAIAttackVars_StepTimer, 0);
    }

    /* Tick 1: spawn the thunder projectile at the caster's panel, aimed in
     * their facing direction. R7 is preserved across the spawn call via the
     * guest stack to match the original's push {r7} / pop {r7}. */
    if (LD16(R7 + oAIAttackVars_StepTimer) == 1) {
        u32 buf = SP -= 4;
        ST32(buf, R7);

        GUEST_CALL(object_getFrontDirection);
        u32 panelX = LD8(R5 + oBattleObject_PanelX);
        u32 panelY = LD8(R5 + oBattleObject_PanelY);
        R0 = R0 + panelX;

        R1 = panelY;
        R2 = LD8(R7 + oAIAttackVars_InitFlag);

        u32 unk0c    = LD32(R7 + oAIAttackVars_AttackBoost2);
        u32 damage   = LD32(R7 + oAIAttackVars_Damage);
        u32 atkBoost = LD16(R7 + oAIAttackVars_AttackBoost);
        R4 = unk0c;
        R6 = damage + atkBoost;
        R7 = 0;
        R3 = 0xA << 16;

        GUEST_CALL(spawnThunderObjectMaybe_80c954c);

        R7 = LD32(SP);
        SP += 4;
    }

    /* Advance tick counter and end the attack once it has run for 4 ticks. */
    u32 tick = LD16(R7 + oAIAttackVars_StepTimer) + 1;
    ST16(R7 + oAIAttackVars_StepTimer, tick);
    if ((s32)tick > 4) {
        ST16(R7 + oAIAttackVars_HitElement, 4);
    }

    RETURN_TO_CALLER();
}

/* ── sub_80EC81E ── */
/* sub_80EC81E(r5 = battle object, r7 = AI attack vars):
 * Tick the wind-up timer on the AI attack state. On first entry (when the flag
 * byte at Unk_01 is 0), initialise the timer to 30 frames and set Unk_01 = 4.
 * Each call decrements the timer; once it underflows, clear the battle object's
 * "related object 1" pointer and the AI data's Unk_68 field, then leave the
 * attack state via object_exitAttackState. */
Next sub_80EC81E(void) {
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        ST16(R7 + oAIAttackVars_StepTimer, 0x1E);
        ST8(R7 + oAIAttackVars_AttackElement, 4);
    }

    u32 timer = LD16(R7 + oAIAttackVars_StepTimer);
    ST16(R7 + oAIAttackVars_StepTimer, timer - 1);

    /* BGE after SUBS r0, #1: branches when the pre-decrement value was >= 1
     * as a signed 32-bit quantity (LDRH zero-extends, so the high half is 0). */
    if ((s32)timer >= 1) {
        RETURN_TO_CALLER();
    }

    R0 = 0;
    ST32(R5 + oBattleObject_RelatedObject1Ptr, R0);
    R1 = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(R1 + oAIData_TempAttackObjectPtr, 0);
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── sub_80EC844 ── */
/* sub_80EC844: AI attack handler. Looks up a value in a table indexed by an AI byte,
 * updates a counter on the battle object, and exits the attack state. */
Next sub_80EC844(void) {
    /* Index table at byte_80EC870 by (AI byte * 2), read the entry as a u16. */
    u8 aiByte = (u8)LD8(R7 + oAIAttackVars_AiDifficultyTier);
    u16 amount = LD16(A_byte_80EC870 + (aiByte * 2));

    /* Attempt to apply the value; result is ignored on failure. */
    R0 = amount;
    R1 = 1;
    GUEST_CALL(battleObject_applyHPAndHandleSpecial_800E2FC);
    /* Original does TST r0,r0; BNE loc_80EC85A — both paths land at the same label. */

    /* Add 5 (capped) to the battle object's alliance byte, then leave attack state. */
    R0 = (u8)LD8(R5 + oBattleObject_Alliance);
    R1 = 5;
    R2 = 1;
    GUEST_CALL(tableAddSaturatedByte_800AB46);

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
    if (LD8(R7 + 1 /* oAIAttackVars_AttackElement */) == 0) {
        ST16(R7 + oAIAttackVars_StepTimer, 10);
        ST8(R7 + 1, 4);
    }

    u32 timer = LD16(R7 + oAIAttackVars_StepTimer);
    SET_FLAGS_CMP(timer, 1); /* flags of SUBS timer, #1 */
    timer = timer - 1;
    ST16(R7 + oAIAttackVars_StepTimer, timer);

    if ((s32)timer >= 0) {
        R0 = timer;
        RETURN_TO_CALLER();
    }

    /* Timer expired: detach the object we were tracking and exit. */
    ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(aiData + oAIData_TempAttackObjectPtr, 0);

    R0 = 0;
    R1 = aiData;
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── sub_80EC960 ── */
/* AI attack state-machine dispatcher (sub_80EC960).
 *
 * Reads the attack state byte from the AI attack vars block, uses it as a
 * raw byte offset into the two-entry jump table at off_80EC974, and calls
 * the handler it finds there with r0 = state byte.  Everything the caller
 * cares about (r1, r5, r8..r12) is simply whatever the handler leaves
 * behind, so the call flows straight through. */

/* register form: the handler target is computed at runtime by indexing the
 * jump table with the state byte, so no compile-time function name can be
 * used and no generated wrapper covers this call site. */
static void call_attack_handler_sub_80EC960(u32 handler, u32 state) {
    R0 = state;         /* handler argument: current attack state */
    R1 = handler;       /* the original enters the callee with r1 = table word */
    GUEST_CALL_ADDR(handler);
}

/* One step of the AI attack: pick and run the handler for the state byte.
 * Note the table is indexed by the byte itself, not scaled by 4. */
static void run_ai_attack_step_sub_80EC960(u32 attack_vars) {
    u32 state   = LD8(attack_vars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80EC974 + state);
    call_attack_handler_sub_80EC960(handler, state);
}

/* sub_80EC960(r7 = AI attack vars block)
 * inputs: r2 r5 r6 r7 r10 lr; outputs: r1 r5 r8 r9 r10 r11 r12 (from callee) */
Next sub_80EC960(void) {
    run_ai_attack_step_sub_80EC960(R7);
    RETURN_TO_CALLER();
}

/* ── aiAttack_tickTimerAndExitWhenDone_80ECA0C ── */
/* aiAttack_tickTimerAndExitWhenDone_80ECA0C: tick down the AI attack timer in the object's AIAttackVars.
 * When the counter (oAIAttackVars_StepTimer) drops below zero, leave the attack
 * state. Flags reflect the decrement (SUBS r0, #1) when the callee is not
 * taken; otherwise whatever object_exitAttackState leaves is fine. */
Next aiAttack_tickTimerAndExitWhenDone_80ECA0C(void) {
    u32 count = LD16(R7 + oAIAttackVars_StepTimer);
    u32 remaining = count - 1;

    SET_FLAGS_CMP(count, 1);
    ST16(R7 + oAIAttackVars_StepTimer, (u16)remaining);
    R0 = remaining;

    if ((s32)remaining < 0)
        GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── sub_80ECA34 ── */
/* sub_80ECA34: battle-AI attack dispatcher. The AI attack state block pointed
 * to by R7 holds a selector byte (oAIAttackVars_HitElement) used as a word offset
 * into the two-entry handler table at off_80ECA48 (so the stored value is
 * 0 or 4):
 *   entry 0 -> sub_80ECA50
 *   entry 4 -> sub_80ECAA4
 * The selected handler is invoked with the selector byte in r0 and its own
 * Thumb entry address in r1 (the callees read/keep these), with the other
 * argument registers passing through untouched. The original branches with
 * LR pointing just past the call, so the handler returns straight back to
 * this function's caller; its outputs (r1, r5, r8-r12) become ours. */
Next sub_80ECA34(void) {
    u32 selector = LD8(R7 + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80ECA48 + selector);

    R0 = selector;
    R1 = handler;

    if (selector == 0)
        GUEST_CALL(sub_80ECA50);
    else
        GUEST_CALL(sub_80ECAA4);

    RETURN_TO_CALLER();
}

/* ── sub_80ECA50 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setAnimation has no typed C interface yet */
static inline void call_object_setAnimation(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setAnimation);
}

typedef struct { u32 r3, r4, r5, r6, r7, r8, r9; } call_sub_80EB562_result;
/* register form: generated wrapper: ai_clearAttackVar has no typed C interface yet */
static inline call_sub_80EB562_result call_sub_80EB562(u32 r5) {
    R5 = r5;
    GUEST_CALL(ai_clearAttackVar);
    return (call_sub_80EB562_result){ R3, R4, R5, R6, R7, R8, R9 };
}

typedef struct { u32 r1, r2, r3; } call_object_setDefaultCounterTime_result;
/* register form: generated wrapper: object_setDefaultCounterTime has no typed C interface yet */
static inline call_object_setDefaultCounterTime_result call_object_setDefaultCounterTime(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setDefaultCounterTime);
    return (call_object_setDefaultCounterTime_result){ R1, R2, R3 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_sub_80ECA50;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_sub_80ECA50 call_PlaySoundEffect_sub_80ECA50(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_sub_80ECA50){ R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_sub_80ECA50;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_sub_80ECA50 call_object_getFrontDirection_sub_80ECA50(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_sub_80ECA50){ R0 };
}

typedef struct { u32 r1, r5, r7; } call_sub_80C9184_result;
/* register form: generated wrapper: sub_80C9184 has no typed C interface yet */
static inline call_sub_80C9184_result call_sub_80C9184(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R6 = r6;
    GUEST_CALL(sub_80C9184);
    return (call_sub_80C9184_result){ R1, R5, R7 };
}

/* sub_80ECA50: AI buster-shot attack sequence.
 * State machine: on first call it starts the fire animation and seeds a frame timer;
 * on the next call (timer == 1) it plays the impact sound and spawns the damage
 * projectile; every call advances the small timer and caps the parent state word. */

struct sub_80ECA50_result {
    u32 r1, r5, r7, r8, r9, r10;
};

static struct sub_80ECA50_result buster_shot_attack(u32 r1, u32 r2, u32 r5,
                                                   u32 r7, u32 r8, u32 r9, u32 r10) {
    struct sub_80ECA50_result res;

    if (LD8(r7 + oAIAttackVars_AttackElement) == 0) {
        /* First entry: begin fire animation, init the counter, seed timer to 0. */
        call_object_setAnimation(0xE, r5);

        call_sub_80EB562_result eb = call_sub_80EB562(r5);
        r5 = eb.r5;
        r7 = eb.r7;   /* r7 may differ; every later [r7,#…] uses it */

        call_object_setDefaultCounterTime_result cd =
            call_object_setDefaultCounterTime(r5);

        ST8(r7 + oAIAttackVars_AttackElement, 4);
        ST16(r7 + oAIAttackVars_StepTimer, 0);

        res.r1  = cd.r1;
        res.r5  = r5;
        res.r7  = r7;
        res.r8  = eb.r8;
        res.r9  = eb.r9;
        res.r10 = r10;
    } else if (LD16(r7 + oAIAttackVars_StepTimer) == 1) {
        /* Frame 1 of the timer: fire the buster sound and projectile. */
        call_PlaySoundEffect_result_sub_80ECA50 ps = call_PlaySoundEffect_sub_80ECA50(SOUND_BUSTER_6A, r2);
        r5 = ps.r5;
        r7 = ps.r7;

        call_object_getFrontDirection_result_sub_80ECA50 fd = call_object_getFrontDirection_sub_80ECA50(r5);
        /* fd.r1 is discarded: sub_80C9184 gets panel-Y instead. */

        u32 unk_0c   = LD32(r7 + oAIAttackVars_AttackBoost2);
        u32 boost    = LD16(r7 + oAIAttackVars_AttackBoost);
        u32 damage   = LD32(r7 + oAIAttackVars_Damage) + boost;
        u32 unk_02   = LD8(r7  + oAIAttackVars_InitFlag);
        u32 panel_x  = LD8(r5  + oBattleObject_PanelX);
        u32 panel_y  = LD8(r5  + oBattleObject_PanelY);

        call_sub_80C9184_result c91 = call_sub_80C9184(
            fd.r0 + panel_x, panel_y, unk_02, 0, unk_0c, damage);

        res.r1  = c91.r1;
        res.r5  = c91.r5;
        res.r7  = c91.r7;
        res.r8  = r8;
        res.r9  = r9;
        res.r10 = ps.r10;
    } else {
        /* Neither init nor projectile frame: everything passes through. */
        res.r1  = r1;
        res.r5  = r5;
        res.r7  = r7;
        res.r8  = r8;
        res.r9  = r9;
        res.r10 = r10;
    }

    /* Advance the per-attack frame timer; once past 4, signal completion. */
    u32 t = (u32)((s32)LD16(res.r7 + oAIAttackVars_StepTimer) + 1);
    ST16(res.r7 + oAIAttackVars_StepTimer, t);
    if ((s32)t > 4)
        ST16(res.r7 + oAIAttackVars_HitElement, 4);

    return res;
}

Next sub_80ECA50(void) {
    struct sub_80ECA50_result res = buster_shot_attack(R1, R2, R5, R7, R8, R9, R10);
    R1  = res.r1;
    R5  = res.r5;
    R7  = res.r7;
    R8  = res.r8;
    R9  = res.r9;
    R10 = res.r10;
    RETURN_TO_CALLER();
}

/* ── sub_80ECAA4 ── */
/* sub_80ECAA4(r5 = battleObject, r6 = ?, r7 = aiAttackVars):
 * Tick an AI attack-state machine step. On entry, the AI attack vars' state byte
 * (offset 0x01) gates a one-time initialisation of a 10-tick countdown. While
 * the countdown is positive it just decrements; once it expires, the function
 * clears two pointer slots and exits the current attack state via
 * object_exitAttackState. r1 is left as it was read from the AIData pointer
 * (callers branch on it), but since the function explicitly loads r1 we restore
 * the same value here. */
Next sub_80ECAA4(void) {
    /* One-shot init: if the state byte at offset 1 is zero, kick off a 10-tick
     * countdown (offset 0x10) and mark the state byte as in-progress (4). */
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        ST16(R7 + oAIAttackVars_StepTimer, 0xA);
        ST8(R7 + oAIAttackVars_AttackElement, 4);
    }

    /* Decrement the countdown. */
    u32 countdown = LD16(R7 + oAIAttackVars_StepTimer) - 1;
    ST16(R7 + oAIAttackVars_StepTimer, (u16)countdown);

    /* While the countdown is still positive, just return (r1 unchanged). */
    if ((s32)countdown >= 0) {
        RETURN_TO_CALLER();
    }

    /* Countdown expired: clear related-object pointer on the battle object,
     * clear an AIData slot, then exit the attack state. */
    ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);
    R1 = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(R1 + oAIData_TempAttackObjectPtr, 0);
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── bubbleShotAttack_80ECBB0 ── */
/* bubbleShotAttack_80ECBB0 — trampoline for the "bubble shot" AI attack.
 *
 * This routine only selects a follow-up handler based on the element the
 * attack last hit (the HitElement byte in the AI attack vars) and tail-calls
 * it with r0 = that element and r1 = the selected handler address. The
 * handler runs and returns straight to bubbleShotAttack's caller, leaving
 * its results in r1, r5 and r8-r12.
 *
 * Handler table at off_80ECBC4, indexed by the HitElement byte:
 *   +0 -> ai_attackFrontStrikeStep   (struck an enemy: step in and strike)
 *   +4 -> ai_attackHoldTimeoutUpdate (attack timed out while holding the shot)
 */

/* Helper for the one call no wrapper covers: the jump through the table word.
 * The original enters the handler with r0 = HitElement and r1 = handler
 * address; every other register (r2 r3 r5 r6 r7 r10) passes through as it
 * was on entry, and the handler's register results stay in place. */
static void call_hit_element_handler_bubbleShotAttack_80ECBB0(u32 element, u32 handler) { /* register form: calls through a guest code address (noted by the tool) */
    R0 = element; /* ldrb r0, [r7, #oAIAttackVars_HitElement] before the branch */
    R1 = handler; /* ldr r1, [table, r0] gives the Thumb handler address */
    GUEST_CALL_ADDR(handler);
}

/* Look up the follow-up handler for the element that was hit and jump to it.
 * attackVars (r7 in the original) points at the AI attack vars; the handler
 * receives the untouched r2/r3/r5/r6/r10 and leaves its results in the
 * output registers for bubbleShotAttack's caller. */
static void dispatch_hit_element_handler(u32 attackVars) {
    u32 element = LD8(attackVars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80ECBC4 + element);
    call_hit_element_handler_bubbleShotAttack_80ECBB0(element, handler);
}

Next bubbleShotAttack_80ECBB0(void) {
    dispatch_hit_element_handler(R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackFrontStrikeStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setAnimation has no typed C interface yet */
static inline void call_object_setAnimation_ai_attackFrontStrikeStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setAnimation);
}

typedef struct { u32 r2, r3; } call_object_setDefaultCounterTime_result_ai_attackFrontStrikeStep;
/* register form: generated wrapper: object_setDefaultCounterTime has no typed C interface yet */
static inline call_object_setDefaultCounterTime_result_ai_attackFrontStrikeStep call_object_setDefaultCounterTime_ai_attackFrontStrikeStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setDefaultCounterTime);
    return (call_object_setDefaultCounterTime_result_ai_attackFrontStrikeStep){ R2, R3 };
}

/* register form: generated wrapper: battle_getAIHandlerSlot_80ECC86 has no typed C interface yet */
static inline u32 call_sub_80ECC86(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_getAIHandlerSlot_80ECC86);
    return R0;
}

typedef struct { u32 r3, r4, r5, r6; bool v; } call_spawn_t1_0x5_tempAttackObject_80B8E30_result;
/* register form: generated wrapper: spawn_t1_0x5_tempAttackObject_80B8E30 has no typed C interface yet */
static inline call_spawn_t1_0x5_tempAttackObject_80B8E30_result call_spawn_t1_0x5_tempAttackObject_80B8E30(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(spawn_t1_0x5_tempAttackObject_80B8E30);
    return (call_spawn_t1_0x5_tempAttackObject_80B8E30_result){ R3, R4, R5, R6, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_attackFrontStrikeStep;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_attackFrontStrikeStep call_object_setFlag1_ai_attackFrontStrikeStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_attackFrontStrikeStep){ R1, R2 };
}

typedef struct { u32 r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_ai_attackFrontStrikeStep;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_ai_attackFrontStrikeStep call_PlaySoundEffect_ai_attackFrontStrikeStep(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_ai_attackFrontStrikeStep){ R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_ai_attackFrontStrikeStep;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_ai_attackFrontStrikeStep call_object_getFrontDirection_ai_attackFrontStrikeStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_ai_attackFrontStrikeStep){ R0 };
}

typedef struct { u32 r1, r2, r3, r5, r7; } call_spawn_t3_0x12_80C6ADA_result;
/* register form: generated wrapper: spawn_t3_0x12_80C6ADA has no typed C interface yet */
static inline call_spawn_t3_0x12_80C6ADA_result call_spawn_t3_0x12_80C6ADA(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(spawn_t3_0x12_80C6ADA);
    return (call_spawn_t3_0x12_80C6ADA_result){ R1, R2, R3, R5, R7 };
}

/* AI front-strike step:
 * First invocation (AttackElement==0): spawn the attack object, set element flag,
 * zero the step timer → returns after a 1-frame hold.
 * Subsequent invocations (timer > 1): increment hit counter, play sound,
 * spawn the damage projectile. */

#define SOUND_FRONT_STRIKE    0xAF
#define ANIM_FRONT_STRIKE     0xB
#define ELEMENT_FRONT         4
#define HIT_COUNTER_OFFSET    0x10
#define OBJECT_FLAGS_UNK_BIT_22_BIT_ai_attackFrontStrikeStep  22

typedef struct {
    u32 r1;
} FrontStrikeResult;

static FrontStrikeResult front_strike_step(u32 battleObj, u32 aiVars,
                                           u32 r1_in, u32 r2_in)
{
    if (LD8(aiVars + oAIAttackVars_AttackElement) == 0) {
        /* Setup phase: configure animation, counter, and temp attack object */
        call_object_setAnimation_ai_attackFrontStrikeStep(ANIM_FRONT_STRIKE, battleObj);
        call_object_setDefaultCounterTime_result_ai_attackFrontStrikeStep counter =
            call_object_setDefaultCounterTime_ai_attackFrontStrikeStep(battleObj);

        u32 attackParam = call_sub_80ECC86(battleObj) << 8;
        u32 tier = LD8(aiVars + oAIAttackVars_AiDifficultyTier);
        attackParam += LD8(A_dword_80ECC54 + tier);

        u32 objRef = LD32(battleObj + oBattleObject_AIDataPtr)
                     + oAIData_TempAttackObjectPtr;

        /* register form: spawn_t1 takes R7 as argument and may clobber it;
         * the original preserves R7 across this call via push {r7}/pop {r7} */
        u32 saved_r7 = R7;
        call_spawn_t1_0x5_tempAttackObject_80B8E30(
            attackParam, A_dword_80ECC54,
            counter.r2, counter.r3,
            attackParam, battleObj, objRef);
        R7 = saved_r7;

        r1_in = call_object_setFlag1_ai_attackFrontStrikeStep(1u << OBJECT_FLAGS_UNK_BIT_22_BIT_ai_attackFrontStrikeStep,
                                     battleObj).r1;

        ST8(aiVars + oAIAttackVars_AttackElement, ELEMENT_FRONT);
        ST16(aiVars + oAIAttackVars_StepTimer, 0);
    }

    /* Advance per-step timer (runs every invocation) */
    u32 timer = LD16(aiVars + oAIAttackVars_StepTimer) + 1;
    ST16(aiVars + oAIAttackVars_StepTimer, timer);

    if ((s32)timer <= 1) {
        /* 1-frame hold after setup, or wraparound edge: return current r1 */
        return (FrontStrikeResult){ .r1 = r1_in };
    }

    /* Attack frame: increment hit counter, play sound, spawn damage projectile */
    u32 tempObj = LD32(LD32(battleObj + oBattleObject_AIDataPtr)
                       + oAIData_TempAttackObjectPtr);
    ST8(tempObj + HIT_COUNTER_OFFSET, LD8(tempObj + HIT_COUNTER_OFFSET) + 1);

    call_PlaySoundEffect_ai_attackFrontStrikeStep(SOUND_FRONT_STRIKE, r2_in);

    u32 dir = call_object_getFrontDirection_ai_attackFrontStrikeStep(battleObj).r0;
    u32 targetX = dir + LD8(battleObj + oBattleObject_PanelX);

    u32 tier2 = LD8(aiVars + oAIAttackVars_AiDifficultyTier);
    u32 speedShift = LD8(A_dword_80ECC5C + tier2) << 16;

    u32 panelY = LD8(battleObj + oBattleObject_PanelY);
    u32 boost2 = LD32(aiVars + oAIAttackVars_AttackBoost2);
    u32 damage = LD32(aiVars + oAIAttackVars_Damage)
                 + LD16(aiVars + oAIAttackVars_AttackBoost);
    u32 initFlag = LD8(aiVars + oAIAttackVars_InitFlag);

    u32 r1_out = call_spawn_t3_0x12_80C6ADA(
        targetX, panelY, initFlag, speedShift,
        boost2, battleObj, damage).r1;

    ST16(aiVars + oAIAttackVars_HitElement, ELEMENT_FRONT);

    return (FrontStrikeResult){ .r1 = r1_out };
}

/* ai_attackFrontStrikeStep(r2, r3, r5=battleObj, r7=aiVars, r10, lr) -> r1 */
Next ai_attackFrontStrikeStep(void) {
    R1 = front_strike_step(R5, R7, R1, R2).r1;
    RETURN_TO_CALLER();
}

/* ── ai_attackHoldTimeoutUpdate ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_ai_attackHoldTimeoutUpdate;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_ai_attackHoldTimeoutUpdate call_object_exitAttackState_ai_attackHoldTimeoutUpdate(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_ai_attackHoldTimeoutUpdate){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* ai_attackHoldTimeoutUpdate: per-frame timeout for the AI's held-attack state.
 * r5 = battle object, r6 = pass-through argument for object_exitAttackState,
 * r7 = this attack's variable block.
 * Returns the r1 value the original leaves for its caller: on the early path
 * it is untouched (passed through), after the exit-state call it is whatever
 * object_exitAttackState left there. */
static u32 attackHoldTimeoutUpdate(u32 r1_in, u32 battleObject, u32 r6, u32 attackVars) {
    /* First tick of the attack: no element assigned yet, so start a
     * 20-frame hold and mark the attack as element 4. */
    if (LD8(attackVars + oAIAttackVars_AttackElement) == 0) {
        ST16(attackVars + oAIAttackVars_StepTimer, 0x14);
        ST8(attackVars + oAIAttackVars_AttackElement, 4);
    }

    /* Count the hold down; while frames remain, nothing else happens. */
    u32 timer = LD16(attackVars + oAIAttackVars_StepTimer);
    timer -= 1;
    ST16(attackVars + oAIAttackVars_StepTimer, timer);
    if ((s32)timer >= 0)
        return r1_in;   /* hold still active */

    /* Hold expired: drop the temporary attack object and leave the attack state. */
    ST32(battleObject + oBattleObject_RelatedObject1Ptr, 0);
    u32 aiData = LD32(battleObject + oBattleObject_AIDataPtr);
    ST32(aiData + oAIData_TempAttackObjectPtr, 0);

    call_object_exitAttackState_result_ai_attackHoldTimeoutUpdate result =
        call_object_exitAttackState_ai_attackHoldTimeoutUpdate(battleObject, r6, attackVars);
    return result.r1;
}

Next ai_attackHoldTimeoutUpdate(void) {
    R1 = attackHoldTimeoutUpdate(R1, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── battle_getAIHandlerSlot_80ECC86 ── */
/* Map a battle object's AI index to its AI behavior slot.
 * Reads the object's AI data pointer, takes the AI index from it, and looks the
 * index up in a small table (0 = no special AI, 2 = custom AI handler).
 * Returns the table byte in r0; lr is the return address and r5 the object. */

/* AI index -> handler slot table at 0x080ECC94 */
#define AI_SLOT_TABLE A_off_80ECC94

static u32 ai_index_to_slot(u32 battle_object) {
    u32 ai_data = LD32(battle_object + oBattleObject_AIDataPtr);
    u32 ai_index = LD8(ai_data + oAIData_AIIndex);
    return LD8(AI_SLOT_TABLE + ai_index);
}

Next battle_getAIHandlerSlot_80ECC86(void) {
    R0 = ai_index_to_slot(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80ECF2E ── */
/* sub_80ECF2E: AI attack-state dispatcher.
 *
 * Reads the current attack-state byte from the AI attack variable block
 * (r7) and calls the matching handler through the table at off_80ECF40:
 *   state 0 -> sub_80ECF48
 *   state 1 -> sub_80ECF8E
 * The state byte is passed in r0, the handler's own table address in r1
 * (exactly as the original leaves them before `bx r1`).  Everything the
 * handler leaves behind — r1, r5, r8-r12 — is this function's result;
 * r0/r2/r3/r4/r6/r7/lr are scratch. */

/* register form: indirect call through the AI attack-state jump table
 * (mov lr, pc; bx r1 with the table entry); no wrapper covers it */
static u32 call_attack_handler_sub_80ECF2E(u32 state, u32 handler) {
    R0 = state;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Look up the handler for the current attack state and call it. */
static void ai_attack_dispatch(u32 vars) {
    u32 state = LD8(vars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80ECF40 + state);
    call_attack_handler_sub_80ECF2E(state, handler);
}

Next sub_80ECF2E(void) {
    ai_attack_dispatch(R7);
    RETURN_TO_CALLER();
}

/* ── sub_80ECF48 ── */
/* sub_80ECF48:
 *   inputs:  r3 r5 r7 r10 lr
 *   outputs: r1 r5
 *   clobbers: nz c v r0 r2 r3 lr
 *   preserved: r4 r5 r6 r7
 *
 * Fold the AI attack boost into the damage field, then drive the attack
 * object's animation, refresh the default counter timer, spawn a temporary
 * attack child object, flag the parent, and stash a few AI bookkeeping
 * fields.
 */
Next sub_80ECF48(void) {
    u32 saved_r4 = R4;

    /* damage += attackBoost */
    u32 boost = LD16(R7 + oAIAttackVars_AttackBoost);
    u32 damage = LD32(R7 + oAIAttackVars_Damage) + boost;
    ST32(R7 + oAIAttackVars_Damage, damage);

    /* battle_findFilteredPanelCountAhead() -> byte stored at Unk_16 */
    GUEST_CALL(battle_findFilteredPanelCountAhead);
    ST8(R7 + oAIAttackVars_TargetPanelX, (u8)R0);

    ST8(R7 + oAIAttackVars_TargetPanelY, 3);
    ST8(R5 + oBattleObject_CurAnim, 0x0A);

    GUEST_CALL(object_setDefaultCounterTime);

    /* spawn_t1_0x5_tempAttackObject_80B8E30(r4 = 0x1D, r5 = parent,
     *                                       r7 = r5->AIDataPtr + 0x68,
     *                                       r3/r10 passed through) */
    {
        u32 saved_r7 = R7;
        R4 = 0x1D;
        R7 = LD32(R5 + oBattleObject_AIDataPtr) + 0x68;
        GUEST_CALL(spawn_t1_0x5_tempAttackObject_80B8E30);
        R7 = saved_r7;
    }

    R0 = (u32)1 << OBJECT_FLAGS_UNK_BIT_22_BIT;
    GUEST_CALL(object_setFlag1);

    ST8(R7 + oAIAttackVars_SubStepIndex, 9);
    ST32(R7 + oAIAttackVars_ScratchVar, (u32)-1);
    ST16(R7 + oAIAttackVars_HitElement, 4);

    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_80ECF8E ── */
/* sub_80ECF8E: AI attack state for a battle object.
 *   inputs:  r5 = battle object, r6 = scratch, r7 = AI attack vars, lr
 *   outputs: r1, r5
 *   clobbers (caller does not read): nz c v r0 r2 r3 r7 lr
 *   preserved: r4 r5 r6
 *
 * Per-frame attack routine: on entry with state==0, primes the AI data and
 * plays anim 0xD, then sets up per-attack parameters and spawns an effect
 * object at the target panel. Every frame it decrements a tick counter;
 * when it expires it bounces the target panel around the battlefield
 * (flipping direction and consulting battle_findFilteredPanelCountAhead for a row hint), then
 * decrements an iteration counter. Once iterations are exhausted, it
 * tears down the attack state. */

Next sub_80ECF8E(void) {
    /* r4/r6 are push-saved in the original; restore them so the caller
     * sees the values it passed in. r5 is never written in this function. */
    u32 r5 = R5;
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;

    u32 state = LD8(R7 + oAIAttackVars_AttackElement);

    if (state != 1) {
        if (state != 2) {
            /* First entry: poke the AI data sub-struct, set anim 0xD. */
            u32 aiData = LD32(r5 + oBattleObject_AIDataPtr);
            u32 unk68  = LD32(aiData + oAIData_TempAttackObjectPtr);
            ST8(unk68 + 0x10, 1);
            R0 = 0xD;
            GUEST_CALL(object_setAnimation);
        }

        /* ---- Per-attack setup ---- */
        ST8(R7 + oAIAttackVars_AttackElement, 1);
        ST16(R7 + oAIAttackVars_StepTimer, 8);

        {
            u32 param  = (LD32(R7 + oAIAttackVars_AttackBoost2) & 0xFFFFu) | 0x10000u;
            u32 col    = LD8(R7 + oAIAttackVars_TargetPanelX);
            u32 row    = LD8(R7 + oAIAttackVars_TargetPanelY);
            u32 side   = LD8(R7 + oAIAttackVars_InitFlag);
            u32 damage = LD32(R7 + oAIAttackVars_Damage);

            R0 = col;  R1 = row;  R2 = side;  R3 = 0;
            R4 = param;
            R6 = damage;
            GUEST_CALL(sub_80C5F2C);
        }

        {
            u32 col = LD8(R7 + oAIAttackVars_TargetPanelX);
            u32 row = LD8(R7 + oAIAttackVars_TargetPanelY);
            R0 = col;  R1 = row;
            GUEST_CALL(object_getCoordinatesForPanels); /* R0=x, R1=y */
            /* Original: mov r2, r1 ; mov r1, r0  — save y first. */
            R2 = R1;
            R1 = R0;
            R3 = 0;
            R4 = 0x2F;
            GUEST_CALL(spawn_t1_0x0_EffectObject);
        }

        if (R0 != 0) {
            /* Spawn succeeded: load the AI attack timer value into R1
             * (matches the original `ldrb r1`) and write it to the
             * new effect object. */
            R1 = LD8(R7 + oAIAttackVars_TimerOrCycles);
            ST16(R0 + oBattleObject_Timer, R1);
        }
        /* If spawn returned NULL, R1 stays as spawn_t1_0x0_EffectObject left it,
         * matching the original which skips the ldrb in that case. */
    }
    /* state == 1: skip setup entirely, R1 = caller's input value. */

    /* ---- Per-frame timer tick ---- */
    {
        u32 orig_timer = LD16(R7 + oAIAttackVars_StepTimer);
        u32 timer = orig_timer - 1;
        ST16(R7 + oAIAttackVars_StepTimer, (u16)timer);
        SET_FLAGS_CMP(orig_timer, 1);  /* `subs r0, r0, #1` */

        if ((s32)timer >= 0) {
            goto done;  /* bge locret: still ticking */
        }
    }

    /* ---- Timer expired: bounce the target panel ---- */
    {
        u32 dir = LD32(R7 + oAIAttackVars_ScratchVar);
        u32 row = LD8(R7 + oAIAttackVars_TargetPanelY);
        s32 probe = (s32)row + (s32)dir;

        if (probe >= 1 && probe <= 3) {
            /* Step along the column stays in range: take the step. */
            u32 new_row = (u32)probe;
            ST8(R7 + oAIAttackVars_TargetPanelY, (u8)new_row);
            R1 = new_row;
        } else {
            /* Out of range: flip the column-direction step and ask the
             * helper which way the row should now nudge. */
            u32 neg_dir = (u32)(-(s32)dir);
            ST32(R7 + oAIAttackVars_ScratchVar, neg_dir);
            GUEST_CALL(battle_findFilteredPanelCountAhead);  /* R0 = row hint */

            u32 old_col = LD8(R7 + oAIAttackVars_TargetPanelX);
            u32 hint    = R0;

            if (hint == old_col) {
                /* Helper says same row: bounce straight along the row. */
                u32 new_row = (u32)((s32)row - (s32)dir);
                ST8(R7 + oAIAttackVars_TargetPanelY, (u8)new_row);
                R1 = new_row;
            } else {
                /* Helper gives a different row: nudge toward it. */
                s32 delta = (hint > old_col) ? 1 : -1;
                u32 new_col = (u32)((s32)old_col + delta);
                ST8(R7 + oAIAttackVars_TargetPanelX, (u8)new_col);
                R1 = new_col;
            }
        }
    }

    /* ---- Decrement bounce iteration counter ---- */
    {
        u32 orig_lives = LD8(R7 + oAIAttackVars_SubStepIndex);
        u32 lives = orig_lives - 1;
        ST8(R7 + oAIAttackVars_SubStepIndex, (u8)lives);
        SET_FLAGS_CMP(orig_lives, 1);  /* `subs r0, r0, #1` for the beq below */

        if (lives != 0) {
            /* More iterations left: loop back into the setup phase. */
            ST8(R7 + oAIAttackVars_AttackElement, 2);
            /* `mov r0, #2` does not set flags in Thumb; flags stay from SUBS. */
            goto done;
        }

        /* Attack finished: drop related-object pointer, clear AI data
         * sub-struct reference, then exit the attack state. */
        u32 aiData = LD32(r5 + oBattleObject_AIDataPtr);
        ST32(r5 + oBattleObject_RelatedObject1Ptr, 0);
        ST32(aiData + oAIData_TempAttackObjectPtr, 0);
        R0 = 0;
        R1 = aiData;
        GUEST_CALL(object_exitAttackState);
        /* Flags at return come from object_exitAttackState (matches original). */
    }

done:
    R4 = saved_r4;
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── ai_attack_dispatch_80ED13E ── */
/* register form: calls battle_object_aiSnakeArmUpdate_80ED154 outside this file, which has no typed C interface yet */
static void call_attack_type_handler(u32 idx, u32 target, u32 ai_vars,
                                     u32 r2, u32 r3, u32 r4, u32 r5, u32 r6,
                                     u32 r8, u32 r9, u32 r10, u32 r11, u32 r12) {
    R0  = idx;
    R1  = target;
    R7  = ai_vars;
    R2  = r2;  R3  = r3;  R4  = r4;  R5  = r5;  R6  = r6;
    R8  = r8;  R9  = r9;  R10 = r10; R11 = r11; R12 = r12;
    GUEST_CALL(battle_object_aiSnakeArmUpdate_80ED154);
}

/* Read the AI attack-type index from the attack variables and dispatch to the
 * matching handler via the single-entry jump table at off_80ED150. */
static void ai_attack_dispatch_sub_80ED13E(u32 ai_vars,
                               u32 r2, u32 r3, u32 r4, u32 r5, u32 r6,
                               u32 r8, u32 r9, u32 r10, u32 r11, u32 r12) {
    u32 idx = LD8(ai_vars + oAIAttackVars_HitElement);
    u32 target = LD32(A_off_80ED150 + idx);
    call_attack_type_handler(idx, target, ai_vars,
                             r2, r3, r4, r5, r6, r8, r9, r10, r11, r12);
}

/* ai_attack_dispatch_80ED13E(r7 = AI attack variables, r2..r6/r8..r12 = context) -> all regs */
Next ai_attack_dispatch_80ED13E(void) {
    ai_attack_dispatch_sub_80ED13E(R7, R2, R3, R4, R5, R6, R8, R9, R10, R11, R12);
    RETURN_TO_CALLER();
}

/* ── battle_elementHitDispatcher ── */
/* battle_elementHitDispatcher (0x080ED374).
 *
 * Trampoline: it picks one of three AI attack handlers from the jump table at
 * off_80ED388 using the hit-element byte stored in the AI attack vars, then
 * jumps to it (mov lr, pc / bx r1, so the handler returns through pop {pc}
 * straight back to the dispatcher's caller). The handler is entered with
 * r0 = HitElement byte and r1 = its own table entry address; r3, r5, r6, r7
 * and r10 pass through untouched. Everything the dispatcher "outputs"
 * (r1, r5, r8-r12) is simply whatever the chosen handler leaves behind, so
 * nothing needs to be copied through return values here. */

/* register form: the call target is read from the game's jump table at
 * off_80ED388, and the callee both consumes live guest registers
 * (r3, r5, r6, r7, r10) and produces the dispatcher's live outputs
 * (r1, r5, r8-r12), so this call cannot be typed. */
static void element_hit_dispatch(u32 r3, u32 r5, u32 r6, u32 r7, u32 r10) {
    u32 element = LD8(r7 + oAIAttackVars_HitElement);

    /* ldr with a register offset is not scaled: the byte already holds the
     * byte offset into the table (0, 4 or 8). */
    u32 handler = LD32(A_off_80ED388 + element);

    R0 = element;   /* handler sees the hit-element byte in r0 */
    R1 = handler;   /* ...and its own table entry address in r1 */
    R3 = r3;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R10 = r10;

    GUEST_CALL_ADDR(handler);

    /* On return r1, r5, r8, r9, r10, r11, r12 hold the handler's results;
     * those are exactly the dispatcher's outputs, left in place. */
}

Next battle_elementHitDispatcher(void) {
    element_hit_dispatch(R3, R5, R6, R7, R10);
    RETURN_TO_CALLER();
}

/* ── aiAttackStepTick_80ED394 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r5, r10; } call_sub_80ED3B6_result;
/* register form: generated wrapper: sub_80ED3B6 has no typed C interface yet */
static inline call_sub_80ED3B6_result call_sub_80ED3B6(u32 r3, u32 r5, u32 r7) {
    R3 = r3;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(sub_80ED3B6);
    return (call_sub_80ED3B6_result){ R1, R5, R10 };
}

/* AI attack step handler. Each tick it decrements the attack step timer;
 * on the very first tick (attack element still 0) it seeds the timer with
 * 10 ticks and marks the element as armed (4). When the countdown expires
 * it stores the hit element and hands control to the follow-up routine,
 * which produces the outgoing r1/r5/r10 state. */
struct AiAttackTickResult {
    u32 r1;
    u32 r5;
    u32 r10;
};

static struct AiAttackTickResult ai_attack_tick_sub_80ED394(u32 r1, u32 r3, u32 r5, u32 r7, u32 r10) {
    /* Arm a freshly selected attack: set the countdown and attack element. */
    if (LD8(r7 + oAIAttackVars_AttackElement) == 0) {
        ST16(r7 + oAIAttackVars_StepTimer, 10);
        ST8(r7 + oAIAttackVars_AttackElement, 4);
    }

    /* Count the step timer down; while it is still positive, wait. */
    u32 timer = LD16(r7 + oAIAttackVars_StepTimer) - 1;
    ST16(r7 + oAIAttackVars_StepTimer, timer);
    if ((s32)timer > 0)
        return (struct AiAttackTickResult){ r1, r5, r10 };

    /* Timer expired: record the hit element and run the attack. */
    ST16(r7 + oAIAttackVars_HitElement, 4);
    call_sub_80ED3B6_result next = call_sub_80ED3B6(r3, r5, r7);
    return (struct AiAttackTickResult){ next.r1, r5, r10 };
}

/* aiAttackStepTick_80ED394: in r3 r5 r7 r10, out r1 r5 r10 (r5/r10 pass through unchanged) */
Next aiAttackStepTick_80ED394(void) {
    struct AiAttackTickResult res = ai_attack_tick_sub_80ED394(R1, R3, R5, R7, R10);
    R1 = res.r1;
    R5 = res.r5;
    R10 = res.r10;
    RETURN_TO_CALLER();
}

/* ── ai_attackSpawnTimedTempObject ── */
/* AI attack step: drive the temporary attack object spawned by an AI attack.
 *
 * r5 holds the acting BattleObject, r7 this attack's variable block. While
 * oAIAttackVars_AttackElement is still zero the temp attack object's
 * animation is (re)started, the step timer is armed (10 steps) and the attack
 * element moves to its countdown phase (4). Every call after that decrements
 * the timer; once it reaches zero the attack is torn down: the "in progress"
 * flag is cleared, the temp object reference is dropped from both the battle
 * object and the AI data, and the object leaves its attack state.
 *
 * r1 on exit is never computed by this code: it is either the entry value
 * (early paths) or whatever the last callee left behind, so the adapter does
 * not write it. */

/* register form: direct bl calls to functions that have no generated wrapper */
static void guest_battle_setObjectAnim(u32 object, u32 anim) {
    R0 = object;
    R1 = anim;
    GUEST_CALL(battle_setObjectAnim);
}

static void guest_object_clearFlag(u32 flags) { /* register form: calls object_clearFlag, which has no typed C interface yet (noted by the tool) */
    R0 = flags;
    GUEST_CALL(object_clearFlag);
}

static void guest_object_exitAttackState(u32 arg, u32 aiData) { /* register form: calls object_exitAttackState, which has no typed C interface yet (noted by the tool) */
    /* r1 holds the AI data pointer across the call in the original */
    R0 = arg;
    R1 = aiData;
    GUEST_CALL(object_exitAttackState);
}

static void run_attackSpawnTimedTempObject(u32 object, u32 r6_unused, u32 vars) {
    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        u32 tempObject = LD32(LD32(object + oBattleObject_AIDataPtr)
                              + oAIData_TempAttackObjectPtr);
        guest_battle_setObjectAnim(tempObject, 0);
        ST16(vars + oAIAttackVars_StepTimer, 10);
        ST8(vars + oAIAttackVars_AttackElement, 4);
    }

    u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);
    if ((s32)timer > 0)
        return;                                 /* still counting down */

    guest_object_clearFlag(1u << 22);           /* temp-object attack flag */
    ST32(object + oBattleObject_RelatedObject1Ptr, 0);
    u32 aiData = LD32(object + oBattleObject_AIDataPtr);
    ST32(aiData + oAIData_TempAttackObjectPtr, 0);
    guest_object_exitAttackState(0, aiData);
}

Next ai_attackSpawnTimedTempObject(void) {
    run_attackSpawnTimedTempObject(R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── aiAttack_dispatchByHitElement ── */
/* aiAttack_dispatchByHitElement — dispatch the AI attack handler for the element that
 * was just hit. The two-entry table at off_80ED75C is indexed by the raw HitElement
 * byte of the AI vars block (r7): entry 0 is the default handler, the other entry is
 * the walk-forward-then-attack handler. The chosen handler runs with the caller's
 * state (r2, r5, r6, r7, r10 pass through untouched) and leaves this routine's only
 * outputs — r1, r5, r8-r12 — in the guest registers; r0-r4/r6/r7 and the flags are
 * free to clobber. */

/* register form: the handler address is fetched from a jump table at run time, and the
 * handler produces this function's outputs (r1, r5, r8-r12) directly in the guest
 * registers, so they must flow through unmodified to our caller. */
static void call_attack_element_handler(u32 handler, u32 element) {
    R0 = element;
    R1 = handler;           /* the original's table load leaves the target in r1 */
    GUEST_CALL_ADDR(handler);
}

/* Read the hit element, pick the matching handler from the table, and call it. */
static void dispatch_by_hit_element(u32 aiVars) {
    u32 element = LD8(aiVars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80ED75C + element);
    call_attack_element_handler(handler, element);
}

Next aiAttack_dispatchByHitElement(void) {
    dispatch_by_hit_element(R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackTelegraphStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setAnimation has no typed C interface yet */
static inline void call_object_setAnimation_ai_attackTelegraphStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setAnimation);
}

typedef struct { u32 r3, r4, r5, r6, r7, r8, r9; bool v; } call_ai_clearAttackVar_result;
/* register form: generated wrapper: ai_clearAttackVar has no typed C interface yet */
static inline call_ai_clearAttackVar_result call_ai_clearAttackVar(u32 r5) {
    R5 = r5;
    GUEST_CALL(ai_clearAttackVar);
    return (call_ai_clearAttackVar_result){ R3, R4, R5, R6, R7, R8, R9, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_attackTelegraphStep;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_attackTelegraphStep call_object_setFlag1_ai_attackTelegraphStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_attackTelegraphStep){ R1, R2 };
}

typedef struct { u32 r1, r2, r3, r5, r6, r7, r10; } call_PlaySoundEffect_result_ai_attackTelegraphStep;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_ai_attackTelegraphStep call_PlaySoundEffect_ai_attackTelegraphStep(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_ai_attackTelegraphStep){ R1, R2, R3, R5, R6, R7, R10 };
}

/* Telegraph phase of an enemy AI attack. r7 points at the acting object's
 * AI attack variable block, r5 at the object itself.
 *
 * While no attack element is queued (AttackElement == 0) this starts the
 * wind-up once: play the charge animation, clear the AI attack state, raise
 * the object's "charging attack" flag, reset the step timer and mark the
 * attack as telegraphing (AttackElement = 4). Every frame afterwards the
 * step timer ticks; the charge sound plays on its first tick, and once more
 * than 4 ticks have elapsed the hit element is armed so the next step can
 * fire the attack. */
static void ai_attackTelegraphStep_run(u32 vars, u32 object, u32 soundArg2) {
    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        call_object_setAnimation_ai_attackTelegraphStep(0xE, object);
        call_ai_clearAttackVar(object);
        call_object_setFlag1_ai_attackTelegraphStep(1u << OBJECT_FLAGS_UNK_BIT_22_BIT, object);
        ST16(vars + oAIAttackVars_StepTimer, 0);
        ST8(vars + oAIAttackVars_AttackElement, 4);
    }

    u32 timer = LD16(vars + oAIAttackVars_StepTimer);
    if (timer == 1)
        call_PlaySoundEffect_ai_attackTelegraphStep(0xF8, soundArg2);

    timer += 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);
    if ((s32)timer > 4)
        ST16(vars + oAIAttackVars_HitElement, 4);
}

Next ai_attackTelegraphStep(void) {
    ai_attackTelegraphStep_run(R7, R5, R2);
    RETURN_TO_CALLER();
}

/* ── airspinAttack_80EDCC0 ── */
/* airspinAttack_80EDCC0 -- AI "air spin" attack dispatcher.
 *
 * r7 points at the AI attack variable block.  The byte at
 * oAIAttackVars_HitElement selects an entry in the jump table at
 * off_80EDCD4 (ai_beastSpAttackStep for the neutral element,
 * ai_attack_stepTick_80EDD5C otherwise).  The selected handler is called with the
 * element in r0.  Because the original loads the handler address into
 * r1 immediately before `bx r1` and the handlers preserve r1, the
 * table entry must also be left in r1 across the call; the handler's
 * register results (r1, r5, r8-r12) are this function's outputs. */

/* register form: the handler is reached through a computed code
 * address, and its register results (r1, r5, r8-r12) must stay in the
 * guest register file, so this call cannot be an ordinary typed
 * wrapper. */
static void call_element_handler(u32 handler, u32 element) {
    R0 = element;
    R1 = handler;               /* r1 holds the table entry across the call */
    GUEST_CALL_ADDR(handler);
}

/* Look up the per-element handler for the last hit and run it. */
static void run_airspin_attack(u32 attackVars) {
    u32 element = LD8(attackVars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80EDCD4 + element);
    call_element_handler(handler, element);
}

/* inputs: r7 = AI attack variable block; r5, r6, r10 are passed
 * through to the handler (r5/r8-r12 are its outputs, r6/r7 and the
 * flags are scratch). */
Next airspinAttack_80EDCC0(void) {
    run_airspin_attack(R7);
    RETURN_TO_CALLER();
}

/* ── ai_beastSpAttackStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_setAnimation has no typed C interface yet */
static inline void call_object_setAnimation_ai_beastSpAttackStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setAnimation);
}

typedef struct { u32 r3; bool c, v; } call_object_setDefaultCounterTime_result_ai_beastSpAttackStep;
/* register form: generated wrapper: object_setDefaultCounterTime has no typed C interface yet */
static inline call_object_setDefaultCounterTime_result_ai_beastSpAttackStep call_object_setDefaultCounterTime_ai_beastSpAttackStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setDefaultCounterTime);
    return (call_object_setDefaultCounterTime_result_ai_beastSpAttackStep){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_beastSpAttackStep;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_beastSpAttackStep call_object_setFlag1_ai_beastSpAttackStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_beastSpAttackStep){ R1, R2 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_ai_beastSpAttackStep;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_ai_beastSpAttackStep call_object_getFrontDirection_ai_beastSpAttackStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_ai_beastSpAttackStep){ R0 };
}

typedef struct { u32 r1, r2; } call_object_clearFlag_result_ai_beastSpAttackStep;
/* register form: generated wrapper: object_clearFlag has no typed C interface yet */
static inline call_object_clearFlag_result_ai_beastSpAttackStep call_object_clearFlag_ai_beastSpAttackStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_clearFlag);
    return (call_object_clearFlag_result_ai_beastSpAttackStep){ R1, R2 };
}

/* Jump-table base for difficulty-tier-dependent Beast attack functions */
#define SP_ATTACK_TABLE  A_off_80EDD54

/* register form: indirect call through difficulty-tier jump table;
 * r4, r6, r7 must be saved and restored because the outer function's
 * contract preserves them, and the callee's push/pop (r7) and
 * argument setup (r4, r6) overwrite them. */
static u32 sp_attack_fire(u32 funcPtr, u32 x, u32 y, u32 initFlag,
                          u32 boost2, u32 obj, u32 damage) {
    u32 sv4 = R4, sv6 = R6, sv7 = R7;
    R0 = x; R1 = y; R2 = initFlag; R3 = 0;
    R4 = boost2; R5 = obj; R6 = damage; R7 = funcPtr;
    GUEST_CALL_ADDR(funcPtr);
    u32 result = R1;
    R4 = sv4; R6 = sv6; R7 = sv7;
    return result;
}

/* Advance one step of Beast's special-attack AI state machine.
 * obj = battle object pointer, vars = AI attack variables block.
 * Returns r1 (the only output the function body computes). */
static u32 beast_sp_attack_step(u32 obj, u32 vars, u32 r10, u32 r1) {
    /* First frame: assign the element, set animation, start the countdown */
    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        call_object_setAnimation_ai_beastSpAttackStep(0xc, obj);
        call_object_setDefaultCounterTime_ai_beastSpAttackStep(obj);
        r1 = call_object_setFlag1_ai_beastSpAttackStep(0x400040, obj).r1;
        ST16(vars + oAIAttackVars_StepTimer, 0xf);
        ST8(vars + oAIAttackVars_AttackElement, 4);
    }

    /* Timer reaches 13: fire the directional attack via jump table */
    if (LD16(vars + oAIAttackVars_StepTimer) == 0xd) {
        u8 tier    = LD8(vars + oAIAttackVars_AiDifficultyTier);
        u32 target = LD32(SP_ATTACK_TABLE + tier * 4);
        u32 front  = call_object_getFrontDirection_ai_beastSpAttackStep(obj).r0;
        u8  panelX = LD8(obj + oBattleObject_PanelX);
        u8  panelY = LD8(obj + oBattleObject_PanelY);
        u32 dmg    = LD32(vars + oAIAttackVars_Damage)
                   + LD16(vars + oAIAttackVars_AttackBoost);
        u32 boost2 = LD32(vars + oAIAttackVars_AttackBoost2);
        u8  initFl = LD8(vars + oAIAttackVars_InitFlag);

        r1 = sp_attack_fire(target, front + panelX, panelY, initFl,
                            boost2, obj, dmg);
    }

    /* Decrement step timer; if it wraps past zero, clear flag and set hit element */
    u32 timer = LD16(vars + oAIAttackVars_StepTimer);
    ST16(vars + oAIAttackVars_StepTimer, timer - 1);

    if ((s32)(timer - 1) < 0) {
        r1 = call_object_clearFlag_ai_beastSpAttackStep(0x40, obj).r1;
        ST16(vars + oAIAttackVars_HitElement, 4);
    }

    return r1;
}

/* ai_beastSpAttackStep(r5=obj, r7=vars, r10, lr) -> r1; preserves r4,r6,r7;
 * r5,r8-r12 pass through unchanged. */
Next ai_beastSpAttackStep(void) {
    R1 = beast_sp_attack_step(R5, R7, R10, R1);
    RETURN_TO_CALLER();
}

/* ── ai_attack_stepTick_80EDD5C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_sub_80EDD5C;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_sub_80EDD5C call_object_exitAttackState_sub_80EDD5C(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_sub_80EDD5C){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* AI attack step handler (r7 = AI attack vars block, r5/r6 = object pointers).
 *
 * If no attack element is queued yet, seed the step timer with 5 frames and
 * mark the attack as element 4. Then count the step timer down; when it runs
 * out, hand the object back to object_exitAttackState.
 *
 * r1 is only ever written by object_exitAttackState, so it passes through
 * unchanged on the frames where that call does not happen. */
static u32 ai_attack_step_tick(u32 r1, u32 r5, u32 r6, u32 r7) {
    /* Element 0 means "nothing chosen yet": start a fresh attack step. */
    if (LD8(r7 + oAIAttackVars_AttackElement) == 0) {
        ST16(r7 + oAIAttackVars_StepTimer, 5);
        ST8(r7 + oAIAttackVars_AttackElement, 4);
    }

    /* Tick the step timer down one frame. */
    u32 timer = LD16(r7 + oAIAttackVars_StepTimer) - 1;
    ST16(r7 + oAIAttackVars_StepTimer, timer & 0xFFFF);

    /* Timer still counting (result non-negative): nothing else to do. */
    if ((s32)timer >= 0)
        return r1;

    /* Step finished: leave the attack state and take its r1. */
    return call_object_exitAttackState_sub_80EDD5C(r5, r6, r7).r1;
}

/* ai_attack_stepTick_80EDD5C(r5, r6, r7, lr) -> r1 (r1 untouched unless the exit call runs) */
Next ai_attack_stepTick_80EDD5C(void) {
    R1 = ai_attack_step_tick(R1, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── ai_dispatchHitAttackByElement_80EEC70 ── */
/* ai_dispatchHitAttackByElement_80EEC70 — dispatch the AI's pending attack.
 *
 * The attack-state struct reached through r7 holds the element of the hit
 * that triggered this attack. That element selects an entry in a small jump
 * table at off_80EEC84:
 *   entry 0 -> aiAttack_windupTick_80EEC90
 *   entry 1 -> ai_frontPanelSlashAttack
 *   entry 2 -> ai_attackWaitThenCleanup
 * Note the original indexes the word table with the raw element byte
 * (`ldr r1, [r1, r0]`, no shift), so the offset is the element itself.
 *
 * The handler is entered with r0 = hit element and r1 = its own entry
 * address (the word just loaded from the table), exactly as the original's
 * `mov lr, pc; bx r1` sequence leaves them; r2, r5, r6, r7 and r10 reach it
 * untouched. Everything the handler leaves in r1, r5, r8-r12 is this
 * function's result — those registers simply stay as the call left them —
 * and the function then returns to its own caller. */

/* register form: the attack handler is reached through a code address read
 * from the jump table (the original's `mov lr, pc; bx r1`), so no typed
 * wrapper exists for it. */
static void call_hit_handler(u32 handler, u32 element) {
    R0 = element;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
}

static void run_hit_attack(u32 attackVars) {
    u32 element = LD8(attackVars + oAIAttackVars_HitElement);
    /* The table is indexed with the raw element byte, matching the
     * original's unshifted `ldr r1, [r1, r0]`. */
    u32 handler = LD32(A_off_80EEC84 + element);
    call_hit_handler(handler, element);
}

Next ai_dispatchHitAttackByElement_80EEC70(void) {
    run_hit_attack(R7);
    RETURN_TO_CALLER();
}

/* ── aiAttack_windupTick_80EEC90 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; } call_object_setAnimation_result;
/* register form: generated wrapper: object_setAnimation has no typed C interface yet */
static inline call_object_setAnimation_result call_object_setAnimation_sub_80EEC90(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setAnimation);
    return (call_object_setAnimation_result){ R1 };
}

typedef struct { u32 r3; bool v; } call_object_setDefaultCounterTime_result_sub_80EEC90;
/* register form: generated wrapper: object_setDefaultCounterTime has no typed C interface yet */
static inline call_object_setDefaultCounterTime_result_sub_80EEC90 call_object_setDefaultCounterTime_sub_80EEC90(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setDefaultCounterTime);
    return (call_object_setDefaultCounterTime_result_sub_80EEC90){ R3, OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_sub_80EEC90;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_sub_80EEC90 call_object_setFlag1_sub_80EEC90(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_sub_80EEC90){ R1, R2 };
}

/* Enemy AI attack tick (bn6f asm31.s:121661).
 *
 * r5 is the battle object, r7 the AI attack-variable block, r10 the toolkit
 * battle state (only read inside object_setDefaultCounterTime).
 *
 * While AttackElement is still 0 the attack is winding up: play the charge
 * animation, reset the counter to the default, mark the "charging" flag bit,
 * and set AttackElement to 4 (charged).  While charged, the step timer counts
 * down once per tick; when it expires the release animation (0x13) plays, the
 * timer reloads to 0x15, and the hit element becomes 4.
 *
 * r1 on exit is whatever the last called game function left there (the
 * original never sets it itself), so the typed function returns nothing and
 * the adapter leaves R1 untouched.
 */
static void ai_attack_windup_tick_sub_80EEC90(u32 obj, u32 vars) {
    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        /* Not charged yet: start the charge-up sequence. */
        call_object_setAnimation_sub_80EEC90(0x12, obj);
        call_object_setDefaultCounterTime_sub_80EEC90(obj);
        call_object_setFlag1_sub_80EEC90(1u << OBJECT_FLAGS_UNK_BIT_22_BIT, obj);
        ST16(vars + oAIAttackVars_StepTimer, 0xE);
        ST8(vars + oAIAttackVars_AttackElement, 4);
    } else {
        /* Charged: count the step timer down and fire when it runs out. */
        u32 timer = LD16(vars + oAIAttackVars_StepTimer);
        ST16(vars + oAIAttackVars_StepTimer, timer - 1);
        if ((s32)(timer - 1) >= 0)
            return;
        call_object_setAnimation_sub_80EEC90(0x13, obj);
        ST16(vars + oAIAttackVars_StepTimer, 0x15);
        ST16(vars + oAIAttackVars_HitElement, 4);
    }
}

/* aiAttack_windupTick_80EEC90(r5 = object, r7 = AI attack vars, r10 = battle state) -> r1 */
Next aiAttack_windupTick_80EEC90(void) {
    ai_attack_windup_tick_sub_80EEC90(R5, R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackWaitThenCleanup ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_ai_attackWaitThenCleanup;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_ai_attackWaitThenCleanup call_object_exitAttackState_ai_attackWaitThenCleanup(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_ai_attackWaitThenCleanup){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* AI attack state: count down the step timer, and once it runs out clear the
 * temp attack object references and hand the object back to its idle state.
 *
 * object      = battle object (r5, passed through to exitAttackState)
 * auxParam    = r6, passed through to exitAttackState untouched
 * attackVars  = per-object AI attack state block (r7)
 *
 * r1 out: on the cleanup path it is the AI data pointer left behind by the
 * callee chain; on the early path the original leaves it untouched, so the
 * adapter never writes it.
 */
static void attack_wait_then_cleanup(u32 object, u32 auxParam, u32 attackVars)
{
    /* First entry into this state: seed the timer and mark the element. */
    if (LD8(attackVars + oAIAttackVars_AttackElement) == 0) {
        ST16(attackVars + oAIAttackVars_StepTimer, 5);
        ST8(attackVars + oAIAttackVars_AttackElement, 4);
    }

    /* Tick the timer; a signed underflow (0 -> -1) means time is up. */
    s32 timer = LD16(attackVars + oAIAttackVars_StepTimer) - 1;
    ST16(attackVars + oAIAttackVars_StepTimer, timer);
    if (timer >= 0)
        return;

    /* Drop the reference to the attack object and exit the attack state. */
    ST32(object + oBattleObject_RelatedObject1Ptr, 0);
    u32 aiData = LD32(object + oBattleObject_AIDataPtr);
    ST32(aiData + oAIData_TempAttackObjectPtr, 0);
    call_object_exitAttackState_ai_attackWaitThenCleanup(object, auxParam, attackVars);
}

Next ai_attackWaitThenCleanup(void) {
    attack_wait_then_cleanup(R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackDispatchByHitElement_80EF534 ── */
/* battle_aiAttackDispatchByHitElement_80EF534: dispatch one AI attack step based on the element the attack hit
 * with. r7 points at the AI attack vars; the HitElement byte indexes a
 * two-entry handler table (SnakeArm attack-part step / plain attack tick-down),
 * and the selected handler is tail-called with r0 = element and r1 = handler
 * address (the original does mov lr,pc / bx r1, so the handler's return lands
 * back in this function's caller). Everything listed as output (r1, r4-r12) is
 * simply whatever the handler leaves behind. */

/* Helper for the indirect tail call: sets r0/r1 the way the original does,
 * then jumps to the handler address. Register form: the callee is reached
 * through a code address, not a known function name. */
static u32 call_attack_element_handler_sub_80EF534(u32 element, u32 handler)
{
    R0 = element;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Load the hit element from the AI attack vars and run the matching handler. */
static void run_ai_attack_element_step(u32 aiVars)
{
    u32 element = LD8(aiVars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80EF548 + element);

    call_attack_element_handler_sub_80EF534(element, handler);
}

/* battle_aiAttackDispatchByHitElement_80EF534(r5, r6, r7 = AI attack vars, r10, lr): tail-calls the element's
 * attack handler; no results of its own besides what the handler leaves. */
Next battle_aiAttackDispatchByHitElement_80EF534(void)
{
    run_ai_attack_element_step(R7);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackTickDown_80EF608 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_object_clearFlag_result;
/* register form: generated wrapper: object_clearFlag has no typed C interface yet */
static inline call_object_clearFlag_result call_object_clearFlag_battle_aiAttackTickDown_80EF608(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_clearFlag);
    return (call_object_clearFlag_result){ R2 };
}

typedef struct { u32 r0, r1, r2, r3; bool z, n, c, v; } call_object_exitAttackState_result_battle_aiAttackTickDown_80EF608;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_battle_aiAttackTickDown_80EF608 call_object_exitAttackState_battle_aiAttackTickDown_80EF608(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_battle_aiAttackTickDown_80EF608){ R0, R1, R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Tick down an in-progress AI attack (r7 = AI attack state, r5/r6 = battle
 * object pointers passed through to the object helpers).
 *
 * Each call decrements the attack's step timer; while it is still positive
 * there is nothing more to do this tick. Once it reaches zero, the charge
 * cycle counter is decremented too: as long as charge remains, the stored hit
 * element is reset, and on the tick where the charge runs out the object's
 * "elemental hit" flag bit is cleared and the object leaves its attack state.
 *
 * Returns the r1 value the original leaves behind: whatever
 * object_exitAttackState produced, or the incoming r1 on the early paths. */
static u32 ai_attack_tick_down(u32 r1_in, u32 object, u32 obj_state, u32 vars)
{
    u32 step_timer = LD16(vars + oAIAttackVars_StepTimer);
    step_timer -= 1;
    ST16(vars + oAIAttackVars_StepTimer, step_timer);
    if ((s32)step_timer > 0)
        return r1_in;                       /* still stepping through the attack */

    u32 charge = LD16(vars + oAIAttackVars_ChargeCycles);
    charge -= 1;
    ST16(vars + oAIAttackVars_ChargeCycles, charge);
    if (charge != 0) {
        /* Charge not spent yet: just forget the hit element for this cycle. */
        ST16(vars + oAIAttackVars_HitElement, 0);
        return r1_in;
    }

    /* Charge spent: clear the flag bit and return to the idle state. */
    call_object_clearFlag_battle_aiAttackTickDown_80EF608(1u << 22, object);
    call_object_exitAttackState_result_battle_aiAttackTickDown_80EF608 exit_state =
        call_object_exitAttackState_battle_aiAttackTickDown_80EF608(object, obj_state, vars);
    return exit_state.r1;
}

/* battle_aiAttackTickDown_80EF608
 * inputs: r5 r6 r7 lr   outputs: r1 r4 r6 r7   (r4/r6/r7 pass through) */
Next battle_aiAttackTickDown_80EF608(void) {
    R1 = ai_attack_tick_down(R1, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── nullsub_104 ── */
/* nullsub_104 does nothing; it exists only to fill a call slot in its caller. */
static void nullsub_104_noop(void) {
}

Next nullsub_104(void) {
    nullsub_104_noop();
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

/* ── ai_dispatchAttackStep_80F153C ── */
/* ai_dispatchAttackStep_80F153C -- AI attack dispatcher (bn6f asm31.s).
 *
 * Reads the routine selector from the AI attack-vars block pointed to by r7
 * and jumps to one of the two handlers in the table at off_80F1550.  The
 * handler is entered with r0 = selector byte, r1 = its own address and
 * r3/r5/r6/r7 passed through untouched; the values it leaves behind in
 * r1, r5 and r8-r12 are ai_dispatchAttackStep_80F153C's outputs, so they never pass through
 * C values here -- the guest call leaves them in the guest registers directly. */

/* register form: the call target comes from the dispatch table, and the
 * handler's outputs live in guest registers, not in C return values. */
static void run_attack_handler(u32 selector, u32 handler,
                               u32 passR3, u32 passR5, u32 passR6, u32 varsPtr) {
    /* The original pushes r7 before the call and pops it afterwards, so the
     * attack-vars pointer survives even if the handler clobbers r7. */
    u32 savedR7 = R7;

    R0 = selector;   /* handler's argument: the selector byte */
    R1 = handler;    /* as in the original, r1 holds the target address */
    R3 = passR3;
    R5 = passR5;
    R6 = passR6;
    R7 = varsPtr;

    GUEST_CALL_ADDR(handler);

    R7 = savedR7;
}

/* Typed body of ai_dispatchAttackStep_80F153C: pick the handler for the current AI attack
 * step and run it. */
static void dispatch_ai_attack_sub_80F153C(u32 varsPtr, u32 passR3, u32 passR5, u32 passR6) {
    u32 selector = LD8(varsPtr + oAIAttackVars_HitElement);
    u32 handler  = LD32(A_off_80F1550 + selector);   /* table is byte-indexed */
    run_attack_handler(selector, handler, passR3, passR5, passR6, varsPtr);
}

/* ai_dispatchAttackStep_80F153C(r3, r5, r6, r7 = attack vars) -> r1 r5 r8 r9 r10 r11 r12 */
Next ai_dispatchAttackStep_80F153C(void) {
    dispatch_ai_attack_sub_80F153C(R7, R3, R5, R6);
    RETURN_TO_CALLER();
}

/* ── ai_dispatchAttackStep_80F15CE ── */
/* AI attack dispatcher. The AI attack vars struct holds a selector byte; this
 * routine reads it, looks up one of seven attack handlers in the table at
 * off_80F15E0, and calls it with r0 = selector and r1 = handler address (the
 * original's "mov lr, pc / bx r1"). The handler returns straight back here,
 * and everything it leaves in r1 and r5, r8-r12 passes through to this
 * routine's caller; r7 (the attack vars pointer) is preserved. */

/* Calls an attack handler through its code address, mirroring the original's
 * "mov lr, pc; bx r1" dispatch.
 * Register form: the call goes through a computed code address, and the
 * handler's results must stay in the guest registers (r1, r5, r8-r12) so
 * they reach ai_dispatchAttackStep_80F15CE's caller untouched. */
static u32 call_attack_handler_sub_80F15CE(u32 handler, u32 attackIndex) {
    R0 = attackIndex;   /* selector byte, as the original passes it */
    R1 = handler;       /* the table entry is still live in r1 at the call */
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Read the attack selector from the AI attack vars and run the matching
 * handler. The handler leaves its results in r1/r5/r8-r12, which stay in the
 * guest registers for the caller of ai_dispatchAttackStep_80F15CE. */
static void run_ai_attack_sub_80F15CE(u32 attackVars) {
    u32 attackIndex = LD8(attackVars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80F15E0 + attackIndex);
    call_attack_handler_sub_80F15CE(handler, attackIndex);
}

/* ai_dispatchAttackStep_80F15CE(r5, r6, r7 = AI attack vars) -> r1 r5 r8 r9 r10 r11 r12
 * The outputs are exactly what the dispatched handler leaves behind, so the
 * adapter computes nothing itself. */
Next ai_dispatchAttackStep_80F15CE(void) {
    run_ai_attack_sub_80F15CE(R7);
    RETURN_TO_CALLER();
}

/* ── ai_lungeForwardThreePanels ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r4; bool z, n, c, v; } call_object_canMove_result_ai_lungeForwardThreePanels;
/* register form: generated wrapper: object_canMove has no typed C interface yet */
static inline call_object_canMove_result_ai_lungeForwardThreePanels call_object_canMove_ai_lungeForwardThreePanels(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_canMove);
    return (call_object_canMove_result_ai_lungeForwardThreePanels){ R1, R4, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_ai_lungeForwardThreePanels;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_ai_lungeForwardThreePanels call_object_getFrontDirection_ai_lungeForwardThreePanels(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_ai_lungeForwardThreePanels){ R0 };
}

typedef struct { u32 r0, r2, r3, r4, r5; bool c, v; } call_object_checkPanelParameters_result_ai_lungeForwardThreePanels;
/* register form: generated wrapper: object_checkPanelParameters has no typed C interface yet */
static inline call_object_checkPanelParameters_result_ai_lungeForwardThreePanels call_object_checkPanelParameters_ai_lungeForwardThreePanels(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_checkPanelParameters);
    return (call_object_checkPanelParameters_result_ai_lungeForwardThreePanels){ R0, R2, R3, R4, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2, r3; bool c, v; } call_object_reservePanel_result_ai_lungeForwardThreePanels;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_ai_lungeForwardThreePanels call_object_reservePanel_ai_lungeForwardThreePanels(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_ai_lungeForwardThreePanels){ R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline void call_object_clearCollisionRegion_ai_lungeForwardThreePanels(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_lungeForwardThreePanels;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_lungeForwardThreePanels call_object_setFlag1_ai_lungeForwardThreePanels(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_lungeForwardThreePanels){ R1, R2 };
}

/* AI lunge: attempt to move the battle object 3 panels forward.
 *
 * If the object can't move (object_canMove sets Z), or if the candidate panel
 * fails validation (checkPanelParameters returns 0), we record a "stall" status
 * in the AI vars and return.
 *
 * On success we record the target panel, reserve both the target and the
 * object's current panel, clear the collision region, and mark the object
 * as CANNOT_SLIDE so the engine won't try to reposition it again.
 *
 * Returns the final value of r1 (which comes from object_canMove on the
 * can't-move path, panelY on the invalid-panel path, or object_setFlag1
 * on the success path).  R5 is preserved unchanged. */
static u32 ai_lunge_forward(u32 obj, u32 ai_vars) {
    call_object_canMove_result_ai_lungeForwardThreePanels cm = call_object_canMove_ai_lungeForwardThreePanels(obj);
    if (cm.z) {
        /* Object cannot move this tick — record a no-action step. */
        ST16(ai_vars + oAIAttackVars_StepTimer, 2);
        ST16(ai_vars + oAIAttackVars_HitElement, 0xC);
        return cm.r1;
    }

    /* Candidate panel: X = currentX + frontDirection*3, Y unchanged. */
    u32 target_x = call_object_getFrontDirection_ai_lungeForwardThreePanels(obj).r0 * 3u
                 + (u32)LD8(obj + oBattleObject_PanelX);
    u32 target_y = (u32)LD8(obj + oBattleObject_PanelY);

    /* Validate the candidate against the stored panel parameters. */
    u32 param_lo = LD32(A_dword_80F168C);
    u32 param_hi = LD32(A_dword_80F1690);
    call_object_checkPanelParameters_result_ai_lungeForwardThreePanels check =
        call_object_checkPanelParameters_ai_lungeForwardThreePanels(target_x, target_y, param_lo, param_hi);

    if (check.r0 == 0) {
        /* Invalid panel — same stall status, but r1 holds the Y we saved
         * on the stack (the original's pop restores it after the call). */
        ST16(ai_vars + oAIAttackVars_StepTimer, 2);
        ST16(ai_vars + oAIAttackVars_HitElement, 0xC);
        return target_y;
    }

    /* Panel is valid: record the target, then reserve both target and current. */
    ST8(ai_vars + oAIAttackVars_TargetPanelX, target_x);
    ST8(ai_vars + oAIAttackVars_TargetPanelY, target_y);
    call_object_reservePanel_ai_lungeForwardThreePanels(target_x, target_y, obj);

    u32 cur_x = (u32)LD8(obj + oBattleObject_PanelX);
    u32 cur_y = (u32)LD8(obj + oBattleObject_PanelY);
    ST8(obj + oBattleObject_FuturePanelX, cur_x);
    ST8(obj + oBattleObject_FuturePanelY, cur_y);
    call_object_reservePanel_ai_lungeForwardThreePanels(cur_x, cur_y, obj);

    /* Drop the old collision footprint and lock the object in place. */
    call_object_clearCollisionRegion_ai_lungeForwardThreePanels(obj);
    call_object_setFlag1_result_ai_lungeForwardThreePanels sf =
        call_object_setFlag1_ai_lungeForwardThreePanels(OBJECT_FLAGS_CANNOT_SLIDE, obj);

    /* Success: 2-step animation with hit element 8; r1 comes from setFlag1. */
    ST16(ai_vars + oAIAttackVars_StepTimer, 2);
    ST16(ai_vars + oAIAttackVars_HitElement, 8);
    return sf.r1;
}

Next ai_lungeForwardThreePanels(void) {
    R1 = ai_lunge_forward(R5, R7);
    RETURN_TO_CALLER();
}

/* ── battleObject_aiDispatchAttack_80F17C4 ── */
/*
 * AI attack dispatch for a battle object.
 *
 * The object's "hit element" byte selects one of three attack handlers from
 * the table at off_80F17D8 via an unscaled word load (ldr r1, [table, element]),
 * so valid element bytes step through the table in 4-byte strides exactly as
 * the original does. The handler is then called (not tail-called: the original
 * returns here first and pops r7/pc). It is entered with
 *   r0 = hit element
 *   r1 = its own entry address (left over from the table load)
 *   r2/r5/r6/r7 = untouched from this dispatch's caller,
 * and whatever it leaves in r1/r5/r8-r12 is this function's output state.
 */

/* Call the attack handler at `handler` exactly as the original dispatch does:
 * r0 = hit element, r1 = the handler's own address; the other argument
 * registers (r2/r5/r6/r7) still hold the values this dispatch was entered
 * with. Returns the handler's r0. */
static u32 call_attack_handler(u32 self, u32 handler, u32 hitElement) { /* register form: calls through a guest code address (noted by the tool) */
    R0 = hitElement;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    R7 = self;                /* the original's pop {r7} restores the caller's r7 */
    return R0;
}

/* Look up and invoke the attack handler for the object's current hit element. */
static void dispatch_attack(u32 self) {
    u32 hitElement = LD8(self + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80F17D8 + hitElement);
    call_attack_handler(self, handler, hitElement);
}

Next battleObject_aiDispatchAttack_80F17C4(void) {
    dispatch_attack(R7);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackWindupStepWood_80F17E4 ── */
/* AI attack step handler (r5 = battle object, r7 = AI attack vars).
 *
 * First invocation (AttackElement == 0): initialize the attack — pick element
 * 4, play animation 0xF on the object, and load the step timer from the
 * attack's boost byte.
 * Later invocations: count the step timer down; once it reaches 0, write
 * element 4 into HitElement to mark the attack's hit phase. */
static void step_ai_attack(u32 obj, u32 atk) {
    if (LD8(atk + oAIAttackVars_AttackElement) == 0) {
        /* Wind-up not started yet: begin the attack. */
        ST8(atk + oAIAttackVars_AttackElement, 4);
        ST8(obj + oBattleObject_CurAnim, 0xF);
        ST16(atk + oAIAttackVars_StepTimer, LD8(atk + oAIAttackVars_AttackBoost2));
    } else {
        /* Tick the step timer; on expiry, the attack connects. */
        u32 timer = LD16(atk + oAIAttackVars_StepTimer);
        timer -= 1;
        ST16(atk + oAIAttackVars_StepTimer, timer);
        if ((s32)timer <= 0)
            ST16(atk + oAIAttackVars_HitElement, 4);
    }
}

/* battle_aiAttackWindupStepWood_80F17E4(r5 = battle object, r7 = AI attack vars) -> nothing */
Next battle_aiAttackWindupStepWood_80F17E4(void) {
    step_ai_attack(R5, R7);
    RETURN_TO_CALLER();
}

/* ── battleObject_attackVars_tickExitTimer_80F1888 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_battleObject_attackVars_tickExitTimer_80F1888;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_battleObject_attackVars_tickExitTimer_80F1888 call_object_exitAttackState_battleObject_attackVars_tickExitTimer_80F1888(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_battleObject_attackVars_tickExitTimer_80F1888){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Tick an AI attack's exit timer.
 *
 * r5 = battle object, r6 = object context passed through to exitAttackState,
 * r7 = AI attack vars. While an attack is winding down its exit element has
 * not been chosen yet (AttackElement == 0): choose element 4, switch the
 * object to anim 7 and load the step timer with the attack's cycle count.
 * Otherwise the step timer counts down each tick; when it expires the object
 * is handed back to object_exitAttackState.
 *
 * Returns the value the original leaves in r1: unchanged when
 * object_exitAttackState is not called, otherwise whatever that call leaves
 * in r1. Flags are clobbered either way and callers ignore them. */
static u32 battleObject_attackVars_tickExitTimer(u32 r1_out, u32 object, u32 ctx, u32 vars) {
    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        /* First tick of the exit: set up the exit element, anim and timer. */
        ST8(vars + oAIAttackVars_AttackElement, 4);
        ST8(object + oBattleObject_CurAnim, 7);
        ST16(vars + oAIAttackVars_StepTimer, LD8(vars + oAIAttackVars_TimerOrCycles));
        return r1_out;
    }

    /* Count the step timer down one tick and store the new value. */
    u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
    ST16(vars + oAIAttackVars_StepTimer, timer & 0xFFFF);

    /* bgt: still counting down, nothing else to do this tick. */
    if ((s32)timer > 0)
        return r1_out;

    /* Timer expired: let the object leave its attack state. */
    return call_object_exitAttackState_battleObject_attackVars_tickExitTimer_80F1888(object, ctx, vars).r1;
}

/* battleObject_attackVars_tickExitTimer_80F1888
 * (r5 = battle object, r6 = context, r7 = AI attack vars) -> r1 */
Next battleObject_attackVars_tickExitTimer_80F1888(void) {
    R1 = battleObject_attackVars_tickExitTimer(R1, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── sub_80F2330 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_updateSprite_result_sub_80F2330;
/* register form: generated wrapper: object_updateSprite has no typed C interface yet */
static inline call_object_updateSprite_result_sub_80F2330 call_object_updateSprite_sub_80F2330(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateSprite);
    return (call_object_updateSprite_result_sub_80F2330){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Battle object per-state update: dispatch to the handler selected by the
 * object's CurState byte, then run the shared sprite update on the object. */

/* Register values the CurState handler leaves behind that the rest of this
 * function (and its callers) go on to use. */
typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
} AfterStateHandler;

/* register form: the per-state handler is reached through the jump table at
 * off_80F2348 with the original's `mov lr, pc; bx r1` idiom, so no generated
 * wrapper covers it. The handler runs with the registers exactly as the
 * original leaves them at the bx: r0 = CurState, r1 = table entry (still live
 * in r1), r5 = battle object, r2-r4/r6-r12 = the caller's values. It may
 * clobber anything; the r5 it leaves feeds object_updateSprite and its
 * r8-r12 are this function's outputs, so they are read back here. */
static AfterStateHandler call_curState_handler(u32 state, u32 handler)
{
    AfterStateHandler left;

    R0 = state;   /* CurState byte, as the original passes it */
    R1 = handler; /* the table entry is still live in r1 at the bx */
    GUEST_CALL_ADDR(handler);

    left.r5 = R5;
    left.r8 = R8;
    left.r9 = R9;
    left.r10 = R10;
    left.r11 = R11;
    left.r12 = R12;
    return left;
}

/* Final register results of one object update. */
typedef struct {
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r10;
    u32 r11;
    u32 r12;
} BattleObjectUpdate;

static BattleObjectUpdate battle_object_update_sub_80F2330(u32 object)
{
    BattleObjectUpdate out;

    /* CurState indexes the handler table byte-wise, exactly as the
     * original's `ldrb r0` / `ldr r1, [r1, r0]` does. */
    u32 state = LD8(object + oBattleObject_CurState);
    u32 handler = LD32(A_off_80F2348 + state);

    AfterStateHandler left = call_curState_handler(state, handler);

    /* object_updateSprite runs on the object pointer the handler left in
     * r5; the wrapper leaves r10 as it currently is, i.e. as the handler
     * left it, matching the original. It preserves r8-r12, so the handler's
     * values pass straight through to this function's outputs. */
    call_object_updateSprite_result_sub_80F2330 sprite = call_object_updateSprite_sub_80F2330(left.r5);

    out.r5 = sprite.r5;
    out.r8 = left.r8;
    out.r9 = left.r9;
    out.r10 = left.r10;
    out.r11 = left.r11;
    out.r12 = left.r12;
    return out;
}

/* sub_80F2330(r4 r5 r6 r7 r8 r9 r10 r11 r12 lr in; r5 r8 r9 r10 r11 r12 out) */
Next sub_80F2330(void)
{
    BattleObjectUpdate out = battle_object_update_sub_80F2330(R5);
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R10 = out.r10;
    R11 = out.r11;
    R12 = out.r12;
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

/* ── sub_80F7732 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80F7732;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80F7732 call_sub_800FE28_sub_80F7732(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80F7732){ R0, R2 };
}

/* sub_80F7732: run sub_800FE28 on the fixed parameter block at dword_80F7740,
 * passing the caller's r5 through untouched, and hand back its result.
 * (The block's words 0x1212181E are configuration data consumed by the callee.) */
static u32 sub_80F7732_run(u32 param_r5) {
    return call_sub_800FE28_sub_80F7732(A_dword_80F7740, param_r5).r0;
}

Next sub_80F7732(void) {
    R0 = sub_80F7732_run(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80F77D4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80F77D4;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80F77D4 call_sub_800FE28_sub_80F77D4(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80F77D4){ R0, R2 };
}

/* sub_80F77D4: calls sub_800FE28 with the fixed pointer dword_80F77E4 (0x03030302)
 * and this function's r5, returning sub_800FE28's r0 to the caller. */
static u32 sub_80F77D4_impl(u32 arg5) {
    return call_sub_800FE28_sub_80F77D4(A_dword_80F77E4, arg5).r0;
}

Next sub_80F77D4(void) {
    R0 = sub_80F77D4_impl(R5);
    RETURN_TO_CALLER();
}

/* ── ai_attackStepDispatch_80F79F0 ── */
/* AI attack step dispatcher (0x080F79F0).
 *
 * r7 points to the AI attack vars; the byte at oAIAttackVars_HitElement selects
 * one of three handlers through the table at off_80F7A04. The original loads
 * r0 with the selector byte and r1 with the handler's table entry (its thumb
 * address), then jumps to it with lr = 0x080F79FC. The handler returns here,
 * and its results stay in the guest registers (r1, r5, r8-r12), which this
 * function's caller then reads. */

/* register form: dispatch through a handler address taken from the table.
 * The callee's outputs (r1, r5, r8-r12) live in the guest register file, so
 * no typed return value could carry them; the call must be made in register
 * form with the arguments placed directly in r0/r1. */
static void call_ai_handler(u32 selector, u32 handler) {
    R0 = selector;      /* handler's argument: the selected attack index */
    R1 = handler;       /* the original leaves the table entry in r1 across the call */
    GUEST_CALL_ADDR(handler);
}

/* Read the attack selector from the AI vars and run the matching handler. */
static void run_ai_attack_sub_80F79F0(u32 vars) {
    u32 selector = LD8(vars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80F7A04 + selector);
    call_ai_handler(selector, handler);
}

/* ai_attackStepDispatch_80F79F0(r7 = AI attack vars; r5/r6 pass through to the handler).
 * The handler's results are already in the guest registers when it returns,
 * so the adapter reads only the vars pointer and writes nothing itself. */
Next ai_attackStepDispatch_80F79F0(void) {
    run_ai_attack_sub_80F79F0(R7);
    RETURN_TO_CALLER();
}

/* ── ai_dispatchAttackState_80F7AD2 ── */
/* AI attack dispatcher (ai_dispatchAttackState_80F7AD2).
 *
 * r7 points at the AI attack state (oAIAttackVars). The byte at offset
 * oAIAttackVars_HitElement is used directly as a BYTE offset into the handler
 * table at off_80F7AE4 (the stored values are pre-multiplied by 4), selecting
 * one of six handlers. The handler is entered with:
 *   r0 = the selector byte
 *   r1 = the handler's own address
 * and every other register (r2, r3, r5, r6, r10, ...) exactly as this
 * trampoline received it.
 *
 * The original is a pure trampoline: it pushes {r7,lr}, calls the handler with
 * lr pointing at its own pop, and the pop restores r7 and returns straight to
 * this function's caller. So r7 must come back unchanged even though the
 * handler may clobber it, and r1/r5/r8-r12 pass through as the handler left
 * them (they are this function's outputs).
 */

/* Register form: the handler address comes from the dispatch table at run
 * time and no generated wrapper covers the six possible targets, so the call
 * has to go through the code address, like the original's `mov lr, pc; bx r1`. */
static void call_attack_handler_sub_80F7AD2(u32 selector, u32 handler)
{
    R0 = selector;   /* selector byte in r0 */
    R1 = handler;    /* handler's own address in r1 */
    GUEST_CALL_ADDR(handler);
}

static void dispatch_ai_attack_sub_80F7AD2(u32 vars)
{
    u32 selector = LD8(vars + oAIAttackVars_HitElement);
    u32 handler  = LD32(A_off_80F7AE4 + selector);  /* byte offset, not index */

    call_attack_handler_sub_80F7AD2(selector, handler);
}

Next ai_dispatchAttackState_80F7AD2(void)
{
    u32 vars = R7;

    dispatch_ai_attack_sub_80F7AD2(vars);

    R7 = vars;   /* pop {r7}: r7 is preserved across the dispatched handler */
    RETURN_TO_CALLER();
}

/* ── ai_chooseAttackPanel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r4; bool z, n, c, v; } call_object_canMove_result_ai_chooseAttackPanel;
/* register form: generated wrapper: object_canMove has no typed C interface yet */
static inline call_object_canMove_result_ai_chooseAttackPanel call_object_canMove_ai_chooseAttackPanel(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_canMove);
    return (call_object_canMove_result_ai_chooseAttackPanel){ R1, R4, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r3; bool c, v; } call_object_reservePanel_result_ai_chooseAttackPanel;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_ai_chooseAttackPanel call_object_reservePanel_ai_chooseAttackPanel(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_ai_chooseAttackPanel){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_chooseAttackPanel;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_chooseAttackPanel call_object_setFlag1_ai_chooseAttackPanel(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_chooseAttackPanel){ R1, R2 };
}

/* Per-alliance panel-selector table: two 8-byte entries at byte_80F7CE0.
 *   [r0] = selector argument 2
 *   [r0+4] = selector argument 3
 * Indexed by alliance * 8. */

/* register form: indirect call through a function pointer loaded from AI vars
 * (original: `mov lr, pc; bx r1`). The callback receives r0 = target object,
 * r1 = its own address, r2/r3 from the per-alliance table, and returns
 * r0 = chosen panel X (0 means "decline"), r1 = chosen panel Y. */
static void call_panel_selector(u32 addr, u32 target,
                                u32 r2_val, u32 r3_val,
                                u32 *out_x, u32 *out_y) {
    R0 = target;
    R1 = addr;
    R2 = r2_val;
    R3 = r3_val;
    GUEST_CALL_ADDR(addr);
    *out_x = R0;
    *out_y = R1;
}

/* AI attack-step handler for the current battle object (r5) and its AI
 * state block (r7).
 *
 * If the object can move, invoke the alliance-specific panel selector via
 * its function pointer; on success, stamp the chosen panel on the object,
 * reserve it, set CANNOT_SLIDE, and start the step animation (CurAnim = 4,
 * 6-frame step timer, HitElement = 4).
 *
 * On failure (object can't move, or selector declines) fall back to a
 * short wait: AttackDuration = 0x1E, 1-frame step timer, HitElement = 0x18. */
static void ai_choose_attack_panel(u32 r5, u32 r7) {
    /* Can this object move at all? */
    if (call_object_canMove_ai_chooseAttackPanel(r5).z) {
        /* No — 30-frame wait, one-step timer, hit element 0x18. */
        ST16(r7 + oAIAttackVars_AttackDuration, 0x1E);
        ST16(r7 + oAIAttackVars_StepTimer, 1);
        ST16(r7 + oAIAttackVars_HitElement, 0x18);
        return;
    }

    /* Pick the selector function and its two extra arguments from the
     * per-alliance table (8 bytes per entry: arg2 at +0, arg3 at +4). */
    u32 alliance  = LD8(r5 + oBattleObject_Alliance);
    u32 sel_entry = A_byte_80F7CE0 + (alliance << 3);
    u32 sel_arg2  = LD32(sel_entry);
    u32 sel_arg3  = LD32(sel_entry + 4);

    /* Invoke the selector: returns panel X (0 = decline) and panel Y. */
    u32 callback_addr = LD32(r7 + oAIAttackVars_PanelChooseCallback);
    u32 target_obj    = LD32(r7 + oAIAttackVars_TargetObjectPtr);
    u32 panel_x, panel_y;
    call_panel_selector(callback_addr, target_obj, sel_arg2, sel_arg3,
                        &panel_x, &panel_y);

    if (panel_x == 0) {
        /* Selector declined — same fallback as "cannot move". */
        ST16(r7 + oAIAttackVars_AttackDuration, 0x1E);
        ST16(r7 + oAIAttackVars_StepTimer, 1);
        ST16(r7 + oAIAttackVars_HitElement, 0x18);
        return;
    }

    /* Stamp the chosen panel and lock it against sliding. */
    ST8(r5 + oBattleObject_FuturePanelX, panel_x);
    ST8(r5 + oBattleObject_FuturePanelY, panel_y);
    call_object_reservePanel_ai_chooseAttackPanel(panel_x, panel_y, r5);
    call_object_setFlag1_ai_chooseAttackPanel(OBJECT_FLAGS_CANNOT_SLIDE, r5);

    /* Begin the step animation (anim 4) and advance the AI turn state. */
    ST8(r5 + oBattleObject_CurAnim, 4);
    ST16(r7 + oAIAttackVars_StepTimer, 6);
    ST8(r7 + oAIAttackVars_HasMoved, 1);
    ST16(r7 + oAIAttackVars_HitElement, 4);
}

/* 0x080F7C8C — inputs: r5, r7, lr; outputs: r1, r5, r8–r12.
 * r5 is unchanged; r8–r12 are untouched (callees preserve them);
 * r1 is left in place by whichever callee ran last
 * (object_canMove / the selector / object_setFlag1). */
Next ai_chooseAttackPanel(void) {
    ai_choose_attack_panel(R5, R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackDispatchStep_80F7EA6 ── */
/*
 * ai_attackDispatchStep_80F7EA6 -- AI attack dispatcher.
 *
 * Reads the selected attack slot from the AI variable block in r7
 * ([r7 + oAIAttackVars_HitElement]) and calls the matching handler from the
 * jump table at off_80F7EB8. The original tail-calls the handler
 * (mov lr,pc / bx r1), so the handler's register state, except r7, is
 * exactly what this function leaves behind for its caller: the only thing
 * that runs after the handler is the pop {r7,pc}.
 *
 * Note the table is indexed by the raw byte value -- the original adds it
 * to the table base as a plain byte offset, with no *4 scaling.
 */

/* register form: performs the one indirect call to the handler with
 * the register state the original sets up (r0 = attack slot, r1 = table
 * entry, r5/r6/r7/r10 passed through), then reloads r7 the way the
 * original's pop {r7,pc} does, so the handler may clobber it freely.
 * Every other register the handler leaves (r1, r5, r8..r12, flags) stays in
 * the guest registers and is ai_attackDispatchStep_80F7EA6's own result. */
static u32 call_attack_handler_sub_80F7EA6(u32 handler, u32 slot, u32 r5, u32 r6, u32 r7, u32 r10)
{
    R0 = slot;      /* the handlers take the selected attack slot in r0 */
    R1 = handler;   /* the original also leaves the table entry in r1 */
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R10 = r10;

    GUEST_CALL_ADDR(handler);

    R7 = r7;        /* pop {r7,pc}: r7 must survive the handler unchanged */
    return R0;
}

/* Pick the handler for the current attack slot and run it. The incoming lr
 * is only the return address consumed by the original's pop {r7,pc}; the
 * caller return is handled by the adapter, so it is not an argument here. */
static void dispatch_ai_attack_sub_80F7EA6(u32 r5, u32 r6, u32 r7, u32 r10)
{
    u32 slot = LD8(r7 + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80F7EB8 + slot);

    call_attack_handler_sub_80F7EA6(handler, slot, r5, r6, r7, r10);
}

/* ai_attackDispatchStep_80F7EA6(r5, r6, r7 = AI vars ptr, r10) -- results: r1 r5 r8 r9 r10 r11 r12
 * all come straight from the attack handler, so there is nothing to write here. */
Next ai_attackDispatchStep_80F7EA6(void)
{
    dispatch_ai_attack_sub_80F7EA6(R5, R6, R7, R10);
    RETURN_TO_CALLER();
}

/* ── battle_aiGetAttackIndex ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_battle_aiGetAttackIndex;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_battle_aiGetAttackIndex call_sub_800FE28_battle_aiGetAttackIndex(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_battle_aiGetAttackIndex){ R0, R2 };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag_battle_aiGetAttackIndex(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

/* battle_aiGetAttackIndex: pick the attack the AI wants to use.
 * sub_800FE28 looks up the base attack index from the AI config data
 * (dword_80F85D0 holds a pointer to it); if the battle object's flag word
 * has bits 15 or 13 set, the alternate attack (base index + 60) is used. */
static u32 ai_pick_attack(u32 config, u32 object) {
    u32 index = call_sub_800FE28_battle_aiGetAttackIndex(config, object).r0;
    u32 flags = call_object_getFlag_battle_aiGetAttackIndex(object);

    if (flags & 0xA000)
        index += 0x3C;
    return index;
}

/* battle_aiGetAttackIndex(r5 = battle object) -> r0 */
Next battle_aiGetAttackIndex(void) {
    R0 = ai_pick_attack(A_dword_80F85D0, R5);
    RETURN_TO_CALLER();
}

/* ── sub_80F85D4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80F85D4;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80F85D4 call_sub_800FE28_sub_80F85D4(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80F85D4){ R0, R2 };
}

/* sub_80F85D4: pass the flag table at dword_80F85E4 to sub_800FE28 and return its result.
 * r5 is forwarded untouched; r1/r2 and the flags are clobbered by the callee. */
static u32 sub_80F85D4_body(u32 r5) {
    return call_sub_800FE28_sub_80F85D4(A_dword_80F85E4, r5).r0;
}

Next sub_80F85D4(void) {
    R0 = sub_80F85D4_body(R5);
    RETURN_TO_CALLER();
}

/* ── battle_getSignedDistanceToOpposingBattler_80F8610 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r5; } call_battle_findPlayer_result_sub_80F8610;
/* register form: generated wrapper: battle_findPlayer has no typed C interface yet */
static inline call_battle_findPlayer_result_sub_80F8610 call_battle_findPlayer_sub_80F8610(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_findPlayer);
    return (call_battle_findPlayer_result_sub_80F8610){ R0, R5 };
}

typedef struct { u32 r0, r2, r3, r4, r5; } call_object_getClosestPanelMatchingRowFiltered_result_sub_80F8610;
/* register form: generated wrapper: object_getClosestPanelMatchingRowFiltered has no typed C interface yet */
static inline call_object_getClosestPanelMatchingRowFiltered_result_sub_80F8610 call_object_getClosestPanelMatchingRowFiltered_sub_80F8610(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_getClosestPanelMatchingRowFiltered);
    return (call_object_getClosestPanelMatchingRowFiltered_result_sub_80F8610){ R0, R2, R3, R4, R5 };
}

/* Per-side parameters for the closest-panel lookup (byte_80F864C), indexed
 * by alliance with an 8-byte stride: side 0 -> {0, 0x20},
 * side 1 -> {0x20, 0}. */

/* Signed horizontal distance between the battler fighting from the opposite
 * side of the field and the closest panel in that battler's row, searched
 * with this object's side's parameters. Returns 0 when no panel was found
 * (the original simply leaves the callee's 0 in r0). The sign of the result
 * depends on which side this object is on. */
static u32 distance_to_opposing_battler(u32 self) {
    /* Find the battler on the other side of the field. */
    u32 foe_alliance = LD8(self + oBattleObject_Alliance) ^ 1;
    u32 foe = call_battle_findPlayer_sub_80F8610(foe_alliance).r0;
    u32 foe_x = LD8(foe + oBattleObject_PanelX);
    u32 foe_y = LD8(foe + oBattleObject_PanelY);

    /* This object's own side, re-read after the lookup as the original does. */
    u32 alliance = LD8(self + oBattleObject_Alliance);
    u32 params = A_byte_80F864C + (alliance << 3);

    u32 found = call_object_getClosestPanelMatchingRowFiltered_sub_80F8610(
        alliance, foe_y, LD32(params), LD32(params + 4)).r0;

    if (found == 0)
        return 0;               /* no panel found: result is the callee's 0 */
    if (alliance == 0)
        return foe_x - found;   /* foe sits to the right of the panel */
    else
        return found - foe_x;   /* foe sits to the left of the panel */
}

/* battle_getSignedDistanceToOpposingBattler_80F8610(r5 = this BattleObject, r10 = battle state) -> r0, r5 (unchanged) */
Next battle_getSignedDistanceToOpposingBattler_80F8610(void) {
    u32 self = R5;
    R0 = distance_to_opposing_battler(self);
    R5 = self;                  /* output r5: the original leaves it unchanged */
    RETURN_TO_CALLER();
}

/* ── battle_aiLoadAttackVarsFromTables ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_battle_aiGetVersionedTableEntry_800FE12_result_battle_aiLoadAttackVarsFromTables;
/* register form: generated wrapper: battle_aiGetVersionedTableEntry_800FE12 has no typed C interface yet */
static inline call_battle_aiGetVersionedTableEntry_800FE12_result_battle_aiLoadAttackVarsFromTables call_battle_aiGetVersionedTableEntry_800FE12_battle_aiLoadAttackVarsFromTables(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battle_aiGetVersionedTableEntry_800FE12);
    return (call_battle_aiGetVersionedTableEntry_800FE12_result_battle_aiLoadAttackVarsFromTables){ R0, R2 };
}

/* Load the AI attack parameters for one battle object from the two
 * versioned data tables (low and high half of the damage word, entries
 * are 8 bytes apart) and store the combined value at offset 0xA0 in the
 * object's AI data block. */
static void load_attack_vars(u32 entryIndex, u32 battleObject) {
    u32 lowWord  = call_battle_aiGetVersionedTableEntry_800FE12_battle_aiLoadAttackVarsFromTables(
                       entryIndex * 8 + A_byte_80F86BC, battleObject).r0;
    u32 highWord = call_battle_aiGetVersionedTableEntry_800FE12_battle_aiLoadAttackVarsFromTables(
                       entryIndex * 8 + A_byte_80F86E0, battleObject).r0;
    u32 damage   = lowWord | (highWord << 16);

    u32 aiData = LD32(battleObject + oBattleObject_AIDataPtr);
    ST32(aiData + 0xA0 + oAIAttackVars_Damage, damage);
}

/* battle_aiLoadAttackVarsFromTables(r0 = table entry index, r5 = battle object) */
Next battle_aiLoadAttackVarsFromTables(void) {
    load_attack_vars(R0, R5);
    RETURN_TO_CALLER();
}

/* ── ai_attack_dispatchByHitElement_80F870C ── */
/* ai_attack_dispatchByHitElement_80F870C -- AI attack dispatcher.
 *
 * Reads the HitElement byte from the AI attack vars (r7) and calls one of
 * three element handlers through the table at off_80F8720:
 *   entry 0 -> sub_80F872C
 *   entry 1 -> ai_attackStepWindupAnim3Hit8
 *   entry 2 -> ai_tickAttackWindup_80F87DA
 * The raw HitElement byte is used directly as the BYTE offset into the
 * table (the game stores it already scaled to 0/4/8), and the handler is
 * entered with r0 = that same element value and r1 = its own table entry.
 * r5/r6 and the other live registers simply stay live for the handler;
 * whatever it leaves in r1 and r8-r12 is what this function leaves for its
 * caller, and r7 comes back as it was on entry (the original wraps the
 * call in push {r7,lr}/pop {r7,pc}).
 */

/* register form: indirect call through the element handler table; the
   table holds thumb addresses, and r7 must be restored after the handler
   returns just as the original's pop {r7,pc} does. */
static void call_element_handler_sub_80F870C(u32 handler, u32 element) {
    u32 saved_r7 = R7;
    R0 = element;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    R7 = saved_r7;
}

/* Look up the handler for the current hit element and run it. */
static void dispatch_element_attack(u32 attack_vars) {
    u32 element = LD8(attack_vars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80F8720 + element); /* byte offset, not word index */
    call_element_handler_sub_80F870C(handler, element);
}

/* ai_attack_dispatchByHitElement_80F870C(r5/r6 pass through, r7 = AI attack vars, lr = return address)
 * -> r1 r5 r8 r9 r10 r11 r12 as left by the handler, r7 preserved. */
Next ai_attack_dispatchByHitElement_80F870C(void) {
    dispatch_element_attack(R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackStepWindupAnim3Hit8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_object_removePanelReserve_result_ai_attackStepWindupAnim3Hit8;
/* register form: generated wrapper: object_removePanelReserve has no typed C interface yet */
static inline call_object_removePanelReserve_result_ai_attackStepWindupAnim3Hit8 call_object_removePanelReserve_ai_attackStepWindupAnim3Hit8(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_removePanelReserve);
    return (call_object_removePanelReserve_result_ai_attackStepWindupAnim3Hit8){ R3 };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_ai_attackStepWindupAnim3Hit8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r3, r4, r6; bool c, v; } call_object_updateCollisionPanels_result_ai_attackStepWindupAnim3Hit8;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_ai_attackStepWindupAnim3Hit8 call_object_updateCollisionPanels_ai_attackStepWindupAnim3Hit8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_ai_attackStepWindupAnim3Hit8){ R3, R4, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_clearFlag has no typed C interface yet */
static inline void call_object_clearFlag_ai_attackStepWindupAnim3Hit8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_clearFlag);
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_attackStepWindupAnim3Hit8;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_attackStepWindupAnim3Hit8 call_object_setFlag1_ai_attackStepWindupAnim3Hit8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_attackStepWindupAnim3Hit8){ R1, R2 };
}

/* AI attack step "windup -> anim 3 -> 8-frame hit".
 *
 * r5 = battle object, r7 = this attack's variable block.
 *
 * First entry (AttackElement == 0): switch the attack element to 4, play
 * animation 3, commit the planned future panel as the current panel, refresh
 * the panel reservation / coordinates / collision, clear the 0x40 flag, set
 * the "currently moving" flag and reload the step timer with 3.
 *
 * Later entries (AttackElement != 0): count the step timer down; while it is
 * still positive nothing else happens.  When it reaches zero (or wraps from
 * 0), store hit element 8 to end the wind-up.
 *
 * r1 on exit: on the first-entry path it is whatever object_setFlag1 left in
 * r1 (the last wrapper call leaves it there); on the countdown path the
 * original never touches r1.  The nz/c/v flags are clobbered freely. */
static void ai_attackStepWindupAnim3Hit8_run(u32 object, u32 vars) {
    u32 element = LD8(vars + oAIAttackVars_AttackElement);

    if (element != 0) {
        /* Winding up: tick the step timer, start the hit when it expires. */
        u32 timer = LD16(vars + oAIAttackVars_StepTimer);

        timer -= 1;
        ST16(vars + oAIAttackVars_StepTimer, timer);

        if ((s32)timer > 0)
            return;

        ST16(vars + oAIAttackVars_HitElement, 8);
        return;
    }

    /* First step of the attack: begin the wind-up animation and move onto
     * the panel that was planned for this attack. */
    ST8(vars + oAIAttackVars_AttackElement, 4);
    ST8(object + oBattleObject_CurAnim, 3);

    u32 futureX = LD8(object + oBattleObject_FuturePanelX);
    u32 futureY = LD8(object + oBattleObject_FuturePanelY);
    ST8(object + oBattleObject_PanelX, futureX);
    ST8(object + oBattleObject_PanelY, futureY);

    call_object_removePanelReserve_ai_attackStepWindupAnim3Hit8(futureX, futureY, object);
    call_object_setCoordinatesFromPanels_ai_attackStepWindupAnim3Hit8(object);
    call_object_updateCollisionPanels_ai_attackStepWindupAnim3Hit8(object);

    call_object_clearFlag_ai_attackStepWindupAnim3Hit8(0x40, object);
    call_object_setFlag1_ai_attackStepWindupAnim3Hit8(1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT, object);

    ST16(vars + oAIAttackVars_StepTimer, 3);
}

/* ai_attackStepWindupAnim3Hit8(r5 = battle object, r7 = attack vars) -> r1 */
Next ai_attackStepWindupAnim3Hit8(void) {
    ai_attackStepWindupAnim3Hit8_run(R5, R7);
    /* r1 needs no write here: on the wind-up path the object_setFlag1 wrapper
     * already left its r1 result in place, and on the countdown path the
     * original leaves r1 as it came in. */
    RETURN_TO_CALLER();
}

/* ── ai_tickAttackWindup_80F87DA ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_sub_80F87DA;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_sub_80F87DA call_object_exitAttackState_sub_80F87DA(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_sub_80F87DA){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* AI attack tick (ai_tickAttackWindup_80F87DA).
 *
 * r7 points at the AI's attack variables, r5 at the battle object, r6 is
 * passed through to the state-exit helper. When AttackElement is still
 * unset (zero) this is the first tick of the attack: lock in element 4,
 * switch the object to animation 0 and start the step timer from the
 * attack duration. Otherwise count the step timer down; once it expires
 * (or was already 0), hand control back via object_exitAttackState.
 *
 * r1 is untouched on the early paths (it is only set as a side effect of
 * the exit routine), so it is passed in and returned unchanged there. */
static u32 ai_attack_tick_sub_80F87DA(u32 r1, u32 object, u32 passthrough, u32 attack) {
    if (LD8(attack + oAIAttackVars_AttackElement) == 0) {
        /* First tick: choose the attack element and initialise the timer. */
        ST8(attack + oAIAttackVars_AttackElement, 4);
        ST8(object + oBattleObject_CurAnim, 0);
        ST16(attack + oAIAttackVars_StepTimer,
             LD16(attack + oAIAttackVars_AttackDuration));
        return r1;
    }

    /* Later ticks: decrement the step timer, staying in this state while
     * it is still positive (the "gt" test is signed, so 0 wraps past it). */
    u32 timer = LD16(attack + oAIAttackVars_StepTimer) - 1;
    ST16(attack + oAIAttackVars_StepTimer, timer);
    if ((s32)timer > 0)
        return r1;

    /* Timer expired: leave the attack state; r1 comes from the callee. */
    call_object_exitAttackState_result_sub_80F87DA exit =
        call_object_exitAttackState_sub_80F87DA(object, passthrough, attack);
    return exit.r1;
}

/* ai_tickAttackWindup_80F87DA(r5 = object, r6 = passthrough, r7 = AI attack vars) -> r1 */
Next ai_tickAttackWindup_80F87DA(void) {
    R1 = ai_attack_tick_sub_80F87DA(R1, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── ai_dispatchAttackStep_80F8996 ── */
/* AI attack-state dispatcher.
 *
 * The battle AI keeps its current attack state in a per-object variable block;
 * the byte at oAIAttackVars_HitElement selects one of six attack step handlers
 * from a jump table. This routine is a pure trampoline: it reads the selector,
 * loads the handler address, and tail-calls it with r0 = selector (and r1 =
 * the handler address, as the original leaves them). Everything callers see
 * afterwards — r1, r5, r8-r12 — is whatever the chosen handler leaves behind,
 * which the guest call below produces directly. */

/* Jump table indexed by the HitElement byte (one handler per element/state). */
#define AI_ATTACK_HANDLER_TABLE A_off_80F89A8

/* Call the selected attack step handler through its address.
 * Sets r0 = element and r1 = handler exactly as the original's `mov lr, pc;
 * bx r1` sequence does; the handler's register results (r1, r5, r8-r12) stay
 * in the guest register file for the caller. Returns the handler's r0. */
static u32 call_ai_attack_handler_sub_80F8996(u32 handler, u32 element) { /* register form: calls through a guest code address (noted by the tool) */
    R0 = element;
    R1 = handler;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Look up the handler for the current attack element and run it. */
static void dispatch_ai_attack_step(u32 attackVars) {
    u32 element = LD8(attackVars + oAIAttackVars_HitElement);
    u32 handler = LD32(AI_ATTACK_HANDLER_TABLE + element);
    call_ai_attack_handler_sub_80F8996(handler, element);
}

/* ai_dispatchAttackStep_80F8996(r5 r6 r7 r10 lr; r7 = attack variable block) */
Next ai_dispatchAttackStep_80F8996(void) {
    dispatch_ai_attack_step(R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackStepLand ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_object_removePanelReserve_result_ai_attackStepLand;
/* register form: generated wrapper: object_removePanelReserve has no typed C interface yet */
static inline call_object_removePanelReserve_result_ai_attackStepLand call_object_removePanelReserve_ai_attackStepLand(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_removePanelReserve);
    return (call_object_removePanelReserve_result_ai_attackStepLand){ R3 };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_ai_attackStepLand(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r1, r2, r3, r4, r6; bool c, v; } call_object_updateCollisionPanels_result_ai_attackStepLand;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_ai_attackStepLand call_object_updateCollisionPanels_ai_attackStepLand(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_ai_attackStepLand){ R1, R2, R3, R4, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* AI attack-step landing handler (0x080F8A1C).
 *
 * Runs once per frame while an NPC is stepping toward the panel it plans to
 * attack from.  Counts the step timer down; while it is still positive there
 * is nothing else to do (r1 passes through untouched).  When it expires the
 * object arrives: switch to the landing animation, move onto the planned
 * panel, refresh the panel reservation and collision data, then restart the
 * step timer and set the hit element for the landing hit.
 *
 * object: the stepping battle object (r5)
 * vars:   the object's AI attack variable block (r7)
 * r1_in:  incoming r1, returned unchanged while the timer is still running
 * returns: outgoing r1 (object_updateCollisionPanels' result once landed)
 */
static u32 attack_step_land(u32 object, u32 vars, u32 r1_in) {
    u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);

    if ((s32)timer > 0)                  /* still stepping: GT on timer - 1 */
        return r1_in;

    ST8(object + oBattleObject_CurAnim, 3);   /* landing animation */

    /* Arrive on the panel the AI planned to attack from. */
    u32 nextX = LD8(object + oBattleObject_FuturePanelX);
    u32 nextY = LD8(object + oBattleObject_FuturePanelY);
    ST8(object + oBattleObject_PanelX, nextX);
    ST8(object + oBattleObject_PanelY, nextY);

    call_object_removePanelReserve_ai_attackStepLand(nextX, nextY, object);
    call_object_setCoordinatesFromPanels_ai_attackStepLand(object);
    call_object_updateCollisionPanels_result_ai_attackStepLand collision =
        call_object_updateCollisionPanels_ai_attackStepLand(object);

    ST16(vars + oAIAttackVars_StepTimer, 3);   /* restart the step timer */
    ST16(vars + oAIAttackVars_HitElement, 8);  /* element of the landing hit */
    return collision.r1;
}

/* ai_attackStepLand(r5 = object, r7 = AI attack vars) -> r1 */
Next ai_attackStepLand(void) {
    R1 = attack_step_land(R5, R7, R1);
    RETURN_TO_CALLER();
}

/* ── ai_attackTimerStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_clearFlag has no typed C interface yet */
static inline void call_object_clearFlag_ai_attackTimerStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_clearFlag);
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_attackTimerStep;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_attackTimerStep call_object_setFlag1_ai_attackTimerStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_attackTimerStep){ R1, R2 };
}

/* Advance the AI's attack-step timer by one tick.
 *
 * r7 points to the AI attack vars, r5 to the owning battle object. When the
 * decremented timer is still positive the tick is done. When it runs out
 * (0 or underflowed), the attack sequence finishes: the 0x40 attack flag is
 * cleared, the "currently moving" flag is set on the object, and the hit
 * element is reset to 0xC.
 *
 * Returns r1 as the original leaves it: unchanged on the early path, and on
 * the reset path whatever object_setFlag1 left behind. */
static u32 ai_attack_timer_step(u32 object, u32 vars, u32 r1) {
    u32 timer = LD16(vars + oAIAttackVars_StepTimer);

    timer -= 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);

    if ((s32)timer > 0)
        return r1;

    /* Attack step finished: reset the attack state. */
    call_object_clearFlag_ai_attackTimerStep(0x40, object);
    call_object_setFlag1_result_ai_attackTimerStep flags =
        call_object_setFlag1_ai_attackTimerStep(1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT, object);
    ST16(vars + oAIAttackVars_HitElement, 0xC);

    return flags.r1;
}

/* ai_attackTimerStep(r5 = object, r7 = AI attack vars) -> r1 */
Next ai_attackTimerStep(void) {
    R1 = ai_attack_timer_step(R5, R7, R1);
    RETURN_TO_CALLER();
}

/* ── battle_aiChargeAttackTick_80F8A66 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; } call_sub_80F8EAA_result;
/* register form: generated wrapper: battle_highlightFrontPanel has no typed C interface yet */
static inline call_sub_80F8EAA_result call_sub_80F8EAA(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_highlightFrontPanel);
    return (call_sub_80F8EAA_result){ R1 };
}

/* AI attack charge handler (r5 = battle object, r7 = AI attack vars).
 *
 * While the attack element is unset, this starts the charge-up: picks the
 * attack element, plays the charge animation, and arms the step timer.
 * Once charging, it counts charge cycles (calling battle_highlightFrontPanel on three of
 * every four cycles) and counts down the step timer; when it runs out the
 * hit element is set and the attack fires. */
static u32 ai_attack_charge(u32 object, u32 vars, u32 r1) {
    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        /* First tick of the attack: set element, animation and timers. */
        ST8(vars + oAIAttackVars_AttackElement, 4);
        ST8(object + oBattleObject_CurAnim, 0xF);
        ST16(vars + oAIAttackVars_StepTimer, 0x18);
        ST16(vars + oAIAttackVars_ChargeCycles, 0);
        return r1;
    }

    /* Count this charge cycle; every fourth cycle (bit 2 set) skips the call. */
    u32 cycles = LD16(vars + oAIAttackVars_ChargeCycles) + 1;
    ST16(vars + oAIAttackVars_ChargeCycles, cycles);
    if ((cycles & 4) == 0)
        r1 = call_sub_80F8EAA(object).r1;

    /* Step the charge timer; when it expires, mark the hit element. */
    u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);
    if ((s32)timer <= 0)
        ST16(vars + oAIAttackVars_HitElement, 0x10);

    return r1;
}

/* battle_aiChargeAttackTick_80F8A66(r5 = battle object, r7 = AI attack vars) -> r1
 * r1 comes from battle_highlightFrontPanel when it runs, and is left as on entry otherwise. */
Next battle_aiChargeAttackTick_80F8A66(void) {
    R1 = ai_attack_charge(R5, R7, R1);
    RETURN_TO_CALLER();
}

/* ── ai_updateAttackHoldState ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_ai_updateAttackHoldState;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_ai_updateAttackHoldState call_object_exitAttackState_ai_updateAttackHoldState(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_ai_updateAttackHoldState){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Advance one frame of an AI attack hold.
 *
 * r7 points at the AI attack variables, r5 at the owning battle object, and
 * r6 is an opaque parameter passed through to object_exitAttackState.
 *
 * If no attack is in progress (AttackElement == 0), start one: mark the attack
 * as active, switch the object to its attack animation, and load the hold
 * timer with the attack's duration. Otherwise count the hold timer down; when
 * it reaches zero, hand the object back to object_exitAttackState.
 *
 * Returns the value the caller should see in r1: on the paths that return
 * early the original never touches r1, so it passes through unchanged; on the
 * path that calls object_exitAttackState, r1 is whatever that call left. */
static u32 ai_update_attack_hold_state(u32 r1, u32 obj, u32 r6, u32 vars) {
    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        /* Begin the attack hold: state 4, attack animation, timer = duration. */
        ST8(vars + oAIAttackVars_AttackElement, 4);
        ST8(obj + oBattleObject_CurAnim, 7);
        ST16(vars + oAIAttackVars_StepTimer,
             LD16(vars + oAIAttackVars_AttackDuration));
        return r1;
    }

    /* Hold in progress: tick the timer down (16-bit counter, 32-bit compare). */
    u32 timer = (u32)LD16(vars + oAIAttackVars_StepTimer) - 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);
    if ((s32)timer > 0)
        return r1;                        /* still holding */

    /* Timer expired: leave the attack state and propagate its r1. */
    return call_object_exitAttackState_ai_updateAttackHoldState(obj, r6, vars).r1;
}

/* ai_updateAttackHoldState(r5 = battle object, r6 = pass-through,
 *                          r7 = AI attack vars) -> r1 */
Next ai_updateAttackHoldState(void) {
    R1 = ai_update_attack_hold_state(R1, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── sub_80FD240 ── */
/* sub_80FD240: no-op trampoline; just returns to LR. */
Next sub_80FD240(void) {
    RETURN_TO_CALLER();
}

/* ── sub_80FD27A ── */
/* sub_80FD27A: return pointer to some object's "battle state" record, optionally
 * advanced past a 0x3C-byte header when a flag bit is set.
 *
 *   inputs:  r5 lr
 *   outputs: r0
 *   clobbers: nz c v r1 r2 lr
 *   preserved: r4
 */
Next sub_80FD27A(void) {
    u32 saved_r4 = R4;
    R0 = A_dword_80FD298;
    GUEST_CALL(sub_800FE28);
    u32 result = R0;
    GUEST_CALL(object_getFlag);
    if (R0 & 0xA000) {
        result += 0x3c;
    }
    R0 = result;
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_80FD29C ── */
/* sub_80FD29C: get a battle-related object pointer, offset by 0x3C if flag 0xA000 is set.
 *   inputs:  r5 lr
 *   outputs: r0
 *   clobbers: nz c v r1 r2 lr
 *   preserved: r4 */
Next sub_80FD29C(void) {
    R0 = A_dword_80FD2BC;
    GUEST_CALL(sub_800FE28);            /* returns a battle object pointer in R0 */
    u32 obj = R0;

    GUEST_CALL(object_getFlag);          /* fetch flags for the current object */
    SET_FLAGS_NZ(R0 & 0xA000);           /* TST R0, #0xA000 */
    if (R0 & 0xA000) {
        obj += 0x3C;
    }

    R0 = obj;
    RETURN_TO_CALLER();
}

/* ── sub_80FD2C0 ── */
/* sub_80FD2C0(r5 = current object) -> r0 = pointer into the object's handler
 * table. Base pointer comes from sub_800FE28(dword_80FD2E0); the standard
 * offset 0x3C is added when object_getFlag() has bits 0xA000 set. */
Next sub_80FD2C0(void) {
    u32 base;
    u32 flags;

    /* Fetch the handler-table base pointer for this object. */
    R0 = A_dword_80FD2E0;
    GUEST_CALL(sub_800FE28);
    base = R0;

    /* Read the object's flag bits and check the 0xA000 mask. */
    GUEST_CALL(object_getFlag);
    flags = R0;

    if (flags & 0xA000)
        base += 0x3C;

    R0 = base;
    RETURN_TO_CALLER();
}

/* ── sub_80FD2E4 ── */
/* sub_80FD2E4(r5 = opaque, lr = return address)
 * Trivial trampoline: load the game's static dword_80FD2F4 (0x5040403) into
 * r0 and tail-call sub_800FE28, returning its r0 to the caller. */
Next sub_80FD2E4(void) {
    R0 = A_dword_80FD2F4;
    GUEST_CALL(sub_800FE28);
    RETURN_TO_CALLER();
}

/* ── sub_80FD31C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r5; } call_battle_findPlayer_result;
/* register form: generated wrapper: battle_findPlayer has no typed C interface yet */
static inline call_battle_findPlayer_result call_battle_findPlayer(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_findPlayer);
    return (call_battle_findPlayer_result){ R0, R5 };
}

typedef struct { u32 r0; } call_object_getEnemyDirection_result;
/* register form: generated wrapper: object_getEnemyDirection has no typed C interface yet */
static inline call_object_getEnemyDirection_result call_object_getEnemyDirection(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getEnemyDirection);
    return (call_object_getEnemyDirection_result){ R0 };
}

/* Scan the BattleState's Unk_a0 slot array for a live 0xD2-type attack object
 * on the opposing player's panel row whose horizontal offset, multiplied by
 * the enemy's facing direction, equals 1 — i.e. the attack points at the player.
 *
 * player_obj: the local player's BattleObject* (entry r5)
 * toolkit:    the Toolkit*          (entry r10)
 * Returns 1 if such a threat is found, 0 otherwise. */
static u32 detect_enemy_attack(u32 player_obj, u32 toolkit) {
    /* battle_findPlayer returns the BattleObject of the opposing alliance. */
    u32 opp_alliance = LD8(player_obj + oBattleObject_Alliance) ^ 1u;
    u32 enemy_obj = call_battle_findPlayer(opp_alliance).r0;

    u32 enemy_panel_x = LD8(enemy_obj + oBattleObject_PanelX);
    u32 enemy_panel_y = LD8(enemy_obj + oBattleObject_PanelY);

    /* Base of the 7-slot (step 4, up to 0x1C) Unk_a0 array in BattleState. */
    u32 slot_base = LD32(toolkit + oToolkit_BattleStatePtr) + oBattleState_BattleObjectList;

    for (s32 idx = 0; idx <= 0x1C; idx += 4) {
        u32 obj = LD32(slot_base + idx);
        if (obj == 0)
            continue;

        /* Must be a 0xD2-type attack (halfword at offset 0x28). */
        if (LD16(obj + 0x28) != 0xD2)
            continue;

        /* Must be on the same panel row (byte 0x13) as the opposing player. */
        if (LD8(obj + 0x13) != enemy_panel_y)
            continue;

        /* Horizontal distance: enemy_x - obj_x (unsigned wrap matches ldrb/sub). */
        u32 dx = enemy_panel_x - (u32)LD8(obj + 0x12);

        /* Facing direction sign returned for the player's BattleObject. */
        u32 dir = call_object_getEnemyDirection(player_obj).r0;

        /* dx * dir == 1 ⇒ attack points directly at the enemy's position. */
        if (dx * dir == 1)
            return 1;
    }

    return 0;
}

/* sub_80FD31C(r5 = player BattleObject*, r10 = Toolkit*) -> r0 = threat flag; r5, r6 preserved */
Next sub_80FD31C(void) {
    R0 = detect_enemy_attack(R5, R10);
    RETURN_TO_CALLER();
}

/* ── sub_80FD3CC ── */
/* sub_80FD3CC(r0 = moveIndex, r5 = battleObject):
 * Looks up two versioned damage tables (base and bonus) for the chosen move
 * and packs them into the AI attack vars' Damage field (low 16 = base,
 * high 16 = bonus) of the battle object's AI data. */
Next sub_80FD3CC(void) {
    u32 moveIndex = R0;

    /* First table: base damage values. Compute pointer = tableBase + moveIndex*8,
     * then call with (R0 = pointer, R1 = table base, R2 = element size). */
    R0 = A_byte_80FD408 + moveIndex * 8;
    R1 = A_byte_80FD408;
    R2 = 8;
    GUEST_CALL(battle_aiGetVersionedTableEntry_800FE12);
    u32 damageBase = R0;

    /* Second table: bonus damage values, same layout, shifted into upper 16 bits. */
    R0 = A_byte_80FD42C + moveIndex * 8;
    R1 = A_byte_80FD42C;
    R2 = 8;
    GUEST_CALL(battle_aiGetVersionedTableEntry_800FE12);
    u32 packed = damageBase | (R0 << 16);

    /* Write to oAIAttackVars_Damage within the AI data block (offset 0xA0
     * within the AI data pointer from the battle object). */
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(aiData + 0xA0 + oAIAttackVars_Damage, packed);

    RETURN_TO_CALLER();
}

/* ── sub_80FD4C8 ── */
/* Enemy AI attack dispatcher. The AI state block holds an attack index in
 * its oAIAttackVars_HitElement byte; that index selects an entry of the jump
 * table at off_80FD4DC and this function calls the matching attack routine.
 * Everything the caller sees afterwards (r1, r5, r8-r12) is simply whatever
 * the dispatched handler left behind; r7 survives the call, matching the
 * original's push {r7} / pop {r7} pair. */

/* Call the selected attack routine by address.
 * register form: the dispatch goes through a jump table (ldr r1,[table,off];
 * bx r1), so no generated wrapper covers it. The callee must see exactly the
 * registers the original gives it: the attack index in r0 and the raw table
 * entry (the handler's own address) still in r1 at the moment of the call. */
static u32 call_attack_handler_sub_80FD4C8(u32 attack_index, u32 table_entry) {
    R0 = attack_index;
    R1 = table_entry;
    GUEST_CALL_ADDR(table_entry);
    return R0;
}

/* Read the attack index from the AI state block and run the handler it
 * selects. All results flow through from the handler untouched. */
static void run_ai_attack(u32 state) {
    u32 attack_index = LD8(state + oAIAttackVars_HitElement);
    u32 table_entry = LD32(A_off_80FD4DC + attack_index);
    call_attack_handler_sub_80FD4C8(attack_index, table_entry);
}

/* sub_80FD4C8(r7 = AI attack state block).
 * outputs r1 r5 r8 r9 r10 r11 r12 come from the dispatched handler and are
 * already in the registers when the handler returns, so the adapter writes
 * none of them. */
Next sub_80FD4C8(void) {
    run_ai_attack(R7);
    RETURN_TO_CALLER();
}

/* ── sub_80FD4E8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r4; bool z, n, c, v; } call_object_canMove_result_sub_80FD4E8;
/* register form: generated wrapper: object_canMove has no typed C interface yet */
static inline call_object_canMove_result_sub_80FD4E8 call_object_canMove_sub_80FD4E8(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_canMove);
    return (call_object_canMove_result_sub_80FD4E8){ R1, R4, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r3; bool c, v; } call_object_reservePanel_result_sub_80FD4E8;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_sub_80FD4E8 call_object_reservePanel_sub_80FD4E8(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_sub_80FD4E8){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_sub_80FD4E8;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_sub_80FD4E8 call_object_setFlag1_sub_80FD4E8(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_sub_80FD4E8){ R1, R2 };
}

/* Indirect call through a guest code address (mov lr, pc; bx rN).
 * Returns r0 and r1 after the callee completes. */
typedef struct { u32 r0; u32 r1; } indirect_call_result;

/* register form: indirect call through guest code address (bx rN pattern) */
static indirect_call_result call_indirect(u32 a0, u32 a1, u32 a2, u32 a3, u32 target) {
    R0 = a0; R1 = a1; R2 = a2; R3 = a3;
    GUEST_CALL_ADDR(target);
    return (indirect_call_result){ R0, R1 };
}

/* Write the "cannot reach destination" cooldown values into AI attack vars. */
static void set_fail_timers(u32 ai_vars) {
    ST16(ai_vars + oAIAttackVars_AttackDuration, 0x50);
    ST16(ai_vars + oAIAttackVars_StepTimer, 1);
    ST16(ai_vars + oAIAttackVars_HitElement, 8);
}

/* Write the "slide animation + success timer" values into object and AI vars. */
static void set_success_timers(u32 object, u32 ai_vars) {
    ST8(object + oBattleObject_CurAnim, 4);
    ST16(ai_vars + oAIAttackVars_StepTimer, 3);
    ST16(ai_vars + oAIAttackVars_HitElement, 4);
}

/* One AI attack-move step for a battle object.
 * If the object can move, look up its alliance's move-descriptor (two u32's
 * per alliance at byte_80FD538), indirect-call the alliance's target-search
 * routine, and on success commit the future panel + reserve + slide flag.
 * Returns the final value of r1 (the only computed output; other output
 * registers pass through unchanged). */
static u32 do_attack_move(u32 object, u32 ai_vars) {
    call_object_canMove_result_sub_80FD4E8 can_move = call_object_canMove_sub_80FD4E8(object);
    if (can_move.z) {
        set_fail_timers(ai_vars);
        return can_move.r1;
    }

    /* Fetch the alliance's 8-byte descriptor: two u32 arguments for the indirect call. */
    u32 alliance = LD8(object + oBattleObject_Alliance);
    u32 desc = A_byte_80FD538 + (alliance << 3);
    u32 a2 = LD32(desc);
    u32 a3 = LD32(desc + 4);

    /* Call the alliance's target-search routine at [ai_vars + oAIAttackVars_PanelChooseCallback]. */
    u32 a0 = LD32(ai_vars + oAIAttackVars_TargetObjectPtr);
    u32 target = LD32(ai_vars + oAIAttackVars_PanelChooseCallback);
    indirect_call_result move = call_indirect(a0, target, a2, a3, target);

    if (move.r0 == 0) {
        set_fail_timers(ai_vars);
        return move.r1;
    }

    /* Destination found – commit the future panel, reserve it, mark cannot-slide. */
    ST8(object + oBattleObject_FuturePanelX, move.r0);
    ST8(object + oBattleObject_FuturePanelY, move.r1);
    call_object_reservePanel_sub_80FD4E8(move.r0, move.r1, object);
    call_object_setFlag1_result_sub_80FD4E8 sf = call_object_setFlag1_sub_80FD4E8(OBJECT_FLAGS_CANNOT_SLIDE, object);

    set_success_timers(object, ai_vars);
    return sf.r1;
}

/* sub_80FD4E8(r5 = battle object, r7 = AI attack vars) -> r1 */
Next sub_80FD4E8(void) {
    R1 = do_attack_move(R5, R7);
    RETURN_TO_CALLER();
}

/* ── sub_80FD548 ── */
/* sub_80FD548: AI attack follow-through timer. Counts down the attack cooldown in
 * oAIAttackVars_StepTimer; once it expires, plays the recovery animation, commits the
 * object to its planned (future) panel position and switches the AI state machine
 * to idle-ish values (Unk_10 = 4, Unk_00 = 8). Leaves r1 as the panel-update helper
 * returned it (or untouched when returning early). */
Next sub_80FD548(void) {
    u32 timer = LD16(R7 + oAIAttackVars_StepTimer) - 1;
    ST16(R7 + oAIAttackVars_StepTimer, timer);

    if ((s32)timer > 0)
        RETURN_TO_CALLER(); /* still cooling down */

    /* Recovery: play anim 3 and move onto the panels decided during the attack. */
    ST8(R5 + oBattleObject_CurAnim, 3);
    u32 panelX = LD8(R5 + oBattleObject_FuturePanelX);
    u32 panelY = LD8(R5 + oBattleObject_FuturePanelY);
    ST8(R5 + oBattleObject_PanelX, panelX);
    ST8(R5 + oBattleObject_PanelY, panelY);

    R0 = panelX;
    R1 = panelY;
    GUEST_CALL(object_removePanelReserve);
    GUEST_CALL(object_setCoordinatesFromPanels);
    GUEST_CALL(object_updateCollisionPanels); /* r1 passes through untouched */

    ST16(R7 + oAIAttackVars_StepTimer, 4);
    ST16(R7 + oAIAttackVars_HitElement, 8);
    RETURN_TO_CALLER();
}

/* ── sub_80FD574 ── */
/* sub_80FD574(r5 = battle object, r7 = AI attack vars):
 * Ticks down the AI attack timer (oAIAttackVars_StepTimer) once per call.
 *
 * While the attack is still winding down (Unk_01 == 0): when the timer
 * reaches zero, reset the object's animation, clear flag 0x40, mark the
 * object as currently moving, reload the timer from Unk_18 and advance
 * the attack state to 4.
 *
 * Once in state 4 (Unk_01 != 0): when the timer reaches zero, hand
 * control back to the AI's normal state machine via object_exitAttackState.
 */
Next sub_80FD574(void) {
    u32 obj = R5;   /* battle object this AI state belongs to */
    u32 ai  = R7;   /* oAIAttackVars struct */

    if (LD8(ai + oAIAttackVars_AttackElement) == 0) {
        /* Attack winding down: count the timer down to zero. */
        u32 timer = (u32)LD16(ai + oAIAttackVars_StepTimer) - 1;
        ST16(ai + oAIAttackVars_StepTimer, timer);
        if ((s32)timer > 0)
            RETURN_TO_CALLER();

        /* Timer expired: restore the idle animation and movement. */
        ST8(obj + oBattleObject_CurAnim, 0);
        R0 = 0x40;
        GUEST_CALL(object_clearFlag);
        R0 = 1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
        GUEST_CALL(object_setFlag1);

        /* Restart the cooldown and switch to attack state 4. */
        ST16(ai + oAIAttackVars_StepTimer, LD16(ai + oAIAttackVars_AttackDuration));
        ST8(ai + oAIAttackVars_AttackElement, 4);
        SET_FLAGS_NZ(4); /* from the original's mov r0, #4 */
        RETURN_TO_CALLER();
    } else {
        /* Attack finished: count the timer down, then exit the attack. */
        u32 timer = (u32)LD16(ai + oAIAttackVars_StepTimer) - 1;
        ST16(ai + oAIAttackVars_StepTimer, timer);
        if ((s32)timer > 0)
            RETURN_TO_CALLER();

        GUEST_CALL(object_exitAttackState);
        RETURN_TO_CALLER();
    }
}

/* ── sub_80FD5AE ── */
/* AI attack command dispatcher.
 *
 * r7 points to an AI attack variable block; the byte at
 * oAIAttackVars_HitElement is a raw byte offset into the handler table at
 * off_80FD5C0. The selected handler is entered with the dispatcher's live
 * register state (r0 = command byte, r1 = handler address, the caller's
 * r3/r5/r6/r7/r10 untouched, SP shifted down by the push {r7,lr}) and
 * returns straight through the dispatcher's pop {r7,pc} to the dispatcher's
 * caller, so this behaves as a tail call that additionally restores r7.
 * Everything the caller sees afterwards (r1/r5/r8-r12, flags) is whatever
 * the handler left in the guest register file. */

/* register form: the handler is reached through a jump-table address and
 * must see the dispatcher's exact register state (r0/r1 set here, r3/r5/r6/
 * r7/r10 passed through, SP lowered by the dispatcher's push {r7,lr}); its
 * results stay in the guest register file (r1/r5/r8-r12), which typed C
 * values cannot carry out of the call. */
static void call_attack_handler_sub_80FD5AE(u32 handler, u32 cmd, u32 r3, u32 r5, u32 r6,
                                u32 r7, u32 r10, u32 ret_lr) {
    SP -= 8;                        /* push {r7,lr} */
    ST32(SP, r7);
    ST32(SP + 4, ret_lr);
    R0 = cmd;                       /* command byte, as at the original bx */
    R1 = handler;                   /* the original leaves the handler in r1 */
    R3 = r3;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R10 = r10;
    GUEST_CALL_ADDR(handler);
    SP += 8;                        /* pop {r7,pc} */
    R7 = r7;                        /* ...restores the incoming r7 */
}

/* Typed body: pick the handler for the command byte and run it. */
static void dispatch_ai_attack(u32 vars, u32 r3, u32 r5, u32 r6, u32 r10,
                               u32 ret_lr) {
    u32 cmd = LD8(vars + oAIAttackVars_HitElement);
    /* The table is indexed by the raw byte value, not by entry number. */
    u32 handler = LD32(A_off_80FD5C0 + cmd);
    call_attack_handler_sub_80FD5AE(handler, cmd, r3, r5, r6, vars, r10, ret_lr);
}

Next sub_80FD5AE(void) {
    dispatch_ai_attack(R7, R3, R5, R6, R10, LR);
    RETURN_TO_CALLER();
}

/* ── sub_80FD638 ── */
/* sub_80FD638(r5 = battle object, r7 = AI attack state):
 * Wind-up tick for an AI attack. Counts down oAIAttackVars_StepTimer; while it is
 * still positive there is nothing to do yet. When it expires, switch the object
 * to its attack animation, commit the reserved panels (FuturePanelX/Y ->
 * PanelX/Y), refresh the object's position and collision panels from them, and
 * reset the attack state (Unk_10 = 4 cooldown, Unk_00 = 8). */
Next sub_80FD638(void) {
    u32 countdown = LD16(R7 + oAIAttackVars_StepTimer) - 1;
    ST16(R7 + oAIAttackVars_StepTimer, countdown);
    if ((s32)countdown > 0)
        RETURN_TO_CALLER();

    ST8(R5 + oBattleObject_CurAnim, 3);

    u32 panelX = LD8(R5 + oBattleObject_FuturePanelX);
    u32 panelY = LD8(R5 + oBattleObject_FuturePanelY);
    ST8(R5 + oBattleObject_PanelX, panelX);
    ST8(R5 + oBattleObject_PanelY, panelY);

    R0 = panelX;
    R1 = panelY;
    GUEST_CALL(object_removePanelReserve);
    GUEST_CALL(object_setCoordinatesFromPanels);
    GUEST_CALL(object_updateCollisionPanels);

    ST16(R7 + oAIAttackVars_StepTimer, 4);
    ST16(R7 + oAIAttackVars_HitElement, 8);
    RETURN_TO_CALLER();
}

/* ── sub_80FD664 ── */
/* sub_80FD664(r7 = attack vars object):
 * Tick down the AI attack timer (oAIAttackVars_StepTimer). While it is still
 * nonzero after the decrement, do nothing more. When it reaches zero, clear
 * the 0x40 bit on the object, mark it as currently moving (flag 1<<19) and
 * reset the attack state timer oAIAttackVars_HitElement to 0xC. */
Next sub_80FD664(void) {
    u32 counter = LD16(R7 + oAIAttackVars_StepTimer);

    counter -= 1;
    ST16(R7 + oAIAttackVars_StepTimer, counter);

    /* bgt on the zero-extended halfword: only an exact 0 falls through. */
    if ((s32)counter > 0)
        RETURN_TO_CALLER();

    R0 = 0x40;
    GUEST_CALL(object_clearFlag);

    R0 = 1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
    GUEST_CALL(object_setFlag1);

    ST16(R7 + oAIAttackVars_HitElement, 0xC);
    RETURN_TO_CALLER();
}

/* ── sub_80FD682 ── */
/* sub_80FD682: step one AI attack object's cooldown timer.
 * r5 = battle object, r7 = its AI attack variables.
 * When the state byte is still zero, enter the waiting state: set state 4,
 * reset the object's current animation and arm a 30-frame countdown.
 * Otherwise count the timer down; once it expires (reaches 0 or wraps),
 * store 0x10 into the halfword at offset 0 to advance to the next state. */
Next sub_80FD682(void) {
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        ST8(R7 + oAIAttackVars_AttackElement, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST16(R7 + oAIAttackVars_StepTimer, 0x1E);
    } else {
        u32 timer = LD16(R7 + oAIAttackVars_StepTimer);
        timer -= 1;
        ST16(R7 + oAIAttackVars_StepTimer, timer);
        if ((s32)timer <= 0)
            ST16(R7 + oAIAttackVars_HitElement, 0x10);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80FD6A6 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; bool c, v; } call_object_setCounterTime_result;
/* register form: generated wrapper: object_setCounterTime has no typed C interface yet */
static inline call_object_setCounterTime_result call_object_setCounterTime(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCounterTime);
    return (call_object_setCounterTime_result){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_sub_80FD6A6;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_sub_80FD6A6 call_object_getFrontDirection_sub_80FD6A6(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_sub_80FD6A6){ R0 };
}

typedef struct { u32 r1, r5, r7; } call_sub_80C81A6_result;
/* register form: generated wrapper: sub_80C81A6 has no typed C interface yet */
static inline call_sub_80C81A6_result call_sub_80C81A6(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R6 = r6;
    GUEST_CALL(sub_80C81A6);
    return (call_sub_80C81A6_result){ R1, R5, R7 };
}

/* AI attack step machine for a battle object.
 * Path 1 (var01==0): enter windup – set animation 5, arm a 0x18-tick counter.
 * Path 2 (var10 > 0): count down the windup timer.
 * Path 3 (timer expired): fire at the panel directly in front of the object. */

/* Direction → attack-ID table (indexed by var03). */
/* A_byte_80FD6F0 holds {0xE, 0xC, 0xA, 0x8} */

typedef struct { u32 r1, r5; } sub_80FD6A6_result;

static sub_80FD6A6_result ai_attack_step(u32 r1, u32 r3, u32 r5, u32 r7) {
    /* --- First tick: begin windup --- */
    if (LD8(r7 + oAIAttackVars_AttackElement) == 0) {
        ST8(r7 + oAIAttackVars_AttackElement, 4);
        ST8(r5 + oBattleObject_CurAnim, 5);
        call_object_setCounterTime_result sc =
            call_object_setCounterTime(0x12, r5);
        ST16(r7 + oAIAttackVars_StepTimer, 0x18);
        return (sub_80FD6A6_result){ sc.r1, r5 };
    }

    /* --- Subsequent ticks: decrement windup counter --- */
    u32 remaining = LD16(r7 + oAIAttackVars_StepTimer) - 1;
    ST16(r7 + oAIAttackVars_StepTimer, remaining);
    if ((s32)remaining > 0)
        return (sub_80FD6A6_result){ r1, r5 };

    /* --- Timer expired: fire the attack forward --- */
    /* Build attack ID: lower byte = direction index, upper byte = table value */
    u32 dir_idx = LD8(r7 + oAIAttackVars_AiDifficultyTier);
    u32 attack_id = dir_idx | ((u32)LD8(A_byte_80FD6F0 + dir_idx) << 8);

    /* Get the panel offset directly in front of this object */
    call_object_getFrontDirection_result_sub_80FD6A6 front =
        call_object_getFrontDirection_sub_80FD6A6(r5);

    u32 target_x = front.r0 + LD8(r5 + oBattleObject_PanelX);
    u32 target_y = LD8(r5 + oBattleObject_PanelY);
    u32 atk_type = LD8(r7 + oAIAttackVars_InitFlag);
    u32 damage   = LD32(r7 + oAIAttackVars_Damage);

    /* Spawn the attack hit at the computed position */
    call_sub_80C81A6_result fired =
        call_sub_80C81A6(target_x, target_y, atk_type, r3, attack_id, damage);

    /* Transition to "recovery" state (var00 = 0x14) */
    ST16(fired.r7 + oAIAttackVars_HitElement, 0x14);

    return (sub_80FD6A6_result){ fired.r1, fired.r5 };
}

/* sub_80FD6A6(r1, r3, r5, r7) -> r1, r5  [preserves r4, r6] */
Next sub_80FD6A6(void) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    sub_80FD6A6_result res = ai_attack_step(R1, R3, R5, R7);
    R1 = res.r1;
    R5 = res.r5;
    R4 = saved_r4;
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── sub_80FD6F4 ── */
/* sub_80FD6F4(r5 = battle object ptr, r7 = attack vars ptr):
 * AI attack behaviour: on first call, snapshot a timer from oAIAttackVars_AttackDuration and start
 * counting; once the timer reaches zero, clear the current animation and exit the attack
 * state. Leaves R1 as the output (unchanged here, so callers' R1 flows through). */
Next sub_80FD6F4(void) {
    u8 state = LD8(R7 + oAIAttackVars_AttackElement);
    if (state == 0) {
        /* First invocation: latch the countdown. */
        ST8(R7 + oAIAttackVars_AttackElement, 4);
        ST16(R7 + oAIAttackVars_StepTimer, LD16(R7 + oAIAttackVars_AttackDuration));
    } else {
        /* Subsequent calls: tick the countdown. */
        u16 timer = LD16(R7 + oAIAttackVars_StepTimer) - 1;
        ST16(R7 + oAIAttackVars_StepTimer, timer);
        if (timer == 0) {
            ST8(R5 + oBattleObject_CurAnim, 0);
            R0 = 0;
            GUEST_CALL(object_exitAttackState);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_80FD718 ── */
/* sub_80FD718 — AI attack-script step dispatcher.
 *
 * r7 points to an AIAttackVars block; the byte at oAIAttackVars_HitElement holds
 * the current step, stored as a byte offset into the handler table at
 * off_80FD72C. The word at that offset is the thumb address of the handler
 * for this step. The handler is entered with the caller's registers intact
 * except r0 = step and r1 = its own table entry (the original's
 * "ldr r1,[r1,r0]; mov lr,pc; bx r1" sequence), and whatever it leaves in
 * r1/r5/r8-r12 is this function's result. Afterwards r7 is restored to the
 * block pointer, matching the original's push/pop {r7} around the call. */

/* register form: the handler address comes from the runtime table at
 * off_80FD72C, so this call has to go through the address (mov lr,pc; bx). */
static u32 call_attack_handler_sub_80FD718(u32 handler, u32 step, u32 table_entry,
                               u32 arg_r3, u32 arg_r5, u32 arg_r6, u32 arg_r10)
{
    R0 = step;          /* ldrb r0,[r7,#oAIAttackVars_HitElement] */
    R1 = table_entry;   /* ldr r1,[r1,r0] */
    R3 = arg_r3;
    R5 = arg_r5;
    R6 = arg_r6;
    R10 = arg_r10;
    GUEST_CALL_ADDR(handler);
    return R0;
}

static void dispatch_attack_step(u32 vars, u32 arg_r3, u32 arg_r5,
                                 u32 arg_r6, u32 arg_r10)
{
    u32 saved_vars = vars;                     /* push {r7} */
    u32 step = LD8(vars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80FD72C + step);  /* ldr r1,[r1,r0] */

    call_attack_handler_sub_80FD718(handler, step, handler, arg_r3, arg_r5, arg_r6, arg_r10);

    /* register form: the handler ran on the live guest register file and may
     * have clobbered r7; the original's pop {r7} puts the block pointer back,
     * which can only be expressed by writing the guest register itself. */
    R7 = saved_vars;
}

Next sub_80FD718(void) {
    dispatch_attack_step(R7, R3, R5, R6, R10);
    RETURN_TO_CALLER();
}

/* ── sub_80FD748 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4; bool z, n; } call_object_canMove_result_sub_80FD748;
/* register form: generated wrapper: object_canMove has no typed C interface yet */
static inline call_object_canMove_result_sub_80FD748 call_object_canMove_sub_80FD748(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_canMove);
    return (call_object_canMove_result_sub_80FD748){ R4, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3; bool c, v; } call_object_reservePanel_result_sub_80FD748;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_sub_80FD748 call_object_reservePanel_sub_80FD748(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_sub_80FD748){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_sub_80FD748;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_sub_80FD748 call_object_setFlag1_sub_80FD748(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_sub_80FD748){ R1, R2 };
}

typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result call_object_exitAttackState(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: indirect call through the target-finding function stored in AI vars */
static u32 call_ai_panel_target(u32 arg0, u32 func_addr, u32 arg2, u32 arg3,
                                u32 *out_panel_y) {
    R0 = arg0; R1 = func_addr; R2 = arg2; R3 = arg3;
    GUEST_CALL_ADDR(func_addr);
    *out_panel_y = R1;
    return R0;
}

/* AI attack panel selection for an enemy object.
 * Query movement ability; if able to move, call the alliance-indexed
 * target-finder to get a future panel, reserve it, lock sliding and
 * start the slide animation.  If movement is disallowed or the target
 * finder returns 0, clear the AI flag and exit the attack state. */
static void ai_attack_select_panel(u32 obj, u32 r6, u32 ai_vars) {
    call_object_canMove_result_sub_80FD748 can_move = call_object_canMove_sub_80FD748(obj);
    if (can_move.z) {
        ST8(ai_vars + oAIAttackVars_HasMoved, 0);
        call_object_exitAttackState(obj, r6, ai_vars);
        return;
    }

    /* Alliance byte selects an 8-byte table entry (two u32 args) */
    u32 alliance = LD8(obj + oBattleObject_Alliance);
    u32 tbl_base = A_byte_80FD798 + (alliance << 3);
    u32 arg2 = LD32(tbl_base);
    u32 arg3 = LD32(tbl_base + 4);
    u32 arg0 = LD32(ai_vars + oAIAttackVars_TargetObjectPtr);
    u32 target_fn = LD32(ai_vars + oAIAttackVars_PanelChooseCallback);

    u32 panel_y;
    u32 panel_x = call_ai_panel_target(arg0, target_fn, arg2, arg3, &panel_y);

    if (panel_x == 0) {
        ST8(ai_vars + oAIAttackVars_HasMoved, 0);
        call_object_exitAttackState(obj, r6, ai_vars);
        return;
    }

    /* Commit the chosen panel and lock the object into slide state */
    ST8(obj + oBattleObject_FuturePanelX, panel_x);
    ST8(obj + oBattleObject_FuturePanelY, panel_y);
    call_object_reservePanel_sub_80FD748(panel_x, panel_y, obj);
    call_object_setFlag1_sub_80FD748(OBJECT_FLAGS_CANNOT_SLIDE, obj);

    ST8(obj + oBattleObject_CurAnim, 4);                  /* slide animation */
    ST16(ai_vars + oAIAttackVars_StepTimer, 3);
    ST8(ai_vars + oAIAttackVars_HasMoved, 1);
    ST16(ai_vars + oAIAttackVars_HitElement, 4);
}

/* sub_80FD748(r5 = battle object, r6 = AI arg, r7 = AI attack vars)
 * -> r1 from last callee; r5/r8-r12 unchanged */
Next sub_80FD748(void) {
    ai_attack_select_panel(R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── sub_80FD7A8 ── */
/* sub_80FD7A8: tick an AI attack object's "approach" countdown.
 *   inputs:  r5 = battle object, r7 = AI attack vars
 *   outputs: r1 = (preserved -- see comment)
 *   clobbers: nz, c, v, r0, r2, r3, lr
 *
 * Decrements oAIAttackVars_StepTimer. While it stays strictly positive (signed),
 * the function is a no-op. When it reaches zero or goes negative the object
 * is snapped onto its pre-reserved FuturePanel, the panel reservation is
 * released, its coordinates and collision panels are refreshed, animation 3
 * is set, and the next phase is armed (Unk_10 = 4, Unk_00 = 8).
 *
 * R1 is the function's documented output register. On the early-return
 * path the original never writes R1, so output R1 == input R1. On the
 * body path the original also leaves R1 untouched after the last `bl`
 * (object_updateCollisionPanels), so output R1 == that callee's tail R1
 * in either case. Our version matches both because no synthetic write to
 * R1 is introduced. */
Next sub_80FD7A8(void) {
    u32 timer = LD16(R7 + oAIAttackVars_StepTimer);

    /* ldrh r0, [r7,#oAIAttackVars_StepTimer]; sub r0, #1; strh r0, [r7,...] */
    SET_FLAGS_CMP(timer, 1);
    timer -= 1;
    ST16(R7 + oAIAttackVars_StepTimer, timer);

    /* bgt locret_80FD7D2 -- skip the body while the decremented timer is
     * still strictly positive (BGT = N==V && Z==0). */
    if (NEGATIVE_FLAG() == OVERFLOW_FLAG() && !ZERO_FLAG()) {
        R0 = timer; /* match original r0 on return (SUBS result) */
        RETURN_TO_CALLER();
    }

    /* Countdown elapsed: commit the pre-reserved future panel. */
    /* mov r0, #3; strb r0, [r5,#oBattleObject_CurAnim] */
    R0 = 3;
    ST8(R5 + oBattleObject_CurAnim, R0);

    /* ldrb r0,[r5,#FPx]; ldrb r1,[r5,#FPy]; strb r0,[r5,#Px]; strb r1,[r5,#Py] */
    R0 = LD8(R5 + oBattleObject_FuturePanelX);
    R1 = LD8(R5 + oBattleObject_FuturePanelY);
    ST8(R5 + oBattleObject_PanelX, R0);
    ST8(R5 + oBattleObject_PanelY, R1);

    GUEST_CALL(object_removePanelReserve);
    GUEST_CALL(object_setCoordinatesFromPanels);
    GUEST_CALL(object_updateCollisionPanels);

    /* mov r0, #4; strh r0, [r7,#oAIAttackVars_StepTimer] */
    R0 = 4;
    ST16(R7 + oAIAttackVars_StepTimer, R0);

    /* mov r0, #8; strh r0, [r7,#oAIAttackVars_HitElement] */
    R0 = 8;
    ST16(R7 + oAIAttackVars_HitElement, R0);

    RETURN_TO_CALLER();
}

/* ── sub_80FD7D4 ── */
/* sub_80FD7D4: tick down an AI attack timer; when it reaches zero, clear the
 * 0x40 flag bit on the battle object, raise the "currently moving" flag, and
 * reset another AI attack variable to 0xC.
 *
 * Inputs:  r5, r7 (pointer to AI attack vars), lr
 * Outputs: r1
 * Clobbers: nz, c, v, r0, r2, lr
 */
Next sub_80FD7D4(void) {
    u16 counter = LD16(R7 + oAIAttackVars_StepTimer);
    counter--;
    ST16(R7 + oAIAttackVars_StepTimer, counter);
    SET_FLAGS_NZ(counter);

    if (!NEGATIVE_FLAG() && !ZERO_FLAG()) {
        /* Counter still positive: nothing more to do. */
        RETURN_TO_CALLER();
    }

    /* Counter hit zero (or wrapped): clear flag 0x40, raise the moving flag,
     * and seed Unk_00 with 0xC. */
    R0 = 0x40;
    GUEST_CALL(object_clearFlag);

    R0 = 1 << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
    GUEST_CALL(object_setFlag1);

    ST16(R7 + oAIAttackVars_HitElement, 0xC);
    SET_FLAGS_NZ(0xC);

    RETURN_TO_CALLER();
}

/* ── sub_80FD7F2 ── */
/* sub_80FD7F2 (r5 = battle object, r7 = AI attack vars):
 * Tick the AI's active attack.
 *   - First tick (state byte Unk_01 == 0): enter the attack state (Unk_01 = 4),
 *     reset the object's animation and start a 10-frame attack timer (Unk_10).
 *   - While the timer runs: on every 8th frame (timer bit 2 clear) call the
 *     attack handler with the attack's parameter byte (Unk_0c), then count the
 *     timer down.
 *   - When the timer expires, load the follow-up delay into Unk_00 (0x10). */
Next sub_80FD7F2(void) {
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        /* Attack just triggered: set up state, animation and timer. */
        ST8(R7 + oAIAttackVars_AttackElement, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST16(R7 + oAIAttackVars_StepTimer, 10);
    } else {
        u32 timer = LD16(R7 + oAIAttackVars_StepTimer);

        /* Fire the attack on frames where timer bit 2 is clear. */
        if (!(timer & 4)) {
            R0 = LD8(R7 + oAIAttackVars_AttackBoost2);
            GUEST_CALL(sub_80FDCA4);
        }

        timer--;
        ST16(R7 + oAIAttackVars_StepTimer, timer);
        if ((s32)timer <= 0)
            ST16(R7 + oAIAttackVars_HitElement, 0x10); /* timer ran out: set delay */
    }
    RETURN_TO_CALLER();
}

/* ── sub_80FD822 ── */
/* sub_80FD822(r5 = battle object, r7 = AI attack vars):
 * Per-frame tick of the AI's charge-up attack. The first time it runs (state
 * byte still zero) it switches the object into the charge animation and
 * starts a 30-frame countdown; on later frames it fires the attack on every
 * 8th frame (while bit 2 of the countdown is clear, the carry of "lsr #3")
 * and, once the countdown expires, stores 0x14 into Unk_00 so the AI goes
 * back to picking a new action. */
Next sub_80FD822(void) {
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        /* Begin the attack: enter the charging state, play the charge
         * animation and start a 30-frame timer. */
        ST8(R7 + oAIAttackVars_AttackElement, 4);
        ST8(R5 + oBattleObject_CurAnim, 7);
        SET_FLAGS_NZ(0x1E);
        ST16(R7 + oAIAttackVars_StepTimer, 0x1E);
        RETURN_TO_CALLER();
    }

    /* The attack is charging: fire it on frames where bit 2 of the
     * countdown is clear. */
    u32 timer = LD16(R7 + oAIAttackVars_StepTimer);
    if (!(timer & 4)) {
        R0 = LD8(R7 + oAIAttackVars_AttackBoost2);
        GUEST_CALL(sub_80FDCA4);
    }

    /* Count this frame down; while the timer is still positive we're done,
     * otherwise reset the attack state. */
    SET_FLAGS_CMP(timer, 1);
    ST16(R7 + oAIAttackVars_StepTimer, timer - 1);
    if (ZERO_FLAG() || NEGATIVE_FLAG() != OVERFLOW_FLAG()) {
        SET_FLAGS_NZ(0x14);
        ST16(R7 + oAIAttackVars_HitElement, 0x14);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80FD852 ── */
/* sub_80FD852 (r3 = ?, r5 = battle object, r7 = AI-attack vars, r10 = ?, lr = return):
 *   Tick the AI attack's wind-up/impact animation.
 *   On the first call (when Unk_01 == 0) it kicks off the attack: starts
 *   a counter window, sets the caster animation, spawns the actual hit
 *   via sub_80C82A8, plays the hit SFX, and stashes a frame counter.
 *   On every later call it just counts that frame counter down; when it
 *   reaches zero the wind-up finishes and the attack advances to its
 *   next state.
 *   inputs:  r3 r5 r7 r10 lr
 *   outputs: r1 r5 r10
 *   clobbers: nz c v r0 r2 r3 lr
 *   preserved: r4 r5 r6 r7 r10
 */
Next sub_80FD852(void) {
    /* r4 and r6 get clobbered in the first-tick path; original saves/restores
     * them via push {r4,r6,lr} / pop {r4,r6,pc}, so we record and restore. */
    u32 saved_R4 = R4;
    u32 saved_R6 = R6;

    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        /* First tick — set up the attack. */
        ST8(R7 + oAIAttackVars_AttackElement, 4);

        /* Counter window for the attack. */
        R0 = 0x18;
        GUEST_CALL(object_setCounterTime);

        /* Trigger the caster's attack animation. */
        ST8(R5 + oBattleObject_CurAnim, 9);

        /* Build the packed descriptor (r4) that sub_80C82A8 consumes:
         *   element-id | (sub-element << 8) | (direction << 16). */
        R4  = LD8(R7 + oAIAttackVars_AiDifficultyTier);
        R4 |= LD8(A_dword_80FD8AC + LD8(R7 + oAIAttackVars_AiDifficultyTier)) << 8;
        R4 |= LD8(R7 + oAIAttackVars_AttackBoost2)            << 16;

        /* Damage value goes in r6 for the callee. */
        R6 = LD32(R7 + oAIAttackVars_Damage);

        /* Spawn the hit/projectile. */
        R0 = LD8(R5 + oBattleObject_PanelX);
        R1 = LD8(R5 + oBattleObject_PanelY);
        R2 = LD8(R7 + oAIAttackVars_InitFlag);
        GUEST_CALL(sub_80C82A8);

        /* Hit SFX. */
        R0 = 0x17E;   /* 0x7F + 0xFF */
        GUEST_CALL(PlaySoundEffect);

        /* Stash sub-element index as the frame counter ticked by the
         * else-branch on subsequent calls. */
        ST16(R7 + oAIAttackVars_StepTimer,
             LD8(A_dword_80FD8AC + LD8(R7 + oAIAttackVars_AiDifficultyTier)));

        /* Asm leaves r1 = &dword_80FD8AC at exit. */
        R1 = A_dword_80FD8AC;
    } else {
        /* Wind-up animation in progress — count down its frames. */
        u32 timer = LD16(R7 + oAIAttackVars_StepTimer) - 1;
        ST16(R7 + oAIAttackVars_StepTimer, (u16)timer);
        if (!((s32)timer > 0)) {
            /* Timer reached zero: hand off to the next attack state. */
            ST16(R7 + oAIAttackVars_HitElement, 0x18);
        }
    }

    /* Restore callee-preserved registers. */
    R4 = saved_R4;
    R6 = saved_R6;

    RETURN_TO_CALLER();
}

/* ── sub_80FD8B0 ── */
/* sub_80FD8B0(r5 = battle object, r6 = ?, r7 = AI attack vars):
 *  If the AI-attack init flag is 0, set it to 4, reset the object's
 *  current animation to 0, and copy the timer from Unk_18 into Unk_10
 *  (i.e. arm a timed attack sub-state). Otherwise count Unk_10 down
 *  each call; when it reaches 0, exit the attack state.
 *  Returns in r1 (untouched by this function). */
Next sub_80FD8B0(void) {
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        ST8(R7 + oAIAttackVars_AttackElement, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST16(R7 + oAIAttackVars_StepTimer, LD16(R7 + oAIAttackVars_AttackDuration));
    } else {
        u32 timer = LD16(R7 + oAIAttackVars_StepTimer) - 1;
        ST16(R7 + oAIAttackVars_StepTimer, timer);
        if ((s32)timer <= 0) {
            R0 = timer;  /* preserve r0 for callee */
            GUEST_CALL(object_exitAttackState);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_80FDA62 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline void call_object_reservePanel(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
}

typedef struct { u32 r0, r4, r5; } call_object_getPanelsIgnoreColumnFiltered_result;
/* register form: generated wrapper: object_getPanelsIgnoreColumnFiltered has no typed C interface yet */
static inline call_object_getPanelsIgnoreColumnFiltered_result call_object_getPanelsIgnoreColumnFiltered(u32 r0, u32 r2, u32 r3, u32 r7) {
    R0 = r0;
    R2 = r2;
    R3 = r3;
    R7 = r7;
    GUEST_CALL(object_getPanelsIgnoreColumnFiltered);
    return (call_object_getPanelsIgnoreColumnFiltered_result){ R0, R4, R5 };
}

typedef struct { u32 r0, r1, r4, r5, r7; } call_object_getPanelsInRowFiltered_result;
/* register form: generated wrapper: object_getPanelsInRowFiltered has no typed C interface yet */
static inline call_object_getPanelsInRowFiltered_result call_object_getPanelsInRowFiltered(u32 r0, u32 r2, u32 r3, u32 r7) {
    R0 = r0;
    R2 = r2;
    R3 = r3;
    R7 = r7;
    GUEST_CALL(object_getPanelsInRowFiltered);
    return (call_object_getPanelsInRowFiltered_result){ R0, R1, R4, R5, R7 };
}

typedef struct { u32 r0, r2, r3, r7; } call_GetPositiveSignedRNG_result;
/* register form: generated wrapper: GetPositiveSignedRNG has no typed C interface yet */
static inline call_GetPositiveSignedRNG_result call_GetPositiveSignedRNG(void) {
    GUEST_CALL(GetPositiveSignedRNG);
    return (call_GetPositiveSignedRNG_result){ R0, R2, R3, R7 };
}

typedef struct { u32 r2, r3; bool z, n, c, v; } call_object_removePanelReserve_result;
/* register form: generated wrapper: object_removePanelReserve has no typed C interface yet */
static inline call_object_removePanelReserve_result call_object_removePanelReserve_sub_80FDA62(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_removePanelReserve);
    return (call_object_removePanelReserve_result){ R2, R3, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* sub_80FDA62 - pick a random panel for a panel-targeting AI action.
 *
 * The actor (r5) reserves its panel slot, then the candidate panels matching
 * the requested filters are gathered into a list on the stack: first with the
 * "ignore column" filter, falling back to the "same row" filter when that
 * list comes back empty.  One entry is chosen with the BIOS random number
 * (svc 6 Div remainder), its byte is decoded as column = high nibble and
 * row = low 3 bits, and the reservation is released.  With no candidate the
 * result is row 0 and r1 whatever the row-filter call left behind.
 *
 * Inputs:  r0 = target object, r2/r3 = filter parameters, r5 = actor object,
 *          lr = return address.  r4-r7 are preserved (r6/r7 via push/pop).
 * Outputs: r0 = row, r1 = column, r5/r6/r7 as on entry. */

typedef struct {
    u32 row;      /* r0: low 3 bits of the chosen panel byte */
    u32 column;   /* r1: high nibble of the chosen panel byte */
    u32 actor;    /* r5, passed through unchanged */
    u32 savedR6;  /* r6, restored (the original pops it) */
    u32 savedR7;  /* r7, restored (the original pops it) */
} PanelPick;

/* BIOS Div (swi 6): quotient in r0, remainder in r1. */
/* register form: swi 6 returns its remainder in r1, which no wrapper-style
 * single return value can carry, so this helper reads R1 after the call. */
static u32 bios_div_remainder(u32 dividend, u32 divisor) {
    R0 = dividend;
    R1 = divisor;
    swi_call(6);
    return R1;
}

static PanelPick pick_random_panel(u32 object, u32 filterA, u32 filterB,
                                   u32 actor, u32 entryR6, u32 entryR7,
                                   u32 entryLR) {
    PanelPick result;

    /* register form: the panel list is filled by the callees through its
     * guest address, so the original's exact stack frame is reproduced. */
    SP -= 12;                            /* push {r6,r7,lr} */
    ST32(SP, entryR6);
    ST32(SP + 4, entryR7);
    ST32(SP + 8, entryLR);
    SP -= 0x24;                          /* three spills + 0x18-byte panel list */
    ST32(SP, object);
    ST32(SP + 4, filterA);
    ST32(SP + 8, filterB);
    u32 panelList = SP + 0xC;

    /* Hold the actor's panel reservation while searching. */
    call_object_reservePanel(LD8(actor + 0x12), LD8(actor + 0x13), actor);

    /* Candidate panels: ignore the object's column first, else use its row. */
    u32 count = call_object_getPanelsIgnoreColumnFiltered(
        LD8(object + 0x13), filterA, filterB, panelList).r0;
    u32 emptyRowListR1 = 0;
    if (count == 0) {
        call_object_getPanelsInRowFiltered_result rowList =
            call_object_getPanelsInRowFiltered(
                LD8(object + 0x13), filterA, filterB, panelList);
        count = rowList.r0;
        emptyRowListR1 = rowList.r1;     /* the r1 the original leaves behind */
    }

    if (count != 0) {
        u32 panel = LD8(panelList +
                        bios_div_remainder(call_GetPositiveSignedRNG().r0, count));
        result.column = panel >> 4;          /* high nibble */
        result.row = (panel << 29) >> 29;    /* low 3 bits */
    } else {
        result.row = 0;
        result.column = emptyRowListR1;
    }

    /* Release the reservation; the original saves r0/r1 across this call. */
    SP -= 8;
    ST32(SP, result.row);
    ST32(SP + 4, result.column);
    call_object_removePanelReserve_sub_80FDA62(LD8(actor + 0x12), LD8(actor + 0x13), actor);
    SP += 8;

    SP += 0x24;                          /* drop the local frame ... */
    SP += 12;                            /* ... and pop {r6,r7,pc} */

    result.actor = actor;
    result.savedR6 = entryR6;
    result.savedR7 = entryR7;
    return result;
}

Next sub_80FDA62(void) {
    PanelPick result = pick_random_panel(R0, R2, R3, R5, R6, R7, LR);
    R0 = result.row;
    R1 = result.column;
    R5 = result.actor;
    R6 = result.savedR6;
    R7 = result.savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_80FDAC4 ── */
/* sub_80FDAC4: pick a target panel for an object based on its column preferences.
 * inputs:  r0 (struct ptr), r2 (param2), r3 (param3), r5 (object), lr
 * outputs: r0 (panelX), r1 (panelY), r5, r6, r7 (must equal inputs)
 * clobbers: nz, c, v, r2, r3, lr */
Next sub_80FDAC4(void) {
    u32 param0 = R0, param2 = R2, param3 = R3;
    u32 in_r7 = R7;
    u32 buf = SP - 0x24;
    SP = buf;
    ST32(buf + 0x00, R0);
    ST32(buf + 0x04, R2);
    ST32(buf + 0x08, R3);

    /* Reserve the object's current column so movement routines see it busy. */
    R0 = (u32)LD8(R5 + oBattleObject_PanelX);
    R1 = (u32)LD8(R5 + oBattleObject_PanelY);
    GUEST_CALL(object_reservePanel);

    /* First pass: pack (row<<4 | col) for each preferred column that
     * checkPanelParameters accepts. */
    u32 r4 = 0, r6 = 0;
    u32 out_x = 0, out_y = 0;
    bool have_panel = false;
    for (r4 = 0; r4 < 2; r4++) {
        u32 alliance = LD8(R5 + oBattleObject_Alliance);
        u32 col = LD8((u32)A_byte_80FDB80 + alliance * 2 + r4);
        u32 row_byte = LD8(param0 + 0x13);
        R0 = col;
        R1 = row_byte;
        R2 = param2;
        R3 = param3;
        GUEST_CALL(object_checkPanelParameters);
        if (R0 != 0) {
            ST8(buf + 0x0C + r6, (u8)((row_byte << 4) | (col & 0xF)));
            r6++;
        }
    }

    if (r6 == 0) {
        u32 self_x = LD8(R5 + oBattleObject_PanelX);
        u32 self_y = LD8(R5 + oBattleObject_PanelY);
        u32 target_row = LD8(param0 + 0x13);

        /* If we're on the requested row already, see whether our X is one of
         * the preferred columns; if so, "use self" ends the search. */
        if (self_y == target_row) {
            for (r4 = 0; r4 < 2; r4++) {
                u32 alliance = LD8(R5 + oBattleObject_Alliance);
                u32 c = LD8((u32)A_byte_80FDB80 + alliance * 2 + r4);
                if (self_x == c) {
                    out_x = self_x;
                    out_y = self_y;
                    have_panel = true;
                    goto use_panel;
                }
            }
        }

        /* Otherwise ask the column which panel rows it allows. */
        r6 = 0;
        for (r4 = 0; r4 < 2; r4++) {
            u32 alliance = LD8(R5 + oBattleObject_Alliance);
            u32 col = LD8((u32)A_byte_80FDB80 + alliance * 2 + r4);
            R0 = col;
            R2 = param2;
            R3 = param3;
            /* Pass the output buffer pointer in R7 as the original does.
             * The leaf preserves R7, so we must save/restore to honour the
             * preserved-register contract. */
            u32 saved_r7 = in_r7;
            R7 = buf + 0x0C + r6;
            GUEST_CALL(object_getPanelsInColumnFiltered);
            in_r7 = saved_r7;
            r6 += R0;
        }

        if (r6 == 0) {
            /* Nothing reachable at all: stay put. */
            out_x = self_x;
            out_y = self_y;
            have_panel = true;
            goto use_panel;
        }
    }

    /* Pick one of the packed candidates uniformly at random.
     * SVC 6 = SoftDiv: returns quotient in R0, remainder in R1. */
    GUEST_CALL(GetPositiveSignedRNG);
    R1 = r6;
    swi_call(6);
    {
        u32 packed = LD8(buf + 0x0C + R1);
        out_y = packed >> 4;
        out_x = (packed << 29) >> 29;
    }
    have_panel = true;

use_panel:
    (void)have_panel;
    /* Release the column we reserved at the top. The original saves r0/r1
     * across this call with inline push/pop so the chosen panel survives. */
    {
        u32 save_x = out_x, save_y = out_y;
        R0 = (u32)LD8(R5 + oBattleObject_PanelX);
        R1 = (u32)LD8(R5 + oBattleObject_PanelY);
        GUEST_CALL(object_removePanelReserve);
        out_x = save_x;
        out_y = save_y;
    }

    R0 = out_x;
    R1 = out_y;
    R7 = in_r7;
    SP = buf + 0x24;
    RETURN_TO_CALLER();
}

/* ── sub_80FDCA4 ── */
/* sub_80FDCA4(r0 = panel count, r5 = battle object):
 * Starting from the object's own panel, highlight `count` panels in a row,
 * stepping in the direction the object is facing (object_getFrontDirection).
 * The last call's r1 is left as object_highlightPanel left it. */
Next sub_80FDCA4(void) {
    u32 count = R0;
    u32 obj = R5;

    u32 step = 1;
    do {
        /* Direction the object faces: -1, 0 or +1. */
        GUEST_CALL(object_getFrontDirection);
        s32 offset = (s32)R0 * (s32)step;

        R0 = offset + (s32)LD8(obj + oBattleObject_PanelX);
        R1 = LD8(obj + oBattleObject_PanelY);
        GUEST_CALL(object_highlightPanel);

        step++;
    } while ((s32)step <= (s32)count);

    RETURN_TO_CALLER();
}

/* ── sub_80FDEC4 ── */
/* sub_80FDEC4(r5 = battle object, r3/r10 = context pass-through):
 * Turn-end style housekeeping for one BattleObject:
 *   - query the AI difficulty tier (result stashed in r4, where the
 *     original keeps it; dead afterwards but restored from the stack),
 *   - call sub_80FE05C(2) (result kept in r6, likewise dead but must be
 *     live across the battle_findPlayer call),
 *   - find the player on the opposite alliance,
 *   - call sub_80CAC58(player, 2, obj + 0x60),
 *   - clear obj->ExtraVars[1] and store the object's panel position,
 *     packed as PanelX | PanelY << 4, into obj->ExtraVars[2].
 * Ends with r0 = the packed panel position, r1 = PanelY << 4 (values the
 * original leaves in place after the ORR), NZ from that ORR, C clear
 * (the LSL #4 of a byte can never shift a set bit out) and V untouched
 * from sub_80CAC58. r4/r6/r7 end as they came in. */
Next sub_80FDEC4(void) {
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(battle_getAiDifficultyTier_800FE36);
    R4 = R0; /* AI difficulty tier */

    R0 = 2;
    GUEST_CALL(sub_80FE05C);
    R6 = R0; /* kept live across the battle_findPlayer call below */

    /* look up the object on the other side of the field */
    R0 = (u32)LD8(R5 + oBattleObject_Alliance) ^ 1u;
    R1 = 1;
    GUEST_CALL(battle_findPlayer);
    /* r0/r1 now hold the callee's result, which sub_80CAC58 reads */

    R2 = 2;
    R7 = R5 + 0x60; /* scratch pointer the callee works on */
    GUEST_CALL(sub_80CAC58);
    R7 = savedR7;

    ST32(R5 + oBattleObject_ExtraVars + 4, 0);

    /* pack the panel position: X in the low byte, Y in the next nibble */
    R0 = (u32)LD8(R5 + oBattleObject_PanelX);
    R1 = (u32)LD8(R5 + oBattleObject_PanelY);
    /* lsl r1, r1, #4: carry is bit 28 of a byte-sized value, always 0 */
    cpu.c = 0;
    R1 <<= 4;
    R0 |= R1;
    SET_FLAGS_NZ(R0);
    ST32(R5 + oBattleObject_ExtraVars + 8, R0);

    R4 = savedR4;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── sub_80FDEFC ── */
/* sub_80FDEFC(r5 = battle object):
 * AI bookkeeping for a battle object, run each AI tick.
 *
 * 1. If the AI's state word (GetAIDataUnk0x48Flag) has bit 5 set, raise the
 *    OBJECT_FLAGS_UNK_4 flag on the object (marks it as "about to act").
 * 2. ExtraVars+8 holds the panel the object last targeted, packed as
 *    (panelY << 4) | panelX. If that stored target differs from the panel the
 *    object is actually standing on now (PanelX/PanelY), copy the old target
 *    into ExtraVars+4 (the "previous" slot) and record the current panel as
 *    the new stored target. If they already match, do nothing. */
Next sub_80FDEFC(void) {
    u32 obj = R5;

    GUEST_CALL(GetAIDataUnk0x48Flag);
    if (R0 & 0x20) {
        R0 = OBJECT_FLAGS_UNK_4;
        GUEST_CALL(object_setFlag1);
    }

    u32 storedTarget = LD32(obj + (oBattleObject_ExtraVars + 8));
    u32 targetX = storedTarget & 0x7;   /* low 3 bits: X coordinate */
    u32 targetY = storedTarget >> 4;    /* upper bits: Y coordinate */
    u32 panelX = LD8(obj + oBattleObject_PanelX);
    u32 panelY = LD8(obj + oBattleObject_PanelY);

    if (targetX != panelX || targetY != panelY) {
        ST32(obj + (oBattleObject_ExtraVars + 4), targetX | (targetY << 4));
        ST32(obj + (oBattleObject_ExtraVars + 8), panelX | (panelY << 4));
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

/* ── sub_80FDFD0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_sub_800FE28_result_sub_80FDFD0;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80FDFD0 call_sub_800FE28_sub_80FDFD0(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80FDFD0){ R0 };
}

/* sub_80FDFD0: hand sub_800FE28 the address of the constant table at
 * dword_80FDFE0 (0x14141414) along with the value already in r5.
 * Inputs arrive in r5; the result is r0 from the callee. */
static u32 run_with_config_table(u32 arg5) {
    return call_sub_800FE28_sub_80FDFD0(A_dword_80FDFE0, arg5).r0;
}

Next sub_80FDFD0(void) {
    R0 = run_with_config_table(R5);
    RETURN_TO_CALLER();
}

/* ── battle_setAIAttackDamage_80FDFE4 ── */
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

/* battle_setAIAttackDamage_80FDFE4(r0 = table index, r5 = BattleObject ptr) -> r3 = damage sub-block ptr; r2 from callee; r4,r6 preserved */
Next battle_setAIAttackDamage_80FDFE4(void) {
    R3 = store_ai_damage(R0, R5);
    RETURN_TO_CALLER();
}

/* ── sub_80FE05C ── */
/* sub_80FE05C(r0 = index):
 * Look up two versioned table entries (one for stats/cooldowns and a fixed per-version entry),
 * then combine them into a single u32: the low half from the first table, the high half from the
 * second table. Returns the packed value in r0; preserves r4 (input index) and r6 (first result). */
Next sub_80FE05C(void) {
    u32 index = R0;

    /* First lookup: per-version stats at byte_80FE090, stride 8 bytes. */
    R0 = index * 8 + A_byte_80FE090;
    R2 = 8;
    GUEST_CALL(battle_aiGetVersionedTableEntry_800FE12);
    u32 lo = R0;

    /* Second lookup: per-version constants at byte_80FE0B4, stride 8 bytes. */
    R0 = index * 8 + A_byte_80FE0B4;
    R2 = 8;
    GUEST_CALL(battle_aiGetVersionedTableEntry_800FE12);

    /* Pack: high 16 bits from the second table, low 16 bits from the first. */
    R0 = (R0 << 16) | lo;
    RETURN_TO_CALLER();
}

/* ── battle_aiDispatchAttackPhase_80FE0D4 ── */
/* Register form: pure jump-table trampoline for AI attack-phase dispatch.
 * The only "computation" is an indexed table load followed by a tail-call to one
 * of four handlers that receive/return all guest registers; no typed C interface
 * can express that flow. */
static void ai_attack_phase_dispatch(u32 r7) {
    u32 index = LD8(r7 + oAIAttackVars_HitElement);
    R0 = index;                                        /* byte the callee may inspect */
    R1 = LD32(A_off_80FE0E8 + index);                  /* Thumb function-pointer from table */

    switch (index) {
    case 0:  GUEST_CALL(sub_80FE0F8);                       break;
    case 4:  GUEST_CALL(sub_80FE130);                       break;
    case 8:  GUEST_CALL(battle_aiAttack_stepGlide_80FE1B4);                       break;
    case 12: GUEST_CALL(battle_aiTickAttackTimer_80FE1FE);  break;
    }

    R7 = r7;   /* original pop {r7,pc} restores the caller's r7 */
}

/* battle_aiDispatchAttackPhase_80FE0D4 – dispatch one of four AI attack-phase handlers by jump table.
 * r7 = AIAttackVars pointer; index byte read from oAIAttackVars_HitElement. */
Next battle_aiDispatchAttackPhase_80FE0D4(void) {
    ai_attack_phase_dispatch(R7);
    RETURN_TO_CALLER();
}

/* ── sub_80FE0F8 ── */
/* sub_80FE0F8(r5 = battle object, r7 = AI attack state):
 * Enemy AI windup handler for an attack. While the windup is running
 * (Unk_01 != 0), tick its timer (Unk_10) down; once it expires, hand
 * control back to the firing state by writing 4 to Unk_00. When the
 * windup has not started yet, begin it: mark it as begun, set the
 * "shot fired" bit in the AI flag word (bit 0x20), play animation 8
 * and load the windup timer with 14 frames. If the bit was already
 * set, go straight to the firing state instead. */
Next sub_80FE0F8(void) {
    if (LD8(R7 + oAIAttackVars_AttackElement) != 0) {
        /* windup in progress: count the timer down */
        s32 timer = (s32)LD16(R7 + oAIAttackVars_StepTimer) - 1;
        ST16(R7 + oAIAttackVars_StepTimer, (u16)timer);
        if (timer <= 0)
            ST16(R7 + oAIAttackVars_HitElement, 4); /* windup done: fire */
        RETURN_TO_CALLER();
    }

    /* start a new windup */
    ST8(R7 + oAIAttackVars_AttackElement, 4);

    GUEST_CALL(GetAIDataUnk0x48Flag);
    R1 = 0x20;
    if (R0 & R1) {
        /* shot already fired this attack: go straight to firing */
        ST16(R7 + oAIAttackVars_HitElement, 4);
        RETURN_TO_CALLER();
    }

    /* mark the shot as fired and start the windup animation */
    R0 = 0x20;
    GUEST_CALL(SetAIDataUnk0x48Flag); /* clobbers r1 */
    ST8(R5 + oBattleObject_CurAnim, 8);
    ST16(R7 + oAIAttackVars_StepTimer, 0xE);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttack_stepGlide_80FE1B4 ── */
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

    u32 orig_timer = (u32)LD16(ai_vars + oAIAttackVars_StepTimer);
    u32 timer = orig_timer - 1u;
    ST16(ai_vars + oAIAttackVars_StepTimer, (u16)timer);

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
    ST16(ai_vars + oAIAttackVars_HitElement, (u16)next_state);

    /* Terminal flags on fall-through: N=Z=C=V=0. ADD(1,0) yields exactly that. */
    return (MotionStepResult){ next_state, 1u, 0u };
}

Next battle_aiAttack_stepGlide_80FE1B4(void) {
    MotionStepResult s = step_battle_object_motion(R5, R7);
    R0 = s.r0;
    SET_FLAGS_ADD(s.add_a, s.add_b);
    RETURN_TO_CALLER();
}

/* ── battle_aiTickAttackTimer_80FE1FE ── */
/* battle_aiTickAttackTimer_80FE1FE: AI attack driver for one oAIAttackVars block (r7).
 *
 * The byte at offset oAIAttackVars_AttackElement (offset 1) is the attack's armed
 * flag:
 *  - 0: the attack hasn't been started yet. Arm it: store 4 into the flag
 *       and copy the attack's charge time (Unk_18) into the countdown Unk_10.
 *  - nonzero: count Unk_10 down by one; once it drops to zero (or wraps),
 *       call object_exitAttackState_801171C to actually perform the attack.
 *
 * The caller inspects r0 and the nz/c/v flags afterwards, so they are left
 * exactly as the original instruction stream leaves them; r5/r6/r7 pass
 * through untouched. */
Next battle_aiTickAttackTimer_80FE1FE(void) {
    if (LD8(R7 + 1) == 0) {
        /* start-up: arm the attack and load its countdown */
        ST8(R7 + 1, 4);
        u32 timer = LD16(R7 + oAIAttackVars_AttackDuration);
        ST16(R7 + oAIAttackVars_StepTimer, timer);
        SET_FLAGS_NZ(4);        /* flags of the original's `mov r0,#4` */
        R0 = timer;             /* r0 ends holding the value written to Unk_10 */
    } else {
        /* armed: tick the countdown */
        u32 left = LD16(R7 + oAIAttackVars_StepTimer);
        u32 timer = left - 1;
        ST16(R7 + oAIAttackVars_StepTimer, timer);
        SET_FLAGS_CMP(left, 1); /* flags of the original's `subs r0,#1` */
        R0 = timer;
        if ((s32)timer <= 0)    /* bgt not taken: countdown expired */
            GUEST_CALL(object_exitAttackState_801171C);
    }
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackPhaseDispatch_80FE21E ── */
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

/* register form: calls ai_stepAttackSpawnNext, AI attack phase 2 handler */
static void call_attack_phase2(u32 state, u32 entry, u32 vars) {
    R0 = state; R1 = entry; R7 = vars;
    GUEST_CALL(ai_stepAttackSpawnNext);
    R7 = vars;
}

/* register form: calls battle_aiAttackCountdownTick_80FE30C, AI attack phase 3 handler */
static void call_attack_phase3(u32 state, u32 entry, u32 vars) {
    R0 = state; R1 = entry; R7 = vars;
    GUEST_CALL(battle_aiAttackCountdownTick_80FE30C);
    R7 = vars;
}

/* Tail-dispatch to the current AI attack phase handler.
 * The state byte at [vars+oAIAttackVars_HitElement] is the table offset (0,4,8,12);
 * it indexes off_80FE230 to pick one of four handlers, each receiving the state
 * in r0 and its own entry address in r1. */
static void ai_attack_dispatch_sub_80FE21E(u32 vars) {
    u32 state = LD8(vars + oAIAttackVars_HitElement);
    u32 entry = LD32(A_off_80FE230 + state);

    switch (state) {
        case 0:  call_attack_phase0(state, entry, vars); break;
        case 4:  call_attack_phase1(state, entry, vars); break;
        case 8:  call_attack_phase2(state, entry, vars); break;
        case 12: call_attack_phase3(state, entry, vars); break;
        default: break;
    }
}

Next battle_aiAttackPhaseDispatch_80FE21E(void) {
    ai_attack_dispatch_sub_80FE21E(R7);
    RETURN_TO_CALLER();
}

/* ── sub_80FE240 ── */
/* sub_80FE240(r5 = battle object ptr, r7 = AI attack vars ptr):
 * Advance the per-attack AI timer.  On the first tick it initialises the
 * attack and either fires it immediately or starts a delay countdown.
 * While the delay is active it decrements the countdown each tick; once
 * the countdown expires (or no delay was needed) it sets the trigger
 * field to 4, which the outer AI loop interprets as "attack now". */
Next sub_80FE240(void) {
    u32 aiVars = R7;
    u32 battleObj = R5;

    if (LD8(aiVars + oAIAttackVars_AttackElement) != 0) {
        /* Already initialised – tick down the delay counter */
        s32 remaining = (s32)LD16(aiVars + oAIAttackVars_StepTimer) - 1;
        ST16(aiVars + oAIAttackVars_StepTimer, (u16)remaining);

        if (remaining > 0) {
            /* Still waiting – return unchanged */
            RETURN_TO_CALLER();
        }

        /* Countdown expired: fire the attack */
        ST16(aiVars + oAIAttackVars_HitElement, 4);
    } else {
        /* First tick: set up the attack state */
        ST8(aiVars + oAIAttackVars_AttackElement, 4);
        ST8(aiVars + oAIAttackVars_TargetPanelX, 0);
        ST8(aiVars + oAIAttackVars_TargetPanelY, 0);

        GUEST_CALL(GetAIDataUnk0x48Flag);

        R1 = 0x20;  /* delay-required bit */
        if ((R0 & R1) == 0) {
            /* No delay: trigger the attack immediately */
            ST16(aiVars + oAIAttackVars_HitElement, 4);
        } else {
            /* Delayed attack: clear the gate flag, begin countdown */
            R0 = 0x20;
            GUEST_CALL(ClearAIDataUnk0x48Flag);

            ST8(battleObj + oBattleObject_CurAnim, 7);

            R0 = 0xE1;
            GUEST_CALL(PlaySoundEffect);

            ST16(aiVars + oAIAttackVars_StepTimer, 0x13);  /* 19-tick delay */
        }
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
    if (LD8(R7 + 1 /* oAIAttackVars_AttackElement */) == 0) {
        /* Begin the wind-up phase. */
        ST8(R7 + 1 /* oAIAttackVars_AttackElement */, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        R0 = 0x1E;
        ST16(R7 + oAIAttackVars_StepTimer, R0);
        SET_FLAGS_NZ(R0);
    } else {
        /* Count the wind-up timer down; once it hits zero, attack. */
        u32 remaining = LD16(R7 + oAIAttackVars_StepTimer);
        u32 left = remaining - 1;
        ST16(R7 + oAIAttackVars_StepTimer, left);
        SET_FLAGS_CMP(remaining, 1);
        if ((s32)left > 0) {
            R0 = left;
            RETURN_TO_CALLER();
        }
        R0 = 8;
        ST16(R7 /* oAIAttackVars_HitElement */, R0);
        SET_FLAGS_NZ(R0);
    }
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackCountdownTick_80FE30C ── */
/* Countdown phase: when first entered, copy Unk_18 to Unk_10 and start the timer.
 * Each subsequent tick decrements Unk_10; when it reaches zero, clear the attack
 * animation and exit the attack state.
 * r7 = AI attack vars pointer, r5 = battle object pointer. */
static void ai_attack_countdown_tick(u32 battle_object, u32 ai_attack_vars) {
    u8 phase = LD8(ai_attack_vars + oAIAttackVars_AttackElement);
    if (phase == 0) {
        ST8(ai_attack_vars + oAIAttackVars_AttackElement, 4);
        ST16(ai_attack_vars + oAIAttackVars_StepTimer,
             LD16(ai_attack_vars + oAIAttackVars_AttackDuration));
        return;
    }
    u32 timer = LD16(ai_attack_vars + oAIAttackVars_StepTimer) - 1;
    ST16(ai_attack_vars + oAIAttackVars_StepTimer, (u16)timer);
    if ((s32)timer > 0)
        return;
    ST8(battle_object + oBattleObject_CurAnim, 0);
    /* register form: calls object_exitAttackState outside this file, which has no typed C interface yet */
    GUEST_CALL(object_exitAttackState);
}

Next battle_aiAttackCountdownTick_80FE30C(void) {
    ai_attack_countdown_tick(R5, R7);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── ai_chargeShakeUpdate ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_camera_initShakeEffect_80302a8_result;
/* register form: generated wrapper: camera_initShakeEffect_80302a8 has no typed C interface yet */
static inline call_camera_initShakeEffect_80302a8_result call_camera_initShakeEffect_80302a8(u32 r0, u32 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(camera_initShakeEffect_80302a8);
    return (call_camera_initShakeEffect_80302a8_result){ R2 };
}

typedef struct { u32 r1, r5, r7; } call_battle_highlightTargetColumns_result;
/* register form: generated wrapper: battle_highlightTargetColumns has no typed C interface yet */
static inline call_battle_highlightTargetColumns_result call_battle_highlightTargetColumns(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_highlightTargetColumns);
    return (call_battle_highlightTargetColumns_result){ R1, R5, R7 };
}

/* ai_chargeShakeUpdate (0x080fe394): drives one frame of the charge/shake attack animation.
 * Initializes the animation on first call, then each frame ticks the camera shake countdown,
 * occasionally re-rolls the battle state via battle_highlightTargetColumns, and advances to
 * the next animation phase when the duration expires. */
static void ai_charge_shake_update(u32 battle_obj, u32 ai_vars) {
    if (LD8(ai_vars + oAIAttackVars_AttackElement) == 0) {
        /* First entry: initialize the attack animation state. */
        ST8(ai_vars + oAIAttackVars_AttackElement, 4);
        ST8(battle_obj + oBattleObject_CurAnim, 4);
        ST16(ai_vars + oAIAttackVars_StepTimer, LD8(ai_vars + oAIAttackVars_AttackBoost2));
        ST8(ai_vars + oAIAttackVars_TimerOrCycles, 0);
        call_camera_initShakeEffect_80302a8(0, 0xA);
        ST16(ai_vars + oAIAttackVars_ChargeCycles, 0xA);
        return;
    }

    /* Decrement shake cycle counter; restart shake effect when expired. */
    u32 shake = LD16(ai_vars + oAIAttackVars_ChargeCycles) - 1;
    ST16(ai_vars + oAIAttackVars_ChargeCycles, shake);
    if ((s32)shake <= 0) {
        call_camera_initShakeEffect_80302a8(0, 0xA);
        ST16(ai_vars + oAIAttackVars_ChargeCycles, 0xA);
    }

    /* Increment step counter; re-roll target columns unless bit 2 of the value is set
     * (the original does lsr #3 and skips the call on carry). */
    u32 step = (u32)(LD8(ai_vars + oAIAttackVars_TimerOrCycles) + 1);
    ST8(ai_vars + oAIAttackVars_TimerOrCycles, step);
    if (((step >> 2) & 1) == 0)
        call_battle_highlightTargetColumns(battle_obj);

    /* Countdown animation duration; advance to next phase when finished. */
    u32 ticks = LD16(ai_vars + oAIAttackVars_StepTimer) - 1;
    ST16(ai_vars + oAIAttackVars_StepTimer, ticks);
    if ((s32)ticks <= 0)
        ST16(ai_vars + oAIAttackVars_HitElement, 8);
}

/* ai_chargeShakeUpdate(r5 = battle obj, r7 = AI attack vars, r10, lr) -> r1, r5 (via callee wrappers) */
Next ai_chargeShakeUpdate(void) {
    ai_charge_shake_update(R5, R7);
    RETURN_TO_CALLER();
}

/* ── aiAttackDispatchHandler_80FE452 ── */
/* aiAttackDispatchHandler_80FE452: AI attack command dispatcher.
 *
 * r7 points to the AI attack state block. The byte at offset 0
 * (oAIAttackVars_HitElement) indexes the handler table at off_80FE464:
 *     entry 0 -> ai_stageWoodAttackHit
 *     entry 1 -> aiAttackState_tickTimer_sub_80FE4AC
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
    u32 selector = LD8(R7);              /* byte at oAIAttackVars_HitElement (offset 0) */

    R0 = selector;
    if (selector == 0) {
        R1 = LD32(A_off_80FE464);        /* .word ai_stageWoodAttackHit+1 */
        GUEST_CALL(ai_stageWoodAttackHit);
    } else {
        R1 = LD32(A_off_80FE464 + 4);    /* .word aiAttackState_tickTimer_sub_80FE4AC+1 */
        GUEST_CALL(aiAttackState_tickTimer_sub_80FE4AC);
    }

    RETURN_TO_CALLER();
}

/* ── aiAttackState_tickTimer_sub_80FE4AC ── */
/* aiAttackState_tickTimer_sub_80FE4AC: AI attack-state tick.
 *
 * Inputs:  r5 = BattleObject*, r6/r7 = AI attack vars / context, lr = return address.
 * Outputs: r1.
 * Clobbers: r0, r1, r2, r3, r6, r7, lr, flags.
 *
 * First call initializes the state (sets Unk_01 = 4, CurAnim = 0, copies Unk_18
 * into the timer Unk_10). Subsequent calls tick the timer and, when it reaches
 * zero, exit the attack state via object_exitAttackState.
 */
Next aiAttackState_tickTimer_sub_80FE4AC(void) {
    if (LD8(R7 + oAIAttackVars_AttackElement) == 0) {
        /* First-time initialization. */
        ST8(R7 + oAIAttackVars_AttackElement, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST16(R7 + oAIAttackVars_StepTimer, LD16(R7 + oAIAttackVars_AttackDuration));
    } else {
        /* Tick the countdown timer. */
        u32 timer = LD16(R7 + oAIAttackVars_StepTimer) - 1;
        ST16(R7 + oAIAttackVars_StepTimer, (u16)timer);
        if (!(timer & 0x80000000) && timer != 0) {
            /* timer > 0 signed -> still ticking, return. */
        } else {
            GUEST_CALL(object_exitAttackState);
        }
    }
    RETURN_TO_CALLER();
}

/* ── battleObject_panelToPixelOffset_80FE5C0 ── */
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

Next battleObject_panelToPixelOffset_80FE5C0(void) {
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

/* ── nav_pickAdjacentPanel_80FE5EE ── */
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

/* nav_pickAdjacentPanel_80FE5EE(r0=target_x, r1=target_y, r2=panel_ptr, r5=battle_obj) -> r0,r1
 * Outputs r3, r5, r7: r3 is left by callees/SWI; r5 and r7 are preserved. */
Next nav_pickAdjacentPanel_80FE5EE(void) {
    u32 x, y;
    select_panel(R0, R1, R2, R5, &x, &y);
    R0 = x;
    R1 = y;
    RETURN_TO_CALLER();
}

/* ── ai_circusManPickAttackOffset ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result call_sub_800FE28(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result){ R0, R2 };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

/* CircusMan AI: choose the offset (in the attack table) of the attack to use.
 *
 * Two weighted random picks are made through sub_800FE28 over the weight table
 * byte_80FE9DC ({14,14,14,14,0,0,0,0}): the first pick (entries 0-3) gives a
 * base offset, the second (entries 4-7) gives a value multiplied by the
 * object's ExtraVars[8] counter. If the object's flag bits 0xA000 are set
 * (some CircusMan state), 60 is added to shift to the alternate attack set.
 */
#define CIRCUSMAN_EXTRA_ATTACK_DELTA 0x3C
#define CIRCUSMAN_FLAG_MASK          0xA000

static u32 circusman_pick_attack_offset(u32 object)
{
    u32 weight_table = A_byte_80FE9DC;
    u32 offset;

    /* First pick: base attack offset from the first half of the weight table. */
    offset = call_sub_800FE28(weight_table, object).r0;

    /* Second pick: scaled by ExtraVars[8] and added to the base offset. */
    u32 pick = call_sub_800FE28(weight_table + 4, object).r0;
    u32 extra = LD32(object + (oBattleObject_ExtraVars + 8));
    offset += pick * extra;

    /* Flag bits 0xA000 select the alternate attack group. */
    if (call_object_getFlag(object) & CIRCUSMAN_FLAG_MASK)
        offset += CIRCUSMAN_EXTRA_ATTACK_DELTA;

    return offset;
}

/* ai_circusManPickAttackOffset(r5 = battle object) -> r0 */
Next ai_circusManPickAttackOffset(void) {
    R0 = circusman_pick_attack_offset(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80FEA08 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80FEA08;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80FEA08 call_sub_800FE28_sub_80FEA08(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80FEA08){ R0, R2 };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag_sub_80FEA08(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

/* sub_80FEA08 — compute a value for a battle object (r5):
 * - look up two table entries via sub_800FE28, one at byte_80FEA34 and one at +4,
 * - scale the second by the object's ExtraVars counter at +0xC,
 * - add them, then add 0x3C if the object's flag bits 0xA000 are set.
 * Returns the total in r0; flags and r1/r2 are scratch. r4 is preserved. */
static u32 sub_80FEA08_compute(u32 obj) {
    u32 table = A_byte_80FEA34;

    u32 first = call_sub_800FE28_sub_80FEA08(table, obj).r0;
    u32 second = call_sub_800FE28_sub_80FEA08(table + 4, obj).r0;

    u32 count = LD32(obj + (oBattleObject_ExtraVars + 0xC));
    u32 total = first + second * count;

    if (call_object_getFlag_sub_80FEA08(obj) & 0xA000)
        total += 0x3C;

    return total;
}

/* sub_80FEA08(r5 = battle object) -> r0 */
Next sub_80FEA08(void) {
    R0 = sub_80FEA08_compute(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80FEA3C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80FEA3C;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80FEA3C call_sub_800FE28_sub_80FEA3C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80FEA3C){ R0, R2 };
}

/* Three related values live as consecutive entries in a small table at
 * byte_80FEA68. sub_800FE28 resolves one entry (its pointer in r0, context
 * in r5); this routine fetches entries 0, 1 and 2 and returns them in
 * r0, r1 and r2. */
#define BYTE_TABLE A_byte_80FEA68

static void fetch_table_triple(u32 table, u32 r5, u32 out[3]) {
    out[0] = call_sub_800FE28_sub_80FEA3C(table + 0, r5).r0;
    out[1] = call_sub_800FE28_sub_80FEA3C(table + 4, r5).r0;
    out[2] = call_sub_800FE28_sub_80FEA3C(table + 8, r5).r0;
}

/* sub_80FEA3C(r5 = context) -> r0, r1, r2 */
Next sub_80FEA3C(void) {
    u32 results[3];
    fetch_table_triple(BYTE_TABLE, R5, results);
    R0 = results[0];
    R1 = results[1];
    R2 = results[2];
    RETURN_TO_CALLER();
}

/* ── ai_circusManLoadAttackParams ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_battle_aiGetVersionedTableEntry_800FE12_result;
/* register form: generated wrapper: battle_aiGetVersionedTableEntry_800FE12 has no typed C interface yet */
static inline call_battle_aiGetVersionedTableEntry_800FE12_result call_battle_aiGetVersionedTableEntry_800FE12(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(battle_aiGetVersionedTableEntry_800FE12);
    return (call_battle_aiGetVersionedTableEntry_800FE12_result){ R0, R2 };
}

/* CircusMan AI: read the version-dependent damage and attack-id bytes for the
 * custom attack whose index arrives in r0, pack them as one 32-bit pair and
 * store them into this object's AI attack variables. */

/* battle_aiGetVersionedTableEntry_800FE12: (r0 = table entry index, r5 = battle
 * object) -> r0 = first halfword of the entry, r2 = second halfword. */
static u32 circ_attack_value(u32 attackId, u32 baseTable, u32 battleObject)
{
    call_battle_aiGetVersionedTableEntry_800FE12_result entry =
        call_battle_aiGetVersionedTableEntry_800FE12(attackId * 8 + baseTable, battleObject);
    return entry.r0;
}

static void ai_circusManLoadAttackParams_typed(u32 attackId, u32 battleObject)
{
    u32 damage    = circ_attack_value(attackId, A_byte_80FEC44, battleObject);
    u32 attackId2 = circ_attack_value(attackId, A_byte_80FEC68, battleObject);

    u32 packed = damage | (attackId2 << 16);

    u32 attackVars = LD32(battleObject + oBattleObject_AIDataPtr) + 0xA0;
    ST32(attackVars + oAIAttackVars_Damage, packed);
}

Next ai_circusManLoadAttackParams(void)
{
    ai_circusManLoadAttackParams_typed(R0, R5);
    RETURN_TO_CALLER();
}

/* ── aiAttackStepChooseMovePanel_80FEE44 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r4; bool z, n, c, v; } call_object_canMove_result_sub_80FEE44;
/* register form: generated wrapper: object_canMove has no typed C interface yet */
static inline call_object_canMove_result_sub_80FEE44 call_object_canMove_sub_80FEE44(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_canMove);
    return (call_object_canMove_result_sub_80FEE44){ R1, R4, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r3; bool c, v; } call_object_reservePanel_result_sub_80FEE44;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_sub_80FEE44 call_object_reservePanel_sub_80FEE44(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_sub_80FEE44){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2; } call_object_setFlag1_result_sub_80FEE44;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_sub_80FEE44 call_object_setFlag1_sub_80FEE44(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_sub_80FEE44){ R2 };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_sub_80FEE44;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_sub_80FEE44 call_object_clearCollisionRegion_sub_80FEE44(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_sub_80FEE44){ R1 };
}

/* aiAttackStepChooseMovePanel_80FEE44: AI attack step handler.
 *
 * r5 = BattleObject*, r7 = AIAttackVars* of the acting object.
 * If no attack element is queued yet, the object is marked as attacking and
 * mobility is checked: a mobile object asks the AI callback which panel to
 * move to, reserves it and starts the move animation; an immobile one falls
 * back to a stationary attack. If an element is already queued, the step
 * timer counts down and the hit element fires once it reaches zero.
 *
 * r1 on exit: on the timer path the original never touches r1 (it stays as
 * on entry); on the other paths it is the last callee's r1. r5/r8-r12 pass
 * through untouched. */

#define ANIM_IDLE          4     /* written to AttackElement, CurAnim, HitElement */
#define HIT_ELEMENT_SHOCK  0x18
#define ATTACK_DURATION    0x3C
#define STEP_TIMER_RELOAD  5

struct panel_choice {
    u32 x;
    u32 y;
};

/* register form: indirect call through the AI's panel-choose callback
 * (mov lr, pc; bx r1). On entry the callback sees its own address in r1,
 * the target object in r0 and two per-alliance parameters in r2/r3. */
static struct panel_choice call_panel_choose_sub_80FEE44(u32 callback, u32 target, u32 data0, u32 data1) {
    R0 = target;
    R1 = callback;
    R2 = data0;
    R3 = data1;
    GUEST_CALL_ADDR(callback);
    struct panel_choice ret = { R0, R1 };
    return ret;
}

/* Stationary attack fallback: fixed duration and hit element. */
static void stationary_attack(u32 ai) {
    ST16(ai + oAIAttackVars_AttackDuration, ATTACK_DURATION);
    ST16(ai + oAIAttackVars_HitElement, HIT_ELEMENT_SHOCK);
}

static u32 sub_80FEE44_step(u32 obj, u32 ai, u32 r1_out) {
    u32 element = LD8(ai + oAIAttackVars_AttackElement);

    /* Attack element already queued: tick the timer, fire when it runs out. */
    if (element != 0) {
        u32 timer = LD16(ai + oAIAttackVars_StepTimer) - 1;
        ST16(ai + oAIAttackVars_StepTimer, timer);
        if ((s32)timer <= 0)               /* BGT after SUBS: taken only while > 0 */
            ST16(ai + oAIAttackVars_HitElement, ANIM_IDLE);
        return r1_out;
    }

    /* New attack: remember we are attacking, then check if we can move. */
    ST8(ai + oAIAttackVars_AttackElement, ANIM_IDLE);

    call_object_canMove_result_sub_80FEE44 can = call_object_canMove_sub_80FEE44(obj);
    if (can.z) {                           /* beq: cannot move */
        stationary_attack(ai);
        return can.r1;
    }

    /* Ask the AI which panel to move to; parameters come from a small
     * per-alliance table. */
    u32 table = A_byte_80FEEA8 + (LD8(obj + oBattleObject_Alliance) << 3);
    struct panel_choice pick = call_panel_choose_sub_80FEE44(
        LD32(ai + oAIAttackVars_PanelChooseCallback),
        LD32(ai + oAIAttackVars_TargetObjectPtr),
        LD32(table),
        LD32(table + 4));

    if (pick.x == 0) {                     /* tst r0,r0 / beq: no panel chosen */
        stationary_attack(ai);
        return pick.y;
    }

    /* Commit the chosen panel and start the move. */
    ST8(obj + oBattleObject_FuturePanelX, pick.x);
    ST8(obj + oBattleObject_FuturePanelY, pick.y);
    call_object_reservePanel_sub_80FEE44(pick.x, pick.y, obj);

    ST8(obj + oBattleObject_CurAnim, ANIM_IDLE);
    call_object_setFlag1_sub_80FEE44(OBJECT_FLAGS_CANNOT_SLIDE, obj);
    call_object_clearCollisionRegion_result_sub_80FEE44 clr = call_object_clearCollisionRegion_sub_80FEE44(obj);

    ST16(ai + oAIAttackVars_StepTimer, STEP_TIMER_RELOAD);
    return clr.r1;
}

/* aiAttackStepChooseMovePanel_80FEE44(r5 = BattleObject*, r7 = AIAttackVars*) -> r1 */
Next aiAttackStepChooseMovePanel_80FEE44(void) {
    R1 = sub_80FEE44_step(R5, R7, R1);
    RETURN_TO_CALLER();
}

/* ── battle_resetAIAttackStepState_80FEF50 ── */
/* Reset an AI attack's per-step state: no sub-step, no panel param flags,
 * and the hit element back to its default (0xC). */

static void reset_attack_step_state(u32 vars)
{
    ST8(vars + oAIAttackVars_SubStepIndex, 0);
    ST32(vars + oAIAttackVars_PanelParamFlags2, 0);
    ST16(vars + oAIAttackVars_HitElement, 0xC);
}

/* battle_resetAIAttackStepState_80FEF50(r7 = AIAttackVars pointer) */
Next battle_resetAIAttackStepState_80FEF50(void) {
    reset_attack_step_state(R7);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackWindupTick_80FEFFA ── */
/* AI attack wind-up tick for one battle object.
 * If no attack element has been chosen yet, start the attack: element 4,
 * animation 9, and a step timer of 0x17. Otherwise count the step timer
 * down; once it expires, record hit element 0x18. */
static void ai_attack_windup_tick_sub_80FEFFA(u32 object, u32 vars) {
    u32 attack_element = LD8(vars + oAIAttackVars_AttackElement);

    if (attack_element == 0) {
        /* Begin the wind-up: choose element, play the charge animation, start the timer. */
        ST8(vars + oAIAttackVars_AttackElement, 4);
        ST8(object + oBattleObject_CurAnim, 9);
        ST16(vars + oAIAttackVars_StepTimer, 0x17);
    } else {
        u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
        ST16(vars + oAIAttackVars_StepTimer, timer);
        if ((s32)timer <= 0)            /* timer expired (GT branch not taken) */
            ST16(vars + oAIAttackVars_HitElement, 0x18);
    }
}

/* battle_aiAttackWindupTick_80FEFFA(r5 = battle object, r7 = AI attack vars) -> nothing */
Next battle_aiAttackWindupTick_80FEFFA(void) {
    ai_attack_windup_tick_sub_80FEFFA(R5, R7);
    RETURN_TO_CALLER();
}

/* ── ai_dispatchAttackHandler_80FF042 ── */
/* AI attack dispatcher (ai_dispatchAttackHandler_80FF042).
 *
 * Reads the HitElement byte from the AI attack block pointed to by r7 and uses
 * it directly as a byte offset into the handler table at off_80FF054 (valid
 * element values are already multiples of 4 in memory):
 *
 *   0x00 -> battle_aiAttackStepWalk_80FF068
 *   0x04 -> battleObject_moveAlongThrow_80FF0DC
 *   0x08 -> ai_attackChargeHandler
 *   0x0C -> sub_80FF1A2
 *   0x10 -> ai_runAttackTimer
 *
 * It enters the handler with r0 = HitElement and r1 = the table entry, and
 * when the handler returns, this function returns straight back to its own
 * caller. The handler's register state (r1, r5, r8-r12) is what the callers
 * of this function see as its outputs, so it must stay live in the guest
 * register file untouched.
 */

/* register form: the dispatch target is a runtime address taken from the jump
 * table (no wrapper exists for it), and the handler's register results must
 * survive in the guest register file, so this call cannot be expressed as an
 * ordinary typed call. */
static void call_attack_handler_sub_80FF042(u32 target, u32 element, u32 r3)
{
    R0 = element;   /* handler entry state: r0 = HitElement */
    R1 = target;    /* the original leaves the table entry in r1 */
    R3 = r3;        /* r3 passes through to the handler unchanged */

    GUEST_CALL_ADDR(target);
}

static void run_attack_state_handler(u32 attackVars, u32 r3)
{
    u32 element = LD8(attackVars + oAIAttackVars_HitElement);
    u32 target  = LD32(A_off_80FF054 + element);

    call_attack_handler_sub_80FF042(target, element, r3);
}

/* ai_dispatchAttackHandler_80FF042(r7 = AI attack vars, r3/r5/r10/lr pass through) ->
 * handler results in r1 r5 r8 r9 r10 r11 r12 */
Next ai_dispatchAttackHandler_80FF042(void) {
    run_attack_state_handler(R7, R3);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackStepWalk_80FF068 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r4; bool z, n, c, v; } call_object_canMove_result_sub_80FF068;
/* register form: generated wrapper: object_canMove has no typed C interface yet */
static inline call_object_canMove_result_sub_80FF068 call_object_canMove_sub_80FF068(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_canMove);
    return (call_object_canMove_result_sub_80FF068){ R1, R4, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r3; bool c, v; } call_object_reservePanel_result_sub_80FF068;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_sub_80FF068 call_object_reservePanel_sub_80FF068(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_sub_80FF068){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r2; } call_object_setFlag1_result_sub_80FF068;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_sub_80FF068 call_object_setFlag1_sub_80FF068(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_sub_80FF068){ R2 };
}

typedef struct { u32 r1; } call_object_clearCollisionRegion_result_sub_80FF068;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result_sub_80FF068 call_object_clearCollisionRegion_sub_80FF068(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result_sub_80FF068){ R1 };
}

/* AI attack step handler (r5 = battle object, r7 = AIAttackVars).
 *
 * First frame (AttackElement == 0): mark the attack as walking, check that
 * the object can move, then ask the AI's panel-choose callback (given two
 * context words from a per-alliance table) which panel to step to, and start
 * the walk.  If the object cannot move or the callback finds no panel, the
 * attack is cancelled (AttackDuration = 0x3C, HitElement = 0x10).
 * Later frames (AttackElement != 0): count StepTimer down; when it expires,
 * set HitElement = 4 to trigger the hit.
 *
 * r1 on return: the entry r1 on the countdown paths, otherwise whatever the
 * last callee (object_canMove / the callback / object_clearCollisionRegion)
 * left there.  r5 and r8-r12 are untouched. */

#define AI_ELEM_WALKING          4   /* AttackElement value while walking */
#define WALK_ANIM                4   /* oBattleObject_CurAnim while walking */
#define STEP_TIMER_WALK          5   /* StepTimer set when the walk starts */
#define HIT_ELEM_STEP_EXPIRED    4   /* HitElement when the step timer runs out */
#define ATTACK_DURATION_CANCEL   0x3C
#define HIT_ELEM_CANCELLED       0x10

/* Cancel the attack: long duration and the default hit element. */
static void cancel_attack(u32 vars)
{
    ST16(vars + oAIAttackVars_AttackDuration, ATTACK_DURATION_CANCEL);
    ST16(vars + oAIAttackVars_HitElement, HIT_ELEM_CANCELLED);
}

/* Invoke the AI's panel-choose callback through its function pointer
 * (mov lr, pc; bx r1) and report the panel it chose (r0 = X, r1 = Y).
 * register form: the callback address is a runtime value and no generated
 * wrapper covers this call, so the arguments must be placed in R0-R3 by
 * hand (R1 also carries the callback address itself, as in the original)
 * and both result registers R0 and R1 must be read back afterwards. */
static u32 call_panel_choose_sub_80FF068(u32 callback, u32 target, u32 arg2, u32 arg3,
                             u32 *chosen_y)
{
    R0 = target;
    R1 = callback;
    R2 = arg2;
    R3 = arg3;
    GUEST_CALL_ADDR(callback);
    *chosen_y = R1;
    return R0;
}

/* Typed body; returns the r1 value the original leaves behind. */
static u32 ai_attack_step_sub_80FF068(u32 object, u32 vars, u32 r1_in)
{
    /* Attack already under way: run the step countdown. */
    if (LD8(vars + oAIAttackVars_AttackElement) != 0) {
        s32 timer = (s32)LD16(vars + oAIAttackVars_StepTimer) - 1;
        ST16(vars + oAIAttackVars_StepTimer, (u16)timer);
        if (timer > 0)
            return r1_in;
        /* Timer expired: trigger the hit. */
        ST16(vars + oAIAttackVars_HitElement, HIT_ELEM_STEP_EXPIRED);
        return r1_in;
    }

    /* First frame: flag the attack as walking and check movement. */
    ST8(vars + oAIAttackVars_AttackElement, AI_ELEM_WALKING);

    call_object_canMove_result_sub_80FF068 can = call_object_canMove_sub_80FF068(object);
    if (can.z) {                        /* Z set: object cannot move */
        cancel_attack(vars);
        return can.r1;
    }

    /* Ask the callback which panel to walk to; the per-alliance table
     * at byte_80FF0CC supplies two words of context for it. */
    u32 alliance    = LD8(object + oBattleObject_Alliance);
    u32 table_entry = A_byte_80FF0CC + (alliance << 3);
    u32 target      = LD32(vars + oAIAttackVars_TargetObjectPtr);
    u32 callback    = LD32(vars + oAIAttackVars_PanelChooseCallback);

    u32 chosen_y;
    u32 chosen_x = call_panel_choose_sub_80FF068(callback, target,
                                     LD32(table_entry), LD32(table_entry + 4),
                                     &chosen_y);
    if (chosen_x == 0) {                /* no panel chosen: cancel */
        cancel_attack(vars);
        return chosen_y;
    }

    /* Commit the chosen panel and start the walk. */
    ST8(object + oBattleObject_FuturePanelX, (u8)chosen_x);
    ST8(object + oBattleObject_FuturePanelY, (u8)chosen_y);
    call_object_reservePanel_sub_80FF068(chosen_x, chosen_y, object);

    ST8(object + oBattleObject_CurAnim, WALK_ANIM);
    call_object_setFlag1_sub_80FF068(OBJECT_FLAGS_CANNOT_SLIDE, object);
    call_object_clearCollisionRegion_result_sub_80FF068 cleared =
        call_object_clearCollisionRegion_sub_80FF068(object);
    ST16(vars + oAIAttackVars_StepTimer, STEP_TIMER_WALK);
    return cleared.r1;
}

/* battle_aiAttackStepWalk_80FF068(r5 = battle object, r7 = AI attack vars) -> r1
 * r5 and r8-r12 leave the function exactly as they entered. */
Next battle_aiAttackStepWalk_80FF068(void)
{
    R1 = ai_attack_step_sub_80FF068(R5, R7, R1);
    RETURN_TO_CALLER();
}

/* ── ai_attackChargeHandler ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; bool c, v; } call_object_setCounterTime_result_ai_attackChargeHandler;
/* register form: generated wrapper: object_setCounterTime has no typed C interface yet */
static inline call_object_setCounterTime_result_ai_attackChargeHandler call_object_setCounterTime_ai_attackChargeHandler(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCounterTime);
    return (call_object_setCounterTime_result_ai_attackChargeHandler){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* AI charge-attack handler: drives the "charging up" phase of an enemy attack.
 * r5 = battle object, r7 = the AI's attack variables.
 * While the attack element is unset (0), start charging: pick element 4, play
 * animation 10 and arm the step timer from the attack-boost value. While
 * charging, count the timer down; when it runs out, reset the step index,
 * ask the object layer for an 18-frame counter delay and record hit element 12.
 * r1 is only ever set by the object_setCounterTime call; on every other path
 * the original leaves it alone, so it is passed through unchanged. */
static u32 ai_charge_attack_step(u32 r1_out, u32 object, u32 ai) {
    if (LD8(ai + oAIAttackVars_AttackElement) == 0) {
        /* Not charging yet: begin the charge. */
        ST8(ai + oAIAttackVars_AttackElement, 4);
        ST8(object + oBattleObject_CurAnim, 0xA);
        ST16(ai + oAIAttackVars_StepTimer,
             LD8(ai + oAIAttackVars_AttackBoost2));
        return r1_out;
    }

    /* Charging: tick the step timer down. */
    u32 timer = LD16(ai + oAIAttackVars_StepTimer) - 1;
    ST16(ai + oAIAttackVars_StepTimer, timer);
    if ((s32)timer > 0)
        return r1_out;                      /* still counting */

    /* Timer expired: charge released. */
    ST8(ai + oAIAttackVars_AttackStepIndex, 0);
    u32 r1 = call_object_setCounterTime_ai_attackChargeHandler(0x12, object).r1;
    ST16(ai + oAIAttackVars_HitElement, 0xC);
    return r1;
}

Next ai_attackChargeHandler(void) {
    R1 = ai_charge_attack_step(R1, R5, R7);
    RETURN_TO_CALLER();
}

/* ── ai_chargedPanelAttack ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r5; } call_battle_findForwardUsableAttackPanel_result;
/* register form: generated wrapper: battle_findForwardUsableAttackPanel has no typed C interface yet */
static inline call_battle_findForwardUsableAttackPanel_result call_battle_findForwardUsableAttackPanel(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_findForwardUsableAttackPanel);
    return (call_battle_findForwardUsableAttackPanel_result){ R0, R1, R2, R5 };
}

typedef struct { u32 r1, r2, r3, r4, r5; } call_battle_endRelatedObjectAnims_result;
/* register form: generated wrapper: battle_endRelatedObjectAnims has no typed C interface yet */
static inline call_battle_endRelatedObjectAnims_result call_battle_endRelatedObjectAnims(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_endRelatedObjectAnims);
    return (call_battle_endRelatedObjectAnims_result){ R1, R2, R3, R4, R5 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r10; } call_PlaySoundEffect_result_ai_chargedPanelAttack;
/* register form: generated wrapper: PlaySoundEffect has no typed C interface yet */
static inline call_PlaySoundEffect_result_ai_chargedPanelAttack call_PlaySoundEffect_ai_chargedPanelAttack(u32 r0, u32 r2) {
    R0 = r0;
    R2 = r2;
    GUEST_CALL(PlaySoundEffect);
    return (call_PlaySoundEffect_result_ai_chargedPanelAttack){ R1, R2, R3, R4, R5, R6, R7, R10 };
}

typedef struct { u32 r1, r5; } call_sub_80FF7A4_result;
/* register form: generated wrapper: aiAttackForwardToType5E_80FF7A4 has no typed C interface yet */
static inline call_sub_80FF7A4_result call_sub_80FF7A4(u32 r0, u32 r1, u32 r3, u32 r5, u32 r7) {
    R0 = r0;
    R1 = r1;
    R3 = r3;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(aiAttackForwardToType5E_80FF7A4);
    return (call_sub_80FF7A4_result){ R1, R5 };
}

typedef struct { u32 r1, r5; } call_ai_runAttackFromVars_result;
/* register form: generated wrapper: ai_runAttackFromVars has no typed C interface yet */
static inline call_ai_runAttackFromVars_result call_ai_runAttackFromVars(u32 r0, u32 r1, u32 r3, u32 r5, u32 r7) {
    R0 = r0;
    R1 = r1;
    R3 = r3;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(ai_runAttackFromVars);
    return (call_ai_runAttackFromVars_result){ R1, R5 };
}

/* Result of one charged-panel-attack AI step: the values the original
 * leaves behind in r1, r5 and r10. */
struct ai_charge_step_result {
    u32 r1;
    u32 r5;
    u32 r10;
};

/* Charged panel attack, one AI tick.
 *
 * aux   - r3 context passed through to the attack helpers
 * obj   - r5 battle object
 * vars  - r7 AI attack state (oAIAttackVars_*)
 * r10   - r10 context passed through to battle_findForwardUsableAttackPanel
 *
 * When AttackElement is 0 the attack starts: the element is marked as
 * charging (4), a usable panel ahead of the object is picked, the charge
 * pose (anim 0xB / copy 0xFF) is started, related object animations are
 * stopped, sound 0xBF plays, and StepTimer/ChargeCycles are both set to
 * TimerOrCycles + 12.
 *
 * While charging, the step timer counts down.  Hitting ChargeCycles -
 * AttackBoost2 boosts the target panel (aiAttackForwardToType5E_80FF7A4); hitting ChargeCycles -
 * TimerOrCycles fires the attack (ai_runAttackFromVars).  If the timer
 * reaches 0 without either milestone, the attack step index advances; once
 * it reaches SubStepIndex the attack is finished off with HitElement 0x10,
 * otherwise AttackElement is cleared so the next tick charges again. */
static struct ai_charge_step_result ai_charge_step(u32 aux, u32 obj, u32 vars, u32 r10)
{
    struct ai_charge_step_result res;

    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        /* ---- begin a new charge ---- */
        ST8(vars + oAIAttackVars_AttackElement, 4);

        call_battle_findForwardUsableAttackPanel_result panel =
            call_battle_findForwardUsableAttackPanel(obj);
        obj = panel.r5;

        if (panel.r0 == 0) {
            /* Nothing usable ahead: flag a plain hit instead. */
            ST16(vars + oAIAttackVars_HitElement, 0x10);
            res.r1 = panel.r1;
            res.r5 = obj;
            res.r10 = r10;
            return res;
        }

        ST8(vars + oAIAttackVars_TargetPanelX, panel.r0);
        ST8(vars + oAIAttackVars_TargetPanelY, panel.r1);

        /* Charge pose on the object, and stop related object animations. */
        ST8(obj + oBattleObject_CurAnim, 0xB);
        ST8(obj + oBattleObject_CurAnimCopy, 0xFF);

        call_battle_endRelatedObjectAnims_result ended =
            call_battle_endRelatedObjectAnims(obj);
        obj = ended.r5;

        call_PlaySoundEffect_result_ai_chargedPanelAttack sound = call_PlaySoundEffect_ai_chargedPanelAttack(0xBF, ended.r2);
        obj = sound.r5;
        vars = sound.r7;
        r10 = sound.r10;

        /* Both the step timer and the charge cycle counter get the full
         * charge time: the base value plus 12 ticks. */
        u32 charge_time = LD8(vars + oAIAttackVars_TimerOrCycles) + 0xC;
        ST16(vars + oAIAttackVars_StepTimer, charge_time);
        ST16(vars + oAIAttackVars_ChargeCycles, charge_time);

        res.r1 = sound.r1;
        res.r5 = obj;
        res.r10 = r10;
        return res;
    }

    /* ---- charge in progress: count the step timer down ---- */
    u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);

    /* Milestone 1: boost the target panel one cycle before the shot. */
    u32 boost_time = LD16(vars + oAIAttackVars_ChargeCycles)
                   - LD8(vars + oAIAttackVars_AttackBoost2);
    if (timer == boost_time) {
        call_sub_80FF7A4_result boost = call_sub_80FF7A4(
            LD8(vars + oAIAttackVars_TargetPanelX),
            LD8(vars + oAIAttackVars_TargetPanelY),
            aux, obj, vars);
        res.r1 = boost.r1;
        res.r5 = boost.r5;
        res.r10 = r10;
        return res;
    }

    /* Milestone 2: charge complete, fire the attack on the target panel. */
    u32 fire_time = LD16(vars + oAIAttackVars_ChargeCycles)
                  - LD8(vars + oAIAttackVars_TimerOrCycles);
    if (timer == fire_time) {
        call_ai_runAttackFromVars_result attack = call_ai_runAttackFromVars(
            LD8(vars + oAIAttackVars_TargetPanelX),
            LD8(vars + oAIAttackVars_TargetPanelY),
            aux, obj, vars);
        res.r1 = attack.r1;
        res.r5 = attack.r5;
        res.r10 = r10;
        return res;
    }

    /* Timer still counting down - nothing else happens this tick. */
    if ((s32)timer > 0) {
        res.r1 = fire_time;
        res.r5 = obj;
        res.r10 = r10;
        return res;
    }

    /* Timer ran out: move on to the next attack step. */
    u32 step = LD8(vars + oAIAttackVars_AttackStepIndex) + 1;
    ST8(vars + oAIAttackVars_AttackStepIndex, step);

    u32 substeps = LD8(vars + oAIAttackVars_SubStepIndex);
    if ((s32)step >= (s32)substeps) {
        /* All sub-steps done: flag a plain hit and finish. */
        ST16(vars + oAIAttackVars_HitElement, 0x10);
    } else {
        /* More sub-steps queued: clear the element so the next tick
         * starts a fresh charge. */
        ST8(vars + oAIAttackVars_AttackElement, 0);
    }
    res.r1 = substeps;
    res.r5 = obj;
    res.r10 = r10;
    return res;
}

/* ai_chargedPanelAttack (0x080ff1a2): r3 r5 r7 r10 in, r1 r5 r10 out. */
Next ai_chargedPanelAttack(void) {
    struct ai_charge_step_result res = ai_charge_step(R3, R5, R7, R10);
    R1 = res.r1;
    R5 = res.r5;
    R10 = res.r10;
    RETURN_TO_CALLER();
}

/* ── battle_endRelatedObjectAnims ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_battle_EndObjectCurrentAnim_80C44D2_result;
/* register form: generated wrapper: battle_EndObjectCurrentAnim_80C44D2 has no typed C interface yet */
static inline call_battle_EndObjectCurrentAnim_80C44D2_result call_battle_EndObjectCurrentAnim_80C44D2(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_EndObjectCurrentAnim_80C44D2);
    return (call_battle_EndObjectCurrentAnim_80C44D2_result){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_endRelatedObjectAnims: end the current animation of the battle object's
 * related object and of its "extra vars" object, skipping either one when its
 * pointer is NULL. r5 on entry is the battle object.
 *
 * Outputs r1-r5: battle_EndObjectCurrentAnim_80C44D2 leaves its own r1-r5 behind,
 * and that is what callers of this routine see; when no call runs, r1-r5 pass
 * through unchanged (r4/r5 are preserved on every path). */
typedef struct {
    u32 r1, r2, r3, r4, r5;
} EndRelatedAnimsResult;

static EndRelatedAnimsResult battle_end_related_object_anims(u32 obj, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    EndRelatedAnimsResult out = { r1, r2, r3, r4, r5 };

    u32 related = LD32(obj + oBattleObject_RelatedObject2Ptr);
    if (related != 0) {
        call_battle_EndObjectCurrentAnim_80C44D2_result res = call_battle_EndObjectCurrentAnim_80C44D2(related);
        out.r1 = res.r1;
        out.r2 = res.r2;
        out.r3 = res.r3;
        out.r4 = res.r4;
        out.r5 = res.r5;
    }

    u32 extra = LD32(obj + oBattleObject_ExtraVars);
    if (extra != 0) {
        call_battle_EndObjectCurrentAnim_80C44D2_result res = call_battle_EndObjectCurrentAnim_80C44D2(extra);
        out.r1 = res.r1;
        out.r2 = res.r2;
        out.r3 = res.r3;
        out.r4 = res.r4;
        out.r5 = res.r5;
    }

    return out;
}

/* inputs: r5 (battle object), lr; outputs: r1 r2 r3 r4 r5; clobbers: nz c v r0 lr */
Next battle_endRelatedObjectAnims(void) {
    EndRelatedAnimsResult out = battle_end_related_object_anims(R5, R1, R2, R3, R4, R5);
    R1 = out.r1;
    R2 = out.r2;
    R3 = out.r3;
    R4 = out.r4;
    R5 = out.r5;
    RETURN_TO_CALLER();
}

/* ── aiAttack_runStepTwice_80FF680 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r5; } call_sub_80D01C8_result;
/* register form: generated wrapper: object_spawnElemWithParent has no typed C interface yet */
static inline call_sub_80D01C8_result call_sub_80D01C8(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_spawnElemWithParent);
    return (call_sub_80D01C8_result){ R1, R5 };
}

/* AI attack step executor: feeds the shared attack runner (object_spawnElemWithParent) twice.
 *
 * Both passes read the fields of the AI attack variable block (vars) fresh,
 * so any changes object_spawnElemWithParent makes to the block during pass 1 are picked up
 * by pass 2, exactly as in the original:
 *   r0 = AttackStepIndex, r1 = 2 (command type), r2 = InitFlag,
 *   r3 = TargetObjectPtr, r4 = AiDifficultyTier with a mode bit OR-ed into
 *   the upper byte (0 for pass 1, 0x100 for pass 2), r5 = the caller's r5
 *   passed through, r6 = Damage, r7 = a data word taken from the block:
 *   ScratchVar on pass 1, AiAttackParam on pass 2.
 * The runner's r1/r5 results after pass 2 are this function's outputs; the
 * pass 1 results are simply overwritten.
 */
static call_sub_80D01C8_result run_attack_step_sub_80FF680(u32 vars, u32 pass_r5, u32 mode_bit, u32 step_word) {
    u32 step_index = LD8(vars + oAIAttackVars_AttackStepIndex);
    u32 init_flag  = LD8(vars + oAIAttackVars_InitFlag);
    u32 tier       = LD8(vars + oAIAttackVars_AiDifficultyTier) | mode_bit;
    u32 target     = LD32(vars + oAIAttackVars_TargetObjectPtr);
    u32 damage     = LD32(vars + oAIAttackVars_Damage);

    return call_sub_80D01C8(step_index, 2, init_flag, target, tier, pass_r5, damage, step_word);
}

static call_sub_80D01C8_result run_attack_steps(u32 vars, u32 pass_r5) {
    /* Pass 1: step data comes from the scratch word. */
    run_attack_step_sub_80FF680(vars, pass_r5, 0x000, LD32(vars + oAIAttackVars_ScratchVar));

    /* Pass 2: step data comes from the AI attack parameter word. */
    return run_attack_step_sub_80FF680(vars, pass_r5, 0x100, LD32(vars + oAIAttackVars_AiAttackParam));
}

/* aiAttack_runStepTwice_80FF680(r5 = pass-through, r7 = AI attack vars) -> r1, r5 */
Next aiAttack_runStepTwice_80FF680(void) {
    u32 vars = R7, pass_r5 = R5, saved_r4 = R4, saved_r6 = R6;
    call_sub_80D01C8_result res = run_attack_steps(vars, pass_r5);
    R1 = res.r1;
    R5 = res.r5;
    /* The original pushes r4, r6 and r7 around the calls and pops them on
     * the way out, so these preserved registers must end as they began.
     * The wrapper call leaves object_spawnElemWithParent's own register values behind
     * (it clobbers r4/r6/r7), so restore the entry values here. */
    R4 = saved_r4;
    R6 = saved_r6;
    R7 = vars;
    RETURN_TO_CALLER();
}

/* ── battle_findForwardUsableAttackPanel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r4, r5; } call_battle_findPlayer_result_battle_findForwardUsableAttackPanel;
/* register form: generated wrapper: battle_findPlayer has no typed C interface yet */
static inline call_battle_findPlayer_result_battle_findForwardUsableAttackPanel call_battle_findPlayer_battle_findForwardUsableAttackPanel(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_findPlayer);
    return (call_battle_findPlayer_result_battle_findForwardUsableAttackPanel){ R0, R4, R5 };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5; } call_object_getClosestPanelMatchingRowFiltered_result;
/* register form: generated wrapper: object_getClosestPanelMatchingRowFiltered has no typed C interface yet */
static inline call_object_getClosestPanelMatchingRowFiltered_result call_object_getClosestPanelMatchingRowFiltered(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_getClosestPanelMatchingRowFiltered);
    return (call_object_getClosestPanelMatchingRowFiltered_result){ R0, R1, R2, R3, R4, R5 };
}

typedef struct { u32 r0; } call_object_getFrontDirection_result_battle_findForwardUsableAttackPanel;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battle_findForwardUsableAttackPanel call_object_getFrontDirection_battle_findForwardUsableAttackPanel(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battle_findForwardUsableAttackPanel){ R0 };
}

typedef struct { u32 r0, r2, r3, r4, r5; bool c, v; } call_object_checkPanelParameters_result_battle_findForwardUsableAttackPanel;
/* register form: generated wrapper: object_checkPanelParameters has no typed C interface yet */
static inline call_object_checkPanelParameters_result_battle_findForwardUsableAttackPanel call_object_checkPanelParameters_battle_findForwardUsableAttackPanel(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_checkPanelParameters);
    return (call_object_checkPanelParameters_result_battle_findForwardUsableAttackPanel){ R0, R2, R3, R4, R5, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Find the closest usable panel directly in front of the attacker that the
 * opponent occupies. Two-stage search: first the opponent's current row/panel,
 * then a dedicated attack-panel filter; adjacent rows must be clear.
 * Returns (panel_x, panel_y, r2, self).  r0=0 means a usable panel was found. */

typedef struct {
    u32 panel_x;   /* output r0 (0 = success) */
    u32 panel_y;   /* output r1 */
    u32 extra_r2;  /* output r2 – own panel X or callee data */
    u32 self;      /* output r5 – passed through unchanged */
} ForwardPanelResult;

static ForwardPanelResult find_forward_usable_attack_panel(u32 self) {
    u32 alliance = LD8(self + oBattleObject_Alliance);

    /* Opponent's BattleObject → their current row. */
    u32 opponent = call_battle_findPlayer_battle_findForwardUsableAttackPanel(alliance ^ 1).r0;
    u32 opponent_row = LD8(opponent + oBattleObject_PanelY);

    /* --- First search: standard filter table (byte_80FF780) --- */
    u32 f1 = A_byte_80FF780 + (alliance << 3);
    call_object_getClosestPanelMatchingRowFiltered_result s1 =
        call_object_getClosestPanelMatchingRowFiltered(
            alliance, opponent_row, LD32(f1), LD32(f1 + 4));

    if (s1.r0 != 0) {
        u32 self_x = LD8(self + oBattleObject_PanelX);
        u32 self_y = LD8(self + oBattleObject_PanelY);

        /* Panel must be in our row. */
        if (s1.r1 != self_y)
            return (ForwardPanelResult){ s1.r0, s1.r1, self_x, self };

        /* Panel must be behind us (offset × front-direction ≤ 0). */
        u32 dx = s1.r0 - self_x;
        u32 front = call_object_getFrontDirection_battle_findForwardUsableAttackPanel(self).r0;
        if ((s32)(dx * front) > 0)
            return (ForwardPanelResult){ s1.r0, s1.r1, self_x, self };
    }

    /* --- Second search: attack-panel filter table (byte_80FF794) --- */
    u32 f2 = A_byte_80FF794 + (alliance << 3);
    call_object_getClosestPanelMatchingRowFiltered_result s2 =
        call_object_getClosestPanelMatchingRowFiltered(
            alliance, opponent_row, LD32(f2), LD32(f2 + 4));

    if (s2.r0 == 0)
        return (ForwardPanelResult){ 0, s2.r1, s2.r2, self };

    u32 panel_x = s2.r0;
    u32 panel_y = s2.r1;

    /* Check both adjacent rows (±1) using the standard filter. */
    u32 fc = A_byte_80FF780 + (alliance << 3);
    u32 cf2 = LD32(fc);
    u32 cf3 = LD32(fc + 4);

    call_object_checkPanelParameters_result_battle_findForwardUsableAttackPanel chk_up =
        call_object_checkPanelParameters_battle_findForwardUsableAttackPanel(panel_x, panel_y + 1, cf2, cf3);
    if (chk_up.r0 != 0)
        return (ForwardPanelResult){ panel_x, panel_y + 1, chk_up.r2, self };

    /* Second check: the original re-reads r4/r6 after the first callee.
     * checkPanelParameters outputs r4 (and preserves r6 = panel_y). */
    call_object_checkPanelParameters_result_battle_findForwardUsableAttackPanel chk_dn =
        call_object_checkPanelParameters_battle_findForwardUsableAttackPanel(chk_up.r4, panel_y - 1, cf2, cf3);
    if (chk_dn.r0 != 0)
        return (ForwardPanelResult){ chk_up.r4, panel_y - 1, chk_dn.r2, self };

    /* Both adjacent rows clear → usable attack panel found. */
    return (ForwardPanelResult){ 0, panel_y - 1, chk_dn.r2, self };
}

/* battle_findForwardUsableAttackPanel(r5 = self, r10, lr) -> r0 r1 r2 r5 */
Next battle_findForwardUsableAttackPanel(void) {
    ForwardPanelResult res = find_forward_usable_attack_panel(R5);
    R0 = res.panel_x;
    R1 = res.panel_y;
    R2 = res.extra_r2;
    R5 = res.self;
    RETURN_TO_CALLER();
}

/* ── aiAttackForwardToType5E_80FF7A4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r5; } call_sub_80D036C_result;
/* register form: generated wrapper: spawnType5ELinkedToObject has no typed C interface yet */
static inline call_sub_80D036C_result call_sub_80D036C(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(spawnType5ELinkedToObject);
    return (call_sub_80D036C_result){ R1, R5 };
}

/* AI attack-state helper: read the attack's init flag and cycle timer from the
 * AI attack-vars block (r7) and hand them, together with the caller's other
 * arguments, to spawnType5ELinkedToObject.  Its results in r1 and r5 (the updated attack
 * fields) are this function's outputs.
 *
 * Note on r4: the original wraps the call in push {r4} / pop {r4}, because
 * spawnType5ELinkedToObject runs as guest code and clobbers the register.  The adapter saves
 * and restores it around the call so the preserved-register contract holds. */
static call_sub_80D036C_result forward_attack_update(u32 r0, u32 r1, u32 r3, u32 r5, u32 attack_vars)
{
    u32 init_flag = LD8(attack_vars + oAIAttackVars_InitFlag);
    u32 timer     = LD8(attack_vars + oAIAttackVars_TimerOrCycles);

    return call_sub_80D036C(r0, r1, init_flag, r3, timer, r5);
}

Next aiAttackForwardToType5E_80FF7A4(void)
{
    u32 saved_r4 = R4;   /* spawnType5ELinkedToObject clobbers r4; original push/pop preserves it */
    call_sub_80D036C_result res = forward_attack_update(R0, R1, R3, R5, R7);
    R4 = saved_r4;
    R1 = res.r1;
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── ai_runAttackFromVars ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r5; } call_sub_80D04DA_result;
/* register form: generated wrapper: battle_spawnObject5FAtPanel_80D04DA has no typed C interface yet */
static inline call_sub_80D04DA_result call_sub_80D04DA(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    GUEST_CALL(battle_spawnObject5FAtPanel_80D04DA);
    return (call_sub_80D04DA_result){ R1, R5 };
}

/* Pull the AI attack-vars (init flag, difficulty tier, damage) out of the
 * block pointed to by r7, then forward everything through to the attack
 * runner battle_spawnObject5FAtPanel_80D04DA, which returns the updated turn state in r1 and r5. */
static call_sub_80D04DA_result run_attack_from_vars(u32 arg0, u32 arg1,
                                                     u32 arg3, u32 arg5,
                                                     u32 vars)
{
    u32 initFlag       = LD8(vars + oAIAttackVars_InitFlag);
    u32 difficultyTier = LD8(vars + oAIAttackVars_AiDifficultyTier);
    u32 damage         = LD32(vars + oAIAttackVars_Damage);

    return call_sub_80D04DA(arg0, arg1, initFlag, arg3,
                            difficultyTier, arg5, damage);
}

/* ai_runAttackFromVars(r0, r1, r3, r5, r7) -> r1, r5; r4/r6 preserved */
Next ai_runAttackFromVars(void) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;
    call_sub_80D04DA_result result = run_attack_from_vars(R0, R1, R3, R5, R7);
    R1 = result.r1;
    R5 = result.r5;
    R4 = saved_r4;   /* original pushed/popped these around the call */
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── stub_noop_80FFA1A ── */
/* stub_noop_80FFA1A: an empty stub — it returns immediately to its caller
 * (push {lr} / pop {pc}, i.e. a plain return with no effect). */

static void sub_80FFA1A_body(void) {
    /* nothing happens */
}

/* stub_noop_80FFA1A() -> no outputs */
Next stub_noop_80FFA1A(void) {
    sub_80FFA1A_body();
    RETURN_TO_CALLER();
}

/* ── sub_80FFA54 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80FFA54;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80FFA54 call_sub_800FE28_sub_80FFA54(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80FFA54){ R0, R2 };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag_sub_80FFA54(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

/* sub_800FE28 computes a base value from the data table at dword_80FFA74
 * (palette-like entries 0x12,0x18,0x1E,0x24) and the object in r5.
 * If the object's flag has bits 13 or 15 set (0xA000), 60 is added.
 * Returns the adjusted value in r0. */
static u32 sub_80FFA54_typed(u32 object) {
    u32 value = call_sub_800FE28_sub_80FFA54(A_dword_80FFA74, object).r0;

    if (call_object_getFlag_sub_80FFA54(object) & 0xA000)
        value += 0x3C;

    return value;
}

/* sub_80FFA54(r5 = object) -> r0 */
Next sub_80FFA54(void) {
    R0 = sub_80FFA54_typed(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80FFA78 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80FFA78;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80FFA78 call_sub_800FE28_sub_80FFA78(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80FFA78){ R0, R2 };
}

/* sub_80FFA78: tail helper that hands the constant table at dword_80FFA88
 * (a 0xC101216 word) to sub_800FE28 along with the caller's r5, and passes
 * the callee's r0 back to the caller. r2 is also left as sub_800FE28 set it,
 * but r2 is a clobber for every caller, so it is not carried through. */
static u32 sub_80FFA78_body(u32 r5) {
    return call_sub_800FE28_sub_80FFA78(A_dword_80FFA88, r5).r0;
}

/* sub_80FFA78(r5) -> r0 */
Next sub_80FFA78(void) {
    R0 = sub_80FFA78_body(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80FFA8C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80FFA8C;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80FFA8C call_sub_800FE28_sub_80FFA8C(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80FFA8C){ R0, R2 };
}

/* sub_80FFA8C: hands sub_800FE28 the address of a small 4-entry data table
 * (byte_80FFA9C) together with the caller's r5, and returns its result.
 * r5 is only forwarded; the original lets the callee have it. */
static u32 sub_80FFA8C_body(u32 arg_r5) {
    return call_sub_800FE28_sub_80FFA8C(A_byte_80FFA9C, arg_r5).r0;
}

/* sub_80FFA8C(r5) -> r0 */
Next sub_80FFA8C(void) {
    R0 = sub_80FFA8C_body(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80FFAA0 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80FFAA0;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80FFAA0 call_sub_800FE28_sub_80FFAA0(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80FFAA0){ R0, R2 };
}

/* register form: generated wrapper: object_getFlag has no typed C interface yet */
static inline u32 call_object_getFlag_sub_80FFAA0(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFlag);
    return R0;
}

/* sub_80FFAA0 - look up a value through sub_800FE28 (given the data table at
 * dword_80FFAC0 and the caller's r5), then add 0x3C if the object's 0xA000
 * status flag is set. Returns the resulting value in r0. */
static u32 sub_80FFAA0_body(u32 r5) {
    u32 value = call_sub_800FE28_sub_80FFAA0(A_dword_80FFAC0, r5).r0;

    /* object_getFlag: bit set means Z clear, so a nonzero result adds the bonus */
    if (call_object_getFlag_sub_80FFAA0(r5) & 0xA000u)
        value += 0x3C;

    return value;
}

Next sub_80FFAA0(void) {
    R0 = sub_80FFAA0_body(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80FFAC4 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2; } call_sub_800FE28_result_sub_80FFAC4;
/* register form: generated wrapper: sub_800FE28 has no typed C interface yet */
static inline call_sub_800FE28_result_sub_80FFAC4 call_sub_800FE28_sub_80FFAC4(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(sub_800FE28);
    return (call_sub_800FE28_result_sub_80FFAC4){ R0, R2 };
}

/* Loads the address of the 4-byte value 0x12181E24 (a packed list of four
 * flag/event IDs) and hands it, along with r5, to sub_800FE28, returning
 * that call's result. Effectively "check the flag list at dword_80FFAD4". */

static u32 sub_80FFAC4_body(u32 r5_value) {
    return call_sub_800FE28_sub_80FFAC4(A_dword_80FFAD4, r5_value).r0;
}

/* sub_80FFAC4(r5) -> r0 */
Next sub_80FFAC4(void) {
    R0 = sub_80FFAC4_body(R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectHasExtraVars ── */
/* Does the battle object carry any extra-variables bits?
 * (reads oBattleObject_ExtraVars; yields 1 when nonzero, else 0) */
static u32 battle_object_has_extra_vars(u32 object) {
    return LD32(object + oBattleObject_ExtraVars) != 0;
}

/* battle_objectHasExtraVars(r5 = object) -> r0 */
Next battle_objectHasExtraVars(void) {
    R0 = battle_object_has_extra_vars(R5);
    RETURN_TO_CALLER();
}

/* ── ai_attackDispatchByHitElement_80FFB88 ── */
/* ai_attackDispatchByHitElement_80FFB88 -- AI attack dispatch by hit element.
 *
 * r7 points to the AI attack variables; the HitElement byte selects one of
 * three handler routines through the table at off_80FFB9C:
 *   +0: ai_attackStepReservePanel   +4: ai_attackStrikeStep   +8: ai_attackStepTick_80FFC5E
 * The index is used unscaled (the original's `ldr r1,[r1,r0]` does not shift
 * it), so HitElement already holds a byte offset into the table. The handler
 * is entered with r0 = element and r1 = its own table entry, exactly as the
 * original leaves them after `ldrb r0` / `ldr r1,[r1,r0]`.
 *
 * The original is a tail call: it pushes {r7,lr}, calls the handler with
 * lr = 0x80FFB94 (the following `pop {r7,pc}`), and that pop both restores
 * r7 and jumps straight to this function's caller. So the handler's results
 * in r1 and r5/r8-r12 must flow through untouched, r7 must come back as it
 * came in, and the return goes to the incoming lr. Nothing here touches any
 * of those registers: they still hold their entry values when the guest call
 * runs, and RETURN_TO_CALLER() uses the incoming lr.
 */

/* register form: the callee address comes from the element jump table at
 * run time, so there is no single callee to have a generated wrapper; the
 * call must also run in the guest registers so the handler's r1/r5/r8-r12
 * results are left standing for this function's caller. */
static void dispatch_hit_element_handler_sub_80FFB88(u32 attackVars) {
    u32 element = LD8(attackVars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80FFB9C + element);

    R0 = element;   /* handler argument: the hit element byte */
    R1 = handler;   /* the original still holds the target address in r1 */
    GUEST_CALL_ADDR(handler);
}

/* ai_attackDispatchByHitElement_80FFB88(r4, r5, r6, r7 = AI attack vars, lr) -> r1 r5 r8 r9 r10 r11 r12 */
Next ai_attackDispatchByHitElement_80FFB88(void) {
    dispatch_hit_element_handler_sub_80FFB88(R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackStepReservePanel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r4; bool z, n, c, v; } call_object_canMove_result_ai_attackStepReservePanel;
/* register form: generated wrapper: object_canMove has no typed C interface yet */
static inline call_object_canMove_result_ai_attackStepReservePanel call_object_canMove_ai_attackStepReservePanel(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_canMove);
    return (call_object_canMove_result_ai_attackStepReservePanel){ R1, R4, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r3; bool c, v; } call_object_reservePanel_result_ai_attackStepReservePanel;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_ai_attackStepReservePanel call_object_reservePanel_ai_attackStepReservePanel(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_ai_attackStepReservePanel){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_attackStepReservePanel;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_attackStepReservePanel call_object_setFlag1_ai_attackStepReservePanel(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_attackStepReservePanel){ R1, R2 };
}

/* Result of the indirect panel-choose callback (X coordinate, Y coordinate). */
typedef struct { u32 panel_x, panel_y; } panel_choose_result;

/*
 * Call the panel-choose callback through its runtime function pointer.
 * register form: indirect call via GUEST_CALL_ADDR on the callback address;
 * after the call R0 holds the X coordinate and R1 holds the Y coordinate.
 */
static panel_choose_result call_panel_choose_callback_ai_attackStepReservePanel(u32 target, u32 callback, u32 tbl0, u32 tbl1) {
    panel_choose_result res;
    R0 = target;
    R1 = callback;
    R2 = tbl0;
    R3 = tbl1;
    GUEST_CALL_ADDR(callback);
    res.panel_x = R0;
    res.panel_y = R1;
    return res;
}

/*
 * AI attack step: reserve a panel for the attacker.
 *
 * On the first call (AttackElement == 0) the step initializes the element,
 * asks object_canMove whether the attacker can slide, and if so uses an
 * alliance-keyed table plus a PanelChooseCallback to pick a target panel.
 * The chosen panel is stored and reserved, sliding is disabled, and the
 * attack animation (CurAnim 4) is started with a 2-frame step timer.
 *
 * On subsequent calls the step timer is decremented; when it hits zero or
 * below, HitElement is set to 4 (fire the attack hitbox).
 *
 * r4, r6, lr are unused inputs kept for convention; r5 is the attacker's
 * battle object, r7 is the AI attack variables struct.
 */
static void ai_attack_step_reserve_panel(u32 r4, u32 r5, u32 r6, u32 r7, u32 lr) {
    (void)r4; (void)r6; (void)lr;

    if (LD8(r7 + oAIAttackVars_AttackElement) != 0) {
        /* Timer is active: count it down each frame. */
        s32 timer = (s32)LD16(r7 + oAIAttackVars_StepTimer) - 1;
        ST16(r7 + oAIAttackVars_StepTimer, (u32)timer);
        if (timer > 0)
            return;
        /* Timer expired: mark the hit element. */
        ST16(r7 + oAIAttackVars_HitElement, 4);
        return;
    }

    /* First invocation: initialize the attack element. */
    ST8(r7 + oAIAttackVars_AttackElement, 4);

    if (call_object_canMove_ai_attackStepReservePanel(r5).z) {
        /* Cannot move: abandon with a long duration and hit element 8. */
        ST16(r7 + oAIAttackVars_AttackDuration, 0x3C);
        ST16(r7 + oAIAttackVars_HitElement, 8);
        return;
    }

    /* Look up the panel-search range for this alliance (two u32 per entry). */
    u32 alliance = LD8(r5 + oBattleObject_Alliance);
    u32 entry = A_byte_80FFC08 + (alliance << 3);
    u32 tbl0 = LD32(entry);
    u32 tbl1 = LD32(entry + 4);

    /* Invoke the panel-choose callback with target object and range info. */
    u32 target = LD32(r7 + oAIAttackVars_TargetObjectPtr);
    u32 callback = LD32(r7 + oAIAttackVars_PanelChooseCallback);
    panel_choose_result pick = call_panel_choose_callback_ai_attackStepReservePanel(target, callback, tbl0, tbl1);

    if (pick.panel_x == 0) {
        /* Callback found no valid panel: abandon. */
        ST16(r7 + oAIAttackVars_AttackDuration, 0x3C);
        ST16(r7 + oAIAttackVars_HitElement, 8);
        return;
    }

    /* Store the chosen panel and reserve it for the attacker. */
    ST8(r5 + oBattleObject_FuturePanelX, pick.panel_x);
    ST8(r5 + oBattleObject_FuturePanelY, pick.panel_y);
    call_object_reservePanel_ai_attackStepReservePanel(pick.panel_x, pick.panel_y, r5);

    /* Disable sliding, begin attack animation, set 2-frame step timer. */
    call_object_setFlag1_ai_attackStepReservePanel(OBJECT_FLAGS_CANNOT_SLIDE, r5);
    ST8(r5 + oBattleObject_CurAnim, 4);
    ST16(r7 + oAIAttackVars_StepTimer, 2);
}

Next ai_attackStepReservePanel(void) {
    ai_attack_step_reserve_panel(R4, R5, R6, R7, LR);
    RETURN_TO_CALLER();
}

/* ── ai_attackStrikeStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r3; } call_object_removePanelReserve_result_ai_attackStrikeStep;
/* register form: generated wrapper: object_removePanelReserve has no typed C interface yet */
static inline call_object_removePanelReserve_result_ai_attackStrikeStep call_object_removePanelReserve_ai_attackStrikeStep(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_removePanelReserve);
    return (call_object_removePanelReserve_result_ai_attackStrikeStep){ R3 };
}

/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline void call_object_setCoordinatesFromPanels_ai_attackStrikeStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
}

typedef struct { u32 r3, r4, r6; bool c, v; } call_object_updateCollisionPanels_result_ai_attackStrikeStep;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_ai_attackStrikeStep call_object_updateCollisionPanels_ai_attackStrikeStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_ai_attackStrikeStep){ R3, R4, R6, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: generated wrapper: object_clearFlag has no typed C interface yet */
static inline void call_object_clearFlag_ai_attackStrikeStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_clearFlag);
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_attackStrikeStep;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_attackStrikeStep call_object_setFlag1_ai_attackStrikeStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_attackStrikeStep){ R1, R2 };
}

/* ai_attackStrikeStep — one AI step of the "strike" attack.
 *
 * r5 = battle object, r7 = the object's AI attack variables.
 *
 * While AttackElement is still 0 the strike has not begun: pick the strike
 * element, play the attack animation, move the object onto the panel that was
 * reserved in advance (freeing the reservation), refresh its coordinates and
 * collision panels, mark it as currently moving, and start a 2-frame step
 * timer.  Afterwards the step timer counts down; when it reaches zero the hit
 * element is recorded so the strike connects on the next damage step.
 *
 * r1 on exit is simply whatever object_setFlag1 left behind on the frames
 * where it runs (the original never computes an r1 result itself); on the
 * countdown frames r1 is untouched, exactly as in the original. */
#define AI_STRIKE_ELEMENT       4u  /* AttackElement value while striking  */
#define AI_STRIKE_START_TIMER   2u  /* initial StepTimer after starting    */
#define AI_STRIKE_HIT_ELEMENT   8u  /* HitElement stored when timer lapses */

static void ai_attack_strike_step(u32 obj, u32 vars) {
    u32 element = LD8(vars + oAIAttackVars_AttackElement);

    if (element != 0) {
        /* Strike already underway: tick the timer down, and once it expires
           record the hit element (bgt: keep waiting only while it is > 0). */
        u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
        ST16(vars + oAIAttackVars_StepTimer, timer);
        if ((s32)timer > 0)
            return;
        ST16(vars + oAIAttackVars_HitElement, AI_STRIKE_HIT_ELEMENT);
        return;
    }

    /* Begin the strike: switch element and animation, then step onto the
       panel reserved on the previous frames. */
    ST8(vars + oAIAttackVars_AttackElement, AI_STRIKE_ELEMENT);
    ST8(obj + oBattleObject_CurAnim, 3);

    u32 nextX = LD8(obj + oBattleObject_FuturePanelX);
    u32 nextY = LD8(obj + oBattleObject_FuturePanelY);
    ST8(obj + oBattleObject_PanelX, nextX);
    ST8(obj + oBattleObject_PanelY, nextY);

    call_object_removePanelReserve_ai_attackStrikeStep(nextX, nextY, obj);
    call_object_setCoordinatesFromPanels_ai_attackStrikeStep(obj);
    call_object_updateCollisionPanels_ai_attackStrikeStep(obj);
    call_object_clearFlag_ai_attackStrikeStep(0x40, obj);
    call_object_setFlag1_ai_attackStrikeStep(1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT, obj);

    ST16(vars + oAIAttackVars_StepTimer, AI_STRIKE_START_TIMER);
}

/* inputs: r5 r7 lr; outputs: r1 (via the object_setFlag1 callee on the
 * start path); clobbers: nz c v r0 r2 r3 lr; preserved: r4 r6 */
Next ai_attackStrikeStep(void) {
    ai_attack_strike_step(R5, R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackStepTick_80FFC5E ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_sub_80FFC5E;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_sub_80FFC5E call_object_exitAttackState_sub_80FFC5E(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_sub_80FFC5E){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Advance one AI attack step for an object.
 *
 * r7 points at the object's AI attack vars, r5 at the battle object, r6 is a
 * companion pointer passed through to the exit routine.
 *
 * If no attack element is queued yet, start one: set the element to 4, put the
 * object into animation 0 and load the step timer from the attack duration.
 * Otherwise count the step timer down; when it reaches zero, hand the attack
 * state back to object_exitAttackState, which also produces the r1 result.
 * On the paths that skip the call, r1 passes through unchanged.
 */
static u32 ai_attack_step_sub_80FFC5E(u32 object, u32 companion, u32 vars, u32 out_r1) {
    u32 element = LD8(vars + oAIAttackVars_AttackElement);

    if (element == 0) {
        /* No attack queued: begin a new one. */
        ST8(vars + oAIAttackVars_AttackElement, 4);
        ST8(object + oBattleObject_CurAnim, 0);
        ST16(vars + oAIAttackVars_StepTimer,
             LD16(vars + oAIAttackVars_AttackDuration));
        return out_r1;
    }

    /* Attack in progress: tick the timer and exit the attack state at zero. */
    u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);

    if ((s32)timer > 0)
        return out_r1;

    return call_object_exitAttackState_sub_80FFC5E(object, companion, vars).r1;
}

/* ai_attackStepTick_80FFC5E(r5 = object, r6 = companion, r7 = AI attack vars) -> r1 */
Next ai_attackStepTick_80FFC5E(void) {
    R1 = ai_attack_step_sub_80FFC5E(R5, R6, R7, R1);
    RETURN_TO_CALLER();
}

/* ── ai_dispatchElementAttackStep_80FFC82 ── */
/* AI attack dispatcher (ai_dispatchElementAttackStep_80FFC82).
 *
 * r7 points at the AI's attack variables; the hitElement byte stored there
 * selects one of five attack step routines from the jump table at
 * off_80FFC94:
 *   0 ai_panelTargetAttackStep   1 sub_80FFD20      2 sub_80FFD6A
 *   3 ai_chargeShotAttackStep    4 ai_chipAttackTick
 * The chosen step runs with the element in r0, its own table entry in r1 and
 * the rest of the incoming state (r2 r3 r5 r6 r7 r10) unchanged. r7 is
 * preserved for the caller: the original pushes it before the call and pops
 * it afterwards. */

/* register form: the step routine is picked at run time through the game's
 * jump table, so it can only be invoked by address. The pass-through state
 * (r2 r3 r5 r6 r7 r10) is set explicitly so the step sees exactly what the
 * original would have handed it, and r7 is restored after the call just as
 * the original's pop {r7} does. */
static void dispatch_attack_step_sub_80FFC82(u32 element, u32 hitVars, u32 r2, u32 r3,
                                 u32 r5, u32 r6, u32 r10) {
    u32 target = LD32(A_off_80FFC94 + element);

    R0 = element;
    R1 = target;
    R2 = r2;
    R3 = r3;
    R5 = r5;
    R6 = r6;
    R7 = hitVars;
    R10 = r10;
    GUEST_CALL_ADDR(target);
    R7 = hitVars;   /* pop {r7}: hand the caller its r7 back */
}

/* Look up the step for the current hit element and run it. */
static void element_attack_step(u32 hitVars, u32 r2, u32 r3, u32 r5, u32 r6, u32 r10) {
    u32 element = LD8(hitVars + oAIAttackVars_HitElement);
    dispatch_attack_step_sub_80FFC82(element, hitVars, r2, r3, r5, r6, r10);
}

/* ai_dispatchElementAttackStep_80FFC82(r2, r3, r5, r6, r7 = AIAttackVars, r10) -> r1 r5 r8 r9 r10 r11 r12
 * All outputs are whatever the dispatched step leaves behind. */
Next ai_dispatchElementAttackStep_80FFC82(void) {
    element_attack_step(R7, R2, R3, R5, R6, R10);
    RETURN_TO_CALLER();
}

/* ── ai_panelTargetAttackStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4; bool z, n; } call_object_canMove_result_ai_panelTargetAttackStep;
/* register form: generated wrapper: object_canMove has no typed C interface yet */
static inline call_object_canMove_result_ai_panelTargetAttackStep call_object_canMove_ai_panelTargetAttackStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_canMove);
    return (call_object_canMove_result_ai_panelTargetAttackStep){ R4, ZERO_FLAG(), NEGATIVE_FLAG() };
}

typedef struct { u32 r3; bool c, v; } call_object_reservePanel_result_ai_panelTargetAttackStep;
/* register form: generated wrapper: object_reservePanel has no typed C interface yet */
static inline call_object_reservePanel_result_ai_panelTargetAttackStep call_object_reservePanel_ai_panelTargetAttackStep(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_reservePanel);
    return (call_object_reservePanel_result_ai_panelTargetAttackStep){ R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2; } call_object_setFlag1_result_ai_panelTargetAttackStep;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_panelTargetAttackStep call_object_setFlag1_ai_panelTargetAttackStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_panelTargetAttackStep){ R1, R2 };
}

typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_ai_panelTargetAttackStep;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_ai_panelTargetAttackStep call_object_exitAttackState_ai_panelTargetAttackStep(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_ai_panelTargetAttackStep){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* register form: the alliance's panel-choose routine is reached through
 * `bx r1` with the address loaded from oAIAttackVars_PanelChooseCallback,
 * so no generated wrapper covers it. It is called as
 *   r0 = target object, r1 = the callback address itself,
 *   r2/r3 = the alliance's panel-table entry,
 * and it returns the chosen panel in r0 (x coordinate) and r1 (y coordinate),
 * so both registers must be read back after the call. */
static void call_panel_choose_callback(u32 callback, u32 target,
                                       u32 entry_lo, u32 entry_hi,
                                       u32 *out_x, u32 *out_y) {
    R0 = target;
    R1 = callback;
    R2 = entry_lo;
    R3 = entry_hi;
    GUEST_CALL_ADDR(callback);
    *out_x = R0;
    *out_y = R1;
}

/* One step of the AI's "walk to the target's panel and strike" attack.
 *
 * r5 = battle object, r6 = attack phase argument, r7 = AI attack variables.
 *
 * First invocation (AttackElement == 0): latch the element id, make sure the
 * object is able to move, then ask the alliance's panel-choose callback for a
 * destination panel. If one is chosen, store it as the future panel, reserve
 * it, forbid sliding and start the wind-up animation with a 2-frame timer.
 * If the object cannot move or no panel was chosen, abandon the attack.
 * Later invocations (AttackElement != 0): tick the step timer down once per
 * call; when it runs out, record the hit element so the attack itself fires
 * on the next step. */
static void ai_panel_target_attack_step(u32 object, u32 phase, u32 vars) {
    if (LD8(vars + oAIAttackVars_AttackElement) != 0) {
        /* Wind-up in progress: decrement the timer; the original compares the
         * full 32-bit decremented value with bgt, so only a timer of 0 or 1
         * counts as expired. */
        u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
        ST16(vars + oAIAttackVars_StepTimer, timer);
        if ((s32)timer > 0)
            return;
        ST16(vars + oAIAttackVars_HitElement, 4);
        return;
    }

    /* First activation: remember the element so later steps take the timer
     * path above. */
    ST8(vars + oAIAttackVars_AttackElement, 4);

    if (call_object_canMove_ai_panelTargetAttackStep(object).z) {
        /* Object cannot move: give up and leave the attack state. */
        ST8(vars + oAIAttackVars_HasMoved, 0);
        call_object_exitAttackState_ai_panelTargetAttackStep(object, phase, vars);
        return;
    }

    /* The panel-choose routine descriptors are looked up per alliance
     * (one 8-byte entry each: a pair of words passed in r2/r3). */
    u32 entry = A_byte_80FFD10 + (LD8(object + oBattleObject_Alliance) << 3);
    u32 entry_lo = LD32(entry);
    u32 entry_hi = LD32(entry + 4);

    u32 pick_x, pick_y;
    call_panel_choose_callback(LD32(vars + oAIAttackVars_PanelChooseCallback),
                               LD32(vars + oAIAttackVars_TargetObjectPtr),
                               entry_lo, entry_hi, &pick_x, &pick_y);

    if (pick_x == 0) {
        /* Callback found no usable panel: give up and leave the attack state. */
        ST8(vars + oAIAttackVars_HasMoved, 0);
        call_object_exitAttackState_ai_panelTargetAttackStep(object, phase, vars);
        return;
    }

    /* Move to the chosen panel and lock the object in place. */
    ST8(object + oBattleObject_FuturePanelX, pick_x);
    ST8(object + oBattleObject_FuturePanelY, pick_y);
    call_object_reservePanel_ai_panelTargetAttackStep(pick_x, pick_y, object);
    call_object_setFlag1_ai_panelTargetAttackStep(OBJECT_FLAGS_CANNOT_SLIDE, object);

    /* Begin the wind-up: attack animation, clear scratch, mark as moved and
     * start the 2-frame step timer. */
    ST8(object + oBattleObject_CurAnim, 4);
    ST32(vars + oAIAttackVars_ScratchVar, 0);
    ST8(vars + oAIAttackVars_HasMoved, 1);
    ST16(vars + oAIAttackVars_StepTimer, 2);
}

/* ai_panelTargetAttackStep: r5 = battle object, r6 = attack phase,
 * r7 = AI attack variables. Outputs r1/r5/r8-r12 are left exactly as the
 * original leaves them (r1 and r5 come back from the callees themselves),
 * and the flags are clobbered, so nothing needs to be written here. */
Next ai_panelTargetAttackStep(void) {
    ai_panel_target_attack_step(R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── ai_chipAttackTick ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; } call_sub_80E71B8_result;
/* register form: generated wrapper: battle_markObjectDead_80E71B8 has no typed C interface yet */
static inline call_sub_80E71B8_result call_sub_80E71B8(u32 r0) {
    R0 = r0;
    GUEST_CALL(battle_markObjectDead_80E71B8);
    return (call_sub_80E71B8_result){ R1 };
}

typedef struct { u32 r0, r1, r2, r3, r6, r7; bool z, n, c, v; } call_object_exitAttackState_result_ai_chipAttackTick;
/* register form: generated wrapper: object_exitAttackState has no typed C interface yet */
static inline call_object_exitAttackState_result_ai_chipAttackTick call_object_exitAttackState_ai_chipAttackTick(u32 r5, u32 r6, u32 r7) {
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_exitAttackState);
    return (call_object_exitAttackState_result_ai_chipAttackTick){ R0, R1, R2, R3, R6, R7, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* One tick of the AI's chip-attack state (r5 = battle object, r6 = context
 * passed through to the exit routine, r7 = AI attack vars).
 *
 * When the attack element is still unset this is the first tick: pick the
 * element, switch to the wind-up animation and start the step timer at 40.
 * Otherwise count the timer down each tick and leave the attack state once
 * it reaches zero.
 *
 * r1 is the function's only live output; it comes from whichever callee runs
 * and passes through untouched on ticks that call nothing. */
static u32 chip_attack_tick_impl(u32 r1, u32 obj, u32 aux, u32 ai) {
    u32 element = LD8(ai + oAIAttackVars_AttackElement);

    if (element == 0) {
        /* First tick: arm the attack (element 4, idle anim, 40-tick timer). */
        ST8(ai + oAIAttackVars_AttackElement, 4);
        ST8(obj + oBattleObject_CurAnim, 0);
        ST16(ai + oAIAttackVars_StepTimer, 0x28);

        u32 scratch = LD32(ai + oAIAttackVars_ScratchVar);
        if (scratch != 0)
            return call_sub_80E71B8(scratch).r1;
        return r1;
    } else {
        /* Later ticks: tick the timer down; exit the attack when it hits 0. */
        u32 timer = LD16(ai + oAIAttackVars_StepTimer) - 1;
        ST16(ai + oAIAttackVars_StepTimer, timer);
        if ((s32)timer > 0)
            return r1;
        return call_object_exitAttackState_ai_chipAttackTick(obj, aux, ai).r1;
    }
}

/* ai_chipAttackTick(r5 = battle object, r6, r7 = AI attack vars) -> r1 */
Next ai_chipAttackTick(void) {
    R1 = chip_attack_tick_impl(R1, R5, R6, R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackHitElementBranchTable ── */
/*
 * AI "attack hit" state dispatch. The AI work area's HitElement byte selects
 * one of four state handlers from the table at off_80FFE38; the original
 * tail-calls it (mov lr,pc / bx r1, with its epilogue pop {r7,pc} returning
 * straight to this function's caller), so r1/r5/r8/r9/r10/r11/r12 end up
 * holding whatever the chosen handler leaves behind.
 */

/* register form: the call goes through the branch table at off_80FFE38 (no
 * wrapper exists for the four handlers) and the handlers keep their working
 * state in r5-r12, so the guest registers must be live across the call;
 * typed C cannot express this call. */
static void call_hit_element_handler(u32 handler, u32 hit_element,
                                     u32 r5, u32 r6, u32 r7, u32 r10) {
    R0 = hit_element;      /* the original leaves HitElement in r0 for the handler */
    R1 = handler;          /* the table entry is still in r1 at the branch */
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R10 = r10;
    GUEST_CALL_ADDR(handler);
    R7 = r7;               /* pop {r7,pc}: r7 is preserved across the handler */
}

/* Look up the handler for the current HitElement and run it. */
static void ai_hit_element_branch(u32 r5, u32 r6, u32 r7, u32 r10) {
    u32 hit_element = LD8(r7 + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_80FFE38 + hit_element);   /* table index is a byte offset */
    call_hit_element_handler(handler, hit_element, r5, r6, r7, r10);
}

/* ai_attackHitElementBranchTable(r5, r6, r7 = AI attack vars, r10):
 * tail-call the handler selected by HitElement. */
Next ai_attackHitElementBranchTable(void) {
    ai_hit_element_branch(R5, R6, R7, R10);
    RETURN_TO_CALLER();
}

/* ── ai_attackDelayHit ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; } call_sub_80D29B2_result;
/* register form: generated wrapper: sub_80D29B2 has no typed C interface yet */
static inline call_sub_80D29B2_result call_sub_80D29B2(u32 r0) {
    R0 = r0;
    GUEST_CALL(sub_80D29B2);
    return (call_sub_80D29B2_result){ R1 };
}

/* AI attack state "delay before the hit lands".
 * r7 points to the enemy AI's attack variables. On the first call with no
 * element queued yet it starts the attack (element 4, 5-step timer) and, if
 * the AI's scratch flag says so, notifies sub_80D29B2. Every call then counts
 * the timer down; once it reaches zero the hit element is set to 4, i.e. the
 * hit actually comes out.
 *
 * Returns the value the caller should see in r1: unchanged on entry except on
 * the path that runs sub_80D29B2, where it is that call's r1 result. */
static u32 ai_attack_delay_hit(u32 r1, u32 vars) {
    /* First entry: arm the attack. */
    if (LD8(vars + oAIAttackVars_AttackElement) == 0) {
        ST8(vars + oAIAttackVars_AttackElement, 4);
        ST16(vars + oAIAttackVars_StepTimer, 5);

        u32 notify = LD32(LD32(vars + oAIAttackVars_ScratchVar));
        if (notify != 0)
            r1 = call_sub_80D29B2(notify).r1;
    }

    /* Count the delay down; the hit lands when it expires. */
    u32 timer = LD16(vars + oAIAttackVars_StepTimer) - 1;
    ST16(vars + oAIAttackVars_StepTimer, timer);
    if ((s32)timer <= 0)
        ST16(vars + oAIAttackVars_HitElement, 4);

    return r1;
}

/* ai_attackDelayHit(r7 = AI attack vars) -> r1 (flags clobbered, unused by callers) */
Next ai_attackDelayHit(void) {
    R1 = ai_attack_delay_hit(R1, R7);
    RETURN_TO_CALLER();
}

/* ── battle_findClosestPanelOnObjectRow_8100070 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5; bool z, n, c, v; } call_object_getClosestPanelMatchingRowFiltered_result_sub_8100070;
/* register form: generated wrapper: object_getClosestPanelMatchingRowFiltered has no typed C interface yet */
static inline call_object_getClosestPanelMatchingRowFiltered_result_sub_8100070 call_object_getClosestPanelMatchingRowFiltered_sub_8100070(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_getClosestPanelMatchingRowFiltered);
    return (call_object_getClosestPanelMatchingRowFiltered_result_sub_8100070){ R0, R1, R2, R3, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Panel lookup result: object_getClosestPanelMatchingRowFiltered reports the
 * panel it picked in r0/r1 (position/coordinates as the battle engine uses). */
typedef struct { u32 r0, r1; } closest_panel_result;

/* Find the panel closest to (x, y) on the row taken from row_src's byte at
 * +0x13, considering only panels that match self_obj's alliance. */
static closest_panel_result find_closest_panel(u32 row_src, u32 x, u32 y, u32 self_obj) {
    u32 row      = LD8(row_src + 0x13);
    u32 alliance = LD8(self_obj + oBattleObject_Alliance);

    call_object_getClosestPanelMatchingRowFiltered_result_sub_8100070 res =
        call_object_getClosestPanelMatchingRowFiltered_sub_8100070(alliance, row, x, y);

    closest_panel_result out;
    out.r0 = res.r0;
    out.r1 = res.r1;
    return out;
}

/* battle_findClosestPanelOnObjectRow_8100070(r0 = row source object, r2/r3 = x/y position, r5 = asking object)
 * -> r0/r1 = closest matching panel, r5 passed through unchanged.
 * The callee leaves r2/r3/r4/r5 alone, so r5 needs no re-write here. */
Next battle_findClosestPanelOnObjectRow_8100070(void) {
    closest_panel_result res = find_closest_panel(R0, R2, R3, R5);
    R0 = res.r0;
    R1 = res.r1;
    RETURN_TO_CALLER();
}

/* ── ai_attackSpawnThreeAhead ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFrontDirection_result_ai_attackSpawnThreeAhead;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_ai_attackSpawnThreeAhead call_object_getFrontDirection_ai_attackSpawnThreeAhead(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_ai_attackSpawnThreeAhead){ R0 };
}

typedef struct { u32 r1, r2, r3, r4, r5, r6; } call_object_spawnCollisionRegion_result;
/* register form: generated wrapper: object_spawnCollisionRegion has no typed C interface yet */
static inline call_object_spawnCollisionRegion_result call_object_spawnCollisionRegion(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_spawnCollisionRegion);
    return (call_object_spawnCollisionRegion_result){ R1, R2, R3, R4, R5, R6 };
}

/*
 * AI attack action: lays down a line of collision regions one, two and three
 * panels in front of the acting battle object, re-reading the object's facing
 * direction before each spawn.
 *
 * The attribute word (r4) and the damage value (r6) are only loaded as call
 * arguments here; the original keeps them in saved registers and pops them
 * back before returning, so their entry values are threaded through the typed
 * function and written back by the adapter alongside the real results.
 */

/* r4 argument for object_spawnCollisionRegion: the region's attribute word. */
#define AHEAD_REGION_ATTR   0x4050301u
/* r7 argument for object_spawnCollisionRegion: the region's length in panels. */
#define AHEAD_REGION_LENGTH 3u

/* Everything the adapter must write back: the final spawn's r1/r2 results plus
 * the registers the original restores from its stack frame on the way out. */
struct AttackSpawnResult {
    u32 r1;
    u32 r2;
    u32 r4;
    u32 r5;
    u32 r6;
    u32 r7;
};

/* Spawn one collision region `step` panels in front of battleObj, configured
 * from the attack parameters in attackVars. Returns the spawn call's result
 * registers (only the final spawn's r1/r2 are used by the caller). */
static call_object_spawnCollisionRegion_result spawn_ahead_panel(
        u32 battleObj, u32 attackVars, u32 step) {
    u32 direction = call_object_getFrontDirection_ai_attackSpawnThreeAhead(battleObj).r0;
    u32 x = direction * step + LD8(battleObj + oBattleObject_PanelX);
    u32 y = LD8(battleObj + oBattleObject_PanelY);
    u32 initFlag = LD8(attackVars + oAIAttackVars_InitFlag);
    u32 damage = LD32(attackVars + oAIAttackVars_Damage);
    return call_object_spawnCollisionRegion(x, y, initFlag, 0, AHEAD_REGION_ATTR,
                                            battleObj, damage, AHEAD_REGION_LENGTH);
}

/* Spawn the three-panel line ahead of the object. r4Entry/r6Entry are the
 * entry values of r4/r6, which the original pops back before returning. */
static struct AttackSpawnResult ai_attackSpawnThreeAhead_impl(
        u32 battleObj, u32 attackVars, u32 r4Entry, u32 r6Entry) {
    struct AttackSpawnResult out;
    call_object_spawnCollisionRegion_result last;

    (void)spawn_ahead_panel(battleObj, attackVars, 1);   /* one panel ahead   */
    (void)spawn_ahead_panel(battleObj, attackVars, 2);   /* two panels ahead  */
    last = spawn_ahead_panel(battleObj, attackVars, 3);  /* three panels ahead */

    out.r1 = last.r1;       /* result registers of the final spawn call     */
    out.r2 = last.r2;
    out.r4 = r4Entry;       /* saved registers the original pops on return  */
    out.r5 = battleObj;
    out.r6 = r6Entry;
    out.r7 = attackVars;
    return out;
}

/* ai_attackSpawnThreeAhead(r5 = battle object, r7 = attack vars) -> r1 r2 r5 */
Next ai_attackSpawnThreeAhead(void) {
    u32 r4Entry = R4, r6Entry = R6;
    struct AttackSpawnResult out = ai_attackSpawnThreeAhead_impl(R5, R7, r4Entry, r6Entry);
    R1 = out.r1, R2 = out.r2, R4 = out.r4, R5 = out.r5, R6 = out.r6, R7 = out.r7;
    RETURN_TO_CALLER();
}

/* ── ai_highlightTargetPanel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4; bool z, n, c, v; } call_object_highlightPanelRegion_result;
/* register form: generated wrapper: object_highlightPanelRegion has no typed C interface yet */
static inline call_object_highlightPanelRegion_result call_object_highlightPanelRegion(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_highlightPanelRegion);
    return (call_object_highlightPanelRegion_result){ R0, R1, R2, R3, R4, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Highlight the panel the AI is currently targeting.
 * r5 = battle object (its alliance/flip word selects the highlight side),
 * r7 = AI attack state (TargetPanelX is the column to flash),
 * constants 2 and 5 give the highlight region its width and height.
 * Returns whatever object_highlightPanelRegion left in r1 (the only output callers use). */
static u32 highlight_target_panel(u32 objectPtr, u32 attackVarsPtr) {
    u32 panelX   = LD8(attackVarsPtr + oAIAttackVars_TargetPanelX);
    u32 alliance = LD16(objectPtr + oBattleObject_AllianceAndDirectionFlip);

    return call_object_highlightPanelRegion(panelX, 2, 5, alliance).r1;
}

/* ai_highlightTargetPanel(r5 = battle object, r7 = AI attack vars) -> r1 */
Next ai_highlightTargetPanel(void) {
    R1 = highlight_target_panel(R5, R7);
    RETURN_TO_CALLER();
}

/* ── battleObject_dispatch_8108F50 ── */
/* Jump table at 0x08108F68 is indexed by the state byte directly (byte offsets 0, 4, 8
 * into the 3-entry pointer array).  Each handler is called with the full register file
 * intact, matching the original mov-lr / bx-r1 dispatch pattern. */

/* register form: dispatches to the battle-state handler identified by its byte-offset
 * in the 3-entry jump table (0 = battleObject_initNaviObject_8016F56, 4 = battle_8108F74, 8 = battleObject_destroy_8016C4E). */
static void call_battle_state_handler(u32 state) {
    if (state == 0)
        GUEST_CALL(battleObject_initNaviObject_8016F56);
    else if (state == 4)
        GUEST_CALL(battle_8108F74);
    else /* state == 8 */
        GUEST_CALL(battleObject_destroy_8016C4E);
}

/* register form: calls battleObject_updateSprite_8016E64 outside this file, which has no typed C interface yet
 * (post-dispatch update that drives enemy attack animations). */
static void call_post_dispatch_update(void) {
    GUEST_CALL(battleObject_updateSprite_8016E64);
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

/* ── highlightPanelIfSomeCondition_8109660 ── */
/* highlightPanelIfSomeCondition_8109660(r0 = base x, r1 = base y,
 *                                       r2 = offset-list index, r3 = side):
 * Walk the s8 (dx, dy) panel-offset list selected by r2 from
 * PanelOffsetListsPointerTable and highlight every panel (x + dx*dir, y + dy)
 * that lands on the 6x3 battle grid. The x offsets are mirrored horizontally
 * according to r3. Each list ends with an x offset of 0x7F; on return
 * r0 = 0x7F. Preserves r4-r7. */
Next highlightPanelIfSomeCondition_8109660(void) {
    u32 baseX = R0;                             /* base column */
    u32 baseY = R1;                             /* base row */

    R2 <<= 2;                                   /* list index -> word offset */
    u32 list = LD32(A_PanelOffsetListsPointerTable + R2);
    s32 xDir = 1 - (s32)(R3 << 1);              /* +1 or -1: mirror the x offsets */

    for (u32 entry = list;; entry += 2) {
        s32 dx = (s8)LD8(entry);                /* x offset of this entry */
        R0 = (u32)dx;
        if (dx == 0x7F) {                       /* 0x7F terminates the list */
            SET_FLAGS_CMP(dx, 0x7F);
            break;
        }

        R0 = (u32)(dx * xDir + (s32)baseX);         /* panel column, mirrored by side */
        R1 = (u32)((s8)LD8(entry + 1) + (s32)baseY); /* panel row */

        /* highlight the panel only if it is on the 6x3 battle grid */
        if ((s32)R0 >= 1 && (s32)R0 <= 6 && (s32)R1 >= 1 && (s32)R1 <= 3) {
            /* r0/r1 already hold the panel coordinates to highlight */
            GUEST_CALL(object_highlightPanel);
        }
    }

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

/* ── sub_8109708 ── */
/* sub_8109708(r0, r1, r2, r5 = battleObject):
 *   Loads the alliance byte from the battle object, sets up a panel
 *   parameter table whose contents depend on that alliance, then asks
 *   for a random panel relative to (r0, r1) using those parameters.
 *   r4 and r6 are preserved across the call (they are caller-saved
 *   in the ABI but the original push/pops them explicitly). */
Next sub_8109708(void) {
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;

    /* mov r4, r2 -- pass r2 through to GetRandomRelativePanelFiltered */
    R4 = R2;
    /* ldrb r6, [r5, #oBattleObject_Alliance] -- alliance of the attacker */
    R6 = LD8(R5 + oBattleObject_Alliance);
    /* ldr r3, off_8109780 -> R3 = A_off_8109784 (panel param table) */
    R3 = A_off_8109784;
    /* bl GetAllianceDependentPanelParamArgs -- sets up panel reach params */
    GUEST_CALL(GetAllianceDependentPanelParamArgs);
    /* bl GetRandomRelativePanelFiltered -- picks a random relative panel */
    GUEST_CALL(GetRandomRelativePanelFiltered);

    R4 = saved_r4;
    R6 = saved_r6;
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

/* ── sub_8109794 ── */
/* sub_8109794(r0, r1 = object):
 * If the panel under this object can be cracked (flag 0x40 of its panel
 * parameters is clear), crack it. r0/r1 are passed through to both calls. */
Next sub_8109794(void) {
    u32 objectLo = R0;
    u32 objectHi = R1;

    R0 = objectLo;
    R1 = objectHi;
    GUEST_CALL(object_getPanelParameters);

    u32 params = R0;
    R0 = objectLo;   /* pop {r0,r1}: both paths leave the inputs (or crackPanel's results) */
    R1 = objectHi;
    if (!(params & 0x40))
        GUEST_CALL(object_crackPanel);

    RETURN_TO_CALLER();
}

/* ── sub_8109804 ── */
/*
 * AI-attack tail-call dispatch thunk (0x08109804).
 *
 * Reads a selector byte from the AI attack-vars block (r7), looks up a
 * handler address in the two-entry table at off_8109818 (indexed by the
 * raw byte, no ×4 scaling), and branches into it.  The handler sees
 * R0 = selector, R1 = handler address; every register and flag it
 * writes on return IS this thunk's result (the original simply
 * "pop {pc}" back to the caller).
 */

/* register form: tail call through a jump table whose target is only
 * known at runtime; no generated call_* wrapper exists for this
 * pattern, so GUEST_CALL_ADDR performs the original's "mov lr, pc; bx r1". */
static void ai_attack_dispatch_sub_8109804(void) {
    /* selector byte from the attack-vars block */
    R0 = LD8(R7 + oAIAttackVars_HitElement);
    /* table entry (4 bytes wide, indexed by raw selector as in ldr r1,[r1,r0]) */
    R1 = LD32(A_off_8109818 + R0);
    /* branch to handler; on return every guest register and flag holds
     * whatever the callee left behind – that is our output. */
    GUEST_CALL_ADDR(R1);
}

/* inputs: r5 r6 r7 lr; outputs: all regs + flags (pass-through from handler) */
Next sub_8109804(void) {
    ai_attack_dispatch_sub_8109804();
    RETURN_TO_CALLER();
}

/* ── sub_8109820 ── */
/* sub_8109820: AI attack-step handler. If the object can move and the
 * candidate panel passes the parameter check, store the future panel on
 * the battle object and reserve it, set the "cannot slide" flag,
 * dispatch to sub_81098C4 or sub_810990E based on Unk_03, then run
 * battleObject_advanceAIStepMove_810986E. Otherwise clear Unk_1a and call object_exitAttackState_801171C.
 *
 * Inputs:  r5 = battle object ptr, r6 = ?, r7 = AI attack vars ptr
 * Outputs: standard AAPCS; preserves r4..r7. */
Next sub_8109820(void) {
    /* If the object cannot move, bail out to the failure path. */
    GUEST_CALL(object_canMove);
    if (ZERO_FLAG()) goto fail;

    /* Load candidate panel coordinates and parameters, then validate. */
    R0 = LD8(R7 + oAIAttackVars_TargetPanelX);
    R1 = LD8(R7 + oAIAttackVars_TargetPanelY);
    R2 = LD32(R7 + oAIAttackVars_AiAttackParam);
    R3 = LD32(R7 + oAIAttackVars_PanelParamFlags2);
    GUEST_CALL(object_checkPanelParameters);
    if (R0 == 0) goto fail;

    /* Panel OK: reload X/Y into r0/r1, write them as the future panel on
     * the battle object, then reserve the panel with those same args. */
    R0 = LD8(R7 + oAIAttackVars_TargetPanelX);
    ST8(R5 + oBattleObject_FuturePanelX, (u8)R0);
    R1 = LD8(R7 + oAIAttackVars_TargetPanelY);
    ST8(R5 + oBattleObject_FuturePanelY, (u8)R1);
    GUEST_CALL(object_reservePanel);

    R0 = OBJECT_FLAGS_CANNOT_SLIDE;
    GUEST_CALL(object_setFlag1);

    ST16(R7 + oAIAttackVars_HitElement, 4);

    if (LD8(R7 + oAIAttackVars_AiDifficultyTier) != 0) {
        GUEST_CALL(sub_81098C4);
    } else {
        GUEST_CALL(sub_810990E);
    }
    GUEST_CALL(battleObject_advanceAIStepMove_810986E);
    RETURN_TO_CALLER();

fail:
    ST8(R7 + oAIAttackVars_HasMoved, 0);
    GUEST_CALL(object_exitAttackState_801171C);
    RETURN_TO_CALLER();
}

/* ── sub_81098C4 ── */
/* sub_81098C4: drive a battle object toward the AI's target panel.
 *
 * Reads the AI's current target panel (X from oAIAttackVars_TargetPanelX,
 * Y from oAIAttackVars_TargetPanelY) and the object's current panel,
 * decides which axis is off (Y first, then X), writes the matching
 * velocity to the object, and stores the frame/duration pair that
 * the caller uses to know when to issue the hit.
 *
 * Register contract:
 *   inputs:  r5 (object), r7 (AI vars), lr
 *   outputs: r2, r3
 *   clobbers: nz, c, v, r0, r1
 */
Next sub_81098C4(void) {
    u32 obj = R5;
    u32 ai  = R7;

    /* --- Y axis: AIUnk_17 vs object PanelY --- */
    s32 dy    = (s32)((u8)LD8(ai + oAIAttackVars_TargetPanelY))
              - (s32)((u8)LD8(obj + oBattleObject_PanelY));
    u32 speed = LD32(ai + oAIAttackVars_ScratchVar);
    u32 yVel  = speed * (u32)dy;

    /* MUL preserves flags, but in the Thumb encoding it's the result
     * of the multiply that sets NZ; BEQ below tests r1 (the product). */
    SET_FLAGS_NZ(yVel);

    if (yVel == 0) goto x_axis;

    /* Y is the active axis: drive vertically, no horizontal motion. */
    ST32(obj + oBattleObject_YVelocity, yVel);
    ST32(obj + oBattleObject_XVelocity, 0);

    R0 = 0x18 << 16;            /* duration in 16.16 fixed-point */
    goto divide_speed;

x_axis:
    /* --- X axis: AIUnk_16 vs object PanelX --- */
    s32 dx   = (s32)((u8)LD8(ai  + oAIAttackVars_TargetPanelX))
             - (s32)((u8)LD8(obj + oBattleObject_PanelX));
    u32 xVel = speed * (u32)dx;
    SET_FLAGS_NZ(xVel);

    if (xVel != 0) {
        /* Drive horizontally, no vertical motion. */
        ST32(obj + oBattleObject_XVelocity, xVel);
        ST32(obj + oBattleObject_YVelocity, 0);
        R0 = 0x28 << 16;        /* longer duration than the Y case */
        goto divide_speed;
    }

    /* Both axes already match: zero both velocities, mark as hit. */
    ST32(obj + oBattleObject_XVelocity, 0);
    ST32(obj + oBattleObject_YVelocity, 0);
    ST16(ai + oAIAttackVars_StepTimer, 1);
    ST16(ai + oAIAttackVars_ChargeCycles, 1);
    goto done;

divide_speed:
    /* BIOS Divide(r0 / r1): num = duration fixed-point, denom = speed.
     * Quotient in r0, remainder in r1; only r0 (whole frames) is kept. */
    R1 = speed;
    swi_call(6);
    ST16(ai + oAIAttackVars_StepTimer, (u16)R0);
    ST16(ai + oAIAttackVars_ChargeCycles, (u16)(R0 >> 1));

done:
    RETURN_TO_CALLER();
}

/* ── battle_attackTimerTick_8109CBC ── */
/* battle_attackTimerTick_8109CBC (r5 = battle object, r7 = this attack's variable block):
 * Per-frame tick for an AI attack. Counts the attack's remaining-frames
 * counter (oAIAttackVars_StepTimer) down by one; while it is still non-negative
 * the attack simply stays active for another frame. Once the counter runs out,
 * the attack's teardown helpers are run on the object's extra var, its state
 * word (pointed to by oAIAttackVars_ScratchVar) is cleared, and the object leaves
 * the attack state. */
Next battle_attackTimerTick_8109CBC(void) {
    /* Decrement the attack countdown (halfword), keeping the SUBS flags for
     * the early-out path. */
    u32 timer = LD16(R7 + oAIAttackVars_StepTimer);
    SET_FLAGS_CMP(timer, 1);            /* sub r0, #1 */
    timer -= 1;
    R0 = timer;
    ST16(R7 + oAIAttackVars_StepTimer, timer);
    if ((s32)timer >= 0)                /* bge: attack still has frames left */
        RETURN_TO_CALLER();

    /* Timer expired. If the attack still owns a state word, run its two
     * cleanup helpers on the object's extra var, then zero the word. */
    u32 statePtr = LD32(R7 + oAIAttackVars_ScratchVar);
    R0 = LD32(statePtr);
    if (R0 != 0) {
        R0 = LD32(R5 + oBattleObject_ExtraVars + 4);
        GUEST_CALL(battleObject_removeFromNextList_800AA40);
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

/* ── sub_8109CE6 ── */
/* AI attack state-machine dispatcher (bn6f asm31.s).
 *
 * Reads the AI attack sub-state byte from the AIAttackVars struct pointed to
 * by r7 and jumps to its handler through the table at off_8109CF8:
 *   0x00 -> sub_8109D08
 *   0x04 -> ai_commitStepMove
 *   0x08 -> aiAttackVars_countdownSub_8109D98
 *   0x0c -> ai_attackTickDown_timerEndExit_8109DBA
 * The handler is entered with r0 = sub-state index and r1 = its own table
 * entry (mov lr, pc; bx r1), and the results it leaves in r1/r5/r8-r12 are
 * this routine's outputs, so register state flows straight through to
 * sub_8109CE6's caller. */

/* register form: the dispatch target comes from the jump table at
 * off_8109CF8, so this is a mov lr, pc / bx rN call that no generated
 * wrapper covers. The handler must run on the live register file (r0 =
 * sub-state index, r1 = its table entry, r5/r6/r7/r10 passed through
 * untouched) and leaves the outputs r1/r5/r8-r12 in place; those values
 * cannot be round-tripped through C locals or a result struct. */
static u32 dispatch_ai_attack_handler(u32 handler, u32 index, u32 tableEntry) {
    R0 = index;
    R1 = tableEntry;
    GUEST_CALL_ADDR(handler);
    return R0;
}

/* Read the sub-state index from the AI attack vars, look up its handler in
 * the dispatch table and run it. */
static u32 run_ai_attack_step_sub_8109CE6(u32 attackVars) {
    u32 index = LD8(attackVars + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_8109CF8 + index);
    return dispatch_ai_attack_handler(handler, index, handler);
}

Next sub_8109CE6(void) {
    run_ai_attack_step_sub_8109CE6(R7);
    RETURN_TO_CALLER();
}

/* ── aiAttackVars_countdownSub_8109D98 ── */
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
    u32 loaded = LD16(vars + oAIAttackVars_StepTimer);
    u32 counter = loaded - 1;
    ST16(vars + oAIAttackVars_StepTimer, counter);
    set_subs_flags_for_decrement(loaded);

    if (counter != 0)
        return counter;

    call_object_clear_flag(0x40);
    call_object_set_flag1(1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT);

    ST16(vars + oAIAttackVars_StepTimer, LD16(vars + oAIAttackVars_AttackDuration));
    ST16(vars + oAIAttackVars_HitElement, 0xC);
    return 0xC;
}

/* aiAttackVars_countdownSub_8109D98(r7 = AIAttackVars) -> r0, nzcv, r1, r2, lr */
Next aiAttackVars_countdownSub_8109D98(void) {
    R0 = ai_attack_countdown(R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackTickDown_timerEndExit_8109DBA ── */
/* ai_attackTickDown_timerEndExit_8109DBA (r5 = battle object, r7 = AI attack vars):
 * Tick down the attack timer at oAIAttackVars_StepTimer; the decremented
 * count is left in r0 (and the flags reflect the subtraction). While the
 * timer is still counting, nothing else happens. When it reaches zero,
 * reset the object's current animation, mark the attack as finished
 * (Unk_1a = 1) and call object_exitAttackState with r0 = 1. */
Next ai_attackTickDown_timerEndExit_8109DBA(void) {
    u32 timer = LD16(R7 + oAIAttackVars_StepTimer);

    SET_FLAGS_CMP(timer, 1);   /* SUBS timer, #1 */
    timer -= 1;
    ST16(R7 + oAIAttackVars_StepTimer, timer);
    R0 = timer;                /* r0 holds the decremented timer on return */

    if (timer != 0)
        RETURN_TO_CALLER();

    ST8(R5 + oBattleObject_CurAnim, 0);
    ST8(R7 + oAIAttackVars_HasMoved, 1);

    R0 = 1;
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── aiAttackDispatch_8109DD2 ── */
/* AI attack dispatch: selects one of two handlers using the attack-var type
 * byte stored at the head of the AIAttackVars struct referenced by self. */

/* register form: full-register virtual dispatch — both handlers (aiAttackVars_runKeyItemTrapStep_8109DEC
 * and aiAttackTimer_tick_8109E4A) receive and return every CPU register, so C parameter
 * passing cannot express the call. */
static void ai_attack_dispatch_sub_8109DD2(u32 self) {
    u8 idx = LD8(self + oAIAttackVars_HitElement);
    u32 handler_addr = LD32(A_off_8109DE4 + idx);
    R0 = idx;
    R1 = handler_addr;
    u32 saved_r6 = R6;
    if (idx == 0)
        GUEST_CALL(aiAttackVars_runKeyItemTrapStep_8109DEC);
    else
        GUEST_CALL(aiAttackTimer_tick_8109E4A);
    R6 = saved_r6;
}

Next aiAttackDispatch_8109DD2(void) {
    ai_attack_dispatch_sub_8109DD2(R7);
    RETURN_TO_CALLER();
}

/* ── aiAttackTimer_tick_8109E4A ── */
/* register form: calls battleObject_removeFromNextList_800AA40 outside this file, which has no typed C interface yet */
static void ai_recalc(u32 extra_vars) {
    R0 = extra_vars;
    GUEST_CALL(battleObject_removeFromNextList_800AA40);
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

#define oAIAttackVars_AttackElement 0x01
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
    if (LD8(r7 + oAIAttackVars_AttackElement) == 0) {
        ST8(r7 + oAIAttackVars_AttackElement, 1);
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

/* aiAttackTimer_tick_8109E4A(r4, r5, r6, r7, lr) -> nz c v r0 r1 r2 r3 r4 r6 r7 lr */
Next aiAttackTimer_tick_8109E4A(void) {
    R0 = tick_ai_attack_timer(R5, R7);
    SET_FLAGS_CMP(R0 + 1, 1);
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackDispatch_8109E7A ── */
/* AI attack dispatch (08109E7A). The AI attack variable block is pointed to by
 * r7; its first byte selects one of three handlers in the table at
 * off_8109E8C. The selected handler runs with r0 = slot index, r1 = its own
 * address and r5/r6/r7 unchanged. When it returns, its register state is what
 * battle_aiAttackDispatch_8109E7A leaves behind (r1, r5, r8-r12), with r6 restored by the
 * original's pop {r6,pc}. */

/* register form: dispatch through the AI attack jump table with
 * GUEST_CALL_ADDR; no wrapper covers the three handlers, and their register
 * state (r1, r5, r8-r12) is battle_aiAttackDispatch_8109E7A's output, so it is left in the guest
 * register file for the adapter's caller. */
static void dispatch_ai_attack_handler_sub_8109E7A(u32 vars_ptr, u32 in_r5, u32 in_r6)
{
    u32 slot    = LD8(vars_ptr + oAIAttackVars_HitElement);
    u32 handler = LD32(A_off_8109E8C + slot);

    R0 = slot;      /* the handler receives the slot index in r0 */
    R1 = handler;   /* ...and its own address in r1 */
    R5 = in_r5;
    R6 = in_r6;
    R7 = vars_ptr;

    GUEST_CALL_ADDR(handler);

    R6 = in_r6;     /* pop {r6,pc}: r6 is preserved across the dispatch */
}

/* battle_aiAttackDispatch_8109E7A(r5, r6, r7 = AI attack vars): run the selected attack handler. */
Next battle_aiAttackDispatch_8109E7A(void) {
    dispatch_ai_attack_handler_sub_8109E7A(R7, R5, R6);
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
        GUEST_CALL(battleObject_removeFromNextList_800AA40);
    }
    GUEST_CALL(battle_objectUpdateDispatchPhase_80165C2);
    RETURN_TO_CALLER();
}

/* ── battleObject_runAIDispatcher_8109F7E ── */
/* battleObject_runAIDispatcher_8109F7E: call the two init steps in sequence (battleObject_initAndRunAI_8016B02, then
 * battle_object_onAction8EndClearExtraVars_8109FA2) and return; results are whatever the callees leave behind. */
Next battleObject_runAIDispatcher_8109F7E(void) {
    GUEST_CALL(battleObject_initAndRunAI_8016B02);
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
    GUEST_CALL(battleObject_removeFromNextList_800AA40);

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
 *    the per-attack step handler (battle_findApproachPanelTowardEnemy) is installed in Unk_28, the
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
        ST32(R7 + oAIAttackVars_PanelChooseCallback, A_battle_findApproachPanelTowardEnemy | 1);
        ST16(R7 + oAIAttackVars_AttackDuration,
             LD8(LD8(R4 + oAIData_Version_16) + A_byte_8109F46));

        R0 = 0xA;
        GUEST_CALL(object_setAttack0);

        ST8(R6 + 2, 4);
        R0 = 4;
        SET_FLAGS_NZ(R0);
    } else if (LD8(R7 + oAIAttackVars_HasMoved) == 0) {
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

/* ── npc_aiAdvanceSubstate_810A204 ── */
/* Advance the AI actor's sub-state counter; when it reaches 0x28, switch to
 * script 0x0C and reset the counter for the next phase. (r6 = AI state block) */
static void advance_ai_substate(u32 statePtr) {
    u32 subState = LD8(statePtr + oAIState_SubState) + 1;
    ST8(statePtr + oAIState_SubState, subState);

    if ((s32)subState >= 0x28) {
        ST8(statePtr + oAIState_ScriptId, 0x0C);
        ST16(statePtr + oAIState_SubState, 0);
    }
}

/* npc_aiAdvanceSubstate_810A204(r6 = AI state block) -> nothing */
Next npc_aiAdvanceSubstate_810A204(void) {
    advance_ai_substate(R6);
    RETURN_TO_CALLER();
}

/* ── battle_countActorsWithParamAt0x68_810A534 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7; } call_FindBattleActorsWithNonzeroParam_ReturnCountAndSum_result;
/* register form: generated wrapper: FindBattleActorsWithNonzeroParam_ReturnCountAndSum has no typed C interface yet */
static inline call_FindBattleActorsWithNonzeroParam_ReturnCountAndSum_result call_FindBattleActorsWithNonzeroParam_ReturnCountAndSum(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(FindBattleActorsWithNonzeroParam_ReturnCountAndSum);
    return (call_FindBattleActorsWithNonzeroParam_ReturnCountAndSum_result){ R0, R1, R2, R3, R4, R5, R6, R7 };
}

/* battle_countActorsWithParamAt0x68_810A534: scan the battle for actors whose per-actor parameter at offset
 * 0x68 is nonzero.  Fixed search parameters: field offset 0x68, actor kind 7,
 * up to 12 actors.  Returns the number of matches (r0), their parameter sum
 * (r1), and the matching actors' data in r5-r7 as produced by the callee; the
 * search runs with the caller's r5 (search context) and r10. */
static call_FindBattleActorsWithNonzeroParam_ReturnCountAndSum_result
find_actors_with_nonzero_param_0x68(u32 search_context)
{
    return call_FindBattleActorsWithNonzeroParam_ReturnCountAndSum(0x68, 7, 0xC, search_context);
}

/* battle_countActorsWithParamAt0x68_810A534() -> r0 r1 r5 r6 r7 */
Next battle_countActorsWithParamAt0x68_810A534(void) {
    call_FindBattleActorsWithNonzeroParam_ReturnCountAndSum_result result =
        find_actors_with_nonzero_param_0x68(R5);
    R0 = result.r0;
    R1 = result.r1;
    R5 = result.r5;
    R6 = result.r6;
    R7 = result.r7;
    RETURN_TO_CALLER();
}

/* ── battle_objectPhaseStepInitExtraVars_810A5D6 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool z, n, c, v; } call_battleObject_initAndRunAI_8016B02_result;
/* register form: generated wrapper: battleObject_initAndRunAI_8016B02 has no typed C interface yet */
static inline call_battleObject_initAndRunAI_8016B02_result call_battleObject_initAndRunAI_8016B02(u32 r4, u32 r5, u32 r8) {
    R4 = r4;
    R5 = r5;
    R8 = r8;
    GUEST_CALL(battleObject_initAndRunAI_8016B02);
    return (call_battleObject_initAndRunAI_8016B02_result){ R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_objectPhaseStepInitExtraVars_810A5D6: one step of a battle object's per-phase AI (bn6f asm31.s:179874).
 *
 * On the first entry into this phase (PhaseInitialized == 0) the object's two
 * extra phase variables are cleared; then the shared "init and run AI" handler
 * runs for the object.
 *
 * r4/r5/r8 are the arguments the handler expects (r10 is preserved by it).
 * Everything in r1/r5/r8/r9/r10/r11/r12 that callers see is what the handler
 * leaves behind, so the wrapper call leaves the guest registers in their final
 * state and nothing needs to be passed back through the adapter. */

static void object_phase_step(u32 obj_r4, u32 obj_r5, u32 obj_r8)
{
    /* First entry into this phase: reset the object's extra variables. */
    if (LD8(obj_r5 + oBattleObject_PhaseInitialized) == 0) {
        ST32(obj_r5 + oBattleObject_ExtraVars, 0);
        ST32(obj_r5 + oBattleObject_ExtraVars + 4, 0);
    }

    /* battleObject_initAndRunAI_8016B02(r4, r5, r8); r10 stays as on entry.
     * Its register results (r1, r5, r8, r9, r10, r11, r12) are caller-visible
     * outputs and are already in place after this call. */
    call_battleObject_initAndRunAI_8016B02(obj_r4, obj_r5, obj_r8);
}

/* battle_objectPhaseStepInitExtraVars_810A5D6(r4 = object, r5 = object, r8 = object) */
Next battle_objectPhaseStepInitExtraVars_810A5D6(void) {
    object_phase_step(R4, R5, R8);
    RETURN_TO_CALLER();
}

/* ── ai_attackStepDispatchSubStep ── */
/* AI attack step sub-step dispatcher (0x0810AF30).
 * Reads the sub-step byte offset from the AI attack variable block at r7,
 * looks it up in a three-entry jump table, and calls the handler.
 * The handler sees the sub-step byte in r0 and the handler address in r1;
 * all other registers pass through from the caller unchanged. */

/* register form: indirect call through the sub-step jump table
 * (mov lr, pc; bx r1) – no generated wrapper covers this.
 * The computed r0/r1 and the pass-through registers must be live in the
 * guest register file at the moment GUEST_CALL_ADDR fires. */
static void dispatch_attack_substep(void)
{
    u32 substep = LD8(R7 + oAIAttackVars_SubStepIndex);
    u32 target  = LD32(A_off_810AF44 + substep);   /* byte offset into .word table */

    R0 = substep;
    R1 = target;

    GUEST_CALL_ADDR(target);
}

Next ai_attackStepDispatchSubStep(void)
{
    dispatch_attack_substep();
    RETURN_TO_CALLER();
}

/* ── ai_attackStepPlayIntroAnim ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2; } call_object_setFlag1_result_ai_attackStepPlayIntroAnim;
/* register form: generated wrapper: object_setFlag1 has no typed C interface yet */
static inline call_object_setFlag1_result_ai_attackStepPlayIntroAnim call_object_setFlag1_ai_attackStepPlayIntroAnim(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setFlag1);
    return (call_object_setFlag1_result_ai_attackStepPlayIntroAnim){ R2 };
}

typedef struct { u32 r0, r2, r3; bool c, v; } call_sprite_getFrameParameters_result_ai_attackStepPlayIntroAnim;
/* register form: generated wrapper: sprite_getFrameParameters has no typed C interface yet */
static inline call_sprite_getFrameParameters_result_ai_attackStepPlayIntroAnim call_sprite_getFrameParameters_ai_attackStepPlayIntroAnim(u32 r5) {
    R5 = r5;
    GUEST_CALL(sprite_getFrameParameters);
    return (call_sprite_getFrameParameters_result_ai_attackStepPlayIntroAnim){ R0, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r1, r2, r3; bool c, v; } call_object_setCounterTime_result_ai_attackStepPlayIntroAnim;
/* register form: generated wrapper: object_setCounterTime has no typed C interface yet */
static inline call_object_setCounterTime_result_ai_attackStepPlayIntroAnim call_object_setCounterTime_ai_attackStepPlayIntroAnim(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCounterTime);
    return (call_object_setCounterTime_result_ai_attackStepPlayIntroAnim){ R1, R2, R3, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* AI attack state machine: play the intro animation of an attack.
 * object (r5) is the battle object, vars (r7) is this attack's variable block.
 *
 * First call (AttackStepIndex == 0): start the intro animation - set the
 * object flag, clear the scratch word and select the intro animation slot
 * for the current AI difficulty tier (tier * 3 + 1).
 * Later calls: wait until the animation reports its end frame (bit 7 of the
 * sprite frame parameters), then advance the object's animation, start a
 * 30-frame counter and move the attack to sub-step 4.
 *
 * Returns the value the original leaves in r1 on each path:
 *   first-call path   -> 3    (the multiplier from "tier * 3 + 1")
 *   still waiting     -> 0x80 (the mask from the TST)
 *   animation done    -> r1 as left by object_setCounterTime.
 * r2/r3 on exit are whatever the last called routine left there; the
 * wrappers reproduce that, so nothing needs to be written back explicitly. */
static u32 ai_attack_step_play_intro_anim(u32 object, u32 vars) {
    if (LD8(vars + oAIAttackVars_AttackStepIndex) == 0) {
        /* First entry: kick off the intro animation. */
        ST8(vars + oAIAttackVars_AttackStepIndex, 1);
        ST32(vars + oAIAttackVars_ScratchVar, 0);
        call_object_setFlag1_ai_attackStepPlayIntroAnim(1u << OBJECT_FLAGS_UNK_BIT_22_BIT, object);
        ST8(object + oBattleObject_CurAnim,
            LD8(vars + oAIAttackVars_AiDifficultyTier) * 3 + 1);
        return 3;
    }

    u32 frameParams = call_sprite_getFrameParameters_ai_attackStepPlayIntroAnim(object).r0;
    if ((frameParams & 0x80) == 0)
        return 0x80;    /* intro animation not finished yet */

    /* Intro animation reached its end frame: advance the animation and arm
     * the attack counter. */
    ST8(object + oBattleObject_CurAnim, LD8(object + oBattleObject_CurAnim) + 1);
    call_object_setCounterTime_result_ai_attackStepPlayIntroAnim counter =
        call_object_setCounterTime_ai_attackStepPlayIntroAnim(0x1E, object);
    ST16(vars + oAIAttackVars_SubStepIndex, 4);
    return counter.r1;
}

/* ai_attackStepPlayIntroAnim: inputs r5 r7 r10 lr; r10 passes through to
 * object_setCounterTime untouched, lr is only the return address. */
Next ai_attackStepPlayIntroAnim(void) {
    R1 = ai_attack_step_play_intro_anim(R5, R7);
    RETURN_TO_CALLER();
}

/* ── ai_setAttackTargetPanel ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_ai_attackStepDispatchSubStep_result;
/* register form: generated wrapper: ai_attackStepDispatchSubStep has no typed C interface yet */
static inline call_ai_attackStepDispatchSubStep_result call_ai_attackStepDispatchSubStep(u32 r2, u32 r5, u32 r7) {
    R2 = r2;
    R5 = r5;
    R7 = r7;
    GUEST_CALL(ai_attackStepDispatchSubStep);
    return (call_ai_attackStepDispatchSubStep_result){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* ai_setAttackTargetPanel: runs one attack-step dispatch, then if the
 * dispatch set the scratch flag, unpacks the target panel (X,Y) from
 * Unk_40 and writes them plus hit-element 0xC into the AI attack vars.
 *   X = Unk_40 & 0x7   (lsl #29; lsr #29)
 *   Y = Unk_40 >> 4   (full value stays in r1; strb truncates for memory)
 * Output r1 is the dispatch's r1 when scratch is zero, otherwise Y. */
static u32 ai_set_attack_target_panel(u32 r2, u32 r5, u32 r7) {
    call_ai_attackStepDispatchSubStep_result step =
        call_ai_attackStepDispatchSubStep(r2, r5, r7);

    /* r7 may have been rewritten by the dispatch */
    u32 vars = step.r7;
    u32 r1   = step.r1;

    if (LD32(vars + oAIAttackVars_ScratchVar) != 0) {
        u32 packed  = LD32(vars + oAIAttackVars_Unk_40);
        u32 panel_x = packed & 0x7;        /* keep low 3 bits */
        u32 panel_y = packed >> 4;         /* full shifted value for r1 */
        ST8(vars + oAIAttackVars_TargetPanelX, panel_x);
        ST8(vars + oAIAttackVars_TargetPanelY, panel_y);
        ST16(vars + oAIAttackVars_HitElement, 0xC);
        r1 = panel_y;
    }

    return r1;
}

/* ai_setAttackTargetPanel (0x0810b13e)
 * inputs: r2 r5 r7 | outputs: r1 r5 r8 r9 r10 r11 r12 */
Next ai_setAttackTargetPanel(void) {
    R1 = ai_set_attack_target_panel(R2, R5, R7);
    RETURN_TO_CALLER();
}

/* ── ai_checkPanelReachMask ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r2, r4, r5, r6, r7; } call_sub_8109708_result;
/* register form: generated wrapper: sub_8109708 has no typed C interface yet */
static inline call_sub_8109708_result call_sub_8109708(u32 r0, u32 r1, u32 r2, u32 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R5 = r5;
    GUEST_CALL(sub_8109708);
    return (call_sub_8109708_result){ R0, R2, R4, R5, R6, R7 };
}

/* Build a bitmask indicating whether the panel (given by r0/r1) is
 * reachable along two specific AI paths (r2=4 and r2=2) according
 * to sub_8109708.  Bit 0 set = path-4 reachable, bit 1 set = path-2
 * reachable.  r5 is the current AI context passed to the helper. */
static u32 ai_checkPanelReachMask_impl(u32 panelData, u32 panelInfo, u32 aiContext) {
    u32 mask = 0;

    /* Probe reachability for path type 4. */
    call_sub_8109708_result first = call_sub_8109708(panelData, panelInfo, 4, aiContext);
    if (first.r0 != 0)
        mask = 1;

    /* Probe reachability for path type 2. */
    call_sub_8109708_result second = call_sub_8109708(panelData, panelInfo, 2, aiContext);
    if (second.r0 != 0)
        mask |= 2;

    return mask;
}

/* ai_checkPanelReachMask(r0 = panel data, r1 = panel info, r5 = ai context) -> r0 = reach mask */
Next ai_checkPanelReachMask(void) {
    R0 = ai_checkPanelReachMask_impl(R0, R1, R5);
    RETURN_TO_CALLER();
}

/* ── battle_getFrontPanelParamChecked ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_object_getFrontDirection_result_battle_getFrontPanelParamChecked;
/* register form: generated wrapper: object_getFrontDirection has no typed C interface yet */
static inline call_object_getFrontDirection_result_battle_getFrontPanelParamChecked call_object_getFrontDirection_battle_getFrontPanelParamChecked(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_getFrontDirection);
    return (call_object_getFrontDirection_result_battle_getFrontPanelParamChecked){ R0 };
}

typedef struct { u32 r2, r3; } call_GetAllianceDependentPanelParamArgs_result_battle_getFrontPanelParamChecked;
/* register form: generated wrapper: GetAllianceDependentPanelParamArgs has no typed C interface yet */
static inline call_GetAllianceDependentPanelParamArgs_result_battle_getFrontPanelParamChecked call_GetAllianceDependentPanelParamArgs_battle_getFrontPanelParamChecked(u32 r3, u32 r5) {
    R3 = r3;
    R5 = r5;
    GUEST_CALL(GetAllianceDependentPanelParamArgs);
    return (call_GetAllianceDependentPanelParamArgs_result_battle_getFrontPanelParamChecked){ R2, R3 };
}

typedef struct { u32 r0, r4, r5; } call_object_checkPanelParameters_result_battle_getFrontPanelParamChecked;
/* register form: generated wrapper: object_checkPanelParameters has no typed C interface yet */
static inline call_object_checkPanelParameters_result_battle_getFrontPanelParamChecked call_object_checkPanelParameters_battle_getFrontPanelParamChecked(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_checkPanelParameters);
    return (call_object_checkPanelParameters_result_battle_getFrontPanelParamChecked){ R0, R4, R5 };
}

/* Result of the checked front-panel lookup: the panel coordinates to return,
 * plus r4/r5 which the original leaves exactly as it found them. */
typedef struct {
    u32 r0, r1, r4, r5;
} FrontPanelCheckedResult;

/* battle_getFrontPanelParamChecked body.
 * Looks up the panel in front of the object (direction offset + PanelX/Y),
 * validates it against the alliance-dependent panel parameters, and returns
 * the coordinates. If the parameter check fails, x is forced to 0 while the
 * y coordinate is still returned unchanged, matching the original.
 * r4/r5 are pass-through outputs: the original never modifies them. */
static FrontPanelCheckedResult getFrontPanelParamChecked(u32 obj, u32 r4, u32 r5) {
    u32 direction = call_object_getFrontDirection_battle_getFrontPanelParamChecked(obj).r0;
    u32 x = direction + LD8(obj + oBattleObject_PanelX);
    u32 y = LD8(obj + oBattleObject_PanelY);

    call_GetAllianceDependentPanelParamArgs_result_battle_getFrontPanelParamChecked params =
        call_GetAllianceDependentPanelParamArgs_battle_getFrontPanelParamChecked(A_byte_810B6E0, obj);

    u32 valid = call_object_checkPanelParameters_battle_getFrontPanelParamChecked(x, y, params.r2, params.r3).r0;
    if (valid == 0)
        x = 0;

    FrontPanelCheckedResult res;
    res.r0 = x;
    res.r1 = y;
    res.r4 = r4;
    res.r5 = r5;
    return res;
}

/* battle_getFrontPanelParamChecked(r5 = object) -> r0 = x, r1 = y, r4/r5 unchanged */
Next battle_getFrontPanelParamChecked(void) {
    FrontPanelCheckedResult res = getFrontPanelParamChecked(R5, R4, R5);
    R0 = res.r0;
    R1 = res.r1;
    R4 = res.r4;
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── ai_swordyTryStepOrFlipDir ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0, r1, r4, r5; bool z, n; } call_battle_checkAlliancePanelAtObjectPos_result;
/* register form: generated wrapper: battle_checkAlliancePanelAtObjectPos has no typed C interface yet */
static inline call_battle_checkAlliancePanelAtObjectPos_result call_battle_checkAlliancePanelAtObjectPos(u32 r5) {
    R5 = r5;
    GUEST_CALL(battle_checkAlliancePanelAtObjectPos);
    return (call_battle_checkAlliancePanelAtObjectPos_result){ R0, R1, R4, R5, ZERO_FLAG(), NEGATIVE_FLAG() };
}

/* Result of a panel/alliance check, as the callee leaves it in the registers
 * the callers of ai_swordyTryStepOrFlipDir observe. */
typedef struct { u32 r0, r1, r4, r5; } PanelCheckResult;

/* Swordy AI, step attempt: battle_checkAlliancePanelAtObjectPos examines the
 * panel the object would move onto using its stored step offset
 * (oBattleObject_ExtraVars+8) and returns nonzero (Z clear) when that panel is
 * open to the object's alliance. If the current direction is blocked, the step
 * offset is negated (direction flipped) and the check runs again; when both
 * directions are blocked, r0 reports 0. r1/r4 always carry the last check's
 * values, r5 the object pointer (unchanged). */
static PanelCheckResult swordy_try_step_or_flip_dir(u32 obj)
{
    call_battle_checkAlliancePanelAtObjectPos_result check =
        call_battle_checkAlliancePanelAtObjectPos(obj);

    /* Panel in the current direction is usable: report the check's result. */
    if (!check.z)
        return (PanelCheckResult){ check.r0, check.r1, check.r4, check.r5 };

    /* Blocked: flip the stored step offset and try the other direction. */
    u32 step_offset = LD32(obj + oBattleObject_ExtraVars + 8);
    ST32(obj + oBattleObject_ExtraVars + 8, 0 - step_offset);

    check = call_battle_checkAlliancePanelAtObjectPos(obj);
    if (!check.z)
        return (PanelCheckResult){ check.r0, check.r1, check.r4, check.r5 };

    /* Opposite direction is blocked too: no step possible. */
    return (PanelCheckResult){ 0, check.r1, check.r4, check.r5 };
}

/* ai_swordyTryStepOrFlipDir(r5 = battle object) -> r0 r1 r4 r5 */
Next ai_swordyTryStepOrFlipDir(void) {
    PanelCheckResult res = swordy_try_step_or_flip_dir(R5);
    R0 = res.r0;
    R1 = res.r1;
    R4 = res.r4;
    R5 = res.r5;
    RETURN_TO_CALLER();
}

/* ── battle_checkAlliancePanelAtObjectPos ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r2, r3; } call_GetAllianceDependentPanelParamArgs_result_battle_checkAlliancePanelAtObjectPos;
/* register form: generated wrapper: GetAllianceDependentPanelParamArgs has no typed C interface yet */
static inline call_GetAllianceDependentPanelParamArgs_result_battle_checkAlliancePanelAtObjectPos call_GetAllianceDependentPanelParamArgs_battle_checkAlliancePanelAtObjectPos(u32 r3, u32 r5) {
    R3 = r3;
    R5 = r5;
    GUEST_CALL(GetAllianceDependentPanelParamArgs);
    return (call_GetAllianceDependentPanelParamArgs_result_battle_checkAlliancePanelAtObjectPos){ R2, R3 };
}

typedef struct { u32 r0, r2, r3, r4, r5; } call_object_checkPanelParameters_result_battle_checkAlliancePanelAtObjectPos;
/* register form: generated wrapper: object_checkPanelParameters has no typed C interface yet */
static inline call_object_checkPanelParameters_result_battle_checkAlliancePanelAtObjectPos call_object_checkPanelParameters_battle_checkAlliancePanelAtObjectPos(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(object_checkPanelParameters);
    return (call_object_checkPanelParameters_result_battle_checkAlliancePanelAtObjectPos){ R0, R2, R3, R4, R5 };
}

/* Check whether the panel a battle object occupies is valid for its side.
 *
 * The panel position is (PanelX, PanelY + ExtraVars[2]); the alliance-dependent
 * panel parameter table byte_810B6E0 supplies the limits via
 * GetAllianceDependentPanelParamArgs, and object_checkPanelParameters validates
 * the position against them. Returns the panel coordinates in r0/r1 with Z
 * clear when the check passed. r0/r1 are the coordinates, not the check's
 * result: the original pushes them before the calls and pops them back over
 * the callee's r0/r1 right after reading the flags. r4/r5 end exactly as
 * object_checkPanelParameters leaves them, as in the original. */

typedef struct {
    u32 panel_x;   /* -> r0 */
    u32 panel_y;   /* -> r1 */
    u32 check_r0;  /* object_checkPanelParameters' result, feeds the NZ flags */
} PanelCheckResult_battle_checkAlliancePanelAtObjectPos;

static PanelCheckResult_battle_checkAlliancePanelAtObjectPos check_alliance_panel_at_object(u32 object) {
    PanelCheckResult_battle_checkAlliancePanelAtObjectPos result;

    result.panel_x = LD8(object + oBattleObject_PanelX);
    result.panel_y = LD8(object + oBattleObject_PanelY)
                   + LD32(object + (oBattleObject_ExtraVars + 8));

    call_GetAllianceDependentPanelParamArgs_result_battle_checkAlliancePanelAtObjectPos params =
        call_GetAllianceDependentPanelParamArgs_battle_checkAlliancePanelAtObjectPos(A_byte_810B6E0, object);

    result.check_r0 = call_object_checkPanelParameters_battle_checkAlliancePanelAtObjectPos(
        result.panel_x, result.panel_y, params.r2, params.r3).r0;
    return result;
}

/* battle_checkAlliancePanelAtObjectPos(r5 = object) -> nz r0 r1 (r4 r5 pass through) */
Next battle_checkAlliancePanelAtObjectPos(void) {
    PanelCheckResult_battle_checkAlliancePanelAtObjectPos result = check_alliance_panel_at_object(R5);
    R0 = result.panel_x;
    R1 = result.panel_y;
    SET_FLAGS_NZ(result.check_r0);
    RETURN_TO_CALLER();
}

/* ── ai_waitThenScript4 ── */
/* ai_waitThenScript4 — AI state handler that counts a delay, then hands control
 * to script 4. r4 = AI data (holds the version byte selecting the delay table),
 * r6 = AI state (timer + script id).
 *
 * Each call increments the state's timer. Once the timer reaches the per-version
 * threshold from byte_810BBD8, the timer is reset to 0 and the AI's script id is
 * set to 4. r1 always carries the threshold value on return (the original never
 * rewrites it after the table load); flags and r0/r2 are clobbered freely. */
static u32 wait_then_script4(u32 ai_data, u32 ai_state) {
    u32 timer = LD16(ai_state + oAIState_Timer) + 1;
    ST16(ai_state + oAIState_Timer, timer);

    u32 threshold = LD8(A_byte_810BBD8 + LD8(ai_data + oAIData_Version_16));

    if ((s32)timer >= (s32)threshold) {
        ST16(ai_state + oAIState_Timer, 0);
        ST8(ai_state + oAIState_ScriptId, 4);
    }
    return threshold;
}

Next ai_waitThenScript4(void) {
    R1 = wait_then_script4(R4, R6);
    RETURN_TO_CALLER();
}

/* ── battle_objectUpdateInitOnce ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5; } call_ai_attackHitClearState8_result;
/* register form: generated wrapper: ai_attackHitClearState8 has no typed C interface yet */
static inline call_ai_attackHitClearState8_result call_ai_attackHitClearState8(u32 r0) {
    R0 = r0;
    GUEST_CALL(ai_attackHitClearState8);
    return (call_ai_attackHitClearState8_result){ R5 };
}

typedef struct { u32 r4, r5, r6, r7; bool c, v; } call_ai_attackFreeAuxObjects_result;
/* register form: generated wrapper: ai_attackFreeAuxObjects has no typed C interface yet */
static inline call_ai_attackFreeAuxObjects_result call_ai_attackFreeAuxObjects(u32 r5) {
    R5 = r5;
    GUEST_CALL(ai_attackFreeAuxObjects);
    return (call_ai_attackFreeAuxObjects_result){ R4, R5, R6, R7, CARRY_FLAG(), OVERFLOW_FLAG() };
}

typedef struct { u32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; bool z, n, c, v; } call_battle_objectUpdateDispatchPhase_80165C2_result;
/* register form: generated wrapper: battle_objectUpdateDispatchPhase_80165C2 has no typed C interface yet */
static inline call_battle_objectUpdateDispatchPhase_80165C2_result call_battle_objectUpdateDispatchPhase_80165C2(u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8, u32 r9, u32 r11, u32 r12) {
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    R8 = r8;
    R9 = r9;
    R11 = r11;
    R12 = r12;
    GUEST_CALL(battle_objectUpdateDispatchPhase_80165C2);
    return (call_battle_objectUpdateDispatchPhase_80165C2_result){ R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* battle_objectUpdateInitOnce: run an battle object's once-per-spawn initialisation
 * (when its CurPhase is still 0: clear its attack hit state and release its aux
 * objects), then hand the object on to the phase dispatcher.
 *
 * Register notes:
 *  - ai_attackHitClearState8 returns the (possibly updated) object pointer in r5,
 *    which ai_attackFreeAuxObjects consumes and refreshes in r4-r7; those updated
 *    r4-r7 are what the phase dispatcher receives on the init path.
 *  - The dispatcher preserves r10 (and so do the two init helpers), so R10 leaves
 *    this function exactly as it entered and the adapter never touches it.
 *  - All flags are clobbered; callers do not branch on them after this call.
 */
typedef struct {
    u32 r1;
    u32 r5;
    u32 r8;
    u32 r9;
    u32 r11;
    u32 r12;
} object_update_init_once_result;

static object_update_init_once_result object_update_init_once(
    u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7,
    u32 r8, u32 r9, u32 r11, u32 r12)
{
    /* First update of this object: reset its attack state before dispatching. */
    if (LD8(r5 + oBattleObject_CurPhase) == 0) {
        /* Attack-state pointer lives at offset 0x64 in the object. */
        u32 attackState = LD32(r5 + 0x64);
        u32 cleared = call_ai_attackHitClearState8(attackState).r5;

        call_ai_attackFreeAuxObjects_result aux = call_ai_attackFreeAuxObjects(cleared);
        r4 = aux.r4;
        r5 = aux.r5;
        r6 = aux.r6;
        r7 = aux.r7;
    }

    call_battle_objectUpdateDispatchPhase_80165C2_result dispatch =
        call_battle_objectUpdateDispatchPhase_80165C2(r2, r3, r4, r5, r6, r7, r8, r9, r11, r12);

    object_update_init_once_result out;
    out.r1  = dispatch.r1;
    out.r5  = dispatch.r5;
    out.r8  = dispatch.r8;
    out.r9  = dispatch.r9;
    out.r11 = dispatch.r11;
    out.r12 = dispatch.r12;
    return out;
}

/* battle_objectUpdateInitOnce: object update entry (r5 = object, r2-r4/r6-r9/r11/r12 = dispatch state) */
Next battle_objectUpdateInitOnce(void) {
    object_update_init_once_result out =
        object_update_init_once(R2, R3, R4, R5, R6, R7, R8, R9, R11, R12);
    R1 = out.r1;
    R5 = out.r5;
    R8 = out.r8;
    R9 = out.r9;
    R11 = out.r11;
    R12 = out.r12;
    RETURN_TO_CALLER();
}

/* ── battle_aiAttackCountdownTick_810C05C ── */
/* Tick the AI attack countdown for one battle object.
 * The half-word at Unk_10 counts down each tick. While it stays non-negative
 * after the decrement nothing else happens; once it goes negative the timer is
 * reloaded from Unk_0c (the attack's repeat interval) and Unk_00 is set to 8,
 * marking the attack as ready to fire.
 * Note the original loads the half-word with ldrh (unsigned): a stored 0xFFFF
 * decrements to a large positive value and keeps the branch taken, so the load
 * must be zero-extending, not sign-extending. The bge tests the result signed. */
static void ai_attack_tick_sub_810C05C(u32 vars) {
    u32 countdown = LD16(vars + oAIAttackVars_StepTimer);

    countdown -= 1;
    ST16(vars + oAIAttackVars_StepTimer, (u16)countdown);

    if ((s32)countdown >= 0)
        return;

    ST16(vars + oAIAttackVars_StepTimer, (u16)LD8(vars + oAIAttackVars_AttackBoost2));
    ST16(vars + oAIAttackVars_HitElement, 8);
}

/* battle_aiAttackCountdownTick_810C05C(r7 = AI attack vars) -> nothing (r0 and flags are clobbered) */
Next battle_aiAttackCountdownTick_810C05C(void) {
    ai_attack_tick_sub_810C05C(R7);
    RETURN_TO_CALLER();
}

/* ── ai_attackJumpStep ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1; } call_object_clearCollisionRegion_result;
/* register form: generated wrapper: object_clearCollisionRegion has no typed C interface yet */
static inline call_object_clearCollisionRegion_result call_object_clearCollisionRegion_ai_attackJumpStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_clearCollisionRegion);
    return (call_object_clearCollisionRegion_result){ R1 };
}

/* register form: generated wrapper: object_setCollisionRegion has no typed C interface yet */
static inline void call_object_setCollisionRegion_ai_attackJumpStep(u32 r0, u32 r5) {
    R0 = r0;
    R5 = r5;
    GUEST_CALL(object_setCollisionRegion);
}

typedef struct { u32 r1, r2, r3, r4, r6; } call_object_updateCollisionPanels_result_ai_attackJumpStep;
/* register form: generated wrapper: object_updateCollisionPanels has no typed C interface yet */
static inline call_object_updateCollisionPanels_result_ai_attackJumpStep call_object_updateCollisionPanels_ai_attackJumpStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_updateCollisionPanels);
    return (call_object_updateCollisionPanels_result_ai_attackJumpStep){ R1, R2, R3, R4, R6 };
}

typedef struct { u32 r3; } call_object_removePanelReserve_result_ai_attackJumpStep;
/* register form: generated wrapper: object_removePanelReserve has no typed C interface yet */
static inline call_object_removePanelReserve_result_ai_attackJumpStep call_object_removePanelReserve_ai_attackJumpStep(u32 r0, u32 r1, u32 r5) {
    R0 = r0;
    R1 = r1;
    R5 = r5;
    GUEST_CALL(object_removePanelReserve);
    return (call_object_removePanelReserve_result_ai_attackJumpStep){ R3 };
}

typedef struct { u32 r1, r2; bool c, v; } call_object_setCoordinatesFromPanels_result_ai_attackJumpStep;
/* register form: generated wrapper: object_setCoordinatesFromPanels has no typed C interface yet */
static inline call_object_setCoordinatesFromPanels_result_ai_attackJumpStep call_object_setCoordinatesFromPanels_ai_attackJumpStep(u32 r5) {
    R5 = r5;
    GUEST_CALL(object_setCoordinatesFromPanels);
    return (call_object_setCoordinatesFromPanels_result_ai_attackJumpStep){ R1, R2, CARRY_FLAG(), OVERFLOW_FLAG() };
}

/* Advance the jumping object along its velocity, manage its collision region
 * while airborne or landed, and finalize the hop when the timer expires.
 * Returns r1: either the threshold 0x100000 or the value left by the
 * collision/coordinate update call that actually ran. */
static u32 ai_attack_jump_step(u32 obj, u32 ai) {
    /* Integrate velocity into position on each axis. */
    u32 xv = LD32(obj + oBattleObject_XVelocity);
    u32 new_x = LD32(obj + oBattleObject_X) + xv;
    ST32(obj + oBattleObject_X, new_x);

    u32 yv = LD32(obj + oBattleObject_YVelocity);
    u32 new_y = LD32(obj + oBattleObject_Y) + yv;
    ST32(obj + oBattleObject_Y, new_y);

    u32 zv = LD32(obj + oBattleObject_ZVelocity);
    u32 new_z = LD32(obj + oBattleObject_Z) + zv;
    ST32(obj + oBattleObject_Z, new_z);

    /* Damp vertical velocity by the AI hop deceleration value. */
    u32 zdamp = LD32(ai + oAIAttackVars_Unk_40);
    ST32(obj + oBattleObject_ZVelocity, zv - zdamp);

    u32 cd_ptr = LD32(obj + oBattleObject_CollisionDataPtr);
    u32 region = LD8(cd_ptr + oCollisionData_Region);

    u32 r1 = (u32)0x10 << 16;  /* landing threshold: 0x100000 */

    if ((s32)new_z < (s32)r1) {
        /* Still airborne: activate collision if not yet set. */
        if (region == 0) {
            call_object_setCollisionRegion_ai_attackJumpStep(1, obj);
            ST8(obj + oBattleObject_PanelX,
                LD8(obj + oBattleObject_FuturePanelX));
            ST8(obj + oBattleObject_PanelY,
                LD8(obj + oBattleObject_FuturePanelY));
            r1 = call_object_updateCollisionPanels_ai_attackJumpStep(obj).r1;
        }
    } else {
        /* At/above landing height: deactivate collision if active. */
        if (region != 0) {
            r1 = call_object_clearCollisionRegion_ai_attackJumpStep(obj).r1;
        }
    }

    /* Decrement the hop step timer. */
    u32 timer = LD16(ai + oAIAttackVars_StepTimer);
    u32 next_timer = timer - 1;
    ST16(ai + oAIAttackVars_StepTimer, next_timer);

    if ((s32)next_timer < 0) {
        /* Hop complete: snap to destination panel and enter landing state. */
        u32 fx = LD8(obj + oBattleObject_FuturePanelX);
        u32 fy = LD8(obj + oBattleObject_FuturePanelY);
        ST8(obj + oBattleObject_PanelX, fx);
        ST8(obj + oBattleObject_PanelY, fy);

        call_object_removePanelReserve_ai_attackJumpStep(fx, fy, obj);
        r1 = call_object_setCoordinatesFromPanels_ai_attackJumpStep(obj).r1;

        ST32(obj + oBattleObject_Z, 0);
        ST16(ai + oAIAttackVars_StepTimer, 0);
        ST8(obj + oBattleObject_CurAnim, 2);
        ST16(ai + oAIAttackVars_HitElement, 0xC);
    }

    return r1;
}

/* ai_attackJumpStep(r5 = obj, r7 = ai) -> r1 */
Next ai_attackJumpStep(void) {
    R1 = ai_attack_jump_step(R5, R7);
    RETURN_TO_CALLER();
}

/* ── battle_spawnCollisionRegionAtObjectPanel_810C14A ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r5; } call_object_spawnCollisionRegion_result_sub_810C14A;
/* register form: generated wrapper: object_spawnCollisionRegion has no typed C interface yet */
static inline call_object_spawnCollisionRegion_result_sub_810C14A call_object_spawnCollisionRegion_sub_810C14A(u32 r0, u32 r1, u32 r2, u32 r3, u32 r4, u32 r5, u32 r6, u32 r7) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    GUEST_CALL(object_spawnCollisionRegion);
    return (call_object_spawnCollisionRegion_result_sub_810C14A){ R5 };
}

/* Spawn the standard collision region in front of a battle object.
 *
 * r5 on entry points at the battle object whose panel coordinates seed the
 * region. Everything else the callee needs is constant for every caller of
 * this helper: the shape/extent word 0x3205FF83 (bytes 83 FF 05 32 at
 * byte_810C164), region width 5, offset 0, no extra object, and the
 * 0x4600 kind/mode word built in the asm (mov r7,#0x46; lsl r7,#8).
 *
 * object_spawnCollisionRegion answers with the updated object pointer in r5,
 * which this routine passes through to its own caller. r7 is likewise passed
 * through unchanged (it is listed as an output but the original's push/pop
 * restores the entry value), and r4/r6 are preserved the same way - the
 * callee wrapper leaves the call arguments (0x3205FF83 / 0) in guest r4/r6,
 * so the adapter restores the entry values after the call.
 */

/* Result registers of battle_spawnCollisionRegionAtObjectPanel_810C14A: updated object pointer, plus the
 * pass-through values for the registers the original leaves untouched. */
typedef struct {
    u32 object;   /* r5 out - object pointer as updated by the callee */
    u32 r4;       /* preserved: entry value */
    u32 r6;       /* preserved: entry value */
    u32 r7;       /* r7 out - entry value (the 0x4600 constant lives only inside the call) */
} CollisionRegionSpawn;

static CollisionRegionSpawn spawn_collision_region_for_object(u32 object,
                                                              u32 savedR4,
                                                              u32 savedR6,
                                                              u32 savedR7)
{
    /* Panel position of the object the region is spawned for. */
    u32 panelX = LD8(object + oBattleObject_PanelX);
    u32 panelY = LD8(object + oBattleObject_PanelY);

    call_object_spawnCollisionRegion_result_sub_810C14A spawned =
        call_object_spawnCollisionRegion_sub_810C14A(panelX, panelY, 5, 0,
                                         0x3205FF83, object, 0, 0x4600);

    CollisionRegionSpawn out;
    out.object = spawned.r5;
    out.r4 = savedR4;
    out.r6 = savedR6;
    out.r7 = savedR7;
    return out;
}

/* battle_spawnCollisionRegionAtObjectPanel_810C14A(r5 = object) -> r5 = updated object, r7 unchanged (r4/r6 preserved) */
Next battle_spawnCollisionRegionAtObjectPanel_810C14A(void) {
    CollisionRegionSpawn out = spawn_collision_region_for_object(R5, R4, R6, R7);
    R4 = out.r4;
    R5 = out.object;
    R6 = out.r6;
    R7 = out.r7;
    RETURN_TO_CALLER();
}
