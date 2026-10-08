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

/* Call a lifted (or hand-written) guest function from readable code. Set the
 * argument registers (R0..R3) first; results come back in R0..R3. */
#define GUEST_CALL(fn) do { LR = 0xFFFFFFF1u; run(fn); } while (0)

/* Return to an explicit guest address (capture LR on entry before using
 * GUEST_CALL, which overwrites it). */
#define RETURN_TO(addr) do { cpu.ret = (u32)(addr) & ~1u; return DONE; } while (0)
