---
id: C010
kind: claim
status: holds
created: 2026-08-21
tags: overlays,memory-map,loader
depends: tools/overlay_map.py#loader_contract, game/core/game_config.cpp#g_ts2_cfg, game/recomp_seeds.json
reconfirmed: 2026-08-22 19:15:00
verified_at: 2026-08-22 19:15:00
---

## Claim

Toy Story 2 RE-03 is instruction-verified: LEVEL{,1,2,3}.BIN are alternative contents of one slot at 0x800D12C0, while BITS/MEMORY.BIN is co-resident at 0x800D5D20 and ends at 0x800E5470 with the caller frontier at 0x800E54F8.

## Evidence

tools/overlay_map.py --check over verified SLUS_008.93 and the size-verified retail flat corpus: Ghidra xrefs/decompile locate FUN_8003D88C -> path builder 0x80082870 -> one wrapper 0x8003DE9C -> loader 0x80082508; exact instructions form 0x800D12C0. The same caller loads bits\\memory.bin to 0x800D5D20 first on a compatible path. The 19,040-byte gap equals the largest level module and 5/10 LEVEL+LEVEL1 pairs exceed it. MEMORY.BIN is 63,312 bytes, its 11-word absolute prefix maps inside the exact placement, the loader preserves the sector-rounded tail, and the caller computes 0x800E54F8. Selftest passes 10/10 including an in-memory mutation that widens the next slot and forces the opposite co-residence-possible answer.

## What would falsify it

a retail call-flow decompile shows a second level wrapper/load on the same invocation, any level module is relocated rather than loaded raw, MEMORY.BIN does not use the exact CdlFILE size contract, or a changed verifier/config no longer passes the positive and forced-opposite gates

## Re-confirmed 2026-08-21 03:36:17

Re-verified 2026-08-21: overlay_map.py --check and 10/10 selftest pass against retail SLUS/corpus; clean Clang 22 build, CTest 4/4, and real zero-argument launcher refusal all pass with shipping slots 0x800D12C0 and 0x800D5D20.

## Re-confirmed 2026-08-21 03:41:57

Re-verified after strengthening the verifier: exact retail CdSearchFile/CdlFILE.size return chain, retry propagation, sector-tail copy arguments, and arena mask/bias ALU sequence pass; overlay_map 10/10, shipping --check, clean Clang 22 build, CTest 4/4, and real launcher RC3 remain green.

## Re-confirmed 2026-08-21

Post-landing overlay_map shipping check passed and selftest passed 10/10, including the forced co-resident-possible opposite.

## Re-confirmed 2026-08-21

Post-landing Clang CTest passed 6/6; overlay_map shipping gate and selftest remained green for the LEVEL and MEMORY slots while exactly 21 modules were emitted.

## Re-confirmed 2026-08-21

Post-landing Clang CTest 7/7 and overlay_map shipping/selftest gates passed; the game configuration change was documentation-only for the independently verified stock-libcd path.

## Re-confirmed 2026-08-22 12:40:38

2026-08-22 overlay_map.py --check and 10/10 selftest remain green; the strengthened contract additionally exact-verifies FMV load/entry at the same physical slot without changing the established LEVEL/MEMORY placement.

## Re-confirmed 2026-08-22 14:18:14

2026-08-22 runtime-inheritance migration: overlay_map_selftest passed all controls in the clean Clang scratch/build-runtime CTest suite; immutable overlay facts remained unchanged behind the legacy adapter.

## Re-confirmed 2026-08-22 18:46:27

After removing the obsolete resident diagnostic reentry seed, overlay_map shipping/selftest re-derived the two-slot contract and passed 10/10 including the forced co-residence opposite; Toy Story Clang consumer CTest passed 9/9.

## Re-confirmed 2026-08-22 19:15:00

Post-commit 3b17154 authoritative Clang CTest passes 9/9; recomp substrate selftest passes with the renderer split seed removed and the existing 22-module overlay model unchanged.
