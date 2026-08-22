---
id: C014
kind: claim
status: holds
created: 2026-08-22
tags: recomp,overlays,fmv,boot
depends: tools/recomp_substrate.py#measure, tools/overlay_map.py#loader_contract, game/recomp_seeds.json
reconfirmed: 2026-08-22 19:10:49
verified_at: 2026-08-22 19:10:49
---

## Claim

Toy Story 2 has 22 proven loaded code modules: the original 21-module 245,148-byte plain-overlay census plus the mixed 510,960-byte FMV/FMV.BIN module loaded at 0x800D5D20 and entered at 0x800D6628=file+0x908; the generated substrate executes FMV through path parsing and into the resident renderer.

## Evidence

overlay_map.py --check exact-calls 0x8003EEAC->0x80082508 and 0x8003EEC4->0x800D6628, verifies retail FMV file+0x908 begins 0x27BDFF10, and passes 10/10 selftest. recomp_substrate emits 365 roots->889 resident functions and 222 roots->308 functions across 22 modules/75 TUs. A bounded retail run executes generated FMV functions, completes the A0:0x25 path parser and reaches the resident renderer before the later RenderQueue boundary.

## What would falsify it

retail bytes or call flow no longer place/enter FMV at the measured addresses, the 22-module emission denominator changes, or a clean headless run fails before FMV for a newly introduced reason

## Re-confirmed 2026-08-22 18:46:27

After removing the obsolete resident diagnostic reentry seed, recomp_substrate selftest reverified the complete 22-module denominator and emitted 365 resident roots to 889 functions plus 22 overlays to 308 functions; all five controls passed and Toy Story Clang consumer CTest passed 9/9.

## Re-confirmed 2026-08-22 19:10:49

Pinned/build-checked psxport 57a17a14; recomp_substrate retained the exact 365-to-889 resident and 222-to-308 overlay denominators across 22 modules/75 TUs, and the bounded retail route executes FMV through A0:0x25 and the resident renderer table to RenderQueue capacity.
