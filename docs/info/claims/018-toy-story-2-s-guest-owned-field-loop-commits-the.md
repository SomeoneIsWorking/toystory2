---
id: C018
kind: claim
status: holds
created: 2026-08-22
tags: render,frame-fence,boot
depends: game/sync/field_clock.cpp#field_turn, psxport.pin
reconfirmed: 2026-08-26 22:44:43
verified_at: 2026-08-26 22:44:43
---

## Claim

Toy Story 2's host-owned field turn commits the captured render queue once per measured display field
without increasing `RQ_MAX`.

## Evidence

Recorded pre-fix evidence had 6,613 flushes, 5,518 re-emits, no commits, and an accumulated
`65,337 + 1,210` entries above the 65,536 cap. The corrected bounded retail trace reached 8,320
commits with a maximum captured field of 3,096 and no overflow; inspected 960x720 presents 1,500 and
2,100 were coherent title frames. `game/sync/field_clock.cpp` retains the field-turn ownership that
fixed the accumulation. These recorded measurements remain historical runtime evidence and are not
treated as current dynarec execution certification.

## What would falsify it

The title field callback stops using the shared presentation fence, a same-input trace overflows in
one field, or `RQ_MAX` is raised to hide renewed cross-field accumulation.
