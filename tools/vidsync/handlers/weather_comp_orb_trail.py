"""A repair handler for the Weather comp's orb trail: at the loss the video's comp takes its outcome --
the orb ring closing on the tornado -- while ours, one raise of eWeatherCompState.orbTrail
(unk_2000310+0x06) behind, never closes it: the pictures leave the video in the comp's own top middle
and then, as the outcome's screens play, everywhere.

Writing the counter ahead, or the 6-frame tick that raises it, holds nothing (this handler's earlier
attempts: the raise's writer and the capture's check read the ring's own points, not the value left
behind), and the built-ins' tornado place, state, flags, hit and RNG writes all stall at the same
frames.  So this one runs the capture instead: owMapObject_weatherTornado closes the ring when the
tornado is inside it, and the ring's points are MegaMan's own places one and more raise periods back.
The tornado is written to those places -- pinned for a raise period or two, so the check sees it inside
on its own frame, and never within 20 px of MegaMan, where
owMapObject_weatherTornado_checkHitPlayer makes it a hit instead -- at each frame around the loss; the
game's own capture code then plays the video's outcome, past which the sync re-locks.  Every placement
is judged, as the built-ins judge theirs, by how far the path then holds; where the best still runs
out, every live field of the comp is nudged +-1 (or kicked to a small value) at the new divergence and
chained, and a fix is returned only if it holds well past the loss."""

NAME = "weather_comp_orb_trail"

BACK = 90               # frames before the loss the tornado is live (try_tornado's window)
AHEAD = 12              # frames past the loss to trace the trail and MegaMan
HOLD = 600              # frames past the loss a fix must hold (the test's bar)
TAKE = 300              # the built-ins' bar: holding this far past D is worth taking
STEPS = 5               # chain steps past the first placement
TORNADO_ID = 0x2000100  # map object 0's id word while the Weather comp's tornado is live (an id, not an address)
ORB = 0x06              # eWeatherCompState.orbTrail (unk_2000310+0x06)
COMP_LIVE = 0x28        # the comp's live counters run to +0x28, where the tornadoes' saved state begins
NEAR_HIT = 24           # px: nearer than checkHitPlayer's 20 the tornado on MegaMan is a hit, not a capture


def try_fix(D: int, sync) -> tuple | None:
    '''The sync lost the video near game frame D: a fix, or None.'''
    syms, b, e, log = sync.syms, sync.b, sync.b.e, sync.log
    comp = syms.WEATHER_COMP_STATE
    orb = comp + ORB

    # ours only: on the overworld map, the Weather comp's tornado live as map object 0 (as try_tornado)
    w0 = sync.state_at(max(D - BACK, sync.ring[0][0]))
    if w0 is None:
        return None
    W = w0[0]
    e.load(w0[1])
    if e.read(syms.GS_SUBSYSTEM, 1)[0] != 4:
        return None
    if e.u32(syms.OW_MAP_OBJECTS + 4) != TORNADO_ID or not e.read(syms.OW_MAP_OBJECTS, 1)[0] & 1:
        return None
    ks = sync.path_keys()
    p0 = b.press_gf
    hi = min(D + AHEAD, p0 + len(ks) - 1)
    if hi < D:
        return None

    # the trail and MegaMan every frame around the loss, on the path's own keys (the run's pokes in):
    # the ring's points are MegaMan's own places one and more raise periods back
    sd = lambda x, y: ((x - y + 128) & 0xFF) - 128
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
    sync.set_emu_pokes()
    e.load(w0[1])
    snap, pos = {}, {}
    for g in range(W, hi + 1):
        if g > W:
            e.step(ks[g - p0][0])
        snap[g] = bytes(e.read(comp, COMP_LIVE))
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        pos[g] = (e.u32(p + 0x1C), e.u32(p + 0x20))
    sync.set_emu_pokes()
    rises = [g for g in range(W + 1, hi + 1) if 0 < sd(snap[g][ORB], snap[g - 1][ORB]) < 8]
    gaps = [l - f for f, l in zip(rises, rises[1:])]
    cad = max(set(gaps), key=gaps.count) if gaps else 6
    if not 2 <= cad <= 12:
        cad = 6
    live = [o for o in range(COMP_LIVE) if len({snap[g][o] for g in snap}) > 1]
    log(f'  orb trail: eWeatherCompState.orbTrail ({orb:#x}) rose at {" ".join(map(str, rises[:10])) or "no frame"}'
        f' (cadence {cad}): closing the ring on the tornado, not writing the counter')
    ck = sync.ck_file(w0)               # state_at keeps one slot: the return re-derives w from W

    # the tornado into the ring, at each of MegaMan's own trail places, pinned a raise period or two, at
    # each frame around the loss: the capture's own check then runs the video's outcome
    objs = syms.OW_MAP_OBJECTS
    until = D + HOLD
    tasks = []
    for a in range(D - 24, min(D + 13, hi + 1), 2):
        xa, ya = pos[a]
        for k in range(cad, 8 * cad + 1, max(2, cad // 2)):
            bk = a - k
            if bk < W:
                continue
            x, y = pos[bk]
            if (abs(s32(x - xa)) >> 16) + (abs(s32(y - ya)) >> 16) < NEAR_HIT:
                continue                # on MegaMan: the hit (checkHitPlayer), not the capture
            spec = f'{objs + 0xC:#x}={x:#x}/4,{objs + 0x10:#x}={y:#x}/4'
            for pin in (cad, 2 * cad):
                tasks.append((ck, [[q, spec] for q in range(a, a + pin)], until, f'I{a - D:+d}:{k}:{pin}'))
        x, y = pos[a - 2 * cad]
        spec = f'{objs + 0xC:#x}={x:#x}/4,{objs + 0x10:#x}={y:#x}/4'
        tasks.append((ck, [[q, spec] for q in range(a, a + 2 * cad)] +
                      [[a, f'{orb:#x}={(snap[a][ORB] + 1) & 0xFF:#04x}/1']], until, f'N{a - D:+d}'))
    if not tasks:
        return None
    res = sync.evaluate(tasks)
    ok = [t for t in tasks if res[t[3]][0] >= until]
    top = max(tasks, key=lambda t: (res[t[3]][0], -res[t[3]][1]))
    log(f'  orb trail: tornado into the ring: {len(ok)} of {len(tasks)} hold to {until} '
        f'(furthest {top[3]} to {res[top[3]][0]}, mean cost {res[top[3]][1]:.3f})')
    if ok:
        t = min(ok, key=lambda t: res[t[3]][1])
        log(f'  orb trail: take {t[3]} (mean cost {res[t[3]][1]:.3f})')
        return sync.state_at(W), t[1]

    # where the best still runs out: every live field of the comp nudged +-1 (or kicked to a small
    # value) at the new divergence, on top of it -- the outcome reads the comp as well as the ring
    base, reach, mean = top[1], res[top[3]][0], res[top[3]][1]
    for _ in range(STEPS):
        if reach >= until or not live:
            break
        r = reach
        ext = []
        for o in live:
            v0 = snap[min(max(r, W), hi)][o]
            for dv in (1, -1):
                for g in (r - 4, r, r + 4):
                    ext.append((ck, base + [[g, f'{comp + o:#x}={(v0 + dv) & 0xFF:#04x}/1']], until,
                                f'A{o:#04x}@{g - D:+d}:{dv}'))
            for v in (0, 1, 2):
                ext.append((ck, base + [[r, f'{comp + o:#x}={v:#04x}/1']], until, f'B{o:#04x}:{v}'))
        rx = sync.evaluate(ext)
        bx = max(ext, key=lambda t: (rx[t[3]][0], -rx[t[3]][1]))
        log(f'  orb trail: comp state at {r}: furthest {bx[3]} to {rx[bx[3]][0]} (mean cost {rx[bx[3]][1]:.3f})')
        if rx[bx[3]][0] <= r + 1:
            break
        base, reach, mean = bx[1], rx[bx[3]][0], rx[bx[3]][1]
    if reach >= until:
        log(f'  orb trail: take the chain (holds to {reach}, mean cost {mean:.3f})')
        return sync.state_at(W), base
    if reach >= D + TAKE:
        log(f'  orb trail: take the chain (holds to {reach}, mean cost {mean:.3f}); past the comp outcome')
        return sync.state_at(W), base
    return None
