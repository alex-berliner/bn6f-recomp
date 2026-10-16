"""tornglobal.py RESULT.json STATE SLOT G1,G2,... [HALF] [STEP]: where does the video show a Weather comp
tornado? Replay RESULT's path from STATE (env EXTRA: more pokes [[g, spec], ...]; past a loss_D.json's D the
video's display keys at the handlers' mapping) and at each listed game frame G put tornado SLOT's X/Y (a
write before the step G -> G+1) on a grid of +-HALF px (default 160) around MegaMan, STEP px apart (default
8), then 1 px around the best; judged on the picture after the next step (it shows the logic of the frame
before) against the video. Prints per G: the best place, its cost, the cost where the tornado is, the
median over the grid (a place far below the median: the video shows a tornado there)."""
import os, sys, json, re
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import numpy as np
import syms
import beam
from emu import Emu

rj, state, SLOT = sys.argv[1], sys.argv[2], int(sys.argv[3])
GS = sorted(int(x) for x in sys.argv[4].split(','))
HALF = int(sys.argv[5]) if len(sys.argv) > 5 else 160
STEP = int(sys.argv[6]) if len(sys.argv) > 6 else 8
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam('data/full')
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
o = objs + SLOT * osz
s16 = lambda v: (v >> 16) - (1 << 16) if v >> 16 >= 1 << 15 else v >> 16
last = p0 + len(r['keys']) - 1
m_ = re.search(r'loss_(\d+)\.json$', rj)
if m_: last = min(last, int(m_[1]))
vL = r['vid'][last - p0]


def kv(g):
    if g < last:
        k, v = r['keys'][g - p0], r['vid'][g - p0]
        if k & 1 and not b.key(v) & 1: k &= ~1
        return k, v
    v = vL + int(round((g - last) * beam.RATE)); return b.key(v), v


e = Emu(); e.loadf(state); g, _ = e.gf()
EXTRA = json.load(open(os.environ['EXTRA'])) if os.environ.get('EXTRA') else []
for pg, ps in list(r['pokes']) + EXTRA:
    if pg >= g: e.poke(pg, ps)
A = 9000


def cost(x, y):
    """(held paused while it is placed: a moving tornado on a blocked tile re-picks its direction forever,
    owMapObject_weatherTornado_repickDirection_80A90E4, and the frame never ends)"""
    e.load(A)
    e.cmd(f'write32 {o + 0xC:x} {(x << 16) & 0xFFFFFFFF:x}'); e.cmd(f'write32 {o + 0x10:x} {(y << 16) & 0xFFFFFFFF:x}')
    w8 = e.u32(o + 8); e.cmd(f'write32 {o + 8:x} {(w8 & 0xFFFF00FF) | 0x100:x}')
    w24 = e.u32(o + 0x24); e.cmd(f'write32 {o + 0x24:x} {(w24 & 0xFFFF00FF) | 0x2000:x}')
    e.step(kv(g)[0]); k2, v2 = kv(g + 1); e.step(k2)
    return min(e.errs(v2 - 2, 5))


import time
T0 = time.time()
for G in GS:
    while g < G:
        e.step(kv(g)[0]); g += 1
    e.save(A)
    p = e.u32(syms.GS_OW_PLAYER_PTR); mx, my = s16(e.u32(p + 0x1C)), s16(e.u32(p + 0x20))
    tx, ty = s16(e.u32(o + 0xC)), s16(e.u32(o + 0x10))
    grid = {(mx + dx, my + dy): cost(mx + dx, my + dy) for dx in range(-HALF, HALF + 1, STEP)
            for dy in range(-HALF, HALF + 1, STEP)}
    bx, by = min(grid, key=grid.get)
    fine = {(bx + dx, by + dy): cost(bx + dx, by + dy) for dx in range(-STEP, STEP + 1) for dy in range(-STEP, STEP + 1)}
    fx, fy = min(fine, key=fine.get)
    here = cost(tx, ty)
    print(f'{G}: mm ({mx},{my}) tornado {SLOT} at ({tx},{ty}) state {bytes(e.read(o + 8, 2)).hex()} cost there {here:.2f}; '
          f'grid median {float(np.median(list(grid.values()))):.2f}; best ({fx},{fy}) {fine[(fx, fy)]:.2f} '
          f'(rel. MegaMan {fx - mx:+d},{fy - my:+d}) [{time.time() - T0:.0f} s]', flush=True)
    e.load(A)
