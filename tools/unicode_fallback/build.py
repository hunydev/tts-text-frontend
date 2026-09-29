#!/usr/bin/env python3
"""Build the Unicode spoken-fallback files for every CLDR locale.

    build.py --ucd DIR --cldr DIR --out DIR [--locales ko,en] [--emoji-header FILE]

Inputs are pinned releases (Unicode 17.0.0 UCD/emoji files in --ucd, a CLDR
release checkout in --cldr). For each locale with names the builder writes:

  <tag>.idx  direct index: header, then one little-endian u32 per code point
             U+0000..limit-1 giving the offset of its name in <tag>.dat (0 = none)
  <tag>.dat  string pool: header, then NUL-terminated UTF-8 names
  <tag>.seq  sorted multi-code-point sequences: header, then fixed records of a
             zero-padded 60-byte UTF-8 key and the u32 offset of its name
  <tag>.tsv  the same content for review: kind, key, characters, name, source

plus manifest.json (sources, hashes, counts, transforms) and coverage.tsv.
Keys drop U+FE0E/U+FE0F, as CLDR keys do. Names come from CLDR `type="tts"`
annotations (then annotationsDerived) along the locale's parent chain; English
also falls back to the lowercased UCD name for symbols. Digit keycaps read the
digit alone, from the locale's RBNF spellout-numbering rules. Output is
deterministic: the same inputs give byte-identical files. See
docs/unicode-fallback.ko.md.
"""
import argparse
import hashlib
import json
import re
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

INDEX_LIMIT = 0x20000          # planes 0-1 hold every symbol and emoji base
KEY_BYTES = 60
FORMAT = 1
ASCII_LIMIT = 0x80             # ASCII symbols belong to the engine's own tables
VARIATION = {0xFE0E, 0xFE0F}
MODIFIERS = set(range(0x1F3FB, 0x1F400))
REGIONAL = set(range(0x1F1E6, 0x1F200))
TAGS = set(range(0xE0020, 0xE0080))
# Sentence punctuation is prosody, not a name: full-width and ideographic forms
# of . , : ; ! ?, ellipses, inverted marks, and the middle dots and bullets that
# separate words (유•초•중등, 자택∙직장, 10⋅26). Range marks (5∼6) belong to the
# range reading, not to a symbol name.
PAUSE_PUNCTUATION = set(map(ord, "、。，．：；！？…‥・·‧﹐﹑﹒﹔﹕﹖﹗｡､･〃¿¡•∙⋅․‣⁃◦∼～〜"))
SYMBOL_CATEGORIES = {"Sm", "Sc", "Sk", "So"}
PUNCTUATION_NAMED = {"Po"}     # other P* (brackets, quotes, dashes) stay prosody


def cps(text):
    return [ord(c) for c in text]


def key_of(seq):
    return tuple(c for c in seq if c not in VARIATION)


def hexkey(seq):
    return " ".join("%04X" % c for c in seq)


def clean(name):
    name = " ".join(name.split())
    # CLDR label patterns ("{0}: {1}") join a category and a detail; a comma
    # carries the same pause in speech without a colon for TTS to name.
    return name.replace(": ", ", ")


# ---------------------------------------------------------------- UCD / emoji
def read_ucd(path):
    """Names, categories and the code points with a compatibility decomposition."""
    names, category, compatibility = {}, {}, set()
    for line in path.read_text(encoding="utf-8").splitlines():
        fields = line.split(";")
        cp, name, gc = int(fields[0], 16), fields[1], fields[2]
        category[cp] = gc
        if fields[5].startswith("<"):
            compatibility.add(cp)
        if not name.startswith("<"):
            names[cp] = name
    return names, category, compatibility


def read_sequences(*paths):
    """RGI sequences: kind -> list of scalar tuples (ranges expanded)."""
    out = {}
    for path in paths:
        for line in path.read_text(encoding="utf-8").splitlines():
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            points, kind = [f.strip() for f in line.split(";")[:2]]
            if ".." in points:
                a, b = (int(x, 16) for x in points.split(".."))
                seqs = [(c,) for c in range(a, b + 1)]
            else:
                seqs = [tuple(int(x, 16) for x in points.split())]
            out.setdefault(kind, []).extend(seqs)
    return out


def read_property(path, prop):
    ranges = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        points, name = [f.strip() for f in line.split(";")[:2]]
        if name != prop:
            continue
        a, _, b = points.partition("..")
        ranges.append((int(a, 16), int(b or a, 16)))
    ranges.sort()
    merged = []
    for a, b in ranges:
        if merged and a <= merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], max(merged[-1][1], b))
        else:
            merged.append((a, b))
    return merged


# ---------------------------------------------------------------------- CLDR
def parent_map(cldr):
    root = ET.parse(cldr / "common/supplemental/supplementalData.xml").getroot()
    parents = {}
    for group in root.iter("parentLocales"):
        if group.get("component"):
            continue
        for item in group.iter("parentLocale"):
            for loc in item.get("locales").split():
                parents[loc] = item.get("parent")
    return parents


def chain(locale, parents):
    out = []
    while locale and locale != "root":
        out.append(locale)
        locale = parents.get(locale) or (locale.rsplit("_", 1)[0] if "_" in locale else "root")
    return out


def read_annotations(path):
    """tts names of one CLDR file: key -> name ('' marks explicit inheritance)."""
    if not path.exists():
        return {}
    out = {}
    for item in ET.parse(path).getroot().iter("annotation"):
        if item.get("type") != "tts" or item.get("draft") in ("unconfirmed", "provisional"):
            continue
        text = (item.text or "").strip()
        out[key_of(cps(item.get("cp")))] = "" if text == "↑↑↑" else text
    return out


def resolve(kind_dir, loc_chain, cache):
    """key -> (name, source locale) along the parent chain."""
    resolved = {}
    for loc in reversed(loc_chain):           # parents first, children override
        if (kind_dir, loc) not in cache:
            cache[(kind_dir, loc)] = read_annotations(kind_dir / (loc + ".xml"))
        for key, name in cache[(kind_dir, loc)].items():
            if name:
                resolved[key] = (name, loc)
    return resolved


RULE = re.compile(r"^\s*(-?[0-9x.]+)\s*(?:/\d+)?\s*:\s*(.*?);\s*$")


def rbnf_rulesets(path):
    if not path.exists():
        return {}
    text = "".join(node.text or "" for node in ET.parse(path).getroot().iter("rbnfRules"))
    sets, current = {}, None
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("%") and line.endswith(":"):
            current = sets.setdefault(line[1:-1].lstrip("%"), [])
        elif current is not None:
            m = RULE.match(line)
            if m and re.fullmatch(r"\d+", m.group(1)):
                current.append((int(m.group(1)), m.group(2)))
    return sets


def rbnf_digit(sets, name, digit, depth=0):
    rules = sets.get(name)
    if not rules or depth > 8:
        return None
    base = [r for r in rules if r[0] <= digit]
    if not base:
        return None
    text = max(base)[1].strip().strip("'")
    ref = re.fullmatch(r"=%([\w-]+)=", text)
    if ref:
        return rbnf_digit(sets, ref.group(1), digit, depth + 1)
    if re.search(r"[<>=%#\[\]$]", text) or not text:
        return None
    return text


def digit_words(cldr, loc_chain):
    for loc in loc_chain:
        sets = rbnf_rulesets(cldr / "common/rbnf" / (loc + ".xml"))
        for name in ("spellout-numbering", "spellout-cardinal"):
            words = [rbnf_digit(sets, name, d) for d in range(10)]
            if all(words):
                return words, "rbnf:%s:%s" % (loc, name)
    return None, None


# --------------------------------------------------------------------- build
def eligible_single(cp, category, basic):
    if cp < ASCII_LIMIT or cp >= INDEX_LIMIT or cp in PAUSE_PUNCTUATION:
        return False
    if cp in MODIFIERS or cp in REGIONAL or cp in VARIATION:
        return False
    gc = category.get(cp)
    return (cp in basic) or gc in SYMBOL_CATEGORIES or gc in PUNCTUATION_NAMED


def build_locale(loc, ctx):
    loc_chain = chain(loc, ctx["parents"])
    direct = resolve(ctx["cldr"] / "common/annotations", loc_chain, ctx["cache"])
    derived = resolve(ctx["cldr"] / "common/annotationsDerived", loc_chain, ctx["cache"])
    singles, sequences = {}, {}

    def pick(key):
        if key in direct:
            name, src = direct[key]
            return clean(name), "cldr-annotations:" + src
        if key in derived:
            name, src = derived[key]
            return clean(name), "cldr-derived:" + src
        return None

    for cp in sorted(set(ctx["category"]) | ctx["basic"]):
        if not eligible_single(cp, ctx["category"], ctx["basic"]):
            continue
        hit = pick((cp,))
        # A compatibility character (㎡ SQUARE M SQUARED, ① CIRCLED DIGIT ONE) is
        # named after its glyph; its reading is its decomposition, left to TN.
        if not hit and loc_chain[-1] == "en" and ctx["category"].get(cp) in SYMBOL_CATEGORIES \
                and cp in ctx["names"] and cp not in ctx["compatibility"]:
            hit = ctx["names"][cp].lower().replace("-", " "), "ucd-17.0.0"
        if hit:
            singles[cp] = hit
    words, word_source = digit_words(ctx["cldr"], loc_chain)
    for kind, seqs in sorted(ctx["sequences"].items()):
        if kind == "Basic_Emoji":
            continue
        for seq in seqs:
            key = key_of(seq)
            if len(key) < 2:
                continue
            if kind == "Emoji_Keycap_Sequence":
                base = key[0]
                if 0x30 <= base <= 0x39:
                    if words:
                        sequences[key] = (words[base - 0x30], word_source)
                    continue
                hit = pick((base,))       # #️⃣ *️⃣ read their base symbol
            else:
                hit = pick(key)
            if hit:
                sequences[key] = hit
    return loc_chain, singles, sequences


def pack(singles, sequences):
    pool, offsets = bytearray(), {}

    def put(name):
        if name not in offsets:
            offsets[name] = 16 + len(pool)
            pool.extend(name.encode("utf-8") + b"\0")
        return offsets[name]

    index = [0] * INDEX_LIMIT
    for cp in sorted(singles):
        index[cp] = put(singles[cp][0])
    records = []
    for key in sorted(sequences, key=lambda k: "".join(map(chr, k)).encode("utf-8")):
        raw = "".join(map(chr, key)).encode("utf-8")
        assert len(raw) <= KEY_BYTES, hexkey(key)
        records.append(raw.ljust(KEY_BYTES, b"\0") + struct.pack("<I", put(sequences[key][0])))
    body = bytes(pool) + b"".join(records) + struct.pack("<%dI" % INDEX_LIMIT, *index)
    build_id = struct.unpack("<I", hashlib.sha256(body).digest()[:4])[0]
    dat = b"TTFUFDT1" + struct.pack("<II", build_id, len(pool)) + bytes(pool)
    idx = b"TTFUFIX1" + struct.pack("<II", build_id, INDEX_LIMIT) + struct.pack("<%dI" % INDEX_LIMIT, *index)
    seq = b"TTFUFSQ1" + struct.pack("<II", build_id, len(records)) + b"".join(records)
    return idx, dat, seq, build_id


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ucd", type=Path, required=True)
    ap.add_argument("--cldr", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--locales", help="comma-separated CLDR ids (default: every annotations locale)")
    ap.add_argument("--emoji-header", type=Path, help="also write the Extended_Pictographic C table")
    args = ap.parse_args()
    names, category, compatibility = read_ucd(args.ucd / "UnicodeData.txt")
    sequences = read_sequences(args.ucd / "emoji-sequences.txt", args.ucd / "emoji-zwj-sequences.txt")
    basic = {s[0] for s in sequences.get("Basic_Emoji", []) if len(key_of(s)) == 1}
    ctx = dict(names=names, category=category, compatibility=compatibility,
               sequences=sequences, basic=basic,
               cldr=args.cldr, parents=parent_map(args.cldr), cache={})
    available = sorted(p.stem for p in (args.cldr / "common/annotations").glob("*.xml") if p.stem != "root")
    locales = args.locales.split(",") if args.locales else available
    args.out.mkdir(parents=True, exist_ok=True)
    sources = sorted([args.ucd / f for f in ("UnicodeData.txt", "emoji-sequences.txt",
                                             "emoji-zwj-sequences.txt", "emoji-data.txt")]
                     + [args.cldr / "common/supplemental/supplementalData.xml"])
    manifest = dict(format=FORMAT, index_limit=INDEX_LIMIT, key_bytes=KEY_BYTES,
                    unicode="17.0.0", cldr=args.cldr.name,
                    sources={p.name: sha256(p) for p in sources},
                    transforms=["drop U+FE0E/U+FE0F from keys",
                                "collapse whitespace; ': ' -> ', '",
                                "digit keycaps -> RBNF spellout-numbering digit word",
                                "#/* keycaps -> base symbol name",
                                "en only: UCD name lowercased, '-' -> ' ' when CLDR has none "
                                "(never for a compatibility character)",
                                "never named: ASCII, pause punctuation, middle dots and bullets, "
                                "range marks"],
                    locales={})
    coverage = ["locale\tchain\tsingles\tsequences\tdigits\tsingle_cldr\tsingle_ucd\tbuild_id"]
    rgi = sum(1 for k, v in sequences.items() if k != "Basic_Emoji" for s in v if len(key_of(s)) >= 2)
    for loc in locales:
        loc_chain, singles, seqs = build_locale(loc, ctx)
        if not singles and not seqs:
            continue
        tag = loc.replace("_", "-")
        idx, dat, seq, build_id = pack(singles, seqs)
        (args.out / (tag + ".idx")).write_bytes(idx)
        (args.out / (tag + ".dat")).write_bytes(dat)
        (args.out / (tag + ".seq")).write_bytes(seq)
        rows = ["kind\tkey\tchars\tname\tsource"]
        rows += ["single\t%04X\t%s\t%s\t%s" % (cp, chr(cp), n, s) for cp, (n, s) in sorted(singles.items())]
        rows += ["sequence\t%s\t%s\t%s\t%s" % (hexkey(k), "".join(map(chr, k)), n, s)
                 for k, (n, s) in sorted(seqs.items())]
        (args.out / (tag + ".tsv")).write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
        ucd = sum(1 for n, s in singles.values() if s.startswith("ucd"))
        digits = sum(1 for n, s in seqs.values() if s.startswith("rbnf"))
        manifest["locales"][tag] = dict(cldr_id=loc, chain=loc_chain, singles=len(singles),
                                        sequences=len(seqs), build_id="%08x" % build_id,
                                        files={ext: hashlib.sha256(b).hexdigest()
                                               for ext, b in (("idx", idx), ("dat", dat), ("seq", seq))})
        coverage.append("%s\t%s\t%d\t%d/%d\t%d\t%d\t%d\t%08x" % (
            tag, ">".join(loc_chain), len(singles), len(seqs), rgi, digits, len(singles) - ucd, ucd, build_id))
    (args.out / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=1, sort_keys=True) + "\n",
                                            encoding="utf-8")
    (args.out / "coverage.tsv").write_text("\n".join(coverage) + "\n", encoding="utf-8")
    if args.emoji_header:
        ranges = read_property(args.ucd / "emoji-data.txt", "Extended_Pictographic")
        body = ",\n".join("    {0x%05X, 0x%05X}" % r for r in ranges)
        args.emoji_header.write_text(
            "/* Generated by tools/unicode_fallback/build.py from Unicode 17.0.0 emoji-data.txt.\n"
            "   Extended_Pictographic ranges; do not edit by hand. */\n"
            "#ifndef TTF_EMOJI_RANGES_H\n#define TTF_EMOJI_RANGES_H\n#include <stdint.h>\n"
            "static const uint32_t ttf_extended_pictographic[][2] = {\n%s\n};\n#endif\n" % body,
            encoding="utf-8", newline="\n")
    print("%d locales -> %s" % (len(manifest["locales"]), args.out))


if __name__ == "__main__":
    sys.exit(main())
