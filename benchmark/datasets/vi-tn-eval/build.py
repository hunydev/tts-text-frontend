#!/usr/bin/env python3
"""Build the Vietnamese TN evaluation set from its markup sources (same markup as zh-tn-eval).

Each source line is `category<TAB>markup[<TAB>note]`. In the markup,
{written|reading} is a span the TN must read: the text keeps `written`, the
expected normalization has `reading`, and the span is scored. The reading is
hanzi (Simplified; numbers in Chinese numerals, Latin letters kept) and may list alternatives:
`a/b` (either), `[x,y]` (choose one, an empty choice allowed) and `(x)`
(optional). {written|=reading} keeps `written` in the expected text (a word the
TN must leave alone).

  python3 -I benchmark/datasets/zh-tn-eval/build.py            # src/ -> zh-tn-eval-v1.jsonl
  python3 -I benchmark/datasets/zh-tn-eval/build.py heldout    # src-heldout/ -> zh-tn-heldout-v1.jsonl
  python3 -I benchmark/datasets/zh-tn-eval/build.py heldout2   # src-heldout2/ -> zh-tn-heldout-v2.jsonl

The held-out set (2026-10-11) was written by authors who had not seen the pack
or the first set, and its readings were reviewed and frozen before any system ran.
"""

import itertools
import json
import re
import sys
import unicodedata
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPAN = re.compile(r"\{([^{}|]+)\|([^{}]*)\}")
READING = re.compile(r"^[^\d{}|\[\]()/]*$")
LIMIT = 64


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
        for combo in itertools.product(*parts):
            out.append("".join(combo))
            if len(out) > LIMIT:
                raise ValueError("too many spellings: " + reading)
    return list(dict.fromkeys(out))


def parse(line, where):
    fields = line.split("\t")
    if len(fields) not in (2, 3):
        raise ValueError(f"{where}: expected category<TAB>markup[<TAB>note]")
    category, markup = fields[0].strip(), fields[1].strip()
    note = fields[2].strip() if len(fields) == 3 else ""
    text, expected, targets, cursor = [], [], [], 0
    for m in SPAN.finditer(markup):
        plain = markup[cursor:m.start()]
        text.append(plain)
        expected.append(plain)
        written, reading = m.group(1), m.group(2)
        keep = reading.startswith("=")
        if keep:
            reading = reading[1:]
        spellings = expand(reading)
        for s in spellings:
            if not s or not READING.match(s):
                raise ValueError(f"{where}: reading {s!r} must be words without digits")
        text.append(written)
        expected.append(written if keep else spellings[0])
        targets.append(dict(written=written, readings=spellings, keep=keep))
        cursor = m.end()
    rest = markup[cursor:]
    if "{" in rest or "}" in rest or "|" in "".join(text + [rest]):
        raise ValueError(f"{where}: unbalanced markup")
    text.append(rest)
    expected.append(rest)
    if not targets:
        raise ValueError(f"{where}: no scored span")
    return dict(category=category, text="".join(text), expected="".join(expected),
                targets=targets, note=note)


SETS = {"eval": ("src", "vi-tn-eval-v1.jsonl", "vi-tn-v1"),
        "heldout": ("src-heldout", "vi-tn-heldout-v1.jsonl", "vi-tn-h1"),
        "heldout2": ("src-heldout2", "vi-tn-heldout-v2.jsonl", "vi-tn-h2"),
        "heldout3": ("src-heldout3", "vi-tn-heldout-v3.jsonl", "vi-tn-h3"),
        "heldout4": ("src-heldout4", "vi-tn-heldout-v4.jsonl", "vi-tn-h4")}


def main():
    source, output, prefix = SETS[sys.argv[1] if len(sys.argv) > 1 else "eval"]
    rows, seen, errors = [], set(), []
    for path in sorted((HERE / source).glob("*.txt")):
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip() or line.startswith("#"):
                continue
            where = f"{path.name}:{number}"
            try:
                row = parse(unicodedata.normalize("NFC", line), where)
            except ValueError as error:
                errors.append(str(error))
                continue
            if row["text"] in seen:
                errors.append(f"{where}: duplicate text {row['text']}")
                continue
            seen.add(row["text"])
            row["source"] = where
            rows.append(row)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        sys.exit(1)
    counts = {}
    for row in rows:
        counts[row["category"]] = counts.get(row["category"], 0) + 1
        row["id"] = "%s-%s-%03d" % (prefix, row["category"], counts[row["category"]])
    keys = ("id", "category", "text", "expected", "targets", "note", "source")
    out = HERE / output
    out.write_text("".join(json.dumps({k: r[k] for k in keys}, ensure_ascii=False) + "\n" for r in rows),
                   encoding="utf-8")
    print(json.dumps(dict(rows=len(rows), targets=sum(len(r["targets"]) for r in rows), categories=counts),
                     ensure_ascii=False))


if __name__ == "__main__":
    main()
