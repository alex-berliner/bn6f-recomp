#!/bin/bash
# segj.sh RESULT OUTDIR EVERY: cut a result into save-file segments (scripts/segments.py), with the test emu_server
cd ~/vidsync; cp /mnt/c/Users/alex/segments.py /mnt/c/Users/alex/export_trace.py /mnt/c/Users/alex/emu.py scripts/
export VIDSYNC_THUMBS=$PWD/data/full/thumb.u8 VIDSYNC_ROM=$PWD/data/bn6f.gba VIDSYNC_BIOS=$PWD/data/gba_bios.bin VIDSYNC_EMU=$PWD/build/emu_server_t VIDSYNC_MGBA_RUN=$PWD/build/harness/build-tools/mgba_run
python3 scripts/segments.py "$1" "$2" --every "$3" 2>&1 | tail -30
