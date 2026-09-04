#!/usr/bin/env python3
"""Opposite-answer tests for the shipping structure scanner."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from structure import policy, scan_repository


class StructureScannerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        (self.root / "game/core").mkdir(parents=True)
        (self.root / "game/runtime").mkdir(parents=True)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def rules(self) -> set[str]:
        return {violation.rule for violation in scan_repository(self.root).violations}

    def test_clean_fixture_passes_and_reports_denominators(self) -> None:
        (self.root / "game/runtime/engine.cpp").write_text("int engine() { return 1; }\n")
        report = scan_repository(self.root)
        self.assertEqual(report.violations, ())
        self.assertEqual(report.files_scanned, 1)
        self.assertEqual(report.source_files_measured, 1)

    def test_static_product_dependency_fires(self) -> None:
        (self.root / "game/runtime/engine.cpp").write_text('#include "recomp_iface.h"\n')
        self.assertIn("static-product-dependency", self.rules())

    def test_direct_stderr_and_stray_getenv_fire(self) -> None:
        (self.root / "game/runtime/engine.cpp").write_text('fprintf(stderr, "bad"); getenv("BAD");\n')
        rules = self.rules()
        self.assertIn("direct-product-diagnostic", rules)
        self.assertIn("stray-environment-read", rules)

    def test_stderr_sink_is_rejected_but_stdout_sink_is_allowed(self) -> None:
        path = self.root / "game/runtime/engine.cpp"
        path.write_text('fputs("ok", stdout);\n')
        self.assertNotIn("direct-product-diagnostic", self.rules())
        path.write_text('fputs("bad", stderr);\n')
        self.assertIn("direct-product-diagnostic", self.rules())

    def test_config_owner_may_ingest_environment(self) -> None:
        (self.root / "game/core/game_config.cpp").write_text('auto value = std::getenv("SETTING");\n')
        self.assertNotIn("stray-environment-read", self.rules())

    def test_line_limit_reports_exact_file_and_measurement(self) -> None:
        path = self.root / "game/runtime/monolith.cpp"
        path.write_text("\n".join("int x;" for _ in range(policy.SOURCE_LINE_LIMIT + 1)))
        violation = next(item for item in scan_repository(self.root).violations if item.rule == "source-line-limit")
        self.assertEqual(violation.path, Path("game/runtime/monolith.cpp"))
        self.assertIn(str(policy.SOURCE_LINE_LIMIT + 1), violation.detail)


if __name__ == "__main__":
    unittest.main()
