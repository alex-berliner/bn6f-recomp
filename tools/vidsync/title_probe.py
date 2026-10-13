import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from feat import video, emu_thumb, MASK
from emu import Emu
t, s = video(sys.argv[1])
# how much does the video change frame to frame on the title (first 6 s)?
d = np.abs(np.diff(t[:400].astype(np.int16), axis=0)).mean((1, 2))
print('video consecutive diffs (first 60):', ' '.join(f'{x:.1f}' for x in d[:60]))
e = Emu(); E = []
for i in range(1200):
    e.step(); E.append(emu_thumb(e.shot()))
E = np.array(E)
de = np.abs(np.diff(E, axis=0)).mean((1, 2))
print('emu consecutive diffs 200..260:', ' '.join(f'{x:.1f}' for x in de[200:260]))
# brightness mapping: least squares video = a*emu + b on frame pairs with matching content (title vs title)
v0 = t[100].astype(np.float32); best = min(range(250, 1200), key=lambda i: np.abs(E[i] - v0)[MASK].mean())
A = np.vstack([E[best][MASK], np.ones(MASK.sum())]).T; (a, b), *_ = np.linalg.lstsq(A, v0[MASK], rcond=None)
print('best emu frame for video 100:', best, 'map a=%.3f b=%.2f' % (a, b), 'resid', np.abs(a * E[best] + b - v0)[MASK].mean())
np.save('data/start/emu_title.npy', E)
