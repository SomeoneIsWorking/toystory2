---
id: C019
kind: claim
status: holds
created: 2026-08-24
tags: assets,formats
depends: tools/raw_unpack.py, tools/raw_probe.py#walk
---

## Claim

Toy Story 2 `.RAW` payloads decode with Traveller's Tales' OWN LZ decompressor — the routine TSR's Ghidra research names `DecompressRAW` (mateusfavarin/tsr, MIT) — NOT RNC ProPack in any variant; there is no RNC method byte to recover, and BOTH header CRCs (packed AND unpacked) verify on every chunk of every `.RAW` measured

## Evidence

MEASURED 2026-08-24 by tools/raw_unpack.py over the whole extracted corpus. THE FRONTIER QUESTION DISSOLVED, it was not answered: the step asked "RNC method 1 or 2?" and the measurement is NEITHER. Attempted decompression per the falsifier of C005 — temisu/ancient's algorithms ported to scratch harnesses (`scratch/raw/be16_test.py`, `scratch/raw/old_test.py`): RNC1-new fails all 39 LEVEL01 chunks (huffman table/dead-end errors from the first table read), RNC2-new fails 38/39, a big-endian-word RNC1 variant fails identically, and RNC1-old/RNC2-old (ancient's `RNCDecompressOld`, full VLC/backward-bitstream port) fail all with backward-copy range errors. THE CODEC THAT PASSES is TT's flag-bit LZ scheme transcribed faithfully from tsr's `scripts/extract_raw.py::decompress_raw_chunk` (itself derived from the game's own `DecompressRAW`): RESULT — 46 of 46 `.RAW` files, **813 of 813 chunks**, each decoding to exactly its header unpackedLen with packedCRC-16/ARC AND unpackedCRC-16/ARC both matching (exit 0; log `scratch/logs/raw_unpack_corpus.txt`). INDEPENDENT CROSS-CHECK that could have disagreed: mouksx/Toy-Story-2-Modding ships its own extraction of the SHA-1-identical `level.raw`; our decoded chunk output matches all 39 of their sub-files BYTE-FOR-BYTE (`scratch/raw/level.raw-*.bin`). SELFTEST gates five hermetic classes through the same `verify_data` production path: POSITIVE literal `A` under both derived CRCs; NEGATIVE A non-.RAW refused (exit 2); NEGATIVE B exact chunk EOF with the sentinel removed (exit 1); NEGATIVE C flipped payload byte caught by the packed CRC (exit 1); NEGATIVE D flipped UNPACKED-CRC header field with intact payload caught by the unpacked gate (exit 1). HEADER FIELDS NOW FULLY ACCOUNTED: be16@0x08 == crc16(UNPACKED) on 39/39 LEVEL01 chunks and be16@0x0A == crc16(PACKED) on 39/39 — the header keeps the standard RNC field layout minus magic while the payload is TT's codec. TRAP, recorded because one session WILL fall into it: tiny chunk LEVEL01#19 (35->64 B) ALSO decodes under RNC2-new with both CRCs passing AND output matching the independent extraction — one chunk can "confirm" RNC2; only the corpus discriminates. NEXT LAYER UNLOCKED: each decoded chunk begins with an LE u32 behaving like a command/packet ID (LEVEL01 chunks 0..9 read 0x0,0x1,0x2,0x3,0x4,0x5,0x8,0xE,0x10,0x11), matching tsr's loader taxonomy shape (<0x20 texture packets etc.) — TS2's own ID table and packet structures are UNMEASURED and are the next RE step.

## What would falsify it

any `.RAW` chunk failing this decoder while passing under an RNC variant (the reverse of today's measurement); a corpus file where the TT decode verifies both CRCs but disagrees with an independent extraction; a second .RAW-bearing TT title whose docs/game code name a DIFFERENT routine for the same 14-byte-header container; or a guest-side Ghidra read of SLUS_008.93 locating its own decompressor whose algorithm differs from the transcription here
