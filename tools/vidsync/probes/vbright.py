"""vbright.py VDIR RESULTJSON G1 G2: per game frame, the video's mean thumbnail brightness and the display
key, using the run's recorded video mapping inside its range and RATE past it."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
import beam

vdir, rj, g1, g2 = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
r = json.load(open(rj))
b = beam.Beam(vdir)
p0 = r['press_gf']
vid = r['vid']; ks = r['keys']
for g in range(g1, g2 + 1):
    i = g - p0
    v = vid[i] if 0 <= i < len(vid) else None
    if v is None:
        v = vid[-1] + int(round((g - (p0 + len(vid) - 1)) * beam.RATE))
    T, x, Vm = b.vthumb(v)
    print(f'{g} v{v} bright {Vm:6.1f} key {b.key(v):#05x}')
