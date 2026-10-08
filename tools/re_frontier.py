#!/usr/bin/env python3
"""Shim over the shared RE-frontier engine in psxport/tools/port/; the data lives in this repo.

Run from the repo root: the engine resolves docs/re-frontier.md against the current working directory.

    python3 tools/re_frontier.py next
"""
import os
import runpy
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PSXPORT = os.environ.get("PSXPORT_DIR") or os.path.join(ROOT, "external", "psxport")
if not os.path.isabs(PSXPORT):
    PSXPORT = os.path.join(ROOT, PSXPORT)
ENGINE = os.path.join(PSXPORT, "tools", "port", "re_frontier.py")

if not os.path.isfile(ENGINE):
    sys.exit(f"{ENGINE} is missing — run `git submodule update --init external/psxport` "
             f"(or set PSXPORT_DIR). This shim has no fallback ON PURPOSE: a local reimplementation "
             f"is exactly the divergence the hoist removed.")

sys.argv[0] = ENGINE
runpy.run_path(ENGINE, run_name="__main__")
