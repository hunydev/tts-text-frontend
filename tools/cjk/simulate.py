#!/usr/bin/env python3
"""Simulate character-fallback + 두음법칙 over the generated table.

Purpose: find which authored contracts a character-only fallback already
satisfies, and which therefore require a cjk_word entry. This decides the
MINIMUM system word list, so the project does not ship a large dictionary
it cannot review (design 6.3, 15).

This is an analysis aid, not a specification of the C implementation.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent
LEAD_N, LEAD_R, LEAD_NG = 2, 5, 11
PALATAL = frozenset({2, 3, 6, 7, 12, 17, 20})


def load(path):
    table = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        _, _, key, value = line.split("\t")
        table[key] = value.split("|")
    return table


def initial_form(syllable):
    index = ord(syllable) - 0xAC00
    if not 0 <= index < 19 * 21 * 28:
        return syllable
    lead, vowel, tail = index // 588, index % 588 // 28, index % 28
    if lead == LEAD_R:
        lead = LEAD_NG if vowel in PALATAL else LEAD_N
    elif lead == LEAD_N and vowel in PALATAL:
        lead = LEAD_NG
    else:
        return syllable
    return chr(0xAC00 + (lead * 21 + vowel) * 28 + tail)


def convert(text, table):
    """Character fallback only: preferred reading, 두음 at run start."""
    out, initial = [], True
    for char in text:
        readings = table.get(char)
        if not readings:
            out.append(char)
            initial = True
            continue
        reading = readings[0]
        out.append(initial_form(reading) if initial else reading)
        initial = False
    return "".join(out)


CASES = [
    # (한자, 기대 표기)
    ("山", "산"), ("水", "수"), ("人", "인"), ("大", "대"), ("國", "국"),
    ("漢字", "한자"), ("中國", "중국"), ("大韓民國", "대한민국"),
    ("音樂", "음악"), ("女子", "여자"), ("老人", "노인"), ("歷史", "역사"),
    ("男女", "남녀"), ("男女老少", "남녀노소"), ("國立", "국립"),
    ("獨立", "독립"), ("東京", "동경"), ("北京", "북경"), ("山水", "산수"),
    ("金氏", "김씨"), ("重慶", "중경"), ("少年少女", "소년소녀"),
    ("女性", "여성"), ("老少", "노소"), ("歷代", "역대"), ("來日", "내일"),
    ("未來", "미래"), ("例外", "예외"), ("事例", "사례"), ("良心", "양심"),
    ("改良", "개량"), ("龍宮", "용궁"), ("恐龍", "공룡"), ("李氏", "이씨"),
    ("旅行", "여행"), ("勞動", "노동"), ("樂園", "낙원"), ("快樂", "쾌락"),
]


def main():
    table = load(ROOT / "out/resources/hanja_readings.tsv")
    need_word, ok = [], []
    for source, expected in CASES:
        got = convert(source, table)
        (ok if got == expected else need_word).append((source, expected, got))
    print("글자 fallback만으로 통과: %d / %d" % (len(ok), len(CASES)))
    for source, expected, _ in ok:
        print("   OK   %-8s -> %s" % (source, expected))
    print()
    print("cjk_word 엔트리 필요: %d" % len(need_word))
    for source, expected, got in need_word:
        print("   NEED %-8s 기대 %-8s  fallback %s" % (source, expected, got))
    return 0


if __name__ == "__main__":
    sys.exit(main())
