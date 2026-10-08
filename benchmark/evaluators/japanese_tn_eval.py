#!/usr/bin/env python3
"""Japanese TN against the in-house evaluation set (benchmark/datasets/ja-tn-eval).

The set marks every span a TN must read with its kana reading(s). Systems write
numbers differently (kanji numerals, hiragana), so each output is read by the
same reader (pyopenjtalk) as in japanese_jsut.py, and a span is correct when one
of its readings appears in the reading, in order after the previous span:

  raw / wetext / nemo / ours / ours-kata   as in japanese_jsut.py
  direct   ours without the reader: the span reading must appear in our own
           output (kana compared, kanji dropped); words marked to keep must stay
           as written. This separates our TN errors from reader errors.

A sentence is correct when all its spans are. Both sides are canonicalized as in
japanese_jsut.py; a digit read with a lengthened vowel (にー, ごー in phone
numbers) matches the plain reading.

  ~/venv-nemo/bin/python -I benchmark/evaluators/japanese_tn_eval.py \\
      --library build/libtts_text_frontend.so --resources resources --tag NAME
"""

import argparse
import json
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from japanese_jsut import canonical as jsut_canonical, systems  # noqa: E402


def canonical(text):
    """japanese_jsut.canonical, and スィ as シ (the reader writes ピースィー for PC)."""
    return jsut_canonical(text).replace("スィ", "シ")


def pattern(reading):
    """A reading as a regex: ニ and ゴ may carry a long vowel (にー, ごー)."""
    text = canonical(reading)
    out, i = [], 0
    # The long-vowel rule joins across words (マデ + イチジ → マデーチジ).
    if text[:1] in ("イ", "ウ", "オ"):
        out.append("(?:%s|ー)" % text[0])
        i = 1
    while i < len(text):
        ch = text[i]
        if ch in "ニゴ":
            out.append(ch + "ー?")
            i += 2 if text[i + 1:i + 2] == "ー" else 1
        else:
            out.append(re.escape(ch))
            i += 1
    return re.compile("".join(out))


def score(spoken, targets, compiled):
    cursor, marks = 0, []
    for target, patterns in zip(targets, compiled):
        best = None
        for p in patterns:
            m = p.search(spoken, cursor)
            if m and (best is None or m.start() < best.start()):
                best = m
        marks.append(best is not None)
        if best is not None:
            cursor = best.end()
    return marks


def direct(output, targets, compiled):
    spoken, cursor, marks = canonical(output), 0, []
    for target, patterns in zip(targets, compiled):
        if target["keep"]:
            marks.append(target["written"] in output)
            continue
        best = None
        for p in patterns:
            m = p.search(spoken, cursor)
            if m and (best is None or m.start() < best.start()):
                best = m
        marks.append(best is not None)
        if best is not None:
            cursor = best.end()
    return marks


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True)
    parser.add_argument("--resources", required=True)
    parser.add_argument("--data", default=str(ROOT / "benchmark/datasets/ja-tn-eval/ja-tn-eval-v1.jsonl"))
    parser.add_argument("--tag", required=True)
    parser.add_argument("--out-dir", default=str(Path.home() / "tts-text-frontend/benchmark/generated/ja-tn-eval"))
    args = parser.parse_args()
    rows = [json.loads(line) for line in Path(args.data).read_text(encoding="utf-8").splitlines() if line]
    found, reader, engine = systems(args)
    started = time.time()
    results = []
    for r in rows:
        compiled = [[pattern(s) for s in t["readings"]] for t in r["targets"]]
        item = dict(id=r["id"], category=r["category"], text=r["text"], expected=r["expected"],
                    targets=[t["written"] for t in r["targets"]])
        for name, run in found.items():
            try:
                normalized = run(r["text"])
                spoken = canonical(reader(normalized))
                marks = score(spoken, r["targets"], compiled)
                item[name] = dict(normalized=normalized, kana=spoken, marks=marks)
            except Exception as error:
                item[name] = dict(error=str(error)[:200], marks=[False] * len(r["targets"]))
            if name == "ours" and "normalized" in item[name]:
                item["direct"] = dict(normalized=item[name]["normalized"],
                                      marks=direct(item[name]["normalized"], r["targets"], compiled))
        if "direct" not in item:
            item["direct"] = dict(marks=[False] * len(r["targets"]))
        results.append(item)
    engine.close()
    names = list(found) + ["direct"]
    summary = dict(tag=args.tag, data=str(Path(args.data).name), reader="pyopenjtalk g2p(kana=True)",
                   rows=len(rows), seconds=round(time.time() - started, 1), systems={}, categories={})

    def block(chosen):
        out = {}
        for name in names:
            marks = [m for r in chosen for m in r[name]["marks"]]
            out[name] = dict(spans=sum(marks), span_n=len(marks), span_rate=round(sum(marks) / max(1, len(marks)), 4),
                             sentences=sum(all(r[name]["marks"]) for r in chosen),
                             sentence_rate=round(sum(all(r[name]["marks"]) for r in chosen) / max(1, len(chosen)), 4),
                             errors=sum("error" in r[name] for r in chosen))
        return out
    summary["systems"] = block(results)
    for category in dict.fromkeys(r["category"] for r in results):
        chosen = [r for r in results if r["category"] == category]
        summary["categories"][category] = dict(n=len(chosen), **block(chosen))
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / f"results-{args.tag}.jsonl").write_text(
        "".join(json.dumps(r, ensure_ascii=False) + "\n" for r in results), encoding="utf-8")
    (out / f"summary-{args.tag}.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                                                   encoding="utf-8")
    line = " | ".join("%s %d/%d (%.1f%%) sent %.1f%%" % (n, v["spans"], v["span_n"], 100 * v["span_rate"],
                                                         100 * v["sentence_rate"])
                      for n, v in summary["systems"].items())
    print(line)


if __name__ == "__main__":
    main()
