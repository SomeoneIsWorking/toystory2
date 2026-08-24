---
id: C014
kind: claim
status: holds
created: 2026-08-22
tags: recomp,overlays,fmv,boot
depends: tools/recomp_substrate.py#measure, tools/overlay_map.py#loader_contract, game/recomp_seeds.json
reconfirmed: 2026-08-24 20:19:56
verified_at: 2026-08-24 20:19:56
---

## Claim

Toy Story 2 has 22 proven loaded code modules: the original 21-module 245,148-byte plain-overlay census plus the mixed 510,960-byte FMV/FMV.BIN module loaded at 0x800D5D20 and entered at 0x800D6628=file+0x908; the generated substrate executes FMV through path parsing and into the resident renderer.

## Evidence

overlay_map.py --check exact-calls 0x8003EEAC->0x80082508 and 0x8003EEC4->0x800D6628, verifies retail FMV file+0x908 begins 0x27BDFF10, and passes 10/10 selftest. recomp_substrate emits 365 roots->889 resident functions and 22 modules->309 functions across 75 TUs after the proven LEVEL01 entry was added. A bounded retail run executes generated FMV functions, completes the A0:0x25 path parser, renders the stable title, executes LEVEL01 entry 0x800D12C4 and reaches the later model-pointer boundary in C021.

## What would falsify it

retail bytes or call flow no longer place/enter FMV at the measured addresses, the 22-module emission denominator changes, or a clean headless run fails before FMV for a newly introduced reason

## Re-confirmed 2026-08-22 18:46:27

After removing the obsolete resident diagnostic reentry seed, recomp_substrate selftest reverified the complete 22-module denominator and emitted 365 resident roots to 889 functions plus 22 overlays to 308 functions; all five controls passed and Toy Story Clang consumer CTest passed 9/9.

## Re-confirmed 2026-08-22 19:10:49

Pinned/build-checked psxport 57a17a14; recomp_substrate retained the exact 365-to-889 resident and 222-to-308 overlay denominators across 22 modules/75 TUs, and the bounded retail route executes FMV through A0:0x25 and the resident renderer table to RenderQueue capacity.

## Re-confirmed 2026-08-22 19:15:01

Post-commit 3b17154 fresh emission still covers 365 resident roots and 222 roots across 22 modules; authoritative CTest passes 9/9.

## Re-confirmed 2026-08-22 19:27:36

The current 22-module substrate retains exact 365-to-889 resident and 222-to-308 overlay denominators; the bounded retail route executes FMV, renders the stable title, loads LEVEL01 in the proven arena and reaches target 0x800D12C4.

## Re-confirmed 2026-08-22 19:46:56

Pinned d2266f4b recomp_substrate selftest retains the exact 365-to-889 resident and 222-to-308 overlay denominators across 22 modules/75 TUs; bounded retail executes FMV, renders the title, loads LEVEL01 and reaches 0x800D12C4; CTest passes 10/10.

## Re-confirmed 2026-08-24 after the proven LEVEL01 entry seed

The shipping substrate selftest passes 5/5 and emits 365 resident roots to 889 functions plus 22
modules to 309 functions. The one-function module increase is the classified `0x800D12C4` entry in
C020, not a changed module census. The Clang build and complete 11/11 CTest gate pass.

Current recorded framework pin bc8c8897 retains the same 22-module / 309-function emission under an
explicit Clang build and complete 11/11 CTest gate; no runtime launch was used for the pin migration.

## Re-confirmed 2026-08-24 20:19:56

After 456a31f, recomp_substrate --selftest verified the 22-module denominator and emitted 309 overlay functions; omission control failed as required.
