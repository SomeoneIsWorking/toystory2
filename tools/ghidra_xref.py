#!/usr/bin/env python3
"""ghidra_xref.py — WHO references these guest addresses? A Ghidra postScript with TWO independent
methods and a printed denominator.

  external/psxport/tools/decomp.sh is the importer; run this over an imported project:
    pyghidraRun -H scratch/ghidra ts2boot -process -noanalysis \\
        -scriptPath tools -postScript ghidra_xref.py <out.txt> <addr-or-range> [more...]
  A range is `lo..hi` (hi exclusive), a single target is a bare hex address. `--selftest` instead of
  <out.txt> runs the both-classes gate and exits non-zero on regression.
  tools/re_xref.sh wraps the invocation so no session retypes it.

WHY THIS EXISTS. The RE-first rule says find the function, then DECOMPILE it — but on this game a
data-table address is typically formed by a `lui`/`addiu` pair, and Ghidra's Reference DB only
materialises such a reference where its constant propagation succeeded. "Ghidra shows no xref" is
therefore not a measurement, and one confident false negative on this game's central question already
came from a tool that scanned a prefix and reported a clean zero (docs/issues/0002-*). So this script
answers the SAME question twice:

  METHOD A — Ghidra's Reference DB (getReferencesTo). Sees indexed/computed refs Ghidra resolved, and
    reports the containing function, so its hits are directly decompilable.
  METHOD B — an independent per-word fold over the image bytes: every `lui rX,hi` followed (before rX
    is redefined) by an `addiu/ori rY,rX,lo` or by any base+offset load/store on rX. Per WORD, never
    with a whole-.text disassembler pass, because the first undecodable word stops those dead.

A DISAGREEMENT between A and B is the interesting output and is printed as such: B-only means Ghidra
missed the reference; A-only means the reference is not a plain lui pair (a table read, a $gp-relative
load, an addu of two registers) and cannot be found by scanning for constants.

WHAT A NEGATIVE PRINTS — the denominator, every run: the instruction range scanned, words examined,
words that did not decode, `lui`s seen, pairs folded, distinct addresses formed, and then per target
the count from EACH method. "0 of 12,670 folded pairs form 0x800D1000" is a measurement; "no xrefs"
would not be.

BLIND SPOTS, printed every run:
  * $gp-RELATIVE ACCESS IS INVISIBLE to method B. This exe's header carries gp0=0, so $gp is set at
    run time and a `lw rX,off($gp)` names an address neither method can resolve.
  * A base held in memory (a pointer, a jump/dispatch table, a struct field) is invisible to B and
    only visible to A if Ghidra resolved it.
  * ONLY THIS IMAGE. Overlay code is absent unless ram_image.py injected it, so a reference from
    inside an overlay cannot appear at all.
  * B folds a lui forward to the FIRST redefinition of that register only; a compiler that
    interleaved two uses of one lui across a branch can hide the second use.
  * A hit is a REFERENCE, not a meaning. It says an instruction forms the address; what the code
    does with it is the decompile that follows.
"""
import os
import struct
import sys

RAM_BASE = 0x80000000        # the flat image's load base, fixed by decomp.sh's importer
# tools/base_fit.py's fitted overlay load base (claim C003). Held here ONLY as the subject of a
# regression check — it is NOT a resident base and must never be pasted into a GameConfig or an
# overlay table. This tool once claimed the boot exe forms it; the check below is why it cannot again.
OVERLAY_BASE_FIT = 0x800D1000
OP_LUI, OP_ADDIU, OP_ORI = 0x0F, 0x09, 0x0D
LOADS = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26}                    # lb lh lwl lw lbu lhu lwr
STORES = {0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x32, 0x3A}                   # sb sh swl sw swr lwc2 swc2
MEM_OPS = LOADS | STORES
IMM_RT = {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}             # addi..lui: all define rt
BRANCH_NODEF = {0x01, 0x02, 0x04, 0x05, 0x06, 0x07}                   # REGIMM + j + beq/bne/blez/bgtz


def sx16(v):
    return v - 0x10000 if v & 0x8000 else v


def defined_reg(w):
    """Which GPR does this word write? -> a set of register numbers, or None meaning "UNKNOWN, kill
    everything".

    THIS FUNCTION IS THE FIX FOR A MEASURED FALSE POSITIVE, and it is why the fold cannot just track
    lui-then-addiu. Found 2026-08-12 while xref'ing 0x800D1000: the fold reported two references, at
    0x800565E0 and 0x80056790, that DO NOT EXIST. The real code is

        800565D4  lw    $v1, 0x18($sp)      <- $v1 REDEFINED from memory; its old lui hi16 is dead
        800565E0  addiu $s5, $v1, 0x1000    <- the fold paired this with a lui $v1,0x800d from before

    i.e. the earlier version modelled only addiu/ori as register-defining, so any other write left a
    stale hi16 behind and the fold MANUFACTURED an address. On this port that is the worst possible
    failure mode: 0x800D1000 is the fitted overlay base whose existence in the boot exe is the exact
    question under test (docs/info/claims/003-*), and the tool answered YES by fabrication. An
    unrecognised word therefore kills ALL tracking rather than being ignored: a missed reference is a
    reported blind spot, a fabricated one is a false finding."""
    op = w >> 26
    if op in IMM_RT:
        return {(w >> 16) & 31}
    if op in LOADS or op in (0x10, 0x11, 0x12, 0x13):        # loads, and mfc/cfc from a coprocessor
        return {(w >> 16) & 31}
    if op in STORES or op in BRANCH_NODEF:
        # A store defines no register; nor do j/beq/bne/blez/bgtz. REGIMM's bltzal/bgezal DO write
        # $ra — bit 4 of rt distinguishes them.
        if op == 0x01 and ((w >> 16) & 0x10):
            return {31}
        return set()
    if op == 0x03:                                            # jal
        return {31}
    if op == 0x00:                                            # SPECIAL
        funct = w & 0x3F
        if funct in (0x08, 0x0C, 0x0D):                       # jr, syscall, break
            return set()
        if funct in (0x10, 0x12):                             # mfhi, mflo -> rd
            return {(w >> 11) & 31}
        if funct in (0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B):     # mthi/mtlo, mult/multu/div/divu
            return set()
        return {(w >> 11) & 31}                               # everything else writes rd (incl. jalr)
    if op in (0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
              0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F):
        return set()                                          # coprocessor load/store: no GPR write
    return None                                               # not a word we model -> kill everything


# A call does not "define" a register in the decode sense, but o32 lets the CALLEE destroy these, so
# a hi16 held in one of them across a jal/jalr is dead and pairing across the call FABRICATES an
# address exactly the way the stale-$v1 case did. $at is scratch for the assembler for the same
# reason. Callee-saved ($s0-$s7, $sp, $fp, $gp) legitimately survive a call and are NOT killed.
CALLER_SAVED = {1} | {2, 3} | set(range(4, 8)) | set(range(8, 16)) | {24, 25} | {31}


def fold(image, spans, base):
    """-> (refs {addr: [pc,...]}, stats). Per-word; never a whole-range disasm pass. `spans` are the
    guest ranges the image ACTUALLY CARRIES, from ram_image.py's manifest — folding over the zero
    region would manufacture references out of memory nobody loaded."""
    refs = {}
    st = dict(words=0, nofold=0, luis=0, pairs=0, killall=0, killcall=0)
    for slo, shi in spans:
        upper = {}          # reg -> (hi16, pc of the lui); never carried across a span boundary
        _fold_span(image, slo, shi, base, upper, refs, st)
    return refs, st


def is_call(w):
    return (w >> 26) == 0x03 or ((w >> 26) == 0x00 and (w & 0x3F) == 0x09)     # jal / jalr


def _fold_span(image, lo, hi, base, upper, refs, st):
    defer = [False]
    for pc in range(lo, hi & ~3, 4):
        _fold_word(struct.unpack_from("<I", image, pc - base)[0], pc, upper, refs, st, defer)


def _fold_word(w, pc, upper, refs, st, defer):
    """One word. Two orderings here ARE the correctness argument, and getting either backwards costs
    real references or manufactures fake ones:

    1. A word is first USED to form an address against the hi16s live BEFORE it, and only THEN does
       its own definition kill what it overwrites — otherwise `lui rX,hi` / `addiu rX,rX,lo`, the
       commonest form of all, would never fold.
    2. A call's caller-saved kill lands one word LATE, after the DELAY SLOT. On MIPS the word after a
       jal executes before the callee does, and `jal f` / `addiu $a0,$a0,lo` in the delay slot is how
       an argument is passed — killing $a0 at the jal itself would lose exactly the references most
       worth finding."""
    st["words"] += 1
    op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
    was_deferred, defer[0] = defer[0], False
    if op == OP_LUI:
        st["luis"] += 1
        upper[rt] = ((w & 0xFFFF) << 16, pc)
        if was_deferred:            # a lui IN a call's delay slot: it runs, then the callee eats it
            st["killcall"] += 1
            for r in CALLER_SAVED:
                upper.pop(r, None)
        return
    if op in (OP_ADDIU, OP_ORI) and rs in upper:
        v = upper[rs][0] + (sx16(w & 0xFFFF) if op == OP_ADDIU else (w & 0xFFFF))
        st["pairs"] += 1
        refs.setdefault(v & 0xFFFFFFFF, []).append(pc)
    elif op in MEM_OPS and rs in upper:
        st["pairs"] += 1
        refs.setdefault((upper[rs][0] + sx16(w & 0xFFFF)) & 0xFFFFFFFF, []).append(pc)

    d = defined_reg(w)
    if d is None:
        # An unmodelled word may write ANY register. Killing all tracking costs real references and
        # that loss is blind spot 4 — but the alternative is the measured fabrication in
        # defined_reg()'s docstring, and a missed reference is reported while a fabricated one is not.
        st["nofold"] += 1
        st["killall"] += 1
        upper.clear()
        return
    for r in d:
        upper.pop(r, None)
    if was_deferred:
        st["killcall"] += 1
        for r in CALLER_SAVED:
            upper.pop(r, None)
    if is_call(w):
        defer[0] = True


# ------------------------------------------------------------------ Ghidra side
def _prog():
    return currentProgram          # noqa: F821  (injected by Ghidra)


def instr_stats(spans):
    """How many DEFINED instructions Ghidra has, and how many of them lie OUTSIDE the bytes the image
    actually carries. The second number is the interesting one: a run of 0x00000000 disassembles as a
    valid `sll $zero,$zero,0`, so analysis walks off the end of .text and defines thousands of
    fabricated nops. This is reported, never used as the scan range."""
    n = out = 0
    for ins in _prog().getListing().getInstructions(True):
        a = ins.getAddress().getOffset()
        n += 1
        if not any(lo <= a < hi for lo, hi in spans):
            out += 1
    return n, out


def fn_at(addr_off):
    a = _prog().getAddressFactory().getAddress("%08x" % addr_off)
    f = _prog().getFunctionManager().getFunctionContaining(a)
    return "%s@%08X" % (f.getName(), f.getEntryPoint().getOffset()) if f else "(no function)"


def ghidra_refs(target):
    af = _prog().getAddressFactory()
    rm = _prog().getReferenceManager()
    out = []
    for r in rm.getReferencesTo(af.getAddress("%08x" % target)):
        out.append((r.getFromAddress().getOffset(), str(r.getReferenceType())))
    return sorted(out)


def parse_targets(toks):
    ts = []
    for t in toks:
        if ".." in t:
            lo, _, hi = t.partition("..")
            lo, hi = int(lo, 16), int(hi, 16)
            if hi <= lo:
                raise SystemExit("[xref] REFUSED: range %s is empty or reversed" % t)
            ts.extend(range(lo, hi, 4))
        else:
            ts.append(int(t, 16))
    if not ts:
        raise SystemExit("[xref] REFUSED: no targets given — a run with no target would print a "
                         "clean report about nothing")
    return ts


BLIND = [
    "$gp-RELATIVE ACCESS IS INVISIBLE to method B (this exe's gp0=0, so $gp is set at run time); "
    "lw rX,off($gp) names an address neither method resolves",
    "A base held IN MEMORY (pointer, dispatch table, struct field) is invisible to B and visible to "
    "A only where Ghidra resolved it",
    "ONLY THIS IMAGE: overlay code is absent unless ram_image.py injected it, so a reference from "
    "inside an overlay cannot appear at all",
    "B UNDER-reports on purpose: a hi16 dies at the first write to its register, at any word B does "
    "not model (which kills ALL tracking), and at a jal/jalr for the o32 caller-saved set. A real "
    "reference whose lui is separated from its use by any of those is INVISIBLE to B. This direction "
    "was chosen after B FABRICATED two references to 0x800D1000 (see defined_reg's docstring)",
    "B is straight-line only: it does not follow branches, so a hi16 established on one path and "
    "used on another is neither seen nor invalidated correctly",
    "A hit is a REFERENCE, not a meaning — the decompile that follows is what says what the code does",
]


def run(out_path, targets, image, spans, ninstr, ninstr_out):
    refs, st = fold(image, spans, RAM_BASE)
    lines = []

    def p(s=""):
        lines.append(s)
        print(s)

    p("[xref] scanned the %d span(s) ram_image.py says this image carries: %s"
      % (len(spans), " ".join("[0x%08X,0x%08X)" % s for s in spans)))
    p("[xref] %d words examined, %d words not foldable, %d lui, %d pairs folded -> %d distinct "
      "addresses" % (st["words"], st["nofold"], st["luis"], st["pairs"], len(refs)))
    p("[xref] Ghidra has %d defined instructions, of which %d lie OUTSIDE those spans (zeros "
      "disassemble as nop, so analysis walks past .text — reported, never scanned)"
      % (ninstr, ninstr_out))
    p("[xref] %d target(s)" % len(targets))
    hitfns = set()
    for t in targets:
        g = ghidra_refs(t)
        b = refs.get(t, [])
        p("")
        p("[xref] target 0x%08X: method A (Ghidra refs) %d, method B (folded pairs) %d%s"
          % (t, len(g), len(b), "   <- NEITHER METHOD SEES A REFERENCE" if not g and not b else ""))
        for pc, kind in g:
            p("        A  from 0x%08X  %-14s  in %s" % (pc, kind, fn_at(pc)))
            hitfns.add(fn_at(pc))
        for pc in b:
            mark = "" if any(pc == x for x, _ in g) else "   (B ONLY — Ghidra missed this reference)"
            p("        B  from 0x%08X  lui+lo pair    in %s%s" % (pc, fn_at(pc), mark))
            hitfns.add(fn_at(pc))
        for pc, kind in g:
            if pc not in b:
                p("        A ONLY from 0x%08X — not a plain lui pair (table read / $gp / computed)"
                  % pc)
    p("")
    p("[xref] %d distinct containing function(s): %s"
      % (len(hitfns), " ".join(sorted(hitfns)) if hitfns else "(none)"))
    for s in BLIND:
        p("[xref] blind spot: %s" % s)
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print("[xref] wrote %s" % out_path)
    return 0


# ------------------------------------------------------------------ method B's own gate, no Ghidra
# Every word below is TRANSCRIBED FROM THE REAL IMAGE at the VA in its comment, and check (0) diffs
# the transcription against those bytes BY CODE. A hand-copied vector that drifted from the code it
# claims to model would be a selftest about nothing.
VECTORS = [
    # (name, [(va_or_None, word)], target, must_form, why)
    ("FABRICATION, the measured one: a hi16 carried across two lw's that redefine the register",
     [(None,        0x3C03800D),   # lui   $v1, 0x800d   (synthetic: the stale hi16's establishment)
      (0x800565C0,  0x8C82001C),   # lw    $v0, 0x1c($a0)
      (0x800565C4,  0x8F030008),   # lw    $v1, 8($t8)      <- $v1 redefined; hi16 now DEAD
      (0x800565C8,  0x2451FFFF),   # addiu $s1, $v0, -1
      (0x800565CC,  0xAC830008),   # sw    $v1, 8($a0)
      (0x800565D0,  0x8FA20010),   # lw    $v0, 0x10($sp)
      (0x800565D4,  0x8FA30018),   # lw    $v1, 0x18($sp)   <- redefined again
      (0x800565D8,  0x24561000),   # addiu $s6, $v0, 0x1000
      (0x800565DC,  0x06200060),   # bltz  $s1, 0x80056760
      (0x800565E0,  0x24751000)],  # addiu $s5, $v1, 0x1000 <- the fabricated "reference"
     0x800D1000, False,
     "this is the exact sequence the old fold reported as a reference to the fitted overlay base"),

    ("a genuine lui/addiu pair still folds",
     [(0x80056774,  0x3C07800D),   # lui   $a3, 0x800d
      (0x80056778,  0x24E71240)],  # addiu $a3, $a3, 0x1240
     0x800D1240, True, "the commonest form; killing the destination must happen AFTER the fold"),

    ("a genuine lui + base+offset load still folds",
     [(0x80056784,  0x3C04800D),   # lui   $a0, 0x800d
      (0x80056788,  0x84841242)],  # lh    $a0, 0x1242($a0)
     0x800D1242, True, "the load form, which is how a table entry is read"),

    ("an UNMODELLED word kills all tracking",
     [(None, 0x3C07800D),          # lui   $a3, 0x800d
      (None, 0x74000000),          # (opcode 0x1D: not an R3000A opcode we model)
      (None, 0x24E71240)],         # addiu $a3, $a3, 0x1240
     0x800D1240, False, "an unknown word may write any register, so nothing may survive it"),

    ("a hi16 in a CALLER-SAVED register does not survive a call",
     [(None, 0x3C07800D),          # lui   $a3, 0x800d
      (None, 0x0C020000),          # jal   0x80080000
      (None, 0x00000000),          # nop            (the delay slot)
      (None, 0x24E71240)],         # addiu $a3, $a3, 0x1240
     0x800D1240, False, "o32 lets the callee destroy $a3"),

    ("...but it DOES survive in the call's DELAY SLOT, which runs before the callee",
     [(None, 0x3C07800D),          # lui   $a3, 0x800d
      (None, 0x0C020000),          # jal   0x80080000
      (None, 0x24E71240)],         # addiu $a3, $a3, 0x1240   <- delay slot: executes first
     0x800D1240, True, "jal + addiu in the delay slot is how an argument is passed; the most "
                       "valuable references there are would be lost by killing at the jal"),

    ("a hi16 in a CALLEE-SAVED register survives a call",
     [(None, 0x3C10800D),          # lui   $s0, 0x800d
      (None, 0x0C020000),          # jal   0x80080000
      (None, 0x00000000),          # nop
      (None, 0x26101240)],         # addiu $s0, $s0, 0x1240
     0x800D1240, True, "o32 requires the callee to preserve $s0 — killing it would lose real refs"),
]


def fold_words(words, base=0x80010000):
    """Run the fold over a synthetic instruction sequence. Same code path as the image scan."""
    img = b"".join(struct.pack("<I", w) for w in words)
    return fold(img, [(base, base + 4 * len(words))], base)


def fold_selftest(image, spans):
    """method B's gate. Runs WITHOUT Ghidra, because method B is pure Python and a gate that needs a
    12 GB decompiler to run is a gate nobody runs. Both classes, and both DIRECTIONS: three vectors
    must form their address and four must not, so a fold that answered 'no' to everything — the easy
    way to make a false-positive report go away — fails here just as loudly as the fabrication did."""
    print("[fold-selftest] plan: %d checks — (0) every vector word is diffed BY CODE against the real "
          "image at the VA in its comment; (1..%d) each vector must form / must NOT form its target; "
          "(%d) the fold is non-trivial over the real image; (%d) the FABRICATED reference to the "
          "fitted overlay base 0x800D1000 is absent from the real image's fold."
          % (len(VECTORS) + 3, len(VECTORS), len(VECTORS) + 1, len(VECTORS) + 2))
    fails = []

    def ck(name, ok, detail=""):
        print("[fold-selftest] %s  %s%s"
              % ("PASS" if ok else "FAIL", name, ("   (%s)" % detail) if detail else ""))
        if not ok:
            fails.append(name)

    # (0) the vectors ARE the image's bytes.
    drift = []
    n_tr = 0
    for name, seq, _t, _m, _w in VECTORS:
        for va, w in seq:
            if va is None:
                continue
            n_tr += 1
            got = struct.unpack_from("<I", image, va - RAM_BASE)[0]
            if got != w:
                drift.append("0x%08X: vector 0x%08X != image 0x%08X" % (va, w, got))
    ck("every transcribed vector word matches the real image", not drift,
       "; ".join(drift) if drift else "%d of %d vector words carry a VA and all %d match"
       % (n_tr, sum(len(s) for _n, s, _t, _m, _w in VECTORS), n_tr))

    for name, seq, target, must, why in VECTORS:
        refs, _st = fold_words([w for _va, w in seq])
        formed = target in refs
        ck("%s%s must %sform 0x%08X" % ("" if must else "NEGATIVE: ", name,
                                        "" if must else "NOT ", target),
           formed == must,
           "%s — %s" % ("formed at " + " ".join("0x%08X" % p for p in refs[target])
                        if formed else "not formed", why))

    refs, st = fold(image, spans, RAM_BASE)
    ck("the fold is non-trivial over the real image", st["pairs"] > 1000 and len(refs) > 100,
       "%d words, %d lui, %d pairs -> %d distinct addresses; %d words unmodelled (killed all "
       "tracking), %d call-kills" % (st["words"], st["luis"], st["pairs"], len(refs), st["killall"],
                                     st["killcall"]))

    # The regression itself, over the REAL bytes rather than a vector. This is the check that would
    # have caught the fabricated report, and its DENOMINATOR is the pair count above: "0 of N".
    base_pcs = refs.get(OVERLAY_BASE_FIT, [])
    ck("NEGATIVE: the fitted overlay base 0x%08X is formed by NOTHING in this image"
       % OVERLAY_BASE_FIT, not base_pcs,
       ("STILL FABRICATED at " + " ".join("0x%08X" % p for p in base_pcs)) if base_pcs else
       "0 of %d folded pairs form it (claim C003 stands: the boot exe does not build this constant "
       "as a lui+lo pair)" % st["pairs"])

    print("[fold-selftest] %d/%d passed" % (len(VECTORS) + 3 - len(fails), len(VECTORS) + 3))
    print("[fold-selftest] what this CANNOT see: method A (Ghidra's reference DB) at all — run "
          "`tools/re_xref.sh --selftest` for the cross-validating gate. And it cannot see whether a "
          "TRUE reference to any target is of an invisible kind:")
    for s in BLIND:
        print("[fold-selftest] blind spot: %s" % s)
    return 1 if fails else 0


def selftest(image, spans, ninstr, ninstr_out):
    """Both classes, and the two methods cross-validate each other. A method that can only say 'yes'
    is worthless here: the whole question this script gets asked is whether a zero is real."""
    lo = min(s[0] for s in spans)
    hi = max(s[1] for s in spans)
    print("[selftest] plan: method B's own gate (%d checks, below) and then 5 CROSS-METHOD checks — "
          "(1) the fold is non-trivial over the real image; (2) a POSITIVE " % (len(VECTORS) + 3))
    b_rc = fold_selftest(image, spans)
    print("[selftest] plan (cont): 5 checks — (1) the fold is non-trivial over the real image; (2) a POSITIVE "
          "control: some address is formed by a lui pair AND Ghidra agrees a reference exists there; "
          "(3) a NEGATIVE control: an address in the never-written zero region is formed by NOTHING; "
          "(4) refusals: an empty range, a reversed range and an empty target list; (5) the scan range "
          "came from the MANIFEST, not from Ghidra's defined instructions (which run past .text).")
    fails = []

    def ck(name, ok, detail=""):
        print("[selftest] %s  %s%s" % ("PASS" if ok else "FAIL", name, ("   (%s)" % detail) if detail else ""))
        if not ok:
            fails.append(name)

    refs, st = fold(image, spans, RAM_BASE)
    ck("the fold is non-trivial over the real image", st["pairs"] > 1000 and len(refs) > 100,
       "%d pairs -> %d distinct addresses over %d words" % (st["pairs"], len(refs), st["words"]))

    # POSITIVE: pick a folded address that lies INSIDE the instruction range and that Ghidra also
    # has a reference to. Cross-validation, not self-agreement: A and B are computed independently.
    pos = None
    for a in sorted(refs):
        if lo <= a < hi and ghidra_refs(a):
            pos = a
            break
    ck("positive control: an address both methods independently see", pos is not None,
       ("0x%08X: B %d pair(s), A %d ref(s)" % (pos, len(refs[pos]), len(ghidra_refs(pos))))
       if pos else "NO address is seen by both methods — one of them is broken")

    # NEGATIVE: the zero region above the loaded image. Nothing can legitimately form an address that
    # is not in the fold, so a non-empty answer here means the fold is fabricating.
    neg = [a for a in (0x801F0000, 0x801F4000, 0x801F8000) if a in refs]
    ck("negative control: 3 addresses in the untouched high region are formed by NOTHING",
       not neg, "unexpectedly formed: " + " ".join("0x%08X" % a for a in neg) if neg else "0 of 3 formed")

    refused = 0
    for bad in (["8000..8000"], ["80010004..80010000"], []):
        try:
            parse_targets(bad)
        except SystemExit:
            refused += 1
    ck("refusals: an empty range, a reversed range and an empty target list", refused == 3,
       "%d of 3 refused" % refused)

    # The scan range must be the MANIFEST's, and the manifest must be STRICTLY SMALLER than what
    # Ghidra defines — if it were not, this check could not tell the two apart and the whole reason
    # the manifest exists would be untested.
    ck("the scan range is the manifest's, and Ghidra's defined instructions really do run past it",
       st["words"] * 4 == sum(h - l for l, h in spans) and ninstr_out > 0,
       "manifest %d words vs %d Ghidra instructions, %d of them outside the placed spans"
       % (st["words"], ninstr, ninstr_out))

    print("[selftest] %d/5 cross-method checks passed; method B's own gate returned %d"
          % (5 - len(fails), b_rc))
    print("[selftest] what this CANNOT see: whether a target's TRUE reference is one of the invisible "
          "kinds (blind spots below) — no selftest can, which is why they print on every run.")
    for s in BLIND:
        print("[selftest] blind spot: %s" % s)
    return 1 if (fails or b_rc) else 0


def load_spans(img_path):
    """The placed extent, from ram_image.py's manifest. REFUSES rather than falling back: the obvious
    fallback (Ghidra's defined instructions) is measurably WRONG here — zeros disassemble as nop, so it
    over-reports by ~45%, and a silent fallback would fold over memory nobody ever loaded."""
    import json
    man_path = img_path + ".placements.json"
    if not os.path.isfile(man_path):
        raise SystemExit("[xref] REFUSED: no placement manifest beside the image (%s). Rebuild the "
                         "image with `python3 tools/ram_image.py` — this script will not guess the "
                         "extent from Ghidra's disassembly, which runs past .text into zeros."
                         % man_path)
    man = json.load(open(man_path))
    spans = sorted((p["lo"], p["hi"]) for p in man["placements"])
    if not spans:
        raise SystemExit("[xref] REFUSED: the manifest lists ZERO placements — there is nothing to scan")
    return spans


STATUS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                      "scratch", "logs", "ghidra-xref.status")


def _status(rc, why):
    """Ghidra headless EXITS 0 whatever a postScript does — `sys.exit(1)` here surfaces as a logged
    SystemExit and a green shell. So the real verdict is written here and tools/re_xref.sh exits on
    it; a run that dies before writing this leaves the previous verdict removed, never stale."""
    with open(STATUS, "w") as f:
        f.write("%d %s\n" % (rc, why))


def main():
    os.makedirs(os.path.dirname(STATUS), exist_ok=True)
    if os.path.exists(STATUS):
        os.remove(STATUS)
    try:
        args = [tok for a in getScriptArgs() for tok in str(a).split()]      # noqa: F821
        if not args:
            raise SystemExit("REFUSED: usage: <out.txt|--selftest> <addr|lo..hi> ...")
        img_path = _prog().getExecutablePath()
        if not os.path.isfile(img_path):
            raise SystemExit("REFUSED: the imported image %r is gone — method B cannot run, and "
                             "reporting method A alone would silently halve the evidence" % img_path)
        image = open(img_path, "rb").read()
        spans = load_spans(img_path)
        ninstr, ninstr_out = instr_stats(spans)
        if ninstr == 0:
            raise SystemExit("REFUSED: the program has ZERO defined instructions — import/analysis "
                             "did not happen, and every answer here would be a clean false zero")
        if args[0] == "--selftest":
            rc = selftest(image, spans, ninstr, ninstr_out)
            _status(rc, "selftest")
        else:
            rc = run(args[0], parse_targets(args[1:]), image, spans, ninstr, ninstr_out)
            _status(rc, "xref run")
    except SystemExit as e:
        msg = str(e) if not isinstance(e.code, int) else "exit %s" % e.code
        print("[xref] %s" % msg, file=sys.stderr)
        _status(2, msg.replace("\n", " ")[:200])
        rc = 2
    sys.exit(rc)


DEFAULT_IMAGE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                             "scratch", "ghidra", "ram-boot.bin")


def standalone_main(argv):
    """`python3 tools/ghidra_xref.py --selftest` — method B alone, no Ghidra. This exists because
    method B is the half that FABRICATED a reference, and a gate that needs a decompiler installed is
    a gate that does not run in CI or in a hurry. It REFUSES anything else: an xref run without
    method A would report half the evidence as if it were all of it."""
    if argv[1:2] != ["--selftest"]:
        print("ghidra_xref.py: standalone mode runs ONLY `--selftest` (method B, the pure-Python "
              "fold). An xref run needs Ghidra's reference DB for method A — use tools/re_xref.sh. "
              "Reporting method B alone would silently halve the evidence.", file=sys.stderr)
        return 2
    img = os.environ.get("TS2_RAM_IMAGE", DEFAULT_IMAGE)
    if not os.path.isfile(img):
        print("[fold-selftest] REFUSED: no RAM image at %s. Build it with `python3 "
              "tools/ram_image.py` (it needs the disc; see .env.example). Refusing rather than "
              "running the %d synthetic vectors alone, which would certify nothing about this game."
              % (img, len(VECTORS)), file=sys.stderr)
        return 2
    return fold_selftest(open(img, "rb").read(), load_spans(img))


if "currentProgram" in globals():          # injected by Ghidra/PyGhidra; the postScript path
    main()
elif __name__ == "__main__":
    sys.exit(standalone_main(sys.argv))
