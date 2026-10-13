#!/usr/bin/env python3
"""Score the engine on the owner's TN evaluation sets (benchmark/datasets/user-tn-eval).

  python3 -I benchmark/evaluators/user_tn_eval.py run --library build/libtts_text_frontend.so \
      --resources resources [--lang ko-KR ...] [--tag NAME]
  python3 -I benchmark/evaluators/user_tn_eval.py score [--lang ...] [--tag NAME] [--show N]

Each sentence is sent in a request of its own language (speech output). A span is right when
one of its readings appears in the output after the previous span, after folding: NFKC, lower
case, whitespace and punctuation removed. Per language:
  en-US  a gold single letter (w) also matches the engine's letter name (double you);
  ja-JP  katakana = hiragana and ー = the vowel before it; a span that fails is also tried on
         the output's kana reading (pyopenjtalk, when installed), since kanji words stay kanji;
  zh-CN  〇 = 零, 圆 = 元.
Scores are given for dev and test separately. Only dev misses are written out
(benchmark/generated/user-tn-eval/<tag>/<lang>-dev-misses.json): fixes come from dev, and the
test misses are not read.
"""
import argparse
import collections
import itertools
import json
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "benchmark/datasets/user-tn-eval"
OUT = ROOT / "benchmark/generated/user-tn-eval"
LANGS = ["ko-KR", "en-US", "ja-JP", "zh-CN", "vi-VN"]
LETTERS = dict(a="ay", b="bee", c="see", d="dee", e="ee", f="ef", g="gee", h="aitch", i="eye", j="jay",
               k="kay", l="el", m="em", n="en", o="oh", p="pee", q="cue", r="ar", s="ess", t="tee",
               u="you", v="vee", w="double you", x="ex", y="why", z="zee")
VOWEL = {ch: v for row, v in (("あかさたなはまやらわがざだばぱぁゃ", "あ"), ("いきしちにひみりぎじぢびぴぃ", "い"),
                              ("うくすつぬふむゆるぐずづぶぷぅゅ", "う"), ("えけせてねへめれげぜでべぺぇ", "え"),
                              ("おこそとのほもよろをごぞどぼぽぉょ", "お")) for ch in row}


def letter_variants(reading):
    words = reading.split(" ")
    if len(words) > 12 or not any(len(w) == 1 and w in LETTERS for w in words):
        return [reading]
    options = [[w, LETTERS[w]] if len(w) == 1 and w in LETTERS else [w] for w in words]
    return [" ".join(c) for c in itertools.islice(itertools.product(*options), 64)]


def fold(s, lang):
    s = unicodedata.normalize("NFKC", s).lower()
    if lang == "ja-JP":
        s = "".join(chr(ord(c) - 0x60) if "ァ" <= c <= "ヶ" else c for c in s)
        s = "".join(VOWEL.get(s[i - 1], c) if c == "ー" and i else c for i, c in enumerate(s))
    elif lang == "zh-CN":
        s = s.replace("〇", "零").replace("圆", "元")
    return "".join(c for c in s if c == "ー" or not c.isspace() and unicodedata.category(c)[0] not in "PS")


def judge(row, output, lang):
    out, at, result = fold(output, lang), 0, []
    for target in row["targets"]:
        best = None
        for reading in target["readings"]:
            r = fold(reading, lang)
            k = out.find(r, at) if r else at
            if k >= 0 and (best is None or k < best[0]):
                best = (k, k + len(r))
        result.append(best is not None)
        if best:
            at = best[1]
    return result


def rows_of(lang):
    rows = [json.loads(line) for line in (DATA / f"user-tn-{lang}.jsonl").open(encoding="utf-8")]
    if lang == "en-US":
        for r in rows:
            for t in r["targets"]:
                t["readings"] = list(dict.fromkeys(v for x in t["readings"] for v in letter_variants(x)))
    return rows


def run(args):
    sys.path.insert(0, str(ROOT / "tools/web"))
    from native import Engine
    out = OUT / args.tag
    out.mkdir(parents=True, exist_ok=True)
    for lang in args.lang:
        engine = Engine(Path(args.library), Path(args.resources), dict(default_language=lang, output_mode="speech"))
        outputs = {}
        for r in rows_of(lang):
            try:
                outputs[r["id"]] = engine.normalize(r["text"].encode())["text"]
            except Exception as error:  # a failed request is a wrong row
                outputs[r["id"]] = "ERROR " + str(error)
        (out / f"{lang}-outputs.json").write_text(json.dumps(outputs, ensure_ascii=False, indent=1), encoding="utf-8")
    score(args)


def score(args):
    try:
        import pyopenjtalk
    except ImportError:
        pyopenjtalk = None
    out = OUT / args.tag
    summary = {}
    for lang in args.lang:
        outputs = json.loads((out / f"{lang}-outputs.json").read_text(encoding="utf-8"))
        tally = {s: collections.Counter() for s in ("dev", "test")}
        per = collections.defaultdict(lambda: [0, 0])
        misses = []
        for r in rows_of(lang):
            text = outputs[r["id"]]
            res = judge(r, text, lang)
            if lang == "ja-JP" and pyopenjtalk and not all(res):
                heard = judge(r, pyopenjtalk.g2p(text, kana=True), lang)
                res = [a or b for a, b in zip(res, heard)]
            t = tally[r["split"]]
            t["sentences"] += 1
            t["sentence_ok"] += all(res)
            t["spans"] += len(res)
            t["span_ok"] += sum(res)
            if r["split"] == "dev":
                per[r["category"]][0] += len(res)
                per[r["category"]][1] += sum(res)
                if not all(res):
                    misses.append({"id": r["id"], "category": r["category"], "text": r["text"], "output": text,
                                   "wrong": [{"written": g["written"], "readings": g["readings"][:4]}
                                             for g, ok in zip(r["targets"], res) if not ok]})
        summary[lang] = {s: dict(t) for s, t in tally.items()}
        (out / f"{lang}-dev-misses.json").write_text(json.dumps(misses, ensure_ascii=False, indent=1), encoding="utf-8")
        line = []
        for s, t in tally.items():
            line.append(f"{s} spans {t['span_ok']}/{t['spans']} ({100 * t['span_ok'] / t['spans']:.1f}%) "
                        f"sentences {100 * t['sentence_ok'] / t['sentences']:.1f}%")
        print(f"{lang}: " + " | ".join(line))
        if args.categories:
            print("   dev " + "  ".join(f"{k} {v[1]}/{v[0]}" for k, v in sorted(per.items())))
        for m in misses[:args.show]:
            print("  ", m["id"], "|", m["text"], "\n      ->", m["output"], "\n      want",
                  "; ".join(f"{w['written']}: {' / '.join(w['readings'][:2])}" for w in m["wrong"]))
    (out / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=1), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["run", "score"])
    parser.add_argument("--library", default=str(ROOT / "build/libtts_text_frontend.so"))
    parser.add_argument("--resources", default=str(ROOT / "resources"))
    parser.add_argument("--lang", nargs="+", default=LANGS, choices=LANGS)
    parser.add_argument("--tag", default="current")
    parser.add_argument("--show", type=int, default=0)
    parser.add_argument("--categories", action="store_true")
    args = parser.parse_args()
    run(args) if args.command == "run" else score(args)


if __name__ == "__main__":
    main()
