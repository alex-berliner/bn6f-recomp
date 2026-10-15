"""blob.py VDIR RESULTJSON G1 G2: per game frame, find dark blobs in the video's thumbnail away from the
centre (MegaMan) and print their centroids as offsets from the centre (thumb px, x2 = game px)."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import numpy as np
import beam

vdir, rj, g1, g2 = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
r = json.load(open(rj))
b = beam.Beam(vdir)
p0 = r['press_gf']; vid = r['vid']
for g in range(g1, g2 + 1):
    i = g - p0
    v = vid[i] if 0 <= i < len(vid) else vid[-1] + int(round((g - (p0 + len(vid) - 1)) * beam.RATE))
    T = b.T[min(max(v, 0), b.N - 1)].astype(np.float32)
    H, W = T.shape
    cy, cx = H // 2, W // 2
    dark = T < 105
    # blobs by simple flood fill
    from scipy import ndimage
    lab, n = ndimage.label(dark)
    out = []
    for k in range(1, n + 1):
        ys, xs = np.nonzero(lab == k)
        if len(ys) < 12: continue
        my, mx = ys.mean(), xs.mean()
        dc = ((my - cy) ** 2 + (mx - cx) ** 2) ** 0.5
        if dc < 6: continue                     # MegaMan (centred)
        out.append((len(ys), round(mx - cx, 1), round(my - cy, 1), round(dc, 1)))
    out.sort(reverse=True)
    print(f'{g} v{v} mean {T.mean():6.1f} blobs ' + ' '.join(f'[{s}@({x:+.1f},{y:+.1f})d{d:.0f}]' for s, x, y, d in out[:3]))
