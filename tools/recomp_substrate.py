#!/usr/bin/env python3
"""Emit and verify Toy Story 2's executable plus its 21 measured code overlays.

The executable identity, crt0 group, and overlay destinations are checked by their existing
production instruments before the shipping psxport emitter runs. Generated code is gitignored and
never edited; this tool is the one authoritative producer and staleness gate for it.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import NoReturn

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import discdump
import extract_exe
from resolve_disc import resolve

EXE = ROOT / "scratch/bin/toystory2/SLUS_008.93"
FLAT = ROOT / "scratch/flat"
EXTRACTED = ROOT / "scratch/raw/toystory2-recomp-inputs"
GENERATED = ROOT / "generated"
SEEDS = ROOT / "game/recomp_seeds.json"
CONFIG = ROOT / "game/core/game_config.cpp"
MEASUREMENT = GENERATED / ".recomp.measurement.json"
HASH_FILE = GENERATED / ".recomp.hash"

OVERLAY_STEMS = ("BITS__MEMORY",) + tuple(
    f"LEVEL{level:02d}__LEVEL{part}" for level in range(1, 11) for part in ("", "1")
)
OVERLAY_PATHS = {
    "BITS__MEMORY": "BITS/MEMORY.BIN",
    **{
        f"LEVEL{level:02d}__LEVEL{part}": f"LEVEL{level:02d}/LEVEL{part}.BIN"
        for level in range(1, 11)
        for part in ("", "1")
    },
}

MAIN_COUNT = re.compile(
    r"^\[func\] functions: (?P<roots>\d+) seeds -> (?P<functions>\d+) recompiled",
    re.MULTILINE,
)
OVERLAY_COUNT = re.compile(
    r"^\[ov_[^]]+_func\] functions: (?P<roots>\d+) seeds -> "
    r"(?P<functions>\d+) recompiled",
    re.MULTILINE,
)


class Refused(RuntimeError):
    """Required evidence was absent or internally incomplete."""


class Mismatch(RuntimeError):
    """Measured retail input disagreed with a tracked shipping fact."""


@dataclass(frozen=True)
class Outcome:
    resident_roots: int
    resident_functions: int
    overlay_roots: int
    overlay_functions: int
    overlays: int
    sources: int
    version: str
    output_sha256: str


def psxport_dir() -> Path:
    return Path(os.environ.get("PSXPORT_DIR", ROOT / "external/psxport")).resolve()


def recompiler_sources() -> tuple[Path, ...]:
    directory = psxport_dir() / "tools/recomp"
    return tuple(directory / name for name in ("emit.py", "decode.py", "psexe.py"))


def fail(message: str) -> NoReturn:
    raise Refused(message)


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            command,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    except OSError as error:
        raise Refused(f"could not execute {command[0]}: {error}") from error


def require_retail(path: Path = EXE) -> dict[str, int]:
    expected_sha, expected_size, expected_name, reason = extract_exe.expected()
    if expected_sha is None or expected_size is None or expected_name is None:
        fail(f"target identity is unavailable: {reason}")
    try:
        data = path.read_bytes()
    except OSError as error:
        raise Refused(f"cannot read retail executable {path}: {error}") from error
    actual_sha = hashlib.sha1(data).hexdigest()
    if (
        path.name != expected_name
        or len(data) != expected_size
        or actual_sha != expected_sha
    ):
        raise Mismatch(
            f"{path} is not the tracked {expected_name}: got {len(data)} bytes sha1 "
            f"{actual_sha}, expected {expected_size} bytes sha1 {expected_sha}"
        )
    header = extract_exe.psexe_header(data)
    if header is None:
        raise Mismatch(f"identity-matched {path} is not a PS-X EXE")
    expected_header = {
        "pc0": 0x80082D60,
        "t_addr": 0x80010000,
        "t_size": 0x91800,
    }
    for name, expected in expected_header.items():
        if header[name] != expected:
            raise Mismatch(
                f"PS-X EXE {name} is 0x{header[name]:08X}, expected 0x{expected:08X}"
            )
    return header


def provision(disc_argument: str | None) -> None:
    disc = resolve(disc_argument, verbose=True)
    executable = run([sys.executable, "-B", str(TOOLS / "extract_exe.py"), disc])
    if executable.returncode:
        fail(
            f"executable provisioning refused with exit {executable.returncode}:\n"
            f"{executable.stdout.rstrip()}"
        )

    entries = discdump.listing(disc)
    by_path = {path.upper(): (path, size) for path, _lba, size in entries}
    if len(by_path) != len(entries):
        fail("disc listing contains case-insensitive duplicate paths")
    FLAT.mkdir(parents=True, exist_ok=True)
    extracted = 0
    total_bytes = 0
    for stem in OVERLAY_STEMS:
        wanted = OVERLAY_PATHS[stem]
        record = by_path.get(wanted.upper())
        if record is None:
            fail(f"verified disc listing omits required code module {wanted}")
        path, expected_size = record
        destination = discdump.get(disc, path, str(EXTRACTED / Path(path).parent))
        if not destination:
            fail(f"discdump could not extract required code module {path}")
        source = Path(destination)
        if source.stat().st_size != expected_size:
            fail(
                f"{path} extraction has {source.stat().st_size} bytes, listing says {expected_size}"
            )
        shutil.copyfile(source, FLAT / f"{stem}.BIN")
        extracted += 1
        total_bytes += expected_size
    print(
        f"[recomp] provisioned {extracted}/{len(OVERLAY_STEMS)} measured code modules, "
        f"{total_bytes} bytes"
    )


def require_overlays(directory: Path = FLAT) -> tuple[Path, ...]:
    paths = tuple(directory / f"{stem}.BIN" for stem in OVERLAY_STEMS)
    missing = [path.name for path in paths if not path.is_file()]
    if missing:
        fail(
            f"overlay corpus has {len(paths) - len(missing)}/{len(paths)} required modules; "
            f"missing {', '.join(missing)}"
        )
    total = sum(path.stat().st_size for path in paths)
    if total != 245_148:
        raise Mismatch(
            f"21-module overlay corpus totals {total} bytes, expected measured 245148"
        )
    return paths


def verify_maps() -> None:
    checks = (
        ("crt0", [sys.executable, "-B", str(TOOLS / "verify_crt0.py"), "--check"]),
        (
            "overlay map",
            [sys.executable, "-B", str(TOOLS / "overlay_map.py"), "--check"],
        ),
    )
    for name, command in checks:
        result = run(command)
        if result.returncode:
            raise Mismatch(
                f"shipping {name} verifier exited {result.returncode}:\n{result.stdout.rstrip()}"
            )
    print(
        "[recomp] verified executable identity, crt0 group, and LEVEL/MEMORY slot map"
    )


def staged_overlays(paths: tuple[Path, ...], destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    for path in paths:
        shutil.copyfile(path, destination / path.name)


def invoke_emitter(
    output: Path,
    overlays: Path,
    *,
    seeds: Path = SEEDS,
    executable: Path = EXE,
    shards: str | None = None,
) -> subprocess.CompletedProcess[str]:
    output.parent.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    environment["PYTHONDONTWRITEBYTECODE"] = "1"
    environment["PSXPORT_SHARDS"] = shards or os.environ.get("PSXPORT_SHARDS", "8")
    environment.pop("PSXPORT_USE_GHIDRA", None)
    emitter = recompiler_sources()[0]
    try:
        return subprocess.run(
            [
                sys.executable,
                "-B",
                str(emitter),
                str(executable),
                str(output),
                "--seeds",
                str(seeds),
                "--overlays",
                str(overlays),
            ],
            cwd=ROOT,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    except OSError as error:
        raise Refused(f"could not execute shipping emitter: {error}") from error


def source_manifest(directory: Path) -> tuple[str, ...]:
    manifest = directory / "rec_sources.cmake"
    try:
        text = manifest.read_text(encoding="utf-8")
    except OSError as error:
        raise Refused(f"generated source manifest is unavailable: {error}") from error
    names = tuple(re.findall(r"^\s+(\S+\.c)\s*$", text, re.MULTILINE))
    if not names or len(names) != len(set(names)):
        fail(
            "generated source manifest is empty or contains duplicate translation units"
        )
    missing = [name for name in names if not (directory / name).is_file()]
    if missing:
        fail("generated source manifest names missing files: " + ", ".join(missing))
    return names


def generated_digest(directory: Path, sources: tuple[str, ...]) -> str:
    digest = hashlib.sha256()
    names = tuple(
        dict.fromkeys(
            (
                *sources,
                "rec_decls.h",
                "overlay_table.h",
                "overlay_table.c",
                ".recomp_version",
            )
        )
    )
    for name in names:
        path = directory / name
        try:
            data = path.read_bytes()
        except OSError as error:
            raise Refused(
                f"generated output is unavailable for hashing: {error}"
            ) from error
        digest.update(name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(data)
    return digest.hexdigest()


def measure(directory: Path, output: str) -> Outcome:
    main = MAIN_COUNT.search(output)
    overlays = tuple(OVERLAY_COUNT.finditer(output))
    if main is None:
        fail("emitter succeeded without the resident seed/function denominator")
    if len(overlays) != len(OVERLAY_STEMS):
        fail(
            f"emitter reported {len(overlays)} overlay denominators, expected {len(OVERLAY_STEMS)}"
        )
    resident_roots = int(main.group("roots"))
    resident_functions = int(main.group("functions"))
    overlay_roots = sum(int(match.group("roots")) for match in overlays)
    overlay_functions = sum(int(match.group("functions")) for match in overlays)
    if resident_roots <= 0 or resident_functions < resident_roots:
        fail(
            f"invalid resident discovery denominator: {resident_roots} roots -> "
            f"{resident_functions} functions"
        )

    header = require_retail()
    try:
        declarations = (directory / "rec_decls.h").read_text(encoding="utf-8")
        table_header = (directory / "overlay_table.h").read_text(encoding="utf-8")
        table = (directory / "overlay_table.c").read_text(encoding="utf-8")
        version = (directory / ".recomp_version").read_text(encoding="utf-8").strip()
    except OSError as error:
        raise Refused(
            f"emitter omitted a required generated interface: {error}"
        ) from error
    required_symbols = (
        "void func_80082D60(Core*);",
        "void func_80089344(Core*);",
        "void func_8007A9E8(Core*);",
        "void main_dispatch(Core* c, uint32_t addr);",
        "void shard_set_override(uint32_t addr, OverrideFn fn);",
    )
    missing_symbols = [
        symbol for symbol in required_symbols if symbol not in declarations
    ]
    if missing_symbols:
        raise Mismatch("generated declarations omit " + ", ".join(missing_symbols))
    physical_lo = header["t_addr"] & 0x1FFF_FFFF
    physical_hi = (header["t_addr"] + header["t_size"]) & 0x1FFF_FFFF
    if (
        f"#define REC_MAIN_LO 0x{physical_lo:08X}u" not in table_header
        or f"#define REC_MAIN_HI 0x{physical_hi:08X}u" not in table_header
    ):
        raise Mismatch(
            f"generated resident range is not [0x{physical_lo:08X},0x{physical_hi:08X})"
        )
    if f"const int g_rec_overlay_count = {len(OVERLAY_STEMS)};" not in table:
        raise Mismatch("generated overlay table does not contain exactly 21 modules")
    missing_overlays = [stem for stem in OVERLAY_STEMS if f'"{stem}"' not in table]
    if missing_overlays:
        raise Mismatch("generated overlay table omits " + ", ".join(missing_overlays))
    sources = source_manifest(directory)
    if not version:
        fail("generated substrate has an empty recompiler version stamp")
    return Outcome(
        resident_roots,
        resident_functions,
        overlay_roots,
        overlay_functions,
        len(overlays),
        len(sources),
        version,
        generated_digest(directory, sources),
    )


def emit(directory: Path, paths: tuple[Path, ...], *, seeds: Path = SEEDS) -> Outcome:
    scratch = ROOT / "scratch/raw"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ts2-overlays-", dir=scratch) as temporary:
        overlay_directory = Path(temporary)
        staged_overlays(paths, overlay_directory)
        result = invoke_emitter(directory / "main.c", overlay_directory, seeds=seeds)
    if result.returncode:
        fail(f"shipping emitter exited {result.returncode}:\n{result.stdout.rstrip()}")
    return measure(directory, result.stdout)


def input_hash(paths: tuple[Path, ...]) -> str:
    digest = hashlib.sha256()
    inputs = (EXE, *paths, SEEDS, *recompiler_sources())
    for path in inputs:
        if not path.is_file():
            fail(f"required recompilation input is absent: {path}")
        digest.update(path.name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()


def generated_complete() -> bool:
    try:
        source_manifest(GENERATED)
    except Refused:
        return False
    return all(
        (GENERATED / name).is_file()
        for name in (
            "rec_decls.h",
            "overlay_table.h",
            "overlay_table.c",
            ".recomp_version",
        )
    )


def write_measurement(outcome: Outcome, identity: str) -> None:
    HASH_FILE.write_text(identity + "\n", encoding="utf-8")
    MEASUREMENT.write_text(
        json.dumps(outcome.__dict__, sort_keys=True) + "\n", encoding="utf-8"
    )


def read_measurement() -> Outcome:
    try:
        values = json.loads(MEASUREMENT.read_text(encoding="utf-8"))
        return Outcome(**values)
    except (KeyError, OSError, TypeError, json.JSONDecodeError) as error:
        raise Refused(f"generated measurement is unreadable: {error}") from error


def ensure(disc_argument: str | None) -> Outcome:
    provision(disc_argument)
    require_retail()
    paths = require_overlays()
    verify_maps()
    identity = input_hash(paths)
    force = os.environ.get("PSXPORT_FORCE_RECOMP", "") not in ("", "0")
    if (
        not force
        and HASH_FILE.is_file()
        and HASH_FILE.read_text(encoding="utf-8").strip() == identity
        and MEASUREMENT.is_file()
        and generated_complete()
    ):
        try:
            outcome = read_measurement()
            validated = measure(GENERATED, measurement_output(outcome))
        except Refused as error:
            print(f"[recomp] refreshing incomplete generated measurement: {error}")
        else:
            if validated != outcome:
                raise Mismatch(
                    f"stored generated measurement {outcome} disagrees with current interfaces "
                    f"{validated}"
                )
            print(
                f"[recomp] generated substrate is current (version {outcome.version})"
            )
            return outcome
    GENERATED.mkdir(parents=True, exist_ok=True)
    outcome = emit(GENERATED, paths)
    write_measurement(outcome, identity)
    return outcome


def measurement_output(outcome: Outcome) -> str:
    lines = [
        (
            f"[func] functions: {outcome.resident_roots} seeds -> "
            f"{outcome.resident_functions} recompiled"
        )
    ]
    roots_left = outcome.overlay_roots
    functions_left = outcome.overlay_functions
    for index in range(outcome.overlays):
        roots = roots_left if index == 0 else 0
        functions = functions_left if index == 0 else 0
        roots_left -= roots
        functions_left -= functions
        lines.append(
            f"[ov_measurement_{index}_func] functions: {roots} seeds -> {functions} recompiled"
        )
    return "\n".join(lines)


def check() -> Outcome:
    require_retail()
    paths = require_overlays()
    verify_maps()
    scratch = ROOT / "scratch/raw"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="ts2-recomp-check-", dir=scratch
    ) as temporary:
        return emit(Path(temporary), paths)


def selftest() -> bool:
    require_retail()
    paths = require_overlays()
    verify_maps()
    scratch = ROOT / "scratch/raw"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="ts2-recomp-positive-", dir=scratch
    ) as temporary:
        directory = Path(temporary)
        outcome = emit(directory, paths)
        print(
            "PASS positive: "
            f"{outcome.resident_roots} roots -> {outcome.resident_functions} resident functions; "
            f"{outcome.overlays} overlays -> {outcome.overlay_functions} functions"
        )
        sources = source_manifest(directory)
        source = directory / sources[0]
        source.write_bytes(source.read_bytes() + b"\n")
        if generated_digest(directory, sources) == outcome.output_sha256:
            raise Refused("generated-output digest accepted one changed source byte")
        print(
            "PASS negative: one changed generated source byte changes the output identity"
        )
    passed = 2
    with tempfile.TemporaryDirectory(
        prefix="ts2-recomp-negative-", dir=scratch
    ) as temporary:
        directory = Path(temporary)
        wrong_exe = directory / EXE.name
        data = bytearray(EXE.read_bytes())
        data[-1] ^= 1
        wrong_exe.write_bytes(data)
        try:
            require_retail(wrong_exe)
        except Mismatch:
            passed += 1
            print("PASS negative: one changed executable byte is refused by identity")

        bad_seeds = directory / "outside-text.json"
        bad_seeds.write_text('{"main": ["0x90000000"]}\n', encoding="utf-8")
        overlay_directory = directory / "overlays"
        staged_overlays(paths, overlay_directory)
        result = invoke_emitter(
            directory / "bad/main.c", overlay_directory, seeds=bad_seeds, shards="1"
        )
        if result.returncode and "seed(s) outside the module text" in result.stdout:
            passed += 1
            print("PASS negative: an out-of-text explicit seed is refused")

        incomplete_directory = directory / "incomplete"
        incomplete_directory.mkdir()
        for path in paths[:-1]:
            shutil.copyfile(path, incomplete_directory / path.name)
        try:
            require_overlays(incomplete_directory)
        except Refused:
            passed += 1
            print(
                "PASS negative: the 21-module denominator detects one omitted overlay"
            )
    print(f"SELFTEST {passed}/5")
    return passed == 5


def report(outcome: Outcome) -> None:
    print(
        "PASS: "
        f"{outcome.resident_roots} roots -> {outcome.resident_functions} resident functions; "
        f"{outcome.overlay_roots} roots -> {outcome.overlay_functions} functions across "
        f"{outcome.overlays} overlays; {outcome.sources} generated TUs; version {outcome.version}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", nargs="?", help="optional disc path for --ensure")
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--ensure", action="store_true")
    action.add_argument("--check", action="store_true")
    action.add_argument("--selftest", action="store_true")
    arguments = parser.parse_args()
    try:
        if arguments.selftest:
            return 0 if selftest() else 1
        outcome = ensure(arguments.disc) if arguments.ensure else check()
        report(outcome)
        return 0
    except Mismatch as error:
        print(f"MISMATCH: {error}", file=sys.stderr)
        return 1
    except (OSError, Refused) as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
