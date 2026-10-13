"""path_score.py VDIR RESULT.json [UNTIL]: replay a result straight and score every frame at its matched video frame."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from scipy import ndimage
import beam as bm
from feat import emu_thumb
vdir, res = sys.argv[1], sys.argv[2]; until = int(sys.argv[3]) if len(sys.argv) > 3 else 10**9
b = bm.Beam(vdir, log=lambda *a: None)
r = json.load(open(res)); g0 = r['press_gf']
b.start(g0); e = b.e; e.load(b.branches[0].slot)
errs = []
for i, (k, v) in enumerate(zip(r['keys'], r['vid'])):
    e.step(k)
    E = ndimage.gaussian_filter(emu_thumb(e.shot()), bm.BLUR)
    errs.append(b.errs(E, v)[bm.K])
    if g0 + i >= until: break
errs = np.array(errs)
run = np.convolve(errs > 2.0, np.ones(10), 'valid')
bad = np.nonzero(run >= 8)[0]
print('first sustained bad window at game', g0 + bad[0] if len(bad) else None)
for j in range(0, len(errs), 100):
    print(g0 + j, r['vid'][j], round(float(np.median(errs[j:j + 100])), 2), round(float(errs[j:j + 100].max()), 2))
