/*
 * m4a_init.c — m4aSoundInit (m4aSoundInit in bn6f libs.s).
 *
 * Initialises the m4a sound engine: SoundInit, the CGB channel driver,
 * SoundMode, then opens the 0x20 music players described by gMPlayTable
 * (gMPlayTable).
 *
 * C-port difference: the original first CpuSets SoundMainRAM into IWRAM at
 * 0x3005700 so it can run from fast memory. Here that address dispatches to
 * the readable mixer (m4a_mixer.c), so the copy is skipped and the 1 KB is
 * used for the port's own IWRAM code.
 */
#include "game.h"

#define SOUND_INFO      0x030045C0u   /* iSoundInfo: gSoundInfo */
#define CGB_CHANNELS    0x03005600u   /* iCgbChannels */
#define SOUND_MODE      0x0093F400u   /* reverb/volume/channels/frequency */
#define MPLAY_TABLE     0x08158278u   /* gMPlayTable: {info, tracks, numTracks, pad, unk} */
#define MPLAY_COUNT     0x20
#define MEMACC_DEFAULT  0x02010B90u   /* byte_2010B90 */

Next m4aSoundInit(void) {
    u32 ret = LR;
    R0 = SOUND_INFO;   GUEST_CALL(sub_814ECC8);    /* SoundInit */
    R0 = CGB_CHANNELS; GUEST_CALL(sub_814EB84);    /* CGB channel init */
    R0 = SOUND_MODE;   GUEST_CALL(sub_814EE2C);    /* SoundMode */
    for (u32 i = 0; i < MPLAY_COUNT; i++) {
        u32 entry = MPLAY_TABLE + i * 12;
        u32 player = LD32(entry);
        R0 = player;
        R1 = LD32(entry + 4);                      /* tracks */
        R2 = LD8(entry + 8);                       /* number of tracks */
        GUEST_CALL(sub_814F08C);                   /* MPlayOpen */
        ST8(player + 0x0B, LD16(entry + 0x0A));    /* player->priority-ish field */
        ST32(player + 0x18, MEMACC_DEFAULT);
    }
    R0 = ret;                                      /* as the original's `pop {r0}; bx r0` */
    RETURN_TO(ret);
}
