#!/usr/bin/env python3
"""Classify the retail renderer dispatch target at 0x8001040C.

This is a consumer-side guard for generic emitter recovery. It proves from the
shipping executable that the address is an in-function jump-table trampoline,
not a callable function root or an arbitrary function-pointer destination, and
refuses a game seed that would split the containing function.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from pathlib import Path

import overlay_map

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = Path(overlay_map.EXE)
DEFAULT_SEEDS = ROOT / "game" / "recomp_seeds.json"
DEFAULT_GENERATED = ROOT / "generated"
EXE_BASE = 0x80010000
PAYLOAD = 0x800
FUNCTION = 0x800100E4
TABLE_BASE = 0x800103EC
DISPATCH = 0x800103E4
REENTRY = 0x8001040C
DESTINATION = 0x80014838
SIBLING_REENTRY = 0x800104E4
EXPECTED_SLOTS = [TABLE_BASE + slot * 8 for slot in range(32)]


class Refused(Exception):
    """The supplied evidence does not prove this entry class."""


def word(exe: bytes, address: int) -> int:
    offset = PAYLOAD + address - EXE_BASE
    if offset < PAYLOAD or offset + 4 > len(exe):
        raise Refused(f"0x{address:08X} is outside the executable payload")
    return struct.unpack_from("<I", exe, offset)[0]


def j_target(pc: int, instruction: int) -> int:
    if instruction >> 26 != 0x02:
        raise Refused(f"0x{pc:08X} is not an unconditional j trampoline")
    return ((pc + 4) & 0xF0000000) | ((instruction & 0x03FFFFFF) << 2)


def analyze(exe: bytes) -> None:
    if exe[:8] != b"PS-X EXE":
        raise Refused("input is not a PS-X EXE")

    expected = {
        0x80010354: 0x3C198001,  # lui t9,0x8001
        0x80010358: 0x373903EC,  # ori t9,t9,0x03ec
        0x80010360: 0x3109001F,  # andi t1,t0,31
        0x800103DC: 0x000948C0,  # sll t1,t1,3
        0x800103E0: 0x01394820,  # add t1,t1,t9
        DISPATCH: 0x01200008,  # jr t1, not jalr
        DISPATCH + 4: 0x22100004,
    }
    for address, wanted in expected.items():
        actual = word(exe, address)
        if actual != wanted:
            raise Refused(
                f"dispatch instruction 0x{address:08X} changed: "
                f"0x{actual:08X} != 0x{wanted:08X}"
            )

    # A run of j+nop pairs is data-driven intra-function control flow.  This
    # rejects treating the selected slot as a normal prologue/function root.
    for slot in range(32):
        address = TABLE_BASE + slot * 8
        j_target(address, word(exe, address))
        if word(exe, address + 4) != 0:
            raise Refused(f"jump-table slot 0x{address:08X} lacks its nop delay slot")

    if not (TABLE_BASE <= REENTRY < TABLE_BASE + 32 * 8):
        raise Refused("the observed target is not inside the proven jump table")
    target = j_target(REENTRY, word(exe, REENTRY))
    if target != DESTINATION:
        raise Refused(
            f"re-entry trampoline target changed: 0x{target:08X} != 0x{DESTINATION:08X}"
        )


def load_seeds(path: Path) -> dict[str, object]:
    text = re.sub(r"//.*", "", path.read_text(encoding="utf-8"))
    return json.loads(text)


def generated_table_cases(sources: list[str]) -> list[int]:
    candidates: list[list[int]] = []
    switch_pattern = re.compile(
        r"switch \(c->r\[9\]\) \{(?P<body>.*?)default:", re.DOTALL
    )
    case_pattern = re.compile(r"case 0x([0-9A-Fa-f]{8})u: goto L_\1;")
    for source in sources:
        for match in switch_pattern.finditer(source):
            cases = [int(value, 16) for value in case_pattern.findall(match["body"])]
            if TABLE_BASE in cases:
                candidates.append(cases)
    if len(candidates) != 1:
        raise Refused(
            "generated substrate must contain exactly one resident switch for the table; "
            f"found {len(candidates)}"
        )
    if candidates[0] != EXPECTED_SLOTS:
        got = ", ".join(f"0x{address:08X}" for address in candidates[0])
        raise Refused(f"generated resident switch is not the exact 32-slot table: {got}")
    return candidates[0]


def check_generated(generated_dir: Path) -> None:
    paths = sorted(generated_dir.glob("shard_*.c"))
    if not paths:
        raise Refused(f"generated resident shards are absent: {generated_dir}")
    generated_table_cases(
        [path.read_text(encoding="utf-8", errors="strict") for path in paths]
    )


def check(exe_path: Path, seeds_path: Path, generated_dir: Path) -> None:
    analyze(exe_path.read_bytes())
    check_generated(generated_dir)
    seeds = load_seeds(seeds_path)
    address = f"0x{REENTRY:08X}"
    if address in seeds.get("main_reentry", []) or address in seeds.get("main", []):
        raise Refused(f"{address} must not split the containing renderer function")
    print(
        "[render-reentry] generated resident switch has the exact 32 retail slots, including "
        f"observed 0x{REENTRY:08X} and sibling 0x{SIBLING_REENTRY:08X}; no game seed splits "
        "the function"
    )


def check_fixed_log(log: str) -> None:
    for target in (REENTRY, SIBLING_REENTRY):
        if re.search(rf"\[recomp-MISS \d+\].*0x{target:08X}", log):
            raise Refused(f"trace still misses internal table slot 0x{target:08X}")
    if "unimplemented BIOS A0:0x25" in log:
        raise Refused("trace still contains the retired shared BIOS A0:0x25 boundary")
    if "Fps60::rq_capture OVERFLOW" not in log:
        raise Refused(
            "trace has no post-renderer boundary proving the table path advanced"
        )
    print(
        "[render-reentry] live route passed observed and sibling slots without a recomp miss; "
        "next boundary is RenderQueue capture capacity"
    )


def selftest(exe_path: Path, generated_dir: Path) -> int:
    original = exe_path.read_bytes()
    tests: list[tuple[str, bool]] = []

    try:
        check(exe_path, DEFAULT_SEEDS, generated_dir)
        tests.append(("positive exact generated table without a split seed", True))
    except (OSError, Refused, json.JSONDecodeError):
        tests.append(
            ("positive exact generated table without a split seed", False)
        )

    exact_generated = (
        "switch (c->r[9]) { "
        + " ".join(
            f"case 0x{address:08X}u: goto L_{address:08X};"
            for address in EXPECTED_SLOTS
        )
        + " default: rec_dispatch(c, c->r[9]); return; }"
    )
    generated_controls = (
        ("positive exact synthetic generated table", exact_generated, True),
        (
            "negative generated table missing sibling",
            exact_generated.replace(
                f"case 0x{SIBLING_REENTRY:08X}u: goto L_{SIBLING_REENTRY:08X};", ""
            ),
            False,
        ),
        (
            "negative generated table includes adjacent body",
            exact_generated.replace(
                " default:", " case 0x800104ECu: goto L_800104EC; default:"
            ),
            False,
        ),
    )
    for name, source, accepted in generated_controls:
        try:
            generated_table_cases([source])
            result = True
        except Refused:
            result = False
        tests.append((name, result == accepted))

    def expect(name: str, blob: bytes, accepted: bool) -> None:
        try:
            analyze(blob)
            result = True
        except Refused:
            result = False
        tests.append((name, result == accepted))

    mutated = bytearray(original)
    struct.pack_into("<I", mutated, PAYLOAD + DISPATCH - EXE_BASE, 0x0120F809)
    expect("negative jalr function-pointer dispatch", bytes(mutated), False)
    mutated = bytearray(original)
    struct.pack_into("<I", mutated, PAYLOAD + REENTRY - EXE_BASE, 0x27BDFFE0)
    expect("negative function-prologue target", bytes(mutated), False)
    mutated = bytearray(original)
    struct.pack_into("<I", mutated, PAYLOAD + REENTRY + 4 - EXE_BASE, 0xAFBF001C)
    expect("negative non-table delay slot", bytes(mutated), False)

    exact_log = "[fps60:error] Fps60::rq_capture OVERFLOW: bounded control\n"
    try:
        check_fixed_log(exact_log)
        tests.append(("positive post-table live boundary", True))
    except Refused:
        tests.append(("positive post-table live boundary", False))
    for target, name in (
        (REENTRY, "negative old first-slot miss"),
        (SIBLING_REENTRY, "negative sibling-slot miss"),
    ):
        try:
            check_fixed_log(
                exact_log
                + f"[hle:warn] [recomp-MISS 0] no recompiled fn for 0x{target:08X}\n"
            )
            tests.append((name, False))
        except Refused:
            tests.append((name, True))

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
    parser.add_argument("--seeds", type=Path, default=DEFAULT_SEEDS)
    parser.add_argument("--generated-dir", type=Path, default=DEFAULT_GENERATED)
    args = parser.parse_args()
    try:
        if args.selftest:
            return selftest(args.exe, args.generated_dir)
        check(args.exe, args.seeds, args.generated_dir)
        if args.check_log:
            check_fixed_log(
                args.check_log.read_text(encoding="utf-8", errors="replace")
            )
        return 0
    except (OSError, Refused, json.JSONDecodeError) as error:
        print(f"[render-reentry] REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
