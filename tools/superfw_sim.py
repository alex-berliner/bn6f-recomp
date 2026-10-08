#!/usr/bin/env python3
"""Apply what SuperFW (Supercard firmware) does to a ROM before booting it.

  tools/superfw_sim.py IN.gba OUT.gba [--no-igm] [--no-directsave] [--fast-ewram]

Follows SuperFW's loader (src/loader.c load_gba_rom, src/patcher.c
patch_apply_rom) for a game found in its built-in database:
  - the database patches for the game code (res/patches.db; for BR6E: the
    WAITCNT store at 0x108 becomes a no-op, the IRQ-vector literal at 0x1F8
    becomes 0x03007FF4);
  - the DirectSave payload area (7 KiB) and the in-game menu after the ROM,
    from ROUND_UP(size, 1 KiB) + 1 KiB;
  - the header's first instruction becomes a branch to the menu's startup
    code, and the original boot address goes to 0xB8; header checksum fixed.
The in-game menu is a stand-in with the same startup and IRQ entry as
SuperFW's src/ingame.S: it points the BIOS IRQ vector at its handler, which
forwards every IRQ to the game's handler at 0x03007FF4 (the hotkey never
matches here). --fast-ewram also sets REG_MEMCTRL as SuperFW's launch does
with EWRAM overclocking on.
"""
import os, struct, subprocess, sys, tempfile

DB = os.environ.get("SUPERFW_DB", "/tmp/superfw_src/res/patches.db")
DIRSAVE_REQ_SPACE = 7 * 1024

STUB = r"""
.arm
igm:
    {fast_ewram}
    adr r0, handler
    mov r1, #0x04000000
    str r0, [r1, #-4]           @ BIOS IRQ vector (0x03007FFC mirror)
    mov r0, #0x08000000
    ldr pc, [r0, #0xB8]         @ the game's boot address
handler:
    ldrb r1, [r0, #0x202]       @ REG_IF
    tst r1, #1
    ldreq pc, [r0, #-12]        @ not VBlank: the game's handler (0x03007FF4)
    ldr r2, hotkey
    ldr r1, [r0, #0x130]        @ REG_KEYINPUT
    cmp r2, r1, lsl #16
    ldrne pc, [r0, #-12]
menu:
    b menu                      @ the real menu would open here
hotkey:
    .word 0x00F30000            @ L+R+Select+Start
"""
FAST_EWRAM = "ldr r0, =0x04000800\n    ldr r1, =0x0E000020\n    str r1, [r0]\n    b 1f\n    .ltorg\n1:"


def db_patch(gcode, ver):
    db = open(DB, "rb").read()
    sig, dbver, pcnt, idxcnt = struct.unpack_from("<4I", db, 0)
    assert sig == 0x31424450 and dbver == 0x10000
    prgs, i = [], 0
    while i < 512 and db[512 + i]:
        n = db[512 + i]
        prgs.append(db[512 + i + 1: 512 + i + 1 + n])
        i += n + 1
    entries = 1024 + 512 * idxcnt
    for k in range(pcnt):
        g = db[1024 + 8 * k: 1024 + 8 * k + 4]
        off = struct.unpack_from("<I", db, 1024 + 8 * k + 4)[0]
        if g == gcode and (off & 0xFF) == ver:
            p = entries + 4 * (off >> 8)
            h = struct.unpack_from("<I", db, p)[0]
            counts = (h & 0xFF, (h >> 8) & 0x1F, (h >> 16) & 0xFF, (h >> 24) & 0xF)
            ops = struct.unpack_from("<%dI" % sum(counts), db, p + 4)
            return counts, ops, prgs
    return None


def apply_ops(rom, ops, prgs):
    i = 0
    while i < len(ops):
        o = ops[i]
        opc, arg, addr = o >> 28, (o >> 25) & 7, o & 0x1FFFFFF
        if opc == 0:
            rom[addr:addr + len(prgs[arg])] = prgs[arg]
        elif opc == 1:
            rom[addr:addr + 2] = struct.pack("<H", 0x46C0)
        elif opc == 2:
            rom[addr:addr + 4] = struct.pack("<I", 0xE1A00000)
        elif opc == 3:
            for j in range(arg + 1):
                rom[addr + j] = (ops[i + 1 + j // 4] >> (8 * (j % 4))) & 0xFF
            i += (arg + 1 + 3) // 4
        elif opc == 4:
            for j in range(arg + 1):
                i += 1
                rom[addr + 4 * j: addr + 4 * j + 4] = struct.pack("<I", ops[i])
        else:
            sys.exit(f"patch op {opc:#x} not simulated")
        i += 1


def assemble(src):
    with tempfile.TemporaryDirectory() as d:
        s, o, b = (os.path.join(d, n) for n in ("stub.s", "stub.o", "stub.bin"))
        open(s, "w").write(src)
        subprocess.run(["arm-none-eabi-as", "-mcpu=arm7tdmi", s, "-o", o], check=True)
        subprocess.run(["arm-none-eabi-objcopy", "-O", "binary", o, b], check=True)
        return open(b, "rb").read()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    igm = "--no-igm" not in sys.argv
    directsave = "--no-directsave" not in sys.argv
    rom = bytearray(open(args[0], "rb").read())
    size = len(rom)
    found = db_patch(bytes(rom[0xAC:0xB0]), rom[0xBC])
    if not found:
        sys.exit(f"{rom[0xAC:0xB0].decode()} v{rom[0xBC]} is not in the database: SuperFW would ask about patching")
    (wcnt, save, irqh, rtc), ops, prgs = found
    apply_ops(rom, ops[:wcnt], prgs)
    apply_ops(rom, ops[wcnt:wcnt + save], prgs)
    romrsize = ((size + 1023) & ~1023) + 1024
    igm_addr = romrsize + (DIRSAVE_REQ_SPACE if directsave else 0)
    if igm:
        apply_ops(rom, ops[wcnt + save:wcnt + save + irqh], prgs)
        stub = assemble(STUB.format(fast_ewram=FAST_EWRAM if "--fast-ewram" in sys.argv else ""))
        rom += b"\xFF" * (igm_addr + len(stub) - len(rom))
        rom[igm_addr:igm_addr + len(stub)] = stub
        ibranch = struct.unpack_from("<I", rom, 0)[0]
        boot = ((ibranch & 0xFFFFFF) << 2) + 8 + 0x08000000
        struct.pack_into("<I", rom, 0, 0xEA000000 | ((igm_addr - 8) >> 2))
        struct.pack_into("<I", rom, 0xB8, boot)
        print(f"in-game menu at {0x08000000 + igm_addr:#x}, boot address {boot:#x} at 0xB8")
    rom[0xBD] = (-sum(rom[0xA0:0xBD]) - 0x19) & 0xFF
    open(args[1], "wb").write(rom)
    print(f"database patches: {wcnt} WAITCNT, {save} save, {irqh if igm else 0} IRQ-handler ops applied")


if __name__ == "__main__":
    main()
