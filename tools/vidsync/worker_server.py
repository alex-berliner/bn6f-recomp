"""worker_server.py VDIR PORT [WORKERS]: rngfix candidates for a driver on another machine, over TCP.
Newline JSON: {"op": "put", "name", "state"(base64)} stores a checkpoint's machine; {"op": "task", "id", "ck",
"extra", "until"} runs one (ck['state'] names a put) -> {"id", "reached"}; "quick", and the driver's judges
"path" and "grid" (rngfix.KINDS: the message is the task), the same way."""
import sys, os, json, socket, threading, base64
sys.path.insert(0, os.path.dirname(__file__))
from multiprocessing import Pool
import rngfix
from emu import gamepaths
vdir, port = sys.argv[1], int(sys.argv[2]); w = int(sys.argv[3]) if len(sys.argv) > 3 else 15
store = os.path.join(gamepaths.VIDSYNC_DATA, 'wstore'); os.makedirs(store, exist_ok=True)
pool = Pool(w, initializer=rngfix._init, initargs=(vdir,), maxtasksperchild=100)   # (a long-lived worker drifts)
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); srv.bind(('0.0.0.0', port)); srv.listen(4)
print('listening', port, flush=True)
def serve(conn, addr):
    print('driver', addr, flush=True)
    conn.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    for o, v in (('TCP_KEEPIDLE', 60), ('TCP_KEEPINTVL', 20), ('TCP_KEEPCNT', 3)):
        if hasattr(socket, o): conn.setsockopt(socket.IPPROTO_TCP, getattr(socket, o), v)
    f = conn.makefile('rwb'); lock = threading.Lock()
    def reply(r):               # bound to this connection: tasks of a gone driver reply into the void
        with lock:
            try: f.write((json.dumps({'id': r[0], 'reached': r[1]}) + '\n').encode()); f.flush()   # (a quick score, for 'quick')
            except OSError: pass
    def failed(tid, res):       # a task that raised replies as failed (else the driver waits on it: run13g 1793272)
        def cb(ex):
            print('task error', tid, repr(ex), flush=True); reply((tid, res))
        return cb
    try:
        for line in f:
            m = json.loads(line)
            if m['op'] == 'put':
                open(os.path.join(store, m['name']), 'wb').write(base64.b64decode(m['state']))
            elif m['op'] == 'task':
                ck = dict(m['ck']); ck['state'] = os.path.join(store, ck['state'])
                pool.apply_async(rngfix._task, ((ck, m['extra'], m['until'], m['id']),), callback=reply,
                                 error_callback=failed(m['id'], [ck.get('g', 0), 9.0]))
            elif m['op'] == 'quick':
                ck = dict(m['ck']); ck['state'] = os.path.join(store, ck['state'])
                pool.apply_async(rngfix._qtask, ((ck, m['extra'], m['keys'], m['vids'], m['at'], m['id'], m.get('opts', {})),), callback=reply,
                                 error_callback=failed(m['id'], 1e9))
            elif m['op'] in rngfix.KINDS:       # a driver judge ('path', 'grid'): the message is the task
                m['ck'] = dict(m['ck'], state=os.path.join(store, m['ck']['state']))
                bad = [1e9] * len(m.get('cands', [])) if m['op'] == 'grid' else [m['ck'].get('g', 0), 6.0]
                pool.apply_async(rngfix.KINDS[m['op']], (m,), callback=reply, error_callback=failed(m['id'], bad))
    except (OSError, ValueError) as ex: print('driver error', addr, repr(ex), flush=True)
    print('driver gone', addr, flush=True)
while True:                     # a thread per driver: a half-dead old line never blocks a new driver
    conn, addr = srv.accept()
    threading.Thread(target=serve, args=(conn, addr), daemon=True).start()
