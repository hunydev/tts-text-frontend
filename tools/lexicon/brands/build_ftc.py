#!/usr/bin/env python3
"""Add brand pronunciations registered with the FTC franchise disclosure list.

    build_ftc.py FTC_DIR REPORT_DIR --library LIB [--resources DIR] [--english-corpus FILE ...]

A franchisor registers the brand name, often as "<Hangul>(<Latin>)" or
"<Latin>(<Hangul>)" (코드런(CODE RUN), Funny Funny(퍼니퍼니)). The Hangul side is the
owner's own reading, so the phonetic test is looser than for Wikidata labels;
it only has to rule out translated names (빵농장(Bread Farm)).

1. Kept: both sides pure (Hangul and spaces / Latin letters, spaces, & ' -; the
   joiners stay in the key as written, a dot is outside the key grammar),
   jamo similarity >= 0.45 between the Hangul and the engine's reading of the
   written name (capital words read as words), and for several words every word
   aligned with a piece of the Hangul (>= 0.3; failures go to review).
2. Dropped: a lone capital abbreviation of four letters or fewer (TOT, NBM), a
   single word common in English text (--english-corpus, words seen three
   times or more), keys outside the lexicon grammar, terms the system lexicon
   owns, pairs the engine already reads identically, terms the Wikidata pack
   already has (a different reading goes to review), conflicting readings.
3. Writes lexicons/ko-KR/brand-ftc.jsonl (scope brands, review pending), rebuilds
   resources/domains/brands/ko-KR/lexicon.tsv and drops entries that another rule
   overrides with the pack loaded as the domain layer. Run after build_pack.py.
"""
import argparse
import collections
import html
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_pack import KEY, PROBE, ROOT, Engine, similarity, slug  # noqa: E402

SOURCE = "ftc-franchise-brands-2026-09-30"
HANGUL = r"[가-힣]+(?: [가-힣]+)*"
LATIN = r"[A-Za-z][A-Za-z&'.\- ]*[A-Za-z.]|[A-Za-z]"
PAIRS = (re.compile(r"^(?P<ko>%s)\s*\((?P<en>%s)\)$" % (HANGUL, LATIN)),
         re.compile(r"^(?P<en>%s)\s*\((?P<ko>%s)\)$" % (LATIN, HANGUL)))
EN_EVAL = ROOT / "benchmark/generated/en-eval"
DEFAULT_CORPUS = [EN_EVAL / "sproat-sample-shard58.jsonl", EN_EVAL / "nemo-eval-acacc21.jsonl"]


def aligned(parts, ko):
    """Best split of the Hangul into one piece per word, scored by its weakest piece."""
    best = {0: 1.0}
    for i, part in enumerate(parts):
        nxt = {}
        for pos, score in best.items():
            for end in range(pos + 1, len(ko) - (len(parts) - i - 1) + 1):
                value = min(score, similarity(part, ko[pos:end]))
                if value > nxt.get(end, -1):
                    nxt[end] = value
        best = nxt
    return best.get(len(ko), 0)


def vocabulary(paths):
    counts = collections.Counter()
    for path in paths:
        with path.open(encoding="utf-8") as h:
            for line in h:
                counts.update(re.findall(r"[a-z]+", line.lower()))
    return {w for w, n in counts.items() if n >= 3}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ftc", type=Path)
    ap.add_argument("report", type=Path)
    ap.add_argument("--library", type=Path, required=True)
    ap.add_argument("--resources", type=Path, default=ROOT / "resources")
    ap.add_argument("--english-corpus", type=Path, action="append")
    args = ap.parse_args()
    corpus = args.english_corpus or DEFAULT_CORPUS
    missing = [str(p) for p in corpus if not p.exists()]
    if missing:
        raise SystemExit("English corpus not found: %s (pass --english-corpus)" % ", ".join(missing))
    common = vocabulary(corpus)
    engine = Engine(args.library, args.resources, dict(default_language="ko-KR", output_mode="speech",
                                                        detailed_trace=False))
    cache = {}

    def reading(form):
        if form not in cache:
            text = engine.normalize((form + "를").encode())["text"]
            cache[form] = re.sub(r"(을|를)$", "", text).strip()
        return cache[form]

    system_terms, wikidata = set(), {}
    for p in (ROOT / "lexicons/ko-KR").glob("*.jsonl"):
        if p.name == "brand-ftc.jsonl":
            continue
        for line in p.read_text(encoding="utf-8").splitlines():
            if line.strip():
                r = json.loads(line)
                if p.name.startswith("brand-"):
                    wikidata[r["term"]] = r["pronunciation"]
                else:
                    system_terms.update([r["term"], *r.get("aliases", [])])
    seen = {}
    for p in sorted(args.ftc.glob("brands-*.jsonl"), reverse=True):  # newest base year first
        for line in p.read_text(encoding="utf-8").splitlines():
            r = json.loads(line)
            name = " ".join(html.unescape(html.unescape(r.get("brandNm") or "")).replace("’", "'").split())
            for pattern in PAIRS:
                m = pattern.match(name)
                if m:
                    seen.setdefault((m.group("en").strip(), m.group("ko").replace(" ", "")), r)
                    break
    stats = collections.Counter()
    accepted, review = [], []
    for (en, ko), r in sorted(seen.items()):
        # The joiners - ' & stay in the key (Brow-Doll, MOM'S PIZZA, Bowl & Bowl); they
        # are matched as written, so replacing them would give a key no text reaches.
        words = en.split()
        term = " ".join(w.lower() for w in words)
        if not KEY.match(term) or len(words) > 8 or term in system_terms:
            stats["key-grammar-or-system"] += 1
            continue
        if len(words) == 1 and words[0].isalpha() and words[0].isupper() and len(words[0]) <= 4:
            stats["abbreviation"] += 1
            continue
        if len(words) == 1 and term in common:
            stats["common-english-word"] += 1
            continue
        before = reading(en)
        if before.replace(" ", "") == ko:
            stats["already-read"] += 1
            continue
        spoken = [w.capitalize() if w.isupper() and len(w) > 1 and re.sub("[-'&]", "", w).isalpha() else w
                  for w in words]
        sim = max(similarity(before, ko), similarity(reading(" ".join(spoken)), ko))
        row = dict(term=term, written=en, pronunciation=ko, before=before, similarity=round(sim, 3),
                   brand=r["brandMnno"], name=r["brandNm"], year=r["jngBizCrtraYr"],
                   industry=(r.get("indutyMlsfcNm") or "").strip())
        if sim < 0.45:
            stats["not-phonetic"] += 1
            continue
        if len(words) > 1 and aligned([reading(w) for w in spoken if w != "&"], ko) < 0.3:
            stats["word-alignment"] += 1
            review.append(dict(row, reason="word-alignment"))
            continue
        if term in wikidata:
            if wikidata[term].replace(" ", "") != ko:
                review.append(dict(row, reason="wikidata-differs", wikidata=wikidata[term]))
            stats["in-wikidata-pack"] += 1
            continue
        accepted.append(row)
    by_term = collections.defaultdict(list)
    for row in accepted:
        by_term[row["term"]].append(row)
    accepted = []
    for term, rows in sorted(by_term.items()):
        if len({x["pronunciation"] for x in rows}) > 1:
            stats["conflict"] += 1
            review.extend(dict(x, reason="conflict") for x in rows)
        else:
            accepted.append(rows[0])
    args.report.mkdir(parents=True, exist_ok=True)
    (args.report / "candidates.jsonl").write_text("".join(json.dumps(c, ensure_ascii=False) + "\n" for c in accepted),
                                                  encoding="utf-8")
    (args.report / "review.jsonl").write_text("".join(json.dumps(c, ensure_ascii=False) + "\n" for c in review),
                                              encoding="utf-8")

    def record(c):
        return dict(id="brand-ftc-%s-%s" % (c["brand"].lower().replace("_", "-"), slug(c["term"])),
                    source_lang="en-US", target_lang="ko-KR", term=c["term"], aliases=[], pronunciation=c["pronunciation"],
                    category="brands", scope="brands", review_status="pending", evidence=[SOURCE],
                    notes="FTC franchise brand %s (%s, base year %s): registered \"%s\"; jamo similarity %.2f to the "
                          "reading without the pack (%s). Machine-selected; review pending."
                          % (c["brand"], c["industry"] or "-", c["year"], c["name"], c["similarity"], c["before"]))
    lexicon = ROOT / "lexicons/ko-KR/brand-ftc.jsonl"
    pack = ROOT / "resources/domains/brands/ko-KR/lexicon.tsv"
    dropped = []
    for _ in range(3):
        lexicon.write_text("".join(json.dumps(record(c), ensure_ascii=False) + "\n" for c in accepted), encoding="utf-8")
        subprocess.run([sys.executable, str(ROOT / "tools/lexicon/manage.py"), "build"], check=True,
                       stdout=subprocess.DEVNULL)
        check = Engine(args.library, args.resources, dict(default_language="ko-KR", output_mode="speech",
                                                           detailed_trace=False), [(pack, 2)])
        keep = []
        for c in accepted:
            got = check.normalize((c["written"] + PROBE).encode())["text"]
            (keep if got == c["pronunciation"] + PROBE else dropped).append(c)
        check.close()
        if len(keep) == len(accepted):
            break
        accepted = keep
    (args.report / "dropped-unreachable.jsonl").write_text(
        "".join(json.dumps(c, ensure_ascii=False) + "\n" for c in dropped), encoding="utf-8")
    print("registered pairs:", len(seen), dict(stats))
    print("pack entries:", len(accepted), "| review:", len(review), "| dropped unreachable:", len(dropped))


if __name__ == "__main__":
    main()
