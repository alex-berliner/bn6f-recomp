"""fixlog.py [RUN]: every fix the driver made in RUN (all of RUN.N), as one JSON object, for the vidsync-tick
workflow's learn step (wf/tools/vidsync_learn.py: what each fix says about the game goes into symbols/db).

  fixes   each: dir (the RUN.N directory; its fix_NNNN checkpoint replays the path with the fix), n, time,
          handler (the handler line logged before it: "rng" for the RNG search's W= lines), game (the game
          frame the writes go in at), writes ([address, value, size] each), mean (its mean cost if logged),
          line (the fix line as logged)

Run on the PC through py.sh (the working directory is the vidsync one). RUN defaults to ~/vidsync/runbase."""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from digest import FIX, HANDLER, MEAN, run_dirs  # noqa: E402

WRITE = re.compile(r"(0x[0-9a-fA-F]+)=(0x[0-9a-fA-F]+)/(\d)")
AT = re.compile(r"at game (\d+)")


def fixes(run: str) -> list[dict]:
    out = []
    for d in run_dirs(run):
        path = d + ".out"
        if not os.path.exists(path):
            continue
        handler, mean = None, None
        for line in open(path, errors="replace"):
            line = line.rstrip("\n")
            m = HANDLER.match(line)
            if m:
                handler = "rng" if m.group(1).startswith("W=") else m.group(1).strip()
                c = MEAN.search(m.group(2))
                mean = float(c.group(1)) if c else mean
                continue
            m = FIX.match(line)
            if not m:
                continue
            body = m.group(3)
            at = AT.search(body)
            out.append({"dir": d, "n": int(m.group(2)), "time": m.group(1), "handler": handler,
                        "game": int(at.group(1)) if at else None,
                        "writes": [[int(a, 16), int(v, 16), int(s)] for a, v, s in WRITE.findall(body)],
                        "mean": mean, "line": line[:400]})
            handler, mean = None, None
    return out


if __name__ == "__main__":
    run = sys.argv[1] if len(sys.argv) > 1 else open(os.path.expanduser("~/vidsync/runbase")).read().strip()
    print(json.dumps({"run": run, "fixes": fixes(run)}))
