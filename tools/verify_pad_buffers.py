#!/usr/bin/env python3
"""Derive Toy Story 2's pad-buffer wiring from the retail executable.

This checker answers one narrow question: where does the shipping pad initializer register its two
output buffers, and does GameConfig route host samples to those exact destinations?  It derives the
answer from the unique call, the initializer's pointer stores/context step, and the game's independent
packet consumer.  Missing evidence is a refusal, never a zero-match pass.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = ROOT / "scratch" / "bin" / "toystory2" / "SLUS_008.93"
SHIPPING_SOURCE = ROOT / "game" / "core" / "game_config.cpp"

PAD_INIT = 0x800971D8
PAD_CONSUMER = 0x8003AC58
CONTEXT_POINTER_STORE = 0x800972B0
CONTEXT_STRIDE_STEP = 0x800972FC


class Refused(RuntimeError):
    """The requested assertion could not be made from the supplied evidence."""


@dataclass(frozen=True)
class Image:
    load: int
    words: tuple[int, ...]

    def word(self, pc: int) -> int:
        index = (pc - self.load) // 4
        if pc < self.load or pc % 4 or index >= len(self.words):
            raise Refused(f"address 0x{pc:08X} is outside the loaded executable words")
        return self.words[index]


@dataclass(frozen=True)
class Measurement:
    call_pc: int
    slot_buffers: tuple[int, int]
    pointer_table: int
    pointer_stride: int
    consumer_buffer: int
    words_scanned: int


CONSTANT_RE = re.compile(
    r"^static constexpr uint32_t (kPad\w+) = (0x[0-9A-Fa-f]+|[0-9]+)u;",
    re.MULTILINE,
)


def load_image(path: Path) -> Image:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise Refused(f"cannot read {path}: {exc}") from exc
    if len(data) < 0x800 or data[:8] != b"PS-X EXE":
        raise Refused(f"{path} is not a PS-X EXE")
    load, size = struct.unpack_from("<II", data, 0x18)
    payload = data[0x800 : 0x800 + size]
    if len(payload) != size or size % 4:
        raise Refused(
            f"header declares {size} loaded bytes but {len(payload)} aligned bytes are available"
        )
    return Image(load, struct.unpack(f"<{size // 4}I", payload))


def jump_target(pc: int, word: int) -> int:
    return ((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def formed_constant(lui: int, addiu: int, register: int) -> int:
    if lui >> 26 != 0x0F or (lui >> 16) & 31 != register:
        raise Refused(f"expected lui for r{register}, got 0x{lui:08X}")
    if (
        addiu >> 26 != 0x09
        or (addiu >> 21) & 31 != register
        or (addiu >> 16) & 31 != register
    ):
        raise Refused(f"expected addiu r{register},r{register},imm, got 0x{addiu:08X}")
    return (((lui & 0xFFFF) << 16) + signed16(addiu & 0xFFFF)) & 0xFFFFFFFF


def unique_pad_init_call(image: Image) -> tuple[int, int]:
    calls = []
    for index, word in enumerate(image.words):
        pc = image.load + index * 4
        if word >> 26 == 3 and jump_target(pc, word) == PAD_INIT:
            calls.append(index)
    if len(calls) != 1:
        raise Refused(
            f"scanned {len(image.words)} loaded words and found {len(calls)} calls to "
            f"the decompiled pad initializer 0x{PAD_INIT:08X}; exactly one is required"
        )
    index = calls[0]
    return image.load + index * 4, index


def measure(image: Image) -> Measurement:
    call_pc, call_index = unique_pad_init_call(image)
    if call_index < 4:
        raise Refused(
            "pad initializer call has no room for its argument-producing instructions"
        )
    slot0 = formed_constant(image.words[call_index - 4], image.words[call_index - 3], 4)
    slot1 = formed_constant(image.words[call_index - 2], image.words[call_index - 1], 5)

    context = formed_constant(image.word(0x80097208), image.word(0x8009720C), 16)
    store0 = image.word(CONTEXT_POINTER_STORE)
    store1 = image.word(CONTEXT_POINTER_STORE + 4)
    expected_store0 = (0x2B << 26) | (16 << 21) | (17 << 16)
    expected_store1 = (0x2B << 26) | (16 << 21) | (18 << 16)
    if store0 & 0xFFFF0000 != expected_store0 or store1 & 0xFFFF0000 != expected_store1:
        raise Refused(
            "pad initializer no longer stores saved a0/a1 through the decompiled s0 context base"
        )
    pointer0 = (context + signed16(store0 & 0xFFFF)) & 0xFFFFFFFF
    pointer1 = (context + signed16(store1 & 0xFFFF)) & 0xFFFFFFFF

    stride_word = image.word(CONTEXT_STRIDE_STEP)
    if (
        stride_word >> 26 != 0x09
        or (stride_word >> 21) & 31 != 4
        or (stride_word >> 16) & 31 != 4
    ):
        raise Refused(
            f"expected the per-port context step at 0x{CONTEXT_STRIDE_STEP:08X}, "
            f"got 0x{stride_word:08X}"
        )
    stride = signed16(stride_word & 0xFFFF)
    if stride <= 0 or pointer1 - pointer0 != stride:
        raise Refused(
            f"derived pointer fields 0x{pointer0:08X}/0x{pointer1:08X} disagree with "
            f"the positive context stride {stride}"
        )

    consumer = formed_constant(
        image.word(PAD_CONSUMER + 12), image.word(PAD_CONSUMER + 16), 17
    )
    return Measurement(
        call_pc, (slot0, slot1), pointer0, stride, consumer, len(image.words)
    )


def shipping_state(path: Path = SHIPPING_SOURCE) -> tuple[dict[str, int], list[str]]:
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise Refused(f"cannot read shipping source {path}: {exc}") from exc
    constants = {name: int(value, 0) for name, value in CONSTANT_RE.findall(text)}
    wanted = (
        "kPadSlot0Buffer",
        "kPadSlot1Buffer",
        "kPadDriverPointerTable",
        "kPadDriverContextStride",
    )
    missing = [name for name in wanted if name not in constants]
    if missing:
        raise Refused(f"shipping source does not define {', '.join(missing)}")

    uncommented = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.DOTALL)
    bindings = {
        "padSlot0Buf": "kPadSlot0Buffer",
        "padSlot1Buf": "kPadSlot1Buffer",
        "padDriverFn": "0",
        "padSlotPtrTable": "kPadDriverPointerTable",
        "padSlotPtrStride": "kPadDriverContextStride",
    }
    failures = []
    for field, expected in bindings.items():
        match = re.search(rf"\.{field}\s*=\s*([^,\n]+)", uncommented)
        actual = match.group(1).strip() if match else "<absent>"
        if actual != expected:
            failures.append(f"GameConfig .{field} ships {actual}, expected {expected}")
    return constants, failures


def compare(measured: Measurement, constants: dict[str, int]) -> list[str]:
    actual = {
        "kPadSlot0Buffer": measured.slot_buffers[0],
        "kPadSlot1Buffer": measured.slot_buffers[1],
        "kPadDriverPointerTable": measured.pointer_table,
        "kPadDriverContextStride": measured.pointer_stride,
    }
    failures = [
        f"{name}: retail derives 0x{value:08X}, shipping uses 0x{constants[name]:08X}"
        for name, value in actual.items()
        if value != constants[name]
    ]
    if measured.consumer_buffer != measured.slot_buffers[0]:
        failures.append(
            f"consumer reads 0x{measured.consumer_buffer:08X}, not slot 0 "
            f"0x{measured.slot_buffers[0]:08X}"
        )
    return failures


def check(image: Image, verbose: bool = True) -> list[str]:
    measured = measure(image)
    constants, config_failures = shipping_state()
    failures = [*config_failures, *compare(measured, constants)]
    if verbose:
        print(
            f"scanned {measured.words_scanned} loaded words; unique pad-init call "
            f"0x{measured.call_pc:08X} passes slot buffers "
            f"0x{measured.slot_buffers[0]:08X}/0x{measured.slot_buffers[1]:08X}"
        )
        print(
            f"driver pointer fields begin at 0x{measured.pointer_table:08X} with "
            f"0x{measured.pointer_stride:X}-byte context stride"
        )
        print(
            f"independent game packet consumer reads 0x{measured.consumer_buffer:08X}"
        )
        for failure in failures:
            print(f"MISMATCH: {failure}")
        if not failures:
            print(
                "MATCH: retail initializer, driver contexts, consumer and all 5 GameConfig bindings agree"
            )
        print(
            "blind spot: this static gate proves destinations and registration shape, not that a "
            "particular host input changes live gameplay"
        )
    return failures


def replace_word(image: Image, pc: int, word: int) -> Image:
    words = list(image.words)
    words[(pc - image.load) // 4] = word
    return Image(image.load, tuple(words))


def selftest(image: Image) -> bool:
    cases: list[tuple[str, bool]] = [
        ("positive: retail evidence matches shipping", not check(image, False))
    ]

    call_pc, _ = unique_pad_init_call(image)
    mutated = replace_word(image, call_pc - 12, image.word(call_pc - 12) ^ 1)
    cases.append(
        ("negative: changed slot-0 argument is rejected", bool(check(mutated, False)))
    )

    mutated = replace_word(
        image, CONTEXT_STRIDE_STEP, image.word(CONTEXT_STRIDE_STEP) ^ 1
    )
    try:
        measure(mutated)
        cases.append(("negative: changed context stride is refused", False))
    except Refused:
        cases.append(("negative: changed context stride is refused", True))

    mutated = replace_word(image, PAD_CONSUMER + 16, image.word(PAD_CONSUMER + 16) ^ 1)
    cases.append(
        ("negative: changed consumer buffer is rejected", bool(check(mutated, False)))
    )

    mutated = replace_word(image, call_pc, image.word(call_pc) ^ 1)
    try:
        measure(mutated)
        cases.append(("negative: removed unique init call is refused", False))
    except Refused:
        cases.append(("negative: removed unique init call is refused", True))

    for label, passed in cases:
        print(f"{'PASS' if passed else 'FAIL'}: {label}")
    print(f"selftest: {sum(passed for _, passed in cases)}/{len(cases)} cases")
    return all(passed for _, passed in cases)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--exe", type=Path, default=DEFAULT_EXE, help="retail PS-X EXE to inspect"
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--check",
        action="store_true",
        help="compare derived retail facts with shipping config",
    )
    mode.add_argument(
        "--selftest",
        action="store_true",
        help="exercise positive, negative and refusal classes",
    )
    args = parser.parse_args()
    try:
        image = load_image(args.exe)
        if args.selftest:
            return 0 if selftest(image) else 1
        return 1 if check(image) else 0
    except Refused as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
