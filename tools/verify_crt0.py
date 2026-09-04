#!/usr/bin/env python3
"""Verify Toy Story 2's complete crt0 boot group from SLUS_008.93 instructions.

Ghidra's first entry decompile stopped after the BSS clear because words immediately after the
terminating ``break`` look like instructions. They are startup data: the entry itself loads one of
those words to construct ``sp``. This purpose-built symbolic walk follows the real entry sequence
through both returning ``jal`` calls and the ``break`` while treating the referenced words as data.

Every measured GameConfig field is printed beside the instruction chain that proves it. ``--check``
also parses the shipping ``game/core/game_config.cpp`` and compares its constants and designated
initialiser against this run. There is no second expected-address table in this tool.

Exit 0 means a complete agreeing group. Exit 1 means real bytes were analysed but disagree with the
shipping config. Exit 2 is a refusal: malformed/wrong input or a crt0 shape this instrument cannot
prove. ``--selftest`` drives the same analysis and shipping comparison through positive, binary and
source mutations, malformed inputs, and (when ``--cross`` is supplied) a real second executable.
"""

from __future__ import annotations

import argparse
import hashlib
import itertools
import os
import re
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PSXPORT = Path(os.environ.get("PSXPORT_DIR", ROOT / "external" / "psxport"))
sys.path.insert(0, str(PSXPORT / "tools"))

from formats import psx_exe as psexe

DEFAULT_EXE = ROOT / "scratch" / "bin" / "toystory2" / "SLUS_008.93"
SHIPPED_FILE = ROOT / "game" / "core" / "game_config.cpp"
IDENTITY_FILE = ROOT / "docs" / "info" / "exe-identity.txt"

REG = (
    "zero",
    "at",
    "v0",
    "v1",
    "a0",
    "a1",
    "a2",
    "a3",
    "t0",
    "t1",
    "t2",
    "t3",
    "t4",
    "t5",
    "t6",
    "t7",
    "s0",
    "s1",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "t8",
    "t9",
    "k0",
    "k1",
    "gp",
    "sp",
    "fp",
    "ra",
)

# ``addiu t2,zero,0xA0; jr t2; addiu t1,zero,0x39`` dispatches BIOS A-table
# function 0x39, InitHeap(ptr,size). This identifies the first jal semantically;
# its target address still comes from the executable.
INIT_HEAP_THUNK = (0x240A00A0, 0x01400008, 0x24090039)
SCAN_LIMIT = 96


class Refused(Exception):
    """The input or control-flow shape cannot support a boot-group conclusion."""


@dataclass(frozen=True)
class Insn:
    pc: int
    word: int
    kind: str
    rs: int
    rt: int
    rd: int
    sa: int
    imm: int
    simm: int

    @property
    def target(self) -> int:
        if self.kind in ("jal", "j"):
            return ((self.pc + 4) & 0xF0000000) | ((self.word & 0x03FFFFFF) << 2)
        return self.pc + 4 + self.simm * 4

    @property
    def text(self) -> str:
        if self.kind == "lui":
            return f"lui {REG[self.rt]}, 0x{self.imm:04x}"
        if self.kind in ("addi", "addiu"):
            return f"{self.kind} {REG[self.rt]}, {REG[self.rs]}, {self.simm}"
        if self.kind == "ori":
            return f"ori {REG[self.rt]}, {REG[self.rs]}, 0x{self.imm:04x}"
        if self.kind in ("lw", "sw"):
            return f"{self.kind} {REG[self.rt]}, {self.simm}({REG[self.rs]})"
        if self.kind in ("sll", "srl"):
            return f"{self.kind} {REG[self.rd]}, {REG[self.rt]}, {self.sa}"
        if self.kind in ("addu", "subu", "or", "sltu"):
            return f"{self.kind} {REG[self.rd]}, {REG[self.rs]}, {REG[self.rt]}"
        if self.kind in ("jal", "j"):
            return f"{self.kind} 0x{self.target:08x}"
        if self.kind in ("beq", "bne"):
            return f"{self.kind} {REG[self.rs]}, {REG[self.rt]}, 0x{self.target:08x}"
        if self.kind == "nop":
            return "nop"
        if self.kind in ("break", "jr"):
            return self.kind if self.kind == "break" else f"jr {REG[self.rs]}"
        return f".word 0x{self.word:08x}"

    def cite(self) -> str:
        return f"0x{self.pc:08X}: {self.word:08X}  {self.text}"


@dataclass
class Shipping:
    path: Path
    raw: str
    constants: dict[str, int]
    fields: dict[str, str]
    stack_bias: tuple[str, str] | None


CONST_FIELD = {
    "kCrt0BssZeroLo": "bssZeroLo",
    "kCrt0BssZeroHi": "bssZeroHi",
    "kCrt0StackTopBase": "stackTopBase",
    "kCrt0StackTopBase2": "stackTopBase2",
    "kCrt0HeapBase": "heapBase",
    "kCrt0HeapSizePtr": "heapSizePtr",
    "kCrt0HeapBasePtr": "heapBasePtr",
    "kCrt0Gp": "gp",
    "kCrt0LibcInit": "libcInit",
    "kCrt0GameMain": "gameMain",
    "kCrt0Entry": "entry",
    "kCrt0StackBias": "stackBias",
}
HEADER_CONST = {
    "kPsExeEntry": "entry",
    "kPsExeTextAddr": "load",
    "kPsExeTextSize": "text_size",
    "kPsExeSpHeader": "sp_base",
}
CFG_CONST = {
    "bssZeroLo": "kCrt0BssZeroLo",
    "bssZeroHi": "kCrt0BssZeroHi",
    "stackTopBase": "kCrt0StackTopBase",
    "stackTopBase2": "kCrt0StackTopBase2",
    "heapBase": "kCrt0HeapBase",
    "heapSizePtr": "kCrt0HeapSizePtr",
    "heapBasePtr": "kCrt0HeapBasePtr",
    "gp": "kCrt0Gp",
    "libcInit": "kCrt0LibcInit",
    "gameMain": "kCrt0GameMain",
    "crt0": "kCrt0Entry",
}


def decode(pc: int, word: int) -> Insn:
    op = word >> 26
    rs, rt, rd = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
    sa, fn, imm = (word >> 6) & 31, word & 63, word & 0xFFFF
    simm = imm - 0x10000 if imm & 0x8000 else imm
    if word == 0:
        kind = "nop"
    elif op == 0:
        kind = {
            0x00: "sll",
            0x02: "srl",
            0x08: "jr",
            0x0D: "break",
            0x21: "addu",
            0x23: "subu",
            0x25: "or",
            0x2B: "sltu",
        }.get(fn, "other")
    else:
        kind = {
            0x02: "j",
            0x03: "jal",
            0x04: "beq",
            0x05: "bne",
            0x08: "addi",
            0x09: "addiu",
            0x0D: "ori",
            0x0F: "lui",
            0x23: "lw",
            0x2B: "sw",
        }.get(op, "other")
    return Insn(pc, word, kind, rs, rt, rd, sa, imm, simm)


def load_exe(path: Path):
    if not path.is_file():
        raise Refused(f"{path} does not exist — SCANNED NOTHING")
    try:
        return psexe.load(str(path))
    except (OSError, ValueError, struct.error) as exc:
        raise Refused(
            f"{path} ({path.stat().st_size} bytes) is not a complete PS-X EXE — SCANNED NOTHING: {exc}"
        ) from exc


def identity() -> tuple[str, int, str]:
    if not IDENTITY_FILE.is_file():
        raise Refused(f"{IDENTITY_FILE} is missing — target identity is unknown")
    for line in IDENTITY_FILE.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            parts = line.split()
            if len(parts) == 3 and re.fullmatch(r"[0-9a-f]{40}", parts[0]):
                return parts[0], int(parts[1]), parts[2]
    raise Refused(f"{IDENTITY_FILE} has no '<sha1> <size> <name>' record")


def require_target(path: Path) -> None:
    expected_sha, expected_size, expected_name = identity()
    data = path.read_bytes()
    actual_sha = hashlib.sha1(data).hexdigest()
    if (
        path.name != expected_name
        or len(data) != expected_size
        or actual_sha != expected_sha
    ):
        raise Refused(
            f"{path} is not the verified target {expected_name}: got {len(data)} bytes sha1 {actual_sha}, "
            f"expected {expected_size} bytes sha1 {expected_sha}. SCANNED NOTHING as Toy Story 2."
        )


def analyze(exe) -> dict:
    reg: list[int | None] = [None] * 32
    provenance: list[int | None] = [None] * 32
    reg[0] = 0
    out: dict = {
        "entry": exe.entry,
        "why": {},
        "abs_loads": [],
        "abs_stores": [],
        "jals": [],
        "walked": 0,
    }
    pending_loop: tuple[int, int, list[Insn]] | None = None
    pc = exe.entry

    def insn(at: int) -> Insn:
        try:
            return decode(at, exe.word(at))
        except IndexError as exc:
            raise Refused(
                f"crt0 walk left the mapped image at 0x{at:08X} after {out['walked']} instruction(s)"
            ) from exc

    def apply(op: Insn) -> None:
        rs, rt, rd = op.rs, op.rt, op.rd
        if op.kind == "lui":
            reg[rt], provenance[rt] = op.imm << 16, None
        elif op.kind in ("addi", "addiu"):
            reg[rt] = None if reg[rs] is None else (reg[rs] + op.simm) & 0xFFFFFFFF
            provenance[rt] = provenance[rs]
        elif op.kind == "ori":
            reg[rt] = None if reg[rs] is None else reg[rs] | op.imm
            provenance[rt] = None
        elif op.kind in ("sll", "srl"):
            if reg[rt] is None:
                reg[rd] = None
            elif op.kind == "sll":
                reg[rd] = (reg[rt] << op.sa) & 0xFFFFFFFF
            else:
                reg[rd] = reg[rt] >> op.sa
            provenance[rd] = None
        elif op.kind in ("addu", "subu", "or"):
            left, right = reg[rs], reg[rt]
            if left is None or right is None:
                reg[rd] = None
            elif op.kind == "addu":
                reg[rd] = (left + right) & 0xFFFFFFFF
            elif op.kind == "subu":
                reg[rd] = (left - right) & 0xFFFFFFFF
            else:
                reg[rd] = left | right
            provenance[rd] = None
        elif op.kind == "sltu":
            reg[rd], provenance[rd] = None, None
        elif op.kind == "lw":
            reg[rt], provenance[rt] = None, None
        reg[0], provenance[0] = 0, None

    while out["walked"] < SCAN_LIMIT:
        op = insn(pc)
        out["walked"] += 1

        if op.kind == "break":
            out["break"] = op
            break

        if (
            op.kind == "sw"
            and op.rt == 0
            and reg[op.rs] is not None
            and pending_loop is None
        ):
            lo = (reg[op.rs] + op.simm) & 0xFFFFFFFF
            pending_loop = (op.rs, lo, [op])
        elif pending_loop is not None:
            loop_reg, lo, evidence = pending_loop
            evidence.append(op)
            if op.kind == "bne" and op.simm < 0:
                sltu = next(
                    (
                        item
                        for item in evidence
                        if item.kind == "sltu" and item.rs == loop_reg
                    ),
                    None,
                )
                if sltu is None or reg[sltu.rt] is None or op.target != evidence[0].pc:
                    raise Refused(
                        f"candidate BSS loop at 0x{evidence[0].pc:08X} has an unproved bound/back-edge"
                    )
                out["bssZeroLo"], out["bssZeroHi"] = lo, reg[sltu.rt]
                out["why"]["bssZeroLo"] = list(evidence)
                out["why"]["bssZeroHi"] = list(evidence)
                pending_loop = None

        if op.kind in ("lw", "sw") and reg[op.rs] is not None:
            address = (reg[op.rs] + op.simm) & 0xFFFFFFFF
            record = (op, address, REG[op.rt])
            if op.kind == "lw":
                out["abs_loads"].append(record)
            elif op.rt != 0:
                out["abs_stores"].append(record)

        if (
            op.kind == "subu"
            and provenance[op.rs] is not None
            and provenance[op.rt] is not None
            and "heapSubu" not in out
        ):
            out["heapSubu"] = (op, provenance[op.rs], provenance[op.rt])

        if op.kind == "jal":
            if out["walked"] == SCAN_LIMIT:
                raise Refused(
                    f"jal at 0x{pc:08X} has a delay slot beyond the {SCAN_LIMIT}-instruction scan window"
                )
            delay = insn(pc + 4)
            out["walked"] += 1
            apply(delay)
            out["jals"].append(
                (op, delay, tuple(reg[register] for register in (4, 5, 6, 7)))
            )
            pc += 8
            continue

        apply(op)
        if op.kind == "lw" and out["abs_loads"] and out["abs_loads"][-1][0].pc == pc:
            address = out["abs_loads"][-1][1]
            try:
                reg[op.rt], provenance[op.rt] = exe.word(address), address
            except IndexError:
                reg[op.rt], provenance[op.rt] = None, None

        pc += 4
    if "break" not in out:
        raise Refused(f"entry did not reach break within {SCAN_LIMIT} instruction(s)")

    _derive_fields(exe, out)
    _validate_shape(exe, out)
    return out


def _walk(exe, start: int, stop: int) -> list[Insn]:
    return [decode(pc, exe.word(pc)) for pc in range(start, stop, 4)]


def _derive_fields(exe, out: dict) -> None:
    sequence = _walk(exe, exe.entry, out["break"].pc + 4)

    loop = out["why"]["bssZeroLo"]
    loop_store = loop[0]
    loop_test = next(op for op in loop if op.kind == "sltu")

    def constant_chain(register: int, before: int) -> list[Insn]:
        prefix = [op for op in sequence if op.pc < before]
        low = next(
            (
                op
                for op in reversed(prefix)
                if op.kind in ("addi", "addiu", "ori") and op.rt == register
            ),
            None,
        )
        if low is None:
            return []
        high = next(
            (
                op
                for op in reversed(prefix)
                if op.pc < low.pc and op.kind == "lui" and op.rt == low.rs
            ),
            None,
        )
        return [item for item in (high, low) if item is not None]

    out["why"]["bssZeroLo"] = [
        *constant_chain(loop_store.rs, loop_store.pc),
        *loop,
    ]
    out["why"]["bssZeroHi"] = [
        *constant_chain(loop_test.rt, loop_store.pc),
        *loop,
    ]

    stack_or = next((op for op in sequence if op.kind == "or" and op.rd == 29), None)
    if stack_or is not None:
        source = stack_or.rs
        prefix = [op for op in sequence if op.pc <= stack_or.pc]
        load = next(
            (op for op in reversed(prefix) if op.kind == "lw" and op.rt == source), None
        )
        if load is not None:
            load_record = next(
                (item for item in out["abs_loads"] if item[0].pc == load.pc), None
            )
            if load_record is not None:
                biases = [
                    op
                    for op in prefix
                    if load.pc < op.pc < stack_or.pc
                    and op.kind in ("addi", "addiu")
                    and op.rs == source
                    and op.rt == source
                ]
                out["stackTopBase"] = load_record[1]
                out["stackBias"] = sum(op.simm for op in biases)
                address_add = next(
                    (
                        op
                        for op in reversed(prefix)
                        if op.pc < load.pc and op.kind == "addu" and op.rd == load.rs
                    ),
                    None,
                )
                address_chain: list[Insn] = []
                if address_add is not None:
                    address_chain.extend(constant_chain(address_add.rs, address_add.pc))
                    offset_def = next(
                        (
                            op
                            for op in reversed(prefix)
                            if op.pc < address_add.pc
                            and op.kind in ("addi", "addiu")
                            and op.rt == address_add.rt
                            and op.rs == 0
                        ),
                        None,
                    )
                    if offset_def is not None:
                        address_chain.append(offset_def)
                    address_chain.append(address_add)
                    address_chain.sort(key=lambda item: item.pc)
                out["why"]["stackTopBase"] = [
                    *address_chain,
                    load,
                    *biases,
                    stack_or,
                ]
                out["why"]["stackBias"] = [
                    op for op in sequence if load.pc <= op.pc <= stack_or.pc
                ]

    for first, second in itertools.pairwise(sequence):
        if (
            first.kind == "sll"
            and first.sa == 3
            and second.kind == "srl"
            and second.sa == 3
        ):
            prefix = [op for op in sequence if op.pc < first.pc]
            add = next(
                (
                    op
                    for op in reversed(prefix)
                    if op.kind in ("addi", "addiu") and op.rt == first.rt
                ),
                None,
            )
            if add is not None:
                high = next(
                    (
                        op
                        for op in reversed(prefix)
                        if op.pc < add.pc and op.kind == "lui" and op.rt == add.rs
                    ),
                    None,
                )
                if high is not None:
                    out["heapBase"] = ((high.imm << 16) + add.simm) & 0xFFFFFFFF
                    out["why"]["heapBase"] = [high, add, first, second]
            break

    if "heapSubu" in out:
        subtract, stack_source, reserve_source = out["heapSubu"]
        if stack_source == out.get("stackTopBase"):
            out["stackTopBase2"] = reserve_source
            reserve_load = next(
                op for op, address, _ in out["abs_loads"] if address == reserve_source
            )
            reserve_base = next(
                (
                    op
                    for op in reversed(sequence)
                    if op.pc < reserve_load.pc
                    and op.kind == "lui"
                    and op.rt == reserve_load.rs
                ),
                None,
            )
            second_subtract = next(
                (
                    op
                    for op in sequence
                    if op.pc > subtract.pc
                    and op.kind == "subu"
                    and op.rs == subtract.rd
                ),
                None,
            )
            out["why"]["stackTopBase2"] = [
                *([reserve_base] if reserve_base else []),
                reserve_load,
                subtract,
                *([second_subtract] if second_subtract else []),
            ]

    gp_add = next(
        (op for op in sequence if op.kind in ("addi", "addiu") and op.rt == 28), None
    )
    if gp_add is not None:
        gp_lui = next(
            (
                op
                for op in sequence
                if op.pc < gp_add.pc and op.kind == "lui" and op.rt == gp_add.rs
            ),
            None,
        )
        if gp_lui is not None:
            out["gp"] = ((gp_lui.imm << 16) + gp_add.simm) & 0xFFFFFFFF
            out["why"]["gp"] = [gp_lui, gp_add]

    if out["jals"]:
        libc, libc_delay, _ = out["jals"][0]
        out["libcInit"] = libc.target
        thunk = [
            decode(libc.target + index * 4, exe.word(libc.target + index * 4))
            for index in range(3)
        ]
        out["why"]["libcInit"] = [libc, libc_delay, *thunk]
    if len(out["jals"]) >= 2:
        main, main_delay, _ = out["jals"][1]
        out["gameMain"] = main.target
        between = [op for op in sequence if out["jals"][0][0].pc <= op.pc <= main.pc]
        out["why"]["gameMain"] = [*between, main_delay]

    first_jal_pc = out["jals"][0][0].pc if out["jals"] else out["break"].pc
    prologue_stores = [item for item in out["abs_stores"] if item[0].pc < first_jal_pc]
    out["heapSizePtr"] = None
    out["heapBasePtr"] = None
    out["why"]["heapSizePtr"] = [item[0] for item in prologue_stores]
    out["why"]["heapBasePtr"] = [item[0] for item in prologue_stores]

    out["why"]["entry"] = [sequence[0]]
    if out.get("stackTopBase") is not None:
        out["stackTopWord"] = exe.word(out["stackTopBase"])
    if out.get("stackTopBase2") is not None:
        out["stackReserveWord"] = exe.word(out["stackTopBase2"])
    if all(
        key in out
        for key in ("stackTopWord", "stackReserveWord", "heapBase", "stackBias")
    ):
        biased = (out["stackTopWord"] + out["stackBias"]) & 0xFFFFFFFF
        out["sp"] = biased | 0x80000000
        out["heapSize"] = (
            biased - out["stackReserveWord"] - (out["heapBase"] & 0x1FFFFFFF)
        ) & 0xFFFFFFFF


def _validate_shape(exe, out: dict) -> None:
    required = (
        "bssZeroLo",
        "bssZeroHi",
        "stackTopBase",
        "stackBias",
        "stackTopBase2",
        "heapBase",
        "gp",
        "libcInit",
        "gameMain",
        "break",
        "sp",
        "heapSize",
    )
    missing = [field for field in required if field not in out]
    if missing:
        raise Refused(
            f"0x{exe.entry:08X} is not a complete supported crt0: walked {out['walked']} instruction(s), "
            f"could not prove {', '.join(missing)}"
        )
    if len(out["jals"]) != 2:
        raise Refused(
            f"crt0 reached break with {len(out['jals'])} jal(s), expected InitHeap then gameMain"
        )
    libc, delay, live = out["jals"][0]
    thunk = tuple(exe.word(libc.target + index * 4) for index in range(3))
    if thunk != INIT_HEAP_THUNK:
        raise Refused(
            f"first jal target 0x{libc.target:08X} is not the A(39h) InitHeap thunk: {thunk!r}"
        )
    if delay.kind not in ("addi", "addiu") or (delay.rs, delay.rt, delay.simm) != (
        4,
        4,
        4,
    ):
        raise Refused(f"InitHeap delay slot is not addi a0,a0,4: {delay.cite()}")
    if live[1] is None:
        raise Refused(
            "a1 is unresolved at InitHeap; heap size was not proven live at the call"
        )
    if (
        out["bssZeroHi"] <= out["bssZeroLo"]
        or out["bssZeroLo"] & 3
        or out["bssZeroHi"] & 3
    ):
        raise Refused(
            f"BSS bounds are inverted or unaligned: 0x{out['bssZeroLo']:08X}..0x{out['bssZeroHi']:08X}"
        )
    if out["stackTopBase"] <= out["break"].pc:
        raise Refused(
            "the stack word is not inline data after the terminating break; Ghidra truncation remains unresolved"
        )


CONST_RE = re.compile(
    r"^\s*static\s+constexpr\s+(?:u?int32_t)\s+(\w+)\s*=\s*([^;]+);",
    re.MULTILINE,
)
CFG_OPEN_RE = re.compile(r"\bg_ts2_cfg\s*=\s*\{")
FIELD_RE = re.compile(r"\.(\w+)\s*=\s*([^,\n]*)")
STACK_BIAS_RE = re.compile(r"\.stackBias\s*=\s*\{([^}]*)\}")


def _strip_comments(text: str) -> str:
    return "\n".join(line.split("//", maxsplit=1)[0] for line in text.splitlines())


def _evaluate(token: str, constants: dict[str, int], where: str) -> int:
    value = token.strip().rstrip("uU")
    try:
        return int(value, 0)
    except ValueError:
        if value in constants:
            return constants[value]
        raise Refused(
            f"{where}: cannot evaluate {token!r}; an ungated field is not a pass"
        ) from None


def parse_shipping(path: Path = SHIPPED_FILE, text: str | None = None) -> Shipping:
    if text is None:
        if not path.is_file():
            raise Refused(f"shipping file {path} is missing — NOTHING was compared")
        text = path.read_text(encoding="utf-8")
    source = _strip_comments(text)
    constants: dict[str, int] = {}
    for match in CONST_RE.finditer(source):
        constants[match.group(1)] = _evaluate(
            match.group(2), constants, f"{path}:{match.group(1)}"
        )
    start = CFG_OPEN_RE.search(source)
    if start is None:
        raise Refused(f"{path} has no g_ts2_cfg initialiser")
    index, depth, end = start.end(), 1, None
    while index < len(source):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                end = index
                break
        index += 1
    if end is None:
        raise Refused(f"{path}: unbalanced g_ts2_cfg braces")
    body = source[start.end() : end]
    fields = {name: value.strip() for name, value in FIELD_RE.findall(body)}
    bias_match = STACK_BIAS_RE.search(body)
    bias = (
        tuple(item.strip() for item in bias_match.group(1).split(","))
        if bias_match
        else None
    )
    if bias is not None and len(bias) != 2:
        raise Refused(f"{path}: stackBias must contain declared,value")
    return Shipping(path, text, constants, fields, bias)


def measured(out: dict, field: str) -> int:
    value = out.get(field)
    return 0 if value is None and field in ("heapSizePtr", "heapBasePtr") else value


def compare_shipping(
    out: dict, exe, shipping: Shipping, exe_path: Path
) -> tuple[list[str], list[str]]:
    failures: list[str] = []
    lines: list[str] = []
    for constant, field in {**HEADER_CONST, **CONST_FIELD}.items():
        wanted = (
            getattr(exe, field) if constant in HEADER_CONST else measured(out, field)
        )
        got = shipping.constants.get(constant)
        ok = got == wanted
        lines.append(
            f"{'ok ' if ok else 'BAD'} {constant:<22} ships {format_value(got):<12} measured {format_value(wanted)}"
        )
        if not ok:
            failures.append(
                f"{constant} ships {format_value(got)} but instructions measured {field}={format_value(wanted)}"
            )

    for field, constant in CFG_CONST.items():
        token = shipping.fields.get(field)
        if token != constant:
            failures.append(f"g_ts2_cfg .{field} must name {constant}; got {token!r}")
    if shipping.stack_bias != ("1", "kCrt0StackBias"):
        failures.append(
            f"g_ts2_cfg .stackBias must be {{1, kCrt0StackBias}}; got {shipping.stack_bias!r}"
        )

    expected_sha, _, _ = identity()
    actual_sha = hashlib.sha1(exe_path.read_bytes()).hexdigest()
    if actual_sha != expected_sha:
        failures.append(
            f"analysed image sha1 {actual_sha} is not recorded target {expected_sha}"
        )
    if expected_sha not in shipping.raw:
        failures.append("game_config.cpp does not cite the verified executable sha1")
    return failures, lines


def format_value(value) -> str:
    if value is None:
        return "MISSING"
    if isinstance(value, int):
        return f"0x{value & 0xFFFFFFFF:08X}"
    return str(value)


def report(out: dict, exe_path: Path) -> None:
    print(f"verify_crt0 {exe_path}")
    print(
        f"  entry 0x{out['entry']:08X}: walked {out['walked']} instruction(s), "
        f"2 returning jal(s), break at 0x{out['break'].pc:08X}"
    )
    print("  measured GameConfig boot group (instruction evidence):")
    for field in CONST_FIELD.values():
        if field == "entry":
            note = "PS-X EXE pc0; first decoded instruction follows"
        elif field in ("heapSizePtr", "heapBasePtr"):
            note = f"ABSENT; all {len(out['why'][field])} absolute pre-InitHeap store(s) shown"
        elif field == "stackBias" and out[field] == 0:
            note = "0; no bias instruction exists between the stack-word load and or sp"
        else:
            note = format_value(measured(out, field))
        print(f"    {field:<15} {note}")
        evidence = out["why"][field]
        if not evidence:
            print(
                "      (no absolute pre-InitHeap stores; absence proven by complete walk)"
            )
        for item in evidence:
            print(f"      {item.cite()}")
    print("  Ghidra truncation resolved by control/data separation:")
    print(f"    control terminates at {out['break'].cite()}")
    print(
        f"    stack load references post-break data mem[0x{out['stackTopBase']:08X}]="
        f"0x{out['stackTopWord']:08X}; it is read, never executed"
    )
    print(
        f"  derived plan: sp=fp=0x{out['sp']:08X}, gp=0x{out['gp']:08X}, "
        f"InitHeap(a0=0x{out['heapBase'] + 4:08X}, a1=0x{out['heapSize']:X}), "
        f"then gameMain=0x{out['gameMain']:08X}"
    )


def verify(
    exe_path: Path, shipping_path: Path = SHIPPED_FILE, require_identity: bool = True
) -> tuple[dict, list[str], list[str]]:
    if require_identity:
        require_target(exe_path)
    exe = load_exe(exe_path)
    out = analyze(exe)
    shipping = parse_shipping(shipping_path)
    failures, lines = compare_shipping(out, exe, shipping, exe_path)
    return out, failures, lines


def selftest(exe_path: Path, cross: Path | None) -> int:
    results: list[tuple[str, bool, str]] = []
    try:
        out, failures, lines = verify(exe_path)
        shipping = parse_shipping()
        exe = load_exe(exe_path)
    except Refused as exc:
        print(f"REFUSED: {exc}")
        return 2
    results.append(
        (
            "positive: shipping config equals instruction measurement",
            not failures,
            f"{len(lines)} constants compared; {'; '.join(failures) or '0 disagreements'}",
        )
    )

    scratch = ROOT / "scratch" / "raw"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="verify-crt0-", dir=scratch) as directory:
        work = Path(directory)
        original = bytearray(exe_path.read_bytes())
        bss_add = next(
            op
            for op in out["why"]["bssZeroHi"]
            if op.kind in ("addi", "addiu") and op.rt == 3
        )
        offset = 0x800 + bss_add.pc - exe.load
        mutated_word = (bss_add.word & 0xFFFF0000) | ((bss_add.imm + 4) & 0xFFFF)
        struct.pack_into("<I", original, offset, mutated_word)
        mutated = work / "mutated.exe"
        mutated.write_bytes(original)
        try:
            mutated_out = analyze(load_exe(mutated))
            mutation_failures, _ = compare_shipping(
                mutated_out, load_exe(mutated), shipping, mutated
            )
            changed = mutated_out["bssZeroHi"] != out["bssZeroHi"]
            named = any("kCrt0BssZeroHi" in failure for failure in mutation_failures)
            results.append(
                (
                    "mutation negative: changed BSS-bound instruction changes the measurement",
                    changed and named,
                    f"0x{out['bssZeroHi']:08X} -> 0x{mutated_out['bssZeroHi']:08X}; {len(mutation_failures)} disagreement(s)",
                )
            )
        except Refused as exc:
            results.append(
                (
                    "mutation negative: changed BSS-bound instruction changes the measurement",
                    False,
                    f"unexpected refusal: {exc}",
                )
            )

        poisoned_source = re.sub(
            r"(static\s+constexpr\s+uint32_t\s+kCrt0GameMain\s*=\s*)([^;]+)",
            r"\g<1>0x80999999u",
            shipping.raw,
            count=1,
        )
        poisoned = parse_shipping(SHIPPED_FILE, poisoned_source)
        source_failures, _ = compare_shipping(out, exe, poisoned, exe_path)
        results.append(
            (
                "mutation negative: changed shipping gameMain is rejected",
                any("kCrt0GameMain" in failure for failure in source_failures),
                f"{len(source_failures)} disagreement(s)",
            )
        )

        malformed = {
            "missing": work / "missing.exe",
            "zero-byte": work / "zero.exe",
            "garbage": work / "garbage.exe",
            "truncated": work / "truncated.exe",
        }
        malformed["zero-byte"].write_bytes(b"")
        malformed["garbage"].write_bytes(b"NOT-AN-EXE")
        malformed["truncated"].write_bytes(exe_path.read_bytes()[:-4])
        for label, path in malformed.items():
            try:
                load_exe(path)
                results.append(
                    (
                        f"malformed negative: {label} input refuses",
                        False,
                        "returned an image",
                    )
                )
            except Refused as exc:
                results.append(
                    (f"malformed negative: {label} input refuses", True, str(exc))
                )

    try:
        analyze(
            psexe.PsxExe(
                out["gameMain"],
                exe.gp,
                exe.load,
                exe.text_size,
                exe.sp_base,
                exe.sp_off,
                exe.text,
            )
        )
        results.append(
            (
                "wrong-entry negative: gameMain is not accepted as crt0",
                False,
                "returned a boot group",
            )
        )
    except Refused as exc:
        results.append(
            ("wrong-entry negative: gameMain is not accepted as crt0", True, str(exc))
        )

    if cross is not None:
        try:
            foreign = load_exe(cross)
            foreign_out = analyze(foreign)
            signature = ("bssZeroLo", "stackTopBase", "gp", "libcInit", "gameMain")
            differs = any(foreign_out[field] != out[field] for field in signature)
            try:
                require_target(cross)
                identity_refused = False
            except Refused:
                identity_refused = True
            results.append(
                (
                    "cross-binary negative: second executable has a different measured crt0 and is refused as target",
                    differs and identity_refused,
                    f"entry 0x{foreign.entry:08X} vs 0x{exe.entry:08X}; identity_refused={identity_refused}",
                )
            )
        except Refused as exc:
            results.append(
                (
                    "cross-binary negative: second executable is analysable",
                    False,
                    str(exc),
                )
            )
    else:
        print(
            "NOTE: cross-binary negative not run; pass --cross <other PS-X EXE> for that independent case."
        )

    passed = sum(ok for _, ok, _ in results)
    for name, ok, detail in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}\n         {detail}")
    print(f"verify_crt0 --selftest: {passed}/{len(results)} passed")
    return 0 if passed == len(results) else 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE, help="target PS-X EXE")
    parser.add_argument(
        "--shipped", type=Path, default=SHIPPED_FILE, help="shipping GameConfig source"
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare shipping constants to this measurement",
    )
    parser.add_argument(
        "--selftest", action="store_true", help="run positive and falsifying cases"
    )
    parser.add_argument(
        "--cross",
        type=Path,
        help="a real second game's PS-X EXE for the cross-binary negative",
    )
    args = parser.parse_args()
    try:
        if args.selftest:
            return selftest(args.exe, args.cross)
        require_target(args.exe)
        exe = load_exe(args.exe)
        out = analyze(exe)
        report(out, args.exe)
        if args.check:
            failures, lines = compare_shipping(
                out, exe, parse_shipping(args.shipped), args.exe
            )
            print(f"  shipping comparison ({len(lines)} constants):")
            for line in lines:
                print(f"    {line}")
            if failures:
                print("DISAGREEMENTS:")
                for failure in failures:
                    print(f"  - {failure}")
                return 1
        return 0
    except Refused as exc:
        print(f"verify_crt0: REFUSING — {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
