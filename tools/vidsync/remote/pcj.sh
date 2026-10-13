#!/bin/bash
cd ~/vidsync; cp /mnt/c/Users/alex/pathcheck.py scripts/; cp /mnt/c/Users/alex/emu_server.c src/; make -s
export VIDSYNC_ROM=$PWD/data/bn6f.gba VIDSYNC_BIOS=$PWD/data/gba_bios.bin VIDSYNC_THUMBS=$PWD/data/full/thumb.u8
python3 scripts/pathcheck.py "$@" 2>&1
