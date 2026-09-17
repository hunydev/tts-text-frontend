#!/usr/bin/env python3
"""Aggregate failure reasons without changing the benchmark's exact-match score.

Reads a completed evaluator report. It emits counts only, never corpus text.
Casefold agreement is a diagnostic, not an alternative accuracy metric.
"""

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import sha256


def diagnose(directory):
    summary = json.loads((directory / "summary.json").read_text(encoding="utf-8"))
    if summary.get("status") != "completed":
        raise ValueError("a completed evaluation is required")
    results = directory / "results.jsonl"
    before_hash = sha256(results)
    classes = defaultdict(Counter)
    semantics = defaultdict(Counter)
    cases = 0
    with results.open(encoding="utf-8") as source:
        for line in source:
            record = json.loads(line)
            cases += 1
            for span in record["spans"]:
                key = span["semiotic_class"] + "/" + span["reference_status"]
                actual, spoken = span["actual"], span["spoken"]
                if spoken is None:
                    reason = "unlabeled"
                elif record.get("engine_error"):
                    reason = "engine-error"
                elif span.get("alignment_error") or actual is None:
                    reason = "unaligned"
                elif actual == spoken:
                    reason = "exact"
                elif actual.casefold() == spoken.casefold():
                    reason = "casefold-only-difference"
                else:
                    reason = "other-text-difference"
                classes[key][reason] += 1
                semantics[key]["+".join(span.get("engine_semantics", [])) or "none"] += 1
    if cases != summary["summary"]["cases"] or before_hash != sha256(results):
        raise ValueError("incomplete or changing result file")
    return {
        "schema_version": 1,
        "cases": cases,
        "results_sha256": before_hash,
        "metric_policy": summary["metric_policy"],
        "reasons": {key: dict(value) for key, value in sorted(classes.items())},
        "engine_semantics": {key: dict(value) for key, value in sorted(semantics.items())},
        "note": "Diagnostic counts only. Casefold differences remain exact-match failures. "
        "A matching semantic category does not prove a correct value or reading. "
        "Reference statuses are kept separate; no case text is exported.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(diagnose(args.report), ensure_ascii=False, indent=2))
    except (ValueError, KeyError, TypeError, OSError) as error:
        parser.exit(2, str(error) + "\n")


if __name__ == "__main__":
    main()
