# CPU-oracle triage (tools/oracle.py)

Full run 2026-10-08 (4 random states per function, snapshots from the
original): 9,267 of 13,671 lifted functions compared on at least one state;
67 differed. One was a real deviation, now fixed: the hand-written
sub_30068E8 did not leave the original's flags (no caller was shown to read
them; 12 of the 67). The rest are harness artifacts or by design, and will
recur in later runs until the harness models them:

| Functions | Class | Why it is not a port bug |
|---|---|---|
| 34 | **BIOS leftovers**: r1/r3 after Sqrt, ArcTan2, CpuSet, CpuFastSet, LZ77 | The host's SWI emulation (`cport/src/runtime/swi.c`) set only the documented results; the GBA build calls the real BIOS. **Fixed in the host:** swi.c now follows the BIOS routines register for register (CpuSet returns through `pop {r3}`, leaving r3 = 0x170; Sqrt's Newton iteration; CpuFastSet's last block; LZ77Wram's end pointers and back-reference overshoot). 31 now pass (16 states each). The other 3 (ho_8034218, sub_8034268, sub_8039658) are **unexplained**: every callee passes on its own and memory matches exactly, but the composed run ends with r2 = 0x500/0x80, r3 = -1 where the original has 0, 0; not I/O (the I/O template below changes nothing). Unobservable: ho_8034218's dispatcher returns straight up the subsystem chain, and sub_8039658's dispatcher calls IsCurSubsystemInUse and sub_803C59C, neither of which reads r2/r3. Finding the function would need call-boundary tracing on the host. |
| 5 | **Sound hardware**: CgbSound and its callers (SoundMain, m4aSoundMain, call_m4aSoundMain, sub_3005D78) | CgbSound reads NR52's live channel status; m4a_2_814F00C restarts sound DMA. **Fixed in the oracle:** the host now gets mGBA's post-reset I/O (below); CgbSound passes, no memory differences remain. What remains is r12/flags from the hand-written mixer (SoundMainRAM, which promises r4-r11), discarded by the interrupt dispatcher. |
| 12 | **Fixed**: sub_30068E8 (hand-written) did not reproduce the original's flags; 11 callers passed them out (sub_801C416, sub_801C9E4, sub_801CDEC, sub_801D564, sub_802FE28, and via sub_802FE28: sub_802AEB8, sub_811FA98, sub_81267A8, sub_8127374, sub_812907C, sub_8134548) | sub_30068E8 now leaves the original's flags on every path; difftest compares them (`compare=...,nz,c,v`). All 12 pass the oracle (16 states each), lockstep identical on all routes. |
| 6 | **Hand-written, narrower contract**: sub_3006028, sub_30061E8, sub_3006440, sub_3006920, copyTo_iObjectAttr3001D70_3006814, sub_814E260 | They promise only the registers listed in `cport/host/difftest.cfg` (`compare=`). oracle.py now holds them to that (24 states each: all pass, except one state of sub_3006920 with an odd random buffer pointer, the misaligned-load case below). sub_30061E8 also differed in memory in one state: its random position pointer was odd, and the ARM7 turns a misaligned `ldrsh` into a sign-extended byte load. The port does the same on the GBA (same instruction), but the hand-written files' `S16()` is a plain host read there. With the pointer real calls pass (obj + 0x34), memory matches exactly. |
| 4 | **Callers of those**: sub_8002694, sub_8003E18, sub_8004218, sub_8004510 | They pass a hand-written callee's unpromised r2/r3/r7 out. Checked: sub_800C5E0, called next by every caller, does not read r3. |
| 2 | **Serial I/O**: libSIOControlStart, libSIO814469C | They read SIOCNT: mGBA's is freshly reset (0), the host's was the snapshot's. **Fixed in the oracle:** both pass with the I/O template. |
| 2 | **IWRAM data relocated to ROM**: sub_802C5E6, sub_802C646 | r7 = 0x081D7018 (port) vs 0x03006B18 (original): the same data, read from its ROM source (`IWRAM_ROM`). |
| 2 | **Random input out of domain**: getPrimaryElementWeaknessMultipler_3007432, sub_814E4CC | Table index past the 5x5 table (real calls pass elements 0-4); `bx r3` with r3 = 0x64, inside the BIOS. |

oracle.py's I/O template: mGBA's --call-snaps core is reset and loads only
the display registers (below 0x58) from a snapshot, so the host now gets
the I/O that mGBA reads back after running a `bx lr` function (0x58 up),
and write-only display registers as 0.

Harness improvements that would remove most of the rest: call-boundary
tracing on the host (to place the ho_8034218 group); hand-written files' U16/S16/U32 modelling
misaligned loads on the host (random pointers only).
