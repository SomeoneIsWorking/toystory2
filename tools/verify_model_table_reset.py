#!/usr/bin/env python3
"""Verify the retail model-package reset and its post-fix live continuation.

The consumer fault at 0xEDA4F893 was downstream of a stale package-table slot.
This gate ties the retail branch-delay instruction, generated reset loop, and a
bounded live writer trace together without substituting a pointer or skipping a
guest access.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

import overlay_map

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = Path(overlay_map.EXE)
DEFAULT_GENERATED = ROOT / "generated"
DEFAULT_LOG = ROOT / "scratch" / "logs" / "re16-reset-fixed.log"
EXE_BASE = 0x80010000
PAYLOAD = 0x800
RESET_FUNCTION = 0x80041F38
RESET_BRANCH = 0x80041FF8
RESET_DELAY_SLOT = 0x80041FFC
TABLE_SLOT_9 = 0x800C728C
LOADER_PC = 0x80041A10
RESET_RETURN = 0x80041F6C
RETIRED_FAULT = "UNMAPPED RAM read16 @ 0xEDA4F893"
MINIMUM_PROVEN_FIELD = 10000

WATCH_RE = re.compile(
    rf"^\[wwatch\] f(\d+).* store \[{TABLE_SLOT_9:08X}\]=([0-9A-Fa-f]{{8}}) "
    r"by pc=([0-9A-Fa-f]{8}) ra=([0-9A-Fa-f]{8})",
    re.MULTILINE,
)


class Refused(Exception):
    """The supplied evidence does not prove the reset correction."""


@dataclass(frozen=True)
class WatchEvent:
    field: int
    value: int
    pc: int
    ra: int


@dataclass(frozen=True)
class RuntimeEvidence:
    first_package: WatchEvent
    clearing_reset: WatchEvent
    replacement_package: WatchEvent
    last_field: int


def word(exe: bytes, address: int) -> int:
    offset = PAYLOAD + address - EXE_BASE
    if offset < PAYLOAD or offset + 4 > len(exe):
        raise Refused(f"0x{address:08X} is outside the executable payload")
    return struct.unpack_from("<I", exe, offset)[0]


def check_retail(exe: bytes) -> None:
    if exe[:8] != b"PS-X EXE":
        raise Refused("input is not a PS-X EXE")
    expected = {
        RESET_BRANCH: 0x1440FFE5,  # bnez v0,0x80041F90
        RESET_DELAY_SLOT: 0x24A50004,  # addiu a1,a1,4
    }
    for address, wanted in expected.items():
        actual = word(exe, address)
        if actual != wanted:
            raise Refused(
                f"reset instruction 0x{address:08X} changed: "
                f"0x{actual:08X} != 0x{wanted:08X}"
            )


def generated_reset_body(sources: list[str]) -> str:
    bodies: list[str] = []
    pattern = re.compile(
        rf"void gen_func_{RESET_FUNCTION:08X}\(Core\* c\) \{{(?P<body>.*?)\n\}}",
        re.DOTALL,
    )
    for source in sources:
        bodies.extend(match["body"] for match in pattern.finditer(source))
    if len(bodies) != 1:
        raise Refused(
            f"generated substrate must contain exactly one reset body; found {len(bodies)}"
        )
    if any(f"gen_func_{RESET_DELAY_SLOT:08X}" in source for source in sources):
        raise Refused(
            f"retail delay slot 0x{RESET_DELAY_SLOT:08X} is still partitioned as a function"
        )
    body = bodies[0]
    loop = re.compile(
        r"c->mem_w32\(\(c->r\[5\] \+ \(uint32_t\)0\), c->r\[0\]\);"
        r".*?\{ int _t = \(c->r\[2\] != c->r\[0\]\); "
        r"c->r\[5\] = c->r\[5\] \+ \(uint32_t\)4;"
        r".*?if \(_t\) goto L_80041F90; \}",
        re.DOTALL,
    )
    if not loop.search(body):
        raise Refused("generated reset loop does not advance r5 in the retail branch delay slot")
    return body


def check_generated(generated_dir: Path) -> None:
    paths = sorted(generated_dir.glob("shard_*.c"))
    if not paths:
        raise Refused(f"generated resident shards are absent: {generated_dir}")
    generated_reset_body(
        [path.read_text(encoding="utf-8", errors="strict") for path in paths]
    )


def parse_events(log: str) -> list[WatchEvent]:
    events: list[WatchEvent] = []
    for match in WATCH_RE.finditer(log):
        field, value, pc, ra = match.groups()
        events.append(
            WatchEvent(int(field), int(value, 16), int(pc, 16), int(ra, 16))
        )
    return events


def classify_runtime(log: str) -> RuntimeEvidence:
    if RETIRED_FAULT in log:
        raise Refused("trace regressed to the retired stale-package pointer fault")
    if "FATAL:" in log:
        raise Refused("trace terminates at a new fatal boundary")
    if "recomp-MISS" in log:
        raise Refused("trace contains a recompiled-dispatch miss")

    events = parse_events(log)
    if not events:
        raise Refused(f"trace has no writer events for model-package slot 0x{TABLE_SLOT_9:08X}")
    first_package = next(
        (event for event in events if event.value and event.pc == LOADER_PC), None
    )
    if first_package is None:
        raise Refused("trace never loads a nonzero package into slot 9 through 0x80041A10")
    clearing_reset = next(
        (
            event
            for event in events
            if event.field > first_package.field
            and event.value == 0
            and event.ra == RESET_RETURN
        ),
        None,
    )
    if clearing_reset is None:
        raise Refused("slot 9 is not cleared by a later execution of the retail reset")
    replacement_package = next(
        (
            event
            for event in events
            if event.field > clearing_reset.field
            and event.value
            and event.value != first_package.value
            and event.pc == LOADER_PC
        ),
        None,
    )
    if replacement_package is None:
        raise Refused("slot 9 is not repopulated with a distinct package after the reset")
    last_field = max(event.field for event in events)
    if last_field < MINIMUM_PROVEN_FIELD:
        raise Refused(
            f"trace stops at field {last_field}, before the measured continuation "
            f"field {MINIMUM_PROVEN_FIELD}"
        )
    return RuntimeEvidence(
        first_package, clearing_reset, replacement_package, last_field
    )


def synthetic_log() -> str:
    return "\n".join(
        (
            "[wwatch] f2617 core=0x1 store [800C728C]=8013B770 "
            "by pc=80041A10 ra=8003CED8 stage=00000000",
            "[wwatch] f4708 core=0x1 store [800C728C]=00000000 "
            "by pc=80082E7C ra=80041F6C stage=00000000",
            "[wwatch] f7164 core=0x1 store [800C728C]=8012ED8C "
            "by pc=80041A10 ra=8003CED8 stage=00000000",
            "[wwatch] f10303 core=0x1 store [800C728C]=00000000 "
            "by pc=80082E7C ra=80041F6C stage=00000000",
        )
    )


def check(exe_path: Path, generated_dir: Path, log_path: Path | None) -> None:
    check_retail(exe_path.read_bytes())
    check_generated(generated_dir)
    if log_path is not None:
        evidence = classify_runtime(
            log_path.read_text(encoding="utf-8", errors="replace")
        )
        print(
            "[model-reset] slot 9 "
            f"0x{evidence.first_package.value:08X} -> reset at f{evidence.clearing_reset.field} "
            f"-> 0x{evidence.replacement_package.value:08X}; "
            f"continued without fatal/miss through f{evidence.last_field}"
        )
    else:
        print(
            "[model-reset] retail branch delay slot and generated 128-entry reset loop agree"
        )


def selftest(exe_path: Path, generated_dir: Path) -> int:
    tests: list[tuple[str, bool]] = []

    try:
        check(exe_path, generated_dir, None)
        tests.append(("positive retail/generated reset", True))
    except (OSError, Refused):
        tests.append(("positive retail/generated reset", False))

    exact = synthetic_log()
    controls = (
        ("positive clear/reload/progress", exact, True),
        (
            "negative missing clear",
            "\n".join(line for line in exact.splitlines() if "=00000000" not in line),
            False,
        ),
        (
            "negative same stale package reloaded",
            exact.replace("8012ED8C", "8013B770"),
            False,
        ),
        (
            "negative retired pointer fault",
            exact + f"\nFATAL: {RETIRED_FAULT}\n",
            False,
        ),
        (
            "negative dispatch miss",
            exact + "\n[recomp-MISS 0] no recompiled fn for 0x80041FFC\n",
            False,
        ),
        (
            "negative insufficient continuation",
            exact.replace("f10303", "f9000"),
            False,
        ),
    )
    for name, log, accepted in controls:
        try:
            classify_runtime(log)
            result = True
        except Refused:
            result = False
        tests.append((name, result == accepted))

    for name, passed in tests:
        print(f"[selftest] {'PASS' if passed else 'FAIL'} {name}")
    print(f"[selftest] {sum(ok for _, ok in tests)}/{len(tests)} passed")
    return 0 if all(ok for _, ok in tests) else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--check", action="store_true")
    modes.add_argument("--selftest", action="store_true")
    modes.add_argument("--check-log", type=Path)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--generated-dir", type=Path, default=DEFAULT_GENERATED)
    args = parser.parse_args()
    try:
        if args.selftest:
            return selftest(args.exe, args.generated_dir)
        check(args.exe, args.generated_dir, args.check_log)
        return 0
    except (OSError, Refused) as error:
        print(f"[model-reset] REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
