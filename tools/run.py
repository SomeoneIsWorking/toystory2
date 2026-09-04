#!/usr/bin/env python3
"""Resolve, build, and launch Toy Story 2's native/dynarec product."""

from __future__ import annotations

import argparse
import os
import platform
import runpy
import shutil
import subprocess
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import TextIO

ROOT = Path(__file__).resolve().parents[1]
PLAYER_BUILD_DIR = "build/player"
CYAN = "\033[1;36m"
RED = "\033[1;31m"
RESET = "\033[0m"


class LaunchError(RuntimeError):
    """A user-facing launcher refusal."""


class Host:
    """Narrow injectable seam around host discovery and process execution."""

    @staticmethod
    def which(name: str) -> str | None:
        return shutil.which(name)

    @staticmethod
    def run(args: Sequence[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
        return subprocess.run(list(args), check=False, **kwargs)

    @staticmethod
    def system() -> str:
        return platform.system()

    @staticmethod
    def linux_distribution() -> str:
        try:
            for line in Path("/etc/os-release").read_text().splitlines():
                key, separator, value = line.partition("=")
                if separator and key == "ID":
                    return value.strip().strip('"').lower()
        except OSError:
            pass
        return "unknown"


def emit_line(message: str, stdout: TextIO) -> None:
    print(f"{CYAN}[run]{RESET} {message}", file=stdout, flush=True)


def package_command(host: Host, package: str) -> str | None:
    system = host.system()
    if system == "Darwin":
        commands = {
            "cmake": "brew install cmake",
            "git": "brew install git",
            "glslc": "brew install shaderc",
            "pkg-config": "brew install pkg-config",
            "sdl3": "brew install sdl3",
            "sdl3-image": "brew install sdl3_image",
            "freetype": "brew install freetype",
            "zlib": "brew install zlib",
            "zstd": "brew install zstd",
            "compiler": "xcode-select --install",
        }
        return commands[package]
    if system == "Windows":
        commands = {
            "cmake": "winget install Kitware.CMake",
            "git": "winget install Git.Git",
            "glslc": "winget install KhronosGroup.VulkanSDK",
            "pkg-config": "vcpkg install pkgconf",
            "sdl3": "vcpkg install sdl3",
            "sdl3-image": "vcpkg install sdl3-image",
            "freetype": "vcpkg install freetype",
            "zlib": "vcpkg install zlib",
            "zstd": "vcpkg install zstd",
            "compiler": (
                "winget install Microsoft.VisualStudio.2022.BuildTools --override "
                '"--wait --passive --add Microsoft.VisualStudio.Workload.VCTools '
                '--includeRecommended"'
            ),
        }
        return commands[package]
    if system != "Linux":
        return None

    distribution = host.linux_distribution()
    if distribution in {"fedora", "rhel", "centos", "rocky", "almalinux"}:
        commands = {
            "cmake": "sudo dnf install cmake",
            "git": "sudo dnf install git",
            "glslc": "sudo dnf install glslc",
            "pkg-config": "sudo dnf install pkgconf-pkg-config",
            "sdl3": "sudo dnf install SDL3-devel",
            "sdl3-image": "sudo dnf install SDL3_image-devel",
            "freetype": "sudo dnf install freetype-devel",
            "zlib": "sudo dnf install zlib-devel",
            "zstd": "sudo dnf install libzstd-devel",
            "compiler": "sudo dnf install gcc gcc-c++",
        }
        return commands[package]
    if distribution in {"debian", "ubuntu", "linuxmint", "pop"}:
        commands = {
            "cmake": "sudo apt install cmake",
            "git": "sudo apt install git",
            "glslc": "sudo apt install glslc",
            "pkg-config": "sudo apt install pkg-config",
            "sdl3": "sudo apt install libsdl3-dev",
            "sdl3-image": "sudo apt install libsdl3-image-dev",
            "freetype": "sudo apt install libfreetype-dev",
            "zlib": "sudo apt install zlib1g-dev",
            "zstd": "sudo apt install libzstd-dev",
            "compiler": "sudo apt install build-essential",
        }
        return commands[package]
    return None


def missing_dependency(host: Host, name: str, package: str) -> LaunchError:
    command = package_command(host, package)
    if command:
        return LaunchError(f"{name} not found. Install it with: {command}")
    system = host.system()
    distribution = host.linux_distribution() if system == "Linux" else "unknown"
    return LaunchError(
        f"{name} not found, and no package command is recorded for "
        f"{system}/{distribution}; install it with your platform package manager and rerun"
    )


def require_tool(host: Host, name: str, package: str | None = None) -> str:
    resolved = host.which(name)
    if resolved is None:
        raise missing_dependency(host, name, package or name)
    return resolved


def run_stage(
    host: Host,
    args: Sequence[str],
    failure: str,
    *,
    root: Path,
    env: Mapping[str, str],
) -> subprocess.CompletedProcess[str]:
    try:
        result = host.run(list(args), cwd=root, env=dict(env))
    except OSError as exc:
        raise LaunchError(f"{failure}: {exc}") from exc
    if result.returncode != 0:
        raise LaunchError(failure)
    return result


def command_output(host: Host, args: Sequence[str], *, root: Path) -> tuple[int, str]:
    try:
        result = host.run(
            list(args),
            cwd=root,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except OSError:
        return 127, ""
    return result.returncode, (result.stdout or "").strip()


def require_library(
    host: Host,
    pkg_config: str,
    module: str,
    label: str,
    package: str,
    *,
    root: Path,
    env: Mapping[str, str],
) -> None:
    run_stage(
        host,
        [pkg_config, "--exists", module],
        str(missing_dependency(host, label, package)),
        root=root,
        env=env,
    )


def compiler_arguments(environment: Mapping[str, str]) -> list[str]:
    """Pass configured compiler paths through without classifying their identity."""

    arguments: list[str] = []
    if compiler := environment.get("CC"):
        arguments.append(f"-DCMAKE_C_COMPILER={compiler}")
    if compiler := environment.get("CXX"):
        arguments.append(f"-DCMAKE_CXX_COMPILER={compiler}")
    return arguments


def framework_revision(host: Host, path: Path, *, root: Path) -> tuple[str, bool]:
    returncode, revision = command_output(
        host, ["git", "-C", str(path), "rev-parse", "--short", "HEAD"], root=root
    )
    if returncode != 0 or not revision:
        revision = "?"
    _, status = command_output(
        host, ["git", "-C", str(path), "status", "--porcelain"], root=root
    )
    return revision, bool(status)


def announce_framework(
    host: Host, setting: str, path: Path, *, root: Path, stdout: TextIO
) -> None:
    revision, dirty = framework_revision(host, path, root=root)
    suffix = " +dirty" if dirty else ""
    if setting == "external/psxport":
        emit_line(
            f"framework: external/psxport -> {path.resolve()} @ {revision}{suffix}",
            stdout,
        )
        return
    emit_line(
        f"framework: *** {setting} *** (DEV CLONE {revision}{suffix}) "
        "— NOT the recorded pin",
        stdout,
    )


def argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", nargs="?", help="path to the user's Toy Story 2 disc")
    parser.add_argument(
        "--prepare-only",
        action="store_true",
        help="build the current product without launching it",
    )
    return parser


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    return argument_parser().parse_args(list(argv))


def launch(
    argv: Sequence[str],
    *,
    environ: Mapping[str, str] | None = None,
    host: Host | None = None,
    root: Path = ROOT,
    python_executable: str = sys.executable,
    stdout: TextIO = sys.stdout,
    stderr: TextIO = sys.stderr,
) -> int:
    """Run the shipping path; injected host seams keep its tests hermetic."""

    # Help is a discovery-free launcher operation. Handle argparse's two standard spellings before
    # checking tools, resolving the framework, or resolving game media.
    if "-h" in argv or "--help" in argv:
        argument_parser().print_help(file=stdout)
        return 0

    environment = dict(os.environ if environ is None else environ)
    machine = host or Host()
    try:
        options = parse_args(argv)
        cmake = require_tool(machine, "cmake")
        require_tool(machine, "git")
        require_tool(machine, "glslc")
        pkg_config = require_tool(machine, "pkg-config")
        for module, label, package in (
            ("sdl3", "SDL3 development files", "sdl3"),
            ("sdl3-image", "SDL3_image development files", "sdl3-image"),
            ("freetype2", "FreeType development files", "freetype"),
        ):
            require_library(
                machine,
                pkg_config,
                module,
                label,
                package,
                root=root,
                env=environment,
            )

        framework_setting = environment.get("PSXPORT_DIR") or "external/psxport"
        if not environment.get("PSXPORT_DIR"):
            run_stage(
                machine,
                [python_executable, "tools/psxport_sync.py", "--auto"],
                "could not resolve external/psxport",
                root=root,
                env=environment,
            )
        framework = Path(framework_setting)
        if not framework.is_absolute():
            framework = root / framework
        framework = framework.resolve()
        if not (framework / "cmake" / "psxport.cmake").is_file():
            raise LaunchError(
                f"PSXPORT_DIR={framework_setting} is not a psxport checkout"
            )
        announce_framework(
            machine, framework_setting, framework, root=root, stdout=stdout
        )

        run_environment = dict(environment)
        run_environment["PSXPORT_DIR"] = str(framework)
        if options.disc:
            run_environment["PSXPORT_TS2_DISC"] = options.disc

        emit_line("building the Toy Story 2 port…", stdout)
        configure = [
            cmake,
            "-S",
            ".",
            "-B",
            PLAYER_BUILD_DIR,
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_TESTING=OFF",
            f"-DPSXPORT_DIR={framework}",
            f"-DPython3_EXECUTABLE={python_executable}",
            *compiler_arguments(environment),
        ]
        configure_failure = "cmake configure failed"
        configure_hints = [
            (label, package_command(machine, package))
            for label, package in (
                ("C/C++ toolchain", "compiler"),
                ("zlib development files", "zlib"),
                ("zstd development files", "zstd"),
            )
        ]
        actionable_hints = [
            f"{label}: {command}"
            for label, command in configure_hints
            if command is not None
        ]
        if actionable_hints:
            configure_failure += (
                ". Install any dependency CMake named with: "
                + "; ".join(actionable_hints)
            )
        run_stage(
            machine,
            configure,
            configure_failure,
            root=root,
            env=run_environment,
        )
        run_stage(
            machine,
            [
                cmake,
                "--build",
                PLAYER_BUILD_DIR,
                "-j",
                str(os.cpu_count() or 4),
                "--target",
                "toystory2_port",
            ],
            "port build failed",
            root=root,
            env=run_environment,
        )
    except LaunchError as exc:
        print(f"{RED}[run] error:{RESET} {exc}", file=stderr)
        return 1

    if options.prepare_only:
        emit_line("Toy Story 2 is built and ready.", stdout)
        return 0

    executable = root / PLAYER_BUILD_DIR / "bin" / "toystory2_port"
    policy = runpy.run_path(str(framework / "tools/port/launch_environment.py"))
    run_environment = policy["player_environment"](run_environment)
    run_environment.setdefault("PSXPORT_ASSET_DIR", str(framework))
    emit_line(f"launching {executable.relative_to(root)}", stdout)
    try:
        result = machine.run([str(executable)], cwd=root, env=run_environment)
    except OSError as exc:
        print(f"{RED}[run] error:{RESET} launch failed: {exc}", file=stderr)
        return 1
    return result.returncode


def main(argv: Sequence[str] | None = None) -> int:
    return launch(sys.argv[1:] if argv is None else argv)


if __name__ == "__main__":
    raise SystemExit(main())
