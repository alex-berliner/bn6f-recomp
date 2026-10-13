"""sync_probe.py VDIR DELTA PRESS_GF NFRAMES: naive forward sync over a clip, printing per-frame match stats."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from feat import video, emu_thumb, MASK
from emu import Emu
vdir, delta, press_gf, nfr = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
T, _ = video(vdir); B = np.load(f'{vdir}/bits.npy')
T = T.astype(np.float32)
def err(E, v): return np.abs(E - T[v])[MASK].mean()
e = Emu()
e.step(n=400); e.step(0x3FF & ~8, 2)                 # PRESS START
g, h = e.step(n=press_gf - 402)                      # idle on the NEW GAME menu
V_A = int(np.nonzero(B & 1)[0][0])                   # the video's NEW GAME press
v = V_A - delta                                      # video frame matched to the current emulator state
log = []
for i in range(nfr):
    key = 0x3FF & ~int(B[v + delta])
    g2, h2 = e.step(key); nhw = h2 - h; h = h2
    E = emu_thumb(e.shot())
    cands = range(v + 1, v + nhw + 3)
    es = [err(E, c) for c in cands]
    j = int(np.argmin(es)); vb = cands[j]
    log.append((g2, nhw, vb - v, es[j], es[min(nhw - 1, len(es) - 1)], B[v + delta]))
    v = vb
    if v + delta + 5 >= len(T): break
L = np.array(log, dtype=float)
print('frames', len(L), 'video end', v)
print('step histogram (video frames per game frame):', np.unique(L[:, 2], return_counts=True))
print('err percentiles', np.percentile(L[:, 3], [50, 90, 99, 100]).round(2))
for k in range(0, len(L), max(1, len(L) // 60)):
    r = L[k]; print(int(r[0]), int(r[1]), int(r[2]), round(r[3], 2), round(r[4], 2), int(r[5]))
