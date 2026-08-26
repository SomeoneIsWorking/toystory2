---
id: I019
kind: instrument
status: trusted
created: 2026-08-26
---

## Instrument

tools/verify_projection_publication.py — retail libgte projection leaf/call/config comparator

## Validated by

2026-08-26: identity-checked SLUS_008.93 positive derives exact SetGeomOffset 0x80083CD4, SetGeomScreen 0x80083CF4, graphics-init values 256/120/160, and matching half-open HLE window. Five independent mutations change either leaf body, the initializer call, an initializer argument, and the shipping binding; all are refused or rejected (6/6). Hermetic C++ boundary separately passes 19 checks through shipping runtime handlers. Blind spot: it does not prove a serialized live run reaches the leaves or that a native producer consumes ProjParams.

## Known failure modes

(none recorded yet)
