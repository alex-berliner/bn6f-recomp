"""sync.py: rebuild a run's inputs from its video by replaying them in lockstep with the original ROM.

The video shows BizHawk's input display: the buttons held on each emulated
(hardware) frame. The emulator here runs one game frame at a time; the key
for game frame g is the display's value at the video frame of g's first
hardware frame (plus DELTA). A video frame is found from the hardware frame
count:  v(h) = V0 + R*h + offset, where the offset (duplicated or dropped
video frames) is tracked by matching the emulator's screen against the video.
When the screens stop matching, the input edges just before are moved a
frame or two at a time until they match again.
"""
import sys, os, json, time
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from collections import deque
from scipy import ndimage
from feat import video, emu_thumb, MASK

BLUR = 0.7             # both images are blurred a little: the video is scaled and compressed
from emu import Emu

RING = 1024            # save states kept (one per game frame)
OFFS = range(-3, 4)    # offsets tried around the tracked one


class Sync:
    def __init__(self, vdir, delta=1, rate=1.0, log=print):
        self.T, _ = video(vdir)
        self.B = np.load(f'{vdir}/bits.npy')
        self.delta, self.rate, self.log = delta, rate, log
        self.e = Emu()
        self.keys = {}         # game frame -> keyinput (active low)
        self.hw = {}           # game frame -> hardware frame count before it
        self.err = {}          # game frame -> match error of the screen after it
        self.vid = {}          # game frame -> matched video frame
        self.thumb = {}        # game frame -> emulator thumbnail after it (recent only)
        self.off = 0
        self.v0 = 0.0
        self.vcache = {}
        self.recent = deque(maxlen=60)

    # -- video lookups
    def vmap(self, h):
        return int(round(self.v0 + self.rate * h)) + self.off

    def vthumb(self, v):
        t = self.vcache.get(v)
        if t is None:
            t = ndimage.gaussian_filter(self.T[v].astype(np.float32), BLUR)
            self.vcache[v] = t
            if len(self.vcache) > 20000:
                for k in list(self.vcache)[:5000]:
                    del self.vcache[k]
        return t

    def frame_err(self, E, v):
        if v < 0 or v >= len(self.T):
            return 1e9
        return float(np.abs(E - self.vthumb(v))[MASK].mean())

    def expected(self, v):
        """Error of a correct match: grows with the frame's detail (edges blur differently in the video)."""
        V = self.vthumb(v)
        edge = (np.abs(np.diff(V, axis=0))[MASK[1:]].mean() + np.abs(np.diff(V, axis=1))[MASK[:, 1:]].mean()) / 2
        return 0.15 + 0.62 * float(edge)

    def limit(self, v):
        """Error above which a frame counts as not matching."""
        x = self.expected(v)
        return 1.8 * x + 1.5

    def key_at(self, h, shift=0):
        v = self.vmap(h) + self.delta + shift
        return 0x3FF & ~int(self.B[min(max(v, 0), len(self.B) - 1)])

    # -- emulator
    def local(self, E, v):
        """(error, offset) of the best video frame within one of v: the capture's pacing jitters
        (a frame shown twice, then one skipped), so a frame's picture can sit one either side.
        Ties (still pictures) keep the tracked offset."""
        es = [(self.frame_err(E, v + o), abs(o), o) for o in (0, -1, 1)]
        e0 = es[0][0]
        err, _, o = min(es)
        if e0 - err < max(0.3 * self.expected(v), 0.3):
            return e0, 0
        return err, o

    def step(self, g, key, fixed=False):
        """Run game frame g; returns (thumbnail, hw count after, key used, video frame, error).
        The game reads the keys on its last hardware frame, and that frame's input is on the
        same video frame as its picture: unless the key is fixed, run once, find the video frame
        showing the result, and if its input differs, run again from before with that key."""
        if not fixed:
            self.e.save(RING + 1)
        for it in range(3):
            _, h1 = self.e.step(key)
            E = ndimage.gaussian_filter(emu_thumb(self.e.shot()), BLUR)
            v = self.vmap(h1 - 1)
            err, o = self.local(E, v)
            if fixed:
                break
            k2 = 0x3FF & ~int(self.B[min(v + o + self.delta, len(self.B) - 1)])
            if k2 == key:
                break
            self.e.load(RING + 1); key = k2
        return E, h1, key, v + o, err, o

    def state_slot(self, g):
        return g % RING

    def match(self, E, h_last):
        """Best (offset, error) of a screen against the video around v(h_last)."""
        es = [(self.frame_err(E, self.vmap(h_last) + o), o) for o in OFFS]
        return min(es)[1], min(es)[0], es

    # -- main loop
    def start(self, press_gf, v_press):
        """Boot, PRESS START, wait on NEW GAME until game frame press_gf, which is video frame v_press."""
        e = self.e
        e.step(n=400); e.step(0x3FF & ~8, 2)
        g, h = e.step(n=press_gf - 402)
        self.g, self.h = g, h
        self.v0 = v_press - self.rate * h

    def run(self, until_g, thr=None):
        bad = 0
        while self.g < until_g:
            g = self.g
            self.e.save(self.state_slot(g)); self.hw[g] = self.h
            key = self.keys.get(g)
            fixed = key is not None
            if key is None:
                key = self.key_at(self.h)
            E, h1, key, v, err, o = self.step(g, key, fixed=fixed)
            self.keys[g] = key
            lim = thr if thr is not None else self.limit(v)
            self.track_offset(g, o)
            self.err[g] = err; self.vid[g] = v; self.thumb[g] = E
            self.thumb.pop(g - RING, None)
            self.g, self.h = g + 1, h1
            bad = bad + 1 if err > lim else 0
            if g % 500 == 0:
                self.log(f'game {g} hw {h1} video {v} err {err:.2f} lim {lim:.2f} off {self.off}')
            if bad >= 6:
                if not self.repair(g - 5, lim):
                    self.log(f'stuck at game frame {g - 5} (video {self.vid[g - 5]}, err {self.err[g - 5]:.1f})')
                    return False
                bad = 0
        return True

    def track_offset(self, g, o):
        """The offset itself moves when most of the last 30 frames that showed a clear
        neighbour preferred the same one (at least 10 of them)."""
        self.votes = getattr(self, 'votes', deque(maxlen=30))
        self.votes.append(o)
        n = {q: sum(1 for x in self.votes if x == q) for q in (-1, 1)}
        for q in (-1, 1):
            if n[q] >= 10 and n[q] >= 0.7 * len(self.votes):
                self.off += q; self.votes.clear()
                self.log(f'offset {self.off:+d} at game frame {g}')

    # -- repair: move input edges before the divergence
    def replay_from(self, g0, g1, keys, off0, score_from=None):
        """Load the state before g0 and run to g1 with keys (dict; else from the video); return
        (score of the last 40 frames in units of a correct match's error, per-frame results, hw)."""
        self.e.load(self.state_slot(g0))
        h = self.hw[g0]; saved_off = self.off; self.off = off0
        errs = []; out = {}
        for g in range(g0, g1):
            k = keys.get(g)
            fixed = k is not None
            if k is None:
                k = self.key_at(h)
            E, h1, k, v, err, o = self.step(g, k, fixed=fixed)
            errs.append(err / self.expected(v)); out[g] = (k, h, E, v, err, o)
            h = h1
        self.off = saved_off
        sf = (score_from - g0) if score_from is not None else -40
        return float(np.mean(errs[sf:])), out, h

    def repair(self, gd, thr, window=90, look=50):
        """Divergence first seen at gd. Candidates, each replayed and scored over [gd-6, gd+look]
        in units of a correct match's error:
          - an offset jump of 1..8 frames from just before gd (map loads can take BizHawk's core a
            different number of hardware frames; the keys from there on are read again);
          - one input edge in [gd-window, gd] moved by 1 or 2 frames."""
        self.tries = getattr(self, 'tries', {})
        self.tries[gd] = self.tries.get(gd, 0) + 1
        if self.tries[gd] > 2:
            return False
        g0 = max(min(self.hw), gd - window, self.g - RING + 2)
        base_keys = {g: self.keys[g] for g in range(g0, self.g)}
        off0 = self.off
        cands = []
        js = max(g0, gd - 2)
        for d in (-1, 1, -2, 2, -3, 3, -4, 4, -5, 5, -6, 6, -7, 7, -8, 8):
            ks = {g: k for g, k in base_keys.items() if g < js}
            cands.append(('offset', js, d, ks, off0 + d))
        edges = [g for g in range(g0 + 1, gd + 1) if self.keys[g] != self.keys[g - 1]][-8:]
        for eg in reversed(edges):
            for sh in (-1, 1, -2, 2):
                ks = {g: k for g, k in base_keys.items() if g < gd - 1}
                if sh < 0:
                    for g in range(eg + sh, eg):
                        if g >= g0: ks[g] = base_keys[eg]
                else:
                    for g in range(eg, eg + sh):
                        ks[g] = base_keys[eg - 1]
                cands.append(('edge', eg, sh, ks, off0))
        self.log(f'diverged at game frame {gd} (video {self.vid[gd]}, err {self.err[gd]:.1f}); {len(cands)} candidates')
        best = None
        for kind, at, d, ks, off in cands:
            start = min(at, js) if kind == 'offset' else g0
            start = max(start, g0)
            # frames from start on that ks does not fix are read from the video again
            score, out, h = self.replay_from(start, gd + look, {g: k for g, k in ks.items() if g >= start}, off,
                                             score_from=gd - 6)
            if best is None or score < best[0]:
                best = (score, kind, at, d, out, off, start)
            if score < 1.3:
                break
        score, kind, at, d, out, off, start = best
        if score > 1.8:
            return False
        self.log(f'repair: {kind} {"jump" if kind == "offset" else "edge at game frame " + str(at) + " moved"} {d:+d} (score {score:.2f})')
        self.e.load(self.state_slot(start))
        for g in range(start, self.g + 1):
            self.keys.pop(g, None)
        for g in range(start, gd + look):
            self.keys[g] = out[g][0]
        self.off = off
        self.g, self.h = start, self.hw[start]
        return True


if __name__ == '__main__':
    vdir, press_gf, until = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    delta = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    rate = float(sys.argv[5]) if len(sys.argv) > 5 else 1.0
    s = Sync(vdir, delta, rate)
    vA = int(np.nonzero(s.B & 1)[0][0])
    s.start(press_gf, vA)
    t = time.time(); ok = s.run(until)
    gs = sorted(s.err)
    e = np.array([s.err[g] for g in gs])
    print('ok' if ok else 'stopped', 'game frames', gs[0], '..', gs[-1], 'video', s.vid[gs[-1]], f'{time.time() - t:.0f}s')
    print('err percentiles 50/90/99/max', np.percentile(e, [50, 90, 99, 100]).round(2))
    for g in gs[::max(1, len(gs) // 40)]:
        print(g, s.hw[g], s.vid[g], round(s.err[g], 2), hex(s.keys[g]))
    json.dump({'keys': {g: s.keys[g] for g in gs}, 'vid': {g: s.vid[g] for g in gs}}, open(f'{vdir}/sync.json', 'w'))
