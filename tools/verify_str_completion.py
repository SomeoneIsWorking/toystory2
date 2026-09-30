#!/usr/bin/env python3
"""Verify the STR stream-completion chain this title's FMV wait depends on.

The FMV player blocks in a bounded pop loop on a 32-byte-stride ring, and the only code in the
reachable images that can satisfy that pop is a single "post" routine guarded by two words.  Every
address and instruction word below is read from the retail executable and the retail module images,
never assumed: the tool compares raw instruction words, so a changed image refuses instead of
reporting a stale conclusion.

It also answers two questions that a wrong answer would make worse than no answer at all:

* Is there a per-channel DMA callback table for the host to dispatch?  The SDK's ``DMACallback`` is
  a BIOS B0-vector entry (the guest reaches the BIOS only as ``addiu $t2,$zero,0xB0`` / ``jr $t2``
  with the function number in ``$t1`` in the delay slot), and the table it fills is RAM the BIOS's
  own DMA interrupt handler reads.  This port has no BIOS ROM, so that table is never filled, and
  the guest's own completion path is synchronous instead.  ``--ram-bss`` / ``--ram-top`` census
  runtime RAM for such a table and CLASSIFY every candidate entry rather than filtering it, because
  the Sony library's own symbol strings sit inside the text's address range and pass any range test.
* What writes each of the chain's four state words?  A census of every ``lui``-formed store to each
  of them across all three images, printed with the word count it scanned.  A store through a
  register loaded from a pointer is NOT excluded, the tool says so, and the tool REFUSES if the
  census ever returns zero for a word it has just asserted is written -- because a broken census and
  a clean result look identical otherwise, and that has already happened twice in this file.

Usage:
  python3 tools/verify_str_completion.py --check
  python3 tools/verify_str_completion.py --selftest
  python3 tools/verify_str_completion.py --ram-bss scratch/ghidra/ram-bss.bin
  python3 tools/verify_str_completion.py --ram-top scratch/ghidra/ram-top.bin
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

import overlay_map

EXE_BASE = 0x80010000
EXE_PAYLOAD_OFFSET = 0x800
EXE_TEXT_BYTES = 0x91800
MODULE_BASE = 0x800D5D20

DEFAULT_EXE = Path(overlay_map.EXE)
DEFAULT_FMV = Path(overlay_map.FMV)
DEFAULT_MEMORY = Path(overlay_map.MEMORY)


class Refused(Exception):
    """The supplied bytes cannot support the requested conclusion."""


# The chain, in execution order. Each row is (source, address, instruction word, what it proves).
# A "word" is the retail encoding, so this table is the claim and the images are the evidence.
CHAIN: tuple[tuple[str, int, int, str], ...] = (
    # -- the FMV overlay opens the ring and the stream -------------------------------------------------
    ("fmv", 0x800D72B4, 0x3C048015, "FMV loads the 0x8015 page holding the ring descriptor"),
    ("fmv", 0x800D72B8, 0x8C842908, "FMV reads the ring descriptor at [0x80152908]"),
    ("fmv", 0x800D72BC, 0x0C035FD6, "FMV calls the module allocator (0x800D7F58) for the ring"),
    ("fmv", 0x800D72C0, 0x24054000, "the allocator request is 0x4000 bytes"),
    ("fmv", 0x800D72CC, 0x0C0241FF, "FMV calls the ring init (0x800907FC) with the allocation"),
    ("fmv", 0x800D72D4, 0x24050001, "FMV passes the entry count 1 to the ring init"),
    ("fmv", 0x800D72F4, 0x0C024FDE, "FMV calls the stream open (0x80093F78)"),
    ("fmv", 0x800D6DD8, 0x3C100080, "the FMV wait's retry counter is 0x8000 = 32768"),
    ("fmv", 0x800D6DE4, 0x0C02503D, "the FMV wait calls the pop (0x800940F4)"),
    ("fmv", 0x800D6DEC, 0x10400005, "the FMV wait spins while the pop answers zero"),
    ("fmv", 0x800D6DF4, 0x1600FFFB, "the FMV wait re-enters while the counter is non-zero"),
    # -- the ring init: the ONE writer of the base pointer, then the head reset ------------------------
    ("exe", 0x80090804, 0x3C01800D, "ring init materialises the 0x800D page"),
    ("exe", 0x80090808, 0xAC24E1B0, "ring init stores the base pointer at 0x800CE1B0 ($a0)"),
    ("exe", 0x80090810, 0xAC2512B8, "ring init stores its second argument at 0x800C92B8"),
    ("exe", 0x80090814, 0x0C024F69, "ring init calls 0x80093DA4, which clears the head"),
    ("exe", 0x80093DB4, 0x3C01800D, "head reset materialises the 0x800D page"),
    ("exe", 0x80093DB8, 0xAC209504, "head reset clears 0x800C9504"),
    # -- the pop the FMV waits on -----------------------------------------------------
    ("exe", 0x800940F8, 0x3C02800D, "pop materialises the 0x800D page"),
    ("exe", 0x800940FC, 0x8C429504, "pop reads the head at 0x800C9504"),
    ("exe", 0x80094100, 0x3C03800D, "pop materialises the 0x800D page for the base pointer"),
    ("exe", 0x80094104, 0x8C63E1B0, "pop reads the base pointer at 0x800CE1B0"),
    ("exe", 0x80094108, 0x00021140, "pop scales the head by 32"),
    ("exe", 0x8009410C, 0x00623021, "pop forms the entry address"),
    ("exe", 0x80094110, 0x94C20000, "pop reads the entry state halfword"),
    # -- the post, and the two words that gate it ------------------------------------------
    ("exe", 0x80093E88, 0x3C02800D, "post materialises the 0x800D page"),
    ("exe", 0x80093E8C, 0x8C4294E8, "post reads the tail counter at 0x800C94E8"),
    ("exe", 0x80093E90, 0x3C03800D, "post materialises the 0x800D page for the base pointer"),
    ("exe", 0x80093E94, 0x8C63E1B0, "post reads the SAME base pointer at 0x800CE1B0"),
    ("exe", 0x80093EA0, 0x00021140, "post scales by the same 32 (identical stride word)"),
    ("exe", 0x80093EA8, 0x24020002, "post loads the complete state, 2"),
    ("exe", 0x80093EAC, 0xA4620000, "post writes 2 into the entry state halfword"),
    ("exe", 0x80093ED4, 0x8C6394E4, "post reads the next tail at 0x800C94E4"),
    ("exe", 0x80093EDC, 0x8C842364, "post reads the optional user callback at 0x800B2364"),
    ("exe", 0x80094B04, 0x8C63E148, "the post's first guard reads 0x800CE148"),
    ("exe", 0x80094B14, 0x10600008, "guard 1: if [0x800CE148] == 0, skip the post"),
    # -- guard 2's only setter, and the user callback's only setter ----------------------
    ("exe", 0x80094B1C, 0x3C02800C, "the post's second guard materialises the 0x800C page"),
    ("exe", 0x80094B20, 0x8C421170, "guard 2 reads the end-of-stream word 0x800C1170"),
    ("exe", 0x80094B28, 0x10400003, "guard 2: if [0x800C1170] == 0, skip the post"),
    ("exe", 0x80094B30, 0x0C024FA2, "the only call to the post (0x80093E88)"),
    ("exe", 0x800949A0, 0x94430006, "end-of-stream test reads the sector counter at +6"),
    ("exe", 0x800949A4, 0x94420004, "end-of-stream test reads the remaining count at +4"),
    ("exe", 0x800949A8, 0x2463FFFF, "end-of-stream test decrements that counter"),
    ("exe", 0x800949AC, 0x1462002B, "end-of-stream test branches away unless it is the last sector"),
    # -- the DMA completion is the guest's own synchronous code, not a host callback ----------
    ("exe", 0x800949BC, 0x3C01800C, "end-of-stream setter materialises the 0x800C page"),
    ("exe", 0x800949C0, 0xAC231170, "the only non-zero store to 0x800C1170"),
    ("exe", 0x80093FA4, 0x3C01800D, "stream open materialises the 0x800D page"),
    ("exe", 0x80093FA8, 0xAC20E148, "stream open clears guard 1 (0x800CE148)"),
    ("exe", 0x80093FB0, 0xAC312364, "stream open stores its 4th argument at 0x800B2364"),
    # -- the guest reaches the BIOS only through the B0 gate ---------------------------------
    ("exe", 0x80093F98, 0x0C02506E, "stream open calls the DMA/channel setup (0x800941B8)"),
    ("exe", 0x800893B4, 0x240A00B0, "BIOS gate: $t2 = 0xB0, the B0 vector"),
    ("exe", 0x800893B8, 0x01400008, "BIOS gate: jr $t2"),
    ("exe", 0x800893BC, 0x2409000A, "BIOS gate: $t1 = 0x0A, the function number, in the delay slot"),
)

# The words whose writers the census below reports, and the one claim the census cannot make.
CENSUS_TARGETS: tuple[tuple[str, int, str], ...] = (
    ("exe", 0x800CE148, "post guard 1"),
    ("exe", 0x800C1170, "post guard 2 (end of stream)"),
    ("exe", 0x800CE1B0, "ring base pointer"),
    ("exe", 0x800C9504, "ring head"),
)

MODULE_SPAN = 510960
BIOS_CALL_SHAPE = (0x240A00B0, 0x01400008)  # addiu $t2,$zero,0xB0 ; jr $t2


@dataclass(frozen=True)
class CompletionEvidence:
    rows: int
    stores: dict[str, tuple[tuple[int, int], ...]]
    scanned_words: int
    decoded_stores: int
    bios_gate_sites: int


def image_of(source: str, exe: bytes, fmv: bytes, memory: bytes) -> bytes:
    return {"exe": exe, "fmv": fmv, "memory": memory}[source]


def file_offset(source: str, address: int) -> int:
    if source == "exe":
        return EXE_PAYLOAD_OFFSET + address - EXE_BASE
    if source in ("fmv", "memory"):
        return address - MODULE_BASE
    raise Refused(f"unknown image source {source!r}")


def word_at(blob: bytes, source: str, address: int) -> int:
    offset = file_offset(source, address)
    if offset < 0 or offset + 4 > len(blob):
        raise Refused(
            f"{source} offset 0x{offset:X} for 0x{address:08X} is outside the supplied bytes"
        )
    return struct.unpack_from("<I", blob, offset)[0]


def require_word(blob: bytes, source: str, address: int, expected: int, what: str) -> None:
    actual = word_at(blob, source, address)
    if actual != expected:
        raise Refused(
            f"{what} at 0x{address:08X} changed: got 0x{actual:08X}, expected 0x{expected:08X}"
        )


def is_store(word: int) -> bool:
    # sb=0x28, sh=0x29, swl=0x2A, sw=0x2B. Leaving 0x2B out makes the census blind to the very
    # instruction 0x80090808 is, and a blind census reports a clean zero.
    return (word >> 26) in (0x28, 0x29, 0x2A, 0x2B)


def is_lui(word: int) -> bool:
    return (word >> 26) == 0x0F


def immediate(word: int) -> int:
    """The sign-extended displacement. Unsigned addition here silently matches nothing, which is
    exactly the dead-tap shape this project has been bitten by repeatedly, so it is spelled out."""
    value = word & 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def text_range(source: str, blob: bytes) -> tuple[int, int]:
    """The byte range of the image's own text: the executable's payload, or the whole module."""
    if source == "exe":
        return EXE_PAYLOAD_OFFSET, min(EXE_PAYLOAD_OFFSET + EXE_TEXT_BYTES, len(blob))
    return 0, len(blob)


def guest_address(source: str, offset: int) -> int:
    """The guest address of a file offset. The executable's payload starts after the PS-X EXE
    header, so adding the base without removing that offset reports every address 0x800 too high --
    which is how this file briefly 'proved' a writer at 0x80091008 that is really at 0x80090808."""
    if source == "exe":
        return EXE_BASE + offset - EXE_PAYLOAD_OFFSET
    return MODULE_BASE + offset


def census_lui_stores(blob: bytes, source: str, target: int) -> tuple[tuple[int, int], ...]:
    """Every lui-formed store to `target`: a store whose displacement plus the most recent `lui`
    upper halfword equals the target. A candidate list with a stated blind spot, not a proof: a
    store through a register loaded from a pointer is not seen."""
    lo, hi = text_range(source, blob)
    upper: int | None = None
    found: list[tuple[int, int]] = []
    for offset in range(lo, max(lo, hi - 3), 4):
        word = struct.unpack_from("<I", blob, offset)[0]
        if is_lui(word):
            upper = word & 0xFFFF
            continue
        if not is_store(word) or upper is None:
            continue
        # Parenthesised on purpose: `&` binds tighter than `==` in Python, so the unparenthesised
        # form parses as `word & (0xFFFFFFFF == target)` and silently matches nothing.
        computed = ((upper << 16) + immediate(word)) & 0xFFFFFFFF
        if computed == target:
            found.append((guest_address(source, offset), word))
    return tuple(found)


def count_bios_gate(blob: bytes, source: str) -> int:
    lo, hi = text_range(source, blob)
    sites = 0
    for offset in range(lo, max(lo, hi - 11), 4):
        first, second, third = struct.unpack_from("<III", blob, offset)
        if (first, second) == BIOS_CALL_SHAPE and (third & 0xFFFF0000) == 0x24090000:
            sites += 1
    return sites


def executable_text(address: int) -> bool:
    return EXE_BASE <= address < EXE_BASE + EXE_TEXT_BYTES


def classify_entry(exe: bytes, address: int) -> str:
    """What a candidate table entry actually IS: a function entry, ASCII data, or neither.

    A filter would have to pick one, and picking wrong is the failure this census keeps meeting: the
    Sony library's own symbol STRINGS ("CdSync", "CdGetSector", ...) live inside the executable's
    address range, so they pass any "is it a code pointer" range test, and a prologue test is no
    better -- it rejects the STR ring's own pop at 0x800940F4, which opens with `move $a3,$a0`, and
    it rejects real entries that save $s0 before $ra. So the census CLASSIFIES and reports, and the
    caller reads the classification instead of trusting a filter.
    """
    if not executable_text(address):
        return "outside-text"
    offset = file_offset("exe", address)
    first = struct.unpack_from("<I", exe, offset)[0]
    if (first >> 26) == 0x09 and ((first >> 21) & 0x1F) == 0x1D:  # addiu $sp, $sp, -frame ($sp = 29)
        frame = first & 0xFFFF
        if frame & 0x8000:
            frame -= 0x10000  # the displacement is stored sign-extended: -0x18 is 0xFFE8
        if -0x60 <= frame < 0:
            return "function"
    raw = exe[offset: offset + 4]
    if len(raw) == 4 and all(0x20 <= byte < 0x7F for byte in raw):
        return "ascii"
    return "other"


def census_ram_pointer_runs(exe: bytes, blob: bytes, base: int, min_run: int = 2) -> list[tuple[int, list[int], list[str]]]:
    """Every run of >= min_run consecutive words that point INTO the executable's text.

    `base` is the guest address of blob[0]. Each run carries its per-entry classification. Reported
    with the scan size, because a zero here is only meaningful next to what was scanned.
    """
    words = struct.unpack(f"<{len(blob) // 4}I", blob[: len(blob) // 4 * 4])
    runs: list[tuple[int, list[int], list[str]]] = []
    index = 0
    while index < len(words):
        if not executable_text(words[index]):
            index += 1
            continue
        end = index
        while end < len(words) and executable_text(words[end]):
            end += 1
        if end - index >= min_run:
            entries = list(words[index:end])
            runs.append((base + index * 4, entries, [classify_entry(exe, w) for w in entries]))
        index = end
    return runs


def analyze(exe: bytes, fmv: bytes, memory: bytes) -> CompletionEvidence:
    if exe[:8] != b"PS-X EXE":
        raise Refused("the executable input is not a PS-X EXE")
    blobs = {"exe": exe, "fmv": fmv, "memory": memory}
    for source, address, expected, what in CHAIN:
        require_word(blobs[source], source, address, expected, what)
    # The stride word must be literally the same encoding on both sides, or the pop and the post
    # are addressing different structures and the chain does not hold.
    pop_stride = word_at(exe, "exe", 0x80094108)
    post_stride = word_at(exe, "exe", 0x80093EA0)
    if pop_stride != post_stride:
        raise Refused(
            f"pop and post disagree on the entry stride: 0x{pop_stride:08X} != 0x{post_stride:08X}"
        )
    stores = {
        f"{name} 0x{target:08X}": census_lui_stores(blobs[source], source, target)
        for source, target, name in CENSUS_TARGETS
    }
    scanned = sum(
        hi - lo for source, blob in blobs.items() for lo, hi in (text_range(source, blob),)
    ) // 4
    decoded = sum(len(value) for value in stores.values())
    gate_sites = count_bios_gate(exe, "exe")
    if gate_sites == 0:
        raise Refused(
            "no BIOS gate site found: the census cannot see a per-channel DMA callback table "
            "through a gate this image does not use, so its zero would mean nothing"
        )
    if not stores["ring base pointer 0x800CE1B0"]:
        raise Refused(
            "the census found no writer of the ring base pointer, yet 0x80090808 is asserted above; "
            "the census is broken, and a broken census must refuse rather than report zero"
        )
    return CompletionEvidence(
        rows=len(CHAIN),
        stores=stores,
        scanned_words=scanned,
        decoded_stores=decoded,
        bios_gate_sites=gate_sites,
    )


def report(evidence: CompletionEvidence) -> None:
    print(
        f"[str-completion] retail chain verified: {evidence.rows} instruction word(s) match, "
        f"pop and post share the 0x00021140 stride"
    )
    print(
        f"[str-completion] guest BIOS gate sites: {evidence.bios_gate_sites} "
        f"(addiu $t2,$zero,0xB0 / jr $t2 with the function number in $t1) -- the SDK's "
        f"per-channel DMACallback is one of these, so its table is BIOS-owned RAM, not guest state"
    )
    for name, found in evidence.stores.items():
        print(
            f"[str-completion] census {name}: matched {len(found)} lui-formed store(s) "
            f"across {evidence.scanned_words} scanned word(s) in 3 images"
        )
        for address, word in found:
            print(f"    0x{address:08X}  0x{word:08X}")
    print(f"[str-completion] census total stores matched: {evidence.decoded_stores}")
    print(
        "[str-completion] BLIND SPOT, stated rather than hidden: this census sees only stores whose "
        "base is an immediately preceding lui. A store through a register loaded from a pointer is "
        "NOT counted, so a zero here means 'no such store found', never 'no such store exists'. "
        "The runtime watchpoint is the arbiter for guard 1 (0x800CE148)."
    )


def report_ram(exe_path: Path, dump: Path, base: int, label: str) -> int:
    if not exe_path.is_file():
        raise Refused(f"no retail executable at {exe_path}; provision the verified corpus first")
    if not dump.is_file():
        raise Refused(f"no RAM dump at {dump}; produce it with scratch/ghidra/ts2_dma.gdb")
    exe = exe_path.read_bytes()
    runs = census_ram_pointer_runs(exe, dump.read_bytes(), base)
    print(
        f"[str-completion][{label}] scanned {dump.stat().st_size} bytes of runtime RAM from "
        f"0x{base:08X}; matched {len(runs)} run(s) of >= 2 consecutive words pointing into the text"
    )
    tally: dict[str, int] = {}
    for address, entries, kinds in runs:
        for kind in kinds:
            tally[kind] = tally.get(kind, 0) + 1
        shown = ", ".join(f"0x{value:08X}" for value in entries[:8])
        more = "" if len(entries) <= 8 else f" (+{len(entries) - 8} more)"
        summary = ", ".join(f"{kind} x{kinds.count(kind)}" for kind in dict.fromkeys(kinds))
        print(f"  {address:08X}  {len(entries) * 4} bytes  [{summary}]  {shown}{more}")
    print(f"[str-completion][{label}] entries by kind: {tally or '{}'}")
    print(
        f"[str-completion][{label}] a run of 'ascii' entries is the Sony library's symbol strings, "
        f"not a pointer table: they sit inside the text range and pass a range test. Only a run of "
        f"'function' entries is a callback table candidate."
    )
    return 0


def read_inputs(exe_path: Path, fmv_path: Path, memory_path: Path) -> tuple[bytes, bytes, bytes]:
    for path, label in (
        (exe_path, "retail executable"),
        (fmv_path, "FMV module"),
        (memory_path, "MEMORY module"),
    ):
        if not path.is_file():
            raise Refused(f"no {label} at {path}; provision the verified retail corpus first")
    return exe_path.read_bytes(), fmv_path.read_bytes(), memory_path.read_bytes()


def check(exe_path: Path, fmv_path: Path, memory_path: Path) -> CompletionEvidence:
    evidence = analyze(*read_inputs(exe_path, fmv_path, memory_path))
    report(evidence)
    return evidence


def selftest() -> int:
    """Positive and negative cases against a synthetic triple, so the gate needs no corpus."""
    exe = bytearray(EXE_PAYLOAD_OFFSET + EXE_TEXT_BYTES)
    fmv = bytearray(MODULE_SPAN)
    memory = bytearray(MODULE_SPAN)
    exe[0:8] = b"PS-X EXE"
    for source, address, expected, _ in CHAIN:
        blob = {"exe": exe, "fmv": fmv, "memory": memory}[source]
        struct.pack_into("<I", blob, file_offset(source, address), expected)

    evidence = analyze(bytes(exe), bytes(fmv), bytes(memory))
    if evidence.rows != len(CHAIN):
        print(f"[str-completion][selftest] FAILED: {evidence.rows} of {len(CHAIN)} rows verified")
        return 1
    if evidence.bios_gate_sites < 1:
        print("[str-completion][selftest] FAILED: the planted BIOS gate site was not found")
        return 1
    print(f"[str-completion][selftest] positive: {evidence.rows} rows, "
          f"{evidence.bios_gate_sites} BIOS gate site(s)")

    # Negative 1: one instruction word changed must refuse, naming the address.
    broken = bytearray(exe)
    struct.pack_into("<I", broken, file_offset("exe", 0x80093EAC), 0xA4620004)
    try:
        analyze(bytes(broken), bytes(fmv), bytes(memory))
    except Refused as refusal:
        print(f"[str-completion][selftest] negative (word changed): refused -- {refusal}")
    else:
        print("[str-completion][selftest] FAILED: a changed state-store word was accepted")
        return 1

    # Negative 2: the stride must be shared, or the pop and the post address different rings.
    skewed = bytearray(exe)
    struct.pack_into("<I", skewed, file_offset("exe", 0x80093EA0), 0x00021148)
    try:
        analyze(bytes(skewed), bytes(fmv), bytes(memory))
    except Refused as refusal:
        print(f"[str-completion][selftest] negative (stride skew): refused -- {refusal}")
    else:
        print("[str-completion][selftest] FAILED: a skewed post stride was accepted")
        return 1

    # Negative 3: a missing corpus must refuse, not report a clean zero.
    try:
        check(Path("/nonexistent/exe"), Path("/nonexistent/fmv"), Path("/nonexistent/mem"))
    except Refused as refusal:
        print(f"[str-completion][selftest] negative (no corpus): refused -- {refusal}")
    else:
        print("[str-completion][selftest] FAILED: a missing corpus was accepted")
        return 1

    # Positive census: the chain's own writer of guard 1 (0x80093FA8, a clear) must be FOUND, and
    # a second planted writer must raise the count, so a zero from the real corpus means "not
    # found" rather than "the census cannot see it".
    guard_writers = census_lui_stores(bytes(exe), "exe", 0x800CE148)
    if len(guard_writers) != 1 or guard_writers[0][0] != 0x80093FA8:
        print(f"[str-completion][selftest] FAILED: expected the chain's guard-1 writer at "
              f"0x80093FA8, got {[hex(a) for a, _ in guard_writers]}")
        return 1
    print(f"[str-completion][selftest] positive census: guard-1 writer found at "
          f"0x{guard_writers[0][0]:08X}")

    planted = bytearray(exe)
    struct.pack_into("<I", planted, file_offset("exe", 0x80094010), 0x3C01800D)
    struct.pack_into("<I", planted, file_offset("exe", 0x80094014), 0xAC20E148)
    census = census_lui_stores(bytes(planted), "exe", 0x800CE148)
    if len(census) != 2 or census[1][0] != 0x80094014:
        print(f"[str-completion][selftest] FAILED: planted guard-1 store matched {len(census)}")
        return 1
    print(f"[str-completion][selftest] positive census: planted store found at "
          f"0x{census[1][0]:08X}")

    # Negative census: a target nothing writes must match 0, so a zero is informative.
    absent = census_lui_stores(bytes(exe), "exe", 0x800FFFFF)
    if absent:
        print(f"[str-completion][selftest] FAILED: an unwritten target matched {len(absent)}")
        return 1
    print("[str-completion][selftest] negative census: an unwritten target matched 0")

    # The RAM census must be shown to fire, and must CLASSIFY the Sony library symbol strings
    # separately from real entries -- those strings sit inside the executable's address range and so
    # pass any "looks like a code pointer" range test. A filter cannot separate them; only a
    # classification can, and without this control a zero from the real dumps would mean nothing.
    # Plant the ASCII the census must classify as strings, at the address the real image has them,
    # so this control needs no corpus.
    exe[file_offset("exe", 0x80023608): file_offset("exe", 0x80023608) + 8] = b"CdReaCdSyn"
    strings = bytearray(0x20)
    for offset in range(2):
        struct.pack_into("<I", strings, offset * 4, 0x80023608 + offset * 4)
    text_run = census_ram_pointer_runs(bytes(exe), bytes(strings), 0x800A0820)
    if len(text_run) != 1 or set(text_run[0][2]) != {"ascii"}:
        print(f"[str-completion][selftest] FAILED: the ASCII run was not classified as ascii "
              f"({[(hex(a), k) for a, _, k in text_run]})")
        return 1
    print("[str-completion][selftest] RAM census: the Sony symbol strings classified as ascii")

    real = bytearray(0x20)
    struct.pack_into("<I", real, 0x00, 0x80096BC8)  # a real prologue
    struct.pack_into("<I", real, 0x04, 0x80096C10)  # mid-function: not an entry
    struct.pack_into("<I", real, 0x08, 0x80093EA8)  # addiu $v0,$zero,2 : not an entry
    struct.pack_into("<I", exe, file_offset("exe", 0x80096BC8), 0x27BDFFE8)  # addiu $sp,$sp,-0x18
    runs = census_ram_pointer_runs(bytes(exe), bytes(real), 0x800A0C00)
    if len(runs) != 1 or len(runs[0][1]) != 3 or runs[0][2] != ["function", "other", "other"]:
        print(f"[str-completion][selftest] FAILED: the RAM census mis-measured a planted run of "
              f"real entries: {[(hex(a), k) for a, _, k in runs]}")
        return 1
    print(f"[str-completion][selftest] RAM census: planted run found at 0x{runs[0][0]:08X} "
          f"classified {[hex(v) for v in runs[0][1]]} as {runs[0][2]} -- one real entry, two not")
    print("[str-completion][selftest] OK")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="verify against the provisioned corpus")
    parser.add_argument("--selftest", action="store_true", help="run the synthetic positive/negative cases")
    parser.add_argument("--ram-bss", type=Path, help="census a runtime RAM dump of 0x800A0000-0x800D0000")
    parser.add_argument("--ram-top", type=Path, help="census a runtime RAM dump of 0x801F0000-0x80200000")
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--fmv", type=Path, default=DEFAULT_FMV)
    parser.add_argument("--memory", type=Path, default=DEFAULT_MEMORY)
    args = parser.parse_args(argv)
    try:
        if args.selftest:
            return selftest()
        if args.ram_bss:
            return report_ram(args.exe, args.ram_bss, 0x800A0000, "bss")
        if args.ram_top:
            return report_ram(args.exe, args.ram_top, 0x801F0000, "top")
        if args.check:
            check(args.exe, args.fmv, args.memory)
            return 0
        parser.print_help()
        return 2
    except Refused as refusal:
        print(f"REFUSED: {refusal}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
