#!/bin/bash
# install.sh TGZ: put a deploy (tools/vidsync/deploy.py) in place as ~/vidsync/recomp: unpack it beside the
# old one, link the PC's own data into it, build mgba_run and emu_server there, then swap the two directories
# (jobs that are running keep the files they opened; the previous tree stays as ~/vidsync/recomp.old).
set -euo pipefail
home=$HOME/vidsync; new=$home/recomp.new
rm -rf "$new"; mkdir -p "$new"; tar xzf "$1" -C "$new"
mkdir -p "$new/bn6f" "$new/roms" "$new/build-vidsync"
ln -s "$home/data/bn6f.gba" "$new/bn6f/bn6f.gba"
ln -s "$home/data/gba_bios.bin" "$new/roms/gba_bios.bin"
ln -s "$home/data" "$new/build-vidsync/data"
ln -s "$home/data/ready" "$new/build-vidsync/ready"
VIDSYNC_SYMS=$new/vidsync-syms.json make -s -C "$new/tools" mgba_run emu_server
rm -rf "$home/recomp.old"
if [ -d "$home/recomp" ]; then mv "$home/recomp" "$home/recomp.old"; fi
mv "$new" "$home/recomp"
echo "installed $(tr -d '\n' < "$home/recomp/DEPLOYED.json")"
