---
id: C009
kind: claim
status: holds
created: 2026-08-21
tags:
depends: tools/verify_crt0.py#analyze, game/core/game_config.cpp#g_ts2_cfg
reconfirmed: 2026-08-22 14:18:13
verified_at: 2026-08-22 14:18:13
---

## Claim

Toy Story 2 RE-01 is instruction-verified: SLUS_008.93 crt0 clears [0x800A1070,0x800D12C0), constructs sp=fp=0x80200000 and gp=0x800A0CD8, calls InitHeap 0x80089344 with heap [0x800D12C4,+0x126D40), then calls gameMain 0x8007A9E8; both optional heap globals are absent.

## Evidence

I008 / `tools/verify_crt0.py --check` walked 43 instructions from header pc0 0x80082D60 through jal 0x80089344, the restored ra, jal 0x8007A9E8, and break 0x80082E08; it printed the exact instruction chain for every field and compared all 16 shipping/header constants. Its 9/9 cross selftest used Tomba!2 MAIN.EXE as the real second binary. psxport's independently implemented C++ `crt0_extract` agreed on every field it covers and on the derived InitHeap plan.

## What would falsify it

Any verified SLUS_008.93 instruction measurement disagrees with one of these fields, the executable identity changes, or tools/verify_crt0.py fails a mutation/malformed/cross-binary case.

## Re-confirmed 2026-08-21

Post-landing verify_crt0 matched all 16 shipping constants and the complete measured startup plan.

## Re-confirmed 2026-08-21

Post-landing Clang CTest passed 6/6; verify_crt0 shipping comparison remained 16/16 and generated/oracle first-call state agreed 34/34.

## Re-confirmed 2026-08-21

Post-landing Clang CTest 7/7, crt0 verifier, oracle boundary 34/34, and forced-mutation opposite answer passed against psxport 3418a79b.

## Re-confirmed 2026-08-22 14:18:13

2026-08-22 runtime-inheritance migration: verify_crt0_selftest passed in the clean Clang scratch/build-runtime CTest suite and the shipping headless route printed 10 AGREE, 0 DISAGREE before dispatching measured gameMain.
