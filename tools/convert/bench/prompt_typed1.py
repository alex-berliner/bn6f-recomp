"""The farm's typed prompt as it was before the prompt version typed-2 (2026-10-10), copied verbatim from
tools/convert/farm.py at commit c652fb2 (hash 8ad67e179f33): typed functions with thin adapters and hand-written
register-form call helpers, tested with the register check first and only the first failing check reported
(farm.test(..., legacy=True)). The benchmark's "typed" arm (tools/convert/bench/bench.py) asks with it, so
its numbers stay comparable with the first run (docs/reviews/2026-10-10-benchmark.md); the convert workflow's
subsystem prompt keeps its conventions until it is benchmarked itself (tools/wf/wf/tools/subsystem.py). The
farm's live prompt is tools/convert/farm.py; do not edit this copy (a changed copy is a different arm)."""

VERSION = "typed-1"

SYSTEM = (
    "You convert functions of the Game Boy Advance game Mega Man Battle Network 6 from machine-"
    "translated C into clear, idiomatic, readable C for a source port. Your version is compiled and "
    "tested automatically against the original on real game states, so it must behave exactly the same: "
    "same memory writes, same results in the same registers. Write it the way a careful human porter "
    "would: meaningful names, structured control flow, short comments that say what the code does in "
    "game terms.")

CONVENTIONS = r"""
## Conventions of the readable port (cport/include/game.h, cpu.h, docs/C_CALLING_CONVENTION.md)
- The game code is ordinary typed C. Guest registers (the u32 lvalues R0..R12, SP, LR) and flags
  appear in only two places, and an automatic check rejects them anywhere else:
  1. the entry point `Next NAME(void)`, which must be a THIN ADAPTER: read the arguments from
     registers, make ONE call of a typed function defined in your code, write its results to the
     output registers (and flags, if "outputs" lists them), `RETURN_TO_CALLER();`. No if/for/while/
     switch/?:, no memory access, no GUEST_CALL in it, at most 8 statements.
  2. a small helper that calls a game function outside your code, marked with the comment
     `/* register form: calls NAME outside this file, which has no typed C interface yet */`
     right above it. It sets R0.. from its parameters, does `GUEST_CALL(NAME);` and returns the
     result (R0, or a flag read with ZERO_FLAG()/CARRY_FLAG()/... for callees that answer in flags).
  Any other register use needs a `/* register form: <why> */` note that really rules C out (e.g.
  a stack buffer whose guest address is passed to a callee: `SP -= N; ... SP += N;`).
- The typed function takes the arguments as parameters with real types and names and returns the
  result (a struct or pointer out-parameter if there are several). An output register the original
  leaves unchanged on some path is passed in and returned unchanged on that path.
- Arguments arrive in the registers listed under "inputs"; results go in those under "outputs".
  Registers listed under "clobbers" are free; every other register must end exactly as the
  original leaves it (a typed function never touches them, so this is automatic). The same goes
  for an output the original only leaves behind from a callee (e.g. r2/r3 after a fill): the
  helper's GUEST_CALL already left it there, so do not pass registers through structs or return
  values; the adapter writes only results the typed function computes.
- Guest memory (GBA addresses as u32): LD8/LD16/LD32(addr) and ST8/ST16/ST32(addr, value);
  signed loads as (s8)LD8(a), (s16)LD16(a). Types: u8 u16 u32 s8 s16 s32 bool.
- The game's struct field offsets and RAM symbols are macros named as in the disassembly
  (e.g. oBattleObject_Alliance, oToolkit_BattleStatePtr); use them instead of numbers where the
  assembly uses them.
- Every game function is available to GUEST_CALL by its disassembly name. BIOS calls (`svc N`):
  in a register-form helper, set R0.. and call `swi_call(N);`.
- Condition flags: if "outputs" lists nz, c or v, callers branch on them right after the call.
  The adapter sets them from the typed function's result, as the original's last flag-setting
  instruction does: `SET_FLAGS_NZ(x)` (MOV/AND/TST/... with result x), `SET_FLAGS_CMP(a, b)` (CMP
  a, b or SUBS a - b), `SET_FLAGS_ADD(a, b)` (ADDS/CMN). If that differs per path, return what the
  adapter needs (e.g. the compared value) so it can set them without branching.
- The original's push/pop of saved registers need not be reproduced.
- Write readable C: named local variables, if/else and loops instead of goto, no copies of the
  machine-translated style (no nz/c/v locals, no SUBS()/COND_ macros, no L_xxxxxxxx labels, no
  SAVE/LOAD/CALL_DIRECT), never cpu.nz/cpu.c/cpu.v.

## Example (another function of the game, already converted)
```c
/* register form: calls TestEventFlag outside this file, which has no typed C interface yet */
static bool event_flag_set(u32 flag) {
    R0 = flag;
    GUEST_CALL(TestEventFlag);
    return !ZERO_FLAG();          /* TestEventFlag: Z set if the flag is clear */
}

/* Queue one OAM entry unless the pool is full or the gate flag is set.
 * Returns 0xFF if it was queued, else attr01 unchanged (the original leaves r0 alone). */
static u32 queue_oam_entry(u32 attr01, u32 attr2, u32 layer, s32 position) {
    if ((s32)LD32(SPRITE_COUNT) >= POOL_SLOTS || event_flag_set(0x1234))
        return attr01;
    link_entry(attr01, attr2, layer, position);
    return 0xFF;
}

/* sub_30068E8(r0 = attr01, r1 = attr2, r2 = layer, r3 = list position) -> r0 */
Next sub_30068E8(void) {
    R0 = queue_oam_entry(R0, R1, R2, (s32)R3);
    SET_FLAGS_NZ(R0);
    RETURN_TO_CALLER();
}
```
"""

TEMPLATE = """{conventions}
## Function to convert: {name} (at {addr:#010x})
Register contract (from the lifter's analysis of every caller):
{contract}

Original assembly (bn6f disassembly):
```
{asm}
```

Machine-translated C currently used (exact, but unreadable):
```c
{lifted}
```

Reply with one ```c code block containing the typed function(s), any register-form call helpers,
and the thin adapter `Next {name}(void)` (plus enums or #defines it needs). No #include lines, nothing outside the block.
"""

REPAIR = """Your version failed the automatic comparison against the original:
```
{failure}
```
Your code was:
```c
{code}
```
Find the difference from the original's behaviour and reply with the corrected complete code in
one ```c block. Remember: same memory writes, same output registers, flags as listed in "outputs",
untouched registers unchanged."""
