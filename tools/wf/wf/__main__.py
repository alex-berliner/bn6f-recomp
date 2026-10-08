"""Start workflows, answer the ones waiting at a pause point, see where every
run is, and regenerate the docs.

  python -m wf verify  [--function NAME]... [--snapshots DIR] [--route NAME]... [--battle-code] [--trials N]
  python -m wf capture --function NAME... [--route NAME]... [--calls N]
  python -m wf farm    --model PROVIDER/MODEL... --snapshots DIR [--rounds N] [--jobs N] FUNCTION...
  python -m wf convert --subsystem NAME FUNCTION...
  python -m wf record  --symbol NAME... [--rename OLD NEW]... --message-file FILE
  python -m wf waiting RUN_ID              # what a paused run needs
  python -m wf answer  RUN_ID ANSWER.json  # give it, and carry on
  python -m wf retry   RUN_ID              # carry on a failed or interrupted run from where it stopped
  python -m wf status
  python -m wf docs [OUT_DIR]

Every run is tracked for the Burr UI (`tools/wf/.venv/bin/burr --no-open
--no-copy-demo_data --host 0.0.0.0`, port 7241); tool logs go to
build-wf-runs/RUN_ID.
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
from wf.graphs.convert import ConvertState, convert_graph
from wf.graphs.farm import FarmBatchState, farm_batch_graph
from wf.graphs.record import RecordState, record_graph
from wf.graphs.verify import VerifyReport, VerifyState, run_verify
from wf.repo import REPO, RUNS
from wf.routes import ALL_ROUTES, route_named
from wf.status import all_runs, workflow_of_run
from wf.types import FunctionName, ModelSpec, Rename, Route, RouteName, SnapshotDir, SymbolName


class VerifyCommand(BaseModel):
    command: Literal["verify"]
    functions: tuple[FunctionName, ...] = Field(alias="function")
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


class ConvertCommand(BaseModel):
    command: Literal["convert"]
    subsystem: str
    functions: tuple[FunctionName, ...]


class RecordCommand(BaseModel):
    command: Literal["record"]
    symbols: tuple[SymbolName, ...] = Field(alias="symbol")
    renames: tuple[tuple[SymbolName, SymbolName], ...] = Field(alias="rename")
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


class DocsCommand(BaseModel):
    command: Literal["docs"]
    out_dir: Path


Command = (VerifyCommand | CaptureCommand | FarmCommand | ConvertCommand | RecordCommand
           | WaitingCommand | AnswerCommand | RetryCommand | StatusCommand | DocsCommand)


def main() -> None:
    command = parse_command()
    match command:
        case VerifyCommand():
            start_verify(command)
        case CaptureCommand():
            start_capture(command)
        case FarmCommand():
            start_farm(command)
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
        case DocsCommand():
            for path in write_docs(command.out_dir):
                print(f"wrote {path}")


# ── starting runs ──────────────────────────────────────────────────────

def start_verify(command: VerifyCommand) -> None:
    run_id, run_dir = new_run("verify")
    start = VerifyState(run_dir=run_dir, functions=command.functions, snapshots=command.snapshots,
                        routes=routes_named(command.routes), battle_code=command.battle_code, trials=command.trials)
    print_report(run_verify(start, run_id))


def start_capture(command: CaptureCommand) -> None:
    run_id, run_dir = new_run("capture")
    start = CaptureState(run_dir=run_dir, functions=command.functions, snapshots=SnapshotDir(run_dir / "snapshots"),
                         candidate_routes=routes_named(command.routes), calls_per_function=command.calls)
    print_json(run_capture(start, run_id))


def start_farm(command: FarmCommand) -> None:
    run_id, run_dir = new_run("farm-batch")
    start = FarmBatchState(run_dir=run_dir, functions=command.functions, models=command.models,
                           snapshots=command.snapshots, rounds=command.rounds, jobs=command.jobs)
    print_outcome(farm_batch_graph().start(start, run_id))


def start_convert(command: ConvertCommand) -> None:
    run_id, run_dir = new_run("convert")
    start = ConvertState(run_dir=run_dir, subsystem=command.subsystem, functions=command.functions)
    print_outcome(convert_graph().start(start, run_id))


def start_record(command: RecordCommand) -> None:
    run_id, run_dir = new_run("record")
    start = RecordState(run_dir=run_dir, symbols=command.symbols,
                        renames=tuple(Rename(old=old, new=new) for old, new in command.renames),
                        commit_message=command.message_file.read_text())
    print_outcome(record_graph().start(start, run_id))


def new_run(workflow: str) -> tuple[str, Path]:
    run_id = time.strftime(f"{workflow}-%Y%m%d-%H%M%S")
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
    if isinstance(outcome, Paused):
        print(f"{outcome.run_id} is waiting at {outcome.step} for a {outcome.answer_type.__name__}.")
        print("It was given:")
        print(outcome.request.model_dump_json(indent=2))
        print(f"Answer with `python -m wf answer {outcome.run_id} ANSWER.json`, where ANSWER.json matches:")
        print(json.dumps(outcome.answer_type.model_json_schema(), indent=2))
        return
    results = outcome.state.model_dump(mode="json", include={"report", "commit", "tallies", "pairs"})
    print(f"{outcome.run_id} finished:")
    print(json.dumps(results, indent=2))


def print_report(report: VerifyReport) -> None:
    dirty = " + uncommitted changes" if report.sources.has_uncommitted_changes else ""
    print(f"verdict: {report.verdict.value} ({report.sources.commit[:10]}{dirty})")
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
    for status in all_runs():
        where = f" at {status.step}" if status.step else ""
        detail = f" ({status.detail})" if status.detail else ""
        print(f"{status.last_activity:%m-%d %H:%M}  {status.workflow:10s} {status.run_id:45s} "
              f"{status.phase.value}{where}{detail}")


# ── arguments ──────────────────────────────────────────────────────────

def parse_command() -> Command:
    parser = argparse.ArgumentParser(prog="wf", description="The recomp project's workflows.")
    commands = parser.add_subparsers(dest="command", required=True)

    verify = commands.add_parser("verify", help="lift, build, difftest, oracle, lockstep (and battle sweep)")
    verify.add_argument("--function", action="append", default=[], help="a changed function to check")
    verify.add_argument("--snapshots", type=Path, help="captured calls of the functions (wf capture)")
    verify.add_argument("--route", action="append", default=[], help="a route to replay (default: all)")
    verify.add_argument("--battle-code", action="store_true", help="the change touches battle code")
    verify.add_argument("--trials", type=int, default=300, help="random difftest trials per function")

    capture = commands.add_parser("capture", help="save the original game's calls of functions")
    capture.add_argument("--function", action="append", required=True)
    capture.add_argument("--route", action="append", default=[], help="routes to choose from (default: all)")
    capture.add_argument("--calls", type=int, default=40, help="calls to save per function")

    farm = commands.add_parser("farm", help="outside models convert small functions")
    farm.add_argument("functions", nargs="+")
    farm.add_argument("--model", action="append", required=True)
    farm.add_argument("--snapshots", type=Path, required=True)
    farm.add_argument("--rounds", type=int, default=3)
    farm.add_argument("--jobs", type=int, default=4)

    convert = commands.add_parser("convert", help="Claude converts a subsystem (stops for Claude to write it)")
    convert.add_argument("functions", nargs="+")
    convert.add_argument("--subsystem", required=True)

    record = commands.add_parser("record", help="symbols/db, renames and comments into bn6f, commit")
    record.add_argument("--symbol", action="append", default=[])
    record.add_argument("--rename", action="append", nargs=2, default=[], metavar=("OLD", "NEW"))
    record.add_argument("--message-file", type=Path, required=True)

    waiting = commands.add_parser("waiting", help="what a paused run needs")
    waiting.add_argument("run_id")

    answer_parser = commands.add_parser("answer", help="answer a paused run and carry on")
    answer_parser.add_argument("run_id")
    answer_parser.add_argument("answer_file", type=Path)

    retry = commands.add_parser("retry", help="carry on a failed or interrupted run from where it stopped")
    retry.add_argument("run_id")

    commands.add_parser("status", help="where every run is")

    docs = commands.add_parser("docs", help="regenerate docs/workflows from the code")
    docs.add_argument("out_dir", type=Path, nargs="?", default=REPO / "docs/workflows")

    return TypeAdapter[Command](Command).validate_python(vars(parser.parse_args()))


if __name__ == "__main__":
    main()
