#!/bin/bash
cd ~/vidsync; export VIDSYNC_ROM=$PWD/data/bn6f.gba VIDSYNC_BIOS=$PWD/data/gba_bios.bin VIDSYNC_THUMBS=$PWD/data/full/thumb.u8
python3 /mnt/c/Users/alex/fcmp.py 2>&1 | tail -40
