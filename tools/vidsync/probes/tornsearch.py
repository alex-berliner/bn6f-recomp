"""tornsearch.py RESULT.json STATE SLOT WT1 WT2 GA GB [TOP]: which single write of a Weather comp tornado's
movement state makes its track the video's? From STATE (a ring state on RESULT's path) with the path's keys
(A released for at most FILL_A frames between presses held, env FILL_A, default 2; past the loss the video's
display keys at the handlers' mapping), at each frame Wt in WT1..WT2 (step 2) the tornado in map object slot
SLOT gets: mode +9 = 0 (moving), direction +5 = d (0..7), move countdown +0x24 = m (2..46 step 2), moves since
the last pause +0x28 = n (0..2) -- a write before the step Wt -> Wt+1 (a poke at Wt - 1). Each candidate is
replayed to GB and scored by the mean picture cost over GA..GB (the sync's cost, best of offsets -2..2).
Prints the TOP best (default 15) and, for the best, the cost and the tornado's place every 4 frames."""
import os, sys, json, re
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms
import beam
from emu import Emu

rj, state, SLOT = sys.argv[1], sys.argv[2], int(sys.argv[3])
WT1, WT2, GA, GB = (int(x) for x in sys.argv[4:8])
TOP = int(sys.argv[8]) if len(sys.argv) > 8 else 15
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam('data/full')
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
o = objs + SLOT * osz
s16 = lambda v: (v >> 16) - (1 << 16) if v >> 16 >= 1 << 15 else v >> 16
last = p0 + len(r['keys']) - 1
m_ = re.search(r'loss_(\d+)\.json$', rj)
if m_: last = min(last, int(m_[1]))
vL = r['vid'][last - p0]
FILL = int(os.environ.get('FILL_A', '2'))


def kv0(g):
    if g < last: return r['keys'][g - p0], r['vid'][g - p0]
    v = vL + int(round((g - last) * beam.RATE)); return b.key(v), v


def kv(g):
    k, v = kv0(g)
    if FILL and k & 1 and any(not kv0(g - d)[0] & 1 for d in range(1, FILL + 1)) \
            and any(not kv0(g + d)[0] & 1 for d in range(1, FILL + 1)):
        k &= ~1
    return k, v


e = Emu(); e.loadf(state); g, _ = e.gf()
for pg, ps in r['pokes']:
    if pg >= g: e.poke(pg, ps)
base = {}
while g <= WT2:
    if g >= WT1 and (g - WT1) % int(os.environ.get("WT_STEP", "2")) == 0:
        base[g] = 9100 + len(base); e.save(base[g])
    e.step(kv(g)[0]); g += 1


def run(wt, d, m, n, trace=False):
    e.load(base[wt])
    e.cmd(f'write32 {o + 8:x} {(e.u32(o + 8) & 0xFFFF00FF) & 0xFFFFFFFF:x}')        # +9 mode 0 (keeps +8, +A, +B)
    b5 = e.u32(o + 4); e.cmd(f'write32 {o + 4:x} {(b5 & 0xFFFF00FF) | d << 8:x}')     # +5 direction
    w24 = e.u32(o + 0x24); e.cmd(f'write32 {o + 0x24:x} {(w24 & 0xFFFFFF00) | m:x}')  # +0x24 countdown
    w28 = e.u32(o + 0x28); e.cmd(f'write32 {o + 0x28:x} {(w28 & 0xFFFFFF00) | n:x}')  # +0x28 moves
    tot, cnt, rows = 0.0, 0, []
    for gg in range(wt, GB):
        k, v = kv(gg); e.step(k)
        if gg + 1 >= GA:
            c = min(e.errs(v - 2, 5)); tot += c; cnt += 1
            if trace and (gg + 1) % 4 == 0:
                rows.append((gg + 1, round(c, 2), s16(e.u32(o + 0xC)), s16(e.u32(o + 0x10)), e.read(o + 8, 2).hex(),
                             bytes(e.read(syms.EVENT_FLAGS + (0xBE0 >> 3), 2)).hex()))
    return tot / max(cnt, 1), rows


res = []
if os.environ.get('CANDS'):      # "wt,d,m,n;...": only these (Wt among the saved frames)
    for c in os.environ['CANDS'].split(';'):
        wt, d, m, n = (int(x) for x in c.split(','))
        res.append((run(wt, d, m, n)[0], wt, d, m, n))
else:
    for wt in sorted(base):
        for d in range(8):
            for m in range(2, 47, 2):
                for n in range(3):
                    res.append((run(wt, d, m, n)[0], wt, d, m, n))
res.sort()
for x in res[:TOP]:
    print('cost %.3f  Wt %d dir %d mv %d n %d' % x, flush=True)
c, wt, d, m, n = res[0]
for row in run(wt, d, m, n, True)[1]:
    print(*row)
