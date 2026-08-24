---
id: C018
kind: claim
status: holds
created: 2026-08-22
tags: render,frame-fence,boot
depends: game/sync/field_clock.cpp#field_turn, tools/verify_frame_fence.py#classify_post_fix, tools/verify_frame_fence.py#classify_title, psxport.pin
reconfirmed: 2026-08-22 19:46:57
verified_at: 2026-08-22 19:46:57
---

## Claim

Toy Story 2's guest-owned field loop commits the shared captured render queue once per measured display field and renders a stable retail title without increasing RQ_MAX

## Evidence

tools/verify_frame_fence.py classifies real A/B logs: pre-fix 6,613 flushes, 5,518 re-emits, no
commits, max flush 1,231, accumulation 65,337+1,210>65,536. The current pinned-framework run advances
through the seeded LEVEL01 entry for 8,320 commits, max flush 1,472, max captured field 3,096 and no
overflow. Its 960x720 presents 1,500 and 2,100 pass blue/yellow/red/green title color-family checks;
real present 900 and 2,400 controls are rejected. The post-fix trace must also reach C021's exact
model-pointer consumer/address pair, so an unrelated read16 cannot certify the route.

## What would falsify it

The title field callback stops calling the neutral shared presentation fence, directly couples the
title to a temporal decorator, a same-input trace overflows or reaches the generic capacity in one
field, the real title discriminator rejects the title or accepts the early/transition controls, or RQ_MAX
is raised to hide the symptom.

## Re-confirmed 2026-08-22 19:46:57

Pinned d2266f4b neutral-frame-fence verifier accepts 3,210 commits with max flush 1,472 and max captured field 2,943 under unchanged RQ_MAX, accepts title presents 2,100/2,400, rejects real early/transition captures, and reaches 0x800D12C4; CTest passes 10/10.

## Re-confirmed 2026-08-24 after RE-14's seed advanced the route

The same fence mechanism carries a DEEPER run: 8,320 commits, max captured field 3,096, still no overflow, title now measured at presents 1500/2100 with 900/2400 as the transition/black controls. The commit counts moved because the guest route advanced past the seeded LEVEL01 entry (C020), not because the fence changed; the verifier's expectations were updated to the moved boundary in the same change.

The exact current trace gate and its 12/12 selftest controls pass; the Clang build and complete 11/11
CTest gate also pass.

## Framework-policy migration 2026-08-24

Current pin bc8c8897 moves guest-VRAM picture ownership behind a required runtime policy. This title
is legacy-backed, and its adapter projects the already-verified static true answer while no native
producer exists. The Clang port/oracle build and 11/11 CTest gate pass. No runtime launch was made, so
the 8,320-commit visual evidence above remains attributed to d2266f4b.
