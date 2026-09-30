#!/usr/bin/env python3
"""Select a representative, low-duplication subset of a Korean release import.

    select_representative.py IMPORT_DIR OUT_DIR [--per-category 200] [--reviews FILE] [--seed 20261001]

The AI Hub scripts repeat a few sentence frames with different numbers (주민번호
610030-4984446인 …). Per category this keeps sentences that are scorable, carry
no structural flag, are not in a conflicting input group and are not listed in
the reference-error review file, then
  1. keeps one sentence per input group and per masked frame (digits -> 0,
     Latin runs -> A, whitespace collapsed);
  2. picks greedily the sentence least similar to those already chosen
     (character 3-grams of the masked frame, Jaccard), with a bonus for a
     theme or span route not yet covered.
OUT_DIR is a complete import (records.jsonl, manifest.json) the Korean
evaluator reads unchanged, plus selection.json with the method and counts.
The source records are not redistributable: keep OUT_DIR under the ignored
benchmark/generated/.
"""
import argparse
import hashlib
import json
import random
import re
from collections import defaultdict
from pathlib import Path

DIGITS = re.compile(r"\d+(?:[.,]\d+)*")
LATIN = re.compile(r"[A-Za-z]+")
SPACE = re.compile(r"\s+")


def frame(text):
    return SPACE.sub(" ", LATIN.sub("A", DIGITS.sub("0", text))).strip()


def grams(text):
    t = frame(text).replace(" ", "")
    return {t[i:i + 3] for i in range(max(1, len(t) - 2))}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def pick(pool, n, rng):
    """Greedy max-diversity selection over one category's candidates."""
    if len(pool) <= n:
        return pool
    if len(pool) > 4000:
        pool = rng.sample(pool, 4000)
    feats = [grams(r["text"]) for r in pool]
    labels = [set(r["themes"]) | {s["route"] for s in r["spans"]} for r in pool]
    closest = [0.0] * len(pool)
    covered = set()
    chosen, taken = [], [False] * len(pool)
    first = max(range(len(pool)), key=lambda i: (len(labels[i]), -len(pool[i]["text"])))
    for _ in range(n):
        if chosen:
            best = max((i for i in range(len(pool)) if not taken[i]),
                       key=lambda i: (1 - closest[i]) + 0.5 * bool(labels[i] - covered))
        else:
            best = first
        taken[best] = True
        chosen.append(pool[best])
        covered |= labels[best]
        f = feats[best]
        for i in range(len(pool)):
            if not taken[i]:
                g = feats[i]
                sim = len(f & g) / len(f | g) if f or g else 1.0
                if sim > closest[i]:
                    closest[i] = sim
    return chosen


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("import_dir", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--per-category", type=int, default=200)
    ap.add_argument("--reviews", type=Path, help="reference-error review file; listed ids are left out")
    ap.add_argument("--seed", type=int, default=20261001)
    args = ap.parse_args()
    if args.out.exists():
        raise SystemExit("output exists; create a new selection directory")
    manifest = json.loads((args.import_dir / "manifest.json").read_text(encoding="utf-8"))
    if manifest["records_sha256"] != sha256(args.import_dir / "records.jsonl"):
        raise SystemExit("modified import")
    excluded = set()
    if args.reviews:
        excluded = {json.loads(l)["id"] for l in args.reviews.read_text(encoding="utf-8").splitlines() if l.strip()}
    conflicts = set(manifest["conflicting_input_groups"])
    rng = random.Random(args.seed)
    by_category = defaultdict(list)
    seen_groups, seen_frames = set(), set()
    counts = defaultdict(lambda: dict(records=0, eligible=0, distinct=0, selected=0))
    with (args.import_dir / "records.jsonl").open(encoding="utf-8") as f:
        for line in f:
            r = json.loads(line)
            c = counts[r["category_en"]]
            c["records"] += 1
            if (r["expected"] is None or r["flags"] or r["input_group"] in conflicts or r["id"] in excluded):
                continue
            c["eligible"] += 1
            key = (r["category_en"], frame(r["text"]))
            if r["input_group"] in seen_groups or key in seen_frames:
                continue
            seen_groups.add(r["input_group"])
            seen_frames.add(key)
            c["distinct"] += 1
            by_category[r["category_en"]].append(r)
    selected = []
    for cat in sorted(by_category):
        chosen = pick(by_category[cat], args.per_category, rng)
        counts[cat]["selected"] = len(chosen)
        selected.extend(sorted(chosen, key=lambda r: r["id"]))
    args.out.mkdir(parents=True)
    records = args.out / "records.jsonl"
    records.write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in selected), encoding="utf-8")
    (args.out / "quarantine.jsonl").write_text("", encoding="utf-8")
    method = dict(per_category=args.per_category, seed=args.seed, reviews=str(args.reviews) if args.reviews else None,
                  reviews_sha256=sha256(args.reviews) if args.reviews else None,
                  rules=["scorable, unflagged, no conflicting input group, not a listed reference error",
                         "one per input group and per masked frame (digits 0, Latin A)",
                         "greedy max-diversity on masked 3-grams (Jaccard) + theme/route coverage bonus"],
                  selector_sha256=sha256(Path(__file__)))
    ids = [r["id"] for r in selected]
    sub = dict(manifest)
    sub["counts"] = dict(manifest["counts"], imported=len(selected),
                         flagged_records=0, quarantined=0)
    sub["conflicting_input_groups"] = []
    sub["duplicate_input_records"] = 0
    sub["unique_input_groups"] = len(selected)
    sub["records_sha256"] = sha256(records)
    sub["selection"] = dict(method, source_import_id=manifest["import_id"],
                            categories={k: v for k, v in sorted(counts.items())})
    sub["import_id"] = hashlib.sha256(json.dumps(dict(source=manifest["import_id"], ids=ids, method=method),
                                                 sort_keys=True).encode()).hexdigest()
    (args.out / "manifest.json").write_text(json.dumps(sub, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (args.out / "selection.json").write_text(json.dumps(dict(sub["selection"], ids=ids), ensure_ascii=False,
                                                        indent=2) + "\n", encoding="utf-8")
    total = {k: sum(v[k] for v in counts.values()) for k in ("records", "eligible", "distinct", "selected")}
    print(json.dumps(total))
    for k, v in sorted(counts.items()):
        print("  %-24s records %6d eligible %6d distinct frames %6d selected %4d" % (
            k, v["records"], v["eligible"], v["distinct"], v["selected"]))


if __name__ == "__main__":
    main()
