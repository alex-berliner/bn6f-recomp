"""pt_join.py TABLE.jsonl PRESS.jsonl SEC_GOOD.json OUT.txt [KMAX] [N]: (p, k) pairs whose primary at the R press is a
table entry with the video's encounter frame and whose secondary at the battle deals the video's hand; up to N of
them, spread over the primary at the battle (the battle's enemies take it) and then the secondary."""
import sys, json, collections
from pk_search import BASE_P, BASE_K, ENC
tab, prs, sec, out = sys.argv[1:5]
KMAX = int(sys.argv[5]) if len(sys.argv) > 5 else 6000
N = int(sys.argv[6]) if len(sys.argv) > 6 else 240
T = {}
for l in open(tab):
    x, f, b, white, pw = json.loads(l)
    if white is not None and abs(white - ENC) <= 1: T[x, f] = pw
good = set(json.load(open(sec)))
# secondary steps from the R press to where sec_search took the hand (pk_search's S0: 54 frames after 10900)
S_BATTLE = 19571 - 17454 + 108
by_pw = collections.defaultdict(list)
for l in open(prs):
    p, c, s, rate, g = json.loads(l)
    if c is None or s is None or rate != 2.0: continue
    for k in range(KMAX + 1):
        pw = T.get((c + k, (p - BASE_P + k - BASE_K) & 3))
        if pw is not None and s + 2 * k + S_BATTLE in good:
            by_pw[pw].append((p, k, s + 2 * k + S_BATTLE))
print(len(T), 'table entries pass;', sum(map(len, by_pw.values())), 'pairs;', len(by_pw), 'primaries at the battle')
pick = []
lists = [sorted(v, key=lambda t: t[0] + t[1]) for v in by_pw.values()]
while len(pick) < N and any(lists):
    for v in lists:
        seen = {t[2] for t in pick}
        while v and v[0][2] in seen: v.pop(0)
        if v and len(pick) < N: pick.append(v.pop(0))
open(out, 'w').write(''.join(f'{p} {k}\n' for p, k, _ in pick))
print(len(pick), 'picked ->', out)
