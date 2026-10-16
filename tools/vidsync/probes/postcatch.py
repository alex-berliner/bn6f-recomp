"""postcatch.py STATE LOSS_JSON D G1 G2 STEP OUT.png [POKES_JSON] [SHIFT]: from STATE (a machine at some game
frame G0 <= G1, e.g. the nocatch handler's tornado_nocatch_tail.state), step the video's display keys (the
mapping the handlers use past a loss: vid[D] + round((g - D) * RATE), shifted SHIFT frames early) to G2 and
save emulator | video at full size every STEP game frames from G1 (OUT.png); print each frame's game state:
subsystem, map, MegaMan's place, flag bytes 0xBE0-0xBEF, eWeatherCompState's first 0x18 bytes, the
cutscene's script position, the screen mean, and the video's. POKES_JSON: [[g, spec], ...] still to apply."""
import os, sys, json, subprocess
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
import numpy as np
from PIL import Image
import syms
import beam
from emu import Emu
from feat import emu_thumb

state, rj = sys.argv[1], sys.argv[2]
D, G1, G2, STEP = (int(x) for x in sys.argv[3:7])
out = sys.argv[7]
pokes = json.load(open(sys.argv[8])) if len(sys.argv) > 8 and sys.argv[8] != '-' else []
SHIFT = int(sys.argv[9]) if len(sys.argv) > 9 else 0
r = json.load(open(rj)); p0 = r['press_gf']
b = beam.Beam('data/full')
vD = r['vid'][min(D - p0, len(r['vid']) - 1)]
vat = lambda g: vD + int(round((g - D) * beam.RATE))   # the video frame of game frame g's step (g -> g + 1)
COMP = syms.WEATHER_COMP_STATE
FA4 = syms.EVENT_FLAGS + (0xBE0 >> 3)
s16 = lambda v: (v >> 16) - (1 << 16) if v >> 16 >= 1 << 15 else v >> 16
src = 'data/small.mp4' if os.path.exists('data/small.mp4') else 'data/full.mp4'
flt = '' if src.endswith('small.mp4') else ',crop=906:604:37:56,scale=240:160:flags=area'


def vframes(v0, v1):
    pre = max(v0 - 30, 0)
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', src, '-vf',
                          f"select=between(n\\,{v0 - pre}\\,{v1 - pre}){flt}", '-frames:v', str(v1 - v0 + 1),
                          '-vsync', '0', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
    n = len(raw) // (160 * 240 * 3)
    a = np.frombuffer(raw[:n * 160 * 240 * 3], np.uint8).reshape(n, 160, 240, 3)
    return {v0 + i: a[i] for i in range(n)}


e = Emu(); e.loadf(state); g, _ = e.gf()
for pg, ps in pokes:
    if pg >= g: e.poke(pg, ps)
want = [x for x in range(G1, G2 + 1) if (x - G1) % STEP == 0]
V = vframes(vat(want[0] - 1 - SHIFT) - 2, vat(want[-1] - 1 - SHIFT) + 2) if want else {}
rows = []
while g < G2:
    k = b.key(vat(g + SHIFT))
    e.step(k); g += 1
    p = e.u32(syms.GS_OW_PLAYER_PTR)
    mm = (s16(e.u32(p + 0x1C)), s16(e.u32(p + 0x20))) if p else None
    shot = e.shot(); m = float(emu_thumb(shot).mean()); v = vat(g - 1 + SHIFT)
    print(f'{g}: key {k:03x} sub {e.read(syms.GS_SUBSYSTEM, 1)[0]:#x} map {bytes(e.read(syms.GS_MAP, 2)).hex()} '
          f'mm {mm} f4 {bytes(e.read(FA4, 2)).hex()} comp {bytes(e.read(COMP, 0x18)).hex()} '
          f'mean {m:.0f} video {v} {float(b.T[min(v, b.N - 1)].mean()):.0f}', flush=True)
    if g in want and v in V:
        rows.append(np.hstack([shot, np.full((160, 4, 3), 255, np.uint8), V[v]]))
if rows:
    for i in range(0, len(rows), 40):
        Image.fromarray(np.vstack(rows[i:i + 40])).save(out if i == 0 else out.replace('.png', f'_{i // 40}.png'))
