/* runtime.h — services the lifted code and platform layers share. */
#pragma once
#include "cpu.h"

void    cpu_switch_mode(u32 new_ctl);
GuestFn reloc_lookup(u32 addr);
void    rt_log(const char *msg, u32 value);
__attribute__((noreturn)) void rt_fatal(const char *msg, u32 value);
