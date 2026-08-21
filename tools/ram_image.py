#!/usr/bin/env python3
"""ram_image.py — build the 2 MiB KSEG0 image Ghidra is imported over, HEADER-DRIVEN.

  python3 tools/ram_image.py                                  # boot exe only -> scratch/ghidra/ram-boot.bin
  python3 tools/ram_image.py -o scratch/ghidra/ram-l1.bin \\
      --overlay scratch/flat/LEVEL01__LEVEL.BIN@0x800D12C0    # + one overlay at RE-03's proven base
  python3 tools/ram_image.py --selftest                       # gates BOTH classes

WHY THIS EXISTS. `external/psxport/tools/decomp.sh` imports a **RAM DUMP** as a flat binary based at
0x80000000, so every Ghidra address equals a guest virtual address. This port has no emulator run and
therefore no dump; what it has is a PS-EXE, whose bytes start 0x800 into the file and land at `t_addr`.
Importing the PS-EXE *directly* at 0x80000000 would put every instruction 0x8000F800 bytes below its
real address — and Ghidra would decompile it perfectly, so the error is invisible in the output. This
tool does the placement ONCE, from the header, so no session ever does that arithmetic by hand.

WHAT IT REFUSES TO DO (exit 2, never a partial image):
  * a file that is not `PS-X EXE`;
  * `0x800 + t_size != filesize` — a truncated or padded extraction;
  * a placement (text or overlay) that leaves [0x80000000, 0x80200000);
  * two placements that OVERLAP — silently one-clobbering-the-other is how a fabricated image gets
    decompiled as if it were real;
  * an `--overlay` base given without `0x` hex, or a missing input file.
It also does NOT verify the exe's identity — that is `tools/extract_exe.py`'s job and duplicating the
expectation would let the two drift. It re-reads and prints the header instead.

WHAT A NEGATIVE / A SUCCESS BOTH PRINT — the denominator: every placement with its file offset, guest
range and byte count, the total bytes placed, and the fraction of the 2 MiB that is ZERO. Zero is not
"memory that is zero at boot", it is "memory this image says NOTHING about", and the difference is the
whole reason the blind spots are printed on every run.

BLIND SPOTS, printed every run:
  * BSS IS NOT MATERIALISED. The header declares b_size = 0 and this game clears its own BSS (RE-01),
    so everything above the loaded image reads as 0 here. A decompiled body that loads from such an
    address gives you the ADDRESS, never the value.
  * NO OVERLAY IS PRESENT unless you inject it. The tool accepts any explicit base, so an image is
    evidence only when that base is independently proven; C010 proves the LEVEL slot at 0x800D12C0.
  * NO RELOCATION AND NO PATCHING. crt0 has not run: $gp is unset, and any address the loader or a
    fixup table would have rewritten still holds its on-disc value.
  * Hardware registers (0x1F80xxxx) and the BIOS ROM are absent — this is RAM only, so a jal into the
    BIOS lands in nothing.
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
            raise Refuse(f"--overlay {path}: no such file (extract with tools/extract_disc_files.py)")
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
    """Record WHICH BYTES THIS IMAGE ACTUALLY CARRIES, beside the image.

    The 2 MiB image is otherwise indistinguishable from RAM that is genuinely zero, and a consumer
    that re-derives the extent from Ghidra's DEFINED INSTRUCTIONS gets a WRONG, LARGER span: a run of
    0x00000000 disassembles as valid `sll $zero,$zero,0` (nop), so analysis walks straight off the end
    of .text into the zero region and "the instruction range" silently includes fabricated code.
    Measured 2026-08-12: 215,308 words that way against the header's 148,992. So the extent is
    published HERE, by the tool that performed the placement, and consumers refuse without it."""
    import json
    man = {"image": os.path.basename(out), "ram_base": RAM_BASE, "ram_size": RAM_SIZE,
           "header": {k: hdr[k] for k in HDR_KEYS},
           "placements": [{"name": p["name"], "src": p["src"], "lo": p["lo"], "hi": p["hi"]}
                          for p in placements]}
    with open(manifest_path(out), "w") as f:
        json.dump(man, f, indent=2, sort_keys=True)
        f.write("\n")
    return manifest_path(out)


def selftest():
    """Gate BOTH classes: a real exe must build and land pc0's word; five LIES must each REFUSE."""
    import tempfile
    fails = []
    checks = []

    def ck(name, ok, detail=""):
        checks.append((name, ok, detail))
        if not ok:
            fails.append(name)

    print("[selftest] plan: 1 POSITIVE (the real boot exe builds and pc0 holds a decodable word) and "
          "6 NEGATIVES that must each REFUSE (exit 2). A build that merely 'succeeds' proves nothing: "
          "the placement arithmetic is only tested by reading a byte back at a KNOWN guest address.")
    if not os.path.isfile(EXE):
        print(f"[selftest] REFUSED: the corpus is absent ({EXE}). This selftest cannot pass or fail "
              "without the boot exe — run tools/extract_exe.py.", file=sys.stderr)
        return 2

    # POSITIVE. The discriminating check is not "it built" but "a byte at a guest address I can name
    # independently is the byte the FILE has at the offset the header implies".
    img, hdr, pl = build(EXE, [])
    off = hdr["pc0"] - RAM_BASE
    word = struct.unpack_from("<I", img, off)[0]
    raw = open(EXE, "rb").read()
    file_word = struct.unpack_from("<I", raw, HDR_BYTES + hdr["pc0"] - hdr["t_addr"])[0]
    ck("positive: pc0's word in the image == the same word in the FILE",
       word == file_word, f"image 0x{word:08X} vs file 0x{file_word:08X}")
    # ...and that it is a plausible instruction rather than the padding an off-by-a-segment would give.
    ck("positive: pc0's word is not 0 (an image placed at the wrong base reads padding here)",
       word != 0, f"0x{word:08X}")
    ck("positive: the whole text segment is placed", pl[0]["hi"] - pl[0]["lo"] == hdr["t_size"])

    # The MANIFEST is the only thing that tells a consumer which bytes are real, so gate that it says
    # the same thing the image does — and that it says it in GUEST addresses, not file offsets.
    import json
    scratch_m = os.path.join(ROOT, "scratch", "selftest")
    os.makedirs(scratch_m, exist_ok=True)
    mimg = os.path.join(scratch_m, "manifest-probe.bin")
    write_manifest(mimg, hdr, pl)
    man = json.load(open(manifest_path(mimg)))
    os.remove(manifest_path(mimg))
    ck("positive: the manifest reports the placed span in GUEST addresses and nothing else",
       [(p["lo"], p["hi"]) for p in man["placements"]] == [(hdr["t_addr"], hdr["t_addr"] + hdr["t_size"])],
       str(man["placements"]))

    def refuses(name, fn):
        try:
            fn()
        except Refuse as e:
            ck("negative: " + name, True, str(e)[:90])
            return
        ck("negative: " + name, False, "BUILT ANYWAY — this class is undetected")

    # NEVER /tmp — a small RAM-backed tmpfs on this machine, and the truncated-exe case alone is
    # ~0.6 MB. scratch/ is gitignored and on disk (CLAUDE.md).
    scratch = os.path.join(ROOT, "scratch", "selftest")
    os.makedirs(scratch, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as td:
        bad = os.path.join(td, "notanexe.bin")
        open(bad, "wb").write(b"MZ" + b"\0" * 0x900)
        refuses("a non-PS-EXE input", lambda: build(bad, []))

        trunc = os.path.join(td, "trunc.exe")
        open(trunc, "wb").write(raw[:len(raw) - 4])
        refuses("a TRUNCATED exe (0x800+t_size != filesize)", lambda: build(trunc, []))

        refuses("an absent exe path", lambda: build(os.path.join(td, "nope.exe"), []))

        ov = os.path.join(td, "ov.bin")
        open(ov, "wb").write(b"\x01" * 0x1000)
        refuses("an overlay placed OUTSIDE the 2 MiB RAM",
                lambda: build(EXE, [(ov, 0x801FFF00)]))
        refuses("an overlay OVERLAPPING the boot text",
                lambda: build(EXE, [(ov, hdr["t_addr"] + 0x100)]))
        refuses("two overlays overlapping EACH OTHER",
                lambda: build(EXE, [(ov, 0x800D1000), (ov, 0x800D1800)]))

    for name, ok, detail in checks:
        print(f"[selftest] {'PASS' if ok else 'FAIL'}  {name}" + (f"   ({detail})" if detail else ""))
    print(f"[selftest] {len(checks) - len(fails)}/{len(checks)} passed; "
          f"1 positive class and 6 negative classes exercised")
    print("[selftest] what this CANNOT see: whether the exe is the RIGHT exe (that is "
          "tools/extract_exe.py + docs/info/exe-identity.txt), and whether an injected overlay base is "
          "CORRECT — no selftest can know that; use the independent RE-03 verifier.")
    return 1 if fails else 0


def parse_overlay(arg):
    if "@" not in arg:
        raise Refuse(f"--overlay {arg}: expected <path>@<0xBASE>")
    path, _, base = arg.rpartition("@")
    if not base.lower().startswith("0x"):
        raise Refuse(f"--overlay {arg}: base must be hex with a 0x prefix (got {base!r}) — a decimal "
                     "base here would be a silent off-by-a-megabyte")
    return path, int(base, 16)


def main(argv):
    if argv and argv[0] == "--selftest":
        return selftest()
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
