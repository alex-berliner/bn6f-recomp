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
12 named plus 12 random functions as skip+null and writes one table. Nothing under
symbols/db or bn6f is touched; the orchestrator (`wf record`) applies what a human
accepts from the table.

  python -m wf intervene --function NAME_OR_ADDR [--intervention skip|return:K|null]
                         [--route R|auto] [--frames N]
  python -m wf intervene-pilot [--route R] [--frames N] [--random N]

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
    state_path = it.STATES_DIR / f"{route.name}_{start:06d}.state"
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


# ── pilot: 12 named + 12 random skip + null runs ──────────────────────

PROJECT_PILOT = "recomp-intervene-pilot"


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
    route: RouteName = "battles1"
    frames: int = DEFAULT_FRAMES
    selected_named: tuple[FunctionName, ...] = ()
    selected_random: tuple[FunctionName, ...] = ()
    results: dict[str, RunRecord] = {}
    null_results: dict[str, RunRecord] = {}
    notes: tuple[ToolMessage, ...] = ()


workflow_pilot = Workflow[PilotState](PilotState, project=PROJECT_PILOT)


class PilotSelectInput(Result):
    named: tuple[FunctionName, ...]
    random: tuple[FunctionName, ...]


class PilotSelectOutput(Result):
    selected_named: tuple[FunctionName, ...]
    selected_random: tuple[FunctionName, ...]


@workflow_pilot.step
def select_pilot_targets(given: PilotSelectInput) -> PilotSelectOutput:
    """Pick the named functions in order, then add the random tail from the lifted set
    whose names hint at drawing / OAM / palette / fade / sound / sprite effects."""
    named = list(given.named) if given.named else list(PILOT_NAMED)
    rng_pool = list(named)
    extras = list(given.random)
    return PilotSelectOutput(selected_named=tuple(named),
                              selected_random=tuple(extras))


class PilotRunInput(Result):
    run_dir: Path
    selected_named: tuple[FunctionName, ...]
    selected_random: tuple[FunctionName, ...]
    route: RouteName
    frames: int


class PilotRunOutput(Result):
    results: dict[str, RunRecord]
    null_results: dict[str, RunRecord]
    notes: tuple[ToolMessage, ...]


@workflow_pilot.step
def run_pilot_trials(given: PilotRunInput) -> PilotRunOutput:
    """For each selected function: run null + skip on `route`. One JSON per row; a
    pilot.tsv summarises deterministic/hung/first_executed/anchor differences.

    The trials are independent subprocess runs (each its own mgba_run invocation with
    its own patched ROM copy and its own artifact directory), so they fan out across a
    thread pool. Results are re-ordered to the selection order before returning, so
    pilot.tsv is deterministic regardless of completion order."""
    import gamepaths  # noqa: PLC0415
    from concurrent.futures import ThreadPoolExecutor                          # noqa: PLC0415
    mgba_run = Path(gamepaths.MGBA_RUN)
    bios = Path(gamepaths.GBA_BIOS)
    from wf.routes import route_named                                          # noqa: PLC0415
    route = route_named(given.route)
    anchors = load_anchors()
    selected = list(given.selected_named) + list(given.selected_random)
    notes: list[ToolMessage] = []
    jobs: list[tuple[str, str]] = [(name, kind) for name in selected for kind in ("null", "skip")]

    def _one(job: tuple[str, str]) -> tuple[str, str, RunRecord | None, str | None]:
        name, kind = job
        try:
            finfo = find_function(name, on=route)
        except ValueError as err:
            return name, kind, None, f"{name}: {err}"
        try:
            rec = run_intervene_one(function=finfo, intervention=kind, k=None,
                                     route=route, frames=given.frames, mgba_run=mgba_run,
                                     bios=bios, out_dir=given.run_dir, anchors=anchors)
        except Exception as err:                                                    # noqa: BLE001
            return name, kind, None, f"{name} {kind}: {err}"
        return name, kind, rec, None

    workers = min(8, max(1, len(jobs)))
    with ThreadPoolExecutor(max_workers=workers) as pool:
        done = list(pool.map(_one, jobs))

    results: dict[str, RunRecord] = {}
    nulls: dict[str, RunRecord] = {}
    for name, kind, rec, err in done:
        if err is not None:
            notes.append(ToolMessage(err))
            continue
        assert rec is not None
        (results if kind == "skip" else nulls)[rec.function] = rec
    notes.append(ToolMessage(f"{len(results)} skip runs, {len(nulls)} null runs "
                             f"over {len(selected)} functions ({workers} workers)"))
    return PilotRunOutput(results=results, null_results=nulls, notes=tuple(notes))


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
    """A pilot.md table (24 rows: name, source, intervention, deterministic, hung,
    first_executed, anchor diffs) and a pilot.tsv next to it."""
    given.run_dir.mkdir(parents=True, exist_ok=True)
    selected = list(given.selected_named) + list(given.selected_random)
    is_named = {n: True for n in given.selected_named}
    is_named.update({n: False for n in given.selected_random})
    rows: list[str] = []
    rows.append(("name\tsource\tintervention\tdeterministic\thung\t"
                 "first_frame_executed\tanchors_diff\tdiffering_frames\t"
                 "first_anchor_diff\troute"))
    for name in selected:
        src = "named" if is_named.get(name) else "random"
        for intervention, table in (("skip", given.results), ("null", given.null_results)):
            rec = table.get(name)
            if rec is None:
                rows.append(f"{name}\t{src}\t{intervention}\t-\t-\t-\t-\t-\t-\t{given.route}")
                continue
            differing = sum(d.differing_frames for d in rec.anchor_diffs)
            f0 = rec.first_frame_executed if rec.first_frame_executed is not None else "-"
            first = next((d for d in rec.anchor_diffs if d.differing_frames), None)
            fad = "-" if first is None else f"{first.name}@{first.first_frame}"
            rows.append(f"{rec.function}\t{src}\t{rec.intervention}\t"
                        f"{rec.deterministic}\t{rec.hung}\t{f0}\t{len(rec.anchor_diffs)}\t"
                        f"{differing}\t{fad}\t{rec.route}")
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
