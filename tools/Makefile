# The test harness binaries (git-ignored, paths from tools/paths.env):
#   make -C tools            both
#   make -C tools mgba_run   build-tools/mgba_run: headless mGBA that replays a route on a ROM and
#                            prints digests, snapshots and profiles (every check runs it)
#   make -C tools record     build-tools/record: plays the original game in a window and records a
#                            route (tools/record.sh)
# Needs libmgba's development files, and SDL2's for record
# (Debian/Ubuntu: sudo apt install libmgba-dev libsdl2-dev).
ROOT := $(abspath $(CURDIR)/..)
include paths.env

CFLAGS ?= -O2

.PHONY: all mgba_run record
all: mgba_run record
mgba_run: $(MGBA_RUN)
record: $(RECORD)

$(MGBA_RUN): mgba_run.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $< -lmgba

$(RECORD): record.c paths.env
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -DGAME_FRAME_ADDR=$(GAME_FRAME_ADDR)u -o $@ $< -lmgba $$(sdl2-config --cflags --libs)
