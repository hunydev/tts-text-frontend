#!/usr/bin/env python3
"""The project owner's real-sentence sets (v2) scored like the owner TN evaluation sets.

  python3 -I benchmark/evaluators/owner_v2_eval.py run --lang en-US --tag NAME \\
      --library build/libtts_text_frontend.so --resources resources [--request ko-KR]
  python3 -I benchmark/evaluators/owner_v2_eval.py score --lang en-US --tag NAME [--show N]

A v2 set is given by the owner as tn-eval/<lang>-v2.zip. en-US and th-TH are real sentences
(scb-mt-en-th-2020, CC BY-SA 4.0) kept local under benchmark/generated/<xx>-owner/ (the license
keeps them out of the repository); ko-KR, ja-JP, zh-CN and vi-VN are synthetic held-out sets
under benchmark/generated/owner-v2-src/. Lines are [id<TAB>]category<TAB>markup<TAB>note in the markup
of zh-tn-eval ({written|reading}, a/b, [x,y], (x)); category skip lines are dropped. Spans are
judged with the folding, policy variants and letter variants of user_tn_eval.py. Results go to
benchmark/generated/owner-v2/ (local only).
"""
import argparse
import collections
import importlib.util
import json
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCES = {"en-US": ROOT / "benchmark/generated/en-owner/en-US-v2/tn_eval_en-US_v2.tsv",
           "th-TH": ROOT / "benchmark/generated/th-owner/th-TH-v2/tn_eval_th-TH_v2.tsv",
           # synthetic held-out v2 of the other four languages (owner, 2026-10-15): no id column
           "ko-KR": ROOT / "benchmark/generated/owner-v2-src/ko-KR-v2/tn_eval_ko-KR_v2.tsv",
           "ja-JP": ROOT / "benchmark/generated/owner-v2-src/ja-JP-v2/tn_eval_ja-JP_v2.tsv",
           "zh-CN": ROOT / "benchmark/generated/owner-v2-src/zh-CN-v2/tn_eval_zh-CN_v2.tsv",
           "vi-VN": ROOT / "benchmark/generated/owner-v2-src/vi-VN-v2/tn_eval_vi-VN_v2.tsv"}
OUT = ROOT / "benchmark/generated/owner-v2"
sys.path.insert(0, str(ROOT / "benchmark/evaluators"))
import user_tn_eval as u  # noqa: E402


def markup_parser():
    spec = importlib.util.spec_from_file_location("th_build", ROOT / "benchmark/datasets/th-tn-eval/build.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.parse


def rows_of(lang):
    parse, rows = markup_parser(), []
    for number, line in enumerate(SOURCES[lang].read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        first = line.split("\t", 1)[0]
        if first.isdigit():
            rid, rest = line.split("\t", 1)
        else:
            rid, rest = f"{number:04d}", line
        if rest.split("\t", 1)[0].strip() == "skip":
            continue
        row = parse(unicodedata.normalize("NFC", rest), f"{SOURCES[lang].name}:{number}")
        row["id"] = f"{lang}-v2-{rid}"
        for t in row["targets"]:
            t["readings"] = u.policy_variants(lang, t)
            if lang == "en-US":
                t["readings"] = list(dict.fromkeys(v for x in t["readings"] for v in u.letter_variants(x)))
        rows.append(row)
    return rows


def run(args):
    sys.path.insert(0, str(ROOT / "tools/web"))
    from native import Engine
    engine = Engine(Path(args.library), Path(args.resources),
                    dict(default_language=args.request or args.lang, output_mode="speech"))
    outputs = {}
    for r in rows_of(args.lang):
        try:
            outputs[r["id"]] = engine.normalize(r["text"].encode())["text"]
        except Exception as error:  # a failed request is a wrong row
            outputs[r["id"]] = "ERROR " + str(error)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / f"{args.lang}-{args.tag}.json").write_text(json.dumps(outputs, ensure_ascii=False, indent=1),
                                                      encoding="utf-8")
    score(args)


def score(args):
    outputs = json.loads((OUT / f"{args.lang}-{args.tag}.json").read_text(encoding="utf-8"))
    try:
        import pyopenjtalk
    except ImportError:
        pyopenjtalk = None
    spans = span_ok = sentences = sentence_ok = errors = 0
    per = collections.defaultdict(lambda: [0, 0])
    misses = []
    for r in rows_of(args.lang):
        text = outputs[r["id"]]
        errors += text.startswith("ERROR ")
        res = u.judge(r, text, args.lang)
        # the same extra views as user_tn_eval.score
        if args.lang == "ja-JP" and pyopenjtalk and not all(res):
            heard = pyopenjtalk.g2p(text, kana=True)
            for view in (heard, text.replace("ー", ""), heard.replace("ー", "")):
                res = [a or b for a, b in zip(res, u.judge(r, view, args.lang))]
        if args.lang == "ko-KR" and not all(res):
            plain = {**r, "targets": [{**g, "readings": [x.replace(" 에 ", " ") for x in g["readings"]]}
                                      if "-" in g["written"] else g for g in r["targets"]]}
            res = [a or b for a, b in zip(res, u.judge(plain, text.replace("다시", " "), args.lang))]
        spans += len(res)
        span_ok += sum(res)
        sentences += 1
        sentence_ok += all(res)
        per[r["category"]][0] += len(res)
        per[r["category"]][1] += sum(res)
        if not all(res):
            misses.append({"id": r["id"], "category": r["category"], "text": r["text"], "output": text,
                           "wrong": [{"written": g["written"], "readings": g["readings"][:4]}
                                     for g, ok in zip(r["targets"], res) if not ok]})
    (OUT / f"{args.lang}-{args.tag}-misses.json").write_text(json.dumps(misses, ensure_ascii=False, indent=1),
                                                             encoding="utf-8")
    print(f"{args.lang} {args.tag}: spans {span_ok}/{spans} ({100 * span_ok / spans:.1f}%)  "
          f"sentences {sentence_ok}/{sentences} ({100 * sentence_ok / sentences:.1f}%)  errors {errors}")
    print("  " + "  ".join(f"{k} {v[1]}/{v[0]}" for k, v in sorted(per.items())))
    for m in misses[:args.show]:
        print(f"- [{m['category']}] {m['text']}\n    out: {m['output']}\n    want: "
              + " | ".join(f"{w['written']} -> {' / '.join(w['readings'][:2])}" for w in m["wrong"]))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("command", choices=("run", "score"))
    p.add_argument("--lang", required=True, choices=sorted(SOURCES))
    p.add_argument("--tag", default="current")
    p.add_argument("--request", help="the request language (default: --lang)")
    p.add_argument("--library", default=str(ROOT / "build/libtts_text_frontend.so"))
    p.add_argument("--resources", default=str(ROOT / "resources"))
    p.add_argument("--show", type=int, default=0)
    args = p.parse_args()
    (run if args.command == "run" else score)(args)


if __name__ == "__main__":
    main()
