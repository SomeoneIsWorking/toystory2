#!/usr/bin/env python3
"""extract_disc_files.py — extract the disc's non-media files into a FLAT scratch dir, verifying sizes.

  python3 tools/extract_disc_files.py [/path/to/disc.chd]
  python3 tools/extract_disc_files.py --keep-media          # also extract TOY2FMV/ (525 MB)

Why a flat directory: `tools/code_scan.py --census DIR` and `tools/base_fit.py` both work over a
directory of files, and this game's interesting files all share four names (LEVEL.BIN, LEVEL1.BIN,
LEVEL.DAT, LEVEL.RAW) in ten different directories. Flattening to `LEVEL02__LEVEL.BIN` keeps them
distinguishable and keeps every downstream tool free of directory-walking logic.

DENOMINATORS, printed every run: entries in the listing, entries skipped (and why), attempted,
extracted-AND-size-verified, failed. Exits non-zero if anything failed or if ZERO files were extracted
— "nothing to scan" must never look like a clean run, because every census downstream of this would
then report a clean bill of health over an empty directory.

THE SKIP IS A STATED DENOMINATOR GAP, NOT A FILTER. By default TOY2FMV/ (26 files, 525 MB = 95.3% of
the disc: 22 .STR MDEC videos, 3 .XA audio tracks, DUMMY.DAT) is NOT extracted, because scanning half a
gigabyte of MDEC data to conclude it is MDEC data costs an hour. Consequence: any statement made from
this corpus covers 274 of the disc's 300 files, and a code overlay hidden inside an .STR would not be
seen. `--keep-media` closes the gap; the census prints the same caveat.

Everything written here is disc-derived and therefore gitignored (scratch/). Nothing extracted may be
committed.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import discdump  # noqa: E402
from resolve_disc import resolve  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLAT = os.path.join(ROOT, "scratch", "flat")
TREE = os.path.join(ROOT, "scratch", "raw")
SKIP_DIRS = ("TOY2FMV",)


def main(argv):
    keep_media = "--keep-media" in argv
    argv = [a for a in argv if not a.startswith("--")]
    disc = resolve(argv[0] if argv else None, verbose=True)
    entries = discdump.listing(disc)              # refuses (exit 2) on a zero-entry read
    skip = () if keep_media else SKIP_DIRS
    todo = [e for e in entries if e[0].split("/")[0] not in skip]
    skipped = [e for e in entries if e[0].split("/")[0] in skip]

    os.makedirs(FLAT, exist_ok=True)
    ok, failed = 0, []
    for path, _lba, size in todo:
        d = os.path.dirname(path) or "ROOT"
        outdir = os.path.join(TREE, d)
        dest = discdump.get(disc, path, outdir)
        if not dest or os.path.getsize(dest) != size:
            failed.append((path, size, os.path.getsize(dest) if dest and os.path.isfile(dest) else -1))
            continue
        flat = os.path.join(FLAT, f"{d.replace('/', '_')}__{os.path.basename(path)}")
        with open(dest, "rb") as a, open(flat, "wb") as b:
            b.write(a.read())
        ok += 1

    print(f"\n[extract] listing entries: {len(entries)}")
    print(f"[extract] skipped ({'nothing — --keep-media' if keep_media else 'dirs ' + str(SKIP_DIRS)}): "
          f"{len(skipped)} files, {sum(e[2] for e in skipped)} bytes")
    print(f"[extract] attempted: {len(todo)}   extracted AND size-verified: {ok}   FAILED: {len(failed)}")
    for path, want, got in failed:
        print(f"[extract]   FAIL {path}: expected {want} B, got {got}")
    print(f"[extract] flat corpus: {FLAT}")
    if not keep_media:
        print("[extract] DENOMINATOR GAP, stated: TOY2FMV/ was not extracted, so anything measured from "
              "this corpus covers the other files only. A code overlay hidden inside an .STR would not "
              "be seen — implausible, but unmeasured, and unmeasured is not absent. --keep-media closes it.")
    if ok == 0:
        print("[extract] REFUSING: extracted ZERO files. Nothing downstream may report a result over "
              "this directory.", file=sys.stderr)
        return 2
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
