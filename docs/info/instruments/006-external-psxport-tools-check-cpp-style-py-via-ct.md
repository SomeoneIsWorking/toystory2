---
id: I006
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

external/psxport/tools/check_cpp_style.py via CTest cpp_policy — checks this consumer's Clang format, 1,200-line structure cap, and touched compile-backed C++ TUs with clang-tidy

## Validated by

Validated 2026-08-21 on Toy Story 2 in both failure classes and restored clean: deliberately malformed formatting in game/core/recomp_register.cpp made cpp_policy fail at clang-format; a formatted sizeof(pointer)/sizeof(pointer[0]) probe made it fail with bugprone-sizeof-expression; the restored four-TU Clang compile database passed. A missing compile database refuses instead of reporting zero tidy TUs.

## Known failure modes

(none recorded yet)
