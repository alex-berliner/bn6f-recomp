# The knowledge layer and the first whole-subsystem pilot (2026-10-12)

Plan: docs/reviews/2026-10-12-knowledge-first.md. Work in build-wf-worktrees/q-subsys (branch q-subsys).

## Recommendation

**Move the farm to subsystems.** The pilot converted 32 functions in one go, with one repair round.
All of them passed every check. Per function it cost about a fifth of today's typed farm in outside tokens.
It left 4 adapters and no register-level code outside the generated wrappers.

Start with the subsystems that have few entry points, using the same steps (below).
Keep the leaf farm only for functions that belong to no subsystem.

## Part 1: the knowledge layer

Tools are in `tools/sig/`. Every step can be run again:

1. `bounds.py`: runs the lifter's analysis without writing anything. For every entry it records two
   proposals:
   - the refined contract (`final`), which is an upper bound;
   - the `ceiling` model.

   graph.json proposes signatures only for readable functions; this covers all of them.
2. `derive.py`: the mechanical pass.
   - **Liveness over each function's own assembly** (`asm.py`), with a call reading what its callee
     needs. This is solved to a fixpoint over the call graph, and it is bounded by the contract.
     - Saves through `push`/`pop` and `mov rL, r8`+`push` do not count as reads.
     - The pass marks a result `tainted` when it relied on a callee it could not narrow (a computed
       jump, or an unseen pointer call).
   - **Captured calls** (`build-snapshots`, 4,436 functions): each argument register is classified as
     one of:
     - a small or signed int;
     - a pointer into a region;
     - a bn6f struct instance (the 362 RAM instances from `gen_structs.py`);
     - a ROM label;
     - code.
   - **Typed readable adapters** supply parameter types and names.
   - **bn6f's signature comments.**
   - **Callers' own unchanged arguments**, propagated to callees that have no captures.
3. `ask.py run` / `merge`: sends the unsure functions to MiniMax-M3, 8 per request, with all the
   evidence and the assembly. Later rounds show the callees' signatures that are already agreed.
   - A model's answer is `confident` only when:
     - its registers include everything liveness found and lie inside the contract;
     - its result lies between the bounds;
     - its types match the captured calls (pointer vs int, struct name).
   - Otherwise it is `unsure`, and both proposals are kept.
4. `write_db.py`: writes a `signature:` block into every db function entry (2,653 entries). The block
   holds:
   - the prototype;
   - `{reg, type, name}` for each parameter;
   - the result;
   - `globals`, `keep`, and `flags` (when callers test the Z flag it leaves);
   - confidence, `decided_by`, and one evidence line per source.

   Every function, with or without an entry, is also in `symbols/signatures.tsv`. The format is
   documented in symbols/db/README.md.

**Result (14,024 entries: 13,691 lifted units plus their secondary entries):**

| | functions |
|---|---|
| confident | 12,539 (89%): 3,778 by the mechanical pass alone, 8,761 where a model and the mechanical pass agree; 2,191 of them are `void f(void)` |
| unsure | 1,295: the model and the evidence disagree, or there was no answer |
| none | 190: they return somewhere other than their caller (the lifter), so they have no C signature |

**Cost:**
- MiniMax-M3: 1,587 requests, 6.3M input and 1.5M output tokens, for 12,226 answers. That is about
  640 tokens per function, and about 4 hours of wall time at 4 requests in parallel.
- Hyper had no credits today. zai/glm-5.3's weekly quota runs out until 10-14.

**Weak spots:**
- **Wide signatures.** About 1,800 functions sit behind a computed jump or an unseen pointer call.
  There, the model may keep every register of the contract, and the result is still "confident"
  because it is safe. `sub_803F838` is an example, at r7–r12.
- **Results that are both r0 and a flag.** `TestEventFlag` and `IsScreenFadeActive` return a value
  in r0 and also leave a flag that callers test. The block records this as `flags: z`, and the
  generated wrapper returns `.z`.
- **Parameter names.** Names such as `r0` and `r1` remain where no source gave a better one; for
  example the two bytes of an event flag.

**Subsystems** are in `tools/sig/subsystems.json` (`subsystems.py`):
- The grouping uses call edges plus the edges the routes observed, inside one bn6f source file.
- The closest addresses are joined first, with a cap of 60 per group.
- Hubs (12 or more callers) attach to the group that holds 70% of their callers.
- A unit with no edge inside its file joins its neighbour in address order.
- Entry points are the members called from outside the group, plus those with no known caller.

| size | subsystems | units |
|---|---|---|
| 1 | 710 | 710 |
| 2–9 | 1,146 | 5,024 |
| 10–19 | 205 | 2,747 |
| 20–60 | 143 | 5,210 |

Of the 20–60 groups, 23 are 80% or more covered by routes and captured. The ones with the fewest entry
points:

| subsystem | units | entry points |
|---|---|---|
| chatbox_803FEB4 | 60 | 1 |
| npc/N_809E570 | 60 | 2 |
| the pilot | 33 | 4 |
| Cutscene_80376F4 | 60 | 8 |
| asm36/misc_81356D4 | 60 | 8 |

## Part 2: the pilot, the title screen (`asm03_0/start_802F544`)

**What it covers.** 33 units are in the group. One is converted: `dead_802FA44` is dead code and is left
lifted.

There are 4 entry points:
- `startscreen_render_802F544`, reached from the main loop's table;
- `sub_802FDB0` and its second entry `loc_802FDB4`;
- `copyObjAttributesToIWRAM_802FE0C`.

The three nested state tables became C arrays of functions.

**The result** is `cport/src/game/start_screen.c`, 886 lines:
- a typed `StartScreen` struct (copied from `gen_structs.py`, with its offset asserts);
- 32 static typed functions that call each other directly;
- 48 generated wrappers for calls out of the subsystem:
  - 47 typed, from confident callee signatures;
  - 1 register form, for an unsure callee;
  - registers the call sites never set pass through untouched;
  - Z-flag results come back as `.z`;
- 4 thin adapters.

**How:**
1. `subsys_prompt.py` builds the prompt from:
   - the signatures;
   - the assembly;
   - today's readable C;
   - the struct;
   - the wrappers.
2. `convert.py` sends it.
3. opencode/deepseek-v4-pro wrote the file. It needed a second prompt, because the first one let the
   model reuse the game's function names, which lifted.h already declares, and left out `offsetof` and `G`.
4. One repair round followed. kimi-k3 returned an empty answer after 16k reasoning tokens, and
   glm-5.3 was out of quota.

**Verification** (in the worktree; the main tree was in use):

| check | result |
|---|---|
| difftest, captured calls | 40/40 calls of each entry |
| difftest, random | 2,000 inputs per entry: 0 failures, except 17 for `startscreen_render_802F544` (see below) |
| difftest, state sweep (`tools/sig/state_sweep.py`) | the 40 captured calls with every valid state byte, timer and cursor value: 12,000 states, 7,625 in domain, 0 failures, so every state handler ran from real memory |
| CPU oracle | no contract register differs. Every state differs only in r14, and the lifted original shows the same r14 difference in this run. That is an oracle artifact; another session is triaging the oracle today |
| lockstep | 16/16 routes identical, and beastsp (220,000 frames) identical |
| gate | `gate.py`: green |
| ROM | the bn6f ROM is unchanged (no bn6f edits) |

The 17 random failures all happen in trials where the state bytes hold values the game never uses.
There the original jumps past its ROM table into whatever lies after it. The old register-form
version mirrored that jump; typed C does not.

**Measured against today's path:**

| | today's path (typed farm, glm-5.3-flash, scorecard) | pilot |
|---|---|---|
| outside tokens per function | 29k per passing function; 18k for ≤10 lines, 149–188k for 31–60 lines; 70% pass | about 6.5k per function, counting every attempt (deepseek 3 calls: 78k in, 131k out; kimi's empty 46k). About 4.5k counting only the good prompt and the repair. Plus about 0.6k per function for the knowledge layer, which is shared |
| Claude tokens | (not measured per function) | about 360k for the whole session: building the knowledge layer and the tools, the pilot, and this write-up. About 150k of that went to the pilot, so about 5k per function; the next subsystems reuse the tools |
| `Next` adapters | 26 Next bodies (15 register-form, 11 thin) for the 26 readable members; 6 still lifted | 4 adapters, at the entry points only |
| register uses (R0–R12) | 171 | 72, all in the generated wrappers and adapters |
| guest calls | 73 | 48 (one per wrapper) |
| lint violations | 15 functions, 100 hits | 0 |
| memory accesses | 52 LD/ST, 11 struct-field accesses | 17 LD/ST, 59 struct-field accesses |
| lint baseline | 20,322 hits | 20,217 hits |

**Bugs the checks found, all fixed:**
1. The model passed `EVENT_1704` whole as one byte, where bn6f's `movflag` splits it into r0 and r1.
   The compiler warned (the state sweep would have failed). The prompt now explains `movflag`.
2. `sub_802FDB0` did not leave r3 (the end of the table walked), which its callers read.
   Caught by the difftest and the oracle.
3. Flags were read after wrapper calls (lint). The wrapper generator now returns `.z`.
4. Prompt and tool gaps:
   - names that clash with lifted.h;
   - missing `offsetof`/`G`;
   - the second entry `loc_802FDB4`, which the lifter refused.

   All are fixed in `subsys_prompt.py`.

**What was hard:**
- **Random inputs** almost never reach a nested state machine with valid state bytes (99% out of
  domain). The state sweep fixed that, and it should be a standard step for every subsystem with
  dispatch tables.
- **Leftover registers** that callers read (`keep`) are easy to miss. The prompt now says how an
  adapter reproduces them.
- **Pass-through registers.** A callee that reads its caller's r7 or r8–r12 cannot be called with a
  C argument. The wrapper generator leaves registers that no call site sets untouched.
- **Conservative contracts.** High-register saves made r8–r12 look like arguments everywhere until
  the liveness pass learned to treat them as saves.

**Not done:**
- No bn6f renames or `// symdb:` comments: the bn6f checkout is shared with the running work.
- `gen/structs.h` is still not part of the build. The struct is copied into the file.

## How to run the next subsystem

```sh
python3 tools/sig/bounds.py && python3 tools/sig/derive.py
python3 tools/sig/ask.py run && python3 tools/sig/ask.py merge          # rounds: --round N --names ...
python3 tools/sig/subsystems.py
python3 tools/sig/subsys_prompt.py SUBSYSTEM --file NAME.c [--skip DEAD...] [--entries ...] > prompt.txt
python3 tools/sig/convert.py opencode/deepseek-v4-pro prompt.txt NAME.c
# install, build, difftest (-n, -S, and the state sweep for each dispatcher), oracle, lockstep
python3 tools/sig/record_subsystem.py SPEC.json && python3 tools/symdb.py stamp ENTRY...
python3 tools/sig/write_db.py
```
