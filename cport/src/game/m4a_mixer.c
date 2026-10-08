/*
 * m4a_mixer.c — the m4a ("Sappy") PCM mixer, SoundMainRAM.
 *
 * Readable replacement for the code m4a's SoundInit copies to IWRAM
 * (0x3005700, from SoundMainRAM in bn6f libs.s): it ages the previous
 * buffer half (reverb or silence), then advances every active DirectSound
 * channel's envelope and mixes its samples into the PCM double buffer that
 * DMA streams to the FIFOs.
 *
 * Entry is a tail-jump from SoundMain, inside SoundMain's stack frame:
 *   r0 = SoundInfo, r4 = pcmDmaCounter, r5 = buffer position this frame,
 *   r8 = samples per frame; [sp+0x14] = scanline budget, [sp+0x18] = SoundInfo
 * and it returns through SoundMain's epilogue. All arithmetic (8-bit
 * wraparound mixing, 23-bit fixed-point resampling, envelope steps) matches
 * the original exactly; cport/host/difftest checks it against the lifted code.
 */
#include "game.h"

/* ── layouts (offsets as used by this game's m4a build) ─────────────── */
enum {
    SI_IDENT = 0x00, SI_DMA_COUNTER = 0x04, SI_REVERB = 0x05, SI_MAX_CHANS = 0x06,
    SI_MASTER_VOL = 0x07, SI_MAX_LINES = 0x0C, SI_DIV_FREQ = 0x18,
    SI_CHANNELS = 0x50, SI_PCM_BUFFER = 0x350,
};
enum {
    CH_STATUS = 0x00, CH_TYPE = 0x01, CH_RIGHT_VOL = 0x02, CH_LEFT_VOL = 0x03,
    CH_ATTACK = 0x04, CH_DECAY = 0x05, CH_SUSTAIN = 0x06, CH_RELEASE = 0x07,
    CH_ENV_VOL = 0x09, CH_ENV_VOL_R = 0x0A, CH_ENV_VOL_L = 0x0B,
    CH_ECHO_VOL = 0x0C, CH_ECHO_LEN = 0x0D,
    CH_COUNT = 0x18, CH_FW = 0x1C, CH_FREQ = 0x20, CH_WAVE = 0x24, CH_CUR_PTR = 0x28,
    CH_SIZE = 0x40,
};
enum { WAV_STATUS_HI = 0x03, WAV_LOOP_START = 0x08, WAV_SIZE = 0x0C, WAV_DATA = 0x10 };

/* channel status flags */
enum {
    SF_ENV_MASK = 0x03,     /* 3 attack, 2 decay, 1 sustain, 0 release */
    SF_IEC = 0x04,          /* pseudo-echo tail */
    SF_LOOP = 0x10,
    SF_STOP = 0x40,
    SF_START = 0x80,
    SF_ON = 0xC7,
};
enum { TYPE_FIX = 0x08 };   /* fixed-frequency sample: no resampling */

#define PCM_DMA_BUF_SIZE 0x630  /* right buffer; the left one follows it */
#define SOUND_IDENT 0x68736D53u /* 'Smsh' */

#define U8(a)  (*(u8 *)(uintptr_t)(u32)(a))
#define S8(a)  (*(s8 *)(uintptr_t)(u32)(a))
#define U32(a) (*(u32 *)(uintptr_t)(u32)(a))

/* The previous buffer half becomes a quieter echo, or is cleared. `half`
 * is the distance to the left-channel buffer (PCM_DMA_BUF_SIZE). */
IWRAM_HELPER __attribute__((noinline, optimize("O2"))) static void age_buffer(u32 info, u32 dma_counter, u32 buf, u32 samples, u32 half) {
    u32 reverb = U8(info + SI_REVERB);
    if (reverb) {
        u32 prev = dma_counter == 2 ? info + SI_PCM_BUFFER : buf + samples;
        s32 n = (s32)samples;
        do {
            s32 sum = S8(buf + half) + S8(buf) + S8(prev + half) + S8(prev);
            s32 echo = (sum * (s32)reverb) >> 9;
            if (echo & 0x80)
                echo++;
            U8(buf + half) = (u8)echo;
            U8(buf) = (u8)echo;
            buf++, prev++;
        } while (--n > 0);
        return;
    }
    /* Clear both halves a word at a time, in the original's order: one
     * word if bit 2 of the count is set, two more if bit 3 is, then four
     * per 16 samples (always at least one round of four). */
    u32 right = buf, left = buf + half;
    u32 blocks = samples >> 3;
    if (samples & 4) { U32(right) = 0; right += 4; U32(left) = 0; left += 4; }
    int odd = blocks & 1;
    blocks >>= 1;
    if (odd) {
        U32(right) = 0; right += 4; U32(left) = 0; left += 4;
        U32(right) = 0; right += 4; U32(left) = 0; left += 4;
    }
    s32 n = (s32)blocks;
    do {
        for (int k = 0; k < 4; k++) {
            U32(right) = 0; right += 4;
            U32(left) = 0; left += 4;
        }
    } while (--n > 0);
}

/* Envelope step for one channel. Returns the new status, or -1 when the
 * channel has finished (status already cleared). */
static int envelope(u32 ch, u32 wav, u32 *env_out) {
    u32 status = U8(ch + CH_STATUS);
    u32 env;
    if (status & SF_START) {
        if (status & SF_STOP) {
            U8(ch + CH_STATUS) = 0;
            return -1;
        }
        status = 3;                                  /* attack */
        U8(ch + CH_STATUS) = 3;
        U32(ch + CH_CUR_PTR) = wav + WAV_DATA;
        U32(ch + CH_COUNT) = U32(wav + WAV_SIZE);
        env = 0;
        U8(ch + CH_ENV_VOL) = 0;
        U32(ch + CH_FW) = 0;
        if (U8(wav + WAV_STATUS_HI) & 0xC0) {
            status |= SF_LOOP;
            U8(ch + CH_STATUS) = (u8)status;
        }
        goto attack;
    }
    env = U8(ch + CH_ENV_VOL);
    if (status & SF_IEC) {
        u32 len = U8(ch + CH_ECHO_LEN);
        U8(ch + CH_ECHO_LEN) = (u8)(len - 1);
        if (len > 1)
            goto done;
        U8(ch + CH_STATUS) = 0;
        return -1;
    }
    if (status & SF_STOP) {                          /* release */
        env = (env * U8(ch + CH_RELEASE)) >> 8;
        if (env > U8(ch + CH_ECHO_VOL))
            goto done;
    to_echo:
        env = U8(ch + CH_ECHO_VOL);
        if (env == 0) {
            U8(ch + CH_STATUS) = 0;
            return -1;
        }
        status |= SF_IEC;
        U8(ch + CH_STATUS) = (u8)status;
        goto done;
    }
    switch (status & SF_ENV_MASK) {
    case 2: {                                        /* decay */
        u32 sustain = U8(ch + CH_SUSTAIN);
        env = (env * U8(ch + CH_DECAY)) >> 8;
        if (env > sustain)
            goto done;
        env = sustain;
        if (sustain == 0)
            goto to_echo;
        status--;
        U8(ch + CH_STATUS) = (u8)status;
        goto done;
    }
    case 3:
    attack:
        env += U8(ch + CH_ATTACK);
        if (env >= 0xFF) {
            env = 0xFF;
            status--;
            U8(ch + CH_STATUS) = (u8)status;
        }
        goto done;
    default:                                         /* sustain: unchanged */
        goto done;
    }
done:
    *env_out = env;
    return (int)status;
}

/* Output is mixed a 32-bit word (four samples) at a time, like the
 * original: rotating the word right by 8 brings the next output byte to the
 * top, where the sample's 8-bit contribution (sample * volume) >> 8 is added
 * (wrapping there, as an 8-bit add would). Four steps visit bytes 0..3 and
 * leave the word unrotated. Sample counts per frame are multiples of 16 and
 * the buffers are word aligned, as the original assumes. */
static inline u32 rot8(u32 w) { return (w >> 8) | (w << 24); }
static inline u32 rot8_add(u32 w, s32 sample, u32 vol16) {
    return rot8(w) + (((u32)sample * vol16) & 0xFF000000u);
}

/* The output pointer advances a byte per sample; when it reaches a word
 * boundary the finished word is stored and the next one loaded. A channel
 * that stops mid-word leaves the rest of that word unmixed (rotated back into
 * place). */
#define STORE_WORD(o, r, l) (U32((o) - 4) = (r), U32((o) - 4 + PCM_DMA_BUF_SIZE) = (l))
#define LOAD_WORD(o, r, l)  ((r) = U32(o), (l) = U32((o) + PCM_DMA_BUF_SIZE))

/* Fixed-frequency channel: one sample per output byte. Returns 0 when the
 * sample ran out without a loop (channel stopped). */
IWRAM_HELPER __attribute__((noinline, optimize("O2")))
static int mix_fixed(u32 ch, u32 o, u32 samples, u32 volr, u32 voll,
                     u32 loop_start, u32 loop_len, u32 *count_io, u32 *src_io) {
    /* Counters wrap like the original's 32-bit registers: unsigned
     * arithmetic, signed only where the original compares signed. */
    u32 count = *count_io;
    u32 src = *src_io, vr = volr << 16, vl = voll << 16, end = o + samples;
    u32 r, l;
    LOAD_WORD(o, r, l);
    for (;;) {
        s32 s = S8(src++);
        r = rot8_add(r, s, vr);
        l = rot8_add(l, s, vl);
        if (--count == 0) {
            if (!loop_len) {
                U8(ch + CH_STATUS) = 0;
                while (++o & 3) {
                    r = rot8(r);
                    l = rot8(l);
                }
                STORE_WORD(o, r, l);
                return 0;
            }
            src = loop_start;
            count = loop_len;
        }
        if ((++o & 3) == 0) {
            STORE_WORD(o, r, l);
            if (o == end)
                break;
            LOAD_WORD(o, r, l);
        }
    }
    *count_io = count;
    *src_io = src;
    return 1;
}

/* Resampled channel: linear interpolation between neighbouring samples with
 * a 23-bit fractional position `fw` advancing by divFreq * frequency. */
IWRAM_HELPER __attribute__((noinline, optimize("O2")))
static int mix_resampled(u32 ch, u32 info, u32 o, u32 samples, u32 volr, u32 voll,
                         u32 loop_start, u32 loop_len, u32 *count_io, u32 *src_io) {
    u32 count = *count_io;            /* wraps like a 32-bit register (see mix_fixed) */
    u32 src = *src_io, vr = volr << 16, vl = voll << 16, end = o + samples;
    u32 fw = U32(ch + CH_FW);
    u32 step = U32(info + SI_DIV_FREQ) * U32(ch + CH_FREQ);
    s32 s0 = S8(src);
    s32 delta = S8(++src) - s0;
    u32 r, l;
    LOAD_WORD(o, r, l);
    for (;;) {
        s32 s = s0 + ((s32)(fw * (u32)delta) >> 23);
        r = rot8_add(r, s, vr);
        l = rot8_add(l, s, vl);
        fw += step;
        u32 advance = fw >> 23;
        if (advance) {
            fw &= ~0x3F800000u;
            /* the original's subs/ble: signed compare of the exact result */
            int ran_out = (s32)count <= (s32)advance;
            count -= advance;
            if (ran_out) {
                if (!loop_len) {
                    U8(ch + CH_STATUS) = 0;
                    while (++o & 3) {
                        r = rot8(r);
                        l = rot8(l);
                    }
                    STORE_WORD(o, r, l);
                    return 0;
                }
                /* Wrap into the loop, possibly several times. */
                u32 back = -count;
                src = loop_start;
                for (;;) {
                    s64 sum = (s64)(s32)count + (s32)loop_len;   /* adds/bgt: exact result */
                    count += loop_len;
                    if (sum > 0)
                        break;
                    back -= loop_len;
                }
                s0 = S8(src += back);
            } else if (--advance == 0) {
                s0 += delta;                       /* moved exactly one sample */
            } else {
                s0 = S8(src += advance);
            }
            delta = S8(++src) - s0;
        }
        if ((++o & 3) == 0) {
            STORE_WORD(o, r, l);
            if (o == end)
                break;
            LOAD_WORD(o, r, l);
        }
    }
    *count_io = count;
    *src_io = src - 1;
    U32(ch + CH_FW) = fw;
    return 1;
}

/* The per-channel driver runs once per channel per frame: Thumb keeps it
 * small; the per-sample loops above are ARM. */
IWRAM_THUMB Next SoundMainRAM_iwram_03005700(void) {
    u32 sp = SP;                       /* SoundMain's frame */
    u32 info = U32(sp + 0x18);
    u32 buf = R5, samples = R8;
    u32 max_lines = U32(sp + 0x14);

    age_buffer(R0, R4, buf, samples, R6);

    u32 ch = info + SI_CHANNELS;
    for (u32 left = U8(info + SI_MAX_CHANS); ; ch += CH_SIZE) {
        U32(sp + 4) = left;
        u32 wav = U32(ch + CH_WAVE);
        if (max_lines) {
            /* Out of time for this frame: leave the remaining channels. */
            u32 line = U8(0x04000006);
            if (line < 160)
                line += 228;
            if (line >= max_lines)
                break;
        }
        u32 env;
        int status = U8(ch + CH_STATUS) & SF_ON ? envelope(ch, wav, &env) : -1;
        if (status >= 0) {
            U8(ch + CH_ENV_VOL) = (u8)env;
            env = ((U8(info + SI_MASTER_VOL) + 1) * env) >> 4;
            U8(ch + CH_ENV_VOL_R) = (u8)((U8(ch + CH_RIGHT_VOL) * env) >> 8);
            U8(ch + CH_ENV_VOL_L) = (u8)((U8(ch + CH_LEFT_VOL) * env) >> 8);
            u32 volr = U8(ch + CH_ENV_VOL_R), voll = U8(ch + CH_ENV_VOL_L);

            u32 loop_start = 0, loop_len = status & SF_LOOP;
            U32(sp + 0x10) = loop_len;
            if (loop_len) {
                loop_start = wav + WAV_DATA + U32(wav + WAV_LOOP_START);
                loop_len = U32(wav + WAV_SIZE) - U32(wav + WAV_LOOP_START);
                U32(sp + 0xC) = loop_start;
                U32(sp + 0x10) = loop_len;
            }
            U32(sp) = samples;
            u32 out = U32(sp + 8);
            u32 count = U32(ch + CH_COUNT), src = U32(ch + CH_CUR_PTR);
            int alive = U8(ch + CH_TYPE) & TYPE_FIX
                ? mix_fixed(ch, out, samples, volr, voll, loop_start, loop_len, &count, &src)
                : mix_resampled(ch, info, out, samples, volr, voll, loop_start, loop_len, &count, &src);
            if (alive) {
                U32(ch + CH_COUNT) = count;
                U32(ch + CH_CUR_PTR) = src;
            }
        }
        if ((s32)--left <= 0)
            break;
    }

    /* SoundMain's epilogue: release the sound lock and its frame. */
    U32(info + SI_IDENT) = SOUND_IDENT;
    sp += 0x1C;
    R8 = U32(sp + 0x00); R9 = U32(sp + 0x04); R10 = U32(sp + 0x08); R11 = U32(sp + 0x0C);
    R0 = R8; R1 = R9; R2 = R10;
    R4 = U32(sp + 0x10); R5 = U32(sp + 0x14); R6 = U32(sp + 0x18); R7 = U32(sp + 0x1C);
    R3 = U32(sp + 0x20);
    SP = sp + 0x24;
    cpu.ret = R3 & ~1u;
    return DONE;
}
