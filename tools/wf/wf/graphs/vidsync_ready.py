"""Turn the vidsync sync's newest checkpoint into the `beastsp` route (docs/vidsync/README.md): on the driver
PC, export it as a route-harness recording verified with mgba_run, and cut it into save-file segments (one
about every 20,000 game frames, each checked equal to the run); check the segments; copy the recording and
the segments here (build-vidsync/ready). With --import, tools/import_route.py brings it into
tools/traces/beastsp.* (checkpoints, profiles, coverage, the route's length) and the port is lockstepped on
the longer route; the import replays the whole route here, so it is asked for, not the default."""
import re
import subprocess
from pathlib import Path

from pydantic import BaseModel, TypeAdapter

from wf.burr_typed import Finished, Graph, Transition, Workflow, current_run_id
from wf.graphs.verify import VerifyReport, VerifyState, run_verify
from wf.repo import REPO, TRACES, VIDSYNC_READY
from wf.tools.vidsync import (DRIVER_PC, Checkpoint, JobResult, driver_status, fetch, newest_checkpoint, on_pc,
                              resolve_checkpoint, run_job)
from wf.types import FrameCount, LogPath, RemotePath, Result, Route, RouteName

PROJECT = "recomp-vidsync"
ROUTES_PY = REPO / "tools/wf/wf/routes.py"
SEGMENT_FILES = ("beastsp_seg_*", "segments.json", "VERIFY.txt")


def vidsync_ready_graph() -> Graph["VidsyncReadyState"]:
    return workflow.graph(
        first=pick_checkpoint,
        ends=[write_report],
        transitions=[
            Transition(after=pick_checkpoint, go_to=export_recording),
            Transition(after=export_recording, go_to=cut_segments, when=exported),
            Transition(after=export_recording, go_to=write_report, when=export_failed),
            Transition(after=cut_segments, go_to=check_segments),
            Transition(after=check_segments, go_to=copy_here, when=segments_follow_the_run),
            Transition(after=check_segments, go_to=write_report, when=segments_differ),
            Transition(after=copy_here, go_to=import_route, when=import_wanted),
            Transition(after=copy_here, go_to=write_report, when=no_import_wanted),
            Transition(after=import_route, go_to=lockstep_port, when=imported),
            Transition(after=import_route, go_to=write_report, when=import_failed),
            Transition(after=lockstep_port, go_to=write_report),
        ],
    )


def run_vidsync_ready(start: "VidsyncReadyState", run_id: str, tracked: bool = True) -> "ReadyReport":
    outcome = vidsync_ready_graph().start(start, run_id, tracked=tracked)
    if not isinstance(outcome, Finished) or outcome.state.report is None:
        raise RuntimeError(f"vidsync-ready {run_id} finished without a report")
    return outcome.state.report


# ── the run's state ────────────────────────────────────────────────────

class Segment(BaseModel):
    """One entry of segments.json (tools/vidsync/segments.py)."""
    segment: int
    run_from: int
    checkpoint_to: int
    run_to: int
    differs_at: int | None
    teardown_save_is_next_start: bool


class SegmentCheck(Result):
    segments: int
    differing: tuple[int, ...]              # segments whose game state left the run's: the recording is wrong
    longer_than_spacing: tuple[int, ...]    # segments spanning more than the spacing the port validator wants
    save_chain_breaks: tuple[int, ...]      # segments whose in-game save is not the next one's start


class ImportResult(Result):
    ok: bool
    length: FrameCount | None               # the route's length routes.py now has
    log: LogPath


class ReadyReport(Result):
    checkpoint: Checkpoint | None
    export: JobResult | None
    segments: SegmentCheck | None
    copied_to: Path | None
    imported: ImportResult | None
    lockstep: VerifyReport | None


class VidsyncReadyState(BaseModel):
    run_dir: Path
    checkpoint_spec: RemotePath | None = None    # a checkpoint path or a run's name (None: the newest of the last run)
    most: FrameCount | None = None               # a run's newest checkpoint at most this game frame
    out_dir: RemotePath = RemotePath("data/ready")
    name: str = "bn6f_beastsp"
    every: int = 20000
    max_spacing: int = 30000
    local_ready: Path | None = None              # where to copy it here (None: VIDSYNC_READY, build-vidsync/ready)
    import_wanted: bool = False
    checkpoint: Checkpoint | None = None
    export: JobResult | None = None
    cut: JobResult | None = None
    segments: SegmentCheck | None = None
    copied_to: Path | None = None
    imported: ImportResult | None = None
    lockstep: VerifyReport | None = None
    report: ReadyReport | None = None


workflow = Workflow[VidsyncReadyState](VidsyncReadyState, project=PROJECT)


# ── steps ──────────────────────────────────────────────────────────────

class PickInput(Result):
    checkpoint_spec: RemotePath | None
    most: FrameCount | None


class PickOutput(Result):
    checkpoint: Checkpoint


@workflow.step
def pick_checkpoint(given: PickInput) -> PickOutput:
    """The checkpoint to export: the one named, or a run's newest (at most `most`), or the last run's newest."""
    if given.checkpoint_spec is not None and given.most is None:
        return PickOutput(checkpoint=resolve_checkpoint(given.checkpoint_spec))
    run = given.checkpoint_spec or driver_status().run
    found = newest_checkpoint(run, given.most)
    if found is None:
        raise RuntimeError(f"{run}: no checkpoint" + (f" at most {given.most}" if given.most else ""))
    return PickOutput(checkpoint=found)


class ExportInput(Result):
    checkpoint: Checkpoint
    out_dir: RemotePath
    name: str


class ExportOutput(Result):
    export: JobResult


@workflow.step
def export_recording(given: ExportInput) -> ExportOutput:
    """export.sh on the driver PC: the trace, pokes and save, replayed in mgba_run and emu_server, which must agree."""
    return ExportOutput(export=run_job(DRIVER_PC, f"export-{given.name}", "export.sh", given.checkpoint.path,
                                       given.out_dir, given.name, timeout_hours=2))


class CutInput(Result):
    out_dir: RemotePath
    name: str
    every: int


class CutOutput(Result):
    cut: JobResult


@workflow.step
def cut_segments(given: CutInput) -> CutOutput:
    """segments.sh on the driver PC: save-file segments about every `every` game frames, each replayed against
    the run (VERIFY.txt, segments.json)."""
    return CutOutput(cut=run_job(DRIVER_PC, f"segments-{given.name}", "segments.sh",
                                 f"{given.out_dir}/{given.name}.result.json", f"{given.out_dir}/segments",
                                 str(given.every), timeout_hours=6))


class CheckInput(Result):
    out_dir: RemotePath
    max_spacing: int
    run_dir: Path


class CheckOutput(Result):
    segments: SegmentCheck


@workflow.step
def check_segments(given: CheckInput) -> CheckOutput:
    """Read segments.json: every segment's game state must equal the run's; long segments and breaks in the
    save chain are reported."""
    local = fetch(DRIVER_PC, RemotePath(f"{given.out_dir}/segments/segments.json"), given.run_dir / "segments.json")
    return CheckOutput(segments=segment_check(TypeAdapter(list[Segment]).validate_json(local.read_text()),
                                              given.max_spacing))


def segment_check(segments: list[Segment], max_spacing: int) -> SegmentCheck:
    return SegmentCheck(
        segments=len(segments),
        differing=tuple(s.segment for s in segments if s.differs_at is not None),
        longer_than_spacing=tuple(s.segment for s in segments if s.checkpoint_to - s.run_from > max_spacing),
        save_chain_breaks=tuple(s.segment for s in segments[:-1] if not s.teardown_save_is_next_start))


class CopyInput(Result):
    out_dir: RemotePath
    name: str
    local_ready: Path | None
    run_dir: Path


class CopyOutput(Result):
    copied_to: Path


@workflow.step
def copy_here(given: CopyInput) -> CopyOutput:
    """Copy the recording and the segments (a few MB) from the driver PC to build-vidsync/ready (or local_ready)."""
    target = given.local_ready or VIDSYNC_READY
    names = " ".join([f"{given.name}.{ext}" for ext in ("trace", "poke", "sav")]
                     + [f"segments/{pattern}" for pattern in SEGMENT_FILES])
    tgz = RemotePath(f"{given.out_dir}/ready.tgz")
    _ = on_pc(DRIVER_PC, f"cd ~/vidsync/recomp/build-vidsync/{given.out_dir} && tar czf ready.tgz {names}\n", timeout=600)
    local = fetch(DRIVER_PC, tgz, given.run_dir / "ready.tgz")
    target.mkdir(parents=True, exist_ok=True)
    subprocess.run(["rm", "-rf", str(target / "segments")], check=True)
    subprocess.run(["tar", "xzf", str(local), "-C", str(target)], check=True)
    local.unlink()
    return CopyOutput(copied_to=target)


class ImportInput(Result):
    copied_to: Path
    name: str
    run_dir: Path


class ImportOutput(Result):
    imported: ImportResult


@workflow.step
def import_route(given: ImportInput) -> ImportOutput:
    """tools/import_route.py: the recording into tools/traces/beastsp.*, its checkpoints, window profiles,
    coverage, and the route's length in routes.py."""
    log = LogPath(given.run_dir / "import_route.log")
    done = subprocess.run(["python3", "tools/import_route.py", str(given.copied_to), "--source", given.name],
                          cwd=REPO, capture_output=True, text=True)
    _ = log.write_text(done.stdout + done.stderr)
    return ImportOutput(imported=ImportResult(ok=done.returncode == 0, length=beastsp_length(), log=log))


def beastsp_length(routes_py: Path = ROUTES_PY) -> FrameCount | None:
    """The beastsp route's length as routes.py has it now (import_route.py rewrites it)."""
    match = re.search(r'_route\("beastsp", (\d+)\)', routes_py.read_text())
    return FrameCount(int(match[1])) if match else None


class LockstepInput(Result):
    imported: ImportResult
    run_dir: Path


class LockstepOutput(Result):
    lockstep: VerifyReport


@workflow.step
def lockstep_port(given: LockstepInput) -> LockstepOutput:
    """Lockstep the port on the longer beastsp route (a child verify run: build and lockstep, no functions)."""
    assert given.imported.length is not None
    run_id = f"{current_run_id()}-verify"
    run_dir = given.run_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    route = Route(name=RouteName("beastsp"), trace=TRACES / "beastsp.trace", length=given.imported.length)
    start = VerifyState(run_dir=run_dir, functions=(), snapshots=None, routes=(route,), battle_code=False)
    return LockstepOutput(lockstep=run_verify(start, run_id))


class ReportInput(Result):
    checkpoint: Checkpoint | None
    export: JobResult | None
    segments: SegmentCheck | None
    copied_to: Path | None
    imported: ImportResult | None
    lockstep: VerifyReport | None


class ReportOutput(Result):
    report: ReadyReport


@workflow.step
def write_report(given: ReportInput) -> ReportOutput:
    """What was exported, whether the segments follow the run, where it was copied, the import and lockstep."""
    return ReportOutput(report=ReadyReport(checkpoint=given.checkpoint, export=given.export, segments=given.segments,
                                           copied_to=given.copied_to, imported=given.imported, lockstep=given.lockstep))


# ── decisions ──────────────────────────────────────────────────────────

def exported(state: VidsyncReadyState) -> bool:
    return state.export is not None and state.export.ok


def export_failed(state: VidsyncReadyState) -> bool:
    return not exported(state)


def segments_follow_the_run(state: VidsyncReadyState) -> bool:
    return state.segments is not None and state.segments.segments > 0 and not state.segments.differing


def segments_differ(state: VidsyncReadyState) -> bool:
    return not segments_follow_the_run(state)


def import_wanted(state: VidsyncReadyState) -> bool:
    return state.import_wanted


def no_import_wanted(state: VidsyncReadyState) -> bool:
    return not state.import_wanted


def imported(state: VidsyncReadyState) -> bool:
    return state.imported is not None and state.imported.ok and state.imported.length is not None


def import_failed(state: VidsyncReadyState) -> bool:
    return not imported(state)

