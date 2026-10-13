#!/usr/bin/env bash
# make_ready.sh [RUNDIR]: refresh VIDSYNC_READY (build-vidsync/ready, tools/paths.env) from the sync on
# 192.168.1.175: export the newest checkpoint of data/auto/RUNDIR (default: the run named in
# C:/Users/alex/runbase, .0) as a verified route-harness recording, cut it into save-file segments
# (segments.py, a checkpoint about every 20000 game frames), then copy it here. Uses the job scripts
# exportj.sh / segj.sh in C:/Users/alex on that machine (the compute is there).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
. "$ROOT/tools/paths.env"   # VIDSYNC, VIDSYNC_READY
PC=alex@192.168.1.175
RUN=${1:-$(ssh $PC "type C:\\Users\\alex\\runbase" | tr -d '\r').0}
ssh $PC "wsl -d Ubuntu -- bash /mnt/c/Users/alex/exportj.sh $RUN bn6f_beastsp" | tr -d '\r\0'
ssh $PC "wsl -d Ubuntu -- bash -c 'rm -rf ~/vidsync/data/ready/segments'; wsl -d Ubuntu -- bash /mnt/c/Users/alex/segj.sh data/ready/bn6f_beastsp.result.json data/ready/segments 20000" | tr -d '\r\0'
ssh $PC "wsl -d Ubuntu -- bash -c 'cd ~/vidsync/data/ready && tar czf /mnt/c/Users/alex/ready.tgz bn6f_beastsp.trace bn6f_beastsp.poke bn6f_beastsp.sav segments/beastsp_seg_* segments/segments.json segments/VERIFY.txt'"
scp -q "$PC:C:/Users/alex/ready.tgz" "$VIDSYNC/ready.tgz"
rm -rf "$VIDSYNC_READY/segments"; tar xzf "$VIDSYNC/ready.tgz" -C "$VIDSYNC_READY" && rm "$VIDSYNC/ready.tgz"
echo "ready/ refreshed from $RUN"
