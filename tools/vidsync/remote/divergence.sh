#!/bin/bash
# divergence.sh RESULT OUT ARGS...: where mgba_run and emu_server part ways on an exported result
# (divergence.py; run in build-vidsync like export.sh).
set -eo pipefail
. ~/vidsync/recomp/tools/vidsync/remote/env.sh
python3 -u "$V/divergence.py" "$@" 2>&1 | tail -5
