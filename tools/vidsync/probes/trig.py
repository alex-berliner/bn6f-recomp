"""npcs.py RESULT STATE G: active overworld NPC objects (0x20057B0 + n*0xD8, X +0x24, Y +0x28) every 20 frames."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0])
prev = None
for k in range(0, 140):
    e.step(r['keys'][G + k - g0] if G + k - g0 < len(r['keys']) else 0x3FF)
    po = e.u32(0x02001B98); cv = e.read(0x02001B8E, 1)[0]
    cur = (cv, e.read(0x02000310, 0x18).hex())
    if cur != prev or k % 20 == 0:
        print(G + k, 'trig', hex(cv), 'pos', hex(e.u32(po + 0x1C)), hex(e.u32(po + 0x20)), hex(e.u32(po + 0x24)), 'key', hex(r['keys'][G + k - g0]), 's310', cur[1])
    prev = cur
