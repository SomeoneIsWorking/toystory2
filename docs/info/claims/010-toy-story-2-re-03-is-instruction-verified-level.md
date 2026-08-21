---
id: C010
kind: claim
status: holds
created: 2026-08-21
tags: overlays,memory-map,loader
depends: tools/overlay_map.py#loader_contract, game/core/game_config.cpp#g_ts2_cfg
reconfirmed: 2026-08-21 03:41:57
verified_at: 2026-08-21 03:41:57
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
