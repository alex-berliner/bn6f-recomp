"""`wf check-names`: check a function's name against the running game, and against what happens
when the function is taken away, with a calibration gate built into every batch.

    python -m wf check-names [--only NAME]... [--from-tsv FILE [--tsv-where K=V]...] [--count N]
                             [--jobs J] [--model PROVIDER/MODEL] [--frames-window N]

Six steps, in order:

  1. `choose_the_batch` picks the names (named with `--only`, from a TSV with `--from-tsv` such
     as the name audit, or `--count N` random ones among the functions `wf facts` says a recorded
     route reached), pairs each with another name in the batch (its swapped control) and reads
     each one's symbols/db description.
  2. `write_the_claims` asks the model, once per function, for claims and one causal prediction
     from the name and that description ALONE -- blind to the facts and to the assembly, so the
     claims predict what the later steps then observe. One retry on a reply that does not fit.
  3. `check_the_claims` (script) checks each claim against the function's `wf facts` record, and
     each name's PARTNER's claims against the same record: the swapped control for the passive
     half. `pass` / `fail` / `unknown`.
  4. `run_the_predictions` (script) runs each prediction through `wf intervene` on the route the
     facts picked, against a `null` control on the same function, route and window -- and runs the
     PARTNER's prediction against this function's intervention the same way. Everything the active
     check asks of `wf intervene` goes through one function in tools/check_names.py
     (`intervene_once`).
  5. `calibrate_and_level` does the arithmetic: what the true answers did, what the swapped ones
     did, whether the batch discriminates (the gate), and the level each name earned.
  6. `write_the_outputs` writes report.tsv, one NAME.json per name and calibration.md into the
     run's own directory under build-wf-runs/.

Nothing under bn6f/ or symbols/db is written, and no model ever sees the facts: the model writes
the claims blind, and the graph decides everything else.
"""
from __future__ import annotations

import threading
import time
from collections.abc import Callable, Sequence
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from pydantic import BaseModel

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.tools import check_names as cn
from wf.tools.check_names import (Action, ActiveResult, Answer, BatchReport, Calibration,
                                  ClaimCheck, Control, FACTS, NameResult)
from wf.tools.facts import FunctionFacts
from wf.types import FunctionName, GuestAddress, ModelSpec, Result, RouteName, ToolMessage

PROJECT = cn.PROJECT


def check_names_graph() -> Graph["CheckNamesState"]:
    return workflow.graph(
        first=choose_the_batch,
        ends=[write_the_outputs],
        transitions=[
            Transition(after=choose_the_batch, go_to=write_the_claims, when=some_selected),
            Transition(after=choose_the_batch, go_to=write_the_outputs, when=none_selected),
            Transition(after=write_the_claims, go_to=check_the_claims),
            Transition(after=check_the_claims, go_to=run_the_predictions),
            Transition(after=run_the_predictions, go_to=calibrate_and_level),
            Transition(after=calibrate_and_level, go_to=write_the_outputs),
        ],
    )


def run_check_names(start: "CheckNamesState", run_id: str, tracked: bool = True) -> BatchReport:
    outcome = check_names_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"check-names run {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class CheckNamesState(BaseModel):
    run_dir: Path                                # build-wf-runs/check-names-RUN_ID
    run_id: str = ""                             # the run's id (the model's call tags carry it)
    facts_dir: Path = FACTS                      # build/facts, read (never written)
    only: tuple[FunctionName, ...] = ()
    from_tsv: Path | None = None
    tsv_where: tuple[str, ...] = ()
    count: int = cn.DEFAULT_COUNT
    jobs: int = cn.DEFAULT_JOBS
    model: ModelSpec = cn.DEFAULT_MODEL
    frames_window: int = cn.DEFAULT_FRAMES_WINDOW
    seed: int = 0
    names: tuple[FunctionName, ...] = ()
    addresses: dict[FunctionName, GuestAddress] = {}
    descriptions: dict[FunctionName, ToolMessage] = {}
    swaps: dict[FunctionName, FunctionName] = {}
    answers: dict[FunctionName, Answer] = {}
    checks: dict[FunctionName, tuple[ClaimCheck, ...]] = {}
    swapped_checks: dict[FunctionName, tuple[ClaimCheck, ...]] = {}
    actives: dict[FunctionName, ActiveResult] = {}
    swapped_actives: dict[FunctionName, ActiveResult] = {}
    controls: dict[FunctionName, Control] = {}
    results: dict[FunctionName, NameResult] = {}
    seconds: float = 0.0
    notes: tuple[ToolMessage, ...] = ()
    calibration: Calibration | None = None
    report: BatchReport | None = None


workflow = Workflow[CheckNamesState](CheckNamesState, project=PROJECT)


# ── step one: the batch ────────────────────────────────────────────────

class ChooseInput(Result):
    run_dir: Path
    facts_dir: Path
    only: tuple[FunctionName, ...]
    from_tsv: Path | None
    tsv_where: tuple[str, ...]
    count: int
    seed: int
    seconds: float = 0.0
    notes: tuple[ToolMessage, ...] = ()


class ChooseOutput(Result):
    names: tuple[FunctionName, ...]
    addresses: dict[FunctionName, GuestAddress]
    descriptions: dict[FunctionName, ToolMessage]
    swaps: dict[FunctionName, FunctionName]
    seconds: float
    notes: tuple[ToolMessage, ...]


@workflow.step
def choose_the_batch(given: ChooseInput) -> ChooseOutput:
    """The functions in this run, their addresses, their symbols/db descriptions, and the name
    each one is calibrated against.

    `--only` wins; `--from-tsv` takes the `name` column of a TSV (the name audit, with
    `--tsv-where kind=function --tsv-where verdict=wrong`); `--count N` adds N random names from
    symbols/db whose `wf facts` record says a recorded route reached them. The pairing is drawn
    from the whole batch, so a calibration covers every name in it."""
    started = time.monotonic()
    notes: list[ToolMessage] = list(given.notes)
    index = cn.coverage_index(given.facts_dir)
    named = [str(name) for name in given.only]
    if given.from_tsv is not None:
        rows = cn.names_from_tsv(given.from_tsv, given.tsv_where)
        if not rows:
            notes.append(ToolMessage(f"{given.from_tsv}: no row matched "
                                     f"{', '.join(given.tsv_where) or '(no filter)'}"))
        named += list(rows)
    named = list(dict.fromkeys(named))
    if given.count:
        taken = cn.random_reached(given.count, given.seed, exclude=named,
                                  facts_dir=given.facts_dir)
        named += list(taken)
    addresses: dict[FunctionName, GuestAddress] = {}
    descriptions: dict[FunctionName, ToolMessage] = {}
    for item in named:
        address_hex = index.get(item, ("", False))[0]
        if not address_hex:
            notes.append(ToolMessage(f"{item}: no row in the facts coverage index "
                                     f"(build/facts/coverage.tsv); nothing to check it against"))
            continue
        addresses[FunctionName(item)] = GuestAddress(int(address_hex, 16))
        descriptions[FunctionName(item)] = ToolMessage(cn.description_of(item))
    kept = tuple(FunctionName(item) for item in dict.fromkeys(addresses))
    if not kept:
        notes.append(ToolMessage("no function in this batch has a `wf facts` record"))
    pairing = cn.pair_up([str(name) for name in kept], given.seed)
    swaps = {FunctionName(name): FunctionName(partner)
             for name, partner in pairing.swaps.items()}
    notes.append(ToolMessage(f"{len(kept)} function(s) in the batch, each paired with a random "
                             f"other one (its swapped control)"))
    return ChooseOutput(names=kept, addresses=addresses, descriptions=descriptions, swaps=swaps,
                        seconds=given.seconds + round(time.monotonic() - started, 1),
                        notes=tuple(notes))


# ── step two: the blind claims ─────────────────────────────────────────

class ClaimsInput(Result):
    run_dir: Path
    names: tuple[FunctionName, ...]
    addresses: dict[FunctionName, GuestAddress]
    descriptions: dict[FunctionName, ToolMessage]
    model: ModelSpec
    jobs: int
    seconds: float = 0.0
    notes: tuple[ToolMessage, ...] = ()


class ClaimsOutput(Result):
    answers: dict[FunctionName, Answer]
    seconds: float
    notes: tuple[ToolMessage, ...]


@workflow.step
def write_the_claims(given: ClaimsInput) -> ClaimsOutput:
    """Ask each function's model for claims and one prediction, from the name and its symbols/db
    description alone. Nothing about the runtime facts and nothing about the assembly is in the
    prompt: the claims have to predict what steps three and four then observe.

    `jobs` calls in flight; one retry on a reply that does not fit the schema (`wf investigate`'s
    convention), and an answer that still does not fit is kept with its problem."""
    started = time.monotonic()
    notes: list[ToolMessage] = list(given.notes)
    run_id = _run_id()
    answers: dict[FunctionName, Answer] = {}

    def one(name: FunctionName) -> Answer:
        return cn.ask_claims(given.model, name, given.addresses[name],
                             str(given.descriptions.get(name, "")), given.run_dir, run_id)

    for answer in _pool(list(given.names), given.jobs, one):
        answers[answer.name] = answer
        if answer.reply is None:
            notes.append(ToolMessage(f"{answer.name}: {answer.problem}"))
    usable = sum(1 for answer in answers.values() if answer.reply is not None)
    notes.append(ToolMessage(f"{usable}/{len(answers)} function(s) gave a usable answer "
                             f"({sum(answer.retries for answer in answers.values())} retried)"))
    return ClaimsOutput(answers=answers,
                        seconds=given.seconds + round(time.monotonic() - started, 1),
                        notes=tuple(notes))


# ── step three: the passive check, with its swapped control ────────────

class CheckInput(Result):
    facts_dir: Path
    names: tuple[FunctionName, ...]
    addresses: dict[FunctionName, GuestAddress]
    answers: dict[FunctionName, Answer]
    swaps: dict[FunctionName, FunctionName]
    seconds: float = 0.0
    notes: tuple[ToolMessage, ...] = ()


class CheckOutput(Result):
    checks: dict[FunctionName, tuple[ClaimCheck, ...]]
    swapped_checks: dict[FunctionName, tuple[ClaimCheck, ...]]
    notes: tuple[ToolMessage, ...]


@workflow.step
def check_the_claims(given: CheckInput) -> CheckOutput:
    """Each name's own claims against its own `wf facts` record, and its PARTNER's claims against
    the same record. A claim is `pass`, `fail` or `unknown` (the facts cannot show it: a
    write-only sound register, one replayed call, a route whose scene the table cannot name).

    The swapped half is the calibration: the same checker, the same record, an answer written
    about a different function."""
    notes: list[ToolMessage] = list(given.notes)
    checks: dict[FunctionName, tuple[ClaimCheck, ...]] = {}
    swapped: dict[FunctionName, tuple[ClaimCheck, ...]] = {}
    routes: set[str] = set()
    for name in given.names:
        record = _record(name, given.addresses, given.facts_dir)
        answer = given.answers.get(name)
        partner = given.swaps.get(name)
        partner_answer = given.answers.get(partner) if partner is not None else None
        checks[name] = cn.check_claims(answer.reply.claims if answer and answer.reply else (),
                                       record)
        swapped[name] = cn.check_claims(
            partner_answer.reply.claims if partner_answer and partner_answer.reply else (),
            record)
        if record is not None:
            routes |= {str(route.route) for route in record.routes}
    unclear = cn.unclear_routes(sorted(routes))
    if unclear:
        notes.append(ToolMessage(f"routes whose scene the table cannot name, so a scene claim "
                                 f"about them is unknown rather than passed or failed: "
                                 f"{', '.join(unclear)} -- which scenes are they?"))
    decided = sum(1 for group in checks.values() for check in group if check.status != "unknown")
    failed = sum(1 for group in checks.values() for check in group if check.status == "fail")
    notes.append(ToolMessage(f"{decided} claim(s) checkable, {failed} contradicted, over "
                             f"{len(given.names)} function(s)"))
    return CheckOutput(checks=checks, swapped_checks=swapped, seconds=given.seconds,
                       notes=tuple(notes))


# ── step four: the active check, with its swapped control ──────────────

class PredictInput(Result):
    run_dir: Path
    facts_dir: Path
    names: tuple[FunctionName, ...]
    addresses: dict[FunctionName, GuestAddress]
    answers: dict[FunctionName, Answer]
    swaps: dict[FunctionName, FunctionName]
    frames_window: int
    jobs: int
    seconds: float = 0.0
    notes: tuple[ToolMessage, ...] = ()


class PredictOutput(Result):
    actives: dict[FunctionName, ActiveResult]
    swapped_actives: dict[FunctionName, ActiveResult]
    controls: dict[FunctionName, Control]
    seconds: float
    notes: tuple[ToolMessage, ...]


@workflow.step
def run_the_predictions(given: PredictInput) -> PredictOutput:
    """Run every prediction through `wf intervene`, and every partner's prediction against the
    same function's intervention.

    For one function: pick the route the facts picked on which the function actually runs (the
    `null` control and both interventions then use that one route, so the control is a control),
    run `null` at each window a prediction asks for (a control has to cover the window it
    validates), run each intervention, and evaluate its predicates against the anchor diffs. One
    function that fails, hangs or is never reached never stops the batch."""
    started = time.monotonic()
    notes: list[ToolMessage] = list(given.notes)
    out_dir = given.run_dir / "intervene"
    lock = threading.Lock()
    actives: dict[FunctionName, ActiveResult] = {}
    swapped_actives: dict[FunctionName, ActiveResult] = {}
    controls: dict[FunctionName, Control] = {}
    done = [0]

    def one(name: FunctionName) -> _OneActive:
        record = _record(name, given.addresses, given.facts_dir)
        answer = given.answers.get(name)
        partner = given.swaps.get(name)
        partner_answer = given.answers.get(partner) if partner is not None else None
        active, swapped, control, note = _active_for_one(
            name=name, address_hex=f"0x{int(given.addresses[name]):08X}", record=record,
            true_action=answer.reply.prediction if answer and answer.reply else None,
            swapped_action=(partner_answer.reply.prediction
                            if partner_answer and partner_answer.reply else None),
            out_dir=out_dir, frames_window=given.frames_window)
        with lock:
            done[0] += 1
            if done[0] % 5 == 0 or done[0] == len(given.names):
                notes.append(ToolMessage(f"{done[0]}/{len(given.names)} functions intervened "
                                         f"({time.monotonic() - started:.0f}s so far)"))
        return _OneActive(name=name, active=active, swapped=swapped, control=control,
                          notes=note)

    for one_done in _pool(list(given.names), given.jobs, one):
        if one_done.active is not None:
            actives[one_done.name] = one_done.active
        if one_done.swapped is not None:
            swapped_actives[one_done.name] = one_done.swapped
        if one_done.control is not None:
            controls[one_done.name] = one_done.control
        notes += list(one_done.notes)
    counts: dict[str, int] = {}
    for active in actives.values():
        counts[active.outcome] = counts.get(active.outcome, 0) + 1
    notes.append(ToolMessage("true predictions: "
                             + (", ".join(f"{key} {value}"
                                          for key, value in sorted(counts.items())) or "none")))
    counts = {}
    for active in swapped_actives.values():
        counts[active.outcome] = counts.get(active.outcome, 0) + 1
    notes.append(ToolMessage("swapped predictions: "
                             + (", ".join(f"{key} {value}"
                                          for key, value in sorted(counts.items())) or "none")))
    return PredictOutput(actives=actives, swapped_actives=swapped_actives, controls=controls,
                         seconds=given.seconds + round(time.monotonic() - started, 1),
                         notes=tuple(notes))


class _OneActive(BaseModel):
    """One function's active half: the true prediction, the partner's prediction run against the
    same function, and the `null` control they are both judged against."""
    name: FunctionName
    active: ActiveResult | None = None
    swapped: ActiveResult | None = None
    control: Control | None = None
    notes: tuple[ToolMessage, ...] = ()


def _active_for_one(*, name: FunctionName, address_hex: str, record: FunctionFacts | None,
                    true_action: Action | None, swapped_action: Action | None,
                    out_dir: Path, frames_window: int
                    ) -> tuple[ActiveResult | None, ActiveResult | None, Control | None,
                               tuple[ToolMessage, ...]]:
    """One function's active half, end to end. The route is chosen once (the first the facts
    picked on which the function runs), so the control and both interventions share it."""
    notes: list[ToolMessage] = []
    routes = cn.candidate_routes(record)
    if not routes:
        return None, None, None, (ToolMessage(
            f"{name}: no recorded route `wf intervene` can replay reached it"),)
    route: RouteName | None = None
    for candidate, _length in routes:
        probe = cn.intervene_once(name=name, address_hex=address_hex, route=candidate,
                                  kind="probe", out_dir=out_dir, frames_window=frames_window)
        if probe.ran:
            route = candidate
            break
        if probe.problem:
            notes.append(ToolMessage(str(probe.problem)))
    if route is None:
        return None, None, None, tuple(notes + [ToolMessage(
            f"{name}: not reached on any of {len(routes)} candidate route(s) inside the first "
            f"{frames_window} game frames")])
    controls: dict[int, Control] = {}

    def run(action: Action | None) -> ActiveResult | None:
        """One prediction on the chosen route, judged against a `null` run at its window.
        A NoEffect (the model said the name implies no visible effect) is judged as-is
        without any intervention: a pure arithmetical outcome, no emulator in the loop."""
        if action is None:
            return None
        if isinstance(action, cn.NoEffect):
            return cn.judge_prediction(action, None, None, route=route, seconds=0.0)
        window = int(action.window)
        if window not in controls:
            control_run = cn.intervene_once(name=name, address_hex=address_hex, route=route,
                                            kind="null", window=window, out_dir=out_dir,
                                            frames_window=frames_window)
            cn.drop_roms(control_run.roms)
            controls[window] = _control_of(control_run, route, window)
        observed = cn.intervene_once(name=name, address_hex=address_hex, route=route,
                                     kind=action.kind, k=getattr(action, "k", None),
                                     window=window, out_dir=out_dir,
                                     frames_window=frames_window)
        try:
            return cn.judge_prediction(action, observed.record, controls[window], route=route,
                                       seconds=observed.seconds)
        finally:
            cn.drop_roms(observed.roms)

    active = run(true_action)
    swapped = run(swapped_action)
    # A no-prediction outcome (NoEffect, an error) has no emulator window to bind a control to.
    # A tested outcome (held, failed, hung, undetermined) is the only kind a control validates.
    control: Control | None = None
    for run_result in (active, swapped):
        if run_result is None or not run_result.tested or run_result.route is None:
            continue
        candidate = controls.get(run_result.window)
        if candidate is not None:
            control = candidate
            break
    if control is not None and not control.sound:
        notes.append(ToolMessage(f"{name} on {route}: the null control was not sound, so no "
                                 f"prediction from this start point means anything"))
    return active, swapped, control, tuple(notes)


def _control_of(observation: object, route: RouteName, window: int) -> Control:
    """A `null` run's outcome as a Control: sound only when it showed no difference at all and
    did not hang (the route/window, not the patch, is what determinism is about)."""
    record = getattr(observation, "record", None)
    if record is None:
        problem = getattr(observation, "problem", None)
        return Control(route=route, window=window, deterministic=None, hung=False,
                       problem=problem if isinstance(problem, str) else None)
    deterministic = getattr(record, "deterministic", None)
    return Control(route=route, window=window,
                   deterministic=deterministic if isinstance(deterministic, bool) else None,
                   hung=bool(getattr(record, "hung", False)))


# ── step five: the gate and the levels ─────────────────────────────────

class LevelInput(Result):
    facts_dir: Path
    names: tuple[FunctionName, ...]
    addresses: dict[FunctionName, GuestAddress]
    answers: dict[FunctionName, Answer]
    checks: dict[FunctionName, tuple[ClaimCheck, ...]]
    swapped_checks: dict[FunctionName, tuple[ClaimCheck, ...]]
    actives: dict[FunctionName, ActiveResult]
    swapped_actives: dict[FunctionName, ActiveResult]
    controls: dict[FunctionName, Control]
    swaps: dict[FunctionName, FunctionName]


class LevelOutput(Result):
    results: dict[FunctionName, NameResult]
    calibration: Calibration


@workflow.step
def calibrate_and_level(given: LevelInput) -> LevelOutput:
    """The arithmetic, and only the arithmetic: what the true claims and predictions did, what the
    swapped ones did against the same facts and interventions, whether the batch discriminates
    (the gate), and the level each name earned under that gate."""
    true_checks = [given.checks.get(name, ()) for name in given.names]
    swapped_checks = [given.swapped_checks.get(name, ()) for name in given.names]
    true_runs = [given.actives[name] for name in given.names if name in given.actives]
    swapped_runs = [given.swapped_actives[name] for name in given.names
                    if name in given.swapped_actives]
    cal = cn.calibration(true_checks, swapped_checks, true_runs, swapped_runs)
    results: dict[FunctionName, NameResult] = {}
    for name in given.names:
        record = _record(name, given.addresses, given.facts_dir)
        checks = given.checks.get(name, ())
        active = given.actives.get(name)
        swapped_active = given.swapped_actives.get(name)
        control = given.controls.get(name)
        control_held = bool(swapped_active is not None and swapped_active.held)
        passive = cn.passive_verdict(checks)
        level, gated_by = cn.level_for(
            reached=bool(record is not None and cn.reached_routes(record)), passive=passive,
            active=active or cn.ActiveResult(
                outcome="none",
                problem=ToolMessage("no prediction was run for this function")),
            control_held=control_held, claims_ok=cal.claims_ok,
            predictions_ok=cal.predictions_ok)
        results[name] = NameResult(
            name=name, address=given.addresses[name],
            address_hex=f"0x{int(given.addresses[name]):08X}", level=level, gated_by=gated_by,
            passive=passive, answer=given.answers.get(name), checks=checks,
            swapped_from=given.swaps.get(name),
            swapped_checks=given.swapped_checks.get(name, ()), active=active,
            swapped_active=swapped_active, control=control,
            routes=cn.candidate_routes(record))
    return LevelOutput(results=results, calibration=cal)


# ── step six: the run's output ─────────────────────────────────────────

class WriteInput(Result):
    run_dir: Path
    run_id: str
    names: tuple[FunctionName, ...]
    results: dict[FunctionName, NameResult]
    calibration: Calibration | None = None
    seconds: float
    notes: tuple[ToolMessage, ...]


class WriteOutput(Result):
    report: BatchReport


@workflow.step
def write_the_outputs(given: WriteInput) -> WriteOutput:
    """report.tsv (one row per name: level, claims, the prediction, its active outcome, the
    partner's, and the null control), one NAME.json per name with everything the run knows about
    it, and calibration.md with the batch's own numbers. All of it in the run's own directory
    under build-wf-runs/: nothing under symbols/db or bn6f is written."""
    results = [given.results[name] for name in given.names if name in given.results]
    tokens = sum(answer.tokens for answer in (result.answer for result in results)
                 if answer is not None)
    calibration = given.calibration or cn.calibration([], [], [], [])
    return WriteOutput(report=cn.write_report(results, calibration, given.run_dir,
                                             given.run_id, given.seconds, tokens,
                                             notes=given.notes))


# ── decisions ──────────────────────────────────────────────────────────

def some_selected(state: CheckNamesState) -> bool:
    return bool(state.names)


def none_selected(state: CheckNamesState) -> bool:
    return not state.names


# ── helpers ────────────────────────────────────────────────────────────

def _pool[T](items: Sequence[T], jobs: int, one: Callable[[T], T]) -> list[T]:
    """one(item) for every item, at most JOBS threads at a time, in the items' order (a slow item
    never reorders the run's output)."""
    with ThreadPoolExecutor(max_workers=max(1, min(jobs, len(items) or 1))) as pool:
        return list(pool.map(one, items))


def _record(name: FunctionName, addresses: dict[FunctionName, GuestAddress],
            facts_dir: Path) -> FunctionFacts | None:
    """One function's `wf facts` record, read where it is needed rather than carried through the
    run's state (a record is a few kB and there are thousands)."""
    return cn.read_record(f"0x{int(addresses[name]):08X}", facts_dir)


def _run_id() -> str:
    """This run's id, for the model-call tags."""
    try:
        return current_run_id()
    except RuntimeError:
        return "check-names-manual"