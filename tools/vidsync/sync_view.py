"""sync_view.py VDIR DELTA PRESS_GF UNTIL_GF EVERY OUT.png: naive sync, then a sheet of emulator (left) vs matched video (right)."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np; from PIL import Image
from feat import video, emu_thumb, MASK
from emu import Emu
vdir, delta, press_gf, until, every, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
start = int(sys.argv[7]) if len(sys.argv) > 7 else 0
T, _ = video(vdir); B = np.load(f'{vdir}/bits.npy'); Tf = T.astype(np.float32)
e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); g, h = e.step(n=press_gf - 402)
V_A = int(np.nonzero(B & 1)[0][0]); v = V_A - delta; rows = []
while g < until:
    key = 0x3FF & ~int(B[v + delta]); g, h2 = e.step(key); nhw = h2 - h; h = h2
    E = emu_thumb(e.shot()); cands = range(v + 1, v + nhw + 3)
    es = [np.abs(E - Tf[c])[MASK].mean() for c in cands]; v = cands[int(np.argmin(es))]
    if g >= start and g % every == 0:
        rows.append(np.hstack([E.astype(np.uint8), np.full((80, 4), 255, np.uint8), T[v]]))
        print(g, v, round(min(es), 2), B[v + delta])
sheet = np.vstack([np.vstack([r, np.full((4, r.shape[1]), 255, np.uint8)]) for r in rows])
Image.fromarray(sheet).resize((sheet.shape[1] * 2, sheet.shape[0] * 2), Image.NEAREST).save(out)
