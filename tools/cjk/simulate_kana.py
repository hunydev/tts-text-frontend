#!/usr/bin/env python3
"""Simulate the kana scanner against the authored contracts.

Analysis aid for docs/cjk-korean-reading.ko.md 7, not a specification of the
C implementation. It exists to prove the generated table plus four positional
rules reproduce the expected values that were authored independently.
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent
SYLLABLE_BASE = 0xAC00
# 어두 청음화: ㅋ->ㄱ, ㅌ->ㄷ, ㅊ->ㅈ. ㅍ 은 바뀌지 않는다 (ぱ행은 항상 ㅍ).
INITIAL_LEAD = {15: 0, 16: 3, 14: 12}
CODA_SIOT, CODA_NIEUN = 19, 4


def decompose(syllable):
    index = ord(syllable) - SYLLABLE_BASE
    if not 0 <= index < 19 * 21 * 28:
        return None
    return index // 588, index % 588 // 28, index % 28


def compose(lead, vowel, tail):
    return chr(SYLLABLE_BASE + (lead * 21 + vowel) * 28 + tail)


def initial_form(syllable):
    parts = decompose(syllable)
    if not parts:
        return syllable
    lead, vowel, tail = parts
    return compose(INITIAL_LEAD.get(lead, lead), vowel, tail)


def with_coda(syllable, coda):
    parts = decompose(syllable)
    if not parts or parts[2]:
        return None                       # 이미 받침이 있으면 결합하지 않는다
    return compose(parts[0], parts[1], coda)


def load(path):
    readings, marks, folds, long_after = {}, {}, {}, set()
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        kind, _, key, value = line.split("\t")
        if kind == "kana_reading":
            readings[key] = value
        elif kind == "kana_mark":
            marks[key] = value
        elif kind == "kana_fold":
            folds[key] = value
        elif kind == "kana_long_after":
            if value == "1":
                long_after.add(tuple(key.split(":")))
    return readings, marks, folds, long_after


def fold(text, folds):
    """Longest-match fold, so a half-width base plus its voicing mark folds as
    one unit (ﾎﾟ -> ポ) instead of degrading to ホ followed by a stray mark."""
    out, at = [], 0
    while at < len(text):
        pair = text[at:at + 2]
        if len(pair) == 2 and pair in folds:
            out.append(folds[pair])
            at += 2
            continue
        out.append(folds.get(text[at], text[at]))
        at += 1
    return "".join(out)


def convert(text, tables):
    readings, marks, folds, long_after = tables
    text = fold(text, folds)
    out, at, initial = [], 0, True
    while at < len(text):
        # 최장일치: 요음 두 글자를 한 글자보다 먼저 본다.
        pair = text[at:at + 2]
        if len(pair) == 2 and pair in readings:
            key, width = pair, 2
        elif text[at] in readings:
            key, width = text[at], 1
        elif text[at] in marks:
            role = marks[text[at]]
            if role == "long-vowel":
                pass                                   # 장음 부호는 출력하지 않는다
            else:
                # 결합할 앞 음절이 없거나 이미 받침이 있으면 결합하지 못한다.
                # 그때 문자를 버리면 설계 10 의 보존 정책을 어기므로 원문을 남긴다.
                coda = CODA_SIOT if role == "sokuon" else CODA_NIEUN
                joined = with_coda(out[-1], coda) if out else None
                if joined:
                    out[-1] = joined
                else:
                    out.append(text[at])
                    initial = True
            at += 1
            continue
        else:
            out.append(text[at])                       # 미해결: 원문 보존
            at += 1
            initial = True
            continue
        # 장음: 직전 음절의 중성을 잇는 う/お 는 적지 않는다. 비교 대상은
        # 음절 전체가 아니라 모음이므로, 표의 대표 음절에서 중성만 꺼낸다.
        if out and decompose(out[-1]):
            vowel = decompose(out[-1])[1]
            if any(kana == key and decompose(sample) and decompose(sample)[1] == vowel
                   for sample, kana in long_after):
                at += width
                continue
        reading = readings[key]
        out.append(initial_form(reading) if initial else reading)
        initial = False
        at += width
    return "".join(out)


def main():
    tables = load(ROOT.parent.parent / "resources/languages/ko-KR/cjk/kana_readings.tsv")
    rows = [json.loads(line) for line in
            (ROOT.parent.parent / "tests/evaluation/cjk_reading_v22.jsonl")
            .read_text(encoding="utf-8").splitlines()]
    checked = failed = 0
    for row in rows:
        if row["kana"] != "korean" or row["hanja"] == "korean":
            continue
        if "word-entry" in row["basis"]:
            continue
        checked += 1
        got = convert(row["text"], tables)
        if got != row["expected"]:
            failed += 1
            print("  FAIL %-28s 기대 %-12s 결과 %s" % (row["id"], row["expected"], got))
    print("가나 계약 %d건 검사, 불일치 %d건" % (checked, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
