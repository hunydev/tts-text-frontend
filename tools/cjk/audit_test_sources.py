#!/usr/bin/env python3
"""List literal Han/Kana code points in repository test sources.

Uses the design's section 8.1 blocks, independent of reading-table coverage.
Tracked and untracked, non-ignored files below tests/ are inspected as UTF-8.
Generated/ignored files (e.g. node_modules, pycache) and binary files are excluded.
Source-language escape sequences are not evaluated. No network or mutations.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HAN = ((0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xF900, 0xFAFF),
       (0x2F800, 0x2FA1F), (0x2E80, 0x2EFF), (0x2F00, 0x2FDF),
       (0x20000, 0x2A6DF), (0x2A700, 0x2B73F), (0x2B740, 0x2B81F),
       (0x2B820, 0x2CEAF), (0x2CEB0, 0x2EBEF), (0x30000, 0x3134F),
       (0x31350, 0x323AF))
KANA = ((0x3040, 0x309F), (0x30A0, 0x30FF), (0x31F0, 0x31FF),
        (0xFF66, 0xFF9D), (0x1B000, 0x1B16F))


def cjk_script(character):
    cp = ord(character)
    if any(begin <= cp <= end for begin, end in HAN):
        return "HAN"
    if any(begin <= cp <= end for begin, end in KANA):
        return "KANA"
    return None


def survey(root=ROOT):
    names = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", "tests/"],
        cwd=root).decode("utf-8").split("\0")
    rows = []
    for name in sorted(set(names) - {""}):
        path = root / name
        if not path.is_file() or path.is_symlink():
            continue
        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        unique = set(content)
        characters = {script: sorted({c for c in unique if cjk_script(c) == script})
                      for script in ("HAN", "KANA")}
        if any(characters.values()):
            rows.append(dict(file=name, characters=characters))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--format", choices=("json", "markdown"), default="json")
    args = parser.parse_args()
    rows = survey(args.root.resolve())
    day = datetime.now(timezone.utc).date().isoformat()
    if args.format == "json":
        print(json.dumps(dict(date=day, files=rows), ensure_ascii=False, indent=2))
    else:
        print(f"조사 일자: {day} (UTC). CJK 포함 파일: {len(rows)}개.\n")
        print("| 파일 | Han 문자 | Kana 문자 |\n|---|---|---|")
        for row in rows:
            han, kana = (" ".join(row["characters"][s]) or "—" for s in ("HAN", "KANA"))
            print(f"| `{row['file']}` | {han} | {kana} |")


if __name__ == "__main__":
    main()
