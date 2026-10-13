#!/bin/bash
# stop.sh WHAT...: stop the driver (`driver`: supervise.sh, autosync.py and its emulators), the worker server
# (`workers`), or a job by name (job.sh NAME), by process group: other jobs on the PC (an export, a segment
# cut) go on. Also stops a driver or worker server started before the move into recomp (~/vidsync/scripts).
for what in "$@"; do
  case $what in
    driver|workers) f=$HOME/vidsync/$what.pgid ;;
    *) f=$HOME/vidsync/jobs/$what.pgid ;;
  esac
  if [ -f "$f" ]; then kill -TERM -- -"$(cat "$f")" 2>/dev/null; rm -f "$f"; fi
  case $what in
    driver) pkill -f "bash .*supervise.sh"; pkill -f "autosync.py" ;;
    workers) pkill -f "worker_server.py" ;;
  esac
done
sleep 2
for what in "$@"; do
  case $what in
    driver) pkill -9 -f "autosync.py"; echo "driver: $(pgrep -fc autosync.py) left" ;;
    workers) pkill -9 -f "worker_server.py"; echo "workers: $(pgrep -fc worker_server.py) left" ;;
    *) echo "$what stopped" ;;
  esac
done
