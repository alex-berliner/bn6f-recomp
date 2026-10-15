"""steerdry.py STATE RESULTJSON D: the steer handler's candidates, judged by hand: 0xBE4 clear + the tornado
pinned to the far corner just before the natural catch, the tornado onto MegaMan's free walk from D+on0 to
D+rearm+2, 0xBE4 set at D+rearm; path_reach-style replay with the per-frame cost printed near the end."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms, numpy as np
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
N_OBJ = 56
HOLD = 600
hi = D + 130
lv = r['vid'][-1]
n0 = len(r['keys'])
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
print(f'from {g0}, catch at {hit}, slots {sorted(slots)}')
gc = hit - 2
cx = (min(s32(v[0]) >> 16 for v in pos0.values()) - 1024) << 16 & 0xFFFFFFFF
cy = (min(s32(v[1]) >> 16 for v in pos0.values()) - 768) << 16 & 0xFFFFFFFF
corner = ','.join(f'{objs + i * osz + o:#x}={(cx if o == 0xC else cy):#x}/4' for i in sorted(slots) for o in (0xC, 0x10))
pre = [[gc, f'{FA4:#x}={(fnat[gc] & ~M4) & ~M6:#04x}/1'], [gc, corner]]

def run(pokes, show=True):
    e.loadf(state)
    byf = {}
    for g, spec in pokes: byf.setdefault(g, []).append(spec)
    pending = dict(byf)
    bad = 0; reach = None
    for g in range(g0, D + HOLD):
        for spec in pending.pop(g, []): e.poke(g, spec)
        e.step(ks[g - p0][0])
        c = min(e.errs(ks[g - p0][1] - 3, 7))
        if show and D + 20 <= g <= D + 60:
            sub = e.read(syms.GS_SUBSYSTEM, 1)[0]
            p = e.u32(syms.GS_OW_PLAYER_PTR)
            mm = f'{s32(e.u32(p + 0x1C)) >> 16},{s32(e.u32(p + 0x20)) >> 16}' if p else '-'
            buf = bytes(e.read(objs, osz * N_OBJ))
            tors = [(int.from_bytes(buf[i * osz + 0xC:i * osz + 0x10], 'little'),
                     int.from_bytes(buf[i * osz + 0x10:i * osz + 0x14], 'little'))
                    for i in range(N_OBJ) if buf[i * osz] & 1 and buf[i * osz + 1] == 0x1A]
            ts = ' '.join(f'({s32(x) >> 16},{s32(y) >> 16})' for x, y in tors)
            print(f'  {g} cost {c:5.2f} sub {sub:#04x} mm {mm} tor {ts}')
        bad = bad + 1 if c > 2.8 else 0
        if bad >= 30: reach = g - 30; break
    return reach

# the free walk replay (clear + pin): MegaMan's places and the flags byte
e.loadf(state)
byf = {}
for g, spec in pre: byf.setdefault(g, []).append(spec)
pos_d, fnat2 = {}, {}
pend = dict(byf)
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    fnat2[g] = e.read(FA4, 1)[0]
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    if p: pos_d[g] = (e.u32(p + 0x1C), e.u32(p + 0x20))
print('free walk replayed')
for on0, rearm in ((50, 92), (56, 92), (62, 92), (50, 96), (44, 90)):
    place = [[g, ','.join(f'{objs + i * osz + o:#x}={pos_d[g][j]:#x}/4' for i in sorted(slots) for o, j in ((0xC, 0), (0x10, 1)))]
             for g in range(D + on0, D + rearm + 3)]
    pin = [[g, corner] for g in range(gc + 16, D + on0, 16)]
    arm = [[D + rearm, f'{FA4:#x}={(fnat2[D + rearm] & ~M6) | M4:#04x}/1']]
    reach = run(pre + pin + place + arm, show=(on0, rearm) == (50, 92))
    print(f'on0 D+{on0} rearm D+{rearm}: reach {reach} ({(reach - D) if reach else "HOLDS"} )')
e.close()
