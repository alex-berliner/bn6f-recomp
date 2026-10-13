"""render.py LOSS_JSON CHECKPOINT OUTDIR: pictures of a loss for the vidsync-repair workflow, with the probes:
OUTDIR/compare.png, the emulator beside the video (probes/fullcmp.py) every 10 game frames over the 150 before
the loss and the 30 after, and OUTDIR/video.png, what the video shows over the 8 seconds after it
(probes/vidstrip.py). CHECKPOINT: a fix_/ckpt_ path (without .json) before the loss. Run on the PC through
py.sh."""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # tools/vidsync


def main() -> None:
    loss_json, checkpoint, out = sys.argv[1:4]
    os.makedirs(out, exist_ok=True)
    r = json.load(open(loss_json))
    d = int(re.search(r"loss_(\d+)\.json$", loss_json)[1])
    g = json.load(open(checkpoint + ".json"))["g"]
    last = r["press_gf"] + len(r["keys"]) - 1
    first, end = max(g + 1, d - 150), min(d + 30, last)
    video = r["vid"][min(d - r["press_gf"], len(r["vid"]) - 1)]
    for args in (["probes/fullcmp.py", loss_json, checkpoint + ".state", str(first), str(end), "10",
                  os.path.join(out, "compare.png")],
                 ["probes/vidstrip.py", str(video), str(video + 480), "40", os.path.join(out, "video.png")]):
        subprocess.run([sys.executable, os.path.join(HERE, args[0]), *args[1:]], check=True)
    print(json.dumps({"compare": os.path.join(out, "compare.png"), "video": os.path.join(out, "video.png"),
                      "frames": [first, end], "video_frames": [video, video + 480]}))


if __name__ == "__main__":
    main()
