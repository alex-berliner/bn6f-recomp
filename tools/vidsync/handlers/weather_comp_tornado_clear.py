"""A repair handler for the Weather comp's tornado on MegaMan's path -- the HIT, at the video's own
moment (the loss at game 435876, comp map 0x83:0x01, tornado map object slot 1, type 0x1A).  The
video's MegaMan had lured the tornado onto his own path, where it sat parked and unrendered near
(-56..-96, 216..239); around game 435,929-435,943 his walk brought him inside
owMapObject_weatherTornado_checkHitPlayer's +-20 px box, and the hit played in the video too: the
picture darkens one step at ~435,931-435,934 (the darker Navi palette the moment the hit processes),
the white flash peaks at ~435,943-435,955 and the video runs on -- flag 0xBE6,
weatherComp_pickScript_8090104 decrements the comp's strike counter (3 -> 2) and runs the cutscene:
a white fade ramp, a flash peak, an auto-walk putting MegaMan back ~110 px right, then the fade
lifts.  Our run's tornado parked at (48,232), also on his path but ~100 px from where the video's
sat, so our hit fired at 435876, ~55 frames early, and the sync lost the video; pinning the tornado
far away (rounds 1-2) removes the early hit but then no hit ever fires, and around 435,935+ our run
takes a corner fade the video does not and the sync breaks there instead.

So this handler stages the hit where the video has it: at the bottom-left end of his loop, just
before his walk clamps against the map's bottom wall at y~239.  Pass 1 stays v1's: the path's own
keys over the whole window record MegaMan's coords and every live tornado's place each frame, giving
the near-tornado slots, the far-corner and offset parks, and the STEP-framed pin writer.  Pass 2
steps again with the far-corner pin IN -- from W on the path's own keys while they last (to E, the
last known key, ~D+29 on this loss) -- and past E the walk is DERIVED BY PICTURE, not read from the
input display (the display's arrow bits are noisy on this map segment while the rainbow-orb trail
animates, and round 3's display-read walk left his true line, so every staging point was wrong): a
greedy per-frame key search in the beam's spirit -- 0x3FF and each single direction, plus the
previous frame's pick, each stepped from the same saved state and scored by sync.b.e.errs at the
frame's own video mapping, the best kept and MegaMan's place recorded, on to D+90.  Each candidate
then parks the tornado away from W through D+24 (no early hit) and writes it to ONE fixed path
point P -- a point he crosses in D+50..D+70 -- every frame from D+25 until 12 frames after he first
comes within the hit box of P, and nothing after: the hit fires in the video's own band with the
video's fade, put-back and palette -- the tornado's mode, counters and RNG untouched.  The gate is
relaxed so the beam (which searches keys itself and is the real judge) gets to run the fix: every
candidate is judged by sync.path_reach only to D+50 -- past that the display-read keys inside
path_reach are the unreliable ones -- against v1's pure far-corner pin judged the same way, and the
smallest-mean holder whose mean is no worse than the pure pin's mean + 0.15 is taken."""


NAME = "weather_comp_tornado_clear"

BACK = 120               # frames before the loss to watch from (the tornado's approach)
HOLD = 600               # frames past the loss a fix must hold (the test's bar)
NEAR = 48                # px: this close to MegaMan our game captures where the video does not (hit box 20)
STEP = 16                # frames between pin writes: the tornado drifts up to ~2.2 px/frame
N_OBJ = 56               # eOverworldMapObjects slots (oOverworldMapObject_Size each)
TORNADO = 0x1A           # the type byte: the Weather comp's tornado (owMapObject_weatherTornado)
OFF_TYPE = 0x1                  # header +0 bit 0 active, +1 the type byte
OFF_X, OFF_Y = 0xC, 0x10        # the map object's X/Y (s16.16)
OFF_PX, OFF_PY = 0x1C, 0x20     # the overworld player object's X/Y (s16.16)
CORNER = (1024, 768)            # px left of / above the smallest X/Y the path visits: not rendered, no tile there
PARK_OFFS = ((-464, -192), (512, -256))   # px off the tornado's own place (the probe's (-416,40) among them)
WALK_SLOT = 250                 # an emulator save slot outside the beam's (100..163) and the ring's (3000+)


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
    last = ks[-1][0]
    key = lambda g: ks[g - p0][0] if 0 <= g - p0 < len(ks) else last

    # pass 1 (v1): the path's own keys over the whole window -- MegaMan's coords and every live
    # tornado's place, each frame; the path's extent feeds the far-corner park
    pos, tor = {}, {}
    sync.set_emu_pokes()
    e.load(w0[1])
    for g in range(W, D + HOLD + 1):
        if g > W:
            e.step(key(g))
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p:
            pos[g] = (s32(e.u32(p + OFF_PX)) >> 16, s32(e.u32(p + OFF_PY)) >> 16)
        buf = bytes(e.read(objs, osz * N_OBJ))
        for i in range(N_OBJ):
            o = i * osz
            if buf[o] & 1 and buf[o + OFF_TYPE] == TORNADO:
                tor.setdefault(i, {})[g] = (int.from_bytes(buf[o + OFF_X:o + OFF_X + 4], 'little'),
                                            int.from_bytes(buf[o + OFF_Y:o + OFF_Y + 4], 'little'))
    sync.set_emu_pokes()
    if not pos or not tor:
        log('  tornado clear: no live tornado map object on the map: not this loss')
        return None

    # the video shows the hit only in its own band: a tornado near MegaMan earlier is ours being wrong
    near = {}
    for i, t in tor.items():
        for g in sorted(t):
            if g not in pos:
                continue
            mx, my = pos[g]
            dx = abs((s32(t[g][0]) >> 16) - mx)
            dy = abs((s32(t[g][1]) >> 16) - my)
            if dx * dx + dy * dy <= NEAR * NEAR:
                near[i] = g
                break
    if not near:
        log('  tornado clear: no tornado comes near MegaMan over the window: not this loss')
        return None
    xs = [x for x, _ in pos.values()]
    ys = [y for _, y in pos.values()]
    log('  tornado clear: tornado slot(s) ' + ' '.join(f'{i} (near at {near[i] - D:+d})' for i in sorted(near)) +
        f" on MegaMan's path (window x {min(xs)}..{max(xs)}, y {min(ys)}..{max(ys)}): staging the hit")

    # v1's writers: the write spec (two write32 entries per frame, X then Y), the parks, the STEP pin
    def spec(i, rx, ry):
        return f'{objs + i * osz + OFF_X:#x}={rx:#x}/4,{objs + i * osz + OFF_Y:#x}={ry:#x}/4'

    def park(i, start, off):
        if off is None:
            x, y = min(xs) - CORNER[0], min(ys) - CORNER[1]
        else:
            fs = sorted(tor[i])
            g0 = next((f for f in fs if f >= start), fs[-1])
            x = (s32(tor[i][g0][0]) >> 16) + off[0]
            y = (s32(tor[i][g0][1]) >> 16) + off[1]
        return (x << 16) & 0xFFFFFFFF, (y << 16) & 0xFFFFFFFF

    def pin(start, off, until):
        s = ','.join(spec(i, *park(i, start, off)) for i in sorted(near))
        return [[g, s] for g in range(start, until + 1, STEP)]

    # pass 2 (round 4): with the far-corner pin IN, MegaMan's walk from W on the path's own keys
    # while they last (to E, the last known key, ~D+29 on this loss); past E the walk is DERIVED BY
    # PICTURE, not read from the input display (its arrow bits are noisy while the rainbow-orb trail
    # animates on this map segment): a greedy per-frame key search in the beam's spirit -- 0x3FF and
    # each single direction, plus the previous frame's pick, each stepped from the same saved state
    # and scored by e.errs at the frame's own video mapping, the best kept -- recording his true
    # place each frame to D+90 (the walk is unaffected by the pin, so these are the places the real
    # run takes on those keys)
    lv = ks[-1][1]
    E = p0 + len(ks) - 1                       # the last frame the path's own keys cover
    away = pin(W, None, D + 24)                # the away pin: no early hit, handed over to P at D+25
    ktry = [0x3FF] + [0x3FF & ~(1 << bt) for bt in (4, 5, 6, 7)]
    prev, place = None, {}
    sync.set_emu_pokes(away)
    e.load(w0[1])
    for g in range(W, D + 90 + 1):
        if g > W:
            if g <= E:
                prev = key(g)
                e.step(prev)
            else:
                snap = e.save(WALK_SLOT)       # every candidate key steps from the same state
                v = lv + int(round((g - E) * sync.bm.RATE))
                ks_c = ktry + ([prev] if prev is not None and prev not in ktry else [])
                bk, bc = None, None
                for k in ks_c:
                    e.load(WALK_SLOT)
                    e.step(k)
                    c = e.errs(v, 1)[0]
                    if bc is None or c < bc:
                        bc, bk = c, k
                e.load(WALK_SLOT)
                e.step(bk)
                prev = bk
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p:
            place[g] = (s32(e.u32(p + OFF_PX)) >> 16, s32(e.u32(p + OFF_PY)) >> 16)
    sync.set_emu_pokes()

    # the candidates: the away pin from W through D+24 (STEP=16), then ONE fixed path point P -- a
    # point MegaMan crosses in D+50..D+70 -- written EVERY frame from D+25 through 12 frames past the
    # first frame he is inside the +-20 px hit box of P (ar, scanned from a-24), nothing after and
    # never re-pinned (the tornado is unrendered; its later wander is not in the picture)
    cands = []
    for a in range(D + 50, D + 71, 2):
        if a not in place:
            continue
        P = place[a]
        ar = next((g for g in range(a - 24, a + 1) if g in place and
                   abs(place[g][0] - P[0]) <= 20 and abs(place[g][1] - P[1]) <= 20), a)
        s = ','.join(spec(i, (P[0] << 16) & 0xFFFFFFFF, (P[1] << 16) & 0xFFFFFFFF) for i in sorted(near))
        cands.append((away + [[g, s] for g in range(D + 25, min(ar + 12, D + 90) + 1)],
                      f'the path hit at {a - D:+d} (arrival {ar - D:+d}, P {P[0]},{P[1]})'))

    # the relaxed gate: path_reach only to D+50 -- past that the display-read keys inside path_reach
    # are the unreliable ones, and the beam (which searches keys itself) is the real judge; v1's pure
    # far-corner pin judged the same way is the baseline a candidate's mean may exceed by at most 0.15
    until = D + 50
    pure = pin(W, None, D + HOLD)
    pr, pm = sync.path_reach(w0, pure, until, D)
    log(f'  tornado clear: pin from {W - D:+d} at the far corner: holds to {pr} (mean cost {pm:.3f})')
    best = None
    for pokes, tag in cands:
        reach, mean = sync.path_reach(w0, pokes, until, D)
        log(f'  tornado clear: {tag}: holds to {reach} (mean cost {mean:.3f})')
        if reach >= until and mean <= pm + 0.15 and (best is None or mean < best[0]):
            best = (mean, tag, pokes)
    if best:
        log(f'  tornado clear: take {best[1]} (mean cost {best[0]:.3f})')
        return sync.state_at(W), best[2]
    return None
