#!/usr/bin/env python3
"""Check Android string resources and report localization quality signals."""

from __future__ import annotations

import argparse
import re
import subprocess
import xml.etree.ElementTree as ET
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


PRINTF_RE = re.compile(r"%(?:\d+\$)?[-+# 0,(<]*\d*(?:\.\d+)?[a-zA-Z]")
BRACE_RE = re.compile(r"\{[A-Za-z0-9_]+\}")
HAN_RE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff]")


@dataclass(frozen=True)
class ResourceEntry:
    name: str
    tag: str
    text: str


def resource_files() -> dict[str, Path]:
    root = Path("app/src/main/res")
    files: dict[str, Path] = {}
    default_file = root / "values" / "strings.xml"
    if default_file.is_file():
        files["zh"] = default_file

    for directory in sorted(root.glob("values-*/")):
        candidate = directory / "strings.xml"
        if candidate.is_file():
            files[directory.name.removeprefix("values-")] = candidate
    return files


def parse_file(path: Path) -> tuple[dict[str, ResourceEntry], list[str], str | None]:
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as error:
        return {}, [], f"{path}: invalid XML: {error}"

    entries: dict[str, ResourceEntry] = {}
    duplicates: list[str] = []
    for element in root:
        name = element.get("name")
        if not name:
            continue
        if name in entries:
            duplicates.append(name)
        entries[name] = ResourceEntry(
            name=name,
            tag=element.tag,
            text="".join(element.itertext()),
        )
    return entries, duplicates, None


def placeholders(text: str) -> Counter[str]:
    return Counter(PRINTF_RE.findall(text) + BRACE_RE.findall(text))


def changed_keys(base_sha: str | None, head_sha: str) -> set[str] | None:
    if not base_sha:
        return None
    result = subprocess.run(
        [
            "git",
            "diff",
            "--unified=0",
            base_sha,
            head_sha,
            "--",
            "app/src/main/res",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    keys: set[str] = set()
    for line in result.stdout.splitlines():
        if not line.startswith("+") or line.startswith("+++"):
            continue
        match = re.search(r'<(?:string|plurals|string-array|integer-array)\b[^>]*\bname="([^"]+)"', line)
        if match:
            keys.add(match.group(1))
    return keys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-sha")
    parser.add_argument("--head-sha", default="HEAD")
    args = parser.parse_args()

    files = resource_files()
    if "zh" not in files:
        print("Missing app/src/main/res/values/strings.xml")
        return 1

    errors: list[str] = []
    warnings: list[str] = []
    data: dict[str, dict[str, ResourceEntry]] = {}
    duplicate_count = 0

    for language, path in files.items():
        entries, duplicates, parse_error = parse_file(path)
        if parse_error:
            errors.append(parse_error)
            continue
        data[language] = entries
        duplicate_count += len(duplicates)
        for name in duplicates:
            errors.append(f"{path}: duplicate resource name: {name}")

    base = data.get("zh", {})
    base_names = set(base)
    changed = changed_keys(args.base_sha, args.head_sha)
    missing_total = 0
    untranslated_total = 0
    script_hint_total = 0
    placeholder_warning_total = 0

    for language, entries in sorted(data.items()):
        if language == "zh":
            continue

        missing = base_names - set(entries)
        extra = set(entries) - base_names
        missing_total += len(missing)

        if missing:
            warnings.append(f"{language}: {len(missing)} resource(s) missing from translation")
        for name in sorted(extra):
            errors.append(f"{files[language]}: resource not present in zh baseline: {name}")

        for name in sorted(base_names & set(entries)):
            source = base[name]
            target = entries[name]
            if source.tag != target.tag:
                errors.append(
                    f"{files[language]}:{name}: resource type differs from zh ({source.tag} vs {target.tag})"
                )

            if source.text.strip() and source.text == target.text:
                untranslated_total += 1

            target_has_han = bool(HAN_RE.search(target.text))
            if target_has_han and language != "zh":
                script_hint_total += 1

            if placeholders(source.text) != placeholders(target.text):
                message = f"{files[language]}:{name}: placeholder signature differs from zh"
                if changed is not None and name in changed:
                    errors.append(message)
                else:
                    placeholder_warning_total += 1

    if untranslated_total:
        warnings.append(f"{untranslated_total} translated value(s) are identical to zh baseline")
    if script_hint_total:
        warnings.append(f"{script_hint_total} non-zh value(s) contain Han characters; review language selection")
    if placeholder_warning_total:
        warnings.append(
            f"{placeholder_warning_total} existing placeholder mismatch(es) are outside this change and remain warnings"
        )

    print(
        "Localization summary: "
        f"languages={len(data)}, duplicates={duplicate_count}, missing={missing_total}, "
        f"untranslated={untranslated_total}"
    )
    for warning in warnings:
        print(f"warning: {warning}")

    if errors:
        print("Localization check failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Localization structural check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
