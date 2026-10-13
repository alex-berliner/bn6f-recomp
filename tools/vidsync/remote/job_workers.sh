#!/bin/bash
# job_workers.sh [VDIR]: the worker server (15 workers, TCP 7777) the driver sends candidate syncs to, on the
# video directory VDIR (default data/full). Launched through launch.ps1 (WMI); its process group goes to
# ~/vidsync/workers.pgid (stop.sh workers).
[ "$(ps -o pgid= $$ | tr -d ' ')" = "$$" ] || exec setsid bash "$0" "$@"
. ~/vidsync/recomp/tools/vidsync/remote/env.sh
echo $$ > ~/vidsync/workers.pgid
vd=${1:-data/full}
export VIDSYNC_THUMBS=$PWD/$vd/thumb.u8
if [ -f "$vd/thumb.base" ]; then export VIDSYNC_THUMB_BASE=$(cat "$vd/thumb.base"); fi
exec python3 -u "$V/worker_server.py" "$vd" 7777 15 > data/ws.log 2>&1 < /dev/null
