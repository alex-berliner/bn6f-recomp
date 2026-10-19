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
  python -m wf investigate [--count N] [--only NAME]... [--retry-unnamed RUN_ID] [--model M] [--checker M] [--jobs N]
                       # cheap models propose candidate symbols/db entries for placeholder functions;
                       # the candidates (for review, nothing installed) go to build/investigate/RUN_ID
  (a model can be a tier: tier/cheap, tier/strong; each request goes to the first of its sources with quota)
  python -m wf convert --subsystem WHAT --file NAME.c [--model P/M]... [--rounds N] [--snapshots DIR]
                       [--candidate FILE | --farm-model P/M [--prompt old]] [--defer-record] [--battle-code] FUNCTION...
  (--prompt old: the farm's old-prompt candidates, filed as P/M+old; a convert run installing them queues
   them for retype, tools/abi/retype_queue.tsv)
  python -m wf record  [--symbol NAME]... [--rename OLD NEW]... [--pending] --message-file FILE
  python -m wf vidsync-run [--start CHECKPOINT|RUN] [--run NAME] [--no-deploy] [--replay PATH] [--watch-hours H]
                       [--stall-minutes 30] [--pathcheck-minutes 60] [--no-repair]   # docs/vidsync/README.md
  python -m wf vidsync-repair --run NAME [--loss G] [--model P/M]... [--max-attempts N]
  python -m wf vidsync-ready [--checkpoint PATH|RUN] [--most G] [--out-dir DIR] [--every N] [--local DIR] [--import]
  python -m wf vidsync-status [RUN]        # the driver's state on 192.168.1.175 (furthest frame, last event)
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
from wf.graphs.vidsync_ready import VidsyncReadyState, vidsync_ready_graph
from wf.graphs.vidsync_repair import VidsyncRepairState, vidsync_repair_graph
from wf.graphs.vidsync_run import VidsyncRunState, vidsync_run_graph
from wf.graphs.vidsync_tick import VidsyncTickState, note_waiting, vidsync_tick_graph
from wf.tools.sources import TIERS, Tier
from wf.tools import investigate
from wf.tools.vidsync import driver_status
from wf.tools.investigate import DEFAULT_CHECKER as DEFAULT_INVESTIGATE_CHECKER
from wf.tools.investigate import DEFAULT_MODEL as DEFAULT_INVESTIGATE_MODEL
from wf.tools.investigate import DEFAULT_COUNT as DEFAULT_INVESTIGATE_COUNT
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
    retry_unnamed: str | None = None
    model: ModelSpec
    checker: ModelSpec
    jobs: int


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


Command = (VerifyCommand | CaptureCommand | FarmCommand | QueueCommand | DraftsCommand | InvestigateCommand
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


def start_investigate(command: InvestigateCommand) -> None:
    run_id, _ = new_run("investigate")
    run_dir = investigate.INVESTIGATE / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    before = investigate.quota_snapshot(run_dir / "quota-before.json")
    start = InvestigateState(run_dir=run_dir, count=command.count, only=command.only,
                             retry_unnamed=command.retry_unnamed, model=command.model,
                             checker=command.checker, jobs=command.jobs)
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
    look.add_argument("--only", action="append", default=[], help="investigate these functions, whatever else")
    look.add_argument("--retry-unnamed", help="an earlier run's ID: re-ask the functions its proposals left "
                                              "without a name (their new candidates go to the new run folder)")
    look.add_argument("--model", default=DEFAULT_INVESTIGATE_MODEL, help="who proposes (default hyper/glm-5.3-flash)")
    look.add_argument("--checker", default=DEFAULT_INVESTIGATE_CHECKER,
                      help="who cross-checks (default hyper/qwen3.8-flash)")
    look.add_argument("--jobs", type=int, default=16)

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
