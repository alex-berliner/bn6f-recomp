"""A repair handler for the Weather comp's tornado catching MegaMan ~94 frames before the video's own catch
does (the loss at game 435876, comp map 0x83:0x01, tornado map object slot 1 parked at (48,232) px).

On the live lineage our tornado sits at (48,232) on MegaMan's path; the video's tornado is elsewhere (its
cloud shows at roughly +100,+60 px off him in the video's thumbnails, then wanders), and the video's
MegaMan walks FREE past the point where ours stands: our tornado, armed (event flag 0xBE4), catches him
(owMapObject_weatherTornado_checkHitPlayer_80A91A4, flag 0xBE6) at 435874 -- and even unarmed it carries
him (the wind drag the walk replay shows, to a random drop at (-150,239)), which the video never does.
The video's own catch is at ~435968: its white flash ramps 435971-976, with the tornado's dark cloud
covering him from ~435963.

So the handler stages the video's catch with ONE flag write.  The game's own catch check
(sub_80A91A4: flag 0xBE4 armed + MegaMan inside the tornado's radius) sets 0xBE6, and the catch animation is
a fixed brightness table (sub_80A937C over byte_80A93C0): a long dark ramp then a white peak, then the
script's put-back and walk-on -- all deterministic.  Our run's trouble is only that the natural walk, armed,
has MegaMan step into the parked tornado's radius at D-2 (a strike: 0xBE6, strikes 2->1), 56 frames before
the comp script's own catch phase (the comp state flips +0x1a 0x1a->0x04 at D+58 in the suppressed walk, the
same script beat the video's pictures enter their ramp at).  So: clear 0xBE4 just before the natural catch
(no strike; the suppressed walk then matches the video's pictures to D+53 with the tornado roaming exactly
as the video's own cloud), then set 0xBE4 again just before that script phase change -- the game's own catch
check then fires the video's catch in the video's catch phase, and the ramp, put-back and walk-on are the
game's own, aligned with the video's frames.  The re-arm frame is swept; a fix is returned only if a
candidate holds to D + HOLD, the smallest mean cost winning."""

NAME = "weather_comp_tornado_steer"

BACK = 120               # frames before the loss to watch from (the tornado parked, the walk in)
HOLD = 600               # frames past the loss a fix must hold (the test's bar)
N_OBJ = 56               # eOverworldMapObjects slots (oOverworldMapObject_Size each)
TORNADO = 0x1A           # the type byte: the Weather comp's tornado (owMapObject_weatherTornado)
OFF_TYPE = 0x1           # header +0 bit 0 active, +1 the type byte
OFF_X, OFF_Y = 0xC, 0x10  # the map object's X/Y (s16.16)
OFF_PX, OFF_PY = 0x1C, 0x20  # the overworld player object's X/Y (s16.16)
CORNER = (1024, 768)     # px left of / above the smallest X/Y the walk visits: not rendered, no tile there
CLEAR = (6, 4, 2)        # 0xBE4 cleared (and the pin started) this many frames before the natural catch
REARM = (54, 56, 58)     # 0xBE4 set again this many frames past the loss, just before the script catch phase at D+58
TRACK0 = 48             # the tornado leaves the corner and takes station this many frames past the loss
HOLD_OFF = 58           # ...and holds the station offset until this many frames past the loss
STATION = (28, 22)      # the station: game px off MegaMan, SE of him (the video cloud's diff centroid ~(+26,+30))
CLOSE = (6, 5)          # the offset it slides to by the catch frame (inside the video cloud's sprite, outside the carry box)
FA4 = None               # filled in try_fix: EVENT_FLAGS + (0xBE4 >> 3)
M4 = 0x80 >> (0xBE4 & 7) # the arm flag's bit mask
M6 = 0x80 >> (0xBE6 & 7) # the hit flag's bit mask (same byte: never copy it from the natural run)


def try_fix(D: int, sync) -> tuple | None:
    '''The sync lost the video near game frame D: a fix, or None.'''
    global FA4
    syms, b, e, log = sync.syms, sync.b, sync.b.e, sync.log
    objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
    s32 = lambda v: v - (1 << 32) if v >= 1 << 32 else v
    FA4 = syms.EVENT_FLAGS + (0xBE4 >> 3)

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
    hi = D + 130
    pk = ks[:D - p0 + 1]
    lv = pk[-1][1]
    keys = list(pk) + [(b.key(lv + int(round(k * sync.bm.RATE))), lv + int(round(k * sync.bm.RATE)))
                       for k in range(1, hi - D + 2)]  # past the loss: the video's display keys, as path_reach

    # the natural replay to D+8 (the run's own pokes): the early catch, the tornado's slots and place, and
    # the flags byte at FA4 every frame (the clear/arm writes keep the byte's other bits as the run had them)
    sync.set_emu_pokes()
    e.load(w0[1])
    hit = None
    slots, tor0 = set(), {}
    pos0, fnat = {}, {}
    for g in range(W, D + 9):
        if g > W:
            e.step(keys[g - p0][0])
        fnat[g] = e.read(FA4, 1)[0]
        if e.read(syms.EVENT_FLAGS + (0xBE6 >> 3), 1)[0] & M6 and hit is None:
            hit = g
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p:
            pos0[g] = (e.u32(p + OFF_PX), e.u32(p + OFF_PY))
        buf = bytes(e.read(objs, osz * N_OBJ))
        for i in range(N_OBJ):
            o = i * osz
            if buf[o] & 1 and buf[o + OFF_TYPE] == TORNADO:
                slots.add(i)
                tor0[i] = (int.from_bytes(buf[o + OFF_X:o + OFF_X + 4], 'little'),
                           int.from_bytes(buf[o + OFF_Y:o + OFF_Y + 4], 'little'))
    if hit is None:
        log('  tornado steer: no tornado catch in the natural walk to D+8: not this loss')
        return None
    if not pos0 or not slots:
        log('  tornado steer: no live tornado map object or no player place: not this loss')
        return None
    log('  tornado steer: the natural catch fires at ' + f'{hit - D:+d}'
        + '; tornado slot(s) ' + ' '.join(f'{i} at ({s32(x) >> 16},{s32(y) >> 16})'
                                          for i, (x, y) in sorted(tor0.items())))

    # MegaMan's own free walk every frame to D+124: the path's keys to D, the video's display keys past it,
    # with 0xBE4 clear and the tornado pinned at the far corner from just before the natural catch (no
    # catch, no carry -- the video's walk)
    g_clear = hit - 2
    cx = (min(s32(v[0]) >> 16 for v in pos0.values()) - CORNER[0]) << 16 & 0xFFFFFFFF
    cy = (min(s32(v[1]) >> 16 for v in pos0.values()) - CORNER[1]) << 16 & 0xFFFFFFFF
    corner = ','.join(f'{objs + i * osz + o:#x}={(cx if o == OFF_X else cy):#x}/4'
                      for i in sorted(slots) for o in (OFF_X, OFF_Y))
    pre = [[g_clear, f'{FA4:#x}={(fnat[g_clear] & ~M4) & ~M6:#04x}/1'],
           [g_clear, corner]]
    sync.set_emu_pokes(pre)
    e.load(w0[1])
    pos_d, fnat2 = {}, {}
    for g in range(W, hi):
        if g > W:
            e.step(keys[g - p0][0])
        fnat2[g] = e.read(FA4, 1)[0]
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p:
            pos_d[g] = (e.u32(p + OFF_PX), e.u32(p + OFF_PY))
    sync.set_emu_pokes()
    if D + max(CATCH) + 2 not in pos_d:
        log('  tornado steer: no replayed free walk past the loss')
        return None

    # the sweep: clear 0xBE4 just before the natural catch (the tornado then roams untouched -- the same
    # RNG lineage as the video's cloud, the pictures match to D+53), and set it again just before the
    # script's catch phase flips at D+58: the game's own check then fires the catch inside the phase, and
    # the fixed-table ramp, put-back and walk-on are the video's own frames.
    until = D + HOLD
    tasks, seen = [], set()
    for ra in REARM:
        if D + ra >= len(fnat2) + W:
            continue
        g = D + ra
        if g not in fnat2:
            continue
        arm = [[g, f'{FA4:#x}={(fnat2[g] & ~M6) | M4:#04x}/1']]
        tag = f'R{ra}'
        if tag in seen:
            continue
        seen.add(tag)
        tasks.append((sync.ck_file(w0), pre + arm, until, tag))
    if not tasks:
        return None
    log(f'  tornado steer: {len(tasks)} candidates (the re-arm at D+{REARM})')
    res = sync.evaluate(tasks)
    ok = [t for t in tasks if res[t[3]][0] >= until]
    top = max(tasks, key=lambda t: (res[t[3]][0], -res[t[3]][1]))
    log(f'  tornado steer: {len(ok)} of {len(tasks)} hold to {until} '
        f'(furthest {top[3]} to {res[top[3]][0]}, mean cost {res[top[3]][1]:.3f})')
    if not ok:
        return None
    t = min(ok, key=lambda t: res[t[3]][1])
    log(f'  tornado steer: take {t[3]} (mean cost {res[t[3]][1]:.3f})')
    return sync.state_at(W), t[1]