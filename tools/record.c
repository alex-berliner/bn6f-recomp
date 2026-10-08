/*
 * record.c — play the original ROM and record an input trace for the tests.
 *
 *   build-tools/record ROM OUT.trace [--save START.sav] [--scale N]
 *   (tools/record.sh NAME [START.sav] wraps this)
 *
 * Runs the ROM on the same mGBA core and BIOS as the test harness
 * (mgba_run), in a window with sound. Keys are latched once per game frame
 * (when the game's frame counter changes) and written as
 * "game_frame,keyinput" lines, which is exactly how mgba_run
 * --game-frame-addr replays them, so a recording replays identically.
 *
 * Every recording gets, next to OUT.trace:
 *   OUT.sav      the save memory it starts from: a copy of --save, or a
 *                blank save for a new game (the test scripts load it);
 *   OUT.end.sav  the save memory (SRAM) when recording stopped (a starting point
 *                for the next recording; tools/check_recording.sh checks a
 *                replay ends with the same).
 * The trace ends with "# end hw_frames=... game_frame=... <digest>", the
 * machine state after the last frame, which check_recording.sh compares too.
 *
 * Under the game a status bar shows REC (or STOPPED/PAUSED/FAST), the game
 * frame, the number of input changes recorded, and every button, lit while
 * held: a key that does not light up is not reaching the recorder (e.g. the
 * window lost focus).
 *
 * Keyboard: arrows, X = A, Z = B, A = L, S = R, Enter = Start,
 * Backspace = Select. Controllers: d-pad/left stick, east = A, south = B,
 * shoulders = L/R, Start, Back = Select. Hold Tab = fast forward,
 * P = pause, Esc, closing the window or Ctrl+C = stop.
 * No savestates, rewind or soft reset: they would break the input chain.
 *
 * Self-test (tools/record_test.sh): RECORD_TEST_FRAMES=N stops after N
 * frames, presses pseudo-random keys at arbitrary frames instead of reading
 * the keyboard, and prints mgba_run-style "digest" lines every 20 game
 * frames, so a replay of the trace can be compared.
 *
 * Build: gcc -O2 -o build-tools/record tools/record.c -lmgba $(sdl2-config --cflags --libs)
 */
#include <SDL.h>
#include <fcntl.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_FRAME_ADDR 0x020093D4u   /* toolkit pointer; [+0] -> u16 game frame counter */
#define SAVE_SIZE 0x8000              /* SRAM_V113: 32 KB battery SRAM */
#define SAMPLE_RATE 48000
#define FPS 59.7275
#define BAR 20                        /* status bar height, GBA pixels */

enum { K_A, K_B, K_SELECT, K_START, K_RIGHT, K_LEFT, K_UP, K_DOWN, K_R, K_L };

static volatile sig_atomic_t g_stop;
static void on_signal(int sig) { (void)sig; g_stop = 1; }

static void quiet_log(struct mLogger *l, int c, enum mLogLevel lv, const char *f, va_list a) {
    (void)l; (void)c; (void)lv; (void)f; (void)a;
}

static unsigned keyboard_keys(const Uint8 *ks) {
    static const struct { SDL_Scancode sc; int bit; } map[] = {
        { SDL_SCANCODE_X, K_A }, { SDL_SCANCODE_Z, K_B }, { SDL_SCANCODE_BACKSPACE, K_SELECT },
        { SDL_SCANCODE_RETURN, K_START }, { SDL_SCANCODE_RIGHT, K_RIGHT }, { SDL_SCANCODE_LEFT, K_LEFT },
        { SDL_SCANCODE_UP, K_UP }, { SDL_SCANCODE_DOWN, K_DOWN }, { SDL_SCANCODE_S, K_R }, { SDL_SCANCODE_A, K_L } };
    unsigned m = 0;
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if (ks[map[i].sc])
            m |= 1u << map[i].bit;
    return m;
}

static unsigned pad_keys(SDL_GameController *gc) {
    if (!gc)
        return 0;
    static const struct { SDL_GameControllerButton b; int bit; } map[] = {
        { SDL_CONTROLLER_BUTTON_B, K_A }, { SDL_CONTROLLER_BUTTON_A, K_B },
        { SDL_CONTROLLER_BUTTON_BACK, K_SELECT }, { SDL_CONTROLLER_BUTTON_START, K_START },
        { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, K_RIGHT }, { SDL_CONTROLLER_BUTTON_DPAD_LEFT, K_LEFT },
        { SDL_CONTROLLER_BUTTON_DPAD_UP, K_UP }, { SDL_CONTROLLER_BUTTON_DPAD_DOWN, K_DOWN },
        { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, K_R }, { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, K_L } };
    unsigned m = 0;
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++)
        if (SDL_GameControllerGetButton(gc, map[i].b))
            m |= 1u << map[i].bit;
    int x = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
    int y = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
    if (x > 16000) m |= 1u << K_RIGHT;
    if (x < -16000) m |= 1u << K_LEFT;
    if (y > 16000) m |= 1u << K_DOWN;
    if (y < -16000) m |= 1u << K_UP;
    if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) m |= 1u << K_L;
    if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) m |= 1u << K_R;
    return m;
}

/* ── status bar: a 5x7 font drawn with the renderer ──────────────────── */

static const struct { char c; unsigned char rows[7]; } FONT[] = {
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } }, { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } }, { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } }, { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } }, { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } }, { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { 'A', { 0x0E, 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11 } }, { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } }, { 'D', { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } }, { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F } }, { 'H', { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'I', { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } }, { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'M', { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 } }, { 'N', { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 } },
    { 'O', { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } }, { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } }, { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
    { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } }, { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } }, { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } },
    { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } }, { 'Y', { 0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04 } },
    { ':', { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 } }, { '-', { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 } },
    { '^', { 0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04 } }, { 'v', { 0x04, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04 } },
    { '<', { 0x00, 0x04, 0x08, 0x1F, 0x08, 0x04, 0x00 } }, { '>', { 0x00, 0x04, 0x02, 0x1F, 0x02, 0x04, 0x00 } },
};

/* Draw text at (x, y) in GBA pixels; returns the x after it. */
static int text(SDL_Renderer *ren, int x, int y, const char *s, Uint8 r, Uint8 g, Uint8 b) {
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    for (; *s; s++, x += 6) {
        for (unsigned i = 0; i < sizeof FONT / sizeof FONT[0]; i++) {
            if (FONT[i].c != *s)
                continue;
            for (int row = 0; row < 7; row++)
                for (int col = 0; col < 5; col++)
                    if (FONT[i].rows[row] & (0x10 >> col)) {
                        SDL_Rect px = { x + col, y + row, 1, 1 };
                        SDL_RenderFillRect(ren, &px);
                    }
        }
    }
    return x;
}

static void status_bar(SDL_Renderer *ren, int y, const char *state, unsigned when, unsigned events, unsigned held) {
    SDL_SetRenderDrawColor(ren, 16, 16, 24, 255);
    SDL_Rect bg = { 0, y, 240, BAR };
    SDL_RenderFillRect(ren, &bg);
    char buf[64];
    int red = !strcmp(state, "REC");
    int x = text(ren, 2, y + 2, state, red ? 255 : 255, red ? 60 : 200, red ? 60 : 0);
    snprintf(buf, sizeof buf, "FRAME %u", when);
    x = text(ren, x + 8, y + 2, buf, 220, 220, 220);
    snprintf(buf, sizeof buf, "INPUTS %u", events);
    text(ren, x + 8, y + 2, buf, 220, 220, 220);
    static const struct { const char *label; int bit; } buttons[] = {
        { "<", K_LEFT }, { "^", K_UP }, { "v", K_DOWN }, { ">", K_RIGHT }, { "A", K_A }, { "B", K_B },
        { "L", K_L }, { "R", K_R }, { "START", K_START }, { "SELECT", K_SELECT } };
    x = 2;
    for (unsigned i = 0; i < sizeof buttons / sizeof buttons[0]; i++) {
        int on = !(held & (1u << buttons[i].bit));
        x = text(ren, x, y + 11, buttons[i].label, on ? 255 : 70, on ? 220 : 70, on ? 0 : 90) + 6;
    }
}

/* ── state digests, as mgba_run prints them ──────────────────────────── */

static void digest(struct mCore *core, char *out, size_t n) {
    static const struct { const char *n; unsigned base, size; } dr[] = {
        { "ewram", 0x02000000, 0x40000 }, { "pal", 0x05000000, 0x400 },
        { "vram", 0x06000000, 0x18000 }, { "oam", 0x07000000, 0x400 } };
    size_t len = 0;
    for (unsigned r = 0; r < 4; r++) {
        unsigned long long h = 1469598103934665603ull;
        for (unsigned a = 0; a < dr[r].size; a += 4) {
            h ^= core->busRead32(core, dr[r].base + a);
            h *= 1099511628211ull;
        }
        len += (size_t)snprintf(out + len, n - len, "%s%s=%016llx", r ? " " : "", dr[r].n, h);
    }
}

static void *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = malloc(*size);
    if (fread(buf, 1, *size, f) != *size) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

static int write_file(const char *path, const void *buf, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(buf, 1, size, f) != size)
        return 0;
    return fclose(f) == 0;
}

int main(int argc, char **argv) {
    const char *rom = NULL, *out = NULL, *save = NULL;
    const char *bios = "gbarecomp/bios/gba_bios.bin";
    int scale = 3;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--save") && i + 1 < argc) save = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--bios") && i + 1 < argc) bios = argv[++i];
        else if (!rom) rom = argv[i];
        else if (!out) out = argv[i];
    }
    if (!rom || !out) {
        fprintf(stderr, "usage: %s ROM OUT.trace [--save START.sav] [--scale N] [--bios FILE]\n", argv[0]);
        return 2;
    }
    char base[1024], path[1100];
    snprintf(base, sizeof base, "%s", out);
    char *dot = strrchr(base, '.');
    if (dot && !strcmp(dot, ".trace"))
        *dot = 0;

    struct mLogger logger = { .log = quiet_log };
    mLogSetDefaultLogger(&logger);
    struct mCore *core = mCoreFind(rom);
    if (!core || !core->init(core)) {
        fprintf(stderr, "cannot create core\n");
        return 1;
    }
    mCoreInitConfig(core, NULL);
    struct mCoreOptions opts = { .useBios = true, .skipBios = true, .volume = 0x100 };
    mCoreConfigLoadDefaults(&core->config, &opts);
    mCoreLoadConfig(core);
    unsigned w, h;
    core->desiredVideoDimensions(core, &w, &h);
    color_t *fb = calloc((size_t)w * h, sizeof(color_t));
    core->setVideoBuffer(core, fb, w);
    if (!mCoreLoadFile(core, rom)) {
        fprintf(stderr, "cannot load %s\n", rom);
        return 1;
    }
    struct VFile *bvf = VFileOpen(bios, O_RDONLY);
    if (!bvf) {
        fprintf(stderr, "cannot open BIOS %s (run from the repository root or pass --bios)\n", bios);
        return 1;
    }
    core->loadBIOS(core, bvf, 0);

    /* The starting save: a private copy of --save, or a blank one. Either
     * way it is written next to the trace and loaded from memory, exactly
     * as the test scripts will load it. */
    size_t save_size = SAVE_SIZE;
    void *start = save ? read_file(save, &save_size) : NULL;
    if (save && !start) {
        fprintf(stderr, "cannot read %s\n", save);
        return 1;
    }
    if (!start) {
        start = malloc(SAVE_SIZE);
        memset(start, 0xFF, SAVE_SIZE);
    }
    snprintf(path, sizeof path, "%s.sav", base);
    if (!write_file(path, start, save_size)) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    core->loadSave(core, VFileMemChunk(start, save_size));

    core->setAudioBufferSize(core, 2048);
    blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), SAMPLE_RATE);
    blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), SAMPLE_RATE);
    core->reset(core);

    FILE *trace = fopen(out, "w");
    if (!trace) {
        fprintf(stderr, "cannot write %s\n", out);
        return 1;
    }
    fprintf(trace, "# gbarecomp-keyinput-v1\n# game_frame,keyinput_active_low (tools/record; start save %s%s)\n",
            save ? save : "blank", save ? "" : ", new game");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    signal(SIGINT, on_signal);      /* Ctrl+C or a closed terminal still finish the files */
    signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal);
    SDL_Window *win = SDL_CreateWindow("BN6 recorder", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       (int)w * scale, (int)(h + BAR) * scale, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(ren, (int)w, (int)h + BAR);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, (int)w, (int)h);
    SDL_AudioSpec want = { .freq = SAMPLE_RATE, .format = AUDIO_S16SYS, .channels = 2, .samples = 1024 }, have;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio)
        SDL_PauseAudioDevice(audio, 0);
    SDL_GameController *pad = NULL;
    for (int i = 0; i < SDL_NumJoysticks() && !pad; i++)
        if (SDL_IsGameController(i))
            pad = SDL_GameControllerOpen(i);

    printf("Recording to %s (starting save: %s.sav)\n", out, base);
    if (pad)
        printf("Controller: %s\n", SDL_GameControllerName(pad));
    printf("Keys: arrows, X=A, Z=B, A=L, S=R, Enter=Start, Backspace=Select; Tab=fast forward, P=pause, Esc=stop\n");

    const char *test_env = getenv("RECORD_TEST_FRAMES");
    unsigned test_frames = test_env ? (unsigned)atoi(test_env) : 0, last_digest = ~0u;
    unsigned rng = 12345, test_keys = 0;
    unsigned latched = 0x3FF, written = 0x3FF, prev_when = 0, events = 0, frames_run = 0;
    unsigned held = 0x3FF, when = 0;
    int first = 1, paused = 0, recording = 1;
    Uint64 next = SDL_GetPerformanceCounter();
    const double tick = SDL_GetPerformanceFrequency() / FPS;
    static short samples[8192];
    SDL_Rect game_rect = { 0, 0, (int)w, (int)h };
    for (int running = 1; running && !g_stop;) {
        if (test_frames && frames_run >= test_frames)
            break;
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                running = 0;
            else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
                if (ev.key.keysym.sym == SDLK_p) paused = !paused;
            } else if (ev.type == SDL_CONTROLLERDEVICEADDED && !pad)
                pad = SDL_GameControllerOpen(ev.cdevice.which);
        }
        const Uint8 *ks = SDL_GetKeyboardState(NULL);
        held = 0x3FF & ~(keyboard_keys(ks) | pad_keys(pad));
        int fast = ks[SDL_SCANCODE_TAB] || test_frames;
        if (paused) {
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, &game_rect);
            status_bar(ren, (int)h, "PAUSED", when, events, held);
            SDL_RenderPresent(ren);
            SDL_Delay(16);
            next = SDL_GetPerformanceCounter();
            continue;
        }
        if (test_frames) {      /* key changes at arbitrary hardware frames */
            rng = rng * 1103515245u + 12345u;
            if ((rng >> 16) % 5 == 0)
                test_keys = (rng >> 8) & 0xFB;  /* A B START RIGHT LEFT UP DOWN; never SELECT */
            held = 0x3FF & ~test_keys;
        }
        /* The same game-frame timeline as mgba_run's replay. */
        when = core->busRead16(core, core->busRead32(core, GAME_FRAME_ADDR));
        if (first || when != prev_when) {
            if (!first && when < prev_when && recording) {
                recording = 0;
                fprintf(stderr, "game frame counter went back (%u -> %u; soft reset?): "
                        "the trace ends here, press Esc\n", prev_when, when);
            }
            latched = held;
            if (recording && (first || latched != written)) {
                fprintf(trace, "%u,0x%03X\n", when, latched);
                fflush(trace);
                written = latched;
                events++;
            }
            prev_when = when;
            first = 0;
        }
        if (test_frames && when % 20 == 0 && when != last_digest) {
            char d[160];
            last_digest = when;
            digest(core, d, sizeof d);
            printf("digest game=%u hw=%u %s\n", when, frames_run, d);
        }
        core->setKeys(core, ~latched & 0x3FF);
        core->runFrame(core);
        frames_run++;

        if (!fast || frames_run % 8 == 0) {
            SDL_UpdateTexture(tex, NULL, fb, (int)(w * sizeof(color_t)));
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, &game_rect);
            status_bar(ren, (int)h, !recording ? "STOPPED" : fast ? "FAST" : "REC", when, events, held);
            SDL_RenderPresent(ren);
        }
        if (frames_run % 30 == 0) {
            char title[96];
            snprintf(title, sizeof title, "BN6 recorder - %s - frame %u - %u inputs",
                     recording ? "REC" : "STOPPED", when, events);
            SDL_SetWindowTitle(win, title);
        }

        blip_t *l = core->getAudioChannel(core, 0), *r = core->getAudioChannel(core, 1);
        int n = blip_samples_avail(l);
        if (n > (int)(sizeof samples / sizeof samples[0] / 2))
            n = sizeof samples / sizeof samples[0] / 2;
        blip_read_samples(l, samples, n, 1);
        blip_read_samples(r, samples + 1, n, 1);
        if (fast) {
            next = SDL_GetPerformanceCounter();
            continue;
        }
        if (audio && SDL_GetQueuedAudioSize(audio) < SAMPLE_RATE / 4 * 4)
            SDL_QueueAudio(audio, samples, (Uint32)n * 4);
        next += (Uint64)tick;
        Uint64 now = SDL_GetPerformanceCounter();
        if (next > now)
            SDL_Delay((Uint32)((next - now) * 1000 / SDL_GetPerformanceFrequency()));
        else if (now - next > 10 * tick)
            next = now;     /* fell far behind: don't race to catch up */
    }

    /* Where the recording ended, for tools/check_recording.sh. */
    char d[160];
    digest(core, d, sizeof d);
    fprintf(trace, "# end hw_frames=%u game_frame=%u %s%s\n", frames_run, prev_when, d,
            recording ? "" : " (recording stopped early: game frame counter went back)");
    fclose(trace);
    /* The game's save memory as it sees it (32 KB of SRAM at 0x0E000000;
     * libmgba 0.10's savedataClone returns garbage here). */
    unsigned char *sram = malloc(SAVE_SIZE);
    for (unsigned a = 0; a < SAVE_SIZE; a++)
        sram[a] = (unsigned char)core->busRead8(core, 0x0E000000 + a);
    snprintf(path, sizeof path, "%s.end.sav", base);
    if (write_file(path, sram, SAVE_SIZE))
        printf("Ending save: %s%s\n", path, memcmp(sram, start, SAVE_SIZE < save_size ? SAVE_SIZE : save_size)
               ? " (the game saved)" : " (unchanged)");
    else
        fprintf(stderr, "could not write the ending save %s\n", path);
    free(sram);
    free(start);
    printf("%u input changes recorded up to game frame %u (%u frames): %s\n", events, prev_when, frames_run, out);
    SDL_Quit();
    core->deinit(core);
    return 0;
}
