"""trig.py RESULT STATE G: for 140 frames from G, the map's trigger code under MegaMan
(eGameState_CoordInteractionValue), his place, the key and eWeatherCompState's first 0x18 bytes, whenever
they change and every 20 frames."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms   # game addresses by name from bn6f
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0])
prev = None
for k in range(0, 140):
    e.step(r['keys'][G + k - g0] if G + k - g0 < len(r['keys']) else 0x3FF)
    po = e.u32(syms.GS_OW_PLAYER_PTR); cv = e.read(syms.GS_COORD_INTERACTION, 1)[0]
    cur = (cv, e.read(syms.WEATHER_COMP_STATE, 0x18).hex())
    if cur != prev or k % 20 == 0:
        print(G + k, 'trig', hex(cv), 'pos', hex(e.u32(po + 0x1C)), hex(e.u32(po + 0x20)), hex(e.u32(po + 0x24)), 'key', hex(r['keys'][G + k - g0]), 's310', cur[1])
    prev = cur
