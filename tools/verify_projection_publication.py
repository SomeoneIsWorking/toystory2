#!/usr/bin/env python3
"""Verify Toy Story 2's retail projection-publication leaves and shipping HLE wiring.

The native camera, widescreen policy, and future transform interpolation all need the projection the
game actually authored. This checker proves the two linked libgte leaves directly from SLUS_008.93,
proves the title initializer calls them with its retail values, and checks that GameConfig binds them
through the measured shared SDK window. Registration remains exact-address and is covered at the C++
boundary; the window is only the admission envelope. Missing or changed evidence is a refusal, never
a match.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EXE = ROOT / "scratch" / "bin" / "toystory2" / "SLUS_008.93"
SHIPPING_SOURCE = ROOT / "game" / "core" / "game_config.cpp"

EXPECTED_SHA1 = "f90c9cd6b4fc9845adfe34e306b7df393bf9154c"
GRAPHICS_INIT = 0x8003A650
SET_GEOM_OFFSET = 0x80083CD4
SET_GEOM_SCREEN = 0x80083CF4
PROJECTION_LEAVES_END = 0x80083D00
EXPECTED_INIT_VALUES = (256, 120, 160)

OFFSET_BODY = (
    0x00042400,  # sll a0,a0,16
    0x00052C00,  # sll a1,a1,16
    0x48C4C000,  # ctc2 a0,CR24 (OFX)
    0x48C5C800,  # ctc2 a1,CR25 (OFY)
    0x03E00008,  # jr ra
    0x00000000,
)
SCREEN_BODY = (
    0x48C4D000,  # ctc2 a0,CR26 (H)
    0x03E00008,  # jr ra
    0x00000000,
)

CONSTANT_RE = re.compile(
    r"^static constexpr uint32_t (k(?:ProjectionLeaves(?:Lo|Hi)|SetGeom(?:Offset|Screen))) "
    r"= (0x[0-9A-Fa-f]+|[0-9]+)u;",
    re.MULTILINE,
)


class Refused(RuntimeError):
    """The supplied bytes do not establish the requested fact."""


@dataclass(frozen=True)
class Image:
    load: int
    words: tuple[int, ...]
    sha1: str

    def word(self, pc: int) -> int:
        index = (pc - self.load) // 4
        if pc < self.load or pc % 4 or index < 0 or index >= len(self.words):
            raise Refused(f"address 0x{pc:08X} is outside the loaded executable words")
        return self.words[index]


@dataclass(frozen=True)
class Measurement:
    offset: int
    screen: int
    window_lo: int
    window_hi: int
    init_values: tuple[int, int, int]


def load_image(path: Path) -> Image:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise Refused(f"cannot read {path}: {exc}") from exc
    digest = hashlib.sha1(data).hexdigest()
    if digest != EXPECTED_SHA1:
        raise Refused(f"{path} sha1 is {digest}, expected SLUS_008.93 {EXPECTED_SHA1}")
    if len(data) < 0x800 or data[:8] != b"PS-X EXE":
        raise Refused(f"{path} is not a PS-X EXE")
    load, size = struct.unpack_from("<II", data, 0x18)
    payload = data[0x800 : 0x800 + size]
    if len(payload) != size or size % 4:
        raise Refused(
            f"header declares {size} loaded bytes but {len(payload)} aligned bytes are available"
        )
    return Image(load, struct.unpack(f"<{size // 4}I", payload), digest)


def jump_target(pc: int, word: int) -> int:
    if word >> 26 != 3:
        raise Refused(f"expected jal at 0x{pc:08X}, got 0x{word:08X}")
    return ((pc + 4) & 0xF0000000) | ((word & 0x03FFFFFF) << 2)


def immediate(image: Image, pc: int, register: int) -> int:
    word = image.word(pc)
    if word >> 26 != 0x09 or (word >> 21) & 31 != 0 or (word >> 16) & 31 != register:
        raise Refused(
            f"expected addiu r{register},zero,imm at 0x{pc:08X}, got 0x{word:08X}"
        )
    value = word & 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def require_words(
    image: Image, start: int, expected: tuple[int, ...], label: str
) -> None:
    actual = tuple(image.word(start + index * 4) for index in range(len(expected)))
    if actual != expected:
        mismatch = next(
            index
            for index, pair in enumerate(zip(actual, expected))
            if pair[0] != pair[1]
        )
        pc = start + mismatch * 4
        raise Refused(
            f"{label} differs at 0x{pc:08X}: retail has 0x{actual[mismatch]:08X}, "
            f"expected 0x{expected[mismatch]:08X}"
        )


def measure(image: Image) -> Measurement:
    require_words(image, SET_GEOM_OFFSET, OFFSET_BODY, "SetGeomOffset leaf")
    require_words(image, SET_GEOM_SCREEN, SCREEN_BODY, "SetGeomScreen leaf")

    offset_call = GRAPHICS_INIT + 0x1C
    screen_call = GRAPHICS_INIT + 0x24
    if jump_target(offset_call, image.word(offset_call)) != SET_GEOM_OFFSET:
        raise Refused(
            "graphics initializer no longer calls the measured SetGeomOffset leaf"
        )
    if jump_target(screen_call, image.word(screen_call)) != SET_GEOM_SCREEN:
        raise Refused(
            "graphics initializer no longer calls the measured SetGeomScreen leaf"
        )

    ofx = immediate(image, offset_call - 4, 4)
    # The second argument is intentionally in the jal delay slot.
    ofy = immediate(image, offset_call + 4, 5)
    # SetGeomScreen's only argument is intentionally in its jal delay slot.
    h = immediate(image, screen_call + 4, 4)
    if (ofx, ofy, h) != EXPECTED_INIT_VALUES:
        raise Refused(
            f"graphics initializer publishes ({ofx},{ofy},{h}), expected the measured retail "
            f"values {EXPECTED_INIT_VALUES}"
        )

    return Measurement(
        offset=SET_GEOM_OFFSET,
        screen=SET_GEOM_SCREEN,
        window_lo=SET_GEOM_OFFSET,
        window_hi=PROJECTION_LEAVES_END,
        init_values=(ofx, ofy, h),
    )


def shipping_state(text: str) -> tuple[dict[str, int], list[str]]:
    constants = {name: int(value, 0) for name, value in CONSTANT_RE.findall(text)}
    expected = {
        "kProjectionLeavesLo": SET_GEOM_OFFSET,
        "kProjectionLeavesHi": PROJECTION_LEAVES_END,
        "kSetGeomOffset": SET_GEOM_OFFSET,
        "kSetGeomScreen": SET_GEOM_SCREEN,
    }
    failures = []
    for name, value in expected.items():
        if name not in constants:
            failures.append(f"shipping source does not define {name}")
        elif constants[name] != value:
            failures.append(
                f"{name}: retail derives 0x{value:08X}, shipping uses 0x{constants[name]:08X}"
            )

    uncommented = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.DOTALL)
    bindings = (
        (
            "windowLo",
            "{kSdkGraphicsWindowLo, ts2::cd::kStockLibcdLayout.libraryWindowLo}",
            r"\{[^}\n]+\}",
        ),
        (
            "windowHi",
            "{kSdkGraphicsWindowHi, ts2::cd::kStockLibcdLayout.libraryWindowHi}",
            r"\{[^}\n]+\}",
        ),
        ("setGeomOffset", "kSetGeomOffset", r"[^,}\n]+"),
        ("setGeomScreen", "kSetGeomScreen", r"[^,}\n]+"),
    )
    for field, expected_value, value_pattern in bindings:
        match = re.search(rf"\.{field}\s*=\s*({value_pattern})", uncommented)
        actual = re.sub(r"\s+", "", match.group(1)) if match else "<absent>"
        wanted = re.sub(r"\s+", "", expected_value)
        if actual != wanted:
            failures.append(f"GameConfig .{field} ships {actual}, expected {wanted}")
    for relation in (
        "static constexpr uint32_t kSdkGraphicsWindowLo = kProjectionLeavesLo;",
        "static constexpr uint32_t kSdkGraphicsWindowHi = kVSyncBodyHi;",
    ):
        if relation not in uncommented:
            failures.append(f"shipping source is missing measured SDK window relation: {relation}")
    return constants, failures


def check(image: Image, source: str, verbose: bool = True) -> list[str]:
    measured = measure(image)
    _, failures = shipping_state(source)
    if verbose:
        ofx, ofy, h = measured.init_values
        print(
            f"identity: sha1 {image.sha1}; SetGeomOffset 0x{measured.offset:08X}, "
            f"SetGeomScreen 0x{measured.screen:08X}"
        )
        print(
            f"graphics init 0x{GRAPHICS_INIT:08X} publishes OFX={ofx}, OFY={ofy}, H={h}"
        )
        print(
            f"shipping HLE window [0x{measured.window_lo:08X},0x{measured.window_hi:08X})"
        )
        for failure in failures:
            print(f"MISMATCH: {failure}")
        if not failures:
            print(
                "MATCH: retail leaf bodies, initializer calls, constants, and HLE bindings agree"
            )
        print(
            "blind spot: static evidence proves publication wiring, not that a serialized live run "
            "has reached these leaves or that any native producer consumes the recorded projection"
        )
    return failures


def replace_word(image: Image, pc: int, word: int) -> Image:
    words = list(image.words)
    words[(pc - image.load) // 4] = word
    return Image(image.load, tuple(words), image.sha1)


def selftest(image: Image, source: str) -> bool:
    cases: list[tuple[str, bool]] = [
        ("positive: retail evidence matches shipping", not check(image, source, False))
    ]

    for label, pc in (
        ("changed SetGeomOffset body is refused", SET_GEOM_OFFSET + 8),
        ("changed SetGeomScreen body is refused", SET_GEOM_SCREEN),
        ("changed initializer call is refused", GRAPHICS_INIT + 0x1C),
        ("changed initializer argument is refused", GRAPHICS_INIT + 0x28),
    ):
        mutated = replace_word(image, pc, image.word(pc) ^ 1)
        try:
            measure(mutated)
            cases.append((f"negative: {label}", False))
        except Refused:
            cases.append((f"negative: {label}", True))

    mutated_source = source.replace(
        ".setGeomScreen = kSetGeomScreen", ".setGeomScreen = kSetGeomOffset", 1
    )
    cases.append(
        (
            "negative: changed shipping binding is rejected",
            bool(check(image, mutated_source, False)),
        )
    )

    for label, passed in cases:
        print(f"{'PASS' if passed else 'FAIL'}: {label}")
    print(f"selftest: {sum(passed for _, passed in cases)}/{len(cases)} cases")
    return all(passed for _, passed in cases)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    try:
        image = load_image(args.exe)
        source = SHIPPING_SOURCE.read_text(encoding="utf-8")
        if args.selftest:
            return 0 if selftest(image, source) else 1
        return 1 if check(image, source) else 0
    except (OSError, Refused) as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
