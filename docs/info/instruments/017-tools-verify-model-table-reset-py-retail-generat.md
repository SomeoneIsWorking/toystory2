---
id: I017
kind: instrument
status: trusted
created: 2026-08-25
---

## Instrument

tools/verify_model_table_reset.py -- retail/generated/live model-table reset verifier

## Validated by

Validated both answers on 2026-08-25. Positive exact-checks retail bnez 0x80041FF8 plus delay-slot addiu a1,a1,4 at 0x80041FFC, the generated reset body with r5 increment and absence of a standalone delay-slot function, then classifies the real slot-9 writer log as nonzero package -> later reset zero -> distinct replacement package with progress through field 10,303 and no fatal/recomp-MISS. Six synthetic runtime controls include the positive and independently reject missing clear, same stale package reload, retired pointer fault, dispatch miss, and insufficient continuation; selftest passes 7/7. Known limit: the 180-second progress denominator is bounded evidence, not indefinite stability or interactive gameplay.

## Known failure modes

(none recorded yet)
