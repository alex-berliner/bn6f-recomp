"""make_resume.py RESULT.json AT K G OUT: a beam checkpoint (OUT.json/.state) that replays RESULT's keys
with K frames of no input inserted before game frame AT (a wait that steers a random roll), stopping
at game frame G of the original timeline (G+K after the wait). The video mapping is shifted by the
inserted frames, which the video never had."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import beam as bm
from emu import Emu
res, AT, K, G, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
R = json.load(open(res)); P0 = R['press_gf']
keys = R['keys'][:AT - P0] + [0x3FF] * K + R['keys'][AT - P0:G - P0]
vid = R['vid'][:AT - P0] + [R['vid'][AT - P0 - 1]] * K + R['vid'][AT - P0:G - P0]
e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=P0 - 402)
i = 0
while i < len(keys):
    j = i
    while j < len(keys) and keys[j] == keys[i]: j += 1
    e.step(keys[i], j - i); i = j
g, h = e.gf()
v0 = vid[-1] - bm.RATE * (h - 1)
e.savef(out + '.state')
json.dump({'press_gf': P0, 'g': g, 'h': h, 'v0': v0, 'cost': [0.5 * abs(o) for o in bm.OFFS],
           'keys': keys, 'vid': vid, 'frame_cost': [], 'inserted': [[AT, K]]}, open(out + '.json', 'w'))
print('checkpoint at game frame', g, 'hw', h, 'video', vid[-1], '->', out, '| GMD bytes', e.read(0x0200434C, 4).hex())
