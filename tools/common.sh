# Shared helpers for the test scripts (source this).
#
# poke_args ROM TRACE: extra mgba_run arguments for a trace's .poke sidecar
# ("GAME_FRAME ADDR=VALUE/SIZE,..."): the writes are applied at the entry of
# main_awaitFrame (the end of a game frame's work) in that ROM, so the
# original and a port see them at the same point of the same game frame.
#
# A trace recorded from a save file (tools/record --save) has it next to it
# as NAME.sav; poke_args passes it too (--save), so every script that replays
# traces starts both ROMs from it.
poke_args() {
  local rom=$1 trace=$2 p="${2%.trace}.poke" elf g spec pc
  [ -f "${trace%.trace}.sav" ] && echo --save "${trace%.trace}.sav"
  [ -f "$p" ] || return 0
  read -r g spec < "$p"
  case "$rom" in
    */bn6f/bn6f.gba) elf="${rom%.gba}.elf" ;;
    *) elf="${rom%.gba}.elf" ;;
  esac
  pc=0x$(arm-none-eabi-nm "$elf" | awk '$3=="main_awaitFrame"{print $1; exit}')
  echo --poke-at "$pc" "$g" "$spec"
}
