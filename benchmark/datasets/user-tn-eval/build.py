#!/usr/bin/env python3
"""Build the owner's TN evaluation sets (ko-KR, en-US, ja-JP, zh-CN, vi-VN, th-TH; 1,000 lines each).

  python3 -I benchmark/datasets/user-tn-eval/build.py     # src/<lang>.tsv + amend.tsv -> user-tn-<lang>.jsonl

src/<lang>.tsv is the owner's file as delivered (2026-10-13), never edited. Each line is
`category<TAB>markup<TAB>note`; {written|reading} is a scored span and the reading may list
alternatives: `a/b` (either), `[x,y]` (choose one, an empty choice allowed), `(x)` (optional).

amend.tsv widens a reading only where the owner's policy accepts another reading
(lang<TAB>line<TAB>was<TAB>now<TAB>why); `was` must still match the source line, so the
original stays on record. Readings are never changed to fit an output.

Each language is split once, stratified by category with a fixed seed, into dev (700) and
test (300). Fixes are made from dev; test is scored but its misses are not read.
"""

import collections
import itertools
import json
import random
import re
import unicodedata
from pathlib import Path

HERE = Path(__file__).resolve().parent
LANGS = ["ko-KR", "en-US", "ja-JP", "zh-CN", "vi-VN", "th-TH"]
SPAN = re.compile(r"\{([^{}|]+)\|([^{}]*)\}")
SEED = 20261014
TEST = 300
LIMIT = 256


def expand(reading):
    """All spellings of one reading: a/b, [x,y], (x)."""
    out = []
    for alternative in reading.split("/"):
        parts, i = [], 0
        while i < len(alternative):
            ch = alternative[i]
            if ch in "[(":
                close = alternative.index("]" if ch == "[" else ")", i)
                body = alternative[i + 1:close]
                parts.append(body.split(",") if ch == "[" else [body, ""])
                i = close + 1
            else:
                parts.append([ch])
                i += 1
        for combo in itertools.islice(itertools.product(*parts), LIMIT):
            out.append(" ".join("".join(combo).split()))
    return list(dict.fromkeys(out))


def amendments():
    out = {}
    for line in (HERE / "amend.tsv").open(encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        lang, number, was, now, why = line.rstrip("\n").split("\t")
        out[lang, int(number)] = (was, now, why)
    return out


def split(rows):
    """Stratified by category: rank each row by its position in its shuffled category."""
    rng = random.Random(SEED)
    by_category = collections.defaultdict(list)
    for r in rows:
        by_category[r["category"]].append(r)
    ranked = []
    for category in sorted(by_category):
        group = by_category[category]
        rng.shuffle(group)
        ranked += [((i + rng.random()) / len(group), r["line"]) for i, r in enumerate(group)]
    test = {line for _, line in sorted(ranked)[:TEST]}
    for r in rows:
        r["split"] = "test" if r["line"] in test else "dev"


def build(lang, amend):
    rows, used = [], set()
    for number, raw in enumerate((HERE / "src" / f"{lang}.tsv").open(encoding="utf-8"), 1):
        raw = raw.rstrip("\n")
        if not raw.strip() or raw.startswith("#"):
            continue
        category, markup = raw.split("\t")[:2]
        if (lang, number) in amend:
            was, now, _ = amend[lang, number]
            assert was in markup, f"{lang}:{number}: amend.tsv `was` no longer matches"
            markup = markup.replace(was, now)
            used.add((lang, number))
        text, targets, cursor = [], [], 0
        for m in SPAN.finditer(markup):
            text += [markup[cursor:m.start()], m.group(1)]
            targets.append({"written": m.group(1), "readings": expand(m.group(2))})
            cursor = m.end()
        text.append(markup[cursor:])
        rows.append({"id": f"{lang}-{number:04d}", "line": number, "category": category,
                     "text": unicodedata.normalize("NFC", "".join(text)), "targets": targets})
    split(rows)
    with (HERE / f"user-tn-{lang}.jsonl").open("w", encoding="utf-8") as f:
        for r in rows:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")
    counts = collections.Counter(r["split"] for r in rows)
    print(f"{lang}: {len(rows)} rows, dev {counts['dev']}, test {counts['test']}, "
          f"spans {sum(len(r['targets']) for r in rows)}")
    return used


if __name__ == "__main__":
    amend = amendments()
    used = set()
    for lang in LANGS:
        used |= build(lang, amend)
    assert used == set(amend), f"unused amendments: {sorted(set(amend) - used)}"
