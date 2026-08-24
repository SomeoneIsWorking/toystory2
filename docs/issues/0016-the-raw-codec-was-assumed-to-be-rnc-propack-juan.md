---
id: 16
title: the .RAW codec was assumed to be RNC ProPack (juanmv94 attribution); every RNC variant fails
status: dead-end
symptom: RNC decompressor fails or silently half-works on TS2 .RAW chunks; huffman dead-end / backward-copy-range errors; method byte unrecoverable
tags: assets,formats,dead-end,prior-art,cross-platform
created: 2026-08-24
updated: 2026-08-24
---

DEAD END, measured 2026-08-24 (RE-08, docs/info/claims/019-* superseding 005).

## What was tried
temisu/ancient algorithms ported to scratch harnesses: RNC1-new (39/39 LEVEL01 chunks fail), RNC2-new (38/39 fail), big-endian-word RNC1 variant (fails identically), RNC1-old and RNC2-old via a full RNCDecompressOld port (all fail, backward-copy range). The header keeps RNC's FIELD LAYOUT minus magic (both CRCs verify against it), which is why the hypothesis looked right.

## Why it looked right
juanmv94's notes say "RNC PRO-PACK"; the 14-byte header is exactly an 18-byte RNC header minus 4 bytes. TRAP: tiny chunk LEVEL01#19 (35->64 B) ALSO decodes under RNC2-new with both CRCs passing and output matching the independent extraction — one chunk can confirm the wrong codec.

## What actually works
TT's own flag-bit LZ scheme, DecompressRAW per mateusfavarin/tsr (MIT): 813/813 corpus chunks pass both CRCs; decoded LEVEL01 byte-matches all 39 mouksx extractions. tools/raw_unpack.py ships it.

## Rule this leaves behind
Never settle a codec question on a single chunk, and never trust a field-layout match as a codec identification.
