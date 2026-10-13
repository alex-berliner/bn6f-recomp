# Sourced by every job script on a compute PC. The deployed tree (tools/vidsync/deploy.py) is the part of the
# recomp tree vidsync needs, in ~/vidsync/recomp; the PC's own data stays in ~/vidsync/data (never copied over
# the LAN). In the deployed tree bn6f/bn6f.gba, roms/gba_bios.bin and build-vidsync/data link to that data, so
# tools/paths.env's defaults hold; bn6f's symbols come from vidsync-syms.json (there is no bn6f here).
R=$(readlink -f "$HOME/vidsync/recomp")   # the deploy itself, not the link: a job keeps its own tree while
V=$R/tools/vidsync                       # the next deploy moves the link (install.sh keeps a tree in use)
export VIDSYNC_SYMS=$R/vidsync-syms.json
cd "$R/build-vidsync" || exit 1          # VIDSYNC: the scripts name data/... relative to it
export VIDSYNC_THUMBS=$PWD/data/full/thumb.u8
