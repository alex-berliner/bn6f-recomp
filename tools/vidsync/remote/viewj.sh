#!/bin/bash
cd ~/vidsync; cp /mnt/c/Users/alex/view.py scripts/; export VIDSYNC_ROM=$PWD/data/bn6f.gba VIDSYNC_BIOS=$PWD/data/gba_bios.bin
python3 scripts/view.py data/full "$1" $2 $3 $4 /mnt/c/Users/alex/view.png 2>&1 | tail -30
