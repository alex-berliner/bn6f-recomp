import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np; from PIL import Image
from feat import video, emu_thumb, MASK
from sync import Sync
s = Sync('data/start', -1, 1.0, log=lambda *a: None)
vA = int(np.nonzero(s.B & 1)[0][0]); s.start(600, vA)
s.run(948, thr=1e9)
rows = []
for i in range(40):
    g = s.g; s.e.save(5000); key = s.key_at(s.h)
    E, h1, key = s.step(g, key); s.g, s.h = g + 1, h1
    es = [round(s.frame_err(E, s.vmap(h1 - 1) + o), 1) for o in range(-4, 5)]
    print(g, s.vmap(h1 - 1), es)
    if i in (5, 20, 39): rows.append(np.hstack([E, s.T[s.vmap(h1 - 1)].astype(float), np.abs(E - s.T[s.vmap(h1 - 1)]) * 3]))
im = np.clip(np.vstack(rows), 0, 255).astype(np.uint8); Image.fromarray(im).resize((im.shape[1] * 3, im.shape[0] * 3), Image.NEAREST).save('data/start/errcheck.png')
