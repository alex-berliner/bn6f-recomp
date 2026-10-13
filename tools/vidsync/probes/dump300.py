"""dump300.py RESULT STATE G: the 0x10 bytes before eWeatherCompState and its first 0xF0, every 30 frames
from G to G+60."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms   # game addresses by name from bn6f
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0])
for k in range(0, 61):
    e.step(r['keys'][G + k - g0] if G + k - g0 < len(r['keys']) else 0x3FF)
    if k % 30: continue
    d = e.read(syms.WEATHER_COMP_STATE - 0x10, 0x100)
    print(G + k, ' '.join(d[j:j + 4][::-1].hex() for j in range(0, 0x100, 4)))
