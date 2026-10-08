#!/usr/bin/env python3
"""Japanese TN against JSUT BASIC5000 readings (sarulab-speech/jsut-label, CC-BY-SA 4.0).

No public Japanese set writes numbers out in hiragana, so every system is read
by the same reader (pyopenjtalk) and compared with the manually annotated kana
of JSUT (kana_level2). The reader's own kanji errors are shared by all systems,
so differences between systems come from their number handling:

  raw      pyopenjtalk on the original text (its NJD expands digits itself)
  wetext   wetext ja TN (kanji numerals), then pyopenjtalk
  nemo     NVIDIA nemo_text_processing ja TN (kanji numerals) on the text with
           fullwidth ASCII folded (it skips fullwidth digits), then pyopenjtalk
  ours     this frontend, ja-JP speech (hiragana numbers), then pyopenjtalk
  ours-kata  the same with our number spans in katakana: the reader segments a
           katakana run as one word, while hiragana numbers next to text can be
           split (はちごう read as particle は), so this shows reader effects

Number spans (TN only, no reader): every span of ours whose source has a digit
is correct when its reading (canonical) is found in the reference reading, in
order after the previous span. This isolates our number readings from the
reader's kanji errors.

Both sides are canonicalized: NFKC, katakana, long vowels as ー, punctuation and
spaces dropped. Run with a Python that has pyopenjtalk(-plus) and wetext, e.g.
  ~/venv-nemo/bin/python -I benchmark/evaluators/japanese_jsut.py \\
      --library build/libtts_text_frontend.so --resources resources --tag NAME
Data: benchmark/generated/ja-jsut/basic5000.yaml (local only, never committed).
"""

import argparse
import json
import re
import sys
import time
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/web"))

DIGIT = re.compile(r"[0-9０-９]")
KANJI_NUMERAL = re.compile(r"[〇一二三四五六七八九十百千万億]")
O_ROW = set("オコソトノホモヨロゴゾドボポョォ")
U_ROW = set("ウクスツヌフムユルグズヅブプュゥ")
E_ROW = set("エケセテネヘメレゲゼデベペェ")


def load(path):
    rows, current = [], None
    for line in path.read_text(encoding="utf-8").splitlines():
        if re.match(r"^BASIC5000_\d+:\s*$", line):
            current = dict(id=line.strip().rstrip(":"))
            rows.append(current)
            continue
        m = re.match(r"^\s+(text_level2|kana_level2):\s*(.*)$", line)
        if m and current is not None:
            current[m.group(1)] = m.group(2).strip()
    return [r for r in rows if r.get("text_level2") and r.get("kana_level2")]


def canonical(text):
    text = unicodedata.normalize("NFKC", text)
    out = []
    for ch in text:
        cp = ord(ch)
        if 0x3041 <= cp <= 0x3096:
            ch = chr(cp + 0x60)
        if ("ァ" <= ch <= "ヺ") or ch == "ー":
            out.append(ch)
    s = "".join(out)
    # Long vowels: オウ/ヨウ/トオ → オー, ウウ → ウー, エイ → エー (both sides alike);
    # ジュッ and ジッ (じゅっぷん/じっぷん) are one reading.
    res = []
    for ch in s:
        prev = res[-1] if res else ""
        if (ch == "ウ" and (prev in O_ROW or prev in U_ROW)) or (ch == "オ" and prev in O_ROW):
            ch = "ー"
        elif ch == "イ" and prev in E_ROW:
            ch = "ー"
        res.append(ch)
    return "".join(res).replace("ジュッ", "ジッ")


def distance(a, b):
    previous = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        current = [i]
        for j, cb in enumerate(b, 1):
            current.append(min(previous[j] + 1, current[j - 1] + 1, previous[j - 1] + (ca != cb)))
        previous = current
    return previous[-1]


def systems(args):
    import pyopenjtalk
    found = {"raw": lambda text: text}
    try:
        from wetext import Normalizer
        wetext = Normalizer(lang="ja", operator="tn")
        found["wetext"] = wetext.normalize
    except Exception as error:  # optional comparison system
        print("wetext unavailable:", error, file=sys.stderr)
    try:
        from nemo_text_processing.text_normalization.normalize import Normalizer as NemoNormalizer
        nemo = NemoNormalizer(input_case="cased", lang="ja",
                              cache_dir=str(Path.home() / "ja-eval/nemo-cache"))
        # NeMo does not read fullwidth digits (JSUT writes them), so it gets the
        # text with fullwidth ASCII folded; the other systems read it as written.
        fold = str.maketrans({chr(c): chr(c - 0xFEE0) for c in range(0xFF01, 0xFF5F)})
        found["nemo"] = lambda text: nemo.normalize(text.translate(fold))
    except Exception as error:  # optional comparison system
        print("nemo unavailable:", error, file=sys.stderr)
    from native import Engine
    engine = Engine(Path(args.library), Path(args.resources),
                    dict(default_language="ja-JP", output_mode="speech"))
    found["ours"] = lambda text: engine.normalize(text.encode())["text"]

    def kata(text):
        result = engine.normalize(text.encode())
        raw, source = bytearray(result["text"].encode()), text.encode()
        for span in sorted(result["spans"], key=lambda x: -x["begin"]):
            if DIGIT.search(source[span["input_begin"]:span["input_end"]].decode(errors="ignore")):
                piece = raw[span["begin"]:span["end"]].decode()
                piece = "".join(chr(ord(c) + 0x60) if 0x3041 <= ord(c) <= 0x3096 else c for c in piece)
                raw[span["begin"]:span["end"]] = piece.encode()
        return raw.decode()
    found["ours-kata"] = kata
    reader = lambda text: pyopenjtalk.g2p(text, kana=True)
    return found, reader, engine


def number_spans(engine, text, reference):
    """Our digit spans and whether each reading appears, in order, in the reference."""
    result = engine.normalize(text.encode())
    raw, source = result["text"].encode(), text.encode()
    cursor, spans = 0, []
    for span in result["spans"]:
        written = source[span["input_begin"]:span["input_end"]].decode(errors="ignore")
        if not DIGIT.search(written):
            continue
        reading = canonical(raw[span["begin"]:span["end"]].decode())
        at = reference.find(reading, cursor) if reading else -1
        # The long-vowel rule joins across words (マデ + イチジ → マデーチジ).
        if at < 0 and reading[:1] in ("イ", "ウ", "オ"):
            at = reference.find("ー" + reading[1:], cursor)
        spans.append(dict(written=written, reading=reading, ok=at >= 0))
        if at >= 0:
            cursor = at + len(reading)
    return spans


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True)
    parser.add_argument("--resources", required=True)
    here = ROOT / "benchmark/generated/ja-jsut/basic5000.yaml"
    if not here.exists():
        here = Path.home() / "tts-text-frontend/benchmark/generated/ja-jsut/basic5000.yaml"
    parser.add_argument("--data", default=str(here))
    parser.add_argument("--tag", required=True)
    parser.add_argument("--out-dir", default=str(Path.home() / "tts-text-frontend/benchmark/generated/ja-jsut"))
    parser.add_argument("--all", action="store_true", help="score all 5000 sentences, not only number sentences")
    args = parser.parse_args()
    rows = load(Path(args.data))
    for r in rows:
        text = r["text_level2"]
        r["subset"] = "digits" if DIGIT.search(text) else ("kanji-numeral" if KANJI_NUMERAL.search(text) else "other")
    if not args.all:
        rows = [r for r in rows if r["subset"] != "other"]
    review = json.loads((ROOT / "benchmark/manifests/ja-jsut-review.json").read_text(encoding="utf-8"))
    reviewed = review["cases"]
    found, reader, engine = systems(args)
    started = time.time()
    results = []
    for r in rows:
        ref = canonical(r["kana_level2"])
        item = dict(id=r["id"], subset=r["subset"], text=r["text_level2"], reference=r["kana_level2"],
                    review=reviewed.get(r["id"], {}).get("class", ""))
        for name, run in found.items():
            try:
                normalized = run(r["text_level2"])
                spoken = canonical(reader(normalized))
                item[name] = dict(normalized=normalized, kana=spoken, exact=spoken == ref,
                                  edits=distance(spoken, ref), length=len(ref))
            except Exception as error:
                item[name] = dict(error=str(error)[:200], exact=False, edits=len(ref), length=len(ref))
        item["spans"] = number_spans(engine, r["text_level2"], ref)
        results.append(item)
    engine.close()
    summary = dict(tag=args.tag, source="JSUT BASIC5000 kana_level2 (jsut-label, CC-BY-SA 4.0)",
                   reader="pyopenjtalk g2p(kana=True)", seconds=round(time.time() - started, 1), subsets={})
    summary["review"] = dict(path="benchmark/manifests/ja-jsut-review.json", policy=review["policy"],
                             cases=len(reviewed))
    for subset in sorted({r["subset"] for r in results}) + ["all", "digits-adjusted"]:
        if subset == "digits-adjusted":
            chosen = [r for r in results if r["subset"] == "digits" and not r["review"]]
        else:
            chosen = [r for r in results if subset == "all" or r["subset"] == subset]
        block = dict(n=len(chosen))
        for name in found:
            exact = sum(r[name]["exact"] for r in chosen)
            edits = sum(r[name]["edits"] for r in chosen)
            length = sum(r[name]["length"] for r in chosen) or 1
            block[name] = dict(exact=exact, exact_rate=round(exact / max(1, len(chosen)), 4),
                               cer=round(edits / length, 4),
                               errors=sum("error" in r[name] for r in chosen))
        spans = [s for r in chosen for s in r["spans"]]
        block["number_spans"] = dict(n=len(spans), ok=sum(s["ok"] for s in spans),
                                     rate=round(sum(s["ok"] for s in spans) / max(1, len(spans)), 4))
        summary["subsets"][subset] = block
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / f"results-{args.tag}.jsonl").write_text(
        "".join(json.dumps(r, ensure_ascii=False) + "\n" for r in results), encoding="utf-8")
    (out / f"summary-{args.tag}.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                                                   encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False))


if __name__ == "__main__":
    main()
