#!/usr/bin/env python3
"""Check changed repository files for deterministic syntax and merge errors."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def changed_files(base_sha: str | None, head_sha: str) -> list[Path]:
    if base_sha:
        command = [
            "git",
            "diff",
            "--name-only",
            "--diff-filter=ACMR",
            base_sha,
            head_sha,
        ]
    else:
        command = ["git", "ls-files"]

    result = subprocess.run(command, check=True, capture_output=True, text=True)
    return [Path(line) for line in result.stdout.splitlines() if line]


def check_text_file(path: Path, errors: list[str]) -> None:
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return

    for line_number, line in enumerate(text.splitlines(), start=1):
        if line.startswith(("<<<<<<< ", ">>>>>>> ")):
            errors.append(f"{path}:{line_number}: merge conflict marker")


def check_json_file(path: Path, errors: list[str]) -> None:
    try:
        with path.open(encoding="utf-8") as stream:
            json.load(stream)
    except (OSError, json.JSONDecodeError) as error:
        errors.append(f"{path}: invalid JSON: {error}")


def check_xml_file(path: Path, errors: list[str]) -> None:
    try:
        ET.parse(path)
    except (OSError, ET.ParseError) as error:
        errors.append(f"{path}: invalid XML: {error}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-sha")
    parser.add_argument("--head-sha", default="HEAD")
    args = parser.parse_args()

    repo_root = Path.cwd()
    errors: list[str] = []
    files = changed_files(args.base_sha, args.head_sha)

    for relative_path in files:
        path = repo_root / relative_path
        if not path.is_file():
            continue

        check_text_file(relative_path, errors)

        if relative_path.suffix.lower() == ".json":
            check_json_file(path, errors)
        elif relative_path.suffix.lower() == ".xml":
            check_xml_file(path, errors)

    if errors:
        print("Repository hygiene check failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    print(f"Repository hygiene check passed for {len(files)} changed file(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
