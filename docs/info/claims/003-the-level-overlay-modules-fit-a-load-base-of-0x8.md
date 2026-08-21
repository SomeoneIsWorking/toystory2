---
id: C003
kind: claim
status: holds
created: 2026-08-12
tags: overlays,memory-map
depends: tools/base_fit.py
reconfirmed: 2026-08-21
verified_at: 2026-08-21 03:46:38
---

## Claim

The LEVEL overlay modules FIT a load base of 0x800D1000 — a measured FIT, explicitly NOT a confirmed resident base, and it must not be written into overlaySlots or overlay_bases

## Evidence

MEASURED 2026-08-12 by tools/base_fit.py: slide a candidate base over all of RAM at 4 KiB steps and score by the fraction of the files out-of-boot-.text jal targets landing in [base, base+size), reporting a runner-up at least one file-length away so a flat landscape is visible. Over 178 files, 17 fit the stated rule (>=50% of >=4 targets, margin >=2 over the runner-up). 15 of the 17 peak at 0x800D1000 with 75-100% hit and a 0.0% RUNNER-UP. THE NEGATIVE CLASS BEHAVES: ZERO .RAW/.DAT/.ALL files fit any base. Reproduce with `python3 tools/base_fit.py --selftest`, which gates the positive class (the LEVEL*/LEVEL.BIN modules must fit, EXACTLY 9 of 10 at this slot and LEVEL09 at 0x800D2000) against two negative classes (150 asset files, and the 5 PAD/PATH*.BIN trap files) and REFUSES exit 2 on a missing corpus or a missing boot exe. CORRECTION 2026-08-12: until reviewed, that selftest could not tell a MEASURED base from a HARDCODED one - it gated only the fit/nofit verdict plus `len(at_slot) >= 5`, which a hardcode maximises - so this claim's central VALUE was ungated. A DERIVATION gate now asserts the LEVEL09 disagreement, that re-scoring at the printed base reproduces the claimed hit count, and that a decoy base one page up scores strictly worse; the sabotage that used to PASS (report the slot for every file, keep the true argmax's count) now exits 1 with 4 FAILs. See docs/info/instruments/002-*. INDEPENDENT CORROBORATION from bytes the fit does not use: the modules own trailer tables hold absolute 0x800Dxxxx pointers (LEVEL06/LEVEL1.BIN 0x800D1334/0x800D1420/0x800D12F4; BITS/MEMORY.BIN 11 consecutive absolute address words after its initial word), and the boot exe holds 22 literal address words above its .text end, 20 of them in 0x800D12D4..0x800E3614. OUTLIERS REPORTED AS-IS rather than smoothed: LEVEL09/LEVEL.BIN peaks at 0x800D2000 (66.7%, only 12 targets, weak); LEVEL10/LEVEL1.BIN at 0x800B4000 (66.7%, 6 targets, weak); BITS/MEMORY.BIN peaks sharply at 0x800D6614 (409/413 = 99.0% at 4-byte granularity) while 0x800D6614 is not a plausible aligned resident base. WHY THIS IS A FIT AND NOT AN ANSWER: fitting alone did not establish how the loader computed a base or whether LEVEL and LEVEL1 were co-resident. C010 now supersedes those former open questions with instruction evidence: the loader forms exact LEVEL slot 0x800D12C0, LEVEL and LEVEL1 are alternative contents, and MEMORY is simultaneous at 0x800D5D20. BLIND SPOTS: PLAIN absolutely-linked code only (a relocated module has no absolute jal to fit and reads as no-evidence); 4 KiB granularity by default; a fit is evidence about one module in isolation and cannot decide the slot COUNT.

## What would falsify it

a decompile of the overlay loader (the function referencing the overlay-name string group at VA 0x80022F84..0x80022FA8 — level1.bin at 0x80022F84, level.bin itself at 0x80022FA8; xref BOTH) showing it computes a different base — which is entirely possible, because the constant is materialised NOWHERE in the boot exe, so the resident base could differ from the fitted one and would still fit; also falsified if any module turns out to be relocated rather than absolutely linked

## Re-confirmed 2026-08-21

Post-landing base_fit selftest re-derived the historical 0x800D1000 coarse fit with argmax and decoy controls; C010 remains the exact load-address authority.
