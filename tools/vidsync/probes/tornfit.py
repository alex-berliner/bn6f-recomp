"""tornfit.py RESULT.json STATE G1 G2 OUT.json [SLOTS] [R]: the video's tornado track, fitted by the picture.
From STATE (a ring state on RESULT's path, at or before G1) step the path's keys (past its end the video's
display keys at the handlers' mapping) to G1, then for every game frame g in G1..G2 and each tornado map
object slot in SLOTS (default: every live tornado, the one nearest MegaMan first): its X/Y written (px, as
a write before the step g -> g+1, i.e. a poke at g - 1, judged on the picture after the next step: it
shows the logic of the frame before) at the predicted place (the last fitted place plus
its last motion) +-R px (default 3), the step taken, and the picture compared with the video's frame (the
sync's cost, best over offsets -2..2); the best place is kept and the machine goes on from it. A slot whose
cost does not change with its place (off screen, under a cutscene) keeps its own motion. OUT.json: per frame
{g, slot: [natural x, y, fitted x, y, best cost, natural cost, spread]}."""
import os, sys, json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms
import beam
from emu import Emu

r = json.load(open(sys.argv[1])); p0 = r['press_gf']
G1, G2, out = int(sys.argv[3]), int(sys.argv[4]), sys.argv[5]
SLOTS = [int(x) for x in sys.argv[6].split(',')] if len(sys.argv) > 6 and sys.argv[6] != '-' else None
R = int(sys.argv[7]) if len(sys.argv) > 7 else 3
PNG = sys.argv[8] if len(sys.argv) > 8 else None        # ours | the video every PSTEP frames from PFROM
PFROM, PSTEP = (int(sys.argv[9]), int(sys.argv[10])) if len(sys.argv) > 10 else (G1, 4)
b = beam.Beam('data/full')
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s16 = lambda v: (v >> 16) - (1 << 16) if v >> 16 >= 1 << 15 else v >> 16
last = p0 + len(r['keys']) - 1
import re as _re
_m = _re.search(r'loss_(\d+)\.json$', sys.argv[1])
if _m: last = min(last, int(_m[1]))      # past a loss the path's mapping is the sync losing it
vL = r['vid'][last - p0]


def kv0(g):
    """(key, video frame) of the step g -> g + 1"""
    if g < last: return r['keys'][g - p0], r['vid'][g - p0]
    v = vL + int(round((g - last) * beam.RATE)); return b.key(v), v


FILL = int(os.environ.get('FILL_A', '0'))   # A released for at most FILL frames between presses: held


def kv(g):
    k, v = kv0(g)
    if FILL and k & 1:
        before = any(not kv0(g - d)[0] & 1 for d in range(1, FILL + 1))
        after = any(not kv0(g + d)[0] & 1 for d in range(1, FILL + 1))
        if before and after:
            k &= ~1
    return k, v


e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for pg, ps in r['pokes']:
    if pg >= g: e.poke(pg, ps)
while g < G1:
    e.step(kv(g)[0]); g += 1
A, T = 9000, 9001


def place(s):
    a = objs + s * osz
    return s16(e.u32(a + 0xC)), s16(e.u32(a + 0x10))


def live():
    buf = bytes(e.read(objs, osz * 56))
    return [i for i in range(56) if buf[i * osz] & 1 and buf[i * osz + 1] == 0x1A]


def cost_with(writes, k, v):
    """the picture shows the logic of the frame before (OAM goes out at the next vblank): a place written before
    the step g -> g+1 is judged on the picture after the step g+1 -> g+2"""
    e.load(A)
    for s, (x, y) in writes.items():
        a = objs + s * osz
        e.cmd(f'write32 {a + 0xC:x} {(x << 16) & 0xFFFFFFFF:x}'); e.cmd(f'write32 {a + 0x10:x} {(y << 16) & 0xFFFFFFFF:x}')
    e.step(k)
    k2, v2 = kv(g + 1)
    e.step(k2)
    return min(e.errs(v2 - 2, 5))


fit, vel, rows, pics = {}, {}, [], []
if PNG:
    import subprocess
    import numpy as np
    from PIL import Image
    src = 'data/small.mp4' if os.path.exists('data/small.mp4') else 'data/full.mp4'
    flt = '' if src.endswith('small.mp4') else ',crop=906:604:37:56,scale=240:160:flags=area'
    va, vb_ = kv(PFROM)[1] - 4, kv(G2)[1] + 4
    pre = max(va - 30, 0)
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', src, '-vf',
                          f"select=between(n\\,{va - pre}\\,{vb_ - pre}){flt}", '-frames:v', str(vb_ - va + 1),
                          '-vsync', '0', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
    nv = len(raw) // (160 * 240 * 3)
    VF = {va + i: np.frombuffer(raw[i * 115200:(i + 1) * 115200], np.uint8).reshape(160, 240, 3) for i in range(nv)}
e.save(A)
while g <= G2:
    k, v = kv(g)
    slots = SLOTS if SLOTS is not None else live()
    nat = {s: place(s) for s in slots}
    c0 = cost_with({}, k, v)
    row = {'g': g, 'v': v, 'natural_cost': round(c0, 3)}
    chosen = {}
    for s in slots:
        px, py = fit.get(s, nat[s])
        vx, vy = vel.get(s, (0, 0))
        cx, cy = px + vx, py + vy
        cands = {(cx + dx, cy + dy) for dx in range(-R, R + 1) for dy in range(-R, R + 1)} | {nat[s]}
        res = {c: cost_with({**chosen, s: c}, k, v) for c in cands}
        best = min(res, key=res.get)
        spread = max(res.values()) - min(res.values())
        if spread < 0.05:
            best = nat[s]                 # not seen: its own motion
        else:
            chosen[s] = best
        if s in fit: vel[s] = (best[0] - fit[s][0], best[1] - fit[s][1])
        fit[s] = best
        row[str(s)] = [nat[s][0], nat[s][1], best[0], best[1], round(res.get(best, c0), 3), round(res[nat[s]], 3),
                       round(spread, 3)]
    # go on from the chosen places
    e.load(A)
    for s, (x, y) in chosen.items():
        a = objs + s * osz
        e.cmd(f'write32 {a + 0xC:x} {(x << 16) & 0xFFFFFFFF:x}'); e.cmd(f'write32 {a + 0x10:x} {(y << 16) & 0xFFFFFFFF:x}')
    e.step(k); g += 1
    row['cost'] = round(min(e.errs(v - 2, 5)), 3)    # (this picture shows the frame before's places)
    if PNG and g >= PFROM and (g - PFROM) % PSTEP == 0 and v in VF:
        pics.append(np.hstack([e.shot(), np.full((160, 4, 3), 255, np.uint8), VF[v]]))
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    row['mm'] = [s16(e.u32(p + 0x1C)), s16(e.u32(p + 0x20))] if p else None
    row['f'] = bytes(e.read(syms.EVENT_FLAGS + (0xBE0 >> 3), 2)).hex()
    c = bytes(e.read(syms.WEATHER_COMP_STATE, 0x18))
    row['orbs'], row['c8'], row['key'] = c[6] | c[7] << 8, c[8], f'{k:03x}'
    e.save(A)
    rows.append(row)
    print(json.dumps(row), flush=True)
json.dump(rows, open(out, 'w'))
for i in range(0, len(pics), 10):
    Image.fromarray(np.vstack(pics[i:i + 10])).save(PNG.replace('.png', f'_{i // 10}.png'))
