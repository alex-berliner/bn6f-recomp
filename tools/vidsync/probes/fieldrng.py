"""fieldrng.py LOSS_JSON STATE TRIGGER ENTRY: does the battle field depend on the RNG? From STATE, the loss's keys,
the battle pointer = ENTRY at TRIGGER, and the primary seed moved by j at a frame W (j in -3..3, W in a few
frames after the trigger); prints a hash of the field rows at the battle's start for each, and saves pictures."""
import sys, json, hashlib
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__))))   # tools/vidsync
import numpy as np
from PIL import Image
from emu import Emu
import rngfix as rf
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; f = int(sys.argv[3]); ent = int(sys.argv[4], 16)
def run(extra, until):
    e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
    for p in r['pokes']:
        if p[0] >= g: e.poke(*p)
    for p in extra: e.poke(*p)
    for gg in range(g, until): e.step(r['keys'][gg - g0] if gg - g0 < len(r['keys']) else 0x3FF)
    shot = e.shot(); e.close(); return shot
base = [[f, rf.spec(0x02001B9C, ent)]]
end = int(sys.argv[5]) if len(sys.argv) > 5 else f + 100
shots = {}
for W in (f + 2, f + 20, f + 40, f + 60):
    for j in (-2, -1, 0, 1, 2):
        e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
        for p in r['pokes']:
            if p[0] >= g: e.poke(*p)
        e.poke(*base[0])
        for gg in range(g, W): e.step(r['keys'][gg - g0])
        P = e.u32(rf.PRIMARY); e.close()
        extra = base + ([[W, rf.spec(rf.PRIMARY, rf.rng_move(P, j)) + ',' + rf.spec(0x0203CBE4, rf.rng_move(P, j))]] if j else [])
        s = run(extra, end); field = s[90:160]
        h = hashlib.md5(field.tobytes()).hexdigest()[:8]; shots[(W, j)] = s
        print(W, j, h, flush=True)
Image.fromarray(np.vstack([np.hstack([shots[(W, j)] for j in (-2, -1, 0, 1, 2)]) for W in (f + 2, f + 60)])).save(sys.argv[6] if len(sys.argv) > 6 else 'fieldrng.png')
