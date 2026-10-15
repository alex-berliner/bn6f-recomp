"""pokeprobe.py STATE: load the state, queue old pokes one at a time, step one frame, and see which of them
actually fires (reads the written words before/after)."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from emu import Emu

state = sys.argv[1]
e = Emu(); e.loadf(state)
g0, _ = e.gf()
print(f'state at {g0}')
r = json.load(open('data/auto/run7n.1/ckpt_0435000.json')) if len(sys.argv) < 3 else json.load(open(sys.argv[2]))
for pg, ps in r['pokes'][-3:]:
    # parse the spec's first address
    spec = ps.split(',')[0]
    addr_s, val_s = spec.split('=')
    addr = int(addr_s, 16); n = 2 if val_s.endswith('/2') else 4 if val_s.endswith('/4') else 1
    before = e.read(addr, n)
    e.poke(pg, ps)
    e.step(0x3FF)
    after = e.read(addr, n)
    g1, _ = e.gf()
    print(f'poke {pg} {spec}: before {before.hex()} after {after.hex()} now at {g1} -> {"FIRED" if before != after else "inert"}')
    e.loadf(state)   # rewind for the next test
e.close()
