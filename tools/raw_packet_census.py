#!/usr/bin/env python3
"""Census the unclaimed packet IDs at the start of verified Toy Story 2 .RAW chunks.

  python3 tools/raw_packet_census.py FILE.RAW [MORE.RAW...]
  python3 tools/raw_packet_census.py --all
  python3 tools/raw_packet_census.py --selftest

This is RE-15's deliberately narrow first step. It establishes the source denominator and raw
little-endian u32 command values only; it assigns NO meanings to IDs. The tool always decodes through
raw_unpack.decode_data(), so every reported header came from a complete container whose packed and
unpacked CRCs passed. A decoded chunk shorter than four bytes is a falsification of this header
hypothesis, never an ignored record.
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

from raw_unpack import DecodedRawChunk, default_corpus, decode_data


@dataclass(frozen=True)
class PacketIdStats:
    chunk_count: int
    decoded_bytes: int
    body_bytes: int
    minimum_chunk_bytes: int
    maximum_chunk_bytes: int


def census_chunks(chunks: list[DecodedRawChunk], label: str) -> tuple[int, dict[int, PacketIdStats] | None]:
    """Return raw header statistics, refusing an incomplete four-byte header.

    This is the shipping boundary for packet interpretation. It intentionally knows nothing about
    the semantic meaning or payload layout of a command ID.
    """
    aggregate: dict[int, list[int]] = {}
    for chunk in chunks:
        if len(chunk.payload) < 4:
            print(
                f"{label}: FAIL chunk {chunk.index} @0x{chunk.offset:06X} decodes to "
                f"{len(chunk.payload)} byte(s), fewer than the proposed u32 packet header",
                file=sys.stderr,
            )
            return 1, None
        packet_id = struct.unpack_from("<I", chunk.payload)[0]
        values = aggregate.setdefault(packet_id, [0, 0, 0, len(chunk.payload), len(chunk.payload)])
        values[0] += 1
        values[1] += len(chunk.payload)
        values[2] += len(chunk.payload) - 4
        values[3] = min(values[3], len(chunk.payload))
        values[4] = max(values[4], len(chunk.payload))
    return 0, {
        packet_id: PacketIdStats(
            chunk_count=values[0],
            decoded_bytes=values[1],
            body_bytes=values[2],
            minimum_chunk_bytes=values[3],
            maximum_chunk_bytes=values[4],
        )
        for packet_id, values in aggregate.items()
    }


def merge_stats(totals: dict[int, list[int]], stats: dict[int, PacketIdStats]) -> None:
    for packet_id, value in stats.items():
        aggregate = totals.setdefault(
            packet_id,
            [0, 0, 0, value.minimum_chunk_bytes, value.maximum_chunk_bytes],
        )
        aggregate[0] += value.chunk_count
        aggregate[1] += value.decoded_bytes
        aggregate[2] += value.body_bytes
        aggregate[3] = min(aggregate[3], value.minimum_chunk_bytes)
        aggregate[4] = max(aggregate[4], value.maximum_chunk_bytes)


def census_file(path: Path) -> tuple[int, dict[int, PacketIdStats] | None]:
    try:
        data = path.read_bytes()
    except OSError as error:
        print(f"{path}: REFUSED: cannot read input: {error}", file=sys.stderr)
        return 2, None
    rc, chunks = decode_data(data, str(path), verbose=False)
    if rc != 0:
        return rc, None
    assert chunks is not None
    return census_chunks(chunks, str(path))


def print_summary(files: int, totals: dict[int, list[int]]) -> None:
    total_chunks = sum(values[0] for values in totals.values())
    total_bytes = sum(values[1] for values in totals.values())
    print(
        f"packet-header census: files={files} decoded_chunks={total_chunks} decoded_bytes={total_bytes} "
        f"distinct_ids={len(totals)}"
    )
    print("  id          chunks  decoded_bytes  body_bytes  chunk_bytes[min,max]")
    for packet_id in sorted(totals):
        chunks, decoded_bytes, body_bytes, minimum, maximum = totals[packet_id]
        print(
            f"  0x{packet_id:08X}  {chunks:6d}  {decoded_bytes:13d}  {body_bytes:10d}  "
            f"[{minimum},{maximum}]"
        )


def selftest() -> int:
    # This targets the same census function invoked after real container decoding. raw_unpack's
    # selftest separately exercises the shared decompression and CRC path; here the discriminator is
    # the packet-header contract: a full u32 is accepted and a short decoded chunk must be rejected.
    positive = [
        DecodedRawChunk(index=0, offset=0x10, payload=struct.pack("<I", 0x10203040) + b"abc"),
        DecodedRawChunk(index=1, offset=0x20, payload=struct.pack("<I", 0x10203040) + b"d"),
        DecodedRawChunk(index=2, offset=0x30, payload=struct.pack("<I", 0x00000011)),
    ]
    print("[POSITIVE] two raw IDs with bounded decoded-size statistics")
    rc, stats = census_chunks(positive, "synthetic-packets")
    failures: list[str] = []
    if rc != 0 or stats is None:
        failures.append(f"positive exited {rc}")
    else:
        first = stats[0x10203040]
        second = stats[0x11]
        if (first.chunk_count, first.decoded_bytes, first.body_bytes, first.minimum_chunk_bytes, first.maximum_chunk_bytes) != (
            2,
            12,
            4,
            5,
            7,
        ):
            failures.append(f"wrong first-id statistics: {first}")
        if (second.chunk_count, second.body_bytes) != (1, 0):
            failures.append(f"wrong second-id statistics: {second}")

    print("[NEGATIVE] a decoded chunk shorter than a u32 header must fail")
    rc, stats = census_chunks([DecodedRawChunk(index=0, offset=0x44, payload=b"\x01\x02\x03")], "synthetic-short")
    if rc != 1 or stats is not None:
        failures.append(f"short-header negative exited {rc} with {stats}")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("SELFTEST PASS: raw IDs are counted and short headers are rejected on the shipping census path.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Census raw u32 packet IDs in verified TS2 .RAW chunks.")
    parser.add_argument("files", nargs="*", help=".RAW containers to decode and census")
    parser.add_argument("--all", action="store_true", help="census every *.RAW under scratch/flat/")
    parser.add_argument("--selftest", action="store_true", help="run hermetic census controls")
    args = parser.parse_args()
    if args.selftest:
        return selftest()

    files = [Path(file) for file in args.files]
    if args.all:
        raws, _ = default_corpus()
        if not raws:
            print("REFUSED: no *.RAW under scratch/flat/", file=sys.stderr)
            return 2
        files.extend(raws)
    if not files:
        parser.print_help()
        return 2

    totals: dict[int, list[int]] = {}
    successful_files = 0
    failures: Counter[int] = Counter()
    for path in files:
        rc, stats = census_file(path)
        if rc != 0:
            failures[rc] += 1
            continue
        assert stats is not None
        merge_stats(totals, stats)
        successful_files += 1

    if failures:
        print(
            f"FAIL: checked {len(files)} file(s), decoded {successful_files}; "
            f"failures={dict(sorted(failures.items()))}. No partial census is accepted.",
            file=sys.stderr,
        )
        return 1 if failures[1] else 2
    print_summary(successful_files, totals)
    return 0


if __name__ == "__main__":
    sys.exit(main())
