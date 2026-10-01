#!/usr/bin/env python3
"""headless_run.py — one bounded, headless, silent, unpaced Toy Story 2 run, with its ledger read back.

It launches the built product through psxport's agent environment (offscreen window, dummy audio, no
pacing, a tracked settings file), caps it at N native frames, optionally captures presented frames, and
then reads the run's own log: how it ended, how many presents completed, and the guest-execution
ledger. It kills only the PID it captured, and only on its own timeout; it never touches another
product instance.

    uv run --frozen python tools/headless_run.py --binary build/ts2/bin/toystory2_port --frames 300
    uv run --frozen python tools/headless_run.py --selftest
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
class RunPlan:
    binary: Path
    frames: int
    shots: tuple[int, ...]
    debug: str
    timeout: int
    disc: str
    control_port: int = 0


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


def poll_ledger(process: subprocess.Popen, port: int) -> dict[str, int] | None:
    """Ask the live run for its guest-execution counters until it exits; the last answer is the ledger."""
    sys.path.insert(0, str(FRAMEWORK / "tools"))
    from dbgclient import LiveClient

    last: dict[str, int] | None = None
    while process.poll() is None:
        try:
            last = parse_guest_ledger(LiveClient(port, timeout=5.0).send("guest"))
        except (OSError, ValueError):
            pass
        time.sleep(0.2)
    return last


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
    try:
        if plan.control_port:
            ledger = poll_ledger(process, plan.control_port)
        code = process.wait(timeout=plan.timeout)
    except subprocess.TimeoutExpired:
        os.kill(process.pid, signal.SIGKILL)
        process.wait()
        code = -signal.SIGKILL
        print(f"[run] TIMEOUT after {plan.timeout}s; killed pid {process.pid}", flush=True)
    text = log.read_text(errors="replace") if log.is_file() else ""
    summary = summarize(text)
    print(f"[run] exit={code} log={log} ({len(text.splitlines())} lines)")
    for key, lines in summary.items():
        print(f"[{key}] {len(lines)} line(s)")
        for line in lines[-12:]:
            print(f"  {line}")
    if plan.control_port:
        print(f"[guest-ledger] {ledger if ledger else 'NO ANSWER from the control channel (not measured)'}")
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
    plan = RunPlan(args.binary.resolve(), args.frames, shots, args.debug, args.timeout, disc, args.control_port)
    return run(plan, dict(os.environ))


if __name__ == "__main__":
    raise SystemExit(main())
