# Host build of the lifted C + hand-written replacements, for differential
# testing (`make -C cport/host test`). Guest RAM is mapped at its real
# addresses, so memory accesses are plain pointer accesses as on the GBA.
CC      := gcc
BUILD   := ../build-host
CFLAGS  := -O1 -g0 -DTARGET_FLAT -DDIFFTEST -I../include -I. -fno-strict-aliasing \
           -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable -w
LIFTED  := $(wildcard ../src/lifted/*.c)
SRCS    := $(LIFTED) $(wildcard ../src/game/*.c) ../src/runtime/cpu.c ../src/runtime/swi.c \
           ../src/runtime/reloc.c memmap.c difftest.c difftest_gen.c
OBJS    := $(patsubst ../%.c,$(BUILD)/%.o,$(filter ../%,$(SRCS))) $(BUILD)/host/memmap.o $(BUILD)/host/difftest.o $(BUILD)/host/difftest_gen.o

all: $(BUILD)/difftest

$(BUILD)/%.o: ../%.c $(wildcard ../include/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/host/%.o: %.c $(wildcard ../include/*.h) hostmem.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/difftest: $(OBJS)
	$(CC) -o $@ $(OBJS)

test: $(BUILD)/difftest
	cd .. && BN6F_ROM=../bn6f/bn6f.gba $(BUILD:../%=%)/difftest -n $(or $(N),300)

.PHONY: all test
