#!/usr/bin/env python3
"""Build the optional Korean brand pronunciation pack from collected Wikidata rows.

    build_pack.py WIKIDATA_DIR REPORT_DIR --library LIB [--resources DIR]

1. Each (written Latin name, Korean label) pair: the written form is read by the
   engine inside a Korean sentence; the Korean label counts as a pronunciation
   only when it is phonetically close (jamo similarity >= 0.6; aliases only then,
   labels 0.4..0.6 go to review). Translated names (China Mobile 중국이동) fail.
2. Dropped: pairs the engine already reads identically, all-capital written forms
   (GT, SNT: abbreviations are read by letters), keys outside the lexicon grammar
   (letters, spaces and the joiners - ' &; a dot or digit is outside it),
   terms the system lexicon already owns, conflicting readings (review).
3. Writes lexicons/ko-KR/brand-wikidata.jsonl (scope brands, review pending),
   builds resources/domains/brands/ko-KR/lexicon.tsv, then checks every entry with
   the pack loaded as the domain layer and drops the ones another rule overrides.
REPORT_DIR receives candidates.jsonl (all decisions) and review.jsonl.
"""
import argparse
import collections
import json
import re
import subprocess
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/web"))
from native import Engine  # noqa: E402

LEGAL = re.compile(r"(,?\s+(inc\.?|incorporated|corp\.?|corporation|co\.?,?\s*ltd\.?|co\.?|ltd\.?|limited|llc|plc|"
                   r"ag|sa|s\.a\.|gmbh|n\.v\.|nv|s\.p\.a\.|spa|oyj|ab|asa|a/s|k\.k\.|holdings?|group))+$", re.I)
KO_PREFIX = re.compile(r"^(\(주\)|㈜|주식회사)\s*|\s*(\(주\)|㈜|주식회사)$")
KO_LEGAL = {"코퍼레이션": ("corporation", "corp"), "코포레이션": ("corporation", "corp"), "컴퍼니": ("company", "co"),
            "리미티드": ("limited", "ltd"), "인코퍼레이티드": ("inc", "incorporated"), "인크": ("inc",),
            "홀딩스": ("holdings",), "그룹": ("group",)}
HANGUL = re.compile(r"^[가-힣]+( [가-힣]+)*$")
LATIN = re.compile(r"^[A-Za-z][A-Za-z0-9&.'\- ]*[A-Za-z0-9.]$")
# Words join letter runs with - ' & (coca-cola, mcdonald's, pull&bear); & alone is a word.
WORD = r"[a-z]+(?:[-'&][a-z]+)*"
KEY = re.compile(r"^%s(?: (?:%s|&(?= )))*$" % (WORD, WORD))
PROBE = "의 발음을 확인합니다."


def slug(term):
    """Record id part; joiners stay distinct (ali-express ali_express, ali express ali-express)."""
    marked = term.replace("-", "_").replace("'", "_s_").replace("&", "_n_")
    return re.sub(r"[^a-z_]+", "-", marked)[:40]


def jamo(text):
    return unicodedata.normalize("NFD", text.replace(" ", ""))


def similarity(a, b):
    a, b = jamo(a), jamo(b)
    if not a or not b:
        return 0.0
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
        prev = cur
    return 1 - prev[-1] / max(len(a), len(b))


def written_forms(row):
    out = {}
    for k, name in enumerate([row["en"]] + row.get("aliases", [])):
        name = " ".join(name.split())
        stripped = LEGAL.sub("", name).strip()
        form = stripped if len(stripped) >= 2 else name
        if 2 <= len(form) <= 40 and LATIN.match(form) and not form.isdigit():
            out[form] = out.get(form, True) and k > 0
    return sorted(out.items())


def korean_form(ko, term):
    ko = re.sub(r"\s*\([^)]*\)$", "", ko).strip()
    ko = KO_PREFIX.sub("", ko).strip()
    if not HANGUL.match(ko):
        return None
    words = ko.split(" ")
    while len(words) > 1 and words[-1] in KO_LEGAL and not any(w in term.split() for w in KO_LEGAL[words[-1]]):
        words.pop()
    return " ".join(words)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("wikidata", type=Path)
    ap.add_argument("report", type=Path)
    ap.add_argument("--library", type=Path, required=True)
    ap.add_argument("--resources", type=Path, default=ROOT / "resources")
    args = ap.parse_args()
    engine = Engine(args.library, args.resources, dict(default_language="ko-KR", output_mode="speech",
                                                        detailed_trace=False))
    cache = {}

    def reading(form):
        if form not in cache:
            text = engine.normalize((form + "를").encode())["text"]
            cache[form] = re.sub(r"(을|를)$", "", text).strip()
        return cache[form]

    system_terms = set()
    for p in (ROOT / "lexicons/ko-KR").glob("*.jsonl"):
        if p.name.startswith("brand-"):  # the brand packs themselves
            continue
        for line in p.read_text(encoding="utf-8").splitlines():
            if line.strip():
                r = json.loads(line)
                system_terms.update([r["term"], *r.get("aliases", [])])
    rows = [json.loads(l) for p in sorted(args.wikidata.glob("*.jsonl"))
            for l in p.read_text(encoding="utf-8").splitlines() if l.strip()]
    stats = collections.Counter()
    by_key = collections.defaultdict(list)
    for row in rows:
        for form, alias in written_forms(row):
            term = form.lower()
            ko = korean_form(row["ko"], term)
            if not ko:
                stats["korean-not-hangul"] += 1
                continue
            if not any(ch.islower() for ch in form):
                stats["all-capital"] += 1
                continue
            if not KEY.match(term) or len(term.split()) > 8 or term in system_terms:
                stats["key-grammar-or-system"] += 1
                continue
            before = reading(form)
            if before.replace(" ", "") == ko.replace(" ", ""):
                stats["already-read"] += 1
                continue
            sim = similarity(before, ko)
            status = "accept" if sim >= 0.6 else "review" if sim >= 0.4 and not alias else "reject"
            by_key[term].append(dict(term=term, written=form, pronunciation=ko, before=before, similarity=round(sim, 3),
                                     status=status, qid=row["qid"], cls=row["cls"], alias=alias, en=row["en"],
                                     ko=row["ko"]))
    decided = []
    for term, cands in sorted(by_key.items()):
        best = max(cands, key=lambda c: (c["similarity"], c["qid"]))
        if len({c["pronunciation"].replace(" ", "") for c in cands}) > 1 and best["status"] == "accept":
            best = dict(best, status="review", conflict=sorted({c["pronunciation"] for c in cands}))
        decided.append(best)
    args.report.mkdir(parents=True, exist_ok=True)
    (args.report / "candidates.jsonl").write_text("".join(json.dumps(c, ensure_ascii=False) + "\n" for c in decided),
                                                  encoding="utf-8")
    (args.report / "review.jsonl").write_text("".join(json.dumps(c, ensure_ascii=False) + "\n"
                                                      for c in decided if c["status"] == "review"), encoding="utf-8")
    accepted = [c for c in decided if c["status"] == "accept"]

    def record(c):
        return dict(id="brand-wd-%s-%s" % (c["qid"].lower(), slug(c["term"])),
                    source_lang="en-US", target_lang="ko-KR", term=c["term"], aliases=[], pronunciation=c["pronunciation"],
                    category="brands", scope="brands", review_status="pending", evidence=["wikidata-brands-2026-09-30"],
                    notes="Wikidata %s (%s): en \"%s\", ko \"%s\"; jamo similarity %.2f to the reading without the "
                          "pack (%s). Machine-selected; review pending." % (c["qid"], c["cls"], c["en"], c["ko"],
                                                                             c["similarity"], c["before"]))
    lexicon = ROOT / "lexicons/ko-KR/brand-wikidata.jsonl"
    pack = ROOT / "resources/domains/brands/ko-KR/lexicon.tsv"
    dropped = []
    for _ in range(3):
        lexicon.write_text("".join(json.dumps(record(c), ensure_ascii=False) + "\n" for c in accepted),
                           encoding="utf-8")
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
    counts = collections.Counter(c["status"] for c in decided)
    print("pairs:", dict(stats))
    print("decisions:", dict(counts), "| pack entries:", len(accepted), "| dropped unreachable:", len(dropped))


if __name__ == "__main__":
    main()
