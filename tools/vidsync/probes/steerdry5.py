"""steerdry5.py STATE RESULTJSON D RA: the free walk + one re-arm at D+RA; prints the catch frame (0xBE6),
the comp state's key fields, and the cost trace D+40..D+140."""
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
CS = syms.WEATHER_COMP_STATE
HOLD = 600; hi = D + 140
lv = r['vid'][-1]; n0 = len(r['keys'])
ks = list(zip(r['keys'], r['vid'])) + [(b.key(lv + int(round(k * beam.RATE))), lv + int(round(k * beam.RATE)))
                                       for k in range(1, hi - p0 - n0 + 3)]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
hit = None; fnat = {}
for g in range(g0, D + 9):
    if g > g0: e.step(ks[g - p0][0])
    fnat[g] = e.read(FA4, 1)[0]
    if e.read(syms.EVENT_FLAGS + (0xBE6 >> 3), 1)[0] & M6 and hit is None: hit = g
gc = hit - 2
pre = [[gc, f'{FA4:#x}={(fnat[gc] & ~M4) & ~M6:#04x}/1'],
       [D + RA, f'{FA4:#x}={(e.read(FA4, 1)[0] & ~M6) | M4:#04x}/1']]
e.loadf(state)
pend = {}
for g, spec in pre: pend.setdefault(g, []).append(spec)
caught = None; bad = 0
for g in range(g0, D + HOLD):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    c = min(e.errs(ks[g - p0][1] - 3, 7))
    if D + 40 <= g <= D + 139:
        buf = bytes(e.read(CS + 0x10, 0x40))
        w = ' '.join(f'{i * 2 + 0x10:02x}:{int.from_bytes(buf[i * 2:i * 2 + 2], "little"):04x}' for i in range(0x20))
        print(f'  {g} cost {c:5.2f} | {w}')
    bad = bad + 1 if c > 2.8 else 0
    if caught is None and e.read(syms.EVENT_FLAGS + (0xBE6 >> 3), 1)[0] & M6: caught = g
    if bad >= 30:
        print(f'BROKE at {g - 30}, catch(0xBE6) at {caught}')
        break
else:
    print(f'HOLDS TO BAR, catch(0xBE6) at {caught}')
e.close()
