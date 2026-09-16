#!/usr/bin/env python3
"""Import rights-cleared local CoreaSpeech JSONL exports for optional NC evaluation.

Numeric text1/text2 are different utterances, not written/spoken pairs. Generated
N2gkPlus fields are never references. No download, audio loading, or upstream code.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from benchmark.common import sha256, validate

SOURCES = {
    "universal": {
        "url": "https://huggingface.co/datasets/aanonyyy/M6A5P1Q7",
        "revision": "9829037ec950e08db7ef499656cae69c7157bd6b",
        "fields": ("text1", "text2"),
    },
    "training": {
        "url": "https://huggingface.co/datasets/aanonyyy/F5I9N7A1",
        "revision": "950d6a50b2b3e338d141313d11440fe47b037745",
        "fields": ("text",),
    },
}
CODE_REVISION = "424229a8affea08848e2e647c0d3e471b3ef9d39"
DATA_LICENSE = "CC-BY-NC-4.0 claim conflicts with official dataset-card no-modification/no-redistribution terms; permission unresolved"
TOKENS = re.compile(r"[0-9]+(?:[.,:/~%-][0-9]+)*|[A-Za-z]+(?:[-'][A-Za-z]+)*|[₩$€£%]")


def discover_spans(text):
    """Unlabeled lexical regions for gap inspection, not TN class ground truth."""
    spans = []
    for match in TOKENS.finditer(text):
        written = match.group()
        semiotic_class = (
            "NUMBER"
            if written[0].isascii() and written[0].isdigit()
            else "WORD"
            if written[0].isascii() and written[0].isalpha()
            else "SYMBOL"
        )
        spans.append(
            {
                "begin": len(text[: match.start()].encode("utf-8")),
                "end": len(text[: match.end()].encode("utf-8")),
                "written": written,
                "spoken": None,
                "semiotic_class": semiotic_class,
                "reference_status": "unlabeled",
                "selection_reason": "lexical-shape-only; not semantic ground truth",
            }
        )
    return spans


def import_rows(
    path,
    *,
    split,
    dataset_part="universal",
    revision=None,
    references=None,
    reference_provenance=None,
):
    path = Path(path)
    source = SOURCES[dataset_part]
    digest = sha256(path)
    references = references or {}
    used_references = set()
    with path.open(encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, 1):
            if not line.strip():
                continue
            try:
                row = json.loads(line)
                if not isinstance(row, dict):
                    raise TypeError("row must be an object")
                for field in source["fields"]:
                    text = row.get(field)
                    if not isinstance(text, str) or not text.strip():
                        raise ValueError(
                            f"missing nonempty {field}; export the official text fields explicitly"
                        )
                    identity = (
                        f"coreaspeech:{split}:{digest[:24]}:{line_number}:{field}"
                    )
                    record = {
                        "schema_version": 1,
                        "id": identity,
                        "dataset": "coreaspeech",
                        "split": split,
                        "language": "ko-KR",
                        "text": text,
                        "expected": None,
                        "reference_status": "unlabeled",
                        "usage": "noncommercial-benchmark-only",
                        "provenance": {
                            "source_url": source["url"],
                            "revision": revision or source["revision"],
                            "source_file": path.name,
                            "source_sha256": digest,
                            "license": DATA_LICENSE,
                            "license_conflict": True,
                            "source_line": line_number,
                            "text_field": field,
                            "source_code_revision": CODE_REVISION,
                        },
                        "spans": discover_spans(text),
                        "metadata": {
                            "dataset_part": dataset_part,
                            "selection_version": "corea-unlabeled-v1",
                            "reference_warning": "cross-sentence audio test; no TN reference implied",
                            "generated_normalization_ignored": "N2gkPlus" in row
                            or "N2gk" in row,
                            "whitespace_policy": "preserve; no reference rewriting",
                        },
                    }
                    if identity in references:
                        reference = references[identity]
                        if reference["text"] != text:
                            raise ValueError(
                                "provided reference text does not match source text"
                            )
                        record["expected"] = reference.get("expected")
                        record["reference_status"] = "provided-reference"
                        if "spans" in reference:
                            record["spans"] = [
                                dict(span, reference_status="provided-reference")
                                for span in reference["spans"]
                            ]
                        record["provenance"]["provided_reference"] = (
                            reference_provenance or {}
                        )
                        record["metadata"]["reference_warning"] = (
                            "caller-provided reference; no human approval or correctness inferred"
                        )
                        used_references.add(identity)
                    yield validate(record)
            except (ValueError, TypeError, KeyError, UnicodeError) as error:
                raise ValueError(f"{path}:{line_number}: {error}") from error
    unused = sorted(set(references) - used_references)
    if unused:
        raise ValueError(
            f"{len(unused)} provided reference IDs were not found in this export"
        )


def load_references(path):
    references = {}
    with Path(path).open(encoding="utf-8") as handle:
        for number, line in enumerate(handle, 1):
            if not line.strip():
                continue
            try:
                row = json.loads(line)
                if (
                    not isinstance(row, dict)
                    or not isinstance(row.get("id"), str)
                    or not isinstance(row.get("text"), str)
                ):
                    raise TypeError("reference requires id and exact source text")
                if row["id"] in references:
                    raise ValueError("duplicate reference id")
                if row.get("expected") is not None and not isinstance(
                    row["expected"], str
                ):
                    raise ValueError("expected must be text or null")
                if "spans" in row:
                    if not isinstance(row["spans"], list) or not row["spans"]:
                        raise ValueError("provided spans must be a nonempty array")
                    for span in row["spans"]:
                        if not isinstance(span, dict) or not isinstance(
                            span.get("spoken"), str
                        ):
                            raise TypeError(
                                "each provided span needs a spoken reference"
                            )
                if row.get("expected") is None and not row.get("spans"):
                    raise ValueError(
                        "reference needs expected text or explicit referenced spans"
                    )
                references[row["id"]] = row
            except (ValueError, TypeError) as error:
                raise ValueError(f"{path}:{number}: {error}") from error
    return references


def safe_output(path, inputs):
    resolved = Path(path).resolve()
    if resolved in {Path(p).resolve() for p in inputs if p is not None}:
        raise ValueError("output must not replace an input or reference file")
    if resolved.is_relative_to(ROOT) and not resolved.is_relative_to(
        ROOT / "benchmark/generated"
    ):
        raise ValueError(
            "repository data outputs are allowed only under benchmark/generated/"
        )
    return resolved


def atomic_jsonl(path, records):
    path.parent.mkdir(parents=True, exist_ok=True)
    name = None
    try:
        with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", dir=path.parent, delete=False
        ) as output:
            name = output.name
            for record in records:
                output.write(
                    json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n"
                )
        os.replace(name, path)
    finally:
        if name and os.path.exists(name):
            os.unlink(name)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "input", type=Path, help="local UTF-8 JSONL export; no parquet/audio is loaded"
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--split", required=True, help="explicit source split, normally numeric"
    )
    parser.add_argument("--dataset-part", choices=tuple(SOURCES), default="universal")
    parser.add_argument(
        "--revision",
        help="actual upstream dataset revision when different from the documented pin",
    )
    parser.add_argument(
        "--references",
        type=Path,
        help="optional local JSONL {id,text,expected and/or spans}",
    )
    parser.add_argument(
        "--allow-noncommercial",
        action="store_true",
        help="opt in to a separate noncommercial benchmark; does not grant permission",
    )
    parser.add_argument(
        "--acknowledge-data-rights",
        action="store_true",
        help="confirm rights for this use despite unresolved dataset-card licensing conflict",
    )
    args = parser.parse_args(argv)
    if not args.allow_noncommercial:
        parser.error(
            "--allow-noncommercial is required; this dataset is never a product resource"
        )
    if not args.acknowledge_data_rights:
        parser.error(
            "--acknowledge-data-rights is required; NC opt-in does not resolve the license conflict"
        )
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", args.split):
        parser.error("split must use letters, digits, underscore, dot or hyphen")
    try:
        output = safe_output(args.output, (args.input, args.references))
        refs = load_references(args.references) if args.references else {}
        reference_provenance = (
            {
                "source_file": args.references.name,
                "source_sha256": sha256(args.references),
            }
            if args.references
            else None
        )
        stats = Counter()

        def records():
            for record in import_rows(
                args.input,
                split=args.split,
                dataset_part=args.dataset_part,
                revision=args.revision,
                references=refs,
                reference_provenance=reference_provenance,
            ):
                record["provenance"]["rights_acknowledged_by_invoker"] = True
                stats["emitted_records"] += 1
                stats[record["reference_status"]] += 1
                yield record
            if not stats["emitted_records"]:
                raise ValueError("input contains no records")

        atomic_jsonl(output, records())
        print(
            json.dumps(
                {
                    "output": str(output),
                    "counts": dict(stats),
                    "usage": "noncommercial-benchmark-only",
                    "license_conflict": True,
                },
                ensure_ascii=False,
            ),
            file=sys.stderr,
        )
        return 0
    except (OSError, ValueError, UnicodeError) as error:
        parser.exit(2, f"error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
