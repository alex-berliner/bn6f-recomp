#!/bin/bash
# export.sh CHECKPOINT OUTDIR NAME: export a checkpoint (a fix_/ckpt_ path without .json) as a route-harness
# recording OUTDIR/NAME.trace/.poke/.sav, verified with mgba_run (export_trace.py --verify), and keep the
# checkpoint's json beside it as NAME.result.json (segments.sh cuts that).
set -eo pipefail
. ~/vidsync/recomp/tools/vidsync/remote/env.sh
mkdir -p "$2"
python3 -u "$V/export_trace.py" "$1.json" "$2/$3" --verify 2>&1 | tail -5
cp "$1.json" "$2/$3.result.json"
ls -la "$2"
