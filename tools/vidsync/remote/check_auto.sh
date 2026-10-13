#!/bin/bash
cd ~/vidsync/data/auto; echo "procs $(pgrep -fc scripts/autosync.py)"; d=$(ls -d run7k.*/ 2>/dev/null | sed "s:/$::" | sort -t. -k2 -n | tail -1); echo "dir $d"; cat run7k.sup 2>/dev/null
tail -${1:-15} $d/auto.log 2>/dev/null; grep -m1 -A3 Traceback $d.out 2>/dev/null
