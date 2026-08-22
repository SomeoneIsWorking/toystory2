---
id: I009
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

tools/overlay_map.py -- the exact Toy Story 2 overlay-loader and MEMORY.BIN placement verifier

## Validated by

Validated both answers through the same shipping census/slot_report path on 2026-08-21. Real SLUS_008.93 plus the size-verified retail flat corpus derives LEVEL=0x800D12C0, MEMORY=0x800D5D20, one-slot alternative level contents, and the 63,312-byte MEMORY placement/frontier. The selftest then mutates all four real MEMORY/FMV destination pairs in memory to 0x800D9D20; the same derivation changes to co-resident-possible instead of one-slot. It also refuses zero-call/misaligned/out-of-text callees and rejects a stack-relative decoy as a slot. --check compares the measured values to GameConfig and recomp_seeds.

Revalidated 2026-08-22 after closing the FMV class gap: the same production contract exact-checks
loader call `0x8003EEAC`, immediate entry call `0x8003EEC4 -> 0x800D6628`, file offset `0x908`, retail
entry word `0x27BDFF10`, and the shared FMV base in the seeds. Shipping `--check` and all 10 self-test
classes pass, including the unchanged forced-opposite slot result.

## Known failure modes

(none recorded yet)
