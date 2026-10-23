#!/bin/bash
# py.sh SCRIPT [ARGS]: run tools/vidsync/SCRIPT (pathcheck.py, probes/fullcmp.py, ...) in the vidsync working
# directory with the PC's environment (env.sh). E.g. from the main machine:
#   echo 'bash ~/vidsync/recomp/tools/vidsync/remote/py.sh probes/trig.py RESULT STATE G' |
#     ssh alex@10.67.1.175 wsl -d Ubuntu -- bash -s
. ~/vidsync/recomp/tools/vidsync/remote/env.sh
script=$1; shift
exec python3 -u "$V/$script" "$@"
