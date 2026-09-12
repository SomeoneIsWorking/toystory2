"""Compare retail-derived overlay locations with their shipping C++ owners."""

import ast
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(ROOT, "game", "core", "game_config.cpp")
SHARED_IMAGE_HEADER = os.path.join(ROOT, "game", "overlay", "shared_slot_image.h")


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


def shipping_comparison(measured, out=sys.stdout, config=None, slot_header=None):
    """Require both shipping consumers to use the independently measured slots."""
    if config is None:
        with open(CONFIG, encoding="utf-8") as source:
            config = source.read()
    if slot_header is None:
        with open(SHARED_IMAGE_HEADER, encoding="utf-8") as source:
            slot_header = source.read()

    checks = [
        ("game_config kLevelOverlayBase", _constant(config, "kLevelOverlayBase"), measured["level_base"]),
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
    checks.append(("game_config MEMORY address alias", 1 if memory_alias else 0, 1))
    slot_text = re.search(r"\.overlaySlots\s*=\s*\{\{(.*?)\}\},", config, re.DOTALL)
    slots_ok = bool(
        slot_text
        and re.search(r'\{\s*kLevelOverlayBase\s*,\s*"LEVEL"\s*\}', slot_text.group(0))
        and re.search(r'\{\s*kMemoryOverlayBase\s*,\s*"MEMORY"\s*\}', slot_text.group(0))
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
        and "game_config MEMORY address alias"
        in shipping_comparison(contract, io.StringIO(), changed_config, slot_header),
        "mutated GameConfig must lose its overlay-owner alias",
    )
