#!/bin/bash
# segments.sh RESULT OUTDIR EVERY: cut a result into save-file segments (segments.py), a checkpoint about every
# EVERY game frames, with VERIFY.txt; OUTDIR is emptied first.
set -o pipefail
. ~/vidsync/recomp/tools/vidsync/remote/env.sh
rm -rf "$2"
python3 -u "$V/segments.py" "$1" "$2" --every "$3" 2>&1 | tail -30
