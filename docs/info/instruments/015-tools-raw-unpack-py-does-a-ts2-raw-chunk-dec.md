---
id: I015
kind: instrument
status: trusted
created: 2026-08-24
tags: assets,formats,unpacker
depends: tools/raw_unpack.py
---

## Instrument

tools/raw_unpack.py — does a TS2 .RAW chunk decode with TT's `DecompressRAW` LZ scheme under BOTH its header CRCs (packed AND unpacked)? (the RE-08 codec instrument; supersedes raw_probe.py's framing-only coverage)

## Validation

TRUSTED only after it showed the OTHER answer. The RNC attempts that FAILED (new/old × method 1/2, plus a big-endian-word variant — 38–39 of 39 LEVEL01 chunks each) were run through the same harness before the TT transcription passed, so the instrument has demonstrated both directions on real data. POSITIVE: 46 files / **813 chunks**, every one decoding to exactly unpackedLen with crc16-ARC matching BOTH header fields, exit 0 (`scratch/logs/raw_unpack_corpus.txt`, 2026-08-24). EXTERNAL WITNESS: decoded LEVEL01 output byte-matches all 39 of mouksx's independently extracted sub-files. The hermetic SELFTEST uses the same `verify_data` production seam and gates five classes without copyrighted inputs: a minimal literal `A` stream under both derived CRCs (exit 0); non-.RAW REFUSED (2); exact chunk EOF with the sentinel removed FAIL (1); flipped payload byte caught by packed CRC (1); flipped unpacked-CRC field caught independently (1). The sentinel-negative exposed and closed issue #18; the unpacked-CRC negative proves the former C005 blind spot is actually checked.

## Blind spots

does not classify what decodes INTO (command/packet IDs are the next layer, unmeasured); leeway@0x0C and chunkCount@0x0D are recorded but their semantics under the TT codec are UNMEASURED; the decoder is a faithful transcription of tsr's decompilation of a DIFFERENT (but same-engine) title — a SLUS_008.93 Ghidra read of its own decompressor has never been done and is the standing deeper evidence. The hermetic fixture is a regression control, not corpus evidence; `--all` over the extracted retail corpus remains the faithfulness gate.
