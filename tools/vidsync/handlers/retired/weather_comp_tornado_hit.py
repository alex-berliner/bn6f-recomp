"""A repair handler for the Weather comp's tornado hitting MegaMan at the video's own moment (the loss at
game 435876, comp map 0x83:0x01, tornado map object slot 1, type 0x1A).  Our comp's tornado parked on his
path at (48,232) px, so owMapObject_weatherTornado_checkHitPlayer (the +-0x14 px box, flag 0xBE6) caught
him at 435876 while the video's tornado -- parked ~100 px away, where its own RNG walk left it -- catches
him only at ~435,929-435,935: our white flash runs ~55 frames early and the sync loses the video.  The
tornado's parked place comes from its own RNG walk and no single RNG write found moves it (the driver
searched 1h47m), and pinning it away forever removes the early hit but then no hit ever fires where the
video's does.  The video's cutscene puts MegaMan back at the comp's own fixed point (eWeatherCompState
+0x12/+0x14, weatherComp_putPlayerBack_808F810), so only the hit's moment matters.

So this handler stages the hit: the tornado is pinned far off the path (the unrendered far corner past the
smallest X/Y the path visits) from the window's start W up to a swept frame t, then written ONTO MegaMan --
his own replayed place, distance 0, inside the 0x14 box -- at t and t+1, so the game's own capture runs at
exactly t: flag 0xBE6, the strike counter down, the video's white fade, put-back and palette.  The sweep
tries t over D+34..D+80 (the video's darkening sits at ~D+55), each judged by sync.path_reach all the way
to D+HOLD -- the cutscene and its after-play included, so the video's own hit time wins the mean cost
outright.  MegaMan's place at t comes from two replays: the path's own keys to D and the video's display
keys past it (what path_reach itself steps), and a greedy per-frame key search past the last path key scored
with the beam's own offset-window cost min(errs(v-3,7)) (the display's arrow bits are noisy here).  A fix is
returned only if a candidate holds to D+HOLD; the smallest mean cost wins."""

NAME = "weather_comp_tornado_hit"

BACK = 120               # frames before the loss to watch from (the tornado's approach)
HOLD = 600               # frames past the loss a fix must hold (the test's bar)
NEAR = 48                # px: this close to MegaMan our game captures where the video does not (hit box 20)
STEP = 16                # frames between away-pin writes
N_OBJ = 56               # eOverworldMapObjects slots (oOverworldMapObject_Size each)
TORNADO = 0x1A           # the type byte: the Weather comp's tornado (owMapObject_weatherTornado)
OFF_TYPE = 0x1           # header +0 bit 0 active, +1 the type byte
OFF_X, OFF_Y = 0xC, 0x10  # the map object's X/Y (s16.16)
OFF_PX, OFF_PY = 0x1C, 0x20  # the overworld player object's X/Y (s16.16)
CORNER = (1024, 768)     # px left of / above the smallest X/Y the path visits: not rendered, no tile there
EARLY, LATE, TSTEP = 34, 80, 2   # the hit-time sweep (frames past D)
WALK_SLOT = 250          # an emulator save slot outside the beam's (100..163) and the ring's (3000+)


def try_fix(D: int, sync) -> tuple | None:
    '''The sync lost the video near game frame D: a fix, or None.'''
    syms, b, e, log = sync.syms, sync.b, sync.b.e, sync.log
    objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v

    w0 = sync.state_at(max(D - BACK, sync.ring[0][0]))
    if w0 is None:
        return None
    W = w0[0]
    e.load(w0[1])
    if e.read(syms.GS_SUBSYSTEM, 1)[0] != 4:
        return None                                    # not on the overworld map: not this loss
    ks = sync.path_keys()
    if not ks:
        return None
    p0 = b.press_gf
    hi = D + LATE + 8
    pk = ks[:D - p0 + 1]                               # the path's own keys, to the loss
    lv = pk[-1][1]
    keys = list(pk) + [(b.key(lv + int(round(k * sync.bm.RATE))), lv + int(round(k * sync.bm.RATE)))
                       for k in range(1, hi - D + 2)]  # past the loss: the video's display keys, as path_reach

    # the far corner off the path the window records; the away-pin writes
    def pin(end, cx, cy):
        spec = ','.join(f'{objs + i * osz + o:#x}={(cx if o == OFF_X else cy):#x}/4'
                        for i in sorted(slots) for o in (OFF_X, OFF_Y))
        return [[g, spec] for g in range(W, end, STEP)]

    # pass 1a: the natural replay to D+8 (the run's own pokes): every live tornado's place -- the
    # near-check that recognises this loss (our early hit)
    slots, tor, pos0 = set(), {}, {}
    sync.set_emu_pokes()
    e.load(w0[1])
    for g in range(W, D + 9):
        if g > W:
            e.step(keys[g - p0][0])
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p:
            pos0[g] = (e.u32(p + OFF_PX), e.u32(p + OFF_PY))
        buf = bytes(e.read(objs, osz * N_OBJ))
        for i in range(N_OBJ):
            o = i * osz
            if buf[o] & 1 and buf[o + OFF_TYPE] == TORNADO:
                slots.add(i)
                tor.setdefault(i, {})[g] = (int.from_bytes(buf[o + OFF_X:o + OFF_X + 4], 'little'),
                                            int.from_bytes(buf[o + OFF_Y:o + OFF_Y + 4], 'little'))
    if not pos0 or not slots:
        log('  tornado hit: no live tornado map object on the map: not this loss')
        return None
    near = {}
    for i, t in tor.items():
        for g in sorted(t):
            if g not in pos0:
                continue
            mx, my = s32(pos0[g][0]) >> 16, s32(pos0[g][1]) >> 16
            dx = abs((s32(t[g][0]) >> 16) - mx)
            dy = abs((s32(t[g][1]) >> 16) - my)
            if dx * dx + dy * dy <= NEAR * NEAR:
                near[i] = g
                break
    if not near:
        log('  tornado hit: no tornado comes near MegaMan over the window: not this loss')
        return None
    log('  tornado hit: tornado slot(s) ' + ' '.join(f'{i} (near at {near[i] - D:+d})' for i in sorted(near)) +
        ': staging the hit at the video\'s moment')

    # pass 1b: the same window with the tornado pinned away from the start -- MegaMan's FREE walk each
    # frame (no capture anywhere), which is what the staged hit is written onto; the corner comes from
    # pass 1a's places (the walk to D+8 spans the path's extent here)
    cx = (min(s32(v[0]) >> 16 for v in pos0.values()) - CORNER[0]) << 16 & 0xFFFFFFFF
    cy = (min(s32(v[1]) >> 16 for v in pos0.values()) - CORNER[1]) << 16 & 0xFFFFFFFF
    pos_d = {}
    sync.set_emu_pokes(pin(hi, cx, cy))
    e.load(w0[1])
    for g in range(W, hi + 1):
        if g > W:
            e.step(keys[g - p0][0])
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p:
            pos_d[g] = (e.u32(p + OFF_PX), e.u32(p + OFF_PY))

    # pass 2: the greedy walk past the last path key E, scored with the beam's own offset-window cost
    E = p0 + len(pk) - 1
    ktry = [0x3FF] + [0x3FF & ~(1 << bt) for bt in (4, 5, 6, 7)] + \
           [0x3FF & ~((1 << a) | (1 << c)) for a in (4, 5) for c in (6, 7)]
    pos_g = {}
    sync.set_emu_pokes(pin(hi, cx, cy))
    e.load(w0[1])
    prev = None
    for g in range(W, hi + 1):
        if g > W:
            if g <= E:
                prev = keys[g - p0][0]
                e.step(prev)
            else:
                e.save(WALK_SLOT)
                v = keys[g - p0][1]
                cand = ktry + ([prev] if prev is not None and prev not in ktry else [])
                bk, bc = None, None
                for k in cand:
                    e.load(WALK_SLOT)
                    e.step(k)
                    c = min(e.errs(v - 3, 7))
                    if bc is None or c < bc:
                        bc, bk = c, k
                e.load(WALK_SLOT)
                e.step(bk)
                prev = bk
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p:
            pos_g[g] = (e.u32(p + OFF_PX), e.u32(p + OFF_PY))
    sync.set_emu_pokes()

    # the sweep: the tornado away from W to t, then ON MegaMan (his display-walk place, distance 0, inside
    # the 0x14 box) every frame t..t+16 -- the capture runs at t in any run of these keys; the video's own
    # hit time wins the mean cost outright
    until = D + HOLD
    pure = pin(hi, cx, cy)
    pr, pm = sync.path_reach(w0, pure, until, D)
    log(f'  tornado hit: the away pin alone (no hit at all): holds to {pr} (mean cost {pm:.3f})')
    ov = [g for g in range(E + 1, min(D + LATE, hi) + 1) if g in pos_g and g in pos_d]
    drift = max((abs(s32(pos_d[g][0]) - s32(pos_g[g][0])) + abs(s32(pos_d[g][1]) - s32(pos_g[g][1]))
                 for g in ov), default=0)
    log(f'  tornado hit: the two walks past the last path key ({E - D:+d}) disagree by at most '
        f'{drift} (s16.16, {len(ov)} frames) over the sweep')
    best = None
    for t in range(D + EARLY, D + LATE + 1, TSTEP):
        if t not in pos_d:
            continue
        hit = [[g, ','.join(f'{objs + i * osz + o:#x}={pos_d[g][j]:#x}/4'
                            for i in sorted(slots) for o, j in ((OFF_X, 0), (OFF_Y, 1)))]
               for g in range(t, min(t + 17, hi))]
        pokes = pin(t, cx, cy) + hit
        reach, mean = sync.path_reach(w0, pokes, until, D)
        if best is None or reach > best[0] or (reach == best[0] and mean < best[1]):
            log(f'  tornado hit: hit at {t - D:+d}: holds to {reach} (mean cost {mean:.3f})')
        if reach >= until and (best is None or (mean, -reach) < (best[1], -best[0])):
            best = (reach, mean, t, pokes)
    if best is None:
        log('  tornado hit: no hit time in the sweep holds to the bar')
        return None
    log(f'  tornado hit: take the hit at {best[2] - D:+d} (mean cost {best[1]:.3f})')
    return sync.state_at(W), best[3]