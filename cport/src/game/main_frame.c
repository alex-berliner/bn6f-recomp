/*
 * main_frame.c — frame pacing of the main loop (main_awaitFrame in bn6f
 * main.s).
 *
 * The VBlank handler counts frames in gameFrameCounter; the main loop runs one
 * game step every dword_2009930 frames. This waits for the next VBlank at or
 * after that point and resets the counter.
 *
 * C-port addition: how long the game step took (in scanlines, a frame is
 * 228) is accumulated in g_frame_stats so tools/speed.sh can measure CPU load.
 */
#include "game.h"

#define REG_DISPSTAT   0x04000004u
#define REG_VCOUNT     0x04000006u
#define VBLANK_COUNT   0x0200A870u   /* gameFrameCounter, incremented by the VBlank IRQ */
#define FRAMES_PER_STEP 0x02009930u  /* dword_2009930 */

/* [0] game steps, [1] total busy scanlines, [2] steps that missed a frame */
u32 g_frame_stats[3];

Next main_awaitFrame(void) {
    u32 waited = LD32(VBLANK_COUNT);
    u32 line = LD16(REG_VCOUNT);
    /* Scanlines since the VBlank that ended the previous wait. */
    u32 busy = waited * 228 + (line >= 160 ? line - 160 : line + 68);
    g_frame_stats[0]++;
    g_frame_stats[1] += busy;
    g_frame_stats[2] += busy >= 228;

    u32 frames, step;
#if defined(BOOT_DIAG)
    unsigned stuck = 0;    /* VBlank periods seen without the IRQ counting one */
#endif
    do {
        while (!(LD16(REG_DISPSTAT) & 1))
            ;
        frames = LD32(VBLANK_COUNT);
        step = LD32(FRAMES_PER_STEP);
#if defined(BOOT_DIAG)
        if ((s32)frames < (s32)step) {
            while (LD16(REG_DISPSTAT) & 1)
                ;
            if (LD32(VBLANK_COUNT) == frames && ++stuck > 120) {
                void diag_color(u16 bgr, unsigned frames);
                for (;;)
                    diag_color(0x021F, 1);        /* orange: VBlank IRQs are not arriving */
            }
        }
#endif
    } while ((s32)frames < (s32)step);
    ST32(VBLANK_COUNT, 0);

    /* registers as the original leaves them */
    R0 = VBLANK_COUNT;
    R1 = 0;
    R2 = frames;
    RETURN_TO_CALLER();
}
