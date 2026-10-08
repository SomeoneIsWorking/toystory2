#!/usr/bin/env python3
"""Exact-frame pad schedule: taps in, psxport `.pad` bytes out.

A tap is `FRAME:BUTTON[:HOLD]`, FRAME being the pad frame (host logic frame whose `Pad::serviceFrame`
resolves the mask, counted from boot). The schedule compiles into psxport's replay file
(`PSXPORT_PAD_REPLAY`), applied after every other input source. tools/headless_run.py takes `--tap`.

An absolute schedule assumes how many pad frames boot, the front end and movies consume, so it rots when
any changes (docs/issues/0044). Use it as a live probe; keep long-lived routes as recorded phase-keyed
`.pad` files under replays/.
"""
from __future__ import annotations

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
    """The v1 `.pad` bytes for `length` pad frames from boot, as one unkeyed segment; overlapping taps merge
    (union of pressed bits). A tap reaching past `length` is refused rather than truncated."""
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
