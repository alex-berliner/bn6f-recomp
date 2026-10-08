# CPU-oracle triage (tools/oracle.py)

Full run 2026-10-08 (4 random states per function, snapshots from the
original): 9,267 of 13,671 lifted functions compared on at least one state;
67 differed. One was a real deviation, now fixed: the hand-written
sub_30068E8 did not leave the original's flags (no caller was shown to read
them; 12 of the 67). The rest are harness artifacts or by design, and will
recur in later runs until the harness models them:

| Functions | Class | Why it is not a port bug |
|---|---|---|
| 34 | **BIOS leftovers**: r1/r3 differ after Div, Sqrt, ArcTan2, CpuSet, CpuFastSet, LZ77 | The host build emulates SWIs in `cport/src/runtime/swi.c`, which sets only the documented results. The GBA build calls the real BIOS. Every failing call made a BIOS call (`mgba_run --call-snaps` writes `snap_X.swi`); everything else matched. (The BIOS restores the caller's flags on return, `movs pc, lr`, so flags are never BIOS leftovers.) |
| 5 | **Sound hardware**: CgbSound and its callers (SoundMain, m4aSoundMain, call_m4aSoundMain, sub_3005D78) | CgbSound reads NR52's live channel status; m4a_2_814F00C restarts sound DMA. The host has the snapshot's frozen I/O. oracle.py now clears NR52's status bits (CgbSound: 16/16 pass); the rest is r12/flags discarded by the interrupt dispatcher, and one channel byte that depends on live sound DMA. |
| 12 | **Fixed**: sub_30068E8 (hand-written) did not reproduce the original's flags; 11 callers passed them out (sub_801C416, sub_801C9E4, sub_801CDEC, sub_801D564, sub_802FE28, and via sub_802FE28: sub_802AEB8, sub_811FA98, sub_81267A8, sub_8127374, sub_812907C, sub_8134548) | sub_30068E8 now leaves the original's flags on every path; difftest compares them (`compare=...,nz,c,v`). All 12 pass the oracle (16 states each), lockstep identical on all routes. |
| 6 | **Hand-written, narrower contract**: sub_3006028, sub_30061E8, sub_3006440, sub_3006920, copyTo_iObjectAttr3001D70_3006814, sub_814E260 | They promise only the registers listed in `cport/host/difftest.cfg` (`compare=`); the oracle holds them to the lifted contract. sub_30061E8 also differed in memory in one state: its random position pointer was odd, and the ARM7 turns a misaligned `ldrsh` into a sign-extended byte load. The port does the same on the GBA (same instruction), but the hand-written files' `S16()` is a plain host read there. With the pointer real calls pass (obj + 0x34), memory matches exactly. |
| 4 | **Callers of those**: sub_8002694, sub_8003E18, sub_8004218, sub_8004510 | They pass a hand-written callee's unpromised r2/r3/r7 out. Checked: sub_800C5E0, called next by every caller, does not read r3. |
| 2 | **Serial I/O**: libSIOControlStart, libSIO814469C | They read SIOCNT: mGBA's is freshly reset (0), the host's is the snapshot's. |
| 2 | **IWRAM data relocated to ROM**: sub_802C5E6, sub_802C646 | r7 = 0x081D7018 (port) vs 0x03006B18 (original): the same data, read from its ROM source (`IWRAM_ROM`). |
| 2 | **Random input out of domain**: getPrimaryElementWeaknessMultipler_3007432, sub_814E4CC | Table index past the 5x5 table (real calls pass elements 0-4); `bx r3` with r3 = 0x64, inside the BIOS. |

Harness improvements that would remove most of these: a host BIOS that
reproduces the real BIOS's leftover registers; the oracle using
difftest.cfg's `compare=` for hand-written functions; host non-display I/O
matching mGBA's post-reset values.
