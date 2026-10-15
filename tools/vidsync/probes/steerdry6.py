"""steerdry6.py STATE RESULTJSON D RA: the free walk + the re-arm at D+RA; dumps 0xBE4/0xBE6 bytes, the
full object list, and the comp state's +0x1a per frame D+40..D+120."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
RA = int(sys.argv[4])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
F6 = syms.EVENT_FLAGS + (0xBE6 >> 3)
CS = syms.WEATHER_COMP_STATE
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
N_OBJ = 56; hi = D + 125
lv = r['vid'][-1]; n0 = len(r['keys'])
ks = list(zip(r['keys'], r['vid'])) + [(b.key(lv + int(round(k * beam.RATE))), lv + int(round(k * beam.RATE)))
                                       for k in range(1, hi - p0 - n0 + 3)]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
hit = None; fnat = {}
for g in range(g0, D + 9):
    if g > g0: e.step(ks[g - p0][0])
    fnat[g] = e.read(FA4, 1)[0]
    if e.read(F6, 1)[0] & M6 and hit is None: hit = g
gc = hit - 2
pre = [[gc, f'{FA4:#x}={(fnat[gc] & ~M4) & ~M6:#04x}/1'],
       [D + RA, f'{FA4:#x}={(e.read(FA4, 1)[0] & ~M6) | M4:#04x}/1']]
e.loadf(state)
pend = {}
for g, spec in pre: pend.setdefault(g, []).append(spec)
seen = {}
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    if g < D + 40: continue
    f4 = e.read(FA4, 1)[0]; f6 = e.read(F6, 1)[0]
    ph = int.from_bytes(bytes(e.read(CS + 0x1A, 2)), 'little')
    buf = bytes(e.read(objs, osz * N_OBJ))
    live = [(i, buf[i * osz + 1], s32(int.from_bytes(buf[i * osz + 0xC:i * osz + 0x10], 'little')) >> 16,
             s32(int.from_bytes(buf[i * osz + 0x10:i * osz + 0x14], 'little')) >> 16)
            for i in range(N_OBJ) if buf[i * osz] & 1]
    key = tuple((i, t) for i, t, x, y in live)
    tag = ''
    if key not in seen:
        seen[key] = g; tag = ' <<SET'
    print(f'{g} f4={f4:02x} f6={f6:02x} ph={ph:04x} ' + ' '.join(f'{i}/t{t}:({x},{y})' for i, t, x, y in live) + tag)
e.close()
