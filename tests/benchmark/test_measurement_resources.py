"""Original semantic contracts prompted by aggregate external coverage gaps.

No external corpus sentence or generated reference is vendored in these fixtures.
"""

import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CLI = Path(sys.argv[1]).resolve()
RESOURCES = Path(sys.argv[2]).resolve()
spec = importlib.util.spec_from_file_location(
    "ttf_protocol", ROOT / "tools/evaluation/evaluate.py"
)
protocol = importlib.util.module_from_spec(spec)
spec.loader.exec_module(protocol)
cases = [
    (
        "en-US",
        "The speed is 28 mph.",
        "The speed is twenty eight miles per hour.",
        "28 mph",
        "mph",
    ),
    (
        "en-US",
        "The speed is 47 km/h.",
        "The speed is forty seven kilometers per hour.",
        "47 km/h",
        "km_per_hour",
    ),
    ("en-US", "It measures 3 ft.", "It measures three feet.", "3 ft", "ft"),
    ("en-US", "It measures 1 ft.", "It measures one foot.", "1 ft", "ft"),
    (
        "en-US",
        "It measures 4.5 mi.",
        "It measures four point five miles.",
        "4.5 mi",
        "mi",
    ),
    ("en-US", "It measures 3 nm.", "It measures three nanometers.", "3 nm", "nm"),
    ("en-US", "It stores 48 KB.", "It stores forty eight kilobytes.", "48 KB", "KB"),
    ("en-US", "It generates 7 MW.", "It generates seven megawatts.", "7 MW", "MW"),
    (
        "en-US",
        "It uses 4 Wh/km.",
        "It uses four watt hours per kilometer.",
        "4 Wh/km",
        "Wh_per_km",
    ),
    (
        "en-US",
        "It covers 2 km².",
        "It covers two square kilometers.",
        "2 km²",
        "square_km",
    ),
    ("en-US", "It costs US$15.", "It costs fifteen dollars.", "US$15", "USD"),
    (
        "en-US",
        "It costs €19.25.",
        "It costs nineteen euros and twenty five cents.",
        "€19.25",
        "EUR",
    ),
    (
        "ko-KR",
        "속도는 28 mph입니다.",
        "속도는 시속 이십팔 마일입니다.",
        "28 mph",
        "mph",
    ),
    (
        "ko-KR",
        "속도는 47 km/h입니다.",
        "속도는 시속 사십칠 킬로미터입니다.",
        "47 km/h",
        "km_per_hour",
    ),
    ("ko-KR", "길이는 3 ft입니다.", "길이는 삼 피트입니다.", "3 ft", "ft"),
    (
        "ko-KR",
        "전비는 4 Wh/km입니다.",
        "전비는 킬로미터당 사 와트시입니다.",
        "4 Wh/km",
        "Wh_per_km",
    ),
    (
        "ko-KR",
        "면적은 2 km²입니다.",
        "면적은 이 제곱킬로미터입니다.",
        "2 km²",
        "square_km",
    ),
    ("ko-KR", "금액은 US$15입니다.", "금액은 십오 달러입니다.", "US$15", "USD"),
    (
        "en-US",
        "A speed of 2~3 mph.",
        "A speed of two to three miles per hour.",
        "2~3 mph",
        "mph",
    ),
    (
        "ko-KR",
        "속도는 2~3 mph입니다.",
        "속도는 시속 이 마일에서 시속 삼 마일입니다.",
        "2~3 mph",
        "mph",
    ),
]
cases += [
    ("ko-KR", "금액은 US$15달러입니다.", "금액은 십오 달러입니다.", "US$15달러", "USD"),
    ("ko-KR", "금액은 €15유로입니다.", "금액은 십오 유로입니다.", "€15유로", "EUR"),
    ("ko-KR", "금액은 ₩15원입니다.", "금액은 십오 원입니다.", "₩15원", "KRW"),
    ("ko-KR", "금액은 US$15원입니다.", "금액은 US$15원입니다.", "US$15원", ""),
    ("ko-KR", "금액은 €15원입니다.", "금액은 €15원입니다.", "€15원", ""),
    ("ko-KR", "금액은 $15원입니다.", "금액은 $15원입니다.", "$15원", ""),
    ("en-US", "It costs US$15 kg.", "It costs US$15 kg.", "US$15 kg", ""),
]
count = 0
for language, text, expected, surface, unit in cases:
    for chunk in [1, 7, 64]:
        run = subprocess.run(
            [
                str(CLI),
                "stream-json",
                "--resources",
                str(RESOURCES),
                "--language",
                language,
                "--chunk-bytes",
                str(chunk),
                "-",
            ],
            input=text.encode(),
            capture_output=True,
            check=True,
            timeout=15,
        )
        segments = protocol.parse_stream(run.stdout.decode(), text)
        actual = "".join(s["text"] for s in segments)
        assert actual == expected, (text, expected, actual)
        spans = [
            s
            for segment in segments
            for s in segment["spans"]
            if text.encode()[s["input_begin"] : s["input_end"]].decode() == surface
        ]
        assert len(spans) == 1 and spans[0]["unit"] == unit, (surface, spans)
        count += 2
# Resource-declared Latin prefixes generalize without language-pair code or named
# hardcoded prefixes, and the customer layer can replace their semantic unit.
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "currency.tsv"
    path.write_text("currency_prefix\t*\tZZ$\tEUR\n")
    actual = subprocess.check_output(
        [
            str(CLI),
            "normalize",
            "--resources",
            str(RESOURCES),
            "--language",
            "en-US",
            "--customer",
            str(path),
            "ZZ$17",
        ],
        text=True,
    ).rstrip("\n")
    assert actual == "seventeen euros", actual
    count += 1
    path.write_text("currency_prefix\t*\tZZ$\tUSD\n")
    text = "금액은 ZZ$15달러입니다."
    payload = subprocess.check_output(
        [
            str(CLI),
            "stream-json",
            "--resources",
            str(RESOURCES),
            "--customer",
            str(path),
            "--chunk-bytes",
            "1",
            "-",
        ],
        input=text.encode(),
    ).decode()
    segments = protocol.parse_stream(payload, text)
    amount = next(
        s for segment in segments for s in segment["spans"] if s["unit"] == "USD"
    )
    assert amount["resource_layer"] == "customer", amount
    assert "".join(segment["text"] for segment in segments) == "금액은 십오 달러입니다."
    count += 2
for text in [
    "US$15oops",
    "27mphX",
    "3km/hours",
    "`US$15`",
    "https://example.com/28mph",
]:
    actual = subprocess.check_output(
        [
            str(CLI),
            "normalize",
            "--resources",
            str(RESOURCES),
            "--language",
            "en-US",
            text,
        ],
        text=True,
    ).rstrip("\n")
    assert actual == text, (text, actual)
    count += 1
print(f"{count} original measurement/currency/metadata/chunk/resource contracts passed")
