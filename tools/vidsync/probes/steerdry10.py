"""steerdry10.py STATE RESULTJSON D ARM: the clear at the natural catch -2; the tornado slot 1 onto MegaMan
for [ARM-4, ARM+2]; the arm (0xBE4) + the enable (0xBEA) at D+ARM. Dumps f4/f9/chatbox flags + costs."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
ARM = int(sys.argv[4])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
F6 = syms.EVENT_FLAGS + (0xBE6 >> 3)
FAA = syms.EVENT_FLAGS + (0xBEA >> 3); MA = 0x80 >> (0xBEA & 7)
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
N_OBJ = 56; HOLD = 600; hi = D + 140
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
fa = e.read(FA4, 1)[0]; fb = e.read(FAA, 1)[0]
CLEAR = [gc, f'{FA4:#x}={(fnat[gc] & ~M4) & ~M6:#04x}/1']
ARMENTRY = [D + ARM, f'{FA4:#x}={(fa & ~M6) | M4:#04x}/1', f'{FAA:#x}={(fb | MA):#02x}/1']
e.loadf(state)
pend = {gc: [CLEAR[1]]}
pos_d = {}
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    if p: pos_d[g] = (e.u32(p + 0x1C), e.u32(p + 0x20))

def run(pokes, show=False):
    e.loadf(state)
    pend = {}
    for entry in pokes:
        g, specs = entry[0], entry[1:]
        pend.setdefault(g, []).extend(specs)
    bad = 0; caught = None
    for g in range(g0, D + HOLD):
        for spec in pend.pop(g, []): e.poke(g, spec)
        e.step(ks[g - p0][0])
        c = min(e.errs(ks[g - p0][1] - 3, 7))
        if caught is None and e.read(F6, 1)[0] & M6: caught = g
        if show and D + 38 <= g <= D + 120:
            f4 = e.read(FA4, 1)[0]; f9 = e.read(FAA, 1)[0]
            cb = e.read(0x2009F38, 1)[0]
            print(f'  {g} cost {c:5.2f} f4={f4:02x} f9={f9:02x} cb={cb:02x}')
        bad = bad + 1 if c > 2.8 else 0
        if bad >= 30: return g - 30, caught
    return None, caught

def tpoke(g):
    return [g, ','.join(f'{objs + 1 * osz + o:#x}={pos_d[g][j]:#x}/4' for o, j in ((0xC, 0), (0x10, 1)))]

pokes = [CLEAR, ARMENTRY] + [tpoke(g) for g in range(D + ARM - 4, D + ARM + 3) if g in pos_d]
reach, caught = run(pokes, show=True)
print(f'arm at D+{ARM}: reach {reach if reach else "HOLDS TO BAR"} catch0 {caught}')
e.close()
