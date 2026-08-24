---
id: C005
kind: claim
status: falsified
created: 2026-08-12
tags: assets,formats
depends: tools/raw_probe.py
reconfirmed: 2026-08-12
verified_at: 2026-08-12
falsified_on: 2026-08-24
superseded_by: C019
---

## Claim

A Toy Story 2 .RAW asset file is a concatenation of RNC ProPack chunks whose standard 18-byte header has had its 4-byte magic stripped, leaving a 14-byte header terminated by a 0xFFFFFFFF sentinel — the FRAMING is confirmed by CRC, the CODEC is not

## Evidence

MEASURED 2026-08-12 by tools/raw_probe.py. THE HYPOTHESIS was formed by combining two independent third-party sources NEITHER of which states it (mateusfavarin/tsr docs/RAW.MD gives the 0x0E header size and the sentinel but never names a codec; juanmv94 says RNC PRO-PACK but gives no layout) and was then tested falsifiably. LAYOUT: +0x00 be32 unpackedLen, +0x04 be32 packedLen, +0x08 be16 unpackedCRC, +0x0A be16 packedCRC, +0x0C u8 leeway, +0x0D u8 packChunks. THE DISCRIMINATOR IS THE PACKED CRC, NOT THE LENGTHS — two plausible u32 lengths per chunk are weak agreement, and a self-consistent chunk walk can be produced by a wrong header size; a CRC is 16 bits of independent agreement per chunk over bytes the header does not describe. RESULT on LEVEL01/LEVEL.RAW: chunks_scanned=39 crc_ok=39 crc_bad=0, CRC-16/ARC over each payload equalling be16@0x0A exactly, consuming 469,266 of 469,276 bytes and stopping on the sentinel. SELFTEST gates FOUR classes, EACH ON THE SHIPPED CLI PATH'S OWN EXIT STATUS, and was run: POSITIVE 39/39, exit 0; NEGATIVE A (a real .DAT) rejected at offset 0 on implausible lengths, exit 2; NEGATIVE B (truncated) walks 11 valid chunks, never reaches a sentinel, 2,514 bytes unexplained, exit 1; NEGATIVE C (one payload byte flipped) yields crc_ok=38 crc_bad=1, exit 1 - which is what proves the CRC check FIRES rather than being computed and ignored. CORRECTION 2026-08-12: this claim previously described NEGATIVE B as a gated class while the assertion was on probe()'s internal tuple, and report() - the CLI - exited 0 on a truncation because crc_bad was 0. A valid PREFIX now exits 1 and says how many bytes it could not explain; docs/info/instruments/003-* records the failure mode. INDEPENDENT CROSS-CONFIRMATION from a source that could have disagreed: mouksx/Toy-Story-2-Modding extracted 1.Andys House/level.raw is byte-length-identical to our LEVEL01/LEVEL.RAW (469,276 B) and its 39 sub-files match our 39 decoded chunk lengths exactly and in order — so that repository extracted assets are literally our PSX chunks, which also makes its .RAW/.DAT prose PSX-valid evidence even though its .NGN prose is not. CONSEQUENCE: any standard RNC decompressor works on TS2 data after synthesising the 4-byte magic. BLIND SPOTS, printed on every run: the probe does NOT decompress, so a header match is not proof of the bitstream codec; the METHOD byte (1 or 2) is unrecoverable because it lives in the stripped magic and must be settled by attempting decompression, never assumed; the unpacked CRC is unchecked pending a decompressor, so this covers FRAMING only.

## What would falsify it

any TS2 .RAW file producing a packed-CRC mismatch under this layout; or an attempted decompression failing under BOTH RNC method 1 and method 2, which would mean the header identification is coincidental and the codec is something else; or the unpacked CRC failing once a decompressor exists

## FALSIFIED 2026-08-24 — exactly by the third listed falsifier

The framing half SURVIVES (14-byte header layout, sentinel, packedCRC@0x0A — re-verified since inside C019's 813/813 sweep), but the CODEC half is dead: attempted decompression per this claim's own falsifier shows RNC fails (new AND old variants, both methods; see C019 for the full measurement) and the payloads decode with TT's own `DecompressRAW` LZ scheme instead. The CONSEQUENCE line above ("any standard RNC decompressor works after synthesising the magic") was wrong, and juanmv94's "RNC PRO-PACK" attribution — one of the two sources this hypothesis was built from — does not describe this container. Superseded by `docs/info/claims/019-*`.
