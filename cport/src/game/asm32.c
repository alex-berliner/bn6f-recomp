/*
 * asm32.c: readable functions of bn6f asm32.s, up to 0x08119438, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

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

/* ── battle_packObjStateBitToAttrCode_8113154 ── */
/* battle_packObjStateBitToAttrCode_8113154(r4 = battle object, r6 = destination byte):
 * Packs the object's state bit at offset 0x16 (bit 0 only) into a small
 * attribute code: 0 -> 4, 1 -> 8, and stores it to [r6]. Sets r1 = 1 and
 * leaves the flags from the final add. */
Next battle_packObjStateBitToAttrCode_8113154(void) {
    u32 code = (LD8(R4 + 0x16) & 1) << 2;

    R1 = 1;
    SET_FLAGS_ADD(code, 4);
    code = code + 4;

    ST8(R6, code);
    R0 = code;
    RETURN_TO_CALLER();
}

/* ── battle_initAttackFromPreset_8113162 ── */
/* battle_initAttackFromPreset_8113162: initialize an attack on a battle object.
 * r4 = object supplying the attack index (its +0x16 byte selects one preset),
 * r5 = context object whose +0x13 state byte must pass battle_pickRandomRowObjectFields_810971A's check,
 * r7 = object whose attack fields are (re)initialized from the preset tables.
 * Returns early without touching r7 if the check fails; flags on that path come
 * from the "state == 0" comparison, otherwise from object_setAttack0. */
Next battle_initAttackFromPreset_8113162(void) {
    R0 = LD8(R5 + 0x13);
    GUEST_CALL(battle_pickRandomRowObjectFields_810971A);

    /* cmp r0, #0 from the original: caller may branch on these flags. */
    SET_FLAGS_CMP(R0, 0);
    if (R0 == 0)
        RETURN_TO_CALLER();

    /* Attack preset selected by the attacker's +0x16 index. */
    u32 preset = LD8(R4 + 0x16);

    /* Per-preset stat tables: power, element/type, hit behavior, etc. */
    ST8 (R7 + 0xF,  LD8 (A_byte_81130DE + preset));
    ST8 (R7 + 0xE,  LD8 (A_byte_81130D8 + preset));
    ST8 (R7 + 0xC,  LD8 (A_byte_81130C6 + preset));
    ST8 (R7 + 0x18, LD8 (A_byte_81130D2 + preset));
    ST16(R7 + 0x8,  LD8 (A_byte_81130C0 + preset));

    /* Fixed width of 10 for this attack. */
    ST16(R7 + 0xA, 10);

    /* Per-preset word-sized field (e.g. sprite/animation pointer). */
    R1 = preset << 2;
    ST32(R7 + 0x30, LD32(A_byte_81130A8 + R1));

    R0 = 10;
    GUEST_CALL(object_setAttack0);
    RETURN_TO_CALLER();
}

/* ── petMenu_updateObject_81169B0 ── */
/* petMenu_updateObject_81169B0: per-frame update of one PET menu object (object pointer in r5).
 *
 * The object's mode byte at (r5 + 9) selects an entry of the three-entry
 * handler table off_81169CC by byte offset (0, 4 or 8). The chosen handler
 * runs with r7 = ePETMenuData and r1 = the mode byte, and returns to
 * 0x81169BE (the "mov lr, pc; bx r0" idiom), i.e. straight into the
 * sprite update below. Afterwards the object's sprite is refreshed with
 * sprite_update, then r7 is restored and the function returns to its own
 * caller via pop {r7,pc} -- which resumes at the lr saved on entry, NOT at
 * the link register left behind by the internal calls. */
Next petMenu_updateObject_81169B0(void) {
    u32 returnAddr = LR;    /* push {r7,lr}: pop {pc} returns here */
    u32 savedR7 = R7;

    R7 = A_ePETMenuData;

    /* ldrb r1,[r5,#9]; ldr r0,[table,r1]; mov lr,pc; bx r0 */
    u32 mode = LD8(R5 + 9);
    R1 = mode;
    LR = 0x81169BE;                      /* handlers return into the bl below */
    R0 = LD32(A_off_81169CC + mode);     /* table entry: handler address | 1 */

    switch (mode) {
    case 0: GUEST_CALL(npc_tickDisappearTimer_81169D8); break;
    case 4: GUEST_CALL(battle_reloadObjectSprite_8116A04); break;
    case 8: GUEST_CALL(sub_8116A28); break;
    default: break;  /* unreachable: the mode byte is always 0, 4 or 8 */
    }

    /* bl sprite_update */
    LR = 0x81169C2 | 1u;
    GUEST_CALL(sprite_update);

    /* pop {r7,pc}: restore the caller's r7 and return to the address that
     * was saved on entry, not to sprite_update's return link. */
    R7 = savedR7;
    LR = returnAddr;
    RETURN_TO_CALLER();
}

/* ── npc_tickDisappearTimer_81169D8 ── */
/* npc_tickDisappearTimer_81169D8: run one step of an object's exit/disappear sequence.
 * While the signed 16-bit flag at r7+0x10 is set, do nothing. Otherwise
 * count the byte timer at r5+0xC down; when it reaches zero, put the
 * object into state 4 with animation id 0x23 and reload its sprite
 * (animation 1, no shadow).
 *
 * r4/r5 are preserved; r2/r3 are never touched. r0/r1 keep the values
 * the original leaves behind on the early-exit paths. */
Next npc_tickDisappearTimer_81169D8(void) {
    /* Nothing to do while the +0x10 flag is set. (r0 holds the 0x10
     * offset the original computes before the load.) */
    R0 = 0x10;
    s32 flag = LDS16(R7 + 0x10);
    R1 = (u32)flag;
    SET_FLAGS_NZ(flag);
    if (flag != 0)
        RETURN_TO_CALLER();

    /* Tick the timer down; still busy until it hits zero. */
    u32 timer = LD8(R5 + 0xC);
    R0 = timer - 1;
    ST8(R5 + 0xC, timer - 1);
    SET_FLAGS_CMP(timer, 1);
    if (timer != 1)
        RETURN_TO_CALLER();

    /* Timer expired: switch to the exit state and reset the sprite. */
    ST8(R5 + 9, 4);
    ST8(R5 + 0, 0x23);
    R0 = 1;
    GUEST_CALL(sprite_setAnimation);
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    RETURN_TO_CALLER();
}

/* ── battle_reloadObjectSprite_8116A04 ── */
/* battle_reloadObjectSprite_8116A04 (r5 = battle object):
 * Refresh the object's sprite from its current frame parameters.
 * If the frame parameters report that the animation must be (re)loaded
 * (bits 0xC0 set), restart it: select animation 0, load its data, remove
 * the shadow, run the follow-up setup in sub_8116A44, and mark the object
 * state byte at +9 as 8 (also leaving that value in r0). Flags are left
 * from the last flag-setting instruction (TST r0, 0xC0 on the skip path,
 * MOV r0, #8 otherwise). */
Next battle_reloadObjectSprite_8116A04(void) {
    GUEST_CALL(sprite_getFrameParameters);
    u32 frameParams = R0;

    /* mov r1, #0xC0 happens before the branch, so R1 is 0xC0 on exit
     * even when the reload is skipped. */
    R1 = 0xC0;

    if ((frameParams & 0xC0) == 0) {
        /* tst r0, r1 -> Z set, nothing to do */
        SET_FLAGS_NZ(frameParams & 0xC0);
        RETURN_TO_CALLER();
    }

    R0 = 0;
    GUEST_CALL(sprite_setAnimation);        /* sprite_setAnimation(0) */
    GUEST_CALL(sprite_loadAnimationData);
    GUEST_CALL(sprite_noShadow);
    GUEST_CALL(sub_8116A44);

    /* obj->state = 8 (MOV #8 is flag-setting in Thumb); r0 ends as 8 too,
     * regardless of what sub_8116A44 left in it. */
    R0 = 8;
    SET_FLAGS_NZ(8);
    ST8(R5 + 9, 8);
    RETURN_TO_CALLER();
}

/* ── sub_8116A28 ── */
/* sub_8116A28: conditionally fix up a state byte, then continue into sub_8116A44.
 * If the counter byte pointed to by r7 has reached 8, the byte at r5+8 is also
 * set to 8 (both are kept in sync). Flags/results come from sub_8116A44. */
Next sub_8116A28(void) {
    if (LD8(R7) == 8)
        ST8(R5 + 8, 8);

    GUEST_CALL(sub_8116A44);
    RETURN_TO_CALLER();
}

/* ── sprite_dispatchVariantUpdate_8116B9C ── */
/* sprite_dispatchVariantUpdate_8116B9C(r5 = sprite/object pointer):
 * Run the per-variant update routine selected by the byte at [r5+9], then
 * advance the sprite animation. The selector byte indexes a 3-entry word
 * table, so it only ever holds 0, 4 or 8 (byte offsets into the table).
 * All registers except r0/r1 (clobbered by the call) are passed through. */
Next sprite_dispatchVariantUpdate_8116B9C(void) {
    switch (LD8(R5 + 9)) {
    case 0: GUEST_CALL(sprite_fallingObjectStepDown_8116BC0); break; /* off_8116BB4[0] */
    case 4: GUEST_CALL(sub_8116BFC); break; /* off_8116BB4[1] */
    case 8: GUEST_CALL(sprite_scrollStepAndCullOffscreen_8116C14); break; /* off_8116BB4[2] */
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── sprite_fallingObjectStepDown_8116BC0 ── */
/* sprite_fallingObjectStepDown_8116BC0(r5 = falling object state):
 * Move a falling object 8 pixels further down, then check it against the
 * stop depth selected by the state byte at [r5+4] (an index into the
 * halfword table at byte_8116BEC: 0x20, 0x30, ... 0x90). While the object
 * is still above its stop depth, nothing else happens; once it has reached
 * or passed it, snap its coordinate to the stop depth and store phase 4
 * into the state byte at [r5+9].
 *
 * Register results: r0/r1 = the coordinates read back (r0 becomes 4 when
 * the object settles), r2 = state byte * 2, r3 = the table entry
 * (or whatever sprite_setCoordinates leaves in r3 when it is called). */
Next sprite_fallingObjectStepDown_8116BC0(void) {
    R0 = 0;
    R1 = 8;
    GUEST_CALL(sprite_addCoordinates);   /* move down by (0, 8) */
    GUEST_CALL(sprite_getCoordinates);   /* r0 = x, r1 = y */

    R2 = LD8(R5 + 4) << 1;               /* state byte -> halfword offset */
    R3 = LD16(A_byte_8116BEC + R2);      /* stop depth for this state */

    SET_FLAGS_CMP(R3, R1);
    if ((s32)R3 <= (s32)R1) {            /* depth reached: settle here */
        R1 = R3;
        GUEST_CALL(sprite_setCoordinates);
        R0 = 4;
        ST8(R5 + 9, R0);                 /* advance to phase 4 */
        SET_FLAGS_NZ(R0);
    }
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

/* ── sprite_scrollStepAndCullOffscreen_8116C14 ── */
/* sprite_scrollStepAndCullOffscreen_8116C14(r7 = sprite):
 * Nudge the sprite along one axis by 8x the signed value stored at
 * sprite+0x10, then look at the resulting coordinate. If it has moved
 * past zero into negative territory, clear the sprite's scale counter
 * and mark it unscalable (it has scrolled off / shrunk away).
 *
 * Outputs: r0/r1 hold the coordinates returned by sprite_getCoordinates;
 * flags come from the CMP against that coordinate, or (when the reset
 * call is made) from whatever that callee leaves behind. */
Next sprite_scrollStepAndCullOffscreen_8116C14(void) {
    s32 offset = LDS16(R7 + 0x10);

    R0 = (u32)offset << 3;
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    GUEST_CALL(sprite_getCoordinates);
    SET_FLAGS_CMP(R0, 0);

    if ((s32)R0 < 0)
        GUEST_CALL(sprite_clearScaleCounterAndMakeUnscalable_804668A);

    RETURN_TO_CALLER();
}

/* ── record_adjustNthSubRecordFieldC_8116C40 ── */
/* record_adjustNthSubRecordFieldC_8116C40: adjust the 16-bit value at +0xC of an Nth sub-record by +/-2,
 * clamped to 0x16..0x1E.
 *
 * The sub-record pointer comes from record_getNthSubRecordPtr_8116CD4 (called
 * with the caller's registers untouched; it picks the record itself).
 * Direction depends on the bytes at [r7+4] and [r5+4]:
 *   equal     -> increase by 2, clamped so the value never exceeds 0x1E
 *   different -> decrease by 2, clamped so the value never goes below 0x16
 *
 * inputs:  r4 r5 r6 r7 lr
 * outputs: r4 r5 r6 (unchanged)
 * clobbers: nz c v r0 r1 r2 r3 lr
 */
Next record_adjustNthSubRecordFieldC_8116C40(void) {
    GUEST_CALL(record_getNthSubRecordPtr_8116CD4);
    u32 record = R0;

    s32 limit = 0x16;
    s32 step = -2;
    u32 raise = 0;

    u32 lhs = LD8(R7 + 4);
    u32 rhs = LD8(R5 + 4);
    if (lhs == rhs) {
        limit = 0x1E;
        step = 2;
        raise = 1;
    }

    s32 value = (s32)LD16(record + 0xC) + step;

    if (raise) {
        if (value >= limit)
            value = limit;
    } else {
        if (value <= limit)
            value = limit;
    }

    ST16(record + 0xC, (u32)value);
    RETURN_TO_CALLER();
}

/* ── record_getNthSubRecordPtr_8116CD4 ── */
/* record_getNthSubRecordPtr_8116CD4(r5 = record pointer): returns r5 + 16 * (high nibble of byte at r5+2),
 * i.e. selects one of sixteen 16-byte sub-records within the record. */
Next record_getNthSubRecordPtr_8116CD4(void) {
    u32 kind = (LD8(R5 + 2) >> 4) << 4;
    R0 = R5 + kind;
    RETURN_TO_CALLER();
}

/* ── sub_8116D00 ── */
/* Object offsets for the battle sprite record pointed to by R5 */
#define oBattleSprite_Type       0x00   /* class/type ID byte */
#define oBattleSprite_Animation  0x04   /* animation index */
#define oBattleSprite_Variant    0x05   /* if non-zero, use alternate coordinates */
#define oBattleSprite_State      0x08   /* current behavior step */
#define oBattleSprite_SubState   0x09   /* y-offset sub-state */
#define oBattleSprite_AnIM1      0x0C   /* animation ID primary slot */
#define oBattleSprite_AnIM2      0x0D   /* animation ID secondary slot */

/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void call_initGuiSprite_sub_8116D00(u32 sprite_id, u32 gui_arg) {
    R0 = sprite_id;
    R1 = gui_arg;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation_sub_8116D00(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_sprite_loadAnimationData_sub_8116D00(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_sprite_noShadow_sub_8116D00(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void call_sprite_setSubEntryPaletteBank_sub_8116D00(u32 bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_sprite_setPalette_sub_8116D00(u32 pal) {
    R0 = pal;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_sprite_setPriority_sub_8116D00(u32 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void call_slotList_storeHalfwordAndClearField(u32 slot) {
    R0 = slot;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void call_sprite_setSubRecordFieldA_sub_8116D00(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void call_sprite_setCoordinates_sub_8116D00(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

/* register form: calls battle_objectRunBehaviorStep_8116D6C outside this file, which has no typed C interface yet */
static void call_battle_objectRunBehaviorStep(u32 sub_offset) {
    R2 = sub_offset;
    GUEST_CALL(battle_objectRunBehaviorStep_8116D6C);
}

/* Initialize a battle sprite object (type 0x23) with its animation, palette, and
 * slot settings, then immediately advance to the next behavior step.
 * gui_arg: second argument to initGuiSprite (passed through from R1).
 * obj: guest pointer to the battle sprite record (R5). */
static void init_battle_sprite_and_run_step(u32 gui_arg, u32 obj) {
    /* Write the sprite class/type ID */
    ST8(obj + oBattleSprite_Type, 0x23);

    /* Initialize GUI sprite slot 2, passing the caller's R1 context */
    call_initGuiSprite_sub_8116D00(2, gui_arg);

    /* Copy animation ID from offset 4 into the two animation slots at +0xC and +0xD */
    u8 anim_id = (u8)LD8(obj + oBattleSprite_Animation);
    ST8(obj + oBattleSprite_AnIM1, anim_id);
    ST8(obj + oBattleSprite_AnIM2, anim_id);

    /* Configure sprite rendering: animation, data load, no shadow */
    call_sprite_setAnimation_sub_8116D00(anim_id);
    call_sprite_loadAnimationData_sub_8116D00();
    call_sprite_noShadow_sub_8116D00();

    /* Palette and priority setup */
    call_sprite_setSubEntryPaletteBank_sub_8116D00(0xC);
    call_sprite_setPalette_sub_8116D00(0);
    call_sprite_setPriority_sub_8116D00(0);

    /* Reserve an OAM/sprite slot and initialize sub-record field */
    call_slotList_storeHalfwordAndClearField(0x38B);
    call_sprite_setSubRecordFieldA_sub_8116D00(0);

    /* Set behavior state to step 4 (active) */
    ST8(obj + oBattleSprite_State, 4);

    /* Choose coordinates based on variant flag at offset 5 */
    u32 x_coord;
    u32 y_offset;
    if ((u8)LD8(obj + oBattleSprite_Variant) != 0) {
        x_coord = 0xAA;
        y_offset = 4;
    } else {
        x_coord = 0x11A;
        y_offset = 0;
    }
    ST8(obj + oBattleSprite_SubState, y_offset);

    /* Position the sprite on screen */
    call_sprite_setCoordinates_sub_8116D00(x_coord, 0x97);

    /* Tail-advance: run the next behavior step (sets r0-r3, flags, possibly r5) */
    call_battle_objectRunBehaviorStep(y_offset);
}

/* sub_8116D00(r5 = battle sprite ptr, r1 = gui arg) */
Next sub_8116D00(void) {
    init_battle_sprite_and_run_step(R1, R5);
    RETURN_TO_CALLER();
}

/* ── battle_objectRunBehaviorStep_8116D6C ── */
/* battle_objectRunBehaviorStep_8116D6C: run one step of the battle object at R5, then refresh its sprite.
 *
 * [R5+9] holds a byte offset into a small table of behavior handlers
 * (off_8116D84: 0 -> battle_moveSpriteLeftUntilX170_8116D90, 4 -> petMenu_updateSpriteVisibility_8116DB4, 8 -> battle_stepForwardAnimWait_8116DEC).
 * The original performs an indirect `mov lr, pc; bx r0` call, so each
 * handler returns straight back here; afterwards the object's sprite
 * animation is advanced by sprite_update before returning to the caller.
 * All other registers (R2..R12) are simply passed through to the handler. */
Next battle_objectRunBehaviorStep_8116D6C(void) {
    switch (LD8(R5 + 9)) {
    case 0: GUEST_CALL(battle_moveSpriteLeftUntilX170_8116D90); break;
    case 4: GUEST_CALL(petMenu_updateSpriteVisibility_8116DB4); break;
    case 8: GUEST_CALL(battle_stepForwardAnimWait_8116DEC); break;
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── battle_moveSpriteLeftUntilX170_8116D90 ── */
/* battle_moveSpriteLeftUntilX170_8116D90(r5 = battle object):
 * Drift the object's sprite 16 px to the left; once its x coordinate has
 * reached 0xAA or below, clamp it to exactly 0xAA and advance the object's
 * state byte to 4. r2 is left holding the 0xAA boundary constant on both
 * paths, matching the original. Flags reflect the last flag-setting
 * instruction (CMP on the early-out path, MOVS #4 plus callee flags
 * otherwise). */
Next battle_moveSpriteLeftUntilX170_8116D90(void) {
    R0 = -(u32)0x10;                    /* add (-16, 0) to the sprite position */
    R1 = 0;
    GUEST_CALL(sprite_addCoordinates);

    GUEST_CALL(sprite_getCoordinates);  /* r0 = x, r1 = y */

    R2 = 0xAA;                          /* boundary constant, kept in r2 */
    if ((s32)R0 > (s32)R2) {
        SET_FLAGS_CMP(R0, R2);
        RETURN_TO_CALLER();
    }

    R0 = R2;                            /* snap x back to the boundary... */
    GUEST_CALL(sprite_setCoordinates);

    R0 = 4;                             /* ...and mark the object as finished */
    SET_FLAGS_NZ(R0);
    ST8(R5 + 9, R0);
    RETURN_TO_CALLER();
}

/* ── petMenu_updateSpriteVisibility_8116DB4 ── */
/* petMenu_updateSpriteVisibility_8116DB4(r5 = sprite object):
 * Keeps a PET-menu-linked sprite (e.g. the MegaMan sprite shown on the PET)
 * in sync with the PET menu's state:
 *   - ePETMenuData+4 (menu substate) being 8 means the sprite should be shown;
 *     the show/hide flag is written to object+0xC, and whenever it changes the
 *     sprite's animation is (re)started with no shadow.
 *   - sprite_update is then run for the object.
 *   - When the menu's main state (ePETMenuData+0) has reached 8, object+0x9 is
 *     set to 8 to mark the sprite as done; r0..r3 and the nz/c/v flags are left
 *     as sprite_update / the final state comparison left them.
 * Preserves r4, r5 and the incoming r7. */
Next petMenu_updateSpriteVisibility_8116DB4(void) {
    u32 petMenuSubstate = LD8(A_ePETMenuData + 4);
    u32 showSprite = (petMenuSubstate == 8) ? 1u : 0u;

    /* Update the object's visible flag; restart the animation only on change. */
    ST8(R5 + 0xC, showSprite);
    if ((u8)LD8(R5 + 0xD) != showSprite) {
        ST8(R5 + 0xD, showSprite);
        R0 = showSprite;
        GUEST_CALL(sprite_setAnimation);      /* (u8 a1) -> void */
        GUEST_CALL(sprite_loadAnimationData); /* () -> void */
        GUEST_CALL(sprite_noShadow);          /* () -> void */
    }

    GUEST_CALL(sprite_update);                /* leaves results in r1..r3 */

    u32 petMenuState = LD8(A_ePETMenuData);
    if (petMenuState == 8) {
        /* Menu finished: flag the object with state 8. */
        R0 = 8;
        ST8(R5 + 9, 8);
        SET_FLAGS_NZ(8);
    } else {
        /* Keep the state byte in r0 and the comparison flags for the caller. */
        R0 = petMenuState;
        SET_FLAGS_CMP(petMenuState, 8);
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

/* ── battle_objectUpdateByKind_8116F34 ── */
/* Jump table jt_8116F4C: Thumb entry points of the per-object update handlers,
 * exactly as they appear in the table the original indexes through. */
enum {
    ADDR_sub_8116F60                       = 0x08116F60 + 1,
    ADDR_battle_objectUpdateWithFieldC_8116F8C = 0x08116F8C + 1,
    ADDR_sub_8116F9C                       = 0x08116F9C + 1,
};

/* battle_objectUpdateByKind_8116F34: run one battle object's type-specific update, then its sprite update.
 * R5 points at the battle object; the byte at obj+4 selects which handler runs:
 *   0/3 -> sub_8116F60, 1/4 -> battle_objectUpdateWithFieldC_8116F8C, 2 -> sub_8116F9C.
 * For fidelity with the original `mov lr, pc; bx r0`, each handler is entered with
 * R0 = its own jump-table entry and R1 = type*4 (result of the `lsl r1, r1, #2`).
 * Tail behaviour: after the handler returns, sprite_update is called on the object. */
Next battle_objectUpdateByKind_8116F34(void) {
    u32 kind = LD8(R5 + 4);

    switch (kind) {
    case 0:
    case 3:
        R0 = ADDR_sub_8116F60;
        R1 = kind << 2;
        GUEST_CALL(sub_8116F60);
        break;

    case 1:
    case 4:
        R0 = ADDR_battle_objectUpdateWithFieldC_8116F8C;
        R1 = kind << 2;
        GUEST_CALL(battle_objectUpdateWithFieldC_8116F8C);
        break;

    case 2:
        R0 = ADDR_sub_8116F9C;
        R1 = kind << 2;
        GUEST_CALL(sub_8116F9C);
        break;

    default:
        /* Unreachable: the object's update kind is always 0..4. (The original
         * would index past jt_8116F4C and jump to garbage.) */
        break;
    }

    GUEST_CALL(sprite_update);

    RETURN_TO_CALLER();
}

/* ── battle_objectUpdateWithFieldC_8116F8C ── */
/* battle_objectUpdateWithFieldC_8116F8C: update hook for the object in r5.
 * Runs the "before" update for the object, then passes the object's
 * halfword at +0xC to the "after" update routine. All register results
 * (r0-r3, flags) are simply whatever sub_81170C0 leaves behind. */
Next battle_objectUpdateWithFieldC_8116F8C(void) {
    GUEST_CALL(sub_81170AC);

    R0 = LD16(R5 + 0xC);
    GUEST_CALL(sub_81170C0);

    RETURN_TO_CALLER();
}

/* ── sub_8116FDC ── */
/* register form: calls getStructFrom2008450 outside this file, which has no typed C interface yet */
static u32 lookupPoolEntry(u32 index) {
    R0 = index;
    GUEST_CALL(getStructFrom2008450);
    if (ZERO_FLAG())
        return 0;               /* Z set: no object registered at this index */
    return R1;                  /* address of the object's struct */
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static u32 setSpriteCoordinates(u32 x, u32 y, u32 entry) {
    R0 = x;
    R1 = y;
    R5 = entry;                 /* the callee keeps working on the entry through r5 */
    GUEST_CALL(sprite_setCoordinates);
    return R5;                  /* entry pointer as the callee left it */
}

/* Move the three sprites belonging to the submenu entries starting at pool
 * index firstIndex, and set or clear each entry's "off screen" flag (bit 1
 * of its status byte).
 *
 * The selected entry (firstIndex itself) is drawn at the anchor (baseX,
 * baseY).  Its two neighbours sit 13 pixels left and right of the anchor
 * (byte_8117060 holds one {dx, dy} word pair per slot), but while their
 * halfword at +0xE is zero they crowd towards the anchor instead: 7 pixels
 * over while their counter at +0xC is still below 10, 3 pixels over after
 * that.  Bit 1 stays set unless the anchor is on screen (baseX < 0x38) and
 * the submenu id in sSubmenu is 0x18 or 8.
 *
 * r5 is clobbered by the sprite call but the original restores it on exit,
 * so it is passed in and handed back unchanged. */
static u32 moveSubmenuEntrySprites(u32 baseX, u32 baseY, u32 firstIndex, u32 r5) {
    u32 endIndex = firstIndex + 3;
    u32 slot = 0;

    for (u32 index = firstIndex; (s32)index < (s32)endIndex; index++, slot++) {
        u32 entry = lookupPoolEntry(index);
        if (entry == 0)
            continue;           /* nothing registered at this index */

        u32 xOffset;
        if (index != firstIndex && LD16(entry + 0xE) == 0) {
            /* Side entries huddle towards the selected one while it counts up. */
            xOffset = ((s32)LD16(entry + 0xC) < 10) ? 0xFFFFFFF9u : 0xFFFFFFFDu;
        } else {
            xOffset = LD32(A_byte_8117060 + slot * 8);
        }
        u32 yOffset = LD32(A_byte_8117060 + slot * 8 + 4);

        entry = setSpriteCoordinates(baseX + xOffset, baseY + yOffset, entry);

        u32 submenuId = LD8(A_sSubmenu);
        u32 status = LD8(entry);
        if ((s32)baseX >= 0x38 || (submenuId != 0x18 && submenuId != 8))
            ST8(entry, status | 2);     /* anchor off screen / wrong menu: hide */
        else
            ST8(entry, status & ~2u);   /* anchor on screen: show */
    }
    return r5;
}

/* sub_8116FDC(r0 = anchor x, r1 = anchor y, r2 = first pool index) -> r3 left
 * by the sprite callee, r5 restored, r4/r6/r7 untouched. */
Next sub_8116FDC(void) {
    R5 = moveSubmenuEntrySprites(R0, R1, R2, R5);
    RETURN_TO_CALLER();
}

/* ── sprite_setStructHalfwordPairOnThree_8117084 ── */
/* sprite_setStructHalfwordPairOnThree_8117084(r0 = halfword A, r1 = halfword B, r2 = first struct index):
 * Walks the three struct indices r2..r2+2. For each index, looks up the
 * corresponding struct from the pool at 0x2008450; while a struct is found,
 * its halfwords at +0xC and +0xE are filled with the two values passed in.
 * Stops at the first index that has no struct (lookup returns r0 == 0).
 * On a normal run-through the flags are those of the final index comparison;
 * on an early abort they are whatever the lookup left behind.
 * r4-r7 are untouched, and r1/r2 end up holding the values the last lookup
 * returned, exactly as in the original. */
Next sprite_setStructHalfwordPairOnThree_8117084(void) {
    u32 valueA = R0;
    u32 valueB = R1;
    u32 index = R2;
    u32 endIndex = index + 3;

    for (;;) {
        R0 = index;
        GUEST_CALL(getStructFrom2008450);
        if (R0 == 0)
            break; /* no struct for this index: stop, keeping the callee's flags/registers */

        u32 entry = R1; /* struct pointer returned by the lookup */
        ST16(entry + 0xC, valueA);
        ST16(entry + 0xE, valueB);
        R0 = valueB;

        index++;
        if ((s32)index >= (s32)endIndex) {
            SET_FLAGS_CMP(index, endIndex);
            break;
        }
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

/* ── sub_81176D0 ── */
/* register form: calls one of three known object getters selected by loop index;
   the callees read R5 (object pointer) and may clobber R4, so save/restore R4-R7 */
static u32 call_object_getter(u32 obj_ptr, u32 index) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R5 = obj_ptr;
    R6 = index;
    switch (index) {
    case 0: GUEST_CALL(battle_getObjectPairOddByteClamped8_811774C); break;
    case 1: GUEST_CALL(sub_8117764); break;
    default: GUEST_CALL(sub_8117768); break;
    }
    u32 result = R0;
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return result;
}

/* register form: calls sub_802FE28 outside this file, which has no typed C interface yet;
   preserves R4-R7 per the caller's contract */
static void play_sound_event(u32 sound_id, u16 packed_attr) {
    u32 s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = sound_id;
    R1 = packed_attr;
    R2 = 0;
    R3 = 3;
    GUEST_CALL(sub_802FE28);
    R4 = s4; R5 = s5; R6 = s6; R7 = s7;
}

/* Read the 32-bit base sound-ID pair from the type-indexed ROM table.
 * Table layout at byte_8117724: two u16 entries (high, low) per 4-byte slot;
 * slot selected by the object's type byte at offset 4. */
static u32 read_sound_base(u32 obj_ptr) {
    u32 off = (u32)LD8(obj_ptr + 4) * 4;
    u32 hi = LD16(A_byte_8117724 + off);
    u32 lo = LD16(A_byte_8117724 + off + 2);
    return (hi << 16) | lo;
}

/* Play three sound events for a battle object, advancing the upper half of the
 * base sound-ID by 0x80000 each iteration.  Each call packs the getter result
 * into a 16-bit attribute alongside fixed flags 0xA000 and 0x800.
 * Returns 0x80000 (the value left in R0 by the final MOV/LSL before pop). */
static u32 battle_play_object_sounds(u32 obj_ptr) {
    u32 sound_base = read_sound_base(obj_ptr);
    for (int i = 0; i < 3; i++) {
        u32 mod = call_object_getter(obj_ptr, (u32)i);
        u16 packed = (u16)(0xA800 | mod);
        play_sound_event(sound_base, packed);
        sound_base += 0x00080000;
    }
    return 0x00080000;
}

/* sub_81176D0(r5 = battle-object pointer) -> r0 = 0x80000;
 * flags from CMP r6, 3 with r6 = 3 (Z=1, N=0, C=1, V=0).
 * r4-r7 preserved (push/pop in original). */
Next sub_81176D0(void) {
    R0 = battle_play_object_sounds(R5);
    SET_FLAGS_CMP(3, 3);
    RETURN_TO_CALLER();
}

/* ── battle_getObjectPairOddByteClamped8_811774C ── */
/* battle_getObjectPairOddByteClamped8_811774C(r5 = object/state pointer):
 * Look up a byte in the table at unk_201C400. The index is taken from the
 * byte at [r5+4], doubled and incremented (i.e. the odd entry of a byte
 * pair), and the result is clamped to at most 8.
 * Returns: r0 = clamped table value, r1 = table index.
 * Flags: as if `cmp value, #8` ran; on the clamp path n/z are then
 * re-set by `mov r0, #8` (c/v keep the comparison's values). */
Next battle_getObjectPairOddByteClamped8_811774C(void) {
    u32 index = (LD8(R5 + 4) << 1) + 1;
    u32 value = LD8(A_unk_201C400 + index);

    SET_FLAGS_CMP(value, 8);
    if ((s32)value > 8) {
        value = 8;
        SET_FLAGS_NZ(value);
    }

    R0 = value;
    R1 = index;
    RETURN_TO_CALLER();
}

/* ── sub_8117764 ── */
/* sub_8117764: return 9, setting Z/N from that value. */
Next sub_8117764(void) {
    R0 = 9;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── object_dispatchModeHandler_81178D8 ── */
/* object_dispatchModeHandler_81178D8: behaviour dispatcher for the object whose pointer is in R5.
 *
 * The byte at [R5+8] is the object's mode, stored already scaled as a byte
 * offset into the handler pointer table at off_81178EC (the game only uses
 * 0, 4 or 8, which select sub_81178FC / sub_811794C / sub_8117990). The word
 * read from the table is the handler's Thumb entry address (odd). That word
 * is the call target and is also passed to the handler itself in R0, along
 * with the mode byte in R1 and the shared work-area address in R7; R2-R6 and
 * R8-R12 pass through untouched. Whatever the handler leaves in R0/R1 and
 * the flags is what the caller of this dispatcher sees.
 *
 * Note the mode byte is added to the table base *directly* (ldr r0, [r0,r1]),
 * so it must not be re-scaled here -- the loaded word, not mode/4, decides
 * which handler runs.
 */

#define HANDLER_SUB_81178FC 0x081178FDu /* = sub_81178FC + 1 (Thumb) */
#define HANDLER_SUB_811794C 0x0811794Du /* = sub_811794C + 1 (Thumb) */
#define HANDLER_SUB_8117990 0x08117991u /* = sub_8117990 + 1 (Thumb) */

Next object_dispatchModeHandler_81178D8(void) {
    u32 mode = (u8)LD8(R5 + 8);

    R7 = A_byte_2011C10;
    R1 = mode;

    /* Fetch the handler pointer exactly as the original does: the mode byte
     * is added straight to the table base, and the loaded word is both the
     * branch target and the value handed to the callee in R0. */
    u32 handler = LD32(A_off_81178EC + mode);
    R0 = handler;

    if (handler == HANDLER_SUB_81178FC) {
        GUEST_CALL(sub_81178FC);
    } else if (handler == HANDLER_SUB_811794C) {
        GUEST_CALL(sub_811794C);
    } else if (handler == HANDLER_SUB_8117990) {
        GUEST_CALL(sub_8117990);
    }
    /* Any other word can only result from reading past the three-entry table
     * (mode >= 12, which real game states never produce). In that case the
     * loaded pointer simply remains in R0 and nothing else is disturbed. */

    RETURN_TO_CALLER();
}

/* ── sub_8117998 ── */
/* sub_8117998: fetches a lookup-table pointer from sub_8047D84, then computes
 * (byte[table] - 1) * 2 + byte[r5 + 4]; r0 = the resulting offset.
 * The entry index is 1-based, hence the -1. r4-r7 and lr are only shuffled
 * through the stack in the original; the "mov r6, #0" there is dead because
 * the pop overwrites r6, so this wrapper simply preserves r4-r7. */
Next sub_8117998(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    GUEST_CALL(sub_8047D84);
    u32 table = R0;

    R0 = ((LD8(table) - 1) << 1) + LD8(savedR5 + 4);

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── dispatchStateHandler_81179AE ── */
/* dispatchStateHandler_81179AE: per-frame state dispatcher.
 *
 * Fetches the active work area through sub_8047D84 (which locates it via
 * the singleton pointer at byte_2011C10), then invokes one of four state
 * handlers depending on the state byte at the start of that work area.
 * The handlers run with r1 = state * 4 and r4 = work area; r2/r3 and
 * r5-r12 pass straight through to them. r4-r7 are preserved for the
 * caller, exactly as the original's push/pop does. */
Next dispatchStateHandler_81179AE(void) {
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    R7 = A_byte_2011C10;
    GUEST_CALL(sub_8047D84);

    u32 work = R0;
    u32 state = LD8(work);

    R1 = state << 2;
    R4 = work;
    switch (state) {
    case 0: GUEST_CALL(sub_81179E0); break;
    case 1: GUEST_CALL(keyItemMenu_renderDescription_81179E4); break;
    case 2: GUEST_CALL(sub_8117A14); break;
    default: GUEST_CALL(sub_8117AB4); break; /* state 3; larger values are unreachable */
    }

    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── keyItemMenu_renderDescription_81179E4 ── */
/* keyItemMenu_renderDescription_81179E4: render the description of the selected key item.
 * r4 = key item menu state (halfword at +2 is the item id),
 * r7 = menu context ([r7+0x24] selects which gfx block to render),
 * r10 = toolkit. The result of the key item lookup is stored into the
 * toolkit's unk_200A220 work struct. */
Next keyItemMenu_renderDescription_81179E4(void) {
    /* original push {r4-r7,lr}: r4-r7 are read here and restored on exit */
    u32 savedR4 = R4, savedR5 = R5, savedR6 = R6, savedR7 = R7;

    /* look up the key item (id in the halfword at menu_state+2) */
    R0 = LD16(R4 + 2);
    GUEST_CALL(CheckKeyItem);

    /* remember the lookup result in the unk_200A220 work struct */
    u32 work = LD32(R10 + oToolkit_Unk200a220_Ptr);
    ST32(work + 8, R0);

    /* render the item's text graphic:
     * r0 = source selector from the menu context, r1 = 0x12,
     * r2 = text buffer, r3 = VRAM destination,
     * r4/r5 = tile layout, r6 = source data, r7 = 0 */
    R0 = LD32(R7 + 0x24);
    R1 = 0x12;
    R2 = A_unk_2016200;
    R3 = 0x6013520;
    R4 = 2;
    R5 = 1;
    R6 = A_dword_86A5D60;
    R7 = 0;
    GUEST_CALL(renderTextGfx_8045F8C);

    /* pop {r4-r7} */
    R4 = savedR4;
    R5 = savedR5;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_8117A14 ── */
/* Folder screen, selected-entry redraw: render the four code letters of the
 * chip shown at *entry (chip id halfword at +2) into the 0x200-byte tile
 * buffer at unk_2016300, then draw the per-code "copies in folder" tallies
 * by scanning the folder, and queue the finished tiles to VRAM at 0x6013B40.
 *
 * The drawing callees keep the tile-buffer write cursor in guest r6: each one
 * advances it and hands it back, so the cursor is threaded from call to call.
 * They also read guest r4 (the chip id whose own folder entry is skipped when
 * counting) and r5 (the chip's database entry), so those registers are loaded
 * around the calls exactly as the original does. r4-r7 themselves are
 * preserved outputs of this function: the original saves/restores them, so
 * the entry values are passed in and handed back untouched. */

#define CODE_TILE_BUFFER      A_unk_2016300
#define CODE_TILE_BUFFER_SIZE 0x200u      /* 0x20 << 4 in the original */
#define CODE_TILE_VRAM_DEST   0x6013B40u

/* r0-r3 exactly as the final callee left them; callers of the original see
 * these verbatim (r4-r7 are preserved and returned separately). */
struct GfxCalleeTail {
    u32 r0, r1, r2, r3;
};

/* Everything the caller observes after the call: r0-r3 from the last callee,
 * r4-r7 unchanged (the original push/pop {r4-r7}). */
struct FolderCodeResult {
    u32 r0, r1, r2, r3;
    u32 r4, r5, r6, r7;
};

/* register form: calls ZeroFillByEightWords outside this file, which has no typed C interface yet */
static void clear_code_tile_buffer(u32 dest, u32 size) {
    R0 = dest;
    R1 = size;
    GUEST_CALL(ZeroFillByEightWords);
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_8117A14(u32 which_chip) {
    R0 = which_chip;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: sub_8117A84 draws one code letter at the r6 tile cursor and
 * hands back the advanced cursor in r6; it has no typed C interface yet */
static u32 draw_code_letter(u32 code, u32 chip_data, u32 cursor) {
    R0 = code;
    R5 = chip_data;     /* callee sees the chip's database entry in r5 */
    R6 = cursor;
    GUEST_CALL(sub_8117A84);
    return R6;
}

/* register form: calls folder_CountChipsOfCodeOrSkipEntry_8117AA0 outside this file,
 * which has no typed C interface yet; it counts folder entries matching the
 * code (skipping the r4 chip id's own entry) and draws the tally at the r6
 * cursor, handing back the advanced cursor */
static u32 count_folder_chips(u32 chip_id, u32 code, u32 chip_data, u32 cursor) {
    R0 = chip_id;
    R1 = code;
    R4 = chip_id;       /* entry to skip when counting */
    R5 = chip_data;
    R6 = cursor;
    R7 = code;
    GUEST_CALL(folder_CountChipsOfCodeOrSkipEntry_8117AA0);
    return R6;
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has
 * no typed C interface yet; r0-r3 as the callee leaves them are returned */
static struct GfxCalleeTail queue_code_tile_upload(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    struct GfxCalleeTail tail = { R0, R1, R2, R3 };
    return tail;
}

static struct FolderCodeResult build_code_counter_tiles(u32 entry, u32 r5_in, u32 r6_in, u32 r7_in) {
    struct FolderCodeResult out;

    /* Clear the tile buffer the codes and tallies are rendered into. */
    clear_code_tile_buffer(CODE_TILE_BUFFER, CODE_TILE_BUFFER_SIZE);

    u32 chip_id = LD16(entry + 2);
    u32 chip = get_chip_data_sub_8117A14(chip_id);

    /* Bytes 0-3 of the chip's database entry are its four code letters. */
    u32 codes[4];
    codes[0] = LD8(chip);
    codes[1] = LD8(chip + 1);
    codes[2] = LD8(chip + 2);
    codes[3] = LD8(chip + 3);

    /* Draw the four code letters first, cursor advancing through the buffer. */
    u32 cursor = CODE_TILE_BUFFER;
    for (u32 i = 0; i < 4; i++)
        cursor = draw_code_letter(codes[i], chip, cursor);

    /* Then the per-code folder tallies; the original re-reads the id here. */
    chip_id = LD16(entry + 2);
    for (u32 i = 0; i < 4; i++)
        cursor = count_folder_chips(chip_id, codes[i], chip, cursor);

    /* Hand the finished tiles to the DMA queue; guest r4-r7 hold the values
     * the drawing callees left (chip id / chip entry / cursor / last code),
     * exactly as in the original's call. */
    struct GfxCalleeTail tail = queue_code_tile_upload(CODE_TILE_BUFFER, CODE_TILE_VRAM_DEST, CODE_TILE_BUFFER_SIZE);

    out.r0 = tail.r0;
    out.r1 = tail.r1;
    out.r2 = tail.r2;
    out.r3 = tail.r3;
    out.r4 = entry;     /* preserved: the original pops r4-r7 */
    out.r5 = r5_in;
    out.r6 = r6_in;
    out.r7 = r7_in;
    return out;
}

/* sub_8117A14(r4 -> folder cursor entry, chip id at +2; r5-r7 untouched inputs)
 * -> r0-r3 as left by QueueEightWordAlignedGFXTransfer, nz/c/v likewise,
 * r4-r7 preserved. */
Next sub_8117A14(void) {
    struct FolderCodeResult out = build_code_counter_tiles(R4, R5, R6, R7);
    R0 = out.r0, R1 = out.r1, R2 = out.r2, R3 = out.r3, R4 = out.r4, R5 = out.r5, R6 = out.r6, R7 = out.r7;
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

/* ── folder_CountChipsOfCodeOrSkipEntry_8117AA0 ── */
/* folder_CountChipsOfCodeOrSkipEntry_8117AA0(r0 = chip id, r1 = letter code index, r6 = folder entry pointer):
 * If r1 is a regular letter code (0..0x1A), count how many chips in the folder
 * match that id and code (GetChipCountOfCode) and hand the result to
 * copyTwoDigitNumberTiles_8117B94. Otherwise the entry has no letter code to match: skip past it
 * by advancing the folder pointer one entry (0x40 bytes).
 * Results are left in r2..r7 by the callee chain / the pointer bump. */
Next folder_CountChipsOfCodeOrSkipEntry_8117AA0(void) {
    if ((s32)R1 <= 0x1A) {
        /* r0/r1 already hold the chip id and code for GetChipCountOfCode. */
        GUEST_CALL(GetChipCountOfCode);
        /* copyTwoDigitNumberTiles_8117B94 picks up the count (and any other live registers). */
        GUEST_CALL(copyTwoDigitNumberTiles_8117B94);
    } else {
        R6 += 0x40;
    }
    RETURN_TO_CALLER();
}

/* ── copyTwoDigitNumberTiles_8117B94 ── */
/* copyTwoDigitNumberTiles_8117B94(r0 = entry index, r6 = destination buffer):
 * Copies two 32-byte entries from the table at unk_2031204 into the buffer
 * pointed to by r6. The index is split by dividing r0 by 10 (BIOS Div):
 * the quotient selects the first entry and the remainder the second one.
 * On return r6 points past both copied entries; r7 is left untouched. */
Next copyTwoDigitNumberTiles_8117B94(void) {
    u32 dest = R6;

    /* svc 6 = Div: r0 = r0 / r1 (quotient), r1 = r0 % r1 (remainder). */
    R1 = 10;
    swi_call(6);
    u32 quotient = R0;
    u32 remainder = R1;

    /* First entry, skipped when the whole index fits in one table row. */
    if (quotient != 0) {
        R0 = A_unk_2031204 + (quotient << 5);
        R1 = dest;
        R2 = 0x20;
        GUEST_CALL(CopyByEightWords);
    }

    /* Second entry directly below the first one. */
    dest += 0x20;
    R0 = A_unk_2031204 + (remainder << 5);
    R1 = dest;
    R2 = 0x20;
    GUEST_CALL(CopyByEightWords);

    R6 = dest + 0x20;
    RETURN_TO_CALLER();
}

/* ── sub_8117BE8 ── */
/* register form: calls initGuiSprite_8002770 outside this file, which has no typed C interface yet */
static void call_initGuiSprite_sub_8117BE8(int a1, int a2) {
    R0 = a1; R1 = a2;
    GUEST_CALL(initGuiSprite_8002770);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_sprite_setAnimation_sub_8117BE8(u8 anim) {
    R0 = anim;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_sprite_loadAnimationData_sub_8117BE8(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_sprite_noShadow_sub_8117BE8(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setSubEntryPaletteBank_800304A outside this file, which has no typed C interface yet */
static void call_sprite_setSubEntryPaletteBank_sub_8117BE8(int bank) {
    R0 = bank;
    GUEST_CALL(sprite_setSubEntryPaletteBank_800304A);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_sprite_setPalette_sub_8117BE8(int pal) {
    R0 = pal;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_sprite_setPriority_sub_8117BE8(int prio) {
    R0 = prio;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void call_slotList_storeHalfword_sub_8117BE8(int value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void call_sprite_setSubRecordFieldA_sub_8117BE8(int value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void call_sprite_setCoordinates_sub_8117BE8(int x, int y) {
    R0 = x; R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

/* register form: calls sub_8117C4C outside this file, which has no typed C interface yet */
static u32 call_postInit(void) {
    R0 = 0;
    GUEST_CALL(sub_8117C4C);
    return R0;
}

/* Initialize a GUI sprite record at recordPtr:
 * - write the sprite ID (0x23) and reset its frame counter
 * - assign the sprite to GUI slot 0xa with its shared data table
 * - configure animation, palette, priority, OAM attribute, position
 * - set the state/sub-state bytes, then run the post-init routine
 * Returns r0 from the post-init call (the original's only visible result). */
static u32 initGuiSpriteRecord(u32 recordPtr) {
    ST8(recordPtr, 0x23);                              /* sprite ID */
    call_initGuiSprite_sub_8117BE8(0xa, A_unk_2034B6C);            /* bind to GUI slot + data */
    ST8(recordPtr + 0xC, 0);                           /* reset animation tick */
    call_sprite_setAnimation_sub_8117BE8(0);                       /* start at animation 0 */
    call_sprite_loadAnimationData_sub_8117BE8();                   /* load frame set */
    call_sprite_noShadow_sub_8117BE8();                            /* disable drop shadow */
    call_sprite_setSubEntryPaletteBank_sub_8117BE8(9);             /* palette bank 9 */
    call_sprite_setPalette_sub_8117BE8(0);                         /* palette index 0 */
    call_sprite_setPriority_sub_8117BE8(2);                        /* mid-layer priority */
    call_slotList_storeHalfword_sub_8117BE8(0x170);                /* OAM attr2 (tile+flip) */
    call_sprite_setSubRecordFieldA_sub_8117BE8(0);                 /* clear sub-record field A */
    call_sprite_setCoordinates_sub_8117BE8(0xD4, 0xC);             /* screen position (0xD4,0x0C) */
    ST8(recordPtr + 8, 4);                             /* state byte */
    ST8(recordPtr + 9, 0);                             /* sub-state byte */
    return call_postInit();                            /* post-init → r0, flags */
}

/* sub_8117BE8(r5 = sprite record pointer) -> r0 */
Next sub_8117BE8(void) {
    R0 = initGuiSpriteRecord(R5);
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
    GUEST_CALL(getStructFrom2008450); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
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

/* ── battle_refreshObjectSprite_8117E20 ── */
/* battle_refreshObjectSprite_8117E20: per-frame sprite refresh for a battle object.
 * r5 = object struct, r7+0xD = the object's current state byte.
 * The animation to show is the one stored at r5+4, or that plus 5 when the
 * state byte differs from the copy cached at r5+3 (i.e. the object changed
 * state since the animation was last picked). If the resulting animation
 * differs from the one actually loaded (cached at r5+0xC), load it and drop
 * the shadow; finally advance the sprite one frame. */
Next battle_refreshObjectSprite_8117E20(void) {
    u32 state       = LD8(R7 + 0xD);
    u32 baseAnim    = LD8(R5 + 4);
    u32 wantedAnim  = (state == LD8(R5 + 3)) ? baseAnim : baseAnim + 5;
    u32 currentAnim = LD8(R5 + 0xC);

    if (wantedAnim != currentAnim) {
        ST8(R5 + 0xC, wantedAnim);
        R0 = wantedAnim;
        R1 = currentAnim;
        GUEST_CALL(sprite_setAnimation);
        GUEST_CALL(sprite_loadAnimationData);
        GUEST_CALL(sprite_noShadow);
    }

    GUEST_CALL(sprite_update);
    RETURN_TO_CALLER();
}

/* ── sub_8118F44 ── */
/* Coordinate table for battle sprites (x,y pairs, indexed by sprite field +5 * 4) */
#define SPRITE_TYPE_INIT        0x23
#define COORD_TABLE_ADDR        A_byte_8118FA4

/* register form: calls loc_8002774 outside this file, which has no typed C interface yet */
static void call_configureSpriteFlag(u32 a1, u32 a2) {
    R0 = a1;
    R1 = a2;
    GUEST_CALL(loc_8002774);
}

/* register form: calls sprite_setAnimation outside this file, which has no typed C interface yet */
static void call_setAnimation_sub_8118F44(u32 animId) {
    R0 = animId;
    GUEST_CALL(sprite_setAnimation);
}

/* register form: calls sprite_loadAnimationData outside this file, which has no typed C interface yet */
static void call_loadAnimationData_sub_8118F44(void) {
    GUEST_CALL(sprite_loadAnimationData);
}

/* register form: calls sprite_noShadow outside this file, which has no typed C interface yet */
static void call_noShadow_sub_8118F44(void) {
    GUEST_CALL(sprite_noShadow);
}

/* register form: calls sprite_setPalette outside this file, which has no typed C interface yet */
static void call_setPalette_sub_8118F44(u32 palette) {
    R0 = palette;
    GUEST_CALL(sprite_setPalette);
}

/* register form: calls sprite_setPriority_8002E14 outside this file, which has no typed C interface yet */
static void call_setPriority_sub_8118F44(u32 priority) {
    R0 = priority;
    GUEST_CALL(sprite_setPriority_8002E14);
}

/* register form: calls slotList_storeHalfwordAndClearField_800306C outside this file, which has no typed C interface yet */
static void call_storeHalfwordAndClear(u32 value) {
    R0 = value;
    GUEST_CALL(slotList_storeHalfwordAndClearField_800306C);
}

/* register form: calls sub_81190E0 outside this file, which has no typed C interface yet */
static void call_initSubSlot(void) {
    GUEST_CALL(sub_81190E0);
}

/* register form: calls sprite_setSubRecordFieldA_8003060 outside this file, which has no typed C interface yet */
static void call_setSubRecordFieldA_sub_8118F44(u32 value) {
    R0 = value;
    GUEST_CALL(sprite_setSubRecordFieldA_8003060);
}

/* register form: calls sprite_setCoordinates outside this file, which has no typed C interface yet */
static void call_setCoordinates(u32 x, u32 y) {
    R0 = x;
    R1 = y;
    GUEST_CALL(sprite_setCoordinates);
}

/* register form: calls sprite_dispatchUpdate_8118FC0 outside this file, which has no typed C interface yet */
static void call_dispatchUpdate(u32 tablePtr) {
    R2 = tablePtr;
    GUEST_CALL(sprite_dispatchUpdate_8118FC0);
}

/* Initialize a battle sprite object: set type, animation, visual properties,
 * coordinates, and dispatch its first update.  sprite is the GBA address
 * of the sprite record (caller's R5). */
static void init_battle_sprite(u32 sprite) {
    /* Mark the sprite as type 0x23 (battle entity). */
    ST8(sprite, SPRITE_TYPE_INIT);

    /* Enable per-sprite configuration slot. */
    call_configureSpriteFlag(8, A_unk_201E284);

    /* Copy animation ID (field +4) into the sub-animation slot (field +0xC). */
    u32 animId = LD8(sprite + 4);
    ST8(sprite + 0xC, animId);

    /* Load and apply the animation, disable shadow. */
    call_setAnimation_sub_8118F44(animId);
    call_loadAnimationData_sub_8118F44();
    call_noShadow_sub_8118F44();

    /* Palette 0, priority 2. */
    call_setPalette_sub_8118F44(0);
    call_setPriority_sub_8118F44(2);

    /* Clear sub-slot halfword and reset state. */
    call_storeHalfwordAndClear(0);
    call_initSubSlot();
    call_setSubRecordFieldA_sub_8118F44(4);

    /* Look up (x, y) from the coordinate table using sprite field +5. */
    u32 offset = (LD8(sprite + 5)) << 2;
    u32 x = LD16(COORD_TABLE_ADDR + offset);
    u32 y = LD16(COORD_TABLE_ADDR + offset + 2);
    call_setCoordinates(x, y);

    /* Set active state (field +8 = 4) and trigger the first update tick. */
    ST8(sprite + 8, 4);
    call_dispatchUpdate(COORD_TABLE_ADDR);
}

/* sub_8118F44(r5 = sprite record pointer) -> void (registers preserved per callee chain) */
Next sub_8118F44(void) {
    init_battle_sprite(R5);
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

/* ── nullsub_8118FE0 ── */
/* nullsub_8118FE0: empty stub — pushes and immediately pops LR, doing nothing.
 * Present in the original ROM only to occupy a slot in a function table. */
Next nullsub_8118FE0(void) {
    RETURN_TO_CALLER();
}

/* ── sub_8119018 ── */
/* register form: calls sub_8137884 outside this file, which has no typed C interface yet */
static u32 lookup_script_entry(u32 index, u32 obj_ptr, u32 script_data) {
    R0 = index;
    R1 = index;
    R2 = obj_ptr;
    R3 = script_data;
    GUEST_CALL(sub_8137884);
    return R0;
}

/* register form: calls sub_804A24C outside this file, which has no typed C interface yet */
static u32 advance_event_script(u32 value) {
    R0 = value;
    GUEST_CALL(sub_804A24C);
    return R0;
}

/* register form: calls sub_3006B94 outside this file, which has no typed C interface yet */
static void run_text_script(u32 r0val, u32 obj_ptr, u32 script_data) {
    R0 = r0val;
    R1 = A_sub_3006B94 | 1;
    R2 = obj_ptr;
    R3 = script_data;
    R4 = 8;
    R5 = 1;
    R6 = A_dword_86B7AE0;
    R7 = 0;
    GUEST_CALL(sub_3006B94);
}

/* Dispatch a battle event script for the given object.
 * Writes active-state 4 to the object, resolves the script type from its
 * action byte (offset 3) via two parallel lookup tables, validates the entry,
 * then tail-calls the text-script interpreter. */
static void do_battle_event_script(u32 object) {
    ST8(object + 9, 4);

    u32 index = LD8(object + 3) - 7;
    u32 offset = index << 2;
    u32 obj_table_val = LD32(A_off_811905C + offset);
    u32 script_table_val = LD32(A_byte_811906C + offset);

    u32 validated = lookup_script_entry(index, obj_table_val, script_table_val);
    validated >>= 4;
    validated = advance_event_script(validated);

    run_text_script(validated, obj_table_val, script_table_val);
}

/* sub_8119018(r5 = battle object ptr) -> r0-r3, flags via tail call to sub_3006B94 */
Next sub_8119018(void) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7, saved_lr = LR;
    do_battle_event_script(saved_r5);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7; LR = saved_lr;
    RETURN_TO_CALLER();
}

/* ── sprite_testFrameParamsMaskC0_81190F8 ── */
/* sprite_testFrameParamsMaskC0_81190F8(r0 = sprite handle):
 * Looks up the sprite struct registered for the handle in r0 (via the list at
 * 0x2008450) and reports whether the sprite's current frame carries the status
 * bits 0xC0 in its frame parameters.
 * Returns r0 = 1 if the bits are set, 0 if not (or if no struct was found),
 * with N/Z reflecting r0 and C/V cleared (the original's "mov r0, r4" is an
 * adds r0, r4, #0, which zeroes the carry/overflow flags, and the trailing
 * tst r0, r0 leaves them cleared). r1 ends as 0xC0 on the found path;
 * r4/r5 are preserved. */
Next sprite_testFrameParamsMaskC0_81190F8(void) {
    u32 savedR4 = R4, savedR5 = R5;
    u32 flagged = 0;

    /* The callees see r4 = 0 here, matching the original's working register. */
    R4 = 0;

    /* Look up the entry for the handle in r0; Z set means it is unused. */
    GUEST_CALL(getStructFrom2008450);        /* r1 = the entry; Z set when its first byte is 0 */
    if (!ZERO_FLAG()) {
        /* The frame-parameter query takes the accompanying value in r5. */
        R5 = R1;
        GUEST_CALL(sprite_getFrameParameters);   /* -> r0 = frame parameter bits */

        /* Test the frame parameter bits against the 0xC0 mask (also left in r1). */
        R1 = 0xC0;
        if (R0 & R1)
            flagged = 1;
    }

    /* Restore the caller's r4/r5 (the original pops them back off the stack). */
    R4 = savedR4;
    R5 = savedR5;

    R0 = flagged;
    /* mov r0, r4 == adds r0, r4, #0: no carry-out and no signed overflow are
     * possible, so this clears C and V (N/Z from r4 as well). The original's
     * tst r0, r0 then re-derives the same N/Z while leaving C/V cleared. */
    SET_FLAGS_ADD(R0, 0);
    SET_FLAGS_NZ(R0);
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
