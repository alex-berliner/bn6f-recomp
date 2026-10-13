"""rngfix.py: trying RNG writes against the video. A candidate is a checkpoint W of the sync (the best path's
machine at a game frame, its offset costs and video mapping) plus memory writes; it scores the game frame the
sync then reaches before it loses the video (or UNTIL)."""
import os, sys, json, base64
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import beam as bm
import syms   # game addresses by name from bn6f

PRIMARY, SECONDARY = syms.PRIMARY_RNG_SEED, syms.SECONDARY_RNG_SEED
# a frame worse than this counts toward losing the video: real losses score 4-6, while a static, very
# detailed screen that matches can sit at 2.3 (resampling error)
LOSS = 2.8      # frame cost above which the sync has lost the video (bright detailed areas match at <= 2.74; a battle gone another way spikes to 3.2+)
C = 0x873CA9E5

def rng_step(x): return (((((x << 1) | (x >> 31)) & 0xffffffff) + 1) & 0xffffffff) ^ C
def rng_back(x):
    y = ((x ^ C) - 1) & 0xffffffff
    return ((y >> 1) | (y << 31)) & 0xffffffff
def rng_move(x, j):
    for _ in range(abs(j)): x = rng_step(x) if j > 0 else rng_back(x)
    return x
def spec(addr, val, size=4):
    if addr == PRIMARY:
        # a battle's intro snapshots the primary seed (sub_800B144 -> the link packet, BATTLE_SEED_COPY_LINK, looped
        # back to BATTLE_SEED_COPY) and restores it ~62 frames later (battle_copyStructsIncludingBattleStats_800b2d8), even
        # offline: a write in that window must go to the copies too (they are unused outside a battle intro)
        return f'0x{addr:x}=0x{val:x}/4,0x{syms.BATTLE_SEED_COPY_LINK:x}=0x{val:x}/4,0x{syms.BATTLE_SEED_COPY:x}=0x{val:x}/4'
    return f'0x{addr:x}=0x{val:x}/{size}'

def run_task(b, ck, extra, until, bad_max=30):
    """From checkpoint ck (meta dict with 'state' a file path) with ck's pokes and extra ones: [the game frame
    reached before the sync's frame cost stays above 2 for bad_max frames, or until; the mean frame cost]."""
    for br in getattr(b, 'branches', []): b.free.append(br.slot)
    b.e.pokes_clear(); b.pokes = []
    for g, s in ck['pokes'] + extra: b.add_poke(g, s)
    slot = b.free.pop(); b.e.loadf(ck['state']); b.e.save(slot)
    b.g, b.v0 = ck['g'], ck['v0']
    b.branches = [bm.Branch(slot, ck['h'], np.array(ck['cost']), bm.Node(ck['key'], ck['v'], None))]
    bad = 0; tot = 0.0; n = 0
    loss = ck.get('loss', LOSS)
    b.e.cmd('mask ' + ' '.join(str(x) for x in ck.get('mask', [])))   # (a stream overlay's area left out)
    while b.g < until:
        for g, best, _, v in b.run(until - b.g, loss, b.N - bm.K - 2):   # stops after a frame above the loss cost
            tot += best; n += 1
            bad = bad + 1 if best > loss else 0
            if bad >= bad_max: return [g - bad_max, tot / n]
            if v >= b.N - bm.K - 2: return [until, tot / n]   # the video ends
    return [b.g, tot / max(n, 1)]

# ---- a pool of sync workers on this machine
_B = None
def _init(vdir):
    global _B
    _B = bm.Beam(vdir, log=lambda *a: None)
def _task(t):
    ck, extra, until, tid = t
    return tid, run_task(_B, ck, extra, until)

# ---- the quick pass: the known path's keys from W (no sync), then the picture against the video only at a
# few frames near the loss. A write that fixes the event makes those frames match where the path lost them.
def quick_score(e, ck, extra, keys, vids, at, span=3, cap=None):
    """keys[i]/vids[i]: the path's key and video frame for game frame ck['g'] + i; at: indices to compare.
    -> summed best error over offsets -span..span at those frames, each at most cap (lower is better)."""
    e.pokes_clear(); e.cmd('mask')
    for g, s in ck['pokes'] + extra: e.poke(g, s)
    e.loadf(ck['state'])
    tot, want = 0.0, set(at)
    i = 0
    while i < len(keys):
        if i in want:
            e.step(keys[i]); x = min(e.errs(vids[i] - span, 2 * span + 1)); i += 1
            tot += x if cap is None else min(x, cap)
            continue
        j = i
        while j < len(keys) and keys[j] == keys[i] and j not in want: j += 1
        e.step(keys[i], j - i); i = j
    return tot

_E = None
def _qtask(t):
    global _E
    if _E is None:
        from emu import Emu
        _E = Emu()
    ck, extra, keys, vids, at, tid = t[:6]
    return tid, quick_score(_E, ck, extra, keys, vids, at, **(t[6] if len(t) > 6 else {}))
