#!/usr/bin/env python3
"""ghidra_xref.py — WHO references these guest addresses? A Ghidra postScript with TWO independent
methods and a printed denominator.

  Import tools/ram_image.py's output at KSEG0, then run this over that Ghidra project:
    pyghidraRun -H scratch/ghidra ts2boot -process -noanalysis \\
        -scriptPath tools -postScript ghidra_xref.py <out.txt> <addr-or-range> [more...]
  A range is `lo..hi` (hi exclusive), a single target is a bare hex address. The driver is
  <out.txt> runs the both-classes gate and exits non-zero on regression.
  tools/re_xref.py wraps the invocation so no session retypes it.

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

RAM_BASE = 0x80000000  # the flat image's KSEG0 import base
# A 4 KiB-floored FITTED overlay load base, recovered from module `jal` targets. Held here ONLY as the
# subject of a regression check — it is NOT a resident base and must never be pasted into a GameConfig
# or an overlay table.
OVERLAY_BASE_FIT = 0x800D1000
OP_LUI, OP_ADDIU, OP_ORI = 0x0F, 0x09, 0x0D
LOADS = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26}  # lb lh lwl lw lbu lhu lwr
STORES = {0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x32, 0x3A}  # sb sh swl sw swr lwc2 swc2
MEM_OPS = LOADS | STORES
IMM_RT = {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}  # addi..lui: all define rt
BRANCH_NODEF = {0x01, 0x02, 0x04, 0x05, 0x06, 0x07}  # REGIMM + j + beq/bne/blez/bgtz


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
    question under test, and the tool answered YES by fabrication. An
    unrecognised word therefore kills ALL tracking rather than being ignored: a missed reference is a
    reported blind spot, a fabricated one is a false finding."""
    op = w >> 26
    if op in IMM_RT:
        return {(w >> 16) & 31}
    if op in LOADS or op in (
        0x10,
        0x11,
        0x12,
        0x13,
    ):  # loads, and mfc/cfc from a coprocessor
        return {(w >> 16) & 31}
    if op in STORES or op in BRANCH_NODEF:
        # A store defines no register; nor do j/beq/bne/blez/bgtz. REGIMM's bltzal/bgezal DO write
        # $ra — bit 4 of rt distinguishes them.
        if op == 0x01 and ((w >> 16) & 0x10):
            return {31}
        return set()
    if op == 0x03:  # jal
        return {31}
    if op == 0x00:  # SPECIAL
        funct = w & 0x3F
        if funct in (0x08, 0x0C, 0x0D):  # jr, syscall, break
            return set()
        if funct in (0x10, 0x12):  # mfhi, mflo -> rd
            return {(w >> 11) & 31}
        if funct in (
            0x11,
            0x13,
            0x18,
            0x19,
            0x1A,
            0x1B,
        ):  # mthi/mtlo, mult/multu/div/divu
            return set()
        return {(w >> 11) & 31}  # everything else writes rd (incl. jalr)
    if op in (
        0x30,
        0x31,
        0x32,
        0x33,
        0x34,
        0x35,
        0x36,
        0x37,
        0x38,
        0x39,
        0x3A,
        0x3B,
        0x3C,
        0x3D,
        0x3E,
        0x3F,
    ):
        return set()  # coprocessor load/store: no GPR write
    return None  # not a word we model -> kill everything


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
    st = {"words": 0, "nofold": 0, "luis": 0, "pairs": 0, "killall": 0, "killcall": 0}
    for slo, shi in spans:
        upper = {}  # reg -> (hi16, pc of the lui); never carried across a span boundary
        _fold_span(image, slo, shi, base, upper, refs, st)
    return refs, st


def is_call(w):
    return (w >> 26) == 0x03 or ((w >> 26) == 0x00 and (w & 0x3F) == 0x09)  # jal / jalr


def _fold_span(image, lo, hi, base, upper, refs, st):
    defer = [False]
    for pc in range(lo, hi & ~3, 4):
        _fold_word(
            struct.unpack_from("<I", image, pc - base)[0], pc, upper, refs, st, defer
        )


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
        if (
            was_deferred
        ):  # a lui IN a call's delay slot: it runs, then the callee eats it
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
    return currentProgram  # noqa: F821  (injected by Ghidra)


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
    a = _prog().getAddressFactory().getAddress(f"{addr_off:08x}")
    f = _prog().getFunctionManager().getFunctionContaining(a)
    return (
        f"{f.getName()}@{f.getEntryPoint().getOffset():08X}" if f else "(no function)"
    )


def ghidra_refs(target):
    af = _prog().getAddressFactory()
    rm = _prog().getReferenceManager()
    out = []
    for r in rm.getReferencesTo(af.getAddress(f"{target:08x}")):
        out.append((r.getFromAddress().getOffset(), str(r.getReferenceType())))
    return sorted(out)


def parse_targets(toks):
    ts = []
    for t in toks:
        if ".." in t:
            lo, _, hi = t.partition("..")
            lo, hi = int(lo, 16), int(hi, 16)
            if hi <= lo:
                raise SystemExit(f"[xref] REFUSED: range {t} is empty or reversed")
            ts.extend(range(lo, hi, 4))
        else:
            ts.append(int(t, 16))
    if not ts:
        raise SystemExit(
            "[xref] REFUSED: no targets given — a run with no target would print a "
            "clean report about nothing"
        )
    return ts


BLIND = [
    (
        "$gp-RELATIVE ACCESS IS INVISIBLE to method B (this exe's gp0=0, so $gp is set at run time); "
        "lw rX,off($gp) names an address neither method resolves"
    ),
    (
        "A base held IN MEMORY (pointer, dispatch table, struct field) is invisible to B and visible to "
        "A only where Ghidra resolved it"
    ),
    (
        "ONLY THIS IMAGE: overlay code is absent unless ram_image.py injected it, so a reference from "
        "inside an overlay cannot appear at all"
    ),
    (
        "B UNDER-reports on purpose: a hi16 dies at the first write to its register, at any word B does "
        "not model (which kills ALL tracking), and at a jal/jalr for the o32 caller-saved set. A real "
        "reference whose lui is separated from its use by any of those is INVISIBLE to B. This direction "
        "was chosen after B FABRICATED two references to 0x800D1000 (see defined_reg's docstring)"
    ),
    (
        "B is straight-line only: it does not follow branches, so a hi16 established on one path and "
        "used on another is neither seen nor invalidated correctly"
    ),
    "A hit is a REFERENCE, not a meaning — the decompile that follows is what says what the code does",
]


def run(out_path, targets, image, spans, ninstr, ninstr_out):
    refs, st = fold(image, spans, RAM_BASE)
    lines = []

    def p(s=""):
        lines.append(s)
        print(s)

    span_text = " ".join(f"[0x{lo:08X},0x{hi:08X})" for lo, hi in spans)
    p(
        f"[xref] scanned the {len(spans)} span(s) ram_image.py says this image carries: {span_text}"
    )
    p(
        f"[xref] {st['words']} words examined, {st['nofold']} words not foldable, {st['luis']} lui, "
        f"{st['pairs']} pairs folded -> {len(refs)} distinct addresses"
    )
    p(
        f"[xref] Ghidra has {ninstr} defined instructions, of which {ninstr_out} lie OUTSIDE those "
        "spans (zeros disassemble as nop, so analysis walks past .text — reported, never scanned)"
    )
    p(f"[xref] {len(targets)} target(s)")
    hitfns = set()
    for t in targets:
        g = ghidra_refs(t)
        b = refs.get(t, [])
        p("")
        neither = "   <- NEITHER METHOD SEES A REFERENCE" if not g and not b else ""
        p(
            f"[xref] target 0x{t:08X}: method A (Ghidra refs) {len(g)}, "
            f"method B (folded pairs) {len(b)}{neither}"
        )
        for pc, kind in g:
            p(f"        A  from 0x{pc:08X}  {kind:<14}  in {fn_at(pc)}")
            hitfns.add(fn_at(pc))
        for pc in b:
            mark = (
                ""
                if any(pc == x for x, _ in g)
                else "   (B ONLY — Ghidra missed this reference)"
            )
            p(f"        B  from 0x{pc:08X}  lui+lo pair    in {fn_at(pc)}{mark}")
            hitfns.add(fn_at(pc))
        for pc, kind in g:
            if pc not in b:
                p(
                    f"        A ONLY from 0x{pc:08X} — not a plain lui pair (table read / $gp / computed)"
                )
    p("")
    function_text = " ".join(sorted(hitfns)) if hitfns else "(none)"
    p(f"[xref] {len(hitfns)} distinct containing function(s): {function_text}")
    for s in BLIND:
        p(f"[xref] blind spot: {s}")
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"[xref] wrote {out_path}")
    return 0


def main():
    os.makedirs(os.path.dirname(STATUS), exist_ok=True)
    if os.path.exists(STATUS):
        os.remove(STATUS)
    try:
        args = [tok for a in getScriptArgs() for tok in str(a).split()]  # noqa: F821
        if not args:
            raise SystemExit("REFUSED: usage: <out.txt> <addr|lo..hi> ...")
        img_path = _prog().getExecutablePath()
        if not os.path.isfile(img_path):
            raise SystemExit(
                f"REFUSED: the imported image {img_path!r} is gone — method B cannot run, and "
                "reporting method A alone would silently halve the evidence"
            )
        with open(img_path, "rb") as image_file:
            image = image_file.read()
        spans = load_spans(img_path)
        ninstr, ninstr_out = instr_stats(spans)
        if ninstr == 0:
            raise SystemExit(
                "REFUSED: the program has ZERO defined instructions — import/analysis "
                "did not happen, and every answer here would be a clean false zero"
            )
        rc = run(args[0], parse_targets(args[1:]), image, spans, ninstr, ninstr_out)
        _status(rc, "xref run")
    except SystemExit as e:
        msg = str(e) if not isinstance(e.code, int) else f"exit {e.code}"
        print(f"[xref] {msg}", file=sys.stderr)
        _status(2, msg.replace("\n", " ")[:200])
        rc = 2
    # Ghidra logs even SystemExit(0) as a postscript ERROR and then still returns launcher exit 0.
    # The status file above is the authoritative channel; returning keeps a passing gate visibly clean.
    return rc


def running_under_ghidra():
    """Whether Ghidra injected its script API into this module.

    PyGhidra 3 / Ghidra 12 exposes ``getScriptArgs`` through its script namespace without adding
    ``currentProgram`` to ``globals()``. Probe the API operation this script actually needs; a normal
    Python process raises ``NameError`` here and is told to use tools/re_xref.py.
    """
    try:
        getScriptArgs  # noqa: B018  (injected by Ghidra/PyGhidra)
    except NameError:
        return False
    return True


if running_under_ghidra():
    main()
elif __name__ == "__main__":
    print("ghidra_xref.py runs inside Ghidra; drive it with tools/re_xref.py.", file=sys.stderr)
    sys.exit(2)
