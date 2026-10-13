"""replay_sheet.py VDIR RESULT.json FROM TO EVERY OUT.png: replay a result's keys; sheet of emulator | its matched video frame."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np; from PIL import Image
from feat import video, emu_thumb
from emu import Emu
vdir, res, a, b, every, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
T, _ = video(vdir); r = json.load(open(res)); g0 = r['press_gf']
e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=g0 - 402)
rows = []
for i, (k, v) in enumerate(zip(r['keys'], r['vid'])):
    g = g0 + i
    e.step(k)
    if a <= g < b and (g - a) % every == 0:
        E = emu_thumb(e.shot())
        rows.append(np.hstack([E, np.full((80, 3), 255.), T[v], np.full((80, 3), 255.), T[min(v + 4, len(T) - 1)]]))
        print(g, v, hex(k), round(r['cost'][i], 2) if i < len(r['cost']) else '')
    if g >= b: break
im = np.clip(np.vstack(rows), 0, 255).astype(np.uint8)
Image.fromarray(im).resize((im.shape[1] * 2, im.shape[0] * 2), Image.NEAREST).save(out)
