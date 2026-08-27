---
id: 27
title: Toy Story 2 FMV overlay owns a blocking guest movie loop
status: investigating
symptom: FMV/FMV.BIN function 0x800D7088 decodes and presents every STR frame inside one guest call and directly calls linked VSync 0x80088628 once per movie frame
state_items: S003,S007
tags: frame-loop,vsync,fmv,re18
created: 2026-08-27
updated: 2026-08-27
---

## Root cause


## What was tried / dead ends


## Resolution

### Note (2026-08-27)
Ghidra decomp and generated source agree that 0x800D7088 performs the entire STR open/demux/MDEC/upload/display loop and calls VSync at return PC 0x800D7590 once per movie frame. Its only direct caller is selector 0x800D6628. psxport Fmv::play is a native blocking movie owner, but it returns frame count and does not expose the guest contract's playback-mode skip result; do not substitute it until the title seam preserves that outcome.
