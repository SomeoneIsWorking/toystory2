#!/usr/bin/env python3
"""verify_route.py — Toy Story 2's gameplay control check: title -> Andy's Room, exact and repeatable.

Runs the named route (tools/ts2_route.py) through the headless product with pad edges delivered at exact
pad frames, captures guest RAM and the presented picture at exact frames, and judges from GUEST STATE:

    uv run --frozen python tools/verify_route.py --route                 # reach Andy's Room, once
    uv run --frozen python tools/verify_route.py --determinism           # the same route twice, byte-compared
    uv run --frozen python tools/verify_route.py --negative              # route minus its last tap must FAIL
    uv run --frozen python tools/verify_route.py --selftest

`--route` passes only if Buzz's object exists in the dump taken after the route and every cited
instruction of tools/ts2_guest_words.py matches that dump. `--negative` drops the "PRESS X" confirm and
must report that Buzz does not exist, so the arrival predicate is shown to say no. `--determinism` runs
the route twice and requires identical RAM-dump SHA-256s, identical picture SHA-256s and non-black pixel
counts, and identical tap schedules.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import headless_run  # noqa: E402
import ts2_guest_words as words  # noqa: E402
from ts2_route import ROUTES, Tap  # noqa: E402

ROUTE = "andys-room"
ARRIVAL_FRAME = 900  # Buzz has been controllable since ~pad frame 800
SHOT_FRAMES = (450, 780, 900)
DUMP_FRAMES = (780, 900)
RUN_FRAMES = 1000
NON_BLACK = re.compile(r"present_(\d+)\.png .*non-black (\d+)/(\d+)")


@dataclass(frozen=True)
class Capture:
    """Everything one run is judged by: hashes and counts of what it wrote at exact frames."""

    dumps: dict[int, str]
    pictures: dict[int, str]
    non_black: dict[int, tuple[int, int]]
    taps: tuple[str, ...]

    def mismatches(self, other: "Capture") -> list[str]:
        found = []
        for name in ("dumps", "pictures", "non_black", "taps"):
            if getattr(self, name) != getattr(other, name):
                found.append(f"{name}: {getattr(self, name)} != {getattr(other, name)}")
        return found


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect(result: headless_run.RunResult, plan: headless_run.RunPlan) -> Capture:
    """Hash what the run wrote; a requested capture that is absent is a refusal, not an empty entry."""
    screenshots = result.work / "scratch" / "screenshots"
    dumps = {}
    pictures = {}
    for frame in plan.dump_at:
        dumps[frame] = sha256(dump_path(result, frame))
    for frame in plan.shots:
        pictures[frame] = sha256(screenshots / f"present_{frame}.png")
    non_black = {int(f): (int(n), int(d)) for f, n, d in NON_BLACK.findall(result.log_text)}
    missing = set(plan.shots) - set(non_black)
    if missing:
        raise RuntimeError(f"the log reports no non-black count for presented frame(s) {sorted(missing)}")
    taps = tuple(f"{t.button}:{t.frame}:{t.hold}" for t in plan.taps)
    return Capture(dumps, pictures, non_black, taps)


def execute_route(taps: tuple[Tap, ...], binary: Path, frames: int, shots: tuple[int, ...],
                  dump_at: tuple[int, ...]) -> tuple[headless_run.RunResult, headless_run.RunPlan]:
    """One headless run of `taps` that must exit cleanly with its whole pad schedule consumed."""
    plan = headless_run.RunPlan(binary=binary, frames=frames, shots=shots, dump_at=dump_at, debug="",
                                timeout=300, disc=headless_run.default_disc(), taps=taps)
    result = headless_run.execute(plan, dict(os.environ))
    if result.code != 0:
        raise RuntimeError(f"the product exited {result.code}; log {result.log}")
    if "replay fully consumed" not in result.log_text:
        raise RuntimeError("the pad schedule was not fully consumed, so the run did not reach the route's end")
    return result, plan


def dump_path(result: headless_run.RunResult, frame: int) -> Path:
    return result.work / "scratch" / "bin" / f"padram_{frame}.bin"


def run_once(taps: tuple[Tap, ...], binary: Path) -> tuple[Capture, words.RamDump]:
    result, plan = execute_route(taps, binary, RUN_FRAMES, SHOT_FRAMES, DUMP_FRAMES)
    return collect(result, plan), words.RamDump.read(dump_path(result, ARRIVAL_FRAME))


def judge_arrival(dump: words.RamDump) -> list[str]:
    """Why this dump is not Andy's Room with Buzz present; empty means arrived."""
    problems = [f"instruction at 0x{a:08X} is 0x{found:08X}, expected 0x{want:08X}"
                for a, want, found in words.unmatched_evidence(dump)]
    player = words.read_player(dump)
    if not player.exists:
        problems.append("Buzz's object (0x800B2188) is all zero: gameplay was not entered")
    return problems


def command_route(binary: Path) -> int:
    _, dump = run_once(ROUTES[ROUTE], binary)
    problems = judge_arrival(dump)
    player = words.read_player(dump)
    print(f"[route] pad frame {ARRIVAL_FRAME}: Buzz x={player.x} y={player.y} z={player.z} yaw={player.yaw}")
    for problem in problems:
        print(f"[route] FAIL {problem}")
    print(f"[route] {'PASS' if not problems else 'FAIL'}")
    return 1 if problems else 0


def command_negative(binary: Path) -> int:
    taps = ROUTES[ROUTE][:-1]
    _, dump = run_once(taps, binary)
    problems = judge_arrival(dump)
    player = words.read_player(dump)
    print(f"[negative] route without its last tap: Buzz exists={player.exists}; {len(problems)} problem(s)")
    if not problems or player.exists:
        print("[negative] FAIL: the arrival predicate accepted a route that never left the PRESS X card")
        return 1
    print("[negative] PASS: the predicate rejects it")
    return 0


def command_determinism(binary: Path) -> int:
    first, _ = run_once(ROUTES[ROUTE], binary)
    second, _ = run_once(ROUTES[ROUTE], binary)
    differences = first.mismatches(second)
    for frame in sorted(first.dumps):
        print(f"[determinism] ram@{frame}: {first.dumps[frame][:16]} vs {second.dumps[frame][:16]}")
    for frame in sorted(first.pictures):
        print(f"[determinism] shot@{frame}: {first.pictures[frame][:16]} vs {second.pictures[frame][:16]} "
              f"non-black {first.non_black[frame]} vs {second.non_black[frame]}")
    for difference in differences:
        print(f"[determinism] MISMATCH {difference}")
    print(f"[determinism] {'PASS' if not differences else 'FAIL'}: "
          f"{len(first.dumps)} RAM dumps, {len(first.pictures)} pictures compared")
    return 1 if differences else 0


class VerifyRouteTest(unittest.TestCase):
    def capture(self, **overrides) -> Capture:
        base = dict(dumps={900: "a"}, pictures={900: "b"}, non_black={900: (5, 10)}, taps=("cross:790:4",))
        base.update(overrides)
        return Capture(**base)

    def test_identical_captures_have_no_mismatch(self):
        self.assertEqual(self.capture().mismatches(self.capture()), [])

    def test_each_kind_of_difference_is_reported(self):
        for field, value in (("dumps", {900: "z"}), ("pictures", {900: "z"}), ("non_black", {900: (6, 10)}),
                             ("taps", ("cross:791:4",))):
            self.assertEqual(len(self.capture().mismatches(self.capture(**{field: value}))), 1, field)

    def test_arrival_is_refused_for_an_empty_player_object_and_for_foreign_code(self):
        blank = words.RamDump(bytes(words.RAM_BYTES))
        problems = judge_arrival(blank)
        self.assertTrue(any("all zero" in p for p in problems))
        self.assertEqual(len(problems), len(words.EVIDENCE) + 1)

    def test_the_route_is_the_four_measured_taps_in_order(self):
        taps = ROUTES[ROUTE]
        self.assertEqual([t.button for t in taps], ["start", "cross", "cross", "cross"])
        self.assertEqual([t.frame for t in taps], sorted(t.frame for t in taps))

    def test_every_capture_frame_is_inside_the_run(self):
        self.assertTrue(all(f < RUN_FRAMES for f in SHOT_FRAMES + DUMP_FRAMES))
        self.assertIn(ARRIVAL_FRAME, DUMP_FRAMES)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", type=Path, default=headless_run.ROOT / "build/verify/bin/toystory2_port")
    parser.add_argument("--route", action="store_true")
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--determinism", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(VerifyRouteTest)
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    binary = args.binary.resolve()
    chosen = [name for name in ("route", "negative", "determinism") if getattr(args, name)]
    if len(chosen) != 1:
        parser.error("choose exactly one of --route, --negative, --determinism, --selftest")
    return {"route": command_route, "negative": command_negative, "determinism": command_determinism}[chosen[0]](binary)


if __name__ == "__main__":
    sys.exit(main())
