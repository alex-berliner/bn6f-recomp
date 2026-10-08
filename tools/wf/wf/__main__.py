"""Start workflows and draw them.

  python -m wf verify [--function NAME ...] [--route NAME ...] [--trials N]
  python -m wf diagram OUT_DIR

Runs are tracked for the Burr UI (`burr --no-open`, http://localhost:7241).
"""
import argparse
import time
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, Field, TypeAdapter

from wf.graphs.verify import PROJECT, VerifyReport, VerifyState, run_verify, verify_application
from wf.repo import RUNS
from wf.routes import ALL_ROUTES, route_named
from wf.types import FunctionName, RouteName


class VerifyCommand(BaseModel):
    command: Literal["verify"]
    functions: tuple[FunctionName, ...] = Field(alias="function")
    routes: tuple[RouteName, ...] = Field(alias="route")
    trials: int


class DiagramCommand(BaseModel):
    command: Literal["diagram"]
    out_dir: Path


Command = VerifyCommand | DiagramCommand


def main() -> None:
    command = parse_command()
    if isinstance(command, VerifyCommand):
        start_verify(command)
    else:
        draw_diagrams(command.out_dir)


def parse_command() -> Command:
    parser = argparse.ArgumentParser(prog="wf")
    commands = parser.add_subparsers(dest="command", required=True)

    verify = commands.add_parser("verify", help="build, difftest and lockstep the main checkout")
    verify.add_argument("--function", action="append", default=[], help="a changed function to difftest")
    verify.add_argument("--route", action="append", default=[], help="a route to replay (default: all)")
    verify.add_argument("--trials", type=int, default=300, help="random difftest trials per function")

    diagram = commands.add_parser("diagram", help="draw each workflow as a PNG")
    diagram.add_argument("out_dir", type=Path)

    return TypeAdapter[Command](Command).validate_python(vars(parser.parse_args()))


def start_verify(command: VerifyCommand) -> None:
    run_id = time.strftime("verify-%Y%m%d-%H%M%S")
    run_dir = RUNS / run_id
    run_dir.mkdir(parents=True)
    start = VerifyState(
        run_dir=run_dir,
        functions=command.functions,
        routes=tuple(route_named(name) for name in command.routes) if command.routes else ALL_ROUTES,
        trials=command.trials,
    )
    print(f"{run_id}: watch it in the Burr UI, project {PROJECT}; tool logs in {run_dir}")
    print_report(run_verify(start, run_id))


def print_report(report: VerifyReport) -> None:
    dirty = " + uncommitted changes" if report.sources.has_uncommitted_changes else ""
    print(f"verdict: {report.verdict.value} ({report.sources.commit[:10]}{dirty})")
    if report.failed_functions:
        print(f"  difftest failed: {', '.join(report.failed_functions)}")
    if report.unchecked_functions:
        print(f"  never compared (all trials out of domain): {', '.join(report.unchecked_functions)}")
    if report.routes_checked:
        print(f"  routes checked: {', '.join(report.routes_checked)}")
    if report.failed_routes:
        print(f"  routes failed: {', '.join(report.failed_routes)}")


def draw_diagrams(out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    example = VerifyState(run_dir=out_dir, functions=(), routes=(), trials=0)
    verify_application(example, run_id="diagram", tracked=False).draw(out_dir / "verify")
    print(f"wrote {out_dir / 'verify.png'}")


if __name__ == "__main__":
    main()
