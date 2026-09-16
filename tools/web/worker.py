#!/usr/bin/env python3
"""One disposable native-library process per web operation."""

import json
import random
import sys
import time
from pathlib import Path
from native import Engine, flatten
from workbench import ROOT, corpus_path, reference_audit, reference_case_reviews, stream_sizes, validate, version


def emit(event):
    print(json.dumps(event, ensure_ascii=False, separators=(",", ":")), flush=True)


def sizes(length, size):
    return [size] * ((length + size - 1) // size)


def compare(engine, source, batch, options):
    variants = []
    canonical = None
    schedules = [(f"{n} byte", sizes(len(source), n)) for n in options["verify_chunks"]]
    rng = random.Random(options["random_seed"])
    at = 0
    schedule = []
    while at < len(source):
        n = rng.randint(1, 31)
        schedule.append(n)
        at += n
    schedules.append((f"random seed {options['random_seed']}", schedule))
    if options["chunk_sizes"]:
        schedules.append(("custom writes", stream_sizes(len(source), options)))
    for label, schedule in schedules:
        segments = engine.stream(source, schedule)
        flat = flatten(segments)
        if canonical is None:
            canonical = segments
        variants.append(
            dict(
                label=label,
                chunk_sizes=schedule[:64],
                chunk_count=len(schedule),
                match=flat["text"] == batch["text"],
                span_match=flat["spans"] == batch["spans"],
                segments_match=segments == canonical,
                segment_count=len(segments),
            )
        )
    return dict(
        passed=all(
            v["match"] and v["span_match"] and v["segments_match"] for v in variants
        ),
        variants=variants,
    )


def run(request, library, resources):
    engine = Engine(library, resources, request["options"])
    started = time.monotonic()
    try:
        source = request["text"].encode()
        mode = request["mode"]
        opts = request["options"]
        emit(dict(event="start", version=version(), mode=mode))
        if mode == "stream":
            segments = engine.stream(
                source, stream_sizes(len(source), opts), emit, opts["delay_ms"]
            )
            result = flatten(segments)
        else:
            result = engine.normalize(source)
        result["tokens"] = engine.tokens(source)
        result["expected_match"] = (
            result["text"] == request["expected"] if "expected" in request else None
        )
        result["reference_review"] = {
            "reference_source": "request-supplied expectation" if "expected" in request else "no expectation supplied",
            "review_status": "not-independently-reviewed",
            "human_approved": False,
        }
        if mode == "verify":
            result["verification"] = compare(engine, source, result, opts)
        emit(
            dict(
                result,
                event="result",
                elapsed_ms=round((time.monotonic() - started) * 1000, 3),
            )
        )
    finally:
        engine.close()


def corpus(request, library, resources):
    sys.path.insert(0, str(ROOT / "tools/evaluation"))
    from evaluate import read_corpus, check_spans

    rows = read_corpus(corpus_path(request["corpus"]))
    audit = reference_audit()
    reviews = reference_case_reviews(audit, request["corpus"])
    results = []
    emit(dict(event="start", version=version(), mode="corpus", total=len(rows)))
    for row in rows:
        opts = validate(
            dict(
                mode="verify",
                text=row["text"],
                options={"default_language": row["default_language"]},
            )
        )["options"]
        overrides = [
            (resources / item["path"], {"domain": 2, "customer": 3}[item["layer"]])
            for item in row["resource_overrides"]
        ]
        engine = Engine(library, resources, opts, overrides)
        try:
            source = row["text"].encode()
            batch = engine.normalize(source)
            verification = compare(engine, source, batch, opts)
            segments = engine.stream(source, sizes(len(source), 7))
            failures = check_spans(row, segments)
            passed = (
                batch["text"] == row["expected"]
                and not failures
                and verification["passed"]
            )
            result = dict(
                id=row["id"],
                text=row["text"],
                expected=row["expected"],
                actual=batch["text"],
                passed=passed,
                metadata_failures=failures,
                parity=verification["passed"],
                reference_review=reviews[row["id"]],
            )
            results.append(result)
            emit(dict(result, event="case"))
        finally:
            engine.close()
    emit(
        dict(
            event="result",
            total=len(results),
            passed=sum(r["passed"] for r in results),
            failed=sum(not r["passed"] for r in results),
            cases=results,
            pending_independent_review=len(results),
            human_approved_cases=0,
        )
    )


if __name__ == "__main__":
    try:
        request = json.load(sys.stdin)
        is_corpus = "corpus" in request
        request = validate(request, corpus=is_corpus)
        (corpus if is_corpus else run)(request, Path(sys.argv[1]), Path(sys.argv[2]))
        emit(dict(event="end", ok=True))
    except BrokenPipeError:
        sys.exit(1)
    except Exception as exc:
        emit(dict(event="error", message=str(exc)))
        sys.exit(1)
