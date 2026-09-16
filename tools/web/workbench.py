"""Validated workbench requests and catalog, shared by server and worker."""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAX_TEXT_BYTES = 16384
CORPORA = {
    "regression": ("기본 회귀 · KO / EN / 코드믹스", "cases.jsonl"),
    "finance-content": ("금융·콘텐츠 시나리오", "domain_cases.jsonl"),
    "advanced": ("금융 식별자·수치·콘텐츠 확장", "advanced_cases.jsonl"),
    "protected": ("코드·태그·경로 보호", "protected_cases.jsonl"),
    "content-v7": ("제목·고유명사·발음 확장", "content_v7.jsonl"),
    "finance-v7": ("단위가 반복되는 금융 범위", "finance_ranges_v7.jsonl"),
}
DEFAULTS = dict(
    default_language="ko-KR",
    render_policy="phonetic",
    sentence_split=True,
    phrase_split=True,
    first_target=48,
    normal_target=112,
    max_length=200,
    max_pending_bytes=1048576,
    chunk_bytes=7,
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
        ("domain", ("none", "finance")),
    ]:
        if opts[key] not in allowed:
            raise ValueError(key + ": 지원하지 않는 값입니다.")
    for key in ("sentence_split", "phrase_split"):
        if type(opts[key]) is not bool:
            raise ValueError(key + ": boolean이 필요합니다.")
    for key in ("first_target", "normal_target", "max_length"):
        integer(opts[key], key, 1, 1000000)
    integer(opts["max_pending_bytes"], "max_pending_bytes", 64, 1048576)
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
    if (len(result["text"].encode()) // opts["chunk_bytes"]) * opts["delay_ms"] > 8000:
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


def catalog():
    samples = [
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
            expected="애플은 사과입니다.\n가격은 만이천오백 원입니다.\nPlease go to the first floor.",
            options={"chunk_bytes": 7, "delay_ms": 50},
        ),
        dict(
            id="protected",
            category="문서",
            title="코드·파일 경로는 그대로",
            text="<code>GPU 3개</code> 예제는 3개입니다.\n설정 파일 /etc/app/config-v2.json을 확인합니다.",
            expected="<code>GPU 3개</code> 예제는 세 개입니다.\n설정 파일 /etc/app/config-v2.json을 확인합니다.",
        ),
    ]
    for key in ("advanced", "content-v7", "finance-v7"):
        if corpus_path(key).exists():
            for row in [
                json.loads(x)
                for x in corpus_path(key).read_text().splitlines()
                if x.strip()
            ][:4]:
                samples.append(
                    dict(
                        id=key + "-" + row["id"],
                        category="금융" if key != "content-v7" else "콘텐츠",
                        title=row["text"][:45],
                        text=row["text"],
                        expected=row["expected"],
                        options={
                            "default_language": row.get("default_language", "ko-KR")
                        },
                    )
                )
    return dict(
        version=version(),
        abi_version=1,
        samples=samples,
        defaults=DEFAULTS,
        corpora=[
            dict(
                id=k,
                title=v[0],
                count=sum(
                    bool(x.strip()) for x in corpus_path(k).read_text().splitlines()
                ),
            )
            for k, v in CORPORA.items()
            if corpus_path(k).exists()
        ],
        limits=dict(
            text_bytes=MAX_TEXT_BYTES, request_seconds=45, concurrent_requests=4
        ),
    )
