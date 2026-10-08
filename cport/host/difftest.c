/*
 * difftest — check hand-written replacements against their lifted originals.
 *
 *   difftest [-n TRIALS] [-s SEED] [NAME...]      (default: every pair)
 *
 * For each trial both versions run (in forked children, 2 s timeout) from the
 * same randomised guest RAM and registers; all guest memory, cpu.ret and the
 * registers listed for the function in difftest.cfg must match.
 */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "lifted.h"
#include "hostmem.h"

typedef struct { const char *name; GuestFn hand, lifted; } Pair;
/* Optional structured input generator: fills guest RAM/registers in place. */
typedef void (*GenFn)(Cpu *in, u32 (*rnd)(void));
GenFn difftest_generator(const char *name);
#define PAIR(n) { #n, n, L_##n },
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
} Cfg;
static Cfg g_cfg[256];
static int g_ncfg;

static void load_cfg(const char *path) {
    FILE *f = fopen(path, "r");
    char line[1024];
    while (f && fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        Cfg *c = &g_cfg[g_ncfg++];
        memset(c, 0, sizeof *c);
        char *tok = strtok(line, " \t\n");
        snprintf(c->name, sizeof c->name, "%s", tok);
        while ((tok = strtok(NULL, " \t\n"))) {
            if (!strncmp(tok, "compare=", 8)) {
                for (char *r = strtok(tok + 8, ","); r; r = strtok(NULL, ","))
                    c->compare_mask |= 1u << atoi(r + 1);
                break;   /* strtok state reused; compare= must be last */
            }
            if (tok[0] == 'r' && strchr(tok, '=')) {
                int reg = atoi(tok + 1);
                char *v = strchr(tok, '=') + 1;
                c->has_choice[reg] = 1;
                for (char *p = v; p && *p && c->nchoices[reg] < 16; ) {
                    c->choices[reg][c->nchoices[reg]++] = (u32)strtoul(p, &p, 0);
                    if (*p == '|') p++; else break;
                }
            }
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

static void die(int sig) { (void)sig; _exit(3); }

static void run_child(GuestFn f, const Cpu *in, const u8 *mem_in, Result *res, u8 *mem_out) {
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGSEGV, die);
        signal(SIGBUS, die);
        signal(SIGALRM, die);
        signal(SIGABRT, die);
        signal(SIGFPE, die);
        alarm(2);
        load_mem(mem_in);
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

static int test_pair(const Pair *p, int trials) {
    const Cfg *c = cfg_for(p->name);
    size_t n = snapshot_size();
    u8 *mem_in = malloc(n);
    u8 *out_a = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    u8 *out_b = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Result *ra = mmap(NULL, sizeof *ra, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Result *rb = mmap(NULL, sizeof *rb, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    int fails = 0, crashes = 0;
    for (int t = 0; t < trials; t++) {
        for (size_t i = 0; i < n; i += 4) {
            u32 v = rnd();
            memcpy(mem_in + i, &v, 4);
        }
        Cpu in;
        memset(&in, 0, sizeof in);
        for (int r = 0; r < 13; r++) {
            if (c && c->has_choice[r])
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
        run_child(p->lifted, &in, mem_in, ra, out_a);
        run_child(p->hand, &in, mem_in, rb, out_b);
        if (ra->status) { crashes++; continue; }   /* original faults: input out of domain */
        const char *why = NULL;
        /* Stack below the final SP is dead after return (callee scratch, or a
         * caller frame the function popped). */
        u32 dead_top = ra->cpu.r[13] > GUEST_SP ? ra->cpu.r[13] : GUEST_SP;
        if (dead_top > GUEST_SP + 0x100) dead_top = GUEST_SP + 0x100;
        size_t lo = g_regions[0].size + (GUEST_SP - STACK_SCRATCH - 0x03000000u);
        size_t hi = g_regions[0].size + (dead_top - 0x03000000u);
        memcpy(out_b + lo, out_a + lo, hi - lo);
        if (rb->status) why = "hand-written version crashed";
        else if (memcmp(out_a, out_b, n)) why = "guest memory differs";
        else if (ra->cpu.ret != rb->cpu.ret) why = "return target differs";
        else if (ra->cpu.r[13] != rb->cpu.r[13]) why = "SP differs";
        else if (c) {
            for (int r = 0; r < 13; r++)
                if ((c->compare_mask >> r & 1) && ra->cpu.r[r] != rb->cpu.r[r]) why = "register differs";
        }
        if (why) {
            if (fails++ < 5) {
                fprintf(stderr, "  %s trial %d: %s\n    inputs:", p->name, t, why);
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
            }
        }
    }
    printf("%-40s %s  (%d trials, %d failures, %d out-of-domain)\n", p->name,
           fails ? "FAIL" : "ok  ", trials, fails, crashes);
    return fails;
}

int main(int argc, char **argv) {
    int trials = 200;
    g_rng = 0x9E3779B97F4A7C15ull;
    int first = 1;
    for (; first < argc && argv[first][0] == '-'; first++) {
        if (!strcmp(argv[first], "-n")) trials = atoi(argv[++first]);
        else if (!strcmp(argv[first], "-s")) g_rng = strtoull(argv[++first], 0, 0) | 1;
    }
    struct rlimit nocore = { 0, 0 };
    setrlimit(RLIMIT_CORE, &nocore);
    host_map_memory(getenv("BN6F_ROM") ? getenv("BN6F_ROM") : "../bn6f/bn6f.gba");
    load_cfg(getenv("DIFFTEST_CFG") ? getenv("DIFFTEST_CFG") : "host/difftest.cfg");
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
