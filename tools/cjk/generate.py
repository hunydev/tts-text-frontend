#!/usr/bin/env python3
"""Build the Korean Hanja reading tables from the Unihan database.

See docs/cjk-korean-reading.ko.md. `build --check` verifies all three runtime
tables byte for byte, including LF line endings, without modifying any file.

Build time only. Nothing here runs at TTS runtime: the product loads the
generated TSV like any other resource.

Data source
    Unicode Unihan database (Unicode License). The committed CI input is a
    line subset produced by `subset`, not the full archive (design 11.3 A).

Contract
    * Only canonical readings are stored. Word-initial 두음법칙 forms are
      derived at run time, never baked into the table (design 27-6).
    * Multiple genuine readings keep every candidate, preferred first,
      chosen deterministically. No random selection at any stage.
    * A character without a usable reading is omitted; the run-time policy
      preserves its source text (design 10).
"""
import argparse
import collections
import json
import pathlib
import sys
import tempfile

from kana_table import build as build_kana
from word_table import build as build_words

TABLES = ("hanja_readings.tsv", "kana_readings.tsv", "cjk_words.tsv")

# ---------------------------------------------------------------- constants

# UAX #38 kIRG_KSource prefixes. KC (Korean History Online database) and KU
# (moved/removed reference) are not KS standards and are out of scope.
KS_STANDARD = {
    "K0": "KS X 1001:2004",
    "K1": "KS X 1002:2001",
    "K2": "KS X 1027-1:2011",
    "K3": "KS X 1027-2:2011",
    "K4": "KS X 1027-3:2011",
    "K5": "KS X 1027-4:2011",
    "K6": "KS X 1027-5:2021",
}
DEFAULT_SOURCES = ("K0", "K1")

# UAX #38 kHangul source letters.
TAG_MEANING = {
    "0": "KS X 1001",
    "1": "KS X 1002",
    "E": "한문 교육용 기초 한자",
    "N": "인명용 한자",
    "X": "현재 인코딩에서 제거된 구 K-source",
}
# Preferred-reading order. A basic-education reading outranks a code-set
# listing, which outranks a personal-name-only reading. Deterministic.
TAG_RANK = (("E", 0), ("0", 1), ("1", 2), ("N", 3))
# Hand-ordered first readings (2026-10-27): where Unihan ranks two readings alike, the
# one Korean words use goes first. 滑 활 (원활 평활 활주로 滑雪 활설; 골 only in 滑稽 골계).
PREFERRED_FIRST = {"滑": "활"}

SYLLABLE_BASE, LEAD_COUNT, VOWEL_COUNT, TAIL_COUNT = 0xAC00, 19, 21, 28
LEAD_N, LEAD_R, LEAD_NG = 2, 5, 11
# ㅑ ㅒ ㅕ ㅖ ㅛ ㅠ ㅣ — the vowels that take the ㅇ form under 두음법칙.
PALATAL_VOWELS = frozenset({2, 3, 6, 7, 12, 17, 20})

FIELD_FILES = {
    "kIRG_KSource": "Unihan_IRGSources.txt",
    "kCompatibilityVariant": "Unihan_IRGSources.txt",
    "kHangul": "Unihan_Readings.txt",
}
SUBSET_NAME = "unihan-k-subset.txt"


# ------------------------------------------------------------------ hangul

def decompose(syllable):
    if len(syllable) != 1:
        return None
    index = ord(syllable) - SYLLABLE_BASE
    if not 0 <= index < LEAD_COUNT * VOWEL_COUNT * TAIL_COUNT:
        return None
    return (index // (VOWEL_COUNT * TAIL_COUNT),
            index % (VOWEL_COUNT * TAIL_COUNT) // TAIL_COUNT,
            index % TAIL_COUNT)


def compose(lead, vowel, tail):
    return chr(SYLLABLE_BASE + (lead * VOWEL_COUNT + vowel) * TAIL_COUNT + tail)


def initial_form(syllable):
    """The 두음법칙 form of a canonical reading, or None when unaffected.

    ㄹ + ㅑㅕㅖㅛㅠㅣ -> ㅇ,  ㄹ + 그 밖의 모음 -> ㄴ,  ㄴ + ㅑㅕㅛㅠㅣ -> ㅇ.
    This is the run-time rule, reproduced here only to detect and fold the
    derived forms that Unihan lists as separate readings. The generated table
    never stores the result.
    """
    parts = decompose(syllable)
    if not parts:
        return None
    lead, vowel, tail = parts
    if lead == LEAD_R:
        return compose(LEAD_NG if vowel in PALATAL_VOWELS else LEAD_N, vowel, tail)
    if lead == LEAD_N and vowel in PALATAL_VOWELS:
        return compose(LEAD_NG, vowel, tail)
    return None


# ------------------------------------------------------------------ unihan

def source_path(unihan, name):
    """The committed subset holds every field the build reads, in one file.
    Fall back to the upstream per-field files when it is not present, so the
    same generator runs from a fresh download and from the CI input."""
    subset = unihan / SUBSET_NAME
    return subset if subset.is_file() else unihan / FIELD_FILES[name]


def read_field(unihan, name):
    marker = "\t" + name + "\t"
    with source_path(unihan, name).open(encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("#") or marker not in line:
                continue
            codepoint, _, value = line.rstrip("\n").split("\t", 2)
            yield int(codepoint[2:], 16), value


def unicode_version(unihan):
    with source_path(unihan, "kIRG_KSource").open(encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("# Unicode Version"):
                return line.split()[-1]
    return "unknown"


def load(unihan):
    sources = collections.defaultdict(set)
    for codepoint, value in read_field(unihan, "kIRG_KSource"):
        for part in value.split():
            sources[part.split("-")[0]].add(codepoint)
    readings = {}
    for codepoint, value in read_field(unihan, "kHangul"):
        entries = []
        for part in value.split():
            syllable, _, tags = part.partition(":")
            entries.append((syllable, tags))
        readings[codepoint] = entries
    compatibility = {}
    for codepoint, value in read_field(unihan, "kCompatibilityVariant"):
        compatibility[codepoint] = int(value[2:], 16)
    return sources, readings, compatibility


# ---------------------------------------------------------------- resolving

def rank(tags):
    for letter, value in TAG_RANK:
        if letter in tags:
            return value
    return len(TAG_RANK)


def confidence(tags):
    """A code-set or education listing is a documented reading. A personal-name
    listing alone is a conventional variant and stays lower."""
    if set(tags) & {"0", "1", "E"}:
        return "high"
    return "medium"


def resolve(entries):
    """Fold 두음 forms, then order the remaining readings deterministically.

    A pair looks like 두음 whenever one reading is the other's word-initial
    form, but the shape alone does not prove it. 怒 lists 로:0 and 노:0E, and
    노 is the ordinary reading (분노) while 로 survives only in 희로애락; the
    same holds for 異(이/리), 諾(낙/락) and 撓(요/뇨). Folding those would
    delete the common reading.

    The decisive signal is the source tag: a genuine derived form is never
    better attested than the reading it comes from. So fold only when the
    ㄹ/ㄴ-onset reading ranks at least as high as its initial form, and keep
    both otherwise.
    """
    usable = [(s, t) for s, t in entries if t.replace("X", "")]
    if not usable:
        return [], []
    tag_of = {}
    for syllable, tags in usable:
        tag_of.setdefault(syllable, tags)
    derived = set()
    for syllable, tags in usable:
        form = initial_form(syllable)
        if form in tag_of and rank(tags) <= rank(tag_of[form]):
            derived.add(form)
    canonical = [(s, t) for s, t in usable if s not in derived]
    if not canonical:                       # defensive: never drop everything
        canonical = usable
    ordered = sorted(
        canonical,
        key=lambda item: (rank(item[1]),
                          [s for s, _ in usable].index(item[0]),
                          item[0]),
    )
    folded = sorted(derived)
    return ordered, folded


# ----------------------------------------------------------------- outputs

def build(unihan, out, selected):
    sources, readings, compatibility = load(unihan)
    repertoire = set().union(*(sources[key] for key in selected))

    rows, provenance = [], []
    resolved = folded_total = 0
    multi = collections.Counter()
    for codepoint in sorted(repertoire):
        ordered, folded = resolve(readings.get(codepoint, []))
        if not ordered:
            continue
        if chr(codepoint) in PREFERRED_FIRST:
            ordered.sort(key=lambda item: item[0] != PREFERRED_FIRST[chr(codepoint)])
        resolved += 1
        folded_total += len(folded)
        multi[len(ordered)] += 1
        rows.append((chr(codepoint), "|".join(s for s, _ in ordered)))
        for index, (syllable, tags) in enumerate(ordered):
            provenance.append(dict(
                codepoint="U+%04X" % codepoint,
                character=chr(codepoint),
                reading=syllable,
                preferred=index == 0,
                source="UNIHAN_KHANGUL",
                source_tags=tags,
                confidence=confidence(tags),
                k_sources=sorted(k for k in selected if codepoint in sources[k]),
            ))

    # CJK Compatibility Ideographs exist largely because Korean encodes one Han
    # character once per reading: U+F914/U+F95C/U+F9BF are 樂 as 낙/락/요 while
    # U+6A02 carries all of them. A compatibility character's own kHangul is
    # therefore MORE specific than its canonical target's, and must win.
    # Inheriting the target's preferred reading would collapse the distinction.
    table = dict(rows)
    compat_rows = []
    for codepoint, target in sorted(compatibility.items()):
        if chr(codepoint) in table:
            continue                        # already in the selected repertoire
        ordered, _ = resolve(readings.get(codepoint, []))
        if ordered:
            value = "|".join(s for s, _ in ordered)
            origin, tags = "UNIHAN_KHANGUL_COMPATIBILITY", ordered[0][1]
            grade = confidence(tags)
        elif chr(target) in table:
            value = table[chr(target)]      # no own reading: fold to the target
            origin, tags = "UNIHAN_COMPATIBILITY_VARIANT", "->U+%04X" % target
            grade = "high"
        else:
            continue
        compat_rows.append((chr(codepoint), value))
        provenance.append(dict(
            codepoint="U+%04X" % codepoint,
            character=chr(codepoint),
            reading=value.split("|")[0],
            preferred=True,
            source=origin,
            source_tags=tags,
            confidence=grade,
            k_sources=[],
        ))

    out.mkdir(parents=True, exist_ok=True)
    header = (
        "# Generated by tools/cjk/generate.py; edit data/cjk/ sources, never this file.\n"
        "# Canonical readings only. 두음법칙 forms are applied at run time (design 7.3).\n"
        "# Multiple readings are preferred-first and deterministic; provenance is in\n"
        "# build output provenance.json. Matching this default is not linguistic approval.\n"
    )
    lines = ["hanja_reading\tko-KR\t%s\t%s\n" % row for row in rows + compat_rows]
    (out / "hanja_readings.tsv").write_text(header + "".join(lines), encoding="utf-8", newline="\n")
    (out / "provenance.json").write_text(
        json.dumps(dict(schema_version=1,
                        source="Unicode Unihan database (Unicode License)",
                        unicode_version=unicode_version(unihan),
                        tag_meaning=TAG_MEANING,
                        entries=provenance),
                   ensure_ascii=False, indent=1),
        encoding="utf-8", newline="\n")

    report = dict(
        unicode_version=unicode_version(unihan),
        selected_sources={k: KS_STANDARD[k] for k in selected},
        repertoire=len(repertoire),
        resolved=resolved,
        unresolved=len(repertoire) - resolved,
        coverage_percent=round(100.0 * resolved / len(repertoire), 2),
        reading_counts={str(k): v for k, v in sorted(multi.items())},
        folded_initial_forms=folded_total,
        compatibility_rows=len(compat_rows),
        compatibility_own_reading=sum(
            1 for row in provenance if row["source"] == "UNIHAN_KHANGUL_COMPATIBILITY"),
        compatibility_inherited=sum(
            1 for row in provenance if row["source"] == "UNIHAN_COMPATIBILITY_VARIANT"),
        runtime_rows=len(rows) + len(compat_rows),
        supplementary_plane=sum(1 for c in repertoire if c > 0xFFFF),
    )
    (out / "report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8", newline="\n")
    return report


def subset(unihan, out):
    """Emit the committed CI input: only the lines the generator reads.

    Every K source is kept, not just the selected ones, so the repertoire can
    be changed later without fetching the full archive again.
    """
    out.mkdir(parents=True, exist_ok=True)
    wanted = (("Unihan_IRGSources.txt", ("kIRG_KSource", "kCompatibilityVariant")),
              ("Unihan_Readings.txt", ("kHangul",)))
    lines = ["# Subset of the Unicode Unihan database (Unicode License).\n",
             "# Unicode Version %s\n" % unicode_version(unihan),
             "# Fields: kIRG_KSource, kCompatibilityVariant, kHangul.\n",
             "# Produced by tools/cjk/generate.py subset. Do not edit by hand.\n"]
    for filename, fields in wanted:
        markers = tuple("\t" + f + "\t" for f in fields)
        with (unihan / filename).open(encoding="utf-8") as handle:
            for line in handle:
                if not line.startswith("#") and any(m in line for m in markers):
                    lines.append(line)
    target = out / SUBSET_NAME
    target.write_text("".join(lines), encoding="utf-8", newline="\n")
    return dict(path=str(target), lines=len(lines),
                bytes=target.stat().st_size)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("build", "subset"))
    parser.add_argument("--unihan", type=pathlib.Path, required=True,
                        help="directory holding the Unihan files or the committed subset")
    parser.add_argument("--out", type=pathlib.Path, required=True)
    parser.add_argument("--check", action="store_true",
                        help="build only: compare generated TSV bytes with --out, without writing")
    parser.add_argument("--sources", default=",".join(DEFAULT_SOURCES),
                        help="comma-separated kIRG_KSource prefixes (default: K0,K1)")
    args = parser.parse_args(argv)

    if args.command == "subset":
        if args.check:
            parser.error("--check is only available for build")
        result = subset(args.unihan, args.out)
    else:
        selected = tuple(s.strip() for s in args.sources.split(",") if s.strip())
        unknown = [s for s in selected if s not in KS_STANDARD]
        if unknown:
            parser.error("not a KS standard source: %s" % ", ".join(unknown))
        with tempfile.TemporaryDirectory(prefix="ttf-cjk-build-") as temporary:
            target = pathlib.Path(temporary) if args.check else args.out
            result = build(args.unihan, target, selected)
            result["kana"] = build_kana(target)
            result["words"] = build_words(target)
            if args.check:
                mismatches = [name for name in TABLES if not (args.out / name).is_file()
                              or (args.out / name).read_bytes() != (target / name).read_bytes()]
                result["check"] = dict(passed=not mismatches, mismatches=mismatches)
    json.dump(result, sys.stdout, ensure_ascii=False, indent=2)
    sys.stdout.write("\n")
    return int(bool(result.get("check", {}).get("mismatches")))


if __name__ == "__main__":
    raise SystemExit(main())
