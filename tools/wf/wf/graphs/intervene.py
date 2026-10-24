"""Causal checks of function names: break a function on purpose and watch what the machine does.

A name like `drawHpBar` is a claim about what a function does. `wf investigate` asks a
model to read the claim off the assembly; this workflow does not ask. It patches the ROM
(skip the function, return a constant, or rewrite the entry with its own bytes as a
determinism control), replays the original and the patched ROM from the *same* cached
emulator state on the *same* recorded trace, and diffs the machine's visible state every
game frame: OAM, palettes, VRAM, the display and sound I/O registers, and the screen
itself. Resolve the function in bn6f.elf (refusing ARM, RAM-resident and too-short
targets it cannot patch honestly), bisect mgba_run for the first frame the route calls
it, cache a state just before that frame, then patch and observe. Every verdict is gated
on a `null` run: it must show zero differences, or the route/window is nondeterministic
and the run proves nothing. `wf intervene` does one function; `wf intervene-pilot` runs
24 named plus 24 random reached functions as null + skip + return:0 + return:1 and writes
one table. Nothing under symbols/db or bn6f is touched; the orchestrator (`wf record`)
applies what a human accepts from the table.

  python -m wf intervene --function NAME_OR_ADDR --intervention skip|null|return:K
                         [--route R|auto] [--frames N]
  python -m wf intervene-pilot [--route R|auto] [--frames N] [--random N] [--jobs J]

`--route auto` reads the facts layer (build/facts/by_address/0xADDR.json), which records
for every function which routes execute it, and takes the shortest one. The first-frame
search then covers the whole route from cached states, so a function that first runs 900
frames in is found instead of being reported "not reached".

Interventions are ROM patching only — no emulator C changes, no savestate editing.
See docs/workflows/intervene-notes.md for the anchors, the predicate language, the
hang semantics and the pilot's results.
"""
from __future__ import annotations

import time
from pathlib import Path

from pydantic import BaseModel, field_validator

from wf.burr_typed import Finished, Graph, Transition, Workflow
from wf.repo import REPO
from wf.tools import intervene as it
from wf.tools.intervene import (DIR, DEFAULT_FRAMES, PILOT_NAMED, PROJECT, RunRecord, find_function,
                                first_game_frame, load_anchors, pick_route, run_intervene_one,
                                save_state_at, select_intervene_targets)
from wf.types import (FrameCount, FunctionName, GuestAddress, Result, Route, RouteName, ToolMessage)


def intervene_graph() -> Graph["InterveneState"]:
    return workflow.graph(
        first=resolve_function,
        ends=[write_artifacts],
        transitions=[
            Transition(after=resolve_function, go_to=find_and_cache),
            Transition(after=find_and_cache, go_to=run_intervention, when=found_a_frame),
            Transition(after=find_and_cache, go_to=write_artifacts, when=no_execution_found),
            Transition(after=run_intervention, go_to=validate_null, when=is_a_real_intervention),
            Transition(after=run_intervention, go_to=write_artifacts, when=is_the_null_run),
            Transition(after=validate_null, go_to=write_artifacts),
        ],
    )


def run_intervene(start: "InterveneState", run_id: str, tracked: bool = True) -> "InterveneState":
    outcome = intervene_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished):
        raise RuntimeError(f"intervene run {run_id} did not finish")
    return outcome.state


# ── the run's state ────────────────────────────────────────────────────

class InterveneState(BaseModel):
    run_dir: Path
    function_name: FunctionName
    address: GuestAddress | None = None
    # 'skip' | 'return' | 'null'. A validator canonicalises 'return:K' to 'return':
    # the graph's branches and run_intervene_one() both switch on the kind alone and
    # carry K separately, so a 'return:K' kind matches neither branch (TransitionError:
    # "0 of these held"). K lives in intervention_k.
    intervention: str = "skip"
    intervention_k: int | None = None
    route: RouteName = "wander"
    frames: int = DEFAULT_FRAMES
    lead_in_frames: int = 4
    notes: tuple[ToolMessage, ...] = ()
    function_size: int | None = None
    record: RunRecord | None = None
    null_record: RunRecord | None = None
    deterministic: bool = True
    wall_seconds: float = 0.0

    @field_validator("intervention")
    @classmethod
    def _canonical_intervention(cls, value: str) -> str:
        return it.InterventionKind.canonical(value)


workflow = Workflow[InterveneState](InterveneState, project=PROJECT)


# ── resolve the function ──────────────────────────────────────────────

class ResolveInput(Result):
    function_name: FunctionName
    notes: tuple[ToolMessage, ...] = ()


class ResolveOutput(Result):
    address: GuestAddress
    function_size: int
    notes: tuple[ToolMessage, ...]


@workflow.step
def resolve_function(given: ResolveInput) -> ResolveOutput:
    """Name/address -> bn6f ELF; refuse ARM/RAM/short functions (brief: skip as `unsupported`)."""
    try:
        finfo = find_function(given.function_name)
    except ValueError as err:
        # The graph still produces an end state; the downstream steps surface the error.
        return ResolveOutput(address=0x00000000, function_size=0,
                              notes=given.notes + (ToolMessage(str(err)),))
    return ResolveOutput(address=finfo.address, function_size=finfo.size,
                          notes=given.notes + (ToolMessage(
                              f"{finfo.name} at {finfo.address:#010x}, {finfo.size} bytes"),))


# ── bisect first game frame, cache emulator state at (frame - lead) ───

class FindCacheInput(Result):
    function_name: FunctionName
    route: RouteName
    frames: int
    lead_in_frames: int
    notes: tuple[ToolMessage, ...]


class FindCacheOutput(Result):
    notes: tuple[ToolMessage, ...]
    address: GuestAddress


@workflow.step
def find_and_cache(given: FindCacheInput) -> FindCacheOutput:
    """Bisect mgba_run --count-entry on the route to find the first game frame the
    function executes; save an mGBA state at (frame - lead_in) so the patched ROM
    loads from the same machine state baseline does."""
    import gamepaths  # noqa: PLC0415
    mgba_run = Path(gamepaths.MGBA_RUN)
    bios = Path(gamepaths.GBA_BIOS)
    from wf.routes import route_named
    route = route_named(given.route)
    try:
        finfo = find_function(given.function_name, on=route)
    except ValueError as err:
        return FindCacheOutput(notes=given.notes + (ToolMessage(str(err)),),
                                address=0x00000000)
    notes = list(given.notes)
    try:
        ff = first_game_frame(finfo, route, lead_in_frames=given.lead_in_frames,
                              mgba_run=mgba_run, bios=bios)
    except Exception as err:                                                            # noqa: BLE001
        return FindCacheOutput(notes=notes + (ToolMessage(f"bisect failed: {err}"),),
                                address=finfo.address)
    if ff is None:
        notes.append(ToolMessage(f"{finfo.name} never executes on {route.name} "
                                  "in the bisect window; nothing to do"))
        return FindCacheOutput(notes=tuple(notes), address=finfo.address)
    start = max(1, ff - given.lead_in_frames)
    # Snap down to the cached grid (the search's own states): saving a state at an
    # arbitrary frame means replaying the route from its first frame, which on `beastsp`
    # is fifteen minutes for a state the grid already has.
    snapped = it.nearest_cached_state(route, start)
    if snapped is not None:
        start = snapped
    state_path = it.state_path_at(route, start)
    try:
        save_state_at(route, at_frame=start, mgba_run=mgba_run, bios=bios, out=state_path)
    except Exception as err:                                                            # noqa: BLE001
        notes.append(ToolMessage(f"save_state_at {start} failed: {err}"))
        return FindCacheOutput(notes=tuple(notes), address=finfo.address)
    notes.append(ToolMessage(f"first game frame {finfo.name} executes: {ff}; "
                              f"cached state at {start} -> {state_path}"))
    return FindCacheOutput(notes=tuple(notes), address=finfo.address)


# ── run the intervention on both ROMs from the cached state ───────────

class RunInput(Result):
    run_dir: Path
    function_name: FunctionName
    address: GuestAddress
    function_size: int
    intervention: str
    intervention_k: int | None
    route: RouteName
    frames: int
    lead_in_frames: int
    notes: tuple[ToolMessage, ...]


class RunOutput(Result):
    record: RunRecord | None
    notes: tuple[ToolMessage, ...]
    wall_seconds: float


@workflow.step
def run_intervention(given: RunInput) -> RunOutput:
    """Patch the ROM, replay baseline + patched from the cached state, observe anchors."""
    import gamepaths  # noqa: PLC0415
    mgba_run = Path(gamepaths.MGBA_RUN)
    bios = Path(gamepaths.GBA_BIOS)
    from wf.routes import route_named
    route = route_named(given.route)
    finfo = find_function(given.function_name, on=route)
    anchors = load_anchors()
    t0 = time.monotonic()
    try:
        rec = run_intervene_one(function=finfo, intervention=given.intervention, k=given.intervention_k,
                                  route=route, frames=given.frames, mgba_run=mgba_run, bios=bios,
                                  out_dir=given.run_dir, anchors=anchors,
                                  lead_in_frames=given.lead_in_frames)
    except Exception as err:                                                            # noqa: BLE001
        return RunOutput(record=None, wall_seconds=time.monotonic() - t0,
                         notes=given.notes + (ToolMessage(f"run_intervention failed: {err}"),))
    wall = time.monotonic() - t0
    diffs = sum(d.differing_frames for d in rec.anchor_diffs) if rec.anchor_diffs else 0
    return RunOutput(record=rec, wall_seconds=wall,
                     notes=given.notes + (
                         ToolMessage(f"{finfo.name}: deterministic={rec.deterministic}, "
                                     f"hung={rec.hung}, first_executed={rec.first_frame_executed}, "
                                     f"{diffs} anchor frames differ, {wall:.1f}s"),))


# ── validate the start point: null reproduces ─────────────────────────

class ValidateInput(Result):
    run_dir: Path
    function_name: FunctionName
    address: GuestAddress
    function_size: int
    intervention: str
    intervention_k: int | None
    route: RouteName
    frames: int
    lead_in_frames: int
    record: RunRecord | None
    notes: tuple[ToolMessage, ...]


class ValidateOutput(Result):
    record: RunRecord | None
    null_record: RunRecord | None
    deterministic: bool
    notes: tuple[ToolMessage, ...]


@workflow.step
def validate_null(given: ValidateInput) -> ValidateOutput:
    """For real interventions (skip / return:K), the route/window must be deterministic:
    run `null` on the same function+route and confirm zero anchor differences."""
    import gamepaths  # noqa: PLC0415
    mgba_run = Path(gamepaths.MGBA_RUN)
    bios = Path(gamepaths.GBA_BIOS)
    from wf.routes import route_named
    route = route_named(given.route)
    finfo = find_function(given.function_name, on=route)
    anchors = load_anchors()
    notes = list(given.notes)
    try:
        null_rec = run_intervene_one(function=finfo, intervention="null", k=None,
                                       route=route, frames=given.frames, mgba_run=mgba_run,
                                       bios=bios, out_dir=given.run_dir, anchors=anchors,
                                       lead_in_frames=given.lead_in_frames)
    except Exception as err:                                                            # noqa: BLE001
        notes.append(ToolMessage(f"null failed: {err}"))
        return ValidateOutput(record=given.record, null_record=None, deterministic=False,
                              notes=tuple(notes))
    deterministic = null_rec.deterministic and not null_rec.hung
    if not deterministic:
        notes.append(ToolMessage(
            f"null differs on {finfo.name}@{route.name}: route/window non-deterministic; "
            f"this start point is invalid for the {given.intervention} verdict."))
    return ValidateOutput(record=given.record, null_record=null_rec, deterministic=deterministic,
                          notes=tuple(notes))


# ── write per-run JSON + run-level report.tsv ────────────────────────

class WriteInput(Result):
    run_dir: Path
    function_name: FunctionName
    address: GuestAddress
    intervention: str
    intervention_k: int | None
    route: RouteName
    frames: int
    lead_in_frames: int
    deterministic: bool
    record: RunRecord | None
    null_record: RunRecord | None
    wall_seconds: float
    notes: tuple[ToolMessage, ...]


class WriteOutput(Result):
    pass


@workflow.step
def write_artifacts(given: WriteInput) -> WriteOutput:
    """A run-level report.tsv with one row per (function, intervention) and one per null."""
    given.run_dir.mkdir(parents=True, exist_ok=True)
    report_path = given.run_dir / "report.tsv"
    rows = ["function\taddress\tintervention\tk\troute\tdeterministic\thung\t"
            "first_executed\tanchors_diff\tdiffering_frames\twall_s\tnotes"]
    def _row(rec: RunRecord | None, intervention: str, intervention_k: int | None,
              wall: float | None) -> str:
        if rec is None:
            return (f"{given.function_name}\t{given.address:#010x}\t{intervention}\t"
                    f"{intervention_k if intervention_k is not None else '-'}\t"
                    f"{given.route}\t-\t-\t-\t-\t0\t{wall if wall is not None else '-'}\t-")
        differing = sum(d.differing_frames for d in rec.anchor_diffs)
        first = rec.first_frame_executed if rec.first_frame_executed is not None else "-"
        # Each row carries *its own* record's notes, not the shared run notes: the
        # null row must not inherit the skip row's verdict in its notes column.
        notes = "; ".join(str(n) for n in rec.notes).replace("\t", " ") or "-"
        return (f"{rec.function}\t{rec.address:#010x}\t{rec.intervention}\t"
                f"{rec.intervention_k if rec.intervention_k is not None else '-'}\t"
                f"{rec.route}\t{rec.deterministic}\t{rec.hung}\t{first}\t"
                f"{len(rec.anchor_diffs)}\t{differing}\t{wall if wall is not None else '-'}\t{notes}")
    rows.append(_row(given.record, given.intervention, given.intervention_k, given.wall_seconds))
    if given.null_record is not None:
        rows.append(_row(given.null_record, "null", None, None))
    report_path.write_text("\n".join(rows) + "\n")
    return WriteOutput()


# ── pilot: 24 named + 24 random reached functions x 4 interventions ───

PROJECT_PILOT = "recomp-intervene-pilot"

# The four interventions the pilot runs on every function. `null` is the determinism
# control (it rewrites the entry with its own bytes, so a difference there means the
# route/window is noisy and the other three rows prove nothing).
PILOT_INTERVENTIONS: tuple[tuple[str, int | None], ...] = (
    ("null", None), ("skip", None), ("return", 0), ("return", 1),
)


def intervene_pilot_graph() -> Graph["PilotState"]:
    return workflow_pilot.graph(
        first=select_pilot_targets,
        ends=[write_pilot_artifacts],
        transitions=[
            Transition(after=select_pilot_targets, go_to=run_pilot_trials,
                       when=pilot_has_targets),
            Transition(after=select_pilot_targets, go_to=write_pilot_artifacts,
                       when=pilot_has_none),
            Transition(after=run_pilot_trials, go_to=write_pilot_artifacts),
        ],
    )


class PilotState(BaseModel):
    run_dir: Path
    named: tuple[FunctionName, ...] = ()
    random: tuple[FunctionName, ...] = ()
    random_pool: tuple[FunctionName, ...] = ()    # candidates in draw order, more than `random`
    random_want: int = 24                        # how many of the pool a route really enters
    route: RouteName = "auto"
    frames: int = DEFAULT_FRAMES
    jobs: int = 8
    selected_named: tuple[FunctionName, ...] = ()
    selected_random: tuple[FunctionName, ...] = ()
    results: dict[str, RunRecord] = {}          # "NAME\tINTERVENTION" -> the record
    null_results: dict[str, RunRecord] = {}
    notes: tuple[ToolMessage, ...] = ()


workflow_pilot = Workflow[PilotState](PilotState, project=PROJECT_PILOT)


def pilot_key(name: str, intervention: str, k: int | None = None) -> str:
    """The table key for a pilot result -- `name\tKIND[?K]`. `return` and `return:K`
    are different rows (PILOT_INTERVENTIONS carries both K=0 and K=1), so the key
    has to disambiguate; skipping the K collapses both rows to one. null/skip
    never have a K so the suffix is empty.

    Storage and lookup both use this form so 'return:0' and 'return:1' each get
    their own slot."""
    if k is None:
        return f"{name}\t{intervention}"
    return f"{name}\t{intervention}:{k}"


class PilotSelectInput(Result):
    named: tuple[FunctionName, ...]
    random: tuple[FunctionName, ...]


class PilotSelectOutput(Result):
    selected_named: tuple[FunctionName, ...]
    selected_random: tuple[FunctionName, ...]


@workflow_pilot.step
def select_pilot_targets(given: PilotSelectInput) -> PilotSelectOutput:
    """The named functions in order, then the random tail (already drawn from the
    reached set by the caller, so every pilot row is a function some recording runs)."""
    return PilotSelectOutput(selected_named=tuple(given.named),
                             selected_random=tuple(given.random))


class PilotRunInput(Result):
    run_dir: Path
    selected_named: tuple[FunctionName, ...]
    random_pool: tuple[FunctionName, ...]
    random_want: int
    route: RouteName
    frames: int
    jobs: int


class PilotRunOutput(Result):
    results: dict[str, RunRecord]
    null_results: dict[str, RunRecord]
    selected_random: tuple[FunctionName, ...] = ()
    notes: tuple[ToolMessage, ...]


@workflow_pilot.step
def run_pilot_trials(given: PilotRunInput) -> PilotRunOutput:
    """For each selected function: find the first frame it executes on the shortest
    route that reaches it (once), then run null + skip + return:0 + return:1 from the
    same cached state. One JSON per (function, intervention); pilot.tsv summarises
    deterministic / hung / first_executed / anchor differences.

    The random half is drawn from `random_pool` in order and stops at the first
    `random_want` the routes actually enter -- the facts layer's `reached` flag is
    generous (it credits a function's whole range, so a symbol whose tail runs as a
    shared tail of something else reads as reached although nothing branches to its
    entry), and a row with no route to run on is not a row.

    The route's states are cached once, up front, per route in play, because the
    whole-route first-frame search and every run of every function on that route share
    them (caching them from several threads at once would race and duplicate a
    20-minute pass over `beastsp`).

    The trials are independent (each its own mgba_run invocation, its own patched ROM
    in its own temp directory, its own artifact directory), so they fan out across a
    thread pool. Results are re-ordered to the selection order before returning, so
    pilot.tsv does not depend on completion order."""
    import gamepaths  # noqa: PLC0415
    import time  # noqa: PLC0415
    from concurrent.futures import ThreadPoolExecutor                          # noqa: PLC0415
    mgba_run = Path(gamepaths.MGBA_RUN)
    bios = Path(gamepaths.GBA_BIOS)
    anchors = load_anchors()
    notes: list[ToolMessage] = []

    # 1. which route does each function run on? The named set is fixed; the random half
    #    walks its candidate pool and stops at the first `random_want` that some route
    #    actually enters, so a draw the facts oversold costs a note, not a table row.
    from wf.routes import route_named                                          # noqa: PLC0415
    fixed_route = None
    if given.route != "auto":
        fixed_route = route_named(given.route)
    routes: dict[str, Route] = {}
    first_frames: dict[str, int | None] = {}
    todo: list[str] = []
    queue: list[str] = list(given.selected_named)
    random_picked: list[str] = []
    for name in queue + list(given.random_pool):
        is_random = name in given.random_pool
        if is_random and len(random_picked) >= given.random_want:
            break
        try:
            finfo = find_function(name)
        except ValueError as err:
            notes.append(ToolMessage(f"{name}: {err}"))
            continue
        candidates = [fixed_route] if fixed_route is not None else list(it.routes_executing(finfo))
        if not candidates:
            notes.append(ToolMessage(f"{name}: no recorded route lists it; skipped"))
            continue
        # The facts pick the order, the route's execution map confirms it (the facts are
        # loose about `executed`: a symbol reached by a jump into its middle reads as
        # executed although nothing branches to its entry).
        picked: Route | None = None
        for route in candidates:
            grid = it.ensure_route_states(route, mgba_run=mgba_run, bios=bios)
            known, segment = it.first_execution_segment(
                finfo.address, route, grid=grid, last=int(route.length),
                mgba_run=mgba_run, bios=bios)
            if not known or segment is not None:
                picked = route
                break
            notes.append(ToolMessage(
                f"{name}: the facts list {route.name}, but the route's execution map says "
                f"nothing ever enters it; trying the next route"))
        if picked is None:
            notes.append(ToolMessage(
                f"{name}: no recorded route enters it (the facts list "
                f"{', '.join(str(r.name) for r in candidates)})"))
            continue
        routes[name] = picked
        todo.append(name)
        if is_random:
            random_picked.append(name)

    # 2. cache each route's states (and its execution map) once, then search + run.
    #    Both are shared by every function on that route and both are one whole-route
    #    pass, so building them from several trial threads would duplicate the work
    #    (and race on the same files).
    for route in sorted({str(r.name): r for r in routes.values()}.values(), key=lambda r: str(r.name)):
        try:
            cached = it.ensure_route_states(route, mgba_run=mgba_run, bios=bios)
        except Exception as err:                                                    # noqa: BLE001
            notes.append(ToolMessage(f"{route.name}: caching states failed: {err}"))
            continue
        notes.append(ToolMessage(f"{route.name}: {len(cached)} cached states "
                                 f"(stride {it.state_stride(route)} frames)"))
        started = time.time()
        try:
            mapping = it.ensure_route_exec_map(route, grid=cached, last=int(route.length),
                                               mgba_run=mgba_run, bios=bios)
        except Exception as err:                                                    # noqa: BLE001
            notes.append(ToolMessage(f"{route.name}: execution map failed: {err}"))
            continue
        notes.append(ToolMessage(f"{route.name}: execution map of {len(mapping or ())} "
                                 f"addresses ({time.time() - started:.0f}s)"))

    def _one(job: tuple[str, str, int | None]) -> tuple[str, str, int | None, RunRecord | None, str | None]:
        name, kind, k = job
        route = routes[name]
        try:
            finfo = find_function(name, on=route)
        except ValueError as err:
            return name, kind, k, None, f"{name}: {err}"
        ff = first_frames.get(name)
        if ff is None and name not in first_frames:
            try:
                ff = first_game_frame(finfo, route, mgba_run=mgba_run, bios=bios)
            except Exception as err:                                                # noqa: BLE001
                return name, kind, k, None, f"{name}: first-frame search failed: {err}"
            first_frames[name] = ff
        try:
            rec = run_intervene_one(function=finfo, intervention=kind, k=k,
                                     route=route, frames=given.frames, mgba_run=mgba_run,
                                     bios=bios, out_dir=given.run_dir, anchors=anchors,
                                     first_frame=ff)
        except Exception as err:                                                    # noqa: BLE001
            return name, kind, k, None, f"{name} {kind}: {err}"
        return name, kind, k, rec, None

    jobs = [(name, kind, k) for name in todo for kind, k in PILOT_INTERVENTIONS]
    workers = min(max(1, given.jobs), max(1, len(jobs)))
    with ThreadPoolExecutor(max_workers=workers) as pool:
        done = list(pool.map(_one, jobs))

    results: dict[str, RunRecord] = {}
    nulls: dict[str, RunRecord] = {}
    for name, kind, k, rec, err in done:
        if err is not None:
            notes.append(ToolMessage(err))
            continue
        assert rec is not None
        # PILOT_INTERVENTIONS uses kinds 'skip' / 'return' (no K); the record's
        # `intervention` field carries the full 'return:K'. Store by KIND+K so
        # 'return:0' and 'return:1' each get their own slot in `results` -- a
        # bare `kind` collapses them both to one row in pilot.tsv.
        (nulls if kind == "null" else results)[pilot_key(name, kind, k)] = rec
    notes.append(ToolMessage(f"{len(results)} intervention runs, {len(nulls)} null runs "
                             f"over {len(todo)} functions x {len(PILOT_INTERVENTIONS)} "
                             f"interventions ({workers} workers)"))
    return PilotRunOutput(results=results, null_results=nulls,
                          selected_random=tuple(random_picked), notes=tuple(notes))


class PilotWriteInput(Result):
    run_dir: Path
    selected_named: tuple[FunctionName, ...]
    selected_random: tuple[FunctionName, ...]
    route: RouteName
    frames: int
    results: dict[str, RunRecord]
    null_results: dict[str, RunRecord]
    notes: tuple[ToolMessage, ...]


class PilotWriteOutput(Result):
    pass


@workflow_pilot.step
def write_pilot_artifacts(given: PilotWriteInput) -> PilotWriteOutput:
    """A pilot.tsv with one row per (function, intervention): the route the facts layer
    chose, whether the run was deterministic, whether it hung and *which side* hung,
    mGBA's illegal-opcode count on each side (the crash evidence), the first frame the
    function executed, and the first anchor that differed."""
    given.run_dir.mkdir(parents=True, exist_ok=True)
    selected = list(given.selected_named) + list(given.selected_random)
    is_named = {n: True for n in given.selected_named}
    is_named.update({n: False for n in given.selected_random})
    rows: list[str] = []
    rows.append(("name\tsource\tintervention\troute\tdeterministic\thung\thung_side\t"
                 "illegal_ops_base\tillegal_ops_patched\tpatch_bytes\tfirst_frame_executed\t"
                 "anchors_diff\tdiffering_frames\tfirst_anchor_diff\tnote"))
    for name in selected:
        src = "named" if is_named.get(name) else "random"
        for kind, k in PILOT_INTERVENTIONS:
            table = given.null_results if kind == "null" else given.results
            rec = table.get(pilot_key(name, kind, k))
            display_kind = f"{kind}:{k}" if kind == "return" else kind
            if rec is None:
                # `null` rows may legitimately be missing when only a real run completed
                rows.append(f"{name}\t{src}\t{display_kind}\t-\t-\t-\t-\t-\t-\t-\t-\t-\t0\t-\t-")
                continue
            differing = sum(d.differing_frames for d in rec.anchor_diffs)
            f0 = rec.first_frame_executed if rec.first_frame_executed is not None else "-"
            first = next((d for d in rec.anchor_diffs if d.differing_frames), None)
            fad = "-" if first is None else f"{first.name}@{first.first_frame}"
            note = next((str(n) for n in rec.notes if "hung" in str(n)), "-")
            note = note.replace("\t", " ").replace("\n", " ")
            rows.append(f"{rec.function}\t{src}\t{display_kind}\t{rec.route}\t"
                        f"{rec.deterministic}\t{rec.hung}\t{rec.hung_side or '-'}\t"
                        f"{rec.illegal_opcodes[0]}\t{rec.illegal_opcodes[1]}\t"
                        f"{rec.patch_bytes or '-'}\t{f0}\t{len(rec.anchor_diffs)}\t"
                        f"{differing}\t{fad}\t{note}")
    (given.run_dir / "pilot.tsv").write_text("\n".join(rows) + "\n")
    return PilotWriteOutput()


# ── decisions ─────────────────────────────────────────────────────────

def found_a_frame(state: InterveneState) -> bool:
    return any("first game frame" in str(n) and "never" not in str(n)
                and "cached state" in str(n) for n in state.notes)


def no_execution_found(state: InterveneState) -> bool:
    return not found_a_frame(state)


def is_a_real_intervention(state: InterveneState) -> bool:
    return state.intervention in {"skip", "return"}


def is_the_null_run(state: InterveneState) -> bool:
    return state.intervention == "null"


def pilot_has_targets(state: PilotState) -> bool:
    return bool(state.selected_named or state.selected_random)


def pilot_has_none(state: PilotState) -> bool:
    return not pilot_has_targets(state)
