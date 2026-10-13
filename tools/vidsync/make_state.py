"""make_state.py RESULT.json G OUT: replay a result's keys straight from boot to game frame G and save
a checkpoint there (OUT.state, OUT.json), so searches start from exactly that path."""
import sys, os, json; sys.path.insert(0, os.path.dirname(__file__))
from emu import Emu
res, G, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
R = json.load(open(res)); P0 = R['press_gf']; K = R['keys']
e = Emu(); e.step(n=400); e.step(0x3FF & ~8, 2); e.step(n=P0 - 402)
i = 0
while P0 + i < G:
    j = i
    while P0 + j < G and K[j] == K[i]: j += 1
    e.step(K[i], j - i); i = j
g, h = e.gf(); e.savef(out + '.state')
json.dump({'g': g, 'h': h, 'press_gf': P0}, open(out + '.json', 'w'))
print('state at game frame', g, 'hw', h, '->', out)
