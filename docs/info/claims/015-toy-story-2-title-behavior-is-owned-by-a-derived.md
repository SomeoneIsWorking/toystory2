---
id: C015
kind: claim
status: holds
created: 2026-08-22
tags: architecture,runtime
depends: game/core/toystory2_runtime.cpp#ToyStory2Runtime, game/core/main.cpp#main
---

## Claim

Toy Story 2 title behavior is owned by a derived process-lifetime ToyStory2Runtime: boot dispatch and RE-10 override installation no longer occupy GameHooks, while immutable measured facts remain behind the bounded LegacyGameRuntimeAdapter.

## Evidence

Clang build compiled seam, port and oracle boundary against psxport 7f5d3f13; CTest passed 7/7 including format/tidy and forced-negative oracle controls; direct headless retail boot armed ts2-field and reached the unchanged emitted FMV BIOS A0:0x25 boundary.

## What would falsify it

a shipping path invokes legacy GameHooks bootInit/registerOverrides again, Core lacks the installed derived runtime, or the same retail boot no longer reaches the established FMV boundary
