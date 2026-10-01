"""Compare retail-derived overlay locations with their shipping C++ owners."""

import ast
import hashlib
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(ROOT, "game", "core", "guest_facts.h")
SHARED_IMAGE_HEADER = os.path.join(ROOT, "game", "overlay", "shared_slot_image.h")
LEVEL_IMAGE_HEADER = os.path.join(ROOT, "game", "overlay", "level_slot_image.h")
FLAT = os.path.join(ROOT, "scratch", "flat")
LEVEL_COUNT = 10
LEVEL_FILES = ("LEVEL.BIN", "LEVEL1.BIN")
_LEVEL_ROW = re.compile(
    r'\{\s*"((?:\\.|[^"\\])*)"\s*,\s*"((?:\\.|[^"\\])*)"\s*,\s*"((?:\\.|[^"\\])*)"\s*,\s*([0-9]+)u\s*,\s*"([0-9a-f]{64})"\s*\}'
)


def _constant(source, name):
    match = re.search(
        rf"\b{re.escape(name)}\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)u?\s*;", source
    )
    return int(match.group(1), 0) if match else None


def shared_slot_load_address(slot_header=None):
    """Read the shipping shared-slot load address; fail if its declaration is absent."""
    if slot_header is None:
        with open(SHARED_IMAGE_HEADER, encoding="utf-8") as source:
            slot_header = source.read()
    address = _constant(slot_header, "kLoadAddress")
    if address is None:
        raise ValueError(f"{SHARED_IMAGE_HEADER}: kLoadAddress has no numeric declaration")
    return address


def _retail_digest(slot_header, name):
    match = re.search(rf'\b{re.escape(name)}\s*=\s*"([0-9a-f]{{64}})"\s*;', slot_header)
    return match.group(1) if match else None


def _guest_path(slot_header, name):
    match = re.search(
        rf'\b{re.escape(name)}\s*=\s*("(?:\\.|[^"\\])*")\s*;', slot_header
    )
    return ast.literal_eval(match.group(1)) if match else None


def shipped_level_modules(level_header):
    """The LEVEL rows the shipping header declares: (guest path, disc path, name, bytes, sha256)."""
    return [
        (ast.literal_eval(f'"{g}"'), ast.literal_eval(f'"{d}"'), name, int(size), digest)
        for g, d, name, size, digest in _LEVEL_ROW.findall(level_header)
    ]


def retail_level_modules(flat):
    """The same rows derived from the retail disc files in the flat corpus; refuses a missing file."""
    rows = []
    for level in range(1, LEVEL_COUNT + 1):
        for name in LEVEL_FILES:
            path = os.path.join(flat, f"LEVEL{level:02d}__{name}")
            if not os.path.isfile(path):
                raise FileNotFoundError(f"{path}: the flat corpus lacks a retail LEVEL module; nothing was compared")
            with open(path, "rb") as handle:
                data = handle.read()
            rows.append(
                (
                    f"level{level:02d}\\{name.lower()}",
                    f"\\LEVEL{level:02d}\\{name};1",
                    f"LEVEL{level:02d}/{name}",
                    len(data),
                    hashlib.sha256(data).hexdigest(),
                )
            )
    return rows


def level_module_checks(measured, level_header, flat):
    """One check per shipped LEVEL module plus the slot geometry; each names its first differing field."""
    shipped = shipped_level_modules(level_header)
    expected = retail_level_modules(flat)
    checks = [
        ("LevelSlotImage kLoadAddress", _constant(level_header, "kLoadAddress"), measured["level_base"]),
        (
            "LevelSlotImage kWindowBytes",
            _constant(level_header, "kWindowBytes"),
            measured["memory_base"] - measured["level_base"],
        ),
        ("LevelSlotImage kModuleCount", _constant(level_header, "kModuleCount"), len(expected)),
        ("LevelSlotImage declared row count", len(shipped), len(expected)),
    ]
    for index, row in enumerate(expected):
        actual = shipped[index] if index < len(shipped) else None
        checks.append((f"LevelSlotImage {row[2]}", actual, row))
    return checks


def shipping_comparison(measured, out=sys.stdout, config=None, slot_header=None, level_header=None, flat=FLAT):
    """Require both shipping consumers to use the independently measured slots."""
    if config is None:
        with open(CONFIG, encoding="utf-8") as source:
            config = source.read()
    if slot_header is None:
        with open(SHARED_IMAGE_HEADER, encoding="utf-8") as source:
            slot_header = source.read()

    checks = [
        ("guest_facts kLevelOverlayBase", _constant(config, "kLevelOverlayBase"), measured["level_base"]),
        ("SharedSlotImage kLoadAddress", shared_slot_load_address(slot_header), measured["memory_base"]),
        ("SharedSlotImage kMemoryFileBytes", _constant(slot_header, "kMemoryFileBytes"), measured["memory_size"]),
        ("SharedSlotImage kFmvFileBytes", _constant(slot_header, "kFmvFileBytes"), measured["fmv_size"]),
        ("SharedSlotImage kMemoryGuestPath", _guest_path(slot_header, "kMemoryGuestPath"), measured["memory_guest_path"]),
        ("SharedSlotImage kFmvGuestPath", _guest_path(slot_header, "kFmvGuestPath"), measured["fmv_guest_path"]),
        (
            "SharedSlotImage kMemoryRetailSha256",
            _retail_digest(slot_header, "kMemoryRetailSha256"),
            measured["memory_sha256"],
        ),
        (
            "SharedSlotImage kFmvRetailSha256",
            _retail_digest(slot_header, "kFmvRetailSha256"),
            measured["fmv_sha256"],
        ),
    ]
    memory_alias = re.search(
        r"\bkMemoryOverlayBase\s*=\s*ts2::SharedSlotImage::kLoadAddress\s*;",
        config,
    )
    checks.append(("guest_facts MEMORY address alias", 1 if memory_alias else 0, 1))
    if level_header is None:
        with open(LEVEL_IMAGE_HEADER, encoding="utf-8") as source:
            level_header = source.read()
    checks.extend(level_module_checks(measured, level_header, flat))
    print("== shipping comparison (proven fields only) ==", file=out)
    failures = []
    for name, actual, expected in checks:
        ok = actual == expected
        print(
            "   %-4s %-43s ships %-32.60r measured %-.60r"
            % ("ok" if ok else "FAIL", name, actual, expected),
            file=out,
        )
        if not ok:
            failures.append(name)
    return failures


def shipping_selftest(contract, check):
    """Prove the shipping comparison detects an address drift in either consumer."""
    with open(CONFIG, encoding="utf-8") as source:
        config = source.read()
    with open(SHARED_IMAGE_HEADER, encoding="utf-8") as source:
        slot_header = source.read()
    changed_header = re.sub(
        r"(\bkLoadAddress\s*=\s*)0x[0-9A-Fa-f]+u?\s*;",
        r"\g<1>0x800D5D24u;",
        slot_header,
        count=1,
    )
    check(
        "SHIPPING NEGATIVE: a changed MEMORY address in the overlay owner is rejected",
        changed_header != slot_header
        and "SharedSlotImage kLoadAddress"
        in shipping_comparison(contract, io.StringIO(), config, changed_header),
        "mutated overlay address 0x800D5D24 must disagree with retail call flow",
    )
    changed_fmv_size = re.sub(
        r"(\bkFmvFileBytes\s*=\s*)[0-9]+u?\s*;",
        r"\g<1>510956u;",
        slot_header,
        count=1,
    )
    check(
        "SHIPPING NEGATIVE: a changed FMV file length is rejected",
        changed_fmv_size != slot_header
        and "SharedSlotImage kFmvFileBytes"
        in shipping_comparison(contract, io.StringIO(), config, changed_fmv_size),
        "mutated file length must disagree with the flat retail module",
    )
    changed_fmv_path = re.sub(
        r'(\bkFmvGuestPath\s*=\s*)"(?:\\.|[^"\\])*"',
        r'\g<1>"fmv\\\\fmv.bix"',
        slot_header,
        count=1,
    )
    check(
        "SHIPPING NEGATIVE: a changed FMV guest path is rejected",
        changed_fmv_path != slot_header
        and "SharedSlotImage kFmvGuestPath"
        in shipping_comparison(contract, io.StringIO(), config, changed_fmv_path),
        "mutated path must disagree with the executable call site",
    )
    changed_digest = re.sub(
        r'(\bkMemoryRetailSha256\s*=\s*")[0-9a-f]{64}("\s*;)',
        r'\g<1>' + "0" * 64 + r'\2',
        slot_header,
        count=1,
    )
    check(
        "SHIPPING NEGATIVE: a changed retail MEMORY digest is rejected",
        changed_digest != slot_header
        and "SharedSlotImage kMemoryRetailSha256"
        in shipping_comparison(contract, io.StringIO(), config, changed_digest),
        "mutated 64-character digest must disagree with the flat retail module",
    )
    changed_fmv_digest = re.sub(
        r'(\bkFmvRetailSha256\s*=\s*")[0-9a-f]{64}("\s*;)',
        r'\g<1>' + "0" * 64 + r'\2',
        slot_header,
        count=1,
    )
    check(
        "SHIPPING NEGATIVE: a changed retail FMV digest is rejected",
        changed_fmv_digest != slot_header
        and "SharedSlotImage kFmvRetailSha256"
        in shipping_comparison(contract, io.StringIO(), config, changed_fmv_digest),
        "mutated 64-character digest must disagree with the flat retail module",
    )
    changed_config = config.replace(
        "kMemoryOverlayBase = ts2::SharedSlotImage::kLoadAddress;",
        "kMemoryOverlayBase = kLevelOverlayBase;",
        1,
    )
    check(
        "SHIPPING NEGATIVE: an unbound MEMORY configuration address is rejected",
        changed_config != config
        and "guest_facts MEMORY address alias"
        in shipping_comparison(contract, io.StringIO(), changed_config, slot_header),
        "mutated facts must lose their overlay-owner alias",
    )
    with open(LEVEL_IMAGE_HEADER, encoding="utf-8") as source:
        level_header = source.read()
    changed_level_digest = re.sub(
        r'(LEVEL05/LEVEL1\.BIN",\s*[0-9]+u,\s*")[0-9a-f]{64}(")',
        r"\g<1>" + "0" * 64 + r"\2",
        level_header,
        count=1,
    )
    check(
        "SHIPPING NEGATIVE: a changed LEVEL module digest is rejected",
        changed_level_digest != level_header
        and "LevelSlotImage LEVEL05/LEVEL1.BIN"
        in shipping_comparison(contract, io.StringIO(), config, slot_header, changed_level_digest),
        "mutated digest must disagree with the flat retail LEVEL05/LEVEL1.BIN",
    )
    changed_level_size = re.sub(
        r'(LEVEL02/LEVEL\.BIN",\s*)19040u', r"\g<1>19044u", level_header, count=1
    )
    check(
        "SHIPPING NEGATIVE: a changed LEVEL module length is rejected",
        changed_level_size != level_header
        and "LevelSlotImage LEVEL02/LEVEL.BIN"
        in shipping_comparison(contract, io.StringIO(), config, slot_header, changed_level_size),
        "mutated length must disagree with the flat retail LEVEL02/LEVEL.BIN",
    )
    dropped_level_row = re.sub(
        r'    \{"level10\\\\level1\.bin".*?\},\n', "", level_header, count=1, flags=re.DOTALL
    )
    check(
        "SHIPPING NEGATIVE: a dropped LEVEL module row is rejected",
        dropped_level_row != level_header
        and "LevelSlotImage declared row count"
        in shipping_comparison(contract, io.StringIO(), config, slot_header, dropped_level_row),
        "a missing row would leave a retail module unauthenticated",
    )
