"""diffmap.py RESULT.json STATE G1 G2 [STEP] [VJITTER]: per game frame, the emulator frame vs the video frame as
numbers: mean abs diff over 8x6 tiles of 30x27 (printed as the worst tiles' (r,c):diff), each side's mean
brightness, and the best video-frame offset within +-VJITTER (default 0)."""
import os, sys, json, subprocess
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; a, bnd = int(sys.argv[3]), int(sys.argv[4])
step = int(sys.argv[5]) if len(sys.argv) > 5 else 4; vjit = int(sys.argv[6]) if len(sys.argv) > 6 else 0
_cache = {}
def vf(v):
    if v not in _cache:
        pre = max(v - 30, 0)
        b = subprocess.run(['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', 'data/full.mp4', '-vf',
                            f"select=eq(n\\,{v - pre}),crop=906:604:37:56,scale=240:160:flags=area", '-frames:v', '1',
                            '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
        _cache[v] = np.frombuffer(b, np.uint8).reshape(160, 240, 3).astype(np.int16)
        if len(_cache) > 90:
            _cache.pop(next(iter(_cache)))
    return _cache[v]
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, bnd):
    e.step(r['keys'][gg - g0])
    f = gg + 1
    if f < a or (f - a) % step:
        continue
    emu = e.shot().astype(np.int16)
    v0 = r['vid'][f - g0]
    best = (None, 1e9)
    for dj in range(-vjit, vjit + 1):
        d = np.abs(emu - vf(v0 + dj)).mean()
        if d < best[1]:
            best = (dj, d)
    v = vf(v0 + (best[0] or 0))
    tiles = np.abs(emu - v).reshape(5, 32, 8, 30, 3).mean(axis=(1, 3, 4))
    worst = sorted(((tiles[i, j], i, j) for i in range(5) for j in range(8)), reverse=True)[:5]
    print(f'{f} v{v0 + (best[0] or 0)} diff {best[1]:6.2f} (dj {best[0]}) emu {emu.mean():6.1f} vid {v.mean():6.1f} '
          f'worst ' + ' '.join(f'({i},{j}){d:.0f}' for d, i, j in worst))
