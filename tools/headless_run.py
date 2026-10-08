#!/usr/bin/env python3
"""One bounded, headless, silent, unpaced Toy Story 2 run, with its ledger read back.

Launches the built product through psxport's agent environment, caps it at N native frames, optionally
captures frames, and reads the run's log: how it ended, presents completed, the guest-execution ledger.
It kills only the PID it captured, and only on its own timeout.

    uv run --frozen python tools/headless_run.py --binary build/ts2/bin/toystory2_port --frames 300
    uv run --frozen python tools/headless_run.py --frames 900 \\
        --tap 600:start --tap 640:cross:6 --shot-at 590,700,880 --dump-at 880
    uv run --frozen python tools/headless_run.py --aspect 16x9 --shot-at 900 --frames 1000 --tap 600:start ...

`--tap FRAME:BUTTON[:HOLD]` is an exact-frame pad edge, compiled by tools/ts2_route.py into a psxport pad
replay. `--dump-at F,...` writes the 2 MiB guest RAM at those pad frames (PSXPORT_PAD_DUMP_AT); `--shot-at`
captures presented frames (PSXPORT_PRESENT_SHOT_AT). The product runs in `scratch/headless/work`.

With `--control-port` the loop is uncapped and the driver polls the live ledger; `--stop-frame` SIGINTs the
captured PID once the presented-frame counter reaches it. The poll never sends input.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import signal
import subprocess
import time
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from execution_ledger import Ledger, is_ledger_line, parse as parse_ledger, render as render_ledger  # noqa: E402
from resolve_disc import resolve as resolve_disc  # noqa: E402
from ts2_route import Tap, compile_pad, parse_tap  # noqa: E402

FRAMEWORK = ROOT / "external" / "psxport"
SCRATCH = ROOT / "scratch" / "headless"
SETTINGS_4X3 = ROOT / "config" / "aspect_4x3.ini"
SETTINGS_16X9 = ROOT / "config" / "aspect_16x9.ini"
# `fps60` is a product switch separate from the aspect, so it lives in its own tracked settings file.
SETTINGS_4X3_FPS60 = ROOT / "config" / "aspect_4x3_fps60.ini"
SETTINGS_16X9_FPS60 = ROOT / "config" / "aspect_16x9_fps60.ini"
# 4:3 is the product default; --settings names the 16:9 file, so both legs differ only in aspect.
SETTINGS = SETTINGS_4X3
DEFAULT_ICD = "/usr/share/vulkan/icd.d/radeon_icd.x86_64.json"

PRESENT_LINE = re.compile(r"present_shot|\[shot\]|present-shot", re.IGNORECASE)
EXIT_LINE = re.compile(r"frame driver required|native boot returned|REFUSED|abort|fault", re.IGNORECASE)


class StopWatch:
    """The frame at which a control-channel run ends; asked once per poll."""

    def __init__(self, stop_frame: int) -> None:
        self.stop_frame = stop_frame
        self.ended_by_stop = False

    def finished(self, frame: int) -> bool:
        return self.stop_frame > 0 and frame >= self.stop_frame


@dataclass(frozen=True)
class RunPlan:
    binary: Path
    frames: int
    shots: tuple[int, ...]
    dump_at: tuple[int, ...]
    debug: str
    timeout: int
    disc: str
    control_port: int = 0
    taps: tuple[Tap, ...] = ()
    sink: str = "960x720"
    stop_frame: int = 0
    settings: Path = SETTINGS_4X3
    # A recorded phase-keyed `.pad` to replay instead of a tap schedule (offsets from each screen's entry phase).
    pad: Path | None = None

    @property
    def pad_frames(self) -> int:
        """Length of the pad schedule: the run's frames, or past the last tap if that is later."""
        return max([self.frames] + [tap.end for tap in self.taps])


def build_environment(base: dict[str, str], plan: RunPlan, log: Path, pad: Path | None = None) -> dict[str, str]:
    """The explicit agent environment for one run; refuses to guess a disc."""
    if not plan.disc:
        raise ValueError("no disc: set PSXPORT_TS2_DISC (repo .env) or pass --disc")
    sys.path.insert(0, str(FRAMEWORK / "tools" / "port"))
    from launch_environment import agent_environment

    env = agent_environment(base, settings=plan.settings)
    env.update(
        {
            "PSXPORT_PRESENT_SINK": plan.sink,
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
    if plan.dump_at:
        env["PSXPORT_PAD_DUMP_AT"] = ",".join(str(frame) for frame in plan.dump_at)
    if plan.taps:
        if pad is None:
            raise ValueError("taps need a compiled pad schedule file")
        env["PSXPORT_PAD_REPLAY"] = str(pad)
    elif plan.pad:
        env["PSXPORT_PAD_REPLAY"] = str(plan.pad)
    return env


def summarize(log_text: str) -> dict[str, list[str]]:
    """Group the log lines that answer 'how did it end' and 'what is the ledger'."""
    summary: dict[str, list[str]] = {"exit": [], "shots": [], "ledger": []}
    for line in log_text.splitlines():
        if PRESENT_LINE.search(line):
            summary["shots"].append(line)
        elif EXIT_LINE.search(line):
            summary["exit"].append(line)
        elif is_ledger_line(line):
            summary["ledger"].append(line)
    return summary


def poll_ledger(process: subprocess.Popen, port: int, watch: StopWatch) -> tuple[Ledger | None, StopWatch]:
    """Ask the live run for its guest-execution counters until it exits, ending the run at its stop
    frame; the last answer is the ledger. Observation only: no input is ever sent from here."""
    sys.path.insert(0, str(FRAMEWORK / "tools"))
    from dbgclient import LiveClient

    last: Ledger | None = None
    while process.poll() is None:
        try:
            client = LiveClient(port, timeout=5.0)
            last = parse_ledger(client.send("guest"))
            frame = client.frame()
            client.close()
            if watch.finished(frame):
                watch.ended_by_stop = True
                process.send_signal(signal.SIGINT)
                break
        except (OSError, ValueError, RuntimeError) as error:
            print(f"[poll] {type(error).__name__}: {error}", file=sys.stderr, flush=True)
        time.sleep(0.2)
    return last, watch


def prepare_workdir() -> Path:
    """A fresh working directory for the product's cwd-relative outputs, so a run never writes into `scratch/` inputs."""
    work = SCRATCH / "work"
    if work.exists():
        shutil.rmtree(work)
    (work / "scratch" / "bin").mkdir(parents=True)
    (work / "scratch" / "screenshots").mkdir(parents=True)
    return work


@dataclass(frozen=True)
class RunResult:
    code: int
    log: Path
    log_text: str
    work: Path
    ledger: Ledger | None


def execute(plan: RunPlan, base: dict[str, str]) -> RunResult:
    """Launch the product once in a fresh working directory and wait for it; returns what it left."""
    if not plan.binary.is_file():
        raise FileNotFoundError(f"{plan.binary} does not exist; build the product first")
    SCRATCH.mkdir(parents=True, exist_ok=True)
    log = SCRATCH / "run.log"
    log.unlink(missing_ok=True)
    work = prepare_workdir()
    pad = work / "route.pad"
    if plan.pad:
        if plan.taps:
            raise ValueError("--pad and --tap are two ways to drive one replay; pass one")
        pad = plan.pad.resolve()
    elif plan.taps:
        pad.write_bytes(compile_pad(plan.taps, plan.pad_frames))
    env = build_environment(base, plan, log, pad)
    process = subprocess.Popen([str(plan.binary)], cwd=work, env=env, stdout=subprocess.DEVNULL,
                               stderr=subprocess.STDOUT)
    print(f"[run] captured pid {process.pid}; killing only that pid on timeout", flush=True)
    ledger: Ledger | None = None
    watch = StopWatch(plan.stop_frame)
    try:
        if plan.control_port:
            ledger, watch = poll_ledger(process, plan.control_port, watch)
        code = process.wait(timeout=plan.timeout)
    except subprocess.TimeoutExpired:
        os.kill(process.pid, signal.SIGKILL)
        process.wait()
        code = -signal.SIGKILL
        print(f"[run] TIMEOUT after {plan.timeout}s; killed pid {process.pid}", flush=True)
    if watch.ended_by_stop and code in (0, -signal.SIGINT, 128 + signal.SIGINT):
        code = 0  # the run ended at the requested frame, by the interrupt this driver sent
    text = log.read_text(errors="replace") if log.is_file() else ""
    return RunResult(code, log, text, work, ledger)


def run(plan: RunPlan, base: dict[str, str]) -> int:
    try:
        result = execute(plan, base)
    except FileNotFoundError as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        return 2
    summary = summarize(result.log_text)
    print(f"[run] exit={result.code} log={result.log} ({len(result.log_text.splitlines())} lines) work={result.work}")
    for key, lines in summary.items():
        print(f"[{key}] {len(lines)} line(s)")
        for line in lines[-12:]:
            print(f"  {line}")
    try:
        for line in render_ledger(parse_ledger(result.log_text)):
            print(f"[run-end ledger] {line}")
    except ValueError as error:
        print(f"[run-end ledger] NOT MEASURED: {error}")
    if plan.control_port:
        print(f"[guest-ledger] {result.ledger if result.ledger else 'NO ANSWER from the control channel (not measured)'}")
    for tap in plan.taps:
        print(f"[tap] {tap.button} x{tap.hold} scheduled at pad frame {tap.frame}")
    return 0 if result.code == 0 else 1


def default_disc() -> str:
    """The disc, resolved by tools/resolve_disc.py — the one implementation of that question. A run
    with no disc raises SystemExit(2) from there, naming every source it tried."""
    return resolve_disc()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/verify/bin/toystory2_port")
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--shot-at", default="")
    parser.add_argument("--debug", default="")
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--disc", default="")
    parser.add_argument("--control-port", type=int, default=0, help="loopback port for the live ledger query")
    parser.add_argument("--tap", action="append", default=[], help="PADFRAME:BUTTON[:HOLD], repeatable, exact")
    parser.add_argument("--dump-at", default="", help="pad frames at which to write the 2 MiB guest RAM")
    parser.add_argument("--stop-frame", type=int, default=0, help="quit once this presented frame is reached")
    parser.add_argument("--sink", default="960x720",
                        help="the headless presentation sink WxH (PSXPORT_PRESENT_SINK)")
    parser.add_argument("--aspect", choices=("4x3", "16x9"), default="4x3",
                        help="which tracked shipping settings file configures the run")
    parser.add_argument("--pad", type=Path, default=None,
                        help="a recorded phase-keyed .pad to replay instead of --tap schedules")
    parser.add_argument("--fps60", action="store_true",
                        help="gate the interpolated 60 fps configuration (the tracked *_fps60.ini files)")
    args = parser.parse_args()
    disc = args.disc or default_disc()
    shots = tuple(int(s) for s in args.shot_at.split(",") if s)
    taps = tuple(parse_tap(text) for text in args.tap)
    if args.stop_frame and not args.control_port:
        parser.error("--stop-frame needs --control-port")
    dump_at = tuple(int(f) for f in args.dump_at.split(",") if f)
    plan = RunPlan(
        args.binary.resolve(), args.frames, shots, dump_at, args.debug, args.timeout, disc, args.control_port, taps,
        args.sink, args.stop_frame,
        (SETTINGS_16X9_FPS60 if args.fps60 else SETTINGS_16X9) if args.aspect == "16x9"
        else (SETTINGS_4X3_FPS60 if args.fps60 else SETTINGS_4X3),
        args.pad.resolve() if args.pad else None,
    )
    return run(plan, dict(os.environ))


if __name__ == "__main__":
    raise SystemExit(main())
