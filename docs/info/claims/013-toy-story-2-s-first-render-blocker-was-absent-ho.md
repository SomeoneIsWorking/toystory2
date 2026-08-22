---
id: C013
kind: claim
status: holds
created: 2026-08-22
tags: render,vblank,host-turn,recomp
depends: game/sync/field_clock.cpp#ts2_field_clock_install, tools/overlay_map.py#loader_contract
---

## Claim

Toy Story 2's first render blocker was absent host field delivery, not missing geometry: after graphics init 0x8003A650 arms exact registered VBlank callback 0x80039D60 at the GPU field rate, live headless boot presents non-black legal and ESRB frames and reaches FMV.

## Evidence

Exact decompile chain: 0x8003A650 registers 0x80039D60; 0x80039D60 increments gp+0x7FC and services 0x800A10F8 via 0x80021028; 0x8003FA68 waits on both. Before the seam: 30-second watchdog at 0x8003FA68 and zero presents. After the seam: presents 30/120 are 15.79% non-black and present 900 is 33.58%, with retail legal/ESRB imagery, followed by FMV execution.

## What would falsify it

an independent reference trace shows the title does not invoke 0x80039D60 once per hardware field, or the same retail input with the seam no longer changes zero presents into non-black frames
