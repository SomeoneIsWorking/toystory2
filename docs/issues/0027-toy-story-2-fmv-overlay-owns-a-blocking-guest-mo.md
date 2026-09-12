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
Ghidra decompilation and exact retail instructions show that 0x800D7088 performs the entire STR open/demux/MDEC/upload/display loop and calls VSync at return PC 0x800D7590 once per movie frame. Its only direct caller is 0x800D6628. psxport `Fmv::play` is a native blocking movie owner, but it returns frame count and does not expose the guest contract's playback-mode skip result; do not substitute it until the title seam preserves that outcome.

### Live boundary (2026-09-12)

After the exact LEVEL00 RAW transaction returned through bounded Lightrec execution, the next strict
front-end call reached FMV entry `0x800D6628` and refused after 46 cycles: no active code-image identity
covered that address. The title now has a shared-slot observer that authenticates the FMV source and
transferred bytes, retires MEMORY, invalidates stale translations, and publishes an FMV generation.
That owner passes synthetic identity transitions and a bounded retail run published authenticated
FMV generation 4. The run next logged `CdRead(1 sectors) with NO Setloc` and exhausted a strict guest
call at `0x800940F4`; their relationship is not yet established. This precedes the independent
movie-loop ownership above; the run completed no frame.
