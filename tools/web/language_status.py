#!/usr/bin/env python3
"""Language support status for the workbench (/languages).

Run in its own process: python3 language_status.py LIBRARY RESOURCES. Prints
one JSON object. Curated facts (detection evidence, scope, gaps) come from
language_support.json; everything else is read live from the repository and
the built library, so a new pack shows up without editing this file.
"""

import collections
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
from native import Engine  # noqa: E402

FLAGS = ("absorb_foreign", "attached_grammar", "declared_only", "sentence_word_weight")


def packs():
    found = []
    for directory in sorted((ROOT / "languages").iterdir()):
        if not (directory / "pack.c").is_file():
            continue
        files = sorted(p.name for p in directory.glob("*.c"))
        lines = sum(len(p.read_text(encoding="utf-8", errors="replace").splitlines())
                    for p in directory.glob("*.c"))
        found.append(dict(tag=directory.name, files=files, lines=lines))
    return found


def manifest_files(resources):
    listed = []
    for line in (resources / "manifest.ttfm").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            listed.append(line)
    return listed


def resource_rows(resources, tags):
    """Rows per language and kind over the manifest files (shared '*' rows apart)."""
    rows = {tag: collections.Counter() for tag in tags + ["*"]}
    files = {tag: set() for tag in tags + ["*"]}
    flags = {tag: {} for tag in tags}
    for name in manifest_files(resources):
        path = resources / name
        if not path.is_file() or path.suffix != ".tsv":
            continue
        with path.open(encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if not line.strip() or line.startswith("#"):
                    continue
                parts = line.rstrip("\n").split("\t")
                if len(parts) < 4:
                    continue
                kind, lang, key, value = parts[:4]
                if lang not in rows:
                    continue
                rows[lang][kind] += 1
                files[lang].add(name)
                if kind == "context" and lang in flags and key in FLAGS:
                    flags[lang][key] = value
    out = {}
    for tag in rows:
        kinds = rows[tag]
        out[tag] = dict(total=sum(kinds.values()), kinds=len(kinds),
                        top=kinds.most_common(12), files=sorted(files[tag]))
    return out, flags


def contracts(tags):
    per = {tag: collections.Counter() for tag in tags}
    for path in sorted((ROOT / "tests/evaluation").glob("*.jsonl")):
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            try:
                row = json.loads(line)
            except ValueError:
                continue
            lang = row.get("default_language") if isinstance(row, dict) else None
            if lang in per:
                per[lang][path.stem] += 1
    return {tag: dict(total=sum(c.values()), files=len(c), top=c.most_common(8)) for tag, c in per.items()}


def probe(library, resources, tags, items, input_format):
    """Run each item through each language; an error or rejected control is recorded."""
    results = {}
    for tag in tags:
        options = dict(default_language=tag, output_mode="speech", input_format=input_format)
        if input_format == "annotated":
            options["invalid_control"] = "error"
        try:
            engine = Engine(library, resources, options)
        except Exception as error:  # a pack that cannot start is itself a status
            results[tag] = {item["id"]: dict(error=str(error)[:160]) for item in items}
            continue
        try:
            row = {}
            for item in items:
                if item.get("only") and tag not in item["only"]:
                    continue
                text = item.get("texts", {}).get(tag, item.get("text"))
                if text is None:
                    continue
                try:
                    out = engine.normalize(text.encode())["text"]
                    row[item["id"]] = dict(input=text, output=out, unchanged=out == text)
                except Exception as error:
                    row[item["id"]] = dict(input=text, error=str(error)[:160])
            results[tag] = row
        finally:
            engine.close()
    return results


def git_head():
    try:
        return subprocess.run(["git", "-C", str(ROOT), "log", "-1", "--format=%h %cs %s"],
                              capture_output=True, text=True, timeout=10).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def main():
    library, resources = Path(sys.argv[1]), Path(sys.argv[2])
    support = json.loads((HERE / "language_support.json").read_text(encoding="utf-8"))
    built = packs()
    tags = [p["tag"] for p in built]
    rows, flags = resource_rows(resources, tags)
    status = dict(
        head=git_head(),
        support=support,
        packs=built,
        resources=rows,
        flags=flags,
        contracts=contracts(tags),
        samples=probe(library, resources, tags, support["samples"], "plain"),
        controls=probe(library, resources, tags, support["controls"], "annotated"),
    )
    print(json.dumps(status, ensure_ascii=False))


if __name__ == "__main__":
    main()
