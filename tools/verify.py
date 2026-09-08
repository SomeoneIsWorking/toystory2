#!/usr/bin/env python3
"""Verify the Toy Story 2 product through psxport's shared consumer contract."""

from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    framework = Path(os.environ.get("PSXPORT_DIR", ROOT / "external/psxport")).resolve()
    owner = framework / "tools/port/consumer_verify.py"
    if not owner.is_file():
        print(
            f"Toy Story 2 verification requires {owner}; resolve external/psxport "
            "with tools/psxport_sync.py --auto or set PSXPORT_DIR",
            file=sys.stderr,
        )
        return 2

    sys.path.insert(0, str(framework / "tools"))
    from port.consumer_verify import ConsumerVerifyConfig, run_consumer_verification

    build = ROOT / "build/verify"
    return run_consumer_verification(
        ConsumerVerifyConfig(
            name="Toy Story 2",
            root=ROOT,
            build=build,
            psxport=framework,
            product=build / "bin/toystory2_port",
            cmake_module=ROOT / "cmake/toystory2_port.cmake",
            test_regex=".",
            cmake_definitions=("-DBUILD_TESTING=ON",),
        )
    )


if __name__ == "__main__":
    raise SystemExit(main())
