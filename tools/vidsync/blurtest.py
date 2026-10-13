import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from scipy import ndimage
from feat import MASK
from sync import Sync
s = Sync('data/start', -1, 1.0, log=lambda *a: None)
vA = int(np.nonzero(s.B & 1)[0][0]); s.start(600, vA); s.off = -1
s.run(1040, thr=1e9)
for sig in (0, 0.7, 1.0, 1.5, 2.0):
    f = (lambda x: ndimage.gaussian_filter(x.astype(np.float32), sig)) if sig else (lambda x: x.astype(np.float32))
    good, near = [], []
    for g in range(960, 1039):
        E = f(s.thumb[g]); v = s.vid[g]
        es = {o: np.abs(E - f(s.T[v + o]))[MASK].mean() for o in (-2, -1, 0, 1, 2)}
        good.append(es[0]); near.append(min(es[-1], es[1]))
    good, near = np.array(good), np.array(near)
    print(f'sigma {sig}: correct median {np.median(good):.2f} max {good.max():.2f} | off-by-one median {np.median(near):.2f} min {near.min():.2f} | ratio median {np.median(near/good):.2f}')
