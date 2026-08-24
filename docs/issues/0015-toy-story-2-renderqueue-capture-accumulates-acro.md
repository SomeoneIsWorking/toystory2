---
id: 15
title: Toy Story 2 RenderQueue capture accumulates across fields until FPS60_RQ_MAX
status: resolved
symptom: Bounded retail boot reaches Fps60::rq_capture with 65337 captured plus 1210 and aborts above RQ_MAX 65536 after many guest DrawOTag flushes
tags: render,frame-fence,host-turn,queue,boot
created: 2026-08-22
updated: 2026-08-22
---

## Root cause

Toy Story 2 still runs its guest-owned frame loop. Its host field callback dispatched the exact guest
VBlank handler and then called lower-level `gpu_present(core)` directly. `RenderQueue::flush` captures
every submitted list into the active presentation capture. The title called no frame fence, so
ordinary draws and repeat scanouts from thousands of separate fields accumulated as one apparent
frame.

## What was tried / dead ends

Raising `RQ_MAX` was rejected by measurement. The pre-fix trace contains 6,613 flushes, 5,518 of them
explicit `reemit=1` scanouts of already-consumed content. Its largest individual flush is 1,231 items,
yet the unfenced accumulator reaches 65,337 before a 1,210-item flush crosses the generic 65,536 cap.
That rules out both a legitimate over-capacity Toy Story frame and a single runaway geometry producer.
Dropping repeat flushes was also rejected: repeated presentation is real guest behavior, and the shared
renderer must preserve it within the correct field lifetime.

## Resolution

### Resolution (2026-08-22)
ToyStory2 `field_turn` called lower-level `gpu_present` instead of the neutral presentation fence, so
the captured queue never rotated across fields. The title-owned one-field VBlank boundary now calls
`game->presentation.commit(core, 1)` without a temporal decorator; no queue cap changed and no
primitives are dropped. Real A/B and title-image discriminators are in
`tools/verify_frame_fence.py`.

The current pinned-framework retail trace records 8,320 field commits, a maximum individual flush of
1,472 and maximum captured field of 3,096, with no overflow. It renders stable 960x720 retail titles
at presents 1,500 and 2,100, executes the classified LEVEL01 entry at 0x800D12C4, then reaches the
independent model-pointer boundary in C021.
