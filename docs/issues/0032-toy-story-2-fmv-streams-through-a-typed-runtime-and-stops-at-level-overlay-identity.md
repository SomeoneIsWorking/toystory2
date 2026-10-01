---
id: 32
title: Toy Story 2 plays its four intro movies through a typed GameRuntime, and stops at LEVEL overlay code-image identity
status: open
symptom: After the movies, `0x8007C278` loads a LEVEL overlay and the first jump into it faults with `guest address 0x800D1DBC resolves to zero or multiple active code images`
tags: cd,str,fmv,typed-runtime,dma-callback,intro-movies,overlay-identity
state_items: S002,S003
created: 2026-10-01
updated: 2026-10-01
---

## Decision

TS2 is migrated to a typed `ToyStory2Runtime` (no `LegacyGameRuntimeAdapter`, no `GameConfig`, no
`game_hooks.cpp`); the framework refusal in `cd_ready_delivery.cpp` is not "fixed" because it is
correct for legacy runtimes. Every fact is typed in `game/core/guest_facts.h`.

## The chain that kept the movie from finishing (all measured)

1. A RAM-spinning guest never received the CD interrupt: psxport issue 0145 (segments now end at the
   next CDC deadline). Result: 6 of 6 owed completions delivered, ring entries filled.
2. State `2` is written only by `0x80093E88`, which libcd registers as DMA channel 3's callback via
   `0x80091148` -> `0x8008888C` (libapi DMACallback). libapi's table is `[0x8009FD60 + 4*ch]`,
   scanned by libapi's DMA IRQ dispatcher `0x800890C4` (`lui 0x800a; addiu -0x2a0` at
   `0x80089110/14`, 4-byte stride at `0x8008916C`). The framework stands in for that dispatcher and
   acknowledges the DICR flag first, so it must call the table word itself:
   `PlatformHlePlan::dmaCallbackTable = 0x8009FD60` (the `0x8009EC9C` table is
   InterruptCallback's, whose slot 3 is the dispatcher; pointing at it delivered the dispatcher
   to an already-acknowledged flag and called nothing).
3. `[0x800CE148]` (the other guard of `0x80094B14`) is irrelevant on this path; the data-end
   callback is what posts the frame.
4. Each VSync in the FMV loop is a `FrameBoundary` exit. The native frame contract requires exactly
   one presentation fence per `stepFrame`, so a movie cannot be played inside one guest call:
   `OuterLoopPhase::introMovies` steps a `ResumableGuestCall` one display field per frame.
   `serviceDeferredDisplay` (resident `0x80021028`) is skipped while the movies own the display.

## Measured

Headless, silent, unpaced (`tools/headless_run.py --control-port`): the four movies return after
194, 41, 73 and 69 display fields (377 presented frames, each exactly one fence). Opened captures
show real MDEC-decoded frames (frame 190: blue light shafts over debris, 394,317 of 691,200
pixels non-black at 960x720; frame 300: the character illustration, 208,962 of 691,200). Ledger
polled live from the control channel at the last answer before the fault: 3,975 translated blocks,
1,500,854,459 executed instructions, 4,619 cache misses, 0 faults; 38,765,035 invalidations
(unexplained, see below).

## Open

- **Blocking: LEVEL overlay identity.** Only MEMORY and FMV have an image owner
  (`game/overlay/shared_slot_image.*`). The LEVEL module at `0x800D12C0` is loaded by
  `0x8007C278` but never authenticated or activated, so the first dispatch into it fails closed.
  This was previously unreachable (the port stopped at the FMV).
- The 38.7 million invalidations in 377 frames are not explained; most likely the per-field
  `SharedSlotImage`/CD read invalidation granularity. Not investigated.
- The loader and asset-decode calls (`0x8007BC74(10,0)`, the front-end poll, `0x8007C278`,
  `0x8003D88C`) are finite multi-slice initialization transactions inside one frame (up to 33M
  guest cycles), not yielding loads. CLAUDE.md's "loading must be asynchronous" is not met.
- Lost under the direct runtime: the `PSXPORT_TS2_CARD`/default memory-card path and the window title.
  The SIO acknowledge deadline is not included in the psxport segment cap.
- After the movies the front-end poll returned an entry event without showing a menu in this
  headless run (no input); whether that is retail's attract path is not established.
