"""extract_thumb.py VIDEO OUTDIR: extract.py's game thumbnails only (OUTDIR/thumb.u8, N x 80 x 120 gray), for a
machine that takes the input bits from elsewhere. The filter graph is extract.py's, strip and all (ffmpeg then
scales in YUV and converts after, as there; scaling alone it would convert first and come out a few levels
different), and the strip is dropped."""
import subprocess, sys, os, numpy as np
video, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
GX, GY, GW, GH = 37, 56, 907, 604
SW, SH = 150, 36
flt = (f"[0:v]split=2[a][b];[a]crop={GW}:{GH}:{GX}:{GY},scale=120:80:flags=area,pad=150:80[g];"
       f"[b]crop={SW}:{SH}:330:84[s];[g][s]vstack=inputs=2,format=rgb24[o]")
p = subprocess.Popen(['ffmpeg', '-v', 'error', '-threads', '8', '-i', video, '-filter_complex', flt, '-map', '[o]',
                      '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], stdout=subprocess.PIPE, bufsize=1 << 24)
FR = 150 * (80 + SH) * 3
ft = open(f'{out}/thumb.u8', 'wb'); n = 0
while True:
    b = p.stdout.read(FR * 600)
    if not b: break
    a = np.frombuffer(b, np.uint8).reshape(-1, 80 + SH, 150, 3)
    g = a[:, :80, :120].astype(np.float32)
    ft.write((0.299 * g[..., 0] + 0.587 * g[..., 1] + 0.114 * g[..., 2]).round().astype(np.uint8).tobytes())
    n += len(a)
    if n % 60000 < 600: print(n, flush=True)
print('frames', n)
