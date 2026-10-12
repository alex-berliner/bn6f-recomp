/* blockcov.c: the coverage build's runtime (tools/blockcov/blockcov.h). Compiled only into a
 * coverage build of the host difftest. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
/* blockcov.h comes first through -include (the coverage build) */

CovShared *g_cov;
u32 *g_cov_stamp;

/* Shared with every forked child: mapped before main, so before any fork. */
__attribute__((constructor)) static void cov_init(void) {
    g_cov = mmap(NULL, sizeof *g_cov, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    g_cov_stamp = mmap(NULL, (0x400000u + 0x4000u) * sizeof(u32), PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_cov == MAP_FAILED || g_cov_stamp == MAP_FAILED) { perror("blockcov mmap"); exit(2); }
}

void cov_trial_begin(void) {
    g_cov->gen++;
    g_cov->nhits = 0;
    g_cov->nloads = 0;
    g_cov->logloads = 0;
}

/* A compared trial: what the lifted original reached (and, with BLOCKCOV_LOADS set, what it loaded). */
void cov_trial_end(const char *name, const char *label, int t) {
    printf("cov %s %s %d hits=", name, label ? label : "random", t);
    for (u32 i = 0; i < g_cov->nhits; i++)
        printf(i ? ",%x" : "%x", g_cov->hits[i]);
    if (getenv("BLOCKCOV_LOADS")) {
        printf(" loads=");
        for (u32 i = 0; i < g_cov->nloads; i++)
            printf(i ? ",%x:%u:%x" : "%x:%u:%x", g_cov->loads[i].addr, g_cov->loads[i].size, g_cov->loads[i].value);
    }
    printf("\n");
    fflush(stdout);
}
