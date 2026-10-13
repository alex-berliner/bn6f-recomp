#!/bin/bash
# supervise.sh OUTBASE START: run autosync; after a crash go on from the newest checkpoint (fix_* / ckpt_*) in
# a new directory OUTBASE.N; stop when it is stuck (needs a person) or done, or after 5 crashes in a row.
cd ~/vidsync; export VIDSYNC_ROM=$PWD/data/bn6f.gba VIDSYNC_BIOS=$PWD/data/gba_bios.bin VIDSYNC_THUMBS=$PWD/data/full/thumb.u8
base=$1; start=$2; n=0; crashes=0
while true; do
  out=$base.$n
  python3 -u scripts/autosync.py data/full $start $out --remote 192.168.1.167:7777 --workers 13 > $out.out 2>&1 < /dev/null
  if grep -qE "done:|stuck near" $out/auto.log 2>/dev/null; then echo "$(date +%T) $out finished: $(tail -1 $out/auto.log)" >> $base.sup; break; fi
  crashes=$((crashes + 1)); echo "$(date +%T) $out crashed ($crashes): $(grep -m1 -A3 Traceback $out.out | tail -1)" >> $base.sup
  [ $crashes -ge 5 ] && break
  start=$(python3 - "$out" "$start" <<'PY'
import sys, glob, json
best = (json.load(open(sys.argv[2] + '.json'))['g'], sys.argv[2])
for f in glob.glob(sys.argv[1] + '/fix_*.json') + glob.glob(sys.argv[1] + '/ckpt_*.json'):
    g = json.load(open(f))['g']
    if g > best[0]: best = (g, f[:-5])
print(best[1])
PY
)
  n=$((n + 1))
done
