#!/usr/bin/env python3
"""Compare text evaluation reports with identical references and test settings."""
from __future__ import annotations

import argparse
import html
import json
from pathlib import Path
import sys

from evaluate import EvaluationError, sha256, summarize, write_json


REFERENCE_FIELDS = ("text", "expected", "category", "default_language", "resource_overrides",
                    "expected_spans")


def read_report(path: Path) -> dict:
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise EvaluationError(f"{path}: invalid JSON") from exc
    if not isinstance(report, dict) or report.get("version") != 2:
        raise EvaluationError(f"{path}: expected text report version 2")
    cases = report.get("cases")
    if not isinstance(cases, list) or not cases:
        raise EvaluationError(f"{path}: missing cases")
    ids = set()
    for case in cases:
        if not isinstance(case, dict) or not isinstance(case.get("id"), str) or case["id"] in ids:
            raise EvaluationError(f"{path}: invalid or duplicate case id")
        ids.add(case["id"])
        for field in REFERENCE_FIELDS + ("normalized", "passed", "matches_expected",
                                         "batch_stream_match", "metadata_failures", "difference", "metadata"):
            if field not in case:
                raise EvaluationError(f"{path}: case {case['id']} missing {field}")
        if not all(isinstance(case.get(field), str) for field in
                   ("text", "expected", "normalized", "category", "default_language")):
            raise EvaluationError(f"{path}: invalid case text")
        if not all(type(case[field]) is bool for field in
                   ("passed", "matches_expected", "batch_stream_match")):
            raise EvaluationError(f"{path}: invalid case status")
        if (case["matches_expected"] != (case["normalized"] == case["expected"]) or
                case["passed"] != (case["matches_expected"] and case["batch_stream_match"]
                                   and not case["metadata_failures"])):
            raise EvaluationError(f"{path}: inconsistent case status")
    try:
        if summarize(cases) != report.get("summary"):
            raise EvaluationError(f"{path}: summary does not match case results")
        if not isinstance(report["provenance"]["chunk_bytes"], list):
            raise EvaluationError(f"{path}: missing chunk settings")
    except (KeyError, TypeError) as exc:
        raise EvaluationError(f"{path}: invalid report fields") from exc
    return report


def compare(before: dict, after: dict) -> dict:
    old = {case["id"]: case for case in before["cases"]}
    new = {case["id"]: case for case in after["cases"]}
    if set(old) != set(new):
        raise EvaluationError("case sets differ; evaluate both revisions against the same corpus")
    if before["provenance"]["chunk_bytes"] != after["provenance"]["chunk_bytes"]:
        raise EvaluationError("stream chunk settings differ")
    rows = []
    for key in sorted(old):
        first, second = old[key], new[key]
        if any(first[field] != second[field] for field in REFERENCE_FIELDS):
            raise EvaluationError(f"{key}: references or configuration changed; comparison refused")
        status = ("unchanged_pass" if first["passed"] and second["passed"] else
                  "regressed" if first["passed"] else
                  "fixed" if second["passed"] else "remaining_failure")
        rows.append({"id": key, "category": second["category"], "status": status,
                     "text": second["text"], "expected": second["expected"],
                     "before": first["normalized"], "after": second["normalized"],
                     "before_errors": first["difference"]["character_edit_distance"],
                     "after_errors": second["difference"]["character_edit_distance"],
                     "before_metadata_failures": first["metadata_failures"],
                     "after_metadata_failures": second["metadata_failures"]})
    return {"version": 1,
            "summary": {**{status: sum(row["status"] == status for row in rows)
                           for status in ("fixed", "regressed", "remaining_failure", "unchanged_pass")},
                        "text_error_increased": sum(row["after_errors"] > row["before_errors"]
                                                    for row in rows)},
            "before": before["summary"], "after": after["summary"],
            "categories": {category: {
                "before": summarize([c for c in before["cases"] if c["category"] == category]),
                "after": summarize([c for c in after["cases"] if c["category"] == category])}
                for category in sorted({case["category"] for case in after["cases"]})},
            "cases": rows}


def write_html(path: Path, report: dict) -> None:
    esc = html.escape
    parts = ['<!doctype html><html lang="en"><meta charset="utf-8">',
             '<meta name="viewport" content="width=device-width,initial-scale=1">',
             '<title>Frontend text regression comparison</title>',
             '<style>body{font:16px/1.5 sans-serif;max-width:1100px;margin:2rem auto;padding:1rem}',
             'pre{white-space:pre-wrap;overflow-wrap:anywhere}article{border-top:1px solid #888}</style>',
             '<h1>Frontend text regression comparison</h1>',
             '<p>Both revisions use identical references and test settings. Remaining failures are ',
             'reported explicitly; this comparison does not replace the strict acceptance gate.</p>',
             '<pre>' + esc(json.dumps(report["summary"], indent=2)) + '</pre>']
    order = {"regressed": 0, "remaining_failure": 1, "fixed": 2, "unchanged_pass": 3}
    for case in sorted(report["cases"], key=lambda c: (order[c["status"]], c["id"])):
        parts.append('<article><h2>' + esc(case["id"]) + ' — ' + esc(case["status"]) + '</h2>')
        for label in ("text", "expected", "before", "after"):
            parts.append('<h3>' + label + '</h3><pre>' + esc(case[label]) + '</pre>')
        parts.append('</article>')
    parts.append('</html>')
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = compare(read_report(args.before), read_report(args.after))
        result["provenance"] = {"before_sha256": sha256(args.before), "after_sha256": sha256(args.after)}
        args.output_dir.mkdir(parents=True, exist_ok=False)
        write_json(args.output_dir / "comparison.json", result)
        write_html(args.output_dir / "comparison.html", result)
    except (EvaluationError, OSError, UnicodeError) as exc:
        print(f"comparison: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result["summary"]))
    return 1 if result["summary"]["regressed"] or result["summary"]["text_error_increased"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
