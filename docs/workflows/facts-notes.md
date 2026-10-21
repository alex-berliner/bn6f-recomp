# `wf facts` — the runtime-facts layer: the output format

`wf facts` records, for **every lifted function**, what the ORIGINAL game actually did at
runtime, from the recordings alone. No model is called anywhere in it: a function's name is a
field of its record, never its key, and the later steps (a claim checker, an mGBA
intervention runner) are what decide what to do with what this writes.

    python -m wf facts [--only NAME]... [--states K=8] [--jobs J=6] [--limit N]
                       # WF_FACTS_DIR overrides where it is written (default build/facts,
                       # in the tree this tools/wf lives in)

Everything it writes is regenerable and git-ignored (`build/`), and is rebuilt per function
only when its inputs moved (below).

| Path | What |
|---|---|
| `build/facts/by_address/0x08XXXXXX.json` | one record per function, keyed by **entry address** (102 of them are IWRAM units and read `0x0300XXXX.json`) |
| `build/facts/coverage.tsv` | one row per function, for `sort`, `join` and eyeballing |
| `build/facts/progress.txt` | one appended line per milestone: when the run started, every 25 replays, every 5 minutes, and what it ended with |

## The record (`by_address/0x08XXXXXX.json`)

```jsonc
{
  "address": 134222754,               // the entry address, the key everything is filed under
  "address_hex": "0x080013A2",        // the file name, and what to grep for
  "name": "battle_SetStateUnk5c_80013A2",   // the name today; it changes on a rename, the key does not
  "end": 134222762,                   // the unit's [entry, end) from the lifted header
  "asm": "asm00_0.s:2631",           // where bn6f defines it

  "reached": true,                    // did any recorded route execute it?
  "captured": true,                   // does build-snapshots/NAME/ hold real calls of it?
  "captured_states": 8,               // how many entry snapshots the capture holds
  "states": 8,                        // how many were replayed this run (--states K)
  "probes": 8, "returned": 8,         // and how many of them the original came back from

  "routes": [                         // one row per recording under tools/traces/coverage
    {"route": "battle", "executed": true,        // the entry address is in battle.prof
     "body_executed": true,                      // some address of its range is
     "entry_cycles": 2,                          // CYCLES the profile attributes to the entry PC
     "function_cycles": 15,                      // and to the whole [entry, end) range
     "body_instructions": 4,                     // distinct PCs of it the profile lists
     "frames": 20000,                            // the route's length, read from wf/routes.py
     "frames_from": "wf/routes.py",              // where that length came from
     "calls": null,                              // calls through the recorded indirect sites
     "calls_per_1000_frames": null,
     "callers": []},
    {"route": "beastsp", "executed": true, "entry_cycles": 118, "function_cycles": 963,
     "frames": 432312, "frames_from": "wf/routes.py",
     "calls": 42, "calls_per_1000_frames": 0.0971,
     "callers": [{"site": "0x03005C68",      // the call instruction
                  "caller": "0x0805A1B0",     // the function holding it ("" when no unit does)
                  "name": "runScript_805A1B0", "count": 42}]}
  ],

  "calls": 42,                         // over every route
  "calls_per_1000_frames": 0.0971,     // calls / the routes' frames, so a short and a long route compare
  "cycles": 963,                       // over every route
  "routes_reached": 17,

  "arguments": [                       // what the replayed calls were given (r0..r3, sp)
    {"reg": "r0", "samples": 8, "distinct": 1, "lowest": 12, "highest": 12, "values": [12]}
  ],
  "returns": [                         // what they returned (r0 at the exit)
    {"reg": "r0", "samples": 8, "distinct": 1, "lowest": 12, "highest": 12, "values": [12]}
  ],

  "facts": { /* RuntimeFacts, see below */ },     // null: no captures, or the replay gave nothing
  "facts_error": null,                             // why there are no facts, when there are none
  "inputs": { /* the cache key, see below */ },
  "built": "2026-10-21T11:16:00Z"
}
```

### Where each field comes from, and what it does not say

* **`routes[].executed` / `body_executed`** come from the route's `.prof`. `body_executed`
  covers a function entered in the middle (a jump table), `reached` is either.
* **`entry_cycles` / `function_cycles` are CYCLES, not call counts.** `tools/mgba_run
  --profile-game` writes one line per executed PC halfword with the cycles spent there
  (`tools/import_route.py` merges the windows). So these say how long the function ran on
  that route, never how often it was called.
* **`calls` and `callers` come from the route's `.edges`**, which hold real execution
  counts — but only at the **indirect call and jump sites** of the ABI export
  (`tools/abi/call_edges.py`). A function reached only by a direct `bl` has `calls: null`,
  and its callers are not listed anywhere here. `calls: null` means "not observed", never
  "not called".
* **`frames`** is the route's length from `tools/wf/wf/routes.py`, read when the run starts.
  `beastsp` grows as the vidsync recording is imported, so no length is written down here;
  when a route has no entry there (the union sweeps `chips`, `fuzz`, `maps`,
  `battlesweep`) the profile's own header span is used and `frames_from` says so.
* **`arguments` / `returns`** count only the probes that returned (the rest have no exit).
  Register values are unsigned 32-bit as the snapshot header holds them.

### `facts` — the RuntimeFacts block

The same block `wf runtime-evidence` builds with `wf/tools/runtime_evidence.py`'s
`summarise`, so a later step can read either: `probes`, `returned`, `register_inputs`,
`register_outputs`, `registers_written`, `written_patterns`, `io_writes` (name, address,
size, value, how many probes made it), `memory_writes` (grouped by the bn6f RAM symbol that
contains them: bytes and distinct values) and `route_coverage`, which this layer fills from
its own index instead of re-reading the `.prof` files per function.

Its blind spots are the original's: only memory and registers whose value **changed** are
listed (a write of the value already there, and every read, are invisible), and write-only
hardware registers read back as junk, so an empty `io_writes` does not argue against a name
about display, scrolling, DMA or sound.

## `coverage.tsv`

One row per function, tab separated, with a `#` header:

```
# address  name  reached  routes  calls/1k  captured  facts  error
0x080013A2 battle_SetStateUnk5c_80013A2  yes  battle,battles1,beastsp,menus2  0  8  yes  -
```

* `routes` — the recorded routes that reached it, in reading order (`-` for none)
* `calls/1k` — calls through the recorded indirect sites per 1000 frames, over every route
* `captured` — how many captured entry states exist
* `facts` — `yes` when the replay produced runtime facts
* `error` — the `facts_error`, or `-`

## The cache (`inputs`)

A function is not rebuilt when its inputs are unchanged, so re-running after a new capture,
a new recording or a new `mgba_run` only does the work that changed:

```jsonc
"inputs": {
  "mgba_run": "<sha1> <mtime> <size>",   // the runner that did the replay
  "rom": "<sha1> <mtime> <size>",        // bn6f.gba
  "snapshots": "<newest mtime> <count>", // its capture: build-snapshots/NAME/
  "states": 8,
  "coverage": "40dfbaf82f1133de"         // a hash over every .prof/.edges mtime
}
```

The two halves are separate on purpose. When only `coverage` moves (a route grew — `beastsp`
is the one that will), the record's coverage is re-read and its facts are **kept**: no
mGBA runs at all. When `snapshots`, `states`, `mgba_run` or `rom` move, the function goes
back through the replay.

## Failure is a record, not a crash

`facts_error` says why a function has no facts, and the run carries on:

* `state N did not return within 5000000 instructions` — the original did not come back from
  that entry state within the step budget (`MAX_STEPS` in `wf/tools/facts.py`); a function
  that runs a whole frame (a map loader) needs a big budget, and one that never returns on
  any of them ends here
* `mgba_run did not run (...)` — the runner is missing or could not be started
* anything else the replay raised, as `TypeName: message`

Functions with no captured calls at all have `captured: false`, `facts: null` and **no**
`facts_error`: there was nothing to replay, which is not a failure.

## Where the output goes

`build/facts`, in the tree the `wf` package lives in (so a worktree writes into its own
`build/`), or `$WF_FACTS_DIR`. The inputs come from the main checkout regardless
(`wf/repo.py`): `tools/traces/coverage` for the recordings, `build-snapshots` for the
captures, `cport/src/lifted` for the function set, `bn6f/bn6f.gba` and
`build-tools/mgba_run` for the replays.

## Notes for the next steps

* **Key on the address, not the name.** Renames change `name`, never `address_hex`.
* A `reached: false` function is one the recordings say nothing about — the weakest kind of
  silence, and not evidence for or against a name.
* `calls: null` means "not seen at an indirect site", not "never called".
* `entry_cycles`/`function_cycles` rank functions by time on a route; they are not counts.