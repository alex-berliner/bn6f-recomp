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
#include <mgba/core/serialize.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/gba/interface.h>
#include <mgba-util/vfs.h>

#include <dirent.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_verbose_log = 1;
static struct mCore *g_core;
static unsigned long g_illegal;   /* illegal opcodes executed; the first few are logged with registers */

static void log_cb(struct mLogger *logger, int category, enum mLogLevel level,
                   const char *format, va_list args) {
    (void)logger;
    const char *cat = mLogCategoryName(category);
    if (g_core && !strncmp(format, "Illegal opcode", 14)) {
        if (++g_illegal > 16)
            return;
        int32_t pc = 0, lr = 0, cpsr = 0;
        g_core->readRegister(g_core, "pc", &pc);
        g_core->readRegister(g_core, "lr", &lr);
        g_core->readRegister(g_core, "cpsr", &cpsr);
        fprintf(stderr, "[%s] ", cat ? cat : "?");
        vfprintf(stderr, format, args);
        fprintf(stderr, " (pc=%08x lr=%08x cpsr=%08x)\n", (uint32_t)pc, (uint32_t)lr, (uint32_t)cpsr);
        return;
    }
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

static int snap_filter_ok(struct mCore *core, int reg, uint32_t off, uint32_t mask, uint32_t val) {
    if (reg < 0) return 1;
    char name[16];
    snprintf(name, sizeof name, "r%d", reg & 15);
    int32_t v = 0;
    core->readRegister(core, name, &v);
    return (core->busRead32(core, (uint32_t)v + off) & mask) == val;
}

/* Snapshot file: "BN6S", version, r0-r15 (r15 = the instruction's address),
 * cpsr, then guest memory in cport/host/memmap.c region order. */
static void save_snapshot_named(struct mCore *core, const char *dir, const char *name, uint32_t pc, uint32_t cpsr);
static void save_snapshot(struct mCore *core, const char *dir, unsigned long n, uint32_t pc, uint32_t cpsr) {
    char name[32];
    snprintf(name, sizeof name, "%04lu", n);
    save_snapshot_named(core, dir, name, pc, cpsr);
}
static void save_snapshot_named(struct mCore *core, const char *dir, const char *name, uint32_t pc, uint32_t cpsr) {
    static const struct { unsigned base, size; } rg[] = {
        { 0x02000000, 0x40000 }, { 0x03000000, 0x8000 }, { 0x04000000, 0x400 }, { 0x05000000, 0x400 },
        { 0x06000000, 0x18000 }, { 0x07000000, 0x400 }, { 0x0E000000, 0x8000 },
    };
    char path[512];
    snprintf(path, sizeof path, "%s/snap_%s.bin", dir, name);
    FILE *o = fopen(path, "wb");
    if (!o) { perror(path); return; }
    uint32_t hdr[2 + 16 + 1] = { 0x53364E42u, 1 };
    for (int r = 0; r < 15; r++) {
        char name[8];
        snprintf(name, sizeof name, "r%d", r);
        int32_t v = 0;
        core->readRegister(core, name, &v);
        hdr[2 + r] = (uint32_t)v;
    }
    hdr[2 + 15] = pc;
    hdr[2 + 16] = cpsr;
    fwrite(hdr, sizeof hdr, 1, o);
    for (unsigned r = 0; r < sizeof rg / sizeof rg[0]; r++)
        for (unsigned a = 0; a < rg[r].size; a++) {
            uint8_t b = core->busRead8(core, rg[r].base + a);
            fputc(b, o);
        }
    fclose(o);
}

static uint8_t snapf_bits[0x02000000 / 16];
static uint8_t snapf_count[0x400000];

#define NHIT 4096
static struct { uint32_t r0, lr, n; } hits[NHIT];
static void count_hit(uint32_t r0, uint32_t lr) {
    uint32_t h = (r0 * 2654435761u ^ lr * 40503u) & (NHIT - 1);
    for (unsigned k = 0; k < NHIT; k++, h = (h + 1) & (NHIT - 1)) {
        if (hits[h].n && hits[h].r0 == r0 && hits[h].lr == lr) { hits[h].n++; return; }
        if (!hits[h].n) { hits[h].r0 = r0; hits[h].lr = lr; hits[h].n = 1; return; }
    }
}
static int hit_cmp(const void *a, const void *b) {
    const uint32_t na = ((const uint32_t *)a)[2], nb = ((const uint32_t *)b)[2];
    return na < nb ? 1 : na > nb ? -1 : 0;
}
static void count_dump(void) {
    qsort(hits, NHIT, sizeof hits[0], hit_cmp);
    for (unsigned k = 0; k < 60 && hits[k].n; k++)
        printf("count %8u r0=%08x lr=%08x\n", hits[k].n, hits[k].r0, hits[k].lr);
}

static uint64_t pg_rom[0x01000000 / 2], pg_ram[0x8000 / 2], pg_ewram[0x40000 / 2], pg_bios[0x4000 / 2];

int main(int argc, char **argv) {
    const char *rom = NULL, *input = NULL, *shots = NULL, *bios = NULL, *dump = NULL;
    unsigned frames = 600, every = 0, bios_intro = 0;
    unsigned long marker = 0, guest = 0, game_frame_addr = 0, until_game_frame = 0, digest_every = 0;
    unsigned long stall_frames = 3600;   /* with --until-game-frame: stop after a minute without a game frame */
    unsigned long dump_at[64]; unsigned ndump_at = 0; unsigned long long dumped_at = 0; const char *dump_at_prefix = NULL;
    /* --profile-game G0 G1: cycle profile from game frame G0 to G1 (needs
     * --game-frame-addr); input keeps following the trace while profiling. */
    unsigned long pg_from = 0, pg_to = 0;
    /* --count-entry ADDR: while profiling, histogram (r0, lr) at each
     * execution of ADDR (e.g. a runtime function's entry). */
    unsigned long count_pc = 0;
    /* --load-state FILE / --save-state FILE: start from / end with an mGBA
     * save state (for building input traces step by step). */
    const char *load_state = NULL, *save_state = NULL;
    /* --save FILE: start with this save memory (a private copy; the file is
     * never written). Traces recorded from a save carry it as NAME.sav. */
    const char *save_file = NULL;
    /* --call-snaps DIR MAXSTEPS: CPU oracle. For every DIR/snap_*.bin that
     * has no .exit.bin: load its memory and registers into the core (IRQs
     * off), run from its PC until the function returns to its LR at the
     * entry SP, and save the result as snap_*.exit.bin (no exit file if it
     * does not return within MAXSTEPS instructions). */
    const char *call_dir = NULL;
    unsigned long call_steps = 0;
    /* --dump-save FILE: write the save memory when the run ends (the
     * --save contents if the game never accessed it). --final-digest: print
     * a "final" digest line (as --digest-every) after the last frame. */
    const char *dump_save = NULL;
    int final_digest = 0;
    void *save_data = NULL;
    /* --poke-at PC G "ADDR=VALUE/SIZE,...": write memory once, when the
     * instruction at PC is about to run at game frame >= G (a fixed point in
     * the game's frame loop, e.g. main_awaitFrame's entry, so the original
     * and a port see the change at the same moment). Needs --profile-game
     * covering G (stepping). VALUE may be "*ADDR" (read at the write's
     * size) or "*ADDR^X" (that, XOR X). */
    unsigned long poke_pc = 0, poke_g = 0;
    const char *poke_spec = NULL;
    int poked = 0;
    /* --trace-branches FILE: while stepping (--profile-game window), write
     * every non-sequential PC (branch target) with the game frame. */
    FILE *branch_trace = NULL;
    uint32_t prev_pc = 0;
    /* --watch ADDR LEN: print LEN bytes at ADDR whenever the game frame
     * counter changes (with --game-frame-addr). */
    unsigned long watch_addr = 0, watch_len = 0, watch_last = ~0ul;
    /* --snap ADDR DIR MAX EVERY: while profiling, save the machine state
     * (registers + guest memory) at every EVERY-th execution of ADDR, up to
     * MAX files, for replay by cport/host/difftest -S DIR. */
    unsigned long snap_pc = 0, snap_max = 0, snap_every = 1, snap_hits = 0, snap_saved = 0;
    const char *snap_dir = NULL;
    /* --snap-filter R OFF MASK VAL: only count hits where
     * (mem32[rR + OFF] & MASK) == VAL (e.g. "this music player is playing"). */
    int snapf_reg = -1;
    /* --snap-exit: also save the state when each captured call returns
     * (snap_NNNN.exit.bin), for checking lifted code against the original. */
    int snap_exit = 0;
    const char *snapf_dir = NULL;
    unsigned long snapf_max = 0;
    struct { uint32_t lr, sp, func; unsigned long step; int irq; char name[32]; } pend[256];
    unsigned long steps_total = 0;
    int npend = 0;
    uint32_t pending_lr = 0, pending_sp = 0;
    unsigned long pending_n = 0;
    int pending = 0;
    unsigned long snapf_off = 0, snapf_mask = 0, snapf_val = 0;
    int pg_active = 0, pg_continue = 0, pg_done = 0;
    uint64_t pg_total = 0;
    unsigned pg_hw0 = 0;
    unsigned last_digest = ~0u;
    unsigned long shot_game = 0;
    int trace_pc = 0;
    unsigned long prof_from = 0, prof_steps = 0;
    unsigned step_from = 0, code_lo = 0x08800000, code_hi = 0x0A000000, arm_lo = 0, arm_hi = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames")) frames = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--input")) input = argv[++i];
        else if (!strcmp(argv[i], "--shots")) shots = argv[++i];
        else if (!strcmp(argv[i], "--bios-intro")) bios_intro = 1;   /* boot through the BIOS logo, as hardware does */
        else if (!strcmp(argv[i], "--every")) every = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--bios")) bios = argv[++i];
        else if (!strcmp(argv[i], "--marker")) marker = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--dump-ram")) dump = argv[++i];
        else if (!strcmp(argv[i], "--dump-at-game")) {   /* G1,G2,... PREFIX: palette+VRAM+OAM at those game frames */
            for (char *t = strtok(argv[++i], ","); t && ndump_at < 64; t = strtok(NULL, ","))
                dump_at[ndump_at++] = strtoul(t, 0, 0);
            dump_at_prefix = argv[++i];
        }
        else if (!strcmp(argv[i], "--quiet")) g_verbose_log = 0;
        else if (!strcmp(argv[i], "--game-frame-addr")) game_frame_addr = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--until-game-frame")) until_game_frame = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--stall-frames")) stall_frames = strtoul(argv[++i], 0, 0);   /* 0: never give up */
        else if (!strcmp(argv[i], "--digest-every")) digest_every = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--shot-at-game-frame")) shot_game = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--guest")) guest = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--trace-pc")) trace_pc = 1;
        else if (!strcmp(argv[i], "--step-from")) step_from = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--count-entry")) count_pc = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--load-state")) load_state = argv[++i];
        else if (!strcmp(argv[i], "--save")) save_file = argv[++i];
        else if (!strcmp(argv[i], "--call-snaps")) { call_dir = argv[++i]; call_steps = strtoul(argv[++i], 0, 0); }
        else if (!strcmp(argv[i], "--dump-save")) dump_save = argv[++i];
        else if (!strcmp(argv[i], "--final-digest")) final_digest = 1;
        else if (!strcmp(argv[i], "--poke-at")) {
            poke_pc = strtoul(argv[++i], 0, 0); poke_g = strtoul(argv[++i], 0, 0); poke_spec = argv[++i];
        }
        else if (!strcmp(argv[i], "--trace-branches")) branch_trace = fopen(argv[++i], "w");
        else if (!strcmp(argv[i], "--watch")) { watch_addr = strtoul(argv[++i], 0, 0); watch_len = strtoul(argv[++i], 0, 0); }
        else if (!strcmp(argv[i], "--save-state")) save_state = argv[++i];
        else if (!strcmp(argv[i], "--snap-exit")) snap_exit = 1;
        else if (!strcmp(argv[i], "--snap-funcs")) {
            /* FILE DIR MAXPER: entry+exit states of the first MAXPER calls of
             * every function listed (hex addresses) */
            FILE *f = fopen(argv[++i], "r");
            snapf_dir = argv[++i];
            snapf_max = strtoul(argv[++i], 0, 0);
            char line[64];
            while (f && fgets(line, sizeof line, f)) {
                uint32_t a = strtoul(line, 0, 16) & ~1u;
                if (a >= 0x08000000u && a < 0x0A000000u) snapf_bits[(a - 0x08000000u) >> 4] |= 1u << ((a >> 1) & 7);
            }
            if (f) fclose(f);
        }
        else if (!strcmp(argv[i], "--snap-filter")) {
            snapf_reg = atoi(argv[++i]); snapf_off = strtoul(argv[++i], 0, 0);
            snapf_mask = strtoul(argv[++i], 0, 0); snapf_val = strtoul(argv[++i], 0, 0);
        }
        else if (!strcmp(argv[i], "--snap")) {
            snap_pc = strtoul(argv[++i], 0, 0); snap_dir = argv[++i];
            snap_max = strtoul(argv[++i], 0, 0); snap_every = strtoul(argv[++i], 0, 0);
        }
        else if (!strcmp(argv[i], "--step-window")) { pg_from = strtoul(argv[++i], 0, 0); pg_to = strtoul(argv[++i], 0, 0); pg_continue = 1; }
        else if (!strcmp(argv[i], "--profile-game")) { pg_from = strtoul(argv[++i], 0, 0); pg_to = strtoul(argv[++i], 0, 0); }
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
    g_core = core;
    if (!core || !core->init(core)) {
        fprintf(stderr, "cannot create core\n");
        return 1;
    }
    mCoreInitConfig(core, NULL);
    struct mCoreOptions opts = { .useBios = bios != NULL, .skipBios = !bios_intro, .volume = 0 };
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
    if (save_file) {
        FILE *f = fopen(save_file, "rb");
        if (!f) {
            fprintf(stderr, "cannot read %s\n", save_file);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        size_t n = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        void *buf = malloc(n);
        if (fread(buf, 1, n, f) != n) {
            fprintf(stderr, "cannot read %s\n", save_file);
            return 1;
        }
        fclose(f);
        core->loadSave(core, VFileMemChunk(buf, n));
        save_data = buf;
    }
    if (bios) {
        struct VFile *vf = VFileOpen(bios, O_RDONLY);
        if (vf) core->loadBIOS(core, vf, 0);
    }
    core->reset(core);
    if (load_state) {   /* raw core state (core->saveState) */
        size_t n = core->stateSize(core);
        void *buf = malloc(n);
        FILE *f = fopen(load_state, "rb");
        if (!f || fread(buf, 1, n, f) != n || !core->loadState(core, buf)) {
            fprintf(stderr, "cannot load state %s\n", load_state);
            return 1;
        }
        fclose(f);
        free(buf);
    }

    if (call_dir) {
        DIR *d = opendir(call_dir);
        struct dirent *de;
        unsigned done = 0, timeouts = 0;
        static const struct { unsigned base, size; } rg[] = {
            { 0x02000000, 0x40000 }, { 0x03000000, 0x8000 }, { 0x04000000, 0x400 }, { 0x05000000, 0x400 },
            { 0x06000000, 0x18000 }, { 0x07000000, 0x400 }, { 0x0E000000, 0x8000 },
        };
        while (d && (de = readdir(d))) {
            size_t ln = strlen(de->d_name);
            if (strncmp(de->d_name, "snap_", 5) || ln < 9 || strcmp(de->d_name + ln - 4, ".bin") ||
                (ln > 9 && !strcmp(de->d_name + ln - 9, ".exit.bin")))
                continue;
            char path[600], exitp[600], name[300];
            snprintf(path, sizeof path, "%s/%s", call_dir, de->d_name);
            snprintf(name, sizeof name, "%.*s.exit", (int)(ln - 9), de->d_name + 5);
            snprintf(exitp, sizeof exitp, "%s/snap_%s.bin", call_dir, name);
            if (access(exitp, F_OK) == 0) continue;
            FILE *f = fopen(path, "rb");
            uint32_t hdr[19];
            if (!f || fread(hdr, sizeof hdr, 1, f) != 1 || hdr[0] != 0x53364E42u) { if (f) fclose(f); continue; }
            for (unsigned r = 0; r < sizeof rg / sizeof rg[0]; r++) {
                for (unsigned a = 0; a < rg[r].size; a += 4) {
                    uint32_t w;
                    if (fread(&w, 4, 1, f) != 1) break;
                    if (rg[r].base == 0x04000000) {      /* I/O: only the display registers (no side effects) */
                        if (a < 0x58) {
                            core->busWrite16(core, rg[r].base + a, (uint16_t)w);
                            core->busWrite16(core, rg[r].base + a + 2, (uint16_t)(w >> 16));
                        }
                        continue;
                    }
                    if (rg[r].base == 0x0E000000) {
                        for (int b = 0; b < 4; b++) core->busWrite8(core, rg[r].base + a + b, (uint8_t)(w >> (8 * b)));
                    } else {
                        core->busWrite32(core, rg[r].base + a, w);
                    }
                }
            }
            fclose(f);
            {   /* the keys the snapshot's KEYINPUT shows (read-only: set through the core) */
                FILE *kf = fopen(path, "rb");
                uint16_t ki = 0x3FF;
                if (kf && fseek(kf, 76 + 0x40000 + 0x8000 + 0x130, SEEK_SET) == 0 && fread(&ki, 2, 1, kf) == 1)
                    core->setKeys(core, ~ki & 0x3FF);
                if (kf) fclose(kf);
            }
            core->busWrite16(core, 0x04000208, 0);   /* IME off: no interrupts during the call */
            core->busWrite16(core, 0x04000200, 0);
            for (int r = 0; r < 15; r++) {
                char rn[8];
                snprintf(rn, sizeof rn, "r%d", r);
                int32_t v = (int32_t)hdr[2 + r];
                core->writeRegister(core, rn, &v);
            }
            /* banked stacks as the game's startup sets them (GameEntryPoint):
             * a BIOS call pushes its frame on the SVC stack */
            static const struct { int32_t mode, sp; } banks[] = { { 0x93, 0x03007FE0 }, { 0x92, 0x03007F60 } };
            for (int b = 0; b < 2; b++) {
                int32_t m = banks[b].mode, v = banks[b].sp;
                core->writeRegister(core, "cpsr", &m);
                core->writeRegister(core, "sp", &v);
            }
            int32_t cpsr = (int32_t)(hdr[18] | 0x80), pcv = (int32_t)hdr[17];
            core->writeRegister(core, "cpsr", &cpsr);
            {   /* the system-mode registers survive the mode switches */
                int32_t v = (int32_t)hdr[2 + 13];
                core->writeRegister(core, "sp", &v);
                v = (int32_t)hdr[2 + 14];
                core->writeRegister(core, "lr", &v);
            }
            core->writeRegister(core, "pc", &pcv);
            if (getenv("CALL_DEBUG")) {
                for (int r = 0; r < 15; r++) {
                    char rn[8]; int32_t v = 0;
                    snprintf(rn, sizeof rn, "r%d", r);
                    core->readRegister(core, rn, &v);
                    if ((uint32_t)v != hdr[2 + r]) printf("  %s: wrote %08x read %08x\n", rn, hdr[2 + r], (uint32_t)v);
                }
                int32_t v = 0; core->readRegister(core, "pc", &v); printf("  pc: wrote %08x read %08x\n", hdr[17], (uint32_t)v);
                core->readRegister(core, "cpsr", &v); printf("  cpsr: wrote %08x read %08x\n", hdr[18] | 0x80, (uint32_t)v);
                int32_t m = 0x93; core->writeRegister(core, "cpsr", &m); core->readRegister(core, "sp", &v);
                printf("  svc sp: %08x\n", (uint32_t)v);
                m = (int32_t)(hdr[18] | 0x80); core->writeRegister(core, "cpsr", &m); core->readRegister(core, "sp", &v);
                printf("  sys sp: %08x (want %08x)\n", (uint32_t)v, hdr[2 + 13]);
            }
            uint32_t lr = hdr[2 + 14] & ~1u, sp0 = hdr[2 + 13];
            unsigned long n;
            int32_t pc = 0, sp = 0;
            for (n = 0; n < call_steps; n++) {
                int32_t c1 = 0;
                core->readRegister(core, "pc", &pc);
                core->readRegister(core, "cpsr", &c1);
                core->readRegister(core, "sp", &sp);
                pc -= (c1 & 0x20) ? 2 : 4;      /* mGBA pipeline PC -> next instruction to execute */
                if (((uint32_t)pc & ~1u) == lr && (uint32_t)sp == sp0) break;
                core->step(core);
            }
            if (n < call_steps) {
                int32_t c2 = 0;
                core->readRegister(core, "cpsr", &c2);
                save_snapshot_named(core, call_dir, name, (uint32_t)pc, (uint32_t)c2);
                done++;
            } else {
                timeouts++;
            }
        }
        if (d) closedir(d);
        printf("call-snaps: %u returned, %u did not return within %lu steps\n", done, timeouts, call_steps);
        core->deinit(core);
        return 0;
    }

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
    unsigned stall_game = ~0u, stall_since = 0;
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
            /* A game that stops counting frames (crashed, or hung) would
             * otherwise run on to --frames. */
            if (when != stall_game) {
                stall_game = when;
                stall_since = fr;
            } else if (until_game_frame && stall_frames && fr - stall_since >= stall_frames) {
                printf("stalled: game frame %u since hardware frame %u\n", when, stall_since);
                frames = fr;
                break;
            }
        }
        if (watch_len && game_frame_addr && when != watch_last) {
            watch_last = when;
            printf("watch game=%u hw=%u", when, fr);
            for (unsigned long k = 0; k < watch_len; k++)
                printf(" %02x", core->busRead8(core, watch_addr + k));
            printf("\n");
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
        /* --dump-at-game: the bytes the digest hashes (palette, VRAM, OAM, in
         * that order), at the same moment as a digest would be taken. */
        for (unsigned k = 0; k < ndump_at && game_frame_addr; k++) {
            if ((dumped_at >> k) & 1 || when < dump_at[k])
                continue;
            dumped_at |= 1ull << k;
            char path[512];
            snprintf(path, sizeof path, "%s_%lu.bin", dump_at_prefix, dump_at[k]);
            FILE *o = fopen(path, "wb");
            static const unsigned base[3] = { 0x05000000, 0x06000000, 0x07000000 }, size[3] = { 0x400, 0x18000, 0x400 };
            for (unsigned r = 0; r < 3; r++)
                for (unsigned a = 0; a < size[r]; a += 4) {
                    uint32_t v = core->busRead32(core, base[r] + a);
                    fwrite(&v, 4, 1, o);
                }
            fclose(o);
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
        if (pg_to && game_frame_addr && !pg_active && !pg_done && when >= pg_from && when < pg_to) {
            pg_active = 1;
            pg_hw0 = fr;
        }
        if (pg_active && when >= pg_to && pg_continue) {
            pg_active = 0;          /* --step-window: back to whole frames */
            pg_done = 1;
        }
        if (pg_active && when >= pg_to) {
            FILE *pf = fopen("profile.txt", "w");
            fprintf(pf, "# game %lu..%lu hw %u..%u cycles %llu\n", pg_from, pg_to, pg_hw0, fr,
                    (unsigned long long)pg_total);
            for (uint32_t i = 0; i < 0x01000000 / 2; i++) if (pg_rom[i]) fprintf(pf, "%08x %llu\n", 0x08000000u + i * 2, (unsigned long long)pg_rom[i]);
            for (uint32_t i = 0; i < 0x8000 / 2; i++) if (pg_ram[i]) fprintf(pf, "%08x %llu\n", 0x03000000u + i * 2, (unsigned long long)pg_ram[i]);
            for (uint32_t i = 0; i < 0x40000 / 2; i++) if (pg_ewram[i]) fprintf(pf, "%08x %llu\n", 0x02000000u + i * 2, (unsigned long long)pg_ewram[i]);
            for (uint32_t i = 0; i < 0x4000 / 2; i++) if (pg_bios[i]) fprintf(pf, "%08x %llu\n", i * 2, (unsigned long long)pg_bios[i]);
            fclose(pf);
            printf("profiled game frames %lu..%lu (hw %u..%u)\n", pg_from, pg_to, pg_hw0, fr);
            if (count_pc) count_dump();
            break;
        }
        /* a pending --poke-at steps its frame on its own */
        int poke_step = poke_spec && !poked && game_frame_addr && when + 1 >= poke_g;
        if (pg_active || poke_step) {
            /* step one hardware frame, attributing cycles to the PC */
            uint32_t f0 = core->frameCounter(core);
            uint64_t t0 = mTimingGlobalTime(core->timing);
            while (core->frameCounter(core) == f0) {
                int32_t pc = 0, cpsr = 0;
                core->readRegister(core, "pc", &pc);
                core->readRegister(core, "cpsr", &cpsr);
                pc -= (cpsr & 0x20) ? 2 : 4;      /* mGBA pipeline PC -> next instruction to execute */
                if (poke_spec && !poked && when >= poke_g && ((uint32_t)pc & ~1u) == (poke_pc & ~1u)) {
                    static char spec[16384];
                    snprintf(spec, sizeof spec, "%s", poke_spec);
                    for (char *t = strtok(spec, ","); t; t = strtok(NULL, ",")) {
                        uint32_t a = strtoul(t, &t, 0), v = 0, sz = 4;
                        if (*t == '=') t++;
                        char *slash = strchr(t, '/');
                        if (slash) sz = strtoul(slash + 1, NULL, 0);
                        if (*t == '*') {    /* *SRC[^XOR]: read SRC at the write's size */
                            uint32_t src = strtoul(t + 1, &t, 0);
                            v = sz == 1 ? core->busRead8(core, src) : sz == 2 ? core->busRead16(core, src)
                                                                       : core->busRead32(core, src);
                            if (*t == '^') v ^= strtoul(t + 1, &t, 0);
                        } else {
                            v = strtoul(t, &t, 0);
                        }
                        if (sz == 1) core->busWrite8(core, a, v);
                        else if (sz == 2) core->busWrite16(core, a, v);
                        else core->busWrite32(core, a, v);
                        printf("poke game=%u %08x=%x/%u\n", when, a, v, sz);
                    }
                    poked = 1;
                }
                if (branch_trace && (uint32_t)pc != prev_pc + 2 && (uint32_t)pc != prev_pc + 4)
                    fprintf(branch_trace, "%u %08x\n", when, (uint32_t)pc);
                prev_pc = (uint32_t)pc;
                steps_total++;
                if (snapf_dir) {
                    uint32_t u = (uint32_t)pc & ~1u;
                    /* calls that do not come back to LR soon (e.g. functions
                     * that return past an inline immediate) are dropped */
                    while (npend && steps_total - pend[0].step > 2000000) {
                        memmove(&pend[0], &pend[1], (size_t)(npend - 1) * sizeof pend[0]);
                        npend--;
                    }
                    /* returns: innermost first; an outer return also drops
                     * calls that unwound without returning */
                    /* an interrupt during a captured call would show up as
                     * differences: such captures are discarded */
                    if ((cpsr & 0x1F) == 0x12)
                        for (int k = 0; k < npend; k++)
                            pend[k].irq = 1;
                    for (int k = npend - 1; k >= 0; k--)
                        if (u == pend[k].lr) {
                            int32_t sp = 0;
                            core->readRegister(core, "sp", &sp);
                            if ((uint32_t)sp == pend[k].sp) {
                                char nm[48];
                                if (!pend[k].irq) {
                                    snprintf(nm, sizeof nm, "%s.exit", pend[k].name);
                                    save_snapshot_named(core, snapf_dir, nm, u, (uint32_t)cpsr);
                                } else {
                                    char path[600];
                                    snprintf(path, sizeof path, "%s/snap_%s.bin", snapf_dir, pend[k].name);
                                    remove(path);
                                    snapf_count[(pend[k].func - 0x08000000u) >> 1 & 0x3FFFFF]--;
                                }
                                npend = k;
                                break;
                            }
                        }
                    if (u >= 0x08000000u && u < 0x0A000000u &&
                        (snapf_bits[(u - 0x08000000u) >> 4] >> ((u >> 1) & 7) & 1) &&
                        snapf_count[(u - 0x08000000u) >> 1 & 0x3FFFFF]++ < snapf_max && npend < 256) {
                        int32_t lr = 0, sp = 0;
                        core->readRegister(core, "lr", &lr);
                        core->readRegister(core, "sp", &sp);
                        pend[npend].lr = (uint32_t)lr & ~1u;
                        pend[npend].sp = (uint32_t)sp;
                        pend[npend].step = steps_total;
                        pend[npend].func = u;
                        pend[npend].irq = 0;
                        snprintf(pend[npend].name, sizeof pend[npend].name, "%08x_%u", u,
                                 (unsigned)snapf_count[(u - 0x08000000u) >> 1 & 0x3FFFFF] - 1);
                        save_snapshot_named(core, snapf_dir, pend[npend].name, u, (uint32_t)cpsr);
                        npend++;
                    }
                }
                if (pending && ((uint32_t)pc & ~1u) == pending_lr) {
                    int32_t sp = 0;
                    core->readRegister(core, "sp", &sp);
                    if (getenv("SNAP_DEBUG")) fprintf(stderr, "at lr: sp=%08x\n", (uint32_t)sp);
                    if ((uint32_t)sp == pending_sp) {
                        char name[32];
                        snprintf(name, sizeof name, "%04lu.exit", pending_n);
                        save_snapshot_named(core, snap_dir, name, (uint32_t)pc, (uint32_t)cpsr);
                        pending = 0;
                    }
                }
                if (!pending && snap_pc && ((uint32_t)pc & ~1u) == (snap_pc & ~1u) && snap_saved < snap_max &&
                    snap_filter_ok(core, snapf_reg, snapf_off, snapf_mask, snapf_val) &&
                    snap_hits++ % snap_every == 0) {
                    if (snap_exit) {
                        int32_t lr = 0, sp = 0;
                        core->readRegister(core, "lr", &lr);
                        core->readRegister(core, "sp", &sp);
                        pending_lr = (uint32_t)lr & ~1u;
                        pending_sp = (uint32_t)sp;
                        pending_n = snap_saved;
                        pending = 1;
                        if (getenv("SNAP_DEBUG")) fprintf(stderr, "entry %lu lr=%08x sp=%08x\n", pending_n, pending_lr, pending_sp);
                    }
                    save_snapshot(core, snap_dir, snap_saved++, (uint32_t)pc, (uint32_t)cpsr);
                }
                if (count_pc && ((uint32_t)pc & ~1u) == (count_pc & ~1u)) {
                    int32_t r0 = 0, lr = 0;
                    core->readRegister(core, "r0", &r0);
                    core->readRegister(core, "lr", &lr);
                    count_hit((uint32_t)r0, (uint32_t)lr);
                }
                core->step(core);
                uint64_t t1 = mTimingGlobalTime(core->timing);
                uint32_t d = (uint32_t)(t1 - t0);
                t0 = t1;
                if (!pg_active)
                    continue;
                pg_total += d;
                uint32_t u = (uint32_t)pc;
                if (u >= 0x08000000u && u < 0x09000000u) pg_rom[(u - 0x08000000u) >> 1] += d;
                else if (u >= 0x03000000u && u < 0x03008000u) pg_ram[(u - 0x03000000u) >> 1] += d;
                else if (u >= 0x02000000u && u < 0x02040000u) pg_ewram[(u - 0x02000000u) >> 1] += d;
                else if (u < 0x4000) pg_bios[u >> 1] += d;
            }
            continue;
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
    if (save_state) {
        size_t n = core->stateSize(core);
        void *buf = calloc(1, n);
        FILE *f = fopen(save_state, "wb");
        if (!f || !core->saveState(core, buf) || fwrite(buf, 1, n, f) != n)
            fprintf(stderr, "cannot save state %s\n", save_state);
        if (f) fclose(f);
        free(buf);
    }
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
    if (final_digest) {
        static const struct { const char *n; unsigned base, size; } dr[] = {
            { "ewram", 0x02000000, 0x40000 }, { "pal", 0x05000000, 0x400 },
            { "vram", 0x06000000, 0x18000 }, { "oam", 0x07000000, 0x400 } };
        unsigned when = game_frame_addr ? core->busRead16(core, core->busRead32(core, game_frame_addr)) : 0;
        printf("final game=%u hw=%u", when, frames);
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
    if (dump_save) {
        /* The game's save memory as it sees it: BN6 has 32 KB of SRAM at
         * 0x0E000000. (libmgba 0.10's savedataClone returns garbage here.) */
        FILE *o = fopen(dump_save, "wb");
        for (unsigned a = 0; o && a < 0x8000; a++)
            fputc(core->busRead8(core, 0x0E000000 + a), o);
        if (o) fclose(o);
    }
    free(save_data);
    if (g_illegal)
        fprintf(stderr, "%lu illegal opcodes executed in all\n", g_illegal);
    printf("ran %u frames\n", frames);
    core->deinit(core);
    return 0;
}
