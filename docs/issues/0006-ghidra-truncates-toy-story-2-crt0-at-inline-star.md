---
id: 6
title: Ghidra truncates Toy Story 2 crt0 at inline startup data
status: resolved
symptom: entry decompile at 0x80082D60 stops after BSS clear and does not expose stack, heap, libcInit, or gameMain
tags: boot,crt0,ghidra,instrument
created: 2026-08-21
updated: 2026-08-21
---

## Root cause

The crt0 interleaves control with startup data. It reaches InitHeap, reloads `ra`, calls gameMain, and
terminates at `break` `0x80082E08`, while the earlier `lw` at `0x80082DA4` reads the post-break word at
`0x80082E10` as stack data (`0x00200000`). Ghidra could not prove that boundary and truncated the entry
decompile.

## What was tried / dead ends

The 24-line Ghidra entry decompile proved only the BSS clear. Copying its apparent bounds into
`GameConfig` would have left the rest of the boot group unproved and is deliberately rejected.

## Resolution

`tools/verify_crt0.py` symbolically walks the real sequence, keeps post-break words as data, prints each
field's instruction chain, compares it to the shipping config, and gates mutation, malformed and real
cross-binary negatives.
