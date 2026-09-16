"""Exercise independent references, metadata, parity, metrics, and report failures."""
from argparse import Namespace
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile


cli, resource_root = sys.argv[1:]
root = Path(__file__).resolve().parents[2]
evaluator = root / "tools" / "evaluation" / "evaluate.py"
spec = importlib.util.spec_from_file_location("ttf_evaluate", evaluator)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def rejected(callback):
    try:
        callback()
    except module.EvaluationError:
        return
    raise AssertionError("invalid evaluation input was accepted")


def run(output, corpus):
    return subprocess.run(
        [sys.executable, str(evaluator), "--ttf", cli, "--resources", resource_root,
         "--corpus", str(corpus), "--output-dir", str(output)],
        capture_output=True, text=True, encoding="utf-8", timeout=60,
    )


def write_case(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False) + "\n", encoding="utf-8")


# Unicode codepoints, not UTF-8 bytes; exact comparison never folds case/spacing.
for expected, actual, distance in (("", "", 0), ("", "가", 1), ("가", "", 1),
                                   ("애플", "에플", 1), ("Apple", "apple", 1),
                                   ("a b", "ab", 1), ("kitten", "sitting", 3),
                                   ("\r\n", "\n", 1), ("é", "e\u0301", 2)):
    result = module.text_difference(expected, actual)
    assert result["character_edit_distance"] == distance, result
    assert result["character_error_rate"] == distance / max(1, len(expected)), result
    assert bool(result["edits"]) == bool(distance)

with tempfile.TemporaryDirectory(prefix="ttf-evaluation-") as temporary:
    directory = Path(temporary)
    output = directory / "corpus-run"
    corpus = root / "tests" / "evaluation" / "cases.jsonl"
    process = run(output, corpus)
    assert process.returncode == 0, (process.stdout, process.stderr)
    summary = json.loads(process.stdout)
    assert summary["cases"] >= 100 and summary["failed"] == 0, summary
    assert summary["mismatches"] == 0 and summary["character_error_rate"] == 0, summary
    assert summary["fallback_spans"] >= 1, summary
    report = json.loads((output / "report.json").read_text(encoding="utf-8"))
    assert report["version"] == 2
    assert report["provenance"]["chunk_bytes"] == [1, 7, 64]
    assert report["provenance"]["resource_sha256"]
    assert len(report["categories"]) >= 12
    assert sum(group["cases"] for group in report["categories"].values()) == summary["cases"]
    for case in report["cases"]:
        assert case["passed"] and case["batch_stream_match"]
        assert len(case["stream_variants"]) == 3
        assert case["batch_normalized"] == case["expected"]
    apple = next(case for case in report["cases"] if case["id"] == "apple-ko")
    assert apple["metadata"]["lexicon_hit_spans"] == 1
    assert {"source": "en-US", "context": "ko-KR", "render": "ko-KR", "spans": 1} in apple["metadata"]["language_routes"]
    assert (output / "cases.csv").is_file()
    before = (output / "report.json").read_bytes()
    assert run(output, corpus).returncode == 2
    assert (output / "report.json").read_bytes() == before, "existing report was changed"

    # Text mismatches must produce reviewable evidence and a failing exit code.
    corpus = directory / "mismatch.jsonl"
    write_case(corpus, {"id": "mismatch", "text": "Apple은 사과입니다.", "expected": "애플은 사가입니다.", "category": "regression"})
    process = run(directory / "mismatch-run", corpus)
    assert process.returncode == 1, process.stderr
    summary = json.loads(process.stdout)
    assert summary["mismatches"] == 1 and summary["failed"] == 1
    assert summary["character_edit_distance"] == 1
    failure = json.loads((directory / "mismatch-run" / "report.json").read_text(encoding="utf-8"))
    assert failure["cases"][0]["difference"]["edits"] == [
        {"operation": "replace", "expected_begin": 5, "expected_end": 6,
         "actual_begin": 5, "actual_end": 6, "expected": "가", "actual": "과"}]

    # Identical rendered text can still have wrong language metadata.
    write_case(corpus, {"id": "metadata", "text": "Apple은 사과입니다.", "expected": "애플은 사과입니다.",
                        "expected_spans": [{"surface": "Apple", "context_lang": "en-US"}]})
    process = run(directory / "metadata-run", corpus)
    assert process.returncode == 1, process.stderr
    summary = json.loads(process.stdout)
    assert summary["mismatches"] == 0 and summary["metadata_failures"] == 1 and summary["failed"] == 1

    # Batch output alone cannot satisfy the gate if a stream variant differs.
    payload = (output / "apple-ko" / "segments.jsonl").read_text(encoding="utf-8")
    write_case(corpus, {"id": "parity", "text": "Apple은 사과입니다.", "expected": "애플은 사과입니다."})
    actual_frontend = module.frontend
    try:
        module.frontend = lambda command, _: ("incorrect batch\n" if command[1] == "normalize" else payload, 0.0)
        summary = module.evaluate(Namespace(corpus=corpus, ttf=Path(cli), resources=Path(resource_root),
                                            output_dir=directory / "parity-run", chunk_bytes=[1]))
    finally:
        module.frontend = actual_frontend
    assert summary["mismatches"] == 0 and summary["batch_stream_failures"] == 1 and summary["failed"] == 1

    for bad in ("{", "[]", '{"id":"../escape","text":"a","expected":"a"}',
                '{"id":"a","text":1,"expected":"a"}',
                '{"id":"a","text":"a","expected":"a","default_language":null}',
                '{"id":"a","text":"a","expected":"a"}\n{"id":"a","text":"b","expected":"b"}'):
        corpus.write_text(bad, encoding="utf-8")
        rejected(lambda: module.read_corpus(corpus))
    base = {"id": "invalid", "text": "Apple", "expected": "Apple"}
    for invalid in ({"category": "=formula"}, {"expected_spans": {}},
                    {"expected_spans": [{"surface": "missing", "context_lang": "en-US"}]},
                    {"expected_spans": [{"surface": "Apple", "occurrence": True, "context_lang": "en-US"}]},
                    {"expected_spans": [{"surface": "Apple", "occurrence": -1, "context_lang": "en-US"}]},
                    {"expected_spans": [{"surface": "Apple", "unknown_field": "x"}]},
                    {"expected_spans": [{"surface": "Apple", "context_lang": None}]},
                    {"resource_overrides": {}},
                    {"resource_overrides": [{"layer": "invalid", "path": "test.tsv"}]},
                    {"resource_overrides": [{"layer": "domain", "path": "../outside.tsv"}]}):
        write_case(corpus, dict(base, **invalid))
        rejected(lambda: module.read_corpus(corpus))

    rejected(lambda: module.parse_stream("", ""))
    rejected(lambda: module.parse_stream('{"version":1,"event":"end","segment_count":0,"input_bytes":0}', "x"))
    rejected(lambda: module.parse_stream('{"version":2,"event":"end","segment_count":0,"input_bytes":0}', ""))
    rejected(lambda: module.parse_stream('{"version":1,"event":"end","segment_count":0,"input_bytes":0}\n{}', ""))
    lines = payload.splitlines()
    original_segment = json.loads(lines[0])
    for field, bad_value in (("end", 1),  # inside Hangul UTF-8
                             ("input_end", 4),  # source coverage gap
                             ("input_begin", 1), ("begin", True), ("context_lang", None)):
        segment = copy.deepcopy(original_segment)
        segment["spans"][0][field] = bad_value
        rejected(lambda: module.parse_stream(json.dumps(segment) + "\n" + "\n".join(lines[1:]), apple["text"]))
    segment = copy.deepcopy(original_segment)
    del segment["spans"][-1]
    rejected(lambda: module.parse_stream(json.dumps(segment) + "\n" + "\n".join(lines[1:]), apple["text"]))

    # Report source strings and diagnostics must never become executable markup.
    hostile = '<script>alert("input")</script><img src=x onerror=alert(1)>'
    escaped_report = copy.deepcopy(report)
    escaped_report["cases"] = [dict(apple, text=hostile, normalized=hostile, expected=hostile,
                                    notes=hostile, metadata={"path": hostile})]
    module.write_html(directory / "escaped.html", escaped_report)
    page = (directory / "escaped.html").read_text(encoding="utf-8")
    assert "<script>" not in page and "<img " not in page
    assert "&lt;script&gt;" in page and "&quot;input&quot;" in page

print("text corpus, category/CER metrics, routing metadata, batch/stream parity, and report failure handling passed")
