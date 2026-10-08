/*
 * game.h — common header for hand-written (readable) replacements of lifted
 * functions.
 *
 * A replacement keeps the lifted calling convention: it is `Next name(void)`,
 * reads its arguments from the guest registers (R0..R12, LR) and returns to
 * the guest caller. Everything else is ordinary C: guest addresses are real
 * pointers on hardware (and in host test builds, where guest RAM is mapped at
 * the same addresses).
 */
#pragma once
#include "cpu.h"
#include "lifted.h"

#if defined(TARGET_GBA)
/* Hot code: ARM state, run from IWRAM like the routines the game copied there. */
#define IWRAM_CODE __attribute__((section(".iwram"), target("arm"), noinline))
#else
#define IWRAM_CODE
#endif

#define GUEST_PTR(type, addr) ((type *)(uintptr_t)(u32)(addr))

/* Return to the guest caller (the address in LR). */
#define RETURN_TO_CALLER() do { cpu.ret = LR & ~1u; return DONE; } while (0)
