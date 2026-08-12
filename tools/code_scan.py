#!/usr/bin/env python3
"""code_scan.py — does a disc file contain R3000A CODE, or only data?

THE QUESTION IT EXISTS TO ANSWER: does this game stream code overlays (the way Vagrant Story streams
.PRG modules), or is the boot executable the whole engine? For Toy Story 2 the answer came out YES —
22 overlay files, docs/info/claims/002-* — which is the opposite of the answer this file's original
consumer (megamanx4) got from the identical code.

THIS FILE IS A VERBATIM COPY of megamanx4/tools/code_scan.py: identical logic, identical thresholds,
identical selftest, only this paragraph and the `.ARC` example below reworded. It is deliberately NOT
a fourth code detector — a re-implementation would have its own calibration and its own bugs, and the
whole point of the census is that the discriminator underneath it is ONE calibrated copy imported from
external/psxport/tools/exe_similarity.py. If it is ever hoisted into psxport/tools/port/, this copy
becomes a shim.

METHOD, reusing psxport's already-calibrated code/data discriminator (`tools/exe_similarity.py`
`word_valid` / `norm`, recalibrated + selftested 2026-08-12 over 5215 hand-disassembled code
windows): a 16-word window counts as CODE-PLAUSIBLE when all 16 words are legal R3000A instructions
AND the window holds >=4 distinct normalised opcode shapes. Measured over verified code, every
window passes both; confirmed data tables sit at <=2 distinct tokens.

Because a file inside an archive need not be word-aligned to the archive start, every file is scanned
at all FOUR byte phases and the BEST phase is reported. A second, independent signal is printed
beside it: the density of `jr $ra` (0x03E00008), the MIPS function epilogue — real code has one every
few tens of words, data essentially never has them at a plausible rate.

WHAT A NEGATIVE PRINTS (this is the point): every file prints its denominator — bytes read, windows
examined at the winning phase, windows kept, and the largest contiguous plausible run. "0.0% of
17,412 windows" is a measurement; "no code found" would not be.

BLIND SPOTS, stated because the tool cannot see past them:
  * COMPRESSED or encrypted code would read as data. This tool cannot distinguish "no code" from
    "packed code". A packed overlay would show as high-entropy data with ~0 plausible windows.
  * Very short code fragments (< 16 words = 64 bytes) are invisible by construction.
  * It reads whole files as flat blobs; it does not parse the .ARC container, so a small code member
    inside a large texture archive contributes a small percentage rather than standing out.
  * It says nothing about WHERE code would be loaded. That is the loader question, answered
    separately by looking at the boot exe.

--selftest gates BOTH classes and exits non-zero if either regresses.
"""
import argparse
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# The discriminator itself lives in the framework, so there is ONE calibrated copy. Resolved the same
# way CMake resolves it: $PSXPORT_DIR, defaulting to the pinned submodule — never an absolute path.
_PX = os.environ.get("PSXPORT_DIR") or os.path.join(ROOT, "external", "psxport")
if not os.path.isabs(_PX):
    _PX = os.path.join(ROOT, _PX)
if not os.path.isfile(os.path.join(_PX, "tools", "exe_similarity.py")):
    print(f"REFUSING: no exe_similarity.py under {_PX}/tools — the code/data discriminator is not "
          "available, so NOTHING was scanned. `git submodule update --init external/psxport`, or set "
          "$PSXPORT_DIR.", file=sys.stderr)
    sys.exit(2)
sys.path.insert(0, os.path.join(_PX, "tools"))
from exe_similarity import word_valid, norm  # noqa: E402

WINDOW = 16
MIN_VALID = 16
MIN_DISTINCT = 4
JR_RA = 0x03E00008


def refuse(msg):
    print(f"REFUSING: {msg}", file=sys.stderr)
    sys.exit(2)


def scan_blob(data):
    """Best-of-4-phases scan. Returns dict of metrics, or None if the blob is shorter than a window."""
    best = None
    for phase in range(4):
        img = data[phase:]
        n = (len(img) - 4) // 4
        if n <= WINDOW:
            continue
        words = list(struct.unpack_from("<%dI" % n, img, 0))
        valid = [word_valid(w) for w in words]
        toks = [norm(w) for w in words]
        nwin = n - WINDOW
        kept = 0
        run = best_run = 0
        for i in range(nwin):
            if all(valid[i:i + WINDOW]) and len(set(toks[i:i + WINDOW])) >= MIN_DISTINCT:
                kept += 1
                run += 1
                best_run = max(best_run, run)
            else:
                run = 0
        jr = sum(1 for w in words if w == JR_RA)
        m = dict(phase=phase, words=n, windows=nwin, kept=kept,
                 pct=100.0 * kept / nwin if nwin else 0.0,
                 jr=jr, jr_per_kw=1000.0 * jr / n if n else 0.0,
                 # a plausible run of R windows spans R+15 words
                 run_bytes=(best_run + WINDOW - 1) * 4 if best_run else 0)
        if best is None or m["kept"] > best["kept"]:
            best = m
    return best


def load(path, off=0, length=None):
    if not os.path.isfile(path):
        refuse(f"{path} does not exist — scanned NOTHING of it.")
    with open(path, "rb") as f:
        f.seek(off)
        return f.read(length) if length else f.read()


def row(label, data):
    m = scan_blob(data)
    if m is None:
        print(f"{label:<24} {len(data):>9} bytes  TOO SHORT for a 16-word window — NOT SCANNED")
        return None
    print(f"{label:<24} {len(data):>9} bytes  ph{m['phase']}  "
          f"{m['kept']:>7}/{m['windows']:<7} windows code-plausible = {m['pct']:5.1f}%  "
          f"jr$ra {m['jr']:>5} ({m['jr_per_kw']:5.2f}/1k words)  "
          f"longest run {m['run_bytes']:>7} B")
    return m


def selftest(exe, neg_file):
    """POSITIVE: a region the PS-EXE header itself certifies is code — 64 KiB starting at the entry
       point pc0 — must read as overwhelmingly code-plausible.
       NEGATIVE: an XA/STR media file must read as essentially no code.
       Either regressing exits 1 — a discriminator is only trustworthy run against both classes.

       The positive is deliberately NOT the whole .text: .text measures 79.6%, because this game's
       t_size covers its data too (d_addr/d_size are 0 and a 160-entry file-path table sits at file
       offset 0xDED4E). Gating on the whole segment would be gating on a code/data MIXTURE and would
       have to be tuned down to pass, which is how a threshold stops meaning anything. The entry
       region is code by the header's own declaration."""
    data = load(exe)
    pc0, t_addr = struct.unpack_from("<I", data, 0x10)[0], struct.unpack_from("<I", data, 0x18)[0]
    off = 0x800 + (pc0 - t_addr)
    print(f"[selftest] POSITIVE class — 64 KiB at entry pc0=0x{pc0:08X} (file offset 0x{off:X}), "
          "must be >=95% code-plausible")
    p = row("POS entry region", data[off:off + (64 << 10)])
    print("[selftest] NEGATIVE class — a media file chunk (must be <=2% code-plausible)")
    n = row("NEG media chunk", load(neg_file, 0, 4 << 20))
    fails = []
    if p is None or p["pct"] < 95.0:
        fails.append(f"POSITIVE only {p['pct'] if p else 0:.1f}% — the detector cannot see known code")
    if n is None or n["pct"] > 2.0:
        fails.append(f"NEGATIVE {n['pct'] if n else 0:.1f}% — the detector fires on known non-code")
    for f in fails:
        print(f"[selftest] FAIL {f}")
    print(f"[selftest] {'PASS — both classes behave' if not fails else 'FAILED'}")
    return 1 if fails else 0


def census(directory, glob_pat, ctrl_exe):
    """THE headline measurement, as a reproducible command: over EVERY file in `directory`, three
    independent signals, each with a control from a binary known to be code.

    Signal 1  `jr $ra` count at ANY byte alignment  — real MIPS has ~12-20 per 1k words.
    Signal 2  code-plausible 16-word windows (%)    — the calibrated exe_similarity filter.
    Signal 3  fraction of j/jal whose target lands inside the control exe's .text.

    Three signals because one of them lies on this corpus: a glyph-pattern archive reaches 60%
    code-plausible windows with ZERO jr $ra and 6% valid call targets. No single number decides.
    """
    import glob as _g
    files = sorted(_g.glob(os.path.join(directory, glob_pat)))
    if not files:
        refuse(f"{directory}/{glob_pat} matched ZERO files — scanned NOTHING. This is not a "
               "'no code found' result.")
    exe = load(ctrl_exe)
    t_addr = struct.unpack_from("<I", exe, 0x18)[0]
    t_end = t_addr + struct.unpack_from("<I", exe, 0x1C)[0]
    pat = struct.pack("<I", JR_RA)

    def jal_frac(data, off=0):
        n = (len(data) - off - 4) // 4
        if n <= 0:
            return 0, 0.0
        ws = struct.unpack_from("<%dI" % n, data, off)
        js = [x for x in ws if (x >> 26) in (2, 3)]
        ok = [x for x in js if t_addr <= (((x & 0x3FFFFFF) << 2) | 0x80000000) < t_end]
        return len(js), (100.0 * len(ok) / len(js) if js else 0.0)

    def count_jr(data):
        c, i = 0, data.find(pat)
        while i >= 0:
            c += 1
            i = data.find(pat, i + 1)
        return c

    print(f"CONTROL {os.path.basename(ctrl_exe)} .text 0x{t_addr:08X}..0x{t_end:08X}")
    m = row("  CTRL .text", exe[0x800:])
    nj, fr = jal_frac(exe, 0x800)
    print(f"  CTRL j/jal={nj} into-.text={fr:.1f}%")
    print(f"\nCENSUS DENOMINATOR: {len(files)} files matching {glob_pat} under {directory}")
    tot = withjr = 0
    rows = []
    for p in files:
        d = load(p)
        tot += len(d)
        jr = count_jr(d)
        withjr += 1 if jr else 0
        s = scan_blob(d)
        nj, fr = jal_frac(d)
        rows.append((s["pct"] if s else -1.0, os.path.basename(p), len(d), jr, nj, fr))
    rows.sort(reverse=True)
    print(f"  {tot} bytes ({tot / 1048576:.1f} MiB) read in full")
    print(f"  files containing >=1 `jr $ra` at ANY alignment: {withjr} of {len(files)}")
    print("  ranked by code-plausible % (top 12; the remainder are all lower):")
    for pct, n, s, jr, nj, fr in rows[:12]:
        print(f"    {n:<16} {s:>8} B  {pct:5.1f}% plausible  jr$ra {jr:>4}  "
              f"j/jal {nj:>5} into-.text {fr:5.1f}%")
    if len(rows) > 12:
        print(f"    ... TRUNCATED: {len(rows) - 12} further files not printed")
    print("  BLIND SPOT: compressed/packed code would land in the low band with the data. This "
          "census can say 'no PLAIN R3000A code', not 'no code'.")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="*")
    ap.add_argument("--census", metavar="DIR",
                    help="run the three-signal census over every --glob match in DIR")
    ap.add_argument("--glob", default="*", help="census file pattern (default *)")
    ap.add_argument("--ctrl-exe", help="PS-X EXE used as the code control for --census")
    ap.add_argument("--exe", help="a PS-X EXE; its .text (file offset 0x800+) is scanned")
    ap.add_argument("--cap", type=int, default=0, help="scan only the first N bytes of each file")
    ap.add_argument("--selftest", nargs=2, metavar=("EXE", "MEDIA"))
    a = ap.parse_args()
    if a.selftest:
        return selftest(*a.selftest)
    if a.census:
        if not a.ctrl_exe:
            refuse("--census needs --ctrl-exe: without a known-code control the numbers have no "
                   "scale and a 0% could mean the detector is broken. Scanned NOTHING.")
        return census(a.census, a.glob, a.ctrl_exe)
    if not a.files and not a.exe:
        refuse("no files given — scanned NOTHING. This is not a 'no code' result.")
    if a.exe:
        row(os.path.basename(a.exe) + " .text", load(a.exe, 0x800))
    for p in a.files:
        row(os.path.basename(p), load(p, 0, a.cap or None))
    return 0


if __name__ == "__main__":
    sys.exit(main())
