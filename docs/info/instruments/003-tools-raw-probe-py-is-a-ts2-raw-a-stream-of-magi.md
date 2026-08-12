---
id: I003
kind: instrument
status: trusted
created: 2026-08-12
---

## Instrument

tools/raw_probe.py — is a TS2 .RAW a stream of magic-stripped RNC ProPack chunks? (the asset container probe)

## Validated by

Validated in BOTH directions on real data 2026-08-12 by --selftest over FOUR classes, real output: POSITIVE = LEVEL01/LEVEL.RAW, chunks=39 crc_ok=39 crc_bad=0, consumed 469,266 of 469,276 bytes, stopped on the 0xFFFFFFFF sentinel; NEGATIVE A = a real LEVEL.DAT, rejected at offset 0 on implausible lengths (ulen=889,192,448 plen=1,174,421,248); NEGATIVE B = the positive truncated to a third, walks 11 chunks and then dies without reaching a sentinel; NEGATIVE C = the positive with one payload byte flipped, crc_ok=38 crc_bad=1. Exit 0, SELFTEST PASS. NEGATIVE C IS THE ONE THAT MATTERS: it proves the CRC check FIRES rather than being computed and ignored, without which every crc_ok this tool ever printed would be meaningless. THE DISCRIMINATOR IS THE CRC, NOT THE LENGTHS, by design - two plausible u32 lengths per chunk are weak agreement and a self-consistent walk can be produced by a wrong header size, whereas the CRC is 16 independent bits per chunk over bytes the header does not describe. EXIT STATUS IS THE RESULT and all four classes are asserted on report()'s exit status, not on probe()'s internal tuple (fixed 2026-08-12, see failure modes): 0 only for a walk that reaches the 0xFFFFFFFF sentinel with zero CRC mismatches; 2 for ZERO chunks parsed (REFUSED - the hypothesis was never exercised); 1 for any CRC mismatch (FALSIFIED); 1 for a walk that stops anywhere else, printing how many bytes were left unexplained and that N verified chunks are a VALID PREFIX, not a confirmed file. It cleans up its own selftest byproducts. BLIND SPOTS printed on every run: it does NOT decompress, so a header match is not proof of the bitstream codec; it cannot read the RNC method byte (1 or 2) because that byte lives in the stripped 4-byte magic, so the method must be settled by attempting decompression and never assumed; the unpacked CRC is unchecked pending a decompressor, so the result covers FRAMING only.

## Known failure modes

FOUND AND FIXED 2026-08-12 (adversarial review, reproduced here before fixing): report() - the CLI path, i.e. the only channel a script or a hook can read - returned 0 whenever crc_bad == 0, REGARDLESS of why the chunk walk stopped. On a 100 KB truncation of LEVEL01/LEVEL.RAW it exited 0 while printing `chunks_scanned=7 crc_ok=7 crc_bad=0 consumed=96550/100000 stopped_because='implausible lengths at 0x17926'` - clean over a walk that never reached the sentinel and left 3,450 bytes unaccounted for, which is exactly the "clean over broken" this tree forbids. The selftest did not catch it because NEGATIVE B asserted on probe()'s internal tuple while the shipped path was report(): THE GATED CLASS AND THE SHIPPED PATH HAD DIVERGED. Fixed by returning non-zero unless the reason is the sentinel, and by asserting every class on report()'s return value (the POSITIVE too, so a stricter report() cannot start rejecting real files). Reverting the sentinel check now makes NEGATIVE B FAIL: "NEGATIVE B (truncated) exited 0 from report() - the CLI reports CLEAN over a walk that never reached a sentinel", exit 1. GENERAL LESSON: assert the gated class through the SHIPPED entry point, or the selftest certifies a function nobody calls.
