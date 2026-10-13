"""addfree.py A B FILE...: add the free window (A, B) to each json."""
import sys, json
a, z = int(sys.argv[1]), int(sys.argv[2])
for p in sys.argv[3:]:
    d = json.load(open(p)); fr = [tuple(x) for x in d.get('free', [])]
    if (a, z) not in fr: fr.append((a, z))
    d['free'] = sorted(fr); json.dump(d, open(p, 'w'))
print(len(sys.argv) - 3, 'files')
