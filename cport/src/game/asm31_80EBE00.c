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
 *   +0 -> sub_80EBE20, +4 -> battle_aiBusterAttackTick_80EBE54, +8 -> sub_80EBEB2
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
        R1 = LD32(CHARGE_ATTACK_TABLE + 4); /* battle_aiBusterAttackTick_80EBE54 (thumb) */
        GUEST_CALL(battle_aiBusterAttackTick_80EBE54);
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

/* ── battle_aiBusterAttackTick_80EBE54 ── */
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

/* ── sub_80EC350 ── */
/* sub_80EC350(r5 = battle object ptr, r7 = AIAttackVars ptr)
 * AI attack handler: computes the attack outcome via sub_80E192C, and if the
 * opposing alliance slot is empty, optionally invokes sub_800BF16 to trigger
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

    u32 unk02  = LD8(aiVars + oAIAttackVars_Unk_02);
    u32 unk03  = LD8(aiVars + oAIAttackVars_Unk_03);
    u32 unk0c  = LD32(aiVars + oAIAttackVars_Unk_0c);
    u32 damage = LD32(aiVars + oAIAttackVars_Damage);

    /* atkBoost (u16) goes into the high half of r7; unk14 (u16) is the low half. */
    u32 atkBoost = ((u32)LD16(aiVars + oAIAttackVars_AttackBoost)) << 16;
    u32 unk14    = (u32)LD16(aiVars + oAIAttackVars_Unk_14);
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
            u32 cur_unk14 = LD16(R7 + oAIAttackVars_Unk_14);
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
        GUEST_CALL(sub_800BF16);
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
 * oAIAttackVars_Unk_00 selects an entry of the two-word jump table at
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
    u32 selector = LD8(ai_vars + oAIAttackVars_Unk_00);
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
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        R0 = 0xE;
        GUEST_CALL(object_setAnimation);

        GUEST_CALL(sub_80EB562);

        GUEST_CALL(object_setDefaultCounterTime);

        R0 = 1u << OBJECT_FLAGS_UNK_BIT_22_BIT;
        GUEST_CALL(object_setFlag1);

        ST8(R7 + oAIAttackVars_Unk_01, 4);
        ST16(R7 + oAIAttackVars_Unk_10, 0);
    }

    /* Tick 1: spawn the thunder projectile at the caster's panel, aimed in
     * their facing direction. R7 is preserved across the spawn call via the
     * guest stack to match the original's push {r7} / pop {r7}. */
    if (LD16(R7 + oAIAttackVars_Unk_10) == 1) {
        u32 buf = SP -= 4;
        ST32(buf, R7);

        GUEST_CALL(object_getFrontDirection);
        u32 panelX = LD8(R5 + oBattleObject_PanelX);
        u32 panelY = LD8(R5 + oBattleObject_PanelY);
        R0 = R0 + panelX;

        R1 = panelY;
        R2 = LD8(R7 + oAIAttackVars_Unk_02);

        u32 unk0c    = LD32(R7 + oAIAttackVars_Unk_0c);
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
    u32 tick = LD16(R7 + oAIAttackVars_Unk_10) + 1;
    ST16(R7 + oAIAttackVars_Unk_10, tick);
    if ((s32)tick > 4) {
        ST16(R7 + oAIAttackVars_Unk_00, 4);
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
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        ST16(R7 + oAIAttackVars_Unk_10, 0x1E);
        ST8(R7 + oAIAttackVars_Unk_01, 4);
    }

    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    ST16(R7 + oAIAttackVars_Unk_10, timer - 1);

    /* BGE after SUBS r0, #1: branches when the pre-decrement value was >= 1
     * as a signed 32-bit quantity (LDRH zero-extends, so the high half is 0). */
    if ((s32)timer >= 1) {
        RETURN_TO_CALLER();
    }

    R0 = 0;
    ST32(R5 + oBattleObject_RelatedObject1Ptr, R0);
    R1 = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(R1 + oAIData_Unk_68, 0);
    GUEST_CALL(object_exitAttackState);

    RETURN_TO_CALLER();
}

/* ── sub_80EC844 ── */
/* sub_80EC844: AI attack handler. Looks up a value in a table indexed by an AI byte,
 * updates a counter on the battle object, and exits the attack state. */
Next sub_80EC844(void) {
    /* Index table at byte_80EC870 by (AI byte * 2), read the entry as a u16. */
    u8 aiByte = (u8)LD8(R7 + oAIAttackVars_Unk_03);
    u16 amount = LD16(A_byte_80EC870 + (aiByte * 2));

    /* Attempt to apply the value; result is ignored on failure. */
    R0 = amount;
    R1 = 1;
    GUEST_CALL(sub_800E2FC);
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
    u32 state   = LD8(attack_vars + oAIAttackVars_Unk_00);
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

/* ── sub_80ECA34 ── */
/* sub_80ECA34: battle-AI attack dispatcher. The AI attack state block pointed
 * to by R7 holds a selector byte (oAIAttackVars_Unk_00) used as a word offset
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
    u32 selector = LD8(R7 + oAIAttackVars_Unk_00);
    u32 handler = LD32(A_off_80ECA48 + selector);

    R0 = selector;
    R1 = handler;

    if (selector == 0)
        GUEST_CALL(sub_80ECA50);
    else
        GUEST_CALL(sub_80ECAA4);

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
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        ST16(R7 + oAIAttackVars_Unk_10, 0xA);
        ST8(R7 + oAIAttackVars_Unk_01, 4);
    }

    /* Decrement the countdown. */
    u32 countdown = LD16(R7 + oAIAttackVars_Unk_10) - 1;
    ST16(R7 + oAIAttackVars_Unk_10, (u16)countdown);

    /* While the countdown is still positive, just return (r1 unchanged). */
    if ((s32)countdown >= 0) {
        RETURN_TO_CALLER();
    }

    /* Countdown expired: clear related-object pointer on the battle object,
     * clear an AIData slot, then exit the attack state. */
    ST32(R5 + oBattleObject_RelatedObject1Ptr, 0);
    R1 = LD32(R5 + oBattleObject_AIDataPtr);
    ST32(R1 + oAIData_Unk_68, 0);
    GUEST_CALL(object_exitAttackState);

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
    u32 state = LD8(vars + oAIAttackVars_Unk_00);
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

    /* sub_80ED040() -> byte stored at Unk_16 */
    GUEST_CALL(sub_80ED040);
    ST8(R7 + oAIAttackVars_Unk_16, (u8)R0);

    ST8(R7 + oAIAttackVars_Unk_17, 3);
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

    ST8(R7 + oAIAttackVars_Unk_0e, 9);
    ST32(R7 + oAIAttackVars_Unk_30, (u32)-1);
    ST16(R7 + oAIAttackVars_Unk_00, 4);

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
 * (flipping direction and consulting sub_80ED040 for a row hint), then
 * decrements an iteration counter. Once iterations are exhausted, it
 * tears down the attack state. */

Next sub_80ECF8E(void) {
    /* r4/r6 are push-saved in the original; restore them so the caller
     * sees the values it passed in. r5 is never written in this function. */
    u32 r5 = R5;
    u32 saved_r4 = R4;
    u32 saved_r6 = R6;

    u32 state = LD8(R7 + oAIAttackVars_Unk_01);

    if (state != 1) {
        if (state != 2) {
            /* First entry: poke the AI data sub-struct, set anim 0xD. */
            u32 aiData = LD32(r5 + oBattleObject_AIDataPtr);
            u32 unk68  = LD32(aiData + oAIData_Unk_68);
            ST8(unk68 + 0x10, 1);
            R0 = 0xD;
            GUEST_CALL(object_setAnimation);
        }

        /* ---- Per-attack setup ---- */
        ST8(R7 + oAIAttackVars_Unk_01, 1);
        ST16(R7 + oAIAttackVars_Unk_10, 8);

        {
            u32 param  = (LD32(R7 + oAIAttackVars_Unk_0c) & 0xFFFFu) | 0x10000u;
            u32 col    = LD8(R7 + oAIAttackVars_Unk_16);
            u32 row    = LD8(R7 + oAIAttackVars_Unk_17);
            u32 side   = LD8(R7 + oAIAttackVars_Unk_02);
            u32 damage = LD32(R7 + oAIAttackVars_Damage);

            R0 = col;  R1 = row;  R2 = side;  R3 = 0;
            R4 = param;
            R6 = damage;
            GUEST_CALL(sub_80C5F2C);
        }

        {
            u32 col = LD8(R7 + oAIAttackVars_Unk_16);
            u32 row = LD8(R7 + oAIAttackVars_Unk_17);
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
            R1 = LD8(R7 + oAIAttackVars_Unk_0d);
            ST16(R0 + oBattleObject_Timer, R1);
        }
        /* If spawn returned NULL, R1 stays as spawn_t1_0x0_EffectObject left it,
         * matching the original which skips the ldrb in that case. */
    }
    /* state == 1: skip setup entirely, R1 = caller's input value. */

    /* ---- Per-frame timer tick ---- */
    {
        u32 orig_timer = LD16(R7 + oAIAttackVars_Unk_10);
        u32 timer = orig_timer - 1;
        ST16(R7 + oAIAttackVars_Unk_10, (u16)timer);
        SET_FLAGS_CMP(orig_timer, 1);  /* `subs r0, r0, #1` */

        if ((s32)timer >= 0) {
            goto done;  /* bge locret: still ticking */
        }
    }

    /* ---- Timer expired: bounce the target panel ---- */
    {
        u32 dir = LD32(R7 + oAIAttackVars_Unk_30);
        u32 row = LD8(R7 + oAIAttackVars_Unk_17);
        s32 probe = (s32)row + (s32)dir;

        if (probe >= 1 && probe <= 3) {
            /* Step along the column stays in range: take the step. */
            u32 new_row = (u32)probe;
            ST8(R7 + oAIAttackVars_Unk_17, (u8)new_row);
            R1 = new_row;
        } else {
            /* Out of range: flip the column-direction step and ask the
             * helper which way the row should now nudge. */
            u32 neg_dir = (u32)(-(s32)dir);
            ST32(R7 + oAIAttackVars_Unk_30, neg_dir);
            GUEST_CALL(sub_80ED040);  /* R0 = row hint */

            u32 old_col = LD8(R7 + oAIAttackVars_Unk_16);
            u32 hint    = R0;

            if (hint == old_col) {
                /* Helper says same row: bounce straight along the row. */
                u32 new_row = (u32)((s32)row - (s32)dir);
                ST8(R7 + oAIAttackVars_Unk_17, (u8)new_row);
                R1 = new_row;
            } else {
                /* Helper gives a different row: nudge toward it. */
                s32 delta = (hint > old_col) ? 1 : -1;
                u32 new_col = (u32)((s32)old_col + delta);
                ST8(R7 + oAIAttackVars_Unk_16, (u8)new_col);
                R1 = new_col;
            }
        }
    }

    /* ---- Decrement bounce iteration counter ---- */
    {
        u32 orig_lives = LD8(R7 + oAIAttackVars_Unk_0e);
        u32 lives = orig_lives - 1;
        ST8(R7 + oAIAttackVars_Unk_0e, (u8)lives);
        SET_FLAGS_CMP(orig_lives, 1);  /* `subs r0, r0, #1` for the beq below */

        if (lives != 0) {
            /* More iterations left: loop back into the setup phase. */
            ST8(R7 + oAIAttackVars_Unk_01, 2);
            /* `mov r0, #2` does not set flags in Thumb; flags stay from SUBS. */
            goto done;
        }

        /* Attack finished: drop related-object pointer, clear AI data
         * sub-struct reference, then exit the attack state. */
        u32 aiData = LD32(r5 + oBattleObject_AIDataPtr);
        ST32(r5 + oBattleObject_RelatedObject1Ptr, 0);
        ST32(aiData + oAIData_Unk_68, 0);
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

/* ai_attack_dispatch_80ED13E(r7 = AI attack variables, r2..r6/r8..r12 = context) -> all regs */
Next ai_attack_dispatch_80ED13E(void) {
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
 * its oAIAttackVars_Unk_00 byte; that index selects an entry of the jump
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
    u32 attack_index = LD8(state + oAIAttackVars_Unk_00);
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

/* ── sub_80FD548 ── */
/* sub_80FD548: AI attack follow-through timer. Counts down the attack cooldown in
 * oAIAttackVars_Unk_10; once it expires, plays the recovery animation, commits the
 * object to its planned (future) panel position and switches the AI state machine
 * to idle-ish values (Unk_10 = 4, Unk_00 = 8). Leaves r1 as the panel-update helper
 * returned it (or untouched when returning early). */
Next sub_80FD548(void) {
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10) - 1;
    ST16(R7 + oAIAttackVars_Unk_10, timer);

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

    ST16(R7 + oAIAttackVars_Unk_10, 4);
    ST16(R7 + oAIAttackVars_Unk_00, 8);
    RETURN_TO_CALLER();
}

/* ── sub_80FD574 ── */
/* sub_80FD574(r5 = battle object, r7 = AI attack vars):
 * Ticks down the AI attack timer (oAIAttackVars_Unk_10) once per call.
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

    if (LD8(ai + oAIAttackVars_Unk_01) == 0) {
        /* Attack winding down: count the timer down to zero. */
        u32 timer = (u32)LD16(ai + oAIAttackVars_Unk_10) - 1;
        ST16(ai + oAIAttackVars_Unk_10, timer);
        if ((s32)timer > 0)
            RETURN_TO_CALLER();

        /* Timer expired: restore the idle animation and movement. */
        ST8(obj + oBattleObject_CurAnim, 0);
        R0 = 0x40;
        GUEST_CALL(object_clearFlag);
        R0 = 1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
        GUEST_CALL(object_setFlag1);

        /* Restart the cooldown and switch to attack state 4. */
        ST16(ai + oAIAttackVars_Unk_10, LD16(ai + oAIAttackVars_Unk_18));
        ST8(ai + oAIAttackVars_Unk_01, 4);
        SET_FLAGS_NZ(4); /* from the original's mov r0, #4 */
        RETURN_TO_CALLER();
    } else {
        /* Attack finished: count the timer down, then exit the attack. */
        u32 timer = (u32)LD16(ai + oAIAttackVars_Unk_10) - 1;
        ST16(ai + oAIAttackVars_Unk_10, timer);
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
 * oAIAttackVars_Unk_00 is a raw byte offset into the handler table at
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
    u32 cmd = LD8(vars + oAIAttackVars_Unk_00);
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
 * Wind-up tick for an AI attack. Counts down oAIAttackVars_Unk_10; while it is
 * still positive there is nothing to do yet. When it expires, switch the object
 * to its attack animation, commit the reserved panels (FuturePanelX/Y ->
 * PanelX/Y), refresh the object's position and collision panels from them, and
 * reset the attack state (Unk_10 = 4 cooldown, Unk_00 = 8). */
Next sub_80FD638(void) {
    u32 countdown = LD16(R7 + oAIAttackVars_Unk_10) - 1;
    ST16(R7 + oAIAttackVars_Unk_10, countdown);
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

    ST16(R7 + oAIAttackVars_Unk_10, 4);
    ST16(R7 + oAIAttackVars_Unk_00, 8);
    RETURN_TO_CALLER();
}

/* ── sub_80FD664 ── */
/* sub_80FD664(r7 = attack vars object):
 * Tick down the AI attack timer (oAIAttackVars_Unk_10). While it is still
 * nonzero after the decrement, do nothing more. When it reaches zero, clear
 * the 0x40 bit on the object, mark it as currently moving (flag 1<<19) and
 * reset the attack state timer oAIAttackVars_Unk_00 to 0xC. */
Next sub_80FD664(void) {
    u32 counter = LD16(R7 + oAIAttackVars_Unk_10);

    counter -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, counter);

    /* bgt on the zero-extended halfword: only an exact 0 falls through. */
    if ((s32)counter > 0)
        RETURN_TO_CALLER();

    R0 = 0x40;
    GUEST_CALL(object_clearFlag);

    R0 = 1u << OBJECT_FLAGS_CURRENTLY_MOVING_1_BIT;
    GUEST_CALL(object_setFlag1);

    ST16(R7 + oAIAttackVars_Unk_00, 0xC);
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
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        ST8(R7 + oAIAttackVars_Unk_01, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST16(R7 + oAIAttackVars_Unk_10, 0x1E);
    } else {
        u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
        timer -= 1;
        ST16(R7 + oAIAttackVars_Unk_10, timer);
        if ((s32)timer <= 0)
            ST16(R7 + oAIAttackVars_Unk_00, 0x10);
    }
    RETURN_TO_CALLER();
}

/* ── sub_80FD6F4 ── */
/* sub_80FD6F4(r5 = battle object ptr, r7 = attack vars ptr):
 * AI attack behaviour: on first call, snapshot a timer from oAIAttackVars_Unk_18 and start
 * counting; once the timer reaches zero, clear the current animation and exit the attack
 * state. Leaves R1 as the output (unchanged here, so callers' R1 flows through). */
Next sub_80FD6F4(void) {
    u8 state = LD8(R7 + oAIAttackVars_Unk_01);
    if (state == 0) {
        /* First invocation: latch the countdown. */
        ST8(R7 + oAIAttackVars_Unk_01, 4);
        ST16(R7 + oAIAttackVars_Unk_10, LD16(R7 + oAIAttackVars_Unk_18));
    } else {
        /* Subsequent calls: tick the countdown. */
        u16 timer = LD16(R7 + oAIAttackVars_Unk_10) - 1;
        ST16(R7 + oAIAttackVars_Unk_10, timer);
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
 * r7 points to an AIAttackVars block; the byte at oAIAttackVars_Unk_00 holds
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
    R0 = step;          /* ldrb r0,[r7,#oAIAttackVars_Unk_00] */
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
    u32 step = LD8(vars + oAIAttackVars_Unk_00);
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

/* ── sub_80FD7A8 ── */
/* sub_80FD7A8: tick an AI attack object's "approach" countdown.
 *   inputs:  r5 = battle object, r7 = AI attack vars
 *   outputs: r1 = (preserved -- see comment)
 *   clobbers: nz, c, v, r0, r2, r3, lr
 *
 * Decrements oAIAttackVars_Unk_10. While it stays strictly positive (signed),
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
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);

    /* ldrh r0, [r7,#oAIAttackVars_Unk_10]; sub r0, #1; strh r0, [r7,...] */
    SET_FLAGS_CMP(timer, 1);
    timer -= 1;
    ST16(R7 + oAIAttackVars_Unk_10, timer);

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

    /* mov r0, #4; strh r0, [r7,#oAIAttackVars_Unk_10] */
    R0 = 4;
    ST16(R7 + oAIAttackVars_Unk_10, R0);

    /* mov r0, #8; strh r0, [r7,#oAIAttackVars_Unk_00] */
    R0 = 8;
    ST16(R7 + oAIAttackVars_Unk_00, R0);

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
    u16 counter = LD16(R7 + oAIAttackVars_Unk_10);
    counter--;
    ST16(R7 + oAIAttackVars_Unk_10, counter);
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

    ST16(R7 + oAIAttackVars_Unk_00, 0xC);
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
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        /* Attack just triggered: set up state, animation and timer. */
        ST8(R7 + oAIAttackVars_Unk_01, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST16(R7 + oAIAttackVars_Unk_10, 10);
    } else {
        u32 timer = LD16(R7 + oAIAttackVars_Unk_10);

        /* Fire the attack on frames where timer bit 2 is clear. */
        if (!(timer & 4)) {
            R0 = LD8(R7 + oAIAttackVars_Unk_0c);
            GUEST_CALL(sub_80FDCA4);
        }

        timer--;
        ST16(R7 + oAIAttackVars_Unk_10, timer);
        if ((s32)timer <= 0)
            ST16(R7 + oAIAttackVars_Unk_00, 0x10); /* timer ran out: set delay */
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
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        /* Begin the attack: enter the charging state, play the charge
         * animation and start a 30-frame timer. */
        ST8(R7 + oAIAttackVars_Unk_01, 4);
        ST8(R5 + oBattleObject_CurAnim, 7);
        SET_FLAGS_NZ(0x1E);
        ST16(R7 + oAIAttackVars_Unk_10, 0x1E);
        RETURN_TO_CALLER();
    }

    /* The attack is charging: fire it on frames where bit 2 of the
     * countdown is clear. */
    u32 timer = LD16(R7 + oAIAttackVars_Unk_10);
    if (!(timer & 4)) {
        R0 = LD8(R7 + oAIAttackVars_Unk_0c);
        GUEST_CALL(sub_80FDCA4);
    }

    /* Count this frame down; while the timer is still positive we're done,
     * otherwise reset the attack state. */
    SET_FLAGS_CMP(timer, 1);
    ST16(R7 + oAIAttackVars_Unk_10, timer - 1);
    if (ZERO_FLAG() || NEGATIVE_FLAG() != OVERFLOW_FLAG()) {
        SET_FLAGS_NZ(0x14);
        ST16(R7 + oAIAttackVars_Unk_00, 0x14);
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

    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        /* First tick — set up the attack. */
        ST8(R7 + oAIAttackVars_Unk_01, 4);

        /* Counter window for the attack. */
        R0 = 0x18;
        GUEST_CALL(object_setCounterTime);

        /* Trigger the caster's attack animation. */
        ST8(R5 + oBattleObject_CurAnim, 9);

        /* Build the packed descriptor (r4) that sub_80C82A8 consumes:
         *   element-id | (sub-element << 8) | (direction << 16). */
        R4  = LD8(R7 + oAIAttackVars_Unk_03);
        R4 |= LD8(A_dword_80FD8AC + LD8(R7 + oAIAttackVars_Unk_03)) << 8;
        R4 |= LD8(R7 + oAIAttackVars_Unk_0c)            << 16;

        /* Damage value goes in r6 for the callee. */
        R6 = LD32(R7 + oAIAttackVars_Damage);

        /* Spawn the hit/projectile. */
        R0 = LD8(R5 + oBattleObject_PanelX);
        R1 = LD8(R5 + oBattleObject_PanelY);
        R2 = LD8(R7 + oAIAttackVars_Unk_02);
        GUEST_CALL(sub_80C82A8);

        /* Hit SFX. */
        R0 = 0x17E;   /* 0x7F + 0xFF */
        GUEST_CALL(PlaySoundEffect);

        /* Stash sub-element index as the frame counter ticked by the
         * else-branch on subsequent calls. */
        ST16(R7 + oAIAttackVars_Unk_10,
             LD8(A_dword_80FD8AC + LD8(R7 + oAIAttackVars_Unk_03)));

        /* Asm leaves r1 = &dword_80FD8AC at exit. */
        R1 = A_dword_80FD8AC;
    } else {
        /* Wind-up animation in progress — count down its frames. */
        u32 timer = LD16(R7 + oAIAttackVars_Unk_10) - 1;
        ST16(R7 + oAIAttackVars_Unk_10, (u16)timer);
        if (!((s32)timer > 0)) {
            /* Timer reached zero: hand off to the next attack state. */
            ST16(R7 + oAIAttackVars_Unk_00, 0x18);
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
    if (LD8(R7 + oAIAttackVars_Unk_01) == 0) {
        ST8(R7 + oAIAttackVars_Unk_01, 4);
        ST8(R5 + oBattleObject_CurAnim, 0);
        ST16(R7 + oAIAttackVars_Unk_10, LD16(R7 + oAIAttackVars_Unk_18));
    } else {
        u32 timer = LD16(R7 + oAIAttackVars_Unk_10) - 1;
        ST16(R7 + oAIAttackVars_Unk_10, timer);
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

/* ── battle_aiDispatchAttackPhase_80FE0D4 ── */
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
    case 8:  GUEST_CALL(battle_aiAttack_stepGlide_80FE1B4);                       break;
    case 12: GUEST_CALL(battle_aiTickAttackTimer_80FE1FE);  break;
    }

    R7 = r7;   /* original pop {r7,pc} restores the caller's r7 */
}

/* battle_aiDispatchAttackPhase_80FE0D4 – dispatch one of four AI attack-phase handlers by jump table.
 * r7 = AIAttackVars pointer; index byte read from oAIAttackVars_Unk_00. */
Next battle_aiDispatchAttackPhase_80FE0D4(void) {
    ai_attack_phase_dispatch(R7);
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

Next battle_aiAttack_stepGlide_80FE1B4(void) {
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

/* register form: calls sub_80FE2A8, AI attack phase 2 handler */
static void call_attack_phase2(u32 state, u32 entry, u32 vars) {
    R0 = state; R1 = entry; R7 = vars;
    GUEST_CALL(sub_80FE2A8);
    R7 = vars;
}

/* register form: calls battle_aiAttackCountdownTick_80FE30C, AI attack phase 3 handler */
static void call_attack_phase3(u32 state, u32 entry, u32 vars) {
    R0 = state; R1 = entry; R7 = vars;
    GUEST_CALL(battle_aiAttackCountdownTick_80FE30C);
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

Next battle_aiAttackPhaseDispatch_80FE21E(void) {
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

/* ── battle_aiAttackCountdownTick_80FE30C ── */
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

Next battle_aiAttackCountdownTick_80FE30C(void) {
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
    u32 selector = LD8(R7);              /* byte at oAIAttackVars_Unk_00 (offset 0) */

    R0 = selector;
    if (selector == 0) {
        R1 = LD32(A_off_80FE464);        /* .word sub_80FE46C+1 */
        GUEST_CALL(sub_80FE46C);
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

    if (!(R0 & 0x40)) {
        R0 = objectLo;
        R1 = objectHi;
        GUEST_CALL(object_crackPanel);
    }

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
    R0 = LD8(R7 + oAIAttackVars_Unk_00);
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
 * sub_810986E. Otherwise clear Unk_1a and call sub_801171C.
 *
 * Inputs:  r5 = battle object ptr, r6 = ?, r7 = AI attack vars ptr
 * Outputs: standard AAPCS; preserves r4..r7. */
Next sub_8109820(void) {
    /* If the object cannot move, bail out to the failure path. */
    GUEST_CALL(object_canMove);
    if (ZERO_FLAG()) goto fail;

    /* Load candidate panel coordinates and parameters, then validate. */
    R0 = LD8(R7 + oAIAttackVars_Unk_16);
    R1 = LD8(R7 + oAIAttackVars_Unk_17);
    R2 = LD32(R7 + oAIAttackVars_Unk_34);
    R3 = LD32(R7 + oAIAttackVars_Unk_38);
    GUEST_CALL(object_checkPanelParameters);
    if (R0 == 0) goto fail;

    /* Panel OK: reload X/Y into r0/r1, write them as the future panel on
     * the battle object, then reserve the panel with those same args. */
    R0 = LD8(R7 + oAIAttackVars_Unk_16);
    ST8(R5 + oBattleObject_FuturePanelX, (u8)R0);
    R1 = LD8(R7 + oAIAttackVars_Unk_17);
    ST8(R5 + oBattleObject_FuturePanelY, (u8)R1);
    GUEST_CALL(object_reservePanel);

    R0 = OBJECT_FLAGS_CANNOT_SLIDE;
    GUEST_CALL(object_setFlag1);

    ST16(R7 + oAIAttackVars_Unk_00, 4);

    if (LD8(R7 + oAIAttackVars_Unk_03) != 0) {
        GUEST_CALL(sub_81098C4);
    } else {
        GUEST_CALL(sub_810990E);
    }
    GUEST_CALL(sub_810986E);
    RETURN_TO_CALLER();

fail:
    ST8(R7 + oAIAttackVars_Unk_1a, 0);
    GUEST_CALL(sub_801171C);
    RETURN_TO_CALLER();
}

/* ── sub_81098C4 ── */
/* sub_81098C4: drive a battle object toward the AI's target panel.
 *
 * Reads the AI's current target panel (X from oAIAttackVars_Unk_16,
 * Y from oAIAttackVars_Unk_17) and the object's current panel,
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
    s32 dy    = (s32)((u8)LD8(ai + oAIAttackVars_Unk_17))
              - (s32)((u8)LD8(obj + oBattleObject_PanelY));
    u32 speed = LD32(ai + oAIAttackVars_Unk_30);
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
    s32 dx   = (s32)((u8)LD8(ai  + oAIAttackVars_Unk_16))
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
    ST16(ai + oAIAttackVars_Unk_10, 1);
    ST16(ai + oAIAttackVars_Unk_12, 1);
    goto done;

divide_speed:
    /* BIOS Divide(r0 / r1): num = duration fixed-point, denom = speed.
     * Quotient in r0, remainder in r1; only r0 (whole frames) is kept. */
    R1 = speed;
    swi_call(6);
    ST16(ai + oAIAttackVars_Unk_10, (u16)R0);
    ST16(ai + oAIAttackVars_Unk_12, (u16)(R0 >> 1));

done:
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

/* ── sub_8109CE6 ── */
/* AI attack state-machine dispatcher (bn6f asm31.s).
 *
 * Reads the AI attack sub-state byte from the AIAttackVars struct pointed to
 * by r7 and jumps to its handler through the table at off_8109CF8:
 *   0x00 -> sub_8109D08
 *   0x04 -> sub_8109D70
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
    u32 index = LD8(attackVars + oAIAttackVars_Unk_00);
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

/* aiAttackVars_countdownSub_8109D98(r7 = AIAttackVars) -> r0, nzcv, r1, r2, lr */
Next aiAttackVars_countdownSub_8109D98(void) {
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

/* ── aiAttackDispatch_8109DD2 ── */
/* AI attack dispatch: selects one of two handlers using the attack-var type
 * byte stored at the head of the AIAttackVars struct referenced by self. */

/* register form: full-register virtual dispatch — both handlers (sub_8109DEC
 * and aiAttackTimer_tick_8109E4A) receive and return every CPU register, so C parameter
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
        GUEST_CALL(aiAttackTimer_tick_8109E4A);
    R6 = saved_r6;
}

Next aiAttackDispatch_8109DD2(void) {
    ai_attack_dispatch_sub_8109DD2(R7);
    RETURN_TO_CALLER();
}

/* ── aiAttackTimer_tick_8109E4A ── */
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

/* aiAttackTimer_tick_8109E4A(r4, r5, r6, r7, lr) -> nz c v r0 r1 r2 r3 r4 r6 r7 lr */
Next aiAttackTimer_tick_8109E4A(void) {
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
