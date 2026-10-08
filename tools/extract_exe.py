#!/usr/bin/env python3
"""Extract this game's boot executable from your disc into scratch/ and check it.

  python3 tools/extract_exe.py [/path/to/disc.chd]

Extracts SLUS_008.93 (SYSTEM.CNF: `BOOT = cdrom:\\SLUS_008.93;1`) to scratch/bin/toystory2/, prints the
PS-EXE header and compares SHA-1 and size with docs/info/exe-identity.txt. There is no decomp of Toy Story 2,
so the expected value is this repo's own measurement: the check detects a different image, not which is right.
The header values are the boot group in game/facts/guest_facts.h.
"""

import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import discdump
from resolve_disc import resolve

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE_ON_DISC = "SLUS_008.93"
OUT_DIR = os.path.join(ROOT, "scratch", "bin", "toystory2")
IDENTITY = os.path.join(ROOT, "docs", "info", "exe-identity.txt")


def expected():
    """(sha1, size, name, source) from docs/info/exe-identity.txt, or (None, None, None, why-not)."""
    if not os.path.isfile(IDENTITY):
        return (
            None,
            None,
            None,
            (
                f"{IDENTITY} is missing — it is a TRACKED file, so a checkout without "
                "it is broken rather than merely unconfigured"
            ),
        )
    with open(IDENTITY, encoding="utf-8", errors="replace") as identity_file:
        for line in identity_file:
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            tok = s.split()
            if len(tok) >= 3 and len(tok[0]) == 40:
                return tok[0].lower(), int(tok[1]), tok[2], IDENTITY
            return (
                None,
                None,
                None,
                f"{IDENTITY}'s first data line is not `<sha1> <size> <name>`: {s!r}",
            )
    return None, None, None, f"{IDENTITY} has no data line"


def psexe_header(data):
    if data[:8] != b"PS-X EXE":
        return None
    f = struct.unpack("<11I", data[0x10 : 0x10 + 44])
    keys = [
        "pc0",
        "gp0",
        "t_addr",
        "t_size",
        "d_addr",
        "d_size",
        "b_addr",
        "b_size",
        "s_addr",
        "s_size",
        "sp_gp",
    ]
    return dict(zip(keys, f))


def main():
    disc = resolve(sys.argv[1] if len(sys.argv) > 1 else None, verbose=True)
    dest = discdump.get(disc, EXE_ON_DISC, OUT_DIR)
    if not dest:
        print(
            f"[exe] {EXE_ON_DISC} was NOT found on {disc} — is this the right disc? "
            "(USA retail: SLUS-00893)",
            file=sys.stderr,
        )
        return 2
    with open(dest, "rb") as executable:
        data = executable.read()
    got = hashlib.sha1(data).hexdigest()
    print(f"[exe] {dest}  {len(data)} bytes  sha1 {got}")

    hdr = psexe_header(data)
    if not hdr:
        print(
            "[exe] NOT a PS-X EXE — the extracted file is not a PSX executable",
            file=sys.stderr,
        )
        return 2
    print(
        "[exe] PS-EXE header: entry pc0=0x{pc0:08X} text=0x{t_addr:08X}+0x{t_size:X} "
        "data=0x{d_addr:08X}+0x{d_size:X} bss=0x{b_addr:08X}+0x{b_size:X} "
        "sp=0x{s_addr:08X} gp0=0x{gp0:08X}".format(**hdr)
    )
    if hdr["d_size"] == 0 and hdr["b_size"] == 0:
        print(
            "[exe] note: the header declares NO data and NO bss segment, so this game clears its own "
            "BSS in crt0. RE-01 measures that range as [0x800A1070,0x800D12C0)."
        )
    if hdr["gp0"] == 0:
        print(
            "[exe] note: gp0 = 0, so the LOADER sets no $gp; RE-01 proves crt0 sets $gp=0x800A0CD8."
        )

    want, wantsz, wantname, why = expected()
    if want is None:
        # Say so rather than pass quietly when there is nothing to check against.
        print(
            f"[exe] CANNOT CHECK the identity: {why}. This run verified the file's SHAPE only "
            "(PS-X EXE + header) and NOT its identity. Restore docs/info/exe-identity.txt from git; "
            "this tool will not substitute a value silently.",
            file=sys.stderr,
        )
        return 2
    if got == want and len(data) == wantsz:
        print(
            f"[exe] MATCH docs/info/exe-identity.txt ({wantname} {wantsz} B sha1 {want}) — this is the "
            "image every number in this repo was measured on. NOTE that the "
            "expectation is OUR OWN measurement, not an independent witness: there is no decomp of "
            "this game to check against."
        )
        return 0
    print(
        f"[exe] MISMATCH: this disc yields sha1 {got} / {len(data)} B; this repo was measured on "
        f"{want} / {wantsz} B (from {os.path.relpath(why, ROOT)}). A different region or revision, or "
        "a bad rip. Every measured fact in docs/ describes the OTHER image — stop and identify your "
        "disc rather than assuming one of the two is wrong.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
