"""Declarative structure policy; the scanner contains no project-specific literals."""

from __future__ import annotations

from pathlib import Path

SOURCE_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".py"})
CPP_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"})
SOURCE_LINE_LIMIT = 1200

IGNORED_PARTS = frozenset({".git", ".venv", "build", "external", "scratch", "__pycache__"})

RETIRED_PATHS = (
    Path("generated"),
    Path("game/recomp_seeds.json"),
    Path("game/core/recomp_register.cpp"),
    Path("tests/toystory2_recomp_boundary.cpp"),
    Path("tools/compare_recomp_boundary.py"),
    Path("tools/recomp_substrate.py"),
    Path("tools/verify_frame_fence.py"),
    Path("tools/verify_model_table_reset.py"),
    Path("tools/verify_render_reentry.py"),
    Path("tools/verify_vsync_ownership.py"),
)

PRODUCT_ROOTS = (Path("game"), Path("cmake"))
PRODUCT_FILES = (Path("CMakeLists.txt"), Path("bootstrap.py"), Path("run.sh"), Path("tools/run.py"))

STATIC_PRODUCT_MARKERS = (
    "recomp_iface.h",
    "runtime/recomp/",
    "RecompRegistry",
    "rec_dispatch(",
    "psxport_recomp",
    "psxport_recomp(",
    "gen_func_",
    "TS2_HAVE_SUBSTRATE",
    "generated/rec_sources.cmake",
    "tools/recomp_substrate.py",
    "toystory2_generated",
    "PSXPORT_ENGINE",
)

CPP_STDERR_PATTERNS = (
    r"\bfprintf\s*\(\s*stderr\b",
    r"\bfputs\s*\([^\n]*,\s*stderr\s*\)",
    r"\b(?:std::)?perror\s*\(",
    r"\bstd::cerr\b",
    r"\bcfg_log[a-z]*\s*\(",
)
CONFIG_OWNER_FILES = frozenset({Path("game/core/game_config.cpp")})
