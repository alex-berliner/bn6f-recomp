"""clouddump.py RESULT STATE G [N]: the comp's cloud map object (id 0x1E, header type 3) whole (0x78 bytes)
every frame for N frames from G (default 90), with each field that changed since the frame before and the
comp state eWeatherCompState+0..0x2C; ends with a per-field summary (how often it changed, its range)."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; G = int(sys.argv[3]); N = int(sys.argv[4]) if len(sys.argv) > 4 else 90
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, G): e.step(r['keys'][gg - g0])

objs = syms.OW_MAP_OBJECTS; size = syms.OW_MAP_OBJECT_SIZE
slot = None
for n in range(56):
    a = objs + n * size
    if e.read(a, 1)[0] & 1 and e.read(a + 1, 1)[0] == 3 and e.u32(a + 4) & 0xFF == 0x1E:
        slot = n; break
if slot is None:
    print(f'no id-0x1e type-3 object at game {G}'); sys.exit(1)
a = objs + slot * size
print(f'slot {slot} at {a:#x}, game {G}, comp state {syms.WEATHER_COMP_STATE:#x}')
prev = None
hist = {}
for k in range(N + 1):
    e.step(r['keys'][G + k - g0] if G + k - g0 < len(r['keys']) else 0x3FF)
    raw = bytes(e.read(a, size)); comp = bytes(e.read(syms.WEATHER_COMP_STATE, 0x2C))
    if prev is not None:
        ch = []
        for o in range(size):
            if raw[o] != prev[0][o]:
                w = o & ~3
                ch.append(f'{o:+#04x}:{prev[0][o]:02x}->{raw[o]:02x}')
                v0 = int.from_bytes(prev[0][w:w+4], 'little'); v1 = int.from_bytes(raw[w:w+4], 'little')
                ch[-1] += f' (w{w:#04x} {v0:#010x}->{v1:#010x})'
        cc = [f'{o + i:#04x}:{prev[1][o + i]:02x}->{comp[o + i]:02x}' for i in (0,) for o in range(0x2C)
              if comp[o + i] != prev[1][o + i]]
        if ch or cc:
            print(f'{G + k}: ' + ' '.join(ch + cc))
    prev = (raw, comp)
    for o in range(size):
        hist.setdefault(o, []).append(raw[o])
    hst = {}
    for o in range(0x2C):
        hst.setdefault(o, []).append(comp[o])
often = [o for o in range(size) if len(set(hist[o])) > 2]
print('object fields changing >2 values:', ' '.join(f'{o:#04x}({len(set(hist[o]))} vals {min(hist[o]):02x}..{max(hist[o]):02x})' for o in often))
print('comp fields changing >2 values:', ' '.join(f'{o:#04x}({len(set(hst[o]))} vals {min(hst[o]):02x}..{max(hst[o]):02x})' for o in hst if len(set(hst[o])) > 2))
