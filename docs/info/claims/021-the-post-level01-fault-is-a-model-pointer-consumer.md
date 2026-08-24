---
id: C021
kind: claim
status: holds
created: 2026-08-24
tags: render,models,re16,pointers
depends: game/recomp_seeds.json#overlay_seeds, tools/verify_frame_fence.py#classify_post_fix
---

## Claim

The post-LEVEL01 fault at `0xEDA4F893` is a guest model-pointer dereference by resident function
`0x800426E0`, not an unimplemented PSX hardware address; the unresolved question is which producer
left the model slot stale, overwritten, unrelocated, or selected with the wrong index

## Evidence

The exact post-seed trace records `last-fn-entered=0x800426E0`, `a0=t0=0xEDA4F893`, and
`FATAL: UNMAPPED RAM read16 @ 0xEDA4F893`. `tools/verify_frame_fence.py` now requires both the exact
consumer and exact address, and rejects the same address through a different consumer plus the same
consumer with a different address. Fresh Ghidra decompilation of verified `SLUS_008.93` resolves the
chain `8007A9E8 -> 8007B254 -> 8002A070 -> 80025E44 -> 8002518C -> 800426E0`:
`8002518C` indexes the model-package table at `0x800C7268`, then loads a nested model pointer from
`package + 8 + index*4` and passes it to `800426E0`, whose first operation dereferences it. The table
writer/loader `80041A10` stores the loaded package base and calls relocator `800418C0`, which converts
the nested package offsets to absolute pointers. An earlier RAM snapshot shows valid absolute nested
pointers in the same table; the terminal trace instead shows coordinate-like signed halfwords at
`v0=0x8013B770`, including bytes `93 F8 A4 ED` that form the wild pointer. Address
`0xEDA4F893` is outside PSX main RAM and hardware windows, so mapping or ignoring it would conceal
corrupt guest object/model provenance.

## What would falsify it

A corrected decompile showing `800426E0` consumes a hardware address rather than a model pointer; a
trace reaching the same exact fault without `8002518C` selecting from `0x800C7268`; or a write trace
showing `0xEDA4F893` is a valid relocated model address rather than payload bytes interpreted as one
