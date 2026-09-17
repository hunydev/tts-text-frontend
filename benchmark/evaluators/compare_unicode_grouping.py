#!/usr/bin/env python3
"""Compare original Unicode-number references on two native engine snapshots.

Only the fixed, authored references() in test_unicode_grouping.py are scored.
Generated Decimal cases, malformed-input guards and streaming repeats are not
included. Exact text, whole-object class and exact string-valued IR are separate
metrics; this selected development set does not estimate production accuracy.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "benchmark"))
from common import digest_json, sha256, tree_hash

REFERENCE_PATH = ROOT / "tests/integration/test_unicode_grouping.py"
NATIVE_PATH = ROOT / "tools/web/native.py"
METRICS = ("text_exact", "semantic_class_exact", "ir_value_exact", "ir_exact")


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def reference_cases():
    references = module(REFERENCE_PATH, "unicode_grouping_references")
    cases = []
    seen = set()
    for separator in references.SEPARATORS:
        for row in references.references(separator):
            if len(row) != 7 or not all(isinstance(field, str) for field in row):
                raise ValueError("expected seven string fields in an authored reference")
            language, text, expected, semantic, value, unit, secondary = row
            if not language or not text or not expected or not semantic or not value:
                raise ValueError("incomplete authored reference")
            if separator not in text or (language, text) in seen:
                raise ValueError("separator missing or duplicate authored reference")
            seen.add((language, text))
            cases.append(dict(language=language, text=text, expected=expected,
                              semantic=semantic, value=value, unit=unit, secondary=secondary,
                              separator=f"U+{ord(separator):04X}"))
    if not cases:
        raise ValueError("no authored references")
    return cases


def verdict(case, actual):
    spans = actual["spans"]
    aligned = (len(spans) == 1 and spans[0]["input_begin"] == 0 and
               spans[0]["input_end"] == len(case["text"].encode("utf-8")))
    semantic = aligned and spans[0]["semantic_type"] == case["semantic"]
    value = aligned and all(spans[0][key] == case[key] for key in ("value", "unit", "secondary"))
    return {"text_exact": actual["text"] == case["expected"],
            "semantic_class_exact": bool(semantic), "ir_value_exact": bool(value),
            "ir_exact": bool(semantic and value), "whole_object_aligned": bool(aligned),
            "engine_error": False}


def snapshot(library, resources):
    return {"library_sha256": sha256(library), "resources_sha256": tree_hash(resources)}


def count(rows):
    result = {"total": len(rows), "engine_errors": sum(row["engine_error"] for row in rows),
              "whole_object_aligned": sum(row["whole_object_aligned"] for row in rows)}
    result.update({metric: sum(row[metric] for row in rows) for metric in METRICS})
    return result


def evaluate(native, cases, library, resources):
    rows = []
    outputs = Counter()
    engines = {}
    try:
        for case in cases:
            language = case["language"]
            if language not in engines:
                engine = native.Engine(library, resources, {"default_language": language})
                engines[language] = engine
            try:
                actual = engines[language].normalize(case["text"].encode("utf-8"))
                rows.append(verdict(case, actual))
                outputs["+".join(span["semantic_type"] for span in actual["spans"])] += 1
            except ValueError:
                rows.append(dict.fromkeys((*METRICS, "whole_object_aligned"), False) |
                            {"engine_error": True})
                outputs["engine-error"] += 1
    finally:
        for engine in engines.values():
            engine.close()
    if len(rows) != len(cases):
        raise ValueError("incomplete evaluation")
    groups = {}
    for field in ("semantic", "language", "separator"):
        buckets = defaultdict(list)
        for case, row in zip(cases, rows):
            buckets[case[field]].append(row)
        groups[field] = {key: count(value) for key, value in sorted(buckets.items())}
    return {"metrics": count(rows),
            "by_reference_class": groups["semantic"], "by_language": groups["language"],
            "by_separator": groups["separator"],
            "actual_span_class_sequences": dict(sorted(outputs.items()))}, rows


def compare(before_rows, after_rows):
    result = {}
    for metric in METRICS:
        fixed = sum(not old[metric] and new[metric] for old, new in zip(before_rows, after_rows))
        regressed = sum(old[metric] and not new[metric] for old, new in zip(before_rows, after_rows))
        unchanged_correct = sum(old[metric] and new[metric] for old, new in zip(before_rows, after_rows))
        unchanged_incorrect = sum(not old[metric] and not new[metric]
                                  for old, new in zip(before_rows, after_rows))
        assert fixed + regressed + unchanged_correct + unchanged_incorrect == len(before_rows)
        result[metric] = dict(fixed=fixed, regressed=regressed,
                              unchanged_correct=unchanged_correct,
                              unchanged_incorrect=unchanged_incorrect)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-library", type=Path, required=True)
    parser.add_argument("--baseline-resources", type=Path, required=True)
    parser.add_argument("--candidate-library", type=Path, required=True)
    parser.add_argument("--candidate-resources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    for name in ("baseline_library", "baseline_resources", "candidate_library", "candidate_resources"):
        setattr(args, name, getattr(args, name).resolve())
    provenance = {"reference_module_sha256": sha256(REFERENCE_PATH),
                  "evaluator_sha256": sha256(Path(__file__)),
                  "native_adapter_sha256": sha256(NATIVE_PATH),
                  "baseline": snapshot(args.baseline_library, args.baseline_resources),
                  "candidate": snapshot(args.candidate_library, args.candidate_resources)}
    cases = reference_cases()
    native = module(NATIVE_PATH, "unicode_grouping_native")
    before, before_rows = evaluate(native, cases, args.baseline_library, args.baseline_resources)
    after, after_rows = evaluate(native, cases, args.candidate_library, args.candidate_resources)
    current = {"reference_module_sha256": sha256(REFERENCE_PATH),
               "evaluator_sha256": sha256(Path(__file__)),
               "native_adapter_sha256": sha256(NATIVE_PATH),
               "baseline": snapshot(args.baseline_library, args.baseline_resources),
               "candidate": snapshot(args.candidate_library, args.candidate_resources)}
    if current != provenance:
        raise ValueError("evaluation inputs changed during measurement")
    output = {"schema_version": 1, "status": "completed", "cases": len(cases),
              "reference_source": "tests/integration/test_unicode_grouping.py:references",
              "reference_status": "authored-development-reference; pending independent review",
              "sample_policy": "Every fixed authored reference for NBSP, NNBSP and thin space, once; "
                               "default language from each reference, default segment configuration. "
                               "No Decimal-generated cases, invalid-input guards or chunk repetitions.",
              "metric_policy": {
                  "text_exact": "Literal normalized text equals authored complete reference.",
                  "semantic_class_exact": "One span covers all original UTF-8 bytes and has the reference class.",
                  "ir_value_exact": "One whole-input span has exact value, secondary and unit strings, regardless of class.",
                  "ir_exact": "Both whole-object class and value tuple match; no float or case normalization.",
                  "failure_policy": "Errors and unaligned/multiple spans remain failures in the denominator.",
              }, "reference_cases_sha256": digest_json(cases), "provenance": provenance,
              "before": before, "after": after, "comparison": compare(before_rows, after_rows),
              "limitations": "Selected feature development references, authored in this project; "
                             "not held-out, not independently approved and not a production accuracy estimate. "
                             "This aggregate contains no third-party corpus or input/output strings."}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(args.output)
    print(json.dumps({"cases": len(cases), "before": before["metrics"],
                      "after": after["metrics"], "comparison": output["comparison"]}, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(str(error)) from error
