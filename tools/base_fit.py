#!/usr/bin/env python3
"""base_fit.py — MEASURE where an overlay module would be LOADED, instead of assuming it.

  python3 tools/base_fit.py                       # every file in scratch/flat/
  python3 tools/base_fit.py 'scratch/flat/LEVEL*'  # any glob(s), repo-relative or absolute
  python3 tools/base_fit.py --selftest            # gates BOTH classes over the real corpus

THE QUESTION. `tools/code_scan.py` answers "is this file code?". It says nothing about WHERE that code
lives, and an overlay is keyed BY its load address: a wrong base emits a whole module of correctly
decoded instructions at wrong addresses, and every jal target, pointer test and router lookup then goes
silently wrong. So the base has to be measured before a single overlay may be recompiled (RE-03).

METHOD. A statically linked overlay's own intra-module calls are absolute `jal` into
[base, base+filesize). Slide a candidate base over every 4 KiB-aligned address in RAM and score it by
how many of the file's out-of-boot-.text jal targets land inside that window. Report the argmax WITH a
runner-up that is at least one file-length away — adjacent 4 KiB bases overlap almost entirely, so
"next best base" would be a tie by construction and a margin test over it could never fire.

WHAT A NEGATIVE PRINTS. Per file: the number of out-of-.text jal targets (the denominator), the best
base and its hit fraction, and the runner-up's. Three distinct negatives are distinguishable and all
three are printed rather than collapsed into silence:
  * `NO out-of-.text jal` — NO EVIDENCE either way (a leaf-only or tiny module looks like this too).
  * `no fit` — a base was scored and REJECTED: the landscape is flat or the margin is noise. This is
    what a data file must look like, and 100+ of this disc's files do look like it.
  * `TOO SHORT` — under one word.
A run over a missing or empty corpus REFUSES (exit 2) saying it fitted NOTHING.

BLIND SPOTS, because the method cannot see past them:
  * PLAIN, absolutely-linked code only. A relocated (PIC / fixup-table) module has no absolute jal to
    fit and would read as NO EVIDENCE.
  * 4 KiB granularity by default (--step): a real base that is not 4 KiB-aligned is reported at the
    aligned address below it. RE-03's instruction-derived verifier now supplies the exact bases.
  * A fit is evidence about ONE module in isolation. It cannot tell you whether two modules that both
    fit the same base are alternatives sharing one slot or two modules at two bases. This instrument
    cannot reproduce RE-03's resolved slot-count answer; `tools/overlay_map.py` owns that question.
"""
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLAT = os.path.join(ROOT, "scratch", "flat")
EXE = os.environ.get("TS2_EXE") or os.path.join(ROOT, "scratch", "bin", "toystory2", "SLUS_008.93")
RAM_LO, RAM_HI = 0x80010000, 0x80200000

BLIND_SPOTS = [
    "PLAIN, absolutely-linked code only — a relocated (PIC/fixup-table) module has no absolute jal to "
    "fit and reads as NO EVIDENCE",
    "4 KiB granularity by default (--step): a base that is not 4 KiB-aligned is reported at the aligned "
    "address below it",
    "a fit is evidence about ONE module in isolation: two modules fitting the same base may be "
    "alternatives sharing a slot OR two modules at two bases; overlay_map.py owns the resolved answer",
]

# The measured overlay slot (docs/info/claims/003-*). Used ONLY by --selftest, as the value the
# positive class must keep re-deriving from the bytes; the reporting path never mentions it, so an
# ordinary run cannot be biased by it.
SLOT = 0x800D1000
# The DERIVATION gate's anchors, measured 2026-08-12 over scratch/flat and NOT chosen for convenience:
# 9 of the 10 LEVEL*/LEVEL.BIN modules peak at SLOT and LEVEL09 peaks one page above it. That single
# disagreement is what makes the gate non-vacuous — a tool that reports SLOT for everything scores 10.
POS_AT_SLOT = 9
OUTLIER = "LEVEL09__LEVEL.BIN"
OUTLIER_BASE = 0x800D2000


def text_range(exe_path):
    if not os.path.isfile(exe_path):
        print(f"REFUSING: no boot executable at {exe_path} — without its .text range every jal target "
              "counts as 'out of .text' and every file would appear to fit something. Run "
              "`python3 tools/extract_exe.py` (or set $TS2_EXE).", file=sys.stderr)
        raise SystemExit(2)
    d = open(exe_path, "rb").read()
    if d[:8] != b"PS-X EXE":
        print(f"REFUSING: {exe_path} is not a PS-X EXE", file=sys.stderr)
        raise SystemExit(2)
    t_addr, t_size = struct.unpack_from("<II", d, 0x18)
    return t_addr, t_addr + t_size


def fit(path, t_addr, t_end, step=0x1000):
    """(verdict, n_out, best_base, best_frac, runner_base, runner_frac). verdict in
    {'fit','nofit','noevidence','short'}."""
    d = open(path, "rb").read()
    n = len(d) // 4
    if n <= 0:
        return "short", 0, 0, 0.0, 0, 0.0
    ws = struct.unpack_from("<%dI" % n, d, 0)
    out = [t for t in (((w & 0x3FFFFFF) << 2) | 0x80000000 for w in ws if (w >> 26) in (2, 3))
           if not (t_addr <= t < t_end)]
    if not out:
        return "noevidence", 0, 0, 0.0, 0, 0.0
    scores = sorted(((sum(1 for t in out if b <= t < b + len(d)), b)
                     for b in range(RAM_LO, RAM_HI, step)), reverse=True)
    h1, b1 = scores[0]
    h2, b2 = next((s for s in scores[1:] if abs(s[1] - b1) >= max(len(d), step)), (0, 0))
    f1, f2 = h1 / len(out), h2 / len(out)
    good = f1 >= 0.5 and h1 - h2 >= 2 and len(out) >= 4
    return ("fit" if good else "nofit"), len(out), b1, f1, b2, f2


def collect(pats):
    files = sorted({f for p in pats for f in glob.glob(p if os.path.isabs(p) else os.path.join(ROOT, p))
                    if os.path.isfile(f)})
    if not files:
        print(f"REFUSING: {pats} matched ZERO files — fitted NOTHING. This is not a 'no overlays' "
              f"result. Populate the corpus with `python3 tools/extract_disc_files.py`.", file=sys.stderr)
        raise SystemExit(2)
    return files


def report(pats, step=0x1000):
    t_addr, t_end = text_range(EXE)
    files = collect(pats)
    print(f"boot .text 0x{t_addr:08X}..0x{t_end:08X} (from the PS-EXE header of {os.path.basename(EXE)}); "
          f"fitting {len(files)} files at {step // 1024} KiB granularity")
    print(f"{'file':<28}{'out-jal':>8} {'best base':>12} {'hit':>7}  {'runner-up':>11} {'hit':>7}  verdict")
    tally = {"fit": 0, "nofit": 0, "noevidence": 0, "short": 0}
    for p in files:
        v, n, b1, f1, b2, f2 = fit(p, t_addr, t_end, step)
        tally[v] += 1
        name = os.path.basename(p)
        if v == "short":
            print(f"{name:<28}{'-':>8}  TOO SHORT (under one word) — NOT FITTED")
        elif v == "noevidence":
            print(f"{name:<28}{0:>8}  NO out-of-.text jal — NO EVIDENCE either way")
        else:
            print(f"{name:<28}{n:>8}   0x{b1:08X} {f1 * 100:6.1f}%  0x{b2:08X} {f2 * 100:6.1f}%  "
                  f"{'FITS a base' if v == 'fit' else 'no fit'}")
    print(f"\n{tally['fit']} of {len(files)} files fit a load base under the stated rule "
          "(>=50% of >=4 out-of-.text jals inside [base,base+size), margin >=2 over a runner-up at "
          "least one file-length away)")
    print(f"the other {len(files) - tally['fit']}: {tally['nofit']} scored and REJECTED, "
          f"{tally['noevidence']} with no out-of-.text jal at all (no evidence either way), "
          f"{tally['short']} too short")
    for b in BLIND_SPOTS:
        print(f"  blind spot: {b}")
    return 0


def score_at(path, base, t_addr, t_end):
    """Hit fraction of a SPECIFIC base — the primitive the DERIVATION gate needs. Returns
    (hits, n_out); (0, 0) when the file has no out-of-.text jal to score."""
    d = open(path, "rb").read()
    n = len(d) // 4
    if n <= 0:
        return 0, 0
    ws = struct.unpack_from("<%dI" % n, d, 0)
    out = [t for t in (((w & 0x3FFFFFF) << 2) | 0x80000000 for w in ws if (w >> 26) in (2, 3))
           if not (t_addr <= t < t_end)]
    return sum(1 for t in out if base <= t < base + len(d)), len(out)


def selftest(step=0x1000):
    """POSITIVE: the LEVEL*/LEVEL.BIN overlays must fit, and mostly at the recorded slot.
    DERIVATION: the reported base must be an ARGMAX OVER THE BYTES, not the recorded slot echoed back.
      Two independent assertions, both of which a hardcoded `base = SLOT` fails:
        (a) the corpus disagrees with the slot in a known place — LEVEL09/LEVEL.BIN's argmax is
            0x800D2000, NOT the slot — so exactly 9 of 10 may sit at SLOT and LEVEL09 must not;
        (b) a DECOY base one step above each fitter's reported base must score STRICTLY WORSE than the
            reported one. A tool echoing a constant would report a base it never scored, and the decoy
            (or the reported base itself) would tie or win.
      This gate exists because `len(at_slot) >= 5` alone is MAXIMISED by a hardcode: the earlier
      selftest passed with `h1, b1 = scores[0][0], 0x800D1000` spliced into fit().
    NEGATIVE 1: no .ALL/.RAW/.DAT asset may fit any base.
    NEGATIVE 2: PAD/PATH*.BIN — the trap case (code-plausible bytes, zero jal) — must not fit."""
    t_addr, t_end = text_range(EXE)
    fails = []

    pos = collect([os.path.join(FLAT, "LEVEL0[1-9]__LEVEL.BIN"), os.path.join(FLAT, "LEVEL10__LEVEL.BIN")])
    # (name, verdict, n_out, base, frac, runner_base, runner_frac) per file, scored ONCE.
    scored = [(os.path.basename(p),) + fit(p, t_addr, t_end, step) for p in pos]
    fits = [(s[0],) + s[2:] for s in scored if s[1] == "fit"]
    at_slot = [f for f in fits if f[2] == SLOT]
    print(f"[POSITIVE] {len(pos)} LEVEL*/LEVEL.BIN overlays — must fit a base, and most at one shared slot")
    for name, n, b, f1, b2, f2 in fits:
        print(f"  {name:<24} out-jal {n:>4}  base 0x{b:08X} {f1 * 100:5.1f}%  runner-up 0x{b2:08X} "
              f"{f2 * 100:5.1f}%")
    print(f"  fitted {len(fits)} of {len(pos)}; {len(at_slot)} of them at 0x{SLOT:08X}")
    if len(fits) < 6:
        fails.append(f"only {len(fits)} of {len(pos)} LEVEL.BIN modules fit a base (expected >=6)")
    if len(at_slot) != POS_AT_SLOT:
        fails.append(f"{len(at_slot)} of {len(pos)} modules fit the recorded slot 0x{SLOT:08X}, expected "
                     f"exactly {POS_AT_SLOT} — either the corpus changed, the recorded slot is wrong, or "
                     f"the base is being ASSERTED rather than measured (a hardcode gives {len(pos)}); "
                     "claims/003 is the arbiter")

    # ---- DERIVATION GATE: is the reported base an argmax over the bytes, or a constant echoed back? ----
    by_name = {os.path.basename(p): p for p in pos}
    print(f"[DERIVATION] the reported base must come from the bytes, not from SLOT. {OUTLIER} disagrees "
          f"with the slot in the real corpus (argmax 0x{OUTLIER_BASE:08X}), and a decoy base "
          f"+0x{step:X} must score strictly worse than the reported one.")
    got = {f[0]: f[2] for f in fits}
    if got.get(OUTLIER) != OUTLIER_BASE:
        fails.append(f"{OUTLIER} reports base 0x{(got.get(OUTLIER) or 0):08X}, expected the measured "
                     f"argmax 0x{OUTLIER_BASE:08X}. The one file in the corpus that DISAGREES with the "
                     "recorded slot no longer does, which is what a hardcoded base looks like")
    for name, n, b, f1, b2, f2 in fits:
        h_rep, n_out = score_at(by_name[name], b, t_addr, t_end)
        h_dec, _ = score_at(by_name[name], b + step, t_addr, t_end)
        claimed = round(f1 * n)
        print(f"  {name:<24} reported base 0x{b:08X} claims {claimed:>4}/{n_out:<4} hits; "
              f"re-scored at that base {h_rep:>4}; decoy 0x{b + step:08X} {h_dec:>4}")
        if h_rep != claimed:
            fails.append(f"{name}: the reported base 0x{b:08X} re-scores {h_rep}/{n_out} but the tool "
                         f"claimed {claimed} — the printed base is NOT the base that produced the score, "
                         "i.e. the value is asserted, not derived")
        if h_dec >= h_rep:
            fails.append(f"{name}: decoy base 0x{b + step:08X} scores {h_dec} >= the reported base's "
                         f"{h_rep} — the landscape has no peak at the reported base, so the fit is not "
                         "an argmax")

    neg1 = collect([os.path.join(FLAT, "*.ALL"), os.path.join(FLAT, "*.RAW"), os.path.join(FLAT, "*.DAT")])
    bad1 = [os.path.basename(p) for p in neg1 if fit(p, t_addr, t_end, step)[0] == "fit"]
    print(f"[NEGATIVE 1] {len(neg1)} .ALL/.RAW/.DAT asset files — NONE may fit a base")
    print(f"  fitted: {len(bad1)}" + (f" :: {bad1[:8]}" if bad1 else ""))
    if bad1:
        fails.append(f"{len(bad1)} asset files fitted a load base: {bad1[:8]} — the discriminator is "
                     "admitting data, so every 'FITS' it reports is suspect")

    neg2 = collect([os.path.join(FLAT, "PAD__PATH*.BIN")])
    bad2 = [os.path.basename(p) for p in neg2 if fit(p, t_addr, t_end, step)[0] == "fit"]
    print(f"[NEGATIVE 2] {len(neg2)} PAD/PATH*.BIN — the trap case: 48.9-87.2% code-plausible bytes with "
          "ZERO j/jal. Must not fit")
    print(f"  fitted: {len(bad2)}" + (f" :: {bad2}" if bad2 else ""))
    if bad2:
        fails.append(f"PATH data fitted a base: {bad2}")

    print()
    if fails:
        for f in fails:
            print(f"FAIL: {f}")
        return 1
    print(f"SELFTEST PASS: the positive class re-derived its bases from the bytes (argmax consistency + "
          f"decoy + the LEVEL09 disagreement), and {len(neg1) + len(neg2)} negative files were all "
          f"rejected.")
    return 0


if __name__ == "__main__":
    a = sys.argv[1:]
    step = 0x1000
    if "--step" in a:
        i = a.index("--step")
        step = int(a[i + 1], 0)
        del a[i:i + 2]
    if a and a[0] == "--selftest":
        sys.exit(selftest(step))
    sys.exit(report(a or [os.path.join(FLAT, "*")], step))
