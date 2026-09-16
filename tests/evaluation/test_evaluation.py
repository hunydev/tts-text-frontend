"""Exercise actual bytewise frontend output plus evaluator input/report failures."""
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


with tempfile.TemporaryDirectory(prefix="ttf-evaluation-") as temporary:
    directory = Path(temporary)
    output = directory / "corpus-run"
    process = run(output, root / "tests" / "evaluation" / "cases.jsonl")
    assert process.returncode == 0, process.stderr
    summary = json.loads(process.stdout)
    assert summary["cases"] >= 12 and summary["mismatches"] == 0, summary
    assert summary["fallback_spans"] >= 1 and summary["human_review"] == "pending", summary
    report = json.loads((output / "report.json").read_text(encoding="utf-8"))
    assert report["provenance"]["chunk_bytes"] == 1
    assert report["provenance"]["resource_sha256"]
    for case in report["cases"]:
        assert case["audio"] is None
        assert case["human_review"]["pronunciation_correctness"] is None
    apple = next(case for case in report["cases"] if case["id"] == "apple-ko")
    assert apple["metadata"]["lexicon_hit_spans"] == 1
    assert {"source": "en-US", "context": "ko-KR", "render": "ko-KR", "spans": 1} in apple["metadata"]["language_routes"]
    assert (output / "review.csv").is_file()
    before = (output / "report.json").read_bytes()
    assert run(output, root / "tests" / "evaluation" / "cases.jsonl").returncode == 2
    assert (output / "report.json").read_bytes() == before, "existing report was changed"

    # A mismatch still produces reviewable evidence, then exits nonzero.
    corpus = directory / "mismatch.jsonl"
    corpus.write_text(json.dumps({"id": "mismatch", "text": "Apple은 사과입니다.", "expected": "wrong"}), encoding="utf-8")
    process = run(directory / "mismatch-run", corpus)
    assert process.returncode == 1, process.stderr
    assert json.loads(process.stdout)["mismatches"] == 1

    for bad in ("{", "[]", '{"id":"../escape","text":"a","expected":"a"}',
                '{"id":"a","text":1,"expected":"a"}',
                '{"id":"a","text":"a","expected":"a","default_language":null}',
                '{"id":"a","text":"a","expected":"a"}\n{"id":"a","text":"b","expected":"b"}'):
        corpus.write_text(bad, encoding="utf-8")
        rejected(lambda: module.read_corpus(corpus))
    rejected(lambda: module.parse_stream("", ""))
    rejected(lambda: module.parse_stream('{"version":1,"event":"end","segment_count":0,"input_bytes":0}', "x"))
    rejected(lambda: module.parse_stream('{"version":2,"event":"end","segment_count":0,"input_bytes":0}', ""))
    rejected(lambda: module.parse_stream('{"version":1,"event":"end","segment_count":0,"input_bytes":0}\n{}', ""))
    payload = (output / "apple-ko" / "segments.jsonl").read_text(encoding="utf-8")
    lines = payload.splitlines()
    segment = json.loads(lines[0])
    segment["spans"][0]["end"] = 1  # inside Hangul UTF-8
    rejected(lambda: module.parse_stream(json.dumps(segment) + "\n" + "\n".join(lines[1:]), apple["text"]))

    audio = directory / "bad-audio"
    audio.mkdir()
    rejected(lambda: module.audio_artifact(audio))
    for manifest in ([], {"status": "failed"}, {"status": "complete", "jobs": "invalid"},
                     {"status": "complete", "jobs": [{}], "combined_wav": "../outside.wav"},
                     {"status": "complete", "jobs": [{}], "combined_wav": "missing.wav"}):
        (audio / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        rejected(lambda: module.audio_artifact(audio))

    hostile = '<script>alert("input")</script><img src=x onerror=alert(1)>'
    escaped_report = {"summary": {}, "cases": [{"id": "safe", "text": hostile,
        "normalized": hostile, "expected": hostile, "notes": hostile,
        "matches_expected": True, "metadata": {"path": hostile}, "audio": None}]}
    module.write_html(directory / "escaped.html", escaped_report)
    page = (directory / "escaped.html").read_text(encoding="utf-8")
    assert "<script>" not in page and "<img " not in page
    assert "&lt;script&gt;" in page and "&quot;input&quot;" in page

print("evaluation corpus, metadata, failure handling, safe HTML, and artifact preservation passed")
