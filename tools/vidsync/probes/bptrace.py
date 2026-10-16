import sys, json
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__))))   # tools/vidsync
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']
c = json.load(open(sys.argv[2] + '.json'))
e = Emu(); e.loadf(sys.argv[2] + '.state'); g, _ = e.gf()
print('state at', g, 'bp', hex(e.u32(0x02001B9C)), 'pokes >= g:', [(p[0], p[1][:60]) for p in c['pokes'] if p[0] >= g - 5])
for p in c['pokes']:
    if p[0] >= g: e.poke(*p)
bp = e.u32(0x02001B9C)
for gg in range(g, g + 100):
    e.step(r['keys'][gg - g0]); b2 = e.u32(0x02001B9C)
    if b2 != bp: print('frame', gg + 1, 'bp', hex(bp), '->', hex(b2)); bp = b2
