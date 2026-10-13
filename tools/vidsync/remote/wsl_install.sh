#!/bin/bash
set -e
cd ~ && rm -rf vidsync && tar -xzf /mnt/c/Users/alex/vidsync_pkg.tgz && cd vidsync && make -s && ls -la build/emu_server
sha1sum data/bn6f.gba
