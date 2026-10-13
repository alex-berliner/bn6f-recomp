import sys, os, subprocess; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np; from PIL import Image
from sync import Sync
VID='/tmp/claude-1000/-home-alex-Code-recomp/bb066827-df96-49e7-a696-c1663c454b73/scratchpad/bsp/start.mp4'
s = Sync('data/start', -1, 1.0, log=lambda *a: None)
vA = int(np.nonzero(s.B & 1)[0][0]); s.start(600, vA); s.off = -1
s.run(985, thr=1e9)
v = s.vid[984]; em = s.e.shot().astype(np.float32).mean(2)
print('video frame', v)
raw = subprocess.run(['ffmpeg','-v','error','-i',VID,'-vf',f"select=eq(n\\,{v})",'-vsync','0','-frames:v','1','-f','rawvideo','-pix_fmt','gray','-'],capture_output=True).stdout
full = np.frombuffer(raw, np.uint8).reshape(720,1280)
img = Image.fromarray(full)
best = []
for x0 in np.arange(35.0, 40.1, 0.5):
    for y0 in np.arange(54.0, 59.1, 0.5):
        for w in (904, 905, 906, 907, 908, 909):
            h = w * 2 / 3
            c = np.asarray(img.resize((240, 160), Image.BOX, box=(x0, y0, x0 + w, y0 + h)), np.float32)
            best.append((np.abs(c - em)[20:, :].mean(), x0, y0, w))
best.sort(); print(best[:8])
