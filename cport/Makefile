# Lifted BN6 Falzar -> GBA ROM.
#
#   make -C cport            # builds cport/build/bn6f_lifted.gba
#
# Inputs: ../bn6f/bn6f.gba (byte-matching ROM built from the disassembly; its
# data stays at the original addresses) and src/lifted/*.c (tools/lifter).

CC      := $(shell command -v ccache >/dev/null && echo "ccache arm-none-eabi-gcc" || echo arm-none-eabi-gcc)
OBJCOPY := arm-none-eabi-objcopy
BASEROM ?= ../bn6f/bn6f.gba
BUILD   := build

ARCH    := -mcpu=arm7tdmi -mthumb -mthumb-interwork
OPT     ?= -Os
LIFTED_OPT ?= -O2
CFLAGS  := $(ARCH) -ffreestanding -fno-strict-aliasing -fomit-frame-pointer \
           -DTARGET_GBA -Iinclude -Wall -Wno-unused-label -Wno-unused-variable -Wno-array-bounds -Wno-comment -Werror=return-type -Wno-unused-but-set-variable
LDFLAGS := $(ARCH) -nostartfiles -T gba.ld -Wl,--gc-sections -Wl,-Map,$(BUILD)/bn6f_lifted.map

LIFTED  := $(wildcard src/lifted/*.c)
GAME    := $(wildcard src/game/*.c)
RUNTIME := $(wildcard src/runtime/*.c) $(wildcard src/gba/*.c) $(GAME)
OBJS    := $(patsubst src/%.c,$(BUILD)/%.o,$(LIFTED) $(RUNTIME)) $(BUILD)/base.o

all: $(BUILD)/bn6f_lifted.gba

$(BUILD)/lifted/%.o: src/lifted/%.c $(wildcard include/*.h) Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LIFTED_OPT) -c $< -o $@

$(BUILD)/%.o: src/%.c $(wildcard include/*.h) Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OPT) -c $< -o $@

# The ARM IRQ entry must be ARM code.
$(BUILD)/gba/irq.o: CFLAGS += -marm

$(BUILD)/base.o: $(BASEROM)
	@mkdir -p $(BUILD)
	$(OBJCOPY) -I binary -O elf32-littlearm -B arm \
	    --rename-section .data=.base,alloc,load,readonly,data,contents $< $@

$(BUILD)/bn6f_lifted.elf: $(OBJS) gba.ld
	$(CC) $(LDFLAGS) $(OBJS) -lc -lgcc -o $@

$(BUILD)/bn6f_lifted.gba: $(BUILD)/bn6f_lifted.elf ../tools/mkrom.py
	python3 ../tools/mkrom.py $< $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
