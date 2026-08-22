---
id: C016
kind: claim
status: holds
created: 2026-08-22
tags: boot,fmv,bios,recomp
depends: tools/verify_fmv_boundary.py#analyze, psxport.pin
reconfirmed: 2026-08-22 18:15:55
verified_at: 2026-08-22 18:15:55
---

## Claim

Toy Story 2 retail FMV path parsing passes the shared BIOS A0:0x25 boundary on pinned psxport ad5cf802

## Evidence

Exact SLUS/FMV words prove 0x800D8E48 -> Sony leaf 0x80082E5C -> A0:0x25 and byte store at 0x800D8E50. tools/verify_fmv_boundary.py mutation controls reject changed call/leaf/store classes. A clean Clang build against pinned ad5cf802 produced a bounded real-disc trace with the exact 15 normalized bytes of toy2fmv\\acti.str and 62 same-caller calls, no old unimplemented fatal, then the independently classified renderer boundary 0x800104E4.

## What would falsify it

The verified SLUS/FMV call chain changes, the pinned framework lacks A0:0x25, a same-input retail trace fails to complete all 15 normalized first-path bytes, or the gate accepts a mutated call/leaf/store or old first-call fatal.

## Re-confirmed 2026-08-22 18:15:55

Confirmed immediately against the landed/pinned state after creation: psxport_sync --check reports build commit ad5cf802 equals psxport.pin; bounded real-disc trace passes the exact 15-byte first path and 62 A0:0x25 calls before gated sibling renderer miss 0x800104E4.
