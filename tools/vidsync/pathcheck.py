"""pathcheck.py RESULT.json [--save G OUT]: replay a result (keys, video frames, pokes) with plain steps, as the
route harness does (each poke once), and score every frame against its video frame (best of offsets -3..3).
Prints the mean cost per 500 frames and the first stretch of 30 frames above the loss cost: a path the sync
built only holds if it holds here. --save G OUT: stop at game frame G and write the checkpoint OUT.json/.state
(the pokes before G) for autosync to resume from."""
import sys, os, json, argparse
sys.path.insert(0, os.path.dirname(__file__))
from emu import Emu
import rngfix as rf
import beam as bm
ap = argparse.ArgumentParser(); ap.add_argument('result'); ap.add_argument('--save', nargs=2)
A = ap.parse_args()
r = json.load(open(A.result)); g0 = r['press_gf']
G = int(A.save[0]) if A.save else None
pokes = [p for p in r.get('pokes', []) if G is None or p[0] < G]
e = Emu()
for g, s in pokes: e.poke(g, s)
e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=g0 - 402)
bad, tot, n, first, masked = 0, 0.0, 0, None, False
for i, (k, v) in enumerate(zip(r['keys'], r['vid'])):
    g = g0 + i + 1                       # the frame's picture (after its key) is the video's frame v
    if G is not None and g > G: break
    ov = any(a <= g < z + 300 for a, z in r.get('lenient', []))   # a stream overlay's window (+300: alerts outlast it)
    if ov != masked: e.cmd('mask ' + ('0 52 6 114' if ov else '')); masked = ov
    e.step(k)
    c = min(e.errs(v - 3, 7))
    tot += c; n += 1
    free = any(a <= g < z for a, z in r.get('free', []))   # a kill's frames up to its deletion: not the video's
    bad = bad + 1 if c > rf.LOSS and not free else 0
    if bad == 30 and first is None:
        first = g - 30; print(f'LOST at game {first} (video {r["vid"][i - 30]})', flush=True)
    if g % 500 == 0:
        print(f'game {g} mean {tot / n:.2f}', flush=True); tot, n = 0.0, 0
if G is not None:
    gg, h = e.gf(); assert gg == G, (gg, G)
    m = G - g0
    e.savef(A.save[1] + '.state')
    json.dump({'press_gf': g0, 'g': G, 'h': h, 'v0': r['vid'][m - 1] - bm.RATE * h,
               'cost': [0.5 * abs(o) for o in bm.OFFS], 'keys': r['keys'][:m], 'vid': r['vid'][:m],
               'frame_cost': [], 'pokes': pokes, 'lenient': r.get('lenient', []), 'free': r.get('free', [])}, open(A.save[1] + '.json', 'w'))
    print('saved', A.save[1], 'game', G, 'hw', h)
print('holds to the end' if first is None else f'first loss {first}', 'pokes', len(pokes))
