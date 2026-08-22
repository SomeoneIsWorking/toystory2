---
id: 14
title: Renderer computed jump table is not lowered into dispatchable re-entries
status: investigating
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

`tools/verify_render_reentry.py` exact-checks the table construction and all 32 entries, with negative
controls for jalr/function-pointer dispatch, a function prologue, and a non-table slot. A live seed for
observed slot 0x8001040C reaches its body, loops to the same dispatcher, and then fail-fasts on slot
0x800104E4.

## Resolution

Pending a generic framework computed-jump-table lowering fix. Do not seed all slots or add a Toy
Story renderer workaround.
