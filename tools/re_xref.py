#!/usr/bin/env python3
"""Run the Toy Story 2 Ghidra xref instrument and honor its real verdict.

Examples:
  python3 tools/re_xref.py --selftest
  python3 tools/re_xref.py scratch/decomp/xref-overlay-names.txt 80022F84..80022FAC
  python3 tools/re_xref.py --project ts2boot_re00 --selftest

Ghidra's headless launcher returns success even when a Python postscript raises ``SystemExit``.
``ghidra_xref.py`` therefore writes a separate status file; this wrapper deletes any stale verdict,
runs the postscript, and refuses unless a new well-formed verdict appears.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROJECT_DIR = ROOT / "scratch" / "ghidra"
LOG_DIR = ROOT / "scratch" / "logs"
STATUS = LOG_DIR / "ghidra-xref.status"


def parser() -> argparse.ArgumentParser:
    out = argparse.ArgumentParser(
        description="Cross-reference guest addresses with Ghidra and an independent instruction fold."
    )
    out.add_argument(
        "--project",
        default=os.environ.get("TS2_GHIDRA_PROJECT", "ts2boot"),
        help="Ghidra project name under scratch/ghidra (default: %(default)s)",
    )
    out.add_argument(
        "--selftest",
        action="store_true",
        help="run positive, negative, refusal, and cross-method controls",
    )
    out.add_argument("output", nargs="?", help="report path for an xref run")
    out.add_argument("targets", nargs="*", help="hex guest addresses or lo..hi ranges")
    return out


def refuse(message: str) -> int:
    print(f"re_xref.py: REFUSED: {message}", file=sys.stderr)
    return 2


def script_arguments(args: argparse.Namespace) -> list[str] | None:
    if args.selftest:
        if args.output or args.targets:
            return None
        return ["--selftest"]
    if not args.output or not args.targets:
        return None
    return [args.output, *args.targets]


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    post_args = script_arguments(args)
    if post_args is None:
        return refuse(
            "choose --selftest alone, or provide <output> and at least one <target>"
        )

    project = PROJECT_DIR / f"{args.project}.gpr"
    if not project.is_file():
        return refuse(
            f"no Ghidra project {project.relative_to(ROOT)}; build the RAM image and import it "
            "with the commands in docs/re-frontier.md RE-00"
        )
    launcher = shutil.which("pyghidraRun")
    if not launcher:
        return refuse(
            "pyghidraRun is not on PATH; Ghidra 12's Python postscript provider is required"
        )

    LOG_DIR.mkdir(parents=True, exist_ok=True)
    STATUS.unlink(missing_ok=True)
    command = [
        launcher,
        "-H",
        str(PROJECT_DIR),
        args.project,
        "-process",
        "-noanalysis",
        "-scriptPath",
        str(ROOT / "tools"),
        "-postScript",
        "ghidra_xref.py",
        *post_args,
        "-scriptlog",
        str(LOG_DIR / "ghidra-xref.log"),
    ]
    completed = subprocess.run(command, cwd=ROOT, check=False)
    if completed.returncode != 0:
        return refuse(
            f"pyghidraRun exited {completed.returncode} before a trustworthy verdict"
        )
    if not STATUS.is_file():
        return refuse(
            "ghidra_xref.py wrote no scratch/logs/ghidra-xref.status; Ghidra can exit 0 even when "
            "the postscript never ran"
        )

    verdict = STATUS.read_text(encoding="utf-8", errors="replace").strip()
    fields = verdict.split(maxsplit=1)
    if not fields or not fields[0].isdigit():
        return refuse(f"malformed postscript verdict {verdict!r}")
    rc = int(fields[0])
    if rc not in (0, 1, 2):
        return refuse(
            f"postscript verdict uses unsupported exit code {rc}: {verdict!r}"
        )
    if rc:
        print(f"re_xref.py: ghidra_xref.py verdict: {verdict}", file=sys.stderr)
    return rc


if __name__ == "__main__":
    sys.exit(main())
