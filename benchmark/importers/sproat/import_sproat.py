#!/usr/bin/env python3
"""Convert original Sproat three-column TSV to contextual benchmark JSONL.

This is an original importer, not a copy of the upstream split/loader scripts.
No native engine is called and no reference is generated from engine output.
"""

import argparse
import hashlib
import json
import re
import tempfile
from collections import Counter
from pathlib import Path

DATASET_URL = "https://www.kaggle.com/datasets/richardwilliamsproat/text-normalization-for-english-russian-and-polish"
MAX_LINE_BYTES = 1024 * 1024
MAX_SENTENCE_BYTES = 4 * 1024 * 1024
MAX_SENTENCE_TOKENS = 10000


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def infer_split(source_file):
    match = re.search(r"output-(\d{5})-of-00100$", source_file)
    if not match or int(match[1]) > 99:
        raise ValueError("cannot infer split: provide --split for a non-shard input")
    shard = int(match[1])
    return "train" if shard < 90 else "dev" if shard < 95 else "test"


def sentences(path, drop_incomplete=False):
    """Yield complete sentences while retaining input line numbers and markers."""
    tokens, size, start = [], 0, 1
    with path.open("rb") as stream:
        line_number = 0
        while True:
            raw = stream.readline(MAX_LINE_BYTES + 1)
            if not raw:
                break
            line_number += 1
            if len(raw) > MAX_LINE_BYTES:
                raise ValueError(f"line {line_number}: exceeds line byte limit")
            line = raw.decode("utf-8", errors="strict").rstrip("\r\n")
            fields = line.split("\t")
            if fields == ["<eos>", "<eos>"]:
                if not tokens:
                    raise ValueError(f"line {line_number}: empty sentence")
                yield start, line_number, tokens
                tokens, size, start = [], 0, line_number + 1
                continue
            if (
                len(fields) != 3
                or not re.fullmatch(r"[A-Z][A-Z0-9_]*", fields[0])
                or not fields[1]
            ):
                raise ValueError(
                    f"line {line_number}: expected CLASS<TAB>written<TAB>spoken"
                )
            size += len(raw)
            if size > MAX_SENTENCE_BYTES or len(tokens) >= MAX_SENTENCE_TOKENS:
                raise ValueError(
                    f"line {line_number}: sentence exceeds bounded-memory limits"
                )
            tokens.append((fields[0], fields[1], fields[2], line_number))
    if tokens and not drop_incomplete:
        raise ValueError(
            "incomplete final sentence: use --drop-incomplete only for an intentional prefix"
        )


def make_case(start, end, tokens, args, source_hash):
    written, spoken, spans, offset = [], [], [], 0
    for category, surface, original, line in tokens:
        rendered = (
            surface if original == "<self>" else "" if original == "sil" else original
        )
        if written:
            offset += 1
        width = len(surface.encode("utf-8"))
        spans.append(
            {
                "begin": offset,
                "end": offset + width,
                "written": surface,
                "spoken": rendered,
                "semiotic_class": category,
                "reference_status": "dataset-reference",
                "metadata": {"original_spoken": original, "source_line": line},
            }
        )
        offset += width
        written.append(surface)
        if rendered:
            spoken.append(rendered)
    key = hashlib.sha256(args.source_file.encode("utf-8")).hexdigest()[:12]
    return {
        "schema_version": 1,
        "id": f"sproat:{args.language}:{key}:L{start}-{end}",
        "dataset": "sproat",
        "split": args.split,
        "language": args.language,
        "text": " ".join(written),
        "expected": " ".join(spoken),
        "reference_status": "dataset-reference",
        "usage": "benchmark-only",
        "provenance": {
            "source_url": args.source_url,
            "revision": args.revision,
            "source_file": args.source_file,
            "source_sha256": source_hash,
            "license": "CC-BY-SA-4.0",
            "license_evidence": "distribution LICENSE; Kaggle metadata Unknown",
            "license_url": "https://creativecommons.org/licenses/by-sa/4.0/",
            "source_artifact_scope": "prefix"
            if args.partial_source
            else "provided-file",
            "source_line_begin": start,
            "source_line_end": end,
        },
        "spans": spans,
        "metadata": {
            "reconstruction": "written tokens joined with one ASCII space; original whitespace unavailable",
            "reference_mapping": "<self> copies written; sil becomes empty; other references unchanged",
            "reference_origin": "Google Kestrel generated; not human-approved ground truth",
            "selection": "input-prefix" if args.limit else "all-complete-sentences",
            "sentence_limit": args.limit,
        },
    }


def convert(args):
    if args.input.resolve() == args.output.resolve():
        raise ValueError("input and output must differ")
    if args.limit is not None and args.limit < 1:
        raise ValueError("--limit must be positive")
    args.split = args.split or infer_split(args.source_file)
    source_hash = digest(args.input)
    counts, count, last_line = Counter(), 0, 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    name = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            newline="\n",
            dir=args.output.parent,
            delete=False,
        ) as out:
            name = Path(out.name)
            for start, end, tokens in sentences(args.input, args.drop_incomplete):
                case = make_case(start, end, tokens, args, source_hash)
                out.write(
                    json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n"
                )
                counts.update(token[0] for token in tokens)
                count += 1
                last_line = end
                if args.limit and count >= args.limit:
                    break
        if not count:
            raise ValueError("input contains no complete sentences")
        name.replace(args.output)
    finally:
        if name and name.exists():
            name.unlink()
    return {
        "sentences": count,
        "tokens": sum(counts.values()),
        "classes": dict(sorted(counts.items())),
        "last_source_line": last_line,
        "source_sha256": source_hash,
        "output_sha256": digest(args.output),
        "split": args.split,
        "partial_source": args.partial_source,
        "sentence_limit": args.limit,
        "drop_incomplete": args.drop_incomplete,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--source-file",
        required=True,
        help="original archive member, not a local alias",
    )
    parser.add_argument("--source-url", default=DATASET_URL)
    parser.add_argument("--revision", default="kaggle-version-1")
    parser.add_argument(
        "--split", help="default: infer train/dev/test from original shard number"
    )
    parser.add_argument(
        "--language", choices=("en-US", "ru-RU", "pl-PL"), default="en-US"
    )
    parser.add_argument(
        "--limit", type=int, help="maximum complete sentences from input prefix"
    )
    parser.add_argument(
        "--partial-source",
        action="store_true",
        help="input is an explicitly partial source artifact",
    )
    parser.add_argument("--drop-incomplete", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(convert(args), ensure_ascii=False, sort_keys=True))
    except (OSError, ValueError, UnicodeError) as exc:
        parser.exit(2, f"sproat import: {exc}\n")


if __name__ == "__main__":
    main()
