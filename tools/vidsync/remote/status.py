"""status.py [RUN] [--text]: the driver's state, as one JSON object (the vidsync-run workflow's watch step reads
it) or, with --text, one screen for a person. RUN defaults to ~/vidsync/runbase (the run job_driver.sh started
last); its directories are data/auto/RUN.0, RUN.1, ... (supervise.sh starts a new one after a crash).

  furthest   the furthest game frame any of the run's logs reached ("game N" in auto.log), the progress measure
  alive      whether the run's driver (supervise.sh or autosync.py) is running
  ended      when it is not: done (end of the video), stuck (autosync gave up at a loss), error, crashed
  loss       the game frame of the loss it is repairing, if the last event is a loss
Run on the PC through py.sh (the working directory is the vidsync one)."""
import glob
import json
import os
import re
import sys

GAME = re.compile(r"(?:near )?game (\d+)")
EVENT = re.compile(r"lost the video|^.{9}fix \d|done:|stuck|internal error")
LOSS = re.compile(r"lost the video near game (\d+)")


def processes() -> list[str]:
    """The command lines of every process."""
    lines = []
    for path in glob.glob("/proc/[0-9]*/cmdline"):
        try:
            with open(path, "rb") as fh:
                lines.append(fh.read().replace(b"\0", b" ").decode(errors="replace"))
        except OSError:
            pass
    return lines


def status(run: str) -> dict[str, object]:
    base = os.path.join("data", "auto", run)
    dirs = sorted((d for d in glob.glob(base + ".*") if os.path.isdir(d)), key=lambda d: int(d.rsplit(".", 1)[1])
                  if d.rsplit(".", 1)[1].isdigit() else -1)
    furthest, furthest_at, fixes = None, None, 0
    for d in dirs:
        try:
            log = open(os.path.join(d, "auto.log"), errors="replace").read().splitlines()
        except OSError:
            continue
        for line in log:
            if "replaying a path" in line:
                continue
            for m in GAME.finditer(line):
                if furthest is None or int(m[1]) > furthest:
                    furthest, furthest_at = int(m[1]), line[:8]
            if re.match(r"^.{9}fix \d", line):
                fixes += 1
    newest = dirs[-1] if dirs else None
    tail: list[str] = []
    events: list[str] = []
    if newest:
        try:
            log = open(os.path.join(newest, "auto.log"), errors="replace").read().splitlines()
            tail = log[-5:]
            events = [line for line in log if EVENT.search(line)]
        except OSError:
            pass
    procs = processes()
    alive = any(("autosync.py" in p and f"/{run}." in p) or ("supervise.sh" in p and f"/{run} " in p + " ")
                for p in procs)
    last = events[-1] if events else ""
    ended = None
    if not alive and newest:
        ended = ("stuck" if "done: stuck" in last else "done" if "done:" in last
                 else "error" if "internal error" in last else "crashed")
    loss = LOSS.search(last)
    sup = open(base + ".sup").read().splitlines() if os.path.exists(base + ".sup") else []
    start = open(base + ".start").read().strip() if os.path.exists(base + ".start") else None
    return {"run": run, "dirs": dirs, "newest_dir": newest, "start": start, "alive": alive, "ended": ended,
            "furthest": furthest, "furthest_at": furthest_at, "fixes": fixes, "last_event": last[:300],
            "loss": int(loss[1]) if loss else None, "supervisor": sup[-3:], "log_tail": [t[:300] for t in tail],
            "workers_alive": sum("worker_server.py" in p and "python" in p for p in procs)}


def main() -> None:
    args = [a for a in sys.argv[1:] if a != "--text"]
    runbase = next(path for path in (os.path.expanduser("~/vidsync/runbase"), "/mnt/c/Users/alex/runbase")
                   if os.path.exists(path))   # (the second: where it was before the move into recomp)
    run = args[0] if args else open(runbase).read().strip()
    s = status(run)
    if "--text" not in sys.argv:
        print(json.dumps(s))
        return
    print(f"run {s['newest_dir']} {'running' if s['alive'] else 'ended: ' + str(s['ended'])}, furthest game "
          f"{s['furthest']} (at {s['furthest_at']}), fixes {s['fixes']}, start {s['start']}")
    for line in s["supervisor"]:
        print("supervisor:", line)
    print("last event:", s["last_event"][:170])
    for line in s["log_tail"][-2:]:
        print(line[:170])


if __name__ == "__main__":
    main()
