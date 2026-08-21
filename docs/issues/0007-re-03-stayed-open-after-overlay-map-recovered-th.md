---
id: 7
title: RE-03 stayed open after overlay_map recovered the exact loader destinations
status: resolved
symptom: Toy Story 2 still treated 0x800D1000 as only a fitted overlay base and left overlaySlots empty even though overlay_map.py selftest passed at exact destinations
tags: overlays,loader,workflow,instrument
created: 2026-08-21
updated: 2026-08-21
---

## Root cause

The repaired `tools/overlay_map.py` result was never integrated as an RE milestone. Its selftest
checked destination recovery, but no claim or trusted instrument tied that result to the retail
loader call flow, no normal gate compared it to shipping configuration, and the frontier kept C003's
coarse 4 KiB fit as its last accepted evidence. The old size-only verdict also called a wider interval
"two-slot", overclaiming what capacity alone can prove.

## What was tried / dead ends

The old `0x800D1000` base fit remains useful independent corroboration, but it cannot prove a
non-page-aligned destination or simultaneous residency. Pairwise module sizes can disprove
co-residence in the real interval, but a widened interval alone can only make co-residence possible;
it cannot prove that two loads occur.

## Resolution

C010/I009 connect exact instruction pairs and the one-call path builder to the retail bytes, compare
the derived LEVEL/MEMORY placements with GameConfig and recompiler seeds, and force the opposite
`co-resident-possible` answer by mutating all real MEMORY/FMV destination pairs through the same
census path. RE-03 is now re-verified and normal CTest runs the verifier. RE-04 remains partial because
the located retail `(path,dest)` loader is not psxport's `(dest,lba,size)` hook ABI.

### Resolution (2026-08-21)
Resolved by C010/I009: exact retail instruction/call-flow proof now drives shipping config and seeds, normal CTest, and a mutated forced-opposite answer; RE-04 stays partial because the located loader ABI is incompatible with psxport cdFileLoad.
