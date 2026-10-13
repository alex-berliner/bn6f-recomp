"""vidstrip.py V0 V1 STEP OUT: video frames V0..V1 (240x160), 4 per row."""
import sys, subprocess, numpy as np
from PIL import Image
def vf(v):
    pre = max(v - 30, 0)
    b = subprocess.run(['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', 'data/full.mp4', '-vf', f"select=eq(n\\,{v - pre}),crop=906:604:37:56,scale=240:160:flags=area", '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
    return np.frombuffer(b, np.uint8).reshape(160, 240, 3)
a, z, st = map(int, sys.argv[1:4]); fr = [vf(v) for v in range(a, z, st)]
while len(fr) % 4: fr.append(np.zeros_like(fr[0]))
Image.fromarray(np.vstack([np.hstack(fr[i:i + 4]) for i in range(0, len(fr), 4)])).save(sys.argv[4])
