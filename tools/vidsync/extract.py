"""extract.py VIDEO OUTDIR [TMPL.npz]: one decode pass over the video, writing
  OUTDIR/thumb.u8   N x 80 x 120 gray thumbnails of the game screen (GBA pixels / 2)
  OUTDIR/scores.f4  N x 10 input-display slot scores (U D L R S s B A L R; > 110 = held)
  OUTDIR/strip.u8   N x 36 x 150 x 3 raw display strip (for re-classifying later)
The game screen is video x 37..944, y 56..660 (240x160 at ~3.78x)."""
import subprocess, sys, os, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
video, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
GX, GY, GW, GH = 37, 56, 907, 604
SW, SH = 150, 36
# one output frame: the game thumbnail (120x80, padded to 150 wide) above the display strip (150x36)
flt = (f"[0:v]split=2[a][b];[a]crop={GW}:{GH}:{GX}:{GY},scale=120:80:flags=area,pad=150:80[g];"
       f"[b]crop={SW}:{SH}:330:84[s];[g][s]vstack=inputs=2,format=rgb24[o]")
p = subprocess.Popen(['ffmpeg', '-v', 'error', '-threads', '8', '-i', video, '-filter_complex', flt, '-map', '[o]',
                      '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], stdout=subprocess.PIPE, bufsize=1 << 24)
FR = 150 * (80 + SH) * 3
ft = open(f'{out}/thumb.u8', 'wb'); fs = open(f'{out}/strip.u8', 'wb')
n = 0
while True:
    b = p.stdout.read(FR * 600)
    if not b: break
    assert len(b) % FR == 0, len(b)
    a = np.frombuffer(b, np.uint8).reshape(-1, 80 + SH, 150, 3)
    g = a[:, :80, :120].astype(np.float32)
    gray = (0.299 * g[..., 0] + 0.587 * g[..., 1] + 0.114 * g[..., 2]).round().astype(np.uint8)
    ft.write(gray.tobytes()); fs.write(np.ascontiguousarray(a[:, 80:]).tobytes())
    n += len(a)
    if n % 60000 < 600: print(n, flush=True)
print('frames', n)
