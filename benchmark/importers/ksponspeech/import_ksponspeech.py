#!/usr/bin/env python3
"""Import local KsponSpeech dual annotations as TN candidates, never full gold.

Original stdlib implementation. No data download or upstream code execution.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from benchmark.common import validate

SOURCE_URL = "https://www.aihub.or.kr/aihubdata/data/view.do?dataSetSn=123"
FORMAT_REVISION = "54fe0cebd890e519d424ae9d339a57040b399108"
POLICY_URL = "https://aihub.or.kr/intrcn/guid/usagepolicy.do?currMenu=151&topMenu=105"
DATA_LICENSE = "AI-Hub dataset-specific terms; not the preprocessor MIT license"


def select_class(written, spoken):
    """Conservative selection, not a claim of linguistic correctness."""
    if not written.strip() or not spoken.strip():
        return None, "empty-dual-side"
    if re.search(r"[+*]|(?:^|\s)[blonu]/", written + " " + spoken) or "/" in spoken:
        return None, "transcription-or-disfluency-mark"
    if not re.search(r"[가-힣]", spoken):
        return None, "no-korean-spoken-reference"
    if written == spoken:
        return None, "unchanged-dual"
    if re.search(r"[0-9]", written):
        value = written.strip()
        if re.fullmatch(r"\d{2,3}-\d{3,4}-\d{4}", value):
            return "PHONE", "numeric-phone-shape"
        if re.fullmatch(r"\d{4}[-./]\s*\d{1,2}[-./]\s*\d{1,2}\.?", value) or re.search(
            r"\d+년.*\d+월", value
        ):
            return "DATE", "numeric-date-shape"
        if re.fullmatch(r"\d{1,2}:\d{2}(?::\d{2})?", value):
            return "TIME", "numeric-time-shape"
        if re.search(r"\d\s*[~∼〜–]\s*\d", value):
            return "RANGE", "numeric-range-shape"
        if "%" in value or "퍼센트" in value:
            return "PERCENT", "numeric-percent-marker"
        if re.search(r"[₩$€£¥]|원|달러|유로", value):
            return "CURRENCY", "numeric-currency-marker"
        if re.fullmatch(r"\d+\s*/\s*\d+", value):
            return "FRACTION", "numeric-fraction-shape"
        if re.fullmatch(r"\d+(?:st|nd|rd|th)", value, re.IGNORECASE):
            return "ORDINAL", "numeric-ordinal-suffix"
        if re.search(r"\d\s*(?:개|명|살|시|권|마리|대|잔)(?:\s|$)", value):
            return "COUNTER", "numeric-counter-marker"
        if re.search(
            r"\d\s*(?:kg|km|cm|mm|ml|gb|ghz|hz|초|분|킬로그램)(?:\s|$)",
            value,
            re.IGNORECASE,
        ):
            return "MEASURE", "numeric-unit-marker"
        if re.search(r"[A-Za-z]", value):
            return "ALNUM_TERM", "numeric-latin-token"
        return "NUMBER", "contains-ascii-number"
    if re.search(r"[A-Za-z]", written):
        if re.fullmatch(r"[A-Z](?:[A-Z.]|\s)*", written.strip()):
            return "INITIALISM", "uppercase-latin-candidate"
        return "WORD", "latin-pronunciation-candidate"
    if re.search(r"[%₩$€£¥&@#~∼〜]", written):
        return "SYMBOL", "symbol-pronunciation-candidate"
    return None, "orthography-or-pronunciation-only"


def reconstruct(raw):
    """Replace strict (written)/(spoken) pairs with written sides.

    Literal balanced parentheses are preserved. Nested/unbalanced annotations
    fail explicitly. raw_*_character offsets describe the decoded original.
    """
    parts, pairs = [], []
    cursor = output_bytes = 0
    while cursor < len(raw):
        if raw[cursor] == ")":
            raise ValueError("unmatched closing parenthesis in transcript")
        if raw[cursor] != "(":
            end = cursor + 1
            while end < len(raw) and raw[end] not in "()":
                end += 1
            fragment = raw[cursor:end]
            parts.append(fragment)
            output_bytes += len(fragment.encode("utf-8"))
            cursor = end
            continue
        close = raw.find(")", cursor + 1)
        if close < 0 or "(" in raw[cursor + 1 : close]:
            raise ValueError("nested or unclosed parenthesis in transcript")
        if not raw.startswith("/(", close + 1):
            fragment = raw[cursor : close + 1]
            parts.append(fragment)
            output_bytes += len(fragment.encode("utf-8"))
            cursor = close + 1
            continue
        spoken_begin = close + 3
        stop = raw.find(")", spoken_begin)
        if stop < 0 or "(" in raw[spoken_begin:stop]:
            raise ValueError("nested or unclosed spoken annotation")
        written, spoken = raw[cursor + 1 : close], raw[spoken_begin:stop]
        if not written:
            raise ValueError("empty written side in dual annotation")
        size = len(written.encode("utf-8"))
        pairs.append(
            {
                "begin": output_bytes,
                "end": output_bytes + size,
                "written": written,
                "spoken": spoken,
                "raw_begin_character": cursor,
                "raw_end_character": stop + 1,
            }
        )
        parts.append(written)
        output_bytes += size
        cursor = stop + 1
    return "".join(parts), pairs


def byte_offsets(raw, data, encoding):
    base = "utf-8" if encoding == "utf-8-sig" else encoding
    bom = (
        b"\xef\xbb\xbf"
        if encoding == "utf-8-sig" and data.startswith(b"\xef\xbb\xbf")
        else b""
    )
    if bom + raw.encode(base) != data:
        raise ValueError(
            "source encoding does not round-trip; cannot prove raw byte offsets"
        )
    offsets = [len(bom)]
    for character in raw:
        offsets.append(offsets[-1] + len(character.encode(base)))
    return offsets


def import_file(
    path, *, source_file=None, split, encoding="cp949", revision="local-file-sha256"
):
    path = Path(path)
    data = path.read_bytes()
    raw = data.decode(encoding, errors="strict")
    if "\0" in raw:
        raise ValueError("transcript contains NUL")
    text, pairs = reconstruct(raw)
    offsets = byte_offsets(raw, data, encoding)
    spans, excluded = [], []
    for pair in pairs:
        pair["raw_begin_byte"] = offsets[pair["raw_begin_character"]]
        pair["raw_end_byte"] = offsets[pair["raw_end_character"]]
        semiotic_class, reason = select_class(pair["written"], pair["spoken"])
        pair["selection_reason"] = reason
        if semiotic_class is None:
            excluded.append(pair)
        else:
            pair.update(semiotic_class=semiotic_class, reference_status="candidate")
            spans.append(pair)
    digest = hashlib.sha256(data).hexdigest()
    relative = source_file or path.name
    identity = hashlib.sha256((relative + "\0" + digest).encode("utf-8")).hexdigest()[
        :24
    ]
    return validate(
        {
            "schema_version": 1,
            "id": f"ksponspeech:{split}:{identity}",
            "dataset": "ksponspeech",
            "split": split,
            "language": "ko-KR",
            "text": text,
            "expected": None,
            "reference_status": "candidate" if spans else "unlabeled",
            "usage": "benchmark-only",
            "provenance": {
                "source_url": SOURCE_URL,
                "revision": revision,
                "source_file": relative,
                "source_sha256": digest,
                "license": DATA_LICENSE,
                "policy_url": POLICY_URL,
                "format_reference_revision": FORMAT_REVISION,
                "encoding": encoding,
            },
            "spans": spans,
            "metadata": {
                "annotation_kind": "orthographic/phonetic; TN candidates require review",
                "selection_version": "kspon-tn-candidates-v1",
                "excluded_dual_annotations": excluded,
                "dual_annotation_count": len(pairs),
                "whitespace_policy": "preserve; no reference rewriting",
                "whole_sentence_reference_available": False,
            },
        }
    )


def safe_output(path, inputs):
    resolved = Path(path).resolve()
    if resolved in {Path(p).resolve() for p in inputs}:
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
    temp_name = None
    try:
        with tempfile.NamedTemporaryFile(
            "w", encoding="utf-8", dir=path.parent, delete=False
        ) as output:
            temp_name = output.name
            for record in records:
                output.write(
                    json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n"
                )
        os.replace(temp_name, path)
    finally:
        if temp_name and os.path.exists(temp_name):
            os.unlink(temp_name)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "input", type=Path, help="local .txt file or directory of .txt files"
    )
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--split", required=True, help="explicit original split, e.g. eval-clean"
    )
    parser.add_argument(
        "--encoding", choices=("cp949", "euc-kr", "utf-8", "utf-8-sig"), default="cp949"
    )
    parser.add_argument(
        "--revision",
        default="local-file-sha256",
        help="dataset release ID if known; actual bytes are always hashed",
    )
    parser.add_argument(
        "--include-unlabeled",
        action="store_true",
        help="also retain files without selected TN spans",
    )
    parser.add_argument(
        "--acknowledge-data-rights",
        action="store_true",
        help="confirm rights for this local use, including AI Hub use/export terms; grants no permission",
    )
    args = parser.parse_args(argv)
    if not args.acknowledge_data_rights:
        parser.error(
            "--acknowledge-data-rights is required; code MIT does not license dataset use"
        )
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", args.split):
        parser.error("split must use letters, digits, underscore, dot or hyphen")
    try:
        if args.input.is_file():
            paths, base = [args.input], args.input.parent
        elif args.input.is_dir():
            paths, base = sorted(args.input.rglob("*.txt")), args.input
        else:
            raise ValueError("input file or directory does not exist")
        if not paths:
            raise ValueError("input contains no .txt transcripts")
        output = safe_output(args.output, paths)
        stats = Counter()

        def records():
            for path in paths:
                try:
                    record = import_file(
                        path,
                        source_file=path.relative_to(base).as_posix(),
                        split=args.split,
                        encoding=args.encoding,
                        revision=args.revision,
                    )
                except (ValueError, UnicodeError) as error:
                    raise ValueError(f"{path}: {error}") from error
                stats["source_files"] += 1
                stats["selected_spans"] += len(record["spans"])
                stats["excluded_dual_spans"] += len(
                    record["metadata"]["excluded_dual_annotations"]
                )
                if record["spans"] or args.include_unlabeled:
                    record["provenance"]["rights_acknowledged_by_invoker"] = True
                    stats["emitted_records"] += 1
                    yield record
                else:
                    stats["files_without_tn_candidates"] += 1

        atomic_jsonl(output, records())
        print(
            json.dumps(
                {
                    "output": str(output),
                    "counts": dict(stats),
                    "sentence_gold_records": 0,
                    "reference_policy": "selected dual spans are candidates, not gold",
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
