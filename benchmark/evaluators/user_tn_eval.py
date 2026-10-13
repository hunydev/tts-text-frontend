#!/usr/bin/env python3
"""Score the engine on the owner's TN evaluation sets (benchmark/datasets/user-tn-eval).

  python3 -I benchmark/evaluators/user_tn_eval.py run --library build/libtts_text_frontend.so \
      --resources resources [--lang ko-KR ...] [--tag NAME]
  python3 -I benchmark/evaluators/user_tn_eval.py score [--lang ...] [--tag NAME] [--show N]

Each sentence is sent in a request of its own language (speech output). A span is right when
one of its readings appears in the output after the previous span, after folding: NFKC, lower
case, whitespace and punctuation removed. Per language:
  en-US  a gold single letter (w) also matches the engine's letter name (double you);
  ja-JP  katakana = hiragana and ー = the vowel before it, and ー may be left out on either side (に = にー); a span that fails is also tried on
         the output's kana reading (pyopenjtalk, when installed), since kanji words stay kanji;
  zh-CN  〇 = 零, 圆 = 元, 两百/两千/两万/两亿 = 二…, 点半 = 点三十(分);
  vi-VN  gi-ga = ghi-ga, đề-xi-ben = đê-xi-ben (spelling variants);
  ko-KR  an hour in native numbers (스물세 시 = 이십삼 시), a letter and one digit in English
         (B7 비세븐 = 비 칠), 에잇 = 에이트, and a hyphen read 다시 = 에 or nothing.
These are reading policies both sides accept (docs/HANDOFF.md, policy differences kept); per-line
widenings are in benchmark/datasets/user-tn-eval/amend.tsv.
Scores are given for dev and test separately. Only dev misses are written out
(benchmark/generated/user-tn-eval/<tag>/<lang>-dev-misses.json): fixes come from dev, and the
test misses are not read.
"""
import argparse
import collections
import itertools
import json
import re
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


KO_NATIVE = {"일": "한", "이": "두", "삼": "세", "사": "네", "오": "다섯", "육": "여섯", "칠": "일곱", "팔": "여덟",
             "구": "아홉", "십": "열", "십일": "열한", "십이": "열두", "십삼": "열세", "십사": "열네", "십오": "열다섯",
             "십육": "열여섯", "십칠": "열일곱", "십팔": "열여덟", "십구": "열아홉", "이십": "스무", "이십일": "스물한",
             "이십이": "스물두", "이십삼": "스물세", "이십사": "스물네"}
KO_HOUR = re.compile(r"(?<![가-힣])(이십[일이삼사]?|십[일이삼사오육칠팔구]?|[일이삼사오육칠팔구]) 시")
KO_LETTER = dict(A="에이", B="비", C="씨", D="디", E="이", F="에프", G="지", H="에이치", I="아이", J="제이",
                 K="케이", L="엘", M="엠", N="엔", O="오", P="피", Q="큐", R="알", S="에스", T="티", U="유",
                 V="브이", W="더블유", X="엑스", Y="와이", Z="제트")
KO_SINO = "영일이삼사오육칠팔구"
KO_ENGLISH = ["제로", "원", "투", "쓰리", "포", "파이브", "식스", "세븐", "에잇", "나인"]


def policy_variants(lang, target):
    """Readings a reading policy accepts besides the gold ones (see the module docstring)."""
    readings, out = target["readings"], list(target["readings"])
    if lang == "ko-KR":
        out += [KO_HOUR.sub(lambda m: KO_NATIVE[m.group(1)] + " 시", r) for r in readings]
        m = re.match(r"([A-Z])([0-9])(?![0-9])", target["written"])
        if m:
            sino = f"{KO_LETTER[m.group(1)]} {KO_SINO[int(m.group(2))]}"
            english = KO_LETTER[m.group(1)] + KO_ENGLISH[int(m.group(2))]
            out += [english + r[len(sino):] for r in readings if r.startswith(sino)]
    elif lang == "ja-JP":
        out += [r.replace("ー", "") for r in readings]
    elif lang == "zh-CN":
        out += [r.replace("点半", "点三十分") for r in readings] + [r.replace("点半", "点三十") for r in readings]
    return list(dict.fromkeys(out))


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
        s = re.sub("两(?=[百千万亿])", "二", s)
    elif lang == "ko-KR":
        s = s.replace("에이트", "에잇")
    s = "".join(c for c in s if c == "ー" or not c.isspace() and unicodedata.category(c)[0] not in "PS")
    if lang == "vi-VN":
        s = s.replace("ghiga", "giga").replace("đêxiben", "đềxiben")
    return s


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
    for r in rows:
        for t in r["targets"]:
            t["readings"] = policy_variants(lang, t)
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
                heard = pyopenjtalk.g2p(text, kana=True)
                # each view also without its long-vowel marks (ごー = ご)
                for view in (heard, text.replace("ー", ""), heard.replace("ー", "")):
                    res = [a or b for a, b in zip(res, judge(r, view, lang))]
            if lang == "ko-KR" and not all(res):
                # a hyphen read 다시 (Korean separator policy) = the gold 에 or nothing
                plain = {**r, "targets": [{**g, "readings": [x.replace(" 에 ", " ") for x in g["readings"]]}
                                          if "-" in g["written"] else g for g in r["targets"]]}
                res = [a or b for a, b in zip(res, judge(plain, text.replace("다시", " "), lang))]
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
