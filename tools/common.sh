# Shared helpers for the test scripts (source this, with ROOT set to the repo root).
#
# It loads the game profile, tools/paths.env: ORIG_ROM, ORIG_ELF, GBA_BIOS, MGBA_RUN,
# GAME_FRAME_ADDR, TRACES, ABI_GRAPH and the rest, as absolute paths under $ROOT.
: "${ROOT:?set ROOT to the repo root before sourcing tools/common.sh}"
. "$ROOT/tools/paths.env"

# poke_args ROM TRACE: extra mgba_run arguments for a trace's .poke sidecar
# (lines "GAME_FRAME ADDR=VALUE/SIZE,...", in game-frame order): each line's
# writes are applied at the entry of main_awaitFrame (the end of a game
# frame's work) in that ROM, in the first hardware frame that starts on
# GAME_FRAME or later. Which game frame's work that ends depends on the
# build's speed, so a line may say where the original's write landed:
# "GAME_FRAME:LANDED ..." (tools/import_route.py measures it). The original
# then goes by GAME_FRAME as always, and any other ROM by LANDED (mgba_run
# --poke-live), so a slower port gets the write at the same point.
#
# A trace recorded from a save file (tools/record --save) has it next to it
# as NAME.sav; poke_args passes it too (--save), so every script that replays
# traces starts both ROMs from it.
poke_args() {
  local rom=$1 trace=$2 p="${2%.trace}.poke" elf g spec pc live=
  [ -f "${trace%.trace}.sav" ] && echo --save "${trace%.trace}.sav"
  [ -f "$p" ] || return 0
  case "$rom" in
    */bn6f/bn6f.gba) elf="${rom%.gba}.elf" ;;
    *) elf="${rom%.gba}.elf" ;;
  esac
  pc=$(arm-none-eabi-nm "$elf" 2>/dev/null | awk '$3=="main_awaitFrame"{print $1; exit}')
  if [ -z "$pc" ]; then   # without it the pokes land nowhere and the replay silently diverges
    echo "poke_args: main_awaitFrame not found in $elf (needed for $p); the ELF must sit next to the ROM" >&2
    kill -TERM $$   # we run inside $(...): stop the calling script
    return 1
  fi
  if [ "$(realpath "$rom")" != "$(realpath "$ORIG_ROM")" ] && grep -q '^[0-9]*:' "$p"; then
    live=1
    echo --poke-live
  fi
  while read -r g spec; do
    [ -n "$g" ] && [ "${g#\#}" = "$g" ] || continue
    if [ -n "$live" ]; then g=${g#*:}; else g=${g%%:*}; fi
    echo --poke-at "0x$pc" "$g" "$spec"
  done < "$p"
}
