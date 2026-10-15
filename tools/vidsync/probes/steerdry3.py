"""steerdry3.py STATE RESULTJSON D: the station-track steer; per catch frame t the reach and the best
candidate's cost trace."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
N_OBJ = 56; HOLD = 600; hi = D + 130
TRACK0, HOLD_OFF = 48, 58
STATION, CLOSE = (28 << 16, 22 << 16), (6 << 16, 5 << 16)
lv = r['vid'][-1]; n0 = len(r['keys'])
ks = list(zip(r['keys'], r['vid'])) + [(b.key(lv + int(round(k * beam.RATE))), lv + int(round(k * beam.RATE)))
                                       for k in range(1, hi - p0 - n0 + 3)]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
slots, pos0, hit, fnat = set(), {}, None, {}
for g in range(g0, D + 9):
    if g > g0: e.step(ks[g - p0][0])
    fnat[g] = e.read(FA4, 1)[0]
    if e.read(syms.EVENT_FLAGS + (0xBE6 >> 3), 1)[0] & M6 and hit is None: hit = g
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    if p: pos0[g] = (e.u32(p + 0x1C), e.u32(p + 0x20))
    buf = bytes(e.read(objs, osz * N_OBJ))
    for i in range(N_OBJ):
        o = i * osz
        if buf[o] & 1 and buf[o + 1] == 0x1A: slots.add(i)
gc = hit - 2
cx = (min(s32(v[0]) >> 16 for v in pos0.values()) - 1024) << 16 & 0xFFFFFFFF
cy = (min(s32(v[1]) >> 16 for v in pos0.values()) - 768) << 16 & 0xFFFFFFFF
corner = ','.join(f'{objs + i * osz + o:#x}={(cx if o == 0xC else cy):#x}/4' for i in sorted(slots) for o in (0xC, 0x10))
pre = [[gc, f'{FA4:#x}={(fnat[gc] & ~M4) & ~M6:#04x}/1'], [gc, corner]]
e.loadf(state)
byf = {}
for g, spec in pre: byf.setdefault(g, []).append(spec)
pend = dict(byf)
pos_d, fnat2 = {}, {}
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    fnat2[g] = e.read(FA4, 1)[0]
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    if p: pos_d[g] = (e.u32(p + 0x1C), e.u32(p + 0x20))

def build(t):
    place = []
    for g in range(D + TRACK0, D + t + 4):
        if g <= D + HOLD_OFF: ox, oy = STATION
        elif g < D + t:
            f = (g - D - HOLD_OFF) / (t - HOLD_OFF)
            ox, oy = (int(STATION[0] + (CLOSE[0] - STATION[0]) * f), int(STATION[1] + (CLOSE[1] - STATION[1]) * f))
        else: ox, oy = 0, 0
        place.append([g, ','.join(f'{objs + i * osz + o:#x}={(pos_d[g][j] + (ox if o == 0xC else oy)) & 0xFFFFFFFF:#x}/4'
                                  for i in sorted(slots) for o, j in ((0xC, 0), (0x10, 1)))])
    pin = [[g, corner] for g in range(gc + 16, D + TRACK0, 16)]
    arm = [[D + t, f'{FA4:#x}={(fnat2[D + t] & ~M6) | M4:#04x}/1']]
    return pre + pin + place + arm

def run(pokes, show=False):
    e.loadf(state)
    byf = {}
    for g, spec in pokes: byf.setdefault(g, []).append(spec)
    pend = dict(byf); bad = 0
    for g in range(g0, D + HOLD):
        for spec in pend.pop(g, []): e.poke(g, spec)
        e.step(ks[g - p0][0])
        c = min(e.errs(ks[g - p0][1] - 3, 7))
        if show and D + 40 <= g <= D + 130: print(f'  {g} cost {c:5.2f}')
        bad = bad + 1 if c > 2.8 else 0
        if bad >= 30: return g - 30
    return None

best = None
for t in range(84, 99, 2):
    reach = run(build(t))
    print(f'catch at D+{t}: reach {reach if reach else "HOLDS TO BAR"}')
    if reach is None and best is None: best = t
if best is not None:
    print(f'--- cost trace, catch at D+{best} ---')
    run(build(best), show=True)
e.close()
