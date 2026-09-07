#!/usr/bin/env python3
"""Check editor shutdown using a built editor: python test_shutdown.py <binary>."""

import argparse
import subprocess
import tempfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("editor", type=Path)
    args = parser.parse_args()
    editor = args.editor.resolve(strict=True)

    with tempfile.TemporaryDirectory(prefix="redot editor shutdown ") as directory:
        project = Path(directory)
        (project / "project.godot").write_text(
            'config_version=5\n[application]\nconfig/name="Editor shutdown regression"\n',
            encoding="utf-8",
        )
        for frames in (10, 120, 600):
            command = [
                str(editor),
                "--headless",
                "--editor",
                "--path",
                str(project),
                "--quit-after",
                str(frames),
                "--max-fps",
                "60",
            ]
            try:
                result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            except subprocess.TimeoutExpired:
                print(f"FAIL: editor shutdown timed out after {frames} frames")
                return 1
            if result.returncode != 0 or result.stderr.strip():
                print(f"FAIL: editor shutdown after {frames} frames (exit {result.returncode})")
                print(result.stdout)
                print(result.stderr)
                return 1
            print(f"PASS: editor shutdown after {frames} frames")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
