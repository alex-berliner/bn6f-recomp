"""The two compute PCs vidsync runs on (docs/vidsync/README.md): deploy to them, start and stop the driver and
the worker server, read the driver's state, run jobs there and fetch small results. Everything heavy runs on
the PCs; this machine only sends commands and reads JSON, logs and pictures back (never video: the LAN is a
slow WireGuard link).

Commands reach WSL as a bash script on stdin (`ssh PC wsl -d Ubuntu -- bash -s`: no quoting through cmd.exe).
Anything that must outlive the SSH session (the driver, the worker server, a long job) is started through
WMI (remote/launch.ps1): SSH sessions to Windows kill their children. The scripts it runs there are the
deployed tools/vidsync/remote/ (tools/vidsync/deploy.py)."""
import base64
import re
import shlex
import subprocess
import time
from enum import Enum
from pathlib import Path

from pydantic import BaseModel

from wf.repo import REPO
from wf.tools.errors import ToolOutputError
from wf.types import FrameCount, LogPath, PcHost, RemotePath, Result, RunName, ToolMessage

DRIVER_PC = PcHost("10.67.1.175")   # the driver (supervise.sh -> autosync.py, 13 local workers), exports,
                                      # segment cuts, pathcheck, handler tests
WORKER_PC = PcHost("10.67.1.167")   # the worker server (15 workers, TCP 7777)
PCS = (DRIVER_PC, WORKER_PC)
USER = "alex"
REMOTE = "~/vidsync/recomp/tools/vidsync/remote"   # the deployed job scripts
LAUNCH = "C:/Users/alex/vidsync_launch.ps1"        # remote/launch.ps1, copied there by deploy.py
DEPLOY = REPO / "tools/vidsync/deploy.py"
SSH = ("ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=20", "-o", "ServerAliveInterval=30")
_SAFE = re.compile(r"^[\w./:=,+-]+$")   # what a job's arguments may hold (they pass through cmd.exe and PowerShell)


class RemoteError(Exception):
    """A command on a compute PC failed (unreachable, or it exited non-zero)."""


# ── what the PCs report ────────────────────────────────────────────────

class PcDeploy(Result):
    """What one PC said when a deploy was installed there."""
    pc: PcHost
    ok: bool
    commit: str
    uncommitted: bool    # the deployed files had changes not committed yet
    output: ToolMessage


class DriverEnd(Enum):
    DONE = "done"          # the sync reached the end of the video (or a test run's --until)
    STUCK = "stuck"        # autosync gave up at a loss: no handler and no RNG write fixed it
    ERROR = "error"        # autosync stopped on an internal error
    CRASHED = "crashed"    # its process died (supervise.sh gives up after 5 crashes in a row)


class DriverStatus(Result):
    """remote/status.py: where the driver is."""
    run: RunName
    newest_dir: RemotePath | None
    start: RemotePath | None          # the checkpoint the run started from (job_driver.sh writes it)
    alive: bool
    ended: DriverEnd | None
    furthest: FrameCount | None       # the furthest game frame the run's logs reached: the progress measure
    fixes: int
    last_event: ToolMessage
    loss: FrameCount | None           # the loss it is repairing, if that is its last event
    log_tail: tuple[ToolMessage, ...]


class Checkpoint(Result):
    """A fix_/ckpt_ checkpoint on the driver PC: the path's keys, video frames and pokes to game frame `g`,
    and the machine there (.state)."""
    path: RemotePath
    g: FrameCount


class JobResult(Result):
    name: str
    exit_code: int
    log_tail: tuple[ToolMessage, ...]

    @property
    def ok(self) -> bool:
        return self.exit_code == 0


class _Deployed(BaseModel):
    commit: str
    uncommitted: bool


class _DeployLine(BaseModel):
    pc: PcHost
    ok: bool
    output: str
    deployed: _Deployed


# ── running things there ───────────────────────────────────────────────

def on_pc(pc: PcHost, script: str, timeout: float = 600) -> str:
    """Run a bash script in the PC's WSL; its output (raises RemoteError when it fails)."""
    try:
        done = subprocess.run([*SSH, f"{USER}@{pc}", "wsl -d Ubuntu -- bash -s"], input=script, capture_output=True,
                              text=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise RemoteError(f"{pc}: no answer in {timeout:.0f} s to: {script[:200]}") from error
    output = _clean(done.stdout)
    if done.returncode != 0:
        raise RemoteError(f"{pc}: exit {done.returncode}: {script[:200]}\n{_clean(done.stderr)[-1500:]}\n{output[-1500:]}")
    return output


def remote(pc: PcHost, *command: str, timeout: float = 600) -> str:
    """Run one of the deployed remote/ scripts (`remote(pc, "py.sh", "remote/status.py")`)."""
    return on_pc(pc, f"bash {REMOTE}/{shlex.join(command)}\n", timeout)


def launch(pc: PcHost, *job: str) -> None:
    """Start remote/JOB ARGS through WMI (launch.ps1), so it outlives this SSH session."""
    for part in job:
        if not _SAFE.match(part):
            raise ValueError(f"{part!r}: a launched job's arguments may hold only letters, digits and ./:=,+-_")
    command = f'powershell -NoProfile -ExecutionPolicy Bypass -File {LAUNCH} -Job "{" ".join(job)}"'
    done = subprocess.run([*SSH, f"{USER}@{pc}", command], capture_output=True, text=True, timeout=120)
    if done.returncode != 0 or "rc 0" not in done.stdout:
        raise RemoteError(f"{pc}: launch {' '.join(job)}: {_clean(done.stdout + done.stderr)[-500:]}")


def run_job(pc: PcHost, name: str, *script: str, timeout_hours: float = 6, poll_seconds: float = 30) -> JobResult:
    """Run remote/SCRIPT ARGS as a job (job.sh: launched through WMI, its output in ~/vidsync/jobs/NAME.log) and
    wait for it to end."""
    launch(pc, "job.sh", name, *script)
    deadline = now() + timeout_hours * 3600
    while True:
        pause(poll_seconds)
        done = on_pc(pc, f"cat ~/vidsync/jobs/{name}.done 2>/dev/null || true\n", timeout=120).strip()
        if done:
            tail = on_pc(pc, f"tail -40 ~/vidsync/jobs/{name}.log\n", timeout=120)
            return JobResult(name=name, exit_code=int(done), log_tail=_lines(tail))
        if now() > deadline:
            _ = remote(pc, "stop.sh", name)
            raise RemoteError(f"{pc}: job {name} still running after {timeout_hours} h: stopped")


def fetch(pc: PcHost, path: RemotePath, local: Path) -> Path:
    """Copy a small file (a picture, a json) from the PC's vidsync working directory to `local`."""
    text = on_pc(pc, f"cd ~/vidsync/recomp/build-vidsync && base64 -w0 {shlex.quote(path)}\n", timeout=300)
    local.parent.mkdir(parents=True, exist_ok=True)
    _ = local.write_bytes(base64.b64decode(text))
    return local


def now() -> float:
    return time.time()


def pause(seconds: float) -> None:
    time.sleep(seconds)


# ── deploy ─────────────────────────────────────────────────────────────

def deploy(pcs: tuple[PcHost, ...], log: LogPath) -> tuple[PcDeploy, ...]:
    """tools/vidsync/deploy.py: this checkout's vidsync, built on each PC."""
    done = subprocess.run(["python3", str(DEPLOY), "--json", *(arg for pc in pcs for arg in ("--pc", pc))],
                          capture_output=True, text=True, timeout=1800, cwd=REPO)
    _ = log.write_text(done.stdout + done.stderr)
    lines = [_DeployLine.model_validate_json(line) for line in done.stdout.splitlines() if line.startswith("{")]
    if not lines:
        raise ToolOutputError(f"deploy.py printed no result, see {log}")
    return tuple(PcDeploy(pc=line.pc, ok=line.ok, commit=line.deployed.commit, uncommitted=line.deployed.uncommitted,
                          output=ToolMessage(line.output[-500:])) for line in lines)


# ── the driver and the worker server ───────────────────────────────────

def driver_status(run: RunName | None = None) -> DriverStatus:
    """The run's state (default: the run started last, ~/vidsync/runbase on the driver PC)."""
    output = remote(DRIVER_PC, "py.sh", "remote/status.py", *((run,) if run else ()), timeout=180)
    return DriverStatus.model_validate_json(json_line(output))


def newest_checkpoint(run: RunName | RemotePath, most: FrameCount | None = None) -> Checkpoint | None:
    """A run's checkpoint (or a run directory's) with the largest game frame, at most `most`."""
    output = json_line(remote(DRIVER_PC, "py.sh", "remote/newest.py", run, *((str(most),) if most is not None else ()),
                              timeout=180))
    return None if output == "null" else Checkpoint.model_validate_json(output)


def checkpoint_at(path: RemotePath) -> Checkpoint:
    """A checkpoint by its path (without .json), with its game frame read from its json."""
    script = (f"cd ~/vidsync/recomp/build-vidsync && "
              f"python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))[\"g\"])' {shlex.quote(path)}.json\n")
    return Checkpoint(path=path, g=FrameCount(int(on_pc(DRIVER_PC, script, timeout=120).strip())))


def resolve_checkpoint(spec: RemotePath) -> Checkpoint:
    """A checkpoint path (data/auto/run6y.0/fix_0026), or a run's name or directory for its newest checkpoint."""
    if "/" in spec:
        try:
            return checkpoint_at(spec)
        except RemoteError:
            pass   # not a checkpoint: a run directory
    found = newest_checkpoint(RemotePath(spec.rstrip("/")))
    if found is None:
        raise RemoteError(f"{spec}: no checkpoint (fix_*/ckpt_*, or the run's start)")
    return found


def run_names() -> tuple[RunName, ...]:
    """Every run name in data/auto on the driver PC (from its RUN.N directories)."""
    output = on_pc(DRIVER_PC, "ls ~/vidsync/data/auto\n", timeout=120)
    return tuple(sorted({RunName(name.rsplit(".", 1)[0]) for name in output.split()
                         if re.fullmatch(r"[\w-]+\.\d+", name)}))


def next_run_name(after: RunName, taken: tuple[RunName, ...]) -> RunName:
    """The run name after `after` in the runs' own sequence, not taken yet: run7k -> run7l, run7z -> run8a,
    run9 -> run9a."""
    match = re.fullmatch(r"(.*?)(\d+)([a-z]?)", after)
    if match is None:
        candidate = RunName(f"{after}a")
    else:
        stem, number, letter = match[1], int(match[2]), match[3]
        if not letter:
            candidate = RunName(f"{stem}{number}a")
        elif letter == "z":
            candidate = RunName(f"{stem}{number + 1}a")
        else:
            candidate = RunName(f"{stem}{number}{chr(ord(letter) + 1)}")
    return next_run_name(candidate, taken) if candidate in taken else candidate


def stop_driver() -> None:
    _ = remote(DRIVER_PC, "stop.sh", "driver", timeout=120)


def start_driver(run: RunName, start: RemotePath, replay: RemotePath | None = None) -> None:
    """Stop the driver that runs, then start run RUN from checkpoint START (job_driver.sh, through WMI)."""
    stop_driver()
    launch(DRIVER_PC, "job_driver.sh", run, start, *((replay,) if replay else ()))


def restart_workers() -> int:
    """Stop the worker server and start it from the deployed tree; how many worker_server processes run then."""
    _ = remote(WORKER_PC, "stop.sh", "workers", timeout=120)
    launch(WORKER_PC, "job_workers.sh")
    pause(20)
    return int(on_pc(WORKER_PC, "pgrep -fc worker_server.py || true\n", timeout=120).strip() or 0)


# ── losses (vidsync-repair) ────────────────────────────────────────────

class LossCase(Result):
    """remote/loss.py: a loss of a run and what to test a fix from."""
    run: RunName
    loss: FrameCount
    video: int                          # the video frame there
    loss_json: RemotePath               # data/auto/RUN.N/loss_D.json: the path to the loss
    pokes: int
    checkpoint: Checkpoint | None       # a checkpoint at least 300 frames before it (None: the run has none)
    log: tuple[ToolMessage, ...]        # the driver's log from the loss on: what the built-in handlers tried


class _LossLine(BaseModel):
    loss: int
    video: int
    loss_json: str
    pokes: int
    checkpoint: Checkpoint | None
    log: list[str]


def find_loss(run: RunName, loss: FrameCount | None = None) -> LossCase | None:
    """The run's loss at game frame `loss` (None: its newest)."""
    output = json_line(remote(DRIVER_PC, "py.sh", "remote/loss.py", run, *((str(loss),) if loss is not None else ()),
                              timeout=300))
    if output == "null":
        return None
    line = _LossLine.model_validate_json(output)
    return LossCase(run=run, loss=FrameCount(line.loss), video=line.video, loss_json=RemotePath(line.loss_json),
                    pokes=line.pokes, checkpoint=line.checkpoint, log=tuple(ToolMessage(text) for text in line.log))


def render_loss(case: LossCase, local_dir: Path) -> tuple[Path, ...]:
    """The probes' pictures of the loss (remote/render.py on the driver PC), copied to local_dir."""
    assert case.checkpoint is not None
    out = RemotePath(f"data/repair/{case.run}-{case.loss}")
    job = run_job(DRIVER_PC, f"render-{case.run}-{case.loss}", "py.sh", "remote/render.py", case.loss_json,
                  case.checkpoint.path, out, timeout_hours=1, poll_seconds=15)
    if not job.ok:
        raise RemoteError(f"render.py failed: {' | '.join(job.log_tail[-5:])}")
    return tuple(fetch(DRIVER_PC, RemotePath(f"{out}/{name}"), local_dir / name) for name in ("compare.png", "video.png"))


class HandlerTest(Result):
    """A handler synced on its own from the loss checkpoint, on the driver PC's own workers only (remote
    workers once returned stale results), then the path it made replayed against the video."""
    out_dir: RemotePath
    reached: FrameCount | None          # where the test sync stopped (None: it did not say)
    needed: FrameCount                  # 600 frames past the loss
    fixes: int
    pathcheck_holds: bool
    log_tail: tuple[ToolMessage, ...]
    driver_was_alive: bool = False      # the production driver ran before the test stopped it

    @property
    def passed(self) -> bool:
        return self.reached is not None and self.reached >= self.needed and self.fixes > 0 and self.pathcheck_holds


def test_handler(case: LossCase, handler: str, attempt: int, hold: int = 600) -> HandlerTest:
    """autosync.py from the loss checkpoint with only this handler (--handlers, no RNG search) to `hold` frames
    past the loss (--until), then pathcheck.py on its result. A smoke test first (the handler imports on the
    driver PC, with its syms); then the driver is stopped (the test needs the PC's cores) and whether it ran is
    noted, so a failed test can restart it (restart_after_test)."""
    assert case.checkpoint is not None
    out = RemotePath(f"data/auto/{case.run}-test-{handler}-{attempt}")
    needed = FrameCount(case.loss + hold)
    smoke = (". ~/vidsync/recomp/tools/vidsync/remote/env.sh && cd $V && python3 -c "
             f"'import sys; sys.path.insert(0, \".\"); import syms, importlib; m = importlib.import_module(\"handlers.{handler}\"); "
             "assert callable(m.try_fix) and m.NAME' && echo smoke-ok\n")
    try:
        smoked = "smoke-ok" in on_pc(DRIVER_PC, smoke, timeout=180)
    except RemoteError as error:
        smoked, why = False, str(error)[-300:]
    else:
        why = "it did not import"
    if not smoked:
        return HandlerTest(out_dir=out, reached=None, needed=needed, fixes=0, pathcheck_holds=False,
                           log_tail=(ToolMessage(f"smoke test failed: {why}"),), driver_was_alive=False)
    alive = driver_status().alive
    stop_driver()
    _ = on_pc(DRIVER_PC, f"rm -rf ~/vidsync/data/auto/{case.run}-test-{handler}-{attempt}\n", timeout=120)
    sync = run_job(DRIVER_PC, f"test-{handler}-{attempt}", "py.sh", "autosync.py", "data/full", case.checkpoint.path,
                   out, "--workers", "13", "--until", str(needed), "--handlers", handler, timeout_hours=4)
    done = next((line for line in reversed(sync.log_tail) if " done: " in line), None)
    reached_text = re.search(r"done: reached game (\d+)", done or "")
    fixes_text = re.search(r"fixes (\d+)$", done or "")
    reached = FrameCount(int(reached_text[1])) if reached_text else None
    holds = False
    if reached is not None:
        check = run_job(DRIVER_PC, f"test-{handler}-{attempt}-pathcheck", "py.sh", "pathcheck.py", f"{out}/result.json")
        holds = any(line.startswith("holds to the end") for line in check.log_tail)
    return HandlerTest(out_dir=out, reached=reached, needed=needed, fixes=int(fixes_text[1]) if fixes_text else 0,
                       pathcheck_holds=holds, log_tail=sync.log_tail[-25:], driver_was_alive=alive)


def restart_after_test(case: LossCase, test: HandlerTest) -> RunName | None:
    """A failed test stopped a running driver: start it again as the next run from the run's newest
    checkpoint (it goes on searching at the loss; the handlers that failed there are kept in its checkpoints).
    None when the driver was not running before the test."""
    if not test.driver_was_alive:
        return None
    checkpoint = newest_checkpoint(case.run)
    if checkpoint is None:
        return None
    run = next_run_name(case.run, run_names())
    start_driver(run, checkpoint.path)
    return run


# ── helpers ────────────────────────────────────────────────────────────

def _clean(text: str) -> str:
    return text.replace("\r", "").replace("\0", "")


def _lines(text: str) -> tuple[ToolMessage, ...]:
    return tuple(ToolMessage(line[:300]) for line in text.splitlines())


def json_line(text: str) -> str:
    """The last line of a tool's output that is JSON (the scripts print one)."""
    for line in reversed(text.splitlines()):
        if line.startswith(("{", "[", "null")):
            return line
    raise ToolOutputError(f"no JSON in: {text[-500:]}")

