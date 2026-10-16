"""autosync.py VDIR START OUTDIR [--remote HOST:PORT] [--workers N]: sync the whole video, fixing random events
with RNG writes.

The sync (beam.py) runs from checkpoint START, keeping the best path's machine every RING_EVERY game frames.
When it loses the video at game frame D, the event that went another way lies somewhere before D: the frames
where the best path's RNGs move other than at their usual rate (an encounter check, a battle's setup, a draw)
are the random events; for write points W one frame before each (latest first; then fixed distances), try the primary and the secondary RNG moved j steps along their
streams (j = +-1, +-2, ...), each written at W (as the route harness's .poke lines apply), and score how far
the sync then gets. The first wave with candidates that hold the video to D + MARGIN wins (the one that goes
furthest, then the smallest |j|): its write joins the run's pokes, and the sync goes on from W.
OUTDIR gets the log, checkpoints (ckpt_G.json/.state, resumable with beam.py --resume) and, at the end,
result.json (keys, video mapping, pokes)."""
import sys, os, hashlib, json, time, base64, socket, threading, argparse, shutil
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from multiprocessing import Pool
import beam as bm
import rngfix as rf
from emu import gamepaths
import syms   # game addresses by name from bn6f

RING_EVERY, RING_LEN = 30, 400
DISTS = (40, 120, 300, 600, 1200, 2400)
JMAX, WAVE, MARGIN, PARTIAL = 2000, 120, 400, 150

ap = argparse.ArgumentParser()
ap.add_argument('vdir'); ap.add_argument('start'); ap.add_argument('out')
ap.add_argument('--remote'); ap.add_argument('--workers', type=int, default=14)
ap.add_argument('--every', type=int, default=5000)
ap.add_argument('--until', type=int, help='stop once the sync reaches this game frame (a handler test)')
ap.add_argument('--handlers', help='only these repair handlers, comma-separated (built-in names or handlers/ '
                'modules), and no RNG search: a handler test from a loss checkpoint (vidsync-repair)')
A = ap.parse_args()
ONLY = set(A.handlers.split(',')) if A.handlers else None
os.makedirs(A.out, exist_ok=True)
LOG = open(os.path.join(A.out, 'auto.log'), 'a')
def log(*a):
    s = time.strftime('%H:%M:%S ') + ' '.join(str(x) for x in a)
    print(s, flush=True); LOG.write(s + '\n'); LOG.flush()

def tid_of(t): return t[3] if len(t) == 4 else t[5]

class Remote:
    """The worker server on the other machine."""
    IDLE = 600                 # seconds without a reply: the link (WireGuard) is dead even if TCP says not
    def __init__(self, hp):
        self.hp = hp; self.left = []; self.down = 0; self.connect()
    def connect(self):
        h, p = self.hp.split(':'); self.s = socket.create_connection((h, int(p)), timeout=30)
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        for o, v in (('TCP_KEEPIDLE', 60), ('TCP_KEEPINTVL', 20), ('TCP_KEEPCNT', 3)):
            if hasattr(socket, o): self.s.setsockopt(socket.IPPROTO_TCP, getattr(socket, o), v)
        self.s.settimeout(self.IDLE)
        self.f = self.s.makefile('rwb'); self.sent = set(); self.down = 0
    def ok(self):
        """Up, or reconnected (tried at most every 5 minutes after a failure)."""
        if not self.down: return True
        if time.time() - self.down < 300: return False
        try: self.connect(); log('remote workers back'); return True
        except OSError as ex: self.down = time.time(); log('remote workers still gone:', ex); return False
    names = {}                 # local path -> the name its current contents have on the server
    def put(self, name, path):
        """A state file under a name that carries its contents' hash: ck_file rewrites w_G.state whenever a
        later path reaches G, and the server must not keep running tasks on the first one it got."""
        data = open(path, 'rb').read()
        name = f'{name}.{hashlib.sha1(data).hexdigest()[:16]}'; Remote.names[path] = name
        if name in self.sent: return
        self.f.write((json.dumps({'op': 'put', 'name': name, 'state': base64.b64encode(data).decode()}) + '\n').encode())
        self.f.flush(); self.sent.add(name)
    batch = 0
    def run(self, tasks, out):
        # ids carry a batch number: replies to tasks of an earlier driver (or an abandoned batch) still
        # queued on the server are skipped
        Remote.batch += 1; tag = f'{os.getpid()}.{Remote.batch}:'
        want = {}; self.left = []
        try: self._run(tasks, out, tag, want)
        except (OSError, ValueError) as ex:          # a timeout is an OSError; a cut line, bad JSON
            log('remote workers lost:', repr(ex), sum(tid_of(t) not in out for t in tasks), 'tasks run here')
            self.left = [t for t in tasks if tid_of(t) not in out]; self.down = time.time()
            try: self.s.close()
            except OSError: pass
    def _run(self, tasks, out, tag, want):
        for t in tasks:
            c = dict(t[0]); c['state'] = Remote.names[t[0]['state']]
            if len(t) == 4:
                m = {'op': 'task', 'id': tag + t[3], 'ck': c, 'extra': t[1], 'until': t[2]}
            else:
                m = {'op': 'quick', 'id': tag + t[5], 'ck': c, 'extra': t[1], 'keys': t[2], 'vids': t[3], 'at': t[4],
                     'opts': t[6] if len(t) > 6 else {}}
            want[m['id']] = t
            self.f.write((json.dumps(m) + '\n').encode())
        self.f.flush()
        while want:
            line = self.f.readline()
            if not line: raise OSError('closed')
            r = json.loads(line)
            if r['id'] in want:
                del want[r['id']]; out[r['id'][len(tag):]] = r['reached']

pool = Pool(A.workers, initializer=rf._init, initargs=(A.vdir,), maxtasksperchild=100)   # (a long-lived worker drifts: fresh ones every 100 tasks)
remote = None
if A.remote:
    try: remote = Remote(A.remote); log('remote workers at', A.remote)
    except OSError as ex: log('no remote workers:', ex)

def evaluate(tasks):
    """tasks: [(ck, extra, until, id)] (a sync: -> game frame reached) or [(ck, extra, keys, vids, at, id)] (the
    quick pass: -> picture error) -> {id: result}; split between this machine and the remote."""
    out = {}
    fn = rf._task if len(tasks[0]) == 4 else rf._qtask
    if remote and len(tasks) > 1 and remote.ok():
        k = len(tasks) // 2
        try:
            for path in {t[0]['state'] for t in tasks[k:]}: remote.put(os.path.basename(path), path)
        except OSError as ex: remote.down = time.time(); log('remote workers lost:', repr(ex)); return evaluate(tasks)
        th = threading.Thread(target=remote.run, args=(tasks[k:], out)); th.start()
        for tid, r in pool.imap_unordered(fn, tasks[:k]): out[tid] = r
        th.join()
        for tid, r in pool.imap_unordered(fn, remote.left): out[tid] = r
    else:
        for tid, r in pool.imap_unordered(fn, tasks): out[tid] = r
    return out

b = bm.Beam(A.vdir, log=log)
per = bm.resume(b, A.start)
log('resumed at game', b.g, 'video', b.branches[0].node.v, 'pokes', len(b.pokes))
LENIENT_START = [tuple(x) for x in json.load(open(A.start + '.json')).get('lenient', [])]
HINTS = {tuple(p) for p in b.pokes if p[0] >= b.g}     # a resumed path's fixes from a later run, kept as hints
if HINTS: log('hints: later fixes', len(HINTS))
# a later run's path (VIDSYNC_REPLAY: a result/loss json): its keys and video frames are stepped as they are
# while the picture holds (a reward written differently changes the pack, not what the keys must be); the sync
# takes over at the first loss
REPLAY = None
if os.environ.get('VIDSYNC_REPLAY'):
    _r = json.load(open(os.environ['VIDSYNC_REPLAY']))
    if _r['press_gf'] == b.press_gf: REPLAY = (_r['keys'], _r['vid']); log('replaying a path to game', b.press_gf + len(_r['keys']))
def replay_on():
    return REPLAY is not None and b.g - b.press_gf < len(REPLAY[0]) - 1
def replay(n):
    """Up to n game frames of the replayed path on the best branch -> b.run's records."""
    global REPLAY
    for br in b.branches[1:]: b.free.append(br.slot)
    b.branches = b.branches[:1]; br = b.branches[0]
    b.e.load(br.slot); out = []
    for _ in range(n):
        i = b.g - b.press_gf
        if i >= len(REPLAY[0]) - 1: break
        k, v = REPLAY[0][i], REPLAY[1][i]
        b.e.step(k); b.g += 1
        c = min(b.e.errs(v - 3, 7))
        br.node = bm.Node(k, v, br.node)
        out.append((b.g, c, 1, v))
        if c > rf.LOSS: br.bad += 1
        else: br.bad = 0
        if br.bad >= 30: break
    b.e.save(br.slot); br.h = b.e.gf()[1]
    b.v0 = br.node.v - bm.RATE * br.h; br.cost = np.array([0.5 * abs(o) for o in bm.OFFS])
    return out
recent_fv = []             # the last frames' (game frame, cost, video frame), for the loss log
reward_watch = [0, None, None]   # (game frame of the last look, GIVEN then, game frame before a new reward)
ring = []                  # (g, v0, {id(node): (slot, h, cost, node)}): every branch, so a repair starts from
RSLOT = 3000               # the ancestor of the path that wins later, not whichever branch led at the time
ring_free = list(range(RSLOT, RSLOT + 8 * RING_LEN + 64))
def snap():
    ent = {}
    for br in b.branches:
        slot = ring_free.pop(); b.e.load(br.slot); b.e.save(slot)
        ent[id(br.node)] = (slot, br.h, br.cost.copy(), br.node)
    ring.append((b.g, b.v0, ent))
    while len(ring) > RING_LEN:
        for slot, *_ in ring.pop(0)[2].values(): ring_free.append(slot)

def ancestor(g):
    """The ring entry (g, slot, h, v0, cost, node) of the best path at ring game frame g."""
    n = b.branches[0].node
    for _ in range(b.g - g):
        n = n.parent
    for rg, v0, ent in ring:
        if rg == g:
            e = ent.get(id(n))
            return None if e is None else (g, e[0], e[1], v0, e[2], e[3])
    return None

def ck_file(w):
    g, slot, h, v0, cost, node = w
    path = os.path.join(A.out, f'w_{g}.state')
    b.e.load(slot); b.e.savef(path)
    return {'state': path, 'g': g, 'h': h, 'v0': v0, 'cost': cost.tolist(), 'key': node.key, 'v': node.v,
            'pokes': [list(p) for p in b.pokes if p[0] < g]}

def dist(a, x, mx=400):
    for i in range(mx + 1):
        if a == x: return i
        a = rf.rng_step(a)
    return None

def path_keys():
    return [n for n in b.keys_of(b.branches[0])]

def events(D, back=2400):
    """[(f, uses P, uses S)]: frames f in (D - back, D] where an RNG moved other than its usual amount."""
    gs = [r[0] for r in ring if r[0] >= D - back]
    if not gs: gs = [ring[0][0]]
    w = ancestor(gs[0])
    if w is None: return []
    ks = path_keys(); p0 = b.press_gf
    b.e.load(w[1]); P, S = b.e.u32(rf.PRIMARY), b.e.u32(rf.SECONDARY); rows = []
    bp = b.e.u32(BATTLE_PTR); TRIGGERS.clear()
    for g in range(w[0], min(D, b.g)):
        b.e.step(ks[g - p0][0]); P2, S2 = b.e.u32(rf.PRIMARY), b.e.u32(rf.SECONDARY)
        rows.append((g + 1, dist(P, P2), dist(S, S2))); P, S = P2, S2
        bp2 = b.e.u32(BATTLE_PTR)
        if bp2 != bp and 0x08000000 <= bp2 < 0x0A000000: TRIGGERS.append((g + 1, bp2))
        bp = bp2
    if not rows: return []
    import collections
    mp = collections.Counter(r[1] for r in rows).most_common(1)[0][0]
    ms = collections.Counter(r[2] for r in rows).most_common(1)[0][0]
    BIG[:] = [f for f, dp, ds in rows if dp is None or ds is None or (dp or 0) > mp + 3 or (ds or 0) > ms + 6]
    return [(f, dp != mp, ds != ms) for f, dp, ds in rows if dp != mp or ds != ms]

BATTLE_PTR = syms.GS_BATTLE_PTR     # GameState.CurBattleDataPtr: an encounter's battle settings, set on the trigger frame
TRIGGERS = []               # (game frame, pointer) where the best path set it, from events()
EVENTS = []                 # the last events() result
FORM_TOP = 3
BIG = []                     # event frames where an RNG jumped far (a battle's setup, the folder shuffle)
RECENT = 600                 # frames before a loss whose events get the first narrow pass
PARTIAL_EARLY = 150          # a partial fix this far past the loss is taken after a narrow stage
OFFSET_COST = 2.5          # the mean cost an offset jump must sync at
PARTIALS = []                # (reached, w, pokes) of partial fixes found during a repair

ENC_ROOTS = tuple(syms.ENC_ROOTS + 8 * k for k in range(4))   # pickBattleSettingsEntry_80AA5F4 picks one by flags
def battle_table(p):
    """The current map's random-encounter entries (0x10 bytes each, ended by 0xFF), the list holding p, found as
    pickBattleSettingsEntry_80AA5F4 does: root[group >= 0x80][group & 0x7F][map] under each root.
    (Walking back from p runs into other data, and writing that over the pointer crashes the game.)"""
    grp, mp = b.e.read(syms.GS_MAP, 2)
    rom = lambda a: 0x08000000 <= a < 0x0A000000
    for r in ENC_ROOTS:
        t = b.e.u32(r + 4 * (grp >= 0x80))
        if not rom(t): continue
        t = b.e.u32(t + 4 * (grp & 0x7F))
        if not rom(t): continue
        q = b.e.u32(t + 4 * mp); out = []
        while rom(q) and b.e.read(q, 1)[0] != 0xFF and len(out) < 64: out.append(q); q += 0x10
        if p in out: return out
    # not a random encounter of this map (a battle from battleSettingsList0, by a cutscene or map object, with
    # an index chosen elsewhere): the entries around p in its table with the same settings bytes 4..7
    head = b.e.read(p + 4, 4); out = [p]
    for d in (-0x10, 0x10):
        q = p + d
        for _ in range(16):
            if b.e.read(q + 4, 4) != head or not rom(b.e.u32(q + 12)): break
            out.append(q); q += d
    return sorted(out)

def try_formations(f, p, D):
    """An encounter triggered at f with battle settings p: the formation is iCurrFrame % (eligible entries),
    which a different walk or a slightly different trigger frame changes. Write each entry of the area's table
    over the pointer right after the trigger (the battle reads it ~66 frames later) and sync each over the
    battle's start. The battle's hand and enemy moves come from the RNGs and may be wrong too, so an entry that
    does not hold to the loss is still taken when it clearly syncs best there; the next losses fix the rest."""
    w = state_at(f)
    if w is None: return None
    ck = ck_file(w); until = D + MARGIN
    ents = battle_table(p)
    if len(ents) < 2: return None
    # the sync itself over the battle's start (intro, custom screen): a quick score on the path's video mapping
    # favours the path's entry (a different enemy count moves the custom screen by ~30 frames, which the path's
    # mapping absorbed for its own entry)
    end = min(D, f + 400)
    res = evaluate([(ck, [[f, rf.spec(BATTLE_PTR, q)]], end, f'B{q:x}') for q in ents])
    full = [q for q in ents if res[f'B{q:x}'][0] >= end]
    if not full: return None
    order = sorted(full, key=lambda q: res[f'B{q:x}'][1])
    m = lambda q: res[f'B{q:x}'][1]
    log(f'  formation at {f}: entry {order[0]:x} syncs best to {end} (mean cost {m(order[0]):.3f}'
        + (f', next {m(order[1]):.3f}' if len(order) > 1 else '')
        + (f'; the path has {p:x}: {m(p):.3f})' if p in full else f'; the path has {p:x}: lost)'))
    q = order[0]
    if q == p or (p in full and m(q) > 0.85 * m(p)): return None
    t = (ck, [[f, rf.spec(BATTLE_PTR, q)]], until, f'B{q:x}')
    r = evaluate([t])[t[3]]
    if r[0] >= until: log(f'  formation at {f}: B{q:x} holds to {until}')
    else: log(f'  formation at {f}: B{q:x} gets to {r[0]}; take it (its hand and moves come next)')
    return w, [t[1][0]]

def try_offset(D, back=40, span=90):
    """Not random: the video runs ahead or behind by a fixed amount from here (a save or load took BizHawk a
    different number of frames than ours). Re-anchor the video mapping: v0 shifted by d for |d| <= span from a
    state a little before the loss; one that holds wins."""
    w = state_at(max(D - back, ring[0][0]))
    if w is None: return None
    ck = ck_file(w); until = D + MARGIN
    tasks = []
    for d in range(-span, span + 1):
        if abs(d) <= 3: continue
        c = dict(ck); c['v0'] = ck['v0'] + d
        tasks.append((c, [], until, f'O{d}'))
    res = evaluate(tasks)
    # a real shift matches well once re-anchored (a frozen video stretch also 'holds' at a cost near the loss)
    ok = [t for t in tasks if res[t[3]][0] >= until and res[t[3]][1] < OFFSET_COST]
    if not ok:
        log(f'  video offset: no jump within {span} holds (furthest {max(r[0] for r in res.values())})'); return None
    t = min(ok, key=lambda t: res[t[3]][1]); d = int(t[3][1:])
    log(f'  video offset: a jump of {d} frames at {w[0]} holds to {until} (mean cost {res[t[3]][1]:.3f})')
    return (w[0], w[1], w[2], w[3] + d, w[4], w[5]), []

OK_RGB = np.array([247., 135., 114.])   # the custom screen's OK button, mean colour over [115:125, 92:110]
FOLDER = syms.BATTLE_FOLDER          # the battle's shuffled folder: 30 u16 chips (id | code << 9); the hand is the first 5
VIDEO = os.path.join(gamepaths.VIDSYNC_DATA, 'full.mp4')
SMALL = os.path.join(os.path.dirname(VIDEO), 'small.mp4')   # the game screen at 240x160 (job_small.sh): fast
def _vsrc():
    """(video file, filter before the frame select's output) - the small copy when it is there."""
    if os.path.exists(SMALL): return SMALL, ''
    return VIDEO, ',crop=906:604:37:56,scale=240:160:flags=area'

def video_frame(v):
    """Video frame v's game screen at the GBA's size (RGB), from the full video."""
    import subprocess
    pre = max(v - 30, 0)
    src, flt = _vsrc()
    cmd = ['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', src, '-vf',
           f"select=eq(n\\,{v - pre}){flt}", '-frames:v', '1',
           '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']
    b = subprocess.run(cmd, capture_output=True).stdout
    return np.frombuffer(b, np.uint8).reshape(160, 240, 3).astype(np.float32) if len(b) == 160 * 240 * 3 else None

def video_frames(v0, v1):
    """Video frames v0..v1 (game screens at the GBA's size, RGB float), one decode."""
    import subprocess
    pre = max(v0 - 30, 0)
    src, flt = _vsrc()
    cmd = ['ffmpeg', '-v', 'error', '-ss', f'{pre / 60:.4f}', '-i', src, '-vf',
           f"select=between(n\\,{v0 - pre}\\,{v1 - pre}){flt}", '-frames:v', str(v1 - v0 + 1),
           '-vsync', '0', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-']
    raw = subprocess.run(cmd, capture_output=True).stdout
    n = len(raw) // (160 * 240 * 3)
    a = np.frombuffer(raw[:n * 160 * 240 * 3], np.uint8).reshape(n, 160, 240, 3)
    return {v0 + i: a[i].astype(np.float32) for i in range(n)}

def slot_box(i): return (slice(103, 124), slice(4 + 16 * i, 20 + 16 * i))

PACK = syms.CHIP_PACK             # chip pack: a count byte per chip id (12 bytes) and code index (the chip's 4 codes)
CHIPDATA = syms.CHIP_DATA         # ChipData, 44 bytes per chip id: its codes first
def pack_chips():
    """The chips in the pack as folder entries (id | code << 9)."""
    raw = b.e.read(PACK, 12 * 0x140); cd = b.e.read(CHIPDATA, 44 * 0x140); out = set()
    for cid in range(1, 0x140):
        for k in range(4):
            n, code = raw[cid * 12 + k], cd[44 * cid + k]
            if n and n != 0xFF and code != 0xFF: out.add(cid | code << 9)
    return out

def folder_spec(order, pad=0):
    w = list(order) + [pad] * (30 - len(order))
    return ','.join(f'0x{FOLDER + 4 * k:x}=0x{w[2 * k] | (w[2 * k + 1] << 16):x}/4' for k in range(15))

KEEP_NEXT = [False]           # the fix repair returns keeps the pokes after it (they stay valid)
GMD_GROUPS = syms.MYSTERY_DATA_GROUPS      # InternetMysteryDataMapGroupEntries: {map group, maps}, ended by 1
EVENT_FLAGS = syms.EVENT_FLAGS      # eEventFlags: flag n at byte n >> 3, mask 0x80 >> (n & 7)
GMD_BASE = syms.MYSTERY_DATA         # u16 per mystery data id (from 0x1400): two picked indexes, rolled at jack-in
MAP_AT = syms.GS_MAP           # GameState MapGroup, MapNumber
def gmd_records():
    """The current map's green mystery data: [(id, n1, n2)], n1/n2 the sizes of the two tables the jack-in roll
    picks the record's two bytes from (mysteryData_processMapGroups_809F9DC, sub_809FA68)."""
    grp, mp = b.e.read(MAP_AT, 2)
    rom = lambda a: 0x08000000 <= a < 0x0A000000
    def count(t):
        n = 0
        while rom(t) and n < 32 and b.e.read(t + 8 * n, 1)[0] != 0: n += 1
        return n
    a, maps = GMD_GROUPS, None
    for _ in range(64):
        gid = b.e.u32(a)
        if gid == 1: break
        if gid == grp: maps = b.e.u32(a + 4); break
        a += 8
    if maps is None or not rom(maps): return []
    for i in range(mp + 1):
        if not rom(b.e.u32(maps + 4 * i)): return []
    q, out = b.e.u32(maps + 4 * mp), []
    for _ in range(32):
        t = b.e.read(q, 1)[0]
        if t == 0: break
        if t == 5:
            rid = int.from_bytes(b.e.read(q + 2, 2), 'little')
            out.append((rid, count(b.e.u32(q + 4)), count(b.e.u32(q + 8))))
        q += 12
    return out

def path_reach(w, extra, until, D):
    """The path's own keys from ring-style entry w with extra pokes (the run's are in): the game frame where
    the picture leaves the video (30 frames above the loss cost), or until. From the loss D on the path's video
    mapping is the sync losing it: the video goes on at its rate from D, keys read from it. A candidate judged
    this way differs from the path only by its write."""
    p0 = b.press_gf; ks = path_keys()[:D - p0 + 1]
    lv = ks[-1][1]; ks = list(ks) + [(b.key(lv + int(round(k * bm.RATE))), lv + int(round(k * bm.RATE))) for k in range(1, until - D + 2)]
    set_emu_pokes(extra); b.e.load(w[1]); bad = 0; tot = 0.0; n = 0
    for g in range(w[0], until):
        mask_for(g)
        b.e.step(ks[g - p0][0]); c = min(b.e.errs(ks[g - p0][1] - 3, 7)); tot += c; n += 1
        bad = bad + 1 if c > rf.LOSS else 0
        if bad >= 30: set_emu_pokes(); return g - 30, tot / n
    set_emu_pokes(); return until, tot / max(n, 1)

GS_PLAYER = syms.GS_PLAYER_XYZ          # GameState.PlayerX/Y/Z: where the map (re)load puts MegaMan (after a battle too)
OW_PLAYER_PTR = syms.GS_OW_PLAYER_PTR      # GameState.OverworldPlayerObjectPtr: live coords at +0x1C/+0x20/+0x24 (16.16)
def try_position(D):
    """Back on the map after a battle, MegaMan stands elsewhere than in the video: the video's encounter came at
    another point of his walk. Write the place the map reload uses (GameState.PlayerX/Y) during the black screen
    before the loss: each point of the walk before the encounter, and that walk's direction carried on."""
    w = state_at(max(D - 2000, ring[0][0]))
    if w is None: return None
    ks = path_keys(); p0 = b.press_gf
    set_emu_pokes(); b.e.load(w[1]); trail = []; dark = None
    for g in range(w[0], D):
        b.e.step(ks[g - p0][0])
        po = b.e.u32(OW_PLAYER_PTR)
        inb = b.e.read(syms.GS_SUBSYSTEM, 1)[0] == 0x0C         # GameState.SubsystemIndex: 0x0C in battle, 0x04 on the map
        if 0x02000000 <= po < 0x02040000 and not inb:
            xy = (b.e.u32(po + 0x1C), b.e.u32(po + 0x20))
            if not trail or trail[-1][1] != xy: trail.append((g, xy))
        if inb: dark = g
    if dark is None or dark < D - 1500 or len(trail) < 3: return None
    walk = [xy for g, xy in trail if g <= dark][-60:]
    if len(walk) < 3: return None
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
    (x1, y1), (x0, y0) = walk[-1], walk[-2]
    dx, dy = s32(x1) - s32(x0), s32(y1) - s32(y0)
    cands = list(dict.fromkeys(walk + [((s32(x1) + k * dx) & 0xFFFFFFFF, (s32(y1) + k * dy) & 0xFFFFFFFF) for k in range(1, 40)]))
    W = dark - 40                                    # still in the battle: before the map reload reads it
    w2 = state_at(W)
    if w2 is None: return None
    until = D + 200; best = None
    for x, y in cands:
        r = path_reach(w2, [[W, f'{GS_PLAYER:#x}={x:#x}/4,{GS_PLAYER + 4:#x}={y:#x}/4']], until, D)
        if best is None or (r[0], -r[1]) > (best[0][0], -best[0][1]): best = (r, x, y)
    log(f'  position: {len(cands)} places tried; best ({best[1]:#x}, {best[2]:#x}) reaches {best[0][0]} (mean cost {best[0][1]:.3f})')
    if best[0][0] >= D + 150:
        KEEP_NEXT[0] = True
        return w2, [[W, f'{GS_PLAYER:#x}={best[1]:#x}/4,{GS_PLAYER + 4:#x}={best[2]:#x}/4']]
    return None

def try_relocate(D):
    """On the map MegaMan stands elsewhere than in the video (an earlier bump or route that the picture could not
    tell apart on a repeating map, a return from battle): find the place whose picture matches the video at the
    loss (MegaMan's coordinates and walk target written, 16 px steps over +-1024 px, then 2 px around the best)
    and write it there; the path's keys from there must hold."""
    w = state_at(D)
    if w is None: return None
    b.e.load(w[1])
    if b.e.read(syms.GS_SUBSYSTEM, 1)[0] != 4: return None                 # on the map only
    po = b.e.u32(OW_PLAYER_PTR)
    if not 0x02000000 <= po < 0x02040000: return None
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
    x0, y0 = s32(b.e.u32(po + 0x1C)), s32(b.e.u32(po + 0x20))
    ks = path_keys(); p0 = b.press_gf; vT = ks[D - p0][1]
    def spec(dx, dy):
        x, y = (x0 + (dx << 16)) & 0xFFFFFFFF, (y0 + (dy << 16)) & 0xFFFFFFFF
        return ','.join(f'{po + o:#x}={v:#x}/4' for o, v in ((0x1C, x), (0x20, y), (0x28, x), (0x2C, y)))
    def err(dx, dy):
        b.e.load(w[1])
        x, y = (x0 + (dx << 16)) & 0xFFFFFFFF, (y0 + (dy << 16)) & 0xFFFFFFFF
        for o, v in ((0x1C, x), (0x20, y), (0x28, x), (0x2C, y)): b.e.cmd(f'write32 {po + o:x} {v:x}')
        for k in range(3): b.e.step(ks[D + k - p0][0] if D + k - p0 < len(ks) else 0x3FF)
        return min(b.e.errs(vT + int(round(3 * bm.RATE)) - 2, 5))     # (the path's mapping after D is the loss)
    set_emu_pokes()
    e0 = err(0, 0)
    best = min((err(dx, dy), dx, dy) for dx in range(-1024, 1025, 16) for dy in range(-1024, 1025, 16))
    best = min([best] + [(err(best[1] + a, best[2] + c), best[1] + a, best[2] + c) for a in range(-14, 15, 2) for c in range(-14, 15, 2)])
    log(f'  relocate: MegaMan +({best[1]}, {best[2]}) px matches the video at {D} ({best[0]:.2f}; where he is {e0:.2f})')
    if best[0] > rf.LOSS or (best[1], best[2]) == (0, 0): return None
    poke = [D, spec(best[1], best[2])]
    r = path_reach(w, [poke], D + 400, D)
    log(f'  relocate: the path from there reaches {r[0]} (mean cost {r[1]:.3f})')
    if r[0] >= D + 150: KEEP_NEXT[0] = True; return w, [poke]
    return None

MAPOBJS, MAPOBJ_SIZE = syms.OW_MAP_OBJECTS, syms.OW_MAP_OBJECT_SIZE     # eOverworldMapObjects (56): X +0xC, Y +0x10 (s16.16)
def try_mapobj(D):
    """A roaming map object (the Weather comp's tornado) goes another way than the video's: for each map object
    that moved in the last 60 frames, the place (16 px steps over +-512 px, then 2 px around the best) whose
    picture matches the video a little before the loss, written there; the path's keys from there must hold."""
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
    def objs():
        out = {}
        for n in range(56):
            a = MAPOBJS + n * MAPOBJ_SIZE
            if b.e.read(a, 1)[0] & 1: out[n] = (s32(b.e.u32(a + 0xC)), s32(b.e.u32(a + 0x10)))
        return out
    w0 = state_at(max(D - 60, ring[0][0]))
    if w0 is None: return None
    b.e.load(w0[1])
    if b.e.read(syms.GS_SUBSYSTEM, 1)[0] != 4: return None
    before = objs()
    wD = state_at(D); b.e.load(wD[1]); after = objs()
    movers = [n for n in after if n in before and after[n] != before[n]]
    if not movers: return None
    ks = path_keys(); p0 = b.press_gf
    best_all = None
    for back in (90, 60):
        W = D - back
        w = state_at(W)
        if w is None: continue
        b.e.load(w[1]); now = objs()
        for n in [n for n in movers if n in now]:
            a = MAPOBJS + n * MAPOBJ_SIZE; x0, y0 = now[n]; vT = ks[W + 3 - p0][1]
            def err(dx, dy):
                b.e.load(w[1])
                b.e.cmd(f'write32 {a + 0xC:x} {(x0 + (dx << 16)) & 0xFFFFFFFF:x}')
                b.e.cmd(f'write32 {a + 0x10:x} {(y0 + (dy << 16)) & 0xFFFFFFFF:x}')
                for k in range(3): b.e.step(ks[W + k - p0][0])
                return min(b.e.errs(vT - 2, 5))
            set_emu_pokes()
            e0 = err(0, 0)
            best = min((err(dx, dy), dx, dy) for dx in range(-512, 513, 16) for dy in range(-512, 513, 16))
            best = min([best] + [(err(best[1] + i, best[2] + j), best[1] + i, best[2] + j) for i in range(-14, 15, 2) for j in range(-14, 15, 2)])
            log(f'  map object {n} at {W}: +({best[1]}, {best[2]}) px matches the video ({best[0]:.2f}; where it is {e0:.2f})')
            if (best[1], best[2]) != (0, 0) and best[0] < e0 - 0.05 and (best_all is None or best[0] < best_all[0]):
                spec = f'{a + 0xC:#x}={(x0 + (best[1] << 16)) & 0xFFFFFFFF:#x}/4,{a + 0x10:#x}={(y0 + (best[2] << 16)) & 0xFFFFFFFF:#x}/4'
                # its home/target as s16 copies elsewhere (the tornado's saved X/Y, eWeatherCompState +0x40 / +0x46):
                # moved with it
                b.e.load(w[1]); low = b.e.read(0x02000000, 0x2000); home = []
                for k in range(0, 0x2000 - 8, 2):
                    if int.from_bytes(low[k:k + 2], 'little', signed=True) == x0 >> 16:
                        for j in range(k + 2, k + 8, 2):
                            if int.from_bytes(low[j:j + 2], 'little', signed=True) == y0 >> 16 and y0 >> 16 not in (0, -1):
                                home.append((0x02000000 + k, 0x02000000 + j))
                hspec = ','.join(f'{hx:#x}={((x0 >> 16) + best[1]) & 0xFFFF:#x}/2,{hy:#x}={((y0 >> 16) + best[2]) & 0xFFFF:#x}/2' for hx, hy in home[:2])
                best_all = (best[0], W, [spec] + ([spec + ',' + hspec] if hspec else []))
    if best_all is None: return None
    _, W, specs = best_all
    best = None
    for sp in specs:
        w = state_at(W)                # (state_at keeps one slot)
        r = path_reach(w, [[W, sp]], D + 400, D)
        log(f'  map object: {sp[:90]}: the path from there reaches {r[0]} (mean cost {r[1]:.3f})')
        if best is None or r[0] > best[0][0]: best = (r, sp)
    if best[0][0] >= D + 150: return state_at(W), [[W, best[1]]]
    return None

def try_tornado(D):
    """The Weather comp's tornado (map object 0, id 0x2000100): it stands at a place of its own and, when its
    countdown (object +0x24/+0x25; eWeatherCompState keeps saved copies, read only on a map reload) runs out,
    drifts: ours stands elsewhere and on another phase than the video's. Its place matched to the video's
    picture 90 frames before the loss (as try_mapobj), then every countdown value with it; the sync picks."""
    W = D - 90
    w = state_at(max(W, ring[0][0]))
    if w is None: return None
    b.e.load(w[1]); W = w[0]
    if b.e.u32(MAPOBJS + 4) != 0x2000100 or not b.e.read(MAPOBJS, 1)[0] & 1: return None
    s32 = lambda v: v - (1 << 32) if v >= 1 << 31 else v
    x0, y0 = s32(b.e.u32(MAPOBJS + 0xC)), s32(b.e.u32(MAPOBJS + 0x10))
    ks = path_keys(); p0 = b.press_gf; vT = ks[W + 3 - p0][1]
    def err(dx, dy):
        b.e.load(w[1])
        b.e.cmd(f'write32 {MAPOBJS + 0xC:x} {(x0 + (dx << 16)) & 0xFFFFFFFF:x}')
        b.e.cmd(f'write32 {MAPOBJS + 0x10:x} {(y0 + (dy << 16)) & 0xFFFFFFFF:x}')
        for k in range(3): b.e.step(ks[W + k - p0][0])
        return min(b.e.errs(vT - 2, 5))
    set_emu_pokes()
    best = min((err(dx, dy), dx, dy) for dx in range(-512, 513, 16) for dy in range(-512, 513, 16))
    best = min([best] + [(err(best[1] + i, best[2] + j), best[1] + i, best[2] + j) for i in range(-14, 15, 2) for j in range(-14, 15, 2)])
    pos = f'{MAPOBJS + 0xC:#x}={(x0 + (best[1] << 16)) & 0xFFFFFFFF:#x}/4,{MAPOBJS + 0x10:#x}={(y0 + (best[2] << 16)) & 0xFFFFFFFF:#x}/4'
    log(f'  tornado: +({best[1]}, {best[2]}) px at {W} ({best[0]:.2f})')
    w = state_at(W); ck = ck_file(w)
    # its state (owMapObject_weatherTornado_80A8E74, state 4: owMapObject_weatherTornado_moveOrPause_80A8F48):
    # +9 mode (0 moving, 1 paused), +0x25 the pause left (of +0x27), +5 the direction (0..7,
    # owMapObject_weatherTornado_step_80A9038) and +0x24 the frames to the next random one (of +0x26)
    o = MAPOBJS
    tasks = [(ck, [[W, pos + f',{o + 9:#x}=0x1/1,{o + 0x25:#x}={t:#x}/1']], D + 300, f'P{t}') for t in range(1, 201, 3)]
    tasks += [(ck, [[W, pos + f',{o + 9:#x}=0x0/1,{o + 5:#x}={d:#x}/1,{o + 0x24:#x}={t:#x}/1']], D + 300, f'M{d}:{t}')
              for d in range(8) for t in range(1, 61, 2)]
    tasks.append((ck, [[W, pos]], D + 300, 'Tpos'))
    res = evaluate(tasks)
    ok = [t for t in tasks if res[t[3]][0] >= D + 300]
    top = max(tasks, key=lambda t: res[t[3]][0])
    log(f'  tornado: {len(ok)} of {len(tasks)} states hold to {D + 300} (furthest {top[3]} to {res[top[3]][0]})')
    if not ok and top[3].startswith('M'):
        # its place within a few px and the timer near the best (MegaMan's orb ring must close around it)
        d0, t0 = map(int, top[3][1:].split(':'))
        bx, by = x0 + (best[1] << 16), y0 + (best[2] << 16)
        tf = [(ck, [[W, f'{o + 0xC:#x}={(bx + (dx << 16)) & 0xFFFFFFFF:#x}/4,{o + 0x10:#x}={(by + (dy << 16)) & 0xFFFFFFFF:#x}/4,'
                        f'{o + 9:#x}=0x0/1,{o + 5:#x}={d0:#x}/1,{o + 0x24:#x}={t:#x}/1']], D + 300, f'F{dx}:{dy}:{t}')
              for dx in range(-12, 13, 4) for dy in range(-12, 13, 4) for t in range(max(1, t0 - 6), min(61, t0 + 7))]
        rf_ = evaluate(tf)
        okf = [t for t in tf if rf_[t[3]][0] >= D + 300]
        bf = max(tf, key=lambda t: (rf_[t[3]][0], -rf_[t[3]][1]))
        log(f'  tornado: fine: {len(okf)} of {len(tf)} hold; furthest {bf[3]} to {rf_[bf[3]][0]}')
        if okf:
            t = min(okf, key=lambda t: rf_[t[3]][1]); log(f'  tornado: take {t[3]} (mean cost {rf_[t[3]][1]:.3f})')
            return w, [t[1][0]]
        if rf_[bf[3]][0] > res[top[3]][0]:
            top = bf; res[bf[3]] = rf_[bf[3]]; top = (bf[0], bf[1], bf[2], 'M' + f'{d0}:{int(bf[3].split(":")[2])}')
            res[top[3]] = rf_[bf[3]]
        # caught: MegaMan's orb ring closed around it in the video (owMapObject_weatherTornado_caught_80A8FA0 tests
        # flag 0xBE9 + its index, owMapObject_weatherTornado_isCaught_80A91D4, set elsewhere when the ring closes):
        # with the best start, the flag set at each frame near
        # the loss
        fl = 0xBE9 + b.e.read(o + 4, 1)[0]; fa, fm = EVENT_FLAGS + (fl >> 3), 0x80 >> (fl & 7)
        b.e.load(w[1]); fb = b.e.read(fa, 1)[0] | fm
        tc = [(ck, list(top[1]) + [[g, f'{fa:#x}={fb:#04x}/1']], D + 300, f'C{g}') for g in range(D - 40, D + 21, 2)]
        rc = evaluate(tc)
        okc = [t for t in tc if rc[t[3]][0] >= D + 300]
        bc = max(tc, key=lambda t: (rc[t[3]][0], -rc[t[3]][1]))
        log(f'  tornado: caught: {len(okc)} of {len(tc)} hold; furthest {bc[3]} to {rc[bc[3]][0]}')
        if okc:
            t = min(okc, key=lambda t: rc[t[3]][1]); log(f'  tornado: take {t[3]} (mean cost {rc[t[3]][1]:.3f})')
            return w, t[1]
        # it hit MegaMan in the video (owMapObject_weatherTornado_checkHitPlayer_80A91A4: within 20 px sets flag
        # 0xBE6; he is put back at the ring's
        # start, the meter emptied): put on MegaMan at each frame near the loss, with and without the best start
        th_ = []
        for base in ([], list(top[1])):
            for g in range(D - 60, D + 1, 3):
                wg = state_at(g, base)
                if wg is None: continue
                b.e.load(wg[1]); pp = b.e.u32(OW_PLAYER_PTR)
                px, py = b.e.u32(pp + 0x1C), b.e.u32(pp + 0x20)
                th_.append((ck, base + [[g, f'{o + 0xC:#x}={px:#x}/4,{o + 0x10:#x}={py:#x}/4']], D + 300, f'H{len(base)}@{g}'))
        set_emu_pokes()
        rh = evaluate(th_)
        okh = [t for t in th_ if rh[t[3]][0] >= D + 300]
        bh = max(th_, key=lambda t: (rh[t[3]][0], -rh[t[3]][1]))
        log(f'  tornado: hit: {len(okh)} of {len(th_)} hold; furthest {bh[3]} to {rh[bh[3]][0]}')
        if okh:
            t = min(okh, key=lambda t: rh[t[3]][1]); log(f'  tornado: take {t[3]} (mean cost {rh[t[3]][1]:.3f})')
            return w, t[1]
        # placed again just before the capture (MegaMan's orb ring must close around it: a few px decide), on
        # top of the best start
        for W2 in (D - 30, D - 20, D - 12):
            w2 = state_at(W2, top[1])
            if w2 is None: continue
            b.e.load(w2[1]); xa, ya = s32(b.e.u32(o + 0xC)), s32(b.e.u32(o + 0x10)); vT2 = ks[W2 + 3 - p0][1]
            def err2(dx, dy):
                b.e.load(w2[1])
                b.e.cmd(f'write32 {o + 0xC:x} {(xa + (dx << 16)) & 0xFFFFFFFF:x}')
                b.e.cmd(f'write32 {o + 0x10:x} {(ya + (dy << 16)) & 0xFFFFFFFF:x}')
                for k in range(3): b.e.step(ks[W2 + k - p0][0])
                return min(b.e.errs(vT2 - 2, 5))
            set_emu_pokes(top[1])
            e2 = min((err2(dx, dy), dx, dy) for dx in range(-48, 49, 4) for dy in range(-48, 49, 4))
            set_emu_pokes()
            ckw = ck_file(state_at(W2, top[1]))
            tp = [(ckw, [[W2, f'{o + 0xC:#x}={(xa + ((e2[1] + i) << 16)) & 0xFFFFFFFF:#x}/4,{o + 0x10:#x}={(ya + ((e2[2] + j) << 16)) & 0xFFFFFFFF:#x}/4']],
                   D + 300, f'R{W2}:{e2[1] + i}:{e2[2] + j}') for i in range(-4, 5, 2) for j in range(-4, 5, 2)]
            rp = evaluate(tp)
            okp = [t for t in tp if rp[t[3]][0] >= D + 300]
            bp = max(tp, key=lambda t: (rp[t[3]][0], -rp[t[3]][1]))
            log(f'  tornado: again at {W2}: +({e2[1]}, {e2[2]}) px ({e2[0]:.2f}); {len(okp)} of {len(tp)} hold, furthest {bp[3]} to {rp[bp[3]][0]}')
            if okp:
                t = min(okp, key=lambda t: rp[t[3]][1]); log(f'  tornado: take {t[3]} (mean cost {rp[t[3]][1]:.3f})')
                return w, list(top[1]) + t[1]
        # the comp's flags (0xBE0..0xBFF: ring, capture, weather) set or cleared near the loss
        b.e.load(w[1]); tg = []
        for fl in range(0xBE0, 0xC00):
            fa, fm = EVENT_FLAGS + (fl >> 3), 0x80 >> (fl & 7); cur = b.e.read(fa, 1)[0]
            for g in (D - 24, D - 12, D - 4):
                tg.append((ck, list(top[1]) + [[g, f'{fa:#x}={cur ^ fm:#04x}/1']], D + 300, f'G{fl:x}{"-" if cur & fm else "+"}@{g}'))
        rg = evaluate(tg)
        okg = [t for t in tg if rg[t[3]][0] >= D + 300]
        bg = sorted(tg, key=lambda t: (-rg[t[3]][0], rg[t[3]][1]))[:3]
        log(f'  tornado: flags: {len(okg)} of {len(tg)} hold; furthest ' + ', '.join(f'{t[3]} to {rg[t[3]][0]}' for t in bg))
        if okg:
            t = min(okg, key=lambda t: rg[t[3]][1]); log(f'  tornado: take {t[3]} (mean cost {rg[t[3]][1]:.3f})')
            return w, t[1]
        # moving, it takes a random direction every +0x26 (60) frames
        # (owMapObject_weatherTornado_repickDirection_80A90E4, GetRNG): the best start,
        # then each next direction written just after it is drawn, the one that holds furthest each time
        d0, t0 = map(int, top[3][1:].split(':'))
        pokes = list(top[1]); reach = res[top[3]][0]
        for k in range(4):
            f = W + t0 + 60 * k + 1
            if f > D + 300: break
            tk = [(ck, pokes + [[f, f'{o + 5:#x}={d:#x}/1']], D + 300, f'D{k}:{d}') for d in range(8)]
            rk = evaluate(tk)
            bt = max(tk, key=lambda t: (rk[t[3]][0], -rk[t[3]][1]))
            log(f'  tornado: direction at {f}: {bt[3]} reaches {rk[bt[3]][0]} (mean cost {rk[bt[3]][1]:.3f})')
            if rk[bt[3]][0] < reach: break
            pokes = bt[1]; reach = rk[bt[3]][0]
            if reach >= D + 300:
                log(f'  tornado: take {[p[0] for p in pokes]}')
                return w, pokes
        return None
    if not ok: return None
    t = min(ok, key=lambda t: res[t[3]][1])
    log(f'  tornado: take {t[3]} (mean cost {res[t[3]][1]:.3f})')
    return w, [t[1][0]]

ENC_STEPS = syms.ENC_STEPS         # eStruct2001c04 Unk_12: walk counter; (>> 6, max 16) picks the encounter odds
def try_encounter(D):
    """The video's random battle starts where ours does not: our walk counter (an earlier route, a relocation)
    makes the odds too low for any RNG value. At each primary-RNG event of the last 64 frames: the counter at
    its top and the RNG moved by up to 100; the sync picks one whose battle starts as in the video."""
    w = state_at(D)
    if w is None: return None
    b.e.load(w[1])
    if b.e.read(syms.GS_SUBSYSTEM, 1)[0] != 4: return None
    rolls = [f for f, p, q in EVENTS if p and D - 64 <= f <= D + 2]
    if not rolls: return None
    tasks = []; ck = None
    for f in rolls[-3:]:
        wf = state_at(f - 2)
        if wf is None: continue
        ckf = ck_file(wf); b.e.load(wf[1]); P0 = b.e.u32(rf.PRIMARY)
        for j in range(-100, 101):
            spec = f'{ENC_STEPS:#x}=0x400/2,' + rf.spec(rf.PRIMARY, rf.rng_move(P0, j))
            tasks.append((ckf, [[f - 2, spec]], D + 200, f'E{f}:{j}'))
        ck = (ck or {}); ck[f] = wf
    if not tasks: return None
    res = evaluate(tasks)
    ok = [t for t in tasks if res[t[3]][0] >= D + 60]
    if not ok:
        log(f'  encounter: no write starts the battle (furthest {max(r[0] for r in res.values())})'); return None
    t = max(ok, key=lambda t: (res[t[3]][0], -res[t[3]][1]))
    f = int(t[3][1:].split(':')[0])
    log(f'  encounter: {len(ok)} start it; take {t[3]} (to {res[t[3]][0]}, mean cost {res[t[3]][1]:.3f})')
    return state_at(f - 2), [t[1][0]]          # (again: state_at's scratch slot held another write point)

LENIENT = list(LENIENT_START)  # [(from, to)]: game frames where a stream overlay covers part of the video
FREE = [tuple(x) for x in json.load(open(A.start + '.json')).get('free', [])]   # [(from, to)]: frames a kill
                               # plays with the video's keys up to the deletion, its battle not the video's
def save_ck(path):
    """bm.checkpoint, with the overlay windows (resume takes them back)"""
    bm.checkpoint(b, path, [])
    c = json.load(open(path + '.json')); c['lenient'] = LENIENT; c['free'] = FREE; json.dump(c, open(path + '.json', 'w'))
OVERLAY_MASK = [0, 52, 6, 114]    # thumbnail rows/columns a stream alert covers (its text runs to ~100 px down)
def lossmax(g):
    return rf.LOSS
def in_overlay(g):
    return any(a <= g < z for a, z in LENIENT)
MASK_ON = [False]
def mask_for(g, e=None):
    """the comparison mask as the overlay windows want it at game frame g (changed only when it changes)"""
    want = in_overlay(g)
    if want != MASK_ON[0] or e is not None:
        (e or b.e).cmd('mask ' + (' '.join(map(str, OVERLAY_MASK)) if want else '')); MASK_ON[0] = want
def try_overlay(D):
    """A stream alert over the video (top middle of the screen): the game under it matches. A sync from just
    before the loss with that area left out of the comparison for 600 frames: if it holds, the run leaves it
    out there."""
    w = state_at(max(D - 60, ring[0][0]))
    if w is None: return None
    ck = ck_file(w); ck['mask'] = OVERLAY_MASK
    r = evaluate([(ck, [], D + 600, 'OV')] * 2)['OV']
    log(f'  overlay: a sync without the top middle of the screen gets to {r[0]} (mean cost {r[1]:.3f})')
    if r[0] < D + 600 or r[1] > 1.6: return None   # (the mask hides most of a battle field: only a clean match counts)
    LENIENT.append((w[0], D + 900))   # (an alert can outlast the 600 frames tried)
    return w, []

def try_gmd(D):
    """Green mystery data the video has (or lacks) on this map: its two bytes come from the roll at jack-in.
    Write each pair of picks for each of the map's records just before the map was entered (its objects are
    placed at the map load) and sync to the loss and past it."""
    # (back past later writes too: a mystery data placement changes nothing they fixed, and the fix keeps them)
    gs = [r[0] for r in ring if r[0] <= D]
    if len(gs) < 2: return None
    w = ancestor(gs[-1])
    if w is None: return None
    b.e.load(w[1]); here = b.e.read(MAP_AT, 2); recs = gmd_records()
    if not recs: return None
    # the newest ring entry on another map, then frame by frame to the change
    prev = None
    for g in reversed(gs[:-1]):
        a = ancestor(g)
        if a is None: return None
        b.e.load(a[1])
        if b.e.read(MAP_AT, 2) != here: prev = g; break
    if prev is None: return None
    ks = path_keys(); p0 = b.press_gf
    log(f'  mystery data: map {here.hex()} at {gs[-1]}; last ring entry elsewhere {prev} (ring from {ring[0][0]})')
    set_emu_pokes(); b.e.load(ancestor(prev)[1]); fm = None
    for g in range(prev, prev + RING_EVERY + 1):
        if b.e.read(MAP_AT, 2) == here: fm = g; break
        b.e.step(ks[g - p0][0])
    if fm is None: return None
    w = state_at(fm - 1)
    if w is None: return None
    ck = ck_file(w); until = D + MARGIN
    later = [list(p) for p in b.pokes if p[0] > w[0]]     # the path's fixes after the map entry come along
    b.e.load(w[1])
    tasks = []
    for rid, n1, n2 in recs:
        addr = GMD_BASE + 2 * (rid - 0x1400)
        cur = int.from_bytes(b.e.read(addr, 2), 'little')
        for x in range(max(n1, 1)):
            for y in range(max(n2, 1)):
                if x | y << 8 == cur: continue
                tasks.append((ck, [[w[0], f'{addr:#x}={x | y << 8:#06x}/2']] + later, until, f'G{rid:x}:{x}:{y}'))
        # or not there at all (the video took it on an earlier visit): its event flag (the record id) set
        fa, fmask = EVENT_FLAGS + (rid >> 3), 0x80 >> (rid & 7)
        fb = b.e.read(fa, 1)[0]
        if not fb & fmask:
            tasks.append((ck, [[w[0], f'{fa:#x}={fb | fmask:#04x}/1']] + later, until, f'G{rid:x}:gone'))
    # both records placed elsewhere at once (positions are byte 1, one record per position; contents kept)
    if len(recs) == 2:
        (r1, _, p1n), (r2, _, p2n) = recs
        a1, a2 = GMD_BASE + 2 * (r1 - 0x1400), GMD_BASE + 2 * (r2 - 0x1400)
        c1, c2 = (int.from_bytes(b.e.read(a, 2), 'little') for a in (a1, a2))
        for q1 in range(p1n):
            for q2 in range(p2n):
                if q1 == q2 or (q1, q2) == (c1 >> 8, c2 >> 8) or q1 == c1 >> 8 or q2 == c2 >> 8: continue
                v1, v2 = (c1 & 0xFF) | q1 << 8, (c2 & 0xFF) | q2 << 8
                tasks.append((ck, [[w[0], f'{a1:#x}={v1:#06x}/2,{a2:#x}={v2:#06x}/2']] + later, until, f'G2:{q1}:{q2}'))
    if not tasks: return None
    log(f'  mystery data: map {here.hex()} entered at {fm}, records ' + ' '.join(f'{r:x}({a}x{c})' for r, a, c in recs)
        + f': {len(tasks)} writes')
    until = min(until, D + 200)
    res = {t[3]: path_reach(w, [t[1][0]], until, D) for t in tasks}
    ok = [t for t in tasks if res[t[3]][0] >= until]
    if ok:
        if len(ok) == len(tasks): log('  mystery data: every write holds (not the cause)'); return None
        t = min(ok, key=lambda t: res[t[3]][1])
        log(f'  mystery data: {len(ok)} hold to {until}; take {t[3]} (mean cost {res[t[3]][1]:.3f}) -> {t[1][0][1]}')
        KEEP_NEXT[0] = True; return w, [t[1][0]]
    t = max(tasks, key=lambda t: (res[t[3]][0], -res[t[3]][1]))
    log(f'  mystery data: furthest {t[3]} to {res[t[3]][0]}')
    if res[t[3]][0] >= D + PARTIAL: KEEP_NEXT[0] = True; return w, [t[1][0]]
    return None

GIVEN = syms.REWARD_GIVEN            # eStruct200A008 Unk04: the battle reward actually given (packed u16)
SHOWN_Q = syms.REWARD_SHOWN          # the result screen's display queue entry for it
BSTATE = syms.BATTLE_STATE           # BattleState: enemy lists at +0x4C + 8*side, counts at +8+side
REWARD_TABLES = syms.REWARD_TABLES    # 20 u16 rewards per enemy id (0x28 bytes)
PANEL = (slice(110, 130), slice(34, 126))  # the result screen's GET DATA name box (its picture follows the other reward too)
REWARD_SEEN = set()
REWARD_GAIN = 20              # summed name-box error a reward must remove to replace the game's
def reward_candidates(v0):
    """The rewards the battle's enemies can give, read from BattleState as the roll (sub_80AA8E0) does; the
    reading (side, u8/u16 entries) whose tables hold the reward the game gave."""
    for side in (1, 0):
        n = b.e.read(BSTATE + 8 + side, 1)[0]
        if not 0 < n <= 8: continue
        for size in (2, 1):
            raw = b.e.read(BSTATE + 0x4C + 8 * side, 8)
            ids = [int.from_bytes(raw[k * size:(k + 1) * size], 'little') for k in range(min(n, 8 // size))]
            out = set()
            for eid in ids:
                if not 0 < eid < 0x200: continue
                t = b.e.read(REWARD_TABLES + eid * 0x28, 0x28)
                out |= {int.from_bytes(t[k:k + 2], 'little') for k in range(0, 0x28, 2)}
            out.discard(0xFFFF)
            if v0 in out: REWARD_IDS[:] = ids; return sorted(out)
    return []

ENEMY_KIND = syms.ENEMY_KINDS       # 3 bytes per enemy id: version, actor type, AI index (GetVerActorTyAndAIIdx_80182B4)
HELD_TABLES = syms.HELD_TABLES      # [actor type][AI index] -> 0x20 bytes per version: the chips a virus can hold (sub_80AED50)
REWARD_IDS = []
BATTLE_MD = syms.BATTLE_MYSTERY_DATA        # battle-start mystery data contents: 8 u16 per row (setup byte 2 >> 4), RNG & 0xE
def battle_md_candidates(v):
    """Reward 2 can be the battle's mystery data (spawnMysteryData_8007424): the rows of its table holding v."""
    out = set()
    for row in range(8):
        raw = b.e.read(BATTLE_MD + 16 * row, 16)
        vals = {int.from_bytes(raw[k:k + 2], 'little') for k in range(0, 16, 2)}
        if v in vals: out |= vals
    return out
def held_candidates():
    """Reward 2 is the chip a deleted virus held, rolled at its spawn from its held-chip table."""
    out = set()
    for eid in REWARD_IDS:
        ver, at, ai = b.e.read(ENEMY_KIND + 3 * eid, 3)
        t = b.e.u32(HELD_TABLES + 4 * at)
        if not 0x08000000 <= t < 0x0A000000: continue
        t = b.e.u32(t + 4 * ai)
        if not 0x08000000 <= t < 0x0A000000: continue
        raw = b.e.read(t + 0x20 * ver, 0x20)
        out |= {int.from_bytes(raw[k:k + 2], 'little') for k in range(0, 0x20, 2)}
    out.discard(0xFFFF)
    return out

def check_reward(g_lo, D=None):
    """A battle's rewards were given on the best path after game frame g_lo: compare the result screen's GET
    DATA box with the full-size video where each of the (up to) two rewards shows; for each, another reward of
    the battle's tables that matches clearly better is written into the result record before the game copies
    it to the screen and to what it gives (REWARD_RECORD/+2, a frame or two before) -> a fix (w, [pokes]) or None."""
    gs = [r[0] for r in ring if r[0] <= g_lo]
    if not gs: return None
    w = ancestor(gs[-1])
    if w is None: return None
    ks = path_keys(); p0 = b.press_gf
    if D is not None: ks = ks[:D - p0 + 1]           # at a loss: the path's mapping after it is the sync losing it
    # past that (a loss right after the result screen): the video goes on at its rate, its keys
    lv = ks[-1][1]; ks = list(ks) + [(b.key(lv + int(round(k * bm.RATE))), lv + int(round(k * bm.RATE))) for k in range(1, 900)]
    set_emu_pokes(); b.e.load(w[1]); v_prev = b.e.read(GIVEN, 4); gc = None
    for g in range(w[0], b.g):
        b.e.step(ks[g - p0][0]); v = b.e.read(GIVEN, 4)
        if v != v_prev and int.from_bytes(v[:2], 'little') not in (0, 0xFFFF) and g + 1 not in REWARD_SEEN:
            gc = g + 1; break
        v_prev = v
    if gc is None: return None
    REWARD_SEEN.add(gc)
    r1, r2 = int.from_bytes(v[:2], 'little'), int.from_bytes(v[2:], 'little')
    cands = reward_candidates(r1)
    if len(cands) < 2: return None
    # the result screen's timing differs between battles (when its rewards show after the given value is set):
    # the name box every 5 frames over the next 400, each candidate judged by how much it lowers the error
    # against the video over the frames where it changes anything
    at = list(range(gc + 5, gc + 400, 5))
    vmap = {t: ks[t - 1 - p0][1] for t in at if t - 1 - p0 < len(ks)}
    if len(vmap) < len(at): return None
    vf_ = video_frames(min(vmap.values()), max(vmap.values()))
    if any(v not in vf_ for v in vmap.values()): return None
    w = state_at(gc - 3)
    if w is None: return None
    def pk(a, c):
        return [[gc - 3, f'{syms.REWARD_NAVI_STATS:#x}={a:#06x}/2,{syms.REWARD_NAVI_STATS + 2:#x}={c:#06x}/2'], [gc - 2, f'{syms.REWARD_RECORD:#x}={a:#06x}/2,{syms.REWARD_RECORD + 2:#x}={c:#06x}/2']]
    def errs(a, c):
        set_emu_pokes(pk(a, c)); b.e.load(w[1]); out = []; g = gc - 3
        for t in at:
            while g < t: b.e.step(ks[g - p0][0]); g += 1
            out.append(float(np.abs(b.e.shot().astype(np.float32)[PANEL] - vf_[vmap[t]][PANEL]).mean()))
        set_emu_pokes()
        return np.array(out)
    E0 = errs(r1, r2)
    gain = lambda E: float(np.sum(E0 - E))          # summed improvement over the frames
    b1 = max(((gain(errs(x, r2)), x) for x in cands if x != r1), default=(0.0, r1))
    a = b1[1] if b1[0] > REWARD_GAIN else r1
    c2 = sorted((set(cands) | held_candidates() | battle_md_candidates(r2) | {r2}) - {0xFFFF, r2}) if r2 not in (0, 0xFFFF) else []
    Ea = errs(a, r2) if a != r1 else E0
    b2 = max(((float(np.sum(Ea - errs(a, x))), x) for x in c2), default=(0.0, r2))
    c = b2[1] if b2[0] > REWARD_GAIN else r2
    log(f'  rewards at {gc}: {r1:#06x} {r2:#06x}; best {b1[1]:#06x} (gain {b1[0]:.0f}) {b2[1]:#06x} (gain {b2[0]:.0f})')
    if (a, c) == (r1, r2): return None
    return w, pk(a, c)

T1_OBJS, T1_SIZE = syms.T1_OBJECTS, 0xD8   # battle objects (navis, viruses): HP at +0x24, MaxHP +0x26, Alliance +0x16
PANEL_T = (slice(8, 76), slice(12, 108))  # the result panel in a thumbnail (80x120)
T1_ACTIVE = syms.T1_ACTIVE         # eActiveT1BattleObjectsBitfield: a bit per live battle object
def enemies_alive(side=1):
    """Live battle objects of an alliance: 1 the enemies, 0 MegaMan's side."""
    out = []; act = b.e.u32(T1_ACTIVE)
    for n in range(16):
        if not act >> (31 - n) & 1: continue     # object 0 is the top bit
        a = T1_OBJS + n * T1_SIZE
        hp, mx = int.from_bytes(b.e.read(a + 0x24, 2), 'little'), int.from_bytes(b.e.read(a + 0x26, 2), 'little')
        if mx and hp and b.e.read(a + 0x16, 1)[0] == side: out.append(a)
    return out

KILL_COST = 1.5
def try_kill(D, side=1):
    """A battle that went another way than the video's (its AI, hands, damage): end it when the video's does
    (side 1: our enemies deleted; side 0: MegaMan deleted, a game over the video has).
    Delete our enemies (HP 0) once with no keys to learn the result screen's look and how long it takes to show,
    find that screen in the video after the loss, and delete them that long before it; the sync confirms."""
    from feat import emu_thumb
    w = state_at(D)
    if w is None: return None
    b.e.load(w[1]); foes = enemies_alive(side)
    if not foes: return None
    # how far back this battle goes (up to 600 frames): the video's end is not looked for before it (an
    # earlier battle's result screen would undo what was fixed since)
    back = 0; wk = state_at(max(D - 600, ring[0][0]))
    if wk is not None:
        ks = path_keys(); set_emu_pokes()
        back = D - wk[0]
        for g in range(wk[0], D):                       # frame by frame: the map between two battles is short
            b.e.step(ks[g - b.press_gf][0])
            # (no live enemy: between two battles of a series, the game stays in battle mode)
            if b.e.read(syms.GS_SUBSYSTEM, 1)[0] != 0x0C or not enemies_alive(1): back = D - (g + 1)
    w = state_at(D); b.e.load(w[1])     # (state_at keeps one slot)
    log(f"  {'kill' if side else 'die'}: this battle goes back {back} frames (of 600 looked at)")
    kill = ','.join(f'{a + 0x24:#x}=0x0000/2' for a in foes)
    set_emu_pokes([[D, kill]]); b.e.load(w[1]); th = []
    for g in range(D, D + 420):
        b.e.step(0x3FF); th.append(emu_thumb(b.e.shot()))
    set_emu_pokes()
    tmpl = th[-1][PANEL_T]
    d = [float(np.abs(x[PANEL_T] - tmpl).mean()) for x in th]
    L = next((k for k, x in enumerate(d) if x < 12), None)
    vD = w[5].v if w[5] is not None else b.vbase(w[2])
    cands = []                  # (deletion frame, the video's end frame, how): each tried until one holds
    # (from up to 600 frames before the loss: the video's battle can end before the sync notices)
    if L is not None:
        dv = [float(np.abs(b.T[min(max(v, 0), b.N - 1)][PANEL_T].astype(np.float32) - tmpl).mean())
              for v in range(vD - back, vD + 4000)]
        # (the deletion inside this battle: the video can still show the last one's result screen early in it)
        vr = next((k - back for k, x in enumerate(dv) if x < 20 and int(round((k - back) / bm.RATE)) - L >= -back), None)
        if vr is None: log(f'  {"kill" if side else "die"}: no result screen in the video within -{back}..4000 frames of {vD} (best {min(dv):.1f})')
        else:
            gR = D + int(round(vr / bm.RATE)); cands.append((gR - L, gR, f'result screen {L} frames after'))
    # a battle that ends in black (the story takes over): our black comes Lb frames after the deletion; the
    # video's first black frame after the battle's start puts the deletion Lb before it
    Lb = next((k for k, x in enumerate(th) if float(x.mean()) < 10), None)
    if Lb is not None:
        # (a black frame after this battle's start by at least Lb: the black between two battles of a series
        # comes before it)
        vb = next((v for v in range(vD - back + int(Lb * bm.RATE), vD + 4000) if float(b.T[min(max(v, 0), b.N - 1)].mean()) < 10), None)
        if vb is not None:
            gb = D + int(round((vb - vD) / bm.RATE)); cands.insert(0, (gb - Lb, gb, f'black {Lb} frames after'))
    # the first 150 frames after the deletion ("YOU WIN" / "ENEMY DELETED", the fade) laid along the video: for
    # a battle with no result screen (the story ends it, its talk mashed through) or one the template misplaces
    seq = np.stack([th[k] for k in range(0, 150, 5)]).astype(np.float32)
    best = (1e9, None); cs = []
    for o in range(-back, 4000):
        vs = [min(max(vD + o + int(round(k * bm.RATE)), 0), b.N - 1) for k in range(0, 150, 5)]
        c = float(np.abs(b.T[vs].astype(np.float32) - seq).mean()); cs.append(c)
        if c < best[0]: best = (c, o)
    med = float(np.median(cs))
    # the earliest place that looks alike (a later battle's end can look the same, or more so): the first local
    # minimum below 0.6x the median
    first = next((k for k in range(1, len(cs) - 1) if cs[k] <= 0.6 * med and cs[k] <= cs[k - 1] and cs[k] <= cs[k + 1]), None)
    log(f'  {"kill" if side else "die"}: the 150 frames after a deletion look most like the video at {vD + best[1]} (cost {best[0]:.1f}, median {med:.1f})'
        + (f'; first alike at {vD + first - back} (cost {cs[first]:.1f})' if first is not None else ''))
    if first is not None:
        W0 = D + int(round((first - back) / bm.RATE)); cands.append((W0, W0 + 150, 'the deletion and fade'))
    for W0, gR, how in cands:
        if W0 < D - back:
            log(f'  {"kill" if side else "die"}: {how}: a deletion at {W0} is before this battle ({D - back})'); continue
        # the machine at W0: from the loss on with the video's keys (the battle is ours to end anyway)
        if W0 < D: w2 = state_at(max(W0, ring[0][0]))
        else:
            w = state_at(D); b.e.load(w[1]); set_emu_pokes(); node = w[5]
            for g in range(D, W0):          # (each frame on the path: its keys are the trace's)
                vg = vD + int(round((g - D) * bm.RATE)); k = b.key(vg); b.e.step(k); node = bm.Node(k, vg, node)
            gg, h = b.e.gf(); slot = RSLOT - 1; b.e.save(slot)
            w2 = (W0, slot, h, vD + int(round((W0 - D) * bm.RATE)) - bm.RATE * h, np.array([0.5 * abs(o) for o in bm.OFFS]), node)
        if w2 is None: continue
        b.e.load(w2[1]); foes2 = enemies_alive(side) or foes          # the enemies alive then (a virus dies later)
        kill = ','.join(f'{a + 0x24:#x}=0x0000/2' for a in foes2)
        log(f'  {"kill" if side else "die"}: {len(foes2)} enemies; {how}: the video has it at game ~{gR} -> delete at ~{W0}')
        ck = ck_file(w2); until = gR + 300
        tasks = [(ck, [[W0 + d, kill]], until, f'K{d}') for d in range(0, 13, 2)]
        res = evaluate(tasks)
        # (a close match only: a deletion that cuts short a battle the video goes on with scrapes by at 1.7-2.0;
        # the right ones were 0.7-1.2)
        ok = [t for t in tasks if res[t[3]][0] >= gR + 120 and res[t[3]][1] < KILL_COST]
        if not ok:
            log(f'  {"kill" if side else "die"}: no deletion time holds (furthest {max(r[0] for r in res.values())}, want {gR + 120}, best mean cost {min(r[1] for r in res.values()):.2f})'); continue
        t = max(ok, key=lambda t: (res[t[3]][0], -res[t[3]][1]))
        log(f'  {"kill" if side else "die"}: delete at {t[1][0][0]} holds to {res[t[3]][0]} (mean cost {res[t[3]][1]:.3f})')
        if W0 > D: FREE.append((D, W0))
        return w2, [t[1][0]]
    return None

# subchips with a movement counter (submenu_subchip_8124230 etc.): event flag byte/mask, counter address.
# When the flag is set and the counter is 0, npc_80350BC clears the flag and runs the "<item> just expired!"
# cutscene, which pauses the map (roaming objects too)
SUBCHIPS = {'SneakRun': (syms.SUBCHIP_FLAGS, 0x20, syms.SNEAKRUN_STEPS), 'LocEnemy': (syms.SUBCHIP_FLAGS, 0x10, syms.LOCENEMY_STEPS)}
def try_subchip(D):
    """The video's subchip ran out at another time than ours (or ours was not on): its expiry, forced on each
    frame around the loss (flag set, counter 0); the sync picks the frame."""
    w = state_at(max(D - 40, ring[0][0]))
    if w is None: return None
    b.e.load(w[1])
    if b.e.read(MAP_AT, 1)[0] < 0x80: return None           # subchips work on the internet only
    ck = ck_file(w); until = D + MARGIN; tasks = []
    for name, (fa, m, ca) in SUBCHIPS.items():
        fb = b.e.read(fa, 1)[0] | m
        for W in range(w[0] + 1, D + 6):
            tasks.append((ck, [[W, f'{fa:#x}={fb:#04x}/1,{ca:#x}=0x0/4']], until, f'{name}@{W}'))
    res = evaluate(tasks)
    ok = [t for t in tasks if res[t[3]][0] >= until]
    if not ok: return None
    t = min(ok, key=lambda t: res[t[3]][1])
    log(f'  subchip: {len(ok)} hold to {until}; take {t[3]} (mean cost {res[t[3]][1]:.3f})')
    return w, [t[1][0]]

def try_hand(D):
    """The first hand of a battle is the shuffled folder's first 5 chips, and one order of 5 out of 30 is too
    rare to reach with secondary RNG writes. Write the folder order instead: after the shuffle (a big jump of
    the secondary), for each hand slot in turn, put each of the folder's distinct chips there, render the custom
    screen and compare that slot (icon and code) with the full-size video frame."""
    last = max([p[0] for p in b.pokes] + [0])
    shuf = [f for f, p, q in EVENTS if q and f in set(BIG) and f > last]
    ks = path_keys(); p0 = b.press_gf
    is_ok = lambda im: np.abs(im[115:125, 92:110].mean(axis=(0, 1)) - OK_RGB).max() < 12
    c = None
    if shuf:
        f = shuf[-1]
        w = state_at(f)
        if w is None: return None
        # the custom screen: its OK button in place (the panel slides in some frames after the battle's start)
        b.e.load(w[1])
        for g in range(f, min(D + 30, b.g)):
            b.e.step(ks[g - p0][0])
            if is_ok(b.e.shot().astype(np.float32)): c = g + 1 + 3; break
    else:
        # a later turn's hand: the next chips of the (compacted) battle folder, drawn when the custom screen
        # opens; the latest custom screen before the loss, the folder written 20 frames before it opens
        g1 = max(last + 1, D - 1500, ring[0][0])
        w = state_at(g1)
        if w is None: return None
        b.e.load(w[1]); was = False; opens = []
        for g in range(g1, min(D + 30, b.g)):
            b.e.step(ks[g - p0][0]); on = is_ok(b.e.shot().astype(np.float32))
            if on and not was: opens.append(g + 1)
            was = on
        if not opens: return None
        c = opens[-1] + 3; f = opens[-1] - 20
        if f <= last: return None
        w = state_at(f)
        if w is None: return None
    if c is None or c > b.g: return None
    b.e.load(w[1]); folder = list(np.frombuffer(b.e.read(FOLDER, 60), np.uint16))
    pad = 0xFFFF if 0xFFFF in folder else 0          # a later turn: used chips are 0xFFFF
    folder = [x for x in folder if x not in (0, 0xFFFF)]
    if not folder: return None
    V = video_frame(ks[c - p0 - 1][1])
    if V is None: log('  hand: no video frame'); return None
    def render(order):
        set_emu_pokes([[f, folder_spec(order, pad)]])
        b.e.load(w[1])
        for g in range(f, c): b.e.step(ks[g - p0][0])
        set_emu_pokes()
        return b.e.shot().astype(np.float32)
    def err(E, i):
        y, x = slot_box(i); a, bb = E[y, x], V[y, x]
        return float(np.abs((a - a.mean()) - (bb - bb.mean())).mean())
    base = render(folder)
    e0 = [err(base, i) for i in range(5)]
    # the folder's chips, and the pack's: the video's folder can hold chips ours does not (a folder edit that
    # went another way); one from the pack takes the place of the last chip of the shuffled folder
    pack = sorted(pack_chips() - set(folder))
    chosen, rest = [], list(folder)
    for i in range(5):
        best = None
        for cand in sorted(set(rest)) + pack:
            r = list(rest)
            if cand in r: r.remove(cand)
            else: r = r[:-1]
            E = render(chosen + [cand] + r)
            e = err(E, i)
            if best is None or e < best[0]: best = (e, cand)
        chosen.append(best[1])
        if best[1] in rest: rest.remove(best[1])
        else: rest = rest[:-1]; pack.remove(best[1])
    order = chosen + rest
    E = render(order)
    e1 = [err(E, i) for i in range(5)]
    log(f'  hand at {f} (custom screen {c}): ' + ' '.join(f'{x:04x}' for x in folder[:5]) + ' -> ' + ' '.join(f'{x:04x}' for x in order[:5]) +
        f'; slot error {sum(e0):.1f} -> {sum(e1):.1f}')
    if order[:5] == folder[:5] or sum(e1) >= sum(e0): return None
    ck = ck_file(w); until = D + MARGIN
    poke = [f, folder_spec(order, pad)]
    res = evaluate([(ck, [poke], until, 'H')] * 2)['H']
    log(f'  hand: the sync gets to {res[0]} (mean cost {res[1]:.3f})')
    # a clearly better hand is right even when the battle still goes another way after it (the enemies' moves
    # come from the primary RNG): take it when it costs no frames; the next loss gets the RNG
    if res[0] >= D + PARTIAL or res[0] >= until or (sum(e1) < 0.9 * sum(e0) and res[0] >= D): return w, [poke]
    # a hand that clearly matches the video better (checked against it slot by slot) is right even if the
    # battle then goes another way sooner than before: the next loss gets the RNG
    if sum(e1) < 0.8 * sum(e0) and res[0] >= c + 30: return w, [poke]
    return None

def set_emu_pokes(extra=()):
    b.e.pokes_clear()
    for pg, ps in list(b.pokes) + list(extra): b.e.poke(pg, ps)

def state_at(g, extra=()):
    """A ring-style entry for the best path at game frame g (stepped from the ring entry before it), with the
    run's pokes and extra ones. The latest ring entry on the best path is used: one that holds another path's
    machine (a fix that played frames itself, past the ring entries it went on from) is passed over."""
    w = None
    for rg in reversed([r[0] for r in ring if r[0] <= g]):
        w = ancestor(rg)
        if w is not None: break
    if w is None: return None
    ks = path_keys(); p0 = b.press_gf
    set_emu_pokes(extra)
    b.e.load(w[1])
    for gg in range(w[0], g): b.e.step(ks[gg - p0][0])
    set_emu_pokes()
    gg, h = b.e.gf()
    n = b.branches[0].node
    for _ in range(b.g - g): n = n.parent
    slot = RSLOT - 1; b.e.save(slot)
    return (g, slot, h, w[3], np.array([0.5 * abs(o) for o in bm.OFFS]), n)

QUICK_TOP, QUICK_GAIN = 12, 0.85
def try_at(w, D, which, jmax, base=()):
    """RNG writes at w: (w, poke) for one that holds to D + MARGIN or gets PARTIAL past D, else None. Every
    candidate first gets the quick pass (the path's keys, the picture at a few frames where the path lost the
    video); only the best of each wave that beat no write get the sync."""
    ck = ck_file(w); ck['pokes'] = ck['pokes'] + [list(p) for p in base]
    b.e.load(w[1]); P0, S0 = b.e.u32(rf.PRIMARY), b.e.u32(rf.SECONDARY)
    until = D + MARGIN
    ks = path_keys()[w[0] - b.press_gf:b.g - b.press_gf]
    keys, vids = [k for k, v in ks], [v for k, v in ks]
    # past the loss the path's video mapping is stuck where it lost the video (a battle the video starts there
    # would score worse than walking on): the video goes on at its rate from the loss, keys read from it
    i0 = D - w[0]
    if 0 <= i0 < len(vids):
        vD = vids[i0]
        for i in range(i0 + 1, len(vids)):
            vids[i] = vD + int(round((i - i0) * bm.RATE)); keys[i] = b.key(vids[i])
    # every 3rd frame from W on: an event's effect can show well before the loss (a hand on the custom screen)
    # and a right write keeps the frames that matched matching
    at = list(range(1, len(keys), 3))
    if not at: return None
    q0 = evaluate([(ck, [], keys, vids, at, 'q0')] * 2)['q0']
    cands = []
    for j in range(1, jmax + 1):
        for sgn in (j, -j):
            if 'P' in which: cands.append(('P', sgn))
            if 'S' in which: cands.append(('S', sgn))
    top = (D, None); nsync = 0; res_cost = {}
    for i in range(0, len(cands), WAVE * 4):
        writes = {}
        for r, j in cands[i:i + WAVE * 4]:
            addr, x0 = (rf.PRIMARY, P0) if r == 'P' else (rf.SECONDARY, S0)
            writes[f'{r}{j}'] = [[w[0], rf.spec(addr, rf.rng_move(x0, j))]]
        qs = evaluate([(ck, ex, keys, vids, at, tid) for tid, ex in writes.items()])
        good = sorted((q, tid) for tid, q in qs.items())[:QUICK_TOP]
        tasks = [(ck, writes[tid], until, tid) for q, tid in good]; nsync += len(tasks)
        res = evaluate(tasks)
        ok = [t for t in tasks if res[t[3]][0] >= until]
        # the chosen one again, alone, on this machine: a fix the run then loses at once (the loop at 370434)
        while ok:
            t = min(ok, key=lambda t: res[t[3]][1])
            chk = pool.apply(rf._task, ((ck, t[1], until, 'chk'),))[1]
            if chk[0] >= until: break
            log(f'  W={w[0]}: {t[3]} held to {res[t[3]][0]} in the batch but reaches {chk[0]} again here: dropped')
            ok.remove(t)
        if ok:
            # several can hold for a while (an encounter a frame early picks another layout of the same
            # viruses, iCurrFrame % N): the one the video matches best
            log(f'  W={w[0]}: {len(ok)} hold to {until}; take {t[3]} (mean cost {res[t[3]][1]:.3f}) -> {t[1][0][1]} (synced {nsync} of {i + len(writes)})')
            return w, list(base) + [t[1][0]]
        for t in tasks:
            if res[t[3]][0] > top[0] or (top[1] is not None and res[t[3]][0] == top[0]
                                         and res[t[3]][1] < res_cost.get(top[1][3], 9.9)):
                top = (res[t[3]][0], t)
            res_cost[t[3]] = res[t[3]][1]
    if top[1] is not None and top[0] >= D + PARTIAL:
        # a partial fix is only a last resort (repair takes the furthest after every write point had its turn)
        # (w's slot is state_at's scratch slot, which every later write point overwrites: the state goes to
        # its own file)
        keep = os.path.join(A.out, f'partial_{w[0]}.state'); shutil.copyfile(ck['state'], keep)
        PARTIALS.append((top[0], w, list(base) + [top[1][1][0]], res_cost.get(top[1][3], 9.9), keep))
    log(f'  W={w[0]} ({which}, |j| <= {jmax}): furthest {top[0]}, synced {nsync} of {len(cands)}')
    return None

# repair handlers outside this file (handlers/NAME.py, handlers/__init__.py): tried after the built-in ones,
# before the RNG search
import importlib.util, glob as _glob
PLUGINS = []
for _path in sorted(_glob.glob(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'handlers', '[!_]*.py'))):
    _spec = importlib.util.spec_from_file_location('vidsync_handler_' + os.path.basename(_path)[:-3], _path)
    _mod = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_mod)
    def _guarded(D, _m=_mod):
        try:
            return _m.try_fix(D, sys.modules[__name__])
        except Exception as e:   # a broken handler must not stop the driver: log it, skip it
            log(f'  handler {_m.NAME} failed: {type(e).__name__}: {e}')
            return None
    PLUGINS.append((_mod.NAME, _guarded))
if PLUGINS: log('handlers from handlers/:', ' '.join(name for name, _ in PLUGINS))

LAST_FIX = [None, None]       # (loss, handler) of the last fix
FAILED = {}                   # loss -> handlers whose fix there came back to it
def repair(D):
    ks = b.keys_of(b.branches[0])
    json.dump({'press_gf': b.press_gf, 'status': f'lost near {D}', 'lenient': LENIENT, 'free': FREE, 'keys': [k for k, v in ks], 'vid': [v for k, v in ks],
               'pokes': b.pokes}, open(os.path.join(A.out, f'loss_{D}.json'), 'w'))
    ev = events(D); EVENTS[:] = ev
    last = max([p[0] for p in b.pokes] + [0])      # encounters before the last write held to there already
    for f, p in reversed(TRIGGERS):
        if f <= last or (ONLY is not None and 'formations' not in ONLY): continue
        r = try_formations(f, p, D)
        if r: return r
    r = None if ONLY is not None and 'reward' not in ONLY else check_reward(max(ring[0][0], D - 900), D)   # a reward given just before the loss (its screen differs)
    if r: KEEP_NEXT[0] = True; return r
    # a handler whose fix came back to the same loss is not asked there again (it would loop)
    if LAST_FIX[0] is not None and abs(LAST_FIX[0] - D) <= 30:
        FAILED.setdefault(LAST_FIX[0], set()).add(LAST_FIX[1])
        log(f'  the last fix ({LAST_FIX[1]}) came back to this loss: not asking it again here')
        if LAST_FIX[1] == 'overlay':    # its window goes too (the run would compare less than the tasks do)
            LENIENT[:] = [x for x in LENIENT if not x[0] <= D < x[1]]
    skip = set().union(*[v for d, v in FAILED.items() if abs(d - D) <= 30])
    for name, h in (('offset', try_offset), ('overlay', try_overlay), ('hand', try_hand), ('gmd', try_gmd),
                    ('subchip', try_subchip), ('position', try_position), ('relocate', try_relocate), ('tornado', try_tornado), ('mapobj', try_mapobj),
                    ('encounter', try_encounter), ('kill', try_kill), ('die', lambda D: try_kill(D, 0)), *PLUGINS):
        if name in skip or (ONLY is not None and name not in ONLY): continue
        if name == 'die' and 'kill' in skip: continue    # (ending the battle was not it: the video's MegaMan lives)
        r = h(D)
        if r: LAST_FIX[:] = [D, name]; return r
    if ONLY is not None and 'rng' not in ONLY:
        log(f'  no handler of {",".join(sorted(ONLY))} fixed it'); return None
    LAST_FIX[:] = [D, 'rng']
    log(f'  RNG events before the loss: {len(ev)}, latest ' + ' '.join(f'{f}{"P" if p else ""}{"S" if q else ""}' for f, p, q in ev[-12:]))
    PARTIALS.clear()
    # narrow first over every write point (the big jumps, a battle's setup and its folder shuffle, then latest
    # first), then widely: big jumps, then the rest
    big = [e for e in ev if e[0] in set(BIG)]
    small = [e for e in reversed(ev) if e[0] not in set(BIG)]
    log(f'  big RNG jumps: {" ".join(str(e[0]) for e in big[-8:])}')
    # the narrow pass over the events of the last RECENT frames (latest first) and the big jumps comes first; a partial fix
    # well past the loss found there is taken (the loss after it is the next event: an encounter avoided, then
    # the video's own encounter a little later), before hours over every older write point
    recent = [e for e in small if e[0] >= D - RECENT]; older = [e for e in small if e[0] < D - RECENT]
    stages = ((300, recent + list(reversed(big)), PARTIAL_EARLY), (300, older, PARTIAL_EARLY),
              (2000, list(reversed(big)), None), (2000, small, None))
    def take_partial(need):
        ok = [x for x in PARTIALS if x[0] >= D + need]
        if not ok: return None
        far, w, new, c, keep = max(ok, key=lambda x: (x[0], x[1][0], -x[3]))   # furthest, the latest write, the best cost
        b.e.loadf(keep); b.e.save(w[1])
        assert b.e.gf() == (w[0], w[2]), (b.e.gf(), w[0], w[2])
        log(f'  no write holds; the furthest partial: {new[-1][1]} at {w[0]} reaches {far} ({far - D} past the loss, mean cost {c:.3f})')
        return w, new
    for jmax, group, need in stages:
        for f, p, q in group:
            w = state_at(f - 2)          # a write at W goes in at the end of the step W -> W+1: before the step f-1 -> f that is the event
            if w is None: continue
            r = try_at(w, D, ('P' if p else '') + ('S' if q else ''), jmax)
            if r: return r
        if need is not None:
            r = take_partial(need)
            if r: return r
    r = take_partial(PARTIAL)
    if r: return r
    return repair_fixed(D)

def repair_fixed(D):
    end_v = b.N - bm.K - 2
    for d in DISTS:
        gs = [r[0] for r in ring if r[0] <= D - d]
        if not gs: break
        w = ancestor(gs[-1])
        if w is None: log(f'  no ancestor state at {gs[-1]}'); continue
        ck = ck_file(w)
        b.e.load(w[1]); P0, S0 = b.e.u32(rf.PRIMARY), b.e.u32(rf.SECONDARY)
        until = D + MARGIN
        base = evaluate([(ck, [], until, 'base')])['base'][0]
        log(f'  W={w[0]} (D-{D - w[0]}): no write reaches {base} (the sync lost it at {D})')
        cands = []
        for j in range(1, JMAX + 1):
            for s in (j, -j):
                cands.append(('P', s)); cands.append(('S', s))
        top = (base, None)            # the furthest partial so far
        for i in range(0, len(cands), WAVE):
            wave = cands[i:i + WAVE]
            tasks = []
            for n, (r, j) in enumerate(wave):
                addr, x0 = (rf.PRIMARY, P0) if r == 'P' else (rf.SECONDARY, S0)
                tasks.append((ck, [[w[0], rf.spec(addr, rf.rng_move(x0, j))]], until, f'{r}{j}'))
            res = evaluate(tasks)
            ok = [(res[t[3]][0], -res[t[3]][1], t) for t in tasks if res[t[3]][0] >= until]
            if ok:
                ok.sort(key=lambda o: (o[0], o[1]), reverse=True)
                best = ok[0][2]
                log(f'  W={w[0]}: {len(ok)} of {len(tasks)} hold to {until}; take {best[3]} -> {best[1][0][1]}')
                return w, [best[1][0]]
            for t in tasks:
                if res[t[3]][0] > top[0]: top = (res[t[3]][0], t)
            if i // WAVE % 10 == 9:
                log(f'  W={w[0]}: |j| <= {abs(int(wave[-1][1]))}, furthest {top[0]}')
        # nothing holds to the margin: the candidate that gets furthest, if that is well past the loss
        if top[1] is not None and top[0] >= D + PARTIAL:
            t = top[1]
            log(f'  W={w[0]}: {t[3]} gets furthest, to {top[0]} ({top[0] - D} past the loss); take it -> {t[1][0][1]}')
            return w, [t[1][0]]
    return None

ck_every = A.every
bad = 0; last_v = []; status = 'end of video'; t0 = time.time(); g0 = b.g; nfix = 0
while True:
    # a stretch of frames at once (b.run: in C while there is one branch); it ends where a frame below may
    # snapshot, log or checkpoint, and b.run stops after a frame above the desync cost, a stall or the video's end
    stop = min((b.g // RING_EVERY + 1) * RING_EVERY, (b.g // 500 + 1) * 500, (b.g // ck_every + 1) * ck_every)
    lost = end = stalled = False
    mask_for(b.g)
    stretch = replay(stop - b.g) if replay_on() else b.run(stop - b.g, lossmax(b.g), b.N - bm.K - 2, last_v, (60, 3))
    for g, best, nbr, v in stretch:
        per.append(best); recent_fv.append((g, round(best, 2), v)); del recent_fv[:-40]
        bad = bad + 1 if best > lossmax(g) else 0
        if g % RING_EVERY == 0: snap()
        if g % 500 == 0:
            log(f'game {g} video {v} branches {nbr} cost {best:.2f} {(g - g0) / (time.time() - t0):.0f} fps fixes {nfix}')
        if g % ck_every == 0: save_ck(os.path.join(A.out, f'ckpt_{g:07d}'))
        last_v.append(v); last_v = last_v[-60:]
        stalled = len(last_v) == 60 and last_v[-1] - last_v[0] < 3
        if bad >= 30 or stalled:
            lost = True; break
        if v >= b.N - bm.K - 2:
            end = True; break
        if A.until is not None and g >= A.until:
            end = True; status = f'reached game {g}'; break
    fix = None; keep_later = False     # a reward fix keeps the pokes after it (a resumed path's later fixes)
    if not lost and not end:
        # a battle reward given on the best path (the sync hardly sees the GET DATA box): checked once the
        # path is 200 frames past it
        b.e.load(b.branches[0].slot); gv = int.from_bytes(b.e.read(GIVEN, 2), 'little')
        if gv != reward_watch[1]:
            if gv not in (0, 0xFFFF): reward_watch[2] = reward_watch[0]
            reward_watch[1] = gv
        reward_watch[0] = b.g
        if reward_watch[2] is not None and b.g >= reward_watch[2] + 450:
            fix = check_reward(reward_watch[2]); reward_watch[2] = None
            if fix: log('reward fix'); keep_later = True
    if lost or fix:
        if not fix:
            D = b.g - (30 if bad >= 30 else 60)
            log(f'lost the video near game {D} (video {v}){" (stalled)" if stalled else ""}: looking for an RNG write')
            log('  last frames (game, cost, video): ' + ' '.join(f'{g_}:{c_}:{v_}' for g_, c_, v_ in recent_fv[::4]))
            KEEP_NEXT[0] = False; fix = repair(D); keep_later = keep_later or KEEP_NEXT[0]
            if fix is None:
                status = f'stuck near game {D} (video {v}): no RNG write found'
                log(status); break
        w, new = fix
        g, slot, h, v0, cost, node = w
        for br in b.branches: b.free.append(br.slot)
        s2 = b.free.pop(); b.e.load(slot); b.e.save(s2)
        if b.e.gf() != (g, h):      # a slot overwritten since the fix was found: the sync would start elsewhere
            status = f'internal error: the fix at game {g} (hw {h}) has a machine at {b.e.gf()}'
            log(status); break
        b.branches = [bm.Branch(s2, h, cost.copy(), node)]
        b.g, b.v0 = g, v0
        # pokes after the fix go, except a resumed path's later fixes (HINTS) well past the loss: their
        # writes are absolute values (an RNG seed, a pointer, a folder order) that hold again once the path joins
        far = g if keep_later else (D + 300 if lost else g)
        b.pokes = [p for p in b.pokes if p[0] < g or p in new or keep_later
                   or (tuple(p) in HINTS and p[0] > far)]
        for p in new:
            if list(p) not in b.pokes: b.pokes.append(list(p))
        b.pokes.sort(key=lambda p: p[0])
        set_emu_pokes()
        # ring entries past the fix's machine, and past the loss: a fix past the loss (try_kill, a handler that
        # plays a window itself) leaves the old path's entries in (D, g], which no longer lie on the best path
        cut = min(D, g) if lost else g
        while ring and ring[-1][0] > cut:
            for slot2, *_ in ring.pop()[2].values(): ring_free.append(slot2)
        if not keep_later and REPLAY is not None: REPLAY = None; log('replay off: the path left the replayed one')
        bad = 0; last_v = []; nfix += 1
        log(f'fix {nfix}: ' + '; '.join(f'write {p[1]} at game {p[0]}' for p in new) + '; going on from there')
        log(f'  from: machine {b.e.gf()}, branch h {h}, v0 {b.v0:.1f} (video {b.vbase(h)}), pokes from {g}: '
            + ' '.join(str(p[0]) for p in b.pokes if p[0] >= g))
        save_ck(os.path.join(A.out, f'fix_{nfix:04d}'))
        continue
    if end:
        break
ks = b.keys_of(b.branches[0])
json.dump({'press_gf': b.press_gf, 'status': status, 'lenient': LENIENT, 'free': FREE, 'keys': [k for k, v in ks], 'vid': [v for k, v in ks],
           'pokes': b.pokes}, open(os.path.join(A.out, 'result.json'), 'w'))
log('done:', status, 'fixes', nfix)
