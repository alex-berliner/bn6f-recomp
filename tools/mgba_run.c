/*
 * mgba_run — headless libmgba runner for comparing the original and lifted ROMs.
 *
 *   mgba_run ROM [--frames N] [--input TRACE] [--shots DIR --every N]
 *                [--bios FILE] [--marker ADDR] [--dump-ram PREFIX]
 *
 * TRACE uses gbarecomp's keyinput format: "frame,0xKEYINPUT" (active low).
 * Screenshots are binary PPMs (DIR/frame_NNNNNN.ppm).
 */
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/gba/interface.h>
#include <mgba-util/vfs.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_verbose_log = 1;

static void log_cb(struct mLogger *logger, int category, enum mLogLevel level,
                   const char *format, va_list args) {
    (void)logger;
    const char *cat = mLogCategoryName(category);
    if (!g_verbose_log && level > mLOG_WARN)
        return;
    if (level & (mLOG_DEBUG | mLOG_STUB))
        return;
    if (cat && strcmp(cat, "GBA Debug") && level > mLOG_WARN && !(level & mLOG_FATAL))
        return;
    fprintf(stderr, "[%s] ", cat ? cat : "?");
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
}

typedef struct { unsigned frame; unsigned keyinput; } Ev;

int main(int argc, char **argv) {
    const char *rom = NULL, *input = NULL, *shots = NULL, *bios = NULL, *dump = NULL;
    unsigned frames = 600, every = 0;
    unsigned long marker = 0, guest = 0, game_frame_addr = 0, until_game_frame = 0, digest_every = 0;
    unsigned last_digest = ~0u;
    unsigned long shot_game = 0;
    int trace_pc = 0;
    unsigned long prof_from = 0, prof_steps = 0;
    unsigned step_from = 0, code_lo = 0x08800000, code_hi = 0x0A000000, arm_lo = 0, arm_hi = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames")) frames = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--input")) input = argv[++i];
        else if (!strcmp(argv[i], "--shots")) shots = argv[++i];
        else if (!strcmp(argv[i], "--every")) every = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--bios")) bios = argv[++i];
        else if (!strcmp(argv[i], "--marker")) marker = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--dump-ram")) dump = argv[++i];
        else if (!strcmp(argv[i], "--quiet")) g_verbose_log = 0;
        else if (!strcmp(argv[i], "--game-frame-addr")) game_frame_addr = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--until-game-frame")) until_game_frame = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--digest-every")) digest_every = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--shot-at-game-frame")) shot_game = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--guest")) guest = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--trace-pc")) trace_pc = 1;
        else if (!strcmp(argv[i], "--step-from")) step_from = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--profile")) { prof_from = strtoul(argv[++i], 0, 0); prof_steps = strtoul(argv[++i], 0, 0); }
        else if (!strcmp(argv[i], "--code-lo")) code_lo = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--code-hi")) code_hi = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--arm-range")) { arm_lo = strtoul(argv[++i], 0, 0); arm_hi = strtoul(argv[++i], 0, 0); }
        else rom = argv[i];
    }
    if (!rom) {
        fprintf(stderr, "usage: mgba_run ROM [options]\n");
        return 2;
    }

    struct mLogger logger = { .log = log_cb };
    mLogSetDefaultLogger(&logger);

    struct mCore *core = mCoreFind(rom);
    if (!core || !core->init(core)) {
        fprintf(stderr, "cannot create core\n");
        return 1;
    }
    mCoreInitConfig(core, NULL);
    struct mCoreOptions opts = { .useBios = bios != NULL, .skipBios = true, .volume = 0 };
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
    if (bios) {
        struct VFile *vf = VFileOpen(bios, O_RDONLY);
        if (vf) core->loadBIOS(core, vf, 0);
    }
    core->reset(core);

    Ev *evs = NULL;
    size_t nev = 0, cap = 0, evi = 0;
    if (input) {
        FILE *f = fopen(input, "r");
        char line[128];
        while (f && fgets(line, sizeof line, f)) {
            unsigned fr, k;
            if (line[0] == '#' || sscanf(line, "%u,0x%x", &fr, &k) != 2) continue;
            if (nev == cap) evs = realloc(evs, (cap = cap ? cap * 2 : 256) * sizeof *evs);
            evs[nev++] = (Ev){ fr, k };
        }
        if (f) fclose(f);
    }

    unsigned keyinput = 0x3FF;
    for (unsigned fr = 0; fr < frames; fr++) {
        /* Input timeline: hardware frames, or (with --game-frame-addr) the
         * game's own frame counter, so builds of different speed see the
         * same input at the same game state. */
        unsigned when = fr;
        if (game_frame_addr) {
            when = core->busRead16(core, core->busRead32(core, game_frame_addr));
            if (shot_game && shots && when >= shot_game) {
                char path[512];
                snprintf(path, sizeof path, "%s/game_%06lu.ppm", shots, shot_game);
                FILE *o = fopen(path, "wb");
                fprintf(o, "P6 %u %u 255\n", w, h);
                for (size_t i = 0; i < (size_t)w * h; i++) {
                    color_t c = fb[i];
                    fputc(c & 0xFF, o); fputc((c >> 8) & 0xFF, o); fputc((c >> 16) & 0xFF, o);
                }
                fclose(o);
                shot_game = 0;
            }
            if (until_game_frame && when >= until_game_frame) {
                printf("reached game frame %u at hardware frame %u\n", when, fr);
                frames = fr;
                break;
            }
        }
        while (evi < nev && evs[evi].frame <= when)
            keyinput = evs[evi++].keyinput;
        /* At each checkpoint game frame (start of the frame, before input is
         * read), print a digest of EWRAM, palette, VRAM and OAM. */
        if (digest_every && game_frame_addr && when % digest_every == 0 && when != last_digest) {
            last_digest = when;
            static const struct { const char *n; unsigned base, size; } dr[] = {
                { "ewram", 0x02000000, 0x40000 }, { "pal", 0x05000000, 0x400 },
                { "vram", 0x06000000, 0x18000 }, { "oam", 0x07000000, 0x400 } };
            printf("digest game=%u hw=%u", when, fr);
            for (unsigned r = 0; r < 4; r++) {
                unsigned long long h = 1469598103934665603ull;
                for (unsigned a = 0; a < dr[r].size; a += 4) {
                    h ^= core->busRead32(core, dr[r].base + a);
                    h *= 1099511628211ull;
                }
                printf(" %s=%016llx", dr[r].n, h);
            }
            printf("\n");
        }
        core->setKeys(core, ~keyinput & 0x3FF);
        if (prof_steps && fr == prof_from) {
            /* Cycle profile: attribute every instruction's cycles (wait
             * states included) to its PC; aggregated per halfword. */
            static uint32_t rom_cyc[0x01000000 / 2], ram_cyc[0x8000 / 2], bios_cyc[0x4000 / 2];
            uint64_t t0 = mTimingGlobalTime(core->timing);
            for (unsigned long n = 0; n < prof_steps; n++) {
                int32_t pc = 0;
                core->readRegister(core, "pc", &pc);
                core->step(core);
                uint64_t t1 = mTimingGlobalTime(core->timing);
                uint32_t d = (uint32_t)(t1 - t0);
                t0 = t1;
                uint32_t u = (uint32_t)pc;
                if (u >= 0x08000000u && u < 0x09000000u) rom_cyc[(u - 0x08000000u) >> 1] += d;
                else if (u >= 0x03000000u && u < 0x03008000u) ram_cyc[(u - 0x03000000u) >> 1] += d;
                else if (u < 0x4000) bios_cyc[u >> 1] += d;
            }
            FILE *pf = fopen("profile.txt", "w");
            for (uint32_t i = 0; i < 0x01000000 / 2; i++) if (rom_cyc[i]) fprintf(pf, "%08x %u\n", 0x08000000u + i * 2, rom_cyc[i]);
            for (uint32_t i = 0; i < 0x8000 / 2; i++) if (ram_cyc[i]) fprintf(pf, "%08x %u\n", 0x03000000u + i * 2, ram_cyc[i]);
            for (uint32_t i = 0; i < 0x4000 / 2; i++) if (bios_cyc[i]) fprintf(pf, "%08x %u\n", i * 2, bios_cyc[i]);
            fclose(pf);
            goto done;
        }
        if (step_from && fr >= step_from) {
            /* Step instruction-by-instruction; stop when the PC leaves host code. */
            unsigned ring[128], rp = 0, prev = 0;
            for (unsigned long n = 0; n < 50000000UL; n++) {
                int32_t pc = 0;
                core->readRegister(core, "pc", &pc);
                unsigned u = (unsigned)pc;
                if (u != prev + 2 && u != prev + 4) ring[rp++ & 127] = (prev << 0), ring[rp++ & 127] = u;
                prev = u;
                int ok = (u < 0x4000) || (u >= code_lo && u < code_hi);
                if (u >= 0x08000000 && !(u >= arm_lo && u < arm_hi)) {
                    int32_t cpsr = 0; core->readRegister(core, "cpsr", &cpsr);
                    if (!(cpsr & 0x20)) ok = 0;   /* ARM state outside the IRQ entry */
                }
                if (!ok) {
                    printf("PC left host code at step %lu: pc=%08x\nlast PCs:", n, u);
                    for (unsigned k = 0; k < 128; k++) printf(" %08x", ring[(rp + k) & 127]);
                    printf("\n");
                    goto done;
                }
                core->step(core);
            }
            printf("no escape within step budget\n");
            goto done;
        }
        core->runFrame(core);
        if (trace_pc) {
            int32_t pc = 0, lr = 0;
            core->readRegister(core, "pc", &pc);
            core->readRegister(core, "lr", &lr);
            printf("frame %u: pc=%08x lr=%08x", fr, (unsigned)pc, (unsigned)lr);
            if (guest) printf(" glr=%08x gsp=%08x", core->busRead32(core, guest + 56), core->busRead32(core, guest + 52));
            printf("\n");
        }
        if (marker && core->busRead32(core, marker) == 0xDEADC0DE) {
            unsigned v = core->busRead32(core, marker + 4);
            unsigned lr = core->busRead32(core, marker + 12);
            printf("FATAL at frame %u: value=%#010x lr=%#010x\n", fr, v, lr);
            frames = fr + 1;
            break;
        }
        if (shots && ((every && (fr + 1) % every == 0) || fr + 1 == frames)) {
            char path[512];
            snprintf(path, sizeof path, "%s/frame_%06u.ppm", shots, fr + 1);
            FILE *o = fopen(path, "wb");
            fprintf(o, "P6 %u %u 255\n", w, h);
            for (size_t i = 0; i < (size_t)w * h; i++) {
                color_t c = fb[i];
#ifdef COLOR_16_BIT
                unsigned r = (c & 0x1F) << 3, g = ((c >> 5) & 0x3F) << 2, b = ((c >> 11) & 0x1F) << 3;
#else
                unsigned r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF;
#endif
                fputc(r, o); fputc(g, o); fputc(b, o);
            }
            fclose(o);
        }
    }
done:
    if (dump) {
        static const struct { const char *name; unsigned base, size; } regions[] = {
            { "ewram", 0x02000000, 0x40000 }, { "iwram", 0x03000000, 0x8000 },
            { "vram", 0x06000000, 0x18000 }, { "pal", 0x05000000, 0x400 }, { "oam", 0x07000000, 0x400 },
        };
        for (unsigned r = 0; r < 5; r++) {
            char path[512];
            snprintf(path, sizeof path, "%s.%s.bin", dump, regions[r].name);
            FILE *o = fopen(path, "wb");
            for (unsigned a = 0; a < regions[r].size; a += 4) {
                unsigned v = core->busRead32(core, regions[r].base + a);
                fwrite(&v, 4, 1, o);
            }
            fclose(o);
        }
    }
    {
        static const char *regs[] = { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r12", "sp", "lr", "pc", "cpsr" };
        printf("host:");
        for (unsigned i = 0; i < sizeof regs / sizeof *regs; i++) {
            int32_t v = 0;
            core->readRegister(core, regs[i], &v);
            printf(" %s=%08x", regs[i], (unsigned)v);
        }
        printf("\n");
    }
    if (guest) {
        printf("guest:");
        for (unsigned i = 0; i < 21; i++)
            printf(" %08x", core->busRead32(core, guest + i * 4));
        printf("\n");
    }
    printf("ran %u frames\n", frames);
    core->deinit(core);
    return 0;
}
