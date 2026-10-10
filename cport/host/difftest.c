/*
 * difftest — check hand-written replacements against their lifted originals.
 *
 *   difftest [-n TRIALS] [-s SEED] [NAME...]      (default: every pair)
 *   difftest -S DIR [-m N] NAME                   (replay captured states, plus
 *                                                  N varied copies of each)
 *
 * For each trial both versions run (in forked children, 0.2 s timeout) from
 * the same randomised guest RAM and registers (out of domain when the original
 * faults, or returns elsewhere though it never unwinds); all guest memory, cpu.ret, SP
 * and what the function's register contract compares must match: every r0-r12
 * it does not leave free and the flags callers read (the lifter's refined
 * contract, in difftest_pairs.h), or the function's compare= line in
 * difftest.cfg when it has one.
 *
 * With -S, the inputs are real machine states captured at the function's
 * entry in the original game (tools/mgba_run --snap), for functions whose
 * inputs are structured guest state that random data cannot exercise.
 *
 *   difftest -L CAND.so -R r0,r1 [-F nz,c,v] [-S DIR | -n TRIALS] NAME
 *
 * Candidate mode (tools/convert): cand_NAME from the shared library CAND.so
 * against the lifted NAME, comparing guest memory, SP, the return target,
 * the registers given with -R and the flags given with -F (nz = N and Z),
 * unless difftest.cfg has a compare= line for NAME.
 */
#include <dlfcn.h>
#include <dirent.h>
#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lifted.h"
#include "lifted_originals.h"   /* L_NAME, for difftest_pairs.h; only the difftest reads it */
#include "dispatch.h"
#include "hostmem.h"

/* compare_regs/compare_flags: the lifter's contract for the function (difftest_pairs.h), what is
 * compared after a call unless difftest.cfg gives the function a compare= line. returns: the
 * lifter proves the original always returns to its caller (it never unwinds past it). */
typedef struct { const char *name; GuestFn hand, lifted; u32 compare_regs; int compare_flags, returns; } Pair;
/* Optional structured input generator: fills guest RAM/registers in place. */
typedef void (*GenFn)(Cpu *in, u32 (*rnd)(void));
GenFn difftest_generator(const char *name);
GenFn difftest_snap_mutator(const char *name);
#define PAIR(n, regs, flags, returns) { #n, n, L_##n, regs, flags, returns },
static const Pair kPairs[] = {
#include "difftest_pairs.h"
};

/* Per-function test configuration (difftest.cfg): registers to compare and
 * fixed/limited input values. */
typedef struct {
    char name[128];
    u32 compare_mask;            /* bit n = compare rn after return */
    int has_choice[13];
    u32 choices[13][16];
    int nchoices[13];
    u32 ignore_addr[4], ignore_len[4];   /* regions allowed to differ */
    int nignore;
    int hwtick;                          /* simulate a running VCOUNT/DISPSTAT */
    int compare_flags;                   /* compare flags too: 1 N/Z, 2 C, 4 V */
    int has_compare;                     /* a compare= line (else the pair's contract) */
} Cfg;
/* One entry per configured function; grows (the generated cfg lines of
 * tools/abi/gen.py can number in the thousands). */
static Cfg *g_cfg;
static int g_ncfg, g_cfg_cap;

static Cfg *new_cfg(const char *name) {
    if (g_ncfg == g_cfg_cap) {
        g_cfg_cap = g_cfg_cap ? g_cfg_cap * 2 : 256;
        g_cfg = realloc(g_cfg, (size_t)g_cfg_cap * sizeof *g_cfg);
        if (!g_cfg) { perror("difftest.cfg"); exit(2); }
    }
    Cfg *c = &g_cfg[g_ncfg++];
    memset(c, 0, sizeof *c);
    snprintf(c->name, sizeof c->name, "%s", name);
    return c;
}

/* A setting the parser cannot use is skipped, loudly (it used to be skipped
 * silently, or to write past the arrays). */
static void cfg_warn(const char *path, int lineno, const char *what, const char *tok) {
    fprintf(stderr, "%s:%d: %s, ignored: %s\n", path, lineno, what, tok);
}

/* "rN" (N 0..12) as in compare= and rN=, or -1. */
static int cfg_reg(const char *tok) {
    char *end;
    if (tok[0] != 'r' || tok[1] < '0' || tok[1] > '9') return -1;
    long n = strtol(tok + 1, &end, 10);
    return (n <= 12 && (*end == '\0' || *end == '=')) ? (int)n : -1;
}

static void load_cfg(const char *path) {
    FILE *f = fopen(path, "r");
    char line[4096];
    int lineno = 0;
    while (f && fgets(line, sizeof line, f)) {
        lineno++;
        if (!strchr(line, '\n') && !feof(f)) {     /* too long: drop the whole line */
            cfg_warn(path, lineno, "line longer than 4095 characters", "");
            int ch;
            while ((ch = fgetc(f)) != EOF && ch != '\n')
                ;
            continue;
        }
        if (line[0] == '#' || line[0] == '\n') continue;
        char *tok = strtok(line, " \t\n");
        if (!tok) continue;
        Cfg *c = new_cfg(tok);
        while ((tok = strtok(NULL, " \t\n"))) {
            if (!strncmp(tok, "compare=", 8)) {
                c->has_compare = 1;
                for (char *r = strtok(tok + 8, ","); r; r = strtok(NULL, ",")) {
                    int reg = cfg_reg(r);
                    if (reg >= 0) c->compare_mask |= 1u << reg;
                    else if (!strcmp(r, "nz")) c->compare_flags |= 1;
                    else if (!strcmp(r, "c")) c->compare_flags |= 2;
                    else if (!strcmp(r, "v")) c->compare_flags |= 4;
                    else cfg_warn(path, lineno, "compare= takes r0..r12, nz, c, v", r);
                }
                break;   /* strtok state reused; compare= must be last */
            }
            if (!strcmp(tok, "hwtick")) { c->hwtick = 1; continue; }
            if (!strncmp(tok, "ignore=", 7)) {
                char *p = tok + 7;
                u32 addr = (u32)strtoul(p, &p, 0);
                if (*p != ':') { cfg_warn(path, lineno, "ignore= wants ADDR:LEN", tok); continue; }
                if (c->nignore == (int)(sizeof c->ignore_addr / sizeof c->ignore_addr[0])) {
                    cfg_warn(path, lineno, "more than 4 ignore= regions", tok);
                    continue;
                }
                c->ignore_addr[c->nignore] = addr;
                c->ignore_len[c->nignore++] = (u32)strtoul(p + 1, NULL, 0);
                continue;
            }
            if (tok[0] == 'r' && strchr(tok, '=')) {
                int reg = cfg_reg(tok);
                if (reg < 0) { cfg_warn(path, lineno, "rN= takes r0..r12", tok); continue; }
                char *v = strchr(tok, '=') + 1;
                for (char *p = v; p && *p; ) {
                    if (c->nchoices[reg] == 16) { cfg_warn(path, lineno, "more than 16 values", tok); break; }
                    c->choices[reg][c->nchoices[reg]++] = (u32)strtoul(p, &p, 0);
                    if (*p == '|') p++; else break;
                }
                c->has_choice[reg] = c->nchoices[reg] > 0;
                continue;
            }
            cfg_warn(path, lineno, "unknown setting", tok);
        }
    }
    if (f) fclose(f);
}

static const Cfg *cfg_for(const char *name) {
    for (int i = 0; i < g_ncfg; i++)
        if (!strcmp(g_cfg[i].name, name)) return &g_cfg[i];
    return NULL;
}

static u64 g_rng;
static u32 rnd(void) {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17;
    return (u32)(g_rng >> 16);
}

static u32 random_value(void) {
    switch (rnd() % 5) {
    case 0: return 0x02000000u + (rnd() % 0x40000 & ~3u);   /* EWRAM pointer */
    case 1: return 0x03000000u + (rnd() % 0x7000 & ~3u);    /* IWRAM pointer */
    case 2: return rnd() % 64;
    case 3: return (u32)(s32)(rnd() % 256 - 128);
    default: return rnd();
    }
}

#define GUEST_SP 0x03007D00u
#define STACK_SCRATCH 0x400u

typedef struct {
    Cpu cpu;
    int status;          /* 0 ok, 1 crashed/timeout */
} Result;

static size_t snapshot_size(void) {
    size_t n = 0;
    for (unsigned i = 0; i < g_region_count; i++) n += g_regions[i].size;
    return n;
}
static void save_mem(u8 *buf) {
    for (unsigned i = 0; i < g_region_count; i++) {
        memcpy(buf, (void *)(uintptr_t)g_regions[i].base, g_regions[i].size);
        buf += g_regions[i].size;
    }
}
static void load_mem(const u8 *buf) {
    for (unsigned i = 0; i < g_region_count; i++) {
        memcpy((void *)(uintptr_t)g_regions[i].base, buf, g_regions[i].size);
        buf += g_regions[i].size;
    }
}

static void die(int sig) {
    if (getenv("DIFFTEST_VERBOSE")) {
        char msg[64];
        int n = snprintf(msg, sizeof msg, "[child] signal %d, guest pc-ish lr=%#x\n", sig, cpu.r[14]);
        write(2, msg, n);
    }
    _exit(3);
}

/* A memory fault: with DIFFTEST_VERBOSE, say where (a store into the
 * read-only ROM is the one cause worth naming: host/memmap.c). */
static void die_fault(int sig, siginfo_t *si, void *ctx) {
    (void)ctx;
    if (getenv("DIFFTEST_VERBOSE")) {
        u32 a = (u32)(uintptr_t)si->si_addr;
        char msg[96];
        int n = snprintf(msg, sizeof msg, "[child] %s at %#x, guest pc-ish lr=%#x\n",
                         a - 0x08000000u < 0x06000000u && si->si_code == SEGV_ACCERR ? "store into ROM"
                                                                                    : "bad access",
                         a, cpu.r[14]);
        write(2, msg, n);
    }
    (void)sig;
    _exit(3);
}

/* For functions that wait on the display: advance VCOUNT and the DISPSTAT
 * VBlank flag in the background, like the LCD would. */
static void *hw_ticker(void *arg) {
    (void)arg;
    volatile u16 *dispstat = (volatile u16 *)(uintptr_t)0x04000004u;
    volatile u16 *vcount = (volatile u16 *)(uintptr_t)0x04000006u;
    for (u32 line = 0;; line = (line + 1) % 228) {
        *vcount = (u16)line;
        *dispstat = (u16)((*dispstat & ~1u) | (line >= 160 && line < 227));
        for (volatile int k = 0; k < 200; k++)
            ;
    }
    return NULL;
}

static int g_hwtick;

static void run_child(GuestFn f, const Cpu *in, const u8 *mem_in, Result *res, u8 *mem_out) {
    if (getenv("DIFFTEST_NOFORK")) {          /* debugging: run in-process */
        load_mem(mem_in);
        cpu = *in;
        run(f);
        res->cpu = cpu;
        res->status = 0;
        save_mem(mem_out);
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        if (!getenv("DIFFTEST_VERBOSE"))       /* out-of-domain inputs abort noisily */
            freopen("/dev/null", "w", stderr);
        struct sigaction fault = { .sa_sigaction = die_fault, .sa_flags = SA_SIGINFO };
        sigaction(SIGSEGV, &fault, NULL);
        sigaction(SIGBUS, &fault, NULL);
        signal(SIGALRM, die);
        signal(SIGABRT, die);
        signal(SIGFPE, die);
        /* Functions finish in microseconds; anything still running after
         * 0.2 s is spinning on hardware state the host model does not
         * simulate (e.g. polling VCOUNT): out of domain. With hwtick the
         * VCOUNT it polls comes from a ticker thread, which a loaded machine
         * (a parallel verify) can starve for longer: 2 s. */
        struct itimerval t = { { 0, 0 }, { g_hwtick ? 2 : 0, g_hwtick ? 0 : 200000 } };
        setitimer(ITIMER_REAL, &t, NULL);
        load_mem(mem_in);
        if (g_hwtick) {
            pthread_t th;
            pthread_create(&th, NULL, hw_ticker, NULL);
        }
        cpu = *in;
        run(f);
        res->cpu = cpu;
        res->status = 0;
        save_mem(mem_out);
        _exit(0);
    }
    int st;
    waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        res->status = 1;
        memset(&res->cpu, 0, sizeof res->cpu);
        memcpy(mem_out, mem_in, snapshot_size());
    }
}

static const char *g_snap_dir;
static int g_mutations = 4;
static const char *g_truth_dir;   /* -m: varied copies per snapshot (functions with a mutator) */
static void check_trial(const Pair *p, const Cfg *c, int t, const Cpu *inp, const u8 *mem_in, size_t n,
                        u8 *out_a, u8 *out_b, Result *ra, Result *rb, int *pfails, int *pcrashes,
                        const char *label);

static int cmp_str(const void *a, const void *b) { return strcmp(*(char **)a, *(char **)b); }
static int list_snaps(const char *dir, char ***out) {
    DIR *d = opendir(dir);
    int n = 0, cap = 0;
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (strncmp(e->d_name, "snap_", 5) || strstr(e->d_name, ".exit")) continue;
        if (n == cap) *out = realloc(*out, (cap = cap ? cap * 2 : 64) * sizeof **out);
        (*out)[n] = malloc(strlen(dir) + strlen(e->d_name) + 2);
        sprintf((*out)[n++], "%s/%s", dir, e->d_name);
    }
    if (d) closedir(d);
    qsort(*out, n, sizeof **out, cmp_str);
    return n;
}

/* Snapshot (tools/mgba_run --snap): "BN6S", version, r0-r15, cpsr, memory. */
static int load_snapshot(const char *path, Cpu *in, u8 *mem) {
    FILE *f = fopen(path, "rb");
    u32 hdr[19];
    if (!f || fread(hdr, sizeof hdr, 1, f) != 1 || hdr[0] != 0x53364E42u) {
        fprintf(stderr, "bad snapshot %s\n", path);
        if (f) fclose(f);
        return 0;
    }
    size_t n = snapshot_size();
    size_t got = fread(mem, 1, n, f);
    fclose(f);
    if (got != n) { fprintf(stderr, "short snapshot %s\n", path); return 0; }
    memset(in, 0, sizeof *in);
    for (int r = 0; r < 15; r++) in->r[r] = hdr[2 + r];
    u32 cpsr = hdr[18];
    u32 nf = cpsr >> 31 & 1, zf = cpsr >> 30 & 1;
    in->nz = zf ? 0 : (nf ? 0x80000000u : 1u);
    in->c = cpsr >> 29 & 1;
    in->v = cpsr >> 28 & 1;
    in->cpsr_ctl = cpsr & 0xFF;
    in->bank_sp[BANK_IRQ] = 0x03007F60u;
    in->bank_sp[BANK_SVC] = 0x03007FE0u;
    return 1;
}

static int test_pair(const Pair *p, int trials) {
    const Cfg *given = cfg_for(p->name);
    Cfg cfg;    /* the function's difftest.cfg line, comparing what its contract says unless it has compare= */
    if (given) cfg = *given;
    else memset(&cfg, 0, sizeof cfg);
    if (!cfg.has_compare) {
        cfg.compare_mask = p->compare_regs;
        cfg.compare_flags = p->compare_flags;
    }
    const Cfg *c = &cfg;
    g_hwtick = c->hwtick;
    size_t n = snapshot_size();
    u8 *mem_in = malloc(n);
    u8 *out_a = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    u8 *out_b = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Result *ra = mmap(NULL, sizeof *ra, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Result *rb = mmap(NULL, sizeof *rb, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int fails = 0, crashes = 0;
    if (g_snap_dir) {
        char **files = NULL;
        int nf = list_snaps(g_snap_dir, &files);
        GenFn mut = difftest_snap_mutator(p->name);
        int runs = 0;
        for (int t = 0; t < nf; t++) {
            /* the captured state as is, then g_mutations varied copies */
            for (int k = 0; k <= (mut ? g_mutations : 0); k++, runs++) {
                Cpu in;
                if (!load_snapshot(files[t], &in, mem_in)) break;
                if (k) {
                    load_mem(mem_in);
                    mut(&in, rnd);
                    save_mem(mem_in);
                }
                check_trial(p, c, t, &in, mem_in, n, out_a, out_b, ra, rb, &fails, &crashes, files[t]);
            }
        }
        printf("%-40s %s  (%d snapshots, %d runs, %d failures, %d out-of-domain)\n", p->name,
               fails ? "FAIL" : "ok  ", nf, runs, fails, crashes);
        return fails;
    }
    for (int t = 0; t < trials; t++) {
        for (size_t i = 0; i < n; i += 4) {
            u32 v = rnd();
            memcpy(mem_in + i, &v, 4);
        }
        Cpu in;
        memset(&in, 0, sizeof in);
        for (int r = 0; r < 13; r++) {
            if (c->has_choice[r])
                in.r[r] = c->choices[r][rnd() % c->nchoices[r]];
            else
                in.r[r] = random_value();
        }
        in.r[13] = GUEST_SP;
        in.r[14] = 0x0BAD0000u | (rnd() & 0xFFFE);
        in.nz = rnd(); in.c = rnd() & 1; in.v = rnd() & 1;
        in.cpsr_ctl = 0x1F;
        in.bank_sp[BANK_IRQ] = 0x03007F60u;
        GenFn gen = difftest_generator(p->name);
        if (gen) {
            load_mem(mem_in);
            gen(&in, rnd);
            save_mem(mem_in);
        }
        check_trial(p, c, t, &in, mem_in, n, out_a, out_b, ra, rb, &fails, &crashes, NULL);
    }
    printf("%-40s %s  (%d trials, %d failures, %d out-of-domain)\n", p->name,
           fails ? "FAIL" : "ok  ", trials, fails, crashes);
    return fails;
}

static void check_trial(const Pair *p, const Cfg *c, int t, const Cpu *inp, const u8 *mem_in, size_t n,
                        u8 *out_a, u8 *out_b, Result *ra, Result *rb, int *pfails, int *pcrashes,
                        const char *label) {
        Cpu in = *inp;
        in.ret = in.r[14] & ~1u;     /* what a caller expects back (RETURN_NR leaves it) */
        int fails = *pfails;
        if (getenv("DIFFTEST_ONLY") && atoi(getenv("DIFFTEST_ONLY")) != t) return;
        run_child(p->lifted, &in, mem_in, ra, out_a);
        run_child(p->hand, &in, mem_in, rb, out_b);
        if (ra->status) { (*pcrashes)++; return; }   /* original faults: input out of domain */
        /* An original that never unwinds coming back elsewhere: the input overwrote its own stack
         * frame (e.g. a fill over its pushed LR), so it is out of domain too. */
        if (p->returns && ra->cpu.ret != in.ret) { (*pcrashes)++; return; }
        const char *why = NULL;
        /* Stack below the final SP is dead after return (callee scratch, or a
         * caller frame the function popped). */
        u32 sp0 = in.r[13];
        u32 dead_top = ra->cpu.r[13] > sp0 ? ra->cpu.r[13] : sp0;
        if (dead_top > sp0 + 0x100) dead_top = sp0 + 0x100;
        size_t lo = g_regions[0].size + (sp0 - STACK_SCRATCH - 0x03000000u);
        size_t hi = g_regions[0].size + (dead_top - 0x03000000u);
        memcpy(out_b + lo, out_a + lo, hi - lo);
        if (g_hwtick) {
            size_t io = g_regions[0].size + g_regions[1].size;
            memcpy(out_b + io, out_a + io, 8);
        }
        for (int k = 0; k < c->nignore; k++) {
            size_t off = 0;
            for (unsigned r = 0; r < g_region_count; r++) {
                if (c->ignore_addr[k] - g_regions[r].base < g_regions[r].size) {
                    off += c->ignore_addr[k] - g_regions[r].base;
                    memcpy(out_b + off, out_a + off, c->ignore_len[k]);
                    break;
                }
                off += g_regions[r].size;
            }
        }
        if (rb->status) why = "hand-written version crashed";
        else if (memcmp(out_a, out_b, n)) why = "guest memory differs";
        else if (ra->cpu.ret != rb->cpu.ret) why = "return target differs";
        else if (ra->cpu.r[13] != rb->cpu.r[13]) why = "SP differs";
        else {
            for (int r = 0; r < 13; r++)
                if ((c->compare_mask >> r & 1) && ra->cpu.r[r] != rb->cpu.r[r]) why = "register differs";
            if (!why && (c->compare_flags & 1) &&
                (((s32)ra->cpu.nz < 0) != ((s32)rb->cpu.nz < 0) || (ra->cpu.nz == 0) != (rb->cpu.nz == 0)))
                why = "N/Z flags differ";
            if (!why && (c->compare_flags & 2) && (ra->cpu.c & 1) != (rb->cpu.c & 1)) why = "C flag differs";
            if (!why && (c->compare_flags & 4) && (ra->cpu.v & 1) != (rb->cpu.v & 1)) why = "V flag differs";
        }
        if (why) {
            if (fails++ < 5) {
                fprintf(stderr, "  %s %s %d: %s\n    inputs:", p->name, label ? label : "trial", t, why);
                for (int r = 0; r < 13; r++) fprintf(stderr, " r%d=%#x", r, in.r[r]);
                fprintf(stderr, "\n");
                for (size_t i = 0; i < n && !strcmp(why, "guest memory differs"); i++)
                    if (out_a[i] != out_b[i]) {
                        size_t off = i;
                        unsigned k = 0;
                        while (off >= g_regions[k].size) off -= g_regions[k++].size;
                        fprintf(stderr, "    first diff at %#zx: lifted %02x hand %02x\n",
                                g_regions[k].base + off, out_a[i], out_b[i]);
                        break;
                    }
                for (int r = 0; r < 13 && !strcmp(why, "register differs"); r++)
                    if ((c->compare_mask >> r & 1) && ra->cpu.r[r] != rb->cpu.r[r])
                        fprintf(stderr, "    r%d: lifted %#x hand %#x\n", r, ra->cpu.r[r], rb->cpu.r[r]);
            }
        }
        *pfails = fails;
}

/* -G DIR: ground truth. Each snap_NNNN.bin (a function's entry state in the
 * original game) is run through the lifted code and the result compared with
 * snap_NNNN.exit.bin (the original's state when that call returned). Memory
 * below the final SP and the IWRAM block holding the original's code copies
 * are not compared; interrupts that ran during the call show up as extra
 * differences (sound state, frame counters). */
/* ORACLE_CONTRACT=FILE ("ENTRY_HEX REGMASK_HEX FLAGS" lines, from the
 * lifter's per-function contracts): compare only the registers callers
 * rely on (not those listed as clobbered) and the flags they read
 * (FLAGS bits: 1 N/Z, 2 C, 4 V); a crash of the lifted code (an access the
 * host does not map, e.g. from a random input) is reported as out of domain.
 * ORACLE_LIFTED=1 runs the lifted original (L_NAME) of each hand-written
 * function instead, so a difference can be checked against the code the
 * conversion replaced (callees stay as they are in the port). */
typedef struct { u32 entry, mask, flags; } Contract;
static Contract *g_contracts;
static int g_ncontracts;
static void load_contracts(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    unsigned e, m, fl;
    int cap = 0;
    while (fscanf(f, "%x %x %x", &e, &m, &fl) == 3) {
        if (g_ncontracts == cap) g_contracts = realloc(g_contracts, (cap = cap ? cap * 2 : 4096) * sizeof *g_contracts);
        g_contracts[g_ncontracts++] = (Contract){ e, m, fl };
    }
    fclose(f);
}
static const Contract *contract_for(u32 pc) {
    for (int i = 0; i < g_ncontracts; i++)
        if (g_contracts[i].entry == (pc & ~1u)) return &g_contracts[i];
    return NULL;
}

static int ground_truth(const char *dir) {
    if (getenv("ORACLE_CONTRACT")) load_contracts(getenv("ORACLE_CONTRACT"));
    char **files = NULL;
    int nf = list_snaps(dir, &files), bad = 0;
    size_t n = snapshot_size();
    u8 *in_mem = malloc(n), *exp_mem = malloc(n);
    u8 *out = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Result *res = mmap(NULL, sizeof *res, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    for (int t = 0; t < nf; t++) {
        char exit_path[1024];
        snprintf(exit_path, sizeof exit_path, "%.*s.exit.bin", (int)(strlen(files[t]) - 4), files[t]);
        Cpu in, exp;
        if (!load_snapshot(files[t], &in, in_mem) || !load_snapshot(exit_path, &exp, exp_mem)) continue;
        u32 pc;
        { FILE *f = fopen(files[t], "rb"); u32 h[19]; fread(h, sizeof h, 1, f); fclose(f); pc = h[17]; }
        GuestFn fn = dispatch_lookup(pc | ((in.cpsr_ctl & 0x20) ? 1 : 0));
        if (!fn) { printf("%s: no lifted function at %#x\n", files[t], pc); continue; }
        if (getenv("ORACLE_LIFTED"))      /* baseline: the lifted original in place of a hand-written override */
            for (unsigned i = 0; i < sizeof kPairs / sizeof kPairs[0]; i++)
                if (kPairs[i].hand == fn) { fn = kPairs[i].lifted; break; }
        run_child(fn, &in, in_mem, res, out);
        const Contract *ct = contract_for(pc);
        if (res->status && ct) { printf("%s: out of domain\n", files[t] + strlen(dir) + 1); continue; }
        if (res->status) { printf("%s: lifted code crashed\n", files[t]); bad++; continue; }
        int diffs = 0, mem_diffs = 0;
        size_t off = 0;
        for (unsigned r = 0; r < g_region_count; r++) {
            for (u32 a = 0; a < g_regions[r].size; a++) {
                u32 addr = g_regions[r].base + a;
                if (addr >= 0x03005700u && addr < 0x030079D4u) continue;           /* code copies */
                if (addr >= 0x03007E00u && addr < 0x03008000u) continue;           /* IRQ/SVC stacks, BIOS */
                if (addr >= 0x04000000u && addr < 0x04000400u) {                   /* I/O: write-only regs read as open bus */
                    if (getenv("ORACLE_SHOW_IO") && out[off + a] != exp_mem[off + a])   /* debugging: report, do not fail */
                        printf("  %s: io %#x lifted %02x original %02x\n", files[t], addr, out[off + a], exp_mem[off + a]);
                    continue;
                }
                if (addr >= 0x03007900u - 0x400 && addr < exp.r[13]) continue;      /* dead stack */
                if (out[off + a] != exp_mem[off + a]) {
                    u32 b = a;
                    while (b + 1 < g_regions[r].size && out[off + b + 1] != exp_mem[off + b + 1]) b++;
                    mem_diffs++;
                    if (diffs++ < 12)
                        printf("  %s: %#x..%#x lifted %02x.. original %02x..\n", files[t] + strlen(dir) + 1,
                               addr, g_regions[r].base + b, out[off + a], exp_mem[off + a]);
                    a = b;
                }
            }
            off += g_regions[r].size;
        }
        for (int r = 0; r < 16; r++)
            if (r != 15 && (!ct || (r < 13 && (ct->mask >> r & 1))) && res->cpu.r[r] != exp.r[r] && diffs++ < 20)
                printf("  %s: r%d lifted %#x original %#x\n", files[t] + strlen(dir) + 1, r, res->cpu.r[r], exp.r[r]);
        if (ct) {
            int nz_bad = ((s32)res->cpu.nz < 0) != ((s32)exp.nz < 0) || (res->cpu.nz == 0) != (exp.nz == 0);
            if (((ct->flags & 1) && nz_bad) || ((ct->flags & 2) && (res->cpu.c & 1) != (exp.c & 1)) ||
                ((ct->flags & 4) && (res->cpu.v & 1) != (exp.v & 1))) {
                diffs++;
                printf("  %s: flags lifted nz=%#x c=%u v=%u original nz=%#x c=%u v=%u\n", files[t] + strlen(dir) + 1,
                       res->cpu.nz, res->cpu.c & 1, res->cpu.v & 1, exp.nz, exp.c & 1, exp.v & 1);
            }
        }
        if (diffs) bad++;
        printf("%s: %s (%d differences)\n", files[t] + strlen(dir) + 1,
               mem_diffs ? "MEM" : diffs ? "REG" : "ok", diffs);
    }
    printf("%d of %d calls differ from the original\n", bad, nf);
    return bad != 0;
}

int main(int argc, char **argv) {
    int trials = 200;
    const char *cand_lib = NULL, *cand_regs = "";
    int cand_flags = 0;
    g_rng = 0x9E3779B97F4A7C15ull;
    int first = 1;
    for (; first < argc && argv[first][0] == '-'; first++) {
        if (!strcmp(argv[first], "-n")) trials = atoi(argv[++first]);
        else if (!strcmp(argv[first], "-s")) g_rng = strtoull(argv[++first], 0, 0) | 1;
        else if (!strcmp(argv[first], "-S")) g_snap_dir = argv[++first];
        else if (!strcmp(argv[first], "-m")) g_mutations = atoi(argv[++first]);
        else if (!strcmp(argv[first], "-G")) g_truth_dir = argv[++first];
        else if (!strcmp(argv[first], "-L")) cand_lib = argv[++first];
        else if (!strcmp(argv[first], "-R")) cand_regs = argv[++first];
        else if (!strcmp(argv[first], "-F")) {
            const char *f = argv[++first];
            cand_flags = (strstr(f, "nz") ? 1 : 0) | (strchr(f, 'c') ? 2 : 0) | (strchr(f, 'v') ? 4 : 0);
        }
    }
    struct rlimit nocore = { 0, 0 };
    setrlimit(RLIMIT_CORE, &nocore);
    host_map_memory(getenv("BN6F_ROM") ? getenv("BN6F_ROM") : "../bn6f/bn6f.gba");
    load_cfg(getenv("DIFFTEST_CFG") ? getenv("DIFFTEST_CFG") : "host/difftest.cfg");
    if (g_truth_dir)
        return ground_truth(g_truth_dir);
    if (cand_lib) {     /* candidate mode: cand_NAME against the lifted NAME */
        if (first >= argc) { fprintf(stderr, "-L needs a function name\n"); return 2; }
        const char *name = argv[first];
        void *lib = dlopen(cand_lib, RTLD_NOW | RTLD_LOCAL);
        if (!lib) { printf("%s: cannot load candidate: %s\n", name, dlerror()); return 2; }
        char sym[160];
        snprintf(sym, sizeof sym, "cand_%s", name);
        /* the lifted original: L_NAME once NAME has a hand-written override */
        char lsym[160];
        snprintf(lsym, sizeof lsym, "L_%s", name);
        GuestFn hand = (GuestFn)dlsym(lib, sym), lifted = (GuestFn)dlsym(RTLD_DEFAULT, lsym);
        if (!lifted)
            lifted = (GuestFn)dlsym(RTLD_DEFAULT, name);
        if (!hand || !lifted) { printf("%s: missing %s\n", name, hand ? name : sym); return 2; }
        /* -R/-F take the place of the contract: a compare= line in difftest.cfg still wins */
        Pair pr = { name, hand, lifted, 0, cand_flags, 0 };
        for (unsigned i = 0; i < sizeof kPairs / sizeof kPairs[0]; i++)
            if (!strcmp(kPairs[i].name, name)) pr.returns = kPairs[i].returns;
        char regs[256];
        snprintf(regs, sizeof regs, "%s", cand_regs);
        for (char *r = strtok(regs, ","); r; r = strtok(NULL, ",")) {
            int reg = cfg_reg(r);
            if (reg >= 0) pr.compare_regs |= 1u << reg;
            else fprintf(stderr, "-R takes r0..r12, ignored: %s\n", r);
        }
        return test_pair(&pr, trials) != 0;
    }
    int fails = 0, ran = 0;
    for (unsigned i = 0; i < sizeof kPairs / sizeof kPairs[0]; i++) {
        int want = first >= argc;
        for (int a = first; a < argc; a++) want |= !strcmp(argv[a], kPairs[i].name);
        if (!want) continue;
        ran++;
        fails += test_pair(&kPairs[i], trials) != 0;
    }
    printf("%d function(s) tested, %d failing\n", ran, fails);
    return fails != 0;
}
