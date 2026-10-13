import sys; sys.path.insert(0, __import__('os').path.dirname(__file__))
import syms   # game addresses by name from bn6f
from emu import Emu
SEED=syms.PRIMARY_RNG_SEED
START=0x3FF & ~(1<<3); A=0x3FF & ~1
e=Emu()
for i in range(12):
    g,h=e.step(n=50); print('idle', g, h, hex(e.u32(SEED)))
e.close()
# press Start at different title frames, then run 600 frames of nothing: compare seeds
for wait in (400, 401, 402, 450, 600):
    e=Emu(); e.step(n=wait); e.step(START, 3); g,h=e.step(n=300); s1=hex(e.u32(SEED)); g,h=e.step(n=600)
    print('wait', wait, 'seed after +300', s1, '+900', hex(e.u32(SEED)), g); e.close()
