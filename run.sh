#!/usr/bin/env bash
# run.sh — the USER's build-and-play entry point for the Toy Story 2 native PC port.
#
#   ./run.sh ["/path/to/Toy Story 2 (USA).chd"]
#
# AGENTS MUST NOT RUN THIS (standing USER directive, external/psxport/CLAUDE.md): it is the windowed
# play launcher, it competes with the user's session for the tree, and its submodule re-sync silently
# reverts in-progress framework work to the recorded pin — after which every measurement describes a
# different framework than the one you think you are running. Build explicitly instead:
#
#   cmake -S . -B build && cmake --build build --target toystory2_seam -j$(nproc)
#
# THERE IS NOTHING TO LAUNCH YET, and this script says so and stops rather than pretending. It does
# every step that IS real today — check the toolchain, announce which framework checkout is in play,
# sync submodules, resolve the disc, extract and identity-check SLUS_008.93, build what builds — and
# then refuses at the recompile step, because emit.py needs this game's seed set, its boot group and the
# load bases of its 21 code overlays (docs/re-frontier.md RE-01/RE-02/RE-03).
set -eu
cd "$(dirname "$0")"

say() { printf '\033[1;36m[run]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[run] error:\033[0m %s\n' "$*" >&2; exit 1; }

# ---- 0. toolchain -------------------------------------------------------------------------------
command -v cmake      >/dev/null || die "cmake not found"
command -v python3    >/dev/null || die "python3 not found"
command -v pkg-config >/dev/null || die "pkg-config not found"
pkg-config --exists sdl3 || die "SDL3 not found (Linux: SDL3-devel/libsdl3-dev; macOS: brew install sdl3)"
CC="${CC:-clang}"
CXX="${CXX:-clang++}"
is_clang() { case "$("$1" --version 2>/dev/null)" in *clang*) return 0;; *) return 1;; esac; }
is_clang "$CC" || die "CC=$CC is not Clang"
is_clang "$CXX" || die "CXX=$CXX is not Clang"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# ---- 0a. WHICH FRAMEWORK CHECKOUT IS THIS RUN BUILT FROM? ---------------------------------------
# Default: the pinned submodule, so `git clone && ./run.sh` works standalone. Override to build against
# the workspace's framework dev clone without touching the submodule. ANNOUNCED either way — a binary
# built from in-progress framework work must never be mistaken for one built from the pin.
# external/psxport is NOT a git submodule any more (2026-08-16): it is a symlink to the workspace's
# shared framework clone when there is one — so a framework edit is live in every port at once, which
# is the point — or a private clone at psxport.pin otherwise. Establish whichever applies before we
# look at it. tools/psxport_sync.py explains the two submodule incidents that motivated the change.
python3 tools/psxport_sync.py --auto || die "could not resolve external/psxport"
PSXPORT_DIR="${PSXPORT_DIR:-external/psxport}"
[ -f "$PSXPORT_DIR/cmake/psxport.cmake" ] || die "PSXPORT_DIR=$PSXPORT_DIR is not a psxport checkout"
if [ "$PSXPORT_DIR" = "external/psxport" ]; then
  say "framework: external/psxport -> $(readlink -f external/psxport 2>/dev/null || echo '?') @ $(git -C external/psxport rev-parse --short HEAD 2>/dev/null || echo '?')$(
        [ -n "$(git -C external/psxport status --porcelain 2>/dev/null)" ] && echo ' +dirty')"
else
  say "framework: *** $PSXPORT_DIR *** (DEV CLONE $(git -C "$PSXPORT_DIR" rev-parse --short HEAD 2>/dev/null || echo '?')$(
        [ -n "$(git -C "$PSXPORT_DIR" status --porcelain 2>/dev/null)" ] && echo ' +dirty')) — NOT the recorded pin"
fi

# ---- 0b. sync git submodules (the framework's nested vendors are the SHARED clone's concern now) --
# ONE implementation, shared by every port: external/psxport/scripts/sync-submodules.sh. It lives
# INSIDE the framework (external/psxport is a symlink to the shared clone), which psxport_sync.py
# --auto above has established — that is the prerequisite for this block.
# KNOWN DEFECT, do not trust its all-clear: it certifies pins it never checked, because
# `git submodule status --recursive` aborts on beetle-psx's URL-less nested deps/lightning/gnulib and
# the script's `|| true` swallows the non-zero exit. See
# external/psxport/docs/workspace/KNOWN-DEFECT-sync-submodules.md.
# The framework's own nested vendors (vendor/lucent, vendor/beetle-psx) are the SHARED clone's —
# initialized once there, never per-port (the old per-port init loop is gone).
if command -v git >/dev/null && [ -f external/psxport/scripts/sync-submodules.sh ]; then
  bash external/psxport/scripts/sync-submodules.sh || die "submodule sync failed"
fi

# ---- 1. resolve the disc + extract the boot executable ------------------------------------------
# ONE implementation of the resolution order (CLI arg > $PSXPORT_TS2_DISC > .env > *.chd drop-in):
# tools/resolve_disc.py, which extract_exe.py also uses. Duplicating it in shell is how two answers to
# "which disc" appear.
PSXPORT_DIR="$PSXPORT_DIR" python3 tools/extract_exe.py ${1:+"$1"} || die "executable provisioning failed"

# ---- 2. build what builds today -----------------------------------------------------------------
say "building the framework library + the seam check…"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPSXPORT_DIR="$(cd "$PSXPORT_DIR" && pwd)" \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" >/dev/null \
  || die "cmake configure failed"
cmake --build build -j "$JOBS" --target toystory2_seam || die "seam check failed"

# ---- 3. STOP. There is no port binary yet ------------------------------------------------------
cat <<'EOF'

[run] ------------------------------------------------------------------------------------------
[run] STOPPING HERE, ON PURPOSE. There is no toystory2_port binary to launch.
[run]
[run] The next step is the static recompilation of SLUS_008.93, and it CANNOT run yet:
[run]   * RE-01  the crt0/boot group is unknown, so there is no guest main() to dispatch
[run]   * RE-02  game/recomp_seeds.json is empty — seeds are grown from real [recomp-MISS] fail-fasts
[run]   * RE-03  this game STREAMS 22 CODE OVERLAYS and their load bases are not confirmed. A base is
[run]            MEASURED (0x800D1000, 15 modules) but how the loader computes it is unknown, so it is
[run]            a fit and not a resident base. emit.py refuses a missing base by design.
[run]
[run] AND THERE IS NO DECOMP OF THIS GAME: no symbol map, no function boundaries. Everything above is
[run] Ghidra from zero (external/psxport/tools/decomp.sh). docs/references.md says how that was
[run] established, and that a search negative is weaker than a measurement.
[run]
[run] python3 tools/re_frontier.py next        -- the step that is actually ready to work
[run] python3 tools/base_fit.py --selftest     -- re-derive the overlay slot from the bytes
[run] python3 tools/raw_probe.py --selftest scratch/flat/LEVEL01__LEVEL.RAW scratch/flat/LEVEL01__LEVEL.DAT
[run] ------------------------------------------------------------------------------------------
EOF
exit 3
