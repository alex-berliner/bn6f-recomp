#pragma once
#include "cpu.h"

typedef struct { const char *name; u32 base; u32 size; } HostRegion;
extern const HostRegion g_regions[];
extern const unsigned g_region_count;
void host_map_memory(const char *rom_path);
