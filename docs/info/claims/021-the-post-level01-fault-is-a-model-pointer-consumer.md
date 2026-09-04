---
id: C021
kind: claim
status: holds
created: 2026-08-24
tags: models,delay-slot,binary-analysis
depends: docs/info/exe-identity.txt
reconfirmed: 2026-08-26 21:40:52
verified_at: 2026-08-26 21:40:52
---

## Claim

The former post-LEVEL01 model-pointer fault was caused by misclassifying a branch-delay-slot address
as a function entry, which skipped the retail model-table clear increment and left slot 9 stale.

## Evidence

Retail function `0x80041FDC` clears 128 model-pointer slots. The increment at `0x80041FFC` is the
delay-slot instruction of the loop branch, not an independent entry. Earlier writer watches observed
slot 9 transition from stale `0x8013B770` to zero and then reload to `0x8012ED8C` when the complete
retail loop executed; the same bounded route continued through field 10,303 without the prior
unmapped read. The static-code misclassification and all generated output derived from it have been
removed.

## What would falsify it

Instruction-level execution of the identity-matching retail loop fails to clear all 128 slots, or a
trace shows slot 9 becomes stale through a different writer after a complete clear and reload.
