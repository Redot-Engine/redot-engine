#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from typing import Any


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], text=True, stderr=subprocess.PIPE).rstrip("\n")


def commit(sha: str) -> str:
    if not re.fullmatch(r"[0-9a-fA-F]{40}", sha):
        raise ValueError("Invalid commit ID")
    try:
        return git("rev-parse", "--verify", f"{sha}^{{commit}}")
    except subprocess.CalledProcessError as error:
        raise ValueError("Commit is unavailable") from error


def lint_args(
    event_name: str, event: dict[str, Any], expected_sha: str, all_files: bool = False
) -> tuple[list[str], str]:
    head = git("rev-parse", "HEAD")
    if commit(expected_sha) != head:
        raise ValueError("Checkout does not match the event commit")

    base = ""
    if event_name == "push":
        if event.get("deleted"):
            raise ValueError("Deleted refs must be excluded before checkout")
        if commit(event.get("after", "")) != head:
            raise ValueError("Checkout does not match the push head")
        if event.get("created") or event.get("forced") or event.get("ref", "").startswith("refs/tags/"):
            return ["--all-files"], "created-forced-or-tag-push"
        base = event.get("before", "")
    elif event_name == "pull_request":
        base = event.get("pull_request", {}).get("base", {}).get("sha", "")
    elif event_name == "merge_group":
        group = event.get("merge_group", {})
        if group and commit(group.get("head_sha", "")) != head:
            raise ValueError("Checkout does not match the merge group head")
        base = group.get("base_sha", "")

    if all_files:
        return ["--all-files"], "explicit-full-tree"
    try:
        base = commit(base or "")
        git("merge-base", "--is-ancestor", base, head)
    except (ValueError, subprocess.CalledProcessError):
        return ["--all-files"], "no-verified-ancestor"

    # Changes to lint inputs can affect files outside the commit range. Deleted
    # documentation also needs validation even when no XML remains in the diff.
    try:
        paths = git("diff", "--name-only", "--no-ext-diff", "-z", base, head).split("\0")
    except subprocess.CalledProcessError:
        return ["--all-files"], "diff-unavailable"
    for path in paths:
        if (
            path.startswith((".github/", "misc/scripts/", "misc/utility/", "doc/tools/", "tests/python_build/"))
            or path.endswith((".yaml", ".yml", ".toml", ".csproj", ".props", ".targets", ".sln", ".slnx"))
            or path.rsplit("/", 1)[-1] in (".clang-format", ".editorconfig", "global.json")
            or path in ("custom_dict.txt", "gles3_builders.py", "glsl_builders.py", "methods.py", "platform_methods.py")
            or path.startswith("platform/web/")
            and ("eslint" in path or "jsdoc2rst/" in path or "package" in path)
            or path.endswith(".xml")
            and (path.startswith("doc/classes/") or "/doc_classes/" in path)
        ):
            return ["--all-files"], "lint-inputs-changed"
    return ["--from-ref", base, "--to-ref", head], "verified-range"


def main() -> int:
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as file:
        event = json.load(file)
    args, reason = lint_args(
        os.environ["GITHUB_EVENT_NAME"], event, os.environ["GITHUB_SHA"], os.environ.get("LINT_ALL_FILES") == "true"
    )
    print(f"Lint scope: {reason}; {' '.join(args)}", flush=True)
    return subprocess.call(
        [sys.executable, "-m", "pre_commit", "run", "--show-diff-on-failure", "--color=always", *args]
    )


if __name__ == "__main__":
    sys.exit(main())
