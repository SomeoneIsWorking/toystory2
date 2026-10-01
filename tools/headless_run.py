#!/usr/bin/env python3
"""headless_run.py — one bounded, headless, silent, unpaced Toy Story 2 run, with its ledger read back.

It launches the built product through psxport's agent environment (offscreen window, dummy audio, no
pacing, a tracked settings file), caps it at N native frames, optionally captures presented frames, and
then reads the run's own log: how it ended, how many presents completed, and the guest-execution
ledger. It kills only the PID it captured, and only on its own timeout; it never touches another
product instance.

    uv run --frozen python tools/headless_run.py --binary build/ts2/bin/toystory2_port --frames 300
    uv run --frozen python tools/headless_run.py --control-port 17301 --stop-frame 900 \\
        --tap 600:start --tap 640:cross:6 --shot-at 590,700,880
    uv run --frozen python tools/headless_run.py --selftest

With `--control-port` the framework leaves the frame loop uncapped (the run is driven over the socket),
so `--stop-frame` is what ends it: the driver interrupts the product it launched (SIGINT to the captured
PID, which the product handles by shutting down and reporting) once the presented-frame counter
reaches it. The control channel's own `quit` only closes the client's session.
`--tap FRAME:BUTTON[:FRAMES]` fires a real pad edge (press, hold FRAMES presented frames, release)
through the product's own control channel when the counter first reaches FRAME.
"""

from __future__ import annotations

import argparse
import os
import re
import signal
import subprocess
import time
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FRAMEWORK = ROOT / "external" / "psxport"
SCRATCH = ROOT / "scratch" / "headless"
SETTINGS = ROOT / "config" / "aspect_4x3.ini"
DEFAULT_ICD = "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json"

PRESENT_LINE = re.compile(r"present_shot|\[shot\]|present-shot", re.IGNORECASE)
EXIT_LINE = re.compile(r"frame driver required|native boot returned|REFUSED|abort|fault", re.IGNORECASE)
LEDGER_LINE = re.compile(r"lightrec|translated|fallback", re.IGNORECASE)


@dataclass(frozen=True)
class Tap:
    frame: int
    button: str
    frames: int = 4


def parse_tap(text: str) -> Tap:
    """`FRAME:BUTTON[:FRAMES]`; anything else is a refusal, never a silently dropped input."""
    parts = text.split(":")
    if len(parts) not in (2, 3) or not parts[0].isdigit() or not parts[1].isalpha():
        raise ValueError(f"tap {text!r} is not FRAME:BUTTON[:FRAMES]")
    frames = int(parts[2]) if len(parts) == 3 else 4
    if frames < 1:
        raise ValueError(f"tap {text!r} must hold at least one frame")
    return Tap(int(parts[0]), parts[1], frames)


class Scenario:
    """The taps still owed and the frame at which the run ends; asked once per poll."""

    def __init__(self, taps: tuple[Tap, ...], stop_frame: int) -> None:
        self.pending = sorted(taps, key=lambda tap: tap.frame)
        self.stop_frame = stop_frame
        self.fired: list[tuple[int, Tap]] = []
        self.ended_by_stop = False

    def due(self, frame: int) -> list[Tap]:
        ready = [tap for tap in self.pending if tap.frame <= frame]
        self.pending = [tap for tap in self.pending if tap.frame > frame]
        self.fired.extend((frame, tap) for tap in ready)
        return ready

    def finished(self, frame: int) -> bool:
        return self.stop_frame > 0 and frame >= self.stop_frame


@dataclass(frozen=True)
class RunPlan:
    binary: Path
    frames: int
    shots: tuple[int, ...]
    debug: str
    timeout: int
    disc: str
    control_port: int = 0
    taps: tuple[Tap, ...] = ()
    stop_frame: int = 0


def build_environment(base: dict[str, str], plan: RunPlan, log: Path) -> dict[str, str]:
    """The explicit agent environment for one run; refuses to guess a disc."""
    if not plan.disc:
        raise ValueError("no disc: set PSXPORT_TS2_DISC (repo .env) or pass --disc")
    sys.path.insert(0, str(FRAMEWORK / "tools" / "port"))
    from launch_environment import agent_environment

    env = agent_environment(base, settings=SETTINGS)
    env.update(
        {
            "PSXPORT_PRESENT_SINK": "960x720",
            "PSXPORT_LOG_FILE": str(log),
            "PSXPORT_DISC": plan.disc,
            "PSXPORT_DEBUG": plan.debug,
            "PSXPORT_WATCHDOG": str(plan.timeout),
            "PSXPORT_NATIVE_FRAMES": str(plan.frames),
            "SDL_VIDEODRIVER": "offscreen",
            "SDL_AUDIODRIVER": "dummy",
            "VK_ICD_FILENAMES": base.get("VK_ICD_FILENAMES", DEFAULT_ICD),
        }
    )
    if plan.control_port:
        env["PSXPORT_DEBUG_SERVER"] = str(plan.control_port)
    if plan.shots:
        env["PSXPORT_PRESENT_SHOT_AT"] = ",".join(str(shot) for shot in plan.shots)
    return env


def summarize(log_text: str) -> dict[str, list[str]]:
    """Group the log lines that answer 'how did it end' and 'what is the ledger'."""
    summary: dict[str, list[str]] = {"exit": [], "shots": [], "ledger": []}
    for line in log_text.splitlines():
        if PRESENT_LINE.search(line):
            summary["shots"].append(line)
        elif EXIT_LINE.search(line):
            summary["exit"].append(line)
        elif LEDGER_LINE.search(line):
            summary["ledger"].append(line)
    return summary


GUEST_FIELD = re.compile(r"(\w+)=(\d+)")


def parse_guest_ledger(reply: str) -> dict[str, int]:
    """The `guest:` line of the control channel as counters; an absent line is a refusal, not zeros."""
    for line in reply.splitlines():
        if line.startswith("guest:"):
            return {name: int(value) for name, value in GUEST_FIELD.findall(line)}
    raise ValueError("control channel reply carries no 'guest:' line")


def poll_ledger(process: subprocess.Popen, port: int, scenario: Scenario) -> tuple[dict[str, int] | None, Scenario]:
    """Ask the live run for its guest-execution counters until it exits, firing the scenario's taps and
    ending the run at its stop frame; the last answer is the ledger."""
    sys.path.insert(0, str(FRAMEWORK / "tools"))
    from dbgclient import LiveClient

    last: dict[str, int] | None = None
    while process.poll() is None:
        try:
            client = LiveClient(port, timeout=5.0)
            last = parse_guest_ledger(client.send("guest"))
            frame = client.frame()
            for tap in scenario.due(frame):
                client.tap(tap.button, tap.frames)
            client.close()
            if scenario.finished(frame):
                scenario.ended_by_stop = True
                process.send_signal(signal.SIGINT)
                break
        except (OSError, ValueError, RuntimeError) as error:
            print(f"[poll] {type(error).__name__}: {error}", file=sys.stderr, flush=True)
        time.sleep(0.2)
    return last, scenario


def run(plan: RunPlan, base: dict[str, str]) -> int:
    if not plan.binary.is_file():
        print(f"REFUSED: {plan.binary} does not exist; build the product first", file=sys.stderr)
        return 2
    SCRATCH.mkdir(parents=True, exist_ok=True)
    log = SCRATCH / "run.log"
    log.unlink(missing_ok=True)
    env = build_environment(base, plan, log)
    process = subprocess.Popen([str(plan.binary)], cwd=ROOT, env=env, stdout=subprocess.DEVNULL,
                               stderr=subprocess.STDOUT)
    print(f"[run] captured pid {process.pid}; killing only that pid on timeout", flush=True)
    ledger: dict[str, int] | None = None
    scenario = Scenario(plan.taps, plan.stop_frame)
    try:
        if plan.control_port:
            ledger, scenario = poll_ledger(process, plan.control_port, scenario)
        code = process.wait(timeout=plan.timeout)
    except subprocess.TimeoutExpired:
        os.kill(process.pid, signal.SIGKILL)
        process.wait()
        code = -signal.SIGKILL
        print(f"[run] TIMEOUT after {plan.timeout}s; killed pid {process.pid}", flush=True)
    text = log.read_text(errors="replace") if log.is_file() else ""
    summary = summarize(text)
    if scenario.ended_by_stop and code in (0, -signal.SIGINT, 128 + signal.SIGINT):
        code = 0  # the run ended at the requested frame, by the interrupt this driver sent
    print(f"[run] exit={code} log={log} ({len(text.splitlines())} lines)")
    for key, lines in summary.items():
        print(f"[{key}] {len(lines)} line(s)")
        for line in lines[-12:]:
            print(f"  {line}")
    if plan.control_port:
        print(f"[guest-ledger] {ledger if ledger else 'NO ANSWER from the control channel (not measured)'}")
        for frame, tap in scenario.fired:
            print(f"[tap] {tap.button} x{tap.frames} fired at presented frame {frame} (asked for {tap.frame})")
        for tap in scenario.pending:
            print(f"[tap] NOT FIRED: {tap.button} at frame {tap.frame} (the run ended first)")
    return 0 if code == 0 else 1


class HeadlessRunTest(unittest.TestCase):
    def plan(self, **kw) -> RunPlan:
        base = dict(binary=Path("x"), frames=5, shots=(3, 4), debug="cd", timeout=9, disc="/d.chd")
        base.update(kw)
        return RunPlan(**base)

    def test_environment_is_headless_and_carries_the_plan(self):
        env = build_environment({}, self.plan(), Path("/l"))
        self.assertEqual(env["PSXPORT_NATIVE_FRAMES"], "5")
        self.assertEqual(env["PSXPORT_PRESENT_SHOT_AT"], "3,4")
        self.assertEqual(env["PSXPORT_NOAUDIO"], "1")
        self.assertEqual(env["SDL_VIDEODRIVER"], "offscreen")

    def test_no_disc_is_refused(self):
        with self.assertRaises(ValueError):
            build_environment({}, self.plan(disc=""), Path("/l"))

    def test_no_shots_sets_no_capture_variable(self):
        env = build_environment({}, self.plan(shots=()), Path("/l"))
        self.assertNotIn("PSXPORT_PRESENT_SHOT_AT", env)

    def test_guest_ledger_parses_counters_and_refuses_a_reply_without_them(self):
        got = parse_guest_ledger("guest: calls=3 translated_blocks=12 faults=0\n---\n")
        self.assertEqual(got["translated_blocks"], 12)
        self.assertEqual(got["faults"], 0)
        with self.assertRaises(ValueError):
            parse_guest_ledger("guest: no core in this frame is not a ledger\nother: 1\n".replace("guest:", "x:"))

    def test_taps_parse_and_malformed_ones_are_refused(self):
        self.assertEqual(parse_tap("600:start"), Tap(600, "start", 4))
        self.assertEqual(parse_tap("640:cross:6"), Tap(640, "cross", 6))
        for bad in ("start", "600", "x:start", "600:start:0", "600:start:1:2", "600:1"):
            with self.assertRaises(ValueError):
                parse_tap(bad)

    def test_scenario_fires_each_tap_once_in_order_and_stops_at_its_frame(self):
        scenario = Scenario((Tap(20, "cross"), Tap(10, "start")), 30)
        self.assertEqual(scenario.due(5), [])
        self.assertEqual([tap.button for tap in scenario.due(12)], ["start"])
        self.assertEqual([tap.button for tap in scenario.due(25)], ["cross"])
        self.assertEqual(scenario.due(40), [])
        self.assertFalse(scenario.finished(29))
        self.assertTrue(scenario.finished(30))
        self.assertFalse(Scenario((), 0).finished(10**6))

    def test_summary_separates_exit_from_noise_and_reports_zero_honestly(self):
        got = summarize("[boot] hello\nframe driver required a completed guest call\n")
        self.assertEqual(len(got["exit"]), 1)
        self.assertEqual(got["shots"], [])
        self.assertEqual(got["ledger"], [])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/ts2/bin/toystory2_port")
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--shot-at", default="")
    parser.add_argument("--debug", default="")
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--disc", default="")
    parser.add_argument("--control-port", type=int, default=0, help="loopback port for the live ledger query")
    parser.add_argument("--tap", action="append", default=[], help="FRAME:BUTTON[:FRAMES], repeatable")
    parser.add_argument("--stop-frame", type=int, default=0, help="quit once this presented frame is reached")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(HeadlessRunTest)
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    disc = args.disc or os.environ.get("PSXPORT_TS2_DISC", "")
    if not disc and (ROOT / ".env").is_file():
        for line in (ROOT / ".env").read_text().splitlines():
            if line.startswith("PSXPORT_TS2_DISC="):
                disc = line.split("=", 1)[1].strip().strip('"')
    shots = tuple(int(s) for s in args.shot_at.split(",") if s)
    taps = tuple(parse_tap(text) for text in args.tap)
    if (taps or args.stop_frame) and not args.control_port:
        parser.error("--tap and --stop-frame need --control-port")
    plan = RunPlan(
        args.binary.resolve(), args.frames, shots, args.debug, args.timeout, disc, args.control_port, taps, args.stop_frame
    )
    return run(plan, dict(os.environ))


if __name__ == "__main__":
    raise SystemExit(main())
