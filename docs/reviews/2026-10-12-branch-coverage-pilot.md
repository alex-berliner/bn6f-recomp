# Branch coverage pilot (2026-10-12)

The user chose this over re-reviewing old installs against their assembly: find the blocks of each
function that no check's input runs, build inputs that run them, and compare the readable C against
the original there. It targets the mutation survey's "U" class: real changes on paths no check ran
(docs/reviews/2026-10-11-mutation-survey.md).

This is a pilot of steps 1-3 at aa4f8270 (2,346 readable functions).

**In short:**
- **Coverage:**
  - 79% of readable functions have every block of their original run by some check's input;
  - 1,261 of 7,958 blocks (16%) in 482 functions are run by none.
- **Step 2, a fuzzer's dictionary (no models):** reached 411 of those blocks (33%) with inputs the original runs
  cleanly. It found **7 real bugs**, now taken out.
- **Step 3, outside models (50 functions):** reached 30 of 77 target blocks:
  - glm-5.3-flash 22 of 38, MiniMax-M3 8 of 39;
  - well under a cent in all;
  - no new bugs on those blocks.
- **Grading:** of the survey's surviving mutants in the functions that got new inputs, the new inputs kill **29 of
  89**. That's 27 of 53 in the classes that are real bugs; none of the harmless ones.

## How

- **Tool:** `tools/blockcov/blockcov.py` (`build`, `measure`, `fuzz`, `synth`, `grade`).
- **Data:**
  - per-function coverage in `tools/blockcov/coverage-20261012.tsv`;
  - step 3 per block in `tools/blockcov/pilot-20261012.tsv`.
- **Blocks:**
  - Each block is one of the lifted original's: it starts at the entry, at a branch target, or after a
    conditional branch.
  - A coverage build of the host difftest puts a probe at every instruction of the `L_NAME` bodies and logs what
    the original loads. difftest.c only calls three hook macros, empty in the normal builds.
- **"Covered by a check":** an input the check *compares* runs the block. That means one of:
  - a captured call;
  - one of verify's 300 random inputs that isn't out of domain;
  - one of the oracle's 24 states that mGBA returns from and the oracle counts.
- **New inputs** are machine states (the snapshot format). One counts only when two things hold:
  - mGBA runs the original to its return, with no misaligned load, no open-bus read, and no input pointing into
    the free stack;
  - the lifted original runs it in domain.

  Then the difftest (`-S`: readable vs lifted) and the oracle (`-G`: readable vs mGBA) compare the readable C on
  it.
- **Step 2:**
  - Start from the function's captured calls and oracle states.
  - Set input registers, and bytes the original loads, to the function's own compare constants, constants it
    loads into registers, and jump-table cases (±1).
  - Keep variants that reach new blocks: 6 rounds of 32 per function.
- **Step 3:**
  - The model gets the instructions, the target block, the branch decisions on a path to it, and 3 real base
    states with the values the original loaded on them.
  - It answers with a JSON spec: base, registers, memory writes.
  - Feedback is the blocks reached, the last one, and the loads, or why the input was refused.
  - At most 3 rounds, 2 blocks per function.
  - The 50 functions are all 22 that hold survey survivors plus a stratified rest (11 S, 21 M, 15 L, 3 XL).

## Coverage (step 1)

| | blocks | share of 7,958 |
|---|---|---|
| captured calls | 6,078 | 76% |
| oracle states | 5,174 | 65% |
| random inputs | 3,246 | 41% |
| **no check** | **1,261** | **16%** |

- 1,862 of 2,344 functions (79%) are fully covered.
- 33 functions have no block covered at all.
- `menu_moveCursor*` are secondary entries without a lifted body of their own.

## Reaching the uncovered blocks (steps 2 and 3)

| | blocks tried | reached cleanly | rate | cost |
|---|---|---|---|---|
| step 2, dictionary | 1,261 (449 functions with a seed) | 411 | 33% | 60,448 host runs, about 1 minute on 8 cores |
| step 3, glm-5.3-flash | 38 | 22 | 58% | 81 calls, 0.99M tokens; about 45k tokens (about $0.001) per block |
| step 3, MiniMax-M3 | 39 | 8 | 21% | 106 calls, 0.41M tokens; about 52k tokens (under $0.001) per block |
| left after both | 820 | | | |

**Step 2 by size:**

| | reached | rate |
|---|---|---|
| S | 65 of 149 | 44% |
| M | 220 of 534 | 41% |
| L | 114 of 469 | 24% |
| XL | 12 of 109 | 11% |

33 functions have no seed: no captured call or oracle state that the original runs in domain.

**Step 3, by model:**

glm-5.3-flash reached most of its blocks on the first try (14 of 22):
- 30 of its replies had no parseable JSON;
- a lenient parser and more rounds should lift its rate.

MiniMax-M3 mostly wrote inputs the original could not run (18) or that stopped at the same block.

Costs use the plans' rates: Hyper about $0.02 per million tokens, MiniMax about $0.013.

## Differences found

All 9 came from step 2; step 3's 30 blocks showed none.

| function | what differs | verdict |
|---|---|---|
| battle_isAllianceActiveAndAiFlag0x4000_802DD2A | r1 = 0x4000 (`ldr; tst`) not left on the flag-clear path | bug, taken out |
| battle_setupPETNavis_8135314 | r4, r6, r7 not restored when a navi slot is filled | bug, taken out |
| chatbox_selectPress_8133C58 | r4, r6 not restored after SELECT | bug, taken out |
| sub_8009FCC | r6, r7 not restored when the queue is not empty | bug, taken out |
| object_updateCollisionData | r2 (flags \| 0x10000) not set in time stop | bug, taken out |
| battle_damagePlayerLeave1HP_8013FD0 | takes HP from the caller's r5, not the player | bug, taken out |
| objectStepTable_803A886 | table index truncated to u8 (index 0xFF reads entry 0) | bug, taken out |
| battle_objectDispatchPanelChange_80C6CFC, battle_object_collisionPhase_80C4F02 | panel data differs on mGBA only | artifact: a panel type past `_object_setPanelType`'s table, which the lifted IWRAM code reads from its ROM copy (`tools/oracle_triage.md`) |

- Six of the seven are register mirroring or a missing argument register on a path the game's own calls never
  took, the survey's U class.
- One (objectStepTable) is only reachable past the end of its table, but it differs on a clean input, so it went.
- `objectStepTable_803A886` was an old-prompt MiniMax-M3 install.

## Grading with the survey

The survey's mutants (at d73a473) were carried to this tree where the function's text is unchanged. 219
cheap-check survivors carried over:
- 89 are in functions that got new inputs;
- 33 are in functions whose uncovered blocks no new input reached;
- 97 are in fully covered functions. Inputs that reach blocks cannot help those: they differ only on values or
  combinations of blocks no input takes, or not at all.

| class | survivors | killed by the new inputs |
|---|---|---|
| offset | 8 | 6 |
| boundary | 13 | 7 |
| classic | 11 | 6 |
| store | 14 | 5 |
| registers | 6 | 2 |
| flag_leftover | 1 | 1 |
| **real-bug classes** | **53** | **27 (51%)** |
| flags (outside the contract) | 16 | 0 |
| sign_width (mostly equivalent) | 20 | 2 |
| **all** | **89** | **29 (33%)** |

How they were killed:
- 25 by both the difftest and the oracle;
- 4 by the difftest only;
- 1 by the oracle only.

No mutant my survey review called equivalent or outside the contract was killed.

## Recommendation for steps 4-5 (not done)

1. **Make new inputs permanent test inputs.**
   - Store the clean inputs with their mGBA exits in the snapshot store, beside the captured calls (e.g.
     `build-snapshots/NAME/snap_synth_K.bin`).
   - The captured-call difftest and the oracle then run them in every later verify, retype and codemod check, with
     no new wiring in verify.
2. **At install (the convert workflow):**
   - measure the candidate's block coverage (seconds per function);
   - run step 2 on what is uncovered, then step 3 for medium and larger functions only;
   - compare on the new inputs before accepting.

   A function whose blocks stay uncovered is installed with that noted ("blocks never compared"), not silently.
3. **For the installed backlog:**
   - run step 2 with more rounds on the 307 functions still uncovered;
   - then step 3 with glm-5.3-flash, with a lenient JSON parser and 4 rounds, on the 820 blocks left: at the
     pilot's rate about 1,700 calls and 20M tokens, under $1 of Hyper's daily credit, spread over its hourly
     limits.
   - Take out whatever differs, as here.
   - Report the remaining uncovered blocks per function in verify's report.
4. **Retyping (S4) and codemods (S3)** inherit the inputs from step 1. A retyped function then has to match the
   original on every block the inputs reach, not only on the game's calls.
5. **Seeds:** 33 functions have no seed the original runs. They need a capture from a route (where one exists) or
   a generator (J's `difftest_gen.c` way) before steps 2-3 can help them.
