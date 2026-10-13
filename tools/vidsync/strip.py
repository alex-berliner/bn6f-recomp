# strip.py VIDEO OUT.npy: the input display strip (x 330..480, y 84..120) of every frame
import subprocess, sys, numpy as np
X0,X1,Y0,Y1=330,480,84,120
p=subprocess.Popen(['ffmpeg','-v','error','-i',sys.argv[1],'-vf',f'crop={X1-X0}:{Y1-Y0}:{X0}:{Y0}','-f','rawvideo','-pix_fmt','rgb24','-'],stdout=subprocess.PIPE)
n=(X1-X0)*(Y1-Y0)*3; fs=[]
while True:
    b=p.stdout.read(n)
    if len(b)<n: assert not b, len(b); break
    fs.append(np.frombuffer(b,np.uint8))
a=np.array(fs).reshape(-1,Y1-Y0,X1-X0,3); np.save(sys.argv[2], a); print(a.shape)
