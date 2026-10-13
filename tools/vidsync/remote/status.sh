#!/bin/bash
# one-screen driver status: the newest run (base in RUNBASE), its furthest frame, the loss it is on and for how long
cd ~/vidsync/data/auto; base=$(cat /mnt/c/Users/alex/runbase 2>/dev/null); d=$(ls -d $base.*/ 2>/dev/null | sed "s:/$::" | sort -t. -k2 -n | tail -1)
echo "$(date +%T) run $d procs $(pgrep -fc scripts/autosync.py) max $(grep -ohE "(near )?game [0-9]+" $base.*/auto.log | grep -oE "[0-9]+" | sort -n | tail -1) fixes $(grep -chE "^.{9}fix [0-9]" $base.*/auto.log | paste -sd+ | bc)"
cat $base.sup 2>/dev/null
l=$(grep -E "lost the video|^.{9}fix [0-9]|done:|stuck|internal error" $d/auto.log | tail -1)
echo "last event: ${l:0:170}"
if echo "$l" | grep -q "lost the video"; then t=$(echo "$l" | cut -c1-8); echo "on this loss for $(( ( $(date +%s) - $(date -d $t +%s) + 86400 ) % 86400 / 60 )) min"; fi
tail -1 $d/auto.log | cut -c1-170
