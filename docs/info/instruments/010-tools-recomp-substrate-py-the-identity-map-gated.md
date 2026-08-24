---
id: I010
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

tools/recomp_substrate.py -- the identity/map-gated 22-module shipping emitter and generated-interface validator

## Validated by

Validated both answers on 2026-08-21 through the production identity, corpus, emitter, and generated-output
digest paths. Verified SLUS_008.93 plus all 21 measured modules emits 321 resident roots -> 864
functions and 176 overlay roots -> 243 functions in 72 TUs. The 5/5 selftest changes one generated
source byte, changes one executable byte, supplies an out-of-text seed, and omits one overlay; each
production gate detects its negative.

Revalidated 2026-08-22 with the proven FMV module included. The production path provisions exactly
22/22 modules / 756,108 file bytes and emits 365 roots -> 889 resident functions plus 222 roots -> 308
functions across 22 modules / 75 translation units. The same 5/5 selftest still exercises changed
generated output, changed executable identity, an out-of-text seed, and an omitted corpus member.

Revalidated 2026-08-25 after the generic overlay-root correction and LEVEL01 seed. The production
path provisions 22/22 modules / 756,108 bytes and emits 360 roots -> 884 resident functions plus 223
roots -> 309 functions across 22 modules / 75 translation units. The five removed roots are retail
branch delay slots falsely targeted by jal-shaped mixed overlay data; the whole-pipeline emitter test
proves the opposite answer by failing before that structural rejection.

## Known failure modes

(none recorded yet)
