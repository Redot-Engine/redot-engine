#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import shlex
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Callable
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "misc/scripts"))
from ci_lint import lint_args, main


class LintScopeTests(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        original = os.getcwd()
        os.chdir(directory.name)
        self.addCleanup(os.chdir, original)
        self.git("init", "-q")
        self.git("config", "user.name", "Lint Test")
        self.git("config", "user.email", "lint@example.invalid")
        self.git("config", "core.autocrlf", "false")
        Path("old.txt").write_text("old\n")
        self.base = self.commit()
        Path("changed.txt").write_text("new\n")
        self.head = self.commit()

    def git(self, *args: str) -> str:
        return subprocess.check_output(["git", *args], text=True, stderr=subprocess.PIPE).strip()

    def commit(self) -> str:
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")
        return self.git("rev-parse", "HEAD")

    def push(self, **kwargs: object) -> dict[str, object]:
        return {"before": self.base, "after": self.head, "ref": "refs/heads/main", **kwargs}

    def test_push_range(self) -> None:
        self.assertEqual(lint_args("push", self.push(), self.head)[0], ["--from-ref", self.base, "--to-ref", self.head])

    def test_multi_commit_push(self) -> None:
        Path("another.txt").write_text("another\n")
        self.head = self.commit()
        self.test_push_range()

    def test_pr_merge_and_merge_group(self) -> None:
        self.git("checkout", "-qb", "target", self.base)
        Path("target.txt").write_text("target\n")
        target = self.commit()
        self.git("merge", "--no-ff", "-qm", "merge", self.head)
        merge = self.git("rev-parse", "HEAD")
        events = {
            "pull_request": {"pull_request": {"base": {"sha": target}, "head": {"sha": self.head}}},
            "merge_group": {"merge_group": {"base_sha": target, "head_sha": merge}},
        }
        for name, event in events.items():
            with self.subTest(name=name):
                self.assertEqual(lint_args(name, event, merge)[0], ["--from-ref", target, "--to-ref", merge])

    def test_uncertain_base_uses_all_files(self) -> None:
        for base in (None, "", "0" * 40, "f" * 40, "$(not-a-sha)"):
            with self.subTest(base=base):
                self.assertEqual(lint_args("push", self.push(before=base), self.head)[0], ["--all-files"])

    def test_created_forced_and_tag_pushes_use_all_files(self) -> None:
        for change in ({"created": True}, {"forced": True}, {"ref": "refs/tags/v1"}):
            with self.subTest(change=change):
                self.assertEqual(lint_args("push", self.push(**change), self.head)[0], ["--all-files"])

    def test_other_events_and_explicit_full_run(self) -> None:
        for name in ("schedule", "workflow_dispatch", "unknown", "pull_request", "merge_group"):
            with self.subTest(name=name):
                self.assertEqual(lint_args(name, {}, self.head)[0], ["--all-files"])
        self.assertEqual(lint_args("push", self.push(), self.head, True)[0], ["--all-files"])

    def test_invalid_checkout_and_mismatched_event_head_fail(self) -> None:
        for sha in ("", "invalid", "f" * 40, self.base):
            with self.subTest(sha=sha), self.assertRaises(ValueError):
                lint_args("push", self.push(), sha)
        for name, event in (
            ("push", self.push(after=self.base)),
            ("push", self.push(deleted=True)),
            ("merge_group", {"merge_group": {"base_sha": self.base, "head_sha": self.base}}),
        ):
            with self.subTest(event=event), self.assertRaises(ValueError):
                lint_args(name, event, self.head)

    def test_divergent_base_uses_all_files(self) -> None:
        self.git("checkout", "-qb", "divergent", self.base)
        Path("divergent.txt").write_text("divergent\n")
        other = self.commit()
        self.git("checkout", "-q", self.head)
        self.assertEqual(lint_args("push", self.push(before=other), self.head)[0], ["--all-files"])

    def test_shallow_checkout_uses_all_files(self) -> None:
        source = Path.cwd()
        self.git("clone", "--quiet", "--depth=1", source.as_uri(), "shallow")
        os.chdir(source / "shallow")
        try:
            self.assertEqual(lint_args("push", self.push(), self.head)[0], ["--all-files"])
        finally:
            os.chdir(source)

    def test_diff_error_uses_all_files(self) -> None:
        import ci_lint

        original: Callable[..., str] = ci_lint.git

        def failing_diff(*args: str) -> str:
            if args[0] == "diff":
                raise subprocess.CalledProcessError(1, ["git", *args])
            return original(*args)

        with patch("ci_lint.git", side_effect=failing_diff):
            self.assertEqual(lint_args("push", self.push(), self.head)[0], ["--all-files"])

    def test_configuration_and_deleted_docs_use_all_files(self) -> None:
        for filename in (
            ".pre-commit-config.yaml",
            "misc/scripts/helper.py",
            "doc/classes/Example.xml",
            "modules/mono/.editorconfig",
            "methods.py",
            "tests/python_build/fixtures/example.glsl",
        ):
            with self.subTest(filename=filename):
                self.git("checkout", "-q", self.head)
                Path(filename).parent.mkdir(parents=True, exist_ok=True)
                Path(filename).write_text("fixture\n")
                changed = self.commit()
                self.assertEqual(lint_args("push", self.push(after=changed), changed)[0], ["--all-files"])
                if filename.endswith(".xml"):
                    Path(filename).unlink()
                    deleted = self.commit()
                    self.assertEqual(
                        lint_args("push", self.push(before=changed, after=deleted), deleted)[0], ["--all-files"]
                    )

    def test_main_propagates_lint_status(self) -> None:
        Path("event.json").write_text(json.dumps(self.push()))
        with patch.dict(
            os.environ, {"GITHUB_EVENT_PATH": "event.json", "GITHUB_EVENT_NAME": "push", "GITHUB_SHA": self.head}
        ), patch("ci_lint.subprocess.call", return_value=9) as call:
            self.assertEqual(main(), 9)
        self.assertEqual(call.call_args.args[0][-4:], ["--from-ref", self.base, "--to-ref", self.head])

    def test_pre_commit_receives_changed_paths_without_shell_evaluation(self) -> None:
        names = [
            "space name.txt",
            "quote's.txt",
            "$(touch sentinel).txt",
            "`touch sentinel`.txt",
            "-dash.txt",
            "unicode-é.txt",
        ]
        if os.name != "nt":
            names += ['double"quote.txt', "tab\tname.txt", "back\\slash.txt", "line\nname.txt"]
        Path("record.py").write_text(
            "import json, sys\nfrom pathlib import Path\nPath('record.json').write_text(json.dumps(sys.argv[1:]))\n"
        )
        # JSON is also YAML; using an argument-safe entry avoids fixture quoting assumptions.

        Path(".pre-commit-config.yaml").write_text(
            json.dumps(
                {
                    "repos": [
                        {
                            "repo": "local",
                            "hooks": [
                                {
                                    "id": "record",
                                    "name": "record",
                                    "language": "system",
                                    "entry": shlex.join([sys.executable.replace("\\", "/"), "record.py"]),
                                    "files": "\\.txt$",
                                    "require_serial": True,
                                }
                            ],
                        }
                    ]
                }
            )
        )
        base = self.commit()
        for name in names:
            Path(name).write_text("fixture\n")
        self.git("mv", "changed.txt", "renamed.txt")
        self.git("rm", "old.txt")
        head = self.commit()
        args, _ = lint_args("push", self.push(before=base, after=head), head)
        subprocess.run(
            [sys.executable, "-m", "pre_commit", "run", *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        self.assertEqual(set(json.loads(Path("record.json").read_text())), set(names + ["renamed.txt"]))
        self.assertFalse(Path("sentinel").exists())

    def test_range_and_all_files_detect_the_expected_failures(self) -> None:
        Path("check.py").write_text(
            "import sys\nfrom pathlib import Path\nsys.exit(any('BAD' in Path(p).read_text() for p in sys.argv[1:]))\n"
        )
        Path(".pre-commit-config.yaml").write_text(
            json.dumps(
                {
                    "repos": [
                        {
                            "repo": "local",
                            "hooks": [
                                {
                                    "id": "check",
                                    "name": "check",
                                    "language": "system",
                                    "entry": shlex.join([sys.executable.replace("\\", "/"), "check.py"]),
                                    "files": "\\.txt$",
                                }
                            ],
                        }
                    ]
                }
            )
        )
        Path("old.txt").write_text("BAD\n")
        base = self.commit()
        Path("changed.txt").write_text("good\n")
        head = self.commit()
        args, _ = lint_args("push", self.push(before=base, after=head), head)
        command = [sys.executable, "-m", "pre_commit", "run"]
        for scope, status in ((args, 0), (["--all-files"], 1)):
            with self.subTest(scope=scope):
                result = subprocess.run([*command, *scope], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                self.assertEqual(result.returncode, status, result.stdout)
        Path("changed.txt").write_text("BAD\n")
        head = self.commit()
        args, _ = lint_args("push", self.push(before=base, after=head), head)
        result = subprocess.run([*command, *args], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 1, result.stdout)


if __name__ == "__main__":
    unittest.main()
