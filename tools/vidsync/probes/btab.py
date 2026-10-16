"""btab.py STATE: the current map's random-encounter lists under each of the four roots (entries of 0x10 bytes):
each entry's bytes and the first bytes behind its pointers."""
import sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.dirname(__import__('os').path.abspath(__file__))))   # tools/vidsync
from emu import Emu
import syms
e = Emu(); e.loadf(sys.argv[1])
grp, mp = e.read(syms.GS_MAP, 2)
print('map', hex(grp), hex(mp), 'battle ptr', hex(e.u32(0x02001B9C)))
rom = lambda a: 0x08000000 <= a < 0x0A000000
for k in range(4):
    r = syms.ENC_ROOTS + 8 * k
    t = e.u32(r + 4 * (grp >= 0x80))
    if not rom(t): print('root', k, 'none'); continue
    t = e.u32(t + 4 * (grp & 0x7F))
    if not rom(t): print('root', k, 'none'); continue
    q = e.u32(t + 4 * mp); n = 0
    print('root', k, hex(r), 'list', hex(q))
    while rom(q) and e.read(q, 1)[0] != 0xFF and n < 40:
        b = e.read(q, 16); p8, p12 = int.from_bytes(b[8:12], 'little'), int.from_bytes(b[12:16], 'little')
        en = e.read(p12, 16).hex() if rom(p12) else ''
        print(f'  {q:x} {b[:8].hex()} +8 {p8:x} +12 {p12:x} -> {en}')
        q += 0x10; n += 1
