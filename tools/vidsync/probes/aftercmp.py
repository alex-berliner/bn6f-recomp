"""aftercmp.py STATE RESULTJSON D R N: from the machine at game frame R (STATE), play the video's display keys
for N frames and, every 5 frames, find the video frame (within +-400 of the expected one) our picture looks most
like -- does our game run early, late, or on another screen than the video after a handler's played window?
The expected video frame at game g is vid[D] + round((g - D) * RATE), the mapping the handlers use past a loss."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
from emu import Emu
from feat import emu_thumb
import beam

state, rj = sys.argv[1], sys.argv[2]
D, R, N = int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
vD = r['vid'][min(D - p0, len(r['vid']) - 1)]
vat = lambda g: vD + int(round((g - D) * beam.RATE))
e = Emu(); e.loadf(state)
g, _ = e.gf()
assert g == R, (g, R)
for k in range(N + 1):
    if k % 5 == 0:
        x = emu_thumb(e.shot()).astype(np.float32)
        v = vat(g)
        offs = range(-400, 401)
        cs = [float(np.abs(x - b.T[min(max(v + o, 0), b.N - 1)].astype(np.float32)).mean()) for o in offs]
        i = int(np.argmin(cs))
        print(f'game {g} video {v}: cost at 0 {cs[400]:.1f}; best {cs[i]:.1f} at {offs[i]:+d}; '
              f'our picture mean {x.mean():.0f}, video mean {b.T[v].mean():.0f}', flush=True)
    e.step(b.key(vat(g)))
    g += 1
