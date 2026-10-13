"""emu.py: drive build-tools/emu_server (make -C tools emu_server; one game frame per step) from Python.
Environment: VIDSYNC_ROM, VIDSYNC_BIOS, VIDSYNC_EMU (defaults: tools/paths.env's ORIG_ROM, GBA_BIOS, EMU_SERVER)."""
import subprocess, numpy as np, os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/
import gamepaths  # tools/gamepaths.py: the game profile (tools/paths.env)
ROOT=gamepaths.ROOT
ROM=os.environ.get('VIDSYNC_ROM', gamepaths.ORIG_ROM)
BIOS=os.environ.get('VIDSYNC_BIOS', gamepaths.GBA_BIOS)
class Emu:
    def __init__(self, save=None):
        args=[os.environ.get('VIDSYNC_EMU', gamepaths.EMU_SERVER), ROM, BIOS]+([save] if save else [])
        self.p=subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    def cmd(self, s):
        self.p.stdin.write((s+'\n').encode()); self.p.stdin.flush()
        return self.p.stdout.readline().decode().split()
    def step(self, key=0x3FF, n=1):
        r=self.cmd(f'step {key:x} {n}'); return int(r[1]), int(r[2])
    def shot(self):
        r=self.cmd('shot'); w,h=int(r[1]),int(r[2])
        b=self.p.stdout.read(w*h*3); return np.frombuffer(b,np.uint8).reshape(h,w,3)
    def save(self, slot): self.cmd(f'save {slot}')
    def load(self, slot): self.cmd(f'load {slot}')
    def savef(self, path): assert self.cmd(f'savef {path}')[0] == 'ok'
    def loadf(self, path): assert self.cmd(f'loadf {path}')[0] == 'ok'
    def errs(self, v0, n):
        return [float(x) for x in self.cmd(f'errs {v0} {n}')[1:]]
    def hash(self):
        r = self.cmd('hash'); return r[1], int(r[2], 16)
    def digest(self):
        """The route harness's state digest: {'game': G, 'hw': H, 'ewram': hex, 'pal': .., 'vram': .., 'oam': ..}."""
        r = self.cmd('digest'); return {k: (int(v) if k in ('game', 'hw') else v) for k, v in (t.split('=') for t in r[1:])}
    def read(self, addr, n): return bytes.fromhex(self.cmd(f'read {addr:x} {n}')[1])
    def u32(self, addr): return int.from_bytes(self.read(addr,4),'little')
    def poke(self, g, spec): assert self.cmd(f'poke {g} {spec}')[0] == 'ok'
    def pokes_clear(self): self.cmd('pokes')
    def gf(self): r=self.cmd('gf'); return int(r[1]), int(r[2])
    def close(self):
        self.p.stdin.write(b'quit\n'); self.p.stdin.close(); self.p.wait()
