import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from scipy import ndimage
from feat import MASK
from sync import Sync
s = Sync('data/start', 0, 1.0, log=lambda *a: None)
vA = int(np.nonzero(s.B & 1)[0][0]); s.start(600, vA); s.off = 0
s.run(1100, thr=1e9)
rows = []
for g in range(900, 1099):
    v = s.vid[g]; V = s.vthumb(v)
    edge = (np.abs(np.diff(V, axis=0))[:-0 or None, :][:, :][MASK[1:]].mean() + np.abs(np.diff(V, axis=1))[MASK[:, 1:]].mean()) / 2
    es = {o: s.frame_err(s.thumb[g], v + o) for o in (-1, 0, 1)}
    rows.append((g, v, edge, es[0], es[-1], es[1], V[MASK].mean(), s.thumb[g][MASK].mean()))
for r in rows[::6]: print(' '.join(f'{x:.2f}' if isinstance(x, float) else str(x) for x in r))
r = np.array([x[2:4] for x in rows], float)
print('err/edge ratio percentiles', np.percentile(r[:, 1] / (r[:, 0] + 0.05), [10, 50, 90, 100]).round(2))
