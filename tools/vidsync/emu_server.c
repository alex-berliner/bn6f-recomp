/*
 * emu_server.c — the original ROM on the test harness's mGBA core, driven
 * one game frame at a time over stdin/stdout (for tools/vidsync).
 *
 *   emu_server ROM BIOS [SAVE]
 *
 * Commands (one per line), each answered with one line (shot also writes
 * the frame after it):
 *   step KEY [N]  run N game frames (default 1) holding KEY (keyinput,
 *                 active low, hex); keys are set as mgba_run --game-frame-addr
 *                 replays a trace: before every hardware frame while the
 *                 counter still reads the frame. -> "ok GAME HW"
 *   shot          -> "shot 240 160" then 240*160*3 bytes of RGB
 *   save SLOT / load SLOT   in-memory save states -> "ok"
 *   read ADDR N   -> "mem HEX..."
 *   gf            -> "gf GAME HW"
 *   write32 ADDR VALUE   poke memory (hex) -> "ok"
 *   poke G SPEC   a write for game frame G, as the route harness's .poke lines ("ADDR=VALUE/SIZE,...",
 *                 values hex with 0x): applied at the first entry of main_awaitFrame in a hardware frame
 *                 that starts on game frame G (tools/mgba_run --poke-at); kept for every replay -> "ok"
 *   pokes         drop all pokes -> "ok"
 *   savef FILE / loadf FILE   checkpoint the machine (and counters) to a file -> "ok"
 *   hash          -> "hash H64 SEED": EWRAM+IWRAM hash and the primary RNG seed
 *   digest        -> "digest game=GAME hw=HW ewram=.. pal=.. vram=.. oam=..": the route harness's
 *                 state digest (tools/mgba_run --final-digest)
 *   errs V0 N     -> "errs E..": the picture against video frames V0.. (VIDSYNC_THUMBS)
 *   sram FILE     write the save memory to FILE -> "ok"
 *   bits FILE N   load the input display's per-video-frame bits (a 1-D uint16 .npy; also VIDSYNC_BITS at
 *                 start), video frames clamped to 0..N-1 (N defaults to the thumbnails' or the bits' count)
 *   stepx SLOT KEY NEW V0   beam.py's step of a branch in one go: load SLOT, one game frame with KEY (hex),
 *                 save into NEW; the video base vb of the frame's last hardware frame (as follow's) ->
 *                 "stepx HW VB E*17 H64 SEED": errs at vb-8..vb+8, then what hash reports
 *   follow ...    beam.py's advance() for a single branch, many game frames in one go (see follow())
 *   prof          with VIDSYNC_PROF set: calls and microseconds per part since the last prof -> "prof ..."
 *   quit
 * GAME is the game frame counter, unwrapped (see game()).
 * VIDSYNC_ERRS_CHECK (testing): every blur and picture error also computed the plain way (blur_ref,
 * frame_err), aborting unless they are the same bits.
 */
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba/core/timing.h>
#include <mgba/internal/gba/gba.h>
#include <mgba-util/vfs.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

/* VIDSYNC_PROF (testing): time spent per part, reported (and reset) by the prof command */
enum { P_STEP, P_THUMB, P_VBLUR, P_SUM, P_SAVE, P_LOAD, P_HASH, P_N };
static const char *prof_name[P_N] = {"step", "thumb", "vblur", "sum", "save", "load", "hash"};
static double prof_t[P_N];
static long prof_n[P_N];
static int prof_on;
static double now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}
#define PROF(i, stmt) do { if (prof_on) { double t0_ = now_us(); stmt; prof_t[i] += now_us() - t0_; prof_n[i]++; } \
                           else { stmt; } } while (0)

#define GAME_FRAME_ADDR 0x020093D4u
#define SAVE_SIZE 0x8000

static void log_cb(struct mLogger *l, int c, enum mLogLevel lv, const char *f, va_list a) {
    (void)l; (void)c; (void)lv; (void)f; (void)a;
}
static struct mLogger logger = { .log = log_cb };

static struct mCore *core;
static color_t *fb;
static unsigned long long hw;
/* GAME: the u16 counter unwrapped. Whenever it reads lower than before (the
 * wrap past 65535, or a reset) counting continues upward from the previous
 * value, so the game frames of a trace only ever increase. */
static unsigned base, last16;
/* the timeline's pokes up to this game frame are in (kept with every state): a poke for game frame G goes in at
 * the first entry of main_awaitFrame in a hardware frame that starts on game frame G or later, once, as
 * tools/mgba_run applies --poke-at (when the game is already waiting in main_awaitFrame as G begins, that is
 * the next game frame's entry) */
static long long poked_upto = -1;
typedef struct { void *st; unsigned long long hw; unsigned base, last16; long long poked_upto; } Slot;
static Slot slots[65536];
static size_t state_size;   /* state buffers are zeroed: saveState leaves the state's reserved bytes alone */

static unsigned counter16(void) {
    return core->busRead16(core, core->busRead32(core, GAME_FRAME_ADDR));
}
static unsigned game(void) {
    unsigned c = counter16();
    if (c < last16) base += last16 + 1 - c;
    last16 = c;
    return base + c;
}

/* ---- pokes: memory writes at given game frames, at main_awaitFrame's entry (the end of a game frame's
 * work) as the route harness applies its .poke lines, so a trace with writes replays the same there */
#define AWAIT_FRAME_PC 0x080003A0u   /* main_awaitFrame in bn6f.gba */
typedef struct { unsigned g; char spec[480]; } Poke;
static Poke *pokes;
static int npokes, cappokes;

static void apply_spec(const char *spec0) {
    char spec[480];
    snprintf(spec, sizeof spec, "%s", spec0);
    for (char *t = strtok(spec, ","); t; t = strtok(NULL, ",")) {
        uint32_t a = strtoul(t, &t, 0), v = 0, sz = 4;
        if (*t == '=') t++;
        v = strtoul(t, &t, 0);
        if (*t == '/') sz = strtoul(t + 1, NULL, 0);
        if (sz == 1) core->busWrite8(core, a, v);
        else if (sz == 2) core->busWrite16(core, a, v);
        else core->busWrite32(core, a, v);
    }
}

/* A bad write can crash the game: its frame counter stops. After a minute of hardware frames in one game
 * frame (or a hardware frame without a vblank) the machine is dead: hardware frames do nothing and pictures
 * match nothing (the sync loses it at once) until a state is loaded. */
#define HW_CYCLES_MAX (280896ull * 8)
static int dead;

/* one hardware frame with the keys; a pending poke steps it instruction by instruction */
static void run_hw(unsigned key) {
    if (dead) return;
    core->setKeys(core, ~key & 0x3FF);
    unsigned g = game();
    int pending = 0;
    for (int i = 0; i < npokes; i++) if ((long long)pokes[i].g > poked_upto && pokes[i].g <= g) { pending = 1; break; }
    if (!pending) { core->runFrame(core); hw++; return; }
    uint32_t f0 = core->frameCounter(core);
    uint64_t t0 = mTimingGlobalTime(core->timing);
    while (core->frameCounter(core) == f0) {
        if (mTimingGlobalTime(core->timing) - t0 > HW_CYCLES_MAX) { dead = 1; return; }
        int32_t pc = 0, cpsr = 0;
        core->readRegister(core, "pc", &pc);
        core->readRegister(core, "cpsr", &cpsr);
        pc -= (cpsr & 0x20) ? 2 : 4;
        if (pending && ((uint32_t)pc & ~1u) == (AWAIT_FRAME_PC & ~1u)) {
            /* every pending poke, in game-frame order */
            for (long long last = poked_upto;;) {
                int k = -1;
                for (int i = 0; i < npokes; i++)
                    if ((long long)pokes[i].g > last && pokes[i].g <= g && (k < 0 || pokes[i].g < pokes[k].g)) k = i;
                if (k < 0) break;
                for (int i = 0; i < npokes; i++) if (pokes[i].g == pokes[k].g) apply_spec(pokes[i].spec);
                last = pokes[k].g;
            }
            poked_upto = g; pending = 0;
        }
        core->step(core);
    }
    hw++;
}


/* ---- picture comparison (tools/vidsync beam.py's errs, in C) ----
 * Thumbnails are the game screen at half size (120x80) in gray, blurred with
 * scipy's gaussian_filter(sigma 0.7): a 7-tap kernel, reflected edges. The
 * video's come from VIDSYNC_THUMBS (thumb.u8: frames of 80x120 bytes). */
#define TW 120
#define TH 80
#define TN (TW * TH)
#define BLUR_SIGMA 0.7
#define CAP 6.0
static const uint8_t *vthumbs;
static size_t nvthumbs;
static long vbase_frame;   /* VIDSYNC_THUMB_BASE: the video frame the file starts at (a slice) */
static float kern[7];
static uint8_t mask[TN];
static int nmask;

static void cmp_init(void) {
    double sum = 0;
    for (int i = -3; i <= 3; i++) sum += kern[i + 3] = (float)exp(-0.5 * i * i / (BLUR_SIGMA * BLUR_SIGMA));
    for (int i = 0; i < 7; i++) kern[i] /= (float)sum;
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            int m = !(y >= 3 && y < 9 && x >= 38 && x < 60) && y > 0 && y < TH - 1 && x > 0 && x < TW - 1;
            mask[y * TW + x] = (uint8_t)m;
            nmask += m;
        }
    const char *path = getenv("VIDSYNC_THUMBS");
    if (!path) return;
    int fd = open(path, O_RDONLY);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) < 0) return;
    vthumbs = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (vthumbs == MAP_FAILED) { vthumbs = NULL; return; }
    nvthumbs = (size_t)st.st_size / TN;
    const char *b = getenv("VIDSYNC_THUMB_BASE");
    vbase_frame = b ? atol(b) : 0;
}

static int reflect(int i, int n) { return i < 0 ? -i - 1 : i >= n ? 2 * n - i - 1 : i; }

static void blur_ref(const float *in, float *out) {
    static float tmp[TN];
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float a = 0;
            for (int k = -3; k <= 3; k++) a += kern[k + 3] * in[reflect(y + k, TH) * TW + x];
            tmp[y * TW + x] = a;
        }
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float a = 0;
            for (int k = -3; k <= 3; k++) a += kern[k + 3] * tmp[y * TW + reflect(x + k, TW)];
            out[y * TW + x] = a;
        }
}

/* blur_ref with the same arithmetic per pixel (a = 0, then a += kern * in for the 7 taps in order) in SIMD
 * across a row's pixels: 8 at a time, the reflected edges from a padded row */
typedef float v8f __attribute__((vector_size(32)));
typedef float v8fu __attribute__((vector_size(32), aligned(4)));
static int blur_check = -1;          /* VIDSYNC_ERRS_CHECK (testing): against blur_ref, every call */
static void blur(const float *in, float *out) {
    static float tmp[TN];
    for (int y = 0; y < TH; y++) {
        const float *r[7];
        for (int k = -3; k <= 3; k++) r[k + 3] = in + reflect(y + k, TH) * TW;
        for (int x = 0; x < TW; x += 8) {
            v8f a = {0};
            for (int k = 0; k < 7; k++) a += kern[k] * *(const v8fu *)(r[k] + x);
            *(v8fu *)(tmp + y * TW + x) = a;
        }
    }
    for (int y = 0; y < TH; y++) {
        float pad[TW + 6 + 8];
        for (int x = -3; x < TW + 3; x++) pad[x + 3] = tmp[y * TW + reflect(x, TW)];
        for (int x = 0; x < TW; x += 8) {
            v8f a = {0};
            for (int k = 0; k < 7; k++) a += kern[k] * *(const v8fu *)(pad + x + k);
            *(v8fu *)(out + y * TW + x) = a;
        }
    }
    if (blur_check < 0) blur_check = getenv("VIDSYNC_ERRS_CHECK") != NULL;
    if (blur_check) {
        static float ref[TN];
        blur_ref(in, ref);
        if (memcmp(ref, out, sizeof ref)) { fprintf(stderr, "blur: differs from blur_ref\n"); abort(); }
    }
}

/* a video frame, blurred, with its mean and expected match error (cached) */
typedef struct { long v; float img[TN]; float mean, expect; } VFrame;
#define VCACHE 4096
static VFrame *vcache;

static const VFrame *vframe(long v) {
    if (!vcache) vcache = calloc(VCACHE, sizeof *vcache);
    v -= vbase_frame;
    if (v < 0) v = 0;
    if ((size_t)v >= nvthumbs) v = (long)nvthumbs - 1;
    VFrame *f = &vcache[v % VCACHE];
    if (f->v == v + 1) return f;               /* stored as v+1 so a zeroed slot is empty */
    static float raw[TN];
    for (int i = 0; i < TN; i++) raw[i] = vthumbs[(size_t)v * TN + i];
    PROF(P_VBLUR, blur(raw, f->img));
    double m = 0, ey = 0, ex = 0; int ny = 0, nx = 0;
    for (int i = 0; i < TN; i++) if (mask[i]) m += f->img[i];
    for (int y = 0; y < TH - 1; y++) for (int x = 0; x < TW; x++)
        if (mask[(y + 1) * TW + x]) { ey += fabsf(f->img[(y + 1) * TW + x] - f->img[y * TW + x]); ny++; }
    for (int y = 0; y < TH; y++) for (int x = 0; x < TW - 1; x++)
        if (mask[y * TW + x + 1]) { ex += fabsf(f->img[y * TW + x + 1] - f->img[y * TW + x]); nx++; }
    f->mean = (float)(m / nmask);
    f->expect = (float)(0.8 + 0.62 * (ey / ny + ex / nx) / 2);
    f->v = v + 1;
    return f;
}

/* the emulator's current picture as a blurred thumbnail */
static void emu_thumb(const color_t *fb, unsigned stride, float *out) {
    static float g[TN];
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            float a = 0;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) {
                    color_t c = fb[(size_t)(2 * y + dy) * stride + 2 * x + dx];
                    a += 0.299f * (c & 0xFF) + 0.587f * ((c >> 8) & 0xFF) + 0.114f * ((c >> 16) & 0xFF);
                }
            g[y * TW + x] = a / 4;
        }
    blur(g, out);
}

/* normalized error of picture E against video frame v: brightness counts a quarter */
static float frame_err(const float *E, float emean, long v) {
    if (dead) return (float)CAP;         /* a dead machine matches nothing */
    const VFrame *f = vframe(v);
    float d = emean - f->mean;
    double s = 0;
    for (int i = 0; i < TN; i++) if (mask[i]) s += fabsf(E[i] - f->img[i] - d);
    float e = (float)((s / nmask + 0.25 * fabsf(d)) / f->expect);
    return e < CAP ? e : (float)CAP;
}

/* frame_err for the n (<= 32) video frames v0.. at once, the same numbers: the frames' masked pixels sit in
 * a ring of 32 columns (clamped video frame % 32), and one pass over the pixels sums all the columns, each in
 * pixel order with frame_err's float terms and double sum, in SIMD across the columns. */
typedef double v4d __attribute__((vector_size(32)));
typedef int v8i __attribute__((vector_size(32)));
#define RING 32
static int midx[TN];                 /* the masked pixels, in order */
static v8f (*ring)[RING / 8];        /* [masked pixel][column / 8] */
static long ring_v[RING];            /* the clamped video frame in a column, + 1 (0: none) */
static float ring_mean[RING], ring_expect[RING];
/* the compared pixels: all but the input display and the outer frame, less the rectangle rows y0..y1-1,
 * columns x0..x1-1 (a stream overlay on the video); the video frames' cached statistics depend on it */
static void set_mask(int y0, int y1, int x0, int x1) {
    nmask = 0;
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            int m = !(y >= 3 && y < 9 && x >= 38 && x < 60) && y > 0 && y < TH - 1 && x > 0 && x < TW - 1
                    && !(y >= y0 && y < y1 && x >= x0 && x < x1);
            mask[y * TW + x] = (uint8_t)m;
            nmask += m;
        }
    if (vcache) memset(vcache, 0, VCACHE * sizeof *vcache);
    if (ring) { free(ring); ring = NULL; }
    memset(ring_v, 0, sizeof ring_v);
}

static long vclamp(long v) {
    v -= vbase_frame;
    if (v < 0) v = 0;
    if ((size_t)v >= nvthumbs) v = (long)nvthumbs - 1;
    return v;
}

static void errs_block(const float *E, float emean, long v0, int n, float *out) {
    if (!ring) {
        for (int i = 0, j = 0; i < TN; i++) if (mask[i]) midx[j++] = i;
        ring = aligned_alloc(32, (size_t)nmask * sizeof *ring);
        memset(ring, 0, (size_t)nmask * sizeof *ring);
    }
    for (int i = 0; i < n; i++) {
        long c = vclamp(v0 + i);
        int col = (int)(c % RING);
        if (ring_v[col] == c + 1) continue;
        const VFrame *f = vframe(v0 + i);
        float *r = (float *)ring;
        for (int j = 0; j < nmask; j++) r[(size_t)j * RING + col] = f->img[midx[j]];
        ring_mean[col] = f->mean; ring_expect[col] = f->expect; ring_v[col] = c + 1;
    }
    v8f d[RING / 8];
    for (int b = 0; b < RING / 8; b++)
        for (int k = 0; k < 8; k++) d[b][k] = emean - ring_mean[b * 8 + k];
    double t0 = prof_on ? now_us() : 0;
    v4d acc[RING / 4] = {0};
    const v8i absm = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};
    for (int j = 0; j < nmask; j++) {
        float x = E[midx[j]];
        v8f e = {x, x, x, x, x, x, x, x};
        for (int b = 0; b < RING / 8; b++) {
            v8f t = e - ring[j][b] - d[b];
            t = (v8f)((v8i)t & absm);                       /* fabsf */
            acc[2 * b] += __builtin_convertvector(__builtin_shufflevector(t, t, 0, 1, 2, 3), v4d);
            acc[2 * b + 1] += __builtin_convertvector(__builtin_shufflevector(t, t, 4, 5, 6, 7), v4d);
        }
    }
    if (prof_on) { prof_t[P_SUM] += now_us() - t0; prof_n[P_SUM]++; }
    for (int i = 0; i < n; i++) {
        int col = (int)(vclamp(v0 + i) % RING);
        double s = acc[col / 4][col % 4];
        float dd = emean - ring_mean[col];
        float e = (float)((s / nmask + 0.25 * fabsf(dd)) / ring_expect[col]);
        out[i] = e < CAP ? e : (float)CAP;
    }
    static int check = -1;               /* VIDSYNC_ERRS_CHECK (testing): against frame_err, every call */
    if (check < 0) check = getenv("VIDSYNC_ERRS_CHECK") != NULL;
    if (check)
        for (int i = 0; i < n; i++) {
            float r = frame_err(E, emean, v0 + i);
            if (memcmp(&r, &out[i], sizeof r)) {
                fprintf(stderr, "errs_block: video frame %ld: %.9g, frame_err %.9g\n", v0 + i, out[i], r);
                abort();
            }
        }
    if (dead) for (int i = 0; i < n; i++) out[i] = (float)CAP;
}

/* one game frame holding key (the step command's unit) */
static void step_game1(unsigned key) {
    unsigned g = game(), guard = 0;
    while (game() == g && guard++ < 3600) run_hw(key);
    if (game() == g) dead = 1;           /* a minute in one game frame: crashed */
}
static void step_game(unsigned key) { PROF(P_STEP, step_game1(key)); }

/* the current picture's errors against video frames v0..v0+n-1 as the errs command reports them: printed
 * with %.5f and read back as a double. (double)e * 1e5 is exact (24 + 12 significant bits), rint rounds half
 * to even as glibc's printf does, and the division is correctly rounded as strtod is. */
static void errs_now(unsigned stride, long v0, int n, double *out) {
    static float E[TN];
    PROF(P_THUMB, emu_thumb(fb, stride, E));
    double m = 0;
    for (int i = 0; i < TN; i++) if (mask[i]) m += E[i];
    float emean = (float)(m / nmask), e[RING];
    for (int i0 = 0; i0 < n; i0 += RING) {
        int k = n - i0 < RING ? n - i0 : RING;
        if (vthumbs) errs_block(E, emean, v0 + i0, k, e);
        else for (int i = 0; i < k; i++) e[i] = (float)CAP;
        for (int i = 0; i < k; i++) out[i0 + i] = rint((double)e[i] * 1e5) / 1e5;
    }
}

/* ---- the input display (tools/vidsync bits.npy): per video frame, the held buttons' bits */
static uint16_t *vbits;
static long nvbits, nvclamp;

static int bits_load(const char *path, long n) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char pre[10];
    int ok = 0;
    if (fread(pre, 1, 10, f) == 10 && !memcmp(pre, "\x93NUMPY", 6)) {
        size_t hl = pre[8] | pre[9] << 8, off = 10;
        if (pre[6] >= 2) {                       /* version 2+: a 4-byte header length */
            unsigned char x[2];
            if (fread(x, 1, 2, f) != 2) goto out;
            hl |= (size_t)x[0] << 16 | (size_t)x[1] << 24; off = 12;
        }
        char *h = calloc(1, hl + 1);
        if (fread(h, 1, hl, f) == hl) {
            const char *sh = strstr(h, "'shape'");
            long cnt = -1;
            if (sh && (sh = strchr(sh, '('))) cnt = strtol(sh + 1, NULL, 10);
            if (strstr(h, "'<u2'") && strstr(h, "'fortran_order': False") && cnt > 0) {
                uint16_t *b = malloc((size_t)cnt * 2);
                if (fseek(f, (long)(off + hl), SEEK_SET) == 0 && fread(b, 2, (size_t)cnt, f) == (size_t)cnt) {
                    free(vbits); vbits = b; nvbits = cnt; ok = 1;
                } else free(b);
            }
        }
        free(h);
    }
out:
    fclose(f);
    if (ok) nvclamp = n > 0 ? n : vthumbs ? vbase_frame + (long)nvthumbs : nvbits;
    return ok;
}

/* beam.py's key(v): the keys held at video frame v (already + DELTA), keyinput's active-low form */
static unsigned vkey(long v) {
    if (v > nvclamp - 1) v = nvclamp - 1;
    if (v < 0) v = 0;
    return v < nvbits ? (~(unsigned)vbits[v]) & 0x3FF : 0x3FF;
}

static void slot_save(Slot *s) {
    if (!s->st) s->st = calloc(1, state_size);
    PROF(P_SAVE, core->saveState(core, s->st));
    s->hw = hw; s->base = base; s->last16 = last16; s->poked_upto = poked_upto;
}
static void slot_load(const Slot *s) {
    PROF(P_LOAD, core->loadState(core, s->st)); dead = 0;
    hw = s->hw; base = s->base; last16 = s->last16; poked_upto = s->poked_upto;
}

/* " H64 SEED": the game's working memory (EWRAM, IWRAM; branches equal here are the same game), the RNG seed */
static void print_hash(void) {
    struct GBA *gba = core->board;
    const uint64_t *w = (const uint64_t *)gba->memory.wram, *iw = (const uint64_t *)gba->memory.iwram;
    uint64_t hsh = 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < 0x40000 / 8; i++) hsh = (hsh ^ w[i]) * 0x100000001B3ull + (hsh >> 29);
    for (size_t i = 0; i < 0x8000 / 8; i++) hsh = (hsh ^ iw[i]) * 0x100000001B3ull + (hsh >> 29);
    printf(" %016llx %08x", (unsigned long long)hsh, (unsigned)core->busRead32(core, 0x020013F0));
}

/* ---- follow: beam.py's Beam.advance() for a single branch, frame after frame, while it stays one branch.
 *
 *   follow SLOT N V0 H DELTA TAU STOPCOST VEND STALLW STALLD NHIST HIST.. COST*17
 *
 * From the branch's machine in SLOT (hardware frame count H, offset costs COST for offsets -8..8, the
 * video base V0 of beam.py's vbase), each game frame: pred = min(cost + JUMP); the key at the video frame of
 * the best predicted offset; one game frame with it; the picture's errors at vb-8..vb+8; the new costs, the
 * chosen video frame, the frame's best cost; costs less the best; the window recentred on the best offset
 * (V0 += o, the costs shifted, the far end filled with max + 3). It stops
 *   - before a frame where offsets with pred < min(pred) + TAU read different keys (beam.py splits there)
 *     -> "split";
 *   - after N frames -> "n";
 *   - after a frame whose best cost exceeds STOPCOST -> "cost", or whose video frame is >= VEND (if >= 0)
 *     -> "end", or (STALLW > 0) whose video frame is less than STALLD past the one STALLW-1 frames earlier,
 *     counting the caller's NHIST previous frames HIST -> "stall": the caller's desync/stall rules see those.
 * SLOT ends up holding the state after the last frame run (the machine is unspecified: load SLOT).
 * -> "follow M WHY V0 H COST*17" and then KEY V BEST for each of the M frames run. Doubles as numpy has them;
 * they are printed with %.17g, which reads back exactly. */
#define K 8
#define NOFF (2 * K + 1)
#define RATE (60 / 59.7275)

static double jump_cost(int d) {
    d = abs(d);
    return d == 0 ? 0.0 : d == 1 ? 2.0 : 4.0 + 0.5 * d;
}

static long vbase_of(double v0, unsigned long long h) { return (long)rint(v0 + RATE * (double)h); }

static void follow(char *args, unsigned stride) {
    char *p = args;
    long slot = strtol(p, &p, 10);
    long n = strtol(p, &p, 10);
    double v0 = strtod(p, &p);
    unsigned long long h = strtoull(p, &p, 10);
    long delta = strtol(p, &p, 10);
    double tau = strtod(p, &p), stopc = strtod(p, &p);
    long vend = strtol(p, &p, 10);
    long stallw = strtol(p, &p, 10), stalld = strtol(p, &p, 10), nh = strtol(p, &p, 10);
    if (n < 0) n = 0;
    if (nh < 0) nh = 0;
    long *hist = malloc((size_t)(nh + n + 1) * sizeof *hist);
    for (long i = 0; i < nh; i++) hist[i] = strtol(p, &p, 10);
    double cost[NOFF];
    for (int i = 0; i < NOFF; i++) cost[i] = strtod(p, &p);
    if (!vthumbs || !vbits || slot < 0 || slot > 0xFFFF || !slots[slot].st) {
        free(hist); printf("err follow needs VIDSYNC_THUMBS, the bits and a saved SLOT\n"); return;
    }

    static double J[NOFF][NOFF];
    static int jinit;
    if (!jinit) {
        for (int a = 0; a < NOFF; a++) for (int b = 0; b < NOFF; b++) J[a][b] = jump_cost(a - b);
        jinit = 1;
    }
    /* each frame as beam.py's advance() has the machine: loaded from the state saved after the frame before
     * (at first the branch's slot), one game frame, saved (mGBA's audio state depends on that order) */
    static Slot buf[2];
    for (int i = 0; i < 2; i++) if (!buf[i].st) buf[i].st = calloc(1, state_size);
    Slot *pre = &slots[slot];
    unsigned *rk = malloc((size_t)(n + 1) * sizeof *rk);
    long *rv = malloc((size_t)(n + 1) * sizeof *rv);
    double *rb = malloc((size_t)(n + 1) * sizeof *rb);
    long m = 0;
    const char *why = "n";
    while (m < n) {
        double pred[NOFF];
        for (int a = 0; a < NOFF; a++) {
            double x = cost[0] + J[a][0];
            for (int b = 1; b < NOFF; b++) { double y = cost[b] + J[a][b]; if (y < x) x = y; }
            pred[a] = x;
        }
        int ib = 0;
        for (int i = 1; i < NOFF; i++) if (pred[i] < pred[ib]) ib = i;
        double lim = pred[ib] + tau;
        long vb0 = vbase_of(v0, h);
        unsigned k0 = vkey(vb0 + (ib - K) + delta);
        /* the keys the near offsets read after the frame, if it takes one hardware frame (the usual case;
         * then vb == vb0): disagreeing, the branch splits here */
        int split = 0;
        for (int i = 0; i < NOFF; i++) if (pred[i] < lim && vkey(vb0 + (i - K) + delta) != k0) split = 1;
        if (split) { why = "split"; break; }
        slot_load(pre);
        step_game(k0);
        unsigned long long h1 = hw;
        long vb = vbase_of(v0, h1 - 1);
        if (vb != vb0) {                     /* a longer frame: the keys at the real vb decide */
            for (int i = 0; i < NOFF; i++) if (pred[i] < lim && vkey(vb + (i - K) + delta) != k0) split = 1;
            if (split) { why = "split"; break; }   /* the branch stays as before this frame (pre) */
        }
        Slot *post = &buf[m & 1];
        slot_save(post);
        double er[NOFF], c[NOFF];
        errs_now(stride, vb - K, NOFF, er);
        for (int i = 0; i < NOFF; i++) c[i] = pred[i] + er[i];
        int bo = 0;
        for (int i = 1; i < NOFF; i++) if (c[i] < c[bo]) bo = i;
        double best = c[bo];
        for (int i = 0; i < NOFF; i++) cost[i] = c[i] - best;
        int io = 0;
        for (int i = 1; i < NOFF; i++) if (cost[i] < cost[io]) io = i;
        int o = io - K;
        if (o) {
            v0 += o;
            double mx = cost[0];
            for (int i = 1; i < NOFF; i++) if (cost[i] > mx) mx = cost[i];
            double s[NOFF];
            for (int i = 0; i < NOFF; i++) {
                int j = i + o;
                s[i] = j >= 0 && j < NOFF ? cost[j] : mx + 3.0;
            }
            memcpy(cost, s, sizeof cost);
        }
        h = h1;
        pre = post;
        long v = vb + (bo - K);
        rk[m] = k0; rv[m] = v; rb[m] = best; m++;
        hist[nh++] = v;
        if (best > stopc) { why = "cost"; break; }
        if (vend >= 0 && v >= vend) { why = "end"; break; }
        if (stallw > 0 && nh >= stallw && v - hist[nh - stallw] < stalld) { why = "stall"; break; }
    }
    printf("follow %ld %s %.17g %llu", m, why, v0, h);
    for (int i = 0; i < NOFF; i++) printf(" %.17g", cost[i]);
    for (long i = 0; i < m; i++) printf(" %u %ld %.17g", rk[i], rv[i], rb[i]);
    printf("\n");
    if (pre != &slots[slot]) {               /* the branch's slot: the state after its last frame */
        memcpy(slots[slot].st, pre->st, state_size);
        slots[slot].hw = pre->hw; slots[slot].base = pre->base; slots[slot].last16 = pre->last16;
        slots[slot].poked_upto = pre->poked_upto;   /* else the next load applies this stretch's pokes again */
    }
    free(hist); free(rk); free(rv); free(rb);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ROM BIOS [SAVE]\n", argv[0]); return 1; }
    mLogSetDefaultLogger(&logger);
    core = mCoreFind(argv[1]);
    if (!core || !core->init(core)) return 1;
    mCoreInitConfig(core, NULL);
    struct mCoreOptions opts = { .useBios = 1, .skipBios = 1, .volume = 0 };
    mCoreConfigLoadDefaults(&core->config, &opts);
    mCoreLoadConfig(core);
    unsigned w, h;
    core->desiredVideoDimensions(core, &w, &h);
    fb = calloc((size_t)w * h, sizeof *fb);
    core->setVideoBuffer(core, fb, w);
    if (!mCoreLoadFile(core, argv[1])) return 1;
    void *save = calloc(1, SAVE_SIZE);
    memset(save, 0xFF, SAVE_SIZE);
    if (argc > 3) {
        FILE *f = fopen(argv[3], "rb");
        if (!f || fread(save, 1, SAVE_SIZE, f) == 0) return 1;
        fclose(f);
    }
    core->loadSave(core, VFileMemChunk(save, SAVE_SIZE));
    struct VFile *vb = VFileOpen(argv[2], O_RDONLY);
    if (!vb) return 1;
    core->loadBIOS(core, vb, 0);
    core->reset(core);
    state_size = core->stateSize(core);
    unsigned char *rgb = malloc((size_t)w * h * 3);
    cmp_init();
    prof_on = getenv("VIDSYNC_PROF") != NULL;
    const char *bp = getenv("VIDSYNC_BITS");
    if (bp && !bits_load(bp, 0)) fprintf(stderr, "emu_server: cannot read the bits in %s\n", bp);

    static char line[1 << 16];
    setvbuf(stdout, NULL, _IOFBF, 1 << 20);
    while (fgets(line, sizeof line, stdin)) {
        char cmd[16] = "";
        sscanf(line, "%15s", cmd);
        if (!strcmp(cmd, "step")) {
            unsigned key = 0x3FF, n = 1;
            sscanf(line, "%*s %x %u", &key, &n);
            for (unsigned i = 0; i < n; i++) step_game(key);
            printf("ok %u %llu\n", game(), hw);
        } else if (!strcmp(cmd, "shot")) {
            for (size_t i = 0; i < (size_t)w * h; i++) {
                color_t c = fb[i];
                rgb[3 * i] = c & 0xFF; rgb[3 * i + 1] = (c >> 8) & 0xFF; rgb[3 * i + 2] = (c >> 16) & 0xFF;
            }
            printf("shot %u %u\n", w, h);
            fwrite(rgb, 1, (size_t)w * h * 3, stdout);
        } else if (!strcmp(cmd, "save") || !strcmp(cmd, "load")) {
            unsigned s = 0;
            sscanf(line, "%*s %u", &s);
            s &= 0xFFFF;
            if (cmd[0] == 's') {
                if (!slots[s].st) slots[s].st = calloc(1, state_size);
                PROF(P_SAVE, core->saveState(core, slots[s].st));
                slots[s].hw = hw; slots[s].base = base; slots[s].last16 = last16; slots[s].poked_upto = poked_upto;
                printf("ok\n");
            } else if (slots[s].st) {
                PROF(P_LOAD, core->loadState(core, slots[s].st)); dead = 0;
                hw = slots[s].hw; base = slots[s].base; last16 = slots[s].last16; poked_upto = slots[s].poked_upto;
                printf("ok\n");
            } else {
                printf("err empty slot\n");
            }
        } else if (!strcmp(cmd, "savef") || !strcmp(cmd, "loadf")) {
            /* a checkpoint file: hw, base, last16, the core state, then poked_upto */
            char path[400];
            int ok = 0;
            void *st = calloc(1, state_size);
            if (sscanf(line, "%*s %399s", path) == 1) {
                FILE *f = fopen(path, cmd[0] == 's' ? "wb" : "rb");
                if (f && cmd[0] == 's') {
                    core->saveState(core, st);
                    ok = fwrite(&hw, sizeof hw, 1, f) == 1 && fwrite(&base, sizeof base, 1, f) == 1 &&
                         fwrite(&last16, sizeof last16, 1, f) == 1 && fwrite(st, 1, state_size, f) == state_size &&
                         fwrite(&poked_upto, sizeof poked_upto, 1, f) == 1;
                } else if (f) {
                    ok = fread(&hw, sizeof hw, 1, f) == 1 && fread(&base, sizeof base, 1, f) == 1 &&
                         fread(&last16, sizeof last16, 1, f) == 1 && fread(st, 1, state_size, f) == state_size &&
                         core->loadState(core, st);
                    dead = 0;
                    /* files from before poked_upto: everything before this game frame counts as in */
                    if (ok && fread(&poked_upto, sizeof poked_upto, 1, f) != 1) poked_upto = (long long)game() - 1;
                }
                if (f) fclose(f);
            }
            free(st);
            printf(ok ? "ok\n" : "err %s\n", path);
        } else if (!strcmp(cmd, "errs")) {
            /* errs V0 N: the current picture's normalized error against video frames V0..V0+N-1 */
            long v0 = 0; int n = 0;
            sscanf(line, "%*s %ld %d", &v0, &n);
            static float E[TN];
            emu_thumb(fb, w, E);
            double m = 0;
            for (int i = 0; i < TN; i++) if (mask[i]) m += E[i];
            float emean = (float)(m / nmask), e[RING];
            printf("errs");
            for (int i0 = 0; i0 < n; i0 += RING) {
                int k = n - i0 < RING ? n - i0 : RING;
                if (vthumbs) errs_block(E, emean, v0 + i0, k, e);
                for (int i = 0; i < k; i++) printf(" %.5f", vthumbs ? e[i] : (float)CAP);
            }
            printf("\n");
        } else if (!strcmp(cmd, "hash")) {
            printf("hash");
            PROF(P_HASH, print_hash());
            printf("\n");
        } else if (!strcmp(cmd, "stepx")) {
            unsigned s0 = 0, key = 0x3FF, s1 = 0; double v0 = 0;
            if (sscanf(line, "%*s %u %x %u %lf", &s0, &key, &s1, &v0) == 4 && slots[s0 & 0xFFFF].st) {
                s0 &= 0xFFFF; s1 &= 0xFFFF;
                PROF(P_LOAD, core->loadState(core, slots[s0].st)); dead = 0;
                hw = slots[s0].hw; base = slots[s0].base; last16 = slots[s0].last16; poked_upto = slots[s0].poked_upto;
                step_game(key);
                long vb = vbase_of(v0, hw - 1);
                double er[NOFF];
                errs_now(w, vb - K, NOFF, er);
                if (!slots[s1].st) slots[s1].st = calloc(1, state_size);
                PROF(P_SAVE, core->saveState(core, slots[s1].st));
                slots[s1].hw = hw; slots[s1].base = base; slots[s1].last16 = last16; slots[s1].poked_upto = poked_upto;
                printf("stepx %llu %ld", hw, vb);
                for (int i = 0; i < NOFF; i++) printf(" %.5f", er[i]);
                PROF(P_HASH, print_hash());
                printf("\n");
            } else printf("err stepx SLOT KEY NEW V0\n");
        } else if (!strcmp(cmd, "digest")) {
            /* the route harness's state digest (tools/mgba_run --final-digest, tools/record's "# end"):
             * a 64-bit FNV over the 32-bit words of EWRAM, palette, VRAM and OAM */
            static const struct { const char *n; unsigned base, size; } dr[] = {
                { "ewram", 0x02000000, 0x40000 }, { "pal", 0x05000000, 0x400 },
                { "vram", 0x06000000, 0x18000 }, { "oam", 0x07000000, 0x400 } };
            printf("digest game=%u hw=%llu", game(), hw);
            for (unsigned r = 0; r < 4; r++) {
                unsigned long long d = 1469598103934665603ull;
                for (unsigned a = 0; a < dr[r].size; a += 4) {
                    d ^= core->busRead32(core, dr[r].base + a);
                    d *= 1099511628211ull;
                }
                printf(" %s=%016llx", dr[r].n, d);
            }
            printf("\n");
        } else if (!strcmp(cmd, "read")) {
            unsigned a = 0, n = 0;
            sscanf(line, "%*s %x %u", &a, &n);
            printf("mem ");
            for (unsigned i = 0; i < n; i++) printf("%02x", core->busRead8(core, a + i));
            printf("\n");
        } else if (!strcmp(cmd, "poke")) {
            unsigned g = 0; char spec[480] = "";
            if (sscanf(line, "%*s %u %479s", &g, spec) == 2) {
                if (npokes == cappokes) pokes = realloc(pokes, (cappokes = cappokes * 2 + 16) * sizeof *pokes);
                pokes[npokes].g = g; snprintf(pokes[npokes].spec, sizeof pokes[npokes].spec, "%s", spec); npokes++;
                printf("ok\n");
            } else printf("err poke G SPEC\n");
        } else if (!strcmp(cmd, "pokes")) {
            npokes = 0;
            printf("ok\n");
        } else if (!strcmp(cmd, "write32")) {
            unsigned a = 0, v = 0;
            sscanf(line, "%*s %x %x", &a, &v);
            core->busWrite32(core, a, v);
            printf("ok\n");
        } else if (!strcmp(cmd, "mask")) {
            /* mask [Y0 Y1 X0 X1]: leave that thumbnail rectangle out of the comparison (none: the default) */
            int y0 = 0, y1 = 0, x0 = 0, x1 = 0;
            if (sscanf(line, "%*s %d %d %d %d", &y0, &y1, &x0, &x1) != 4) y0 = y1 = x0 = x1 = 0;
            set_mask(y0, y1, x0, x1);
            printf("ok\n");
        } else if (!strcmp(cmd, "gf")) {
            unsigned g = game();
            printf("gf %u %llu\n", g, hw);
        } else if (!strcmp(cmd, "sram")) {
            char path[200];
            if (sscanf(line, "%*s %199s", path) == 1) {
                FILE *f = fopen(path, "wb");
                /* the game's save memory as it is now (the buffer given at start is only its first contents) */
                struct GBA *gba = core->board;
                size_t n = GBASavedataSize(&gba->memory.savedata);
                const void *d = gba->memory.savedata.data;
                if (!d || !n) { d = save; n = SAVE_SIZE; }
                if (f) { fwrite(d, 1, n, f); fclose(f); }
            }
            printf("ok\n");
        } else if (!strcmp(cmd, "bits")) {
            char path[400]; long n = 0;
            if (sscanf(line, "%*s %399s %ld", path, &n) >= 1 && bits_load(path, n)) printf("ok %ld\n", nvbits);
            else printf("err bits\n");
        } else if (!strcmp(cmd, "follow")) {
            follow(line + 6, w);
        } else if (!strcmp(cmd, "prof")) {
            printf("prof");
            for (int i = 0; i < P_N; i++) printf(" %s %ld %.0f", prof_name[i], prof_n[i], prof_t[i]), prof_t[i] = 0, prof_n[i] = 0;
            printf("\n");
        } else if (!strcmp(cmd, "quit")) {
            break;
        } else {
            printf("err %s\n", cmd);
        }
        fflush(stdout);
    }
    return 0;
}
