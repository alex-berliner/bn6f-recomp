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
