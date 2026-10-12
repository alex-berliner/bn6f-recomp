# Knowledge first, then whole subsystems (2026-10-12)

The user approved this on 2026-10-12 ("Do it!"). It replaces most of the joint plan's S3/S4
(docs/reviews/2026-10-10-4-joint-plan.md) and the later "remove the register layer" stage.

## Why

Most rework in today's path comes from missing information, not from the conversion:
- functions go in as register-form C (plan B), get retyped later, then lose their register adapter;
- each pass exists because the signature, argument types and struct layouts were unknown the first time;
- even typed functions keep a `Next NAME(void)` adapter, and removing those is a whole-call-graph change.

Much of that information already exists or can be derived mechanically:
- the lifter's contracts (inputs, outputs), fixed on 10-12 for registers a function restores and reads back;
- about 20,000 captured calls, which show what each register holds;
- bn6f's structs and enums;
- the call graph, which shows the subsystem boundaries.

With signatures, types and named structs in the prompt, a model can write idiomatic C in one pass. Inside a
subsystem the functions call each other directly; only its entry points keep register adapters.

## What changes

1. **Plan B ends.** The farm queue routes every size to the typed prompt (`work_queue.ROUTES`). Open
   old-prompt jobs are retired. The register-form functions already installed stay on
   `tools/abi/retype_queue.tsv`; their subsystem's pass redoes them.
2. **The knowledge layer.** A proposed signature for every function, in symbols/db:
   - parameters and return value with types;
   - which register each one comes from;
   - the struct each pointer points to;
   - evidence: contract, captured-call values, bn6f structs.

   It is derived mechanically first. Outside models fill gaps, and Claude spot-checks.
   Subsystems come from the call graph plus file layout. Each one has a list of entry points (called from
   outside) and internal functions.
3. **Pilot one mid-sized subsystem end to end** (for example chatbox or event flags):
   - finish its signatures and types;
   - convert it in one go to idiomatic C with direct internal calls and typed structs;
   - keep register adapters only at the entry points;
   - verify at the boundary (difftest, oracle, coverage inputs) and in lockstep (16 routes plus beastsp).
4. **Compare the pilot with today's path:**
   - outside tokens and Claude tokens per function;
   - adapters and register uses left;
   - bugs found.

   If it wins, the farm moves to subsystems: its unit of work becomes a subsystem with its signatures.

## What stays

- Every check: difftest, CPU oracle, coverage inputs, lockstep, gate. The byte-identical ROM.
- Typed farm conversion of leaf functions continues until the pilot decides.
- The outside models write; Claude orchestrates and reviews, and pre-review by outside models
  (`tools/convert/prereview.py`) cuts Claude's reading.
