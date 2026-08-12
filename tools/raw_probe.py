#!/usr/bin/env python3
"""raw_probe.py — test ONE falsifiable hypothesis about Toy Story 2's .RAW asset container.

  python3 tools/raw_probe.py scratch/flat/LEVEL01__LEVEL.RAW [more files...]
  python3 tools/raw_probe.py --selftest <a real .RAW> <a file that is NOT a .RAW>

THE HYPOTHESIS. A TS2 `.RAW` is a concatenation of RNC ProPack chunks whose standard 18-byte RNC header
has had its 4-byte magic (`'RNC'` + method) STRIPPED, leaving 14 (0x0E) bytes:

    +0x00 be32 unpacked length
    +0x04 be32 packed length
    +0x08 be16 unpacked CRC
    +0x0A be16 packed CRC        <- the falsifiable part
    +0x0C u8   leeway
    +0x0D u8   pack-chunk count

with the stream terminated by a `0xFFFFFFFF` sentinel.

WHERE IT CAME FROM, and why it needed testing rather than believing: it is the CONJUNCTION of two
independent third-party sources, neither of which states it. `mateusfavarin/tsr` (MIT, a decomp of a
DIFFERENT Traveller's Tales title — docs/references.md) documents the 0x0E header size and the sentinel
but never names a codec; `juanmv94`'s TT PSX notes say "RNC PRO-PACK" but give no layout.

THE DISCRIMINATOR IS THE PACKED CRC, NOT THE LENGTHS. A layout that merely "parses" proves very little:
two plausible u32 lengths per chunk are weak agreement, and a chunk walk that self-consistently reaches
the end of the file can be produced by the wrong header size. The CRC is 16 bits of independent
agreement PER CHUNK over bytes the header does not describe.

WHAT A NEGATIVE PRINTS. Every run prints chunks_scanned, crc_ok, crc_bad, bytes consumed vs file size,
and WHY the walk stopped. THREE SHAPES OF NOT-CLEAN, all non-zero exits, because `crc_bad == 0` on its own
is not a result: zero chunks is REFUSED (exit 2) — the walk never started, so the hypothesis was never
exercised; any CRC mismatch is 1 — FALSIFIED; and a walk that stops anywhere other than the 0xFFFFFFFF
sentinel is 1 — a VALID PREFIX with bytes left unexplained, which is what a truncated file produces and
which this tool used to exit 0 on.

BLIND SPOTS, printed on every run:
  * it does NOT decompress, so a header match does not prove the bitstream codec;
  * it cannot read the RNC method byte (1 or 2) — that byte lives in the stripped magic, so the method
    must be settled by attempting decompression with both, never assumed;
  * the UNPACKED CRC is unchecked, because checking it needs a working decompressor. Today's result
    covers the FRAMING only.
"""
import os
import struct
import sys

BLIND_SPOTS = [
    "does NOT decompress — an RNC *header* match does not prove the bitstream codec",
    "cannot read the RNC method byte (1 or 2): it lives in the stripped 4-byte magic",
    "unpacked CRC is unchecked (would require a working decompressor) — this covers FRAMING only",
]


def crc16_arc(buf: bytes) -> int:
    """CRC-16/ARC (poly 0xA001 reflected, init 0) — the CRC RNC ProPack uses."""
    crc = 0
    for b in buf:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def probe(path, verbose=True):
    """(chunks, crc_ok, crc_bad, consumed, size, reason_stopped)."""
    data = open(path, "rb").read()
    size = len(data)
    cur = 0
    chunks = crc_ok = crc_bad = 0
    reason = "ran off the end without a sentinel"
    while cur + 4 <= size:
        if data[cur:cur + 4] == b"\xff\xff\xff\xff":
            reason = "0xFFFFFFFF sentinel"
            break
        if cur + 0x0E > size:
            reason = f"header truncated at 0x{cur:X}"
            break
        ulen, plen = struct.unpack_from(">II", data, cur)
        ucrc, pcrc = struct.unpack_from(">HH", data, cur + 8)
        leeway, nchunk = data[cur + 0x0C], data[cur + 0x0D]
        if plen == 0 or ulen == 0 or cur + 0x0E + plen > size:
            reason = (f"implausible lengths at 0x{cur:X} "
                      f"(ulen={ulen} plen={plen} remaining={size - cur - 0x0E})")
            break
        got = crc16_arc(data[cur + 0x0E: cur + 0x0E + plen])
        ok = (got == pcrc)
        crc_ok += ok
        crc_bad += (not ok)
        chunks += 1
        if verbose and (chunks <= 6 or not ok):
            print(f"  chunk {chunks:3d} @0x{cur:06X} ulen={ulen:7d} plen={plen:7d} "
                  f"ucrc={ucrc:04X} pcrc={pcrc:04X} crc16arc={got:04X} "
                  f"{'OK' if ok else 'MISMATCH'} leeway={leeway} nchunk={nchunk}")
        cur += 0x0E + plen
    return chunks, crc_ok, crc_bad, cur, size, reason


def report(path, verbose=True):
    """Exit status IS the machine-readable result, so the three failure shapes must all be non-zero:
         2  the walk never started (zero chunks) — the hypothesis was never exercised;
         1  a chunk's packed CRC mismatched — the hypothesis is FALSIFIED on this file;
         1  the walk stopped anywhere other than the 0xFFFFFFFF sentinel — the file is not accounted
            for end to end, and `crc_bad == 0` over a PREFIX is not a clean result. A truncated .RAW
            walks a valid prefix and then dies on implausible lengths; before this, that printed
            crc_bad=0 and exited 0, i.e. reported clean over a broken corpus."""
    c, ok, bad, consumed, size, reason = probe(path, verbose=verbose)
    print(f"{path}: chunks_scanned={c} crc_ok={ok} crc_bad={bad} "
          f"consumed={consumed}/{size} bytes stopped_because='{reason}'")
    for b in BLIND_SPOTS:
        print(f"  blind spot: {b}")
    if c == 0:
        print(f"  REFUSED: parsed ZERO chunks from {size} bytes — this is NOT a clean result, the "
              f"hypothesis was never exercised.")
        return 2
    if bad:
        print(f"  FALSIFIED: {bad} of {c} chunks failed the packed-CRC check.")
        return 1
    if "sentinel" not in reason:
        print(f"  NOT CLEAN: the walk stopped at 0x{consumed:X} without reaching the 0xFFFFFFFF "
              f"sentinel ({size - consumed} of {size} bytes unexplained), because {reason}. "
              f"{ok} chunks did verify — that is a VALID PREFIX, not a confirmed file.")
        return 1
    return 0


def selftest(pos, neg):
    """Four classes: the real file must confirm; a non-.RAW, a truncation and a single flipped payload
    byte must each be REJECTED. NEGATIVE C is the one that proves the CRC check FIRES rather than being
    computed and ignored."""
    if not (os.path.isfile(pos) and os.path.isfile(neg)):
        print(f"REFUSING: need two real files; got pos={pos!r} exists={os.path.isfile(pos)} "
              f"neg={neg!r} exists={os.path.isfile(neg)}. Populate the corpus with "
              f"`python3 tools/extract_disc_files.py`.", file=sys.stderr)
        return 2
    tmp = []
    fails = []
    try:
        print(f"[POSITIVE] {pos} — must parse >0 chunks, 0 CRC mismatches, consume to a sentinel")
        c, ok, bad, consumed, size, reason = probe(pos)
        print(f"  chunks={c} crc_ok={ok} crc_bad={bad} consumed={consumed}/{size} stopped='{reason}'")
        if c == 0:
            fails.append("positive parsed 0 chunks")
        if bad:
            fails.append(f"positive had {bad} CRC mismatches")
        if "sentinel" not in reason:
            fails.append(f"positive did not reach a sentinel: {reason}")
        rc1 = report(pos, verbose=False)
        print(f"  report() exit status = {rc1} (must be 0 — the positive is also gated on the SHIPPED "
              "path, so a report() made strict enough to reject the negatives cannot start rejecting "
              "real files)")
        if rc1 != 0:
            fails.append(f"positive exited {rc1} from report()")

        print(f"[NEGATIVE A] {neg} (a real file that is NOT a .RAW) — must FAIL, asserted on report()'s "
              "exit status")
        rc2 = report(neg, verbose=False)
        print(f"  report() exit status = {rc2} (must be non-zero)")
        if rc2 == 0:
            fails.append("NEGATIVE A passed as a valid .RAW — the probe cannot discriminate")

        d = open(pos, "rb").read()
        print("[NEGATIVE B] the positive, truncated to a third — must FAIL, and the assertion is on the "
              "CLI's own EXIT STATUS via report(), not on probe()'s internal tuple: the gated class and "
              "the shipped path must be the same path. (A truncation walks a valid prefix with "
              "crc_bad=0; asserting on the tuple let report() keep exiting 0 over it.)")
        t = pos + ".selftest-trunc"
        tmp.append(t)
        open(t, "wb").write(d[:len(d) // 3])
        rc3 = report(t, verbose=False)
        print(f"  report() exit status = {rc3} (must be non-zero)")
        if rc3 == 0:
            fails.append("NEGATIVE B (truncated) exited 0 from report() — the CLI reports CLEAN over a "
                         "walk that never reached a sentinel and left bytes unexplained")

        print("[NEGATIVE C] the positive with payload byte 0x20 flipped — the CRC must catch it")
        m = bytearray(d)
        m[0x20] ^= 0xFF
        t2 = pos + ".selftest-corrupt"
        tmp.append(t2)
        open(t2, "wb").write(bytes(m))
        c4, ok4, bad4, _, _, r4 = probe(t2, verbose=False)
        rc4 = report(t2, verbose=False)
        print(f"  chunks={c4} crc_ok={ok4} crc_bad={bad4} stopped='{r4}'; report() exit = {rc4}")
        if rc4 == 0:
            fails.append("NEGATIVE C: report() exited 0 on a corrupted payload")
        if bad4 == 0:
            fails.append("NEGATIVE C: a corrupted payload produced ZERO CRC mismatches — the CRC check "
                         "is not firing, so every 'crc_ok' this tool has ever printed is meaningless")
    finally:
        for t in tmp:
            if os.path.isfile(t):
                os.remove(t)

    print()
    if fails:
        for f in fails:
            print(f"FAIL: {f}")
        return 1
    print("SELFTEST PASS: positive confirmed, all 3 negatives rejected on report()'s own exit status.")
    return 0


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        sys.exit(2)
    if a[0] == "--selftest":
        if len(a) != 3:
            print("--selftest needs <a real .RAW> <a file that is NOT a .RAW>", file=sys.stderr)
            sys.exit(2)
        sys.exit(selftest(a[1], a[2]))
    sys.exit(max(report(p) for p in a))
