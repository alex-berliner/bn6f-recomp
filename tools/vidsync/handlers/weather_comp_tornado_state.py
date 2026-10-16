"""A repair handler for the Mr. Weather comp (map group 0x83) when one of its tornadoes goes another way than
the video's: ours parks on MegaMan's path and catches him, or stands where the video's does not, so the
orb ring MegaMan lays does not close around it where the video's closes.

A tornado (map object type 0x1A, owMapObject_weatherTornado_80A8E74) moves in segments: mode +9 = 0 moving,
+0x24 frames left of the segment (of +0x26 = 45), direction +5 (0..7, drawn by
owMapObject_weatherTornado_pickDirection_80A9004 from GetRNG at each segment's end); after +7 (2) segments
(+0x28 counts them) it pauses (+9 = 1) for +0x27 = 160 frames (+0x25 counts down). Where it goes is RNG and
history (blocked re-picks, the cutscenes that stop it), so one movement state written at one frame puts
it on the video's track for the rest of the segment and the pause after it: that is the write this handler
looks for. For each live tornado and write frame Wt (every 4 frames over the BACK frames before the loss):
moving with direction d, countdown m and segments n, or paused with countdown p -- a byte write per field,
as a poke at Wt - 1 (before the step Wt -> Wt+1). A tornado ours has elsewhere (off screen, where the video
shows it near MegaMan) is also placed: X/Y on a grid around MegaMan's place at the loss, held paused for p
frames (paused: a moving tornado put on a blocked tile re-picks its direction forever,
owMapObject_weatherTornado_repickDirection_80A90E4), first coarse, then finely around the best. Every
candidate gets the quick pass (the path's keys and video frames, the picture's error every other frame from
the earliest Wt to the loss + TAIL), the best few a sync from the window's start; the furthest one that gets
past the loss wins.

The ring itself only closes when MegaMan holds A all along the trail (sub_809005C resets it on any frame
without A): the input display drops A for single frames there (bits_fixes.json), so the path's keys are
taken with those corrections (an A release the corrected display does not show is held)."""

NAME = "weather_comp_tornado_state"

BACK = 120               # frames before the loss the write may go in
WT_STEP = 6              # write frames every WT_STEP (movement writes)
PLACE_STEP = 16          # write frames every PLACE_STEP (placements)
GRID, GRID_STEP = 160, 16  # placements: +-GRID px around MegaMan, GRID_STEP apart (then +-GRID_STEP, 2 apart)
PAUSES = (60, 120, 180, 255)  # placements: paused for this long
TAIL = 60                # frames past the loss the quick pass compares
TOP = 12                 # candidates that get a sync
HOLD = 300               # frames past the loss the sync is run to
TAKE = 100               # a fix must get this far past the loss (the comp's next tornado is the next loss)
N_OBJ = 56
TORNADO = 0x1A           # map object type byte (+1)
ACTIVE = 4               # state +8: active (moving or paused)


def _tornadoes(e, syms):
    objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
    buf = bytes(e.read(objs, osz * N_OBJ))
    return [i for i in range(N_OBJ) if buf[i * osz] & 1 and buf[i * osz + 1] == TORNADO and buf[i * osz + 8] == ACTIVE]


def match(D: int, sync) -> bool:
    '''Cheap: on the map in the Weather comp (map group 0x83) with a live tornado near the loss.'''
    syms, e = sync.syms, sync.b.e
    w = sync.state_at(max(D - 3, sync.ring[0][0]))
    if w is None:
        return False
    e.load(w[1])
    return (e.read(syms.GS_SUBSYSTEM, 1)[0] == 4 and e.read(syms.GS_MAP, 1)[0] == 0x83
            and bool(_tornadoes(e, syms)))


def try_fix(D: int, sync) -> tuple | None:
    '''The sync lost the video near game frame D: a fix, or None.'''
    syms, b, e, log, bm = sync.syms, sync.b, sync.b.e, sync.log, sync.bm
    objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
    w0 = sync.state_at(max(D - BACK, sync.ring[0][0]))
    if w0 is None:
        return None
    W = w0[0]
    p0 = b.press_gf
    ks = sync.path_keys()
    if D - p0 >= len(ks):
        return None
    # the path's keys and video frames from W to the loss (A held where the corrected display holds it), the
    # video's display keys at its rate past it (as path_reach)
    keys, vids = [], []
    for g in range(W, D + 1):
        k, v = ks[g - p0]
        if k & 1 and not b.key(v) & 1:
            k &= ~1
        keys.append(k); vids.append(v)
    vD = vids[-1]
    for i in range(1, TAIL + 2):
        v = vD + int(round(i * bm.RATE)); keys.append(b.key(v)); vids.append(v)
    held = sum(1 for g in range(W, D + 1) if keys[g - W] != ks[g - p0][0])
    ck = sync.ck_file(w0)
    # the tornadoes live at the loss, as the path has them
    e.load(w0[1])
    slots = _tornadoes(e, syms)
    if not slots:
        return None
    wts = list(range(W + 2, D - 8, WT_STEP))
    first = wts[0] - W
    at = list(range(first, len(keys), 2))
    q0 = sync.evaluate([(ck, [], keys, vids, at, 'q0')] * 2)['q0']
    cands = {}
    for s in slots:
        o = objs + s * osz
        for wt in wts:
            for d in range(8):
                for m in range(2, 47, 4):
                    for n in range(3):
                        cands[f'{s}:{wt}:M{d}:{m}:{n}'] = [[wt - 1, f'{o + 9:#x}=0x0/1,{o + 5:#x}={d:#x}/1,'
                                                                    f'{o + 0x24:#x}={m:#x}/1,{o + 0x28:#x}={n:#x}/1']]
            for p in range(4, 161, 4):
                cands[f'{s}:{wt}:P{p}'] = [[wt - 1, f'{o + 9:#x}=0x1/1,{o + 0x25:#x}={p:#x}/1']]
    # placements: around MegaMan's place at the loss, as the path has it
    wD = sync.state_at(D)
    e.load(wD[1])
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
    pp = e.u32(syms.GS_OW_PLAYER_PTR)
    mx, my = s32(e.u32(pp + 0x1C)) >> 16, s32(e.u32(pp + 0x20)) >> 16
    pwts = list(range(W + 2, D - 8, PLACE_STEP))
    place = lambda s, wt, x, y, p: [[wt - 1, f'{objs + s * osz + 0xC:#x}={(x << 16) & 0xFFFFFFFF:#x}/4,'
                                             f'{objs + s * osz + 0x10:#x}={(y << 16) & 0xFFFFFFFF:#x}/4,'
                                             f'{objs + s * osz + 9:#x}=0x1/1,{objs + s * osz + 0x25:#x}={p:#x}/1']]
    for s in slots:
        for wt in pwts:
            for dx in range(-GRID, GRID + 1, GRID_STEP):
                for dy in range(-GRID, GRID + 1, GRID_STEP):
                    for p in PAUSES:
                        cands[f'{s}:{wt}:X{mx + dx}:{my + dy}:{p}'] = place(s, wt, mx + dx, my + dy, p)
    log(f'  tornado state: slots {slots}, {len(wts)} write frames {wts[0]}..{wts[-1]} and {len(pwts)} for '
        f'placements around MegaMan ({mx},{my}), {len(cands)} writes; {held} A releases held as the corrected '
        f'display has them; the path scores {q0:.0f}')
    qs = sync.evaluate([(ck, ex, keys, vids, at, tid) for tid, ex in cands.items()])
    # the best placement again, finely: +-GRID_STEP px 2 apart, the pauses between
    bp = min((tid for tid in qs if ':X' in tid), key=qs.get, default=None)
    if bp is not None:
        s_, wt_, x_, y_, p_ = bp.split(':')
        s_, wt_, x_, y_, p_ = int(s_), int(wt_), int(x_[1:]), int(y_), int(p_)
        fine = {f'{s_}:{wt_}:X{x_ + dx}:{y_ + dy}:{p}': place(s_, wt_, x_ + dx, y_ + dy, p)
                for dx in range(-GRID_STEP, GRID_STEP + 1, 2) for dy in range(-GRID_STEP, GRID_STEP + 1, 2)
                for p in range(max(4, p_ - 56), min(255, p_ + 56) + 1, 8)}
        log(f'  tornado state: best placement {bp} ({qs[bp]:.0f}); {len(fine)} fine ones')
        cands.update(fine)
        qs.update(sync.evaluate([(ck, ex, keys, vids, at, tid) for tid, ex in fine.items()]))
    good = sorted((q, tid) for tid, q in qs.items())[:TOP]
    log('  tornado state: quick best ' + ' '.join(f'{tid} {q:.0f}' for q, tid in good[:6]))
    if not good or good[0][0] >= q0:
        log('  tornado state: no write scores better than the path')
        return None
    until = D + HOLD
    res = sync.evaluate([(ck, cands[tid], until, tid) for q, tid in good])
    best = max(good, key=lambda t: (res[t[1]][0], -res[t[1]][1]))[1]
    log(f'  tornado state: synced: ' + ' '.join(f'{tid}->{res[tid][0]} ({res[tid][1]:.2f})' for q, tid in good[:6])
        + f'; best {best} to {res[best][0]} (mean cost {res[best][1]:.3f})')
    if res[best][0] < D + TAKE:
        return None
    log(f'  tornado state: take {best}: {cands[best][0][1]} at {cands[best][0][0]}')
    return sync.state_at(W), cands[best]
