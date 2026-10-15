"""objdump2.py STATE RESULTJSON D G1 G2: all live overworld map objects (type, x, y) each frame, to spot
which object carries the visible storm cloud near MegaMan."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
G1, G2 = int(sys.argv[4]), int(sys.argv[5])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
N_OBJ = 56; hi = G2 + 4
lv = r['vid'][-1]; n0 = len(r['keys'])
ks = list(zip(r['keys'], r['vid'])) + [(b.key(lv + int(round(k * beam.RATE))), lv + int(round(k * beam.RATE)))
                                       for k in range(1, hi - p0 - n0 + 3)]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
hit = None
for g in range(g0, D + 9):
    if g > g0: e.step(ks[g - p0][0])
    if e.read(syms.EVENT_FLAGS + (0xBE6 >> 3), 1)[0] & M6 and hit is None: hit = g
gc = hit - 2
pre = [[gc, f'{FA4:#x}={(e.read(FA4, 1)[0] & ~M4) & ~M6:#04x}/1']]
e.loadf(state)
byf = {}
for g, spec in pre: byf.setdefault(g, []).append(spec)
pend = dict(byf)
prev = {}
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    if g < G1: continue
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    mx, my = s32(e.u32(p + 0x1C)) >> 16, s32(e.u32(p + 0x20)) >> 16
    mx = mx - 65536 if mx > 32768 else mx; my = my - 65536 if my > 32768 else my
    buf = bytes(e.read(objs, osz * N_OBJ))
    live = []
    for i in range(N_OBJ):
        o = i * osz
        if not (buf[o] & 1): continue
        t = buf[o + 1]
        x, y = int.from_bytes(buf[o + 0xC:o + 0x10], 'little'), int.from_bytes(buf[o + 0x10:o + 0x14], 'little')
        x, y = s32(x) >> 16, s32(y) >> 16
        live.append((i, t, x, y))
    cur = {(i, t): (x, y) for i, t, x, y in live}
    new = [k for k in cur if k not in prev]
    gone = [k for k in prev if k not in cur]
    print(f'{g} mm {mx:4d},{my:4d} objs ' + ' '.join(f'{i}/t{t}:({x},{y})' for i, t, x, y in live)
          + ('  NEW ' + ' '.join(f'{i}/t{t}' for i, t in new) if new else '')
          + ('  GONE ' + ' '.join(f'{i}/t{t}' for i, t in gone) if gone else ''))
    prev = cur
e.close()
