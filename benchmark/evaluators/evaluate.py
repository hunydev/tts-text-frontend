#!/usr/bin/env python3
"""Streaming text benchmark evaluation with native context and explicit denominators.

Reports agreement with supplied references, not linguistic or synthesized-speech
quality. Candidate spans are never combined with dataset-reference accuracy.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

BENCHMARK = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BENCHMARK))
from common import ROOT, digest_json, ensure_output, read_records, sha256, tree_hash


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Backend:
    def __init__(self, library, cli, resources, options, overrides):
        self.library, self.cli, self.resources = library, cli, resources
        self.options, self.overrides, self.engines = options, overrides, {}
        self.adapter = (
            load_module("ttf_benchmark_native", ROOT / "tools/web/native.py")
            if library
            else None
        )
        self.protocol = (
            load_module("ttf_benchmark_protocol", ROOT / "tools/evaluation/evaluate.py")
            if cli
            else None
        )

    def run(self, text, language, chunk=None):
        raw = text.encode()
        if self.library:
            if language not in self.engines:
                self.engines[language] = self.adapter.Engine(
                    self.library,
                    self.resources,
                    dict(self.options, default_language=language),
                    [(p, 2) for p in self.overrides],
                )
            engine = self.engines[language]
            if chunk:
                result = self.adapter.flatten(
                    engine.stream(raw, [chunk] * ((len(raw) + chunk - 1) // chunk))
                )
            else:
                result = engine.normalize(raw)
            return validate_result(text, result)
        command = [
            str(self.cli),
            "stream-json",
            "--resources",
            str(self.resources),
            "--language",
            language,
            "--chunk-bytes",
            str(chunk or max(1, len(raw))),
        ]
        for path in self.overrides:
            command += ["--domain", str(path)]
        completed = subprocess.run(
            command + ["-"], input=raw, capture_output=True, timeout=30, check=True
        )
        segments = self.protocol.parse_stream(completed.stdout.decode(), text)
        offset, spans = 0, []
        for segment in segments:
            for span in segment["spans"]:
                spans.append(
                    dict(span, begin=span["begin"] + offset, end=span["end"] + offset)
                )
            offset += len(segment["text"].encode())
        return validate_result(
            text, {"text": "".join(s["text"] for s in segments), "spans": spans}
        )

    def close(self):
        for engine in self.engines.values():
            engine.close()


def validate_result(text, result):
    """Apply the same full source/output coverage contract to every backend."""
    if (
        not isinstance(result, dict)
        or not isinstance(result.get("text"), str)
        or not isinstance(result.get("spans"), list)
    ):
        raise ValueError("native result must contain text and spans")  # noqa: TRY004 - adapter validation contract
    source, output = text.encode("utf-8"), result["text"].encode("utf-8")
    input_at, output_at = 0, 0
    for span in result["spans"]:
        if not isinstance(span, dict):
            raise ValueError("native span must be an object")  # noqa: TRY004 - adapter validation contract
        fields = ("input_begin", "input_end", "begin", "end")
        if any(type(span.get(key)) is not int for key in fields):
            raise ValueError("native offsets must be integers")
        start, stop, begin, end = (span[key] for key in fields)
        if start != input_at or not start < stop <= len(source):
            raise ValueError("native spans must cover input contiguously")
        if begin != output_at or not begin <= end <= len(output):
            raise ValueError("native spans must cover output contiguously")
        surface = source[start:stop].decode("utf-8")
        normalized = output[begin:end].decode("utf-8")
        if any(not isinstance(span.get(key), str) for key in ("semantic_type", "rule")):
            raise ValueError("native semantic type and rule must be strings")
        for key, expected in (("surface", surface), ("normalized", normalized)):
            if key in span and span[key] != expected:
                raise ValueError(f"native {key} does not match its offsets")
        input_at, output_at = stop, end
    if input_at != len(source) or output_at != len(output):
        raise ValueError("native spans do not cover all input and output")
    return result


def align_span(reference, result):
    start, end = reference["begin"], reference["end"]
    selected = [
        s for s in result["spans"] if s["input_begin"] < end and s["input_end"] > start
    ]
    if (
        not selected
        or selected[0]["input_begin"] != start
        or selected[-1]["input_end"] != end
    ):
        return None, "crossed-or-missing-input-boundary", selected
    at = start
    for span in selected:
        if span["input_begin"] != at or span["input_end"] <= at:
            return None, "noncontiguous-input-coverage", selected
        at = span["input_end"]
    actual = (
        result["text"].encode()[selected[0]["begin"] : selected[-1]["end"]].decode()
    )
    return actual, None, selected


def spoken_projection(record, scored, result, error):
    """Project complete references, rejecting any unaccounted lexical output."""
    if error:
        return None, "engine-error"
    if not scored:
        return None, "no-reference-spans"
    raw, at = record["text"].encode(), 0
    for span in scored + [{"begin": len(raw), "end": len(raw), "actual": ""}]:
        if span["actual"] is None:
            return None, "unaligned-reference-span"
        if at < span["begin"]:
            if raw[at : span["begin"]].decode("utf-8").strip():
                return None, "non-whitespace-input-gap"
            gap, alignment, _ = align_span({"begin": at, "end": span["begin"]}, result)
            if alignment:
                return None, "unaligned-whitespace-gap"
            if gap.strip():
                return None, "non-whitespace-gap-output"
        at = span["end"]
    words = []
    for span in scored:
        value = span["actual"]
        if (
            span["semiotic_class"] == "PUNCT"
            and span.get("spoken") == ""
            and value == span["written"]
        ):
            value = ""
        if value:
            words.append(value)
    return " ".join(words), None


def mismatch_kind(reference, actual, alignment_error):
    """Name why a span missed, so a dataset convention is not read as an error.

    This is a diagnostic. The accuracy above compares bytes and folds nothing.
    """
    if alignment_error:
        return "alignment"
    if actual is None:
        return "no-output"
    if actual == reference:
        return None
    if actual.lower() == reference.lower():
        return "case-only"
    if actual.split() == reference.split():
        return "space-only"
    if actual.lower().split() == reference.lower().split():
        return "case-and-space"
    return "different"


def score_case(record, result, error=None):
    if not error:
        try:
            validate_result(record["text"], result)
        except (ValueError, UnicodeError) as exc:
            error = f"invalid-native-result: {exc}"
    scored = []
    for reference in record["spans"]:
        actual, alignment, native = (
            align_span(reference, result) if not error else (None, "engine-error", [])
        )
        target = reference.get("spoken")
        scored.append(
            dict(
                reference,
                actual=actual,
                alignment_error=alignment,
                exact=None if target is None else actual == target,
                mismatch_kind=None
                if target is None or actual == target
                else mismatch_kind(target, actual, alignment),
                engine_semantics=[s["semantic_type"] for s in native],
                rules=[s["rule"] for s in native],
                lexicon_routes=[
                    s.get("lexicon", "") for s in native if s.get("lexicon")
                ],
            )
        )
    expected = record.get("expected")
    projection, projected_exact, projection_error = None, None, None
    # The secondary projection removes only unchanged punctuation with an explicit
    # PUNCT -> empty reference. It never rewrites numbers, case or lexical choices.
    if expected is not None:
        projection, projection_error = spoken_projection(record, scored, result, error)
        projected_exact = projection_error is None and projection == expected
    return {
        "id": record["id"],
        "dataset": record["dataset"],
        "split": record["split"],
        "language": record["language"],
        "reference_status": record["reference_status"],
        "usage": record["usage"],
        "text": record["text"],
        "expected": expected,
        "actual": result["text"] if not error else None,
        "sentence_exact": None
        if expected is None
        else not error and result["text"] == expected,
        "spoken_projection": projection,
        "spoken_projection_exact": projected_exact,
        "spoken_projection_error": projection_error,
        "engine_error": error,
        "spans": scored,
        "reference_digest": digest_json(record),
    }


def increment(bucket, exact, unaligned=False):
    bucket["total"] = bucket.get("total", 0) + 1
    bucket["exact"] = bucket.get("exact", 0) + int(exact is True)
    bucket["unaligned"] = bucket.get("unaligned", 0) + int(unaligned)
    bucket["accuracy"] = bucket["exact"] / bucket["total"]


class Summary:
    def __init__(self):
        self.data = {
            "cases": 0,
            "engine_errors": 0,
            "unlabeled_spans": 0,
            "sentence": {},
            "spoken_projection": {},
            "spans": {},
            "semiotic_classes": {},
            "class_to_engine_semantics": {},
            # Diagnostics. They never enter the accuracy above.
            "mismatch_kinds": {},
            "diagnostic_case_insensitive": {},
            "chunk_comparisons": 0,
            "chunk_failures": 0,
        }

    def add(self, row):
        d = self.data
        d["cases"] += 1
        d["engine_errors"] += bool(row["engine_error"])
        if row["sentence_exact"] is not None:
            increment(
                d["sentence"].setdefault(row["reference_status"], {}),
                row["sentence_exact"],
            )
        if row["spoken_projection_exact"] is not None:
            bucket = d["spoken_projection"].setdefault(row["reference_status"], {})
            reason = row["spoken_projection_error"]
            increment(bucket, row["spoken_projection_exact"], bool(reason))
            bucket["unavailable"] = bucket.get("unavailable", 0) + bool(reason)
            bucket["available"] = bucket["total"] - bucket["unavailable"]
            bucket["coverage"] = bucket["available"] / bucket["total"]
            if reason:
                reasons = bucket.setdefault("unavailable_reasons", {})
                reasons[reason] = reasons.get(reason, 0) + 1
        for span in row["spans"]:
            label, status = span["semiotic_class"], span["reference_status"]
            if span["exact"] is None:
                d["unlabeled_spans"] += 1
                continue
            increment(
                d["spans"].setdefault(status, {}),
                span["exact"],
                bool(span["alignment_error"]),
            )
            increment(
                d["semiotic_classes"].setdefault(label, {}).setdefault(status, {}),
                span["exact"],
                bool(span["alignment_error"]),
            )
            kind = span.get("mismatch_kind")
            if kind:
                kinds = d["mismatch_kinds"].setdefault(label, {})
                kinds[kind] = kinds.get(kind, 0) + 1
            folded = d["diagnostic_case_insensitive"].setdefault(label, {})
            increment(folded, span["exact"] is True or kind == "case-only")
            observed = "+".join(span["engine_semantics"]) or "NONE"
            confusion = d["class_to_engine_semantics"].setdefault(label, {})
            confusion[observed] = confusion.get(observed, 0) + 1
        for comparison in row["streaming"]:
            d["chunk_comparisons"] += 1
            d["chunk_failures"] += not comparison["equal"]

    def finish(self):
        d = self.data
        for label, excluded in (("all", set()), ("tn_only", {"PLAIN", "PUNCT"})):
            classes = [
                sum(
                    values.get(s, {}).get("total", 0)
                    for s in ("dataset-reference", "provided-reference")
                )
                for key, values in d["semiotic_classes"].items()
                if key not in excluded
            ]
            exact = [
                sum(
                    values.get(s, {}).get("exact", 0)
                    for s in ("dataset-reference", "provided-reference")
                )
                for key, values in d["semiotic_classes"].items()
                if key not in excluded
            ]
            total = sum(classes)
            rates = [ok / n for ok, n in zip(exact, classes) if n]
            d[label + "_reference_spans"] = {
                "total": total,
                "exact": sum(exact),
                "micro_accuracy": sum(exact) / total if total else None,
                "macro_class_accuracy": sum(rates) / len(rates) if rates else None,
            }
        return d


def write_json(path, value):
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def fingerprint(corpus, program, resources, overrides, library):
    adapter = ROOT / (
        "tools/web/native.py" if library else "tools/evaluation/evaluate.py"
    )
    return {
        "corpus_sha256": sha256(corpus),
        "program_sha256": sha256(program),
        "resources_sha256": tree_hash(resources),
        "overrides": [
            {"path": str(path), "sha256": sha256(path)} for path in overrides
        ],
        "evaluator_sha256": sha256(Path(__file__)),
        "schema_sha256": sha256(BENCHMARK / "common.py"),
        "adapter_sha256": {adapter.relative_to(ROOT).as_posix(): sha256(adapter)},
    }


def evaluate(args):
    corpus = args.corpus.resolve(strict=True)
    resources = args.resources.resolve(strict=True)
    program = (args.library or args.ttf).resolve(strict=True)
    overrides = [p.resolve(strict=True) for p in args.domain]
    args.output_dir = ensure_output(args.output_dir)
    args.output_dir.mkdir(parents=True, exist_ok=False)
    before = fingerprint(corpus, program, resources, overrides, args.library)
    options = {}  # Native defaults are stable ABI v1; record the exact backend.
    backend = Backend(
        program if args.library else None,
        program if not args.library else None,
        resources,
        options,
        overrides,
    )
    summary, case_hash = Summary(), __import__("hashlib").sha256()
    use_counts, datasets = Counter(), Counter()
    try:
        with (
            (args.output_dir / "results.jsonl").open("w", encoding="utf-8") as results,
            (args.output_dir / "failures.jsonl").open(
                "w", encoding="utf-8"
            ) as failures,
        ):
            for record in read_records(corpus, args.limit):
                if (
                    record["usage"] == "noncommercial-benchmark-only"
                    and not args.allow_noncommercial
                ):
                    raise ValueError(
                        "noncommercial benchmark: explicitly pass --allow-noncommercial only for an authorized noncommercial use"
                    )
                case_hash.update((digest_json(record) + "\n").encode())
                use_counts[record["usage"]] += 1
                datasets[record["dataset"] + "/" + record["split"]] += 1
                result, error = {"text": "", "spans": []}, None
                try:
                    result = backend.run(record["text"], record["language"])
                except (ValueError, OSError, subprocess.SubprocessError) as exc:
                    error = str(exc)
                row = score_case(record, result, error)
                row["streaming"] = []
                for chunk in args.chunk_bytes:
                    try:
                        streamed = backend.run(
                            record["text"], record["language"], chunk
                        )
                        # Both adapters return the same full result schema. Compare
                        # every native span field in addition to text.
                        equal = not error and streamed == result
                        row["streaming"].append({"chunk_bytes": chunk, "equal": equal})
                    except (ValueError, OSError, subprocess.SubprocessError) as exc:
                        row["streaming"].append(
                            {"chunk_bytes": chunk, "equal": False, "error": str(exc)}
                        )
                summary.add(row)
                results.write(json.dumps(row, ensure_ascii=False) + "\n")
                for index, span in enumerate(row["spans"]):
                    if span["exact"] is False:
                        failures.write(
                            json.dumps(
                                dict(
                                    id=row["id"],
                                    dataset=row["dataset"],
                                    split=row["split"],
                                    usage=row["usage"],
                                    span_index=index,
                                    text=row["text"],
                                    **span,
                                ),
                                ensure_ascii=False,
                            )
                            + "\n"
                        )
    finally:
        backend.close()
    after = fingerprint(corpus, program, resources, overrides, args.library)
    if before != after:
        changed = ", ".join(key for key in before if before[key] != after[key])
        raise ValueError(
            f"evaluation inputs changed during execution: {changed}; no completed summary written"
        )
    report = {
        "schema_version": 1,
        "status": "completed",
        "summary": summary.finish(),
        "provenance": dict(
            **before,
            evaluated_records_sha256=case_hash.hexdigest(),
            limit=args.limit,
            backend="native-batch" if args.library else "cli-one-write-stream",
            input_stability="sha256-before-after-identical",
            chunk_bytes=args.chunk_bytes,
            python=sys.version,
            datasets=dict(datasets),
            usage=dict(use_counts),
        ),
        "metric_policy": "v2-exact-context-complete-output",
        "note": "Exact UTF-8 text equality: no case, whitespace or punctuation folding. Full native input/output coverage is required. Unaligned reference spans count as failures. Secondary spoken projection omits only unchanged PUNCT with an explicit empty reference, requires whitespace-only output for uncovered whitespace input, and joins complete source spans with spaces. Every labeled sentence is in the projection denominator; unavailable projections count as failures and coverage/reasons are reported separately. Candidate agreement is separate from reference accuracy. `mismatch_kinds` and `diagnostic_case_insensitive` are diagnostics that name why a span missed (case, spacing, alignment or a different reading); they never enter the accuracy, which still folds nothing. Input, resource, program and adapter hashes must match before and after execution. No audio or independent human correctness claim. Failures/reports inherit source usage restrictions and must stay outside product resources.",
    }
    write_json(args.output_dir / "summary.json", report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, required=True)
    backend = parser.add_mutually_exclusive_group()
    backend.add_argument("--library", type=Path)
    backend.add_argument("--ttf", type=Path)
    parser.add_argument("--resources", type=Path, default=ROOT / "resources")
    parser.add_argument("--domain", type=Path, action="append", default=[])
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--limit",
        type=int,
        default=1000,
        help="deterministic prefix; 0 means all records",
    )
    parser.add_argument("--chunk-bytes", type=int, nargs="*", default=[])
    parser.add_argument("--allow-noncommercial", action="store_true")
    args = parser.parse_args()
    if args.limit < 0 or any(n <= 0 for n in args.chunk_bytes):
        parser.error("limit must be nonnegative and chunk sizes positive")
    if not args.library and not args.ttf:
        args.ttf = ROOT / "build/ttf"
    try:
        report = evaluate(args)
        print(json.dumps(report["summary"], ensure_ascii=False))
        return 0  # Measured mismatches are expected research results, not tool failure.
    except (ValueError, OSError, UnicodeError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
