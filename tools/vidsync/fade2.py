import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from feat import video, emu_thumb, MASK
from emu import Emu
T, _ = video('data/start'); B = np.load('data/start/bits.npy')
vb = T[:, MASK].mean(1)
vA = int(np.nonzero(B & 1)[0][0])
# video: per frame brightness 560..700 and the input
print('video', ' '.join(f'{v}:{vb[v]:.0f}{"s" if B[v]&4 else ""}' for v in range(590, 660)))
for delta in (0, 1, 2):
    e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); g, h = e.step(n=198)
    v0 = vA - delta - h; rows = []
    while g < 1000:
        key = 0x3FF & ~int(B[h + v0 + delta]); g, h2 = e.step(key)
        rows.append((g, h2 - 1 + v0, emu_thumb(e.shot())[MASK].mean())); h = h2
    first = next(r for r in rows if r[2] > 5)
    print('delta', delta, 'emu fade starts at game', first[0], 'video-equivalent', first[1],
          ' '.join(f'{v}:{b:.0f}' for g, v, b in rows if 590 <= v <= 660))
    e.close()
