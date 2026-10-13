"""feat.py: shared helpers — the video feature arrays and emulator-frame thumbnails in the same space."""
import numpy as np, os
class Slice:
    """Thumbnails of video frames base.. (a slice of thumb.u8), indexed by video frame."""
    def __init__(self, t, base): self.t, self.base = t, base
    def __len__(self): return self.base + len(self.t)
    def __getitem__(self, v): return self.t[min(max(v - self.base, 0), len(self.t) - 1)]

def video(dirpath):
    t = np.memmap(f'{dirpath}/thumb.u8', np.uint8, 'r').reshape(-1, 80, 120)
    if os.path.exists(f'{dirpath}/thumb.base'):
        t = Slice(t, int(open(f'{dirpath}/thumb.base').read()))
        s = None
        if os.path.exists(f'{dirpath}/strip.u8'):
            s = np.memmap(f'{dirpath}/strip.u8', np.uint8, 'r').reshape(-1, 36, 150, 3)
        return t, s
    s = None                     # a machine with only the thumbnails (extract_thumb.py) and the bits
    if os.path.exists(f'{dirpath}/strip.u8'):
        s = np.memmap(f'{dirpath}/strip.u8', np.uint8, 'r').reshape(-1, 36, 150, 3)
    return t, s
def emu_thumb(rgb):
    g = rgb.astype(np.float32)
    gray = 0.299 * g[..., 0] + 0.587 * g[..., 1] + 0.114 * g[..., 2]
    return gray.reshape(80, 2, 120, 2).mean((1, 3))
# the input display covers video y 91..111, x 335..470 -> thumbnail rows 4..8, cols 39..58
MASK = np.ones((80, 120), bool); MASK[3:9, 38:60] = False
# the outermost pixel takes in the overlay's frame around the game screen
MASK[0, :] = MASK[-1, :] = MASK[:, 0] = MASK[:, -1] = False
