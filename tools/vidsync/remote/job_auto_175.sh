#!/bin/bash
cd ~/vidsync; for f in rngfix.py worker_server.py autosync.py feat.py emu.py beam.py; do cp /mnt/c/Users/alex/$f scripts/; done
cp /mnt/c/Users/alex/emu_server.c src/; make -s; cp /mnt/c/Users/alex/supervise.sh ./
export VIDSYNC_REPLAY=data/auto/run5w.0/loss_241015.json
exec bash supervise.sh ${OUT:-data/auto/run7k} ${START:-data/auto/run6y.0/fix_0026}
