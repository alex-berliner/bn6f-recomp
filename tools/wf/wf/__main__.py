"""Start workflows, answer the ones waiting at a pause point, see where every
run is, and regenerate the docs. From anywhere in the repo, `./wf ARGS` (the
launcher at the root) is `python -m wf ARGS` with this folder's venv.

  python -m wf verify  [--function NAME]... [--all-readable] [--snapshots DIR] [--route NAME]... [--battle-code]
                       [--trials N]   # it says first what it will compare: no functions, no difftest or oracle
  python -m wf capture --function NAME... [--route NAME]... [--calls N]   # into build-snapshots/NAME
  python -m wf farm    --model PROVIDER/MODEL... --snapshots DIR [--rounds N] [--jobs N] [--prompt typed|old] FUNCTION...
  python -m wf queue   [--hours H] [--lane PROVIDER=WORKERS]... [--cap TOKENS] [--rounds N] [--leave-alone FILE]...
  python -m wf queue   --stop | --status    # ask the running queue to finish its farm runs and end (SIGTERM
                                            # or Ctrl-C too; twice: at once, jobs under way stay open); its ledger
  python -m wf drafts  --model PROVIDER/MODEL [--drafter P/M] [--prompt old] [--jobs N] [FUNCTION...]   # entries for accepted candidates
  python -m wf investigate [--count N] [--only NAME]... [--only-file FILE] [--retry-unnamed RUN_ID]
                       [--facts|--no-facts] [--model M] [--checker M] [--jobs N]
                       # cheap models propose candidate symbols/db entries for placeholder functions;
                       # the candidates (for review, nothing installed) go to build/investigate/RUN_ID.
                       # --facts adds what the original game did at runtime (`wf facts`, build/facts):
                       # the scenes, the runtime callers, the RAM and I/O written, the arguments and
                       # the returns of the calls replayed in mGBA; the name has to be consistent with it
  python -m wf investigate-data [--count N] [--only NAME]... [--model M] [--checker M] [--jobs N]
                       # the same for placeholder DATA labels (byte_203F750, unk_2017A00): the code
                       # that uses the label is the evidence; candidates go to build/investigate_data/RUN_ID
  python -m wf intervene   --function NAME_OR_ADDR [--intervention skip|return:K|null]
                           [--route R] [--frames N]
                       # mGBA intervention runner: patch a function, replay baseline+patched from a
                       # cached state on the same trace, observe OAM/palette/VRAM/I-O/screen. Patched
                       # ROMs under build/intervene/patched-roms; per-run JSON under build/intervene/RUN_ID/
  python -m wf intervene-pilot [--route R] [--frames N] [--random N]
                       # 12 named + N random functions, null + skip each on the route. Pilot table
                       # at build/intervene/RUN_ID/pilot.tsv (and pilot.md).
  (a model can be a tier: tier/cheap, tier/strong; each request goes to the first of its sources with quota)
  python -m wf convert --subsystem WHAT --file NAME.c [--model P/M]... [--rounds N] [--snapshots DIR]
                       [--candidate FILE | --farm-model P/M [--prompt old]] [--defer-record] [--battle-code] FUNCTION...
  (--prompt old: the farm's old-prompt candidates, filed as P/M+old; a convert run installing them queues
   them for retype, tools/abi/retype_queue.tsv)
  python -m wf runtime-evidence [--count N] [--only NAME]... [--states K=8] [--jobs J] [--model P/M]
                       # runtime evidence for the names `wf investigate` produced: replay in mGBA,
                       # summarise memory/I/O, judge against the current name. Output to
                       # build/runtime/RUN_ID (per-function NAME.yaml + report.tsv + contradictions.tsv).
                       # Nothing under symbols/db is touched; `wf record` applies the evidence after review.
  python -m wf facts [--only NAME]... [--states K=8] [--jobs J=6] [--limit N]
                       # the runtime-facts layer, for every lifted function: what the ORIGINAL game did
                       # at runtime, from the recordings alone (route coverage, runtime callers, and
                       # K captured calls replayed in mGBA). No model is called. Output to
                       # build/facts (by_address/0x08XXXXXX.json per function + coverage.tsv +
                       # progress.txt); regenerable, never committed, and cached per function.
  python -m wf check-names [--only NAME]... [--from-tsv FILE [--tsv-where K=V]...] [--count N]
                            [--jobs J] [--model P/M] [--frames-window N]
                       # check a function's name against the running game: a model writes claims and
                       # one causal prediction BLIND (name + symbols/db description only), the claims
                       # are checked against build/facts, the prediction is run through `wf intervene`
                       # against a null control, and every batch carries swapped controls and a
                       # calibration gate. Output to build-wf-runs/check-names-RUN (report.tsv +
                       # NAME.json + calibration.md). Nothing under symbols/db or bn6f is written.
  python -m wf record  [--symbol NAME]... [--rename OLD NEW]... [--pending] --message-file FILE
  python -m wf vidsync-run [--start CHECKPOINT|RUN] [--run NAME] [--no-deploy] [--replay PATH] [--watch-hours H]
                       [--stall-minutes 30] [--pathcheck-minutes 60] [--no-repair]   # docs/vidsync/README.md
  python -m wf vidsync-repair --run NAME [--loss G] [--model P/M]... [--max-attempts N]
  python -m wf vidsync-ready [--checkpoint PATH|RUN] [--most G] [--out-dir DIR] [--every N] [--local DIR] [--import]
  python -m wf vidsync-status [RUN]        # the driver's state on 10.67.1.175 (furthest frame, last event)
  python -m wf vidsync-tick [--run NAME]    # one check-in (for a 15-minute recurring prompt): acts on the
                                            # routine cases, stops at `decide` for the rest
  python -m wf waiting RUN_ID              # what a paused run needs
  python -m wf answer  RUN_ID ANSWER.json  # give it, and carry on
  python -m wf retry   RUN_ID              # carry on a failed or interrupted run from where it stopped
  python -m wf status
  python -m wf freeze  --reason WHY        # installs, renames and commits wait (models go on); returns once
  python -m wf thaw                        #   the ones under way are done; thaw lets them go on
  python -m wf archive [--dry-run]         # copy the text logs of the runs symbols/db cites to build-wf-archive
  python -m wf docs [OUT_DIR]

Every run is tracked for the Burr UI (`tools/wf/.venv/bin/burr --no-open
--no-copy-demo_data --host 0.0.0.0`, port 7241); tool logs go to
build-wf-runs/RUN_ID; what all runs share (locks, provider outages, the install
freeze) is in build-wf-state.
"""
import argparse
import hashlib
import json
import signal
import time
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, Field, TypeAdapter

from wf.burr_typed import Finished, Paused
from wf.describe import write_docs
from wf.graphs.capture import CaptureState, run_capture
from wf.graphs.drafts import DraftsState, run_drafts
from wf.graphs.convert import DEFAULT_MODELS, PENDING_RENAMES, ConvertState, ReviewRequest, convert_graph, review_sheet
from wf.graphs.farm import FarmBatchState, farm_batch_graph
from wf.graphs.queue import QueueState, ledger_summary, queue_graph, request_stop
from wf.graphs.record import RecordState, record_graph
from wf.graphs.verify import VerifyReport, VerifyState, all_readable, run_verify, what_it_checks
from wf.graphs.investigate import InvestigateState, run_investigate
from wf.graphs.investigate_data import InvestigateDataState, run_investigate_data
from wf.graphs.runtime_evidence import RuntimeEvidenceState, run_runtime_evidence, JUDGE_MODE_DEFAULT
from wf.graphs.facts import FactsState, run_facts, summarise_report
from wf.graphs.check_names import CheckNamesState, run_check_names
from wf.tools.check_names import DEFAULT_COUNT as DEFAULT_CN_COUNT
from wf.tools.check_names import DEFAULT_FRAMES_WINDOW as DEFAULT_CN_WINDOW
from wf.tools.check_names import DEFAULT_JOBS as DEFAULT_CN_JOBS
from wf.tools.check_names import DEFAULT_MODEL as DEFAULT_CN_MODEL
from wf.tools.check_names import summarise as summarise_check_names
from wf.tools.facts import DEFAULT_STATES as DEFAULT_FACTS_STATES
from wf.tools.facts import DEFAULT_JOBS as DEFAULT_FACTS_JOBS
from wf.tools.facts import DEFAULT_LIMIT as DEFAULT_FACTS_LIMIT
from wf.tools.facts import FACTS
from wf.tools.runtime_evidence import DEFAULT_COUNT as DEFAULT_RE_COUNT
from wf.tools.runtime_evidence import DEFAULT_MODEL as DEFAULT_RE_MODEL
from wf.tools.runtime_evidence import DEFAULT_STATES as DEFAULT_RE_STATES
from wf.tools.runtime_evidence import DEFAULT_JOBS as DEFAULT_RE_JOBS
from wf.graphs.vidsync_ready import VidsyncReadyState, vidsync_ready_graph
from wf.graphs.vidsync_repair import VidsyncRepairState, vidsync_repair_graph
from wf.graphs.vidsync_run import VidsyncRunState, vidsync_run_graph
from wf.graphs.vidsync_tick import VidsyncTickState, note_waiting, vidsync_tick_graph
from wf.tools.sources import TIERS, Tier
from wf.tools import investigate
from wf.tools import investigate_data
from wf.tools.vidsync import driver_status
from wf.tools.investigate import DEFAULT_CHECKER as DEFAULT_INVESTIGATE_CHECKER
from wf.tools.investigate import DEFAULT_MODEL as DEFAULT_INVESTIGATE_MODEL
from wf.tools.investigate import DEFAULT_COUNT as DEFAULT_INVESTIGATE_COUNT
from wf.tools.investigate import FACTS_BY_ADDRESS
from wf.tools.investigate_data import DEFAULT_CHECKER as DEFAULT_INVESTIGATE_DATA_CHECKER
from wf.tools.investigate_data import DEFAULT_COUNT as DEFAULT_INVESTIGATE_DATA_COUNT
from wf.tools.investigate_data import DEFAULT_MODEL as DEFAULT_INVESTIGATE_DATA_MODEL
from wf.tools.investigate_data import DEFAULT_JOBS as DEFAULT_INVESTIGATE_DATA_JOBS
from wf.repo import REPO, RUNS, SNAPSHOTS
from wf.tools.archive import ARCHIVE, archive_cited_runs
from wf.tools.locks import freeze_installs, install_freeze, thaw_installs
from wf.tools.models import PromptKind, with_prompt
from wf.tools.sources import models_for
from wf.tools.work_queue import CAP, LANES, STOP, Lane, only_one_queue, read_ledger
from wf.routes import ALL_ROUTES, LONG_ROUTES, route_named
from wf.status import all_runs, workflow_of_run
from wf.types import (FrameCount, FunctionName, ModelSpec, RemotePath, Rename, Route, RouteName, RunName, SnapshotDir,
                      SymbolName)


class VerifyCommand(BaseModel):
    command: Literal["verify"]
    functions: tuple[FunctionName, ...] = Field(alias="function")
    all_readable: bool
    snapshots: SnapshotDir | None
    routes: tuple[RouteName, ...] = Field(alias="route")
    battle_code: bool
    trials: int


class CaptureCommand(BaseModel):
    command: Literal["capture"]
    functions: tuple[FunctionName, ...] = Field(alias="function")
    routes: tuple[RouteName, ...] = Field(alias="route")
    calls: int


class FarmCommand(BaseModel):
    command: Literal["farm"]
    functions: tuple[FunctionName, ...]
    models: tuple[ModelSpec, ...] = Field(alias="model")
    snapshots: SnapshotDir
    rounds: int
    jobs: int
    prompt: PromptKind


class QueueCommand(BaseModel):
    command: Literal["queue"]
    hours: float | None
    lanes: tuple[str, ...] = Field(alias="lane")
    cap: int
    rounds: int
    shift_hours: float
    leave_alone: tuple[Path, ...]
    stop: bool
    status: bool


class DraftsCommand(BaseModel):
    command: Literal["drafts"]
    model: ModelSpec
    drafter: ModelSpec | None = None
    functions: tuple[FunctionName, ...]
    jobs: int
    prompt: PromptKind


class InvestigateCommand(BaseModel):
    command: Literal["investigate"]
    count: int
    only: tuple[FunctionName, ...] = Field(alias="only")
    only_file: Path | None = None
    retry_unnamed: str | None = None
    gather: bool = False
    model: ModelSpec
    checker: ModelSpec
    jobs: int


class InvestigateDataCommand(BaseModel):
    command: Literal["investigate-data"]
    count: int
    only: tuple[SymbolName, ...] = Field(alias="only")
    model: ModelSpec
    checker: ModelSpec
    jobs: int


class InterveneCommand(BaseModel):
    command: Literal["intervene"]
    function: str = Field(alias="function")            # a single bn6f name or hex address
    intervention: str
    intervention_k: int | None = None
    route: str = "wander"
    frames: int = 120


class IntervenePilotCommand(BaseModel):
    command: Literal["intervene-pilot"]
    route: str = "auto"
    frames: int = 120
    random: int = 24
    only: list[str] = Field(default_factory=list)
    jobs: int = 8
    seed: int | None = None


class RuntimeEvidenceCommand(BaseModel):
    command: Literal["runtime-evidence"]
    count: int
    only: tuple[FunctionName, ...] = Field(alias="only")
    states: int
    jobs: int
    model: ModelSpec
    contrastive: bool = False                # opt into the contrastive judge


class FactsCommand(BaseModel):
    command: Literal["facts"]
    only: tuple[FunctionName, ...] = Field(alias="only")
    states: int
    jobs: int
    limit: int


class CheckNamesCommand(BaseModel):
    command: Literal["check-names"]
    only: tuple[FunctionName, ...]
    from_tsv: Path | None = None
    tsv_where: tuple[str, ...] = ()
    count: int
    jobs: int
    model: ModelSpec
    frames_window: int
    seed: int | None = None


class ConvertCommand(BaseModel):
    command: Literal["convert"]
    subsystem: str
    file: str
    functions: tuple[FunctionName, ...]
    models: tuple[ModelSpec, ...] = Field(alias="model")
    rounds: int
    snapshots: SnapshotDir | None
    candidate: Path | None
    farm_model: ModelSpec | None
    defer_record: bool
    battle_code: bool
    prompt: PromptKind


class RecordCommand(BaseModel):
    command: Literal["record"]
    symbols: tuple[SymbolName, ...] = Field(alias="symbol")
    renames: tuple[tuple[SymbolName, SymbolName], ...] = Field(alias="rename")
    pending: bool
    message_file: Path


class VidsyncRunCommand(BaseModel):
    command: Literal["vidsync-run"]
    start: RemotePath | None
    run: RunName | None
    no_deploy: bool
    replay: RemotePath | None
    watch_hours: float | None
    poll_minutes: float
    stall_minutes: float
    pathcheck_minutes: float
    no_repair: bool


class VidsyncRepairCommand(BaseModel):
    command: Literal["vidsync-repair"]
    run: RunName
    loss: FrameCount | None
    models: tuple[ModelSpec, ...] = Field(alias="model")
    max_attempts: int


class VidsyncReadyCommand(BaseModel):
    command: Literal["vidsync-ready"]
    checkpoint: RemotePath | None
    most: FrameCount | None
    out_dir: RemotePath
    name: str
    every: int
    local: Path | None
    import_route: bool = Field(alias="import")


class VidsyncTickCommand(BaseModel):
    command: Literal["vidsync-tick"]
    run: RunName | None


class VidsyncStatusCommand(BaseModel):
    command: Literal["vidsync-status"]
    run: RunName | None


class WaitingCommand(BaseModel):
    command: Literal["waiting"]
    run_id: str


class AnswerCommand(BaseModel):
    command: Literal["answer"]
    run_id: str
    answer_file: Path


class RetryCommand(BaseModel):
    command: Literal["retry"]
    run_id: str


class StatusCommand(BaseModel):
    command: Literal["status"]


class FreezeCommand(BaseModel):
    command: Literal["freeze"]
    reason: str


class ThawCommand(BaseModel):
    command: Literal["thaw"]


class ArchiveCommand(BaseModel):
    command: Literal["archive"]
    dry_run: bool


class DocsCommand(BaseModel):
    command: Literal["docs"]
    out_dir: Path


Command = (VerifyCommand | CaptureCommand | FarmCommand | QueueCommand | DraftsCommand
           | InvestigateCommand | InvestigateDataCommand | RuntimeEvidenceCommand | FactsCommand
           | CheckNamesCommand
           | InterveneCommand | IntervenePilotCommand
           | ConvertCommand | RecordCommand
           | VidsyncRunCommand | VidsyncRepairCommand | VidsyncReadyCommand | VidsyncTickCommand | VidsyncStatusCommand | WaitingCommand | AnswerCommand | RetryCommand | StatusCommand | FreezeCommand | ThawCommand
           | ArchiveCommand | DocsCommand)


def main() -> None:
    command = parse_command()
    match command:
        case VerifyCommand():
            start_verify(command)
        case CaptureCommand():
            start_capture(command)
        case FarmCommand():
            start_farm(command)
        case QueueCommand():
            start_queue(command)
        case DraftsCommand():
            start_drafts(command)
        case InvestigateCommand():
            start_investigate(command)
        case InvestigateDataCommand():
            start_investigate_data(command)
        case InterveneCommand():
            start_intervene(command)
        case IntervenePilotCommand():
            start_intervene_pilot(command)
        case RuntimeEvidenceCommand():
            start_runtime_evidence(command)
        case FactsCommand():
            start_facts(command)
        case CheckNamesCommand():
            start_check_names(command)
        case ConvertCommand():
            start_convert(command)
        case RecordCommand():
            start_record(command)
        case VidsyncRunCommand():
            start_vidsync_run(command)
        case VidsyncRepairCommand():
            start_vidsync_repair(command)
        case VidsyncReadyCommand():
            start_vidsync_ready(command)
        case VidsyncTickCommand():
            start_vidsync_tick(command)
        case VidsyncStatusCommand():
            print(driver_status(command.run).model_dump_json(indent=2))
        case WaitingCommand():
            show_waiting(command.run_id)
        case AnswerCommand():
            answer(command)
        case RetryCommand():
            print_outcome(workflow_of_run(command.run_id).graph().retry(command.run_id))
        case StatusCommand():
            show_status()
        case FreezeCommand():
            freeze = freeze_installs(command.reason)
            print(f"installs frozen since {freeze.since:%m-%d %H:%M} ({freeze.reason}); none under way. "
                  "`wf thaw` lets them go on.")
        case ThawCommand():
            thawed = thaw_installs()
            print(f"thawed: installs frozen since {thawed.since:%m-%d %H:%M} ({thawed.reason}) go on" if thawed
                  else "installs were not frozen")
        case ArchiveCommand():
            show_archive(command.dry_run)
        case DocsCommand():
            for path in write_docs(command.out_dir):
                print(f"wrote {path}")


# ── starting runs ──────────────────────────────────────────────────────

def start_verify(command: VerifyCommand) -> None:
    functions = command.functions + (all_readable() if command.all_readable else ())
    run_id, run_dir = new_run("verify")
    start = VerifyState(run_dir=run_dir, functions=tuple(dict.fromkeys(functions)), snapshots=command.snapshots,
                        routes=routes_named(command.routes), battle_code=command.battle_code, trials=command.trials)
    print(f"{run_id} {what_it_checks(start)}")
    print_report(run_verify(start, run_id))


def start_capture(command: CaptureCommand) -> None:
    run_id, run_dir = new_run("capture")
    start = CaptureState(run_dir=run_dir, functions=command.functions, snapshots=SnapshotDir(SNAPSHOTS),
                         candidate_routes=routes_named(command.routes) if command.routes else ALL_ROUTES + LONG_ROUTES,
                         calls_per_function=command.calls)
    print_json(run_capture(start, run_id))


def start_farm(command: FarmCommand) -> None:
    run_id, run_dir = new_run("farm-batch")
    start = FarmBatchState(run_dir=run_dir, functions=command.functions, models=command.models,
                           snapshots=command.snapshots, rounds=command.rounds, jobs=command.jobs,
                           prompt=command.prompt)
    print_outcome(farm_batch_graph().start(start, run_id))


def start_queue(command: QueueCommand) -> None:
    if command.status:
        print("\n".join(ledger_summary(read_ledger())))
        return
    if command.stop:
        STOP.parent.mkdir(parents=True, exist_ok=True)
        STOP.touch()
        print(f"asked the queue to stop ({STOP}): it takes no new jobs and ends once its farm runs have finished")
        return
    with only_one_queue():
        STOP.unlink(missing_ok=True)
        for number in (signal.SIGTERM, signal.SIGINT):   # once: finish the farm runs under way; twice: at once
            _ = signal.signal(number, lambda number, frame: request_stop())
        lanes = tuple(Lane(provider=name, workers=int(workers)) for name, _, workers in
                      (spec.partition("=") for spec in command.lanes)) or LANES
        run_id, run_dir = new_run("queue")
        start = QueueState(run_dir=run_dir, snapshots=SnapshotDir(SNAPSHOTS), lanes=lanes, rounds=command.rounds,
                           cap=command.cap, shift_hours=command.shift_hours, leave_alone=command.leave_alone,
                           until=time.time() + command.hours * 3600 if command.hours else None)
        try:
            print_outcome(queue_graph().start(start, run_id))
        finally:
            STOP.unlink(missing_ok=True)


def start_drafts(command: DraftsCommand) -> None:
    run_id, _ = new_run("drafts")
    start = DraftsState(model=with_prompt(command.model, command.prompt), drafter=command.drafter,
                        functions=command.functions or None,
                        jobs=command.jobs)
    print_json(run_drafts(start, run_id))


def read_only_file(path: Path) -> tuple[FunctionName, ...]:
    """The function names an --only-file lists, one per line (`#` comments and blank lines are
    skipped): the explicit list of a run over a chosen set (an A/B half, say) without repeating
    --only sixty times."""
    names: list[FunctionName] = []
    for line in path.read_text().splitlines():
        text = line.split("#", 1)[0].strip()
        if text:
            names.append(FunctionName(text))
    return tuple(dict.fromkeys(names))


def start_investigate(command: InvestigateCommand) -> None:
    run_id, _ = new_run("investigate")
    run_dir = investigate.INVESTIGATE / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    before = investigate.quota_snapshot(run_dir / "quota-before.json")
    only = command.only + (read_only_file(command.only_file) if command.only_file else ())
    if command.gather:
        print(f"{FACTS_BY_ADDRESS}: runtime facts for the models are on "
              f"({sum(1 for _ in FACTS_BY_ADDRESS.glob('*.json'))} records)")
    start = InvestigateState(run_dir=run_dir, count=command.count, only=only,
                             retry_unnamed=command.retry_unnamed, gather=command.gather,
                             model=command.model, checker=command.checker, jobs=command.jobs)
    try:
        report = run_investigate(start, run_id)
    finally:
        after = investigate.quota_snapshot(run_dir / "quota-after.json")
    print_json(report)
    print(report.totals())
    if before is not None and after is not None:
        spent = before - after
        per = f", {spent / report.selected:.2f} per function" if report.selected else ""
        print(f"hyper units spent: {spent:.2f} ({before:.2f} -> {after:.2f}){per}")


def start_investigate_data(command: InvestigateDataCommand) -> None:
    """The data-label run of `wf investigate`: the candidates go to build/investigate_data/RUN_ID."""
    run_id, _ = new_run("investigate-data")
    run_dir = investigate_data.INVESTIGATE_DATA / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    start = InvestigateDataState(run_dir=run_dir, count=command.count, only=command.only,
                                 model=command.model, checker=command.checker, jobs=command.jobs)
    report = run_investigate_data(start, run_id)
    print_json(report)
    print(report.totals())


def start_intervene(command: InterveneCommand) -> None:
    """`wf intervene`: patch one function, replay baseline+patched, observe anchors."""
    from wf.graphs.intervene import (InterveneState, PROJECT, run_intervene)
    from wf.tools.intervene import (InterventionKind, find_function, pick_route)
    run_id, run_dir = new_run("intervene")
    kind, k = InterventionKind.parse(command.intervention, command.intervention_k)
    route = command.route
    if route == "auto":
        # Pick the shortest route that calls this function (the .edges files).
        finfo = find_function(command.function)
        picked = pick_route(finfo)
        if picked is not None:
            route = str(picked.name)
    start = InterveneState(run_dir=run_dir, function_name=command.function,
                            intervention=InterventionKind.canonical(kind),
                            intervention_k=k,
                            route=route, frames=command.frames)
    final = run_intervene(start, run_id)
    print(f"{run_id} ({command.function}, {command.intervention}, {route}, {command.frames} frames)")
    if final.notes:
        for note in final.notes:
            print(f"  {note}")
    print(f"artifacts: {run_dir / 'report.tsv'}")
    print(f"per-run json: {run_dir / command.function / 'result.json'}")


# How many candidates the random half is drawn from per function wanted. The facts'
# `reached` flag is generous, so a draw of exactly `--random` names can come up short
# of functions a route really enters; the pilot step walks the pool until it has
# `--random` it can run.
RANDOM_POOL_FACTOR = 6


def start_intervene_pilot(command: IntervenePilotCommand) -> None:
    """`wf intervene-pilot`: 24 named functions + N random *reached* functions, each run
    as null + skip + return:0 + return:1 on the shortest route whose facts execute it."""
    from wf.graphs.intervene import (PILOT_INTERVENTIONS, PILOT_NAMED, PilotState,
                                     intervene_pilot_graph)
    from wf.tools.intervene import reached_function_names, select_intervene_targets
    run_id, run_dir = new_run("intervene-pilot")
    selected_named = tuple(PILOT_NAMED)
    selected_random: tuple[FunctionName, ...] = tuple()
    if command.only:
        selected_named = tuple(name for name in command.only if name in PILOT_NAMED)
    random_pool: tuple[FunctionName, ...] = ()
    seed = command.seed
    if seed is None:
        # A stable draw per run: Python's hash() is salted per process, so the same
        # run_id would give a different set of functions on every invocation. The seed
        # is printed with the run, so a table in the notes can be re-drawn exactly.
        seed = int(hashlib.sha256(f"{run_id}|{command.route}".encode()).hexdigest()[:8], 16)
    if command.random:
        # The random tail is drawn from the facts layer's reached set, but as a POOL
        # several times longer than asked for: the facts' `reached` flag is generous, so
        # some draws turn out to have no route that enters them, and the pilot step walks
        # the pool until it has `--random` functions it can really run.
        chosen = select_intervene_targets(
            count=len(selected_named) + command.random * RANDOM_POOL_FACTOR, named=selected_named,
            rng_seed=seed,
            reached=reached_function_names())
        random_pool = tuple(chosen[len(selected_named):])
    start = PilotState(run_dir=run_dir, named=selected_named, random=random_pool,
                       random_pool=random_pool, random_want=command.random,
                       route=command.route, frames=command.frames, jobs=command.jobs)
    state = intervene_pilot_graph().start(start, run_id, tracked=False)
    if isinstance(state, Finished):
        state = state.state
    interventions = " ".join(kind + (f":{k}" if k is not None else "")
                             for kind, k in PILOT_INTERVENTIONS)
    print(f"{run_id}  ({len(selected_named)} named + {len(random_pool)} random candidates, "
          f"{interventions}, {command.frames} frames, seed {seed})")
    print(f"artifacts: {run_dir / 'pilot.tsv'}")
    for note in state.notes:
        print(f"  {note}")


def start_runtime_evidence(command: RuntimeEvidenceCommand) -> None:
    """Gather runtime evidence for the names `wf investigate` proposed (build/runtime/RUN_ID)."""
    run_id, run_dir = new_run("runtime-evidence")
    (run_dir).mkdir(parents=True, exist_ok=True)
    judge_mode = "contrastive" if command.contrastive else JUDGE_MODE_DEFAULT
    start = RuntimeEvidenceState(run_dir=run_dir, count=command.count, only=command.only,
                                 states=command.states, jobs=command.jobs, model=command.model,
                                 judge_mode=judge_mode)
    print(f"selected {command.count} function(s) (states={command.states}, jobs={command.jobs}, "
          f"model={command.model}, judge={judge_mode})")
    report = run_runtime_evidence(start, run_id)
    print_json(report)
    contradictions = run_dir / "contradictions.tsv"
    print(f"\ncontradictions: {run_dir / 'contradictions.tsv'}"
          + (f" (count: {len(contradictions.read_text().splitlines()) - 1})"
             if contradictions.exists() else ""))
    print(f"per-function yaml: {run_dir}/NAME.yaml  (one per selected function)")
    print(f"report: {run_dir / 'report.tsv'}")


def start_facts(command: FactsCommand) -> None:
    """The runtime-facts layer over every lifted function (build/facts; no model is called)."""
    run_id = time.strftime("facts-%Y%m%d-%H%M%S")
    facts_dir = FACTS
    facts_dir.mkdir(parents=True, exist_ok=True)
    print("recording what the original game did at runtime for every lifted function")
    print(f"  output: {facts_dir} (by_address/0x08XXXXXX.json, coverage.tsv, progress.txt)")
    print(f"  states={command.states}, jobs={command.jobs}, limit={command.limit or 'all'}"
          + (f", only={', '.join(command.only)}" if command.only else ""))
    start = FactsState(facts_dir=facts_dir, only=command.only, states=command.states,
                       jobs=command.jobs, limit=command.limit)
    report = run_facts(start, run_id)
    print()
    print(summarise_report(report, report.seconds))


def start_check_names(command: CheckNamesCommand) -> None:
    """`wf check-names`: blind claims, the facts, an intervention, and the gate.

    The batch is `--only` plus a `--from-tsv` selection plus `--count N` random reached names;
    every name in it is paired with another name in it (the swapped control), and the batch's
    calibration decides whether any level is granted at all."""
    import random as _random
    run_id, run_dir = new_run("check-names")
    seed = command.seed if command.seed is not None else _random.randrange(1 << 30)
    start = CheckNamesState(run_dir=run_dir, run_id=run_id, only=command.only,
                            from_tsv=command.from_tsv, tsv_where=command.tsv_where,
                            count=command.count, jobs=command.jobs, model=command.model,
                            frames_window=command.frames_window, seed=seed)
    print(f"{run_id}: checking names against the running game (model {command.model}, "
          f"jobs {command.jobs}, seed {seed})")
    print(f"  output: {run_dir}/report.tsv, {run_dir}/NAME.json, {run_dir}/calibration.md")
    report = run_check_names(start, run_id)
    print()
    print(summarise_check_names(report))
    print(f"\nreport: {run_dir / 'report.tsv'}")
    print(f"calibration: {run_dir / 'calibration.md'}")


def start_convert(command: ConvertCommand) -> None:
    run_id, run_dir = new_run("convert")
    start = ConvertState(run_dir=run_dir, subsystem=command.subsystem, file_name=command.file,
                         functions=command.functions,
                         models=tuple(dict.fromkeys(model for spec in command.models for model in models_for(spec)))
                         or DEFAULT_MODELS,
                         rounds=command.rounds,
                         snapshots=command.snapshots, candidate=command.candidate,
                         farm_model=with_prompt(command.farm_model, command.prompt) if command.farm_model else None,
                         defer_record=command.defer_record, battle_code=command.battle_code)
    print_outcome(convert_graph().start(start, run_id))


def start_record(command: RecordCommand) -> None:
    run_id, run_dir = new_run("record")
    pairs = list(command.renames)
    if command.pending:   # the renames parallel convert runs deferred (they keep their entries' names until now)
        pairs += [(SymbolName(old), SymbolName(new)) for old, new in
                  (line.split() for line in PENDING_RENAMES.read_text().splitlines() if line.strip())]
    start = RecordState(run_dir=run_dir, symbols=(*command.symbols, *(old for old, _ in pairs[len(command.renames):])),
                        renames=tuple(Rename(old=old, new=new) for old, new in pairs),
                        commit_message=command.message_file.read_text(),
                        pending_renames=PENDING_RENAMES if command.pending else None)
    print_outcome(record_graph().start(start, run_id))


def start_vidsync_run(command: VidsyncRunCommand) -> None:
    run_id, run_dir = new_run("vidsync-run")
    start = VidsyncRunState(run_dir=run_dir, deploy=not command.no_deploy, start_from=command.start, run=command.run,
                            replay=command.replay, watch_hours=command.watch_hours, poll_minutes=command.poll_minutes,
                            stall_minutes=command.stall_minutes, pathcheck_minutes=command.pathcheck_minutes,
                            repair=not command.no_repair)
    print_outcome(vidsync_run_graph().start(start, run_id))


def start_vidsync_repair(command: VidsyncRepairCommand) -> None:
    run_id, run_dir = new_run("vidsync-repair")
    start = VidsyncRepairState(run_dir=run_dir, run=command.run, loss=command.loss,
                               models=tuple(dict.fromkeys(model for spec in command.models for model in models_for(spec)))
                               or TIERS[Tier.STRONG], max_attempts=command.max_attempts)
    print_outcome(vidsync_repair_graph().start(start, run_id))


def start_vidsync_ready(command: VidsyncReadyCommand) -> None:
    run_id, run_dir = new_run("vidsync-ready")
    start = VidsyncReadyState(run_dir=run_dir, checkpoint_spec=command.checkpoint, most=command.most,
                              out_dir=command.out_dir, name=command.name, every=command.every,
                              local_ready=command.local, import_wanted=command.import_route)
    print_outcome(vidsync_ready_graph().start(start, run_id))


def start_vidsync_tick(command: VidsyncTickCommand) -> None:
    run_id, run_dir = new_run("vidsync-tick")
    outcome = vidsync_tick_graph().start(VidsyncTickState(run_dir=run_dir, run=command.run), run_id)
    if isinstance(outcome, Paused):
        note_waiting(run_dir, run_id)
    print_outcome(outcome)


def new_run(workflow: str) -> tuple[str, Path]:
    run_id = time.strftime(f"{workflow}-%Y%m%d-%H%M%S")
    taken = 1
    while (RUNS / run_id).exists():   # two runs started in the same second
        taken += 1
        run_id = time.strftime(f"{workflow}-%Y%m%d-%H%M%S-{taken}")
    run_dir = RUNS / run_id
    run_dir.mkdir(parents=True)
    print(f"{run_id}: tool logs in {run_dir}")
    return run_id, run_dir


def routes_named(names: tuple[RouteName, ...]) -> tuple[Route, ...]:
    return tuple(route_named(name) for name in names) if names else ALL_ROUTES


# ── paused runs ────────────────────────────────────────────────────────

def show_waiting(run_id: str) -> None:
    paused = workflow_of_run(run_id).graph().waiting(run_id)
    if paused is None:
        print(f"{run_id} is not waiting for anything")
        return
    print_outcome(paused)


def answer(command: AnswerCommand) -> None:
    graph = workflow_of_run(command.run_id).graph()
    paused = graph.waiting(command.run_id)
    if paused is None:
        raise SystemExit(f"{command.run_id} is not waiting for anything")
    given = paused.answer_type.model_validate_json(command.answer_file.read_text())
    print_outcome(graph.resume(command.run_id, given))


# ── printing ───────────────────────────────────────────────────────────

def print_outcome(outcome: Finished[BaseModel] | Paused) -> None:
    if isinstance(outcome, Paused) and isinstance(outcome.request, ReviewRequest):
        print(f"{outcome.run_id} is waiting for review.\n")
        print(review_sheet(outcome.request))
        return
    if isinstance(outcome, Paused):
        print(f"{outcome.run_id} is waiting at {outcome.step} for a {outcome.answer_type.__name__}.")
        print("It was given:")
        print(outcome.request.model_dump_json(indent=2))
        print(f"Answer with `python -m wf answer {outcome.run_id} ANSWER.json`, where ANSWER.json matches:")
        print(json.dumps(outcome.answer_type.model_json_schema(), indent=2))
        return
    results = outcome.state.model_dump(mode="json", include={"report", "commit", "tallies", "pairs", "outcome", "renames", "totals", "shifts"})
    print(f"{outcome.run_id} finished:")
    print(json.dumps(results, indent=2))


def print_report(report: VerifyReport) -> None:
    dirty = " + uncommitted changes" if report.sources.has_uncommitted_changes else ""
    print(f"verdict: {report.verdict.value} ({report.sources.commit[:10]}{dirty})")
    print(f"  functions compared by the difftests and the oracle: {report.functions_checked}"
          if report.functions_checked else
          "  no functions were given: the difftests and the CPU oracle compared nothing (build and lockstep only)")
    if report.build_failure is not None:
        print(f"  build log: {report.build_failure.log}")
        for line in report.build_failure.last_lines:
            print(f"    {line}")
    if report.failed_functions:
        print(f"  difftest failed: {', '.join(report.failed_functions)}")
    if report.oracle_differences:
        print(f"  the CPU oracle saw differences: {', '.join(report.oracle_differences)}")
    if report.never_compared:
        print(f"  never compared by any check: {', '.join(report.never_compared)}")
    if report.routes_checked:
        print(f"  routes checked: {', '.join(report.routes_checked)}")
    if report.failed_routes:
        print(f"  routes failed: {', '.join(report.failed_routes)}")


def print_json(result: BaseModel) -> None:
    print(result.model_dump_json(indent=2))


def show_status() -> None:
    freeze = install_freeze()
    if freeze is not None:
        print(f"installs frozen since {freeze.since:%m-%d %H:%M} ({freeze.reason}): installs, renames and commits "
              "wait for `wf thaw`")
    for status in all_runs():
        where = f" at {status.step}" if status.step else ""
        detail = f" ({status.detail})" if status.detail else ""
        print(f"{status.last_activity:%m-%d %H:%M}  {status.workflow:10s} {status.run_id:45s} "
              f"{status.phase.value}{where}{detail}")


def show_archive(dry_run: bool) -> None:
    report = archive_cited_runs(dry_run)
    verb = "would copy" if dry_run else "copied"
    print(f"symbols/db cites {report.cited} runs; {verb} {report.files} files ({report.bytes / 1e6:.1f} MB) "
          f"from {len(report.copied_from)} of them")
    print(f"{ARCHIVE}: {report.archive_files} files, {report.archive_bytes / 1e6:.1f} MB")
    if report.nowhere:
        print(f"cited, but in neither build-wf-runs nor the archive: {', '.join(report.nowhere)}")


# ── arguments ──────────────────────────────────────────────────────────

def _intervention_arg(text: str) -> str:
    """argparse `type` for --intervention: one free-form string, validated here.

    The option used to spell its choices out as `["skip", "null"] + [f"return:{k}" for k
    in range(256)]`, which put 258 entries in `--help` for what is three kinds of
    argument. `InterventionKind.parse` is the one validator, so it is what argparse
    calls: a bad value is a usage error naming the three forms, not a wall of choices.
    """
    from wf.tools.intervene import InterventionKind
    text = text.strip()
    try:
        InterventionKind.parse(text)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            f"{error} (three forms: 'skip', 'null', 'return:K' with K in 0..255)") from error
    return text


def build_intervene_parser(commands: argparse._SubParsersAction) -> argparse.ArgumentParser:
    """`wf intervene`: one function, one intervention. Split out of parse_command() so a
    test can read the help text -- `--intervention` used to list all 258 `return:K`
    values there."""
    interv = commands.add_parser("intervene",
                                   help="wf intervene: mGBA intervention runner for causal "
                                        "checks of function names (no model calls)")
    interv.add_argument("--function", required=True,
                         help="a function name or hex address to patch (the THUMB bit is accepted)")
    interv.add_argument("--intervention", type=_intervention_arg, default="skip",
                         metavar="KIND",
                         help="skip | null | return:K (K is 0..255). skip = entry->push {lr}; "
                              "pop {pc} (returns the way the function's own epilogue does, "
                              "so the CPU's instruction set is unchanged); null = rewrite "
                              "the entry with its own bytes (determinism control)")
    interv.add_argument("--route", default="auto",
                         help="a recorded route in tools/traces, or `auto` (default) = the "
                              "shortest recorded route whose facts execute this function")
    interv.add_argument("--frames", type=int, default=120,
                         help="how many game frames to replay baseline+patched from the cached state")
    return interv


def build_intervene_pilot_parser(commands: argparse._SubParsersAction) -> argparse.ArgumentParser:
    """`wf intervene-pilot`: 24 named + N random reached functions, four interventions each."""
    intpilot = commands.add_parser("intervene-pilot",
                                    help="24 named + N random reached functions, null+skip+return:0+"
                                         "return:1 each, each on the shortest route that reaches it")
    intpilot.add_argument("--route", default="auto",
                           help="a recorded route in tools/traces, or `auto` (default) = the "
                                "shortest recorded route whose facts execute each function")
    intpilot.add_argument("--frames", type=int, default=120,
                           help="how many game frames to replay per run (default 120)")
    intpilot.add_argument("--random", type=int, default=24,
                           help="how many random reached functions to add (default 24)")
    intpilot.add_argument("--only", action="append", default=[],
                           help="run only these named functions (repeatable)")
    intpilot.add_argument("--jobs", type=int, default=8,
                           help="how many functions to run at a time (default 8)")
    intpilot.add_argument("--seed", type=int, default=None,
                           help="the seed the random half is drawn with (default: one per "
                                "run id; the seed is printed with the run)")
    return intpilot


def parse_command() -> Command:
    parser = argparse.ArgumentParser(prog="wf", description="The recomp project's workflows.")
    commands = parser.add_subparsers(dest="command", required=True)

    verify = commands.add_parser("verify", help="lift, build, difftest, oracle, lockstep (and battle sweep)")
    verify.add_argument("--function", action="append", default=[], help="a changed function to check")
    verify.add_argument("--all-readable", action="store_true",
                        help="check every readable function (`Next NAME(void)` in cport/src/game), with --function's")
    verify.add_argument("--snapshots", type=Path, help="captured calls of the functions (wf capture: build-snapshots)")
    verify.add_argument("--route", action="append", default=[], help="a route to replay (default: all)")
    verify.add_argument("--battle-code", action="store_true", help="the change touches battle code")
    verify.add_argument("--trials", type=int, default=300, help="random difftest trials per function")

    capture = commands.add_parser("capture", help="save the original game's calls of functions (into build-snapshots)")
    capture.add_argument("--function", action="append", required=True)
    capture.add_argument("--route", action="append", default=[], help="routes to choose from (default: all)")
    capture.add_argument("--calls", type=int, default=40, help="calls to save per function")

    farm = commands.add_parser("farm", help="outside models convert small functions")
    farm.add_argument("functions", nargs="+")
    farm.add_argument("--model", action="append", required=True)
    farm.add_argument("--snapshots", type=Path, required=True)
    farm.add_argument("--rounds", type=int, default=3)
    farm.add_argument("--jobs", type=int, default=4)
    farm.add_argument("--prompt", choices=[kind.value for kind in PromptKind], default=PromptKind.TYPED.value,
                      help="typed (default): typed C with thin adapters, register check on; old: the prompt before "
                           "it (register-form C, no register check; installs are queued for retype)")

    work = commands.add_parser("queue", help="one queue keeps every provider with quota busy on what is left to farm")
    work.add_argument("--hours", type=float, help="take no new jobs after this many hours (default: until stopped)")
    work.add_argument("--lane", action="append", default=[],
                      help="PROVIDER=WORKERS, repeatable (default: hyper=16 minimax=4 zai=2 opencode=2)")
    work.add_argument("--cap", type=int, default=CAP, help="tokens a function's job may spend over all its models")
    work.add_argument("--rounds", type=int, default=1, help="repairs per model (then the next model)")
    work.add_argument("--shift-hours", type=float, default=3.0, help="rebuild the difftest and plan again this often")
    work.add_argument("--leave-alone", type=Path, action="append", default=[],
                      help="a file of functions (one per line) another driver will farm: never taken (re-read each plan)")
    work.add_argument("--stop", action="store_true", help="ask the running queue to finish its farm runs and end")
    work.add_argument("--status", action="store_true", help="the jobs' ledger: by state, band and next model")

    drafts = commands.add_parser("drafts", help="a farm model drafts symbol entries for its accepted candidates")
    drafts.add_argument("functions", nargs="*")
    drafts.add_argument("--model", required=True, help="whose accepted candidates")
    drafts.add_argument("--drafter", help="who writes the entries (default: --model; e.g. minimax/MiniMax-M3)")
    drafts.add_argument("--jobs", type=int, default=16)
    drafts.add_argument("--prompt", choices=[kind.value for kind in PromptKind], default=PromptKind.TYPED.value,
                        help="whose candidates: the model's under this prompt (old: filed as MODEL+old)")

    look = commands.add_parser("investigate",
                               help="cheap models propose candidate symbols/db entries for placeholder functions")
    look.add_argument("--count", type=int, default=DEFAULT_INVESTIGATE_COUNT,
                      help=f"how many functions to take (default {DEFAULT_INVESTIGATE_COUNT})")
    look.add_argument("--only", action="append", default=[],
                      help="investigate these functions, whatever else (an earlier candidate does "
                           "not rule a named function out)")
    look.add_argument("--only-file", type=Path,
                      help="a file of function names, one per line, to investigate like --only "
                           "(# comments allowed): the explicit list of a run over a chosen set")
    look.add_argument("--facts", dest="gather", action="store_true", default=False,
                      help="show the models what the original game did at runtime, from `wf facts` "
                           "(the scenes it runs in, its runtime callers, the RAM and I/O it writes, "
                           "its arguments and returns), and require the proposed name to fit")
    look.add_argument("--no-facts", dest="gather", action="store_false",
                      help="do not show the runtime facts (default)")
    look.add_argument("--retry-unnamed", help="an earlier run's ID: re-ask the functions its proposals left "
                                              "without a name (their new candidates go to the new run folder)")
    look.add_argument("--model", default=DEFAULT_INVESTIGATE_MODEL, help="who proposes (default hyper/glm-5.3-flash)")
    look.add_argument("--checker", default=DEFAULT_INVESTIGATE_CHECKER,
                      help="who cross-checks (default hyper/qwen3.8-flash)")
    look.add_argument("--jobs", type=int, default=16)

    data = commands.add_parser("investigate-data",
                               help="cheap models propose candidate symbols/db entries for "
                                    "placeholder DATA labels, read from the code that uses them")
    data.add_argument("--count", type=int, default=DEFAULT_INVESTIGATE_DATA_COUNT,
                      help=f"how many labels to take (default {DEFAULT_INVESTIGATE_DATA_COUNT})")
    data.add_argument("--only", action="append", default=[],
                      help="investigate these data labels, whatever else")
    data.add_argument("--model", default=DEFAULT_INVESTIGATE_DATA_MODEL,
                      help=f"who proposes (default {DEFAULT_INVESTIGATE_DATA_MODEL})")
    data.add_argument("--checker", default=DEFAULT_INVESTIGATE_DATA_CHECKER,
                      help=f"who cross-checks (default {DEFAULT_INVESTIGATE_DATA_CHECKER})")
    data.add_argument("--jobs", type=int, default=DEFAULT_INVESTIGATE_DATA_JOBS)

    build_intervene_parser(commands)
    build_intervene_pilot_parser(commands)

    runtime = commands.add_parser("runtime-evidence",
                                  help="gather runtime evidence for the names `wf investigate` proposed "
                                       "(build/runtime/RUN_ID; nothing under symbols/db is touched)")
    runtime.add_argument("--count", type=int, default=DEFAULT_RE_COUNT,
                         help=f"how many functions to take (default {DEFAULT_RE_COUNT})")
    runtime.add_argument("--only", action="append", default=[],
                         help="runtime-evidence these functions, whatever else (re-judges cached facts)")
    runtime.add_argument("--states", type=int, default=DEFAULT_RE_STATES,
                         help=f"entry states per function (default {DEFAULT_RE_STATES})")
    runtime.add_argument("--jobs", type=int, default=DEFAULT_RE_JOBS,
                         help=f"mgba_run calls in flight (default {DEFAULT_RE_JOBS})")
    runtime.add_argument("--model", default=DEFAULT_RE_MODEL,
                         help=f"the judge model (default {DEFAULT_RE_MODEL})")
    runtime.add_argument("--contrastive", action="store_true",
                         help="use the contrastive (multiple-choice) judge instead of the legacy "
                              "consistent/contradicts/unknown prompt; calibration on 60 of the "
                              "runtime-evidence-20261019-180951 batch's functions gave (a) 33.3% "
                              "accuracy (95% CI [22.7%, 45.9%]) and (b) 40.0% false-positive rate "
                              "(95% CI [28.6%, 52.6%]); the brief requires both bars clearly "
                              "cleared, so the legacy judge remains the default")

    facts_parser = commands.add_parser("facts",
                                       help="what the original game did at runtime, for every lifted "
                                            "function, from the recordings alone (build/facts; no model "
                                            "is called)")
    facts_parser.add_argument("--only", action="append", default=[],
                              help="these functions (name or 0x address), whatever else")
    facts_parser.add_argument("--states", type=int, default=DEFAULT_FACTS_STATES,
                              help=f"captured entry states replayed per function (default {DEFAULT_FACTS_STATES})")
    facts_parser.add_argument("--jobs", type=int, default=DEFAULT_FACTS_JOBS,
                              help=f"functions replayed at once (default {DEFAULT_FACTS_JOBS})")
    facts_parser.add_argument("--limit", type=int, default=DEFAULT_FACTS_LIMIT,
                              help=f"the first N functions by address (default {DEFAULT_FACTS_LIMIT}: all)")

    check_names = commands.add_parser(
        "check-names",
        help="check a function's name against the running game: claims written blind, checked "
             "against build/facts, a prediction run through wf intervene, and a calibration gate")
    check_names.add_argument("--only", action="append", default=[],
                             help="these functions, whatever else (name or 0x address)")
    check_names.add_argument("--from-tsv", type=Path,
                             help="a TSV with a `name` column: take its names (e.g. the name "
                                  "audit at docs/reviews/2026-10-21-minimax-name-audit.tsv)")
    check_names.add_argument("--tsv-where", action="append", default=[], metavar="KEY=VALUE",
                             help="keep only the TSV rows whose column matches, e.g. "
                                  "--tsv-where kind=function --tsv-where verdict=wrong")
    check_names.add_argument("--count", type=int, default=DEFAULT_CN_COUNT,
                             help=f"how many random reached functions to add to the batch "
                                  f"(default {DEFAULT_CN_COUNT})")
    check_names.add_argument("--jobs", type=int, default=DEFAULT_CN_JOBS,
                             help=f"model calls / intervened runs in flight (default {DEFAULT_CN_JOBS})")
    check_names.add_argument("--model", default=DEFAULT_CN_MODEL,
                             help=f"who writes the blind claims (default {DEFAULT_CN_MODEL})")
    check_names.add_argument("--frames-window", type=int, default=DEFAULT_CN_WINDOW,
                             help=f"how far into a route to hunt for a function's first call "
                                  f"(default {DEFAULT_CN_WINDOW} game frames)")
    check_names.add_argument("--seed", type=int, default=None,
                             help="the batch's random seed (default: from the run id; pass one "
                                  "to repeat a batch)")

    convert = commands.add_parser("convert", help="a strong model converts a subsystem; stops for review")
    convert.add_argument("functions", nargs="+")
    convert.add_argument("--subsystem", required=True, help="what the functions do together, in a few words")
    convert.add_argument("--file", required=True, help="the new file under cport/src/game, e.g. bg_redraw.c")
    convert.add_argument("--model", action="append", default=[],
                         help="the models in order of preference; a tier stands for its models (default: tier/strong = "
                              + ", ".join(DEFAULT_MODELS) + ")")
    convert.add_argument("--rounds", type=int, default=3, help="attempts per model")
    convert.add_argument("--snapshots", type=Path, help="reuse captured calls instead of capturing")
    convert.add_argument("--candidate", type=Path, help="a ready file to try first")
    convert.add_argument("--farm-model", help="try the farm's accepted candidates by this model first (with their drafts)")
    convert.add_argument("--defer-record", action="store_true",
                         help="commit the entries, leave renames and bn6f comments to one batch record (parallel runs)")
    convert.add_argument("--battle-code", action="store_true", help="the subsystem is battle code (battle sweep)")
    convert.add_argument("--prompt", choices=[kind.value for kind in PromptKind], default=PromptKind.TYPED.value,
                         help="with --farm-model: its candidates under this prompt (old: register form allowed, the "
                              "install queues them for retype in tools/abi/retype_queue.tsv)")

    record = commands.add_parser("record", help="symbols/db, renames and comments into bn6f, commit")
    record.add_argument("--symbol", action="append", default=[])
    record.add_argument("--rename", action="append", nargs=2, default=[], metavar=("OLD", "NEW"))
    record.add_argument("--pending", action="store_true",
                        help="also make the renames convert runs deferred (symbols/pending_renames.txt), emptying it")
    record.add_argument("--message-file", type=Path, required=True)

    vrun = commands.add_parser("vidsync-run", help="deploy vidsync to the compute PCs, (re)start the driver, watch it")
    vrun.add_argument("--start", help="restart the driver from this checkpoint (data/auto/RUN.N/fix_0003, without "
                                      ".json) or a run's newest (its name); default: watch the running driver")
    vrun.add_argument("--run", help="the new run's name (default: the next after the last run's, run7k -> run7l)")
    vrun.add_argument("--no-deploy", action="store_true", help="do not deploy (nor restart the worker server)")
    vrun.add_argument("--replay", help="VIDSYNC_REPLAY for the driver: a result/loss json whose path is stepped as it is")
    vrun.add_argument("--watch-hours", type=float, help="end the watch after this long (default: when stuck or ended)")
    vrun.add_argument("--poll-minutes", type=float, default=5)
    vrun.add_argument("--stall-minutes", type=float, default=30, help="stuck: the furthest frame has not moved this long")
    vrun.add_argument("--pathcheck-minutes", type=float, default=60)
    vrun.add_argument("--no-repair", action="store_true", help="when stuck, report instead of opening vidsync-repair")

    vrepair = commands.add_parser("vidsync-repair", help="a new repair handler for a loss the driver is stuck on")
    vrepair.add_argument("--run", required=True, help="the stuck run's name (wf vidsync-status)")
    vrepair.add_argument("--loss", type=int, help="the loss's game frame (default: the run's newest)")
    vrepair.add_argument("--model", action="append", default=[], help="models in order (default: tier/strong)")
    vrepair.add_argument("--max-attempts", type=int, default=4)

    vready = commands.add_parser("vidsync-ready", help="export the sync, cut segments, copy here (and import the route)")
    vready.add_argument("--checkpoint", help="a checkpoint path, or a run's name for its newest (default: the last run's)")
    vready.add_argument("--most", type=int, help="the run's newest checkpoint at most this game frame")
    vready.add_argument("--out-dir", default="data/ready", help="on the driver PC (default data/ready)")
    vready.add_argument("--name", default="bn6f_beastsp")
    vready.add_argument("--every", type=int, default=20000, help="a segment about every N game frames")
    vready.add_argument("--local", type=Path, help="copy it here (default: build-vidsync/ready)")
    vready.add_argument("--import", action="store_true",
                        help="then tools/import_route.py and lockstep the port on beastsp (replays it here)")

    vtick = commands.add_parser("vidsync-tick", help="one check-in on the sync: act on routine cases, stop at decide")
    vtick.add_argument("--run", help="the run to look at (default: the one started last)")

    vstatus = commands.add_parser("vidsync-status", help="the driver's state on the driver PC")
    vstatus.add_argument("run", nargs="?")

    waiting = commands.add_parser("waiting", help="what a paused run needs")
    waiting.add_argument("run_id")

    answer_parser = commands.add_parser("answer", help="answer a paused run and carry on")
    answer_parser.add_argument("run_id")
    answer_parser.add_argument("answer_file", type=Path)

    retry = commands.add_parser("retry", help="carry on a failed or interrupted run from where it stopped")
    retry.add_argument("run_id")

    commands.add_parser("status", help="where every run is")

    freeze = commands.add_parser("freeze", help="installs, renames and commits wait (models go on), for structural moves")
    freeze.add_argument("--reason", required=True, help="what the window is for, e.g. 'the db rewrite'")

    commands.add_parser("thaw", help="end the install freeze")

    archive = commands.add_parser("archive", help="copy the text logs of the runs symbols/db cites to build-wf-archive")
    archive.add_argument("--dry-run", action="store_true", help="only say what would be copied")

    docs = commands.add_parser("docs", help="regenerate docs/workflows from the code")
    docs.add_argument("out_dir", type=Path, nargs="?", default=REPO / "docs/workflows")

    given: dict[str, object] = dict(vars(parser.parse_args()))
    for name, value in given.items():   # paths as given where wf was started; the tools run elsewhere (cport/)
        if isinstance(value, Path):
            given[name] = value.resolve()
    return TypeAdapter[Command](Command).validate_python(given)


if __name__ == "__main__":
    main()
