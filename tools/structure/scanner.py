"""Filesystem scanner for product-boundary and source-structure invariants."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

from . import policy


@dataclass(frozen=True)
class Violation:
    path: Path
    rule: str
    detail: str


@dataclass(frozen=True)
class ScanReport:
    files_scanned: int
    source_files_measured: int
    violations: tuple[Violation, ...]


def _ignored(path: Path) -> bool:
    return any(part in policy.IGNORED_PARTS for part in path.parts)


def _product_files(root: Path) -> set[Path]:
    files = {path for path in policy.PRODUCT_FILES if (root / path).is_file()}
    for product_root in policy.PRODUCT_ROOTS:
        directory = root / product_root
        if directory.is_dir():
            files.update(path.relative_to(root) for path in directory.rglob("*") if path.is_file())
    return files


def scan_repository(root: Path) -> ScanReport:
    root = root.resolve()
    violations: list[Violation] = []
    for retired in policy.RETIRED_PATHS:
        if (root / retired).exists():
            violations.append(Violation(retired, "retired-static-path", "obsolete static execution artifact exists"))

    product_files = _product_files(root)
    for relative in sorted(product_files):
        text = (root / relative).read_text(encoding="utf-8", errors="replace")
        for marker in policy.STATIC_PRODUCT_MARKERS:
            if marker in text:
                violations.append(Violation(relative, "static-product-dependency", marker))
        if relative.suffix in policy.CPP_SUFFIXES:
            for pattern in policy.CPP_STDERR_PATTERNS:
                if re.search(pattern, text):
                    violations.append(Violation(relative, "direct-product-diagnostic", pattern))
            if re.search(r"\b(?:std::)?getenv\s*\(", text) and relative not in policy.CONFIG_OWNER_FILES:
                violations.append(Violation(relative, "stray-environment-read", "getenv outside configuration owner"))

    measured = 0
    for path in root.rglob("*"):
        relative = path.relative_to(root)
        if not path.is_file() or _ignored(relative) or path.suffix not in policy.SOURCE_SUFFIXES:
            continue
        measured += 1
        line_count = len(path.read_text(encoding="utf-8", errors="replace").splitlines())
        if line_count > policy.SOURCE_LINE_LIMIT:
            violations.append(
                Violation(relative, "source-line-limit", f"{line_count} lines exceeds {policy.SOURCE_LINE_LIMIT}")
            )

    return ScanReport(len(product_files), measured, tuple(violations))
