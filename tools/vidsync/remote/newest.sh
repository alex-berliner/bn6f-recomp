#!/bin/bash
# newest.sh RUNDIR [MAXG]: the checkpoint (fix_/ckpt_) with the largest game frame (at most MAXG)
cd ~/vidsync/data/auto/$1 && python3 - "$2" <<'PY'
import json, glob, sys
mx = int(sys.argv[1]) if sys.argv[1] else 1 << 30
best = max((json.load(open(f))['g'], f[:-5]) for f in glob.glob('fix_*.json') + glob.glob('ckpt_*.json') if json.load(open(f))['g'] <= mx)
print(best[0], best[1])
PY
