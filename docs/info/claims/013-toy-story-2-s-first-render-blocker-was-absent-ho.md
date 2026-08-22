---
id: C013
kind: claim
status: holds
created: 2026-08-22
tags: render,vblank,host-turn,recomp
depends: game/core/toystory2_runtime.cpp#ToyStory2Runtime, game/sync/field_clock.cpp#ts2_field_clock_install, tools/overlay_map.py#loader_contract
reconfirmed: 2026-08-22 17:53:35
verified_at: 2026-08-22 17:53:35
---

## Claim

Toy Story 2's first render blocker was absent host field delivery, not missing geometry: after graphics init 0x8003A650 arms exact registered VBlank callback 0x80039D60 at the GPU field rate, live headless boot presents non-black legal and ESRB frames and reaches FMV.

## Evidence

Exact decompile chain: 0x8003A650 registers 0x80039D60; 0x80039D60 increments gp+0x7FC and services 0x800A10F8 via 0x80021028; 0x8003FA68 waits on both. Before the seam: 30-second watchdog at 0x8003FA68 and zero presents. After the seam: presents 30/120 are 15.79% non-black and present 900 is 33.58%, with retail legal/ESRB imagery, followed by FMV execution.

## What would falsify it

an independent reference trace shows the title does not invoke 0x80039D60 once per hardware field, or the same retail input with the seam no longer changes zero presents into non-black frames

## Re-confirmed 2026-08-22 14:18:14

2026-08-22 runtime-inheritance migration: the direct shipping binary logged ToyStory2Runtime boot dispatch, armed ts2-field after graphics init, then reached the same emitted FMV call stack and honest BIOS A0:0x25 boundary; full Clang CTest was 7/7.

## Re-confirmed 2026-08-22 17:53:35

Reverified after ToyStory2Runtime inheritance move: current Clang build passes cpp_policy and the bounded retail headless route advances through visible-card field delivery, emitted FMV, all 15 first-path A0:0x25 parser calls and into the renderer boundary at 0x800104E4; this is impossible on the old zero-host-turn path.
