# C calling convention and global state for readable code

**Status: phase 1 tools partly built (2026-10-09): 3.1, 3.2, 3.3 (mechanical part),
3.4, 3.5, 3.8; see "Built so far" under §3.** Nothing uses them in the build yet (3.7);
no `abi:` blocks exist. Design agreed 2026-10-08. Decisions taken with the user: the C
function keeps the bn6f name and the register adapter is `abi_NAME`; "global state"
covers registers and typed structs; tools first, then a LangGraph orchestrator.

## Context

Readable functions in `cport/src/game/` (17 files, ~81 entry points) still use the lifted
convention: `Next NAME(void)`, arguments and results in the global `cpu.r[]` (`R0..R12`),
flags as results, `GUEST_CALL` between functions — even between two readable functions.
About 9% of readable code (20–25% in `npc_actions`, `npc_commands`, `ow_objects`,
`event_flags`, `sprite_api`) only exists to reproduce leftover registers and flags.

Example, `setBGTileIdsBlocks1And2_8030628` (`cport/src/game/ow_objects.c:38`): its real
interface is `(tileId1, tileId2, col, row)` with r10 = toolkit, but it sets r0/r2/r6/r7/flags
"for callers". Reading the five callers (`sub_803066E`, `sub_80306C8`, `sub_8030722`, its
sibling, `sub_8030808`, all via `MapTilesState200be70.UnkCallback_18`) shows none reads them.
The lifter cannot see that: indirect callees get every register as an output
(`lift.py:461`, the global `indirect_ret`; 2,920 of 13,691 lifted headers list the full set).

**Goal:** every readable function is ordinary C — typed parameters, a typed return value, no
guest registers or flags, typed struct access to game memory, direct C calls between readable
functions — unless a documented exception says the register convention is genuinely needed.
The register world (lifted code, dispatch tables) reaches readable code only through
generated adapters.

## 1. The target convention

```c
/* readable code: the real name is the C function */
void setBGTileIdsBlocks1And2_8030628(u16 tileId1, u16 tileId2, u32 col, u32 row) {
    u16 *blocks = G(u16, toolkit.iBGTileIdBlocks_Ptr);
    u32 entry = row * 32 + col;
    blocks[1 * 0x400 + entry] = tileId1;
    blocks[2 * 0x400 + entry] = tileId2;
}

/* generated (abi_thunks.c): the register adapter lifted code and dispatch tables call */
Next abi_setBGTileIdsBlocks1And2_8030628(void) {
    setBGTileIdsBlocks1And2_8030628(R0, R1, R3, R4);
    RETURN_TO_CALLER();
}
```

Rules:
- **Naming (decided):** the C function keeps the bn6f name; the generated adapter is
  `abi_NAME`. The lifter points dispatch-table slots and lifted call sites at `abi_NAME`.
- **Arguments:** guest input registers become parameters in register order, whatever
  registers they arrived in (r3/r4 above). A "self" pointer (usually r5) is the first
  parameter, typed (`OverworldNPCObject *npc`).
- **Results:** r0 → return value; r0:r1 pairs → a small struct; a flag result
  (`-> !zf`, `-> c`) → `bool`, which the adapter turns back into flags; an in/out register
  (e.g. the NPC script cursor in r6) → returned new value or a pointer parameter.
- **Globals instead of registers:** r10 is the toolkit (`A_eToolkit` 0x020093B0) once
  proven invariant (§3.6); readable code uses `toolkit.X` and never takes r10.
- **No leftovers by default.** A C function cannot clobber guest registers, so its adapter
  preserves every register it does not return — strictly safer for callers that rely on
  preservation (e.g. `sub_800647C` reading r6 after `sub_8002378`). Leftovers a still-lifted
  caller genuinely reads are listed per function (`keep:`, §2) and produced only in the
  adapter, never in the C body.
- **Calls:** readable → readable is a plain C call. Readable → lifted goes through a
  generated typed wrapper (§3.4), so hand-written code never touches `R0..R12`.
- **Game memory:** typed structs generated from `bn6f/include/structs/*.inc` (§3.5);
  `LD/ST + oX` offsets only where no struct exists yet.

## 2. The signature spec (symbols/db)

A new `abi:` block in each `symbols/db/NAME.yaml` is the single source for the adapter,
the typed wrapper, the difftest contract and the bn6f signature comment:

```yaml
abi:
  c: void setBGTileIdsBlocks1And2_8030628(u16 tileId1, u16 tileId2, u32 col, u32 row)
  args: [r0, r1, r3, r4]
  ret: none                 # r0 | r0:r1 | flag:z | flag:!z | flag:c | none
  globals: [toolkit]        # r10 pinned to A_eToolkit
  keep: []                  # register outputs a still-lifted caller reads, with expression + caller
  convention: c             # c | registers (an exception, §5, with reason:)
  evidence:                 # callers checked and how (tools/abi/report.py output, edges, asm read)
```

`tools/symdb.py check` validates the block. `tools/symdoc.py` writes the C signature into
bn6f as the `// (...) -> ...` comment at the definition (the format already used on ~409
functions, normalised to C types).

## 3. Tooling (orchestrator-agnostic CLIs with JSON output)

| # | Tool | What it does | Builds on |
|---|---|---|---|
| 3.1 | `lift.py --export-abi build-abi/graph.json` | Per entry: inputs, outputs, may_write, preserved, needs_ret/may_unwind, indirect-target flag; per call site: caller, kind (call/tail/indirect), registers and flags live after. | Data already computed in memory: `effects`, `live_out_all`, `callers_live`, `inputs`, `may_write`, `preserved` (`tools/lifter/lift.py:296-760`); `lift.py --why NAME` (was `LIFT_WHY`) is a one-function version |
| 3.2 | `mgba_run --call-edges` | Records observed (indirect call site → target) pairs while replaying routes and sweeps, so indirect callees get per-site liveness instead of the global `indirect_ret`. Output next to `tools/traces/coverage/*.prof`. | `tools/mgba_run.c`, the coverage runs |
| 3.3 | `tools/abi/report.py NAME` | The evidence an agent or person needs to choose a signature: every caller site (direct, tail, observed indirect, plus hand-listed sites for RAM-held pointers such as `UnkCallback_18`), what each reads after the call, which callers are readable vs lifted, unwinding, stack frame use, r10 use, the existing bn6f signature comment and db `interface:`. Proposes `args`/`ret`/`keep`. | 3.1, 3.2, `symbols/db`, bn6f comments |
| 3.4 | `tools/abi/gen.py` | From all `abi:` blocks: `cport/src/game/abi_thunks.c` (adapters, with IWRAM attributes from `tools/lifter/iwram.txt`), `cport/include/abi.h` (C prototypes), `cport/include/lifted_calls.h` (typed wrappers for lifted callees: set args and r10, `GUEST_CALL`, return R0/flag), generated `difftest.cfg` lines (compare = ret registers + flags + keep; `r10=0x020093B0`). | `game.h` macros, `cport/host/difftest.cfg` syntax |
| 3.5 | `tools/abi/gen_structs.py` | `bn6f/include/structs/*.inc` (43 files, `u32 Name // loc=0x18`, `struct_org`) → `cport/include/gen/structs.h`: padded structs, `_Static_assert(offsetof(...))` on every field, guest pointer fields as `u32` (the host test build is 64-bit), `G(T, addr)` to dereference, named globals (`toolkit`, `eMapTilesState200be70`). New fields discovered during conversion go back into the bn6f `.inc` (ROM stays identical), per the symbol rule. | `GUEST_PTR` (`game.h:34`); host flat build maps guest RAM at GBA addresses (`cpu.h:60`) |
| 3.6 | r10 invariance check | Static: every write to r10 (`mov r10, rX`, ~dozens in asm00_0/asm03_2/asm38) is a restore of the toolkit or a local use bracketed by save/restore. Dynamic: r10 == `A_eToolkit` at entry of every function whose inputs include r10, over all routes and sweeps. Result recorded in `symbols/db/eToolkit.yaml`; functions that use r10 otherwise (m4a mixer) are listed. | mgba_run, coverage routes |
| 3.7 | Lifter changes | Functions with an `abi:` block and a C definition count as overridden (today: regex `Next NAME(void) {`, `lift.py:845`); dispatch slots and lifted call sites use `abi_NAME`; `L_NAME` stays the DIFFTEST original; `PAIR(abi_NAME)`. | `lift.py:845-935` |
| 3.8 | `tools/abi/lint.py` | Fails when hand-written code outside generated files uses `R0..R12`, `SP`, `LR`, `SET_FLAGS_*`, `cpu.`, `GUEST_CALL` — except in functions whose spec says `convention: registers`. Run from the build or as a pre-commit check. | — |
| 3.9 | difftest | Compares `abi_NAME` with `L_NAME` on the generated contract (memory, `cpu.ret`, SP, ret registers, flags, keep); r10 pinned. Candidate mode (`-L cand.so`) compiles core + generated adapter, so model candidates are testable without a full rebuild. | `cport/host/difftest.c`, `farm.py` `test()` |

### Built so far (2026-10-09)

- **3.1** `python3 tools/lifter/lift.py --export-abi build-abi/graph.json [--call-edges EDGES...]`
  (`tools/lifter/abi_export.py`): stops before writing C; a lift without the option is
  byte-identical. Masks are name lists (`r0`..`r12`, `lr`, `nz`, `c`, `v`).
- **3.2** `python3 tools/abi/call_edges.py build-abi/graph.json [ROUTE...]` runs
  `mgba_run --call-edges SITES OUT` (steps like a coverage run, ~4 min for all routes in
  parallel) and writes `tools/traces/coverage/ROUTE.edges` ("SITE TARGET LR COUNT").
  Merged by the export (`*_observed` fields): an indirect target whose entries were all
  observed gets the liveness at its edges' return points (for a jump, the LR when it is
  the return point of a call to the jumping unit, e.g. a `bx r3` veneer); an observed
  site reads and writes what its targets do. Evidence from the routes, not proof.
- **3.3, mechanical part** `python3 tools/abi/report.py build-abi/graph.json NAME... | --summary
  [--captures build-abi/captures.json]`: contract, proposed `args`/`ret`/`globals`/`keep`,
  blockers (`unwinds`, `flags_in`, `flags_out`, `keep`, `callers_unknown`, `ret_and_flag`)
  and every known call site with what it reads after. The export re-runs the lifter's
  analysis with refinements switched on one at a time (`tools/lifter/abi_export.py`:
  edges; BIOS calls with the registers `swi.c` shows they use; per-call-site liveness, a
  call reading the callee's own inputs plus what passes through it; saves are not uses and
  restored registers pass through calls; flag results, where every known caller tests one
  condition and the function sets it on every path, become `ret: flag:z` etc.).
  `python3 tools/abi/captures.py build-abi/graph.json [--blockers keep,flags_out]` checks
  claimed leftovers against the capture workflow's snapshots of the original: unchanged in
  every captured call means the adapter (which preserves them) is exact. Not yet: bn6f
  comments and db `interface:`, stack arguments, hand-listed RAM pointer sites.
- **3.4** `python3 tools/abi/gen.py [--out DIR] [SPEC...]` (default `symbols/db`, output
  `cport/build-abi/gen`); `gen.py --test` compiles the fixtures in `tools/abi/tests/`
  (the pilot, `TestEventFlag`/`TestEventFlagFromImmediate` as `convention: registers`
  callees returning `bool` from Z) for GBA and host. `convention: registers` specs give a
  typed `call_NAME` wrapper (`NAME` is still `Next NAME(void)` in `lifted.h` until 3.7).
  Adapters save and restore r0-r12 and the flags around the C call, because a C body may
  call register-form code through the wrappers; in/out registers are written `"&r6"`.
  The `PAIR(n)` macro cannot yet pair `abi_NAME` with `L_NAME` (3.7/3.9).
- **3.5** `python3 tools/abi/gen_structs.py` → `cport/include/gen/structs.h` (git-ignored,
  not built yet): 50 structs and 362 RAM instances (`toolkit` = `eToolkit`), every offset
  asserted against the ELF's `oStruct_Field` symbols; `G(T, addr)`. `S20047CC` is left out
  (never assembled).
- **3.8** `python3 tools/abi/lint.py [--check]`: register-level code per function (typed,
  `Next` body, thin adapter, `register form:` exception) as JSON; `--check` fails only on
  hits beyond `tools/abi/lint_baseline.json` (1,456 functions today). The convert
  workflow runs it on each candidate file (`check_registers`); not in `make`.
- **Refined contracts in the lifted headers** (review item R1-A7, 2026-10-10): every lift runs the
  export's final model (`abi_export.REFINED`: edges from the committed
  `tools/traces/coverage/*.edges`, BIOS calls, per-site liveness, saves pass through) and writes
  its inputs and its callers' reads into each header; may-write and preserved stay the lifter's.
  9 lifted units get a wider contract than the lifter's own: the routes show callers it does not see
  (SoundMain and the rfu IWRAM copies, reached through pointers); the lift lists them.
  The code is unchanged: it follows the conservative analysis. So models are shown, the difftest
  compares (`difftest_pairs.h`: every r0-r12 not free plus the flags callers read, unless
  `difftest.cfg` has a `compare=` line), stamps record and the leftover lint checks the same
  contract. Against the lifter's own, for the 1,434 readable units: outputs narrower in 888,
  compared registers fewer in 777, inputs narrower in 820, none wider.
  `difftest_pairs.h` also says whether the original always returns to its caller (the lifter proves
  it never unwinds); for those, an original that comes back elsewhere on a random input has had its
  own frame overwritten by the input, and the trial counts as out of domain.
- **Leftover reads** `python3 tools/abi/leftovers.py [--check]` (review item R1-A21): readable
  code that reads, after `GUEST_CALL(F)`, a register or flag F does not return (its lifted
  contract), or one the original of that caller never reads after F (per-site liveness in
  `graph.json`), or passes such a leftover into the next callee. It would have caught all four
  TestEventFlag bugs of 67673ff. `--check` fails unless a site carries a note
  (`register form: relies on F leaving rN (contract says clobbered)` or
  `leftover unused: F's rN (why)`); "zflag" sites (the original branches on F's Z, the C tests
  r0, which F also returns) are listed for a look at F. 0 open sites since 2026-10-10.

What the export says today (1,435 readable entries in 1,434 units; 13 routes, 1,314
distinct edges from 340 of 1,862 computed call/jump sites). Readable units whose callers
may read all of r0-r12: 124 with the lifter's contracts, 7 with the edges. Auto-acceptable
signatures and blockers after each step (each includes the ones above it):

| step | auto | keep | flags_out | unwinds | callers_unknown | ret_and_flag | flags_in |
|---|---|---|---|---|---|---|---|
| lifter contracts | 238 | 1027 | 863 | 55 | 477 | | 74 |
| + observed call edges | 329 | 984 | 573 | 55 | 55 | | 6 |
| + BIOS calls' real registers | 332 | 981 | 573 | 55 | 55 | | 6 |
| + per-call-site liveness | 463 | 871 | 366 | 55 | 55 | | 6 |
| + saves pass through | 545 | 786 | 366 | 55 | 55 | | 6 |
| + flag results | 569 (40%) | 786 | 289 | 55 | 55 | 9 | 6 |
| + captured calls (keep) | 644 | 711 | 289 | 55 | 55 | 9 | 6 |
| + captured calls (keep, flags) | 664 (46%) | 704 | 269 | 55 | 55 | 9 | 6 |
| ceiling: nothing unseen is touched | 795 | 597 | 25 | 55 | | 21 | 5 |

Flag results: of 366 flag blocks, 77 clear: 68 become `ret: flag:z` (callers test only
eq/ne), 9 need r0 and Z together (`ret_and_flag`). In the rest the flags flow on to
callers the routes never showed (278 functions), into a callee that reads flags (57),
reach an unobserved indirect call (34), or use them as data (12); lt/ge/gt occur only
mixed with others. Leftovers: 261 functions lose registers that a later callee only
saved (r3, r6, r4, r2 most). Of 512 blocked by `keep` alone, 75 leave the kept registers
unchanged in every captured call (median 40 calls), 401 change them in some call (a
caller may really read the leftover: keep it in the adapter or convert the caller), 36
have no captures. The ceiling shows what more route coverage could still buy (+131).

The 7 still read in full: `MPlayMain`,
`custMenuPressOK_8028D3A`, `keyItems_RemoveFromInventoryAndGive_803CDF8` (several callers
each read some), `battle_objectUpdateDispatchPhase_80165C2`, `sub_803F798` (a caller
reads everything), `chatbox_processScriptEntry_8042678` (an observed return point reads
everything), `SoundMainRAM_iwram_03005700` (callers unknown).

## 4. Per-function workflow (callees before callers)

1. **Select** a call tree from the routes' coverage (callees first; topological order from
   graph.json).
2. **Analyse:** `tools/abi/report.py NAME`.
3. **Decide the signature** and write the `abi:` block with its evidence. Auto-acceptable
   when the report proves every dropped output dead at every known call site and the
   function does not unwind; otherwise a person (or Claude) signs off.
4. **Rewrite** the body as C against the spec; convert readable callers to direct calls and
   lifted callees to `lifted_calls.h` wrappers; remove mirroring code.
5. **Generate** (`gen.py`, `gen_structs.py`), build, `lint.py`.
6. **Verify** in the existing order (`docs/PROCESS.md` §3): host difftest random and
   captured calls, CPU oracle, lockstep on every route, battle sweep for battle code,
   `tools/speed.sh` when adapters sit on hot paths. Record when a check compared nothing.
7. **Record:** symbols/db (abi block, evidence, repro), `symdoc.py` comment in bn6f,
   renames if the meaning is now confirmed. Commit locally.

## 5. When the register convention stays (exceptions)

Allowed only with `convention: registers` and a stated reason:
- **Non-local control flow:** returns somewhere other than its caller (`may_unwind`),
  pops or shares a caller's frame (e.g. `menu_moveCursorUpDown_811FA22` / `menu_moveCursor_811FA36`
  in `asm32_811CAEC.c`).
- **Temporary:** a still-lifted caller reads a computed leftover that cannot sensibly be
  produced in the adapter — convert that caller next instead of keeping it.
- **Measured hot paths** where the adapter costs speed (`tools/speed.sh`), e.g. the m4a
  mixer in IWRAM; decide by measurement, not up front.
- **Interrupt entry and boot** code.
- Functions that pass pointers to their own guest-stack buffers into lifted callees need a
  guest-stack allocation helper rather than an exception; note them in the spec.

## 6. Rollout

- **Phase 1 — tools** 3.1–3.9, piloted on `setBGTileIdsBlocks1And2_8030628`
  (expected: 4-line body, generated adapter, difftest compares memory only, all routes
  identical in lockstep).
- **Phase 2 — retrofit the existing readable code by hand**, leaves first: `event_flags`
  (14 wrappers → `bool`/`void` C functions), `ow_objects`, `palette_effects`,
  `battle_helpers`; then the fully readable trees (`battle_isTimeStop` → `battle_getFlags`,
  `NPCCommand_jump` → `ReadNPCScriptWord`, `npc_drawSprites_800467C` → sprite pipeline);
  then `npc_commands`, `npc_actions`, `npc_script`, the farm's functions (by bn6f source since the
  regroup, `tools/layout.py`); m4a by measurement.
  Update `docs/PROCESS.md` (replace the "register mirroring rule") and the `farm.py`
  CONVENTIONS/TEMPLATE so new model output is already C-ABI.
- **Phase 3 — LangGraph orchestrator (decided: after the tools exist).** Its own venv under
  `tools/abi/`, pinned `langgraph` + SQLite checkpointer. Graph: select → analyse (tool) →
  propose signature (cheap model) → sign-off gate (auto or interrupt) → rewrite (cheap
  model) → generate/build/lint → verify → repair loop → escalate to a stronger model →
  queue for Claude → integrate (human gate). Model calls go through `tools/llm.py` (pacers,
  `usage.jsonl`, `scorecard.py`). Difftest-level checks run in parallel via candidate mode;
  full rebuild, oracle and lockstep run serialised under a build lock (or per-run worktree).

## 7. Open questions

- Should C-only functions drop the address suffix when renamed, or keep bn6f names exactly?
- Generated files committed or built (lifted C is not committed today)? For now built and
  git-ignored (`cport/include/gen/`, `cport/build-abi/`); only generators and fixtures are committed.
- Typed struct field naming: keep bn6f CamelCase (`UnkCallback_18`) for traceability, or
  camelCase in C?
