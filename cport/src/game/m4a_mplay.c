/*
 * m4a_mplay.c — m4a ("Sappy") music player sequencer, MPlayMain
 * (MPlayMain in bn6f libs.s).
 *
 * Once per frame SoundMain calls MPlayMain for the head of the music player
 * chain. Each player first locks itself (ident++), then lets the next player
 * in the chain run, and then advances its own song: for every tempo tick it
 * counts down note gate times, interprets each track's command stream (notes,
 * waits and the MPlayJumpTable commands) and steps the LFO; finally it pushes
 * changed volume and pitch to the track's sound channels.
 *
 * The original recurses once per chained player (MPlayMainNext). So does
 * run_chain, but only the locking recurses (a few words of stack per
 * player); each player's frame work runs in play(), one at a time.
 */
#include "game.h"

/* ── layouts (offsets as used by this game's m4a build) ─────────────── */
enum {  /* MusicPlayerInfo */
    MP_STATUS = 0x04, MP_TRACK_COUNT = 0x08, MP_CMD = 0x0A, MP_CLOCK = 0x0C,
    MP_TEMPO_I = 0x20, MP_TEMPO_C = 0x22, MP_TRACKS = 0x2C,
    MP_IDENT = 0x34, MP_NEXT = 0x38, MP_NEXT_ARG = 0x3C,
};
enum {  /* MusicPlayerTrack */
    TR_FLAGS = 0x00, TR_WAIT = 0x01, TR_RUNNING_STATUS = 0x07, TR_KEY_SHIFT = 0x08,
    TR_PITCH = 0x09, TR_BEND_RANGE = 0x0F, TR_VOL_X = 0x13, TR_MOD_M = 0x16, TR_MOD = 0x17,
    TR_MOD_T = 0x18, TR_LFO_SPEED = 0x19, TR_LFO_SPEED_C = 0x1A, TR_LFO_DELAY_C = 0x1C,
    TR_CHAN = 0x20, TR_TONE_TYPE = 0x24, TR_CMD_PTR = 0x40,
    TR_SIZE = 0x50,
};
enum {  /* SoundChannel */
    CH_STATUS = 0x00, CH_TYPE = 0x01, CH_KEY = 0x08, CH_GATE_TIME = 0x10, CH_MODIFY = 0x1D,
    CH_FREQ = 0x20, CH_WAVE = 0x24, CH_NEXT = 0x34,
};
enum {  /* SoundInfo */
    SI_CGB_FREQ = 0x30,     /* MidiKeyToCgbFreq */
    SI_JUMP_TABLE = 0x34,   /* MPlayJumpTable: commands 0xB1..0xCE */
    SI_PLY_NOTE = 0x38,     /* ply_note: commands 0xCF..0xFF */
};

/* track flags */
enum { TRK_VOL = 0x03, TRK_PITCH = 0x0C, TRK_START = 0x40, TRK_EXIST = 0x80 };
/* channel status */
enum { CHN_ON = 0xC7, CHN_STOP = 0x40 };
/* channel modify flags (CGB) */
enum { CGB_VOL_CHANGED = 1, CGB_FREQ_CHANGED = 2 };

#define ID_NUMBER   0x68736D53u     /* 'Smsh' */
#define SOUND_INFO  0x03007FF0u     /* SOUND_INFO_PTR */
#define CLOCK_TABLE 0x081C068Cu     /* gClockTable: wait lengths of commands 0x80..0xB0 */
#define MPLAY_MAIN  0x0814E260u
#define TICK        150             /* tempo units per sequencer tick */
#define MAX_CHAIN   64

#define U8(a)  (*(u8 *)(uintptr_t)(u32)(a))
#define S8(a)  (*(s8 *)(uintptr_t)(u32)(a))
#define U16(a) (*(u16 *)(uintptr_t)(u32)(a))
#define U32(a) (*(u32 *)(uintptr_t)(u32)(a))

/* Lifted m4a helpers (bn6f names in comments). */
#define FadeOutBody     sub_814F228
#define ClearChain      m4a_ply_trampoline_814ECA0
#define Clear64byte     sub_814ECB4
#define TrkVolPitSet    sub_814F2F0
#define ChnVolSetAsm    sprite_crossfadeColorChannels_814E528
#define MidiKeyToFreq   sub_814E7F4

static void call1(GuestFn fn, u32 a0) { R0 = a0; GUEST_CALL(fn); }

/* Follow a channel list link; a channel linked to itself ends the list. */
static u32 next_channel(u32 ch) {
    u32 next = U32(ch + CH_NEXT);
    if (next == ch) {
        next = 0;
        U32(ch + CH_NEXT) = 0;
    }
    return next;
}

/* Count down the gate time of each sounding note; free stopped channels. */
static void update_gate_times(u32 trk) {
    for (u32 ch = U32(trk + TR_CHAN); ch; ch = next_channel(ch)) {
        u32 status = U8(ch + CH_STATUS);
        if (!(status & CHN_ON)) {
            call1(ClearChain, ch);
            continue;
        }
        u32 gate = U8(ch + CH_GATE_TIME);
        if (gate) {
            U8(ch + CH_GATE_TIME) = (u8)--gate;
            if (gate == 0)
                U8(ch + CH_STATUS) = (u8)(status | CHN_STOP);
        }
    }
}

/* Run the track's commands until one sets a wait. Returns 0 if a command
 * stopped the track (its flags became 0). */
static int run_commands(u32 si, u32 mp, u32 trk) {
    while (U8(trk + TR_WAIT) == 0) {
        u32 p = U32(trk + TR_CMD_PTR);
        u32 cmd = U8(p);
        if (cmd < 0x80) {
            cmd = U8(trk + TR_RUNNING_STATUS);      /* running status: repeat the last note/command */
        } else {
            U32(trk + TR_CMD_PTR) = p + 1;
            if (cmd >= 0xBD)
                U8(trk + TR_RUNNING_STATUS) = (u8)cmd;
        }
        if (cmd >= 0xCF) {                          /* note */
            R0 = cmd - 0xCF; R1 = mp; R2 = trk;
            GUEST_CALL_ADDR(U32(si + SI_PLY_NOTE));
        } else if (cmd > 0xB0) {                    /* MPlayJumpTable command */
            U8(mp + MP_CMD) = (u8)(cmd - 0xB1);
            R0 = mp; R1 = trk;
            GUEST_CALL_ADDR(U32(U32(si + SI_JUMP_TABLE) + (cmd - 0xB1) * 4));
            if (U8(trk + TR_FLAGS) == 0)
                return 0;
        } else {                                    /* wait */
            U8(trk + TR_WAIT) = U8(CLOCK_TABLE + cmd - 0x80);
        }
    }
    return 1;
}

/* Step the track's LFO (vibrato/tremolo/pan modulation). */
static void step_lfo(u32 trk) {
    u32 speed = U8(trk + TR_LFO_SPEED);
    if (speed == 0 || U8(trk + TR_MOD) == 0)
        return;
    if (U8(trk + TR_LFO_DELAY_C)) {
        U8(trk + TR_LFO_DELAY_C)--;
        return;
    }
    /* Triangle wave from the phase counter. The phase sum is used unwrapped,
     * as the original does. */
    u32 phase = U8(trk + TR_LFO_SPEED_C) + speed;
    U8(trk + TR_LFO_SPEED_C) = (u8)phase;
    s32 wave = ((phase - 0x40) & 0x80) ? (s8)phase : (s32)(0x80 - phase);
    s32 mod = (s32)(U8(trk + TR_MOD) * (u32)wave) >> 6;
    if ((u8)(U8(trk + TR_MOD_M) ^ mod) == 0)
        return;
    U8(trk + TR_MOD_M) = (u8)mod;
    U8(trk + TR_FLAGS) |= U8(trk + TR_MOD_T) == 0 ? TRK_PITCH : TRK_VOL;
}

/* One sequencer tick for every track. Returns the bit mask of live tracks. */
static u32 tick(u32 si, u32 mp) {
    u32 live = 0;
    u32 trk = U32(mp + MP_TRACKS);
    s32 n = U8(mp + MP_TRACK_COUNT);
    for (u32 bit = 1;; bit <<= 1, trk += TR_SIZE) {
        if (U8(trk + TR_FLAGS) & TRK_EXIST) {
            live |= bit;
            update_gate_times(trk);
            if (U8(trk + TR_FLAGS) & TRK_START) {
                call1(Clear64byte, trk);
                U8(trk + TR_FLAGS) = TRK_EXIST;
                U8(trk + TR_BEND_RANGE) = 2;
                U8(trk + TR_VOL_X) = 0x40;
                U8(trk + TR_LFO_SPEED) = 0x16;
                U8(trk + TR_TONE_TYPE) = 1;
            }
            if (run_commands(si, mp, trk)) {
                U8(trk + TR_WAIT)--;
                step_lfo(trk);
            }
        }
        if (--n <= 0)
            break;
    }
    return live;
}

/* Push changed volume/pitch of each track to its channels. */
static void update_channels(u32 si, u32 mp) {
    u32 trk = U32(mp + MP_TRACKS);
    s32 n = U8(mp + MP_TRACK_COUNT);
    u32 r4 = R4, r5 = R5;
    for (;; trk += TR_SIZE) {
        u32 flags = U8(trk + TR_FLAGS);
        if ((flags & TRK_EXIST) && (flags & (TRK_VOL | TRK_PITCH))) {
            R0 = mp; R1 = trk;
            GUEST_CALL(TrkVolPitSet);
            for (u32 ch = U32(trk + TR_CHAN); ch; ch = next_channel(ch)) {
                if (!(U8(ch + CH_STATUS) & CHN_ON)) {
                    call1(ClearChain, ch);
                    continue;
                }
                u32 cgb = U8(ch + CH_TYPE) & 7;     /* 0: DirectSound, 1-4: CGB channel */
                if (U8(trk + TR_FLAGS) & TRK_VOL) {
                    R4 = ch; R5 = trk;              /* register arguments */
                    GUEST_CALL(ChnVolSetAsm);
                    R4 = r4; R5 = r5;
                    if (cgb)
                        U8(ch + CH_MODIFY) |= CGB_VOL_CHANGED;
                }
                if (U8(trk + TR_FLAGS) & TRK_PITCH) {
                    s32 key = U8(ch + CH_KEY) + S8(trk + TR_KEY_SHIFT);
                    if (key < 0)
                        key = 0;
                    if (cgb) {
                        R0 = cgb; R1 = (u32)key; R2 = U8(trk + TR_PITCH);
                        GUEST_CALL_ADDR(U32(si + SI_CGB_FREQ));
                        U32(ch + CH_FREQ) = R0;
                        U8(ch + CH_MODIFY) |= CGB_FREQ_CHANGED;
                    } else {
                        R0 = U32(ch + CH_WAVE); R1 = (u32)key; R2 = U8(trk + TR_PITCH);
                        GUEST_CALL(MidiKeyToFreq);
                        U32(ch + CH_FREQ) = R0;
                    }
                }
            }
            U8(trk + TR_FLAGS) &= 0xF0;
        }
        if (--n <= 0)
            break;
    }
}

/* Advance one player by a frame (the part after the chain call). */
__attribute__((noinline)) static void play(u32 mp) {
    if ((s32)U32(mp + MP_STATUS) >= 0) {
        u32 si = U32(SOUND_INFO);
        call1(FadeOutBody, mp);
        if ((s32)U32(mp + MP_STATUS) >= 0) {
            /* tempoC accumulates tempoI per frame; every TICK units is a tick */
            u32 t = U16(mp + MP_TEMPO_C) + U16(mp + MP_TEMPO_I);
            for (;;) {
                U16(mp + MP_TEMPO_C) = (u16)t;
                if (t < TICK)
                    break;
                u32 live = tick(si, mp);
                U32(mp + MP_CLOCK)++;
                if (!live) {
                    U32(mp + MP_STATUS) = 0x80000000u;      /* song ended */
                    goto unlock;
                }
                U32(mp + MP_STATUS) = live;
                t = U16(mp + MP_TEMPO_C) - TICK;
            }
            update_channels(si, mp);
        }
    }
unlock:
    U32(mp + MP_IDENT) = ID_NUMBER;
}

/* MPlayMain for one player: lock it (a player that is already locked, e.g.
 * being set up by the game, ends the chain), let the next player in the
 * chain run, then advance this one. */
static void run_chain(u32 mp, int depth) {
    if (U32(mp + MP_IDENT) != ID_NUMBER)
        return;
    U32(mp + MP_IDENT) = ID_NUMBER + 1;
    u32 next = U32(mp + MP_NEXT);
    if (next) {
        u32 arg = U32(mp + MP_NEXT_ARG);
        if ((next & ~1u) == MPLAY_MAIN && depth < MAX_CHAIN) {
            run_chain(arg, depth + 1);
        } else {
            R0 = arg;                               /* some other chained routine */
            GUEST_CALL_ADDR(next);
        }
    }
    play(mp);
}

Next MPlayMain(void) {
    run_chain(R0, 0);
    RETURN_TO_CALLER();
}
