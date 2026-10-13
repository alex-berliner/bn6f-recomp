#!/bin/bash
# install.sh TGZ: put a deploy (tools/vidsync/deploy.py) in place: unpack it as ~/vidsync/deploys/STAMP, link
# the PC's own data into it, build mgba_run and emu_server there, then point the link ~/vidsync/recomp at it
# (a rename of the link: jobs that run keep their own tree; WSL1, on 192.168.1.167, cannot rename a directory
# a process works in). The three newest deploys are kept.
set -euo pipefail
home=$HOME/vidsync; new=$home/deploys/$(date +%Y%m%d-%H%M%S)
mkdir -p "$new"; tar xzf "$1" -C "$new"
mkdir -p "$new/bn6f" "$new/roms" "$new/build-vidsync"
ln -s "$home/data/bn6f.gba" "$new/bn6f/bn6f.gba"
ln -s "$home/data/gba_bios.bin" "$new/roms/gba_bios.bin"
ln -s "$home/data" "$new/build-vidsync/data"
ln -s "$home/data/ready" "$new/build-vidsync/ready"
VIDSYNC_SYMS=$new/vidsync-syms.json make -s -C "$new/tools" mgba_run emu_server
if [ -d "$home/recomp" ] && [ ! -L "$home/recomp" ]; then   # a tree from before deploys/: set aside once
  mv "$home/recomp" "$home/deploys/0-first" || { echo "stop the jobs running in $home/recomp first"; exit 1; }
fi
ln -sfn "$new" "$home/recomp.link"; mv -T "$home/recomp.link" "$home/recomp"
# older deploys go, except one a process still works in (its cwd: the job scripts cd into the deploy's
# build-vidsync, and relative data/... paths die with it) or runs from
in_use() { for p in /proc/[0-9]*; do
             case "$(readlink "$p/cwd" 2>/dev/null)" in "$1"*) return 0 ;; esac
             tr '\0' ' ' < "$p/cmdline" 2>/dev/null | grep -q "$1" && return 0
           done; return 1; }
for d in $(find "$home/deploys" -mindepth 1 -maxdepth 1 -type d | sort | head -n -3); do
  if in_use "$d"; then echo "kept $d (in use)"; else rm -rf "$d" 2>/dev/null || true; fi
done
echo "installed $(tr -d '\n' < "$home/recomp/DEPLOYED.json") in $new"
