---
id: C014
kind: claim
status: holds
created: 2026-08-22
tags: overlays,fmv,boot
depends: tools/overlay_map.py#loader_contract, docs/info/exe-identity.txt
reconfirmed: 2026-08-25 00:53:16
verified_at: 2026-08-25 00:53:16
---

## Claim

Toy Story 2 has 22 proven loaded code modules: 21 plain LEVEL-family modules totaling 245,148 bytes,
plus the mixed 510,960-byte `FMV/FMV.BIN` module loaded at `0x800D5D20` and entered at
`0x800D6628` (`file+0x908`).

## Evidence

`tools/overlay_map.py --check` derives the module inventory and exact load/entry addresses from the
identity-checked retail executable and disc corpus. It verifies calls `0x8003EEAC -> 0x80082508` and
`0x8003EEC4 -> 0x800D6628`, and verifies that the FMV entry begins with the retail function prologue
word `0x27BDFF10`. The 22-module census is preserved as executable/media evidence; it is no longer a
generated-code emission denominator.

## What would falsify it

An identity-matching retail disc has a different complete LEVEL-family census, or the retail bytes
or call flow no longer place and enter FMV at the measured addresses.
