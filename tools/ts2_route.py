#!/usr/bin/env python3
"""ts2_route.py — Toy Story 2 pad routes as EXACT-FRAME input schedules.

A route is a list of taps `FRAME:BUTTON[:HOLD]`, where FRAME is a PAD FRAME: the index of the host
logic frame whose `Pad::serviceFrame` resolves the controller mask the guest receives (one per
`stepFrame`, counted from boot). The schedule is compiled into psxport's own replay file
(`PSXPORT_PAD_REPLAY`, uint16 little-endian active-low mask per pad frame), which the framework applies
inside `serviceFrame` after every other input source. Nothing is polled against wall-clock time, so a
tap lands on its frame on every run.

    uv run --frozen python tools/ts2_route.py --selftest
    uv run --frozen python tools/ts2_route.py --route andys-room --write scratch/route/andys-room.pad
"""

from __future__ import annotations

import argparse
import struct
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

# PSX digital pad, active low (bit clear = pressed). The same masks as psxport's control channel.
BUTTON_MASKS = {
    "select": 0x0001,
    "start": 0x0008,
    "up": 0x0010,
    "right": 0x0020,
    "down": 0x0040,
    "left": 0x0080,
    "l2": 0x0100,
    "r2": 0x0200,
    "l1": 0x0400,
    "r1": 0x0800,
    "triangle": 0x1000,
    "circle": 0x2000,
    "cross": 0x4000,
    "square": 0x8000,
}
IDLE = 0xFFFF
DEFAULT_HOLD = 4


@dataclass(frozen=True)
class Tap:
    frame: int
    button: str
    hold: int = DEFAULT_HOLD

    @property
    def end(self) -> int:
        return self.frame + self.hold


def parse_tap(text: str) -> Tap:
    """`FRAME:BUTTON[:HOLD]`; anything else is a refusal, never a silently dropped input."""
    parts = text.split(":")
    if len(parts) not in (2, 3) or not parts[0].isdigit() or parts[1] not in BUTTON_MASKS:
        raise ValueError(f"tap {text!r} is not FRAME:BUTTON[:HOLD] with a button from {sorted(BUTTON_MASKS)}")
    hold = int(parts[2]) if len(parts) == 3 else DEFAULT_HOLD
    if len(parts) == 3 and not parts[2].isdigit() or hold < 1:
        raise ValueError(f"tap {text!r} must hold at least one frame")
    return Tap(int(parts[0]), parts[1], hold)


def compile_pad(taps: tuple[Tap, ...], length: int) -> bytes:
    """One little-endian uint16 per pad frame for `length` frames; overlapping taps merge as the
    controller would (union of pressed bits). A tap reaching past `length` is refused, because a
    truncated press would be a different input than the one the route states."""
    masks = [IDLE] * length
    for tap in taps:
        if tap.end > length:
            raise ValueError(f"tap {tap} ends at frame {tap.end}, past the {length}-frame schedule")
        for frame in range(tap.frame, tap.end):
            masks[frame] &= ~BUTTON_MASKS[tap.button] & 0xFFFF
    return struct.pack(f"<{length}H", *masks)


# Named routes. Frames are PAD FRAMES from boot, measured on the retail disc: the four intro movies end
# near pad frame 440, "PRESS START" blinks until the first tap, the story movie between the level-select
# confirm and "PRESS X" ends before frame 780 (an opened capture shows the "LEVEL 1: ANDY'S HOUSE" card),
# and Buzz's object exists in guest RAM from the "PRESS X" confirm on (tools/ts2_guest_words.py).
TITLE_TO_ANDYS_ROOM: tuple[Tap, ...] = (
    Tap(500, "start"),  # title "PRESS START"
    Tap(560, "cross"),  # main menu: START GAME
    Tap(620, "cross"),  # level select: Andy's House
    Tap(790, "cross"),  # "LEVEL 1: ANDY'S HOUSE, PRESS X"
)
ROUTES: dict[str, tuple[Tap, ...]] = {"andys-room": TITLE_TO_ANDYS_ROOM}


class RouteTest(unittest.TestCase):
    def test_taps_parse_and_malformed_ones_are_refused(self):
        self.assertEqual(parse_tap("600:start"), Tap(600, "start", 4))
        self.assertEqual(parse_tap("640:cross:6"), Tap(640, "cross", 6))
        for bad in ("start", "600", "x:start", "600:start:0", "600:start:1:2", "600:1", "600:fire", "600:start:x"):
            with self.assertRaises(ValueError, msg=bad):
                parse_tap(bad)

    def test_compiled_schedule_presses_exactly_the_requested_frames(self):
        data = compile_pad((Tap(2, "cross", 2), Tap(3, "up", 1)), 6)
        masks = struct.unpack("<6H", data)
        self.assertEqual(masks[0], IDLE)
        self.assertEqual(masks[1], IDLE)
        self.assertEqual(masks[2], IDLE & ~0x4000)
        self.assertEqual(masks[3], IDLE & ~0x4000 & ~0x0010)
        self.assertEqual(masks[4], IDLE)

    def test_a_tap_past_the_schedule_is_refused_not_truncated(self):
        with self.assertRaises(ValueError):
            compile_pad((Tap(5, "cross", 4),), 8)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--route", choices=sorted(ROUTES))
    parser.add_argument("--write", type=Path)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(RouteTest)
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    if not args.route or not args.write:
        parser.error("--route and --write are required")
    taps = ROUTES[args.route]
    args.write.write_bytes(compile_pad(taps, max(tap.end for tap in taps) + 1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
