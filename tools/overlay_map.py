#!/usr/bin/env python3
"""overlay_map.py — WHERE does the CD loader put each file? Decoded out of the boot executable.

  python3 tools/overlay_map.py                  # the census + the slot arithmetic
  python3 tools/overlay_map.py --loader 0x...   # census a different callee
  python3 tools/overlay_map.py --selftest       # gates BOTH classes, exits non-zero on regression

THE QUESTION, and why this tool and not base_fit.py. `tools/base_fit.py` measures where a module's own
absolute `jal` targets say it COULD be loaded. That is a fit, and a fit has two limits this tool does
not: it is 4 KiB-granular, and it can never say HOW the loader arrives at a base. This tool answers the
mechanism instead, from the one place the answer is unambiguous — the loader's own call sites:

    lui  a1, 0x800D          # 0x8003DEA4
    addiu a1, a1, 0x12C0     # 0x8003DEA8   -> a1 = 0x800D12C0
    jal  0x80082508          # 0x8003DEAC   FileLoadTo(path, dest)

so the DESTINATION is a compile-time constant in the caller, and every file the game loads to a fixed
address is in the table this prints. A fit that disagrees with this is the fit being coarse.

THE ANCHOR, which is the one thing here that is RE and not derivation: `--loader` defaults to
0x80082508, the two-argument CD file loader, identified by decompiling it (docs/info/claims/008-*).
Everything else on this page is computed from the bytes. Point `--loader` somewhere else and the census
re-runs against that callee with no other change — which is exactly what --selftest's negative control
does.

WHAT A NEGATIVE PRINTS, every run, because "the table is empty" and "I never looked" must not read the
same: the words examined, the `jal` opcodes seen, the call sites found for this callee, and per site
whether each argument register FOLDED to a literal, was STACK/GP-relative, or was not resolvable — with
the instruction that defined it. A callee with zero call sites REFUSES (exit 2) rather than printing an
empty table; so does a missing executable and a missing flat corpus.

BLIND SPOTS, printed every run:
  * A destination computed at RUN TIME (a pointer read from memory, an allocator result, a $gp load) is
    invisible here — it is not a literal in the caller. Such a site is printed as UNRESOLVED, never
    dropped, so it is a listed gap and not a silent absence.
  * ONLY THE BOOT EXECUTABLE is scanned. A loader call made from inside an overlay cannot appear.
  * The scan folds a fixed window of instructions before the call (plus the delay slot). An argument set
    further back than that window, or across a branch, reads as UNRESOLVED.
  * A destination is where the loader is TOLD to put the file. It is not proof the file is code, nor
    that the file on this disc is the one this site loads — the path string is the only link, and it is
    printed so it can be checked.
  * The slot arithmetic assumes the two destinations it compares are CO-RESIDENT. They are here because
    both loads happen in one function on one path (FUN_8003D88C), which the census prints; it is not a
    general property.
"""
import argparse
import glob
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLAT = os.path.join(ROOT, "scratch", "flat")
EXE = os.environ.get("TS2_EXE") or os.path.join(ROOT, "scratch", "bin", "toystory2", "SLUS_008.93")

# The RE'd anchor. NOT derived — see the module docstring.
LOADER = 0x80082508
WINDOW = 24          # instructions before the call that the fold looks back over
A0, A1, SP, GP = 4, 5, 29, 28

OP_SPECIAL, OP_J, OP_JAL, OP_COP0, OP_COP1, OP_COP2 = 0x00, 0x02, 0x03, 0x10, 0x11, 0x12
OP_ADDI, OP_ADDIU, OP_SLTI, OP_SLTIU, OP_ANDI, OP_ORI, OP_XORI, OP_LUI = (
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F)
IMM_DEFINES_RT = {OP_ADDI, OP_ADDIU, OP_SLTI, OP_SLTIU, OP_ANDI, OP_ORI, OP_XORI, OP_LUI}
LOADS = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x30, 0x31, 0x32, 0x33, 0x35, 0x37}
STORES = {0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x38, 0x39, 0x3A, 0x3D, 0x3E, 0x3F}
BRANCHES = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17}
# SPECIAL functs that write NO general register (or write hi/lo only)
SPECIAL_NO_RD = {0x08, 0x0C, 0x0D, 0x0F, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B}

BLIND = [
    "a destination computed at RUN TIME (memory pointer, allocator result, $gp load) is not a literal "
    "in the caller and is printed UNRESOLVED, never dropped",
    "ONLY THE BOOT EXECUTABLE is scanned: a loader call from inside an overlay cannot appear",
    "the fold looks back %d instructions plus the delay slot; an argument set further back, or across a "
    "branch, reads UNRESOLVED" % WINDOW,
    "a destination is where the loader is TOLD to write, not proof the file is code, and the path string "
    "is the only link to a file on this disc",
    "the slot arithmetic assumes its two destinations are CO-RESIDENT — true here because one function "
    "on one path performs both loads, not a general property",
]


def sx16(v):
    return v - 0x10000 if v & 0x8000 else v


def defines(word):
    """Which general register this instruction writes, or None. Returns -1 for 'unknown instruction —
    assume it clobbers everything', because a fold that silently ignores an instruction it cannot read
    is how a stale register value gets reported as an argument."""
    op = word >> 26
    if op == OP_SPECIAL:
        funct = word & 0x3F
        if funct in SPECIAL_NO_RD:
            return None if funct != 0x09 else (word >> 11) & 31       # jalr writes rd
        return (word >> 11) & 31
    if op in IMM_DEFINES_RT or op in LOADS:
        return (word >> 16) & 31
    if op in STORES or op in BRANCHES or op == OP_J:
        return None
    if op == OP_JAL:
        return 31
    if op in (OP_COP0, OP_COP1, OP_COP2):
        rs = (word >> 21) & 31
        return (word >> 16) & 31 if rs in (0x00, 0x02) else None      # mfcZ / cfcZ write rt
    return -1


class Exe:
    def __init__(self, path):
        if not os.path.isfile(path):
            raise SystemExit("REFUSED: no boot executable at %s — nothing to census. Run "
                             "`python3 tools/extract_exe.py` (or set $TS2_EXE)." % path)
        self.raw = open(path, "rb").read()
        if self.raw[:8] != b"PS-X EXE":
            raise SystemExit("REFUSED: %s is not a PS-X EXE" % path)
        self.path = path
        self.t_addr, self.t_size = struct.unpack_from("<II", self.raw, 0x18)
        self.t_end = self.t_addr + self.t_size
        self.words = struct.unpack_from("<%dI" % (self.t_size // 4), self.raw, 0x800)

    def word(self, va):
        return self.words[(va - self.t_addr) // 4]

    def cstr(self, va, limit=64):
        if not (self.t_addr <= va < self.t_end):
            return None
        off = 0x800 + va - self.t_addr
        end = self.raw.find(b"\0", off, off + limit)
        if end < 0:
            return None
        s = self.raw[off:end]
        return s.decode("latin-1") if all(32 <= b < 127 for b in s) else None


def fold_args(exe, site, regs=(A0, A1)):
    """Value of each register in `regs` as the call at `site` sees it. Emulates lui/addiu/ori forward
    over the WINDOW instructions ending at the delay slot. Returns {reg: (value|None, why)}."""
    lo = max(exe.t_addr, site - 4 * WINDOW)
    val, why = {}, {}
    for va in range(lo, site + 8, 4):
        if va == site:
            continue                                      # the jal itself writes only $ra
        w = exe.word(va)
        op = w >> 26
        d = defines(w)
        if d == -1:
            val.clear()
            why = {r: "clobbered by an instruction the fold cannot read at 0x%08X" % va for r in regs}
            continue
        if d in (None, 0):
            continue
        rs, rt, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if op == OP_LUI:
            val[rt], why[rt] = imm << 16, "lui at 0x%08X" % va
        elif op in (OP_ADDIU, OP_ADDI) and rs in val:
            val[rt], why[rt] = val[rs] & 0xFFFFFFFF, "lui+addiu, addiu at 0x%08X" % va  # SABOTAGE
        elif op == OP_ORI and rs in val:
            val[rt], why[rt] = val[rs] | imm, "lui+ori, ori at 0x%08X" % va
        elif op in (OP_ADDIU, OP_ADDI) and rs == SP:
            val.pop(rt, None)
            why[rt] = "STACK: addiu r%d,$sp,%d at 0x%08X" % (rt, sx16(imm), va)
        elif op in LOADS and rs == GP:
            val.pop(rt, None)
            why[rt] = "GP-RELATIVE load at 0x%08X — the value is not in the instruction stream" % va
        else:
            val.pop(rt, None)
            why[rt] = "written at 0x%08X by op 0x%02X, not a foldable constant" % (va, op)
    return {r: (val.get(r), why.get(r, "never written in the %d-instruction window" % WINDOW))
            for r in regs}


def census(exe, loader):
    """(sites, stats). sites = [(site, containing-jal-order, a0, a0str, a1, why0, why1)]."""
    if loader % 4 or not (exe.t_addr <= loader < exe.t_end):
        raise SystemExit("REFUSED: callee 0x%08X is not a word-aligned address inside this executable's "
                         ".text [0x%08X,0x%08X)" % (loader, exe.t_addr, exe.t_end))
    njal = 0
    sites = []
    for i, w in enumerate(exe.words):
        if (w >> 26) != OP_JAL:
            continue
        njal += 1
        if (((w & 0x3FFFFFF) << 2) | 0x80000000) == loader:
            sites.append(exe.t_addr + 4 * i)
    stats = {"words": len(exe.words), "jal": njal, "sites": len(sites)}
    if not sites:
        raise SystemExit("REFUSED: 0 of %d `jal` in this executable target 0x%08X. Either the callee is "
                         "wrong or this is not the measured executable — printing an empty destination "
                         "table here would be a clean false zero." % (njal, loader))
    rows = []
    for s in sites:
        f = fold_args(exe, s)
        a0, w0 = f[A0]
        a1, w1 = f[A1]
        rows.append((s, a0, exe.cstr(a0) if a0 else None, a1, w0, w1))
    stats["a1_literal"] = sum(1 for r in rows if r[3] is not None)
    stats["a0_string"] = sum(1 for r in rows if r[2] is not None)
    return rows, stats


def out_jals(path, t_addr, t_end):
    b = open(path, "rb").read()
    n = len(b) // 4
    if n == 0:
        return b"", []
    ws = struct.unpack_from("<%dI" % n, b, 0)
    return b, [t for t in (((w & 0x3FFFFFF) << 2) | 0x80000000 for w in ws if (w >> 26) in (OP_J, OP_JAL))
               if not (t_addr <= t < t_end)]


def modules():
    """The LEVEL*/LEVEL*.BIN corpus, from the flat extraction. REFUSES on an empty corpus."""
    files = sorted(glob.glob(os.path.join(FLAT, "LEVEL*__LEVEL*.BIN")))
    if not files:
        raise SystemExit("REFUSED: 0 LEVEL*__LEVEL*.BIN in %s — the slot arithmetic would be over an "
                         "empty set. Populate it with `python3 tools/extract_disc_files.py`." % FLAT)
    return files


def total_hits(base, files, t_addr, t_end):
    """(hits, denominator) — how many of these files' out-of-.text absolute jal targets land inside
    [base, base+filesize) for each file. The DENOMINATOR is returned so 0 hits is a measurement."""
    hits = den = 0
    for p in files:
        b, out_t = out_jals(p, t_addr, t_end)
        den += len(out_t)
        hits += sum(1 for t in out_t if base <= t < base + len(b))
    return hits, den


def slot_report(exe, rows, out=sys.stdout):
    """The memory map, derived by INTERSECTING two independent methods — no base is written here by
    hand. Method 1 is the census (a constant in the caller); method 2 is the modules' own absolute jal
    targets (base_fit.py's evidence, at word granularity). The overlay slot is the census destination
    the modules agree with, and a destination NO module agrees with scores 0 — which is what makes the
    choice a measurement rather than a preference.

    Returns (slot_base, next_base, verdict) where verdict in {'one-slot','two-slot','undecided'}."""
    dests = sorted({r[3] for r in rows if r[3] is not None})
    named = {}
    for s, a0, a0s, a1, w0, w1 in rows:
        if a1 is not None and a0s:
            named.setdefault(a1, []).append(a0s)
    files = modules()
    print("== which census destination do the LEVEL*.BIN modules themselves agree with? ==", file=out)
    scored = []
    for dst in dests:
        h, den = total_hits(dst, files, exe.t_addr, exe.t_end)
        scored.append((h, dst, den))
        print("   0x%08X  %4d of %d out-of-.text jal targets land in-module  (%s)"
              % (dst, h, den, ", ".join(named.get(dst, ["path built at run time"]))), file=out)
    scored.sort(reverse=True)
    print("== the overlay slot, and what bounds it ==", file=out)
    if not scored or scored[0][0] == 0:
        print("   UNDECIDABLE: NO census destination catches a single module jal target (%d targets over "
              "%d modules). The two methods do not intersect, so this tool names no slot."
              % (scored[0][2] if scored else 0, len(files)), file=out)
        return None, None, "undecided"
    (h1, slot, den), (h2, runner) = scored[0], (scored[1][0], scored[1][1]) if len(scored) > 1 else (0, 0)
    if h1 <= h2:
        print("   UNDECIDABLE: the best destination 0x%08X (%d) does not beat the runner-up 0x%08X (%d)"
              % (slot, h1, runner, h2), file=out)
        return None, None, "undecided"
    print("   slot base           0x%08X  (%d/%d module targets; runner-up 0x%08X scores %d)"
          % (slot, h1, den, runner, h2), file=out)
    above = [d for d in dests if d > slot]
    if not above:
        print("   the slot is 0x%08X and NOTHING in the census sits above it, so its size is "
              "unbounded by this method" % slot, file=out)
        return slot, None, "undecided"
    nxt = above[0]
    window = nxt - slot
    # WHICH call sites put a file at the slot, and how each one's PATH was determined. Printed per site
    # rather than for the first one, because the slot having several callers is the finding, not noise;
    # and a slot with no site would mean `dests` and `rows` had come apart, so it REFUSES.
    at_slot = [r for r in rows if r[3] == slot]
    if not at_slot:
        raise SystemExit("REFUSED: the slot 0x%08X was scored from the destination set but no call site "
                         "in the census carries it — the two views of `rows` have diverged." % slot)
    print("   slot base           0x%08X  (%d of %d call site(s) load here)"
          % (slot, len(at_slot), len(rows)), file=out)
    for s, a0, a0s, a1, w0, w1 in at_slot:
        print("     site 0x%08X   dest: %-46s path: %s" % (s, w1, ('"%s"' % a0s) if a0s else w0), file=out)
    print("   next fixed load     0x%08X  (%s)" % (nxt, ", ".join(named.get(nxt, ["<no path string>"]))),
          file=out)
    print("   window              %d bytes" % window, file=out)
    files = modules()
    sizes = {os.path.basename(f): os.path.getsize(f) for f in files}
    biggest = max(sizes.items(), key=lambda kv: kv[1])
    print("   largest LEVEL*.BIN  %d bytes (%s)%s" % (biggest[1], biggest[0],
          "  == the window EXACTLY" if biggest[1] == window else "  != the window"), file=out)
    over = {k: v for k, v in sizes.items() if v > window}
    print("   modules larger than the window: %s" % ("NONE (all %d fit)" % len(sizes) if not over
                                                     else ", ".join("%s %d" % kv for kv in over.items())),
          file=out)
    # THE SLOT-COUNT QUESTION. If LEVEL.BIN and LEVEL1.BIN were two slots they would both have to be
    # resident inside the window; print the arithmetic per level rather than a conclusion.
    print("   two-slot test — a level's .BIN parts summed against the window:", file=out)
    pairs, refuted = 0, 0
    for lv in sorted({k.split("__")[0] for k in sizes}):
        parts = {k: v for k, v in sizes.items() if k.startswith(lv + "__")}
        if len(parts) < 2:
            continue
        pairs += 1
        tot = sum(parts.values())
        bad = tot > window
        refuted += bad
        print("     %-8s %-46s sum %6d  %s" % (lv, " + ".join("%s %d" % (k.split("__")[1], v)
              for k, v in sorted(parts.items())), tot,
              "EXCEEDS the window by %d — cannot be co-resident" % (tot - window) if bad else "fits"),
              file=out)
    verdict = "one-slot" if refuted else ("two-slot" if pairs else "undecided")
    print("   verdict: %s — %d of %d multi-part levels cannot hold both parts at once"
          % ("ONE SLOT, alternative contents" if verdict == "one-slot" else
             "not refuted by size: two slots remain possible", refuted, pairs), file=out)
    return slot, nxt, verdict


def score_report(exe, slot, out=sys.stdout):
    """Diff the census-derived base BY CODE against the independent jal-target evidence base_fit uses,
    and against that method's 4 KiB-floored answer. Prints both counts for every module."""
    floor = slot & ~0xFFF
    print("== the census base scored against each module's own absolute jal targets ==", file=out)
    print("   (independent evidence: base_fit.py's method, here at WORD granularity on two fixed bases)",
          file=out)
    print("   %-24s %7s %7s  %s" % ("module", "out-jal", "size", "hits@0x%08X / hits@0x%08X (4 KiB floor)"
                                    % (slot, floor)), file=out)
    better = worse = 0
    rows = []
    for p in modules():
        b, out_t = out_jals(p, exe.t_addr, exe.t_end)
        if not out_t:
            print("   %-24s %7s %7d  NO EVIDENCE (no out-of-.text jal)"
                  % (os.path.basename(p), "0", len(b)), file=out)
            continue
        h1 = sum(1 for t in out_t if slot <= t < slot + len(b))
        h0 = sum(1 for t in out_t if floor <= t < floor + len(b))
        better += h1 > h0
        worse += h1 < h0
        rows.append((os.path.basename(p), len(out_t), len(b), h1, h0))
        print("   %-24s %7d %7d  %4d / %-4d %s" % (os.path.basename(p), len(out_t), len(b), h1, h0,
              "<- census base strictly BETTER" if h1 > h0 else
              ("<- census base WORSE" if h1 < h0 else ("all %d" % len(out_t) if h1 == len(out_t) else ""))),
              file=out)
    print("   %d module(s) score strictly better at the census base, %d worse. A base that were merely "
          "the 4 KiB floor's equal could not be told from it by this evidence." % (better, worse), file=out)
    return better, worse, rows


def report(loader, out=sys.stdout):
    exe = Exe(EXE)
    rows, st = census(exe, loader)
    print("boot exe %s  .text [0x%08X,0x%08X)" % (os.path.basename(exe.path), exe.t_addr, exe.t_end),
          file=out)
    print("== call-site census of callee 0x%08X ==" % loader, file=out)
    print("   %d words examined, %d `jal` seen, %d target this callee; %d of those form a LITERAL a1, "
          "%d resolve a0 to a string" % (st["words"], st["jal"], st["sites"], st["a1_literal"],
                                         st["a0_string"]), file=out)
    print("   %-12s %-34s %-12s %s" % ("site", "a0 (path)", "a1 (dest)", "how a1 was determined"), file=out)
    for s, a0, a0s, a1, w0, w1 in rows:
        a0txt = ('"%s"' % a0s) if a0s else ("0x%08X" % a0 if a0 else "UNRESOLVED")
        print("   0x%08X   %-34s %-12s %s"
              % (s, a0txt[:34], "0x%08X" % a1 if a1 is not None else "UNRESOLVED", w1), file=out)
    if st["a1_literal"] == 0:
        print("   0 of %d call sites form a literal destination: every one is computed at run time, so "
              "this method cannot name a base for this callee." % st["sites"], file=out)
    slot, nxt, verdict = slot_report(exe, rows, out)
    better = worse = 0
    if slot is not None:
        better, worse, _ = score_report(exe, slot, out)
    for b in BLIND:
        print("[blind spot] %s" % b, file=out)
    return {"rows": rows, "stats": st, "slot": slot, "next": nxt, "verdict": verdict,
            "better": better, "worse": worse}


# ---------------------------------------------------------------------------------------------------
# THE GATE. Anchors measured 2026-08-12 and used ONLY here, so the reporting path cannot be biased by
# them. Each is something a BROKEN fold gets wrong in a specific way.
GATE_SLOT = 0x800D12C0          # the overlay slot, from site 0x8003DEAC
GATE_NEXT = 0x800D5D20          # the destination bounding the slot from above, from site 0x8003DB50
# MEASURED, and not what a first reading expects: FOUR sites load 0x800D5D20 and they name TWO files —
# BITS/MEMORY.BIN and FMV/FMV.BIN share this buffer. That is evidence about FMV.BIN's class (RE-04), so
# it is anchored here rather than smoothed into "the MEMORY.BIN destination".
GATE_NEXT_SITES = 4
GATE_NEXT_PATHS = ["bits\\memory.bin", "fmv\\fmv.bin"]
GATE_SITES = 13                 # call sites of 0x80082508
DECOY_LOADER = 0x80082870       # the PATH BUILDER: 1 call site, whose a1 is a stack buffer


def selftest():
    import io
    fails = []

    def ck(name, ok, detail):
        print("[selftest] %-4s %s\n            %s" % ("PASS" if ok else "FAIL", name, detail))
        if not ok:
            fails.append(name)

    exe = Exe(EXE)
    rows, st = census(exe, LOADER)
    dests = {r[3] for r in rows if r[3] is not None}
    ck("POSITIVE: the loader's call sites are found and their destinations fold",
       st["sites"] == GATE_SITES and GATE_SLOT in dests and GATE_NEXT in dests,
       "%d sites (expected %d), %d literal destinations; slot 0x%08X %s, next 0x%08X %s"
       % (st["sites"], GATE_SITES, st["a1_literal"], GATE_SLOT, "present" if GATE_SLOT in dests
          else "MISSING", GATE_NEXT, "present" if GATE_NEXT in dests else "MISSING"))

    # EVERY site loading the bounding destination must resolve a PATH, and the measured set of paths is
    # the anchor — not a count of one. Four sites load 0x800D5D20 and they name TWO different files, so
    # asserting a unique site here would be asserting something the bytes contradict.
    mem = [r for r in rows if r[3] == GATE_NEXT]
    paths = sorted({(r[2] or "").lower() for r in mem})
    ck("POSITIVE: every site loading the bounding destination resolves a PATH, and the path SET is the "
       "measured one",
       len(mem) == GATE_NEXT_SITES and all(r[2] for r in mem) and paths == GATE_NEXT_PATHS,
       "0x%08X <- %d site(s) (expected %d), %d resolve a path; paths %r (expected %r)"
       % (GATE_NEXT, len(mem), GATE_NEXT_SITES, sum(1 for r in mem if r[2]), paths, GATE_NEXT_PATHS))

    # NEGATIVE CONTROL 1: the path BUILDER. Same code path, a callee that is not a loader. Its argument
    # is a stack buffer, so a fold that fabricates constants shows up here as a bogus 0x800Dxxxx.
    drows, dst = census(exe, DECOY_LOADER)
    dbad = [r for r in drows if r[3] is not None and 0x800D0000 <= r[3] < 0x800E0000]
    ck("NEGATIVE control: the path builder's a1 is a STACK buffer and folds to no slot-like literal",
       dst["sites"] == 1 and not dbad,
       "%d site(s); a1 %s; %d slot-like literal(s) fabricated"
       % (dst["sites"], drows[0][5] if drows else "-", len(dbad)))

    # NEGATIVE CONTROL 2: refusals. An address with no callers, a misaligned one, an out-of-range one.
    refused = 0
    for bad in (0x8009F000, 0x80082509, 0x80200000):
        try:
            census(exe, bad)
        except SystemExit:
            refused += 1
    ck("REFUSALS: a callee with 0 call sites, a misaligned one and an out-of-.text one all refuse",
       refused == 3, "%d of 3 refused" % refused)

    # DERIVATION: the slot, the window and the slot-count verdict must come out of the census, and the
    # census base must be DISTINGUISHABLE from base_fit.py's 4 KiB-floored answer by the module
    # evidence. Without that last clause the whole finding would be unfalsifiable decoration.
    buf = io.StringIO()
    slot, nxt, verdict = slot_report(exe, rows, buf)
    better, worse, mrows = score_report(exe, slot, buf) if slot else (0, 0, [])
    ck("DERIVATION: slot, next base and the ONE-SLOT verdict are re-derived from the census",
       slot == GATE_SLOT and nxt == GATE_NEXT and verdict == "one-slot",
       "derived slot 0x%s, next 0x%s, verdict %s" % (("%08X" % slot) if slot else "-",
                                                     ("%08X" % nxt) if nxt else "-", verdict))
    ck("DISCRIMINATION: the census base beats the 4 KiB floor on the modules' own jal targets",
       better >= 1 and worse == 0,
       "%d module(s) strictly better at 0x%08X than at 0x%08X, %d worse"
       % (better, slot or 0, (slot or 0) & ~0xFFF, worse))
    full = [r for r in mrows if r[3] == r[1]]
    ck("DISCRIMINATION: at the census base at least one module goes from partial to 100%",
       any(r[3] == r[1] and r[4] < r[1] for r in mrows),
       "%d of %d scoring modules land ALL their targets at the census base; upgraded from partial: %s"
       % (len(full), len(mrows), ", ".join("%s %d/%d->%d/%d" % (r[0], r[4], r[1], r[3], r[1])
                                          for r in mrows if r[3] == r[1] and r[4] < r[1]) or "NONE"))

    print("[selftest] %d/7 passed" % (7 - len(fails)))
    print("[selftest] what this CANNOT see: whether the loader anchor 0x%08X really is the CD file "
          "loader (that is a decompile, docs/info/claims/008-*), and whether a destination computed at "
          "run time exists that this method never shows." % LOADER)
    for b in BLIND:
        print("[selftest] blind spot: %s" % b)
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description="where the CD loader puts each file, decoded from the exe")
    ap.add_argument("--loader", default=hex(LOADER), help="callee VA to census (default the CD loader)")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        raise SystemExit(selftest())
    report(int(a.loader, 0))


if __name__ == "__main__":
    main()
