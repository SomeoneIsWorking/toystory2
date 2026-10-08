#!/usr/bin/env python3
"""Locate (and if needed build) the framework `discdump` tool and read a disc.

Every disc read goes through psxport's discdump (external/psxport/tools/discdump.cpp). The framework
checkout is $PSXPORT_DIR (default: the sibling checkout); $PSXPORT_DISCDUMP overrides with a prebuilt
binary. Builds land in this repo's gitignored build/tools/, keyed by checkout. No file list is cached.
"""
import hashlib
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def psxport_dir():
    d = os.environ.get("PSXPORT_DIR") or os.path.join(ROOT, "external", "psxport")
    if not os.path.isabs(d):
        d = os.path.join(ROOT, d)
    return d


def find(build_if_missing=True):
    """Return an absolute path to a usable `discdump`, or raise SystemExit(2) saying why not."""
    override = os.environ.get("PSXPORT_DISCDUMP")
    if override:
        if not os.access(override, os.X_OK):
            print(f"[discdump] $PSXPORT_DISCDUMP={override} is not executable", file=sys.stderr)
            raise SystemExit(2)
        return os.path.abspath(override)

    px = psxport_dir()
    if not os.path.isfile(os.path.join(px, "cmake", "psxport.cmake")):
        print(
            f"[discdump] PSXPORT_DIR={px} is not a psxport checkout — run "
            "`git submodule update --init external/psxport`, or set PSXPORT_DIR.",
            file=sys.stderr,
        )
        raise SystemExit(2)

    # A built binary wins over building; the build dir is keyed by the checkout so a dev PSXPORT_DIR
    # never reuses another checkout's binary, and nothing is written into external/psxport.
    build_dir = os.path.join(ROOT, "build", "tools",
                             "discdump-" + hashlib.sha1(os.path.realpath(px).encode()).hexdigest()[:12])
    searched = []
    for base in (build_dir, os.path.join(px, "build")):
        for name in ("discdump", "discdump.exe"):
            cand = os.path.join(base, "tools", name)
            searched.append(cand)
            if os.access(cand, os.X_OK):
                return cand
    if not build_if_missing:
        print("[discdump] no usable binary. Looked at: $PSXPORT_DISCDUMP (unset), "
              + ", ".join(searched), file=sys.stderr)
        raise SystemExit(2)

    print(f"[discdump] building it from {px} into {build_dir} (first run only)…", file=sys.stderr)
    jobs = str(os.cpu_count() or 4)
    for cmd in (
        ["cmake", "-S", px, "-B", build_dir, "-DCMAKE_BUILD_TYPE=Release"],
        ["cmake", "--build", build_dir, "-j", jobs, "--target", "discdump"],
    ):
        r = subprocess.run(cmd, stdout=subprocess.DEVNULL)
        if r.returncode != 0:
            print(f"[discdump] FAILED: {' '.join(cmd)}\n"
                  f"[discdump] the framework checkout is PSXPORT_DIR={px}"
                  + ("" if os.environ.get("PSXPORT_DIR") else " (unset, defaulted to the submodule)")
                  + ".\n"
                  "[discdump] the usual cause is psxport's OWN submodules being unpopulated — "
                  "initialising external/psxport does NOT reach them, `--recursive` aborts on "
                  "beetle-psx's url-less deps/lightning/gnulib, and sync-submodules.sh certifies "
                  "pins it never checked. Fix it per path:\n"
                  f"    git -C {px} submodule update --init vendor/lucent vendor/beetle-psx\n"
                  "[discdump] or set PSXPORT_DIR at a working checkout, or $PSXPORT_DISCDUMP at a "
                  "prebuilt binary.", file=sys.stderr)
            raise SystemExit(2)
    return find(build_if_missing=False)


def listing(disc, dd=None):
    """Every file on the disc, as a list of (path, lba, size). Raises SystemExit(2) on a bad read."""
    dd = dd or find()
    out = subprocess.run([dd, "list", disc], capture_output=True, text=True)
    if out.returncode != 0:
        print(f"[discdump] list failed on {disc}:\n{out.stdout}{out.stderr}", file=sys.stderr)
        raise SystemExit(2)
    # `discdump list` prints each file with its full path already ("ARC/PL00_U.ARC   LBA 1100   796672 bytes"),
    # so the header directory must not be prepended again.
    files = []
    for line in out.stdout.splitlines():
        s = line.strip()
        if not s or s.startswith("disc:") or s.startswith("root dir") or (s.endswith("/") and " " not in s):
            continue
        parts = s.split()
        if len(parts) >= 5 and parts[1] == "LBA":
            files.append((parts[0], int(parts[2]), int(parts[3])))
    if not files:
        print(f"[discdump] list produced ZERO files for {disc} — refusing to report an empty disc "
              "as a successful read", file=sys.stderr)
        raise SystemExit(2)
    return files


def get(disc, path_on_disc, outdir, dd=None):
    """Extract one file. `path_on_disc` uses forward slashes ('ARC/PL00_U.ARC'), exactly as
    `discdump list` prints it — the backslash form does NOT resolve. Returns the written path."""
    dd = dd or find()
    os.makedirs(outdir, exist_ok=True)
    out = subprocess.run([dd, "get", path_on_disc, disc, outdir], capture_output=True, text=True)
    dest = os.path.join(outdir, os.path.basename(path_on_disc))
    if out.returncode != 0 or not os.path.isfile(dest):
        print(f"[discdump] get {path_on_disc} failed:\n{out.stdout}{out.stderr}", file=sys.stderr)
        return None
    return dest


# CLI. `list` refuses (exit 2) rather than print an empty listing: discdump missing, disc unresolvable
# or zero entries each exit non-zero naming which.
def _main(argv):
    if not argv or argv[0] not in ("list", "get"):
        print("usage: discdump.py list [disc]\n"
              "       discdump.py get <PATH/ON/DISC> <outdir> [disc]\n"
              "\nThe disc is resolved by tools/resolve_disc.py (CLI arg > $PSXPORT_TS2_DISC > .env > a "
              "*.chd in the repo root).", file=sys.stderr)
        return 2
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from resolve_disc import resolve
    if argv[0] == "list":
        disc = resolve(argv[1] if len(argv) > 1 else None, verbose=True)
        files = listing(disc)                     # refuses on zero
        total = 0
        for path, lba, size in files:
            total += size
            print(f"{path:<32} LBA {lba:<8} {size:>12} bytes")
        print(f"\n[discdump] {len(files)} files, {total} bytes total, on {disc}")
        print("[discdump] DENOMINATOR: this is every entry discdump's ISO9660 walk returned. It says "
              "nothing about what is INSIDE any of them.")
        return 0
    if len(argv) < 3:
        print("usage: discdump.py get <PATH/ON/DISC> <outdir> [disc]", file=sys.stderr)
        return 2
    disc = resolve(argv[3] if len(argv) > 3 else None, verbose=True)
    dest = get(disc, argv[1], argv[2])
    if not dest:
        return 2
    print(dest)
    return 0


if __name__ == "__main__":
    sys.exit(_main(sys.argv[1:]))
