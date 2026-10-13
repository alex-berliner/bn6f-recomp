# The test harness binaries (git-ignored, paths from tools/paths.env):
#   make -C tools            both
#   make -C tools mgba_run   build-tools/mgba_run: headless mGBA that replays a route on a ROM and
#                            prints digests, snapshots and profiles (every check runs it)
#   make -C tools record     build-tools/record: plays the original game in a window and records a
#                            route (tools/record.sh)
#   make -C tools emu_server build-tools/emu_server: vidsync's emulator server (tools/vidsync,
#                            docs/vidsync/README.md); not in `all`
# Needs libmgba's development files, and SDL2's for record
# (Debian/Ubuntu: sudo apt install libmgba-dev libsdl2-dev).
ROOT := $(abspath $(CURDIR)/..)
include paths.env

CFLAGS ?= -O2

.PHONY: all mgba_run record emu_server
all: mgba_run record
mgba_run: $(MGBA_RUN)
record: $(RECORD)
emu_server: $(EMU_SERVER)

$(MGBA_RUN): mgba_run.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $< -lmgba

$(RECORD): record.c paths.env
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -DGAME_FRAME_ADDR=$(GAME_FRAME_ADDR)u -o $@ $< -lmgba $$(sdl2-config --cflags --libs)

$(EMU_SERVER): vidsync/emu_server.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -march=native -o $@ $< -lmgba -lm
