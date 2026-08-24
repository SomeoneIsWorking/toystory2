---
id: C016
kind: claim
status: holds
created: 2026-08-22
tags: boot,fmv,bios,recomp
depends: tools/verify_fmv_boundary.py#analyze, psxport.pin
reconfirmed: 2026-08-25 00:53:16
verified_at: 2026-08-25 00:53:16
---

## Claim

Toy Story 2 retail FMV path parsing passes the shared BIOS A0:0x25 boundary; current pin 8611d756 retains that shared implementation

## Evidence

Exact SLUS/FMV words prove 0x800D8E48 -> Sony leaf 0x80082E5C -> A0:0x25 and byte store at
0x800D8E50. tools/verify_fmv_boundary.py mutation controls reject changed call/leaf/store classes. A
the last clean Clang/runtime gate against d2266f4b produced a bounded real-disc trace with the exact 15
normalized bytes of toy2fmv\\acti.str and 62 same-caller calls, with no old unimplemented fatal.

## What would falsify it

The verified SLUS/FMV call chain changes, the pinned framework lacks A0:0x25, a same-input retail trace fails to complete all 15 normalized first-path bytes, or the gate accepts a mutated call/leaf/store or old first-call fatal.

## Re-confirmed 2026-08-22 19:10:06

Pinned/build-checked psxport 57a17a14; bounded retail gate again observed the exact 15 normalized first-path bytes and 62 same-caller A0:0x25 calls, then the exact renderer verifier found neither former miss before the later RenderQueue boundary; Clang consumer CTest passed 9/9.

## Re-confirmed 2026-08-22 19:15:01

Post-commit 3b17154 live FMV gate on pinned psxport 57a17a14 observes the exact 15 normalized path bytes and 62 same-caller A0:25 calls before reaching the later renderer boundary.

## Re-confirmed 2026-08-22 19:46:56

Pinned d2266f4b live FMV gate observes the exact 15 normalized first-path bytes and 62 same-caller BIOS A0:0x25 calls; the final Clang consumer CTest passes 10/10.

## Re-confirmed 2026-08-24 at current pin bc8c8897

The shared toupper implementation remains present; an explicit Clang port/oracle build and complete
11/11 CTest gate pass. No runtime launch was performed for this pin migration, so the exact live path
sequence remains evidence from d2266f4b rather than being relabelled as a bc8c8897 observation.

## Re-confirmed 2026-08-24 20:19:57

After 456a31f at pinned bc8c8897, verify_fmv_boundary --check reproduced the retail call chain through A0:0x25 and normalized path return.

## Re-confirmed 2026-08-24 at current pin 9c2e3f1c

`verify_fmv_boundary.py --check` reproduced the retail call chain through A0:0x25 and its normalized
path return, and the explicit Clang port/oracle build plus complete 11/11 CTest gate passed. No runtime
launch was performed, so the exact live path sequence remains evidence from d2266f4b.

## Re-confirmed 2026-08-25 00:53:16

At pushed framework pin aa0b2067, verify_fmv_boundary.py --check reproduced the exact retail call chain through shared BIOS A0:0x25 and the normalized-path return; the clean Clang port/oracle build passed 12/12 CTest without a game launch.
