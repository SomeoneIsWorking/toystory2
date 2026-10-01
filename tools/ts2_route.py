#!/usr/bin/env python3
"""ts2_route.py — Toy Story 2 pad routes as EXACT-FRAME input schedules.

A route is a list of taps `FRAME:BUTTON[:HOLD]`, where FRAME is a PAD FRAME: the index of the host
logic frame whose `Pad::serviceFrame` resolves the controller mask the guest receives (one per
`stepFrame`, counted from boot). The schedule is compiled into psxport's own replay file
(`PSXPORT_PAD_REPLAY`), which the framework applies inside `serviceFrame` after every other input
source. Nothing is polled against wall-clock time, so a tap lands on its frame on every run.

The file is psxport's v1 phase-keyed `.pad` container, written through the framework's own
`tools/psx_pad.py` so the format has exactly one spelling. A tap is numbered from BOOT, which is not a
phase-relative offset, so the route is written as ONE EXPLICITLY UNKEYED segment: the runtime replays
an unkeyed segment absolutely from boot (the meaning these frame numbers have) and reports the card
identity as unknown rather than borrowing a phase key the route never observed.

    uv run --frozen python tools/ts2_route.py --route andys-room --write scratch/route/andys-room.pad
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

FRAMEWORK = Path(__file__).resolve().parents[1] / "external" / "psxport"
sys.path.insert(0, str(FRAMEWORK / "tools"))

# PSX digital pad, active low (bit clear = pressed). The framework's own bit table, not a second copy.
from psx_pad import NEUTRAL, PSX_BUTTON_BITS, UNKEYED_PHASE, encode  # noqa: E402

BUTTON_MASKS = PSX_BUTTON_BITS
IDLE = NEUTRAL
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
    """The v1 `.pad` bytes for `length` pad frames from boot, as one unkeyed segment; overlapping taps
    merge as the controller would (union of pressed bits). A tap reaching past `length` is refused,
    because a truncated press would be a different input than the one the route states."""
    masks = [IDLE] * length
    for tap in taps:
        if tap.end > length:
            raise ValueError(f"tap {tap} ends at frame {tap.end}, past the {length}-frame schedule")
        for frame in range(tap.frame, tap.end):
            masks[frame] &= ~BUTTON_MASKS[tap.button] & 0xFFFF
    runs: list[tuple[int, int]] = []
    for value in masks:
        if runs and runs[-1][0] == value:
            runs[-1] = (value, runs[-1][1] + 1)
        else:
            runs.append((value, 1))
    # card kind 0 = unknown: this route says nothing about the memory card it was measured against.
    return encode(0, bytes(32), [(UNKEYED_PHASE, runs)])


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--route", choices=sorted(ROUTES))
    parser.add_argument("--write", type=Path)
    args = parser.parse_args()
    if not args.route or not args.write:
        parser.error("--route and --write are required")
    taps = ROUTES[args.route]
    args.write.write_bytes(compile_pad(taps, max(tap.end for tap in taps) + 1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
