#!/usr/bin/env python3
"""Author the CJK reading contracts for tests/evaluation/cjk_reading_v22.jsonl.

Written BEFORE any implementation exists, as AGENTS.md requires. Expected
values are authored from the stated policy, never read back from a candidate.

Two option fields are new to this corpus schema: "hanja" and "kana"
(passthrough|korean). The runner must learn them; see the report.

Basis codes used in `basis`:
  char-fallback   설계 5: 글자 독음 + 어두 변이만으로 성립
  word-entry      설계 5: cjk_word 엔트리가 있어야 성립
  initial-rule    설계 7.3: 두음법칙 / 어두 청음화
  sokuon          촉음 っ 는 앞 음절 ㅅ 받침
  moraic-n        발음 ん 은 ㄴ 받침
  long-vowel      장모음은 따로 적지 않는다
  preserve        설계 10: 미해결은 원문 보존
  passthrough     설계 1/18: 옵션이 꺼져 있으면 기존 결과 불변
  contract        설계 3.2 옵션 조합 계약
"""
import json
import pathlib

OUT = pathlib.Path(__file__).resolve().parent / "out" / "cjk_reading_v22.jsonl"

NOTE = ("Developer-authored CJK reading contract fixed before implementing the "
        "candidate; provisional project reading policy, pending independent "
        "linguistic review. Korean readings of Han follow 한국 한자음, not "
        "Chinese/Japanese transliteration; kana follows Korean loanword "
        "orthography. No synthesized audio and no externally copied corpus.")

# (id, category, text, expected, hanja, kana, extra, basis)
CASES = [
    # ---- 한자: 기본 글자 (char fallback) --------------------------------
    ("hanja-char-san", "cjk/hanja-char", "山", "산", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-char-su", "cjk/hanja-char", "水", "수", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-char-in", "cjk/hanja-char", "人", "인", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-char-dae", "cjk/hanja-char", "大", "대", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-char-guk", "cjk/hanja-char", "國", "국", "korean", "passthrough", {}, "char-fallback"),

    # ---- 한자: 한자어 ----------------------------------------------------
    ("hanja-word-hanja", "cjk/hanja-word", "漢字", "한자", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-word-jungguk", "cjk/hanja-word", "中國", "중국", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-word-daehanmingug", "cjk/hanja-word", "大韓民國", "대한민국", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-word-sansu", "cjk/hanja-word", "山水", "산수", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-word-eumak", "cjk/hanja-word", "音樂", "음악", "korean", "passthrough", {}, "word-entry"),
    ("hanja-word-nagwon", "cjk/hanja-word", "樂園", "낙원", "korean", "passthrough", {}, "char-fallback+initial-rule"),
    ("hanja-word-kwaerak", "cjk/hanja-word", "快樂", "쾌락", "korean", "passthrough", {}, "char-fallback"),

    # ---- 한자: 두음법칙 --------------------------------------------------
    ("hanja-initial-yeoja", "cjk/hanja-initial", "女子", "여자", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-namnyeo", "cjk/hanja-initial", "男女", "남녀", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-initial-noin", "cjk/hanja-initial", "老人", "노인", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-noso", "cjk/hanja-initial", "老少", "노소", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-yeoksa", "cjk/hanja-initial", "歷史", "역사", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-yeokdae", "cjk/hanja-initial", "歷代", "역대", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-naeil", "cjk/hanja-initial", "來日", "내일", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-mirae", "cjk/hanja-initial", "未來", "미래", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-initial-yeoe", "cjk/hanja-initial", "例外", "예외", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-sarye", "cjk/hanja-initial", "事例", "사례", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-initial-yangsim", "cjk/hanja-initial", "良心", "양심", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-gaeryang", "cjk/hanja-initial", "改良", "개량", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-initial-yonggung", "cjk/hanja-initial", "龍宮", "용궁", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-gongnyong", "cjk/hanja-initial", "恐龍", "공룡", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-initial-yeohaeng", "cjk/hanja-initial", "旅行", "여행", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-nodong", "cjk/hanja-initial", "勞動", "노동", "korean", "passthrough", {}, "initial-rule"),
    ("hanja-initial-guklip", "cjk/hanja-initial", "國立", "국립", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-initial-dongnip-text", "cjk/hanja-initial", "獨立", "독립", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-initial-compound", "cjk/hanja-initial", "男女老少", "남녀노소", "korean", "passthrough", {}, "word-entry"),

    # ---- 한자: 고유명사 / 인명 -------------------------------------------
    ("hanja-name-kim", "cjk/hanja-name", "金氏", "김씨", "korean", "passthrough", {}, "word-entry"),
    ("hanja-name-lee", "cjk/hanja-name", "李氏", "이씨", "korean", "passthrough", {}, "initial-rule"),

    # ---- 한자: 번역이 아님 (설계 1, 16) ----------------------------------
    ("hanja-not-translation-tokyo", "cjk/hanja-policy", "東京", "동경", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-not-translation-beijing", "cjk/hanja-policy", "北京", "북경", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-not-translation-chongqing", "cjk/hanja-policy", "重慶", "중경", "korean", "passthrough", {}, "char-fallback"),

    # ---- 한자: 혼합 문맥 --------------------------------------------------
    ("hanja-mixed-korean", "cjk/hanja-mixed", "오늘 中國 시장이 열렸습니다.",
     "오늘 중국 시장이 열렸습니다.", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-mixed-latin", "cjk/hanja-mixed", "Korea 大韓民國 API",
     "Korea 대한민국 API", "korean", "passthrough", {}, "char-fallback"),

    # ---- 한자: 호환 한자 (자체 독음이 canonical보다 구체적) ---------------
    # U+F914/F95C/F9BF 는 樂 을 낙/락/요 로 나누어 인코딩한 한국어용 호환 한자다.
    # 어두에서는 두음법칙이 그대로 적용되므로, 특정 독음 선택은 어중에서 검사한다.
    ("hanja-compat-rak", "cjk/hanja-compat", "快樂", "쾌락", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-compat-yo", "cjk/hanja-compat", "樂", "요", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-compat-nak", "cjk/hanja-compat", "樂", "낙", "korean", "passthrough", {}, "char-fallback"),
    ("hanja-compat-initial-rule", "cjk/hanja-compat", "樂", "낙", "korean", "passthrough", {}, "initial-rule"),

    # ---- 한자: 미해결 보존 (설계 10) -------------------------------------
    # 틀을 한국어로 둔다. 라틴 대문자 런은 speech 모드에서 기존 약어 읽기가
    # 적용되어(ABC123 -> 에이비씨원투스리) 미해결 한자 보존과 무관한 동작이 섞인다.
    ("hanja-unresolved-preserve", "cjk/hanja-preserve", "가\U00020BB7나",
     "가\U00020BB7나", "korean", "passthrough", {}, "preserve"),
    ("hanja-unresolved-partial", "cjk/hanja-preserve", "山\U00020BB7水",
     "산\U00020BB7수", "korean", "passthrough", {}, "preserve+char-fallback"),

    # ---- 한자: PASSTHROUGH ----------------------------------------------
    ("hanja-passthrough-word", "cjk/passthrough", "大韓民國", "大韓民國", "passthrough", "passthrough", {}, "passthrough"),
    ("hanja-passthrough-sentence", "cjk/passthrough", "오늘 中國 시장이 열렸습니다.",
     "오늘 中國 시장이 열렸습니다.", "passthrough", "passthrough", {}, "passthrough"),

    # ---- 가나: 기본 -------------------------------------------------------
    ("kana-basic-sushi", "cjk/kana-basic", "すし", "스시", "passthrough", "korean", {}, "char-fallback"),
    ("kana-basic-hiroshima", "cjk/kana-basic", "ひろしま", "히로시마", "passthrough", "korean", {}, "char-fallback"),
    ("kana-basic-nagoya", "cjk/kana-basic", "なごや", "나고야", "passthrough", "korean", {}, "char-fallback"),
    ("kana-basic-fujisan", "cjk/kana-basic", "ふじさん", "후지산", "passthrough", "korean", {}, "char-fallback+moraic-n"),

    # ---- 가나: 어두 청음화 ------------------------------------------------
    ("kana-initial-tanaka", "cjk/kana-initial", "たなか", "다나카", "passthrough", "korean", {}, "initial-rule"),
    ("kana-initial-kato", "cjk/kana-initial", "かとう", "가토", "passthrough", "korean", {}, "initial-rule+long-vowel"),
    ("kana-initial-chiba", "cjk/kana-initial", "ちば", "지바", "passthrough", "korean", {}, "initial-rule"),
    ("kana-initial-kobe", "cjk/kana-initial", "こうべ", "고베", "passthrough", "korean", {}, "initial-rule+long-vowel"),
    ("kana-initial-yokohama", "cjk/kana-initial", "よこはま", "요코하마", "passthrough", "korean", {}, "char-fallback"),

    # ---- 가나: 촉음 -------------------------------------------------------
    ("kana-sokuon-sapporo", "cjk/kana-sokuon", "さっぽろ", "삿포로", "passthrough", "korean", {}, "sokuon"),
    ("kana-sokuon-hokkaido", "cjk/kana-sokuon", "ほっかいどう", "홋카이도", "passthrough", "korean", {}, "sokuon+initial-rule+long-vowel"),

    # ---- 가나: 발음 ん ----------------------------------------------------
    ("kana-moraic-shinbun", "cjk/kana-moraic-n", "しんぶん", "신분", "passthrough", "korean", {}, "moraic-n"),

    # ---- 가나: 장음 -------------------------------------------------------
    ("kana-long-tokyo", "cjk/kana-long", "とうきょう", "도쿄", "passthrough", "korean", {}, "initial-rule+long-vowel"),
    ("kana-long-osaka", "cjk/kana-long", "おおさか", "오사카", "passthrough", "korean", {}, "long-vowel+initial-rule"),
    ("kana-long-kyoto", "cjk/kana-long", "きょうと", "교토", "passthrough", "korean", {}, "initial-rule+long-vowel"),
    ("kana-long-ramen", "cjk/kana-long", "ラーメン", "라멘", "passthrough", "korean", {}, "long-vowel+moraic-n"),

    # ---- 가나: 요음 -------------------------------------------------------
    ("kana-youon-kitakyushu", "cjk/kana-youon", "きたきゅうしゅう", "기타큐슈",
     "passthrough", "korean", {}, "initial-rule+long-vowel"),
    ("kana-youon-osakajo", "cjk/kana-youon", "おおさかじょう", "오사카조",
     "passthrough", "korean", {}, "long-vowel"),

    # ---- 가나: 가타카나 ---------------------------------------------------
    ("kana-katakana-sapporo", "cjk/kana-katakana", "サッポロ", "삿포로", "passthrough", "korean", {}, "sokuon"),
    ("kana-katakana-sushi", "cjk/kana-katakana", "スシ", "스시", "passthrough", "korean", {}, "char-fallback"),

    # ---- 가나: 반각 가타카나 (설계 8.3) -----------------------------------
    # 장음 흡수는 히라가나에만 적용한다. 가타카나의 ウ 는 외래어에서 실음절인
    # 경우가 많다 (ソウル=소우루, ボウル=보우루). 그 대가로 가타카나로 적은
    # 일본 고유어는 장음이 남는다. 설계 1 이 부분 변환을 허용하는 범위다.
    ("kana-halfwidth-tokyo", "cjk/kana-halfwidth", "ﾄｳｷｮｳ", "도우쿄우",
     "passthrough", "korean", {}, "halfwidth-fold"),
    ("kana-katakana-no-long-absorb", "cjk/kana-long", "ボウル", "보우루",
     "passthrough", "korean", {}, "char-fallback"),
    ("kana-halfwidth-voiced", "cjk/kana-halfwidth", "ｻｯﾎﾟﾛ", "삿포로",
     "passthrough", "korean", {}, "halfwidth-fold+sokuon"),

    # ---- 가나: 결합할 앞 음절이 없는 마크는 원문 보존 (설계 10) -------------
    ("kana-orphan-moraic-n", "cjk/kana-preserve", "ん", "ん", "passthrough", "korean", {}, "preserve"),
    ("kana-orphan-sokuon", "cjk/kana-preserve", "っ", "っ", "passthrough", "korean", {}, "preserve"),

    # ---- 가나: 외래어 예외 (설계 7.5) -------------------------------------
    ("kana-loanword-seoul", "cjk/kana-loanword", "ソウル", "서울", "passthrough", "korean", {}, "word-entry"),
    # 규칙대로면 ん->ㄴ 이라 '우돈'이다. '우동'은 이미 국어에 정착한 차용어이므로
    # 규칙이 아니라 엔트리로 처리한다 (설계 7.5).
    ("kana-loanword-udon", "cjk/kana-loanword", "うどん", "우동", "passthrough", "korean", {}, "word-entry"),

    # ---- 가나: 혼합 문맥 --------------------------------------------------
    ("kana-mixed-korean", "cjk/kana-mixed", "오늘 サッポロ 여행", "오늘 삿포로 여행",
     "passthrough", "korean", {}, "sokuon"),

    # ---- 가나: PASSTHROUGH ------------------------------------------------
    ("kana-passthrough-word", "cjk/passthrough", "とうきょう", "とうきょう",
     "passthrough", "passthrough", {}, "passthrough"),

    # ---- 옵션 조합 계약 (설계 3.2) ----------------------------------------
    ("combo-both-off", "cjk/option-combo", "東京とうきょう", "東京とうきょう",
     "passthrough", "passthrough", {}, "contract"),
    ("combo-hanja-only", "cjk/option-combo", "東京とうきょう", "동경とうきょう",
     "korean", "passthrough", {}, "contract"),
    ("combo-kana-only", "cjk/option-combo", "東京とうきょう", "東京도쿄",
     "passthrough", "korean", {}, "contract"),
    ("combo-both-on", "cjk/option-combo", "東京とうきょう", "동경도쿄",
     "korean", "korean", {}, "contract"),
    ("combo-multilingual-line", "cjk/option-combo", "大韓民國 Korea 中國 日本",
     "대한민국 Korea 중국 일본", "korean", "passthrough", {}, "char-fallback"),
    # 간체자·신자체는 한국 표준(K0/K1) 자형이 아니다. 설계 1의 성공 기준상
    # 원문 보존이 정상 동작이며, 커버리지를 맞추려 범위를 넓히지 않는다.
    ("combo-simplified-preserved", "cjk/option-combo", "中国 日本",
     "중国 일본", "korean", "passthrough", {}, "preserve+char-fallback"),

    # ---- unreadable 정책과의 상호작용 (설계 13.3) -------------------------
    ("policy-drop-converted", "cjk/unreadable", "오늘 中國 시장", "오늘 중국 시장",
     "korean", "passthrough", {"unreadable": "drop"}, "contract"),
    ("policy-drop-unconverted", "cjk/unreadable", "오늘 中國 시장", "오늘 시장",
     "passthrough", "passthrough", {"unreadable": "drop"}, "passthrough"),
    # 기존 v20 계약이 가漢나 + drop -> '가 나' 로 공백을 남긴다. 미해결 문자를
    # 지우면서 두 낱말을 붙이지 않는다는 정책이며, CJK 라고 달라질 이유가 없다.
    ("policy-drop-unresolved", "cjk/unreadable", "山\U00020BB7水", "산 수",
     "korean", "passthrough", {"unreadable": "drop"}, "contract"),

    # ---- 결정성 -----------------------------------------------------------
    # 같은 글자가 위치에 따라 달라지되, 같은 입력은 항상 같은 결과를 낸다.
    ("determinism-multi-reading", "cjk/determinism", "樂樂樂", "낙락락",
     "korean", "passthrough", {}, "initial-rule"),
    ("determinism-north", "cjk/determinism", "北北", "북북",
     "korean", "passthrough", {}, "char-fallback"),
]


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    seen = set()
    rows = []
    for identifier, category, text, expected, hanja, kana, extra, basis in CASES:
        assert identifier not in seen, "duplicate id: " + identifier
        seen.add(identifier)
        row = dict(
            id=identifier,
            category=category,
            text=text,
            expected=expected,
            default_language="ko-KR",
            output_mode="speech",
            input_format="plain",
            unreadable=extra.get("unreadable", "preserve"),
            hanja=hanja,
            kana=kana,
            expected_spans=[],
            basis=basis,
            notes=NOTE,
        )
        rows.append(json.dumps(row, ensure_ascii=False))
    OUT.write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
    print("%d contracts -> %s" % (len(rows), OUT))
    by_basis = {}
    for *_, basis in CASES:
        by_basis[basis] = by_basis.get(basis, 0) + 1
    for key in sorted(by_basis):
        print("  %-36s %d" % (key, by_basis[key]))


if __name__ == "__main__":
    main()
