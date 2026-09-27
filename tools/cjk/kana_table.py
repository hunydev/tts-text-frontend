#!/usr/bin/env python3
"""Build the kana -> Hangul reading table.

Draft for tools/cjk/kana_table.py. See docs/cjk-korean-reading.ko.md 7.

Source
    외래어 표기법 (문화체육관광부 고시) 제2장 표 4 및 제4장 제2절.
    A government notice; 저작권법 제7조 제2호 excludes 국가의 고시 from
    copyright protection. Recorded in docs/THIRD_PARTY.md. This is a
    provenance note, not a legal opinion.

Model
    The table stores ONE form per kana: the 어중·어말 form. Three things are
    NOT in the table because they are positional or combining rules, exactly
    like 두음법칙 on the Hanja side:

      어두 청음화  ㅋ->ㄱ, ㅌ->ㄷ, ㅊ->ㅈ at the start of a run
      촉음 っ      add a ㅅ coda to the syllable already emitted
      발음 ん      add a ㄴ coda to the syllable already emitted
      장음         ー, and う/お continuing the same vowel, emit nothing

    Storing the 어중 form and deriving the 어두 form keeps the same contract
    the Hanja table uses: never bake a positional variant into the data.
"""
import argparse
import json
import pathlib
import sys

# ---- 청음 / 탁음 / 반탁음. Values are the 어중·어말 forms. ------------------
GOJUON = {
    "あ": "아", "い": "이", "う": "우", "え": "에", "お": "오",
    "か": "카", "き": "키", "く": "쿠", "け": "케", "こ": "코",
    "さ": "사", "し": "시", "す": "스", "せ": "세", "そ": "소",
    "た": "타", "ち": "치", "つ": "쓰", "て": "테", "と": "토",
    "な": "나", "に": "니", "ぬ": "누", "ね": "네", "の": "노",
    "は": "하", "ひ": "히", "ふ": "후", "へ": "헤", "ほ": "호",
    "ま": "마", "み": "미", "む": "무", "め": "메", "も": "모",
    "や": "야", "ゆ": "유", "よ": "요",
    "ら": "라", "り": "리", "る": "루", "れ": "레", "ろ": "로",
    "わ": "와", "ゐ": "이", "ゑ": "에", "を": "오",
    "が": "가", "ぎ": "기", "ぐ": "구", "げ": "게", "ご": "고",
    "ざ": "자", "じ": "지", "ず": "즈", "ぜ": "제", "ぞ": "조",
    "だ": "다", "ぢ": "지", "づ": "즈", "で": "데", "ど": "도",
    "ば": "바", "び": "비", "ぶ": "부", "べ": "베", "ぼ": "보",
    "ぱ": "파", "ぴ": "피", "ぷ": "푸", "ぺ": "페", "ぽ": "포",
}
# ---- 요음 -----------------------------------------------------------------
YOON = {
    "きゃ": "캬", "きゅ": "큐", "きょ": "쿄",
    "しゃ": "샤", "しゅ": "슈", "しょ": "쇼",
    "ちゃ": "차", "ちゅ": "추", "ちょ": "초",
    "にゃ": "냐", "にゅ": "뉴", "にょ": "뇨",
    "ひゃ": "햐", "ひゅ": "휴", "ひょ": "효",
    "みゃ": "먀", "みゅ": "뮤", "みょ": "묘",
    "りゃ": "랴", "りゅ": "류", "りょ": "료",
    "ぎゃ": "갸", "ぎゅ": "규", "ぎょ": "교",
    "じゃ": "자", "じゅ": "주", "じょ": "조",
    "びゃ": "뱌", "びゅ": "뷰", "びょ": "뵤",
    "ぴゃ": "퍄", "ぴゅ": "퓨", "ぴょ": "표",
}
# ---- 위치·결합 규칙으로 처리하는 문자 ---------------------------------------
MARKS = {
    "っ": "sokuon",        # 앞 음절에 ㅅ 받침
    "ん": "moraic-n",      # 앞 음절에 ㄴ 받침
    "ー": "long-vowel",    # 장음 부호: 출력 없음
}
# 장음으로 흡수되는 모음 연속. (앞 음절의 대표 중성, 이어지는 가나)
# 히라가나에만 적용한다. 가타카나 외래어의 모음은 보존한다 (설계 §7.2).
LONG_VOWEL_PAIRS = (("오", "う"), ("오", "お"), ("우", "う"), ("유", "う"), ("요", "う"))

HALFWIDTH = {
    "ｱ": "ア", "ｲ": "イ", "ｳ": "ウ", "ｴ": "エ", "ｵ": "オ",
    "ｶ": "カ", "ｷ": "キ", "ｸ": "ク", "ｹ": "ケ", "ｺ": "コ",
    "ｻ": "サ", "ｼ": "シ", "ｽ": "ス", "ｾ": "セ", "ｿ": "ソ",
    "ﾀ": "タ", "ﾁ": "チ", "ﾂ": "ツ", "ﾃ": "テ", "ﾄ": "ト",
    "ﾅ": "ナ", "ﾆ": "ニ", "ﾇ": "ヌ", "ﾈ": "ネ", "ﾉ": "ノ",
    "ﾊ": "ハ", "ﾋ": "ヒ", "ﾌ": "フ", "ﾍ": "ヘ", "ﾎ": "ホ",
    "ﾏ": "マ", "ﾐ": "ミ", "ﾑ": "ム", "ﾒ": "メ", "ﾓ": "モ",
    "ﾔ": "ヤ", "ﾕ": "ユ", "ﾖ": "ヨ",
    "ﾗ": "ラ", "ﾘ": "リ", "ﾙ": "ル", "ﾚ": "レ", "ﾛ": "ロ",
    "ﾜ": "ワ", "ｦ": "ヲ", "ﾝ": "ン", "ｯ": "ッ", "ｰ": "ー",
    # 요음 소자와 소모음. 이것이 빠지면 ﾄｳｷｮｳ 같은 반각 표기가 깨진다.
    "ｬ": "ャ", "ｭ": "ュ", "ｮ": "ョ",
    "ｧ": "ァ", "ｨ": "ィ", "ｩ": "ゥ", "ｪ": "ェ", "ｫ": "ォ",
}
# 반각의 탁점·반탁점(ﾞ ﾟ)은 앞 글자와 결합하는 별개 문자다. 접지 않고 두면
# ﾎﾟ 가 '호' + 원문 ﾟ 로 **잘못 읽히므로**, 두 글자 열쇠로 접는다. 보존보다
# 오독이 나쁘다.
HALFWIDTH_VOICED = {
    "ｶ": "ガ", "ｷ": "ギ", "ｸ": "グ", "ｹ": "ゲ", "ｺ": "ゴ",
    "ｻ": "ザ", "ｼ": "ジ", "ｽ": "ズ", "ｾ": "ゼ", "ｿ": "ゾ",
    "ﾀ": "ダ", "ﾁ": "ヂ", "ﾂ": "ヅ", "ﾃ": "デ", "ﾄ": "ド",
    "ﾊ": "バ", "ﾋ": "ビ", "ﾌ": "ブ", "ﾍ": "ベ", "ﾎ": "ボ",
}
HALFWIDTH_SEMIVOICED = {"ﾊ": "パ", "ﾋ": "ピ", "ﾌ": "プ", "ﾍ": "ペ", "ﾎ": "ポ"}

KATAKANA_OFFSET = 0x30A1 - 0x3041


def to_katakana(text):
    out = []
    for char in text:
        code = ord(char)
        out.append(chr(code + KATAKANA_OFFSET) if 0x3041 <= code <= 0x3096 else char)
    return "".join(out)


def build(out):
    rows, marks, notes = [], [], []
    seen = set()

    def add(key, value, kind="kana_reading"):
        if key in seen:
            raise SystemExit("duplicate kana key: " + key)
        seen.add(key)
        rows.append((kind, key, value))

    # 요음을 먼저 넣어 최장일치가 두 글자 조합을 잡게 한다.
    for source in (YOON, GOJUON):
        for kana, reading in source.items():
            add(kana, reading)
            add(to_katakana(kana), reading)
    for kana, role in MARKS.items():
        add(kana, role, "kana_mark")
        katakana = to_katakana(kana)
        if katakana != kana:
            add(katakana, role, "kana_mark")
    # 두 글자 열쇠를 먼저 넣어 최장일치가 탁점 결합을 잡게 한다.
    for half, full in HALFWIDTH_VOICED.items():
        add(half + "ﾞ", full, "kana_fold")
    for half, full in HALFWIDTH_SEMIVOICED.items():
        add(half + "ﾟ", full, "kana_fold")
    for half, full in HALFWIDTH.items():
        add(half, full, "kana_fold")
    for vowel, kana in LONG_VOWEL_PAIRS:
        marks.append(("kana_long_after", vowel + ":" + kana, "1"))

    header = (
        "# Generated by tools/cjk/kana_table.py; do not edit by hand.\n"
        "# Source: 외래어 표기법 (문화체육관광부 고시) 제2장 표 4, 제4장 제2절.\n"
        "# 저작권법 제7조 제2호의 국가 고시. 자세한 근거는 docs/THIRD_PARTY.md.\n"
        "#\n"
        "# Values are the 어중·어말 forms. The 어두 form is derived at run time\n"
        "# (ㅋ->ㄱ, ㅌ->ㄷ, ㅊ->ㅈ), exactly as 두음법칙 is derived for Hanja.\n"
        "# 촉음/발음/장음은 kana_mark 로 선언하고 결합 규칙으로 처리한다.\n"
        "# Matching this default is not independent linguistic approval.\n"
    )
    lines = ["%s\tko-KR\t%s\t%s\n" % (kind, key, value) for kind, key, value in rows]
    lines += ["kana_long_after\tko-KR\t%s\t%s\n" % (v, k) for _, v, k in marks]
    out.mkdir(parents=True, exist_ok=True)
    (out / "kana_readings.tsv").write_text(header + "".join(lines), encoding="utf-8", newline="\n")

    report = dict(
        source="외래어 표기법 (문화체육관광부 고시) 제2장 표 4, 제4장 제2절",
        license_basis="저작권법 제7조 제2호 (국가의 고시) — 보호받지 못하는 저작물",
        rows=len(rows) + len(marks),
        readings=sum(1 for kind, _, _ in rows if kind == "kana_reading"),
        marks=sum(1 for kind, _, _ in rows if kind == "kana_mark"),
        halfwidth_folds=sum(1 for kind, _, _ in rows if kind == "kana_fold"),
        long_vowel_pairs=len(marks),
        runtime_rules=["어두 청음화 ㅋ->ㄱ ㅌ->ㄷ ㅊ->ㅈ",
                       "촉음 っ -> 앞 음절 ㅅ 받침",
                       "발음 ん -> 앞 음절 ㄴ 받침",
                       "장음 ー 및 kana_long_after -> 출력 없음"],
        review_status="pending-independent-review",
    )
    (out / "kana_report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8", newline="\n")
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args(argv)
    json.dump(build(args.out), sys.stdout, ensure_ascii=False, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
