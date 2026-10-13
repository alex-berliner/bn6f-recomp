"""view.py VDIR RESULT.json FROM TO STEP OUT.png: replay a result (keys and pokes); emulator | its video frame
at game frames FROM..TO every STEP, with the keys."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np; from PIL import Image
from feat import video, emu_thumb
from emu import Emu
from inputs import name
vdir, res, a, b, step, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
T, _ = video(vdir); B = np.load(f'{vdir}/bits.npy')
r = json.load(open(res)); g0 = r['press_gf']
e = Emu()
for g, s in r.get('pokes', []): e.poke(g, s)
e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=g0 - 402)
rows = []
for i, (k, v) in enumerate(zip(r['keys'], r['vid'])):
    g = g0 + i
    if g + 1 < a: e.step(k); continue
    e.step(k)
    if a <= g + 1 < b and (g + 1 - a) % step == 0:
        E = emu_thumb(e.shot()); w = np.full((80, 3), 255.)
        rows.append(np.vstack([np.hstack([E, w, T[v].astype(float)]), np.full((2, 243), 255.)]))
        print(g + 1, v, name(0x3ff & ~k), name(int(B[v])))
    if g + 1 >= b: break
im = np.clip(np.vstack(rows), 0, 255).astype(np.uint8)
Image.fromarray(im).resize((im.shape[1] * 2, im.shape[0] * 2), Image.NEAREST).save(out)
