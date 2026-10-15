"""A repair handler for the Weather comp's tornado catching MegaMan ~91 frames before the video's own catch
(the loss at game 435876, comp map 0x83:0x01): our tornado sits on his path (where its own RNG walk left it)
so owMapObject_weatherTornado_checkHitPlayer grabs him at 435876, while the video's tornado, parked elsewhere
by its own RNG, grabs him only at ~435967 (its white flash 435971-987).  No single RNG write moves the
tornado (the driver searched ~2h), and the check is the game's own +-0x14 px box that fires whenever flag
0xBE4 is armed and a tornado is inside it, so the only lever is the arm flag itself.

This handler stages both catches at the video's own frames instead.  Flag 0xBE4 (byte 0x2001e04, bit 0x08)
is cleared at 435873 so the early grab cannot fire and MegaMan free-walks the video's path; re-armed at
435959; the active tornado (slot 1) is written onto MegaMan's own free-walk place +0x14 px south at 435965
and 435966, so the game's own capture runs at 435967 = the video's frame exactly.  Flag 0xBEA|0xBEF (byte
0x2001e05, 0x21) written at 435968 puts the caught tornado into its dissipate state -- without it the
tornado falls back to its hunt state while still inside the box and re-fires the catch every frame,
re-triggering the cutscene text over and over (observed: 0xBE6 held ~30 frames and pseudo-catches every ~21
frames).  At 435975 0xBEF is cleared again (the next tornado returns to hunter state 4) and 0xBE4 is cleared
(the remaining tornado freezes off-screen and cannot catch).

The second catch (the video's at 436402, comp end and map reload following it) is staged the same way:
0xBE4 re-armed at 436394, the hunter tornado (slot 0) written onto MegaMan's own place +0x14 south at
436400/436401, capture at 436402, 0xBE4 cleared at 436403.  Pinning alone was not enough at first: the
second tornado sat in attack state 8 (0xBEF) with its isCaught flag semantics unresolved and every
teleport-pinned grab failed; clearing 0xBEF at 435975 (state back to 4, the state the first catch succeeds
from) is what makes the staged second capture fire.

MegaMan's places for the pins come from two replays of the handler's own: the gate-off free walk (no capture
anywhere) gives his track up to the first catch, and the staged first catch gives his track (the knock
cutscene and the walk after it) up to the second.  Both catches verified frame-exact against the video's
white flashes.  The comp-end cutscene that follows still runs ~180 frames late (its text waits consume the
runner's taps differently than the video's game did), so the whole-screen reload fade lands late.  The mask window below (thumbnail rows 30-75, cols 28-91: the
tornado sprite at MegaMan, the cutscene's text box and the tornado-over-camera dark dip) leaves the ground
rows 76-79 compared -- the camera follows MegaMan, so the scrolling ground is his per-frame place seen through the
picture and the mask cannot hide a divergence in him -- and MegaMan's own per-frame place is additionally
checked against this handler's replays in the validation pass.  The reload-tail repair is the follow-up loss."""

NAME = "weather_comp_tornado_nocatch"

BACK = 120               # frames before the loss to watch from
HOLD = 600               # frames past the loss a fix is tried to
NEAR = 48                # px: this close to MegaMan our game captures where the video does not (box 20)
N_OBJ = 56               # eOverworldMapObject slots (oOverworldMapObject_Size each)
TORNADO = 0x1A           # the type byte: the Weather comp's tornado map object
OFF_TYPE = 0x1           # header +0 bit 0 active, +1 the type byte
OFF_X, OFF_Y = 0xC, 0x10  # the map object's X/Y (s16.16)
OFF_ST = 0x8             # the map object's state byte
OFF_PX, OFF_PY = 0x1C, 0x20  # the overworld player object's X/Y (s16.16)

CATCH1_V, CATCH2_V = 435967, 436402  # the video's own catches (white flashes 435971-987 and 436406-420)
C1, C2 = CATCH1_V, CATCH2_V          # our staged captures: the same frames (the inputs hold A down over
                                     # 435966-435968, so the cutscene's text box skips as the video's does)
MASK_RECT = [30, 76, 28, 92]         # thumbnail rows 30-67, cols 28-91: the storm's tornado sprite, the
                                     # catch cutscene's text box and the tornado-over-camera dark dip; the
                                     # ground rows 68-79 stay compared (the camera follows MegaMan, so the
                                     # scrolling ground is his per-frame place seen through the picture)


def try_fix(D: int, sync) -> tuple | None:
    '''The sync lost the video near game frame D: a fix, or None.'''
    syms, b, e, log = sync.syms, sync.b, sync.b.e, sync.log
    objs, osz = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE
    fa4 = syms.EVENT_FLAGS + (0xBE4 >> 3)   # the byte holding flags 0xBE0..0xBE7
    fa5 = syms.EVENT_FLAGS + (0xBE8 >> 3)   # the byte holding flags 0xBE8..0xBEF
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v

    w0 = sync.state_at(max(D - BACK, sync.ring[0][0]))
    if w0 is None:
        return None
    W = w0[0]
    e.load(w0[1])
    if e.read(syms.GS_SUBSYSTEM, 1)[0] != 4:
        return None                                    # not on the overworld map: not this loss
    if not (W <= D - 3 < C1 and W <= C2 - 8):
        return None                                    # not the window this recipe is tuned for
    ks = sync.path_keys()
    if not ks:
        return None
    p0 = b.press_gf
    hi = C2 + 16
    pk = ks[:D - p0 + 1]                               # the path's own keys, to the loss
    lv = pk[-1][1]
    keys = list(pk) + [(b.key(lv + int(round(k * sync.bm.RATE))), lv + int(round(k * sync.bm.RATE)))
                       for k in range(1, hi - D + 2)]  # past the loss: the video's display keys, as path_reach
    gate = f'{fa4:#x}={(0xC4 & ~0x08) & ~0x02:#04x}/1'  # 0xBE4 clear, 0xBE6 clear: no capture can fire

    def mm_track(pokes, end):
        '''A replay of the window with pokes: MegaMan's raw s16.16 place each frame.'''
        sync.set_emu_pokes(pokes)
        e.load(w0[1])
        pos = {}
        for g in range(W, end + 1):
            if g > W:
                e.step(keys[g - p0][0])
            p = e.u32(syms.GS_OW_PLAYER_PTR)
            if p:
                pos[g] = (e.u32(p + OFF_PX), e.u32(p + OFF_PY))
        return pos

    # pass 1a: the natural replay to D+8 (the run's own pokes): every live tornado's place -- the near-check
    # that recognises this loss (our early capture)
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
        log('  tornado nocatch: no live tornado map object on the map: not this loss')
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
        log('  tornado nocatch: no tornado comes near MegaMan over the window: not this loss')
        return None
    log('  tornado nocatch: tornado slot(s) ' + ' '.join(f'{i} (near at {near[i] - D:+d})' for i in sorted(near)) +
        ': staging the catches at the video\'s moments')

    # the two tornado slots the recipe needs: the first catcher (nearest at the loss) and the next hunter
    first = min(near, key=lambda i: near[i])
    rest = sorted(i for i in slots if i != first)
    if not rest:
        log('  tornado nocatch: no second tornado slot for the second catch')
        return None
    second = rest[0]

    # pass 1b: the gate-off free walk (no capture anywhere): MegaMan's track the first catch is written onto
    pos1 = mm_track([[D - 3, gate]], C1)
    if C1 not in pos1:
        return None

    # pass 2: the staged first catch (arm, pins, dissipate, disarm): MegaMan's track the second catch is
    # written onto, and the check that the capture really fires where the video's does
    pins = lambda slot, pos, g: (f'{objs + slot * osz + OFF_X:#x}={((s32(pos[g][0]) >> 16) & 0xFFFF) << 16 & 0xFFFFFFFF:#x}/4,'
                                 f'{objs + slot * osz + OFF_Y:#x}={(((s32(pos[g][1]) >> 16) + 14) & 0xFFFF) << 16 & 0xFFFFFFFF:#x}/4')
    first_catch = [[D - 3, gate],
                   [C1 - 8, f'{fa4:#x}={(0xC4 & ~0x02) | 0x08:#04x}/1'],
                   [C1 - 2, pins(first, pos1, C1 - 1)], [C1 - 1, pins(first, pos1, C1)],
                   [C1 + 1, f'{fa5:#x}=0x21/1'],
                   [C1 + 8, f'{fa5:#x}=0x20/1,' + f'{fa4:#x}=0xC4/1']]
    pos2 = {}
    sync.set_emu_pokes(first_catch)
    e.load(w0[1])
    got = None
    for g in range(W, C2 + 1):
        if g > W:
            e.step(keys[g - p0][0])
            f4 = e.read(fa4, 1)[0]
            if got is None and f4 & 0x02:
                got = g                                # the capture ran
        p = e.u32(syms.GS_OW_PLAYER_PTR)
        if p:
            pos2[g] = (e.u32(p + OFF_PX), e.u32(p + OFF_PY))
    sync.set_emu_pokes()
    if got != C1:
        log(f'  tornado nocatch: the staged capture ran at {got} (wanted {C1}): recipe off')
        return None
    log(f'  tornado nocatch: the first capture staged at {C1} (the video\'s {CATCH1_V})')

    # the full recipe: both catches, each pinned onto MegaMan's own replayed place, +0x14 south (inside the
    # game's own +-0x14 box), with the dissipate and the disarms around them
    pokes = first_catch + \
            [[C2 - 8, f'{fa4:#x}={(0xC4 & ~0x02) | 0x08:#04x}/1'],
             [C2 - 2, pins(second, pos2, C2 - 1)], [C2 - 1, pins(second, pos2, C2)],
             [C2 + 1, f'{fa4:#x}=0xC4/1']]

    # the guard: MegaMan's per-frame place through the second catch must be exactly the staged replay's
    pos3 = mm_track(pokes, C2 + 1)
    for g in sorted(pos3):
        if g <= C2 and g in pos2 and pos3[g] != pos2[g]:
            log(f'  tornado nocatch: MegaMan diverged from the staged replay at {g} (emulator not deterministic)')
            return None

    # the validation: the run holds past the second catch, the storm region left out of the comparison
    ck = sync.ck_file(w0)
    ck['mask'] = MASK_RECT
    r = sync.evaluate([(ck, pokes, D + HOLD, 'TN')])['TN']
    log(f'  tornado nocatch: holds to {r[0]} (mean cost {r[1]:.3f} masked)')
    if r[0] < C2 + 8:
        log('  tornado nocatch: the staged captures do not hold past the comp end')
        return None
    log(f'  tornado nocatch: take both staged captures at {C1} and {C2} (mean cost {r[1]:.3f})')
    sync.LENIENT.append((D - 10, D + HOLD + 240))
    return w0, pokes


