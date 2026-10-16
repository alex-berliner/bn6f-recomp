"""tapfix.py LOSS_JSON OUT_JSON TAPLEN: the loss's keys with the video's own schedule from 530714 (Up TAPLEN frames,
B, Up x4 at +5, B, A x4 at +10, B) and a linear video mapping from there."""
import sys, json
r = json.load(open(sys.argv[1])); g0 = r['press_gf']; n = int(sys.argv[3])
G, V = 530714, 540384
B, UB, AB = 0x3fd, 0x3bd, 0x3fc
sched = {}
for i in range(60): sched[G + i] = B
for i in range(n): sched[G + i] = UB
for i in range(5, 9): sched[G + i] = UB
for i in range(10, 14): sched[G + i] = AB
keys, vid = r['keys'][:G - g0], r['vid'][:G - g0]
for g in range(G, G + 60):
    keys.append(sched[g]); vid.append(V + (g - G))
r['keys'], r['vid'] = keys, vid
json.dump(r, open(sys.argv[2], 'w'))
