#!/usr/bin/env python3
"""Provision, build, and launch Toy Story 2's current generated-code port target."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
CYAN = "\033[1;36m"
RED = "\033[1;31m"
RESET = "\033[0m"


class LaunchError(RuntimeError):
    """A launcher precondition or required command failed."""


Command = Callable[..., subprocess.CompletedProcess[str]]
Which = Callable[[str], str | None]
Emit = Callable[[str], None]


def emit_line(message: str) -> None:
    print(message, flush=True)


def execute(
    args: Sequence[str],
    *,
    cwd: Path,
    env: Mapping[str, str] | None = None,
    capture_output: bool = False,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(args),
        cwd=cwd,
        env=None if env is None else dict(env),
        check=False,
        text=True,
        capture_output=capture_output,
    )


def checked(
    command: Command,
    args: Sequence[str],
    message: str,
    *,
    env: Mapping[str, str] | None = None,
    capture_output: bool = False,
) -> subprocess.CompletedProcess[str]:
    result = command(args, cwd=ROOT, env=env, capture_output=capture_output)
    if result.returncode:
        raise LaunchError(message)
    return result


def require(which: Which, name: str) -> str:
    path = which(name)
    if path is None:
        raise LaunchError(f"{name} not found")
    return path


def clang_compiler(command: Command, compiler: str, variable: str) -> None:
    result = command([compiler, "--version"], cwd=ROOT, capture_output=True)
    if result.returncode or "clang" not in (result.stdout or "").lower():
        raise LaunchError(f"{variable}={compiler} is not Clang")


def git_text(command: Command, git: str, framework: Path, *args: str) -> str:
    result = command([git, "-C", str(framework), *args], cwd=ROOT, capture_output=True)
    return (result.stdout or "").strip() if result.returncode == 0 else ""


def launch(
    argv: Sequence[str],
    *,
    environ: Mapping[str, str] = os.environ,
    which: Which = shutil.which,
    command: Command = execute,
    emit: Emit = emit_line,
) -> int:
    if len(argv) > 1:
        raise LaunchError('usage: ./run.sh ["/path/to/Toy Story 2 (USA).chd"]')
    disc = argv[0] if argv else None

    cmake = require(which, "cmake")
    pkg_config = require(which, "pkg-config")
    cc = environ.get("CC", "clang")
    cxx = environ.get("CXX", "clang++")
    require(which, cc)
    require(which, cxx)
    clang_compiler(command, cc, "CC")
    clang_compiler(command, cxx, "CXX")
    checked(
        command,
        [pkg_config, "--exists", "sdl3"],
        "SDL3 not found (Linux: SDL3-devel/libsdl3-dev; macOS: brew install sdl3)",
    )

    run_env = dict(environ)
    checked(
        command,
        [sys.executable, str(ROOT / "tools" / "psxport_sync.py"), "--auto"],
        "could not resolve external/psxport",
        env=run_env,
    )
    configured_framework = Path(environ.get("PSXPORT_DIR", "external/psxport"))
    framework = (
        configured_framework
        if configured_framework.is_absolute()
        else ROOT / configured_framework
    )
    framework = framework.resolve()
    if not (framework / "cmake" / "psxport.cmake").is_file():
        raise LaunchError(
            f"PSXPORT_DIR={configured_framework} is not a psxport checkout"
        )

    git = which("git")
    revision = (
        git_text(command, git, framework, "rev-parse", "--short", "HEAD") if git else ""
    )
    dirty = (
        bool(git_text(command, git, framework, "status", "--porcelain"))
        if git
        else False
    )
    suffix = " +dirty" if dirty else ""
    if configured_framework == Path("external/psxport"):
        emit(
            f"{CYAN}[run]{RESET} framework: external/psxport -> {framework} @ {revision or '?'}{suffix}"
        )
    else:
        emit(
            f"{CYAN}[run]{RESET} framework: *** {configured_framework} *** "
            f"(DEV CLONE {revision or '?'}{suffix}) — NOT the recorded pin"
        )

    sync_submodules = ROOT / "external" / "psxport" / "scripts" / "sync-submodules.sh"
    if git and sync_submodules.is_file():
        checked(command, ["bash", str(sync_submodules)], "submodule sync failed")

    run_env["PSXPORT_DIR"] = str(framework)
    recomp = [
        sys.executable,
        "-B",
        str(ROOT / "tools" / "recomp_substrate.py"),
        "--ensure",
    ]
    if disc is not None:
        recomp.append(disc)
        run_env["PSXPORT_TS2_DISC"] = disc
    checked(
        command,
        recomp,
        "verified executable/overlay provisioning or recompilation failed",
        env=run_env,
    )

    emit(f"{CYAN}[run]{RESET} building the Toy Story 2 port…")
    checked(
        command,
        [
            cmake,
            "-S",
            ".",
            "-B",
            "build",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DPSXPORT_DIR={framework}",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
        ],
        "cmake configure failed",
        capture_output=True,
    )
    checked(
        command,
        [
            cmake,
            "--build",
            "build",
            "-j",
            str(os.cpu_count() or 4),
            "--target",
            "toystory2_port",
        ],
        "port build failed",
    )
    run_env.setdefault("PSXPORT_ASSET_DIR", str(framework))
    executable = ROOT / "scratch" / "bin" / "toystory2_port"
    emit(f"{CYAN}[run]{RESET} launching {executable.relative_to(ROOT)}")
    return command(
        [str(executable), str(ROOT / "scratch/bin/toystory2/SLUS_008.93")],
        cwd=ROOT,
        env=run_env,
    ).returncode


def selftest() -> int:
    commands: list[list[str]] = []

    def fake_which(name: str) -> str | None:
        return f"/fake/{name}"

    def fake_command(
        args: Sequence[str],
        *,
        cwd: Path,
        env: Mapping[str, str] | None = None,
        capture_output: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        del cwd, env, capture_output
        words = list(args)
        commands.append(words)
        if words[-1:] == ["--version"]:
            return SimpleNamespace(returncode=0, stdout="clang version 22")
        if "rev-parse" in words:
            return SimpleNamespace(returncode=0, stdout="deadbeef\n")
        if "status" in words:
            return SimpleNamespace(returncode=0, stdout="")
        return SimpleNamespace(returncode=0, stdout="")

    passed = 0
    result = launch(
        [], environ={}, which=fake_which, command=fake_command, emit=lambda _: None
    )
    has_recomp = any(
        str(ROOT / "tools" / "recomp_substrate.py") in words and "--ensure" in words
        for words in commands
    )
    has_port = any(
        "--target" in words and "toystory2_port" in words for words in commands
    )
    launched = any(words and words[0].endswith("toystory2_port") for words in commands)
    if result == 0 and has_recomp and has_port and launched:
        passed += 1
        print(
            "[run-selftest] PASS positive: default route provisions, emits, builds, and launches the port"
        )
    else:
        print(
            "[run-selftest] FAIL positive: default route lost provisioning/build/refusal semantics"
        )

    try:
        launch(
            [],
            environ={},
            which=lambda name: None if name == "cmake" else fake_which(name),
            command=fake_command,
        )
    except LaunchError as exc:
        if str(exc) == "cmake not found":
            passed += 1
            print(
                "[run-selftest] PASS refusal: a missing required tool exits through the named error"
            )

    def provisioning_fails(
        args: Sequence[str],
        *,
        cwd: Path,
        env: Mapping[str, str] | None = None,
        capture_output: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        result = fake_command(args, cwd=cwd, env=env, capture_output=capture_output)
        if str(ROOT / "tools" / "recomp_substrate.py") in args:
            return SimpleNamespace(returncode=2, stdout="")
        return result

    before_provisioning = len(commands)
    try:
        launch(
            ["missing.chd"],
            environ={},
            which=fake_which,
            command=provisioning_fails,
            emit=lambda _: None,
        )
    except LaunchError as exc:
        new_commands = commands[before_provisioning:]
        configured = any("-S" in words and "-B" in words for words in new_commands)
        if (
            str(exc)
            == "verified executable/overlay provisioning or recompilation failed"
            and not configured
        ):
            passed += 1
            print(
                "[run-selftest] PASS refusal: provisioning failure stops before configure/build"
            )

    print(f"[run-selftest] {passed}/3 passed")
    return 0 if passed == 3 else 1


def main(argv: Sequence[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if args == ["--selftest"]:
        return selftest()
    try:
        return launch(args)
    except LaunchError as exc:
        print(f"{RED}[run] error:{RESET} {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
