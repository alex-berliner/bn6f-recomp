"""panels.py RESULT STATE G: step the result's keys to game frame G; print ePanelData's types (+2) and alliances (+3)
as 5 rows of 8 (panels of 0x20 bytes at 0x02039AE0, row-major)."""
import sys, json, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3])
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0] if gg - g0 < len(r['keys']) else 0x3FF)
d = e.read(0x02039AE0, 0x500)
for row in range(5):
    print(' '.join(f'{d[(row * 8 + col) * 0x20 + 2]:02x}/{d[(row * 8 + col) * 0x20 + 3]:x}' for col in range(8)))
