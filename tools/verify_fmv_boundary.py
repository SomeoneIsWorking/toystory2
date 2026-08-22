#!/usr/bin/env python3
"""Verify the FMV ISO-9660 parser's shared-BIOS boundary and live progress.

The retail FMV module calls the executable's Sony-library A(25h) leaf once per
path byte, then stores the returned byte in its normalized token buffer.  The
leaf is shared PSX BIOS behavior (`toupper`), not Toy Story 2 behavior.  This
tool therefore never supplies the return value.  Its live mode runs the
shipping port and proves that it normalizes every byte of the retail boot path
``toy2fmv\\acti.str`` after the old first-call fail-fast boundary.

Usage:
  python3 tools/verify_fmv_boundary.py --check
  python3 tools/verify_fmv_boundary.py --selftest
  python3 tools/verify_fmv_boundary.py --check-log scratch/logs/fmv-boundary.log
  python3 tools/verify_fmv_boundary.py --live --port scratch/bin/toystory2_port
"""

from __future__ import annotations

import argparse
import os
import re
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import overlay_map

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = Path(overlay_map.EXE)
DEFAULT_FMV = Path(overlay_map.FMV)
DEFAULT_PORT = ROOT / "scratch" / "bin" / "toystory2_port"
DEFAULT_LOG = ROOT / "scratch" / "logs" / "fmv-bios-boundary.log"

EXE_BASE = 0x80010000
EXE_PAYLOAD_OFFSET = 0x800
FMV_BASE = (
    overlay_map.GATE_NEXT
)  # RE-03/C010's one authority for the shared physical arena.
PARSER_CALL = 0x800D8E48
PARSER_RETURN = 0x800D8E50
BOOT_PATH = 0x800D5D48  # FMV selector 2 on the current retail boot route.
BIOS_LEAF = 0x80082E5C
BIOS_TABLE = 0xA0
BIOS_FUNCTION = 0x25

CALL_RE = re.compile(
    r"^\[bios\] A0:0x25\(0x(?P<a0>[0-9A-Fa-f]{8}),.*?\) from 0x(?P<ra>[0-9A-Fa-f]{8})$",
    re.MULTILINE,
)


class Refused(Exception):
    """The supplied bytes or trace cannot support the requested conclusion."""


@dataclass(frozen=True)
class BoundaryEvidence:
    parser_call: int
    bios_leaf: int
    bios_table: int
    bios_function: int
    parser_return: int
    parser_inputs: tuple[int, ...]


def word(blob: bytes, offset: int, label: str) -> int:
    if offset < 0 or offset + 4 > len(blob):
        raise Refused(f"{label} offset 0x{offset:X} is outside the supplied bytes")
    return struct.unpack_from("<I", blob, offset)[0]


def require_word(blob: bytes, offset: int, expected: int, label: str) -> None:
    actual = word(blob, offset, label)
    if actual != expected:
        raise Refused(f"{label} changed: got 0x{actual:08X}, expected 0x{expected:08X}")


def jal_target(pc: int, instruction: int) -> int:
    if instruction >> 26 != 0x03:
        raise Refused(
            f"FMV parser instruction at 0x{pc:08X} is not jal: 0x{instruction:08X}"
        )
    return ((pc + 4) & 0xF0000000) | ((instruction & 0x03FFFFFF) << 2)


def analyze(exe: bytes, fmv: bytes) -> BoundaryEvidence:
    if exe[:8] != b"PS-X EXE":
        raise Refused("the executable input is not a PS-X EXE")

    parser_offset = PARSER_CALL - FMV_BASE
    call = word(fmv, parser_offset, "FMV parser BIOS call")
    target = jal_target(PARSER_CALL, call)
    if target != BIOS_LEAF:
        raise Refused(
            f"FMV parser call target changed: 0x{target:08X} != 0x{BIOS_LEAF:08X}"
        )
    # The delay slot advances the input pointer.  The return site stores v0 as
    # one byte, proving that a stale or fabricated return changes the path.
    require_word(fmv, parser_offset + 4, 0x26310001, "FMV parser input advance")
    require_word(fmv, parser_offset + 8, 0xA2020000, "FMV parser normalized-byte store")

    leaf_offset = EXE_PAYLOAD_OFFSET + BIOS_LEAF - EXE_BASE
    require_word(exe, leaf_offset, 0x240A00A0, "Sony-library A0 table select")
    require_word(exe, leaf_offset + 4, 0x01400008, "Sony-library BIOS tail jump")
    require_word(exe, leaf_offset + 8, 0x24090025, "Sony-library function 0x25 select")

    path_offset = BOOT_PATH - FMV_BASE
    path_end = fmv.find(b"\0", path_offset, path_offset + 64)
    if path_end < 0:
        raise Refused("the first FMV path is no longer null-terminated within 64 bytes")
    first_path = fmv[path_offset:path_end]
    if first_path != b"toy2fmv\\acti.str":
        raise Refused(f"the retail boot FMV path changed: {first_path!r}")
    parser_inputs = tuple(first_path.replace(b"\\", b""))

    return BoundaryEvidence(
        parser_call=PARSER_CALL,
        bios_leaf=BIOS_LEAF,
        bios_table=BIOS_TABLE,
        bios_function=BIOS_FUNCTION,
        parser_return=PARSER_RETURN,
        parser_inputs=parser_inputs,
    )


def read_inputs(exe_path: Path, fmv_path: Path) -> tuple[bytes, bytes]:
    for path, label in ((exe_path, "retail executable"), (fmv_path, "FMV module")):
        if not path.is_file():
            raise Refused(
                f"no {label} at {path}; provision the verified retail corpus first"
            )
    return exe_path.read_bytes(), fmv_path.read_bytes()


def check_static(exe_path: Path, fmv_path: Path) -> BoundaryEvidence:
    evidence = analyze(*read_inputs(exe_path, fmv_path))
    print(
        "[fmv-boundary] retail call chain: "
        f"0x{evidence.parser_call:08X} -> 0x{evidence.bios_leaf:08X} -> "
        f"A0:0x{evidence.bios_function:02X}; return 0x{evidence.parser_return:08X} "
        f"stores v0 for {len(evidence.parser_inputs)} normalized path bytes"
    )
    return evidence


def live_progress(log: str, expected: tuple[int, ...]) -> tuple[int, ...]:
    calls = tuple(
        int(match.group("a0"), 16)
        for match in CALL_RE.finditer(log)
        if int(match.group("ra"), 16) == PARSER_RETURN
    )
    if not calls:
        raise Refused(
            "the shipping trace never reached FMV parser BIOS A0:0x25 from 0x800D8E50"
        )
    if calls[0] != expected[0]:
        raise Refused(
            f"the first retail parser byte changed: 0x{calls[0]:02X} != 0x{expected[0]:02X}"
        )
    if len(calls) < len(expected):
        fatal = "unimplemented BIOS A0:0x25" in log
        suffix = " and hit the old shared-runtime fail-fast" if fatal else ""
        raise Refused(
            f"the FMV parser reached {len(calls)} of {len(expected)} toupper calls for its first path"
            f"{suffix}; complete post-boundary parser progress was not observed"
        )
    if calls[: len(expected)] != expected:
        raise Refused(
            "the FMV parser's first-path input sequence changed: "
            + ", ".join(f"0x{value:02X}" for value in calls[: len(expected)])
        )
    print(
        "[fmv-boundary] live parser advanced beyond the old boundary: "
        + ", ".join(f"0x{value:02X}" for value in calls[:8])
        + (" ..." if len(calls) > 8 else "")
        + f" ({len(calls)} A0:0x25 calls from 0x{PARSER_RETURN:08X})"
    )
    return calls


def run_live(
    port: Path, output: Path, expected: tuple[int, ...], timeout_seconds: int
) -> tuple[int, ...]:
    if not port.is_file():
        raise Refused(f"no shipping port binary at {port}; build toystory2_port first")
    env = os.environ.copy()
    env.update(
        {
            "PSXPORT_ASSET_DIR": str(ROOT / "external" / "psxport"),
            "PSXPORT_DEBUG": "bios",
            "PSXPORT_NOAUDIO": "1",
            "PSXPORT_NOWINDOW": "1",
            "PSXPORT_WATCHDOG": "5",
            "PSXPORT_WATCHDOG_BOOT": "30",
        }
    )
    try:
        completed = subprocess.run(
            [str(port)],
            cwd=ROOT,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout_seconds,
            check=False,
        )
        captured = completed.stdout
    except subprocess.TimeoutExpired as error:
        # subprocess kills the exact child it created; no shared process-name kill.
        captured = error.stdout or b""
    text = captured.decode("utf-8", errors="replace")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")
    print(
        "[fmv-boundary] wrote bounded retail trace "
        f"{output.resolve().relative_to(ROOT)}"
    )
    return live_progress(text, expected)


def selftest(exe_path: Path, fmv_path: Path) -> int:
    exe, fmv = read_inputs(exe_path, fmv_path)
    checks: list[tuple[str, bool, str]] = []

    def passes(name: str, operation) -> None:
        try:
            operation()
        except Refused as error:
            checks.append((name, False, str(error)))
        else:
            checks.append((name, True, "accepted"))

    def refuses(name: str, operation) -> None:
        try:
            operation()
        except Refused as error:
            checks.append((name, True, str(error)))
        else:
            checks.append((name, False, "accepted invalid evidence"))

    evidence = analyze(exe, fmv)
    passes("positive: retail parser-to-BIOS chain", lambda: analyze(exe, fmv))

    broken_call = bytearray(fmv)
    struct.pack_into("<I", broken_call, PARSER_CALL - FMV_BASE, 0)
    refuses("negative: parser call is no longer jal", lambda: analyze(exe, broken_call))

    broken_leaf = bytearray(exe)
    struct.pack_into(
        "<I", broken_leaf, EXE_PAYLOAD_OFFSET + BIOS_LEAF - EXE_BASE + 8, 0x24090024
    )
    refuses(
        "negative: SDK leaf selects a different BIOS function",
        lambda: analyze(broken_leaf, fmv),
    )

    first_only = (
        "[bios] A0:0x25(0x00000074, 0x00000000, 0x00000000, 0x00000000) "
        "from 0x800D8E50\n[hle:error] FATAL: unimplemented BIOS A0:0x25\n"
    )
    refuses(
        "negative: old first-call fail-fast does not count as progress",
        lambda: live_progress(first_only, evidence.parser_inputs),
    )

    advanced = "".join(
        f"[bios] A0:0x25(0x{value:08X}, 0x00000000, 0x00000000, 0x00000000) "
        "from 0x800D8E50\n"
        for value in evidence.parser_inputs
    )
    passes(
        "positive: the complete first path proves post-boundary progress",
        lambda: live_progress(advanced, evidence.parser_inputs),
    )

    failures = [name for name, ok, _ in checks if not ok]
    for name, ok, detail in checks:
        print(f"[selftest] {'PASS' if ok else 'FAIL'}  {name} ({detail})")
    print(f"[selftest] {len(checks) - len(failures)}/{len(checks)} passed")
    return 1 if failures else 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    modes = result.add_mutually_exclusive_group(required=True)
    modes.add_argument(
        "--check", action="store_true", help="verify the retail static call chain"
    )
    modes.add_argument(
        "--selftest", action="store_true", help="exercise positive and negative classes"
    )
    modes.add_argument(
        "--check-log", type=Path, help="verify a previously captured shipping trace"
    )
    modes.add_argument(
        "--live", action="store_true", help="run and verify the shipping retail route"
    )
    result.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    result.add_argument("--fmv", type=Path, default=DEFAULT_FMV)
    result.add_argument("--port", type=Path, default=DEFAULT_PORT)
    result.add_argument("--output", type=Path, default=DEFAULT_LOG)
    result.add_argument("--timeout", type=int, default=45)
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        if args.selftest:
            return selftest(args.exe, args.fmv)
        evidence = check_static(args.exe, args.fmv)
        if args.check_log:
            live_progress(
                args.check_log.read_text(encoding="utf-8", errors="replace"),
                evidence.parser_inputs,
            )
        elif args.live:
            if args.timeout <= 0:
                raise Refused("--timeout must be positive")
            run_live(args.port, args.output, evidence.parser_inputs, args.timeout)
        return 0
    except (OSError, Refused) as error:
        print(f"[fmv-boundary] REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
