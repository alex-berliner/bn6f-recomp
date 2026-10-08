#pragma once
#include "cpu.h"

#define REG16(a) (*(volatile u16 *)(a))
#define REG32(a) (*(volatile u32 *)(a))
#define REG_DISPSTAT REG16(0x04000004)
#define REG_KEYCNT   REG16(0x04000132)
#define REG_IME      REG16(0x04000208)
#define REG_WAITCNT  REG16(0x04000204)
#define BIOS_IRQ_VECTOR REG32(0x03007FFC)

#define ARM_CODE __attribute__((target("arm")))

void gba_boot(void) __attribute__((noreturn));
void irq_entry(void);
