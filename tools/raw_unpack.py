#!/usr/bin/env python3
"""raw_unpack.py — RE-08: decompress Toy Story 2's .RAW asset container.

  python3 tools/raw_unpack.py FILE.RAW...                # verify every chunk, write nothing
  python3 tools/raw_unpack.py --all                      # verify every *.RAW under scratch/flat/
  python3 tools/raw_unpack.py FILE.RAW --unpack OUT      # also write concatenated unpacked bytes
  python3 tools/raw_unpack.py --selftest                 # positive AND negative classes

WHAT THIS ASSERTS. A TS2 .RAW is a stream of chunks whose 14-byte header keeps the standard RNC
field layout minus magic (be32 unpackedLen, be32 packedLen, be16 unpackedCRC, be16 packedCRC,
leeway, chunkCount), terminated by a 0xFFFFFFFF sentinel — but the payload is NOT RNC ProPack.
It decodes with Traveller's Tales' own LZ scheme, the routine TSR's Ghidra research names
`DecompressRAW` (mateusfavarin/tsr, MIT — docs/references.md). A chunk PASSES only when BOTH
CRC fields verify: packedCRC-16/ARC over the compressed payload AND unpackedCRC-16/ARC over the
decoded bytes, and the decoder ends exactly at unpackedLen.

THE METHOD-BYTE QUESTION IS CLOSED BY MEASUREMENT, NOT ASSUMPTION. The frontier step asked
"RNC method 1 or 2?" because the method lives in a magic TS2 was thought to strip. Measured
2026-08-24: ancient-format RNC1/RNC2 (new AND old variants) fail on 38/39 LEVEL01 chunks;
this decoder passes every chunk of every .RAW measured, with both CRCs. There is no RNC method
byte to recover — the premise dissolved. TRAP FOR THE NEXT SESSION: tiny chunk LEVEL01#19
(35->64 B) ALSO decodes under RNC2-new with both CRCs passing and output matching an independent
extraction. One chunk "confirming" RNC2 proves nothing; the corpus does.

ALGORITHM PROVENANCE. The decoder below is a faithful transcription of mateusfavarin/tsr's
`scripts/extract_raw.py::decompress_raw_chunk` (MIT), which itself derives from the game's own
`DecompressRAW` (Ghidra). Deliberately kept close to the decompiled control flow — the flag-bit
shift register, its masking points and the interleaved literal/match state machine are exactly
what the game executes; renaming deeper would trade provable correctness for looks. SHAPE taken
and cited per docs/references.md; nothing vendored.

EXIT CODES (distinct, machine-readable):
  0  PASS    — every chunk of every named file decompressed with both CRCs verified;
  1  FAIL    — the hypothesis was exercised and contradicted (CRC mismatch either field, decode
               error, size mismatch, or a stream that never reaches its sentinel);
  2  REFUSED — nothing was asserted: zero chunks parsed (not a .RAW), missing corpus, or a
               --unpack input count error.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from raw_probe import SENTINEL_REASON, crc16_arc, walk


class DecompressionError(Exception):
    """The bitstream violated the format."""


def decompress_tt(payload_with_header: bytes) -> bytes:
    """Decode one chunk (14-byte header + payload) with TT's DecompressRAW.

    Faithful transcription of tsr scripts/extract_raw.py::decompress_raw_chunk.
    State names follow the decompiled code (src3/src4 cursors, bitbuf flag
    register, u6 length accumulator, u11 distance-high accumulator) so the
    provenance stays checkable line by line.
    """
    if len(payload_with_header) < 0x0F:
        raise DecompressionError("chunk too small for header + bitstream seed")
    expected_size = struct.unpack_from(">I", payload_with_header, 0)[0]
    chunk = payload_with_header
    n = len(chunk)

    out = bytearray()
    src3 = 0x0F
    bitbuf = ((chunk[0x0E] * 2 + 1) * 2) & 0xFFFFFFFF

    def next_flag(src_pos: int, buf: int):
        """Consume one flag bit MSB-first, refilling from stream bytes."""
        bit = ((buf * 2) >> 8) & 1
        buf = (buf * 2) & 0xFF
        if buf == 0:
            if src_pos >= n:
                raise DecompressionError("stream ended while refilling bit buffer")
            refill = chunk[src_pos]
            src_pos += 1
            buf = refill * 2 + bit
            bit = (buf >> 8) & 1
        return src_pos, buf, bit

    while True:
        # Literal phase: flag 0 emits up to two literals before a match flag.
        while True:
            bit11 = ((bitbuf * 2) >> 8) & 1
            src4 = src3
            u6 = (bitbuf * 2) & 0xFFFFFFFF
            if bit11 != 0:
                break
            src4 = src3 + 1
            if src3 >= n:
                raise DecompressionError("literal read past compressed chunk")
            out.append(chunk[src3])

            bitbuf = (bitbuf * 4) & 0xFFFFFFFF
            bit11 = (bitbuf >> 8) & 1
            u6 = bitbuf
            if bit11 != 0:
                break
            if src4 >= n:
                raise DecompressionError("literal read past compressed chunk")
            src3 = src4 + 1
            out.append(chunk[src4])

        bitbuf = u6 & 0xFF
        if bitbuf == 0:
            if src4 >= n:
                raise DecompressionError("bit-buffer refill past compressed chunk")
            refill = chunk[src4]
            src4 += 1
            bitbuf = refill * 2 + bit11
            if ((bitbuf >> 8) & 1) == 0:
                if src4 >= n:
                    raise DecompressionError("literal read past compressed chunk")
                src3 = src4 + 1
                out.append(chunk[src4])
                continue

        # Match phase.
        u6 = 2
        u11 = 0
        direct_backref = False

        src4, bitbuf, u12 = next_flag(src4, bitbuf)

        if u12 == 0:
            src3 = src4
            src3, bitbuf, u12 = next_flag(src3, bitbuf)
            u6 = u12 + 4
            src3, bitbuf, u10 = next_flag(src3, bitbuf)
            if u10 != 0:
                src3, bitbuf, u6 = next_flag(src3, bitbuf)
                u6 = (u12 + 3) * 2 + u6
                if u6 == 9:
                    # dword-RLE: 4 flag bits give a dword count, then raw dwords
                    copy_dwords = 0
                    for _ in range(4):
                        src3, bitbuf, u6 = next_flag(src3, bitbuf)
                        copy_dwords = copy_dwords * 2 + u6
                    copy_dwords += 3
                    needed = copy_dwords * 4
                    if src3 + needed > n:
                        raise DecompressionError("RLE block past compressed chunk")
                    out.extend(chunk[src3: src3 + needed])
                    src3 += needed
                    if len(out) < expected_size:
                        continue
                    if len(out) == expected_size:
                        return bytes(out)
                    raise DecompressionError("decoded size exceeded expected")
                src4 = src3
        else:
            src4, bitbuf, u12 = next_flag(src4, bitbuf)
            if u12 == 0:
                direct_backref = True
            else:
                u6 = 3
                src4, bitbuf, u12 = next_flag(src4, bitbuf)
                src3 = src4
                if u12 != 0:
                    if src4 >= n:
                        raise DecompressionError("length extension past compressed chunk")
                    src3 = src4 + 1
                    u6 = chunk[src4] + 8
                    if chunk[src4] != 0:
                        src4 = src3
                    else:
                        src3, bitbuf, u6 = next_flag(src3, bitbuf)
                        if u6 == 0:
                            if len(out) != expected_size:
                                raise DecompressionError(
                                    f"chunk ended at {len(out)} bytes, expected {expected_size}")
                            return bytes(out)
                        continue

        if not direct_backref:
            src4, bitbuf, u12 = next_flag(src3, bitbuf)
            if u12 != 0:
                src4, bitbuf, u11 = next_flag(src4, bitbuf)
                src4, bitbuf, u12 = next_flag(src4, bitbuf)
                if u12 == 0:
                    if u11 == 0:
                        u11 = 1
                        src4, bitbuf, u12 = next_flag(src4, bitbuf)
                        u11 = u11 * 2 + u12
                else:
                    src4, bitbuf, u12 = next_flag(src4, bitbuf)
                    u11 = (u11 * 2 + u12) | 4
                    src4, bitbuf, u12 = next_flag(src4, bitbuf)
                    if u12 == 0:
                        src4, bitbuf, u12 = next_flag(src4, bitbuf)
                        u11 = u11 * 2 + u12
                u11 <<= 8

        if src4 >= n:
            raise DecompressionError("back-reference distance past compressed chunk")
        distance = chunk[src4] | u11
        src3 = src4 + 1

        copy_src = len(out) - distance
        src_word = copy_src - 1
        if copy_src < 0:
            raise DecompressionError("back-reference before start of output")

        if (u6 & 1) != 0:
            if copy_src - 1 < 0:
                raise DecompressionError("odd-length back-reference underflow")
            out.append(out[copy_src - 1])
            src_word = copy_src

        repeats = (u6 >> 1) - 1
        if repeats < -1:
            raise DecompressionError("invalid repeat count")

        if distance == 0:
            if src_word < 0:
                raise DecompressionError("zero-distance back-reference underflow")
            repeated_byte = out[src_word]
            for _ in range(repeats + 1):
                out.append(repeated_byte)
                out.append(repeated_byte)
        else:
            for _ in range(repeats + 1):
                if src_word + 1 >= len(out):
                    raise DecompressionError("back-reference beyond output")
                out.append(out[src_word])
                out.append(out[src_word + 1])
                src_word += 2

        if len(out) > expected_size:
            raise DecompressionError(f"decoded size exceeded expected ({expected_size})")


def verify_data(data: bytes, label: str) -> tuple[int, bytes | None]:
    """Verify one complete container and return (exit status, decoded bytes).

    This is the single production path used by both file inputs and the
    hermetic regression controls.  Refused/failing inputs return no output.
    """
    chunks, consumed, reason = walk(data)
    print(f"{label}: {len(chunks)} chunks, consumed {consumed}/{len(data)} bytes, "
          f"stopped_because='{reason}'")
    if not chunks:
        print(f"  REFUSED: zero chunks parsed from {len(data)} bytes — not a .RAW; "
              f"nothing was asserted.")
        return 2, None
    if reason != SENTINEL_REASON:
        print(f"  FAIL: stream stopped before the sentinel ({len(data) - consumed} bytes "
              f"unexplained): {reason}")
        return 1, None
    bad = 0
    total_raw = 0
    decoded = []
    for i, ch in enumerate(chunks):
        off, ulen, plen, ucrc, pcrc, _leeway, nchunk, payload = ch
        got_packed = crc16_arc(payload)
        if got_packed != pcrc:
            bad += 1
            print(f"  chunk {i} @0x{off:06X}: FAIL packed CRC {got_packed:04X} != "
                  f"header {pcrc:04X} — not decoding a payload that contradicts its header")
            decoded.append(None)
            continue
        try:
            out = decompress_tt(data[off:off + 0x0E + plen])
        except DecompressionError as e:
            bad += 1
            print(f"  chunk {i} @0x{off:06X}: FAIL decode: {e}")
            decoded.append(None)
            continue
        got_unpacked = crc16_arc(out)
        if len(out) != ulen or got_unpacked != ucrc:
            bad += 1
            print(f"  chunk {i} @0x{off:06X}: FAIL unpacked len={len(out)} (want {ulen}) "
                  f"CRC {got_unpacked:04X} (want {ucrc:04X})")
            decoded.append(None)
            continue
        decoded.append(out)
        total_raw += len(out)
        if i < 6:
            print(f"  chunk {i} @0x{off:06X}: OK ulen={ulen} plen={plen} "
                  f"packed_crc ok unpacked_crc ok")
    ok = len(chunks) - bad
    print(f"  summary: chunks={len(chunks)} ok={ok} failed={bad} "
          f"unpacked_bytes={total_raw} leeway/chunkcount="
          f"{sorted({(ch[5], ch[6]) for ch in chunks})[:6]}")
    if bad:
        print(f"  FAIL: {bad} of {len(chunks)} chunks did not decompress+verify.")
        return 1, None
    return 0, b"".join(d for d in decoded if d is not None)


def verify_file(path: Path, want_output: Path | None) -> int:
    rc, blob = verify_data(path.read_bytes(), str(path))
    if rc == 0 and want_output:
        assert blob is not None
        want_output.write_bytes(blob)
        print(f"  wrote {want_output} ({len(blob)} bytes)")
    return rc


def default_corpus():
    flat = Path(__file__).resolve().parent.parent / "scratch" / "flat"
    return sorted(flat.glob("*.RAW")), sorted(flat.glob("*.DAT"))


def synthetic_literal_container() -> bytes:
    """Build a copyright-free one-chunk stream for hermetic regression tests.

    ``5e 41 00`` is the format's minimal literal ``A`` followed by its normal
    end marker. Header lengths and both CRCs are derived here rather than
    copied as expected constants, so mutations still exercise the production
    framing and verification path.
    """
    unpacked = b"A"
    packed = b"\x5eA\x00"
    header = struct.pack(
        ">IIHHBB",
        len(unpacked),
        len(packed),
        crc16_arc(unpacked),
        crc16_arc(packed),
        0,
        0,
    )
    return header + packed + b"\xff\xff\xff\xff"


def selftest() -> int:
    positive = synthetic_literal_container()
    fails = []

    print("[POSITIVE] hermetic literal stream — decode A with both CRCs verified")
    rc, output = verify_data(positive, "synthetic-positive.RAW")
    if rc != 0 or output != b"A":
        fails.append(f"positive exited {rc} with output {output!r}")

    print("[NEGATIVE A] non-.RAW bytes — must REFUSE (exit 2)")
    rc, _ = verify_data(b"not a RAW container", "synthetic-not-raw.DAT")
    if rc != 2:
        fails.append(f"negative A exited {rc}, expected 2")

    print("[NEGATIVE B] positive without its sentinel — valid chunk is not a clean stream")
    rc, _ = verify_data(positive[:-4], "synthetic-no-sentinel.RAW")
    if rc != 1:
        fails.append(f"negative B exited {rc}, expected 1")

    mut = bytearray(positive)
    mut[0x0F] ^= 0xFF
    print("[NEGATIVE C] flipped payload byte — packed CRC must catch it (exit 1)")
    rc, _ = verify_data(bytes(mut), "synthetic-corrupt-payload.RAW")
    if rc != 1:
        fails.append(f"negative C exited {rc}, expected 1")

    # NEGATIVE D: leave the payload alone, corrupt ONLY the unpacked-CRC header
    # field — proves the unpacked-CRC gate fires independently of the packed one.
    mutD = bytearray(positive)
    mutD[0x08] ^= 0xFF
    print("[NEGATIVE D] flipped UNPACKED-CRC header field, payload intact (exit 1)")
    rc, _ = verify_data(bytes(mutD), "synthetic-corrupt-ucrc.RAW")
    if rc != 1:
        fails.append(f"negative D exited {rc}, expected 1")

    print()
    if fails:
        for f in fails:
            print(f"FAIL: {f}")
        return 1
    print("SELFTEST PASS: positive verified; negatives A-D rejected on the shipped exit paths.")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Verify/decompress TS2 .RAW containers (TT DecompressRAW codec, both CRCs).",
        epilog="See module docstring for assertions, provenance and the RNC trap.")
    ap.add_argument("files", nargs="*", help=".RAW files to verify")
    ap.add_argument("--all", action="store_true",
                    help="verify every *.RAW under scratch/flat/ (prints per-file verdicts)")
    ap.add_argument("--unpack", metavar="OUT", type=Path,
                    help="write the concatenated unpacked bytes of a single input file")
    ap.add_argument("--selftest", action="store_true",
                    help="run hermetic positive and negative decoder controls")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    files = [Path(f) for f in a.files]
    if a.all:
        raws, _ = default_corpus()
        if not raws:
            print("REFUSED: no *.RAW under scratch/flat/", file=sys.stderr)
            return 2
        files.extend(raws)
    if not files:
        ap.print_help()
        return 2
    outs = [None] * len(files)
    if a.unpack:
        if len(files) != 1:
            print("--unpack needs exactly one input file", file=sys.stderr)
            return 2
        outs[0] = a.unpack
    return max(verify_file(f, o) for f, o in zip(files, outs))


if __name__ == "__main__":
    sys.exit(main())
