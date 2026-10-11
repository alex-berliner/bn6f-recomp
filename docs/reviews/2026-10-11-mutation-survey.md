# Mutation survey of the checks (2026-10-11)

Joint plan S3 opens with this (review 1 A24): before tools rewrite thousands of functions, measure how
well the checks catch real bugs. Small realistic bugs were put, one at a time, into readable functions,
and every check verify runs was asked to find them.

**In short:**
- The checks catch about **three bugs in four**: 67% of the 1,175 mutants fall to the cheap checks
  (random and captured-call difftest, CPU oracle, leftover lint), and lockstep catches 16% of the rest.
- What gets through is mostly **code no test input reaches**: a path no captured call takes, a
  neighbouring flag bit no captured state has set, a negative value the captures never contain.
- Expect about **60 to 100 hidden bugs** (somewhere between 30 and 250) in the 1,503 readable
  functions the 51d348b review did not read. About half of them are in the 340 medium-sized ones.
- **Re-review the medium and large functions, not everything.** Sample the small ones first.
- Two checks were improved here:
  - the oracle now compares the return target (landed);
  - the leftover lint has a `--deep` mode (opt-in for now).
- The captured-call check had silently stopped running for **245 renamed functions**.

## How

- **Tool:** `tools/mutate/mutate.py` (plan, run, probe, recheck, lockstep, report, tsv). Per-mutant data
  is in `tools/mutate/survey-20261011.tsv`; my reading of 60 survivors is in
  `tools/mutate/review-20261011.tsv`.
- **Sample:** 239 functions at d73a473, stratified by size (S ≤ 20 halfwords, M ≤ 60, L ≤ 200, XL
  above) and by whether they have captured calls, at most 10 per file. All 4 functions no route runs
  were included.
- **Mutants:** about 5 per function, classes balanced. Each is built in a copy of cport (never the
  tree's). 111 compiled to the same object code and were dropped as equivalent; 2 did not compile.
- **Mutation classes:** each copies a bug class this project has had (git log, the farm's failure logs,
  `symbols/pool_drops.tsv`).

| class | what the mutant does | from |
|---|---|---|
| flag_leftover | a callee's flag answer read from r0 (`ZERO_FLAG()` → `R0 == 0`) | 67673ff, e988740 |
| flags | `SET_FLAGS_*` dropped, operands swapped, C/V lost | ec5c0ac |
| registers | wrong or missing register in an adapter or before a call | b86ffb0 |
| return_target | returns to LR+4 | b86ffb0, 80d5a92 |
| sign_width | `LDS16`→`LD16`, `(s8)`→`(u8)`, a cast dropped, a local widened | 4d2bee5 |
| offset | field offset or index stride off by one access | sub_813DA94 |
| store | store dropped, store or load width changed | farm memory diffs |
| boundary | constant ±1, `<`↔`<=`, `==`↔`!=` | f554532 |
| classic | `&&`↔`\|\|`, `+`↔`-`, a call dropped | textbook |

- **What counts as a kill:** a check reporting a difference that the unmutated code (the baseline)
  does not have.
- **Lockstep:** replayed for 200 mutants (150 random survivors of the cheap checks, 50 random killed
  ones), only on the routes whose run of the original executes the function.

## What the checks catch

| check | kills, of 1,175 | when it compared anything | compared nothing | not run |
|---|---|---|---|---|
| random difftest (300 inputs) | 33% | 67% | 596 (51%) | – |
| captured-call difftest | 34% | 61% | – | 517 (no captures) |
| CPU oracle (24 states) | 53% (41% before the return-target fix) | 61% | 164 | – |
| leftover lint | 1% | | | |
| new GCC warning (`-Wextra`, extra) | 3% | | | |
| any cheap check | **67%** | | | |
| lockstep | 51% of cheap-killed, **16% of cheap survivors** | | | |
| any check (estimate) | **73%** | | | |

Per class (lockstep: kills among the replayed cheap-check survivors):

| class | n | random | captured | oracle | any cheap | lockstep on survivors | any check |
|---|---|---|---|---|---|---|---|
| flag_leftover | 33 | 0% | 42% | 27% | 55% | 2/7 | 68% |
| flags | 148 | 16% | 20% | 36% | 42% | 0/37 | 42% |
| registers | 163 | 46% | 44% | 54% | 79% | 3/13 | 83% |
| return_target | 150 | 49% | 54% | 89% | 96% | 0/2 | 96% |
| sign_width | 66 | 8% | 6% | 15% | 20% | 2/21 | 27% |
| offset | 156 | 31% | 36% | 58% | 72% | 3/11 | 79% |
| store | 159 | 32% | 23% | 42% | 60% | 3/22 | 65% |
| boundary | 149 | 34% | 31% | 52% | 70% | 5/16 | 80% |
| classic | 151 | 40% | 40% | 60% | 76% | 5/12 | 86% |

By stratum (any cheap check):

| | captured | no captures |
|---|---|---|
| S | 88% | 69% |
| M | 57% | 59% |
| L | 68% | 49% |
| XL | 75% | 60% |

What each check is good for:
- **The oracle** kills the most (156 mutants only it kills). It starts from real memory, but loses 14%
  of mutants to states where nothing could be compared.
- **Captured calls** are the best check for small functions. They need captures, though: 44% of the
  sample had none, and 1,120 of today's 1,967 readable functions have none.
- **Random inputs** compare nothing for half the mutants: pointer-following code goes out of domain.
- **Lockstep** catches what reaches the screen on the 16 routes. It is the only check that caught 23
  survivors, among them both TestEventFlag-style mutants and a lost sign extension, which is the
  history of 67673ff again. It almost never sees return targets (1 of 15) or flag changes (3 of 44).
- **The leftover lint** in its default form catches 4 of the 33 TestEventFlag-style mutants.

## What gets through

383 of 1,175 mutants survive the cheap checks; lockstep would catch about 60 of them.

I read 60 random survivors against their code and assembly; 55 still survive after the oracle fix:

| what it is | share | harmful? |
|---|---|---|
| **U**: a real change on a path no check ran | 44% | yes |
| **C**: changes only registers or flags outside the contract (no known caller reads them) | 27% | no, if the contract is right; lockstep killed none of 9 |
| **E**: equivalent (a mask, an earlier test or a zero fill hides it) | 18% | no |
| **D**: differs only on values the game never passes | 7% | not today |
| **M**: a real change on a path a check ran but did not compare | 4% | yes |

Reach probes back this up: the same site with a probe in place of the mutation. 31% of the surviving
sites are run by no cheap check:
- 21/44 of offset survivors;
- 19/64 of store survivors;
- 17/42 of boundary survivors;
- but only 7/86 of flag survivors, which are mostly C.

Lockstep killed 12 of 46 unreached survivors and 10 of 93 reached ones.

Weighting these together: about **12% of real bugs escape every check**. A typical escape:

- **A path the inputs never take:** a selector value (`selector == 8`), a state, a status-effect bit
  or a timer path. These are the offset, store and boundary survivors.
- **A TestEventFlag answer read from r0:** it differs only when a neighbouring flag bit is set, and no
  captured state has one. Lockstep caught 2 of 7.
- **A lost sign extension or truncation:** the captured calls hold no negative position or tile id
  above 255.
- **A callee's argument register left unset:** in the captured calls the register already held the
  right value.

This matches what the 51d348b review found by reading code ("typically on paths no captured call or
lockstep route reached").

## The new harness (J's changes since d73a473)

My difftest and oracle verdicts above came from the harness as of d73a473 plus my oracle fix. J has
since changed it:
- misaligned loads on random inputs are out of domain;
- jump tables get valid selectors;
- there are new input generators;
- the oracle skips states the lifted original cannot run.

I carried the mutants over to ca6189b where the function's text is unchanged (752 of 1,175; the rest
are in functions renamed or taken out) and ran the cheap checks again:

| | changed verdicts, of 752 |
|---|---|
| killed by any cheap check | **2** (both now survive) |
| random difftest | 8 kills now out of domain, 4 new kills, 6 now compared |
| oracle | 1 new kill |
| warnings | 4 (the file around them changed) |

The new harness changes nothing that matters for these conclusions. Lockstep does not use the harness.
The probes, the 96-state oracle and the crash reruns are from the old tree; they would move as little.
All 50 kills that rested only on "hand-written version crashed" reproduced, so none was a timeout
caused by load.

## Hidden bugs in the functions the 51d348b review did not read

The review read farm sets 34-66 (381 functions) against their assembly and took out 55 of them,
bugs every check had passed:

| size | reviewed | taken out | rate |
|---|---|---|---|
| S | 63 | 1 | 2% |
| M | 182 | 30 | 16% |
| L | 121 | 22 | 18% |
| XL | 12 | 2 | 17% |

By writer, against what size alone predicts:
- glm-5.3-flash: 4 taken out where 7.8 were expected;
- qwen3.8-flash: 28 where 31.3 were expected;
- MiniMax-M3: 23 where 15.9 were expected.

Not reviewed: 1,590 functions readable before the review, **1,503 still readable today** (the rest were
taken out or renamed past tracking).

| writer | S, captured | S, no captures | M | L, XL |
|---|---|---|---|---|
| glm-5.3-flash (sets 1-33 and the earlier farm) | 568 | 494 | 282 | 14 |
| others (hand, Claude, unknown, glm-5.3, kimi, step) | 23 | 38 | 60 | 24 |

How the estimate is made, for each size band:
1. start from the review's rate for that size;
2. scale it up for functions without captures, by the ratio the mutation survey measured (S ×2.6, M ×1,
   L ×1.6);
3. scale glm-5.3-flash's functions by its observed 0.5, or leave them at 1.0.

| | expected hidden bugs | in S | in M | in L/XL |
|---|---|---|---|---|
| glm factor 0.5 | **57** (31-146) | 17 | 33 | 8 |
| glm factor 1.0 | **97** (52-258) | 31 | 56 | 9 |

Ranges are 95% intervals on the review's rates.

The small-function rate (1 in 63) drives the wide upper end. **The medium functions are the solid
part: about 33-56 bugs in 342 functions**, one in every 6-10.

The mutation survey says why size matters: a small function's few paths are mostly covered by its
captured calls (88% of mutants killed), while a medium one has paths no input takes (57%). Without
captures, small functions lose most of that advantage (69%).

## Recommendations

**Re-review against the assembly, but not everything:**
1. **All medium, large and XL functions not yet reviewed:** 282 + 14 by glm-5.3-flash and 84 by
   others, 380 in all. Expect about 40-65 bugs, about one per 6-9 functions read, the same yield as
   51d348b.
2. **The MiniMax-M3 old-prompt installs still in the tree** (sets 1-6 of the old prompt): M3's rate
   was 23% in the review. They are in the retype queue anyway; read them against the assembly then.
3. **Small functions: sample before committing.** Read 100 random small functions without captures
   (the riskiest small stratum, 532 of them). If it finds at most 1 bug, stop: the rest hold perhaps
   10-30 bugs among 1,100 functions, too thin to find by reading. If it finds 3 or more, extend.
4. **Don't re-review the reviewed sets 34-66 or the hand-written subsystems again.**

**Cheap check additions, by how much of what survives they catch:**
1. **Captured calls for every readable function.**
   - 245 renamed functions have captures stored under their old names. Verify looks them up by the
     new name, so their captured-call difftest has run nothing since the rename. All 245 pass on their
     old captures today.
   - Fix: look captures up by address, or have `tools/rename_symbol.py` link `build-snapshots/NEW` to
     the old folder.
   - Then capture the other ~870 readable functions that have none; nearly all are on a route.
   - This is the biggest lever for small functions (69% → 88%).
2. **The oracle's return target:** done ("CPU oracle: compare the return target", landed with this report). The oracle catches 89% of return-target
   mutants instead of 0%. On the tree it found nothing new at d73a473 or at ca6189b.
3. **96 oracle states instead of 24** for installs and the re-review. It killed 34 of the 383 survivors
   (9%), mostly offset, store and boundary. It costs about 4× the oracle's time (a median of 10 s → 40 s).
4. **`leftovers.py --deep` as the default,** after triaging its 22 open sites (most are helpers'
   unused `return R0;`). TestEventFlag-style mutants killed by the cheap checks: 55% → 94%. This is the
   one bug class that has already reached the game.
5. **A new-warning check with `-Wextra` (review 1 A23).** It killed 23 survivors, mostly dropped
   signed casts, at no cost. Most of those are harmless (E/D), so make it a review prompt, not a gate.
6. **Not worth much:** more random trials. Random inputs fail by going out of domain, not by too few
   trials. Structured inputs (A26, B11) would help, as J's generators show on the functions they cover.

**For S3's mechanical passes:**
- A pass whose output compiles to the same object code is proven equivalent: accept it without tests.
  The survey used this test; refactor-only passes (names, struct fields, constants) should mostly meet
  it.
- Anything else needs every check that applies: random and captured difftest, the 96-state oracle, the
  leftover lint with `--deep`, lockstep on the routes that run it.
- "Compared nothing" means not verified.
- Expect such a pass's bugs to be the kinds above: about 1 in 8 would get through. Read a sample of
  each pass's changed medium functions against the assembly.

## Left undone

- The lockstep probe pass (`lockstep --probe`): which routes run a surviving statement. Written, not
  run.
- More random trials. Not run, for the reason above.
- The 22 open `--deep` sites. Listed by `tools/abi/leftovers.py --deep`, not triaged.
- Linking the 245 renamed functions' captures: not done, because it changes what other workstreams'
  verifies check. Measured only.

## Rerunning

```sh
tools/mutate/mutate.py catalog && tools/mutate/mutate.py plan --functions 240 --per-function 5 --per-file 10
tools/mutate/mutate.py run --jobs 8                 # cheap checks; resumable
tools/mutate/mutate.py probe --ids LIST             # reach probes
tools/mutate/mutate.py lockstep --select LIST       # lockstep on chosen mutants
tools/mutate/mutate.py report; tools/mutate/mutate.py tsv OUT.tsv
MUTATE_OUT=build-mutate-new tools/mutate/mutate.py reanchor build-mutate OLD_GAME_COPY   # same mutants, later tree
```
