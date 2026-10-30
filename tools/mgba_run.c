/*
 * mgba_run — headless libmgba runner for comparing the original and lifted ROMs.
 *
 *   mgba_run ROM [--frames N] [--input TRACE] [--shots DIR --every N]
 *                [--bios FILE] [--marker ADDR] [--dump-ram PREFIX]
 *
 * TRACE uses gbarecomp's keyinput format: "frame,0xKEYINPUT" (active low).
 * Screenshots are binary PPMs (DIR/frame_NNNNNN.ppm).
 *
 * Build: make -C tools mgba_run (needs libmgba-dev).
 */
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/core/config.h>
#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/arm/arm.h>
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
static int g_quiet_calls;         /* --call-snaps: random inputs run into garbage; do not log every step */

/* --call-snaps: the loads of a call that a port is not held to, written to
 * DIR/snap_NAME.bus for the CPU oracle (tools/oracle.py): misaligned halfword
 * and word loads (the ARM7 rotates them; typed C reads what its type says)
 * and open-bus reads (mGBA's bad-load messages; the host maps those
 * addresses as plain memory). Only the call's own instructions are watched. */
static int g_watch_bus;
static unsigned g_nbus;
static char g_bus[4][96];
static void bus_note(const char *fmt, ...) {
    if (g_nbus < sizeof g_bus / sizeof g_bus[0]) {
        va_list a;
        va_start(a, fmt);
        vsnprintf(g_bus[g_nbus], sizeof g_bus[0], fmt, a);
        va_end(a);
    }
    g_nbus++;
}
/* mGBA's messages for a load that returns open bus. Twelve unused I/O
 * halfwords (upper halves of 32-bit registers) read as 0 instead, the same on
 * the host: not open bus (checked against libmgba 0.10.2 by reading every
 * unused register at several points of the instruction stream). */
static int open_bus_message(const char *msg) {
    static const char *const bad[] = { "Bad memory Load", "Out of bounds ROM Load", "Bad BIOS Load",
                                       "Read from write-only I/O register" };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++)
        if (!strncmp(msg, bad[i], strlen(bad[i]))) return 1;
    static const char unused[] = "Read from unused I/O register: ";
    if (strncmp(msg, unused, sizeof unused - 1)) return 0;
    static const unsigned zero[] = { 0x066, 0x06A, 0x06E, 0x076, 0x07A, 0x07E, 0x086, 0x08A, 0x136, 0x142, 0x15A, 0x206 };
    unsigned reg = (unsigned)strtoul(msg + sizeof unused - 1, 0, 16);
    for (unsigned i = 0; i < sizeof zero / sizeof zero[0]; i++)
        if (reg == zero[i]) return 0;
    return 1;
}
static uint32_t (*g_load32)(struct ARMCore *, uint32_t, int *);
static uint32_t (*g_load16)(struct ARMCore *, uint32_t, int *);
static uint32_t watched_load32(struct ARMCore *cpu, uint32_t a, int *cycles) {
    if (g_watch_bus && (a & 3)) bus_note("misaligned word load at %08x", a);
    return g_load32(cpu, a, cycles);
}
static uint32_t watched_load16(struct ARMCore *cpu, uint32_t a, int *cycles) {
    if (g_watch_bus && (a & 1)) bus_note("misaligned halfword load at %08x", a);
    return g_load16(cpu, a, cycles);
}
static void watch_loads(struct mCore *core) {
    struct ARMCore *cpu = core->cpu;
    if (cpu->memory.load32 != watched_load32) { g_load32 = cpu->memory.load32; cpu->memory.load32 = watched_load32; }
    if (cpu->memory.load16 != watched_load16) { g_load16 = cpu->memory.load16; cpu->memory.load16 = watched_load16; }
}

static void log_cb(struct mLogger *logger, int category, enum mLogLevel level,
                   const char *format, va_list args) {
    (void)logger;
    const char *cat = mLogCategoryName(category);
    if (g_watch_bus && (level & mLOG_GAME_ERROR)) {
        char msg[96];
        va_list a;
        va_copy(a, args);
        vsnprintf(msg, sizeof msg, format, a);
        va_end(a);
        if (open_bus_message(msg)) bus_note("open bus: %s", msg);
    }
    if (g_quiet_calls && !(level & mLOG_FATAL))
        return;
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
static uint16_t snapf_count[0x400000];   /* calls captured per halfword address */

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

/* --call-edges SITES OUT: the indirect branch sites to watch (one bit per
 * halfword of ROM and IWRAM) and the (site, target, lr) triples seen there,
 * with how often. */
static uint8_t ce_rom[0x02000000 / 16], ce_iwram[0x8000 / 16];
static int ce_watched(uint32_t a) {
    if (a >= 0x08000000u && a < 0x0A000000u) return ce_rom[(a - 0x08000000u) >> 4] >> ((a >> 1) & 7) & 1;
    if (a >= 0x03000000u && a < 0x03008000u) return ce_iwram[(a - 0x03000000u) >> 4] >> ((a >> 1) & 7) & 1;
    return 0;
}
static void ce_watch(uint32_t a) {
    a &= ~1u;
    if (a >= 0x08000000u && a < 0x0A000000u) ce_rom[(a - 0x08000000u) >> 4] |= 1u << ((a >> 1) & 7);
    else if (a >= 0x03000000u && a < 0x03008000u) ce_iwram[(a - 0x03000000u) >> 4] |= 1u << ((a >> 1) & 7);
}
#define NEDGE 65536
static struct { uint32_t site, target, lr; unsigned long n; } edges[NEDGE];
static unsigned long ce_lost;   /* edges dropped because the table is full */
static void ce_count(uint32_t site, uint32_t target, uint32_t lr) {
    uint32_t h = (site * 2654435761u ^ target * 40503u ^ lr * 69069u) & (NEDGE - 1);
    for (unsigned k = 0; k < NEDGE; k++, h = (h + 1) & (NEDGE - 1)) {
        if (edges[h].n && edges[h].site == site && edges[h].target == target && edges[h].lr == lr) { edges[h].n++; return; }
        if (!edges[h].n) { edges[h].site = site; edges[h].target = target; edges[h].lr = lr; edges[h].n = 1; return; }
    }
    ce_lost++;
}

/* --count-entries FILE OUT [G0 [G1]]: how many times each of the entry
 * addresses listed in FILE (hex, one per line) is EXECUTED while the game frame
 * counter is in [G0, G1). Unlike --profile-game, which attributes CYCLES to a
 * PC, this counts EXECUTIONS of the watched entries, so it answers "how many
 * times was this function called" for every listed function in one pass -- what
 * wf facts records as `calls`. The same stepping loop runs (forced on for the
 * window), so the counts are exact; OUT lines are "ADDR COUNT" for every
 * watched address that ran at least once. */
#define NENTRY 65536
static uint8_t ce_count_rom[0x02000000 / 16], ce_count_iwram[0x8000 / 16];
static uint32_t entry_addr[NENTRY];        /* the watched entries, in file order */
static unsigned long entry_n[NENTRY];
static unsigned n_entries;
static int entry_ready;
static inline void count_entry(uint32_t a) {
    if (!entry_ready) return;
    a &= ~1u;
    if (a >= 0x08000000u && a < 0x0A000000u) {
        if (!(ce_count_rom[(a - 0x08000000u) >> 4] >> ((a >> 1) & 7) & 1)) return;
    } else if (a >= 0x03000000u && a < 0x03008000u) {
        if (!(ce_count_iwram[(a - 0x03000000u) >> 4] >> ((a >> 1) & 7) & 1)) return;
    } else return;
    uint32_t h = (a * 2654435761u) & (NENTRY - 1);
    for (unsigned k = 0; k < NENTRY; k++, h = (h + 1) & (NENTRY - 1))
        if (entry_addr[h] == a) { entry_n[h]++; return; }
}
static void count_dump_ones(const char *out, unsigned long from, unsigned long to) {
    FILE *o = fopen(out, "w");
    if (!o) { perror(out); return; }
    unsigned seen = 0;
    for (unsigned k = 0; k < NENTRY; k++)
        if (entry_addr[k] && entry_n[k]) { fprintf(o, "%08x %lu\n", entry_addr[k], entry_n[k]); seen++; }
    fprintf(o, "# count-entries: %u watched, %u executed, game frames %lu..%lu\n",
            n_entries, seen, from, to);
    fclose(o);
    printf("count-entries: %u of %u entries executed over game frames %lu..%lu -> %s\n",
           seen, n_entries, from, to, out);
}

/* The game's frame counter (a u16 behind a pointer at ADDR), unwrapped: when it
 * reads lower than before (the wrap past 65535) counting continues upward
 * from the previous value, so a trace longer than 65536 game frames keeps
 * one increasing timeline. Traces that never wrap see the raw counter. */
static unsigned gc_base, gc_last;   /* file scope: --resume-game-frame sets them after --load-state */
static unsigned game_counter(struct mCore *core, unsigned long addr) {
    unsigned c = core->busRead16(core, core->busRead32(core, addr));
    if (c < gc_last)
        gc_base += gc_last + 1 - c;
    gc_last = c;
    return gc_base + c;
}

/* What game_counter would return now, without moving its state (for reads in mid-frame). */
static unsigned game_counter_peek(struct mCore *core, unsigned long addr) {
    unsigned c = core->busRead16(core, core->busRead32(core, addr));
    return c < gc_last ? gc_base + gc_last + 1 : gc_base + c;
}

/* --handoff DIR G: load a game's full state (the same format as
 * --dump-mem-at-when writes) into CORE at game frame G.  GAME memory
 * regions are overwritten from DIR; IWRAM skips the destination's own
 * runtime (0x03005700-0x03007FFE) and its BIOS IRQ vector (0x03007FFC),
 * so a port started cold can be resumed without the BIOS clear wiping
 * its variables.  The DIR is read in two layouts:
 *   1. DIR/<region>.bin (consolidated, useful for tests / scripts)
 *   2. DIR/<prefix>_<g>.<region>.bin (the form --dump-mem-at-when writes)
 * The first file that opens wins; missing files are skipped (a region
 * may have been left out of a partial handoff). See main_:
 * main_awaitFrame is the "end of frame G's work just happened" mark,
 * the counter increments to G+1 once frame G finishes.  When the
 * trace's --input resumes with the same key for G+1, both ROMs see
 * it at the same point and continue on the same timeline. */
static int handoff_open(const char *dir, const char *region, char *path, size_t pl) {
    snprintf(path, pl, "%s/%s.bin", dir, region);
    if (access(path, F_OK) == 0) return 0;
    /* try to find a <prefix>_<g>.<region>.bin in the same dir (the form
     * --dump-mem-at-when writes); name ends in "." + region + ".bin" */
    char tail[64];
    snprintf(tail, sizeof tail, ".%s.bin", region);
    size_t tl = strlen(tail);
    DIR *dh = opendir(dir);
    if (!dh) return -1;
    struct dirent *de;
    while ((de = readdir(dh))) {
        const char *nm = de->d_name;
        size_t l = strlen(nm);
        if (l < tl || strcmp(nm + l - tl, tail)) continue;
        snprintf(path, pl, "%s/%s", dir, nm);
        closedir(dh);
        return 0;
    }
    closedir(dh);
    return -1;
}
static void do_handoff(struct mCore *core, const char *dir) {
    static const struct { const char *suffix; unsigned base, size; } rg[] = {
        { "ewram", 0x02000000, 0x40000 }, { "iwram", 0x03000000, 0x8000 },
        { "vram", 0x06000000, 0x18000 }, { "pal", 0x05000000, 0x400 },
        { "oam", 0x07000000, 0x400 }, { "save", 0x0E000000, 0x8000 },
    };
    unsigned long n_skew = 0;
    for (unsigned r = 0; r < sizeof rg / sizeof rg[0]; r++) {
        char path[512];
        if (handoff_open(dir, rg[r].suffix, path, sizeof path) != 0) {
            fprintf(stderr, "handoff: %s not found in %s\n", rg[r].suffix, dir);
            continue;
        }
        FILE *f = fopen(path, "rb");
        if (!f) { fprintf(stderr, "handoff: cannot read %s\n", path); continue; }
        unsigned char *buf = malloc(rg[r].size);
        if (fread(buf, 1, rg[r].size, f) != rg[r].size) {
            fprintf(stderr, "handoff: short read %s\n", path);
            free(buf); fclose(f); continue;
        }
        fclose(f);
        /* For SAVE (0x0E000000..) the cart's storage is FLASH (BN6 uses
         * FLASH1M, 128 KB).  libmgba routes every byte written through
         * GBASavedataWriteFlash's command state machine: a 32-KB byte-by-byte
         * busWrite8 stream from this dump will see any 0xAA / 0x55 / 0xA0 /
         * 0x10 / 0x30 / 0x90 / 0xB0 / 0xF0 byte trigger unlock / erase /
         * program commands, corrupting the underlying buffer before all of
         * the 32 KB is written.  Use core->savedataRestore, which copies the
         * raw buffer in directly and leaves the FSM idle, so the game's
         * normal 0xAA/0x55/byte write sequence works correctly from this
         * point on. (libmgba 0.10 returns false on cold boot because the
         * cart's savedata struct isn't allocated yet; we then fall back to
         * byte-by-byte busWrite8 which works for sections whose game state
         * doesn't diverge strongly enough to drive the FSM differently.) */
        if (rg[r].base == 0x0E000000u && core->savedataRestore) {
            if (core->savedataRestore(core, buf, rg[r].size, 0)) {
                fprintf(stderr, "handoff: %s restored %lu bytes via savedataRestore\n",
                        rg[r].suffix, (unsigned long)rg[r].size);
            } else {
                fprintf(stderr, "handoff: %s savedataRestore(%lu) failed, falling back\n",
                        rg[r].suffix, (unsigned long)rg[r].size);
                for (unsigned a = 0; a < rg[r].size; a++)
                    core->busWrite8(core, rg[r].base + a, buf[a]);
            }
            free(buf);
            continue;
        }
        /* IWRAM is shared with the destination's IWRAM code/state: three
         * portions must NOT be overwritten:
         *   0x03005700-0x030079CF: the port's runtime code & globals
         *                           (cport/gba.ld: iwram 0x03005700, size 0x22D0)
         *   0x030079D0-0x03007DFF: the port's IRQ and SYS stacks (kept by the
         *                           IRQ entry's mode switch)
         *   0x03007E00-0x03007FFB: the game's user-mode stack. The port's CPU
         *                           registers (SP, LR, pc) are NOT overwritten,
         *                           so its current call chain points at the
         *                           port's own frames. Copying the original's
         *                           bytes here would inject ROM-relative return
         *                           addresses from the original bn6f into the
         *                           lifted port's pop {pc} targets, plus destroy
         *                           the port's saved regs from main_ and its
         *                           callees: it must run to the next VBlank on
         *                           its own stack pattern, and continue with
         *                           EWRAM/IWRAM-game/VRAM/PAL/OAM/SAVE writes
         *                           that match the original from now on.
         * 0x03007FFC-0x03007FFF (the BIOS IRQ vector) is part of the stack
         * range above and is skipped with it. */
        static const struct { unsigned lo, hi; } iwram_skip[] = {
            { 0x03005700u, 0x03007E00u },
            { 0x03007E00u, 0x03008000u },
        };
        for (unsigned a = 0; a < rg[r].size; a++) {
            unsigned addr = rg[r].base + a;
            int skip = 0;
            if (rg[r].base == 0x03000000) {
                for (unsigned s = 0; s < sizeof iwram_skip / sizeof iwram_skip[0]; s++)
                    if (iwram_skip[s].lo <= addr && addr < iwram_skip[s].hi) { skip = 1; break; }
            }
            if (skip) continue;
            unsigned char prev = core->busRead8(core, addr);
            int changed = (prev != buf[a]);
            if (changed) {
                if (n_skew < 10) {
                    fprintf(stderr, "handoff: diff %s[0x%08x] prev=0x%02x buf=0x%02x\n", rg[r].suffix, addr, prev, buf[a]);
                }
                n_skew++;
            }
            if (!changed) continue;
            /* GBA VRAM/PAL/OAM are 16-bit wide: byte stores BROADCAST the
             * value to both bytes of the halfword.  For example,
             * busWrite8(VRAM+1, 0x24) sets the 16-bit slot [VRAM+0..VRAM+1]
             * to 0x2424 (00 24 00 24 in the halfword pair), not 0x00XX.
             * Use 16-bit stores aligned to halfwords.  EWRAM, IWRAM and
             * SRAM are 8-bit wide and busWrite8 works there, but most of
             * the destination's bytes equal the buffer anyway so we
             * wasted no uop on those.  Use the wider write so we don't
             * corrupt neighbouring bytes. */
            if (rg[r].base == 0x06000000u || rg[r].base == 0x05000000u || rg[r].base == 0x07000000u) {
                /* VRAM/PAL/OAM: 16-bit write.  The 16-bit slot at addr & ~1
                 * holds buf[addr & ~1] (low) and buf[(addr & ~1) + 1] (high).
                 * If only one of them differs from prev, that's still fine:
                 * we read both, mask in the new value, write back the slot,
                 * and leave the other byte along. */
                unsigned hi = (addr & 0x1FFFEu);  /* halfword-aligned byte offset 0..0x1FFFC */
                uint16_t slot;
                if (hi < rg[r].size - 1) {
                    slot = (uint16_t)buf[hi] | ((uint16_t)buf[hi + 1] << 8);
                    core->busWrite16(core, rg[r].base + hi, slot);
                } else {
                    /* Tail byte: busWrite8 broadcasts (same as the upper
                     * case); write the matching low byte from the buffer
                     * and trust broadcast symmetry. */
                    core->busWrite8(core, addr, buf[a]);
                }
            } else {
                core->busWrite8(core, addr, buf[a]);
            }
        }
        free(buf);
        fprintf(stderr, "handoff: %s wrote %lu bytes from %s\n", rg[r].suffix,
                (unsigned long)rg[r].size, path);
    }
    /* Last: the destination's I/O registers. Only the 5 critical to
     * staying in sync with the original need to be restored (VBlank flag
     * in DISPSTAT, KEYINPUT, plus the interrupt dispatch state in
     * IF/IE/IME). Writing the full 0x04000000..0x04000400 block —
     * including the DMA, sound FIFO, BG scroll and timer registers —
     * left the destination with subtle sub-instruction timing errors
     * that made the port's VRAM start diverging later in the section
     * (e.g. game=6200 in the story1 trace). The rest of the I/O space
     * is reset on the emulator to the same value both sides already,
     * so we let the destination keep its pre-handoff values there. */
    char path[512];
    if (handoff_open(dir, "ior", path, sizeof path) == 0) {
        FILE *f = fopen(path, "rb");
        if (f) {
            /* File is the "full I/O block" format (paths//ior.bin) where
             * the byte at offset o holds the value at I/O address
             * 0x04000000 + o. */
            const struct { unsigned gba; unsigned off; } regs[] = {
                { 0x04000004u, 0x0004u },   /* DISPSTAT */
                { 0x04000130u, 0x0130u },   /* KEYINPUT */
                { 0x04000202u, 0x0202u },   /* IF */
                { 0x04000200u, 0x0200u },   /* IE */
                { 0x04000208u, 0x0208u },   /* IME */
            };
            /* Need at least offset 0x020B+4 = 0x020C bytes. */
            unsigned char buf[0x020C];
            if (fread(buf, 1, sizeof buf, f) == sizeof buf)
                for (unsigned r = 0; r < sizeof regs / sizeof regs[0]; r++)
                    for (unsigned b = 0; b < 4; b++)
                        core->busWrite8(core, regs[r].gba + b, buf[regs[r].off + b]);
            else
                fprintf(stderr, "handoff: ior file too short, skipping IOR restores\n");
            fclose(f);
        } else {
            fprintf(stderr, "handoff: cannot read %s\n", path);
        }
    }
    fprintf(stderr, "handoff: %lu bytes changed across all regions\n", n_skew);
}

int main(int argc, char **argv) {
    const char *rom = NULL, *input = NULL, *shots = NULL, *bios = NULL, *dump = NULL;
    unsigned frames = 600, every = 0, bios_intro = 0;
    unsigned long marker = 0, guest = 0, game_frame_addr = 0, until_game_frame = 0, digest_every = 0;
    unsigned long stall_frames = 3600;   /* with --until-game-frame: stop after a minute without a game frame */
    unsigned long dump_at[64]; unsigned ndump_at = 0; unsigned long long dumped_at = 0; const char *dump_at_prefix = NULL;
    /* --dump-mem-at-when G1,G2,... PREFIX: at those game frames (start of HW
     * frame where when == G), write the game's full state (EWRAM, IWRAM,
     * VRAM, palette, OAM, SRAM, and a small block of key I/O registers)
     * as PREFIX_<g>.{ewram,iwram,vram,pal,oam,save,ior}.bin, so a
     * pair-replay diff (tools/handoff_diff.py) can list differing addresses
     * with bn6f symbols and classify them.  Same file naming the --handoff
     * source reads, so one run on the original produces the dir the port
     * reads. */
    unsigned long dump_mem_at[64]; unsigned ndump_mem_at = 0; unsigned long long dumped_mem_at = 0; const char *dump_mem_at_prefix = NULL;
    /* --dump-pre-handoff PREFIX: at the START of frame G (when == handoff_g),
     * BEFORE do_handoff writes anything, dump the destination's bus state to
     * PREFIX_<G>.{ewram,iwram,vram,pal,oam,save,ior}.bin.  Combine with
     * --handoff DIR G to compare the destination's own cold-accumulated
     * state against the original's dump at the same moment, region by
     * region. */
    const char *dump_pre_handoff_prefix = NULL;
    /* --handoff DIR G: pre-load the original's full state from the same file
     * format --dump-mem-at-when writes (DIR/<...>.bin), at game frame G
     * (start of HW frame where when == G): GAME memory regions only
     * (EWRAM, game's IWRAM, game stack, VRAM, palette, OAM, SRAM) are
     * overwritten.  The port's runtime IWRAM (0x03005700-0x03007EFE) and
     * its BIOS IRQ vector (0x03007FFC) are left alone.  Any --poke-at
     * writes whose game frame is < G are skipped (the original's poke_gs
     * are "the trace replay's frame counter when pc == poke_pc", and the
     * counter at --handoff G is exactly G).  Used to start a long route
     * mid-way from the original. */
    const char *handoff_dir = NULL;
    unsigned long handoff_g = 0;
    int handoff_done = 0;
    /* --profile-game G0 G1: cycle profile from game frame G0 to G1 (needs
     * --game-frame-addr); input keeps following the trace while profiling. */
    unsigned long pg_from = 0, pg_to = 0;
    /* --count-entry ADDR: while profiling, histogram (r0, lr) at each
     * execution of ADDR (e.g. a runtime function's entry). */
    unsigned long count_pc = 0;
    /* --count-entries FILE OUT [G0 [G1]]: exact execution counts of the entry
     * addresses in FILE over the game-frame window [G0, G1) (see count_entry). */
    const char *cnt_file = NULL;
    unsigned long cnt_from = 0, cnt_to = 0;
    /* --load-state FILE / --save-state FILE: start from / end with an mGBA
     * save state (for building input traces step by step). */
    const char *load_state = NULL, *save_state = NULL;
    /* --resume-game-frame G (with --load-state of a state saved at game frame G, e.g. by
     * --save-state-every): the unwrapped game frame counter goes on from G, and --poke-at writes
     * before G (already in the state) are skipped. --save-state-every N PREFIX: save a state at the
     * start of the first hardware frame of every game frame that is a multiple of N, as
     * PREFIX_GGGGGGG.state (checkpoints to resume a long trace from). */
    unsigned long resume_g = 0, save_every = 0, next_save = 0; const char *save_prefix = NULL;
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
    /* repeatable: several writes, each at its own game frame (in order).
     * G is the game frame counter at the start of the hardware frame, so where
     * a write lands depends on the build's speed: a build still busy with game
     * frame G's work when that hardware frame starts gets it at the end of G,
     * one that had finished G at the end of G+1. --poke-live: G is the
     * counter when PC is reached instead (with
     * main_awaitFrame's entry, the game frame whose work just ended), the same
     * point in every build; "at=" in the "poke" lines is that counter, so a
     * replay of the original says where its writes landed. */
    int poke_live = 0;
    enum { MAX_POKES = 65536 };
    static unsigned long poke_gs[MAX_POKES];
    static const char *poke_specs[MAX_POKES];
    unsigned long poke_pc = 0;
    int npokes = 0, next_poke = 0;
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
    int snapf_keep_interrupted = 0;   /* --snap-keep-interrupted: keep the entry of a call an IRQ interrupted */
    /* --call-edges SITES OUT: step the whole run and record, at every listed
     * indirect branch site (hex addresses, e.g. from lift.py --export-abi via
     * tools/abi/call_edges.py), where it went and the LR it went with (the
     * return point of the call, also for jumps through veneers). OUT lines:
     * "SITE TARGET LR COUNT" (hex; TARGET has the Thumb bit). */
    const char *ce_out = NULL;
    int ce_pending = 0;
    uint32_t ce_site = 0, ce_lr = 0, ce_mode = 0, ce_next = 0;
    struct { uint32_t lr, sp, func; unsigned long step; int irq; char name[32]; } pend[256];
    unsigned long steps_total = 0;
    int npend = 0;
    uint32_t pending_lr = 0, pending_sp = 0;
    unsigned long pending_n = 0;
    int pending = 0;
    unsigned long snapf_off = 0, snapf_mask = 0, snapf_val = 0;
    int pg_active = 0, pg_continue = 0, pg_done = 0;
    /* --watch-writes ADDR LEN FROM TO: single-step the game frames FROM..TO and
     * print every change of the LEN bytes at ADDR with the pc that did it */
    uint32_t ww_addr = 0, ww_len = 0;
    unsigned long ww_from = 1, ww_to = 0;
    uint8_t ww_prev[64];
    const char *ce_out_cnt = NULL;
    int cnt_active = 0, cnt_dumped = 0;
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
        else if (!strcmp(argv[i], "--dump-mem-at-when")) {   /* G1,G2,... PREFIX: full game state at those game frames */
            for (char *t = strtok(argv[++i], ","); t && ndump_mem_at < 64; t = strtok(NULL, ","))
                dump_mem_at[ndump_mem_at++] = strtoul(t, 0, 0);
            dump_mem_at_prefix = argv[++i];
        }
        else if (!strcmp(argv[i], "--watch-writes")) {      /* ADDR LEN FROM_GAME TO_GAME: who wrote these bytes (LEN <= 64) */
            ww_addr = strtoul(argv[++i], 0, 0);
            ww_len = strtoul(argv[++i], 0, 0);
            if (ww_len > 64) ww_len = 64;
            ww_from = strtoul(argv[++i], 0, 0);
            ww_to = strtoul(argv[++i], 0, 0);
        }
        else if (!strcmp(argv[i], "--handoff")) {            /* DIR G: write into the game state at game frame G */
            handoff_dir = argv[++i];
            handoff_g = strtoul(argv[++i], 0, 0);
        }
        else if (!strcmp(argv[i], "--dump-pre-handoff")) {  /* PREFIX: dump the destination's bus state at the moment do_handoff fires, BEFORE the overwrite */
            dump_pre_handoff_prefix = argv[++i];
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
        else if (!strcmp(argv[i], "--count-entries")) {
            cnt_file = argv[++i]; ce_out_cnt = argv[++i];
            cnt_from = strtoul(argv[++i], 0, 0);
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') cnt_to = strtoul(argv[++i], 0, 0);
            else { cnt_to = ~0ul; if (i + 1 < argc && argv[i + 1][0] == '-') i++; }
            FILE *f = fopen(cnt_file, "r");
            if (!f) { fprintf(stderr, "cannot read %s\n", cnt_file); return 2; }
            char line[64];
            while (fgets(line, sizeof line, f)) {
                uint32_t a = strtoul(line, 0, 16) & ~1u;
                if (!a) continue;
                if (n_entries >= NENTRY) { fprintf(stderr, "too many entries in %s\n", cnt_file); return 2; }
                if (a >= 0x08000000u && a < 0x0A000000u) ce_count_rom[(a - 0x08000000u) >> 4] |= 1u << ((a >> 1) & 7);
                else if (a >= 0x03000000u && a < 0x03008000u) ce_count_iwram[(a - 0x03000000u) >> 4] |= 1u << ((a >> 1) & 7);
                else continue;   /* not in ROM or IWRAM: nothing to step there */
                uint32_t h = (a * 2654435761u) & (NENTRY - 1);
                while (entry_addr[h]) h = (h + 1) & (NENTRY - 1);
                entry_addr[h] = a; n_entries++;
            }
            fclose(f);
            entry_ready = 1;
        }
        else if (!strcmp(argv[i], "--load-state")) load_state = argv[++i];
        else if (!strcmp(argv[i], "--save")) save_file = argv[++i];
        else if (!strcmp(argv[i], "--call-snaps")) { call_dir = argv[++i]; call_steps = strtoul(argv[++i], 0, 0); }
        else if (!strcmp(argv[i], "--dump-save")) dump_save = argv[++i];
        else if (!strcmp(argv[i], "--final-digest")) final_digest = 1;
        else if (!strcmp(argv[i], "--poke-live")) poke_live = 1;
        else if (!strcmp(argv[i], "--poke-at")) {
            poke_pc = strtoul(argv[++i], 0, 0);
            if (npokes == MAX_POKES) { fprintf(stderr, "too many --poke-at\n"); return 1; }
            poke_gs[npokes] = strtoul(argv[++i], 0, 0); poke_specs[npokes++] = argv[++i];
        }
        else if (!strcmp(argv[i], "--trace-branches")) branch_trace = fopen(argv[++i], "w");
        else if (!strcmp(argv[i], "--watch")) { watch_addr = strtoul(argv[++i], 0, 0); watch_len = strtoul(argv[++i], 0, 0); }
        else if (!strcmp(argv[i], "--save-state")) save_state = argv[++i];
        else if (!strcmp(argv[i], "--resume-game-frame")) resume_g = strtoul(argv[++i], 0, 0);
        else if (!strcmp(argv[i], "--save-state-every")) { save_every = strtoul(argv[++i], 0, 0); save_prefix = argv[++i]; }
        else if (!strcmp(argv[i], "--snap-exit")) snap_exit = 1;
        else if (!strcmp(argv[i], "--snap-keep-interrupted")) snapf_keep_interrupted = 1;
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
        else if (!strcmp(argv[i], "--call-edges")) {
            FILE *f = fopen(argv[++i], "r");
            ce_out = argv[++i];
            char line[64];
            if (!f) { fprintf(stderr, "cannot read %s\n", argv[i - 1]); return 2; }
            while (fgets(line, sizeof line, f))
                if (line[0] != '#') ce_watch(strtoul(line, 0, 16));
            fclose(f);
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
    if (resume_g && game_frame_addr) {   /* the counter reads resume_g now; earlier writes are in the state */
        gc_last = core->busRead16(core, core->busRead32(core, game_frame_addr));
        gc_base = (unsigned)resume_g - gc_last;
        while (next_poke < npokes && poke_gs[next_poke] < resume_g) next_poke++;
    }
    if (save_every) next_save = (resume_g / save_every + 1) * save_every;

    if (call_dir) {
        DIR *d = opendir(call_dir);
        struct dirent *de;
        unsigned done = 0, timeouts = 0;
        g_quiet_calls = 1;
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
            /* A previous call may have run into garbage and left the CPU
             * halted or stopped, which memory and registers do not undo. */
            core->reset(core);
            watch_loads(core);
            g_nbus = 0;
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
            uint8_t swis[64]; unsigned nswis = 0;   /* BIOS calls the function made, for --call-snaps triage */
            unsigned long n;
            int32_t pc = 0, sp = 0;
            for (n = 0; n < call_steps; n++) {
                int32_t c1 = 0;
                core->readRegister(core, "pc", &pc);
                core->readRegister(core, "cpsr", &c1);
                core->readRegister(core, "sp", &sp);
                pc -= (c1 & 0x20) ? 2 : 4;      /* mGBA pipeline PC -> next instruction to execute */
                if (((uint32_t)pc & ~1u) == lr && (uint32_t)sp == sp0) break;
                /* BIOS Halt, Stop, IntrWait, VBlankIntrWait never wake with
                 * interrupts off (and mGBA's step would not return): the
                 * call does not return here. */
                unsigned swi = ~0u;
                if (c1 & 0x20) {
                    uint16_t op = core->busRead16(core, (uint32_t)pc);
                    if ((op & 0xFF00) == 0xDF00) swi = op & 0xFF;
                } else {
                    uint32_t op = core->busRead32(core, (uint32_t)pc);
                    if ((op & 0x0F000000) == 0x0F000000) swi = (op >> 16) & 0xFF;
                }
                if (swi >= 2 && swi <= 5) { n = call_steps; break; }
                if (swi != ~0u && nswis < sizeof swis)
                    swis[nswis++] = (uint8_t)swi;
                /* executing outside BIOS, EWRAM, IWRAM or ROM: the call ran
                 * into a bad pointer and does not return */
                uint32_t region = (uint32_t)pc >> 24;
                if (!(((uint32_t)pc < 0x4000) || region == 2 || region == 3 || (region >= 8 && region <= 0xD))) {
                    n = call_steps;
                    break;
                }
                g_watch_bus = 1;
                core->step(core);
                g_watch_bus = 0;
            }
            if (n < call_steps) {
                int32_t c2 = 0;
                core->readRegister(core, "cpsr", &c2);
                save_snapshot_named(core, call_dir, name, (uint32_t)pc, (uint32_t)c2);
                done++;
                if (nswis) {   /* DIR/snap_NAME.swi: the SWI numbers executed, in order */
                    char sp[700];
                    snprintf(sp, sizeof sp, "%s/snap_%.*s.swi", call_dir, (int)(ln - 9), de->d_name + 5);
                    FILE *o = fopen(sp, "w");
                    for (unsigned k = 0; o && k < nswis; k++) fprintf(o, "%02x ", swis[k]);
                    if (o) { fputc('\n', o); fclose(o); }
                }
                if (g_nbus) {   /* DIR/snap_NAME.bus: loads the port is not held to (see g_watch_bus) */
                    char bp[700];
                    snprintf(bp, sizeof bp, "%s/snap_%.*s.bus", call_dir, (int)(ln - 9), de->d_name + 5);
                    FILE *o = fopen(bp, "w");
                    for (unsigned k = 0; o && k < g_nbus && k < sizeof g_bus / sizeof g_bus[0]; k++)
                        fprintf(o, "%s\n", g_bus[k]);
                    if (o) { fprintf(o, "%u in all\n", g_nbus); fclose(o); }
                }
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
    /* Mid-frame trigger for --dump-mem-at-when / --handoff: the frame in
     * which the game counter goes G-1 -> G is stepped one instruction at a
     * time and the loop is left right after the instruction that made the
     * counter read G; the next iteration then runs the dump/handoff blocks
     * at that program point (trig_resume), and the rest of the frame is run
     * on, still with the keys and poke timeline of frame G-1. */
    int trig_resume = 0;
    for (unsigned fr = 0; fr < frames; fr++) {
        /* Input timeline: hardware frames, or (with --game-frame-addr) the
         * game's own frame counter, so builds of different speed see the
         * same input at the same game state. */
        unsigned when = fr;
        if (game_frame_addr) {
            when = game_counter(core, game_frame_addr);
            if (save_every && when >= next_save) {
                char path[600];
                snprintf(path, sizeof path, "%s_%07u.state", save_prefix, when);
                size_t n = core->stateSize(core);
                void *buf = malloc(n);
                FILE *f = fopen(path, "wb");
                if (!f || !core->saveState(core, buf) || fwrite(buf, 1, n, f) != n) fprintf(stderr, "cannot save state %s\n", path);
                else printf("state game=%u hw=%u %s\n", when, fr, path);
                if (f) fclose(f);
                free(buf);
                next_save = (when / save_every + 1) * save_every;
            }
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
        int resumed = trig_resume;
        trig_resume = 0;
        while (!resumed && evi < nev && evs[evi].frame <= when)
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
        /* --dump-mem-at-when: taken in the main loop right after the game
         * counter was incremented to G (see the trig_t stepping below: the
         * strh at 0x08000308 in main_, before CapIncrementGameTimeFrames), the
         * same program point --handoff uses; the full game state, keyed by game frame for
         * tools/handoff_diff.py to name differing addresses with bn6f
         * symbols and --handoff to load into a port. */
        for (unsigned k = 0; k < ndump_mem_at && game_frame_addr; k++) {
            if ((dumped_mem_at >> k) & 1 || when < dump_mem_at[k])
                continue;
            dumped_mem_at |= 1ull << k;
            unsigned long g = dump_mem_at[k];
            static const struct { const char *suffix; unsigned base, size; } rg[] = {
                { "ewram", 0x02000000, 0x40000 }, { "iwram", 0x03000000, 0x8000 },
                { "vram", 0x06000000, 0x18000 }, { "pal", 0x05000000, 0x400 },
                { "oam", 0x07000000, 0x400 }, { "save", 0x0E000000, 0x8000 },
            };
            for (unsigned r = 0; r < sizeof rg / sizeof rg[0]; r++) {
                char path[512];
                snprintf(path, sizeof path, "%s_%lu.%s.bin", dump_mem_at_prefix, g, rg[r].suffix);
                FILE *o = fopen(path, "wb");
                if (!o) { perror(path); break; }
                for (unsigned a = 0; a < rg[r].size; a += 4) {
                    uint32_t v = core->busRead32(core, rg[r].base + a);
                    fwrite(&v, 4, 1, o);
                }
                fclose(o);
            }
            /* Full I/O register block (0x04000000-0x04000400) so a port can be
             * restored to the original's machine state at this game
             * frame. The next block (the IOR restore in --handoff) writes
             * back, with care around DMA enable bits and FIFO sound
             * registers. */
            {
                char path[512];
                snprintf(path, sizeof path, "%s_%lu.ior.bin", dump_mem_at_prefix, g);
                FILE *o = fopen(path, "wb");
                if (o) {
                    for (unsigned a = 0; a < 0x400; a++) {
                        uint8_t v = core->busRead8(core, 0x04000000 + a);
                        fputc(v, o);
                        /* IF (0x202), reading IF on real hardware is the
                         * same as before; on libmgba it does NOT clear
                         * pending flags. The handoff restores IF by
                         * writing it back byte-by-byte anyway, but a
                         * defensive reset is fine if it disagrees. */
                    }
                    fclose(o);
                }
            }
            printf("dump-mem-at-when game=%lu hw=%u -> %s_%lu.*.bin\n", g, fr, dump_mem_at_prefix, g);
            fflush(stdout);
        }
        /* --handoff G: at the start of the first HW frame whose game counter
         * reads G, overwrite the game's regions from handoff_dir.  Pokes
         * before G are skipped from here on (next_poke is advanced). */
        if (handoff_dir && !handoff_done && when == handoff_g && game_frame_addr) {
            /* --dump-pre-handoff PREFIX: write the destination's bus state to
             * PREFIX_<G>.region.bin BEFORE do_handoff modifies anything, so we
             * can diff the destination's own state against what handoff is
             * about to write into it.  Same program point as the
             * --dump-mem-at-when path above (counter just became G). */
            if (dump_pre_handoff_prefix) {
                static const struct { const char *suffix; unsigned base, size; } rg[] = {
                    { "ewram", 0x02000000, 0x40000 }, { "iwram", 0x03000000, 0x8000 },
                    { "vram", 0x06000000, 0x18000 }, { "pal", 0x05000000, 0x400 },
                    { "oam", 0x07000000, 0x400 }, { "save", 0x0E000000, 0x8000 },
                };
                for (unsigned r = 0; r < sizeof rg / sizeof rg[0]; r++) {
                    char path[512];
                    snprintf(path, sizeof path, "%s_%lu.%s.bin", dump_pre_handoff_prefix, (unsigned long)handoff_g, rg[r].suffix);
                    FILE *o = fopen(path, "wb");
                    if (!o) { perror(path); break; }
                    for (unsigned a = 0; a < rg[r].size; a += 4) {
                        uint32_t v = core->busRead32(core, rg[r].base + a);
                        fwrite(&v, 4, 1, o);
                    }
                    fclose(o);
                }
                {
                    char path[512];
                    snprintf(path, sizeof path, "%s_%lu.ior.bin", dump_pre_handoff_prefix, (unsigned long)handoff_g);
                    FILE *o = fopen(path, "wb");
                    if (o) {
                        for (unsigned a = 0; a < 0x400; a++)
                            fputc(core->busRead8(core, 0x04000000 + a), o);
                        fclose(o);
                    }
                }
                fprintf(stderr, "dump-pre-handoff game=%u hw=%u -> %s_%lu.*.bin\n",
                        when, fr, dump_pre_handoff_prefix, (unsigned long)handoff_g);
                fflush(stderr);
            }
            fprintf(stderr, "handoff: applying at game=%u hw=%u from %s\n", when, fr, handoff_dir);
            do_handoff(core, handoff_dir);
            handoff_done = 1;
            while (next_poke < npokes && poke_gs[next_poke] < handoff_g) {
                fprintf(stderr, "handoff: skip poke game=%lu spec=%s\n", poke_gs[next_poke], poke_specs[next_poke]);
                next_poke++;
            }
        }
        if (resumed) when--;    /* the rest of frame G-1: keys and pokes as in the cold run */
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
        if (cnt_file && game_frame_addr && !cnt_active && when >= cnt_from && when < cnt_to)
            cnt_active = 1;
        if (cnt_active && when >= cnt_to) {
            count_dump_ones(ce_out_cnt, cnt_from, cnt_to);
            cnt_dumped = 1;
            break;
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
        int poke_step = next_poke < npokes && game_frame_addr && when + 1 + 2 * poke_live >= poke_gs[next_poke];
        unsigned trig_t = 0;
        if (game_frame_addr) {
            for (unsigned k = 0; k < ndump_mem_at; k++)
                if (!((dumped_mem_at >> k) & 1) && dump_mem_at[k] == when + 1) trig_t = when + 1;
            if (handoff_dir && !handoff_done && handoff_g == when + 1) trig_t = when + 1;
        }
        int ww_step = ww_len && game_frame_addr && when >= ww_from && when <= ww_to;
        if (pg_active || poke_step || ce_out || cnt_active || trig_t || ww_step) {
            /* step one hardware frame, attributing cycles to the PC */
            uint32_t f0 = core->frameCounter(core);
            uint64_t t0 = mTimingGlobalTime(core->timing);
            while (core->frameCounter(core) == f0) {
                int32_t pc = 0, cpsr = 0;
                core->readRegister(core, "pc", &pc);
                core->readRegister(core, "cpsr", &cpsr);
                pc -= (cpsr & 0x20) ? 2 : 4;      /* mGBA pipeline PC -> next instruction to execute */
                if (trig_t && game_counter_peek(core, game_frame_addr) >= trig_t) {
                    int32_t sp = 0, lr = 0;
                    core->readRegister(core, "sp", &sp);
                    core->readRegister(core, "lr", &lr);
                    fprintf(stderr, "trigger: game counter reads %u before pc=%08x cpsr=%08x sp=%08x lr=%08x\n",
                            trig_t, (uint32_t)pc, (uint32_t)cpsr, (uint32_t)sp, (uint32_t)lr);
                    trig_resume = 1;
                    break;
                }
                if (ce_out) {
                    uint32_t u = (uint32_t)pc;
                    if (ce_pending) {
                        ce_pending = 0;
                        /* a conditional branch not taken, or an interrupt came first: nothing seen */
                        if (u != ce_next && (uint32_t)(cpsr & 0x1F) == ce_mode)
                            ce_count(ce_site, u | ((cpsr & 0x20) ? 1u : 0u), ce_lr);
                    }
                    if (ce_watched(u)) {
                        int32_t lr = 0;
                        core->readRegister(core, "lr", &lr);
                        ce_pending = 1;
                        ce_site = u;
                        ce_lr = (uint32_t)lr;
                        ce_mode = (uint32_t)cpsr & 0x1F;
                        ce_next = u + ((cpsr & 0x20) ? 2 : 4);
                    }
                }
                unsigned at = next_poke < npokes && ((uint32_t)pc & ~1u) == (poke_pc & ~1u) ? game_counter_peek(core, game_frame_addr) : 0;
                while (next_poke < npokes && (poke_live ? at : when) >= poke_gs[next_poke] && ((uint32_t)pc & ~1u) == (poke_pc & ~1u)) {
                    static char spec[16384];
                    snprintf(spec, sizeof spec, "%s", poke_specs[next_poke]);
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
                        printf("poke game=%u %08x=%x/%u at=%u\n", when, a, v, sz, at);
                    }
                    next_poke++;
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
                                } else if (!snapf_keep_interrupted) {
                                    char path[600];
                                    snprintf(path, sizeof path, "%s/snap_%s.bin", snapf_dir, pend[k].name);
                                    remove(path);
                                    snapf_count[(pend[k].func - 0x08000000u) >> 1 & 0x3FFFFF]--;
                                }   /* else: the entry stays, without an exit (difftest -S and the
                                     * oracle need only the entry; -G skips calls without an exit) */
                                npend = k;
                                break;
                            }
                        }
                    /* the count stops at the limit: a byte counter that kept
                     * counting would wrap after 256 calls and capture again,
                     * overwriting earlier entries (and pairing them with
                     * older exits) */
                    if (u >= 0x08000000u && u < 0x0A000000u &&
                        (snapf_bits[(u - 0x08000000u) >> 4] >> ((u >> 1) & 7) & 1) &&
                        snapf_count[(u - 0x08000000u) >> 1 & 0x3FFFFF] < snapf_max && npend < 256) {
                        snapf_count[(u - 0x08000000u) >> 1 & 0x3FFFFF]++;
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
                if (cnt_active) count_entry((uint32_t)pc);
                if (count_pc && ((uint32_t)pc & ~1u) == (count_pc & ~1u)) {
                    int32_t r0 = 0, lr = 0;
                    core->readRegister(core, "r0", &r0);
                    core->readRegister(core, "lr", &lr);
                    count_hit((uint32_t)r0, (uint32_t)lr);
                }
                if (ww_step) {
                    /* the watched bytes before the step, then who changed them */
                    for (uint32_t k = 0; k < ww_len; k++) ww_prev[k] = core->busRead8(core, ww_addr + k);
                    int32_t lr = 0;
                    core->readRegister(core, "lr", &lr);
                    int dma = 0;
                    for (uint32_t a = 0x040000BA; a <= 0x040000DE; a += 12)
                        if (core->busRead16(core, a) & 0x8000) dma = 1;
                    core->step(core);
                    for (uint32_t k = 0; k < ww_len; k++) {
                        uint8_t nv = core->busRead8(core, ww_addr + k);
                        if (nv != ww_prev[k])
                            printf("watch game=%u hw=%u pc=%08x lr=%08x cpsr=%08x addr=%08x %02x->%02x%s\n",
                                   when, fr, (uint32_t)pc, (uint32_t)lr, (uint32_t)cpsr,
                                   ww_addr + k, ww_prev[k], nv, dma ? " dma?" : "");
                    }
                } else
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
    /* a window that ended early (--until-game-frame, the frame budget, end of trace)
     * still counted what it saw: report it rather than lose it */
    if (cnt_file && !cnt_dumped) count_dump_ones(ce_out_cnt, cnt_from, cnt_to);
    if (ce_out) {
        FILE *o = fopen(ce_out, "w");
        unsigned long distinct = 0;
        if (!o) { perror(ce_out); return 1; }
        fprintf(o, "# site target lr count (mgba_run --call-edges; %lu edges lost to a full table)\n", ce_lost);
        for (unsigned k = 0; k < NEDGE; k++)
            if (edges[k].n) {
                fprintf(o, "%08x %08x %08x %lu\n", edges[k].site, edges[k].target, edges[k].lr, edges[k].n);
                distinct++;
            }
        fclose(o);
        printf("call-edges: %lu distinct (site, target, lr)%s\n", distinct, ce_lost ? " (table full: some lost)" : "");
    }
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
        unsigned when = game_frame_addr ? game_counter(core, game_frame_addr) : 0;
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
