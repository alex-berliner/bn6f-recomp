"""fullcmp.py RESULT.json STATE G1 G2 STEP OUT.png: emulator | video at full size, game frames G1..G2."""
import os, sys, json, numpy as np, subprocess
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
from PIL import Image
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; a, bnd, step = int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
def vf(v):
    pre = max(v - 30, 0)
    b = subprocess.run(['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', 'data/full.mp4', '-vf', f"select=eq(n\\,{v - pre}),crop=906:604:37:56,scale=240:160:flags=area", '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
    return np.frombuffer(b, np.uint8).reshape(160, 240, 3)
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
import os
for p in json.loads(os.environ.get("EXTRA", "[]")): e.poke(*p)
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
rows = []
for gg in range(g, bnd):
    e.step(r['keys'][gg - g0])
    if gg + 1 >= a and (gg + 1 - a) % step == 0:
        rows.append(np.hstack([e.shot(), np.full((160, 4, 3), 255, np.uint8), vf(r['vid'][gg - g0])]))
Image.fromarray(np.vstack(rows)).save(sys.argv[6])
