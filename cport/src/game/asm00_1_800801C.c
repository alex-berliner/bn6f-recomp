/*
 * asm00_1_800801C.c: readable functions of bn6f asm00_1.s, from 0x0800801C, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── battle_runMainStep_800801C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_sub_802DE5C_result;
/* register form: generated wrapper: battleObject_runAIScript_802DE5C has no typed C interface yet */
static inline call_sub_802DE5C_result call_sub_802DE5C(void) {
    GUEST_CALL(battleObject_runAIScript_802DE5C);
    return (call_sub_802DE5C_result){ R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* Battle-object state dispatcher: reads the current state byte from the object's
 * base pointer, jumps to the matching handler in a table of ten, runs the shared
 * post-processing routine battleObject_runAIScript_802DE5C, then loads the result byte. */

/* register form: dispatches through the jump table at off_8008038 whose byte-
 * scaled index comes from the object's state byte; the handler expects R0 and R5
 * set up before entry, other caller registers pass through. */
static u32 dispatch_battle_object_state(void) {
    u32 state_base = A_dword_203CA70;
    u32 state_byte = LD8(state_base);
    u32 handler_addr = LD32(A_off_8008038 + state_byte);

    R0 = state_byte;
    R5 = state_base;
    GUEST_CALL_ADDR(handler_addr);

    call_sub_802DE5C();

    /* Result byte at object offset +4 (byte_203CA74 – dword_203CA70). */
    return LD8(state_base + (A_byte_203CA74 - 0x203CA70));
}

/* battle_runMainStep_800801C: r2 r3 r4 r6 r7 r10 lr (pass-through) -> r0 r2..r12 */
Next battle_runMainStep_800801C(void) {
    u32 caller_r5 = R5;          /* preserved via push/pop in the original */
    R0 = dispatch_battle_object_state();
    R5 = caller_r5;
    RETURN_TO_CALLER();
}

/* ── battle_runLoserSequence_800825A ── */
/* battle_runLoserSequence_800825A: once-per-frame update of the post-battle wait state.
 * controller: block with the oEndCtrl_* fields; toolkit: Toolkit holding the
 * battle and game state pointers. preserved_r4 is the caller's r4, which the
 * original keeps in a stack slot across the update and pops on return, so the
 * typed function has to carry it through unchanged.
 * The first call wraps the finished fight up (flag bookkeeping, outcome copy,
 * loser jingle, link message); every call ticks the countdown and ends the
 * wait once the comm side releases the fight and the countdown has run out. */

/* Controller block pointed to by r5 (field offsets as in the disassembly). */
#define oEndCtrl_Step   3    /* u8:  0 until the end-of-battle wrap-up has run once */
#define oEndCtrl_Next   4    /* u8:  hand-over state, written when the wait ends */
#define oEndCtrl_Timer  8    /* s16: frames left before the wait ends */

/* Event-flag batch that is dispatched and then cleared again below. */
#define END_OF_BATTLE_FLAGS 0xE4C53

/* Helpers for calls whose only interface is the guest ABI (typed wrappers are
 * not available yet). Each sets the input registers, makes one call and returns
 * the result the disassembly leaves in r0. */
static void dispatch_flags(u32 flags) { /* register form: calls dispatch_801DACC, which has no typed C interface yet (noted by the tool) */
    R0 = flags;
    GUEST_CALL(dispatch_801DACC);
}
static void clear_flag_bits(u32 flags) { /* register form: calls flags_clearBits_eStruct2035280_801BED6, which has no typed C interface yet (noted by the tool) */
    R0 = flags;
    GUEST_CALL(flags_clearBits_eStruct2035280_801BED6);
}
static u32 get_battle_effects_sub_800825A(void) { /* register form: calls GetBattleEffects, which has no typed C interface yet (noted by the tool) */
    GUEST_CALL(GetBattleEffects);
    return R0;
}
static void play_music(u32 song) { /* register form: calls PlayMusic, which has no typed C interface yet (noted by the tool) */
    R0 = song;
    GUEST_CALL(PlayMusic);
}
static u32 battle_get_phase_mode(void) { /* register form: calls battle_getPhaseMode_800A152, which has no typed C interface yet (noted by the tool) */
    GUEST_CALL(battle_getPhaseMode_800A152);
    return R0;
}
static u32 battle_800A8B2(u32 phase_mode) { /* register form: calls battle_getLoserEffectTimer, which has no typed C interface yet (noted by the tool) */
    R0 = phase_mode;
    GUEST_CALL(battle_getLoserEffectTimer);
    return R0;
}
static void comm_801E792(u32 value) { /* register form: calls battle_dispatchKindEffect_801E792, which has no typed C interface yet (noted by the tool) */
    R0 = value;
    GUEST_CALL(battle_dispatchKindEffect_801E792);
}
static u32 cross_net_battle_ready(void) { /* register form: calls comm_checkCrossNetBattleReady_801E754, which has no typed C interface yet (noted by the tool) */
    GUEST_CALL(comm_checkCrossNetBattleReady_801E754);
    return R0;
}
static void set_unk02(u32 value) { /* register form: calls eStruct200A008_setUnk02, which has no typed C interface yet (noted by the tool) */
    R0 = value;
    GUEST_CALL(eStruct200A008_setUnk02);
}

/* register form: pulls a pair of bytes from a guest address as a s16 via the
 * signed halfword load (the original uses "ldrh r0,[r5,r0]; tst r0,r0" on a
 * value the CPU produced with ldrsh, so the memory must be read with the same
 * sign extension). */
static s16 timer_signed(u32 controller) {
    return (s16)LDS16(controller + oEndCtrl_Timer);
}

static u32 update_battle_end_wait(u32 controller, u32 toolkit, u32 preserved_r4) {
    if (LD8(controller + oEndCtrl_Step) == 0) {
        /* one-time wrap-up of the finished fight */
        dispatch_flags(END_OF_BATTLE_FLAGS);
        clear_flag_bits(END_OF_BATTLE_FLAGS);

        /* store the flipped outcome flag (win <-> lose) in the battle state */
        u32 battle = LD32(toolkit + oToolkit_BattleStatePtr);
        ST8(battle + oBattleState_LoserSide, LD8(battle + oBattleState_PlayerSide) ^ 1);

        if (get_battle_effects_sub_800825A() & 8)        /* bit 3: the player lost */
            play_music(SONG_LOSER);

        /* bit 1 shortens the wait from 0x66 to 0x5e frames */
        u16 wait;
        if (get_battle_effects_sub_800825A() & 2)
            wait = 0x5E;
        else
            wait = 0x66;
        ST16(controller + oEndCtrl_Timer, wait);

        /* tell the link layer how the fight ended (phase mode 7 sends a fixed 0x18) */
        u32 phase_mode = battle_get_phase_mode();
        u32 link_value = battle_800A8B2(phase_mode);
        if (phase_mode == 7)
            link_value = 0x18;
        comm_801E792(link_value);

        ST8(controller + oEndCtrl_Step, 4);
    }

    /* tick the countdown one frame */
    ST16(controller + oEndCtrl_Timer, (u16)(LD16(controller + oEndCtrl_Timer) - 1));

    if (cross_net_battle_ready() != 0)
        return preserved_r4;                  /* link side still in control: don't end the wait locally */

    if (timer_signed(controller) > 0)
        return preserved_r4;                  /* countdown not finished yet */

    /* wait is over: hand the controller and the game state to the follow-up */
    ST8(controller + oEndCtrl_Next, 2);
    ST8(LD32(toolkit + oToolkit_GameStatePtr) + oGameState_BattleEndSubState, 2);
    set_unk02(1);

    return preserved_r4;
}

/* battle_runLoserSequence_800825A(r5 = controller, r10 = toolkit); r4 leaves as the caller passed it */
Next battle_runLoserSequence_800825A(void) {
    R4 = update_battle_end_wait(R5, R10, R4);
    RETURN_TO_CALLER();
}

/* ── sub_800840C ── */
/* sub_800840C: pre-battle intro step; driven by a small state byte at r5.
 * On the first call it copies some preloaded graphics to two VRAM buffers and
 * marks that step done. When the global "intro busy" flag (battle_runControllerStep_801483C) clears
 * it either arms a 4-tick local timer or, once that has elapsed, finds both
 * players' BattleObjects, runs them through sub_8015A38, and writes 4 into [r5]. */
Next sub_800840C(void) {
    u8 phase = LD8(R5 + 3);

    if (phase == 0) {
        R0 = A_byte_203F558;
        R1 = A_byte_203F658;
        GUEST_CALL(sub_80147E4);
        ST8(R5 + 3, 4);
    }

    GUEST_CALL(battle_runControllerStep_801483C);
    if (R0 != 0) {
        RETURN_TO_CALLER();
    }

    if (LD8(R5 + 2) == 0) {
        GUEST_CALL(timer_resetAndStart_801482C);
        ST8(R5 + 2, 4);
        RETURN_TO_CALLER();
    }

    R0 = 0;
    GUEST_CALL(battle_findPlayer);
    GUEST_CALL(sub_8015A38);

    R0 = 1;
    GUEST_CALL(battle_findPlayer);
    GUEST_CALL(sub_8015A38);

    ST32(R5, 4);
    RETURN_TO_CALLER();
}

/* ── battle_objAdvanceStateTimer_8008492 ── */
/* battle_objAdvanceStateTimer_8008492: advance the per-object battle state byte of the object in r5.
 *
 * - In battle mode 5, or when the 0x40 battle flag is clear, the object is
 *   driven by battle_runControllerStep_801483C: once it reports "done" (r0 == 0) and the object's
 *   state byte at +2 is still zero, restart the timer and store state 4.
 * - In every other case (flag test passes, or the state byte was already
 *   non-zero) store state 6 at +4 instead.
 *
 * Flags on return follow the original's last flag-setting instruction on
 * each path: tst r0 (battle_runControllerStep_801483C result) or the mov of the stored constant.
 * c/v are left as the last callee left them, matching the original. */
Next battle_objAdvanceStateTimer_8008492(void) {
    GUEST_CALL(GetBattleMode);

    if (R0 != 5) {
        GUEST_CALL(TestBattleFlag_0x40); /* answers in Z; the original branches on it */
        if (!ZERO_FLAG()) {
            R0 = 6;
            SET_FLAGS_NZ(R0);           /* mov r0, #6 */
            ST8(R5 + 4, R0);
            RETURN_TO_CALLER();
        }
    }

    GUEST_CALL(battle_runControllerStep_801483C);
    SET_FLAGS_NZ(R0);                   /* tst r0, r0 */
    if (R0 != 0)
        RETURN_TO_CALLER();

    if (LD8(R5 + 2) != 0) {
        R0 = 6;
        SET_FLAGS_NZ(R0);               /* mov r0, #6 */
        ST8(R5 + 4, R0);
        RETURN_TO_CALLER();
    }

    GUEST_CALL(timer_resetAndStart_801482C);
    R0 = 4;
    SET_FLAGS_NZ(R0);                   /* mov r0, #4 */
    ST8(R5 + 2, R0);
    RETURN_TO_CALLER();
}

/* ── clearDword_203CA70Record_80084C0 ── */
/* clearDword_203CA70Record_80084C0: clear the 12-byte record at dword_203CA70 (3 words).
 * Flags, r2, r3 and r5 are left exactly as ZeroFillByWord leaves them. */
Next clearDword_203CA70Record_80084C0(void) {
    R0 = A_dword_203CA70;  /* block to clear */
    R1 = 0xC;              /* size in bytes */
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── battle_runBattlePhase_8009158 ── */
/* Battle state machine tick: runs the current state's phase handler (chosen through the
 * jump table at off_80091BC), then keeps the low-HP warning siren in step with the
 * player's HP: it starts when HP falls to a quarter of max or below and stops again
 * once HP climbs back above that. */

/* GetBattleEffects() bit that enables the low-HP warning check */
#define BATTLE_EFFECTS_LOW_HP_WARNING 0x8

/* Sound ids used by the warning (both sound calls take 0x1F as their first argument) */
#define SOUND_LOW_HP_WARNING 0x11A /* looping low-HP siren          */
#define SOUND_LOW_HP_OFF     0x100 /* stops the siren               */

/* register form: calls one of the battle state phase handlers from the jump table at
 * off_80091BC outside this file, which have no typed C interface yet. The index field
 * holds a byte offset into the table (4 * state number), and the original passes it in
 * r0 before branching through the table entry. */
static void run_state_phase_handler(u32 table_offset)
{
    R0 = table_offset;
    switch (table_offset) {
    case 0x00: GUEST_CALL(battle_runBattleIntroSpawnStep_80091F0); break;
    case 0x04: GUEST_CALL(battle_dispatchStatePhase_80092A0); break;
    case 0x08: GUEST_CALL(sub_8009338); break;
    case 0x0C: GUEST_CALL(battle_stepRunAndDecideResult_800938A); break;
    case 0x10: GUEST_CALL(battle_DispatchStatePhase_800945C); break;
    case 0x14: GUEST_CALL(battle_initFadeToBattleResult_80094DA); break;
    case 0x18: GUEST_CALL(battle_tutFullSynchroTextStep); break;
    case 0x1C: GUEST_CALL(sub_8009552); break;
    case 0x20: GUEST_CALL(sub_8009594); break;
    case 0x24: GUEST_CALL(sub_80095C8); break;
    default: break; /* out of range: never happens, the index is always 0..9 * 4 */
    }
}

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 get_battle_effects(void)
{
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls battle_preUpdateAndGetPlayerObject_80103EC outside this file, which has no typed C interface yet */
static u32 preupdate_and_get_player_object(void)
{
    GUEST_CALL(battle_preUpdateAndGetPlayerObject_80103EC);
    return R0;
}

/* register form: calls sound_800065A outside this file, which has no typed C interface yet */
static void sound_setup_800065A(u32 channel, u32 param, u32 pitch)
{
    R0 = channel; R1 = param; R2 = pitch;
    GUEST_CALL(sound_800065A);
}

/* register form: calls sound_8000672 outside this file, which has no typed C interface yet */
static void sound_play_8000672(u32 channel, u32 sound_id)
{
    R0 = channel; R1 = sound_id;
    GUEST_CALL(sound_8000672);
}

/* register form: the battle state pointer is carried in r5 and the guest callees above
 * (the phase handler in particular) may leave an updated pointer there; the original
 * addresses the low-HP flag through r5, so the current value can only be had from the
 * register itself */
static u32 battle_state_in_r5(void)
{
    return R5;
}

static void battle_state_update(u32 battle_state)
{
    run_state_phase_handler(LD8(battle_state + oBattleState_Index_01));

    if ((get_battle_effects() & BATTLE_EFFECTS_LOW_HP_WARNING) == 0)
        return;

    u32 player = preupdate_and_get_player_object();
    u16 hp = LD16(player + oBattleObject_HP);
    u16 quarter_max = LD16(player + oBattleObject_MaxHP) / 4;

    battle_state = battle_state_in_r5();
    u16 warning_active = LD16(battle_state + oBattleState_LowHPAlertTimers); /* 1 while the siren plays */

    if (hp > quarter_max) {
        if (warning_active == 0)
            return; /* warning not playing, nothing to stop */
        ST16(battle_state + oBattleState_LowHPAlertTimers, 0);
        sound_setup_800065A(0x1F, 0xFFFF, 0);            /* release the warning tone */
        sound_play_8000672(0x1F, SOUND_LOW_HP_OFF);
    } else {
        if (warning_active != 0)
            return; /* warning already playing */
        ST16(battle_state + oBattleState_LowHPAlertTimers, 1);
        sound_setup_800065A(0x1F, 0xFFFF, 0x100);         /* arm the warning tone */
        sound_play_8000672(0x1F, SOUND_LOW_HP_WARNING);
    }
}

/* battle_runBattlePhase_8009158(r5 = battle state) -> no computed outputs: r2..r6 and r8..r12 carry
 * whatever the phase handler and the sound calls left in them */
Next battle_runBattlePhase_8009158(void) {
    battle_state_update(R5);
    RETURN_TO_CALLER();
}

/* ── task484_RunFrame_800927C ── */
/* task484_RunFrame_800927C: run one frame of the task identified by ID 0x484.
 * Requests the task (setting its request bits), dispatches it, then runs
 * the shared post-dispatch steps and clears the active task state. */
Next task484_RunFrame_800927C(void) {
    /* Request the task and let the dispatcher run it. */
    R0 = 0x484;
    GUEST_CALL(eStruct2035280_SetRequestBits_801BECC);

    R0 = 0x484;
    GUEST_CALL(dispatch_801DA48);

    /* Shared bookkeeping after a dispatched task. */
    GUEST_CALL(battle_initSceneStart_801E5F8);
    GUEST_CALL(battle_sceneFrameSetup_801DA24);

    /* Clear the task state (r0 = r1 = 0). */
    R0 = 0;
    R1 = 0;
    GUEST_CALL(battle_eStruct2035280_postEvent_801E0A0);

    RETURN_TO_CALLER();
}

/* ── battle_dispatchStatePhase_80092A0 ── */
/* battle_dispatchStatePhase_80092A0: battle-state phase dispatcher.
 *
 * r5 points to the battle state; the byte at offset 2 selects an entry in
 * the handler table at off_80092B4 (entries at byte offsets 0, 4 and 8):
 *   0 -> battle_runState4CountdownOrAdvance_80092C0, 4 -> battle_stateIntroEffectWait, 8 -> sub_8009314.
 *
 * The original does `ldr r1, [table, phase]; mov lr, pc; bx r1`, so the
 * handler is entered with r0 = the phase byte and r1 = its own (Thumb)
 * entry address; r2-r12 pass through untouched. The handler's results
 * (registers and flags) flow straight back to our caller. */
Next battle_dispatchStatePhase_80092A0(void) {
    u32 phase = LD8(R5 + 2); /* oBattleState_StatePhase */
    u32 handler = LD32(A_off_80092B4 + phase);

    R0 = phase;
    R1 = handler;

    switch (handler) {
    case 0x080092C1: /* battle_runState4CountdownOrAdvance_80092C0 + 1 (Thumb) */
        GUEST_CALL(battle_runState4CountdownOrAdvance_80092C0);
        break;
    case 0x080092F1: /* battle_stateIntroEffectWait + 1 (Thumb) */
        GUEST_CALL(battle_stateIntroEffectWait);
        break;
    case 0x08009315: /* sub_8009314 + 1 (Thumb) */
        GUEST_CALL(sub_8009314);
        break;
    default:
        /* Out-of-range phase: the original would jump to whatever word the
         * table lookup produced; no valid game state reaches this. */
        break;
    }

    RETURN_TO_CALLER();
}

/* ── battle_runState4CountdownOrAdvance_80092C0 ── */
/* battle_runState4CountdownOrAdvance_80092C0: battle-flow helper driven from the battle state object in r5.
 *
 * - If the state byte at offset 3 is already running, count the 16-bit timer
 *   (oBattleState_CountdownTimer) down; while it is still positive just return with
 *   the decremented timer in r0. When it expires, write 4 into the 16-bit
 *   field at offset 2 (whose upper half is the state byte, so this also
 *   clears it) and return.
 * - Otherwise, if the pending flag oBattleState_State4EventCount is set, start a fresh
 *   countdown of 10, set the state byte to 4 and return.
 * - Otherwise hand control to battle state 8 with sub-state 0.
 *
 * Outputs: r0 = value written by the last MOV (decremented timer on the
 * still-counting path, 4 when the countdown starts/expires, 0 when switching
 * to state 8), r1 = the pending flag read on the state-0 path (0 when it was
 * clear), flags as the original's last flag-setting instruction leaves them
 * (nz from that MOV, c/v left over from the timer SUBS where it ran). */
Next battle_runState4CountdownOrAdvance_80092C0(void) {
    if (LD8(R5 + 3) != 0) {
        /* State already running: tick the countdown timer. */
        u32 timer = LD16(R5 + oBattleState_CountdownTimer);
        SET_FLAGS_CMP(timer, 1);
        ST16(R5 + oBattleState_CountdownTimer, timer - 1);
        if ((s32)(timer - 1) >= 0) {
            /* Still counting down. */
            R0 = timer - 1;
        } else {
            /* Timer expired: state 4 via the halfword at offset 2. */
            R0 = 4;
            SET_FLAGS_NZ(4);
            ST16(R5 + 2, 4);
        }
    } else {
        /* Idle: check whether something is pending. */
        R1 = LD8(R5 + oBattleState_State4EventCount);
        if (R1 != 0) {
            /* Begin a fresh 10-frame countdown and enter state 4. */
            ST16(R5 + oBattleState_CountdownTimer, 10);
            R0 = 4;
            SET_FLAGS_NZ(4);
            ST8(R5 + 3, 4);
        } else {
            /* Nothing pending: switch to battle state 8, sub-state 0. */
            ST8(R5 + 1, 8);
            R0 = 0;
            SET_FLAGS_NZ(0);
            ST16(R5 + 2, 0);
        }
    }
    RETURN_TO_CALLER();
}

/* ── sub_8009338 ── */
/* sub_8009338 (r5 = battle state):
 * Per-frame tick of the battle-end sequence.
 *
 * The first time through (byte at +3 still clear), raise flag 4 in the
 * battle state's unk11 flag set, mark the sequence as started and run the
 * one-time setup in battle_initCustomScreen_8026840. Every frame then polls sub_8026A28:
 *   0 -> sequence still running, nothing to do this frame;
 *   2 -> run battle_stateGotoPhase14_800AABC (one final step) and return;
 *   otherwise the sequence is over: flag both fighters' AI data and switch
 *   the battle state machine to index 0xC with the halfword at +2 cleared. */
Next sub_8009338(void) {
    if (LD8(R5 + 3) == 0) {
        R0 = 4;
        GUEST_CALL(setBattleStateUnk11Flag_800A9CA);
        ST8(R5 + 3, 1);
        GUEST_CALL(battle_initCustomScreen_8026840);
    }

    GUEST_CALL(sub_8026A28);
    u32 phase = R0;

    if (phase == 0) {
        /* Still in progress: return with the flags of "cmp r0, #0". */
        SET_FLAGS_CMP(phase, 0);
        RETURN_TO_CALLER();
    }

    if (phase == 2) {
        GUEST_CALL(battle_stateGotoPhase14_800AABC);
        RETURN_TO_CALLER();
    }

    /* Battle finished: tell the AI on both sides (alliance 0 and 1). */
    R0 = 0;
    GUEST_CALL(battle_findPlayer);
    SET_FLAGS_CMP(R0, 0);              /* cmp r0, #0 */
    if (R0 != 0) {
        R0 = LD32(R0 + oBattleObject_AIDataPtr);
        R1 = 1;
        ST8(R0 + oAIData_BeastOutLockoutTimer, R1);
    }

    R0 = 1;
    GUEST_CALL(battle_findPlayer);
    SET_FLAGS_CMP(R0, 0);              /* cmp r0, #0 */
    if (R0 != 0) {
        R0 = LD32(R0 + oBattleObject_AIDataPtr);
        R1 = 1;
        ST8(R0 + oAIData_BeastOutLockoutTimer, R1);
    }

    /* Advance to battle state 12 and clear the counter halfword. */
    R0 = 0xC;
    ST8(R5 + 1, R0);
    SET_FLAGS_NZ(R0);                  /* mov r0, #0xC */
    R0 = 0;
    ST16(R5 + 2, R0);
    SET_FLAGS_NZ(R0);                  /* mov r0, #0 */
    RETURN_TO_CALLER();
}

/* ── battle_DispatchStatePhase_800945C ── */
/* battle_DispatchStatePhase_800945C: battle-state phase dispatcher.
 *
 * R5 points to the battle state; the phase byte at offset 2 selects one of
 * two handlers from the jump table at off_8009470. The phase byte is passed
 * to the handler in r0, and — exactly as the original's `ldr r1, [r1, r0]`
 * leaves it — r1 holds the selected table entry (the handler's Thumb
 * address, bit 0 set) when the handler starts.
 *
 * The original pushes lr before the call, so the handler runs with SP four
 * bytes lower than on entry; that slot has to exist here too. r2-r12 and
 * the incoming flags pass through to the handler untouched, and whatever
 * the handler leaves in the registers and flags is returned to our caller. */
Next battle_DispatchStatePhase_800945C(void) {
    u32 phase = LD8(R5 + 2);                /* ldrb r0, [r5,#2] */

    /* push {lr}: the handler observes the lowered SP */
    SP -= 4;
    ST32(SP, LR);

    R0 = phase;
    R1 = LD32(A_off_8009470 + phase * 4);   /* handler address | 1 */
    if (phase == 0)
        GUEST_CALL(battle_waitLinkSync_8009478);
    else
        GUEST_CALL(battle_updateChipSelectInit_80094B6);

    SP += 4;                                /* pop {pc}: return through LR */
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
        ST16(state + oBattleState_CountdownTimer, 0);
        ST8(state + 3, 4);
        R0 = 4;
        SET_FLAGS_NZ(4);
        RETURN_TO_CALLER();
    }

    R0 = 0x1F2F3F4F;
    GUEST_CALL(battle_doBothTrackedIdsMatch_800B46C);

    if (R0 != 0) {
        /* Both sides agree: shut down the tracker and signal readiness. */
        GUEST_CALL(battle_resetLinkSyncFlags_800B460);
        GUEST_CALL(sub_800B444);
        ST16(state + 2, 4);
        R0 = 4;
        SET_FLAGS_NZ(4);
        RETURN_TO_CALLER();
    }

    /* Still waiting: count frames; r0 stays equal to the incremented
     * timer. After 180 frames, force progress by switching the battle
     * state to its next handler (state index lives at offset 0). */
    u32 timer = LD16(state + oBattleState_CountdownTimer) + 1;
    ST16(state + oBattleState_CountdownTimer, timer);
    R0 = timer;
    SET_FLAGS_CMP(timer, 0xB4);
    if ((s32)timer > 0xB4) {
        R0 = 0xC;
        ST32(state + 0, 0xC);
        SET_FLAGS_NZ(0xC);
    }
    RETURN_TO_CALLER();
}

/* ── fifo_pushRecord16_8009FF8 ── */
/* fifo_pushRecord16_8009FF8(r0..r3 = four words of a record):
 * Append one 16-byte record to the FIFO at dword_3002180. The write slot is
 * selected by the counter word at byte_3000EA8, which is then incremented
 * (truncated to 28 bits by the original's lsl/lsr pair, so only the low
 * 28 bits ever wrap/grow).
 * r4/r6/r7 are saved and restored by the original, so they are preserved.
 * The flags are those of `adds r4, #1` on the new count (callers test it). */
Next fifo_pushRecord16_8009FF8(void) {
    u32 count = LD32(A_byte_3000EA8);
    u32 slot = A_dword_3002180 + (count << 4);

    ST32(slot + 0x00, R0);
    ST32(slot + 0x04, R1);
    ST32(slot + 0x08, R2);
    ST32(slot + 0x0C, R3);

    /* lsl r4,#4 / lsr r4,#4: keep only the low 28 bits of the counter */
    count = (count << 4) >> 4;
    SET_FLAGS_ADD(count, 1);
    count += 1;
    ST32(A_byte_3000EA8, count);

    RETURN_TO_CALLER();
}

/* ── fifo_resetWriteCounter_800A01C ── */
/* fifo_resetWriteCounter_800A01C: clear the flag/state word at byte_3000EA8.
 * Returns r0 = 0, r1 = &byte_3000EA8, with Z set (result of "mov r0, #0"). */
Next fifo_resetWriteCounter_800A01C(void) {
    R0 = 0;
    R1 = A_byte_3000EA8;
    ST32(R1, R0);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── PauseBattle ── */
/* PauseBattle (0800A028): set the battle-paused flag on the current game state.
 * r10 = toolkit; returns r0 = 1, r1 = game state pointer, NZ from r0 = 1. */
Next PauseBattle(void) {
    R1 = LD32(R10 + oToolkit_GameStatePtr);
    R0 = 1;
    SET_FLAGS_NZ(R0);
    ST8(R1 + oGameState_BattlePaused, R0);
    RETURN_TO_CALLER();
}

/* ── UnpauseBattle ── */
/* UnpauseBattle: clear the "battle paused" flag of the current game state. */
Next UnpauseBattle(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    ST8(gameState + oGameState_BattlePaused, 0);
    RETURN_TO_CALLER();
}

/* ── battle_isPaused ── */
/* battle_isPaused (0x0800A03C):
 * Read the battle-paused flag from the current game state.
 * r10 = toolkit pointer. Returns r0 = oGameState_BattlePaused byte and
 * leaves Z set iff the game is paused (r0 == 0); r1 = game state pointer. */
Next battle_isPaused(void) {
    u32 gameState = LD32(R10 + oToolkit_GameStatePtr);
    u32 paused = LD8(gameState + oGameState_BattlePaused);

    R0 = paused;
    R1 = gameState;
    SET_FLAGS_NZ(paused);

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

/* ── battle_isTimeStopPauseOrBattleFlags0x20_800a0a4 ── */
/* battle_isTimeStopPauseOrBattleFlags0x20_800a0a4:
 * Predicate consulted all over the battle engine to decide whether battle
 * time is currently standing still. It yields 1 when
 *   - battle_isTimeStop() reports an active time-stop, or
 *   - battle_isPaused() reports the game is NOT paused, or
 *   - battle_getFlags() has bit 0x20 set,
 * and 0 only when none of those hold.
 * r1 is left as the last callee left it on the early exits, and is 0x20 on
 * the battle_getFlags() path; callers rely on this. */
Next battle_isTimeStopPauseOrBattleFlags0x20_800a0a4(void) {
    GUEST_CALL(battle_isTimeStop); /* answers in Z; the original branches on it */
    if (!ZERO_FLAG()) {
        R0 = 1;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_isPaused);
    if (R0 == 0) {
        R0 = 1;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_getFlags);
    R1 = 0x20;
    R0 = (R0 & 0x20) != 0;
    RETURN_TO_CALLER();
}

/* ── clearBattleCounters_800A0C6 ── */
/* clearBattleCounters_800A0C6: clear the two 8-byte counter pairs at dword_2036820. */
Next clearBattleCounters_800A0C6(void) {
    ST32(A_dword_2036820, 0);
    ST32(A_dword_2036820 + 4, 0);
    ST32(A_dword_2036820 + 8, 0);
    ST32(A_dword_2036820 + 0xC, 0);
    RETURN_TO_CALLER();
}

/* ── input_recordStateChangeForEntry_800A0D6 ── */
/* input_recordStateChangeForEntry_800A0D6(r0 = entry index, r1 = new value):
 * Updates one 8-byte entry of the dword_2036820 array (entry size 8, so the
 * index is scaled by 3):
 *   +2: previous value, replaced by r1
 *   +4: bits that changed 0 -> 1 (newly set)
 *   +6: bits that changed 1 -> 0 (newly cleared)
 * Leaves r2 = newly cleared bits, r3 = entry address, and c/v from the
 * address computation (ADDS base + index*8). */
Next input_recordStateChangeForEntry_800A0D6(void) {
    u32 entry = A_dword_2036820 + (R0 << 3);
    SET_FLAGS_ADD(A_dword_2036820, R0 << 3);
    R3 = entry;

    u32 prev = LD16(entry + 2);
    ST16(entry + 2, R1);            /* store the new value */
    ST16(entry + 4, ~prev & R1);    /* bits newly set */
    prev &= ~R1;
    ST16(entry + 6, prev);          /* bits newly cleared */
    R2 = prev;

    RETURN_TO_CALLER();
}

/* ── battle_DecrementAllianceCounterSlot4_800A104 ── */
/* battle_DecrementAllianceCounterSlot4_800A104(r5 = battle object, r10 = toolkit):
 * Count down one of the alliance counters in the battle state (the slot
 * selected by this object's alliance + 4), but only while the AI data's
 * byte at offset oAIData_DisableCounterDecrement is zero. The decremented value is left
 * in r2; if the early-out is taken, r2 is left untouched. */
Next battle_DecrementAllianceCounterSlot4_800A104(void) {
    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);

    if (LD8(aiData + 2 /* oAIData_DisableCounterDecrement */) == 0) {
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        u32 slot = LD8(R5 + oBattleObject_Alliance) + 4;

        u32 count = LD8(battleState + slot) - 1;
        ST8(battleState + slot, count);

        R2 = count;
    }

    RETURN_TO_CALLER();
}

/* ── relatedToIsBattleOver_800A11C ── */
/* relatedToIsBattleOver_800A11C(r5 = battle object, r10 = toolkit):
 * An actor has finished (or is leaving) the battle: decrement its alliance's
 * member-count byte in the battle state, then remove it from the list of
 * alive actors that still get their turn. */
Next relatedToIsBattleOver_800A11C(void) {
    u32 *battleState = (u32 *)LD32(R10 + oToolkit_BattleStatePtr);
    u32 countSlot = (u32)LD8(R5 + oBattleObject_Alliance) + oBattleState_AliveAllianceCount;

    /* One fewer member of this actor's alliance. */
    ST8((u32)battleState + countSlot, (u8)LD8((u32)battleState + countSlot) - 1);

    /* Drop this actor from every alive-actor slot it occupies. */
    u32 *slot = (u32 *)((u32)battleState + oBattleState_AliveBattleActors);
    u32 i;
    for (i = 0; i < 8; i++) {
        if (*slot == R5)
            *slot = 0;
        slot++;
    }

    R2 = i;                 /* loop counter ends at 8 */
    SET_FLAGS_CMP(i, 8);
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
    u8 unk0d = LD8(battle + oBattleState_PlayerSide);
    u8 unk04 = LD8(battle + 4); /* oBattleState_AliveActorTurnIndex */

    R2 = unk0d;
    R1 = unk04;

    if (unk04 == 0) {
        R0 = (unk0d != 0) ? 1 : 2;
        SET_FLAGS_NZ(unk0d);
    } else {
        u8 unk05 = LD8(battle + 5); /* oBattleState_OpponentAliveCount */
        R1 = unk05;
        if (unk05 == 0) {
            R0 = (unk0d != 0) ? 2 : 1;
            SET_FLAGS_NZ(unk0d);
        } else {
            R1 = LD8(battle + oBattleState_BattleOverConditionFlag);
            R0 = (R1 != 0) ? 7 : 0;
            SET_FLAGS_NZ(R1);
        }
    }
    RETURN_TO_CALLER();
}

/* ── battle_isBattleOver ── */
/* battle_isBattleOver: check whether the current battle should end.
 * Reads the battle state through the toolkit pointer in r10.
 * Returns r0 = 0 only when all end-of-battle conditions hold:
 * the two state flags (oBattleState_AliveAllianceCount/13) must be set and
 * oBattleState_BattleOverConditionFlag must be clear; otherwise r0 = 1.
 * r3 = the battle state pointer; r1 = the last flag byte examined;
 * flags reflect the last test. */
Next battle_isBattleOver(void) {
    R0 = 1;

    R3 = LD32(R10 + oToolkit_BattleStatePtr);

    u32 flag12 = LD8(R3 + oBattleState_AliveAllianceCount);
    R1 = flag12;
    SET_FLAGS_NZ(flag12);
    if (flag12 == 0)
        RETURN_TO_CALLER();

    u32 flag13 = LD8(R3 + oBattleState_BattleOverSecondaryFlag);
    R1 = flag13;
    SET_FLAGS_NZ(flag13);
    if (flag13 == 0)
        RETURN_TO_CALLER();

    u32 flag0b = LD8(R3 + oBattleState_BattleOverConditionFlag);
    R1 = flag0b;
    SET_FLAGS_NZ(flag0b);
    if (flag0b != 0)
        RETURN_TO_CALLER();

    /* All conditions met: battle is over. */
    R0 = 0;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_checkNaviFreeze_800A1D0 ── */
/* register form: calls battle_isTimeStop outside this file, which has no typed C interface yet */
static bool time_is_stopped(u32 *r1_out) {
    GUEST_CALL(battle_isTimeStop);
    *r1_out = R1;                /* callee modifies r1; caller never overwrites before returning */
    return !ZERO_FLAG();         /* bne taken when Z=0 → time is stopped */
}

/* register form: calls battle_isBattleOver outside this file, which has no typed C interface yet */
static bool battle_is_over(u32 *r1_out) {
    GUEST_CALL(battle_isBattleOver);
    *r1_out = R1;
    return R0 != 0;             /* tst r0, r0; bne → non-zero means the battle ended */
}

/* register form: calls GetBattleNaviStatsByte outside this file, which has no typed C interface yet */
static u32 get_navi_stat_byte(u32 navi, u32 offset) {
    R0 = navi;
    R1 = offset;
    GUEST_CALL(GetBattleNaviStatsByte);
    return R0;
}

/* register form: calls battle_getFlags outside this file, which has no typed C interface yet */
static u32 get_battle_flags(void) {
    GUEST_CALL(battle_getFlags);
    return R0;
}

/* Freeze/slowdown status values in navi stat byte 0x2c (offset from battle navi table). */
#define STAT_FREEZE_1   0x17
#define STAT_FREEZE_2   0x18
#define STAT_BYTE       0x2C

/* Battle flag bits queried when a freeze status is active. */
#define FLAG_FREEZE     2
#define FLAG_SLOWDOWN   0x10

struct FreezeResult {
    u32 active;   /* 1 = freeze/slowdown condition is in effect, 0 = not */
    u32 mask;     /* r1: flag bit used, or callee-passthrough value on early exits */
};

/* Determine whether the battle navi freeze/slowdown condition is active.
 * Checks the player's and enemy's status byte (0x17 or 0x18 = frozen/stopped),
 * then verifies the corresponding battle flag bit.
 * r1_in is passed through on early-exit paths (callee overwrites it in practice). */
static struct FreezeResult check_navi_freeze(u32 r1_in) {
    u32 r1 = r1_in;

    /* If time is stopped, the freeze check is irrelevant – bail out. */
    if (time_is_stopped(&r1))
        return (struct FreezeResult){ 0, r1 };

    /* If the battle already ended, same early exit. */
    if (battle_is_over(&r1))
        return (struct FreezeResult){ 0, r1 };

    /* Player navi (index 0) must have freeze status 0x17 or 0x18,
     * or else check enemy navi (index 1) for the same values.
     * Neither matching → skip the bit-2 check and go straight to bit-0x10 check. */
    u32 pstat = get_navi_stat_byte(0, STAT_BYTE);
    bool any_freeze = (pstat == STAT_FREEZE_1 || pstat == STAT_FREEZE_2);

    if (!any_freeze) {
        u32 estat = get_navi_stat_byte(1, STAT_BYTE);
        any_freeze = (estat == STAT_FREEZE_1 || estat == STAT_FREEZE_2);
    }

    /* Path A: at least one navi has freeze status – check FLAG_FREEZE (bit 1). */
    if (any_freeze) {
        r1 = FLAG_FREEZE;
        u32 flags = get_battle_flags();
        if (flags & FLAG_FREEZE)
            return (struct FreezeResult){ 1, FLAG_FREEZE };
        /* Bit not set – fall through to the slowdown check. */
    }

    /* Path B: check FLAG_SLOWDOWN (bit 4). */
    r1 = FLAG_SLOWDOWN;
    u32 flags = get_battle_flags();
    if (flags & FLAG_SLOWDOWN)
        return (struct FreezeResult){ 1, FLAG_SLOWDOWN };

    return (struct FreezeResult){ 0, FLAG_SLOWDOWN };
}

/* battle_checkNaviFreeze_800A1D0(r6, r10) -> r0 (freeze active), r1, flags nz */
Next battle_checkNaviFreeze_800A1D0(void) {
    struct FreezeResult res = check_navi_freeze(R1);
    R0 = res.active;
    R1 = res.mask;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_setFlags ── */
/* battle_setFlags(r0 = flag bits):
 * Sets the given bits in the battle state's flags halfword
 * (BattleState + oBattleState_BattleStateFlags), where the battle state is
 * taken from the toolkit (r10).
 * Leaves r1 = battle state pointer, r2 = new flags value. */
Next battle_setFlags(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    u32 flags = LD16(battle + oBattleState_BattleStateFlags) | R0;
    ST16(battle + oBattleState_BattleStateFlags, flags);

    R1 = battle;
    R2 = flags;
    RETURN_TO_CALLER();
}

/* ── battle_clearFlags ── */
/* battle_clearFlags(r0 = flag bits to clear):
 * Clears the given bits of the battle state's halfword flag field.
 * r10 = toolkit pointer; r1 = battle state pointer, r2 = new flags. */
Next battle_clearFlags(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 flags = LD16(battleState + oBattleState_BattleStateFlags) & ~R0;

    ST16(battleState + oBattleState_BattleStateFlags, flags);
    R1 = battleState;
    R2 = flags;
    RETURN_TO_CALLER();
}

/* ── battle_initTypeAndFieldFromSettings_800A2F8 ── */
/* battle_initTypeAndFieldFromSettings_800A2F8: initialize battle state fields at battle start.
 *
 * - Stores (navi stats halfword at 0x3E) / 100 into the navi stats byte at
 *   0x2B (via the BIOS divide call).
 * - Copies the battle type and battlefield from the current battle settings
 *   into the battle state, so the rest of the battle code can read them
 *   from one place.
 *
 * Outputs: r3 = battle state pointer; r2 and r4 are untouched. */
Next battle_initTypeAndFieldFromSettings_800A2F8(void) {
    u32 naviStats = LD32(R10 + oToolkit_NaviStatsPtr);

    R0 = LD16(naviStats + 0x3E);
    R1 = 100;
    swi_call(6); /* BIOS Div: R0 = R0 / R1, R1 = remainder */
    ST8(naviStats + 0x2B, R0);

    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u32 settings = LD32(battle + oBattleState_BattleSettings);

    ST8(battle + 6, LD8(settings + 0)); /* battlefield */
    ST8(battle + oBattleState_BattleType, LD8(settings + 3));

    R3 = battle; /* the original leaves the battle state pointer in r3 */
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

/* ── battle_getNaviStatsByte_800A540 ── */
/* battle_getNaviStatsByte_800A540(r0 = arg0, r1 = arg1): fetch one byte of the currently
 * selected navi's stats. If the battle-effect flag 0x200000 is set, read it
 * from the active navi's stats block (0x203CCE0, keeping r0 as passed in);
 * otherwise read it from the current PET's navi stats.
 * Returns the byte in r0; r4 and r6 are preserved. */
Next battle_getNaviStatsByte_800A540(void) {
    u32 arg0 = R0;
    u32 arg1 = R1;

    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 0x200000) {
        /* effect active: arg0 still holds the battle-effects result */
        R1 = arg1;
        GUEST_CALL(GetNaviStats203CCE0Byte);
    } else {
        R0 = arg0;
        R1 = arg1;
        GUEST_CALL(GetCurPETNaviStatsByte);
    }
    RETURN_TO_CALLER();
}

/* ── shuffleFolder_800A570 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r0; } call_getChip8021DA8_result;
/* register form: generated wrapper: getChip8021DA8 has no typed C interface yet */
static inline call_getChip8021DA8_result call_getChip8021DA8(u32 r0) {
    R0 = r0;
    GUEST_CALL(getChip8021DA8);
    return (call_getChip8021DA8_result){ R0 };
}

typedef struct { u32 r2, r5, r6, r7; } call_ShuffleHwordList_SecondaryRNG_result;
/* register form: generated wrapper: ShuffleHwordList_SecondaryRNG has no typed C interface yet */
static inline call_ShuffleHwordList_SecondaryRNG_result call_ShuffleHwordList_SecondaryRNG(u32 r2) {
    R2 = r2;
    GUEST_CALL(ShuffleHwordList_SecondaryRNG);
    return (call_ShuffleHwordList_SecondaryRNG_result){ R2, R5, R6, R7 };
}

/* register form: generated wrapper: GetBattleMode has no typed C interface yet */
static inline u32 call_GetBattleMode(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

typedef struct { u32 r0, r7; } call_GetPositiveSignedRNGSecondary_result;
/* register form: generated wrapper: GetPositiveSignedRNGSecondary has no typed C interface yet */
static inline call_GetPositiveSignedRNGSecondary_result call_GetPositiveSignedRNGSecondary(void) {
    GUEST_CALL(GetPositiveSignedRNGSecondary);
    return (call_GetPositiveSignedRNGSecondary_result){ R0, R7 };
}

/* register form: generated wrapper: sub_800A672 has no typed C interface yet */
static inline u32 call_sub_800A672(u32 r0, u32 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(sub_800A672);
    return R0;
}

typedef struct { u32 r1, r2, r3; bool c; } call_CopyHalfwords_result;
/* register form: generated wrapper: CopyHalfwords has no typed C interface yet */
static inline call_CopyHalfwords_result call_CopyHalfwords(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyHalfwords);
    return (call_CopyHalfwords_result){ R1, R2, R3, CARRY_FLAG() };
}

/* Shuffle a folder of 30 battle chips: separate into gigas and non-gigas, shuffle each
 * temporary list, insert gigas at random positions into the non-giga list, optionally
 * swap a random chip pair into the last two slots, then write the result back. */

#define FOLDER_SIZE        30
#define GIGA_ELEM_TYPE     2
#define CHIP_ELEM_TYPE_OFF 7
#define CHIP_ID_MASK       0x1FF

#define NON_GIGA_BASE      A_dword_2033000
#define GIGA_BASE          A_word_2033040

#define SKIP_PIN_SHIFT     2       /* bytes to skip when "reg" flag is set */
#define SKIP_TAG_COUNT     2       /* additional entries skipped for "tag" flag */

#define NORMAL_MOD_OFF     12      /* divisor = count - 12 for non-crossover */
#define NORMAL_INS_OFF     10      /* insertion base for non-crossover */
#define CROSSOVER_MOD      11      /* divisor for crossover */
#define CROSSOVER_INS      8       /* insertion base for crossover */
#define SWAP_MODULUS       19      /* divisor for the random pair selection */

#define SLOT_LAST_1_OFFSET   0x38  /* offset of slot 28 in the folder (halfword 28) */
#define SLOT_LAST_2_OFFSET   0x3A  /* offset of slot 29 (halfword 29) */
#define BATTLESTATE_IDX_OFF  0x45  /* byte storing the chosen pair index */

#define COPY_HALFWORDS_CNT 0x3C    /* 60 halfwords = 120 bytes */

/* register form: DivARM (svc 6) reads dividend/divisor from R0/R1 and leaves
   the remainder in R1, which swi_call does not expose as a return value */
static u32 divarm_remainder(u32 dividend, u32 divisor) {
    R0 = dividend;
    R1 = divisor;
    swi_call(6);
    return R1;
}

/* register form: ShuffleHwordList_SecondaryRNG reads list pointer from R0 and count
   from R1, which the generated wrapper (r2-only) does not set */
static void shuffle_hword_list(u32 list_ptr, u32 count) {
    R0 = list_ptr;
    R1 = count;
    call_ShuffleHwordList_SecondaryRNG(count);
}

static void shuffle_folder(u32 folder_ptr, u32 reg_flag, u32 tag_flag, u32 toolkit) {
    u32 non_giga_count = 0;
    u32 giga_count = 0;

    /* Scan all 30 folder entries, split into non-giga and giga temporary arrays */
    for (u32 i = 0; i < FOLDER_SIZE; i++) {
        u16 raw = (u16)LD16(folder_ptr + i * 2);
        u32 chip_id = raw & CHIP_ID_MASK;
        u32 chip_data = call_getChip8021DA8(chip_id).r0;
        u32 elem_type = LD8(chip_data + CHIP_ELEM_TYPE_OFF);

        if (elem_type == GIGA_ELEM_TYPE) {
            ST16(GIGA_BASE + giga_count * 2, raw);
            giga_count++;
        } else {
            ST16(NON_GIGA_BASE + non_giga_count * 2, raw);
            non_giga_count++;
        }
    }

    /* Shuffle the non-giga list, honoring the reg and tag skip flags */
    if (non_giga_count != 0) {
        u32 list_start = NON_GIGA_BASE;
        u32 shuffle_len = non_giga_count;

        if (reg_flag != 0) {
            list_start += SKIP_PIN_SHIFT;
            shuffle_len -= 1;
        }
        if (tag_flag != 0) {
            shuffle_len -= SKIP_TAG_COUNT;
        }

        shuffle_hword_list(list_start, shuffle_len);
    }

    /* Shuffle the giga list, then insert each giga at a random position */
    if (giga_count != 0) {
        shuffle_hword_list(GIGA_BASE, giga_count);

        u32 battle_mode = call_GetBattleMode();
        bool is_crossover = (battle_mode == 1);

        for (u32 idx = 0; idx < giga_count; idx++) {
            u32 rng_val = call_GetPositiveSignedRNGSecondary().r0;
            u32 insert_pos;

            if (is_crossover) {
                u32 remainder = divarm_remainder(rng_val, CROSSOVER_MOD);
                insert_pos = CROSSOVER_INS + remainder;
            } else {
                u32 divisor = non_giga_count - NORMAL_MOD_OFF;
                u32 remainder = divarm_remainder(rng_val, divisor);
                insert_pos = NORMAL_INS_OFF + remainder;
            }

            u16 giga_chip = (u16)LD16(GIGA_BASE + idx * 2);
            non_giga_count = call_sub_800A672(NON_GIGA_BASE, non_giga_count,
                                               giga_chip, insert_pos);
        }
    }

    /* If tag_flag is set, swap a random consecutive pair into the last two slots */
    if (tag_flag != 0) {
        u32 rng_val = call_GetPositiveSignedRNGSecondary().r0;
        u32 remainder = divarm_remainder(rng_val, SWAP_MODULUS);
        u32 pair_index = remainder + 1;

        /* Record the chosen index in the battle state */
        u32 battle_state = LD32(toolkit + oToolkit_BattleStatePtr);
        ST8(battle_state + BATTLESTATE_IDX_OFF, pair_index);

        u32 byte_off = pair_index * 2;

        /* Swap first slot of the pair with slot 28 */
        u16 tmp = (u16)LD16(NON_GIGA_BASE + SLOT_LAST_1_OFFSET);
        u16 sel = (u16)LD16(NON_GIGA_BASE + byte_off);
        ST16(NON_GIGA_BASE + SLOT_LAST_1_OFFSET, sel);
        ST16(NON_GIGA_BASE + byte_off, tmp);

        /* Swap second slot of the pair with slot 29 */
        byte_off += 2;
        u16 tmp2 = (u16)LD16(NON_GIGA_BASE + SLOT_LAST_2_OFFSET);
        u16 sel2 = (u16)LD16(NON_GIGA_BASE + byte_off);
        ST16(NON_GIGA_BASE + SLOT_LAST_2_OFFSET, sel2);
        ST16(NON_GIGA_BASE + byte_off, tmp2);
    }

    /* Write the final folder back to the caller's original buffer */
    call_CopyHalfwords(NON_GIGA_BASE, folder_ptr, COPY_HALFWORDS_CNT);
}

/* shuffleFolder_800A570: r0=folder_ptr, r1=reg, r2=tag, r10=toolkit
 * Preserved: r4-r7. Clobbers: r0-r3, lr, flags. No output registers. */
Next shuffleFolder_800A570(void) {
    shuffle_folder(R0, R1, R2, R10);
    RETURN_TO_CALLER();
}

/* ── battle_getStateUnk40_800A704 ── */
/* battle_getStateUnk40_800A704: return the value stored at oBattleState_ChipUseFrameCounter of the
 * current battle state (accessed through the r10 toolkit pointer). */
Next battle_getStateUnk40_800A704(void) {
    R0 = LD32(LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_ChipUseFrameCounter);
    RETURN_TO_CALLER();
}

/* ── getBattleSettingsFromList0 ── */
/* getBattleSettingsFromList0(r0 = index): return a pointer to entry `index`
 * (16 bytes each) of battleSettingsList0. Clobbers r1 like the original. */
Next getBattleSettingsFromList0(void) {
    R1 = (u32)R0 * 0x10;
    R0 = A_battleSettingsList0 + R1;
    RETURN_TO_CALLER();
}

/* ── isSameSubsystem_800A732 ── */
/* isSameSubsystem_800A732: () -> !zf
 * Returns nonzero (Z clear) when the currently loaded subsystem counts as
 * "the same": either no subsystem is currently in use, or the subsystem
 * state byte at eStruct203F7D8+1 still has bit 1 set. On the path where the
 * subsystem is in use, r3 holds the struct address, r1 the state byte and
 * r2 the mask 2; otherwise those registers keep whatever
 * IsCurSubsystemInUse left in them. r4 is restored unchanged.
 *
 * Flag note: the original's final "mov r0, r4" is really an
 * "adds r0, r4, #0", which clears C and V; the following "tst r0, r0"
 * only refreshes N and Z. */
Next isSameSubsystem_800A732(void) {
    u32 same = 1; /* r4 = TRUE */

    GUEST_CALL(IsCurSubsystemInUse); /* answers in Z; the original branches on it */

    if (!ZERO_FLAG()) {
        /* Subsystem is in use: "same" only while its state bit 1 is set. */
        R3 = A_eStruct203F7D8;
        R1 = LD8(A_eStruct203F7D8 + 1);
        R2 = 2;
        if (!(R1 & R2))
            same = 0; /* FALSE */
    }

    R0 = same;
    SET_FLAGS_ADD(same, 0); /* adds r0, r4, #0: N/Z from result, C = V = 0 */
    RETURN_TO_CALLER();
}

/* ── battle_canAllianceAct_800A772 ── */
/* battle_canAllianceAct_800A772(r0 = alliance selector (0 = player, else opponent), r5 = battle object):
 * Decide whether this object is allowed to act.
 * Loads the object's AI data and returns 0 immediately if its chip lockout
 * timer is still running. Otherwise checks the battle state's Unk_5c flags
 * (via battle_getStateUnk5c_800139A) for the calling side's bit:
 * bit 2 (0x04) for the player alliance, bit 3 (0x08) for the opponent
 * alliance. Returns r0 = 1 if the bit is set, 0 if not.
 * Side effects matching the original: r1 ends as the tested bit (or the
 * lockout timer in the early-out path) and r2 ends as the caller's r0. */
Next battle_canAllianceAct_800A772(void) {
    u32 callerR0 = R0;

    u32 aiData = LD32(R5 + oBattleObject_AIDataPtr);
    u32 lockoutTimer = LD8(aiData + oAIData_ChipLockoutTimer);
    if (lockoutTimer != 0) {
        R0 = 0;
        R1 = lockoutTimer;
        RETURN_TO_CALLER();
    }

    GUEST_CALL(battle_getStateUnk5c_800139A);
    u32 stateUnk5c = R0;

    /* Which alliance's flag to test is picked by the caller's r0. */
    u32 allianceFlag = (callerR0 == 0) ? 4 : 8;

    R1 = allianceFlag;
    R2 = callerR0;
    R0 = (stateUnk5c & allianceFlag) ? 1 : 0;
    RETURN_TO_CALLER();
}

/* ── sfx_playSfx0C_800A79C ── */
/* sfx_playSfx0C_800A79C: play sound effect #0x0C (battle_SetStateUnk5c_80013A2 is the sfx player). */
Next sfx_playSfx0C_800A79C(void) {
    R0 = 0xC;
    GUEST_CALL(battle_SetStateUnk5c_80013A2);
    RETURN_TO_CALLER();
}

/* ── battle_countOpponentActorsWithNameIdInRange_800A7A6 ── */
/* battle_countOpponentActorsWithNameIdInRange_800A7A6(r1 = lowest name ID, r2 = highest name ID):
 * Count the opponent-side battle actors whose name ID falls within
 * [r1, r2] (inclusive). The opponent actor list holds four slots
 * (0x10 bytes), each a pointer that may be NULL.
 * Returns r0 = number of matching actors, r3 = 0x10 (loop end value),
 * with the flags left by the final MOV r0, r4 (ADDS r0, r4, #0). */
Next battle_countOpponentActorsWithNameIdInRange_800A7A6(void) {
    u32 minNameID = R1;
    u32 maxNameID = R2;
    u32 actorList = LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_OpponentActors;
    u32 count = 0;

    for (u32 slot = 0; slot < 0x10; slot += 4) {
        u32 actor = LD32(actorList + slot);
        if (actor != 0) {
            u32 nameID = LD16(actor + oBattleObject_NameID);
            if ((s32)nameID >= (s32)minNameID && (s32)nameID <= (s32)maxNameID)
                count++;
        }
    }

    R0 = count;
    R3 = 0x10;
    SET_FLAGS_ADD(R0, 0); /* MOV r0, r4 == ADDS r0, r4, #0 */
    RETURN_TO_CALLER();
}

/* ── battle_getStateByte0d_800A7E2 ── */
/* battle_getStateByte0d_800A7E2: return the byte at oBattleState_PlayerSide of the current battle state
 * (r10 = toolkit). */
Next battle_getStateByte0d_800A7E2(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    R0 = LD8(battleState + oBattleState_PlayerSide);
    RETURN_TO_CALLER();
}

/* ── encryption_decryptPackTable30_800a7ea ── */
/* encryption_decryptPackTable30_800a7ea(r0 = address of a table of 30 packed halfwords):
 * Decrypt every entry of the table in place by feeding each halfword to
 * encryption_applyPack_8006e70. Each halfword is split into its low 9 bits
 * (passed in r0) and its high 7 bits (passed in r1), with r2 = 0.
 * The table pointer register (r0 on entry) and the caller's r4/r6 are
 * preserved; r0/r1/r2 are left holding the final split values, and the
 * flags come from the last loop-counter compare. */
Next encryption_decryptPackTable30_800a7ea(void) {
    u32 tableAddr = R0;
    u32 savedR4 = R4;
    u32 savedR6 = R6;
    u32 lastPayload = 0;
    u32 lastKey = 0;
    u32 entry;

    for (entry = 0; entry < 30; entry++) {
        u32 packed = LD16(tableAddr);
        u32 payload = packed & 0x1FF; /* low 9 bits */
        u32 key = packed >> 9;        /* high 7 bits */

        R0 = payload;
        R1 = key;
        R2 = 0;
        R4 = tableAddr; /* loop registers stay live across the call */
        R6 = entry;

        /* The original spills the payload around the call; keep the guest
           stack layout identical in case the callee looks at it. */
        SP -= 4;
        ST32(SP, payload);
        GUEST_CALL(encryption_applyPack_8006e70);
        SP += 4;

        lastPayload = payload;
        lastKey = key;
        tableAddr += 2;
    }

    /* The loop leaves the last split values in r0/r1 and r2 = 0, and the
       pop {r4,r6} restores the caller's registers before returning. */
    R0 = lastPayload;
    R1 = lastKey;
    R2 = 0;
    R4 = savedR4;
    R6 = savedR6;
    SET_FLAGS_CMP(entry, 30); /* final iteration compared counter (0x1E) vs 0x1E */
    RETURN_TO_CALLER();
}

/* ── battle_getStateByteNibbles_800A832 ── */
/* battle_getStateByteNibbles_800A832: read the battle state's Unk_1f byte and split it into nibbles.
 * Outputs: r0 = low nibble, r1 = high nibble, r2 = 0xF;
 * nz from the low nibble, c from bit 3 of the original byte (the LSR carry-out). */
Next battle_getStateByteNibbles_800A832(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 stateByte = LD8(battleState + oBattleState_BattleResult);

    R1 = stateByte >> 4;
    R0 = stateByte & 0xF;
    R2 = 0xF;

    SET_FLAGS_NZ(R0);
    cpu.c = (stateByte >> 3) & 1;

    RETURN_TO_CALLER();
}

/* ── setTwoStructs_800A840 ── */
/* setTwoStructs_800A840(r0 = battle result byte, r10 = toolkit):
 * Writes the byte into the current battle state, then updates the battle
 * result via eStruct200A008_setBattleResult. Returns that function's r1. */
Next setTwoStructs_800A840(void) {
    u32 resultByte = R0;

    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    ST8(battleState + oBattleState_BattleResult, resultByte);

    /* eStruct200A008_setBattleResult takes the same byte in r0 and reads
     * the toolkit from r10; both are already in place. Its result in r1
     * is passed through to the caller. */
    GUEST_CALL(eStruct200A008_setBattleResult);

    RETURN_TO_CALLER();
}

/* ── sub_800A86E ── */
/* sub_800A86E: scan the current battle state's 8 "alive battle actor" slots.
 * Every slot is read: r3 ends up holding the raw contents of the last slot
 * (0 if empty), r2 the AI-data pointer of the last occupied slot (or its
 * incoming value if none was occupied), and r0/r1 the unk_0e byte / HP of
 * that actor (HP is skipped when unk_0e is 0xFF). r4 and r7 are preserved;
 * the flags left behind are those of the final loop-counter compare (8 vs 8). */
Next sub_800A86E(void) {
    u32 slotPtr = LD32(R10 + oToolkit_BattleStatePtr) + oBattleState_AliveBattleActors;
    u32 actor;
    u32 aiData = R2; /* r2 keeps its incoming value if no slot is occupied */

    for (u32 i = 0; i < 8; i++) {
        actor = LD32(slotPtr);
        if (actor != 0) {
            aiData = LD32(actor + oBattleObject_AIDataPtr);
            R0 = LD8(aiData + oAIData_AISubordinateSlot);
            if (R0 != 0xFF)
                R1 = LD16(actor + oBattleObject_HP);
        }
        slotPtr += 4;
    }

    R2 = aiData;
    R3 = actor;             /* contents of the last slot, occupied or not */
    SET_FLAGS_CMP(8, 8);    /* cmp r4, #8 with r4 == 8 at loop exit */
    RETURN_TO_CALLER();
}

/* ── battle_tickCustomScreenEffect_800A892 ── */
/* battle_tickCustomScreenEffect_800A892:
 * Custom battle effect tick: if the active battle effects include the "custom
 * style" effect (0x100) but not the one at bit 3, and battle_getStateByteNibbles_800A832 reports
 * state 2, run the effect's main handler battle_initFullSetup_8005DBE. */
Next battle_tickCustomScreenEffect_800A892(void) {
    u32 effects = 0;

    GUEST_CALL(GetBattleEffects);
    effects = R0;

    if ((effects & 0x100) != 0 && (effects & 8) == 0) {
        GUEST_CALL(battle_getStateByteNibbles_800A832);
        if (R0 == 2) {
            GUEST_CALL(battle_initFullSetup_8005DBE);
        }
    }

    RETURN_TO_CALLER();
}

/* ── battle_testFlag0x40_800A8F8 ── */
/* battle_testFlag0x40_800A8F8: test battle flag 0x40 and return the result.
 * r0 = 1 if the flag is set, 0 if not; nz/c/v reflect the final `mov r0, r4`
 * (encoded as adds r0, r4, #0, so C and V are cleared). */
Next battle_testFlag0x40_800A8F8(void) {
    u32 result;

    GUEST_CALL(TestBattleFlag_0x40); /* answers in Z; the original branches on it */

    result = ZERO_FLAG() ? 0 : 1;
    R0 = result;
    SET_FLAGS_ADD(result, 0);

    RETURN_TO_CALLER();
}

/* ── clearBattleStateCounter_800A908 ── */
/* Clear the 32-bit battle state counter at dword_2000B30 to 0 and return its address in r1.
 * r0 is set to 0 so callers can detect "just cleared" via the NZ flag. */
static u32 clear_battle_state_counter(void) {
    ST32(A_dword_2000B30, 0);
    return 0;
}

Next clearBattleStateCounter_800A908(void) {
    R0 = clear_battle_state_counter();
    R1 = A_dword_2000B30;
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── sub_800A954 ── */
/* sub_800A954: run initIdListStruct_800A964 over each of the two work buffers at
 * byte_20349C0 and byte_2034A10. r2/r3/r4 pass through untouched. */
Next sub_800A954(void) {
    R0 = A_byte_20349C0;
    GUEST_CALL(initIdListStruct_800A964);

    R0 = A_byte_2034A10;
    GUEST_CALL(initIdListStruct_800A964);

    RETURN_TO_CALLER();
}

/* ── initIdListStruct_800A964 ── */
/* initIdListStruct_800A964(r0 = struct pointer):
 * Initialize a record list structure: clear the whole 0xC8-byte block to zero,
 * then mark the 12-byte ID array at offset 2 as empty (0xFF). */
Next initIdListStruct_800A964(void) {
    u32 mem = R0;
    u32 savedR4 = R4;

    /* ZeroFillByWord(mem, 0x50 words = 0xC8 bytes) */
    R1 = 0x50;
    GUEST_CALL(ZeroFillByWord);

    /* ByteFill(mem + 2, 12, 0xFF): every entry id starts as "unused" */
    R0 = mem + 2;
    R1 = 0xC;
    R2 = 0xFF;
    GUEST_CALL(ByteFill);

    R4 = savedR4;
    RETURN_TO_CALLER();
}

/* ── battle_IsBattleEnding_800A97A ── */
/* battle_IsBattleEnding_800A97A: check whether the battle is ending.
 * Returns 1 when GetBattleEffects reports the bit-3 effect *and* the battle
 * state's phase counter (byte 7) has reached 15; otherwise returns 0.
 * Flags on exit match the original's last comparison on each path. */
Next battle_IsBattleEnding_800A97A(void) {
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    R1 = 8;
    if (effects & 8) {
        /* Effect bit is set: consult the battle state's phase counter. */
        u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
        R3 = battleState;
        u32 phase = LD8(battleState + 7);
        R1 = phase;

        SET_FLAGS_CMP(phase, 0xF);
        if ((s32)phase >= 0xF) {
            R0 = 1;
            RETURN_TO_CALLER();
        }
        R0 = 0;
        RETURN_TO_CALLER();
    }

    /* Effect bit clear: phase counter is never consulted. */
    SET_FLAGS_NZ(effects & 8);
    R0 = 0;
    RETURN_TO_CALLER();
}

/* ── LoadVirusNameIDsForUpToFourSlots_800A998 ── */
/* LoadVirusNameIDsForUpToFourSlots_800A998(r0 = name_id_arr: *u16[4]):
 * Collects the NameIDs of the living actors on one side of the battle
 * (the side selected by oBattleState_PlayerSide, flipped via 1^side) into
 * the caller's 4-slot u16 array, skipping empty actor slots.
 * Returns the number of entries written.
 * r4 is saved and restored by the original, so it stays unchanged. */
Next LoadVirusNameIDsForUpToFourSlots_800A998(void) {
    u32 out = R0;

    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    /* 1 ^ side: flip the actor list selector. */
    u32 side = 1 ^ LD8(battleState + oBattleState_PlayerSide);
    u32 actors = battleState + side * 0x10 + oBattleState_AliveBattleActors;

    u32 byteOffset = 0;
    for (u32 i = 0; i < 4; i++) {
        u32 actor = LD32(actors + i * 4);
        if (actor != 0) {
            ST16(out + byteOffset, LD16(actor + oBattleObject_NameID));
            byteOffset += 2;
        }
    }

    R0 = byteOffset >> 1; /* number of entries to process */
    RETURN_TO_CALLER();
}

/* ── setBattleStateUnk11Flag_800A9CA ── */
/* setBattleStateUnk11Flag_800A9CA(r0 = flag bits, r10 = toolkit):
 * set the given bits in the battle state's Unk_11 flags byte.
 * r3 = pointer to the battle state struct. */
Next setBattleStateUnk11Flag_800A9CA(void) {
    u32 flags = R0;
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    ST8(battle + oBattleState_StateFlags,
        LD8(battle + oBattleState_StateFlags) | flags);

    R3 = battle;
    RETURN_TO_CALLER();
}

/* ── clearBattleStateUnk11Flag_800A9D6 ── */
/* clearBattleStateUnk11Flag_800A9D6(r0 = bit mask, r10 = toolkit):
 * clear the requested bits of the current battle state's Unk_11 flag byte.
 * Leaves r3 pointing at the battle state. */
Next clearBattleStateUnk11Flag_800A9D6(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);
    u32 flags = LD8(battleState + oBattleState_StateFlags);

    flags &= ~R0;
    ST8(battleState + oBattleState_StateFlags, flags);

    R3 = battleState;
    RETURN_TO_CALLER();
}

/* ── battle_GetObjectByteAt14_800A9E2 ── */
/* battle_GetObjectByteAt14_800A9E2(r0 = battle object index, r10 = toolkit):
 * returns the byte at battleState + 0x14 + r0 (an object stat the caller
 * wants to inspect); r3 = battle state pointer, flags from the index add. */
Next battle_GetObjectByteAt14_800A9E2(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    R3 = battle;
    R0 += 0x14;
    SET_FLAGS_ADD(R0, 0x14);
    R0 = LD8(battle + R0);
    RETURN_TO_CALLER();
}

/* ── battle_networkInvert ── */
/* battle_networkInvert(r0 = value, r10 = toolkit):
 * XORs r0 with the byte at battleState->Unk_0d and returns the result in r0.
 * Also leaves the battle state pointer in r1, the toggled byte in r2, and the
 * NZ flags set from the result (the original's EOR is its last flag setter). */
Next battle_networkInvert(void) {
    u32 value = R0;

    R1 = LD32(R10 + oToolkit_BattleStatePtr);
    R2 = LD8(R1 + oBattleState_PlayerSide);
    R0 = value ^ R2;

    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}

/* ── battle_clearEnemyFadeinList ── */
/* battle_clearEnemyFadeinList:
 * Reset the enemy fade-in list used during battle setup. Asks getNextEntryAddress_800AAAE
 * (called with r0 = 0) for the list's base address, then clears the first
 * 0x80 bytes of it. Returns with r1 = 0x80 (the size that was cleared). */
Next battle_clearEnemyFadeinList(void) {
    R0 = 0;
    GUEST_CALL(getNextEntryAddress_800AAAE);   /* r0 = base address of the fade-in list */

    R1 = 0x80;                 /* size in bytes */
    GUEST_CALL(ZeroFillByWord); /* zero-fill r1 bytes starting at r0 */

    RETURN_TO_CALLER();
}

/* ── battleObject_isCurrentTurnBattleObject_800AA06 ── */
/* battleObject_isCurrentTurnBattleObject_800AA06(r0 = handle passed through to getNextEntryAddress_800AAAE, r5 = expected id):
 * Asks getNextEntryAddress_800AAAE (called with r0 unchanged) for a pointer, then checks
 * whether the id field at that pointer matches r5. Returns r0 = 1 on match,
 * 0 otherwise (also left in r2, with flags from the move). */
Next battleObject_isCurrentTurnBattleObject_800AA06(void) {
    GUEST_CALL(getNextEntryAddress_800AAAE);

    u32 match = 0;
    if (LD32(R0) == R5)
        match = 1;

    R2 = match;
    R0 = match;
    SET_FLAGS_ADD(match, 0);
    RETURN_TO_CALLER();
}

/* ── list_tryAddUniqueEntry_800AA1A ── */
/* list_tryAddUniqueEntry_800AA1A(r5 = value to register):
 * Walk the 8-word list (0x20 bytes) whose address getNextEntryAddress_800AAAE()
 * returns and register the value in the first free (0) slot.
 * Returns r0 = 1 if the value was stored, r0 = 0 if it was already in the list
 * or the list was full. r1 ends up holding the last list word that was read. */
Next list_tryAddUniqueEntry_800AA1A(void) {
    GUEST_CALL(getNextEntryAddress_800AAAE);
    u32 list = R0;
    u32 value = R5;

    u32 offset = 0;
    for (;;) {
        u32 entry = LD32(list + offset);
        R1 = entry;

        if (entry == value) {
            /* already present: nothing to do */
            SET_FLAGS_CMP(entry, value);
            R0 = 0;
            RETURN_TO_CALLER();
        }

        if (entry == 0) {
            /* free slot: claim it for this value */
            SET_FLAGS_CMP(entry, 0);
            ST32(list + offset, value);
            R0 = 1;
            RETURN_TO_CALLER();
        }

        offset += 4;
        SET_FLAGS_CMP(offset, 0x20);
        if (offset == 0x20) {
            /* reached the end without finding it: list is full */
            R0 = 0;
            RETURN_TO_CALLER();
        }
    }
}

/* ── compactEntryWords_800AA64 ── */
/* compactEntryWords_800AA64: compact the 7-word entry (0x1C bytes) returned by
 * getNextEntryAddress_800AAAE. Non-zero words are shifted to the front of the
 * entry and the remainder is zero-filled, so the entry has no holes.
 * Leaves R0 = 0, R1/R3 = one past the entry's last word, and flags from the
 * final CMP of the fill loop. The incoming R4 is preserved.
 * Inputs: R0 is passed through to getNextEntryAddress_800AAAE. */
Next compactEntryWords_800AA64(void) {
    u32 savedR4 = R4;

    GUEST_CALL(getNextEntryAddress_800AAAE);
    u32 entry = R0;

    u32 end = entry + 0x1C; /* the entry is 7 words long */
    u32 src = entry;
    u32 dst = entry;

    /* Move every non-zero word to the front of the entry. */
    while (src != end) {
        u32 word = LD32(src);
        if (word != 0) {
            ST32(dst, word);
            dst += 4;
        }
        src += 4;
    }

    /* Zero-fill the unused tail. */
    while (dst != end) {
        ST32(dst, 0);
        dst += 4;
    }

    R0 = 0;
    R1 = dst; /* == end */
    R3 = src; /* == end */
    R4 = savedR4;
    /* The original's last flag-setting instruction is `cmp r1, r4` with
     * r4 still holding the entry end (it is restored only afterwards). */
    SET_FLAGS_CMP(R1, R3);

    RETURN_TO_CALLER();
}

/* ── battle_countAliveNextEntries_800AA92 ── */
/* battle_countAliveNextEntries_800AA92: fetch a table pointer from getNextEntryAddress_800AAAE, then count how many
 * of its eight u32 entries are nonzero.
 * outputs: r0 = nonzero count, r1 = last word read, r2 = 0x20 (loop end),
 *          r3 = table pointer; c/v left as the final `cmp r2, #0x20` leaves
 *          them (c set, v clear). */
Next battle_countAliveNextEntries_800AA92(void) {
    GUEST_CALL(getNextEntryAddress_800AAAE);
    R3 = R0;

    u32 count = 0;
    for (u32 offset = 0; offset < 0x20; offset += 4) {
        R1 = LD32(R3 + offset);
        if (R1 != 0)
            count++;
    }

    R0 = count;
    R2 = 0x20;
    SET_FLAGS_CMP(R2, 0x20);
    RETURN_TO_CALLER();
}

/* ── getNextEntryAddress_800AAAE ── */
/* getNextEntryAddress_800AAAE(r0 = element index):
 * Computes the address of entry `r0` in the array at unk_2038170, whose
 * elements are 0x20 bytes each. Returns the address in r0 and the array
 * base in r1 (callers use r1 as the read/write base register). */
Next getNextEntryAddress_800AAAE(void) {
    u32 base = A_unk_2038170;

    R0 = base + R0 * 0x20;
    R1 = base;
    RETURN_TO_CALLER();
}

/* ── battle_stateGotoPhase14_800AABC ── */
/* battle_stateGotoPhase14_800AABC: advance the battle state into phase 0x14.
 * Runs setTwoStructs_800A840(4), then sets the battle state's phase index
 * to 0x14 and zeroes its halfword at offset 2. r5 is preserved (the original
 * pushes/pops it); r0 = battle state pointer, r1 = 0, flags as by `mov r1, #0`. */
Next battle_stateGotoPhase14_800AABC(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);

    R0 = 4;
    GUEST_CALL(setTwoStructs_800A840);

    battle = LD32(R10 + oToolkit_BattleStatePtr);
    ST8(battle + 1, 0x14);   /* oBattleState_Index_01 */
    ST16(battle + 2, 0);     /* oBattleState_StatePhaseAndInitFlag */

    R0 = battle;
    R1 = 0;
    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── sub_800AAF2 ── */
/* sub_800AAF2:
 * Passes 0 in r0 to clearIndexEntryPair_800AAFC and tail-returns its results
 * (r2/r3/r4 flow straight out of the callee). */
Next sub_800AAF2(void) {
    R0 = 0;
    GUEST_CALL(clearIndexEntryPair_800AAFC);
    RETURN_TO_CALLER();
}

/* ── clearIndexEntryPair_800AAFC ── */
/* clearIndexEntryPair_800AAFC(r0 = entry index):
 * Fills two 40-word (0x28) arrays with 0xFFFFFFFF, one entry into each.
 * The first array lives at unk_20018C0, the second at unk_2000260; both are
 * laid out as 0x28-byte records selected by the index in r0.
 * Caller's r3 and r4 are preserved; r2 is left at 0xFFFFFFFF.
 */
Next clearIndexEntryPair_800AAFC(void) {
    u32 index = R0;
    u32 callerR4 = R4;

    /* Clear entry `index` of the first table. */
    R0 = A_unk_20018C0 + index * 0x28;
    R1 = 0x28;
    R2 = 0xFFFFFFFF;
    GUEST_CALL(WordFill);

    /* Clear the matching entry of the second table. */
    R0 = A_unk_2000260 + index * 0x28;
    R1 = 0x28;
    R2 = 0xFFFFFFFF;
    GUEST_CALL(WordFill);

    /* Restore the caller's r4 (the tail of `push {r4,lr}` / `pop {r4,pc}`). */
    R4 = callerR4;
    RETURN_TO_CALLER();
}

/* ── clearWorkArea_2000FA0_800AB22 ── */
/* clearWorkArea_2000FA0_800AB22: clear word_2000FA0 (0x1A halfwords, i.e. 52 bytes).
 * R2/R3 pass through untouched; flags come from ZeroFillByHalfword. */
Next clearWorkArea_2000FA0_800AB22(void) {
    R0 = A_word_2000FA0;
    R1 = 0x1A;
    GUEST_CALL(ZeroFillByHalfword);
    RETURN_TO_CALLER();
}

/* ── table_GetByte16Row_203EAE0_800AB3A ── */
/* table_GetByte16Row_203EAE0_800AB3A(r0 = row, r1 = column):
 * read one byte from a 16-byte-per-row table at byte_203EAE0.
 * Returns the byte in r0 and the table address of that byte in r3. */
Next table_GetByte16Row_203EAE0_800AB3A(void) {
    u32 row = R0, column = R1;
    u32 entry = A_byte_203EAE0 + row * 0x10;
    R0 = LD8(entry + column);
    R3 = entry;
    RETURN_TO_CALLER();
}

/* ── tableAddSaturatedByte_800AB46 ── */
/* tableAddSaturatedByte_800AB46(r0 = row, r1 = column, r2 = addend):
 * Adds r2 to one byte of the table at byte_203EAE0 (16-byte rows selected
 * by r0, byte within the row by r1), saturating the result at 0xFF.
 * Sets c/v from the CMP against 0xFF and leaves the row base address in r3. */
Next tableAddSaturatedByte_800AB46(void) {
    u32 entry = A_byte_203EAE0 + R0 * 0x10;

    u32 sum = LD8(entry + R1) + R2;
    SET_FLAGS_CMP(sum, 0xFF);
    if ((s32)sum > 0xFF)
        sum = 0xFF;

    ST8(entry + R1, sum);
    R3 = entry;
    RETURN_TO_CALLER();
}

/* ── zeroFill_800AB70 ── */
/* zeroFill_800AB70: clear the 0x20-byte block at byte_203EAE0.
 * r2/r3 are passed through untouched. */
Next zeroFill_800AB70(void) {
    R0 = A_byte_203EAE0;
    R1 = 0x20;
    GUEST_CALL(ZeroFillByWord);
    RETURN_TO_CALLER();
}

/* ── battle_updatePhaseState_800AE0C ── */
/* battle_updatePhaseState_800AE0C: advance the battle's phase state machine.
 *
 * - If the current phase (Unk_1c) has moved past the previously recorded one
 *   (Unk_1b), record the new phase and, once it reaches 2 or more, kick off
 *   the setup routine for that phase (eStruct2035280_DrawMessageMode_801E228) unless the current battle
 *   mode is 6, which handles its own setup.
 * - Then count the phase timer (Unk_1d) down; when it expires, reset the
 *   phase back to 0.
 */
Next battle_updatePhaseState_800AE0C(void) {
    u32 savedR4 = R4;
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u32 phase = LD8(battle + oBattleState_PhaseMessageDelayCounter);
    u32 prevPhase = LD8(battle + oBattleState_PanelSelectedCount);

    SET_FLAGS_CMP(phase, prevPhase);
    if ((s32)phase > (s32)prevPhase) {
        ST8(battle + oBattleState_PanelSelectedCount, phase);
        SET_FLAGS_CMP(phase, 2);
        if ((s32)phase >= 2) {
            GUEST_CALL(GetBattleMode);
            SET_FLAGS_CMP(R0, 6);
            if (R0 != 6) {
                /* hand the new phase to the setup routine; it expects the
                 * battle state in r4 */
                R0 = phase - 2;
                R4 = battle;
                GUEST_CALL(eStruct2035280_DrawMessageMode_801E228);
                R4 = savedR4;
            }
        }
    }

    {
        u32 phaseTimer = LD8(battle + oBattleState_MessageTimer);
        SET_FLAGS_NZ(phaseTimer);
        if (phaseTimer != 0) {
            ST8(battle + oBattleState_MessageTimer, phaseTimer - 1);
        } else {
            ST8(battle + oBattleState_PhaseMessageDelayCounter, 0);
        }
    }

    RETURN_TO_CALLER();
}

/* ── battle_bumpTimerAndReloadSubcount_800AE44 ── */
/* battle_bumpTimerAndReloadSubcount_800AE44: advance the battle timer — bump the byte at Unk_1c and
 * reload the timeout counter at Unk_1d with 10. r10 = toolkit pointer. */
Next battle_bumpTimerAndReloadSubcount_800AE44(void) {
    u32 battleState = LD32(R10 + oToolkit_BattleStatePtr);

    ST8(battleState + oBattleState_PhaseMessageDelayCounter, LD8(battleState + oBattleState_PhaseMessageDelayCounter) + 1);
    ST8(battleState + oBattleState_MessageTimer, 10);

    /* output: r1 = battle state pointer (the original keeps r1 live) */
    R1 = battleState;
    RETURN_TO_CALLER();
}

/* ── sub_800AE54 ── */
/* sub_800AE54(r0 = key A, r1 = key B, r2 = list selector):
 * Search one of the battle state's object-pointer lists for an object whose
 * bytes 0x12/0x13 match r0/r1. The selector picks an entry of the descriptor
 * table dword_800AE8C, whose two bytes are {offset into battle state, entry
 * count}; the list itself is an array of object pointers at that offset.
 * Returns the matching object, or 0 if none matches. On return r2 holds the
 * remaining count (0 when nothing was found), r3 the address of the slot
 * examined last (found: the match itself; not found: one past the end).
 * r4, r6, r7 are preserved. */
Next sub_800AE54(void) {
    u32 battle = LD32(R10 + oToolkit_BattleStatePtr);
    u32 desc = A_dword_800AE8C + (R2 << 1);
    u32 slot = battle + LD8(desc);
    u32 remaining = LD8(desc + 1);

    for (;;) {
        u32 obj = LD32(slot);
        if (obj != 0
                && LD8(obj + 0x12) == R0
                && LD8(obj + 0x13) == R1) {
            R0 = obj;
            R2 = remaining;
            R3 = slot;
            break;
        }
        slot += 4;
        remaining -= 1;
        if (remaining == 0) {
            R0 = 0;
            R2 = 0;
            R3 = slot;
            break;
        }
    }
    RETURN_TO_CALLER();
}

/* ── battle_bustingLevelCategoryForEffect_800AF84 ── */
/* battle_bustingLevelCategoryForEffect_800AF84: choose a battle-mode pair and hand it to sub_800AC20.
 *
 *   If battle effect bit 3 (0x8) is clear:
 *     mode = effect bit 0 (0 or 1), arg1 = 0, r3 = 1, r2 = raw effects value.
 *   If bit 3 is set:
 *     mode = 2, arg1 = battleState->oBattleState_PlayerSide with its low bit
 *     flipped, unless battle_getStateByteNibbles_800A832 returned 1.
 *
 * r4-r7 and r10 pass through untouched; everything else is whatever
 * sub_800AC20 leaves behind. */
Next battle_bustingLevelCategoryForEffect_800AF84(void) {
    GUEST_CALL(GetBattleEffects);
    u32 effects = R0;

    if (effects & 8) {
        GUEST_CALL(battle_getStateByteNibbles_800A832);
        u32 nibble = R0;

        u32 statePtr = LD32(R10 + oToolkit_BattleStatePtr);
        u32 stateByte = LD8(statePtr + oBattleState_PlayerSide);

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

/* ── someChipHandValidationHappensHere_800B090 ── */
/* someChipHandValidationHappensHere_800B090 -- battle-start hand sanity check.
 *
 * Unless battle flag 0x40 is set, the hand entry selected by the hand's
 * leading byte is inspected and overwritten with the placeholder chip id
 * 0x185 when it is unusable: an empty slot (0xFFFF) is left alone, a chip
 * whose code is rejected by validateChipCode_8006EE8 is blanked, and element
 * chips (elemIdx 1/2) go through a stat-table check in which the original
 * overwrites the fetched stat byte with the hand address (mov r0, r9) before
 * comparing, so the table byte is really compared against the hand pointer.
 *
 * The original pushes r0-r9 and pops them before returning, so all ten
 * argument registers reach the caller unchanged.  The guest callees used
 * here preserve r3-r9; only r0-r2 get disturbed (call arguments/returns and
 * getChip8021DA8's r2 side result), so those three are round-tripped through
 * the typed function and written back by the adapter. */

#define PLACEHOLDER_CHIP_ID 0x185   /* dword_800B10C */

/* The caller's r0-r2, which the original leaves untouched. */
typedef struct { u32 r0, r1, r2; } CallerRegs;

/* register form: calls TestBattleFlag_0x40 outside this file, which has no typed C interface yet */
static bool test_battle_flag_0x40(u32 r8val, u32 r9val) {
    R0 = r8val;                    /* the original moves r8/r9 into r0/r1 first */
    R1 = r9val;
    GUEST_CALL(TestBattleFlag_0x40);
    return R0 != 0;                /* original: cmp r0, #0 / bne */
}

/* register form: calls getBattleHandAddr_8010018 outside this file, which has no typed C interface yet */
static u32 get_battle_hand_addr(u32 naviId) {
    R0 = naviId;
    GUEST_CALL(getBattleHandAddr_8010018);
    return R0;
}

/* register form: calls validateChipCode_8006EE8 outside this file, which has no typed C interface yet */
static bool chip_code_rejected(u32 chipId, u32 codeArg) {
    R0 = chipId;
    R1 = codeArg;
    GUEST_CALL(validateChipCode_8006EE8);
    return !ZERO_FLAG();           /* original: bne -> blank the entry */
}

/* register form: calls getChip8021DA8 outside this file, which has no typed C interface yet */
static u32 get_chip_data_addr(u32 chipId) {
    R0 = chipId;
    GUEST_CALL(getChip8021DA8);
    return R0;
}

/* register form: calls GetBattleNaviStatsByte outside this file, which has no typed C interface yet */
/* leftover unused: GetBattleNaviStatsByte's r0 (its only caller discards it; the original overwrites it) */
static u32 get_battle_navi_stats_byte(u32 naviId, u32 offset) {
    R0 = naviId;
    R1 = offset;
    GUEST_CALL(GetBattleNaviStatsByte);
    return R0;
}

/* Inspect the hand slot the hand's leading byte points at; blank it with the
 * placeholder chip when it fails validation. */
static CallerRegs validate_hand_chip(u32 r0In, u32 r1In, u32 r2In,
                                     u32 savedR8, u32 savedR9, u32 toolkit) {
    if (test_battle_flag_0x40(savedR8, savedR9))
        return (CallerRegs){ r0In, r1In, r2In };     /* flag 0x40: hand untouched */

    u32 battleState = LD32(toolkit + oToolkit_BattleStatePtr);
    u32 naviId = LD8(battleState + oBattleState_PlayerSide);

    /* Hand layout: one leading byte, then 2-byte chip entries from offset 2;
     * the leading byte selects the entry under scrutiny. */
    u32 hand = get_battle_hand_addr(naviId);
    u32 slotIndex = LD8(hand);
    u32 entry = hand + slotIndex * 2 + 2;
    u32 chipId = LD16(entry);

    if (chipId == 0xFFFF)                            /* empty slot: nothing to check */
        return (CallerRegs){ r0In, r1In, r2In };

    if (chip_code_rejected(chipId, 0xFF)) {
        ST16(entry, PLACEHOLDER_CHIP_ID);            /* bad code: blank the entry */
        return (CallerRegs){ r0In, r1In, r2In };
    }

    u32 elemIdx = LD8(get_chip_data_addr(chipId) + 7);   /* ChipData.elemIdx */
    if (elemIdx == 1 || elemIdx == 2) {
        /* The stat byte fetched here is immediately overwritten by the hand
         * address in the original (mov r0, r9), so the comparison below is
         * against the hand pointer, exactly as the game does it. */
        (void)get_battle_navi_stats_byte(naviId, 0xA + elemIdx);
        u32 tableByte = LD8(A_dword_20367E0 + elemIdx);
        if ((s32)tableByte >= (s32)hand)
            ST16(entry, PLACEHOLDER_CHIP_ID);
    }

    return (CallerRegs){ r0In, r1In, r2In };
}

/* someChipHandValidationHappensHere_800B090
 * inputs: r0-r10, lr -- only r10 is read as data; r8/r9 feed TestBattleFlag_0x40
 * outputs: r0-r9 left exactly as they arrived (the original push/pops them) */
Next someChipHandValidationHappensHere_800B090(void) {
    CallerRegs keep = validate_hand_chip(R0, R1, R2, R8, R9, R10);
    R0 = keep.r0;
    R1 = keep.r1;
    R2 = keep.r2;
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

/* ── battle_initBattleStateBuffer_800B144 ── */
#define NAVA_STATS_OFF   0xC
#define NAVA_STATS_WORDS 0x64
#define EVENT_FLAGS_OFF  0x3C4
#define SENTINEL         0x12345678u

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 get_battle_effects_sub_800B144(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls GetBattleMode outside this file, which has no typed C interface yet */
static u32 get_battle_mode_sub_800B144(void) {
    GUEST_CALL(GetBattleMode);
    return R0;
}

/* register form: calls GetCurPETNavi outside this file, which has no typed C interface yet */
static u32 get_cur_pet_navi_sub_800B144(void) {
    GUEST_CALL(GetCurPETNavi);
    return R0;
}

/* register form: calls GetNaviStatsAddrGivenCurPETNavi outside this file, which has no typed C interface yet */
static u32 get_navi_stats_addr(u32 pet_id) {
    R0 = pet_id;
    GUEST_CALL(GetNaviStatsAddrGivenCurPETNavi);
    return R0;
}

/* register form: calls GetNaviStats203CCE0Addr outside this file, which has no typed C interface yet */
static u32 get_navi_stats_backup_addr(u32 index) {
    R0 = index;
    GUEST_CALL(GetNaviStats203CCE0Addr);
    return R0;
}

/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void copy_words_sub_800B144(u32 src, u32 dest, u32 count) {
    R0 = src; R1 = dest; R2 = count;
    GUEST_CALL(CopyWords);
}

/* register form: calls CopyBytes outside this file, which has no typed C interface yet */
static void copy_bytes(u32 src, u32 dest, u32 count) {
    R0 = src; R1 = dest; R2 = count;
    GUEST_CALL(CopyBytes);
}

/* register form: calls sub_80134C8 outside this file, which has no typed C interface yet */
static void init_navi_stats_battle1(u32 navi_stats, u32 battle_buf) {
    R0 = navi_stats; R4 = battle_buf;
    GUEST_CALL(sub_80134C8);
}

/* register form: calls initNaviStats_WithDefaultStatsMaybe_8013438 outside this file, which has no typed C interface yet */
static void init_navi_stats_default(u32 navi_stats, u32 battle_buf) {
    R0 = navi_stats; R4 = battle_buf;
    GUEST_CALL(initNaviStats_WithDefaultStatsMaybe_8013438);
}

/* register form: calls sub_8013554 outside this file, which has no typed C interface yet */
static void init_navi_stats_battle7(u32 navi_stats, u32 battle_buf) {
    R0 = navi_stats; R4 = battle_buf;
    GUEST_CALL(sub_8013554);
}

/* register form: calls sub_80135E8 outside this file, which has no typed C interface yet */
static void init_navi_stats_battle9(u32 navi_stats, u32 battle_buf) {
    R0 = navi_stats; R4 = battle_buf;
    GUEST_CALL(sub_80135E8);
}

/* register form: calls battle_testFlag0x40_800A8F8 outside this file, which has no typed C interface yet */
static u32 battle_flag_0x40_test(u32 battle_buf) {
    R4 = battle_buf;
    GUEST_CALL(battle_testFlag0x40_800A8F8);
    return R0;
}

/* register form: calls TestEventFlagFromImmediate outside this file, which has no typed C interface yet */
static bool event_flag_163_set(void) {
    R0 = 1; R1 = 0x63;
    GUEST_CALL(TestEventFlagFromImmediate);
    return !ZERO_FLAG();
}

/* register form: calls sub_81209DC outside this file, which has no typed C interface yet */
static void get_byte_pair(u32 arg, u32 r6_val, u32 *lo, u32 *hi) {
    R0 = arg; R6 = r6_val;
    GUEST_CALL(sub_81209DC);
    *lo = R0; *hi = R1;
}

/* register form: calls stub_alwaysReturnsFalse_8010D20 outside this file, which has no typed C interface yet */
static u32 stub_false(u32 battle_buf) {
    R4 = battle_buf;
    GUEST_CALL(stub_alwaysReturnsFalse_8010D20);
    return R0;
}

/* register form: calls sub_8121198 outside this file, which has no typed C interface yet */
static u32 get_flag_163_result(void) {
    GUEST_CALL(sub_8121198);
    return R0;
}

/* register form: calls gameState_getProtectedBugfrags_803D0F4 outside this file, which has no typed C interface yet */
static u32 battle_state_query(u32 val, u32 offset) {
    R0 = val; R1 = offset;
    GUEST_CALL(gameState_getProtectedBugfrags_803D0F4);
    return R0;
}

/* register form: calls battle_requestSetBattleMode_80200A4 outside this file, which has no typed C interface yet */
static void battle_sfx_call(u32 id) {
    R0 = id;
    GUEST_CALL(battle_requestSetBattleMode_80200A4);
}

/* Fill count halfword-slots starting at addr with the given value. */
static void fill_halfwords(u32 addr, u32 value, u32 count) {
    for (u32 i = 0; i < count; i++)
        ST16(addr + i * 2, value);
}

/* Initialize the battle state buffer for the upcoming battle.
 * r6_input: caller's R6 at entry, used for sub_81209DC calls on non-default paths. */
static void init_battle_state(u32 toolkit_ptr, u32 r6_input) {
    u32 buf = A_dword_203CBE0;
    u32 navi_stats = buf + NAVA_STATS_OFF;

    ST32(buf + 4, LD32(A_ePrimaryRngSeed));
    ST32(buf + 8, LD32(LD32(toolkit_ptr + oToolkit_BattleStatePtr) + oBattleState_BattleSettings));

    bool use_backup = (get_battle_effects_sub_800B144() & 0x200000) != 0;
    bool skip_aux_copies = false;
    u32 r6_for_pairs = r6_input;

    if (use_backup) {
        /* Copy backup Navi stats and fill weapon arrays with 0x474 */
        u32 src = get_navi_stats_backup_addr(0);
        copy_words_sub_800B144(src, navi_stats, NAVA_STATS_WORDS);
        fill_halfwords(buf + 0x70, 0x474, 0x14);
        fill_halfwords(buf + 0x98, 0x474, 0x14);
        skip_aux_copies = true;
    } else {
        u32 mode = get_battle_mode_sub_800B144();
        if (mode == 1) {
            init_navi_stats_battle1(navi_stats, buf);
        } else if (mode == 2 || mode == 3 || mode == 4) {
            init_navi_stats_default(navi_stats, buf);
        } else if (mode == 5 || mode == 8) {
            init_navi_stats_default(navi_stats, buf);
            u32 src = get_navi_stats_addr(0);
            ST16(navi_stats + 0x3E, LD16(src + 0x3E));
            u32 word = LD32(src + 0x40);
            u32 hi = word >> 16;
            ST32(navi_stats + 0x40, (hi << 16) | hi);
        } else if (mode == 7) {
            init_navi_stats_battle7(navi_stats, buf);
        } else if (mode == 9) {
            init_navi_stats_battle9(navi_stats, buf);
        } else {
            /* Default: copy current PET-Navi stats */
            u32 pet = get_cur_pet_navi_sub_800B144();
            u32 src = get_navi_stats_addr(pet);
            copy_words_sub_800B144(src, navi_stats, NAVA_STATS_WORDS);

            if (get_battle_effects_sub_800B144() & 8)
                ST8(navi_stats + 0x21, 3);

            /* Special handling when battle flag 0x40 set, EVENT_163 clear, no PET */
            if (battle_flag_0x40_test(buf) && !event_flag_163_set()) {
                if (get_cur_pet_navi_sub_800B144() == 0)
                    ST8(navi_stats + 0x17, 0xC);
            }
            r6_for_pairs = navi_stats;
        }
    }

    if (!skip_aux_copies) {
        copy_words_sub_800B144(A_unk_20018C0, buf + 0x70, 0x28);
        copy_words_sub_800B144(A_unk_2000260, buf + 0x98, 0x28);
    }

    /* Query byte pairs (sub_81209DC reads R6) */
    u32 lo, hi;
    get_byte_pair(1, r6_for_pairs, &lo, &hi);
    ST8(buf + 0xC8, lo);
    ST8(buf + 0xC9, hi);

    get_byte_pair(1, r6_for_pairs, &lo, &hi);
    ST8(buf + 0xCA, lo);
    ST8(buf + 0xCB, hi);

    /* Snapshot event flags into buffer */
    copy_bytes(LD32(toolkit_ptr + oToolkit_EventFlagsPtr) + EVENT_FLAGS_OFF,
               buf + 0xCC, 0x28);

    ST32(buf + 0xF4, stub_false(buf));

    /* EVENT_163 determines navi ID query result */
    u32 flag_result;
    if (event_flag_163_set())
        flag_result = get_flag_163_result();
    else
        flag_result = 0xFF;
    ST32(buf + 0xC0, flag_result);

    ST32(buf + 0xC4, battle_state_query(flag_result, 0xC0));

    ST32(buf, SENTINEL);
    battle_sfx_call(0x3E);
}

/* battle_initBattleStateBuffer_800B144(r10 = toolkit, r6 = caller R6) -> c v r1 r2 r3 (r4/r6/r7/r5 preserved) */
Next battle_initBattleStateBuffer_800B144(void) {
    u32 saved_r4 = R4, saved_r6 = R6;
    init_battle_state(R10, saved_r6);
    R4 = saved_r4;
    R6 = saved_r6;
    RETURN_TO_CALLER();
}

/* ── battle_copyStructsIncludingBattleStats_800b2d8 ── */
/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void copy_words_battle_copyStructsIncludingBattleStats_800b2d8(u32 src, u32 dest, u32 size) {
    R0 = src; R1 = dest; R2 = size;
    GUEST_CALL(CopyWords);
}

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 get_battle_effects_battle_copyStructsIncludingBattleStats_800b2d8(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* register form: calls util_nandByteArray40 outside this file, which has no typed C interface yet */
static void nand_byte_arrays(u32 a, u32 b, u32 c) {
    R0 = a; R1 = b; R2 = c;
    GUEST_CALL(util_nandByteArray40_80AAB88);
}

/* Copy the working navi-stats buffer into all four battle slots, optionally
 * the secondary buffer when a specific battle-effect bit is active, then
 * transfer auxiliary data (RNG seed, 40-byte arrays, battle-result dwords). */
static void copy_battle_structs(void) {
    /* Copy working stats into the four active navi slots */
    copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_eBattleNaviStats203F4AC, A_eBattleNaviStats0,        oNaviStats_Size);
    copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_eBattleNaviStats203F4AC, A_eBattleNaviStats203CB10, oNaviStats_Size);
    copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_eBattleNaviStats203F4AC, A_eBattleNaviStats2034A60, oNaviStats_Size);
    copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_eBattleNaviStats203F4AC, A_eBattleNaviStats203C9E4, oNaviStats_Size);

    /* If battle-effect bit 3 is set, also propagate the secondary stats buffer */
    if (get_battle_effects_battle_copyStructsIncludingBattleStats_800b2d8() & 8) {
        copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_byte_203F5AC, A_eBattleNaviStats1,          oNaviStats_Size);
        copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_byte_203F5AC, A_eBattleNaviStats203CB74,    oNaviStats_Size);
        copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_byte_203F5AC, A_eBattleNaviStats2034AC4,     oNaviStats_Size);
        copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_byte_203F5AC, A_eBattleNaviStats203C980,         oNaviStats_Size);
    }

    /* Snapshot the RNG seed from the working copy */
    ST32(A_ePrimaryRngSeed, LD32(A_byte_203F4A4));

    /* Copy four auxiliary 0x28-byte data arrays */
    copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_byte_203F510, A_byte_203EB00, 0x28);
    copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_byte_203F610, A_byte_203EB28, 0x28);
    copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_byte_203F538, A_unk_2036790,  0x28);
    copy_words_battle_copyStructsIncludingBattleStats_800b2d8(A_byte_203F638, A_unk_20367B8,  0x28);

    /* Copy a single dword counter */
    ST32(A_byte_203CA50, LD32(A_dword_203F568));

    /* NAND three 40-byte battle-availability arrays */
    nand_byte_arrays(A_byte_203F56C, A_byte_203F66C, A_unk_2036750);

    /* Write paired dwords into the battle-result structure */
    ST32(A_dword_203CDF0,     LD32(A_byte_203F594));
    ST32(A_dword_203CDF0 + 4, LD32(A_byte_203F694));
    ST32(A_dword_203CFA0,     LD32(A_dword_203F560));
    ST32(A_dword_203CFA0 + 4, LD32(A_dword_203F660));
    ST32(A_dword_203F7E0,     LD32(A_dword_203F564));
    ST32(A_dword_203F7E0 + 4, LD32(A_byte_203F664));

    R2 = A_dword_203F7E0; /* register form: the trailing LDR r2,[pc] pool reads leave R2 at this address for the caller */
}

/* battle_copyStructsIncludingBattleStats_800b2d8() */
Next battle_copyStructsIncludingBattleStats_800b2d8(void) {
    copy_battle_structs();
    RETURN_TO_CALLER();
}

/* ── battle_backupStateForRestart_800B3A2 ── */
/* battle_backupStateForRestart_800B3A2: back up the battle state before a restart/continue.
 *
 * Copies the 0x50-word battle work buffer and the current Navi's stats
 * block (0x64 words) into their backup buffers, writes a "data valid"
 * marker word, snapshots a small 0x10-word block, and finally invokes
 * battle_requestSetBattleMode_80200A4(0x32) to finish the snapshot.
 *
 * Inputs:  r3 (unused), r10 = toolkit pointer, lr = return address.
 * Outputs: r2/r3 and the c/v flags as left by the final battle_requestSetBattleMode_80200A4 call. */
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
    GUEST_CALL(battle_requestSetBattleMode_80200A4);

    RETURN_TO_CALLER();
}

/* ── transferBattleHandNaviStats_800B3D8 ── */
/* Effect flag bit checked in GetBattleEffects to decide if the secondary
 * (AI) hand/navi stats buffers are used in place of the primary ones. */
#define BATTLE_EFFECT_FLAG_8  0x8

/* Size of a battle hand/navi stats record copied by CopyWords (in bytes). */
#define HAND_NAVI_RECORD_SIZE 0x50
/* Size of a battle navi stats record copied by CopyWords (in bytes). */
#define BATTLE_NAVI_STATS_SIZE 0x64

/* register form: calls CopyWords outside this file, which has no typed C interface yet */
static void copy_words(const void *src, void *dst, u32 size) {
    R0 = (u32)src;
    R1 = (u32)dst;
    R2 = size;
    GUEST_CALL(CopyWords);
}

/* register form: calls GetBattleEffects outside this file, which has no typed C interface yet */
static u32 get_battle_effects_transferBattleHandNaviStats_800B3D8(void) {
    GUEST_CALL(GetBattleEffects);
    return R0;
}

/* When the slot at the given marker address is not 0xFF, copy HAND_NAVI_RECORD_SIZE bytes
 * from it into the corresponding destination buffer. */
static void copy_slot_if_active(u32 marker_addr, u32 dst_addr) {
    if (LD8(marker_addr) != 0xFF) {
        copy_words((const void *)marker_addr, (void *)dst_addr, HAND_NAVI_RECORD_SIZE);
    }
}

/* Transfer the battle hand/navi stats: the primary pairs are always copied
 * (subject to per-slot activity), and the AI-flagged pairs additionally
 * when the AI battle-effect flag is set in GetBattleEffects. */
static void transfer_battle_hand_navi_stats(void) {
    /* Primary hand/navi stats: copy only if the slot's marker byte is not 0xFF. */
    copy_slot_if_active(A_byte_203F4A4, A_byte_20349C0);

    /* AI battle-effect flag => copy the secondary hand/navi stats if active. */
    if (get_battle_effects_transferBattleHandNaviStats_800B3D8() & BATTLE_EFFECT_FLAG_8) {
        copy_slot_if_active(A_byte_203F5A4, A_byte_2034A10);
    }

    /* Primary battle navi stats block (always copied). */
    copy_words((const void *)A_byte_203F4F4, (void *)A_eBattleNaviStats0, BATTLE_NAVI_STATS_SIZE);

    /* AI battle-effect flag => copy the secondary battle navi stats block. */
    if (get_battle_effects_transferBattleHandNaviStats_800B3D8() & BATTLE_EFFECT_FLAG_8) {
        copy_words((const void *)A_byte_203F5F4, (void *)A_eBattleNaviStats1, BATTLE_NAVI_STATS_SIZE);
    }
}

Next transferBattleHandNaviStats_800B3D8(void) {
    transfer_battle_hand_navi_stats();
    RETURN_TO_CALLER();
}

/* ── sub_800B428 ── */
/* sub_800B428:
 * Reinitializes the record whose pointer lives at dword_203CBE4 (via sub_802C8FA),
 * writes the marker word 0x1F2F3F4F to dword_203CBE0, then invokes battle_requestSetBattleMode_80200A4
 * with mode 4. battle_requestSetBattleMode_80200A4's r1-r3 results pass straight through to our caller. */
Next sub_800B428(void) {
    /* Reset/clear the structure at dword_203CBE4. */
    R0 = A_dword_203CBE4;
    GUEST_CALL(sub_802C8FA);

    /* Stamp the magic value into dword_203CBE0. */
    ST32(A_dword_203CBE0, 0x1F2F3F4F);

    /* Continue with operation mode 4; r1-r3 are left exactly as the callee set them. */
    R0 = 4;
    GUEST_CALL(battle_requestSetBattleMode_80200A4);

    RETURN_TO_CALLER();
}

/* ── sub_800B444 ── */
/* sub_800B444: reload the two saved 12-byte blocks into the working battle
 * data area (byte_203F4A4 -> unk_2035260 and byte_203F5A4 -> unk_203526C),
 * then run battle_syncEffectSlotBetweenBuffers_802C9EA to act on the restored data. */
Next sub_800B444(void) {
    /* first block: 3 words */
    R0 = A_byte_203F4A4;
    R1 = A_unk_2035260;
    R2 = 0xC;
    GUEST_CALL(CopyWords);

    /* second block: 3 words */
    R0 = A_byte_203F5A4;
    R1 = A_unk_203526C;
    R2 = 0xC;
    GUEST_CALL(CopyWords);

    GUEST_CALL(battle_syncEffectSlotBetweenBuffers_802C9EA);

    RETURN_TO_CALLER();
}

/* ── battle_resetLinkSyncFlags_800B460 ── */
/* battle_resetLinkSyncFlags_800B460: zero the 32-bit values at 0x203F4A0 and 0x203F5A0. */
Next battle_resetLinkSyncFlags_800B460(void) {
    ST32(A_dword_203F4A0, 0);
    ST32(A_dword_203F5A0, 0);
    RETURN_TO_CALLER();
}

/* ── battle_doBothTrackedIdsMatch_800B46C ── */
/* battle_doBothTrackedIdsMatch_800B46C(r0 = id): r0 = 1 if both tracked ids (dword_203F4A0 and
 * dword_203F5A0) equal the given id, 0 otherwise.
 * Also hands the loaded values back in r1/r2 and the id in r3; flags are
 * left from the last comparison performed, so callers may branch on them. */
Next battle_doBothTrackedIdsMatch_800B46C(void) {
    u32 id = R0;
    u32 idA = LD32(A_dword_203F4A0);
    u32 idB = LD32(A_dword_203F5A0);

    R3 = id;
    R1 = idA;
    R2 = idB;

    if (idA != id) {
        R0 = 0;
        SET_FLAGS_CMP(idA, id);
    } else if (idB != id) {
        R0 = 0;
        SET_FLAGS_CMP(idB, id);
    } else {
        R0 = 1;
        SET_FLAGS_CMP(idB, id);
    }
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setBattleResult ── */
/* eStruct200A008_setBattleResult(r0 = battle result):
 * Store the battle result byte at byte_200A009 (one byte past
 * eOWPlayerObjectEnd), leaving the field's base address in r1. */
Next eStruct200A008_setBattleResult(void) {
    R1 = A_eOWPlayerObjectEnd;
    ST8(R1 + 1, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getBattleResult ── */
/* eStruct200A008_getBattleResult: return the battle-result flag stored at
 * byte_200A009 (one byte past the start of the OW player object table).
 * Outputs: r0 = flag value, r1 = eOWPlayerObjectEnd (0x200A008). */
Next eStruct200A008_getBattleResult(void) {
    R1 = A_eOWPlayerObjectEnd;   /* literal pool value: eOWPlayerObjectEnd */
    R0 = LD8(R1 + 1);                         /* byte_200A009 */
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk00 ── */
/* eStruct200A008_setUnk00(r0 = value):
 * stores a single byte at the eOWPlayerObject_SpriteData_Size location. */
Next eStruct200A008_setUnk00(void) {
    ST8(A_eOWPlayerObjectEnd, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk02 ── */
/* eStruct200A008_setUnk02(r0 = value):
 * Stores r0 into the byte at offset 2 of the struct at 0x200A008
 * (byte_200A00A), leaving the struct base address in r1. */
Next eStruct200A008_setUnk02(void) {
    u32 value = R0;

    R1 = A_eOWPlayerObjectEnd;
    ST8(R1 + 2, value);

    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk04 ── */
/* eStruct200A008_setUnk04(r0 = value):
 * Write a 16-bit value to the field at offset +4 of the struct based at
 * eOWPlayerObject_SpriteData_Size (i.e. word_200A00C). Used as a simple
 * setter for that field; nothing else is touched. */
Next eStruct200A008_setUnk04(void) {
    u32 structBase = A_eOWPlayerObjectEnd; /* 0x200A008 */
    ST16(structBase + 4, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getUnk04 ── */
/* eStruct200A008_getUnk04: return the u16 field at 0x200A00C (the player's
 * overworld object struct's "Unk04" field). Result in r0. */
Next eStruct200A008_getUnk04(void) {
    R0 = LD16(A_eOWPlayerObjectEnd + 4);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_setUnk06 ── */
/* eStruct200A008_setUnk06(r0 = value):
 * Store r0 as a halfword at word_200A00E — the +0x6 field of the
 * struct whose base is the literal eOWPlayerObjectEnd. */
Next eStruct200A008_setUnk06(void) {
    ST16(A_eOWPlayerObjectEnd + 6, R0);
    RETURN_TO_CALLER();
}

/* ── eStruct200A008_getUnk06 ── */
/* eStruct200A008_getUnk06:
 * read the halfword field at offset 6 of the struct at eOWPlayerObjectEnd
 * (word_200A00E) and return it in r0. */
#define word_200A00E (A_eOWPlayerObjectEnd + 6)

Next eStruct200A008_getUnk06(void) {
    R0 = LD16(word_200A00E);
    RETURN_TO_CALLER();
}

#undef word_200A00E

/* ── eStruct200A008_setUnk03 ── */
/* eStruct200A008_setUnk03(r0 = value):
 * Store the low byte of r0 into the byte just past the end of the overworld
 * player object (byte_200A00B, the "unk03" field of struct 200A008).
 * Leaves r1 = end-of-player-object address (200A008), as the original does. */
Next eStruct200A008_setUnk03(void) {
    u32 base = A_eOWPlayerObjectEnd;

    ST8(base + 3, R0);
    R1 = base;

    RETURN_TO_CALLER();
}

/* ── sub_800B6F2 ── */
/* sub_800B6F2: Scan the battle state's pending-event list and fire any
 * event flags for entries with value >= 0x100 (the flag number is
 * value + 0x300).  Skipped entirely when battle-effect bits 3 or 19
 * indicate a special mode is active.
 *
 * The battle state holds two parallel event arrays selected by the
 * "side" bit from state byte 0x0d:
 *   count byte  at  offset 4 + side   (r1)
 *   event u16[] at  offset 0x4C + 8*side  (r7 base) */
Next sub_800B6F2(void) {
    u32 statePtr = LD32(R10 + oToolkit_BattleStatePtr);

    /* Check battle-effect flags: bit 3 (0x8) or bit 19 (0x80000) */
    GUEST_CALL(GetBattleEffects);
    R1 = 8;
    if (R0 & 8) {
        SET_FLAGS_NZ(R0 & 8);
        RETURN_TO_CALLER();
    }

    R1 = 0x80000;
    if (R0 & 0x80000) {
        SET_FLAGS_NZ(R0 & 0x80000);
        RETURN_TO_CALLER();
    }

    /* Determine which event list: invert byte-0x0d state (side 0 ↔ 1) */
    GUEST_CALL(battle_getStateByte0d_800A7E2);
    u32 side = R0 ^ 1;

    R1 = side + 4;
    u32 count = LD8(statePtr + R1);
    statePtr += (side << 3) + 0x4C;

    /* Walk the event array; fire flags for entries >= 0x100 */
    do {
        u16 evt = LD16(statePtr);

        R1 = 0x100;
        s32 adjusted = (s32)evt - 0x100;   /* SUBS evt, #0x100 */
        if (adjusted >= 0) {
            R1 = 0x300;
            R0 = (u32)(adjusted + 0x300);
            GUEST_CALL(SetEventFlag);
        }

        statePtr += 2;
        s32 prevCount = (s32)count;
        count--;
        SET_FLAGS_CMP(prevCount, 1);       /* SUBS count, #1 – final flags */
    } while ((s32)count > 0);             /* BGT */

    RETURN_TO_CALLER();
}

/* ── sub_800B75A ── */
/* sub_800B75A: store 0xFF (a "cancel/back out" flag?) into byte_203C960. */
Next sub_800B75A(void) {
    ST8(A_byte_203C960, 0xFF);
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
