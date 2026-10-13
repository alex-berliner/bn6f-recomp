"""killview.py RESULT STATE D OUT: our screen after deleting the enemies at D (no keys) | the video around D."""
import os, sys, json, subprocess, numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/vidsync
from PIL import Image
from emu import Emu
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; D = int(sys.argv[3])
def vf(v):
    pre = max(v - 30, 0)
    b = subprocess.run(['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', 'data/full.mp4', '-vf', f"select=eq(n\\,{v - pre}),crop=906:604:37:56,scale=240:160:flags=area", '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
    return np.frombuffer(b, np.uint8).reshape(160, 240, 3)
e = Emu(); e.loadf(sys.argv[2]); g, _ = e.gf()
for p in r['pokes']:
    if p[0] >= g: e.poke(*p)
for gg in range(g, D): e.step(r['keys'][gg - g0])
act = e.u32(0x02034000); foes = []
for n in range(16):
    if not act >> (31 - n) & 1: continue
    a = 0x0203A9B0 + n * 0xD8
    if e.read(a + 0x24, 2) != b'\0\0' and e.read(a + 0x16, 1)[0] == 1: foes.append(a)
print('foes', [hex(a) for a in foes])
e.poke(D, ','.join(f'{a + 0x24:#x}=0x0/2' for a in foes))
vD = r['vid'][D - g0 - 1]; rows = []
for k in range(480):
    e.step(0x3FF)
    if k % 60 == 59:
        rows.append(np.hstack([e.shot(), np.full((160, 4, 3), 255, np.uint8), vf(vD - 300 + k * 2)]))
Image.fromarray(np.vstack(rows)).save(sys.argv[4])
