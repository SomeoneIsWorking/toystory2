---
id: C014
kind: claim
status: holds
created: 2026-08-22
tags: recomp,overlays,fmv,boot
depends: tools/recomp_substrate.py#measure, tools/overlay_map.py#loader_contract, game/recomp_seeds.json
---

## Claim

Toy Story 2 has 22 proven loaded code modules: the original 21-module 245,148-byte plain-overlay census plus the mixed 510,960-byte FMV/FMV.BIN module loaded at 0x800D5D20 and entered at 0x800D6628=file+0x908; the generated substrate executes FMV to its next honest boundary, BIOS A0:0x25.

## Evidence

overlay_map.py --check exact-calls 0x8003EEAC->0x80082508 and 0x8003EEC4->0x800D6628, verifies retail FMV file+0x908 begins 0x27BDFF10, and passes 10/10 selftest. recomp_substrate emits 365 roots->889 resident functions and 222 roots->308 functions across 22 modules/75 TUs. Live stack contains generated FMV functions 0x800D6628/0x800D7088/0x800D8FB0/0x800D8D9C before fail-fast A0:0x25.

## What would falsify it

retail bytes or call flow no longer place/enter FMV at the measured addresses, the 22-module emission denominator changes, or a clean headless run fails before FMV for a newly introduced reason
