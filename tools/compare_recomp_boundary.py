#!/usr/bin/env python3
"""Compare generated crt0 execution with the independent CPU oracle at its first call."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import verify_crt0

EXE = ROOT / "scratch/bin/toystory2/SLUS_008.93"
RAW = ROOT / "scratch/raw/toystory2-boundary"
REGISTER_NAMES = (
    "at",
    "v0",
    "v1",
    "a0",
    "a1",
    "a2",
    "a3",
    "t0",
    "t1",
    "t2",
    "t3",
    "t4",
    "t5",
    "t6",
    "t7",
    "s0",
    "s1",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "t8",
    "t9",
    "k0",
    "k1",
    "gp",
    "sp",
    "fp",
    "ra",
    "lo",
    "hi",
)
HEADER = re.compile(
    r"^# (?P<tag>[A-Z-]+)-REGS(?: step=\d+)? pc=0x(?P<pc>[0-9A-Fa-f]+)$"
)
REGISTER = re.compile(
    r"^# (?P<tag>[A-Z-]+)-REG (?P<name>[a-z0-9]+)=0x(?P<value>[0-9A-Fa-f]+)$"
)


class Refused(RuntimeError):
    """The requested evidence was unavailable or incomplete."""


@dataclass(frozen=True)
class State:
    pc: int
    registers: dict[str, int]


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            command,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    except OSError as error:
        raise Refused(f"could not execute {command[0]}: {error}") from error


def symbolic_boundary(executable: Path) -> tuple[int, int]:
    verify_crt0.require_target(executable)
    result = verify_crt0.analyze(verify_crt0.load_exe(executable))
    return result["entry"], result["libcInit"]


def parse_state(text: str, tag: str) -> State:
    pc: int | None = None
    registers: dict[str, int] = {}
    for line in text.splitlines():
        header = HEADER.match(line)
        if header and header.group("tag") == tag:
            if pc is not None:
                raise Refused(f"trace contains more than one {tag} state block")
            pc = int(header.group("pc"), 16)
        register = REGISTER.match(line)
        if register and register.group("tag") == tag:
            name = register.group("name")
            if name in registers:
                raise Refused(f"trace repeats {tag} register {name}")
            registers[name] = int(register.group("value"), 16)
    if pc is None:
        raise Refused(f"trace contains no {tag} state block")
    missing = sorted(set(REGISTER_NAMES) - registers.keys())
    extra = sorted(registers.keys() - set(REGISTER_NAMES))
    if missing or extra:
        raise Refused(
            f"{tag} register denominator changed: missing={missing}, extra={extra}"
        )
    return State(pc, registers)


def compare(reference: State, port: State) -> list[str]:
    mismatches: list[str] = []
    if reference.pc != port.pc:
        mismatches.append(f"pc: oracle=0x{reference.pc:08X}, port=0x{port.pc:08X}")
    for name in REGISTER_NAMES:
        if reference.registers[name] != port.registers[name]:
            mismatches.append(
                f"{name}: oracle=0x{reference.registers[name]:08X}, "
                f"port=0x{port.registers[name]:08X}"
            )
    return mismatches


def capture(
    executable: Path,
    oracle: Path,
    runner: Path,
    steps: int,
) -> tuple[State, State]:
    entry, boundary = symbolic_boundary(executable)
    if not oracle.is_file():
        raise Refused(f"independent oracle is absent: {oracle}")
    if not runner.is_file():
        raise Refused(f"generated boundary runner is absent: {runner}")
    RAW.mkdir(parents=True, exist_ok=True)
    oracle_trace = RAW / "oracle-first-call.txt"
    oracle_result = run(
        [
            str(oracle),
            str(executable),
            "--steps",
            str(steps),
            "--capture-first-call",
            "--summary-only",
            "--out",
            str(oracle_trace),
        ]
    )
    if oracle_result.returncode:
        raise Refused(
            f"oracle_trace exited {oracle_result.returncode}:\n{oracle_result.stdout.rstrip()}"
        )
    reference = parse_state(oracle_trace.read_text(encoding="utf-8"), "CALL-BOUNDARY")
    if reference.pc != boundary:
        raise Refused(
            f"oracle first call 0x{reference.pc:08X} disagrees with the symbolic crt0 "
            f"boundary 0x{boundary:08X}"
        )
    port_result = run(
        [str(runner), str(executable), f"0x{entry:08X}", f"0x{boundary:08X}"]
    )
    port_trace = RAW / "port-first-call.txt"
    port_trace.write_text(port_result.stdout, encoding="utf-8")
    if port_result.returncode:
        raise Refused(
            f"generated runner exited {port_result.returncode}:\n{port_result.stdout.rstrip()}"
        )
    port = parse_state(port_result.stdout, "PORT-CALL-BOUNDARY")
    return reference, port


def check(
    executable: Path, oracle: Path, runner: Path, steps: int
) -> tuple[State, State]:
    reference, port = capture(executable, oracle, runner, steps)
    mismatches = compare(reference, port)
    if mismatches:
        raise Refused(
            "generated/oracle first-call divergence:\n" + "\n".join(mismatches)
        )
    total = len(REGISTER_NAMES) + 1
    print(
        f"PASS: {total}/{total} state fields agree at the independent oracle's first call "
        f"0x{reference.pc:08X}"
    )
    print(f"traces: {RAW / 'oracle-first-call.txt'} and {RAW / 'port-first-call.txt'}")
    return reference, port


def selftest(executable: Path, oracle: Path, runner: Path, steps: int) -> bool:
    reference, port = check(executable, oracle, runner, steps)
    altered = dict(port.registers)
    altered["a0"] ^= 1
    mismatches = compare(reference, State(port.pc, altered))
    if len(mismatches) != 1 or not mismatches[0].startswith("a0:"):
        raise Refused(
            "comparator did not detect the forced opposite-answer a0 mutation"
        )
    print("PASS negative: one changed port register produced one named mismatch")
    print("SELFTEST 2/2")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=EXE)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--steps", type=int, default=400_000)
    parser.add_argument("--selftest", action="store_true")
    arguments = parser.parse_args()
    try:
        if arguments.steps <= 0:
            raise Refused("--steps must be positive")
        if arguments.selftest:
            return (
                0
                if selftest(
                    arguments.exe,
                    arguments.oracle,
                    arguments.runner,
                    arguments.steps,
                )
                else 1
            )
        check(arguments.exe, arguments.oracle, arguments.runner, arguments.steps)
        return 0
    except (OSError, Refused, verify_crt0.Refused) as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
