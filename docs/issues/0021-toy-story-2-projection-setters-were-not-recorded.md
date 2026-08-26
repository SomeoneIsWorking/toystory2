---
id: 21
title: Toy Story 2 projection setters were not recorded for native consumers
status: resolved
symptom: GameConfig HLE is empty, so retail SetGeomOffset/SetGeomScreen update only GTE control registers and ProjParams remains invalid for widescreen, interpolation camera state, or native rendering
tags: render,projection,widescreen,interpolation,native-renderer,re07,root-cause
created: 2026-08-26
updated: 2026-08-26
---

## Classified cause

The guest already publishes projection through linked libgte leaves at 0x80083CD4 and 0x80083CF4, but Toy Story 2 declared no platform-HLE window or projection bindings. Their recompiled bodies updated CR24/CR25/CR26 only; psxport therefore had no authored projection record for a later native camera or title-owned guest-widescreen plan.

## Resolution

The identity-checked retail executable proves both exact leaf bodies and graphics-init calls with (OFX,OFY,H)=(256,120,160). `game/core/game_config.cpp` binds only [0x80083CD4,0x80083D00), SetGeomOffset, and SetGeomScreen; every unrelated HLE field remains zero. `tools/verify_projection_publication.py` checks retail bytes, calls, constants and bindings and passes 6/6 positive/mutation/refusal classes. `toystory2_projection_boundary` passes 3/3 tests and 19 checks that the installed handlers preserve guest GPR/GTE effects and record a valid projection on the same Core. An exact-`dbdb2baf` product trace reaches each installed leaf four times, first on field 1 with `a0/a1=(256,120)` and `a0=160`, and reports zero ABI violations. No widescreen policy, interpolation, or native producer is enabled by this milestone.
