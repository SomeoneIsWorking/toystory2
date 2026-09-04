#!/usr/bin/env python3
"""overlay_map.py — WHERE does the CD loader put each file? Decoded out of the boot executable.

THE QUESTION, and why this tool and not base_fit.py. `tools/base_fit.py` measures where a module's own
absolute `jal` targets say it COULD be loaded. That is a fit, and a fit has two limits this tool does
not: it is 4 KiB-granular, and it can never say HOW the loader arrives at a base. This tool answers the
mechanism instead, from the one place the answer is unambiguous — the loader's own call sites:

    lui  a1, 0x800D          # 0x8003DEA4
    addiu a1, a1, 0x12C0     # 0x8003DEA8   -> a1 = 0x800D12C0
    jal  0x80082508          # 0x8003DEAC   FileLoadTo(path, dest)

so the DESTINATION is a compile-time constant in the caller, and every file the game loads to a fixed
address is in the table this prints. A fit that disagrees with this is the fit being coarse. The one
RE anchor is `--loader`, which defaults to
0x80082508, the two-argument CD file loader, identified by Ghidra call-flow analysis and independently
checked instruction chains (C010/I009).
Everything else is computed from the bytes. `--selftest` also drives a different callee as a negative.

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
import re
import struct
import sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLAT = os.path.join(ROOT, "scratch", "flat")
EXE = os.environ.get("TS2_EXE") or os.path.join(
    ROOT, "scratch", "bin", "toystory2", "SLUS_008.93"
)
CONFIG = os.path.join(ROOT, "game", "core", "game_config.cpp")
MEMORY = os.path.join(FLAT, "BITS__MEMORY.BIN")
FMV = os.path.join(FLAT, "FMV__FMV.BIN")

# The RE'd anchor. NOT derived — see the module docstring.
LOADER = 0x80082508
LEVEL_CALLER = 0x8003D88C
LEVEL_CALLER_END = 0x8003DE9C
LEVEL_PATH_BUILDER = 0x80082870
LEVEL_LOAD_WRAPPER = 0x8003DE9C
LEVEL_PATH_CALL = 0x8003DCC0
LEVEL_WRAPPER_CALL = 0x8003DCCC
LEVEL_LOADER_CALL = 0x8003DEAC
MEMORY_LOADER_CALL = 0x8003DB50
FMV_LOADER_CALL = 0x8003EEAC
FMV_ENTRY_CALL = 0x8003EEC4
FMV_ENTRY = 0x800D6628
LEVEL_PATTERN = r"LEVEL(?:0[1-9]|10)__LEVEL[0-3]?"
WINDOW = 24  # instructions before the call that the fold looks back over
A0, A1, SP, GP = 4, 5, 29, 28

OP_SPECIAL, OP_J, OP_JAL, OP_COP0, OP_COP1, OP_COP2 = 0x00, 0x02, 0x03, 0x10, 0x11, 0x12
OP_ADDI, OP_ADDIU, OP_SLTI, OP_SLTIU, OP_ANDI, OP_ORI, OP_XORI, OP_LUI = (
    0x08,
    0x09,
    0x0A,
    0x0B,
    0x0C,
    0x0D,
    0x0E,
    0x0F,
)
IMM_DEFINES_RT = {
    OP_ADDI,
    OP_ADDIU,
    OP_SLTI,
    OP_SLTIU,
    OP_ANDI,
    OP_ORI,
    OP_XORI,
    OP_LUI,
}
LOADS = {
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x30,
    0x31,
    0x32,
    0x33,
    0x35,
    0x37,
}
STORES = {0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x38, 0x39, 0x3A, 0x3D, 0x3E, 0x3F}
BRANCHES = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17}
# SPECIAL functs that write NO general register (or write hi/lo only)
SPECIAL_NO_RD = {0x08, 0x0C, 0x0D, 0x0F, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B}

BLIND = [
    (
        "a destination computed at RUN TIME (memory pointer, allocator result, $gp load) is not a literal "
        "in the caller and is printed UNRESOLVED, never dropped"
    ),
    "ONLY THE BOOT EXECUTABLE is scanned: a loader call from inside an overlay cannot appear",
    "the fold looks back %d instructions plus the delay slot; an argument set further back, or across a "
    "branch, reads UNRESOLVED" % WINDOW,
    (
        "a destination is where the loader is TOLD to write, not proof the file is code, and the path string "
        "is the only link to a file on this disc"
    ),
    (
        "the slot arithmetic assumes its two destinations are CO-RESIDENT — true here because one function "
        "on one path performs both loads, not a general property"
    ),
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
            return None if funct != 0x09 else (word >> 11) & 31  # jalr writes rd
        return (word >> 11) & 31
    if op in IMM_DEFINES_RT or op in LOADS:
        return (word >> 16) & 31
    if op in STORES or op in BRANCHES or op == OP_J:
        return None
    if op == OP_JAL:
        return 31
    if op in (OP_COP0, OP_COP1, OP_COP2):
        rs = (word >> 21) & 31
        return (word >> 16) & 31 if rs in (0x00, 0x02) else None  # mfcZ / cfcZ write rt
    return -1


class Exe:
    def __init__(self, path, raw=None):
        if not os.path.isfile(path):
            raise SystemExit(
                f"REFUSED: no boot executable at {path} — nothing to census. Run "
                "`python3 tools/extract_exe.py` (or set $TS2_EXE)."
            )
        self.raw = open(path, "rb").read() if raw is None else raw
        if self.raw[:8] != b"PS-X EXE":
            raise SystemExit(f"REFUSED: {path} is not a PS-X EXE")
        self.path = path
        self.t_addr, self.t_size = struct.unpack_from("<II", self.raw, 0x18)
        self.t_end = self.t_addr + self.t_size
        self.words = struct.unpack_from("<%dI" % (self.t_size // 4), self.raw, 0x800)

    def patched(self, replacements):
        """Return an in-memory mutation of this verified executable.

        Self-tests use this to force the opposite slot-count answer through the same census and
        derivation path. No retail-derived bytes are written to disk.
        """
        raw = bytearray(self.raw)
        for va, word in replacements.items():
            if va % 4 or not (self.t_addr <= va < self.t_end):
                raise ValueError(f"patch address 0x{va:08X} is outside .text")
            struct.pack_into("<I", raw, 0x800 + va - self.t_addr, word)
        return Exe(self.path, bytes(raw))

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
            continue  # the jal itself writes only $ra
        w = exe.word(va)
        op = w >> 26
        d = defines(w)
        if d == -1:
            val.clear()
            why = {
                r: f"clobbered by an instruction the fold cannot read at 0x{va:08X}"
                for r in regs
            }
            continue
        if d in (None, 0):
            continue
        rs, rt, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if op == OP_LUI:
            val[rt], why[rt] = imm << 16, f"lui at 0x{va:08X}"
        elif op in (OP_ADDIU, OP_ADDI) and rs in val:
            # The immediate is SIGN-EXTENDED and ADDED. Dropping it (`val[rs]` alone) is not a small
            # error: every destination then folds to its own `lui` page base, so 0x800D12C0 reads as
            # 0x800D0000 and 0x800D5D20 vanishes entirely — which silently collapses the slot
            # derivation to a page boundary AND makes the discrimination check compare that boundary
            # against the 4 KiB floor, i.e. against itself. Every path string degrades the same way,
            # to whatever byte sits at the page base. Measured 2026-08-13.
            val[rt], why[rt] = (
                (val[rs] + sx16(imm)) & 0xFFFFFFFF,
                f"lui+addiu, addiu at 0x{va:08X}",
            )
        elif op == OP_ORI and rs in val:
            val[rt], why[rt] = val[rs] | imm, f"lui+ori, ori at 0x{va:08X}"
        elif op in (OP_ADDIU, OP_ADDI) and rs == SP:
            val.pop(rt, None)
            why[rt] = "STACK: addiu r%d,$sp,%d at 0x%08X" % (rt, sx16(imm), va)
        elif op in LOADS and rs == GP:
            val.pop(rt, None)
            why[rt] = (
                f"GP-RELATIVE load at 0x{va:08X} — the value is not in the instruction stream"
            )
        else:
            val.pop(rt, None)
            why[rt] = f"written at 0x{va:08X} by op 0x{op:02X}, not a foldable constant"
    return {
        r: (
            val.get(r),
            why.get(r, "never written in the %d-instruction window" % WINDOW),
        )
        for r in regs
    }


def census(exe, loader):
    """(sites, stats). sites = [(site, containing-jal-order, a0, a0str, a1, why0, why1)]."""
    if loader % 4 or not (exe.t_addr <= loader < exe.t_end):
        raise SystemExit(
            f"REFUSED: callee 0x{loader:08X} is not a word-aligned address inside this executable's "
            f".text [0x{exe.t_addr:08X},0x{exe.t_end:08X})"
        )
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
        raise SystemExit(
            "REFUSED: 0 of %d `jal` in this executable target 0x%08X. Either the callee is "
            "wrong or this is not the measured executable — printing an empty destination "
            "table here would be a clean false zero." % (njal, loader)
        )
    rows = []
    for s in sites:
        f = fold_args(exe, s)
        a0, w0 = f[A0]
        a1, w1 = f[A1]
        rows.append((s, a0, exe.cstr(a0) if a0 else None, a1, w0, w1))
    stats["a1_literal"] = sum(1 for r in rows if r[3] is not None)
    stats["a0_string"] = sum(1 for r in rows if r[2] is not None)
    return rows, stats


def jal_target(exe, va):
    word = exe.word(va)
    if word >> 26 != OP_JAL:
        raise ValueError(f"0x{va:08X} is 0x{word:08X}, not jal")
    return ((va + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)


def require_call(exe, site, target, meaning):
    actual = jal_target(exe, site)
    if actual != target:
        raise ValueError(
            f"{meaning}: 0x{site:08X} calls 0x{actual:08X}, expected 0x{target:08X}"
        )
    return f"0x{site:08X}: {exe.word(site):08X}  jal 0x{actual:08X}"


def require_words(exe, expected, meaning):
    """Require exact instructions for a semantic chain and return printable evidence."""
    evidence = []
    for va, word in expected.items():
        actual = exe.word(va)
        if actual != word:
            raise ValueError(
                f"{meaning}: instruction at 0x{va:08X} changed: {actual:08X} != {word:08X}"
            )
        evidence.append(f"0x{va:08X}: {actual:08X}")
    return evidence


def formed_constant(exe, hi_va, lo_va, reg):
    hi, lo = exe.word(hi_va), exe.word(lo_va)
    if hi >> 26 != OP_LUI or ((hi >> 16) & 31) != reg:
        raise ValueError("0x%08X does not load the high half of r%d" % (hi_va, reg))
    if (
        lo >> 26 not in (OP_ADDI, OP_ADDIU)
        or ((lo >> 21) & 31) != reg
        or ((lo >> 16) & 31) != reg
    ):
        raise ValueError("0x%08X does not add the low half of r%d" % (lo_va, reg))
    value = (((hi & 0xFFFF) << 16) + sx16(lo & 0xFFFF)) & 0xFFFFFFFF
    return value, [
        "0x%08X: %08X  lui r%d,0x%04X" % (hi_va, hi, reg, hi & 0xFFFF),
        "0x%08X: %08X  addiu r%d,r%d,%d" % (lo_va, lo, reg, reg, sx16(lo & 0xFFFF)),
    ]


def count_calls(exe, lo, hi, target):
    return sum(
        1
        for va in range(lo, hi, 4)
        if exe.word(va) >> 26 == OP_JAL and jal_target(exe, va) == target
    )


def memory_prefix_pointers(data):
    """Consecutive KSEG0 words immediately after MEMORY.BIN's first count word.

    Do not call the count a function count: the retail bytes do not prove that meaning. The useful
    independent fact is narrower: an address prefix exists, and the loader-derived base maps every
    member back inside the same loaded file.
    """
    pointers = []
    for off in range(4, len(data) & ~3, 4):
        word = struct.unpack_from("<I", data, off)[0]
        if not (0x80000000 <= word < 0x80200000):
            break
        pointers.append((off, word))
    return pointers


def loader_contract(exe, rows, slot, next_base, out=sys.stdout):
    """Prove the fixed-slot, MEMORY.BIN, and FMV.BIN contracts from retail bytes."""
    evidence = []
    evidence.append(
        require_call(exe, LEVEL_PATH_CALL, LEVEL_PATH_BUILDER, "level path selection")
    )
    evidence.append(
        require_call(exe, LEVEL_WRAPPER_CALL, LEVEL_LOAD_WRAPPER, "level load wrapper")
    )
    evidence.append(
        require_call(exe, LEVEL_LOADER_CALL, LOADER, "fixed-destination level load")
    )
    evidence.append(require_call(exe, MEMORY_LOADER_CALL, LOADER, "MEMORY.BIN load"))
    evidence.append(require_call(exe, FMV_LOADER_CALL, LOADER, "FMV.BIN load"))
    evidence.append(require_call(exe, FMV_ENTRY_CALL, FMV_ENTRY, "FMV.BIN entry"))
    if count_calls(exe, LEVEL_CALLER, LEVEL_CALLER_END, LEVEL_PATH_BUILDER) != 1:
        raise ValueError("level caller does not invoke the path builder exactly once")
    if count_calls(exe, LEVEL_CALLER, LEVEL_CALLER_END, LEVEL_LOAD_WRAPPER) != 1:
        raise ValueError(
            "level caller does not invoke the fixed-slot wrapper exactly once"
        )

    selected = []
    for selector, hi, lo in (
        (1, 0x8008296C, 0x80082970),
        (2, 0x8008297C, 0x80082980),
        (3, 0x8008298C, 0x80082990),
        (0, 0x8008299C, 0x800829A0),
    ):
        address, lines = formed_constant(exe, hi, lo, A1)
        name = exe.cstr(address)
        if name is None:
            raise ValueError(
                "selector %d forms 0x%08X, which is not a string" % (selector, address)
            )
        selected.append((selector, address, name, lines))
    expected_names = {0: "level.bin", 1: "level1.bin", 2: "level2.bin", 3: "level3.bin"}
    if {selector: name.lower() for selector, _, name, _ in selected} != expected_names:
        raise ValueError("level selector names disagree with the retail four-way table")

    overlay_rows = [row for row in rows if row[0] == LEVEL_LOADER_CALL]
    memory_rows = [row for row in rows if row[0] == MEMORY_LOADER_CALL]
    fmv_rows = [row for row in rows if row[0] == FMV_LOADER_CALL]
    if len(overlay_rows) != 1 or overlay_rows[0][3] != slot:
        raise ValueError(
            "fixed-slot wrapper did not pass the derived level slot to the loader"
        )
    if (
        len(memory_rows) != 1
        or memory_rows[0][2] != "bits\\memory.bin"
        or memory_rows[0][3] != next_base
    ):
        raise ValueError(
            "MEMORY.BIN call did not pass the derived next slot to the loader"
        )
    if (
        len(fmv_rows) != 1
        or fmv_rows[0][2] != "fmv\\fmv.bin"
        or fmv_rows[0][3] != next_base
    ):
        raise ValueError("FMV.BIN call did not pass the shared slot to the loader")

    # There is a real path through both loads: DAT_800A16A8 == 0 takes the MEMORY branch, and flags
    # == 0 falls through the independent bit-1 test into the one level load. Exact words are checked
    # so this is call-flow evidence, not an inference from address order.
    required_words = {
        0x8003DB30: 0x10600003,  # beq v1,zero,0x8003DB40 -> MEMORY.BIN
        0x8003DC40: 0x30620002,  # andi v0,v1,2
        0x8003DC44: 0x14400023,  # bne v0,zero,0x8003DCD4; zero falls through to level load
    }
    for va, expected in required_words.items():
        if exe.word(va) != expected:
            raise ValueError(
                f"co-residency control at 0x{va:08X} changed: {exe.word(va):08X} != {expected:08X}"
            )

    memory_data = open(MEMORY, "rb").read() if os.path.isfile(MEMORY) else None
    if memory_data is None:
        raise SystemExit(
            f"REFUSED: no {MEMORY}; extract the retail corpus before verifying MEMORY.BIN"
        )
    memory_size = len(memory_data)
    memory_end = next_base + memory_size

    fmv_data = open(FMV, "rb").read() if os.path.isfile(FMV) else None
    if fmv_data is None:
        raise SystemExit(
            f"REFUSED: no {FMV}; extract the retail corpus before verifying FMV.BIN"
        )
    fmv_size = len(fmv_data)
    fmv_entry_offset = FMV_ENTRY - next_base
    if not (0 <= fmv_entry_offset <= fmv_size - 4):
        raise ValueError("FMV entry does not land inside the loader-derived file placement")
    fmv_entry_word = struct.unpack_from("<I", fmv_data, fmv_entry_offset)[0]
    if fmv_entry_word != 0x27BDFF10:  # addiu sp,sp,-0xf0
        raise ValueError(
            f"FMV entry word changed: 0x{fmv_entry_word:08X} != 0x27BDFF10"
        )

    # The arena update is decoded, not fitted: s0 starts at 0x800D5D28, the return value is masked by
    # a materialised 0x000FFFFC, and addiu v1,s0,0x80 forms the bias. The exact ALU words prove which
    # registers feed the final addu; merely finding these constants elsewhere would not prove the
    # formula.
    arena_base, arena_base_lines = formed_constant(exe, 0x8003DB20, 0x8003DB24, 16)
    arena_words = require_words(
        exe,
        {
            0x8003DB58: 0x3C03000F,  # lui v1,0x000F
            0x8003DB5C: 0x3463FFFC,  # ori v1,v1,0xFFFC
            0x8003DB60: 0x00431024,  # and v0,v0,v1
            0x8003DB64: 0x26030080,  # addiu v1,s0,0x80
            0x8003DB68: 0x00431021,  # addu v0,v0,v1
        },
        "MEMORY arena-frontier formula",
    )
    memory_size_mask = ((exe.word(0x8003DB58) & 0xFFFF) << 16) | (
        exe.word(0x8003DB5C) & 0xFFFF
    )
    memory_frontier_bias = arena_base + sx16(exe.word(0x8003DB64) & 0xFFFF)
    memory_frontier = (memory_size & memory_size_mask) + memory_frontier_bias
    pointers = memory_prefix_pointers(memory_data)
    if not pointers or not all(
        next_base <= pointer < memory_end for _, pointer in pointers
    ):
        raise ValueError(
            "MEMORY.BIN's absolute prefix does not map inside the loader-derived placement"
        )

    # Prove the value returned to the caller is CdlFILE.size rather than a sector-rounded byte count:
    # FileLoadTo -> retry wrapper -> inner loader; CdSearchFile writes its CdlFILE at sp+0x10, the inner
    # loader reads +0x14 (the size member), and both wrappers preserve that value in v0 on return.
    evidence.append(
        require_call(exe, 0x800825D8, 0x80082728, "file-loader retry wrapper")
    )
    evidence.append(require_call(exe, 0x80082750, 0x80082608, "inner file loader"))
    evidence.append(require_call(exe, 0x80082648, 0x80092AE8, "CdSearchFile"))
    size_return_words = require_words(
        exe,
        {
            0x80082658: 0x8FB00014,  # lw s0,0x14(sp): CdlFILE.size
            0x80082704: 0x02001021,  # addu v0,s0,zero
            0x80082758: 0x00401821,  # addu v1,v0,zero
            0x8008276C: 0x1052FFF8,  # retry only while the loader requests it
            0x80082770: 0x00601021,  # addu v0,v1,zero in the branch delay slot
        },
        "exact CdlFILE.size return chain",
    )
    if any(defines(exe.word(va)) in (2, -1) for va in range(0x800825DC, 0x80082608, 4)):
        raise ValueError(
            "outer file-loader epilogue clobbers or obscures the returned v0 size"
        )

    # The loader rounds to sectors but preserves [dest+size, ceil(size/2048)) by copying it out before
    # CdRead and back afterwards. Exact argument-setup words tie both copies to the same tail range;
    # call targets alone would not prove that contract.
    evidence.append(
        require_call(exe, 0x800826AC, 0x80082E6C, "save rounded-sector tail")
    )
    evidence.append(require_call(exe, 0x800826BC, 0x80093AF0, "CdRead"))
    evidence.append(
        require_call(exe, 0x800826FC, 0x80082E6C, "restore rounded-sector tail")
    )
    evidence.append(require_call(exe, 0x800826D4, 0x80093BF4, "CdReadSync"))
    tail_words = require_words(
        exe,
        {
            0x80082694: 0x001132C0,  # sll a2,s1,11: rounded byte count
            0x80082698: 0x0206102A,  # slt v0,s0,a2: exact size < rounded size
            0x800826A0: 0x02702821,  # addu a1,s3,s0: save from dest+size
            0x800826B0: 0x00D03023,  # subu a2,a2,s0: tail length
            0x800826E4: 0x001132C0,
            0x800826E8: 0x0206102A,
            0x800826F0: 0x02702021,  # addu a0,s3,s0: restore to dest+size
            0x80082700: 0x00D03023,
        },
        "rounded-sector tail preservation",
    )

    print("== instruction-verified loader contract ==", file=out)
    for line in evidence:
        print("   " + line, file=out)
    print("   exact CdlFILE.size return: " + "; ".join(size_return_words), file=out)
    print("   rounded tail arguments: " + "; ".join(tail_words), file=out)
    print(
        "   arena frontier ALU: " + "; ".join(arena_base_lines + arena_words), file=out
    )
    print(
        "   level selector: one of four names, then exactly one wrapper call:", file=out
    )
    for selector, address, name, lines in sorted(selected):
        print(
            '     %d -> 0x%08X "%s"  (%s)'
            % (selector, address, name, "; ".join(lines)),
            file=out,
        )
    print(
        "   co-resident path: DAT_800A16A8=0 loads MEMORY.BIN first; flags=0 then loads one level file",
        file=out,
    )
    print(
        "   LEVEL slot       [0x%08X,0x%08X)  %d-byte fixed window"
        % (slot, next_base, next_base - slot),
        file=out,
    )
    print(
        "   MEMORY.BIN slot  [0x%08X,0x%08X)  %d retail bytes"
        % (next_base, memory_end, memory_size),
        file=out,
    )
    print(
        "   MEMORY frontier  0x%08X = (size & 0x%05X) + 0x%08X; %d bytes after file end"
        % (
            memory_frontier,
            memory_size_mask,
            memory_frontier_bias,
            memory_frontier - memory_end,
        ),
        file=out,
    )
    print(
        "   MEMORY prefix    %d consecutive absolute word(s), all map inside the loaded file"
        % len(pointers),
        file=out,
    )
    print(
        "   FMV.BIN slot     [0x%08X,0x%08X)  %d retail bytes; entry 0x%08X = file+0x%X"
        % (next_base, next_base + fmv_size, fmv_size, FMV_ENTRY, fmv_entry_offset),
        file=out,
    )
    return {
        "level_base": slot,
        "memory_base": next_base,
        "memory_size": memory_size,
        "memory_end": memory_end,
        "memory_frontier": memory_frontier,
        "memory_size_mask": memory_size_mask,
        "memory_frontier_bias": memory_frontier_bias,
        "memory_prefix_pointers": pointers,
        "fmv_size": fmv_size,
        "fmv_entry": FMV_ENTRY,
        "fmv_entry_offset": fmv_entry_offset,
        "fmv_entry_word": fmv_entry_word,
        "selectors": selected,
    }


def out_jals(path, t_addr, t_end):
    b = open(path, "rb").read()
    n = len(b) // 4
    if n == 0:
        return b"", []
    ws = struct.unpack_from("<%dI" % n, b, 0)
    return b, [
        t
        for t in (
            ((w & 0x3FFFFFF) << 2) | 0x80000000
            for w in ws
            if (w >> 26) in (OP_J, OP_JAL)
        )
        if not (t_addr <= t < t_end)
    ]


def modules():
    """The LEVEL*/LEVEL*.BIN corpus, from the flat extraction. REFUSES on an empty corpus."""
    files = sorted(glob.glob(os.path.join(FLAT, "LEVEL*__LEVEL*.BIN")))
    if not files:
        raise SystemExit(
            f"REFUSED: 0 LEVEL*__LEVEL*.BIN in {FLAT} — the slot arithmetic would be over an "
            "empty set. Populate it with `python3 tools/extract_disc_files.py`."
        )
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

    Returns (slot_base, next_base, verdict) where verdict is `one-slot`,
    `co-resident-possible`, or `undecided`. Size can refute simultaneous residence; it cannot prove
    that two files are simultaneously loaded, so the opposite result is deliberately not called
    `two-slot`.
    """
    dests = sorted({r[3] for r in rows if r[3] is not None})
    named = {}
    for s, a0, a0s, a1, w0, w1 in rows:
        if a1 is not None and a0s:
            named.setdefault(a1, []).append(a0s)
    files = modules()
    print(
        "== which census destination do the LEVEL*.BIN modules themselves agree with? ==",
        file=out,
    )
    scored = []
    for dst in dests:
        h, den = total_hits(dst, files, exe.t_addr, exe.t_end)
        scored.append((h, dst, den))
        print(
            "   0x%08X  %4d of %d out-of-.text jal targets land in-module  (%s)"
            % (dst, h, den, ", ".join(named.get(dst, ["path built at run time"]))),
            file=out,
        )
    scored.sort(reverse=True)
    print("== the overlay slot, and what bounds it ==", file=out)
    if not scored or scored[0][0] == 0:
        print(
            "   UNDECIDABLE: NO census destination catches a single module jal target (%d targets over "
            "%d modules). The two methods do not intersect, so this tool names no slot."
            % (scored[0][2] if scored else 0, len(files)),
            file=out,
        )
        return None, None, "undecided"
    (h1, slot, den), (h2, runner) = (
        scored[0],
        (scored[1][0], scored[1][1]) if len(scored) > 1 else (0, 0),
    )
    if h1 <= h2:
        print(
            "   UNDECIDABLE: the best destination 0x%08X (%d) does not beat the runner-up 0x%08X (%d)"
            % (slot, h1, runner, h2),
            file=out,
        )
        return None, None, "undecided"
    print(
        "   slot base           0x%08X  (%d/%d module targets; runner-up 0x%08X scores %d)"
        % (slot, h1, den, runner, h2),
        file=out,
    )
    above = [d for d in dests if d > slot]
    if not above:
        print(
            f"   the slot is 0x{slot:08X} and NOTHING in the census sits above it, so its size is "
            "unbounded by this method",
            file=out,
        )
        return slot, None, "undecided"
    nxt = above[0]
    window = nxt - slot
    # WHICH call sites put a file at the slot, and how each one's PATH was determined. Printed per site
    # rather than for the first one, because the slot having several callers is the finding, not noise;
    # and a slot with no site would mean `dests` and `rows` had come apart, so it REFUSES.
    at_slot = [r for r in rows if r[3] == slot]
    if not at_slot:
        raise SystemExit(
            f"REFUSED: the slot 0x{slot:08X} was scored from the destination set but no call site "
            "in the census carries it — the two views of `rows` have diverged."
        )
    print(
        "   slot base           0x%08X  (%d of %d call site(s) load here)"
        % (slot, len(at_slot), len(rows)),
        file=out,
    )
    for s, a0, a0s, a1, w0, w1 in at_slot:
        print(
            "     site 0x%08X   dest: %-46s path: %s"
            % (s, w1, (f'"{a0s}"') if a0s else w0),
            file=out,
        )
    print(
        "   next fixed load     0x{:08X}  ({})".format(
            nxt, ", ".join(named.get(nxt, ["<no path string>"]))
        ),
        file=out,
    )
    print("   window              %d bytes" % window, file=out)
    files = modules()
    sizes = {os.path.basename(f): os.path.getsize(f) for f in files}
    biggest = max(sizes.items(), key=lambda kv: kv[1])
    print(
        "   largest LEVEL*.BIN  %d bytes (%s)%s"
        % (
            biggest[1],
            biggest[0],
            "  == the window EXACTLY" if biggest[1] == window else "  != the window",
        ),
        file=out,
    )
    over = {k: v for k, v in sizes.items() if v > window}
    print(
        "   modules larger than the window: %s"
        % (
            "NONE (all %d fit)" % len(sizes)
            if not over
            else ", ".join("%s %d" % kv for kv in over.items())
        ),
        file=out,
    )
    # THE SLOT-COUNT QUESTION. If LEVEL.BIN and LEVEL1.BIN were two slots they would both have to be
    # resident inside the window; print the arithmetic per level rather than a conclusion.
    print(
        "   two-slot test — a level's .BIN parts summed against the window:", file=out
    )
    pairs, refuted = 0, 0
    for lv in sorted({k.split("__")[0] for k in sizes}):
        parts = {k: v for k, v in sizes.items() if k.startswith(lv + "__")}
        if len(parts) < 2:
            continue
        pairs += 1
        tot = sum(parts.values())
        bad = tot > window
        refuted += bad
        print(
            "     %-8s %-46s sum %6d  %s"
            % (
                lv,
                " + ".join(
                    "%s %d" % (k.split("__")[1], v) for k, v in sorted(parts.items())
                ),
                tot,
                "EXCEEDS the window by %d — cannot be co-resident" % (tot - window)
                if bad
                else "fits",
            ),
            file=out,
        )
    verdict = (
        "one-slot" if refuted else ("co-resident-possible" if pairs else "undecided")
    )
    print(
        "   verdict: %s — %d of %d multi-part levels cannot hold both parts at once"
        % (
            "ONE SLOT, alternative contents"
            if verdict == "one-slot"
            else "OPPOSITE ANSWER: co-residence is not refuted by the window",
            refuted,
            pairs,
        ),
        file=out,
    )
    return slot, nxt, verdict


def score_report(exe, slot, out=sys.stdout):
    """Diff the census-derived base BY CODE against the independent jal-target evidence base_fit uses,
    and against that method's 4 KiB-floored answer. Prints both counts for every module."""
    floor = slot & ~0xFFF
    print(
        "== the census base scored against each module's own absolute jal targets ==",
        file=out,
    )
    print(
        "   (independent evidence: base_fit.py's method, here at WORD granularity on two fixed bases)",
        file=out,
    )
    print(
        "   %-24s %7s %7s  %s"
        % (
            "module",
            "out-jal",
            "size",
            f"hits@0x{slot:08X} / hits@0x{floor:08X} (4 KiB floor)",
        ),
        file=out,
    )
    better = worse = 0
    rows = []
    for p in modules():
        b, out_t = out_jals(p, exe.t_addr, exe.t_end)
        if not out_t:
            print(
                "   %-24s %7s %7d  NO EVIDENCE (no out-of-.text jal)"
                % (os.path.basename(p), "0", len(b)),
                file=out,
            )
            continue
        h1 = sum(1 for t in out_t if slot <= t < slot + len(b))
        h0 = sum(1 for t in out_t if floor <= t < floor + len(b))
        better += h1 > h0
        worse += h1 < h0
        rows.append((os.path.basename(p), len(out_t), len(b), h1, h0))
        print(
            "   %-24s %7d %7d  %4d / %-4d %s"
            % (
                os.path.basename(p),
                len(out_t),
                len(b),
                h1,
                h0,
                "<- census base strictly BETTER"
                if h1 > h0
                else (
                    "<- census base WORSE"
                    if h1 < h0
                    else ("all %d" % len(out_t) if h1 == len(out_t) else "")
                ),
            ),
            file=out,
        )
    print(
        "   %d module(s) score strictly better at the census base, %d worse. A base that were merely "
        "the 4 KiB floor's equal could not be told from it by this evidence."
        % (better, worse),
        file=out,
    )
    return better, worse, rows


def report(loader, out=sys.stdout):
    exe = Exe(EXE)
    rows, st = census(exe, loader)
    print(
        f"boot exe {os.path.basename(exe.path)}  .text [0x{exe.t_addr:08X},0x{exe.t_end:08X})",
        file=out,
    )
    print(f"== call-site census of callee 0x{loader:08X} ==", file=out)
    print(
        "   %d words examined, %d `jal` seen, %d target this callee; %d of those form a LITERAL a1, "
        "%d resolve a0 to a string"
        % (st["words"], st["jal"], st["sites"], st["a1_literal"], st["a0_string"]),
        file=out,
    )
    print(
        "   %-12s %-34s %-12s %s"
        % ("site", "a0 (path)", "a1 (dest)", "how a1 was determined"),
        file=out,
    )
    for s, a0, a0s, a1, w0, w1 in rows:
        a0txt = (f'"{a0s}"') if a0s else (f"0x{a0:08X}" if a0 else "UNRESOLVED")
        print(
            "   0x%08X   %-34s %-12s %s"
            % (s, a0txt[:34], f"0x{a1:08X}" if a1 is not None else "UNRESOLVED", w1),
            file=out,
        )
    if st["a1_literal"] == 0:
        print(
            "   0 of %d call sites form a literal destination: every one is computed at run time, so "
            "this method cannot name a base for this callee." % st["sites"],
            file=out,
        )
    slot, nxt, verdict = slot_report(exe, rows, out)
    better = worse = 0
    contract = None
    if slot is not None:
        better, worse, _ = score_report(exe, slot, out)
    if slot is not None and nxt is not None:
        contract = loader_contract(exe, rows, slot, nxt, out)
    for b in BLIND:
        print(f"[blind spot] {b}", file=out)
    return {
        "rows": rows,
        "stats": st,
        "slot": slot,
        "next": nxt,
        "verdict": verdict,
        "better": better,
        "worse": worse,
        "contract": contract,
    }


def shipping_comparison(measured, out=sys.stdout):
    """Compare the two proven resident slots with the runtime title configuration."""
    config = open(CONFIG, encoding="utf-8").read()

    def constant(name):
        match = re.search(
            rf"\b{re.escape(name)}\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)u?\s*;", config
        )
        return int(match.group(1), 0) if match else None

    checks = [
        (
            "game_config kLevelOverlayBase",
            constant("kLevelOverlayBase"),
            measured["level_base"],
        ),
        (
            "game_config kMemoryOverlayBase",
            constant("kMemoryOverlayBase"),
            measured["memory_base"],
        ),
    ]
    slot_text = re.search(r"\.overlaySlots\s*=\s*\{\{(.*?)\}\},", config, re.DOTALL)
    slots_ok = bool(
        slot_text
        and re.search(
            r"\{\s*kLevelOverlayBase\s*,\s*\"LEVEL\"\s*\}", slot_text.group(0)
        )
        and re.search(
            r"\{\s*kMemoryOverlayBase\s*,\s*\"MEMORY\"\s*\}", slot_text.group(0)
        )
    )
    checks.append(("game_config overlaySlots LEVEL+MEMORY", 1 if slots_ok else 0, 1))

    print("== shipping comparison (proven fields only) ==", file=out)
    failures = []
    for name, actual, expected in checks:
        ok = actual == expected
        print(
            "   %-4s %-43s ships %-32r measured %r"
            % ("ok" if ok else "FAIL", name, actual, expected),
            file=out,
        )
        if not ok:
            failures.append(name)
    return failures


# ---------------------------------------------------------------------------------------------------
# THE GATE. Anchors measured 2026-08-12 and used ONLY here, so the reporting path cannot be biased by
# them. Each is something a BROKEN fold gets wrong in a specific way.
GATE_SLOT = 0x800D12C0  # the overlay slot, from site 0x8003DEAC
GATE_NEXT = (
    0x800D5D20  # the destination bounding the slot from above, from site 0x8003DB50
)
# MEASURED, and not what a first reading expects: FOUR sites load 0x800D5D20 and they name TWO files —
# BITS/MEMORY.BIN and FMV/FMV.BIN share this buffer. The post-load call to file+0x908 proves FMV.BIN
# contains entered code; it is anchored here rather than smoothed into "the MEMORY.BIN destination".
GATE_NEXT_SITES = 4
GATE_NEXT_PATHS = ["bits\\memory.bin", "fmv\\fmv.bin"]
GATE_SITES = 13  # call sites of 0x80082508
DECOY_LOADER = 0x80082870  # the PATH BUILDER: 1 call site, whose a1 is a stack buffer


def selftest():
    import io

    fails = []
    checks = []

    def ck(name, ok, detail):
        checks.append(name)
        print(
            "[selftest] %-4s %s\n            %s"
            % ("PASS" if ok else "FAIL", name, detail)
        )
        if not ok:
            fails.append(name)

    exe = Exe(EXE)
    rows, st = census(exe, LOADER)
    dests = {r[3] for r in rows if r[3] is not None}
    ck(
        "POSITIVE: the loader's call sites are found and their destinations fold",
        st["sites"] == GATE_SITES and GATE_SLOT in dests and GATE_NEXT in dests,
        "%d sites (expected %d), %d literal destinations; slot 0x%08X %s, next 0x%08X %s"
        % (
            st["sites"],
            GATE_SITES,
            st["a1_literal"],
            GATE_SLOT,
            "present" if GATE_SLOT in dests else "MISSING",
            GATE_NEXT,
            "present" if GATE_NEXT in dests else "MISSING",
        ),
    )

    # EVERY site loading the bounding destination must resolve a PATH, and the measured set of paths is
    # the anchor — not a count of one. Four sites load 0x800D5D20 and they name TWO different files, so
    # asserting a unique site here would be asserting something the bytes contradict.
    mem = [r for r in rows if r[3] == GATE_NEXT]
    paths = sorted({(r[2] or "").lower() for r in mem})
    ck(
        "POSITIVE: every site loading the bounding destination resolves a PATH, and the path SET is the "
        "measured one",
        len(mem) == GATE_NEXT_SITES
        and all(r[2] for r in mem)
        and paths == GATE_NEXT_PATHS,
        "0x%08X <- %d site(s) (expected %d), %d resolve a path; paths %r (expected %r)"
        % (
            GATE_NEXT,
            len(mem),
            GATE_NEXT_SITES,
            sum(1 for r in mem if r[2]),
            paths,
            GATE_NEXT_PATHS,
        ),
    )

    # NEGATIVE CONTROL 1: the path BUILDER. Same code path, a callee that is not a loader. Its argument
    # is a stack buffer, so a fold that fabricates constants shows up here as a bogus 0x800Dxxxx.
    drows, dst = census(exe, DECOY_LOADER)
    dbad = [r for r in drows if r[3] is not None and 0x800D0000 <= r[3] < 0x800E0000]
    ck(
        "NEGATIVE control: the path builder's a1 is a STACK buffer and folds to no slot-like literal",
        dst["sites"] == 1 and not dbad,
        "%d site(s); a1 %s; %d slot-like literal(s) fabricated"
        % (dst["sites"], drows[0][5] if drows else "-", len(dbad)),
    )

    # NEGATIVE CONTROL 2: refusals. An address with no callers, a misaligned one, an out-of-range one.
    refused = 0
    for bad in (0x8009F000, 0x80082509, 0x80200000):
        try:
            census(exe, bad)
        except SystemExit:
            refused += 1
    ck(
        "REFUSALS: a callee with 0 call sites, a misaligned one and an out-of-.text one all refuse",
        refused == 3,
        "%d of 3 refused" % refused,
    )

    # DERIVATION: the slot, the window and the slot-count verdict must come out of the census, and the
    # census base must be DISTINGUISHABLE from base_fit.py's 4 KiB-floored answer by the module
    # evidence. Without that last clause the whole finding would be unfalsifiable decoration.
    buf = io.StringIO()
    slot, nxt, verdict = slot_report(exe, rows, buf)
    better, worse, mrows = score_report(exe, slot, buf) if slot else (0, 0, [])
    ck(
        "DERIVATION: slot, next base and the ONE-SLOT verdict are re-derived from the census",
        slot == GATE_SLOT and nxt == GATE_NEXT and verdict == "one-slot",
        "derived slot 0x{}, next 0x{}, verdict {}".format(
            (f"{slot:08X}") if slot else "-", (f"{nxt:08X}") if nxt else "-", verdict
        ),
    )
    ck(
        "DISCRIMINATION: the census base beats the 4 KiB floor on the modules' own jal targets",
        better >= 1 and worse == 0,
        "%d module(s) strictly better at 0x%08X than at 0x%08X, %d worse"
        % (better, slot or 0, (slot or 0) & ~0xFFF, worse),
    )
    full = [r for r in mrows if r[3] == r[1]]
    ck(
        "DISCRIMINATION: at the census base at least one module goes from partial to 100%",
        any(r[3] == r[1] and r[4] < r[1] for r in mrows),
        "%d of %d scoring modules land ALL their targets at the census base; upgraded from partial: %s"
        % (
            len(full),
            len(mrows),
            ", ".join(
                "%s %d/%d->%d/%d" % (r[0], r[4], r[1], r[3], r[1])
                for r in mrows
                if r[3] == r[1] and r[4] < r[1]
            )
            or "NONE",
        ),
    )

    contract = loader_contract(exe, rows, slot, nxt, io.StringIO())
    ck(
        "POSITIVE: retail call flow and module bytes prove the complete two-slot contract",
        contract["level_base"] == GATE_SLOT
        and contract["memory_base"] == GATE_NEXT
        and contract["memory_size"] == 63312
        and contract["memory_frontier"] == 0x800E54F8
        and len(contract["memory_prefix_pointers"]) == 11
        and contract["fmv_size"] == 510960
        and contract["fmv_entry"] == FMV_ENTRY
        and contract["fmv_entry_offset"] == 0x908
        and contract["fmv_entry_word"] == 0x27BDFF10,
        "LEVEL 0x%08X; MEMORY [0x%08X,0x%08X), frontier 0x%08X; %d absolute prefix words; "
        "FMV entry 0x%08X=file+0x%X"
        % (
            contract["level_base"],
            contract["memory_base"],
            contract["memory_end"],
            contract["memory_frontier"],
            len(contract["memory_prefix_pointers"]),
            contract["fmv_entry"],
            contract["fmv_entry_offset"],
        ),
    )

    # FORCE THE OPPOSITE ANSWER through the SAME executable census. Moving every real call site's
    # MEMORY/FMV destination to 0x800D9D20 widens the window enough for all ten LEVEL/LEVEL1 pairs.
    # The result must cease saying one-slot; this validates the slot-count instrument can report the
    # other answer instead of merely echoing the shipping expectation.
    replacements = {}
    for hi, lo in (
        (0x8003DB48, 0x8003DB4C),
        (0x8003EEA4, 0x8003EEA8),
        (0x8003FCF4, 0x8003FCF8),
        (0x800417C8, 0x800417CC),
    ):
        replacements[hi] = 0x3C05800E  # lui a1,0x800E
        replacements[lo] = 0x24A59D20  # addiu a1,a1,-0x62E0 -> 0x800D9D20
    opposite_exe = exe.patched(replacements)
    opposite_rows, _ = census(opposite_exe, LOADER)
    opposite_slot, opposite_next, opposite_verdict = slot_report(
        opposite_exe, opposite_rows, io.StringIO()
    )
    ck(
        "FORCED OPPOSITE: a widened next-slot bound reports co-residence possible",
        opposite_slot == GATE_SLOT
        and opposite_next == 0x800D9D20
        and opposite_verdict == "co-resident-possible",
        f"mutated slot 0x{opposite_slot or 0:08X}, next 0x{opposite_next or 0:08X}, verdict {opposite_verdict}",
    )

    shipping_failures = shipping_comparison(contract, io.StringIO())
    ck(
        "SHIPPING: GameConfig equals the independently derived slots",
        not shipping_failures,
        "0 disagreements" if not shipping_failures else ", ".join(shipping_failures),
    )

    print("[selftest] %d/%d passed" % (len(checks) - len(fails), len(checks)))
    print(
        f"[selftest] what this CANNOT see: whether the loader anchor 0x{LOADER:08X} really is the CD file "
        "loader (the Ghidra half of C010/I009), and whether a destination computed at "
        "run time exists that this method never shows."
    )
    for b in BLIND:
        print(f"[selftest] blind spot: {b}")
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(
        description="where the CD loader puts each file, decoded from the exe"
    )
    ap.add_argument(
        "--loader",
        default=hex(LOADER),
        help="callee VA to census (default the CD loader)",
    )
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument(
        "--check",
        action="store_true",
        help="derive the retail map and fail if the runtime GameConfig disagrees",
    )
    a = ap.parse_args()
    if a.selftest:
        raise SystemExit(selftest())
    measured = report(int(a.loader, 0))
    if a.check:
        if int(a.loader, 0) != LOADER or measured["contract"] is None:
            raise SystemExit(
                "REFUSED: --check requires the verified loader anchor and complete contract"
            )
        raise SystemExit(1 if shipping_comparison(measured["contract"]) else 0)


if __name__ == "__main__":
    main()
