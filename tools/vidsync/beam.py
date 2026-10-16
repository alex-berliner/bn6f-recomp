"""beam.py: rebuild a run's inputs from its video — a beam search over emulator paths.

The video shows BizHawk's input display (the buttons held on each emulated
hardware frame) on the same video frame as that hardware frame's picture.
The emulator here runs one game frame at a time and the game reads its keys
on the game frame's last hardware frame h, so the key for a game frame is the
display at the video frame of h:

    v(h) = V0 + R*h + o

R is BizHawk's rate under a 60 fps capture; o, the offset, is hidden: the
capture's pacing jitters by a frame, and a map load can take BizHawk's core a
few hardware frames more or less than ours. Every path keeps a cost for each
offset (Viterbi: small moves are cheap, jumps dearer, and each frame adds how
far its picture is from the video's at that offset, in units of the error a
correct match has on a picture with that much detail). Where the likely
offsets disagree on a key (near a press or release), the path splits into one
branch per key and later pictures decide; branches far behind the best are
dropped.
"""
import sys, os, json, time, pickle
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from scipy import ndimage
from feat import video, emu_thumb, MASK
from emu import Emu

BLUR = 0.7
K = 8                                  # offsets -K..K
OFFS = np.arange(-K, K + 1)
TAU = 0.8                              # offsets this close to a path's best can decide its key
PRUNE = 4.0                            # branches this far behind the best are dropped
MAXB = 8
CAP = 6.0                              # one frame adds at most this much
RATE = 60 / 59.7275
DELTA = int(os.environ.get('DELTA', '0'))   # the key is read DELTA video frames after its picture


def jump_cost(d):
    """A real slip of the capture is rare (about one per 200 frames), so moving the offset must
    cost more than a frozen picture saves: otherwise a branch whose game has stalled (a press read
    late) keeps matching one video frame by stepping the offset back every frame."""
    d = abs(d)
    return 0.0 if d == 0 else 2.0 if d == 1 else 4.0 + 0.5 * d


JUMP = np.array([[jump_cost(a - b) for b in OFFS] for a in OFFS])   # JUMP[new, old]


class Node:
    __slots__ = ('key', 'v', 'parent')

    def __init__(self, key, v, parent):
        self.key, self.v, self.parent = key, v, parent


class Branch:
    __slots__ = ('slot', 'h', 'cost', 'node', 'bad', 'mem', 'seed')

    def __init__(self, slot, h, cost, node, bad=0, mem=None, seed=None):
        self.slot, self.h, self.cost, self.node, self.bad = slot, h, cost, node, bad
        self.mem, self.seed = mem, seed


BITS_FIXES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'bits_fixes.json')


def load_bits(vdir):
    """The input display's bits (bits.npy) with bits_fixes.json's corrections when they are this video's (its
    frame count) -> (bits, the path of a .npy holding them, for emu_server). A corrected copy is written beside
    bits.npy under a name carrying its contents' hash (written once; concurrent writers rename atomically)."""
    path = os.path.abspath(f'{vdir}/bits.npy')
    B = np.load(path)
    try:
        spec = json.load(open(BITS_FIXES))
    except (OSError, ValueError):
        return B, path
    if spec.get('frames') != len(B) or not spec.get('fixes'):
        return B, path
    B = B.copy()
    for f in spec['fixes']:
        B[f['v0']:f['v1'] + 1] = (B[f['v0']:f['v1'] + 1] | f.get('set', 0)) & ~np.uint16(f.get('clear', 0))
    import hashlib
    fixed = os.path.join(os.path.dirname(path), f'bits.fixed.{hashlib.sha1(B.tobytes()).hexdigest()[:12]}.npy')
    if not os.path.exists(fixed):
        tmp = f'{fixed}.{os.getpid()}.tmp.npy'
        np.save(tmp, B); os.replace(tmp, fixed)
    return B, fixed


class Beam:
    def __init__(self, vdir, log=print):
        self.T, _ = video(vdir)
        self.B, bits_path = load_bits(vdir)
        self.N = len(self.T)
        self.log = log
        self.e = Emu()
        self.vc = {}
        self.free = list(range(100, 164))
        self.pokes = []                # [game frame, "ADDR=VALUE/SIZE,..."]: the route harness's .poke lines
        # the single-branch stretches in C (emu_server's follow); VIDSYNC_FAST=0 runs every frame here
        self.fast = (self.C_ERRS and os.environ.get('VIDSYNC_FAST', '1') != '0'
                     and self.e.cmd(f'bits {bits_path} {self.N}')[0] == 'ok')

    def add_poke(self, g, spec):
        self.pokes.append([g, spec]); self.e.poke(g, spec)

    # -- video
    def vthumb(self, v):
        r = self.vc.get(v)
        if r is None:
            V = ndimage.gaussian_filter(self.T[min(max(v, 0), self.N - 1)].astype(np.float32), BLUR)
            edge = (np.abs(np.diff(V, axis=0))[MASK[1:]].mean() + np.abs(np.diff(V, axis=1))[MASK[:, 1:]].mean()) / 2
            r = (V, 0.8 + 0.62 * float(edge), float(V[MASK].mean()))   # floor: compression alone, on a flat screen
            self.vc[v] = r
            if len(self.vc) > 30000:
                for k in list(self.vc)[:10000]:
                    del self.vc[k]
        return r

    def errs(self, E, vb):
        """Normalized error of picture E against video frames vb+OFFS. Overall brightness counts
        only a quarter: fades and blends come out a few levels apart in BizHawk's older core."""
        out = np.empty(len(OFFS))
        Em = float(E[MASK].mean())
        for i, o in enumerate(OFFS):
            V, x, Vm = self.vthumb(vb + o)
            d = Em - Vm
            out[i] = min((float(np.abs(E - V - d)[MASK].mean()) + 0.25 * abs(d)) / x, CAP)
        return out

    def vbase(self, h):
        return int(round(self.v0 + RATE * h))

    def key(self, v):
        v += DELTA
        return 0x3FF & ~int(self.B[min(max(v, 0), self.N - 1)])

    # -- emulator
    C_ERRS = bool(os.environ.get('VIDSYNC_THUMBS'))   # emu_server scores pictures itself

    def run_key(self, key):
        """Run one game frame with key; returns (hw count after, video base frame, errors at vb+OFFS)."""
        _, h1 = self.e.step(key)
        vb = self.vbase(h1 - 1)
        if self.C_ERRS:
            return h1, vb, np.array(self.e.errs(vb - K, len(OFFS)))
        E = ndimage.gaussian_filter(emu_thumb(self.e.shot()), BLUR)
        return h1, vb, self.errs(E, vb)

    def kid(self, slot, key):
        """From the machine in slot, one game frame with key, kept in a new slot: (new slot, hw count after,
        video base frame, errors at vb+OFFS, (memory hash, RNG seed)). With the pictures scored in C, one
        emu_server command (stepx)."""
        if self.C_ERRS:
            new = self.free.pop()
            r = self.e.cmd(f'stepx {slot} {key:x} {new} {float(self.v0)!r}')
            assert r[0] == 'stepx', r
            h1, vb = int(r[1]), int(r[2])
            assert vb == self.vbase(h1 - 1), (vb, h1)
            n = len(OFFS)
            return new, h1, vb, np.array([float(x) for x in r[3:3 + n]]), (r[3 + n], int(r[4 + n], 16))
        self.e.load(slot)
        h1, vb, er = self.run_key(key)
        new = self.free.pop()
        self.e.save(new)
        return new, h1, vb, er, self.e.hash()

    def start(self, press_gf):
        """Boot, PRESS START, wait on NEW GAME until game frame press_gf; the first A on the
        display is the NEW GAME press."""
        e = self.e
        e.step(n=400); e.step(0x3FF & ~8, 2)
        g, h = e.step(n=press_gf - 402)
        vA = int(np.nonzero(self.B & 1)[0][0])
        self.v0 = vA - RATE * h
        self.g = g
        slot = self.free.pop(); e.save(slot)
        cost = np.array([0.5 * abs(o) for o in OFFS])
        self.branches = [Branch(slot, h, cost, None)]

    # -- one game frame for every branch
    def advance(self):
        kids = []
        for br in self.branches:
            pred = (br.cost[None, :] + JUMP).min(1)
            vb0 = self.vbase(br.h)
            k0 = self.key(vb0 + OFFS[int(np.argmin(pred))])
            slot0, h1, vb, er0, mem0 = self.kid(br.slot, k0)
            near = pred < pred.min() + TAU
            keys = {}
            for i in np.nonzero(near)[0]:
                keys.setdefault(self.key(vb + OFFS[i]), []).append(i)
            keys.setdefault(k0, [])               # only far offsets disagree: keep the run anyway
            for k, idx in keys.items():
                if k == k0:
                    hk, vbk, er, slot, mem = h1, vb, er0, slot0, mem0
                    mask = np.ones(len(OFFS), bool)
                    for k2, idx2 in keys.items():
                        if k2 != k0:
                            mask[idx2] = False
                else:
                    slot, hk, vbk, er, mem = self.kid(br.slot, k)
                    mask = np.zeros(len(OFFS), bool); mask[idx] = True
                c = np.where(mask, pred + er, np.inf)
                bo = int(np.argmin(c))
                kids.append(Branch(slot, hk, c, Node(k, vbk + int(OFFS[bo]), br.node), mem=mem[0], seed=mem[1]))
            self.free.append(br.slot)
        kids = self.merge(kids)
        best = min(float(k.cost.min()) for k in kids)
        kids.sort(key=lambda k: float(k.cost.min()))
        alive = [k for k in kids if float(k.cost.min()) <= best + PRUNE]
        # keep the best branch of every RNG state first: branches that look alike can differ
        # in how far the RNG has run, and only a later random event (an encounter, a draw)
        # shows which is right
        keep, seen = [], set()
        for k in alive:
            if k.seed not in seen and len(keep) < MAXB:
                keep.append(k); seen.add(k.seed)
        for k in alive:
            if k not in keep and len(keep) < MAXB:
                keep.append(k)
        keep.sort(key=lambda k: float(k.cost.min()))
        for k in kids:
            if k not in keep:
                self.free.append(k.slot)
        for k in keep:
            k.cost = k.cost - best
        # keep the window centred on the best path's offset: loads can leave BizHawk's core
        # several hardware frames ahead of or behind ours, and that adds up
        o = int(OFFS[int(np.argmin(keep[0].cost))])
        if o:
            self.v0 += o
            for k in keep:
                c = np.full(len(OFFS), float(k.cost.max()) + 3.0)
                if o > 0:
                    c[:-o] = k.cost[o:]
                else:
                    c[-o:] = k.cost[:o]
                k.cost = c
        self.branches = keep
        self.g += 1
        return best

    def follow(self, n, stop_cost=np.inf, vend=-1, hist=(), stall=(0, 0)):
        """advance() for the one branch, up to n game frames in emu_server (the same arithmetic, in doubles, and
        the machine loaded and saved around every frame as advance() has it), while it stays one branch. Stops
        before a frame where it would split, or after a frame whose best cost is above stop_cost, whose video
        frame is >= vend (if >= 0), or (stall = (W, D), W > 0) whose video frame is less than D past the one
        W - 1 frames before (hist: the video frames before this call).
        -> (why, [(game frame, best, video frame)]) with why one of n, split, cost, end, stall."""
        br = self.branches[0]
        hist = list(hist)
        hist = hist[len(hist) - stall[0] + 1:] if stall[0] > 1 else []
        r = self.e.cmd(' '.join(['follow', str(br.slot), str(n), repr(float(self.v0)), str(br.h), str(DELTA),
                                 repr(TAU), repr(float(stop_cost)), str(vend), str(stall[0]), str(stall[1]),
                                 str(len(hist))] + [str(v) for v in hist] + [repr(float(x)) for x in br.cost]))
        assert r[0] == 'follow', r
        m, why = int(r[1]), r[2]
        if not m:
            return why, []
        self.v0, br.h = float(r[3]), int(r[4])
        br.cost = np.array([float(x) for x in r[5:5 + len(OFFS)]])
        recs, node, i = [], br.node, 5 + len(OFFS)
        for _ in range(m):
            k, v, best = int(r[i]), int(r[i + 1]), float(r[i + 2]); i += 3
            node = Node(k, v, node)
            self.g += 1
            recs.append((self.g, best, v))
        br.node, br.mem, br.seed = node, None, None   # br.slot now holds the machine after the last frame
        return why, recs

    def run(self, n, stop_cost=np.inf, vend=-1, hist=(), stall=(0, 0)):
        """Up to n game frames: follow() while there is one branch, advance() otherwise. Stops early after a
        frame as follow() does (best cost above stop_cost, the video at vend, a stall), so a caller's rules on
        those see the machine as it is after the frame.
        -> [(game frame, best cost, branches, the best path's video frame)] per frame."""
        out = []
        hist = list(hist)
        while len(out) < n:
            if self.fast and len(self.branches) == 1:
                why, recs = self.follow(n - len(out), stop_cost, vend, hist, stall)
                out += [(g, best, 1, v) for g, best, v in recs]
                hist += [v for g, best, v in recs]
                if why != 'split':
                    if why != 'n' or len(out) >= n:
                        break
                    continue
                if len(out) >= n:
                    break
            best = self.advance()
            v = self.branches[0].node.v
            out.append((self.g, best, len(self.branches), v))
            hist.append(v)
            if best > stop_cost or 0 <= vend <= v or \
                    (stall[0] > 0 and len(hist) >= stall[0] and v - hist[-stall[0]] < stall[1]):
                break
        return out

    def merge(self, kids):
        """Children whose game is the same (memory and hardware frame) are one branch: keep the
        better history and the best cost for every offset."""
        by = {}
        for k in kids:
            key = (k.mem, k.h)
            o = by.get(key)
            if o is None:
                by[key] = k
                continue
            win, lose = (k, o) if float(k.cost.min()) < float(o.cost.min()) else (o, k)
            win.cost = np.minimum(win.cost, lose.cost)
            self.free.append(lose.slot)
            by[key] = win
        return list(by.values())

    def keys_of(self, br):
        out = []
        n = br.node
        while n is not None:
            out.append((n.key, n.v)); n = n.parent
        return out[::-1]


def checkpoint(b, path, per):
    """The best branch's machine and its keys so far, to resume from."""
    br = b.branches[0]
    b.e.load(br.slot); b.e.savef(path + '.state')
    ks = b.keys_of(br)
    json.dump({'press_gf': b.press_gf, 'g': b.g, 'h': br.h, 'v0': b.v0, 'cost': br.cost.tolist(),
               'keys': [k for k, v in ks], 'vid': [v for k, v in ks], 'frame_cost': per, 'pokes': b.pokes},
              open(path + '.json', 'w'))


def resume(b, path):
    c = json.load(open(path + '.json'))
    b.press_gf, b.g, b.v0 = c['press_gf'], c['g'], c['v0']
    slot = b.free.pop()
    b.e.loadf(path + '.state'); b.e.save(slot)
    node = None
    for k, v in zip(c['keys'], c['vid']):
        node = Node(k, v, node)
    b.branches = [Branch(slot, c['h'], np.array(c['cost']), node)]
    for g, spec in c.get('pokes', []):
        b.add_poke(g, spec)
    return c['frame_cost']


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('vdir'); ap.add_argument('press_gf', type=int); ap.add_argument('until', type=int)
    ap.add_argument('--resume'); ap.add_argument('--every', type=int, default=5000)
    ap.add_argument('--out', default=None)
    a = ap.parse_args()
    vdir = a.vdir
    out = a.out or f'{vdir}/beam{DELTA:+d}'
    os.makedirs(out + '.ckpt', exist_ok=True)
    b = Beam(vdir)
    if a.resume:
        per = resume(b, a.resume)
        print(f'resumed at game {b.g} (video {b.branches[0].node.v})', flush=True)
    else:
        b.start(a.press_gf); b.press_gf = a.press_gf; per = []
    t = time.time(); g_start = b.g
    nb = []
    bad = 0
    last_v = []           # (game, video) of the best path, to see the video stop advancing
    status = 'end of video'
    done = False
    while not done and b.g < a.until:
        # a stretch of frames at once; it ends where a frame below may print, checkpoint or stop the sync
        # (b.run stops after a frame above the desync cost, a stall or the video's end)
        stop = min(a.until, (b.g // 500 + 1) * 500, (b.g // a.every + 1) * a.every)
        for g, best, nbr, v in b.run(stop - b.g, 2.0, b.N - K - 2, last_v, (60, 3)):
            per.append(best)
            nb.append(nbr)
            bad = bad + 1 if best > 2.0 else 0
            last_v.append(v); last_v = last_v[-60:]
            if g % 500 == 0:
                print(f'game {g} video {v} branches {nbr} frame cost {best:.2f} '
                      f'{(g - g_start) / (time.time() - t):.0f} fps', flush=True)
            if g % a.every == 0:
                checkpoint(b, f'{out}.ckpt/{g:07d}', per)
            if bad >= 30:
                status = f'desync near game frame {g - 30} (video {b.keys_of(b.branches[0])[-30][1]})'
                done = True; break
            if len(last_v) == 60 and last_v[-1] - last_v[0] < 3:
                status = f'stalled: video stays at {last_v[0]} from game frame {g - 60} (a missed press?)'
                done = True; break
            if v >= b.N - K - 2:
                done = True; break
    if not done:
        status = 'reached the requested game frame'
    print(status, flush=True)
    ks = b.keys_of(b.branches[0])
    perr = np.array(per)
    print('mean branches', round(float(np.mean(nb)), 2), 'frames', len(ks), 'frame cost percentiles 50/90/99/max',
          np.percentile(perr, [50, 90, 99, 100]).round(2))
    json.dump({'press_gf': b.press_gf, 'status': status, 'keys': [k for k, v in ks], 'vid': [v for k, v in ks],
               'cost': per, 'pokes': b.pokes}, open(f'{out}.json', 'w'))


if __name__ == '__main__':
    main()
