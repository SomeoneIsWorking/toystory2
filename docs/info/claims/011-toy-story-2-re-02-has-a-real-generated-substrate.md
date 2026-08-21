---
id: C011
kind: claim
status: holds
created: 2026-08-21
tags: recomp,boot,overlays
depends: tools/recomp_substrate.py#measure, tools/compare_recomp_boundary.py#check, game/recomp_seeds.json, game/core/recomp_register.cpp#kTs2Recomp, tests/toystory2_recomp_boundary.cpp#capture_boundary
reconfirmed: 2026-08-21 11:45:09
verified_at: 2026-08-21 11:45:09
---

## Claim

Toy Story 2 RE-02 has a real generated substrate through the current CD-completion boundary: the verified executable and exactly 21 measured code overlays emit 864 resident and 243 overlay functions, generated crt0 agrees 34/34 with the independent CPU oracle at first call 0x80089344, and live boot handles the binary-classified IRQ resume 0x80088A2C before reaching stock-libcd command/completion poll 0x80091DE4.

## Evidence

I010: tools/recomp_substrate.py --selftest passed 5/5 and the shipping emitter produced 321 resident roots -> 864 functions plus 176 roots -> 243 functions across 21 overlays and 72 TUs. Its generated-output digest changes when one emitted source byte changes. I011: tools/compare_recomp_boundary.py --selftest passed 2/2 at instruction-derived first jal 0x80089344, including a forced a0 mismatch. Live ./run.sh fail-fast exposed guest pointer 0x80088A2C from RAM[0x8009ECD0]; exact instructions and Ghidra show no prologue, live-v0 use, and a shared stack epilogue. With pinned psxport 692b9b20 it is recorded solely as `main_reentry`, which emits its dispatchable wrapper/body while preserving the containing function's fall-through. The same framework revision owns generic BIOS A0:0x15 (`strcat`); live default boot returns through both `.vh`/`.vb` path compositions and reaches generated stock-libcd command/completion poll 0x80091DE4, with active CHD decompression visible in the watchdog stack.

## What would falsify it

the retail executable or any of the 21 modules changes identity, the shipping emitter produces a different module/interface denominator, generated and oracle state differ at the first call, exact code disproves 0x80088A2C as a mid-function IRQ resume, or live boot again misses it before the CD boundary

## Re-confirmed 2026-08-21 10:53:19

Final integrated gate on 2026-08-21: CTest 6/6 now includes the 5/5 substrate selftest, 2/2 independent generated/oracle boundary compare, launcher, RE-01, RE-03, Clang format and clang-tidy. Emitter denominator remains 321 resident roots -> 864 functions and 176 overlay roots -> 243 functions across 21 overlays/72 TUs; live boundary is stock-libcd command/completion poll 0x80091DE4 after IRQ resume 0x80088A2C.

## Re-confirmed 2026-08-21 11:33:45

Pinned psxport 692b9b20 and final Clang build: substrate stayed 321 resident roots -> 864 functions plus 176 roots -> 243 functions across 21 overlays/72 TUs; generated/oracle gate passed 34/34 with its named forced negative; CTest passed 6/6. The zero-argument launcher resolved the shared clean framework at that exact revision, returned through both asset-suffix compositions, opened the CHD, and reached generated stock-libcd command/completion poll 0x80091DE4 with the framework's continuous-read path actively decompressing disc data; no unimplemented BIOS or recomp miss occurred before that boundary.

## Re-confirmed 2026-08-21 11:45:09

Definitive final-SHA gate: psxport.pin records 692b9b20e3d4a6194452522060fd2657c2235f40 and CMake resolved the clean shared external/psxport checkout at that exact commit with Clang 22.1.8. Substrate remained 321 roots -> 864 resident functions and 176 roots -> 243 functions across 21 overlays/72 TUs; independent oracle agreed 34/34 and its forced register mutation produced one named mismatch; CTest passed 6/6. Zero-argument ./run.sh resolved shared @692b9b20, opened the CHD with no A0:0x15 fatal or recomp miss, and reached generated stock-libcd command/completion poll 0x80091DE4 with active disc_read_raw/CHD decompression.
