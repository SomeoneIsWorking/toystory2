#!/usr/bin/env python3
"""Verify the complete generated Toy Story 2 guest-VSync caller census.

Every call remains fatal at the linked libetc entry. This census prevents a newly emitted resident or
overlay caller from hiding outside the finite-owner work list, and prevents an existing caller from
disappearing without the corresponding RE/ownership update.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GENERATED = ROOT / "generated"

RESIDENT_CALL = re.compile(r"\bfunc_80088628\(c\);")
OVERLAY_CALL = re.compile(r"\brec_dispatch\(c, 0x80088628u\);")
EXPECTED = {"resident": 44, "memory": 11, "fmv": 1, "other_overlays": 0}


class Refused(Exception):
    """The supplied generated source does not match the measured caller census."""


def classify_sources(sources: dict[str, str]) -> dict[str, int]:
    counts = {name: 0 for name in EXPECTED}
    for name, source in sources.items():
        if re.fullmatch(r"shard_[0-9]+\.c", name):
            counts["resident"] += len(RESIDENT_CALL.findall(source))
            continue
        calls = len(OVERLAY_CALL.findall(source))
        if name.startswith("ov_bits__memory_shard_"):
            counts["memory"] += calls
        elif name.startswith("ov_fmv__fmv_shard_"):
            counts["fmv"] += calls
        elif name.startswith("ov_"):
            counts["other_overlays"] += calls

    if counts != EXPECTED:
        details = ", ".join(
            f"{name}={counts[name]} (expected {EXPECTED[name]})" for name in EXPECTED
        )
        raise Refused(f"guest-VSync caller census changed: {details}")
    return counts


def generated_sources(root: Path) -> dict[str, str]:
    if not root.is_dir():
        raise Refused(f"generated source directory is absent: {root}")
    sources = {
        path.name: path.read_text(encoding="utf-8", errors="strict")
        for path in root.glob("*.c")
    }
    if not sources:
        raise Refused(f"generated source directory contains no C translation units: {root}")
    return sources


def check(root: Path) -> None:
    counts = classify_sources(generated_sources(root))
    print(
        "[vsync-ownership] fatal guest callers: "
        f"resident={counts['resident']}, MEMORY={counts['memory']}, "
        f"FMV={counts['fmv']}, other overlays={counts['other_overlays']}"
    )


def selftest() -> int:
    fixture = {
        "shard_1.c": "func_80088628(c);\n" * EXPECTED["resident"],
        "ov_bits__memory_shard_0.c": (
            "rec_dispatch(c, 0x80088628u);\n" * EXPECTED["memory"]
        ),
        "ov_fmv__fmv_shard_0.c": (
            "rec_dispatch(c, 0x80088628u);\n" * EXPECTED["fmv"]
        ),
        "ov_level01__level_shard_0.c": "",
        # Dispatcher plumbing names the wrapper but is not itself a guest caller.
        "shard_disp.c": "func_80088628(c);\nfunc_80088628(c);\n",
    }
    checks: list[tuple[str, bool]] = []

    def expect(name: str, sources: dict[str, str], accepted: bool) -> None:
        try:
            classify_sources(sources)
            result = True
        except Refused:
            result = False
        checks.append((name, result == accepted))

    expect("positive exact four-way census", fixture, True)
    expect(
        "negative missing resident caller",
        {**fixture, "shard_1.c": fixture["shard_1.c"].replace("func_80088628(c);\n", "", 1)},
        False,
    )
    expect(
        "negative new MEMORY caller",
        {
            **fixture,
            "ov_bits__memory_shard_0.c": fixture["ov_bits__memory_shard_0.c"]
            + "rec_dispatch(c, 0x80088628u);\n",
        },
        False,
    )
    expect(
        "negative caller in unrelated overlay",
        {
            **fixture,
            "ov_level01__level_shard_0.c": "rec_dispatch(c, 0x80088628u);\n",
        },
        False,
    )
    expect(
        "positive dispatcher plumbing ignored",
        {**fixture, "shard_disp.c": fixture["shard_disp.c"] * 4},
        True,
    )

    for name, passed in checks:
        print(f"[selftest] {'PASS' if passed else 'FAIL'} {name}")
    print(f"[selftest] {sum(passed for _, passed in checks)}/{len(checks)} passed")
    return 0 if all(passed for _, passed in checks) else 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--generated", type=Path, default=GENERATED)
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    try:
        check(args.generated)
    except (OSError, Refused) as error:
        print(f"[vsync-ownership] REFUSED: {error}")
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
