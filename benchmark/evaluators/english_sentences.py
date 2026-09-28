#!/usr/bin/env python3
"""Sentence agreement of English TN with supplied references, English-only scope.

Scores whole sentences of one or more schema-v1 corpora (NeMo evaluation sets,
the Sproat sample, ...) so different English sources report one comparable
number. Sentences containing another script (Hangul, Han, Kana, Cyrillic, ...)
are out of scope: counted and listed, never scored (benchmark/english_scope.py).

Levels, each a superset of the previous one:
  exact    byte-identical to a reference
  compact  identical after removing whitespace
  loose    identical after case folding, turning punctuation into spaces and
           collapsing whitespace (datasets disagree on punctuation and case:
           Sproat drops it, GoogleTN keeps `` . ``, LibriTTS keeps CHAPTER)
  variant  loose, and British/American spelling folded on both sides. The
           Sproat/GoogleTN references rewrite centre -> center and
           organisation -> organization; that is the reference's spelling
           policy, not normalization, so neither spelling is penalised.
A sentence with several acceptable references (EngConf) matches any of them.
`rejected` counts outputs that loosely equal a reading the dataset marks wrong.

  python3 benchmark/evaluators/english_sentences.py \\
    --corpus benchmark/generated/en-eval/nemo-eval.jsonl \\
    --corpus benchmark/generated/en-eval/sproat-sample.jsonl \\
    --library build/libtts_text_frontend.so --resources resources \\
    --out benchmark/generated/en-eval/reports/NAME
"""
import argparse
import hashlib
import json
import re
import sys
import unicodedata
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from benchmark.common import digest_json, ensure_output, read_records, sha256, tree_hash  # noqa: E402
from benchmark.english_scope import VERSION as SCOPE_VERSION, foreign_scripts  # noqa: E402
from benchmark.evaluators.evaluate import Backend, validate_result  # noqa: E402

METRIC_VERSION = "en-sentence-agreement-v1"
LEVELS = ("exact", "compact", "loose", "variant")
# British -> American spelling, applied to every word on both sides (symmetric, so
# a word that merely looks like a pattern folds identically in output and reference).
SPELLING = [(re.compile(p), r) for p, r in (
    (r"(?<=[a-z]{2})is(e|es|ed|ing|ation|ations|er|ers)$", r"iz\1"),
    (r"(?<=[a-z])ys(e|es|ed|ing)$", r"yz\1"),
    (r"(?<=[a-z]{2})our(s|ed|ing|ite|ites|able|er|ers)?$", r"or\1"),
    (r"(?<=[a-z][^aeiou])re(s|d)?$", r"er\1"),
    (r"(?<=[a-z]{3})mme(s)?$", r"m\1"),
    (r"(?<=[a-z]{3})ogue(s)?$", r"og\1"),
    (r"(?<=[a-z]{3})ence(s)?$", r"ense\1"),
    (r"(?<=[a-z]{2}[aeiou])ll(ed|ing|er|ers)$", r"l\1"),
    (r"(?<=[a-z])(ae|oe)(?=[a-z]{2})", "e"))]


def compact(text):
    return re.sub(r"\s+", "", text)


def loose(text):
    # The backtick is a modifier symbol in Unicode but a quote mark (`` ... '') here.
    folded = "".join(" " if unicodedata.category(ch).startswith("P") or ch == "`" else ch
                     for ch in text.casefold())
    return " ".join(folded.split())


def spelling(text):
    words = []
    for word in loose(text).split():
        if len(word) > 4 and word.isalpha():
            for pattern, replacement in SPELLING:
                word = pattern.sub(replacement, word)
        words.append(word)
    return " ".join(words)


def level(actual, references):
    """Strictest level at which actual equals any reference, or None."""
    if actual is None:
        return None
    for name, fold in (("exact", str), ("compact", compact), ("loose", loose), ("variant", spelling)):
        if fold(actual) in {fold(r) for r in references}:
            return name
    return None


def metric():
    return dict(sentences=0, out_of_scope=0, unscorable=0, scorable=0, errors=0,
                rejected=0, **{name: 0 for name in LEVELS})


def finish(m):
    rates = {name + "_rate": m[name] / m["scorable"] if m["scorable"] else None for name in LEVELS}
    return dict(m, **rates)


def run(corpora, library, resources, output, mode="speech", scope="english", limit=0, backend=None):
    output = ensure_output(output)
    if output.exists():
        raise ValueError("report exists; create a new run directory")
    corpora = [Path(c).resolve(strict=True) for c in corpora]
    library, resources = Path(library).resolve(strict=True), Path(resources).resolve(strict=True)
    harness = [Path(__file__), ROOT / "benchmark/english_scope.py",
               ROOT / "benchmark/evaluators/evaluate.py", ROOT / "benchmark/common.py",
               ROOT / "tools/web/native.py"]

    def identity():
        return dict(corpora={c.name: sha256(c) for c in corpora}, library=sha256(library),
                    resources=tree_hash(resources),
                    harness={p.relative_to(ROOT).as_posix(): sha256(p) for p in harness})

    before = identity()
    output.mkdir(parents=True)
    owned = backend is None
    backend = backend or Backend(library, None, resources, {"output_mode": mode}, [])
    total, datasets, scripts = metric(), defaultdict(metric), Counter()
    started = datetime.now(timezone.utc).isoformat()
    records = hashlib.sha256()
    try:
        with (output / "results.jsonl").open("w", encoding="utf-8", newline="\n") as results, \
             (output / "out-of-scope.jsonl").open("w", encoding="utf-8", newline="\n") as excluded:
            for corpus in corpora:
                for row in read_records(corpus, limit):
                    records.update((digest_json(row) + "\n").encode())
                    bucket = datasets[row["dataset"]]
                    for m in (total, bucket):
                        m["sentences"] += 1
                    found = foreign_scripts(row["text"]) if scope == "english" else []
                    if found:
                        scripts.update(found)
                        for m in (total, bucket):
                            m["out_of_scope"] += 1
                        excluded.write(json.dumps(dict(id=row["id"], scripts=found, text=row["text"]),
                                                  ensure_ascii=False) + "\n")
                        continue
                    references = [r for r in [row.get("expected")] + list(row.get("accepted_references") or []) if r]
                    references = list(dict.fromkeys(references))
                    if not references:
                        for m in (total, bucket):
                            m["unscorable"] += 1
                        continue
                    actual, error = None, None
                    try:
                        result = backend.run(row["text"], row["language"])
                        validate_result(row["text"], result)
                        actual = result["text"]
                    except (ValueError, RuntimeError, OSError, UnicodeError) as exc:
                        error = str(exc)
                    matched = level(actual, references)
                    rejected = matched is None and actual is not None and \
                        loose(actual) in {loose(r) for r in row.get("rejected_references") or []}
                    for m in (total, bucket):
                        m["scorable"] += 1
                        m["errors"] += bool(error)
                        m["rejected"] += rejected
                        if matched:
                            for name in LEVELS[LEVELS.index(matched):]:
                                m[name] += 1
                    results.write(json.dumps(dict(id=row["id"], dataset=row["dataset"], text=row["text"],
                                                  references=references, actual=actual, error=error,
                                                  level=matched, rejected=rejected),
                                             ensure_ascii=False) + "\n")
    finally:
        if owned:
            backend.close()
    if identity() != before:
        raise ValueError("inputs or harness changed during evaluation; report is not publishable")
    protocol = dict(metric_version=METRIC_VERSION, scope=SCOPE_VERSION if scope == "english" else "all",
                    output_mode=mode, limit=limit, corpora=before["corpora"], harness=before["harness"],
                    evaluated_records_sha256=records.hexdigest())
    report = dict(schema_version=1, status="complete", created_at=started,
                  finished_at=datetime.now(timezone.utc).isoformat(),
                  comparison_key=digest_json(protocol), protocol=protocol,
                  engine=dict(library_sha256=before["library"], resources_sha256=before["resources"]),
                  sentences=finish(total), datasets={k: finish(v) for k, v in sorted(datasets.items())},
                  out_of_scope_scripts=dict(scripts),
                  policy=dict(scope="Sentences with letters of a non-English script are out of scope and "
                                     "excluded from every denominator; Latin diacritics and lone Greek "
                                     "letters stay in scope.",
                              levels="exact <= compact <= loose <= variant; loose folds case and punctuation "
                                     "only, never number words; variant also folds British/American spelling.",
                              references="Dataset references, not human-approved gold; several acceptable "
                                         "readings count as a match with any of them."))
    (output / "summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                         encoding="utf-8", newline="\n")
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--corpus", type=Path, action="append", required=True)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--resources", type=Path, default=ROOT / "resources")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--output-mode", choices=("speech", "text"), default="speech")
    parser.add_argument("--scope", choices=("english", "all"), default="english")
    parser.add_argument("--limit", type=int, default=0, help="per-corpus prefix; 0 means all")
    args = parser.parse_args(argv)
    report = run(args.corpus, args.library, args.resources, args.out,
                 args.output_mode, args.scope, args.limit)
    print(json.dumps(dict(sentences=report["sentences"], datasets=report["datasets"]), ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
