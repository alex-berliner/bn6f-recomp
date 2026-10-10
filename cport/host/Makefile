# Host build of the lifted C + hand-written replacements, for differential
# testing (`make -C cport/host test`). Guest RAM is mapped at its real
# addresses, so memory accesses are plain pointer accesses as on the GBA.
CC      := $(shell command -v ccache >/dev/null && echo "ccache gcc" || echo gcc)
BUILD   := ../build-host
CFLAGS  := -O1 -g0 -DTARGET_FLAT -DDIFFTEST -I../include -I. -fno-strict-aliasing \
           -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -w -Werror=implicit-function-declaration
LIFTED  := $(wildcard ../src/lifted/*.c)
SRCS    := $(LIFTED) $(wildcard ../src/game/*.c) ../src/runtime/cpu.c ../src/runtime/swi.c \
           ../src/runtime/reloc.c memmap.c difftest.c difftest_gen.c
OBJS    := $(patsubst ../%.c,$(BUILD)/%.o,$(filter ../%,$(SRCS))) $(BUILD)/host/memmap.o $(BUILD)/host/difftest.o $(BUILD)/host/difftest_gen.o
# Each object's real header dependencies ($(BUILD)/**/*.d), so a changed header
# rebuilds only the files that include it.
DEPFLAGS := -MMD -MP

all: $(BUILD)/difftest

$(BUILD)/%.o: ../%.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/host/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# The lifted originals' declarations (L_NAME, for difftest_pairs.h) are in a
# header of their own that only the difftest reads: installs add to it, and
# nothing else has to recompile.
$(BUILD)/host/difftest.o: CFLAGS += -include lifted_originals.h

-include $(OBJS:.o=.d)

$(BUILD)/difftest: $(OBJS)
	$(CC) -rdynamic -o $@ $(OBJS) -lpthread -ldl

test: $(BUILD)/difftest
	cd .. && BN6F_ROM=../bn6f/bn6f.gba $(BUILD:../%=%)/difftest -n $(or $(N),300)

.PHONY: all test
