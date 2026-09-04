---
id: C017
kind: claim
status: holds
created: 2026-08-22
tags: render,binary-analysis,boot
depends: docs/info/exe-identity.txt
reconfirmed: 2026-08-26 21:40:52
verified_at: 2026-08-26 21:40:52
---

## Claim

Toy Story 2's resident renderer contains exactly 32 ordered internal jump-table destinations from
`0x800103EC` through `0x800104E4`.

## Evidence

The identity-checked retail executable constructs and indexes this 32-way table. Earlier bounded
retail traces crossed both formerly problematic destinations `0x8001040C` and `0x800104E4` before
reaching later independently identified title boundaries. This is retained binary and observation
evidence; the current runtime discovers these destinations from guest control flow without
title-authored dispatch metadata.

## What would falsify it

The identity-matching retail table construction yields a different ordered destination set, or an
instruction-level trace proves control cannot reach one of the identified slots.
