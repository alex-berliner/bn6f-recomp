#!/usr/bin/env bash
# rawdis.sh START END [arm]  — disassemble ROM bytes ignoring bn6f's mapping symbols
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tools/common.sh"
S=$(( $1 - 0x08000000 )); E=$(( $2 - 0x08000000 ))
TMP=$(mktemp)
dd if="$ORIG_ROM" of="$TMP" bs=1 skip=$S count=$((E - S)) status=none
MODE="-M force-thumb"; [ "${3:-}" = arm ] && MODE=""
arm-none-eabi-objdump -D -b binary -m armv4t $MODE --adjust-vma=$1 "$TMP" | grep -E "^ +[0-9a-f]+:"
rm -f "$TMP"
