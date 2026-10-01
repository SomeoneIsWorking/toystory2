#!/usr/bin/env python3
"""verify_movement.py — Toy Story 2 gameplay input, judged from Buzz's guest-RAM position words.

From Andy's Room (tools/ts2_route.py) the route holds D-pad up for 40 pad frames and taps Cross once. RAM
is dumped at exact pad frames and Buzz's object (tools/ts2_guest_words.py) is read from each dump:

    uv run --frozen python tools/verify_movement.py --run         # position before/after, judged
    uv run --frozen python tools/verify_movement.py --negative    # the same run with NO gameplay input must FAIL
    uv run --frozen python tools/verify_movement.py --selftest

Judged, each against a denominator printed beside it:
  * idle control: Buzz's position is bit-identical across 60 pad frames with no input;
  * forward: holding Up moves Buzz at least 4096 position units horizontally (observed: 68,644);
  * jump: tapping Cross lifts Buzz's Y by at least 4096 (PSX Y points down, so Y falls) at some sampled
    frame (observed: 16,640) and he lands back on his starting Y within 4096.
The 4096 floor is far above rounding and is not a claim about the world-unit scale.
The guest sees a press one pad frame after the host mask (the pad word is latched by the next poll), which
the table shows in the `pad` column.
"""

from __future__ import annotations

import argparse
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import headless_run  # noqa: E402
import ts2_guest_words as words  # noqa: E402
import verify_route  # noqa: E402
from ts2_route import ROUTES, Tap  # noqa: E402

IDLE_FROM, IDLE_TO = 900, 960
FORWARD_START, FORWARD_HOLD = 960, 40
JUMP_START, JUMP_HOLD = 1060, 10
JUMP_SAMPLES = (1064, 1068, 1072, 1076, 1080, 1090)
FORWARD_END = FORWARD_START + FORWARD_HOLD
EFFECT_FLOOR = 4096  # a floor far above rounding; the observed effects are 16x larger
GAMEPLAY_TAPS = (Tap(FORWARD_START, "up", FORWARD_HOLD), Tap(JUMP_START, "cross", JUMP_HOLD))
DUMP_FRAMES = tuple(sorted({IDLE_FROM, IDLE_TO, FORWARD_END, JUMP_START, *JUMP_SAMPLES}))
RUN_FRAMES = 1100


@dataclass(frozen=True)
class Sample:
    frame: int
    player: words.PlayerState
    pad: int
    mapped: int


def sample(dump: words.RamDump, frame: int) -> Sample:
    return Sample(frame, words.read_player(dump), dump.half(words.PAD_WORD), dump.half(words.MAPPED_INPUT_WORD))


def judge(samples: dict[int, Sample]) -> list[str]:
    """What the samples fail to show; empty means idle held still, Up moved Buzz, and Cross made him jump."""
    problems = []
    idle_a, idle_b = samples[IDLE_FROM].player, samples[IDLE_TO].player
    if not idle_a.exists:
        problems.append("Buzz does not exist at the start of the idle window")
    if (idle_a.x, idle_a.y, idle_a.z) != (idle_b.x, idle_b.y, idle_b.z):
        problems.append(f"idle control moved: {idle_a} -> {idle_b}")
    before, after = samples[FORWARD_START].player, samples[FORWARD_END].player
    travel = after.horizontal_distance_squared(before) ** 0.5
    if travel < EFFECT_FLOOR:
        problems.append(f"holding Up moved Buzz {travel:.0f} fixed-point units, below the {EFFECT_FLOOR} floor")
    ground = samples[JUMP_START].player.y
    lift = ground - min(samples[f].player.y for f in JUMP_SAMPLES)
    if lift < EFFECT_FLOOR:
        problems.append(f"Cross lifted Buzz {lift} fixed-point units, below the {EFFECT_FLOOR} floor")
    landing = abs(samples[JUMP_SAMPLES[-1]].player.y - ground)
    if lift >= EFFECT_FLOOR and landing > EFFECT_FLOOR:
        problems.append(f"Buzz did not land back on his starting Y: off by {landing}")
    return problems


def run(taps: tuple[Tap, ...], binary: Path) -> dict[int, Sample]:
    result, _ = verify_route.execute_route(ROUTES["andys-room"] + taps, binary, RUN_FRAMES, (), DUMP_FRAMES)
    return {f: sample(words.RamDump.read(verify_route.dump_path(result, f)), f) for f in DUMP_FRAMES}


def print_table(samples: dict[int, Sample]) -> None:
    print("[movement] pad frame        x         y         z   yaw    pad  mapped")
    for frame in sorted(samples):
        s = samples[frame]
        print(f"[movement] {frame:9d} {s.player.x:8d} {s.player.y:9d} {s.player.z:9d} {s.player.yaw:5d} "
              f"0x{s.pad:04X}  0x{s.mapped:04X}")


def command_run(binary: Path) -> int:
    samples = run(GAMEPLAY_TAPS, binary)
    print_table(samples)
    forward = samples[FORWARD_END].player.horizontal_distance_squared(samples[FORWARD_START].player) ** 0.5
    print(f"[movement] idle {IDLE_FROM}->{IDLE_TO}: {samples[IDLE_FROM].player} -> {samples[IDLE_TO].player}")
    print(f"[movement] Up held {FORWARD_HOLD} pad frames: horizontal travel {forward:.0f} position units "
          f"(Z {samples[FORWARD_START].player.z} -> {samples[FORWARD_END].player.z})")
    problems = judge(samples)
    for problem in problems:
        print(f"[movement] FAIL {problem}")
    print(f"[movement] {'PASS' if not problems else 'FAIL'}")
    return 1 if problems else 0


def command_negative(binary: Path) -> int:
    samples = run((), binary)
    print_table(samples)
    problems = judge(samples)
    wanted = {"holding Up", "Cross lifted"}
    seen = {w for w in wanted if any(w in p for p in problems)}
    print(f"[negative] no gameplay input: {len(problems)} problem(s): {problems}")
    if seen != wanted:
        print("[negative] FAIL: the judge accepted a run with no Up and no Cross")
        return 1
    print("[negative] PASS: the judge rejects a run with no gameplay input")
    return 0


class VerifyMovementTest(unittest.TestCase):
    def samples(self, forward_z=0, lift=0, land=0, idle_dx=0) -> dict[int, Sample]:
        def at(frame, x=0, y=60000, z=0):
            return Sample(frame, words.PlayerState(x, y, z, 0), 0, 0)

        # The idle window ends where the forward hold starts, so they are one sample (one RAM dump).
        out = {IDLE_FROM: at(IDLE_FROM, 100, 60000, 200), IDLE_TO: at(IDLE_TO, 100 + idle_dx, 60000, 200),
               FORWARD_END: at(FORWARD_END, 100, 60000, 200 + forward_z), JUMP_START: at(JUMP_START, 100, 60000, 200)}
        for index, frame in enumerate(JUMP_SAMPLES):
            out[frame] = at(frame, 100, 60000 - (lift if index == 2 else 0) + (land if index == len(JUMP_SAMPLES) - 1 else 0))
        return out

    def test_a_run_that_moves_jumps_and_lands_is_accepted(self):
        self.assertEqual(judge(self.samples(forward_z=68643, lift=16000)), [])

    def test_a_run_with_no_input_is_rejected_for_both_missing_effects(self):
        problems = judge(self.samples())
        self.assertTrue(any("holding Up" in p for p in problems))
        self.assertTrue(any("Cross lifted" in p for p in problems))

    def test_idle_drift_is_rejected(self):
        self.assertTrue(any("idle control moved" in p for p in judge(self.samples(forward_z=68643, lift=16000, idle_dx=1))))

    def test_a_jump_that_never_lands_is_rejected(self):
        self.assertTrue(any("did not land" in p for p in judge(self.samples(forward_z=68643, lift=16000, land=-9000))))

    def test_travel_below_one_world_unit_is_rejected(self):
        self.assertTrue(any("holding Up" in p for p in judge(self.samples(forward_z=EFFECT_FLOOR - 1, lift=16000))))

    def test_the_inputs_precede_their_samples(self):
        self.assertEqual(IDLE_TO, FORWARD_START)
        self.assertLess(JUMP_START, min(JUMP_SAMPLES))
        self.assertGreaterEqual(JUMP_START, FORWARD_END)
        self.assertTrue(all(f < RUN_FRAMES for f in DUMP_FRAMES))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", type=Path, default=headless_run.ROOT / "build/verify/bin/toystory2_port")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(VerifyMovementTest)
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    if args.run == args.negative:
        parser.error("choose exactly one of --run, --negative, --selftest")
    return (command_run if args.run else command_negative)(args.binary.resolve())


if __name__ == "__main__":
    sys.exit(main())
