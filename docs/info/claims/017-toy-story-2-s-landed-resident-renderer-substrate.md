---
id: C017
kind: claim
status: holds
created: 2026-08-22
tags: render,recomp,boot
depends: tools/verify_render_reentry.py#check_generated, game/recomp_seeds.json, psxport.pin
reconfirmed: 2026-08-25 00:53:16
verified_at: 2026-08-25 00:53:16
---

## Claim

Toy Story 2's landed resident renderer substrate lowers exactly the retail 32-way internal jump table without per-slot seeds

## Evidence

Current pinned and built psxport 8611d756. tools/verify_render_reentry.py --selftest passes 13/13
positive/negative classes over the shipping generated switch, requiring exactly 32 ordered slots from
0x800103EC through 0x800104E4. A bounded real-disc log has neither former 0x8001040C nor 0x800104E4
dispatch miss, passes the resolved neutral frame-fence boundary, executes the seeded LEVEL01 entry
0x800D12C4 and reaches the exact later model-pointer boundary in C021. The renderer diagnostic seed
is absent.

## What would falsify it

The retail table construction or 32-slot set changes, generated output gains a missing or extra slot, a renderer slot seed returns, either former dispatch miss recurs, or the same-input live gate no longer reaches the later exact C021 boundary.

## Re-confirmed 2026-08-24 after RE-14

The exact generated-switch verifier still finds all 32 ordered slots with no renderer seed, and its
current 13/13 controls require the moved live boundary's exact address and model consumer rather than
accepting any unrelated unmapped read16. The Clang build and complete 11/11 CTest gate pass.

## Re-confirmed 2026-08-24 at current pin bc8c8897

The exact generated-switch verifier remains 13/13, and the explicit Clang port/oracle build plus
complete 11/11 CTest gate pass. No runtime launch was performed; the later model-pointer boundary
remains the last d2266f4b live observation.

## Re-confirmed 2026-08-22 19:10:49

Pinned/build-checked psxport 57a17a14; the shipping generated verifier passes all 10 classes with exactly 32 ordered internal slots and no renderer seed, while the bounded retail log has neither former dispatch miss and reaches RenderQueue capture capacity.

## Re-confirmed 2026-08-22 19:15:01

Post-commit 3b17154 exact generated-switch check finds the ordered 32/32 retail slots with no game seed; 10/10 controls pass and the live route crosses 0x8001040C and 0x800104E4 without a miss before RenderQueue capacity; CTest 9/9 passes.

## Re-confirmed 2026-08-22 19:25:14

Pinned psxport 57a17a14 still emits the exact 32-slot resident renderer switch with no game seed; updated 11/11 controls and the post-fence live trace cross both former misses and reach independent boundary 0x800D12C4.

## Re-confirmed 2026-08-22 19:46:57

Pinned d2266f4b exact generated-switch verifier passes 11/11 with all 32 retail slots and no renderer seed; final live trace crosses both former misses and reaches 0x800D12C4; CTest passes 10/10.

## Re-confirmed 2026-08-24 20:19:57

After 456a31f, render-reentry check and its controls verified the exact 32-slot table and advanced route to the classified model-pointer boundary.

## Re-confirmed 2026-08-24 at current pin 9c2e3f1c

`verify_render_reentry.py --check` retained the exact 32 retail slots with no game seed, and the
explicit Clang port/oracle build plus complete 11/11 CTest gate passed. No runtime launch was
performed; the later model-pointer boundary remains the last d2266f4b live observation.

## Re-confirmed 2026-08-25 00:53:16

At pushed framework pin aa0b2067, verify_render_reentry.py --check retained exactly 32 ordered retail slots with no renderer seed; its saved real-log classifier crosses both former misses and the RE-16 reset/continuation witness through field 10303; the clean Clang build passed 12/12 CTest.
