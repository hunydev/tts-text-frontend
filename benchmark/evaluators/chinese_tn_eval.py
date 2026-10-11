#!/usr/bin/env python3
"""Chinese (Simplified) TN evaluation on zh-tn-eval-v1: our zh-CN pack and other TNs.

Outputs (run each with the Python that has the system):
  ~/venv-wetext/bin/python -I benchmark/evaluators/chinese_tn_eval.py run --system wetext
  ~/venv-nemo/bin/python -I benchmark/evaluators/chinese_tn_eval.py run --system nemo
  python3 -I benchmark/evaluators/chinese_tn_eval.py run --system raw
  python3 -I benchmark/evaluators/chinese_tn_eval.py run --system ours \
      --library build/libtts_text_frontend.so --resources resources
ours declares zh-CN; ours-ko and ours-en send the same text in a Korean or an
English request, where hanzi sentences are routed to zh-CN (han_sentence).
Score every output present:
  python3 -I benchmark/evaluators/chinese_tn_eval.py score --tag NAME
Add --set heldout to run or score the held-out set.

wetext is VoxCPM2's own normalizer for Chinese. A span is right when one of its
readings appears in the output after the previous span (outputs and readings are
NFKC-folded, lower-cased, 〇 read as 零 and 圆 as 元, spaces and punctuation removed).
Results go to benchmark/generated/zh-tn-eval/ (local only).
"""
import argparse
import collections
import json
import sys
import time
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "benchmark/datasets/zh-tn-eval/zh-tn-eval-v1.jsonl"
OUT = ROOT / "benchmark/generated/zh-tn-eval"
# --set heldout: the held-out set (written without seeing the pack), its own output folder.
SETS = {"eval": (DATA, OUT),
        "heldout": (ROOT / "benchmark/datasets/zh-tn-eval/zh-tn-heldout-v1.jsonl",
                    ROOT / "benchmark/generated/zh-tn-heldout"),
        "heldout2": (ROOT / "benchmark/datasets/zh-tn-eval/zh-tn-heldout-v2.jsonl",
                     ROOT / "benchmark/generated/zh-tn-heldout2"),
        "heldout3": (ROOT / "benchmark/datasets/zh-tn-eval/zh-tn-heldout-v3.jsonl",
                     ROOT / "benchmark/generated/zh-tn-heldout3"),
        "heldout4": (ROOT / "benchmark/datasets/zh-tn-eval/zh-tn-heldout-v4.jsonl",
                     ROOT / "benchmark/generated/zh-tn-heldout4"),
        "heldout5": (ROOT / "benchmark/datasets/zh-tn-eval/zh-tn-heldout-v5.jsonl",
                     ROOT / "benchmark/generated/zh-tn-heldout5")}


def fold(s):
    # 〇 reads 零; 圆 is the formal 元 (both yuán), so a TTS reads them alike.
    s = unicodedata.normalize("NFKC", s).lower().replace("〇", "零").replace("圆", "元")
    return "".join(c for c in s if not c.isspace() and not unicodedata.category(c).startswith("P"))


def normalizer(system, args=None):
    if system in ("ours", "ours-ko", "ours-en"):
        if not args or not args.library:
            raise SystemExit("ours needs --library and --resources")
        sys.path.insert(0, str(ROOT / "tools/web"))
        from native import Engine
        language = {"ours": "zh-CN", "ours-ko": "ko-KR", "ours-en": "en-US"}[system]
        engine = Engine(Path(args.library), Path(args.resources),
                        dict(default_language=language, output_mode="speech"))
        return lambda t: engine.normalize(t.encode())["text"]
    if system == "raw":
        return lambda t: t
    if system == "wetext":
        from wetext import Normalizer
        tn = Normalizer(lang="zh", operator="tn")
        return tn.normalize
    if system == "nemo":
        from nemo_text_processing.text_normalization.normalize import Normalizer
        tn = Normalizer(input_case="cased", lang="zh", cache_dir=str(OUT / "nemo-cache"))
        return lambda t: tn.normalize(t, verbose=False)
    raise SystemExit("unknown system " + system)


def run(args):
    rows = [json.loads(l) for l in DATA.open(encoding="utf-8")]
    OUT.mkdir(parents=True, exist_ok=True)
    fn = normalizer(args.system, args)
    start = time.time()
    with (OUT / f"output-{args.system}.jsonl").open("w", encoding="utf-8") as f:
        for r in rows:
            try:
                out, err = fn(r["text"]), ""
            except Exception as e:  # a system error is scored as a miss
                out, err = "", str(e)[:200]
            f.write(json.dumps(dict(id=r["id"], output=out, error=err), ensure_ascii=False) + "\n")
    print(args.system, len(rows), "rows", round(time.time() - start, 1), "s")


def spans_ok(row, output):
    text, at, hits = fold(output), 0, []
    for t in row["targets"]:
        best = None
        for reading in t["readings"]:
            k = text.find(fold(reading), at)
            if k >= 0 and (best is None or k < best[0]):
                best = (k, k + len(fold(reading)))
        hits.append(best is not None)
        if best:
            at = best[1]
    return hits


def score(args):
    rows = [json.loads(l) for l in DATA.open(encoding="utf-8")]
    systems = {}
    for path in sorted(OUT.glob("output-*.jsonl")):
        name = path.stem[len("output-"):]
        systems[name] = {json.loads(l)["id"]: json.loads(l) for l in path.open(encoding="utf-8")}
    summary = dict(tag=args.tag, data=DATA.name, rows=len(rows), systems={}, categories={})
    detail = []
    cats = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0]))
    for name, outs in systems.items():
        spans = span_n = sentences = errors = 0
        for r in rows:
            o = outs.get(r["id"], {"output": "", "error": "missing"})
            hits = spans_ok(r, o["output"])
            spans += sum(hits)
            span_n += len(hits)
            sentences += all(hits)
            errors += bool(o.get("error"))
            c = cats[r["category"]][name]
            c[0] += sum(hits)
            c[1] += len(hits)
        summary["systems"][name] = dict(spans=spans, span_n=span_n, span_rate=round(spans / span_n, 4),
                                        sentences=sentences, sentence_rate=round(sentences / len(rows), 4),
                                        errors=errors)
    for cat, per in cats.items():
        summary["categories"][cat] = {n: dict(spans=v[0], span_n=v[1], rate=round(v[0] / v[1], 4)) for n, v in per.items()}
    for r in rows:
        detail.append(dict(id=r["id"], category=r["category"], text=r["text"], expected=r["expected"],
                           readings=[t["readings"] for t in r["targets"]],
                           **{n: dict(output=systems[n].get(r["id"], {}).get("output", ""),
                                      hits=spans_ok(r, systems[n].get(r["id"], {}).get("output", "")))
                              for n in systems}))
    (OUT / f"summary-{args.tag}.json").write_text(json.dumps(summary, ensure_ascii=False, indent=1), encoding="utf-8")
    (OUT / f"results-{args.tag}.jsonl").write_text("".join(json.dumps(d, ensure_ascii=False) + "\n" for d in detail),
                                                   encoding="utf-8")
    for n, v in summary["systems"].items():
        print(f"{n:8} spans {v['spans']}/{v['span_n']} ({100 * v['span_rate']:.1f}%)  sentences "
              f"{v['sentences']}/{len(rows)} ({100 * v['sentence_rate']:.1f}%)  errors {v['errors']}")
    names = list(summary["systems"])
    print("%-9s" % "category" + "".join("%10s" % n for n in names))
    for cat in sorted(summary["categories"], key=lambda c: -summary["categories"][c][names[0]]["span_n"]):
        print("%-9s" % cat + "".join("%9.1f%%" % (100 * summary["categories"][cat][n]["rate"]) for n in names))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--system", required=True)
    r.add_argument("--library")
    r.add_argument("--resources")
    s = sub.add_parser("score")
    s.add_argument("--tag", required=True)
    for p in (r, s):
        p.add_argument("--set", choices=sorted(SETS), default="eval")
    args = ap.parse_args()
    global DATA, OUT
    DATA, OUT = SETS[args.set]
    (run if args.cmd == "run" else score)(args)


if __name__ == "__main__":
    main()
