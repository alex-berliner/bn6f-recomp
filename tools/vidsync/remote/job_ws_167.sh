#!/bin/bash
cd ~/vidsync; for f in rngfix.py worker_server.py autosync.py feat.py emu.py beam.py; do cp /mnt/c/Users/alex/$f scripts/; done
cp /mnt/c/Users/alex/emu_server.c src/; make -s
export VIDSYNC_ROM=$PWD/data/bn6f.gba VIDSYNC_BIOS=$PWD/data/gba_bios.bin
VD=${VD:-data/full}
if [ -f $VD/thumb.base ]; then export VIDSYNC_THUMBS=$PWD/$VD/thumb.u8 VIDSYNC_THUMB_BASE=$(cat $VD/thumb.base); else export VIDSYNC_THUMBS=$PWD/$VD/thumb.u8; fi
exec python3 -u scripts/worker_server.py $VD 7777 15 > data/ws.log 2>&1 < /dev/null
