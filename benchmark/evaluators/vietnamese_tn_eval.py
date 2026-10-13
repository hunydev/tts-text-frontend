#!/usr/bin/env python3
"""Vietnamese TN evaluation on vi-tn-eval: our vi-VN pack and the raw text.

  python3 -I benchmark/evaluators/vietnamese_tn_eval.py run --set eval \
      --library build/libtts_text_frontend.so --resources resources [--system ours|ours-en|ours-ko|raw]
  python3 -I benchmark/evaluators/vietnamese_tn_eval.py score --set eval [--system ours] [--show N]

ours declares vi-VN; ours-en and ours-ko send the same text in an English or a
Korean request, where Vietnamese sentences are routed to vi-VN (latin_sentence).
A span is right when one of its readings appears in the output after the
previous span (NFC, lower-cased, spaces, hyphens and punctuation removed).
Results go to benchmark/generated/vi-tn-eval/ (local only).
"""
import argparse
import collections
import json
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SETS = {"eval": ROOT / "benchmark/datasets/vi-tn-eval/vi-tn-eval-v1.jsonl",
        "heldout": ROOT / "benchmark/datasets/vi-tn-eval/vi-tn-heldout-v1.jsonl"}
OUT = ROOT / "benchmark/generated/vi-tn-eval"
LANG = {"ours": "vi-VN", "ours-en": "en-US", "ours-ko": "ko-KR"}


def fold(s):
    s = unicodedata.normalize("NFC", s).lower()
    return "".join(c for c in s if not c.isspace() and not unicodedata.category(c)[0] in "PS")


def judge(row, output):
    out, at, spans = fold(output), 0, []
    for target in row["targets"]:
        best = None
        for reading in target["readings"]:
            r = fold(reading)
            k = out.find(r, at) if r else at
            if k >= 0 and (best is None or k < best[0]):
                best = (k, k + len(r))
        spans.append(best is not None)
        if best:
            at = best[1]
    return spans


def run(args):
    sys.path.insert(0, str(ROOT / "tools/web"))
    from native import Engine
    rows = [json.loads(line) for line in SETS[args.set].open(encoding="utf-8")]
    outputs = {}
    if args.system == "raw":
        outputs = {r["id"]: r["text"] for r in rows}
    else:
        engine = Engine(Path(args.library), Path(args.resources),
                        dict(default_language=LANG[args.system], output_mode="speech"))
        for r in rows:
            try:
                outputs[r["id"]] = engine.normalize(r["text"].encode())["text"]
            except Exception as error:  # a failed request is a wrong row
                outputs[r["id"]] = "ERROR " + str(error)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / f"{args.set}-{args.system}.json").write_text(json.dumps(outputs, ensure_ascii=False, indent=1),
                                                       encoding="utf-8")
    score(args)


def score(args):
    rows = [json.loads(line) for line in SETS[args.set].open(encoding="utf-8")]
    outputs = json.loads((OUT / f"{args.set}-{args.system}.json").read_text(encoding="utf-8"))
    spans = sentences = span_ok = sent_ok = errors = 0
    per = collections.defaultdict(lambda: [0, 0])
    misses = []
    for r in rows:
        out = outputs[r["id"]]
        errors += out.startswith("ERROR ")
        result = judge(r, out)
        spans += len(result)
        span_ok += sum(result)
        sentences += 1
        sent_ok += all(result)
        per[r["category"]][0] += 1
        per[r["category"]][1] += all(result)
        if not all(result):
            misses.append((r, out, result))
    print(f"{args.system} {args.set}: spans {span_ok}/{spans} ({100 * span_ok / spans:.1f}%)  "
          f"sentences {sent_ok}/{sentences} ({100 * sent_ok / sentences:.1f}%)  errors {errors}")
    print("  " + "  ".join(f"{k} {v[1]}/{v[0]}" for k, v in sorted(per.items())))
    for r, out, result in misses[:args.show]:
        wrong = [t["written"] + "→" + "/".join(t["readings"][:3]) for t, ok in zip(r["targets"], result) if not ok]
        print(f"- [{r['category']}] {r['text']}\n    out: {out}\n    want: {' | '.join(wrong)}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("command", choices=("run", "score"))
    p.add_argument("--set", default="eval", choices=sorted(SETS))
    p.add_argument("--system", default="ours", choices=("ours", "ours-en", "ours-ko", "raw"))
    p.add_argument("--library")
    p.add_argument("--resources")
    p.add_argument("--show", type=int, default=0)
    args = p.parse_args()
    (run if args.command == "run" else score)(args)


if __name__ == "__main__":
    main()
