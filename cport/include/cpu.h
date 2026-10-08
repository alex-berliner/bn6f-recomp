/*
 * cpu.h — guest machine model for lifted BN6 code.
 *
 * Lifted functions operate on a global ARM7TDMI register file (`cpu`). On the
 * GBA target, guest memory accesses are plain volatile loads/stores, so the
 * hardware itself provides unaligned-access rotation, I/O side effects and
 * wait states exactly as the original code saw them.
 */
#pragma once
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* Banked register sets (indices into Cpu.bank_*). */
enum { BANK_USR = 0, BANK_FIQ, BANK_IRQ, BANK_SVC, BANK_ABT, BANK_UND, BANK_COUNT };

typedef struct Cpu {
    u32 r[16];
    /* Flags. N and Z always come from the same result on ARMv4T, so they
     * share one word: N = (s32)nz < 0, Z = nz == 0. C and V are 0/1. */
    u32 nz, c, v;
    /* Even guest address the last guest return went to. Call sites compare
     * it with their continuation; a mismatch unwinds further. */
    u32 ret;
    u32 cpsr_ctl;              /* CPSR bits 0-7: mode, T, F, I */
    u32 spsr;                  /* SPSR of the current mode */
    u32 bank_sp[BANK_COUNT];
    u32 bank_lr[BANK_COUNT];
    u32 bank_spsr[BANK_COUNT];
} Cpu;

extern Cpu cpu;

#define R0  (cpu.r[0])
#define R1  (cpu.r[1])
#define R2  (cpu.r[2])
#define R3  (cpu.r[3])
#define R4  (cpu.r[4])
#define R5  (cpu.r[5])
#define R6  (cpu.r[6])
#define R7  (cpu.r[7])
#define R8  (cpu.r[8])
#define R9  (cpu.r[9])
#define R10 (cpu.r[10])
#define R11 (cpu.r[11])
#define R12 (cpu.r[12])
/* The guest stack pointer is volatile so an interrupt always sees its
 * current value: the IRQ handler pushes below it. */
#define SP  (*(volatile u32 *)&cpu.r[13])
#define LR  (cpu.r[14])

/* ── guest memory ─────────────────────────────────────────────────────── */
#if defined(TARGET_GBA) || defined(TARGET_FLAT)
#define LD32(a)  (*(volatile u32 *)(uintptr_t)(u32)(a))
#define LD16(a)  ((u32)*(volatile u16 *)(uintptr_t)(u32)(a))
#define LD8(a)   ((u32)*(volatile u8 *)(uintptr_t)(u32)(a))
#define LDS16(a) ((u32)(s32)*(volatile s16 *)(uintptr_t)(u32)(a))
#define LDS8(a)  ((u32)(s32)*(volatile s8 *)(uintptr_t)(u32)(a))
#define ST32(a, v) (*(volatile u32 *)(uintptr_t)(u32)(a) = (u32)(v))
#define ST16(a, v) (*(volatile u16 *)(uintptr_t)(u32)(a) = (u16)(v))
#define ST8(a, v)  (*(volatile u8 *)(uintptr_t)(u32)(a) = (u8)(v))
#else
u32  mem_ld32(u32 a);
u32  mem_ld16(u32 a);
u32  mem_ld8(u32 a);
u32  mem_lds16(u32 a);
u32  mem_lds8(u32 a);
void mem_st32(u32 a, u32 v);
void mem_st16(u32 a, u32 v);
void mem_st8(u32 a, u32 v);
#define LD32(a)  mem_ld32((u32)(a))
#define LD16(a)  mem_ld16((u32)(a))
#define LD8(a)   mem_ld8((u32)(a))
#define LDS16(a) mem_lds16((u32)(a))
#define LDS8(a)  mem_lds8((u32)(a))
#define ST32(a, v) mem_st32((u32)(a), (u32)(v))
#define ST16(a, v) mem_st16((u32)(a), (u32)(v))
#define ST8(a, v)  mem_st8((u32)(a), (u32)(v))
#endif

/* ── condition codes ──────────────────────────────────────────────────── */
/* Lifted functions keep the guest registers and flags in C locals
 * (r0..r12, lr, nz, c, v) and sync them with `cpu` around calls; these
 * macros refer to those locals. */
#define FLAG_N  ((s32)nz < 0)
#define FLAG_Z  (nz == 0)
#define COND_EQ (nz == 0)
#define COND_NE (nz != 0)
#define COND_CS (c != 0)
#define COND_CC (c == 0)
#define COND_MI ((s32)nz < 0)
#define COND_PL ((s32)nz >= 0)
#define COND_VS (v != 0)
#define COND_VC (v == 0)
#define COND_HI (c != 0 && nz != 0)
#define COND_LS (c == 0 || nz == 0)
#define COND_GE (FLAG_N == (v != 0))
#define COND_LT (FLAG_N != (v != 0))
#define COND_GT (nz != 0 && FLAG_N == (v != 0))
#define COND_LE (nz == 0 || FLAG_N != (v != 0))

/* ── ALU helpers: set the local c/v flags, yield the result ───────────── */
#define ADDS(a, b) ({ u32 a_ = (a), b_ = (b), r_ = a_ + b_; \
    c = r_ < a_; v = (~(a_ ^ b_) & (a_ ^ r_)) >> 31; r_; })
#define SUBS(a, b) ({ u32 a_ = (a), b_ = (b), r_ = a_ - b_; \
    c = a_ >= b_; v = ((a_ ^ b_) & (a_ ^ r_)) >> 31; r_; })
#define ADCS(a, b) ({ u32 a_ = (a), b_ = (b); u64 w_ = (u64)a_ + b_ + c; u32 r_ = (u32)w_; \
    c = (u32)(w_ >> 32); v = (~(a_ ^ b_) & (a_ ^ r_)) >> 31; r_; })
#define SBCS(a, b) ({ u32 a_ = (a), b_ = (b), br_ = c ^ 1, r_ = a_ - b_ - br_; \
    c = (u64)a_ >= (u64)b_ + br_; v = ((a_ ^ b_) & (a_ ^ r_)) >> 31; r_; })

static inline u32 ROR(u32 x, u32 n) { n &= 31; return n ? (x >> n) | (x << (32 - n)) : x; }

/* Register-specified shifts (amount = Rs & 0xFF). */
static inline u32 LSL_REG(u32 x, u32 s) { return s < 32 ? x << s : 0; }
static inline u32 LSR_REG(u32 x, u32 s) { return s < 32 ? x >> s : 0; }
static inline u32 ASR_REG(u32 x, u32 s) { return (u32)((s32)x >> (s < 32 ? s : 31)); }
static inline u32 ROR_REG(u32 x, u32 s) { return ROR(x, s); }
/* ...and their flag-setting forms (update *cf; amount 0 keeps C). */
static inline u32 lsl_reg_c(u32 x, u32 s, u32 *cf) {
    if (s == 0) return x;
    if (s < 32) { *cf = (x >> (32 - s)) & 1; return x << s; }
    *cf = s == 32 ? (x & 1) : 0;
    return 0;
}
static inline u32 lsr_reg_c(u32 x, u32 s, u32 *cf) {
    if (s == 0) return x;
    if (s < 32) { *cf = (x >> (s - 1)) & 1; return x >> s; }
    *cf = s == 32 ? (x >> 31) : 0;
    return 0;
}
static inline u32 asr_reg_c(u32 x, u32 s, u32 *cf) {
    if (s == 0) return x;
    if (s < 32) { *cf = ((s32)x >> (s - 1)) & 1; return (u32)((s32)x >> s); }
    *cf = x >> 31;
    return (u32)((s32)x >> 31);
}
static inline u32 ror_reg_c(u32 x, u32 s, u32 *cf) {
    if (s == 0) return x;
    s &= 31;
    if (s == 0) { *cf = x >> 31; return x; }
    x = ROR(x, s);
    *cf = x >> 31;
    return x;
}
#define LSL_REG_C(x, s) lsl_reg_c((x), (s), &c)
#define LSR_REG_C(x, s) lsr_reg_c((x), (s), &c)
#define ASR_REG_C(x, s) asr_reg_c((x), (s), &c)
#define ROR_REG_C(x, s) ror_reg_c((x), (s), &c)

/* ── control flow ─────────────────────────────────────────────────────── */
/* Lifted functions return the next function to run instead of calling it, so
 * guest tail branches between functions use constant host stack (Thumb-1 GCC
 * cannot emit far sibling calls). A null `fn` ends the chain. */
typedef struct Next Next;
typedef Next (*GuestFn)(void);
struct Next { GuestFn fn; };
#define NEXT(f) ((Next){ (f) })
#define DONE    ((Next){ 0 })

/* The trampoline: runs `f` and every function it tail-calls. Lives in
 * IWRAM (ARM) on the GBA since every guest call goes through it. */
void run(GuestFn f);

Next jump_to(u32 addr);             /* tail-jump; unknown targets unwind as a return */
void dispatch(u32 addr);            /* call the lifted code for a guest address */
void swi_call(u32 n);               /* BIOS services */
void undefined_insn(u32 addr);
void exception_return(u32 addr);
u32  cpu_get_cpsr(void);
void cpu_set_cpsr(u32 v, u32 mask);
u32  cpu_get_spsr(void);
void cpu_set_spsr(u32 v, u32 mask);

/* Direct call of a lifted function: LR holds the guest return address, the
 * callee reports where it returned to, and anything but our continuation
 * means an outer frame owns the return, so keep unwinding. */
/* SAVE/LOAD are defined per lifted function: they copy the registers that
 * function uses between its locals and `cpu`. */
#if defined(LIFT_TRACE)
void trace_ret_mismatch(u32 expected);
#endif

/* LIFT_TRACE: verify that registers a call site keeps in host registers
 * (per the lifter's preserved/may-write analysis) are untouched by the callee. */
#if defined(LIFT_TRACE)
void trace_sync_violation(u32 ra, int which);
#define SYNC_SNAP(...) u32 snap_[] = { __VA_ARGS__ };
#define SYNC_CHECK(ra, ...) do { u32 now_[] = { __VA_ARGS__ }; \
    for (unsigned k_ = 0; k_ < sizeof now_ / sizeof now_[0]; k_++) \
        if (now_[k_] != snap_[k_]) trace_sync_violation((ra), (int)k_); } while (0)
#else
#define SYNC_SNAP(...)
#define SYNC_CHECK(ra, ...)
#endif

/* Per-call-site unwind (the lifter publishes the right registers first). */
#if defined(LIFT_TRACE)
#define RET_MISMATCH(ra) do { trace_ret_mismatch((u32)(ra)); return DONE; } while (0)
#else
#define RET_MISMATCH(ra) return DONE
#endif

/* A callee that returned somewhere else unwinds through us: publish all
 * our registers (SAVE) for the frame that owns the continuation. */
#if defined(LIFT_TRACE)
#define RET_CHECK(ra) do { if (cpu.ret != (u32)(ra)) { trace_ret_mismatch((u32)(ra)); SAVE; return DONE; } } while (0)
#else
#define RET_CHECK(ra) do { if (cpu.ret != (u32)(ra)) { SAVE; return DONE; } } while (0)
#endif
/* Direct calls run the callee in place and only enter the trampoline when
 * it asks for a tail call. */
#define CALL_DIRECT(f_) do { Next n_ = f_(); if (n_.fn) run(n_.fn); } while (0)
#define CALL(fn, ra)    do { SAVE; LR = (u32)(ra) | 1u; CALL_DIRECT(fn); LOAD; RET_CHECK(ra); } while (0)
#define CALL_A(fn, ra)  do { SAVE; LR = (u32)(ra); CALL_DIRECT(fn); LOAD; RET_CHECK(ra); } while (0)
#define CALLX(x, ra)    do { u32 x_ = (x); SAVE; dispatch(x_); LOAD; RET_CHECK(ra); } while (0)
#define RETURN(x)       do { u32 x_ = (x); SAVE; cpu.ret = x_ & ~1u; return DONE; } while (0)
#define TAIL(fn)        do { SAVE; return NEXT(fn); } while (0)
#define JUMP(x)         do { u32 x_ = (x); SAVE; return jump_to(x_); } while (0)
#define SWI(n)          do { SAVE; swi_call(n); LOAD; } while (0)
#define UNDEFINED(a)    do { SAVE; undefined_insn(a); return DONE; } while (0)
#define EXCEPTION_RETURN(x) do { u32 x_ = (x); SAVE; exception_return(x_); return DONE; } while (0)

/* Data that lives inside the IWRAM routine block is read from its ROM copy
 * (bn6f IWRAMRoutinesROMLocation). */
#define IWRAM_ROM(a) ((u32)(a) - 0x03005B00u + 0x081D6000u)
