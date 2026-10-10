/*
 * memmap.c — host test builds: map the GBA address space at its real
 * addresses so lifted and hand-written code can use guest pointers directly.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "cpu.h"
#include "runtime.h"
#include "hostmem.h"

const HostRegion g_regions[] = {
    { "ewram", 0x02000000u, 0x40000 },
    { "iwram", 0x03000000u, 0x8000 },
    { "io",    0x04000000u, 0x400 },
    { "pal",   0x05000000u, 0x400 },
    { "vram",  0x06000000u, 0x18000 },
    { "oam",   0x07000000u, 0x400 },
    { "sram",  0x0E000000u, 0x8000 },
};
const unsigned g_region_count = sizeof g_regions / sizeof g_regions[0];

static void map_at(u32 addr, size_t size) {
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != (void *)(uintptr_t)addr) {
        fprintf(stderr, "cannot map guest memory at %#x\n", addr);
        exit(1);
    }
}

void host_map_memory(const char *rom_path) {
    for (unsigned i = 0; i < g_region_count; i++)
        map_at(g_regions[i].base, g_regions[i].size);
    FILE *f = fopen(rom_path, "rb");
    if (!f) { perror(rom_path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    size_t rom_size = ((size_t)n + 0xFFF) & ~(size_t)0xFFF;
    map_at(0x08000000u, rom_size);
    if (fread((void *)(uintptr_t)0x08000000u, 1, (size_t)n, f) != (size_t)n) { perror("read rom"); exit(1); }
    fclose(f);
    /* Read-only, so a store into ROM faults instead of changing the ROM image
     * (the store macros in cpu.h no longer skip ROM addresses either). */
    if (mprotect((void *)(uintptr_t)0x08000000u, rom_size, PROT_READ)) { perror("mprotect rom"); exit(1); }
}

void rt_log(const char *msg, u32 value) { fprintf(stderr, "[log] %s %#x\n", msg, value); }

void rt_fatal(const char *msg, u32 value) {
    fprintf(stderr, "[fatal] %s %#x\n", msg, value);
    abort();
}
