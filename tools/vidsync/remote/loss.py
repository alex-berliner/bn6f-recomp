"""loss.py RUN [D]: a loss of a run, as JSON for the vidsync-repair workflow: the loss json autosync wrote
(data/auto/RUN.N/loss_D.json: the path's keys, video frames and pokes up to the loss; the newest one when D is
not given), the video frame there, a checkpoint at least LEAD game frames before it to test a handler from (the
run's own, else its start), and what the log says about the repair (from the loss to the next fix or the end).
Run on the PC through py.sh."""
import glob
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import newest  # noqa: E402

LEAD = 300   # the handlers look back from the loss through the sync's ring of past machines: give it room


def main() -> None:
    run = sys.argv[1]
    want = int(sys.argv[2]) if len(sys.argv) > 2 else None
    losses = []
    for path in glob.glob(os.path.join("data", "auto", run + ".*", "loss_*.json")):
        d = int(re.search(r"loss_(\d+)\.json$", path)[1])
        if want is None or d == want:
            losses.append((os.path.getmtime(path), d, path))
    if not losses:
        print(json.dumps(None))
        return
    _, d, path = max(losses)
    r = json.load(open(path))
    i = min(d - r["press_gf"], len(r["vid"]) - 1)
    best = None
    for lead in (LEAD, 0):
        for ck in newest.candidates(run):
            g = json.load(open(ck + ".json"))["g"]
            if g <= d - lead and (best is None or g > best[0]):
                best = (g, ck)
        if best is not None:
            break
    log = open(os.path.join(os.path.dirname(path), "auto.log"), errors="replace").read().splitlines()
    at = next((k for k, line in enumerate(log) if f"lost the video near game {d}" in line), None)
    excerpt = []
    if at is not None:
        for line in log[at:]:
            excerpt.append(line[:300])
            if len(excerpt) > 1 and re.match(r"^.{9}(fix \d|done:|lost the video)", line):
                break
    print(json.dumps({"loss": d, "video": r["vid"][i], "loss_json": path, "pokes": len(r["pokes"]),
                      "checkpoint": None if best is None else {"g": best[0], "path": best[1]},
                      "log": excerpt[:40] + (["..."] + excerpt[-20:] if len(excerpt) > 60 else excerpt[40:])}))


if __name__ == "__main__":
    main()
