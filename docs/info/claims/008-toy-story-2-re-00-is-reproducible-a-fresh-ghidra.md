---
id: C008
kind: claim
status: holds
created: 2026-08-20
tags: ghidra,re-supply,reproducibility
depends: tools/ram_image.py, tools/ghidra_xref.py, tools/re_xref.py
---

## Claim

Toy Story 2 RE-00 is reproducible: a fresh Ghidra 12 project over the verified SLUS_008.93 RAM image supplies guest-address xrefs and decompiled C

## Evidence

Verified 2026-08-20 from the tracked workflow, not inherited scratch residue. SHA-1 of scratch/bin/toystory2/SLUS_008.93 matched docs/info/exe-identity.txt at f90c9cd6b4fc9845adfe34e306b7df393bf9154c. tools/ram_image.py placed exactly 595,968 executable bytes at [0x80010000,0x800A1800). A fresh ts2boot_re00 import with the framework decompiler reported Analysis succeeded using MIPS:LE:32:default. python3 tools/re_xref.py --project ts2boot_re00 --selftest passed 10/10 fold controls and 5/5 independent Ghidra/fold controls. The framework decompiler then emitted one 24-line C function for the header entry 0x80082D60. This proves the RE supply, not correctness of the decompiled types or completion of crt0/overlay-loader RE.

## What would falsify it

a fresh import from the tracked ram-image workflow failing analysis, the cross-method selftest failing either answer class, the entry decompile producing no function, or the executable identity changing
