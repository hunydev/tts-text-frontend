#!/usr/bin/env python3
"""Build the Japanese TN evaluation set from its markup sources.

Each source line is `category<TAB>markup[<TAB>note]`. In the markup,
{written|reading} is a span the TN must read: the text keeps `written`, the
expected normalization has `reading`, and the span is scored. The reading is
kana (hiragana numbers, katakana letters and units) and may list alternatives:
`a/b` (either), `[x,y]` (choose one, an empty choice allowed) and `(x)`
(optional). {written|=reading} keeps `written` in the expected text (a word the
TN must leave alone) and still scores its reading through the reader.

  python3 -I benchmark/datasets/ja-tn-eval/build.py   # writes ja-tn-eval-v1.jsonl
"""

import itertools
import json
import re
import sys
import unicodedata
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPAN = re.compile(r"\{([^{}|]+)\|([^{}]*)\}")
KANA = re.compile(r"^[ぁ-ゖァ-ヺー、 ]*$")
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
            if not s or not KANA.match(s):
                raise ValueError(f"{where}: reading {s!r} must be kana")
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


def main():
    rows, seen, errors = [], set(), []
    for path in sorted((HERE / "src").glob("*.txt")):
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
        row["id"] = "ja-tn-v1-%s-%03d" % (row["category"], counts[row["category"]])
    keys = ("id", "category", "text", "expected", "targets", "note", "source")
    out = HERE / "ja-tn-eval-v1.jsonl"
    out.write_text("".join(json.dumps({k: r[k] for k in keys}, ensure_ascii=False) + "\n" for r in rows),
                   encoding="utf-8")
    print(json.dumps(dict(rows=len(rows), targets=sum(len(r["targets"]) for r in rows), categories=counts),
                     ensure_ascii=False))


if __name__ == "__main__":
    main()
