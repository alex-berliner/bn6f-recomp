# Non-LLM cleanup ideas for recomp: debate report

Moderator: C (Opus 5.5, medium). Debaters: A (Opus 5.5, xhigh) and B (Fable 5.1, xhigh). Both worked read-only. The figures below are their measurements from 2026-10-10; a farm run was live while they measured, so some counts were still changing.

## Summary (top recommendations, in priority order)

1. **The "1,500 unhandled farm results" is mostly a ghost.** About 1,370 of them are already installed, many under newer names. Only about 221-224 are really pending. The real backlog is the ~1,418 register-form bodies already in cport. Keep a small tracked ledger of the pending ones (`symbols/pool.tsv`) and re-test only those, without a model.
2. **Fix a few small safety bugs first:**
   - 31 readable call sites read a register that the callee's contract says nobody reads. This is the same bug class as commit 67673ff.
   - `difftest.c` has an unbounded `g_cfg[256]`.
   - On the host, the ROM is mapped writable.
3. **Make builds stop recompiling everything.** Today each install recompiles all 143 files. Fix:
   - keep install-specific prototypes out of `lifted.h`;
   - split the generated files at fixed points;
   - use real dependency files;
   - enlarge ccache to 20 GB.
4. **Make verification cheaper:**
   - cache the original game's lockstep results (about half the lockstep time);
   - build bn6f once per record run, and skip the reverify when the port ROM bytes are unchanged;
   - later, bisect without rebuilding.
5. **Add a commit gate:** lint plus a symdb check (sped up from 12 s to about 1 s), with a baseline that only moves down.
6. **Move the `leaves_farm_*.c` files into one file per bn6f source** before any mass rewrite.
7. **Convert register form to typed C mechanically, without a model:**
   - pure bodies first;
   - write the signatures (`abi:` blocks) that can be proven;
   - finish lifter step 3.7;
   - then rewrite call sites, starting from the bottom of the call tree.
8. **Make the lifter smarter:** golden-output tests first, then emitting bn6f's own symbol names, resolving jump tables per table, and simplification passes.
9. **Make the checks stronger:**
   - compiler warnings and UBSan (a checker for C undefined behaviour) on the host build;
   - random difftest inputs taken from real RAM;
   - a mutation-testing survey.

A and B converged on every area. The only differences left are in emphasis, noted per area below.

---

## Area 1. The farm pool (the "~1,500 unhandled" accepted results)

**Ideas:**
- A1: a ledger.
- A2: automatic triage of the oracle drops.
- B14: an index of the backlog.
- B26: re-test the backlog against the current build.

**Debate:**
- The first count disagreed: B found 900 pending, A found 214. B had not followed renames.
- Both re-checked and agreed:
  - 685-688 are installed under the same name;
  - 686 are installed under a later name (median body similarity 1.0);
  - **221-224 are truly pending.** About 60 of those are already typed. The rest are register form.
- Their drop reasons live only in the Burr logs: 88 oracle, 25 other entries, 13 lockstep set-asides, 10 difftest, 9 compile.

**Agreed:**
- `tools/convert/pool.py` builds a tracked `symbols/pool.tsv`, one row per candidate, with these columns:
  - current name after renames;
  - state: installed, pending or dropped;
  - drop reason, model, run id, lint kind.
- Backfill the drop reasons once from `~/.burr/recomp-convert/*/log.jsonl`. After that, `accept_candidate` and `drop_failing_functions` append rows as they go.
- The builder must be idempotent (safe to run again) and must tolerate files appearing mid-scan, because the farm is live.
- `accepted/` stays a git-ignored cache.
- Re-test only the pending ones (candidate-mode difftest plus lint, in parallel). Sort the oracle drops into the classes in `tools/oracle_triage.md`. Group-install whatever passes.
- When a function has several candidates, apply one rule: prefer typed form, then fewest register references.
- The 13 lockstep set-asides wait for rebuild-free bisection (Area 6).

**Don't:**
- re-test the installed ones;
- re-farm pending ones just to get the new convention;
- commit `accepted/`.

**Effort:** small to medium.

**First steps:**
1. `pool.py build` with the Burr backfill.
2. `pool.py retest`.

## Area 2. Typed signatures: register form to typed C without a model

**Ideas:**
- A3/B1: write `abi:` blocks automatically.
- A4/B5: split pure bodies into a typed function plus a thin adapter.
- A5/B2: rewrite call sites into typed calls.
- A6: remove dead "mirror" writes.
- A7: give models and the difftest the refined contracts.
- A21: a lint for reading leftovers (registers a callee doesn't promise to return).
- A33: infer types from captured calls.
- B18: a callees-first work order.
- B20: generate interface comments.
- B29: rule-based names for trivial accessors.
- B30: retire hand-kept remnants.
- B33: the difftest config overflow.

**Debate:**
- B confirmed A21 is real. The lifter builds each function's contract (which registers it reads and returns) from the original assembly only. A readable caller that reads a register its original never read is therefore invisible to the contract, and narrowing contracts could break it.
- **When to write `abi:` blocks.** A wanted them written only together with a body change, so they can't drift out of date. B wanted them all written up front. They met in the middle:
  - Tier 1, signatures provable from lifter contracts alone (238): write now.
  - Tier 2, signatures that rest on observed edges or captures (up to 664): write only with a body change, or mark `probable`.
  - Every block is regenerable and stores a short hash of the facts it came from. `symdb check` flags blocks whose evidence has changed.
  - Wrapper specs (`convention: registers`) only for the **31** lifted callees that stay as they are (223 call sites). A found that 69 of the top 100 callees are already readable, so their specs come with their typed bodies. Specs for callees that return a flag name that flag result.
- **When A7 happens.** A wanted it early, B late. Agreed: right after A21, about a day in. Farm `compare=` lines stay conservative until A21 has run.
- **A33.** Only exact cases count: a parameter becomes a typed struct pointer only when every captured value falls inside one known RAM instance.

**Agreed order:**
1. B33, the `g_cfg[256]` fix.
2. A21: the lint, fix the 31 sites, add readable callers' reads into `callers_live`.
3. A7.
4. Pilot the A4/B5 codemod on the 132 pure bodies that are statically auto. Use one shared `typify.py`; the lifter will reuse it in Area 4.
5. Lifter step 3.7, with `gen.py` output in the build. B called this the keystone. Pilot it on `setBGTileIdsBlocks1And2_8030628`, and measure adapter cost on hot paths (the m4a mixer) with `tools/speed.sh`.
6. A6: first the 77 functions whose dead writes are provably safe; the rest only after `captures.py` checks them.
7. A5/B2: call-site rewrites in B18's order, only for callees that have an adapter or wrapper.
8. A33 (exact cases), B20, B29 (through the record workflow), B30.

**Don't:**
- hand-write `abi:` for the ~700 functions blocked by `keep`;
- delete code on route evidence alone.

**Effort:** medium overall.

## Area 3. Names, structs and constants

**Ideas:**
- A12/B3: rewrite field accesses using the typed structs.
- B4: name raw numbers.
- A13: a census of unnamed fields.
- A14: named enum constants.
- A22: the lifter keeps bn6f's own symbol names.

**Debate:**
- A found that 34 of the 44 local offset definitions already exist in bn6f's structs. They are missing only because `bn6f_syms.h` exports just the equates lifted code uses.
- B accepted A's restriction on B4: plain value matching gives false hits (for example, `0x02000000` carries several labels).
- **The key safety point** (A, confirmed by B):
  - Today every `LD`/`ST` is volatile.
  - On the host, `LD` reproduces ARM's rotated misaligned loads, and `ST` drops stores to ROM.
  - The struct accessor `G()` (`gen_structs.py:511`) is a plain cast and would lose all three. That could hang a wait loop on hardware, or let a typed store silently write into ROM on the host, which maps ROM writable (`memmap.c:27,43`).
- A first said struct rewrites should wait for A33's types. B rebutted: the offset's prefix already names the struct, so no type is needed.

**Agreed:**
1. A22 in the lifter: 510 wrong aliases and 1,870 bare numbers. Guard it with golden tests.
2. Export all struct offsets, or include `gen/structs.h` in both builds. Delete the local copies, and add the 10 missing ones to bn6f `.inc` files with the ROM kept identical.
3. Struct rewrite, with:
   - `G()` returning a **volatile** pointer;
   - `LD`/`ST` kept for I/O addresses and in any loop with no call in it;
   - only base pointers known to be aligned (`R10`/toolkit, the 362 named instances, pointers derived from them);
   - width mismatches reported, not rewritten.
4. Map ROM read-only on the host, so a typed ROM store faults and is counted as out of domain.
5. A14, table-driven.
6. B4, only where the original's literal pool names the symbol.
7. A13 census, as raw material for naming fields in bn6f.

**Don't:**
- invent field names (`Unk_` stays until there is evidence);
- switch field names to camelCase;
- rewrite before the lint ratchet exists.

**Effort:** A22 about a day; the rewrite script about a day plus a verify.

## Area 4. Making the lifter smarter

**Ideas:**
- A8/B6: decompiler and peephole passes.
- A32: try Ghidra.
- A10/B7: resolve indirect-call liveness per jump table.
- B28: a forced-edge oracle.
- B34: commit the `.edges` files.
- A9: instantiate clone families.
- A29: generate trivial shapes from a table.
- B22: golden-output tests for the lifter.
- B23: vectorise the ROM pointer scan.
- B19: a sparse dispatch table.

**Debate:**
- B23: the scan is 0.7-1.2 s of a ~12 s lift, so do it only when already editing `lift.py`.
- B19 is deferred: the ROM isn't short of space, and a two-level table adds a load to every indirect call.
- On Ghidra, they ended at: no spike now. If goto-to-if/while structuring stalls, spend one day on 20 fixed functions, write up the comparison, and never import its output.

**Agreed order:**
1. B22 golden tests, and B34 (commit `.edges`, stamped with the trace hash).
2. A22 (Area 3).
3. A10/B7 per-table liveness. It is sound, gives +131 auto signatures, and cuts flag blockers from 269 to 25.
4. B28 only for tables kept in RAM.
5. A29 trivial shapes: 83 nullsubs, 128 wrappers, getter/setter runs; delete the 216 hand copies.
6. A9 clone families: 483 lifted siblings.
7. A8 passes one at a time, each gated by the golden diff plus lockstep of the lifted ROM: copy propagation, push/pop, dead flags, loops, and goto structuring last. Simple leaves are emitted as typed C through `typify.py`.

**Effort:**
- B22: about a day.
- A10: 2-3 days.
- Each peephole pass: about a day.
- A29 and A9: about a day each.

## Area 5. Build speed

**Ideas:**
- A15: lifted output that doesn't depend on what is installed.
- B9: dependency files, stable partitions, a bigger ccache.
- B32: a precompiled header.
- B's lifter function index.

**Debate:**
- A measured where the time goes: link about 12 s, lift about 12 s, and the rest is recompiling all 143 files because `lifted.h` changes on every install (`lift.py:892-893`).
- A conceded weak-symbol overrides because of the typo risk (a misspelled override silently keeps the lifted version). They stay a measured follow-up. That follow-up must check:
  - LTO inlining;
  - ROM size (there is no `-ffunction-sections`);
  - the "override defines all entries" check still runs.
- B accepted A's measurement: the ~42 s `gather_context` floor is the fresh worktree's lift plus host build, not file globbing. So the function index is just cleanup (Area 8).

**Agreed:**
1. ccache to 20 GB, with the hit rate logged.
2. Stable headers: plain prototypes only in `lifted.h`; `L_` prototypes in `lifted_originals.h`, included only by the difftest.
3. Split both module files and `difftest_lifted` at **fixed recorded address boundaries** (`tools/lifter/splits.txt`), not every 20k lines.
4. `-MMD` dependency files.
5. Reuse the main tree's host difftest when the lifted output is unchanged.
6. Precompiled headers only if measurement says so.

**Don't:**
- turn LTO off for verify builds;
- use weak aliases without measuring them.

**Effort:** about a day for steps 1-4.

## Area 6. Verify, bisect, record and capture

**Ideas:**
- A16/B12: cache the original's lockstep digests.
- A17/B21: rebuild-free bisection.
- A18/B10 and A28: snapshot store and capture cache.
- A19: pick routes from coverage.
- A27/B25: record workflow speed-ups.
- B13: faster symdb check.
- B24: batch difftest trials.
- B27: run the oracle and lockstep at once.
- A30: timings.

**Debate:**
- B conceded B24: its 252 s figure was old, and the random stage's median is now 7.1 s.
- B conceded B27: running the oracle first is cheap gating, since its median is 10 s and its failures would otherwise waste a 16-core lockstep.
- A found that no evidence in `symbols/db` points at `build-wf-runs`. Instead, 205 reproduce lines cite `/tmp/snaps` and 75 cite `/tmp/oracle`, both temporary.

**Agreed order:**
1. Cache the original's lockstep digests, keyed by route, frames, bn6f sha1, `mgba_run` build and poke file. All 13 routes then fit in one wave on 16 cores, about half the wall time.
2. A19: routes chosen from coverage, for repair and bisect rounds only.
3. Record workflow: one bn6f build per run. Skip the reverify only when the port ROM bytes match the pre-rename build.
4. Rebuild-free bisection, using a selector table patched into the ROM.
5. Snapshot store: block reads, 4 KB content-addressed pages and zstd, with one decoder shared by its readers. Cache key: bn6f sha1, function, route and trace hash.
6. Rewrite `reproduce:` lines to cite that key, so snapshots can be rebuilt. Retention can then go by age, never touching a run whose owner is alive.
7. More oracle states for farm groups up front, and A30 timings.

**Effort:**
- Steps 1-3: about a day.
- Bisection: about 2 days.
- Snapshot store: about 2 days.

## Area 7. Stronger checks

**Ideas:**
- A23: strict warnings and UBSan.
- A24: mutation testing.
- A25: bounded equivalence checking with CBMC.
- A26: structured random inputs from the typed structs.
- B11: random inputs seeded from real RAM.

**Debate:**
- A wanted mutation testing as a gate for codemods; B wanted a one-off survey. They met:
  - Run a calibration survey first: 100 functions × 3 mutants.
  - Then use it to gate *auto-apply without review* on the functions a codemod touches. A estimates about 4 min on 16 cores.
  - Survivors go to review rather than blocking.
  - A surviving "remove a leftover write" mutant may be harmless; check it with `captures.py` before calling the check blind.
- Both rejected A25 for now: the tools aren't installed, and the original game is the judge.

**Agreed order:**
1. A23 now. Strict warnings for `cport/src/game` (the host build uses `-w` today), plus UBSan on readable code. Read each hit before calling it a bug.
2. B11: generalise the existing `difftest_gen.c -m` mutator so random trials start from captured RAM. Today `battle_getFlags` is out of domain in 1,997 of 2,000 trials.
3. A26: pointer inputs aimed at real instances of the right struct.
4. A24: survey, then the narrow gate.

**Effort:**
- A23: half a day.
- B11 and A26: about a day each.

## Area 8. Organisation, tool sprawl, docs

**Ideas:**
- A11/B8: regroup the farm files by bn6f source.
- A20/B16: a tool inventory and attic.
- B17: lint in the build, with a ratchet.
- B15: one shared library for the duplicated parsers.
- B31/A30: prune build output.
- A31: generate the counts in the docs.

**Debate:**
- B accepted A's order: regroup *before* the Area 2 codemods, so their diffs land in the final files.
- A accepted landing B13 together with the commit gate.
- B15 goes incrementally, with no big move while farm runs are live.

**Agreed order:**
1. Cleanup of `build-wf-runs` (57 GB) and the dozen `cport/build*` directories, using Area 6's retention rule.
2. B13 (`yaml.CSafeLoader`), together with a pre-commit gate: `lint.py --check` and `symdb check`. `make check` also runs `tools/wf/tests`. Codemods ratchet `lint_baseline.json` down in the same commit.
3. B15, first only the parts `typify.py` needs:
   - contracts from `graph.json`;
   - the override regex;
   - the function index.
4. Regroup `leaves_farm_*.c` (about 1,325 functions) by bn6f source file. Use a deterministic, re-runnable tool, run at a quiet point. It rewrites `readable_c:` in the db; one verify follows. Later farm groups land in an inbox file that the tool redistributes.
5. Move about 15 unreferenced one-shot scripts to `tools/attic/`, and generate `tools/README.md` from their docstrings.
6. A31: a generated `docs/STATUS.md`. `C_CALLING_CONVENTION.md` still says "17 files, ~81 entry points"; reality is 56 files and 1,435 entry points.

**Don't:**
- reorganise the `tools/wf` graphs;
- commit `accepted/`.

---

## Appendix: stage 1 idea lists

**A: 33 ideas, exhausted after 3 rounds** (20, then 11, then 2 followed by "No more ideas").

A1 farm pool ledger · A2 automatic triage of oracle drops · A3 auto-write `abi:` blocks · A4 mechanically split pure bodies · A5 call-site rewrite bottom-up · A6 remove dead mirror writes · A7 refined contracts for models and the difftest · A8 decompiler passes in the lifter · A9 instantiate clone families · A10 per-table indirect liveness · A11 move farm files into subsystems · A12 typed struct rewrite and moving local offsets to bn6f · A13 census of unnamed fields · A14 named constants · A15 install-independent lifted output · A16 cache original lockstep digests · A17 bisect without rebuilding · A18 content-addressed snapshot store · A19 coverage-picked quick-verify routes · A20 tool inventory and gate · A21 lint for leftover reads and readable callers in `callers_live` · A22 lifter keeps bn6f names · A23 strict warnings and UBSan · A24 mutation testing · A25 bounded equivalence checking · A26 structured random inputs from structs · A27 record: one bn6f build, skip reverify if identical · A28 capture cache keyed by bn6f · A29 generate trivial shapes from a table · A30 `wf` timings and gc · A31 generated doc counts · A32 Ghidra drafts · A33 infer types from captured calls

**B: 34 ideas, exhausted after 4 rounds** (20, then 11, then 3, then "No more ideas").

B1 auto `abi:` blocks and lifter step 3.7 · B2 typed call-site rewrite · B3 struct-typed access · B4 name raw numbers · B5 typify trivial bodies · B6 lifter peepholes · B7 static jump-table resolution · B8 regroup `leaves_farm` by origin · B9 build hygiene (`-MMD`, stable partitions, ccache) · B10 snapshot format, compression and recapture cache · B11 random difftest seeded from real RAM · B12 cache lockstep digests · B13 faster symdb check · B14 index and lint the farm backlog · B15 shared parser library · B16 tool index and archive · B17 lint in the build as a ratchet · B18 callees-first work order · B19 sparse dispatch table · B20 interface comments from the spec · B21 rebuild-free bisection · B22 lifter golden tests · B23 vectorise ROM pointer scan · B24 batch difftest trials · B25 skip reverify after pure renames · B26 re-test the backlog · B27 oracle and lockstep concurrently · B28 forced-edge oracle · B29 rule-based names for trivial accessors · B30 retire hand-kept remnants · B31 prune build directories · B32 precompiled header · B33 `g_cfg[256]` overflow · B34 commit observed call edges

**Overlaps** (the same idea found independently): A1/B14, A2/B26, A3/B1, A4/B5, A5/B2, A8/B6, A10/B7, A11/B8, A12/B3, A16/B12, A17/B21, A18/B10, A20/B16+B17, A27/B25, A30/B31.

**Dropped or deferred by agreement:**
- B19: deferred until flash-cart work resumes.
- B23: only when already editing `lift.py`.
- B24 and B27: dropped.
- A25: not now.
- A32: only if goto structuring stalls.
- Weak-symbol overrides: a measured follow-up.
