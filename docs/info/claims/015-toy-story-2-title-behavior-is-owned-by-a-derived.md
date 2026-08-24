---
id: C015
kind: claim
status: holds
created: 2026-08-22
tags: architecture,runtime
depends: game/core/toystory2_runtime.cpp#ToyStory2Runtime, game/core/toystory2_runtime.h#ToyStory2Runtime, game/core/game_config.cpp#legacy::measuredConfig, game/core/main.cpp#main
reconfirmed: 2026-08-25 00:53:16
verified_at: 2026-08-25 00:53:16
---

## Claim

Toy Story 2 title behavior is owned by a derived process-lifetime ToyStory2Runtime: boot dispatch and RE-10 override installation no longer occupy GameHooks, while immutable measured facts remain behind the bounded LegacyGameRuntimeAdapter.

## Evidence

Clang build compiled seam, port and oracle boundary against psxport 7f5d3f13; CTest passed 7/7 including format/tidy and forced-negative oracle controls; direct headless retail boot armed ts2-field and reached the unchanged emitted FMV BIOS A0:0x25 boundary.

## What would falsify it

a shipping path invokes legacy GameHooks bootInit/registerOverrides again, Core lacks the installed derived runtime, the title is treated as a direct runtime while it still derives `LegacyGameRuntimeAdapter`, guest-VRAM ownership bypasses that adapter without a measured second ownership state, or the same retail boot no longer reaches the established FMV boundary

## Re-confirmed 2026-08-24 against psxport bc8c8897

Toy Story 2 is explicitly LEGACY-BACKED, not direct: `ToyStory2Runtime final` derives
`LegacyGameRuntimeAdapter`, whose new required `guestVramIsPicture(const Game&)` implementation is the
sole projection of `measuredConfig.preserveVramBackdrop`. The verified route has one ownership answer:
true, because the title still has no native producer and its field loop presents guest VBlank,
DrawOTag and upload-only VRAM content. No redundant title override was added. An explicit Clang build
of the port and oracle boundary plus the complete 11/11 CTest gate passed against fetchable framework
commit `bc8c8897`; no runtime launch was used for this API migration.

## Re-confirmed 2026-08-24 20:19:56

After 456a31f, fresh Clang port/oracle build and CTest 11/11 passed with ToyStory2Runtime legacy adapter ownership and no direct temporal coupling.

## Re-confirmed 2026-08-24 at current pin 9c2e3f1c

The product and oracle targets compiled with Clang and the complete 11/11 CTest gate passed with the
same process-lifetime `ToyStory2Runtime` ownership. No runtime launch was performed.

## Re-confirmed 2026-08-25 00:53:16

At pushed framework pin aa0b2067, a clean Clang build compiled the shipping port and oracle boundary with the same process-lifetime ToyStory2Runtime ownership and passed 12/12 CTest; no game launch was used.
