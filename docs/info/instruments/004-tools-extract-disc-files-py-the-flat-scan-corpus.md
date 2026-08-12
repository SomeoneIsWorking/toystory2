---
id: I004
kind: instrument
status: trusted
created: 2026-08-12
---

## Instrument

tools/extract_disc_files.py — the flat scan corpus (every non-media disc file, size-verified)

## Validated by

Validated on real data 2026-08-12, real output: listing entries 300, skipped (TOY2FMV) 26 files / 524,939,264 bytes, attempted 274, extracted AND SIZE-VERIFIED 274, FAILED 0, exit 0. Every extraction is checked against the size the ISO9660 listing declares, so a short read is a FAILURE and not a quietly smaller file - which matters because every downstream census reads these bytes as a whole file. It exits non-zero if any file failed and REFUSES (exit 2) if ZERO were extracted, because nothing-to-scan must never look like a clean run: a census over an empty directory is exactly the shape of a green-over-nothing bug. Its disc reads go through tools/discdump.py (the framework's own ISO9660/CHD reader, resolved through PSXPORT_DIR and built into this repo's gitignored scratch/, never into the read-only submodule), so there is ONE answer to what is on the disc. THE SKIP IS A STATED DENOMINATOR GAP, NOT A FILTER, and it is printed on every run: TOY2FMV/ is 95.3% of the disc by bytes and is not extracted by default, so any statement made from this corpus covers 274 of 300 files and a code overlay hidden inside an .STR would not be seen. --keep-media closes the gap. Everything it writes is disc-derived and gitignored; nothing it produces may be committed.

## Known failure modes

(none recorded yet)
