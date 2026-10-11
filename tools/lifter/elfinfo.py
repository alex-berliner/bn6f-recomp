"""Symbol, section, byte and source-line access for the bn6f ELF.

Everything the lifter knows about the game comes from the byte-matching bn6f
build: bn6f.elf (symbols, sections, DWARF line table) + bn6f.gba (bytes) +
the .s sources (original assembly text, for comments).
"""
from __future__ import annotations

import os
import re
import struct
import subprocess
from dataclasses import dataclass, field

SYM_RE = re.compile(
    r"^\s*\d+:\s+([0-9a-f]+)\s+(\d+)\s+(\S+)\s+(\S+)\s+\S+\s+(\S+)\s+(.+?)\s*$")


@dataclass
class Section:
    name: str
    vma: int
    size: int
    file_off: int        # offset of the section bytes inside the .gba image (-1 = none)
    executable: bool


@dataclass
class ElfInfo:
    rom: bytes
    sections: list[Section] = field(default_factory=list)
    funcs: dict[int, tuple[str, int, str]] = field(default_factory=dict)  # addr -> (mode, size, name)
    labels: dict[int, str] = field(default_factory=dict)      # code/data labels (NOTYPE, non-ABS)
    label_names: dict[str, int] = field(default_factory=dict)
    mapping: list[tuple[int, str]] = field(default_factory=list)  # (addr, 'a'|'t'|'d')
    equates: dict[str, int] = field(default_factory=dict)     # ABS assembler constants
    lines: dict[int, tuple[str, int]] = field(default_factory=dict)  # addr -> (file, line)
    # addr -> the source file's path under src_root, where the line table names it (22 loader.s,
    # 23 data.s, ... share a basename, so `lines` alone can't say which one)
    line_paths: dict[int, str] = field(default_factory=dict)
    src_root: str = ""
    _src_cache: dict[str, list[str]] = field(default_factory=dict)
    _src_paths: dict[str, str] = field(default_factory=dict)

    # ── bytes ──────────────────────────────────────────────────────────
    def section_for(self, addr: int) -> Section | None:
        for s in self.sections:
            if s.file_off >= 0 and s.vma <= addr < s.vma + s.size:
                return s
        return None

    def rom_offset(self, addr: int) -> int | None:
        s = self.section_for(addr)
        if s is None:
            if 0x08000000 <= addr < 0x08000000 + len(self.rom):
                return addr - 0x08000000
            return None
        return s.file_off + (addr - s.vma)

    def u16(self, addr: int) -> int | None:
        o = self.rom_offset(addr)
        if o is None or o + 2 > len(self.rom):
            return None
        return struct.unpack_from("<H", self.rom, o)[0]

    def u32(self, addr: int) -> int | None:
        o = self.rom_offset(addr)
        if o is None or o + 4 > len(self.rom):
            return None
        return struct.unpack_from("<I", self.rom, o)[0]

    def is_readonly(self, addr: int) -> bool:
        """True when `addr` holds immutable bytes at runtime (cartridge ROM)."""
        return 0x08000000 <= addr < 0x08000000 + len(self.rom)

    # ── source text ────────────────────────────────────────────────────
    def source_line(self, addr: int) -> str | None:
        loc = self.lines.get(addr)
        if not loc:
            return None
        fname, line = loc
        path = self.line_paths.get(addr) or self._src_paths.get(fname)
        if path is None:
            return None
        text = self._src_cache.get(path)
        if text is None:
            with open(path, encoding="utf-8", errors="replace") as fh:
                text = fh.read().split("\n")
            self._src_cache[path] = text
        if 1 <= line <= len(text):
            return text[line - 1].strip()
        return None


def _run(args: list[str]) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def load(bn6f_dir: str) -> ElfInfo:
    elf = os.path.join(bn6f_dir, "bn6f.elf")
    with open(os.path.join(bn6f_dir, "bn6f.gba"), "rb") as fh:
        info = ElfInfo(rom=fh.read())
    info.src_root = bn6f_dir

    # Sections. The ROM image is objcopy -O binary of the ELF, so a loadable
    # section's bytes live at (LMA - 0x08000000). readelf -lW gives LMAs via
    # program headers; simpler: ROM-resident sections have VMA in ROM, and
    # iwram_text's LMA is IWRAMRoutinesROMLocation.
    sec_re = re.compile(r"^\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\S+\s+(\S*)")
    secidx: dict[str, Section] = {}
    for line in _run(["readelf", "-SW", elf]).splitlines():
        m = sec_re.match(line)
        if not m:
            continue
        idx, name, typ, vma, _off, size, flags = m.groups()
        vma, size = int(vma, 16), int(size, 16)
        if typ not in ("PROGBITS", "NOBITS"):
            continue
        s = Section(name, vma, size, -1, "X" in flags)
        if typ == "PROGBITS" and 0x08000000 <= vma < 0x0A000000:
            s.file_off = vma - 0x08000000
        info.sections.append(s)
        secidx[idx] = s

    # Symbols.
    for line in _run(["readelf", "-sW", elf]).splitlines():
        m = SYM_RE.match(line)
        if not m:
            continue
        value, size, typ, bind, ndx, name = m.groups()
        value, size = int(value, 16), int(size)
        if ndx == "UND" or not name:
            continue
        if ndx == "ABS":
            if typ == "NOTYPE" and not name.endswith(".o"):
                info.equates.setdefault(name, value)
            continue
        if name in ("$t", "$a", "$d") or name.startswith(("$t.", "$a.", "$d.")):
            info.mapping.append((value, name[1]))
            continue
        if typ == "FUNC":
            mode = "t" if value & 1 else "a"
            info.funcs.setdefault(value & ~1, (mode, size, name))
        info.labels.setdefault(value & ~1 if typ == "FUNC" else value, name)
        info.label_names.setdefault(name, value)
    info.mapping.sort()

    # iwram_text: VMA 0x03005B00, bytes at its LMA inside the ROM image.
    lma = info.label_names.get("IWRAMRoutinesROMLocation")
    for s in info.sections:
        if s.name == "iwram_text" and lma is not None:
            s.file_off = lma - 0x08000000

    # DWARF line table -> per-address source location. Rows give the file's basename; the
    # header line before each run of rows ("./maps/ACDCTown/loader.s:") gives its path.
    cur_path = None
    for line in _run(["readelf", "--debug-dump=decodedline", elf]).splitlines():
        parts = line.split()
        if len(parts) == 1 and line.endswith(".s:"):
            cur_path = os.path.normpath(os.path.join(bn6f_dir, line[:-1]))
            continue
        if len(parts) >= 3 and parts[0].endswith(".s") and parts[1].isdigit() and parts[2].startswith("0x"):
            a = int(parts[2], 16)
            if a not in info.lines:
                info.lines[a] = (parts[0], int(parts[1]))
                if cur_path is not None and os.path.basename(cur_path) == parts[0] and os.path.isfile(cur_path):
                    info.line_paths[a] = cur_path
    # Resolve basenames to paths inside the repo.
    for root, _dirs, files in os.walk(bn6f_dir):
        if "/.git" in root or "/tools" in root:
            continue
        for f in files:
            if f.endswith(".s"):
                info._src_paths.setdefault(f, os.path.join(root, f))
    return info
