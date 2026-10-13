"""emustrip.py RESULT STATE G0 G1 STEP OUT: our screen only (path keys to the result's end, then none), with EXTRA pokes."""
import sys, json, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
from PIL import Image
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; a, z, st = int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for p in json.loads(os.environ.get('EXTRA', '[]')): e.poke(*p)
fr = []
for gg in range(g, z):
    e.step(r['keys'][gg - g0] if gg - g0 < len(r['keys']) else 0x3FF)
    if gg + 1 >= a and (gg + 1 - a) % st == 0: fr.append(e.shot())
while len(fr) % 4: fr.append(np.zeros_like(fr[0]))
Image.fromarray(np.vstack([np.hstack(fr[i:i + 4]) for i in range(0, len(fr), 4)])).save(sys.argv[6])
