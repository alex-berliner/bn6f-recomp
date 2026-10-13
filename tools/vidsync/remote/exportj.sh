#!/bin/bash
# exportj.sh RUNDIR OUTNAME: build the patched harness, export the newest checkpoint of RUNDIR, verify it
cd ~/vidsync; cp /mnt/c/Users/alex/export_trace.py scripts/; mkdir -p build; tar xzf /mnt/c/Users/alex/harness_src.tgz -C build
mkdir -p build/harness/build-tools; gcc -O2 -o build/harness/build-tools/mgba_run build/harness/tools/mgba_run.c -lmgba || exit 1
export VIDSYNC_ROM=$PWD/data/bn6f.gba VIDSYNC_BIOS=$PWD/data/gba_bios.bin VIDSYNC_THUMBS=$PWD/data/full/thumb.u8
ck=$(bash /mnt/c/Users/alex/newest.sh $1 | awk '{print $2}'); echo "checkpoint $1/$ck"
mkdir -p data/ready; python3 scripts/export_trace.py data/auto/$1/$ck.json data/ready/$2 --verify 2>&1 | tail -5
cp data/auto/$1/$ck.json data/ready/$2.result.json; ls -la data/ready
