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
tracing on the host (to place the ho_8034218 group). The misaligned-load cases
(random odd pointers) are now recognised as a class: see the 2026-10-10 section.

## Oracle drops from farm groups (convert runs, 2026-10-08/09)

When farm groups went through convert, the full verify's oracle took 89 functions
out (97 drops). Their differences are in `symbols/pool_drops.tsv` (made by
`tools/convert/pool.py` from the Burr logs), sorted against the table above by name,
by what each function calls, and by the shape of the difference. Only 5 match a
known class: 4 BIOS leftovers (ho_8034218 and sub_8034268 with the unexplained
r2 = 0x500, r3 = -1; the two LZ77 wrappers) and 1 caller of the hand-written ones
(sub_8002694). The other 84 match none: 31 differ in memory, 31 in registers,
16 in flags only, 5 only in a register's upper bits (the readable code returns a
narrower value than the register the original left), and 1 has a callee that differed
in the same run. 88 of the 89 are still not installed.

## Full verify 2026-10-10: three classes of random state the port is not held to

verify-20261010-191155 (all 1,434 readable functions, 24 states each, `--snapshots build-snapshots`):
1,296 compared, 9 differed, each in one or two states. None is a port bug. In every one the random
input leaves what real calls pass (aligned pointers into mapped memory, data above SP), and the
difference comes from how the ARM7 or the original's own stack treats that input. Six differ the same
way under `--lifted`, but that flag swaps only the function under test; its callees stay the port's. So
the trace below is what places each one, not the flag.

**Handled in the oracle by class.** `mgba_run --call-snaps` now notes, for the original's call,
every misaligned halfword or word load and every open-bus read (mGBA's own messages: unused or
write-only I/O register, unmapped memory, BIOS) in `snap_*.bus`. With the notes on, the exit
snapshots of these 10 states are byte-identical to before. `oracle.py` counts a differing state as
out of domain when the original made such a load, or when an entry register points into the free
stack (0x03007500 up to SP). It lists those states in `DIR/explained.txt`, with their snapshots in
`DIR/explained/`.

| Class | Function (state) | What happens |
|---|---|---|
| misaligned load | sprite_projectAndCull | r0, the position pointer, is 0x0810962B (odd). The original's `ldrsh` at an odd address loads the sign-extended byte; the hand-written `S16()` reads two bytes on the host. Real calls pass obj + 0x24. |
| misaligned load | battle_tickCollisionTimer_8010162 | The collision pointer at [r5 + 0x54] is 0x060000A3, so `ldrh` of 0x060000C9 returns 0xFF0000FF. The original's `sub r1, #1; blt` takes that as expired and stores nothing. The readable u16 counter is neither 0xFFFF nor 0, so it stores 0x00FE (VRAM 0x060000C8 differs). With an aligned pointer the two agree: ldrh gives 0-0xFFFF, and `blt` after `sub #1` means "was 0". |
| misaligned load | sub_80313FC | r0 = 0x080005AD (odd, ROM). sub_8031176's word loads from it rotate. The readable callee sprite_worldToScreenPos_80310EC reads through a plain `u32 *`: the same `ldr` on the GBA, but an unrotated unaligned read on the host. So dword_200F3E0 and the camera byte 0x02009F63 differ. |
| open bus | battle_getSpecialActorSlotIfAiMatches_80EBB78 (2 states), battle_updateAngerTint_80143E4 | r5 = 0x02008470 is not a battle object: its AI-data pointer [r5 + 0x58] is 0x04000600, past the I/O registers. The version byte (0x04000616) and the anger halfword (0x04000634) are open bus on the GBA (r2 = 5; the tint is applied) and 0 in the host's I/O page. |
| open bus | battle_updateObjectByKind_8026E98 | r5 = 0x04000208 (IME). sub_8029C08 ORs the "object's" byte at +0xE (0x04000216, unused I/O) into the record that fifo_pushRecord16_8009FF8 stores at 0x03002180. |
| open bus | sub_8031C7C | r0 = 0x04000208. battle_calcRelPosToTarget_803172E reads the words at 0x0400020C-0x04000218 (unused I/O) into dword_200F3E0. |
| free stack | CutsceneCmd_write_or_offset_ow_player_fixed_anim_select_8037dac | r7, the script pointer, is 0x03007DA8 = SP - 4. The original's `push {lr}` and ReadMapScriptByte's `push {r7, lr}` write the script bytes before it reads them (it reads the pushed LR's 0); the port's frames are not the original's (r4 = 0x11, and 0x0200ACE2 differs). |
| free stack | battle_spawnAndInitObject0x33_80CA686 | r5 = 0x03007D10, SP = 0x03007D7C. BasicBattleObjectInitialize copies the alliance halfword from r5 + 0x16, where object_spawnType3's frame had just left 0x0203 (the high half of the new object's pointer). The port leaves 0 there. |

mGBA reads 12 unused I/O halfwords as 0, not open bus (0x066, 0x06A, 0x06E, 0x076, 0x07A, 0x07E, 0x086,
0x08A, 0x136, 0x142, 0x15A, 0x206; checked by reading every unused register at several points of
the instruction stream). The host reads them as 0 too, so they are not counted. Write-only registers
are open bus in mGBA 0.10.2.

Limits: only differing states are reclassified, so a state that differs for some other reason and
also makes such a load would be hidden; the function's other states still count. The free-stack
check sees entry registers only, not pointers loaded from memory.

Result: the full verify verify-20261010-195836 (8065daa: all 1,435 readable functions, 16 routes)
passed. The oracle compared 1,293 of 1,434 functions, 0 differed, and these same 10 states were
explained; difftests and lockstep passed (16/16 routes identical). The verdict is "passed, but some
functions were never compared": 70 functions that no check compared (74 in the previous run). Such a
verify names the functions (`--function NAME` for each of tools/funcindex.py's
`readable_functions()`). With no `--function`, the difftests run nothing and the oracle tests every
lifted function.

## Checkpoint verify 2026-10-11: a fourth class, and three real differences

verify-20261011-170143 (all 1,971 readable functions, 16 routes, battle sweep) passed both difftests;
the oracle found 4 functions differing. Three were real and are taken out (8e00a34):
battle_processCollisionEvent_80C5C40 (r4 not restored), battleObject_runFirstTurnPhase_80C08EE
(a u16 countdown where the original's is 32-bit and signed), classifyActiveChipElements_8028E4C (tests
a callee's leftover Z where the original tests r0).

The fourth, dispatchHandlerById_8010D58, had r0 = 0x76, a handler id past its five-entry table. On
mGBA the original jumped through the word 0x1D8 bytes past the table, which happened to land in code
and return; the port's lifted original cannot run that state at all (out of domain: no entry at that
address), and the readable switch calls nothing. **Handled by class:** `oracle.py` now also counts a
differing state of a readable function as out of domain when the port's lifted original cannot run it
either (`difftest -G` with `ORACLE_LIFTED=1` on that one snapshot says "out of domain"), the rule the
difftest applies to random inputs the original cannot run. Not with `--lifted`. Of the four states
above it explains this one and none of the other three.

## Full verify 2026-10-12: one difference, IWRAM data relocated to ROM

After the installs of 10-11/12 (old-prompt sets 12-25, typed sets 81-91), the full verify
verify-20261012-015023 (bd5770c: all 2,346 readable functions, 16 routes) passed both difftests and
lockstep; the oracle found one differing state, in battle_objectDispatchPanelChange_80C6CFC. Not the
readable code: its lifted original (`ORACLE_LIFTED=1`) differs the same way. The random r2 is
0x03007DA0 (a stack address), which object_setPanelType stores as panel type 0xA0;
_object_updatePanelParameters then ORs in word 0xA0 of its 13-entry table at 0x03007924, i.e. the
word at 0x03007BA4. On the GBA that is IWRAM (0 in this state); the lifted code reads IWRAM data from
its ROM source (`IWRAM_ROM`), where 0x081D80A4 holds 0x6158, so the panel flags byte is 0xF8 instead
of 0xA0 (0x02039C14). Real calls pass a panel type of 0-12 (a byte from the object). Same class as
sub_802C5E6/sub_802C646 above, reached through an index past the table. **Handled by class:**
`oracle.py` now also counts a differing state of a readable function as out of domain when the
port's lifted original, run on that snapshot (`ORACLE_LIFTED=1`), differs from the original in
exactly the same bytes and registers: the difference is then the lifted code's or the harness's, and
the readable code matches its lifted original there. Not with `--lifted`.

The same verify's predecessors found two real (if out-of-game) differences, both fixed: Sio32IDInit
(a shift by 32 or more is 0 on the ARM, not count mod 32: bb36704) and
battle_objectDispatchPanelChange_80C6CFC itself (it switched on the low byte of r2: bd5770c).
