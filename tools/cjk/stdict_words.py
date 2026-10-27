"""Word table part of tools/cjk/stdict_readings.py (merged into the tool after validation)."""
import collections
import unicodedata
from pathlib import Path

CJK = Path(__file__).resolve().parents[2] / "resources/languages/ko-KR/cjk"
JAMO_D, JAMO_J = 3, 12  # ㄷ, ㅈ initial indexes


def _table(path, kind):
    out = {}
    for line in Path(path).open(encoding="utf-8"):
        p = line.rstrip("\n").split("\t")
        if not line.startswith("#") and len(p) == 4 and p[0] == kind:
            out.setdefault(p[2], p[3])
    return out


def _initial_variant(s):
    cp = ord(s) - 0xAC00
    if cp < 0 or cp > 11171:
        return s
    lead, vowel = cp // 588, cp % 588 // 28
    rep = lead
    if lead == 2 and vowel in (6, 12, 17, 20):
        rep = 11
    elif lead == 5:
        if vowel in (2, 6, 7, 12, 17, 20):
            rep = 11
        elif vowel in (0, 1, 8, 11, 13, 18):
            rep = 2
    return chr(0xAC00 + rep * 588 + cp % 588)


class Simulator:
    """Reads a Han string the way src/cjk/cjk_reading.c does with hanja=korean."""

    def __init__(self, chars):
        self.read = {k: v.split("|")[0] for k, v in _table(CJK / "hanja_readings.tsv", "hanja_reading").items()}
        for k, v in chars.items():
            self.read.setdefault(k, v[0])
        self.words = _table(CJK / "cjk_words.tsv", "cjk_word")
        self.soft = _table(CJK / "hanja_rules.tsv", "hanja_soft_after")
        self.before = _table(CJK / "hanja_rules.tsv", "hanja_before_initial")

    def __call__(self, src, words=None):
        words = self.words if words is None else words
        out, i, initial, last = [], 0, True, None
        while i < len(src):
            hit = None
            for n in range(min(6, len(src) - i), 0, -1):
                if src[i:i + n] in words:
                    hit = n
                    break
            if hit:
                out.append(words[src[i:i + hit]]); last = out[-1][-1]; initial = False; i += hit
                continue
            c = src[i]
            r = self.read.get(c)
            if r is None:
                out.append(c); initial = True; last = None; i += 1
                continue
            if initial:
                r = _initial_variant(r)
            else:
                code = (ord(last) - 0xAC00) % 28 if last and "가" <= last <= "힣" else -1
                if code in (0, 4) and r in self.soft:
                    r = self.soft[r]
            if i + 1 < len(src) and src[i + 1] in self.read:
                nxt = self.read[src[i + 1]]
                lead = (ord(nxt) - 0xAC00) // 588
                jamo = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"[lead]
                if c + ":" + jamo in self.before:
                    r = self.before[c + ":" + jamo]
            out.append(r); last = r; initial = False; i += 1
        return "".join(out)


def word_table(pairs, chars):
    """Dictionary words of 2-6 characters whose reading the character rules miss, unambiguous only."""
    sim = Simulator(chars)
    readings = collections.defaultdict(set)
    for source, hangul in pairs:
        readings[source].add(hangul)
    # The dictionary writes some words with a compatibility ideograph (滑稽 with U+F904,
    # read 골), but text uses the unified character (滑 U+6ED1, read 활 first). List such
    # a word under the unified form too, unless that form is a word of its own (2026-10-27).
    folded = collections.defaultdict(set)
    for source, hs in readings.items():
        unified = unicodedata.normalize("NFC", source)
        if unified != source:
            folded[unified] |= hs
    for unified, hs in folded.items():
        readings.setdefault(unified, hs)
    table = {}
    for source, hs in readings.items():
        if 2 <= len(source) <= 6 and len(hs) == 1:
            h = next(iter(hs))
            if sim(source) != h:
                table[source] = h
    # An entry also matches inside longer words. Drop one that other dictionary
    # words read differently more often than as the entry does (樂器 악기 inside
    # 娛樂器 오락기): a substring with its own reading elsewhere is not a word.
    inside = collections.defaultdict(lambda: [0, 0])
    for source, hs in readings.items():
        if len(hs) != 1:
            continue
        h = next(iter(hs))
        for n in range(2, min(6, len(source) - 1) + 1):
            for i in range(len(source) - n + 1):
                key = source[i:i + n]
                if key in table:
                    inside[key][h[i:i + n] == table[key]] += 1
    for key, (other, same) in inside.items():
        if other > same:
            del table[key]
    return table, sim
