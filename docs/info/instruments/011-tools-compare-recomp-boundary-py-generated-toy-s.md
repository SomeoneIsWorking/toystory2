---
id: I011
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

tools/compare_recomp_boundary.py -- generated Toy Story 2 execution versus independent Mednafen CPU oracle at the symbolic first-call boundary

## Validated by

Validated both answers on 2026-08-21. Shipping generated crt0 and independent oracle agree on pc plus all 33 nonzero/HI/LO register fields at first call 0x80089344 (34/34). The 2/2 selftest mutates only port a0 and the same comparator reports exactly one named mismatch, proving it can produce the opposite answer. Revalidated at exact pinned psxport `3418a79b` after guest-instruction accounting became part of every generated block: the shipping runner owns a complete `Game`, so `Core::game->timing` follows the same initialized ownership path as the port rather than dereferencing an orphan `Core`.

## Known failure modes

The generated runner must construct the framework's whole-machine `Game`, not an orphan `Core`;
guest-instruction accounting reaches `Core::game->timing` before the first-call capture.
