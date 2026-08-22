---
id: C017
kind: claim
status: holds
created: 2026-08-22
tags: render,recomp,boot
depends: tools/verify_render_reentry.py#check_generated, game/recomp_seeds.json, psxport.pin
reconfirmed: 2026-08-22 19:10:49
verified_at: 2026-08-22 19:10:49
---

## Claim

Toy Story 2's landed resident renderer substrate lowers exactly the retail 32-way internal jump table without per-slot seeds

## Evidence

Pinned and built psxport 57a17a14. tools/verify_render_reentry.py --selftest passes 10/10 positive/negative classes over the shipping generated switch, requiring exactly 32 ordered slots from 0x800103EC through 0x800104E4. A bounded real-disc log has neither former 0x8001040C nor 0x800104E4 dispatch miss and reaches RenderQueue capture capacity. The renderer diagnostic seed is absent.

## What would falsify it

The retail table construction or 32-slot set changes, generated output gains a missing or extra slot, a renderer slot seed returns, either former dispatch miss recurs, or the same-input live gate no longer reaches RenderQueue capacity.

## Re-confirmed 2026-08-22 19:10:49

Pinned/build-checked psxport 57a17a14; the shipping generated verifier passes all 10 classes with exactly 32 ordered internal slots and no renderer seed, while the bounded retail log has neither former dispatch miss and reaches RenderQueue capture capacity.
