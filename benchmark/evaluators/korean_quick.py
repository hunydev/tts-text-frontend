#!/usr/bin/env python3
"""Fast Korean regression check against the release benchmark, without a report.

    korean_quick.py LIBRARY RESOURCES OUT.jsonl.gz [--baseline OLD.jsonl.gz] [--jobs N]

Runs the Korean evaluator's options over import-v1, applies the reference-error
exclusions (reviews/reference-errors-v2.jsonl), writes id -> actual, and prints
raw and adjusted variant agreement. With a baseline it also writes
OUT.diff.json with every sentence whose verdict changed. --jobs splits the
sentences over that many engine processes (default 2, the VM's cores); the
output order and the numbers do not depend on it. The benchmark data are local
only (benchmark/generated/, ignored): --data or TTF_KO_RELEASE points at them.
"""
import argparse
import gzip
import json
import multiprocessing
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from benchmark.evaluators import korean_release as kr  # noqa: E402

backend = None


def start(library, resources):
    global backend
    backend = kr.Backend(Path(library), None, Path(resources), kr.OPTIONS, [])


def read(text):
    try:
        return backend.run(text, "ko-KR")["text"]
    except (ValueError, RuntimeError, OSError):
        return None


def default_data():
    if os.environ.get("TTF_KO_RELEASE"):
        return Path(os.environ["TTF_KO_RELEASE"])
    here = ROOT / "benchmark/generated/ko-release"
    return here if here.exists() else Path.home() / "tts-text-frontend/benchmark/generated/ko-release"


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("library")
    p.add_argument("resources")
    p.add_argument("out")
    p.add_argument("--baseline")
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--data", type=Path, default=default_data())
    a = p.parse_args()
    excluded = {json.loads(line)["id"] for line in (a.data / "reviews/reference-errors-v2.jsonl").open()}
    rows = [json.loads(line) for line in (a.data / "import-v1/records.jsonl").open(encoding="utf-8")]
    texts = [row["text"] for row in rows]
    if a.jobs > 1:
        with multiprocessing.get_context("fork").Pool(a.jobs, start, (a.library, a.resources)) as pool:
            actuals = pool.map(read, texts, chunksize=500)
    else:
        start(a.library, a.resources)
        actuals = [read(text) for text in texts]
    before = {}
    if a.baseline:
        with gzip.open(a.baseline, "rt", encoding="utf-8") as h:
            before = {r["id"]: r["actual"] for r in map(json.loads, h)}
    raw = adj = raw_n = adj_n = changed = 0
    gained, lost = [], []
    variant = kr.variant
    with gzip.open(a.out, "wt", encoding="utf-8") as w:
        for row, actual in zip(rows, actuals):
            w.write(json.dumps(dict(id=row["id"], actual=actual), ensure_ascii=False) + "\n")
            expected = row["expected"]
            if expected is None:
                continue
            ok = actual is not None and variant(actual) == variant(expected)
            raw_n += 1
            raw += ok
            if row["id"] not in excluded:
                adj_n += 1
                adj += ok
            if a.baseline and before.get(row["id"]) != actual:
                old = before.get(row["id"])
                changed += 1
                was = old is not None and variant(old) == variant(expected)
                item = dict(id=row["id"], text=row["text"], expected=expected, before=old, after=actual)
                if ok and not was:
                    gained.append(item)
                elif was and not ok:
                    lost.append(item)
    summary = dict(raw=raw, raw_n=raw_n, raw_rate=raw / raw_n, adjusted=adj, adjusted_n=adj_n,
                   adjusted_rate=adj / adj_n, changed=changed, gained=len(gained), lost=len(lost))
    print(json.dumps(summary, ensure_ascii=False))
    if a.baseline:
        diff = Path(a.out).with_suffix("").with_suffix(".diff.json")
        diff.write_text(json.dumps(dict(summary=summary, gained=gained, lost=lost), ensure_ascii=False, indent=1))
        for item in lost[:15]:
            print("LOST", item["text"][:70], "|", (item["before"] or "")[:60], "->", (item["after"] or "")[:60])


if __name__ == "__main__":
    main()
