"""Shared, dependency-free benchmark schema and reproducibility helpers."""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATUSES = {"dataset-reference", "candidate", "unlabeled", "provided-reference"}
USES = {"benchmark-only", "noncommercial-benchmark-only"}


def ensure_output(path):
    """Keep derived external-data artifacts outside product/source directories."""
    path = Path(path).resolve()
    try:
        relative = path.relative_to(ROOT)
    except ValueError:
        return path
    if relative.parts[:2] != ("benchmark", "generated"):
        raise ValueError(
            "benchmark outputs inside the repository must be under benchmark/generated"
        )
    return path


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def tree_hash(root):
    root = Path(root)
    files = {
        p.relative_to(root).as_posix(): sha256(p)
        for p in sorted(root.rglob("*"))
        if p.is_file()
    }
    return hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest()


def digest_json(value):
    return hashlib.sha256(
        json.dumps(
            value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
        ).encode()
    ).hexdigest()


def validate(record):
    if not isinstance(record, dict) or record.get("schema_version") != 1:
        raise ValueError("unsupported benchmark record schema")
    for field in ("id", "dataset", "split", "language", "text"):
        if (
            not isinstance(record.get(field), str)
            or not record[field]
            or "\0" in record[field]
        ):
            raise ValueError(f"{field}: expected nonempty text without NUL")
    if not re.fullmatch(r"[A-Za-z0-9_.:/-]+", record["id"]):
        raise ValueError("unsafe record id")
    if (
        record.get("reference_status") not in STATUSES
        or record.get("usage") not in USES
    ):
        raise ValueError("invalid reference status or usage restriction")
    if (
        record["dataset"] == "coreaspeech"
        and record["usage"] != "noncommercial-benchmark-only"
    ):
        raise ValueError("CoreaSpeech data require noncommercial benchmark marking")
    if record.get("expected") is not None and not isinstance(record["expected"], str):
        raise ValueError("expected must be a string or null")
    if record["reference_status"] == "unlabeled" and record.get("expected") is not None:
        raise ValueError("unlabeled records cannot have a sentence reference")
    provenance = record.get("provenance")
    if not isinstance(provenance, dict) or not all(
        isinstance(provenance.get(k), str) and provenance[k]
        for k in ("source_url", "revision", "source_file", "source_sha256", "license")
    ):
        raise ValueError("source provenance is incomplete")
    if not re.fullmatch(r"[a-f0-9]{64}", provenance["source_sha256"]):
        raise ValueError("source_sha256 must identify the actual local input")
    raw = record["text"].encode("utf-8")
    spans = record.get("spans")
    if not isinstance(spans, list):
        raise ValueError("spans must be an array")  # noqa: TRY004 - schema errors share one exception contract
    end = 0
    for span in spans:
        if not isinstance(span, dict):
            raise ValueError("span must be an object")  # noqa: TRY004 - schema errors share one exception contract
        begin, stop = span.get("begin"), span.get("end")
        if (
            type(begin) is not int
            or type(stop) is not int
            or not end <= begin < stop <= len(raw)
        ):
            raise ValueError(
                "spans must be ordered, nonoverlapping UTF-8 byte intervals"
            )
        if raw[begin:stop].decode("utf-8") != span.get("written"):
            raise ValueError("span written text does not match its UTF-8 offsets")
        if (
            not isinstance(span.get("semiotic_class"), str)
            or not span["semiotic_class"]
        ):
            raise ValueError("missing source semiotic class")
        if span.get("reference_status") not in STATUSES:
            raise ValueError("invalid span reference status")
        if span.get("spoken") is not None and not isinstance(span["spoken"], str):
            raise ValueError("span spoken must be text or null")
        if span["reference_status"] == "unlabeled" and span.get("spoken") is not None:
            raise ValueError("unlabeled span cannot have a spoken reference")
        if span.get("spoken") is not None and "\0" in span["spoken"]:
            raise ValueError("span reference contains NUL")
        end = stop
    if record.get("expected") is not None:
        record["expected"].encode("utf-8")
        if "\0" in record["expected"]:
            raise ValueError("sentence reference contains NUL")
    return record


def read_records(path, limit=0):
    seen = set()
    with Path(path).open(encoding="utf-8") as handle:
        count = 0
        for line_number, line in enumerate(handle, 1):
            if not line.strip():
                continue
            try:
                record = validate(json.loads(line))
                if record["id"] in seen:
                    raise ValueError("duplicate record id")
                seen.add(record["id"])
            except (ValueError, TypeError, UnicodeError) as error:
                raise ValueError(f"{path}:{line_number}: {error}") from error
            yield record
            count += 1
            if limit and count >= limit:
                break
        if not count:
            raise ValueError("benchmark contains no records")
