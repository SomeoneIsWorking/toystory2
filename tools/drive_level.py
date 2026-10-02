#!/usr/bin/env python3
"""drive_level.py — walk Buzz through Toy Story 2's own level data, on the control channel alone.

The level's pickups are not a host-side invention: the guest holds the token ids itself and raises its
own flag when one is collected. This tool reads that data and steers with the d-pad, so the game
decides everything that happens and the host only asks Buzz to walk.

    uv run --frozen python tools/headless_run.py --control-port 5963 --tap 500:start ... &
    uv run --frozen python tools/drive_level.py --port 5963                 # walk the current area
    uv run --frozen python tools/drive_level.py --port 5963 --save scratch/x.pad

GUEST MEMORY IS NEVER WRITTEN. The only channels used are `press`/`release` (input), `rw` (read) and
`padrec save` (write a file). Nothing here stores a byte into the guest, sets a flag, or moves Buzz
anywhere he did not choose to walk: the loop closes on the position the guest itself reports.

Guest state this reads, all established from the executable rather than guessed:

  0x800B2188   Buzz's object: X, Y, Z as three words in world units; the tools below shift them by 5
               to match the level's own 16-bit object coordinates.
  0x800A1274   pointer to the area's object table: [count][entry...] — one entry per placed object.
  entry+0x10   the object's record; record+0x14 points at its (X, Y, Z); record+0x18 is non-zero while
               the object exists (the guest's own `FUN_80048638` existence test).
  0x800A8668   the five token ids the level holds, 16-bit at stride 8 (`FUN_8007678c` case 2 walks
               this table and compares each object's id against it).
  0x800A1544   the guest's own "a token was collected" flag; 0x800A866C + 16*slot is that slot's state.
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FRAMEWORK = ROOT / "external" / "psxport"
sys.path.insert(0, str(FRAMEWORK / "tools"))

from dbgclient import LiveClient  # noqa: E402

BUTTONS = ("up", "down", "left", "right")
PLAYER = 0x800B2188
AREA_TABLE = 0x800A1274
TOKEN_IDS = 0x800A8668
TOKEN_SLOT_STATE = 0x800A866C
TOKEN_SLOT_STRIDE = 0x10
TOKEN_COLLECTED_FLAG = 0x800A1544
TOKEN_SLOTS = 5
MAX_READ = 64          # the control surface's own per-command cap; it states the cap when it refuses
WORLD_SHIFT = 5        # the guest's own getter shifts the level's 16-bit object coordinates by 5


def s32(value: int) -> int:
    return struct.unpack("<i", struct.pack("<I", value & 0xFFFFFFFF))[0]


def unit(dx: float, dz: float):
    length = math.hypot(dx, dz)
    return None if length < 1e-6 else (dx / length, dz / length)


@dataclass(frozen=True)
class AreaObject:
    index: int
    position: tuple[int, int, int]
    exists: int


class Session:
    """One live game, read and driven over the control channel."""

    def __init__(self, port: int, timeout: float = 60.0):
        # The server serves a connection to completion before accepting the next, so a driver that
        # keeps a session MUST keep exactly one: a second client would never be serviced.
        self.client = LiveClient(port, timeout=timeout)

    def read(self, address: int, count: int) -> list[int]:
        out: list[int] = []
        while count > 0:
            take = min(count, MAX_READ)
            out.extend(self.client.words(address, take))
            address += 4 * take
            count -= take
        return [value & 0xFFFFFFFF for value in out]

    def word(self, address: int) -> int:
        return self.read(address, 1)[0]

    def buzz(self) -> tuple[int, int, int]:
        x, y, z = (s32(value) for value in self.read(PLAYER, 3))
        return x >> WORLD_SHIFT, y >> WORLD_SHIFT, z >> WORLD_SHIFT

    def held(self, buttons) -> None:
        for button in BUTTONS:
            self.client.send(f"{'press' if button in buttons else 'release'} {button}")

    def area_objects(self) -> list[AreaObject]:
        """Every placed object in the current area, with the position the guest's own getter reads."""
        table = s32(self.word(AREA_TABLE))
        count = s32(self.word(table))
        objects = []
        for index, entry in enumerate(self.read(table + 4, max(count, 0))):
            if not entry:
                continue
            record = self.word(entry + 0x10)
            if not (0x80000000 <= record < 0x80200000):
                continue
            position_record = self.word(record + 0x14)
            if not (0x80000000 <= position_record < 0x80200000):
                continue
            objects.append(AreaObject(index, tuple(s32(w) for w in self.read(position_record, 3)),
                                      self.word(record + 0x18)))
        return objects

    def token_ids(self) -> list[int]:
        """The five token ids the guest holds for this level, in its own order."""
        words = self.read(TOKEN_IDS, TOKEN_SLOTS * 2)
        return [words[index * 2] & 0xFFFF for index in range(TOKEN_SLOTS)]

    def token_targets(self) -> list[tuple[int, int, int]]:
        """Where the guest's own token ids point in THIS area: table[id - 1], the guest's own index."""
        objects = self.area_objects()
        by_index = {obj.index: obj for obj in objects}
        return [by_index[token - 1].position for token in self.token_ids() if token - 1 in by_index]

    def collected(self) -> dict[str, int]:
        """The guest's own collection state: the flag it raises and each slot's state word."""
        return {
            "flag": self.word(TOKEN_COLLECTED_FLAG),
            "slots": [self.word(TOKEN_SLOT_STATE + TOKEN_SLOT_STRIDE * index)
                      for index in range(TOKEN_SLOTS)],
        }

    def save_recording(self, path: str, frames: int = 0) -> str:
        return self.client.send(f"padrec save {path} {frames}".strip()).strip()

    def close(self) -> None:
        try:
            self.held(())
        except OSError:
            pass
        self.client.close()


class Walker:
    """Steers Buzz toward a world coordinate with d-pad holds, closing on the position he reports.

    Two things are learned rather than assumed, because the guest makes both true. The d-pad is
    camera-relative and the camera turns as Buzz turns, so every trial re-measures which world
    direction a button produced; and one direction at a time is held, because holding two makes the
    guest turn into the diagonal and the approach never closes.
    """

    def __init__(self, session: Session):
        self.session = session
        self.axis: dict[str, tuple[float, float]] = {}

    def calibrate(self, settle: float = 0.8, hold: float = 0.35) -> None:
        self.session.held(())
        time.sleep(settle)
        for button in BUTTONS:
            self.session.held(())
            time.sleep(settle)
            before = self.session.buzz()
            self.session.held({button})
            time.sleep(hold)
            after = self.session.buzz()
            direction = unit(after[0] - before[0], after[2] - before[2])
            if direction:
                self.axis[button] = direction
            print(f"  {button:>5}: ({before[0]},{before[2]}) -> ({after[0]},{after[2]})"
                  f"  step=({after[0] - before[0]},{after[2] - before[2]})")
        self.session.held(())
        time.sleep(settle)

    def best_button(self, dx: float, dz: float, skip=()) -> str:
        """The d-pad direction whose measured world vector points most nearly at (dx, dz)."""
        ranked = sorted(self.axis.items(), key=lambda item: -(item[1][0] * dx + item[1][1] * dz))
        for button, _ in ranked:
            if button not in skip:
                return button
        return ranked[0][0]

    def walk_to(self, target, tolerance: int = 200, seconds: float = 90.0, settle: float = 0.12,
                hold: float = 0.50, slide: float = 0.60, detours_allowed: int = 12
                ) -> tuple[float | None, str]:
        """Walk one leg toward `target`. Returns the remaining distance, or None with why it stopped.

        One direction at a time, held long enough to be worth measuring: the guest turns into a
        direction before it commits to it, so a trial shorter than the turn reports nothing and the
        walker then slides off in a direction that was never towards the target.
        """
        started = time.time()
        detours = 0
        tried: list[str] = []
        closest = math.inf
        while time.time() - started < seconds:
            self.session.held(())
            time.sleep(settle)
            here = self.session.buzz()
            dx, dz = target[0] - here[0], target[2] - here[2]
            distance = math.hypot(dx, dz)
            closest = min(closest, distance)
            if distance <= tolerance:
                return distance, "reached"
            button = self.best_button(dx, dz, skip=tuple(tried))
            self.session.held({button})
            time.sleep(hold)
            after = self.session.buzz()
            step = (after[0] - here[0], after[2] - here[2])
            direction = unit(*step)
            if direction:
                self.axis[button] = direction
            self.session.held(())
            gained = distance - math.hypot(target[0] - after[0], target[2] - after[2])
            if gained > 0:
                tried = []
                continue
            tried.append(button)
            if len(tried) < len(BUTTONS):
                continue
            # Every direction has been tried from here and none closed on the target: it is
            # furniture or a wall, which is what a room full of furniture does to a straight
            # approach. Sidestep along the obstruction the way a person does, alternating the way
            # round, and come back onto the line.
            tried = []
            sideways = 1 if detours % 2 == 0 else -1
            self.session.held({self.best_button(sideways * dz, -sideways * dx)})
            time.sleep(slide)
            self.session.held(())
            detours += 1
            if detours > detours_allowed:
                return None, (f"gave up after {detours} detours; closest approach {closest:.0f} "
                              f"short of {target}")
        return None, f"out of time, closest approach {closest:.0f} short of {target}"

    def drive(self, targets, tolerance: int = 200, seconds: float = 90.0) -> dict[int, tuple]:
        report = {}
        for number, target in enumerate(targets, start=1):
            distance, outcome = self.walk_to(target, tolerance=tolerance, seconds=seconds)
            report[number] = (target, distance, outcome, self.session.buzz())
            print(f"  target {number} {target}: {outcome}"
                  + (f", {distance:.0f} away" if distance is not None else "")
                  + f", Buzz at {report[number][3]}", flush=True)
        return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, required=True, help="PSXPORT_DEBUG_SERVER port of a live run")
    parser.add_argument("--tolerance", type=int, default=200, help="how close counts as arrived")
    parser.add_argument("--seconds", type=float, default=90.0, help="budget per target")
    parser.add_argument("--save", help="cut the live session into this .pad replay when finished")
    parser.add_argument("--frames", type=int, default=0, help="frames to keep in the saved replay")
    args = parser.parse_args(argv)

    session = Session(args.port)
    try:
        print(f"Buzz at {session.buzz()}")
        print(f"token ids the guest holds: {[hex(t) for t in session.token_ids()]}")
        print(f"guest collection state: {session.collected()}")
        targets = session.token_targets()
        print(f"tokens in this area: {len(targets)} -> {targets}")
        walker = Walker(session)
        walker.calibrate()
        for target in targets:
            print(f"-- walking to {target}", flush=True)
            walker.drive([target], tolerance=args.tolerance, seconds=args.seconds)
            print(f"   guest collection state: {session.collected()}", flush=True)
        if args.save:
            print(session.save_recording(args.save, args.frames))
    finally:
        session.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
