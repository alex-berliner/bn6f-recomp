"""mashcmp.py STATE RESULTJSON D R N: from the machine at game frame R (after a handler's played window), try
key strategies for N frames -- the video's display keys, and A tapped every P frames from R + S -- and report
for each: the frames the map (MapGroup, MapNumber) changed, and after the last change how our picture lines up
with the video (best video offset and cost over +-600, every 10 frames for 100 frames).  A good match costs
~12-25 here (measured before the storm)."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
import syms
from emu import Emu
from feat import emu_thumb
import beam

state, rj = sys.argv[1], sys.argv[2]
D, R, N = int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
vD = r['vid'][min(D - p0, len(r['vid']) - 1)]
vat = lambda g: vD + int(round((g - D) * beam.RATE))
A_DOWN, NONE = 0x3FE, 0x3FF


def video_keys(g):
    return b.key(vat(g))


def tapper(period, start):
    return lambda g: A_DOWN if g >= start and (g - start) % period == 0 else NONE


def trial(name, keyf):
    e = Emu(); e.loadf(state)
    g, _ = e.gf()
    maps, last = [], tuple(e.read(syms.GS_MAP, 2))
    for _ in range(N):
        e.step(keyf(g)); g += 1
        m = tuple(e.read(syms.GS_MAP, 2))
        if m != last:
            maps.append((g, f'{m[0]:#x}:{m[1]:#x}')); last = m
    lines = []
    for k in range(10):
        x = emu_thumb(e.shot()).astype(np.float32)
        v = vat(g)
        cs = [float(np.abs(x - b.T[min(max(v + o, 0), b.N - 1)].astype(np.float32)).mean()) for o in range(-600, 601)]
        i = int(np.argmin(cs)); lines.append((i - 600, round(cs[i], 1)))
        for _ in range(10):
            e.step(NONE); g += 1
    print(f'{name}: map changes {maps}; after: (offset, cost) {lines}', flush=True)


trial('video keys', video_keys)
for period in (2, 3, 4, 6, 8):
    trial(f'A every {period}', tapper(period, R))
