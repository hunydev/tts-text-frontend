#!/usr/bin/env python3
"""Evaluate normalized text, language metadata, and batch/stream parity (stdlib only)."""
from __future__ import annotations

import argparse
from collections import Counter
import csv
import difflib
import hashlib
import html
import json
from pathlib import Path
import re
import subprocess
import sys
import time


class EvaluationError(ValueError):
    """The corpus or frontend protocol cannot be evaluated reliably."""


SPAN_FIELDS = ("script", "source_lang", "context_lang", "render_lang", "semantic_type",
               "value", "secondary", "unit", "rule", "resource_layer", "pronunciation_layer", "lexicon",
               "verbalizer", "context_trace")
CHECK_FIELDS = set(SPAN_FIELDS) | {"normalized"}
FRONTEND_DEFAULTS = {"output_mode": "text", "input_format": "plain",
                     "unreadable": "preserve", "brackets": "read", "unknown_foreign": "guess",
                     "max_control_bytes": 4096}


def frontend_settings(case: dict) -> dict:
    """Effective legacy-compatible settings; missing fields keep old reports valid."""
    result = dict(FRONTEND_DEFAULTS)
    for field, choices in (("output_mode", ("text", "speech")),
                           ("input_format", ("plain", "annotated")),
                           ("unreadable", ("preserve", "drop")),
                           ("brackets", ("read", "skip", "preserve")),
                           ("unknown_foreign", ("guess", "spell", "preserve"))):
        if field in case:
            if not isinstance(case[field], str) or case[field] not in choices:
                raise EvaluationError(f"{field}: expected {'|'.join(choices)}")
            result[field] = case[field]
    if "max_control_bytes" in case:
        value = case["max_control_bytes"]
        if type(value) is not int or value <= 0:
            raise EvaluationError("max_control_bytes: expected a positive integer")
        result["max_control_bytes"] = value
    return result


def frontend_flags(case: dict) -> list[str]:
    """Only explicit options are sent, so old corpora still run on old CLIs."""
    settings = frontend_settings(case)
    return [value for field in FRONTEND_DEFAULTS if field in case
            for value in ("--" + field.replace("_", "-"), str(settings[field]))]


def integer(value: object, field: str) -> int:
    if type(value) is not int or value < 0:
        raise EvaluationError(f"{field}: expected a nonnegative integer")
    return value


def surface_range(text: str, surface: str, occurrence: int) -> tuple[int, int]:
    position = -len(surface)
    for _ in range(occurrence + 1):
        position = text.find(surface, position + len(surface))
        if position < 0:
            raise EvaluationError(f"expected span surface {surface!r} occurrence {occurrence} is absent")
    begin = len(text[:position].encode("utf-8"))
    return begin, begin + len(surface.encode("utf-8"))


def read_corpus(path: Path) -> list[dict]:
    cases = []
    identifiers = set()
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            case = json.loads(line)
        except json.JSONDecodeError as exc:
            raise EvaluationError(f"corpus line {number}: {exc.msg}") from exc
        if not isinstance(case, dict):
            raise EvaluationError(f"corpus line {number}: expected an object")
        for key in ("id", "text", "expected"):
            if not isinstance(case.get(key), str):
                raise EvaluationError(f"corpus line {number}: {key} must be a string")
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,79}", case["id"]):
            raise EvaluationError(f"corpus line {number}: unsafe or empty id")
        if case["id"] in identifiers:
            raise EvaluationError(f"corpus line {number}: duplicate id {case['id']}")
        if any("\x00" in case[key] for key in ("text", "expected")):
            raise EvaluationError(f"corpus line {number}: embedded NUL is unsupported")
        for key in ("default_language", "notes", "category"):
            if key in case and not isinstance(case[key], str):
                raise EvaluationError(f"corpus line {number}: {key} must be a string")
        case.setdefault("default_language", "ko-KR")
        case.setdefault("notes", "")
        case.setdefault("category", "uncategorized")
        frontend_settings(case)
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_/-]{0,79}", case["category"]):
            raise EvaluationError(f"corpus line {number}: invalid category")
        if not re.fullmatch(r"[A-Za-z]{2,8}(?:-[A-Za-z0-9]{1,8})*", case["default_language"]):
            raise EvaluationError(f"corpus line {number}: invalid default_language")
        try:
            case["text"].encode("utf-8")
            case["expected"].encode("utf-8")
        except UnicodeError as exc:
            raise EvaluationError(f"corpus line {number}: invalid Unicode") from exc
        overrides = case.setdefault("resource_overrides", [])
        if not isinstance(overrides, list):
            raise EvaluationError(f"corpus line {number}: resource_overrides must be an array")
        for override in overrides:
            if not isinstance(override, dict) or set(override) != {"layer", "path"}:
                raise EvaluationError(f"corpus line {number}: invalid resource override")
            if override["layer"] not in ("domain", "customer") or not isinstance(override["path"], str):
                raise EvaluationError(f"corpus line {number}: invalid resource override layer/path")
            relative = Path(override["path"])
            if not override["path"] or relative.is_absolute() or ".." in relative.parts or "\\" in override["path"] or ":" in override["path"]:
                raise EvaluationError(f"corpus line {number}: resource path must stay inside resources")
        checks = case.setdefault("expected_spans", [])
        if not isinstance(checks, list):
            raise EvaluationError(f"corpus line {number}: expected_spans must be an array")
        for check in checks:
            if not isinstance(check, dict) or not isinstance(check.get("surface"), str) or not check["surface"]:
                raise EvaluationError(f"corpus line {number}: expected span needs a nonempty surface")
            unknown = set(check) - CHECK_FIELDS - {"surface", "occurrence"}
            if unknown or not (set(check) & CHECK_FIELDS):
                raise EvaluationError(f"corpus line {number}: invalid or empty expected span fields")
            for key in set(check) & CHECK_FIELDS:
                if not isinstance(check[key], str):
                    raise EvaluationError(f"corpus line {number}: expected span {key} must be a string")
            occurrence = integer(check.get("occurrence", 0), "expected span occurrence")
            surface_range(case["text"], check["surface"], occurrence)
        identifiers.add(case["id"])
        cases.append(case)
    if not cases:
        raise EvaluationError("corpus has no cases")
    return cases


def parse_stream(payload: str, original: str) -> list[dict]:
    """Require complete input/output coverage, metadata, and valid UTF-8 boundaries."""
    segments = []
    source = original.encode("utf-8")
    input_at = 0
    ended = False
    for number, line in enumerate(payload.splitlines(), 1):
        try:
            event = json.loads(line)
        except json.JSONDecodeError as exc:
            raise EvaluationError(f"stream line {number}: invalid JSON") from exc
        if not isinstance(event, dict) or type(event.get("version")) is not int or event["version"] != 1:
            raise EvaluationError(f"stream line {number}: expected protocol version 1")
        if ended:
            raise EvaluationError("stream contains data after its end event")
        if event.get("event") == "end":
            if integer(event.get("segment_count"), "segment_count") != len(segments):
                raise EvaluationError("stream segment_count does not match")
            if integer(event.get("input_bytes"), "input_bytes") != len(source) or input_at != len(source):
                raise EvaluationError("stream did not consume the complete input")
            ended = True
            continue
        if event.get("event") != "segment":
            raise EvaluationError("stream has an unknown event")
        if integer(event.get("sequence"), "sequence") != len(segments):
            raise EvaluationError("stream segment sequence is not contiguous")
        begin = integer(event.get("input_begin"), "input_begin")
        end = integer(event.get("input_end"), "input_end")
        if begin != input_at or not begin < end <= len(source):
            raise EvaluationError("stream input offsets are not contiguous")
        if not isinstance(event.get("text"), str) or not isinstance(event.get("spans"), list):
            raise EvaluationError("stream segment text/spans have invalid types")
        normalized = event["text"].encode("utf-8")
        output_at = 0
        source_at = begin
        for span in event["spans"]:
            if not isinstance(span, dict):
                raise EvaluationError("stream span must be an object")
            span_begin = integer(span.get("begin"), "span.begin")
            span_end = integer(span.get("end"), "span.end")
            source_begin = integer(span.get("input_begin"), "span.input_begin")
            source_end = integer(span.get("input_end"), "span.input_end")
            if span_begin != output_at or not span_begin <= span_end <= len(normalized):
                raise EvaluationError("stream output span offsets are not contiguous")
            if source_begin != source_at or not begin <= source_begin < source_end <= end:
                raise EvaluationError("stream input span offsets are not contiguous")
            for key in SPAN_FIELDS:
                if not isinstance(span.get(key), str):
                    raise EvaluationError(f"stream span.{key} must be a string")
            try:
                normalized[span_begin:span_end].decode("utf-8")
                source[source_begin:source_end].decode("utf-8")
            except UnicodeError as exc:
                raise EvaluationError("stream span splits a UTF-8 character") from exc
            output_at = span_end
            source_at = source_end
        if output_at != len(normalized) or source_at != end:
            raise EvaluationError("stream spans do not cover input and normalized text")
        segments.append(event)
        input_at = end
    if not ended:
        raise EvaluationError("stream is missing its end event")
    return segments


def check_spans(case: dict, segments: list[dict]) -> list[dict]:
    failures = []
    spans = []
    for segment in segments:
        output = segment["text"].encode("utf-8")
        for span in segment["spans"]:
            spans.append(dict(span, normalized=output[span["begin"]:span["end"]].decode("utf-8")))
    for index, check in enumerate(case["expected_spans"]):
        begin, end = surface_range(case["text"], check["surface"], check.get("occurrence", 0))
        actual = next((span for span in spans if span["input_begin"] == begin and span["input_end"] == end), None)
        if actual is None:
            failures.append({"check": index, "surface": check["surface"], "input_begin": begin,
                             "input_end": end, "error": "no span with these exact source offsets"})
            continue
        for key in sorted(set(check) & CHECK_FIELDS):
            if check[key] != actual[key]:
                failures.append({"check": index, "surface": check["surface"], "field": key,
                                 "expected": check[key], "actual": actual[key]})
    return failures


def describe_spans(segments: list[dict]) -> dict:
    paths = Counter()
    languages = Counter()
    semantics = Counter()
    fallback = 0
    lexicon_hits = 0
    for segment in segments:
        for span in segment["spans"]:
            path = span["lexicon"]
            if path:
                paths[path] += 1
            fallback += int("fallback" in path or "prototype" in path)
            lexicon_hits += int(path.startswith("lexicon:"))
            languages[(span["source_lang"], span["context_lang"], span["render_lang"])] += 1
            semantics[span["semantic_type"]] += 1
    return {
        "fallback_spans": fallback,
        "lexicon_hit_spans": lexicon_hits,
        "pronunciation_paths": dict(sorted(paths.items())),
        "semantic_types": dict(sorted(semantics.items())),
        "language_routes": [
            {"source": source, "context": context, "render": render, "spans": count}
            for (source, context, render), count in sorted(languages.items())
        ],
    }


def text_difference(expected: str, actual: str) -> dict:
    """Exact codepoint Levenshtein distance; display edits use difflib's alignment."""
    distance = 0
    if expected != actual:
        row = list(range(len(actual) + 1))
        for i, left in enumerate(expected, 1):
            following = [i]
            for j, right in enumerate(actual, 1):
                following.append(min(row[j] + 1, following[-1] + 1, row[j - 1] + (left != right)))
            row = following
        distance = row[-1]
    edits = []
    for tag, i, j, a, b in difflib.SequenceMatcher(None, expected, actual, autojunk=False).get_opcodes():
        if tag != "equal":
            edits.append({"operation": tag, "expected_begin": i, "expected_end": j,
                          "actual_begin": a, "actual_end": b, "expected": expected[i:j],
                          "actual": actual[a:b]})
    return {"character_edit_distance": distance, "reference_characters": len(expected),
            "character_error_rate": distance / max(1, len(expected)), "edits": edits}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def summarize(cases: list[dict]) -> dict:
    distance = sum(case["difference"]["character_edit_distance"] for case in cases)
    reference = sum(case["difference"]["reference_characters"] for case in cases)
    matches = sum(case["matches_expected"] for case in cases)
    passed = sum(case["passed"] for case in cases)
    return {"cases": len(cases), "passed": passed, "failed": len(cases) - passed,
            "matches": matches, "mismatches": len(cases) - matches,
            "exact_match_rate": matches / len(cases) if cases else 0.0,
            "metadata_failures": sum(bool(case["metadata_failures"]) for case in cases),
            "batch_stream_failures": sum(not case["batch_stream_match"] for case in cases),
            "character_edit_distance": distance, "reference_characters": reference,
            "character_error_rate": distance / max(1, reference),
            "fallback_spans": sum(case["metadata"]["fallback_spans"] for case in cases)}


def write_html(path: Path, report: dict) -> None:
    esc = html.escape
    parts = [
        '<!doctype html><html lang="en"><meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width,initial-scale=1">',
        '<title>TTS frontend text evaluation</title>',
        '<style>body{max-width:1100px;margin:2rem auto;padding:0 1rem;font:16px/1.5 sans-serif}',
        'article{border:1px solid #aaa;border-radius:8px;padding:1rem;margin:1rem 0}',
        'pre{white-space:pre-wrap;overflow-wrap:anywhere}table{border-collapse:collapse}',
        'th,td{padding:.4rem .7rem;border:1px solid #aaa;text-align:left}',
        '.fail{border-color:#b22}.pass{border-color:#285b35}code{white-space:pre-wrap}</style>',
        '<h1>TTS frontend text evaluation</h1>',
        '<p>References are explicit corpus expectations. All spaces, punctuation, case, and Unicode ',
        'codepoints are significant. A passing case requires exact text, expected metadata, ',
        'and batch/stream parity.</p>',
        '<pre>' + esc(json.dumps(report["summary"], ensure_ascii=False, indent=2)) + '</pre>',
        '<h2>Categories</h2><table><tr><th>Category</th><th>Passed / cases</th>',
        '<th>Text mismatches</th><th>Metadata failures</th><th>Parity failures</th><th>CER</th></tr>',
    ]
    for category, summary in sorted(report["categories"].items()):
        parts.append('<tr><td>' + esc(category) + '</td><td>' + str(summary["passed"]) + ' / ' +
                     str(summary["cases"]) + '</td><td>' + str(summary["mismatches"]) + '</td><td>' +
                     str(summary["metadata_failures"]) + '</td><td>' + str(summary["batch_stream_failures"]) +
                     '</td><td>' + f'{summary["character_error_rate"]:.4f}' + '</td></tr>')
    parts.append('</table><p>Failures appear first. Detailed edit offsets use Unicode codepoints; '
                 'protocol span offsets use UTF-8 bytes.</p>')
    for case in sorted(report["cases"], key=lambda item: item["passed"]):
        parts.append('<article class="' + ('pass' if case["passed"] else 'fail') + '"><h2>' +
                     esc(case["id"]) + ' — ' + ('PASS' if case["passed"] else 'FAIL') + '</h2>')
        parts.append('<p>Category: ' + esc(case["category"]) + '; exact text: ' + str(case["matches_expected"]).lower() +
                     '; batch/stream parity: ' + str(case["batch_stream_match"]).lower() + '</p>')
        for label, key in (("Input", "text"), ("Expected", "expected"), ("Normalized", "normalized")):
            parts.append('<h3>' + label + '</h3><pre>' + esc(case[key]) + '</pre>')
        parts.append('<p>' + esc(case["notes"]) + '</p>')
        if not case["passed"]:
            for label, key in (("Character differences", "difference"), ("Metadata failures", "metadata_failures"),
                               ("Streaming variants", "stream_variants")):
                parts.append('<h3>' + label + '</h3><pre>' +
                             esc(json.dumps(case[key], ensure_ascii=False, indent=2)) + '</pre>')
        parts.append('<details><summary>Semantic classes, resource paths, and language routes</summary><pre>' +
                     esc(json.dumps(case["metadata"], ensure_ascii=False, indent=2)) + '</pre></details></article>')
    parts.append('</html>')
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")


def frontend(command: list[str], case_id: str) -> tuple[str, float]:
    started = time.perf_counter()
    process = subprocess.run(command, capture_output=True, timeout=60)
    elapsed = time.perf_counter() - started
    if process.returncode:
        detail = process.stderr.decode("utf-8", errors="replace").strip()
        raise EvaluationError(f"{case_id}: frontend failed: {detail}")
    return process.stdout.decode("utf-8"), round(elapsed, 6)


def evaluate(args: argparse.Namespace) -> dict:
    cases = read_corpus(args.corpus)
    cli = args.ttf.resolve(strict=True)
    resources = args.resources.resolve(strict=True)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    results = []
    for case in cases:
        directory = args.output_dir / case["id"]
        directory.mkdir()
        input_file = directory / "input.txt"
        input_file.write_bytes(case["text"].encode("utf-8"))
        flags = ["--resources", str(resources), "--language", case["default_language"],
                 *frontend_flags(case)]
        for override in case["resource_overrides"]:
            resource_path = (resources / override["path"]).resolve(strict=True)
            try:
                resource_path.relative_to(resources)
            except ValueError as exc:
                raise EvaluationError("resource override escapes resource directory") from exc
            flags.extend(["--" + override["layer"], str(resource_path)])
        batch, batch_seconds = frontend([str(cli), "normalize", *flags, "--", case["text"]], case["id"])
        if not batch.endswith("\n"):
            raise EvaluationError(f"{case['id']}: normalize output is missing its final CLI newline")
        batch = batch[:-1]  # Remove exactly the CLI newline; never strip input whitespace.
        variants = []
        canonical = None
        metadata_failures = []
        for chunk in args.chunk_bytes:
            payload, seconds = frontend([str(cli), "stream-json", *flags, "--chunk-bytes", str(chunk),
                                         str(input_file.resolve())], case["id"])
            segments = parse_stream(payload, case["text"])
            normalized = "".join(segment["text"] for segment in segments)
            errors = check_spans(case, segments)
            metadata_failures.extend(dict(error, chunk_bytes=chunk) for error in errors)
            filename = "segments.jsonl" if canonical is None else f"segments-{chunk}.jsonl"
            (directory / filename).write_text(payload, encoding="utf-8")
            variants.append({"chunk_bytes": chunk, "normalized": normalized, "matches_batch": normalized == batch,
                             "matches_expected": normalized == case["expected"], "segment_count": len(segments),
                             "metadata_failures": errors, "frontend_process_seconds": seconds})
            if canonical is None:
                canonical = segments
        normalized = variants[0]["normalized"]
        parity = all(variant["matches_batch"] for variant in variants)
        matches = normalized == case["expected"]
        recorded_case = dict(case, **frontend_settings(case))
        results.append(dict(recorded_case, normalized=normalized, batch_normalized=batch, matches_expected=matches,
                            batch_stream_match=parity, metadata_failures=metadata_failures,
                            passed=matches and parity and not metadata_failures,
                            difference=text_difference(case["expected"], normalized),
                            segment_count=len(canonical), metadata=describe_spans(canonical),
                            batch_process_seconds=batch_seconds, stream_variants=variants))
    summary = summarize(results)
    report = {
        "version": 2, "summary": summary,
        "categories": {category: summarize([case for case in results if case["category"] == category])
                       for category in sorted({case["category"] for case in results})},
        "provenance": {"ttf": str(cli), "ttf_sha256": sha256(cli),
                       "corpus": str(args.corpus.resolve()), "corpus_sha256": sha256(args.corpus),
                       "resources": str(resources),
                       "resource_sha256": {p.relative_to(resources).as_posix(): sha256(p)
                                           for p in sorted(resources.rglob("*")) if p.is_file()},
                       "chunk_bytes": args.chunk_bytes, "python": sys.version},
        "metric_note": "CER is codepoint Levenshtein distance / max(1, reference codepoints), "
                       "without Unicode, whitespace, punctuation, or case folding. Display edits "
                       "use difflib alignment; they are not the Levenshtein operation sequence.",
        "timing_note": "Process timings include CLI startup and resource loading; they are not callback latency.",
        "cases": results,
    }
    write_json(args.output_dir / "report.json", report)
    write_html(args.output_dir / "report.html", report)
    with (args.output_dir / "cases.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(("id", "category", "passed", "exact_match", "metadata_failures", "batch_stream_match",
                         "character_edit_distance", "reference_characters", "character_error_rate"))
        for case in results:
            writer.writerow((case["id"], case["category"], case["passed"], case["matches_expected"],
                             len(case["metadata_failures"]), case["batch_stream_match"],
                             case["difference"]["character_edit_distance"], case["difference"]["reference_characters"],
                             case["difference"]["character_error_rate"]))
    return summary


def chunk_size(value: str) -> int:
    try:
        number = int(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("chunk size must be an integer") from exc
    if not 1 <= number <= 65536:
        raise argparse.ArgumentTypeError("chunk size must be between 1 and 65536 bytes")
    return number


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ttf", type=Path, default=Path("build/ttf"))
    parser.add_argument("--resources", type=Path, default=Path("resources"))
    parser.add_argument("--corpus", type=Path, default=Path("tests/evaluation/cases.jsonl"))
    parser.add_argument("--output-dir", type=Path, required=True, help="new directory; existing paths are refused")
    parser.add_argument("--chunk-bytes", type=chunk_size, nargs="+", default=[1, 7, 64],
                        help="streaming chunk sizes to compare against batch (default: 1 7 64)")
    args = parser.parse_args()
    args.chunk_bytes = list(dict.fromkeys(args.chunk_bytes))
    try:
        summary = evaluate(args)
    except (EvaluationError, OSError, UnicodeError, subprocess.SubprocessError) as exc:
        print(f"evaluation: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(summary, ensure_ascii=False))
    return 1 if summary["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
