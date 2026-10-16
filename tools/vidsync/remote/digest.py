"""digest.py [RUN]: what the driver's log says about the loss it is on, as one JSON object (the vidsync-tick
workflow's triage reads it; docs/vidsync/README.md). RUN defaults to ~/vidsync/runbase.

  loss              the game frame of the loss it is on (its last event a loss), or None
  first_seen        when it first lost the video within 30 frames of it, in this run (HH:MM:SS)
  minutes_on_loss   how long since then
  fixes_at_loss     the fixes the driver made for this loss (one that comes back to it counts again): each its
                    number, the handler that wrote it (the logged line before it), its mean cost if logged, and
                    whether it was empty (a video offset or an overlay window: no RAM write)
  came_back         handlers whose fix there came back to the loss ("not asking it again here")
  handlers          each handler's last line at this loss (what it found)
  search            the RNG search's last line (W=...) at this loss
  recent_fixes      the run's last 12 fixes, with their handlers and mean costs (a poor match is a suspect)
  traceback         the last traceback in the newest RUN.N.out, if any (a crash)

Run on the PC through py.sh (the working directory is the vidsync one)."""
import glob
import json
import os
import re
import sys
import time

LOSS = re.compile(r"^(\d\d:\d\d:\d\d) lost the video near game (\d+)")
FIX = re.compile(r"^(\d\d:\d\d:\d\d) fix (\d+): (.*)")
HANDLER = re.compile(r"^\d\d:\d\d:\d\d   ([a-z][a-z _]*?|W=\d+[^:]*): (.*)")
MEAN = re.compile(r"mean cost (\d+\.\d+)")
NEAR = 30            # a loss within this many frames of another is the same loss


def run_dirs(run: str) -> list[str]:
    base = os.path.join("data", "auto", run)
    return sorted((d for d in glob.glob(base + ".*") if os.path.isdir(d) and d.rsplit(".", 1)[1].isdigit()),
                  key=lambda d: int(d.rsplit(".", 1)[1]))


def seconds(hms: str) -> int:
    h, m, s = map(int, hms.split(":"))
    return h * 3600 + m * 60 + s


def digest(run: str) -> dict[str, object]:
    dirs = run_dirs(run)
    lines: list[str] = []
    for d in dirs:
        try:
            lines += open(os.path.join(d, "auto.log"), errors="replace").read().splitlines()
        except OSError:
            pass
    loss, first_seen = None, None
    fixes_at_loss: list[dict[str, object]] = []
    came_back: list[str] = []
    handlers: dict[str, str] = {}
    search = None
    recent: list[dict[str, object]] = []
    last_handler: tuple[str, str] | None = None
    current = None                                 # the loss the lines now belong to
    for line in lines:
        m = LOSS.match(line)
        if m:
            g = int(m[2])
            if current is None or abs(g - current) > NEAR:
                current, first_seen = g, m[1]
                fixes_at_loss, came_back, handlers, search = [], [], {}, None
            last_handler = None
            continue
        m = FIX.match(line)
        if m:
            name, said = last_handler if last_handler else ("?", "")
            cost = MEAN.search(said)
            fix = {"n": int(m[2]), "at": m[1], "handler": name, "mean_cost": float(cost[1]) if cost else None,
                   "empty": m[3].startswith(";") or m[3].strip() == "; going on from there", "writes": m[3][:200]}
            recent.append(fix)
            if current is not None:
                fixes_at_loss.append(fix)
            last_handler = None
            continue
        if "came back to this loss" in line:
            hm = re.search(r"the last fix \(([^)]+)\)", line)
            if hm:
                came_back.append(hm[1])
            continue
        m = HANDLER.match(line)
        if m and m[1].strip() != "from":           # ("  from: machine ...": where a fix resumed, not a handler)
            name = "rng" if m[1].startswith("W=") else m[1].strip()
            last_handler = (name, m[2])
            if name == "rng":
                search = line[9:300]
            else:
                handlers[name] = m[2][:300]
    last_loss = None
    for line in reversed(lines):
        if LOSS.match(line) or FIX.match(line) or "done:" in line:
            m = LOSS.match(line) or re.search(r"stuck near game (\d+)", line)   # (a driver that gave up there)
            last_loss = int(m[2] if m.re is LOSS else m[1]) if m else None
            break
    now = time.strftime("%H:%M:%S")
    minutes = None
    if last_loss is not None and first_seen is not None:
        minutes = ((seconds(now) - seconds(first_seen)) % 86400) / 60
    traceback = None
    if dirs:
        try:
            out = open(dirs[-1] + ".out", errors="replace").read()
            if "Traceback" in out:
                tail = out[out.rindex("Traceback"):].strip().splitlines()
                traceback = " | ".join(tail[-3:])[:500]
        except OSError:
            pass
    return {"run": run, "loss": last_loss, "first_seen": first_seen if last_loss is not None else None,
            "minutes_on_loss": round(minutes, 1) if minutes is not None else None,
            "fixes_at_loss": fixes_at_loss if last_loss is not None else [],
            "came_back": came_back if last_loss is not None else [],
            "handlers": handlers if last_loss is not None else {}, "search": search if last_loss is not None else None,
            "recent_fixes": recent[-12:], "traceback": traceback}


def main() -> None:
    run = sys.argv[1] if len(sys.argv) > 1 else open(os.path.expanduser("~/vidsync/runbase")).read().strip()
    print(json.dumps(digest(run)))


if __name__ == "__main__":
    main()
