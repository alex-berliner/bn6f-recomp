#!/bin/bash
# the furthest game frame of the newest run's logs
cd ~/vidsync/data/auto; d=$(ls -d run7k.*/ 2>/dev/null | sed "s:/$::" | sort -t. -k2 -n | tail -1)
echo "procs $(pgrep -fc scripts/autosync.py) max $(grep -ohE "(near )?game [0-9]+" run7k.*/auto.log | grep -oE "[0-9]+" | sort -n | tail -1)"
