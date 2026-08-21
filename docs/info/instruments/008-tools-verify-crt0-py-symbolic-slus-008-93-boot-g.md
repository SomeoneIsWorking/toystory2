---
id: I008
kind: instrument
status: trusted
created: 2026-08-21
---

## Instrument

tools/verify_crt0.py — symbolic SLUS_008.93 boot-group verifier

## Validated by

On verified SLUS_008.93, `--check` compared 16 shipping/header constants to instruction-derived values. `--selftest --cross ../Tomba2Engine/scratch/bin/tomba2/MAIN.EXE` passed 9/9: real target positive, a mutated BSS immediate moved bssZeroHi and disagreed, a poisoned shipping gameMain disagreed, missing/zero/garbage/truncated inputs refused, gameMain was rejected as a crt0, and the real Tomba!2 executable produced a different crt0 and was refused as the Toy Story target. As an independent implementation check, psxport's C++ `crt0_extract` (the shipping runtime decoder) agreed on BSS, both stack sources, zero bias, heap base/size, gp, InitHeap and both absent optional stores; the Python verifier additionally proves the post-InitHeap gameMain/control terminator and shipping-source comparison.

## Known failure modes

The portable CTest runs the eight self-contained cases. The ninth, genuine cross-binary case requires a separately provisioned PS-X executable and therefore is run explicitly in the shared workspace with `--cross ../Tomba2Engine/scratch/bin/tomba2/MAIN.EXE`; it must never become a hidden bare-clone dependency.
