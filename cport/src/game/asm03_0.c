/*
 * asm03_0.c: readable functions of bn6f asm03_0.s, up to 0x080299B6, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_80269D0 ── */
/* sub_80269D0: ask battle_GetObjectByteAt14_800A9E2 for a status value and reduce it to a boolean:
 * r0 = 1 if any of bits 0/2 (mask 5) is set, otherwise r0 = 0.
 * The call's carry/overflow flags pass through unchanged to the caller. */
Next sub_80269D0(void) {
    R0 = 0;
    GUEST_CALL(battle_GetObjectByteAt14_800A9E2);

    R1 = 5;
    R0 = (R0 & R1) != 0 ? 1 : 0;

    RETURN_TO_CALLER();
}

/* ── sub_80269E2 ── */
/* eS20364C0 RAM pointer (r5 in the original). */
#define eS20364C0_PTR A_eS20364C0_JumpOffset00

/* Replicates sub_80269E2:
 *   - r0 starts at 0.
 *   - Read byte 0 as r1; if r1 is 0 or 8, r0 becomes 1 (and that CMP's flags win).
 *   - Otherwise read byte 1 as r1; if r1 is any of {0, 8, 4, 0xC, 0x2C, 0x30, 0x34,
 *     0x10, 0x14, 0x18, 0x1C, 0x20}, r0 becomes 1 and that CMP's flags win.
 *   - Else r0 stays 0 and the final CMP r1, #0x20 (BNE taken) sets the flags.
 * Returns the r0 result and the operands of the final CMP so the adapter can
 * set c/v exactly as the original does. */
static u32 check_es20364c0(u8 *last_cmp_r1, u8 *last_cmp_op) {
    u8 b0 = LD8(eS20364C0_PTR);
    u8 b1 = LD8(eS20364C0_PTR + 1);
    u32 result = 0;

    if (b0 == 0) {
        result = 1;
        *last_cmp_r1 = b0; *last_cmp_op = 0;
    } else if (b0 == 8) {
        result = 1;
        *last_cmp_r1 = b0; *last_cmp_op = 8;
    } else if (b1 == 0) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0;
    } else if (b1 == 8) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 8;
    } else if (b1 == 4) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 4;
    } else if (b1 == 0x0C) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x0C;
    } else if (b1 == 0x2C) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x2C;
    } else if (b1 == 0x30) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x30;
    } else if (b1 == 0x34) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x34;
    } else if (b1 == 0x10) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x10;
    } else if (b1 == 0x14) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x14;
    } else if (b1 == 0x18) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x18;
    } else if (b1 == 0x1C) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x1C;
    } else if (b1 == 0x20) {
        result = 1;
        *last_cmp_r1 = b1; *last_cmp_op = 0x20;
    } else {
        /* Fall through: b1 != 0x20, BNE taken, r0 stays 0. Final CMP was r1, #0x20. */
        result = 0;
        *last_cmp_r1 = b1; *last_cmp_op = 0x20;
    }
    return result;
}

/* sub_80269E2(r5 = eS20364C0 pointer) -> r0, sets c, v from the final CMP */
Next sub_80269E2(void) {
    u8 last_r1, last_op;
    u32 r = check_es20364c0(&last_r1, &last_op);
    R0 = r;
    SET_FLAGS_CMP((u32)last_r1, (u32)last_op);
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

/* ── sub_8026B04 ── */
/* Number of tile entries per animation row */
#define TILEMAP_COLS       0x14
/* Byte stride between successive tilemap columns */
#define TILEMAP_STRIDE     0x1E
/* Total animation countdown value */
#define TILE_ANIM_TOTAL    0x78
/* Tile rows per animation frame */
#define TILE_ROW_COUNT     0xF
/* Camera scroll step (dword_8026CC8) */
#define CAMERA_SCROLL_STEP 0x18000
/* Jump offset for the trigger-selection state */
#define JUMP_OFFSET_TRIGGER   0x2C
/* Jump offset for wait-selection state */
#define JUMP_OFFSET_WAIT_SEL  4

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_effect(u32 sound_id) {
    R0 = sound_id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls sub_801E71C outside this file, which has no typed C interface yet */
static void bg_scroll_apply(u32 scroll_delta, u32 progress) {
    R0 = scroll_delta;
    R1 = progress;
    GUEST_CALL(sub_801E71C);
}

/* register form: calls dispatch_801DACC outside this file, which has no typed C interface yet */
static void dispatch_bg_flags(u32 flags) {
    R0 = flags;
    GUEST_CALL(dispatch_801DACC);
}

/* register form: calls sub_801E0A0 outside this file, which has no typed C interface yet */
static void tile_bg_setup(u32 rows, u32 start) {
    R0 = rows;
    R1 = start;
    GUEST_CALL(sub_801E0A0);
}

/* register form: calls bgmap_WriteTileIdAtBlock_800187C outside this file, which has no typed C interface yet */
static void write_bg_tile(u32 row, u32 col, u32 bg_index, u16 tile_id) {
    R0 = row;
    R1 = col;
    R2 = bg_index;
    R3 = tile_id;
    GUEST_CALL(bgmap_WriteTileIdAtBlock_800187C);
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 get_battle_mode_sub_8026B04(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls RetrieveAndRenderEnemyVirusNamesInBattle_801E574 outside this file, which has no typed C interface yet */
static void render_enemy_virus_names(void) {
    GUEST_CALL(RetrieveAndRenderEnemyVirusNamesInBattle_801E574);
}

/* register form: calls battle_damagePlayerLeave1HP_8013FD0 outside this file, which has no typed C interface yet */
static void damage_player_leave_1hp(u32 player_index) {
    R0 = player_index;
    GUEST_CALL(battle_damagePlayerLeave1HP_8013FD0);
}

/* register form: sub_8029C08 reads r2-r7 as object method state carried across the
 * entire function's register flow (camera ptr in r2, scroll const in r3, etc.).
 * Typed C cannot express "value lives in a specific guest register across multiple calls." */
static void s20364c0_finalize(u32 self) {
    R5 = self;
    GUEST_CALL(sub_8029C08);
}

/* Update the tile-scrolling battle object (S20364C0).
 * self:    pointer to the state struct (r5)
 * toolkit: global toolkit pointer (r10)
 * r4, r6:  caller-saved values that sub_8029C08 reads via those registers.
 * R2/R3 are written as guest registers inside the camera path because the game
 * passes them forward through multiple GUEST_CALLs to sub_8029C08. */
static void s20364c0_update(u32 self, u32 toolkit, u32 r4, u32 r6) {
    /* --- Initialization (only when state flag is zero) --- */
    if (LD8(self + oS20364C0_Unk_02) == 0) {
        play_sound_effect(SOUND_SELECT_79);

        u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
        ST16(render_info + 0x18, TILE_ANIM_TOTAL);

        bg_scroll_apply(0, render_info);
        dispatch_bg_flags(0x80);
        tile_bg_setup(TILE_ROW_COUNT, 0);

        ST32(self + 0x44, TILE_ROW_COUNT);
        ST32(self + 0x40, TILE_ANIM_TOTAL);
        ST8(self + oS20364C0_Unk_02, TILE_ANIM_TOTAL);
    }

    /* --- Tile-rendering loop --- */
    u32 progress = LD32(self + 0x40) - 0xc;
    u32 rows = ((4 & progress) >> 2) + 1;

    for (u32 row_iter = 0; row_iter < rows; row_iter++) {
        u32 tile_index = LD32(self + 0x44) - 1;
        ST32(self + 0x44, tile_index);
        u32 tilemap_ptr = A_unk_2035000 + tile_index * 2;

        for (u32 col = 0; col < TILEMAP_COLS; col++) {
            u16 tile_id = LD16(tilemap_ptr);
            write_bg_tile(tile_index, col, 3, tile_id);
            tilemap_ptr += TILEMAP_STRIDE;
        }
    }

    /* --- Camera adjustment: if not in battle mode 1, scroll camera up --- */
    if (get_battle_mode_sub_8026B04() != 1) {
        u32 camera = LD32(toolkit + oToolkit_CameraPtr);
        ST32(camera + 0x34, LD32(camera + 0x34) - CAMERA_SCROLL_STEP);
        /* register form: sub_8029C08 reads r2/r3 as accumulated register state; C locals
         * cannot flow into those guest registers through the intervening GUEST_CALLs */
        R2 = camera;
        R3 = CAMERA_SCROLL_STEP;
    }

    /* --- Write back progress and scroll position --- */
    ST32(self + 0x40, progress);
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render_info + 0x18, progress);
    bg_scroll_apply(TILE_ANIM_TOTAL - progress, progress);

    /* --- When animation completes (progress reached zero): resolve outcome --- */
    if (progress == 0) {
        u32 battle_state = LD32(toolkit + oToolkit_BattleStatePtr);

        if (LD8(battle_state + oBattleState_Unk_07) == 1) {
            render_enemy_virus_names();
        } else {
            damage_player_leave_1hp(0);
            damage_player_leave_1hp(1);
        }

        /* Choose next state based on the mode byte in self */
        u8 mode = LD8(self + oS20364C0_Unk_0c);
        bool go_wait;

        if (mode == 0xFF) {
            go_wait = true;
        } else if (mode != 5) {
            go_wait = false;
        } else {
            u32 bs = LD32(toolkit + oToolkit_BattleStatePtr);
            go_wait = (LD8(bs + 7) >= 5);
        }

        if (go_wait) {
            ST8(self + oS20364C0_JumpOffset01, JUMP_OFFSET_WAIT_SEL);
        } else {
            ST8(self + oS20364C0_JumpOffset01, JUMP_OFFSET_TRIGGER);
        }
        ST16(self + oS20364C0_Unk_02, 0);
    }

    /* --- Finalize: call object method that produces this function's return registers --- */
    s20364c0_finalize(self);
}

/* sub_8026B04(r5=self, r10=toolkit, r4, r6=state for sub_8029C08) -> all regs + flags */
Next sub_8026B04(void) {
    s20364c0_update(R5, R10, R4, R6);
    RETURN_TO_CALLER();
}

/* ── sub_8026BF4 ── */
/* register form: calls dispatch_801DACC outside this file, which has no typed C interface yet */
static void call_dispatch_801DACC(u32 flags) {
    R0 = flags;
    GUEST_CALL(dispatch_801DACC);
}

/* register form: calls sub_8029D80 outside this file, which has no typed C interface yet */
static void call_sub_8029D80(void) {
    GUEST_CALL(sub_8029D80);
}

/* register form: calls clearBattleStateUnk11Flag_800A9D6 outside this file, which has no typed C interface yet */
static void call_clear_battle_state_unk11_flag(u32 bit) {
    R0 = bit;
    GUEST_CALL(clearBattleStateUnk11Flag_800A9D6);
}

/* register form: CopyBackgroundTiles takes 6 args in R0-R5 and clobbers R4-R7
 * which hold the loop's saved self/scroll state; R1-R3 output values flow into
 * the subsequent sub_8029C08 call as register arguments. */
static void call_copy_bg_tiles(u32 j, u32 i, u32 block, u32 tile_ids, u32 j_size, u32 i_size) {
    u32 saved_r4 = R4, saved_r5 = R5, saved_r6 = R6, saved_r7 = R7;
    R0 = j; R1 = i; R2 = block; R3 = tile_ids; R4 = j_size; R5 = i_size;
    GUEST_CALL(CopyBackgroundTiles);
    R4 = saved_r4; R5 = saved_r5; R6 = saved_r6; R7 = saved_r7;
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 call_get_battle_mode(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls sub_801E71C outside this file, which has no typed C interface yet */
static void call_sub_801E71C(u32 delta, u32 value) {
    R0 = delta;
    R1 = value;
    GUEST_CALL(sub_801E71C);
}

/* register form: sub_8029C08 takes R2-R7 as register context. R2/R3 carry either
 * the camera pointer + scroll constant (mode != 1) or opaque CopyBackgroundTiles
 * output (mode 1), which can only live in guest registers between the calls. */
static void call_sub_8029C08(u32 self) {
    R5 = self;
    GUEST_CALL(sub_8029C08);
}

#define TILE_BLOCK_ALL_ZERO  ((u32)0x08026C88)

/* Background-scroll state update (S20364C0 object in r5, toolkit in r10):
 *  - one-shot init (zero extra pointers, dispatch BG update, clear a battle flag)
 *  - scroll the tile map one column (or two) and copy background tiles
 *  - shift camera by one tile-block unless in battle mode 1
 *  - publish scroll position to state object and render-info
 *  - notify scroll subsystem; wrap at end of strip
 *  - per-frame update callback                                    */
static void sub_8026BF4_impl(u32 self, u32 toolkit) {
    /* First-run init: set the gate flag, clear extra pointers, dispatch
     * a BG update, and clear battle-state bit 2 of unk11. */
    if (LD8(self + oS20364C0_Unk_02) == FALSE) {
        ST32(self + oS20364C0_Extra_Unk_44, 0);
        ST32(self + oS20364C0_Extra_Unk_40, 0);
        ST8(self + oS20364C0_Unk_02, TRUE);
        call_dispatch_801DACC(0x2000);
        call_sub_8029D80();
        call_clear_battle_state_unk11_flag(4);
    }

    /* Compute the new scroll position and how many tile columns to copy
     * (2 if bit-2 of scroll_pos is clear, else 1). */
    u32 scroll_pos = LD32(self + oS20364C0_Extra_Unk_40) + 0xC;
    u32 iterations = (((4u & scroll_pos) >> 2) ^ 1u) + 1u;

    /* Increment the tile counter and copy one column of BG tiles per pass. */
    for (u32 n = iterations; n != 0; n--) {
        u32 j = LD32(self + oS20364C0_Extra_Unk_44);
        ST32(self + oS20364C0_Extra_Unk_44, j + 1);
        call_copy_bg_tiles(j, 0, 3, TILE_BLOCK_ALL_ZERO, 1, 0x14);
    }

    /* Outside battle mode 1, shift the camera Y by one tile-block (0x18000).
     * R2/R3 must hold the camera pointer and constant as register arguments
     * for the sub_8029C08 call that follows. */
    if (call_get_battle_mode() != 1) {
        u32 camera = LD32(toolkit + oToolkit_CameraPtr);
        ST32(camera + 0x34, LD32(camera + 0x34) + 0x18000);
        /* register form: R2/R3 are register args consumed by sub_8029C08 below */
        R2 = camera;
        R3 = 0x18000;
    }

    /* Publish the new scroll position to the state object and render-info. */
    ST32(self + oS20364C0_Extra_Unk_40, scroll_pos);
    u32 render_info = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render_info + 0x18, scroll_pos);

    /* Notify the scrolling subsystem; wrap the strip when fully scrolled. */
    call_sub_801E71C(0x78 - scroll_pos, scroll_pos);
    if (scroll_pos == 0x78) {
        ST8(self + 0x1, LD8(self + 0xB));
        ST8(self + 0x2, 0);
    }

    /* Per-frame update callback. */
    call_sub_8029C08(self);
}

/* sub_8026BF4(self: * S20364C0 $r5, toolkit: * $r10) */
Next sub_8026BF4(void) {
    sub_8026BF4_impl(R5, R10);
    RETURN_TO_CALLER();
}

/* ── custMenuUpdateWaitSelectionState_8026CCC ── */
/* custMenuUpdateWaitSelectionState_8026CCC — per-frame update of the S20364C0 custom-menu object (self in R5).
 *
 * Asks sub_802A220 for the menu's current result:
 *   - anything but 0xFF: latch it into the object at +0x38, switch the object
 *     to its 0x40 handler (which routes through custMenuMainMaybe_8026A88 and
 *     triggers sub_8027548), clear the frame counter at +0x40, and return.
 *   - 0xFF (nothing chosen yet): run the menu's update handlers with the
 *     current joypad state and tick the frame counter at +0x40. */
Next custMenuUpdateWaitSelectionState_8026CCC(void) {
    GUEST_CALL(sub_802A220);
    u32 result = R0;

    if (result != 0xFF) {
        SET_FLAGS_CMP(result, 0xFF);

        /* record the selection and hand control to the 0x40 state handler */
        ST16(R5 + oS20364C0_Unk_38, result);
        ST8(R5 + 1, 0x40);   /* oS20364C0_JumpOffset01 */
        ST8(R5 + 2, 0);      /* oS20364C0_Unk_02 */
        ST32(R5 + 0x40, 0);

        R0 = 0;
        SET_FLAGS_NZ(0);
    } else {
        /* menu still open: feed it the joypad, update it, tick its counter */
        u32 savedR7 = R7;

        R7 = LD32(R10 + 4);   /* oToolkit_JoypadPtr */

        GUEST_CALL(custMenuSomeHandler_8028B74); /* (self, joypad) */
        GUEST_CALL(sub_8028820);
        GUEST_CALL(sub_8029C08);                 /* (self) */
        GUEST_CALL(sub_802899C);
        GUEST_CALL(sub_8029D34);

        u32 counter = LD32(R5 + 0x40);
        R0 = counter + 1;
        ST32(R5 + 0x40, R0);
        SET_FLAGS_ADD(counter, 1);

        /* r7 is only scratch here; the original pops it back for the caller */
        R7 = savedR7;
    }

    RETURN_TO_CALLER();
}

/* ── chatbox_stateMaybeFinishText_8026E4C ── */
/* chatbox_stateMaybeFinishText_8026E4C (r5 = chatbox state struct, S20364C0):
 * Advance the chatbox one step. If flag 0x80 of eFlags2009F38 is set, ask
 * sub_802A220 whether the text is still busy: while it reports 0xFF, just
 * run the periodic update. Otherwise (flag clear, or text finished) play
 * sound effect 0x9E, set the state byte at +1 to 4, clear the timer
 * halfword at +2, and run the periodic update. */
Next chatbox_stateMaybeFinishText_8026E4C(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    if (R0 != 0) {
        GUEST_CALL(sub_802A220);
        if (R0 == 0xFF) {
            GUEST_CALL(sub_8029C08);
            RETURN_TO_CALLER();
        }
        GUEST_CALL(chatbox_8040818);
    }

    R0 = 0x9E;
    GUEST_CALL(PlaySoundEffect);

    ST8(R5 + 1, 4);
    ST16(R5 + 2, 0);

    GUEST_CALL(sub_8029C08);
    RETURN_TO_CALLER();
}

/* ── battle_updateObjectByKind_8026E98 ── */
/* battle_updateObjectByKind_8026E98(self = R5): one update step for a battle object.
 *
 * Reads the global battle effect flags and passes 4 in R4 if the 0x200000
 * effect is active, 0 otherwise. It then dispatches through the handler
 * table at off_8026EBC, indexed by the kind byte at self+2, calling the
 * selected handler with the kind in R0, its own thumb pointer in R1,
 * self in R5 and the effect flag in R4. Finally it runs sub_8029C08 on
 * the same object.
 *
 * R4 is only the effect flag for the duration of the calls; the caller's
 * value is restored on return (push/pop {r4} in the original).
 *
 * Flags before the calls mirror the original's TST/MOV pair:
 *   - effect present: N/Z from "movs r4, #4"
 *   - effect absent:  N/Z from "tst r0, #0x200000" (Z set)
 * C/V are untouched by both. The flags on return are whatever
 * sub_8029C08 leaves. */
Next battle_updateObjectByKind_8026E98(void) {
    u32 self = R5;
    u32 saved_r4 = R4;

    R4 = 0;
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 0x200000) {
        R4 = 4;
        SET_FLAGS_NZ(4);
    } else {
        SET_FLAGS_NZ(effects & 0x200000);
    }

    u32 kind = LD8(self + 2);
    R0 = kind;
    R1 = LD32(A_off_8026EBC + kind); /* thumb handler pointer from the ROM table */
    switch (kind) {
    case 0: GUEST_CALL(sub_8026EC8); break;
    case 4: GUEST_CALL(sub_8026F1A); break;
    case 8: GUEST_CALL(sub_8026FAA); break;
    }

    GUEST_CALL(sub_8029C08);
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_8026EC8 ── */
/* Register form: GetBattleEffects has no typed C interface yet. */
static u32 get_battle_effects(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* Register form: GetBattleMode has no typed C interface yet. */
static u32 get_battle_mode_sub_8026EC8(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* Register form: chatbox_runScript has no typed C interface yet. */
static void chatbox_run_script(u32 archive, u32 script_idx) {
    R0 = archive;
    R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

/* Register form: PlaySoundEffect has no typed C interface yet. */
static void play_sound_effect_sub_8026EC8(u32 id) {
    R0 = id;
    GUEST_CALL(PlaySoundEffect);
}

/* Runs the tutorial/battle-run dialog script appropriate for the current state
 * and plays the unselect jingle. r5 points to a small battle-state object
 * (+2 = script-result slot, +0xC = tutorial-index byte); r4 is a base pointer
 * into the per-row text-script indices used by the run-away dialog. */
static void run_battle_dialog(u32 base_r4, u32 state_ptr) {
    u32 effects = get_battle_effects();

    if (effects & 0x20) {
        ST16(state_ptr + 2, 4);
        chatbox_run_script(A_TextScriptBattleRunDialog, base_r4);
    } else {
        ST16(state_ptr + 2, 8);
        if (get_battle_mode_sub_8026EC8() == 5) {
            chatbox_run_script(LD32(A_TextScriptFirstBattleTutorials + 0x0C), 0x23);
        } else {
            u32 idx = LD8(state_ptr + 0xC);
            if (idx == 0xFF) {
                chatbox_run_script(A_TextScriptBattleRunDialog, base_r4 + 3);
            } else {
                chatbox_run_script(LD32(A_TextScriptFirstBattleTutorials + (idx << 2)),
                                   LD8(A_byte_8027034 + idx));
            }
        }
    }

    play_sound_effect_sub_8026EC8(SOUND_UNSELECT_7B);
}

Next sub_8026EC8(void) {
    run_battle_dialog(R4, R5);
    RETURN_TO_CALLER();
}

/* ── sub_8026FC0 ── */
/* Struct offsets for the battle-run tutorial state object (S20364C0). */
#define oBattleRunSelf_Timer  1
#define oBattleRunSelf_State  2
#define oBattleRunSelf_Mode   4

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 call_GetBattleEffects(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls battle_ClearPendingEventFlag_802A0EC outside this file, which has no typed C interface yet */
static void call_battle_ClearPendingEventFlag_802A0EC(void) {
    GUEST_CALL(battle_ClearPendingEventFlag_802A0EC);
}

/* register form: calls chatbox_mask_eFlags2009F38 outside this file, which has no typed C interface yet */
static u32 call_chatbox_mask_eFlags2009F38(u32 flag) {
    R0 = flag;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    return R0;
}

/* register form: calls chatbox_runScript outside this file, which has no typed C interface yet */
static void call_chatbox_runScript(u32 archive, u32 script_idx) {
    R0 = archive;
    R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

/* Drive the "battle is about to run away" tutorial dialog.
 *  - On first entry (state == 0): mark the tutorial seen (state := 1),
 *    clear the pending event flag, then run the tutorial script at page
 *    1 (mode == 2) or page 2 (mode != 2), plus the page offset (4) that
 *    comes from GetBattleEffects() & 0x200000.
 *  - On subsequent entries, while chatbox flag 0x80 is busy, do nothing.
 *    Once it clears, set timer = 0x14 and zero the 16-bit state field. */
static u32 run_battle_run_tutorial(u32 self) {
    u32 page_offset = (call_GetBattleEffects() & 0x200000u) ? 4u : 0u;

    u8 state = LD8(self + oBattleRunSelf_State);
    if (state == 0) {
        ST8(self + oBattleRunSelf_State, 1);
        call_battle_ClearPendingEventFlag_802A0EC();
        /* mov r1, #1; if (mode != 2) mov r1, #2; add r1, r4
         * So r1 == 1 when mode == 2, else r1 == 2; then +page_offset. */
        u32 script_idx = (LD8(self + oBattleRunSelf_Mode) == 2) ? 1u : 2u;
        script_idx += page_offset;
        call_chatbox_runScript(A_TextScriptBattleRunDialog, (u8)script_idx);
        return 0;
    }

    u32 mask = call_chatbox_mask_eFlags2009F38(0x80);
    if (mask != 0)
        return (u32)mask;
    ST8(self + oBattleRunSelf_Timer, 0x14);
    ST16(self + oBattleRunSelf_State, 0);
    return 0;
}

Next sub_8026FC0(void) {
    u32 result = run_battle_run_tutorial(R5);
    SET_FLAGS_NZ(result);
    RETURN_TO_CALLER();
}

/* ── battle_loadObjTableEntryAndSetState4_80272B8 ── */
/* battle_loadObjTableEntryAndSetState4_80272B8: load the AI/behavior table entry selected by the object's
 * state byte (+0xC) into the object (+0x40) and switch its state to 4.
 * Inputs: R5 = object pointer. Outputs: R0 = 4, R1 = table base, nz/c. */
Next battle_loadObjTableEntryAndSetState4_80272B8(void) {
    u32 tableBase = A_off_8027BEC;

    u32 index = LD8(R5 + 0xC);
    /* carry flag comes from the bit shifted out of the LSL by 2
     * (a byte value shifted left twice can never set it, but keep it exact) */
    u32 carryOut = (index >> 30) & 1;

    ST32(R5 + 0x40, LD32(tableBase + (index << 2)));

    R1 = tableBase;
    R0 = 4;
    cpu.c = carryOut;
    SET_FLAGS_NZ(R0); /* flags as left by "mov r0, #4" */

    ST16(R5 + 2, 4);
    RETURN_TO_CALLER();
}

/* ── chatbox_setState4IfFlag80Clear_8027320 ── */
/* chatbox_setState4IfFlag80Clear_8027320: test the 0x80 bit of the chatbox event flags (eFlags2009F38).
 * If it is not set, write halfword 4 to [r5+2] (presumably a chatbox state
 * or timer field). Either way r0 holds the flag-test result and the flags
 * reflect the last comparison/move as in the original. */
Next chatbox_setState4IfFlag80Clear_8027320(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);

    SET_FLAGS_CMP(R0, 0);
    if (R0 == 0) {
        R0 = 4;
        ST16(R5 + 2, R0);
        SET_FLAGS_NZ(R0);
    }

    RETURN_TO_CALLER();
}

/* ── sub_8027394 ── */
/* sub_8027394(r5 = object/state pointer):
 * Reset the entity at r5 to state 4 and clear its halfword field.
 * Leaves r0 = 0 with flags set accordingly. */
Next sub_8027394(void) {
    ST8(R5 + 1, 4);
    R0 = 0;
    ST16(R5 + 2, 0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_object_SelectHandlerByMode_80273CC ── */
/* battle_object_SelectHandlerByMode_80273CC (R5 = battle object):
 * The object re-selects its current handler: read the mode byte at +0xC,
 * look up the routine pointer indexed by it in the table at off_8027C84,
 * store that pointer into the object at +0x40, and set the halfword state
 * field at +2 to 4.
 *
 * On return: R0 = 4, R1 = table address, N/Z from "MOV r0,#4" (both clear)
 * and C from the "LSL r0,r0,#2" (bit 30 of the mode byte). */
Next battle_object_SelectHandlerByMode_80273CC(void) {
    u32 mode = LD8(R5 + 0xC);

    R1 = A_off_8027C84;

    /* lsl r0, r0, #2 — index = mode * 4, and C is the bit shifted out
     * (bit 30 of the byte).  Emulate that carry exactly with a doubling
     * add: x + x carries out bit 31 of x, so doubling (mode << 1) carries
     * out bit 30 of mode, just like the shift.  N/Z here are overwritten
     * by the MOV below, same as in the original. */
    SET_FLAGS_ADD(mode << 1, mode << 1);
    u32 index = mode << 2;

    R0 = LD32(R1 + index);
    ST32(R5 + 0x40, R0);

    R0 = 4;
    ST16(R5 + 2, R0);

    SET_FLAGS_NZ(R0); /* mov r0, #4 updates N and Z only */
    RETURN_TO_CALLER();
}

/* ── sub_80273E4 ── */
/* sub_80273E4(r5 = pointer to a byte):
 * store 8 into the byte at r5; r0 = 8. */
Next sub_80273E4(void) {
    R0 = 8;
    SET_FLAGS_NZ(R0);
    ST8(R5, R0);
    RETURN_TO_CALLER();
}

/* ── chatbox_resetTextStateIfHidden_80273EC ── */
/* chatbox_resetTextStateIfHidden_80273EC(r5 = textbox object S20364C0):
 * Unless the chatbox "shown" flag (0x80 of eFlags2009F38) is set, reset the
 * textbox state: copy the stored line count/character at +0xD into the active
 * slot at +1 and clear the halfword at +2 (character timer/index).
 * Then update the textbox via sub_8029C08. */
Next chatbox_resetTextStateIfHidden_80273EC(void) {
    R0 = 0x80;
    GUEST_CALL(chatbox_mask_eFlags2009F38);
    if (R0 == 0) {
        ST8(R5 + 1, LD8(R5 + 0xD));
        ST16(R5 + 2, 0);
    }
    GUEST_CALL(sub_8029C08); /* self stays in r5 */
    RETURN_TO_CALLER();
}

/* ── chip_SelectNameTextTable_8027D10 ── */
/* chip_SelectNameTextTable_8027D10(r0 = chip id): pick the chip-name text table for a chip id.
 * The id is truncated to 9 bits; ids above 0xFF select the second name
 * table (TextScriptChipNames1) and wrap around to 8 bits.
 * Outputs: r0 = pointer to the name table, r1 = index into it. */
Next chip_SelectNameTextTable_8027D10(void) {
    u32 chipId = (R0 << 23) >> 23;      /* 9-bit chip id */

    u32 tablePtr = A_off_8027D2C;       /* -> TextScriptChipNames0 */
    if ((s32)chipId > 0xFF) {
        tablePtr += 4;                  /* -> TextScriptChipNames1 */
        chipId = (chipId << 24) >> 24;  /* wrap into the second table */
    }

    R0 = LD32(tablePtr);
    R1 = chipId;
    RETURN_TO_CALLER();
}

/* ── chip_getScript_8027D34 ── */
/* chip_getScript_8027D34(r0 = chip id):
 * Resolves the text script table holding a chip's description. Standard chip
 * ids (0x00-0x0FF) use the regular table; ids above 0x0FF (EX/Giga chips) use
 * the second table and are indexed modulo 0x100.
 * Outputs: r0 = pointer stored at the head of the selected table,
 *          r1 = the chip's index within that table. */
Next chip_getScript_8027D34(void) {
    /* Literal-pool entries pointing at the two description script tables. */
    u32 tablePtr = A_off_8027D50;

    u32 chip = R0 & 0x1FF;
    if (chip > 0xFF) {
        tablePtr += sizeof(u32); /* second table: EX/Giga chip descriptions */
        chip &= 0xFF;
    }

    R0 = LD32(tablePtr);
    R1 = chip;
    RETURN_TO_CALLER();
}

/* ── battle_countFolderChips_8027D58 ── */
/* battle_countFolderChips_8027D58: scan the equipped battle folder's chip ID list (up to 30
 * entries) for its 0xFFFF terminator.
 * Returns: r0 = number of chips stored before the terminator (30 if the
 *          folder is full and has none),
 *          r2 = address of the terminator entry (or one past the last
 *          entry when the scan ran off the end),
 *          r3 = number of entries left unscanned at that point.
 * The original uses r4 as scratch for the loaded ID but restores it on
 * return, so it is simply left untouched here. */
Next battle_countFolderChips_8027D58(void) {
    u32 cursor = A_eBattleFolder;
    u32 remaining = 0x1E;

    while (remaining != 0) {
        if (LD16(cursor) == 0xFFFF)
            break;
        cursor += 2;
        remaining -= 1;
    }

    R0 = 0x1E - remaining;
    R2 = cursor;
    R3 = remaining;
    RETURN_TO_CALLER();
}

/* ── sub_8027D78 ── */
/* Register-form helpers for game functions that have no typed C interface yet. */

/* register form: calls sub_801E71C outside this file, which has no typed C interface yet */
static void call_sub_801E71C_sub_8027D78(void) {
    R0 = 0;
    GUEST_CALL(sub_801E71C);
}

/* register form: calls dispatch_801DACC outside this file, which has no typed C interface yet */
static void call_dispatch_801DACC_sub_8027D78(u32 flags) {
    R0 = flags;
    GUEST_CALL(dispatch_801DACC);
}

/* register form: calls sub_801E0A0 outside this file, which has no typed C interface yet */
static void call_sub_801E0A0(u32 a0, u32 a1) {
    R0 = a0;
    R1 = a1;
    GUEST_CALL(sub_801E0A0);
}

/* register form: calls eStruct2035280_SetRequestBits_801BECC outside this file, which has no typed C interface yet */
static void call_SetRequestBits_801BECC(u32 bits) {
    R0 = bits;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);
}

/* register form: calls dispatch_801DA48 outside this file, which has no typed C interface yet */
static void call_dispatch_801DA48(u32 bits) {
    R0 = bits;
    GUEST_CALL(dispatch_801DA48);
}

/* register form: calls battle_IsBattleEnding_800A97A outside this file, which has no typed C interface yet */
static bool battle_is_ending(void) {
    GUEST_CALL(battle_IsBattleEnding_800A97A);
    return !ZERO_FLAG();        /* Z clear when battle is ending */
}

/* register form: calls battle_testFlag0x40_800A8F8 outside this file, which has no typed C interface yet */
static bool battle_test_flag0x40(void) {
    GUEST_CALL(battle_testFlag0x40_800A8F8);
    return !ZERO_FLAG();        /* Z clear when flag is set */
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 get_battle_mode_sub_8027D78(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls sub_801DF0C outside this file, which has no typed C interface yet */
static void call_sub_801DF0C(void) { GUEST_CALL(sub_801DF0C); }

/* register form: calls sub_801DEEE outside this file, which has no typed C interface yet */
static void call_sub_801DEEE(void) { GUEST_CALL(sub_801DEEE); }

/* register form: calls custGauge_Init_801DED0 outside this file, which has no typed C interface yet */
static void cust_gauge_init(void) { GUEST_CALL(custGauge_Init_801DED0); }

/* register form: calls ClearCustGauge outside this file, which has no typed C interface yet */
static void clear_cust_gauge(void) { GUEST_CALL(ClearCustGauge); }


/* Request flag bits (from the dispatch constants in the original). */
enum {
    CUST_REQUEST_ENDING_BIT = 0x400u,
    CUST_REQUEST_BIT        = 0x20000u,
    CUST_REQUEST_MODE1_BIT  = 0x20u,
    CUST_REQUEST_OTHER_BIT  = 0x10u,
};

/* Unconditional "ending" request: the original at the top of the function
 * calls SetRequestBits BEFORE dispatch (the reverse of the other paths). */
static void set_request_ending_bit(void) {
    call_SetRequestBits_801BECC(CUST_REQUEST_ENDING_BIT);
    call_dispatch_801DA48(CUST_REQUEST_ENDING_BIT);
}

/* Request paths used for 0x20000 / 0x20 / 0x10: dispatch BEFORE SetRequestBits. */
static void cust_request(u32 bits) {
    call_dispatch_801DA48(bits);
    call_SetRequestBits_801BECC(bits);
}

/* Per-turn "battle ends" routine for the Custom Gauge / panel UI.
 * `toolkit` is the GBA pointer held in r10 on entry. */
static void battle_custom_gauge_per_turn(u32 toolkit) {
    /* Clear pending-trigger field in the render-info struct. */
    u32 render = LD32(toolkit + oToolkit_RenderInfoPtr);
    ST16(render + 0x18, 0);

    call_sub_801E71C_sub_8027D78();                              /* gauge setup / display reset */
    call_dispatch_801DACC_sub_8027D78(0x80);                     /* mark request flag 0x80 */
    call_sub_801E0A0(0, 0);                          /* clear gauge counter */
    set_request_ending_bit();                        /* ending flag (0x400): SetRequestBits then dispatch */

    if (battle_is_ending())
        return;

    /* Battle-state byte 7 selects which ending routine we take. */
    u32 bstate = LD32(toolkit + oToolkit_BattleStatePtr);
    u32 path   = LD8(bstate + 7);

    if (path == 1) {
        if (battle_test_flag0x40()) {
            call_sub_801DF0C();                      /* "flag 0x40" end-of-turn path */
            return;
        }
        if (get_battle_mode_sub_8027D78() == 1) {
            call_sub_801DEEE();                      /* mode-1 end-of-turn cleanup */
        } else {
            cust_gauge_init();                       /* default init for custom gauge */
        }
        return;
    }

    /* path != 1 */
    if (battle_test_flag0x40()) {
        cust_request(CUST_REQUEST_BIT);              /* 0x20000: dispatch + request bits */
        return;
    }
    clear_cust_gauge();
    if (get_battle_mode_sub_8027D78() == 1) {
        cust_request(CUST_REQUEST_MODE1_BIT);        /* 0x20 */
    } else {
        cust_request(CUST_REQUEST_OTHER_BIT);        /* 0x10 */
    }
}

Next sub_8027D78(void) {
    /* The typed function touches no guest registers, so flags (including v)
     * and clobbered/preserved registers are automatically correct. */
    battle_custom_gauge_per_turn(R10);
    RETURN_TO_CALLER();
}

/* ── sub_8027E90 ── */
/* Battle mode flag used to select the panel-status branch. */
enum {
    BATTLE_MODE_SINGLE = 1,
};

/* Offsets into a battle-record slot (12 slots, 0xC bytes each). */
#define SLOT_SIZE         0xC
#define SLOT_COUNT_sub_8027E90        12
#define PANEL_MODE_OFFSET 0x84
#define PANEL_FLAG_OFFSET 0x8B
#define PANEL_MODE_VALUE_SINGLE 3
#define PANEL_MODE_VALUE_OTHER  2

/* ROM address of the per-slot template table (each entry: 4-byte panel-id pointer). */
#define SLOT_TEMPLATE_TABLE_PTR A_dword_802A7CC

/* RAM address of the battle-mode configuration struct. */
#define BATTLE_CONFIG_PTR A_dword_20349A0

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 get_battle_mode_sub_8027E90(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls sub_8029FB4 outside this file, which has no typed C interface yet */
static bool sub_8029FB4_retbool(void) {
    GUEST_CALL(sub_8029FB4);
    return R0 != 0;
}

/* register form: calls sub_802A57E outside this file, which has no typed C interface yet */
static bool sub_802A57E_retbool(void) {
    GUEST_CALL(sub_802A57E);
    return R0 != 0;
}

/* sub_8027E90: initialise the 12 battle-record slots from the ROM template, then set the
 * panel-status byte on the slot array header according to the current battle mode.
 * slot_array_ptr (r4) points to the slot array. Returns the address one past the last slot
 * (r4 + 12 * 0xC), matching the original's final r2. */
static u32 reset_battle_record(u32 slot_array_ptr) {
    u32 template_addr = SLOT_TEMPLATE_TABLE_PTR;
    u32 slot_addr = slot_array_ptr;
    for (u32 i = 0; i < SLOT_COUNT_sub_8027E90; i++) {
        ST32(slot_addr,     LD32(template_addr));
        ST32(slot_addr + 4, 0);
        ST32(slot_addr + 8, 0);
        template_addr += 4;
        slot_addr += SLOT_SIZE;
    }

    u32 mode = get_battle_mode_sub_8027E90();
    if (mode == BATTLE_MODE_SINGLE) {
        if (LD8(BATTLE_CONFIG_PTR + 4) == 0) {
            ST8(slot_array_ptr + PANEL_MODE_OFFSET, PANEL_MODE_VALUE_SINGLE);
        }
    } else if (sub_8029FB4_retbool()) {
        u32 flag = 1;
        if (sub_802A57E_retbool()) {
            flag = 0;
        }
        ST8(slot_array_ptr + PANEL_MODE_OFFSET, PANEL_MODE_VALUE_OTHER);
        ST8(slot_array_ptr + PANEL_FLAG_OFFSET, (u8)flag);
    }

    return slot_addr;
}

Next sub_8027E90(void) {
    R2 = reset_battle_record(R4);
    RETURN_TO_CALLER();
}

/* ── battle_BuildFolderEntryRecords_8027EE8 ── */
/* battle_BuildFolderEntryRecords_8027EE8(r4 = destination records, r5 = source header struct, r6 = slot limit):
 * Fill battle folder entries into 12-byte records. The record count is the
 * byte at [r5,#5], clamped to the limit in r6; each record gets a zero byte
 * at +0 and a pointer to its entry inside eBattleFolder (2 bytes per entry)
 * at +8. r4 and r6 are preserved by the original (push/pop), so the record
 * walk happens in a local pointer. r2 and r3 both end at 0 after filling;
 * when nothing is written, r3 holds the count byte that was read. */
Next battle_BuildFolderEntryRecords_8027EE8(void) {
    u32 count = LD8(R5 + 5);

    if (count == 0 || R6 == 0) {
        R3 = count;             /* r3 keeps the ldrb result on this path */
        RETURN_TO_CALLER();
    }

    u32 slots = ((s32)count < (s32)R6) ? count : R6;
    u32 dst = R4;               /* local walk pointer; r4 itself is restored */
    u32 entry = A_eBattleFolder;/* &eBattleFolder[0], 2 bytes per entry */

    /* The byte at +0 is always 0, not the record index. */
    for (u32 i = slots; i != 0; i--) {
        ST8(dst, 0);
        ST32(dst + 8, entry);
        dst += 0xC;
        entry += 2;
    }

    R2 = 0;
    R3 = 0;
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

/* ── sub_8027F84 ── */
/* Chip ID table indexed by slot category (0..11). */
static const u8 chip_slot_table[12] = {
    0x07, 0x06, 0x05, 0x04, 0x02, 0x11,
    0x10, 0x0F, 0x07, 0x01, 0x00, 0x00,
};

/* register form: calls getLocOfActiveChips_8027E1C outside this file,
 * which has no typed C interface yet */
static u32 loc_of_active_chips(u32 a1) {
    R0 = a1;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    return R0;
}

/* register form: calls sub_8028034 outside this file,
 * which has no typed C interface yet */
static u32 sub_8028034_caller(u32 a0) {
    R0 = a0;
    GUEST_CALL(sub_8028034);
    return R0;
}

/* Sets the chip ID at [r7+2] based on the active chip pointer table.
 * If the chip pointer is 0xA or 0xB, picks from the A-table; otherwise
 * from the B-table. The "sub r6, #1" path swaps slots 5 or 9 down by one
 * in the 0xA/0xB branch only when r6 >= 5 and < 0xA. The picked ID is
 * passed to sub_8028034; if it returns r6 unchanged, the field is
 * set to 0xFF (clamped/max). Otherwise the looked-up value stays. */
static void update_chip_slot_id(u32 r6_in, u32 r7) {
    u32 loc = loc_of_active_chips(LD8(r7 + 2));
    u32 chip_kind = LD8(loc);
    u32 new_r6 = r6_in;

    if (chip_kind != 0xA && chip_kind != 0xB)
        return;

    u32 base;
    if (chip_kind == 0xA) {
        /* r6 == 0xA is the out-of-range slot; clamp via A-table. */
        base = A_off_802A7FC;
        if (r6_in == 0xA)
            goto apply;
        if ((s32)r6_in < 5)
            goto apply;
        /* r6 between 5 and 9 (i.e. 5..9): swap r6 with r6-1 using B-table. */
        base = A_byte_802A804;
        u32 slot_byte = LD8(r7);
        if (slot_byte != 9 && slot_byte != 5)
            goto apply;
        new_r6 = r6_in - 1;
    } else {
        base = A_byte_802A804;
        u32 slot_byte = LD8(r7);
        if (slot_byte == 9 || slot_byte == 5)
            new_r6 = r6_in - 1;
    }

apply:
    {
        u32 offset = chip_slot_table[new_r6];
        u32 lookup = base + offset;
        u32 result = sub_8028034_caller(lookup);
        if (result == new_r6)
            result = 0xFF;
        ST8(r7 + 2, result);
    }
}

Next sub_8027F84(void) {
    update_chip_slot_id(R6, R7);
    RETURN_TO_CALLER();
}

/* ── sub_8027FDC ── */
/* register form: calls getLocOfActiveChips_8027E1C outside this file, which has
 * no typed C interface yet. Takes an int chip id and returns a void*. */
static void *get_loc_of_active_chips_sub_8027FDC(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    return (void *)R0;
}

/* register form: calls sub_8028034 outside this file, which has no typed C
 * interface yet. Takes an address and returns a u8 in R0. */
static u8 sub_8028034_helper(u32 addr) {
    R0 = addr;
    GUEST_CALL(sub_8028034);
    return (u8)R0;
}

/* Per-id index table at 0x08028028 (12 bytes, used as [r6] for r6 in 0..11). */
static const u8 table_byte_8028028[12] = {
    0x1, 0x2, 0x3, 0x5, 0x7, 0x1, 0x2, 0x3, 0x4, 0x5, 0x0, 0x0
};

/* Look up the active-chip byte and decide whether to do the table work.
 * On the early-return path, r1 is left at 0xFF (matches `mov r1, #0xff`).
 * On the main path, r1 ends up holding state[0] (matches `ldrb r1, [r7]`).
 * Returns true if we entered the main path. */
static bool run_table_lookup(void *active_loc, u32 id, u8 flag, u8 *state,
                             u8 *out_r1) {
    u8 loc_byte0 = LD8((u32)active_loc);
    if (loc_byte0 != 0xA && loc_byte0 != 0xB) {
        *out_r1 = 0xFF;
        return false;
    }

    /* Main path: r1 = state[0]. */
    *out_r1 = flag;

    u32 base = 0x802A817;
    u32 bumped_id = id;
    if (id != 0xA && (s32)id >= 5 && (flag == 8 || flag == 4)) {
        base = 0x802A81F;
        bumped_id = id + 1;
    }

    u8 index  = table_byte_8028028[bumped_id];
    u8 lookup = sub_8028034_helper(base + index);

    u8 stored = (lookup == (u8)bumped_id) ? 0xFF : lookup;
    state[3] = stored;
    return true;
}

Next sub_8027FDC(void) {
    u8  *state = (u8 *)R7;
    u32  id    = R6;

    u8 out_r1;
    run_table_lookup(get_loc_of_active_chips_sub_8027FDC(state[3]), id, state[0], state,
                      &out_r1);

    R1 = out_r1;
    RETURN_TO_CALLER();
}

/* ── table_findFirstOccupiedEntry_8028050 ── */
/* table_findFirstOccupiedEntry_8028050(r4 = table cursor, r5 = record):
 * Scans a table of 12-byte entries starting at r4. A leading byte of 0x0A or
 * 0x0B marks an empty entry, so skip those; any other byte marks an occupied
 * entry, which stops the scan. Stores the index of the first occupied entry
 * (or 0x0A if all 12 entries were empty) to r5[7]. r4 and r6 are callee-saved
 * and end unchanged; the cursor/count live in locals (the original pushes and
 * pops r4/r6 around the scan). */
Next table_findFirstOccupiedEntry_8028050(void) {
    u32 entry = R4;
    u32 slot = 0;

    while (LD8(entry) == 0xA || LD8(entry) == 0xB) {
        entry += 0xC;
        if (++slot == 0xC) {
            /* No occupied entry in the whole table: report 0x0A. */
            slot = 0xA;
            break;
        }
    }

    ST8(R5 + 7, slot);
    RETURN_TO_CALLER();
}

/* ── folder_findFirstChipWithFlag_802806C ── */
/* Folder-slot scan helpers used by folder_findFirstChipWithFlag_802806C. */
#define SLOT_COUNT   10   /* number of chip slots walked */
#define SLOT_STRIDE  0xC  /* bytes per slot entry */

/* Slot entry layout:
 *   +0: u8  slot state flag (0 = examine this slot)
 *   +8: u32 pointer to a halfword identifying the slot's chip */

/* folder_findFirstChipWithFlag_802806C(r4 = chip slot list, r5 = owner struct):
 * Walk the ten slots in order.  For each slot flagged 0 at +0, map the
 * halfword at [slot+8] to a chip id (sub_802A53C, masked to 9 bits),
 * fetch that chip's data, and test the 0x20 flag in its status byte
 * (+9).  The first matching slot's index is stored at [r5+7] and the
 * scan stops.  [r5+7] is left alone when it is already non-zero or no
 * slot matches.
 * r2/r3/r5 pass through unchanged, and the caller expects its r4/r6
 * back intact. */
Next folder_findFirstChipWithFlag_802806C(void) {
    u32 savedR4 = R4;
    u32 savedR6 = R6;

    if (LD8(R5 + 7) != 0) /* slot choice already recorded */
        RETURN_TO_CALLER();

    u32 slot = R4;
    for (u32 i = 0; i < SLOT_COUNT; i++) {
        R0 = LD8(slot);
        if (R0 == 0) {
            R0 = LD16(LD32(slot + 8));
            R4 = slot; /* keep the guest view of the loop state in sync */
            R6 = i;
            GUEST_CALL(sub_802A53C); /* chip word -> chip id */
            R0 &= 0x1FF;             /* keep the 9-bit chip id */
            GUEST_CALL(getChip8021DA8);
            R0 = LD8(R0 + 9); /* chip status byte */
            R1 = 0x20;
            if (R0 & 0x20) {
                ST8(R5 + 7, i);
                break;
            }
        }
        slot += SLOT_STRIDE;
    }

    R4 = savedR4;
    R6 = savedR6;
    RETURN_TO_CALLER();
}

/* ── selectChipTableEntryIntoActiveSlot9_80280A2 ── */
/* selectChipTableEntryIntoActiveSlot9_80280A2: register one of the table entries (byte_802A828, halfwords indexed
 * 1..11) into the object at R5.
 *
 * R5+0x10 holds a selection index. Index 0 and anything above 11 are ignored, as
 * is any entry whose bit is already set in the "taken" bitmask word_20349AC
 * (dword_20349A0 + 0xC). Otherwise the entry's halfword is copied to R5+0x22 and
 * active-chip slot 9 is cleared and pointed at that field.
 *
 * On return R2 = (1 << index) whenever the index was in range (it is computed
 * before the bitmask test, so it is also set when the entry turns out to be
 * taken); R4 is preserved. */
Next selectChipTableEntryIntoActiveSlot9_80280A2(void) {
    u32 index = LD8(R5 + 0x10);

    if (index != 0 && index <= 0xB) {
        u32 takenMask = LD16(A_dword_20349A0 + 0xC);
        u32 entryBit = 1u << index;

        R2 = entryBit;

        if (!(takenMask & entryBit)) {
            /* The table pointer lives in r4 between the lookup and the call;
             * r4 itself is saved/restored by the original, so it is unchanged
             * on return. */
            u32 savedR4 = R4;
            u32 table = A_byte_802A828;

            R4 = table + (index - 1) * 2;
            ST16(R5 + 0x22, LD16(R4));

            R0 = 9;
            R1 = table; /* the original still holds the table base in r1 here */
            GUEST_CALL(getLocOfActiveChips_8027E1C);
            u32 slot = R0;

            ST8(slot, 0);
            ST8(slot + 4, 0);
            ST32(slot + 8, R5 + 0x22);

            R4 = savedR4;
        }
    }

    RETURN_TO_CALLER();
}

/* ── battle_spawnMirrorFacingHelper_80280E0 ── */
/* battle_spawnMirrorFacingHelper_80280E0: set up a helper object entry (at R4+0x60) once R5's object
 * signals that it is ready.  Reads R5's state byte (+0x10) and facing byte
 * (+0x16); writes kind/hit-table/behaviour bytes into the new entry.
 * R2 ends up holding R5's facing byte when the setup path runs. */
Next battle_spawnMirrorFacingHelper_80280E0(void) {
    /* Nothing to do if the object is already active/finished. */
    if (LD8(R5 + 0x10) != 0)
        RETURN_TO_CALLER();

    /* Ask battle_checkActiveSlot8NaviStatReady_802A5DA whether the trigger condition holds; bail out if not. */
    GUEST_CALL(battle_checkActiveSlot8NaviStatReady_802A5DA);
    if (R0 == 0)
        RETURN_TO_CALLER();

    u32 entry = R4 + 0x60;

    ST8(entry,       4);              /* object kind */
    ST8(entry + 0xC, 5);              /* ... */
    ST8(entry + 3,   0xB);            /* behaviour/table id */
    ST8(entry + 0xE, 7);              /* ... */

    /* Mirror the source object's facing (+0x16): entry+4 = 1 - facing. */
    R2 = LD8(R5 + 0x16);
    u32 dir = (1u - R2) & 0xFF;
    ST8(entry + 4, dir);

    /* When facing is 1 (dir == 0), also flag the entry at +7. */
    if (dir == 0)
        ST8(entry + 7, 1);

    RETURN_TO_CALLER();
}

/* ── gfx_flushDataListAndContinue_802811C ── */
/* gfx_flushDataListAndContinue_802811C: flush the dataList's pending GFX transfers, then continue with
 * gfx_queueIndexedRecordTiles_802812C seeded by the byte at [R5+0x10]. R4-R7 are passed through. */
Next gfx_flushDataListAndContinue_802811C(void) {
    /* dataList (table of GFX data references) */
    R0 = A_off_802A744;
    GUEST_CALL(QueueGFXTransfersInList);

    R0 = LD8(R5 + 0x10);
    GUEST_CALL(gfx_queueIndexedRecordTiles_802812C);

    RETURN_TO_CALLER();
}

/* ── gfx_queueIndexedRecordTiles_802812C ── */
/* gfx_queueIndexedRecordTiles_802812C(r0 = record index):
 * Queue a record's graphics for transfer: the two 0x40-byte halves of its
 * 0x80-byte block go to VRAM 0x060170E0 and 0x06017160, and a 0x20-byte
 * chunk reached through the record goes to byte_30016B0. */
Next gfx_queueIndexedRecordTiles_802812C(void) {
    u32 savedR4 = R4;
    u32 record = R0;

    /* The record handle stays in r4 across all the calls below. */
    R4 = record;

    /* Pointer to the record's 0x80-byte graphics block. */
    R0 = record;
    GUEST_CALL(getIndexed80hRecordPtr_8028164);

    /* First half to 0x060170E0, then the second half (which directly
     * follows the first in the block) to 0x06017160. */
    R1 = 0x60170E0;
    R2 = 0x40;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
    R0 += R2;                       /* r2 still holds the 0x40 chunk size */

    R1 = 0x6017160;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Fetch the extra chunk associated with the record... */
    R0 = record;
    GUEST_CALL(getPermutedRecord32_8028172);

    /* ...and queue it (0x20 bytes) to byte_30016B0. */
    R1 = A_byte_30016B0;
    R2 = 0x20;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── getIndexed80hRecordPtr_8028164 ── */
/* getIndexed80hRecordPtr_8028164(r0 = index):
 * Look up index in byte_802819C, then compute the address of that entry's
 * 0x80-byte record in the dat38_86 table (table base + entry * 0x80).
 * Returns the address in r0. */
Next getIndexed80hRecordPtr_8028164(void) {
    u8 entry = LD8(A_byte_802819C + R0);
    R0 = A_dat38_86 + entry * 0x80;
    RETURN_TO_CALLER();
}

/* ── getPermutedRecord32_8028172 ── */
/* getPermutedRecord32_8028172(r0 = index):
 * Selects one of the 32-byte records laid out at byte_86E56FC. The index is
 * run through the permutation table at off_802818C (entries {3,0,4,1,5,2,6,0},
 * which remaps slot order), then scaled by the 0x20-byte record size and added
 * to the record base. Returns the record address in r0. */
Next getPermutedRecord32_8028172(void) {
    u32 index = R0;

    u32 entry = LD8(A_off_802818C + index);
    R0 = A_byte_86E56FC + entry * 0x20;

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

/* ── gfx_QueueNextChunkedGfxTransfer_80281C4 ── */
/* gfx_QueueNextChunkedGfxTransfer_80281C4(r0 = source, r1 = destination, r3 = context for battle_getChipElementData_80281A8):
 * Let battle_getChipElementData_80281A8 set up the next graphics chunk (it adjusts r0 = source and
 * r1 = destination), queue it as an 0x80-sized eight-word-aligned GFX
 * transfer, then advance the destination pointer past the chunk just queued.
 * Returns r1 = destination + 0x80; r2 and r3 are preserved (the original
 * saves them around the calls and restores them on the way out). */
Next gfx_QueueNextChunkedGfxTransfer_80281C4(void) {
    u32 saved_r2 = R2;
    u32 saved_r3 = R3;

    /* Set up source/destination for this chunk. */
    GUEST_CALL(battle_getChipElementData_80281A8);

    /* Queue the chunk transfer: QueueEightWordAlignedGFXTransfer(src, dest, 0x80). */
    R2 = 0x80;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Destination pointer moves past the chunk we just queued. */
    R1 += 0x80;

    /* Restore the registers the original pops back before returning. */
    R2 = saved_r2;
    R3 = saved_r3;
    RETURN_TO_CALLER();
}

/* ── sprite_CopyObjTilesToVramSlot_80281D4 ── */
/* sprite_CopyObjTilesToVramSlot_80281D4(r1 = slot index, r0/r2..r7 = passthrough to gfx_QueueNextChunkedGfxTransfer_80281C4):
 * Points at one 0x80-byte chunk of OBJ VRAM tile memory, 0x0600A4A0 + 0x80 * r1,
 * and lets gfx_QueueNextChunkedGfxTransfer_80281C4 perform the (de)compression/copy for that chunk.
 * r1 and r2 are saved across the call and restored for the caller. */
Next sprite_CopyObjTilesToVramSlot_80281D4(void) {
    u32 saved_r1 = R1;
    u32 saved_r2 = R2;

    u32 vram_offset = 0x80 * saved_r1;
    u32 vram_dest = 0x0600A4A0 + vram_offset;

    R1 = vram_dest;
    R2 = vram_offset;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R1 = saved_r1;
    R2 = saved_r2;
    RETURN_TO_CALLER();
}

/* ── gfx_QueueNext64ByteChunk_8028204 ── */
/* gfx_QueueNext64ByteChunk_8028204: queue one 0x40-byte graphics DMA chunk.
 *
 * Entry: r1 = destination address for this chunk.  The helper data_getRecordPtrById_8028214
 * fills in r0 with the matching source address, the transfer is queued with
 * size 0x40, and r1 is advanced past the chunk so the caller can immediately
 * queue the next one.  r2/r3 are scratch for the queue call and are restored
 * before returning.  Flags come from the `adds r1, #0x40` pointer bump. */
Next gfx_QueueNext64ByteChunk_8028204(void) {
    u32 saved_r2 = R2;
    u32 saved_r3 = R3;

    /* Resolve the source address (leaves r1-r3 untouched). */
    GUEST_CALL(data_getRecordPtrById_8028214);

    /* QueueEightWordAlignedGFXTransfer(src = r0, dest = r1, size = 0x40). */
    R2 = 0x40;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);

    /* Restore the caller's r2/r3 and advance the destination pointer. */
    R2 = saved_r2;
    R3 = saved_r3;
    u32 prev_r1 = R1;
    R1 = prev_r1 + 0x40;
    SET_FLAGS_ADD(prev_r1, 0x40);

    RETURN_TO_CALLER();
}

/* ── data_getRecordPtrById_8028214 ── */
/* data_getRecordPtrById_8028214(r0 = record id):
 * Returns a pointer to a 64-byte record for the given id.
 * Ids 0x1B and 0x1C are special-cased and return byte_802A700 instead.
 * Any other id above the table is clamped to the last entry (0x1B).
 * Result in r0; r2 is preserved. */
Next data_getRecordPtrById_8028214(void) {
    u32 id = R0;

    if (id == 0x1B || id == 0x1C) {
        R0 = A_byte_802A700;
    } else {
        if ((s32)id > 0x1C)
            id = 0x1B;
        R0 = A_dword_86E591C + (id << 6);
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

/* ── chipSelect_QueueEntryIconGfx_8028284 ── */
/* chipSelect_QueueEntryIconGfx_8028284: queue the chip-icon graphics for one entry of the chip
 * selection screen.
 *   r4 -> entry struct: byte at +7 selects the source (2 = use the fixed
 *   id 0x1FF), word at +8 points to the chip's id/code halfword.
 * Calls getChipID_802A54E to map the chip halfword to an icon id, then
 * streams the icon tiles: first the chunked transfer header, then the
 * 64-byte tile chunk for id / 512. Flags on return are whatever the last
 * callee leaves, as in the original. */
Next chipSelect_QueueEntryIconGfx_8028284(void) {
    u32 entry = R4;

    u32 chipId;
    if (LD8(entry + 7) == 2) {
        /* Fixed id: this entry does not reference a real chip. */
        chipId = 0x1FF;
    } else {
        R0 = LD16(LD32(entry + 8));
        GUEST_CALL(getChipID_802A54E);
        chipId = R0;
    }

    R0 = chipId;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R0 = LD16(LD32(entry + 8));
    GUEST_CALL(getChipID_802A54E);
    R0 = R0 >> 9;
    GUEST_CALL(gfx_QueueNext64ByteChunk_8028204);

    RETURN_TO_CALLER();
}

/* ── nullsub_14 ── */
/* nullsub_14: 080282AC THUMB — empty stub; does nothing. */
Next nullsub_14(void) {
    RETURN_TO_CALLER();
}

/* ── battle_endActionSetup_8028310 ── */
/* battle_endActionSetup_8028310: apply the two standard "end of battle action" setup calls.
 * First resets something with the value 0x1FF, then triggers action 0x1D.
 * All argument registers (r1-r7) are passed through untouched for the callees. */
Next battle_endActionSetup_8028310(void) {
    R0 = 0x1FF;
    GUEST_CALL(gfx_QueueNextChunkedGfxTransfer_80281C4);

    R0 = 0x1D;
    GUEST_CALL(gfx_QueueNext64ByteChunk_8028204);

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

/* ── beastout_LoadSelectionScreenSpriteSheets_802869E ── */
/* beastout_LoadSelectionScreenSpriteSheets_802869E: load the three sprite sheets of the Beast Out selection screen
 * into VRAM by calling sub_802868C once per sheet.
 *   r0 = graphics source data, r1 = VRAM destination, r3 = sheet slot.
 * r2 is an extra argument for sub_802868C that this function passes through
 * untouched (it is listed as preserved), so each call sees the caller's value.
 * On return r1 = 0x6009B60 and r3 = 3 from the last setup, matching the original. */
Next beastout_LoadSelectionScreenSpriteSheets_802869E(void) {
    /* Sheet 1: byte_802A6C0 graphics -> 0x6009AA0, slot 1 */
    R0 = A_byte_802A6C0;
    R1 = 0x6009AA0;
    R3 = 1;
    GUEST_CALL(sub_802868C);

    /* Sheet 2: byte_802A680 graphics -> 0x6009AE0, slot 2 */
    R0 = A_byte_802A680;
    R1 = 0x6009AE0;
    R3 = 2;
    GUEST_CALL(sub_802868C);

    /* Sheet 3: byte_802A6C0 graphics again -> 0x6009B60, slot 3 */
    R0 = A_byte_802A6C0;
    R1 = 0x6009B60;
    R3 = 3;
    GUEST_CALL(sub_802868C);

    RETURN_TO_CALLER();
}

/* ── sub_80286D4 ── */
/* register form: calls sub_80287D2 outside this file, which has no typed C interface yet */
static void call_sub_80287D2(void) {
    GUEST_CALL(sub_80287D2);
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file, which has no typed C interface yet */
static void queue_gfx(u32 src, u32 dest, u32 size) {
    R0 = src;
    R1 = dest;
    R2 = size;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* register form: calls beastout_LoadSelectionScreenSpriteSheets_802869E outside this file, which has no typed C interface yet */
static void load_selection_screen_sprite_sheets(void) {
    GUEST_CALL(beastout_LoadSelectionScreenSpriteSheets_802869E);
}

/* Queue two DMA tile transfers (map + tileset) selected by a palette index in the
 * battle state's alliance field, then load the selection-screen sprite sheets. */
static void do_palette_gfx_and_sheets(u32 state) {
    call_sub_80287D2();
    u32 idx = (LD8(state + 8) != 0) ? 4 : 0;
    u32 gfx_src = LD32(A_off_8028708 + idx);
    queue_gfx(gfx_src, 0x6009560, 0x540);
    u32 tileset_src = LD32(A_off_8028714 + idx);
    queue_gfx(tileset_src, A_unk_3001AA0, 0x20);
    load_selection_screen_sprite_sheets();
}

Next sub_80286D4(void) {
    do_palette_gfx_and_sheets(R5);
    RETURN_TO_CALLER();
}

/* ── sub_80287D2 ── */
/* register form: calls sub_802868C outside this file, which has no typed C interface yet */
static void sprite_descriptor_fill(void) {
    /* Copy 8 bytes of sprite descriptor table (byte_802A6C0) into the
     * engine's 8-byte sprite descriptor slot at 0x6009360. */
    R0 = (u32)A_byte_802A6C0;
    R1 = 0x6009360;
    R3 = 8;
    GUEST_CALL(sub_802868C);
}

/* register form: calls QueueEightWordAlignedGFXTransfer outside this file,
 * which has no typed C interface yet */
static void queue_sprite_tile_gfx(void) {
    /* Queue a 0x20-byte tile row from ROM (byte_86E587C) into VRAM
     * (unk_3001A80). */
    R0 = (u32)A_byte_86E587C;
    R1 = (u32)A_unk_3001A80;
    R2 = 0x20;
    GUEST_CALL(QueueEightWordAlignedGFXTransfer);
}

/* Queue a sprite tile row into VRAM and prime the sprite descriptor slot. */
static void load_one_sprite_tile_row(void) {
    sprite_descriptor_fill();
    queue_sprite_tile_gfx();
}

/* sub_80287D2(r2, r4, r5, r6, r7, lr) -> r4 r5 r6 r7 (preserved) */
Next sub_80287D2(void) {
    load_one_sprite_tile_row();
    RETURN_TO_CALLER();
}

/* ── sub_8028894 ── */
/* sub_8028894: pick a pair of offsets from the ID byte at [R5+7].
 * IDs 0-4 index the first half of the byte_80288B0 table (R1 stays 0),
 * IDs 5+ index the second half and start R1 at 0x18 instead.
 * Returns R0 = index*16 + 8 and R1 = start + 0x68 (flags set from that add),
 * and leaves the byte_80288B0 table address in R6. */
Next sub_8028894(void) {
    u32 index = LD8(R5 + 7);
    u32 r1Base = 0;

    if (index >= 5) {
        r1Base = 0x18;
        index -= 5;
    }

    R6 = A_byte_80288B0;
    R0 = (index << 4) + 8;
    R1 = r1Base + 0x68;
    SET_FLAGS_ADD(r1Base, 0x68);

    RETURN_TO_CALLER();
}

/* ── loadConstArgsAndPairTable_80288D0 ── */
/* loadConstArgsAndPairTable_80288D0: load arguments for a follow-up call.
 * r0 = 0x5B, r1 = 0x6E (flags left over from computing r1 = 0x70 - 2),
 * r6 = address of the 16-entry byte-pair table at byte_80288E4
 * (entries look like <value, flag> pairs, e.g. dialog/choice parameters). */
Next loadConstArgsAndPairTable_80288D0(void) {
    R0 = 0x58 + 3;
    SET_FLAGS_CMP(0x70, 2);
    R1 = 0x70 - 2;
    R6 = A_byte_80288E4;
    RETURN_TO_CALLER();
}

/* ── custMenuPressOK_8028D3A ── */
/* custMenuPressOK_8028D3A: "OK" pressed in the custom screen menu.
 * r5 = menu state struct (oS20364C0_*). Runs the pre-OK bookkeeping calls,
 * dispatches one of the two highlight handlers depending on the current
 * selection state, then triggers the handler switch in custMenuMainMaybe_8026A88
 * by setting the jump offset at r5+1 to 8, clears the counter at r5+2, plays the
 * select sound and returns 0 in r0 (Z set, as after Thumb `mov r0, #0`).
 * The original returns via `pop {pc}`, i.e. to the LR captured on entry, so the
 * entry LR must be preserved across the guest calls. */
Next custMenuPressOK_8028D3A(void) {
    u32 return_address = LR;
    u32 menu = R5;

    GUEST_CALL(sub_8029110);
    GUEST_CALL(sub_80293F8);

    if (LD8(menu + oS20364C0_Unk_19) != 0) {
        R0 = LD8(menu + oS20364C0_Unk_19);
        GUEST_CALL(sub_802937A);
    } else {
        u16 unk3a = LD16(menu + oS20364C0_Unk_3a);
        if (unk3a != 0) {
            R0 = unk3a;
            GUEST_CALL(sub_80293B0);
        }
    }

    /* hand control back to the custom menu main loop with jump offset 8
     * (these two fields are addressed by raw offsets in the disassembly) */
    ST8(menu + 1, 8);
    ST16(menu + 2, 0);

    R0 = SOUND_SELECT_82;
    GUEST_CALL(PlaySoundEffect);

    LR = return_address;
    R0 = 0;
    SET_FLAGS_NZ(0);
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

/* ── sub_8028E4C ── */
/* register form: calls getLocOfActiveChips_8027E1C outside this file, which has no typed C interface yet */
static u32 get_loc_of_active_chips_sub_8028E4C(u32 index) {
    R0 = index;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    return R0;
}

/* register form: calls getChipID_802A54E outside this file, which has no typed C interface yet */
static u32 get_chip_id_sub_8028E4C(u32 arg) {
    R0 = arg;
    GUEST_CALL(getChipID_802A54E);
    return R0;
}

/* register form: calls chipIDIs13f_8029FF4 outside this file, which has no typed C interface yet */
static bool chip_id_is_13f_sub_8028E4C(u32 arg) {
    R0 = arg;
    GUEST_CALL(chipIDIs13f_8029FF4);
    return !ZERO_FLAG();
}

/* Walk r5's active-chip list, classifying chips by element code and writing results back.
 *   [r5+0xA]: element code if a chip is in the 0x1B-0x1C range.
 *   [r5+9]:   element code (with tie-breaking) if a chip is in the 0x1A range.
 *   [r5+0x20]: u16 result depending on which sentinel condition matched. */
static void scan_chip_elements(u32 r5_in) {
    s32 r6 = -1;           /* initial sentinel for [r5+0x20] */
    u32 r7 = 0x1A;         /* element-code candidate */
    u32 r3 = LD8(r5_in + 8);
    u32 r4;

    ST8(r5_in + 0xA, 0);
    if (r3 == 0) goto done;

    r4 = 0x48;
    while (1) {
        u32 idx = LD8(r5_in + r4);
        u32 ptr = get_loc_of_active_chips_sub_8028E4C(idx);

        if (LD8(ptr) != 0) goto next;

        {
            u32 r0 = get_chip_id_sub_8028E4C(LD16(LD32(ptr + 8)));
            u32 r1 = r0 >> 9;                  /* element code (top bits) */
            r0 = (r0 << 23) >> 23;             /* low byte of chip id */
            bool is13f = chip_id_is_13f_sub_8028E4C(r0);

            if (!is13f) {
                if ((s32)r1 >= 0x1B && (s32)r1 <= 0x1C) {
                    ST8(r5_in + 0xA, (u8)r1);
                    goto next;
                }

                /* r1 is not in the 0x1B-0x1C range; update r7 (0x1A-range candidate). */
                if (r1 != 0x1A) {
                    if (r7 == 0x1A) {
                        r7 = r1;
                    } else if (r7 != r1) {
                        r7 = 0xFF;
                    }
                }

                /* r6 sentinel logic. */
                if ((s32)r0 == r6) {
                    goto next;
                } else if (r6 == -1) {
                    r6 = (s32)r0;
                } else if (r6 == (s32)r0) {
                    goto next;
                } else {
                    r6 = -2;
                }
            }
        }

next:
        r4 += 1;
        r3 -= 1;
        if (r3 != 0) continue;
        break;
    }

done:
    ST16(r5_in + 0x20, (u16)r6);
    ST8(r5_in + 9, (u8)r7);
}

/* sub_8028E4C: walks the active-chip list in r5, classifying chips by element code.
 *   inputs:  r2 r4 r5 r6 r7 r10 lr
 *   outputs: r2 r3 r4 r5 r6 r7
 * No flag outputs are required by callers. */
Next sub_8028E4C(void) {
    scan_chip_elements(R5);
    RETURN_TO_CALLER();
}

/* ── updateCustomScreen_WhenUnselectingChip_8028EC8 ── */
/* register form: calls getLocOfActiveChips_8027E1C outside this file, which has no typed C interface yet */
static u32 get_loc_of_active_chips(u32 param) {
    u32 s2 = R2, s3 = R3, s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = param;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 res = R0;
    R2 = s2; R3 = s3; R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return res;
}

/* register form: calls getChipID_802A54E outside this file, which has no typed C interface yet */
static u32 get_chip_id(u16 raw_data) {
    u32 s1 = R1, s2 = R2, s3 = R3, s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = (u32)raw_data;
    GUEST_CALL(getChipID_802A54E);
    u32 res = R0;
    R1 = s1; R2 = s2; R3 = s3; R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return res;
}

/* register form: calls chipIDIs13f_8029FF4 outside this file, which has no typed C interface yet */
static bool chip_id_is_13f(u32 id) {
    u32 s2 = R2, s3 = R3, s4 = R4, s5 = R5, s6 = R6, s7 = R7;
    R0 = id;
    GUEST_CALL(chipIDIs13f_8029FF4);
    bool res = R0 != 0;
    R2 = s2; R3 = s3; R4 = s4; R5 = s5; R6 = s6; R7 = s7;
    return res;
}

#define SLOT_SIZE 12
#define MAX_ACTIVE_SLOTS 10
#define STAR_CODE 0x1A
#define SPECIAL_CODE_LO 0x1B
#define SPECIAL_CODE_HI 0x1C
#define ACTIVE_CHIP_OFF 0x20
#define SELECTED_COUNT_OFF 8
#define MONOCODE_OFF 9
#define ACTIVE_CODE_OFF 0x0A
#define SELECTED_FLAG_OFF 7

/*
 * Mark each of the 10 active-chip slots as selectable (0) or unselectable (1).
 * state: custom-screen control block.
 * Returns the final scratch value that the loop leaves in r2 (used as output).
 */
static u32 update_chip_slot_selections(u32 state, u32 r2_scratch) {
    u32 slot = get_loc_of_active_chips(0);

    for (int i = 0; i < MAX_ACTIVE_SLOTS; i++) {
        /* Skip non-empty slots and slots already marked "selected" (2) */
        if (LD8(slot) != 0) { slot += SLOT_SIZE; continue; }
        if (LD8(slot + SELECTED_FLAG_OFF) == 2) { slot += SLOT_SIZE; continue; }

        /* If 5 or more chips already selected, no more can be added */
        if (LD8(state + SELECTED_COUNT_OFF) >= 5) {
            ST8(slot + SELECTED_FLAG_OFF, 1);
            slot += SLOT_SIZE;
            continue;
        }

        /* Decode the chip's code and ID from its data pointer */
        u32 data_ptr = LD32(slot + 8);
        u32 chip_val = get_chip_id((u16)LD16(data_ptr));
        u32 code = chip_val >> 9;
        u32 id = chip_val & 0x1FF;
        u32 flag = 0;  /* assume selectable; set to 1 if a check rejects it */

        /* Chips with special ID 0x13f are always selectable */
        if (chip_id_is_13f(id)) {
            ST8(slot + SELECTED_FLAG_OFF, flag);
            slot += SLOT_SIZE;
            continue;
        }

        /* Codes 0x1B–0x1C: special active-code gating */
        if (code >= SPECIAL_CODE_LO && code <= SPECIAL_CODE_HI) {
            u32 active_code = LD8(state + ACTIVE_CODE_OFF);
            if (active_code == 0) {
                flag = 0;
            } else if (active_code == code) {
                flag = 0;
            } else {
                flag = 1;
            }
            ST8(slot + SELECTED_FLAG_OFF, flag);
            slot += SLOT_SIZE;
            continue;
        }

        /* Normal code path: check against active chip */
        {
            s32 active_chip = (s32)LDS16(state + ACTIVE_CHIP_OFF);
            r2_scratch = (u32)active_chip;

            if (active_chip != -1 && id == (u32)active_chip) {
                ST8(slot + SELECTED_FLAG_OFF, flag);  /* selectable */
                slot += SLOT_SIZE;
                continue;
            }
        }

        /* Monocode path (active_chip == -1, or chip ID didn't match active) */
        {
            u32 mono = LD8(state + MONOCODE_OFF);
            r2_scratch = mono;

            if (mono == 0xFF) {
                flag = 1;
                ST8(slot + SELECTED_FLAG_OFF, flag);
                slot += SLOT_SIZE;
                continue;
            }

            if (mono == STAR_CODE) {
                /* All selected chips are stars so far → always compatible */
                ST8(slot + SELECTED_FLAG_OFF, flag);
                slot += SLOT_SIZE;
                continue;
            }
        }

        /* Mixed codes: star wildcard or exact monocode match required */
        if (code == STAR_CODE) {
            /* Chip is a star → selectable regardless of monocode */
            flag = 0;
        } else if (code == r2_scratch) {
            /* Chip code matches the current monocode */
            flag = 0;
        } else {
            flag = 1;
        }
        ST8(slot + SELECTED_FLAG_OFF, flag);
        slot += SLOT_SIZE;
    }

    return r2_scratch;
}

/* updateCustomScreen_WhenUnselectingChip_8028EC8(r2,r4,r5,r6,r7,r10) -> r2,r3; r4,r5,r6,r7 preserved */
Next updateCustomScreen_WhenUnselectingChip_8028EC8(void) {
    R2 = update_chip_slot_selections(R5, R2);
    R3 = 0;
    RETURN_TO_CALLER();
}

/* ── battle_updateActiveChipSlotB_state_8028F48 ── */
/* battle_updateActiveChipSlotB_state_8028F48(r4..r7, r10, lr = caller context; r5 = battle object):
 * Bookkeeping for slot 11 of the active-chip list (the chip currently being
 * handled on the custom screen).  getLocOfActiveChips_8027E1C returns that
 * slot's record.  If the record is in state 2 and its byte at +7 has not yet
 * been set to 2, this decides the byte at +7:
 *   - cleared to 0 when the battle object's byte at +8 is below 5, the helper
 *     sub_802A57E reports non-zero, and the object's byte at +0x19 is zero;
 *   - set to 1 in every other case.
 * The early-exit paths leave the record untouched.  r4-r7 are preserved and
 * r2/r3 pass through unchanged. */
Next battle_updateActiveChipSlotB_state_8028F48(void) {
    R0 = 0xB;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 chip = R0;

    if (LD8(chip) != 2)
        RETURN_TO_CALLER();
    if (LD8(chip + 7) == 2)
        RETURN_TO_CALLER();

    u32 flag;
    if ((s32)LD8(R5 + 8) >= 5) {
        flag = 1;
    } else {
        /* sub_802A57E is queried with the object's byte at +8 in r0, as in
         * the original call site; it reports non-zero when the chip may be
         * unmarked. */
        R0 = LD8(R5 + 8);
        GUEST_CALL(sub_802A57E);
        u32 allowed = R0;
        flag = (allowed != 0 && LD8(R5 + 0x19) == 0) ? 0 : 1;
    }
    ST8(chip + 7, flag);

    RETURN_TO_CALLER();
}

/* ── battle_updateExtraSlotChipKeepFlag_8028F84 ── */
/* battle_updateExtraSlotChipKeepFlag_8028F84: update the "this chip is still on the field" flag (offset 7) of
 * active chip slot 8.
 *
 * getLocOfActiveChips_8027E1C(n) returns a pointer to active-chip slot record n.
 * The flag is cleared only when a chip is in slot 8, its flag is neither 2 nor 3,
 * the chip folder selection at R5 names another chip (byte +8 != 0), and that
 * other chip no longer has an active slot (its record's state byte is 0).
 * In every other reachable case the flag is set to 1; when slot 8 is empty or
 * its flag is 2 nothing is written. */
Next battle_updateExtraSlotChipKeepFlag_8028F84(void) {
    R0 = 8;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    u32 slot = R0;

    /* No chip occupying slot 8: nothing to do. */
    if (LD8(slot) != 8)
        RETURN_TO_CALLER();

    u8 state = LD8(slot + 7);

    /* Flag 2 means another handler already took care of this slot. */
    if (state == 2)
        RETURN_TO_CALLER();

    u8 keep;
    if (state == 3) {
        keep = 1;
    } else {
        /* R5 holds the chip-selection state: byte +8 is the currently
         * selected chip's index (0 = none), and the table at +0x47 maps
         * that index to the partner chip's slot number. */
        u32 sel = LD8(R5 + 8);
        if (sel == 0) {
            keep = 1;
        } else {
            R0 = LD8(R5 + sel + 0x47);
            GUEST_CALL(getLocOfActiveChips_8027E1C);
            /* The partner's slot record is empty -> the chip is gone. */
            keep = (LD8(R0) != 0);
        }
    }

    ST8(slot + 7, keep);
    RETURN_TO_CALLER();
}

/* ── sub_8029032 ── */
#define TILE_VRAM_SLOT    0x1FF
#define SOUND_CONFIRM     0x1D2  /* 0xd3 + 0xff */
#define CHIP_ATTR_OFFSET  0x55   /* base offset for pending-jack-in attribute */

/* register form: calls PlaySoundEffect outside this file, which has no typed C interface yet */
static void play_sound_sub_8029032(u32 sound_id) {
    R0 = sound_id;
    GUEST_CALL(PlaySoundEffect);
}

/* register form: calls battle_ClearPendingEventFlag_802A0EC outside this file, which has no typed C interface yet */
static void clear_pending_event_flag(void) {
    GUEST_CALL(battle_ClearPendingEventFlag_802A0EC);
}

/* register form: calls getLocOfActiveChips_8027E1C outside this file, which has no typed C interface yet */
static u32 get_active_chips_loc(u32 index) {
    R0 = index;
    GUEST_CALL(getLocOfActiveChips_8027E1C);
    return R0;
}

/* register form: calls sprite_CopyObjTilesToVramSlot_80281D4 outside this file, which has no typed C interface yet */
static void copy_obj_tiles(u32 slot, u32 counter, u32 chip_ptr) {
    R0 = slot;
    R1 = counter;
    R4 = chip_ptr;
    GUEST_CALL(sprite_CopyObjTilesToVramSlot_80281D4);
}

/* register form: calls sub_8029CD4 outside this file, which has no typed C interface yet */
static void update_chip_display(u32 count) {
    R0 = count; R1 = 0; R2 = 0;
    GUEST_CALL(sub_8029CD4);
}

/* register form: calls sub_802A00C outside this file, which has no typed C interface yet */
static u32 check_chip_condition(u32 value, u32 chip_ptr) {
    R0 = value;
    R4 = chip_ptr;
    GUEST_CALL(sub_802A00C);
    return R0;
}

/* register form: calls customScreen_ExitSequence_8028E32 outside this file, which has no typed C interface yet */
static void exit_custom_screen(void) {
    GUEST_CALL(customScreen_ExitSequence_8028E32);
}

/* register form: calls sub_8028476 outside this file, which has no typed C interface yet */
static void post_exit_cleanup(void) {
    GUEST_CALL(sub_8028476);
}

/* Handle the confirm/exit action on the custom battle-screen chip submenu.
 * state_ptr points to screen state: byte at +8 = selection count,
 * +0x19 = jack-in pending flag, +0x1a/+0x1b/+0x55+idx = pending attribute slots. */
static void custom_screen_exit_action(u32 state_ptr) {
    u8 selection = LD8(state_ptr + 8);

    if (selection == 0) {
        if (LD8(state_ptr + 0x19) == 0) {
            play_sound_sub_8029032(SOUND_CANT_JACK_IN);
            return;
        }
        u8 pending_idx = LD8(state_ptr + 0x1b);
        ST8(state_ptr + CHIP_ATTR_OFFSET + pending_idx, 0);
        ST8(state_ptr + 0x1a, 0);
        ST8(state_ptr + 0x19, 0);
        play_sound_sub_8029032(SOUND_CONFIRM);
        clear_pending_event_flag();
    } else {
        u32 chip_ptr = get_active_chips_loc(LD8(state_ptr + selection + 0x47));

        if (LD8(chip_ptr) == 2) {
            u8 new_sel = LD8(state_ptr + 8) - 1;
            ST8(state_ptr + 8, new_sel);
            copy_obj_tiles(TILE_VRAM_SLOT, new_sel, chip_ptr);
            update_chip_display(new_sel);
            clear_pending_event_flag();
            play_sound_sub_8029032(SOUND_EXIT_SUBMENU);
            play_sound_sub_8029032(SOUND_CONFIRM);
            ST8(state_ptr + 0x17, 0);
        } else {
            u16 chip_data = LD16(LD32(chip_ptr + 8));
            if (check_chip_condition(chip_data, chip_ptr) != 0) {
                play_sound_sub_8029032(SOUND_EXIT_SUBMENU);
                play_sound_sub_8029032(SOUND_CONFIRM);
                clear_pending_event_flag();
            }
            u8 new_sel = LD8(state_ptr + 8) - 1;
            ST8(state_ptr + 8, new_sel);
            copy_obj_tiles(TILE_VRAM_SLOT, new_sel, chip_ptr);
            update_chip_display(new_sel);

            if (LD8(chip_ptr + 5) != 0) {
                u8 extra = LD8(chip_ptr + 5);
                ST8(chip_ptr + 5, 0);
                ST8(chip_ptr + 4, LD8(chip_ptr + 4) & ~0x3E);
                u32 extra_loc = get_active_chips_loc(extra);
                ST8(extra_loc + 7, 0);
            }
        }
        ST8(chip_ptr + 7, 0);
    }
    exit_custom_screen();
    post_exit_cleanup();
    play_sound_sub_8029032(SOUND_EXIT_SUBMENU);
}

/* sub_8029032(r5 = screen state ptr) -> outputs r2 r3 r5 r6 r7 r8 r9 r10 r11 r12; preserves r4 r7 */
Next sub_8029032(void) {
    u32 saved_r4 = R4;
    custom_screen_exit_action(R5);
    R4 = saved_r4;
    RETURN_TO_CALLER();
}

/* ── sub_8029224 ── */
/* Chip command queue base in work RAM. */
#define CHIP_QUEUE_BASE 0x2033000

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_sub_8029224(u32 chip_id) {
    R0 = chip_id;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* Walk the chip-use command queue at CHIP_QUEUE_BASE, processing opcode slots.
 *
 * Each slot holds a s16 opcode; the preceding u16 is the chip index.
 * Recognised opcodes (0xB8, 0xB9, 0xC0, 0xC1, 0xC3) are validated against the
 * chip's attribute byte (offset 9 of ChipData).  On success the side-effect is
 * applied, a status bit from the target object is merged, and the entry is
 * removed by shifting all following slots down until the 0xFFFF sentinel.
 * The scanner then re-reads the same slot to pick up the shifted-in entry.
 *
 * NOTE: r3 (the target object for the merge) is clobbered by the shift loop,
 * exactly as in the original — subsequent iterations use the stale pointer.  */
static void process_chip_command_queue(u32 target_obj) {
    u32 r3 = target_obj;
    u32 pos = CHIP_QUEUE_BASE + 2;

    for (;;) {
        s16 code = LDS16(pos);
        if (code == -1)
            break;

        /* ---- dispatch: required / forbidden attribute masks ---- */
        u32 need_mask, forbid_mask;
        switch (code) {
        case 0xC0: case 0xC3: case 0xB8:
            need_mask = 2;  forbid_mask = 0; break;
        case 0xC1:
            need_mask = 4;  forbid_mask = 0; break;
        case 0xB9:
            need_mask = 2;  forbid_mask = 1; break;
        default:
            pos += 2;
            continue;
        }

        /* ---- look up chip data for the index stored just before the opcode ---- */
        u32 chip_base = pos - 2;
        u32 chip = get_chip_data_sub_8029224(LD16(chip_base));
        u8 attr = LD8(chip + 9);

        if ((attr & need_mask) == 0 || (attr & forbid_mask) != 0) {
            pos += 2;
            continue;
        }

        /* ---- per-command side effect on the queue entry ---- */
        code = LDS16(pos);  /* reload; original does a second ldrsh here */
        if (code == 0xB8) {
            ST16(chip_base + 0x3C, LD16(chip_base + 0x3C) | 2);
        } else if (code == 0xB9) {
            ST16(chip_base + 0x3C, LD16(chip_base + 0x3C) | 4);
        } else {
            /* 0xC0 / 0xC1 / 0xC3: accumulate field +0xC into +0x16 */
            ST16(pos + 0x16, LD16(pos + 0x16) + LD16(pos + 0xC));
        }

        /* ---- merge bit 0 of r3+0x3C into r3+0x3A ---- */
        u16 bit0 = LD16(r3 + 0x3C) & 1;
        ST16(r3 + 0x3A, LD16(r3 + 0x3A) | bit0);

        /* ---- shift every following slot down by 2 bytes until sentinel ---- */
        u32 dst = pos;
        r3 = pos + 2;  /* src — this overwrites the caller's r3 (clobbered) */

        for (;;) {
            ST16(dst + 0x3C, LD16(r3 + 0x3C));
            ST16(dst + 0x0C, LD16(r3 + 0x0C));
            ST16(dst + 0x24, LD16(r3 + 0x24));
            s16 moved = LDS16(r3);
            ST16(dst, moved);
            r3 += 2;
            dst += 2;
            if (moved == -1)
                break;
        }

        pos -= 2;  /* back up so the +2 below keeps us on the same slot */
        pos += 2;  /* net no-op: re-examine the slot that now holds the shifted-in entry */
    }
}

/* sub_8029224(r3 = target object) — no outputs, clobbers r0–r3, flags, lr */
Next sub_8029224(void) {
    process_chip_command_queue(R3);
    RETURN_TO_CALLER();
}

/* ── battle_compactFolderChipList_802945A ── */
/* battle_compactFolderChipList_802945A: compact the battle folder chip list (30 shorts at eBattleFolder).
 * Drops every empty slot (ID 0xFFFF), shifts the kept entries to the front,
 * and pads the freed tail with 0xFFFF so the list always has 30 entries.
 * r4/r6/r7 are only used as scratch by the original, which pushes and pops
 * them, so their values are preserved for the caller. */
Next battle_compactFolderChipList_802945A(void) {
    u32 savedR4 = R4, savedR6 = R6, savedR7 = R7;

    u32 folder = A_eBattleFolder;
    u32 emptyId = 0xFFFFFFFFu;  /* 0xFFFF sign-extended, the "no chip" ID */
    u32 readOff = 0;            /* byte offset of slot being examined */
    u32 writeOff = 0;           /* byte offset of next free slot */
    u32 slotsToFill = 30;       /* entries still needed to fill the list */

    while (readOff < 0x3C) {
        s32 entry = LDS16(folder + readOff);
        if (entry != (s32)emptyId) {
            ST16(folder + writeOff, (u16)entry);
            writeOff += 2;
            slotsToFill--;
        }
        readOff += 2;
    }

    /* Pad whatever is left of the list with empty slots. */
    while (slotsToFill != 0) {
        ST16(folder + writeOff, (u16)emptyId);
        writeOff += 2;
        slotsToFill--;
    }

    R4 = savedR4;
    R6 = savedR6;
    R7 = savedR7;
    RETURN_TO_CALLER();
}

/* ── sub_80295C8 ── */
/* Scans a chip/program u16 sequence in a battle buffer.
 * First verifies `count` consecutive words all match the target at [data_ptr+4],
 * then walks backwards through a trailing region validating a descending value
 * pattern (0x1A is a wildcard, at most one allowed, value must not go negative).
 * Returns field [data_ptr+2] and adjusted offset on success, or 0 on failure. */

typedef struct {
    u32 r0;       /* result: field from data_ptr+2 on success, 0 on failure */
    u32 r1;       /* on success: r6-8; on failure: last r1 value from loop */
    u32 flag_a;   /* left operand of the final CMP/SUBS for flags */
    u32 flag_b;   /* right operand of the final CMP/SUBS for flags */
    u32 nz_val;   /* value for the final MOVS (0 on failure, r1 on success) */
} SeqScanResult;

static SeqScanResult scan_chip_sequence(u32 count, u32 data_ptr, u32 offset, u32 base) {
    u32 r6 = offset;
    u32 target = LD16(data_ptr + 4);
    u32 r0, r1 = target;

    /* First pass: verify `count` consecutive u16s all equal target */
    u32 remaining = count;
    for (;;) {
        r0 = LD16(base + r6);
        if (r0 != target)
            return (SeqScanResult){0, r1, r0, target, 0};
        r6 += 2;
        remaining--;
        if (remaining == 0)
            break;
    }

    /* Advance past matched block into the trailing descending region */
    u32 wildcard_count = 0;
    r6 += 8;
    r0 = LD16(base + r6) + 1;

    /* Second pass: walk `count` entries validating descending pattern */
    u32 iterations = count;
    for (;;) {
        r1 = LD16(base + r6);
        if (r1 == 0x1A) {
            /* First wildcard at the boundary: re-derive expected from prior entry */
            if (wildcard_count == 0 && r0 == 0x1B)
                r0 = LD16(base + r6 - 2) + 2;
            r1 = r0 - 1;
            wildcard_count++;
        }
        r0--;
        if (r0 != r1)
            return (SeqScanResult){0, r1, r0, r1, 0};
        r6 -= 2;
        iterations--;
        if (iterations == 0)
            break;
    }

    /* Final constraints: at most one wildcard, value must not go below 0 */
    if ((s32)wildcard_count > 1)
        return (SeqScanResult){0, r1, wildcard_count, 1, 0};
    if ((s32)r0 < 0)
        return (SeqScanResult){0, r1, r0, 0, 0};

    /* Success: return data field and adjusted scan position */
    r0 = LD16(data_ptr + 2);
    r1 = r6 - 8;
    return (SeqScanResult){r0, r1, r6, 8, r1};
}

/* sub_80295C8(r2 = count, r3 = data_ptr, r6 = buffer_offset, r7 = buffer_base) -> r0 r1 nzcv */
Next sub_80295C8(void) {
    SeqScanResult res = scan_chip_sequence(R2, R3, R6, R7);
    R0 = res.r0;
    R1 = res.r1;
    SET_FLAGS_CMP(res.flag_a, res.flag_b);
    SET_FLAGS_NZ(res.nz_val);
    RETURN_TO_CALLER();
}

/* ── matchHalfwordPatternEntry_802961A ── */
/* matchHalfwordPatternEntry_802961A: compare a run of halfwords against a reference list.
 * r2 = number of halfwords to check, r3 = pointer to a reference record
 * (result halfword at +2, expected halfword list at +4), r6 = source
 * stride, r7 = source base pointer. The halfwords at r7+r6, r7+r6+2, ...
 * are compared against the list at r3+4, r3+6, ... for r2 entries.
 * If all of them match, r0 = halfword at r3+2 and r1 = r6; otherwise
 * r0 = 0 and r1 = the expected halfword that did not match.
 * Flags: nz from the final result move, c/v from the last comparison;
 * r2, r4 and r7 are preserved. */
Next matchHalfwordPatternEntry_802961A(void) {
    u32 remaining = R2;
    u32 src = R7 + R6; /* halfwords being tested */
    u32 ref = R3 + 4;  /* expected halfword list */

    u32 actual, expected;
    do {
        actual = LD16(src);
        expected = LD16(ref);
        /* the comparison is the last instruction that sets C and V */
        SET_FLAGS_CMP(actual, expected);
        if (actual != expected) {
            /* mismatch: r0 = 0, r1 = the expected halfword that failed */
            R0 = 0;
            R1 = expected;
            SET_FLAGS_NZ(R0);
            RETURN_TO_CALLER();
        }
        src += 2;
        ref += 2;
    } while (--remaining != 0);

    /* all halfwords matched (last comparison was equal, so C=1, V=0):
     * return the record's result halfword */
    R0 = LD16(R3 + 2);
    R1 = R6;
    SET_FLAGS_NZ(R1);
    RETURN_TO_CALLER();
}

/* ── custMenu_80298F4 ── */
/* First battle tutorial custMenu handler.
 *
 * The menu struct's state byte at +0x0C selects one of six tutorial checks.
 * Each check returns the text script index to show, or 0 when there is
 * nothing to show this frame. When a script is produced it is played through
 * the chatbox (TextScriptFirstBattleTutorials is a table of text archives,
 * one entry per tutorial state), then the menu is parked while the text
 * plays: the old value of the timer byte at +0x01 is saved to +0x0D, the
 * timer is restarted at 0x34 and the halfword counter at +0x02 is cleared.
 *
 * Returns 1 if a script was started, 0 otherwise (state 0xFF, states with no
 * check, or a check that returned nothing).
 *
 * Only the menu pointer (r5) is actually used; r4/r6/r7/r10 pass through
 * untouched, and r2/r3 are left exactly as the called game routines leave
 * them, matching the original.
 */

/* register form: calls sub_802996E outside this file, which has no typed C interface yet */
static u32 tutorial_script_0(u32 state) {
    R0 = state;
    GUEST_CALL(sub_802996E);
    return R0;
}

/* register form: calls sub_80299B6 outside this file, which has no typed C interface yet */
static u32 tutorial_script_1(u32 state) {
    R0 = state;
    GUEST_CALL(sub_80299B6);
    return R0;
}

/* register form: calls sub_80299FA outside this file, which has no typed C interface yet */
static u32 tutorial_script_3(u32 state) {
    R0 = state;
    GUEST_CALL(sub_80299FA);
    return R0;
}

/* register form: calls sub_8029A1E outside this file, which has no typed C interface yet */
static u32 tutorial_script_4(u32 state) { /* tutorial states 4 and 5 share this check */
    R0 = state;
    GUEST_CALL(sub_8029A1E);
    return R0;
}

/* register form: calls sub_8029A36 outside this file, which has no typed C interface yet */
static u32 tutorial_script_6(u32 state) {
    R0 = state;
    GUEST_CALL(sub_8029A36);
    return R0;
}

/* register form: calls chatbox_runScript outside this file, which has no typed C interface yet */
static void run_chatbox_script(u32 archive, u32 script_idx) {
    R0 = archive;
    R1 = script_idx;
    GUEST_CALL(chatbox_runScript);
}

static u32 first_battle_tutorial_menu(u32 menu) {
    u32 state = LD8(menu + 0xC);

    u32 script;
    switch (state) {
    case 0:  script = tutorial_script_0(state); break;
    case 1:  script = tutorial_script_1(state); break;
    case 3:  script = tutorial_script_3(state); break;
    case 4:
    case 5:  script = tutorial_script_4(state); break;
    case 6:  script = tutorial_script_6(state); break;
    default: return 0; /* 0xFF: no tutorial active; state 2 and others have no check */
    }

    if (script == 0)
        return 0; /* the check found nothing to say this frame */

    /* Pick the text archive for this tutorial state and start the script. */
    u32 archive = LD32(A_TextScriptFirstBattleTutorials + state * 4);
    run_chatbox_script(archive, script);

    /* Park the menu while the text is on screen. */
    ST8(menu + 0xD, LD8(menu + 1)); /* remember the previous timer value */
    ST8(menu + 1, 0x34);            /* restart the timer at 0x34 ticks */
    ST16(menu + 2, 0);              /* clear the halfword counter */
    return 1;
}

/* custMenu_80298F4(r5 = custMenu) -> r0; r2/r3 reflect the callees, r4-r7/r10 untouched */
Next custMenu_80298F4(void) {
    R0 = first_battle_tutorial_menu(R5);
    RETURN_TO_CALLER();
}
