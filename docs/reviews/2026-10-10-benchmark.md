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
