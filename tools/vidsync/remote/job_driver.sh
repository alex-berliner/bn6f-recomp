#!/bin/bash
# job_driver.sh RUN START [REPLAY]: the sync driver (supervise.sh -> autosync.py) as data/auto/RUN.0, .1, ...
# from checkpoint START (a fix_/ckpt_ path without .json). Launched through launch.ps1 (WMI), so it outlives
# the SSH session that started it. The run's name goes to ~/vidsync/runbase (status.py's default), its start
# to data/auto/RUN.start, its process group to ~/vidsync/driver.pgid (stop.sh driver). REPLAY: a result or
# loss json whose path is stepped as it is while the picture holds (VIDSYNC_REPLAY, autosync.py).
[ "$(ps -o pgid= $$ | tr -d ' ')" = "$$" ] || exec setsid bash "$0" "$@"
. ~/vidsync/recomp/tools/vidsync/remote/env.sh
echo $$ > ~/vidsync/driver.pgid
echo "$1" > ~/vidsync/runbase; echo "$2" > "data/auto/$1.start"
if [ -n "${3:-}" ]; then export VIDSYNC_REPLAY=$3; fi
exec bash "$V/remote/supervise.sh" "data/auto/$1" "$2"
