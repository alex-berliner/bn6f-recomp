"""afterstate.py STATE N [STEP]: from STATE, step N frames with no keys and print every STEP frames what the
game is doing: subsystem, map, the event flag bytes 0xBE0-0xBEF, the first 0x18 bytes of eWeatherCompState,
the first 0x10 of eCutsceneState, MegaMan's place and the live tornado map objects (slot: state, x, y).
(Addresses resolved on the main machine; the PCs have no bn6f.)"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import syms
from emu import Emu

COMP, CUT = 0x2000310, 0x2011c50
FA4, FA5 = syms.EVENT_FLAGS + (0xBE0 >> 3), syms.EVENT_FLAGS + (0xBE8 >> 3)
objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
s16 = lambda v: (v >> 16) - (1 << 16) if v >> 16 >= 1 << 15 else v >> 16
N, STEP = int(sys.argv[2]), int(sys.argv[3]) if len(sys.argv) > 3 else 15
e = Emu(); e.loadf(sys.argv[1])
g, _ = e.gf()
for k in range(N + 1):
    if k % STEP == 0:
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        mm = (s16(e.u32(p + 0x1C)), s16(e.u32(p + 0x20))) if p else None
        buf = bytes(e.read(objs, osz * 56))
        tor = [(i, buf[i * osz + 8], s16(int.from_bytes(buf[i * osz + 0xC:i * osz + 0x10], 'little')),
                s16(int.from_bytes(buf[i * osz + 0x10:i * osz + 0x14], 'little')))
               for i in range(56) if buf[i * osz] & 1 and buf[i * osz + 1] == 0x1A]
        print(f'{g}: sub {e.read(syms.GS_SUBSYSTEM, 1)[0]:#x} map {bytes(e.read(syms.GS_MAP, 2)).hex()} '
              f'flags BE0-7 {e.read(FA4, 1)[0]:#04x} BE8-F {e.read(FA5, 1)[0]:#04x} '
              f'comp {bytes(e.read(COMP, 0x18)).hex()} cut {bytes(e.read(CUT, 0x10)).hex()} mm {mm} tor {tor}',
              flush=True)
    e.step(0x3FF); g += 1
