"""Comparison gates must distinguish regressions from changed references."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tools" / "evaluation"))
import compare
from evaluate import EvaluationError, summarize, text_difference


def case(key, expected, actual, metadata=None):
    metadata = metadata or []
    return {"id": key, "text": "input " + key, "expected": expected, "normalized": actual,
            "category": "finance", "default_language": "ko-KR", "resource_overrides": [],
            "expected_spans": [], "matches_expected": expected == actual,
            "passed": expected == actual and not metadata, "batch_stream_match": True,
            "metadata_failures": metadata, "difference": text_difference(expected, actual),
            "metadata": {"fallback_spans": 0}}


def report(cases):
    return {"version": 2, "cases": cases, "summary": summarize(cases),
            "provenance": {"chunk_bytes": [1, 7, 64]}}


def rejected(callback):
    try:
        callback()
    except EvaluationError:
        return
    raise AssertionError("incomparable evidence accepted")


before = report([case("fix", "애플", "Apple"), case("regress", "원", "원"),
                 case("keep", "세 개", "세 개"), case("remaining", "만 원", "만원")])
after = report([case("fix", "애플", "애플"), case("regress", "원", "원", [{"field": "value"}]),
                case("keep", "세 개", "세 개"), case("remaining", "만 원", "만원")])
result = compare.compare(before, after)
assert result["summary"] == {"fixed": 1, "regressed": 1, "remaining_failure": 1, "unchanged_pass": 1,
                             "text_error_increased": 0}
failed_before = report([case("still_wrong", "가", "나")])
failed_after = report([case("still_wrong", "가", "나라")])
assert compare.compare(failed_before, failed_after)["summary"]["text_error_increased"] == 1
assert result["categories"]["finance"]["before"]["passed"] == 2
for field, value in (("text", "changed input"), ("expected", "changed answer"),
                     ("expected_spans", [{"surface": "input", "value": "1"}]),
                     ("default_language", "en-US"), ("resource_overrides", [{"layer": "domain"}])):
    altered = copy.deepcopy(after)
    altered["cases"][0][field] = value
    rejected(lambda: compare.compare(before, altered))
altered = copy.deepcopy(after)
altered["cases"].pop()
rejected(lambda: compare.compare(before, altered))
altered = copy.deepcopy(after)
altered["provenance"]["chunk_bytes"] = [1]
rejected(lambda: compare.compare(before, altered))

with tempfile.TemporaryDirectory(prefix="ttf-comparison-") as tmp:
    directory = Path(tmp)
    paths = [directory / "before.json", directory / "after.json"]
    for path, value in zip(paths, (before, after)):
        path.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")
        assert compare.read_report(path) == value
    command = [sys.executable, str(root / "tools/evaluation/compare.py"), "--before", str(paths[0]),
               "--after", str(paths[1]), "--output-dir", str(directory / "result")]
    process = subprocess.run(command, capture_output=True, text=True, timeout=10)
    assert process.returncode == 1, process.stderr
    assert json.loads(process.stdout)["regressed"] == 1
    snapshot = (directory / "result/comparison.json").read_bytes()
    assert subprocess.run(command, capture_output=True, timeout=10).returncode == 2
    assert (directory / "result/comparison.json").read_bytes() == snapshot
    for altered in ([], {}, {"version": 1}, dict(before, summary={})):
        paths[0].write_text(json.dumps(altered), encoding="utf-8")
        rejected(lambda: compare.read_report(paths[0]))
    altered = copy.deepcopy(before)
    altered["cases"][0]["passed"] = True
    paths[0].write_text(json.dumps(altered), encoding="utf-8")
    rejected(lambda: compare.read_report(paths[0]))
    result["cases"][0]["text"] = '<script>alert("input")</script>'
    compare.write_html(directory / "escaped.html", result)
    page = (directory / "escaped.html").read_text(encoding="utf-8")
    assert "<script>" not in page and "&lt;script&gt;" in page

print("comparison fixes, metadata regressions, reference integrity and evidence preservation passed")
