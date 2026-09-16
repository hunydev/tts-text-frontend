#!/usr/bin/env python3
"""Generate a reproducible text check and human listening worksheet (stdlib only)."""
from __future__ import annotations

import argparse
from collections import Counter
import csv
import hashlib
import html
import json
from pathlib import Path
import re
import subprocess
import sys
import time
import wave


class EvaluationError(ValueError):
    """The corpus, frontend output, or audio adapter could not be evaluated."""


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
        for key in ("default_language", "notes"):
            if key in case and not isinstance(case[key], str):
                raise EvaluationError(f"corpus line {number}: {key} must be a string")
        case.setdefault("default_language", "ko-KR")
        case.setdefault("notes", "")
        if not re.fullmatch(r"[A-Za-z]{2,8}(?:-[A-Za-z0-9]{1,8})*", case["default_language"]):
            raise EvaluationError(f"corpus line {number}: invalid default_language")
        try:
            case["text"].encode("utf-8")
            case["expected"].encode("utf-8")
        except UnicodeError as exc:
            raise EvaluationError(f"corpus line {number}: invalid Unicode") from exc
        identifiers.add(case["id"])
        cases.append(case)
    if not cases:
        raise EvaluationError("corpus has no cases")
    return cases


def integer(value: object, field: str) -> int:
    if type(value) is not int or value < 0:
        raise EvaluationError(f"stream {field}: expected a nonnegative integer")
    return value


def parse_stream(payload: str, original: str) -> list[dict]:
    """Reject truncated streams and offsets that would corrupt a listening report."""
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
        if begin != input_at or not begin <= end <= len(source):
            raise EvaluationError("stream input offsets are not contiguous")
        if not isinstance(event.get("text"), str) or not isinstance(event.get("spans"), list):
            raise EvaluationError("stream segment text/spans have invalid types")
        normalized = event["text"].encode("utf-8")
        output_at = 0
        for span in event["spans"]:
            if not isinstance(span, dict):
                raise EvaluationError("stream span must be an object")
            span_begin = integer(span.get("begin"), "span.begin")
            span_end = integer(span.get("end"), "span.end")
            source_begin = integer(span.get("input_begin"), "span.input_begin")
            source_end = integer(span.get("input_end"), "span.input_end")
            if span_begin != output_at or not span_begin <= span_end <= len(normalized):
                raise EvaluationError("stream output span offsets are not contiguous")
            if not begin <= source_begin <= source_end <= end:
                raise EvaluationError("stream input span is outside its segment")
            for key in ("source_lang", "context_lang", "render_lang", "lexicon"):
                if not isinstance(span.get(key), str):
                    raise EvaluationError(f"stream span.{key} must be a string")
            try:
                normalized[span_begin:span_end].decode("utf-8")
                source[source_begin:source_end].decode("utf-8")
            except UnicodeError as exc:
                raise EvaluationError("stream span splits a UTF-8 character") from exc
            output_at = span_end
        if output_at != len(normalized):
            raise EvaluationError("stream spans do not cover normalized text")
        segments.append(event)
        input_at = end
    if not ended:
        raise EvaluationError("stream is missing its end event")
    return segments


def describe_spans(segments: list[dict]) -> dict:
    paths = Counter()
    languages = Counter()
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
    return {
        "fallback_spans": fallback,
        "lexicon_hit_spans": lexicon_hits,
        "pronunciation_paths": dict(sorted(paths.items())),
        "language_routes": [
            {"source": source, "context": context, "render": render, "spans": count}
            for (source, context, render), count in sorted(languages.items())
        ],
    }


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def audio_artifact(directory: Path) -> Path | None:
    """Require a completed adapter manifest and a readable audio artifact."""
    try:
        manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise EvaluationError("invalid audio manifest") from exc
    if not isinstance(manifest, dict) or manifest.get("status") != "complete":
        raise EvaluationError("audio manifest is not complete")
    if not isinstance(manifest.get("jobs"), list):
        raise EvaluationError("audio manifest has invalid jobs")
    combined = manifest.get("combined_wav")
    if not manifest["jobs"] and combined is None:
        return None
    if not isinstance(combined, str) or not combined:
        raise EvaluationError("audio manifest has no combined WAV")
    relative = Path(combined)
    if relative.is_absolute() or ".." in relative.parts:
        raise EvaluationError("audio manifest WAV escapes its directory")
    audio = directory / relative
    if not audio.resolve().is_relative_to(directory.resolve()):
        raise EvaluationError("audio manifest WAV escapes its directory")
    try:
        with wave.open(str(audio), "rb") as stream:
            frames = stream.getnframes()
            if not frames or len(stream.readframes(frames)) != frames * stream.getnchannels() * stream.getsampwidth():
                raise EvaluationError("audio artifact is empty or truncated")
    except (OSError, EOFError, wave.Error) as exc:
        raise EvaluationError("invalid combined WAV artifact") from exc
    return audio


def write_html(path: Path, report: dict) -> None:
    esc = html.escape
    parts = [
        '<!doctype html><html lang="en"><meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width,initial-scale=1">',
        '<title>TTS frontend listening review</title>',
        '<style>body{max-width:1000px;margin:2rem auto;padding:0 1rem;font:16px/1.5 sans-serif}',
        'article{border:1px solid #aaa;border-radius:8px;padding:1rem;margin:1rem 0}',
        'pre{white-space:pre-wrap;overflow-wrap:anywhere}label{display:block}input{width:4rem}',
        'textarea{width:95%;min-height:5rem}audio{width:100%}</style>',
        '<h1>TTS frontend listening review</h1>',
        '<p>Text comparison checks deterministic normalization only. Audio presence does not ',
        'establish pronunciation quality. Human review is pending; no automatic voice score is assigned.</p>',
        '<p>Enter scores in <a href="review.csv">review.csv</a> and save that file. ',
        'The blank controls below are listening scratch notes and are not saved automatically.</p>',
        '<pre>' + esc(json.dumps(report["summary"], ensure_ascii=False, indent=2)) + '</pre>',
    ]
    for case in report["cases"]:
        parts.append('<article><h2>' + esc(case["id"]) + '</h2>')
        parts.append('<p>Exact text match: ' + str(case["matches_expected"]).lower() + '</p>')
        for label, key in (("Input", "text"), ("Normalized", "normalized"), ("Expected", "expected")):
            parts.append('<h3>' + label + '</h3><pre>' + esc(case[key]) + '</pre>')
        parts.append('<p>' + esc(case["notes"]) + '</p>')
        parts.append('<details><summary>Pronunciation paths and language routes</summary><pre>' +
                     esc(json.dumps(case["metadata"], ensure_ascii=False, indent=2)) + '</pre></details>')
        if case.get("audio"):
            parts.append('<audio controls preload="none" src="' + esc(case["audio"], quote=True) + '"></audio>')
        else:
            parts.append('<p>Audio was not synthesized.</p>')
        for label in ("Pronunciation correctness", "Intelligibility", "Naturalness"):
            parts.append('<label>' + label + ' (1–5, blank until reviewed): '
                         '<input type="number" min="1" max="5" aria-label="' + label + '"></label>')
        parts.append('<label>Reviewer notes<textarea aria-label="Reviewer notes"></textarea></label></article>')
    parts.append('</html>')
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")


def evaluate(args: argparse.Namespace) -> dict:
    cases = read_corpus(args.corpus)
    cli = args.ttf.resolve(strict=True)
    resources = args.resources.resolve(strict=True)
    bridge = Path(__file__).resolve().parents[1] / "tts_bridge" / "bridge.py"
    # An evaluation run never silently replaces evidence from an earlier run.
    args.output_dir.mkdir(parents=True, exist_ok=False)
    results = []
    for case in cases:
        directory = args.output_dir / case["id"]
        directory.mkdir()
        input_file = directory / "input.txt"
        input_file.write_text(case["text"], encoding="utf-8")
        command = [str(cli), "stream-json", "--resources", str(resources), "--language",
                   case["default_language"], "--chunk-bytes", "1", str(input_file.resolve())]
        started = time.perf_counter()
        process = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=60)
        elapsed = time.perf_counter() - started
        if process.returncode:
            raise EvaluationError(f"{case['id']}: frontend failed: {process.stderr.strip()}")
        segments = parse_stream(process.stdout, case["text"])
        normalized = "".join(segment["text"] for segment in segments)
        segments_file = directory / "segments.jsonl"
        segments_file.write_text(process.stdout, encoding="utf-8")
        result = dict(case, normalized=normalized, matches_expected=normalized == case["expected"],
                      segment_count=len(segments), frontend_process_seconds=round(elapsed, 6),
                      metadata=describe_spans(segments), audio=None,
                      human_review={"status": "pending", "pronunciation_correctness": None,
                                    "intelligibility": None, "naturalness": None, "notes": ""})
        if args.synthesize:
            synthesis = subprocess.run(
                [sys.executable, str(bridge), "--input", str(segments_file.resolve()),
                 "--output-dir", str((directory / "audio").resolve()), "--backend", "espeak"],
                capture_output=True, text=True, encoding="utf-8", timeout=180,
            )
            if synthesis.returncode:
                raise EvaluationError(f"{case['id']}: synthesis failed: {synthesis.stderr.strip()}")
            manifest_path = directory / "audio" / "manifest.json"
            audio = audio_artifact(directory / "audio")
            if audio is not None:
                result["audio"] = audio.relative_to(args.output_dir).as_posix()
            result["audio_manifest"] = manifest_path.relative_to(args.output_dir).as_posix()
        results.append(result)
    matches = sum(case["matches_expected"] for case in results)
    summary = {
        "cases": len(results), "matches": matches, "mismatches": len(results) - matches,
        "fallback_spans": sum(case["metadata"]["fallback_spans"] for case in results),
        "audio_cases": sum(case["audio"] is not None for case in results), "human_review": "pending",
    }
    report = {
        "version": 1, "summary": summary,
        "provenance": {"ttf": str(cli), "ttf_sha256": sha256(cli),
                       "corpus": str(args.corpus.resolve()), "corpus_sha256": sha256(args.corpus),
                       "resources": str(resources),
                       "resource_sha256": {p.relative_to(resources).as_posix(): sha256(p)
                                           for p in sorted(resources.rglob("*")) if p.is_file()},
                       "chunk_bytes": 1, "python": sys.version,
                       "audio_backend": "espeak" if args.synthesize else None},
        "timing_note": "frontend_process_seconds includes CLI startup and bytewise writes; it is not TTFB.",
        "cases": results,
    }
    write_json(args.output_dir / "report.json", report)
    write_html(args.output_dir / "report.html", report)
    with (args.output_dir / "review.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(("id", "reviewer", "pronunciation_correctness_1_to_5", "intelligibility_1_to_5",
                         "naturalness_1_to_5", "notes", "status"))
        for case in results:
            writer.writerow((case["id"], "", "", "", "", "", "pending"))
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ttf", type=Path, default=Path("build/ttf"))
    parser.add_argument("--resources", type=Path, default=Path("resources"))
    parser.add_argument("--corpus", type=Path, default=Path("tests/evaluation/cases.jsonl"))
    parser.add_argument("--output-dir", type=Path, required=True, help="new directory; existing paths are refused")
    parser.add_argument("--synthesize", action="store_true", help="also create audio using the optional eSpeak bridge")
    args = parser.parse_args()
    try:
        summary = evaluate(args)
    except (EvaluationError, OSError, UnicodeError, subprocess.SubprocessError) as exc:
        print(f"evaluation: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(summary, ensure_ascii=False))
    return 1 if summary["mismatches"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
