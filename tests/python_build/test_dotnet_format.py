#!/usr/bin/env python3
from __future__ import annotations

import os
import runpy
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[2] / "misc/scripts/dotnet_format.py"


class DotnetFormatTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        original = os.getcwd()
        os.chdir(self.directory.name)
        self.addCleanup(os.chdir, original)

    def run_script(self, files: list[str], projects: list[str]) -> None:
        with patch.object(sys, "argv", [str(SCRIPT), *files]), patch("glob.glob", return_value=projects):
            runpy.run_path(str(SCRIPT), run_name="__main__")

    def test_preserves_project_and_file_arguments(self) -> None:
        project = os.path.join("project with spaces", "Example.csproj")
        files = [os.path.join("project with spaces", name) for name in ("a b.cs", "$(sentinel).cs", "quote's.cs")]
        with patch("subprocess.run", return_value=subprocess.CompletedProcess([], 0)) as run, patch(
            "os.system"
        ) as shell:
            self.run_script(files, [project])
        shell.assert_not_called()
        run.assert_called_once_with(["dotnet", "format", os.path.dirname(project), "--include", *files])

    def test_propagates_formatter_failure(self) -> None:
        with patch("subprocess.run", return_value=subprocess.CompletedProcess([], 7)), patch(
            "os.system", return_value=7
        ):
            with self.assertRaises(SystemExit) as error:
                self.run_script([os.path.join("project", "file.cs")], [os.path.join("project", "Example.csproj")])
        self.assertEqual(error.exception.code, 7)

    def test_missing_formatter_fails(self) -> None:
        with patch("subprocess.run", side_effect=FileNotFoundError), patch("os.system", return_value=1):
            with self.assertRaises(FileNotFoundError):
                self.run_script([os.path.join("project", "file.cs")], [os.path.join("project", "Example.csproj")])

    def test_groups_files_by_project(self) -> None:
        projects = [os.path.join(name, "Example.csproj") for name in ("one", "two", "unused")]
        files = [os.path.join(name, "file.cs") for name in ("one", "two")]
        with patch("subprocess.run", return_value=subprocess.CompletedProcess([], 0)) as run, patch("os.system"):
            self.run_script(files, projects)
        self.assertEqual(
            [call.args[0] for call in run.call_args_list],
            [["dotnet", "format", name, "--include", filename] for name, filename in zip(("one", "two"), files)],
        )

    def test_unmatched_files_do_not_run_formatter(self) -> None:
        with patch("subprocess.run") as run, patch("os.system") as shell:
            self.run_script([os.path.join("other", "file.cs")], [os.path.join("project", "Example.csproj")])
        run.assert_not_called()
        shell.assert_not_called()
        self.assertEqual(Path("modules/mono/SdkPackageVersions.props").read_text(), "<Project />")
        self.assertEqual(os.environ["GodotSkipGenerated"], "true")


if __name__ == "__main__":
    unittest.main()
