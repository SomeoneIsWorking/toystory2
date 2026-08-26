---
id: C021
kind: claim
status: holds
created: 2026-08-24
tags: render,models,re16,pointers,recompiler,delay-slot
depends: tools/verify_model_table_reset.py#classify_runtime
reconfirmed: 2026-08-26 21:40:52
verified_at: 2026-08-26 21:40:52
---

## Claim

The retired post-LEVEL01 fault at `0xEDA4F893` was a guest model-pointer dereference by resident
function `0x800426E0`, not an unimplemented PSX hardware address. Its producer cause was a stale
model-package table entry: jal-shaped mixed overlay data falsely split reset `0x80041F38` from its
branch delay-slot increment at `0x80041FFC`, so the generated loop cleared slot 0 repeatedly instead
of advancing across all 128 slots.

## Evidence

The exact post-seed trace records `last-fn-entered=0x800426E0`, `a0=t0=0xEDA4F893`, and
`FATAL: UNMAPPED RAM read16 @ 0xEDA4F893`. `tools/verify_frame_fence.py` now requires both the exact
consumer and exact address, and rejects the same address through a different consumer plus the same
consumer with a different address. Fresh Ghidra decompilation of verified `SLUS_008.93` resolves the
chain `8007A9E8 -> 8007B254 -> 8002A070 -> 80025E44 -> 8002518C -> 800426E0`:
`8002518C` indexes the model-package table at `0x800C7268`, then loads a nested model pointer from
`package + 8 + index*4` and passes it to `800426E0`, whose first operation dereferences it. Fault
registers prove package index 9 and model index 0, selecting slot `0x800C728C` and nested pointer
address `0x8013B778`. The table
writer/loader `80041A10` stores the loaded package base and calls relocator `800418C0`, which converts
the nested package offsets to absolute pointers. An earlier RAM snapshot contains expected nested
pointer `0x8013B780`; the terminal trace instead shows coordinate-like signed halfwords at
package `0x8013B770`, including bytes `93 F8 A4 ED` at package+8 that form the wild pointer. Address
`0xEDA4F893` is outside PSX main RAM and hardware windows, so mapping or ignoring it would conceal
corrupt guest object/model provenance.

Writer watches distinguish the producer alternatives. Slot 9 is loaded once with `0x8013B770`;
disc/DMA later overwrites the entire old package prefix. At the reset, generated code writes zero to
`0x800C7268` 128 times and never advances r5. Retail words are `bnez v0,0x80041F90` at `0x80041FF8`
and delay-slot `addiu a1,a1,4` at `0x80041FFC`. Two FMV overlay payload words equal
`0x0C0107FF` (`jal 0x80041FFC`) by coincidence, which is the exact provenance of the false root.

After the generic emitter rejects resident targets that are structurally delay slots, the generated
loop retains the increment and no standalone `gen_func_80041FFC` exists. The RED whole-pipeline
control changes from one increment to 3/3. The real bounded writer trace changes slot 9 from
`0x8013B770` to zero, then loads fresh package `0x8012ED8C` and continues through field 10,303 with no
fatal or recompilation miss. `tools/verify_model_table_reset.py --check-log` gates the retail words,
generated loop, clear/reload ordering, distinct package, retired-fault absence, and progress
denominator.

## What would falsify it

A corrected decompile showing `800426E0` consumes a hardware address rather than a model pointer; a
writer trace showing slot 9 was cleared before arena reuse in the failing build; retail bytes showing
`0x80041FFC` is not the branch delay slot or does not increment a1; overlay evidence showing the
jal-shaped words are executable calls; or a corrected-reset trace that still reaches the exact
`0xEDA4F893` fault

## Re-confirmed 2026-08-25 00:17:49

2026-08-25: exact retail/reset partition evidence plus writer watches prove slot9 stale after false delay-slot root; generic fix restores clear/reload and direct bounded route reaches field 10,303 without fatal/miss. verify_model_table_reset selftest 7/7 and real-log check pass.

## Re-confirmed 2026-08-25 00:53:16

At pushed framework pin aa0b2067, verify_model_table_reset.py --check proved the retail branch delay slot and generated 128-entry reset agree; its saved real-log classifier observed slot 9 transition 0x8013B770 to zero to 0x8012ED8C and continuation through field 10303 without fatal/miss. Clean Clang build and 12/12 CTest passed without launching the game.

## Re-confirmed 2026-08-26 21:40:52

At pushed framework pin 54af32cb, verify_model_table_reset.py --check proved the retail delay slot and generated 128-entry reset agree; the saved real-log classifier still observes slot 9 clear/reload and continuation through field 10303; the clean Clang port/oracle build passed 13/13 CTest without a game launch.
