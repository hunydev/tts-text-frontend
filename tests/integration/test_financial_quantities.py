"""Exact decimal finance values, bounded syntax, unit IR, and streamed parity."""
from decimal import Decimal, localcontext
import json
from pathlib import Path
import random
import subprocess
import sys


cli, resources = sys.argv[1:]
resources = str(Path(resources).resolve())
profiles = ([], ["--first-target", "1", "--normal-target", "3", "--max-length", "12"])
checks = 0


def stream(text, language="ko-KR", chunk=1, profile=(), overlay=()):
    command = [cli, "stream-json", "--resources", resources, "--language", language,
               "--chunk-bytes", str(chunk), *profile, *overlay, "-"]
    run = subprocess.run(command, input=text.encode(), capture_output=True, timeout=10)
    assert run.returncode == 0, (text, command, run.stderr)
    events = [json.loads(line) for line in run.stdout.decode().splitlines()]
    assert events[-1]["event"] == "end", events
    segments = [event for event in events if event["event"] == "segment"]
    return "".join(event["text"] for event in segments), [
        span for event in segments for span in event["spans"]
    ]


def check(text, expected, semantic, value, unit="", secondary="", language="ko-KR",
          overlay=()):
    global checks
    for profile in profiles:
        for chunk in (1, 7, 64):
            output, spans = stream(text, language, chunk, profile, overlay)
            assert output == expected, (text, language, output, expected, profile, chunk)
            assert len(spans) == 1, (text, spans)
            span = spans[0]
            assert span["semantic_type"] == semantic, (text, span)
            assert span["value"] == value and span["unit"] == unit, (text, span)
            assert span["secondary"] == secondary, (text, span)
            assert (span["input_begin"], span["input_end"]) == (0, len(text.encode())), span
            checks += 1


for args in [
    ("25bp", "이십오 베이시스포인트", "MEASURE", "25", "basis_point"),
    ("0.25%p", "영 점 이오 퍼센트포인트", "MEASURE", "0.25", "percentage_point"),
    ("1.5-2.5%", "일 점 오에서 이 점 오 퍼센트", "RANGE", "1.5", "percent", "2.5"),
    ("1.5 - 2.5 km", "일 점 오에서 이 점 오 킬로미터", "RANGE", "1.5", "km", "2.5"),
    ("-1.5 - -0.5%", "마이너스 일 점 오 퍼센트에서 마이너스 영 점 오 퍼센트", "RANGE", "-1.5",
     "percent", "-0.5"),
    ("1,200-2,500원", "천이백에서 이천오백 원", "RANGE", "1200", "KRW", "2500"),
    ("1e2-2e2kg", "백 킬로그램에서 이백 킬로그램", "RANGE", "100", "kg", "200"),
    ("2.5e-2%p", "영 점 영이오 퍼센트포인트", "MEASURE", "0.025", "percentage_point"),
    ("1bp", "one basis point", "MEASURE", "1", "basis_point", "", "en-US"),
    ("2bp", "two basis points", "MEASURE", "2", "basis_point", "", "en-US"),
    ("1%p", "one percentage point", "MEASURE", "1", "percentage_point", "", "en-US"),
    ("1.5-2.5%", "one point five to two point five percent", "RANGE", "1.5", "percent", "2.5",
     "en-US"),
    ("1.5-2.5km", "one point five to two point five kilometers", "RANGE", "1.5", "km", "2.5",
     "en-US"),
    ("9:05 AM", "nine oh five a m", "TIME", "09:05", "", "AM", "en-US"),
    ("9:05PM", "nine oh five p m", "TIME", "09:05", "", "PM", "en-US"),
    ("9:05:01 PM", "nine oh five and one second p m", "TIME", "09:05:01", "", "PM", "en-US"),
]:
    check(*args)

# Bare integer ranges retain the old customer/domain policy, while genuine
# quantities must not silently become identifiers under that policy.
finance = ("--domain", str(Path(resources) / "domains/finance/ko-KR/rules.tsv"))
check("123-456", "일이삼 사오육", "RANGE", "123", "", "456", overlay=finance)
check("1.5-2.5%", "일 점 오에서 이 점 오 퍼센트", "RANGE", "1.5", "percent", "2.5",
      overlay=finance)

# Decimal independently computes values; matching flat numeral rendering checks
# scientific syntax without copying the implementation's arithmetic algorithm.
rng = random.Random(15485863)
expressions = ["1.2e6", "1.20e0", "0001.200e+1", "-0e3", ".5e2", "+1e3", "1e-90",
               "1e90", "0.000001e6", "-1.250e-3"]
for _ in range(40):
    expressions.append(f"{rng.randrange(-999999, 1000000)}.{rng.randrange(1000):03d}"
                       f"e{rng.randrange(-14, 15):+d}")
for expression in expressions:
    with localcontext() as context:
        context.prec = 200
        value = format(Decimal(expression), "f")
    if "." in value:
        value = value.rstrip("0").rstrip(".")
    if expression.startswith("+"):
        value = "+" + value
    expected, _ = stream(value + "원")
    check(expression + "원", expected, "CURRENCY", value, "KRW")

# Invalid exponent syntax, values exceeding IR capacity, and damaged range or
# clock objects preserve their spelling rather than normalize only a prefix.
for text in ["1e999", "1e-999", "1e95", "1e-94", "1e", "1e+", "1.2e-", "1e2e3",
             "1e2.3", "1--2", "1-+2", "1.5-2.5%pX", "25bpx", "0.25%pX", "12%%",
             "13:05 AM", "00:05 PM", "9:60 AM"]:
    for profile in profiles:
        for chunk in (1, 7, 64):
            output, spans = stream(text, profile=profile, chunk=chunk)
            assert output == text, (text, output, spans)
            assert len(spans) == 1 and spans[0]["semantic_type"] == "TEXT", (text, spans)
            assert not spans[0]["value"] and not spans[0]["secondary"], (text, spans)
            checks += 1

print(f"{checks} exact-value/unit/span/chunk checks, including 50 Decimal reference amounts")
