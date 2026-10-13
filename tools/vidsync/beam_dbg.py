import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from scipy import ndimage
from beam import Beam, OFFS, BLUR
from feat import emu_thumb
b = Beam('data/start'); r = json.load(open('data/start/beam.json')); g0 = r['press_gf']
b.start(g0); e = b.e; e.load(b.branches[0].slot)
for i, (k, v) in enumerate(zip(r['keys'], r['vid'])):
    g = g0 + i
    _, h1 = e.step(k)
    if 1730 <= g < 1770 and g % 2 == 0:
        E = ndimage.gaussian_filter(emu_thumb(e.shot()), BLUR)
        es = b.errs(E, b.vbase(h1 - 1))
        print(g, 'node v', v, 'vbase', b.vbase(h1 - 1), 'best off', int(OFFS[np.argmin(es)]), 'err at node v', round(float(es[v - b.vbase(h1 - 1) + 8]) if abs(v - b.vbase(h1 - 1)) <= 8 else -1, 2), 'row', np.round(es, 1).tolist())
    if g >= 1770: break
