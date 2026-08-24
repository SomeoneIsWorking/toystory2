---
id: 14
title: Renderer computed jump table is not lowered into dispatchable re-entries
status: resolved
symptom: Retail boot reaches computed renderer target 0x8001040C; a single re-entry seed advances once then misses sibling slot 0x800104E4 because emitted jump-table entries have no labels
tags: render,recompiler,jump-table,framework,boot
created: 2026-08-22
updated: 2026-08-22
---

## Root cause

The retail function at 0x800100E4 masks the selector with 31, scales it by 8, adds table base
0x800103EC and executes `jr t1`. All 32 entries are `j`+nop trampolines into the same function body.
The generic emitter emits a dynamic `rec_dispatch`, but does not discover/lower the table slots as
internal labels.

## Evidence

`tools/verify_render_reentry.py` exact-checks the table construction and all 32 retail entries. It
also parses the actual emitted resident switch, requires the same ordered 32-slot set, and has
negative controls for a missing sibling, an extra adjacent body, jalr/function-pointer dispatch, a
function prologue, and a non-table slot. A live seed for observed slot 0x8001040C reaches its body,
loops to the same dispatcher, and then fail-fasts on slot 0x800104E4 under the pre-fix emitter.

## Resolution

The generic fix landed in psxport `57a17a14`. It folds both `lui+addiu` and `lui+ori` immediate
bases, derives the exact case count from a contiguous low-bit index mask, rejects unaligned targets,
and emits all 32 targets as local labels. Toy Story 2 now pins and builds that commit. With the
diagnostic seed removed, the shipping verifier finds exactly the 32 retail slots, its current 13/13
positive and negative controls pass, and a bounded real-disc run has no miss at either 0x8001040C or
0x800104E4 before passing the resolved frame fence and LEVEL01 entry to C021's model-pointer
boundary. No game
renderer workaround or per-slot seeds remain.
