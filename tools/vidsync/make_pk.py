"""make_pk.py RUN.json P K G OUT: a beam checkpoint (OUT.json/.state) for a pk_search pair: NEW GAME pressed at
game frame P, then RUN's keys with K frames of no input in Lan's room before the R press at AT (RUN has BASE_K
there), stopping at game frame G of RUN's original timeline. The video mapping repeats the last frame over
inserted waits and drops it over removed ones."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import beam as bm
from emu import Emu
from pk_search import AT, BASE_K
run, P, K, G, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
R = json.load(open(run)); P0 = R['press_gf']; KS, VS = R['keys'], R['vid']
a = AT - P0
keys = KS[:a] + [0x3FF] * K + KS[a + BASE_K:a + BASE_K + G - AT]
vid = VS[:a] + [VS[a - 1]] * K + VS[a + BASE_K:a + BASE_K + G - AT]
e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=P - 402)
i = 0
while i < len(keys):
    j = i
    while j < len(keys) and keys[j] == keys[i]: j += 1
    e.step(keys[i], j - i); i = j
g, h = e.gf()
v0 = vid[-1] - bm.RATE * (h - 1)
e.savef(out + '.state')
json.dump({'press_gf': P, 'g': g, 'h': h, 'v0': v0, 'cost': [0.5 * abs(o) for o in bm.OFFS],
           'keys': keys, 'vid': vid, 'frame_cost': [], 'pk': [P, K]}, open(out + '.json', 'w'))
print('checkpoint at game frame', g, 'hw', h, 'video', vid[-1], '| GMD bytes', e.read(0x0200434C, 4).hex(),
      'secondary', hex(e.u32(0x02001120)))
e.close()
