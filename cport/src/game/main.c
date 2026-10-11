/*
 * main.c: readable functions of bn6f main.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── main_pollGeneralLCDStatus_STAT_LYC_ ── */
/* main_pollGeneralLCDStatus_STAT_LYC_: wait until the LYC bit of the general
 * LCD status register (GeneralLCDStatus_STAT_LYC_) reads as 0, i.e. the
 * display is no longer in the vertical blank/coincidence state being polled.
 * Clobbers r0-r2; returns to caller when the bit clears. */
Next main_pollGeneralLCDStatus_STAT_LYC_(void) {
    u32 *status = (u32 *)A_GeneralLCDStatus_STAT_LYC_;
    while ((u16)LD16((u32)status) & 1) {
        /* spin until STAT bit 0 (vblank flag) is clear */
    }
    RETURN_TO_CALLER();
}

/* ── main_updateJoypad_80003E4 ── */
/* main_updateJoypad_80003E4: main-loop joypad update.
 *
 * Refreshes the toolkit's joypad struct from the hardware key state:
 *   +0x00  keys pressed this frame (active high, computed from ~KeyStatus)
 *   +0x02  keys newly pressed this frame (down now but not last frame)
 *   +0x04  keys that register a press from holding: the frame right after
 *          a key starts being held, and repeat ticks once held 16+ frames
 *   +0x06  previous frame's pressed keys
 *   +0x08..0x11  per-key hold counters, one byte per KEYINPUT bit
 *   +0x13  repeat pacing timer, cycling 1,2,3,4,0 (a repeat fires when 0)
 *
 * Leaves r2/r4/r5/r6/r7 set exactly as the original does; its callers read
 * them even though they are not arguments of this call.
 */
Next main_updateJoypad_80003E4(void) {
    u32 joypad = LD32(R10 + 4);

    /* Advance the repeat pacing timer. */
    u32 repeatTimer = LD8(joypad + oJoypad_LowSensitivityTimer) + 1;
    if (repeatTimer > 4)
        repeatTimer = 0;
    ST8(joypad + oJoypad_LowSensitivityTimer, repeatTimer);

    u32 keysNow  = ~LD16(A_KeyStatus);   /* active-high view of the hardware keys */
    u32 keysPrev = LD16(joypad + 0x00);  /* keys that were down last frame */
    ST16(joypad + 0x06, keysPrev);       /* save as "previous" for next frame */
    ST16(joypad + 0x00, keysNow);        /* store new pressed-key state */

    /* Keys down both this frame and last frame go through hold/repeat logic.
     * (The original loads a 0x3FF key mask here but never uses it.) */
    u32 heldKeys = keysNow & keysPrev;

    for (u32 i = 0; i < 10; i++) {
        u32 keyBit = 1u << i;
        u32 counterAddr = joypad + 0x08 + i;

        if (heldKeys & keyBit) {
            u32 count = LD8(counterAddr);
            if (count < 0x10) {
                count++;
                ST8(counterAddr, count);
                if (count != 1) {
                    /* Held past the second frame: suppress until repeat. */
                    heldKeys &= ~keyBit;
                    R2 = ~keyBit;
                } else {
                    /* Second frame of a new press: key still registers. */
                    R2 = 1;
                }
            } else if (repeatTimer != 0) {
                /* Long hold, between repeat ticks: no press this frame. */
                heldKeys &= ~keyBit;
                R2 = ~keyBit;
            } else {
                /* Repeat tick: key fires again, stays in the held set. */
                R2 = count;
            }
        } else {
            /* Key not held: reset its hold counter. */
            ST8(counterAddr, 0);
            R2 = 0;
        }
    }

    ST16(joypad + 0x04, heldKeys);      /* keys that register this frame */
    R4 = keysNow & ~keysPrev;           /* newly pressed keys */
    ST16(joypad + 0x02, R4);
    R5 = ~keysPrev;
    R6 = heldKeys;
    R7 = repeatTimer;

    RETURN_TO_CALLER();
}

/* ── main_static_screen_fade_8000454 ── */
/* Soft-reset / static-screen-fade handler constants. */
#define KEY_MASK_LOW_NIBBLE 0xF   /* A+B+Select+Start live in the low nibble of the key word */
#define MAIN_STATE_STATIC_SCREEN 0x10
#define RESET_SUBINDEX 0xA        /* subindex written after a successful soft-reset combo */

struct fade_check {
    bool active;   /* true when a screen fade is currently running */
    u32  r3;       /* r3 as IsScreenFadeActive leaves it: every no-call path keeps this value */
};

/* register form: calls IsScreenFadeActive outside this file, which has no typed C interface yet.
 * Answers in the Z flag (set = no fade). It also clobbers r3, and the original keeps that
 * leftover as its own final r3 on every path that makes no further calls. */
static struct fade_check screen_fade_active(void) {
    GUEST_CALL(IsScreenFadeActive);
    struct fade_check r;
    r.active = !ZERO_FLAG();
    r.r3 = R3;
    return r;
}

/* register form: calls test0x200bc50_0x5_813D60C outside this file, which has no typed C interface yet */
static bool reset_block_flag_set(void) {
    GUEST_CALL(test0x200bc50_0x5_813D60C);
    return !ZERO_FLAG();          /* nonzero flag blocks this handler */
}

/* register form: calls start_800023C outside this file, which has no typed C interface yet.
 * The original calls it with r0 = held keys (low nibble), r1 = subindex address,
 * r2 = pressed keys (low nibble), r3 = 0xF, r4 = 0, r7 = toolkit; those call-site
 * register values only exist in the guest registers, so they are set here. */
static void call_start_800023C(u32 held_low, u32 counter_addr, u32 pressed_low, u32 toolkit) {
    R0 = held_low;
    R1 = counter_addr;
    R2 = pressed_low;
    R3 = KEY_MASK_LOW_NIBBLE;
    R4 = 0;
    R7 = toolkit;
    GUEST_CALL(start_800023C);
}

/* register form: calls main_initToolkitAndOtherSubsystems outside this file, which has no typed C interface yet */
static void call_main_init_toolkit(u32 held_low, u32 counter_addr, u32 pressed_low) {
    R0 = held_low;
    R1 = counter_addr;
    R2 = pressed_low;
    GUEST_CALL(main_initToolkitAndOtherSubsystems);
}

/* register form: calls clear_e200AD04 outside this file, which has no typed C interface yet.
 * Also reports the r3 the callee chain leaves behind: the original's final r3 on the
 * reboot path is exactly that leftover (clear_e200AD04 preserves r3). */
static u32 call_clear_e200AD04(void) {
    GUEST_CALL(clear_e200AD04);
    return R3;
}

/*
 * Main-loop handler for the static-screen fade state (r10 = toolkit pointer).
 *
 * Does nothing unless a screen fade is active and the blocking flag at 0x200BC50
 * bit 5 is clear, and the main jumptable is not in state 0x10. Otherwise it counts
 * down the byte at jumptable_index_ptr + 4:
 *   - while the count is still positive, store the decremented count and wait;
 *   - once it expires, watch for the soft-reset key combination (all four low
 *     nibble keys held, at least one of them newly pressed). While it is not
 *     held, restart the wait at 0.
 * When the combination is pressed, reboot the game: run the three init calls and
 * store subindex 0xA into the subindex byte.
 *
 * Returns the final value the original leaves in r3 (0xF on the two "combo not
 * held yet" paths, the callees' leftover on the reboot path, IsScreenFadeActive's
 * leftover everywhere else).
 */
static u32 run_static_screen_fade(u32 toolkit) {
    struct fade_check fade = screen_fade_active();
    if (!fade.active)
        return fade.r3;

    if (reset_block_flag_set())
        return fade.r3;

    u32 jumptable_index_ptr = LD32(toolkit + oToolkit_MainJumptableIndexPtr);

    /* Main jumptable state 0x10 is handled elsewhere. */
    if (LD8(jumptable_index_ptr) == MAIN_STATE_STATIC_SCREEN)
        return fade.r3;

    u32 joypad = LD32(toolkit + oToolkit_JoypadPtr);
    u32 held    = LD16(joypad + oJoypad_Held);
    u32 pressed = LD16(joypad + oJoypad_Pressed);

    u32 counter_addr = jumptable_index_ptr + 4;
    s32 count = (s32)LD8(counter_addr) - 1;

    if (count > 0) {
        ST8(counter_addr, (u32)count);          /* still waiting: keep counting down */
        return fade.r3;
    }

    /* Soft-reset combo not (yet) held: restart the countdown at 0. */
    if ((held & KEY_MASK_LOW_NIBBLE) != KEY_MASK_LOW_NIBBLE
            || (pressed & KEY_MASK_LOW_NIBBLE) == 0) {
        ST8(counter_addr, 0);
        return KEY_MASK_LOW_NIBBLE;
    }

    /* Reset combination detected: reboot into a fresh toolkit. */
    call_start_800023C(held & KEY_MASK_LOW_NIBBLE, counter_addr,
                       pressed & KEY_MASK_LOW_NIBBLE, toolkit);
    call_main_init_toolkit(held & KEY_MASK_LOW_NIBBLE, counter_addr,
                           pressed & KEY_MASK_LOW_NIBBLE);
    return call_clear_e200AD04();
    /* caller stores RESET_SUBINDEX via the return path below */
}

/* main_static_screen_fade_8000454: 08000454-080004A4 THUMB, bn6f main.s:235
 *   inputs:  r4 r5 r6 r7 r8 r9 r10 r11 lr (only r10 is read: the toolkit pointer)
 *   outputs: r3 r4 r5 r6 r7 r8 r9 r10 r11 r12
 *   clobbers: nz c v r0 r1 r2 lr
 * The original saves r4-r7 on entry and pops them back, so they always leave as
 * they came in even though the reboot-path callees trash r4/r7; likewise its
 * final r3 is either 0xF or a callee leftover, which run_static_screen_fade reports. */
Next main_static_screen_fade_8000454(void) {
    u32 r4_in = R4;
    u32 r7_in = R7;
    u32 r3_out = run_static_screen_fade(R10);
    R3 = r3_out;
    R4 = r4_in;
    R7 = r7_in;
    RETURN_TO_CALLER();
}
