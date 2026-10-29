#!/usr/bin/env bash
# ramdump.sh ROUTE GAME_FRAMES — replay ROUTE.trace on the original and the
# port up to GAME_FRAMES+1, dumping each game's full state at every
# GAME_FRAMES with mgba_run --dump-mem-at-when (see tools/mgba_run.c).
#
# Outputs go to $OUT_DIR/{ROUTE}_{orig,port}_<frame>.{ewram,iwram,vram,
# pal,oam,save,ior}.bin, for tools/handoff_diff.py to diff and classify.
#
# Run on a fast host (germandell here, but the script is host-agnostic —
# libmgba must be reachable via $LD_LIBRARY_PATH). $ROOT is the
# recomp-remote mirror; the binary lives at $ROOT/build-tools/mgba_run.bin
# (overridden by $MGBA_RUN).
set -euo pipefail
ROUTE=$1
shift
# remaining args: list of game frames (comma-separated, e.g. "5000,9700")
# or repeated gf lines.  Accept either --frames 5000,9700 or "5000,9700"
GF_LIST=$1
OUT_DIR=${OUT_DIR:-/tmp/ramdump}
ROOT=${ROOT:-~/recomp-remote}
MAX_GF=$(echo $GF_LIST | tr ',' ' ' | tr ' ' '\n' | sort -n | tail -1)
HW_BUDGET=$(( MAX_GF * 3 > 100000 ? MAX_GF * 3 : 100000 ))

# mgba_run binary: avoid mgba_run.bin (it might be in use by other replays);
# fall back to whatever appears.
MGBA_RUN=${MGBA_RUN:-$ROOT/build-tools/mgba_run.bin.new}
[ -x "$MGBA_RUN" ] || MGBA_RUN=$ROOT/build-tools/mgba_run.bin
export LD_LIBRARY_PATH=$ROOT/build-tools/lib:${LD_LIBRARY_PATH:-}

ORIG=$ROOT/bn6f/bn6f.gba
# port ROM may live in cport/build/ (mirrored from the worktree) or
# directly under $ROOT (rsync-friendly).
PORT=$ROOT/cport/build/bn6f_lifted.gba
[ -f "$PORT" ] || PORT=$ROOT/bn6f_lifted.gba
[ -f "$PORT" ] || { echo "port ROM not found (tried $ROOT/cport/build/bn6f_lifted.gba and $ROOT/bn6f_lifted.gba)" >&2; exit 1; }
TRACE=$ROOT/tools/traces/$ROUTE.trace
SAV=$ROOT/tools/traces/${ROUTE%.trace}.sav
[ -f "$SAV" ] || SAV=""
# --save is optional (script tolerates missing saves)
SAVE_ARG=$([ -f "$SAV" ] && echo "--save $SAV" || true)

# poke_args on the original and the port look up main_awaitFrame in the
# ELF next to the ROM. arm-none-eabi-nm may not be on every replay host;
# allow overriding via $ORIG_MAIN_AWAIT_FRAME / $PORT_MAIN_AWAIT_FRAME env
# vars (so a released bn6f.elf / bn6f_lifted.elf pair has a known fixed
# address; the port's main_awaitFrame moves when lifted C is rebuilt).
: "${ORIG_MAIN_AWAIT_FRAME:=080003a0}"   # bn6f fixed address
: "${PORT_MAIN_AWAIT_FRAME:=}"
if [ -z "$PORT_MAIN_AWAIT_FRAME" ]; then
  PORT_MAIN_AWAIT_FRAME=$(arm-none-eabi-nm $ROOT/cport/build/bn6f_lifted.elf 2>/dev/null \
                            | awk '$3=="main_awaitFrame"{print $1; exit}')
fi
[ -n "$PORT_MAIN_AWAIT_FRAME" ] || {
  echo "PORT_MAIN_AWAIT_FRAME not set and arm-none-eabi-nm can't find it in $ROOT/cport/build/bn6f_lifted.elf" >&2
  exit 1
}
ORIG_PC=$ORIG_MAIN_AWAIT_FRAME
PORT_PC=$PORT_MAIN_AWAIT_FRAME

# the .elf next to the ROM is what tools/common.sh's poke_args reads (line
# "elf=\"${rom%.gba}.elf\""). Symlink if missing so the version of poke_args
# recorded by lockstep.sh works unchanged.
[ -e "$ROOT/bn6f/bn6f.elf" ] || ln -sf ${ROOT}/bn6f.gba ${ROOT}/bn6f/bn6f.elf 2>/dev/null || true

poke_args_for() {
  local rom=$1 trace=$2 pc=$3 p="${trace%.trace}.poke"
  [ -f "$p" ] || return
  if [ "$(realpath "$rom")" != "$(realpath "$ORIG")" ] && grep -q '^[0-9]*:' "$p"; then
    echo --poke-live
  fi
  while read -r g spec; do
    [ -n "$g" ] && [ "${g#\#}" = "$g" ] || continue
    if [ "$(realpath "$rom")" != "$(realpath "$ORIG")" ] && grep -q '^[0-9]*:' "$p"; then
      g=${g#*:}
    else
      g=${g%%:*}
    fi
    echo --poke-at "0x$pc" "$g" "$spec"
  done < "$p"
}

mkdir -p "$OUT_DIR"
dump() {
  local rom=$1 tag=$2 out=$3 pc=$4
  echo "[$(date +%H:%M:%S)] $rom $tag" >&2
  $MGBA_RUN "$rom" --frames $HW_BUDGET --bios $ROOT/roms/gba_bios.bin \
    --input $TRACE --game-frame-addr 0x020093D4 \
    --until-game-frame $((MAX_GF + 1)) --dump-mem-at-when "$GF_LIST" "$out" \
    --quiet $SAVE_ARG $(poke_args_for "$rom" "$TRACE" "$pc") > "$out.log" 2>&1 \
    || { echo "FAILED ($tag). last lines:"; tail -10 "$out.log"; exit 1; }
  # resize busy budget won't get hit because until-game-frame terminates.
}

dump "$ORIG" "orig" "$OUT_DIR/${ROUTE}_orig" "$ORIG_PC"
dump "$PORT" "port" "$OUT_DIR/${ROUTE}_port" "$PORT_PC"
echo "[$(date +%H:%M:%S)] done -> $OUT_DIR"
