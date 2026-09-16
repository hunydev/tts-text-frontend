#!/usr/bin/env python3
"""Compare two evaluations of exactly the same benchmark records and policy."""

import argparse
import json
import sys
from collections import Counter, defaultdict
from itertools import zip_longest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from common import ensure_output


def compare(before, after):
    left = json.loads((before / "summary.json").read_text())
    right = json.loads((after / "summary.json").read_text())
    if left.get("status") != "completed" or right.get("status") != "completed":
        raise ValueError("comparison needs two complete evaluations")
    if left["metric_policy"] != right["metric_policy"]:
        raise ValueError("metric policy changed")
    for field in (
        "corpus_sha256",
        "evaluated_records_sha256",
        "evaluator_sha256",
        "schema_sha256",
        "adapter_sha256",
        "input_stability",
        "backend",
        "chunk_bytes",
    ):
        if left["provenance"][field] != right["provenance"][field]:
            raise ValueError(f"cannot compare different {field}")
    changes = defaultdict(Counter)
    samples = defaultdict(list)
    with (
        (before / "results.jsonl").open() as one,
        (after / "results.jsonl").open() as two,
    ):
        for old, new in zip_longest(one, two):
            if old is None or new is None:
                raise ValueError("result counts differ")
            old, new = json.loads(old), json.loads(new)
            if (
                old["id"] != new["id"]
                or old["reference_digest"] != new["reference_digest"]
            ):
                raise ValueError("case order or references changed")
            for a, b in zip_longest(old["spans"], new["spans"]):
                if a is None or b is None:
                    raise ValueError("reference span count changed")
                key = a["semiotic_class"] + "/" + a["reference_status"]
                if a["exact"] is None:
                    changes[key]["unlabeled"] += 1
                    continue
                outcome = (
                    "fixed"
                    if not a["exact"] and b["exact"]
                    else "regressed"
                    if a["exact"] and not b["exact"]
                    else "unchanged-correct"
                    if b["exact"]
                    else "unchanged-wrong"
                )
                changes[key][outcome] += 1
                if outcome in {"fixed", "regressed"} and len(samples[outcome]) < 30:
                    samples[outcome].append(
                        {
                            "id": old["id"],
                            "semiotic_class": a["semiotic_class"],
                            "reference_status": a["reference_status"],
                        }
                    )
    return {
        "schema_version": 1,
        "before": left["summary"],
        "after": right["summary"],
        "changes": {k: dict(v) for k, v in sorted(changes.items())},
        "examples": dict(samples),
        "provenance": {"before": left["provenance"], "after": right["provenance"]},
        "note": "Same references, cases, metric policy and backend required. Known development improvements are not held-out accuracy. Source-use restrictions apply to both reports.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        args.output = ensure_output(args.output)
        report = compare(args.before, args.after)
        with args.output.open("x", encoding="utf-8") as handle:
            json.dump(report, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
        print(json.dumps(report["changes"], ensure_ascii=False))
    except (ValueError, KeyError, OSError) as error:
        parser.exit(2, str(error) + "\n")


if __name__ == "__main__":
    main()
