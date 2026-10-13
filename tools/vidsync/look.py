"""look.py VDIR UNTIL FROM TO EVERY OUT: run the sync (no repair) to UNTIL, then print errors at offsets for FROM..TO and save a sheet."""
import sys, os; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np; from PIL import Image
from sync import Sync
vdir, until, a, b, every, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
s = Sync(vdir, 0, 60 / 59.7275, log=print)
vA = int(np.nonzero(s.B & 1)[0][0]); s.start(600, vA)
import json
if len(sys.argv) > 7:
    s.keys.update({int(k): v for k, v in json.load(open(sys.argv[7]))['keys'].items() if int(k) < a})
s.repair = lambda *x: True
s.run(until)
rows = []
for g in range(a, b):
    v = s.vid[g]
    es = [round(s.frame_err(s.thumb[g], v + o), 1) for o in range(-4, 5)]
    print(g, v, round(s.limit(v), 1), es, hex(s.keys[g]))
    if (g - a) % every == 0:
        rows.append(np.hstack([s.thumb[g], np.full((80, 3), 255.), s.vthumb(v)]))
im = np.clip(np.vstack(rows), 0, 255).astype(np.uint8)
Image.fromarray(im).resize((im.shape[1] * 2, im.shape[0] * 2), Image.NEAREST).save(out)
