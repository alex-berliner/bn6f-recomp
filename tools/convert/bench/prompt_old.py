"""The farm's prompt texts as they were before commit 329fbfd (farm: typed functions with thin adapters,
and the register check), copied verbatim from `git show 329fbfd^:tools/convert/farm.py` for the
benchmark's "old" arm (tools/convert/bench/bench.py) and the farm's `--prompt old` (plan B after the
benchmark: tools/wf/wf/tools/models.py PromptKind reads this one copy). This prompt allows register-form
C (bodies that read and write R0..R12 and GUEST_CALL directly); its candidates are tested without the
register check. The benchmark never installs its candidates; the farm files its own under the model
tagged `+old`, and installing one queues it for retype (tools/abi/retype_queue.tsv). The farm's typed
prompt is tools/convert/farm.py; do not edit this copy (a changed copy is a different prompt: its hash
changes; add it as a new file, arm and PromptKind instead)."""

SYSTEM = (
    "You convert functions of the Game Boy Advance game Mega Man Battle Network 6 from machine-"
    "translated C into clear, idiomatic, readable C for a source port. Your version is compiled and "
    "tested automatically against the original on real game states, so it must behave exactly the same: "
    "same memory writes, same results in the same registers. Write it the way a careful human porter "
    "would: meaningful names, structured control flow, short comments that say what the code does in "
    "game terms.")

CONVENTIONS = r"""
## Conventions of the readable port (cport/include/game.h, cpu.h)
- A function is `Next NAME(void)`. Guest registers are the u32 lvalues R0..R12, SP and LR.
  Arguments arrive in registers (see "inputs"); results are left in the registers listed under
  "outputs". Return with `RETURN_TO_CALLER();` and do not change LR.
- Guest memory (GBA addresses as u32): LD8/LD16/LD32(addr) and ST8/ST16/ST32(addr, value);
  signed loads as (s8)LD8(a), (s16)LD16(a). Types: u8 u16 u32 s8 s16 s32.
- The game's struct field offsets and RAM symbols are macros named as in the disassembly
  (e.g. oBattleObject_Alliance, oToolkit_BattleStatePtr); use them instead of numbers where the
  assembly uses them.
- Calling another function of the game: set R0..R3 (and any other register it reads), then
  `GUEST_CALL(function_name);` and read its results from R0... Every game function is available
  by its disassembly name. BIOS calls (`svc N`): set R0.. and call `swi_call(N);`.
- Condition flags: if "outputs" lists nz, c or v, callers branch on them right after the call.
  Leave them as the original's last flag-setting instruction does: `SET_FLAGS_NZ(x)` (after
  MOV/AND/TST/... with result x), `SET_FLAGS_CMP(a, b)` (after CMP a, b or SUB/SUBS a - b),
  `SET_FLAGS_ADD(a, b)` (after ADDS/CMN). If the last flag-setting instruction differs per
  path, set them on each path.
- Registers listed under "clobbers" are free scratch for you. Every other register must end
  exactly as the original leaves it; registers the original never writes must stay unchanged.
- The original's push/pop of saved registers need not be reproduced (the stack below SP is not
  compared). If it reserves stack space whose address it passes to a callee (e.g. `sub sp, #N;
  add r7, sp, #M`), reserve it on the guest stack too: `u32 buf = SP -= N; ... SP += N;`.
- Write readable C: named local variables, if/else and loops instead of goto where possible,
  no copies of the machine-translated style (no nz/c/v locals, no SUBS()/COND_ macros, no
  L_xxxxxxxx labels, no SAVE/LOAD/CALL_DIRECT). Where the original branches on the flags a
  callee left (`bl f; beq ...`), read them with ZERO_FLAG(), NEGATIVE_FLAG(), CARRY_FLAG() or
  OVERFLOW_FLAG() (game.h), never cpu.nz/cpu.c/cpu.v.

## Example (another function of the game, already converted)
Original assembly of sub_30068E8, which queues one OAM entry:
```
sub_30068E8:  ldr r7, =byte_3001950; ldr r6, [r7]; cmp r6, #0x80; bge locret ...
```
Readable version:
```c
/* sub_30068E8(r0 = attr01, r1 = attr2, r2 = layer, r3 = list position):
 * queue a single OAM entry; r0 = 0xFF if it was queued. */
Next sub_30068E8(void) {
    if ((s32)LD32(SPRITE_COUNT) < POOL_SLOTS && link_entry(R0, R1, R2, (s32)R3))
        R0 = 0xFF;
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

Reply with one ```c code block containing the complete readable `Next {name}(void)` (plus any
static helper functions, enums or #defines it needs). No #include lines, nothing outside the block.
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

