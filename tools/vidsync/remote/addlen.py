"""addlen.py SRC.json DST.json...: add SRC's overlay windows to each DST (union)."""
import sys, json
src = [tuple(x) for x in json.load(open(sys.argv[1])).get('lenient', [])]
for p in sys.argv[2:]:
    d = json.load(open(p)); have = [tuple(x) for x in d.get('lenient', [])]
    d['lenient'] = sorted(set(have) | set(src)); json.dump(d, open(p, 'w')); print(p, d['lenient'])
