#!/usr/bin/env python3
"""Deterministic pronunciation source validation, compilation and review inventory.

JSONL is the editable source; generated runtime TSV remains dependency-free C data.
No engine output is used to create or approve a pronunciation reference.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import date
import json
from pathlib import Path
import re
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[2]
STATUSES = {"pending", "source-checked", "human-reviewed"}
SOURCE_KINDS = {
    "editorial-policy",
    "official-korean-name",
    "official-pronunciation",
    "user-contract",
}
FIELDS = {
    "id",
    "source_lang",
    "target_lang",
    "term",
    "aliases",
    "pronunciation",
    "category",
    "review_status",
    "evidence",
    "notes",
    "alternatives",
    "reviewer",
    "reviewed_at",
    "scope",
}
REQUIRED = FIELDS - {"alternatives", "reviewer", "reviewed_at", "scope"}
LANG = re.compile(r"[a-z]{2,3}(?:-[A-Za-z0-9]{2,8})*\Z")
IDENTIFIER = re.compile(r"[a-z0-9][a-z0-9_-]*\Z")
ASCII_LOWER = str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz")


def canonical(term: str) -> str:
    if (
        not isinstance(term, str)
        or not term
        or term != term.strip()
        or any(ord(c) < 32 or ord(c) == 127 for c in term)
    ):
        raise ValueError(
            "term needs printable text without surrounding whitespace or controls"
        )
    # These authored packs use English source terms. The native key API is wider;
    # the authoring gate deliberately limits entries to matchable lexical shapes.
    if not term.isascii():
        raise ValueError("this source profile accepts ASCII English keys only")
    key = re.sub(" +", " ", term).translate(ASCII_LOWER)
    if len(key.encode()) > 95:
        raise ValueError("canonical key exceeds the 95-byte semantic/lookup limit")
    if " " in key:
        if not 2 <= len(key.split()) <= 8 or not all(
            re.fullmatch("[a-z]+", w) for w in key.split()
        ):
            raise ValueError(
                "multiword keys require 2..8 alphabetic words in the current native grammar"
            )
    elif not (
        key in {"c++", "c#", "3d"}
        or re.fullmatch(r"[a-z]+[0-9]*(?:-[a-z]+[0-9]*|-[0-9]+)?", key)
    ):
        raise ValueError(
            "key is not a supported single lexical word, alphanumeric, hyphenated word, C++ or C# shape"
        )
    if not re.search("[a-z]", key):
        raise ValueError(
            "pure numbers belong in semantic TN, not the pronunciation lexicon"
        )
    return key


def pronunciation(value):
    if (
        not isinstance(value, str)
        or not value
        or value != value.strip()
        or not re.fullmatch("[가-힣]+(?: [가-힣]+)*", value)
    ):
        raise ValueError(
            "Korean pronunciation must contain Hangul syllables separated by single spaces"
        )
    if len(value.encode()) > 1024:
        raise ValueError("pronunciation exceeds authoring limit")


def load(root: Path = ROOT):
    root = Path(root)
    sources = {}
    for path in sorted((root / "lexicons/sources").glob("*.json")):
        document = json.loads(path.read_text(encoding="utf-8"))
        if (
            not isinstance(document, dict)
            or document.get("schema_version") != 1
            or not isinstance(document.get("sources"), list)
        ):
            raise ValueError(f"{path}: unsupported source catalog")
        for source in document["sources"]:
            if not isinstance(source, dict) or not all(
                isinstance(source.get(k), str) and source[k]
                for k in ("id", "title", "url", "kind", "checked_at", "note")
            ):
                raise ValueError(f"{path}: missing source fields")
            if (
                not IDENTIFIER.fullmatch(source["id"])
                or source["id"] in sources
                or source["kind"] not in SOURCE_KINDS
            ):
                raise ValueError(f"{path}: invalid/duplicate source {source['id']}")
            url = urlsplit(source["url"])
            if url.scheme != "https" or not url.netloc or url.username or url.password:
                raise ValueError(
                    f"{path}: source URL must be a credential-free HTTPS URL"
                )
            date.fromisoformat(source["checked_at"])
            sources[source["id"]] = source
    records = []
    ids = set()
    keys = {}
    for path in sorted((root / "lexicons").glob("*/*.jsonl")):
        for line_no, line in enumerate(
            path.read_text(encoding="utf-8").splitlines(), 1
        ):
            if not line.strip():
                continue
            try:
                row = json.loads(line)
                if (
                    not isinstance(row, dict)
                    or set(row) - FIELDS
                    or REQUIRED - set(row)
                ):
                    raise ValueError("unknown or missing record fields")
                if (
                    not isinstance(row["id"], str)
                    or not IDENTIFIER.fullmatch(row["id"])
                    or row["id"] in ids
                ):
                    raise ValueError("invalid/duplicate record id")
                for field in ("source_lang", "target_lang"):
                    if (
                        not isinstance(row[field], str)
                        or not LANG.fullmatch(row[field])
                        or len(row[field]) >= 24
                    ):
                        raise ValueError("invalid language tag")
                if row["source_lang"] != "en-US" or row["target_lang"] != "ko-KR":
                    raise ValueError(
                        "this authoring profile currently supports en-US to ko-KR"
                    )
                if not isinstance(row["category"], str) or not IDENTIFIER.fullmatch(
                    row["category"]
                ):
                    raise ValueError("unsafe category path")
                if row.get("scope", "system") not in {"system", "finance"}:
                    raise ValueError("unknown dictionary scope")
                if row["review_status"] not in STATUSES:
                    raise ValueError("unknown review status")
                if not isinstance(row["notes"], str) or not row["notes"].strip():
                    raise ValueError("an explicit policy/evidence note is required")
                evidence = row["evidence"]
                if (
                    not isinstance(evidence, list)
                    or not evidence
                    or any(not isinstance(k, str) or k not in sources for k in evidence)
                    or len(set(evidence)) != len(evidence)
                ):
                    raise ValueError(
                        "missing, unknown or duplicate evidence references"
                    )
                if row["review_status"] == "source-checked" and not any(
                    sources[k]["kind"] != "editorial-policy" for k in evidence
                ):
                    raise ValueError(
                        "source-checked requires a primary spelling/pronunciation source or explicit user contract"
                    )
                if row["review_status"] == "human-reviewed":
                    if (
                        not isinstance(row.get("reviewer"), str)
                        or not row["reviewer"].strip()
                    ):
                        raise ValueError("human review requires a named reviewer")
                    date.fromisoformat(row.get("reviewed_at", ""))
                pronunciation(row["pronunciation"])
                aliases = row["aliases"]
                if not isinstance(aliases, list) or len(aliases) > 32:
                    raise ValueError("aliases must be a bounded list")
                for value in row.get("alternatives", []):
                    pronunciation(value)
                if "alternatives" in row and not isinstance(row["alternatives"], list):
                    raise ValueError("alternatives must be a list")
                normalized = []
                for term in [row["term"], *aliases]:
                    key = canonical(term)
                    identity = (
                        row.get("scope", "system"),
                        row["source_lang"],
                        row["target_lang"],
                        key,
                    )
                    if identity in keys:
                        raise ValueError(
                            f"canonical key collision {key!r} with {keys[identity]}"
                        )
                    keys[identity] = row["id"]
                    normalized.append(key)
                ids.add(row["id"])
                records.append(
                    dict(
                        row,
                        canonical_keys=normalized,
                        source_file=path.relative_to(root).as_posix(),
                        source_line=line_no,
                    )
                )
            except (ValueError, TypeError, KeyError) as error:
                raise ValueError(f"{path}:{line_no}: {error}") from error
    if not records or not sources:
        raise ValueError("source catalog or entries missing")
    records.sort(
        key=lambda r: (r["target_lang"], r["category"], r["canonical_keys"][0], r["id"])
    )
    return records, sources


def runtime_path(record):
    if record.get("scope", "system") == "finance":
        return "resources/domains/finance/" + record["target_lang"] + "/lexicon.tsv"
    category = record["category"]
    tail = (
        "multiword/" + category.removeprefix("multiword_") + ".tsv"
        if category.startswith("multiword_")
        else category + "/default.tsv"
    )
    return "resources/languages/" + record["target_lang"] + "/lexicon/" + tail


def rendered(records):
    grouped = defaultdict(list)
    for row in records:
        for key in row["canonical_keys"]:
            grouped[runtime_path(row)].append(
                (
                    row["source_lang"] + ":" + key,
                    row["target_lang"],
                    row["pronunciation"],
                )
            )
    output = {}
    for path, rows in grouped.items():
        output[path] = (
            "# Generated by tools/lexicon/manage.py build; edit lexicons/ source JSONL.\n"
            + "# Review status and provenance are in lexicons/; matching a default is not linguistic approval.\n"
            + "".join(
                f"lexicon\t{lang}\t{key}\t{pron}\n" for key, lang, pron in sorted(rows)
            )
        )
    return output


def inventory(root=ROOT):
    records, sources = load(root)
    public = [
        dict(row, sources=[sources[key] for key in row["evidence"]]) for row in records
    ]
    return dict(
        schema_version=1,
        summary=dict(
            entries=len(records),
            runtime_keys=sum(len(r["canonical_keys"]) for r in records),
            categories=dict(sorted(Counter(r["category"] for r in records).items())),
            review_status=dict(
                sorted(Counter(r["review_status"] for r in records).items())
            ),
            source_count=len(sources),
            scopes=dict(
                sorted(Counter(r.get("scope", "system") for r in records).items())
            ),
        ),
        entries=public,
        note="Source-checked confirms a documented choice, not independent human pronunciation approval. Pending entries remain explicit project defaults.",
    )


def build(root=ROOT, check=False):
    records, sources = load(root)
    output = rendered(records)
    mismatches = []
    manifest = set(
        line
        for line in (root / "resources/manifest.ttfm").read_text().splitlines()
        if line and not line.startswith("#")
    )
    existing = {
        str(p.relative_to(root))
        for p in (root / "resources/languages").glob("*/lexicon/**/*.tsv")
    }
    existing.update(
        str(p.relative_to(root))
        for p in (root / "resources/domains/finance").glob("*/lexicon.tsv")
    )
    if existing - set(output):
        raise ValueError(
            "unmanaged runtime lexicons: " + ", ".join(sorted(existing - set(output)))
        )
    for name, text in output.items():
        if "/domains/" in name and name.removeprefix("resources/") in manifest:
            raise ValueError(
                f"{name}: scoped dictionary must not appear in the system manifest"
            )
        if "/domains/" not in name and name.removeprefix("resources/") not in manifest:
            raise ValueError(f"{name}: runtime file missing from manifest")
        path = root / name
        if check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                mismatches.append(name)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            temporary = path.with_suffix(".tsv.tmp")
            temporary.write_text(text, encoding="utf-8")
            temporary.replace(path)
    if mismatches:
        raise ValueError(
            "generated resources differ; run build: " + ", ".join(mismatches)
        )
    return dict(
        entries=len(records),
        runtime_keys=sum(len(r["canonical_keys"]) for r in records),
        files=len(output),
        source_count=len(sources),
        check=check,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("validate", "build", "inventory"))
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify generated runtime TSV without changing it",
    )
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        if args.check and args.command != "build":
            raise ValueError("--check is only valid with build")
        result = (
            build(args.root, args.check)
            if args.command == "build"
            else inventory(args.root)
        )
        if args.command == "validate":
            result = result["summary"]
        text = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        if args.output:
            args.output.write_text(text, encoding="utf-8")
        else:
            print(text, end="")
    except (ValueError, OSError, UnicodeError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
