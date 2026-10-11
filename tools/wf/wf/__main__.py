"""Start workflows, answer the ones waiting at a pause point, see where every
run is, and regenerate the docs. From anywhere in the repo, `./wf ARGS` (the
launcher at the root) is `python -m wf ARGS` with this folder's venv.

  python -m wf verify  [--function NAME]... [--all-readable] [--snapshots DIR] [--route NAME]... [--battle-code]
                       [--trials N]   # it says first what it will compare: no functions, no difftest or oracle
  python -m wf capture --function NAME... [--route NAME]... [--calls N]   # into build-snapshots/NAME
  python -m wf farm    --model PROVIDER/MODEL... --snapshots DIR [--rounds N] [--jobs N] [--prompt typed|old] FUNCTION...
  python -m wf drafts  --model PROVIDER/MODEL [--drafter P/M] [--jobs N] [FUNCTION...]   # entries for accepted candidates
  (a model can be a tier: tier/cheap, tier/strong; each request goes to the first of its sources with quota)
  python -m wf convert --subsystem WHAT --file NAME.c [--model P/M]... [--rounds N] [--snapshots DIR]
                       [--candidate FILE | --farm-model P/M] [--defer-record] [--battle-code] FUNCTION...
  python -m wf record  [--symbol NAME]... [--rename OLD NEW]... [--pending] --message-file FILE
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
from wf.graphs.record import RecordState, record_graph
from wf.graphs.verify import VerifyReport, VerifyState, all_readable, run_verify, what_it_checks
from wf.repo import REPO, RUNS, SNAPSHOTS
from wf.tools.archive import ARCHIVE, archive_cited_runs
from wf.tools.locks import freeze_installs, install_freeze, thaw_installs
from wf.tools.models import PromptKind
from wf.tools.sources import models_for
from wf.routes import ALL_ROUTES, route_named
from wf.status import all_runs, workflow_of_run
from wf.types import FunctionName, ModelSpec, Rename, Route, RouteName, SnapshotDir, SymbolName


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


class DraftsCommand(BaseModel):
    command: Literal["drafts"]
    model: ModelSpec
    drafter: ModelSpec | None = None
    functions: tuple[FunctionName, ...]
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


class RecordCommand(BaseModel):
    command: Literal["record"]
    symbols: tuple[SymbolName, ...] = Field(alias="symbol")
    renames: tuple[tuple[SymbolName, SymbolName], ...] = Field(alias="rename")
    pending: bool
    message_file: Path


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


Command = (VerifyCommand | CaptureCommand | FarmCommand | DraftsCommand | ConvertCommand | RecordCommand
           | WaitingCommand | AnswerCommand | RetryCommand | StatusCommand | FreezeCommand | ThawCommand
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
        case DraftsCommand():
            start_drafts(command)
        case ConvertCommand():
            start_convert(command)
        case RecordCommand():
            start_record(command)
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
                         candidate_routes=routes_named(command.routes), calls_per_function=command.calls)
    print_json(run_capture(start, run_id))


def start_farm(command: FarmCommand) -> None:
    run_id, run_dir = new_run("farm-batch")
    start = FarmBatchState(run_dir=run_dir, functions=command.functions, models=command.models,
                           snapshots=command.snapshots, rounds=command.rounds, jobs=command.jobs,
                           prompt=command.prompt)
    print_outcome(farm_batch_graph().start(start, run_id))


def start_drafts(command: DraftsCommand) -> None:
    run_id, _ = new_run("drafts")
    start = DraftsState(model=command.model, drafter=command.drafter, functions=command.functions or None,
                        jobs=command.jobs)
    print_json(run_drafts(start, run_id))


def start_convert(command: ConvertCommand) -> None:
    run_id, run_dir = new_run("convert")
    start = ConvertState(run_dir=run_dir, subsystem=command.subsystem, file_name=command.file,
                         functions=command.functions,
                         models=tuple(dict.fromkeys(model for spec in command.models for model in models_for(spec)))
                         or DEFAULT_MODELS,
                         rounds=command.rounds,
                         snapshots=command.snapshots, candidate=command.candidate, farm_model=command.farm_model,
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
    results = outcome.state.model_dump(mode="json", include={"report", "commit", "tallies", "pairs", "outcome", "renames"})
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

    drafts = commands.add_parser("drafts", help="a farm model drafts symbol entries for its accepted candidates")
    drafts.add_argument("functions", nargs="*")
    drafts.add_argument("--model", required=True, help="whose accepted candidates")
    drafts.add_argument("--drafter", help="who writes the entries (default: --model; e.g. minimax/MiniMax-M3)")
    drafts.add_argument("--jobs", type=int, default=16)

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

    record = commands.add_parser("record", help="symbols/db, renames and comments into bn6f, commit")
    record.add_argument("--symbol", action="append", default=[])
    record.add_argument("--rename", action="append", nargs=2, default=[], metavar=("OLD", "NEW"))
    record.add_argument("--pending", action="store_true",
                        help="also make the renames convert runs deferred (symbols/pending_renames.txt), emptying it")
    record.add_argument("--message-file", type=Path, required=True)

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
