"""newest.py RUN [MAXG]: the checkpoint (fix_* / ckpt_*) of a run with the largest game frame, at most MAXG, as
JSON {"g": G, "path": "data/auto/RUN.N/fix_0003"} (the path without .json, as autosync.py and export.sh take
it). RUN is a run's name (every data/auto/RUN.N directory and its start, data/auto/RUN.start) or one directory
(data/auto/RUN.N). Run on the PC through py.sh."""
import glob
import json
import os
import sys


def candidates(run: str) -> list[str]:
    if os.path.isdir(run):
        dirs = [run]
    else:
        dirs = [d for d in glob.glob(os.path.join("data", "auto", run + ".*")) if os.path.isdir(d)]
    found = [f[:-5] for d in dirs for f in glob.glob(os.path.join(d, "fix_*.json")) + glob.glob(os.path.join(d, "ckpt_*.json"))]
    start = os.path.join("data", "auto", run + ".start")
    if os.path.exists(start):
        found.append(open(start).read().strip())
    return found


def main() -> None:
    run = sys.argv[1]
    most = int(sys.argv[2]) if len(sys.argv) > 2 else 1 << 30
    best = None
    for path in candidates(run):
        g = json.load(open(path + ".json"))["g"]
        if g <= most and (best is None or g > best[0]):
            best = (g, path)
    print(json.dumps(None if best is None else {"g": best[0], "path": best[1]}))


if __name__ == "__main__":
    main()
