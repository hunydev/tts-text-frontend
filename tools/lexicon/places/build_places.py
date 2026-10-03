#!/usr/bin/env python3
"""Korean readings of romanized South Korean place names from Wikidata (CC0).

    build_places.py PLACES_DIR --library LIB [--resources DIR] [--english-corpus FILE ...]

A place in South Korea carries a Korean label (인천광역시, 잠실동, 독도) and an English
label or alias that is usually its Revised Romanization (Incheon, Jamsil-dong,
Dokdo). The English form is kept only when it is that romanization:

1. Kept: a single word, optionally with a hyphenated administrative suffix
   (Jamsil-dong), of four or more letters, whose segmentation into Revised
   Romanization syllables can spell the Korean label exactly, or the label
   without its administrative suffix (인천광역시 -> 인천, 아산시 -> 아산).
2. Dropped: words common in English text (--english-corpus, three or more
   occurrences); an English word or name (in --english-words, e.g. CMUdict)
   unless it is the place's own full English label (Seoul stays; Guam, and hail
   from Hail-ri, do not); one-syllable readings (surnames such as Jang, Yang);
   keys the system lexicon or a domain pack already owns, keys with two
   different Korean readings, and pairs the engine already reads so.
3. Writes lexicons/ko-KR/place-wikidata.jsonl (category places, system scope,
   review pending) and rebuilds the resources with tools/lexicon/manage.py.
"""
import argparse
import collections
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/web"))
from native import Engine  # noqa: E402

SOURCE = "wikidata-places-2026-10-03"
PACK_PATH = "languages/ko-KR/lexicon/places/default.tsv"
EN_EVAL = ROOT / "benchmark/generated/en-eval"
DEFAULT_CORPUS = [EN_EVAL / "sproat-sample-shard58.jsonl", EN_EVAL / "nemo-eval-acacc21.jsonl"]
INITIALS = {"g": 0, "kk": 1, "n": 2, "d": 3, "tt": 4, "r": 5, "m": 6, "b": 7, "pp": 8, "s": 9, "ss": 10, "": 11,
            "j": 12, "jj": 13, "ch": 14, "k": 15, "t": 16, "p": 17, "h": 18, "l": 5}
VOWELS = {"a": 0, "ae": 1, "ya": 2, "yae": 3, "eo": 4, "e": 5, "yeo": 6, "ye": 7, "o": 8, "wa": 9, "wae": 10,
          "oe": 11, "yo": 12, "u": 13, "wo": 14, "we": 15, "wi": 16, "yu": 17, "eu": 18, "ui": 19, "i": 20}
FINALS = {"": 0, "k": 1, "n": 4, "t": 7, "l": 8, "m": 16, "p": 17, "ng": 21}
SUFFIXES = ("특별자치시", "특별자치도", "광역시", "특별시", "시", "군", "구", "동", "읍", "면", "리", "도")
ROMAN_SUFFIX = {"dong": "동", "si": "시", "gun": "군", "gu": "구", "eup": "읍", "myeon": "면", "ri": "리",
                "do": "도", "ro": "로", "gil": "길", "san": "산", "gang": "강", "cheon": "천"}
WORD = re.compile(r"[A-Z][a-z]{3,}(?:-(?:dong|si|gun|gu|eup|myeon|ri|do|ro|gil|san|gang|cheon))?")
# A base name also stands alone (Gangnam District / 강남구 -> Gangnam 강남, Jamsil-dong / 잠실동 ->
# Jamsil 잠실): the Korean label minus one of these suffixes, after an English class word or a
# hyphenated suffix.
CLASS_WORDS = {"district", "county", "city", "station", "island", "mountain", "province", "bridge", "port",
               "beach", "temple", "stream", "river", "peak", "pass", "village", "town", "lake", "valley"}
BASE_SUFFIXES = SUFFIXES + ("역", "섬", "대교", "교", "항", "해수욕장", "산", "강", "천", "사", "봉", "호", "계곡", "마을")


def syllable(i, v, f):
    return chr(0xAC00 + (INITIALS[i] * 21 + VOWELS[v]) * 28 + FINALS[f])


def spellings(word, limit=256):
    """Every Hangul string the lowercase romanization can spell, syllable by syllable."""
    n = len(word)
    memo = {}

    def go(pos):
        if pos == n:
            return [""]
        if pos in memo:
            return memo[pos]
        out = []
        for il in (2, 1, 0):
            ini = word[pos:pos + il]
            if len(ini) != il or ini not in INITIALS or (ini == "l" and not (pos and word[pos - 1] == "l")):
                continue
            for vl in (3, 2, 1):
                v = word[pos + il:pos + il + vl]
                if len(v) != vl or v not in VOWELS:
                    continue
                q = pos + il + vl
                for fl in (2, 1, 0):
                    f = word[q:q + fl]
                    if len(f) != fl or f not in FINALS:
                        continue
                    out += [syllable(ini, v, f) + rest for rest in go(q + fl)]
        memo[pos] = out[:limit]
        return memo[pos]

    return set(go(0))


def korean(ko):
    ko = re.sub(r"\s*\(.*\)$", "", ko).strip()
    return ko if re.fullmatch(r"[가-힣]+", ko) else None


def match(en, ko):
    """The Korean reading of an English label that romanizes the Korean label, else None."""
    if not WORD.fullmatch(en):
        return None
    word = en.lower()
    base, _, suffix = word.partition("-")
    forms = spellings(word.replace("-", ""))
    if ko in forms:
        return ko
    for s in SUFFIXES:  # 인천광역시 -> Incheon, 아산시 -> Asan
        if ko.endswith(s) and len(ko) > len(s) and ko[:-len(s)] in forms:
            if suffix and ROMAN_SUFFIX.get(suffix) != s:
                continue
            return ko[:-len(s)] + (ROMAN_SUFFIX[suffix] if suffix else "")
    return None


def base_match(en, ko):
    """(Base, reading) for Gangnam District / 강남구 or Jamsil-dong / 잠실동, else None."""
    words = en.split()
    if len(words) == 2 and words[1].lower() in CLASS_WORDS:
        base = words[0]
    elif len(words) == 1 and "-" in en:
        base = en.split("-")[0]
    else:
        return None
    if not re.fullmatch(r"[A-Z][a-z]{3,}", base):
        return None
    forms = spellings(base.lower())
    for s in BASE_SUFFIXES:
        if ko.endswith(s) and len(ko) > len(s) and ko[:-len(s)] in forms:
            return base.lower(), ko[:-len(s)]
    return None


def vocabulary(paths, minimum=3):
    count = collections.Counter()
    for p in paths:
        for line in p.read_text(encoding="utf-8").splitlines():
            r = json.loads(line)
            for t in re.findall(r"[A-Za-z]+", r.get("text") or r.get("input") or ""):
                count[t.lower()] += 1
    return {w for w, c in count.items() if c >= minimum}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("places", type=Path)
    ap.add_argument("--library", type=Path, required=True)
    ap.add_argument("--resources", type=Path, default=ROOT / "resources")
    ap.add_argument("--english-corpus", type=Path, action="append")
    ap.add_argument("--english-words", type=Path,
                    help="word list (one per line, or a CMUdict runtime pronunciation.tsv)")
    args = ap.parse_args()
    corpus = args.english_corpus or DEFAULT_CORPUS
    missing = [str(p) for p in corpus if not p.exists()]
    if missing:
        raise SystemExit("English corpus not found: %s (pass --english-corpus)" % ", ".join(missing))
    common = vocabulary(corpus)
    english = set()
    if args.english_words:
        for line in args.english_words.read_text(encoding="utf-8").splitlines():
            parts = line.split("\t")
            english.add((parts[2] if len(parts) > 2 and parts[0] == "pronunciation" else parts[0]).strip().lower())
    owned = set()
    for p in (ROOT / "lexicons/ko-KR").glob("*.jsonl"):
        if p.name == "place-wikidata.jsonl":
            continue
        for line in p.read_text(encoding="utf-8").splitlines():
            if line.strip():
                r = json.loads(line)
                owned.add(r["term"].lower())
                owned.update(a.lower() for a in r.get("aliases", []))
    readings = collections.defaultdict(collections.Counter)
    qids = {}
    own_label = set()  # (key, reading) found as an item's full English label
    for line in (args.places / "places.jsonl").read_text(encoding="utf-8").splitlines():
        r = json.loads(line)
        ko = korean(r["ko"])
        if not ko:
            continue
        for en in [r["en"], *r["aliases"]]:
            en = en.strip()
            found = []
            reading = match(en, ko)
            if reading:
                found.append((en.lower(), reading))
                if en == r["en"].strip():
                    own_label.add((en.lower(), reading))
            base = base_match(en, ko)
            if base:
                found.append(base)
            for key, reading in found:
                readings[key][reading] += 1
                qids.setdefault((key, reading), r["qid"])
    # Compare with the reading the engine gives without this pack (a resource copy without it).
    out = ROOT / "lexicons/ko-KR/place-wikidata.jsonl"
    scratch = Path(tempfile.mkdtemp(prefix="ttf-places-"))
    without = scratch / "resources"
    shutil.copytree(args.resources, without)
    manifest = without / "manifest.ttfm"
    manifest.write_text("".join(l for l in manifest.read_text(encoding="utf-8").splitlines(True)
                                  if l.strip() != PACK_PATH), encoding="utf-8")
    (without / PACK_PATH).unlink(missing_ok=True)
    engine = Engine(args.library, without, dict(default_language="ko-KR", output_mode="speech"))
    reasons = collections.Counter()
    rows = []
    for key in sorted(readings):
        counter = readings[key]
        if len(counter) > 1:
            reasons["conflict"] += 1
            continue
        reading = next(iter(counter))
        if key.replace("-", "") in common or key in common:
            reasons["common-english-word"] += 1
            continue
        if key in owned:
            reasons["owned"] += 1
            continue
        if len(reading) == 1:
            reasons["one-syllable"] += 1
            continue
        if key.replace("-", "") in english and (key, reading) not in own_label:
            reasons["english-word"] += 1
            continue
        now = re.sub(r"를$", "", engine.normalize((key.capitalize() + "를").encode())["text"]).strip()
        if now.replace(" ", "") == reading:
            reasons["already-read"] += 1
            continue
        qid = qids[(key, reading)]
        rows.append(dict(id="place-wd-%s-%s" % (qid.lower(), re.sub(r"[^a-z0-9]+", "-", key)),
                         source_lang="en-US", target_lang="ko-KR", term=key, aliases=[], pronunciation=reading,
                         category="places", review_status="pending", evidence=[SOURCE],
                         notes="Wikidata %s: Korean label romanized as \"%s\" (Revised Romanization syllables "
                               "spell %s); the reading without the entry was %s. Machine-selected; review pending."
                               % (qid, key, reading, now)))
    out.write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in rows), encoding="utf-8")
    print("romanized pairs", len(readings), dict(reasons), "entries", len(rows))
    engine.close()
    shutil.rmtree(scratch)
    subprocess.run([sys.executable, str(ROOT / "tools/lexicon/manage.py"), "build"], check=True,
                   stdout=subprocess.DEVNULL)


if __name__ == "__main__":
    main()
