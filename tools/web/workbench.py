"""Validated workbench requests and catalog, shared by server and worker."""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAX_TEXT_BYTES = 16384
CORPORA = {
    "ko-identifier-shapes-v23": ("하이픈 숫자열 · 범위 아님 · 원문 방치 방지", "ko_identifier_shapes_v23.jsonl"),
    "break-controls-v23": ("끊어 읽기 · 앞 표기를 한 글자씩 1·2·3", "break_controls_v23.jsonl"),
    "ko-spacing-option-v23": ("띄어쓰기 옵션 spacing=orthographic|reading-unit", "ko_spacing_option_v23.jsonl"),
    "ko-reading-unit-spacing-v23": ("한국어 읽기 단위 띄어쓰기 (선택 옵션)", "ko_reading_unit_spacing_v23.jsonl"),
    "en-street-title-v23": ("영어 St./Mt./Ft. 호칭 전개", "en_street_title_v23.jsonl"),
    "en-caps-word-v23": ("영어 대문자 일반 단어 · 철자 읽기 방지", "en_caps_word_v23.jsonl"),
    "ko-covid-name-v23": ("코로나19 이름 읽기", "ko_covid_name_v23.jsonl"),
    "ko-password-label-v23": ("비밀번호·PIN 라벨 뒤 숫자 한 자리씩", "ko_password_label_v23.jsonl"),
    "ko-card-number-v23": ("카드번호 4-4-4-4 한 자리씩", "ko_card_number_v23.jsonl"),
    "en-month-year-v23": ("영어 월 뒤 연도 읽기", "en_month_year_v23.jsonl"),
    "en-roman-numeral-v23": ("영어 로마 숫자 · 장 번호 기수 · 왕 이름 서수", "en_roman_numeral_v23.jsonl"),
    "ko-alnum-identifier-v23": ("영문+숫자 코드 · 한 글자씩 · 하이픈 다시", "ko_alnum_identifier_v23.jsonl"),
    "en-speech-hyphen-v23": ("영어 speech 하이픈·대시 무음", "en_speech_hyphen_v23.jsonl"),
    "en-decade-zero-v23": ("영어 decade 00s · 2000년대", "en_decade_zero_v23.jsonl"),
    "ko-spacing-v22": ("한국어 수 묶음 띄어쓰기", "ko_spacing_v22.jsonl"),
    "counted-noun-v22": ("나이·차수·학년 단위 경계", "counted_noun_v22.jsonl"),
    "phone-zero-v22": ("전화번호 · 번호/번 꼬리 · 선행 0 자리 읽기", "phone_zero_v22.jsonl"),
    "identifier-fallback-v22": ("식별번호 · 거부된 숫자열 읽기 · 하이픈 다시", "identifier_fallback_v22.jsonl"),
    "lexical-coverage-v22": ("공통 발음 사전 · 사용자 읽기 선택", "lexical_coverage_v22.jsonl"),
    "counter-frequency-v22": ("횟수·번 문맥 단서 / 번호 구분", "counter_frequency_v22.jsonl"),
    "regression": ("기본 회귀 · KO / EN / 코드믹스", "cases.jsonl"),
    "finance-content": ("금융·콘텐츠 시나리오", "domain_cases_v8.jsonl"),
    "advanced": ("금융 식별자·수치·콘텐츠 확장", "advanced_cases.jsonl"),
    "protected": ("코드·태그·경로 보호", "protected_cases.jsonl"),
    "content-v7": ("제목·고유명사·발음 확장", "content_v7.jsonl"),
    "finance-v7": ("단위가 반복되는 금융 범위", "finance_ranges_v7.jsonl"),
    "lexicon-v10": ("사전 확장 · 금융·콘텐츠 문맥", "lexicon_v10.jsonl"),
    "multiword-v8": ("금융·콘텐츠 복합 이름 발음", "multiword_v8.jsonl"),
    "scaled-currency-v12": ("금융 금액 배수·통화·범위", "scaled_currency_v12.jsonl"),
    "english-dates-v13": ("영어 날짜·연도·코드믹스", "english_dates_v13.jsonl"),
    "english-forms-v14": ("영어 통화명·약어·이니셜", "english_forms_v14.jsonl"),
    "korean-counters-v15": ("한국어 서수·횟수·번호", "korean_counters_v15.jsonl"),
    "phone-v18": ("국제전화·내선·계좌번호 구분", "phone_cases_v18.jsonl"),
    "user-patterns-v20": ("읽기 제어·발화용 출력", "user_patterns_v20.jsonl"),
    "numeric-width-v22": ("전각 숫자 · 금액 / 날짜 / 전화 / 셈단위", "numeric_width_v22.jsonl"),
    "typed-controls-v22": ("의미 지정 읽기 · 숫자 / 날짜 / 단위 / 기술 표기", "typed_controls_v22.jsonl"),
    "grouped-controls-v22": ("구간 지정 읽기 · 주소 / 이메일 / URL", "grouped_controls_v22.jsonl"),
    "speech-final-gate-v21": ("발화 최종 관문 · 기호·숫자형 문자", "speech_final_gate_v21.jsonl"),
    "letters-v21": ("& 이니셜 · 대문자 한 글자", "letters_v21.jsonl"),
    "electronic-v21": ("이메일·URL 단어 · 버전 · IP", "electronic_v21.jsonl"),
    "math-hashtag-v21": ("수식·비교·해시태그", "math_hashtag_v21.jsonl"),
    "counter-band-v21": ("대: 연령·가격대와 대수", "counter_band_v21.jsonl"),
    "floor-v21": ("층·지하층 표기", "floor_v21.jsonl"),
    "letters-paths-v21": ("소문자 한 글자 · 메뉴 경로 >", "letters_paths_v21.jsonl"),
    "letter-number-sign-v21": ("Z 읽기 · #숫자", "letter_number_sign_v21.jsonl"),
    "particle-v21": ("조사 교정 (은/는, 로/으로)", "particle_v21.jsonl"),
    "calendar-alias-v21": ("월·요일 별칭 · 날짜 뒤 (Fri)", "calendar_alias_v21.jsonl"),
    "abbreviation-dotted-v21": ("영문 점 약어 · Ltd. / Vol. / vs.", "abbreviation_dotted_v21.jsonl"),
    "speech-character-width-v21": ("전각 영문자 · 최종 문자 읽기", "speech_character_width_v21.jsonl"),
    "particle-table-v21": ("조사 자원표 · (이)랑·(이)라도·(이)나마", "particle_table_v21.jsonl"),
    "mixed-case-v21": ("iOS·5G 같은 혼합 표기", "mixed_case_v21.jsonl"),
    "phone-service-v21": ("대표번호·괄호 전화번호", "phone_service_v21.jsonl"),
    "slash-letters-v21": ("A/S 같은 슬래시 약어", "slash_letters_v21.jsonl"),
    "clock-v21": ("정시 표기 (09:00)", "clock_v21.jsonl"),
    "slash-date-v21": ("9/25 같은 슬래시 날짜", "slash_date_v21.jsonl"),
    "range-mark-v21": ("붙여 쓴 ~ 범위 표시", "range_mark_v21.jsonl"),
    "multi-range-v21": ("3월 1일~3월 5일 구간", "multi_range_v21.jsonl"),
    "handle-v21": ("아이디·핸들 (abc_123, @handle)", "handle_v21.jsonl"),
    "year-cue-v21": ("영어 연도 문맥", "year_cue_v21.jsonl"),
    "bracket-v21": ("괄호 안 처리 옵션", "bracket_v21.jsonl"),
    "unknown-foreign-v21": ("미등록 외국어 단어 정책", "unknown_foreign_v21.jsonl"),
    "decorative-v21": ("장식 문자 이름 읽기", "decorative_v21.jsonl"),
    "initialism-v21": ("대문자 약어 읽기 방식", "initialism_v21.jsonl"),
    "code-word-v21": ("SAVE20 같은 대문자 코드", "code_word_v21.jsonl"),
    "letter-label-v21": ("k7 같은 글자+숫자 라벨", "letter_label_v21.jsonl"),
    "oov-guess-v21": ("미등록 영어 단어 추측 (측정용)", "oov_guess_v21.jsonl"),
}
DEFAULTS = dict(
    default_language="ko-KR",
    render_policy="phonetic",
    input_format="plain",
    output_mode="speech",
    unreadable="preserve",
    hanja="passthrough",
    kana="passthrough",
    brackets="read",
    unknown_foreign="guess",
    decorative="follow",
    initialism="spell",
    spacing="orthographic",
    max_control_bytes=4096,
    sentence_split=True,
    phrase_split=True,
    detailed_trace=True,
    first_target=48,
    normal_target=112,
    max_length=200,
    max_pending_bytes=1048576,
    chunk_bytes=7,
    chunk_sizes=[],
    delay_ms=0,
    domain="none",
    customer_tsv="",
    user_lexicon=[],
    remove_system_words=[],
    verify_chunks=[1, 7, 64],
    random_seed=42,
)


def bounded_string(value, name, cap):
    if not isinstance(value, str) or "\0" in value:
        raise ValueError(name + ": 문자열이 필요합니다 (NUL 제외).")
    try:
        length = len(value.encode("utf-8"))
    except UnicodeError as exc:
        raise ValueError(name + ": 유효한 UTF-8이 아닙니다.") from exc
    if length > cap:
        raise ValueError(f"{name}: 최대 {cap} UTF-8 바이트입니다.")
    return value


def integer(value, name, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{name}: {low}~{high} 범위의 정수가 필요합니다.")
    return value


def stream_sizes(length, options):
    """Explicit write sizes first; fill remaining bytes with the uniform size."""
    schedule = []
    for size in options["chunk_sizes"]:
        if not length:
            break
        size = min(size, length)
        schedule.append(size)
        length -= size
    while length:
        size = min(options["chunk_bytes"], length)
        schedule.append(size)
        length -= size
    return schedule


def validate(request, corpus=False):
    if not isinstance(request, dict):
        raise ValueError("JSON 객체가 필요합니다.")
    if corpus:
        key = request.get("corpus")
        if key not in CORPORA or not corpus_path(key).is_file():
            raise ValueError("등록되지 않은 코퍼스입니다.")
        return {"corpus": key}
    if request.get("mode") not in ("normalize", "stream", "verify"):
        raise ValueError("지원하지 않는 실행 모드입니다.")
    result = {
        "mode": request["mode"],
        "text": bounded_string(request.get("text"), "text", MAX_TEXT_BYTES),
    }
    if "expected" in request:
        result["expected"] = bounded_string(
            request["expected"], "expected", MAX_TEXT_BYTES * 8
        )
    given = request.get("options", {})
    if not isinstance(given, dict) or set(given) - set(DEFAULTS):
        raise ValueError("알 수 없는 설정입니다.")
    opts = dict(DEFAULTS, **given)
    for key, allowed in [
        ("default_language", ("ko-KR", "en-US")),
        ("render_policy", ("phonetic", "preserve", "semantic")),
        ("input_format", ("plain", "annotated")),
        ("output_mode", ("text", "speech")),
        ("unreadable", ("preserve", "drop", "read")),
        ("hanja", ("passthrough", "korean")),
        ("kana", ("passthrough", "korean")),
        ("brackets", ("read", "skip", "preserve")),
        ("unknown_foreign", ("guess", "spell", "preserve")),
        ("decorative", ("follow", "read")),
        ("initialism", ("spell", "word")),
        ("spacing", ("orthographic", "reading-unit")),
        ("domain", ("none", "finance", "finance-vocabulary")),
    ]:
        if opts[key] not in allowed:
            raise ValueError(key + ": 지원하지 않는 값입니다.")
    for key in ("sentence_split", "phrase_split", "detailed_trace"):
        if type(opts[key]) is not bool:
            raise ValueError(key + ": boolean이 필요합니다.")
    for key in ("first_target", "normal_target", "max_length"):
        integer(opts[key], key, 1, 1000000)
    integer(opts["max_pending_bytes"], "max_pending_bytes", 64, 1048576)
    integer(opts["max_control_bytes"], "max_control_bytes", 1, MAX_TEXT_BYTES)
    integer(opts["chunk_bytes"], "chunk_bytes", 1, 65536)
    integer(opts["delay_ms"], "delay_ms", 0, 100)
    integer(opts["random_seed"], "random_seed", 0, 2147483647)
    bounded_string(opts["customer_tsv"], "customer_tsv", 16384)
    if (
        not isinstance(opts["verify_chunks"], list)
        or not 1 <= len(opts["verify_chunks"]) <= 6
    ):
        raise ValueError("검증 청크 크기는 1~6개입니다.")
    for size in opts["verify_chunks"]:
        integer(size, "verify_chunks", 1, 65536)
    if not isinstance(opts["chunk_sizes"], list) or len(opts["chunk_sizes"]) > 256:
        raise ValueError("직접 지정 청크는 최대 256개입니다.")
    for size in opts["chunk_sizes"]:
        integer(size, "chunk_sizes", 1, 65536)
    rows = opts["user_lexicon"]
    if not isinstance(rows, list) or len(rows) > 100:
        raise ValueError("사용자 사전은 최대 100개입니다.")
    for row in rows:
        if not isinstance(row, dict) or set(row) != {"word", "pronunciation"}:
            raise ValueError("사용자 사전 항목 형식이 잘못되었습니다.")
        for key in ("word", "pronunciation"):
            if not bounded_string(row[key], key, 512).strip():
                raise ValueError("사전 단어/발음을 비울 수 없습니다.")
    words = opts["remove_system_words"]
    if not isinstance(words, list) or len(words) > 32:
        raise ValueError("시스템 사전 제외 항목은 최대 32개입니다.")
    for word in words:
        bounded_string(word, "remove_system_words", 128)
    writes = len(stream_sizes(len(result["text"].encode()), opts))
    if result["mode"] == "stream" and max(0, writes - 1) * opts["delay_ms"] > 8000:
        raise ValueError(
            "총 입력 지연은 8초 이내여야 합니다. 청크 크기를 키우거나 지연을 줄이세요."
        )
    result["options"] = opts
    return result


def corpus_path(key):
    return ROOT / "tests/evaluation" / CORPORA[key][1]


def version():
    return re.search(
        r"project\(tts_text_frontend VERSION ([\d.]+)",
        (ROOT / "CMakeLists.txt").read_text(),
    ).group(1)


def reference_audit():
    """Read the review inventory; engine agreement never supplies approval."""
    directory = str(ROOT / "tools/evaluation")
    if directory not in sys.path:
        sys.path.insert(0, directory)
    from audit_references import audit

    return audit(ROOT)


def reference_case_reviews(report, corpus_key):
    relative = corpus_path(corpus_key).relative_to(ROOT).as_posix()
    return {row["id"]: row for row in report["cases"] if row["path"] == relative}


def catalog():
    audit = reference_audit()
    reviewed_files = {row["path"]: row for row in audit["files"]}
    reviewed_cases = {(row["path"], row["id"]): row for row in audit["cases"]}
    samples = [
        dict(
            id="amount-range",
            category="금융",
            title="금액 범위와 생략된 단위",
            text="1~2만 원",
            expected="만 원에서 이만 원",
        ),
        dict(
            id="tilde-symbol",
            category="기호",
            title="범위가 아닌 물결표",
            text="~",
            expected="물결",
        ),
        dict(
            id="mixed",
            category="코드믹스",
            title="같은 1st, 다른 문맥",
            text="1st는 첫 번째라는 뜻입니다.\nPlease go to the 1st floor",
            expected="퍼스트는 첫 번째라는 뜻입니다.\nPlease go to the first floor",
        ),
        dict(
            id="foreign",
            category="발음",
            title="외래어·약어와 영어 보존",
            text="Apple은 사과입니다. GPU를 사용합니다.\nApple released the iPhone.",
            expected="애플은 사과입니다. 지피유를 사용합니다.\nApple released the iPhone.",
        ),
        dict(
            id="reading-preferences",
            category="사전",
            title="사용자 사전 · 티비/올레드/차종 읽기",
            text="TV와 OLED, K3의 이름입니다.",
            expected="티비와 올레드, 케이쓰리의 이름입니다.",
            options={"customer_tsv": (ROOT / "resources/customers/reading-preferences/ko-KR/lexicon.tsv").read_text(encoding="utf-8")},
        ),
        dict(
            id="layer",
            category="리소스",
            title="사용자 발음사전 우선순위",
            text="Apple은 사과입니다.",
            expected="커스텀애플은 사과입니다.",
            options={
                "user_lexicon": [{"word": "Apple", "pronunciation": "커스텀애플"}]
            },
        ),
        dict(
            id="fallback",
            category="발음",
            title="WiFi 시스템 사전 제외",
            text="WiFi를 사용합니다.",
            options={"remove_system_words": ["wifi"]},
        ),
        dict(
            id="pronunciation-variant",
            category="발음",
            title="복수 발음의 명시적 선택과 상세 출처",
            text="read는 예시입니다.",
            options={"customer_tsv": (
                "pronunciation\ten-US\tread\tR IY D\n"
                "pronunciation_variant\ten-US\tread#past\tR EH D\n"
                "pronunciation_choice\ten-US\tread\tpast\n"
            )},
        ),
        dict(
            id="range-layer",
            category="금융",
            title="금융 도메인 숫자 정책",
            text="123-456",
            expected="일이삼 사오육",
            options={"domain": "finance"},
        ),
        dict(
            id="customer",
            category="리소스",
            title="고객 규칙 덮어쓰기",
            text="123-456",
            expected="일이삼 번에 사오육",
            options={
                "domain": "finance",
                "customer_tsv": (
                    ROOT / "resources/customers/example/ko-KR/rules.tsv"
                ).read_text(),
            },
        ),
        dict(
            id="stream",
            category="스트리밍",
            title="문장 콜백과 숫자 lookahead",
            text="Apple은 사과입니다.\n가격은 12,500원입니다.\nPlease go to the 1st floor.",
            expected="애플은 사과입니다.\n가격은 만 이천오백 원입니다.\nPlease go to the first floor.",
            options={"chunk_bytes": 7, "delay_ms": 50},
        ),
        dict(
            id="multiword",
            category="복합 이름",
            title="단어 사이에서 끊긴 복합 이름",
            text="After Effects를 사용합니다.\nBank of America는 은행입니다.",
            expected="애프터 이펙트를 사용합니다.\n뱅크 오브 아메리카는 은행입니다.",
            options={"chunk_sizes": [6, 7, 1], "delay_ms": 30},
        ),
        dict(
            id="multiword-user",
            category="복합 이름",
            title="복합 이름의 사용자 발음 우선순위",
            text="After Effects를 사용합니다.",
            expected="커스텀이펙트를 사용합니다.",
            options={
                "user_lexicon": [
                    {"word": "After Effects", "pronunciation": "커스텀이펙트"}
                ]
            },
        ),
        dict(
            id="protected",
            category="문서",
            title="코드·파일 경로는 그대로",
            text="<code>GPU 3개</code> 예제는 3개입니다.\n설정 파일 /etc/app/config-v2.json을 확인합니다.",
            expected="<code>GPU 3개</code> 예제는 세 개입니다.\n설정 파일 /etc/app/config-v2.json을 확인합니다.",
        ),
    ]
    for sample_id, title, text, expected, classification in (
        ("unicode-grouping-currency", "금액의 NBSP 묶음 (U+00A0)",
         "1\u00a0250원", "천이백오십 원", "reference-policy"),
        ("unicode-grouping-percent", "부호·소수·좁은 NBSP (U+202F)",
         "-1\u202f250.50%", "마이너스 천이백오십 점 오영 퍼센트", "reference-policy"),
        ("unicode-grouping-malformed", "잘못된 얇은 공백 묶음 보존 (U+2009)",
         "1\u200925원", "1\u200925원", "preservation-contract"),
        ("unicode-grouping-range", "금액 범위의 얇은 공백 묶음 (U+2009)",
         "1\u2009250~2\u2009500원", "천이백오십 원에서 이천오백 원", "reference-policy"),
    ):
        samples.append(dict(
            id=sample_id, category="숫자·Unicode", title=title, text=text, expected=expected,
            options={"default_language": "ko-KR", "chunk_bytes": 1},
            reference_review={
                "classification": classification,
                "review_status": "pending-independent-review",
                "reference_source": "developer-authored workbench example; tests/integration/test_unicode_grouping.py",
                "reason": "Explicit grouping/value or malformed-input preservation contract; no independent linguistic approval.",
            },
        ))
    for key in ("advanced", "content-v7", "finance-v7", "scaled-currency-v12", "english-dates-v13", "english-forms-v14", "korean-counters-v15", "phone-v18"):
        if corpus_path(key).exists():
            for row in [
                json.loads(x)
                for x in corpus_path(key).read_text().splitlines()
                if x.strip()
            ][:4]:
                samples.append(
                    dict(
                        id=key + "-" + row["id"],
                        category=("한국어" if key == "korean-counters-v15" else
                                  "콘텐츠" if key == "content-v7" else "금융"),
                        title=row["text"][:45],
                        text=row["text"],
                        expected=row["expected"],
                        options={
                            "default_language": row.get("default_language", "ko-KR"),
                            "output_mode": row.get("output_mode", "text"),
                        },
                        reference_review=reviewed_cases[
                            (corpus_path(key).relative_to(ROOT).as_posix(), row["id"])
                        ],
                    )
                )
    control_titles = {
        "user-v20-literal-ordinal": "지정한 발음으로 치환 · 1{첫}번째",
        "user-v20-literal-phrase": "띄어쓰기를 포함하는 지정 발음",
        "user-v20-native-twenty-one": "고유어 수사 · 21{@native}",
        "user-v20-sino-negative": "한자어 수사 · -12{@sino}",
        "user-v20-alphabet-store": "영문자 이름 · ABC{@alphabet}마트",
        "user-v20-email-digits": "이메일의 영문자·숫자 발화",
        "user-v20-email-text-digits": "이메일 원문 보존과 비교",
        "user-v20-emoji-zwj-drop": "읽을 수 없는 문자 제거",
    }
    if corpus_path("user-patterns-v20").exists():
        relative = corpus_path("user-patterns-v20").relative_to(ROOT).as_posix()
        for line in corpus_path("user-patterns-v20").read_text().splitlines():
            row = json.loads(line)
            if row["id"] not in control_titles:
                continue
            samples.append(dict(
                id=row["id"], category="읽기 제어·출력", title=control_titles[row["id"]],
                text=row["text"], expected=row["expected"],
                options={key: row[key] for key in (
                    "default_language", "input_format", "output_mode", "unreadable", "brackets", "unknown_foreign",
                    "decorative", "initialism", "spacing"
                ) if key in row},
                reference_review=reviewed_cases[(relative, row["id"])],
            ))
    group_titles = {
        "control-group-building": "주소 구간 · 건물·동·호수 읽기",
        "control-group-email-internal": "이메일 지정 · 내부 도메인",
        "control-group-url-path": "URL 지정 · 경로 읽기",
    }
    relative = corpus_path("grouped-controls-v22").relative_to(ROOT).as_posix()
    for line in corpus_path("grouped-controls-v22").read_text().splitlines():
        row = json.loads(line)
        if row["id"] in group_titles:
            samples.append(dict(id=row["id"], category="구간 지정 읽기",
                title=group_titles[row["id"]], text=row["text"], expected=row["expected"],
                options={key: row[key] for key in ("default_language", "input_format", "output_mode")},
                reference_review=reviewed_cases[(relative, row["id"])]))
    typed_titles = {
        "typed-digits-ko": "자릿수 지정 · 선행 0 보존",
        "typed-year-en": "연도 지정 · 영어 단서 없는 연도",
        "typed-date-mdy-ko": "날짜 지정 · 월/일/년",
        "typed-date-dmy-ko": "날짜 지정 · 일/월/년",
        "typed-fraction-ko": "분수 지정 · 날짜와 구별",
        "typed-counter-ko": "셈단위 지정 · 고유어 수사",
        "typed-phone-ko": "전화번호 지정 · 자동 형식 밖 번호",
        "typed-version-ko": "버전 지정 · 소수와 구별",
        "typed-preserve-ko": "구간 원문 보존",
        "typed-skip-join": "구간 생략 · 주변 공백은 입력대로",
    }
    relative = corpus_path("typed-controls-v22").relative_to(ROOT).as_posix()
    for line in corpus_path("typed-controls-v22").read_text().splitlines():
        row = json.loads(line)
        if row["id"] in typed_titles:
            samples.append(dict(id=row["id"], category="의미 지정 읽기",
                title=typed_titles[row["id"]], text=row["text"], expected=row["expected"],
                options={key: row[key] for key in ("default_language", "input_format", "output_mode")},
                reference_review=reviewed_cases[(relative, row["id"])]))
    width_titles = {
        "width-grouped-money": "전각 금액 · 숫자 값으로 읽기",
        "width-phone": "전각 전화번호 · 자릿수 보존",
        "width-date": "전각 날짜 · 달력 해석",
    }
    relative = corpus_path("numeric-width-v22").relative_to(ROOT).as_posix()
    for line in corpus_path("numeric-width-v22").read_text().splitlines():
        row = json.loads(line)
        if row["id"] in width_titles:
            samples.append(dict(id=row["id"], category="전각 숫자",
                title=width_titles[row["id"]], text=row["text"], expected=row["expected"],
                options={key: row[key] for key in ("default_language", "input_format", "output_mode")},
                reference_review=reviewed_cases[(relative, row["id"])]))
    phone_zero_titles = {
        "phone-zero-v22-02-four": "서울 지역번호 · 번호 꼬리",
        "phone-zero-v22-015": "임의 전화 앞자리 · 번호 꼬리",
        "phone-zero-v22-us-phone": "영어 전화번호 · 자리 읽기",
        "phone-zero-v22-bare-0046": "선행 0 · 모든 자릿수 보존",
    }
    relative = corpus_path("phone-zero-v22").relative_to(ROOT).as_posix()
    for line in corpus_path("phone-zero-v22").read_text().splitlines():
        row = json.loads(line)
        if row["id"] in phone_zero_titles:
            samples.append(dict(id=row["id"], category="전화·숫자",
                title=phone_zero_titles[row["id"]], text=row["text"], expected=row["expected"],
                options={key: row[key] for key in ("default_language", "input_format", "output_mode") if key in row},
                reference_review=reviewed_cases[(relative, row["id"])]))
    for corpus_key, category, titles in [
        ("counted-noun-v22", "단위 경계", {"counted-noun-v22-age-attached": "나이 · 조사/어미",
                                       "counted-noun-v22-stage": "차수 · 예약",
                                       "counted-noun-v22-negative-generation": "세대 · 단위 오인 방지"}),
        ("break-controls-v23", "끊어 읽기", {f"break-v23-strength-{level}": f"한 글자씩 끊기 {level}단계" for level in (1, 2, 3)}),
    ]:
        relative = corpus_path(corpus_key).relative_to(ROOT).as_posix()
        for line in corpus_path(corpus_key).read_text().splitlines():
            row = json.loads(line)
            if row["id"] in titles:
                samples.append(dict(id=row["id"], category=category, title=titles[row["id"]],
                    text=row["text"], expected=row["expected"],
                    options={key: row[key] for key in ("default_language", "input_format", "output_mode")},
                    reference_review=reviewed_cases[(relative, row["id"])]))
    example_references = {
        "amount-range": ("finance-v7", "range-krw"),
        "mixed": ("regression", "mixed-ordinal"),
        "foreign": ("regression", "wifi"),
        "fallback": ("regression", "unknown-foobar"),
        "multiword": ("multiword-v8", "multiword-after-effects"),
        "protected": ("protected", "protected-code-challenge"),
        "reading-preferences": ("lexical-coverage-v22", "lexical-v22-preference-tv"),
    }
    for sample in samples:
        if "reference_review" not in sample:
            corpus_key, case_id = example_references.get(sample["id"], ("regression", "mixed-ordinal"))
            relative = corpus_path(corpus_key).relative_to(ROOT).as_posix()
            # The example follows this inventory category; it is not a claim
            # that a different sample text received the source case's review.
            decision = reviewed_cases[(relative, case_id)]
            sample["reference_review"] = {
                "classification": decision["classification"],
                "review_status": decision["review_status"],
                "reference_source": "developer-authored workbench example",
                "reason": "Editable example following the selected project policy; no independent linguistic approval.",
            }
    return dict(
        version=version(),
        abi_version=1,
        samples=samples,
        defaults=DEFAULTS,
        reference_summary=audit["summary"],
        corpora=[
            dict(
                id=k,
                title=v[0],
                count=sum(
                    bool(x.strip()) for x in corpus_path(k).read_text().splitlines()
                ),
                reference_review=reviewed_files[corpus_path(k).relative_to(ROOT).as_posix()],
            )
            for k, v in CORPORA.items()
            if corpus_path(k).exists()
        ],
        limits=dict(
            text_bytes=MAX_TEXT_BYTES, request_seconds=45, concurrent_requests=4
        ),
    )
