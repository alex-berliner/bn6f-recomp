# Prompt benchmark on MiniMax-M3 (2026-10-10)

The first run of the standing benchmark (review 3 Area 6.3; joint plan S2 "Measurement"). The question:
per size band, what does the farm's typed prompt cost per accepted function, compared with the prompt it
replaced? Two extra arms ran too (callee cards, best-attempt hand-over), because M3's tokens are nearly free.

**In short:** on these 60 hard functions, the old prompt passed 3.5 times as often as the typed one (21 of
120 attempts against 6 of 120). Per accepted function, it used about a quarter of the tokens (54k against
230k at 60 lines or fewer). Paired on the same function and run, the typed arm never passed where the old
one failed; the old arm passed 15 times where the typed one failed. The typed arm's one repair mostly goes
to the register check. Behind that check, 77% of its rejected rounds would not even compile.

## Setup

- **Tools and files:**
  - the tool: `tools/convert/bench/bench.py` (modes `run`, `summary` and `retest`);
  - the function set: `tools/convert/bench/functions.tsv`;
  - the old prompt, copied verbatim: `tools/convert/bench/prompt_old.py`;
  - one row per attempt: `tools/convert/bench/results-20261010.tsv`;
  - the rounds with their code: `tools/convert/log/bench/RUN/results.jsonl` (not in git).
- **The same for every arm:**
  - model minimax/MiniMax-M3, with a 40k output cap;
  - the main tree's headers;
  - one difftest binary, built from f2b568e;
  - the farm's acceptance test;
  - one round plus one repair (no repair after a reply without code).
- **Arms:**
  1. **typed:** today's farm prompt (a typed function plus a thin `Next` adapter), with the register check.
  2. **old:** the prompt before 329fbfd (register-form C allowed), without the register check.
  3. **cards:** typed, plus one card per called function: its symbols/db summary and interface.
  4. **handover:** typed, but round 0 is the farm's repair prompt applied to the best candidate another
     model wrote before (pool.py's ranking). The prompt includes today's register-check and behaviour
     reports for that candidate.
- **Runs:**
  - run 20261010-m3: all four arms;
  - run 20261010-m3-b: typed and old again, because one sample per function proved noisy. Across the two
    runs, the typed passes never repeated on the same function; 5 of the old ones did.

### The function set: 60 run, 2 excluded

| band (asm lines) | functions | chosen as |
|---|---|---|
| <=10 | 8 | small |
| 11-20 | 10 | 8 small, 2 experiment |
| 21-30 | 13 | 12 failure class, 1 experiment |
| 31-40 | 15 | 11 failure class, 4 experiment |
| 41-60 | 10 | 6 failure class, 4 experiment |
| >60 | 4 | experiment |

- **experiment:** the max-token experiment's 16 functions. One is excluded, sub_80FDE04, because every
  captured call is out of domain, so it can't pass in any arm.
- **small:** 8 per band. They come from functions where no candidate had passed and M3 had not tried. The
  pick goes round-robin over their last failure class.
- **failure class:** every function of 21-60 lines where no candidate had passed and M3 had not tried.
  There were 31 and all were taken, minus `main_`: the main loop never returns, so the difftest can't
  compare it.
- **Thin bands:**
  - No untried function of 60 lines or fewer was left. Of the failed ones, only those 30 in 21-60 lines
    were untried by M3, so those bands hold all there were.
  - M3 had already tried 10 of the 16 experiment functions with the typed prompt.
  - While the benchmark ran, pool.py re-tested against f2b568e (the refined contracts). For 14 of the 51
    picked "failed" functions, an old candidate now passes. They stay in the set: the benchmark measures
    the prompts, not the backlog.

## Results

**Typed against old, per band**: two runs pooled, so each band has twice its functions as attempts. Tokens
count input (cache reads included) plus output, over the rounds asked.

| band | attempts | typed: first round | typed: after repair | old: first round | old: after repair | typed tokens per accepted | old tokens per accepted |
|---|---|---|---|---|---|---|---|
| <=10 | 16 | 0 | 0 | 0 | 0 | - (116k spent) | - (94k spent) |
| 11-20 | 20 | 0 | 2 (10%) | 7 (35%) | 10 (50%) | 99k | 16k |
| 21-30 | 26 | 2 (8%) | 3 (12%) | 4 (15%) | 6 (23%) | 111k | 45k |
| 31-40 | 30 | 0 | 0 | 2 (7%) | 2 (7%) | - (448k spent) | 159k |
| 41-60 | 20 | 0 | 1 (5%) | 1 (5%) | 3 (15%) | 286k | 97k |
| >60 | 8 | 0 | 0 | 0 | 0 | - (154k spent) | - (135k spent) |
| all <=60 | 112 | 2 (2%) | 6 (5%) | 14 (12%) | 21 (19%) | 230k | 54k |

- **Runaways** (replies with no code): the typed arm had 1 of 238 rounds, the old arm none of 226. At the
  output cap: typed 2 rounds, old 1. M3 rarely runs away; glm-5.3-flash did in 41-45% of its replies.
- **Paired, on the same function and run:**
  - both passed: 6;
  - only old passed: 15;
  - only typed passed: 0;
  - neither: 99.

  An exact sign test on the 15 to 0 split gives p ≈ 0.00006. Each band's cell holds only 0-10 passes, but
  the overall direction is not noise.
- **Per round:** the typed prompt reads about 12% more input (4.3k against 3.8k tokens). It writes about
  23% more output (a mean of 2.2k against 1.8k tokens; medians 871 and 665).
- **Glm, for comparison** (review 3 Area 6): typed 119k against old 37k at 11-20 lines, and 181k against
  58k at 21-30. M3's ratio points the same way.

**All four arms, per band** (`bench.py summary 20261010-m3 20261010-m3-b`; cards and handover have one run):

| arm | band | attempts | first round passed | passed after the repair | tokens | tokens per accepted function | runaways (rounds) | rounds at the cap |
|---|---|---|---|---|---|---|---|---|
| typed | <=10 | 16 | 0 (0%) | 0 (0%) | 116k | - | 0 | 0 |
| typed | 11-20 | 20 | 0 (0%) | 2 (10%) | 198k | 99k | 0 | 0 |
| typed | 21-30 | 26 | 2 (8%) | 3 (12%) | 332k | 111k | 0 | 1 |
| typed | 31-40 | 30 | 0 (0%) | 0 (0%) | 448k | - | 1 | 1 |
| typed | 41-60 | 20 | 0 (0%) | 1 (5%) | 286k | 286k | 0 | 0 |
| typed | >60 | 8 | 0 (0%) | 0 (0%) | 154k | - | 0 | 0 |
| old | <=10 | 16 | 0 (0%) | 0 (0%) | 94k | - | 0 | 0 |
| old | 11-20 | 20 | 7 (35%) | 10 (50%) | 160k | 16k | 0 | 1 |
| old | 21-30 | 26 | 4 (15%) | 6 (23%) | 268k | 45k | 0 | 0 |
| old | 31-40 | 30 | 2 (7%) | 2 (7%) | 317k | 159k | 0 | 0 |
| old | 41-60 | 20 | 1 (5%) | 3 (15%) | 291k | 97k | 0 | 0 |
| old | >60 | 8 | 0 (0%) | 0 (0%) | 135k | - | 0 | 0 |
| cards | <=10 | 8 | 0 (0%) | 1 (12%) | 45k | 45k | 0 | 0 |
| cards | 11-20 | 10 | 2 (20%) | 2 (20%) | 67k | 34k | 0 | 0 |
| cards | 21-30 | 13 | 0 (0%) | 0 (0%) | 141k | - | 0 | 0 |
| cards | 31-40 | 15 | 0 (0%) | 1 (7%) | 219k | 219k | 0 | 1 |
| cards | 41-60 | 10 | 0 (0%) | 1 (10%) | 296k | 296k | 0 | 2 |
| cards | >60 | 4 | 0 (0%) | 0 (0%) | 101k | - | 1 | 0 |
| handover | <=10 | 8 | 0 (0%) | 1 (12%) | 131k | 131k | 0 | 1 |
| handover | 11-20 | 10 | 3 (30%) | 3 (30%) | 117k | 39k | 0 | 0 |
| handover | 21-30 | 13 | 0 (0%) | 1 (8%) | 211k | 211k | 0 | 0 |
| handover | 31-40 | 15 | 1 (7%) | 1 (7%) | 298k | 298k | 1 | 0 |
| handover | 41-60 | 10 | 1 (10%) | 1 (10%) | 192k | 192k | 0 | 1 |
| handover | >60 | 4 | 1 (25%) | 1 (25%) | 80k | 80k | 0 | 0 |

**The extra arms, in run 20261010-m3** (all four arms on the same 60 functions):

| arm | first round | after repair | tokens per accepted | only this arm passed / only typed passed |
|---|---|---|---|---|
| typed | 0 | 2 (3%) | 418k | - |
| cards | 2 (3%) | 5 (8%) | 174k | 5 / 2 (p ≈ 0.45) |
| handover | 6 (10%) | 8 (13%) | 129k | 8 / 2 (p ≈ 0.11) |
| old | 4 (7%) | 8 (13%) | 75k | 6 / 0 |

- Callee cards are not measurably better or worse than the typed prompt. By the review's rule ("kept if not
  worse"), they can stay as an option, but this run doesn't argue for them.
- The hand-over is the best typed arm: 6 of its 8 passes came in round 0, from another model's attempt. It
  cost more input per call (4.9k tokens), but less per pass than the plain typed arm. With one run it is
  suggestive, not proven.

### Why typed attempts fail

The typed arm's last rounds failed as follows (both runs):

| check | rounds |
|---|---|
| register check | 59 |
| differs on captured calls | 24 |
| compile | 23 |
| other | 8 |

The old arm's: compile 59, captured calls 29, other 11.

- **The register check runs first and stops 128 of 238 typed rounds.** The same code re-tested without it
  (no model calls) shows what lies behind the check:

  | result without the check | rounds |
  |---|---|
  | would not compile | 98 (77%) |
  | differs from the original | 26 |
  | behaves correctly | 4 (2 functions) |

  So the one repair mostly fixes form, and the compiler's complaints come too late to act on. In run
  20261010-m3 the cards and handover arms show the same: the check stopped 114 rounds, and 69 of them
  would not compile.
- **Compile errors, both arms:**
  - the top ones are implicit declarations (typed 29 rounds, old 48) and undeclared names (15 and 37);
  - old only: calling a game function as if it were typed C (18 rounds);
  - old more often: wrong arguments to `run`/`swi_call` (23 rounds against 6).
- **Near-miss register notes:** a helper commented "Register-form helper:" instead of `register form:`
  cost NPCCommand_remove_shadow both of its typed rounds in run 1. Normalising that note makes it pass.

## What this means, per band

These numbers come from M3, one repair, and hard functions (51 of the 60 had defeated other models).

| band | measured | recommendation |
|---|---|---|
| <=10 | typed and old: 0 of 16 attempts each; cards and handover: 1 of 8 each | Don't farm these with either prompt. Every earlier model had failed these 8, and here they mostly fail to compile (old) or the register check (typed). They include an m4a trampoline, a tail call into the next routine and a return-target case. Use the mechanical pass, the hand-over, or a strong model. |
| 11-20 | old 50% at 16k per pass; typed 10% at 99k | The old prompt, about 6 times cheaper per pass. Typing happens in a later stage. |
| 21-30 | old 23% at 45k; typed 12% at 111k | The old prompt, about 2.5 times cheaper. |
| 31-40 | old 2 of 30; typed 0 of 30 | Neither is worth M3's repair round. Fix the prompt first (below), or route to the strong tier. |
| 41-60 | old 15% at 97k; typed 5% at 286k | The old prompt is cheaper, but both are poor. Same advice as 31-40. |
| >60 | only the hand-over passed (1 of 4) | Not a farm band. |

**Overall:**
- On M3, typed C comes cheaper from two stages: readable register form first (the old prompt), then the
  mechanical typify (S3) or a retype job (S4). That is review 3's A20/B11/B32 plan.
- The typed prompt's 4x cost is mostly fixable tool behaviour, so it should be re-benchmarked after:
  1. running every check and reporting all problems together (review 3 Area 3 step 3);
  2. generated callee helpers (Area 4.2), aimed at the implicit-declaration and undeclared-name errors;
  3. accepting or inserting the `register form:` note by tool;
  4. handing over the best earlier attempt (B31), the best typed arm here.
- Old-arm passes are register form. Whether to install them per band and retype them later is the user's
  call. None of them were installed or put in tools/convert/accepted.

## Cost

- **Benchmark calls:** 701 M3 calls, 4.73M tokens: 2.99M input including cache reads, and 1.74M output.
  - By arm: typed 1.54M, old 1.27M (two runs each); cards 0.87M, handover 1.03M (one run each).
  - The other 30k tokens went to 7 calls lost to a harness bug: a relative difftest path, fixed in the
    committed tool. Those pairs were asked again.
  - With scorecard.py's measure (input without cache reads, plus output): 4.33M tokens.
- **Quota** (`tools/convert/quota.py`, snapshots in tools/convert/log/bench/): MiniMax's weekly meter went
  from 87% to 81% between 19:00 and 22:59.
  - The M3 farm ran in the same window: 836 calls, 5.39M tokens. The benchmark's share, about 47% of the
    window's tokens, is about 2.8 points of the weekly quota.
  - Together that is about 1.7M tokens per weekly point, provided nothing else used MiniMax in the window.
  - The 5-hour meter reset during the run (72%, then 97%, then 54%), so it gives no measure.

## Farm results from the benchmark

- **Typed-C passes are real farm results.** They are logged to tools/convert/log/FUNC.minimax_MiniMax-M3.jsonl
  with kind "bench ARM". The new accepted files in tools/convert/accepted/ are:
  - ProcessGFXAnims
  - npc_updateHopMovement_809e916
  - sprite_makeScalable
  - sub_8005AF4
  - sub_802F710
  - sub_803D274
  - sub_80E1A0C
  - sub_8109D98
  - sub_8109E4A
  - sub_8117BC8
  - sub_811A1B0
  - sub_811EDB4
  - sub_812B29C

  The other typed passes were on functions that already had an accepted M3 typed file, which was kept.
- **Old-arm passes stay out of the farm's folders.** They cover 16 functions and are register form; they
  are kept in tools/convert/log/bench/*/results.jsonl only.

## Reproduce

```
tools/wf/.venv/bin/python tools/convert/bench/bench.py run RUN --arms typed,old [--jobs 3]
tools/wf/.venv/bin/python tools/convert/bench/bench.py summary RUN [RUN...] --tsv OUT.tsv
tools/wf/.venv/bin/python tools/convert/bench/bench.py retest RUN    # after a header or difftest change
```

- Rerun after every prompt, header or model change. A new prompt becomes a new arm.
- A rerun of `run` with the same RUN finishes only what is unfinished.

## Left undone

- **Too few runs:** two runs of typed and old, one of cards and handover. Most band cells have 0-3 passes,
  and cards against typed is undecided.
- **No test of the second stage:** the cost of the mechanical or model retype that would follow the old
  prompt is unknown until S3/S4 exist.
- **No other models or round counts:** only M3 was measured, with one repair (the farm uses three).

---

## Re-run with the typed prompt typed-2 (2026-10-11)

One versioned change to the farm's typed prompt, made from the fixes listed above, and benchmarked against
today's typed prompt (now called typed-1) and the old prompt.

**In short:**
- typed-2 passed 22 of 120 attempts, typed-1 13 and old 18.
- Paired on the same function and run, typed-2 passed where typed-1 failed 16 times, and the reverse
  happened 7 times (sign test p ≈ 0.09).
- Per accepted function, typed-2 and typed-1 cost about the same (115k and 119k tokens); old costs 74k.
- **typed-2 is now the farm's default** (`wf farm`, `--prompt typed`). The old prompt stays as `--prompt old`.
- The gain is concentrated in the <=10 band (8 of 16 against 0), where the hand-over gave M3 near-miss
  candidates by other models. Above 10 lines typed-2 and typed-1 are level (14 and 13 of 104), and old is
  still ahead at 11-40 lines.

### What changed (version typed-2, hash fd38846da391)

`tools/convert/farm.py` PROMPT_VERSION; every logged round records `prompt_version` next to `prompt_hash`.
typed-1 (hash 8ad67e179f33) is kept verbatim in `tools/convert/bench/prompt_typed1.py`.

1. **Every problem at once.** The test runs every check and lists them all, worst first: entry point,
   compile errors, behaviour, register check, lifted style, review rules. The difftest runs whenever the
   code compiles. An error inside a header macro names the candidate's line, and repeated errors appear once.
2. **Generated callee wrappers.** For each function the lifted body calls, `tools/abi/gen.py`
   `contract_wrapper()` writes a typed `call_NAME()` from the callee's lifted contract:
   - one u32 per input register (r10 stays as on entry);
   - it returns nothing, R0, or a struct of every output register and flag.

   The prompt lists the declarations. The test compiles the candidate with the wrappers it calls. The kept
   candidate includes them, with a `register form:` note, so it installs on its own.
3. **Register-form notes by the tool.** A note in other words ("Register-form helper:") is put in the
   lint's words. A one-call helper is noted by the tool (set R0.., one GUEST_CALL, GUEST_CALL_ADDR or
   swi_call, return R0 or a flag). Any other register use is still reported.
4. **Hand-over** (`tools/wf/wf/tools/handover.py`). Each round starts from the best attempt so far, other
   models' earlier candidates included, with a one-line history of the others. The ranking: passed > behaves
   the same but breaks a form rule > differs only on random inputs > fewer differing captured calls >
   compares nothing > compile error > no entry point, then fewer form problems.
5. **Texts.** The rules now say:
   - calls go through the wrappers; GUEST_CALL_ADDR for a call through an address, swi_call for BIOS;
   - labels are `A_LABEL` macros;
   - signed loads are LDS8/LDS16, and ST* are statements;
   - file-scope initializers must be constants.

   The example uses a wrapper. The repair text says whose attempt it shows.

Not changed: the convert workflow's subsystem prompt keeps typed-1's conventions. The farm's `--prompt old`
keeps the test its benchmark arm ran (only the first failing check).

### Setup

- Same 60 functions, model, output cap, and one round plus one repair as the first run.
- One difftest and one set of headers, from the lift of 33b5c61 (`--difftest`).
- **Runs:** 20261010-t2-a and 20261010-t2-b, each with three arms, 23:50 to 04:08.
- **Arms:**
  - typed2: the farm's prompt and test, hand-over included. M3's own earlier candidates are left out
    (they come from other arms and runs).
  - typed: typed-1 with its test (the register check first, only the first failing check). Re-tested on 60
    saved rounds of the first run, it gives the same class in all 60.
  - old: unchanged.
- **Data:** one row per attempt in `tools/convert/bench/results-20261011.tsv`; the rounds with their code
  in `tools/convert/log/bench/20261010-t2-{a,b}/results.jsonl` (not in git).
- **Reproduce:** `bench.py run RUN --arms typed2,typed,old --difftest PATH` (the default arms now), then
  `bench.py summary RUN...`. The summary adds two tables for typed-2: every failing check, and what
  round 0 started from.

### Results per band

Two runs pooled. Tokens: input (cache reads included) plus output, over the rounds asked.

| band | attempts | typed2: first round | typed2: after repair | typed: first round | typed: after repair | old: first round | old: after repair | tokens per accepted: typed2 / typed / old |
|---|---|---|---|---|---|---|---|---|
| <=10 | 16 | 5 | 8 (50%) | 0 | 0 | 0 | 0 | 13k / - / - |
| 11-20 | 20 | 4 | 6 (30%) | 2 | 5 (25%) | 5 | 7 (35%) | 41k / 35k / 17k |
| 21-30 | 26 | 2 | 4 (15%) | 2 | 6 (23%) | 5 | 5 (19%) | 128k / 49k / 50k |
| 31-40 | 30 | 1 | 1 (3%) | 0 | 1 (3%) | 1 | 5 (17%) | 782k / 414k / 81k |
| 41-60 | 20 | 1 | 2 (10%) | 0 | 1 (5%) | 1 | 1 (5%) | 281k / 318k / 243k |
| >60 | 8 | 0 | 1 (12%) | 0 | 0 | 0 | 0 | 326k / - / - |
| all <=60 | 112 | 13 (12%) | 21 (19%) | 4 (4%) | 13 (12%) | 12 (11%) | 18 (16%) | 105k / 103k / 64k |
| all | 120 | 13 (11%) | 22 (18%) | 4 (3%) | 13 (11%) | 12 (10%) | 18 (15%) | 115k / 119k / 74k |

- **Paired** (passed after the repair):
  - typed2 against typed: both 6, only typed2 16, only typed 7, neither 91 (p ≈ 0.09);
  - typed2 against old: both 7, only typed2 15, only old 11, neither 87 (p ≈ 0.56).
- **Against the first run:** typed-1 passed 13 here against 6 then, and old 18 against 21. The headers
  and difftest changed in between (refined contracts), so compare arms within a run, not across runs.
- **Per call:** typed2 reads 5.4k input tokens (typed-1 4.2k, old 3.8k) and writes 5.9k output on average
  (typed-1 2.3k, old 2.0k). It hit the 40k cap in 11 rounds against 1. The hand-over's repair prompts are
  longer, and the replies longer still.

### What the fixes did

- **The register check no longer eats the repair.** typed-1 stopped 126 of its 236 rounds at the register
  check. typed-2 found register problems in 122 of 224 rounds, but only 15 had nothing else wrong; the
  other 107 rounds' reports also carried the compile errors or behaviour differences. Two passing typed-2
  candidates had a helper the tool noted.
- **Invented call macros are mostly gone.** Implicit declarations of GUEST_CALL_INDIRECT and the like fell
  from 20 rounds (typed-1) to 4 (old: 40). 173 of 221 typed-2 replies call a generated wrapper, and so do 12
  of the 22 passes.
- **New top compile problem: several drafts in one reply.** 31 of 221 typed-2 replies hold more than one
  complete version in separate ```c blocks (typed-1 14, old 17). `extract_code` joins every block, so they
  fail as redefinitions. Together with 7 rounds that named the typed function after the game function
  (lifted.h already declares it as `Next NAME(void)`), they make up most of typed-2's 35 rounds with
  "conflicting types" or "redefinition" errors.
- **Undeclared names remain** (30 rounds): register-named locals (r0, r4), made-up constants, A_ names of
  labels that are not in bn6f_syms.h.

What round 0 started from (typed2), and how those attempts ended:

| round 0 started from | attempts | passed in round 0 | passed after the repair |
|---|---|---|---|
| another model's candidate that differs on random inputs | 28 | 5 | 9 |
| another model's candidate that behaves the same but breaks a form rule | 20 | 4 | 6 |
| no earlier candidate (the first prompt) | 24 | 3 | 5 |
| another model's candidate that differs on captured calls | 42 | 1 | 1 |
| another model's candidate that does not compile | 6 | 0 | 1 |

- All 8 passes at <=10 lines started from a candidate that differed on 1-3 of 300 random inputs (sub_8116CE0,
  sub_8117690, sub_8117BC8, sub_8118F24, sub_811C9FC). Every arm without that start failed all 16.
- A start that differs on captured calls almost never led to a pass (1 of 42).

### Verdict per band: typed-2 against old

The user's current choice is the old prompt for 11-30 lines while it is better. typed-2 output is typed C;
old output is register form, which still needs the retype queue.

| band | measured (passed after the repair, tokens per pass) | verdict |
|---|---|---|
| <=10 | typed2 8/16 at 13k; old 0/16 | typed-2 (the hand-over) |
| 11-20 | typed2 6/20 at 41k; old 7/20 at 17k | old: as many passes at less than half the tokens |
| 21-30 | typed2 4/26 at 128k; old 5/26 at 50k | old |
| 31-40 | typed2 1/30; old 5/30 at 81k | old |
| 41-60 | typed2 2/20; old 1/20 | neither does well; typed-2 if the band is farmed at all (its passes are typed) |
| >60 | typed2 1/8; old 0/8 | not a farm band |

The cells hold 0-8 passes, so a difference of one or two passes is noise.

### Cost

- **Benchmark calls:** 688 M3 calls, 5.4M tokens (3.08M input with cache reads, 2.33M output).
  - By arm: typed2 2.53M, typed-1 1.55M, old 1.33M.
  - By scorecard.py's measure (input without cache reads, plus output): 4.47M.
- **Quota** (`tools/convert/quota.py`): MiniMax's weekly meter went from 80% (23:38) to 74% (05:15).
  - In that window MiniMax served 692 benchmark calls (5.49M tokens) and 800 others (3.87M, mostly the
    old-prompt M3 farm).
  - The benchmark's share, about 59%, is about 3.5 points of the weekly quota: roughly 1.6M tokens per point.

### Left undone

- **Extraction of multi-draft replies:** keep the last block that defines the entry point. It is a tool
  fix, not a prompt change, but it should come next: typed-2's longer replies make it its top compile
  failure.
- **Name the typed function apart from the game function** (a rule or a tool rename).
- **Typed-2's output length:** 2.5 times typed-1's. Worth a look at what the replies spend it on before
  the next prompt version.
- **Still on typed-1:** the convert workflow's subsystem prompt.
- **Not done yet:** generated adapters and the prefilled `abi:` line (review 3 Area 4.3), and reports in
  game terms (Area 3.5).
- **Duplicate wrappers:** two candidates in one farm group that call the same callee each carry its wrapper.
  The convert workflow's renaming keeps them apart (`call_X_FUNC`); one shared copy would be tidier.
- **Hand-over cost:** the farm tests every earlier candidate of a function once, at the start of its run,
  which takes seconds to a minute per function.
