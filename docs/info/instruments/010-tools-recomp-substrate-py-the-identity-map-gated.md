---
id: I010
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

tools/recomp_substrate.py -- the identity/map-gated 21-module shipping emitter and generated-interface validator

## Validated by

Validated both answers on 2026-08-21 through the production identity, corpus, emitter, and generated-output
digest paths. Verified SLUS_008.93 plus all 21 measured modules emits 321 resident roots -> 864
functions and 176 overlay roots -> 243 functions in 72 TUs. The 5/5 selftest changes one generated
source byte, changes one executable byte, supplies an out-of-text seed, and omits one overlay; each
production gate detects its negative.

## Known failure modes

(none recorded yet)
