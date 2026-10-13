"""npcs.py RESULT STATE G: active overworld NPC objects (0x20057B0 + n*0xD8, X +0x24, Y +0x28) every 20 frames."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0])
prev = {}
for k in range(0, 121):
    e.step(r['keys'][G + k - g0] if G + k - g0 < len(r['keys']) else 0x3FF)
    if k % 30: continue
    po = e.u32(0x02001B98); out = [f'player {e.u32(po + 0x1C):#x},{e.u32(po + 0x20):#x}']
    for n in range(56):
        a = 0x02011EE0 + n * 0x78
        if not e.read(a, 1)[0] & 1: continue
        x, y = e.u32(a + 0xC), e.u32(a + 0x10); oid = e.u32(a + 4)
        mv = '*' if prev.get(n) not in (None, (x, y)) else ''
        prev[n] = (x, y); out.append(f'{n}:id{oid:#x} {x:#x},{y:#x}{mv}')
    print(G + k, ' | '.join(out))
