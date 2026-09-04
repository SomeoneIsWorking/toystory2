#!/usr/bin/env python3
"""Check Toy Story 2 product and source structure."""

from __future__ import annotations

from pathlib import Path

from structure import scan_repository


def main() -> int:
    report = scan_repository(Path(__file__).resolve().parent.parent)
    for violation in report.violations:
        print(f"FAIL {violation.path}: {violation.rule}: {violation.detail}")
    print(
        f"structure: scanned {report.files_scanned} product files and "
        f"measured {report.source_files_measured} source files; {len(report.violations)} violation(s)"
    )
    return 1 if report.violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
