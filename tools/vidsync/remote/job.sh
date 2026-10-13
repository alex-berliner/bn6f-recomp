#!/bin/bash
# job.sh NAME SCRIPT [ARGS]: run remote/SCRIPT as a job of its own (launched through launch.ps1, so it outlives
# the SSH session): its output goes to ~/vidsync/jobs/NAME.log, its exit code to NAME.done when it ends, its
# process group to NAME.pgid (stop.sh NAME). The workflows poll NAME.done.
[ "$(ps -o pgid= $$ | tr -d ' ')" = "$$" ] || exec setsid bash "$0" "$@"
name=$1; shift; jobs=$HOME/vidsync/jobs; mkdir -p "$jobs"
rm -f "$jobs/$name.done"; echo $$ > "$jobs/$name.pgid"
bash ~/vidsync/recomp/tools/vidsync/remote/"$@" > "$jobs/$name.log" 2>&1 < /dev/null
echo $? > "$jobs/$name.done"
