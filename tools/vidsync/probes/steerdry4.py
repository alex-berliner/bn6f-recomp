"""steerdry4.py STATE RESULTJSON D: the free walk (0xBE4 clear at the natural catch -2) plus one re-arm
write at D+ra, swept; prints each candidate's reach and the best one's cost trace."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu
import beam

state, rj, D = sys.argv[1], sys.argv[2], int(sys.argv[3])
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam(os.path.dirname(os.path.abspath(rj)) + '/../../full')
FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3); M4 = 0x80 >> (0xBE4 & 7); M6 = 0x80 >> (0xBE6 & 7)
HOLD = 600; hi = D + 130
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
pre = [[gc, f'{FA4:#x}={(fnat[gc] & ~M4) & ~M6:#04x}/1']]
e.loadf(state)
fnat2 = {}
pend = {g: [spec] for g, spec in pre}
for g in range(g0, hi):
    for spec in pend.pop(g, []): e.poke(g, spec)
    e.step(ks[g - p0][0])
    fnat2[g] = e.read(FA4, 1)[0]

def run(pokes, show=False):
    e.loadf(state)
    pend = {}
    for g, spec in pokes: pend.setdefault(g, []).append(spec)
    bad = 0
    for g in range(g0, D + HOLD):
        for spec in pend.pop(g, []): e.poke(g, spec)
        e.step(ks[g - p0][0])
        c = min(e.errs(ks[g - p0][1] - 3, 7))
        if show and D + 40 <= g <= D + 130: print(f'  {g} cost {c:5.2f}')
        bad = bad + 1 if c > 2.8 else 0
        if bad >= 30: return g - 30
    return None

best = None
for ra in range(74, 97, 2):
    reach = run(pre + [[D + ra, f'{FA4:#x}={(fnat2[D + ra] & ~M6) | M4:#04x}/1']])
    print(f're-arm at D+{ra}: reach {reach if reach else "HOLDS TO BAR"}')
    if reach is None and best is None: best = ra
if best is not None:
    print(f'--- cost trace, re-arm at D+{best} ---')
    run(pre + [[D + best, f'{FA4:#x}={(fnat2[D + best] & ~M6) | M4:#04x}/1']], show=True)
e.close()
