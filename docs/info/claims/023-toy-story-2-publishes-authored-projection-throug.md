---
id: C023
kind: claim
status: holds
created: 2026-08-26
tags: render,projection,re07,widescreen,interpolation
depends: game/core/game_config.cpp#kSetGeomOffset
reconfirmed: 2026-08-26 22:44:44
verified_at: 2026-08-26 22:44:44
---

## Claim

Toy Story 2 publishes authored projection through retail SetGeomOffset 0x80083CD4 and SetGeomScreen 0x80083CF4, and the shipping title HLE preserves their guest effects while recording ProjParams on the same Core

## Evidence

Identity-checked SLUS_008.93 exact leaf bodies write CR24/25/26; graphics init 0x8003A650 calls them with 256/120/160. verify_projection_publication.py passes 6/6 positive/mutation/refusal cases. toystory2_projection_boundary passes 3/3 tests and 19 checks for exact registration, GPR/GTE effects, and same-Core geom validity. An exact-`dbdb2baf` product trace reaches both handlers four times, first at field 1 with the same values, and reports zero ABI violations. No native-producer consumption is claimed.

## What would falsify it

A verified SLUS_008.93 byte read changes either leaf/call/value; either verifier or boundary test fails; the HLE window admits a non-projection title function; or a serialized live trace shows calls bypass the installed handlers or record different projection values

## Re-confirmed 2026-08-26 22:44:44

Exact dbdb2baf verifier passed 6/6, hermetic boundary passed 3/3 and 19 checks, and serialized product tracing reached both handlers four times from frame 1 with 256/120/160 and zero ABI violations.
