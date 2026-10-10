# Model pipeline review: what models do, what they cost, and what tools should do instead

Moderator: C3 (Opus 5.5, medium). Debaters: A (Opus 5.5, xhigh) and B (Fable 5.1, xhigh), both read-only. All figures were measured on 2026-10-10 from `tools/convert/usage.jsonl` (about 5,800 calls), `tools/convert/log/*.jsonl`, the Burr run states, `scorecard.py`, the max-token experiment and `lint.py`. A farm run was live while they measured, so some counts were still moving. This review builds on the first one (`2026-10-10-1-non-llm-cleanup.md`, called "review 1" below).

Terms used below:
- A **runaway** is a reply that spends its whole output budget on hidden reasoning and returns no code.
- A **round** is one model attempt. Round 0 is the first try; rounds 1-3 are repairs.
- A **pass** is a candidate that clears every check.
- An **adapter** is the thin `Next NAME(void)` wrapper that moves guest registers in and out of a typed C function.

## Summary (priority order)

1. **Today, at almost no cost:**
   - Put MiniMax-M3 to work. Its plan is about 0.1% used and the period ends 10-13. Take `quota.py` snapshots before and after the first batch, to check that the logged tokens match the plan meter.
   - Re-test the roughly 2,600 saved *failed* candidates against today's headers, with no model calls. This should yield about 35-100 functions.
   - Never resend an identical prompt after a runaway.
   - Treat Hyper's "out of credits" error (HTTP 402) as down until the next 10:13 reset, not for one hour.
   - Remove `hyper/minimax-m3` from the routing. It spends Hyper credit on a model the MiniMax plan gives for free.
2. **Stop paying for runaways.** They are 41-45% of all output tokens, and output is about 90% of cost. Abort a reply in the stream once its reasoning passes a budget with no code started, then hand the function to another model. First check that aborting stops the billing.
3. **Cut the repair chain.**
   - Allow one repair per model, and only for compile and behaviour failures. Then switch to a fresh model, starting from the best attempt so far.
   - Cap spending at about 60k tokens per function.
   - Why: rounds 2-3 used about 37% of the tokens for about 10% of the passes.
4. **Fix the failure reports models get.**
   - Compile line numbers are off by 2 in 683 of 753 reports.
   - Run every check and report all problems together.
   - Let a tool fix mechanical rule breaks instead of spending a repair round on them.
   - Skip functions that can't be tested.
5. **Measure before deciding.** Add provenance (time, prompt hash, cap, settings) to every log row. Keep a cost ledger in each plan's own units, per *installed* function. Build a standing benchmark of about 60 functions. The typed prompt shipped unmeasured; it may cost about 2x at 21-30 instructions, so the benchmark runs the old prompt alongside it.
6. **One shared work queue that every provider pulls from at once**, routed by function size. Spend Hyper's daily credit on a schedule. Pacer settings stay as they are unless the user changes them.
7. **Let tools write the boilerplate.** `gen.py` generates the callee helpers and adapters; the model writes only the typed body plus a one-line signature. Fix the contradictions in the prompt rules.
8. **The 1,418 register-form functions:**
   - review 1's mechanical typing goes first;
   - then a cheap "retype" job does the rest, giving the model the verified body and no assembly. This is about 20M tokens, under one Hyper day.

A and B converged on every point. The only numbers still open are the thin typed-prompt samples at 30 instructions and below, which the benchmark will fill.

---

## Area 1. Runaway replies and reasoning budgets

**Ideas:**
- A1 / B1: abort a runaway mid-stream.
- B2 / A16: an experiment with reasoning-effort settings.
- B3: never resend the same prompt after a runaway.
- A32 / B20: keep the runaway's reasoning text and reply tail.
- B33: sampling settings (temperature or seed) as an experiment arm.

**Data:**
- Runaways by share of tokens:

| Measure | Share |
|---|---|
| All models, input plus output | 35-37% |
| All models, output only | 41-43% |
| glm only, output | 45% |

- Output is the figure that tracks cost.
- 99% of no-code replies stopped at the output cap. Raising the cap from 24k to 40k did not reduce them.
- On the typed prompt, 53% of glm's round-0 calls ran away.
- Z.ai glm-5.3 returned three empty 64k replies of about 16 minutes each on the sound-library subsystem.

**Debate:**
- A first proposed one budget per model, for example 11.1k for glm.
- B showed that glm's *passing* replies on the typed prompt reach 26.7k at the 95th percentile (n=32). An 11.1k budget would abort about a third of the good replies.
- They agreed to budget per (model, prompt kind, size band).

**Agreed approach:**
1. B3 now: after a no-code reply, the next attempt must change the model or the task.
2. Count reasoning tokens in `llm.py`'s stream. Abort when the budget is passed and no code block has started. Then re-queue the function to the next model by size.
   - Budgets are the 95th percentile of passing replies per (model, prompt kind, size band).
   - A band with fewer than about 30 passing replies falls back to the (model, prompt kind) budget, then to the cap.
   - Recompute the budgets weekly.
   - Starting points: glm 12k (old prompt or retype), glm 20k (typed), qwen 22k, deepseek-flash 24k.
3. Billing check: run 50 aborted calls between two `quota.py` snapshots, with no other Hyper traffic in between.
   - About 5 units used means billing stops at the abort.
   - About 15 means it runs on to the cap.
   - If billing does run on, lower the caps per prompt kind instead. A 24k cap for functions of 30 instructions or fewer loses no passes.
4. Run a reasoning-effort arm on the benchmark: glm thinking off or low, qwen with an 8k budget, M3 off against 4-8k.
5. Store the reasoning tail of aborted calls and read about 20, to see what the models loop on.
6. Sampling settings (B33) are an experiment arm only.

**Gain:** up to about 40% of Hyper output tokens. Quote it as "up to" until the billing check is done.

**Effort:** about a day for steps 1-3.

**First step:** add a `budget` argument and an `aborted` log flag to `tools/llm.py` (`_read_events`), and add B3 in `tools/wf/wf/graphs/farm.py`.

**Review 1:** no conflict. Step 4 needs the benchmark from Area 6.

## Area 2. Repair rounds and reusing paid-for work

**Ideas:**
- A2 / B4: one repair per model, then a fresh model.
- B43: a token budget per function.
- A27: repair from the best attempt so far.
- B5: the repair prompt lists the earlier failures.
- B31: hand the best attempt to the next model.
- B30: try the flag variants mechanically.
- A6: keep candidates that behave correctly but are still in register form.
- A8: re-test failed candidates.
- A9: repair the candidates that verify later dropped.
- B41: a reply cache.
- A26: parallel first attempts.

**Data:** how often the next round passed, by failure:

| Failure | Next round passes |
|---|---|
| Compile error | 46% |
| Behaviour on random inputs | 41% |
| Behaviour on captured calls | 33% |
| Review rule | 14% |
| Style | 9% |
| No code | 6% |
| Register check | 6% |
| Nothing testable | 0 of 44 |

- Rounds 2-3 used 9.9-15.7M tokens for 144-165 passes, about 69-95k per pass. Round 0 costs about 10-16k per pass.
- 41% of repairs after a behaviour failure got worse.
- 202 of 419 functions that never passed ended on a worse attempt than their best.
- Fresh models on glm's total failures: qwen passed 15 of 33, M3 8 of 38.

**Debate:**
- A26 (parallel attempts): B dropped it; A first deferred it, then also dropped it. Final: deferred until the M3 meter check shows whether M3 is nearly free.
- B41 (reply cache): dropped, because `max_tokens.py` already skips answers it has saved.

**Agreed approach:**
1. A8 first, since it is free. Re-test the saved failed candidates whenever headers or the difftest change. The header fix turned 28 of 96 compile failures into passes; on the 123 failed pairs whose only errors were missing names, that is about 35 functions.
2. Before any repair, apply the tool fixes from Area 3, including B30 (try the 3 `SET_FLAGS_*` forms).
3. Allow one repair per model, and only for compile and behaviour failures. Then move to a fresh model. The outer cap is about 60k tokens per function across all models; past it, the function waits for the mechanical pass or the strong tier.
4. Repairs start from the best-ranked attempt so far. The ranking: passed > failed only on random inputs > fewer failing snapshots > compile error > style or register check > no code. Add a one-line history of the earlier failures. Handing the best attempt to the next model is a with/without benchmark arm.
5. Candidates that behave correctly but are still in register form go into the retype queue (Area 7).
6. A9: send verify drops back as one repair with the evidence, after review 1's oracle triage.

**Gain:**
- about 30% of farm tokens saved, for about 9% of passes, most of which the fresh model recovers;
- 35-100 functions free from step 1.

**Effort:** half a day for step 1; about a day for steps 2-4.

**First step:** a retest of the failed candidates over `tools/convert/log`.

**Review 1:** results go into its `pool.tsv` / `pool.py`. A9 waits for its oracle triage (A2).

## Area 3. Acceptance tests and failure reports

**Ideas:**
- A4: correct line numbers.
- A5 / B6: report every problem at once.
- A7 / B7: messages in game terms, and messages that say what to do.
- A12 / B24: tool-fixable rules.
- B40: robust code extraction.
- A3 / B8: pre-flight checks.

**Data:**
- `farm.test()` adds two `#include` lines before compiling, so compile line numbers are off by 2 in 683 of 753 reports. Review-rule line numbers are wrong in 49 of 84.
- 265 candidates were never compiled or difftested because an earlier check stopped them.
- 110 of 122 `nz` style failures are `cpu.nz` used after a `GUEST_CALL`.
- 65 rounds went to functions whose captured calls are all out of domain; none of them passed.
- "Return target differs" cost 148 rounds on 63 functions. These are mostly dispatchers using `mov lr, pc` / `bx rN`.
- 505 of 507 no-code replies hit the cap, so B40 is a correctness fix, not a token saver.

**Debate:** none of substance; A and B proposed the same order.

**Agreed approach:**
1. Fix both line-number bugs, and quote the offending line. Use `subsystem._blank_comments`.
2. Before testing, rewrite `(s16)LD16` to `LDS16` and remove `#ifndef` fallbacks with `drafts._without_fallbacks`. Then let the difftest judge.
3. Run every check and report everything together.
4. Pre-flight: skip functions whose captures are all out of domain; flag dispatchers for a hint or the strong tier.
5. Reports in game terms:
   - a symbol and struct field for each memory difference;
   - both return targets;
   - registers described relative to the inputs;
   - identical lines collapsed;
   - style and lint messages that show the line and the fix.
6. B40: accept ` ```cpp ` fences and concatenate multiple blocks.

**Gain:** compile errors are the largest repair class (793 rounds), and every round saved is a 5-12k-token call.

**Effort:** about a day for steps 1-4; step 5 is medium.

**First step:** the +2 line offset and the quoted line in `tools/convert/farm.py` `test()`.

**Review 1:** do its `g_cfg[256]` fix before touching `cport/host/difftest.c`. The symbolizer belongs in its shared parsing library.

## Area 4. Prompt content, and tools writing the boilerplate

**Ideas:**
- A11 / B9: generated callee helpers.
- A30 / B9: generated adapters.
- A22: harvest signatures from accepted candidates.
- B10: legal path-dependent flags in an adapter.
- A13 / B13: consistent prompt rules.
- A28 / B22: callee cards.
- A31 / B25: captured calls in the prompt.
- A10: the nearest already-converted twin as the example.
- B36: prompt ablation.
- B23: one exemplar in subsystem prompts.
- B34: several tiny functions per call.

**Data:**
- Hand-written helpers are 23% of typed candidates' text: a median of 4 per candidate, over 308 distinct callees.
- 12 of 44 hand-written `TestEventFlag` helpers got the flag wrong, the bug class of 67673ff.
- 40 compile errors come from passing an address to `GUEST_CALL` (indirect calls need `GUEST_CALL_ADDR`).
- 22 failures were adapters that weren't thin.
- 590 backlog functions have flag outputs.
- The conventions text tells models to write `(s16)LD16`, which a review rule then rejects. The `event_flags.c` reference file is itself register form.

**Debate:**
- Who writes the signature crossed over: A and B each accepted the other's version. They settled on the merged rule below.
- Callee cards: A wanted them in the prompt now; B wanted a benchmark arm first. Final: an arm in the first benchmark run, kept if not worse.
- Both dropped B34 and A22, because input is about 10% of cost.

**Agreed approach:**
1. Make the prompt rules consistent:
   - fix the `(s16)LD16` contradiction;
   - explain `GUEST_CALL_ADDR`;
   - say `ST8` is a statement;
   - explain constant initializers;
   - replace the register-form reference file;
   - make the repair wording accurate.

   This takes hours.
2. Callee helpers are generated from the lifter contracts into a header in the candidate build, and the prompt says "use these".
3. Adapter rule:
   - The tool prefills the `abi:` line, which is the signature spec. It uses `proposal_final` where one exists. For new conversions it fills the register part from the lifter contract.
   - The model adds types and names, or overrides them. It may drop an over-cautious output only with a `difftest.cfg` `compare=` line and a reason.
   - The tool checks the line: arguments must be among the inputs, results among the outputs, flags among the flag outputs. A line that fails comes back as a normal failure report.
   - `gen.py` writes the adapter.
   - B10's path-dependent flags use gen's flag return forms plus a `FlagResult` return.
4. Benchmark arms:
   - callee cards (ship if not worse);
   - captured calls with symbolized pointers;
   - the twin example;
   - no lifted C;
   - no worked example.
5. B23: one short typed exemplar in subsystem prompts, which are about 35 KB today.

**Gain:**
- the helper and adapter bug classes go away;
- about 20% shorter replies;
- fewer lint and style rounds.

**Effort:** step 1 small; steps 2-3 about 2 days (`gen.py` exists).

**First step:** the step-1 text fixes. Then generate helpers for the benchmark functions and compare them with the models' hand-written ones.

**Review 1:** it allows committed wrapper specs for only 31 callees, and Tier-2 `abi:` blocks only together with a body change. So the helpers stay generated build-time headers, and a candidate's own `abi:` line is written only at install, together with its body. The lint must allow the `GUEST_CALL`s inside the generated header. This builds on review 1's typify plan (one shared generator).

## Area 5. Routing, providers, quota use and throughput

**Ideas:**
- A15 / B15: one shared queue.
- A14 / B12 / B42: route by size and shape.
- B35: tier order, and removing `hyper/minimax-m3`.
- A16: MiniMax-M3 now.
- A17 / B38: a free and slow lane.
- B17 / B27: Z.ai glm-5.3 as the escalation tier.
- A23 / B14: a Hyper schedule.
- A33: try untested models.
- B44: one identity per model across providers.
- B29: concurrency per provider, and time to first byte.

**Data:**
- MiniMax: about 2.3M of about 1.7B tokens a month used.
- Z.ai: 49 calls in 3 days.
- Hyper: about two thirds of one daily credit lapsed. Today about $5 of about $12.6 was used before the reset.
- `first_up` only moves to another provider when one fails.
- `sources._lasts` treats a 402 as one hour.
- glm by size (old prompt):

| Instructions | Pass rate | Calls per pass |
|---|---|---|
| 41-60 | 15 of 38 | 8.3 |
| 61-120 | 2 of 14 | 29 |

- qwen reasons about 9-19k tokens at every size. glm on the typed prompt uses about 4.4k at 10 instructions or fewer.

**Debate:**
- B withdrew its qwen-first order.
- A withdrew raising the MiniMax pacer. PROCESS.md lines 48 and 286 say to leave pacer settings alone, and 2 slots already allow 6,000-13,000 M3 calls a day.
- Before Z.ai glm-5.3 leads the escalation tier, it needs the abort budget.

**Agreed approach:**
1. Today: run an M3 batch on untried captured functions of 60 instructions or fewer, with `quota.py` snapshots before and after. M3's logged 276-695 output tokens, with no reasoning count, may hide its thinking. Move the drafts to M3.
2. Fix 402 handling (down until 10:13) and remove `hyper/minimax-m3`.
3. One shared queue, read from review 1's `pool.tsv`:
   - one worker per provider: 2 for the paced ones, about 16 for Hyper;
   - routing by measured cost per pass and quota left until the reset;
   - this replaces `first_up`.
4. Routing:

| Function | Route |
|---|---|
| 30 instructions or fewer | M3, then glm-flash |
| 31-80 | qwen |
| over 80, or a dispatcher | strong tier: M3, then Z.ai glm-5.3 (with abort budget), then Step-5 |

   The Step-5 lane gets a 64k cap and runs only while OpenCode's paid models are down. B44: pass-rate statistics per model, quota per provider.
5. A Hyper schedule: start right after 10:13, pace to about 23 units an hour, and have the orchestrator check the balance in the evening.
6. A33: try the never-used models on the benchmark. B29: log the time to first byte, to separate pacer waiting from generation time.

**Gain:**
- up to 2-3x accepted functions per day at the same spend;
- 2 extra providers running beside Hyper.

Quote these as "up to" until the meter check and M3's typed pass rate are known.

**Effort:** steps 1-2 hours; step 3 about 2 days; step 5 half a day.

**First step:** the M3 batch with quota snapshots.

**Review 1:** the queue reads its pool ledger.

## Area 6. Measuring cost, and a standing benchmark

**Ideas:**
- A18 / B19 / B20: a provenance ledger in plan units.
- A35 / B18: log failed and aborted calls.
- A19 / B16: a standing benchmark.
- B21: scorecard headline numbers.

**Data:**
- The scorecard leaves out drafts (about 1,630 calls).
- About 15% of accepted candidates are dropped later.
- `scorecard.py` hard-codes 4 rounds and counts convert calls as if they were functions.
- About 630 HTTP 429/402 events and 210 model-error runs are invisible to it.
- The max-token experiment had 2-8 passes per cell, which is noise.

**Typed-prompt cost** (glm tokens over all rounds per accepted function; typed passes in brackets):

| Instructions | Old prompt | Typed prompt |
|---|---|---|
| 11-20 | 37k | 119k (3) |
| 21-30 | 58k | 181k (4) |
| 31-40 | 89k | 102k (12) |
| 41-60 | 224k | 196k (8) |
| 21-60 combined | 76k | 147k (about 2x) |

At 21-40 instructions and today's 40k cap, the typed prompt costs 79k against 62k, about 1.3x. A and B counted size differently at first; B's count included the `thumb_func_start` macro line. They agreed on one measure: assembly instruction lines.

**Agreed approach:**
1. One ledger row per call, failed and aborted calls included, with:
   - time;
   - the model that answered;
   - prompt hash;
   - test-environment hash;
   - cap and reasoning setting;
   - job kind;
   - outcome;
   - the function's final state.
2. Cost per **installed** function, per provider, in each plan's own units (Hyper is about 145k tokens per unit), drafts and later drops included. Fix the scorecard.
3. A benchmark of about 60 functions:
   - B's 16 experiment functions;
   - 16 small ones;
   - about 30 chosen by failure class.

   It counts tokens over all rounds per accepted function per band (10 or fewer, 11-20, 21-30, 31-40, 41-60), runs both prompts on the same functions with the same model, cap and headers, and allows one round plus one repair. Rerun it after every prompt, header or model change.
4. Scorecard headlines: first-round pass rate, and tokens per pass by round and size.

**Gain:** every routing and prompt decision above becomes measurable.

**Effort:** steps 1-2 about a day; the benchmark costs about 1-2M tokens per arm.

**First step:** add provenance fields to `log_round` and `llm.complete`'s usage record.

**Review 1:** shares the file family with `pool.tsv`. Its generated STATUS.md can show the ledger.

## Area 7. Fit with the mechanical typed-C plan, the backlog, and the farm / convert / drafts split

**Ideas:**
- A20 / B11 / B32: two stages, readable first and typed second.
- A21: a retype job.
- A29: callees first.
- A34 / B39: functions with several entry points.
- B28: subsystems built from farm candidates plus glue.
- B27: per-function repair inside a subsystem convert.
- A24 / B26: cheaper, more mechanical drafts.
- A25: a cheap model's pre-review.
- B37: a risk-ranked review digest.

**Data:**
- The backlog is 1,418-1,456 functions: about 811 with only simple register shapes, 590 with flag outputs, and about 100 using sp/lr/cpu. 800 have 10 instructions or fewer.
- 61% of the pool's call edges go to callees still in register form. The top 30 callees cover 21% of the edges.
- 521 entry labels sit in 189 shared code units; 25 candidates are held back for that reason.
- 31 farm groups were committed, against 4 model-written subsystems out of about 12 attempted. A whole-file ask costs 36-64k output tokens.

**Debate:**
- B first proposed typed-first at 30 instructions or fewer and register form first above that. A showed this was backwards, and B withdrew it.
- Final: typed stays the default, as the user chose; the benchmark's old-prompt arm runs; the user decides per band.
- Multi-entry units: B's split by size was accepted.

**Agreed approach:**
1. Backlog order:
   - review 1's mechanical typify takes the simple shapes;
   - the **retype job** takes the 590 with flags (after B10) and whatever typify leaves. Its prompt holds the verified readable body, the contract, the generated helpers and the prefilled `abi:` line, with no assembly or lifted C. Same difftest and register check.
   - the strong tier takes the roughly 100 that use sp/lr/cpu.
2. Order the work callees-first (review 1's B18), starting with PlaySoundEffect (84 callers), TestEventFlagFromImmediate (73), and so on.
3. Multi-entry units of 60 instructions or fewer are farmed together using `subsystem.py`'s per-function test; larger units go to convert.
4. Subsystems: farm each function first, then a strong model writes only the glue, with per-function repairs. Whole-file asks only for code the farm can't split.
5. Drafts: the interface and register-form fields come from tools; M3 writes the name and prose, batched per farm group.
6. Review: a risk-ranked digest built by tools. Claude still reads, but reads the risky functions first. A cheap-model pre-review only if the digest isn't enough.

**Gain:** about 1,000 retypes at about 20k tokens each, about 20M tokens (about 140 Hyper units, or under a day of M3), instead of about 1,400 conversions from scratch at 50-150k per pass.

**Effort:** about 2 days once the Area 4 generator exists.

**First step:** run the retype prompt on 30 backlog functions that typify can't handle, on M3 and glm, and score them.

**Review 1:** depends on its typify pilot, its contract-refining lint (A21), its signature blocks (A7), and the `leaves_farm_*.c` regroup. Do the regroup first, so retyped bodies land in their final files.

---

## Appendix: stage 1 idea lists

**Rounds until each agent had no more ideas:**
- A: 2 rounds. A1-A26, then A27-A35, then "no more ideas".
- B: 3 rounds. B1-B32, then B33-B42, then B43-B44 and "no more ideas".

### Agent A
- A1. Budget reasoning and abort runaways in the stream (37% of farm tokens).
- A2. One repair per model, and only for failure classes that repairs fix; then switch model.
- A3. Pre-flight: skip untestable (out-of-domain) functions; flag dispatchers (`mov lr,pc`, `bx rN`).
- A4. Fix compile line numbers (off by 2 in 683/753) and rule line numbers (49/84 wrong); quote the line.
- A5. Run every check and report all problems (265 candidates never compiled).
- A6. Keep candidates that behave correctly but are still register form, as fallback rows.
- A7. Difftest reports in game terms: symbol and field, both return targets, registers relative to inputs, repeats collapsed.
- A8. Re-test about 2,600 saved failed candidates when the environment changes (97 functions failed only on now-defined names).
- A9. Send verify drops back as one repair with the evidence.
- A10. The most similar converted function as the prompt example (1,739 unconverted functions have a twin).
- A11. Generate typed callee helpers from contracts (helpers are 23% of candidate text; 12/44 `TestEventFlag` helpers wrong).
- A12. A tool fixes mechanical rule breaks (`(s16)LD16`, `#ifndef` fallbacks).
- A13. Make the prompt rules consistent (`LD16` contradiction, `GUEST_CALL_ADDR`, register-form reference file, repair wording).
- A14. Route by size and shape (glm pass rate 83% at 10 instructions or fewer, falling to 0% above 80).
- A15. One shared queue that every provider pulls from all the time.
- A16. MiniMax-M3 to work now; test its reasoning mode; drafts to M3.
- A17. Free or slow sources (Step-5, Z.ai glm-5.3) on the hard leftovers.
- A18. A cost ledger in plan units, with provenance on every row.
- A19. A standing benchmark of 60-80 functions.
- A20. Per size band, decide between typed-first and register form then typify.
- A21. A cheap retype job for what the mechanical pass leaves.
- A22. Harvest `abi:` signatures from accepted typed candidates.
- A23. Spend Hyper's credit on a schedule; a 402 means down until the reset.
- A24. Drafts on the cheapest source, batched per farm group.
- A25. A cheap model's pre-review to shrink Claude's review.
- A26. Parallel first attempts on near-free sources.
- A27. Repair from the best candidate so far (41% of repairs regressed).
- A28. Callee cards from symbols/db (62% of call edges have an entry).
- A29. Type the most-called callees first (top 30 cover 21% of edges).
- A30. The model writes typed C plus an `abi:` spec; `gen.py` writes the adapter.
- A31. Show 2-3 real captured calls with symbolized pointers.
- A32. Keep and read the reasoning text of runaways.
- A33. Benchmark the 10 plan models that have never been called.
- A34. Send multi-entry functions to convert (521 labels).
- A35. Log failed and aborted calls in the cost ledger.

**Checked and dropped by A:**
- duplicate work across models (1.2M tokens);
- keeping the passing parts of a failed subsystem (no cases);
- parse losses (2 of 507);
- provider errors (cost no tokens).

### Agent B
- B1. Abort runaways mid-stream (45% of glm output).
- B2. An experiment with reasoning-effort settings.
- B3. Never resend an identical prompt after a runaway.
- B4. One repair round for cheap models, then a fresh model.
- B5. The repair prompt remembers earlier rounds (23 functions oscillated).
- B6. Report every static problem at once.
- B7. Failure messages that say where and what to write instead.
- B8. Pre-flight the captured calls (66 calls on hopeless functions).
- B9. Generate the adapter and callee helpers; the model writes the typed body only.
- B10. A legal way to set path-dependent flags in an adapter (590 backlog functions).
- B11. Two stages: readable register form first, typify second.
- B12. A size gate for cheap models (61-120 instructions: 14% pass).
- B13. Three prompt lines for recurring compile errors.
- B14. Keep Hyper's daily credit spent (two thirds of one day lapsed).
- B15. Run every quota pool at once, not just the first one up.
- B16. A standing calibration set.
- B17. Z.ai glm-5.3 as the farm's escalation tier.
- B18. Log provider failures where the scorecard sees them (210 model-error runs).
- B19. Cost per installed function, in plan units, per provider (drafts and drops included).
- B20. Log the prompt version, the cap and the reply tail.
- B21. Scorecard headlines: first-round pass rate, tokens per pass by round and size.
- B22. Show the callees' contracts and one-line summaries.
- B23. One short exemplar instead of two whole reference files in subsystem prompts.
- B24. Apply the tool-fixable review rules instead of rejecting.
- B25. Captured inputs, symbolized, in the prompt.
- B26. Mechanical interface and register-form fields in drafts.
- B27. Strong tier with M3 first, and per-function repairs in subsystems.
- B28. Subsystems from farm candidates plus model-written glue.
- B29. Concurrency per provider; log time to first byte.
- B30. Salvage flag variants without a model.
- B31. Hand the best failing candidate to the next model.
- B32. Fixed order for the backlog: mechanical typify, then retype prompt, then strong model.
- B33. Sampling parameters as an experiment arm.
- B34. Several tiny functions per call.
- B35. Order the cheap tier by passes per million output tokens; drop `hyper/minimax-m3`. (The order was withdrawn in debate; the drop stayed.)
- B36. Prompt ablation on the calibration set (lifted C, worked example).
- B37. A risk-ranked review digest for farm groups.
- B38. A free-preview lane with its own rules (Step-5).
- B39. Farm a function's other entry points together.
- B40. Robust code extraction.
- B41. A reply cache in `llm.py`.
- B42. Difficulty signals beyond size for routing.
- B43. A token budget per function across models and rounds.
- B44. One identity per model across providers.

**Checked and dropped by B:** paid calls that never reached a log row (1 in 5,801).
