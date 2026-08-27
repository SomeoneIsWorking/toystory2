#!/usr/bin/env python3
"""Hermetic positive and refusal tests for the shipping launcher."""

from __future__ import annotations

import io
import os
import shutil
import subprocess
import tempfile
import unittest
from collections.abc import Sequence
from pathlib import Path

import run

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRATCH = REPO_ROOT / "scratch" / "raw"
LOCKED_PYTHON = "/locked/venv/bin/python"


class FakeHost(run.Host):
    def __init__(
        self,
        *,
        missing: set[str] | None = None,
        fail_token: str | None = None,
        system: str = "Linux",
        distribution: str = "fedora",
    ) -> None:
        self.missing = missing or set()
        self.fail_token = fail_token
        self.system_name = system
        self.distribution = distribution
        self.commands: list[tuple[list[str], dict[str, object]]] = []

    def which(self, name: str) -> str | None:
        if name in self.missing or Path(name).name in self.missing:
            return None
        return f"/fake/{Path(name).name}"

    def system(self) -> str:
        return self.system_name

    def linux_distribution(self) -> str:
        return self.distribution

    def run(
        self, args: Sequence[str], **kwargs: object
    ) -> subprocess.CompletedProcess[str]:
        command = [str(arg) for arg in args]
        self.commands.append((command, kwargs))
        returncode = 1 if self.fail_token and self.fail_token in command else 0
        stdout = "abcdef12" if "rev-parse" in command else ""
        return subprocess.CompletedProcess(
            command, returncode, stdout=stdout, stderr=""
        )


class LauncherTest(unittest.TestCase):
    def setUp(self) -> None:
        SCRATCH.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="run-selftest-", dir=SCRATCH)
        self.root = Path(self.temp.name)
        framework_cmake = self.root / "external" / "psxport" / "cmake"
        framework_cmake.mkdir(parents=True)
        (framework_cmake / "psxport.cmake").write_text("# fixture\n")
        policy = self.root / "external/psxport/tools/port/launch_environment.py"
        policy.parent.mkdir(parents=True)
        shutil.copyfile(
            REPO_ROOT / "external/psxport/tools/port/launch_environment.py", policy
        )

    def tearDown(self) -> None:
        self.temp.cleanup()

    def invoke(
        self,
        host: FakeHost,
        *argv: str,
        environment: dict[str, str] | None = None,
    ) -> tuple[int, str, str]:
        stdout = io.StringIO()
        stderr = io.StringIO()
        code = run.launch(
            argv,
            environ=environment or {"PATH": os.environ.get("PATH", "")},
            host=host,
            root=self.root,
            python_executable=LOCKED_PYTHON,
            stdout=stdout,
            stderr=stderr,
        )
        return code, stdout.getvalue(), stderr.getvalue()

    @staticmethod
    def command_list(host: FakeHost) -> list[list[str]]:
        return [command for command, _ in host.commands]

    def test_zero_arguments_provisions_builds_and_launches_current_product(
        self,
    ) -> None:
        host = FakeHost()
        code, stdout, stderr = self.invoke(
            host,
            environment={
                "PATH": os.environ.get("PATH", ""),
                "PSXPORT_VK_HEADLESS": "1",
                "PSXPORT_NOAUDIO": "1",
                "PSXPORT_NOPACE": "1",
            },
        )
        commands = self.command_list(host)

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertIn("launching scratch/bin/toystory2_port", stdout)
        self.assertIn([LOCKED_PYTHON, "tools/psxport_sync.py", "--auto"], commands)
        self.assertIn(
            [LOCKED_PYTHON, "-B", "tools/recomp_substrate.py", "--ensure"],
            commands,
        )
        configure = next(command for command in commands if "-S" in command)
        self.assertEqual(configure[configure.index("-B") + 1], run.PLAYER_BUILD_DIR)
        self.assertIn("-DBUILD_TESTING=OFF", configure)
        self.assertIn(f"-DPython3_EXECUTABLE={LOCKED_PYTHON}", configure)
        self.assertFalse(any(Path(command[0]).name == "ctest" for command in commands))
        self.assertFalse(any("test" in argument.lower() for argument in commands[-2]))
        self.assertEqual(commands[-2][2], run.PLAYER_BUILD_DIR)
        self.assertTrue(commands[-1][0].endswith("scratch/bin/toystory2_port"))
        launch_environment = host.commands[-1][1]["env"]
        self.assertEqual(launch_environment["PSXPORT_VK_WINDOW"], "1")
        for key in ("PSXPORT_VK_HEADLESS", "PSXPORT_NOAUDIO", "PSXPORT_NOPACE"):
            self.assertNotIn(key, launch_environment)

    def test_prepare_only_builds_without_launching(self) -> None:
        host = FakeHost()
        code, stdout, stderr = self.invoke(host, "--prepare-only")

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertIn("built and ready", stdout)
        self.assertFalse(
            any(
                command and command[0].endswith("toystory2_port")
                for command in self.command_list(host)
            )
        )

    def test_help_succeeds_before_dependencies_framework_or_disc_discovery(self) -> None:
        for help_argument in ("-h", "--help"):
            with self.subTest(help_argument=help_argument):
                host = FakeHost(missing={"cmake", "git", "glslc", "pkg-config"})
                code, stdout, stderr = self.invoke(
                    host,
                    help_argument,
                    environment={"PSXPORT_TS2_DISC": "/must/not/be/read.chd"},
                )

                self.assertEqual(code, 0)
                self.assertIn("usage:", stdout)
                self.assertIn("--prepare-only", stdout)
                self.assertEqual(stderr, "")
                self.assertEqual(host.commands, [])

    def test_explicit_compilers_pass_through_without_identity_probe(self) -> None:
        host = FakeHost()
        code, _, stderr = self.invoke(
            host,
            "--prepare-only",
            environment={"CC": "custom-c", "CXX": "custom-cxx"},
        )
        commands = self.command_list(host)
        configure = next(command for command in commands if "-S" in command)

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertIn("-DCMAKE_C_COMPILER=custom-c", configure)
        self.assertIn("-DCMAKE_CXX_COMPILER=custom-cxx", configure)
        self.assertFalse(any("--version" in command for command in commands))

    def test_cmake_owns_default_compiler_discovery(self) -> None:
        host = FakeHost()
        code, _, stderr = self.invoke(host, "--prepare-only")
        configure = next(
            command for command in self.command_list(host) if "-S" in command
        )

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertFalse(
            any(argument.startswith("-DCMAKE_C_COMPILER=") for argument in configure)
        )
        self.assertFalse(
            any(argument.startswith("-DCMAKE_CXX_COMPILER=") for argument in configure)
        )

    def test_missing_tool_names_exact_fedora_install_command(self) -> None:
        cases = (
            (FakeHost(missing={"cmake"}), "sudo dnf install cmake"),
            (FakeHost(missing={"glslc"}), "sudo dnf install glslc"),
        )
        for host, expected in cases:
            with self.subTest(expected=expected):
                code, stdout, stderr = self.invoke(host)
                self.assertEqual(code, 1)
                self.assertEqual(stdout, "")
                self.assertIn(expected, stderr)
                self.assertEqual(host.commands, [])

    def test_missing_libraries_name_supported_platform_commands(self) -> None:
        cases = (
            (FakeHost(fail_token="sdl3-image"), "sudo dnf install SDL3_image-devel"),
            (
                FakeHost(fail_token="freetype2", distribution="ubuntu"),
                "sudo apt install libfreetype-dev",
            ),
        )
        for host, expected in cases:
            with self.subTest(expected=expected):
                code, _, stderr = self.invoke(host)
                self.assertEqual(code, 1)
                self.assertIn(expected, stderr)

    def test_configure_failure_names_compiler_install_without_filtering(self) -> None:
        host = FakeHost(fail_token="-S")
        code, _, stderr = self.invoke(host, "--prepare-only")

        self.assertEqual(code, 1)
        self.assertIn("sudo dnf install gcc gcc-c++", stderr)
        self.assertIn("sudo dnf install zlib-devel", stderr)
        self.assertIn("sudo dnf install libzstd-devel", stderr)

    def test_provisioning_failure_stops_before_configure(self) -> None:
        host = FakeHost(fail_token="tools/recomp_substrate.py")
        code, _, stderr = self.invoke(host)
        commands = self.command_list(host)

        self.assertEqual(code, 1)
        self.assertIn("provisioning or recompilation failed", stderr)
        self.assertFalse(any("-S" in command for command in commands))

    def test_explicit_framework_checkout_skips_auto_resolution(self) -> None:
        host = FakeHost()
        framework = self.root / "framework-dev"
        (framework / "cmake").mkdir(parents=True)
        (framework / "cmake" / "psxport.cmake").write_text("# fixture\n")
        code, _, stderr = self.invoke(
            host,
            "--prepare-only",
            environment={"PSXPORT_DIR": str(framework)},
        )

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertNotIn(
            [LOCKED_PYTHON, "tools/psxport_sync.py", "--auto"],
            self.command_list(host),
        )

    def test_shell_and_lock_are_the_stable_entry_contract(self) -> None:
        wrapper = (REPO_ROOT / "run.sh").read_text()
        bootstrap = (REPO_ROOT / "bootstrap.py").read_text()
        project = (REPO_ROOT / "pyproject.toml").read_text()
        lock = (REPO_ROOT / "uv.lock").read_text()

        self.assertEqual(
            wrapper,
            '#!/bin/sh\ncd "$(dirname "$0")" || exit 1\n'
            'exec uv run --frozen python bootstrap.py "$@"\n',
        )
        self.assertIn("from tools.run import main", bootstrap)
        self.assertIn("package = false", project)
        self.assertIn("version = 1", lock)
        self.assertTrue(os.access(REPO_ROOT / "run.sh", os.X_OK))


if __name__ == "__main__":
    unittest.main()
