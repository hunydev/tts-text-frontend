#!/usr/bin/env python3
"""One disposable native-library process per web operation."""

import time as _time

STARTED = _time.monotonic()  # worker process start, for the /perf breakdown

import json
import random
import sys
import time
from pathlib import Path
from native import Engine, flatten
from workbench import (
    ROOT,
    corpus_path,
    reference_audit,
    reference_case_reviews,
    stream_sizes,
    validate,
    version,
)


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
                trace_match=flat.get("trace", []) == batch.get("trace", []),
                segments_match=segments == canonical,
                segment_count=len(segments),
            )
        )
    return dict(
        passed=all(
            v["match"] and v["span_match"] and v["trace_match"] and v["segments_match"] for v in variants
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
        if engine.detailed_trace:
            result.setdefault("trace_version", 1)
            result.setdefault("trace", [])
        result["tokens"] = engine.tokens(source)
        result["resource_validation"] = engine.resource_validation
        sys.path.insert(0, str(ROOT / "tools/lexicon"))
        from coverage import summarize

        result["lexicon_coverage"] = summarize(result["spans"])
        result["expected_match"] = (
            result["text"] == request["expected"] if "expected" in request else None
        )
        result["reference_review"] = {
            "reference_source": "request-supplied expectation"
            if "expected" in request
            else "no expectation supplied",
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


PERF_SIZES = (1, 10, 100, 1000)


def perf_sentences(count):
    """Jev evaluation sentences that mix digits and Latin letters, then other
    sentences with digits, cycled when the set is smaller than count."""
    import re
    path = ROOT / "benchmark/generated/jev-eval/ko-v1/sentences.jsonl"
    texts = [json.loads(l)["text"] for l in path.read_text(encoding="utf-8").splitlines() if l.strip()]
    mixed = [t for t in texts if re.search(r"\d", t) and re.search(r"[A-Za-z]", t)]
    digits = [t for t in texts if re.search(r"\d", t) and t not in set(mixed)]
    pool = mixed + digits
    return [pool[i % len(pool)] for i in range(count)], len(mixed)


def perf(request, library, resources):
    """Engine load, normalization of 1/10/100/1000 newline-joined sentences, and
    the per-request extras of a web normalization (coverage, tokens)."""
    opts = request["options"]
    emit(dict(event="start", version=version(), mode="perf"))
    process_ms = (time.monotonic() - STARTED) * 1000
    t = time.perf_counter()
    engine = Engine(library, resources, opts)
    load_ms = (time.perf_counter() - t) * 1000
    # The scaling table runs without the detailed trace, as production does: the
    # trace keeps every candidate (65536-event cap, about 700 sentences at once).
    traced = None
    if opts.get("detailed_trace"):
        sample = perf_sentences(1)[0][0].encode()
        times = []
        for _ in range(5):
            t = time.perf_counter()
            engine.normalize(sample)
            times.append((time.perf_counter() - t) * 1000)
        traced = sorted(times)[2]
        engine.close()
        engine = Engine(library, resources, dict(opts, detailed_trace=False))
    try:
        sentences, mixed = perf_sentences(max(PERF_SIZES))
        rows = []
        for n in PERF_SIZES:
            source = "\n".join(sentences[:n]).encode()
            repeats = 5 if n <= 10 else 3 if n <= 100 else 1
            times = []
            for _ in range(repeats):
                t = time.perf_counter()
                result = engine.normalize(source)
                times.append((time.perf_counter() - t) * 1000)
            times.sort()
            best = times[len(times) // 2]
            rows.append(dict(sentences=n, bytes=len(source), chars=len(source.decode()), repeats=repeats,
                             normalize_ms=round(best, 2), per_sentence_ms=round(best / n, 3),
                             spans=len(result["spans"])))
            emit(dict(event="progress", row=rows[-1]))
        one = engine.normalize(sentences[0].encode())
        sys.path.insert(0, str(ROOT / "tools/lexicon"))
        from coverage import summarize, coverage_entries, inventory
        t = time.perf_counter()
        summarize(one["spans"])
        coverage_ms = (time.perf_counter() - t) * 1000
        t = time.perf_counter()
        engine.tokens(sentences[0].encode())
        tokens_ms = (time.perf_counter() - t) * 1000
        t = time.perf_counter()
        summarize(one["spans"], dict(entries=inventory()["entries"]))
        uncached_ms = (time.perf_counter() - t) * 1000
        emit(dict(event="result", perf=dict(
            process_start_ms=round(process_ms, 1), engine_load_ms=round(load_ms, 1),
            coverage_ms=round(coverage_ms, 1), coverage_uncached_ms=round(uncached_ms, 1),
            tokens_ms=round(tokens_ms, 2), rows=rows, mixed_sentences=mixed,
            sample=sentences[0], traced_one_ms=round(traced, 2) if traced is not None else None)))
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
                # Historical references describe text mode. New corpus rows
                # explicitly opt into speech output and annotation parsing.
                options={
                    "default_language": row["default_language"],
                    "input_format": row.get("input_format", "plain"),
                    "output_mode": row.get("output_mode", "text"),
                    "unreadable": row.get("unreadable", "preserve"),
                    "hanja": row.get("hanja", "passthrough"),
                    "kana": row.get("kana", "passthrough"),
                    "max_control_bytes": row.get("max_control_bytes", 4096),
                    # Named policies a row declares (decorative, matrix, ...) apply too.
                    **{key: row[key] for key in ("brackets", "unknown_foreign", "decorative",
                                                 "initialism", "spacing", "matrix",
                                                 "symbol_fallback") if key in row},
                },
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
        handler = corpus if is_corpus else perf if request["mode"] == "perf" else run
        handler(request, Path(sys.argv[1]), Path(sys.argv[2]))
        emit(dict(event="end", ok=True))
    except BrokenPipeError:
        sys.exit(1)
    except Exception as exc:
        emit(dict(event="error", message=str(exc)))
        sys.exit(1)
