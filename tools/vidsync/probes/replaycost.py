"""replaycost.py RESULT.json STATE G1 G2 POKES_JSON [SPAN]: replay RESULT's path from STATE (a ring state on
it) with the run's pokes and POKES_JSON's ([[g, spec], ...] or -), the keys held where the corrected display
(bits_fixes.json) holds A, past a loss_D.json's D the video's display keys at the handlers' mapping; for
each game frame in G1..G2 print the key, the picture's cost at video offsets -SPAN..SPAN (default 6) around
the mapping (the best marked), flags 0xBE0-0xBEF, eWeatherCompState +6/+8/+0xb/+0x16 and the cutscene state's
first bytes: is our game early or late against the video, and from which frame."""
import os, sys, json, re
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import syms
import beam
from emu import Emu

rj, state = sys.argv[1], sys.argv[2]
G1, G2 = int(sys.argv[3]), int(sys.argv[4])
extra = json.load(open(sys.argv[5])) if sys.argv[5] != '-' else []
SPAN = int(sys.argv[6]) if len(sys.argv) > 6 else 6
PNG = sys.argv[7] if len(sys.argv) > 7 else None      # ours | the video at the best offset, every PSTEP frames
PSTEP = int(sys.argv[8]) if len(sys.argv) > 8 else 4
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam('data/full')
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


pics = []
if PNG:
    import subprocess
    import numpy as np
    from PIL import Image
    src = 'data/small.mp4' if os.path.exists('data/small.mp4') else 'data/full.mp4'
    flt = '' if src.endswith('small.mp4') else ',crop=906:604:37:56,scale=240:160:flags=area'
    va, vb_ = kv(G1)[1] - SPAN - 2, kv(G2)[1] + SPAN + 2
    pre = max(va - 30, 0)
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', src, '-vf',
                          f"select=between(n\\,{va - pre}\\,{vb_ - pre}){flt}", '-frames:v', str(vb_ - va + 1),
                          '-vsync', '0', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
    VF = {va + i: np.frombuffer(raw[i * 115200:(i + 1) * 115200], np.uint8).reshape(160, 240, 3)
          for i in range(len(raw) // 115200)}
e = Emu(); e.loadf(state); g, _ = e.gf()
for pg, ps in list(r['pokes']) + extra:
    if pg >= g: e.poke(pg, ps)
CUT = getattr(syms, 'CUTSCENE_STATE', None)
while g < G2:
    k, v = kv(g); e.step(k); g += 1
    if g < G1: continue
    c = e.errs(v - SPAN, 2 * SPAN + 1)
    i = min(range(len(c)), key=c.__getitem__)
    comp = bytes(e.read(syms.WEATHER_COMP_STATE, 0x18))
    print(f'{g} key {k:03x} v {v} best {i - SPAN:+d} {c[i]:.2f} at0 {c[SPAN]:.2f} | ' + ' '.join(f'{x:.1f}' for x in c)
          + f' | f {bytes(e.read(syms.EVENT_FLAGS + (0xBE0 >> 3), 2)).hex()} orbs {comp[6]} c8 {comp[8]} left {comp[0xb]}'
          + (f' cut {bytes(e.read(CUT, 8)).hex()}' if CUT else ''), flush=True)
    if PNG and (g - G1) % PSTEP == 0 and v + i - SPAN in VF:
        pics.append(np.hstack([e.shot(), np.full((160, 4, 3), 255, np.uint8), VF[v + i - SPAN]]))
for j in range(0, len(pics), 10):
    Image.fromarray(np.vstack(pics[j:j + 10])).save(PNG.replace('.png', f'_{j // 10}.png'))
