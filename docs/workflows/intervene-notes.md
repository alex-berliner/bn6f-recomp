# intervene

Causal checks of function names. A name like `drawHpBar` is a claim: *this function
updates the HP digits' OAM entries each frame*. `wf investigate` asks a model what
the code looks like it does. `wf intervene` does not ask — it breaks the function on
purpose and watches what the machine does instead.

Nothing is renamed and nothing is installed. The output is a table plus one JSON per
run; the orchestrator reads the table and takes what holds up through `record`.

Runs are tracked in the Burr UI under project `recomp-intervene` (and
`recomp-intervene-pilot` for the pilot).

## Usage

```bash
python -m wf intervene --function NAME_OR_ADDR [--intervention skip|return:K|null] \
                       [--route R | --route auto] [--frames N]
python -m wf intervene-pilot [--route R] [--frames N] [--random N]
```

`--route auto` (the default) picks the shortest recorded route that calls the
function. Interventions are ROM patching — no emulator C changes, no savestate
editing, nothing under `bn6f/` or `symbols/db` touched.

## What a run does

### 1. Resolve the function

`find_function` maps a name or address onto bn6f.elf and **refuses** anything it
cannot patch honestly:

| refusal | why |
|---|---|
| ARM mode (`mode == "a"`) | the patch encodings are THUMB (`bx lr`, `movs r0,#k`) |
| RAM-resident (`0x02…`/`0x03…`) | there is no ROM byte to patch; `patch_rom` has no offset |
| shorter than 4 bytes | a 4-byte `return:K` patch would not fit |

A name with duplicates (ELF aliases, or a static name reused per translation unit)
resolves to the **most-called** address, since that is the one the game is actually
dispatching to.

### 2. Find the first frame it executes

`first_game_frame` bisects over game frames with `mgba_run --count-entry`, which
counts how many times the CPU entered the address within the window. Bisecting on
"has the count gone from 0 to non-zero" yields the first frame the route calls the
function; ~log2(window) emulator runs, ~13 s at the default 1200-frame window.

**A `None` here means "not within the window", never "never".** The window is the
`frames_window` argument (default 1200 ≈ 20 s of play); it is deliberately wider than
the observation window, because most of a route's interesting behaviour happens well
before the point where you would want to start *watching* it.

### 3. Cache an emulator state just before it

The state is saved at `first_frame - lead_in_frames` (default 4, clamped to ≥ 1 —
`mgba_run --until-game-frame 0` exits immediately). Both the baseline and the patched
ROM then load *the same machine state* and replay *the same trace*, so the only
variable is the patch. States are cached per `(route, frame)` under
`build/intervene/states/`, so two functions that first execute on the same frame
share one save.

### 4. Patch and observe

The patched ROM is a full copy written under `build/intervene/patched-roms/`; the
original is never touched. Both ROMs run in **one** `mgba_run` invocation each,
carrying every observation flag at once so the two logs are frame-aligned by
construction:

| flag | feeds |
|---|---|
| `--digest-every 1` | the memory anchors (`oam`, `palette`, `vram_bg`, `vram_obj`), one hash per frame |
| `--watch 0x04000000 0xe0` | the register-set anchors (`display_io`, `dma_control`, `sound_io`), raw bytes per frame |
| `--shots … --every k` | the `screen` anchor, PPM frames sampled every k-th game frame |

The diff is then per-anchor and per-frame: memory anchors compare hashes (a VRAM
block hash so a 64 KB region is one token per frame), register anchors compare raw
bytes, and `screen` compares raw pixel bytes.

### 5. Validate the start point

**Every verdict is gated on a `null` run.** `null` rewrites the function's entry with
*its own first instructions*, producing a byte-identical ROM; if that run shows any
anchor difference, the route/window is nondeterministic and no `skip`/`return:K`
verdict from this window means anything. The null row is written next to the real row
in `report.tsv` so a reader can see the gate, not take it on faith.

## Interventions

| kind | bytes written | meaning |
|---|---|---|
| `skip` | `00 47` (`bx lr`) | the function returns immediately |
| `return:K` | `K 00 20 70 47` (`movs r0,#K; bx lr`) | the function returns the constant K — separates "the call matters" from "the *return value* matters" |
| `null` | the function's own first bytes | determinism control; byte-identical ROM |

`skip` alone cannot tell you whether a function matters because of its writes or
because of what its caller does with the result. `return:K` with K=0 keeps the
control flow intact while zeroing the result, which is how you separate the two.

## Reading `hung`

`hung` is **not** a measurement failure. It means the run stopped making game
frames: mgba_run exhausted its hardware-frame budget without ever printing
`reached game frame N`, which is what a wedged CPU does (an infinite loop, or
execution that has fallen out of THUMB into ARM). Skipping a function the game calls
on a hot path *can and does* wedge it — and that is the finding.

`deterministic` is a property of the **route/window**, so it is gated on the
*baseline* alone. A hung patched run leaves `deterministic` intact and sets `hung`.

## Predicates

A small typed predicate language reads a `RunRecord` and returns
`True` / `False` / `None` (indeterminate — e.g. asked about an anchor that was not
observed). Each is a frozen dataclass with an `evaluate(run)` method, and
`make_predicate(spec)` parses the DSL string form:

| spec | true when |
|---|---|
| `differs(anchor=A, within=N)` | anchor A differs in some frame ≤ N |
| `unchanged(anchor=A, for=N)` | anchor A shows no difference in the first N frames |
| `oam_entries_missing(min=K, first_after=F)` | ≥ K OAM bytes differ, first at frame ≥ F; `None` if OAM wasn't observed |
| `screen_differs(region=x,y,w,h)` | the screen anchor changed |
| `sound_write_missing(reg=R)` | the named sound register does **not** differ (the write is missing) |
| `ram_anchor(name=N) baseline_minus_patched >= K` | the RAM anchor's difference magnitude clears the threshold |

The point of `None` is that a predicate must not silently pass on an anchor the run
never watched.

## Outputs

```
build-wf-runs/intervene-RUN_ID/
  report.tsv                      one row per (function, intervention) + its null row
  FUNCTION/result.json            the full RunRecord: per-anchor diffs, notes, hashes
build-wf-runs/intervene-pilot-RUN_ID/
  pilot.tsv                       24 functions × {skip, null}
  FUNCTION_KIND_ADDR_ROUTE/
    result.json
    baseline/{log.txt,shots/}     mgba_run stdout for the unpatched ROM
    patched/{log.txt,shots/}      mgba_run stdout for the patched ROM
build/intervene/patched-roms/     the patched ROM copies (inputs, not results)
build/intervene/states/           cached mgba_run states, keyed (route, frame)
```

## Pilot

`wf intervene-pilot` runs 12 named functions plus 12 drawn at random from the
keyword-matching lifted set (drawing/OAM/palette/fade/sound/sprite/battle/chip),
each as `skip` **and** `null` — 48 emulator runs. The random tail is filtered
through `find_function` at selection time, because the lifted unit names are a
superset of the ELF symbols and a name that cannot be patched can only ever produce
an `unsupported` row. Trials are independent subprocesses and fan out across a
thread pool (8 workers); results are re-ordered to the selection order so
`pilot.tsv` is byte-identical regardless of completion order.

### Pilot run (2026-10-21, `battles1`, 60 frames, 1200-frame bisect window)

24 functions × {`skip`, `null`}; every `null` was deterministic, so every `skip`
verdict below stands.

| function | source | first frame | `skip` result | first anchor diff |
|---|---|---|---|---|
| `call_m4aSoundMain` | named | 1 | **hung** (43 anchor frames differ) | `display_io@2` |
| `SetScreenFade` | named | 1 | **hung** (43) | `display_io@2` |
| `IsScreenFadeActive` | named | 1 | **hung** (43) | `display_io@2` |
| `LoadGFXAnims` | named | 233 | **hung** (27) | `screen@18` |
| `LoadGFXAnim` | named | 233 | **hung** (27) | `screen@18` |
| `startScreen_AnimatePressStart_803E938` | random | 1 | **hung** (43) | `display_io@2` |
| `palette_copyTableRow_80AA104` | named | 1 | **no observable effect** (0) | — |
| `PlaySoundEffect`, `PlayMusic`, `LoadBGAnimData`, `LoadBGAnimForMapGroup`, `screenFade_80062C8`, `owPlayer_setPalette_809e2a0` | named | — | not reached in 1200 frames | — |
| 11 further random functions | random | — | not reached in 1200 frames | — |

What the pilot actually established:

- **The window is sound.** All 7 executed functions had `deterministic=True` null
  runs with 0 anchor differences over 60 frames. The verdicts are real. (The other 17
  null rows also show 0 differences, but trivially: the function never ran, so there was
  nothing to differ.)
- **Six functions are load-bearing.** Skipping any of them wedges the game within a
  second. `call_m4aSoundMain`'s patched run ends at `pc=0x080005ae` with
  `cpsr=0x2000003f` — the T bit clear, i.e. the game is executing ARM garbage two
  halfwords into the function we just stubbed out. (A seventh function, below, does not.)
- **The three `first_frame=1` named cases share one cached state**
  (`battles1_000001.state`), so their identical diffs are the same divergence, not
  three coincidences. `startScreen_AnimatePressStart_803E938` lands on that same state
  and the same diff for the same reason.
- **`palette_copyTableRow_80AA104` is a genuine no-op on this route.** Its `skip` run
  reaches the same frame with the same register state as the baseline. Its callers
  ignore the return value *and* nothing it writes survives to an anchor within 60
  frames. This is the kind of result the tool exists to produce: a name that promises
  palette work with no visible consequence here.
- **17 of 24 were never reached** inside 1200 frames of a battle route. That is the
  honest limit of one route — a function that matters on the overworld says nothing
  here, and a `None` row is not a negative result.

Two findings worth carrying into `record`/the review: the sound driver and the fade
helpers are hot-path-critical, and the 17 unknowns want a route they actually appear
on (`--route auto` will pick one, or run the pilot against `wander`/`beastsp`).

### `skip` and `return:K` are not interchangeable

The pilot runs `skip` because it is the blunt instrument, but `skip` answers two
questions at once and cannot separate them. Same function, same route, same window:

| intervention | hung | differing anchor frames | first anchor diff |
|---|---|---|---|
| `skip` (`bx lr`) | **yes** | 43 (then the window ends) | `display_io@2` |
| `return:1` (`movs r0,#1; bx lr`) | no | **616** | `display_io@2` |
| `null` | no | 0 | — |

`skip` destroys the return value, and the game then wedges within a second — the
window closes before most of the effect can be observed. `return:1` keeps the control
flow intact with a *wrong but well-formed* answer, and the run survives all 60 frames
while diverging from the baseline in 616 anchor-frames.

So "hung" on a `skip` row is an **under-report**, not a ceiling: the number of
differing frames is bounded by how long the game stayed alive. `first_anchor_diff` is
the honest field on a hung row; on a non-hung row, `differing_frames` is the real
magnitude. `return:K` with a plausible K is the follow-up to run on anything that
hangs, and `return:0` is the specific test for "does the *caller* care about the
result, or only that the call happened".

## Limits

- **One route, one window.** A `None` (not reached) row means "not on this route in
  this window"; it is never evidence that the function is dead.
- **Anchor-limited.** A function can do real work that none of the eight anchors
  observe and still report zero differences. `palette_copyTableRow` may be exactly
  that; `return:K` is the follow-up that would tell.
- **`ram:` in `symbols/anchors.yaml` is empty by policy.** A RAM anchor is only
  trustworthy once a `null` run has shown the window is deterministic; until one has,
  a RAM difference is indistinguishable from emulator noise. Candidates go in the
  file's `#ram-pending` block and move up only with a passing null.
- **A hang masks what came before it.** The diffs up to the wedge are real, but the
  window after it is empty, so `differing_frames` under-reports. Read
  `first_anchor_diff` on a hung row, and follow a hang with a `return:K` run (see
  above) to recover the magnitude.