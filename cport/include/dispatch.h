#pragma once
#include "cpu.h"

extern const u32 g_dispatch_rom_end;
extern const GuestFn g_dispatch_rom[];
extern const GuestFn g_dispatch_iwram[];

GuestFn dispatch_lookup(u32 addr);
