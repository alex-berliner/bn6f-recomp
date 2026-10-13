"""npcs.py RESULT STATE G: active overworld NPC objects (0x20057B0 + n*0xD8, X +0x24, Y +0x28) every 20 frames."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0])
for k in range(0, 120):
    e.step(r['keys'][G + k - g0] if G + k - g0 < len(r['keys']) else 0x3FF)
    if k % 20: continue
    act = e.u32(0x020010E0); po = e.u32(0x02001B98)
    out = [f'player {e.u32(po + 0x1C):#x},{e.u32(po + 0x20):#x}']
    for n in range(16):
        a = 0x020057B0 + n * 0xD8
        if e.read(a, 1)[0] & 1:
            out.append(f'{n}:{a:#x} {e.u32(a + 0x24):#x},{e.u32(a + 0x28):#x} st{e.read(a + 8, 2).hex()}')
    print(G + k, hex(act), ' | '.join(out))
