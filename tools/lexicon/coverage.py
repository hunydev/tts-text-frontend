#!/usr/bin/env python3
"""Find dictionary hits and vocabulary review priorities using native span traces.

Coverage is not pronunciation accuracy. Original text is processed locally; this
command never fetches or uploads user input and never adds fallback output to a
pronunciation dictionary automatically.
"""

import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import sys

from manage import ROOT, inventory

LEXICAL = {"WORD", "INITIALISM", "ACRONYM", "ALNUM_TERM", "ABBREVIATION"}
CACHE = ROOT / "benchmark/generated/cache/lexicon-coverage-index-v1.json"
FIELDS = ("id", "scope", "source_lang", "target_lang", "canonical_keys", "pronunciation", "review_status")


def _fingerprint(root=ROOT):
    rows = []
    for path in sorted((Path(root) / "lexicons").rglob("*")):
        if path.is_file():
            st = path.stat()
            rows.append("%s:%d:%d" % (path.relative_to(root), st.st_size, st.st_mtime_ns))
    return "|".join(rows)


def coverage_entries(root=ROOT, cache=CACHE):
    """The inventory fields summarize() needs, cached by the lexicon sources' size and mtime.

    inventory() validates every source file (~330 ms with the brand packs); a web
    request runs in a fresh worker, so the compact index is kept on disk and
    rebuilt only when a file under lexicons/ changes."""
    key = _fingerprint(root)
    try:
        cached = json.loads(Path(cache).read_text(encoding="utf-8"))
        if cached.get("fingerprint") == key:
            return cached["entries"]
    except (OSError, ValueError, KeyError):
        pass
    entries = [{k: r[k] for k in FIELDS if k in r} for r in inventory(root)["entries"]]
    try:
        Path(cache).parent.mkdir(parents=True, exist_ok=True)
        tmp = Path(cache).with_suffix(".tmp")
        tmp.write_text(json.dumps(dict(fingerprint=key, entries=entries), ensure_ascii=False), encoding="utf-8")
        tmp.replace(cache)
    except OSError:
        pass
    return entries


def summarize(spans, data=None):
    data = data or dict(entries=coverage_entries())
    index = {
        (r.get("scope", "system"), r["source_lang"], r["target_lang"], key): r
        for r in data["entries"]
        for key in r["canonical_keys"]
    }
    groups = {}
    paths = Counter()
    statuses = Counter()
    count = 0
    for span in spans:
        if (
            span["semantic_type"] not in LEXICAL
            or span["source_lang"] == span["context_lang"]
            or span["render_lang"] != span["context_lang"]
        ):
            continue
        trace = span.get("lexicon", "")
        if not trace:
            continue
        count += 1
        route = (
            "direct-lexicon"
            if trace.startswith("lexicon:")
            else "source-dictionary"
            if "dictionary" in trace
            else "fallback"
        )
        paths[route] += 1
        surface = span["surface"]
        key = " ".join(surface.split()).translate(
            str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz")
        )
        scope = (
            "system"
            if trace == "lexicon:system"
            else "finance"
            if trace == "lexicon:domain"
            else None
        )
        entry = (
            index.get((scope, span["source_lang"], span["render_lang"], key))
            if scope
            else None
        )
        if entry and span.get("normalized") != entry["pronunciation"]:
            entry = None
        status = (
            entry["review_status"]
            if entry
            else "external-override"
            if route == "direct-lexicon"
            else "unreviewed-rendering"
        )
        statuses[status] += 1
        identity = (key, span["source_lang"], span["render_lang"], trace)
        if identity not in groups:
            groups[identity] = dict(
                term=surface,
                canonical_key=key,
                source_lang=span["source_lang"],
                render_lang=span["render_lang"],
                route=route,
                trace=trace,
                review_status=status,
                entry_id=entry["id"] if entry else None,
                count=0,
                rendered_examples=[],
            )
        item = groups[identity]
        item["count"] += 1
        if (
            span.get("normalized") not in item["rendered_examples"]
            and len(item["rendered_examples"]) < 3
        ):
            item["rendered_examples"].append(span.get("normalized", ""))
    rows = sorted(
        groups.values(),
        key=lambda x: (x["route"] == "direct-lexicon", -x["count"], x["canonical_key"]),
    )
    return dict(
        eligible_foreign_spans=count,
        routes=dict(paths),
        review_status=dict(statuses),
        direct_lexicon_rate=paths["direct-lexicon"] / count if count else None,
        terms=rows,
        review_candidates=[r for r in rows if r["route"] != "direct-lexicon"],
        note="Counts cover rendered foreign lexical spans only. English context, protected text and semantic numbers are excluded. Coverage and fallback output do not establish pronunciation correctness.",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--ttf", type=Path, default=ROOT / "build/ttf")
    parser.add_argument("--resources", type=Path, default=ROOT / "resources")
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--finance",
        action="store_true",
        help="load the finance vocabulary without legacy numeric policy",
    )
    args = parser.parse_args()
    try:
        text = args.input.read_text(encoding="utf-8")
        options = (
            [
                "--domain",
                str(args.resources.resolve() / "domains/finance/ko-KR/lexicon.tsv"),
            ]
            if args.finance
            else []
        )
        run = subprocess.run(
            [
                str(args.ttf.resolve()),
                "stream-json",
                "--resources",
                str(args.resources.resolve()),
                *options,
                "--chunk-bytes",
                "4096",
                "-",
            ],
            input=text.encode(),
            capture_output=True,
            timeout=60,
            check=True,
        )
        sys.path.insert(0, str(ROOT / "tools/evaluation"))
        from evaluate import parse_stream

        segments = parse_stream(run.stdout.decode(), text)
        raw = text.encode()
        spans = []
        for event in segments:
            output = event["text"].encode()
            spans.extend(
                dict(
                    s,
                    surface=raw[s["input_begin"] : s["input_end"]].decode(),
                    normalized=output[s["begin"] : s["end"]].decode(),
                )
                for s in event["spans"]
            )
        report = summarize(spans)
        result = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.output:
            args.output.write_text(result, encoding="utf-8")
        else:
            print(result, end="")
    except (ValueError, OSError, UnicodeError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
