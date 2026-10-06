#!/usr/bin/env python3
"""English TN against VoxCPM2's built-in normalizer (wetext), on the same sentences and scorer.

VoxCPM2 (github.com/OpenBMB/VoxCPM, src/voxcpm/utils/text_normalize.py) normalizes English
text with clean_text (markdown, emoji, newlines, curly quotes), then
wetext.Normalizer(lang="en", operator="tn"), then spells leftover digits with inflect.
That pipeline is reproduced here as written there. Both systems are scored with
english_sentences.level (exact <= compact <= loose <= variant) under the same English
scope, so the numbers are directly comparable with the /en-quality reports.

Run with a Python that has wetext, inflect and regex (a separate venv):
    ~/venv-wetext/bin/python benchmark/evaluators/compare_wetext.py \\
      --corpus benchmark/generated/en-eval/nemo-eval-acacc21.jsonl ... \\
      --library build/libtts_text_frontend.so --resources resources --out DIR
"""
import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT), str(ROOT / "tools/web")]
from benchmark.common import read_records  # noqa: E402
from benchmark.english_scope import foreign_scripts  # noqa: E402
from benchmark.evaluators.english_sentences import LEVELS, level  # noqa: E402


class VoxCPM2English:
    """The English branch of VoxCPM2's TextNormalizer.normalize (split=False)."""

    def __init__(self):
        import inflect
        import regex
        from wetext import Normalizer
        self.tn = Normalizer(lang="en", operator="tn")
        self.inflect = inflect.engine()
        self.emoji = regex.compile(r"\p{Emoji_Presentation}|\p{Emoji}️", flags=regex.UNICODE)

    @staticmethod
    def clean_markdown(md_text):
        md_text = re.sub(r"```.*?```", "", md_text, flags=re.DOTALL)
        md_text = re.sub(r"`[^`]*`", "", md_text)
        md_text = re.sub(r"!\[[^\]]*\]\([^\)]+\)", "", md_text)
        md_text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", md_text)
        md_text = re.sub(r"^(\s*)-\s+", r"\1", md_text, flags=re.MULTILINE)
        md_text = re.sub(r"<[^>]+>", "", md_text)
        md_text = re.sub(r"^#{1,6}\s*", "", md_text, flags=re.MULTILINE)
        md_text = re.sub(r"\n\s*\n", "\n", md_text)
        return md_text.strip()

    def clean_text(self, text):
        text = self.clean_markdown(text)
        text = self.emoji.sub("", text)
        text = text.replace("\n", " ").replace("\t", " ")
        return text.replace("“", '"').replace("”", '"')

    def spell_out_number(self, text):
        new_text, st = [], None
        for i, c in enumerate(text):
            if not c.isdigit():
                if st is not None:
                    new_text.append(self.inflect.number_to_words(text[st:i]))
                    st = None
                new_text.append(c)
            elif st is None:
                st = i
        if st is not None and st < len(text):
            new_text.append(self.inflect.number_to_words(text[st:]))
        return "".join(new_text)

    def normalize(self, text):
        return self.spell_out_number(self.tn.normalize(self.clean_text(text)))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--corpus", type=Path, action="append", required=True)
    p.add_argument("--library", type=Path, required=True)
    p.add_argument("--resources", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    from native import Engine
    ours = Engine(a.library.resolve(), a.resources.resolve(), dict(output_mode="speech", default_language="en-US"))
    theirs = VoxCPM2English()
    a.out.mkdir(parents=True, exist_ok=False)
    tally = defaultdict(lambda: defaultdict(int))
    with (a.out / "results.jsonl").open("w", encoding="utf-8") as results:
        for corpus in a.corpus:
            for row in read_records(corpus.resolve(), 0):
                if foreign_scripts(row["text"]):
                    continue
                refs = list(dict.fromkeys(r for r in [row.get("expected")] + list(row.get("accepted_references") or []) if r))
                if not refs:
                    continue
                outputs = {}
                for name, fn in (("ours", lambda t: ours.normalize(t.encode())["text"]), ("wetext", theirs.normalize)):
                    try:
                        outputs[name] = fn(row["text"])
                    except Exception as exc:  # a crash is a miss, recorded
                        outputs[name] = None
                        outputs[name + "_error"] = type(exc).__name__
                levels = {name: level(outputs[name], refs) for name in ("ours", "wetext")}
                for key in ("all", corpus.stem, row["dataset"]):
                    t = tally[key]
                    t["scorable"] += 1
                    for name in ("ours", "wetext"):
                        for lv in LEVELS:
                            t["%s_%s" % (name, lv)] += levels[name] is not None and LEVELS.index(levels[name]) <= LEVELS.index(lv)
                    ov, wv = levels["ours"] is not None, levels["wetext"] is not None
                    t["both"] += ov and wv
                    t["only_ours"] += ov and not wv
                    t["only_wetext"] += wv and not ov
                    t["neither"] += not ov and not wv
                results.write(json.dumps(dict(id=row["id"], dataset=row["dataset"], text=row["text"], references=refs,
                                              ours=outputs["ours"], wetext=outputs["wetext"], levels=levels),
                                         ensure_ascii=False) + "\n")
    summary = {k: dict(v, ours_variant_rate=v["ours_variant"] / v["scorable"],
                       wetext_variant_rate=v["wetext_variant"] / v["scorable"]) for k, v in tally.items()}
    (a.out / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for k, v in summary.items():
        print("%-34s n=%5d  ours %.2f%%  wetext %.2f%%  only-ours %d only-wetext %d neither %d" % (
            k, v["scorable"], 100 * v["ours_variant_rate"], 100 * v["wetext_variant_rate"], v["only_ours"],
            v["only_wetext"], v["neither"]))


if __name__ == "__main__":
    main()
