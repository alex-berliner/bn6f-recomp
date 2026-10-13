import sys, os, subprocess; sys.path.insert(0, os.path.dirname(__file__))
import numpy as np; from PIL import Image
from emu import Emu
V='/tmp/claude-1000/-home-alex-Code-recomp/bb066827-df96-49e7-a696-c1663c454b73/scratchpad/bsp/start.mp4'
raw=subprocess.run(['ffmpeg','-v','error','-ss','1.5','-i',V,'-frames:v','1','-vf','crop=907:604:37:56,scale=240:160:flags=area','-f','rawvideo','-pix_fmt','rgb24','-'],capture_output=True).stdout
vid=np.frombuffer(raw,np.uint8).reshape(160,240,3)
e=Emu(); e.step(n=400); e.step(0x3FF&~8, 2); e.step(n=60); em=e.shot()
Image.fromarray(np.hstack([vid,em])).resize((960,320),Image.NEAREST).save('data/start/title_rgb.png')
for (y,x) in [(20,20),(80,10),(150,120),(60,120),(100,200)]:
    print((y,x),'video',vid[y,x],'emu',em[y,x])
