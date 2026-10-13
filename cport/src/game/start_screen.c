#include <stddef.h>
#include <stdint.h>
#include "game.h"
#define G(T, addr) ((T *)(uintptr_t)(u32)(addr))

/*
 * start_screen.c
 *
 * Implements the MMBN6 Falzar start-screen state machine.  The main render
 * loop drives three nested jump tables (initial GFX setup, the title screen,
 * and then load/new-game dispatch).  Menu movement, animation, OAM submits,
 * and flag-triggered script lists all live here.
 */

/* oStartScreen: start_screen_struct (bn6f/include/structs/StartScreen.inc) */
typedef struct StartScreen {
    u8 JumpTableOff_00;                      /* 0x00 */
    u8 JumpTableOff_01;                      /* 0x01 */
    u8 Unk_02;                               /* 0x02 */
    u8 Unk_03;                               /* 0x03 */
    u16 LogoShowTimer;                       /* 0x04 */
    u8 Unk_06;                               /* 0x06 */
    u8 Unk_07;                               /* 0x07 */
    u8 CurrCursorPos;                        /* 0x08 */
    u8 NumChoices;                           /* 0x09 */
    u16 AchievmentFlags;                     /* 0x0A */
    u8 Unk_0C;                               /* 0x0C */
    u8 Und_0D;                               /* 0x0D */
    u8 Unk_0E;                               /* 0x0E */
    u8 Unk_0F;                               /* 0x0F */
    u8 pad_10[0x10];
} StartScreen;
_Static_assert(offsetof(StartScreen, JumpTableOff_00) == 0x0, "StartScreen.JumpTableOff_00");
_Static_assert(offsetof(StartScreen, JumpTableOff_01) == 0x1, "StartScreen.JumpTableOff_01");
_Static_assert(offsetof(StartScreen, Unk_02) == 0x2, "StartScreen.Unk_02");
_Static_assert(offsetof(StartScreen, Unk_03) == 0x3, "StartScreen.Unk_03");
_Static_assert(offsetof(StartScreen, LogoShowTimer) == 0x4, "StartScreen.LogoShowTimer");
_Static_assert(offsetof(StartScreen, Unk_06) == 0x6, "StartScreen.Unk_06");
_Static_assert(offsetof(StartScreen, Unk_07) == 0x7, "StartScreen.Unk_07");
_Static_assert(offsetof(StartScreen, CurrCursorPos) == 0x8, "StartScreen.CurrCursorPos");
_Static_assert(offsetof(StartScreen, NumChoices) == 0x9, "StartScreen.NumChoices");
_Static_assert(offsetof(StartScreen, AchievmentFlags) == 0xA, "StartScreen.AchievmentFlags");
_Static_assert(offsetof(StartScreen, Unk_0C) == 0xC, "StartScreen.Unk_0C");
_Static_assert(offsetof(StartScreen, Und_0D) == 0xD, "StartScreen.Und_0D");
_Static_assert(offsetof(StartScreen, Unk_0E) == 0xE, "StartScreen.Unk_0E");
_Static_assert(offsetof(StartScreen, Unk_0F) == 0xF, "StartScreen.Unk_0F");
_Static_assert(sizeof(StartScreen) == 0x20, "sizeof StartScreen");

/* Generated wrappers for functions this subsystem calls outside itself. */
typedef struct { u32 r0, r1; } U32Pair;   /* a result in r0 and r1 */

typedef struct { bool z; } PlaySoundEffect_result;   /* z: the Z flag it leaves */
/* register form: generated typed wrapper: PlaySoundEffect's signature is confident (symbols/db PlaySoundEffect.yaml, signature:) */
static inline PlaySoundEffect_result call_PlaySoundEffect(u8 r0) {
    R0 = r0;
    GUEST_CALL(PlaySoundEffect);
    return (PlaySoundEffect_result){ ZERO_FLAG() };
}

/* register form: generated typed wrapper: music_80005F2's signature is confident (symbols/db music_80005F2.yaml, signature:) */
static inline void call_music_80005F2(u8 songId) {
    R0 = songId;
    GUEST_CALL(music_80005F2);
}

/* register form: generated typed wrapper: sound_dispatchCueHandler_800068A's signature is confident (symbols/db sound_dispatchCueHandler_800068A.yaml, signature:) */
static inline void call_sound_dispatchCueHandler_800068A(u32 cueId) {
    R0 = cueId;
    GUEST_CALL(sound_dispatchCueHandler_800068A);
}

/* register form: generated typed wrapper: musicGameState_8000784's signature is confident (symbols/db musicGameState_8000784.yaml, signature:) */
static inline U32Pair call_musicGameState_8000784(u8 r0) {
    R0 = r0;
    GUEST_CALL(musicGameState_8000784);
    return (U32Pair){ R0, R1 };
}

/* register form: generated typed wrapper: ZeroFillByEightWords's signature is confident (symbols/db ZeroFillByEightWords.yaml, signature:) */
static inline void call_ZeroFillByEightWords(u32 r0, u16 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(ZeroFillByEightWords);
}

/* register form: generated typed wrapper: CopyWords's signature is confident (symbols/db CopyWords.yaml, signature:) */
static inline void call_CopyWords(u32 r0, u32 r1, u8 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyWords);
}

/* register form: generated typed wrapper: CopyByEightWords's signature is confident (symbols/db CopyByEightWords.yaml, signature:) */
static inline void call_CopyByEightWords(u32 r0, u32 r1, u16 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(CopyByEightWords);
}

/* register form: generated typed wrapper: WordFill's signature is confident (symbols/db WordFill.yaml, signature:) */
static inline void call_WordFill(u32 value, u32 * dest, u32 size) {
    R0 = value;
    R1 = (u32)(uintptr_t)dest;
    R2 = size;
    GUEST_CALL(WordFill);
}

/* register form: generated typed wrapper: decompAndCopyData's signature is confident (symbols/db decompAndCopyData.yaml, signature:) */
static inline void call_decompAndCopyData(u32 r0) {
    R0 = r0;
    GUEST_CALL(decompAndCopyData);
}

/* register form: generated typed wrapper: GetTitleScreenIconCount's signature is confident (symbols/db GetTitleScreenIconCount.yaml, signature:) */
static inline U32Pair call_GetTitleScreenIconCount(void) {
    GUEST_CALL(GetTitleScreenIconCount);
    return (U32Pair){ R0, R1 };
}

/* register form: generated typed wrapper: scriptListRun_8001158's signature is confident (symbols/db scriptListRun_8001158.yaml, signature:) */
static inline void call_scriptListRun_8001158(const u8 * r0) {
    R0 = (u32)(uintptr_t)r0;
    GUEST_CALL(scriptListRun_8001158);
}

/* register form: generated typed wrapper: GetRNG's signature is confident (symbols/db GetRNG.yaml, signature:) */
static inline u32 call_GetRNG(void) {
    GUEST_CALL(GetRNG);
    return R0;
}

/* register form: generated typed wrapper: toolkit_applyOverlayPalette_80015FC's signature is confident (symbols/db toolkit_applyOverlayPalette_80015FC.yaml, signature:) */
static inline void call_toolkit_applyOverlayPalette_80015FC(u8 r0) {
    R0 = r0;
    GUEST_CALL(toolkit_applyOverlayPalette_80015FC);
}

/* register form: generated typed wrapper: SetRenderInfoLCDControl's signature is confident (symbols/db SetRenderInfoLCDControl.yaml, signature:) */
static inline void call_SetRenderInfoLCDControl(u32 r0) {
    R0 = r0;
    GUEST_CALL(SetRenderInfoLCDControl);
}

/* register form: generated typed wrapper: renderInfo_8001788's signature is confident (symbols/db renderInfo_8001788.yaml, signature:) */
static inline void call_renderInfo_8001788(void) {
    GUEST_CALL(renderInfo_8001788);
}

/* register form: generated typed wrapper: renderInfo_80017A0's signature is confident (symbols/db renderInfo_80017A0.yaml, signature:) */
static inline void call_renderInfo_80017A0(void) {
    GUEST_CALL(renderInfo_80017A0);
}

/* register form: generated typed wrapper: zeroFillVRAM's signature is confident (symbols/db zeroFillVRAM.yaml, signature:) */
static inline void call_zeroFillVRAM(void) {
    GUEST_CALL(zeroFillVRAM);
}

/* register form: generated typed wrapper: ZeroFillGFX30025c0's signature is confident (symbols/db ZeroFillGFX30025c0.yaml, signature:) */
static inline void call_ZeroFillGFX30025c0(void) {
    GUEST_CALL(ZeroFillGFX30025c0);
}

/* register form: generated typed wrapper: CopyBackgroundTiles's signature is confident (symbols/db CopyBackgroundTiles.yaml, signature:) */
static inline void call_CopyBackgroundTiles(u8 r0, u8 r1, u8 r2, u32 r3, u8 r4, u8 r5) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    R4 = r4;
    R5 = r5;
    GUEST_CALL(CopyBackgroundTiles);
}

/* register form: generated typed wrapper: zeroFill_e20094C0's signature is confident (symbols/db zeroFill_e20094C0.yaml, signature:) */
static inline void call_zeroFill_e20094C0(void) {
    GUEST_CALL(zeroFill_e20094C0);
}

/* register form: generated typed wrapper: LoadGFXAnim's signature is confident (symbols/db LoadGFXAnim.yaml, signature:) */
static inline void call_LoadGFXAnim(const u8 * r0) {
    R0 = (u32)(uintptr_t)r0;
    GUEST_CALL(LoadGFXAnim);
}

/* register form: generated typed wrapper: LoadGFXAnims's signature is confident (symbols/db LoadGFXAnims.yaml, signature:) */
static inline void call_LoadGFXAnims(u32 data) {
    R0 = data;
    GUEST_CALL(LoadGFXAnims);
}

/* register form: generated typed wrapper: palette_clearEPalette20097a0_80023A8's signature is confident (symbols/db palette_clearEPalette20097a0_80023A8.yaml, signature:) */
static inline void call_palette_clearEPalette20097a0_80023A8(void) {
    GUEST_CALL(palette_clearEPalette20097a0_80023A8);
}

/* register form: generated typed wrapper: resetOverworldScene_800260C's signature is confident (symbols/db resetOverworldScene_800260C.yaml, signature:) */
static inline void call_resetOverworldScene_800260C(void) {
    GUEST_CALL(resetOverworldScene_800260C);
}

/* register form: generated typed wrapper: scenario_prepareEnterMap_8004D48's signature is confident (symbols/db scenario_prepareEnterMap_8004D48.yaml, signature:) */
static inline void call_scenario_prepareEnterMap_8004D48(void) {
    GUEST_CALL(scenario_prepareEnterMap_8004D48);
}

/* register form: generated wrapper: initNewGameData_8004DF0's signature is unsure, so every register its contract may read is passed (r4, r9, r11, r12 pass through as the guest has them: no call site here sets them) */
static inline void call_initNewGameData_8004DF0(u32 r7) {
    R7 = r7;
    GUEST_CALL(initNewGameData_8004DF0);
}

typedef struct { u32 r0; u32 r1; bool z; } SetScreenFade_result;   /* z: the Z flag it leaves */
/* register form: generated typed wrapper: SetScreenFade's signature is confident (symbols/db SetScreenFade.yaml, signature:) */
static inline SetScreenFade_result call_SetScreenFade(u8 r0, u8 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetScreenFade);
    return (SetScreenFade_result){ R0, R1, ZERO_FLAG() };
}

typedef struct { u32 r0; u32 r1; bool z; } IsScreenFadeActive_result;   /* z: the Z flag it leaves */
/* register form: generated typed wrapper: IsScreenFadeActive's signature is confident (symbols/db IsScreenFadeActive.yaml, signature:) */
static inline IsScreenFadeActive_result call_IsScreenFadeActive(void) {
    GUEST_CALL(IsScreenFadeActive);
    return (IsScreenFadeActive_result){ R0, R1, ZERO_FLAG() };
}

/* register form: generated typed wrapper: SetEventFlagFromImmediate's signature is confident (symbols/db SetEventFlagFromImmediate.yaml, signature:) */
static inline U32Pair call_SetEventFlagFromImmediate(u8 r0, u8 r1) {
    R0 = r0;
    R1 = r1;
    GUEST_CALL(SetEventFlagFromImmediate);
    return (U32Pair){ R0, R1 };
}

/* register form: generated typed wrapper: sub_802FE28's signature is confident (symbols/db sub_802FE28.yaml, signature:) */
static inline u8 call_sub_802FE28(u32 r0, u16 r1, u32 r2, u32 r3) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    R3 = r3;
    GUEST_CALL(sub_802FE28);
    return R0;
}

/* register form: generated typed wrapper: sub_8039570's signature is confident (symbols/db sub_8039570.yaml, signature:) */
static inline U32Pair call_sub_8039570(void) {
    GUEST_CALL(sub_8039570);
    return (U32Pair){ R0, R1 };
}

/* register form: generated typed wrapper: call_803D1AC's signature is confident (symbols/db call_803D1AC.yaml, signature:) */
static inline U32Pair call_call_803D1AC(void) {
    GUEST_CALL(call_803D1AC);
    return (U32Pair){ R0, R1 };
}

/* register form: generated typed wrapper: init_eStartScreenAnimationControl200B1A0_1's signature is confident (symbols/db init_eStartScreenAnimationControl200B1A0_1.yaml, signature:) */
static inline void call_init_eStartScreenAnimationControl200B1A0_1(void) {
    GUEST_CALL(init_eStartScreenAnimationControl200B1A0_1);
}

/* register form: generated typed wrapper: startScreen_IsTitleAnimationFinished_803E928's signature is confident (symbols/db startScreen_IsTitleAnimationFinished_803E928.yaml, signature:) */
static inline bool call_startScreen_IsTitleAnimationFinished_803E928(void) {
    GUEST_CALL(startScreen_IsTitleAnimationFinished_803E928);
    return ZERO_FLAG();   /* z set */
}

/* register form: generated typed wrapper: startScreen_TstZero's signature is confident (symbols/db startScreen_TstZero.yaml, signature:) */
static inline bool call_startScreen_TstZero(void) {
    GUEST_CALL(startScreen_TstZero);
    return ZERO_FLAG();   /* z set */
}

/* register form: generated typed wrapper: startScreen_AnimatePressStart_803E938's signature is confident (symbols/db startScreen_AnimatePressStart_803E938.yaml, signature:) */
static inline void call_startScreen_AnimatePressStart_803E938(void) {
    GUEST_CALL(startScreen_AnimatePressStart_803E938);
}

/* register form: generated typed wrapper: eStruct200BC30_setJumpOffset00's signature is confident (symbols/db eStruct200BC30_setJumpOffset00.yaml, signature:) */
static inline void call_eStruct200BC30_setJumpOffset00(u8 value) {
    R0 = value;
    GUEST_CALL(eStruct200BC30_setJumpOffset00);
}

/* register form: generated typed wrapper: clearStruct200BC30_803EBAC's signature is confident (symbols/db clearStruct200BC30_803EBAC.yaml, signature:) */
static inline void call_clearStruct200BC30_803EBAC(void) {
    GUEST_CALL(clearStruct200BC30_803EBAC);
}

/* register form: generated typed wrapper: clear_e200AD04's signature is confident (symbols/db clear_e200AD04.yaml, signature:) */
static inline void call_clear_e200AD04(void) {
    GUEST_CALL(clear_e200AD04);
}

/* register form: generated typed wrapper: loc_803F512's signature is confident (symbols/db loc_803F512.yaml, signature:) */
static inline void call_loc_803F512(void) {
    GUEST_CALL(loc_803F512);
}

typedef struct { u32 r0; u32 r1; bool z; } notZero_eByte200AD04_result;   /* z: the Z flag it leaves */
/* register form: generated typed wrapper: notZero_eByte200AD04's signature is confident (symbols/db notZero_eByte200AD04.yaml, signature:) */
static inline notZero_eByte200AD04_result call_notZero_eByte200AD04(void) {
    GUEST_CALL(notZero_eByte200AD04);
    return (notZero_eByte200AD04_result){ R0, R1, ZERO_FLAG() };
}

/* register form: generated typed wrapper: sub_803F6B0's signature is confident (symbols/db sub_803F6B0.yaml, signature:) */
static inline void call_sub_803F6B0(u32 table_index) {
    R0 = table_index;
    GUEST_CALL(sub_803F6B0);
}

/* register form: generated typed wrapper: sub_803F838's signature is confident (symbols/db sub_803F838.yaml, signature:); r7, r8, r9, r11, r12 pass through as the guest has them (no call site here sets them: the original passes its own caller's values on) */
static inline bool call_sub_803F838(void) {
    GUEST_CALL(sub_803F838);
    return ZERO_FLAG();   /* z set */
}

/* register form: generated typed wrapper: sub_803FA42's signature is confident (symbols/db sub_803FA42.yaml, signature:) */
static inline void call_sub_803FA42(void) {
    GUEST_CALL(sub_803FA42);
}

/* register form: generated typed wrapper: sub_813D90C's signature is confident (symbols/db sub_813D90C.yaml, signature:) */
static inline void call_sub_813D90C(u8 value) {
    R0 = value;
    GUEST_CALL(sub_813D90C);
}

/* register form: generated typed wrapper: zeroFill_813D960's signature is confident (symbols/db zeroFill_813D960.yaml, signature:) */
static inline void call_zeroFill_813D960(void) {
    GUEST_CALL(zeroFill_813D960);
}

/* register form: generated typed wrapper: sub_81440D8's signature is confident (symbols/db sub_81440D8.yaml, signature:) */
static inline void call_sub_81440D8(void) {
    GUEST_CALL(sub_81440D8);
}

/* register form: generated typed wrapper: rfu_REQ_stopMode's signature is confident (symbols/db rfu_REQ_stopMode.yaml, signature:); r6, r8, r9, r11, r12 pass through as the guest has them (no call site here sets them: the original passes its own caller's values on) */
static inline void call_rfu_REQ_stopMode(u32 arg) {
    R7 = arg;
    GUEST_CALL(rfu_REQ_stopMode);
}

/* Forward declarations for the subsystem's own functions. */
static void start_screen_render(StartScreen *unused);
static void start_screen_init_gfx(StartScreen *self);
static void start_screen_second_render(StartScreen *self);
static void start_screen_render_trigger(StartScreen *self);
static void start_screen_run_menu_state(StartScreen *self);
static void start_screen_menu_init(StartScreen *self);
static void start_screen_menu_wait_fade(StartScreen *self);
static void start_screen_menu_title_wait(StartScreen *self);
static void start_screen_menu_update_render(StartScreen *self);
static void start_screen_menu_update(StartScreen *self);
static void start_screen_load_game(StartScreen *self);
static void menu_on_start_press_begin_transition(StartScreen *self);
static void start_screen_menu_update_input(StartScreen *self);
static void start_screen_do_random_script(void);
static void start_screen_update_frame(StartScreen *self);
static void start_screen_submit_oam_set(u32 index);
static void start_screen_play_oam_seq(u32 index);
static void start_screen_load_title_blob(void);
static void start_screen_play_sound_if_flag80(StartScreen *self);
static void start_screen_fade_out_if_flag40(StartScreen *self);
static void start_screen_dispatch_if_flag20(StartScreen *self);
static void start_screen_play_sound_if_flag10(StartScreen *self);
static void start_screen_print_text_if_flag8(StartScreen *self);
static void start_screen_on_status_if_flag4(StartScreen *self);
static void start_screen_play_sfx_if_flag2(StartScreen *self);
static void start_screen_play_flagged_sfx_flag100(StartScreen *self);
static void start_screen_run_trigger_handler_flag200(StartScreen *self);
static void start_screen_init_gfx_base(void);
static void start_screen_copy_bg_tiles(void);
static void start_screen_set_num_choices(StartScreen *self);
static u32 start_screen_init_sprite_pool(u32 table_index);
static void start_screen_copy_obj_attributes_to_iwram(void);

typedef void (*RenderHandler)(StartScreen *);
typedef void (*MenuStateHandler)(StartScreen *);

static const RenderHandler jt_802F560[4] = {
    start_screen_init_gfx,
    start_screen_second_render,
    start_screen_run_menu_state,
    start_screen_load_game,
};

static const MenuStateHandler jt_802F654[5] = {
    start_screen_menu_init,
    start_screen_menu_wait_fade,
    start_screen_menu_title_wait,
    start_screen_menu_update_render,
    start_screen_menu_update,
};

/* startscreen_render_802F544 */
static void start_screen_render(StartScreen *unused) {
    (void)unused;
    call_startScreen_AnimatePressStart_803E938();

    StartScreen *self = G(StartScreen, A_eStartScreen);
    u32 index = self->JumpTableOff_00 / 4;
    if (index < 4) {
        jt_802F560[index](self);
    }
    (void)call_GetRNG();
}

/* startScreen_initGfx_802F574 */
static void start_screen_init_gfx(StartScreen *self) {
    call_toolkit_applyOverlayPalette_80015FC(0xb);
    call_SetRenderInfoLCDControl(0x1140);
    call_renderInfo_8001788();
    call_renderInfo_80017A0();
    start_screen_init_gfx_base();

    SetScreenFade_result fade = call_SetScreenFade(0xc, 0xff);
    (void)call_musicGameState_8000784((u8)fade.r0);
    call_LoadGFXAnims(A_pt_802F5F0);

    self->JumpTableOff_00 = 4;
    self->Unk_02 = 0;

    call_sub_803FA42();

    if (call_sub_803F838()) {
        self->Unk_02 = 1;
        U32Pair counts = call_GetTitleScreenIconCount();
        self->Unk_0C = (u8)counts.r0;
        self->AchievmentFlags = (u16)counts.r1;
    }

    u8 cursor, num_choices;
    if (self->Unk_02 == 0) {
        cursor = 0;
        num_choices = 1;
        if (!call_startScreen_TstZero()) {
            num_choices = 2;
        }
    } else {
        cursor = 1;
        num_choices = 2;
        if (!call_startScreen_TstZero()) {
            num_choices = 3;
        }
    }

    self->CurrCursorPos = cursor;
    self->NumChoices = num_choices;
    self->Unk_03 = 0;
}

/* startscreen_802F60C */
static void start_screen_second_render(StartScreen *self) {
    /* The only valid entry in this sub-dispatch table is index 0. */
    if (self->JumpTableOff_01 == 0) {
        start_screen_render_trigger(self);
    }
}

/* startscreen_render_trigger_802F624 */
static void start_screen_render_trigger(StartScreen *self) {
    call_SetRenderInfoLCDControl(0x1741);
    self->JumpTableOff_00 = 8;
    self->JumpTableOff_01 = 0;
}

/* startScreen_RunStateHandler_802F63C */
static void start_screen_run_menu_state(StartScreen *self) {
    u32 index = self->JumpTableOff_01 / 4;
    if (index < 5) {
        jt_802F654[index](self);
    }
    start_screen_load_title_blob();
}

/* startScreen_init_802F668 */
static void start_screen_menu_init(StartScreen *self) {
    self->Unk_0E = 0;
    self->Unk_0F = 0;
    self->LogoShowTimer = 0xa46;

    call_music_80005F2(1);
    call_toolkit_applyOverlayPalette_80015FC(0xa);
    call_SetRenderInfoLCDControl(0x1340);
    self->Unk_06 = 0;
    start_screen_copy_bg_tiles();
    (void)call_SetScreenFade(8, 0x10);
    self->JumpTableOff_01 = 4;
}

/* sub_802F6A4 */
static void start_screen_menu_wait_fade(StartScreen *self) {
    IsScreenFadeActive_result fade = call_IsScreenFadeActive();
    if (!fade.z) {
        self->JumpTableOff_01 = 8;
    }
}

/* sub_802F6B2 */
static void start_screen_menu_title_wait(StartScreen *self) {
    u16 timer = self->LogoShowTimer;
    if (timer <= 0) {
        self->JumpTableOff_01 = 0x10;
        (void)call_SetScreenFade(0xc, 0x10);
        return;
    }

    timer--;
    self->LogoShowTimer = timer;

    if (timer == 0x12c) {
        call_sound_dispatchCueHandler_800068A(0x1f);
        return;
    }

    if (timer < 0xa0a) {
        if (call_startScreen_IsTitleAnimationFinished_803E928()) {
            start_screen_set_num_choices(self);
            menu_on_start_press_begin_transition(self);
            start_screen_do_random_script();
        }
    }
}

/* startScreen_UpdateAndRenderFrame_802F704 */
static void start_screen_menu_update_render(StartScreen *self) {
    start_screen_menu_update_input(self);
    start_screen_update_frame(self);
}

/* startScreen_update_802F710 */
static void start_screen_menu_update(StartScreen *self) {
    if (self->LogoShowTimer > 0) {
        start_screen_update_frame(self);
    }

    IsScreenFadeActive_result fade = call_IsScreenFadeActive();
    if (fade.z) {
        return;
    }

    if (self->LogoShowTimer <= 0) {
        call_zeroFill_e20094C0();
        call_palette_clearEPalette20097a0_80023A8();
        (void)call_call_803D1AC();
        call_sub_81440D8();
        call_zeroFill_813D960();
        call_clear_e200AD04();
        call_sub_803F6B0(0);
        call_loc_803F512();
        call_init_eStartScreenAnimationControl200B1A0_1();
    } else {
        /* Original stores a halfword: 0x0C in JumpTableOff_00,
           0x00 in JumpTableOff_01. */
        self->JumpTableOff_00 = 0xc;
        self->JumpTableOff_01 = 0;
    }
}

/* load_game_802F756 */
static void start_screen_load_game(StartScreen *self) {
    u32 toolkit = A_eToolkit;
    u32 main_jt = LD32(toolkit + oToolkit_MainJumptableIndexPtr);
    ST8(main_jt, 4);

    call_SetRenderInfoLCDControl(0x1140);
    call_zeroFill_813D960();

    switch (self->CurrCursorPos) {
        case 0:
            call_resetOverworldScene_800260C();
            call_initNewGameData_8004DF0(toolkit);
            call_scenario_prepareEnterMap_8004D48();
            call_sub_81440D8();
            call_rfu_REQ_stopMode(toolkit);
            if (self->Unk_02 != 0) {
                (void)call_SetEventFlagFromImmediate(EVENT_1704 >> 8, EVENT_1704 & 0xFF);
            }
            break;

        case 1:
            if (self->Unk_02 != 0) {
                call_scenario_prepareEnterMap_8004D48();
                (void)call_SetEventFlagFromImmediate(EVENT_1704 >> 8, EVENT_1704 & 0xFF);
                call_sub_81440D8();
                call_rfu_REQ_stopMode(toolkit);
                break;
            }
            /* fall through when Unk_02 == 0 */

        default:
            call_sub_813D90C(0);
            call_scenario_prepareEnterMap_8004D48();
            (void)call_sub_8039570();
            call_clearStruct200BC30_803EBAC();
            call_eStruct200BC30_setJumpOffset00(0xc);
            if (self->Unk_02 != 0) {
                (void)call_SetEventFlagFromImmediate(EVENT_1704 >> 8, EVENT_1704 & 0xFF);
            }
            call_sub_81440D8();
            break;
    }
}

/* menu_onStartPressBeginTransition_802F7E8 */
static void menu_on_start_press_begin_transition(StartScreen *self) {
    u32 joypad = LD32(A_eToolkit + oToolkit_JoypadPtr);
    u16 held_start = LD16(joypad + 2);

    if (held_start & 8) {
        call_zeroFill_813D960();
        self->JumpTableOff_01 = 0xc;
        (void)call_PlaySoundEffect(SOUND_SELECT_67);
        call_LoadGFXAnim(G(const u8, A_byte_802F2E4));
        call_LoadGFXAnim(G(const u8, A_dword_802F334));
    }
}

/* sub_802F81C */
static void start_screen_menu_update_input(StartScreen *self) {
    u32 joypad = LD32(A_eToolkit + oToolkit_JoypadPtr);
    u16 pressed = LD16(joypad + oJoypad_Pressed);

    if (pressed & 9) {
        IsScreenFadeActive_result fade = call_IsScreenFadeActive();
        if (fade.z) {
            return;
        }
        (void)call_musicGameState_8000784((u8)fade.r0);
        (void)call_PlaySoundEffect(0x9d);
        self->JumpTableOff_01 = 0x10;
        (void)call_SetScreenFade(0xc, 0x10);
        return;
    }

    u16 move_buttons = LD16(joypad + 4);

    if (move_buttons & 0x40) {
        u8 old = self->CurrCursorPos;
        u8 cur;
        if (old == 0) {
            cur = self->NumChoices - 1;
        } else {
            cur = old - 1;
        }
        self->CurrCursorPos = cur;
        if (cur != old) {
            (void)call_PlaySoundEffect(SOUND_UNK_66);
        }
        return;
    }

    if (move_buttons & 0x80) {
        u8 old = self->CurrCursorPos;
        u8 cur = old + 1;
        if (cur >= self->NumChoices) {
            cur = 0;
        }
        self->CurrCursorPos = cur;
        if (cur != old) {
            (void)call_PlaySoundEffect(SOUND_UNK_66);
        }
        return;
    }
}

/* sub_802F88E */
static void start_screen_do_random_script(void) {
    u32 frame_ptr = LD32(A_eToolkit + oToolkit_CurFramePtr);
    u16 frame = LD16(frame_ptr);
    if ((frame & 0x3f) <= 0x20) {
        call_scriptListRun_8001158(G(const u8, A_byte_802F8AC));
    }
}

/* sub_802F8D8 */
static void start_screen_update_frame(StartScreen *self) {
    u8 v = self->Unk_06 + 1;
    if (v >= 0x12) {
        v = 0;
    }
    self->Unk_06 = v;

    v = self->Unk_07 + 1;
    if (v >= 0x40) {
        v = 0;
    }
    self->Unk_07 = v;

    u16 y_offset = LD16(A_byte_802F970 + (self->Unk_06 * 2u));
    u32 param = LD32(A_byte_802F960 + (self->CurrCursorPos * 4u));
    (void)call_sub_802FE28(param, y_offset, 0, 0);

    start_screen_submit_oam_set(self->CurrCursorPos);

    if (self->Unk_02 == 0) {
        notZero_eByte200AD04_result not_zero = call_notZero_eByte200AD04();
        if (not_zero.z) {
            (void)call_startScreen_TstZero();
        }
    } else {
        start_screen_play_oam_seq(self->CurrCursorPos);
        notZero_eByte200AD04_result not_zero = call_notZero_eByte200AD04();
        if (not_zero.z) {
            (void)call_startScreen_TstZero();
        }
    }

    start_screen_run_trigger_handler_flag200(self);
    start_screen_play_flagged_sfx_flag100(self);
    start_screen_play_sound_if_flag80(self);
    start_screen_fade_out_if_flag40(self);
    start_screen_dispatch_if_flag20(self);
    start_screen_play_sound_if_flag10(self);
    start_screen_print_text_if_flag8(self);
    start_screen_on_status_if_flag4(self);
    start_screen_play_sfx_if_flag2(self);
}

/* sprite_submitOamSet_802F994 */
static void start_screen_submit_oam_set(u32 index) {
    u32 script_addr = LD32(A_off_802F9A8 + (index * 4u));
    call_scriptListRun_8001158(G(const u8, script_addr));
}

/* playOamSeq_802F9EC */
static void start_screen_play_oam_seq(u32 index) {
    u32 script_addr = LD32(A_off_802FA00 + (index * 4u));
    call_scriptListRun_8001158(G(const u8, script_addr));
}

/* loadTitleBlob_802FB10 */
static void start_screen_load_title_blob(void) {
    call_scriptListRun_8001158(G(const u8, A_byte_802FB20));
}

/* startScreen_playSoundIfFlagSet_802FB64 */
static void start_screen_play_sound_if_flag80(StartScreen *self) {
    if (self->AchievmentFlags & 0x80) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FB7C));
    }
}

/* startScreen_fadeOutUpdateIfFlag40_802FB90 */
static void start_screen_fade_out_if_flag40(StartScreen *self) {
    if (self->AchievmentFlags & 0x40) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FBA8));
    }
}

/* startScreen_dispatchIfFlag20_802FBB4 */
static void start_screen_dispatch_if_flag20(StartScreen *self) {
    if (self->AchievmentFlags & 0x20) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FBCC));
    }
}

/* startScreen_playSoundIfFlagSet_802FBD8 */
static void start_screen_play_sound_if_flag10(StartScreen *self) {
    if (self->AchievmentFlags & 0x10) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FBF0));
    }
}

/* startScreen_printTextIfFlag3_802FBFC */
static void start_screen_print_text_if_flag8(StartScreen *self) {
    if (self->AchievmentFlags & 0x8) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FC14));
    }
}

/* startScreen_onStatusBit2_802FC28 */
static void start_screen_on_status_if_flag4(StartScreen *self) {
    if (self->AchievmentFlags & 0x4) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FC40));
    }
}

/* startScreen_playSfxIfFlagSet_802FC4C */
static void start_screen_play_sfx_if_flag2(StartScreen *self) {
    if (self->AchievmentFlags & 0x2) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FC64));
    }
}

/* startScreen_playFlaggedSfx_802FC70 */
static void start_screen_play_flagged_sfx_flag100(StartScreen *self) {
    if (self->AchievmentFlags & 0x100) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FC88));
    }
}

/* startScreen_runTriggerHandler_802FC9C */
static void start_screen_run_trigger_handler_flag200(StartScreen *self) {
    if (self->AchievmentFlags & 0x200) {
        call_scriptListRun_8001158(G(const u8, A_byte_802FCB4));
    }
}

/* startScreen_initGfx_802FCC0 */
static void start_screen_init_gfx_base(void) {
    call_zeroFillVRAM();
    call_decompAndCopyData(A_initRefs_802FCD8);
    call_ZeroFillGFX30025c0();
}

/* copyBGTiles_802FD3C */
static void start_screen_copy_bg_tiles(void) {
    call_CopyBackgroundTiles(0, 0, 1, A_byte_87F8EB0, 0x20, 0x14);
}

/* sub_802FD54 */
static void start_screen_set_num_choices(StartScreen *self) {
    if (call_startScreen_TstZero()) {
        return;
    }

    u8 num = 2;
    if (self->Unk_02 != 0) {
        num = 3;
    }
    self->NumChoices = num;
}

/* sub_802FDB0 / loc_802FDB4 */
static u32 start_screen_init_sprite_pool(u32 table_index) {
    u32 src = LD32(A_off_802FDF4 + table_index);

    call_CopyWords(src, A_iSpriteLayerLists, 0x20);

    for (u32 i = 0; i < 4; i++) {
        u32 word0 = LD32(src + (i * 8u));
        u32 word1 = LD32(src + (i * 8u) + 4u);
        call_WordFill(word0, G(u32, word1), 0xFFFFFFFFu);
    }

    call_ZeroFillByEightWords(A_iSpritePool, 0x400);
    ST32(A_iSpritePoolUsed, 0);

    return src + 0x20;
}

/* copyObjAttributesToIWRAM_802FE0C */
static void start_screen_copy_obj_attributes_to_iwram(void) {
    call_CopyByEightWords(A_iOamShadow, 0x7000000, 0x400);
}

/* Entry point adapters. */

Next startscreen_render_802F544(void) {
    u32 r4 = R4, r5 = R5, r6 = R6, r7 = R7;
    start_screen_render(0);
    R4 = r4;
    R5 = r5;
    R6 = r6;
    R7 = r7;
    RETURN_TO_CALLER();
}

Next sub_802FDB0(void) {
    u32 r4 = R4;
    u32 r3 = start_screen_init_sprite_pool(0);
    R3 = r3;
    R4 = r4;
    RETURN_TO_CALLER();
}

Next loc_802FDB4(void) {
    u32 r4 = R4;
    u32 r3 = start_screen_init_sprite_pool(4);
    R3 = r3;
    R4 = r4;
    RETURN_TO_CALLER();
}

Next copyObjAttributesToIWRAM_802FE0C(void) {
    start_screen_copy_obj_attributes_to_iwram();
    RETURN_TO_CALLER();
}
