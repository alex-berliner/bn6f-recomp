"""obj0.py RESULT STATE G: map object 0 (eOverworldMapObjects, its first 0x40 bytes) and eWeatherCompState
+0x30..+0x4F, every 10 frames for 100 frames from G."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms   # game addresses by name from bn6f
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0])
for k in range(0, 100):
    e.step(r['keys'][G + k - g0] if G + k - g0 < len(r['keys']) else 0x3FF)
    if k % 10: continue
    d = e.read(syms.OW_MAP_OBJECTS, 0x40)
    print(G + k, ' '.join(d[j:j + 4][::-1].hex() for j in range(0, 0x40, 4)), '|', e.read(syms.WEATHER_COMP_STATE + 0x30, 0x20).hex())
