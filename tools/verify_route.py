#!/usr/bin/env python3
"""verify_route.py — Toy Story 2's gameplay control check: title -> Andy's Room, exact and repeatable.

Runs the named route (tools/ts2_route.py) through the headless product with pad edges delivered at exact
pad frames, captures guest RAM and the presented picture at exact frames, and judges from GUEST STATE:

    uv run --frozen python tools/verify_route.py --route                 # reach Andy's Room, once
    uv run --frozen python tools/verify_route.py --determinism           # the same route twice, byte-compared
    uv run --frozen python tools/verify_route.py --negative              # route minus its last tap must FAIL

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
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import headless_run  # noqa: E402
from execution_ledger import Ledger, parse as parse_ledger, render as render_ledger  # noqa: E402
import ts2_guest_words as words  # noqa: E402
from ts2_route import ROUTES, Tap  # noqa: E402

ROUTE = "andys-room"
ARRIVAL_FRAME = 900  # Buzz has been controllable since ~pad frame 800
SHOT_FRAMES = (450, 780, 900)
DUMP_FRAMES = (780, 900)
RUN_FRAMES = 1000
NON_BLACK = re.compile(r"present_(\d+)\.png .*non-black (\d+)/(\d+)")
# The ordinary end of a run must reach the RAII teardown. Both lines are emitted from DESTRUCTORS —
# `disc_read_report(&disc, "disc hunk cache at shutdown")` from ~Game (runtime/psx/game.cpp) and
# `reportFallbackTelemetry("shutdown")` from ~LightrecExecutor (runtime/cpu/lightrec_executor.cpp) —
# so before the Game had an owner they could not appear at all, and a run that leaked again would
# still pass every other judge here.
TEARDOWN_LINES = (
    re.compile(r"\[disc\] disc hunk cache at shutdown"),
    re.compile(r"Lightrec fallback telemetry \[shutdown\]"),
)

# psxport's phase replay reports that the whole schedule was delivered. Both completion wordings it
# emits are accepted, so a wording change upstream does not read here as a route that never arrived.
REPLAY_COMPLETE = re.compile(r"\[padphase\] replay COMPLETE:")


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


def missing_teardown_lines(log_text: str) -> list[str]:
    """Which destructor-owned end-of-run lines are ABSENT. Empty means the run tore down properly."""
    return [pattern.pattern for pattern in TEARDOWN_LINES if not pattern.search(log_text)]


def execute_route(taps: tuple[Tap, ...], binary: Path, frames: int, shots: tuple[int, ...],
                  dump_at: tuple[int, ...]) -> tuple[headless_run.RunResult, headless_run.RunPlan]:
    """One headless run of `taps` that must exit cleanly with its whole pad schedule consumed."""
    plan = headless_run.RunPlan(binary=binary, frames=frames, shots=shots, dump_at=dump_at, debug="",
                                timeout=300, disc=headless_run.default_disc(), taps=taps)
    result = headless_run.execute(plan, dict(os.environ))
    if result.code != 0:
        raise RuntimeError(f"the product exited {result.code}; log {result.log}")
    if not REPLAY_COMPLETE.search(result.log_text):
        raise RuntimeError("the pad schedule was not fully consumed, so the run did not reach the route's end")
    missing = missing_teardown_lines(result.log_text)
    if missing:
        raise RuntimeError(
            "the run did not tear its owners down at the ordinary end; missing from the log: "
            + ", ".join(missing)
        )
    return result, plan


def dump_path(result: headless_run.RunResult, frame: int) -> Path:
    return result.work / "scratch" / "bin" / f"padram_{frame}.bin"


@dataclass(frozen=True)
class RouteRun:
    capture: Capture
    arrival: words.RamDump
    ledger: Ledger


def run_once(taps: tuple[Tap, ...], binary: Path) -> RouteRun:
    result, plan = execute_route(taps, binary, RUN_FRAMES, SHOT_FRAMES, DUMP_FRAMES)
    return RouteRun(collect(result, plan), words.RamDump.read(dump_path(result, ARRIVAL_FRAME)),
                    parse_ledger(result.log_text))


def judge_ledger(ledger: Ledger) -> list[str]:
    """Why this run's dynarec ledger cannot back a gameplay claim; empty means it executed translated code."""
    problems = []
    if ledger["guest"]["translated_blocks"] == 0 or ledger["guest"]["executed_instructions"] == 0:
        problems.append("the dynarec translated or executed nothing")
    if ledger["guest"]["faults"] != 0:
        problems.append(f"the executor recorded {ledger['guest']['faults']} fault(s)")
    return problems


def judge_arrival(dump: words.RamDump) -> list[str]:
    """Why this dump is not Andy's Room with Buzz present; empty means arrived."""
    problems = [f"instruction at 0x{a:08X} is 0x{found:08X}, expected 0x{want:08X}"
                for a, want, found in words.unmatched_evidence(dump)]
    player = words.read_player(dump)
    if not player.exists:
        problems.append("Buzz's object (0x800B2188) is all zero: gameplay was not entered")
    return problems


def command_route(binary: Path) -> int:
    run = run_once(ROUTES[ROUTE], binary)
    problems = judge_arrival(run.arrival) + judge_ledger(run.ledger)
    player = words.read_player(run.arrival)
    for line in render_ledger(run.ledger):
        print(f"[route] ledger {line}")
    print(f"[route] pad frame {ARRIVAL_FRAME}: Buzz x={player.x} y={player.y} z={player.z} yaw={player.yaw}")
    for problem in problems:
        print(f"[route] FAIL {problem}")
    print(f"[route] {'PASS' if not problems else 'FAIL'}")
    return 1 if problems else 0


def command_negative(binary: Path) -> int:
    taps = ROUTES[ROUTE][:-1]
    run = run_once(taps, binary)
    problems = judge_arrival(run.arrival)
    player = words.read_player(run.arrival)
    print(f"[negative] route without its last tap: Buzz exists={player.exists}; {len(problems)} problem(s)")
    if not problems or player.exists:
        print("[negative] FAIL: the arrival predicate accepted a route that never left the PRESS X card")
        return 1
    print("[negative] PASS: the predicate rejects it")
    return 0


def command_determinism(binary: Path) -> int:
    first = run_once(ROUTES[ROUTE], binary).capture
    second = run_once(ROUTES[ROUTE], binary).capture
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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", type=Path, default=headless_run.ROOT / "build/verify/bin/toystory2_port")
    parser.add_argument("--route", action="store_true")
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--determinism", action="store_true")
    args = parser.parse_args()
    binary = args.binary.resolve()
    chosen = [name for name in ("route", "negative", "determinism") if getattr(args, name)]
    if len(chosen) != 1:
        parser.error("choose exactly one of --route, --negative, --determinism")
    return {"route": command_route, "negative": command_negative, "determinism": command_determinism}[chosen[0]](binary)


if __name__ == "__main__":
    sys.exit(main())
