import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from feat import video, emu_thumb, MASK
from emu import Emu
T, _ = video('data/start'); B = np.load('data/start/bits.npy')
vA = int(np.nonzero(B & 1)[0][0])
for delta in (-1, 0, 1, 2):
    e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); g, h = e.step(n=198)
    v0 = vA - delta - h; rows = []; slot = 0
    while g < 1000:
        e.save(0)
        k1 = 0x3FF & ~int(B[h + v0 + delta]); g1, h2 = e.step(k1)
        k2 = 0x3FF & ~int(B[h2 - 1 + v0 + delta])
        if k2 != k1:
            e.load(0); g1, h2b = e.step(k2)
            if h2b != h2:
                h2 = h2b
        g = g1
        rows.append((g, h2 - 1 + v0, emu_thumb(e.shot())[MASK].mean())); h = h2
    blk = next(i for i, r in enumerate(rows) if r[2] < 1); first = next(r for r in rows[blk:] if r[2] > 5)
    print('last-frame rule delta', delta, 'fade starts at video-equivalent', first[1], '(video: 607)')
    e.close()
