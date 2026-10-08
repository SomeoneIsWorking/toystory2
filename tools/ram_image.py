#!/usr/bin/env python3
"""Build the 2 MiB KSEG0 image Ghidra is imported over, driven by the PS-EXE header.

  python3 tools/ram_image.py                                  # boot exe only -> scratch/ghidra/ram-boot.bin
  python3 tools/ram_image.py -o scratch/ghidra/ram-l1.bin \\
      --overlay scratch/flat/LEVEL01__LEVEL.BIN@0x800D12C0    # + one overlay at RE-03's proven base

Ghidra imports the image as a flat binary at 0x80000000 so Ghidra addresses equal guest addresses. The
PS-EXE bytes start 0x800 into the file and land at `t_addr`; importing it directly would put every
instruction 0x8000F800 below its real address, invisibly. This tool does that placement once.

Refuses (exit 2, never a partial image): a file that is not `PS-X EXE`; `0x800 + t_size != filesize`; a
placement leaving [0x80000000, 0x80200000); overlapping placements; an `--overlay` base without `0x` hex or
a missing input. Exe identity is `tools/extract_exe.py`'s job. Every run prints each placement (file offset,
guest range, bytes), the total placed and the zero fraction: zero means the image says nothing there.

Blind spots, printed every run:
  * BSS is not materialised (b_size = 0; the game clears its own BSS, RE-01), so loads from it give an
    address, never a value.
  * No overlay is present unless injected; C010 proves the LEVEL slot at 0x800D12C0.
  * No relocation or patching: crt0 has not run ($gp unset) and fixup targets hold their on-disc values.
  * Hardware registers (0x1F80xxxx) and the BIOS ROM are absent.
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.environ.get("TS2_EXE") or os.path.join(ROOT, "scratch", "bin", "toystory2", "SLUS_008.93")
OUT_DEFAULT = os.path.join(ROOT, "scratch", "ghidra", "ram-boot.bin")
RAM_BASE, RAM_SIZE = 0x80000000, 0x00200000
HDR_BYTES = 0x800

BLIND_SPOTS = [
    "BSS IS NOT MATERIALISED (header b_size=0; the game clears its own BSS — RE-01). Everything above "
    "the loaded image reads 0: that is 'unknown', not 'zero at boot'",
    "NO OVERLAY unless --overlay injects one. The tool accepts any explicit base, so the image is "
    "evidence only when that base is independently proven (C010 proves LEVEL at 0x800D12C0)",
    "NO RELOCATION, NO PATCHING: crt0 has not run, $gp is unset, and anything a loader/fixup table "
    "would rewrite still holds its on-disc value",
    "RAM ONLY — no 0x1F80xxxx hardware registers and no BIOS ROM, so a call into the BIOS lands in "
    "nothing at all",
]

HDR_KEYS = ["pc0", "gp0", "t_addr", "t_size", "d_addr", "d_size",
            "b_addr", "b_size", "s_addr", "s_size", "sp_gp"]


class Refuse(Exception):
    """A condition under which producing an image would produce a LIE. Exit 2, no output written."""


def psexe_header(data):
    if len(data) < 0x800 or data[:8] != b"PS-X EXE":
        raise Refuse(f"not a PS-X EXE: magic is {data[:8]!r} (need b'PS-X EXE')")
    return dict(zip(HDR_KEYS, struct.unpack("<11I", data[0x10:0x10 + 44])))


def _place(placements, name, src, guest, blob):
    lo, hi = guest, guest + len(blob)
    if not blob:
        raise Refuse(f"{name}: zero bytes to place — an empty input cannot be reported as placed")
    if lo < RAM_BASE or hi > RAM_BASE + RAM_SIZE:
        raise Refuse(f"{name}: [0x{lo:08X},0x{hi:08X}) leaves RAM "
                     f"[0x{RAM_BASE:08X},0x{RAM_BASE + RAM_SIZE:08X})")
    for p in placements:
        if lo < p["hi"] and p["lo"] < hi:
            raise Refuse(f"{name}: [0x{lo:08X},0x{hi:08X}) OVERLAPS {p['name']} "
                         f"[0x{p['lo']:08X},0x{p['hi']:08X}) — one would silently clobber the other")
    placements.append({"name": name, "src": src, "lo": lo, "hi": hi, "blob": blob})


def build(exe_path, overlays):
    """-> (bytes image, header dict, placements). Raises Refuse rather than returning a partial image."""
    if not os.path.isfile(exe_path):
        raise Refuse(f"{exe_path} is absent — run `python3 tools/extract_exe.py` (it needs YOUR disc; "
                     "resolution order is in CLAUDE.md). Nothing extracted is ever committed.")
    data = open(exe_path, "rb").read()
    hdr = psexe_header(data)
    want = HDR_BYTES + hdr["t_size"]
    if want != len(data):
        raise Refuse(f"{exe_path}: header says 0x800 + t_size(0x{hdr['t_size']:X}) = {want} bytes but the "
                     f"file is {len(data)} — truncated, padded, or not the boot exe")
    placements = []
    _place(placements, "boot .text", f"{os.path.basename(exe_path)}+0x800",
           hdr["t_addr"], data[HDR_BYTES:HDR_BYTES + hdr["t_size"]])
    for path, base in overlays:
        if not os.path.isfile(path):
            raise Refuse(f"--overlay {path}: no such file (read it from YOUR disc)")
        _place(placements, "overlay " + os.path.basename(path), os.path.basename(path) + "+0",
               base, open(path, "rb").read())
    img = bytearray(RAM_SIZE)
    for p in placements:
        img[p["lo"] - RAM_BASE:p["hi"] - RAM_BASE] = p["blob"]
    return bytes(img), hdr, placements


def report(img, hdr, placements, out, wrote=True):
    print("[ram_image] PS-EXE header: pc0=0x{pc0:08X} t_addr=0x{t_addr:08X} t_size=0x{t_size:X} "
          "d_size=0x{d_size:X} b_addr=0x{b_addr:08X} b_size=0x{b_size:X} s_addr=0x{s_addr:08X} "
          "gp0=0x{gp0:08X}".format(**hdr))
    total = 0
    for p in placements:
        total += p["hi"] - p["lo"]
        print(f"[ram_image] placed {p['name']:<28} {p['src']:<24} -> "
              f"[0x{p['lo']:08X},0x{p['hi']:08X}) {p['hi'] - p['lo']} B")
    zero = RAM_SIZE - total
    print(f"[ram_image] {len(placements)} placement(s), {total} B placed of {RAM_SIZE} "
          f"({100.0 * total / RAM_SIZE:.1f}%); {zero} B ({100.0 * zero / RAM_SIZE:.1f}%) of the image is "
          "ZERO = memory this image says NOTHING about")
    if wrote:
        print(f"[ram_image] wrote {os.path.relpath(out, ROOT)} (gitignored — it is derived from a "
              "copyrighted executable and must never be committed)")
        print(f"[ram_image] wrote {os.path.relpath(manifest_path(out), ROOT)} — the placement extent, "
              "for consumers that must NOT re-derive it from Ghidra (zeros disassemble as nop)")
    for b in BLIND_SPOTS:
        print(f"[ram_image] blind spot: {b}")


def manifest_path(out):
    return out + ".placements.json"


def write_manifest(out, hdr, placements):
    """Record which bytes this image actually carries, beside the image.

    Ghidra's defined instructions overstate the extent (zero words disassemble as nops, 215,308 words against
    the header's 148,992), so the extent is published here and consumers refuse without it."""
    import json
    man = {"image": os.path.basename(out), "ram_base": RAM_BASE, "ram_size": RAM_SIZE,
           "header": {k: hdr[k] for k in HDR_KEYS},
           "placements": [{"name": p["name"], "src": p["src"], "lo": p["lo"], "hi": p["hi"]}
                          for p in placements]}
    with open(manifest_path(out), "w") as f:
        json.dump(man, f, indent=2, sort_keys=True)
        f.write("\n")
    return manifest_path(out)

def parse_overlay(arg):
    if "@" not in arg:
        raise Refuse(f"--overlay {arg}: expected <path>@<0xBASE>")
    path, _, base = arg.rpartition("@")
    if not base.lower().startswith("0x"):
        raise Refuse(f"--overlay {arg}: base must be hex with a 0x prefix (got {base!r}) — a decimal "
                     "base here would be a silent off-by-a-megabyte")
    return path, int(base, 16)


def main(argv):
    out, exe, overlays = OUT_DEFAULT, EXE, []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("-o", "--out"):
            i += 1
            out = os.path.abspath(argv[i])
        elif a == "--overlay":
            i += 1
            overlays.append(parse_overlay(argv[i]))
        elif a == "--exe":
            i += 1
            exe = os.path.abspath(argv[i])
        else:
            print(f"unknown argument {a!r}; see the docstring", file=sys.stderr)
            return 2
        i += 1
    img, hdr, pl = build(exe, overlays)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(img)
    write_manifest(out, hdr, pl)
    report(img, hdr, pl, out)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Refuse as e:
        print(f"[ram_image] REFUSED: {e}", file=sys.stderr)
        sys.exit(2)
