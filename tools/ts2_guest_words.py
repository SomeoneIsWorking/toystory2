#!/usr/bin/env python3
"""ts2_guest_words.py — Toy Story 2 guest words read from an exact 2 MiB RAM dump, with their evidence.

The dumps come from `PSXPORT_PAD_DUMP_AT` (tools/headless_run.py `--dump-at`): the whole guest RAM at an
exact pad frame. Every address here is derived from instruction bytes of the retail executable; the
`evidence` strings name them so `tools/ts2_guest_words.py --selftest` can re-check them against a dump.

PLAYER OBJECT. The resident object updater at 0x8004CF30..0x8004CF98 loads the object's position and
stores it as the previous position, then calls the player update 0x800489C4:

    0x8004CF6C  lw $v0, 0($s0)    X                0x8004CF78  sw $a1, 0x64($s0)  previous Z
    0x8004CF70  lw $v1, 4($s0)    Y                0x8004CF88  sw $v0, 0x5C($s0)  previous X
    0x8004CF64  lw $a1, 8($s0)    Z                0x8004CF8C  sw $v1, 0x60($s0)  previous Y
    0x8004CF94  jal 0x800489C4    (player update; its $s3 is this object)

and 0x800489C4 reads the controller-derived word `[0x800A15BC] & 0xF0` (the d-pad) at 0x80048A04..0x80048A0C,
then `lw $a0,($s3)` / `lw $a0,8($s3)` (X, Z) at 0x80048AE8 / 0x80048B00 and the yaw halfword
`lh $v1,0xE($s3)` at 0x80048AC4. The object lives at 0x800B2188 in LEVEL01: it is all zero until the
"PRESS X" card is confirmed, which is when Buzz is created.
"""

from __future__ import annotations

import struct
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

RAM_BYTES = 2 * 1024 * 1024
RAM_BASE = 0x80000000

PLAYER_OBJECT = 0x800B2188
PLAYER_X = PLAYER_OBJECT + 0x00
PLAYER_Y = PLAYER_OBJECT + 0x04
PLAYER_Z = PLAYER_OBJECT + 0x08
PLAYER_YAW = PLAYER_OBJECT + 0x0C  # halfword at +0xE, 12-bit angle (low 12 bits); read as a word here
PLAYER_PREVIOUS_X = PLAYER_OBJECT + 0x5C
PLAYER_PREVIOUS_Y = PLAYER_OBJECT + 0x60
PLAYER_PREVIOUS_Z = PLAYER_OBJECT + 0x64
PAD_WORD = 0x800A1480  # the pad decoder's result, stored by the poll at 0x8003B360 (`sh $v0,0x1480($at)`)
PAD_WORD_PREVIOUS = 0x800A11E4  # the previous poll's word, stored at 0x8003B350
MAPPED_INPUT_WORD = 0x800A15BC  # the control-mapped word (0x80073518 `sh $a2,0x15bc($at)`)
PAD_LEFT = 0x0080


class RamDump:
    """One exact guest RAM image; reads are bounds-checked and a wrong-size file is refused."""

    def __init__(self, data: bytes) -> None:
        if len(data) != RAM_BYTES:
            raise ValueError(f"a RAM dump is exactly {RAM_BYTES} bytes, got {len(data)}")
        self.data = data

    @classmethod
    def read(cls, path: Path) -> "RamDump":
        return cls(path.read_bytes())

    def word(self, address: int) -> int:
        offset = address - RAM_BASE
        if address % 4 or not 0 <= offset <= RAM_BYTES - 4:
            raise ValueError(f"0x{address:08X} is not an aligned word inside guest RAM")
        return struct.unpack_from("<i", self.data, offset)[0]

    def half(self, address: int) -> int:
        offset = address - RAM_BASE
        if address % 2 or not 0 <= offset <= RAM_BYTES - 2:
            raise ValueError(f"0x{address:08X} is not an aligned halfword inside guest RAM")
        return struct.unpack_from("<H", self.data, offset)[0]

    def instruction(self, address: int) -> int:
        return self.word(address) & 0xFFFFFFFF


@dataclass(frozen=True)
class PlayerState:
    x: int
    y: int
    z: int
    yaw: int  # 12-bit angle, 0..4095

    @property
    def exists(self) -> bool:
        """Buzz is created when the "PRESS X" card is confirmed; before that the object is all zero."""
        return (self.x, self.y, self.z) != (0, 0, 0)

    def horizontal_distance_squared(self, other: "PlayerState") -> int:
        return (self.x - other.x) ** 2 + (self.z - other.z) ** 2


def read_player(dump: RamDump) -> PlayerState:
    return PlayerState(dump.word(PLAYER_X), dump.word(PLAYER_Y), dump.word(PLAYER_Z), dump.half(PLAYER_OBJECT + 0xE) & 0xFFF)


def encode(opcode: int, rs: int, rt: int, imm: int) -> int:
    return (opcode << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)


# (address, instruction word) pairs this module's addresses were derived from. Registers: s0=16 s3=19
# v0=2 v1=3 a0=4 a1=5. Opcodes: lw=0x23, lh=0x21, sw=0x2B.
EVIDENCE = (
    (0x8004CF6C, encode(0x23, 16, 2, 0)),  # lw $v0, 0($s0)   X
    (0x8004CF70, encode(0x23, 16, 3, 4)),  # lw $v1, 4($s0)   Y
    (0x8004CF64, encode(0x23, 16, 5, 8)),  # lw $a1, 8($s0)   Z
    (0x8004CF88, encode(0x2B, 16, 2, 0x5C)),  # sw $v0, 0x5C($s0) previous X
    (0x8004CF8C, encode(0x2B, 16, 3, 0x60)),  # sw $v1, 0x60($s0) previous Y
    (0x8004CF78, encode(0x2B, 16, 5, 0x64)),  # sw $a1, 0x64($s0) previous Z
    (0x80048AE8, encode(0x23, 19, 4, 0)),  # lw $a0, 0($s3)   player X
    (0x80048B00, encode(0x23, 19, 4, 8)),  # lw $a0, 8($s3)   player Z
    (0x80048AC4, encode(0x21, 19, 3, 0xE)),  # lh $v1, 0xE($s3) player yaw
)


def unmatched_evidence(dump: RamDump, evidence=EVIDENCE) -> list[tuple[int, int, int]]:
    """Every (address, expected, found) whose instruction in `dump` differs from the cited word."""
    return [(a, want, dump.instruction(a)) for a, want in evidence if dump.instruction(a) != want]


class GuestWordsTest(unittest.TestCase):
    def blank(self) -> bytearray:
        return bytearray(RAM_BYTES)

    def test_wrong_size_dump_is_refused(self):
        with self.assertRaises(ValueError):
            RamDump(b"\0" * 10)

    def test_reads_are_bounds_and_alignment_checked(self):
        dump = RamDump(bytes(RAM_BYTES))
        for bad in (0x80000002, 0x80200000, 0x7FFFFFFC):
            with self.assertRaises(ValueError):
                dump.word(bad)

    def test_player_exists_only_once_its_position_is_nonzero(self):
        ram = self.blank()
        self.assertFalse(read_player(RamDump(bytes(ram))).exists)
        struct.pack_into("<iii", ram, PLAYER_X - RAM_BASE, 194774, 60026, -361401)
        struct.pack_into("<H", ram, PLAYER_OBJECT - RAM_BASE + 0xE, 0xAE4 | 0xF000)
        player = read_player(RamDump(bytes(ram)))
        self.assertTrue(player.exists)
        self.assertEqual((player.x, player.y, player.z, player.yaw), (194774, 60026, -361401, 0xAE4))

    def test_evidence_check_reports_a_mismatching_instruction_and_accepts_a_matching_one(self):
        ram = self.blank()
        for address, word in EVIDENCE:
            struct.pack_into("<I", ram, address - RAM_BASE, word)
        self.assertEqual(unmatched_evidence(RamDump(bytes(ram))), [])
        struct.pack_into("<I", ram, EVIDENCE[0][0] - RAM_BASE, 0)
        self.assertEqual([m[0] for m in unmatched_evidence(RamDump(bytes(ram)))], [EVIDENCE[0][0]])

    def test_encoding_matches_the_retail_bytes(self):
        # 0x8004CF6C in the dump reads `0000028e` little-endian = 0x8E020000 = lw $v0, 0($s0)
        self.assertEqual(encode(0x23, 16, 2, 0), 0x8E020000)


def main() -> int:
    if sys.argv[1:] == ["--selftest"]:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(GuestWordsTest)
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    print(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main())
