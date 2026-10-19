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
    reached before the sync's frame cost stays above the loss cost for bad_max frames or its video frame stalls
    (moves under 3 frames in 60: the driver's loss too), or until; the mean frame cost]."""
    for br in getattr(b, 'branches', []): b.free.append(br.slot)
    b.e.pokes_clear(); b.pokes = []
    for g, s in ck['pokes'] + extra: b.add_poke(g, s)
    slot = b.free.pop(); b.e.loadf(ck['state']); b.e.save(slot)
    b.g, b.v0 = ck['g'], ck['v0']
    b.branches = [bm.Branch(slot, ck['h'], np.array(ck['cost']), bm.Node(ck['key'], ck['v'], None))]
    bad = 0; tot = 0.0; n = 0; last_v = []
    strict = [tuple(x) for x in ck.get('strict', [])]
    b.e.cmd('mask ' + ' '.join(str(x) for x in ck.get('mask', [])))   # (a stream overlay's area left out)
    while b.g < until:
        # (in stretches that end at a strict window's edge: the loss cost is one per stretch)
        stop = min([until] + [x for a, z, _ in strict for x in (a, z) if x > b.g])
        loss = loss_at(b.g, strict, ck.get('loss', LOSS))
        for g, best, _, v in b.run(stop - b.g, loss, b.N - bm.K - 2):   # stops after a frame above the loss cost
            tot += best; n += 1
            bad = bad + 1 if best > loss else 0
            if bad >= bad_max: return [g - bad_max, tot / n]
            # (without this a write the driver loses at once as stalled held here: run8l took the same write at
            # 599859 twenty times)
            last_v.append(v); del last_v[:-60]
            if len(last_v) == 60 and last_v[-1] - last_v[0] < 3: return [g - 60, tot / n]
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
    ck, extra, keys, vids, at, tid = t[:6]
    return tid, quick_score(_emu(), ck, extra, keys, vids, at, **(t[6] if len(t) > 6 else {}))

# ---- the driver's own judges, on the workers: path_reach (the path's keys from W with a write, no sync) and a
# grid of RAM writes each scored by the picture a few frames later (try_relocate, try_mapobj, try_tornado)
def loss_at(g, strict=(), base=LOSS):
    """the loss cost at game frame g: base, or lower inside a strict window (a, z, cost)"""
    return min([base] + [c for a, z, c in strict if a <= g < z])

def path_score(e, ck, pokes, keys, vids, until, windows=(), mask=(), loss=LOSS, strict=()):
    """From ck's machine with pokes (all of them: the run's and the write), keys[i] at game frame ck['g'] + i
    against video frame vids[i]: [the game frame where the picture leaves the video (30 frames above loss), or
    until; the mean frame cost]. The comparison leaves out mask inside the windows (the overlay's)."""
    e.pokes_clear()
    for g, s in pokes: e.poke(g, s)
    e.loadf(ck['state'])
    on = None; bad = 0; tot = 0.0; n = 0
    for i, g in enumerate(range(ck['g'], until)):
        want = any(a <= g < z for a, z in windows)
        if want != on: e.cmd('mask ' + (' '.join(map(str, mask)) if want else '')); on = want
        e.step(keys[i]); c = min(e.errs(vids[i] - 3, 7)); tot += c; n += 1
        bad = bad + 1 if c > loss_at(g, strict, loss) else 0
        if bad >= 30: return [g - 30, tot / n]
    return [until, tot / max(n, 1)]

def grid_score(e, ck, pokes, cands, keys, vid, span, mask=()):
    """From ck's machine with pokes, for each candidate (a list of [addr, value] 32-bit writes made at once):
    the writes, keys stepped, then the best picture error over video frames vid - span .. vid + span."""
    e.pokes_clear(); e.cmd('mask ' + ' '.join(map(str, mask)))
    for g, s in pokes: e.poke(g, s)
    e.loadf(ck['state']); e.save(1)
    out = []
    for ws in cands:
        e.load(1)
        for a, v in ws: e.cmd(f'write32 {a:x} {v:x}')
        for k in keys: e.step(k)
        out.append(min(e.errs(vid - span, 2 * span + 1)))
    return out

def _emu():
    global _E
    if _E is None:
        from emu import Emu
        _E = Emu()
    return _E
def _ptask(t):
    return t['id'], path_score(_emu(), t['ck'], t['pokes'], t['keys'], t['vids'], t['until'], t['windows'], t['mask'],
                               strict=[tuple(x) for x in t.get('strict', [])])
def _gtask(t):
    return t['id'], grid_score(_emu(), t['ck'], t['pokes'], t['cands'], t['keys'], t['vid'], t['span'], t['mask'])
KINDS = {'path': _ptask, 'grid': _gtask}

