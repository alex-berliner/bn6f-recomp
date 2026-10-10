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
#include <stdbool.h>   /* typed functions return bool (docs/C_CALLING_CONVENTION.md) */

#if defined(TARGET_GBA)
/* Hot code: ARM state, run from IWRAM like the routines the game copied there. */
#define IWRAM_CODE __attribute__((section(IWRAM_SECTION), target("arm"), noinline))
/* Helpers of IWRAM code: same section and state, but may be inlined (a
 * plain static helper the compiler keeps out of line would land in ROM). */
#define IWRAM_HELPER __attribute__((section(IWRAM_SECTION), target("arm")))
/* IWRAM code where size matters more than ARM's speed (IWRAM is small):
 * Thumb also runs at one cycle per instruction there. */
#define IWRAM_THUMB __attribute__((section(IWRAM_SECTION), target("thumb"), noinline))
#else
#define IWRAM_CODE
#define IWRAM_HELPER
#define IWRAM_THUMB
#endif
/* A rarely taken path of IWRAM code, split out so it stays in ROM (as an
 * ordinary Thumb function) instead of taking IWRAM space; tools/lineprof.py
 * shows which paths of a function run. */
#define COLD_PATH __attribute__((noinline, cold))

#define GUEST_PTR(type, addr) ((type *)(uintptr_t)(u32)(addr))

/* Condition flags a function leaves for its callers, which may branch on
 * them right after the call (the lifted function's header lists them under
 * "outputs"): as after MOVS/ANDS/... with result x, after CMP a, b, or after
 * ADDS/CMN a, b. */
#define SET_FLAGS_NZ(x) (cpu.nz = (u32)(x))
#define SET_FLAGS_CMP(a, b) do { u32 a_ = (a), b_ = (b), r_ = a_ - b_; cpu.nz = r_; \
    cpu.c = a_ >= b_; cpu.v = ((a_ ^ b_) & (a_ ^ r_)) >> 31; } while (0)
#define SET_FLAGS_ADD(a, b) do { u32 a_ = (a), b_ = (b), r_ = a_ + b_; cpu.nz = r_; \
    cpu.c = r_ < a_; cpu.v = (~(a_ ^ b_) & (a_ ^ r_)) >> 31; } while (0)

/* The condition flags a callee left, for code that branches on them right
 * after a GUEST_CALL as the original does (`bl f; beq ...` is
 * `if (ZERO_FLAG())`). */
#define ZERO_FLAG()     (cpu.nz == 0)
#define NEGATIVE_FLAG() ((s32)cpu.nz < 0)
#define CARRY_FLAG()    (cpu.c != 0)
#define OVERFLOW_FLAG() (cpu.v != 0)

/* Return to the guest caller (the address in LR). */
#define RETURN_TO_CALLER() do { cpu.ret = LR & ~1u; return DONE; } while (0)

/* Call a lifted (or hand-written) guest function from readable code. Set the
 * argument registers (R0..R3) first; results come back in R0..R3. LR is kept,
 * so RETURN_TO_CALLER() still works afterwards. */
#define GUEST_CALL(fn) do { u32 lr_ = LR; LR = 0xFFFFFFF1u; run(fn); LR = lr_; } while (0)
/* The same through a guest code address (a function pointer in game data). */
#define GUEST_CALL_ADDR(addr) do { u32 a_ = (addr), lr_ = LR; LR = 0xFFFFFFF1u; dispatch(a_); LR = lr_; } while (0)

/* Return to an explicit guest address (capture LR on entry before using
 * GUEST_CALL, which overwrites it). */
#define RETURN_TO(addr) do { cpu.ret = (u32)(addr) & ~1u; return DONE; } while (0)
