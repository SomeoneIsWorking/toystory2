---
id: 17
title: post-LEVEL01 model-pointer slot contains payload halfwords interpreted as 0xEDA4F893
status: resolved
symptom: deeper retail route fails read16 at 0xEDA4F893 through resident model consumer 0x800426E0
tags: render,models,pointers,re16
created: 2026-08-24
updated: 2026-08-24
---

## Classified cause

This is not a missing host hardware mapping or relocation special case. Resident `8002518C` selects
package index 9 from the model table at `0x800C7268`, uses model index 0, reads the nested pointer at
`package + 8`, and passes it to `800426E0`; that function immediately dereferences it. The table slot
still held `0x8013B770` after its arena was reused, so payload bytes `93 F8 A4 ED` at package+8 became
the wild pointer `0xEDA4F893`.

The stale slot came from a static-recompiler partition defect. Retail reset `80041F38` loops across
128 table entries; its `bnez` at `0x80041FF8` has `addiu a1,a1,4` at `0x80041FFC` as the delay slot.
Mixed-data words in two FMV overlays coincidentally decode as `jal 0x80041FFC`, and `overlay_funcs()`
promoted that structurally impossible delay-slot address to a function root. The generated parent
ended before the delay slot, emitted no increment, and wrote zero to `0x800C7268` 128 times. Slots
1..127 remained stale. Disc/DMA later overwrote the old arena exactly as the guest allocator permits.

## Resolution and verification

The generic emitter now rejects an overlay-discovered resident target when the preceding retail
instruction is a branch/jump: such a target is executed as that instruction's delay slot and cannot
also be a function entry. No game address, pointer substitution, skipped access, or generated-C edit
is involved. The whole-pipeline RED control ran its delay-slot increment once instead of three times;
it now runs 3/3, and the direct emitter suite passes 48/48 with Clang.

Toy Story 2 regenerates with 360 roots / 884 resident functions (five impossible delay-slot roots
removed), no `gen_func_80041FFC`, and an intact r5 increment in `gen_func_80041F38`. The bounded live
writer trace then observes slot 9 load `0x8013B770`, a later reset to zero, and a fresh load
`0x8012ED8C`; execution continues through field 10,303 with no fatal and no recompilation miss. The
former `0xEDA4F893` terminal boundary is crossed.

## Evidence

`scratch/logs/frame-fence-final.log` contains the old exact register/byte diagnostic;
`scratch/logs/re16-reset-fixed.log` contains the corrected slot-9 sequence and continuation.
Reproducible static decompiles are under
`scratch/decomp/re16-{pointer-chain,model-table-producer,model-relocator}.c`. Claim C021 records the
falsifier; RE-16 records the completed dependency and next honest frontier.
