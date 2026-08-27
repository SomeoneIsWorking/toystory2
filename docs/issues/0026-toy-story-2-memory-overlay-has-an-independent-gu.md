---
id: 26
title: Toy Story 2 MEMORY overlay has an independent guest-owned display loop
status: investigating
symptom: BITS/MEMORY.BIN function 0x800DEF6C contains an internal update loop and eleven direct calls to linked VSync 0x80088628
state_items: S003
tags: frame-loop,vsync,memory-overlay,re18
created: 2026-08-27
updated: 2026-08-27
---

## Root cause


## What was tried / dead ends


## Resolution

### Note (2026-08-27)
Generated ov_bits__memory_gen_800DEF6C contains eleven rec_dispatch calls to 0x80088628: eight around pad/display teardown and reinitialization at return PCs 0x800DF0C4..0x800DF114, plus three inside its UI state loop at 0x800E0820..0x800E0830. It also calls the measured field barrier 0x8003FA68 from its internal loop. This is not the resident FrameDriver and must become its own finite native state owner; successful guest VSync is forbidden.
